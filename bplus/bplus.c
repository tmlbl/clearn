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

    for (int i = 0; i < MAX_CHILDREN; i++) {
        node->children[i] = NULL;
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

    printf("slots used: %d/%d\n", node->num_keys, MAX_KEYS);
    printf("data used: %d/%d\n", node->buffer_position, NODE_BUFFER_SIZE);

    return 1;
}

struct bplus_insert_index {
    int pos;
    int found;
};

struct bplus_insert_index bplus_node_find_insert_index(
    struct bplus_node *node, int key_len, char *key, int val_len, char *val) {

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
        if (cmp == 0) {
            ret.found = 1;
            ret.pos = i;
            return ret;
        } else if (cmp < 0) {
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

    return new_node;
}

struct bplus_node *
bplus_node_insert(struct bplus_node *node, char *key, char *value) {
    int key_len = strlen(key);
    int val_len = strlen(value);

    if (node->is_leaf) {
        if (bplus_node_can_fit(node, key_len, val_len)) {
            // normal insert
            struct bplus_insert_index index = bplus_node_find_insert_index(
                node, key_len, key, val_len, value);
            printf("inserting %s at %d\n", key, index.pos);
            bplus_node_insert_at(node, index, key_len, key, val_len, value);

            // no new node
            return NULL;
        } else {
            // split and insert
            struct bplus_node *new_node = bplus_node_split_leaf(node);

            // compare this key to split key to decide which node to insert to
            //
            // insert to the correct node

            return new_node;
        }
    } else {
        // internal node
        // reuse bplus_node_find_insert_index to find split key of correct child
        // insert to that child, test if it split
        //     if it split, add new child to this internal node
        //     if this internal node is full, split the internal node
    }

    return NULL;
}

void bplus_tree_insert(struct bplus_tree *tree, char *key, char *val) {
    // TODO: determine whether to convert root to internal node after split
    bplus_node_insert(tree->root, key, val);
}

void bplus_tree_print_keys(struct bplus_tree *tree) {
    struct bplus_node *root = tree->root;

    for (int i = 0; i < root->num_keys; i++) {
        printf(
            "key: %.*s ", root->key_lengths[i],
            &root->buf[root->key_offsets[i]]);
        printf(
            "value: %.*s\n", root->value_lengths[i],
            &root->buf[root->value_offsets[i]]);
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

    bplus_tree_print_keys(tree);

    bplus_tree_destroy(tree);
    return 0;
}
