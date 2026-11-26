//type:fp
//options_all:--g++ --gnu_version=30406
// GNU C++ before 4.0 accepted a cast of a pointer to an integer type that is
// too small to hold it, with a warning.  That leniency is kept when emulating
// those versions.
int main()
{
  int *ptr = 0;
  int x;

  x = (int)ptr;
  x = (short)ptr;

  return x;
}
