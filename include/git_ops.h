#ifndef RELEASY_GIT_OPS_H
#define RELEASY_GIT_OPS_H

#include <git2.h>
#include "releasy.h"
#include "semver.h"

#define GIT_ERR_REPO_NOT_FOUND -200
#define GIT_ERR_TAG_EXISTS -201
#define GIT_ERR_TAG_NOT_FOUND -202
#define GIT_ERR_NO_TAGS -203
#define GIT_ERR_INVALID_TAG -204
#define GIT_ERR_SIGN_FAILED -205
#define GIT_ERR_DIRTY_REPO -206
#define GIT_ERR_ROLLBACK_FAILED -207

typedef struct {
    git_repository *repo;
    git_signature *signature;
    char *current_branch;
    char *latest_tag;
    int is_dirty;
} git_context_t;

int git_ops_init(git_context_t *ctx);
int git_ops_open_repo(git_context_t *ctx, const char *path);
int git_ops_check_dirty(git_context_t *ctx);
int git_ops_get_latest_tag(git_context_t *ctx, char **tag);
int git_ops_list_tags(git_context_t *ctx, char ***tags, size_t *count);
int git_ops_create_tag(git_context_t *ctx, const char *tag_name, const char *message, int sign);
int git_ops_verify_tag(git_context_t *ctx, const char *tag_name);
int git_ops_rollback(git_context_t *ctx, const char *tag);
const char *git_ops_error_string(int error_code);
void git_ops_cleanup(git_context_t *ctx);

#endif 