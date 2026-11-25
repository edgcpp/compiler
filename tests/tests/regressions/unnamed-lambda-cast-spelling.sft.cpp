//type:cppbe
//options_all:--c++17
//remark:Preserve the written destination type of a reference cast
//filter:sed 's/[[:space:]]*$//'
//match_regex:static_cast<\s*RVINT\s*>\(i\)
//match_regex:static_cast<\s*RR\s*&\s*>\(x\)
//match_regex:static_cast<\s*decltype\(l\)\s*>\(l\)
//match_regex:static_cast<\s*decltype\(l\)\s*&&\s*>\(l\)
//match_regex:static_cast<\s*decltype\(l\)\s*&\s*>\(l\)
//match_regex:static_cast<\s*R\s*&\s*>\(l\)
//match_regex:static_cast<\s*R\s*&&\s*>\(l\)
//match_regex:static_cast<\s*Ref<\s*R\s*>\s*&\s*>\(l\)
//match_regex:const_cast<\s*decltype\(l\)\s*>\(cl\)
//match_regex:const_cast<\s*decltype\(l\)\s*&\s*>\(cl\)
//match_regex:const_cast<\s*R\s*&&\s*>\(cl\)
//match_regex:reinterpret_cast<\s*decltype\(l\)\s*>\(l\)
//match_regex:reinterpret_cast<\s*decltype\(l\)\s*&\s*>\(l\)
//match_regex:reinterpret_cast<\s*R\s*&&\s*>\(l\)
//match_regex:dynamic_cast<\s*decltype\(l\)\s*>\(l\)
//match_regex:dynamic_cast<\s*decltype\(l\)\s*&\s*>\(l\)
//match_regex:dynamic_cast<\s*decltype\(\(d\)\)\s*>\(b\)
//match_regex:dynamic_cast<\s*D\s*&\s*>\(b\)
//match_regex:dynamic_cast<\s*D\s*&&\s*>\(b\)
//match_regex:static_cast\s*<\s*R\s*&&\s*>\s*\(\s*t\s*\)
//match_regex:static_cast\s*<\s*typename X\s*<\s*int\s*>\s*::\s*R\s*&\s*>\s*\(\s*i\s*\)
//match_regex:static_cast<\s*int\s*&\s*>\(get\(\)\)
//match_regex:return static_cast<\s*int\s*&\s*>\(i\);
//match_regex:\(decltype\(l\)\s*&\)l
//match_regex:\(decltype\(l\)\s*&&\)l
//match_regex:\(R\s*&\)l

using RVINT = int&&;
template<class T> using Ref = T&&;

int& through_rvalue_alias(int& x) {
  using RR = int&&;
  // The written & changes the result from int&& to int&.
  return static_cast<RR&>(x);
}

void f(int i) {
  auto&& v1 = static_cast<RVINT>(i);
  auto&& l = [] {};
  auto&& v2 = static_cast<decltype(l)>(l);
  // These casts also exercise reference collapsing.  Keep the decltype
  // and the written reference operator in the generated C++.
  auto&& v3 = static_cast<decltype(l)&&>(l);
  auto& v4 = static_cast<decltype(l)&>(l);
  (void)v1;
  (void)v2;
  (void)v3;
  (void)v4;
}

void through_alias() {
  auto&& l = [] {};
  using R = decltype(l);
  // R& collapses to an lvalue reference.  The generated cast can use R
  // to refer to the closure type while retaining the written &.
  auto& r = static_cast<R&>(l);
  auto&& rr = static_cast<R&&>(l);
  auto& rt = static_cast<Ref<R>&>(l);
  (void)r;
  (void)rr;
  (void)rt;
}

int const_and_reinterpret() {
  auto&& l = [] { return 1; };
  const auto& cl = l;
  using R = decltype(l);
  // Exercise a type operator and reference collapsing for both cast forms.
  auto&& c1 = const_cast<decltype(l)>(cl);
  auto& c2 = const_cast<decltype(l)&>(cl);
  auto&& c3 = const_cast<R&&>(cl);
  auto&& r1 = reinterpret_cast<decltype(l)>(l);
  auto& r2 = reinterpret_cast<decltype(l)&>(l);
  auto&& r3 = reinterpret_cast<R&&>(l);
  return c1() + c2() + c3() + r1() + r2() + r3();
}

int dynamic_same_type() {
  auto&& l = [] { return 1; };
  // A same-type dynamic_cast can refer to the closure via decltype.
  // Calling the results requires the reconstructed type to be the closure.
  return dynamic_cast<decltype(l)>(l)() + dynamic_cast<decltype(l)&>(l)();
}

struct Base { virtual ~Base() = default; };
struct Derived : Base {};

void dynamic(Base& b, Derived& d) {
  using D = decltype((d));
  // D& and D&& both collapse to Derived&.  Preserve the written suffix.
  auto& d1 = dynamic_cast<decltype((d))>(b);
  auto& d2 = dynamic_cast<D&>(b);
  auto& d3 = dynamic_cast<D&&>(b);
  (void)d1;
  (void)d2;
  (void)d3;
}

template<class T> T& dependent_alias(T& t) {
  using R = T&;
  return static_cast<R&&>(t);
}

template int& dependent_alias(int&);

template<class T> using Lvalue = T&;

template<class T> auto rescan_alias(T& t)
    -> decltype(static_cast<Lvalue<T>&&>(t)) {
  return t;
}

int& instantiate_rescanned_alias(int& i) {
  return rescan_alias(i);
}

template<class T> struct X {
  using R = T&;
};

template<class T> int& member_alias(int& i) {
  return static_cast<typename X<int>::R&>(i);
}

struct LateAlias {
  static int& get();
  // The alias is visible here in the source, but the generator has yet to
  // emit it.  The generated cast can use the collapsed type int&.
  static void f(int& = static_cast<R&>(get()));
  using R = int&&;
};

auto make_owner() {
  struct Local { int&& m; };
  static int x;
  return Local{static_cast<int&&>(x)};
}

using Escaped = decltype(make_owner());

int& escaped_operand(int& i) {
  // The member is reachable through Escaped in the source.  The IL's
  // operand refers to the block-local class, so emit the collapsed int&.
  return static_cast<decltype(Escaped::m)&>(i);
}

int c_style_and_functional() {
  auto&& l = [] { return 1; };
  using L = decltype((l));
  using R = decltype(l);
  // Recompiling these calls requires each cast to yield the closure type.
  auto&& c = (decltype(l))l;
  auto& cr = (decltype(l)&)l;
  auto&& rr = (decltype(l)&&)l;
  auto& a = (R&)l;
  // L already denotes the complete reference type for both functional casts.
  auto& p = L(l);
  auto& b = L{l};
  return c() + cr() + rr() + a() + p() + b();
}
