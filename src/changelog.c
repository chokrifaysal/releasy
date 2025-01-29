#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "changelog.h"
#include "git_ops.h"
#include "semver.h"

static int tag_sorting_callback(const void *a, const void *b);

static const char *commit_type_strings[] = {
    "feat", "fix", "docs", "style", "refactor",
    "perf", "test", "build", "ci", "chore",
    "revert", "unknown"
};

static int tag_sorting_callback(const void *a, const void *b) {
    const char *tag_a = *(const char **)a;
    const char *tag_b = *(const char **)b;
    
    // Skip 'v' prefix if present
    if (tag_a[0] == 'v') tag_a++;
    if (tag_b[0] == 'v') tag_b++;
    
    semver_t ver_a, ver_b;
    semver_init(&ver_a);
    semver_init(&ver_b);
    
    if (semver_parse(tag_a, &ver_a) != 0 || semver_parse(tag_b, &ver_b) != 0) {
        semver_free(&ver_a);
        semver_free(&ver_b);
        return 0;
    }
    
    int result = semver_compare(&ver_a, &ver_b);
    semver_free(&ver_a);
    semver_free(&ver_b);
    return result;
}

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

static int get_commit_range(git_repository *repo, const char *from_tag, const char *to_tag,
                          git_revwalk **walker) {
    int error;
    git_oid from_oid, to_oid;

    // Get the "to" commit (newer)
    if (to_tag) {
        error = git_revparse_single((git_object **)&to_oid, repo, to_tag);
        if (error) return error;
    } else {
        error = git_reference_name_to_id(&to_oid, repo, "HEAD");
        if (error) return error;
    }

    // Initialize the revision walker
    error = git_revwalk_new(walker, repo);
    if (error) return error;

    git_revwalk_sorting(*walker, GIT_SORT_TIME);
    error = git_revwalk_push(*walker, &to_oid);
    if (error) {
        git_revwalk_free(*walker);
        return error;
    }

    // If we have a from tag, stop at that commit
    if (from_tag) {
        error = git_revparse_single((git_object **)&from_oid, repo, from_tag);
        if (error) {
            git_revwalk_free(*walker);
            return error;
        }
        error = git_revwalk_hide(*walker, &from_oid);
        if (error) {
            git_revwalk_free(*walker);
            return error;
        }
    }

    return RELEASY_SUCCESS;
}

static int extract_commit_metadata(git_commit *commit, commit_info_t *info) {
    if (!commit || !info) return RELEASY_ERROR;

    // Get commit hash
    const git_oid *oid = git_commit_id(commit);
    char hash[GIT_OID_HEXSZ + 1] = {0};
    git_oid_fmt(hash, oid);
    info->commit_hash = strdup(hash);

    // Get author information
    const git_signature *author = git_commit_author(commit);
    if (author) {
        size_t author_len = strlen(author->name) + strlen(author->email) + 4;
        info->author = malloc(author_len);
        if (info->author) {
            snprintf(info->author, author_len, "%s <%s>", author->name, author->email);
        }

        // Format date
        struct tm *tm = localtime(&author->when.time);
        char date[32];
        strftime(date, sizeof(date), "%Y-%m-%d", tm);
        info->date = strdup(date);
    }

    return RELEASY_SUCCESS;
}

int changelog_generate(changelog_t *log, git_repository *repo, const char *version) {
    if (!log || !repo || !version) return RELEASY_ERROR;

    // Create new changelog entry
    changelog_entry_t *entry = calloc(1, sizeof(changelog_entry_t));
    if (!entry) return RELEASY_ERROR;

    entry->version = strdup(version);
    if (!entry->version) {
        free(entry);
        return RELEASY_ERROR;
    }

    // Get current date
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char date[32];
    strftime(date, sizeof(date), "%Y-%m-%d", tm);
    entry->date = strdup(date);

    // Find previous version tag
    git_strarray tags = {0};
    int error = git_tag_list(&tags, repo);
    if (error == 0 && tags.count > 0) {
        // Sort tags by version
        qsort(tags.strings, tags.count, sizeof(char *), tag_sorting_callback);
        for (size_t i = 0; i < tags.count; i++) {
            if (git_ops_is_version_tag(NULL, tags.strings[i])) {
                entry->previous_version = strdup(tags.strings[i]);
                break;
            }
        }
    }
    git_strarray_free(&tags);

    // Initialize revision walker
    git_revwalk *walker = NULL;
    error = get_commit_range(repo, entry->previous_version, NULL, &walker);
    if (error) {
        changelog_free_entry(entry);
        return error;
    }

    // Walk through commits
    git_oid oid;
    size_t max_commits = 1000;  // Reasonable limit
    size_t commit_count = 0;
    commit_info_t **commits = NULL;

    while (git_revwalk_next(&oid, walker) == 0 && commit_count < max_commits) {
        git_commit *commit = NULL;
        error = git_commit_lookup(&commit, repo, &oid);
        if (error) continue;

        const char *message = git_commit_message(commit);
        if (message) {
            commit_info_t *info = calloc(1, sizeof(commit_info_t));
            if (info) {
                if (changelog_parse_commit(message, info) == RELEASY_SUCCESS) {
                    extract_commit_metadata(commit, info);

                    // Add to commits array
                    commit_info_t **new_commits = realloc(commits, 
                        (commit_count + 1) * sizeof(commit_info_t *));
                    if (new_commits) {
                        commits = new_commits;
                        commits[commit_count++] = info;
                    } else {
                        changelog_free_commit(info);
                        free(info);
                    }
                } else {
                    changelog_free_commit(info);
                    free(info);
                }
            }
        }
        git_commit_free(commit);
    }

    git_revwalk_free(walker);

    // Update entry with commits
    entry->commits = commits;
    entry->count = commit_count;

    // Add entry to changelog
    changelog_entry_t **new_entries = realloc(log->entries,
        (log->count + 1) * sizeof(changelog_entry_t *));
    if (!new_entries) {
        changelog_free_entry(entry);
        return RELEASY_ERROR;
    }

    log->entries = new_entries;
    log->entries[log->count++] = entry;

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
        case CHANGELOG_ERR_GIT_WALK_FAILED:
            return "Failed to walk git commit history";
        case CHANGELOG_ERR_GIT_LOOKUP_FAILED:
            return "Failed to lookup git commit";
        case CHANGELOG_ERR_MEMORY:
            return "Memory allocation failed";
        case CHANGELOG_ERR_TAG_NOT_FOUND:
            return "Version tag not found";
        case CHANGELOG_ERR_INVALID_RANGE:
            return "Invalid commit range";
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