//type:fp
//use_system_includes: true
//edg_header_pack: exp_meta
//require:DO_IL_LOWERING 1

/*
std::meta::parent_of accepts a reflection of a namespace or namespace alias
other than the global namespace ([meta.reflection.scope]); the result is the
enclosing namespace.  has_parent is true for them and false for ::.
*/

#include <experimental/meta>

using namespace std::meta;

namespace outer {
  namespace inner { struct S {}; }
  inline namespace v1 { int x; }
  namespace { int y; }
  namespace alias2 = inner;
}
namespace outer::inner { void f(); }
namespace alias1 = outer::inner;

static_assert(parent_of(^^outer::inner::S) == ^^outer::inner);
static_assert(parent_of(^^outer::inner::f) == ^^outer::inner);
static_assert(parent_of(^^outer::inner) == ^^outer);
static_assert(parent_of(^^outer) == ^^::);
static_assert(parent_of(^^outer::v1) == ^^outer);
static_assert(parent_of(^^outer::x) == ^^outer::v1);
static_assert(parent_of(parent_of(^^outer::y)) == ^^outer);
static_assert(parent_of(^^alias1) == ^^::);
static_assert(parent_of(^^outer::alias2) == ^^outer);
static_assert(is_namespace(parent_of(^^outer::inner)));
static_assert(identifier_of(parent_of(^^outer::inner)) == "outer");
static_assert(has_parent(^^outer) && has_parent(^^alias1));
static_assert(!has_parent(^^::));

// Walking out to the global namespace.
consteval int depth(info r) {
  int n = 0;
  for (; r != ^^::; r = parent_of(r)) ++n;
  return n;
}
static_assert(depth(^^outer::inner::S) == 3);
