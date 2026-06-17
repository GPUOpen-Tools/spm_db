//=============================================================================
// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
//
/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for custom counter calculation.
//=============================================================================

#include "custom_counters.h"

#include <cassert>
#include <cstring>
#include <sstream>

#include "gpa_counters_loader.h"

DerivedSpmTokenType MatchCounterFormulaToken(const std::string& token)
{
    if (token.empty())
    {
        return kDerivedSpmTokenInvalid;
    }
    // Check if operator
    if (token == "sum")
    {
        return kDerivedSpmTokenSum;
    }
    else if (token == "avg")
    {
        return kDerivedSpmTokenAvg;
    }
    else if (token == "max")
    {
        return kDerivedSpmTokenMax;
    }
    else if (token == "min")
    {
        return kDerivedSpmTokenMin;
    }
    else if (token == "+")
    {
        return kDerivedSpmTokenPlus;
    }
    else if (token == "-")
    {
        return kDerivedSpmTokenMinus;
    }
    else if (token == "*")
    {
        return kDerivedSpmTokenMult;
    }
    else if (token == "/")
    {
        return kDerivedSpmTokenDiv;
    }

    // Must be an expression

    // Check if scalar
    char* end;
    strtod(token.c_str(), &end);
    if (strlen(end) == 0)
    {
        return kDerivedSpmTokenScalar;
    }
    else
    {
        // Otherwise, must be a counter
        return kDerivedSpmTokenCounter;
    }
}

std::optional<std::pair<GpaHwBlock, uint32_t>> NameToBlockAndEvent(GpaCounterContext gpa_counter_context, std::string raw_hw_counter_name)
{
    GpaCounterParam counter_param;
    counter_param.is_derived_counter   = true;  // by name, not by block, instance, event
    counter_param.derived_counter_name = raw_hw_counter_name.c_str();

    GpaUInt32 counter_index;
    auto      gpa_func_table = GPUPerfAPICountersEntryPoints::Instance()->GetFuncTable();
    if (gpa_func_table == nullptr)
    {
        return std::nullopt;
    }

    GpaStatus gpa_status = gpa_func_table->GpaCounterLibGetCounterIndex(gpa_counter_context, &counter_param, &counter_index);
    if (gpa_status != kGpaStatusOk)
    {
        // Try adding a 0 before the first or second underscore
        for (size_t upos = raw_hw_counter_name.find('_'); upos != std::string::npos; upos = raw_hw_counter_name.find('_', upos + 1))
        {
            const std::string counter_name_zero = raw_hw_counter_name.substr(0, upos) + "0" + raw_hw_counter_name.substr(upos);

            counter_param.derived_counter_name = counter_name_zero.c_str();

            gpa_status = gpa_func_table->GpaCounterLibGetCounterIndex(gpa_counter_context, &counter_param, &counter_index);
            if (kGpaStatusOk == gpa_status)
            {
                break;
            }
        }
    }

    if (gpa_status == kGpaStatusOk)
    {
        const GpaCounterInfo* counter_info = nullptr;
        gpa_status                         = gpa_func_table->GpaCounterLibGetCounterInfo(gpa_counter_context, counter_index, &counter_info);
        if (gpa_status == kGpaStatusOk && !counter_info->is_derived_counter)
        {
            return std::make_pair(counter_info->gpa_hw_counter->gpa_hw_block, counter_info->gpa_hw_counter->gpa_hw_block_event_id);
        }
    }

    return std::nullopt;
}

/// @brief Evaluates a single counter expression in reverse polish notation (RPN)
///
/// @param [out] result The evaluated expression
/// @param [in] exp_str The string expression to evaluate in reverse polish notation
EvalStatus EvalEnvironment::EvaluateString(Expression& result, const std::string& exp_str)
{
    std::vector<Expression> stack;
    std::stringstream       ss(exp_str);
    for (std::string token; std::getline(ss, token, ',');)
    {
        DerivedSpmTokenType type = MatchCounterFormulaToken(token);
        if (type == kDerivedSpmTokenScalar)
        {
            // Note, treatment of scalar values is not optimized: we just treat them as arrays of the same value.
            Expression exp;
            exp.type  = kExpressionTypeSingleCounter;
            exp.value = {CounterValueList(single_counter_length_, std::stod(token))};

            stack.push_back(exp);
            continue;
        }
        else if (type == kDerivedSpmTokenCounter)
        {
            Expression exp;
            EvalStatus status = GetCounterExpression(exp, token);
            if (kEvalStatusOk != status)
            {
                // Counter not found error
                return status;
            }
            stack.push_back(exp);
            continue;
        }

        // Otherwise perform evaluation
        uint32_t arg_count = kDerivedSpmOperatorArgumentCounts[type];
        if (arg_count > stack.size())
        {
            // Invalid arguments
            return kEvalStatusSyntaxError;
        }
        // Store arguments in array for convenience
        std::vector<Expression> args(arg_count);
        for (int32_t i = arg_count - 1; i >= 0; i--)
        {
            args[i] = stack.back();
            stack.pop_back();
        }

        // Evaluated expression
        Expression exp;
        exp.type  = kExpressionTypeSingleCounter;
        exp.value = std::vector<CounterValueList>(1);

        if (type == kDerivedSpmTokenSum)  // Sum
        {
            if (args[0].type != kExpressionTypeMultiCounter)
            {
                // Invalid arguments: Sum expects multi-counter, got single
                return kEvalStatusSyntaxError;
            }
            exp.value[0] = Sum(args[0].value);
        }
        else if (type == kDerivedSpmTokenAvg)  // Avg
        {
            if (args[0].type != kExpressionTypeMultiCounter)
            {
                // Invalid arguments: Avg expects multi-counter, got single
                return kEvalStatusSyntaxError;
            }
            exp.value[0] = Avg(args[0].value);
        }
        else if (type == kDerivedSpmTokenMax)  // Max
        {
            if (args[0].type != kExpressionTypeMultiCounter)
            {
                // Invalid arguments: Max expects multi-counter, got single
                return kEvalStatusSyntaxError;
            }
            exp.value[0] = Max(args[0].value);
        }
        else if (type == kDerivedSpmTokenMin)  // Min
        {
            if (args[0].type != kExpressionTypeMultiCounter)
            {
                // Invalid arguments: Min expects multi-counter, got single
                return kEvalStatusSyntaxError;
            }
            exp.value[0] = Min(args[0].value);
        }
        else if (type == kDerivedSpmTokenPlus)  // Plus
        {
            if (args[0].type != kExpressionTypeSingleCounter || args[1].type != kExpressionTypeSingleCounter)
            {
                // Invalid arguments: Plus expects single-counter/scalar, got multi
                return kEvalStatusSyntaxError;
            }
            exp.value[0] = Plus(args[0].value[0], args[1].value[0]);
        }
        else if (type == kDerivedSpmTokenMinus)  // Minus
        {
            if (args[0].type != kExpressionTypeSingleCounter || args[1].type != kExpressionTypeSingleCounter)
            {
                // Invalid arguments: Minus expects single-counter/scalar, got multi
                return kEvalStatusSyntaxError;
            }
            exp.value[0] = Minus(args[0].value[0], args[1].value[0]);
        }
        else if (type == kDerivedSpmTokenMult)  // Mult
        {
            if (args[0].type != kExpressionTypeSingleCounter || args[1].type != kExpressionTypeSingleCounter)
            {
                // Invalid arguments: Mult expects single-counter/scalar, got multi
                return kEvalStatusSyntaxError;
            }
            exp.value[0] = Mult(args[0].value[0], args[1].value[0]);
        }
        else if (type == kDerivedSpmTokenDiv)  // Div
        {
            if (args[0].type != kExpressionTypeSingleCounter || args[1].type != kExpressionTypeSingleCounter)
            {
                // Invalid arguments: Div expects single-counter/scalar, got multi
                return kEvalStatusSyntaxError;
            }
            exp.value[0] = Div(args[0].value[0], args[1].value[0]);
        }

        // Push expression back on stack
        stack.push_back(exp);
    }

    if (stack.size() != 1)
    {
        return kEvalStatusSyntaxError;
    }

    result = stack[0];

    return kEvalStatusOk;
}

/// @brief Helper function to retrieve the value of a string expression
///
/// @param [out] exp The output expression value
/// @param [in] exp_str The input string expression
///
/// @retval kEvalStatusOk The operation succeeded
/// @retval kEvalStatusCounterNotFoundError The input string could not be matched with a counter
EvalStatus EvalEnvironment::GetCounterExpression(Expression& exp, const std::string& exp_str) const
{
    auto block_and_event = NameToBlockAndEvent(gpa_counter_context_, exp_str);
    if (block_and_event.has_value())
    {
        auto find_block = raw_spm_db_.Counters().find(block_and_event->first);
        if (find_block == raw_spm_db_.Counters().end())
        {
            return kEvalStatusCounterNotFoundError;
        }
        const auto& block_counters = find_block->second;
        auto        find_counter   = block_counters.find(block_and_event->second);
        if (find_counter == block_counters.end())
        {
            return kEvalStatusCounterNotFoundError;
        }

        const std::vector<spm_db::RawSpmCounter>& counter_instances = find_counter->second;
        exp.value.clear();
        for (const spm_db::RawSpmCounter& counter : counter_instances)
        {
            CounterValueList samples = std::visit(
                [](auto samples) {
                    CounterValueList samples_cvl;
                    samples_cvl.reserve(samples.size());
                    for (auto sample : samples)
                    {
                        samples_cvl.push_back(static_cast<double>(sample));
                    }
                    return samples_cvl;
                },
                counter.Samples());
            exp.value.push_back(std::move(samples));
        }

        exp.type = exp.value.size() == 1 ? kExpressionTypeSingleCounter : kExpressionTypeMultiCounter;
        return kEvalStatusOk;
    }
    else
    {
        auto find_counter = derived_spm_db_.Counters().find(exp_str);
        if (find_counter == derived_spm_db_.Counters().end())
        {
            return kEvalStatusCounterNotFoundError;
        }
        exp.value.clear();
        exp.value.push_back(find_counter->second.Samples());
        exp.type = kExpressionTypeSingleCounter;
        return kEvalStatusOk;
    }
}

/// @brief Element-wise sum a vector of vectors
///
/// @param [in] multi The vector of vectors to sum
///
/// @return The resulting vector of values
CounterValueList EvalEnvironment::Sum(const std::vector<CounterValueList>& multi)
{
    size_t count = multi.size();
    if (count == 0)
    {
        // return empty
        return {};
    }
    size_t len = multi[0].size();

    CounterValueList result(len, 0);
    for (const CounterValueList& single : multi)
    {
        for (uint32_t i = 0; i < len; i++)
        {
            result[i] += single[i];
        }
    }

    return result;
}

/// @brief Element-wise average a vector of vectors
///
/// @param [in] multi The vector of vectors to average
///
/// @return The resulting vector of values
CounterValueList EvalEnvironment::Avg(const std::vector<CounterValueList>& multi)
{
    size_t count = multi.size();
    if (count == 0)
    {
        // return empty
        return {};
    }
    size_t len = multi[0].size();

    CounterValueList result(len, 0);
    for (const CounterValueList& single : multi)
    {
        for (uint32_t i = 0; i < len; i++)
        {
            // Note, we are dividing as we go to avoid overflow
            result[i] += single[i] / count;
        }
    }

    return result;
}

/// @brief Element-wise max a vector of vectors
///
/// @param [in] multi The vector of vectors to max
///
/// @return The resulting vector of values
CounterValueList EvalEnvironment::Max(const std::vector<CounterValueList>& multi)
{
    size_t count = multi.size();
    if (count == 0)
    {
        // return empty
        return {};
    }
    size_t len = multi[0].size();

    CounterValueList result(len, std::numeric_limits<double>::lowest());
    for (const CounterValueList& single : multi)
    {
        for (uint32_t i = 0; i < len; i++)
        {
            result[i] = std::max(result[i], single[i]);
        }
    }

    return result;
}

/// @brief Element-wise min a vector of vectors
///
/// @param [in] multi The vector of vectors to min
///
/// @return The resulting vector of values
CounterValueList EvalEnvironment::Min(const std::vector<CounterValueList>& multi)
{
    size_t count = multi.size();
    if (count == 0)
    {
        // return empty
        return {};
    }
    size_t len = multi[0].size();

    CounterValueList result(len, std::numeric_limits<double>::max());
    for (const CounterValueList& single : multi)
    {
        for (uint32_t i = 0; i < len; i++)
        {
            result[i] = std::min(result[i], single[i]);
        }
    }

    return result;
}

/// @brief Element-wise add two vectors
///
/// @param [in] single1 The lhs vector
/// @param [in] single2 The rhs vector
///
/// @return The resulting vector of values
CounterValueList EvalEnvironment::Plus(const CounterValueList& single1, const CounterValueList& single2)
{
    size_t len = single1.size();

    CounterValueList result(len, 0);
    for (uint32_t i = 0; i < len; i++)
    {
        result[i] = single1[i] + single2[i];
    }

    return result;
}

/// @brief Element-wise add subtract vectors
///
/// @param [in] single1 The lhs vector
/// @param [in] single2 The rhs vector
///
/// @return The resulting vector of values
CounterValueList EvalEnvironment::Minus(const CounterValueList& single1, const CounterValueList& single2)
{
    size_t len = single1.size();

    CounterValueList result(len, 0);
    for (uint32_t i = 0; i < len; i++)
    {
        result[i] = single1[i] - single2[i];
    }

    return result;
}

/// @brief Element-wise multiply two vectors
///
/// @param [in] single1 The lhs vector
/// @param [in] single2 The rhs vector
///
/// @return The resulting vector of values
CounterValueList EvalEnvironment::Mult(const CounterValueList& single1, const CounterValueList& single2)
{
    size_t len = single1.size();

    CounterValueList result(len, 0);
    for (uint32_t i = 0; i < len; i++)
    {
        result[i] = single1[i] * single2[i];
    }

    return result;
}

/// @brief Element-wise add divide vectors
///
///        Divisor of zero will result in value of zero.
///
/// @param [in] single1 The lhs vector
/// @param [in] single2 The rhs vector
///
/// @return The resulting vector of values
CounterValueList EvalEnvironment::Div(const CounterValueList& single1, const CounterValueList& single2)
{
    size_t len = single1.size();

    CounterValueList result(len, 0);
    for (uint32_t i = 0; i < len; i++)
    {
        result[i] = single2[i] == 0 ? 0 : single1[i] / single2[i];
    }

    return result;
}
