//type:cp
//options_all:--c++20
template <class>
concept movable = true;

template <class>
concept sentinel_for = true;

struct string_iterator {};

template <class>
struct lexer {
    template <sentinel_for I>
    struct iterator {};

    static_assert(movable<iterator<string_iterator>>);
};
