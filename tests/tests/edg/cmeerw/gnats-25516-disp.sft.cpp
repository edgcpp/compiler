//type:fp
//options:--c++23
//options_all:--no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^file-scope (base-class@|using-decl@|routine@.*special_kind: +sfk_deduction_guide)/' | grep -E -e '^(  name|  decl_position[.]seq|kind|type|orig_type|derived_class|direct|entity|special_kind|compiler_generated|is_explicit_constructor|is_template_function|is_inheriting_ctor|is_prototype_instantiation|is_explicit_constructor|is_deduction_guide_from_inheriting_ctor|qualifier[.]class_type|position[.]seq):' -e '^file-scope ' -e '^$' | edg-enumerate-il-addrs

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
