#include <cstdio>
    #include <typeinfo>
    struct Base { virtual ~Base() = default; };
    struct A : Base {}; struct B : Base {};
    int main() {
      A a; B b; Base* p1 = &a; Base* p2 = &b;
      std::printf("%d %d %d\n", typeid(*p1) == typeid(A),
                  typeid(*p2) == typeid(B), typeid(*p1) != typeid(*p2));
      return 0;
    }
