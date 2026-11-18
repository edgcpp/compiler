//options_all:--c++17 --microsoft_version 1920
int main() {
              struct S {
                             constexpr static int member() { return 123; }
              } constexpr s;

              constexpr auto r = [](const S& ss) {
                             constexpr auto a = ss.member();
                             constexpr auto b = S::member();
                             return a + b;
              }(s);
}
