//type:fp
//options_all:-w -tused --no_il_lower --il_display
//options:--c++14:--c++14 --gn 140200:--c++14 --clang_version 200100:--ms_c++20 --microsoft_version 1942
//filter:awk '/^file-scope (variable|template)@/{f=1}/^$/{if (f) print $0; f=0}f' | grep -E -e '^(  name|  decl_position[.]seq|  referenced|  needed|  name_linkage|storage_class|init_kind|constant):' -e '^file-scope ' -e '^$' | sed -e 's/@[0-9a-f]*//g'

namespace set_only
{
  template<int I> int var = I;

  void f()
  {
    var<1> = 1;
  }
}

namespace multiple_uses
{
  template<int I> int var = I;

  void f()
  {
    var<1> = 1;
    (var<2> = 2);
    var<3> + 1;
    var<4> += 1;
    &var<5>;
    sizeof(var<6>);
    alignof(var<7>);
    decltype(var<8>)();
  }
}

namespace static_data_member
{
  struct C
  {
    template<int I> static int var;
  };

  template<int I> int C::var = I;

  void f()
  {
    C::var<1> = 1;
    (C::var<2> = 2);
    C::var<3> + 1;
    C::var<4> += 1;
    &C::var<5>;
    sizeof(C::var<6>);
    alignof(C::var<7>);
    decltype(C::var<8>)();
  }
}
