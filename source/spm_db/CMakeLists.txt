#######################################################################################################################
### Copyright Advanced Micro Devices, Inc.
### SPDX-License-Identifier: MIT
###
### \author AMD Developer Tools Team
#######################################################################################################################

# Define library source files.
# The locations of the SPM DB source and header files.
set (CPP_SRC_PREFIX "${PROJECT_SOURCE_DIR}/source/spm_db/")
set (CPP_INC_PREFIX "${PROJECT_SOURCE_DIR}/include/spm_db/")

# C++ Source files and internal header files for SPM DB.
set(CPP_SRC "raw_spm_database.cpp" "derived_raw_builder.cpp" "derived_indexed_builder.cpp" "custom_counters.cpp" "gpa_counters_loader.cpp" "spm_error_internal.h")
# Public header files for SPM DB.
set(CPP_INC "raw_spm_database.h" "derived_spm_database.h" "derived_raw_builder.h" "derived_indexed_builder.h" "spm_error.h" "custom_counters.h" "gpa_counters_loader.h")

# Prepend file locations to filenames to get full paths to sources and headers.
list(TRANSFORM CPP_SRC PREPEND ${CPP_SRC_PREFIX})
list(TRANSFORM CPP_INC PREPEND ${CPP_INC_PREFIX})

add_library(spm_db STATIC ${CPP_SRC} ${CPP_INC})

# Configure compilation flags.

if (WIN32)
    target_compile_definitions(spm_db PRIVATE NOMINMAX)
endif()

# Configure include paths.
# include headers for SPM DB lib
target_include_directories(spm_db PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_include_directories(spm_db PRIVATE "${PROJECT_SOURCE_DIR}/include/spm_db")

# Link to GPA headers target
target_link_libraries(spm_db PUBLIC gpa_counters_headers)

devtools_target_options(spm_db)

