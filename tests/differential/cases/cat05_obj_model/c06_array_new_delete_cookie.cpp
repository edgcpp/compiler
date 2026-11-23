#include <cstdio>
    static int events = 0;
    struct Elem { int id;
      Elem() : id(++events) {}
      ~Elem() { events = events * 10 + id; } };
    int main() {
      Elem* arr = new Elem[3]; delete[] arr;
      std::printf("%d\n", events);
      return 0;
    }
