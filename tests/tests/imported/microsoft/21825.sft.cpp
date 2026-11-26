//type:fn
//options_all:--microsoft_v 1914 --ms_c++17
struct A {
              template <typename T>
              A(T, typename T::type = 0);
              A(int);
};

struct B : A {
              using A::A;
              B(int n) = delete;
};

B b(42L);
