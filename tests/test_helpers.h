#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fnmatch.h>
#include <git2.h>
#include <time.h>

// Check if a file exists that matches the given pattern
static int file_exists_with_pattern(const char *pattern) {
    DIR *dir = opendir(".");
    if (!dir) return 0;
    
    struct dirent *entry;
    int found = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        if (fnmatch(pattern, entry->d_name, 0) == 0) {
            found = 1;
            break;
        }
    }
    
    closedir(dir);
    return found;
}

// Remove all files that match the given pattern
static void remove_files_with_pattern(const char *pattern) {
    DIR *dir = opendir(".");
    if (!dir) return;
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (fnmatch(pattern, entry->d_name, 0) == 0) {
            remove(entry->d_name);
        }
    }
    
    closedir(dir);
}

// Test repository structure
typedef struct {
    git_repository *repo;
    char *path;
    git_signature *author;
} test_repo_t;

// Initialize a test repository
static int init_test_repo(test_repo_t *test_repo) {
    char template[] = "releasy_test_XXXXXX";
    char *temp_dir = mkdtemp(template);
    if (!temp_dir) return -1;
    
    test_repo->path = strdup(temp_dir);
    if (!test_repo->path) return -1;
    
    // Initialize repository
    int error = git_repository_init(&test_repo->repo, test_repo->path, 0);
    if (error) return error;
    
    // Create test signature
    error = git_signature_now(&test_repo->author, "Test User", "test@example.com");
    if (error) {
        git_repository_free(test_repo->repo);
        return error;
    }
    
    return 0;
}

// Create a test commit in the repository
static int create_test_commit(test_repo_t *test_repo, const char *message) {
    git_oid tree_id, commit_id;
    git_tree *tree;
    git_index *index;
    
    // Get the index
    int error = git_repository_index(&index, test_repo->repo);
    if (error) return error;
    
    // Write the index to tree
    error = git_index_write_tree(&tree_id, index);
    git_index_free(index);
    if (error) return error;
    
    // Get the tree
    error = git_tree_lookup(&tree, test_repo->repo, &tree_id);
    if (error) return error;
    
    // Create the commit
    git_oid parent_id;
    git_commit *parent = NULL;
    
    // Try to get the parent commit
    if (git_reference_name_to_id(&parent_id, test_repo->repo, "HEAD") == 0) {
        git_commit_lookup(&parent, test_repo->repo, &parent_id);
    }
    
    error = git_commit_create(
        &commit_id,
        test_repo->repo,
        "HEAD",
        test_repo->author,
        test_repo->author,
        "UTF-8",
        message,
        tree,
        parent ? 1 : 0,
        parent ? (const git_commit **)&parent : NULL
    );
    
    git_tree_free(tree);
    if (parent) git_commit_free(parent);
    
    return error;
}

// Create a test tag in the repository
static int create_test_tag(test_repo_t *test_repo, const char *tag_name) {
    git_oid head_id;
    int error = git_reference_name_to_id(&head_id, test_repo->repo, "HEAD");
    if (error) return error;
    
    git_object *head_commit;
    error = git_object_lookup(&head_commit, test_repo->repo, &head_id, GIT_OBJECT_COMMIT);
    if (error) return error;
    
    error = git_tag_create_lightweight(
        NULL,
        test_repo->repo,
        tag_name,
        head_commit,
        0
    );
    
    git_object_free(head_commit);
    return error;
}

// Cleanup test repository
static void cleanup_test_repo(test_repo_t *test_repo) {
    if (test_repo->author) git_signature_free(test_repo->author);
    if (test_repo->repo) git_repository_free(test_repo->repo);
    if (test_repo->path) {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "rm -rf %s", test_repo->path);
        system(cmd);
        free(test_repo->path);
    }
}

#endif // TEST_HELPERS_H 