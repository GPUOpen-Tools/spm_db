//=============================================================================
// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
//
/// @author AMD Developer Tools Team
/// @file
/// Implementation of DerivedFromRawSpmBuilder.
//=============================================================================

#include "derived_raw_builder.h"

#include <cassert>
#include <cstring>
#include <sstream>

#include "custom_counters.h"
#include "gpa_counters_loader.h"
#include "spm_error_internal.h"

namespace spm_db
{
    Result DerivedFromRawSpmBuilder::AddCustomFormulaCounter(std::string name, std::string formula)
    {
        if (gpa_counters_.find(name) != gpa_counters_.end() || formula_counters_.find(name) != formula_counters_.end())
        {
            return Result::ErrorAlreadyExists;
        }

        // Ensure all HW counters used by the formula are present. We can't check derived counters here since they may not have been added to the builder yet.
        std::vector<Expression> stack;
        std::stringstream       ss(formula);
        for (std::string token; std::getline(ss, token, ',');)
        {
            DerivedSpmTokenType type = MatchCounterFormulaToken(token);
            if (type == kDerivedSpmTokenCounter)
            {
                auto block_and_instance = NameToBlockAndEvent(gpa_counter_ctx_, token);  // TODO we could cache these results since we use them a few times.
                if (block_and_instance.has_value())
                {
                    auto find_block = raw_db_.Counters().find(block_and_instance->first);
                    if (find_block == raw_db_.Counters().end())
                    {
                        return Result::ErrorNotFound;
                    }
                    const auto& block_counters = find_block->second;
                    auto        find_event     = block_counters.find(block_and_instance->second);
                    if (find_event == block_counters.end())
                    {
                        return Result::ErrorNotFound;
                    }
                }
            }
        }

        formula_counters_.emplace(name, FormulaCounter{name, formula});
        return Result::Ok;
    }

    Result DerivedFromRawSpmBuilder::AddGroup(std::string name, std::string description, const std::vector<std::string>& member_names)
    {
        RETURN_ON_ERROR(!member_names.empty(), Result::ErrorInvalidSize);
        groups_.push_back({name, description, member_names});
        return Result::Ok;
    }

    Result DerivedFromRawSpmBuilder::AddGpaCounter(std::string gpa_counter_name, std::string display_name, std::vector<Component> components)
    {
        GpaCounterLibFuncTable* gpa_func_table = GPUPerfAPICountersEntryPoints::Instance()->GetFuncTable();
        if (gpa_func_table == nullptr)
        {
            return Result::ErrorUnavailable;
        }

        if (gpa_counters_.find(gpa_counter_name) != gpa_counters_.end() || formula_counters_.find(gpa_counter_name) != formula_counters_.end())
        {
            return Result::ErrorAlreadyExists;
        }

        // Get the GPA index for the derived counter we're computing.
        GpaUInt32       gpa_counter_index = 0;
        GpaCounterParam counter_param{true, gpa_counter_name.c_str()};
        GpaStatus       gpa_status = gpa_func_table->GpaCounterLibGetCounterIndex(gpa_counter_ctx_, &counter_param, &gpa_counter_index);
        if (gpa_status != kGpaStatusOk)
        {
            assert(gpa_status == kGpaStatusErrorCounterNotFound);
            return Result::ErrorNotFound;
        }

        // Get GPA Counter Info on the derived counter via the index we found
        // in order to determine which raw hardware counters are required to compute it.
        const GpaCounterInfo* gpa_counter_info;
        gpa_status = gpa_func_table->GpaCounterLibGetCounterInfo(gpa_counter_ctx_, gpa_counter_index, &gpa_counter_info);
        if (gpa_status != kGpaStatusOk || !gpa_counter_info->is_derived_counter)
        {
            return Result::ErrorInvalidValue;
        }

        const GpaDerivedCounterInfo* derived_counter_info = gpa_counter_info->gpa_derived_counter;
        GpaUInt32                    hw_counter_count     = derived_counter_info->gpa_hw_counter_count;  // Number of raw counters that make up this counter.
        assert(hw_counter_count > 0);                                                                    // GPA Error: No raw counters for this derived counter
        if (hw_counter_count == 0)
        {
            return Result::ErrorInvalidValue;
        }

        // Check that all HW counters used by the counter are present.
        for (uint32_t i = 0; i < hw_counter_count; i++)
        {
            const GpaHwCounter& hw_counter = derived_counter_info->gpa_hw_counters[i];
            auto                find_block = raw_db_.Counters().find(hw_counter.gpa_hw_block);
            if (find_block == raw_db_.Counters().end())
            {
                return Result::ErrorNotFound;
            }
            const auto& block_counters = find_block->second;
            auto        find_event     = block_counters.find(hw_counter.gpa_hw_block_event_id);
            if (find_event == block_counters.end())
            {
                return Result::ErrorNotFound;
            }
        }

        gpa_counters_.emplace(gpa_counter_name, GpaCounter{gpa_counter_name, gpa_counter_index, derived_counter_info, display_name, components});
        return Result::Ok;
    }

    Result DerivedFromRawSpmBuilder::Build(std::unique_ptr<DerivedSpmDataBase>& out_derived_db)
    {
        GpaCounterLibFuncTable* gpa_func_table = GPUPerfAPICountersEntryPoints::Instance()->GetFuncTable();
        if (gpa_func_table == nullptr)
        {
            return Result::ErrorUnavailable;
        }

        // check that all required derived counters required by formula counters are present as GPA counters.
        for (const std::pair<const std::string, FormulaCounter>& name_formula_counter_pair : formula_counters_)
        {
            const FormulaCounter&   formula_counter = name_formula_counter_pair.second;
            std::vector<Expression> stack;
            std::stringstream       ss(formula_counter.formula);
            for (std::string token; std::getline(ss, token, ',');)
            {
                DerivedSpmTokenType type = MatchCounterFormulaToken(token);
                if (type == kDerivedSpmTokenCounter)
                {
                    auto block_and_instance = NameToBlockAndEvent(gpa_counter_ctx_, token);
                    if (!block_and_instance.has_value())
                    {
                        if (gpa_counters_.find(token) == gpa_counters_.end())
                        {
                            return Result::ErrorNotFound;
                        }
                    }
                }
            }
        }

        // Check that all components are present.
        for (const std::pair<const std::string, GpaCounter>& name_gpa_counter_pair : gpa_counters_)
        {
            const GpaCounter& gpa_counter = name_gpa_counter_pair.second;
            for (const Component& component : gpa_counter.components)
            {
                if (gpa_counters_.find(component.counter_name) == gpa_counters_.end() &&
                    formula_counters_.find(component.counter_name) == formula_counters_.end())
                {
                    return Result::ErrorNotFound;
                }
            }
        }

        // Build the derived DB.
        // We must use move constructor here since make_unique cannot access the constructor itself.
        std::unique_ptr<DerivedSpmDataBase> result = std::make_unique<DerivedSpmDataBase>(DerivedSpmDataBase());
        result->num_samples_                       = raw_db_.NumSamples();

        // Estimate progress as (number of computed counters / number of counters to compute).
        // Assume other work is negligible.
        float progress_denom = static_cast<float>(formula_counters_.size() + gpa_counters_.size());
        float progress_numer = 0.0f;
        progress_callback_(0.0f);

        // Compute all GPA counters.
        for (const std::pair<const std::string, GpaCounter>& name_gpa_counter_pair : gpa_counters_)
        {
            const GpaCounter& counter = name_gpa_counter_pair.second;
            // On Navi4 hardware, the raw counter data for the L0-L2 Cache derived counters may be noisy,
            // which may cause some derived counter samples to appear bad.
            // For these counters, we will try to detect bad data due to noise and correct it (see CorrectNoisyCacheCounters).
            std::string_view name_prefix_view(counter.gpa_counter_name.c_str(), std::min(counter.gpa_counter_name.length(), strlen("L#Cache")));
            bool apply_cache_hit_counter_heuristic = name_prefix_view == "L0Cache" || name_prefix_view == "L1Cache" || name_prefix_view == "L2Cache";

            std::vector<double> samples;
            bool                contains_bad_values;
            samples.reserve(raw_db_.NumSamples());
            Result compute_result = ComputeGpaCounterSamples(counter, samples, contains_bad_values);

            // If we get an invalid value for a Hit%/HitCount counter we know may be noisy, ignore it for now.
            // We will try to correct for it later along with its corresponding hit/miss/request count counters.
            contains_bad_values &= !apply_cache_hit_counter_heuristic;

            if (compute_result == Result::Ok && (!filter_bad_counters_ || !contains_bad_values))
            {
                const char* counter_desc = "";
                gpa_func_table->GpaCounterLibGetCounterDescription(gpa_counter_ctx_, counter.gpa_counter_index, &counter_desc);
                result->counters_.emplace(counter.gpa_counter_name,
                                          DerivedSpmCounter(counter.gpa_counter_name,
                                                            counter.gpa_counter_info->counter_usage_type,
                                                            counter.display_name,
                                                            std::string(counter_desc),
                                                            std::move(samples)));
            }

            // Update progress.
            progress_numer += 1.0f;
            progress_callback_(progress_numer / progress_denom);
        }
        CorrectNoisyCacheCounters(*result);

        // Compute all formula counters.
        // Maybe make this nicer? We give the eval environment a reference to the in-progress derived SPM DB.
        // All it uses it for is counter sample values, so it will not observe any inconsistent state.
        EvalEnvironment counter_env(gpa_counter_ctx_, raw_db_, *result);

        for (const std::pair<const std::string, FormulaCounter>& name_formula_counter_pair : formula_counters_)
        {
            const FormulaCounter& formula_counter = name_formula_counter_pair.second;
            Expression            eval_result;
            EvalStatus            eval_status = counter_env.EvaluateString(eval_result, formula_counter.formula);
            if (eval_status == kEvalStatusOk)
            {
                result->counters_.emplace(
                    formula_counter.name,
                    DerivedSpmCounter(
                        formula_counter.name, kGpaUsageTypeItems, formula_counter.name, formula_counter.formula, std::move(eval_result.value.front())));
            }

            // Update progress.
            progress_numer += 1.0f;
            progress_callback_(progress_numer / progress_denom);
        }

        // Add components to counters.
        for (const std::pair<const std::string, GpaCounter>& name_gpa_counter_pair : gpa_counters_)
        {
            auto result_it = result->counters_.find(name_gpa_counter_pair.first);
            // Skip counters which failed to compute or contained invalid values.
            if (result_it == result->counters_.end())
            {
                continue;
            }
            const GpaCounter&  counter        = name_gpa_counter_pair.second;
            DerivedSpmCounter& result_counter = result_it->second;
            // Try to find the components. Skip any that are absent.
            for (const Component& component : counter.components)
            {
                auto find_result = result->counters_.find(component.counter_name);
                if (find_result == result->counters_.end())
                {
                    continue;
                }

                result_counter.components_.push_back({component.relation_name, &find_result->second});
            }
        }

        // Add Groups.
        for (auto& group : groups_)
        {
            // Try to find group members. Skip any that are absent.
            std::vector<DerivedSpmCounter*> member_counters;
            for (const std::string& member_name : group.member_names)
            {
                auto find_result = result->counters_.find(member_name);
                if (find_result != result->counters_.end())
                {
                    member_counters.push_back(&find_result->second);
                }
            }
            // If the group is nonempty, add it to the data set.
            if (!member_counters.empty())
            {
                result->groups_.push_back(DerivedSpmGroup(group.name, group.description, member_counters));
            }
        }

        out_derived_db = std::move(result);
        return Result::Ok;
    }

    Result DerivedFromRawSpmBuilder::ComputeGpaCounterSamples(const GpaCounter& counter, std::vector<double>& out_samples, bool& out_contains_bad_values)
    {
        GpaCounterLibFuncTable* gpa_func_table = GPUPerfAPICountersEntryPoints::Instance()->GetFuncTable();
        if (gpa_func_table == nullptr)
        {
            return Result::ErrorUnavailable;
        }

        // Number of timestamps for which this derived counter has a value below 0. Used for correcting noisy data (see below).
        uint32_t bad_value_count  = 0;
        uint32_t good_value_count = 0;

        uint32_t hw_counter_count = counter.gpa_counter_info->gpa_hw_counter_count;

        // Sample values from each raw counter comprising this derived counter (in the order given by GPA) at a single timestamp.
        std::vector<GpaUInt64> raw_counter_values(hw_counter_count, 0);
        for (uint32_t sample_index = 0; sample_index < raw_db_.NumSamples(); sample_index++)
        {
            // Read data for each raw counter at this timestamp.
            bool any_value_non_zero = false;  // True if any constituent raw counter is nonzero at this timestamp. Used for an optimization (see below).
            for (size_t i = 0; i < hw_counter_count; i++)
            {
                const auto& required_hw_counter = counter.gpa_counter_info->gpa_hw_counters[i];
                if (required_hw_counter.is_timing_block)
                {
                    raw_counter_values[i] = sampling_interval_;
                }
                else
                {
                    // We checked earlier that all required counters are present.
                    const auto& counter_instances = raw_db_.Counters().at(required_hw_counter.gpa_hw_block).at(required_hw_counter.gpa_hw_block_event_id);
                    // GPA always defines the block instance count based on the largest card in a hardware family,
                    // so it might expect more instances than actually exist on the hardware that generated this data.
                    // In this case, let the counter value remain 0.
                    if (counter_instances.size() > required_hw_counter.gpa_hw_block_instance)
                    {
                        const RawSpmCounter& raw_counter = counter_instances.at(required_hw_counter.gpa_hw_block_instance);
                        raw_counter_values[i] =
                            std::visit([sample_index](const auto& samples) { return static_cast<uint64_t>(samples.at(sample_index)); }, raw_counter.Samples());
                    }
                    any_value_non_zero |= raw_counter_values[i] != 0;
                }
            }

            // Compute derived counter value from raw counters at this timestamp.
            GpaFloat64 derived_counter_value = 0;
            if (any_value_non_zero)  // As an optimization, skip computing the derived counter value if all raw inputs are zero.
            {
                // Use GPA to compute the derived counter value.
                GpaStatus gpa_status = gpa_func_table->GpaCounterLibComputeDerivedCounterResult(
                    gpa_counter_ctx_, counter.gpa_counter_index, raw_counter_values.data(), hw_counter_count, &derived_counter_value);
                assert(gpa_status == kGpaStatusOk);  // GPA Error: Failure to compute derived counter.
                RETURN_ON_ERROR(gpa_status == kGpaStatusOk, Result::ErrorInvalidValue);

                if (derived_counter_value < 0)
                {
                    bad_value_count++;
                }
                else
                {
                    // If we find the first good value after 1 or 2 bad values, assume the first values were noise:
                    // reset the bad value count and treat the initial negative values as 0.
                    if (bad_value_count <= 2 && bad_value_count == sample_index)
                    {
                        bad_value_count = 0;
                    }
                    good_value_count++;
                }
            }

            out_contains_bad_values = (bad_value_count > 0);
            out_samples.push_back(derived_counter_value);
        }
        return Result::Ok;
    }

    /// @brief Apply a heuristic to check if a bad computed L0-L2 cache hit counter value is due to acceptable noise.
    ///
    /// On Navi4 hardware, the raw counter data for the L0-L2 cache hit counters may be noisy.
    /// This function applies a heuristic to determine whether the noise is within tolerable levels, or truly invalid.
    /// Noise is tolerable if:
    /// - The hit percentage is negative, but less than two percent.
    /// or
    /// - The miss count is greater than the request count by at most 20.
    ///
    /// @param [in] cache_hit_pct  A computed sample from a cache hit percentage counter.
    /// @param [in] cache_miss_cnt A computed sample from a cache miss count counter.
    /// @param [in] cache_req_cnt  A computed sample from a cache request count counter.
    ///
    /// @return true if the value appears to be within acceptable bounds for noise, or false if it appears to be truly bad.
    static bool CacheHitInvalidValueIsNoise(GpaFloat64 cache_hit_pct, GpaFloat64 cache_miss_cnt, GpaFloat64 cache_req_cnt)
    {
        return (cache_hit_pct > -2.0) || ((cache_miss_cnt - cache_req_cnt) < 20.0);
    }

    void DerivedFromRawSpmBuilder::CorrectNoisyCacheCounters(DerivedSpmDataBase& derived_db)
    {
        // Correct noisy cache counter readings or mark them as invalid.
        // We do this separately so we can correct the Hit %, Request Count, Hit Count, and Miss Count counters all together.
        for (uint32_t cache_lv = 0; cache_lv <= 2; cache_lv++)
        {
            bool        counters_bad  = false;
            std::string cache_lv_name = "L" + std::to_string(cache_lv);
            auto        find_hit_pct  = derived_db.counters_.find(cache_lv_name + "CacheHit");
            auto        find_hit_cnt  = derived_db.counters_.find(cache_lv_name + "CacheHitCount");
            auto        find_req_cnt  = derived_db.counters_.find(cache_lv_name + "CacheRequestCount");
            auto        find_miss_cnt = derived_db.counters_.find(cache_lv_name + "CacheMissCount");

            // Only perform the correction if all counters are present.
            // We can relax this requirement in the future if we want to support partial sets of cache counters.
            bool hit_pct_present  = find_hit_pct != derived_db.counters_.end();
            bool hit_cnt_present  = find_hit_cnt != derived_db.counters_.end();
            bool req_cnt_present  = find_req_cnt != derived_db.counters_.end();
            bool miss_cnt_present = find_miss_cnt != derived_db.counters_.end();
            if (!(hit_pct_present && hit_cnt_present && req_cnt_present && miss_cnt_present))
            {
                continue;
            }

            // Get the counter value vectors.
            std::vector<double>& cache_hit_pct_samples  = find_hit_pct->second.samples_;
            std::vector<double>& cache_req_cnt_samples  = find_req_cnt->second.samples_;
            std::vector<double>& cache_hit_cnt_samples  = find_hit_cnt->second.samples_;
            std::vector<double>& cache_miss_cnt_samples = find_miss_cnt->second.samples_;

            for (size_t i = 0; i < derived_db.NumSamples(); i++)
            {
                if (cache_hit_pct_samples[i] < 0.0)
                {
                    if (CacheHitInvalidValueIsNoise(cache_hit_pct_samples[i], cache_miss_cnt_samples[i], cache_req_cnt_samples[i]))
                    {
                        cache_hit_pct_samples[i] = 0.0;
                        cache_hit_cnt_samples[i] = 0.0;
                        cache_req_cnt_samples[i] = cache_miss_cnt_samples[i];
                    }
                    else
                    {
                        counters_bad = true;
                        if (filter_bad_counters_)
                        {
                            break;
                        }
                    }
                }
            }

            if (counters_bad && filter_bad_counters_)
            {
                derived_db.counters_.erase(cache_lv_name + "CacheHit");
                derived_db.counters_.erase(cache_lv_name + "CacheHitCount");
                derived_db.counters_.erase(cache_lv_name + "CacheRequestCount");
                derived_db.counters_.erase(cache_lv_name + "CacheMissCount");
                continue;
            }
        }
    }

}  // namespace spm_db
