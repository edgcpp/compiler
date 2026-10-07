//type:rp
//use_system_includes: true
//edg_header_pack: exp_meta

/*
A class member designated by a splice-expression is accessible from any
point ([class.access.base]), and the additional access check for protected
members ([class.protected], CWG 3109) does not apply to it.  So a pointer to a
protected member can be formed with &[:r:], and a protected member can be
named with obj.[:r:] or p->[:r:] where obj or *p is of the designating class
or of any class derived from it, outside of any member or friend of the class.
The same holds when the splice is template-dependent.

Output should be:
$ ./a.out
1 2 3 4 5
6 7
8 9 10
11 12
*/

#include <experimental/meta>
#include <stdio.h>
#include <string_view>

using namespace std::meta;

consteval info member_named(info cls, std::string_view name) {
  for (info m : members_of(cls, access_context::unchecked())) {
    if (has_identifier(m) && identifier_of(m) == name) return m;
  }
  return info{};
}

class C {
protected:
  int prot = 1;
  int get() const { return 2; }
  void set(int v) noexcept { prot = v; }
private:
  int priv = 6;
};

struct D : C {};

// Template-dependent splices.
template <info M> constexpr auto member_pointer = &[:M:];

template <info M, typename T> int read(T *p) {
  return p->[:M:];
}

template <info M, typename T> int call(T &obj) {
  return obj.[:M:]();
}

// The pattern of a CRTP base that reaches the non-public members of the
// class derived from it.
template <typename Derived> struct Base {
  int sum() {
    Derived &d = static_cast<Derived &>(*this);
    constexpr auto pm = &[:member_named(^^Derived, "value"):];
    constexpr auto pf = &[:member_named(^^Derived, "twice"):];
    return d.*pm + (d.*pf)();
  }
};

class Widget : public Base<Widget> {
  friend struct Base<Widget>;  // For the static_cast only.
protected:
  int value = 4;
private:
  int twice() const { return 2 * value; }
};

int main() {
  constexpr info prot = member_named(^^C, "prot");
  constexpr info get = member_named(^^C, "get");
  constexpr info set = member_named(^^C, "set");
  constexpr info priv = member_named(^^C, "priv");
  C c;
  D d;
  C *pc = &c;

  int C::*pm = &[:prot:];
  int (C::*pf)() const = &[:get:];
  void (C::*ps)(int) noexcept = &[:set:];
  int a = c.*pm;
  int b = (c.*pf)();
  int e = c.[:get:]() + 1;
  (c.*ps)(4);
  int f = pc->[:prot:];
  d.[:set:](5);
  int g = d.[:prot:];
  printf("%d %d %d %d %d\n", a, b, e, f, g);

  printf("%d %d\n", c.*member_pointer<priv>,
         d.*member_pointer<prot> + 2);

  printf("%d %d %d\n", read<prot>(&d) + 3, call<get>(d) + 7,
         read<priv>(&c) + 4);

  Widget w;
  printf("%d %d\n", w.sum() - 1,
         w.*member_pointer<member_named(^^Widget, "value")> + 8);
  return 0;
}
