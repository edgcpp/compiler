//type:cp
//module_files:foo.ixx
//options_all:--module_import_diagnostics -d-module_report --c++20 --clang_version=140000 --modules

import foo;

template <class T, class U> struct is_same { static const bool value = false; };
template <class T> struct is_same<T, T> { static const bool value = true; };

static_assert(is_same<decltype(half(1)), _Float16>::value, "_Float16");
static_assert(is_same<decltype(fp16_id(1)), __fp16>::value, "__fp16");
static_assert(is_same<decltype(quad(1)), __float128>::value, "__float128");
static_assert(is_same<decltype(bf16_id(1)), __bf16>::value, "__bf16");

static_assert(kind((_Float16)0) == 1, "_Float16");
static_assert(kind((__fp16)0) == 2, "__fp16");
static_assert(kind((__float128)0) == 3, "__float128");
static_assert(kind(0.0L) == 4, "long double");

static_assert(is_same<decltype(holder::a), _Float16>::value, "a");
static_assert(is_same<decltype(holder::b), __fp16>::value, "b");
static_assert(is_same<decltype(holder::c), __float128>::value, "c");
static_assert(is_same<decltype(holder::d), __bf16>::value, "d");

int main() {
  __float128 q = quad(2);
  return 0;
}
