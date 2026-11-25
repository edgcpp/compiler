//type:fp
//options:--c++20
//options_all:--no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

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

/* @filter
file-scope name-reference[is_global_qualified_name==TRUE]
  num_template_arguments, qualifier, is_template_id, is_global_qualified_name
file-scope name-qualifier
  qualifier.class_type, name, is_class
file-scope src-seq-secondary-decl
  declared_type, entity, decl_position.seq, friend_decl
file-scope class-type-supplement
  template_arg_list
file-scope type[kind==tk_struct]
  source_corresp/name, kind, extra_info
file-scope type[kind==template_arg_list]
  kind, typeref_type, template_arg_list
file-scope type[kind==name_qualifier]
  kind, typeref_type, qualifier, is_global_qualified_name
file-scope type[kind==tk_class]
  source_corresp/name, kind, extra_info
 */
