//type:fp
//options:--c++11
//options_all:--no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

struct VB { };
struct V : VB { };
struct B : virtual V { };
struct C1 { };
struct C2 { };
struct D : virtual B, C1, virtual V, C2 { };

/* @filter
file-scope base-class
  type, derived_class, next, next_direct, next_preorder, direct,
  direct_base_number, is_virtual
file-scope class-type-supplement
  base_classes, direct_base_classes, preorder_base_classes
 */
