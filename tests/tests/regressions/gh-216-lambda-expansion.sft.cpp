//type: cp
//options: --c++26:--c++26 -tused
//remark:[GH #216] Expansion statements in lambdas in expansion bodies.

namespace nested_lambda_expansion
{
  void use(int);

  void f()
  {
    template for (auto x : {1, 2}) {
      [&] { template for (auto z : {5, 6}) { use(x + z); } }();
    }
  }
}

namespace constant_evaluation
{
  constexpr int f()
  {
    int n = 0;
    template for (auto x : {1, 2}) {
      [&] {
        template for (auto z : {5, 6}) {
          n += x + z;
        }
      }();
    }
    return n;
  }
  static_assert(f() == 28);
}

namespace generic_lambda
{
  constexpr int f()
  {
    int n = 0;
    template for (auto x : {1, 2}) {
      [&]<typename T>(T y) {
        template for (auto z : {5, 6}) {
          n += x + y + z;
        }
      }(3);
    }
    return n;
  }
  static_assert(f() == 40);
}
