//type:fp
//options:--c++20:--c++20:--c++20:--c++20:--c++20 --gn 150100
//options_all:--no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^file-scope (name-qualifier@|base-class@|class-type-supplement@|type@.*\nkind: +(tk_struct|tk_class|is_template_alias|template_arg_list|name_qualifier))/' | awk '/^[a-z_]+:$/ { getline n; if (n ~ /^   /) { gsub(/  +/, "", n); print $0 " " n; } else { print $0; print n; } next; }1' | grep -E -e '^(  name|  type|  explicitly_specified|  decl_position[.]seq|is_class|previous_qualifier|qualifier|qualifier[.]class_type|extra_info|kind|type|orig_type|typeref_type|proxy_of_type|template_arg_list):' -e '^file-scope ' -e '^$' | edg-enumerate-il-addrs

template<typename T, typename U = T>
struct A
{
  struct N
  { };

  template<typename V>
  struct M
  { };
};

using INT1 = int;
using INT2 = int;

#if TEST_NUMBER == 1

struct D : A<int>
{ };

struct D1 : A<INT1>
{ };

struct D2 : A<INT2>
{ };

struct DD1 : A<INT1>
{ };

struct DD2 : A<INT2>
{ };

#elif TEST_NUMBER == 2

struct D : ::A<int>
{ };

struct D1 : ::A<INT1>
{ };

struct D2 : ::A<INT2>
{ };

struct DD1 : ::A<INT1>
{ };

struct DD2 : ::A<INT2>
{ };

#elif TEST_NUMBER == 3

template<typename T>
struct D1 : A<T>::N
{ };

template<typename T>
struct D2 : A<T>::template M<T>
{ };

template<typename T>
struct GD1 : ::A<T>::N
{ };

template<typename T>
struct GD2 : ::A<T>::template M<T>
{ };

#elif TEST_NUMBER == 4

template<typename T>
struct B : A<typename T::template N<typename T::type>>
{ };

#elif TEST_NUMBER ==5
// this one should be run in GCC mode

struct B
{ };

namespace ns
{
  template<typename> using A = B;
}

template<typename T>
struct C : ns::A<T>
{ };

#endif
