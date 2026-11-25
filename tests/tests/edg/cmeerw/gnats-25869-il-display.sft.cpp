//type:fp
//options:--c++17 --no_il_lowering --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

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

/* @filter
file-scope type[kind==tk_struct]
  source_corresp/name, next
file-scope scope[kind==sck_class_struct_union]
  assoc_type
 */
