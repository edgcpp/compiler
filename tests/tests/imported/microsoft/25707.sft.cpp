//options_all:--ms_c++14
namespace std {

struct string {
    constexpr string(const char *ptr) : buf() {
        len = __builtin_strlen(ptr);
        for (size_t i = 0; i != len; i++) { buf[i] = ptr[i]; }
    }

    size_t len = 0;
    char buf[16];
};

}

constexpr bool my_equal(const std::string &str, const char *ptr) {
    return __builtin_memcmp(str.buf, ptr, 4) == 0;
}

static_assert(my_equal(std::string{ "cats" }, "cats"), "");
