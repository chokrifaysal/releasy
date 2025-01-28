#include "releasy.h"
#include "ui.h"
#include "git_ops.h"

releasy_config_t g_config = {0};

static struct option long_options[] = {
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'v'},
    {"dry-run", no_argument, 0, 'd'},
    {"config", required_argument, 0, 'c'},
    {"env", required_argument, 0, 'e'},
    {"user-name", required_argument, 0, 'n'},
    {"user-email", required_argument, 0, 'm'},
    {"interactive", no_argument, 0, 'i'},
    {0, 0, 0, 0}
};

static void print_usage(void) {
    printf("Usage: releasy [OPTIONS] COMMAND\n\n"
           "Options:\n"
           "  -h, --help         Show this help message\n"
           "  -v, --version      Show version information\n"
           "  -d, --dry-run      Simulate actions without making changes\n"
           "  -c, --config       Specify config file path\n"
           "  -e, --env          Target environment for deployment\n"
           "  -n, --user-name    Git user name\n"
           "  -m, --user-email   Git user email\n"
           "  -i, --interactive  Enable interactive mode\n\n"
           "Commands:\n"
           "  init      Initialize release configuration\n"
           "  release   Create a new release\n"
           "  deploy    Deploy to target environment\n"
           "  rollback  Revert to previous release\n");
}

int releasy_parse_args(int argc, char **argv) {
    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "hvdc:e:n:m:i",
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
            case 'n':
                g_config.user_name = strdup(optarg);
                break;
            case 'm':
                g_config.user_email = strdup(optarg);
                break;
            case 'i':
                g_config.interactive = 1;
                break;
            default:
                return RELEASY_ERROR;
        }
    }

    return RELEASY_SUCCESS;
}

int releasy_ensure_user_config(void) {
    git_context_t ctx;
    int ret = git_ops_init(&ctx);
    if (ret != RELEASY_SUCCESS) return ret;

    // If user info was provided via CLI, use it
    if (g_config.user_name && g_config.user_email) {
        ret = git_ops_set_user(&ctx, g_config.user_name, g_config.user_email);
        git_ops_cleanup(&ctx);
        return ret;
    }

    // Try to get from git config
    ret = git_ops_get_user_from_git(&ctx);
    if (ret == RELEASY_SUCCESS) {
        g_config.user_name = strdup(ctx.user_name);
        g_config.user_email = strdup(ctx.user_email);
        git_ops_cleanup(&ctx);
        return RELEASY_SUCCESS;
    }

    // Try environment variables
    ret = git_ops_get_user_from_env(&ctx);
    if (ret == RELEASY_SUCCESS) {
        g_config.user_name = strdup(ctx.user_name);
        g_config.user_email = strdup(ctx.user_email);
        git_ops_cleanup(&ctx);
        return RELEASY_SUCCESS;
    }

    // If interactive mode is enabled, prompt user
    if (g_config.interactive) {
        ui_context_t ui_ctx = {0};
        ui_ctx.interactive = 1;
        ui_init(&ui_ctx);

        char name[256] = {0};
        char email[256] = {0};
        printf("Git user configuration not found. Please provide:\n");
        printf("Name: ");
        if (fgets(name, sizeof(name), stdin)) {
            name[strcspn(name, "\n")] = 0;
        }
        printf("Email: ");
        if (fgets(email, sizeof(email), stdin)) {
            email[strcspn(email, "\n")] = 0;
        }

        if (name[0] && email[0]) {
            g_config.user_name = strdup(name);
            g_config.user_email = strdup(email);
            ui_cleanup(&ui_ctx);
            return RELEASY_SUCCESS;
        }
        ui_cleanup(&ui_ctx);
    }

    git_ops_cleanup(&ctx);
    return GIT_ERR_NO_USER_CONFIG;
}

void releasy_cleanup(void) {
    free(g_config.config_path);
    free(g_config.target_env);
    free(g_config.user_name);
    free(g_config.user_email);
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

    ret = releasy_ensure_user_config();
    if (ret != RELEASY_SUCCESS) {
        fprintf(stderr, "Error: %s\n", git_ops_error_string(ret));
        fprintf(stderr, "Please configure git user.name and user.email, or use --user-name and --user-email options\n");
        return ret;
    }

    atexit(releasy_cleanup);
    return RELEASY_SUCCESS;
} 