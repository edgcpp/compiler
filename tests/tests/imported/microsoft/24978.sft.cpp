//options_all:--c++20
struct VectorOpts { int dim = -1; };

template <class T, VectorOpts opts>
class Vector {};

int main() {
    Vector<double, { 2 }> v;
}
