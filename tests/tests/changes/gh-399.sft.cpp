//type:fp
//options:--c++23
//remark:Default-initialized local static constexpr in constant evaluation

struct Q { int q = 1; };
constexpr int default_init() { static constexpr Q q; return q.q; }
static_assert(default_init() == 1);  // Previously failed.
constexpr int value_init() { static constexpr Q q{}; return q.q + 1; }
static_assert(value_init() == 2);

struct R { int r = 3; constexpr R() {} };
constexpr int user_ctor() {
  static constexpr R r;
  constexpr const R* p = &r;
  return p->r;
}
static_assert(user_ctor() == 3);  // Previously failed.

template<typename T> constexpr int templ() {
  static constexpr T t;
  return t.q * 10;
}
static_assert(templ<Q>() == 10);  // Previously failed.
