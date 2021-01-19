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

static mh_stack_ids stack;

mh_heaps* all_heaps = NULL;

void mh_heaps_init() {
  /* Initialize the heaps. */
  all_heaps = malloc(sizeof(mh_heaps));
  all_heaps->gen_id = 0;
  all_heaps->heaps = calloc(MH_HEAPS_INIT_SZ, sizeof(mh_state*));
  all_heaps->curr_size = MH_HEAPS_INIT_SZ;
  int64_t id = mh_new_state("mhdefault");
  assert(id == 0);
  /* Initialize the stack for ids */
  stack.stack = calloc(MH_STACK_INIT_SZ, sizeof(int64_t));
  stack.head = 0;
  stack.size = MH_STACK_INIT_SZ;
}

mh_state* mh_heaps_get_curr_heap() {
  assert(all_heaps != NULL);
  assert(all_heaps->heaps != NULL);
  int64_t id = mh_stack_peek();
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


int64_t mh_new_state(const char* name) {
  mh_state *state = malloc(sizeof(mh_state));
  state->pool_id = all_heaps->gen_id++;
  if (all_heaps->gen_id >= all_heaps->curr_size) {
    mh_grow_mh_heaps();
  }
  all_heaps->heaps[state->pool_id] = state;
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
  return state->pool_id;
}

/* mh_stack functions */
void mh_stack_push(int64_t id) {
  size_t idx = stack.head++;
  if (stack.head >= stack.size) {
    stack.stack = reallocarray(stack.stack,
        MH_STACK_GROWTH_FACTOR * stack.size, sizeof(int64_t)); 
    stack.size = MH_STACK_GROWTH_FACTOR * stack.size;
  }
  stack.stack[idx] = id;
}

/* mh_stack_peek is special, it tries to get the id from the stack.
 * If not possible, it calls PyObject_Get_Current_ModuleId. */
int64_t mh_stack_peek() {
  if (stack.head == 0) {
    return PyObject_Get_Current_ModuleId();
  }
  assert(stack.size > stack.head-1);
  return stack.stack[stack.head-1];
}

int64_t mh_stack_pop() {
  int64_t id = mh_stack_peek(); 
  if (stack.head > 0) {
    stack.head--;
  }
  return id;
}

/* Managing the id of objects */
int64_t mh_get_id(void* ptr) {
  if (ptr == NULL) {
    return 0;
  }
  mh_header* shdr = USER_TO_HEADER(ptr);
  assert(shdr->mh_magic == MH_MAGIC || shdr->mh_magic == MH_NOT_MAGIC);
  assert(shdr->pool_id >= 0);
  assert(shdr->pool_id < all_heaps->gen_id);
  return shdr->pool_id;
}

/* Helper functions */

/* Allows to grow the heaps. */
static void mh_grow_mh_heaps() {
  all_heaps->heaps = reallocarray(all_heaps->heaps,
      MH_HEAPS_GROWTH_FACTOR * all_heaps->curr_size, sizeof(mh_state*)); 
  all_heaps->curr_size *= MH_HEAPS_GROWTH_FACTOR;
}
