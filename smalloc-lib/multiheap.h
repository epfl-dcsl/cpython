/*
 * M(ulti)H(eap) allocator.
 * This adds a layer on top of SMalloc library to manage multiple heaps.
 */
#ifndef _MHEAP_H
#define _MHEAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include "smalloc.h"
/*
 * The multi-heap implementation is as follows:
 * mh_arena: contiguous region of virtual memory from which objects can be allocated.
 * mh_heap: a collection of mh_arena that all share the same id.
 * mh_allocator: a collection of mh_heaps.
 */

/* The initial size for the mh_allocator array of mheaps. */
#define MH_INITIAL_MHEAPS_NB 100
#define MH_PAGE_SIZE 0x1000
#define MH_DEFAULT_POOL_SIZE (100 * MH_PAGE_SIZE)

struct mh_heap;

typedef struct mh_arena {
  struct mh_heap*  parent;    /* pointer to the mh_heap that owns this arena. */ 
  struct mh_arena* prev;      /* pointer to the prev mh_heap. */
  struct mh_arena* next;      /* pointer to the next mh_heap. */
  uint64_t num_elem;          /* number of elements allocated inside the arena. */
  struct smalloc_pool pool;   /* memory pool from smalloc. */  
} mh_arena;

typedef struct mh_heap {
  int64_t pool_id;  /* This is the id of this heap.*/

  mh_arena* lhead;  /* pointer to the first element of the list. */ 
  mh_arena* ltail;  /* pointer to the last element of the list. */
  uint32_t lsize;   /* size of the list .*/
} mh_heap;

struct mh_allocator {
  int64_t next_id;        /* the next id available for mheaps. */ 
  struct mh_heap** mheaps; /* an array of mids indexed by pool_ids, grows dynamically. */
  uint32_t mhsize;       /* the current size of mheaps. */
};

extern struct mh_allocator mhallocator;

/* Hooks for litterbox */
extern void (*register_id)(const char*, int);
extern void (*register_growth)(int, void*, size_t);

/* mh_allocator functions */
void mh_init_allocator();
int64_t mh_new_id(const char*);
void *mh_malloc(int64_t id, size_t size);
void *mh_calloc(int64_t id, size_t nmemb, size_t size);
void *mh_realloc(void* ptr, size_t size);
void mh_free(void* ptr);
int64_t mh_get_id(void* ptr);

/* mh_heap functions */
void mh_heap_init(int64_t id, struct mh_heap* heap);
void *mh_heap_malloc(mh_heap* heap, size_t size);
void mh_heap_free(mh_heap *heap, void *ptr);

/* mh_arena functions */
mh_arena* mh_new_arena(mh_heap* parent, size_t pool_size);

#ifdef __cplusplus
}
#endif

#endif
