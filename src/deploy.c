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
    if (!json_object_is_type(hook_obj, json_type_object)) return DEPLOY_ERR_INVALID_CONFIG;

    json_object *tmp;
    if (json_object_object_get_ex(hook_obj, "id", &tmp))
        hook->id = strdup(json_object_get_string(tmp));
    if (json_object_object_get_ex(hook_obj, "name", &tmp))
        hook->name = strdup(json_object_get_string(tmp));
    if (json_object_object_get_ex(hook_obj, "description", &tmp))
        hook->description = strdup(json_object_get_string(tmp));
    if (json_object_object_get_ex(hook_obj, "script", &tmp))
        hook->script = strdup(json_object_get_string(tmp));
    if (json_object_object_get_ex(hook_obj, "timeout", &tmp))
        hook->timeout = json_object_get_int(tmp);
    if (json_object_object_get_ex(hook_obj, "retry_count", &tmp))
        hook->retry_count = json_object_get_int(tmp);
    if (json_object_object_get_ex(hook_obj, "retry_delay", &tmp))
        hook->retry_delay = json_object_get_int(tmp);

    json_object *env_obj;
    if (json_object_object_get_ex(hook_obj, "env", &env_obj) &&
        json_object_is_type(env_obj, json_type_array)) {
        hook->env_count = json_object_array_length(env_obj);
        hook->env = calloc(hook->env_count, sizeof(char *));
        if (!hook->env) return RELEASY_ERROR;

        for (int i = 0; i < hook->env_count; i++) {
            json_object *env_item = json_object_array_get_idx(env_obj, i);
            hook->env[i] = strdup(json_object_get_string(env_item));
        }
    }

    return RELEASY_SUCCESS;
}

static int deploy_parse_target(json_object *target_obj, deploy_target_t *target) {
    if (!json_object_is_type(target_obj, json_type_object)) return DEPLOY_ERR_INVALID_CONFIG;

    json_object *tmp;
    if (json_object_object_get_ex(target_obj, "name", &tmp))
        target->name = strdup(json_object_get_string(tmp));
    if (json_object_object_get_ex(target_obj, "description", &tmp))
        target->description = strdup(json_object_get_string(tmp));
    if (json_object_object_get_ex(target_obj, "script_path", &tmp))
        target->script_path = strdup(json_object_get_string(tmp));
    if (json_object_object_get_ex(target_obj, "working_dir", &tmp))
        target->working_dir = strdup(json_object_get_string(tmp));
    if (json_object_object_get_ex(target_obj, "timeout", &tmp))
        target->timeout = json_object_get_int(tmp);
    if (json_object_object_get_ex(target_obj, "verify_ssl", &tmp))
        target->verify_ssl = json_object_get_boolean(tmp);
    if (json_object_object_get_ex(target_obj, "status_file", &tmp))
        target->status_file = strdup(json_object_get_string(tmp));

    json_object *env_obj;
    if (json_object_object_get_ex(target_obj, "env", &env_obj) &&
        json_object_is_type(env_obj, json_type_array)) {
        target->env_count = json_object_array_length(env_obj);
        target->env_vars = calloc(target->env_count, sizeof(char *));
        if (!target->env_vars) return RELEASY_ERROR;

        for (int i = 0; i < target->env_count; i++) {
            json_object *env_item = json_object_array_get_idx(env_obj, i);
            target->env_vars[i] = strdup(json_object_get_string(env_item));
        }
    }

    json_object *hooks_obj;
    if (json_object_object_get_ex(target_obj, "hooks", &hooks_obj) &&
        json_object_is_type(hooks_obj, json_type_object)) {
        json_object *pre_hooks, *post_hooks;
        
        if (json_object_object_get_ex(hooks_obj, "pre", &pre_hooks) &&
            json_object_is_type(pre_hooks, json_type_array)) {
            int count = json_object_array_length(pre_hooks);
            target->pre_hooks = calloc(count, sizeof(deploy_hook_t));
            if (!target->pre_hooks) return RELEASY_ERROR;

            for (int i = 0; i < count; i++) {
                json_object *hook = json_object_array_get_idx(pre_hooks, i);
                int ret = deploy_parse_hook(hook, &target->pre_hooks[i]);
                if (ret != RELEASY_SUCCESS) return ret;
            }
        }

        if (json_object_object_get_ex(hooks_obj, "post", &post_hooks) &&
            json_object_is_type(post_hooks, json_type_array)) {
            int count = json_object_array_length(post_hooks);
            target->post_hooks = calloc(count, sizeof(deploy_hook_t));
            if (!target->post_hooks) return RELEASY_ERROR;

            for (int i = 0; i < count; i++) {
                json_object *hook = json_object_array_get_idx(post_hooks, i);
                int ret = deploy_parse_hook(hook, &target->post_hooks[i]);
                if (ret != RELEASY_SUCCESS) return ret;
            }
        }
    }

    return RELEASY_SUCCESS;
}

int deploy_init(deploy_context_t *ctx) {
    if (!ctx) return RELEASY_ERROR;
    memset(ctx, 0, sizeof(deploy_context_t));
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
    if (json_object_object_get_ex(config_obj, "log_file", &tmp))
        ctx->config->log_file = strdup(json_object_get_string(tmp));
    if (json_object_object_get_ex(config_obj, "status_dir", &tmp))
        ctx->config->status_dir = strdup(json_object_get_string(tmp));
    if (json_object_object_get_ex(config_obj, "dry_run", &tmp))
        ctx->config->dry_run = json_object_get_boolean(tmp);
    if (json_object_object_get_ex(config_obj, "verbose", &tmp))
        ctx->config->verbose = json_object_get_boolean(tmp);

    json_object *targets_obj;
    if (json_object_object_get_ex(config_obj, "targets", &targets_obj) &&
        json_object_is_type(targets_obj, json_type_array)) {
        ctx->config->target_count = json_object_array_length(targets_obj);
        ctx->config->targets = calloc(ctx->config->target_count, sizeof(deploy_target_t));
        if (!ctx->config->targets) {
            json_object_put(config_obj);
            return RELEASY_ERROR;
        }

        for (int i = 0; i < ctx->config->target_count; i++) {
            json_object *target = json_object_array_get_idx(targets_obj, i);
            int ret = deploy_parse_target(target, &ctx->config->targets[i]);
            if (ret != RELEASY_SUCCESS) {
                json_object_put(config_obj);
                return ret;
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
    if (ctx->is_dry_run) {
        printf("[DRY RUN] Would execute script: %s\n", script);
        return RELEASY_SUCCESS;
    }

    pid_t pid = fork();
    if (pid < 0) return DEPLOY_ERR_SCRIPT_FAILED;

    if (pid == 0) {
        // Child process
        for (int i = 0; i < env_count; i++) {
            putenv(env[i]);
        }

        execl("/bin/sh", "sh", "-c", script, NULL);
        exit(1);
    }

    // Parent process
    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? RELEASY_SUCCESS : DEPLOY_ERR_SCRIPT_FAILED;
}

static int deploy_execute_hooks(deploy_context_t *ctx, deploy_hook_t *hooks, int count, const char *phase) {
    for (int i = 0; i < count; i++) {
        deploy_hook_t *hook = &hooks[i];
        if (ctx->config->verbose) {
            printf("Executing %s hook: %s\n", phase, hook->name);
        }

        int retries = 0;
        int ret;
        do {
            ret = deploy_execute_script(ctx, hook->script, hook->env, hook->env_count);
            if (ret == RELEASY_SUCCESS) break;
            
            if (retries < hook->retry_count) {
                if (ctx->config->verbose) {
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

    ctx->status = DEPLOY_STATUS_PENDING;
    free(ctx->current_version);
    ctx->current_version = strdup(version);

    if (ctx->config->verbose) {
        printf("Starting deployment of version %s to %s\n", 
               version, ctx->current_target->name);
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

    if (ctx->config->verbose) {
        printf("Rolling back from %s to %s\n", 
               ctx->current_version, ctx->previous_version);
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

static void deploy_free_hooks(deploy_hook_t *hooks, int count) {
    if (!hooks) return;
    
    for (int i = 0; i < count; i++) {
        free(hooks[i].id);
        free(hooks[i].name);
        free(hooks[i].description);
        free(hooks[i].script);
        for (int j = 0; j < hooks[i].env_count; j++) {
            free(hooks[i].env[j]);
        }
        free(hooks[i].env);
    }
    free(hooks);
}

static void deploy_free_target(deploy_target_t *target) {
    if (!target) return;
    
    free(target->name);
    free(target->description);
    free(target->script_path);
    free(target->working_dir);
    free(target->status_file);
    
    for (int i = 0; i < target->env_count; i++) {
        free(target->env_vars[i]);
    }
    free(target->env_vars);
    
    deploy_free_hooks(target->pre_hooks, target->hook_count);
    deploy_free_hooks(target->post_hooks, target->hook_count);
}

void deploy_cleanup(deploy_context_t *ctx) {
    if (!ctx) return;

    if (ctx->config) {
        free(ctx->config->log_file);
        free(ctx->config->status_dir);
        
        for (int i = 0; i < ctx->config->target_count; i++) {
            deploy_free_target(&ctx->config->targets[i]);
        }
        free(ctx->config->targets);
        free(ctx->config);
    }

    free(ctx->current_version);
    free(ctx->previous_version);
    free(ctx->log_path);
    
    memset(ctx, 0, sizeof(deploy_context_t));
} 