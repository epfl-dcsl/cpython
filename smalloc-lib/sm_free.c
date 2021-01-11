/*
 * This file is a part of SMalloc.
 * SMalloc is MIT licensed.
 * Copyright (c) 2017 Andrey Rys.
 */

#include "smalloc_i.h"
#include <stdio.h>

void sm_free_pool(struct smalloc_pool *spool, void *p)
{
	struct smalloc_hdr *shdr;
	char *s;

	if (!smalloc_verify_pool(spool)) {
		errno = EINVAL;
		return;
	}

	if (!p) return;

	shdr = USER_TO_HEADER(p);
	if (smalloc_is_alloc(spool, shdr)) {
		if (spool->do_zero) memset(p, 0, shdr->rsz);
		s = CHAR_PTR(p);
		s += shdr->usz;
		memset(s, 0, HEADER_SZ);
		if (spool->do_zero) memset(s+HEADER_SZ, 0, shdr->rsz - shdr->usz);
		memset(shdr, 0, HEADER_SZ);
		return;
	}

	smalloc_UB(spool, p);
	return;
}

void sm_free(void *p)
{
	sm_free_pool(&smalloc_curr_pool, p);
}


/* (elsa) ADDED THIS */
void sm_free_from_pool(int64_t id, void *p)
{
    if (id >= pool_list.capacity || id < 0) {
        fprintf(stderr, "smalloc-free: Received invalid id\n");
        return;
    }
    struct smalloc_mpools m_pool = pool_list.mpools[id];
    // Linear search
    for (size_t i = 0; i < m_pool.next; ++i) {
        struct smalloc_pool *spool = &(m_pool.pools[i]);
        if (p >= spool->pool && p < spool->pool + m_pool.pools_size) {
            sm_free_pool(spool, p);
            if (--spool->num_elems <= 0 && m_pool.next == 1) { // Only one page to simplify
                if (pool_list.free_ids.sp >= 9) { // remove magic number
                    //fprintf(stderr, "too many freed modules; can't keep up\n");
                    // This must be the end of the program, everything will be freed anyway
                    return;
                }
                pool_list.free_ids.stack[pool_list.free_ids.sp++] = id;
            }

            return;
        }
    }
    fprintf(stderr, "smalloc-free: No corresponding pool was found!! %ld \n", id);
}

