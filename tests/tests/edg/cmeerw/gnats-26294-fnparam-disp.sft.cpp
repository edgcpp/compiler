//type:fp
//options:--c++20
//options_all:--no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^file-scope (name-qualifier@|variable@|variable-template-info@|param-type@.*\nname:|type@.*\nkind: +(tk_struct|tk_pointer|tk_routine|template_arg_list|name_qualifier)|expr-node@.*\nkind: +enk_variable)/' | awk '/^[a-z_]+:$/ { getline n; if (n ~ /^   /) { gsub(/  +/, "", n); print $0 " " n; } else { print $0; print n; } next; }1' | grep -E -e '^(  name|  type|  explicitly_specified|  decl_position[.]seq|name|param_num|is_class|is_reference|position[.]seq|qualifier|previous_qualifier|qualifier[.]class_type|extra_info|kind|type|type_pointed_to|declared_type|param_type_list|typeref_type|template_arg_list|orig_template_arg_list|variable):' -e '^file-scope ' -e '^ +(FALSE|TRUE)' -e '^$' | edg-enumerate-il-addrs

#if TEST_NUMBER == 1

template<typename T>
int var = 0;

struct A
{
  using fn = int (*)(int);
};

int i =
  var<int (*)(A::fn p)> +
  var<int (*)(A::fn *p)>;

#endif
