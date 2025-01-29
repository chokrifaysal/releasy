#ifndef INIT_H
#define INIT_H

/**
 * Initialize a new releasy project.
 * 
 * This function creates the necessary directory structure and configuration files
 * for a new releasy project. It will create:
 * - Directory structure (config, logs, status, scripts)
 * - Default configuration file
 * - Script templates
 * 
 * @param config_path Path where the configuration file should be created
 * @param user_name Git user name for deployment attribution
 * @param user_email Git user email for deployment attribution
 * @return RELEASY_SUCCESS on success, error code otherwise
 */
int init_project(const char *config_path, const char *user_name, const char *user_email);

#endif // INIT_H 