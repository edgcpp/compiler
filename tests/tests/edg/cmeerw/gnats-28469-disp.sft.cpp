//type:fp
//options:--c++20
//options_all:--no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^func-scope expr-node@/' | grep -E -e '^(next|type|kind|constant|operation[.]kind|operands|position[.]seq):' -e '^func-scope ' -e '^$' | edg-enumerate-il-addrs

using INT1 = int;
using INT2 = int;

static constexpr int i = 0;
static constexpr INT1 i1 = 0;
static constexpr INT2 i2 = 0;

INT1 n1 = 0;
INT2 n2 = 0;

template<int I, INT1 I1, INT2 I2>
void f()
{
  I == 0;
  I1 == 0;
  I2 == 0;
  I1 == I2;                     // should keep typerefs on the constant types

  i1 == i2;
  n1 == n2;

  +I;
  +I1;
  +I2;

  +i1;
  +i2;

  +n1;
  +n2;
}
