//type:fp
//options_all:--microsoft_v 1910
template<typename T>
void f();

namespace N {
                template<typename T>
                int f();
}

template<typename T>
decltype(N::f<T>()) g();

int i = g<int>();
