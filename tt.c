#include "tt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t TTSizeMB = 8;

static TTEntry *table = NULL;
static size_t table_size = 0;

int initializeTT(size_t entries)
{
    if (entries == 0)
        return 0;

    if (entries > SIZE_MAX / sizeof(TTEntry))
        return 0;

    TTEntry *new_table = calloc(entries, sizeof(TTEntry));
    if (new_table == NULL) {
        fprintf(stderr, "TT allocation failed\n");
        return 0;
    }

    free(table);
    table = new_table;
    table_size = entries;

    return 1;
}

int initializeTTFromMB(size_t mb)
{
    if (mb == 0)
        return 0;

    if (mb > SIZE_MAX / 1024 / 1024)
        return 0;

    size_t bytes = mb * 1024 * 1024;
    size_t entries = bytes / sizeof(TTEntry);

    return initializeTT(entries);
}

int resizeTTFromMB(size_t mb)
{
    freeTT();
    return initializeTTFromMB(mb);
}

int setTTSizeMB(size_t mb)
{
    if (mb == 0)
        return 0;

    TTSizeMB = mb;

    return resizeTTFromMB(TTSizeMB);
}

void storeTTEntry(uint64_t hash,
                  int16_t score,
                  uint16_t move,
                  uint16_t depth,
                  TTFlag flag)
{
    if (table == NULL || table_size == 0)
        return;

    size_t index = hash % table_size;

    table[index].hash = hash;
    table[index].score = score;
    table[index].move = move;
    table[index].depth = depth;
    table[index].flag = (uint8_t)flag;
}

TTEntry *probeTT(uint64_t hash)
{
    if (table == NULL || table_size == 0)
        return NULL;

    size_t index = hash % table_size;

    if (table[index].hash == hash && table[index].flag != TT_NONE)
        return &table[index];

    return NULL;
}

void clearTT(void)
{
    if (table != NULL)
        memset(table, 0, table_size * sizeof(TTEntry));
}

void freeTT(void)
{
    free(table);
    table = NULL;
    table_size = 0;
}

size_t getTTSize(void)
{
    return table_size;
}

size_t getTTSizeMB(void)
{
    return (table_size * sizeof(TTEntry)) / (1024 * 1024);
}