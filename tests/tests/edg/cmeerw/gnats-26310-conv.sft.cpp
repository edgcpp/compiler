//type:fp
//options:--c++11:--c++11:--c++11:--c++11
//options_all:--no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

template<int I>
struct B : B<I - 1>
{ };

template<>
struct B<0>
{
  int i;
};

B<6> b6;

#if TEST_NUMBER == 1
void f(B<5> b5)
{
  const B<2> &b2 = b5;
}
#elif TEST_NUMBER == 2
void f(B<2> b2)
{
  const B<5> &b5 = static_cast<const B<5> &>(b2);
}
#elif TEST_NUMBER == 3
void f(int B<2>::*p2)
{
  const int B<5>::*p5 = p2;
}
#elif TEST_NUMBER == 4
void f(int B<5>::*p5)
{
  const int B<2>::*p2 = static_cast<const int B<2>::*>(p5);
}
#endif

/* @filter
func-scope expr-node
  kind, type, operands, variable, operation.kind, position.seq,
  position.column, implicit_step_of_explicit_cast, is_lvalue,
  compiler_generated, is_reference_cast, operation.type_kind,
  orig_lvalue_type, is_static_cast, qualifier, num_template_arguments,
  is_global_qualified_name, is_template_id, is_super_qualified,
  is_decltype_qualified, from_prototype_instantiation,
  used_in_primary_declarator
 */
