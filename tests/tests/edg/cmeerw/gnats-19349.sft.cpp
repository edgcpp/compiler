//type:fp
//options:--c++11 --no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

namespace minimal {
  class A {
    int x = [](int i = [] { return 1; }()) { return 2; }();
  };
}

/* @filter
file-scope field
  source_corresp/name, source_corresp/decl_position.seq,
  source_corresp/decl_position.column, has_initializer, initializer
file-scope class-type-supplement
  is_lambda_closure_class, has_field_initializer,
  defined_in_field_initializer, defined_in_variable_initializer,
  lambda_parent.field, lambda_parent.routine, lambda_parent.variable
file-scope type[kind==tk_class]
  source_corresp/name, source_corresp/decl_position.seq,
  source_corresp/decl_position.column, field_list, extra_info
 */
