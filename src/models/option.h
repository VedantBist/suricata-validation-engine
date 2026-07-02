#ifndef MODELS_OPTION_H
#define MODELS_OPTION_H

#include <stddef.h>

#include "models/source_location.h"

/* One parsed `key:value;` option — structure only. Whether "sid" wants a
 * number or "rev" makes sense twice is semantic territory (Phase 5+); the
 * parser stores what it saw. */

typedef enum OptionValueKind {
    OPTVAL_STRING,
    OPTVAL_NUMBER,
    OPTVAL_IDENT
} OptionValueKind;

/* Transient carrier for the option_value nonterminal while the enclosing
 * option reduces (same pattern as Endpoint/PortSpec in the header). */
typedef struct OptionValue {
    OptionValueKind kind;
    char *text;
} OptionValue;

typedef struct Option {
    char *key;                  /* heap-owned */
    OptionValueKind value_kind;
    char *value;                /* heap-owned; string values are unquoted */
    SrcSpan span;
} Option;

typedef struct OptionList {
    Option **items;
    size_t count;
    size_t capacity;
} OptionList;

/* Ownership chain (one direction, no sharing):
 *   lexeme -> Option (option_create takes ownership of key and value)
 *   Option -> OptionList (option_list_append takes ownership of the Option)
 *   OptionList -> Rule (grammar attaches the list; rule_free releases it)
 * Every level frees exactly what it owns; panic-recovery destructors call
 * the same free functions on whichever level was on the parser stack. */

Option *option_create(char *key, OptionValueKind kind, char *value, SrcSpan span);
void option_free(Option *opt);

OptionList *option_list_create(void);
void option_list_append(OptionList *list, Option *opt);
void option_list_free(OptionList *list);

#endif /* MODELS_OPTION_H */
