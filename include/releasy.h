#ifndef RELEASY_H
#define RELEASY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <strings.h>

#define RELEASY_VERSION "0.1.0"
#define RELEASY_SUCCESS 0
#define RELEASY_ERROR -1

typedef struct {
    int verbose;
    int dry_run;
    char *config_path;
    char *target_env;
    char *user_name;
    char *user_email;
    int interactive;
} releasy_config_t;

extern releasy_config_t g_config;

void releasy_init(void);
void releasy_cleanup(void);
int releasy_parse_args(int argc, char **argv);
int releasy_ensure_user_config(void);

#endif 