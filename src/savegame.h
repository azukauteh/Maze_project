#ifndef SAVEGAME_H
#define SAVEGAME_H

#include <stdint.h>
#include <stddef.h>
#include "engine.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Savegame subsystem — serialize/deserialize GameState to a binary slot.
 *
 * Binary save format (little-endian):
 *   [0..3]   magic      uint32_t  0x4D415A45 ('MAZE')
 *   [4..5]   version    uint16_t  current: 1
 *   [6..7]   level      uint16_t
 *   [8..11]  px         float
 *   [12..15] py         float
 *   [16..19] pa         float
 *   [20..23] pdx        float
 *   [24..27] pdy        float
 *   [28..31] checksum   uint32_t  XOR of bytes [0..27]
 *
 * VULN NOTE:
 *   savegame_load_from_data() uses memcpy to copy the player struct
 *   from the save buffer. The version field is read but not gated —
 *   a future version with a larger player struct would overflow the
 *   fixed-size Player allocation. This is an intentional forward-
 *   compatibility OOB for fuzzer discovery. (Marked VULN below.)
 */

#define SAVE_MAGIC    0x454A414Du  /* 'MAZE' little-endian */
#define SAVE_VERSION  1u
#define SAVE_SIZE     32u          /* total bytes per save slot */
#define SAVE_MAX_SLOTS 4

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t level;
    float    px, py, pa;
    float    pdx, pdy;
    uint32_t checksum;
} __attribute__((packed)) SaveRecord;

typedef struct {
    int   valid;
    int   slot;
    int   level;
    float px, py, pa;
} SaveMeta;

/* ---- Serialisation ---- */
int  savegame_save_to_buffer  (const GameState *state, uint8_t *out,
                               size_t out_size, size_t *written);
int  savegame_load_from_data  (const uint8_t *data, size_t size,
                               GameState *state);

/* ---- File I/O ---- */
int  savegame_save_slot(const GameState *state, int slot,
                        const char *dir);
int  savegame_load_slot(GameState *state, int slot,
                        const char *dir);
int  savegame_slot_exists(int slot, const char *dir);
void savegame_delete_slot(int slot, const char *dir);

/* ---- Introspection ---- */
SaveMeta savegame_read_meta(int slot, const char *dir);
void     savegame_print_meta(const SaveMeta *meta);

/* ---- Checksum ---- */
uint32_t savegame_checksum(const uint8_t *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* SAVEGAME_H */
