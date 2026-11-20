//options_all:--c++20
template <class, class>
inline constexpr bool is_same_v = false;

template <class T>
inline constexpr bool is_same_v<T, T> = true;

template <class T>
concept integral = is_same_v<T, unsigned>;

namespace bits {
    template <class T>
    using raw = __underlying_type(T);

    template <class T>
    concept Enum = __is_enum(T);

    template <Enum T>
    constexpr raw<T> rep(T t) { return raw<T>(t); }

    template <integral T>
    constexpr T rep(T t) { return t; }
}

enum class Cardinality : unsigned { };

template <class T, class Container>
struct Heap : public Container {
    using Cardinality = ::Cardinality;

    void next_position(Cardinality length) const {
        bits::rep(length);
    }
};
