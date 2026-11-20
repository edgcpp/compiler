//type:cp
//options_all:--c++20 --microsoft_v 1930
template <auto Size>
struct fixed_string {
    static constexpr auto size = Size;
    constexpr explicit(false) fixed_string(const char* str) {}
};

template <auto Size>
fixed_string(const char(&)[Size]) -> fixed_string<Size>;

template <fixed_string name>
void foo() {}

int main() { foo<"a">(); }
