//options_all:--c++20 --microsoft
struct NonAgg {
              constexpr NonAgg() : mem(42) {}
              int mem;
};

struct Agg {
              int a;

              struct {
                             char b;
                             double c;
                             int d = 10000000;
                             NonAgg na;
              };
};

int main() {
              Agg a{ .a = 1 }; // OK
              Agg b{ .b = 1 }; // spurious error: class "Agg::<unnamed>" has no field 'b'
}
