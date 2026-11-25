//type:fp
//options:--c++20
//options_all:--no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

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

/* @filter
file-scope name-qualifier
  qualifier.class_type, previous_qualifier, name, is_class
file-scope variable
  source_corresp/name, source_corresp/decl_position.seq, type, declared_type
file-scope variable-template-info
  template_arg_list
file-scope param-type[name]
  type, declared_type, name, param_num
file-scope type[kind==tk_struct]
  source_corresp/name, source_corresp/decl_position.seq, kind, extra_info
file-scope type[kind==template_arg_list]
  source_corresp/name, source_corresp/decl_position.seq, kind, declared_type,
  typeref_type, type_pointed_to, param_type_list, extra_info,
  template_arg_list, orig_template_arg_list, qualifier, previous_qualifier,
  is_reference
file-scope type[kind==name_qualifier]
  kind, typeref_type, qualifier
file-scope type[kind==tk_pointer]
  kind, type_pointed_to, is_reference
file-scope type[kind==tk_routine]
  kind, param_type_list
file-scope expr-node[kind==enk_variable]
  kind, type, qualifier, variable, position.seq
 */
