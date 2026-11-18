//type:cp
//options_all:--c++20
template <class T>
struct remove_reference { using type = T; };
template <class T>
struct remove_reference<T &> { using type = T; };
template <class T>
struct remove_reference<T &&> { using type = T; };
template <class T>
using remove_reference_t = typename remove_reference<T>::type;

template <class T>
T &&forward(remove_reference_t<T> &a) { return static_cast<T &&>(a); }
template <class T>
T &&forward(remove_reference_t<T> &&a) { return static_cast<T &&>(a); }

struct INVOKE {
    template <class Fn, class ...Ts>
    constexpr auto operator()(Fn &&Func, Ts &&...Args) const
        requires requires { forward<Fn>(Func)(forward<Ts>(Args)...); }
    { return forward<Fn>(Func)(forward<Ts>(Args)...); }
};
inline constexpr INVOKE invoke;
template <class Fn, class ...Ts>
concept invocable = requires(Fn && Func, Ts &&...Args) {
    invoke(forward<Fn>(Func), forward<Ts>(Args)...);
};

struct IDENTITY {
    constexpr auto &&operator()(auto &&Val) const { return Val; }
};
inline constexpr IDENTITY identity;

int main() { invoke(identity, 3L); }
