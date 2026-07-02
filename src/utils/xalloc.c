#include "utils/xalloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *check(void *ptr, size_t size)
{
    if (ptr == NULL && size != 0) {
        fprintf(stderr, "fatal: out of memory (%zu bytes)\n", size);
        abort();
    }
    return ptr;
}

void *xmalloc(size_t size)
{
    return check(malloc(size), size);
}

void *xcalloc(size_t count, size_t size)
{
    return check(calloc(count, size), count * size);
}

void *xrealloc(void *ptr, size_t size)
{
    return check(realloc(ptr, size), size);
}

char *xstrdup(const char *s)
{
    return check(strdup(s), strlen(s) + 1);
}

char *xstrndup(const char *s, size_t n)
{
    char *copy = xmalloc(n + 1);
    memcpy(copy, s, n);
    copy[n] = '\0';
    return copy;
}
