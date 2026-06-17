//=============================================================================
// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
//
/// @author AMD Developer Tools Team
/// @file
/// @brief Definition of raw SPM database and associated structures.
/// Raw SPM counters come straight from the hardware.
/// They are mostly useful for computing derived counters and not usually
/// useful on their own.
/// A Raw SPM DB is mutable: it is created empty and may be appended to.
/// Raw SPM counters are organized by the GPU hardware block, counter index,
/// and block instance which define the counter on the GPU.
/// Raw SPM counters sample values may be 16 or 32 bits.
//=============================================================================

#ifndef SPMDB_SPM_DB_RAW_SPM_DATABASE_H_
#define SPMDB_SPM_DB_RAW_SPM_DATABASE_H_

#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "spm_error.h"

#include "gpu_performance_api/gpu_perf_api_counters.h"

namespace spm_db
{
    /// @brief A single Streaming Performance Monitor (SPM) counter.
    /// Represents a single hardware counter collected from a hardware block instance.
    class RawSpmCounter
    {
    public:
        /// @brief Create a raw SPM counter from 16-bit samples by copying the samples.
        ///
        /// @param [in]     block             The hardware block that this counter came from.
        /// @param [in]     hw_block_instance The instance of the hardware block that this counter came from.
        /// @param [in]     event_index       The counter index within the block.
        /// @param [in,out] samples           The list of samples. The samples are copied from this list.
        RawSpmCounter(GpaHwBlock block, uint32_t hw_block_instance, uint32_t event_index, std::vector<uint16_t>& samples)
            : gpu_block_id(block)
            , gpu_block_instance(hw_block_instance)
            , event_index(event_index)
            , data_size(sizeof(uint16_t))
            , samples(std::vector(samples))
        {
        }

        /// @brief Create a raw SPM counter from 32-bit samples by copying the samples.
        ///
        /// @param [in]     block             The hardware block that this counter came from.
        /// @param [in]     hw_block_instance The instance of the hardware block that this counter came from.
        /// @param [in]     event_index       The counter index within the block.
        /// @param [in,out] samples           The list of samples. The samples are copied from this list.
        RawSpmCounter(GpaHwBlock block, uint32_t hw_block_instance, uint32_t event_index, std::vector<uint32_t>& samples)
            : gpu_block_id(block)
            , gpu_block_instance(hw_block_instance)
            , event_index(event_index)
            , data_size(sizeof(uint32_t))
            , samples(std::vector(samples))
        {
        }

        /// @brief Create a raw SPM counter from 16-bit samples by moving the samples.
        ///
        /// @param [in]     block             The hardware block that this counter came from.
        /// @param [in]     hw_block_instance The instance of the hardware block that this counter came from.
        /// @param [in]     event_index       The counter index within the block.
        /// @param [in,out] samples           The list of samples. The samples are moved out of this list.
        RawSpmCounter(GpaHwBlock block, uint32_t hw_block_instance, uint32_t event_index, std::vector<uint16_t>&& samples)
            : gpu_block_id(block)
            , gpu_block_instance(hw_block_instance)
            , event_index(event_index)
            , data_size(sizeof(uint16_t))
            , samples(std::move(samples))
        {
        }

        /// @brief Create a raw SPM counter from 32-bit samples by moving the samples.
        ///
        /// @param [in]     block             The hardware block that this counter came from.
        /// @param [in]     hw_block_instance The instance of the hardware block that this counter came from.
        /// @param [in]     event_index       The counter index within the block.
        /// @param [in,out] samples           The list of samples. The samples are moved out of this list.
        RawSpmCounter(GpaHwBlock block, uint32_t hw_block_instance, uint32_t event_index, std::vector<uint32_t>&& samples)
            : gpu_block_id(block)
            , gpu_block_instance(hw_block_instance)
            , event_index(event_index)
            , data_size(sizeof(uint32_t))
            , samples(std::move(samples))
        {
        }

        /// @brief Get the block that produced this counter.
        /// @return The block that this counter came from.
        GpaHwBlock Block() const
        {
            return gpu_block_id;
        }

        /// @brief Get the instance of the block that produced this counter.
        /// @return The instance of the block that this counter came from.
        uint32_t BlockInstance() const
        {
            return gpu_block_instance;
        }

        /// @brief Get this counter's index within its block.
        /// @return The index of this counter within its hardware block.
        uint32_t EventIndex() const
        {
            return event_index;
        }

        /// @brief Get this counter's sample width in bytes.
        /// @return The size of one sample of this counter, in bytes.
        uint32_t SampleWidth() const
        {
            return data_size;
        }

        /// @brief Get the name of this counter by querying GPA.
        /// @param [in] ctx A GPA counter context valid for the hardware that was used to capture this counter.
        /// @return The name of this counter.
        std::string Name(GpaCounterContext ctx) const
        {
            return GetInfo(ctx, kCounterQueryTypeName);
        }

        /// @brief Get the description of this counter by querying GPA.
        /// @param [in] ctx A GPA counter context valid for the hardware that was used to capture this counter.
        /// @return A description of this counter.
        std::string Description(GpaCounterContext ctx) const
        {
            return GetInfo(ctx, kCounterQueryTypeDescription);
        }

        /// @brief Get this counter's list of samples.
        /// @return The samples for this counter.
        const std::variant<std::vector<uint16_t>, std::vector<uint32_t>>& Samples() const
        {
            return samples;
        }

    private:
        GpaHwBlock gpu_block_id;        ///< GPU block identifier.
        uint32_t   gpu_block_instance;  ///< GPU block instance.
        uint32_t   event_index;         ///< Index of the perf counter event.
        uint32_t   data_size;           ///< Size in bytes of a single counter data item.
        /// Counter sample values.
        /// Hardware records either 16 or 32 bit wide counter samples.
        /// Since there can be large amounts of sample data (often 10s of megabytes per profile)
        /// and most counters that are usually captured have 16 bit samples, we do not expand all counters to 32 bits.
        std::variant<std::vector<uint16_t>, std::vector<uint32_t>> samples;

        /// @brief An enum used to specify which parts of a counter info need to be queried.
        /// Helper struct for GetInfo.
        enum CounterQueryType
        {
            kCounterQueryTypeName        = 0,  ///< Query the Counter name.
            kCounterQueryTypeDescription = 1,  ///< Query the counter description.
        };

        /// @brief Helper function to get the name or description of this raw counter.
        ///
        /// If the name or description cannot be determined or an invalid GPA context is provided,
        /// then a placeholder of the form "(block):(block instance):(counter index)" is given instead.
        ///
        /// @param [in] ctx        A GPA counter context valid for the hardware that was used to capture this counter.
        /// @param [in] query_type The piece of information needed for the specified counter.
        ///
        /// @return The name or description of this counter, or a placeholder.
        std::string GetInfo(GpaCounterContext ctx, CounterQueryType query_type) const;
    };

    /// @brief Provides access to raw SPM counter data.
    /// Raw counter data is organized by its source GPU hardware block, the counter index within that block,
    /// and the numeric index of the block within the source hardware.
    class RawSpmDataBase
    {
    public:
        /// @brief Construct an empty RawSpmDataBase which will expect the given number of samples per counter.
        ///
        /// @param num_samples The number of samples each counter will contain.
        RawSpmDataBase(uint32_t num_samples);

        // TODO TransferCounterData methods currently expect to be given counter data for every block instance
        //      and for instances to be received in order.
        // Not great but we do expect to see them all in order in input chunks currently. How can we handle this better?
        // - We could use another builder, which could sort them and ensure that all indices are present in its build method.
        // - We could take a vector containing all instances at once and require it to be dense and in order.
        // - We could NOT take a block instance and assume density/order

        /// @brief Add a set of values for the given counter by copying a vector of 16-bit sample values.
        ///
        /// @param [in]     hw_block_id       The hardware block that the counter comes from.
        /// @param [in]     hw_block_instance The instance of the hardware block that this counter comes from.
        /// @param [in]     event_index  The index of the counter within its hardware block.
        /// @param [in,out] counter_samples   The counter sample values to add. Samples are copied from this vector.
        ///
        /// @retval
        /// kRgpOk The counter data was added successfully.
        /// @retval
        /// kRgpErrorInvalidSize The number of samples in the given vector of samples
        ///                      does not match the other counters in this raw SPM DB.
        Result AddCounter(GpaHwBlock hw_block_id, uint32_t hw_block_instance, uint32_t event_index, std::vector<uint16_t>& counter_samples);

        /// @brief Add a set of values for the given counter by copying a vector of 32-bit sample values.
        ///
        /// @param [in]     hw_block_id       The hardware block that the counter comes from.
        /// @param [in]     hw_block_instance The instance of the hardware block that this counter comes from.
        /// @param [in]     event_index  The index of the counter within its hardware block.
        /// @param [in,out] counter_samples   The counter sample values to add. Samples are copied from this vector.
        ///
        /// @retval
        /// kRgpOk The counter data was added successfully.
        /// @retval
        /// kRgpErrorInvalidSize The number of samples in the given vector of samples
        ///                      does not match the other counters in this raw SPM DB.
        Result AddCounter(GpaHwBlock hw_block_id, uint32_t hw_block_instance, uint32_t event_index, std::vector<uint32_t>& counter_samples);

        /// @brief Add a set of values for the given counter by moving a vector of 16-bit sample values.
        ///
        /// @param [in]     hw_block_id       The hardware block that the counter comes from.
        /// @param [in]     hw_block_instance The instance of the hardware block that this counter comes from.
        /// @param [in]     event_index  The index of the counter within its hardware block.
        /// @param [in,out] counter_samples   The counter sample values to add. Samples are moved from this vector.
        ///
        /// @retval
        /// kRgpOk The counter data was added successfully.
        /// @retval
        /// kRgpErrorInvalidSize The number of samples in the given vector of samples
        ///                      does not match the other counters in this raw SPM DB.
        Result AddCounter(GpaHwBlock hw_block_id, uint32_t hw_block_instance, uint32_t event_index, std::vector<uint16_t>&& counter_samples);

        /// @brief Add a set of values for the given counter by moving a vector of 32-bit sample values.
        ///
        /// @param [in]     hw_block_id       The hardware block that the counter comes from.
        /// @param [in]     hw_block_instance The instance of the hardware block that this counter comes from.
        /// @param [in]     event_index  The index of the counter within its hardware block.
        /// @param [in,out] counter_samples   The counter sample values to add. Samples are moved from this vector.
        ///
        /// @retval
        /// kRgpOk The counter data was added successfully.
        /// @retval
        /// kRgpErrorInvalidSize The number of samples in the given vector of samples
        ///                      does not match the other counters in this raw SPM DB.
        Result AddCounter(GpaHwBlock hw_block_id, uint32_t hw_block_instance, uint32_t event_index, std::vector<uint32_t>&& counter_samples);

        /// @brief Gets the number of samples per counter in the SPM database.
        ///
        /// @retval The number of samples in the the SPM database.
        uint32_t NumSamples() const
        {
            return number_of_samples_;
        }

        /// @brief Gets the number of raw counters in the SPM database.
        ///
        /// @retval The number of counters in the the SPM database.
        uint32_t NumCounters() const
        {
            return number_of_counters_;
        }

        /// @brief Get the stored counters in a map of the form block -> event index -> block instance -> counter.
        /// @return The map containing the stored counters.
        const std::unordered_map<GpaHwBlock, std::unordered_map<uint32_t, std::vector<RawSpmCounter>>>& Counters() const
        {
            return counter_map_;
        }

    private:
        uint32_t number_of_samples_;   ///< Number of sample points per counter in the profile data.
        uint32_t number_of_counters_;  ///< Number of SPM counters in this database.

        // The following works only if we can assume we always have a contiguous list of block indices for each counter.
        // This should always be true, at least for any profiles captured via RDP.
        // However, specific instance IDs _can_ be requested from the driver.
        // Can the hardware record data from some block instances but not others via another capture method?
        // (Or are there situations where the driver will discard data from some block instances?)

        /// @brief A map containing the counters in this Raw SPM DB,
        /// in the form HW Block -> Counter Index -> Block Instance -> Counter
        std::unordered_map<GpaHwBlock, std::unordered_map<uint32_t, std::vector<RawSpmCounter>>> counter_map_;
    };
}  // namespace spm_db

#endif  // !SPMDB_SPM_DB_RAW_SPM_DATABASE_H_
