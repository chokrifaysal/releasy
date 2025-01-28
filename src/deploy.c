#include "deploy.h"
#include "releasy.h"

int deploy_load_config(const char *config_path, deploy_config_t *config) {
    return RELEASY_ERROR;
}

int deploy_execute(const deploy_config_t *config, const char *target_name) {
    return RELEASY_ERROR;
}

void deploy_config_free(deploy_config_t *config) {
    if (!config) return;
    for (int i = 0; i < config->target_count; i++) {
        free(config->targets[i].name);
        free(config->targets[i].target);
        free(config->targets[i].script);
        for (int j = 0; j < config->targets[i].env_count; j++) {
            free(config->targets[i].env[j]);
        }
        free(config->targets[i].env);
    }
    free(config->targets);
} 