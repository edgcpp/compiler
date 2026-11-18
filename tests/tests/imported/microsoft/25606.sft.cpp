//options_all:--microsoft --c++20
//type:fp
constexpr size_t strlen(const char *s) {
    const char *t = s;
    while (*t++ != '\0') {}
    return t - s;
}

struct string_view {
    const char *data;
    size_t size;

    constexpr string_view(const char *s) : data(s), size(strlen(s)) {}
};

struct string {
    char *data = nullptr;
    size_t size = 0;

    constexpr string(string_view sv) : data(new char[sv.size]), size(sv.size) {
        for (auto i = size_t{ 0 }; i < sv.size; i++) { data[i] = sv.data[i]; }
    }
    constexpr ~string() {
        delete[] data;
    }

    friend constexpr bool operator==(const string &s, string_view sv) {
        return s.size == sv.size && __builtin_memcmp(s.data, sv.data, s.size) == 0;
    }
};

constexpr bool test() {
    const string_view sv{"This long string must be dynamically allocated."};
    string s{sv};
    return s == sv;
}

static_assert(test());
