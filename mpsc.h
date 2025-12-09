/**
 * An intrusive MPSC (multi-provider, single consumer) queue implementation.
 *
 * Once initialized the queue can't be moved, its reference must remain stable
 * and you should never copy it.
 */

#ifndef MPSC_H_INCLUDE
#define MPSC_H_INCLUDE

#include <stdatomic.h>

#define container_of(ptr, type, member)                                        \
	(type *)((char *)(ptr) - offsetof(type, member))

/**
 * A node defined an item in the queue. It contains no data field. To store data
 * along a node, you must embed the node in your own struct and use container_of
 * macros to retrieve it.
 */
struct mpsc_node {
	_Atomic(struct mpsc_node *) next;
};

struct mpsc {
	_Atomic(struct mpsc_node *) head;
	_Atomic(struct mpsc_node *) tail;
	struct mpsc_node stub;
};

/**
 * Initialize the queue. Once initialized a queue must remain at the same
 * location in memory and not be copied.
 */
void mpsc_init(struct mpsc *q);

/**
 * Push node to back of queue. This is safe to call from multiple threads in
 * parallel.
 */
void mpsc_push(struct mpsc *q, struct mpsc_node *n);

/**
 * Pop node from front of queue. This must be called from a single thread at a
 * time.
 */
struct mpsc_node *mpsc_pop(struct mpsc *q);

#ifdef MPSC_IMPLEMENTATION

void mpsc_init(struct mpsc *q)
{
	q->head = q->tail = &q->stub;
	q->stub.next = NULL;
}

void mpsc_push(struct mpsc *q, struct mpsc_node *n)
{
	atomic_store(&n->next, NULL);
	struct mpsc_node *prev = atomic_exchange(&q->tail, n);
	atomic_store(&prev->next, n);
}

struct mpsc_node *mpsc_pop(struct mpsc *q)
{
	struct mpsc_node *head = atomic_load(&q->head);
	struct mpsc_node *next = atomic_load(&head->next);

	/* Queue may be empty ? */
	if (head == &q->stub) {
		/* Definitely empty. */
		if (next == NULL)
			return NULL;

		/* Stub has a next value, use it as head. */
		atomic_store(&q->head, next);
		head = next;
		next = atomic_load(&head->next);
	}

	/* This is NOT the last element of the queue */
	if (next != NULL) {
		atomic_store(&q->head, next);
		head->next = NULL;
		return head;
	}

	/* This is the last element of the queue. */
	struct mpsc_node *tail = atomic_load(&q->head);
	if (tail != head)
		return NULL;

	/* Push stub so head/tail is never NULL */
	mpsc_push(q, &q->stub);

	/* Retrieve next head */
	next = atomic_load(&head->next);
	if (next != NULL) {
		/* Store next head as head. */
		atomic_store(&q->head, next);
		head->next = NULL;
		return head;
	}

	return NULL;
}

#endif /* MPSC_IMPLEMENTATION */
#endif /* MPSC_H_INCLUDE */
