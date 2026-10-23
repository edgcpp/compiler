//type:fp
//options:--c++20 --no_exceptions --no_il_lower --il_display
//filter:awk '/^func-scope (dynamic-init|expr-node)@/{f=1}/^$/{if (f) print $0; f=0}f' | grep -E -e '^(constant|kind|non_constant|variable|  (start|end).[^:]*|position.[^:]*|has_[^:]*|is_[^:]*|new_[^:]*):' -e '^ *(FALSE|TRUE)$' -e '^func-scope ' -e '^$' | sed -e 's/@[0-9a-f]*//'

struct A
{
  int i;
  int j;
};

struct C
{
  C(int, int);
};

extern A a;
extern C c;
extern int i;

void f()
{
  new A(a);
  new A{a};
  new A(1, i);
  new A{2, i};

  new C(c);
  new C{c};
  new C(3, i);
  new C{4, i};
}
