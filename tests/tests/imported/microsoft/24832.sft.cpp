//options_all:--ms_c++17
//options::--ms_std_preprocessor
namespace std {
    template<bool _Test, class _Ty = void>
    struct enable_if {};
    template<class _Ty>
    struct enable_if<true, _Ty> { using type = _Ty; };
    template<bool _Test, class _Ty = void>
    using enable_if_t = typename enable_if<_Test, _Ty>::type;
}

#define CPP_PP_CAT_(X, ...)  X ## __VA_ARGS__
#define CPP_PP_CAT(X, ...)   CPP_PP_CAT_(X, __VA_ARGS__)

#define CPP_PP_EVAL_(X, ARGS) X ARGS
#define CPP_PP_EVAL(X, ...) CPP_PP_EVAL_(X, (__VA_ARGS__))

#define CPP_PP_EVAL2_(X, ARGS) X ARGS
#define CPP_PP_EVAL2(X, ...) CPP_PP_EVAL2_(X, (__VA_ARGS__))

#define CPP_PP_EXPAND(...) __VA_ARGS__
#define CPP_PP_EAT(...)

#define CPP_PP_FIRST(LIST) CPP_PP_FIRST_ LIST
#define CPP_PP_FIRST_(...) __VA_ARGS__ CPP_PP_EAT

#define CPP_PP_SECOND(LIST) CPP_PP_SECOND_ LIST
#define CPP_PP_SECOND_(...) CPP_PP_EXPAND

#define CPP_PP_CHECK(...) CPP_PP_EXPAND(CPP_PP_CHECK_N(__VA_ARGS__, 0,))
#define CPP_PP_CHECK_N(x, n, ...) n
#define CPP_PP_PROBE(x) x, 1,
#define CPP_PP_PROBE_N(x, n) x, n,

#define CPP_PP_IS_PAREN(x) CPP_PP_CHECK(CPP_PP_IS_PAREN_PROBE x)
#define CPP_PP_IS_PAREN_PROBE(...) CPP_PP_PROBE(~)

// The final CPP_PP_EXPAND here is to avoid
// https://stackoverflow.com/questions/5134523/msvc-doesnt-expand-va-args-correctly
#define CPP_PP_COUNT(...)                                                       \
    CPP_PP_EXPAND(CPP_PP_COUNT_(__VA_ARGS__,                                    \
        50, 49, 48, 47, 46, 45, 44, 43, 42, 41,                                 \
        40, 39, 38, 37, 36, 35, 34, 33, 32, 31,                                 \
        30, 29, 28, 27, 26, 25, 24, 23, 22, 21,                                 \
        20, 19, 18, 17, 16, 15, 14, 13, 12, 11,                                 \
        10, 9, 8, 7, 6, 5, 4, 3, 2, 1,))

#define CPP_PP_COUNT_(                                                          \
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _10,                           \
    _11, _12, _13, _14, _15, _16, _17, _18, _19, _20,                           \
    _21, _22, _23, _24, _25, _26, _27, _28, _29, _30,                           \
    _31, _32, _33, _34, _35, _36, _37, _38, _39, _40,                           \
    _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, N, ...)                   \
    N

#define CPP_PP_IIF(BIT) CPP_PP_CAT_(CPP_PP_IIF_, BIT)
#define CPP_PP_IIF_0(TRUE, ...) __VA_ARGS__
#define CPP_PP_IIF_1(TRUE, ...) TRUE

#define CPP_PP_LPAREN (
#define CPP_PP_RPAREN )

#define CPP_PP_NOT(BIT) CPP_PP_CAT_(CPP_PP_NOT_, BIT)
#define CPP_PP_NOT_0 1
#define CPP_PP_NOT_1 0

#define CPP_PP_EMPTY()
#define CPP_PP_COMMA() ,
#define CPP_PP_LBRACE() {
#define CPP_PP_RBRACE() }
#define CPP_PP_COMMA_IIF(X)                                                     \
    CPP_PP_IIF(X)(CPP_PP_EMPTY, CPP_PP_COMMA)()

#define CPP_PP_FOR_EACH(M, ...)                                                 \
    CPP_PP_FOR_EACH_N(CPP_PP_COUNT(__VA_ARGS__), M, __VA_ARGS__)
#define CPP_PP_FOR_EACH_N(N, M, ...)                                            \
    CPP_PP_CAT(CPP_PP_FOR_EACH_, N)(M, __VA_ARGS__)
#define CPP_PP_FOR_EACH_1(M, _1)                                                \
    M(_1)
#define CPP_PP_FOR_EACH_2(M, _1, _2)                                            \
    M(_1), M(_2)
#define CPP_PP_FOR_EACH_3(M, _1, _2, _3)                                        \
    M(_1), M(_2), M(_3)
#define CPP_PP_FOR_EACH_4(M, _1, _2, _3, _4)                                    \
    M(_1), M(_2), M(_3), M(_4)
#define CPP_PP_FOR_EACH_5(M, _1, _2, _3, _4, _5)                                \
    M(_1), M(_2), M(_3), M(_4), M(_5)
#define CPP_PP_FOR_EACH_6(M, _1, _2, _3, _4, _5, _6)                            \
    M(_1), M(_2), M(_3), M(_4), M(_5), M(_6)
#define CPP_PP_FOR_EACH_7(M, _1, _2, _3, _4, _5, _6, _7)                        \
    M(_1), M(_2), M(_3), M(_4), M(_5), M(_6), M(_7)
#define CPP_PP_FOR_EACH_8(M, _1, _2, _3, _4, _5, _6, _7, _8)                    \
    M(_1), M(_2), M(_3), M(_4), M(_5), M(_6), M(_7), M(_8)

#define CPP_PP_PROBE_EMPTY_PROBE_CPP_PP_PROBE_EMPTY                             \
    CPP_PP_PROBE(~)

#define CPP_PP_PROBE_EMPTY()
#define CPP_PP_IS_NOT_EMPTY(...)                                                \
    CPP_PP_EVAL(                                                                \
        CPP_PP_CHECK,                                                           \
        CPP_PP_CAT(                                                             \
            CPP_PP_PROBE_EMPTY_PROBE_,                                          \
            CPP_PP_PROBE_EMPTY __VA_ARGS__ ()))

#if defined(_MSC_VER) && !defined(__clang__) && (__cplusplus <= 201703L)
#define CPP_TRUE_FN                                                             \
    !::concepts::detail::instance_<                                             \
        decltype(CPP_true_fn(::concepts::detail::xNil{}))>
#else
#define CPP_TRUE_FN CPP_true_fn(::concepts::detail::xNil{})
#endif

/// INTERNAL ONLY
#define CPP_TEMPLATE_AUX_ CPP_TEMPLATE_SFINAE_AUX_

#define CPP_template_sfinae(...)                                                \
    template<__VA_ARGS__ CPP_TEMPLATE_SFINAE_AUX_

/// INTERNAL ONLY
#define CPP_TEMPLATE_SFINAE_PROBE_CONCEPT_concept                               \
    CPP_PP_PROBE(~)

/// INTERNAL ONLY
#define CPP_TEMPLATE_SFINAE_AUX_WHICH_(FIRST, ...)                              \
    CPP_PP_EVAL(                                                                \
        CPP_PP_CHECK,                                                           \
        CPP_PP_CAT(CPP_TEMPLATE_SFINAE_PROBE_CONCEPT_, FIRST))

/// INTERNAL ONLY
#define CPP_TEMPLATE_SFINAE_AUX_(...)                                           \
    CPP_PP_CAT(                                                                 \
        CPP_TEMPLATE_SFINAE_AUX_,                                               \
        CPP_TEMPLATE_SFINAE_AUX_WHICH_(__VA_ARGS__,))(__VA_ARGS__)

// A template with a requires clause
/// INTERNAL ONLY
#define CPP_TEMPLATE_SFINAE_AUX_0(...) ,                                        \
    bool CPP_true = true,                                                       \
    std::enable_if_t<                                                           \
        CPP_PP_CAT(CPP_TEMPLATE_SFINAE_AUX_3_, __VA_ARGS__) &&                  \
        CPP_BOOL(CPP_true),                                                     \
        int> = 0>

/// INTERNAL ONLY
#define CPP_TEMPLATE_DEF_SFINAE_AUX_(...) ,                                     \
    bool CPP_true,                                                              \
    std::enable_if_t<                                                           \
        CPP_PP_CAT(CPP_TEMPLATE_SFINAE_AUX_3_, __VA_ARGS__) &&                  \
        CPP_BOOL(CPP_true),                                                     \
        int>>

/// INTERNAL ONLY
#define CPP_TEMPLATE_SFINAE_AUX_3_requires

/// INTERNAL ONLY
#define CPP_CTOR_SFINAE_PROBE_NOEXCEPT_noexcept                                 \
    CPP_PP_PROBE(~)

/// INTERNAL ONLY
#define CPP_CTOR_SFINAE_MAKE_PROBE(FIRST,...)                                   \
    CPP_PP_CAT(CPP_CTOR_SFINAE_PROBE_NOEXCEPT_, FIRST)

/// INTERNAL ONLY
#define CPP_CTOR_SFINAE_REQUIRES(...)                                           \
    CPP_PP_CAT(                                                                 \
        CPP_CTOR_SFINAE_REQUIRES_,                                              \
        CPP_PP_EVAL(                                                            \
            CPP_PP_CHECK,                                                       \
            CPP_CTOR_SFINAE_MAKE_PROBE(__VA_ARGS__,)))(__VA_ARGS__)

// No noexcept-clause:
/// INTERNAL ONLY
#define CPP_CTOR_SFINAE_REQUIRES_0(...)                                         \
    std::enable_if_t<                                                           \
        CPP_PP_CAT(CPP_TEMPLATE_SFINAE_AUX_3_, __VA_ARGS__) && CPP_TRUE_FN,     \
        ::concepts::detail::Nil                                                 \
    > = {})

// Yes noexcept-clause:
/// INTERNAL ONLY
#define CPP_CTOR_SFINAE_REQUIRES_1(...)                                         \
    std::enable_if_t<                                                           \
        CPP_PP_EVAL(CPP_PP_CAT,                                                 \
            CPP_TEMPLATE_SFINAE_AUX_3_,                                         \
            CPP_PP_CAT(CPP_CTOR_SFINAE_EAT_NOEXCEPT_, __VA_ARGS__)) && CPP_TRUE_FN,\
        ::concepts::detail::Nil                                                 \
    > = {})                                                                     \
    CPP_PP_EXPAND(CPP_PP_CAT(CPP_CTOR_SFINAE_SHOW_NOEXCEPT_, __VA_ARGS__)))

/// INTERNAL ONLY
#define CPP_CTOR_SFINAE_EAT_NOEXCEPT_noexcept(...)

/// INTERNAL ONLY
#define CPP_CTOR_SFINAE_SHOW_NOEXCEPT_noexcept(...)                             \
    noexcept(__VA_ARGS__)                                                       \
    CPP_PP_EAT CPP_PP_LPAREN

namespace concepts::detail {
    template<typename T>
    inline constexpr T instance_ = T{};

    struct Nil {};
    enum class xNil {};

    struct CPP_true_t {
        constexpr bool operator()(Nil) const noexcept {
            return true;
        }
        constexpr bool operator()(xNil) const noexcept {
            return true;
        }
    };

    inline constexpr CPP_true_t CPP_true_fn_ {};

    constexpr bool CPP_true_fn(xNil) {
        return true;
    }
}

template <const ::concepts::detail::CPP_true_t &CPP_true_fn =
                ::concepts::detail::CPP_true_fn_>
void forward_tuple_interface(CPP_CTOR_SFINAE_REQUIRES(
    noexcept(true)
    requires true)
{}
