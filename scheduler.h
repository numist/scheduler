#pragma once

#include <stdint.h>

struct scheduler_work {
  // Sorry about the member order, required for padding.
  void (*callback)(struct scheduler_work *); // Parameter. Must be non-NULL
  struct scheduler_work *d5b53ac8ba_private;
  unsigned long a40ad7cc3a_private;
  uint16_t delay_millis;    // Parameter. Callback interval in ms
};

// Call this once before using the rest of the API.
// Calling this more than once will cause unwanted behaviour.
void scheduler_init(void);

// Insert a work unit to the schedule.
// Parameters .callback and .delay_millis must be initialized to sane values.
// Other members can be safely left uninitialized.
// If the work unit is already scheduled, unwanted behaviour will result.
void scheduler_add(struct scheduler_work *unit);

// Remove a work unit from the schedule.
// If the work unit is not in the schedule, the call will trap.
void scheduler_remove(struct scheduler_work *unit);

// Turn control over to the scheduler.
// This function will not return until/unless there are no jobs scheduled.
void scheduler_run(void);
