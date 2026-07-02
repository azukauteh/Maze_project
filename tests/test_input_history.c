#include "input_history.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int count_lines(const char *path) {
    FILE *fp = fopen(path, "r");
    int lines = 0;
    int ch;

    if (fp == NULL) {
        return -1;
    }
    while ((ch = fgetc(fp)) != EOF) {
        if (ch == '\n') {
            lines++;
        }
    }
    fclose(fp);
    return lines;
}

static void test_push_get_clear(void) {
    InputHistory hist;
    const InputRecord *record;

    input_history_clear(&hist);
    input_history_push(&hist, EVENT_MOVE_FORWARD, 10);
    input_history_push(&hist, EVENT_TURN_LEFT, 20);
    input_history_push(&hist, EVENT_SAVE, 30);
    assert(hist.count == 3);
    record = input_history_get(&hist, 0);
    assert(record != NULL);
    assert(record->event == EVENT_SAVE);
    assert(record->timestamp_ms == 30);
    input_history_clear(&hist);
    assert(hist.count == 0);
    assert(input_history_get(&hist, 0) == NULL);
}

static void test_wrap(void) {
    InputHistory hist;
    const InputRecord *record;
    int i;

    input_history_clear(&hist);
    for (i = 0; i < INPUT_HISTORY_CAPACITY + 9; ++i) {
        input_history_push(&hist, EVENT_MOVE_FORWARD, (uint64_t)i);
    }
    assert(hist.count == INPUT_HISTORY_CAPACITY);
    record = input_history_get(&hist, 0);
    assert(record != NULL);
    assert(record->timestamp_ms == INPUT_HISTORY_CAPACITY + 8);
    record = input_history_get(&hist, INPUT_HISTORY_CAPACITY - 1);
    assert(record != NULL);
    assert(record->timestamp_ms == 9);
}

static void test_dump_export(void) {
    InputHistory hist;
    const char *path = "input_history_test.csv";
    int i;

    input_history_clear(&hist);
    input_history_dump(&hist);
    for (i = 0; i < 7; ++i) {
        input_history_push(&hist, EVENT_MOVE_BACK, 1000u + (uint64_t)i);
    }
    input_history_dump(&hist);
    assert(input_history_export_csv(&hist, path));
    assert(count_lines(path) == hist.count + 1);
    remove(path);
}

static void test_null_safety(void) {
    input_history_clear(NULL);
    input_history_push(NULL, EVENT_SAVE, 1);
    assert(input_history_get(NULL, 0) == NULL);
    input_history_dump(NULL);
    assert(!input_history_export_csv(NULL, "x.csv"));
    assert(!input_history_export_csv(NULL, NULL));
}

int main(void) {
    test_push_get_clear();
    test_wrap();
    test_dump_export();
    test_null_safety();
    printf("test_input_history passed\n");
    return 0;
}

/* Input history test notes 001. CSV line count includes the header row. */
