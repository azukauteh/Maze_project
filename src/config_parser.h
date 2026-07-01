#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Simple INI-style config parser.
 *
 * Supported syntax:
 *   ; comment
 *   # comment
 *   [section]
 *   key = value
 *   key=value
 *
 * Sections are optional. Keys without a section belong to section "".
 * Values are trimmed of leading/trailing whitespace.
 * Max key length:     63 chars
 * Max value length:  255 chars
 * Max section length: 63 chars
 * Max entries:       128
 */

#define CFG_MAX_KEY     64
#define CFG_MAX_VALUE  256
#define CFG_MAX_SECTION 64
#define CFG_MAX_ENTRIES 128

typedef struct {
    char section[CFG_MAX_SECTION];
    char key    [CFG_MAX_KEY];
    char value  [CFG_MAX_VALUE];
} ConfigEntry;

typedef struct {
    ConfigEntry entries[CFG_MAX_ENTRIES];
    int         count;
} Config;

/*
 * config_parse — parse INI text from data/size into cfg.
 * Returns number of entries parsed, or -1 on fatal error.
 *
 * VULN NOTE: key and section are read into fixed 64-byte stack buffers
 * inside the parser using a manual character loop with no length check —
 * intentional stack-buffer-overflow target for fuzzer discovery.
 */
int config_parse(const char *data, size_t size, Config *cfg);

/*
 * config_get — look up key under section. Returns value pointer or NULL.
 * Pass section="" for top-level keys.
 */
const char *config_get(const Config *cfg, const char *section,
                        const char *key);

/*
 * config_get_int — like config_get but parses value as int.
 * Returns def if key not found or value is not a valid integer.
 */
int config_get_int(const Config *cfg, const char *section,
                   const char *key, int def);

/*
 * config_get_float — like config_get but parses value as float.
 */
float config_get_float(const Config *cfg, const char *section,
                       const char *key, float def);

/*
 * config_dump — print all entries to stdout (for debugging).
 */
void config_dump(const Config *cfg);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_PARSER_H */
