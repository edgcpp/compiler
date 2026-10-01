//type:fp
//options:--c++20
template<typename T> class S
{ };

class C {
private:
  struct N;
  S<N> f();
};

extern template class S<C::N>;
