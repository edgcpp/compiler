//type:fp
//options:--ms_c++17 --microsoft_version=1937 -DLATE=1:--ms_c++20 --microsoft_version=1937:--c++17
//remark:qualified call of a function template found through inline namespace

// In Microsoft permissive mode, the overload set of a function called in a
// template is looked up again when the template is instantiated.  For a
// qualified name found through an inline namespace or a using-directive, that
// lookup must still be done in the qualifying namespace (and not find the
// global function).

template <class T> T&& declval();
template <class T> struct void_t_impl { typedef void type; };

double fn(float, double);

namespace lib {
  inline namespace v1 {
    template <class T> T fn(T, T);
  }
}

template <class T, class U, class = void>
struct can_call { static const bool value = false; };
template <class T, class U>
struct can_call<T, U,
       typename void_t_impl<decltype(lib::fn(declval<T>(),
                                             declval<U>()))>::type> {
  static const bool value = true;
};
static_assert(!can_call<float, double>::value, "");
static_assert(can_call<float, float>::value, "");

namespace lib2 {
  namespace v1 {
    template <class T> T fn(T, T);
  }
  using namespace v1;
}

template <class T, class U> constexpr auto test(int)
    -> decltype(lib2::fn(declval<T>(), declval<U>()), true) { return true; }
template <class T, class U> constexpr bool test(...) { return false; }
static_assert(!test<float, double>(0), "");
static_assert(test<int, int>(0), "");

// A name qualified by "::" is looked up again in the global namespace, even
// when the template is in another namespace.
namespace impl {
  template <class T> T g(T, T);
}
using namespace impl;
namespace other {
  void g();
}
namespace ns {
  using namespace other;
  template <class T, class = void>
  struct S1 { static const bool value = false; };
  template <class T>
  struct S1<T, decltype(void(::g(T(), 1.0)))> {
    static const bool value = true;
  };
  static_assert(S1<double>::value, "");

  void g();
  template <class T, class = void>
  struct S2 { static const bool value = false; };
  template <class T>
  struct S2<T, decltype(void(::g(T(), 1.0)))> {
    static const bool value = true;
  };
  static_assert(S2<double>::value, "");
}

template <class T> T k(T, T);
namespace ns2 {
  void k();
  template <class T, class = void>
  struct S3 { static const bool value = false; };
  template <class T>
  struct S3<T, decltype(void(::k(T(), 1.0)))> {
    static const bool value = true;
  };
  static_assert(S3<double>::value, "");
}

#if LATE
// A function declared later in the global namespace is found through "::".
namespace ns2 {
  template <class T> auto h(T t) -> decltype(::k(t)) { return ::k(t); }
}
int k(int);
int i = ns2::h(1);

// An overload declared later in the qualifying namespace is found.
template <class T, class U> auto h(T t, U u) -> decltype(lib::fn(t, u)) {
  return lib::fn(t, u);
}
namespace lib {
  inline namespace v1 {
    double fn(float, double);
  }
}
double d = h(1.0f, 2.0);
#endif
