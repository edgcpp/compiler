//type:cp
//options_all:-P --ms_c++14 --microsoft_v 1935
#define BOOST_PP_EXPAND(x) BOOST_PP_EXPAND_I(x)
#define BOOST_PP_EXPAND_I(x) x

#define BOOST_PP_REM(...) __VA_ARGS__

#define BOOST_PP_CAT(a, b) BOOST_PP_CAT_I(a, b)
#define BOOST_PP_CAT_I(a, b) a ## b

#define BOOST_PP_IIF(bit, t, f) BOOST_PP_IIF_I(bit, t, f)
#define BOOST_PP_IIF_I(bit, t, f) BOOST_PP_IIF_II(BOOST_PP_IIF_ ## bit(t, f))
#define BOOST_PP_IIF_II(id) id
#define BOOST_PP_IIF_0(t, f) f
#define BOOST_PP_IIF_1(t, f) t

#define BOOST_PP_VARIADIC_ELEM(n, ...) BOOST_PP_VARIADIC_ELEM_I(n,__VA_ARGS__)
#define BOOST_PP_VARIADIC_ELEM_I(n, ...) BOOST_PP_CAT(BOOST_PP_CAT(BOOST_PP_VARIADIC_ELEM_, n)(__VA_ARGS__,),)
#define BOOST_PP_VARIADIC_ELEM_0(e0, ...) e0
#define BOOST_PP_VARIADIC_ELEM_1(e0, e1, ...) e1

#define BOOST_PP_TUPLE_ELEM(...) BOOST_PP_TUPLE_ELEM_II(BOOST_PP_TUPLE_ELEM_O_2, (__VA_ARGS__))
#define BOOST_PP_TUPLE_ELEM_II(m, args) BOOST_PP_CAT(m ## args,)

#define BOOST_PP_TUPLE_ELEM_O_2(n, tuple) \
    BOOST_PP_VARIADIC_ELEM(n, BOOST_PP_EXPAND(BOOST_PP_REM tuple)) \

#define BOOST_PP_TUPLE_EAT_1(e0)

#define ELEM(x) typename T

#define BOOST_PP_FOR_1(s) BOOST_PP_IIF(BOOST_PP_TUPLE_ELEM(1, s), ELEM, BOOST_PP_TUPLE_EAT_1)(s)

template < BOOST_PP_FOR_1(((T), 1)) >
struct Integer;
void f(const Integer<int> &);
