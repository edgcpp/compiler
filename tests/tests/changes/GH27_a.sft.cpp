//type:fn
//options:--c++11 -A:--c++11 --gnu_version 160200:--c++11 --clang_version 230100:--ms_c++20 --microsoft_version 1951
//remark:[GH #27] Incomplete auto deduction in a template definition
// 10/9/26  [GH #27]
//
// In standard and Clang modes, a plain auto deduced from an incomplete
// type in a template definition gets the incomplete-type diagnostic.  GCC
// and Microsoft don't diagnose that in a template definition, so in GNU and
// Microsoft modes the definition is still accepted.  Outside a template,
// the incomplete-type diagnostic is issued in every mode.

void not_a_template()
{
  auto v = void();
}

template<int>
void use_void()
{
  auto v = void();
}

struct Later;
extern Later later_obj;

template<int>
void use_later()
{
  auto x = later_obj;
}

struct Later { int n; };

void call_later()
{
  use_later<0>();
}
