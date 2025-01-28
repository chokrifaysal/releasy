#include "semver.h"

int semver_parse(const char *version_str, semver_t *version) {
    return RELEASY_ERROR;
}

int semver_validate(const semver_t *version) {
    return RELEASY_ERROR;
}

char *semver_to_string(const semver_t *version) {
    return NULL;
}

void semver_free(semver_t *version) {
    if (!version) return;
    free(version->prerelease);
    free(version->build);
} 