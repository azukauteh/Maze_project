#include "event.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void init_state(GameState *state) {
    memset(state, 0, sizeof(*state));
    state->width = 320;
    state->height = 240;
    state->player.px = 100.0f;
    state->player.py = 100.0f;
    state->player.pa = 0.0f;
    state->player.pdx = 1.0f;
    state->player.pdy = 0.0f;
}

static void test_fifo(void) {
    EventQueue queue;
    GameEvent event;

    event_queue_init(&queue);
    assert(event_queue_push(&queue, EVENT_MOVE_FORWARD));
    assert(event_queue_push(&queue, EVENT_MOVE_BACK));
    assert(event_queue_push(&queue, EVENT_TURN_LEFT));
    assert(event_queue_push(&queue, EVENT_TURN_RIGHT));
    assert(event_queue_push(&queue, EVENT_DEBUG_TOGGLE));
    assert(queue.count == 5);
    assert(event_queue_pop(&queue, &event) && event == EVENT_MOVE_FORWARD);
    assert(event_queue_pop(&queue, &event) && event == EVENT_MOVE_BACK);
    assert(event_queue_pop(&queue, &event) && event == EVENT_TURN_LEFT);
    assert(event_queue_pop(&queue, &event) && event == EVENT_TURN_RIGHT);
    assert(event_queue_pop(&queue, &event) && event == EVENT_DEBUG_TOGGLE);
    assert(!event_queue_pop(&queue, &event));
}

static void test_overflow_drops_oldest(void) {
    EventQueue queue;
    GameEvent event;
    int i;

    event_queue_init(&queue);
    for (i = 0; i < EVENT_QUEUE_CAPACITY + 5; ++i) {
        event_queue_push(&queue, i < 5 ? EVENT_MOVE_FORWARD : EVENT_MOVE_BACK);
    }
    assert(queue.count == EVENT_QUEUE_CAPACITY);
    assert(event_queue_pop(&queue, &event));
    assert(event == EVENT_MOVE_BACK);
}

static void test_apply_movement_turn_debug(void) {
    GameState state;
    EventQueue queue;
    EventAudio audio;
    EventHud hud;

    init_state(&state);
    memset(&audio, 0, sizeof(audio));
    memset(&hud, 0, sizeof(hud));
    event_queue_init(&queue);
    event_queue_push(&queue, EVENT_MOVE_FORWARD);
    assert(event_apply(&queue, &state, &audio, &hud) == 1);
    assert(state.player.px > 100.0f || state.player.py != 100.0f);

    event_queue_push(&queue, EVENT_TURN_LEFT);
    assert(event_apply(&queue, &state, &audio, &hud) == 1);
    assert(fabsf(state.player.pa - 5.0f) < 0.01f);

    event_queue_push(&queue, EVENT_DEBUG_TOGGLE);
    assert(event_apply(&queue, &state, &audio, &hud) == 1);
    assert(hud.show_debug == 1);
}

static void test_binding_set(void) {
    event_binding_reset_defaults();
    assert(event_binding_set(123, EVENT_DEBUG_TOGGLE));
    assert(event_binding_event_for_scancode(123) == EVENT_DEBUG_TOGGLE);
    assert(event_binding_set(123, EVENT_SAVE));
    assert(event_binding_event_for_scancode(123) == EVENT_SAVE);
    assert(!event_binding_set(-1, EVENT_SAVE));
}

static void test_null_safety(void) {
    GameEvent event = EVENT_SAVE;

    event_queue_init(NULL);
    event_queue_clear(NULL);
    assert(event_queue_push(NULL, EVENT_MOVE_FORWARD) == 0);
    assert(event_queue_pop(NULL, &event) == 0);
    assert(event == EVENT_NONE);
    assert(event_queue_pop(NULL, NULL) == 0);
    assert(event_pump_sdl(NULL) == 0);
    assert(event_apply(NULL, NULL, NULL, NULL) == 0);
}

int main(void) {
    test_fifo();
    test_overflow_drops_oldest();
    test_apply_movement_turn_debug();
    test_binding_set();
    test_null_safety();
    printf("test_event passed\n");
    return 0;
}

/* Event test notes 001. SDL event pumping is intentionally not required here. */
