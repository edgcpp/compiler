//type:fp
//options:--c++26
//options_all:--no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^file-scope (expr-node@|type@.*\nkind: +(pack_index|is_decltype))/' | sed -e 'N; s/:\n   \+/: /; t print; P; D; :print; p; D' | grep -E -e '^(kind|expr|index_expr|constant|type|typeref_type|operator_type_arg|operands|is_parenthesized|decltype_expr_not_parenthesized|is_dependent_type_operator|operation[.]kind|position[.]seq):' -e '^file-scope ' -e '^$' | edg-enumerate-il-addrs

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
