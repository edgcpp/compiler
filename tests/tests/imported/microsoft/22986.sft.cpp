//options_all:--c++20
//type:fp
template<typename T>
struct as_cref_ { using type = T const &; };

template<typename T>
concept weakly_equality_comparable_with_frag_ = requires(typename as_cref_<T>::type t) { 0; };

static_assert(weakly_equality_comparable_with_frag_<int>);
