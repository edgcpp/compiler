//type: fn
//options: --c++03 --gn 160000:--c++11 --gn 150200:--c++11
//remark:[GH #216] The GNU extension needs GCC 16 and C++11 or later.

namespace gnu_mode_boundary {
  void f() {
    template for (int i : {1}) { (void)i; }
  }
}
