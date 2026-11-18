//options_all:--c++17 --microsoft
//type:fp
namespace std {

template <bool _Test, class _Ty = void>
struct enable_if {};
template <class _Ty>
struct enable_if<true, _Ty> { using type = _Ty; };

template <class _Ty>
struct remove_cv {
    using type = _Ty;

    template <template <class> class _Fn>
    using _Apply = _Fn<_Ty>;
};

template <class _Ty>
using remove_cv_t = typename remove_cv<_Ty>::type;

template <class _Type, template <class...> class _Template>
inline constexpr bool _Is_specialization_v = false;
template <template <class...> class _Template, class... _Types>
inline constexpr bool _Is_specialization_v<_Template<_Types...>, _Template> = true;

template <class>
struct in_place_type_t {};

struct variant {
    template <class _Ty, typename enable_if<_Is_specialization_v<remove_cv_t<_Ty>, in_place_type_t>, int>::type = 0>
    variant(_Ty&&) {}
};

}

void run_test() {
    struct my_variant : std::variant {
        using std::variant::variant;
    };
    my_variant{std::in_place_type_t<int>{}};
}
