//options_all:--microsoft_version 1922
struct NonAgg {
    constexpr NonAgg() : mem(42) {}
    int mem;
};

struct Agg {
    int a;

    union {
        char b;
        double c;
        int d = 10000000;
        NonAgg na;
    };
};

int main() {
    Agg a { 1 };
}
