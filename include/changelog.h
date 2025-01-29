#ifndef RELEASY_CHANGELOG_H
#define RELEASY_CHANGELOG_H

#include <git2.h>
#include "releasy.h"

// Error codes
#define CHANGELOG_ERR_NO_COMMITS -300
#define CHANGELOG_ERR_INVALID_FORMAT -301
#define CHANGELOG_ERR_FILE_ACCESS -302
#define CHANGELOG_ERR_PARSE_FAILED -303
#define CHANGELOG_ERR_GIT_WALK_FAILED -304
#define CHANGELOG_ERR_GIT_LOOKUP_FAILED -305
#define CHANGELOG_ERR_MEMORY -306
#define CHANGELOG_ERR_TAG_NOT_FOUND -307
#define CHANGELOG_ERR_INVALID_RANGE -308
#define CHANGELOG_ERR_BACKUP_FAILED -309
#define CHANGELOG_ERR_INVALID_PATH -310
#define CHANGELOG_ERR_INVALID_CONFIG -311
#define CHANGELOG_ERR_INVALID_VERSION -312

// Commit types for conventional commits
typedef enum {
    COMMIT_TYPE_FEAT,
    COMMIT_TYPE_FIX,
    COMMIT_TYPE_DOCS,
    COMMIT_TYPE_STYLE,
    COMMIT_TYPE_REFACTOR,
    COMMIT_TYPE_PERF,
    COMMIT_TYPE_TEST,
    COMMIT_TYPE_BUILD,
    COMMIT_TYPE_CI,
    COMMIT_TYPE_CHORE,
    COMMIT_TYPE_REVERT,
    COMMIT_TYPE_UNKNOWN
} commit_type_t;

typedef struct {
    commit_type_t type;
    char *scope;
    char *description;
    char *body;
    char *footer;
    int is_breaking;
    char *commit_hash;
    char *author;
    char *date;
} commit_info_t;

typedef struct {
    commit_info_t **commits;
    size_t count;
    char *version;
    char *previous_version;
    char *date;
} changelog_entry_t;

typedef struct {
    char *file_path;
    changelog_entry_t **entries;
    size_t count;
    int include_metadata;
    int group_by_type;
    int include_authors;
    int backup;  // Flag to enable/disable changelog backup
} changelog_t;

// Function declarations
int changelog_init(changelog_t *log, const char *file_path);
int changelog_parse_commit(const char *message, commit_info_t *commit);
int changelog_generate(changelog_t *log, git_repository *repo, const char *version);
int changelog_write(changelog_t *log);
int changelog_free_commit(commit_info_t *commit);
int changelog_free_entry(changelog_entry_t *entry);
void changelog_cleanup(changelog_t *log);

const char *changelog_error_string(int error_code);
const char *changelog_commit_type_string(commit_type_t type);

#endif // RELEASY_CHANGELOG_H 