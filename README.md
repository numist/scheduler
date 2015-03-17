# Scheduler

A simple scheduler for microcontrollers.

***

## Features/Foibles

Memory used by work units is provided by its clients, so there is no global scheduler table. Using a linked list reduces memory movement caused by insertions/removals from the middle of the schedule. A linked list does require more memory than a slab of work units (the link pointers); to mitigate this slightly, the list is singly-linked.

Callbacks take one parameter: the work unit itself. This allows the work unit structure to save a `void *` for the conventional context parameter. Enclosing the work unit in your own data structure allows you to materialize it with a single pointer subtraction, using the result of `offsetof`.

The schedule is ordered by call schedule, so running a work unit is O(1), and a work unit removing itself from inside its callback is also O(1). The scheduler includes a tail-insertion optimization, so insertions also cost O(1) when all work units have the same time interval. This means that the algorithmic complexity of running work units is constant time in the ideal case. The worst case complexity for insertion and removal is O(n).

## Dependencies

This implementation depends on two symbols in addition to standard C headers:
* `unsigned long millis(void);` returns number of millis elapsed since program start
* `void delay(unsigned long millis);` returns after millis milliseconds have elapsed

This library can be used as-is on the Arduino platform; `millis` and `delay` are part of the standard library.

## Usage

* Make sure `scheduler.c` gets compiled into your project.
* `#include "scheduler.h"` in your code.
* Call `scheduler_init()` before using the scheduler.
* Define a work unit by initializing the `.callback` and `.delay_millis` members of a `struct scheduler_work` variable.
* Add the work unit to the scheduler with `scheduler_add(…)`
* When setup is complete, start the scheduler with `scheduler_run()`

`scheduler_run()` will not return until there are no more work units in the schedule. Work units can unregister themselves using `scheduler_remove(…)`.

If you prefer to see actual code using the library, check out the unit tests.

### Gotchas/Debugging

It is illegal to schedule a work unit twice, or to remove an unscheduled work unit. Checking for these conditions is slow, so the library assumes such situations will never occur and will react in unwanted ways when they do. Defining the preprocessor macro `SCHEDULER_STRICT` enables checks that will trap when illegal conditions are detected, at the expense of performance.

Modifying a work unit while it is in the schedule is also illegal and will result in all kinds of unwanted behaviour, including (but not limited to) call starvation. The same applies to modifying the private members of `struct scheduler_work`. There is no built-in mechanism to help detect this condition.

## Testing

The `scheduler_tests.c` file defines a standalone application that runs the entire unit test suite. It takes no arguments.

The test application is built and run by running `clang -Werror -Wall -Wextra scheduler_tests.c && a.out` from a command line equipped with clang (gcc may be substituted as appropriate).

The test application includes `scheduler.c` with `SCHEDULER_STRICT` defined.
