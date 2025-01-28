#ifndef RELEASY_GIT_OPS_H
#define RELEASY_GIT_OPS_H

#include <git2.h>

typedef struct {
    git_repository *repo;
    char *current_branch;
    char *latest_tag;
} git_context_t;

int git_ops_init(git_context_t *ctx);
int git_ops_get_latest_tag(git_context_t *ctx, char **tag);
int git_ops_create_tag(git_context_t *ctx, const char *tag_name);
int git_ops_rollback(git_context_t *ctx, const char *tag);
void git_ops_cleanup(git_context_t *ctx);

#endif 