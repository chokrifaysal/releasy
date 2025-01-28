#ifndef RELEASY_CONFIG_H
#define RELEASY_CONFIG_H

#include <json-c/json.h>

typedef struct {
    char *config_dir;
    json_object *json_config;
} config_context_t;

int config_init(config_context_t *ctx);
int config_load(config_context_t *ctx, const char *path);
const char *config_get_string(config_context_t *ctx, const char *key);
int config_get_bool(config_context_t *ctx, const char *key);
void config_cleanup(config_context_t *ctx);

#endif 