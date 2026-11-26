//options_all:--c++17
template <class ...Ts>
struct tuple {
    template <class ...Us>
    tuple(Us &&...);
};

template <class ...Ts>
struct configuration : tuple<Ts...> {
    template <class ...Us>
    configuration(tuple<Us...> &&);
};

void f(configuration<int> c) {
    configuration edit_scheme = c;
}
