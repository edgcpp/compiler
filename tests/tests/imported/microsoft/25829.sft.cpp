//type:cp
//options_all:--c++11
template <class T> struct NonConst;
template <class T> struct NonConst<const T &> { using type = T &; };

template <class T>
const T &as_const(T &val) noexcept { return val; }

struct Foo {
int val;
const int &Get() const { return val; }

template <class ...T>
auto Get(T &&...a) -> typename NonConst<decltype(Get(a...))>::type {
return const_cast<decltype(Get(a...))>(as_const(*this).Get(a...));
}
};

int f(Foo foo) {
foo.Get() = 3;
return foo.Get();
}
