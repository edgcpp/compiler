//options_all:--c++14
template <class ...T, class ...U>
void i(int, T ..., U ...);

int main() {
    i(3);
}
