//type:fp
//options_all:--c --c23
//options:--no_strict_gnu --gnu_version=130300:--strict_warnings
//remark:C: impl. ptr conv. w/ qualifiers below first level

// C++ permits these conversions, but in C the types pointed to are not
// compatible, so each one requires a diagnostic.

void take(const int *const *);
void take3(const int *const *const *);

void assignment(int **p)
{
  const int *const *q;

  q = p;
  take(q);
}

void initialization(int **p)
{
  const int *const *q = p;

  take(q);
}

void argument(int **p)
{
  take(p);
}

const int *const *return_value(int **p)
{
  return p;
}

void three_levels(int ***p)
{
  const int *const *const *q = p;

  take3(q);
}
