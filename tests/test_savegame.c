/*
 * tests/test_savegame.c — unit tests for the savegame subsystem.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "../src/engine.h"
#include "../src/savegame.h"

static int runs = 0, passed = 0;
#define CHECK(c, m) do { runs++; if(c){passed++; printf("  PASS  %s\n",m);} \
    else printf("  FAIL  %s (line %d)\n",m,__LINE__); } while(0)

static GameState make_state(void) {
    GameState s;
    memset(&s, 0, sizeof(s));
    s.width  = 320;
    s.height = 240;
    engine_init(&s);
    return s;
}

static void free_state(GameState *s) {
    if (s->pixels) { free(s->pixels); s->pixels = NULL; }
}

static void test_checksum(void) {
    printf("\n[savegame] Checksum\n");

    uint8_t data[] = { 0x01, 0x02, 0x03, 0x04 };
    uint32_t cs1 = savegame_checksum(data, sizeof(data));
    uint32_t cs2 = savegame_checksum(data, sizeof(data));
    CHECK(cs1 == cs2, "checksum is deterministic");

    data[0] = 0xFF;
    uint32_t cs3 = savegame_checksum(data, sizeof(data));
    CHECK(cs1 != cs3, "checksum changes when data changes");

    CHECK(savegame_checksum(NULL, 0) == 0, "NULL/empty data = 0");
}

static void test_save_buffer(void) {
    printf("\n[savegame] Save to buffer\n");
    GameState s = make_state();
    s.current_level = 3;
    s.player.px = 200.0f;
    s.player.py = 300.0f;

    uint8_t buf[SAVE_SIZE];
    size_t  wr = 0;
    CHECK(savegame_save_to_buffer(&s, buf, sizeof(buf), &wr) == 1,
          "save to buffer succeeds");
    CHECK(wr == SAVE_SIZE, "written == SAVE_SIZE");

    /* Check magic in output */
    uint32_t magic;
    memcpy(&magic, buf, 4);
    CHECK(magic == SAVE_MAGIC, "magic correct in output");

    /* Buffer too small */
    uint8_t small[4];
    CHECK(savegame_save_to_buffer(&s, small, sizeof(small), &wr) == 0,
          "small buffer rejected");

    /* NULL inputs */
    CHECK(savegame_save_to_buffer(NULL, buf, sizeof(buf), &wr) == 0,
          "NULL state rejected");
    CHECK(savegame_save_to_buffer(&s,  NULL, sizeof(buf), &wr) == 0,
          "NULL out rejected");

    free_state(&s);
}

static void test_round_trip(void) {
    printf("\n[savegame] Round-trip\n");
    GameState s1 = make_state();
    s1.current_level = 4;
    s1.player.px     = 123.5f;
    s1.player.py     = 456.75f;
    s1.player.pa     = 180;
    s1.player.pdx    = -1.0f;
    s1.player.pdy    =  0.0f;

    uint8_t buf[SAVE_SIZE];
    size_t  wr = 0;
    savegame_save_to_buffer(&s1, buf, sizeof(buf), &wr);

    GameState s2 = make_state();
    CHECK(savegame_load_from_data(buf, wr, &s2) == 1,
          "load from buffer succeeds");
    CHECK(s2.current_level == 4,       "level preserved");
    CHECK(fabsf(s2.player.px - 123.5f)   < 0.01f, "px preserved");
    CHECK(fabsf(s2.player.py - 456.75f)  < 0.01f, "py preserved");
    CHECK(s2.player.pa == 180,            "pa preserved");
    CHECK(fabsf(s2.player.pdx - (-1.0f)) < 0.001f, "pdx preserved");
    CHECK(s2.level_complete == 0,          "level_complete reset");

    free_state(&s1);
    free_state(&s2);
}

static void test_load_invalid(void) {
    printf("\n[savegame] Load invalid data\n");
    GameState s = make_state();

    CHECK(savegame_load_from_data(NULL, 32, &s)  == 0, "NULL data");
    CHECK(savegame_load_from_data((uint8_t*)"x", 1, &s) == 0, "too short");

    /* Wrong magic */
    uint8_t buf[SAVE_SIZE];
    memset(buf, 0, sizeof(buf));
    CHECK(savegame_load_from_data(buf, sizeof(buf), &s) == 0, "wrong magic");

    /* Corrupt checksum */
    GameState s2 = make_state();
    s2.current_level = 2;
    size_t wr = 0;
    savegame_save_to_buffer(&s2, buf, sizeof(buf), &wr);
    buf[0] ^= 0xFF; /* corrupt first byte */
    CHECK(savegame_load_from_data(buf, wr, &s) == 0, "corrupt checksum rejected");

    /* NaN in float field */
    savegame_save_to_buffer(&s2, buf, sizeof(buf), &wr);
    /* Overwrite px with NaN */
    float nan_val = 0.0f / 0.0f;
    memcpy(buf + 8, &nan_val, 4);
    /* Recompute checksum so it passes that check */
    uint32_t cs = savegame_checksum(buf, SAVE_SIZE - 4);
    memcpy(buf + SAVE_SIZE - 4, &cs, 4);
    CHECK(savegame_load_from_data(buf, SAVE_SIZE, &s) == 0, "NaN px rejected");

    free_state(&s);
    free_state(&s2);
}

static void test_file_slots(void) {
    printf("\n[savegame] File slot I/O\n");
    const char *dir = "/tmp/maze_test_saves";

    GameState s1 = make_state();
    s1.current_level = 5;
    s1.player.px = 99.0f;

    /* Cleanup before test */
    savegame_delete_slot(0, dir);
    CHECK(savegame_slot_exists(0, dir) == 0, "slot 0 does not exist initially");

    CHECK(savegame_save_slot(&s1, 0, dir) == 1, "save slot 0 succeeds");
    CHECK(savegame_slot_exists(0, dir) == 1, "slot 0 exists after save");

    GameState s2 = make_state();
    CHECK(savegame_load_slot(&s2, 0, dir) == 1, "load slot 0 succeeds");
    CHECK(s2.current_level == 5, "level preserved through file I/O");
    CHECK(fabsf(s2.player.px - 99.0f) < 0.01f, "px preserved through file I/O");

    /* Meta */
    SaveMeta meta = savegame_read_meta(0, dir);
    CHECK(meta.valid == 1,   "meta.valid");
    CHECK(meta.level == 5,   "meta.level");
    CHECK(meta.slot  == 0,   "meta.slot");

    /* Delete */
    savegame_delete_slot(0, dir);
    CHECK(savegame_slot_exists(0, dir) == 0, "slot 0 deleted");

    /* Load missing slot */
    CHECK(savegame_load_slot(&s2, 2, dir) == 0, "load missing slot fails");

    /* Invalid slot index */
    CHECK(savegame_save_slot(&s1, SAVE_MAX_SLOTS, dir) == 0,
          "out-of-range slot rejected");

    free_state(&s1);
    free_state(&s2);
}

int main(void) {
    printf("=== savegame tests ===\n");
    test_checksum();
    test_save_buffer();
    test_round_trip();
    test_load_invalid();
    test_file_slots();
    printf("\n%d / %d passed\n", passed, runs);
    return passed == runs ? 0 : 1;
}
