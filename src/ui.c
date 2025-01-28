#include "ui.h"
#include "releasy.h"
#include <stdarg.h>

int ui_init(ui_context_t *ctx) {
    return RELEASY_ERROR;
}

int ui_confirm(const char *prompt) {
    return 0;
}

int ui_select_option(const char *prompt, const char **options, int count) {
    return -1;
}

void ui_log(ui_context_t *ctx, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(ctx->log_file ? ctx->log_file : stderr, fmt, args);
    va_end(args);
}

void ui_cleanup(ui_context_t *ctx) {
    if (!ctx) return;
    if (ctx->log_file) fclose(ctx->log_file);
} 