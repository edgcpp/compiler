//type:fp
//options_all:--c++14 --clang --il --no_il_lowering
//filter:sed -e 's/name: [^"]*/name: /' -e 's/@[0-9a-f]*//' | grep -E '^(initializer_in_class:|is_template_variable:|is_specialized:|  name: "t?var)'
// | sed -e 's|:[^"]+"|: "|'
struct C
{
  template<int J> static const int tvar1 = 1;
  template<int J> static const int tvar2 = 2;
  template<int J> static const int tvar3 = 3;
  template<int J> static const int tvar4;
  template<int J> static const int tvar5;
  template<int J> static const int tvar6;

  template<> static const int tvar1<0>;
  template<> static const int tvar2<0> = 20;
  template<> static const int tvar3<0> = 30;

  template<> static const int tvar4<0>;
  template<> static const int tvar5<0> = 50;
  template<> static const int tvar6<0> = 60;
};

template<> const int C::tvar1<0> = 10;
template<> const int C::tvar2<0>;

template<> const int C::tvar4<0> = 40;
template<> const int C::tvar5<0>;


template<int J>
struct D
{
  static const int var1 = 1;
  static const int var2 = 2;
  static const int var3 = 3;
  static const int var4;
  static const int var5;
  static const int var6;
};

template<> const int D<0>::var1 = 10;
template<> const int D<0>::var2;

template<> const int D<0>::var4 = 40;
template<> const int D<0>::var5;

template struct D<0>;
