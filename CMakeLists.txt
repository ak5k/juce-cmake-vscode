# Windows: Clang GNU CLI for MSVC toolkit is recommended

cmake_minimum_required(VERSION 3.22)
project(myPlugin VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

# source path (see "pitchfork" project layout)
# change to any other path if needed
set(MY_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(MY_EXTERNAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external")

# easy dependency management
# see https://learn.microsoft.com/en-us/vcpkg/get_started/get-started-vscode?pivots=shell-cmd#3---set-up-the-project-files
# how to use vcpkg.json to add dependencies
set(USE_VCPKG ON)

# check if git repo exists
find_package(Git REQUIRED)
execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --is-inside-work-tree
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE IS_GIT_REPO
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
)

# minimal .gitignore to exclude some files from git
set(MY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/.gitignore")
if(NOT EXISTS "${MY_FILE}")
    set(MULTI_LINE_STRING
            [[
*[Bb]uild*/
JUCE/
vcpkg/
.idea/
.vs/
]]
    )
    file(WRITE "${MY_FILE}" "${MULTI_LINE_STRING}")
endif()

if(NOT IS_GIT_REPO STREQUAL "true")
    message("initializing git repo")
    execute_process(
            COMMAND ${GIT_EXECUTABLE} init
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            RESULT_VARIABLE GIT_SUBMOD_RESULT
    )

    execute_process(
            COMMAND ${GIT_EXECUTABLE} submodule add https://github.com/juce-framework/JUCE
            WORKING_DIRECTORY "${MY_EXTERNAL_DIR}"
            ERROR_QUIET
    )
endif()

# tries to use latest tag as version
execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_LATEST_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
)
if(NOT "${GIT_LATEST_TAG}" STREQUAL "")
    string(REGEX REPLACE "[^0-9.]" "" GIT_LATEST_TAG ${GIT_LATEST_TAG})
endif()
if(NOT "${GIT_LATEST_TAG}" STREQUAL "")
    project(${PROJECT_NAME} VERSION ${GIT_LATEST_TAG} LANGUAGES C CXX)
endif()

# some useful variables for config.h
message("Project version: ${PROJECT_VERSION}")
if(NOT DEFINED BUILD_NUMBER)
    set(BUILD_NUMBER 0)
endif()
if(NOT DEFINED GIT_TAG)
    set(GIT_TAG 0)
    set(BUILD_NUMBER 0)
endif()
string(TIMESTAMP BUILD_DATE "%Y%m%d")

if(NOT EXISTS "${MY_EXTERNAL_DIR}")
    file(MAKE_DIRECTORY "${MY_EXTERNAL_DIR}")
    # add JUCE as submodule
    execute_process(
            COMMAND ${GIT_EXECUTABLE} submodule add https://github.com/juce-framework/JUCE
            WORKING_DIRECTORY "${MY_EXTERNAL_DIR}"
            ERROR_QUIET
    )
    file(CREATE_LINK "${MY_EXTERNAL_DIR}/JUCE" "${CMAKE_CURRENT_SOURCE_DIR}/JUCE" SYMBOLIC)
endif()

# update submodules
execute_process(
        COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
        WORKING_DIRECTORY "${MY_EXTERNAL_DIR}"
        ERROR_QUIET
)

################################################################################
include(JUCE.cmake)
################################################################################

# set up new juce audio plugin project sources from example
if(NOT EXISTS "${MY_SOURCE_DIR}")
    file(MAKE_DIRECTORY "${MY_SOURCE_DIR}")
    file(CREATE_LINK "${MY_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/Source" SYMBOLIC)
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/examples/CMake/AudioPlugin/"
            DESTINATION
            "${MY_SOURCE_DIR}/")
    file(GLOB TXT_FILES "${MY_SOURCE_DIR}/*.txt")
    file(REMOVE "${TXT_FILES}")
endif()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
        "${MY_SOURCE_DIR}/*.c"
        "${MY_SOURCE_DIR}/*.cpp"
        "${MY_SOURCE_DIR}/*.h"
        "${MY_SOURCE_DIR}/*.hpp"
)

target_sources(${PROJECT_NAME}
        PRIVATE
        "${SOURCES}"
)

# compiler flags for source files
if(NOT MSVC)
    message("non-MSVC build")

    # set build flags for clang and gcc
    target_compile_options(${PROJECT_NAME}
            PRIVATE
            -Wall
            -Wextra
    )
endif()

# search and add headers to precompile
# see https://cmake.org/cmake/help/latest/command/target_precompile_headers.html
# feel free to add more paths
file(GLOB_RECURSE HEADERS
        "${CMAKE_CURRENT_SOURCE_DIR}/external/somelibrary/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/somelibrary*.hpp"
)
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT HEADERS STREQUAL "")
    target_precompile_headers(${PROJECT_NAME} PRIVATE "${HEADERS}")
endif()

# include directories from project root
# to use #include <pathInRoot/optionalSubDirs/z.h>
target_include_directories(${PROJECT_NAME}
        SYSTEM PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/external"
)

# convinience stuff begin here
################################################################################

# set up configure header file
# use with #include <configure.h>
# see https://cmake.org/cmake/help/latest/command/configure_file.html
# @VAR@ will be replaced with the value of CMake variable VAR
set(MY_FILE "configure.h")
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${MY_FILE}.in")
    set(MULTI_LINE_STRING
            [[
#pragma once
// NOLINTBEGIN
// clang-format off

#define BUILD_DATE @BUILD_DATE@
#define BUILD_NUMBER @BUILD_NUMBER@
#define GIT_TAG @GIT_TAG@
#define VERSION_MAJOR @PROJECT_VERSION_MAJOR@
#define VERSION_MINOR @PROJECT_VERSION_MINOR@
#define VERSION_PATCH @PROJECT_VERSION_PATCH@

// clang-format on
// NOLINTEND
]]
    )
    file(WRITE ${MY_FILE}.in "${MULTI_LINE_STRING}")
endif()
configure_file("${MY_FILE}.in" "${MY_FILE}" @ONLY)

# set up .vscode/launch.json task for Windows
# see https://code.visualstudio.com/docs/cpp/launch-json-reference
# to use, press F5 in VSCode
# change "program" to your 'test bench' executable path
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.vscode/launch.json")
    set(MULTI_LINE_STRING
            [[
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Launch host to debug JUCE plugin",
      "type": "cppvsdbg",
      "request": "launch",
      "program": "c:/Program Files/REAPER (x64)/reaper.exe",
      "args": [],
      "stopAtEntry": false,
      "cwd": "${fileDirname}",
      "environment": [],
      "preLaunchTask": "CMake: build"
    }
  ]
}
]]
    )
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.vscode")
    file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/.vscode/launch.json" "${MULTI_LINE_STRING}")
endif()

# if not CI build, set up use local vcpkg
if(DEFINED ENV{GITHUB_ACTIONS})
    message("Running in GitHub Actions")
else()
    # init vcpkg
    if(USE_VCPKG)
        # # create vcpkg.json if not exists
        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json")
            set(MULTI_LINE_STRING
                    [[
{
  "dependencies": []
}
]]
            )
            file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json" "${MULTI_LINE_STRING}")
        endif()
        execute_process(
                COMMAND ${GIT_EXECUTABLE} submodule add https://github.com/microsoft/vcpkg.git
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                RESULT_VARIABLE GIT_SUBMOD_RESULT
                ERROR_QUIET
        )
        include("${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    endif()
endif()

message("")