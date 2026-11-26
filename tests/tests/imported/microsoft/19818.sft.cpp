//type:fp
//options_all:--c++17
#include <initializer_list>

struct ABCD {
              int a;
              constexpr ABCD(std::initializer_list<int>) : a{} {
                             a = 1;
              }
};

int main() {
              constexpr ABCD a{ 123 };
}
