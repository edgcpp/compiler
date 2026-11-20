//options_all:--c++20 --microsoft
//type:fp
template <class>
struct join_view {
    template <bool>
    struct _Iterator {
        static constexpr bool _Deref_is_glvalue = true;

        friend constexpr bool operator==(const _Iterator &, const _Iterator &) requires _Deref_is_glvalue {
            return true;
        }
    };
};

bool test(join_view<int>::_Iterator<true> it) {
    return it != it;
}
