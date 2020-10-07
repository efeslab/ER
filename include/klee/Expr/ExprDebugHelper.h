#ifndef KLEE_EXPR_DEBUGHELPER_H
#define KLEE_EXPR_DEBUGHELPER_H
#include "klee/Expr/Constraints.h"
#include "klee/Expr/Expr.h"

/// Dump constraints to a file
/// Need to know the path constraints, what values are symbolic in that
/// constraints. Support dumping querys want to ask initial values or querys
///     want to evaluate certain expressions.
///
/// \param[in] constraints, a list of expressions must be true
/// \param[in] symbolic_objs, a list of symbolic values referred to by the
///     contraints
/// \param[in] query_exprs, a list of expressions whose value we want to know
///     under the given constraints
/// \param[in] filename, a file path to write
void debugDumpConstraintsImpl(
    const klee::Constraints_ty &constraints,
    const std::vector<const klee::Array *> &symbolic_objs,
    const std::vector<klee::ref<klee::Expr>> &query_exprs, const char *filename);
// an overload function to only dump querys asking intial values
void debugDumpConstraintsImpl(
    const klee::Constraints_ty &constraints,
    const std::vector<const klee::Array *> &symbolic_objs,
    const char *filename);
#endif
