//type:fp
//options_all:--microsoft_version 1912 --ms_c++17
template<class T>
constexpr T max(T lhs, T rhs) {
    return lhs < rhs ? rhs : lhs;
}

template<class T>
constexpr size_t new_alignof = max(__STDCPP_DEFAULT_NEW_ALIGNMENT__, alignof(T));

static_assert(new_alignof<int> == __STDCPP_DEFAULT_NEW_ALIGNMENT__, "");
