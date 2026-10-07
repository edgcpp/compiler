//type:fn
//use_system_includes: true
//edg_header_pack: exp_meta

/*
A member designated by a splice-expression is accessible from any point, but
a class member access whose right operand is such a splice is still
ill-formed if the left operand (considered as a pointer) cannot be implicitly
converted to a pointer to the designating class ([class.access.base]).  A
reflect-expression naming a member is subject to the usual access checks.
*/

#include <experimental/meta>
#include <string_view>

using namespace std::meta;

consteval info member_named(info cls, std::string_view name) {
  for (info m : members_of(cls, access_context::unchecked())) {
    if (has_identifier(m) && identifier_of(m) == name) return m;
  }
  return info{};
}

struct C {
  int pub = 1;
protected:
  int prot = 2;
};

struct E : private C {};
struct F : protected C {};

int main() {
  E e;
  F f;
  int a = e.[:^^C::pub:];                       // error: inaccessible base
  int b = f.[:member_named(^^C, "prot"):];      // error: inaccessible base
  int c = (&f)->[:member_named(^^C, "pub"):];   // error: inaccessible base
  static_assert(^^C::prot != info{});           // error: inaccessible
  return a + b + c;
}
