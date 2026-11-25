//type:fp
//options:--clang_version 190100 --target linux_i686:--clang_version 190100 --target linux_x86_64:--clang_version 190100 --target linux_armv7:--clang_version 190100 --target linux_aarch64:--gn 140200 --target linux_i686:--gn 140200 --target linux_x86_64:--gn 140200 --target linux_armv7:--gn 140200 --target linux_aarch64
//options_all:-w --c++17 --il_display
//filter:edg-il-filter

void f(__builtin_va_list) { }

/* @filter
func-scope variable
  source_corresp/name, source_corresp/enclosing_routine, type
 */
