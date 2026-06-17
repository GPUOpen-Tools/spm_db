//=============================================================================
// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
//
/// @author AMD Developer Tools Team
/// @file
/// @brief Definitions for custom formula calculation.
/// Defines a class for computing custom formulas
/// and other associated types.
//=============================================================================

#ifndef SPMDB_SPM_DB_CUSTOM_COUNTERS_H_
#define SPMDB_SPM_DB_CUSTOM_COUNTERS_H_

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "gpu_performance_api/gpu_perf_api_counters.h"

#include "derived_spm_database.h"
#include "raw_spm_database.h"

/// @brief The type of a token in a formula expression.
enum DerivedSpmTokenType
{
    kDerivedSpmTokenSum   = 0,  ///< Operator: sum together multiple instances of a hardware counter
    kDerivedSpmTokenAvg   = 1,  ///< Operator: average multiple instances of a hardware counter
    kDerivedSpmTokenMax   = 2,  ///< Operator: take the maximum of multiple instances of a hardware counter
    kDerivedSpmTokenMin   = 3,  ///< Operator: take the minimum of multiple instances of a hardware counter
    kDerivedSpmTokenPlus  = 4,  ///< Operator: add two counters together
    kDerivedSpmTokenMinus = 5,  ///< Operator: subtract a counter from another counter
    kDerivedSpmTokenMult  = 6,  ///< Operator: multiply two counters together
    kDerivedSpmTokenDiv   = 7,  ///< Operator: divide a counter by another counter

    kDerivedSpmTokenCounter = 8,  ///< Expression: a system SPM counter name, can be either gpa or hw
    kDerivedSpmTokenScalar  = 9,  ///< Expression: a number

    kDerivedSpmTokenInvalid = 10,  ///< Invalid token
};

/// @brief Helper function to match a token string to derived SPM token type
///
/// @param [in] token The token to match as a string
///
/// @return the token type
DerivedSpmTokenType MatchCounterFormulaToken(const std::string& token);

// Lookup array mapping DerivedSpmTokenType to its associated number of arguments
static const uint32_t kDerivedSpmOperatorArgumentCounts[] = {
    1,  // kDerivedSpmTokenSum
    1,  // kDerivedSpmTokenAvg
    1,  // kDerivedSpmTokenMax
    1,  // kDerivedSpmTokenMin
    2,  // kDerivedSpmTokenPlus
    2,  // kDerivedSpmTokenMinus
    2,  // kDerivedSpmTokenMult
    2,  // kDerivedSpmTokenDiv
};

typedef std::vector<double> CounterValueList;

enum ExpressionType
{
    kExpressionTypeSingleCounter,  ///< Corresponds to a single counter
    kExpressionTypeMultiCounter,   ///< Corresponds to a collection of hw counters
};

/// @brief A structure encapsulating a SPM formula expression value
struct Expression
{
    std::vector<CounterValueList> value;  ///< The expression value, only the first element if of type single counter
    ExpressionType                type;   ///< The expression type
};

/// @brief The result of an EvalEnvironment operation.
enum EvalStatus
{
    kEvalStatusOk                   = 0,  ///< No problems encountered
    kEvalStatusCounterNotFoundError = 1,  ///< Counter not found error encountered
    kEvalStatusSyntaxError          = 2,  ///< Syntax error encountered
};

/// @brief A class encapsulating the SPM formula evaluation environment
class EvalEnvironment
{
public:
    /// @brief Create a formula evaluation environment to compute formulas based on the counters in the given DBs.
    ///
    /// The constructed environment must not outlive the input SPM DBs.
    ///
    /// @param [in] gpa_counter_context A GPA counter context valid for the hardware that captured raw_spm_db.
    /// @param [in] raw_spm_db A raw SPM DB that contains raw counters used in counter formulas.
    /// @param [in] derived_spm_db A derived SPM DB that contains derived counters used in counter formulas.
    EvalEnvironment(GpaCounterContext gpa_counter_context, const spm_db::RawSpmDataBase& raw_spm_db, const spm_db::DerivedSpmDataBase& derived_spm_db)
        : gpa_counter_context_(gpa_counter_context)
        , raw_spm_db_(raw_spm_db)
        , derived_spm_db_(derived_spm_db)
        , single_counter_length_(raw_spm_db.NumSamples())
    {
    }

    /// @brief Evaluates a single counter expression in reverse polish notation (RPN)
    ///
    /// @param [out] result The evaluated expression
    /// @param [in] exp_str The string expression to evaluate in reverse polish notation
    EvalStatus EvaluateString(Expression& result, const std::string& exp_str);

private:
    GpaCounterContext                 gpa_counter_context_;    ///< A GPA counter context valid for the hardware that captured the data in raw_spm_db_.
    const spm_db::RawSpmDataBase&     raw_spm_db_;             ///< Raw counter data to use in computing formula counters.
    const spm_db::DerivedSpmDataBase& derived_spm_db_;         ///< Derived counter data to use in computing formula counters.
    size_t                            single_counter_length_;  ///< The length of counter value lists.

    /// @brief Helper function to retrieve the value of a string expression
    ///
    /// @param [out] exp The output expression value
    /// @param [in] exp_str The input string expression
    ///
    /// @retval kEvalStatusOk The operation succeeded
    /// @retval kEvalStatusCounterNotFoundError The input string could not be matched with a counter
    EvalStatus GetCounterExpression(Expression& exp, const std::string& exp_str) const;

    /// @brief Element-wise sum a vector of vectors
    ///
    /// @param [in] multi The vector of vectors to sum
    ///
    /// @return The resulting vector of values
    static CounterValueList Sum(const std::vector<CounterValueList>& multi);

    /// @brief Element-wise average a vector of vectors
    ///
    /// @param [in] multi The vector of vectors to average
    ///
    /// @return The resulting vector of values
    static CounterValueList Avg(const std::vector<CounterValueList>& multi);

    /// @brief Element-wise max a vector of vectors
    ///
    /// @param [in] multi The vector of vectors to max
    ///
    /// @return The resulting vector of values
    static CounterValueList Max(const std::vector<CounterValueList>& multi);

    /// @brief Element-wise min a vector of vectors
    ///
    /// @param [in] multi The vector of vectors to min
    ///
    /// @return The resulting vector of values
    static CounterValueList Min(const std::vector<CounterValueList>& multi);

    /// @brief Element-wise add two vectors
    ///
    /// @param [in] single1 The lhs vector
    /// @param [in] single2 The rhs vector
    ///
    /// @return The resulting vector of values
    static CounterValueList Plus(const CounterValueList& single1, const CounterValueList& single2);

    /// @brief Element-wise add subtract vectors
    ///
    /// @param [in] single1 The lhs vector
    /// @param [in] single2 The rhs vector
    ///
    /// @return The resulting vector of values
    static CounterValueList Minus(const CounterValueList& single1, const CounterValueList& single2);

    /// @brief Element-wise multiply two vectors
    ///
    /// @param [in] single1 The lhs vector
    /// @param [in] single2 The rhs vector
    ///
    /// @return The resulting vector of values
    static CounterValueList Mult(const CounterValueList& single1, const CounterValueList& single2);

    /// @brief Element-wise add divide vectors
    ///
    ///        Divisor of zero will result in value of zero.
    ///
    /// @param [in] single1 The lhs vector
    /// @param [in] single2 The rhs vector
    ///
    /// @return The resulting vector of values
    static CounterValueList Div(const CounterValueList& single1, const CounterValueList& single2);
};

/// @brief Get the GPA hardware block and event index denoted by a custom formula raw counter name.
///
/// Note that the index number is hardware-specific, so this operation requires a counter context valid for the hardware to query.
///
/// A custom formula raw counter name refers to all instances of a counter,
/// and is identical to the GPA name of an instance of the raw counter with the block instance number omitted.
/// For example, TCP_PERF_SEL_REQ.
///
/// @param [in] gpa_counter_context A GPA counter context valid for the hardware to query.
/// @param [in] raw_hw_counter_name The custom formula raw counter name to convert.
///
/// @retval
/// std::nullopt if the name doesn't refer to a raw counter family in the given counter context.
/// @retval
/// An optional containing the hardware block and event index denoted by the given name on success.
std::optional<std::pair<GpaHwBlock, uint32_t>> NameToBlockAndEvent(GpaCounterContext gpa_counter_context, std::string raw_hw_counter_name);

#endif  // !SPMDB_SPM_DB_CUSTOM_COUNTERS_H_
