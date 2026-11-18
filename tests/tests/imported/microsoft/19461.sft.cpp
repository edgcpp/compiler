//options_all:--microsoft_version 1914
template <bool x>
bool f();

void x()
{
    [](auto x) -> decltype (f<noexcept(x)>())
    { return 1; } (2);
}
