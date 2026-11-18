//options_all:--ms_c++17
//type:fp
template <class T>
struct VectorData {
    constexpr VectorData(T nx) {}
};

template <class T>
struct Vector1 : VectorData<T> {
    constexpr Vector1(T fillValue) : VectorData<T>(fillValue) {}
};

extern template class __declspec(dllimport) Vector1<int>;

constexpr Vector1<int> vec{5};
