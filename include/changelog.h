#ifndef RELEASY_CHANGELOG_H
#define RELEASY_CHANGELOG_H

#include <git2.h>
#include "releasy.h"

// Error codes
#define CHANGELOG_ERR_NO_COMMITS -300
#define CHANGELOG_ERR_INVALID_FORMAT -301
#define CHANGELOG_ERR_FILE_ACCESS -302
#define CHANGELOG_ERR_PARSE_FAILED -303

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
    char *breaking_change;
    char *references;
    char *hash;
    char *author;
    char *date;
} commit_info_t;

typedef struct {
    char *version;
    char *date;
    commit_info_t *commits;
    size_t commit_count;
    char *previous_version;
    int has_breaking_changes;
} changelog_entry_t;

typedef struct {
    char *path;
    changelog_entry_t *entries;
    size_t entry_count;
    const char *template_path;
} changelog_t;

// Function declarations
int changelog_init(changelog_t *log, const char *path);
int changelog_parse_commit(const char *message, commit_info_t *info);
int changelog_generate(changelog_t *log, const char *version, const char *from_ref, const char *to_ref);
int changelog_write(changelog_t *log, const char *version);
void changelog_free_commit(commit_info_t *commit);
void changelog_free_entry(changelog_entry_t *entry);
void changelog_cleanup(changelog_t *log);

const char *changelog_commit_type_string(commit_type_t type);
const char *changelog_error_string(int error_code);

#endif // RELEASY_CHANGELOG_H 