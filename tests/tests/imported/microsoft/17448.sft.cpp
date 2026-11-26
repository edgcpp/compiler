//type:fp
//options_all:--microsoft_version 1903
struct Base
{
  virtual int f() noexcept { return 37; }
};
struct Derived : Base
{
  int f() { return 47; }
};
