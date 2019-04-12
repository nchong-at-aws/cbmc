/*******************************************************************\

Module:

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#include "boolbv.h"

#include <util/std_expr.h>
#include <util/std_types.h>

bvt boolbvt::convert_let(const let_exprt &expr)
{
  DATA_INVARIANT(
    expr.value().type() == expr.symbol().type(),
    "let value must have the type of the let symbol");
  DATA_INVARIANT(
    expr.type() == expr.where().type(),
    "let must have the type of the 'where' operand");

  const bvt value_bv = convert_bv(expr.value());

  bool is_boolean = expr.value().type().id() == ID_bool;
  const auto &old_identifier = expr.symbol().get_identifier();

  // produce a new identifier
  const irep_idt new_identifier =
    "boolbvt::scope::" + std::to_string(scope_counter) +
    "::" + id2string(old_identifier);

  scope_counter++;

  // the symbol shall be visible during the recursive call
  // to convert_bv for 'where'
  if(is_boolean)
  {
    DATA_INVARIANT(value_bv.size() == 1, "boolean values have one bit");
    symbols[new_identifier] = value_bv[0];
  }
  else
    map.set_literals(new_identifier, expr.symbol().type(), value_bv);

  // rename bound symbol in 'where'
  exprt where_renamed = expr.where();

  where_renamed.visit_post([old_identifier, new_identifier](exprt &expr) {
    if(expr.id() == ID_symbol)
    {
      auto &symbol_expr = to_symbol_expr(expr);
      if(symbol_expr.get_identifier() == old_identifier)
        symbol_expr.set_identifier(new_identifier);
    }
  });

  // recursive call
  bvt result_bv = convert_bv(where_renamed);

  // the mapping can now be deleted
  if(is_boolean)
    symbols.erase(new_identifier);
  else
    map.erase_literals(new_identifier, expr.symbol().type());

  return result_bv;
}
