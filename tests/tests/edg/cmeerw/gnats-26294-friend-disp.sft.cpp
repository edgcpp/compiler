//type:fp
//options:--c++20
//options_all:--no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^file-scope (name-qualifier@|src-seq-secondary-decl@|class-type-supplement@|type@.*\nkind: +(tk_struct|tk_class|template_arg_list|name_qualifier))/' | awk '/^[a-z_]+:$/ { getline n; if (n ~ /^   /) { gsub(/  +/, "", n); print $0 " " n; } else { print $0; print n; } next; }1' | grep -E -e '^(  name|  type|  explicitly_specified|decl_position[.]seq|is_class|qualifier|qualifier[.]class_type|extra_info|kind|friend_decl|entity|type|declared_type|typeref_type|template_arg_list|orig_template_arg_list|is_global_qualified_name):' -e '^file-scope ' -e '^$' | edg-enumerate-il-addrs

#if TEST_NUMBER == 1

struct B
{ };

template<typename T>
struct C : T::template N<C<T>>
{
  friend typename T::template N<C<T>>;
  friend typename T::template M<C<T>>;

  friend struct B;
  friend struct ::B;
};

#endif
