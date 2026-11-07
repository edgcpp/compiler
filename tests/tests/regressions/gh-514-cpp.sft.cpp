//type:fp
//options_all:--c++20 --strict
//remark:C++: impl. ptr conv. w/ qualifiers below first level

// The const-safe conversions that need a diagnostic in C are standard
// qualification conversions in C++.

void take(const int *const *);
void take3(const int *const *const *);

void add_deep_const(int **p, int ***p3)
{
  const int *const *q = p;

  take(q);
  take(p);
  take3(p3);
}
