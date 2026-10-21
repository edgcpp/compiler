//type:fp
//options_all:--clang --clang_version=210000
//options:--c++20:--c++23
//remark:[GH #338] A lambda in the condition of a Clang enable_if attribute of a function template that names a template parameter
// 10/16/26  [GH #338]
//
// The condition of Clang's enable_if attribute of a function template is
// substituted for the template parameters when the template is a candidate of
// a call.  A lambda in the condition is rescanned then, and the names of the
// template parameters that it uses were not found, "identifier "T" is
// undefined" (see gh-338-lambda-template-param).  The rescan is done in the
// instantiation scope of the template now, as the substitution into the
// signature is, and the names are found.
//
// The two overloads are selected by the size of the type: the condition is
// evaluated with the arguments of the right instance (a call of each
// overload, repeated, and the other overload is not a candidate).

template <class T>
  __attribute__((enable_if(sizeof([]{ return T{}; }()) == sizeof(char), "")))
int pick(T) { return 1; }
template <class T>
  __attribute__((enable_if(sizeof([](T v){ return v; }(T{})) == sizeof(int),
                           "")))
int pick(T) { return 2; }

int t1() { return pick('a') + pick(1) + pick('b') + pick(2); }

// The condition names a private member of the class (a member template that
// is declared in the class): the lambda is rescanned in the access context of
// the template.
class C {
  static constexpr int priv = 1;
public:
  template <class T>
    __attribute__((enable_if(sizeof([]{ return priv + T{}; }()) > 0, "")))
  static int f(T) { return 1; }
};
int t2() { return C::f(1) + C::f('a'); }
