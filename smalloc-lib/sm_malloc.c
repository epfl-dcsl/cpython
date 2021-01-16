/*
 * This file is a part of SMalloc.
 * SMalloc is MIT licensed.
 * Copyright (c) 2017 Andrey Rys.
 */

#include <stdio.h>
#include <stdlib.h>
#include "smalloc_i.h"

void *sm_malloc_mpool(struct smalloc_mpools *m_spool, size_t n, int64_t id)
{
  struct smalloc_pool *spool;
	struct smalloc_hdr *basehdr, *shdr, *dhdr;
	char *s;
	int found;
	size_t x;

  spool = &(m_spool->pools[m_spool->next-1]);

again:	if (!smalloc_verify_pool(spool)) {
		errno = EINVAL;
		return NULL;
	}

	if (n == 0) n++; /* return a block successfully */
	if (n > SIZE_MAX
	|| n > (spool->pool_size - HEADER_SZ)) goto oom;

	shdr = basehdr = spool->pool;
	while (CHAR_PTR(shdr)-CHAR_PTR(basehdr) < spool->pool_size) {
		/*
		 * Already allocated block.
		 * Skip it by jumping over it.
		 */
		if (smalloc_is_alloc(spool, shdr)) {
			s = CHAR_PTR(HEADER_TO_USER(shdr));
			s += shdr->rsz + HEADER_SZ;
			shdr = HEADER_PTR(s);
			continue;
		}
		/*
		 * Free blocks ahead!
		 * Do a second search over them to find out if they're
		 * really large enough to fit the new allocation.
		 */
		else {
			dhdr = shdr; found = 0;
			while (CHAR_PTR(dhdr)-CHAR_PTR(basehdr) < spool->pool_size) {
				/* pre calculate free block size */
				x = CHAR_PTR(dhdr)-CHAR_PTR(shdr);
				/*
				 * ugh, found next allocated block.
				 * skip this candidate then.
				 */
				if (smalloc_is_alloc(spool, dhdr))
					goto allocblock;
				/*
				 * did not see allocated block yet,
				 * but this free block is of enough size
				 * - finally, use it.
				 */
				if (n + HEADER_SZ <= x) {
					x -= HEADER_SZ;
					found = 1;
					goto outfound;
				}
				dhdr++;
			}

outfound:		if (found) {
				uintptr_t tag;
				/* allocate and return this block */
        shdr->magic = MY_MAGIC;
        shdr->pool_id = id;
				shdr->rsz = x;
				shdr->usz = n;
				shdr->tag = tag = smalloc_mktag(shdr);
				if (spool->do_zero) memset(HEADER_TO_USER(shdr), 0, shdr->rsz);
				s = CHAR_PTR(HEADER_TO_USER(shdr));
				s += shdr->usz;
				for (x = 0;
				x < sizeof(struct smalloc_hdr);
				x += sizeof(uintptr_t)) {
					tag = smalloc_uinthash(tag);
					memcpy(s+x, &tag, sizeof(uintptr_t));
				}
				memset(s+x, 0xff, shdr->rsz - shdr->usz);
                spool->num_elems++; // (elsa) ADDED THIS
				return HEADER_TO_USER(shdr);
			}

			/* continue first search for next free block */
allocblock:		shdr = dhdr;
			continue;
		}

		shdr++;
	}

oom:	if (m_spool->oomfn) {
		spool = m_spool->oomfn(m_spool, n);
		if (spool != NULL) {
			m_spool->next++;
			if (sm_align_pool(spool)) goto again;
		}
	}

	errno = ENOMEM;
	return NULL;
}

/* (elsa) ADDED THIS */
void *sm_malloc_from_pool(int64_t id, size_t n)
{
    if (id >= pool_list.capacity) {
        fprintf(stderr, "Capacity of %ld exceeded (required %ld)\n", pool_list.capacity, id);
        exit(33);
    }
    return sm_malloc_mpool(&(pool_list.mpools[id]), n, id);
}

int64_t sm_get_object_id(void* p)
{
    struct smalloc_mpools m_pool;
    struct smalloc_hdr *shdr = USER_TO_HEADER(p);
    int64_t id = shdr->pool_id;
    if (id >= pool_list.capacity || id < 0) {
      return -1;
    }

    if (shdr->magic != MY_MAGIC) {
      return -1;
    }

    m_pool = pool_list.mpools[id];
    // Linear search
    for (size_t i = 0; i < m_pool.next; ++i) {
        struct smalloc_pool *spool = &(m_pool.pools[i]);
        if (p >= spool->pool && p < spool->pool + m_pool.pools_size) {
            if (!smalloc_is_alloc(spool, shdr)) {
              return -1; 
            }
            return id;
        }
    }
    //TODO(aghosn) we have issues here
    fprintf(stderr, "Failure after the loop %ld, %d\n", id, shdr->magic == MY_MAGIC);
    exit(666); 
    return -2;
}

