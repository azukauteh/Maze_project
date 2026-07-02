#include "timer.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fake_now_ms = 0;

static uint64_t fake_clock(void) {
    return fake_now_ms;
}

static void advance_ms(uint64_t ms) {
    fake_now_ms += ms;
}

static void test_frame_delta(void) {
    TimerState timer;

    fake_now_ms = 0;
    timer_set_clock_fn(fake_clock);
    timer_init(&timer);
    advance_ms(16);
    timer_tick(&timer);
    assert(timer_frame_dt_ms(&timer) == 16);
    advance_ms(16);
    timer_tick(&timer);
    assert(timer_frame_dt_ms(&timer) == 16);
    assert(timer.frame_count == 2);
}

static void test_elapsed_seconds(void) {
    TimerState timer;

    fake_now_ms = 0;
    timer_set_clock_fn(fake_clock);
    timer_init(&timer);
    advance_ms(3000);
    timer_tick(&timer);
    assert(fabsf(timer_level_elapsed_sec(&timer) - 3.0f) < 0.001f);
    assert(fabsf(timer_total_elapsed_sec(&timer) - 3.0f) < 0.001f);
}

static void test_format_elapsed(void) {
    TimerState timer;
    char buf[16];

    fake_now_ms = 0;
    timer_set_clock_fn(fake_clock);
    timer_init(&timer);
    advance_ms(3000);
    assert(timer_format_elapsed(&timer, buf, sizeof(buf)) == 1);
    assert(strcmp(buf, "00:03") == 0);
    fake_now_ms = 60000;
    assert(timer_format_elapsed(&timer, buf, sizeof(buf)) == 1);
    assert(strcmp(buf, "01:00") == 0);
    fake_now_ms = 600000;
    assert(timer_format_elapsed(&timer, buf, sizeof(buf)) == 1);
    assert(strcmp(buf, "10:00") == 0);
}

static void test_total_accumulates(void) {
    TimerState timer;

    fake_now_ms = 100;
    timer_set_clock_fn(fake_clock);
    timer_init(&timer);
    advance_ms(10);
    timer_tick(&timer);
    advance_ms(20);
    timer_tick(&timer);
    advance_ms(30);
    timer_tick(&timer);
    assert(timer.frame_count == 3);
    assert(timer.total_ms == 60);
    assert(fabsf(timer_total_elapsed_sec(&timer) - 0.060f) < 0.001f);
}

static void test_null_safety(void) {
    char buf[4] = { 'x', 'x', 'x', '\0' };

    timer_set_clock_fn(NULL);
    timer_init(NULL);
    timer_reset_level(NULL);
    timer_tick(NULL);
    assert(timer_frame_dt_ms(NULL) == 0);
    assert(timer_level_elapsed_sec(NULL) == 0.0f);
    assert(timer_total_elapsed_sec(NULL) == 0.0f);
    assert(timer_format_elapsed(NULL, buf, sizeof(buf)) == 0);
    assert(timer_format_elapsed(NULL, NULL, 0) == 0);
}

int main(void) {
    test_frame_delta();
    test_elapsed_seconds();
    test_format_elapsed();
    test_total_accumulates();
    test_null_safety();
    timer_set_clock_fn(NULL);
    printf("test_timer passed\n");
    return 0;
}

/* Timer test notes 001. Fake time makes every assertion deterministic. */
