#line 1
//type:fp
//options:--c++11:--ms_c++20 --microsoft_version 1950
//options_all:--no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^file-scope (name-qualifier@|src-seq-secondary-decl@|type@.*\nkind: +(is_alias|name_qualifier)|type@.*\nenum_type: +TRUE)/' | awk '/^[a-z_]+:$/ { getline n; if (n ~ /^   /) { gsub(/  +/, "", n); print $0 " " n; } else { print $0; print n; } next; }1' | grep -E -e '^(  name|  decl_position[.]seq|decl_position[.]seq|kind|typeref_type|qualifier|name|qualifier[.]namespace_ptr|has_explicit_enum_base|enum_type|is_scoped_enum|enumerator_list_seen|base_type|base_type_position[.]seq|entity|declared_type|autonomous_tag_decl):' -e '^file-scope ' -e '^$' | edg-enumerate-il-addrs

namespace first_definition
{
  using g_uint = unsigned int;

  namespace ns
  {
    enum class E : g_uint { };
  }

  namespace ns
  {
    using ns1_uint = g_uint;
    using ns2_uint = g_uint;
  }

  namespace ns
  {
    enum class E : ns::ns1_uint;
  }

  enum class ns::E : ns::ns2_uint;
}

namespace second_definition
{
  using g_uint = unsigned int;

  namespace ns
  {
    enum class E : g_uint;
  }

  namespace ns
  {
    using ns1_uint = g_uint;
    using ns2_uint = g_uint;
  }

  namespace ns
  {
    enum class E : ns::ns1_uint { };
  }

  enum class ns::E : ns::ns2_uint;
}

namespace third_definition
{
  using g_uint = unsigned int;

  namespace ns
  {
    enum class E : g_uint;
  }

  namespace ns
  {
    using ns1_uint = g_uint;
    using ns2_uint = g_uint;
  }

  namespace ns
  {
    enum class E : ns::ns1_uint;
  }

  enum class ns::E : ns::ns2_uint { };
}
