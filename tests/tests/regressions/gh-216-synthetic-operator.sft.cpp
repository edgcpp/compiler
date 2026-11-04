//type:fp
//options:--c++26 --gnu_version 160100 --diag_suppress 821 --set_flag reflection -tused
//require:BACK_END_IS_CP_GEN_BE 0
//remark:[GH #216] Synthetic operators during expansion definition recording.

namespace std
{
  namespace meta
  {
    using info = decltype(^^::);
    template<typename R> consteval info substitute(info, R &&);
  }  /* namespace meta */
}  /* namespace std */

namespace synthetic_reflection_range_operators
{
  struct I
  {
    const std::meta::info *p;
    constexpr bool operator!=(const I &other) const
    {
      return p != other.p;
    }
    constexpr I &operator++() { ++p; return *this; }
    constexpr const std::meta::info &operator*() const { return *p; }
  };

  struct R
  {
    const std::meta::info *p;
    constexpr I begin() { return { p }; }
    constexpr I end() { return { p + 1 }; }
  };

  template<typename... Ts> struct S {};

  void f()
  {
    template for (auto x : { 1 })
    {
      constexpr std::meta::info args[] = { ^^int };
      [[maybe_unused]] auto r = std::meta::substitute(^^S, R{ args });
    }
  }
}
