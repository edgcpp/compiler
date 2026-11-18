//type:fp
//options_all:--c++17 --microsoft
using size_t = decltype(sizeof(int));

template<size_t... Is> struct seq;

template<class> struct S;
template<size_t... Is> struct S<seq<Is...>> {
#ifdef WORKAROUND
    static constexpr bool BB = ((Is == 0) || ...);

    template<bool B = BB>
#else
    template<bool B = ((Is == 0) || ...)>
#endif
    static int f() {
        if constexpr (B) return 1;
        else return 0;
    }
};

int main() {
    S<seq<1,2,3>>::f();
}
