//type:fp
//options:--c++20:--c++20:--c++20:--c++20:--c++20:--c++20:--c++20:--c++20
//options_all:--no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^file-scope (expr-node@|name-qualifier@|class-type-supplement@|variable@|type@.*\nkind: +(tk_class|tk_struct|tk_template_param|template_arg_list|name_qualifier|is_decltype))/' | grep -E -e '^(  name|  type|  explicitly_specified|constant|position[.]seq|is_class|qualifier[.]class_type|expr|operands|variable|field|operation[.]kind|previous_qualifier|qualifier|extra_info|kind|type|declared_type|typeref_type|template_arg_list):' -e '^file-scope ' -e '^$' | edg-enumerate-il-addrs

template<typename T = int, typename U = T>
struct A
{
  constexpr A()
  { }

  constexpr A(int)
  { }

  struct B
  { };

  enum E
  { };
};

using INT1 = int;
using INT2 = int;

#if TEST_NUMBER == 1

auto a = static_cast<A<int>>(0);
auto a1 = static_cast<A<INT1>>(0); // should show up in IL
auto a2 = static_cast<A<INT2>>(0); // should show up in IL

auto aa1 = static_cast<A<INT1>>(0); // should point to existing template argument list
auto aa2 = static_cast<A<INT2>>(0); // should point to existing template argument list

#elif TEST_NUMBER == 2

auto a = (A<int>)(0);
auto a1 = (A<INT1>)(0);         // should show up in IL
auto a2 = (A<INT2>)(0);         // should show up in IL

auto aa1 = (A<INT1>)(0);     // should point to existing template argument list
auto aa2 = (A<INT2>)(0);     // should point to existing template argument list

#elif TEST_NUMBER == 3

auto a = A<int>(0);
auto a1 = A<INT1>(0);           // should show up in IL
auto a2 = A<INT2>(0);           // should show up in IL

auto aa1 = A<INT1>(0);       // should point to existing template argument list
auto aa2 = A<INT2>(0);       // should point to existing template argument list

#elif TEST_NUMBER == 4

auto a = new A<int>(0);
auto a1 = new A<INT1>(0);       // should show up in IL
auto a2 = new A<INT2>(0);       // should show up in IL

auto aa1 = new A<INT1>(0);   // should point to existing template argument list
auto aa2 = new A<INT2>(0);   // should point to existing template argument list

#elif TEST_NUMBER == 5

auto a = sizeof(A<int>);
auto a1 = sizeof(A<INT1>);      // should show up in IL
auto a2 = sizeof(A<INT2>);      // should show up in IL

auto aa1 = sizeof(A<INT1>);  // should point to existing template argument list
auto aa2 = sizeof(A<INT2>);  // should point to existing template argument list

#elif TEST_NUMBER == 6

template<typename T>
using AA = T;

template<typename T>
auto var = (typename T::template N<T>(1),
  typename T::template N<AA<T>>(1));

#elif TEST_NUMBER == 7

auto f()
{
  struct S
  {
    using t1 = int;

    struct A
    {
      using t2 = int;
    } a;
  };

  return S{};
}

auto s = f();
decltype(s)::t1 i1;
decltype(s.a)::t2 i2;

#elif TEST_NUMBER == 8

namespace ns
{
  namespace ns2
  {
    struct B
    { };
  };

  template<typename T>
  struct C
  {
    struct D
    {
      D(int, int);
    };
  };

  using A = C<ns2::B>;
};

auto v = ns::A::D{1, 2};

#endif
