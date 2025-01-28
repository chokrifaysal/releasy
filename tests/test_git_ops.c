#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "git_ops.h"

static void test_repo_operations(const char *repo_path) {
    git_context_t ctx;
    int ret;

    printf("Testing Git operations:\n\n");

    ret = git_ops_init(&ctx);
    if (ret != RELEASY_SUCCESS) {
        printf("Failed to initialize git context: %s\n", git_ops_error_string(ret));
        return;
    }

    ret = git_ops_open_repo(&ctx, repo_path);
    if (ret != RELEASY_SUCCESS) {
        printf("Failed to open repository at %s: %s\n", repo_path, git_ops_error_string(ret));
        git_ops_cleanup(&ctx);
        return;
    }

    printf("Repository opened successfully\n");
    if (ctx.current_branch) {
        printf("Current branch: %s\n", ctx.current_branch);
    }
    printf("Repository is %s\n", ctx.is_dirty ? "dirty" : "clean");

    char **tags = NULL;
    size_t count = 0;
    ret = git_ops_list_tags(&ctx, &tags, &count);
    if (ret == RELEASY_SUCCESS && tags != NULL) {
        printf("\nFound %zu tags:\n", count);
        for (size_t i = 0; i < count; i++) {
            if (tags[i]) {
                printf("  %s\n", tags[i]);
                free(tags[i]);
            }
        }
        free(tags);
    } else {
        printf("\nNo tags found or failed to list tags: %s\n", git_ops_error_string(ret));
    }

    char *latest_tag = NULL;
    ret = git_ops_get_latest_tag(&ctx, &latest_tag);
    if (ret == RELEASY_SUCCESS && latest_tag != NULL) {
        printf("\nLatest tag: %s\n", latest_tag);
        free(latest_tag);
    } else {
        printf("\nNo latest tag found: %s\n", git_ops_error_string(ret));
    }

    if (!ctx.is_dirty) {
        const char *test_tag = "test-tag-1.0.0";
        ret = git_ops_create_tag(&ctx, test_tag, "Test tag creation", 0);
        if (ret == RELEASY_SUCCESS) {
            printf("\nCreated tag: %s\n", test_tag);
            
            ret = git_ops_verify_tag(&ctx, test_tag);
            if (ret == RELEASY_SUCCESS) {
                printf("Tag verified successfully\n");
            } else {
                printf("Failed to verify tag: %s\n", git_ops_error_string(ret));
            }
        } else {
            printf("\nFailed to create tag: %s\n", git_ops_error_string(ret));
        }
    } else {
        printf("\nSkipping tag creation in dirty repository\n");
    }

    git_ops_cleanup(&ctx);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s <repo_path>\n", argv[0]);
        return 1;
    }

    test_repo_operations(argv[1]);
    return 0;
} 