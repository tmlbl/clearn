#include "liburing/io_uring.h"
#include "sys/stat.h"
#include <liburing.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define QUEUE_DEPTH 32

int main() {
    struct io_uring ring;
    int ret;
    char *buffer;
    struct stat st;
    const char *filepath = "uring/uring.c";

    ret = io_uring_queue_init(QUEUE_DEPTH, &ring, 0);
    if (ret < 0) {
        fprintf(stderr, "queue_init failed: %s\n", strerror(-ret));
        return 1;
    }

    printf("initialized io_uring queue\n");

    if (stat(filepath, &st) != 0) {
        perror("stat");
        io_uring_queue_exit(&ring);
        return 1;
    }

    printf("allocating %lo bytes to read %s\n", st.st_size, filepath);

    buffer = malloc(st.st_size);
    if (!buffer) {
        perror("malloc");
        io_uring_queue_exit(&ring);
        return 1;
    }

    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        perror("open file");
        io_uring_queue_exit(&ring);
        free(buffer);
        return 1;
    }

    // submission queue entry
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    io_uring_prep_read(sqe, fd, buffer, st.st_size - 1, 0);
    io_uring_sqe_set_data(sqe, buffer);

    // submit
    int num_ops = io_uring_submit(&ring);
    printf("submitted %d operations\n", num_ops);

    // wait for completion
    int completed = 0;
    while (completed < num_ops) {
        // completion queue entry
        struct io_uring_cqe *cqe;
        int cqe_ret = io_uring_wait_cqe(&ring, &cqe);
        if (cqe_ret < 0) {
            fprintf(stderr, "wait_cqe failure: %s\n", strerror(-cqe_ret));
            break;
        }

        // get our data
        char *data = (char *)io_uring_cqe_get_data(cqe);

        // add null termination for printing
        data[st.st_size - 1] = '\0';
        printf("your data, sir: %s\n", data);

        // let uring know we got it
        io_uring_cqe_seen(&ring, cqe);
        completed++;
    }

    free(buffer);
    io_uring_queue_exit(&ring);
    return 0;
}
