//options_all:--c++20
template <class U = decltype([]() {}) >
void foo() {}

int main() {
    foo();
}
