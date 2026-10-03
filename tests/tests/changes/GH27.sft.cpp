//type:fn
//options_all:--c++23
//remark:[GH #27] Clearer diagnostic when auto deduces to void
// 10/4/26  [GH #27]
//
// Deducing auto from a void initializer used to say only
// "cannot deduce \"auto\" type".

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
  auto opts = Args{}.parse();
  auto x = g();
  auto &&r = g();
  const auto c = g();
  auto *p = g();
  auto *q = new auto(g());
  decltype(auto) d = g();
}

auto *h()
{
  return g();
}
