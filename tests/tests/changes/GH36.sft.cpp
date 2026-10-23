//type:fp
//options:--c++26 --set_flag reflection
//use_system_includes: true
//edg_header_pack: meta
//require:TARG_SUPPORTS_X86_64 1
//remark:[GH #36] <meta> defines __cpp_lib_reflection
// 10/22/26 [GH #36]
//
// The EDG-provided <meta> header defines the library feature-test macro
// __cpp_lib_reflection, with the value GCC 16 uses.  The GCC system headers
// that <meta> includes are not accepted for 32-bit targets, so the test
// requires an x86-64 target.

#include <meta>

static_assert(__cpp_lib_reflection == 202603L, "");
static_assert(__cpp_lib_reflection == __cpp_impl_reflection, "");
