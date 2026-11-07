//type:fp
//options_all:--c --c23 --strict
//remark:C: impl. ptr conv. adding qualifiers at the first level only

void add_first_level_const(int **p)
{
  int *const* q = p;

  (void)q;
}

void explicit_deep_qualification(int **p)
{
  const int *const* q = (const int *const* )p;

  (void)q;
}
