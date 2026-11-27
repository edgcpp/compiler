//type:fp
//options:--c++20
//options_all:--no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

template<typename T>
struct A
{
  struct B
  { };
};

using INT1 = int;
using INT2 = int;

template<typename T = int, typename U = T>
using AA = A<T *>;

A<int *> p;
AA<int> a;

AA<> a0;

AA<INT1> a1;
AA<INT2> a2;

AA<INT1> aa1;
AA<INT2> aa2;

AA<INT1, INT1> a11;
AA<INT2, INT2> a22;

AA<int>::B b;
AA<INT1>::B b1;
AA<INT2>::B b2;

AA<INT1>::B bb1;
AA<INT2>::B bb2;

/* @filter
file-scope name-qualifier
  qualifier.class_type, name, is_class
file-scope variable
  source_corresp/name, source_corresp/decl_position.seq, type, declared_type
file-scope class-type-supplement
  template_arg_list
file-scope type[kind==tk_struct]
  source_corresp/name, source_corresp/decl_position.seq, kind, extra_info
file-scope type[kind==template_arg_list]
  kind, typeref_type, template_arg_list
file-scope type[kind==name_qualifier]
  kind, typeref_type, qualifier
file-scope type[kind==is_template_alias]
  source_corresp/name, source_corresp/decl_position.seq, kind, typeref_type,
  template_arg_list, orig_template_arg_list
 */
