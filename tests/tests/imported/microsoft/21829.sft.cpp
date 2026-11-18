//type:fn
//options_all:--microsoft_v 1915 --no_ms_permissive
template <typename T>
struct Base {
              template <class U>
              void foo() {}
};

template <typename T>
struct X : Base<T> {
              void foo() {
                             Base<T>::foo<int>();
              }
};
