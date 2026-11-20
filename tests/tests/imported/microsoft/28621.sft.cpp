//type:cp
//options_all:--microsoft_v 1944
//options:--ms_c++latest:;fn
constexpr auto with_static_constexpr(int x) {
    static constexpr int multiplier = 42;  // Now allowed in constexpr functions
    return x * multiplier;
}

int main()
{
    constexpr auto static_result = with_static_constexpr(2);
    return 0;
}
