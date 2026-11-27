//type:fp
//options:--c++20
//options_all:--no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

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

/* @filter
file-scope name-reference[is_global_qualified_name==TRUE]
  num_template_arguments, qualifier, is_template_id, is_global_qualified_name
file-scope name-qualifier
  qualifier.class_type
file-scope using-decl
  entity, qualifier.class_type, is_using_enum, is_class_member, is_enumerator
file-scope class-type-supplement
  template_arg_list
file-scope type[kind==tk_class]
  source_corresp/decl_position.seq, kind, extra_info
file-scope type[kind==template_arg_list]
  kind, typeref_type, template_arg_list
file-scope type[kind==name_qualifier]
  kind, typeref_type, qualifier
file-scope type[kind==tk_struct]
  source_corresp/decl_position.seq, kind, extra_info
file-scope type[kind==is_alias]
  source_corresp/decl_position.seq, kind, typeref_type
 */
