//type:fp
//options_all:-w -tused --no_il_lower --il_display
//options:--c++14:--c++14 --gn 140200:--c++14 --clang_version 200100:--ms_c++20 --microsoft_version 1942
//filter:edg-il-filter
//script:edg-first-stage-il

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

/* @filter
file-scope variable
  source_corresp/name, source_corresp/decl_position.seq,
  source_corresp/referenced, source_corresp/needed,
  source_corresp/name_linkage, storage_class, init_kind, constant
file-scope template
  source_corresp/name, source_corresp/decl_position.seq,
  source_corresp/referenced, source_corresp/needed,
  source_corresp/name_linkage
 */
