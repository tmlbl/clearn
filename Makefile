CFLAGS := "-Wall"

.PHONY: all
all: bin/poll bin/pthreads

bin/poll: poll/poll.c
	$(CC) -o bin/poll $(CFLAGS) poll/poll.c

bin/pthreads: pthreads/pthreads.c
	$(CC) -o bin/poll $(CFLAGS) poll/poll.c
