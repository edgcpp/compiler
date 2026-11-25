#include <cstdio>
    #include <string>
    struct Box {
      int id; std::string label;
      Box(int i, const char* s) : id(i), label(s) {}
      Box(const Box& o) : id(o.id + 100), label(o.label + "_cp") {}
      ~Box() {}
    };
    __attribute__((noinline)) Box make_box(Box in) {
      return Box(in.id + 1, in.label.c_str());
    }
    int main() {
      Box b0(5, "item");
      Box b1 = make_box(b0);
      std::printf("%d:%s\n", b1.id, b1.label.c_str());
      return 0;
    }
