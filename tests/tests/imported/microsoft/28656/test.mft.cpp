//type:rp
//options_all:--microsoft_v 1944
//options:--ms_c++23:--ms_c++17
int n = 0;
struct S {
  ~S() { n = 1; }
  S const* begin() const { return this; }
  S const* end() const { return nullptr; }
};
S const& g(S const &p) { return p; }
int main() {
  for (auto x : g(S{})) {
    return n;
  }
}
