#ifndef EVENT_H
#define EVENT_H

#include "engine.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EVENT_QUEUE_CAPACITY 32
#define EVENT_BINDING_CAPACITY 32

typedef enum {
    EVENT_NONE = 0,
    EVENT_MOVE_FORWARD,
    EVENT_MOVE_BACK,
    EVENT_TURN_LEFT,
    EVENT_TURN_RIGHT,
    EVENT_QUIT,
    EVENT_PAUSE,
    EVENT_SAVE,
    EVENT_LOAD,
    EVENT_DEBUG_TOGGLE
} GameEvent;

typedef struct {
    GameEvent events[EVENT_QUEUE_CAPACITY];
    int head;
    int tail;
    int count;
} EventQueue;

typedef struct {
    int key_scancode;
    GameEvent event;
} EventBinding;

typedef struct {
    int paused;
    int quit_requested;
    int save_requested;
    int load_requested;
} EventAudio;

typedef struct {
    int show_debug;
    int flash_count;
} EventHud;

void event_queue_init(EventQueue *queue);
int event_queue_push(EventQueue *queue, GameEvent event);
int event_queue_pop(EventQueue *queue, GameEvent *out_event);
void event_queue_clear(EventQueue *queue);
int event_pump_sdl(EventQueue *queue);
int event_apply(EventQueue *queue, GameState *state, EventAudio *audio, EventHud *hud);
int event_binding_set(int key_scancode, GameEvent event);
GameEvent event_binding_event_for_scancode(int key_scancode);
void event_binding_reset_defaults(void);

#ifdef __cplusplus
}
#endif

#endif
