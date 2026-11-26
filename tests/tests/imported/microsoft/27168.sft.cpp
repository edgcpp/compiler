//type:cp
//options_all:--microsoft_v 1940
template <class T>
void f(T) {
    if constexpr (sizeof(T) != sizeof(int)) {
        static_assert(false, "must be int-sized");
    }
}

template <int N, typename T>
struct MeowElement {};
template <int N>
struct MeowElement<N, double> {
    static_assert(false, "We forbid T from being double");
};

int main() {
    f(0); // OK
    MeowElement<1729, long> meow;
    (void)meow;
}
