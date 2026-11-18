//options_all:--c++14
//type:fp
constexpr struct destruct_t {
    template <class>
    static constexpr bool able = true;
} destruct;

constexpr struct re_construct_t {
    template <class T>
    static constexpr bool able = destruct.able<T>;
} re_construct;
