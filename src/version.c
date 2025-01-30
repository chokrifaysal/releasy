#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "version.h"
#include "git_ops.h"

int version_init(version_info_t *info) {
    if (!info) return RELEASY_ERROR;
    memset(info, 0, sizeof(version_info_t));
    return RELEASY_SUCCESS;
}

int version_load(version_info_t *info, const char *version_file) {
    if (!info || !version_file) return RELEASY_ERROR;
    
    FILE *f = fopen(version_file, "r");
    if (!f) return VERSION_ERR_FILE_ACCESS;
    
    char line[256];
    char key[64], value[192];
    
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%63[^=]=%191[^\n]", key, value) == 2) {
            // Trim whitespace
            char *k = key, *v = value;
            while (isspace(*k)) k++;
            while (isspace(*v)) v++;
            char *end = k + strlen(k) - 1;
            while (end > k && isspace(*end)) *end-- = '\0';
            end = v + strlen(v) - 1;
            while (end > v && isspace(*end)) *end-- = '\0';
            
            if (strcmp(k, "current_version") == 0) {
                info->current_version = strdup(v);
            } else if (strcmp(k, "previous_version") == 0) {
                info->previous_version = strdup(v);
            } else if (strcmp(k, "next_version") == 0) {
                info->next_version = strdup(v);
            } else if (strcmp(k, "release_branch") == 0) {
                info->release_branch = strdup(v);
            } else if (strcmp(k, "is_prerelease") == 0) {
                info->is_prerelease = atoi(v);
            } else if (strcmp(k, "prerelease_label") == 0) {
                info->prerelease_label = strdup(v);
            } else if (strcmp(k, "build_number") == 0) {
                info->build_number = atoi(v);
            }
        }
    }
    
    fclose(f);
    return RELEASY_SUCCESS;
}

int version_save(version_info_t *info, const char *version_file) {
    if (!info || !version_file) return RELEASY_ERROR;
    
    FILE *f = fopen(version_file, "w");
    if (!f) return VERSION_ERR_FILE_ACCESS;
    
    if (info->current_version)
        fprintf(f, "current_version=%s\n", info->current_version);
    if (info->previous_version)
        fprintf(f, "previous_version=%s\n", info->previous_version);
    if (info->next_version)
        fprintf(f, "next_version=%s\n", info->next_version);
    if (info->release_branch)
        fprintf(f, "release_branch=%s\n", info->release_branch);
    fprintf(f, "is_prerelease=%d\n", info->is_prerelease);
    if (info->prerelease_label)
        fprintf(f, "prerelease_label=%s\n", info->prerelease_label);
    fprintf(f, "build_number=%d\n", info->build_number);
    
    fclose(f);
    return RELEASY_SUCCESS;
}

int version_increment(version_info_t *info, version_increment_t increment_type, const char *custom_version) {
    if (!info || !info->current_version) return RELEASY_ERROR;
    
    // Parse current version
    semver_t current;
    if (semver_parse(info->current_version, &current) != 0) {
        return VERSION_ERR_INVALID_FORMAT;
    }
    
    // Store previous version
    free(info->previous_version);
    info->previous_version = strdup(info->current_version);
    if (!info->previous_version) {
        semver_free(&current);
        return VERSION_ERR_MEMORY;
    }
    
    // Handle custom version
    if (increment_type == VERSION_INC_CUSTOM) {
        if (!custom_version) {
            semver_free(&current);
            return VERSION_ERR_INVALID_INCREMENT;
        }
        
        semver_t custom;
        if (semver_parse(custom_version, &custom) != 0) {
            semver_free(&current);
            return VERSION_ERR_INVALID_FORMAT;
        }
        
        // Ensure new version is greater than current
        if (semver_compare(&custom, &current) <= 0) {
            semver_free(&current);
            semver_free(&custom);
            return VERSION_ERR_INVALID_INCREMENT;
        }
        
        free(info->current_version);
        info->current_version = strdup(custom_version);
        
        semver_free(&current);
        semver_free(&custom);
        return RELEASY_SUCCESS;
    }
    
    // Increment version based on type
    switch (increment_type) {
        case VERSION_INC_MAJOR:
            current.major++;
            current.minor = 0;
            current.patch = 0;
            break;
        case VERSION_INC_MINOR:
            current.minor++;
            current.patch = 0;
            break;
        case VERSION_INC_PATCH:
            current.patch++;
            break;
        default:
            semver_free(&current);
            return VERSION_ERR_INVALID_INCREMENT;
    }
    
    // Format new version
    char new_version[32];
    snprintf(new_version, sizeof(new_version), "%d.%d.%d",
             current.major, current.minor, current.patch);
    
    free(info->current_version);
    info->current_version = strdup(new_version);
    if (!info->current_version) {
        semver_free(&current);
        return VERSION_ERR_MEMORY;
    }
    
    semver_free(&current);
    return RELEASY_SUCCESS;
}

int version_create_release_branch(git_repository *repo, version_info_t *info) {
    if (!repo || !info || !info->current_version) return RELEASY_ERROR;
    
    // Format branch name
    char branch_name[64];
    snprintf(branch_name, sizeof(branch_name), "release/%s", info->current_version);
    
    // Check if branch already exists
    git_reference *branch_ref = NULL;
    if (git_branch_lookup(&branch_ref, repo, branch_name, GIT_BRANCH_LOCAL) == 0) {
        git_reference_free(branch_ref);
        return VERSION_ERR_BRANCH_EXISTS;
    }
    
    // Get HEAD commit
    git_oid head_oid;
    int error = git_reference_name_to_id(&head_oid, repo, "HEAD");
    if (error) return VERSION_ERR_GIT_OPERATION;
    
    git_commit *head_commit = NULL;
    error = git_commit_lookup(&head_commit, repo, &head_oid);
    if (error) return VERSION_ERR_GIT_OPERATION;
    
    // Create branch
    git_reference *new_branch = NULL;
    error = git_branch_create(&new_branch, repo, branch_name, head_commit, 0);
    
    git_commit_free(head_commit);
    if (new_branch) git_reference_free(new_branch);
    
    if (error) return VERSION_ERR_GIT_OPERATION;
    
    // Store branch name
    free(info->release_branch);
    info->release_branch = strdup(branch_name);
    if (!info->release_branch) return VERSION_ERR_MEMORY;
    
    return RELEASY_SUCCESS;
}

int version_create_tag(git_repository *repo, version_info_t *info, const char *message) {
    if (!repo || !info || !info->current_version) return RELEASY_ERROR;
    
    // Format tag name
    char tag_name[32];
    snprintf(tag_name, sizeof(tag_name), "v%s", info->current_version);
    
    // Check if tag already exists
    git_reference *tag_ref = NULL;
    char tag_ref_name[64];
    snprintf(tag_ref_name, sizeof(tag_ref_name), "refs/tags/%s", tag_name);
    if (git_reference_lookup(&tag_ref, repo, tag_ref_name) == 0) {
        git_reference_free(tag_ref);
        return VERSION_ERR_TAG_EXISTS;
    }
    
    // Get HEAD commit
    git_oid head_oid;
    int error = git_reference_name_to_id(&head_oid, repo, "HEAD");
    if (error) return VERSION_ERR_GIT_OPERATION;
    
    git_commit *head_commit = NULL;
    error = git_commit_lookup(&head_commit, repo, &head_oid);
    if (error) return VERSION_ERR_GIT_OPERATION;
    
    // Create tag
    git_signature *tagger = NULL;
    error = git_signature_default(&tagger, repo);
    if (error) {
        git_commit_free(head_commit);
        return VERSION_ERR_GIT_OPERATION;
    }
    
    git_oid tag_oid;
    error = git_tag_create(
        &tag_oid,
        repo,
        tag_name,
        (git_object *)head_commit,
        tagger,
        message,
        0
    );
    
    git_signature_free(tagger);
    git_commit_free(head_commit);
    
    if (error) return VERSION_ERR_GIT_OPERATION;
    
    return RELEASY_SUCCESS;
}

int version_validate(const char *version) {
    if (!version) return VERSION_ERR_INVALID_FORMAT;
    
    semver_t ver;
    int ret = semver_parse(version, &ver);
    semver_free(&ver);
    
    return ret == 0 ? RELEASY_SUCCESS : VERSION_ERR_INVALID_FORMAT;
}

int version_compare(const char *version1, const char *version2) {
    if (!version1 || !version2) return 0;
    
    semver_t v1, v2;
    if (semver_parse(version1, &v1) != 0 || semver_parse(version2, &v2) != 0) {
        return 0;
    }
    
    int result = semver_compare(&v1, &v2);
    
    semver_free(&v1);
    semver_free(&v2);
    
    return result;
}

int version_get_latest(git_repository *repo, char *version_buf, size_t buf_size) {
    if (!repo || !version_buf || buf_size == 0) return RELEASY_ERROR;
    
    git_strarray tags = {0};
    int error = git_tag_list(&tags, repo);
    if (error) return VERSION_ERR_GIT_OPERATION;
    
    if (tags.count == 0) {
        git_strarray_free(&tags);
        return VERSION_ERR_NO_PREVIOUS_VERSION;
    }
    
    // Find latest version tag
    semver_t latest = {0};
    char *latest_tag = NULL;
    int found = 0;
    
    for (size_t i = 0; i < tags.count; i++) {
        // Skip non-version tags
        if (!git_ops_is_version_tag(NULL, tags.strings[i])) continue;
        
        // Parse version (skip 'v' prefix if present)
        const char *ver = tags.strings[i];
        if (ver[0] == 'v') ver++;
        
        semver_t current;
        if (semver_parse(ver, &current) == 0) {
            if (!found || semver_compare(&current, &latest) > 0) {
                if (found) semver_free(&latest);
                latest = current;
                latest_tag = tags.strings[i];
                found = 1;
            } else {
                semver_free(&current);
            }
        }
    }
    
    int ret = RELEASY_SUCCESS;
    if (found) {
        // Copy version to buffer (skip 'v' prefix if present)
        const char *ver = latest_tag;
        if (ver[0] == 'v') ver++;
        if (strlen(ver) >= buf_size) {
            ret = VERSION_ERR_INVALID_FORMAT;
        } else {
            strcpy(version_buf, ver);
        }
        semver_free(&latest);
    } else {
        ret = VERSION_ERR_NO_PREVIOUS_VERSION;
    }
    
    git_strarray_free(&tags);
    return ret;
}

void version_cleanup(version_info_t *info) {
    if (!info) return;
    free(info->current_version);
    free(info->previous_version);
    free(info->next_version);
    free(info->release_branch);
    free(info->prerelease_label);
    memset(info, 0, sizeof(version_info_t));
}

const char *version_error_string(int error_code) {
    switch (error_code) {
        case RELEASY_SUCCESS:
            return "Success";
        case VERSION_ERR_INVALID_FORMAT:
            return "Invalid version format";
        case VERSION_ERR_INVALID_INCREMENT:
            return "Invalid version increment";
        case VERSION_ERR_TAG_EXISTS:
            return "Version tag already exists";
        case VERSION_ERR_NO_PREVIOUS_VERSION:
            return "No previous version found";
        case VERSION_ERR_BRANCH_EXISTS:
            return "Release branch already exists";
        case VERSION_ERR_FILE_ACCESS:
            return "Failed to access version file";
        case VERSION_ERR_MEMORY:
            return "Memory allocation failed";
        case VERSION_ERR_GIT_OPERATION:
            return "Git operation failed";
        case VERSION_ERR_INVALID_STATE:
            return "Invalid version state";
        default:
            return "Unknown error";
    }
} 