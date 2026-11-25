//type:fp
//options:--c++26
//options_all:--no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

template<int I, typename ... Ts>
Ts ... [I] t;

template<int I, int ... Js>
auto v1 = Js ... [I];

template<int I, int ... Js>
auto v2 = decltype(Js ... [I]){};

template<int I, int ... Js>
auto v3 = decltype((Js ... [I])){};

template<typename ... Ts>
struct C
{
  void f(Ts ... [0]);

  template<int I>
  void g(Ts ... [I]);
};

template struct C<int>;

template<int ... Is>
struct D
{
  void f(int (*)[Is ... [0]]);

  template<int J>
  void g(int (*)[Is ... [J]]);
};

template struct D<1>;

/* @filter
file-scope expr-node
  kind, type, constant, expr, index_expr, operands, operation.kind,
  position.seq, is_parenthesized
file-scope type[kind==pack_index]
  kind, typeref_type, expr, is_dependent_type_operator, operator_type_arg
file-scope type[kind==is_decltype]
  kind, typeref_type, expr, is_dependent_type_operator,
  decltype_expr_not_parenthesized
 */
