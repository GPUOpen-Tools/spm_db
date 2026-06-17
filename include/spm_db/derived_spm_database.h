//=============================================================================
// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
//
/// @author AMD Developer Tools Team
/// @file
/// @brief Derived SPM database classes.
///
/// Derived SPM data is an aggregated, processed form of counter data
/// which is meaningful for users.
/// Derived SPM counters are organized by name and additionally accessible
/// by group.
/// The derived SPM database is constructed by a builder object
/// (See IndexedDerivedSpmBuilder and DerivedFromRawSpmBuilder)
/// and is immutable once constructed.
//=============================================================================

#ifndef SPMDB_SPM_DB_DERIVED_SPM_DATABASE_H_
#define SPMDB_SPM_DB_DERIVED_SPM_DATABASE_H_

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "raw_spm_database.h"
#include "spm_error.h"

#include "gpu_performance_api/gpu_perf_api_types.h"

namespace spm_db
{
    /// @brief A single derived SPM counter.
    ///
    /// A counter includes its samples,
    /// a list of components, and metadata
    /// including its canonical name (for GPA counters this is the name given by GPA),
    /// a friendly display name, a description.
    ///
    /// The components of a counter are a collection of named references to other counters
    /// from the same data set, which can be considered to be a piece of this counter.
    /// For example, the components of a cache hit percentage counter may include
    /// a component with the name "hits" and a reference to a counter for the number of hits for that cache.
    class DerivedSpmCounter
    {
        /// @brief Allow builders to construct an instance of this class.
        friend class DerivedFromRawSpmBuilder;
        friend class IndexedDerivedSpmBuilder;

    public:
        /// @brief Get the unit on the samples for this counter.
        /// @return The unit on the samples for this counter.
        GpaUsageType UsageType() const
        {
            return usage_type_;
        }

        /// @brief Get the canonical name of this counter.
        /// For GPA counters, this is the name given by GPA.
        /// @return The canonical name of this counter.
        const std::string& CanonicalName() const
        {
            return canonical_name_;
        }

        /// @brief Get the display name for this counter.
        /// @return The friendly display name for this counter.
        const std::string& DisplayName() const
        {
            return display_name_;
        }

        /// @brief Get the description for this counter.
        /// @return The description of this counter.
        const std::string& Description() const
        {
            return description_;
        }

        /// @brief Get the samples for this counter.
        /// @return This counter's samples.
        const std::vector<double>& Samples() const
        {
            return samples_;
        }

        /// @brief A counter component, describing one counter's relation to another counter.
        struct Component
        {
            /// @brief The relationship of the component to the counter for which it is a component.
            /// e.g. for the L0CacheHit counter, a component may be the L0CacheRequestCount counter
            /// with the relation name "Requests".
            std::string relation_name;
            /// @brief A reference to the component counter.
            /// The counter is owned by this counter's parent DerivedSpmDataBase.
            DerivedSpmCounter* counter;
        };

        /// @brief Get the components of this counter and their relation to this counter.
        /// @return The counter's components.
        const std::vector<Component>& Components() const
        {
            return components_;
        }

        // TODO
        // Disable the copy assignment operator and copy constructor?
        // This is difficult/impossible due to the use of containers.
        // Clients should only be able to use DerivedSpmCounter instances by reference,
        // as they are only valid for the lifetime of their parent DerivedSpmDataBase.

    private:
        /// @brief Create a derived SPM counter by taking ownership of a moved list of samples.
        /// @param [in]      canonical_name The canonical name of the counter.
        /// @param [in]      usage_type The usage type (unit) of the counter.
        /// @param [in]      display_name A friendly name for the counter.
        /// @param [in]      description A description of the counter.
        /// @param [in, out] samples The counter's samples.
        DerivedSpmCounter(std::string canonical_name, GpaUsageType usage_type, std::string display_name, std::string description, std::vector<double>&& samples)
            : canonical_name_(canonical_name)
            , usage_type_(usage_type)
            , display_name_(display_name)
            , description_(description)
            , components_()
            , samples_(samples)
        {
        }

        /// The canonical name of this counter. For counters defined by GPA, this is their name given by GPA.
        /// For custom counters, this is a name given by the author, used to refer to this counter within the same set of counter definitions.
        std::string  canonical_name_;
        GpaUsageType usage_type_;  ///< The unit of a sample of this counter.

        std::string display_name_;  ///< The friendly name of the counter.
        std::string description_;   ///< The description of the counter.

        /// References to component counters for this counter.
        std::vector<Component> components_;

        std::vector<double> samples_;  ///< The sampled values of this counter.
    };

    /// @brief A group of similar derived SPM counters from a derived SPM data set
    /// which make sense to be viewed together, e.g. the hit ratio for each level of cache.
    class DerivedSpmGroup
    {
        // Allow the builder classes to construct instances of this class.
        friend class DerivedFromRawSpmBuilder;
        friend class IndexedDerivedSpmBuilder;

    public:
        /// @brief Get the name of this group.
        /// @return The name of this group.
        const std::string& Name() const
        {
            return name_;
        }

        /// @brief Get the description of this group.
        /// @return The description of this group.
        const std::string& Description() const
        {
            return description_;
        }

        /// @brief Get the members of this group.
        ///
        /// Members are given by-reference, and are owned by the same DerivedSpmDataBase
        /// that owns this group; as long as the data set exists, all references will be valid.
        ///
        /// @return A vector containing references to the counters that make up this group.
        const std::vector<DerivedSpmCounter*>& Members() const
        {
            return members_;
        }

    private:
        /// @brief Construct a group with the given name, description, and member counters.
        /// @param name The name of this group.
        /// @param description A description of this group.
        /// @param members A vector containing references to the members of this group.
        /// The underlying counters must be owned by the same DB that will contain this group.
        DerivedSpmGroup(std::string name, std::string description, std::vector<DerivedSpmCounter*> members)
            : name_(name)
            , description_(description)
            , members_(members)
        {
        }

        std::string name_;         ///< The name of the group.
        std::string description_;  ///< The description of the group.

        /// References to the counters in the group. References are owned by the parent DB of the group.
        std::vector<DerivedSpmCounter*> members_;
    };

    /// @brief A full derived SPM data set.
    /// Provides counters indexed by canonical name and a list of counter groups.
    /// Each counter in a data set is sampled (or rather, constructed from raw counters sampled) at the same set of points.
    class DerivedSpmDataBase
    {
        /// @brief Allow builders to construct an instance of this class.
        friend class DerivedFromRawSpmBuilder;
        friend class IndexedDerivedSpmBuilder;

    public:
        /// @brief Get the map from canonical names to counters in this derived SPM DB.
        /// @return The name-to-counter map.
        const std::unordered_map<std::string, DerivedSpmCounter>& Counters() const
        {
            return counters_;
        }

        /// @brief Get the list of groups in this derived SPM DB.
        /// @return The list of groups.
        const std::vector<DerivedSpmGroup> Groups() const
        {
            return groups_;
        }

        /// @brief Get the number of samples per counter in this derived SPM DB.
        /// @return The number of samples.
        uint32_t NumSamples() const
        {
            return num_samples_;
        }

        // We cannot use the default copy operations since a derived SPM DB contains references to parts of itself,
        // which would become references to the source object in a copy.
        // If the need arises, we could add an explicit copy constructor that rewrites references.
        DerivedSpmDataBase(const DerivedSpmDataBase& other)           = delete;
        DerivedSpmDataBase& operator=(const DerivedSpmDataBase other) = delete;

        DerivedSpmDataBase(DerivedSpmDataBase&& other)            = default;
        DerivedSpmDataBase& operator=(DerivedSpmDataBase&& other) = default;

    private:
        // Make the default constructor private so that only a builder can create an empty DB.
        DerivedSpmDataBase() = default;

        uint32_t                                           num_samples_;  ///< The number of samples held by each counter.
        std::vector<DerivedSpmGroup>                       groups_;       ///< An array holding all group information.
        std::unordered_map<std::string, DerivedSpmCounter> counters_;     ///< A map from canonical names to counters.
    };
}  // namespace spm_db

#endif  // !SPMDB_SPM_DB_DERIVED_SPM_DATABASE_H_
