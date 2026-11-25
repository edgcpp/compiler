//type:fp
//options:--c++23
//options_all:--no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

template<typename T>
struct B
{
  B(T);
};

template<typename T>
explicit B(T *) -> B<T *>;

template<typename T>
struct D1 : B<T>
{
  using B<T>::B;
};

D1 d1a(1);
D1 d1b(2);
D1 d1c("");

template<typename T>
using A = B<T>;

template<typename T>
struct D2 : A<T>
{
  using D2::B::B;
};

D2 d2a(1);
D2 d2b(2);
D2 d2c("");

/* @filter
file-scope base-class
  type, orig_type, derived_class, direct
file-scope using-decl
  entity, qualifier.class_type, position.seq, is_inheriting_ctor
file-scope routine[special_kind==sfk_deduction_guide]
  source_corresp/name, source_corresp/decl_position.seq, special_kind, type,
  is_prototype_instantiation, is_template_function, is_explicit_constructor,
  is_deduction_guide_from_inheriting_ctor, compiler_generated
 */
