//type: fp
//options: --c++26 --gn 160100
//remark: [GH #216] Retain pragmas on cached try and catch closing braces.

namespace cached_try_pragmas
{
  struct S { char c; int i; };

  void f()
  {
    template for (auto x : { 1, 2 }) try {
#pragma pack(push, 1)
      struct C { char c; int i; };
      static_assert(sizeof(C) == sizeof(char) + sizeof(int));
#pragma pack(pop)
    } catch (...) {
#pragma pack(push, 1)
      struct C { char c; int i; };
      static_assert(sizeof(C) == sizeof(char) + sizeof(int));
#pragma pack(pop)
    }
  }

  struct C { char c; int i; };
  static_assert(sizeof(C) == sizeof(S));
}
