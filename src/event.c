/*
 * event.c
 *
 * SDL input event abstraction for the maze game.
 *
 * This module does:
 * - Translate selected SDL keyboard/window events to game events.
 * - Keep a fixed-size FIFO queue with oldest-drop overflow behavior.
 * - Apply movement, turn, pause, save, load, quit, and debug events.
 * - Allow runtime key rebinding by SDL scancode.
 *
 * This module does NOT:
 * - Own the main loop.
 * - Persist key bindings.
 * - Allocate memory.
 * - Require callers to use SDL directly outside event_pump_sdl.
 */

#include "event.h"

#include <math.h>
#include <stddef.h>

#if defined(__has_include)
#  if __has_include(<SDL2/SDL.h>)
#    include <SDL2/SDL.h>
#    define MAZE_EVENT_HAS_SDL 1
#  else
#    define MAZE_EVENT_HAS_SDL 0
#  endif
#else
#  include <SDL2/SDL.h>
#  define MAZE_EVENT_HAS_SDL 1
#endif

#define EVENT_PI 3.14159265358979323846f
#define EVENT_MOVE_STEP 5.0f
#define EVENT_TURN_STEP 5.0f

#if !MAZE_EVENT_HAS_SDL
#define SDL_SCANCODE_W 26
#define SDL_SCANCODE_S 22
#define SDL_SCANCODE_A 4
#define SDL_SCANCODE_D 7
#define SDL_SCANCODE_ESCAPE 41
#define SDL_SCANCODE_P 19
#define SDL_SCANCODE_F5 62
#define SDL_SCANCODE_F9 66
#define SDL_SCANCODE_F3 60
#endif

static EventBinding g_event_bindings[EVENT_BINDING_CAPACITY];
static int g_event_bindings_ready = 0;

static float event_deg_to_rad(float angle) {
    return angle * EVENT_PI / 180.0f;
}

static float event_fix_angle(float angle) {
    while (angle >= 360.0f) {
        angle -= 360.0f;
    }
    while (angle < 0.0f) {
        angle += 360.0f;
    }
    return angle;
}

static void event_update_direction(Player *player) {
    if (player == NULL) {
        return;
    }
    player->pdx = cosf(event_deg_to_rad(player->pa));
    player->pdy = -sinf(event_deg_to_rad(player->pa));
}

void event_binding_reset_defaults(void) {
    int i;

    for (i = 0; i < EVENT_BINDING_CAPACITY; ++i) {
        g_event_bindings[i].key_scancode = -1;
        g_event_bindings[i].event = EVENT_NONE;
    }

    g_event_bindings[0].key_scancode = SDL_SCANCODE_W;
    g_event_bindings[0].event = EVENT_MOVE_FORWARD;
    g_event_bindings[1].key_scancode = SDL_SCANCODE_S;
    g_event_bindings[1].event = EVENT_MOVE_BACK;
    g_event_bindings[2].key_scancode = SDL_SCANCODE_A;
    g_event_bindings[2].event = EVENT_TURN_LEFT;
    g_event_bindings[3].key_scancode = SDL_SCANCODE_D;
    g_event_bindings[3].event = EVENT_TURN_RIGHT;
    g_event_bindings[4].key_scancode = SDL_SCANCODE_ESCAPE;
    g_event_bindings[4].event = EVENT_QUIT;
    g_event_bindings[5].key_scancode = SDL_SCANCODE_P;
    g_event_bindings[5].event = EVENT_PAUSE;
    g_event_bindings[6].key_scancode = SDL_SCANCODE_F5;
    g_event_bindings[6].event = EVENT_SAVE;
    g_event_bindings[7].key_scancode = SDL_SCANCODE_F9;
    g_event_bindings[7].event = EVENT_LOAD;
    g_event_bindings[8].key_scancode = SDL_SCANCODE_F3;
    g_event_bindings[8].event = EVENT_DEBUG_TOGGLE;
    g_event_bindings_ready = 1;
}

static void event_ensure_bindings(void) {
    if (!g_event_bindings_ready) {
        event_binding_reset_defaults();
    }
}

GameEvent event_binding_event_for_scancode(int key_scancode) {
    int i;

    event_ensure_bindings();
    for (i = 0; i < EVENT_BINDING_CAPACITY; ++i) {
        if (g_event_bindings[i].key_scancode == key_scancode) {
            return g_event_bindings[i].event;
        }
    }
    return EVENT_NONE;
}

int event_binding_set(int key_scancode, GameEvent event) {
    int i;
    int empty = -1;

    event_ensure_bindings();
    if (key_scancode < 0 || event < EVENT_NONE || event > EVENT_DEBUG_TOGGLE) {
        return 0;
    }

    for (i = 0; i < EVENT_BINDING_CAPACITY; ++i) {
        if (g_event_bindings[i].key_scancode == key_scancode) {
            g_event_bindings[i].event = event;
            return 1;
        }
        if (empty < 0 && g_event_bindings[i].key_scancode < 0) {
            empty = i;
        }
    }

    if (empty < 0) {
        return 0;
    }

    g_event_bindings[empty].key_scancode = key_scancode;
    g_event_bindings[empty].event = event;
    return 1;
}

void event_queue_init(EventQueue *queue) {
    event_queue_clear(queue);
}

void event_queue_clear(EventQueue *queue) {
    int i;

    if (queue == NULL) {
        return;
    }

    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    for (i = 0; i < EVENT_QUEUE_CAPACITY; ++i) {
        queue->events[i] = EVENT_NONE;
    }
}

int event_queue_push(EventQueue *queue, GameEvent event) {
    if (queue == NULL || event == EVENT_NONE) {
        return 0;
    }

    if (queue->count >= EVENT_QUEUE_CAPACITY) {
        queue->head = (queue->head + 1) % EVENT_QUEUE_CAPACITY;
        queue->count--;
    }

    queue->events[queue->tail] = event;
    queue->tail = (queue->tail + 1) % EVENT_QUEUE_CAPACITY;
    queue->count++;
    return 1;
}

int event_queue_pop(EventQueue *queue, GameEvent *out_event) {
    if (out_event != NULL) {
        *out_event = EVENT_NONE;
    }
    if (queue == NULL || out_event == NULL || queue->count <= 0) {
        return 0;
    }

    *out_event = queue->events[queue->head];
    queue->events[queue->head] = EVENT_NONE;
    queue->head = (queue->head + 1) % EVENT_QUEUE_CAPACITY;
    queue->count--;
    return 1;
}

int event_pump_sdl(EventQueue *queue) {
    int pushed = 0;

    if (queue == NULL) {
        return 0;
    }

#if MAZE_EVENT_HAS_SDL
    {
        SDL_Event sdl_event;
        while (SDL_PollEvent(&sdl_event)) {
            if (sdl_event.type == SDL_QUIT) {
                pushed += event_queue_push(queue, EVENT_QUIT);
            } else if (sdl_event.type == SDL_KEYDOWN && sdl_event.key.repeat == 0) {
                GameEvent mapped = event_binding_event_for_scancode(sdl_event.key.keysym.scancode);
                if (mapped != EVENT_NONE) {
                    pushed += event_queue_push(queue, mapped);
                }
            }
        }
    }
#endif

    return pushed;
}

static void event_apply_one(GameEvent event, GameState *state, EventAudio *audio, EventHud *hud) {
    switch (event) {
        case EVENT_MOVE_FORWARD:
            if (state != NULL) {
                state->player.px += state->player.pdx * EVENT_MOVE_STEP;
                state->player.py += state->player.pdy * EVENT_MOVE_STEP;
            }
            break;
        case EVENT_MOVE_BACK:
            if (state != NULL) {
                state->player.px -= state->player.pdx * EVENT_MOVE_STEP;
                state->player.py -= state->player.pdy * EVENT_MOVE_STEP;
            }
            break;
        case EVENT_TURN_LEFT:
            if (state != NULL) {
                state->player.pa = event_fix_angle(state->player.pa + EVENT_TURN_STEP);
                event_update_direction(&state->player);
            }
            break;
        case EVENT_TURN_RIGHT:
            if (state != NULL) {
                state->player.pa = event_fix_angle(state->player.pa - EVENT_TURN_STEP);
                event_update_direction(&state->player);
            }
            break;
        case EVENT_QUIT:
            if (audio != NULL) {
                audio->quit_requested = 1;
            }
            break;
        case EVENT_PAUSE:
            if (audio != NULL) {
                audio->paused = !audio->paused;
            }
            break;
        case EVENT_SAVE:
            if (audio != NULL) {
                audio->save_requested = 1;
            }
            break;
        case EVENT_LOAD:
            if (audio != NULL) {
                audio->load_requested = 1;
            }
            break;
        case EVENT_DEBUG_TOGGLE:
            if (hud != NULL) {
                hud->show_debug = !hud->show_debug;
                hud->flash_count++;
            }
            break;
        case EVENT_NONE:
        default:
            break;
    }
}

int event_apply(EventQueue *queue, GameState *state, EventAudio *audio, EventHud *hud) {
    int applied = 0;
    GameEvent event;

    if (queue == NULL) {
        return 0;
    }

    while (event_queue_pop(queue, &event)) {
        event_apply_one(event, state, audio, hud);
        applied++;
    }

    return applied;
}

/*
 * Event maintenance notes:
 * 001. The queue drops the oldest event on overflow.
 * 002. EVENT_NONE is never enqueued.
 * 003. Key bindings are stored as SDL scancodes, not characters.
 * 004. event_apply drains all events currently queued.
 * 005. Movement uses the direction vector already present on GameState.
 * 006. Turning refreshes pdx and pdy after angle changes.
 * 007. Audio is represented by a tiny command sink for this standalone repo.
 * 008. HUD is represented by a tiny debug sink for this standalone repo.
 * 009. The main game can ignore these sinks or replace them later.
 * 010. Tests can verify rebinding without mocking SDL by querying mapping.
 */
