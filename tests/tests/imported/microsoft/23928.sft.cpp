//options_all:--c++20 --il_display
//filter:grep -A4 ^expr_range
struct S {};
void bar(S) {}

void test() {
    bar(S());
}
