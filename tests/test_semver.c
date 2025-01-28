#include <stdio.h>
#include "semver.h"

static void test_version(const char *version_str) {
    semver_t version;
    semver_init(&version);
    
    int ret = semver_parse(version_str, &version);
    if (ret != RELEASY_SUCCESS) {
        printf("Failed to parse '%s': %s\n", version_str, semver_error_string(ret));
        return;
    }

    ret = semver_validate(&version);
    if (ret != RELEASY_SUCCESS) {
        printf("Invalid version '%s': %s\n", version_str, semver_error_string(ret));
        semver_free(&version);
        return;
    }

    char *str = semver_to_string(&version);
    printf("Parsed version: %s -> %s\n", version_str, str);
    free(str);
    semver_free(&version);
}

int main(void) {
    printf("Testing semantic version parsing:\n\n");
    
    test_version("1.0.0");
    test_version("2.1.0-alpha");
    test_version("1.0.0-alpha+001");
    test_version("1.0.0-beta.2+exp.sha.5114f85");
    test_version("1.0.0+20130313144700");
    test_version("invalid");
    test_version("1.0");
    test_version("1.0.0-");
    
    return 0;
} 