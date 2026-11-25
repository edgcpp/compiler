//type:fp
//options:--c++20 --no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

enum E
{
  E1, E2, E3
};

struct B
{
  using E::E1;
  static constexpr int v1 = 1;
  static constexpr int v2 = 1;
  static constexpr int v3 = 1;

protected:
  using E::E2;

private:
  using E::E3;
};

struct D : B
{
  using B::E1;
  using B::v1;

protected:
  using B::E2;
  using B::v2;

private:
  using B::v3;
};

/* @filter
file-scope using-decl
  entity, qualifier.class_type, access, is_representative, is_class_member,
  is_enumerator, is_using_enum, is_using_directive
 */
