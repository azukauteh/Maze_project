#include "particle.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define TEST_W 320
#define TEST_H 240

static Player test_player(void) {
    Player p;
    p.px = 100.0f;
    p.py = 200.0f;
    p.pa = 0.0f;
    p.pdx = 1.0f;
    p.pdy = 0.0f;
    return p;
}

static void test_emit_update_lifecycle(void) {
    ParticleSystem sys;
    float first_life;
    float first_x;
    float first_y;

    particle_system_init(&sys);
    assert(particle_emit(&sys, 200.0f, 200.0f, 10) == 10);
    assert(sys.active_count == 10);
    first_life = sys.particles[0].life;
    first_x = sys.particles[0].wx;
    first_y = sys.particles[0].wy;
    particle_update(&sys, 1.0f);
    assert(sys.active_count == 10);
    assert(sys.particles[0].life < first_life);
    assert(sys.particles[0].wx != first_x || sys.particles[0].wy != first_y);
    particle_update(&sys, 10.0f);
    assert(sys.active_count == 0);
}

static void test_render_null_pixels(void) {
    ParticleSystem sys;
    Player player = test_player();

    particle_system_init(&sys);
    particle_emit(&sys, 220.0f, 200.0f, 5);
    particle_render(&sys, &player, NULL, TEST_W, TEST_H);
}

static void test_capacity_clamps(void) {
    ParticleSystem sys;

    particle_system_init(&sys);
    assert(particle_emit(&sys, 200.0f, 200.0f, PARTICLE_MAX + 50) == PARTICLE_MAX);
    assert(sys.active_count == PARTICLE_MAX);
    assert(particle_emit(&sys, 200.0f, 200.0f, 1) == 0);
    assert(sys.active_count == PARTICLE_MAX);
}

static void test_clear(void) {
    ParticleSystem sys;

    particle_system_init(&sys);
    particle_emit(&sys, 200.0f, 200.0f, 20);
    assert(sys.active_count == 20);
    particle_system_clear(&sys);
    assert(sys.active_count == 0);
}

static void test_render_pixels_and_exit_emitter(void) {
    ParticleSystem sys;
    GameState state;
    uint32_t pixels[TEST_W * TEST_H];
    int i;
    int nonzero = 0;
    Player player = test_player();

    memset(&state, 0, sizeof(state));
    state.player = player;
    particle_system_init(&sys);
    assert(particle_emit_near_exit(&sys, &state, 220.0f, 200.0f) > 0);
    memset(pixels, 0, sizeof(pixels));
    particle_render(&sys, &player, pixels, TEST_W, TEST_H);
    for (i = 0; i < TEST_W * TEST_H; ++i) {
        if (pixels[i] != 0u) {
            nonzero++;
        }
    }
    assert(nonzero > 0);
}

static void test_null_safety(void) {
    ParticleSystem sys;
    Player player = test_player();

    particle_system_init(NULL);
    particle_system_clear(NULL);
    assert(particle_emit(NULL, 1.0f, 1.0f, 1) == 0);
    particle_update(NULL, 1.0f);
    particle_render(NULL, &player, NULL, 0, 0);
    particle_system_init(&sys);
    particle_render(&sys, NULL, NULL, 0, 0);
    assert(particle_emit_near_exit(NULL, NULL, 0.0f, 0.0f) == 0);
}

int main(void) {
    test_emit_update_lifecycle();
    test_render_null_pixels();
    test_capacity_clamps();
    test_clear();
    test_render_pixels_and_exit_emitter();
    test_null_safety();
    printf("test_particle passed\n");
    return 0;
}

/* Particle test notes 001. Deterministic RNG keeps movement assertions stable. */
