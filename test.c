#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

#define MPSC_IMPLEMENTATION
#include "mpsc.h"

#define TRY(fn)                                                                \
	do {                                                                   \
		int err = fn();                                                \
		if (err) {                                                     \
			printf("KO	%s: %d\n", #fn, err);                  \
			return err;                                            \
		} else {                                                       \
			printf("OK 	%s\n", #fn);                           \
		}                                                              \
	} while (0)

#define THREADS 8
#define NODES_PER_THREAD 4096

static int init_push_pop_singlethread(void)
{
	struct mpsc q;
	struct mpsc_node n1, n2, n3;

	mpsc_init(&q);

	mpsc_push(&q, &n1);
	mpsc_push(&q, &n2);
	mpsc_push(&q, &n3);

	if (mpsc_pop(&q) != &n1)
		return 1;
	if (mpsc_pop(&q) != &n2)
		return 2;
	if (mpsc_pop(&q) != &n3)
		return 3;

	return 0;
}

struct threaddata {
	struct mpsc *q;
	struct mpsc_node nodes[NODES_PER_THREAD];
	pthread_t thread;
};

static void *push_nodes(void *ptr)
{
	struct threaddata *td = ptr;

	for (int i = 0; i < NODES_PER_THREAD; i++) {
		mpsc_push(td->q, &td->nodes[i]);
	}

	return NULL;
}

static int init_push_pop_multithread(void)
{
	struct mpsc q;
	mpsc_init(&q);

	// Spawn threads.
	struct threaddata tds[THREADS];
	for (int i = 0; i < THREADS; i++) {
		tds[i].q = &q;

		int err =
		    pthread_create(&tds[i].thread, NULL, push_nodes, &tds[i]);
		if (err)
			return err;
	}

	int popped = 0;
	while (popped < THREADS * NODES_PER_THREAD) {
		struct mpsc_node *n = mpsc_pop(&q);

		if (n == NULL) {
			sched_yield();
			continue;
		}

		popped += 1;
	}

	// Wait for threads.
	for (int i = 0; i < THREADS; i++) {
		int err = pthread_join(tds[i].thread, NULL);
		if (err)
			return err;
	}

	// Control that no nodes were lost.
	if (popped != THREADS * NODES_PER_THREAD) {
		return 1;
	}

	return 0;
}

int main(void)
{
	TRY(init_push_pop_singlethread);
	TRY(init_push_pop_multithread);
	printf("all tests are OK\n");
	return 0;
}
