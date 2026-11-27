//type:fp
//options:--c++17:--c++20 --gn 140200:--c++20 --clang_version 190100:--ms_c++20 --microsoft_version 1942
//options_all:--no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

namespace minimal
{
  template<typename T> T v = 0;
}

static const int non_dpdt_int = 1;
static int (*non_dpdt_fp)() = 0;

template<typename T>
static const T dpdt_var = 1;

template<typename T>
static const T dpdt_var<T *> = 2;

template<typename T>
static T dpdt_var_no_init;

template<typename T>
static T dpdt_var_no_init<T *>;

struct C
{
  template<typename T>
  inline static const T ic_dpdt_var = 1;

  template<typename T>
  static const T ooc_dpdt_var;
};

template<typename T>
const T C::ooc_dpdt_var = 2;

template<typename T>
struct D
{
  inline static const T ic_dpdt_var = 3;

  static const T ooc_dpdt_var;
};

template<typename T>
const T D<T>::ooc_dpdt_var = 4;

int i = dpdt_var<int> + dpdt_var<short> +
        dpdt_var<int *> + dpdt_var<short *> +
        dpdt_var_no_init<int> + dpdt_var_no_init<short> +
        dpdt_var_no_init<int *> + dpdt_var_no_init<short *> +
        C::ic_dpdt_var<int> + C::ic_dpdt_var<short> +
        C::ooc_dpdt_var<int> + C::ooc_dpdt_var<short> +
        D<int>::ic_dpdt_var + D<short>::ic_dpdt_var +
        D<int>::ooc_dpdt_var + D<short>::ooc_dpdt_var;

/* @filter
file-scope variable
  source_corresp/name, source_corresp/decl_position.seq,
  source_corresp/decl_position.column, source_corresp/identifier_range,
  source_corresp/specifiers_range, source_corresp/declarator_range, type
 */
