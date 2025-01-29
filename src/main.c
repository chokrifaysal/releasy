#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include "releasy.h"
#include "git_ops.h"
#include "deploy.h"
#include "ui.h"
#include "semver.h"
#include "config.h"
#include "init.h"

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

static int handle_deploy_command(int argc, char **argv) {
    if (optind >= argc) {
        fprintf(stderr, "Error: Version argument is required for deploy command\n");
        return RELEASY_ERROR;
    }

    const char *version = argv[optind];
    semver_t parsed_version;
    if (semver_parse(version, &parsed_version) != RELEASY_SUCCESS) {
        fprintf(stderr, "Error: Invalid version format: %s\n", version);
        return RELEASY_ERROR;
    }

    if (!g_config.target_env) {
        fprintf(stderr, "Error: Target environment is required (use --env option)\n");
        return RELEASY_ERROR;
    }

    if (!g_config.config_path) {
        g_config.config_path = strdup("config/releasy.json");
        if (!g_config.config_path) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            return RELEASY_ERROR;
        }
    }

    deploy_context_t ctx = {0};
    int ret = deploy_init(&ctx);
    if (ret != RELEASY_SUCCESS) {
        fprintf(stderr, "Error: Failed to initialize deployment context\n");
        return ret;
    }

    deploy_target_t *target = malloc(sizeof(deploy_target_t));
    if (!target) {
        fprintf(stderr, "Error: Failed to allocate memory for target\n");
        return RELEASY_ERROR;
    }
    target->name = strdup(g_config.target_env);
    target->next = NULL;
    ctx.targets = target;
    ctx.dry_run = g_config.dry_run;
    ctx.user_name = g_config.user_name;
    ctx.user_email = g_config.user_email;

    ret = deploy_load_config(&ctx, g_config.config_path);
    if (ret != RELEASY_SUCCESS) {
        fprintf(stderr, "Error: %s\n", deploy_error_string(ret));
        deploy_cleanup(&ctx);
        return ret;
    }

    ret = deploy_set_target(&ctx, g_config.target_env);
    if (ret != RELEASY_SUCCESS) {
        fprintf(stderr, "Error: %s: %s\n", deploy_error_string(ret), g_config.target_env);
        deploy_cleanup(&ctx);
        return ret;
    }

    printf("Deploying version %s to %s environment...\n", version, g_config.target_env);
    if (g_config.dry_run) {
        printf("[DRY RUN] No changes will be made\n");
    }

    ret = deploy_execute(&ctx, version);
    if (ret != RELEASY_SUCCESS) {
        fprintf(stderr, "Error: %s\n", deploy_error_string(ret));
        deploy_cleanup(&ctx);
        return ret;
    }

    deploy_status_t status;
    deploy_get_status(&ctx, &status);
    printf("Deployment status: %s\n", deploy_status_string(status));

    deploy_cleanup(&ctx);
    return RELEASY_SUCCESS;
}

static int handle_rollback_command(void) {
    if (!g_config.target_env) {
        fprintf(stderr, "Error: Target environment is required (use --env option)\n");
        return RELEASY_ERROR;
    }

    if (!g_config.config_path) {
        g_config.config_path = strdup("config/releasy.json");
        if (!g_config.config_path) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            return RELEASY_ERROR;
        }
    }

    deploy_context_t ctx;
    int ret = deploy_init(&ctx);
    if (ret != RELEASY_SUCCESS) {
        fprintf(stderr, "Error: Failed to initialize deployment context\n");
        return ret;
    }

    deploy_target_t *target = malloc(sizeof(deploy_target_t));
    if (!target) {
        fprintf(stderr, "Error: Failed to allocate memory for target\n");
        return RELEASY_ERROR;
    }
    target->name = strdup(g_config.target_env);
    target->next = NULL;
    ctx.targets = target;
    ctx.dry_run = g_config.dry_run;
    ctx.user_name = g_config.user_name;
    ctx.user_email = g_config.user_email;

    ret = deploy_load_config(&ctx, g_config.config_path);
    if (ret != RELEASY_SUCCESS) {
        fprintf(stderr, "Error: %s\n", deploy_error_string(ret));
        deploy_cleanup(&ctx);
        return ret;
    }

    ret = deploy_set_target(&ctx, g_config.target_env);
    if (ret != RELEASY_SUCCESS) {
        fprintf(stderr, "Error: %s: %s\n", deploy_error_string(ret), g_config.target_env);
        deploy_cleanup(&ctx);
        return ret;
    }

    printf("Rolling back deployment in %s environment...\n", g_config.target_env);
    if (g_config.dry_run) {
        printf("[DRY RUN] No changes will be made\n");
    }

    ret = deploy_rollback(&ctx);
    if (ret != RELEASY_SUCCESS) {
        fprintf(stderr, "Error: %s\n", deploy_error_string(ret));
        deploy_cleanup(&ctx);
        return ret;
    }

    deploy_status_t status;
    deploy_get_status(&ctx, &status);
    printf("Rollback status: %s\n", deploy_status_string(status));

    deploy_cleanup(&ctx);
    return RELEASY_SUCCESS;
}

static int handle_init_command(const char *config_path, const char *user_name, const char *user_email) {
    if (!user_name || !user_email) {
        printf("Error: Git user name and email are required for initialization\n");
        return RELEASY_ERROR;
    }

    return init_project(config_path ? config_path : "config/releasy.json", user_name, user_email);
}

static int handle_release_command(void) {
    git_context_t ctx;
    int ret = git_ops_init(&ctx);
    if (ret != RELEASY_SUCCESS) {
        fprintf(stderr, "Error: Failed to initialize git context\n");
        return ret;
    }

    // Check if working directory is clean
    ret = git_ops_check_dirty(&ctx);
    if (ret != RELEASY_SUCCESS) {
        fprintf(stderr, "Error: Working directory is not clean. Please commit or stash your changes.\n");
        git_ops_cleanup(&ctx);
        return ret;
    }

    // Get current version from latest tag
    char current_version[32] = "0.0.0";
    ret = git_ops_get_latest_version(&ctx, current_version, sizeof(current_version));
    if (ret != RELEASY_SUCCESS && ret != GIT_ERR_NO_TAGS) {
        fprintf(stderr, "Error: Failed to get current version\n");
        git_ops_cleanup(&ctx);
        return ret;
    }

    // Parse current version
    semver_t current;
    if (semver_parse(current_version, &current) != RELEASY_SUCCESS) {
        fprintf(stderr, "Error: Invalid current version format: %s\n", current_version);
        git_ops_cleanup(&ctx);
        return RELEASY_ERROR;
    }

    // If in interactive mode, prompt for version bump type
    char new_version[32];
    if (g_config.interactive) {
        printf("Current version: %s\n", current_version);
        printf("Select version bump type:\n");
        printf("1. Major (breaking changes)\n");
        printf("2. Minor (new features)\n");
        printf("3. Patch (bug fixes)\n");
        printf("4. Custom version\n");
        printf("Choice (1-4): ");

        char choice[8];
        if (fgets(choice, sizeof(choice), stdin)) {
            choice[strcspn(choice, "\n")] = 0;
            switch (choice[0]) {
                case '1':
                    current.major++;
                    current.minor = 0;
                    current.patch = 0;
                    break;
                case '2':
                    current.minor++;
                    current.patch = 0;
                    break;
                case '3':
                    current.patch++;
                    break;
                case '4':
                    printf("Enter new version: ");
                    if (fgets(new_version, sizeof(new_version), stdin)) {
                        new_version[strcspn(new_version, "\n")] = 0;
                        if (semver_parse(new_version, &current) != RELEASY_SUCCESS) {
                            fprintf(stderr, "Error: Invalid version format\n");
                            git_ops_cleanup(&ctx);
                            return RELEASY_ERROR;
                        }
                    }
                    break;
                default:
                    fprintf(stderr, "Error: Invalid choice\n");
                    git_ops_cleanup(&ctx);
                    return RELEASY_ERROR;
            }
        }
    } else {
        // Default to patch version bump
        current.patch++;
    }

    // Format new version string
    snprintf(new_version, sizeof(new_version), "%d.%d.%d",
             current.major, current.minor, current.patch);

    printf("Creating new release: %s\n", new_version);
    if (g_config.dry_run) {
        printf("[DRY RUN] No changes will be made\n");
        git_ops_cleanup(&ctx);
        return RELEASY_SUCCESS;
    }

    // Create release tag
    ret = git_ops_create_tag(&ctx, new_version, g_config.user_name, g_config.user_email);
    if (ret != RELEASY_SUCCESS) {
        fprintf(stderr, "Error: Failed to create release tag\n");
        git_ops_cleanup(&ctx);
        return ret;
    }

    printf("Successfully created release %s\n", new_version);
    git_ops_cleanup(&ctx);
    return RELEASY_SUCCESS;
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

    const char *command = argv[optind];
    if (!command) {
        print_usage();
        return 1;
    }

    optind++;  // Move past the command

    if (strcmp(command, "deploy") == 0) {
        ret = handle_deploy_command(argc, argv);
    } else if (strcmp(command, "rollback") == 0) {
        ret = handle_rollback_command();
    } else if (strcmp(command, "init") == 0) {
        ret = handle_init_command(g_config.config_path, g_config.user_name, g_config.user_email);
    } else if (strcmp(command, "release") == 0) {
        ret = handle_release_command();
    } else {
        fprintf(stderr, "Error: Unknown command: %s\n", command);
        print_usage();
        ret = RELEASY_ERROR;
    }

    atexit(releasy_cleanup);
    return ret;
} 