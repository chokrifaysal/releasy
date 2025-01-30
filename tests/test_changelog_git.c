#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <git2.h>
#include "changelog.h"
#include "test_helpers.h"

static void test_git_integration(void) {
    printf("Testing git integration...\n");
    
    test_repo_t test_repo = {0};
    assert(init_test_repo(&test_repo) == 0);
    
    // Create some test commits
    assert(create_test_commit(&test_repo, "feat(core): initial commit") == 0);
    assert(create_test_commit(&test_repo, "fix: bug fix\n\nThis fixes a critical bug") == 0);
    assert(create_test_commit(&test_repo, "feat!: breaking change") == 0);
    
    // Create version tag
    assert(create_test_tag(&test_repo, "v1.0.0") == 0);
    
    // Initialize changelog
    changelog_t log;
    assert(changelog_init(&log, "CHANGELOG.md") == RELEASY_SUCCESS);
    
    // Generate changelog
    assert(changelog_generate(&log, test_repo.repo, "1.0.0") == RELEASY_SUCCESS);
    
    // Verify changelog entries
    assert(log.count == 1);
    changelog_entry_t *entry = log.entries[0];
    assert(entry != NULL);
    assert(strcmp(entry->version, "1.0.0") == 0);
    assert(entry->count == 3);
    
    // Verify commits
    int found_feat = 0, found_fix = 0, found_breaking = 0;
    for (size_t i = 0; i < entry->count; i++) {
        commit_info_t *commit = entry->commits[i];
        if (commit->type == COMMIT_TYPE_FEAT && strcmp(commit->scope, "core") == 0) {
            found_feat = 1;
        } else if (commit->type == COMMIT_TYPE_FIX) {
            found_fix = 1;
        } else if (commit->is_breaking) {
            found_breaking = 1;
        }
    }
    assert(found_feat && found_fix && found_breaking);
    
    // Test changelog writing
    assert(changelog_write(&log) == RELEASY_SUCCESS);
    assert(file_exists_with_pattern("CHANGELOG.md"));
    
    // Cleanup
    remove("CHANGELOG.md");
    changelog_cleanup(&log);
    cleanup_test_repo(&test_repo);
    
    printf("Git integration tests passed!\n");
}

static void test_changelog_formatting(void) {
    printf("Testing changelog formatting...\n");
    
    changelog_t log;
    assert(changelog_init(&log, "format_test.md") == RELEASY_SUCCESS);
    
    // Create test entries
    changelog_entry_t *entry = calloc(1, sizeof(changelog_entry_t));
    assert(entry != NULL);
    entry->version = strdup("1.0.0");
    entry->date = strdup("2024-03-20");
    
    // Create test commits
    entry->commits = calloc(3, sizeof(commit_info_t *));
    assert(entry->commits != NULL);
    
    commit_info_t *commit1 = calloc(1, sizeof(commit_info_t));
    commit1->type = COMMIT_TYPE_FEAT;
    commit1->scope = strdup("api");
    commit1->description = strdup("new feature");
    commit1->author = strdup("Test User");
    entry->commits[0] = commit1;
    
    commit_info_t *commit2 = calloc(1, sizeof(commit_info_t));
    commit2->type = COMMIT_TYPE_FIX;
    commit2->description = strdup("bug fix");
    commit2->is_breaking = 1;
    entry->commits[1] = commit2;
    
    commit_info_t *commit3 = calloc(1, sizeof(commit_info_t));
    commit3->type = COMMIT_TYPE_DOCS;
    commit3->description = strdup("update docs");
    entry->commits[2] = commit3;
    
    entry->count = 3;
    
    // Add entry to changelog
    log.entries = calloc(1, sizeof(changelog_entry_t *));
    assert(log.entries != NULL);
    log.entries[0] = entry;
    log.count = 1;
    
    // Test different formatting options
    log.group_by_type = 1;
    log.include_metadata = 1;
    log.include_authors = 1;
    assert(changelog_write(&log) == RELEASY_SUCCESS);
    
    // Verify file contents
    FILE *f = fopen("format_test.md", "r");
    assert(f != NULL);
    char line[256];
    int found_version = 0, found_feat = 0, found_fix = 0, found_docs = 0;
    
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "## [1.0.0]")) found_version = 1;
        if (strstr(line, "### feat")) found_feat = 1;
        if (strstr(line, "### fix")) found_fix = 1;
        if (strstr(line, "### docs")) found_docs = 1;
    }
    fclose(f);
    
    assert(found_version && found_feat && found_fix && found_docs);
    
    // Cleanup
    remove("format_test.md");
    changelog_cleanup(&log);
    
    printf("Changelog formatting tests passed!\n");
}

int main(void) {
    printf("Running changelog git integration tests...\n\n");
    
    git_libgit2_init();
    
    test_git_integration();
    test_changelog_formatting();
    
    git_libgit2_shutdown();
    
    printf("\nAll changelog git integration tests passed!\n");
    return 0;
} 