//type:rp
//options_all:--c++11 --g++
//options:--exceptions:--no_exceptions

// Partially initialized multidimensional GNU variable-length arrays: each
// element must be constructed and destroyed exactly once, and elements
// without an explicit initializer must be default-initialized.  This also
// covers elements whose default constructor is constexpr (the remaining
// elements are then initialized with a constant, repeated a number of times
// known only at run time).

int constructed;
int destroyed;
int errors;

struct Element {
  int value;
  int live;
  Element() : value(-1), live(1) { ++constructed; }
  Element(int v) : value(v), live(1) { ++constructed; }
  ~Element() {
    if (live != 1) ++errors;
    live = 0;
    ++destroyed;
  }
};

struct Pair {
  Element first;
  Element second;
};

struct Folded {
  int value;
  constexpr Folded() : value(-1) {}
  constexpr Folded(int v) : value(v) {}
  ~Folded() { ++destroyed; }
};

struct Holder {
  Folded folded;
  int extra = 3;
};

static void expect(int value, int wanted) {
  if (value != wanted) ++errors;
}

static void finish(int elements) {
  expect(constructed, elements);
  expect(destroyed, elements);
  constructed = destroyed = 0;
}

void rows_variable(int n) {
  {
    Element a[n][3] = {{Element(1)}, {Element(2), Element(3)}};
    for (int i = 0; i != n; ++i) {
      for (int j = 0; j != 3; ++j) {
        int wanted = -1;
        if (i == 0 && j == 0) wanted = 1;
        if (i == 1 && j == 0) wanted = 2;
        if (i == 1 && j == 1) wanted = 3;
        expect(a[i][j].value, wanted);
      }
    }
  }
  finish(n * 3);
}

void columns_variable(int n) {
  {
    Element a[3][n] = {{Element(1)}, {Element(2), Element(3)}};
    for (int i = 0; i != 3; ++i) {
      for (int j = 0; j != n; ++j) {
        int wanted = -1;
        if (i == 0 && j == 0) wanted = 1;
        if (i == 1 && j == 0) wanted = 2;
        if (i == 1 && j == 1) wanted = 3;
        expect(a[i][j].value, wanted);
      }
    }
  }
  finish(3 * n);
}

void both_variable(int n, int m) {
  {
    Element a[n][m] = {{Element(1)}};
    for (int i = 0; i != n; ++i) {
      for (int j = 0; j != m; ++j) {
        expect(a[i][j].value, i == 0 && j == 0 ? 1 : -1);
      }
    }
  }
  finish(n * m);
}

void middle_variable(int n) {
  {
    Element a[2][n][3] = {{{Element(1)}}, {{}, {Element(2)}}};
    for (int i = 0; i != 2; ++i) {
      for (int j = 0; j != n; ++j) {
        for (int k = 0; k != 3; ++k) {
          int wanted = -1;
          if (i == 0 && j == 0 && k == 0) wanted = 1;
          if (i == 1 && j == 1 && k == 0) wanted = 2;
          expect(a[i][j][k].value, wanted);
        }
      }
    }
  }
  finish(2 * n * 3);
}

void innermost_variable(int n) {
  {
    Element a[2][3][n] = {{{Element(1)}}};
    for (int i = 0; i != 2; ++i) {
      for (int j = 0; j != 3; ++j) {
        for (int k = 0; k != n; ++k) {
          expect(a[i][j][k].value, i == 0 && j == 0 && k == 0 ? 1 : -1);
        }
      }
    }
  }
  finish(2 * 3 * n);
}

void aggregate_elements(int n, int m) {
  {
    Pair a[n][2][m] = {{{{Element(1)}}}, {{}, {{Element(2)}}}};
    for (int i = 0; i != n; ++i) {
      for (int j = 0; j != 2; ++j) {
        for (int k = 0; k != m; ++k) {
          int wanted = -1;
          if (i == 0 && j == 0 && k == 0) wanted = 1;
          if (i == 1 && j == 1 && k == 0) wanted = 2;
          expect(a[i][j][k].first.value, wanted);
          expect(a[i][j][k].second.value, -1);
        }
      }
    }
  }
  finish(n * 2 * m * 2);
}

void empty_initializer(int n, int m) {
  {
    Element a[n][m] = {};
    for (int i = 0; i != n; ++i) {
      for (int j = 0; j != m; ++j) {
        expect(a[i][j].value, -1);
      }
    }
  }
  finish(n * m);
}

void constexpr_elements(int n) {
  {
    Folded a[n] = {Folded(1)};
    for (int i = 0; i != n; ++i) expect(a[i].value, i == 0 ? 1 : -1);
  }
  expect(destroyed, n);
  destroyed = 0;
  {
    Folded a[n][3] = {{Folded(1)}};
    for (int i = 0; i != n; ++i) {
      for (int j = 0; j != 3; ++j) {
        expect(a[i][j].value, i == 0 && j == 0 ? 1 : -1);
      }
    }
  }
  expect(destroyed, n * 3);
  destroyed = 0;
  {
    Folded a[3][n] = {{Folded(1)}, {Folded(2)}};
    for (int i = 0; i != 3; ++i) {
      for (int j = 0; j != n; ++j) {
        int wanted = -1;
        if (i == 0 && j == 0) wanted = 1;
        if (i == 1 && j == 0) wanted = 2;
        expect(a[i][j].value, wanted);
      }
    }
  }
  expect(destroyed, 3 * n);
  destroyed = 0;
}

void constexpr_members(int n) {
  {
    Holder a[3][n] = {{Holder()}};
    for (int i = 0; i != 3; ++i) {
      for (int j = 0; j != n; ++j) {
        expect(a[i][j].folded.value, -1);
        expect(a[i][j].extra, 3);
      }
    }
  }
  expect(destroyed, 3 * n);
  destroyed = 0;
  {
    Holder a[2][n][2] = {{{Holder()}}};
    for (int i = 0; i != 2; ++i) {
      for (int j = 0; j != n; ++j) {
        for (int k = 0; k != 2; ++k) {
          expect(a[i][j][k].folded.value, -1);
          expect(a[i][j][k].extra, 3);
        }
      }
    }
  }
  expect(destroyed, 2 * n * 2);
  destroyed = 0;
}

void no_initializer(int n) {
  // Default initialization of arrays with rows of variable length: the
  // number of elements known at compile time covers only the constant
  // dimensions.
  {
    Element a[2][n];
    expect(a[1][n - 1].value, -1);
  }
  finish(2 * n);
  {
    Element a[2][n][2];
    expect(a[1][n - 1][1].value, -1);
  }
  finish(2 * n * 2);
  {
    Element a[2][2][n];
    expect(a[1][1][n - 1].value, -1);
  }
  finish(2 * 2 * n);
  {
    Element a[2][n][n];
    expect(a[1][n - 1][n - 1].value, -1);
  }
  finish(2 * n * n);
  {
    typedef Element Row[n];
    Row a[3];
    expect(a[2][n - 1].value, -1);
  }
  finish(3 * n);
}

int main() {
  rows_variable(4);
  columns_variable(4);
  both_variable(3, 2);
  both_variable(1, 1);
  middle_variable(3);
  innermost_variable(2);
  aggregate_elements(3, 2);
  empty_initializer(3, 2);
  empty_initializer(0, 2);
  empty_initializer(2, 0);
  constexpr_elements(4);
  constexpr_elements(1);
  constexpr_members(3);
  no_initializer(3);
  no_initializer(1);
  return errors;
}
