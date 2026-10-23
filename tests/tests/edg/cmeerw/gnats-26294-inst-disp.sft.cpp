//type:fp
//options:--c++20
//options_all:--no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^file-scope (instantiation-directive@|name-qualifier@|variable@|class-type-supplement@|routine@.*\n  decl_position|type@.*\nkind: +(tk_struct|is_alias|template_arg_list|name_qualifier))/' | grep -E -e '^(  name|  type|  explicitly_specified|  decl_position[.]seq|entity|is_class|qualifier|qualifier[.]class_type|qualifier[.]namespace_ptr|extra_info|kind|type|declared_type|typeref_type|template_arg_list|orig_template_arg_list|is_global_qualified_name):' -e '^file-scope ' -e '^$' | edg-enumerate-il-addrs

namespace ns
{
  using INT = int;

  template<typename T>
  INT f(INT)
  { return 0; }

  template<typename T>
  int var = 0;

  template<typename T>
  struct C
  { };
}

using INT1 = int;
using INT2 = int;

template INT2 ns::f<INT2 *>(INT2);

template INT2 ns::var<INT2 *>;

template struct ns::C<INT2 *>;
