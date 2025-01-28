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
#include "deploy.h"
#include "ui.h"

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
    if (json_object_object_get_ex(hook_obj, "timeout", &tmp) && tmp)
        hook->timeout = json_object_get_int(tmp);
    if (json_object_object_get_ex(hook_obj, "retry_count", &tmp) && tmp)
        hook->retry_count = json_object_get_int(tmp);
    if (json_object_object_get_ex(hook_obj, "retry_delay", &tmp) && tmp)
        hook->retry_delay = json_object_get_int(tmp);

    // Set default values if not specified
    if (!hook->timeout) hook->timeout = 60;
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
    if (json_object_object_get_ex(target_obj, "timeout", &tmp) && tmp)
        target->timeout = json_object_get_int(tmp);
    if (json_object_object_get_ex(target_obj, "verify_ssl", &tmp) && tmp)
        target->verify_ssl = json_object_get_boolean(tmp);
    if (json_object_object_get_ex(target_obj, "status_file", &tmp) && tmp)
        target->status_file = strdup(json_object_get_string(tmp));

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
            int count = json_object_array_length(pre_hooks);
            if (count > 0) {
                target->pre_hooks = calloc(count, sizeof(deploy_hook_t));
                if (!target->pre_hooks) return RELEASY_ERROR;

                for (int i = 0; i < count; i++) {
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
                            return ret;
                        }
                    }
                }
                target->hook_count = count;
            }
        }

        // Parse post-hooks
        if (json_object_object_get_ex(hooks_obj, "post", &post_hooks) &&
            json_object_is_type(post_hooks, json_type_array)) {
            int count = json_object_array_length(post_hooks);
            if (count > 0) {
                target->post_hooks = calloc(count, sizeof(deploy_hook_t));
                if (!target->post_hooks) {
                    if (target->pre_hooks) {
                        deploy_free_hooks(target->pre_hooks, target->hook_count);
                        free(target->pre_hooks);
                        target->pre_hooks = NULL;
                    }
                    return RELEASY_ERROR;
                }

                for (int i = 0; i < count; i++) {
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
                            if (target->pre_hooks) {
                                deploy_free_hooks(target->pre_hooks, target->hook_count);
                                free(target->pre_hooks);
                                target->pre_hooks = NULL;
                            }
                            return ret;
                        }
                    }
                }
                target->hook_count = count;
            }
        }
    }

    return RELEASY_SUCCESS;
}

int deploy_init(deploy_context_t *ctx) {
    if (!ctx) return RELEASY_ERROR;
    memset(ctx, 0, sizeof(deploy_context_t));
    ctx->status = DEPLOY_STATUS_NONE;
    return RELEASY_SUCCESS;
}

int deploy_load_config(deploy_context_t *ctx, const char *config_path) {
    if (!ctx || !config_path) return RELEASY_ERROR;

    json_object *config_obj = json_object_from_file(config_path);
    if (!config_obj) return DEPLOY_ERR_CONFIG_NOT_FOUND;

    ctx->config = calloc(1, sizeof(deploy_config_t));
    if (!ctx->config) {
        json_object_put(config_obj);
        return RELEASY_ERROR;
    }

    json_object *tmp;
    if (json_object_object_get_ex(config_obj, "log_file", &tmp) && tmp)
        ctx->config->log_file = strdup(json_object_get_string(tmp));
    if (json_object_object_get_ex(config_obj, "status_dir", &tmp) && tmp)
        ctx->config->status_dir = strdup(json_object_get_string(tmp));
    if (json_object_object_get_ex(config_obj, "dry_run", &tmp) && tmp)
        ctx->config->dry_run = json_object_get_boolean(tmp);
    if (json_object_object_get_ex(config_obj, "verbose", &tmp) && tmp)
        ctx->config->verbose = json_object_get_boolean(tmp);

    json_object *targets_obj;
    if (json_object_object_get_ex(config_obj, "targets", &targets_obj) &&
        json_object_is_type(targets_obj, json_type_array)) {
        ctx->config->target_count = json_object_array_length(targets_obj);
        if (ctx->config->target_count > 0) {
            ctx->config->targets = calloc(ctx->config->target_count, sizeof(deploy_target_t));
            if (!ctx->config->targets) {
                json_object_put(config_obj);
                free(ctx->config->log_file);
                free(ctx->config->status_dir);
                free(ctx->config);
                ctx->config = NULL;
                return RELEASY_ERROR;
            }

            for (int i = 0; i < ctx->config->target_count; i++) {
                json_object *target = json_object_array_get_idx(targets_obj, i);
                if (target) {
                    int ret = deploy_parse_target(target, &ctx->config->targets[i]);
                    if (ret != RELEASY_SUCCESS) {
                        // Clean up on failure
                        for (int j = 0; j < i; j++) {
                            deploy_free_target(&ctx->config->targets[j]);
                        }
                        free(ctx->config->targets);
                        free(ctx->config->log_file);
                        free(ctx->config->status_dir);
                        free(ctx->config);
                        ctx->config = NULL;
                        json_object_put(config_obj);
                        return ret;
                    }
                }
            }
        }
    }

    json_object_put(config_obj);
    return RELEASY_SUCCESS;
}

int deploy_set_target(deploy_context_t *ctx, const char *target_name) {
    if (!ctx || !ctx->config || !target_name) return RELEASY_ERROR;

    for (int i = 0; i < ctx->config->target_count; i++) {
        if (strcmp(ctx->config->targets[i].name, target_name) == 0) {
            ctx->current_target = &ctx->config->targets[i];
            return RELEASY_SUCCESS;
        }
    }

    return DEPLOY_ERR_ENV_NOT_FOUND;
}

static int deploy_execute_script(deploy_context_t *ctx, const char *script, char **env, int env_count) {
    if (!ctx || !script) return DEPLOY_ERR_SCRIPT_FAILED;

    if (ctx->is_dry_run) {
        printf("[DRY RUN] Would execute script: %s\n", script);
        return RELEASY_SUCCESS;
    }

    pid_t pid = fork();
    if (pid < 0) return DEPLOY_ERR_SCRIPT_FAILED;

    if (pid == 0) {
        // Child process
        if (env && env_count > 0) {
            for (int i = 0; i < env_count; i++) {
                if (env[i]) {
                    char *env_copy = strdup(env[i]);
                    if (env_copy) {
                        putenv(env_copy);  // Let the system clean this up
                    }
                }
            }
        }

        execl("/bin/sh", "sh", "-c", script, NULL);
        _exit(1);  // Use _exit in child process
    }

    // Parent process
    int status;
    while (waitpid(pid, &status, 0) == -1) {
        if (errno != EINTR) {
            return DEPLOY_ERR_SCRIPT_FAILED;
        }
    }
    
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? RELEASY_SUCCESS : DEPLOY_ERR_SCRIPT_FAILED;
}

static int deploy_execute_hooks(deploy_context_t *ctx, deploy_hook_t *hooks, int count, const char *phase) {
    if (!ctx || !hooks || count <= 0 || !phase) return RELEASY_SUCCESS;  // No hooks to execute is not an error

    for (int i = 0; i < count; i++) {
        deploy_hook_t *hook = &hooks[i];
        if (!hook->script) continue;  // Skip hooks without scripts

        if (ctx->config && ctx->config->verbose) {
            printf("Executing %s hook: %s\n", phase, hook->name ? hook->name : "unnamed");
        }

        int retries = 0;
        int ret;
        do {
            ret = deploy_execute_script(ctx, hook->script, hook->env, hook->env_count);
            if (ret == RELEASY_SUCCESS) break;
            
            if (retries < hook->retry_count) {
                if (ctx->config && ctx->config->verbose) {
                    printf("Hook failed, retrying in %d seconds...\n", hook->retry_delay);
                }
                sleep(hook->retry_delay);
            }
            retries++;
        } while (retries <= hook->retry_count);

        if (ret != RELEASY_SUCCESS) return DEPLOY_ERR_HOOK_FAILED;
    }

    return RELEASY_SUCCESS;
}

int deploy_execute(deploy_context_t *ctx, const char *version) {
    if (!ctx || !ctx->current_target || !version) return RELEASY_ERROR;

    // Save previous version before updating
    free(ctx->previous_version);
    ctx->previous_version = ctx->current_version ? strdup(ctx->current_version) : NULL;

    ctx->status = DEPLOY_STATUS_PENDING;
    free(ctx->current_version);
    ctx->current_version = strdup(version);
    if (!ctx->current_version) {
        ctx->status = DEPLOY_STATUS_FAILED;
        return RELEASY_ERROR;
    }

    if (ctx->config && ctx->config->verbose) {
        printf("Starting deployment of version %s to %s\n", 
               version, ctx->current_target->name ? ctx->current_target->name : "unknown");
    }

    // Execute pre-deployment hooks
    int ret = deploy_execute_hooks(ctx, ctx->current_target->pre_hooks, 
                                 ctx->current_target->hook_count, "pre-deploy");
    if (ret != RELEASY_SUCCESS) {
        ctx->status = DEPLOY_STATUS_FAILED;
        return ret;
    }

    // Execute main deployment script
    ctx->status = DEPLOY_STATUS_RUNNING;
    if (!ctx->current_target->script_path) {
        ctx->status = DEPLOY_STATUS_FAILED;
        return DEPLOY_ERR_SCRIPT_FAILED;
    }

    ret = deploy_execute_script(ctx, ctx->current_target->script_path,
                              ctx->current_target->env_vars,
                              ctx->current_target->env_count);
    if (ret != RELEASY_SUCCESS) {
        ctx->status = DEPLOY_STATUS_FAILED;
        return ret;
    }

    // Execute post-deployment hooks
    ret = deploy_execute_hooks(ctx, ctx->current_target->post_hooks,
                             ctx->current_target->hook_count, "post-deploy");
    if (ret != RELEASY_SUCCESS) {
        ctx->status = DEPLOY_STATUS_FAILED;
        return ret;
    }

    ctx->status = DEPLOY_STATUS_SUCCESS;
    return RELEASY_SUCCESS;
}

int deploy_rollback(deploy_context_t *ctx) {
    if (!ctx || !ctx->current_target || !ctx->previous_version) return RELEASY_ERROR;

    if (ctx->config && ctx->config->verbose) {
        printf("Rolling back from %s to %s\n", 
               ctx->current_version ? ctx->current_version : "unknown",
               ctx->previous_version);
    }

    int ret = deploy_execute(ctx, ctx->previous_version);
    if (ret == RELEASY_SUCCESS) {
        ctx->status = DEPLOY_STATUS_ROLLED_BACK;
    }

    return ret;
}

int deploy_get_status(deploy_context_t *ctx, deploy_status_t *status) {
    if (!ctx || !status) return RELEASY_ERROR;
    *status = ctx->status;
    return RELEASY_SUCCESS;
}

const char *deploy_status_string(deploy_status_t status) {
    switch (status) {
        case DEPLOY_STATUS_NONE:
            return "Not started";
        case DEPLOY_STATUS_PENDING:
            return "Pending";
        case DEPLOY_STATUS_RUNNING:
            return "Running";
        case DEPLOY_STATUS_SUCCESS:
            return "Success";
        case DEPLOY_STATUS_FAILED:
            return "Failed";
        case DEPLOY_STATUS_ROLLED_BACK:
            return "Rolled back";
        default:
            return "Unknown";
    }
}

const char *deploy_error_string(int error_code) {
    switch (error_code) {
        case RELEASY_SUCCESS:
            return "Success";
        case DEPLOY_ERR_CONFIG_NOT_FOUND:
            return "Configuration file not found";
        case DEPLOY_ERR_INVALID_CONFIG:
            return "Invalid configuration format";
        case DEPLOY_ERR_ENV_NOT_FOUND:
            return "Target environment not found";
        case DEPLOY_ERR_SCRIPT_FAILED:
            return "Deployment script failed";
        case DEPLOY_ERR_HOOK_FAILED:
            return "Deployment hook failed";
        case DEPLOY_ERR_STATUS_FAILED:
            return "Failed to update deployment status";
        case DEPLOY_ERR_ROLLBACK_FAILED:
            return "Rollback failed";
        default:
            return "Unknown error";
    }
}

void deploy_free_hooks(deploy_hook_t *hooks, int count) {
    if (!hooks || count <= 0) return;
    
    for (int i = 0; i < count; i++) {
        free(hooks[i].id);
        free(hooks[i].name);
        free(hooks[i].description);
        free(hooks[i].script);
        if (hooks[i].env) {
            for (int j = 0; j < hooks[i].env_count; j++) {
                free(hooks[i].env[j]);
            }
            free(hooks[i].env);
        }
    }
}

void deploy_free_target(deploy_target_t *target) {
    if (!target) return;
    
    free(target->name);
    free(target->description);
    free(target->script_path);
    free(target->working_dir);
    free(target->status_file);
    
    if (target->env_vars) {
        for (int i = 0; i < target->env_count; i++) {
            free(target->env_vars[i]);
        }
        free(target->env_vars);
    }
    
    if (target->pre_hooks) {
        deploy_free_hooks(target->pre_hooks, target->hook_count);
        free(target->pre_hooks);
    }
    if (target->post_hooks) {
        deploy_free_hooks(target->post_hooks, target->hook_count);
        free(target->post_hooks);
    }
}

void deploy_cleanup(deploy_context_t *ctx) {
    if (!ctx) return;

    if (ctx->config) {
        free(ctx->config->log_file);
        free(ctx->config->status_dir);
        
        if (ctx->config->targets) {
            for (int i = 0; i < ctx->config->target_count; i++) {
                deploy_free_target(&ctx->config->targets[i]);
            }
            free(ctx->config->targets);
        }
        free(ctx->config);
    }

    free(ctx->current_version);
    free(ctx->previous_version);
    free(ctx->log_path);
    
    memset(ctx, 0, sizeof(deploy_context_t));
} 