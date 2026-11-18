//options_all:--c++14 --microsoft
template<int N> struct sink {};
struct S {
                constexpr int f() const { return 1; }
};
sink<S().f()> s;
