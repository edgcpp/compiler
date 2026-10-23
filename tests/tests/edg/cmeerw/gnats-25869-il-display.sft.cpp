//type:fp
//options:--c++17 --no_il_lowering --il_display
//filter:grep -E " file-scope type@[0-9a-f]*: E<\(\(bool\)\(V<" | sed -e 's/@[0-9a-f]*:/:/'

template<bool, typename T = void>
struct E
{
  using type = T;
};

template<typename U>
struct V
{
  template<typename T>
  static constexpr bool B = true;

  // make sure the type of the default argument in the implicitly generated
  // deduction guide gets properly output by il_display, something like
  // "E<((bool)(V<U#(3,1)>::B<T#(3,2)>)), void>"
  template<typename T, typename X = typename E<B<T>>::type>
  V(T, U);
};

void f()
{
  V v(1, 2);
  V vv = v;
}
