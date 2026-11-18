//type:fn
//options_all:--microsoft_v 1928 --time_limit 10
template <class>
struct allocator {};

template <class T, class = allocator<T>>
struct vector {};

template <size_t N>
struct NDimVectorGenerator {
    using type = vector<typename NDimVectorGenerator<N - 1>::type>;
    type f();
};

NDimVectorGenerator<0> n;
