//options_all:--ms_c++17
#include <initializer_list>

struct string { ~string(); };
void f(string x) {
    std::initializer_list{ x };
}
