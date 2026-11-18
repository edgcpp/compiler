//options_all:--ms_c++latest --microsoft_version=1930
//type:fp
template <class>
struct array {};

template <int>
struct count {};

template <int M, int N, count<N> C>
struct traits { using type = array<typename traits<M + 1, N, C>::type>; };

template <int N, count<N> C>
struct traits<N, N, C> { using type = int; };

traits<0, 3, count<3>{}>::type x;
