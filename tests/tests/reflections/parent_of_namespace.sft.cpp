//type:fp
//use_system_includes: true
//edg_header_pack: meta
//require:DO_IL_LOWERING 1

/*
std::meta::parent_of accepts a reflection of a namespace or namespace alias
other than the global namespace (N5046 [meta.reflection.queries]); the result
is the enclosing namespace.  has_parent is true for them and false for ::.
*/

#include <meta>

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

// Predeclared std::meta namespace.
static_assert(parent_of(^^std::meta) == ^^std);
static_assert(parent_of(^^std) == ^^::);

// Function-local entities and structured bindings.
struct Pair { int a, b; };
void local_fn() {
  [[maybe_unused]] int lv = 0;
  struct LocalS { int m; };
  using LocalAlias = int;
  enum LocalE { le };
  Pair p{1, 2};
  [[maybe_unused]] auto [sb1, sb2] = p;
  extern int block_ext_var;
  int block_ext_fn();

  static_assert(has_parent(^^lv));
  static_assert(parent_of(^^lv) == ^^local_fn);
  static_assert(parent_of(^^LocalS) == ^^local_fn);
  static_assert(parent_of(^^LocalS::m) == ^^LocalS);
  static_assert(parent_of(^^LocalAlias) == ^^local_fn);
  static_assert(parent_of(^^LocalE) == ^^local_fn);
  static_assert(parent_of(^^le) == ^^LocalE);
  static_assert(!has_parent(^^sb1));
  static_assert(parent_of(^^block_ext_var) == ^^::);
  static_assert(parent_of(^^block_ext_fn) == ^^::);
}
