//options_all:--c++20
template <class F>
constexpr auto call(F &&f) { return static_cast<F &&>(f)(); }

template <class T>
struct Wrap { T val; };

struct S { int x; };
using Alias = Wrap<S &>;

struct variant {
    union { S s; };

    constexpr variant() : s{ 0 } {}

    constexpr S &get() { return s; }

    constexpr void swap() {
        auto ref = Alias{ this->get() };
        call([ref]() { ref.val; });
    }
};

constexpr bool test_variant() {
    variant left;
    left.swap();
    return true;
}

static_assert(test_variant());
