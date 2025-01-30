#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>
#include "release.h"

static char *json_get_string(json_object *obj, const char *key) {
    json_object *value;
    if (!json_object_object_get_ex(obj, key, &value)) return NULL;
    const char *str = json_object_get_string(value);
    return str ? strdup(str) : NULL;
}

static int json_get_bool(json_object *obj, const char *key, int default_value) {
    json_object *value;
    if (!json_object_object_get_ex(obj, key, &value)) return default_value;
    return json_object_get_boolean(value);
}

static char **json_get_string_array(json_object *obj, const char *key, size_t *count) {
    json_object *array;
    if (!json_object_object_get_ex(obj, key, &array)) return NULL;
    if (!json_object_is_type(array, json_type_array)) return NULL;
    
    size_t len = json_object_array_length(array);
    if (len == 0) return NULL;
    
    char **result = calloc(len, sizeof(char *));
    if (!result) return NULL;
    
    size_t valid_count = 0;
    for (size_t i = 0; i < len; i++) {
        json_object *item = json_object_array_get_idx(array, i);
        if (!item) continue;
        
        const char *str = json_object_get_string(item);
        if (!str) continue;
        
        result[valid_count] = strdup(str);
        if (!result[valid_count]) {
            for (size_t j = 0; j < valid_count; j++) free(result[j]);
            free(result);
            return NULL;
        }
        valid_count++;
    }
    
    *count = valid_count;
    return result;
}

int release_load_config(release_config_t *config, const char *config_file) {
    if (!config || !config_file) return RELEASY_ERROR;
    
    json_object *root = json_object_from_file(config_file);
    if (!root) return RELEASE_ERR_FILE_ACCESS;
    
    // Load basic configuration
    config->name = json_get_string(root, "name");
    config->description = json_get_string(root, "description");
    config->target_branch = json_get_string(root, "target_branch");
    config->build_command = json_get_string(root, "build_command");
    config->test_command = json_get_string(root, "test_command");
    config->deploy_command = json_get_string(root, "deploy_command");
    config->publish_command = json_get_string(root, "publish_command");
    config->notify_command = json_get_string(root, "notify_command");
    
    // Load release type
    json_object *type_obj;
    if (json_object_object_get_ex(root, "type", &type_obj)) {
        const char *type_str = json_object_get_string(type_obj);
        if (type_str) {
            if (strcmp(type_str, "major") == 0) config->type = RELEASE_TYPE_MAJOR;
            else if (strcmp(type_str, "minor") == 0) config->type = RELEASE_TYPE_MINOR;
            else if (strcmp(type_str, "patch") == 0) config->type = RELEASE_TYPE_PATCH;
            else if (strcmp(type_str, "prerelease") == 0) config->type = RELEASE_TYPE_PRERELEASE;
            else config->type = RELEASE_TYPE_CUSTOM;
        }
    }
    
    // Load arrays
    config->artifacts = json_get_string_array(root, "artifacts", &config->artifact_count);
    config->environments = json_get_string_array(root, "environments", &config->env_count);
    
    // Load flags
    config->create_changelog = json_get_bool(root, "create_changelog", 1);
    config->create_tag = json_get_bool(root, "create_tag", 1);
    config->push_changes = json_get_bool(root, "push_changes", 1);
    config->notify_on_success = json_get_bool(root, "notify_on_success", 1);
    config->notify_on_failure = json_get_bool(root, "notify_on_failure", 1);
    
    json_object_put(root);
    return RELEASY_SUCCESS;
}

int release_save_config(release_config_t *config, const char *config_file) {
    if (!config || !config_file) return RELEASY_ERROR;
    
    json_object *root = json_object_new_object();
    if (!root) return RELEASE_ERR_MEMORY;
    
    // Save basic configuration
    if (config->name)
        json_object_object_add(root, "name", json_object_new_string(config->name));
    if (config->description)
        json_object_object_add(root, "description", json_object_new_string(config->description));
    if (config->target_branch)
        json_object_object_add(root, "target_branch", json_object_new_string(config->target_branch));
    if (config->build_command)
        json_object_object_add(root, "build_command", json_object_new_string(config->build_command));
    if (config->test_command)
        json_object_object_add(root, "test_command", json_object_new_string(config->test_command));
    if (config->deploy_command)
        json_object_object_add(root, "deploy_command", json_object_new_string(config->deploy_command));
    if (config->publish_command)
        json_object_object_add(root, "publish_command", json_object_new_string(config->publish_command));
    if (config->notify_command)
        json_object_object_add(root, "notify_command", json_object_new_string(config->notify_command));
    
    // Save release type
    const char *type_str = NULL;
    switch (config->type) {
        case RELEASE_TYPE_MAJOR: type_str = "major"; break;
        case RELEASE_TYPE_MINOR: type_str = "minor"; break;
        case RELEASE_TYPE_PATCH: type_str = "patch"; break;
        case RELEASE_TYPE_PRERELEASE: type_str = "prerelease"; break;
        case RELEASE_TYPE_CUSTOM: type_str = "custom"; break;
    }
    if (type_str)
        json_object_object_add(root, "type", json_object_new_string(type_str));
    
    // Save arrays
    if (config->artifacts && config->artifact_count > 0) {
        json_object *artifacts = json_object_new_array();
        for (size_t i = 0; i < config->artifact_count; i++) {
            if (config->artifacts[i])
                json_object_array_add(artifacts, json_object_new_string(config->artifacts[i]));
        }
        json_object_object_add(root, "artifacts", artifacts);
    }
    
    if (config->environments && config->env_count > 0) {
        json_object *environments = json_object_new_array();
        for (size_t i = 0; i < config->env_count; i++) {
            if (config->environments[i])
                json_object_array_add(environments, json_object_new_string(config->environments[i]));
        }
        json_object_object_add(root, "environments", environments);
    }
    
    // Save flags
    json_object_object_add(root, "create_changelog", json_object_new_boolean(config->create_changelog));
    json_object_object_add(root, "create_tag", json_object_new_boolean(config->create_tag));
    json_object_object_add(root, "push_changes", json_object_new_boolean(config->push_changes));
    json_object_object_add(root, "notify_on_success", json_object_new_boolean(config->notify_on_success));
    json_object_object_add(root, "notify_on_failure", json_object_new_boolean(config->notify_on_failure));
    
    // Save to file
    if (json_object_to_file(config_file, root) == -1) {
        json_object_put(root);
        return RELEASE_ERR_FILE_ACCESS;
    }
    
    json_object_put(root);
    return RELEASY_SUCCESS;
} 