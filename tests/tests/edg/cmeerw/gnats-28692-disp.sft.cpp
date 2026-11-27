//type:fp
//options:--c++11 --gn 150200 --no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

template<int ... Is>
struct C {
  struct D { };
};

// make sure the template arguments have the "explicitly specified" flag set
template<int I>
using AI = typename C<__integer_pack(I)...>::D;

using A3 = C<__integer_pack(3)...>::D;

/* @filter
file-scope type[kind==tk_struct]
  source_corresp/name, kind, extra_info
file-scope type[kind==is_alias]
  source_corresp/name, kind, typeref_type, template_arg_list
file-scope type[kind==name_qualifier]
  kind, typeref_type
file-scope class-type-supplement
  template_arg_list
file-scope name-qualifier
  is_class, qualifier.class_type
 */
