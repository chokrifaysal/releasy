#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "release.h"
#include "git_ops.h"

static int execute_command(const char *command, const char *log_file) {
    if (!command) return RELEASY_SUCCESS;
    
    FILE *log = NULL;
    if (log_file) {
        log = fopen(log_file, "a");
        if (!log) return RELEASE_ERR_FILE_ACCESS;
        fprintf(log, "\nExecuting command: %s\n", command);
    }
    
    FILE *pipe = popen(command, "r");
    if (!pipe) {
        if (log) fclose(log);
        return RELEASE_ERR_FILE_ACCESS;
    }
    
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        if (log) fprintf(log, "%s", buffer);
    }
    
    int status = pclose(pipe);
    if (log) {
        fprintf(log, "Command exit status: %d\n", status);
        fclose(log);
    }
    
    return status == 0 ? RELEASY_SUCCESS : RELEASE_ERR_BUILD_FAILED;
}

int release_init(release_state_t *state, release_config_t *config) {
    if (!state || !config) return RELEASY_ERROR;
    
    memset(state, 0, sizeof(release_state_t));
    state->config = config;
    state->status = RELEASE_STATUS_PENDING;
    
    // Create artifact directory if specified
    if (config->artifacts && config->artifact_count > 0) {
        struct stat st = {0};
        if (stat("artifacts", &st) == -1) {
            mkdir("artifacts", 0700);
        }
        state->artifact_path = strdup("artifacts");
    }
    
    // Initialize log file
    state->log_file = strdup("release.log");
    if (!state->log_file) return RELEASE_ERR_MEMORY;
    
    return RELEASY_SUCCESS;
}

int release_prepare(release_state_t *state, const char *version) {
    if (!state || !version) return RELEASY_ERROR;
    
    // Initialize version info
    state->version = calloc(1, sizeof(version_info_t));
    if (!state->version) return RELEASE_ERR_MEMORY;
    
    if (version_init(state->version) != RELEASY_SUCCESS) {
        free(state->version);
        return RELEASE_ERR_INVALID_STATE;
    }
    
    // Set version
    state->version->current_version = strdup(version);
    if (!state->version->current_version) {
        version_cleanup(state->version);
        free(state->version);
        return RELEASE_ERR_MEMORY;
    }
    
    // Create release branch
    git_repository *repo = NULL;
    if (git_repository_open(&repo, ".") != 0) {
        version_cleanup(state->version);
        free(state->version);
        return RELEASE_ERR_GIT_OPERATION;
    }
    
    int ret = version_create_release_branch(repo, state->version);
    if (ret != RELEASY_SUCCESS) {
        git_repository_free(repo);
        version_cleanup(state->version);
        free(state->version);
        return ret;
    }
    
    // Initialize changelog if needed
    if (state->config->create_changelog) {
        state->changelog = calloc(1, sizeof(changelog_t));
        if (!state->changelog) {
            git_repository_free(repo);
            version_cleanup(state->version);
            free(state->version);
            return RELEASE_ERR_MEMORY;
        }
        
        if (changelog_init(state->changelog, "CHANGELOG.md") != RELEASY_SUCCESS) {
            free(state->changelog);
            git_repository_free(repo);
            version_cleanup(state->version);
            free(state->version);
            return RELEASE_ERR_INVALID_STATE;
        }
        
        ret = changelog_generate(state->changelog, repo, version);
        if (ret != RELEASY_SUCCESS) {
            changelog_cleanup(state->changelog);
            free(state->changelog);
            git_repository_free(repo);
            version_cleanup(state->version);
            free(state->version);
            return ret;
        }
    }
    
    git_repository_free(repo);
    state->status = RELEASE_STATUS_IN_PROGRESS;
    return RELEASY_SUCCESS;
}

int release_build(release_state_t *state) {
    if (!state || !state->config) return RELEASY_ERROR;
    
    return execute_command(state->config->build_command, state->log_file);
}

int release_test(release_state_t *state) {
    if (!state || !state->config) return RELEASY_ERROR;
    
    return execute_command(state->config->test_command, state->log_file);
}

int release_deploy(release_state_t *state, const char *environment) {
    if (!state || !state->config || !environment) return RELEASY_ERROR;
    
    // Verify environment is configured
    int valid_env = 0;
    for (size_t i = 0; i < state->config->env_count; i++) {
        if (strcmp(state->config->environments[i], environment) == 0) {
            valid_env = 1;
            break;
        }
    }
    if (!valid_env) return RELEASE_ERR_INVALID_CONFIG;
    
    // Execute deploy command with environment
    char deploy_cmd[1024];
    snprintf(deploy_cmd, sizeof(deploy_cmd), "%s %s", 
             state->config->deploy_command, environment);
    
    return execute_command(deploy_cmd, state->log_file);
}

int release_publish(release_state_t *state) {
    if (!state || !state->config) return RELEASY_ERROR;
    
    // Write changelog if needed
    if (state->config->create_changelog && state->changelog) {
        int ret = changelog_write(state->changelog);
        if (ret != RELEASY_SUCCESS) return ret;
    }
    
    // Create tag if needed
    if (state->config->create_tag) {
        git_repository *repo = NULL;
        if (git_repository_open(&repo, ".") != 0) {
            return RELEASE_ERR_GIT_OPERATION;
        }
        
        char tag_message[256];
        snprintf(tag_message, sizeof(tag_message), "Release %s", 
                 state->version->current_version);
        
        int ret = version_create_tag(repo, state->version, tag_message);
        git_repository_free(repo);
        if (ret != RELEASY_SUCCESS) return ret;
    }
    
    // Execute publish command
    return execute_command(state->config->publish_command, state->log_file);
}

int release_notify(release_state_t *state) {
    if (!state || !state->config) return RELEASY_ERROR;
    
    if ((state->status == RELEASE_STATUS_COMPLETED && state->config->notify_on_success) ||
        (state->status == RELEASE_STATUS_FAILED && state->config->notify_on_failure)) {
        return execute_command(state->config->notify_command, state->log_file);
    }
    
    return RELEASY_SUCCESS;
}

int release_finish(release_state_t *state) {
    if (!state) return RELEASY_ERROR;
    
    state->status = RELEASE_STATUS_COMPLETED;
    return RELEASY_SUCCESS;
}

int release_rollback(release_state_t *state) {
    if (!state) return RELEASY_ERROR;
    
    // TODO: Implement rollback logic
    state->status = RELEASE_STATUS_ROLLED_BACK;
    return RELEASY_SUCCESS;
}

void release_cleanup(release_state_t *state) {
    if (!state) return;
    
    if (state->version) {
        version_cleanup(state->version);
        free(state->version);
    }
    
    if (state->changelog) {
        changelog_cleanup(state->changelog);
        free(state->changelog);
    }
    
    free(state->release_branch);
    free(state->release_tag);
    free(state->release_notes);
    free(state->artifact_path);
    free(state->log_file);
    free(state->error_message);
    
    memset(state, 0, sizeof(release_state_t));
}

const char *release_error_string(int error_code) {
    switch (error_code) {
        case RELEASY_SUCCESS:
            return "Success";
        case RELEASE_ERR_INVALID_CONFIG:
            return "Invalid release configuration";
        case RELEASE_ERR_INVALID_STATE:
            return "Invalid release state";
        case RELEASE_ERR_BUILD_FAILED:
            return "Build failed";
        case RELEASE_ERR_TEST_FAILED:
            return "Tests failed";
        case RELEASE_ERR_DEPLOY_FAILED:
            return "Deployment failed";
        case RELEASE_ERR_PUBLISH_FAILED:
            return "Publishing failed";
        case RELEASE_ERR_NOTIFY_FAILED:
            return "Notification failed";
        case RELEASE_ERR_MEMORY:
            return "Memory allocation failed";
        case RELEASE_ERR_FILE_ACCESS:
            return "File access failed";
        case RELEASE_ERR_GIT_OPERATION:
            return "Git operation failed";
        default:
            return "Unknown error";
    }
}

const char *release_status_string(release_status_t status) {
    switch (status) {
        case RELEASE_STATUS_PENDING:
            return "Pending";
        case RELEASE_STATUS_IN_PROGRESS:
            return "In Progress";
        case RELEASE_STATUS_COMPLETED:
            return "Completed";
        case RELEASE_STATUS_FAILED:
            return "Failed";
        case RELEASE_STATUS_ROLLED_BACK:
            return "Rolled Back";
        default:
            return "Unknown";
    }
}

const char *release_type_string(release_type_t type) {
    switch (type) {
        case RELEASE_TYPE_MAJOR:
            return "Major";
        case RELEASE_TYPE_MINOR:
            return "Minor";
        case RELEASE_TYPE_PATCH:
            return "Patch";
        case RELEASE_TYPE_PRERELEASE:
            return "Pre-release";
        case RELEASE_TYPE_CUSTOM:
            return "Custom";
        default:
            return "Unknown";
    }
} 