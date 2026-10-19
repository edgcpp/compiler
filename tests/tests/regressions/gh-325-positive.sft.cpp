//type:fp
//options_all:--c++20
//options:--gnu_version 150200:--clang_version 220100

struct S {};
struct C { operator int() const; operator int*() const; };
enum E { e };
enum class F { f };

namespace mixed_operand_types {
  void f(const short& x, volatile unsigned long& y, int *const& p,
         volatile int *q, E e, F f) {
    __builtin_add_overflow(x, y, p);
    __builtin_sub_overflow(true, e, q);
#ifndef __clang__
    __builtin_mul_overflow(f, false, p);
    __builtin_add_overflow_p(x, y, char{});
    __builtin_sub_overflow_p(e, true, x);
    __builtin_mul_overflow_p(f, false, y);
#endif
  }
}

namespace constexpr_arithmetic {
  constexpr bool g() {
    int n = 0;
    unsigned char c = 0;
    bool b = !__builtin_add_overflow(1, 2u, &n) && n == 3;
    b = b && !__builtin_sub_overflow(5L, 2, &n) && n == 3;
    b = b && !__builtin_mul_overflow(3, 4u, &n) && n == 12;
    b = b && __builtin_add_overflow(255, 1, &c) && c == 0;
    return b;
  }
  static_assert(g());
}

#ifndef __clang__
namespace overflow_predicates {
  static_assert(!__builtin_add_overflow_p(1, 2, char{}));
  static_assert(__builtin_add_overflow_p(255, 1, (unsigned char)0));
  static_assert(__builtin_sub_overflow_p(0, 1, (unsigned char)0));
  static_assert(__builtin_mul_overflow_p(16, 16, (unsigned char)0));
}

namespace bitfield_width_and_side_effects {
  constexpr bool h() {
    struct B { unsigned b:3; } b{};
    int n = 0;
    return __builtin_add_overflow_p(7, 1, b.b) &&
           __builtin_sub_overflow_p(0, 1, b.b) &&
           __builtin_mul_overflow_p(4, 2, b.b) &&
           !__builtin_add_overflow_p(1, 2, ++n) && n == 1;
  }
  static_assert(h());
}
#endif

namespace type_substitution_failures {
  template<class T, class U, class R>
  concept A = requires(T x, U y, R r) {
    __builtin_add_overflow(x, y, r);
    __builtin_sub_overflow(x, y, r);
    __builtin_mul_overflow(x, y, r);
  };
  static_assert(A<short, unsigned long, int*>);
  static_assert(A<bool, E, volatile int*>);
  static_assert(!A<S, int, int*> && !A<int, S, int*>);
  static_assert(!A<double, int, int*> && !A<int, double, int*>);
  static_assert(!A<int*, int, int*> && !A<int, int*, int*>);
  static_assert(!A<int, int, const int*>);
  static_assert(!A<int, int, int> && !A<int, int, float*>);
  static_assert(!A<C, int, int*> && !A<int, int, C>);
  static_assert(!A<int, int, const bool*> && !A<int, int, const E*>);
  static_assert(!A<int, int, F*>);
#ifdef __clang__
  static_assert(!A<F, int, int*> && !A<int, F, int*>);
  static_assert(A<int, int, bool*> && A<int, int, volatile bool*>);
  static_assert(A<int, int, E*> && A<int, int, volatile E*>);
#else
  static_assert(A<F, int, int*>);
  static_assert(!A<int, int, bool*> && !A<int, int, E*>);

  template<class T, class U, class R>
  concept B = requires(T x, U y, R r) {
    __builtin_add_overflow_p(x, y, r);
    __builtin_sub_overflow_p(x, y, r);
    __builtin_mul_overflow_p(x, y, r);
  };
  static_assert(B<short, unsigned long, const int>);
  static_assert(B<bool, E, unsigned char>);
  static_assert(!B<S, int, int> && !B<int, S, int>);
  static_assert(!B<double, int, int> && !B<int, double, int>);
  static_assert(!B<int, int, bool> && !B<int, int, E>);
  static_assert(!B<int, int, int*> && !B<int, int, double>);
#endif
}

namespace argument_count_substitution_failures {
  template<class T> concept D = requires(T x) {
    __builtin_add_overflow(x);
  };
  static_assert(!D<int>);
#ifndef __clang__
  template<class T> concept H = requires(T x) {
    __builtin_mul_overflow_p(x, x, x, x);
  };
  static_assert(!H<int>);
#endif
}

namespace template_instantiations {
  template<class T, class U, class R> bool i(T x, U y, R& r) {
    return __builtin_add_overflow(x, y, &r) ||
           __builtin_sub_overflow(x, y, &r) ||
           __builtin_mul_overflow(x, y, &r);
  }
  template bool i<short, unsigned long, int>(short, unsigned long, int&);
#ifndef __clang__
  template<class T, class U, class R> bool j(T x, U y, R r) {
    return __builtin_add_overflow_p(x, y, r) ||
           __builtin_sub_overflow_p(x, y, r) ||
           __builtin_mul_overflow_p(x, y, r);
  }
  template bool j<short, unsigned long, int>(short, unsigned long, int);
#endif
}

namespace dependent_types {
  int n;
  void *v;

  template<class T> bool f(T x) {
    /* Never instantiated: dependent arguments are deferred, not checked. */
    return __builtin_add_overflow(x, x, &n);
  }

  template<class T> bool g() {
    return __builtin_add_overflow(sizeof(T), sizeof(T), &n);
  }
  template bool g<int>();
  template bool g<S>();

  template<class... Ts> bool h(Ts... args) {
    return __builtin_sub_overflow(args..., &n);
  }
  template bool h<int, int>(int, int);
  template bool h<short, unsigned>(short, unsigned);

  template<class T> struct W {
    T m;
    bool f(T x) { return __builtin_mul_overflow(x, x, &m); }
  };
  template struct W<int>;
  template struct W<unsigned char>;

  template<class T> bool i(T x) {
    return __builtin_add_overflow(x, x, static_cast<T*>(v));
  }
  template bool i<int>(int);

  void j() {
    auto l = [](auto x, auto y) {
      return __builtin_add_overflow(x, y, &n);
    };
    l(1, 2);
  }
#ifndef __clang__
  template<class T> bool k(T x) {
    return __builtin_add_overflow_p(x, x, T{});
  }
  template bool k<int>(int);
  template bool k<unsigned char>(unsigned char);
#endif
}

namespace array_results {
  void f() {
    int a[1] = {};
    __builtin_add_overflow(1, 2, a);
    __builtin_sub_overflow(1, 2, a);
    __builtin_mul_overflow(1, 2, a);
  }
  template<class T> bool g(T& a) {
    return __builtin_add_overflow(1, 2, a);
  }
  template bool g<int[1]>(int (&)[1]);
  constexpr bool h() {
    int a[1] = {};
    bool b = __builtin_add_overflow(1, 2, a);
    return !b && a[0] == 3;
  }
  static_assert(h());
}

#ifdef __clang__
namespace clang_bool_results {
  void k(bool *p) {
    __builtin_add_overflow(1, 2, p);
    __builtin_sub_overflow(1, 2, p);
    __builtin_mul_overflow(1, 2, p);
    bool a[1] = {};
    __builtin_add_overflow(1, 2, a);
  }
}

namespace clang_enum_results {
  void l(E *p) {
    __builtin_add_overflow(1, 2, p);
    __builtin_sub_overflow(1, 2, p);
    __builtin_mul_overflow(1, 2, p);
    E a[1] = {};
    __builtin_sub_overflow(1, 2, a);
  }
}
#endif
