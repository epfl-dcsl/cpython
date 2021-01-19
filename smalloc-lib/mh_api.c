#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mh_state.h"
#include "mh_api.h"

/* This function will allow us to get the current module id or 0, the default.*/
extern int64_t PyObject_Get_Current_ModuleId(void); 

static void mh_grow_mh_heaps();

mh_heaps* all_heaps = NULL;

void mh_heaps_init() {
  all_heaps = malloc(sizeof(mh_heaps));
  all_heaps->gen_id = 0;
  all_heaps->heaps = calloc(MH_HEAPS_INIT_SZ, sizeof(mh_state*));
  all_heaps->curr_size = MH_HEAPS_INIT_SZ;
  mh_state* deflt = mh_new_state("mhdefault");
  all_heaps->heaps[deflt->pool_id] = deflt;
}

mh_state* mh_heaps_get_curr_heap() {
  assert(all_heaps != NULL);
  assert(all_heaps->heaps != NULL);
  int64_t id = PyObject_Get_Current_ModuleId();
  assert(id >= 0);
  assert(id < all_heaps->gen_id);
  return all_heaps->heaps[id];
}

mh_state* mh_heaps_get_heap(int64_t id) {
  assert(all_heaps != NULL);
  assert(all_heaps->heaps != NULL);
  assert(id >= 0);
  assert(id < all_heaps->curr_size);
  return all_heaps->heaps[id];
}


mh_state* mh_new_state(const char* name) {
  mh_state *state = malloc(sizeof(mh_state));
  state->pool_id = all_heaps->gen_id++;
  if (all_heaps->gen_id >= all_heaps->curr_size) {
    mh_grow_mh_heaps();
  }
  int size = 2 * ((NB_SMALL_SIZE_CLASSES + 7) / 8) * 8; 
  for (int i = 0; i < size; i++) {
    if (i % 2 == 0) {
      state->usedpools[i] = MPTA(state->usedpools, (i / 2));
    } else {
      state->usedpools[i+1] = MPTA(state->usedpools, ((i-1) / 2));
    }
  } 
  state->arenas = NULL;
  state->maxarenas = 0;
  state->unused_arena_objects = NULL;
  state->usable_arenas = NULL;
  memset(state->nfp2lasta, 0, sizeof(struct arena_object*) * MAX_POOLS_IN_ARENA+1);
  state->narenas_currently_allocated = 0;
  state->ntimes_arena_allocated = 0;
  state->narenas_highwater = 0;
  return state;
}

/* Helper functions */

/* Allows to grow the heaps. */
static void mh_grow_mh_heaps() {
  all_heaps->heaps = reallocarray(all_heaps->heaps,
      MH_HEAPS_GROWTH_FACTOR * all_heaps->curr_size, sizeof(mh_state*)); 
  all_heaps->curr_size *= MH_HEAPS_GROWTH_FACTOR;
}



