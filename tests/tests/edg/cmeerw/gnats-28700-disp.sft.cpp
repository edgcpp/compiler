//type:fp
//options:--c++11
//options_all:-w --no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

namespace ns1
{
  int nontmpl();

  template<typename T> int tmpl();

  struct C
  {
    int nontmpl();
    template<typename T> int tmpl();
  };
}

namespace ns2
{
  using namespace ns1;
}

void g()
{
  using INT = int;

  { int i = ns2::nontmpl(); }
  { auto *p = ns2::nontmpl; }
  { auto *p = &ns2::nontmpl; }

  { int i = ns2::tmpl<INT>(); }
  { auto *p = ns2::tmpl<INT>; }
  { auto *p = &ns2::tmpl<INT>; }
  { auto ns1::C::*p = &ns2::C::tmpl<INT>; }
}

/* @filter
file-scope name-reference
  next, qualifier, num_template_arguments, orig_template_arg_list,
  is_template_id
file-scope name-qualifier
  qualifier.class_type, qualifier.namespace_ptr, name, previous_qualifier
func-scope expr-node[kind==enk_routine]
  type, orig_lvalue_type, next, routine, qualifier,
  num_template_arguments, orig_template_arg_list, is_template_id,
  position.seq
func-scope expr-node[kind==enk_constant]
  type, orig_lvalue_type, next, routine, qualifier,
  num_template_arguments, orig_template_arg_list, is_template_id,
  position.seq
 */
