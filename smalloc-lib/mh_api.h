#ifndef _MH_API_H
#define _MH_API_H

#include "mh_state.h"

typedef struct mh_header {
  int64_t pool_id;
} mh_header;

typedef struct mh_heaps {
  mh_state** heaps;
  int64_t gen_id;
  size_t curr_size;
} mh_heaps;

#define MH_HEAPS_INIT_SZ 20
#define MH_HEAPS_GROWTH_FACTOR 2

#define MH_MAGIC (0xdeadbeef)
#define MH_NOT_MAGIC (0xdeadbabe)

#define HEADER_SZ (sizeof(mh_header))

#define VOID_PTR(p) ((void *)p)
#define CHAR_PTR(p) ((char *)p)
#define HEADER_PTR(p) ((mh_header*)p)
#define USER_TO_HEADER(p) (HEADER_PTR((CHAR_PTR(p)-HEADER_SZ)))
#define HEADER_TO_USER(p) (VOID_PTR((CHAR_PTR(p)+HEADER_SZ)))

/* mh_heaps functions */

extern mh_heaps* all_heaps;

void mh_heaps_init();
mh_state* mh_heaps_get_curr_heap();
mh_state* mh_heaps_get_heap(int64_t id);

/* mh_state functions */
mh_state* mh_new_state(const char* name);

#endif
