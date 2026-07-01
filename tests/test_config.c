/*
 * tests/test_config.c — unit tests for config_parser.
 * Run: ./tests/test_config
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "../src/config_parser.h"

static int tests_run = 0, tests_passed = 0;
#define CHECK(cond, msg) do { \
    tests_run++; \
    if (cond) { tests_passed++; printf("  PASS  %s\n", msg); } \
    else       printf("  FAIL  %s  (line %d)\n", msg, __LINE__); \
} while(0)

static void test_basic(void) {
    printf("\n[config] Basic parsing\n");
    Config cfg;

    const char *ini = "key1 = value1\nkey2=value2\n";
    CHECK(config_parse(ini, strlen(ini), &cfg) == 2, "2 top-level keys");
    CHECK(strcmp(config_get(&cfg, "", "key1"), "value1") == 0, "key1 value");
    CHECK(strcmp(config_get(&cfg, "", "key2"), "value2") == 0, "key2 value");
}

static void test_sections(void) {
    printf("\n[config] Sections\n");
    Config cfg;

    const char *ini =
        "[meta]\n"
        "name = Test Level\n"
        "version = 2\n"
        "[spawn]\n"
        "x = 100\n"
        "y = 200\n"
        "angle = 45\n";
    int n = config_parse(ini, strlen(ini), &cfg);
    CHECK(n == 5, "5 entries across 2 sections");
    CHECK(strcmp(config_get(&cfg, "meta",  "name"),    "Test Level") == 0, "name");
    CHECK(strcmp(config_get(&cfg, "spawn", "x"),       "100")        == 0, "x");
    CHECK(strcmp(config_get(&cfg, "spawn", "y"),       "200")        == 0, "y");
    CHECK(strcmp(config_get(&cfg, "spawn", "angle"),   "45")         == 0, "angle");
}

static void test_typed_getters(void) {
    printf("\n[config] Typed getters\n");
    Config cfg;

    const char *ini =
        "[game]\n"
        "speed = 5\n"
        "scale = 1.5\n"
        "notanumber = abc\n";
    config_parse(ini, strlen(ini), &cfg);

    CHECK(config_get_int  (&cfg, "game", "speed",       0)   == 5,    "int 5");
    CHECK(config_get_int  (&cfg, "game", "notanumber", -1)   == -1,   "bad int = def");
    CHECK(config_get_int  (&cfg, "game", "missing",    99)   == 99,   "missing int = def");

    float f = config_get_float(&cfg, "game", "scale", 0.0f);
    CHECK(fabsf(f - 1.5f) < 0.001f, "float 1.5");
    float fdef = config_get_float(&cfg, "game", "missing", 3.14f);
    CHECK(fabsf(fdef - 3.14f) < 0.001f, "missing float = def");
}

static void test_comments(void) {
    printf("\n[config] Comments and blank lines\n");
    Config cfg;

    const char *ini =
        "; full line comment\n"
        "# hash comment\n"
        "\n"
        "key = val ; inline comment\n"
        "key2 = val2 # inline hash\n";
    int n = config_parse(ini, strlen(ini), &cfg);
    CHECK(n == 2, "2 keys ignoring comments");
    CHECK(strcmp(config_get(&cfg, "", "key"),  "val") == 0,  "inline ; stripped");
    CHECK(strcmp(config_get(&cfg, "", "key2"), "val2") == 0, "inline # stripped");
}

static void test_missing_keys(void) {
    printf("\n[config] Missing keys\n");
    Config cfg;
    config_parse("x=1\n", 4, &cfg);

    CHECK(config_get(&cfg, "",     "y")      == NULL, "missing key = NULL");
    CHECK(config_get(&cfg, "sec",  "x")      == NULL, "wrong section = NULL");
    CHECK(config_get(NULL, "",     "x")      == NULL, "NULL cfg = NULL");
    CHECK(config_get(&cfg, NULL,   "x")      == NULL, "NULL section = NULL");
    CHECK(config_get(&cfg, "",     NULL)     == NULL, "NULL key = NULL");
}

static void test_edge_cases(void) {
    printf("\n[config] Edge cases\n");
    Config cfg;

    CHECK(config_parse(NULL, 10, &cfg) == -1, "NULL data = -1");
    CHECK(config_parse("x=1", 0, &cfg) == -1, "zero size = -1");

    /* Whitespace-only value */
    const char *ini = "key =   \n";
    config_parse(ini, strlen(ini), &cfg);
    const char *v = config_get(&cfg, "", "key");
    CHECK(v != NULL && strlen(v) == 0, "whitespace-only value trimmed to empty");

    /* Duplicate keys — last write wins (config_get returns first match) */
    const char *dup = "key=first\nkey=second\n";
    config_parse(dup, strlen(dup), &cfg);
    CHECK(strcmp(config_get(&cfg, "", "key"), "first") == 0,
          "first occurrence returned on duplicate");

    /* Max entries cap */
    char big[CFG_MAX_ENTRIES * 12 + 64];
    int off = 0;
    for (int i = 0; i < CFG_MAX_ENTRIES + 10; i++)
        off += snprintf(big + off, sizeof(big) - off, "k%d=v%d\n", i, i);
    int n = config_parse(big, (size_t)off, &cfg);
    CHECK(n == CFG_MAX_ENTRIES, "excess entries capped at CFG_MAX_ENTRIES");
}

int main(void) {
    printf("=== config_parser tests ===\n");
    test_basic();
    test_sections();
    test_typed_getters();
    test_comments();
    test_missing_keys();
    test_edge_cases();
    printf("\n%d / %d passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
