//type:cp
//module_files:foo.ixx
//options_all:--module_import_diagnostics -d-module_report --c++20 --gnu_version=120000 --modules

import foo;

template <class T, class U> struct is_same { static const bool value = false; };
template <class T> struct is_same<T, T> { static const bool value = true; };

static_assert(is_same<decltype(half(1)), _Float16>::value, "_Float16");
static_assert(is_same<decltype(f80_id(1)), __float80>::value, "__float80");
static_assert(is_same<decltype(quad(1)), __float128>::value, "__float128");
static_assert(is_same<decltype(bf16_id(1)), __bf16>::value, "__bf16");

static_assert(kind((_Float16)0) == 1, "_Float16");
static_assert(kind(0.0f) == 2, "float");
static_assert(kind(0.0) == 3, "double");
static_assert(kind((__float128)0) == 4, "__float128");
static_assert(kind((__bf16)0) == 5, "__bf16");

static_assert(is_same<decltype(quad_ptr(0)), __float128 *>::value, "ptr");
static_assert(is_same<decltype(f16_cptr(0)), const _Float16 *>::value,
              "const ptr");
static_assert(is_same<decltype(half_fn()), _Float16 (*)(_Float16)>::value,
              "fn");

static_assert(is_same<decltype(holder::a), _Float16>::value, "a");
static_assert(is_same<decltype(holder::c), __float128>::value, "c");
static_assert(is_same<decltype(holder::d), __bf16>::value, "d");
static_assert(is_same<decltype(holder::e), __float80>::value, "e");
static_assert(is_same<decltype(holder().get_c()), __float128>::value,
              "get_c");

static_assert(is_same<quad_t, __float128>::value, "quad_t");
static_assert(is_same<bf16_t, __bf16>::value, "bf16_t");

int main() {
  holder h = {};
  __float128 q = quad(h.get_c());
  return 0;
}
