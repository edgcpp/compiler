struct Shape {
  virtual int area() const = 0;
  virtual ~Shape() = default;
};
struct Rect : Shape {
  int w, h;
  Rect(int ww, int hh) : w(ww), h(hh) {}
  int area() const override;
};
int Rect::area() const { return w * h; }
Shape* make_rect(int w, int h) { return new Rect(w, h); }
