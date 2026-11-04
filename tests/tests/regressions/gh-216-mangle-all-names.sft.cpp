//type: cp
//options: --c++26
//requires:MANGLE_ALL_NAMES=1
//remark:[GH #216] Prototype variable mangling without IL lowering.

namespace N {
  template<typename T> int v = 0;

  template<typename T> struct S {
    template<typename U> static int w;
  };

  template<typename T> template<typename U> int S<T>::w = 0;

  template<typename T> int f()
  {
    return v<T> + S<T>::template w<T>;
  }

  template int f<int>();
}  /* namespace N */
