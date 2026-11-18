//type:cp
//options_all:--c++20
template <class, class>
constexpr bool is_same_v = false;
template <class T>
constexpr bool is_same_v<T, T> = true;
template <class T1, class T2>
concept same_as = is_same_v<T1, T2>;

template <typename T, typename ...Args>
concept IsAnyOf = (same_as<T, Args> || ...);

template <IsAnyOf<char, wchar_t> T>
struct Foo { Foo(const T *a); };

Foo foo(L"input");
