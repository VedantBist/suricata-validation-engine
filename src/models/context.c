#include "models/context.h"

#include <stdlib.h>

#include "utils/xalloc.h"

ParserContext *context_create(void)
{
    ParserContext *ctx = xcalloc(1, sizeof(ParserContext));
    ctx->line = 1;
    ctx->column = 1;
    diag_list_init(&ctx->diagnostics);
    return ctx;
}

void context_destroy(ParserContext *ctx)
{
    if (ctx == NULL) {
        return;
    }
    diag_list_free(&ctx->diagnostics);
    free(ctx);
}
