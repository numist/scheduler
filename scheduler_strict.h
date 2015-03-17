#pragma once

#include "scheduler_internal.h"

#ifdef SCHEDULER_STRICT
# if !__has_builtin(__builtin_trap)
#  error Building with SCHEDULER_STRICT requires __builtin_trap
# endif

static _Bool initialized = false;

static void _strict_init() {
  if (initialized) {
    // Calling init multiple times is a programmer error.
    __builtin_trap();
  }
  initialized = true;
}

static void _strict_add(struct scheduler_work *unit) {
  if (!initialized) {
    // Calling API before initialization is programmer error.
    __builtin_trap();
  }
  
  if (!unit->callback) {
    // The work unit is invalid (NULL callback).
    __builtin_trap();
  }
  
  struct scheduler_work *search = &the_scheduler_head;
  do {
    if (search == unit) {
      // The work unit is already in the schedule.
      __builtin_trap();
    }
    search = search->later;
  } while(search);
}

static void _strict_remove(struct scheduler_work *unit) {
  if (!initialized) {
    // Calling API before initialization is programmer error.
    __builtin_trap();
  }
  
  struct scheduler_work *earlier_unit = &the_scheduler_head;
  // This loop is never entered in the common case.
  while(earlier_unit != NULL && earlier_unit->later != unit) {
    earlier_unit = earlier_unit->later;
  }
  if (!earlier_unit) {
    // The work unit was not in the schedule. This is a programmer error.
    __builtin_trap();
  }
}

#else

#define _strict_init()
#define _strict_add(unit)
#define _strict_remove(unit)

#endif
