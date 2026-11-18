//options_all:--c++20
struct iterator {
    int *operator*();
};

template <class T>
concept dereferenceable = requires(T &&t) {
    *(decltype(t) &&)t;
};

template <dereferenceable I>
void is_iterator(I);

void f(iterator i) {
    is_iterator(i);
}
