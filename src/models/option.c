#include "models/option.h"

#include <stdlib.h>

#include "utils/xalloc.h"

Option *option_create(char *key, OptionValueKind kind, char *value, SrcSpan span)
{
    Option *opt = xcalloc(1, sizeof(Option));
    opt->key = key;
    opt->value_kind = kind;
    opt->value = value;
    opt->span = span;
    return opt;
}

void option_free(Option *opt)
{
    if (opt == NULL) {
        return;
    }
    free(opt->key);
    free(opt->value);
    free(opt);
}

OptionList *option_list_create(void)
{
    return xcalloc(1, sizeof(OptionList));
}

void option_list_append(OptionList *list, Option *opt)
{
    if (list->count == list->capacity) {
        list->capacity = list->capacity ? list->capacity * 2 : 8;
        list->items = xrealloc(list->items, list->capacity * sizeof(Option *));
    }
    list->items[list->count++] = opt;
}

void option_list_free(OptionList *list)
{
    if (list == NULL) {
        return;
    }
    for (size_t i = 0; i < list->count; i++) {
        option_free(list->items[i]);
    }
    free(list->items);
    free(list);
}
