#ifndef RELEASY_SEMVER_H
#define RELEASY_SEMVER_H

#include <stdlib.h>
#include <regex.h>
#include "releasy.h"

#define SEMVER_ERR_INVALID_FORMAT -100
#define SEMVER_ERR_INVALID_MAJOR -101
#define SEMVER_ERR_INVALID_MINOR -102
#define SEMVER_ERR_INVALID_PATCH -103
#define SEMVER_ERR_INVALID_PRERELEASE -104
#define SEMVER_ERR_INVALID_BUILD -105
#define SEMVER_ERR_MEMORY -106

typedef struct {
    int major;
    int minor;
    int patch;
    char *prerelease;
    char *build;
} semver_t;

int semver_init(semver_t *version);
int semver_parse(const char *version_str, semver_t *version);
int semver_validate(const semver_t *version);
int semver_compare(const semver_t *v1, const semver_t *v2);
char *semver_to_string(const semver_t *version);
const char *semver_error_string(int error_code);
void semver_free(semver_t *version);

#endif 