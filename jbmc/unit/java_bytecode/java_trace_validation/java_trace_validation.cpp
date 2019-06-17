/*******************************************************************\

Module: Unit tests for java_trace_validation

Author: Diffblue Ltd.

\*******************************************************************/

#include <goto-programs/goto_trace.h>
#include <java_bytecode/java_trace_validation.h>
#include <java_bytecode/java_types.h>
#include <testing-utils/use_catch.h>
#include <util/byte_operators.h>

TEST_CASE("java trace validation", "[core][java_trace_validation]")
{
  const exprt plain_expr = exprt();
  const exprt expr_with_data = exprt("id", java_int_type());
  const symbol_exprt valid_symbol_expr = symbol_exprt("id", java_int_type());
  const symbol_exprt invalid_symbol_expr = symbol_exprt(java_int_type());
  const member_exprt valid_member =
    member_exprt(valid_symbol_expr, "member", java_int_type());
  const member_exprt invalid_member =
    member_exprt(plain_expr, "member", java_int_type());
  const constant_exprt valid_constant = constant_exprt("0", java_int_type());
  const index_exprt index_plain = index_exprt(exprt(), exprt());
  const byte_extract_exprt byte_little_endian =
    byte_extract_exprt(ID_byte_extract_little_endian);
  const address_of_exprt valid_address =
    address_of_exprt(constant_exprt("0", java_int_type()));
  const struct_exprt struct_plain =
    struct_exprt(std::vector<exprt>(), java_int_type());
  const array_exprt array_plain =
    array_exprt(std::vector<exprt>(), array_typet(java_int_type(), exprt()));
  const array_list_exprt array_list_plain = array_list_exprt(
    std::vector<exprt>(), array_typet(java_int_type(), exprt()));

  SECTION("check_symbol_structure")
  {
    INFO("valid symbol expression")
    REQUIRE(check_symbol_structure(valid_symbol_expr));
    INFO("invalid symbol expression, missing identifier")
    REQUIRE_FALSE(check_symbol_structure(invalid_symbol_expr));
    INFO("invalid symbol expression, not a symbol")
    REQUIRE_FALSE(check_symbol_structure(plain_expr));
  }

  SECTION("get_inner_symbol_expr")
  {
    const exprt inner_symbol = exprt(exprt(valid_symbol_expr));
    const exprt inner_nonsymbol = exprt(exprt(exprt()));
    INFO("expression does not have an inner symbol")
    REQUIRE(get_inner_symbol_expr(inner_symbol).has_value());
    INFO("expression does not have an inner symbol")
    REQUIRE_FALSE(get_inner_symbol_expr(inner_nonsymbol).has_value());
  }

  SECTION("check_member_structure")
  {
    INFO("valid member structure")
    REQUIRE(check_member_structure(valid_member));
    INFO("not a member")
    REQUIRE_FALSE(check_member_structure(plain_expr));
    INFO("invalid member structure, no symbol operand")
    REQUIRE_FALSE(check_member_structure(invalid_member));
  }

  SECTION("valid_lhs_expr_high_level")
  {
    INFO("member_exprts are valid lhs expressions")
    REQUIRE(valid_lhs_expr_high_level(valid_member));
    INFO("symbol_exprts are valid lhs expressions")
    REQUIRE(valid_lhs_expr_high_level(valid_symbol_expr));
    INFO("index_exprts are valid lhs expressions")
    REQUIRE(valid_lhs_expr_high_level(index_plain));
    INFO("byte_extract_exprts little endian are valid lhs expressions")
    REQUIRE(valid_lhs_expr_high_level(byte_little_endian));
    INFO("address_of_exprts are not valid lhs expressions, for example")
    REQUIRE_FALSE(valid_lhs_expr_high_level(valid_address));
  }

  SECTION("valid_rhs_expr_high_level")
  {
    INFO("symbol_exprts are valid rhs expressions")
    REQUIRE(valid_rhs_expr_high_level(valid_symbol_expr));
    INFO("address_of_exprts are valid lhs expressions")
    REQUIRE(valid_rhs_expr_high_level(valid_address));
    INFO("struct_exprts are valid lhs expressions")
    REQUIRE(valid_rhs_expr_high_level(struct_plain));
    INFO("array_exprts are valid lhs expressions")
    REQUIRE(valid_rhs_expr_high_level(array_plain));
    INFO("array_list_exprts are valid lhs expressions")
    REQUIRE(valid_rhs_expr_high_level(array_list_plain));
    INFO("constant_exprts are valid lhs expressions")
    REQUIRE(valid_rhs_expr_high_level(valid_constant));
    INFO("member_exprts are not valid lhs expressions, for example")
    REQUIRE_FALSE(valid_rhs_expr_high_level(valid_member));
    INFO("index_exprts are not are valid lhs expressions, for example")
    REQUIRE_FALSE(valid_rhs_expr_high_level(index_plain));
    INFO("byte_extract_exprts are valid lhs expressions, for example")
    REQUIRE_FALSE(valid_rhs_expr_high_level(byte_little_endian));
  }

  SECTION("check_trace_assumptions pass with a valid step")
  {
    goto_tracet trace;
    goto_trace_stept step;
    step.type = goto_trace_stept::typet::ASSIGNMENT;
    step.full_lhs = valid_symbol_expr;
    step.full_lhs_value = valid_constant;
    trace.add_step(step);
    REQUIRE_NOTHROW(
      check_trace_assumptions(trace, namespacet(symbol_tablet())));
  }

  SECTION("check_trace_assumptions fail with an invalid step")
  {
    goto_tracet trace;
    goto_trace_stept step;
    step.type = goto_trace_stept::typet::ASSIGNMENT;
    step.full_lhs = invalid_symbol_expr;
    step.full_lhs_value = valid_member;
    trace.add_step(step);
    REQUIRE_THROWS_AS(
      check_trace_assumptions(trace, namespacet(symbol_tablet())),
      std::runtime_error);
  }
}
