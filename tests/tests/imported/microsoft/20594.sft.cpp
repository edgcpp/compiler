//options_all:--c++17
template <typename T>
struct B {
    B(T p2) {}
};

int main() {
    B<int>* test1 = new B{ 5 };
}
