//type:fp
//options_all:--ms_c++17 --microsoft_version=1914
namespace std {
    template <typename T>
    struct initializer_list {
        constexpr initializer_list(const T*, const T*) noexcept {}
    };
}
struct pair { int a; int b; };
int main() {
    const auto foo = {
        pair{42, 1234},
        {56, 5678}
    };
}
