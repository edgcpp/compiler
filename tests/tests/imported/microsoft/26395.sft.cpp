//type:cp
//options_all:--c++20
//options::--microsoft_v 1940:--clang_v 160000
template <class U, class T>
concept convertible_to = __is_convertible_to(U, T);

template <class T>
struct observable {
    observable(const observable &);
    observable(convertible_to<T> auto &&);
};

struct abstract_button_widget {
    observable<int> on_label;
};
