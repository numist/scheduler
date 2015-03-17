#include "scheduler.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

// Build and run tests with: clang -Werror -Wall -g scheduler_tests.c && a.out

// Turn on extra checks (slower, scheduler traps on programmer error)
#define SCHEDULER_STRICT

// TODO: Turn on instrumentation for profiling optimizations
#define SCHEDULER_PROFILE

//
// Test harness conveniences
//

static int failures = 0;
static int tests = 0;

#define test(cond) do { \
  tests++; \
  if(!(cond)) { \
    printf("F"); \
    failures++; \
  } else { \
    printf("."); \
  } \
} while(0)

static uint32_t the_time_in_millis;
static uint32_t calls_to_delay;

struct _test_unit {
  struct scheduler_work unit;
  unsigned calls;
  unsigned limit;
};
static void _test_unit_callback(struct scheduler_work *unit) {
  struct _test_unit *test_unit = (struct _test_unit *)((uint8_t *)(unit) - offsetof(struct _test_unit, unit));
  test_unit->calls++;
  
  if (unit->delay_millis > 0) {
    test(the_time_in_millis % unit->delay_millis == 0);
  } else if (test_unit->calls % 5 == 0) {
    // Calls with 0 delay must take some amount of time!
    the_time_in_millis++;
  }
  
  if (test_unit->limit > 0) {
    test(the_time_in_millis <= unit->delay_millis * test_unit->limit);
    if (test_unit->calls >= test_unit->limit) {
      scheduler_remove(unit);
    }
  }
}
static void _test_unit_callback_never(struct scheduler_work *unit __attribute__((unused))) {
  test(false);
}

// TODO: test_before() should reset optimized_insert and anything else that can change

// ///////////////////////////////////////////////////////////////////////// //
// Black box tests
// ///////////////////////////////////////////////////////////////////////// //

//
// Ensure scheduler_run returns immediately when the schedule is empty
//

static void test_scheduler_empty(void) {
  the_time_in_millis = 0;
  calls_to_delay = 0;
  scheduler_run();
  test(the_time_in_millis == 0);
  test(calls_to_delay == 0);
}

//
// Test that the max allowed interval size works correctly
//

static void test_scheduler_max_interval(void) {
  the_time_in_millis = 0;

  struct _test_unit test;
  test.unit.callback = _test_unit_callback;
  test.unit.delay_millis = UINT16_MAX;
  test.calls = 0;
  test.limit = 2;

  scheduler_add(&test.unit);
  scheduler_run();
  // Check that the callback happened only twice.
  test(test.calls == test.limit);
  // Check that we did actually overflow correctly.
  test(the_time_in_millis != 0);
  test(the_time_in_millis < UINT32_MAX / 2);
}

//
// Test that the scheduler works properly when millis() overflows
//

static void test_scheduler_overflow(void) {
  struct _test_unit test;
  const unsigned delay = 50;
  the_time_in_millis = -delay;
  test.unit.callback = _test_unit_callback;
  test.unit.delay_millis = delay;
  test.limit = 2;
  test.calls = 0;
  scheduler_add(&test.unit);
  scheduler_run();
  // Check that the callback happened only twice.
  test(test.calls == test.limit);
  // Check that we did actually overflow correctly.
  test(the_time_in_millis == delay);
}

//
// Test classic fizzbuzz, in scheduler form:
// job 1 every 3ms, job 2 every 5ms
//
// 3 5 6 9 10 12 15
// 1 2 1 1  2  1 1&2
//
static void test_scheduler_fb(void) {
  the_time_in_millis = 0;
  
  struct _test_unit fizz;
  fizz.unit.callback = _test_unit_callback;
  fizz.unit.delay_millis = 3;
  fizz.limit = 10;
  fizz.calls = 0;
  scheduler_add(&fizz.unit);

  struct _test_unit buzz;
  buzz.unit.callback = _test_unit_callback;
  buzz.unit.delay_millis = 5;
  buzz.limit = 6;
  buzz.calls = 0;
  scheduler_add(&buzz.unit);

  scheduler_run();
  
  test(fizz.calls == fizz.limit);
  test(buzz.calls == buzz.limit);
  test(the_time_in_millis == 30);
}

//
// Test two jobs that hvae the same delay.
//

static void test_scheduler_ff(void) {
  the_time_in_millis = 0;
  
  struct _test_unit fizz;
  fizz.unit.callback = _test_unit_callback;
  fizz.unit.delay_millis = 3;
  fizz.limit = 5;
  fizz.calls = 0;
  scheduler_add(&fizz.unit);

  struct _test_unit fuzz;
  fuzz.unit.callback = _test_unit_callback;
  fuzz.unit.delay_millis = 3;
  fuzz.limit = 5;
  fuzz.calls = 0;
  scheduler_add(&fuzz.unit);

  scheduler_run();
  
  test(fizz.calls == fizz.limit);
  test(fuzz.calls == fuzz.limit);
  test(the_time_in_millis == 15);
}

//
// Test that one work unit with a delay of 0 does not starve out other work units
//

static struct _test_unit _test_scheduler_starve_fuzz;
static void _test_scheduler_starve_callback(struct scheduler_work *unit) {
  if (_test_scheduler_starve_fuzz.calls == _test_scheduler_starve_fuzz.limit) {
    scheduler_remove(unit);
  } else {
    _test_unit_callback(unit);
  }
}
static void test_scheduler_starve(void) {
  the_time_in_millis = 0;
  
  struct _test_unit fizz;
  fizz.unit.callback = _test_scheduler_starve_callback;
  fizz.unit.delay_millis = 0;
  fizz.limit = 0;
  fizz.calls = 0;
  scheduler_add(&fizz.unit);

  _test_scheduler_starve_fuzz.unit.callback = _test_unit_callback;
  _test_scheduler_starve_fuzz.unit.delay_millis = 3;
  _test_scheduler_starve_fuzz.limit = 5;
  _test_scheduler_starve_fuzz.calls = 0;
  scheduler_add(&_test_scheduler_starve_fuzz.unit);

  scheduler_run();
  
  test(fizz.calls == 75);
  test(_test_scheduler_starve_fuzz.limit == _test_scheduler_starve_fuzz.calls);
  test(the_time_in_millis == 15);
}


//
// Test that removing the last element of a two-job schedule works
//

static void test_scheduler_remove_last_of_2(void) {
  the_time_in_millis = 0;
  
  struct _test_unit fizz;
  fizz.unit.callback = _test_unit_callback;
  fizz.unit.delay_millis = 3;
  fizz.limit = 5;
  fizz.calls = 0;
  scheduler_add(&fizz.unit);

  struct _test_unit buzz;
  buzz.unit.callback = _test_unit_callback_never;
  buzz.unit.delay_millis = 5;
  scheduler_add(&buzz.unit);

  scheduler_remove(&buzz.unit);

  scheduler_run();
  
  test(fizz.calls == fizz.limit);
  test(the_time_in_millis == 15);
}

//
// Test removing the last element of a three-job schedule
//

static void test_scheduler_remove_last_of_3(void) {
  struct _test_unit test_units[3];
  the_time_in_millis = 0;
  
  test_units[0].unit.callback = _test_unit_callback;
  test_units[0].unit.delay_millis = 1;
  test_units[0].limit = 3 - 0;
  test_units[0].calls = 0;
  scheduler_add(&test_units[0].unit);
  
  test_units[1].unit.callback = _test_unit_callback;
  test_units[1].unit.delay_millis = 2;
  test_units[1].limit = 3 - 1;
  test_units[1].calls = 0;
  scheduler_add(&test_units[1].unit);
  
  test_units[2].unit.callback = _test_unit_callback_never;
  test_units[2].unit.delay_millis = 3;
  scheduler_add(&test_units[2].unit);
  
  scheduler_remove(&test_units[2].unit);
  
  scheduler_run();
  
  test(test_units[0].calls == test_units[0].limit);
  test(test_units[1].calls == test_units[1].limit);
  test(the_time_in_millis == 4);
}

//
// Test mid-insertion
//

static void test_scheduler_insert_mid(void) {
  struct _test_unit test_units[3];
  the_time_in_millis = 0;
  
  test_units[0].unit.callback = _test_unit_callback;
  test_units[0].unit.delay_millis = 1;
  test_units[0].limit = 10;
  test_units[0].calls = 0;
  scheduler_add(&test_units[0].unit);
  
  test_units[1].unit.callback = _test_unit_callback;
  test_units[1].unit.delay_millis = 4;
  test_units[1].limit = 10;
  test_units[1].calls = 0;
  scheduler_add(&test_units[1].unit);
  
  test_units[2].unit.callback = _test_unit_callback;
  test_units[2].unit.delay_millis = 9;
  test_units[2].limit = 10;
  test_units[2].calls = 0;
  scheduler_add(&test_units[2].unit);
      
  scheduler_run();
  
  test(test_units[0].calls == test_units[0].limit);
  test(test_units[1].calls == test_units[1].limit);
  test(test_units[2].calls == test_units[2].limit);
  test(the_time_in_millis == (10 * 9));
}

//
// Test mid-deletion
//

static void test_scheduler_remove_mid(void) {
  struct _test_unit test_units[3];
  the_time_in_millis = 0;
  
  test_units[0].unit.callback = _test_unit_callback;
  test_units[0].unit.delay_millis = 1;
  test_units[0].limit = 5;
  test_units[0].calls = 0;
  scheduler_add(&test_units[0].unit);
  
  test_units[1].unit.callback = _test_unit_callback_never;
  test_units[1].unit.delay_millis = 2;
  scheduler_add(&test_units[1].unit);
  
  test_units[2].unit.callback = _test_unit_callback;
  test_units[2].unit.delay_millis = 3;
  test_units[2].limit = 5;
  test_units[2].calls = 0;
  scheduler_add(&test_units[2].unit);
  
  scheduler_remove(&test_units[1].unit);
  
  scheduler_run();
  
  test(test_units[0].calls == test_units[0].limit);
  test(test_units[2].calls == test_units[2].limit);
  test(the_time_in_millis == 15);
}

// ///////////////////////////////////////////////////////////////////////// //
//
// ///////////////////////////////////////////////////////////////////////// //


//
// Symbol dependencies of code under test
//

static void delay(unsigned long millis) {
  test(millis != 0);
  test(millis <= UINT16_MAX);
  // printf("[%lu]", millis);
  calls_to_delay++;
  the_time_in_millis += millis;
}

static unsigned long millis(void) {
  return the_time_in_millis;
}

//
// Code under test
//

#include "scheduler.c"

// ///////////////////////////////////////////////////////////////////////// //
// White box tests
// ///////////////////////////////////////////////////////////////////////// //

static void test_time_lt_time(void) {
  test(!_time_lt_time(0, 0));
  test(_time_lt_time(0, 20));
  test(!_time_lt_time(20, 0));
  test(_time_lt_time((unsigned long)(-10), 10));
  test(!_time_lt_time(10, (unsigned long)(-10)));
}

//
// TODO: test that 3 work units with the same interval always tail-insert
//

static void _test_scheduler_tail_insertion(void) {
  const int test_count = 3;
  struct _test_unit test_units[test_count];
  the_time_in_millis = 0;

  for (int i = 0; i < test_count; i++) {
    test_units[i].unit.callback = _test_unit_callback;
    test_units[i].unit.delay_millis = 50;
    test_units[i].limit = 10;
    test_units[i].calls = 0;
    scheduler_add(&test_units[i].unit);
  }
      
  scheduler_run();
  
  for (int i = 0; i < test_count; i++) {
    test(test_units[i].calls == test_units[i].limit);
  }
  test(the_time_in_millis == (10 * 50));
}
static void test_scheduler_tail_insertion(void) {
  // TODO: Reset profiler
  _test_scheduler_tail_insertion();
  // TODO: Verify profiler never took slow path
}

//
// TODO: test that head.delay_millis results in optimized insertions with N 20ms work units and a single 1000ms work unit.
//

static void _test_scheduler_optimized_insertions(void) {
  const int test_count = 5;
  struct _test_unit test_units[test_count];
  the_time_in_millis = 0;

  for (int i = 0; i < test_count; i++) {
    test_units[i].unit.callback = _test_unit_callback;
    test_units[i].unit.delay_millis = 50;
    test_units[i].limit = 10;
    test_units[i].calls = 0;
  }
  
  test_units[test_count - 1].unit.delay_millis = 550;
  test_units[test_count - 1].limit = 1;

  for (int i = 0; i < test_count; i++) {
    scheduler_add(&test_units[i].unit);
  }

  scheduler_run();
  
  for (int i = 0; i < test_count; i++) {
    test(test_units[i].calls == test_units[i].limit);
  }
  test(the_time_in_millis == 550);
}
static void test_scheduler_optimized_insertions(void) {
  // TODO: reset profiler
  _test_scheduler_optimized_insertions();
  // TODO: Verify profiler took slow path
  
  // TODO: reset profiler
  // TODO: calibrate optimized_insertions to 50ms
  _test_scheduler_optimized_insertions();
  // TODO: Verify profiler never took slow path
}

// ///////////////////////////////////////////////////////////////////////// //
//
// ///////////////////////////////////////////////////////////////////////// //

//
// Test runner
//

int main() {
  scheduler_init();
  
  // Black box
  test_scheduler_empty();
  test_scheduler_max_interval();
  test_scheduler_overflow();
  test_scheduler_fb();
  test_scheduler_ff();
  test_scheduler_starve();
  test_scheduler_remove_last_of_2();
  test_scheduler_remove_last_of_3();
  test_scheduler_insert_mid();
  test_scheduler_remove_mid();

  // White box
  test_time_lt_time();
  test_scheduler_tail_insertion();
  test_scheduler_optimized_insertions();
  
  printf("\n%d failures in %d checks\n", failures, tests);
  return 0;
}
