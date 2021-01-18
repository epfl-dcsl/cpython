
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h> 

#include "multiheap.h"
#include "smalloc_i.h"

/* The global allocator. */
struct mh_allocator mhallocator;

/* The hooks for litterbox. */
void (*register_id)(const char*, int) = NULL;
void (*register_growth)(int, void*, size_t) = NULL;

static void check_null(void* ptr) {
  if (ptr == NULL) {
    fprintf(stderr, "ptr is NULL\n");
    /* get a stack trace. */
	  char *c = NULL;
	  *c = 'X';
  }
}

static void check(int val, const char* msg) {
  if (val == 0) {
    fprintf(stderr, msg);
    /* get a stack trace. */
	  char *c = NULL;
	  *c = 'X';
  }
}

void mh_init_allocator() {
  mhallocator.next_id = 0;
  mhallocator.mhsize = MH_INITIAL_MHEAPS_NB;
  mhallocator.mheaps = calloc(mhallocator.mhsize, sizeof(struct mh_heap*));
  check_null(mhallocator.mheaps); 

  /* Create the default pool.*/
  int64_t id = mh_new_id("mhdefault");
  if (id != 0) {
    fprintf(stderr, "Unable to get 0 as the default id.\n");
    exit(33);
  }
}

// Optimize the way in which we go through the list,
// Add the number of allocated bytes as an attribute of the pool.
// Try to boost the speed of that shit.
//#define REAL_ALLOC 1

#ifdef REAL_ALLOC
int64_t mh_new_id(const char* name) {
  //Do nothing
  return 0;
} 

void *mh_malloc(int64_t id, size_t size) {
  if (size == 0)
    size = 1;
  return malloc(size);
}

void *mh_calloc(int64_t id, size_t nmemb, size_t size) {
  if (nmemb == 0 || size == 0) {
    nmemb = 1;
    size = 1;
  }
  return calloc(nmemb, size);
}

void *mh_realloc(void* ptr, size_t size) {
  if (size == 0) 
    size = 1;
  return realloc(ptr, size);
}

void mh_free(void* ptr) {
  free(ptr);
}

int64_t mh_get_id(void* ptr) {
  return 0;
}
  
#else
int64_t mh_new_id(const char* name) {
  int64_t id = mhallocator.next_id++;
  /* we ran out of space in the mheaps. */
  if (mhallocator.mhsize <= mhallocator.next_id) {
    mhallocator.mhsize *= 2;
    mhallocator.mheaps = reallocarray(mhallocator.mheaps, mhallocator.mhsize, sizeof(struct mh_heap*));
    check_null(mhallocator.mheaps); 
  }

  mhallocator.mheaps[id] = malloc(sizeof(mh_heap));
  check_null(mhallocator.mheaps[id]);
  mh_heap_init(id, mhallocator.mheaps[id]);
  if (register_id != NULL) {
    register_id(name, id);
  }
  return id;
}

void *mh_malloc(int64_t id, size_t size) {
  if (id < 0 || id >= mhallocator.mhsize) {
    fprintf(stderr, "Asking for malloc of id %ld, max is %d\n", id, mhallocator.mhsize-1);
    exit(33);
  } 
  struct mh_heap* heap = mhallocator.mheaps[id];
  return mh_heap_malloc(heap, size);
}

void *mh_calloc(int64_t id, size_t nmemb, size_t size) {
  if (id < 0 || id >= mhallocator.mhsize) {
    fprintf(stderr, "Asking for calloc of id %ld, max is %d\n", id, mhallocator.mhsize-1);
    exit(33);
  }
  struct mh_heap* heap = mhallocator.mheaps[id];
  if (nmemb == 0 || size == 0) {
    nmemb = 1;
    size = 1;
  }
  size_t nsize = nmemb * size; 
  return mh_heap_malloc(heap, nsize);
}

void *mh_realloc(void *ptr, size_t size) {
  int64_t id = mh_get_id(ptr); 
  struct mh_heap* heap = mhallocator.mheaps[id];
  void *real = mh_heap_malloc(heap, size); 
  if (ptr == NULL) {
    return real;
  }
  struct smalloc_hdr* shdr = USER_TO_HEADER(ptr);
  size_t min = (shdr->usz < size)? shdr->usz : size;
  memcpy(real, ptr, min);
  mh_free(ptr);
  ptr = NULL;
  return real;
}

void mh_free(void* ptr) {
  check(ptr != NULL, "calling free with null pointer.\n");
  struct smalloc_hdr *shdr = USER_TO_HEADER(ptr); 
  int64_t id = HDR_GET_ID(shdr);
  check(id >= 0 && id < mhallocator.mhsize, "invalid id");
  mh_heap* heap = mhallocator.mheaps[id];
  check_null(heap);
  mh_heap_free(heap, ptr);
}

int64_t mh_get_id(void* ptr) {
  check(ptr != NULL, "getting id of null\n");
  struct smalloc_hdr *shdr = USER_TO_HEADER(ptr);
  check(smalloc_check_magic(shdr), "magic header is not correct\n");
  return HDR_GET_ID(shdr);
}

#endif //REAL_ALLOC

/* All the functions that relate to mh_heap. */

void mh_heap_init(int64_t id, struct mh_heap* heap) {
  check_null(heap); 
  heap->pool_id = id;
  heap->lhead = NULL;
  heap->ltail = NULL;
  heap->lsize = 0;
}

void *mh_heap_malloc(mh_heap* heap, size_t size) {
  check_null(heap);
  for (mh_arena* arena = heap->lhead; arena != NULL; arena = arena->next) {
    uint64_t free = arena->pool.pool_size - arena->used_bytes; 
    /* Not enough space left here, it's stupid to even look. */
    if (free < (size + HEADER_SZ)) {
      continue;
    }
    void* ptr = sm_malloc_pool(heap->pool_id, &arena->pool, size);
    if (ptr != NULL) {
      struct smalloc_hdr* shdr = USER_TO_HEADER(ptr);
      shdr->arena = (void*)(arena);
      arena->num_elem++;
      arena->used_bytes += shdr->rsz; 
      return ptr;
    }
  }

  /* we failed to allocate, create a new pool. */
  /* account for the header if this is a large alloc. */
  size_t s = size + HEADER_SZ;
  size_t s_aligned = ((s / MH_DEFAULT_POOL_SIZE) + 1) * MH_DEFAULT_POOL_SIZE;
  size_t min_pool_sz = (s < MH_DEFAULT_POOL_SIZE)? MH_DEFAULT_POOL_SIZE : s_aligned; 
  
  mh_new_arena(heap, min_pool_sz);
  /* Simply retry the allocation in a recursive fashion. */ 
  return mh_heap_malloc(heap, size);
}

void mh_heap_free(mh_heap* heap, void* ptr) {
  check_null(heap);
  check_null(ptr);
  struct smalloc_hdr *shdr = USER_TO_HEADER(ptr);
  check(smalloc_check_magic(shdr), "Error not magic\n"); 
  mh_arena* arena = AS_ARENA(shdr->arena); 
  check(smalloc_is_alloc(&arena->pool, shdr), "It is not an alloc\n");
  size_t size = shdr->rsz;
  sm_free_pool(&arena->pool, ptr);
  arena->num_elem--;
  arena->used_bytes -= ((uint64_t) size);
  if (arena->num_elem == 0) {
    check(arena->used_bytes == 0, "No elem, but used bytes not 0\n");
    mh_heap_mv_to_head(heap, arena);
  }
}

void mh_heap_mv_to_head(mh_heap* heap, mh_arena* arena) {
  // Nothing to do.
  if (heap->lhead == arena) {
    return;
  }

  mh_arena* prev = arena->prev;
  mh_arena* next = arena->next;
  if (prev != NULL) {
    prev->next = next;
  }
  if (next != NULL) {
    prev->prev = NULL;
  }
  if (heap->ltail == arena) {
    if (next != NULL) {
      heap->ltail = next;
    } else if (prev != NULL) {
      heap->ltail = prev;
    } else {
      fprintf(stderr, "There is someting really wrong\n");
      exit(33);
    }
  }
}

/* All the functions that relate to mh_arena */

mh_arena* mh_new_arena(mh_heap* parent, size_t pool_size) {
  check(pool_size >= MH_DEFAULT_POOL_SIZE, "wrong size arena");
  check(pool_size % MH_DEFAULT_POOL_SIZE == 0, "wrong size arena");
  check_null(parent);
  mh_arena* arena = malloc(sizeof(mh_arena));
  check_null(arena);
  arena->parent = parent;
  arena->prev = NULL;
  arena->next = NULL;
  arena->num_elem = 0;
  arena->used_bytes = 0;
  arena->pool.pool_size = pool_size;
  arena->pool.pool = mmap(NULL, pool_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0); 
  check_null(arena->pool.pool);
  arena->pool.do_zero = 1;
  arena->pool.oomfn = NULL;

  /* add the arena to the parent. */
  if (parent->lhead == NULL && parent->ltail == NULL) {
    parent->lhead = arena;
    parent->ltail = arena;
  } else {
    // Cheat, put it in front
    mh_arena* head = parent->lhead;
    parent->lhead = arena;
    arena->next = head;
    head->prev = arena;
  }
  parent->lsize++;

  /* registering the arena */
  if (register_growth != NULL) {
    register_growth(parent->pool_id, arena->pool.pool, pool_size);
  }
  return arena;
}
