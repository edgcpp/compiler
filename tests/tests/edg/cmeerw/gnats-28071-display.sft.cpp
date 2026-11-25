//type:fp
//options:--c++11:--c++11 --gn 140200
//options_all:--no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

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

/* @filter
file-scope using-decl
  entity, position.seq, is_class_member, is_inheriting_ctor,
  qualifier.class_type
 */
