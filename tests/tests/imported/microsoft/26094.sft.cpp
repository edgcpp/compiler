//type:cp
//options_all:--c++20 --xref x
//filter:cut -f2- x
template <class, class>
concept inner_concept = true;

template <class>
int some_func();

template <class T>
concept outer_concept = requires {
 { 0 } -> inner_concept<decltype(some_func<T>())>;
};

template <outer_concept T>
void inner_func() {}

void func() { inner_func<int>();}
