#pragma once

#include <stdint.h>

// Sorry about the member order, required for padding.
struct scheduler_work {
  void (*callback)(struct scheduler_work *); // Callback parameter. Must be non-NULL.
  struct scheduler_work *d5b53ac8ba_private;
  unsigned long a40ad7cc3a_private;
  uint16_t delay_millis;    // Desired callback interval in ms
};

// Call this before using any other part of the scheduler API.
// It is illegal to call this more than once.
void scheduler_init(void);

// Insert a work unit to the schedule.
// Members .callback and .delay_millis must be initialized.
// It is illegal to add the same work unit twice.
void scheduler_add(struct scheduler_work *unit);

// Remove a work unit from the schedule.
// It is illegal to attempt to remove a work unit that is not in the schedule.
void scheduler_remove(struct scheduler_work *unit);

// Turn control over to the scheduler.
// This function will not return until/unless there are no jobs scheduled.
// There's no way to call this wrong.
void scheduler_run(void);
