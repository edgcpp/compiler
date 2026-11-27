//type:cp
//module_files:foo.ixx
//options_all:--module_import_diagnostics -d-module_report --c++20 --gnu_version=150100 --modules

import foo;

template <class T, class U> struct is_same { static const bool value = false; };
template <class T> struct is_same<T, T> { static const bool value = true; };

static_assert(is_same<decltype(f32x_half(1)), _Float32x>::value, "_Float32x");
static_assert(is_same<decltype(f64x_half(1)), _Float64x>::value, "_Float64x");
static_assert(is_same<decltype(fp16_id(1)), __fp16>::value, "__fp16");
static_assert(is_same<decltype(quad(1)), __float128>::value, "__float128");
static_assert(is_same<decltype(bf16_id(1)), __bf16>::value, "__bf16");

static_assert(!is_same<decltype(f32x_half(1)), double>::value, "32x");
static_assert(!is_same<decltype(f64x_half(1)), long double>::value, "64x");
static_assert(!is_same<decltype(quad(1)), _Float128>::value, "128");
static_assert(!is_same<decltype(fp16_id(1)), _Float16>::value, "16");

static_assert(kind((_Float16)0) == 1, "_Float16");
static_assert(kind((__fp16)0) == 2, "__fp16");
static_assert(kind((_Float32)0) == 3, "_Float32");
static_assert(kind((_Float32x)0) == 4, "_Float32x");
static_assert(kind((_Float64)0) == 5, "_Float64");
static_assert(kind((_Float64x)0) == 6, "_Float64x");
static_assert(kind((_Float128)0) == 7, "_Float128");
static_assert(kind((__float128)0) == 8, "__float128");
static_assert(kind((__bf16)0) == 9, "__bf16");
static_assert(kind(0.0) == 10, "double");
static_assert(kind(0.0L) == 11, "long double");

static_assert(is_same<decltype(holder::a), _Float32x>::value, "a");
static_assert(is_same<decltype(holder::b), _Float64x>::value, "b");
static_assert(is_same<decltype(holder::c), __fp16>::value, "c");
static_assert(is_same<decltype(holder::d), __float128>::value, "d");
static_assert(is_same<decltype(holder::e), __bf16>::value, "e");

static_assert(size_of<__float128>() == 16, "size_of");
static_assert(is_same<decltype(make_box().value), __float128>::value, "box");
static_assert(is_same<decltype(make_box32x().value), _Float32x>::value,
              "box32x");

int main() {
  __float128 q = quad(make_box().value);
  _Float32x x = f32x_half(make_box32x().value);
  return 0;
}
