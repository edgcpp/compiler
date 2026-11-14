//type: fp
//options: --c++26 --microsoft_version 1952 --ms_permissive
//remark: [GH #216] Cache exactly one unbraced Microsoft asm statement.

namespace braced_asm_body
{
  void f()
  {
    template for (auto x : { 1 }) __asm { nop }
    constexpr int n = 1;
    static_assert(n == 1);
  }
}

namespace unbraced_asm_body
{
  void f()
  {
    template for (auto x : { 1 }) __asm nop
    constexpr int n = 2;
    static_assert(n == 2);
  }
}
