/*
 * SMalloc -- a *static* memory allocator.
 *
 * See README for a complete description.
 *
 * SMalloc is MIT licensed.
 * Copyright (c) 2017 Andrey Rys.
 * Written during Aug2017.
 */

#ifndef _SMALLOC_H
#define _SMALLOC_H

#include <stddef.h>
#include <stdint.h>

extern void (*register_region)(const char*, int, void*, size_t); 
extern void (*register_growth)(int, void*, size_t);

struct smalloc_pool;
struct smalloc_mpools;

typedef struct smalloc_pool * (*smalloc_oom_handler)(struct smalloc_mpools *, size_t);

/* describes static pool, if you're going to use multiple pools at same time */
struct smalloc_pool {
	void *pool; /* pointer to your pool */
	size_t pool_size; /* it's size. Must be aligned with sm_align_pool. */
	int do_zero; /* zero pool before use and all the new allocations from it. */
  size_t num_elems;
};

struct smalloc_mpools {
    struct smalloc_pool *pools;
    size_t next;
    size_t capacity;
    size_t pools_size;
	smalloc_oom_handler oomfn; /* this will be called, if non-NULL, on OOM condition in pool */
};

struct smalloc_pool_list {
    size_t capacity;
    size_t next;
    struct {
        int64_t stack[20]; // for now only a test: will need to make it some kind of list if it works...
        size_t sp;
        size_t gen;
    } free_ids;

    struct smalloc_mpools *mpools;
};

/* a default one which is initialised with sm_set_default_pool. */
extern struct smalloc_pool smalloc_curr_pool;

extern struct smalloc_pool_list pool_list; // (elsa) ADDED THIS, 
                    // need to be initialized first by calling sm_pool_list_init()

/* undefined behavior handler is called on typical malloc UB situations */
typedef void (*smalloc_ub_handler)(struct smalloc_pool *, const void *);

void sm_set_ub_handler(smalloc_ub_handler);

int sm_align_pool(struct smalloc_pool *);
int sm_set_pool(struct smalloc_pool *, void *, size_t, int);
int sm_set_default_pool(void *, size_t, int);
int sm_release_pool(struct smalloc_pool *);
int sm_release_default_pool(void);

// (elsa) ADDED THESE
// they all work on the default pool_list
int sm_pools_init(size_t, size_t, size_t);
int64_t sm_add_mpool(const char* name);
int sm_release_pools(void);

void *sm_malloc_from_pool(int64_t, size_t);
void sm_free_from_pool(void *);


/* Use these with multiple pools which you control */

void *sm_malloc_pool(struct smalloc_mpools *, size_t);
void sm_free_pool(struct smalloc_pool *, void *);
int sm_alloc_valid_pool(struct smalloc_pool *spool, const void *p);
int sm_malloc_stats_pool(struct smalloc_pool *, size_t *, size_t *, size_t *, int *);

/* Use these when you use just default smalloc_curr_pool pool */

void sm_free(void *);
int sm_alloc_valid(const void *p); /* verify pointer without intentional crash */
/*
 * get stats: total used, user used, total free, nr. of allocated blocks.
 * any of pointers maybe set to NULL, but at least one must be non NULL.
 */
int sm_malloc_stats(size_t *, size_t *, size_t *, int *);

/* Adding a method to extract the Id */
int64_t sm_get_object_id(void* p);

#endif
