CFLAGS := -Wall

.PHONY: all
all: bin bin/poll bin/pthreads bin/forking bin/uring bin/bplus

bin:
	mkdir bin

bin/poll: poll/poll.c
	$(CC) -o bin/poll $(CFLAGS) poll/poll.c

bin/pthreads: pthreads/pthreads.c
	$(CC) -o bin/pthreads $(CFLAGS) pthreads/pthreads.c

bin/bplus: bplus/bplus.c
	$(CC) -o bin/bplus $(CFLAGS) bplus/bplus.c

bin/forking: forking/forking.c
	$(CC) -o bin/forking $(CFLAGS) forking/forking.c

bin/uring: uring/uring.c
	$(CC) -o bin/uring $(CFLAGS) uring/uring.c -luring
