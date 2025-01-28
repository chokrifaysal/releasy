#ifndef RELEASY_SEMVER_H
#define RELEASY_SEMVER_H

#include <stdlib.h>
#include "releasy.h"

typedef struct {
    int major;
    int minor;
    int patch;
    char *prerelease;
    char *build;
} semver_t;

int semver_parse(const char *version_str, semver_t *version);
int semver_validate(const semver_t *version);
char *semver_to_string(const semver_t *version);
void semver_free(semver_t *version);

#endif 