//type:fn
//options:--c++23
//remark:[GH #27] Clearer diagnostic when auto deduces to void
// 10/9/26  [GH #27]
//
// A plain auto deduced from void or another incomplete type gets the
// incomplete-type diagnostic, matching decltype(auto).  auto * still
// fails deduction.

struct Incomplete;

struct Incomplete &getIncomplete();

struct Clap {
  template <typename Spec>
  auto parse(this Spec const& spec) {
  }
};

struct Args : Clap {
};

void g();

void f()
{
  auto v1 = void();
  decltype(auto) v2 = void();
  auto v3 = getIncomplete();
  auto opts = Args{}.parse();

  auto *v4 = void();
  auto *v5 = 1;
  auto *p = g();
}

