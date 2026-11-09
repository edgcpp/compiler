//type:fn
//options_all:--c++20
//options:--gnu_version 150200:--clang_version 220100

struct S {};
struct C { operator int() const; operator int*() const; };
enum E { e };
enum class F { f };

namespace invalid_operand_types {
  void f(S s, C c, int *p, double d, void (*g)()) {
    __builtin_add_overflow(s, 1, p);
    __builtin_add_overflow(1, s, p);
    __builtin_sub_overflow(p, 1, p);
    __builtin_sub_overflow(1, p, p);
    __builtin_mul_overflow(d, 1, p);
    __builtin_mul_overflow(1, d, p);
    __builtin_add_overflow(c, 1, p);
    __builtin_sub_overflow(1, nullptr, p);
    __builtin_mul_overflow(g, 1, p);

#ifndef __clang__
    __builtin_add_overflow_p(s, 1, 0);
    __builtin_add_overflow_p(1, s, 0);
    __builtin_sub_overflow_p(p, 1, 0);
    __builtin_sub_overflow_p(1, p, 0);
    __builtin_mul_overflow_p(d, 1, 0);
    __builtin_mul_overflow_p(1, d, 0);
    __builtin_mul_overflow_p(1, c, 0);
#endif
  }
}

namespace invalid_result_types {
  void g(int *p, float *q, bool *b, E *e, F *f, const int *c,
         const volatile int *v, void *w, int **r, S *s, C x) {
    __builtin_add_overflow(1, 2, q);
#ifndef __clang__
    __builtin_sub_overflow(1, 2, b);
    __builtin_mul_overflow(1, 2, e);
#endif
    __builtin_add_overflow(1, 2, f);
    __builtin_sub_overflow(1, 2, c);
    __builtin_mul_overflow(1, 2, v);
    __builtin_add_overflow(1, 2, w);
    __builtin_sub_overflow(1, 2, r);
    __builtin_mul_overflow(1, 2, s);
    __builtin_add_overflow(1, 2, 0);
    __builtin_sub_overflow(1, 2, nullptr);
    __builtin_mul_overflow(1, 2, x);

#ifndef __clang__
    __builtin_add_overflow_p(1, 2, p);
    __builtin_sub_overflow_p(1, 2, true);
    __builtin_mul_overflow_p(1, 2, E::e);
    __builtin_add_overflow_p(1, 2, F::f);
    __builtin_sub_overflow_p(1, 2, 0.0);
    __builtin_mul_overflow_p(1, 2, S{});
    __builtin_add_overflow_p(1, 2, nullptr);
    __builtin_sub_overflow_p(1, 2, x);
#endif
  }
}

namespace invalid_argument_counts {
  void h(int *p) {
    __builtin_add_overflow();
    __builtin_sub_overflow(1);
    __builtin_mul_overflow(1, 2);
    __builtin_add_overflow(1, 2, p, 3);
    __builtin_sub_overflow(1, 2, p, 3);
    __builtin_mul_overflow(1, 2, p, 3);
#ifndef __clang__
    __builtin_add_overflow_p();
    __builtin_sub_overflow_p(1);
    __builtin_mul_overflow_p(1, 2);
    __builtin_add_overflow_p(1, 2, 0, 3);
    __builtin_sub_overflow_p(1, 2, 0, 3);
    __builtin_mul_overflow_p(1, 2, 0, 3);
#endif
  }
}

namespace braced_initializer_arguments {
  void i(int *p) {
    __builtin_add_overflow({1}, 2, p);
    __builtin_sub_overflow(1, {2}, p);
    __builtin_mul_overflow(1, 2, {p});
#ifndef __clang__
    __builtin_add_overflow_p({1}, 2, 0);
    __builtin_sub_overflow_p(1, {2}, 0);
    __builtin_mul_overflow_p(1, 2, {0});
#endif
  }
}

namespace invalid_template_instantiation {
  template<class T> bool j(T x, int *p) {
    return __builtin_add_overflow(x, 1, p);
  }
  template bool j<double>(double, int*);
}

namespace array_and_function_operands {
  void f(int *p) {
    int a[1] = {};
    __builtin_add_overflow(a, 2, p);
    __builtin_add_overflow(f, 2, p);
  }
}

namespace array_second_operand {
  void g(int *p) {
    int a[1] = {};
    __builtin_sub_overflow(1, a, p);
  }
}

#ifndef __clang__
namespace array_predicate_result {
  void h() {
    int a[1] = {};
    __builtin_mul_overflow_p(1, 2, a);
  }
}
#endif

#ifdef __clang__
namespace clang_scoped_enum_operands {
  void f(int *p) {
    __builtin_add_overflow(F::f, 2, p);
    __builtin_sub_overflow(1, F::f, p);
    __builtin_mul_overflow(F::f, F::f, p);
  }
}
#endif

namespace const_and_noninteger_result_pointers {
  void k(F *p, const bool *b, const E *e, const int *q, float *r) {
    __builtin_add_overflow(1, 2, p);
    __builtin_sub_overflow(1, 2, b);
    __builtin_mul_overflow(1, 2, e);
    __builtin_add_overflow(1, 2, q);
    __builtin_sub_overflow(1, 2, r);
  }
}
