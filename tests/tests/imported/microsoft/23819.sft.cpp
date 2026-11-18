//options_all:--c++20
template <class T>
struct storage_for {
    union { T object; };
    constexpr ~storage_for() {}
};

struct nontrivial {
    constexpr ~nontrivial() {}
};

constexpr void test() {
    storage_for<nontrivial> s;
}
