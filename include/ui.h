#ifndef RELEASY_UI_H
#define RELEASY_UI_H

#include <stdio.h>
#include "releasy.h"

typedef struct {
    int interactive;
    FILE *log_file;
} ui_context_t;

int ui_init(ui_context_t *ctx);
int ui_confirm(const char *prompt);
int ui_select_option(const char *prompt, const char **options, int count);
void ui_log(ui_context_t *ctx, const char *fmt, ...);
void ui_cleanup(ui_context_t *ctx);

#endif 