//type:cp
//options_all:--c++20
template <class B, class D>
inline constexpr bool is_base_of_v = __is_base_of(B, D);

template <class>
struct vector {};

struct Test {};

template <class T> requires is_base_of_v<Test, T>
void test(vector<T>) {}

template <class T, class ...Ts> requires is_base_of_v<Test, T> && (is_base_of_v<Test, Ts> && ...)
void test(vector<T>, vector<Ts> ...) {}

void f(vector<Test> vec) { test(vec); }
