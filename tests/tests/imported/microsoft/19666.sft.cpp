//options_all:--microsoft_version 1914
template <typename T, size_t N>
char (*__countof_helper(__unaligned T(&r)[N]))[N];

void f()
{
    int r[1];
    sizeof(*__countof_helper(r));
}
