//type:cp
//options_all:--microsoft_v 1940 --ms_c++20
template <size_t N>
struct Literal
{
    char data[N];

    constexpr Literal(const char(&str)[N])
    {
        for (size_t i = 0; i < N; i++)
        {
            data[i] = str[i];
        }
    }
};

template <Literal L>
struct LiteralString
{
    constexpr LiteralString() = default;

    static constexpr size_t size = sizeof(L.data);
    static constexpr const char* data = L.data;

    inline static bool copied = false;
};

struct Identifier
{
    template <Literal L>
    Identifier(LiteralString<L> literal)
    {
        if (!LiteralString<L>::copied)
        {
            for (size_t i = 0; i < LiteralString<L>::size; i++)
                data[i] = LiteralString<L>::data[i];
            LiteralString<L>::copied = true;
        }
    }

    inline static char* data = new char[1024];
};

int main()
{
    Identifier id1 = LiteralString<"Hello World!">();
    return 0;
}
