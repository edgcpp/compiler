//type:fp
//options_all:--microsoft --ms_c++17 --microsoft_version 1914
template<typename Functor, typename ...Args>
inline auto callwith(Functor &&functor, Args&& ... args) -> decltype(functor(args...)) {
    return functor(args...);
}

template<typename Target>
auto constructorCaller = [](auto&& ... args) {
    return Target(args...);
};

struct Foo {
    Foo(int i);
};

int main() {
    callwith(constructorCaller<Foo>, 4);
}
