//type:fp
//options:--c++20 --no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

namespace use_first_param
{
  template<typename T, typename U, T>
  concept C = true;

  template<C<long, 2> T>
  void f(T);
}

namespace use_second_param
{
  template<typename T, typename U, U>
  concept C = true;

  template<C<long, 2> T>        // "2" should be cast to long
  void f(T);
}

/* @filter
file-scope expr-node
  is_type_constraint, kind, concept_id.args, constant, type, operation.kind,
  operands, position.seq
 */
