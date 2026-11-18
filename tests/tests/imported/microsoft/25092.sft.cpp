//options:--gn 90100:--clang_v 100000
  template<typename T> struct S {
    explicit(sizeof(T) > sizeof(int)) S(int x);
  };

  S<char> s{3};  // Okay.
