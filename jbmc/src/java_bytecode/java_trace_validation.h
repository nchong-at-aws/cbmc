/*******************************************************************\

Module: Java trace validation

Author: Jeannie Moulton

\*******************************************************************/

#ifndef CPROVER_JAVA_BYTECODE_JAVA_TRACE_VALIDATION_H
#define CPROVER_JAVA_BYTECODE_JAVA_TRACE_VALIDATION_H

#include <util/optional.h>

class goto_tracet;
class namespacet;
class exprt;
class symbol_exprt;

/// Checks that the structure of each step of the trace matches certain
/// criteria. If it does not, throw an error. Intended to be called by
/// the caller of \ref build_goto_trace, for example
/// \ref java_multi_path_symex_checkert::build_full_trace()
void check_trace_assumptions(const goto_tracet &trace, const namespacet &ns);

/// \return true iff the expression is a symbol expression and has a non-empty
/// identifier
bool check_symbol_structure(const exprt &symbol_expr);
/// Recursively extracts the first operand of an expression until it reaches a
/// symbol and returns it, or returns an empty optional
optionalt<symbol_exprt> get_inner_symbol_expr(exprt expr);
/// \return true iff the expression is a member expression (or nested member
/// expression) of a valid symbol
bool check_member_structure(const exprt &member_expr);
/// \return true iff the left hand side is superficially an expected expression
/// type
bool valid_lhs_expr_high_level(const exprt &lhs);
/// \return true iff the right hand side is superficially an expected expression
/// type
bool valid_rhs_expr_high_level(const exprt &rhs);

#endif // CPROVER_JAVA_BYTECODE_JAVA_TRACE_VALIDATION_H
