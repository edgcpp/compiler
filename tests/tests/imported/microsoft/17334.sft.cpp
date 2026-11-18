//type:fp
//options_all:--microsoft_version 1903
template <typename... Types> struct count_types {
    static constexpr int value = sizeof...(Types);
};
template <typename... Types> constexpr int count_types_v = count_types<Types...>::value;
static_assert(count_types_v<char, short, int, long> == 4, "BOOM");
