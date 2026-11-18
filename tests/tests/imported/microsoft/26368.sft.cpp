//type:cp
//options_all:--microsoft_v 1938 --c++17
struct S;
S *get();

void f() {
    []() {
        if (auto s = get(); true) {}
    }();
}
