//options_all:--microsoft_version 1914 --c++17
namespace std {
    template <class _Tuple>
    struct tuple_size;

    template <class _Tuple, class = void>
    struct _Tuple_size_sfinae {};

    template <class _Tuple>
    struct _Tuple_size_sfinae<_Tuple, decltype(tuple_size<_Tuple>::value)> {
        static constexpr size_t value = tuple_size<_Tuple>::value;
    };

    template <class _Tuple>
    struct tuple_size<const _Tuple> : _Tuple_size_sfinae<_Tuple> {};
}

struct tag {
    int t;
    int value;
};

void func(tag &tag) {
    const auto &[k, v] = tag;
};
