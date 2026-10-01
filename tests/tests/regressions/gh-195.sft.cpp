//options_all:--c++23
//type:cp
template<typename T> auto f(const T &t) {
  return t[1, 2];   // Previously generated as "t[1]"
}
struct S {
  int operator[](int, int) const;
} s;
int i = f(s);
