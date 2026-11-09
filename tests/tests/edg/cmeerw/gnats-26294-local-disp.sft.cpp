//type:fp
//options:--c++20
//options_all:-w --no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^(file|func)-scope (expr-node@|constant@|name-qualifier@|type@.*\nkind: +(tk_struct|name_qualifier))/' | awk '/^[a-z_]+:$/ { getline n; if (n ~ /^   /) { gsub(/  +/, "", n); print $0 " " n; } else { print $0; print n; } next; }1' | grep -E -e '^(  name|  decl_position[.]seq|position[.]seq|is_class|qualifier|qualifier[.]class_type|extra_info|constant|routine|kind|operation[.]kind|operands|type|declared_type|typeref_type|is_global_qualified_name):' -e '^ +(FALSE|TRUE)' -e '^(file|func)-scope ' -e '^$' | edg-enumerate-il-addrs

#if TEST_NUMBER == 1

struct A
{
  struct B
  {
    static void g();
    int j;
  };
};

void f()
{
  struct A
  {
    struct B
    {
      static void f()
      { }

      int i;
    };

    void h()
    {
      B::f();
      auto p = &B::i;
    }
  };

  {
    A::B b;
    A::B::f();
    auto p = &A::B::i;
  }

  {
    ::A::B b;
    ::A::B::g();
    auto p = &::A::B::j;
  }
}

#endif
