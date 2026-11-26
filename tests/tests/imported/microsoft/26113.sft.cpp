//type:cp
//options_all:--c++20
struct Vector { struct Iterator *iterator = nullptr; };

struct Iterator {
    Vector *parent;
    constexpr Iterator(Vector *p) : parent(p) { parent->iterator = this; }
    constexpr ~Iterator() { parent->iterator = nullptr; }
};

constexpr Iterator begin(Vector *p) { return Iterator(p); }

constexpr bool test() {
    Vector container;
    auto iter = begin(&container);
    auto p = container.iterator->parent;
    return true;
}

static_assert(test());
