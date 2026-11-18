//options_all:--c++17
template <int, class R>
typename R::iterator get(R);

template <int>
int get(int);

template <int N>
constexpr int N_v = N;

template <int N = 0, class = decltype(get<N_v<N>>(0))>
void Sample();

void f() { Sample(); }
