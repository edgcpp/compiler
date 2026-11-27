//type:fp
//options:--c++20
//options_all:--no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

struct B1
{ };

template<unsigned>
struct B2
{ };

template<typename ... T>
struct C : B1, B2<sizeof ... (T)>
{
  C(T ...) : B1{}, B2<0 + sizeof ... (T)>{}
  { }
};

/* @filter
file-scope base-class
  type, derived_class, direct
func-scope constructor-init
  base_class
 */
