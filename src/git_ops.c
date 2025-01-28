#include "git_ops.h"
#include "releasy.h"

int git_ops_init(git_context_t *ctx) {
    git_libgit2_init();
    return RELEASY_ERROR;
}

int git_ops_get_latest_tag(git_context_t *ctx, char **tag) {
    return RELEASY_ERROR;
}

int git_ops_create_tag(git_context_t *ctx, const char *tag_name) {
    return RELEASY_ERROR;
}

int git_ops_rollback(git_context_t *ctx, const char *tag) {
    return RELEASY_ERROR;
}

void git_ops_cleanup(git_context_t *ctx) {
    if (!ctx) return;
    if (ctx->repo) git_repository_free(ctx->repo);
    free(ctx->current_branch);
    free(ctx->latest_tag);
    git_libgit2_shutdown();
} 