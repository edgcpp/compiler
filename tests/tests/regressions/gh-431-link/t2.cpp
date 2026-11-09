template<int N>
struct C {
  virtual int f() { return N; }
};

template struct C<1>;
