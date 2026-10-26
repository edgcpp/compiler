//type:fp
//options:--clang_version 230000:--gnu_version 150000
//options_all:--c++17 -w
//remark:Unrecognized attribute expanding packs of different lengths

// An unrecognized attribute whose arguments expanded packs of different
// lengths was treated as an expansion of all of them, causing a spurious
// error.  Its pack references must also not be diagnosed as unexpanded, nor
// make a later "int ..." look like a parameter pack.

template <class... T> struct S {
  template <class... U> struct In {
    void f(T&... t, U&... u) __attribute__((requires_capability(t..., u...)));
    void g(int ...);
  };
};
S<int, char>::In<long> s;

template <class... T> struct tl {};

template <class... T, class... U>
void h(tl<T...>, U... u) __attribute__((unknown_attr(T..., u...))) {}

void use() {
  h(tl<int, char>(), 1);
}
