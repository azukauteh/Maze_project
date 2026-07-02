#ifndef PARTICLE_H
#define PARTICLE_H

#include <stdint.h>

#include "engine.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PARTICLE_MAX 128

typedef struct {
    float wx;
    float wy;
    float vx;
    float vy;
    float life;
    uint32_t color;
    int size;
} Particle;

typedef struct {
    Particle particles[PARTICLE_MAX];
    int active_count;
    float emitter_wx;
    float emitter_wy;
    unsigned int rng_state;
} ParticleSystem;

void particle_system_init(ParticleSystem *sys);
void particle_system_clear(ParticleSystem *sys);
int particle_emit(ParticleSystem *sys, float wx, float wy, int count);
void particle_update(ParticleSystem *sys, float dt);
void particle_render(const ParticleSystem *sys,
                     const Player *player,
                     uint32_t *pixels,
                     int screen_w,
                     int screen_h);
int particle_emit_near_exit(ParticleSystem *sys, const GameState *state, float exit_wx, float exit_wy);

#ifdef __cplusplus
}
#endif

#endif
