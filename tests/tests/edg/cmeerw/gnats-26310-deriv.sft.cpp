//type:fp
//options:--c++11
//options_all:--no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

struct VB
{
  static constexpr int value = 0;
};

namespace indirect_public_direct_protected
{
  struct B : public virtual VB
  { };

  struct D : public B, protected virtual VB
  { };

  static_assert(D::value == 0, "");
}

namespace indirect_protected_direct_public
{
  struct B : protected virtual VB
  { };

  struct D : public B, public virtual VB
  { };

  static_assert(D::value == 0, "");
}

namespace direct_protected_indirect_public
{
  struct B : public virtual VB
  { };

  struct D : protected virtual VB, public B
  { };

  static_assert(D::value == 0, "");
}

namespace direct_public_indirect_protected
{
  struct B : protected virtual VB
  { };

  struct D : public virtual VB, public B
  { };

  static_assert(D::value == 0, "");
}

namespace direct_protected_indirect_protected
{
  struct B : protected virtual VB
  { };

  struct D : protected virtual VB, public B
  { };
}

namespace indirect_protected_direct_protected
{
  struct B : protected virtual VB
  { };

  struct D : public B, protected virtual VB
  { };
}

namespace protected_diamond
{
  struct VB
  {
    static constexpr int value = 0;
  };

  struct B1 : protected virtual VB
  { };

  struct B2 : public virtual VB
  { };

  struct D : public B1, protected B2
  { };
}

/* @filter
file-scope base-class
  type, derived_class, direct, is_virtual, ambiguous, has_public_derivation
 */
