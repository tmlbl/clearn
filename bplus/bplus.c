#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ORDER 4
#define MAX_KEYS (2 * ORDER - 1)
#define MAX_CHILDREN (2 * ORDER)
#define NODE_BUFFER_SIZE 4096

struct bplus_node {
    char buf[NODE_BUFFER_SIZE];

    uint16_t key_offsets[MAX_KEYS];
    uint16_t key_lengths[MAX_KEYS];
    uint16_t value_offsets[MAX_KEYS];
    uint16_t value_lengths[MAX_KEYS];

    int num_keys;
    uint16_t buffer_position;
    struct bplus_node *children[MAX_CHILDREN];
    struct bplus_node *next;
    int is_leaf;
};

struct bplus_tree {
    struct bplus_node *root;
    int height;
};

struct bplus_node *bplus_node_create(int is_leaf) {
    struct bplus_node *node = malloc(sizeof(struct bplus_node));
    node->num_keys = 0;
    node->is_leaf = is_leaf;
    node->next = NULL;
    node->buffer_position = 0;

    for (int i = 0; i < MAX_CHILDREN; i++) {
        node->children[i] = NULL;
    }

    if (!is_leaf) {
        // add child for keys greater than max split key
        node->children[0] = bplus_node_create(1);
    }

    return node;
}

struct bplus_tree *bplus_tree_create(void) {
    struct bplus_tree *tree = malloc(sizeof(struct bplus_tree));
    tree->root = bplus_node_create(1);
    tree->height = 1;
    return tree;
}

// check if a key and value can fit in our node
int bplus_node_can_fit(struct bplus_node *node, int key_len, int val_len) {
    if ((node->buffer_position + key_len + val_len) >= NODE_BUFFER_SIZE) {
        return 0;
    } else if (node->num_keys == MAX_KEYS) {
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
    for (int i = 0; i < node->num_keys; i++) {
        char *stored_key = &node->buf[node->key_offsets[i]];

        // compare length is shortest length of both keys
        int compare_len = node->key_lengths[i];

        if (key_len < compare_len) {
            compare_len = key_len;
        }

        // search forward until our key is less than their key
        int cmp = memcmp(key, stored_key, compare_len);
        if (cmp == 0 && key_len == node->key_lengths[i]) {
            ret.found = 1;
            ret.pos = i;
            return ret;
        } else if (cmp < 0 || (cmp == 0 && key_len != node->key_lengths[i])) {
            ret.pos = i;
            return ret;
        }
    }

    ret.pos = node->num_keys;
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
        for (int i = node->num_keys; i > index.pos; i--) {
            node->key_offsets[i] = node->key_offsets[i - 1];
            node->key_lengths[i] = node->key_lengths[i - 1];
            node->value_offsets[i] = node->value_offsets[i - 1];
            node->value_lengths[i] = node->value_lengths[i - 1];
        }

        node->num_keys++;
    }

    // insert data to buffer
    memcpy(&node->buf[node->buffer_position], key, key_len);
    node->key_offsets[index.pos] = node->buffer_position;
    node->buffer_position += key_len;

    memcpy(&node->buf[node->buffer_position], val, val_len);
    node->value_offsets[index.pos] = node->buffer_position;
    node->buffer_position += val_len;

    // update metadata
    node->key_lengths[index.pos] = key_len;
    node->value_lengths[index.pos] = val_len;
}

struct bplus_node *bplus_node_split_leaf(struct bplus_node *full_node) {
    assert(full_node->is_leaf);
    assert(full_node->num_keys == MAX_KEYS);

    struct bplus_node *new_node = bplus_node_create(1);

    int split_point = (MAX_KEYS + 1) / 2;

    for (int i = split_point; i < MAX_KEYS; i++) {
        // copy kv pairs to new node
        struct bplus_insert_index index = {
            .pos = i - split_point,
            .found = 0,
        };
        bplus_node_insert_at(
            new_node, index, full_node->key_lengths[i],
            &full_node->buf[full_node->key_offsets[i]],
            full_node->value_lengths[i],
            &full_node->buf[full_node->value_offsets[i]]);
    }

    full_node->num_keys = split_point;

    new_node->next = full_node->next;
    full_node->next = new_node;

    printf("split node %p to create %p\n", full_node, new_node);

    return new_node;
}

void bplus_node_add_child(struct bplus_node *node, struct bplus_node *child) {
    assert(!node->is_leaf);
    assert(child->is_leaf);

    char *child_key = &child->buf[child->key_offsets[child->num_keys - 1]];
    int child_key_len = child->key_lengths[child->num_keys - 1];

    if (!bplus_node_can_fit(node, child_key_len, 0)) {
        printf("internal node full - not implemented!!!\n");
        exit(1);
    }

    struct bplus_insert_index index =
        bplus_node_find_insert_index(node, child_key_len, child_key);

    // shift children
    for (int i = node->num_keys + 1; i > index.pos; i--) {
        node->children[i] = node->children[i - 1];
    }

    // insert key
    bplus_node_insert_at(node, index, child_key_len, child_key, 0, "");

    // insert child
    node->children[index.pos] = child;

    // fix pointers
    for (int i = 0; i < node->num_keys; i++) {
        node->children[i]->next = node->children[i + 1];
    }
}

struct bplus_node *
bplus_node_insert(struct bplus_node *node, char *key, char *value) {
    int key_len = strlen(key);
    int val_len = strlen(value);

    printf("insert (%s => %s) into node %p\n", key, value, node);

    if (node->is_leaf) {
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
            struct bplus_node *new_node = bplus_node_split_leaf(node);

            // compare this key to split key to decide which node to insert to
            char *split_key = &new_node->buf[new_node->key_offsets[0]];
            int compare_len = new_node->key_lengths[0];
            if (key_len < compare_len) {
                compare_len = key_len;
            }

            // insert to the correct node
            if (memcmp(key, split_key, compare_len) < 0) {
                bplus_node_insert(node, key, value);
            } else {
                bplus_node_insert(new_node, key, value);
            }

            return new_node;
        }
    } else {
        // internal node
        // reuse bplus_node_find_insert_index to find split key of correct child
        struct bplus_insert_index index =
            bplus_node_find_insert_index(node, key_len, key);

        // insert to that child, test if it split
        struct bplus_node *new_node =
            bplus_node_insert(node->children[index.pos], key, value);

        //     if it split, add new child to this internal node
        if (new_node != NULL) {
            bplus_node_add_child(node, new_node);
        }
    }

    return NULL;
}

void bplus_tree_insert(struct bplus_tree *tree, char *key, char *val) {
    struct bplus_node *new_node = bplus_node_insert(tree->root, key, val);
    if (new_node != NULL) {
        // increase tree height
        struct bplus_node *new_root = bplus_node_create(0);
        bplus_node_add_child(new_root, tree->root);
        bplus_node_add_child(new_root, new_node);

        tree->root = new_root;
        tree->height += 1;
    }
}

int bplus_node_get(struct bplus_node *node, char *key, char *buf, int buf_len) {
    struct bplus_insert_index index =
        bplus_node_find_insert_index(node, strlen(key), key);

    if (!node->is_leaf) {
        return bplus_node_get(node->children[index.pos], key, buf, buf_len);
    } else if (!index.found) {
        return 1;
    } else {
        int val_len = node->value_lengths[index.pos];
        if (buf_len < (val_len + 1)) {
            printf("buffer too small!\n");
            return -1;
        }

        char *val = &node->buf[node->value_offsets[index.pos]];
        memcpy(buf, val, val_len);
        buf[val_len] = '\0';
    }

    return 0;
}

void bplus_node_print_keys(struct bplus_node *node) {
    if (node->is_leaf) {
        printf("leaf node %p\n", node);
        for (int i = 0; i < node->num_keys; i++) {
            printf(
                "key: %.*s ", node->key_lengths[i],
                &node->buf[node->key_offsets[i]]);
            printf(
                "value: %.*s\n", node->value_lengths[i],
                &node->buf[node->value_offsets[i]]);
        }
        if (node->next != NULL) {
            bplus_node_print_keys(node->next);
        }
    } else {
        printf("internal node %p split keys: ", node);
        for (int i = 0; i < node->num_keys; i++) {
            printf(
                "%d=%.*s ", i, node->key_lengths[i],
                &node->buf[node->key_offsets[i]]);
        }
        printf("\n");
        bplus_node_print_keys(node->children[0]);
    }
}

void bplus_tree_destroy(struct bplus_tree *tree) { free(tree); }

int main(int argc, char *argv[]) {
    struct bplus_tree *tree = bplus_tree_create();

    printf("created a tree\n");

    bplus_tree_insert(tree, "foo", "bar");
    bplus_tree_insert(tree, "dogs", "drool");
    bplus_tree_insert(tree, "cats", "rule");
    bplus_tree_insert(tree, "zoo", "animals");
    bplus_tree_insert(tree, "fart", "poop");

    // overwrite?
    bplus_tree_insert(tree, "foo", "baz");

    bplus_tree_insert(tree, "more", "stuff");
    bplus_tree_insert(tree, "lala", "foofoo");

    // this is one too many
    bplus_tree_insert(tree, "straw", "camel's back");

    // now we are calling insert when our root is an internal node
    bplus_tree_insert(tree, "internal", "node");

    // our final split key is zoo, what if we add something greater?
    bplus_tree_insert(tree, "zzz", "end");

    bplus_node_print_keys(tree->root);

    // let's read our index!
    char buf[128];
    bplus_node_get(tree->root, "foo", &buf[0], 128);
    printf("query result: foo == %s\n", buf);

    bplus_tree_destroy(tree);
    return 0;
}
