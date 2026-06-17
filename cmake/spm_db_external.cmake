#######################################################################################################################
### Copyright Advanced Micro Devices, Inc.
### SPDX-License-Identifier: MIT
###
### @author AMD Developer Tools Team
#######################################################################################################################

include(FetchContent)

set (GPA_BUILD 5)
set (GPA_MAJOR 4)
set (GPA_MINOR 4)
set (GPA_PATCH 0)

if (WIN32)
    SET (GPA_PACKAGE_OS)
    SET (GPA_PACKAGE_EXT .zip)
elseif (LINUX)
    SET (GPA_PACKAGE_OS Linux-)
    SET (GPA_PACKAGE_EXT .tgz)
else ()
    MESSAGE (FATAL_ERROR "Unsupported OS: ${CMAKE_SYSTEM_NAME}")
endif()

SET(GPA_PACKAGE_PREFIX GPUPerfAPI-)
SET (GPA_URL_PREFIX https://github.com/GPUOpen-Tools/gpu_performance_api/releases/download/)

STRING (CONCAT GPA_RELEASE_TAG v${GPA_MAJOR}.${GPA_MINOR}-tag/)

STRING (CONCAT GPA_URL
        ${GPA_URL_PREFIX} ${GPA_RELEASE_TAG} ${GPA_PACKAGE_PREFIX} ${GPA_PACKAGE_OS}
        ${GPA_MAJOR} . ${GPA_MINOR} . ${GPA_PATCH} . ${GPA_BUILD}
        ${GPA_PACKAGE_EXT})

# Use different directories for internal and public builds so both can coexist
SET (GPA_SOURCE_DIR "${PROJECT_SOURCE_DIR}/external/gpu_perf_api")


FetchContent_Declare(
    gpu_perf_api
    URL ${GPA_URL}
    SOURCE_DIR "${GPA_SOURCE_DIR}"
)

FetchContent_MakeAvailable(gpu_perf_api)

# Create IMPORTED targets for GPA
# These targets are globally visible and work with both add_subdirectory and FetchContent

# Header-only INTERFACE library for GPA include directories
add_library(gpa_counters_headers INTERFACE IMPORTED GLOBAL)
set_target_properties(gpa_counters_headers PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${GPA_SOURCE_DIR}/include"
)

# Determine the GPA shared library name based on platform and build type
if (WIN32)
    set(GPA_SHARED_LIBRARY_NAME "GPUPerfAPICounters-x64.dll")
else()
    set(GPA_SHARED_LIBRARY_NAME "libGPUPerfAPICounters.so")
endif()

# MODULE IMPORTED target (loaded at runtime via LoadLibrary/dlopen, never linked)
set(GPA_SHAREDLIB_LOCATION "${GPA_SOURCE_DIR}/bin/${GPA_SHARED_LIBRARY_NAME}")
if (EXISTS "${GPA_SHAREDLIB_LOCATION}")
    add_library(gpa_counters MODULE IMPORTED GLOBAL)
    set_target_properties(gpa_counters PROPERTIES
        IMPORTED_LOCATION "${GPA_SHAREDLIB_LOCATION}"
    )
    message(STATUS "SPM_DB: Created gpa_counters target at ${GPA_SHAREDLIB_LOCATION}")
else()
    message(FATAL_ERROR "SPM_DB: GPA shared library not found at ${GPA_SHAREDLIB_LOCATION}. Ensure FetchContent successfully downloaded GPA.")
endif()

