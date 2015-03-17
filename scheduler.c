#include "scheduler.h"
#include <stddef.h>
#include <stdbool.h>

//
// Compatibility macros
//

// Missing from Arduino toolchain
#ifndef UINT16_MAX
#define UINT16_MAX 0xFFFF
#endif

// Missing from Arduino toolchain
#ifndef UINT32_MAX
#define UINT32_MAX 0xFFFFFFFF
#endif

// Just in case
#ifndef __has_builtin
  #define __has_builtin(x) 0
#endif

//
// Globals
//

// Root of the scheduler's linked list of work units.
//
// This wastes a single unsigned long and a short from the struct.
// Getting around that is not trivial without getters and setters for all
// members with special cases for the head of the scheduler.
// This may waste more CPU time than the memory savings is worth.
static struct scheduler_work the_scheduler_head;

//
// Internal API
//

// De-obfuscate private members of struct scheduler_work:
#define later     d5b53ac8ba_private
#define fire_next a40ad7cc3a_private

// The schedule head's callback parameter allows for an optimization that
// provides tail insertions in O(1).
static inline struct scheduler_work *the_scheduler_last() {
  return (struct scheduler_work *)(the_scheduler_head.callback);
}

static inline void the_scheduler_last_set(struct scheduler_work *unit) {
  the_scheduler_head.callback = (void (*)(struct scheduler_work *))unit;
}

// Overflow-aware time comparison.
static inline _Bool _scheduler_time_is_less_than_time(unsigned long time1, unsigned long time2) {
  // Comparing within an overflow window that is (UINT16_MAX * 2) wide.
  //
  // Visually, where ... is > (UINT32_MAX / 2)
  // 0---time2'---time1'---...---time1---time2---UNSIGNED_LONG_MAX
  //
  // Comparison      | Result| Math            | Visual math result          | Explanation
  // ----------------+-------+-----------------+-----------------------------+-----------------------------------
  // time1  < time2  | TRUE  | time2  - time1  | ---                         | time1 is < time2
  // time1  < time2' | TRUE  | time2' - time1  | --- + ---                   | subtraction causes major overflow
  // time1' < time2' | FALSE | time2' - time1' | --- + --- + --- + ---...--- | subtraction causes minor underflow
  // time1' < time2  | FALSE | time2  - time1' | ---...--- + ---             | time1' is > time2
  
  return (time2 - time1) < (UINT32_MAX / 2);
}

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

static void _strict_check_add(struct scheduler_work *unit) {
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

static void _strict_check_remove(struct scheduler_work *unit) {
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
#endif

//
// Public API
//

void scheduler_init(void) {
#ifdef SCHEDULER_STRICT
  _strict_init();
#endif
  
  the_scheduler_last_set(&the_scheduler_head);
}

#include <stdio.h>
void scheduler_add(struct scheduler_work *unit) {
#ifdef SCHEDULER_STRICT
  _strict_check_add(unit);
#endif

  unit->fire_next = millis() + unit->delay_millis;
  
  struct scheduler_work *later_unit;
  struct scheduler_work *earlier_unit;
  if (the_scheduler_last()->fire_next <= unit->fire_next) {
    later_unit = NULL;
    earlier_unit = the_scheduler_last();
    the_scheduler_last_set(unit);
  } else {
    later_unit = &the_scheduler_head;
    do {
      earlier_unit = later_unit;
      later_unit = later_unit->later;
    } while(later_unit && _scheduler_time_is_less_than_time(later_unit->fire_next, unit->fire_next));
  }
  
  earlier_unit->later = unit;
  unit->later = later_unit;
}

void scheduler_remove(struct scheduler_work *unit) {
#ifdef SCHEDULER_STRICT
  _strict_check_remove(unit);
#endif

  struct scheduler_work *earlier_unit = &the_scheduler_head;
  // This loop is never entered in the common case.
  while(earlier_unit != NULL && earlier_unit->later != unit) {
    earlier_unit = earlier_unit->later;
  }
  
  if (earlier_unit) {
    earlier_unit->later = unit->later;
    
    if (earlier_unit->later == NULL) {
      the_scheduler_last_set(earlier_unit);
    }
  }
}

void scheduler_run(void) {
  while(the_scheduler_head.later != NULL) {
    // Sleep until it's time for the next job.
    unsigned long delay_in_millis = the_scheduler_head.later->fire_next - millis();
    // Watch out for underflow! Delays are limited to uint16_t milliseconds.
    if (delay_in_millis > 0 && delay_in_millis <= UINT16_MAX) {
      delay(delay_in_millis);
    }
    
    struct scheduler_work *current_unit = the_scheduler_head.later;
    current_unit->callback(current_unit);
    
    // Re-enqueue job if it wasn't modified by its callback
    if (the_scheduler_head.later == current_unit) {
      scheduler_remove(current_unit);
      scheduler_add(current_unit);
    }
  }
}
