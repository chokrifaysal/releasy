#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "changelog.h"

static const char *commit_type_strings[] = {
    "feat", "fix", "docs", "style", "refactor",
    "perf", "test", "build", "ci", "chore",
    "revert", "unknown"
};

static commit_type_t parse_commit_type(const char *type_str) {
    if (!type_str) return COMMIT_TYPE_UNKNOWN;
    
    for (int i = 0; i < COMMIT_TYPE_UNKNOWN; i++) {
        if (strcmp(type_str, commit_type_strings[i]) == 0) {
            return (commit_type_t)i;
        }
    }
    return COMMIT_TYPE_UNKNOWN;
}

int changelog_init(changelog_t *log, const char *file_path) {
    if (!log || !file_path) return RELEASY_ERROR;
    
    memset(log, 0, sizeof(changelog_t));
    log->file_path = strdup(file_path);
    if (!log->file_path) return RELEASY_ERROR;
    
    log->include_metadata = 1;
    log->group_by_type = 1;
    log->include_authors = 1;
    
    return RELEASY_SUCCESS;
}

static int parse_conventional_commit(const char *message, commit_info_t *commit) {
    if (!message || !commit) return CHANGELOG_ERR_PARSE_FAILED;
    
    char *msg_copy = strdup(message);
    if (!msg_copy) return RELEASY_ERROR;
    
    // Parse type and scope
    char *type_end = strchr(msg_copy, ':');
    if (!type_end) {
        free(msg_copy);
        return CHANGELOG_ERR_INVALID_FORMAT;
    }
    
    *type_end = '\0';
    char *scope_start = strchr(msg_copy, '(');
    char *scope_end = scope_start ? strchr(scope_start, ')') : NULL;
    
    if (scope_start && scope_end) {
        *scope_end = '\0';
        scope_start++;
        commit->scope = strdup(scope_start);
        *scope_start = '\0';
    }
    
    // Check for breaking change marker
    commit->is_breaking = (strstr(msg_copy, "!") != NULL);
    
    // Parse type
    char *type_str = msg_copy;
    commit->type = parse_commit_type(type_str);
    
    // Parse description (skip leading whitespace)
    const char *desc = type_end + 1;
    while (*desc && isspace(*desc)) desc++;
    commit->description = strdup(desc);
    
    free(msg_copy);
    return commit->description ? RELEASY_SUCCESS : RELEASY_ERROR;
}

int changelog_parse_commit(const char *message, commit_info_t *commit) {
    if (!message || !commit) return CHANGELOG_ERR_PARSE_FAILED;
    
    memset(commit, 0, sizeof(commit_info_t));
    return parse_conventional_commit(message, commit);
}

static int write_commit_group(FILE *f, commit_type_t type, commit_info_t **commits, size_t count) {
    if (!f || !commits) return RELEASY_ERROR;
    
    // Count commits of this type
    size_t type_count = 0;
    for (size_t i = 0; i < count; i++) {
        if (commits[i]->type == type) type_count++;
    }
    
    if (type_count == 0) return RELEASY_SUCCESS;
    
    // Write section header
    fprintf(f, "\n### %s\n\n", changelog_commit_type_string(type));
    
    // Write commits
    for (size_t i = 0; i < count; i++) {
        commit_info_t *commit = commits[i];
        if (commit->type != type) continue;
        
        fprintf(f, "* ");
        if (commit->scope) {
            fprintf(f, "**%s:** ", commit->scope);
        }
        fprintf(f, "%s", commit->description);
        if (commit->is_breaking) {
            fprintf(f, " [BREAKING]");
        }
        if (commit->author) {
            fprintf(f, " (%s)", commit->author);
        }
        fprintf(f, "\n");
    }
    
    return RELEASY_SUCCESS;
}

int changelog_write(changelog_t *log) {
    if (!log || !log->entries || !log->count) return CHANGELOG_ERR_NO_COMMITS;
    
    FILE *f = fopen(log->file_path, "w");
    if (!f) return CHANGELOG_ERR_FILE_ACCESS;
    
    fprintf(f, "# Changelog\n\n");
    
    for (size_t i = 0; i < log->count; i++) {
        changelog_entry_t *entry = log->entries[i];
        
        // Write version header
        fprintf(f, "## [%s]", entry->version);
        if (entry->date) {
            fprintf(f, " - %s", entry->date);
        }
        fprintf(f, "\n");
        
        if (log->group_by_type) {
            // Write commits grouped by type
            for (int type = 0; type < COMMIT_TYPE_UNKNOWN; type++) {
                write_commit_group(f, type, entry->commits, entry->count);
            }
        } else {
            // Write commits in chronological order
            for (size_t j = 0; j < entry->count; j++) {
                commit_info_t *commit = entry->commits[j];
                fprintf(f, "* %s: %s", 
                        changelog_commit_type_string(commit->type),
                        commit->description);
                if (commit->is_breaking) {
                    fprintf(f, " [BREAKING]");
                }
                if (log->include_authors && commit->author) {
                    fprintf(f, " (%s)", commit->author);
                }
                fprintf(f, "\n");
            }
        }
        
        fprintf(f, "\n");
    }
    
    fclose(f);
    return RELEASY_SUCCESS;
}

int changelog_generate(changelog_t *log, git_repository *repo, const char *version) {
    if (!log || !repo || !version) return RELEASY_ERROR;
    
    // TODO: Implement git commit history traversal and changelog generation
    // This will require walking the git commit history between tags
    // and parsing conventional commits
    
    return RELEASY_SUCCESS;
}

int changelog_free_commit(commit_info_t *commit) {
    if (!commit) return RELEASY_SUCCESS;
    
    free(commit->scope);
    free(commit->description);
    free(commit->body);
    free(commit->footer);
    free(commit->commit_hash);
    free(commit->author);
    free(commit->date);
    
    memset(commit, 0, sizeof(commit_info_t));
    return RELEASY_SUCCESS;
}

int changelog_free_entry(changelog_entry_t *entry) {
    if (!entry) return RELEASY_SUCCESS;
    
    if (entry->commits) {
        for (size_t i = 0; i < entry->count; i++) {
            changelog_free_commit(entry->commits[i]);
            free(entry->commits[i]);
        }
        free(entry->commits);
    }
    
    free(entry->version);
    free(entry->previous_version);
    free(entry->date);
    
    memset(entry, 0, sizeof(changelog_entry_t));
    return RELEASY_SUCCESS;
}

void changelog_cleanup(changelog_t *log) {
    if (!log) return;
    
    if (log->entries) {
        for (size_t i = 0; i < log->count; i++) {
            changelog_free_entry(log->entries[i]);
            free(log->entries[i]);
        }
        free(log->entries);
    }
    
    free(log->file_path);
    memset(log, 0, sizeof(changelog_t));
}

const char *changelog_error_string(int error_code) {
    switch (error_code) {
        case RELEASY_SUCCESS:
            return "Success";
        case CHANGELOG_ERR_NO_COMMITS:
            return "No commits found";
        case CHANGELOG_ERR_INVALID_FORMAT:
            return "Invalid commit format";
        case CHANGELOG_ERR_FILE_ACCESS:
            return "Failed to access changelog file";
        case CHANGELOG_ERR_PARSE_FAILED:
            return "Failed to parse commit message";
        default:
            return "Unknown error";
    }
}

const char *changelog_commit_type_string(commit_type_t type) {
    if (type >= 0 && type <= COMMIT_TYPE_UNKNOWN) {
        return commit_type_strings[type];
    }
    return commit_type_strings[COMMIT_TYPE_UNKNOWN];
} 