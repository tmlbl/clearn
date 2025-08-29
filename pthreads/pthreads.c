#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

pthread_mutex_t mx = PTHREAD_MUTEX_INITIALIZER;
int state = 1;

void *worker(void *data) {
    char *buf = (char *)data;

    printf("I am a worker\n");

    pthread_mutex_lock(&mx);
    printf("worker has the lock\n");
    state = 2;
    sleep(2);
    pthread_mutex_unlock(&mx);

    memcpy(buf, "Hello!", 7);

    return NULL;
}

int main(int argc, char *argv[]) {
    char *buf = malloc(4096);

    printf("state is %d\n", state);

    pthread_t thread;
    pthread_create(&thread, NULL, worker, buf);

    sleep(1);

    printf("I am trying to lock...\n");
    pthread_mutex_lock(&mx);
    printf("I have the lock!\n");
    pthread_mutex_unlock(&mx);

    pthread_join(thread, NULL);

    printf("pthread said: %s\n", buf);

    printf("state is now %d\n", state);

    free(buf);
    return 0;
}
