//options_all:--c++20
template <class, class ...>
inline constexpr bool is_constructible_v = true;

template <class>
class optional {};

template <class T>
struct box : optional<T> {
    using optional<T>::operator=;

    box &operator=(const box &) requires (!is_constructible_v<T, T>);
    box &operator=(box &&) requires (!is_constructible_v<T, T>);
};

template <class T>
struct filter_view { box<T> pred; };

auto pred = [](int) { return true; };
void f(filter_view<decltype(pred)> &range) {
    range = static_cast<filter_view<decltype(pred)> &&>(range);
}
