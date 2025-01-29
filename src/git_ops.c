#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include "git_ops.h"
#include "releasy.h"
#include "semver.h"

static int git_ops_get_signature(git_context_t *ctx) {
    if (ctx->signature) return RELEASY_SUCCESS;
    
    int error = git_ops_ensure_user_config(ctx);
    if (error) return error;
    
    error = git_signature_now(&ctx->signature, ctx->user_name, ctx->user_email);
    return error ? GIT_ERR_SIGN_FAILED : RELEASY_SUCCESS;
}

int git_ops_set_user(git_context_t *ctx, const char *name, const char *email) {
    if (!ctx || !name || !email) return RELEASY_ERROR;
    
    char *new_name = strdup(name);
    if (!new_name) return RELEASY_ERROR;
    
    char *new_email = strdup(email);
    if (!new_email) {
        free(new_name);
        return RELEASY_ERROR;
    }
    
    free(ctx->user_name);
    free(ctx->user_email);
    ctx->user_name = new_name;
    ctx->user_email = new_email;
    
    if (ctx->signature) {
        git_signature_free(ctx->signature);
        ctx->signature = NULL;
    }
    
    return RELEASY_SUCCESS;
}

int git_ops_get_user_from_git(git_context_t *ctx) {
    if (!ctx || !ctx->repo) return RELEASY_ERROR;
    
    git_config *cfg;
    int error = git_repository_config(&cfg, ctx->repo);
    if (error) return GIT_ERR_NO_USER_CONFIG;
    
    const char *name = NULL;
    error = git_config_get_string(&name, cfg, "user.name");
    if (error) {
        git_config_free(cfg);
        return GIT_ERR_NO_USER_CONFIG;
    }
    
    const char *email = NULL;
    error = git_config_get_string(&email, cfg, "user.email");
    if (error) {
        git_config_free(cfg);
        return GIT_ERR_NO_USER_CONFIG;
    }
    
    error = git_ops_set_user(ctx, name, email);
    git_config_free(cfg);
    return error;
}

int git_ops_get_user_from_env(git_context_t *ctx) {
    if (!ctx) return RELEASY_ERROR;
    
    const char *name = getenv("GIT_AUTHOR_NAME");
    const char *email = getenv("GIT_AUTHOR_EMAIL");
    
    if (!name || !email) {
        name = getenv("GIT_COMMITTER_NAME");
        email = getenv("GIT_COMMITTER_EMAIL");
    }
    
    if (!name || !email) return GIT_ERR_NO_USER_CONFIG;
    
    return git_ops_set_user(ctx, name, email);
}

int git_ops_ensure_user_config(git_context_t *ctx) {
    if (!ctx) return RELEASY_ERROR;
    
    // If we already have user info, we're good
    if (ctx->user_name && ctx->user_email) return RELEASY_SUCCESS;
    
    // Try git config first
    int error = git_ops_get_user_from_git(ctx);
    if (error == RELEASY_SUCCESS) return RELEASY_SUCCESS;
    
    // Try environment variables
    error = git_ops_get_user_from_env(ctx);
    if (error == RELEASY_SUCCESS) return RELEASY_SUCCESS;
    
    return GIT_ERR_NO_USER_CONFIG;
}

int git_ops_init(git_context_t *ctx) {
    if (!ctx) return RELEASY_ERROR;
    
    git_libgit2_init();
    memset(ctx, 0, sizeof(git_context_t));
    return RELEASY_SUCCESS;
}

int git_ops_open_repo(git_context_t *ctx, const char *path) {
    if (!ctx || !path) return RELEASY_ERROR;
    
    int error = git_repository_open(&ctx->repo, path);
    if (error) return GIT_ERR_REPO_NOT_FOUND;
    
    git_reference *head = NULL;
    error = git_repository_head(&head, ctx->repo);
    if (!error) {
        const char *branch_name = git_reference_shorthand(head);
        if (branch_name) {
            ctx->current_branch = strdup(branch_name);
            if (!ctx->current_branch) {
                git_reference_free(head);
                return RELEASY_ERROR;
            }
        }
        git_reference_free(head);
    }
    
    return git_ops_check_dirty(ctx);
}

int git_ops_check_dirty(git_context_t *ctx) {
    if (!ctx || !ctx->repo) return RELEASY_ERROR;
    
    git_status_options opts = GIT_STATUS_OPTIONS_INIT;
    opts.show = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
    opts.flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED;
    
    git_status_list *status = NULL;
    int error = git_status_list_new(&status, ctx->repo, &opts);
    if (error) return error;
    
    size_t count = git_status_list_entrycount(status);
    ctx->is_dirty = count > 0;
    
    git_status_list_free(status);
    return RELEASY_SUCCESS;
}

static int tag_sorting_callback(const void *a, const void *b) {
    const char *tag_a = *(const char **)a;
    const char *tag_b = *(const char **)b;
    
    // Skip 'v' prefix if present
    if (tag_a[0] == 'v') tag_a++;
    if (tag_b[0] == 'v') tag_b++;
    
    semver_t ver_a, ver_b;
    semver_init(&ver_a);
    semver_init(&ver_b);
    
    if (semver_parse(tag_a, &ver_a) != 0 || semver_parse(tag_b, &ver_b) != 0) {
        semver_free(&ver_a);
        semver_free(&ver_b);
        return 0;
    }
    
    int result = semver_compare(&ver_a, &ver_b);
    semver_free(&ver_a);
    semver_free(&ver_b);
    return result;
}

int git_ops_list_tags(git_context_t *ctx, char ***tags, size_t *count) {
    if (!ctx || !ctx->repo || !tags || !count) return RELEASY_ERROR;
    
    *tags = NULL;
    *count = 0;
    
    git_strarray tag_array;
    int error = git_tag_list(&tag_array, ctx->repo);
    if (error) return GIT_ERR_NO_TAGS;
    
    if (tag_array.count == 0) {
        git_strarray_free(&tag_array);
        return GIT_ERR_NO_TAGS;
    }
    
    *tags = calloc(tag_array.count, sizeof(char *));
    if (!*tags) {
        git_strarray_free(&tag_array);
        return RELEASY_ERROR;
    }
    
    *count = tag_array.count;
    for (size_t i = 0; i < tag_array.count; i++) {
        (*tags)[i] = strdup(tag_array.strings[i]);
        if (!(*tags)[i]) {
            for (size_t j = 0; j < i; j++) {
                free((*tags)[j]);
            }
            free(*tags);
            *tags = NULL;
            *count = 0;
            git_strarray_free(&tag_array);
            return RELEASY_ERROR;
        }
    }
    
    qsort(*tags, *count, sizeof(char *), tag_sorting_callback);
    git_strarray_free(&tag_array);
    
    return RELEASY_SUCCESS;
}

int git_ops_get_latest_tag(git_context_t *ctx, char **tag) {
    if (!ctx || !ctx->repo || !tag) return RELEASY_ERROR;
    
    *tag = NULL;
    char **tags = NULL;
    size_t count = 0;
    
    int error = git_ops_list_tags(ctx, &tags, &count);
    if (error) return error;
    
    if (count > 0 && tags[0]) {
        *tag = strdup(tags[0]);
        if (!*tag) {
            error = RELEASY_ERROR;
        }
    }
    
    for (size_t i = 0; i < count; i++) {
        free(tags[i]);
    }
    free(tags);
    
    return error;
}

int git_ops_get_latest_version(git_context_t *ctx, char *version, size_t size) {
    if (!ctx || !version || size == 0) return RELEASY_ERROR;

    git_strarray tags = {0};
    int ret = git_tag_list(&tags, ctx->repo);
    if (ret != 0) return GIT_ERR_NO_TAGS;

    if (tags.count == 0) {
        git_strarray_free(&tags);
        return GIT_ERR_NO_TAGS;
    }

    // Sort tags by version
    qsort(tags.strings, tags.count, sizeof(char *), tag_sorting_callback);

    // Get the latest version (last in sorted array)
    const char *latest = tags.strings[tags.count - 1];
    if (latest[0] == 'v') latest++; // Skip 'v' prefix if present

    strncpy(version, latest, size - 1);
    version[size - 1] = '\0';

    git_strarray_free(&tags);
    return RELEASY_SUCCESS;
}

int git_ops_create_tag(git_context_t *ctx, const char *version, const char *user_name, const char *user_email) {
    if (!ctx || !version || !user_name || !user_email) return RELEASY_ERROR;

    // Get HEAD commit
    git_oid head_oid;
    git_object *head = NULL;
    char tag_name[64];
    snprintf(tag_name, sizeof(tag_name), "v%s", version);

    int ret = git_revparse_single(&head, ctx->repo, "HEAD");
    if (ret != 0) {
        const git_error *err = git_error_last();
        fprintf(stderr, "Failed to get HEAD: %s\n", err ? err->message : "unknown error");
        return RELEASY_ERROR;
    }

    // Create tag signature
    git_signature *tagger = NULL;
    ret = git_signature_now(&tagger, user_name, user_email);
    if (ret != 0) {
        git_object_free(head);
        return RELEASY_ERROR;
    }

    // Create annotated tag
    char tag_message[256];
    snprintf(tag_message, sizeof(tag_message), "Release version %s", version);

    ret = git_tag_create(NULL, ctx->repo, tag_name, head, tagger, tag_message, 0);
    if (ret != 0) {
        // Try lightweight tag if annotated tag fails
        ret = git_tag_create_lightweight(NULL, ctx->repo, tag_name, head, 0);
    }

    git_object_free(head);
    git_signature_free(tagger);

    return ret == 0 ? RELEASY_SUCCESS : RELEASY_ERROR;
}

int git_ops_verify_tag(git_context_t *ctx, const char *tag_name) {
    if (!ctx || !tag_name) return RELEASY_ERROR;

    git_object *obj = NULL;
    int ret = git_revparse_single(&obj, ctx->repo, tag_name);
    if (ret != 0) return RELEASY_ERROR;

    // Check if it's a tag
    if (git_object_type(obj) != GIT_OBJ_TAG) {
        git_object_free(obj);
        return RELEASY_ERROR;
    }

    // For annotated tags, verify the tagger
    const git_tag *tag = (const git_tag *)obj;
    const git_signature *tagger = git_tag_tagger(tag);
    if (!tagger) {
        git_object_free(obj);
        return RELEASY_ERROR;
    }

    git_object_free(obj);
    return RELEASY_SUCCESS;
}

int git_ops_rollback(git_context_t *ctx, const char *tag) {
    if (!ctx || !ctx->repo || !tag) return RELEASY_ERROR;
    
    int error = git_ops_check_dirty(ctx);
    if (error) return error;
    
    git_object *obj = NULL;
    error = git_revparse_single(&obj, ctx->repo, tag);
    if (error) return GIT_ERR_TAG_NOT_FOUND;
    
    git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
    opts.checkout_strategy = GIT_CHECKOUT_SAFE;
    
    error = git_checkout_tree(ctx->repo, obj, &opts);
    git_object_free(obj);
    if (error) return GIT_ERR_ROLLBACK_FAILED;
    
    git_reference *head = NULL;
    error = git_repository_head(&head, ctx->repo);
    if (!error) {
        error = git_reference_set_target(&head, head, git_object_id(obj), "rollback");
        git_reference_free(head);
    }
    
    return error ? GIT_ERR_ROLLBACK_FAILED : RELEASY_SUCCESS;
}

const char *git_ops_error_string(int error_code) {
    switch (error_code) {
        case RELEASY_SUCCESS:
            return "Success";
        case GIT_ERR_REPO_NOT_FOUND:
            return "Git repository not found";
        case GIT_ERR_TAG_EXISTS:
            return "Tag already exists";
        case GIT_ERR_TAG_NOT_FOUND:
            return "Tag not found";
        case GIT_ERR_NO_TAGS:
            return "No tags found in repository";
        case GIT_ERR_INVALID_TAG:
            return "Invalid tag object";
        case GIT_ERR_SIGN_FAILED:
            return "Failed to create signature";
        case GIT_ERR_DIRTY_REPO:
            return "Repository has uncommitted changes";
        case GIT_ERR_ROLLBACK_FAILED:
            return "Failed to rollback to tag";
        case GIT_ERR_NO_USER_CONFIG:
            return "No git user configuration found";
        case GIT_ERR_INVALID_VERSION:
            return "Invalid version format";
        default:
            return git_error_last() ? git_error_last()->message : "Unknown error";
    }
}

void git_ops_cleanup(git_context_t *ctx) {
    if (!ctx) return;
    
    if (ctx->signature) git_signature_free(ctx->signature);
    if (ctx->repo) git_repository_free(ctx->repo);
    free(ctx->current_branch);
    free(ctx->latest_tag);
    free(ctx->user_name);
    free(ctx->user_email);
    
    memset(ctx, 0, sizeof(git_context_t));
    git_libgit2_shutdown();
}

static int validate_version_format(const char *version) {
    if (!version) return RELEASY_ERROR;

    semver_t ver;
    semver_init(&ver);
    int ret = semver_parse(version, &ver);
    semver_free(&ver);
    return ret == 0 ? RELEASY_SUCCESS : GIT_ERR_INVALID_VERSION;
}

static int normalize_version(const char *version, char *normalized, size_t size) {
    if (!version || !normalized || size == 0) return RELEASY_ERROR;

    // Skip 'v' prefix if present
    const char *ver = version;
    if (ver[0] == 'v') ver++;

    // Validate version format
    int ret = validate_version_format(ver);
    if (ret != RELEASY_SUCCESS) return ret;

    strncpy(normalized, ver, size - 1);
    normalized[size - 1] = '\0';
    return RELEASY_SUCCESS;
}

int git_ops_compare_versions(const char *version1, const char *version2) {
    if (!version1 || !version2) return 0;

    char norm1[32], norm2[32];
    if (normalize_version(version1, norm1, sizeof(norm1)) != RELEASY_SUCCESS ||
        normalize_version(version2, norm2, sizeof(norm2)) != RELEASY_SUCCESS) {
        return 0;
    }

    semver_t v1, v2;
    semver_init(&v1);
    semver_init(&v2);

    if (semver_parse(norm1, &v1) != 0 || semver_parse(norm2, &v2) != 0) {
        semver_free(&v1);
        semver_free(&v2);
        return 0;
    }

    int result = semver_compare(&v1, &v2);
    semver_free(&v1);
    semver_free(&v2);
    return result;
}

int git_ops_is_version_tag(git_context_t *ctx, const char *tag_name) {
    if (!ctx || !tag_name) return 0;

    // Skip 'v' prefix if present
    const char *version = tag_name;
    if (version[0] == 'v') version++;

    return validate_version_format(version) == RELEASY_SUCCESS;
}

int git_ops_get_version_history(git_context_t *ctx, char ***versions, size_t *count) {
    if (!ctx || !versions || !count) return RELEASY_ERROR;

    *versions = NULL;
    *count = 0;

    git_strarray tags;
    int ret = git_tag_list(&tags, ctx->repo);
    if (ret != 0) return GIT_ERR_NO_TAGS;

    // Count version tags
    size_t version_count = 0;
    for (size_t i = 0; i < tags.count; i++) {
        if (git_ops_is_version_tag(ctx, tags.strings[i])) {
            version_count++;
        }
    }

    if (version_count == 0) {
        git_strarray_free(&tags);
        return GIT_ERR_NO_TAGS;
    }

    // Allocate memory for version array
    *versions = calloc(version_count, sizeof(char *));
    if (!*versions) {
        git_strarray_free(&tags);
        return RELEASY_ERROR;
    }

    // Copy version tags
    size_t j = 0;
    for (size_t i = 0; i < tags.count; i++) {
        if (git_ops_is_version_tag(ctx, tags.strings[i])) {
            (*versions)[j] = strdup(tags.strings[i]);
            if (!(*versions)[j]) {
                for (size_t k = 0; k < j; k++) {
                    free((*versions)[k]);
                }
                free(*versions);
                *versions = NULL;
                git_strarray_free(&tags);
                return RELEASY_ERROR;
            }
            j++;
        }
    }

    *count = version_count;
    git_strarray_free(&tags);

    // Sort versions
    qsort(*versions, *count, sizeof(char *), tag_sorting_callback);
    return RELEASY_SUCCESS;
} 