//type:fp
//options_all:--microsoft --ms_c++17 --microsoft_version 1914
#include <initializer_list>

struct t {
    int first;
    int second;
};

int main() {
    for (auto [x, y] : { t{ 1, 2 },{ 3,4 } }) {
    }
}
