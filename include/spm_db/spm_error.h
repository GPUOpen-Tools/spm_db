//=============================================================================
// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
//
/// @author AMD Developer Tools Team
/// @file
/// @brief Definitions for error-handling for the SPM DB library.
//=============================================================================

#ifndef SPMDB_SPM_DB_SPM_ERROR_H_
#define SPMDB_SPM_DB_SPM_ERROR_H_

namespace spm_db
{
    /// @brief The result of an SPM DB library operation.
    enum class Result
    {
        Ok,                  ///< The operation succeeded.
        ErrorInvalidSize,    ///< An object of invalid size was provided.
        ErrorInvalidValue,   ///< An invalid value was provided.
        ErrorOutOfRange,     ///< An out-of-range value was provided.
        ErrorAlreadyExists,  ///< A similar object already exists.
        ErrorNotFound,       ///< An object was not found.
        ErrorUnavailable     ///< A required resource was unavailable.
        // Add more values as needed
    };
}  // namespace spm_db

#endif  // !SPMDB_SPM_DB_SPM_ERROR_H_
