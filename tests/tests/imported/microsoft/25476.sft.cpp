//options_all:--clang --ms_compatibility
//type:fp
#define BOOST_PP_VARIADIC_DO_SIZE(...) BOOST_PP_VARIADIC_SIZE_I(__VA_ARGS__, 8, 7, 6, 5, 4, 3, 2, 1,)
#define BOOST_PP_VARIADIC_SIZE_I(e0, e1, e2, e3, e4, e5, e6, e7, size, ...) size
static_assert(BOOST_PP_VARIADIC_DO_SIZE(a, b, c) == 3, "Bug");
