#ifndef RELEASY_DEPLOY_H
#define RELEASY_DEPLOY_H

typedef struct {
    char *name;
    char *target;
    char *script;
    char **env;
    int env_count;
} deploy_target_t;

typedef struct {
    deploy_target_t *targets;
    int target_count;
} deploy_config_t;

int deploy_load_config(const char *config_path, deploy_config_t *config);
int deploy_execute(const deploy_config_t *config, const char *target_name);
void deploy_config_free(deploy_config_t *config);

#endif 