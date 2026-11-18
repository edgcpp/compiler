//options_all:--microsoft_v 1920
template <class T>
struct identity {
              using type = T;
};

template <class T>
struct tuple {
              tuple &operator=(const tuple&) = delete;

              template <class U = tuple>
              tuple &operator=(typename identity<U&&>::type) {
                             return *this;
              }
};

struct foo {
              tuple<int> t;
};

void f(foo b1, foo b2) {
              b1 = static_cast<foo&&>(b2);
}
