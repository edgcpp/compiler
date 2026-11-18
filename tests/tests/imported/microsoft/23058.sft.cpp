//options_all:--c++17
//type:fp
template <class T>
T &&declval() noexcept;

struct wrapper { bool no_throw; };

template <class T>
constexpr wrapper choice = { noexcept(*declval<T>()) };

template <class T>
void bar(T &&v) noexcept(choice<T>.no_throw) {}

void f(int *x) { bar(x); }
