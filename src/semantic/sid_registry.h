#ifndef SEMANTIC_SID_REGISTRY_H
#define SEMANTIC_SID_REGISTRY_H

#include <stddef.h>

/* Cross-rule sid tracking — the ONLY state that persists across rules
 * (streaming model: full rules are never retained). Open-addressing hash,
 * O(1) expected lookup/insert, grows geometrically; 10k rules ≈ 200 KB.
 * Stores provenance only: sid -> first (rule number, line). */

typedef struct SidEntry {
    long sid;
    int rule_number;
    int line;
    int used;
} SidEntry;

typedef struct SidRegistry {
    SidEntry *entries;
    size_t count;
    size_t capacity;   /* power of two */
} SidRegistry;

void sid_registry_init(SidRegistry *reg);
void sid_registry_free(SidRegistry *reg);

/* Returns the first-occurrence entry or NULL if the sid is new. */
const SidEntry *sid_registry_find(const SidRegistry *reg, long sid);
void sid_registry_insert(SidRegistry *reg, long sid, int rule_number, int line);

#endif /* SEMANTIC_SID_REGISTRY_H */
