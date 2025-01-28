#ifndef RELEASY_DEPLOY_H
#define RELEASY_DEPLOY_H

#include <json-c/json.h>
#include <string.h>
#include "releasy.h"

#define DEPLOY_ERR_CONFIG_NOT_FOUND -300
#define DEPLOY_ERR_INVALID_CONFIG -301
#define DEPLOY_ERR_ENV_NOT_FOUND -302
#define DEPLOY_ERR_SCRIPT_FAILED -303
#define DEPLOY_ERR_HOOK_FAILED -304
#define DEPLOY_ERR_STATUS_FAILED -305
#define DEPLOY_ERR_ROLLBACK_FAILED -306

typedef enum {
    DEPLOY_STATUS_NONE,
    DEPLOY_STATUS_PENDING,
    DEPLOY_STATUS_RUNNING,
    DEPLOY_STATUS_SUCCESS,
    DEPLOY_STATUS_FAILED,
    DEPLOY_STATUS_ROLLED_BACK
} deploy_status_t;

typedef struct {
    char *id;
    char *name;
    char *description;
    char *script;
    char **env;
    int env_count;
    int timeout;
    int retry_count;
    int retry_delay;
} deploy_hook_t;

typedef struct {
    char *name;
    char *description;
    char *script_path;
    char *working_dir;
    char **env_vars;
    int env_count;
    deploy_hook_t *pre_hooks;
    deploy_hook_t *post_hooks;
    int hook_count;
    int timeout;
    int verify_ssl;
    char *status_file;
} deploy_target_t;

typedef struct {
    deploy_target_t *targets;
    int target_count;
    char *log_file;
    char *status_dir;
    int dry_run;
    int verbose;
} deploy_config_t;

typedef struct {
    deploy_config_t *config;
    deploy_target_t *current_target;
    deploy_status_t status;
    char *current_version;
    char *previous_version;
    char *log_path;
    int is_dry_run;
} deploy_context_t;

// Core deployment functions
int deploy_init(deploy_context_t *ctx);
int deploy_load_config(deploy_context_t *ctx, const char *config_path);
int deploy_set_target(deploy_context_t *ctx, const char *target_name);
int deploy_execute(deploy_context_t *ctx, const char *version);
int deploy_rollback(deploy_context_t *ctx);
int deploy_get_status(deploy_context_t *ctx, deploy_status_t *status);

// Helper functions
const char *deploy_status_string(deploy_status_t status);
const char *deploy_error_string(int error_code);
void deploy_cleanup(deploy_context_t *ctx);
void deploy_free_hooks(deploy_hook_t *hooks, int count);
void deploy_free_target(deploy_target_t *target);

#endif 