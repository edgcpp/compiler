//options_all:--ms_c++20
template <class ...>
struct MultipleInstance {
    void MemAccessInvoke(auto function, auto &&...args) {}
};

extern template MultipleInstance<char, short, int, long>;
