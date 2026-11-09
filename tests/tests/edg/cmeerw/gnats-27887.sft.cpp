//type:fp
//options:--c++17:--c++20 --gn 140200:--c++20 --clang_version 190100:--ms_c++20 --microsoft_version 1942
//options_all:--no_il_lower --il_display
//filter:awk '/^file-scope variable@/{f=1}/^$/{if (f) print $0; f=0}f' | grep -E -e '^(type|  name|  [a-z]*_position.seq|  [a-z]*_position.column|    start.seq|    start.column|    end.seq|    end.column):' -e '^  [a-z]*_range$' -e '^file-scope ' -e '^$' | sed -e 's/@[0-9a-f]*//g'

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
