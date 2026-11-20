//options_all:--ms_c++20
struct hash_value_t {
    size_t _value;
};

struct hash_t {
    template <class T>
    static constexpr void nothrow_helper() {
        if constexpr (requires (const T &t) { hash_value_t(t); })
            return;
    }
};
