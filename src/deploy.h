#ifndef DEPLOY_H
#define DEPLOY_H

#include <json-c/json.h>

// Error codes
#define RELEASY_SUCCESS 0
#define RELEASY_ERROR 1

#define DEPLOY_ERR_CONFIG_NOT_FOUND 2
#define DEPLOY_ERR_INVALID_CONFIG 3
#define DEPLOY_ERR_ENV_NOT_FOUND 4
#define DEPLOY_ERR_SCRIPT_FAILED 5
#define DEPLOY_ERR_HOOK_FAILED 6
#define DEPLOY_ERR_STATUS_FAILED 7
#define DEPLOY_ERR_ROLLBACK_FAILED 8

// Status codes
typedef enum {
    DEPLOY_STATUS_NONE = 0,
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
    char *working_dir;
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
    char *status_file;
    char **env_vars;
    int env_count;
    int timeout;
    int verify_ssl;
    deploy_hook_t *pre_hooks;
    deploy_hook_t *post_hooks;
    int pre_hook_count;
    int post_hook_count;
} deploy_target_t;

typedef struct {
    char *config_path;
    char *log_path;
    char *current_version;
    char *previous_version;
    json_object *config;
    deploy_target_t *targets;
    int target_count;
    deploy_target_t *current_target;
    deploy_status_t status;
    int dry_run;
    int verbose;
} deploy_context_t;

// Function declarations
int deploy_init(deploy_context_t *ctx);
int deploy_load_config(deploy_context_t *ctx, const char *config_path);
int deploy_set_target(deploy_context_t *ctx, const char *target_name);
int deploy_execute(deploy_context_t *ctx, const char *version);
int deploy_rollback(deploy_context_t *ctx);
int deploy_get_status(deploy_context_t *ctx, deploy_status_t *status);
const char *deploy_status_string(deploy_status_t status);
const char *deploy_error_string(int error_code);
void deploy_cleanup(deploy_context_t *ctx);
void deploy_free_hooks(deploy_hook_t *hooks, int count);
void deploy_free_target(deploy_target_t *target);

#endif // DEPLOY_H 