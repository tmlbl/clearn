#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define ORDER 4
#define MAX_KEYS (2 * ORDER - 1)
#define MAX_CHILDREN (2 * ORDER)
#define NODE_BUFFER_SIZE 4096

struct bplus_node_disk {
    char buf[NODE_BUFFER_SIZE];

    uint16_t key_offsets[MAX_KEYS];
    uint16_t key_lengths[MAX_KEYS];
    uint16_t value_offsets[MAX_KEYS];
    uint16_t value_lengths[MAX_KEYS];

    int num_keys;
    uint16_t buffer_position;
    int is_leaf;

    uint32_t children[MAX_CHILDREN];
    uint32_t page_id;
};

struct bplus_node {
    struct bplus_node_disk disk;

    struct bplus_node *children[MAX_CHILDREN];
    struct bplus_node *next;

    int dirty;
};

struct bplus_disk_header {
    uint32_t root_page_id;
};

struct bplus_buffer_pool {
    int fd; // file descriptor
    uint32_t next_page_id;
    struct bplus_node *pages[1024];
    int num_cached;
};

struct bplus_tree {
    struct bplus_buffer_pool *pool;
    struct bplus_node *root;
    int height;
};

struct bplus_buffer_pool *bplus_buffer_pool_init(const char *path) {
    struct bplus_buffer_pool *pool = malloc(sizeof(struct bplus_buffer_pool));

    int f = open(path, O_CREAT | O_RDWR, 0644);
    if (f < 0) {
        perror("open buffer pool file");
        free(pool);
        return NULL;
    }

    struct stat st = {0};
    fstat(f, &st);

    pool->fd = f;
    if (st.st_size != 0) {
        pool->next_page_id = (st.st_size - sizeof(struct bplus_disk_header)) /
                             sizeof(struct bplus_node_disk);
    } else {
        pool->next_page_id = 0;
    }
    pool->num_cached = 0;

    return pool;
}

int bplus_buffer_pool_get_offset(uint32_t page_id) {
    return (page_id * sizeof(struct bplus_node_disk)) +
           sizeof(struct bplus_disk_header);
}

struct bplus_node *
bplus_buffer_pool_load(struct bplus_buffer_pool *pool, uint32_t page_id) {
    int offset = bplus_buffer_pool_get_offset(page_id);

    // check if this page_id is in the file
    struct stat st = {0};
    fstat(pool->fd, &st);
    if (st.st_size < (offset + sizeof(struct bplus_node_disk))) {
        return NULL;
    }

    lseek(pool->fd, offset, 0);

    struct bplus_node *node = malloc(sizeof(struct bplus_node));
    int r = read(pool->fd, &node->disk, sizeof(struct bplus_node_disk));
    if (r < sizeof(struct bplus_node_disk)) {
        printf("failed to read? what do I do...\n");
        return NULL;
    }

    node->next = NULL;
    node->dirty = 0;

    for (int i = 0; i < MAX_CHILDREN; i++) {
        node->children[i] = NULL;
    }

    return node;
}

struct bplus_node *bplus_node_get_child(
    struct bplus_buffer_pool *pool, struct bplus_node *node, int child_index) {
    assert(!node->disk.is_leaf);

    if (node->children[child_index] != NULL) {
        return node->children[child_index];
    }

    if (node->disk.num_keys >= child_index) {
        printf(
            "loading child from disk: page_id=%d\n",
            node->disk.children[child_index]);
        struct bplus_node *child =
            bplus_buffer_pool_load(pool, node->disk.children[child_index]);
        node->children[child_index] = child;
        return child;
    }

    return NULL;
}

int bplus_buffer_pool_write(
    struct bplus_buffer_pool *pool, struct bplus_node *node) {

    if (node->dirty) {
        int offset = bplus_buffer_pool_get_offset(node->disk.page_id);

        printf(
            "writing node %p (page_id=%d) at offset %d\n", node,
            node->disk.page_id, offset);

        int written = pwrite(pool->fd, &node->disk, sizeof(node->disk), offset);
        if (written != sizeof(node->disk)) {
            perror("writing node");
            return -1;
        }

        node->dirty = 0;
    }

    for (int i = 0; i < MAX_CHILDREN; i++) {
        if (node->children[i] != NULL) {
            int ret = bplus_buffer_pool_write(pool, node->children[i]);
            if (ret < 0) {
                return ret;
            }
        }
    }

    return 0;
}

int bplus_tree_flush(struct bplus_tree *tree) {
    struct bplus_disk_header header = {
        .root_page_id = tree->root->disk.page_id,
    };
    int written = pwrite(tree->pool->fd, &header, sizeof(header), 0);
    if (written != sizeof(header)) {
        return -1;
    }
    return bplus_buffer_pool_write(tree->pool, tree->root);
}

struct bplus_node *
bplus_node_create(struct bplus_buffer_pool *pool, int is_leaf) {
    struct bplus_node *node = malloc(sizeof(struct bplus_node));
    node->disk.num_keys = 0;
    node->disk.is_leaf = is_leaf;
    node->next = NULL;
    node->disk.buffer_position = 0;
    node->disk.page_id = pool->next_page_id;
    node->dirty = 1;
    pool->next_page_id++;

    for (int i = 0; i < MAX_CHILDREN; i++) {
        node->children[i] = NULL;
    }

    if (!is_leaf) {
        // add child for keys greater than max split key
        node->children[0] = bplus_node_create(pool, 1);
    }

    return node;
}

struct bplus_tree *bplus_tree_create(const char *path) {
    struct bplus_buffer_pool *pool = bplus_buffer_pool_init(path);
    struct bplus_tree *tree = malloc(sizeof(struct bplus_tree));
    tree->pool = pool;

    // read header and load root node
    struct bplus_disk_header header = {0};
    pread(pool->fd, &header, sizeof(header), 0);

    struct bplus_node *disk_root =
        bplus_buffer_pool_load(pool, header.root_page_id);
    if (disk_root != NULL) {
        printf("root node %d was loaded from disk\n", header.root_page_id);
        tree->root = disk_root;
    } else {
        tree->root = bplus_node_create(pool, 1);
    }
    tree->height = 1;
    return tree;
}

// check if a key and value can fit in our node
int bplus_node_can_fit(struct bplus_node *node, int key_len, int val_len) {
    if ((node->disk.buffer_position + key_len + val_len) >= NODE_BUFFER_SIZE) {
        return 0;
    } else if (node->disk.num_keys == MAX_KEYS) {
        return 0;
    }

    return 1;
}

struct bplus_insert_index {
    int pos;
    int found;
};

struct bplus_insert_index
bplus_node_find_insert_index(struct bplus_node *node, int key_len, char *key) {

    struct bplus_insert_index ret = {0};

    // find position in metadata arrays
    // TODO: make this a proper binary search
    for (int i = 0; i < node->disk.num_keys; i++) {
        char *stored_key = &node->disk.buf[node->disk.key_offsets[i]];

        // compare length is shortest length of both keys
        int compare_len = node->disk.key_lengths[i];

        if (key_len < compare_len) {
            compare_len = key_len;
        }

        // search forward until our key is less than their key
        int cmp = memcmp(key, stored_key, compare_len);
        if (cmp == 0 && key_len == node->disk.key_lengths[i]) {
            ret.found = 1;
            ret.pos = i;
            return ret;
        } else if (
            cmp < 0 || (cmp == 0 && key_len != node->disk.key_lengths[i])) {
            ret.pos = i;
            return ret;
        }
    }

    ret.pos = node->disk.num_keys;
    return ret;
}

void bplus_node_insert_at(
    struct bplus_node *node,
    struct bplus_insert_index index,
    int key_len,
    char *key,
    int val_len,
    char *val) {

    if (!index.found) {
        // shift metadata arrays
        for (int i = node->disk.num_keys; i > index.pos; i--) {
            node->disk.key_offsets[i] = node->disk.key_offsets[i - 1];
            node->disk.key_lengths[i] = node->disk.key_lengths[i - 1];
            node->disk.value_offsets[i] = node->disk.value_offsets[i - 1];
            node->disk.value_lengths[i] = node->disk.value_lengths[i - 1];
        }

        node->disk.num_keys++;
    }

    // insert data to buffer
    memcpy(&node->disk.buf[node->disk.buffer_position], key, key_len);
    node->disk.key_offsets[index.pos] = node->disk.buffer_position;
    node->disk.buffer_position += key_len;

    memcpy(&node->disk.buf[node->disk.buffer_position], val, val_len);
    node->disk.value_offsets[index.pos] = node->disk.buffer_position;
    node->disk.buffer_position += val_len;

    // update metadata
    node->disk.key_lengths[index.pos] = key_len;
    node->disk.value_lengths[index.pos] = val_len;

    node->dirty = 1;
}

struct bplus_node *bplus_node_split_leaf(
    struct bplus_buffer_pool *pool, struct bplus_node *full_node) {
    assert(full_node->disk.is_leaf);
    assert(full_node->disk.num_keys == MAX_KEYS);

    struct bplus_node *new_node = bplus_node_create(pool, 1);

    int split_point = (MAX_KEYS + 1) / 2;

    for (int i = split_point; i < MAX_KEYS; i++) {
        // copy kv pairs to new node
        struct bplus_insert_index index = {
            .pos = i - split_point,
            .found = 0,
        };
        bplus_node_insert_at(
            new_node, index, full_node->disk.key_lengths[i],
            &full_node->disk.buf[full_node->disk.key_offsets[i]],
            full_node->disk.value_lengths[i],
            &full_node->disk.buf[full_node->disk.value_offsets[i]]);
    }

    full_node->disk.num_keys = split_point;

    new_node->next = full_node->next;
    full_node->next = new_node;

    printf("split node %p to create %p\n", full_node, new_node);

    return new_node;
}

void bplus_node_add_child(struct bplus_node *node, struct bplus_node *child) {
    assert(!node->disk.is_leaf);
    assert(child->disk.is_leaf);

    char *child_key =
        &child->disk.buf[child->disk.key_offsets[child->disk.num_keys - 1]];
    int child_key_len = child->disk.key_lengths[child->disk.num_keys - 1];

    if (!bplus_node_can_fit(node, child_key_len, 0)) {
        printf("internal node full - not implemented!!!\n");
        exit(1);
    }

    struct bplus_insert_index index =
        bplus_node_find_insert_index(node, child_key_len, child_key);

    // shift children
    for (int i = node->disk.num_keys + 1; i > index.pos; i--) {
        node->children[i] = node->children[i - 1];
    }

    // insert key
    bplus_node_insert_at(node, index, child_key_len, child_key, 0, "");

    // insert child
    node->children[index.pos] = child;

    // fix pointers
    for (int i = 0; i <= node->disk.num_keys; i++) {
        if (i == node->disk.num_keys) {
            node->children[i]->next = NULL;
        } else {
            node->children[i]->next = node->children[i + 1];
        }
        node->disk.children[i] = node->children[i]->disk.page_id;
    }
}

struct bplus_node *bplus_node_insert(
    struct bplus_buffer_pool *pool,
    struct bplus_node *node,
    char *key,
    char *value) {
    int key_len = strlen(key);
    int val_len = strlen(value);

    printf("insert (%s => %s) into node %p\n", key, value, node);

    if (node->disk.is_leaf) {
        if (bplus_node_can_fit(node, key_len, val_len)) {
            // normal insert
            struct bplus_insert_index index =
                bplus_node_find_insert_index(node, key_len, key);
            printf("inserting %s at %d\n", key, index.pos);
            bplus_node_insert_at(node, index, key_len, key, val_len, value);

            // no new node
            return NULL;
        } else {
            // split and insert
            struct bplus_node *new_node = bplus_node_split_leaf(pool, node);

            // compare this key to split key to decide which node to insert to
            char *split_key =
                &new_node->disk.buf[new_node->disk.key_offsets[0]];
            int compare_len = new_node->disk.key_lengths[0];
            if (key_len < compare_len) {
                compare_len = key_len;
            }

            // insert to the correct node
            if (memcmp(key, split_key, compare_len) < 0) {
                bplus_node_insert(pool, node, key, value);
            } else {
                bplus_node_insert(pool, new_node, key, value);
            }

            return new_node;
        }
    } else {
        // internal node
        // reuse bplus_node_find_insert_index to find split key of correct child
        struct bplus_insert_index index =
            bplus_node_find_insert_index(node, key_len, key);

        // insert to that child, test if it split
        struct bplus_node *child = bplus_node_get_child(pool, node, index.pos);
        if (child == NULL) {
            printf("error! couldn't load child\n");
            exit(1);
        }
        struct bplus_node *new_node =
            bplus_node_insert(pool, child, key, value);

        //     if it split, add new child to this internal node
        if (new_node != NULL) {
            bplus_node_add_child(node, new_node);
        }
    }

    return NULL;
}

void bplus_tree_insert(struct bplus_tree *tree, char *key, char *val) {
    struct bplus_node *new_node =
        bplus_node_insert(tree->pool, tree->root, key, val);
    if (new_node != NULL) {
        // increase tree height
        struct bplus_node *new_root = bplus_node_create(tree->pool, 0);
        bplus_node_add_child(new_root, tree->root);
        bplus_node_add_child(new_root, new_node);

        tree->root = new_root;
        tree->height += 1;
    }
}

int bplus_node_get(struct bplus_node *node, char *key, char *buf, int buf_len) {
    struct bplus_insert_index index =
        bplus_node_find_insert_index(node, strlen(key), key);

    if (!node->disk.is_leaf) {
        return bplus_node_get(node->children[index.pos], key, buf, buf_len);
    } else if (!index.found) {
        return 1;
    } else {
        int val_len = node->disk.value_lengths[index.pos];
        if (buf_len < (val_len + 1)) {
            printf("buffer too small!\n");
            return -1;
        }

        char *val = &node->disk.buf[node->disk.value_offsets[index.pos]];
        memcpy(buf, val, val_len);
        buf[val_len] = '\0';
    }

    return 0;
}

void bplus_node_print_keys(
    struct bplus_buffer_pool *pool, struct bplus_node *node) {
    if (node->disk.is_leaf) {
        printf("leaf node %p\n", node);
        for (int i = 0; i < node->disk.num_keys; i++) {
            printf(
                "key: %.*s ", node->disk.key_lengths[i],
                &node->disk.buf[node->disk.key_offsets[i]]);
            printf(
                "value: %.*s\n", node->disk.value_lengths[i],
                &node->disk.buf[node->disk.value_offsets[i]]);
        }
    } else {
        printf("internal node %p split keys: ", node);
        for (int i = 0; i < node->disk.num_keys; i++) {
            printf(
                "%d=%.*s ", i, node->disk.key_lengths[i],
                &node->disk.buf[node->disk.key_offsets[i]]);
        }
        printf("\n");
        for (int i = 0; i <= node->disk.num_keys; i++) {
            bplus_node_print_keys(pool, bplus_node_get_child(pool, node, i));
        }
    }
}

void bplus_tree_print_keys(struct bplus_tree *tree) {
    bplus_node_print_keys(tree->pool, tree->root);
}

void bplus_tree_destroy(struct bplus_tree *tree) { free(tree); }
