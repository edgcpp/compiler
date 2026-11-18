//options_all:--c++20
struct proxy;
struct container { proxy *proxy = nullptr; };
struct proxy { container *cont = nullptr; };

struct iterator {
    constexpr iterator(container *cont) { adopt(cont); }
    constexpr iterator(const iterator &it) { adopt(it.proxy->cont); }
    constexpr void adopt(container *parent) { proxy = parent->proxy; }

    proxy *proxy = nullptr;
};

struct vector {
    constexpr vector() { cont.proxy = new proxy{ &cont }; }
    constexpr ~vector() { delete cont.proxy; }
    constexpr iterator begin() { return iterator(&cont); }
    container cont;
};

constexpr bool equal(iterator) { return true; }

constexpr bool test() {
    vector vec;
    iterator it = vec.begin();
    return equal(it);
}

static_assert(test());
