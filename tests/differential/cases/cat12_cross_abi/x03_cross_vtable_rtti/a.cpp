#include <cstdio>
    struct Shape {
      virtual int area() const = 0;
      virtual ~Shape() = default;
    };
    struct Rect : Shape {
      int w, h;
      Rect(int ww, int hh) : w(ww), h(hh) {}
      int area() const override;
    };
    Shape* make_rect(int w, int h);
    int main() {
      Shape* s = make_rect(6, 7);
      Rect* r = dynamic_cast<Rect*>(s);
      std::printf("%d:%d\n", s->area(), r ? r->w + r->h : -1);
      delete s;
      return 0;
    }
