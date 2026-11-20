//type:fn
//options_all:--microsoft_version 1910
struct S {
  constexpr S() {}
  constexpr int g() const;
};
struct T : S {
  int n;
};
constexpr int S::g() const {
  return this->*static_cast<int S::*>(&T::n);
}
static_assert(S().g(), "");
