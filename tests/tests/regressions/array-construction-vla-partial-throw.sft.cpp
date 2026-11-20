//type:rp
//options_all:--c++11 --g++ --exceptions

// When a default constructor throws while a partially initialized
// multidimensional GNU variable-length array is being constructed, exactly
// the elements already constructed must be destroyed.

int constructed;
int destroyed;
int errors;
int throw_at;

struct Element {
  int live;
  Element() : live(1) {
    if (++constructed == throw_at) throw 1;
  }
  Element(int) : live(1) { ++constructed; }
  ~Element() {
    if (live != 1) ++errors;
    live = 0;
    ++destroyed;
  }
};

static void touch(void *) {}

void rows_variable(int n) {
  Element a[n][3] = {{Element(1)}};
  touch(a);
}

void columns_variable(int n) {
  Element a[3][n] = {{Element(1)}, {}};
  touch(a);
}

void both_variable(int n, int m) {
  Element a[n][2][m] = {{{Element(1)}}};
  touch(a);
}

static void run(void (*construct)(int, int), int n, int m, int elements) {
  for (throw_at = 2; throw_at <= elements; ++throw_at) {
    constructed = destroyed = 0;
    try {
      construct(n, m);
      ++errors;
    } catch (int) {
    }
    if (constructed != throw_at || destroyed != throw_at - 1) ++errors;
  }
}

static void rows(int n, int) { rows_variable(n); }
static void columns(int n, int) { columns_variable(n); }
static void both(int n, int m) { both_variable(n, m); }

int main() {
  run(rows, 3, 0, 9);
  run(columns, 3, 0, 9);
  run(both, 2, 3, 12);
  return errors;
}
