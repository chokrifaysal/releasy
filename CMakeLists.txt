cmake_minimum_required(VERSION 3.13)
project(releasy VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBGIT2 REQUIRED libgit2)
pkg_check_modules(JSON-C REQUIRED json-c)

add_executable(releasy
    src/main.c
    src/semver.c
    src/git_ops.c
    src/deploy.c
    src/config.c
    src/ui.c
)

target_include_directories(releasy
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LIBGIT2_INCLUDE_DIRS}
    ${JSON-C_INCLUDE_DIRS}
)

target_link_libraries(releasy
    PRIVATE
    ${LIBGIT2_LIBRARIES}
    ${JSON-C_LIBRARIES}
)

install(TARGETS releasy
    RUNTIME DESTINATION bin
)

enable_testing()
add_executable(test_semver tests/test_semver.c src/semver.c)
target_include_directories(test_semver PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME test_semver COMMAND test_semver) 