#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mh_state.h"
static int64_t gen_id = 0;

mh_state* mhd_state = NULL;

mh_state* mh_new_state(const char* name) {
  mh_state *state = malloc(sizeof(mh_state));
  state->pool_id = gen_id++;
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
