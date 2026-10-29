//type:rp
//options:--c++11 -tused t2.cpp
//source_files:t2.cpp

template<int N>
struct C {
  virtual int f() { return N; }
};

extern template struct C<1>;

C<1> c;
C<1> *p = &c;

int main()
{
  return p->f() != 1;
}
