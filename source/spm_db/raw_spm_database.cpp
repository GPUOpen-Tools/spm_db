//=============================================================================
// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
//
/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for RawSpmDataBase.
//=============================================================================

#include "raw_spm_database.h"

#include <cassert>

#include "gpa_counters_loader.h"
#include "spm_error_internal.h"

namespace spm_db
{
    RawSpmDataBase::RawSpmDataBase(uint32_t num_samples)
        : number_of_samples_(num_samples)
        , number_of_counters_(0)
        , counter_map_()
    {
    }

    Result RawSpmDataBase::AddCounter(GpaHwBlock hw_block_id, uint32_t hw_block_instance, uint32_t hw_counter_index, std::vector<uint16_t>& counter_samples)
    {
        // Make a temporary copy and call the move version of the method on the copy.
        return AddCounter(hw_block_id, hw_block_instance, hw_counter_index, std::vector(counter_samples));
    }

    Result RawSpmDataBase::AddCounter(GpaHwBlock hw_block_id, uint32_t hw_block_instance, uint32_t hw_counter_index, std::vector<uint32_t>& counter_samples)
    {
        // Make a temporary copy and call the move version of the method on the copy.
        return AddCounter(hw_block_id, hw_block_instance, hw_counter_index, std::vector(counter_samples));
    }

    Result RawSpmDataBase::AddCounter(GpaHwBlock hw_block_id, uint32_t hw_block_instance, uint32_t hw_counter_index, std::vector<uint16_t>&& counter_samples)
    {
        RETURN_ON_ERROR(static_cast<uint32_t>(counter_samples.size()) == number_of_samples_, Result::ErrorInvalidSize);
        // For now, assume we are given counters for each (block, counter index) in order of block instance, and we are given a counter for each instance.
        assert(counter_map_[hw_block_id][hw_counter_index].size() == hw_block_instance);
        counter_map_[hw_block_id][hw_counter_index].emplace_back(static_cast<GpaHwBlock>(hw_block_id), hw_block_instance, hw_counter_index, counter_samples);
        number_of_counters_++;
        return Result::Ok;
    }

    Result RawSpmDataBase::AddCounter(GpaHwBlock hw_block_id, uint32_t hw_block_instance, uint32_t hw_counter_index, std::vector<uint32_t>&& counter_samples)
    {
        RETURN_ON_ERROR(counter_samples.size() == number_of_samples_, Result::ErrorInvalidSize);
        // For now, assume we are given counters for each (block, counter index) in order of block instance, and we are given a counter for each instance.
        assert(counter_map_[hw_block_id][hw_counter_index].size() == hw_block_instance);
        counter_map_[hw_block_id][hw_counter_index].emplace_back(static_cast<GpaHwBlock>(hw_block_id), hw_block_instance, hw_counter_index, counter_samples);
        number_of_counters_++;
        return Result::Ok;
    }

    std::string RawSpmCounter::GetInfo(GpaCounterContext ctx, CounterQueryType query_type) const
    {
        const GpaCounterLibFuncTable* gpa_func_table = GPUPerfAPICountersEntryPoints::Instance()->GetFuncTable();

        if (gpa_func_table != nullptr)
        {
            GpaUInt32       gpa_counter_index            = 0;
            GpaCounterParam counter                      = {};
            counter.is_derived_counter                   = false;
            counter.gpa_hw_counter.gpa_hw_block          = static_cast<GpaHwBlock>(gpu_block_id);
            counter.gpa_hw_counter.gpa_hw_block_instance = gpu_block_instance;
            counter.gpa_hw_counter.gpa_hw_block_event_id = event_index;
            counter.gpa_hw_counter.gpa_shader_mask       = kGpaShaderMaskAll;

            GpaStatus gpa_status = gpa_func_table->GpaCounterLibGetCounterIndex(ctx, &counter, &gpa_counter_index);
            if (gpa_status == kGpaStatusOk)
            {
                const char* gpa_counter_info = nullptr;
                switch (query_type)
                {
                case kCounterQueryTypeName:
                    gpa_status = gpa_func_table->GpaCounterLibGetCounterName(ctx, gpa_counter_index, &gpa_counter_info);
                    break;
                case kCounterQueryTypeDescription:
                    gpa_status = gpa_func_table->GpaCounterLibGetCounterDescription(ctx, gpa_counter_index, &gpa_counter_info);
                    break;
                default:
                    assert(false);  // Invalid query type.
                    return "";
                }
                if (gpa_status == kGpaStatusOk)
                {
                    return std::string(gpa_counter_info);
                }
            }
        }

        // If we get here, then we must have failed to get the requested info via GPA.
        // In this case, provide a string containing the block, instance, and index as a fallback.
        std::string counter_info = std::to_string(gpu_block_id);
        counter_info.append(":");
        counter_info.append(std::to_string(gpu_block_instance));
        counter_info.append(":");
        counter_info.append(std::to_string(event_index));
        return counter_info;
    }
}  // namespace spm_db
