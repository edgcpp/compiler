//type: fp
//options: --c++11 --gn 160000:--c++23 --gn 160000:--c++26
//remark:[GH #216] Expansion statements are a GNU extension from C++11.

namespace gnu_mode_boundary {
  void f() {
    template for (int i : {1}) { (void)i; }
  }
}
