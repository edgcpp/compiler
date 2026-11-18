//type:cp
//options_all:--microsoft_v 1940 --ms_c++latest
// Repros with prod/fe x86chk as of 2023-10-11.
// Affects STL test P2165R4_tuple_like_common_type.

// EDG: Conditional explicit specifier is checked too early in constrained constructor

template <class>
constexpr bool always_false = false;

template <class T>
concept False = always_false<T>;

template <class X>
constexpr bool do_not_instantiate() {
    static_assert(always_false<X>);
    return true;
}

struct T {
    template <False X>
    explicit(do_not_instantiate<X>()) T(X) {}

    T(int) {}
};

T t(5);
