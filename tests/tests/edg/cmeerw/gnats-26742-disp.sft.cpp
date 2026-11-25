//type:fp
//options:--c++20 --no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

// check that is_pack and is_pack_element flags on the template argument lists
// are set correctly

namespace pack_expansions
{
  template<typename T>
  concept C1 = true;

  template<typename T, typename U>
  concept C2 = true;

  template<auto>
  struct DN
  { };

  template<typename ... Ts>
  struct B
  {
    template<Ts ... X>
    static int f_non_type(DN<X> ...);

    template<C2<Ts> ... XT>
    static int f_type(XT ...);

    template<C1 ... XT>
    static int f_type_pack(XT ...);
  };

  auto v1 = B<int, char>::f_non_type<1, 'a'>(DN<1>(), DN<'a'>());
  auto v2 = B<int, char>::f_type<int>(1, 'a');
  auto v3 = B<int, char>::f_type_pack<int>(1, 'a');
}

/* @filter
file-scope routine[template_arg_list]
  source_corresp/name, template_arg_list
 */
