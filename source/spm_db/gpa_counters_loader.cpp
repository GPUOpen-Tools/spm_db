//============================================================================================
// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
//
/// @author AMD Developer Tools
/// @file
/// @brief This is a dynamic loader module for the GPUPerfAPICounters library.
//============================================================================================

#include "gpa_counters_loader.h"

GPUPerfAPICountersEntryPoints* GPUPerfAPICountersEntryPoints::instance_ = nullptr;
#ifdef GPA_COUNTERS_LIB_KEEP_LOADED
std::once_flag GPUPerfAPICountersEntryPoints::init_instance_flag_;
#endif
