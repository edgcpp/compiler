//options_all:--c++20
//type:fp
template <class, class>
inline constexpr bool is_same_v = false;
template <class T>
inline constexpr bool is_same_v<T, T> = true;

template <class F, class ...As>
void Evaluate(F f, As ...as) requires (!is_same_v<decltype(f(as...)), void>) {}

int min(int a, int b) { return a < b ? a : b; }

int main() {
    Evaluate(min, 1, 2);
}
