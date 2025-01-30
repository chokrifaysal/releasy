#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <git2.h>
#include "changelog.h"
#include "test_helpers.h"

static void test_changelog_init(void) {
    printf("Testing changelog initialization...\n");
    
    changelog_t log;
    
    // Test valid initialization
    assert(changelog_init(&log, "CHANGELOG.md") == RELEASY_SUCCESS);
    assert(log.file_path != NULL);
    assert(strcmp(log.file_path, "CHANGELOG.md") == 0);
    assert(log.include_metadata == 1);
    assert(log.group_by_type == 1);
    assert(log.include_authors == 1);
    changelog_cleanup(&log);
    
    // Test invalid file paths
    assert(changelog_init(&log, NULL) == RELEASY_ERROR);
    assert(changelog_init(&log, "") == CHANGELOG_ERR_INVALID_PATH);
    assert(changelog_init(&log, "/absolute/path/changelog.md") == CHANGELOG_ERR_INVALID_PATH);
    assert(changelog_init(&log, "../changelog.md") == CHANGELOG_ERR_INVALID_PATH);
    assert(changelog_init(&log, "changelog.txt") == CHANGELOG_ERR_INVALID_PATH);
    
    printf("Changelog initialization tests passed!\n");
}

static void test_commit_parsing(void) {
    printf("Testing commit message parsing...\n");
    
    commit_info_t commit;
    
    // Test valid conventional commit
    const char *msg = "feat(core): add new feature\n\nThis is the body\n\nBREAKING CHANGE: API changed";
    assert(changelog_parse_commit(msg, &commit) == RELEASY_SUCCESS);
    assert(commit.type == COMMIT_TYPE_FEAT);
    assert(strcmp(commit.scope, "core") == 0);
    assert(strcmp(commit.description, "add new feature") == 0);
    changelog_free_commit(&commit);
    
    // Test breaking change marker
    msg = "feat!: breaking change";
    assert(changelog_parse_commit(msg, &commit) == RELEASY_SUCCESS);
    assert(commit.type == COMMIT_TYPE_FEAT);
    assert(commit.is_breaking == 1);
    changelog_free_commit(&commit);
    
    // Test invalid formats
    assert(changelog_parse_commit(NULL, &commit) == CHANGELOG_ERR_PARSE_FAILED);
    assert(changelog_parse_commit("invalid", &commit) == CHANGELOG_ERR_INVALID_FORMAT);
    assert(changelog_parse_commit("type: ", &commit) == RELEASY_ERROR);
    
    printf("Commit parsing tests passed!\n");
}

static void test_version_validation(void) {
    printf("Testing version validation...\n");
    
    changelog_t log;
    assert(changelog_init(&log, "CHANGELOG.md") == RELEASY_SUCCESS);
    
    // Test valid versions
    assert(changelog_generate(&log, NULL, "1.0.0") == RELEASY_ERROR); // Fails due to NULL repo
    assert(changelog_generate(&log, NULL, "v1.0.0") == RELEASY_ERROR);
    
    // Test invalid versions
    assert(changelog_generate(&log, NULL, NULL) == RELEASY_ERROR);
    assert(changelog_generate(&log, NULL, "invalid") == CHANGELOG_ERR_INVALID_VERSION);
    assert(changelog_generate(&log, NULL, "1.0") == CHANGELOG_ERR_INVALID_VERSION);
    assert(changelog_generate(&log, NULL, "v1") == CHANGELOG_ERR_INVALID_VERSION);
    
    changelog_cleanup(&log);
    printf("Version validation tests passed!\n");
}

static void test_backup_functionality(void) {
    printf("Testing backup functionality...\n");
    
    changelog_t log;
    assert(changelog_init(&log, "test_changelog.md") == RELEASY_SUCCESS);
    
    // Create a test changelog file
    FILE *f = fopen("test_changelog.md", "w");
    assert(f != NULL);
    fprintf(f, "# Test Changelog\n");
    fclose(f);
    
    // Enable backup and write
    log.backup = 1;
    assert(changelog_write(&log) == CHANGELOG_ERR_NO_COMMITS); // Fails due to no commits
    
    // Check if backup was created
    char backup_pattern[256];
    snprintf(backup_pattern, sizeof(backup_pattern), "test_changelog.md.*.bak");
    assert(file_exists_with_pattern(backup_pattern));
    
    // Cleanup
    remove("test_changelog.md");
    remove_files_with_pattern(backup_pattern);
    changelog_cleanup(&log);
    
    printf("Backup functionality tests passed!\n");
}

static void test_error_handling(void) {
    printf("Testing error handling...\n");
    
    // Test error strings
    assert(strcmp(changelog_error_string(RELEASY_SUCCESS), "Success") == 0);
    assert(strcmp(changelog_error_string(CHANGELOG_ERR_NO_COMMITS), "No commits found") == 0);
    assert(strcmp(changelog_error_string(CHANGELOG_ERR_INVALID_FORMAT), "Failed to parse commit message") == 0);
    assert(strcmp(changelog_error_string(CHANGELOG_ERR_INVALID_PATH), "Invalid changelog file path") == 0);
    assert(strcmp(changelog_error_string(9999), "Unknown error") == 0);
    
    printf("Error handling tests passed!\n");
}

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
    printf("Running changelog tests...\n\n");
    
    git_libgit2_init();
    
    test_changelog_init();
    test_commit_parsing();
    test_version_validation();
    test_backup_functionality();
    test_error_handling();
    test_git_integration();
    test_changelog_formatting();
    
    git_libgit2_shutdown();
    
    printf("\nAll changelog tests passed!\n");
    return 0;
} 