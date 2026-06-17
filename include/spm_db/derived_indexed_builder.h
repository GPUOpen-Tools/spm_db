//=============================================================================
// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
//
/// @author AMD Developer Tools Team
/// @file
/// @brief Defines a class for building a derived SPM DB from existing data,
///        with counters indexed numerically.
/// This is useful for deserializing SPM data from RDF or legacy RGP formats.
//=============================================================================

#ifndef SPMDB_SPM_DB_DERIVED_INDEXED_BUILDER_H_
#define SPMDB_SPM_DB_DERIVED_INDEXED_BUILDER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "derived_spm_database.h"

namespace spm_db
{
    /// @brief A builder for a derived SPM DB.
    /// A derived SPM builder is given counters and groups which refer to their components and members by numeric index,
    /// and uses them to construct a derived SPM data set.
    ///
    /// When the builder is requested to construct a derived SPM data set, it first performs validation by
    /// checking whether all counters named by groups and other counters are present,
    /// and only includes them in the group or counter components if so.
    ///
    /// This builder is intended to be used for loading data from the RGP and RDF serialized forms of the derived SPM DB,
    /// which give a sequential numeric index to each counter. Counter components and groups refer to counters by these indices.
    ///
    /// Currently, not all counter name data is present in the serialized forms:
    /// They do not contain canonical GPA names for any counters, do not contain display names for counters used as components,
    /// and use the display name field for component counters to store their relation to their parent.
    /// To work around this, a placeholder or empty string can be provided to AddCounter for the missing names,
    /// and the relation can be provided as the display name for the display name of component counters.
    /// In this case, extract_missing_names = true should be passed to Build.
    class IndexedDerivedSpmBuilder
    {
    public:
        /// @brief Create a new builder to create a derived SPM DB.
        ///
        /// @param [in] num_counters The total number of counters to construct.
        /// @param [in] num_samples  The number of samples in each counter that will be added to the DB.
        /// @param [in] extract_missing_names Whether to derive name data which is missing from serialized SPM DB formats
        ///                                   and not provided to AddCounter.
        IndexedDerivedSpmBuilder(uint32_t num_counters, uint32_t num_samples, bool extract_missing_names)
            : counters_(num_counters, std::nullopt)
            , groups_()
            , num_samples_(num_samples)
            , extract_missing_names_(extract_missing_names)
        {
        }

        /// @brief A description of a component counter to construct.
        struct Component
        {
            std::string relation_name;  /// The relation of the component counter to its parent counter.
            uint32_t    counter_index;  /// The numeric index of the component counter.
        };

        /// @brief Add a counter to the derived SPM DB to build. Copy the counter values.
        ///
        /// @param [in] canonical_name The canonical name of the counter to add.
        /// @param [in] usage_type     The usage type (unit) of the counter to add.
        /// @param [in] display_name   The display name of the counter to add.
        /// @param [in] description    The description of the counter to add.
        /// @param [in] samples        The samples of the counter to add. The samples are copied from the input.
        /// @param [in] components     The component counters and their relations to the counter to add.
        ///
        /// @retval
        /// ErrorOutOfRange if index >= the number of counters this builder was constructed for.
        /// @retval
        /// ErrorAlreadyExists if a counter was already provided for the given index.
        /// @retval
        /// ErrorInvalidSize if the size of samples does not match the number of samples this builder was created for.
        /// @retval
        /// Ok on success.
        Result AddCounter(uint32_t                      index,
                          std::string                   canonical_name,
                          GpaUsageType                  usage_type,
                          std::string                   display_name,
                          std::string                   description,
                          const std::vector<double>&    samples,
                          const std::vector<Component>& components);
        /// @brief Add a counter to the derived SPM DB to build. Move the counter values.
        ///
        /// @param [in]     canonical_name The canonical name of the counter to add.
        /// @param [in]     usage_type     The usage type (unit) of the counter to add.
        /// @param [in]     display_name   The display name of the counter to add.
        /// @param [in]     description    The description of the counter to add.
        /// @param [in,out] samples        The samples of the counter to add. The samples are moved from the input.
        /// @param [in]     components     The component counters and their relations to the counter to add.
        ///
        /// @retval
        /// ErrorOutOfRange if index >= the number of counters this builder was constructed for.
        /// @retval
        /// ErrorAlreadyExists if a counter was already provided for the given index.
        /// @retval
        /// ErrorInvalidSize if the size of samples does not match the number of samples this builder was created for.
        /// @retval
        /// Ok on success.
        Result AddCounter(uint32_t                      index,
                          std::string                   canonical_name,
                          GpaUsageType                  usage_type,
                          std::string                   display_name,
                          std::string                   description,
                          std::vector<double>&&         samples,
                          const std::vector<Component>& components);

        /// @brief Add a group to the derived SPM DB to build.
        ///
        /// @param [in] name         The name of the group to add.
        /// @param [in] description  The description of the group to add.
        /// @param [in] member_names The canonical names of each member of the group to add.
        ///
        /// @retval
        /// ErrorOutOfRange if the index for any member >= the number of counters this builder was constructed for.
        /// @retval
        /// ErrorInvalidSize if the member list is empty.
        /// @retval
        /// Ok on success.
        Result AddGroup(std::string name, std::string description, const std::vector<uint32_t>& member_indices);

        /// @brief Build a derived counter DB from the provided counters and groups
        ///
        /// Each counter's component counters and each group's member counters are checked against the provided counters.
        /// If a counter is missing, it is not included in the group or component list.
        ///
        /// If the method succeeds, the builder's counters and groups are cleared,
        /// and the given unique_ptr is assigned to the newly-created derived SPM data.
        ///
        /// @retval
        /// ErrorNotFound if not all counter indices were populated.
        /// @retval
        /// Ok on success.
        Result Build(std::unique_ptr<DerivedSpmDataBase>& out_derived_db);

    private:
        /// @brief Helper for Build to extract missing names for counters.
        ///
        /// The serialized forms of derived SPM data that this builder is intended for use with
        /// do not contain canonical GPA names for any counters, do not contain display names for counters used as components,
        /// and use the display name field for component counters to store their relation to their parent.
        ///
        /// This method fills in the missing data and reassigns the display name to the correct field
        /// for the counters in this builder.
        ///
        /// This method assumes that all counters in the counters_ vector have a value.
        void ExtractNames();

        /// @brief A description of a counter to construct.
        /// Contains the data for the final counter, with components listed by their names.
        struct Counter
        {
            std::string            canonical_name;  /// The canonical name of the counter.
            GpaUsageType           usage_type;      /// The unit of the counter's samples.
            std::string            display_name;    /// The friendly display name of the counter to build.
            std::string            description;     /// The description of the counter to build.
            std::vector<double>    samples;         /// The samples of the counter to build.
            std::vector<Component> components;      /// The component counters of the counter to build.
        };

        /// @brief A description of a group to construct.
        /// Contains the data for the final group with member counters listed by name.
        struct Group
        {
            std::string           name;             /// The name of the group to build.
            std::string           description;      /// The description of the group to build.
            std::vector<uint32_t> counter_indices;  /// The numeric indices of each counter in the group to build.
        };

        std::vector<std::optional<Counter>> counters_;     /// A map of counter names to counters to construct.
        std::vector<Group>                  groups_;       /// A list of groups to construct.
        uint32_t                            num_samples_;  /// The number of samples per counter.
        /// Whether to derive name data which is missing from serialized SPM DB formats and not provided to AddCounter.
        bool extract_missing_names_;
    };
}  // namespace spm_db

#endif  // !SPMDB_SPM_DB_DERIVED_INDEXED_BUILDER_H_
