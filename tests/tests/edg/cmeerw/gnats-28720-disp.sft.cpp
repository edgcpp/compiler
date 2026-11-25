//type:fp
//options:--c++20 --clang_version 220100
//options_all:--no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

template<typename ... Ts>
int f_pack();

template<typename T1, typename T2, typename ... Ts>
int f_nonpack();

template<typename ... Ts>
extern int v_pack;

template<typename T1, typename T2, typename ... Ts>
extern int v_nonpack;

template<typename ... Ts>
int v1 = f_pack<__builtin_dedup_pack<Ts ...>...>() +
         f_nonpack<__builtin_dedup_pack<Ts ...>...>();

template<typename ... Ts>
int v2 = v_pack<__builtin_dedup_pack<Ts ...>...> +
         v_nonpack<__builtin_dedup_pack<Ts ...>...>;

template<int>
struct B { };

template<typename ... Ts>
struct D : __builtin_dedup_pack<Ts ...>... { };

using INT = int;

int i = v1<int, INT, const INT, const int, long> +
        v2<int, INT, const INT, const int, long>;

D<B<1>, B<1>, B<2>, B<2>> d;

/* @filter
file-scope base-class-derivation
  direct, path, access
file-scope base-class
  type, derived_class, direct, direct_base_number, is_virtual, derivation
file-scope routine[source_corresp/decl_position.seq]
  source_corresp/name, type, storage_class, template_arg_list
file-scope variable
  source_corresp/name, type, storage_class, template_info, is_pack,
  is_pack_element
file-scope variable-template-info
  template_arg_list
 */
