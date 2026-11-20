//type:fn
//options_all:--microsoft_v 1940
struct year {
    explicit year(int i) {}
};

struct S {
    year y;
};

struct U {
    template<typename T> operator T();
};

int main() {
    S{ {U{}} };
}
