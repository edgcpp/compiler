//options_all:--c++14
//type:fp
template <class ...TArgs>
struct GetAPI {
    static auto &get() = delete;
};

template <>
inline auto &GetAPI<>::get() {
    static int x;
    return x;
}
