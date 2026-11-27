//type:fp
//options:--c++20
//options_all:--no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

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

/* @filter
file-scope name-reference[is_global_qualified_name==TRUE]
  num_template_arguments, qualifier, is_template_id, is_global_qualified_name
file-scope instantiation-directive
  declared_type, entity
file-scope name-qualifier
  qualifier.namespace_ptr, name, is_class
file-scope variable
  source_corresp/name, source_corresp/decl_position.seq, type, declared_type
file-scope routine[source_corresp/decl_position.seq]
  source_corresp/name, source_corresp/decl_position.seq, type, declared_type,
  template_arg_list
file-scope class-type-supplement
  template_arg_list
file-scope type[kind==tk_struct]
  source_corresp/name, source_corresp/decl_position.seq, kind, extra_info
file-scope type[kind==template_arg_list]
  source_corresp/name, source_corresp/decl_position.seq, kind, declared_type,
  typeref_type, extra_info, template_arg_list, orig_template_arg_list,
  qualifier
file-scope type[kind==name_qualifier]
  kind, typeref_type, qualifier
file-scope type[kind==is_alias]
  source_corresp/name, source_corresp/decl_position.seq, kind, typeref_type
 */
