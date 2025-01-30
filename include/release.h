#ifndef RELEASY_RELEASE_H
#define RELEASY_RELEASE_H

#include <git2.h>
#include "releasy.h"
#include "version.h"
#include "changelog.h"

// Error codes
#define RELEASE_ERR_INVALID_CONFIG -600
#define RELEASE_ERR_INVALID_STATE -601
#define RELEASE_ERR_BUILD_FAILED -602
#define RELEASE_ERR_TEST_FAILED -603
#define RELEASE_ERR_DEPLOY_FAILED -604
#define RELEASE_ERR_PUBLISH_FAILED -605
#define RELEASE_ERR_NOTIFY_FAILED -606
#define RELEASE_ERR_MEMORY -607
#define RELEASE_ERR_FILE_ACCESS -608
#define RELEASE_ERR_GIT_OPERATION -609

// Release types
typedef enum {
    RELEASE_TYPE_MAJOR,
    RELEASE_TYPE_MINOR,
    RELEASE_TYPE_PATCH,
    RELEASE_TYPE_PRERELEASE,
    RELEASE_TYPE_CUSTOM
} release_type_t;

// Release status
typedef enum {
    RELEASE_STATUS_PENDING,
    RELEASE_STATUS_IN_PROGRESS,
    RELEASE_STATUS_COMPLETED,
    RELEASE_STATUS_FAILED,
    RELEASE_STATUS_ROLLED_BACK
} release_status_t;

// Release configuration
typedef struct {
    char *name;
    char *description;
    release_type_t type;
    char *target_branch;
    char *build_command;
    char *test_command;
    char *deploy_command;
    char *publish_command;
    char *notify_command;
    char **artifacts;
    size_t artifact_count;
    char **environments;
    size_t env_count;
    int create_changelog;
    int create_tag;
    int push_changes;
    int notify_on_success;
    int notify_on_failure;
    release_status_t status;
} release_config_t;

// Release state
typedef struct {
    release_config_t *config;
    version_info_t *version;
    changelog_t *changelog;
    char *release_branch;
    char *release_tag;
    char *release_notes;
    char *artifact_path;
    char *log_file;
    release_status_t status;
    char *error_message;
} release_state_t;

// Function declarations
int release_init(release_state_t *state, release_config_t *config);
int release_load_config(release_config_t *config, const char *config_file);
int release_save_config(release_config_t *config, const char *config_file);
int release_validate(release_state_t *state);
int release_prepare(release_state_t *state, const char *version);
int release_build(release_state_t *state);
int release_test(release_state_t *state);
int release_deploy(release_state_t *state, const char *environment);
int release_publish(release_state_t *state);
int release_notify(release_state_t *state);
int release_finish(release_state_t *state);
int release_rollback(release_state_t *state);
int release_get_status(release_state_t *state, release_status_t *status);
void release_cleanup(release_state_t *state);

const char *release_error_string(int error_code);
const char *release_status_string(release_status_t status);
const char *release_type_string(release_type_t type);

#endif // RELEASY_RELEASE_H 