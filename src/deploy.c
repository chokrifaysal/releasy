#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <json-c/json.h>
#include "deploy.h"
#include "ui.h"
#include "releasy.h"

static int deploy_parse_hook(json_object *hook_obj, deploy_hook_t *hook) {
    if (!hook_obj || !hook) return DEPLOY_ERR_INVALID_CONFIG;
    if (!json_object_is_type(hook_obj, json_type_object)) return DEPLOY_ERR_INVALID_CONFIG;

    // Initialize hook structure
    memset(hook, 0, sizeof(deploy_hook_t));

    json_object *tmp;
    if (json_object_object_get_ex(hook_obj, "id", &tmp) && tmp)
        hook->id = strdup(json_object_get_string(tmp));
    if (json_object_object_get_ex(hook_obj, "name", &tmp) && tmp)
        hook->name = strdup(json_object_get_string(tmp));
    if (json_object_object_get_ex(hook_obj, "description", &tmp) && tmp)
        hook->description = strdup(json_object_get_string(tmp));
    if (json_object_object_get_ex(hook_obj, "script", &tmp) && tmp)
        hook->script = strdup(json_object_get_string(tmp));
    if (json_object_object_get_ex(hook_obj, "working_dir", &tmp) && tmp)
        hook->working_dir = strdup(json_object_get_string(tmp));
    if (json_object_object_get_ex(hook_obj, "timeout", &tmp) && tmp)
        hook->timeout = json_object_get_int(tmp);
    if (json_object_object_get_ex(hook_obj, "retry_count", &tmp) && tmp)
        hook->retry_count = json_object_get_int(tmp);
    if (json_object_object_get_ex(hook_obj, "retry_delay", &tmp) && tmp)
        hook->retry_delay = json_object_get_int(tmp);

    // Set default values
    if (!hook->timeout) hook->timeout = 300;
    if (!hook->retry_count) hook->retry_count = 3;
    if (!hook->retry_delay) hook->retry_delay = 5;

    json_object *env_obj;
    if (json_object_object_get_ex(hook_obj, "env", &env_obj) &&
        json_object_is_type(env_obj, json_type_array)) {
        hook->env_count = json_object_array_length(env_obj);
        if (hook->env_count > 0) {
            hook->env = calloc(hook->env_count, sizeof(char *));
            if (!hook->env) {
                hook->env_count = 0;
                return RELEASY_ERROR;
            }

            for (int i = 0; i < hook->env_count; i++) {
                json_object *env_item = json_object_array_get_idx(env_obj, i);
                if (env_item) {
                    const char *env_str = json_object_get_string(env_item);
                    if (env_str) {
                        hook->env[i] = strdup(env_str);
                        if (!hook->env[i]) {
                            // Clean up on allocation failure
                            for (int j = 0; j < i; j++) {
                                free(hook->env[j]);
                            }
                            free(hook->env);
                            hook->env = NULL;
                            hook->env_count = 0;
                            return RELEASY_ERROR;
                        }
                    }
                }
            }
        }
    }

    return RELEASY_SUCCESS;
}

static int deploy_parse_target(json_object *target_obj, deploy_target_t *target) {
    if (!target_obj || !target) return DEPLOY_ERR_INVALID_CONFIG;
    if (!json_object_is_type(target_obj, json_type_object)) return DEPLOY_ERR_INVALID_CONFIG;

    // Initialize target structure
    memset(target, 0, sizeof(deploy_target_t));

    json_object *tmp;
    if (json_object_object_get_ex(target_obj, "name", &tmp) && tmp)
        target->name = strdup(json_object_get_string(tmp));
    if (json_object_object_get_ex(target_obj, "description", &tmp) && tmp)
        target->description = strdup(json_object_get_string(tmp));
    if (json_object_object_get_ex(target_obj, "script_path", &tmp) && tmp)
        target->script_path = strdup(json_object_get_string(tmp));
    if (json_object_object_get_ex(target_obj, "working_dir", &tmp) && tmp)
        target->working_dir = strdup(json_object_get_string(tmp));
    if (json_object_object_get_ex(target_obj, "status_file", &tmp) && tmp)
        target->status_file = strdup(json_object_get_string(tmp));
    if (json_object_object_get_ex(target_obj, "timeout", &tmp) && tmp)
        target->timeout = json_object_get_int(tmp);
    if (json_object_object_get_ex(target_obj, "verify_ssl", &tmp) && tmp)
        target->verify_ssl = json_object_get_boolean(tmp);

    // Set default values
    if (!target->timeout) target->timeout = 300;
    if (!target->verify_ssl) target->verify_ssl = 1;

    json_object *env_obj;
    if (json_object_object_get_ex(target_obj, "env", &env_obj) &&
        json_object_is_type(env_obj, json_type_array)) {
        target->env_count = json_object_array_length(env_obj);
        if (target->env_count > 0) {
            target->env_vars = calloc(target->env_count, sizeof(char *));
            if (!target->env_vars) {
                target->env_count = 0;
                return RELEASY_ERROR;
            }

            for (int i = 0; i < target->env_count; i++) {
                json_object *env_item = json_object_array_get_idx(env_obj, i);
                if (env_item) {
                    const char *env_str = json_object_get_string(env_item);
                    if (env_str) {
                        target->env_vars[i] = strdup(env_str);
                        if (!target->env_vars[i]) {
                            // Clean up on allocation failure
                            for (int j = 0; j < i; j++) {
                                free(target->env_vars[j]);
                            }
                            free(target->env_vars);
                            target->env_vars = NULL;
                            target->env_count = 0;
                            return RELEASY_ERROR;
                        }
                    }
                }
            }
        }
    }

    json_object *hooks_obj;
    if (json_object_object_get_ex(target_obj, "hooks", &hooks_obj) &&
        json_object_is_type(hooks_obj, json_type_object)) {
        json_object *pre_hooks, *post_hooks;
        
        // Parse pre-hooks
        if (json_object_object_get_ex(hooks_obj, "pre", &pre_hooks) &&
            json_object_is_type(pre_hooks, json_type_array)) {
            target->pre_hook_count = json_object_array_length(pre_hooks);
            if (target->pre_hook_count > 0) {
                target->pre_hooks = calloc(target->pre_hook_count, sizeof(deploy_hook_t));
                if (!target->pre_hooks) {
                    target->pre_hook_count = 0;
                    return RELEASY_ERROR;
                }

                for (int i = 0; i < target->pre_hook_count; i++) {
                    json_object *hook = json_object_array_get_idx(pre_hooks, i);
                    if (hook) {
                        int ret = deploy_parse_hook(hook, &target->pre_hooks[i]);
                        if (ret != RELEASY_SUCCESS) {
                            // Clean up on failure
                            for (int j = 0; j < i; j++) {
                                deploy_free_hooks(&target->pre_hooks[j], 1);
                            }
                            free(target->pre_hooks);
                            target->pre_hooks = NULL;
                            target->pre_hook_count = 0;
                            return ret;
                        }
                    }
                }
            }
        }

        // Parse post-hooks
        if (json_object_object_get_ex(hooks_obj, "post", &post_hooks) &&
            json_object_is_type(post_hooks, json_type_array)) {
            target->post_hook_count = json_object_array_length(post_hooks);
            if (target->post_hook_count > 0) {
                target->post_hooks = calloc(target->post_hook_count, sizeof(deploy_hook_t));
                if (!target->post_hooks) {
                    if (target->pre_hooks) {
                        deploy_free_hooks(target->pre_hooks, target->pre_hook_count);
                        free(target->pre_hooks);
                        target->pre_hooks = NULL;
                        target->pre_hook_count = 0;
                    }
                    return RELEASY_ERROR;
                }

                for (int i = 0; i < target->post_hook_count; i++) {
                    json_object *hook = json_object_array_get_idx(post_hooks, i);
                    if (hook) {
                        int ret = deploy_parse_hook(hook, &target->post_hooks[i]);
                        if (ret != RELEASY_SUCCESS) {
                            // Clean up on failure
                            for (int j = 0; j < i; j++) {
                                deploy_free_hooks(&target->post_hooks[j], 1);
                            }
                            free(target->post_hooks);
                            target->post_hooks = NULL;
                            target->post_hook_count = 0;
                            if (target->pre_hooks) {
                                deploy_free_hooks(target->pre_hooks, target->pre_hook_count);
                                free(target->pre_hooks);
                                target->pre_hooks = NULL;
                                target->pre_hook_count = 0;
                            }
                            return ret;
                        }
                    }
                }
            }
        }
    }

    return RELEASY_SUCCESS;
}

int deploy_init(deploy_context_t *ctx) {
    if (!ctx) return RELEASY_ERROR;

    memset(ctx, 0, sizeof(deploy_context_t));
    ctx->status = DEPLOY_STATUS_NONE;
    ctx->dry_run = 0;
    ctx->verbose = 0;

    return RELEASY_SUCCESS;
}

int deploy_load_config(deploy_context_t *ctx, const char *config_path) {
    if (!ctx || !config_path) return DEPLOY_ERR_INVALID_CONFIG;

    printf("Loading config from: %s\n", config_path);
    ctx->config_path = strdup(config_path);
    if (!ctx->config_path) return RELEASY_ERROR;

    json_object *config = json_object_from_file(config_path);
    if (!config) {
        printf("Failed to load config file\n");
        free(ctx->config_path);
        ctx->config_path = NULL;
        return DEPLOY_ERR_CONFIG_NOT_FOUND;
    }

    printf("Config file loaded successfully\n");
    ctx->config = config;

    json_object *tmp;
    if (json_object_object_get_ex(config, "log_path", &tmp) && tmp)
        ctx->log_path = strdup(json_object_get_string(tmp));

    if (json_object_object_get_ex(config, "dry_run", &tmp) && tmp)
        ctx->dry_run = json_object_get_boolean(tmp);

    if (json_object_object_get_ex(config, "verbose", &tmp) && tmp)
        ctx->verbose = json_object_get_boolean(tmp);

    json_object *targets_obj;
    if (json_object_object_get_ex(config, "targets", &targets_obj) &&
        json_object_is_type(targets_obj, json_type_array)) {
        ctx->target_count = json_object_array_length(targets_obj);
        if (ctx->target_count > 0) {
            ctx->targets = calloc(ctx->target_count, sizeof(deploy_target_t));
            if (!ctx->targets) {
                ctx->target_count = 0;
                json_object_put(ctx->config);
                ctx->config = NULL;
                free(ctx->config_path);
                ctx->config_path = NULL;
                free(ctx->log_path);
                ctx->log_path = NULL;
                return RELEASY_ERROR;
            }

            for (int i = 0; i < ctx->target_count; i++) {
                json_object *target = json_object_array_get_idx(targets_obj, i);
                if (target) {
                    int ret = deploy_parse_target(target, &ctx->targets[i]);
                    if (ret != RELEASY_SUCCESS) {
                        // Clean up on failure
                        for (int j = 0; j < i; j++) {
                            deploy_free_target(&ctx->targets[j]);
                        }
                        free(ctx->targets);
                        ctx->targets = NULL;
                        ctx->target_count = 0;
                        json_object_put(ctx->config);
                        ctx->config = NULL;
                        free(ctx->config_path);
                        ctx->config_path = NULL;
                        free(ctx->log_path);
                        ctx->log_path = NULL;
                        return ret;
                    }
                }
            }
        }
    }

    return RELEASY_SUCCESS;
}

int deploy_set_target(deploy_context_t *ctx, const char *target_name) {
    if (!ctx || !target_name) return DEPLOY_ERR_ENV_NOT_FOUND;

    printf("Setting target to: %s\n", target_name);
    printf("Available targets: %d\n", ctx->target_count);

    for (int i = 0; i < ctx->target_count; i++) {
        printf("Checking target: %s\n", ctx->targets[i].name);
        if (strcmp(ctx->targets[i].name, target_name) == 0) {
            ctx->current_target = &ctx->targets[i];
            return RELEASY_SUCCESS;
        }
    }

    return DEPLOY_ERR_ENV_NOT_FOUND;
}

static int deploy_execute_script(deploy_context_t *ctx, const char *script, char **env, int env_count) {
    if (!ctx || !script) return RELEASY_ERROR;

    if (ctx->dry_run) {
        printf("[DRY RUN] Would execute script: %s\n", script);
        return RELEASY_SUCCESS;
    }

    if (ctx->verbose) {
        printf("Executing script: %s\n", script);
    }

    // Create a pipe for capturing script output
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return RELEASY_ERROR;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return RELEASY_ERROR;
    }

    if (pid == 0) {  // Child process
        close(pipefd[0]);  // Close read end
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        // Set environment variables
        for (int i = 0; i < env_count; i++) {
            if (env[i]) {
                char *env_copy = strdup(env[i]);
                if (env_copy) {
                    char *equals = strchr(env_copy, '=');
                    if (equals) {
                        *equals = '\0';  // Split into name and value
                        setenv(env_copy, equals + 1, 1);
                        free(env_copy);
                    }
                }
            }
        }

        execl("/bin/sh", "sh", "-c", script, (char *)NULL);
        perror("execl");
        exit(1);
    }

    // Parent process
    close(pipefd[1]);  // Close write end

    // Read and display script output
    char buffer[1024];
    ssize_t bytes_read;
    while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        printf("%s", buffer);
    }
    close(pipefd[0]);

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        if (exit_code != 0) {
            printf("Script failed with exit code: %d\n", exit_code);
            return DEPLOY_ERR_SCRIPT_FAILED;
        }
    } else {
        printf("Script terminated abnormally\n");
        return DEPLOY_ERR_SCRIPT_FAILED;
    }

    return RELEASY_SUCCESS;
}

static int deploy_execute_hooks(deploy_context_t *ctx, deploy_hook_t *hooks, int count, const char *phase) {
    if (!ctx || !hooks || count <= 0) return RELEASY_SUCCESS;  // No hooks to execute is not an error

    for (int i = 0; i < count; i++) {
        deploy_hook_t *hook = &hooks[i];
        if (!hook->script) continue;  // Skip hooks without scripts

        if (ctx->verbose) {
            printf("Executing %s hook: %s\n", phase, hook->name ? hook->name : "unnamed");
        }

        int retries = 0;
        int ret;

        do {
            ret = deploy_execute_script(ctx, hook->script, hook->env, hook->env_count);
            if (ret == RELEASY_SUCCESS) break;

            retries++;
            if (retries < hook->retry_count) {
                if (ctx->verbose) {
                    printf("Hook failed, retrying in %d seconds...\n", hook->retry_delay);
                }
                sleep(hook->retry_delay);
            }
        } while (retries <= hook->retry_count);

        if (ret != RELEASY_SUCCESS) {
            printf("Hook failed after %d retries\n", retries);
            return DEPLOY_ERR_HOOK_FAILED;
        }
    }

    return RELEASY_SUCCESS;
}

static int deploy_update_status(deploy_context_t *ctx, const char *version, const char *status) {
    if (!ctx || !ctx->current_target || !version || !status) return RELEASY_ERROR;
    if (!ctx->current_target->status_file) return RELEASY_SUCCESS;  // No status file configured

    json_object *status_obj = NULL;
    FILE *f = fopen(ctx->current_target->status_file, "r");
    if (f) {
        char buffer[4096];
        size_t len = fread(buffer, 1, sizeof(buffer) - 1, f);
        buffer[len] = '\0';
        fclose(f);
        status_obj = json_tokener_parse(buffer);
    }

    if (!status_obj) {
        status_obj = json_object_new_object();
        if (!status_obj) return RELEASY_ERROR;
    }

    // Update status information
    json_object *prev_version = NULL;
    json_object_object_get_ex(status_obj, "current_version", &prev_version);
    if (prev_version) {
        json_object_object_add(status_obj, "previous_version",
                             json_object_new_string(json_object_get_string(prev_version)));
    } else {
        json_object_object_add(status_obj, "previous_version", NULL);
    }

    json_object_object_add(status_obj, "current_version", json_object_new_string(version));
    json_object_object_add(status_obj, "status", json_object_new_string(status));

    // Get current timestamp
    time_t now;
    time(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    json_object_object_add(status_obj, "last_deployment", json_object_new_string(timestamp));

    // Update deployment history
    json_object *history = NULL;
    if (!json_object_object_get_ex(status_obj, "history", &history)) {
        history = json_object_new_array();
        json_object_object_add(status_obj, "history", history);
    }

    json_object *entry = json_object_new_object();
    json_object_object_add(entry, "version", json_object_new_string(version));
    json_object_object_add(entry, "timestamp", json_object_new_string(timestamp));
    json_object_object_add(entry, "status", json_object_new_string(status));
    
    char user_info[256];
    snprintf(user_info, sizeof(user_info), "%s <%s>", ctx->user_name ? ctx->user_name : "unknown",
             ctx->user_email ? ctx->user_email : "unknown");
    json_object_object_add(entry, "user", json_object_new_string(user_info));

    json_object_array_add(history, entry);

    // Write updated status to file
    const char *json_str = json_object_to_json_string_ext(status_obj, JSON_C_TO_STRING_PRETTY);
    if (!json_str) {
        json_object_put(status_obj);
        return RELEASY_ERROR;
    }

    f = fopen(ctx->current_target->status_file, "w");
    if (!f) {
        json_object_put(status_obj);
        return RELEASY_ERROR;
    }

    fprintf(f, "%s\n", json_str);
    fclose(f);
    json_object_put(status_obj);

    return RELEASY_SUCCESS;
}

int deploy_execute(deploy_context_t *ctx, const char *version) {
    if (!ctx || !ctx->current_target || !version) return RELEASY_ERROR;

    // Save previous version for rollback
    free(ctx->previous_version);
    ctx->previous_version = ctx->current_version ? strdup(ctx->current_version) : NULL;
    if (ctx->previous_version && !ctx->previous_version) {
        return RELEASY_ERROR;
    }

    free(ctx->current_version);
    ctx->current_version = strdup(version);
    if (!ctx->current_version) {
        free(ctx->previous_version);
        ctx->previous_version = NULL;
        return RELEASY_ERROR;
    }

    if (ctx->verbose) {
        printf("Starting deployment of version %s to target %s\n",
               version, ctx->current_target->name ? ctx->current_target->name : "unnamed");
        if (ctx->dry_run) {
            printf("[DRY RUN] No changes will be made\n");
        }
    }

    // Execute pre-deployment hooks
    ctx->status = DEPLOY_STATUS_RUNNING;
    deploy_update_status(ctx, version, "running");

    int ret = deploy_execute_hooks(ctx, ctx->current_target->pre_hooks, 
                                 ctx->current_target->pre_hook_count, "pre-deploy");
    if (ret != RELEASY_SUCCESS) {
        ctx->status = DEPLOY_STATUS_FAILED;
        deploy_update_status(ctx, version, "failed");
        return ret;
    }

    // Execute deployment script
    if (ctx->current_target->script_path) {
        ret = deploy_execute_script(ctx, ctx->current_target->script_path,
                                  ctx->current_target->env_vars,
                                  ctx->current_target->env_count);
        if (ret != RELEASY_SUCCESS) {
            ctx->status = DEPLOY_STATUS_FAILED;
            deploy_update_status(ctx, version, "failed");
            return ret;
        }
    }

    // Execute post-deployment hooks
    ret = deploy_execute_hooks(ctx, ctx->current_target->post_hooks,
                             ctx->current_target->post_hook_count, "post-deploy");
    if (ret != RELEASY_SUCCESS) {
        ctx->status = DEPLOY_STATUS_FAILED;
        deploy_update_status(ctx, version, "failed");
        return ret;
    }

    ctx->status = DEPLOY_STATUS_SUCCESS;
    deploy_update_status(ctx, version, "success");
    return RELEASY_SUCCESS;
}

int deploy_rollback(deploy_context_t *ctx) {
    if (!ctx || !ctx->current_target || !ctx->previous_version) return RELEASY_ERROR;

    if (ctx->verbose) {
        printf("Rolling back from version %s to version %s\n",
               ctx->current_version ? ctx->current_version : "unknown",
               ctx->previous_version);
    }

    int ret = deploy_execute(ctx, ctx->previous_version);
    if (ret != RELEASY_SUCCESS) {
        ctx->status = DEPLOY_STATUS_FAILED;
        return DEPLOY_ERR_ROLLBACK_FAILED;
    }

    ctx->status = DEPLOY_STATUS_ROLLED_BACK;
    return RELEASY_SUCCESS;
}

int deploy_get_status(deploy_context_t *ctx, deploy_status_t *status) {
    if (!ctx || !status) return RELEASY_ERROR;
    *status = ctx->status;
    return RELEASY_SUCCESS;
}

const char *deploy_status_string(deploy_status_t status) {
    switch (status) {
        case DEPLOY_STATUS_NONE:
            return "None";
        case DEPLOY_STATUS_PENDING:
            return "Pending";
        case DEPLOY_STATUS_RUNNING:
            return "Running";
        case DEPLOY_STATUS_SUCCESS:
            return "Success";
        case DEPLOY_STATUS_FAILED:
            return "Failed";
        case DEPLOY_STATUS_ROLLED_BACK:
            return "Rolled Back";
        default:
            return "Unknown";
    }
}

const char *deploy_error_string(int error_code) {
    switch (error_code) {
        case RELEASY_SUCCESS:
            return "Success";
        case RELEASY_ERROR:
            return "General error";
        case DEPLOY_ERR_CONFIG_NOT_FOUND:
            return "Configuration file not found";
        case DEPLOY_ERR_INVALID_CONFIG:
            return "Invalid configuration";
        case DEPLOY_ERR_ENV_NOT_FOUND:
            return "Target environment not found";
        case DEPLOY_ERR_SCRIPT_FAILED:
            return "Script execution failed";
        case DEPLOY_ERR_HOOK_FAILED:
            return "Hook execution failed";
        case DEPLOY_ERR_STATUS_FAILED:
            return "Failed to get deployment status";
        case DEPLOY_ERR_ROLLBACK_FAILED:
            return "Rollback failed";
        default:
            return "Unknown error";
    }
}

void deploy_free_hooks(deploy_hook_t *hooks, int count) {
    if (!hooks || count <= 0) return;

    printf("Freeing %d hooks...\n", count);
    for (int i = 0; i < count; i++) {
        printf("Freeing hook %d...\n", i);
        if (hooks[i].id) {
            printf("Freeing hook id: %s\n", hooks[i].id);
            free(hooks[i].id);
            hooks[i].id = NULL;
        }
        if (hooks[i].name) {
            printf("Freeing hook name: %s\n", hooks[i].name);
            free(hooks[i].name);
            hooks[i].name = NULL;
        }
        if (hooks[i].description) {
            printf("Freeing hook description: %s\n", hooks[i].description);
            free(hooks[i].description);
            hooks[i].description = NULL;
        }
        if (hooks[i].script) {
            printf("Freeing hook script: %s\n", hooks[i].script);
            free(hooks[i].script);
            hooks[i].script = NULL;
        }
        if (hooks[i].working_dir) {
            printf("Freeing hook working dir: %s\n", hooks[i].working_dir);
            free(hooks[i].working_dir);
            hooks[i].working_dir = NULL;
        }
        if (hooks[i].env) {
            printf("Freeing %d hook env vars...\n", hooks[i].env_count);
            for (int j = 0; j < hooks[i].env_count; j++) {
                if (hooks[i].env[j]) {
                    printf("Freeing hook env var: %s\n", hooks[i].env[j]);
                    free(hooks[i].env[j]);
                    hooks[i].env[j] = NULL;
                }
            }
            free(hooks[i].env);
            hooks[i].env = NULL;
        }
        hooks[i].env_count = 0;
    }
    printf("Hook cleanup complete\n");
}

void deploy_free_target(deploy_target_t *target) {
    if (!target) return;

    printf("Freeing target...\n");
    if (target->name) {
        printf("Freeing target name: %s\n", target->name);
        free(target->name);
        target->name = NULL;
    }
    if (target->description) {
        printf("Freeing target description: %s\n", target->description);
        free(target->description);
        target->description = NULL;
    }
    if (target->script_path) {
        printf("Freeing target script path: %s\n", target->script_path);
        free(target->script_path);
        target->script_path = NULL;
    }
    if (target->working_dir) {
        printf("Freeing target working dir: %s\n", target->working_dir);
        free(target->working_dir);
        target->working_dir = NULL;
    }
    if (target->status_file) {
        printf("Freeing target status file: %s\n", target->status_file);
        free(target->status_file);
        target->status_file = NULL;
    }

    if (target->env_vars) {
        printf("Freeing %d target env vars...\n", target->env_count);
        for (int i = 0; i < target->env_count; i++) {
            if (target->env_vars[i]) {
                printf("Freeing target env var: %s\n", target->env_vars[i]);
                free(target->env_vars[i]);
                target->env_vars[i] = NULL;
            }
        }
        free(target->env_vars);
        target->env_vars = NULL;
    }
    target->env_count = 0;

    if (target->pre_hooks) {
        printf("Freeing pre-hooks...\n");
        deploy_free_hooks(target->pre_hooks, target->pre_hook_count);
        free(target->pre_hooks);
        target->pre_hooks = NULL;
    }
    target->pre_hook_count = 0;

    if (target->post_hooks) {
        printf("Freeing post-hooks...\n");
        deploy_free_hooks(target->post_hooks, target->post_hook_count);
        free(target->post_hooks);
        target->post_hooks = NULL;
    }
    target->post_hook_count = 0;

    printf("Target cleanup complete\n");
}

void deploy_cleanup(deploy_context_t *ctx) {
    if (!ctx) return;

    printf("Cleaning up deployment context...\n");

    if (ctx->config_path) {
        free(ctx->config_path);
        ctx->config_path = NULL;
    }

    if (ctx->log_path) {
        free(ctx->log_path);
        ctx->log_path = NULL;
    }

    if (ctx->current_version) {
        free(ctx->current_version);
        ctx->current_version = NULL;
    }

    if (ctx->previous_version) {
        free(ctx->previous_version);
        ctx->previous_version = NULL;
    }

    if (ctx->targets) {
        printf("Cleaning up %d targets...\n", ctx->target_count);
        for (int i = 0; i < ctx->target_count; i++) {
            deploy_target_t *target = &ctx->targets[i];
            deploy_free_target(target);
        }
        free(ctx->targets);
        ctx->targets = NULL;
    }
    ctx->target_count = 0;
    ctx->current_target = NULL;

    if (ctx->config) {
        json_object_put(ctx->config);
        ctx->config = NULL;
    }

    printf("Deployment context cleanup complete\n");
} 