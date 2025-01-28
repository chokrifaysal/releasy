#include "config.h"
#include "releasy.h"

int config_init(config_context_t *ctx) {
    return RELEASY_ERROR;
}

int config_load(config_context_t *ctx, const char *path) {
    return RELEASY_ERROR;
}

const char *config_get_string(config_context_t *ctx, const char *key) {
    return NULL;
}

int config_get_bool(config_context_t *ctx, const char *key) {
    return 0;
}

void config_cleanup(config_context_t *ctx) {
    if (!ctx) return;
    free(ctx->config_dir);
    if (ctx->json_config) json_object_put(ctx->json_config);
} 