#ifndef MODELS_SOURCE_LOCATION_H
#define MODELS_SOURCE_LOCATION_H

/* Source span of a token or construct. Doubles as Bison's YYLTYPE via
 * `%define api.location.type {SrcSpan}` — the first_/last_ field names are
 * deliberately Bison-conventional so the default location propagation works
 * unchanged when the real grammar lands (Phase 3).
 *
 * rule_number is this project's extra channel: the ordinal of the non-blank,
 * non-comment line the token belongs to. Diagnostics report it alongside the
 * physical line number. */
typedef struct SrcSpan {
    int first_line;
    int first_column;
    int last_line;
    int last_column;
    int rule_number;
} SrcSpan;

#endif /* MODELS_SOURCE_LOCATION_H */
