#include "releasy.h"
#include "ui.h"

releasy_config_t g_config = {0};

static struct option long_options[] = {
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'v'},
    {"dry-run", no_argument, 0, 'd'},
    {"config", required_argument, 0, 'c'},
    {"env", required_argument, 0, 'e'},
    {0, 0, 0, 0}
};

static void print_usage(void) {
    printf("Usage: releasy [OPTIONS] COMMAND\n\n"
           "Options:\n"
           "  -h, --help     Show this help message\n"
           "  -v, --version  Show version information\n"
           "  -d, --dry-run  Simulate actions without making changes\n"
           "  -c, --config   Specify config file path\n"
           "  -e, --env      Target environment for deployment\n\n"
           "Commands:\n"
           "  init      Initialize release configuration\n"
           "  release   Create a new release\n"
           "  deploy    Deploy to target environment\n"
           "  rollback  Revert to previous release\n");
}

int releasy_parse_args(int argc, char **argv) {
    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "hvdc:e:",
           long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                print_usage();
                return 1;
            case 'v':
                printf("releasy version %s\n", RELEASY_VERSION);
                return 1;
            case 'd':
                g_config.dry_run = 1;
                break;
            case 'c':
                g_config.config_path = strdup(optarg);
                break;
            case 'e':
                g_config.target_env = strdup(optarg);
                break;
            default:
                return RELEASY_ERROR;
        }
    }

    return RELEASY_SUCCESS;
}

void releasy_cleanup(void) {
    free(g_config.config_path);
    free(g_config.target_env);
}

int main(int argc, char **argv) {
    int ret = releasy_parse_args(argc, argv);
    if (ret != RELEASY_SUCCESS) {
        return ret;
    }

    if (argc == 1) {
        print_usage();
        return 1;
    }

    atexit(releasy_cleanup);
    return RELEASY_SUCCESS;
} 