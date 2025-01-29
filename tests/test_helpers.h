#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fnmatch.h>

// Check if a file exists that matches the given pattern
static int file_exists_with_pattern(const char *pattern) {
    DIR *dir = opendir(".");
    if (!dir) return 0;
    
    struct dirent *entry;
    int found = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        if (fnmatch(pattern, entry->d_name, 0) == 0) {
            found = 1;
            break;
        }
    }
    
    closedir(dir);
    return found;
}

// Remove all files that match the given pattern
static void remove_files_with_pattern(const char *pattern) {
    DIR *dir = opendir(".");
    if (!dir) return;
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (fnmatch(pattern, entry->d_name, 0) == 0) {
            remove(entry->d_name);
        }
    }
    
    closedir(dir);
}

#endif // TEST_HELPERS_H 