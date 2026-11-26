//type:rp
//options_all:--c++11
//options:--exceptions:--no_exceptions

int constructed;
int defaults;

int next_value() {
  return ++defaults;
}

struct Element {
  int value;
  Element(int n = next_value()) : value(n) {
    ++constructed;
  }
};

struct Container {
  Element elements[2][3];
  Container();
};

Container::Container() {}

int main() {
  Container object;
  if (constructed != 6 || defaults != 6) {
    return 1;
  }
  for (int i = 0; i != 2; ++i) {
    for (int j = 0; j != 3; ++j) {
      if (object.elements[i][j].value != i * 3 + j + 1) {
        return 2;
      }
    }
  }
  return 0;
}
