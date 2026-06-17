//=============================================================================
// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
//
/// @author AMD Developer Tools Team
/// @file
/// @brief Internal error-handling definitions for the SPM DB library.
//=============================================================================

#ifndef SPMDB_SPM_DB_SPM_ERROR_INTERNAL_H_
#define SPMDB_SPM_DB_SPM_ERROR_INTERNAL_H_

/// Helper macro to return error code y from a function when a specific condition, x, is not met.
#define RETURN_ON_ERROR(x, y) \
    if (!(x))                 \
    {                         \
        return (y);           \
    }

#endif  // !SPMDB_SPM_DB_SPM_ERROR_INTERNAL_H_
