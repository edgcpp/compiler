//options:--c++17 -A:--c++17 --gn 160200:--c++17 --clang_version 230100:--ms_c++17 --microsoft_version 1950;fn:--ms_c++17 --microsoft_version 1951:--c++14 -A;fn:--c++14 --gn 160200;fn:--c++14 --clang_version 230100;fn:--ms_c++14 --microsoft_version 1950;fn:--ms_c++14 --microsoft_version 1951;fn
//options_all:-tused
  template <bool E> void f1(void (*)() noexcept(E));

  void g1();
  void g2() noexcept;

  void h() {
    f1(g1);    // OK: E is false
    f1(g2);    // OK: E is true
  }
