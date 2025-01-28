#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "semver.h"

#define SEMVER_REGEX "^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)(\\-([0-9A-Za-z-]+(\\.[0-9A-Za-z-]+)*))?(\\+([0-9A-Za-z-]+(\\.[0-9A-Za-z-]+)*))?$"

static regex_t semver_regex;
static int regex_compiled = 0;

int semver_init(semver_t *version) {
    if (!version) return RELEASY_ERROR;
    version->major = 0;
    version->minor = 0;
    version->patch = 0;
    version->prerelease = NULL;
    version->build = NULL;
    return RELEASY_SUCCESS;
}

static int compile_regex(void) {
    if (!regex_compiled) {
        if (regcomp(&semver_regex, SEMVER_REGEX, REG_EXTENDED) != 0) {
            return SEMVER_ERR_INVALID_FORMAT;
        }
        regex_compiled = 1;
    }
    return RELEASY_SUCCESS;
}

int semver_parse(const char *version_str, semver_t *version) {
    if (!version_str || !version) return RELEASY_ERROR;
    
    int ret = compile_regex();
    if (ret != RELEASY_SUCCESS) return ret;

    regmatch_t matches[10];
    if (regexec(&semver_regex, version_str, 10, matches, 0) != 0) {
        return SEMVER_ERR_INVALID_FORMAT;
    }

    char buffer[256];
    int len;

    len = matches[1].rm_eo - matches[1].rm_so;
    if (len >= sizeof(buffer)) return SEMVER_ERR_INVALID_MAJOR;
    strncpy(buffer, version_str + matches[1].rm_so, len);
    buffer[len] = '\0';
    version->major = atoi(buffer);

    len = matches[2].rm_eo - matches[2].rm_so;
    if (len >= sizeof(buffer)) return SEMVER_ERR_INVALID_MINOR;
    strncpy(buffer, version_str + matches[2].rm_so, len);
    buffer[len] = '\0';
    version->minor = atoi(buffer);

    len = matches[3].rm_eo - matches[3].rm_so;
    if (len >= sizeof(buffer)) return SEMVER_ERR_INVALID_PATCH;
    strncpy(buffer, version_str + matches[3].rm_so, len);
    buffer[len] = '\0';
    version->patch = atoi(buffer);

    if (matches[5].rm_so != -1) {
        len = matches[5].rm_eo - matches[5].rm_so;
        version->prerelease = malloc(len + 1);
        if (!version->prerelease) return SEMVER_ERR_MEMORY;
        strncpy(version->prerelease, version_str + matches[5].rm_so, len);
        version->prerelease[len] = '\0';
    }

    if (matches[8].rm_so != -1) {
        len = matches[8].rm_eo - matches[8].rm_so;
        version->build = malloc(len + 1);
        if (!version->build) {
            free(version->prerelease);
            version->prerelease = NULL;
            return SEMVER_ERR_MEMORY;
        }
        strncpy(version->build, version_str + matches[8].rm_so, len);
        version->build[len] = '\0';
    }

    return RELEASY_SUCCESS;
}

int semver_validate(const semver_t *version) {
    if (!version) return RELEASY_ERROR;
    if (version->major < 0) return SEMVER_ERR_INVALID_MAJOR;
    if (version->minor < 0) return SEMVER_ERR_INVALID_MINOR;
    if (version->patch < 0) return SEMVER_ERR_INVALID_PATCH;

    if (version->prerelease) {
        const char *p = version->prerelease;
        while (*p) {
            if (!isalnum(*p) && *p != '-' && *p != '.') {
                return SEMVER_ERR_INVALID_PRERELEASE;
            }
            p++;
        }
    }

    if (version->build) {
        const char *p = version->build;
        while (*p) {
            if (!isalnum(*p) && *p != '-' && *p != '.') {
                return SEMVER_ERR_INVALID_BUILD;
            }
            p++;
        }
    }

    return RELEASY_SUCCESS;
}

int semver_compare(const semver_t *v1, const semver_t *v2) {
    if (!v1 || !v2) return 0;

    if (v1->major != v2->major) {
        return v1->major - v2->major;
    }
    if (v1->minor != v2->minor) {
        return v1->minor - v2->minor;
    }
    if (v1->patch != v2->patch) {
        return v1->patch - v2->patch;
    }

    if (!v1->prerelease && !v2->prerelease) return 0;
    if (!v1->prerelease) return 1;
    if (!v2->prerelease) return -1;

    return strcmp(v1->prerelease, v2->prerelease);
}

char *semver_to_string(const semver_t *version) {
    if (!version) return NULL;

    size_t len = 32;
    if (version->prerelease) len += strlen(version->prerelease) + 1;
    if (version->build) len += strlen(version->build) + 1;

    char *str = malloc(len);
    if (!str) return NULL;

    if (version->prerelease && version->build) {
        snprintf(str, len, "%d.%d.%d-%s+%s",
                version->major, version->minor, version->patch,
                version->prerelease, version->build);
    } else if (version->prerelease) {
        snprintf(str, len, "%d.%d.%d-%s",
                version->major, version->minor, version->patch,
                version->prerelease);
    } else if (version->build) {
        snprintf(str, len, "%d.%d.%d+%s",
                version->major, version->minor, version->patch,
                version->build);
    } else {
        snprintf(str, len, "%d.%d.%d",
                version->major, version->minor, version->patch);
    }

    return str;
}

const char *semver_error_string(int error_code) {
    switch (error_code) {
        case RELEASY_SUCCESS:
            return "Success";
        case SEMVER_ERR_INVALID_FORMAT:
            return "Invalid version format";
        case SEMVER_ERR_INVALID_MAJOR:
            return "Invalid major version";
        case SEMVER_ERR_INVALID_MINOR:
            return "Invalid minor version";
        case SEMVER_ERR_INVALID_PATCH:
            return "Invalid patch version";
        case SEMVER_ERR_INVALID_PRERELEASE:
            return "Invalid prerelease identifier";
        case SEMVER_ERR_INVALID_BUILD:
            return "Invalid build metadata";
        case SEMVER_ERR_MEMORY:
            return "Memory allocation failed";
        default:
            return "Unknown error";
    }
}

void semver_free(semver_t *version) {
    if (!version) return;
    free(version->prerelease);
    free(version->build);
    version->prerelease = NULL;
    version->build = NULL;
} 