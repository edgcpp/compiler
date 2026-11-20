//options_all:--c++20
template <class ...>
struct List;
template <class X, class ...Xs>
struct List<X, Xs...> {
    static constexpr auto size = 1 + sizeof...(Xs);
    using Head = X;
    using Tail = List<Xs...>;
};
template <>
struct List<> { static constexpr auto size = 0ULL; };

template <class T>
concept HasT = requires { typename T::T; };

template <class L, template <class> class F>
    requires HasT<F<typename L::Head>> || (L::size == 0)
struct Map { using T = typename Map<typename L::Tail, F>::T; };
template <template <class> class F>
struct Map<List<>, F> { using T = List<>; };

template <class>
struct C { using T = bool; };
using Type0 = Map<List<int>, C>::T;
