//type:cp
//options_all:--c++20
template <class T>
inline constexpr bool is_copy_constructible_v = __is_constructible(T, const T &);
template <class T>
concept copy_constructible = is_copy_constructible_v<T>;

template <class T>
inline constexpr bool is_trivially_copy_constructible_v = __is_trivially_constructible(T, const T &);
template <class T>
concept trivially_copy_constructible = is_trivially_copy_constructible_v<T> && copy_constructible<T>;

template <class T>
inline constexpr bool is_trivially_copyable_v = __is_trivially_copyable(T);

template <class T>
struct Optional {
    Optional(const Optional<T> &) requires trivially_copy_constructible<T> = default;
    Optional(const Optional<T> &) requires copy_constructible<T>;
};

static_assert(is_trivially_copyable_v<Optional<int>>);
