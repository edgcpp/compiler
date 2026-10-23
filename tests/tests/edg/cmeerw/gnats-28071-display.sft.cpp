//type:fp
//options:--c++11:--c++11 --gn 140200
//options_all:--no_il_lower --il_display
//filter:awk '/^file-scope (using-decl)@/{f=1}/^$/{if (f) print $0; f=0}f' | grep -E -e '^(entity|position[.]seq|is_class_member|is_inheriting_ctor|qualifier[.]class_type):' -e '^file-scope ' -e '^$' | sed -e 's/@[0-9a-f]*//g'

namespace maybe_dpdt_base_class
{
  struct Y { };
  template<typename> using A = Y;

  template<typename T>
  struct D : A<T>
  {
    using B = A<T>;
    using B::B;
  };
}

namespace dpdt_base_class
{
  template<typename>
  struct A
  { };

  template<typename T>
  struct D : A<T>
  {
    using B = A<T>;
    using B::B;
  };
}
