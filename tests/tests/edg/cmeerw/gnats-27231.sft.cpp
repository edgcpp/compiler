//type:fp
//options:--c++11 --no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

struct C {
  int f(int);
};
template<typename ... Ts>
struct D {
  template<typename T, int (T::*)(Ts ... args)>
  static int f(Ts ... ts);
};
int i = D<int>::f<C, &C::f>(1);

/* @filter
file-scope param-type
  type, name, param_num, is_parameter_pack, is_pack_element
 */
