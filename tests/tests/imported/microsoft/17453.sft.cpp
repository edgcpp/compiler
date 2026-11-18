//type:fp
//options_all:--microsoft_version 1903
struct Inner {
    Inner() { }
    Inner(int, int) { }
};
struct Outer {
    Inner in;
    Outer() noexcept = default;
    Outer(int x, int y) : in(x, y) { }
};
int main() {
    Outer out(11, 22);
}
