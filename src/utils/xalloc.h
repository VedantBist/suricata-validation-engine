#ifndef UTILS_XALLOC_H
#define UTILS_XALLOC_H

#include <stddef.h>

/* Checked allocation helpers. Allocation failure is not a recoverable
 * condition for this tool: helpers abort with a message on stderr rather
 * than force NULL-checks through every module. */

void *xmalloc(size_t size);
void *xcalloc(size_t count, size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);
char *xstrndup(const char *s, size_t n);

#endif /* UTILS_XALLOC_H */
