#include "bplus.c"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void fill_random(char *buf, int len) {
    for (int i = 0; i < (len - 1); i++) {
        buf[i] = rand() % (97 - 121) + 97;
    }
    buf[len - 1] = '\0';
}

void test_set_and_get() {
    struct bplus_tree *tree = bplus_tree_create("/tmp/bplus_set_and_get");

    bplus_tree_insert(tree, "foo", "bar");

    int ret = bplus_tree_flush(tree);
    if (ret < 0) {
        printf("failed to save\n");
        exit(1);
    }
    bplus_tree_destroy(tree);

    tree = bplus_tree_create("/tmp/bplus_set_and_get");

    char *expect = "bar";
    char buf[32];
    bplus_node_get(tree->root, "foo", &buf[0], 32);
    if (strcmp(buf, "bar") != 0) {
        printf("value doesn't match! %s != %s\n", buf, expect);
        exit(1);
    }
}

int test_split_and_save(int key_size, int num_keys) {
    char *filename = "/tmp/bplus_split_and_save";

    srand(time(NULL));
    struct bplus_tree *tree = bplus_tree_create(filename);

    char *key_buf = malloc(key_size * num_keys);
    char *val_buf = malloc(key_size * num_keys);

    fill_random(key_buf, key_size * num_keys);
    fill_random(val_buf, key_size * num_keys);

    for (int i = 0; i < num_keys; i++) {
        key_buf[(i * key_size) + key_size - 1] = '\0';
        val_buf[(i * key_size) + key_size - 1] = '\0';
        bplus_tree_insert(tree, &key_buf[i * key_size], &val_buf[i * key_size]);
    }

    bplus_tree_print_keys(tree);

    bplus_tree_flush(tree);
    bplus_tree_destroy(tree);

    // load the tree
    tree = bplus_tree_create(filename);

    bplus_tree_print_keys(tree);

    // verify keys are in order
    char *last_key = NULL;
    char *this_key = malloc(16);

    int ret = 0;

    for (int i = 0; i <= tree->root->disk.num_keys; i++) {
        struct bplus_node *node =
            bplus_node_get_child(tree->pool, tree->root, i);
        for (int j = 0; j < node->disk.num_keys; j++) {
            memcpy(this_key, &node->disk.buf[node->disk.key_offsets[j]], 15);
            this_key[15] = '\0';

            if (last_key != NULL && strcmp(last_key, this_key) > 0) {
                printf("keys out of order! %s > %s\n", last_key, this_key);
                ret = 1;
            }

            if (last_key == NULL) {
                last_key = malloc(16);
            }

            strcpy(last_key, this_key);
        }
    }

    bplus_tree_destroy(tree);
    remove(filename);

    return ret;
}

int main(int argc, char *argv[]) {
    int ret = 0;
    // test_set_and_get();
    ret = test_split_and_save(16, 16);
    if (ret != 0) {
        return ret;
    }
    return ret;
}
