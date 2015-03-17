#include "scheduler.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

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

static int _test_scheduler_max_interval_calls = 0;
static void _test_scheduler_max_interval_callback(struct scheduler_work *unit) {
  _test_scheduler_max_interval_calls++;
  if (_test_scheduler_max_interval_calls == 2) {
    scheduler_remove(unit);
  }
}
static void test_scheduler_max_interval(void) {
  the_time_in_millis = 0;

  struct scheduler_work unit;
  bzero(&unit, sizeof(struct scheduler_work));
  unit.callback = _test_scheduler_max_interval_callback;
  unit.delay_millis = UINT16_MAX;
  scheduler_add(&unit);
  scheduler_run();
  // Check that the callback happened only twice.
  test(_test_scheduler_max_interval_calls == 2);
  // Check that we did actually overflow correctly.
  test(the_time_in_millis != 0);
}

//
// Test that the scheduler works properly when millis() overflows
//

static int _test_scheduler_overflow_calls = 0;
static void _test_scheduler_overflow_callback(struct scheduler_work *unit) {
  _test_scheduler_overflow_calls++;
  if (_test_scheduler_overflow_calls == 2) {
    scheduler_remove(unit);
  }
}
static void test_scheduler_overflow(void) {
  struct scheduler_work unit;
  const unsigned delay = 50;
  the_time_in_millis = -delay;
  unit.callback = _test_scheduler_overflow_callback;
  unit.delay_millis = delay;
  scheduler_add(&unit);
  scheduler_run();
  // Check that the callback happened only twice.
  test(_test_scheduler_overflow_calls == 2);
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
static int _test_scheduler_fb_fizz_calls = 0;
static int _test_scheduler_fb_buzz_calls = 0;
static void _test_scheduler_fb_fizz_callback(struct scheduler_work *unit) {
  _test_scheduler_fb_fizz_calls++;
  test(the_time_in_millis % 3 == 0);
  if (the_time_in_millis == 15) {
    test(_test_scheduler_fb_fizz_calls == 5);
    scheduler_remove(unit);
  }
  test(the_time_in_millis <= 15);
}
static void _test_scheduler_fb_buzz_callback(struct scheduler_work *unit) {
  _test_scheduler_fb_buzz_calls++;
  test(the_time_in_millis % 5 == 0);
  if (the_time_in_millis == 15) {
    test(_test_scheduler_fb_buzz_calls == 3);
  }
  if (the_time_in_millis == 30) {
    test(_test_scheduler_fb_buzz_calls == 6);
    scheduler_remove(unit);
  }
  test(the_time_in_millis <= 30);
}
static void test_scheduler_fb(void) {
  the_time_in_millis = 0;
  
  struct scheduler_work fizz;
  fizz.callback = _test_scheduler_fb_fizz_callback;
  fizz.delay_millis = 3;
  scheduler_add(&fizz);

  struct scheduler_work buzz;
  buzz.callback = _test_scheduler_fb_buzz_callback;
  buzz.delay_millis = 5;
  scheduler_add(&buzz);

  scheduler_run();
  
  test(the_time_in_millis == 30);
}

//
// Test two jobs that hvae the same delay.
//

static int _test_scheduler_ff_fizz_calls = 0;
static int _test_scheduler_ff_fuzz_calls = 0;
static void _test_scheduler_ff_fizz_callback(struct scheduler_work *unit) {
  _test_scheduler_ff_fizz_calls++;
  test(the_time_in_millis % 3 == 0);
  if (the_time_in_millis == 15) {
    test(_test_scheduler_ff_fizz_calls == 5);
    scheduler_remove(unit);
  }
  test(the_time_in_millis <= 15);
}
static void _test_scheduler_ff_fuzz_callback(struct scheduler_work *unit) {
  _test_scheduler_ff_fuzz_calls++;
  test(the_time_in_millis % 3 == 0);
  if (the_time_in_millis == 15) {
    test(_test_scheduler_ff_fuzz_calls == 5);
    scheduler_remove(unit);
  }
  test(the_time_in_millis <= 15);
}
static void test_scheduler_ff(void) {
  the_time_in_millis = 0;
  
  struct scheduler_work fizz;
  fizz.callback = _test_scheduler_ff_fizz_callback;
  fizz.delay_millis = 3;
  scheduler_add(&fizz);

  struct scheduler_work fuzz;
  fuzz.callback = _test_scheduler_ff_fuzz_callback;
  fuzz.delay_millis = 3;
  scheduler_add(&fuzz);

  scheduler_run();
  
  test(the_time_in_millis == 15);
}

//
// Test that one work unit with a delay of 0 does not starve out other work units
//

static int _test_scheduler_starve_fizz_calls = 0;
static int _test_scheduler_starve_fuzz_calls = 0;
static void _test_scheduler_starve_fizz_callback(struct scheduler_work *unit) {
  _test_scheduler_starve_fizz_calls++;
  
  // Callbacks will always take time, this one takes 500ns
  the_time_in_millis = _test_scheduler_starve_fizz_calls / 5;
  
  if (the_time_in_millis == 15) {
    // test(_test_scheduler_starve_fizz_calls == 15);
    scheduler_remove(unit);
  }
  test(the_time_in_millis <= 15);
}
static void _test_scheduler_starve_fuzz_callback(struct scheduler_work *unit) {
  _test_scheduler_starve_fuzz_calls++;
  test(the_time_in_millis % 3 == 0);
  if (the_time_in_millis == 15) {
    test(_test_scheduler_starve_fuzz_calls == 5);
    scheduler_remove(unit);
  }
  test(the_time_in_millis <= 30);
}
static void test_scheduler_starve(void) {
  the_time_in_millis = 0;
  
  struct scheduler_work fizz;
  fizz.callback = _test_scheduler_starve_fizz_callback;
  fizz.delay_millis = 0;
  scheduler_add(&fizz);

  struct scheduler_work fuzz;
  fuzz.callback = _test_scheduler_starve_fuzz_callback;
  fuzz.delay_millis = 3;
  scheduler_add(&fuzz);

  scheduler_run();
  
  test(the_time_in_millis == 15);
}


//
// Test that removing the last element of a two-job schedule works
//

static int _test_scheduler_remove_last_fizz_calls = 0;
static void _test_scheduler_remove_last_fizz_callback(struct scheduler_work *unit) {
  _test_scheduler_remove_last_fizz_calls++;
  test(the_time_in_millis % 3 == 0);
  if (the_time_in_millis == 15) {
    test(_test_scheduler_remove_last_fizz_calls == 5);
    scheduler_remove(unit);
  }
  test(the_time_in_millis <= 15);
}
static void _test_scheduler_remove_last_buzz_callback(struct scheduler_work *unit __attribute__((unused))) {
  test(false);
}
static void test_scheduler_remove_last(void) {
  the_time_in_millis = 0;
  
  struct scheduler_work fizz;
  fizz.callback = _test_scheduler_remove_last_fizz_callback;
  fizz.delay_millis = 3;
  scheduler_add(&fizz);

  struct scheduler_work buzz;
  buzz.callback = _test_scheduler_remove_last_buzz_callback;
  buzz.delay_millis = 5;
  scheduler_add(&buzz);

  scheduler_remove(&buzz);

  scheduler_run();
  
  test(the_time_in_millis == 15);
}

//
// TODO: Test that removing the last element of a three-job schedule works
//

//
// TODO: explicitly test mid-insertion
//

//
// TODO: explicitly test mid-deletion
//

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


//
// TODO: test that head.delay_millis results in optimized insertions with N 20ms work units and a single 1000ms work unit.
//

// ///////////////////////////////////////////////////////////////////////// //
//
// ///////////////////////////////////////////////////////////////////////// //

//
// Test runner
//

int main() {
  scheduler_init();
  
  test_scheduler_empty();
  test_scheduler_max_interval();
  test_scheduler_overflow();
  test_scheduler_fb();
  test_scheduler_ff();
  test_scheduler_starve();
  test_time_lt_time();
  test_scheduler_remove_last();
  
  printf("\n%d failures in %d checks\n", failures, tests);
  return 0;
}
