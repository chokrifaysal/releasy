#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <json-c/json.h>
#include "init.h"
#include "deploy.h"
#include "ui.h"

static int create_directory(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        if (mkdir(path, 0755) == -1) {
            printf("Error creating directory %s: %s\n", path, strerror(errno));
            return RELEASY_ERROR;
        }
        printf("Created directory: %s\n", path);
    }
    return RELEASY_SUCCESS;
}

static int create_default_config(const char *config_path, const char *user_name, const char *user_email) {
    json_object *config = json_object_new_object();
    if (!config) return RELEASY_ERROR;

    // Add basic configuration
    json_object_object_add(config, "log_path", json_object_new_string("logs/releasy.log"));
    json_object_object_add(config, "dry_run", json_object_new_boolean(false));
    json_object_object_add(config, "verbose", json_object_new_boolean(true));

    // Create targets array
    json_object *targets = json_object_new_array();
    if (!targets) {
        json_object_put(config);
        return RELEASY_ERROR;
    }

    // Add staging target
    json_object *staging = json_object_new_object();
    if (!staging) {
        json_object_put(config);
        return RELEASY_ERROR;
    }

    json_object_object_add(staging, "name", json_object_new_string("staging"));
    json_object_object_add(staging, "description", json_object_new_string("Staging environment"));
    json_object_object_add(staging, "script_path", json_object_new_string("scripts/deploy-staging.sh"));
    json_object_object_add(staging, "working_dir", json_object_new_string("."));
    json_object_object_add(staging, "status_file", json_object_new_string("status/staging.json"));
    json_object_object_add(staging, "timeout", json_object_new_int(300));
    json_object_object_add(staging, "verify_ssl", json_object_new_boolean(true));

    // Add environment variables
    json_object *env_vars = json_object_new_array();
    json_object_array_add(env_vars, json_object_new_string("DEPLOY_ENV=staging"));
    json_object_array_add(env_vars, json_object_new_string("APP_DEBUG=true"));
    json_object_object_add(staging, "env_vars", env_vars);

    // Add hooks
    json_object *hooks = json_object_new_object();
    json_object *pre_hooks = json_object_new_array();
    json_object *post_hooks = json_object_new_array();

    // Add pre-deployment hook
    json_object *pre_hook = json_object_new_object();
    json_object_object_add(pre_hook, "id", json_object_new_string("test"));
    json_object_object_add(pre_hook, "name", json_object_new_string("Test deployment"));
    json_object_object_add(pre_hook, "description", json_object_new_string("Run test deployment"));
    json_object_object_add(pre_hook, "script", json_object_new_string("scripts/test-deployment.sh"));
    json_object_object_add(pre_hook, "working_dir", json_object_new_string("."));
    json_object_object_add(pre_hook, "timeout", json_object_new_int(60));
    json_object_object_add(pre_hook, "retry_count", json_object_new_int(3));
    json_object_object_add(pre_hook, "retry_delay", json_object_new_int(5));

    json_object *pre_hook_env = json_object_new_array();
    json_object_array_add(pre_hook_env, json_object_new_string("DEPLOY_TEST=true"));
    json_object_object_add(pre_hook, "env", pre_hook_env);
    json_object_array_add(pre_hooks, pre_hook);

    // Add post-deployment hook
    json_object *post_hook = json_object_new_object();
    json_object_object_add(post_hook, "id", json_object_new_string("notify"));
    json_object_object_add(post_hook, "name", json_object_new_string("Send notification"));
    json_object_object_add(post_hook, "description", json_object_new_string("Notify about deployment"));
    json_object_object_add(post_hook, "script", json_object_new_string("scripts/notify.sh"));
    json_object_object_add(post_hook, "working_dir", json_object_new_string("."));
    json_object_object_add(post_hook, "timeout", json_object_new_int(30));
    json_object_object_add(post_hook, "retry_count", json_object_new_int(2));
    json_object_object_add(post_hook, "retry_delay", json_object_new_int(5));

    json_object *post_hook_env = json_object_new_array();
    json_object_array_add(post_hook_env, json_object_new_string("DEPLOY_NOTIFY=true"));
    json_object_object_add(post_hook, "env", post_hook_env);
    json_object_array_add(post_hooks, post_hook);

    json_object_object_add(hooks, "pre", pre_hooks);
    json_object_object_add(hooks, "post", post_hooks);
    json_object_object_add(staging, "hooks", hooks);

    json_object_array_add(targets, staging);
    json_object_object_add(config, "targets", targets);

    // Write configuration to file
    const char *json_str = json_object_to_json_string_ext(config, JSON_C_TO_STRING_PRETTY);
    if (!json_str) {
        json_object_put(config);
        return RELEASY_ERROR;
    }

    FILE *f = fopen(config_path, "w");
    if (!f) {
        printf("Error creating config file %s: %s\n", config_path, strerror(errno));
        json_object_put(config);
        return RELEASY_ERROR;
    }

    fprintf(f, "%s\n", json_str);
    fclose(f);
    json_object_put(config);

    printf("Created default configuration: %s\n", config_path);
    return RELEASY_SUCCESS;
}

static int create_script_templates(void) {
    const char *deploy_script = 
        "#!/bin/bash\n\n"
        "echo \"Deploying to staging environment...\"\n"
        "echo \"Environment variables:\"\n"
        "env | grep \"^DEPLOY_\"\n"
        "echo \"Working directory: $(pwd)\"\n"
        "echo \"Deployment successful\"\n"
        "exit 0\n";

    const char *test_script =
        "#!/bin/bash\n\n"
        "echo \"Running deployment tests...\"\n"
        "echo \"Test environment: $DEPLOY_ENV\"\n"
        "echo \"Tests passed\"\n"
        "exit 0\n";

    const char *notify_script =
        "#!/bin/bash\n\n"
        "echo \"Sending deployment notification...\"\n"
        "echo \"Notification sent\"\n"
        "exit 0\n";

    // Create scripts directory
    if (create_directory("scripts") != RELEASY_SUCCESS) {
        return RELEASY_ERROR;
    }

    // Create script files
    FILE *f;
    const char *scripts[] = {
        "scripts/deploy-staging.sh",
        "scripts/test-deployment.sh",
        "scripts/notify.sh"
    };
    const char *contents[] = {
        deploy_script,
        test_script,
        notify_script
    };

    for (int i = 0; i < 3; i++) {
        f = fopen(scripts[i], "w");
        if (!f) {
            printf("Error creating script %s: %s\n", scripts[i], strerror(errno));
            return RELEASY_ERROR;
        }
        fprintf(f, "%s", contents[i]);
        fclose(f);
        chmod(scripts[i], 0755);
        printf("Created script: %s\n", scripts[i]);
    }

    return RELEASY_SUCCESS;
}

int init_project(const char *config_path, const char *user_name, const char *user_email) {
    printf("Initializing releasy project...\n");

    // Create required directories
    const char *dirs[] = {
        "config",
        "logs",
        "status",
        "scripts"
    };

    for (int i = 0; i < 4; i++) {
        if (create_directory(dirs[i]) != RELEASY_SUCCESS) {
            return RELEASY_ERROR;
        }
    }

    // Create default configuration
    if (create_default_config(config_path, user_name, user_email) != RELEASY_SUCCESS) {
        return RELEASY_ERROR;
    }

    // Create script templates
    if (create_script_templates() != RELEASY_SUCCESS) {
        return RELEASY_ERROR;
    }

    printf("Project initialized successfully\n");
    return RELEASY_SUCCESS;
} 