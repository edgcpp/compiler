//options_all:--c++17
template <auto N>
struct Test {};

template <auto ...Ns>
struct Tests {};

template <class Type, Type N>
void foo(Test<N>) {}

template <class Type, Type ...Ns>
void foos(Tests<Ns...>) {}

int main() {
    Test<10> t;
    foo(t);

    Tests<10, 20> ts;
    foos(ts);
}
