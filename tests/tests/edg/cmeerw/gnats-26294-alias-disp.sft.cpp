//type:fp
//options:--c++20
//options_all:--no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^file-scope (name-qualifier@|variable@|class-type-supplement@|type@.*\nkind: +(tk_struct|is_template_alias|template_arg_list|name_qualifier))/' | grep -E -e '^(  name|  type|  explicitly_specified|  decl_position[.]seq|is_class|qualifier|qualifier[.]class_type|extra_info|kind|name|type|declared_type|typeref_type|orig_template_arg_list|template_arg_list):' -e '^file-scope ' -e '^$' | edg-enumerate-il-addrs

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
