//type:fp
//options_all:--no_il_lower --il_display --c++11
//filter:edg-il-filter
//script:edg-first-stage-il

namespace minimal {
  template<bool>
  struct A {
    friend void f(A a) noexcept(true) { }
  };
  void g(A<true> o) {
    f(o);
  }
}

struct C
{
  friend void f(C p1) noexcept(true) { }

  template<typename U>
  friend void f(C p2, U p3) noexcept(true) { }
};

template<int>
struct D
{
  friend void f(D p4) noexcept(true) { }

  template<typename U>
  friend void f(D p5, U p6) noexcept(true) { }
};

void g(C c, D<0> d)
{
  f(c);
  f(c, 1);

  f(d);
  f(d, 1);
}

/* @filter
func-scope variable
  source_corresp/name, source_corresp/is_local_to_function
 */
