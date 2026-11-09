//type:fp
//options:--c++20 --no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^file-scope (expr-node@)/' | awk '/^[a-z_]+:$/ { getline n; if (n ~ /^   /) { gsub(/  +/, "", n); print $0 " " n; } else { print $0; print n; } next; }1' | grep -E -e '^(is_type_constraint|kind|concept_id[.]args|constant|type|operation[.]kind|operands|  type|  explicitly_specified|  constant|position[.]seq):' -e '^file-scope ' -e '^$' | edg-enumerate-il-addrs

namespace use_first_param
{
  template<typename T, typename U, T>
  concept C = true;

  template<C<long, 2> T>
  void f(T);
}

namespace use_second_param
{
  template<typename T, typename U, U>
  concept C = true;

  template<C<long, 2> T>        // "2" should be cast to long
  void f(T);
}
