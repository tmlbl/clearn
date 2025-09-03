CFLAGS := -Wall

.PHONY: all
all: bin bin/poll bin/pthreads bin/forking bin/uring bin/bplus_test

.PHONY: clean
clean:
	rm bin/*

bin:
	mkdir bin

bin/poll: poll/poll.c
	$(CC) -o bin/poll $(CFLAGS) poll/poll.c

bin/pthreads: pthreads/pthreads.c
	$(CC) -o bin/pthreads $(CFLAGS) pthreads/pthreads.c

bin/bplus_test: bplus/bplus.c bplus/bplus_test.c
	$(CC) -o bin/bplus_test $(CFLAGS) bplus/bplus_test.c -g

bin/forking: forking/forking.c
	$(CC) -o bin/forking $(CFLAGS) forking/forking.c

bin/uring: uring/uring.c
	$(CC) -o bin/uring $(CFLAGS) uring/uring.c -luring
