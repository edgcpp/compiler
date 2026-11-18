//options_all:--c++20
struct Span {
    const int * const * member;

    constexpr Span(int ** arg) : member(arg) {}
};

constexpr int ** ptr = nullptr;

constexpr Span sp(ptr);

int main() {
    (void) sp;
}
