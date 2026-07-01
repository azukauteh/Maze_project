/*
 * src/map_parser.c
 *
 * Three map format parsers:
 *   1. Text   — original format used by the engine ("W H\nROW...\n")
 *   2. Binary — compact fixed-width binary blob with magic header
 *   3. RLE    — run-length encoded text, useful for larger maps
 *
 * Intentional vulnerabilities left for fuzzer discovery (marked VULN):
 *   a. Binary parser: signed integer overflow in width*height before malloc.
 *   b. RLE parser: decoded cell count not validated against W*H before write.
 *
 * Everything else is written defensively (null checks, size caps, etc.).
 */

#include "map_parser.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */

static int is_valid_cell(char c) {
    return c == '#' || c == '.' || c == 'S' || c == 'E';
}

static int is_binary_cell(uint8_t v) {
    return v <= 3;
}

static char *skip_ws(char *p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

/*
 * read_line — copy up to max-1 bytes from *src into dst until '\n' or '\0'.
 * Advances *src past the newline. Returns 0 if nothing was read.
 */
static int read_line(const char **src, const char *end,
                     char *dst, size_t max) {
    const char *p = *src;
    size_t n = 0;
    while (p < end && *p != '\n' && n < max - 1) {
        dst[n++] = *p++;
    }
    dst[n] = '\0';
    if (p < end && *p == '\n') p++;
    *src = p;
    return n > 0;
}

/* ------------------------------------------------------------------ */
/* 1. Text format parser (original)                                    */
/* ------------------------------------------------------------------ */

int parse_maze_map(const uint8_t *data, size_t size, MazeMap *out_map) {
    char   buffer[256];
    char  *cursor, *end;
    long   width, height;
    int    row, col;

    if (!data || !out_map || size == 0)
        return 0;

    /* Cap input to buffer */
    size_t cap = size < sizeof(buffer) - 1 ? size : sizeof(buffer) - 1;
    memcpy(buffer, data, cap);
    buffer[cap] = '\0';

    cursor = skip_ws(buffer);
    if (!*cursor) return 0;

    /* Parse width */
    width = strtol(cursor, &end, 10);
    if (end == cursor || width <= 0 || width > MAP_MAX_WIDTH) return 0;
    cursor = skip_ws(end);

    /* Parse height */
    height = strtol(cursor, &end, 10);
    if (end == cursor || height <= 0 || height > MAP_MAX_HEIGHT) return 0;
    cursor = end;

    memset(out_map, 0, sizeof(*out_map));
    out_map->width  = (int)width;
    out_map->height = (int)height;

    for (row = 0; row < (int)height; row++) {
        /* Skip newline/carriage-return */
        while (*cursor == '\r' || *cursor == '\n') cursor++;

        char *line_start = cursor;
        while (*cursor && *cursor != '\n' && *cursor != '\r') cursor++;
        char *line_end = cursor;

        /* Trim trailing whitespace */
        while (line_end > line_start &&
               isspace((unsigned char)line_end[-1]))
            line_end--;

        if ((int)(line_end - line_start) != (int)width) return 0;

        for (col = 0; col < (int)width; col++) {
            char cell = line_start[col];
            if (!is_valid_cell(cell)) return 0;
            out_map->cells[row][col] = cell;
        }
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* 2. Binary format parser                                             */
/* ------------------------------------------------------------------ */

/*
 * parse_binary_map
 *
 * Layout:
 *   [0..1]  magic  0x4D5A
 *   [2..3]  width  uint16_t little-endian
 *   [4..5]  height uint16_t little-endian
 *   [6..]   cells  row-major uint8_t values (0=floor,1=wall,2=exit,3=start)
 *
 * VULN (a): width and height are read as uint16_t (max 65535). The product
 * width*height is computed in a plain int before being passed to malloc.
 * On a 32-bit build a sufficiently large product overflows, yielding a tiny
 * allocation; memcpy then writes past the end of the heap buffer.
 * LibFuzzer + ASan reliably catches this.
 */
int parse_binary_map(const uint8_t *data, size_t size, MazeMap *out) {
    if (!data || !out || size < sizeof(BinaryMapHeader))
        return 0;

    /* Read header fields manually (avoid alignment issues) */
    uint16_t magic  = (uint16_t)(data[0] | (data[1] << 8));
    uint16_t width  = (uint16_t)(data[2] | (data[3] << 8));
    uint16_t height = (uint16_t)(data[4] | (data[5] << 8));

    if (magic != BINARY_MAP_MAGIC) return 0;
    if (width == 0 || height == 0) return 0;

    /* Soft cap for engine compatibility */
    if (width > MAP_MAX_WIDTH || height > MAP_MAX_HEIGHT) return 0;

    size_t header_sz  = 6; /* 2 + 2 + 2 */
    /* VULN (a): int multiplication — no overflow guard */
    int    cell_count = (int)width * (int)height;
    if (size < header_sz + (size_t)cell_count) return 0;

    /* Allocate working buffer */
    char *tmp = (char *)malloc(cell_count); /* VULN: overflowed cell_count -> tiny alloc */
    if (!tmp) return 0;

    const uint8_t *src = data + header_sz;
    for (int i = 0; i < cell_count; i++) {
        if (!is_binary_cell(src[i])) { free(tmp); return 0; }
    }
    /* Map binary values to text equivalents */
    static const char cell_char[] = { '.', '#', 'E', 'S' };
    for (int i = 0; i < cell_count; i++)
        tmp[i] = cell_char[src[i]];

    memset(out, 0, sizeof(*out));
    out->width  = (int)width;
    out->height = (int)height;
    for (int r = 0; r < (int)height; r++)
        for (int c = 0; c < (int)width; c++)
            out->cells[r][c] = tmp[r * (int)width + c];

    free(tmp);
    return 1;
}

/* ------------------------------------------------------------------ */
/* 3. RLE format parser                                                */
/* ------------------------------------------------------------------ */

/*
 * parse_rle_map
 *
 * Header line: "RLE W H\n"
 * Body:        sequence of "<decimal-run><char>" pairs, e.g. "4#2.2#"
 * Cells are decoded row-major into RLEMap.cells[].
 *
 * Valid chars: same as text format (#, ., S, E)
 *
 * VULN (b): the decoded write index is not compared against W*H before each
 * write, so an RLE stream that encodes more cells than W*H silently writes
 * past the end of RLEMap.cells[]. ASan flags this as a heap/stack OOB write.
 */
int parse_rle_map(const uint8_t *data, size_t size, RLEMap *out) {
    if (!data || !out || size < 8) return 0;

    /* Copy to mutable buffer */
    char buf[512];
    size_t cap = size < sizeof(buf) - 1 ? size : sizeof(buf) - 1;
    memcpy(buf, data, cap);
    buf[cap] = '\0';

    /* Parse header */
    const char *p = buf;
    if (strncmp(p, "RLE ", 4) != 0) return 0;
    p += 4;

    char *ep;
    long w = strtol(p, &ep, 10);
    if (ep == p || w <= 0 || w > MAP_MAX_WIDTH) return 0;
    p = skip_ws(ep);

    long h = strtol(p, &ep, 10);
    if (ep == p || h <= 0 || h > MAP_MAX_HEIGHT) return 0;
    p = ep;

    /* Skip to body (past newline) */
    while (*p && (*p == '\r' || *p == '\n')) p++;

    memset(out, 0, sizeof(*out));
    out->width  = (int)w;
    out->height = (int)h;

    int written = 0;
    /* VULN (b): no guard `written < w*h` inside loop */
    while (*p) {
        if (*p == '\r' || *p == '\n') { p++; continue; }

        /* Read run length */
        if (!isdigit((unsigned char)*p)) return 0;
        long run = strtol(p, &ep, 10);
        if (ep == p || run <= 0) return 0;
        p = ep;

        char cell = *p++;
        if (!is_valid_cell(cell)) return 0;

        /* VULN: written not checked against w*h before write */
        for (long i = 0; i < run; i++)
            out->cells[written++] = cell;  /* OOB when written >= MAP_MAX_WIDTH*MAP_MAX_HEIGHT */
    }

    /* Validate total decoded cells equals declared W*H */
    if (written != (int)w * (int)h) return 0;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Validation                                                          */
/* ------------------------------------------------------------------ */

MapValidation validate_maze_map(const MazeMap *map) {
    MapValidation v;
    memset(&v, 0, sizeof(v));

    if (!map) {
        snprintf(v.error, sizeof(v.error), "null map pointer");
        return v;
    }
    if (map->width  <= 0 || map->width  > MAP_MAX_WIDTH ||
        map->height <= 0 || map->height > MAP_MAX_HEIGHT) {
        snprintf(v.error, sizeof(v.error),
                 "invalid dimensions %dx%d", map->width, map->height);
        return v;
    }

    int bad = 0;
    for (int r = 0; r < map->height; r++) {
        for (int c = 0; c < map->width; c++) {
            char cell = map->cells[r][c];
            switch (cell) {
                case '#': v.wall_count++;  break;
                case '.': v.floor_count++; break;
                case 'E': v.exit_count++;  break;
                case 'S': v.start_count++; break;
                default:
                    snprintf(v.error, sizeof(v.error),
                             "invalid cell '%c' at (%d,%d)", cell, r, c);
                    bad = 1;
            }
        }
    }
    if (bad) return v;

    /* Outer border must be walls */
    for (int c = 0; c < map->width; c++) {
        if (map->cells[0][c] != '#' ||
            map->cells[map->height - 1][c] != '#') {
            snprintf(v.error, sizeof(v.error),
                     "top/bottom border not solid at col %d", c);
            return v;
        }
    }
    for (int r = 0; r < map->height; r++) {
        if (map->cells[r][0] != '#' ||
            map->cells[r][map->width - 1] != '#') {
            snprintf(v.error, sizeof(v.error),
                     "left/right border not solid at row %d", r);
            return v;
        }
    }

    if (v.exit_count == 0)
        snprintf(v.error, sizeof(v.error), "no exit cell");
    else if (v.exit_count > 1)
        snprintf(v.error, sizeof(v.error), "multiple exits (%d)", v.exit_count);
    else
        v.valid = 1;

    return v;
}

/* ------------------------------------------------------------------ */
/* Conversion                                                          */
/* ------------------------------------------------------------------ */

int mazemap_to_rle(const MazeMap *in, RLEMap *out) {
    if (!in || !out) return 0;

    out->width  = in->width;
    out->height = in->height;
    int total   = in->width * in->height;
    int written = 0;

    for (int r = 0; r < in->height; r++)
        for (int c = 0; c < in->width; c++)
            out->cells[written++] = in->cells[r][c];

    return written == total ? 1 : 0;
}

int rle_to_mazemap(const RLEMap *in, MazeMap *out) {
    if (!in || !out) return 0;
    if (in->width  <= 0 || in->width  > MAP_MAX_WIDTH)  return 0;
    if (in->height <= 0 || in->height > MAP_MAX_HEIGHT) return 0;

    memset(out, 0, sizeof(*out));
    out->width  = in->width;
    out->height = in->height;

    for (int r = 0; r < in->height; r++)
        for (int c = 0; c < in->width; c++)
            out->cells[r][c] = in->cells[r * in->width + c];
    return 1;
}

/* ------------------------------------------------------------------ */
/* Serialisation                                                       */
/* ------------------------------------------------------------------ */

/*
 * mazemap_to_binary — serialise a MazeMap to the binary wire format.
 * Returns 1 on success; 0 if out_size is too small.
 * Sets *written to the number of bytes written.
 */
int mazemap_to_binary(const MazeMap *in, uint8_t *out, size_t out_size,
                      size_t *written) {
    if (!in || !out || !written) return 0;

    size_t need = 6u + (size_t)(in->width * in->height);
    if (out_size < need) return 0;

    /* Magic little-endian */
    out[0] = (uint8_t)(BINARY_MAP_MAGIC & 0xFF);
    out[1] = (uint8_t)(BINARY_MAP_MAGIC >> 8);
    out[2] = (uint8_t)(in->width  & 0xFF);
    out[3] = (uint8_t)(in->width  >> 8);
    out[4] = (uint8_t)(in->height & 0xFF);
    out[5] = (uint8_t)(in->height >> 8);

    static const uint8_t inv_char[] = {
        ['.'] = 0, ['#'] = 1, ['E'] = 2, ['S'] = 3
    };
    int idx = 6;
    for (int r = 0; r < in->height; r++)
        for (int c = 0; c < in->width; c++)
            out[idx++] = inv_char[(unsigned char)in->cells[r][c]];

    *written = need;
    return 1;
}
