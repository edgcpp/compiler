//options_all:--microsoft_version 1914
template<class T> struct type_identity {
    using type = T;
};
template<class T> using type_identity_t =
    typename type_identity<T>::type;

struct pair {
    pair& operator=(const volatile pair&) = delete;

    template<class Self = pair>
    pair& operator=(type_identity_t<const Self&>) {
        return *this;
    }
};

struct Derived : pair {};

int main() {
    Derived d;
    d = d;
}
