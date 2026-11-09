//type:fp
//options:--c++20
//options_all:--no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^file-scope (name-qualifier@|variable@|type@.*\nkind: +(tk_pointer|typedef|name_qualifier|is_alias)|expr-node@.*\nkind: +enk_variable)/' | awk '/^[a-z_]+:$/ { getline n; if (n ~ /^   /) { gsub(/  +/, "", n); print $0 " " n; } else { print $0; print n; } next; }1' | grep -E -e '^(  name|  type|  explicitly_specified|  decl_position[.]seq|  reference|  pointer|based_types|position[.]seq|qualifier[.]namespace_ptr|name|qualifier|kind|type|declared_type|variable|orig_template_arg_list|type_pointed_to|typeref_type|is_class|is_reference|is_global_qualified_name):' -e '^ +(FALSE|TRUE)$' -e '^file-scope ' -e '^$' | edg-enumerate-il-addrs

using INT = int;

struct C
{
  struct type
  { };
};

struct D : C
{ };

namespace ns
{
  using INT = int;
  using ::C;

  INT i;

  INT &ir1 = i;
  ::INT &gir1 = i;
  ns::INT &ir2 = ns::i;
  ::ns::INT &gir2 = ::ns::i;

  C c1;
  ::C c2;
  ns::C c3;
  ::ns::C c4;

  using DD = D;

  C::type t1;
  D::type t2;
  DD::type t3;
}
