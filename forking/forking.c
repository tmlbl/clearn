#include <stdio.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    int pid = fork();
    if (pid == 0) {
        // in child
        printf("hello from child, my pid is %d\n", getpid());

        for (int i = 0; i < 10; i++) {
            sleep(1);
            printf("zzz...\n");
        }

        _exit(0);
    } else {
        // in parent
        printf("parent: spawned child %d, waiting...\n", pid);

        int status;
        int wait_pid = wait(&status);
        printf("hello my child. status: %d pid: %d\n", status, wait_pid);
    }

    return 0;
}
