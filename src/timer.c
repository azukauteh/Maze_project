/*
 * timer.c
 *
 * Frame and level timer for the maze game.
 *
 * This module does:
 * - Track per-frame delta time in milliseconds.
 * - Track elapsed time for the active level.
 * - Track elapsed time since the game timer was initialized.
 * - Format elapsed time as MM:SS for the HUD.
 * - Allow tests to inject a deterministic clock function.
 *
 * This module does NOT:
 * - Own the game loop.
 * - Sleep, throttle, or schedule frames.
 * - Require SDL from unit tests.
 * - Allocate memory.
 */

#include "timer.h"

#include <stdio.h>
#include <time.h>

static uint64_t timer_default_clock_ms(void) {
#if defined(_WIN32)
    return (uint64_t)((double)clock() * 1000.0 / (double)CLOCKS_PER_SEC);
#else
    return (uint64_t)((double)clock() * 1000.0 / (double)CLOCKS_PER_SEC);
#endif
}

static TimerClockFn g_timer_clock_fn = timer_default_clock_ms;

static uint64_t timer_now_ms(void) {
    if (g_timer_clock_fn == NULL) {
        return timer_default_clock_ms();
    }
    return g_timer_clock_fn();
}

void timer_set_clock_fn(TimerClockFn fn) {
    if (fn == NULL) {
        g_timer_clock_fn = timer_default_clock_ms;
        return;
    }
    g_timer_clock_fn = fn;
}

void timer_init(TimerState *state) {
    uint64_t now;

    if (state == NULL) {
        return;
    }

    now = timer_now_ms();
    state->frame_start_ms = now;
    state->frame_dt_ms = 0;
    state->level_start_ms = now;
    state->game_start_ms = now;
    state->total_ms = 0;
    state->frame_count = 0;
}

void timer_reset_level(TimerState *state) {
    if (state == NULL) {
        return;
    }
    state->level_start_ms = timer_now_ms();
}

void timer_tick(TimerState *state) {
    uint64_t now;

    if (state == NULL) {
        return;
    }

    now = timer_now_ms();
    if (now >= state->frame_start_ms) {
        state->frame_dt_ms = now - state->frame_start_ms;
    } else {
        state->frame_dt_ms = 0;
    }

    state->frame_start_ms = now;
    if (now >= state->game_start_ms) {
        state->total_ms = now - state->game_start_ms;
    } else {
        state->total_ms = 0;
    }
    state->frame_count++;
}

uint64_t timer_frame_dt_ms(const TimerState *state) {
    if (state == NULL) {
        return 0;
    }
    return state->frame_dt_ms;
}

float timer_level_elapsed_sec(const TimerState *state) {
    uint64_t now;

    if (state == NULL) {
        return 0.0f;
    }

    now = timer_now_ms();
    if (now < state->level_start_ms) {
        return 0.0f;
    }
    return (float)(now - state->level_start_ms) / 1000.0f;
}

float timer_total_elapsed_sec(const TimerState *state) {
    if (state == NULL) {
        return 0.0f;
    }
    return (float)state->total_ms / 1000.0f;
}

int timer_format_elapsed(const TimerState *state, char *buf, size_t bufsz) {
    uint64_t seconds;
    uint64_t minutes;
    uint64_t remain;

    if (state == NULL || buf == NULL || bufsz == 0) {
        return 0;
    }

    seconds = (uint64_t)timer_level_elapsed_sec(state);
    minutes = seconds / 60;
    remain = seconds % 60;

    if (bufsz < 6) {
        buf[0] = '\0';
        return 0;
    }

    snprintf(buf, bufsz, "%02llu:%02llu",
             (unsigned long long)minutes,
             (unsigned long long)remain);
    return 1;
}

/*
 * Timer maintenance notes for future C changes:
 * 001. Keep timer state plain old data so save/debug tooling can inspect it.
 * 002. Do not add heap allocation to this module.
 * 003. Do not make tests depend on wall-clock time.
 * 004. The injected clock is process-global by design for simple C tests.
 * 005. Reset the clock with timer_set_clock_fn(NULL) after tests.
 * 006. timer_tick is the only function that advances total_ms.
 * 007. level elapsed time is sampled from the clock so HUD reads stay live.
 * 008. frame_dt_ms clamps backward clock movement to zero.
 * 009. total_ms clamps backward clock movement to zero.
 * 010. MM:SS formatting intentionally does not cap minutes at 99.
 */
