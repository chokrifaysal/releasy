#ifndef RELEASY_VERSION_H
#define RELEASY_VERSION_H

#include <git2.h>
#include "releasy.h"
#include "semver.h"

// Error codes
#define VERSION_ERR_INVALID_FORMAT -400
#define VERSION_ERR_INVALID_INCREMENT -401
#define VERSION_ERR_TAG_EXISTS -402
#define VERSION_ERR_NO_PREVIOUS_VERSION -403
#define VERSION_ERR_BRANCH_EXISTS -404
#define VERSION_ERR_FILE_ACCESS -405
#define VERSION_ERR_MEMORY -406
#define VERSION_ERR_GIT_OPERATION -407
#define VERSION_ERR_INVALID_STATE -408

// Version increment types
typedef enum {
    VERSION_INC_MAJOR,
    VERSION_INC_MINOR,
    VERSION_INC_PATCH,
    VERSION_INC_CUSTOM
} version_increment_t;

// Version file format
typedef struct {
    char *current_version;
    char *previous_version;
    char *next_version;
    char *release_branch;
    int is_prerelease;
    char *prerelease_label;
    int build_number;
} version_info_t;

// Function declarations
int version_init(version_info_t *info);
int version_load(version_info_t *info, const char *version_file);
int version_save(version_info_t *info, const char *version_file);
int version_increment(version_info_t *info, version_increment_t increment_type, const char *custom_version);
int version_create_release_branch(git_repository *repo, version_info_t *info);
int version_create_tag(git_repository *repo, version_info_t *info, const char *message);
int version_validate(const char *version);
int version_compare(const char *version1, const char *version2);
int version_get_latest(git_repository *repo, char *version_buf, size_t buf_size);
void version_cleanup(version_info_t *info);

const char *version_error_string(int error_code);

#endif // RELEASY_VERSION_H 