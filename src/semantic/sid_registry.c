#include "semantic/sid_registry.h"

#include <stdlib.h>
#include <string.h>

#include "utils/xalloc.h"

enum { INITIAL_CAPACITY = 1024 };   /* power of two */

void sid_registry_init(SidRegistry *reg)
{
    reg->entries = NULL;
    reg->count = 0;
    reg->capacity = 0;
}

void sid_registry_free(SidRegistry *reg)
{
    free(reg->entries);
    sid_registry_init(reg);
}

static size_t slot_for(const SidRegistry *reg, long sid)
{
    /* Fibonacci-style multiplicative hash; capacity is a power of two. */
    size_t h = (size_t)sid * 2654435761u;
    return h & (reg->capacity - 1);
}

static SidEntry *probe(const SidRegistry *reg, long sid)
{
    size_t i = slot_for(reg, sid);
    while (reg->entries[i].used) {
        if (reg->entries[i].sid == sid) {
            return &reg->entries[i];
        }
        i = (i + 1) & (reg->capacity - 1);
    }
    return &reg->entries[i];   /* first empty slot */
}

static void grow(SidRegistry *reg)
{
    SidRegistry old = *reg;
    reg->capacity = old.capacity ? old.capacity * 2 : INITIAL_CAPACITY;
    reg->entries = xcalloc(reg->capacity, sizeof(SidEntry));
    for (size_t i = 0; i < old.capacity; i++) {
        if (old.entries[i].used) {
            *probe(reg, old.entries[i].sid) = old.entries[i];
        }
    }
    free(old.entries);
}

const SidEntry *sid_registry_find(const SidRegistry *reg, long sid)
{
    if (reg->capacity == 0) {
        return NULL;
    }
    SidEntry *e = probe(reg, sid);
    return e->used ? e : NULL;
}

void sid_registry_insert(SidRegistry *reg, long sid, int rule_number, int line)
{
    /* grow at 70% load to keep probe chains short */
    if (reg->capacity == 0 || reg->count * 10 >= reg->capacity * 7) {
        grow(reg);
    }
    SidEntry *e = probe(reg, sid);
    if (!e->used) {
        e->sid = sid;
        e->rule_number = rule_number;
        e->line = line;
        e->used = 1;
        reg->count++;
    }
}
