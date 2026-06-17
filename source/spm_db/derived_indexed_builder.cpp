//=============================================================================
// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
//
/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for IndexedDerivedSpmBuilder.
//=============================================================================

#include "derived_indexed_builder.h"

#include <cstring>

#include "spm_error_internal.h"

namespace spm_db
{
    Result IndexedDerivedSpmBuilder::AddCounter(uint32_t                      index,
                                                std::string                   canonical_name,
                                                GpaUsageType                  usage_type,
                                                std::string                   display_name,
                                                std::string                   description,
                                                const std::vector<double>&    samples,
                                                const std::vector<Component>& components)
    {
        return AddCounter(index, canonical_name, usage_type, display_name, description, std::vector(samples), components);
    }

    Result IndexedDerivedSpmBuilder::AddCounter(uint32_t                      index,
                                                std::string                   canonical_name,
                                                GpaUsageType                  usage_type,
                                                std::string                   display_name,
                                                std::string                   description,
                                                std::vector<double>&&         samples,
                                                const std::vector<Component>& components)
    {
        RETURN_ON_ERROR(index < counters_.size(), Result::ErrorOutOfRange);
        RETURN_ON_ERROR(counters_.at(index) == std::nullopt, Result::ErrorAlreadyExists);
        for (const Component& c : components)
        {
            // TODO check for duplicate entries here?
            RETURN_ON_ERROR(c.counter_index < counters_.size(), Result::ErrorOutOfRange);
        }

        counters_.at(index) = {canonical_name, usage_type, display_name, description, std::move(samples), std::vector(components)};
        return Result::Ok;
    }

    Result IndexedDerivedSpmBuilder::AddGroup(std::string name, std::string description, const std::vector<uint32_t>& member_indices)
    {
        RETURN_ON_ERROR(!member_indices.empty(), Result::ErrorInvalidSize);
        for (uint32_t member_index : member_indices)
        {
            RETURN_ON_ERROR(member_index < counters_.size(), Result::ErrorOutOfRange);
        }

        groups_.push_back({name, description, std::vector(member_indices)});
        return Result::Ok;
    }

    Result IndexedDerivedSpmBuilder::Build(std::unique_ptr<DerivedSpmDataBase>& out_spm_db)
    {
        // Check that all expected counters have been added.
        for (std::optional<Counter> counter : counters_)
        {
            RETURN_ON_ERROR(counter != std::nullopt, Result::ErrorNotFound);
        }

        // We must use move constructor here since make_unique cannot access the constructor itself.
        std::unique_ptr<DerivedSpmDataBase> result = std::make_unique<DerivedSpmDataBase>(DerivedSpmDataBase());
        result->num_samples_                       = num_samples_;

        if (extract_missing_names_)
        {
            ExtractNames();
        }

        // Add counters to the DB under construction, but do not attach components yet.
        for (std::optional<Counter>& counter : counters_)
        {
            // We do not currently check for repeated canonical names (here or in AddCounter).
            // If the same counter is present twice, result will contain only the first instance.
            result->counters_.try_emplace(
                counter->canonical_name,
                DerivedSpmCounter(counter->canonical_name, counter->usage_type, counter->display_name, counter->description, std::move(counter->samples)));
        }

        // Add components to counters.
        for (std::optional<Counter> counter : counters_)
        {
            DerivedSpmCounter& result_counter = result->counters_.at(counter->canonical_name);
            // Find the components of each counter by index.
            // Since all indices are in-bounds and all indices are filled, the counters cannot be absent.
            for (const Component& component : counter->components)
            {
                const std::string& component_name = counters_.at(component.counter_index)->canonical_name;
                result_counter.components_.push_back({component.relation_name, &result->counters_.at(component_name)});
            }
        }

        // Add Groups.
        for (auto& group : groups_)
        {
            // Find group members by index.
            // Since all indices are in-bounds and all indices are filled, the counter cannot be absent.
            std::vector<DerivedSpmCounter*> member_counters;
            for (uint32_t member_index : group.counter_indices)
            {
                const std::string& member_name = counters_.at(member_index)->canonical_name;
                member_counters.push_back(&result->counters_.at(member_name));
            }
            // Add the group to the data set.
            result->groups_.push_back(DerivedSpmGroup(group.name, group.description, member_counters));
        }

        // Reset the builder as it cannot be used to construct the same derived DB again
        // since we moved the counter samples to the new derived DB.
        groups_.clear();
        counters_.clear();

        out_spm_db = std::move(result);
        return Result::Ok;
    }

    void IndexedDerivedSpmBuilder::ExtractNames()
    {
        // Map from display names of default counters (except cache counter components) to their canonical GPA names.
        std::unordered_map<std::string, std::string> name_map{{"Instruction cache hit", "InstCacheHit"},
                                                              {"Scalar cache hit", "ScalarCacheHit"},
                                                              {"L0 cache hit", "L0CacheHit"},
                                                              {"L1 cache hit", "L1CacheHit"},
                                                              {"L2 cache hit", "L2CacheHit"},
                                                              {"Ray box tests", "RayBoxTests"},
                                                              {"Ray triangle tests", "RayTriTests"},
                                                              {"LDS bank conflict", "CSLDSBankConflict"},
                                                              {"Gpu busy cycles", "GPUBusyCycles"},
                                                              {"Memory unit busy", "MemUnitBusy"},
                                                              {"Mem unit busy cycles", "MemUnitBusyCycles"},
                                                              {"Memory unit stalled", "MemUnitStalled"},
                                                              {"Mem unit stalled cycles", "MemUnitStalledCycles"},
                                                              {"Write unit stalled", "WriteUnitStalled"},
                                                              {"Write unit stalled cycles", "WriteUnitStalledCycles"},
                                                              {"Fetch size", "FetchSize"},
                                                              {"Write size", "WriteSize"},
                                                              {"Local video memory bytes", "LocalVidMemBytes"},
                                                              {"PCIe bytes", "PcieBytes"}};
        for (std::optional<Counter>& counter : counters_)
        {
            // Set canonical names of non-component based on display names.
            auto find_result = name_map.find(counter->display_name);
            if (find_result != name_map.end())
            {
                counter->canonical_name = find_result->second;
            }

            for (Component& component : counter->components)
            {
                Counter& component_counter = *counters_.at(component.counter_index);

                // Use component counters' display names as their relation names.
                std::string relation_name = component_counter.display_name;
                component.relation_name   = relation_name;

                // Cache counter component names from serialized derived SPM data are their relations to their parents.
                // Check whether the parent counter is a cache counter, and if so which cache level.
                const std::string& parent_name   = counter->canonical_name;
                size_t             cache_hit_pos = parent_name.find("CacheHit");
                if (cache_hit_pos == parent_name.size() - strlen("CacheHit"))
                {
                    // Recover the component counter's canonical and display names based their relation and parent.
                    std::string cache_lv          = parent_name.substr(0, cache_hit_pos);
                    std::string friendly_cache_lv = (cache_lv == "Inst") ? "Instruction" : cache_lv;

                    if (relation_name == "Requests")
                    {
                        component_counter.canonical_name = cache_lv + "CacheRequestCount";
                        component_counter.display_name   = friendly_cache_lv + " cache requests";
                    }
                    else if (relation_name == "Hits")
                    {
                        component_counter.canonical_name = cache_lv + "CacheHitCount";
                        component_counter.display_name   = friendly_cache_lv + " cache hits";
                    }
                    else if (relation_name == "Misses")
                    {
                        component_counter.canonical_name = cache_lv + "CacheMissCount";
                        component_counter.display_name   = friendly_cache_lv + " cache misses";
                    }
                }
            }
        }
    }
}  // namespace spm_db
