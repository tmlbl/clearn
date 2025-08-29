#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

int setup_fifo(const char *fifo_name) {
    if (mkfifo(fifo_name, S_IRWXU) == -1) {
        printf("error creating %s\n", fifo_name);
        return -1;
    }

    int fd = open(fifo_name, O_RDWR);
    if (fd == -1) {
        printf("error opening %s\n", fifo_name);
    }

    return fd;
}

int should_poll = 1;

void handle_interrupt(int s) { should_poll = 0; }

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_interrupt);

    int fda = setup_fifo("a.fifo");
    int fdb = setup_fifo("b.fifo");

    if (fda < 0 || fdb < 0) {
        _exit(1);
    }

    struct pollfd fds[2] = {0};
    fds[0].fd = fda;
    fds[0].events = POLLIN;
    fds[1].fd = fdb;
    fds[1].events = POLLIN;

    printf("waiting for something to happen...\n");
    while (should_poll) {
        int ready = poll(fds, 2, 1000);

        if (ready == -1) {
            perror("poll error");
            break;
        } else if (ready == 0) {
            printf("nothing ever happens.\n");
            continue;
        }

        char buf[256];

        for (int i = 0; i < 2; i++) {
            if (fds[i].revents & POLLIN) {
                int n = read(fds[i].fd, &buf, sizeof(buf));
                buf[n] = '\0';
                printf("got from fd %d: %s\n", i, buf);
            }
        }
    }

    unlink("a.fifo");
    unlink("b.fifo");
    return 0;
}
