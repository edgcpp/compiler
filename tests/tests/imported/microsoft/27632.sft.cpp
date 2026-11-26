//options_all:--ms_c++latest
//type:fp
#if 1
template <typename T>
struct S
{
                static constexpr auto lambda = [](auto) noexcept(true){};
};

S<bool> s;
#endif
