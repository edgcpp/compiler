//options_all:--microsoft_v 1934 --ms_c++20
template <class T, T ...>
struct integer_sequence {};
template <class T, T N>
using make_integer_sequence = __make_integer_seq<integer_sequence, T, N>;

template <unsigned ...Ns>
using index_sequence = integer_sequence<unsigned, Ns...>;
template <unsigned N>
using make_index_sequence = make_integer_sequence<unsigned, N>;

template <class T>
inline constexpr bool b_0 = []<unsigned ...Ns>(index_sequence<Ns...>) {
  return (requires() { { Ns }; } && ...);
}(make_index_sequence<T::N>{});

struct A { static constexpr unsigned N = 5; };
static_assert(b_0<A>);
