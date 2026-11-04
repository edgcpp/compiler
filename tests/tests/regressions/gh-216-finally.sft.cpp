//type: fp
//options: --c++26
//remark:[GH #216] Preserve pragmas following skipped try statements.

namespace following_identifier_pragmas
{
  template<bool B> int f()
  {
    return [] {
      int n = 0;
      if constexpr (B) try {} catch (...) {}
#pragma pack(push, 1)
      n = 1;
      struct S { char c; int i; };
#pragma pack(pop)
      static_assert(sizeof(S) == sizeof(char) + sizeof(int));
      return n;
    }();
  }
  template int f<false>();
}

namespace cached_following_identifier_pragmas
{
  int f()
  {
    int n = 0;
    template for (auto x : { 1, 2 }) try {} catch (...) {}
#pragma pack(push, 1)
    n = 1;
    struct S { char c; int i; };
#pragma pack(pop)
    static_assert(sizeof(S) == sizeof(char) + sizeof(int));
    return n;
  }
}
