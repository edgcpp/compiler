//type:cp
//options_all:--c++20
namespace ranges {
    template <class, int>
    concept store_size = false;

    template <class I, int K = 0>
    struct subrange {
        subrange(I) requires (!store_size<I, K>) {}
    };
}

void f(int *it) { ranges::subrange{ it }; }
