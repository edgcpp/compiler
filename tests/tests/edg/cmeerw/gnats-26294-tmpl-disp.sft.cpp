//type:fp
//options:--c++20:--c++20:--c++20:--c++20
//options_all:--no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^file-scope (name-qualifier@|field@|class-type-supplement@|type@.*\nkind: +(tk_struct|template_arg_list|name_qualifier|is_template_alias))/' | awk '/^[a-z_]+:$/ { getline n; if (n ~ /^   /) { gsub(/  +/, "", n); print $0 " " n; } else { print $0; print n; } next; }1' | grep -E -e '^(  name|  type|  constant|  explicitly_specified|  decl_position[.]seq|is_class|qualifier|qualifier[.]class_type|extra_info|kind|type|declared_type|typeref_type|template_arg_list|orig_template_arg_list|is_global_qualified_name):' -e '^ +(FALSE|TRUE)' -e '^file-scope ' -e '^$' | edg-enumerate-il-addrs

#if TEST_NUMBER == 1

template<typename T, template<typename> class TT>
struct C
{
  typename T::A m1;

  typename TT<T>::B m2;

  typename T::template D<T> m3;

  typename TT<T>::template E<T> m4;
};

#elif TEST_NUMBER == 2

struct T
{
  struct A { };

  template<typename>
  struct D { };
};

template<typename>
struct TT
{
  struct B { };

  template<typename>
  struct E { };
};

template<typename T, template<typename> class TT>
struct C
{
  typename ::T::A m1;

  typename ::TT<T>::B m2;

  typename ::T::template D<T> m3;

  typename ::TT<T>::template E<T> m4;
};

#elif TEST_NUMBER == 3

template<typename U>
struct A
{
  template<int I>
  struct BN
  {
    using C = int;
  };

  template<typename T>
  struct BT
  {
    using C = int;
  };
};

template<typename T, auto J>
struct B
{
  static int f1(int = A<T *>::template BN<1>::C()) ;
  static int f2(int = A<T *>::template BN<J>::C()) ;

  static int g1(int = A<T *>::template BT<int>::C()) ;
  static int g2(int = A<T *>::template BT<T>::C()) ;
};

int i = B<int, 2>::f1() +
        B<int, 2>::f2() +
        B<int, 3>::g1() +
        B<int, 3>::g2();

#elif TEST_NUMBER == 4

template<typename T>
struct C
{
  template<typename U>
  using M = T;
};

template<typename T>
using A1 = typename T::template N<int>;

template<typename T>
using A2 = typename T::template N<T>;

template<typename T>
using A3 = typename C<T>::template M<1>;

template<typename T>
using A4 = typename C<T>::template M<T>;

#endif
