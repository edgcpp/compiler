//type:fp
//options:--c++20 --no_exceptions --no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

struct A
{
  int i;
  int j;
};

struct C
{
  C(int, int);
};

extern A a;
extern C c;
extern int i;

void f()
{
  new A(a);
  new A{a};
  new A(1, i);
  new A{2, i};

  new C(c);
  new C{c};
  new C(3, i);
  new C{4, i};
}

/* @filter
func-scope dynamic-init
  kind, constant, is_implicit_copy_for_copy_initialization,
  is_copy_constructor_with_implied_source, non_constant,
  is_braced_initializer
func-scope expr-node
  kind, constant, variable, position.seq, position.column,
  new_initializer_is_brace_enclosed, new_initializer_is_paren_aggr_init,
  is_new, has_new_initializer, expr_range/end.column, expr_range/end.seq,
  expr_range/start.column, expr_range/start.seq, is_global_qualified_name,
  is_template_id, is_super_qualified, is_decltype_qualified
 */
