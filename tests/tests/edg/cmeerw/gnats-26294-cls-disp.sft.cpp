//type:fp
//options:--c++20:--c++20:--c++20:--c++20:--c++20:--c++20:--c++20:--c++20:--c++20:--c++20
//options_all:--no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^(func|file)-scope (name-qualifier@|variable@|class-type-supplement@|type@.*\nkind: +(tk_struct|tk_ptr_to_member|is_alias|is_template_alias|is_decltype|template_arg_list|name_qualifier))/' | awk '/^[a-z_]+:$/ { getline n; if (n ~ /^   /) { gsub(/  +/, "", n); print $0 " " n; } else { print $0; print n; } next; }1' | grep -E -e '^(  name|  type|  explicitly_specified|  decl_position[.]seq|is_class|qualifier|qualifier[.]class_type|extra_info|kind|type|declared_type|class_of_which_a_member|orig_class_of_which_a_member|typeref_type|template_arg_list|orig_template_arg_list|is_global_qualified_name):' -e '^(func|file)-scope ' -e '^$' | edg-enumerate-il-addrs

template<typename T, typename U = T>
struct A
{
  struct B
  { };

  template<typename V>
  struct C
  {
    C(V);
  };

  enum E
  { };
};

template<typename T>
struct D
{ };

using INT1 = int;
using INT2 = int;

#if TEST_NUMBER == 1

A<int> a;

A<INT1> a1;                     // should show up in IL
A<INT1, INT1> a11;              // should show up in IL
A<INT2> a2;                     // should show up in IL
A<INT2, INT2> a22;              // should show up in IL

A<INT1> aa1;                    // should point to existing template argument list
A<INT2> aa2;                    // should point to existing template argument list

#elif TEST_NUMBER == 2

A<int>::B b;

A<INT1>::B b1;                  // should show up in IL
A<INT1, INT1>::B b11;           // should show up in IL
A<INT2>::B b2;                  // should show up in IL
A<INT2, INT2>::B b22;           // should show up in IL

A<INT1>::B bb1;                 // should point to existing template argument list
A<INT2>::B bb2;                 // should point to existing template argument list

#elif TEST_NUMBER == 3

A<int>::E e;

A<INT1>::E e1;                  // should show up in IL
A<INT2>::E e2;                  // should show up in IL

A<INT1>::E ee1;                 // should point to existing template argument list
A<INT2>::E ee2;                 // should point to existing template argument list

#elif TEST_NUMBER == 4

D<A<int>> d;
D<A<INT1>> d1;                  // should show up in IL
D<A<INT2>> d2;                  // should show up in IL

D<A<INT1>> dd1;                 // should point to existing template argument list
D<A<INT2>> dd2;                 // should point to existing template argument list

#elif TEST_NUMBER == 5

typename A<int>::B b;

typename A<INT1>::B b1;         // should show up in IL
typename A<INT1, INT1>::B b11;  // should show up in IL
typename A<INT2>::B b2;         // should show up in IL
typename A<INT2, INT2>::B b22;  // should show up in IL

typename A<INT1>::B bb1;     // should point to existing template argument list
typename A<INT2>::B bb2;     // should point to existing template argument list

#elif TEST_NUMBER == 6

A<int>::C c{0};
A<INT1>::C c1{1};
A<INT2>::C c2{2};
A<INT1>::C cc1{3};
A<INT2>::C cc2{4};

#elif TEST_NUMBER == 7

using DI = D<int>;
using DI1 = D<INT1>;
using DI2 = D<INT2>;

template<int I, typename T>
int T::*p = 0;

auto v = (p<1, D<int>>,
          p<2, DI>,
          p<3, DI1>,
          p<4, DI2>);

#elif TEST_NUMBER == 8

void f()
{
  struct B
  {
    int i;
  };

  using A = B;

  {
    struct B
    { };

    static int A::*p = &A::i;
  }
}

#elif TEST_NUMBER == 9

template<typename T>
using L = A<T>;

void f()
{
  int i;

  A<decltype(i)> a1;
  A<decltype(+i)> a2;

  L<decltype(i)> l1;
  L<decltype(+i)> l2;
}

#elif TEST_NUMBER == 10

struct A<int>::B b;

struct A<INT1>::B b1;         // should show up in IL
struct A<INT1, INT1>::B b11;  // should show up in IL
struct A<INT2>::B b2;         // should show up in IL
struct A<INT2, INT2>::B b22;  // should show up in IL

struct A<INT1>::B bb1;       // should point to existing template argument list
struct A<INT2>::B bb2;       // should point to existing template argument list

#endif
