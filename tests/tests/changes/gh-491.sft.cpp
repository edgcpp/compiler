//type:fp
//options:--c++14 -tused
//remark:Spurious error for pack expansions in nested generic lambdas

int f() {
  auto g = [](auto... x) {
    return [](auto, int n = sizeof...(x)) { return n; }(0);
  };
  return g(1, 2);  // Previously a spurious error, now okay.
}
