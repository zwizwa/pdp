/*
  util.h - (c) 2004 Tom Schouten.
  this software is in the public domain.

  This is supposed to be my low-level data structures toolbox.
  Defined as inline static to avoid version and symbol clashes.
*/


#ifndef ZL_UTIL_H
#define ZL_UTIL_H

#include <stdlib.h>

// linkage
#define INLINE inline static

// growable array object
typedef struct {
    int size;
    int elements;
    void **data;
} buf_t;


// fixed 2^n size queue object
typedef struct {
    void **data;
    unsigned int mask;
    unsigned int read;
    unsigned int write;
} queue_t;


typedef buf_t fifo_t; // same

/* small utils */

INLINE int imax(int x, int y){return (x > y) ? x : y;}
INLINE int imin(int x, int y){return (x < y) ? x : y;}

/* protos */

INLINE buf_t *buf_new(int size); ///< create a new growable array
INLINE void buf_add(buf_t *x, void *thing); ///< add an element
INLINE int buf_lookup(buf_t *x, void *thing); ///< lookup element and return index
INLINE int buf_add_to_set(buf_t *x, void *thing); ///< add if not already present
INLINE void buf_clear(buf_t *x);
INLINE void buf_free(buf_t *x);
INLINE void buf_remove(buf_t *x, void *thing);
INLINE int buf_allot(buf_t *x, int bytes); ///< allocate memory and return cell index

INLINE void stack_clear(fifo_t *x);
INLINE void stack_free(fifo_t *x);
INLINE int stack_elements(fifo_t *x);
INLINE void stack_push(fifo_t *x, void *data);
INLINE void *stack_pop(fifo_t *x);
INLINE void stack_clear(fifo_t *x);
INLINE void stack_free(fifo_t *x);
INLINE fifo_t *stack_new(int elements);





/* code */

INLINE buf_t *buf_new(int size){
    buf_t *x = malloc(sizeof(*x));
    x->size = size;
    x->elements = 0;
    x->data = malloc(sizeof(void *) * size);
    return x;
}

INLINE void buf_add(buf_t *x, void *thing){
    if (x->elements == x->size){
	x->size *= 2;
	x->data = realloc(x->data, x->size * sizeof(void *));
    }
    x->data[x->elements++] = thing;
}

INLINE int buf_lookup(buf_t *x, void *thing){
    int i;
    for (i=0; i<x->elements; i++){
	if (x->data[i] == thing) return i; // found
    }
    return -1; // not found
}

// add if not in there
INLINE int buf_add_to_set(buf_t *x, void *thing){
    int indx = buf_lookup(x, thing);
    if (indx < 0){ // add if not found
	indx = x->elements;
	buf_add(x, thing);
    }
    return indx;
}

INLINE void buf_remove(buf_t *x, void *thing){
    int indx = buf_lookup(x, thing);
    if (indx < 0) return; // not here
    x->elements--;
    x->data[indx] = x->data[x->elements];
}

INLINE void buf_clear(buf_t *x){x->elements = 0;}
INLINE void buf_free(buf_t *x){free (x->data); free(x);}

INLINE int buf_allot(buf_t *x, int bytes){
    int words = ((bytes-1) / sizeof(int)) - 1;
    int indx = x->elements;
    while(words--) buf_add(x, 0);
    return indx;
}


INLINE fifo_t *stack_new(int nb_elements){
    fifo_t *x = buf_new(nb_elements);
    return x;
}
INLINE int stack_elements(fifo_t *x){return x->elements;}
INLINE void stack_push(fifo_t *x, void *data){buf_add(x, data);}

INLINE void *stack_pop(fifo_t *x){
    if (!x->elements) return 0;
    return x->data[--x->elements];
}
    
INLINE void stack_clear(fifo_t *x){buf_clear(x);}
INLINE void stack_free(fifo_t *x){buf_free(x);}





#if 0  // FIXME: name conflicts with queue in leaf/queue.h
INLINE queue_t *queue_new(int logsize);
INLINE void queue_free(queue_t *queue);
INLINE void queue_write(queue_t *queue, void *thing);
INLINE void *queue_read(queue_t *queue);
INLINE unsigned int queue_elements(queue_t *queue);
INLINE int queue_full(queue_t *queue);

INLINE queue_t *queue_new(int logsize){
    queue_t *x = malloc(sizeof(*x));
    x->data = malloc(1<<logsize * sizeof(void *));
    x->mask = (1<<logsize) - 1;
    x->read = 0;
    x->write = 0;
    return x;
}
INLINE void queue_free(queue_t *x){
    free(x->data);
    free(x);
}

INLINE unsigned int queue_elements(queue_t *x){
    return (x->write - x->read) & x->mask;
}
INLINE int queue_full(queue_t *x){
    return (x->mask == queue_elements(x));
}

INLINE void queue_write(queue_t *x, void *thing){
    int i = x->write;
    x->data[i++] = thing;
    x->write = i & x->mask;
}

INLINE void *queue_read(queue_t *x){
    int i = x->read;
    void *thing = x->data[i++];
    x->write = i & x->mask;
    return thing;
}

#endif

#endif



