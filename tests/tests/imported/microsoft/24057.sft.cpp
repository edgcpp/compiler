//options_all:--microsoft_v 1929 --ms_c++latest
struct Base {
    virtual ~Base() noexcept {}
};

template <typename D>
struct Derived : Base {
    virtual ~Derived() noexcept override = default;

    D d;
};

struct Weird {
    ~Weird() noexcept(false) {}
};

int main() {
    Derived<Weird> rc;
}
