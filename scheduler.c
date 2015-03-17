#include "scheduler.h"
#include <stddef.h>
#include <stdbool.h>

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
static inline struct scheduler_work *_optimized_insertion_point() {
  return (struct scheduler_work *)(the_scheduler_head.callback);
}

static inline void _set_optimized_insertion_point(struct scheduler_work *unit) {
  the_scheduler_head.callback = (void (*)(struct scheduler_work *))unit;
}

// Overflow-aware time comparison.
static inline _Bool _time_lt_time(unsigned long time1, unsigned long time2) {
  // Visually, where ... is (UINT32_MAX / 2)
  // 0---time2'---time1'---...---time1---time2---UINT32_MAX
  //
  // Comparison      | Result| Math            | Visual math result          | Explanation
  // ----------------+-------+-----------------+-----------------------------+-----------------------------------
  // time1  < time2  | TRUE  | time2  - time1  | ---                         | time1 is < time2
  // time1  < time2' | TRUE  | time2' - time1  | --- + ---                   | subtraction causes major overflow
  // time1' < time2' | FALSE | time2' - time1' | --- + --- + --- + ---...--- | subtraction causes minor underflow
  // time1' < time2  | FALSE | time2  - time1' | ---...--- + ---             | time1' is > time2

  return (time2 - time1) - 1 < (UINT32_MAX / 2);
}

// Import code conditional on SCHEDULER_STRICT
#include "scheduler_strict.h"

//
// Public API
//

void scheduler_init(void) {
  _strict_init();

  // NOTE: Before you try to optimize further, there are general assumptions in
  // add/remove's search algorithms that require head.fire_next to be 0.
  the_scheduler_head.fire_next = 0;
  // Initialize optimized insertion point.
  _set_optimized_insertion_point(&the_scheduler_head);
  // Optimized insertion point defaults to optimizing tail insertions.
  the_scheduler_head.delay_millis = UINT16_MAX;
}

void scheduler_add(struct scheduler_work *unit) {
  _strict_add(unit);

  unit->fire_next = millis() + unit->delay_millis;

  struct scheduler_work *later_unit = _optimized_insertion_point();
  struct scheduler_work *earlier_unit;
  if (later_unit->fire_next > unit->fire_next) {
    later_unit = &the_scheduler_head;
  } else if (the_scheduler_head.delay_millis >= unit->delay_millis) {
    _set_optimized_insertion_point(unit);
  }

  do {
    earlier_unit = later_unit;
    later_unit = later_unit->later;
  } while(later_unit && _time_lt_time(later_unit->fire_next, unit->fire_next));

  earlier_unit->later = unit;
  unit->later = later_unit;
}

void scheduler_remove(struct scheduler_work *unit) {
  _strict_remove(unit);

  struct scheduler_work *earlier_unit = &the_scheduler_head;
  while(earlier_unit != NULL && earlier_unit->later != unit) {
    earlier_unit = earlier_unit->later;
  }

  // earlier_unit is NULL if the unit was not found.
  // That's well-defined as illegal API usage, so we don't check for it.
  earlier_unit->later = unit->later;
  if (_optimized_insertion_point() == unit) {
    _set_optimized_insertion_point(earlier_unit);
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

    // Re-enqueue job if it wasn't removed by its callback
    if (the_scheduler_head.later == current_unit) {
      scheduler_remove(current_unit);
      scheduler_add(current_unit);
    }
  }
}
