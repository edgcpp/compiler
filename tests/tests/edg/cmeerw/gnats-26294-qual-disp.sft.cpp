//type:fp
//options:--c++20
//options_all:--no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

using INT = int;

struct C
{
  struct type
  { };
};

struct D : C
{ };

namespace ns
{
  using INT = int;
  using ::C;

  INT i;

  INT &ir1 = i;
  ::INT &gir1 = i;
  ns::INT &ir2 = ns::i;
  ::ns::INT &gir2 = ::ns::i;

  C c1;
  ::C c2;
  ns::C c3;
  ::ns::C c4;

  using DD = D;

  C::type t1;
  D::type t2;
  DD::type t3;
}

/* @filter
file-scope name-reference[is_global_qualified_name==TRUE]
  num_template_arguments, qualifier, is_template_id, is_global_qualified_name
file-scope name-qualifier
  qualifier.class_type, qualifier.namespace_ptr, previous_qualifier, name,
  is_class
file-scope variable
  source_corresp/name, source_corresp/decl_position.seq, type, declared_type
file-scope type[kind==tk_pointer]
  kind, type_pointed_to, based_types, is_reference
file-scope type[kind==name_qualifier]
  kind, typeref_type, based_types, qualifier, is_global_qualified_name
file-scope type[kind==typedef]
  kind, declared_type, typeref_type, type_pointed_to, based_types,
  orig_template_arg_list, qualifier, previous_qualifier, is_reference
file-scope type[kind==is_alias]
  source_corresp/name, source_corresp/decl_position.seq, kind, typeref_type,
  based_types
file-scope expr-node[kind==enk_variable]
  kind, type, qualifier, variable, position.seq, is_global_qualified_name
 */
