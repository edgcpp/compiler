//type:rp
//options_all:--c++11
//options:--exceptions:--no_exceptions

int count;

struct Element {
  int value;
  Element(int n = ++count) : value(n) {}
};

int main() {
  Element elements[2][3][4];
  for (int i = 0; i != 2; ++i) {
    for (int j = 0; j != 3; ++j) {
      for (int k = 0; k != 4; ++k) {
        if (elements[i][j][k].value != 1 + i * 12 + j * 4 + k)
          return 1;
      }
    }
  }
  Element partial[2][3] = { { Element(90) }, { Element(91) } };
  if (partial[0][0].value != 90 || partial[0][1].value != 25 ||
      partial[0][2].value != 26 || partial[1][0].value != 91 ||
      partial[1][1].value != 27 || partial[1][2].value != 28 ||
      count != 28)
    return 2;
  return 0;
}
