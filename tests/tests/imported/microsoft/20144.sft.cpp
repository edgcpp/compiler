//type:fp
//options_all:--microsoft
constexpr signed int make(unsigned int i)
{
              return i << 16u;
}

constexpr signed int expexted = 0xFFFF0000;
constexpr signed int actual = make(0xFFFFFFFF);

static_assert(actual == expexted, "errro");
