//type:fp
//options_all:--ms_c++17 --microsoft_version 1911
namespace std {
    template <typename>
    struct tuple
    {
        int a;
    };

    template <typename>
    struct tuple_size
    {
        static const int value = 1;
    };

    template <int, typename>
    struct tuple_element
    {
        using type = int;
    };

    template <int>
    int& get(tuple<int>& t)
    {
        return t.a;
    }
}

auto t = std::tuple<int>{1};
auto& [a] = t;
