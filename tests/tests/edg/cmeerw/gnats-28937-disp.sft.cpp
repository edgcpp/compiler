#line 1
//type:fp
//options:--c++11:--ms_c++20 --microsoft_version 1950
//options_all:--no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

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

/* @filter
file-scope name-qualifier
  name, qualifier.namespace_ptr, qualifier.class_type
file-scope src-seq-secondary-decl
  decl_position.seq, entity, declared_type, autonomous_tag_decl
file-scope type[kind==is_alias]
  source_corresp/name, source_corresp/decl_position.seq, kind, typeref_type
file-scope type[kind==name_qualifier]
  kind, typeref_type, qualifier
file-scope type[enum_type==TRUE]
  source_corresp/name, source_corresp/decl_position.seq, kind,
  has_explicit_enum_base, enum_type, is_scoped_enum, enumerator_list_seen,
  base_type, base_type_position.seq
 */
