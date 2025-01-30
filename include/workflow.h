#ifndef RELEASY_WORKFLOW_H
#define RELEASY_WORKFLOW_H

#include <git2.h>
#include "releasy.h"
#include "version.h"

// Error codes
#define WORKFLOW_ERR_INVALID_CONFIG -500
#define WORKFLOW_ERR_INVALID_STATE -501
#define WORKFLOW_ERR_STEP_FAILED -502
#define WORKFLOW_ERR_HOOK_FAILED -503
#define WORKFLOW_ERR_PLUGIN_FAILED -504
#define WORKFLOW_ERR_ARTIFACT_FAILED -505
#define WORKFLOW_ERR_ENV_FAILED -506
#define WORKFLOW_ERR_ROLLBACK_FAILED -507
#define WORKFLOW_ERR_MEMORY -508
#define WORKFLOW_ERR_FILE_ACCESS -509

// Workflow step types
typedef enum {
    WORKFLOW_STEP_VALIDATE,
    WORKFLOW_STEP_VERSION,
    WORKFLOW_STEP_BRANCH,
    WORKFLOW_STEP_BUILD,
    WORKFLOW_STEP_TEST,
    WORKFLOW_STEP_DOCS,
    WORKFLOW_STEP_CHANGELOG,
    WORKFLOW_STEP_TAG,
    WORKFLOW_STEP_PUBLISH,
    WORKFLOW_STEP_NOTIFY,
    WORKFLOW_STEP_CUSTOM
} workflow_step_type_t;

// Workflow step status
typedef enum {
    WORKFLOW_STATUS_PENDING,
    WORKFLOW_STATUS_RUNNING,
    WORKFLOW_STATUS_SUCCESS,
    WORKFLOW_STATUS_FAILED,
    WORKFLOW_STATUS_SKIPPED,
    WORKFLOW_STATUS_ROLLBACK
} workflow_status_t;

// Hook execution points
typedef enum {
    WORKFLOW_HOOK_PRE_STEP,
    WORKFLOW_HOOK_POST_STEP,
    WORKFLOW_HOOK_PRE_WORKFLOW,
    WORKFLOW_HOOK_POST_WORKFLOW,
    WORKFLOW_HOOK_ON_ERROR,
    WORKFLOW_HOOK_ON_ROLLBACK
} workflow_hook_point_t;

// Workflow step configuration
typedef struct {
    workflow_step_type_t type;
    char *name;
    char *description;
    char *command;
    int required;
    int allow_failure;
    char **env_vars;
    size_t env_count;
    char **artifacts;
    size_t artifact_count;
    workflow_status_t status;
} workflow_step_t;

// Hook configuration
typedef struct {
    workflow_hook_point_t point;
    char *name;
    char *command;
    char **env_vars;
    size_t env_count;
} workflow_hook_t;

// Plugin configuration
typedef struct {
    char *name;
    char *path;
    char *version;
    char **config;
    size_t config_count;
} workflow_plugin_t;

// Workflow configuration
typedef struct {
    char *name;
    char *description;
    workflow_step_t **steps;
    size_t step_count;
    workflow_hook_t **hooks;
    size_t hook_count;
    workflow_plugin_t **plugins;
    size_t plugin_count;
    char *working_dir;
    char *artifact_dir;
    char *log_file;
    int notify_on_success;
    int notify_on_failure;
    workflow_status_t status;
} workflow_config_t;

// Function declarations
int workflow_init(workflow_config_t *config);
int workflow_load(workflow_config_t *config, const char *config_file);
int workflow_save(workflow_config_t *config, const char *config_file);
int workflow_validate(workflow_config_t *config);
int workflow_execute(workflow_config_t *config, version_info_t *version);
int workflow_rollback(workflow_config_t *config);
int workflow_add_step(workflow_config_t *config, workflow_step_t *step);
int workflow_add_hook(workflow_config_t *config, workflow_hook_t *hook);
int workflow_add_plugin(workflow_config_t *config, workflow_plugin_t *plugin);
int workflow_get_status(workflow_config_t *config, workflow_status_t *status);
void workflow_cleanup(workflow_config_t *config);

const char *workflow_error_string(int error_code);
const char *workflow_status_string(workflow_status_t status);
const char *workflow_step_type_string(workflow_step_type_t type);

#endif // RELEASY_WORKFLOW_H 