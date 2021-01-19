#ifndef _MH_API_H
#define _MH_API_H

#include "mh_state.h"

typedef struct mh_header {
  int64_t pool_id;
} mh_header;

#define MH_MAGIC (0xdeadbeef)
#define MH_NOT_MAGIC (0xdeadbabe)

#define HEADER_SZ (sizeof(mh_header))

#define VOID_PTR(p) ((void *)p)
#define CHAR_PTR(p) ((char *)p)
#define HEADER_PTR(p) ((mh_header*)p)
#define USER_TO_HEADER(p) (HEADER_PTR((CHAR_PTR(p)-HEADER_SZ)))
#define HEADER_TO_USER(p) (VOID_PTR((CHAR_PTR(p)+HEADER_SZ)))

extern mh_state* mhd_state;

mh_state* mh_new_state(const char* name);

#endif
