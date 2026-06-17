#######################################################################################################################
### Copyright Advanced Micro Devices, Inc.
### SPDX-License-Identifier: MIT
###
### \author AMD Developer Tools Team
#######################################################################################################################

cmake_minimum_required(VERSION 3.25)

project(SpmDb)

# Define version information
set(SPMDB_MAJOR_VERSION 1)
set(SPMDB_MINOR_VERSION 0)

if (NOT SPMDB_PATCH_NUMBER)
    set(SPMDB_PATCH_NUMBER 0)
endif ()

if (NOT SPMDB_BUILD_NUMBER)
    set(SPMDB_BUILD_NUMBER 0)
endif ()

string(TIMESTAMP DATE "\"%m/%d/%Y\"")
string(TIMESTAMP YEAR "\"%Y\"")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if (PROJECT_IS_TOP_LEVEL)
    include(dev_tools)
endif ()


# Fetch external dependencies
include(spm_db_external)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Buildinfo.properties.in" "${CMAKE_CURRENT_SOURCE_DIR}/Buildinfo.properties")

add_subdirectory(source/spm_db)

