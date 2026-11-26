//options_all:--c++14
template <class ...>
using void_t = void;

template <class T>
T &&declval() noexcept;

template <class T, class = void>
struct has_get {
    static constexpr bool value = false;
};

template <class T>
struct has_get<T, void_t<decltype(declval<T>().get())>> {
    static constexpr bool value = true;
};

struct base {
protected:
    int get();
};

struct bad : base {
    using base::get;
};

static_assert(has_get<bad>::value, "");
