//type:cp
//options_all:--microsoft_v 1940 --ms_c++20
template <class, class>
constexpr bool is_same_v = false;
template <class _Ty>
constexpr bool is_same_v<_Ty, _Ty> = true;

template <class _Ty1, class _Ty2>
concept _Same_impl = is_same_v<_Ty1, _Ty2>;

template <class _Ty1, class _Ty2>
concept same_as = _Same_impl<_Ty1, _Ty2>&& _Same_impl<_Ty2, _Ty1>;

struct fullfills_my_concept
{
    template <typename Function>
    static auto test(const Function& func)
    {
        return 10;
    }
};

template <typename T>
concept MyConcept = requires {
    {
        T::test([]() { return 20; })
    } -> same_as<int>;
};

template <MyConcept T = fullfills_my_concept>
struct reprod
{
    reprod() {}
};

int main()
{
    reprod test{};

    return 0;
}
