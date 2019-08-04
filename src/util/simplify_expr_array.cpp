/*******************************************************************\

Module:

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#include "simplify_expr_class.h"

#include "arith_tools.h"
#include "namespace.h"
#include "pointer_offset_size.h"
#include "replace_expr.h"
#include "std_expr.h"
#include "string_constant.h"

simplify_exprt::resultt<>
simplify_exprt::simplify_index(const index_exprt &expr)
{
  bool no_change = true;

  // copy
  auto new_expr = expr;

  // references
  auto &index = new_expr.index();
  auto &array = new_expr.array();

  // extra arithmetic optimizations

  if(index.id()==ID_div &&
     index.operands().size()==2)
  {
    if(index.op0().id()==ID_mult &&
       index.op0().operands().size()==2 &&
       index.op0().op1()==index.op1())
    {
      index = index.op0().op0();
      no_change = false;
    }
    else if(index.op0().id()==ID_mult &&
            index.op0().operands().size()==2 &&
            index.op0().op0()==index.op1())
    {
      index = index.op0().op1();
      no_change = false;
    }
  }

  if(array.id() == ID_lambda)
  {
    // simplify (lambda i: e)(x) to e[i/x]

    const auto &comprehension = to_array_comprehension_expr(array);

    if(index.type() == comprehension.arg().type())
    {
      exprt tmp = comprehension.body();
      replace_expr(comprehension.arg(), index, tmp);
      return changed(simplify_rec(tmp));
    }
  }
  else if(array.id()==ID_with)
  {
    // we have (a WITH [i:=e])[j]

    if(array.operands().size() != 3)
      return unchanged(expr);

    const auto &with_expr = to_with_expr(array);

    if(with_expr.where() == index)
    {
      // simplify (e with [i:=v])[i] to v
      return with_expr.new_value();
    }
    else
    {
      // Turn (a with i:=x)[j] into (i==j)?x:a[j].
      // watch out that the type of i and j might be different.
      const exprt rhs_casted =
        typecast_exprt::conditional_cast(with_expr.where(), index.type());

      exprt equality_expr = simplify_inequality(equal_exprt(index, rhs_casted));

      exprt new_index_expr = simplify_index(
        index_exprt(with_expr.old(), index, new_expr.type())); // recursive call

      if(equality_expr.is_true())
      {
        return with_expr.new_value();
      }
      else if(equality_expr.is_false())
      {
        return new_index_expr;
      }

      if_exprt if_expr(equality_expr, with_expr.new_value(), new_index_expr);
      return changed(simplify_if(if_expr));
    }
  }
  else if(
    array.id() == ID_constant || array.id() == ID_array ||
    array.id() == ID_vector)
  {
    const auto i = numeric_cast<mp_integer>(index);

    if(!i.has_value())
    {
    }
    else if(*i < 0 || *i >= array.operands().size())
    {
      // out of bounds
    }
    else
    {
      // ok
      return array.operands()[numeric_cast_v<std::size_t>(*i)];
    }
  }
  else if(array.id()==ID_string_constant)
  {
    const auto i = numeric_cast<mp_integer>(index);

    const std::string &value = id2string(to_string_constant(array).get_value());

    if(!i.has_value())
    {
    }
    else if(*i < 0 || *i > value.size())
    {
      // out of bounds
    }
    else
    {
      // terminating zero?
      const char v =
        (*i == value.size()) ? 0 : value[numeric_cast_v<std::size_t>(*i)];
      return from_integer(v, new_expr.type());
    }
  }
  else if(array.id()==ID_array_of)
  {
    if(array.operands().size()==1)
    {
      return array.op0();
    }
  }
  else if(array.id() == ID_array_list)
  {
    // These are index/value pairs, alternating.
    for(size_t i=0; i<array.operands().size()/2; i++)
    {
      exprt tmp_index = typecast_exprt(array.operands()[i * 2], index.type());
      simplify(tmp_index);
      if(tmp_index==index)
      {
        return array.operands()[i * 2 + 1];
      }
    }
  }
  else if(array.id()==ID_byte_extract_little_endian ||
          array.id()==ID_byte_extract_big_endian)
  {
    if(array.type().id() == ID_array || array.type().id() == ID_vector)
    {
      optionalt<typet> subtype;
      if(array.type().id() == ID_array)
        subtype = to_array_type(array.type()).subtype();
      else
        subtype = to_vector_type(array.type()).subtype();

      // This rewrites byte_extract(s, o, array_type)[i]
      // to byte_extract(s, o+offset, sub_type)

      auto sub_size = pointer_offset_size(*subtype, ns);
      if(!sub_size.has_value())
        return unchanged(expr);

      // add offset to index
      mult_exprt offset(from_integer(*sub_size, array.op1().type()), index);
      exprt final_offset = simplify_node(plus_exprt(array.op1(), offset));

      exprt result_expr(array.id(), expr.type());
      result_expr.add_to_operands(array.op0(), final_offset);

      return changed(simplify_rec(result_expr));
    }
  }
  else if(array.id()==ID_if)
  {
    const if_exprt &if_expr=to_if_expr(array);
    exprt cond=if_expr.cond();

    index_exprt idx_false=to_index_expr(expr);
    idx_false.array()=if_expr.false_case();

    new_expr.array() = if_expr.true_case();

    exprt result = if_exprt(cond, new_expr, idx_false, expr.type());
    return changed(simplify_rec(result));
  }

  if(no_change)
    return unchanged(expr);
  else
    return std::move(new_expr);
}
