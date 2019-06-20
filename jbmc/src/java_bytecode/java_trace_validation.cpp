/*******************************************************************\

Module: Java trace validation

Author: Jeannie Moulton

\*******************************************************************/

#include "java_trace_validation.h"

#include <algorithm>

#include <goto-programs/goto_trace.h>
#include <util/byte_operators.h>
#include <util/expr.h>
#include <util/expr_util.h>
#include <util/simplify_expr.h>
#include <util/std_expr.h>

bool check_symbol_structure(const exprt &symbol_expr)
{
  const auto symbol = expr_try_dynamic_cast<symbol_exprt>(symbol_expr);
  return symbol && !symbol->get_identifier().empty();
}

/// \return true iff the expression is a symbol or is an expression whose first
/// operand can contain a nested symbol
static bool can_contain_symbol_operand(const exprt &expr)
{
  return can_cast_expr<member_exprt>(expr) ||
         can_cast_expr<index_exprt>(expr) ||
         can_cast_expr<address_of_exprt>(expr) ||
         can_cast_expr<typecast_exprt>(expr) ||
         can_cast_expr<symbol_exprt>(expr) ||
         expr.get(ID_identifier) == ID_byte_extract_little_endian;
}

optionalt<symbol_exprt> get_inner_symbol_expr(exprt expr)
{
  while(expr.has_operands())
  {
    expr = expr.op0();
    if(!can_contain_symbol_operand(expr))
      return {};
  }
  if(!check_symbol_structure(expr))
    return {};
  return *expr_try_dynamic_cast<symbol_exprt>(expr);
}

bool check_member_structure(const exprt &member_expr)
{
  if(!can_cast_expr<member_exprt>(member_expr))
    return false;
  if(!member_expr.has_operands())
    return false;
  const auto symbol_operand = get_inner_symbol_expr(member_expr);
  return symbol_operand.has_value();
}

static void raise_error(const std::string &side, const exprt &expr)
{
  throw std::runtime_error(
    "JBMC Check trace assumption failure on " + side + " expression:\n" +
    expr.pretty());
}

bool valid_lhs_expr_high_level(const exprt &lhs)
{
  return can_cast_expr<member_exprt>(lhs) || can_cast_expr<symbol_exprt>(lhs) ||
         can_cast_expr<index_exprt>(lhs) ||
         lhs.id() == ID_byte_extract_little_endian;
}

bool valid_rhs_expr_high_level(const exprt &rhs)
{
  return can_cast_expr<struct_exprt>(rhs) || can_cast_expr<array_exprt>(rhs) ||
         can_cast_expr<constant_exprt>(rhs) ||
         can_cast_expr<address_of_exprt>(rhs) ||
         can_cast_expr<symbol_exprt>(rhs) ||
         can_cast_expr<array_list_exprt>(rhs) ||
         rhs.id() == ID_byte_extract_little_endian;
}

static void check_lhs_assumptions(const exprt &lhs, const namespacet &ns)
{
  if(!valid_lhs_expr_high_level(lhs))
    raise_error("LHS", lhs);
  // check member lhs structure
  else if(const auto member = expr_try_dynamic_cast<member_exprt>(lhs))
  {
    if(!check_member_structure(*member))
      raise_error("LHS", lhs);
  }
  // check symbol lhs structure
  else if(const auto symbol = expr_try_dynamic_cast<symbol_exprt>(lhs))
  {
    if(!check_symbol_structure(*symbol))
      raise_error("LHS", lhs);
  }
  // check index lhs structure
  else if(const auto index = expr_try_dynamic_cast<index_exprt>(lhs))
  {
    if(index->operands().size() != 2)
      raise_error("LHS", lhs);
    if(!check_symbol_structure(index->op0()))
      raise_error("LHS", lhs);
    if(!expr_try_dynamic_cast<constant_exprt>(index->op1()))
      raise_error("LHS", lhs);
  }
  // check byte extract lhs structure
  else if(lhs.id() == ID_byte_extract_little_endian)
  {
    if(!check_symbol_structure(lhs.op0()))
      raise_error("LHS", lhs);
    if(!expr_try_dynamic_cast<constant_exprt>(simplify_expr(lhs.op1(), ns)))
      raise_error("LHS", lhs);
  }
  else
  {
    raise_error("LHS", lhs);
  }
}

static void check_rhs_assumptions(const exprt &rhs, const namespacet &ns)
{
  if(!valid_rhs_expr_high_level(rhs))
    raise_error("RHS", rhs);
  // check address_of rhs structure (String only)
  else if(const auto address = expr_try_dynamic_cast<address_of_exprt>(rhs))
  {
    const auto symbol_expr = get_inner_symbol_expr(*address);
    if(!symbol_expr)
      raise_error("RHS", rhs);
  }
  // check symbol rhs structure (String only)
  else if(const auto symbol_expr = expr_try_dynamic_cast<symbol_exprt>(rhs))
  {
    if(!check_symbol_structure(*symbol_expr))
      raise_error("RHS", rhs);
  }
  // check struct rhs structure
  else if(const auto struct_expr = expr_try_dynamic_cast<struct_exprt>(rhs))
  {
    if(!struct_expr->has_operands())
      raise_error("RHS", *struct_expr);
    if(
      struct_expr->op0().id() != ID_struct &&
      struct_expr->op0().id() != ID_constant)
    {
      raise_error("RHS", *struct_expr);
    }
    if(
      struct_expr->operands().size() > 1 &&
      std::any_of(
        ++struct_expr->operands().begin(),
        struct_expr->operands().end(),
        [&](const exprt &operand) { return operand.id() != ID_constant; }))
    {
      raise_error("RHS", *struct_expr);
    }
  }
  // check array rhs structure
  else if(can_cast_expr<array_exprt>(rhs))
  {
    // seems no check is required.
  }
  // check array rhs structure
  else if(can_cast_expr<array_list_exprt>(rhs))
  {
    // seems no check is required.
  }
  // check constant rhs structure
  else if(const auto constant_expr = expr_try_dynamic_cast<constant_exprt>(rhs))
  {
    if(constant_expr->has_operands())
    {
      const auto operand = skip_typecast(constant_expr->operands()[0]);
      if(
        operand.id() != ID_constant && operand.id() != ID_address_of &&
        operand.id() != ID_plus)
        raise_error("RHS", *constant_expr);
    }
    else if(constant_expr->get_value().empty())
      raise_error("RHS", *constant_expr);
  }
  // check byte extract rhs structure
  else if(rhs.id() == ID_byte_extract_little_endian)
  {
    if(!can_cast_expr<constant_exprt>(simplify_expr(rhs.op0(), ns)))
      raise_error("LHS", rhs);
    if(!can_cast_expr<constant_exprt>(simplify_expr(rhs.op1(), ns)))
      raise_error("LHS", rhs);
  }
  else
  {
    raise_error("RHS", rhs);
  }
}

static void
check_step_assumptions(const goto_trace_stept &step, const namespacet &ns)
{
  if(!step.is_assignment() && !step.is_decl())
    return;
  check_lhs_assumptions(skip_typecast(step.full_lhs), ns);
  check_rhs_assumptions(skip_typecast(step.full_lhs_value), ns);
}

void check_trace_assumptions(const goto_tracet &trace, const namespacet &ns)
{
  for(const auto &step : trace.steps)
    check_step_assumptions(step, ns);
}
