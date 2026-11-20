//type:fn
//options_all:--c++17
struct BASE { int &&x; };

struct A : BASE {
    mutable int _x = 29;
    union {
        int x[59];
        int y = y;
    };
    int z;
};

constexpr A a = { 37, 47, 59 };

int main() {
    if (a.x + a.y + a.z != 143)
        return 1;

    return 0;
}
