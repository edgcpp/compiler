//type:fp
//options:--c++26:--c++23:--c++26 --gnu=140200:--c++26 --clang_v=200100:--c++26 --gnu=150100:--c++26 --clang_v=190100
//remark:[GH #371] Omitting the comma before a variadic "..." (P3176R1)
//
// In C++26, an ellipsis that is not preceded by a comma, as in
// "void f(int...)", is deprecated.  Like GCC 15 and Clang 20, warn about it
// in C++26 mode (but not when emulating GCC 14 or Clang 19).

void a(int...);                 // deprecated
void b(int, ...);               // OK
void c(...);                    // OK
void d(int x...);               // deprecated
void e(int (*)(int...));        // deprecated (nested)
template<class... T> void f(T......);   // deprecated
template<class... T> void g(T..., ...); // OK
template<class... T> void h(T...);      // OK (pack, not varargs)
void i(auto......);             // deprecated
void j(int = 0 ...);            // deprecated
auto k = [](int...) {};         // deprecated
using F = void(int...);         // deprecated
struct S { void m(int...); };   // deprecated
template<class T> struct X { void m(T...); };  // deprecated
X<int> x;
template<class... T> struct P {
  void m1(T......);             // deprecated (outer pack expansion)
  void m2(T..., ...);           // OK
  void m3(T...);                // OK (outer pack expansion)
  template<class... U> void m4(U......);  // deprecated
  void m5(auto......);          // deprecated
  void m6(auto x...);           // deprecated
  static void m7(int...);       // deprecated
};
P<int, char> p;
void a1(auto...);               // OK (pack)
void a2(auto, ...);             // OK
void a3(auto x...);             // deprecated
void a4(auto... x...);          // deprecated
void a5(auto... x, ...);        // OK
auto l1 = [](auto......) {};    // deprecated
auto l2 = [](auto...) {};       // OK (pack)
auto l3 = [](auto x...) {};     // deprecated
template<class... T> void use(T... t) { f<T...>(t...); }
void z() {
  f<int, char>(1, 'a', 2.0); f<>(); use(1, 2); x.m(1, 2);
  p.m1(1, 'a', 2.0); P<int, char>::m7(1, 2); l1(1, 2); l3(1, 2);
}
