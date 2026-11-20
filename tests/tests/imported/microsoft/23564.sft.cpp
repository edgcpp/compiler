//options_all:--microsoft_version=1928 --ms_c++latest
//type:fp
struct compile_time_string {
    char content[3];
};

constexpr compile_time_string string{ {'A', '0', '1'} };
constexpr auto data = __builtin_memcmp("A01", string.content, 3);
