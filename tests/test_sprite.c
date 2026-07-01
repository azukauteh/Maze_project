/*
 * tests/test_sprite.c — unit tests for the sprite subsystem.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "../src/sprite.h"
#include "../src/texture.h"

static int runs = 0, passed = 0;
#define CHECK(c, m) do { runs++; if(c){passed++; printf("  PASS  %s\n",m);} \
    else printf("  FAIL  %s (line %d)\n",m,__LINE__); } while(0)

static void test_list_lifecycle(void) {
    printf("\n[sprite] List lifecycle\n");
    SpriteList list;
    sprite_list_init(&list);
    CHECK(list.count == 0, "init: count 0");

    int idx = sprite_add(&list, 200.0f, 200.0f, SPRITE_TYPE_DECORATION, 0, 1.0f);
    CHECK(idx == 0, "first add returns 0");
    CHECK(list.count == 1, "count == 1 after add");
    CHECK(list.sprites[0].active == 1, "added sprite is active");
    CHECK(list.sprites[0].type == SPRITE_TYPE_DECORATION, "type correct");

    /* Collect */
    sprite_collect(&list, 0);
    CHECK(list.sprites[0].active == 0, "collected sprite inactive");

    /* Overflow — adding beyond SPRITE_MAX_COUNT */
    sprite_list_clear(&list);
    CHECK(list.count == 0, "clear resets count");
    for (int i = 0; i < SPRITE_MAX_COUNT; i++)
        sprite_add(&list, 0, 0, SPRITE_TYPE_PICKUP, 0, 1.0f);
    CHECK(list.count == SPRITE_MAX_COUNT, "count at max");
    int overflow = sprite_add(&list, 0, 0, SPRITE_TYPE_PICKUP, 0, 1.0f);
    CHECK(overflow == -1, "add beyond max returns -1");
    CHECK(list.count == SPRITE_MAX_COUNT, "count unchanged at max");

    /* Edge: invalid collect index */
    sprite_collect(&list, -1);           /* must not crash */
    sprite_collect(&list, SPRITE_MAX_COUNT); /* must not crash */
    sprite_collect(NULL, 0);             /* must not crash */
}

static void test_zbuffer(void) {
    printf("\n[sprite] ZBuffer\n");
    ZBuffer zb;

    CHECK(zbuffer_alloc(&zb, 320) == 1, "alloc 320 cols");
    CHECK(zb.width == 320, "width == 320");
    CHECK(zb.zbuf != NULL, "zbuf allocated");

    /* All entries initialised to large value */
    int all_large = 1;
    for (int i = 0; i < 320; i++)
        if (zb.zbuf[i] < 1e5f) { all_large = 0; break; }
    CHECK(all_large, "all entries initialised large");

    /* Set and clear */
    zb.zbuf[100] = 42.0f;
    zbuffer_clear(&zb);
    CHECK(zb.zbuf[100] >= 1e5f, "clear resets entry");

    zbuffer_free(&zb);
    CHECK(zb.zbuf == NULL, "free: zbuf NULL");

    /* NULL / invalid */
    CHECK(zbuffer_alloc(NULL, 320) == 0, "NULL zb rejected");
    CHECK(zbuffer_alloc(&zb,    0) == 0, "zero width rejected");
}

static void test_sort(void) {
    printf("\n[sprite] Sort by distance\n");
    SpriteList list;
    sprite_list_init(&list);

    /* Add 3 sprites at known distances from (0,0) */
    sprite_add(&list, 100.0f, 0.0f, SPRITE_TYPE_DECORATION, 0, 1.0f); /* dist=100 */
    sprite_add(&list, 300.0f, 0.0f, SPRITE_TYPE_DECORATION, 0, 1.0f); /* dist=300 */
    sprite_add(&list, 200.0f, 0.0f, SPRITE_TYPE_DECORATION, 0, 1.0f); /* dist=200 */

    sprite_sort_by_distance(&list, 0.0f, 0.0f);

    /* After sort: descending distance (far first) */
    CHECK(list.sprites[0].wx == 300.0f, "farthest sprite first");
    CHECK(list.sprites[1].wx == 200.0f, "middle sprite second");
    CHECK(list.sprites[2].wx == 100.0f, "nearest sprite last");

    /* Single sprite — no crash */
    sprite_list_clear(&list);
    sprite_add(&list, 50.0f, 50.0f, SPRITE_TYPE_PICKUP, 0, 1.0f);
    sprite_sort_by_distance(&list, 0.0f, 0.0f); /* must not crash */

    /* NULL / empty */
    sprite_sort_by_distance(NULL, 0.0f, 0.0f);
    sprite_list_clear(&list);
    sprite_sort_by_distance(&list, 0.0f, 0.0f);
}

static void test_project(void) {
    printf("\n[sprite] Projection\n");
    Sprite sp;
    memset(&sp, 0, sizeof(sp));
    sp.active    = 1;
    sp.wx        = 100.0f + 128.0f; /* one tile ahead of player */
    sp.wy        = 100.0f;
    sp.scale     = 1.0f;
    sp.type      = SPRITE_TYPE_DECORATION;
    sp.tint      = 0xFFFFFFFFu;

    /* Player at (100,100) facing east (angle=0) */
    SpriteProjection proj = sprite_project(&sp,
        100.0f, 100.0f, 0.0f, 320, 240, 60);

    CHECK(proj.visible == 1, "sprite in front of player is visible");
    CHECK(proj.dist    > 0,  "projected distance > 0");
    CHECK(proj.screen_x > 0 && proj.screen_x < 320, "screen_x in bounds");

    /* Sprite behind player (negative cam_z) */
    Sprite behind = sp;
    behind.wx = 100.0f - 200.0f; /* behind east-facing player */
    SpriteProjection proj2 = sprite_project(&behind,
        100.0f, 100.0f, 0.0f, 320, 240, 60);
    CHECK(proj2.visible == 0, "sprite behind player not visible");

    /* Inactive sprite */
    Sprite inactive = sp;
    inactive.active = 0;
    SpriteProjection proj3 = sprite_project(&inactive,
        100.0f, 100.0f, 0.0f, 320, 240, 60);
    CHECK(proj3.visible == 0, "inactive sprite not visible");

    /* NULL sprite */
    SpriteProjection proj4 = sprite_project(NULL, 0, 0, 0, 320, 240, 60);
    CHECK(proj4.visible == 0, "NULL sprite not visible");
}

static void test_render(void) {
    printf("\n[sprite] Render integration\n");
    SpriteList list;
    sprite_list_init(&list);
    sprite_add(&list, 100.0f + 128.0f, 100.0f,
               SPRITE_TYPE_DECORATION, 0, 1.0f);

    TextureRegistry reg;
    texture_registry_generate_defaults(&reg);

    ZBuffer zb;
    zbuffer_alloc(&zb, 320);

    uint32_t *pixels = (uint32_t *)calloc(320 * 240, sizeof(uint32_t));

    /* Must not crash */
    sprite_render_all(&list, &zb, &reg, pixels,
                      320, 240, 100.0f, 100.0f, 0.0f, 60);

    /* NULL safety */
    sprite_render_all(NULL, &zb, &reg, pixels, 320, 240, 0, 0, 0, 60);
    sprite_render_all(&list, NULL, &reg, pixels, 320, 240, 0, 0, 0, 60);
    sprite_render_all(&list, &zb, NULL, pixels, 320, 240, 0, 0, 0, 60);
    sprite_render_all(&list, &zb, &reg, NULL, 320, 240, 0, 0, 0, 60);

    free(pixels);
    zbuffer_free(&zb);
    texture_registry_free(&reg);
    CHECK(1, "sprite render integration does not crash");
}

int main(void) {
    printf("=== sprite tests ===\n");
    test_list_lifecycle();
    test_zbuffer();
    test_sort();
    test_project();
    test_render();
    printf("\n%d / %d passed\n", passed, runs);
    return passed == runs ? 0 : 1;
}
