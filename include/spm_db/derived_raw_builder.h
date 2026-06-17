//=============================================================================
// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
//
/// @author AMD Developer Tools Team
/// @file
/// @brief Defines a class for building a derived SPM DB from raw SPM counters.
/// A DerivedFromRawSpmBuilder is provided with a raw SPM database
/// and a number of GPA and formula counter definitions, from which it
/// creates a derived SPM database.
//=============================================================================

#ifndef SPMDB_SPM_DB_DERIVED_RAW_BUILDER_H_
#define SPMDB_SPM_DB_DERIVED_RAW_BUILDER_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "derived_spm_database.h"
#include "raw_spm_database.h"

namespace spm_db
{
    /// @brief A builder for a derived SPM DB which computes its counters from raw hardware counters.
    ///
    /// The builder is given a raw counter DB for its source data.
    /// Counters use a GPA counter name or a custom counter formula (see TODO reference) to define how their samples are computed,
    /// and their components are given by a list of counter names and relation names.
    ///
    /// Groups are given their members by name.
    ///
    /// When the builder is requested to construct a derived SPM data set, it first performs validation by
    /// checking whether all counters named by groups and other counters are present,
    /// and only includes them in the group or counter components if so.
    class DerivedFromRawSpmBuilder
    {
    public:
        /// @brief A component of a counter to be built.
        struct Component
        {
            std::string relation_name;  /// The relation of the component counter to its parent counter.
            std::string counter_name;   /// The canonical name of the component counter.
        };

        /// @brief Construct a builder to create a derived SPM DB from raw data and a set of counter definitions.
        ///
        /// @param [in] raw_db The raw hardware counters from which to compute derived counters.
        ///                    The constructed object must not outlive the referenced raw SPM DB.
        /// @param [in] sampling_interval The time in shader clocks between samples in the raw SPM DB.
        /// @param [in] ctx A GPA counter context valid for the hardware the raw data was captured on.
        /// @param [in] filter_bad_values A flag indicating whether to remove derived GPA counters with negative sample values.
        /// @param [in] progress_callback Optional. A function that will be called as progress is made towards building the derived SPM DB.
        ///                               The function will be called with the progress as a float between 0 and 1.
        ///                               If no function is provided, a no-op will be used by default.
        DerivedFromRawSpmBuilder(
            const RawSpmDataBase&      raw_db,
            uint32_t                   sampling_interval,
            GpaCounterContext          ctx,
            bool                       filter_bad_values,
            std::function<void(float)> progress_callback = []([[maybe_unused]] float progress) {})
            : raw_db_(raw_db)
            , sampling_interval_(sampling_interval)
            , gpa_counter_ctx_(ctx)
            , filter_bad_counters_(filter_bad_values)
            , progress_callback_(progress_callback)
        {
        }

        // TODO: Take an optional progress callback and cancellation flag.

        /// @brief Add a GPA-based counter to be included in the final derived SPM DB.
        ///
        /// @param [in] gpa_counter_name The name of the counter to add as given by GPA.
        /// @param [in] display_name A friendly display name for the counter.
        /// @param [in] components The list of components for this counter.
        ///
        /// @retval
        /// Ok on success. The counter will be computed at build time.
        /// @retval
        /// ErrorNotFound if a counter named by gpa_counter_name does not exist in the builder's GPA counter context
        ///               or a required raw counter is missing from the builder's raw counter DB.
        /// @retval
        /// ErrorUnavailable if the GPA library was unavailable.
        Result AddGpaCounter(std::string gpa_counter_name, std::string display_name, std::vector<Component> components);

        /// @brief Add a custom formula counter to be included in the final derived SPM DB.
        ///
        /// @param [in] name A unique name for the counter.
        /// @param [in] formula The formula to use to compute the counter.
        ///
        /// @retval
        /// Ok on success. The counter will be computed at build time.
        /// @retval
        /// ErrorNotFound if a required raw counter is not present in the builder's raw counter DB.
        Result AddCustomFormulaCounter(std::string name, std::string formula);

        /// @brief Add a counter group to be included in the final derived SPM DB.
        ///
        /// @param [in] name The name of the group.
        /// @param [in] description A description for the group.
        /// @param [in] member_names A list of names of counters in the group.
        ///
        /// @retval
        /// Ok on success.
        /// @return
        /// ErrorInvalidSize if member_names is empty.
        Result AddGroup(std::string name, std::string description, const std::vector<std::string>& member_names);

        /// @brief Build a derived SPM DB based on the parts added to this builder.
        ///
        /// If some counters referenced as group members are absent,
        /// a DB will be created and affected groups will contain only available counters.
        /// If no counters are available, the group will not be included in the DB.
        ///
        /// If components are missing, a DB will not be created.
        ///
        /// In the event that a custom counter is missing required derived counters, a DB will not be created.
        ///
        /// In the event that a counter cannot be computed due to a GPA failure
        /// or custom counter calculation failure, creation of the DB will succeed and the affected counter will not be included.
        ///
        /// @param [out] out_derived_db A unique_ptr that will contain a reference to the new derived SPM DB on success.
        ///
        /// @retval
        /// Ok on success.
        /// @retval
        /// ErrorNotFound if not all GPA derived counters required for computing custom counters are present.
        /// @retval
        /// ErrorUnavailable if GPA is not loaded.
        Result Build(std::unique_ptr<DerivedSpmDataBase>& out_derived_db);

    private:
        /// @brief Data used to build a GPA derived counter.
        struct GpaCounter
        {
            std::string                  gpa_counter_name;   /// The name of this counter as given by GPA.
            uint32_t                     gpa_counter_index;  /// The GPA counter index corresponding to the name in its builder's counter context.
            const GpaDerivedCounterInfo* gpa_counter_info;   /// The GPA counter info corresponding to the counter index in its builder's counter context.
            std::string                  display_name;       /// The display name of this counter.
            std::vector<Component>       components;         /// The list of components for this counter.
        };

        /// @brief Data used to build a custom derived counter.
        struct FormulaCounter
        {
            std::string name;     /// The name of this counter.
            std::string formula;  /// The formula used to compute this counter.
            // We could add a display name, description, and components here as well
            // but we currently don't have a way to represent them in the custom counter definition format.
        };

        /// @brief Data used to build a counter group.
        struct Group
        {
            std::string              name;          /// The name of this group.
            std::string              description;   /// A description of this group.
            std::vector<std::string> member_names;  /// The names of the members of this group.
        };

        /// @brief Computes sample values for a single counter.
        ///
        /// @param [in]  counter The GPA counter to compute samples for.
        /// @param [out] samples A vector that will hold the computed samples.
        /// @param [out] out_contains_bad_values True if the output sample list contains negative values.
        ///
        /// @retval
        /// Ok on success.
        /// @retval
        /// ErrorInvalidValue if GPA fails to compute sample values.
        /// @retval
        /// ErrorUnavailable if GPA is not loaded.
        Result ComputeGpaCounterSamples(const GpaCounter& counter, std::vector<double>& samples, bool& out_contains_bad_values);

        /// @brief Correct noisy cache counter readings if they are deemed valid (as defined by CacheHitInvalidValueIsNoise)
        ///        or delete them if they are invalid and we are filtering bad counters.
        ///
        /// Helper function for ComputeGpaCounterSamples.
        ///
        /// @param [in, out] derived_counters  The derived counter DB being built.
        void CorrectNoisyCacheCounters(DerivedSpmDataBase& derived_counters);

        const RawSpmDataBase& raw_db_;             ///< A reference to the raw SPM DB to derive counters from.
        uint32_t              sampling_interval_;  ///< The number of shader clocks between SPM samples.
        GpaCounterContext     gpa_counter_ctx_;    ///< A GPA counter context valid for the hardware that captured the source raw counters.

        bool filter_bad_counters_;  ///< If true, do not include counters with negative values in the final output.

        /// @brief Map from GPA name to GPA counter definitions.
        std::unordered_map<std::string, GpaCounter> gpa_counters_;

        /// @brief Map from formula counter names to formula counter definitions.
        std::unordered_map<std::string, FormulaCounter> formula_counters_;

        /// @brief List of counter group definitions.
        std::vector<Group> groups_;

        /// @brief A function that will be called when progress is made during building the derived SPM DB.
        /// The function will be given the progress as a float between 0 and 1.
        std::function<void(float)> progress_callback_;
    };
}  // namespace spm_db

#endif  // !SPMDB_SPM_DB_DERIVED_RAW_BUILDER_H_
