//============================================================================================
// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
//
/// @author AMD Developer Tools
/// @file
/// @brief This is a dynamic loader module for the GPUPerfAPICounters library.
//============================================================================================

#ifndef SPMDB_SPM_DB_GPA_COUNTERS_LOADER_H_
#define SPMDB_SPM_DB_GPA_COUNTERS_LOADER_H_

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <tchar.h>
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <mutex>

#include <gpu_performance_api/gpu_perf_api_counters.h>

/// @brief Singleton class to handle loading the GPA Counters lib.
class GPUPerfAPICountersEntryPoints
{
public:
    /// @brief Gets the singleton instance.
    ///
    /// @return The singleton instance.
    static GPUPerfAPICountersEntryPoints* Instance()
    {
#ifdef GPA_COUNTERS_LIB_KEEP_LOADED
        std::call_once(init_instance_flag_, InitInstance);
#else
        if (nullptr == instance_)
        {
            InitInstance();
        }
#endif
        return instance_;
    }

    /// @brief Deletes the singleton instance.
    static void DeleteInstance()
    {
        if (nullptr != instance_)
        {
            GPUPerfAPICountersEntryPoints* copy_of_instance = instance_;
            instance_                                       = nullptr;
            delete copy_of_instance;
        }
    }

    /// @brief Checks if the entry points are valid.
    ///
    /// @return true if the entry points are valid.
    bool EntryPointsValid() const
    {
        return entry_points_valid_;
    }

    /// @brief Gets the GPA Counters lib function table.
    ///
    /// @return the GPA Counters lib function table.
    GpaCounterLibFuncTable* GetFuncTable()
    {
        if (entry_points_valid_)
        {
            return &func_table_;
        }

        return nullptr;
    }

private:
    bool entry_points_valid_ = true;  ///< Flag indicating if the GPACounters library entry points are valid.

#ifdef _WIN32
    HMODULE module_;  ///< The GPACounters library module handle.
#else
    void* module_;  ///< The GPACounters library module handle.
#endif
    GpaCounterLibFuncTable func_table_;  //<< The GPACounters library function table.

    /// @brief Attempts to initialize the specified GPACounters library entry point.
    ///
    /// @param [in] entry_point_name The name of the entry point to initialize.
    ///
    /// @return The address of the entry point or nullptr if the entry point could not be initialized.
    void* InitEntryPoint(const char* entry_point_name)
    {
        if (nullptr != module_)
        {
#ifdef _WIN32
            return GetProcAddress(module_, entry_point_name);
#else
            return dlsym(module_, entry_point_name);
#endif
        }

        return nullptr;
    }

#define GPA_INTERNAL_SUFFIX

#define GPA_DEBUG_SUFFIX "-d"

    /// @brief Private constructor.
    GPUPerfAPICountersEntryPoints()
    {
#ifdef _WIN32
        TCHAR exe_path[MAX_PATH] = {0};
        DWORD len                = GetModuleFileName(nullptr, exe_path, MAX_PATH);
        if (len == 0 || len == MAX_PATH)
        {
            module_             = nullptr;
            entry_points_valid_ = false;
            return;
        }

        // Remove the executable name to get the directory
        TCHAR* last_backslash = _tcsrchr(exe_path, _T('\\'));
        if (last_backslash)
        {
            *(last_backslash + 1) = _T('\0');
        }

        // Append the DLL name
        _tcscat_s(exe_path, MAX_PATH, _T("GPUPerfAPICounters-x64" GPA_INTERNAL_SUFFIX ".dll"));

        module_ = LoadLibrary(exe_path);
#else
        module_ = dlopen("libGPUPerfAPICounters" GPA_INTERNAL_SUFFIX ".so", RTLD_LAZY);
#endif
        if (nullptr == module_)
        {
#ifdef _WIN32
            // Remove the executable name to get the directory
            last_backslash = _tcsrchr(exe_path, _T('\\'));
            if (last_backslash)
            {
                *(last_backslash + 1) = _T('\0');
            }
            _tcscat_s(exe_path, MAX_PATH, _T("GPUPerfAPICounters-x64" GPA_DEBUG_SUFFIX GPA_INTERNAL_SUFFIX ".dll"));

            module_ = LoadLibrary(exe_path);
#else
            module_ = dlopen("libGPUPerfAPICounters" GPA_DEBUG_SUFFIX GPA_INTERNAL_SUFFIX ".so", RTLD_LAZY);
#endif
        }

        GpaCounterLibGetFuncTablePtrType gpa_counter_lib_get_func_table_fn =
            reinterpret_cast<GpaCounterLibGetFuncTablePtrType>(InitEntryPoint("GpaCounterLibGetFuncTable"));

        entry_points_valid_ &= nullptr != gpa_counter_lib_get_func_table_fn;

        if (entry_points_valid_)
        {
            GpaStatus status = gpa_counter_lib_get_func_table_fn(&func_table_);
            entry_points_valid_ &= status == kGpaStatusOk;
        }
    }

    /// @brief Destructor.
    virtual ~GPUPerfAPICountersEntryPoints()
    {
        if (nullptr != module_)
        {
#ifdef _WIN32
            FreeLibrary(module_);
#else
            dlclose(module_);
#endif
        }
        DeleteInstance();
    }

    /// @brief Disable copy constructor.
    GPUPerfAPICountersEntryPoints(const GPUPerfAPICountersEntryPoints&) = delete;

    /// @brief Disable move constructor.
    GPUPerfAPICountersEntryPoints(GPUPerfAPICountersEntryPoints&&) = delete;

    /// @brief Disable assignment operator.
    ///
    /// @return reference to object.
    GPUPerfAPICountersEntryPoints& operator=(GPUPerfAPICountersEntryPoints&) = delete;

    /// @brief Disable move operator.
    ///
    /// @return reference to object.
    GPUPerfAPICountersEntryPoints& operator=(GPUPerfAPICountersEntryPoints&&) = delete;

    static GPUPerfAPICountersEntryPoints* instance_;  ///< Static singleton instance.
#ifdef GPA_COUNTERS_LIB_KEEP_LOADED
    static std::once_flag init_instance_flag_;  ///< Static instance init flag for multithreading.
#endif

    /// @brief Initializes the singleton instance.
    static void InitInstance()
    {
        instance_ = new (std::nothrow) GPUPerfAPICountersEntryPoints;
    }
};

#endif  // !SPMDB_SPM_DB_GPA_COUNTERS_LOADER_H_
