//type:fp
//options:--c++11 --no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^file-scope (field@|class-type-supplement@|type@.*\nkind: +(tk_class))/' | awk '/^[a-z_]+:$/ { getline n; if (n ~ /^   /) { gsub(/  +/, "", n); print $0 " " n; } else { print $0; print n; } next; }1' | grep -E -e '^(  name|  decl_position[.](seq|column)|has_initializer|has_field_initializer|field_list|initializer|extra_info|is_lambda_closure_class|defined_in_field_initializer|defined_in_variable_initializer|lambda_parent[.](field|routine|variable)):' -e '^file-scope ' -e '^$' | edg-enumerate-il-addrs

namespace minimal {
  class A {
    int x = [](int i = [] { return 1; }()) { return 2; }();
  };
}
