//type:fp
//options:--c++11 --no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^(file-scope name-qualifier@|(file|func)-scope expr-node@.*\nkind: +(enk_routine|enk_variable))/' | grep -E -e '^(kind|type|routine|variable|qualifier|position[.]seq|is_class|qualifier[.]class_type|name|previous_qualifier):' -e '^(file|func)-scope ' -e '^$' | edg-enumerate-il-addrs

struct A
{
  static constexpr bool v = false;
};

template<typename T> struct B
{
  void g(int) noexcept(A::v);
};

struct D : B<bool>
{
  void f()
  {
    g(1);                       // shouldn't have a name-qualifier
  }
};
