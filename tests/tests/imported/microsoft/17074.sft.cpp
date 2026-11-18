//options_all:--microsoft --c++14
struct A
{
  A(const A&) = default;
};

struct B : A
{
  B(const B& other) : A{other}
  {}
};
