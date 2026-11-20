//options_all:--gn 103000 --c++20 --ms_extensions
template <class T>
T &&declval();

template <class T>
void test(T);

template <class T, class U, class = void>
struct is_convertible {};
template <class T, class U>
struct is_convertible<T, U, decltype(test<U>(declval<T>()))> {
    static constexpr bool value = true;
};

template <class T>
struct make_const { using type = const T; };

template <class I> requires is_convertible<const I &, typename make_const<I>::type &>::value
void string_view(I);

class iterator {};
void f(iterator begin) {
    string_view(begin);
}
