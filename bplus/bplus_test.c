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

void test_split_and_save() {
    char *filename = "/tmp/bplus_split_and_save";

    srand(time(NULL));
    struct bplus_tree *tree = bplus_tree_create(filename);

    char key_buf[16];
    char val_buf[16];

    for (int i = 0; i < (MAX_KEYS * 2); i++) {
        fill_random(&key_buf[0], 16);
        fill_random(&val_buf[0], 16);

        printf("insertion #%d\n", i);

        bplus_tree_insert(tree, &key_buf[0], &val_buf[0]);
    }

    bplus_tree_print_keys(tree);

    bplus_tree_flush(tree);
    bplus_tree_destroy(tree);

    // load the tree
    tree = bplus_tree_create(filename);

    bplus_tree_print_keys(tree);

    bplus_tree_destroy(tree);
    remove(filename);
}

int main(int argc, char *argv[]) {
    // test_set_and_get();
    test_split_and_save();
    return 0;
}
