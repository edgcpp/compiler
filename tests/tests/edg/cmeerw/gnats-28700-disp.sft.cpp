//type:fp
//options:--c++11
//options_all:-w --no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^(file-scope (name-reference|name-qualifier)@|func-scope expr-node@.*\nkind: *(enk_routine|enk_constant))/' | grep -E -e '^(  type|  explicitly_specified|name|next|type|orig_lvalue_type|routine|qualifier|qualifier[.](class_type|namespace_ptr)|previous_qualifier|position[.]seq|num_template_arguments|is_template_id|orig_template_argument_list):' -e '^(file|func)-scope ' -e '^$' | edg-enumerate-il-addrs

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
