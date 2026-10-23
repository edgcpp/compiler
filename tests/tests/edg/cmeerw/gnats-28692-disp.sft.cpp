//type:fp
//options:--c++11 --gn 150200 --no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^file-scope (type@.*\nkind: +(name_qualifier|is_alias|tk_struct)|class-type-supplement@|name-qualifier@)/' | grep -E -e '^(  name|  type|  constant|  is_pack|  is_pack_element|  explicitly_specified|kind|typeref_type|template_arg_list|extra_info|is_class|qualifier[.]class_type:):' -e '^file-scope ' -e '^$' | edg-enumerate-il-addrs

template<int ... Is>
struct C {
  struct D { };
};

// make sure the template arguments have the "explicitly specified" flag set
template<int I>
using AI = typename C<__integer_pack(I)...>::D;

using A3 = C<__integer_pack(3)...>::D;
