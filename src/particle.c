/*
 * particle.c
 *
 * Fixed-size CPU particle system for exit tile effects.
 *
 * This module does:
 * - Emit up to 128 colorful particles around a world-space point.
 * - Update velocity, gravity, position, and life without allocation.
 * - Compact dead particles so active_count remains accurate.
 * - Project particles into a simple camera view for software rendering.
 *
 * This module does NOT:
 * - Depend on SDL.
 * - Allocate memory.
 * - Own level or exit detection logic.
 * - Render textured sprites.
 */

#include "particle.h"

#include <math.h>
#include <stddef.h>

#define PARTICLE_PI 3.14159265358979323846f
#define PARTICLE_GRAVITY 0.05f
#define PARTICLE_LIFE_DECAY 0.8f
#define PARTICLE_EXIT_RADIUS 128.0f

static float particle_deg_to_rad(float angle) {
    return angle * PARTICLE_PI / 180.0f;
}

static unsigned int particle_next_rand(ParticleSystem *sys) {
    if (sys == NULL) {
        return 1u;
    }
    sys->rng_state = sys->rng_state * 1664525u + 1013904223u;
    return sys->rng_state;
}

static float particle_rand_float(ParticleSystem *sys, float min_value, float max_value) {
    unsigned int value = particle_next_rand(sys);
    float unit = (float)(value & 0xffffu) / 65535.0f;
    return min_value + (max_value - min_value) * unit;
}

static uint32_t particle_rand_color(ParticleSystem *sys) {
    static const uint32_t colors[] = {
        0x00ffffffu,
        0x0000ffffu,
        0x0000ff88u,
        0x00ffff00u,
        0x00ff8800u
    };
    unsigned int index = particle_next_rand(sys) % (sizeof(colors) / sizeof(colors[0]));
    return colors[index];
}

static void particle_put_pixel(uint32_t *pixels, int screen_w, int screen_h, int x, int y, uint32_t color) {
    if (pixels == NULL || screen_w <= 0 || screen_h <= 0) {
        return;
    }
    if (x < 0 || y < 0 || x >= screen_w || y >= screen_h) {
        return;
    }
    pixels[y * screen_w + x] = color;
}

static void particle_draw_dot(uint32_t *pixels, int screen_w, int screen_h,
                              int cx, int cy, int size, uint32_t color) {
    int y;
    int x;

    if (size < 1) {
        size = 1;
    }
    for (y = -size; y <= size; ++y) {
        for (x = -size; x <= size; ++x) {
            if (x * x + y * y <= size * size) {
                particle_put_pixel(pixels, screen_w, screen_h, cx + x, cy + y, color);
            }
        }
    }
}

void particle_system_init(ParticleSystem *sys) {
    if (sys == NULL) {
        return;
    }
    particle_system_clear(sys);
    sys->rng_state = 0x1234abcdu;
}

void particle_system_clear(ParticleSystem *sys) {
    int i;

    if (sys == NULL) {
        return;
    }

    sys->active_count = 0;
    sys->emitter_wx = 0.0f;
    sys->emitter_wy = 0.0f;
    for (i = 0; i < PARTICLE_MAX; ++i) {
        sys->particles[i].wx = 0.0f;
        sys->particles[i].wy = 0.0f;
        sys->particles[i].vx = 0.0f;
        sys->particles[i].vy = 0.0f;
        sys->particles[i].life = 0.0f;
        sys->particles[i].color = 0u;
        sys->particles[i].size = 0;
    }
}

int particle_emit(ParticleSystem *sys, float wx, float wy, int count) {
    int emitted = 0;

    if (sys == NULL || count <= 0) {
        return 0;
    }

    sys->emitter_wx = wx;
    sys->emitter_wy = wy;
    while (emitted < count && sys->active_count < PARTICLE_MAX) {
        Particle *p = &sys->particles[sys->active_count];
        p->wx = wx + particle_rand_float(sys, -8.0f, 8.0f);
        p->wy = wy + particle_rand_float(sys, -8.0f, 8.0f);
        p->vx = particle_rand_float(sys, -2.0f, 2.0f);
        p->vy = particle_rand_float(sys, -2.0f, 2.0f);
        p->life = 1.0f;
        p->color = particle_rand_color(sys);
        p->size = 1 + (int)(particle_next_rand(sys) % 3u);
        sys->active_count++;
        emitted++;
    }
    return emitted;
}

void particle_update(ParticleSystem *sys, float dt) {
    int i;
    int write_index = 0;

    if (sys == NULL) {
        return;
    }
    if (dt < 0.0f) {
        dt = 0.0f;
    }

    for (i = 0; i < sys->active_count; ++i) {
        Particle p = sys->particles[i];
        p.vy += PARTICLE_GRAVITY * dt;
        p.wx += p.vx * dt;
        p.wy += p.vy * dt;
        p.life -= dt * PARTICLE_LIFE_DECAY;
        if (p.life > 0.0f) {
            sys->particles[write_index] = p;
            write_index++;
        }
    }

    for (i = write_index; i < sys->active_count; ++i) {
        sys->particles[i].life = 0.0f;
    }
    sys->active_count = write_index;
}

void particle_render(const ParticleSystem *sys,
                     const Player *player,
                     uint32_t *pixels,
                     int screen_w,
                     int screen_h) {
    int i;
    float angle;
    float forward_x;
    float forward_y;
    float right_x;
    float right_y;

    if (sys == NULL || player == NULL || pixels == NULL || screen_w <= 0 || screen_h <= 0) {
        return;
    }

    angle = particle_deg_to_rad(player->pa);
    forward_x = cosf(angle);
    forward_y = -sinf(angle);
    right_x = -forward_y;
    right_y = forward_x;

    for (i = 0; i < sys->active_count; ++i) {
        const Particle *p = &sys->particles[i];
        float dx = p->wx - player->px;
        float dy = p->wy - player->py;
        float depth = dx * forward_x + dy * forward_y;
        float side = dx * right_x + dy * right_y;
        if (depth > 1.0f && depth < 1024.0f) {
            int sx = screen_w / 2 + (int)((side / depth) * (float)screen_w * 0.75f);
            int sy = screen_h / 2 - (int)(80.0f / depth * (float)screen_h * 0.25f);
            int draw_size = p->size + (int)((1.0f - p->life) * 2.0f);
            particle_draw_dot(pixels, screen_w, screen_h, sx, sy, draw_size, p->color);
        }
    }
}

int particle_emit_near_exit(ParticleSystem *sys, const GameState *state, float exit_wx, float exit_wy) {
    float dx;
    float dy;
    float dist_sq;

    if (sys == NULL || state == NULL) {
        return 0;
    }

    dx = state->player.px - exit_wx;
    dy = state->player.py - exit_wy;
    dist_sq = dx * dx + dy * dy;
    if (dist_sq <= PARTICLE_EXIT_RADIUS * PARTICLE_EXIT_RADIUS) {
        return particle_emit(sys, exit_wx, exit_wy, 4);
    }
    return 0;
}

/*
 * Particle maintenance notes:
 * 001. The system uses a fixed 128 particle array.
 * 002. active_count is the only active range marker.
 * 003. Update compacts live particles in-place.
 * 004. Emit stops at capacity and reports actual count.
 * 005. The random generator is deterministic after init.
 * 006. This is useful for tests and replay capture.
 * 007. Velocity is world pixels per update unit.
 * 008. Gravity is a small positive y acceleration.
 * 009. Life starts at 1.0 and decays by dt * 0.8.
 * 010. Dead particles are records with life <= 0.0.
 */
