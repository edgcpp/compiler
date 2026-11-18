//type:fp
//options_all:--c++17
template<typename E>
struct mask {
              mask(E) {}
              operator bool() { return false; }
};

int main() {
              int t = 0;
              if (mask(t)) {}
}
