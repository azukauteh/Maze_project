#ifndef TIMER_H
#define TIMER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t (*TimerClockFn)(void);

typedef struct {
    uint64_t frame_start_ms;
    uint64_t frame_dt_ms;
    uint64_t level_start_ms;
    uint64_t game_start_ms;
    uint64_t total_ms;
    uint64_t frame_count;
} TimerState;

void timer_set_clock_fn(TimerClockFn fn);
void timer_init(TimerState *state);
void timer_reset_level(TimerState *state);
void timer_tick(TimerState *state);
uint64_t timer_frame_dt_ms(const TimerState *state);
float timer_level_elapsed_sec(const TimerState *state);
float timer_total_elapsed_sec(const TimerState *state);
int timer_format_elapsed(const TimerState *state, char *buf, size_t bufsz);

#ifdef __cplusplus
}
#endif

#endif
