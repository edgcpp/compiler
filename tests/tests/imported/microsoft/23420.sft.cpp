//options_all:--strict --c++20
//type:fp
template <class T>
struct S {
    friend int f(S s) noexcept(noexcept(s.meow)) {
        return s.meow;
    }

    T meow = 0;
};

int main() {
    return f(S<int>{});
}
