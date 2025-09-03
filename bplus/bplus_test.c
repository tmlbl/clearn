#include "bplus.c"
#include <stdio.h>
#include <stdlib.h>

void test_set_and_get() {
    struct bplus_tree *tree = bplus_tree_create();

    bplus_tree_insert(tree, "foo", "bar");

    char *expect = "bar";
    char buf[32];
    bplus_node_get(tree->root, "foo", &buf[0], 32);
    if (strcmp(buf, "bar") != 0) {
        printf("value doesn't match! %s != %s", buf, expect);
    }

    bplus_tree_destroy(tree);
}

void test_many_data() {
    struct bplus_tree *tree = bplus_tree_create();

    char key_buf[16];
    char val_buf[16];

    for (int i = 0; i < 1000; i++) {
        for (int j = 0; j < 15; j++) {
            char c = rand() % (97 - 121) + 97;
            key_buf[j] = c;
            val_buf[j] = c;
        }

        key_buf[15] = '\0';
        val_buf[15] = '\0';

        printf("insertion #%d\n", i);

        bplus_tree_insert(tree, &key_buf[0], &val_buf[0]);
    }

    bplus_tree_destroy(tree);
}

int main(int argc, char *argv[]) {
    test_set_and_get();
    test_many_data();
    return 0;
}
