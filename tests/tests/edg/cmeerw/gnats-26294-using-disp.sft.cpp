//type:fp
//options:--c++20
//options_all:--no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^file-scope (name-qualifier@|using-decl@|class-type-supplement@|type@.*\nkind: +(tk_class|tk_struct|is_alias|template_arg_list|name_qualifier))/' | awk '/^[a-z_]+:$/ { getline n; if (n ~ /^   /) { gsub(/  +/, "", n); print $0 " " n; } else { print $0; print n; } next; }1' | grep -E -e '^(  type|  explicitly_specified|  decl_position[.]seq|entity|is_class_member|is_using_enum|is_enumerator|qualifier|qualifier[.]class_type|extra_info|kind|type|declared_type|typeref_type|template_arg_list|orig_template_arg_list|is_global_qualified_name):' -e '^file-scope ' -e '^$' | edg-enumerate-il-addrs

template<typename T, typename U = T>
struct A
{
  template<typename V>
  struct B
  {
    void f();
    void g();
  };

  enum E
  { E1 };
};

using INT1 = int;
using INT2 = int;

template<typename T>
struct D : A<T>::template B<T>
{
  using base = A<T, INT1>::template B<T>;

  using base::f;
  using A<T, INT2>::template B<T>::g;

  using A<T, INT2>::E::E1;

  using T::h;
};
