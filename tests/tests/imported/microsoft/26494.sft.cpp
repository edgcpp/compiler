//type:cp
//options_all:--c++20
template <class, class>
concept sentinel_for = true;

template <class>
concept movable = true;

template <class>
struct lexer {
    template <class It, sentinel_for<It> ItEnd>
    struct iterator {};

    static_assert(movable<iterator<int, int>>);
};
