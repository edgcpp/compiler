//options_all:--c++20
template <class T>
struct D {
    void foo(auto);
};

template <class T>
void D<T>::foo(auto) {}
