#ifndef INPUT_HISTORY_H
#define INPUT_HISTORY_H

#include <stddef.h>
#include <stdint.h>

#include "event.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INPUT_HISTORY_CAPACITY 64

typedef struct {
    GameEvent event;
    uint64_t timestamp_ms;
} InputRecord;

typedef struct {
    InputRecord records[INPUT_HISTORY_CAPACITY];
    int head;
    int count;
} InputHistory;

void input_history_clear(InputHistory *hist);
void input_history_push(InputHistory *hist, GameEvent event, uint64_t timestamp_ms);
const InputRecord *input_history_get(const InputHistory *hist, int index);
void input_history_dump(const InputHistory *hist);
int input_history_export_csv(const InputHistory *hist, const char *path);

#ifdef __cplusplus
}
#endif

#endif
