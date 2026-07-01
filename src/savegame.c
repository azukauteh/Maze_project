/*
 * src/savegame.c — binary savegame serialisation.
 *
 * VULN (intentional, for fuzzer):
 *   savegame_load_from_data() checks the magic and version but copies
 *   the player data fields using fixed offsets regardless of the version
 *   field value. A crafted save with version=999 and extra payload bytes
 *   after the standard fields would still be accepted (bad checksum
 *   would normally reject it, but the checksum is computed from the
 *   input buffer — a crafted input where checksum field == XOR of the
 *   preceding bytes passes). The copy itself is bounded here, but the
 *   version bypass means future struct layout changes would be a real
 *   OOB. ASan can catch this when paired with a carefully crafted input.
 */

#include "savegame.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SAVE_DIR_DEFAULT "/tmp/maze_saves"

/* ------------------------------------------------------------------ */
/* Checksum                                                            */
/* ------------------------------------------------------------------ */

uint32_t savegame_checksum(const uint8_t *data, size_t size) {
    uint32_t x = 0;
    for (size_t i = 0; i < size; i++) x ^= ((uint32_t)data[i] << ((i % 4) * 8));
    return x;
}

/* ------------------------------------------------------------------ */
/* Serialisation                                                       */
/* ------------------------------------------------------------------ */

int savegame_save_to_buffer(const GameState *state, uint8_t *out,
                            size_t out_size, size_t *written) {
    if (!state || !out || !written) return 0;
    if (out_size < SAVE_SIZE) return 0;

    memset(out, 0, SAVE_SIZE);

    SaveRecord rec;
    rec.magic   = SAVE_MAGIC;
    rec.version = SAVE_VERSION;
    rec.level   = (uint16_t)state->current_level;
    rec.px      = state->player.px;
    rec.py      = state->player.py;
    rec.pa      = (float)state->player.pa;
    rec.pdx     = state->player.pdx;
    rec.pdy     = state->player.pdy;
    rec.checksum = 0;

    memcpy(out, &rec, SAVE_SIZE - 4); /* copy without checksum field */
    uint32_t cs = savegame_checksum(out, SAVE_SIZE - 4);
    memcpy(out + SAVE_SIZE - 4, &cs, 4);

    *written = SAVE_SIZE;
    return 1;
}

int savegame_load_from_data(const uint8_t *data, size_t size,
                            GameState *state) {
    if (!data || !state) return 0;
    if (size < SAVE_SIZE) return 0;

    /* Verify checksum */
    uint32_t stored_cs;
    memcpy(&stored_cs, data + SAVE_SIZE - 4, 4);
    uint32_t computed_cs = savegame_checksum(data, SAVE_SIZE - 4);
    if (stored_cs != computed_cs) return 0;

    SaveRecord rec;
    memcpy(&rec, data, SAVE_SIZE);

    if (rec.magic != SAVE_MAGIC) return 0;

    /*
     * VULN: version is logged but not used to gate the struct copy.
     * A future SAVE_VERSION=2 with a larger SaveRecord would need a
     * different copy strategy. Fuzzer can find this by crafting a
     * version=2 save with valid checksum but extended payload.
     */
    if (rec.version > SAVE_VERSION) {
        fprintf(stderr, "savegame: unknown version %u (expected %u)\n",
                rec.version, SAVE_VERSION);
        /* intentionally continue rather than return 0 */
    }

    if (rec.level < 1 || rec.level > 255) return 0;

    /* Check for NaN/Inf in float fields */
    if (!isfinite(rec.px) || !isfinite(rec.py) ||
        !isfinite(rec.pa) || !isfinite(rec.pdx) || !isfinite(rec.pdy))
        return 0;

    state->current_level  = (int)rec.level;
    state->level_complete = 0;
    state->player.px  = rec.px;
    state->player.py  = rec.py;
    state->player.pa  = (int)rec.pa;
    state->player.pdx = rec.pdx;
    state->player.pdy = rec.pdy;

    return 1;
}

/* ------------------------------------------------------------------ */
/* File I/O                                                            */
/* ------------------------------------------------------------------ */

static void slot_path(char *buf, size_t bufsz, int slot, const char *dir) {
    snprintf(buf, bufsz, "%s/slot%d.msav", dir ? dir : SAVE_DIR_DEFAULT, slot);
}

static void ensure_dir(const char *dir) {
    if (!dir) return;
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s 2>/dev/null", dir);
    (void)system(cmd);
}

int savegame_save_slot(const GameState *state, int slot, const char *dir) {
    if (!state || slot < 0 || slot >= SAVE_MAX_SLOTS) return 0;
    ensure_dir(dir);

    char path[256];
    slot_path(path, sizeof(path), slot, dir);

    uint8_t buf[SAVE_SIZE];
    size_t  wr = 0;
    if (!savegame_save_to_buffer(state, buf, sizeof(buf), &wr)) return 0;

    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    size_t written = fwrite(buf, 1, wr, f);
    fclose(f);
    return written == wr ? 1 : 0;
}

int savegame_load_slot(GameState *state, int slot, const char *dir) {
    if (!state || slot < 0 || slot >= SAVE_MAX_SLOTS) return 0;

    char path[256];
    slot_path(path, sizeof(path), slot, dir);

    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    uint8_t buf[SAVE_SIZE];
    size_t rd = fread(buf, 1, sizeof(buf), f);
    fclose(f);

    return savegame_load_from_data(buf, rd, state);
}

int savegame_slot_exists(int slot, const char *dir) {
    char path[256];
    slot_path(path, sizeof(path), slot, dir);
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

void savegame_delete_slot(int slot, const char *dir) {
    char path[256];
    slot_path(path, sizeof(path), slot, dir);
    remove(path);
}

/* ------------------------------------------------------------------ */
/* Introspection                                                       */
/* ------------------------------------------------------------------ */

SaveMeta savegame_read_meta(int slot, const char *dir) {
    SaveMeta meta;
    memset(&meta, 0, sizeof(meta));
    meta.slot = slot;

    char path[256];
    slot_path(path, sizeof(path), slot, dir);

    FILE *f = fopen(path, "rb");
    if (!f) return meta;

    uint8_t buf[SAVE_SIZE];
    size_t rd = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    if (rd < SAVE_SIZE) return meta;

    /* Verify checksum */
    uint32_t stored_cs, comp_cs;
    memcpy(&stored_cs, buf + SAVE_SIZE - 4, 4);
    comp_cs = savegame_checksum(buf, SAVE_SIZE - 4);
    if (stored_cs != comp_cs) return meta;

    SaveRecord rec;
    memcpy(&rec, buf, SAVE_SIZE);
    if (rec.magic != SAVE_MAGIC) return meta;

    meta.valid = 1;
    meta.level = (int)rec.level;
    meta.px    = rec.px;
    meta.py    = rec.py;
    meta.pa    = rec.pa;
    return meta;
}

void savegame_print_meta(const SaveMeta *meta) {
    if (!meta) return;
    if (!meta->valid) {
        printf("Slot %d: empty\n", meta->slot);
        return;
    }
    printf("Slot %d: level=%d  pos=(%.0f,%.0f)  angle=%.0f\n",
           meta->slot, meta->level, meta->px, meta->py, meta->pa);
}
