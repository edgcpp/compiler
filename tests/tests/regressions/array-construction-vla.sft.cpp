//type:rp
//options_all:--c++11 --g++
//options:--exceptions:--no_exceptions

int count;

struct Element {
  int value;
  Element() : value(++count) {}
};

int check(int rows) {
  Element elements[rows][3];
  for (int i = 0; i != rows; ++i) {
    for (int j = 0; j != 3; ++j) {
      if (elements[i][j].value != i * 3 + j + 1)
        return 1;
    }
  }
  return count == rows * 3 ? 0 : 2;
}

int main() { return check(2); }
