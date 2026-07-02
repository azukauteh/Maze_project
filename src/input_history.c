/*
 * input_history.c
 *
 * Fixed-size replay/undo scaffold for recent player moves.
 *
 * This module does:
 * - Store the last 64 input records in newest-addressable order.
 * - Provide debug dumping to stdout.
 * - Export the retained records to a small CSV file.
 *
 * This module does NOT:
 * - Allocate memory.
 * - Replay moves into the engine.
 * - Own timestamps or call a clock.
 * - Store SDL events directly.
 */

#include "input_history.h"

#include <stdio.h>

static const char *input_history_event_name(GameEvent event) {
    switch (event) {
        case EVENT_NONE: return "NONE";
        case EVENT_MOVE_FORWARD: return "MOVE_FORWARD";
        case EVENT_MOVE_BACK: return "MOVE_BACK";
        case EVENT_TURN_LEFT: return "TURN_LEFT";
        case EVENT_TURN_RIGHT: return "TURN_RIGHT";
        case EVENT_QUIT: return "QUIT";
        case EVENT_PAUSE: return "PAUSE";
        case EVENT_SAVE: return "SAVE";
        case EVENT_LOAD: return "LOAD";
        case EVENT_DEBUG_TOGGLE: return "DEBUG_TOGGLE";
        default: return "UNKNOWN";
    }
}

void input_history_clear(InputHistory *hist) {
    int i;

    if (hist == NULL) {
        return;
    }

    hist->head = 0;
    hist->count = 0;
    for (i = 0; i < INPUT_HISTORY_CAPACITY; ++i) {
        hist->records[i].event = EVENT_NONE;
        hist->records[i].timestamp_ms = 0;
    }
}

void input_history_push(InputHistory *hist, GameEvent event, uint64_t timestamp_ms) {
    if (hist == NULL) {
        return;
    }

    hist->records[hist->head].event = event;
    hist->records[hist->head].timestamp_ms = timestamp_ms;
    hist->head = (hist->head + 1) % INPUT_HISTORY_CAPACITY;
    if (hist->count < INPUT_HISTORY_CAPACITY) {
        hist->count++;
    }
}

const InputRecord *input_history_get(const InputHistory *hist, int index) {
    int slot;

    if (hist == NULL || index < 0 || index >= hist->count) {
        return NULL;
    }

    slot = hist->head - 1 - index;
    while (slot < 0) {
        slot += INPUT_HISTORY_CAPACITY;
    }
    return &hist->records[slot % INPUT_HISTORY_CAPACITY];
}

void input_history_dump(const InputHistory *hist) {
    int i;

    if (hist == NULL) {
        return;
    }

    printf("input_history count=%d\n", hist->count);
    for (i = 0; i < hist->count; ++i) {
        const InputRecord *record = input_history_get(hist, i);
        if (record != NULL) {
            printf("%02d %s %llu\n", i,
                   input_history_event_name(record->event),
                   (unsigned long long)record->timestamp_ms);
        }
    }
}

int input_history_export_csv(const InputHistory *hist, const char *path) {
    FILE *fp;
    int i;

    if (hist == NULL || path == NULL) {
        return 0;
    }

    fp = fopen(path, "w");
    if (fp == NULL) {
        return 0;
    }

    fprintf(fp, "event,timestamp_ms\n");
    for (i = hist->count - 1; i >= 0; --i) {
        const InputRecord *record = input_history_get(hist, i);
        if (record != NULL) {
            fprintf(fp, "%s,%llu\n",
                    input_history_event_name(record->event),
                    (unsigned long long)record->timestamp_ms);
        }
    }

    fclose(fp);
    return 1;
}

/*
 * Input history maintenance notes:
 * 001. The newest record is always at input_history_get(hist, 0).
 * 002. The oldest retained record is input_history_get(hist, count - 1).
 * 003. Export writes oldest-to-newest for replay tooling.
 * 004. Dump writes newest-to-oldest for live debugging.
 * 005. The history stores GameEvent, not raw keys, to survive rebinding.
 * 006. The caller owns timestamp precision and source.
 * 007. Full history overwrites the oldest record only.
 * 008. Clear scrubs all retained slots for predictable tests.
 * 009. NULL inputs are ignored rather than asserted.
 * 010. CSV output intentionally avoids quoting because names are fixed tokens.
 */
