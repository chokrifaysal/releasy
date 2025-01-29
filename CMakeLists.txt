cmake_minimum_required(VERSION 3.10)
project(releasy C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONC REQUIRED json-c)
pkg_check_modules(LIBGIT2 REQUIRED libgit2)

# Add source files
set(SOURCES
    src/main.c
    src/deploy.c
    src/init.c
    src/git_ops.c
    src/semver.c
    src/ui.c
    src/changelog.c
)

# Create main executable
add_executable(releasy ${SOURCES})
target_include_directories(releasy PRIVATE ${JSONC_INCLUDE_DIRS} ${LIBGIT2_INCLUDE_DIRS} include src)
target_link_libraries(releasy PRIVATE ${JSONC_LIBRARIES} ${LIBGIT2_LIBRARIES})

# Add test executables
add_executable(test_git_ops tests/test_git_ops.c src/git_ops.c src/semver.c)
add_executable(test_semver tests/test_semver.c src/semver.c)
add_executable(test_changelog tests/test_changelog.c src/changelog.c src/git_ops.c src/semver.c)

# Set include directories for test targets
target_include_directories(test_git_ops PRIVATE ${LIBGIT2_INCLUDE_DIRS} include src)
target_include_directories(test_semver PRIVATE include src)
target_include_directories(test_changelog PRIVATE ${LIBGIT2_INCLUDE_DIRS} include src)

# Link libraries
target_link_libraries(test_git_ops ${LIBGIT2_LIBRARIES})
target_link_libraries(test_changelog ${LIBGIT2_LIBRARIES})

# Add tests
add_test(NAME test_git_ops 
         COMMAND test_git_ops ${CMAKE_SOURCE_DIR})
add_test(NAME test_semver 
         COMMAND test_semver)
add_test(NAME test_changelog 
         COMMAND test_changelog) 