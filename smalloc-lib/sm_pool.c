/*
 * This file is a part of SMalloc.
 * SMalloc is MIT licensed.
 * Copyright (c) 2017 Andrey Rys.
 */

#define _GNU_SOURCE
#include "smalloc_i.h"
// (elsa) ADDED THESE for calloc/free and mmap
#include <stdlib.h>
#include <sys/mman.h> 
#include <stdio.h>
#include <assert.h>

struct smalloc_pool smalloc_curr_pool;
struct smalloc_pool_list pool_list; // (elsa) ADDED THIS

// callback to register a new region, arguments are: 
// pkg id, addr start, size or region, protection.
void (*register_region)(const char*, int, void*, size_t) = NULL;
void (*register_growth)(int, void*, size_t) = NULL;

struct smalloc_pool *sm_add_pool(struct smalloc_mpools *, size_t);
int mpools_initialize(struct smalloc_mpools *, size_t, size_t,
        size_t, size_t, smalloc_oom_handler);


int smalloc_verify_pool(struct smalloc_pool *spool)
{
	if (!spool->pool || !spool->pool_size) return 0;
	if (spool->pool_size % HEADER_SZ) return 0;
	return 1;
}

int sm_align_pool(struct smalloc_pool *spool)
{
	size_t x;

	if (smalloc_verify_pool(spool)) return 1;

	x = spool->pool_size % HEADER_SZ;
	if (x) spool->pool_size -= x;
	if (spool->pool_size <= MIN_POOL_SZ) {
		errno = ENOSPC;
		return 0;
	}

	return 1;
}

int sm_set_pool(struct smalloc_pool *spool, void *new_pool, size_t new_pool_size, int do_zero)
{
	if (!spool) {
		errno = EINVAL;
		return 0;
	}

	if (!new_pool || !new_pool_size) {
		if (smalloc_verify_pool(spool)) {
			if (spool->do_zero) memset(spool->pool, 0, spool->pool_size);
			memset(spool, 0, sizeof(struct smalloc_pool));
			return 1;
		}

		errno = EINVAL;
		return 0;
	}

	spool->pool = new_pool;
	spool->pool_size = new_pool_size;
    spool->num_elems = 0;
	if (!sm_align_pool(spool)) return 0;

	if (do_zero) {
		spool->do_zero = do_zero;
		memset(spool->pool, 0, spool->pool_size);
	}

	return 1;
}

int sm_set_default_pool(void *new_pool, size_t new_pool_size, int do_zero)
{
	return sm_set_pool(&smalloc_curr_pool, new_pool, new_pool_size, do_zero);
}

int sm_release_pool(struct smalloc_pool *spool)
{
	return sm_set_pool(spool, NULL, 0, 0);
}

int sm_release_default_pool(void)
{
	return sm_release_pool(&smalloc_curr_pool);
}


/* (elsa) ADDED THIS */
int sm_pools_init(size_t outer_capacity, size_t inner_capacity, size_t pools_size)
{
    if (pool_list.mpools != NULL) {
        // Already initialized
        return 1;
    }

    struct smalloc_mpools *mpools = calloc(outer_capacity, sizeof(struct smalloc_mpools));
    if (mpools == NULL) {
        // TODO error: no more memory
        return 0;
    }
    if (!mpools_initialize(mpools, 0, outer_capacity, inner_capacity, pools_size, &sm_add_pool)) {
        return 0;
    }

    pool_list.mpools = mpools;
    pool_list.capacity = outer_capacity;

    return 1;
}

int64_t sm_add_mpool(const char* name)
{
    uint64_t id;
    if (pool_list.free_ids.sp > 0) {
        id = pool_list.free_ids.stack[--pool_list.free_ids.sp];

        // TODO verify it is valid?
        assert(pool_list.mpools[id].next == 1);
        fprintf(stderr, "Reusing region %ld\n", id);
        return id;
    }
    id = pool_list.free_ids.gen++;
    
    if (id >= pool_list.capacity) {
        size_t new_capacity = 2*pool_list.capacity; // is that too much ? rather add a constant ?
        struct smalloc_mpools *new_mpools = reallocarray(pool_list.mpools, new_capacity, sizeof(struct smalloc_mpools));
        if (new_mpools == NULL) {
            fprintf(stderr, "add-mpool: no more memory (realloc failed)\n");
            return -1;
        }
        size_t m_pool_capacity = new_mpools[0].capacity;
        size_t m_pool_pools_size = new_mpools[0].pools_size; // ??! not ideal
        if (!mpools_initialize(new_mpools, pool_list.capacity, new_capacity, m_pool_capacity, m_pool_pools_size, &sm_add_pool)) {
            return -1;
        }

        pool_list.mpools = new_mpools;
        pool_list.capacity = new_capacity;
    }
    struct smalloc_mpools *m_spool = &(pool_list.mpools[id]);
    if (sm_add_pool(m_spool, 0) == NULL) {
        return -1;
    }
    m_spool->next++;
    if (register_region != NULL) {
       struct smalloc_pool *spool = &(m_spool->pools[m_spool->next-1]);
       register_region(name, id, spool->pool, spool->pool_size);
    } else {
      fprintf(stderr, "Why am I so NULL?\n");
    }
    return id;
}

int sm_release_pools(void)
{
    for (size_t i = 0; i < pool_list.capacity; ++i) {
        /*struct smalloc_mpools *m_spool = &(pool_list.mpools[i]);
        for (size_t j = 0; j < m_spool.capacity; ++j) {
            struct smalloc_pool *pool = &(m_spool.pools[j]);
            if (smalloc_verify_pool(pool) && !sm_release_pool(pool)) {
                return 0;
            }
        }*/ // sometimes it crashes, and all it does is setting the memory to zero...
        free(pool_list.mpools[i].pools);
    }
    free(pool_list.mpools);
    return 1;
}

struct smalloc_pool *sm_add_pool(struct smalloc_mpools *m_spool, size_t n) // default oom handler
{
    if (n > m_spool->pools_size) {
        fprintf(stderr, "add-pool: too big chunk of memory requested\n");
        return NULL;
    }
    if (m_spool->next >= m_spool->capacity) {
        size_t new_capacity = 2*m_spool->capacity; // TODO too much reutilization of code?
        struct smalloc_pool *new_pools = reallocarray(m_spool->pools, new_capacity, sizeof(struct smalloc_pool));
        if (new_pools == NULL) {
            fprintf(stderr, "add-pool: no more memory (realloc failed)\n");
            return NULL;
        }
        m_spool->pools = new_pools;
        m_spool->capacity = new_capacity;
    }
    // Initialize next pool
    struct smalloc_pool *spool = &(m_spool->pools[m_spool->next]);
    void *memptr = mmap(NULL, m_spool->pools_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (!sm_set_pool(spool, memptr, m_spool->pools_size, 0)) {
        fprintf(stderr, "add-pool: invalid memory pointer\n");
        exit(-1);
        return NULL;
    }
    //TODO see if this should be 0
    //register_growth(0, memptr, m_spool->pools_size);
    register_growth(1, memptr, m_spool->pools_size);
    return spool;
}


int mpools_initialize(struct smalloc_mpools *m_spools, size_t start, size_t end,
        size_t m_pool_capacity, size_t m_pool_pools_size, smalloc_oom_handler m_pool_oom_handler)
{
    for (size_t i = start; i < end; ++i) {
        struct smalloc_pool *pools = calloc(m_pool_capacity, sizeof(struct smalloc_pool));
        if (pools == NULL) {
            // Free all
            for (size_t j = 0; j < i; ++j) {
                free(m_spools[j].pools);
            }
            free(m_spools);
            fprintf(stderr, "add-mpool: no more memory (pools allocation failed)\n");
            return 0;
        }
        m_spools[i].pools = pools;
        m_spools[i].capacity = m_pool_capacity;
        m_spools[i].next = 0;
        m_spools[i].oomfn = m_pool_oom_handler;
        m_spools[i].pools_size = m_pool_pools_size;
    }
    return 1;
}



