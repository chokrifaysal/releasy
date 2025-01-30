#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <git2.h>
#include "version.h"
#include "test_helpers.h"

static void test_version_init(void) {
    printf("Testing version initialization...\n");
    
    version_info_t info;
    assert(version_init(&info) == RELEASY_SUCCESS);
    assert(info.current_version == NULL);
    assert(info.previous_version == NULL);
    assert(info.next_version == NULL);
    assert(info.release_branch == NULL);
    assert(info.is_prerelease == 0);
    assert(info.prerelease_label == NULL);
    assert(info.build_number == 0);
    
    version_cleanup(&info);
    printf("Version initialization tests passed!\n");
}

static void test_version_increment(void) {
    printf("Testing version increment...\n");
    
    version_info_t info;
    assert(version_init(&info) == RELEASY_SUCCESS);
    
    // Set initial version
    info.current_version = strdup("1.0.0");
    assert(info.current_version != NULL);
    
    // Test major increment
    assert(version_increment(&info, VERSION_INC_MAJOR, NULL) == RELEASY_SUCCESS);
    assert(strcmp(info.current_version, "2.0.0") == 0);
    assert(strcmp(info.previous_version, "1.0.0") == 0);
    
    // Test minor increment
    assert(version_increment(&info, VERSION_INC_MINOR, NULL) == RELEASY_SUCCESS);
    assert(strcmp(info.current_version, "2.1.0") == 0);
    assert(strcmp(info.previous_version, "2.0.0") == 0);
    
    // Test patch increment
    assert(version_increment(&info, VERSION_INC_PATCH, NULL) == RELEASY_SUCCESS);
    assert(strcmp(info.current_version, "2.1.1") == 0);
    assert(strcmp(info.previous_version, "2.1.0") == 0);
    
    // Test custom version
    assert(version_increment(&info, VERSION_INC_CUSTOM, "3.0.0") == RELEASY_SUCCESS);
    assert(strcmp(info.current_version, "3.0.0") == 0);
    assert(strcmp(info.previous_version, "2.1.1") == 0);
    
    // Test invalid increments
    assert(version_increment(&info, VERSION_INC_CUSTOM, NULL) == VERSION_ERR_INVALID_INCREMENT);
    assert(version_increment(&info, VERSION_INC_CUSTOM, "invalid") == VERSION_ERR_INVALID_FORMAT);
    assert(version_increment(&info, VERSION_INC_CUSTOM, "2.0.0") == VERSION_ERR_INVALID_INCREMENT);
    
    version_cleanup(&info);
    printf("Version increment tests passed!\n");
}

static void test_version_file_io(void) {
    printf("Testing version file I/O...\n");
    
    version_info_t info;
    assert(version_init(&info) == RELEASY_SUCCESS);
    
    // Setup test data
    info.current_version = strdup("1.0.0");
    info.previous_version = strdup("0.9.0");
    info.next_version = strdup("1.1.0");
    info.release_branch = strdup("release/1.0.0");
    info.is_prerelease = 1;
    info.prerelease_label = strdup("beta");
    info.build_number = 42;
    
    // Save to file
    assert(version_save(&info, "test_version.txt") == RELEASY_SUCCESS);
    
    // Load into new struct
    version_info_t loaded;
    assert(version_init(&loaded) == RELEASY_SUCCESS);
    assert(version_load(&loaded, "test_version.txt") == RELEASY_SUCCESS);
    
    // Verify loaded data
    assert(strcmp(loaded.current_version, "1.0.0") == 0);
    assert(strcmp(loaded.previous_version, "0.9.0") == 0);
    assert(strcmp(loaded.next_version, "1.1.0") == 0);
    assert(strcmp(loaded.release_branch, "release/1.0.0") == 0);
    assert(loaded.is_prerelease == 1);
    assert(strcmp(loaded.prerelease_label, "beta") == 0);
    assert(loaded.build_number == 42);
    
    // Cleanup
    remove("test_version.txt");
    version_cleanup(&info);
    version_cleanup(&loaded);
    
    printf("Version file I/O tests passed!\n");
}

static void test_version_git_integration(void) {
    printf("Testing version git integration...\n");
    
    test_repo_t test_repo = {0};
    assert(init_test_repo(&test_repo) == 0);
    
    version_info_t info;
    assert(version_init(&info) == RELEASY_SUCCESS);
    info.current_version = strdup("1.0.0");
    
    // Test release branch creation
    assert(version_create_release_branch(test_repo.repo, &info) == RELEASY_SUCCESS);
    assert(info.release_branch != NULL);
    assert(strstr(info.release_branch, "release/1.0.0") != NULL);
    
    // Test tag creation
    assert(version_create_tag(test_repo.repo, &info, "Release 1.0.0") == RELEASY_SUCCESS);
    
    // Test latest version
    char latest[32];
    assert(version_get_latest(test_repo.repo, latest, sizeof(latest)) == RELEASY_SUCCESS);
    assert(strcmp(latest, "1.0.0") == 0);
    
    // Cleanup
    version_cleanup(&info);
    cleanup_test_repo(&test_repo);
    
    printf("Version git integration tests passed!\n");
}

static void test_version_validation(void) {
    printf("Testing version validation...\n");
    
    // Test valid versions
    assert(version_validate("1.0.0") == RELEASY_SUCCESS);
    assert(version_validate("0.0.1") == RELEASY_SUCCESS);
    assert(version_validate("999.999.999") == RELEASY_SUCCESS);
    
    // Test invalid versions
    assert(version_validate(NULL) == VERSION_ERR_INVALID_FORMAT);
    assert(version_validate("") == VERSION_ERR_INVALID_FORMAT);
    assert(version_validate("invalid") == VERSION_ERR_INVALID_FORMAT);
    assert(version_validate("1.0") == VERSION_ERR_INVALID_FORMAT);
    assert(version_validate("1.0.0.0") == VERSION_ERR_INVALID_FORMAT);
    assert(version_validate("a.b.c") == VERSION_ERR_INVALID_FORMAT);
    
    printf("Version validation tests passed!\n");
}

static void test_version_comparison(void) {
    printf("Testing version comparison...\n");
    
    // Test equal versions
    assert(version_compare("1.0.0", "1.0.0") == 0);
    
    // Test greater versions
    assert(version_compare("2.0.0", "1.0.0") > 0);
    assert(version_compare("1.1.0", "1.0.0") > 0);
    assert(version_compare("1.0.1", "1.0.0") > 0);
    
    // Test lesser versions
    assert(version_compare("1.0.0", "2.0.0") < 0);
    assert(version_compare("1.0.0", "1.1.0") < 0);
    assert(version_compare("1.0.0", "1.0.1") < 0);
    
    // Test invalid versions
    assert(version_compare(NULL, "1.0.0") == 0);
    assert(version_compare("1.0.0", NULL) == 0);
    assert(version_compare("invalid", "1.0.0") == 0);
    assert(version_compare("1.0.0", "invalid") == 0);
    
    printf("Version comparison tests passed!\n");
}

int main(void) {
    printf("Running version management tests...\n\n");
    
    git_libgit2_init();
    
    test_version_init();
    test_version_increment();
    test_version_file_io();
    test_version_git_integration();
    test_version_validation();
    test_version_comparison();
    
    git_libgit2_shutdown();
    
    printf("\nAll version management tests passed!\n");
    return 0;
} 