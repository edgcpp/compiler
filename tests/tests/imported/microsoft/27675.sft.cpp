//type:cp
//options:--microsoft_v 1940 --ms_c++20:--c++20
template <class _Ty>
class optional {
public:
    template <class... _Types>
    constexpr explicit optional(_Types&&... _Args) {
    }
};

struct TypeTeller {};

struct FarStates
{
    optional<TypeTeller> opt;
};

int main()
{
    FarStates my_state{
        .opt{TypeTeller{} }
    };
}
