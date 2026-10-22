/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

lower_name.c -- Do name mangling for IL lowering.

*/

#include "basic_hdrs.h"
#if NEED_NAME_MANGLING
/* Header files common to all files. */
#include "fe_common.h"
/* Header files used by files involved in IL lowering. */
#include "lower_hdrs.h"
#endif /* NEED_NAME_MANGLING */

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Only include this code if it is needed: */
#if NEED_NAME_MANGLING
#include "il_walk.h"
#include "templates.h"
#include "exprutil.h"
#if GNU_FUNCTION_MULTIVERSIONING
#include "sys_predef.h"
#endif /* GNU_FUNCTION_MULTIVERSIONING */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

#if IA64_ABI
/* IA-64 name mangling codes. */
#define MANGLING_CODE_FOR_CONST 'K'
#define MANGLING_CODE_FOR_VOLATILE 'V'
#define MANGLING_CODE_FOR_RESTRICT 'r'
#define MANGLING_STRING_FOR_ATOMIC "U7_Atomic"
#define MANGLING_CODE_FOR_ELLIPSIS 'z'
#define MANGLING_CODE_FOR_EXTERN_C 'Y'
#define MANGLING_STRING_FOR_NOEXCEPT "Do"
#define MANGLING_STRING_FOR_NOEXCEPT_EXPR "DO"
#define MANGLING_STRING_FOR_VOID "v"
#define MANGLING_STRING_FOR_WCHAR_T "w"
#define MANGLING_STRING_FOR_CHAR8_T "Du"
#define MANGLING_STRING_FOR_CHAR16_T "Ds"
#define MANGLING_STRING_FOR_CHAR32_T "Di"
#define MANGLING_STRING_FOR_BOOL "b"
#define MANGLING_STRING_FOR_CHAR "c"
#define MANGLING_STRING_FOR_SIGNED_CHAR "a"
#define MANGLING_STRING_FOR_UNSIGNED_CHAR "h"
#define MANGLING_STRING_FOR_SHORT "s"
#define MANGLING_STRING_FOR_UNSIGNED_SHORT "t"
#define MANGLING_STRING_FOR_INT "i"
#define MANGLING_STRING_FOR_UNSIGNED_INT "j"
#define MANGLING_STRING_FOR_LONG "l"
#define MANGLING_STRING_FOR_UNSIGNED_LONG "m"
#if LONG_LONG_ALLOWED
#define MANGLING_STRING_FOR_LONG_LONG "x"
#define MANGLING_STRING_FOR_UNSIGNED_LONG_LONG "y"
#endif /* LONG_LONG_ALLOWED */
#if INT128_EXTENSIONS_ALLOWED
#define MANGLING_STRING_FOR_INT128 "n"
#define MANGLING_STRING_FOR_UNSIGNED_INT128 "o"
#endif /* INT128_EXTENSIONS_ALLOWED */
#define MANGLING_STRING_FOR_FLOAT16 "DF16_"
#define MANGLING_STRING_FOR_FP16 "Dh"
#define MANGLING_STRING_FOR_FLOAT "f"
#define MANGLING_STRING_FOR_FLOAT32X "DF32x"
#define MANGLING_STRING_FOR_DOUBLE "d"
#define MANGLING_STRING_FOR_FLOAT64X "DF64x"
#define MANGLING_STRING_FOR_LONG_DOUBLE "e"
#define MANGLING_STRING_FOR_FLOAT80 "u7float80"
#define MANGLING_STRING_FOR_FLOAT128 "g"
#define MANGLING_STRING_FOR_STD_BFLOAT16_X86 "DF16b"
#define MANGLING_STRING_FOR_STD_BFLOAT16_ARM "u6__bf16"
#define MANGLING_STRING_FOR_STD_FLOAT16 MANGLING_STRING_FOR_FLOAT16
#define MANGLING_STRING_FOR_STD_FLOAT32 "DF32_"
#define MANGLING_STRING_FOR_STD_FLOAT64 "DF64_"
#define MANGLING_STRING_FOR_STD_FLOAT128 "DF128_"
#if C99_IL_EXTENSIONS_SUPPORTED
#define MANGLING_STRING_FOR_COMPLEX_FLOAT16 "CDF16_"
#define MANGLING_STRING_FOR_COMPLEX_FLOAT "Cf"
#define MANGLING_STRING_FOR_COMPLEX_DOUBLE "Cd"
#define MANGLING_STRING_FOR_COMPLEX_LONG_DOUBLE "Ce"
#define MANGLING_STRING_FOR_COMPLEX_FLOAT80 "Cu7float80"
#define MANGLING_STRING_FOR_COMPLEX_FLOAT128 "Cg"
#define MANGLING_STRING_FOR_COMPLEX_STD_BFLOAT16_X86 "CDF16b"
#define MANGLING_STRING_FOR_COMPLEX_STD_BFLOAT16_ARM "Cu6__bf16"
#define MANGLING_STRING_FOR_COMPLEX_STD_FLOAT16 \
                                            MANGLING_STRING_FOR_COMPLEX_FLOAT16
#define MANGLING_STRING_FOR_COMPLEX_STD_FLOAT32 "CDF32_"
#define MANGLING_STRING_FOR_COMPLEX_STD_FLOAT64 "CDF64_"
#define MANGLING_STRING_FOR_COMPLEX_STD_FLOAT128 "CDF128_"
#define MANGLING_STRING_FOR_COMPLEX_FLOAT32X "CDF32x"
#define MANGLING_STRING_FOR_COMPLEX_FLOAT64X "CDF64x"
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
#define MANGLING_STRING_FOR_REFERENCE "R"
#define MANGLING_STRING_FOR_RVALUE_REFERENCE "O"
#define MANGLING_STRING_FOR_POINTER "P"
#define MANGLING_STRING_FOR_POINTER_TO_MEMBER "M"
#define MANGLING_STRING_FOR_ARRAY "A"
#if GNU_VECTOR_TYPES_ALLOWED
#define MANGLING_STRING_FOR_VECTOR "U8__vector"
#define MANGLING_STRING_FOR_SCALABLE_VECTOR_COUNT "u11__SVCount_t"
#define MANGLING_STRING_FOR_MFP8 "u6__mfp8"
#define MANGLING_STRING_FOR_FLOAT8E4M3 "u12__float8e4m3"
#define MANGLING_STRING_FOR_FLOAT8E5M2 "u12__float8e5m2"
#endif /* GNU_VECTOR_TYPES_ALLOWED */
#define MANGLING_STRING_FOR_META_INFO "U10__metainfo"
#define MANGLING_STRING_FOR_OPERATOR_NEW "nw"
#define MANGLING_STRING_FOR_OPERATOR_DELETE "dl"
#define MANGLING_STRING_FOR_OPERATOR_ARRAY_NEW "na"
#define MANGLING_STRING_FOR_OPERATOR_ARRAY_DELETE "da"
#define MANGLING_STRING_FOR_OPERATOR_UNARY_PLUS "ps"
#define MANGLING_STRING_FOR_OPERATOR_PLUS "pl"
#define MANGLING_STRING_FOR_OPERATOR_NEGATE "ng"
#define MANGLING_STRING_FOR_OPERATOR_MINUS "mi"
#define MANGLING_STRING_FOR_OPERATOR_MULT "ml"
#define MANGLING_STRING_FOR_OPERATOR_DEREFERENCE "de"
#define MANGLING_STRING_FOR_OPERATOR_DIVIDE "dv"
#define MANGLING_STRING_FOR_OPERATOR_REMAINDER "rm"
#define MANGLING_STRING_FOR_OPERATOR_EXCL_OR "eo"
#define MANGLING_STRING_FOR_OPERATOR_AND "an"
#define MANGLING_STRING_FOR_OPERATOR_ADDRESS "ad"
#define MANGLING_STRING_FOR_OPERATOR_OR "or"
#define MANGLING_STRING_FOR_OPERATOR_COMPLEMENT "co"
#define MANGLING_STRING_FOR_OPERATOR_NOT "nt"
#define MANGLING_STRING_FOR_OPERATOR_ASSIGN "aS"
#define MANGLING_STRING_FOR_OPERATOR_LT "lt"
#define MANGLING_STRING_FOR_OPERATOR_GT "gt"
#define MANGLING_STRING_FOR_OPERATOR_PLUS_ASSIGN "pL"
#define MANGLING_STRING_FOR_OPERATOR_MINUS_ASSIGN "mI"
#define MANGLING_STRING_FOR_OPERATOR_TIMES_ASSIGN "mL"
#define MANGLING_STRING_FOR_OPERATOR_DIVIDE_ASSIGN "dV"
#define MANGLING_STRING_FOR_OPERATOR_REMAINDER_ASSIGN "rM"
#define MANGLING_STRING_FOR_OPERATOR_EXCL_OR_ASSIGN "eO"
#define MANGLING_STRING_FOR_OPERATOR_AND_ASSIGN "aN"
#define MANGLING_STRING_FOR_OPERATOR_OR_ASSIGN "oR"
#define MANGLING_STRING_FOR_OPERATOR_SHIFT_LEFT "ls"
#define MANGLING_STRING_FOR_OPERATOR_SHIFT_RIGHT "rs"
#define MANGLING_STRING_FOR_OPERATOR_SHIFT_RIGHT_ASSIGN "rS"
#define MANGLING_STRING_FOR_OPERATOR_SHIFT_LEFT_ASSIGN "lS"
#define MANGLING_STRING_FOR_OPERATOR_EQ "eq"
#define MANGLING_STRING_FOR_OPERATOR_NE "ne"
#define MANGLING_STRING_FOR_OPERATOR_LE "le"
#define MANGLING_STRING_FOR_OPERATOR_GE "ge"
#define MANGLING_STRING_FOR_OPERATOR_SPACESHIP "ss"
#define MANGLING_STRING_FOR_OPERATOR_AND_AND "aa"
#define MANGLING_STRING_FOR_OPERATOR_OR_OR "oo"
#define MANGLING_STRING_FOR_OPERATOR_PLUS_PLUS "pp"
#define MANGLING_STRING_FOR_OPERATOR_MINUS_MINUS "mm"
#define MANGLING_STRING_FOR_OPERATOR_COMMA "cm"
#define MANGLING_STRING_FOR_OPERATOR_ARROW_STAR "pm"
#define MANGLING_STRING_FOR_OPERATOR_ARROW "pt"
#define MANGLING_STRING_FOR_OPERATOR_CALL "cl"
#define MANGLING_STRING_FOR_OPERATOR_SUBSCRIPT "ix"
#define MANGLING_STRING_FOR_OPERATOR_QUESTION "qu"
#define MANGLING_STRING_FOR_LEFT_UNARY_FOLD "fl"
#define MANGLING_STRING_FOR_LEFT_BINARY_FOLD "fL"
#define MANGLING_STRING_FOR_RIGHT_UNARY_FOLD "fr"
#define MANGLING_STRING_FOR_RIGHT_BINARY_FOLD "fR"
#define MANGLING_STRING_FOR_OPERATOR_AWAIT "aw"
#if GNU_EXTENSIONS_ALLOWED
#define MANGLING_STRING_FOR_OPERATOR_GNU_MIN "v23min"
#define MANGLING_STRING_FOR_OPERATOR_GNU_MAX "v23max"
#if ABI_COMPATIBILITY_VERSION >= 402
#define MANGLING_STRING_FOR_TYPEOF_TYPE "Dy"
#define MANGLING_STRING_FOR_TYPEOF_EXPR "DY"
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
#endif /* GNU_EXTENSIONS_ALLOWED */
#define MANGLING_STRING_FOR_CONSTRUCTOR "C1"
#define MANGLING_STRING_FOR_INHERITING_CONSTRUCTOR "CI"
#define MANGLING_STRING_FOR_DESTRUCTOR "D1"
#define MANGLING_STRING_FOR_CONVERSION_FUNC "cv"
#define MANGLING_STRING_FOR_LITERAL_OPERATORS "li"
#define MANGLING_STRING_FOR_NULLPTR "Dn"
#define MANGLING_STRING_FOR_DECLTYPE_TYPE "Dt"
#define MANGLING_STRING_FOR_DECLTYPE_EXPR "DT"
/*
There is no currently specified encoding for __underlying_type.  Clang uses
the "U3eut" vendor extension, and EDG had used "Du" (as an EDG extension) but
the "Du" encoding has now been allocated for char8_t.  A change has been
made to use the Clang mangling as of the 5.1 version.
*/
#if ABI_COMPATIBILITY_VERSION >= 510
#define MANGLING_STRING_FOR_UNDERLYING_TYPE "U3eut"  /* Clang's mangling */
#else /* ABI_COMPATIBILITY_VERSION < 510 */
#define MANGLING_STRING_FOR_UNDERLYING_TYPE "Du"  /* an EDG extension */
#endif /* ABI_COMPATIBILITY_VERSION >= 510 */
#define MANGLING_STRING_FOR_CAST "cv"
#define MANGLING_STRING_FOR_STATIC_CAST "sc"
#define MANGLING_STRING_FOR_CONST_CAST "cc"
#define MANGLING_STRING_FOR_REINTERPRET_CAST "rc"
#define MANGLING_STRING_FOR_DYNAMIC_CAST "dc"
#define MANGLING_STRING_FOR_OPERATOR_DOT_STAR "ds"
#define MANGLING_STRING_FOR_OPERATOR_DOT "dt"
#define MANGLING_STRING_FOR_AUTO "Da"
#define MANGLING_STRING_FOR_DECLTYPE_AUTO "Dc"
#define MANGLING_STRING_FOR_OPERATOR_NOEXCEPT "nx"
#if C99_IL_EXTENSIONS_SUPPORTED
#define MANGLING_STRING_FOR_OPERATOR_REAL_PART "v18__real__"
#define MANGLING_STRING_FOR_OPERATOR_IMAG_PART "v18__imag__"
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
#if MICROSOFT_EXTENSIONS_ALLOWED
/*
Note that these C++/CLI extensions use the IA-64 ABI order-sensitive
"vendor extended type qualifier" encoding.  The substitution handling for
order-sensitive and order-insensitive vendor extended type qualifiers
differs (see the IA-64 ABI spec for details).
*/
#define MANGLING_STRING_FOR_TRACKING_REFERENCE "U8__trkref"
#define MANGLING_STRING_FOR_HANDLE "U8__handle"
#define MANGLING_STRING_FOR_INTERIOR_PTR "U14__interior_ptr"
#define MANGLING_STRING_FOR_PIN_PTR "U9__pin_ptr"
#define MANGLING_STRING_FOR_STATIC_CONSTRUCTOR "C8"
#define MANGLING_STRING_FOR_FINALIZER "D7"
#define MANGLING_STRING_FOR_MANAGED_NULLPTR "DN"
#define MANGLING_STRING_FOR_OPERATOR_HANDLE_TO "v19clihandle"
#define MANGLING_STRING_FOR_OPERATOR_GCNEW "gc"
#define MANGLING_STRING_FOR_SAFE_CAST "v112clisafe_cast"
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#define MANGLING_STRING_FOR_PASS_OBJECT_SIZE "U17pass_object_size"
#define MANGLING_STRING_FOR_SPLICE "v16splice"
#define MANGLING_STRING_FOR_TYPE_SPLICE "Dr"  /* An EDG extension. */

#else /* !IA64_ABI */
/* Cfront-like name mangling codes. */
/*
The original Cfront-like mangling codes were specified in the Annotated
Reference Manual (7.2c), but advances in the language have required adding
additional encodings, and unlike the IA-64 ABI where these encodings are
mutually agreed upon, the Cfront encodings are now specific to EDG.

The encodings have used single characters, but almost all characters have
been used.  The following table lists each letter of the alphabet and how
it is being used.  Note that recent additions have used "D" as an "escape"
character (i.e., to create two-character encodings).

Here is a list of the encodings currently in use (as well as a couple of
characters that are currently unused):

builtin types:
D8 = char8_t
L = long long
S = signed
U = unsigned
Y = decltype(expr)
a = GNU vector_size attribute
b = bool
c = char
d = double
e = ellipsis
f = float
g = char16_t
h = unsigned char
i = int
j = __nullptr
k = char32_t
l = long
m[1248] = __intN
n = nullptr_t
o = __underlying_type
p = typeof(expression)
q = decltype(auto)
r = long double
s = short
t = typeof(type)
u = auto
v = void
w = wchar_t
x = complex
y = decltype(type)
z =


A = array
B = externalized name
C = const
D = "escape"
Dp = pack expansion
Dr = restrict
DR = type splice (expression)
DX = Clang's pass_object_size attribute
E = rvalue reference
F = function type
G = global scope (i.e., ::)
H = handle
I = decltype parameter reference
J = compression
K = extern "C"
L = pointer to member, long long
M = pointer-to-member
N = function parameters
O = operation
P = pointer
Q = nested name (demangle_type_name)
R = reference
S = string literal constant, signed
T = repeated parameter types
U = unsigned
V = volatile
W =
X = constant template argument
Y = decltype
Z = template parameter (demangle_type_name)


*/
#define MANGLING_CODE_FOR_CONST 'C'
#define MANGLING_CODE_FOR_VOLATILE 'V'
#if ABI_COMPATIBILITY_VERSION >= 405
#define MANGLING_CODE_FOR_RESTRICT "Dr"
#endif /* ABI_COMPATIBILITY_VERSION >= 405 */
#define MANGLING_STRING_FOR_ATOMIC "DA"
#define MANGLING_CODE_FOR_ELLIPSIS 'e'
#define MANGLING_CODE_FOR_EXTERN_C 'K'
#define MANGLING_STRING_FOR_NOEXCEPT "Do"
#define MANGLING_STRING_FOR_NOEXCEPT_EXPR "DO"
#define MANGLING_STRING_FOR_VOID "v"
#define MANGLING_STRING_FOR_WCHAR_T "w"
#define MANGLING_STRING_FOR_CHAR8_T "D8"
#define MANGLING_STRING_FOR_CHAR16_T "g"
#define MANGLING_STRING_FOR_CHAR32_T "k"
#define MANGLING_STRING_FOR_BOOL "b"
#define MANGLING_STRING_FOR_CHAR "c"
#define MANGLING_STRING_FOR_SIGNED_CHAR "Sc"
#define MANGLING_STRING_FOR_UNSIGNED_CHAR "Uc"
#define MANGLING_STRING_FOR_SHORT "s"
#define MANGLING_STRING_FOR_UNSIGNED_SHORT "Us"
#define MANGLING_STRING_FOR_INT "i"
#define MANGLING_STRING_FOR_UNSIGNED_INT "Ui"
#define MANGLING_STRING_FOR_LONG "l"
#define MANGLING_STRING_FOR_UNSIGNED_LONG "Ul"
#if LONG_LONG_ALLOWED
#define MANGLING_STRING_FOR_LONG_LONG "L"
#define MANGLING_STRING_FOR_UNSIGNED_LONG_LONG "UL"
#endif /* LONG_LONG_ALLOWED */
#if INT128_EXTENSIONS_ALLOWED
#define MANGLING_STRING_FOR_INT128 "m16"
#define MANGLING_STRING_FOR_UNSIGNED_INT128 "Um16"
#endif /* INT128_EXTENSIONS_ALLOWED */
#define MANGLING_STRING_FOR_FLOAT16 "mf2"
#define MANGLING_STRING_FOR_FP16 "mf6"
#define MANGLING_STRING_FOR_FLOAT "f"
#define MANGLING_STRING_FOR_FLOAT32X "mf4"
#define MANGLING_STRING_FOR_DOUBLE "d"
#define MANGLING_STRING_FOR_FLOAT64X "mf8"
#define MANGLING_STRING_FOR_LONG_DOUBLE "r"
#define MANGLING_STRING_FOR_FLOAT80 "mf10"
#define MANGLING_STRING_FOR_FLOAT128 "mf16"
/*
For extended floating-point types we use the same mangling as we would for any
type in the std namespace.
*/
#define MANGLING_STRING_FOR_STD_BFLOAT16 "Q2_3std8bfloat16"
#define MANGLING_STRING_FOR_STD_FLOAT16 "Q2_3std7float16"
#define MANGLING_STRING_FOR_STD_FLOAT32 "Q2_3std7float32"
#define MANGLING_STRING_FOR_STD_FLOAT64 "Q2_3std7float64"
#define MANGLING_STRING_FOR_STD_FLOAT128 "Q2_3std8float128"
#define MANGLING_STRING_FOR_COMPLEX_FLOAT16 "xmf2"
#define MANGLING_STRING_FOR_COMPLEX_FLOAT "xf"
#define MANGLING_STRING_FOR_COMPLEX_DOUBLE "xd"
#define MANGLING_STRING_FOR_COMPLEX_LONG_DOUBLE "xr"
#define MANGLING_STRING_FOR_COMPLEX_FLOAT80 "xmf10"
#define MANGLING_STRING_FOR_COMPLEX_FLOAT128 "xmf16"
#define MANGLING_STRING_FOR_COMPLEX_STD_BFLOAT16 "xQ2_3std8bfloat16"
#define MANGLING_STRING_FOR_COMPLEX_STD_FLOAT16 \
                                            MANGLING_STRING_FOR_COMPLEX_FLOAT16
#define MANGLING_STRING_FOR_COMPLEX_STD_FLOAT32 "xQ2_3std7float32"
#define MANGLING_STRING_FOR_COMPLEX_STD_FLOAT64 "xQ2_3std7float64"
#define MANGLING_STRING_FOR_COMPLEX_STD_FLOAT128 "xQ2_3std8float128"
#define MANGLING_STRING_FOR_COMPLEX_FLOAT32X "xmf4"
#define MANGLING_STRING_FOR_COMPLEX_FLOAT64X "xmf8"
#if C99_IL_EXTENSIONS_SUPPORTED
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
#define MANGLING_STRING_FOR_REFERENCE "R"
#define MANGLING_STRING_FOR_RVALUE_REFERENCE "E"
#define MANGLING_STRING_FOR_POINTER "P"
#define MANGLING_STRING_FOR_POINTER_TO_MEMBER "M"
#define MANGLING_STRING_FOR_ARRAY "A"
#if GNU_VECTOR_TYPES_ALLOWED
#define MANGLING_STRING_FOR_VECTOR "a"
#define MANGLING_STRING_FOR_SCALABLE_VECTOR_COUNT "11__SVCount_t"
#define MANGLING_STRING_FOR_MFP8 "6__mfp8"
#define MANGLING_STRING_FOR_FLOAT8E4M3 "12__float8e4m3"
#define MANGLING_STRING_FOR_FLOAT8E5M2 "12__float8e5m2"
#endif /* GNU_VECTOR_TYPES_ALLOWED */
#define MANGLING_STRING_FOR_META_INFO "mxi"
#define MANGLING_STRING_FOR_OPERATOR_NEW "nw"
#define MANGLING_STRING_FOR_OPERATOR_DELETE "dl"
#define MANGLING_STRING_FOR_OPERATOR_ARRAY_NEW "nwa"
#define MANGLING_STRING_FOR_OPERATOR_ARRAY_DELETE "dla"
#define MANGLING_STRING_FOR_OPERATOR_PLUS "pl"
#define MANGLING_STRING_FOR_OPERATOR_MINUS "mi"
#define MANGLING_STRING_FOR_OPERATOR_MULT "ml"
#define MANGLING_STRING_FOR_OPERATOR_DIVIDE "dv"
#define MANGLING_STRING_FOR_OPERATOR_REMAINDER "md"
#define MANGLING_STRING_FOR_OPERATOR_EXCL_OR "er"
#define MANGLING_STRING_FOR_OPERATOR_AND "ad"
#define MANGLING_STRING_FOR_OPERATOR_OR "or"
#define MANGLING_STRING_FOR_OPERATOR_COMPLEMENT "co"
#define MANGLING_STRING_FOR_OPERATOR_NOT "nt"
#define MANGLING_STRING_FOR_OPERATOR_ASSIGN "as"
#define MANGLING_STRING_FOR_OPERATOR_LT "lt"
#define MANGLING_STRING_FOR_OPERATOR_GT "gt"
#define MANGLING_STRING_FOR_OPERATOR_PLUS_ASSIGN "apl"
#define MANGLING_STRING_FOR_OPERATOR_MINUS_ASSIGN "ami"
#define MANGLING_STRING_FOR_OPERATOR_TIMES_ASSIGN "amu"
#define MANGLING_STRING_FOR_OPERATOR_DIVIDE_ASSIGN "adv"
#define MANGLING_STRING_FOR_OPERATOR_REMAINDER_ASSIGN "amd"
#define MANGLING_STRING_FOR_OPERATOR_EXCL_OR_ASSIGN "aer"
#define MANGLING_STRING_FOR_OPERATOR_AND_ASSIGN "aad"
#define MANGLING_STRING_FOR_OPERATOR_OR_ASSIGN "aor"
#define MANGLING_STRING_FOR_OPERATOR_SHIFT_LEFT "ls"
#define MANGLING_STRING_FOR_OPERATOR_SHIFT_RIGHT "rs"
#define MANGLING_STRING_FOR_OPERATOR_SHIFT_RIGHT_ASSIGN "ars"
#define MANGLING_STRING_FOR_OPERATOR_SHIFT_LEFT_ASSIGN "als"
#define MANGLING_STRING_FOR_OPERATOR_EQ "eq"
#define MANGLING_STRING_FOR_OPERATOR_NE "ne"
#define MANGLING_STRING_FOR_OPERATOR_LE "le"
#define MANGLING_STRING_FOR_OPERATOR_GE "ge"
#define MANGLING_STRING_FOR_OPERATOR_SPACESHIP "ss"
#define MANGLING_STRING_FOR_OPERATOR_AND_AND "aa"
#define MANGLING_STRING_FOR_OPERATOR_OR_OR "oo"
#define MANGLING_STRING_FOR_OPERATOR_PLUS_PLUS "pp"
#define MANGLING_STRING_FOR_OPERATOR_MINUS_MINUS "mm"
#define MANGLING_STRING_FOR_OPERATOR_COMMA "cm"
#define MANGLING_STRING_FOR_OPERATOR_ARROW_STAR "rm"
#define MANGLING_STRING_FOR_OPERATOR_ARROW "rf"
#define MANGLING_STRING_FOR_OPERATOR_CALL "cl"
#define MANGLING_STRING_FOR_OPERATOR_SUBSCRIPT "vc"
#define MANGLING_STRING_FOR_OPERATOR_QUESTION "qs"
#define MANGLING_STRING_FOR_LEFT_UNARY_FOLD "fl"
#define MANGLING_STRING_FOR_LEFT_BINARY_FOLD "fL"
#define MANGLING_STRING_FOR_RIGHT_UNARY_FOLD "fr"
#define MANGLING_STRING_FOR_RIGHT_BINARY_FOLD "fR"
#define MANGLING_STRING_FOR_OPERATOR_AWAIT "aw"
#if GNU_EXTENSIONS_ALLOWED
#define MANGLING_STRING_FOR_OPERATOR_GNU_MIN "mn"
#define MANGLING_STRING_FOR_OPERATOR_GNU_MAX "mx"
#if ABI_COMPATIBILITY_VERSION >= 402
#define MANGLING_STRING_FOR_TYPEOF_TYPE "t"
#define MANGLING_STRING_FOR_TYPEOF_EXPR "p"
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
#endif /* GNU_EXTENSIONS_ALLOWED */
#define MANGLING_STRING_FOR_CONSTRUCTOR "ct"
#define MANGLING_STRING_FOR_DESTRUCTOR "dt"
#define MANGLING_STRING_FOR_CONVERSION_FUNC "op"
#define MANGLING_STRING_FOR_LITERAL_OPERATORS "li"
#define MANGLING_STRING_FOR_NULLPTR "n"
#define MANGLING_STRING_FOR_CAST "cs"
#define MANGLING_STRING_FOR_DECLTYPE_TYPE "y"
#define MANGLING_STRING_FOR_DECLTYPE_EXPR "Y"
#define MANGLING_STRING_FOR_UNDERLYING_TYPE "o"
#define MANGLING_STRING_FOR_STATIC_CAST "sc"
#define MANGLING_STRING_FOR_CONST_CAST "cc"
#define MANGLING_STRING_FOR_REINTERPRET_CAST "rc"
#define MANGLING_STRING_FOR_DYNAMIC_CAST "dc"
#define MANGLING_STRING_FOR_OPERATOR_DOT_STAR "ds"
#define MANGLING_STRING_FOR_OPERATOR_DOT "dt"
#define MANGLING_STRING_FOR_AUTO "u"
#define MANGLING_STRING_FOR_DECLTYPE_AUTO "q"
#define MANGLING_STRING_FOR_OPERATOR_NOEXCEPT "nx"
#if C99_IL_EXTENSIONS_SUPPORTED
#define MANGLING_STRING_FOR_OPERATOR_REAL_PART "rl"
#define MANGLING_STRING_FOR_OPERATOR_IMAG_PART "im"
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
#if ABI_COMPATIBILITY_VERSION >= 402
#define MANGLING_STRING_FOR_OPERATOR_UNARY_PLUS "ps"
#define MANGLING_STRING_FOR_OPERATOR_NEGATE "ng"
#define MANGLING_STRING_FOR_OPERATOR_DEREFERENCE "de"
#define MANGLING_STRING_FOR_OPERATOR_ADDRESS "ao"
#define MANGLING_STRING_FOR_OPERATOR_PLUS_PLUS_PREFIX "ppe"
#define MANGLING_STRING_FOR_OPERATOR_MINUS_MINUS_PREFIX "mme"
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
#if MICROSOFT_EXTENSIONS_ALLOWED
/* These are all specific to C++/CLI. */
#define MANGLING_STRING_FOR_TRACKING_REFERENCE "Ht"
#define MANGLING_STRING_FOR_HANDLE "Hh"
#define MANGLING_STRING_FOR_INTERIOR_PTR "Hi"
#define MANGLING_STRING_FOR_PIN_PTR "Hp"
#define MANGLING_STRING_FOR_STATIC_CONSTRUCTOR "st"
#define MANGLING_STRING_FOR_FINALIZER "df"
#define MANGLING_STRING_FOR_MANAGED_NULLPTR "j"
#define MANGLING_STRING_FOR_OPERATOR_HANDLE_TO "ht"
#define MANGLING_STRING_FOR_OPERATOR_GCNEW "gc"
#define MANGLING_STRING_FOR_OPERATOR_CLI_SUBSCRIPT "sb"
#define MANGLING_STRING_FOR_SAFE_CAST "sf"
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#define MANGLING_STRING_FOR_PASS_OBJECT_SIZE "DX"
#define MANGLING_STRING_FOR_SPLICE "SP"
#define MANGLING_STRING_FOR_TYPE_SPLICE "DR"

#endif /* IA64_ABI */

/*
The number of characters used for the CRC suffix when a mangled name is
truncated.
*/
#define SIZE_OF_TRUNCATED_SUFFIX 10


/*
Utility that returns TRUE if the variable is a structured binding container
that requires a mangled name.
*/
#define struct_binding_container_needs_mangling(vp)                           \
  ((vp)->is_struct_binding_container &&                                       \
   (vp)->storage_class != (a_storage_class)sc_auto)

#if IA64_ABI

typedef unsigned long 
		a_substitution_index;
			/* The type of a numerical substitution index.	The
			   first index is index zero. */

/*
Data structure used to represent entities in mangled names for which
substitution can be done in the IA-64 ABI name mangling scheme.
Substitution is a technique used to reduce the size of mangled names
by allowing a substitution reference of the form "Snnn_" to indicate
repetition of something that has appeared earlier in the mangled name.
*/
typedef struct a_substitution *a_substitution_ptr;
typedef struct a_substitution {
  a_substitution_ptr
		next;
			/* The next (more recent) available substitution 
			   candidate. */
  a_substitution_ptr
		next_bucket;
			/* The next substitution with the same hash value. */
  char		*entity;
			/* The entity to which this substitution applies. */
  an_il_entry_kind
		kind;
			/* The kind of object represented by this 
			   substitution candidate. */
  a_substitution_index
		index;
			/* The substitution index to use during mangling. */
  a_bit_field	is_pack_expansion:1;
			/* TRUE if this substitution represents a pack
			   expansion for the indicated type.  A type can be
			   on the substitution list twice (once with this
			   flag TRUE, and once FALSE). */
} a_substitution;

/*
A hash table indexed by a substitutable entity (using subst_hash as
the hashing function).  Each entry in the array points to a list of
substitutions with the same hash value.  An entity may be on this list multiple
times (because of different values for a_substitution::is_pack_expansion).
*/
#define SUBSTITUTION_CACHE_SIZE 0x100
STATIC_THREAD a_substitution_ptr
		substitution_cache[SUBSTITUTION_CACHE_SIZE];

/* Hash function for a substitutable entity. */
#define subst_hash(ptr)                                                      \
   (((uintptr_t)ptr >> 8) % SUBSTITUTION_CACHE_SIZE)

#endif /* IA64_ABI */

/*
Control block for mangling.
*/
typedef struct a_mangling_control_block *a_mangling_control_block_ptr;
typedef struct a_mangling_control_block {
  sizeof_t	length;
			/* Current length of the mangled name.  Note that
			   this differs from mangling_text_buffer->size in
			   that it does not count the blanks left as reserved
			   space for leading lengths, which will be removed
			   at the end of generating the name. */
  sizeof_t	num_leftover_spaces;
			/* Count of the extra leftover spaces described
			   above. */
#if IA64_ABI
  a_substitution_ptr
                first_substitution;
			/* The first (reading left-to-right) substitution 
			   candidate for this mangling operation. */
  a_substitution_ptr
                last_substitution;
			/* The last (most recent) substitution candidate for
			   this mangling operation. */
  a_byte_boolean
		force_dependent_array_mangling;
			/* Used to emulate a g++ bug with regard to use
			   of an expression instead of a constant bound
			   for a non-dependent array bound. */
#if CHECKING && ABI_COMPATIBILITY_VERSION < 402
  a_byte_boolean
		mangling_sizeof_expression;
			/* TRUE while the entity being mangled is the
			   expression under a sizeof.  Such an expression may
			   contain operators that otherwise are not allowed
			   to appear in template argument expressions. */
#endif /* CHECKING && ABI_COMPATIBILITY_VERSION < 402 */
  unsigned long
                suppress_substitutions;
			/* Suppress the generation of substitutions when
			   this flag is non-zero.  This is used only in cases
			   where we're trying to emulate a certain observed
			   GNU behavior. */
#else /* !IA64_ABI */
  a_boolean	suppress_partial_spec_args;
			/* TRUE to suppress extra information on partial
			   specialization arguments. */
#endif /* !IA64_ABI */
  a_boolean	lacking_module_id;
			/* TRUE if the entity being mangled depends on a
			   module id.  In cases where the mangling is being
			   performed during a mangling pre-pass, we don't want
			   mangled names to contain the module id of a
			   particular translation unit, so this flag indicates
			   that mangling should be skipped for this particular
			   entity. */
  a_boolean     mangle_auto_placeholder;
                        /* TRUE if the entity being mangled is the return
                           type of a routine where has_deduced_return_type
                           is TRUE -- in which case auto and decltype(auto)
                           typerefs should be mangled explicitly (otherwise
                           the underlying type is used). */
  a_boolean     mangling_prototype_instantiation;
                        /* TRUE if the entity being mangled is a
                           prototype instantiation.  These are not typically
                           mangled when doing lowering, but are mangled in
                           some configurations. */
#if IA64_ABI
  char          ctor_dtor_char;
                        /* When mangling a constructor or destructor, this
                           represents the type of alternate entry point
                           for the constructor/destructor.  This is only used
                           when the mangled name is truncated (as a way to
                           differentiate between alternate entry points
                           that would otherwise have the same mangled name).
                           See truncate_mangled_name. */
#endif /* IA64_ABI */
} a_mangling_control_block;


/*
Text buffers used for mangling.  In some cases, mangling routines can be called
recursively, necessitating multiple mangling text buffers.
*/
typedef struct a_mangling_buffer *a_mangling_buffer_ptr;
typedef struct a_mangling_buffer {
  a_mangling_buffer_ptr
                next;   /* Points to the next entry on the list. */
  a_text_buffer_ptr
                text_buffer;
                        /* A text buffer used for name mangling. */
} a_mangling_buffer;

STATIC_THREAD a_mangling_buffer_ptr
                mangling_buffers_in_use;
                        /* A list of a_mangling_buffer entries that are in-use
                           (points to the buffer currently in-use, previous
                           in-use buffers are linked by next). */

STATIC_THREAD a_mangling_buffer_ptr
                mangling_buffer_free_list;
                        /* A list of free mangling buffers. */

STATIC_THREAD a_text_buffer_ptr
		mangling_text_buffer;
                        /* The text buffer currently being used for mangling.
                           Points to the text buffer at the head of the
                           mangling_buffers_in_use list (i.e., 
                           mangling_buffers_in_use->text_buffer when
                           mangling_buffers_in_use is non-NULL, and NULL
                           otherwise). */

STATIC_THREAD a_boolean
                in_mangling_pre_pass;
                        /* TRUE if we're in a mangling "pre-pass" (e.g.,
                           before generating a PCH file).  In a mangling
                           pre-pass, entities whose mangled names can't be
                           generated yet (e.g., because they rely on a module
                           id) are left to be mangled later. */


static void mangled_encoding_for_type(a_type_ptr               type,
                                      a_mangling_control_block *mctl);
static void mangled_encoding_for_type_full(
                                a_type_ptr               type,
                                a_boolean                suppress_substitution,
                                a_mangling_control_block *mctl);
static void mangled_encoding_for_type_with_pack_expansion(
                                    a_type_ptr               type,
                                    a_boolean                is_pack_expansion,
                                    a_mangling_control_block *mctl);
static void mangled_function_base_name(
                                      a_source_correspondence  *scp,
                                      a_special_function_kind  special_kind,
                                      an_opname_kind           opname_kind,
                                      a_ctor_or_dtor_kind      ctor_dtor_kind,
                                      unsigned int             num_operands,
                                      a_type_ptr               conversion_type,
                                      a_const_char             *ud_suffix,
                                      a_mangling_control_block *mctl);
static void mangled_function_name(
                             a_routine_ptr            routine,
                             a_boolean                suppress_param_encoding,
                             a_boolean                suppress_parent_encoding,
                             a_boolean                force_primary_name,
                             a_boolean                force_individuation,
                             sizeof_t                 *base_name_offset,
                             a_mangling_control_block *mctl);
static void mangled_function_name_externalized_if_necessary(
                             a_routine_ptr            routine,
                             a_boolean                suppress_param_encoding,
                             a_boolean                suppress_parent_encoding,
                             a_boolean                force_primary_name,
                             sizeof_t                 *base_name_offset,
                             a_mangling_control_block *mctl);
static void mangled_variable_name_with_possible_qualification(
                                             a_variable_ptr           variable,
                                             a_mangling_control_block *mctl);
static a_const_char *mangled_expr_operator_name(an_expr_node_ptr expr,
                                                a_boolean        *bad_operator,
                                                a_boolean        *is_cast);

/*
Macro for the typical invocation of mangled_encoding_for_expression_full
where suppress_address_of is FALSE.
*/
#define mangled_encoding_for_expression(expr, in_dependent_expr, mctl)      \
  mangled_encoding_for_expression_full((expr), (in_dependent_expr),         \
                                       /*suppress_address_of=*/FALSE, (mctl))

static void mangled_encoding_for_expression_full(
                                  an_expr_node_ptr         expr,
                                  a_boolean                in_dependent_expr,
                                  a_boolean                suppress_address_of,
                                  a_mangling_control_block *mctl);
static void mangled_name_with_possible_qualification(
                                               a_source_correspondence  *scp,
                                               an_il_entry_kind         kind,
                                               a_template_ptr           tmpl,
                                               a_mangling_control_block *mctl);
static void mangled_encoding_for_constant(
                                  a_constant_ptr           con,
                                  a_boolean                old_form,
                                  a_boolean                in_dependent_expr,
                                  a_boolean                suppress_address_of,
                                  a_mangling_control_block *mctl);
#if !IA64_ABI
static char *compress_mangled_name(a_const_char             *mangled_name,
                                   a_source_correspondence  *scp,
                                   a_mangling_control_block *mctl);
static void add_nesting_level_encoding(unsigned long            nesting_level,
                                       a_mangling_control_block *mctl);
#endif /* !IA64_ABI */
static char *truncate_mangled_name(char                     *mangled_name,
                                   a_source_correspondence  *scp,
                                   a_mangling_control_block *mctl);
static void r_mangled_parent_qualifier(
                             a_source_correspondence  *scp,
                             an_il_entry_kind         kind,
                             unsigned long            nesting_level,
                             a_boolean                needs_to_be_individuated,
                             a_source_correspondence  **top_most_scp,
                             a_mangling_control_block *mctl);
#if IA64_ABI
static void mangled_ia64_parent_qualifier(
                              a_source_correspondence  *scp,
                              an_il_entry_kind         kind,
                              a_boolean                *need_nested_name_close,
                              a_source_correspondence  **discriminator_scp,
                              a_boolean                force_individuation,
                              a_mangling_control_block *mctl);
static void close_ia64_nested_name(
                              a_boolean                 need_nested_name_close,
                              a_source_correspondence  *discriminator_scp,
                              a_mangling_control_block *mctl);
#if ABI_COMPATIBILITY_VERSION >= 402
static a_boolean gnu_requires_decltype_mangling(a_type_ptr type);
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
static a_boolean substitution_available(
                                    char                     *entity,
                                    an_il_entry_kind         kind,
                                    a_boolean                is_pack_expansion,
                                    a_mangling_control_block *mctl);
static a_boolean add_substitution_if_available_full(
                            char                     *entity,
                            an_il_entry_kind         kind,
                            a_boolean                is_pack_expansion,
                            a_boolean                test,
                            a_boolean                *is_standard_substitution,
                            a_mangling_control_block *mctl);
#if DO_IL_LOWERING
static char ctor_dtor_kind_char(a_routine_ptr routine);
static void overwrite_ctor_dtor_mangled_name_kind(char          *name,
                                                  a_routine_ptr routine,
                                                  char          ch);
#endif /* DO_IL_LOWERING */
#endif /* IA64_ABI */
static void mangled_template_arguments(
                                    a_template_arg_ptr       template_arg_list,
                                    a_boolean                partial_spec,
                                    a_boolean                old_form,
                                    a_name_reference_ptr     name_reference,
                                    a_mangling_control_block *mctl);
static a_boolean function_name_mangling_needed(
                                       a_routine_ptr routine,
                                       a_boolean     *suppress_param_encoding);
static a_const_char *mangled_operator_name(an_opname_kind kind,
                                           unsigned int   num_operands);
static void mangled_simple_id(a_source_correspondence_ptr scp,
                              a_template_arg_ptr          template_arg_list,
                              a_name_reference_ptr        name_reference,
                              a_boolean                   include_length,
                              a_mangling_control_block    *mctl);
#if DO_IL_LOWERING || (!IA64_ABI && ABI_COMPATIBILITY_VERSION < 520)
static a_const_char *unmangled_or_fabricated_name_of_variable(
                                                           a_variable_ptr var);
#endif /* DO_IL_LOWERING || (!IA64_ABI && ABI_COMPATIBILITY_VERSION < 520) */
#if ABI_COMPATIBILITY_VERSION >= 402
static void mangled_unresolved_name(an_expr_node_ptr         expr,
                                    an_expr_node_ptr         arguments,
                                    an_expr_node_ptr         selector,
                                    a_boolean                in_dependent_expr,
                                    a_mangling_control_block *mctl);
static void mangled_operator_or_special_function(
                         an_opname_kind           kind,
                         unsigned int             num_operands,
                         a_type_ptr               conversion_type,
                         a_const_char             *ud_suffix,
                         a_template_arg_ptr       template_arg_list,
                         a_name_reference_ptr     name_reference,
                         a_boolean                suppress_operation_indicator,
                         a_boolean                suppress_underscores,
                         a_mangling_control_block *mctl);
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
static a_boolean entity_needs_to_be_individuated(a_source_correspondence *scp,
                                                 an_il_entry_kind        kind);
static void mangled_encoding_for_param_reference(
                                               an_expr_node_ptr         expr,
                                               a_mangling_control_block *mctl);
static void mangled_type_name_full(a_type_ptr               type,
                                   a_boolean                check_for_subst,
                                   a_boolean                ok_to_mangle_type,
                                   a_mangling_control_block *mctl);
static an_expr_node_ptr skip_compiler_generated_expressions(
                                        an_expr_node_ptr expr,
                                        a_boolean        *suppress_address_of);
static void mangled_braced_init_list(an_expr_node_ptr         expr_list,
                                     a_constant_ptr           con,
                                     a_type_ptr               type,
                                     a_mangling_control_block *mctl);
static void mangled_dynamic_init(a_dynamic_init_ptr       dip,
                                 a_type_ptr               type,
                                 a_boolean                is_static_cast,
                                 a_mangling_control_block *mctl);
static a_boolean type_is_lambda_in_initializer(a_type_ptr type);
static a_const_char *give_unnamed_namespace_a_name(
                                               a_namespace_ptr          nsp,
                                               a_mangling_control_block *mctl);
#if GNU_EXTENSIONS_ALLOWED
static void add_abi_tag_mangling(an_attribute_ptr         ap,
                                 a_mangling_control_block *mctl);
#endif /* GNU_EXTENSIONS_ALLOWED */
static void add_variable_template_indication(a_variable_ptr           vp,
                                             a_mangling_control_block *mctl);

static void mangled_name_with_length(a_const_char             *name,
                                     a_mangling_control_block *mctl);

static void mangled_simple_id_or_name(
                              a_source_correspondence_ptr scp,
                              a_const_char                *name,
                              a_template_arg_ptr          template_arg_list,
                              a_name_reference_ptr        name_reference,
                              a_boolean                   include_length,
                              a_mangling_control_block    *mctl);

typedef Ptr_map<a_source_correspondence*, bool>
		an_active_parent_map;
                        /* A convenient type for the map below. */
STATIC_THREAD an_active_parent_map
		*active_parents;
                        /* A map to keep track of whether an entity
                           (a_source_correspondence *) has already been
                           traversed during the mangling of a parent qualifier.
                           */

#if EXPENSIVE_CHECKING && IA64_ABI && ABI_COMPATIBILITY_VERSION >= 405
STATIC_THREAD a_boolean
                skip_substitution_check;
                        /* Skips the IA-64 ABI substitution check when TRUE. */
#endif /* EXPENSIVE_CHECKING && IA64_ABI && ABI_COMPATIBILITY_VERSION >= 405 */

/*
Interface to mangled_type_name_full for the usual case, where the
caller has not checked already for a substitution in IA-64 ABI mode and
it's okay to give the type a mangled name.
*/
#define mangled_type_name(type, mctl)                                        \
  (mangled_type_name_full(                                                   \
      (type), /*check_for_subst=*/TRUE, /*ok_to_mangle_type=*/TRUE, (mctl)))

#if !IA64_ABI
/*
Interface to r_mangled_parent_qualifier, to provide nesting_level == 1 and
needs_to_be_individuated == FALSE.  For the IA-64 ABI,
see mangled_ia64_parent_qualifier.
*/
#define mangled_parent_qualifier(parent, kind, mctl)                  \
  r_mangled_parent_qualifier((parent), (kind), (unsigned long)1,      \
                             /*needs_to_be_individuated=*/FALSE,      \
                             (a_source_correspondence **)NULL,        \
                             (mctl))
#endif /* !IA64_ABI */

/*
Returns the ud-suffix for a literal operator routine, or NULL if the
routine is not a literal operator routine.
*/
#define ud_suffix_for_routine(rp)                                             \
  ((rp)->special_kind == (a_special_function_kind)sfk_udl_operator ?          \
    (unmangled_name_of(&(rp)->source_corresp) == NULL ?                       \
                             NULL :                                           \
                             ud_suffix_from_literal_operator_id(              \
                                 unmangled_name_of(&(rp)->source_corresp))) : \
    NULL)

static void clear_mangling_control_block(
         a_mangling_control_block_ptr mctl,
         a_boolean                    mangling_prototype_instantiation = FALSE)
/*
Set the fields of the indicated mangling control block to default values.
If the entity being mangled is a prototype instantiation, indicate that as
well.
*/
{
  mctl->length = 0;
  mctl->num_leftover_spaces = 0;
#if IA64_ABI
  mctl->first_substitution = NULL;
  mctl->last_substitution = NULL;
  mctl->force_dependent_array_mangling = FALSE;
#if CHECKING && ABI_COMPATIBILITY_VERSION < 402
  mctl->mangling_sizeof_expression = FALSE;
#endif /* CHECKING && ABI_COMPATIBILITY_VERSION < 402 */
  mctl->suppress_substitutions = 0;
#else /* !IA64_ABI */
  mctl->suppress_partial_spec_args = FALSE;
#endif /* !IA64_ABI */
  mctl->lacking_module_id = FALSE;
  mctl->mangle_auto_placeholder = FALSE;
  mctl->mangling_prototype_instantiation = mangling_prototype_instantiation;
#if IA64_ABI
  mctl->ctor_dtor_char = '\0';
#endif /* IA64_ABI */
}  /* clear_mangling_control_block */

#if IA64_ABI

static char *canonical_substitution_entity(a_type_ptr type)
/*
The given type is being processed for mangling substitution.  Return a
"canonical" type entry to be used for this process.  For example, a proxy class
for a template parameter is replaced by the corresponding tk_template_param
entry.
*/
{
  char *entity = (char*)type;

  if (type_is(type, tk_class) &&
      type->variant.class_struct_union.proxy_class) {
    /* If this class is a proxy class for a template parameter, use the
       template parameter as the entity. */
    type = class_type_supp(type)->proxy_of_type;
    if (type != NULL) {
#if ABI_COMPATIBILITY_VERSION >= 414
      /* In rare cases (e.g., class portion of a pointer-to-member in GNU
         emulation mode), the proxy class can be a typedef, so strip that if
         applicable. */
      entity = (char *)canonical_substitution_entity(type);
#else /* ABI_COMPATIBILITY_VERSION < 414 */
      entity = (char *)type;
#endif /* ABI_COMPATIBILITY_VERSION >= 414 */
    }  /* if */
  } else if (type_is(type, tk_typeref)) {
#if ABI_COMPATIBILITY_VERSION >= 402
    if (emulate_gnu_abi_bugs &&
        is_typeref_kind(type, trk_is_decltype) &&
        type->variant.typeref.is_dependent_type_operator &&
        !gnu_requires_decltype_mangling(type)) {
      /* This is a dependent decltype and typically gets its own
         substitution, but if we're emulating GNU and GNU doesn't believe
         the decltype is dependent, then strip the decltype(s) for substitution
         purposes. */
      entity = canonical_substitution_entity(type->variant.typeref.type);
    } else
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
    /* Do not insert code here. */
    {
      type = skip_typedefs_not_dependent_decltypes(type);
      if (type_is(type, tk_typeref)) {
        a_type_qualifier_set  tqs = type->variant.typeref.qualifiers;
        if ((tqs & TRANSPARENT_QUALIFIERS) != TQ_NONE) {
          /* Ignore any "transparent" type qualifiers, i.e., qualifiers that
             are only meant as information to a back end but do not affect the
             type itself. */
          type = type->variant.typeref.type;
          tqs = tqs & ~TRANSPARENT_QUALIFIERS;
          if (tqs != TQ_NONE) {
            type = make_qualified_type(type, tqs);
          }  /* if */
        }  /* if */
      }  /* if */
      entity = (char*)type;
    }  /* if */
  }  /* if */
  return entity;
}  /* canonical_substitution_entity */


static constexpr char
                base_36_digits[37] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

/* Pointer to a list of available (freed) substitutions. */
STATIC_THREAD a_substitution_ptr
		avail_substitutions;


static void alloc_substitution(char                         *entity,
                               an_il_entry_kind             kind,
                               a_boolean                    is_pack_expansion,
                               a_mangling_control_block_ptr mctl)
/*
Allocate a substitution entry for entity, which has the indicated kind,
and add it to the list of substitutions pointed to by
mctl->first_substitution/mctl->last_substitution.  is_pack_expansion is TRUE
when the substitution represents the pack expanded version of the specified
type and should be FALSE otherwise.  Because of the way pack expanded types
are mangled, a type entry can be on the substitution list two times, once
with is_pack_expansion set to FALSE and once with it set to TRUE.
*/
{
  a_substitution_ptr sp, last_sp;

  check_assertion(!is_pack_expansion || kind == iek_type);
#if EXPENSIVE_CHECKING && ABI_COMPATIBILITY_VERSION >= 405
  if (!emulate_gnu_abi_bugs && !skip_substitution_check) {
    /* Verify that there is no available substitution (the caller should
       already have checked this).  Skip the check when emulating GNU ABI bugs
       because in some cases the substitutions are intentionally
       non-standard. */
    a_boolean is_standard_substitution = FALSE;
    check_assertion_str(!(add_substitution_if_available_full(entity,
                                                     kind,
                                                     is_pack_expansion,
                                                     /*test=*/TRUE,
                                                     &is_standard_substitution,
                                                     mctl) &&
                          !is_standard_substitution),
                        "alloc_substitution: missed mangling substitution");
  }  /* if */
#endif /* EXPENSIVE_CHECKING && ABI_COMPATIBILITY_VERSION >= 405 */
  if (mctl->suppress_substitutions == 0) {
    /* Record a canonical representative for the substituted entity (e.g., use
       the corresponding tk_template_param entry for a proxy class of a
       template parameter). */
    a_substitution_ptr *head;
    if (kind == iek_type) {
      entity = canonical_substitution_entity((a_type_ptr)entity);
    }  /* if */
    if (avail_substitutions != NULL) {
      sp = avail_substitutions;
      avail_substitutions = sp->next;
    } else {
      sp = (a_substitution_ptr)alloc_general(sizeof(a_substitution));
    }  /* if */
    head = &substitution_cache[subst_hash(entity)];
    sp->next_bucket = *head;
    sp->kind = kind;
    sp->entity = entity;
    *head = sp;
    ((a_source_correspondence*)entity)->on_mangling_substitution_list = TRUE;
    sp->is_pack_expansion = is_pack_expansion;
    sp->next = NULL;
    last_sp = mctl->last_substitution;
    if (last_sp != NULL) {
      sp->index = last_sp->index+1;
      last_sp->next = sp;
      mctl->last_substitution = sp;
    } else {
      sp->index = 0;
      mctl->first_substitution = mctl->last_substitution = sp;
    }  /* if */
#if DEBUG && EXPENSIVE_CHECKING
    if (db_flag_is_set("substitutions")) {
      fprintf(f_debug, "alloc S%c: <%s> %s",
                       (sp->index == 0 ? ' ' :
                        sp->index < 36 ? base_36_digits[sp->index-1] : '?'),
                       il_entry_kind_names[(int)sp->kind],
                       sp->is_pack_expansion ? "[pack_expansion] " : "");
      if (sp->kind == (an_il_entry_kind)iek_type) {
        db_abbreviated_type((a_type_ptr)sp->entity);
      } else if (sp->kind == (an_il_entry_kind)iek_template) {
        db_template_name((a_template_ptr)sp->entity);
      }  /* if */
      fprintf(f_debug, "\n");
    }  /* if */
#endif /* DEBUG && EXPENSIVE_CHECKING */
  }  /* if */
}  /* alloc_substitution */

#endif /* IA64_ABI */

static a_mangling_buffer_ptr alloc_mangling_buffer(void)
/*
Allocate and initialize a mangling buffer.
*/
{
  a_mangling_buffer_ptr mbp;

  mbp = (a_mangling_buffer_ptr)alloc_general(sizeof(a_mangling_buffer));
  mbp->next = NULL;
  mbp->text_buffer = alloc_text_buffer(2048);
  return mbp;
}  /* alloc_mangling_buffer */


static void push_mangling_text_buffer(void)
/*
Set mangling_text_buffer to a new buffer in preparation for generating
a new mangled name -- allocate one if necessary or use a buffer previously
allocated if available.  If mangling_text_buffer is currently being used,
push it so it'll be restored by a corresponding call to
pop_mangling_text_buffer.
*/
{
  a_mangling_buffer_ptr mbp;

  if (mangling_buffer_free_list == NULL) {
    /* No free buffers, allocate one. */
    mangling_buffer_free_list = alloc_mangling_buffer();
  }  /* if */
  mbp = mangling_buffer_free_list;
  mangling_buffer_free_list = mangling_buffer_free_list->next;
  mbp->next = mangling_buffers_in_use;
  mangling_buffers_in_use = mbp;
  mangling_text_buffer = mbp->text_buffer;
}  /* push_mangling_text_buffer */


static void pop_mangling_text_buffer(void)
/*
Signal that mangling_text_buffer is no longer needed for mangling.
Restore the previous mangling_text_buffer if we're nested.
*/
{
  a_mangling_buffer_ptr mbp = mangling_buffers_in_use;

  check_assertion(mbp != NULL);
  mangling_buffers_in_use = mbp->next;
  mbp->next = mangling_buffer_free_list;
  mangling_buffer_free_list = mbp;
  if (mangling_buffers_in_use == NULL) {
    mangling_text_buffer = NULL;
  } else {
    mangling_text_buffer = mangling_buffers_in_use->text_buffer;
  }  /* if */
}  /* pop_mangling_text_buffer */


static inline void start_mangling(
         a_mangling_control_block_ptr mctl,
         a_boolean                    mangling_prototype_instantiation = FALSE)
/*
Do initialization for mangling one name.  This includes setting
mangling_text_buffer to a new mangling buffer, then clearing it and mctl.
Must be paired with a corresponding call to end_mangling_full
(or pop_mangling_text_buffer if end_mangling_full is not needed).
If mangling a prototype instantiation, mangling_prototype_instantiation should
be set to TRUE.
*/
{
  clear_mangling_control_block(mctl, mangling_prototype_instantiation);
  push_mangling_text_buffer();
  reset_text_buffer(mangling_text_buffer);
#if EXPENSIVE_CHECKING && IA64_ABI && ABI_COMPATIBILITY_VERSION >= 405
  skip_substitution_check = FALSE;
#endif /* EXPENSIVE_CHECKING && IA64_ABI && ABI_COMPATIBILITY_VERSION >= 405 */
}  /* start_mangling */


/*
Add the indicated character to the mangled name.
*/
#define add_to_mangled_name(ch, mctl)                                        \
{                                                                            \
  (mctl)->length += 1;                                                       \
  add_char_to_text_buffer(mangling_text_buffer, (ch));                       \
}


static void add_str_to_mangled_name(a_const_char                 *str,
                                    a_mangling_control_block_ptr mctl)
/*
Add the indicated null-terminated string to the mangled name.
*/
{
  sizeof_t len = strlen(str);

  /* Count characters. */
  mctl->length += len;
  add_to_text_buffer(mangling_text_buffer, str, len);
  check_assertion(mctl->length + mctl->num_leftover_spaces ==
                                                   mangling_text_buffer->size);
}  /* add_str_to_mangled_name */


/*lint -ecall(523,*add_mangled_name_prefix)*/
static void add_mangled_name_prefix(
                                  ARG_UNUSED a_mangling_control_block_ptr mctl)
/*
Add any prefix required at the beginning of a mangled name.
*/
{
#if IA64_ABI
  add_str_to_mangled_name("_Z", mctl);
#endif /* IA64_ABI */
}  /* add_mangled_name_prefix */

#if IA64_ABI

static void add_template_argument_mangled_name_prefix(
                                             ARG_UNUSED a_constant_ptr    con,
                                             a_mangling_control_block_ptr mctl)
/*
Add the mangled name prefix for an external name in a template argument
(typically "_Z") to the mangled name.  Some GNU versions mistakenly omitted
the underscore when the constant, con, has reference type.  This defect was
introduced in 3.4 and fixed in 4.0, but only when using -fabi-version=3 or
higher, and to date the default GNU abi-version is still 2, so for
compatibility omit the underscore in version 30400 and later.
*/
{
#if ABI_COMPATIBILITY_VERSION >= 402
  if (is_reference_type(con->type) &&
      (emulate_gnu_abi_bugs &&
       gnu_abi_version >= 30400)) {
    /* Omit underscore to be compatible with GNU. */
    add_to_mangled_name('Z', mctl);
  } else
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
  /* Do not add code here. */
  {
    add_str_to_mangled_name("_Z", mctl);
  }  /* if */
}  /* add_template_argument_mangled_name_prefix */

#endif /* IA64_ABI */

static char *end_mangling_full(a_source_correspondence      *scp,
                               a_boolean                    final,
                               a_mangling_control_block_ptr mctl)
/*
Do processing at the end of mangling a name, which is in the mangling
buffer.  At the least, this includes adding the final null character.
Return the address of the mangled name in the buffer (this address must
be used before any additional mangling calls overwrite its contents).  Can
return NULL (and not produce a mangled name for scp) if we're in the
mangling pre-pass and a module id is required for this name.  If scp
is non-NULL, allocate a copy of the name in the IL memory region, and
update scp to point to it.  If final is TRUE, it's okay to do final
mangling, which may produce a name that can no longer be embedded in
other mangled names.  The caller can inspect mctl->lacking_module_id to
determine if the mangled name needed a module id in a pre-pass (as mentioned
above, NULL is also returned in this case).
*/
{
  char *buffer;

  if (mctl->lacking_module_id) {
    /* Some part of the mangled name requires a module id.  If we're mangling
       early (e.g., when generating a PCH file), simply discard the mangled
       name -- the correct mangled name will be generated later. */
    check_assertion(in_mangling_pre_pass);
    buffer = NULL;
  } else {
    /* Add the final null. */
    add_to_mangled_name('\0', mctl);
    if (mctl->num_leftover_spaces) {
      /* This string contains some leftover spaces, typically the result of
         saving extra room for potentially large leading length indications.
         Remove those spaces now.  Note that this is primarily used in Cfront
         ABI manglings, but is also used in the IA-64 ABI for cases where an
         unneeded mangling was "erased" (by overwriting it with spaces). */
      char *src = mangling_text_buffer->buffer;
      char *dest = src;
      char ch;
      do {
        ch = *src++;
        if (ch != ' ') {
          *dest++ = ch;
        } else {
          /* Removing a space. */
          mangling_text_buffer->size--;
          mctl->num_leftover_spaces--;
        }  /* if */
      } while (ch != '\0');
      check_assertion_str(mctl->num_leftover_spaces == 0 &&
                          mangling_text_buffer->size == mctl->length,
                         "end_mangling_full: wrong number of leftover spaces");
    }  /* if */
    buffer = mangling_text_buffer->buffer;
    if (final_name_mangling_needed && final) {
      /* Do final name mangling if needed and requested by the caller. */
#if !IA64_ABI
      /* Compress the mangled name to make it smaller. */
      buffer = compress_mangled_name((char *)NULL, scp, mctl);
#endif /* !IA64_ABI */
      /* Truncate the mangled name if necessary. */
      buffer = truncate_mangled_name(buffer, scp, mctl);
    }  /* if */
    if (scp != NULL) {
      /* Allocate space for the mangled name and copy it. */
      char *mangled_name = alloc_lowered_name_string(mctl->length);
      (void)strcpy(mangled_name, buffer);
      /* Save the unmangled form of the name.  Do not save the unmangled
         name for a class that was originally unnamed and has been given a
         name. */
      if (!scp->name_has_been_mangled) {
        scp->unmangled_name_or_mangled_encoding = scp->name;
      }  /* if */
      scp->name = mangled_name;
      scp->name_has_been_mangled = TRUE;
      scp->final_name_mangling_pending = final_name_mangling_needed && !final;
    }  /* if */
  }  /* if */
#if IA64_ABI
  /* Free the substitutions created during this mangling. */
  if (mctl->first_substitution != NULL) {
    /* Clear any flags indicating that entries are on the list. */
    a_substitution_ptr  sp = mctl->first_substitution;
    for (; sp != NULL; sp = sp->next) {
      substitution_cache[subst_hash(sp->entity)] = NULL;
      ((a_source_correspondence*)sp->entity)
                                      ->on_mangling_substitution_list = FALSE;
    }  /* for */
    mctl->last_substitution->next = avail_substitutions;
    avail_substitutions = mctl->first_substitution;
  }  /* if */
#endif /* IA64_ABI */
  /* We're done with this mangling text buffer. */
  pop_mangling_text_buffer();
#if DEBUG
/* GCOVR_EXCL_START */
  if (db_flag_is_set("mangled_names")) {
    if (scp != NULL) {
      db_name(scp);
    }  /* if */
    fprintf(f_debug, " -> %s\n", buffer);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  return buffer;
}  /* end_mangling_full */


static char *end_mangling(a_boolean                    final,
                          a_mangling_control_block_ptr mctl)
/*
An interface to end_mangling_full where the mangled name is not stored in
the entity, but rather returned to the caller for some other use.  Callers
expect that the returned name is non-NULL (which means this routine cannot
be called too early, e.g., before a module id is available for entities
that need a module id in their mangled name).  If final is TRUE, it's okay to
do final mangling, which may produce a name that can no longer be embedded in
other mangled names.
*/
{
  char *name = end_mangling_full((a_source_correspondence *)NULL, final, mctl);
  check_assertion(name != NULL);
  return name;
}  /* end_mangling */


static void add_number_to_mangled_name(a_host_large_unsigned    value,
                                       a_mangling_control_block *mctl)
/*
Add the decimal representation of value to the mangled name.  This is
simple output -- just the digits of the value, with no additional
encoding.
*/
{
  char     buffer[50];
  sizeof_t len = unsigned_to_string_buf(value, buffer);

  mctl->length += len;
  add_to_text_buffer(mangling_text_buffer, buffer, len);
  check_assertion(mctl->length + mctl->num_leftover_spaces ==
                                                   mangling_text_buffer->size);
}  /* add_number_to_mangled_name */

#if IA64_ABI

#if DO_IL_LOWERING

static void add_signed_number_to_mangled_name(long                     value,
                                              a_mangling_control_block *mctl)
/*
Add the decimal representation of value to the mangled name.  This is
simple output -- just the digits of the value, with no additional
encoding.  A negative value is prefixed by "n".
*/
{
  char     buffer[50];
  sizeof_t len = signed_to_string_buf((a_host_large_integer)value, buffer);

  /* Handle negative numbers by replacing '-' with 'n'. */
  if (buffer[0] == '-') buffer[0] = 'n';
  mctl->length += len;
  add_to_text_buffer(mangling_text_buffer, buffer, len);
  check_assertion(mctl->length + mctl->num_leftover_spaces ==
                                                   mangling_text_buffer->size);
}  /* add_signed_number_to_mangled_name */

#endif /* DO_IL_LOWERING */

static void add_base_36_number_to_mangled_name(a_substitution_index      value,
					       a_mangling_control_block  *mctl)
/*
Adds a base-36 representation (using digits and upper case letters) of
value to the mangled name.
*/
{
  a_substitution_index power = 1;

  /* Figure out the smallest power of 36 that will contain value. */
  while (power <= value) {
    power *= 36;
  }  /* while */
  /* Pull back one power of 36 to get the multiplier of the most significant
     digit.  Make sure at least one digit is used, even for zero. */
  if (power > 1) {
    power /= 36;
  }  /* if */
  /* Scan from most significant to least significant digit. */
  do {
    /* Compute the most significant digit for the remaining value. */
    unsigned int digit = (unsigned int)(value / power);  /*lint !e414*/
    /* Emit the digit. */
    add_to_mangled_name(base_36_digits[digit], mctl);
    /* Subtract the value of the digit just emitted. */
    value -= digit * power;
    /* Now do the next smaller power of 36. */
    power /= 36;
  } while (power > 0);
}  /* add_base_36_number_to_mangled_name */


/*
Add a representation of the substitution with the given index to the mangled
name.  The substitution number is written in the mangling as a -1-indexed
value in base 36.  For the first substitution, the number is omitted
altogether.
*/
#define add_substitution_index_to_mangled_name(idx, mctl)                    \
{                                                                            \
  add_to_mangled_name('S', (mctl));                                          \
  if ((idx) > 36) {                                                          \
    add_base_36_number_to_mangled_name((idx)-1, (mctl));                     \
  } else if ((idx) > 0) {                                                    \
    add_to_mangled_name(base_36_digits[(idx)-1], (mctl));                    \
  }  /* if */                                                                \
  add_to_mangled_name('_', (mctl));                                          \
}


static a_template_ptr class_template_of(a_type_ptr type)
/*
If type is an instance of a template, return a pointer to the class template
of which type is an instance.  Return NULL otherwise.
*/
{
  a_symbol_ptr                      template_sym;
  a_template_symbol_supplement_ptr  tssp;
  a_template_ptr                    class_template = NULL;

  type = skip_typedefs_not_dependent_decltypes(type);
  if (is_immediate_class_type(type) && 
      type->variant.class_struct_union.is_template_class) {
    /* The class is an instantiation or specialization -- but it might be a
       nested class within a template class. */
    template_sym = class_template_for_type(type);
    if (template_sym != NULL) {
      /* For a partial specialization, go to the primary template. */
      template_sym = primary_template_of(template_sym);
      tssp = template_sym->variant.template_info;
      check_assertion(tssp != NULL);
      class_template = tssp->il_template_entry;
    }  /* if */
  }  /* if */

  return class_template;
}  /* class_template_of */


/* Returns TRUE if ns is the "std" namespace. */
#define is_namespace_std(ns) \
  ((ns)->is_std)

/* Returns TRUE if scp is the source correspondence for an entity that is a
   member of the "std" namespace. */
#define is_source_corresp_in_namespace_std(scp)         \
  (scp_is_namespace_member(scp) &&                      \
   is_namespace_std(scp_parent_namespace(scp)))

/* Returns TRUE if il_entry is (immediately) within the "std" namespace. */
#define is_in_namespace_std(il_entry)                                \
  (is_source_corresp_in_namespace_std(&((il_entry)->source_corresp)))


static a_boolean is_Sa_substitution(a_template_ptr  template_ptr)
/*
Return TRUE if template_ptr represents ::std::allocator and thus is
eligible for the `Sa' special substitution.
*/
{
  return is_in_namespace_std(template_ptr) && has_name(template_ptr) &&
                   strcmp(template_ptr->source_corresp.name, "allocator") == 0;
}  /* is_Sa_substitution */


static a_boolean is_Sb_substitution(a_template_ptr  template_ptr)
/*
Return TRUE if template_ptr represents ::std::basic_string and thus is
eligible for the `Sb' special substitution.
*/
{
  return is_in_namespace_std(template_ptr) && has_name(template_ptr) &&
               strcmp(template_ptr->source_corresp.name, "basic_string") == 0;
}  /* is_Sb_substitution */


static a_boolean is_char_type(a_type_ptr type)
/*
Return TRUE if the indicated type is "char".
*/
{
  a_type_ptr char_type = integer_type((an_integer_kind)ik_char);

  return identical_types(type, char_type);
}  /* is_char_type */


static a_boolean is_special_char_template(a_type_ptr    type,
					  a_const_char  *template_name)
/*
Return TRUE if type represents ::std::`template_name'<char>. 
*/
{
  a_template_ptr      template_ptr;
  a_template_arg_ptr  arg;
  a_type_ptr          arg_type;
  a_boolean           result = FALSE;

  /* Check that type is an instance of ::std::`template_name'. */
  template_ptr = class_template_of(type);
  if (template_ptr != NULL && 
      is_in_namespace_std(template_ptr) &&
      has_name(template_ptr) &&
      strcmp(template_ptr->source_corresp.name, template_name) == 0) {
    /* Check that the first template argument is char and that there is
       only one argument. */
    type = skip_typerefs(type);
    arg = type->variant.class_struct_union.extra_info->template_arg_list;
    if (arg != NULL && arg->kind == (a_templ_arg_kind)tak_type &&
        arg->next == NULL) {
      arg_type = arg->variant.type;
      if (is_char_type(arg_type)) {
        result = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* is_special_char_template */

  
static a_boolean is_Ss_substitution(a_type_ptr  type)
/*
Return TRUE if type represents ::std::string, a.k.a.
::std::basic_string<char, ::std::char_traits<char>, ::std_allocator<char> >
and thus is eligible for the `Ss' substitution.
*/
{
  a_template_arg_ptr  arg;
  a_type_ptr          arg_type;
  a_template_ptr      tmpl;
  a_boolean           result = FALSE;

  /* First check that type is a template instance of ::std::basic_string. */
  tmpl = class_template_of(type);
  if (tmpl != NULL && is_Sb_substitution(tmpl)) {
    /* Now check the template arguments. */
    /* The first argument should be char. */
    type = skip_typerefs(type);
    arg = type->variant.class_struct_union.extra_info->template_arg_list;
    if (arg != NULL && arg->kind == (a_templ_arg_kind)tak_type) {
      arg_type = arg->variant.type;
      if (is_char_type(arg_type)) {
        /* The second argument should be ::std::char_traits<char>. */
        arg = arg->next;
        if (arg != NULL && arg->kind == (a_templ_arg_kind)tak_type) {
          arg_type = arg->variant.type;
          if (is_special_char_template(arg_type, "char_traits")) {
            /* The third argument should be ::std::allocator<char> and
               should be the last argument. */
            arg = arg->next;
            if (arg != NULL && arg->kind == (a_templ_arg_kind)tak_type &&
                arg->next == NULL) {
              arg_type = arg->variant.type;
              if (is_special_char_template(arg_type, "allocator")) {
                result = TRUE;
              }  /* if */
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* is_Ss_substitution */


static a_boolean is_stream_substitution(a_type_ptr    type,
					a_const_char  *stream_name)
/*
Return TRUE if type represents 
::std::`stream_name'<char, ::std::char_traits<char> >
and thus is eligible for a special substitution.
*/
{
  a_template_ptr      template_ptr;
  a_template_arg_ptr  arg;
  a_type_ptr          arg_type;
  a_boolean           result = FALSE;

  /* Check that type is an instance of ::std::`stream_name'. */
  template_ptr = class_template_of(type);
  if (template_ptr != NULL &&
      is_in_namespace_std(template_ptr) &&
      has_name(template_ptr) &&
      strcmp(template_ptr->source_corresp.name, stream_name) == 0) {
    /* Now check the template arguments. */
    /* The first argument should be char. */
    type = skip_typerefs(type);
    arg = type->variant.class_struct_union.extra_info->template_arg_list;
    if (arg != NULL && arg->kind == (a_templ_arg_kind)tak_type) {
      arg_type = arg->variant.type;
      if (is_char_type(arg_type)) {
        /* The second argument should be ::std::char_traits<char> and
           should be the last argument. */
        arg = arg->next;
        if (arg != NULL && arg->kind == (a_templ_arg_kind)tak_type &&
            arg->next == NULL) {
          arg_type = arg->variant.type;
          if (is_special_char_template(arg_type, "char_traits")) {
            result = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* is_stream_substitution */

#if ABI_COMPATIBILITY_VERSION >= 405 && GNU_EXTENSIONS_ALLOWED

static a_type_ptr topmost_dependent_typeof(a_type_ptr           tp,
                                           a_type_qualifier_set *qualifiers)
/*
Returns the topmost dependent typeof typeref for the specified type or NULL if
the type has no dependent typeof typeref.  *qualifiers is set to a bit vector
of cv-qualifiers that were found while stripping typerefs.  Since this is never
called in C mode, if tp is an array, check for a qualifier on the element type.
*/
{
  *qualifiers = TQ_NONE;

  check_assertion(!C_mode());
  for (;;) {
    if (tp->kind == (a_type_kind)tk_typeref) {
      if ((is_typeref_kind(tp, trk_is_typeof_with_expression) ||
           is_typeref_kind(tp, trk_is_typeof_with_type_operand)) &&
          tp->variant.typeref.is_dependent_type_operator) {
        /* Found a dependent typeof; return it. */
        break;
      } else {
        /* May be a typedef or a qualification. */
        *qualifiers |= tp->variant.typeref.qualifiers;
        tp = tp->variant.typeref.type;
      }  /* if */
    } else if (tp->kind == (a_type_kind)tk_array) {
      /* Check the array element type. */
      tp = tp->variant.array.element_type;
    } else {
      /* Type doesn't have a typeof typeref. */
      tp = NULL;
      break;
    }  /* if */
  }  /* for */
  return tp;
}  /* topmost_dependent_typeof */


static a_boolean identical_types_differ_in_typeof(a_type_ptr type_1,
                                                  a_type_ptr type_2)
/*
Returns TRUE if the two types (which the caller ensures are "identical") differ
in that one contains a typeof typeref and the other doesn't, for example,
typeof(T) and T are "identical", but not for mangling purposes.
Used only in g++ emulation mode.
*/
{
  a_type_ptr           typeof_type_1, typeof_type_2;
  a_type_qualifier_set type_1_quals, type_2_quals;
  a_boolean            result;

  check_assertion(gpp_mode);
  if (type_1 == type_2) {
    /* Identical types can't differ. */
    result = FALSE;
  } else {
    typeof_type_1 = topmost_dependent_typeof(type_1, &type_1_quals);
    typeof_type_2 = topmost_dependent_typeof(type_2, &type_2_quals);
    if (typeof_type_1 == NULL && typeof_type_2 == NULL) {
      /* Neither type has a typeof. */
      result = FALSE;
    } else if (typeof_type_1 != NULL && typeof_type_2 != NULL) {
      /* Both types have a typeof. */
      if (type_1_quals != type_2_quals) {
        /* Different qualifiers on top of the typeofs. */
        result = TRUE;
      } else {
        result = identical_types_differ_in_typeof(
                                          typeof_type_1->variant.typeref.type,
                                          typeof_type_2->variant.typeref.type);
      }  /* if */
    } else {
      /* One has a typeof and the other doesn't. */
      return TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* identical_types_differ_in_typeof */

#endif /* ABI_COMPATIBILITY_VERSION >= 405 && GNU_EXTENSIONS_ALLOWED */

/*
Helper macro for same_types_for_mangling_purposes below (needed because
of conditional compilation).
*/
#if ABI_COMPATIBILITY_VERSION >= 405 && GNU_EXTENSIONS_ALLOWED
/* One type is a dependent typeof typeref and the other type isn't; these get
separate substitutions (the mangling for __typeof is non-standard).  decltype
and __underlying_type don't have this problem because the underlying type isn't
part of the mangling. */
#define and_not_different_only_in_typeof(type1, type2) \
  && !(gpp_mode && identical_types_differ_in_typeof((type1), (type2)))
#else /* !(ABI_COMPATIBILITY_VERSION >= 405 && GNU_EXTENSIONS_ALLOWED) */
#define and_not_different_only_in_typeof(type, sp) /**/
#endif /* ABI_COMPATIBILITY_VERSION >= 405 && GNU_EXTENSIONS_ALLOWED */

/*
Local macro that returns TRUE if the specified type and substitution should be
considered to be a match (for mangling purposes).  Relies on is_pack_expansion
and opts being available in the context in which it appears.
*/
#define same_types_for_mangling_purposes(type, sp) \
  (is_pack_expansion == sp->is_pack_expansion && \
   identical_types_full((type), (a_type_ptr)((sp)->entity), opts) \
   and_not_different_only_in_typeof((type), (a_type_ptr)((sp)->entity)))

static a_boolean record_substitution_for_type(a_type_ptr type);

static a_boolean type_has_abbreviation(a_type_ptr type)
/*
Returns TRUE if the specified type has an IA-64 substitution abbreviation
for it (e.g., ::std::basic_string).
*/
{
  a_boolean result = FALSE;

  if (is_class_struct_type(type) && is_in_namespace_std(type)) {
    result = is_Ss_substitution(type) ||
             is_stream_substitution(type, "basic_istream") ||
             is_stream_substitution(type, "basic_ostream") ||
             is_stream_substitution(type, "basic_iostream");
  }  /* if */
  return result;
}  /* type_has_abbreviation */


static a_boolean add_substitution_if_available_full(
                            char                     *entity,
                            an_il_entry_kind         kind,
                            a_boolean                is_pack_expansion,
                            a_boolean                test,
                            a_boolean                *is_standard_substitution,
                            a_mangling_control_block *mctl)
/*
If there is a substitution available for entity, add it to the mangled name
and return TRUE.  Otherwise return FALSE.  The kind indicates the kind of
entity processed.  is_pack_expansion specifies whether the caller desires
a substitution for the pack expanded type or the non-pack expanded type (and
should be FALSE for non-type entities).  If test is TRUE, just determine
whether a substitution is available; do not put it out.  Set
*is_standard_substitution to TRUE if the available substitution is a "standard"
substitution (and is unset otherwise).
*/
{
  a_substitution_ptr   sp;
  a_boolean            result = FALSE, secondary_tu;
  a_const_char         *str = NULL;
  a_type_kind          type_kind = tk_error;
  a_type_ptr           type, utype = NULL;
  a_boolean            type_kind_is_struct_or_class = FALSE;

  /* Nothing to do if substitution processing is temporarily suspended. */
  if (mctl->suppress_substitutions != 0) goto end_of_routine;
  if (kind == iek_type) {
    entity = canonical_substitution_entity((a_type_ptr)entity);
    if (!record_substitution_for_type((a_type_ptr)entity)) {
      /* This type doesn't get a substitution, so don't bother to look for
         one. */
      goto end_of_routine;
    }  /* if */
  }  /* if */
  if (((a_source_correspondence*)entity)->on_mangling_substitution_list) {
    /* The entity has already had a substitution registered for it; see if
       a match can be found (it may not be because is_pack_expansion may not
       match). */
    for (sp = substitution_cache[subst_hash(entity)];
         sp != NULL;
         sp = sp->next_bucket) {
      if (sp->entity == entity && sp->is_pack_expansion == is_pack_expansion) {
        result = TRUE;
        /* We found a direct substitution for this entity. */
        if (!test) add_substitution_index_to_mangled_name(sp->index, mctl);
        goto end_of_routine;
      }  /* if */
    }  /* for */
    /* Only certain types (ones that aren't an exact match) and secondary IL
       entities should get here. */
#if EXPENSIVE_CHECKING
    check_assertion(kind == iek_type || secondary_translation_unit_seen());
#endif /* EXPENSIVE_CHECKING */
  }  /* if */
  /* See if the entity is one of the special entities for which an
     abbreviation exists. */
  switch (kind) {
    case iek_type:
      {
        type = (a_type_ptr)entity;
        utype = skip_typerefs(type);
        type_kind = utype->kind;
        type_kind_is_struct_or_class = (type_kind == (a_type_kind)tk_struct ||
                                        type_kind == (a_type_kind)tk_class);
        if (!type_kind_is_struct_or_class || !is_in_namespace_std(type)) {
          /* For speed. */
        } else if (is_Ss_substitution(type)) {
          /* ::std::string. */
          str = "Ss";
          break;
        } else if (is_stream_substitution(type, "basic_istream")) {
          str = "Si";
          break;
        } else if (is_stream_substitution(type, "basic_ostream")) {
          str = "So";
          break;
        } else if (is_stream_substitution(type, "basic_iostream")) {
          str = "Sd";
          break;
        }  /* if */
      }
      break;
    case iek_template:
      { a_template_ptr  templ = (a_template_ptr)entity;
        if (!is_source_corresp_in_namespace_std(&templ->source_corresp)) {
          /* For speed. */
        } else if (is_Sa_substitution(templ)) {
          str = "Sa";
        } else if (is_Sb_substitution(templ)) {
          str = "Sb";
        }  /* if */
      }
      break;
    case iek_namespace:
      if (is_namespace_std((a_namespace_ptr)entity)) {
        str = "St";
      }  /* if */
      break;
    default:
      break;
  }  /* switch */
  if (str != NULL) {
    /* There is a special substitution that applies. */
    result = TRUE;
    *is_standard_substitution = TRUE;
    if (!test) add_str_to_mangled_name(str, mctl);
  } else {
    /* Otherwise, see if there is an existing substitution for something
       that appears earlier in the mangled name.  Most cases that have
       substitutions have been handled above, but in some cases, two types
       may have different addresses but the same mangling.  Two such cases are
       nonreal types and cv-qualified types where the underlying types are
       equivalent (but not the same, e.g., because of a typedef).  Those are
       handled here. */
    an_itf_flag_set  opts;
    secondary_tu = secondary_translation_unit_seen();
    if (secondary_tu || kind == iek_type) {
      if (kind == iek_type && !secondary_tu &&
          type_kind_is_struct_or_class &&
          !type->source_corresp.on_mangling_substitution_list &&
          !utype->variant.class_struct_union.is_nonreal_class &&
          !(utype->source_corresp.on_mangling_substitution_list ||
            type_has_abbreviation(utype))) {
        /* Searching for (unlikely) cases that get substitutions that aren't
           covered by the code above can be time consuming -- this test tries
           to detect cases where the code below will never find a substitution.
           */
        goto end_of_routine;
      }  /* if */
      opts = ITF_EXACT_DECLTYPE_EXPR_MATCH_REQUIRED;
#if ABI_COMPATIBILITY_VERSION >= 406 && ABI_COMPATIBILITY_VERSION < 413
      /* Requiring an exact template parameter type had been introduced
         as a "fix" for an alias template issue, but that resulted in
         incorrect substitutions and the alias template issue has been
         fixed elsewhere. */
      opts |= ITF_EXACT_EQUIVALENCE;
#endif /* ABI_COMPATIBILITY_VERSION >= 406 && ABI_COMPATIBILITY_VERSION < 413*/
#if ABI_COMPATIBILITY_VERSION >= 405 && GNU_EXTENSIONS_ALLOWED
      opts |= ITF_EXACT_DOES_NOT_RETURN_MATCH_REQUIRED;
#endif /* ABI_COMPATIBILITY_VERSION >= 405 && GNU_EXTENSIONS_ALLOWED */
      for (sp = mctl->first_substitution; sp != NULL; sp = sp->next) {
        if (sp->kind == kind) {
          if (kind == iek_type) {
            a_type_ptr       stype = (a_type_ptr)sp->entity;
            a_type_ptr       ustype = skip_typerefs(stype);
            if (ustype->kind != type_kind) {
              continue;
            } else if (!secondary_tu) {
              if (type_kind_is_struct_or_class && utype != ustype) {
                a_const_char  *n1, *n2;
                if (!utype->variant.class_struct_union.is_nonreal_class ||
                    !ustype->variant.class_struct_union.is_nonreal_class) {
                  continue;
                }  /* if */
                n1 = unmangled_name_of(&utype->source_corresp);
                n2 = unmangled_name_of(&ustype->source_corresp);
                if (n1 != NULL && n2 != NULL && strcmp(n1, n2) != 0) {
                  continue;
                }  /* if */
              }  /* if */
            }  /* if */
            if (same_types_for_mangling_purposes((a_type_ptr)entity, sp)) {
              result = TRUE;
            }  /* if */
          } else if (entity == sp->entity) {
            result = TRUE;
          } else if (secondary_tu) {
            a_trans_unit_corresp_ptr  tcp1, tcp2;
            tcp1 = trans_unit_corresp_of_unknown_entry(entity);
            tcp2 = trans_unit_corresp_of_unknown_entry(sp->entity);
            if (tcp1 == tcp2 && tcp1 != NULL) {
              result = TRUE;
            }  /* if */
          }  /* if */
        }  /* if */
        if (result) {
          /* We found a substitution for this entity. */
          if (!test) add_substitution_index_to_mangled_name(sp->index, mctl);
          break;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
end_of_routine:;
#if DEBUG && EXPENSIVE_CHECKING
  if (result && db_flag_is_set("substitutions") && str == NULL) {
    fprintf(f_debug, "using S%c: <%s> %s",
                     (sp->index == 0 ? ' ' :
                      sp->index < 36 ? base_36_digits[sp->index-1] : '?'),
                     il_entry_kind_names[(int)sp->kind],
                     sp->is_pack_expansion ? " [pack_expansion]" : "");
    if (sp->kind == (an_il_entry_kind)iek_type) {
      db_abbreviated_type((a_type_ptr)sp->entity);
    } else if (sp->kind == (an_il_entry_kind)iek_template) {
      db_template_name((a_template_ptr)sp->entity);
    }  /* if */
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG && EXPENSIVE_CHECKING */
  return result;
}  /* add_substitution_if_available_full */

#undef same_types_for_mangling_purposes
#undef and_not_different_only_in_typeof

static a_boolean add_substitution_if_available(
                                    char                     *entity,
                                    an_il_entry_kind         kind,
                                    a_boolean                is_pack_expansion,
                                    a_mangling_control_block *mctl)
/*
If there is a substitution available for entity, add it to the mangled name
and return TRUE.  Otherwise return FALSE.  The kind indicates the kind of
entity processed.  is_pack_expansion specifies whether the caller desires
a substitution for the pack expanded type or the non-pack expanded type (and
should be FALSE for non-type entities).
*/
{
  a_boolean is_special_substitution;

  return add_substitution_if_available_full(entity, kind, is_pack_expansion,
                                            /*test=*/FALSE,
                                            &is_special_substitution, mctl);
}  /* add_substitution_if_available */


static a_boolean substitution_available(
                                    char                     *entity,
                                    an_il_entry_kind         kind,
                                    a_boolean                is_pack_expansion,
                                    a_mangling_control_block *mctl)
/*
If there is a substitution available for entity (which has kind "kind", and
matching is_pack_expansion value for types), return TRUE.  Do not add the
substitution to the mangled name.
*/
{
  a_boolean is_special_substitution;

  return add_substitution_if_available_full(entity, kind, is_pack_expansion,
                                            /*test=*/TRUE,
                                            &is_special_substitution, mctl);
}  /* substitution_available */


static void add_prefix_for_local_entity(a_routine_ptr            routine,
                                        a_mangling_control_block *mctl)
/*
Add a prefix indicating the given routine as the containing function
for a local entity, for the IA-64 ABI.
*/
{
  a_boolean suppress_param_encoding = FALSE;
  a_boolean suppress_parent_encoding = FALSE;

  add_to_mangled_name('Z', mctl);
  /* extern "C" functions don't need much mangling.  They do get the
     length preceding the name. */
  if (!function_name_mangling_needed(routine, &suppress_param_encoding)) {
    suppress_param_encoding = TRUE;
    suppress_parent_encoding = TRUE;
  }  /* if */
  mangled_function_name(routine,
                        suppress_param_encoding,
                        suppress_parent_encoding,
                        /*force_primary_name=*/TRUE,
                        /*force_individuation=*/FALSE,
                        /*base_name_offset=*/(sizeof_t *)NULL,
                        mctl);
  add_to_mangled_name('E', mctl);
}  /* add_prefix_for_local_entity */


static void add_prefix_for_local_type(a_type_ptr               type,
                                      a_mangling_control_block *mctl)
/*
type is a local type.  Output the prefix indicating the routine containing
the type, for the IA-64 ABI.
*/
{
  a_routine_ptr enclosing_routine = enclosing_routine_for_local_type(type);
  add_prefix_for_local_entity(enclosing_routine, mctl);
}  /* add_prefix_for_local_type */

#endif /* IA64_ABI */

static void add_mangling_for_default_arg_in_local_type(
                                   a_type_ptr               type,
                                   a_routine_ptr            *enclosing_routine,
                                   a_mangling_control_block *mctl)
/*
type is a lambda closure defined in a default argument.  Output the mangling
(a prefix in the IA-64 ABI, a suffix in the Cfront ABI) indicating the routine
containing the type (IA-64 ABI only) as well as the default argument number.
Return the routine in which the lambda appears in a default argument in
*enclosing_routine if enclosing_routine is not NULL.
*/
{
  a_routine_ptr                 routine;
  a_routine_type_supplement_ptr rtsp;
  a_param_type_ptr              param;
  an_il_entity_list_entry_ptr   entry;
  unsigned long                 param_num;
  a_class_type_supplement_ptr   ctsp = class_type_supp(type);
#if CHECKING
  a_class_symbol_supplement_ptr cssp = symbol_supplement_for_class(type);
#endif /* CHECKING */

  check_assertion(type_is_lambda_closure(type) &&
                  cssp != NULL &&
                  cssp->lambda_immediately_inside_default_arg_expression);
  check_assertion(!type_is_lambda_in_initializer(type));
  routine = ctsp->lambda_parent.routine;
  check_assertion(routine != NULL);
  if (enclosing_routine != NULL) *enclosing_routine = routine;
  rtsp = routine->type->variant.routine.extra_info;
  check_assertion(rtsp != NULL && rtsp->prototyped);
  /* Count the number of parameters.  The encoding specifies that numbers
     are specified from the end of the parameter list (i.e., the last parameter
     is numbered zero). */
  for (param = rtsp->param_type_list, param_num = 0;
       param != NULL;
       param = param->next, param_num++) {}
  /* For each parameter. */
  for (param = rtsp->param_type_list;
       param != NULL;
       param = param->next, param_num--) {
    /* For each entity defined in a default arg (currently only lambdas). */
    for (entry = param->entities_defined_in_default_arg;
         entry != NULL;
         entry = entry->next) {
      if (type == (a_type_ptr)entry->entity.ptr) {
#if IA64_ABI
        /* Mangle with the routine and parameter number. */
        add_prefix_for_local_entity(routine, mctl);
        add_to_mangled_name('d', mctl);
        if (param_num > 1) {
          add_number_to_mangled_name(param_num-2, mctl);
        }  /* if */
        add_to_mangled_name('_', mctl);
#else /* !IA64_ABI */
        /* Add default argument number (routine is added by the caller). */
        add_number_to_mangled_name(param_num, mctl);
#endif /* IA64_ABI */
        goto done;
      }  /* if */
    }  /* for */
  }  /* for */
  /* Didn't find lambda in the parameter list. */
  unexpected_condition();
done:
  return;
}  /* add_mangling_for_default_arg_in_local_type */

#if !IA64_ABI

/*
Information about a spot where space was reserved by
reserve_space_for_length for later use by fill_in_length to fill in
a leading length.
*/
typedef struct a_length_reservation {
  sizeof_t	start_position;
			/* The offset in the mangling_text_buffer of the
			   first character of the space reserved for insertion
			   of the length. */
  sizeof_t	start_length;
			/* The length of the mangled name (not counting the
			   leftover spaces, which will be removed later)
			   preceding the length, used to compute the length
			   of the text following. */
} a_length_reservation;


static void reserve_space_for_length(
                                  a_length_reservation     *length_reservation,
                                  a_mangling_control_block *mctl)
/*
Reserve some space in the mangled name so that we can insert a length
later.  Return information on the position of the reserved space in
*length_reservation.
*/
{
  int i;

  length_reservation->start_position = mangling_text_buffer->size;
  length_reservation->start_length = mctl->length;
  /* Leave room for lengths of up to 9,999,999. */
#define NUM_CHARS_RESERVED_FOR_LENGTH 7
  /* Fill the space with blanks, which cannot be part of a valid mangled
     name.  We'll overwrite some of those blanks with the actual length
     determined later.  The leftover blanks will be removed at the end of
     mangling. */
  for (i = 1; i <= NUM_CHARS_RESERVED_FOR_LENGTH; i++) {
    add_to_mangled_name(' ', mctl);
  }  /* for */
  mctl->length -= NUM_CHARS_RESERVED_FOR_LENGTH;
  mctl->num_leftover_spaces += NUM_CHARS_RESERVED_FOR_LENGTH;
}  /* reserve_space_for_length */


static void fill_in_length(a_length_reservation     *length_reservation,
                           a_mangling_control_block *mctl)
/*
Fill in the length of an item in the space previously reserved by a
call of reserve_space_for_length.  *length_reservation contains the
information returned from that call.
*/
{
  /* Determine the length, and the number of digits needed to
     represent the length. */
  sizeof_t        length = mctl->length - length_reservation->start_length;
  a_number_buffer buffer(length);
  if (buffer.length() > NUM_CHARS_RESERVED_FOR_LENGTH) {
    catastrophe(ec_mangled_name_too_long);
  }  /* if */

  /* Determine the position of the start of the length in the buffer. */
  char *length_pos = mangling_text_buffer->buffer +
                     length_reservation->start_position;
  /* Copy the length. */
  (void)memcpy(length_pos, buffer.as_temp_characters(), buffer.length());
  /* The characters overwritten are no longer leftover spaces. */
  mctl->length += buffer.length();
  mctl->num_leftover_spaces -= buffer.length();
}  /* fill_in_length */

#endif /* !IA64_ABI */

static void mangled_encoding_for_type_qualifiers(
                                           a_type_qualifier_set     qualifiers,
                                           a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the cv-qualifiers (if any)
in the set "qualifiers".
*/
{
#if IA64_ABI
  /* Note that the order matters: restrict, volatile, const, Atomic must be in
     that order. */
  if (qualifiers & TQ_RESTRICT) {
    add_to_mangled_name(MANGLING_CODE_FOR_RESTRICT, mctl);
  }  /* if */
  if (qualifiers & TQ_VOLATILE) {
    add_to_mangled_name(MANGLING_CODE_FOR_VOLATILE, mctl);
  }  /* if */
  if (qualifiers & TQ_CONST) {
    add_to_mangled_name(MANGLING_CODE_FOR_CONST, mctl);
  }  /* if */
#else /* !IA64_ABI */
  if (qualifiers & TQ_CONST) {
    add_to_mangled_name(MANGLING_CODE_FOR_CONST, mctl);
  }  /* if */
  if (qualifiers & TQ_VOLATILE) {
    add_to_mangled_name(MANGLING_CODE_FOR_VOLATILE, mctl);
  }  /* if */
#ifdef MANGLING_CODE_FOR_RESTRICT
  if (qualifiers & TQ_RESTRICT) {
    add_str_to_mangled_name(MANGLING_CODE_FOR_RESTRICT, mctl);
  }  /* if */
#endif /* ifdef MANGLING_CODE_FOR_RESTRICT */
#endif /* IA64_ABI */
  if (qualifiers & TQ_C11_ATOMIC) {
    add_str_to_mangled_name(MANGLING_STRING_FOR_ATOMIC, mctl);
  }  /* if */
}  /* mangled_encoding_for_type_qualifiers */


static void mangled_encoding_for_parameter_types(
                                                a_type_ptr               type,
                                                a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the parameters of function
type "type".
*/
{
  a_routine_type_supplement_ptr rtsp;
  a_param_type_ptr              param;
#if !IA64_ABI
  a_param_type_ptr              existing_param;
  unsigned long                 existing_param_num, num_matching_types;
#endif /* !IA64_ABI */

  /* The encoding for parameter types is as follows:
       (1)  For each parameter, the encoding for the type.  Except in the IA64
            ABI, if a parameter has a type that has appeared already in the
            parameter list, "Tn" is used to repeat the type of parameter "n"
            ("n" can be a multi-digit number; the first parameter is numbered
            1).  If several consecutive parameters have the same type as a
            previous parameter, "Nmn" is used to indicate "m" repetitions of
            the type of parameter "n" ("n" is as for "Tn"; "m" is a one-digit
            number, so a maximum of 9 repetitions is possible).
            If the parameter list is empty, "v" for "void".
       (2)  If the parameter list ends with an ellipsis, a code for the
            ellipsis.
  */
  rtsp = type->variant.routine.extra_info;
  param = rtsp->param_type_list;
  if (param == NULL) {
    /* Void parameter list. */
#if IA64_ABI
    /* No "v" if there is an ellipsis. */
    if (!rtsp->has_ellipsis)
#endif /* IA64_ABI */
    /* Do not add code here. */
    {
      add_to_mangled_name('v', mctl);
    }  /* if */
  } else {
    /* Output the parameter types. */
    /*lint --e{850} param modified in loop */
    for (; param != NULL; param = param->next) {
#if !IA64_ABI
      /* See if the parameter type is the same as any existing parameter
         type. */
#if !CFRONT_OBJECT_CODE_COMPATIBILITY
      /* Only check the first 9 parameters to avoid multi-digit
         numbers which would cause ambiguous mangling. */
#endif /* !CFRONT_OBJECT_CODE_COMPATIBILITY */
      for (existing_param = rtsp->param_type_list, existing_param_num = 1;
           existing_param != param
#if !CFRONT_OBJECT_CODE_COMPATIBILITY
                                   && existing_param_num < 10
#endif /* !CFRONT_OBJECT_CODE_COMPATIBILITY */
                                                             ;
           existing_param = existing_param->next, existing_param_num++) {
        if (f_types_are_compatible(existing_param->type, param->type,
                                   TCF_EXACT_DECLTYPE_EXPR_MATCH_REQUIRED)) {
          /* Found a type that is being reused.  See if there are more
             instances following this one, in which case we can use the "Nmn"
             encoding.  Stop when 9 matches are found, since that's the most
             that can be encoded in a single "Nmn" sequence. */
          for (num_matching_types = 1;
               num_matching_types < 9 && param->next != NULL &&
                 f_types_are_compatible(existing_param->type,
                                        param->next->type,
                                       TCF_EXACT_DECLTYPE_EXPR_MATCH_REQUIRED);
               num_matching_types++, param = param->next) {}
          if (num_matching_types == 1) {
            /* Only one match, so use the "Tn" form. */
            add_to_mangled_name('T', mctl);
          } else {
            /* More than one match, so use the "Nmn" form. */
            add_to_mangled_name('N', mctl);
            /* Output the "m" (repetition count). */
            add_number_to_mangled_name(num_matching_types, mctl);
          }  /* if */
          /* Output the "n" (existing parameter number). */
          add_number_to_mangled_name(existing_param_num, mctl);
          goto arg_done;
        }  /* if */
      }  /* for */
#endif /* !IA64_ABI */
      /* The parameter type does not match any of the previous parameter
         types, so just put it out.  Include an indication of whether the
         parameter represents a pack expansion. */
      mangled_encoding_for_type_with_pack_expansion(param->type,
                              (a_boolean)(param->pack_expansion_descr != NULL),
                              mctl);
      if (has_attr(ak_pass_object_size, param->attributes)) {
        a_constant    *cp;
        a_boolean     ovflo;
        an_attribute  *ap = find_attribute(ak_pass_object_size,
                                           param->attributes);
        add_str_to_mangled_name(MANGLING_STRING_FOR_PASS_OBJECT_SIZE, mctl);
        check_assertion(ap != NULL && ap->arguments != NULL &&
                        ap->arguments->kind == aak_constant);
        cp = ap->arguments->variant.constant;
        check_assertion(constant_is(cp, ck_integer));
        add_number_to_mangled_name(
                  (unsigned long)value_of_integer_constant(cp, &ovflo), mctl);
      }  /* if */
#if !IA64_ABI
arg_done:;
#endif /* !IA64_ABI */
    }  /* for */
  }  /* if */
  /* Output the final "e" (or "z" in the IA64 ABI) for an ellipsis. */
  if (rtsp->has_ellipsis) {
    add_to_mangled_name(MANGLING_CODE_FOR_ELLIPSIS, mctl);
  }  /* if */
}  /* mangled_encoding_for_parameter_types */


static void mangled_encoding_for_ref_qualifier(a_type_ptr               type,
                                               a_mangling_control_block *mctl)
/*
Add an indication of the ref-qualifier (if any) for the specified function
type.
*/
{
  a_const_char                  *s = NULL;
  a_routine_type_supplement_ptr rtsp = type->variant.routine.extra_info;

  check_assertion(type->kind == (a_type_kind)tk_routine);
  if (rtsp->ref_qualifiers == (a_ref_qualifier_kind)rqk_lvalue) {
    s = MANGLING_STRING_FOR_REFERENCE;
  } else if (rtsp->ref_qualifiers == (a_ref_qualifier_kind)rqk_rvalue) {
    s = MANGLING_STRING_FOR_RVALUE_REFERENCE;
  } else {
    check_assertion(rtsp->ref_qualifiers == (a_ref_qualifier_kind)rqk_default);
  }  /* if */
  if (s != NULL) {
#if !IA64_ABI
    add_to_mangled_name('_', mctl);
#endif /* !IA64_ABI */
    add_str_to_mangled_name(s, mctl);
  }  /* if */
}  /* mangled_encoding_for_ref_qualifier */


static void mangled_encoding_for_exception_specification(
                                      a_type_ptr               type,
                                      a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the exception specification (if any)
on the function or member function type "type".
*/
{
  an_exception_specification_ptr  esp = type->variant.routine.extra_info
                                            ->exception_specification;

  if (esp == NULL || esp->indeterminate) {
    /* Nothing to mangle. */
  } else if (esp->is_noexcept &&
             esp->variant.noexcept_arg != NULL &&
             constant_is(esp->variant.noexcept_arg, ck_template_param)) {
    check_assertion(!esp->arg_cached && !esp->indeterminate);
    add_str_to_mangled_name(MANGLING_STRING_FOR_NOEXCEPT_EXPR, mctl);
      mangled_encoding_for_constant(esp->variant.noexcept_arg,
                                    /*old_form=*/FALSE,
                                    /*in_dependent_expr=*/FALSE,
                                    /*suppress_address_of=*/FALSE,
                                    mctl);
#if IA64_ABI
      add_to_mangled_name('E', mctl);
#endif /* IA64_ABI */
  } else if (esp->throw_any) {
  } else if (is_nothrow_spec(esp)) {
    add_str_to_mangled_name(MANGLING_STRING_FOR_NOEXCEPT, mctl);
  } else {
    unexpected_condition();
  }  /* if */
}  /* mangled_encoding_for_exception_specification */


static void mangled_encoding_for_function_type(
                               a_type_ptr               type,
                               a_boolean                do_return_type,
                               a_boolean                mangling_function_name,
                               a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the function type "type".  The return
type of the function is encoded if do_return_type is TRUE.  If
mangling_function_name is TRUE, this routine is being called to mangle a
function with the specified type.  In that case, in the IA-64 ABI, this routine
basically emits the <bare-function-type> and not the <function-type>.  In the
Cfront ABI, the function type markers, as well as whether or not the type is
extern "C", are emitted in either case.  The exception specification, however,
is not emitted when mangling_function_name is TRUE (to provide some backwards
compatibility with previous versions).  The type must not have been lowered
(lowering can modify the parameters or return type).
*/
{
  a_boolean do_markers;

  check_assertion(type->kind == (a_type_kind)tk_routine);
#if DO_IL_LOWERING
  if (il_lowering_flag_of(type)) {
    /* If the type has already been lowered, see if an un-lowered version of
       the type has been saved. */
    type = type->variant.routine.unlowered_type;
    check_assertion(type != NULL && !il_lowering_flag_of(type));
  }  /* if */
#endif /* DO_IL_LOWERING */
#if IA64_ABI
  /* In the IA-64 ABI, "markers" are only emitted for true function types, not
     when mangling a function name. */
  do_markers = !mangling_function_name;
#else /* !IA64_ABI */
  /* In the Cfront ABI, "markers" are always emitted (i.e., for both function
     types and names), but see below for exception specifications. */
  do_markers = TRUE;
#endif /* IA64_ABI */
  /* The encoding for a function type is "F" followed by the encoding
     for the parameter types.  mangled_function_name takes care of putting
     out additional information preceding the "F" if the function is a
     member function. */
  if (do_markers) {
#if IA64_ABI
    if (exc_spec_in_func_type) {
      /* In some modes (as per the C++17 standard), the exception
         specification must be encoded.  In the IA-64 ABI, the exception
         specification is emitted before the "F". */
      mangled_encoding_for_exception_specification(type, mctl);
    }  /* if */
#endif /* IA64_ABI */
    /* Start with the "F" indicating a function type. */
    add_to_mangled_name('F', mctl);
#if !IA64_ABI
    /* Add mangled encoding for a ref-qualifier, if any. */
    mangled_encoding_for_ref_qualifier(type, mctl);
    if (exc_spec_in_func_type && !mangling_function_name) {
      /* In some modes (as per the C++17 standard), the exception
         specification must be encoded.  In the Cfront ABI, the exception
         specification is emitted after the "F" (and ref-qualifier).  Note
         that the exception specification is not added to mangled function
         names (for backward compatibility reasons). */
      mangled_encoding_for_exception_specification(type, mctl);
    }  /* if */
#endif /* !IA64_ABI */
    if (c_and_cpp_function_types_are_distinct &&
        type->variant.routine.extra_info->routine_name_linkage ==
                                           (a_name_linkage_kind)nlk_external) {
      /* The function type is marked as extern "C", and the distinction
         between extern "C" and extern "C++" is significant.  Put out a "K"
         (or a "Y" in the IA64 ABI) to mark the function type as a C
         function. */
      add_to_mangled_name(MANGLING_CODE_FOR_EXTERN_C, mctl);
    }  /* if */
  }  /* if */
#if IA64_ABI
  if (do_return_type) {
    /* Add the return type. */
    mangled_encoding_for_type(type->variant.routine.return_type, mctl);
  }  /* if */
#endif /* IA64_ABI */
  /* Add the parameter types. */
  mangled_encoding_for_parameter_types(type, mctl);
#if !IA64_ABI
  if (do_return_type) {
    /* Add the return type at the end, as "_" followed by the type. */
    add_to_mangled_name('_', mctl);
    mangled_encoding_for_type(type->variant.routine.return_type, mctl);
  }  /* if */
#else /* IA64_ABI */
  if (do_markers) {
    /* Add mangled encoding for a ref-qualifier, if any.  Note that there are
       no substitutions for the non-ref-qualified function type. */
    mangled_encoding_for_ref_qualifier(type, mctl);
    /* Mark the end of the function type. */
    add_to_mangled_name('E', mctl);
  }  /* if */
#endif /* IA64_ABI */
}  /* mangled_encoding_for_function_type */


static void mangled_encoding_for_function_qualifiers(
                                      a_type_ptr               type,
                                      ARG_UNUSED a_boolean     is_class_member,
                                      a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the type qualifiers (if any)
on the function or member function type "type".  is_class_member is used to
differentiate the two cases (i.e., is_class_member is TRUE for a member
function type and FALSE otherwise).
*/
{
  a_routine_type_supplement_ptr rtsp =
                              skip_typerefs(type)->variant.routine.extra_info;
  a_type_qualifier_set          qualifiers = rtsp->qualifiers;

#if ABI_COMPATIBILITY_VERSION >= 406
  if (rtsp->had_been_implicitly_const && mangle_had_been_implicitly_const) {
    /* A change in the C++ standard between C++11 and C++14 will result in
       different mangled names for constexpr non-static member functions that
       are not otherwise declared "const".  Add the "const" qualification to
       the mangled name if mangle_had_been_implicitly_const is TRUE. */
    qualifiers |= TQ_CONST;
  }  /* if */
  if (qualifiers != TQ_NONE) {
    /* In later versions of the ABI, cv-qualifiers are mangled on
       functions as well as member functions (static member functions don't
       have cv-qualifiers). */
    mangled_encoding_for_type_qualifiers(qualifiers, mctl);
  } else
#endif /* ABI_COMPATIBILITY_VERSION >= 406 */
  /* Do not insert code here. */
  {
    /* In earlier versions of the ABI, cv-qualifiers are mangled only on
       member functions. */
    if (rtsp->this_class != NULL) {
      if (qualifiers != TQ_NONE) {
        mangled_encoding_for_type_qualifiers(qualifiers, mctl);
      }  /* if */
#if !IA64_ABI
    } else if (is_class_member) {
      /* Static member function. */
      add_to_mangled_name('S', mctl);
#endif /* !IA64_ABI */
    }  /* if */
  }  /* if */
}  /* mangled_encoding_for_function_qualifiers */

#if !IA64_ABI

static void store_digits_and_underscore(unsigned long            value,
                                        a_boolean                old_form,
                                        a_mangling_control_block *mctl)
/*
Add the decimal representation of value to the mangled name.  This is
used for cases where the distinction between single-digit and multi-digit
cases needs to be indicated.  With old_form TRUE, the representation will
be simply "d" for single-digit cases, and "dd_" for multi-digit cases.
With old_form FALSE, the representation is "_dd_" regardless of the length.
*/
{
  if (old_form) {
    add_number_to_mangled_name(value, mctl);
    if (value > 9) add_to_mangled_name('_', mctl);
  } else {
    add_to_mangled_name('_', mctl);
    add_number_to_mangled_name(value, mctl);
    add_to_mangled_name('_', mctl);
  }  /* if */
}  /* store_digits_and_underscore */

#endif /* !IA64_ABI */

static void mangled_encoding_for_template_parameter(
                                       a_template_param_coordinate *coordinate,
                                       a_template_arg_ptr          args,
                                       a_mangling_control_block    *mctl)
/*
Add to the mangled name the encoding for a template parameter with the
given coordinates.  args points to the template argument list (for a
template template parameter), if any.
*/
{
  check_assertion(distinct_template_signatures &&
                  coordinate->depth !=
                                     CLASS_TEMPLATE_PLACEHOLDER_NESTING_DEPTH);
#if !IA64_ABI
  /* The encoding is "ZnZ" for a first-level parameter, and "Zn_mZ" for
     a non-first-level parameter, with "n" the parameter number, and
     "m" the depth number.  The "Z" on the end is to avoid ambiguities
     when this construct is followed by something that begins with a
     number, e.g., when a template parameter in a function parameter
     list is followed by a class name. */
  add_to_mangled_name('Z', mctl);
  /* Put out the parameter position number. */
  add_number_to_mangled_name((unsigned long)coordinate->position, mctl);
  if (coordinate->depth != 1) {
    /* Put out "_depth". */
    add_to_mangled_name('_', mctl);
    add_number_to_mangled_name((unsigned long)(unsigned)coordinate->depth,
                               mctl);
  }  /* if */
#else /* IA64_ABI */
  /* The IA-64 encoding is "Tnnn_".  The first parameter is "T_". */
  add_to_mangled_name('T', mctl);
  /* Put out the parameter position number. */
  if (coordinate->position != 1) {
    add_number_to_mangled_name((unsigned long)coordinate->position - 2, mctl);
  }  /* if */
  add_to_mangled_name('_', mctl);
#endif /* IA64_ABI */
  if (args != NULL) {
    /* Put out template arguments of a template template parameter. */
    mangled_template_arguments(args,
                               /*partial_spec=*/FALSE,
                               /*old_form=*/FALSE,
                               (a_name_reference_ptr)NULL,
                               mctl);
  }  /* if */
#if !IA64_ABI
  /* Put out the final "Z" for the Cfront-like encoding. */
  add_to_mangled_name('Z', mctl);
#endif /* !IA64_ABI */
}  /* mangled_encoding_for_template_parameter */


static void mangled_encoding_for_template_parameter_with_ctad_check(
                                                a_type                   *type,
                                                a_template_arg_ptr       args,
                                                a_mangling_control_block *mctl)
/*
Emit a mangled encoding for a template parameter, but check to see if the
type represents a class template being used for template argument deduction.
args points to the template argument list (for a template template parameter),
if any.
*/
{
  check_assertion(type->variant.template_param.kind == tptk_param);
  a_template_param_coordinate *coordinate =
                         &type->variant.template_param.extra_info->coordinates;

  if (coordinate->depth == CLASS_TEMPLATE_PLACEHOLDER_NESTING_DEPTH) {
    /* The template parameter represents a class template being used for
       class template argument deduction; mangle it as a type. */
    mangled_type_name(type, mctl);
  } else {
    /* Typical case. */
    mangled_encoding_for_template_parameter(coordinate, args, mctl);
  }  /* if */
}  /* mangled_encoding_for_template_parameter_with_ctad_check */


static void mangled_encoding_for_sizeof(
                                      a_type_ptr                     type,
                                      an_expr_node_ptr               expr,
                                      a_template_param_constant_kind kind,
                                      ARG_UNUSED an_expr_node_ptr    orig_expr,
                                      a_mangling_control_block       *mctl)
/*
Add to the mangled name the encoding of sizeof(type), __ALIGNOF__(type),
__uuidof(type), typeid(type), or noexcept(expr); kind indicates which.  If expr
is non-NULL, the original form used an expression, which expr points to.
"type" is ignored if expr != NULL.  orig_expr is the "original" expression
(where "type" and "expr" most likely originated); it is used in cases where
the original expression may have additional flags that might affect mangling
(i.e., is_cli_typeid).  orig_expr can be NULL.
*/
{
#if !IA64_ABI
  a_boolean     suppress_X = FALSE;
#endif /* IA64_ABI */

#if ABI_COMPATIBILITY_VERSION >= 402
  if ((kind == (a_template_param_constant_kind)tpck_sizeof ||
       kind == (a_template_param_constant_kind)tpck_alignof) &&
#if IA64_ABI
      (!emulate_gnu_abi_bugs || gnu_version >= 40000) &&
#endif /* IA64_ABI */
      (expr == NULL ? !is_instantiation_dependent_type(type) :
                      !expr_is_instantiation_dependent(expr))) {
    /* For a sizeof/alignof whose argument is not dependent, use a literal
       representation of the value rather than the mangled encoding for
       sizeof/alignof.  Often this substitution has already been made by
       the front end, but this can still occur in cases where the
       sizeof/alignof is a subexpression in a dependent backing expression.
       Early versions of GNU don't do this. */
    /* This case may need to be expanded for noexcept, but recent versions
       of GNU don't yet mangle noexcept, so there's nothing to compare it
       against. */
    a_constant_ptr       con = local_constant();
    a_host_large_integer value;
    if (kind == (a_template_param_constant_kind)tpck_sizeof) {
      value = (a_host_large_integer)(expr == NULL ? type->size
                                                  : expr->type->size);
    } else {
      check_assertion(kind == (a_template_param_constant_kind)tpck_alignof);
      value = expr == NULL ? type->alignment : expr->type->alignment;
    }  /* if */
    set_integer_constant(con, value, targ_size_t_int_kind);
    mangled_encoding_for_constant(con,
                                  /*old_form=*/FALSE,
                                  /*in_dependent_expr=*/FALSE,
                                  /*suppress_address_of=*/FALSE,
                                  mctl);
    release_local_constant(&con);
    goto end_of_routine;
  }  /* if */
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
#if !IA64_ABI
  /* Output has the form
       OszZ1Z0O <-- "sizeof(Z1)", Z1 indicating a template parameter.
              ^---- "O" to end the operation encoding.
             ^----- Count of operands, 0 for type and "e" cases, 1 for "X".
          ^^^------ Encoding for type or "e" or "X" (for expression cases).
        ^^--------- Operation ("sz" for sizeof, "af" for __ALIGNOF__,
                    "uu" for __uuidof, or "ty" for typeid)
       ^----------- "O" for operation.
     mangled_encoding_for_expression generates a compatible structure, so
     if you change this be sure to change that as well.
     There are three cases.  When the operation takes a type, then the type
     is encoded after the operation encoding and there are no operands.
     Prior to version 4.2, an expression was indicated with an "e" in place
     of the type (but no expression was contained in the mangled name and
     the operand count was still zero).  In version 4.2 and later, the
     mangled expression is included (operand count is 1), and the type is
     replaced with an "X" to indicate that an expression follows.
  */
  /* Put out the initial "O". */
  add_to_mangled_name('O', mctl);
#else /* IA64_ABI */
  /* The IA-64 ABI form uses "sz" for sizeof(expr) and "st" for
     sizeof(type).  The "sz" form has the normal encoding for an expression.
     The "st" form is followed by a type. */
#endif /* !IA64_ABI */
  /* Put out the operator name. */
  switch (kind) {
    case tpck_sizeof:
#if !IA64_ABI
      add_str_to_mangled_name("sz", mctl);
#else /* IA64_ABI */
      if (expr != NULL) {
        add_str_to_mangled_name("sz", mctl);
      } else {
        add_str_to_mangled_name("st", mctl);
      }  /* if */
#endif /* IA64_ABI */
      break;
    case tpck_alignof:
#if !IA64_ABI
      add_str_to_mangled_name("af", mctl);
#else /* IA64_ABI */
#if ABI_COMPATIBILITY_VERSION >= 402
      if (!(gnu_mode && gnu_abi_version < 40400)) {
        /* Use the official IA-64 ABI encoding. */
        if (expr != NULL) {
          add_str_to_mangled_name("az", mctl);
        } else {
          add_str_to_mangled_name("at", mctl);
        }  /* if */
      } else
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
      /* Do not insert code here. */
      {
        /* Use a "vendor extended operator". */
        if (expr != NULL) {
          add_str_to_mangled_name("v18alignofe", mctl);
        } else {
          add_str_to_mangled_name("v17alignof", mctl);
        }  /* if */
      }  /* if */
#endif /* IA64_ABI */
      break;
    case tpck_uuidof:
#if !IA64_ABI
      add_str_to_mangled_name("uu", mctl);
#else /* IA64_ABI */
      /* Use a "vendor extended operator". */
      if (expr != NULL) {
        add_str_to_mangled_name("v19__uuidofe", mctl);
      } else {
        add_str_to_mangled_name("v18__uuidof", mctl);
      }  /* if */
#endif /* IA64_ABI */
      break;
    case tpck_typeid:
#if !IA64_ABI
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (orig_expr != NULL && orig_expr->is_cli_typeid) {
        /* C++/CLI T::typeid form. */
        add_str_to_mangled_name("ct", mctl);
      } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      /* Do not insert code here. */
      {
        add_str_to_mangled_name("ty", mctl);
      }  /* if */
#else /* IA64_ABI */
      if (expr != NULL) {
#if ABI_COMPATIBILITY_VERSION >= 402
        add_str_to_mangled_name("te", mctl);
#else /* ABI_COMPATIBILITY_VERSION < 402 */
        add_str_to_mangled_name("v17typeide", mctl);
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
      } else {
#if ABI_COMPATIBILITY_VERSION >= 402
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (orig_expr != NULL &&
            orig_expr->is_cli_typeid) {
          /* C++/CLI T::typeid form. */
          add_str_to_mangled_name("v19clitypeid", mctl);
        } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Do not insert code here. */
        {
          add_str_to_mangled_name("ti", mctl);
        }  /* if */
#else /* ABI_COMPATIBILITY_VERSION < 402 */
        add_str_to_mangled_name("v16typeid", mctl);
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
      }  /* if */
#endif /* IA64_ABI */
      break;
    case tpck_noexcept:
      check_assertion(expr != NULL);
      add_str_to_mangled_name(MANGLING_STRING_FOR_OPERATOR_NOEXCEPT, mctl);
#if !IA64_ABI
      /* noexcept can never have a type operand, so there's no need to
         differentiate type and expression cases. */
      suppress_X = TRUE;
#endif /* !IA64_ABI */
      break;
    default:
      unexpected_condition();
  }  /* switch */
  /* The operator name is followed by the encoding for the type or the
     expression. */
  if (expr != NULL) {
#if !IA64_ABI
    /* The expression form.  Prior to 4.2, no expression was included in the
       mangling (in that case, put out "e" instead of the type).  In version
       4.2 and later, put an "X" to indicate that an expression follows
       the operand count. */
#if ABI_COMPATIBILITY_VERSION >= 402
    if (!suppress_X) add_to_mangled_name('X', mctl);
    /* One argument for the new sizeof(expr) variant. */
    store_digits_and_underscore((unsigned long)1, /*old_form=*/FALSE, mctl);
#else /* ABI_COMPATIBILITY_VERSION < 402 */
    add_to_mangled_name('e', mctl);
    /* Always zero operands for the old sizeof(expr) variant. */
    add_to_mangled_name('0', mctl);
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
#endif /* !IA64_ABI */
#if IA64_ABI || ABI_COMPATIBILITY_VERSION >= 402
#if CHECKING && IA64_ABI && ABI_COMPATIBILITY_VERSION < 402
    { a_boolean save_mangling_sizeof_expression =
                                              mctl->mangling_sizeof_expression;
      mctl->mangling_sizeof_expression = TRUE;
#endif /* CHECKING && IA64_ABI && ABI_COMPATIBILITY_VERSION < 402 */
      /* Include the expression in the mangled name.  in_dependent_expr is TRUE
         because this routine is used only for dependent sizeofs. */
      mangled_encoding_for_expression(expr, /*in_dependent_expr=*/TRUE, mctl);
#if CHECKING && IA64_ABI && ABI_COMPATIBILITY_VERSION < 402
      mctl->mangling_sizeof_expression = save_mangling_sizeof_expression;
    }
#endif /* CHECKING && IA64_ABI && ABI_COMPATIBILITY_VERSION < 402 */
#endif /* !IA64_ABI || ABI_COMPATIBILITY_VERSION >= 402 */
  } else {
    /* No expression, so put out the type. */
    mangled_encoding_for_type(type, mctl);
#if !IA64_ABI
    /* Always zero operands for the type variant. */
    add_to_mangled_name('0', mctl);
#endif /* !IA64_ABI */
  }  /* if */
#if !IA64_ABI
  /* Put out the final "O". */
  add_to_mangled_name('O', mctl);
#endif /* !IA64_ABI */
#if ABI_COMPATIBILITY_VERSION >= 402
end_of_routine:;
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
}  /* mangled_encoding_for_sizeof */


static void mangled_encoding_for_sizeof_pack(an_expr_node_ptr         expr,
                                             a_mangling_control_block *mctl)
/*
Provide mangling for a enk_sizeof_pack (sizeof...) expression.
*/
{
  a_template_param_coordinate *coordinates = NULL;
  an_expr_node_ptr            pack_expr = NULL;
  a_boolean                   suppress_address_of = FALSE;
  a_boolean                   no_mangling = FALSE;

  check_assertion(expr->kind == enk_sizeof_pack);
#if IA64_ABI
  /* GNU appears to use the encodings for sizeof rather than for sizeof...
     (at least in current versions).  We'd try to emulate that here, but we'd
     get the substitution wrong for the type.  Instead, just use the proper
     mangling for sizeof... */
  add_str_to_mangled_name("sZ", mctl);
#else /* !IA64_ABI */
  /* Mangling for sizeof...():
       OskZ1Z_0_O <-- "sizeof...(T1)", T1 indicating a template parameter.
                ^---- "O" to end the operation encoding.
             ^^^----- Count of operands (new form), 0 for type and 1 for "X".
          ^^^-------- Encoding for type or "X" (for expression cases).
        ^^----------- Operation ("sk" for sizeof...).
       ^------------- "O" for operation.
     This mangling is similar to that for sizeof (and alignof, etc.) in that
     it either encodes a type (which is always a template parameter) or an
     expression (which is always a function parameter reference).  The
     expression case is differentiated by an initial "X".
  */
  add_to_mangled_name('O', mctl);
  add_str_to_mangled_name("sk", mctl);
#endif /* IA64_ABI */
  if (expr->variant.sizeof_pack.is_template_template) {
    /* A template template parameter. */
    coordinates = &expr->variant.sizeof_pack.variant.templ->coordinates;
  } else if (expr->variant.sizeof_pack.is_type) {
    a_type_ptr type = skip_typerefs(expr->variant.sizeof_pack.variant.type);
    if (type->kind == tk_template_param &&
        type->variant.template_param.kind == tptk_param) {
      /* A type (template parameter). */
      coordinates = &type->variant.template_param.extra_info->coordinates;
    } else {
      /* Presumably a nonreal type. */
      no_mangling = TRUE;
    }  /* if */
  } else {
    /* An expression.  Non-type template parameter case is handled here,
       function parameter case is handled below. */
    pack_expr = expr->variant.sizeof_pack.variant.expr;
    pack_expr = skip_compiler_generated_expressions(pack_expr,
                                                    &suppress_address_of);
    check_assertion(pack_expr != NULL);
    if (is_constant_node(pack_expr) &&
        node_constant(pack_expr)->kind ==
                                     (a_constant_repr_kind)ck_template_param &&
        node_constant(pack_expr)->variant.template_param.kind ==
                                  (a_template_param_constant_kind)tpck_param) {
      /* Non-type template parameter. */
      coordinates = &node_constant(pack_expr)
                                  ->variant.template_param.variant.coordinates;
    }  /* if */
  }  /* if */
  /* The argument is either a template (template) parameter (as identified
     above) or a function parameter. */
  if (no_mangling) {
    /* Indicate that there is no standard mangling for this construct. */
    mangled_name_with_length("?", mctl);
  } else if (coordinates != NULL) {
    /* Note that although this template parameter is a pack, it isn't mangled
       as such (the IA-64 ABI mangling doesn't allow for that). */
    mangled_encoding_for_template_parameter(coordinates,
                                            (a_template_arg *)NULL,
                                            mctl);
#if !IA64_ABI
    store_digits_and_underscore((unsigned long)0, /*old_form=*/FALSE, mctl);
#endif /* !IA64_ABI */
  } else if (pack_expr != NULL) {
    if (pack_expr->kind == (an_expr_node_kind)enk_param_ref &&
        pack_expr->variant.param_ref.param_num != 0) {
      /* Function parameter. */
#if !IA64_ABI
      add_to_mangled_name('X', mctl);
      store_digits_and_underscore((unsigned long)1, /*old_form=*/FALSE, mctl);
#endif /* !IA64_ABI */
      mangled_encoding_for_param_reference(pack_expr, mctl);
    } else {
      /* sizeof... should only be applied to template parameter packs or
         function parameter packs, but in prototype instantiations, those may
         be represented by enk_variables; mangle as a generic expression
         (which can't be decoded, but it's only for a prototype
         instantiation). */
      check_assertion(prototype_instantiations_in_il);
      mangled_encoding_for_expression(pack_expr, /*in_dependent_expr=*/FALSE,
                                      mctl);
    }  /* if */
  } else {
    unexpected_condition();
  }  /* if */
#if !IA64_ABI
  add_to_mangled_name('O', mctl);
#endif /* !IA64_ABI */
}  /* mangled_encoding_for_sizeof_pack */


static unsigned long number_of_operands_in_list(an_expr_node_ptr expr)
/*
Return the number of expressions in the specified list.  Note that any
generated default arguments are not represented in the returned value.
*/
{
  unsigned long    num_operands;

  for (num_operands = 0;
       expr != NULL && !expr->generated_default_arg;
       num_operands++, expr = expr->next) {}
  return num_operands;
}  /* number_of_operands_in_list */


static unsigned long number_of_operands_in_operator_list(
                                             ARG_UNUSED an_opname_kind opname,
                                             an_expr_node_ptr          expr)
/*
Returns the number of arguments that should be used when determining which
mangled encoding to use for the operator associated with opname.  expr is the
argument list.  The IA-64 ABI specifies that when an <unresolved-name> refers
to an operator for which both binary and unary mangled encodings are available,
the binary encoding is chosen.  In the case where a pack expansion is used as
the only argument, this routine returns 2 (to trigger use of the binary
encoding).  Note that the return value of this routine should not be used for
looping through actual arguments (since it may return a number that is greater
than the number of actual arguments).  The Cfront ABI uses the unary encoding
in this case (since there's no specification and to avoid unnecessary ABI
changes).
*/
{
  unsigned long    num_operands;

  num_operands = number_of_operands_in_list(expr);
#if IA64_ABI && ABI_COMPATIBILITY_VERSION >= 410
  switch (opname) {
    case onk_plus:              /* "+" */
    case onk_minus:             /* "-" */
    case onk_star:              /* "*" */
    case onk_ampersand:         /* "&" */
      /* For operations that can be unary or binary, choose binary if
         appropriate. */
      if (num_operands == 1 && expr->is_pack_expansion) {
        num_operands = 2;
      }  /* if */
      break;
    default:
      break;
  }  /* switch */
#endif /* IA64_ABI && ABI_COMPATIBILITY_VERSION >= 410 */
  return num_operands;
}  /* number_of_operands_in_operator_list */


static unsigned long number_of_parameters(a_routine_ptr routine)
/*
Return the number of parameters for the specified routine.
*/
{
  unsigned long    num_params;
  a_param_type_ptr ptp;
  a_routine_type_supplement_ptr
                   rtsp;

  /* Count the routine's parameters. */
  num_params = 0;
  rtsp = skip_typerefs(routine->type)->variant.routine.extra_info;
  for (ptp = rtsp->param_type_list;
       ptp != NULL;
       ptp = ptp->next) {
    ++num_params;
  }  /* for */
  if (rtsp->this_class != NULL) {
    /* If this is a nonstatic member function, the object pointed to by "this"
       is an implicit operand. */
    ++num_params;
  }  /* if */
  return num_params;
}  /* number_of_parameters */


static void mangled_encoding_for_builtin_operation(
                                                an_expr_node_ptr         expr,
                                                a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding of the indicated expression,
which is an enk_builtin_operation (used for some vendor-specific
extensions, e.g., Microsoft __is_base_of).
*/
{
  a_builtin_operation_kind kind;
  unsigned long            num_operands;
  an_expr_node_ptr         operand;

  check_assertion(expr->kind == (an_expr_node_kind)enk_builtin_operation);
  kind = expr->variant.builtin_operation.kind;
  /* Get the operand count. */
  num_operands = number_of_operands_in_list(
                                     expr->variant.builtin_operation.operands);
  check_assertion(num_operands <= 9);
#if !IA64_ABI
  /* Output has the form
       Obi_0_1T1AO <-- "builtin-operation-0(A)", where A is a type
                 ^---- "O" to end the operation encoding.
              ^------- Type operand A
             ^-------- Count of operands.
          ^^^--------- Builtin operation kind number.
        ^^------------ "bi" for builtin operation.
       ^-------------- "O" for operation.
     mangled_encoding_for_expression generates a compatible structure, so
     if you change this be sure to change that as well.
  */
  /* Put out the initial "O". */
  add_to_mangled_name('O', mctl);
  add_str_to_mangled_name("bi", mctl);
  store_digits_and_underscore((unsigned long)kind, /*old_form=*/FALSE, mctl);
  /* Put out the count of operands. */
  add_number_to_mangled_name(num_operands, mctl);
#else /* IA64_ABI */
  /* The IA-64 ABI form uses a "vendor extended operator" of builtinXXX,
     where XXX is the builtin operation kind number. */
  add_to_mangled_name('v', mctl);
  add_number_to_mangled_name(num_operands, mctl);
  if ((int)kind <= 99) {
    /* For backward compatibility. */
    add_str_to_mangled_name("9builtin", mctl);
  } else {
    add_str_to_mangled_name("10builtin", mctl);
    add_number_to_mangled_name(((unsigned)kind) / 100u, mctl);
    kind = (a_builtin_operation_kind)((unsigned)kind % 100u);
  }  /* if */
  add_number_to_mangled_name(((unsigned)kind) / 10u, mctl);
  add_number_to_mangled_name(((unsigned)kind) % 10u, mctl);
#endif /* IA64_ABI */
  for (operand = expr->variant.builtin_operation.operands;
       operand != NULL;
       operand = operand->next) {
    /* Put out the operands. */
    if (operand->kind == (an_expr_node_kind)enk_type_operand) {
#if !IA64_ABI
      add_to_mangled_name('T', mctl);
#else /* IA64_ABI */
      add_str_to_mangled_name("TO", mctl);
#endif /* IA64_ABI */
      mangled_encoding_for_type(operand->variant.type_operand.type, mctl);
    } else {
      mangled_encoding_for_expression(operand, /*in_dependent_expr=*/TRUE,
                                      mctl);
    }  /* if */
  }  /* for */
#if !IA64_ABI
  /* Put out the final "O". */
  add_to_mangled_name('O', mctl);
#endif /* !IA64_ABI */
}  /* mangled_encoding_for_builtin_operation */


static void add_float_value_to_mangled_name(
                                           a_float_kind             float_kind,
                                           an_internal_float_value  *value,
                                           ARG_UNUSED a_boolean     old_form,
                                           a_mangling_control_block *mctl)
/*
Add a string to the mangled name representing the value of the floating
point number as given by *value.  The kind of the floating point value
(e.g., float, double, etc.) is given by float_kind.  If old_form is TRUE,
the length of the value (only emitted in the Cfront ABI) uses the old form
for specifying the length (which can be ambiguous in some cases).
*/
{
#if !IA64_ABI
BEGIN_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED
  /* The Cfront-like ABI encoding for a floating point value is:
       4n1p5 <-- encoding for "-1.5"
         ^^^---- Literal value ("p" for decimal point).
        ^------- "n" indicates negative.
       ^-------- Length of the float.
     cfront 3.0.1 does not implement this, so we made it up. */
  a_number_buffer str = fp_to_string(float_kind, value, (a_boolean *)NULL,
                                     (a_boolean *)NULL, (a_boolean *)NULL);
  size_t          chars_removed = 0;
  /* This is TRUE between the '.' and any 'E' or 'e'. */
  a_boolean       drop_insignificant_zeros = FALSE;

  for (size_t i = 0; i < str.length(); ++i) {
    char curr_char = str[i];
    switch (curr_char) {
      case '.':
        /* Use "d" to represent a decimal point. */
        str[i - chars_removed] = 'd';
        drop_insignificant_zeros = TRUE;
#if !USE_HOST_FP_CONVERSION_ROUTINES || \
    (USE_FLOAT128_FOR_HOST_FP_VALUE && !USE_QUADMATH_LIBRARY)
        /* Check for ".0E" and if found, drop the ".0". */
        if (i + 2 < str.length()) {
          if (str[i + 1] == '0' &&
              str[i + 2] == 'E') {
            /* Do not reprocess 0. */
            ++i;
            /* Drop two characters ('.' and '0') from the output. */
            chars_removed += 2;
          }  /* if */
        }  /* if */
#endif /* !USE_HOST_FP_CONVERSION_ROUTINES || (USE_FLOAT128...) */
        break;
      case '0':
        { /* Check for and drop unnecessary 0s, e.g. change "1.50000e+10" to
             "1.5e+10". */
          size_t k = 0;

          for (; i + k < str.length(); ++k) {
            char lookahead_char = str[i + k];

            if (lookahead_char != '0') {
              break;
            }  /* if */
            str[i + k - chars_removed] = '0';
          }  /* for */
          /* If the character that stopped lookahead is not a digit and we're
             dropping trailing zeros, these zeros are insignificant, drop
             them. */
          if (drop_insignificant_zeros && !isdigit(str[i + k])) {
            /* If the character preceding the zeros is a '.' (at this point
               represented as 'd') allow one '0' to remain. */
            if (str[i - chars_removed - 1] == 'd') {
              chars_removed += k - 1;
            } else {
              chars_removed += k;
            }  /* if */
          }  /* if */
          /* Subtract 1 as there's always going to be at least one '0' (the
             initial '0' that entered this case label). */
          i += k - 1;
        }
        break;
      case 'e':
      case 'E':
        drop_insignificant_zeros = FALSE;
        str[i - chars_removed] = 'e';
#if !USE_HOST_FP_CONVERSION_ROUTINES || \
    (USE_FLOAT128_FOR_HOST_FP_VALUE && !USE_QUADMATH_LIBRARY)
        /* Check for and replace "E2" with "ep2". */
        if (i + 1 < str.length() && isdigit(str[i + 1])) {
          /* Insert a p. */
          if (chars_removed == 0) {
            a_string_view char_view("p", 1);

            /* There are no characters removed, create a new character and
               advance the iterator so the next character seen is still the
               digit. */
            ++i;
            str.insert(i - chars_removed, char_view);
          } else {
            /* There is at least one character that's been removed.  As an
               optimization, use the extra space that exists from the removed
               character for the new 'p' character. */
            --chars_removed;
            str[i - chars_removed] = 'p';
          }  /* if */
        }  /* if */
#endif /* !USE_HOST_FP_CONVERSION_ROUTINES || (USE_FLOAT128...) */
        break;
      case '-':
        /* Use "n" to represent a minus sign. */
        str[i - chars_removed] = 'n';
        break;
      case '+':
        /* Use "p" to represent a plus sign. */
        str[i - chars_removed] = 'p';
        break;
      default:
        str[i - chars_removed] = str[i];
        break;
    }  /* switch */
  }  /* for */
  str.truncate_to(str.length() - chars_removed);
  /* Put out the length of the string (taking into account characters that
     have been either removed or added). */
  store_digits_and_underscore((unsigned long)str.length(), old_form, mctl);
  add_str_to_mangled_name(str.as_temp_characters(), mctl);
#else /* IA64_ABI */
  /* The IA-64 ABI specifies that a floating point value be encoded as a
     hexadecimal string for the constant value, high-order bytes first,
     using lower-case hexadecimal letters. */
  a_number_buffer hex_str = fp_to_hex_string(float_kind, value);
  add_str_to_mangled_name(hex_str.as_temp_characters(), mctl);
#endif /* !IA64_ABI */
}  /* add_float_value_to_mangled_name */


static void mangled_encoding_for_float_constant(
                                             a_constant_ptr           con,
                                             a_boolean                old_form,
                                             a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the ck_float constant con.
This is used to encode floating-point constants as part of the
mangled names of template classes.  If old_form is TRUE, use the old form
of length specification in the mangling for lengths of literals.
old_form is significant only in the Cfront ABI.
*/
{
#if !IA64_ABI
  /* Float: the Cfront-like ABI encoding is like
       L4n1p5 <-- encoding for "-1.5"
          ^^^---- Literal value ("p" for decimal point).
         ^------- "n" indicates negative.
        ^-------- Length of the literal.
       ^--------- "L" indicates a number.
     cfront 3.0.1 does not implement this, so we made it up. */
  add_to_mangled_name('L', mctl);
  add_float_value_to_mangled_name(skip_typerefs(con->type)->variant.float_kind,
                                  &con->variant.float_value, old_form, mctl);
#else /* IA64_ABI */
  /* For IA-64, the encoding is
       L <type> <float> E
     The <float> is a hexadecimal string for the constant value,
     high-order bytes first, using lower-case hexadecimal letters.
  */
  add_to_mangled_name('L', mctl);
  /* Add the encoding for the type. */
  mangled_encoding_for_type(con->type, mctl);
  /* Add the hex digits. */
  add_float_value_to_mangled_name(skip_typerefs(con->type)->variant.float_kind,
                                  &con->variant.float_value, old_form, mctl);
  /* Add the end-of-literal marker. */
  add_to_mangled_name('E', mctl);
#endif /* !IA64_ABI */
}  /* mangled_encoding_for_float_constant */

#if C99_IL_EXTENSIONS_SUPPORTED

static void repr_for_complex_constant(a_constant_ptr           con,
                                      an_internal_float_value *real,
                                      an_internal_float_value *imag)
/*
This routine returns the real and imaginary values of a complex constant
(which can be a ck_complex or ck_aggregate).
*/
{
  check_assertion(con->kind == (a_constant_repr_kind)ck_complex ||
                  con->kind == (a_constant_repr_kind)ck_aggregate);

  if (con->kind == (a_constant_repr_kind)ck_complex) {
    /* Values are part of the complex constant. */
    *real = con->variant.complex_value->real;
    *imag = con->variant.complex_value->imag;
  } else if (con->kind == (a_constant_repr_kind)ck_aggregate) {
    /* The values are in the aggregate. */
    check_assertion(con->variant.aggregate.first_constant->kind ==
                                              (a_constant_repr_kind)ck_float &&
                    con->variant.aggregate.last_constant->kind ==
                                               (a_constant_repr_kind)ck_float);
    *real = con->variant.aggregate.first_constant->variant.float_value;
    *imag = con->variant.aggregate.last_constant->variant.float_value;
  }  /* if */
}  /* repr_for_complex_constant */


static void mangled_encoding_for_complex_constant(
                                             a_constant_ptr           con,
                                             a_boolean                old_form,
                                             a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the ck_complex (or ck_aggregate
that represents complex) constant con.  This is used to encode complex
floating-point constants as part of the mangled names of expressions.  If
old_form is TRUE, use the old form of length specification in the mangling for
lengths of literals.  old_form is significant only in the Cfront ABI.
*/
{
  an_internal_float_value  real, imag;
  a_type_ptr               con_type = con->type;

  check_assertion(con->kind == (a_constant_repr_kind)ck_complex ||
                  con->kind == (a_constant_repr_kind)ck_aggregate);

  /* Extract the values to be encoded from the constant. */
  repr_for_complex_constant(con, &real, &imag);
#if !IA64_ABI
  /* Complex float: the Cfront-like ABI encoding mangles both real and
     imaginary portions of the value as floating point numbers:
       L_3_0d0_3_1d0 <-- encoding for "0.0+1.0i"
                 ^^^---- Imaginary portion of complex number.
              ^^^------- Length of the imaginary portion of the number.
           ^^^---------- Real portion of complex number.
        ^^^------------- Length of the real portion of the complex number.
       ^---------------- "L" indicates a number.
     cfront 3.0.1 does not implement this, so we made it up. */
  add_to_mangled_name('L', mctl);
  add_float_value_to_mangled_name(skip_typerefs(con_type)->variant.float_kind,
                                  &real, old_form, mctl);
  add_float_value_to_mangled_name(skip_typerefs(con_type)->variant.float_kind,
                                  &imag, old_form, mctl);
#else /* IA64_ABI */
  /* For IA-64, the encoding is
       L <type> <real-part float> _ <imag-part float> E
     The <float> is a hexadecimal string for the constant value,
     high-order bytes first, using lower-case hexadecimal letters.
  */
  add_to_mangled_name('L', mctl);
  /* Add the encoding for the type. */
  mangled_encoding_for_type(con_type, mctl);
  /* Add the hex digits for the real portion of the complex number. */
  add_float_value_to_mangled_name(skip_typerefs(con_type)->variant.float_kind,
                                  &real, old_form, mctl);
  add_to_mangled_name('_', mctl);
  /* Add the hex digits for the imaginary portion of the complex number. */
  add_float_value_to_mangled_name(skip_typerefs(con_type)->variant.float_kind,
                                  &imag, old_form, mctl);
  /* Add the end-of-literal marker. */
  add_to_mangled_name('E', mctl);
#endif /* !IA64_ABI */
}  /* mangled_encoding_for_complex_constant */

#endif /* C99_IL_EXTENSIONS_SUPPORTED */

static void mangled_name_with_length(a_const_char             *name,
                                     a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for a name, with a prefix that
indicates the length, e.g., "3abc" for the name "abc".  name is
null-terminated or it is a null pointer (which is encoded as an empty
string, i.e., "0").
*/
{
  if (name != NULL) {
    add_number_to_mangled_name((unsigned long)strlen(name), mctl);
    add_str_to_mangled_name(name, mctl);
  } else {
    add_number_to_mangled_name(0UL, mctl);
  }  /* if */
}  /* mangled_name_with_length */


static void mangled_encoding_for_integer(a_number_buffer          &buffer,
                                         ARG_UNUSED a_type_ptr    type,
                                         ARG_UNUSED a_boolean     old_form,
                                         a_mangling_control_block *mctl)
/*
Emit a mangled encoding for the integer whose value is represented by the
number buffer and whose type is specified by "type".  If old_form is TRUE use
the old form for encoding literals.  The type is unused in the Cfront ABI and
old_form is unused in the IA-64 ABI.
*/
{
#if !IA64_ABI
  /* Integer: the encoding is like
       L3n12  <-- encoding for "-12"
          ^^----- Literal value.
         ^------- "n" indicates negative.
        ^-------- Length of the literal.
       ^--------- "L" indicates a number.
     This is compatible with cfront 3.0.1. */
  /* Use "n" to represent a minus sign. */
  if (buffer[0] == '-') buffer[0] = 'n';
  add_to_mangled_name('L', mctl);
  store_digits_and_underscore((unsigned long)buffer.length(), old_form, mctl);
  add_str_to_mangled_name(buffer.as_temp_characters(), mctl);
#else /* IA64_ABI */
  /* Integer: the encoding is
       L <type> <value number> E
     The <number> is like the above, with "n" indicating negative.
  */
  add_to_mangled_name('L', mctl);
  mangled_encoding_for_type(type, mctl);
  /* Use "n" to represent a minus sign. */
  if (buffer[0] == '-') buffer[0] = 'n';
  if (!is_or_was_nullptr_type(type)) {
    /* As a special case, nullptr is mangled without the value
       (i.e., "L Dn E"). */
    add_str_to_mangled_name(buffer.as_temp_characters(), mctl);
  }  /* if */
  add_to_mangled_name('E', mctl);
#endif /* IA64_ABI */
}  /* mangled_encoding_for_integer */


static void mangled_encoding_for_address_constant(
                                  a_constant_ptr           con,
                                  ARG_UNUSED a_boolean     suppress_address_of,
                                  a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the ck_address constant con.
This is used to encode address constants as part of the mangled names of
template classes.  When suppress_address_of is TRUE, suppress any "address of"
mangling that might otherwise be added.
*/
{
  an_address_base_kind abkind;
#if !IA64_ABI
  a_length_reservation length_reservation;
#endif /* !IA64_ABI */

  /* The offset can be non-zero in cases where a pointer to class was
     cast to a related class.  That's ignored in the output. */
  abkind = con->variant.address.kind;
  check_assertion_str(abkind != (an_address_base_kind)abk_constant,
                      "mangled_encoding_for_address_constant: abk_constant");
#if !IA64_ABI
  /* Address of something other than a constant, i.e., a variable or
     routine.  The encoding is like
       4abcd <-- encoding for address of "abcd"
        ^^^^---- Name of entity.
       ^-------- Length of the name.
     This is compatible with cfront 3.0.1. */
  reserve_space_for_length(&length_reservation, mctl);
#else /* IA64_ABI */
  /* IA-64 encoding.  Indicate that we're taking the address of the literal
     by adding the encoding for unary "&" (unless it's a reference type or
     explicitly disabled by the caller). */
  if (!suppress_address_of && !is_any_reference_type(con->type)) {
    add_str_to_mangled_name("ad", mctl);
  }  /* if */
  add_to_mangled_name('L', mctl);
#endif /* IA64_ABI */
  if (abkind == (an_address_base_kind)abk_variable) {
    a_variable_ptr variable = con->variant.address.variant.variable;
#if IA64_ABI
    add_template_argument_mangled_name_prefix(con, mctl);
#endif /* IA64_ABI */
#if ABI_COMPATIBILITY_VERSION >= 520
    /* Give a variable proper mangling (earlier versions mangled just the
       name, which is incomplete).  The earlier code is maintained for
       backward compatibility. */
    mangled_variable_name_with_possible_qualification(variable, mctl);
#else /* ABI_COMPATIBILITY_VERSION < 520 */
    if (is_class_or_namespace_member(variable)) {
      /* Static data member or namespace member variable. */
      mangled_variable_name_with_possible_qualification(variable, mctl);
    } else {
      /* Normal variable. */
#if !IA64_ABI
      a_const_char *str = unmangled_or_fabricated_name_of_variable(variable);
      check_assertion_str(str != NULL,
                     "mangled_encoding_for_address_constant: addr of unnamed");
      add_str_to_mangled_name(str, mctl);
#else /* IA64_ABI */
#if ABI_COMPATIBILITY_VERSION < 415
      /* This is wrong (the variable name may have been previously mangled,
         thus resulting in names that cannot be demangled), but is left here
         for backward ABI compatibility. */
      mangled_name_with_length(variable->source_corresp.name, mctl);
#else /* ABI_COMPATIBILITY_VERSION >= 415 */
      mangled_variable_name_with_possible_qualification(variable, mctl);
#endif /* ABI_COMPATIBILITY_VERSION < 415 */
#endif /* IA64_ABI */
    }  /* if */
#endif /* ABI_COMPATIBILITY_VERSION >= 520 */
  } else if (abkind == (an_address_base_kind)abk_routine) {
    a_boolean     suppress_param_encoding = FALSE;
    a_boolean     suppress_parent_encoding = FALSE;
    a_routine_ptr routine = con->variant.address.variant.routine;
#if IA64_ABI || ABI_COMPATIBILITY_VERSION >= 604
    if (!function_name_mangling_needed(routine, &suppress_param_encoding)) {
      suppress_param_encoding = TRUE;
      suppress_parent_encoding = TRUE;
    }  /* if */
#else /* !(IA64_ABI || ABI_COMPATIBILITY_VERSION >= 604) */
    /* Parameters were not encoded in earlier versions of the ABI (though this
       could lead to mangled names that are not unique). */
    suppress_param_encoding = TRUE;
#endif /* IA64_ABI || ABI_COMPATIBILITY_VERSION >= 604 */
#if IA64_ABI
    if (emulate_gnu_abi_bugs && suppress_param_encoding) {
      /* g++ 3.2 does not include the "_Z" for extern "C" functions. */
    } else {
      add_template_argument_mangled_name_prefix(con, mctl);
    }  /* if */
#endif /* IA64_ABI */
    mangled_function_name(routine, suppress_param_encoding,
                          suppress_parent_encoding,
                          /*force_primary_name=*/TRUE,
                          /*force_individuation=*/FALSE,
                          /*base_name_offset=*/(sizeof_t *)NULL,
                          mctl);
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (abkind == (an_address_base_kind)abk_uuidof) {
    a_type_ptr   uuid_type;
    a_const_char *uuid_str;

    /* Microsoft __uuidof. */
    /* The uuid string attached to the associated type has the format
         hhhhhhhh-hhhh-hhhh-hhhh-hhhhhhhhhhhh
       (where "h" is a hexadecimal digit).  The mangled form is
       a length followed by "__UUID" followed by the string, with hyphens
       removed.  This is just made up; the Microsoft compiler uses a
       completely different mangling scheme, so compatibility is a moot
       point here. */
#if IA64_ABI
    /* Add encoding to make this into an identifier. */
    add_str_to_mangled_name("_Z38", mctl);
#endif /* IA64_ABI */
    add_str_to_mangled_name("__UUID", mctl);
    uuid_type = con->variant.address.variant.type;
    uuid_str = NULL;
    if (uuid_type == NULL) {
      /* Null GUID case. */
      uuid_str = "00000000-0000-0000-0000-000000000000";
    } else if (is_immediate_class_type(uuid_type)) {
      uuid_str = class_type_supp(uuid_type)->uuid_string;
    } else if (uuid_type->kind == (a_type_kind)tk_enum) {
      uuid_str = integer_type_supp(uuid_type)->uuid_string;
    }  /* if */
    if (uuid_str == NULL) {
      /* This can happen in error cases. */
      uuid_str = "00000000-0000-0000-0000-000000000000";
    }  /* if */
    for (; *uuid_str != '\0'; uuid_str++) {
      if (*uuid_str != '-') add_to_mangled_name(*uuid_str, mctl);
    }  /* for */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else if (abkind == (an_address_base_kind)abk_typeid) {
    /* A typeid(...) expression used as a template argument. */
#if IA64_ABI
    a_const_char *ti_prefix = "TI";
#else /* !IA64_ABI */
    a_const_char *ti_prefix = "__T_";
#endif /* IA64_ABI */
    add_mangled_name_prefix(mctl);
    add_str_to_mangled_name(ti_prefix, mctl);
    mangled_encoding_for_type(con->variant.address.variant.type, mctl);
  } else {
    unexpected_condition_str(
                          "mangled_encoding_for_address_constant: bad abkind");
  }  /* if */
#if !IA64_ABI
  fill_in_length(&length_reservation, mctl);
#else /* IA64_ABI */
  /* Mark end of literal. */
  add_to_mangled_name('E', mctl);
#endif /* IA64_ABI */
}  /* mangled_encoding_for_address_constant */


/*
Introduce two helper macros to help build mangled names for expressions in
a somewhat ABI-agnostic fashion.

add_operation_prefix_to_mangled_name adds the ABI-specific prefix for the
the operation specified by op_string, with the specified number of operands.

add_operation_suffix_to_mangled_name adds the ABI-specific suffix (if needed).
*/
#if IA64_ABI
/* No specific prefix is needed (just add the op_string). */
#define add_operation_prefix_to_mangled_name(op_string, operands, mctl) \
  add_str_to_mangled_name((op_string), (mctl));
/* No suffix is needed. */
#define add_operation_suffix_to_mangled_name(mctl) /* Nothing */
#else /* !IA64_ABI */
/* Add 'O' as a prefix, then the string and the number of operands. */
#define add_operation_prefix_to_mangled_name(op_string, operands, mctl) \
  add_to_mangled_name('O', (mctl));                                     \
  add_str_to_mangled_name((op_string), (mctl));                         \
  store_digits_and_underscore((operands), /*old_form=*/FALSE, mctl);
/* Add 'O' as a closing suffix. */
#define add_operation_suffix_to_mangled_name(mctl)                      \
  add_to_mangled_name('O', (mctl));
#endif /* IA64_ABI */

static void mangled_subobject_path(a_constant_ptr           con,
                                   a_subobject_path         *path,
                                   a_mangling_control_block *mctl)
/*
Provide a mangling for a portion of a subobject path (as specified in path)
for the given constant.  If path is NULL, provide a mangling for the address
constant itself.  This routine is called recursively, in reverse order, for
every element of the subobject path.

For example, for this code:

  void g(A<&b.j[0]+1>) {}

The generated (IA-64 ABI) mangled name is: _Z1g1AIXadixdtL_Z1bE1jLl1EEE.  The
corresponding subobject_path has two components (db_subobject_path output):
".B::j->[1]" and when traversed in reverse order, each iteration contributes
to the mangled name as such:

  _Z1g1AIXadixdtL_Z1bE1jLl1EEE
            ix          Ll1E // component 2 (!is_offset && !is_base_class)
              dt      1j     // component 1 (is_offset)
                L_Z1bE       // address constant itself (path == NULL)
*/
{
  if (path == NULL) {
    mangled_encoding_for_address_constant(con, /*suppress_address_of=*/TRUE,
                                          mctl);
  } else {
    if (path->is_offset) {
      add_operation_prefix_to_mangled_name(
                                        MANGLING_STRING_FOR_OPERATOR_SUBSCRIPT,
                                        2, mctl);
      mangled_subobject_path(con, path->next, mctl);
#if !IA64_ABI
      add_to_mangled_name('C', mctl);
      mangled_encoding_for_type(integer_type(targ_ptrdiff_t_int_kind), mctl);
#endif /* !IA64_ABI */
      a_number_buffer buffer(path->variant.ptr_offset);
      mangled_encoding_for_integer(buffer,
                                   integer_type(targ_ptrdiff_t_int_kind),
                                   /*old_form=*/FALSE,
                                   mctl);
      add_operation_suffix_to_mangled_name(mctl);
    } else if (path->is_base_class) {
      /* This does not contribute to the mangled name. */
      mangled_subobject_path(con, path->next, mctl);
    } else {
      add_operation_prefix_to_mangled_name(MANGLING_STRING_FOR_OPERATOR_DOT,
                                           2, mctl);
      mangled_subobject_path(con, path->next, mctl);
      a_const_char *field_name =
         unmangled_or_fabricated_name_of(&path->variant.field->source_corresp);
      /* It appears that an un-qualified field name is used here. */
      mangled_name_with_length(field_name, mctl);
      add_operation_suffix_to_mangled_name(mctl);
    }  /* if */
  }  /* if */
}  /* mangled_subobject_path */


static void mangled_encoding_for_address_constant_and_possible_subobject_path(
                                                a_constant_ptr           con,
                                                a_mangling_control_block *mctl)
/*
Provide a mangled encoding for an address constant that might possibly
contain a subobject path.  The latter case occurs only in references to
subobjects as arguments to nontype template parameters.
*/
{
  a_subobject_path *soj_path = con->variant.address.subobject_path;

  if (soj_path == NULL) {
    mangled_encoding_for_address_constant(con, /*suppress_address_of=*/FALSE,
                                          mctl);
  } else {
    /* To generate the mangled encoding for a subobject path, reverse it and
       perform a recursive traversal (then reverse it again to restore it to
       the original state. */
#if IA64_ABI
    if (is_pointer_type(con->type)) {
      /* Add an initial "address of" mangling. */
      add_str_to_mangled_name("ad", mctl);
    }  /* if */
#endif /* IA64_ABI */
    soj_path = reverse_simple_list(soj_path);
    mangled_subobject_path(con, soj_path, mctl);
    (void)reverse_simple_list(soj_path);
  }  /* if */
}  /* mangled_encoding_for_address_constant_and_possible_subobject_path */

#if DO_IL_LOWERING || (!IA64_ABI && ABI_COMPATIBILITY_VERSION < 520)

static a_const_char *first_field_name(a_type_ptr              class_type,
                                      a_source_correspondence **field_scp)
/*
Return the name of the first named field of the indicated class, struct,
or union.  If the first member of the class is unnamed, recursively look
at its first member, etc.  If there is no named member, return NULL.
Set *field_scp to point to the source correspondence of the
first named field; leave it unchanged if there is no named field.
*/
{
  a_const_char  *name = NULL;
  a_field_ptr   field;

  /* GNU and Microsoft compilers allow cv-qualifiers on anonymous unions. */
  class_type = skip_typerefs(class_type);
  check_assertion(is_immediate_class_type(class_type));
  /* Look at fields, find the first named one. */
  for (field = class_type->variant.class_struct_union.field_list;
       field != NULL;
       field = field->next) {
    name = field->source_corresp.name;
    if (name != NULL) {
      *field_scp = &field->source_corresp;
      break;
    }  /* if */
    /* Unnamed field. */
    if (field->is_anonymous_parent_object) {
      /* Do a recursive call to process a nested anonymous union. */
      name = first_field_name(field->type, field_scp);
      if (name != NULL) break;
    }  /* if */
    /* Keep looping for other unnamed fields (e.g., unnamed bit fields). */
  }  /* for */
  return name;
}  /* first_field_name */


static a_const_char *unmangled_or_fabricated_name_of_variable(
                                                            a_variable_ptr var)
/*
Return the unmangled or fabricated name of the specified variable.  If the
variable is an anonymous union variable, give it the name of its first member.
Can return NULL in some cases (e.g., temporary variables, but they shouldn't
appear in mangled names).
*/
{
  a_const_char            *name;
  a_source_correspondence *field_scp;

  name = unmangled_or_fabricated_name_of(&var->source_corresp);
  if (name == NULL && var->is_anonymous_parent_object) {
    /* Give a name to an anonymous union variable based on its first
       member's name. */
    name = first_field_name(var->type, &field_scp);
  }  /* if */
  return name;
}  /* unmangled_or_fabricated_name_of_variable */

#endif /* DO_IL_LOWERING || (!IA64_ABI && ABI_COMPATIBILITY_VERSION < 520) */
#if ABI_COMPATIBILITY_VERSION >= 402

static a_boolean is_unresolved_type(a_type_ptr type)
/*
Returns TRUE if type is an <unresolved-type>, i.e., a dependent
decltype-like operator typeref (decltype/splice/pack-index) or a
<template-param>.  This is called on the top-level qualifier during mangling
of an <unresolved-name> to determine which of the three scope resolution cases
is appropriate.  The type must not have had its typerefs skipped by the
caller.
*/
{
  a_boolean       result = FALSE;

  type = skip_typerefs_not_dependent_decltypes(type);
  if (type->kind == (a_type_kind)tk_typeref &&
      (is_typeref_kind(type, trk_is_decltype) ||
       is_typeref_kind(type, trk_is_splice) ||
       is_typeref_kind(type, trk_pack_index))) {
    result = TRUE;
  } else {
    if (is_proxy_class(type)) {
      result = TRUE;
    } else if (is_template_param_type(type)) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* is_unresolved_type */


/*
A local structure to hide the differences between using two different
sets of data structures when recursing through a series of name qualifiers 
to emit a scope resolution mangling.  In one case (uses_qualifiers == TRUE),
a_name_reference and a_name_qualifier are used to traverse the list of
qualifiers, in the second case (uses_qualifiers == FALSE), parent information
is used to traverse a qualified list.  In the latter case, scp and kind
are used to indicate where we are in the list.  is_global_qualified_name
is set to TRUE to indicate that the name has global qualification (e.g., ::X).
*/
typedef struct a_scope_resolution_step {
  a_boolean     uses_qualifiers;
  a_boolean     is_global_qualified_name;
  union {
    /* When uses_qualifiers == TRUE: */
    a_name_qualifier_ptr
                qualifier;
    /* When uses_qualifiers == FALSE: */
    struct {
      a_source_correspondence_ptr
                scp;
      an_il_entry_kind
                kind;
    } scp_kind; 
  } variant;
} a_scope_resolution_step;

/*
Macros to retrieve the source correspondence and kind from a particular
a_scope_resolution_step pointer (regardless of which data structure
representation is being used).
*/
#define scp_of_step(step)                                                    \
  ((step)->uses_qualifiers ?                                                 \
    ((step)->variant.qualifier == NULL ?                                     \
     (a_source_correspondence_ptr)NULL :                                     \
     ((step)->variant.qualifier->is_class ?                                  \
      &(step)->variant.qualifier->qualifier.class_type->source_corresp :     \
      &(step)->variant.qualifier->qualifier.namespace_ptr->source_corresp)) :\
    (step)->variant.scp_kind.scp)

/*lint -emacro(641,kind_of_step)*/
#define kind_of_step(step)                                                   \
  ((step)->uses_qualifiers ?                                                 \
    ((step)->variant.qualifier == NULL ?                                     \
     iek_none :                                                              \
     ((step)->variant.qualifier->is_class ? iek_type : iek_namespace)) :     \
    (step)->variant.scp_kind.kind)

    
static void next_scope_resolution_step(a_scope_resolution_step *current,
                                       a_scope_resolution_step *next,
                                       a_boolean               *top_level)
/*
Returns, in *next, the "next" step upward in the name qualification after the
current level.  Also sets *top_level to indicate when there are no
further qualifications (in which case *next is mostly useless to the
caller -- except for the is_global_qualified_name setting).
*/
{
  *top_level = FALSE;
  check_assertion(current != NULL && next != NULL);
  next->uses_qualifiers = current->uses_qualifiers;
  next->is_global_qualified_name = current->is_global_qualified_name;
  if (current->uses_qualifiers) {
    if (current->variant.qualifier == NULL) {
      check_assertion(current->is_global_qualified_name);
      next->variant.qualifier = NULL;
    } else {
      next->variant.qualifier = current->variant.qualifier->previous_qualifier;
    }  /* if */
    if (next->variant.qualifier == NULL) *top_level = TRUE;
  } else {
    a_source_correspondence_ptr scp = current->variant.scp_kind.scp;
    if (scp->is_class_member) {
      /* Parent is a class. */
      next->variant.scp_kind.scp = &(scp_parent_class(scp))->source_corresp;
      next->variant.scp_kind.kind = iek_type;
    } else if (scp_is_namespace_member(scp)) {
      /* Parent is a namespace. */
      next->variant.scp_kind.scp = &scp_parent_namespace(scp)->source_corresp;
      next->variant.scp_kind.kind = iek_namespace;
    } else {
      /* Must be at the top level. */
      next->variant.scp_kind.scp = NULL;
      next->variant.scp_kind.kind = iek_none;
      *top_level = TRUE;
    }  /* if */
  }  /* if */
}  /* next_scope_resolution_step */


static void mangled_scope_resolution(a_scope_resolution_step  *current,
                                     a_boolean                *need_close,
                                     unsigned long            nesting_level,
                                     a_mangling_control_block *mctl)
/*
Adds scope resolution mangling for the entity represented by "current"
(and recursively all of its parents) to the mangled name.  Used as a 
portion of the mangling for <unresolved-name>.  In the IA-64 ABI,
*need_close is set to TRUE in cases where a terminating "E" needs to be added
by the topmost caller.  nesting_level is a count of the number of levels of
qualifiers seen so far (and is typically set to one by the initial caller).
*/
{
  a_template_arg_ptr          template_arg_list = NULL;
  a_class_type_supplement_ptr ctsp;
  a_source_correspondence_ptr scp;
  a_boolean                   is_top_level_unresolved_type = FALSE;
  a_boolean                   is_top_level;
  a_scope_resolution_step     parent;
  an_il_entry_kind            kind;

  /* Get information about the next step in the qualification. */
  next_scope_resolution_step(current, &parent, &is_top_level);
  /* Note that in the case where only a global qualifier is specified
     (e.g., ::X), scp can be NULL (and kind will be iek_none), but we still
     need to represent the global qualification. */
  scp = scp_of_step(current);
  kind = kind_of_step(current);
  if (!is_top_level) {
    /* Recurse to process any parents first. */
    nesting_level++;
    mangled_scope_resolution(&parent, need_close, nesting_level, mctl);
  } else {
    /* This is the top-most qualifier (or there were no qualifiers). */
    if (kind == iek_type) {
      is_top_level_unresolved_type = is_unresolved_type((a_type_ptr)scp);
    }  /* if */
#if IA64_ABI
    if (current->is_global_qualified_name &&
        !emulate_gnu_abi_bugs) {
      /* Add an indication that the source form used the global scope
         operator. */
      add_str_to_mangled_name("gs", mctl);
    }  /* if */
    if (scp != NULL) add_str_to_mangled_name("sr", mctl);
    if (kind == iek_type &&
        (emulate_gnu_abi_bugs || is_top_level_unresolved_type)) {
      /* The qualifier type is a top-level <unresolved-type>; use an 
         <unresolved-type> encoding rather than a <simple-id> encoding.  */
      if (nesting_level > 1) {
        add_to_mangled_name('N', mctl);
        *need_close = TRUE;
      }  /* if */
      check_assertion(scp != NULL);
      mangled_encoding_for_type((a_type_ptr)scp, mctl);
      scp = NULL;
    } else {
      /* We're not using an <unresolved-type>, so we need a closing 'E'. */
      if (scp != NULL) *need_close = TRUE;
    }  /* if */
#else /* !IA64_ABI */
    if (current->is_global_qualified_name && scp != NULL) {
      /* Add an additional "level" for the global qualification (but not for
         the case where the global qualifier is the only qualifier -- it's
         already accounted for). */
      nesting_level++;
    }  /* if */
    if (nesting_level > 1) {
      /* If we have more than one level of qualifiers, add an indicator that
         contains the total number of qualifiers. */
      add_nesting_level_encoding(nesting_level, mctl);
    }  /* if */
    if (current->is_global_qualified_name) add_str_to_mangled_name("G", mctl);
#endif /* IA64_ABI */
  }  /* if */
  if (scp != NULL) {
#if !IA64_ABI
    if (is_top_level_unresolved_type) {
      /* Emit template parameter encoding rather than the parameter's name if
         this is a top-level unresolved type. */
      mangled_encoding_for_type((a_type_ptr)scp, mctl);
    } else
#endif /* !IA64_ABI */
    /* Do not insert code here. */
    {
      a_const_char *name_ref_name = NULL;
      /* Emit the source name for this qualifier (with any template args). */
      if (kind == iek_type) {
        /* Skip any typedefs. */
        scp = &skip_typerefs((a_type_ptr)scp)->source_corresp;
        if (is_immediate_class_type((a_type_ptr)scp)) {
          ctsp = class_type_supp((a_type_ptr)scp);
          check_assertion(ctsp != NULL &&
                          ctsp->anonymous_union_kind ==
                                            (an_anonymous_union_kind)auk_none);
          template_arg_list = ctsp->template_arg_list;
        }  /* if */
      } else if (kind == iek_namespace &&
                 unmangled_or_fabricated_name_of(scp) == NULL) {
        /* Ensure that an unnamed namespace is given a name. */
        (void)give_unnamed_namespace_a_name((a_namespace_ptr)scp, mctl);
      }  /* if */
      if (current->uses_qualifiers && current->variant.qualifier != NULL) {
        /* If a name reference is used and it has the name that was used
           in the source, use that in cases where the entity is otherwise
           unnamed. */
        name_ref_name = current->variant.qualifier->name;
      }  /* if */
      mangled_simple_id_or_name(scp, name_ref_name, template_arg_list,
                                (a_name_reference_ptr)NULL,
                                /*include_length=*/TRUE, mctl);
    }  /* if */
  }  /* if */
}  /* mangled_scope_resolution */

#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
#if IA64_ABI

/*
Information about a routine needed to call mangled_entity_reference.
This describes everything beyond the name field itself needed to
name a routine, e.g., for conversion or operation functions.
*/
typedef struct a_routine_info_block {
  a_special_function_kind
		special_kind;
  a_type_ptr	conversion_type;
  an_opname_kind
		opname_kind;
  a_template_arg_ptr
		template_arg_list;
  a_const_char  *ud_suffix;
} a_routine_info_block;


static void mangled_entity_reference(a_source_correspondence  *scp,
                                     an_il_entry_kind         kind,
                                     a_routine_info_block     *rinfo,
                                     a_boolean                add_address_of,
                                     a_mangling_control_block *mctl)
/*
Add the encoding for a reference to an entity in an expression, for
the IA-64 ABI.  scp is the source correspondence of the entity, which
has kind "kind".  If rinfo != NULL, the entity is not a routine entry
but it represents a routine, and rinfo points to the information
describing it.  In some cases (as indicated by add_address_of being TRUE)
add mangling for an eok_address_of operation.
*/
{
  a_source_correspondence
             *discriminator_scp = NULL;
  a_type_ptr parent_class = (scp->is_class_member ? scp_parent_class(scp) :
                                                    NULL);
  a_boolean  emulate_old_gnu_behavior = emulate_gnu_abi_bugs
#if ABI_COMPATIBILITY_VERSION >= 402
                                        && gnu_abi_version < 30400
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
                                                                  ;
  a_boolean  use_sr = parent_class != NULL &&
                      (emulate_old_gnu_behavior ||
                       is_template_dependent_type(parent_class));

  if (add_address_of) {
    add_str_to_mangled_name("ad", mctl);
  }  /* if */
  if (use_sr) {
    /* Emit the scope resolution qualification for the entity. */
#if ABI_COMPATIBILITY_VERSION >= 402
    if (!emulate_gnu_abi_bugs) {
      /* There was a major change in the way the scope resolution ("sr")
         mangling is handled (as part of the SFINAE mangling changes).
         This branch represents the newer way that scope resolution
         mangling is handled. */
      a_boolean               need_close = FALSE;
      a_scope_resolution_step step;
      step.uses_qualifiers          = FALSE;
      step.is_global_qualified_name = FALSE;
      step.variant.scp_kind.scp     = &parent_class->source_corresp;
      step.variant.scp_kind.kind    = iek_type;
      mangled_scope_resolution(&step, &need_close, /*nesting_level=*/1, mctl);
      if (need_close) add_to_mangled_name('E', mctl);
    } else
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
    /* Do not insert code here. */
    {
      /* Use the old-style scope resolution operator "sr".  Note that in some
         cases the mangled names that are produced here cannot be demangled. */
      add_str_to_mangled_name("sr", mctl);
      mangled_encoding_for_type(parent_class, mctl);
    }  /* if */
    /* What follows is a <base-unresolved-name>. */
    if (kind == iek_routine) {
      a_routine_ptr rout = (a_routine_ptr)scp;
      mangled_function_name(rout,
                        /*suppress_param_encoding=*/!emulate_old_gnu_behavior,
                        /*suppress_parent_encoding=*/!emulate_old_gnu_behavior,
                        /*force_primary_name=*/TRUE,
                        /*force_individuation=*/FALSE,
                        /*base_name_offset=*/(sizeof_t *)NULL,
                        mctl);
    } else {
      a_boolean need_nested_name_close = FALSE;
      if (emulate_old_gnu_behavior &&
          !is_template_dependent_type(parent_class)) {
        /* g++ 3.3 and earlier put a parent qualifier on member references if
           the parent type is not a template parameter. */
        mangled_ia64_parent_qualifier(scp, kind,
                                      &need_nested_name_close, 
                                      &discriminator_scp,
                                      /*force_individuation=*/FALSE,
                                      mctl);
      }  /* if */
      if (rinfo != NULL) {
        /* Not a routine entry, but it represents a routine (this might be
           the address of an overloaded function, and we can't tell which
           specific function is to be used). */
#if ABI_COMPATIBILITY_VERSION >= 402
        if (!emulate_gnu_abi_bugs &&
            rinfo->special_kind != sfk_none) {
          /* This is some type of special function; make sure it receives
             the proper mangling treatment within an "sr" mangling. */
          mangled_operator_or_special_function(rinfo->opname_kind,
                                        /*num_operands=*/0,
                                        rinfo->conversion_type,
                                        rinfo->ud_suffix,
                                        rinfo->template_arg_list,
                                        (a_name_reference_ptr)NULL,
                                        /*suppress_operation_indicator=*/FALSE,
                                        /*suppress_underscores=*/FALSE,
                                        mctl);
        } else
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
        /* Do not insert code here. */
        {
          mangled_function_base_name(scp,
                                     rinfo->special_kind,
                                     rinfo->opname_kind,
                                     (a_ctor_or_dtor_kind)cdk_none,
                                     /*num_operands=*/0,
                                     rinfo->conversion_type,
                                     rinfo->ud_suffix,
                                     mctl);
        }  /* if */
        if (rinfo->template_arg_list != NULL) {
          /* Put out the template argument list. */
          mangled_template_arguments(rinfo->template_arg_list,
                                     /*partial_spec=*/FALSE,
                                     /*old_form=*/FALSE,
                                     (a_name_reference_ptr)NULL,
                                     mctl);
        }  /* if */
      } else {
        /* Not a routine of any kind. */
        mangled_name_with_length(unmangled_or_fabricated_name_of(scp), mctl);
      }  /* if */
      close_ia64_nested_name(need_nested_name_close, discriminator_scp, mctl);
    }  /* if */
#if ABI_COMPATIBILITY_VERSION >= 402
  } else if (rinfo != NULL && rinfo->ud_suffix != NULL) {
    /* This is a ck_template_param/tpck_unknown_function for a UDL operator
       function; give it a special mangling. */
    mangled_operator_or_special_function(rinfo->opname_kind,
                                         /*num_operands=*/0,
                                         rinfo->conversion_type,
                                         rinfo->ud_suffix,
                                         rinfo->template_arg_list,
                                         (a_name_reference_ptr)NULL,
                                         /*suppress_operation_indicator=*/TRUE,
                                         /*suppress_underscores=*/FALSE,
                                         mctl);
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
  } else if (rinfo != NULL && rinfo->template_arg_list != NULL) {
    /* This is an unqualified, template-dependent routine, which is mangled
       with <unresolved-name>.  It can't be a conversion function or
       operator because those must be member functions. */
    check_assertion(rinfo->conversion_type == NULL &&
                    rinfo->opname_kind == (an_opname_kind)onk_none &&
                    rinfo->special_kind == (a_special_function_kind)sfk_none);
    mangled_simple_id(scp, rinfo->template_arg_list,
                      (a_name_reference_ptr)NULL, /*include_length=*/FALSE,
                      mctl);
  } else {
    /* Use a name as a literal instead of "sr", because the parent class
       is not dependent or the entity is not a class member. */
    a_boolean force_individuation = FALSE;
    add_str_to_mangled_name("L_Z", mctl);
#if ABI_COMPATIBILITY_VERSION >= 402
    if ((kind == iek_routine &&
         (((a_routine_ptr)scp)->storage_class == (a_storage_class)sc_static)
          && !scp->is_class_member) ||
        (kind == iek_variable &&
         ((a_variable_ptr)scp)->storage_class == (a_storage_class)sc_static)) {
      /* This entity has static storage class and needs to be individuated to
         avoid conflicts with similarly named entities in other translation
         units.  Class members don't need individuation. */
      /* Note that g++ does this by adding an 'L' into the mangling at this
         point, but since there's no need to be compatible on this ABI
         extension, use the existing individuation mechanism. */
      force_individuation = TRUE;
    }  /* if */
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
    if (kind == iek_routine) {
      a_routine_ptr rout = (a_routine_ptr)scp;
      a_boolean     suppress_param_encoding;
      /* Don't include parameters if the routine has extern "C" linkage. */
      suppress_param_encoding = !is_name_linkage_kind_subject_to_name_mangling(
                                            rout->source_corresp.name_linkage);
#if ABI_COMPATIBILITY_VERSION >= 402 && BUILTIN_FUNCTIONS_ENABLED
      if (emulate_gnu_abi_bugs && is_gnu_builtin_function(rout)) {
        /* GNU suppresses parameter encodings on GNU-style builtin
           functions. */
        suppress_param_encoding = TRUE;
      }  /* if */
#endif /* ABI_COMPATIBILITY_VERSION >= 402 && BUILTIN_FUNCTIONS_ENABLED */
      mangled_function_name(rout,
                            suppress_param_encoding,
                            /*suppress_parent_encoding=*/FALSE,
                            /*force_primary_name=*/TRUE,
                            force_individuation,
                            /*base_name_offset=*/(sizeof_t *)NULL,
                            mctl);
    } else {
      /* Not a routine. */
      /* Add a parent qualifier for a member if needed. */
      a_boolean need_nested_name_close = FALSE;
      mangled_ia64_parent_qualifier(scp, kind,
                                    &need_nested_name_close, 
                                    &discriminator_scp, force_individuation,
                                    mctl);
      if (rinfo != NULL) {
        /* Not a routine entry, but it represents a routine (this might be
           the address of an overloaded function, and we can't tell which
           specific function is to be used). */
        mangled_function_base_name(scp,
                                   rinfo->special_kind,
                                   rinfo->opname_kind,
                                   (a_ctor_or_dtor_kind)cdk_none,
                                   /*num_operands=*/0,
                                   rinfo->conversion_type,
                                   rinfo->ud_suffix,
                                   mctl);
        if (rinfo->template_arg_list != NULL) {
          /* Put out the template argument list. */
          mangled_template_arguments(rinfo->template_arg_list,
                                     /*partial_spec=*/FALSE,
                                     /*old_form=*/FALSE,
                                     (a_name_reference_ptr)NULL,
                                     mctl);
        }  /* if */
      } else {
        /* Not a routine of any kind. */
        mangled_name_with_length(unmangled_or_fabricated_name_of(scp), mctl);
      }  /* if */
      close_ia64_nested_name(need_nested_name_close, discriminator_scp, mctl);
    }  /* if */
    add_to_mangled_name('E', mctl);
  }  /* if */
}  /* mangled_entity_reference */

#else /* !IA64_ABI */

static void mangled_routine_name(a_routine_ptr            routine,
                                 a_mangling_control_block *mctl)
/*
Add to the mangled name the name of the routine.  Used in cfront ABI only.
*/
{
  a_length_reservation    length_reservation;

  reserve_space_for_length(&length_reservation, mctl);
  mangled_function_name(routine,
                        /*suppress_param_encoding=*/TRUE,
                        /*suppress_parent_encoding=*/FALSE,
                        /*force_primary_name=*/TRUE,
                        /*force_individuation=*/FALSE,
                        /*base_name_offset=*/(sizeof_t *)NULL,
                        mctl);
  fill_in_length(&length_reservation, mctl);
}  /* mangled_routine_name */

#endif /* IA64_ABI */

static void mangled_encoding_for_ptr_to_member_constant(
                                             a_constant_ptr           con,
                                             ARG_UNUSED a_boolean     old_form,
                                             a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the ck_ptr_to_member constant con.
This is used to encode pointer-to-member constants as part of the mangled
names of template classes.  If old_form is TRUE, use the old form of length
specification in the mangling for lengths of literals.
*/
{
#if !IA64_ABI
  a_number_buffer buffer;

  /* Pointer to member:
     For pointers to data members, the offset value encoded as an integer:
       L212  <--- encoding for an offset of "12"
         ^^------ Literal value.
        ^-------- Length of the literal.
       ^--------- "L" indicates a number.
     For pointers to member functions, the __mptr triplet of
     values (delta, index, function or offset), encoded as follows:
       LM0_L2n1_1j
                ^^- Function name, or alternatively "0" if the pointer
                    to member uses an offset (e.g., LM0_L11_0).
           ^^^^---- Index value, encoded as an integer.
         ^--------- Delta value.
       ^^---------- "LM" indicates a pointer to member function.
     This is compatible with cfront 3.0.1.  Note that "0" is always
     used for the offset, not the actual offset value.  This follows
     cfront.  The idea seems to be that "0" is really a way of saying
     "there is no function;" the offset value itself would not be
     of interest to a name demangler. */
  if (!con->variant.ptr_to_member.is_function_ptr) {
    /* Pointer to data member. */
    a_targ_ptrdiff_t delta;
    repr_for_ptr_to_data_member_constant(con, &delta);
    buffer.reset_to(delta);
    /* Use "n" to represent a minus sign. */
    if (buffer[0] == '-') buffer[0] = 'n';
    add_to_mangled_name('L', mctl);
    store_digits_and_underscore((unsigned long)buffer.length(), old_form,
                                mctl);
    add_str_to_mangled_name(buffer.as_temp_characters(), mctl);
  } else {
    /* Pointer to member function. */
    a_targ_ptrdiff_t delta, idx, offset;
    a_routine_ptr    func;

    repr_for_ptr_to_member_function_constant(con, &delta, &idx, &func,
                                             &offset);
    add_str_to_mangled_name("LM", mctl);
    /* Delta value. */
    buffer.reset_to(delta);
    /* Use "n" to represent a minus sign. */
    if (buffer[0] == '-') buffer[0] = 'n';
    add_str_to_mangled_name(buffer.as_temp_characters(), mctl);
    /* Index value. */
    buffer.reset_to(idx);
    /* Use "n" to represent a minus sign. */
    if (buffer[0] == '-') buffer[0] = 'n';
    add_str_to_mangled_name("_L", mctl);
    store_digits_and_underscore((unsigned long)buffer.length(), old_form,
                                mctl);
    add_str_to_mangled_name(buffer.as_temp_characters(), mctl);
    add_to_mangled_name('_', mctl);
    if (func != NULL) {
      a_length_reservation length_reservation;
      /* Name of function. */
      /* The newer version of this includes parent information, but that's
         not compatible with cfront. */
      a_boolean include_parent_info;
#if ABI_COMPATIBILITY_VERSION < 235
      include_parent_info = FALSE;
#else /* ABI_COMPATIBILITY_VERSION >= 235 */
      /* Making this conditional on the new-style mangling for templates
         is a little strange, but if you have the new-style mangling
         you're completely incompatible with cfront, so it's not
         a ridiculous idea. */
      include_parent_info = distinct_template_signatures;
#endif /* ABI_COMPATIBILITY_VERSION < 235 */
      reserve_space_for_length(&length_reservation, mctl);
      if (include_parent_info) {
        /* Include class and namespace information in the name. */
        mangled_function_name(func,
                              /*suppress_param_encoding=*/TRUE, 
                              /*suppress_parent_encoding=*/FALSE,
                              /*force_primary_name=*/TRUE,
                              /*force_individuation=*/FALSE,
                              /*base_name_offset=*/(sizeof_t *)NULL,
                              mctl);
      } else {
        /* Use a simple name (no class or namespace information). */
        a_const_char *str =
                        unmangled_or_fabricated_name_of(&func->source_corresp);

        check_assertion(str != NULL);
        /* Output the name.  Stop on two underscores. */
        for (size_t str_length = 0;
             str[str_length] != '\0' &&
               (str[str_length] != '_' || str[str_length+1] != '_');
             str_length++) {
          add_to_mangled_name(str[str_length], mctl);
        }  /* for */
      }  /* if */
      fill_in_length(&length_reservation, mctl);
    } else {
      /* Offset, always coded as "0". */
      add_to_mangled_name('0', mctl);
    }  /* if */
  }  /* if */
#else /* IA64_ABI */
  a_source_correspondence *scp = NULL;
  an_il_entry_kind        kind = iek_none;
  a_routine_ptr           rout = NULL;
  a_field_ptr             field = NULL;

  if (con->variant.ptr_to_member.is_function_ptr) {
    rout = con->variant.ptr_to_member.variant.routine;
    if (rout != NULL) {
      scp = &rout->source_corresp;
      kind = iek_routine;
    }  /* if */
  } else {
    field = con->variant.ptr_to_member.variant.field;
    if (field != NULL) {
      scp = &field->source_corresp;
      kind = iek_field;
    }  /* if */
  }  /* if */
  if (scp != NULL) {
    mangled_entity_reference(scp, kind, (a_routine_info_block *)NULL,
                             /*add_address_of=*/TRUE, mctl);
  } else {
    /* We have a NULL pointer-to-member constant.  Although not allowed by the
       standard, some compilers accept this as an extension.  The IA64 ABI
       does not specify a mangling for this case; we choose to use the same
       mangling as would be used for an integer constant of this type.  */
    add_to_mangled_name('L', mctl);
    mangled_encoding_for_type(con->type, mctl);
    add_to_mangled_name('0', mctl);
    add_to_mangled_name('E', mctl);
  }  /* if */
#endif /* IA64_ABI */
}  /* mangled_encoding_for_ptr_to_member_constant */


static void mangled_encoding_for_unknown_function(
                                    a_constant_ptr           con,
                                    a_boolean                has_template_args,
                                    a_template_arg_ptr       template_arg_list,
                                    ARG_UNUSED a_boolean     add_address_of,
                                    a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the constant con, which is
a ck_template_param/tpck_unknown_function constant.  This is used
to encode unknown functions that appear in template argument lists
of prototype instantiations.  If has_template_args is TRUE, the function
has an explicit template argument list, given by template_arg_list.
If add_address_of is TRUE, mangling for an "&" operation is added (IA-64 ABI
only).
*/
{
  a_type_ptr              conversion_type =
                                         con->variant.template_param.variant.
                                              unknown_function.conversion_type;
  an_opname_kind          opname_kind =  con->variant.template_param.variant.
                                              unknown_function.opname_kind;
  a_special_function_kind special_kind = (a_special_function_kind)sfk_none;
  a_const_char            *con_name = unmangled_name_of(&con->source_corresp);
  a_const_char            *ud_suffix = NULL;

  /* This routine is a simplified version of mangled_function_name. */
  if (conversion_type != NULL) {
    special_kind = (a_special_function_kind)sfk_conversion;
  } else if (opname_kind != (an_opname_kind)onk_none) {
    special_kind = (a_special_function_kind)sfk_operator;
  }  /* if */
  if (con_name != NULL &&
      strncmp(con_name, CANONICAL_LITERAL_OPERATOR_INTRO,
              LENGTH_CANONICAL_LITERAL_OPERATOR_INTRO) == 0) {
    /* If the constant represents a UDL operator, capture the ud-suffix. */
    ud_suffix = ud_suffix_from_literal_operator_id(con_name);
    special_kind = (a_special_function_kind)sfk_udl_operator;
  }  /* if */
#if !IA64_ABI
  mangled_function_base_name(&con->source_corresp,
                             special_kind,
                             opname_kind,
                             (a_ctor_or_dtor_kind)cdk_none,
                             /*num_operands=*/0,
                             conversion_type,
                             ud_suffix,
                             mctl);
  if (has_template_args) {
    /* Put out the template argument list. */
    mangled_template_arguments(template_arg_list,
                               /*partial_spec=*/FALSE,
                               /*old_form=*/FALSE,
                               (a_name_reference_ptr)NULL,
                               mctl);
  }  /* if */
  if (is_class_or_namespace_member(con)) {
    /* Add a parent qualifier for a member. */
    add_str_to_mangled_name("__", mctl);
    mangled_parent_qualifier(&con->source_corresp, iek_constant, mctl);
  }  /* if */
#else /* IA64_ABI */
  {
    a_routine_info_block rinfo;

    rinfo.special_kind = special_kind;
    rinfo.conversion_type = conversion_type;
    rinfo.opname_kind = opname_kind;
    rinfo.template_arg_list = NULL;
    if (has_template_args) {
      rinfo.template_arg_list = template_arg_list;
    }  /* if */
    rinfo.ud_suffix = ud_suffix;
    mangled_entity_reference(&con->source_corresp, iek_constant,
                             &rinfo, add_address_of, mctl);
  }
#endif /* !IA64_ABI */
}  /* mangled_encoding_for_unknown_function */


static
a_constant_ptr mangled_braced_expression(a_constant_ptr           con,
                                         a_mangling_control_block *mctl);

static void literal_representation(
                                  a_constant_ptr           con,
                                  a_boolean                old_form,
                                  ARG_UNUSED a_boolean     in_dependent_expr,
                                  a_boolean                suppress_address_of,
                                  a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the constant con.
This is used to encode constants as part of the mangled names of
template classes.  If old_form is TRUE, use the old form of length
specification in the mangling for lengths of literals.  For the IA-64
ABI, if in_dependent_expr is TRUE this constant is part of a
template-dependent expression.  Suppress mangling of the implied "address of"
operator on some template constants when suppress_address_of is TRUE
(only in the IA-64 ABI).
*/
{
  a_boolean           has_template_args;
  a_template_arg_ptr  template_arg_list;
  a_constant_ptr      unk_func_con;

#if IA64_ABI
  /* If an expression was recorded for the constant, and we're in
     a template-dependent expression, use the pointer to get
     the unfolded version of the expression as required by the IA-64
     ABI spec.  For constant variables, use the constant not the
     variable name (except when emulating a bug in g++ 3.2).  Enum
     constants also have a non-NULL expression pointer if they were
     given an explicit value in their definitions, but that
     expression shouldn't be put out. */
  if (constant_is(con, ck_template_param)) {
    in_dependent_expr = TRUE;
  }  /* if */
  if (in_dependent_expr && con->expr != NULL &&
      !is_enum_constant(con) &&
      (con->expr->kind != (an_expr_node_kind)enk_variable ||
       emulate_gnu_abi_bugs)) {
    mangled_encoding_for_expression(con->expr, in_dependent_expr, mctl);
    goto end_of_routine;
  }  /* if */
#endif /* IA64_ABI */
  switch (con->kind) {
    case ck_error:
      /* This might come up in mangling names for template instantiations. */
      add_to_mangled_name('?', mctl);
      break;
    case ck_integer:
#if IA64_ABI
      if (emulate_gnu_abi_bugs && in_dependent_expr && has_name(con) &&
          gnu_abi_version <= 30200) {
        /* g++ 3.2 puts out the names of enum constants instead of their
           values in dependent expressions. */
        mangled_entity_reference(&con->source_corresp,
                                 iek_constant,
                                 (a_routine_info_block *)NULL,
                                 /*add_address_of=*/FALSE, mctl);
        break;
      }  /* if */
#endif /* IA64_ABI */
      { a_number_buffer value = decimal_str_for_integer_constant(con);

        mangled_encoding_for_integer(value, con->type, old_form, mctl);
      }
      break;
    case ck_float:
      /* Float constant. */
      mangled_encoding_for_float_constant(con, old_form, mctl);
      break;
    case ck_address:
      /* Address.  Put out the name of the entity whose address is involved. */
      if ((con->variant.address.kind == (an_address_base_kind)abk_constant &&
           constant_is(con->variant.address.variant.constant, ck_string)) ||
          con->variant.address.kind == (an_address_base_kind)abk_temporary) {
        /* Mangle the string constant or temporary representation of a
           constant. */
        mangled_encoding_for_constant(con->variant.address.variant.constant,
                                      /*old_form=*/FALSE,
                                      /*in_dependent_expr=*/FALSE,
                                      /*suppress_address_of=*/FALSE,
                                      mctl);
      } else {
        mangled_encoding_for_address_constant_and_possible_subobject_path(con,
                                                                         mctl);
      }  /* if */
      break;
    case ck_ptr_to_member:
      /* Pointer to member. */
      mangled_encoding_for_ptr_to_member_constant(con, old_form, mctl);
      break;
    case ck_template_param:
      /* This comes up when mangling the names for template entities using
         the modern mangling approach. */
      switch (con->variant.template_param.kind) {
        case tpck_param:
          /* A simple reference to a template parameter. */
          mangled_encoding_for_template_parameter(
                              &con->variant.template_param.variant.coordinates,
                              (a_template_arg *)NULL,
                              mctl);
          break;
        case tpck_expression:
          /* An expression involving template parameters. */
          mangled_encoding_for_expression(expr_node_from_tpck_expression(con),
                                          /*in_dependent_expr=*/TRUE, mctl);
          break;
        case tpck_template_ref:
          /* An unknown function template with a list of explicit template
             arguments.  The template is given by an underlying
             tpck_unknown_function constant. */
          has_template_args = TRUE;
          template_arg_list = con->variant.template_param.variant.
                                                         template_ref.arg_list;
          unk_func_con = con->variant.template_param.variant.template_ref.con;
          check_assertion(is_unknown_function_constant(unk_func_con));
          goto do_unknown_function;
        case tpck_unknown_function:
          /* An unknown function, which may be a member of a class or
             namespace, and may be a conversion function or operator
             function. */
          has_template_args = FALSE;
          template_arg_list = NULL;
          unk_func_con = con;
do_unknown_function:
          { 
#if !IA64_ABI
            a_length_reservation length_reservation;
            reserve_space_for_length(&length_reservation, mctl);
#endif /* !IA64_ABI */
            mangled_encoding_for_unknown_function(unk_func_con,
                                                  has_template_args,
                                                  template_arg_list,
                                                  !suppress_address_of,
                                                  mctl);
#if !IA64_ABI
            fill_in_length(&length_reservation, mctl);
#endif /* !IA64_ABI */
          }
          break;
        case tpck_member:
          /* A member of a template parameter type, e.g., T::x. */
#if !IA64_ABI
          { a_length_reservation length_reservation;
            reserve_space_for_length(&length_reservation, mctl);
            mangled_name_with_possible_qualification(&con->source_corresp,
                                                     iek_constant,
                                                     (a_template_ptr)NULL,
                                                     mctl);
            fill_in_length(&length_reservation, mctl);
          }
#else /* IA64_ABI */
          mangled_entity_reference(&con->source_corresp,
                                   iek_constant,
                                   (a_routine_info_block *)NULL,
                                   /*add_address_of=*/FALSE, mctl);
#endif /* !IA64_ABI */
          break;
        case tpck_dependent_constant:
          literal_representation(con->variant.template_param.variant.constant,
                                 old_form,
                                 /*in_dependent_expr=*/TRUE,
                                 suppress_address_of,
                                 mctl);
          break;
        case tpck_concat_string_literals:
          { a_constant_ptr  elem_cp = con->variant.template_param
                                          .variant.string_literal_list;
            for (; elem_cp != NULL; elem_cp = elem_cp->next) {
              literal_representation(elem_cp, old_form,
                                     /*in_dependent_expr=*/TRUE,
                                     suppress_address_of, mctl);
            }  /* for */
          }
          break;
        case tpck_address:
#if !IA64_ABI
          /* For an address, just mangle the member name. */
          literal_representation(con->variant.template_param.variant.constant,
                                 old_form,
                                 /*in_dependent_expr=*/TRUE,
                                 /*suppress_address_of=*/FALSE,
                                 mctl);
#else /* IA64_ABI */
          con = con->variant.template_param.variant.constant;
          check_assertion(constant_is(con, ck_template_param) &&
                          tpck_is(con, tpck_member));
          mangled_entity_reference(&con->source_corresp, iek_constant,
                                   (a_routine_info_block *)NULL, 
                                   /*add_address_of=*/TRUE, mctl);
#endif /* IA64_ABI */
          break;
        case tpck_sizeof:
        case tpck_alignof:
        case tpck_uuidof:
        case tpck_typeid:
        case tpck_noexcept:
          mangled_encoding_for_sizeof(
                         con->variant.template_param.variant.templ_sizeof.type,
                         generic_sizeof_arg_expr(con),
                         con->variant.template_param.kind,
                         (an_expr_node_ptr)NULL,
                         mctl);
          break;
        case tpck_integer_pack:
          /* GCC models "__integer_pack(<expr>)..." as the pack expansions of
             a function call. */
#if !IA64_ABI
          add_str_to_mangled_name("Osp_1_cl_1_14__integer_pack", mctl);
#else /* IA64_ABI */
          add_str_to_mangled_name("spclL_Z14__integer_packE", mctl);
#endif /* IA64_ABI */
          literal_representation(con->variant.template_param.variant.bound,
                                 old_form,
                                 /*in_dependent_expr=*/TRUE,
                                 /*suppress_address_of=*/FALSE,
                                 mctl);
#if !IA64_ABI
          add_to_mangled_name('O', mctl);
#else /* IA64_ABI */
          add_to_mangled_name('E', mctl);
#endif /* IA64_ABI */
          break;
        case tpck_destructor:
        default:
          unexpected_condition_str(
                            "literal_representation: bad template param kind");
      }  /* switch */
      break;
    case ck_string:
      /* Strings can appear in expressions (e.g., in decltype). */
#if IA64_ABI
      add_to_mangled_name('L', mctl);
      mangled_encoding_for_type(con->type, mctl);
      add_to_mangled_name('E', mctl);
#else /* !IA64_ABI */
      /* The string type has already been emitted by the caller. */
      add_str_to_mangled_name("LS", mctl);
#endif /* IA64_ABI */
      break;
    case ck_aggregate:
#if C99_IL_EXTENSIONS_SUPPORTED
      /* Mangle a complex aggregate. */
      if (con->type->kind == (a_type_kind)tk_complex) {
        mangled_encoding_for_complex_constant(con, old_form, mctl);
      } else
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      /* Do not insert code here. */
      {
        /* Mangle an aggregate constant using an initializer-list mangling. */
        mangled_braced_init_list((an_expr_node_ptr)NULL, con, con->type, mctl);
      }  /* if */
      break;
    case ck_dynamic_init:
      /* These can occur while mangling constants in a compound literal
         aggregate. */
      mangled_dynamic_init(con->variant.dynamic_init.ptr, con->type,
                           /*is_static_cast=*/FALSE, mctl);
      break;
#if C99_IL_EXTENSIONS_SUPPORTED
    case ck_complex:
      mangled_encoding_for_complex_constant(con, old_form, mctl);
      break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case ck_designator:
      (void)mangled_braced_expression(con, mctl);
      break;
    case ck_init_repeat:
      /* Handle a repeated constant. */
      for (a_targ_size_t i = 0; i < con->variant.init_repeat.count; ++i) {
        mangled_encoding_for_constant(con->variant.init_repeat.constant,
                                      old_form, in_dependent_expr,
                                      suppress_address_of, mctl);
      }  /* for */
      break;
    case ck_reflection:
      { a_reflection_value  rv = con->variant.reflection;
        a_tagged_pointer    *iep;
        /* Normalize template-argument reflections to the underlying entity,
           matching form_reflection / query predicates. */
        strip_template_arg(&rv);
        iep = &rv.entity;
        if (iep->kind == iek_scope) {
          a_scope  *scope = (a_scope*)iep->ptr;
          /* Genuine namespaces are reflected as scopes; mangle them via the
             associated namespace entry.  The global namespace (sck_file) is
             left as iek_scope and handled below. */
          if (scope_is(scope, sck_namespace) ||
              scope_is(scope, sck_namespace_extension)) {
            iep->kind = iek_namespace;
            iep->ptr = (char*)scope->variant.assoc_namespace;
          }  /* if */
        }  /* if */
        /* FIXME reflection: Use better prefix and add demangler support. */
        add_str_to_mangled_name("__REFL__", mctl);
        switch (iep->kind) {
          case iek_type:
            mangled_encoding_for_type((a_type*)iep->ptr, mctl);
            break;
          case iek_attribute:
            { an_attribute  *ap = (an_attribute*)iep->ptr;
              a_constant    *cp;
              check_assertion(ap->kind == ak_annotation &&
                              ap->arguments != NULL &&
                              ap->arguments->kind == aak_constant);
              cp = ap->arguments->variant.constant;
              add_number_to_mangled_name(
                    (a_host_large_unsigned)unique_id_for_il_pointer(ap), mctl);
              literal_representation(cp, old_form,
                                     /*in_dependent_expr=*/FALSE,
                                     /*suppress_address_of=*/FALSE, mctl);

            }
            break;
#if IA64_ABI
          case iek_field:
          case iek_variable:
          case iek_routine:
          case iek_template:
          case iek_namespace:
            mangled_entity_reference((a_source_correspondence*)iep->ptr,
                                     iep->kind, (a_routine_info_block*)NULL,
                                     /*add_address_of=*/FALSE, mctl);
            break;
#else /* !IA64_ABI */
          case iek_field:
          case iek_variable:
          case iek_routine:
          case iek_template:
          case iek_namespace:
            { a_length_reservation length_reservation;
              reserve_space_for_length(&length_reservation, mctl);
              mangled_name_with_possible_qualification(
                                (a_source_correspondence*)iep->ptr, iep->kind,
                                (a_template_ptr)NULL, mctl);
              fill_in_length(&length_reservation, mctl);
            }
            break;
#endif /* IA64_ABI */
          case iek_constant:
            literal_representation((a_constant*)iep->ptr, old_form,
                                   /*in_dependent_expr=*/FALSE,
                                   /*suppress_address_of=*/FALSE, mctl);
            break;
          case iek_expr_node:
            mangled_encoding_for_expression((an_expr_node*)iep->ptr,
                                            /*in_dependent_expr=*/FALSE,
                                            mctl);
            break;
          case iek_base_class:
            { a_base_class  *bcp = (a_base_class*)iep->ptr;
              mangled_encoding_for_type(bcp->type, mctl);
              mangled_encoding_for_type(bcp->derived_class, mctl);
            }
            break;
          case iek_scope:
            /* Global namespace (^^::). */
            check_assertion(scope_is((a_scope*)iep->ptr, sck_file));
            add_str_to_mangled_name("v7__GLOBAL", mctl);
            break;
          case iek_token_sequence:
            /* FIXME: Encode tokens. */
            add_str_to_mangled_name("v9tokenseq_", mctl);
            add_number_to_mangled_name(unique_id_for_il_pointer(iep->ptr),
                                       mctl);
            add_str_to_mangled_name("_", mctl);
            break;
          default:
            unexpected_condition_str(
                               "literal_representation: bad reflection kind");
        }  /* switch */
        add_str_to_mangled_name("__ENDREFL__", mctl);
      }
      break;
#if FIXED_POINT_ALLOWED
    case ck_fixed_point:
      /* C++ modes do not currently allowed fixed-point types, and therefore
         no mangling should be needed for them. */
      FALLTHROUGH
#endif /* FIXED_POINT_ALLOWED */
    default:
      unexpected_condition_str("literal_representation: bad constant kind");
  }  /* switch */
#if IA64_ABI
end_of_routine:;
#endif /* IA64_ABI */
}  /* literal_representation */


static void mangled_encoding_for_constant(
                                  a_constant_ptr           con,
                                  a_boolean                old_form,
                                  a_boolean                in_dependent_expr,
                                  a_boolean                suppress_address_of,
                                  a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the constant con.
If old_form is TRUE, use the old form of length specification in the
mangling for lengths of literals.  For the IA-64 ABI, if in_dependent_expr
is TRUE this constant is part of a template-dependent expression.
When suppress_address_of is TRUE, mangling for the implied "address of"
operation that is part of certain template constants is suppressed
(used only in the IA-64 ABI).
*/
{
#if !IA64_ABI
  /* Representation is something like
       CiL15   <-- integer constant 5
           ^-- Literal constant representation.
          ^--- Length of literal constant.
         ^---- L indicates literal constant; c indicates address
               of variable, etc.
       ^^----- Type of constant, with "const" added.
     If the constant is a template parameter constant, skip the "C" and
     the type.  Likewise for other constants that don't have a basic
     type. */
  if (con->kind != (a_constant_repr_kind)ck_template_param &&
      con->kind != (a_constant_repr_kind)ck_dynamic_init &&
      con->kind != (a_constant_repr_kind)ck_aggregate &&
      con->kind != (a_constant_repr_kind)ck_address &&
      con->kind != (a_constant_repr_kind)ck_init_repeat &&
      con->kind != (a_constant_repr_kind)ck_designator) {
    a_type_ptr con_type = con->type;
    add_to_mangled_name('C', mctl);
    /* Put out the constant type. */
    mangled_encoding_for_type(con_type, mctl);
  }  /* if */
#endif /* !IA64_ABI */
  /* Put out the literal representation for the constant. */
  literal_representation(con, old_form, in_dependent_expr, suppress_address_of,
                         mctl);
}  /* mangled_encoding_for_constant */

#if IA64_ABI && ABI_COMPATIBILITY_VERSION < 402

static Small_string<50> bad_mangled_expr_operator_name(an_expr_node_ptr expr)
/*
expr has an expression operator that is not ordinarily valid in an IA-64
mangled name but is allowed under a sizeof expression.  Return the
operator name mangling.  This is only used in versions prior to 4.2
(in 4.2 and later, all operators should have mangled encodings).
*/
{
  unsigned long num_operands;

  /* We expect these names only in nonreal class types and prototype
     instantiations when MANGLE_ALL_NAMES and PROTOTYPE_INSTANTIATIONS_IN_IL
     are TRUE, but depending on the resolution of core issue 339 there
     may be some operators that might be legitimate here.  For the most
     part, however, we just want to get out of here with a valid mangled
     name; it doesn't matter a great deal what it is. */
  /* Count the number of operands. */
  check_assertion(is_operation_node(expr));
  num_operands = number_of_operands_in_list(expr->variant.operation.operands);
  /* Limit the number of operands to a single digit.  Cases with more
     operands will not demangle correctly. */
  if (num_operands > 9) num_operands = 9;
  /* Use the IA-64 ABI form for a vendor extended operator of "unknown". */
  return Small_string<50>("v", num_operands, "7unknown");
}  /* bad_mangled_expr_operator_name */

#endif /* IA64_ABI && ABI_COMPATIBILITY_VERSION < 402 */

static void add_mangling_for_placeholder_expression(
                                                a_mangling_control_block *mctl)
/*
Output the mangling for an expression that is a placeholder for
something in error or skipped over.  The mangling used is a constant zero.
*/
{
  a_constant_ptr zero_constant = local_constant();

  make_zero_of_proper_type(integer_type((an_integer_kind)ik_int),
                           zero_constant);
  mangled_encoding_for_constant(zero_constant,
                                /*old_form=*/FALSE,
                                /*in_dependent_expr=*/FALSE,
                                /*suppress_address_of=*/FALSE,
                                mctl);
  release_local_constant(&zero_constant);
}  /* add_mangling_for_placeholder_expression */


static void mangled_encoding_for_param_reference(
                                                an_expr_node_ptr         expr,
                                                a_mangling_control_block *mctl)
/*
Add an encoding for the function parameter as specified in expr.  These can
appear in late-specified return types.
*/
{
  a_type_qualifier_set  cv_quals = get_type_qualifiers(expr->type);

  check_assertion(expr->kind == (an_expr_node_kind)enk_param_ref);
#if IA64_ABI
  if (expr->variant.param_ref.levels_up == 0 || emulate_gnu_abi_bugs) {
    add_str_to_mangled_name("fp", mctl);
  } else {
    add_str_to_mangled_name("fL", mctl);
    add_number_to_mangled_name(expr->variant.param_ref.levels_up-1, mctl);
    add_to_mangled_name('p', mctl);
  }  /* if */
  if (expr->variant.param_ref.param_num == 0) {
    /* An explicit "this" in a return type. */
    add_to_mangled_name('T', mctl);
  } else {
    if (cv_quals != 0 && !emulate_gnu_abi_bugs) {
      /* Add cv-qualifiers. */
      mangled_encoding_for_type_qualifiers(cv_quals, mctl);
    }  /* if */
    if (expr->variant.param_ref.param_num > 1) {
      add_number_to_mangled_name(expr->variant.param_ref.param_num-2, mctl);
    }  /* if */
    add_to_mangled_name('_', mctl);
  }  /* if */
#else /* !IA64_ABI */
  /* Parameter reference.  Output has the form
      v-vv----- These are optional.
     IC1_2I <-- "const param#1 two levels up"
          ^---- Terminating non-digit character so parameter number won't run
                into an entity with an initial length.
        ^^----- Number of "levels up" for this parameter (0-based).  Omitted
                if zero.
       ^------- Parameter number (1-based) or 0 for "this".
      ^-------- Optional cv-qualifiers.
     ^--------- "I" indicates parameter reference.  */
  add_to_mangled_name('I', mctl);
  if (cv_quals != 0) mangled_encoding_for_type_qualifiers(cv_quals, mctl);
  add_number_to_mangled_name(expr->variant.param_ref.param_num, mctl);
  if (expr->variant.param_ref.levels_up != 0) {
    add_to_mangled_name('_', mctl);
    add_number_to_mangled_name(expr->variant.param_ref.levels_up, mctl);
  }  /* if */
  add_to_mangled_name('I', mctl);
#endif /* IA64_ABI */
}  /* mangled_encoding_for_param_reference */

#if IA64_ABI

/*
Macro that returns TRUE if the expression is a tpck_typeid template parameter
constant.
*/
#define is_typeid_template_param(expr)                                   \
  (((expr)->kind == (an_expr_node_kind)enk_constant) &&                  \
   (node_constant(expr)->kind ==                                         \
                            (a_constant_repr_kind)ck_template_param &&   \
    node_constant(expr)->variant.template_param.kind ==                  \
                           (a_template_param_constant_kind)tpck_typeid))

#endif /* IA64_ABI */

static a_dynamic_init_ptr skip_compiler_generated_initialization(
                                                        a_dynamic_init_ptr dip)
/*
Skip over any compiler-generated initialization that should be ignored by
mangling (the intent is to provide mangling that describes the original
source code, not what the front end has distilled it into).
*/
{
  a_dynamic_init_ptr prev_dip = NULL;

  while (dip != prev_dip) {
    prev_dip = dip;
    /* For constexpr constructors, skip the constant form to retrieve the
       underlying dik_constructor. */
    dip = skip_constexpr_init_folding(dip);
    if (dip->is_creation_of_initializer_list_object) {
      /* Get the dynamic initialization entry for the underlying temporary
         array for this std::initializer_list object. */
      dip = effective_dynamic_init_for_initializer_list_object(dip,
                                                              (a_type **)NULL);
    }  /* if */
    check_assertion(dip != NULL);
  }  /* while */
  return dip;
}  /* skip_compiler_generated_initialization */


static an_expr_node_ptr skip_compiler_generated_expressions(
                                     an_expr_node_ptr     expr,
                                     ARG_UNUSED a_boolean *suppress_address_of)
/*
This routine skips any expressions that appear at the top of the given
expression that aren't relevant to mangling (e.g., compiler-generated
operations, parentheses, etc.) so that the mangled output will accurately
reflect the source code.  *suppress_address_of is set to TRUE
when the caller (in the IA-64 ABI) needs to suppress an implicit "&" operation
when mangling the expression that is returned (otherwise the value of
*suppress_address_of is unchanged).  This can happen, for example, on a
tpck_typeid template parameter constant (because the compiler-generated "*" has
been removed here).  Note that not all compiler-generated expressions are
stripped here; some are intentionally left so that the construct can be
explicitly dealt with later in expression mangling.  Can return NULL in some
unusual situations (e.g., a compiler-generated dynamic init constructor
call that has no arguments).
*/
{
  an_expr_operator_kind op;
  an_expr_node_ptr      prev_expr = NULL;

  /* Drop implicit operations. */
  while (expr != prev_expr && expr != NULL) {
    /* Drop any parentheses. */
    expr = skip_parens(expr);
    prev_expr = expr;
    if (is_operation_node(expr)) {
      an_expr_node_ptr  child = expr->variant.operation.operands;
      op = expr->variant.operation.kind;
#if IA64_ABI
      if (op == (an_expr_operator_kind)eok_lvalue &&
          is_constant_node(child) &&
          node_constant(child)->kind ==
                                     (a_constant_repr_kind)ck_template_param &&
          (node_constant(child)->variant.template_param.kind ==
                       (a_template_param_constant_kind)tpck_unknown_function ||
           node_constant(child)->variant.template_param.kind ==
                          (a_template_param_constant_kind)tpck_template_ref)) {
        /* A tpck_unknown_function/tpck_template_ref constant is typically
           mangled with an implied "address of" operation, but in this case,
           that mangling should be suppressed since there isn't an "&" in the
           source. */
        check_assertion(!*suppress_address_of);
        *suppress_address_of = TRUE;
        expr = child;
        break;
      } else
#endif /* IA64_ABI */
      /* Do not insert code here. */
      {
        if (op == (an_expr_operator_kind)eok_lvalue ||
            op == (an_expr_operator_kind)eok_lvalue_adjust ||
            op == (an_expr_operator_kind)eok_class_rvalue_adjust ||
            op == (an_expr_operator_kind)eok_array_to_pointer ||
            op == (an_expr_operator_kind)eok_reference_to ||
            op == (an_expr_operator_kind)eok_ref_indirect ||
            op == (an_expr_operator_kind)eok_unbox_lvalue ||
            expr->variant.operation.implicit_step_of_explicit_cast ||
            /* Also drop implicit casts in the IA-64 ABI. */
            (is_cast_operation_node(expr) &&
#if !IA64_ABI
             /* In the Cfront ABI, compiler generated casts are not typically
                removed, except for ones that are going to cause mangling
                problems (i.e., casts to a tptk_unknown type). */
             ((expr->type->kind == tk_template_param &&
               expr->type->variant.template_param.kind == tptk_unknown) ||
              /* Also remove base class casts in the Cfront ABI. */
              (op == (an_expr_operator_kind)eok_base_class_cast ||
               op == (an_expr_operator_kind)eok_pm_base_class_cast)) &&
#endif /* !IA64_ABI */
             expr->compiler_generated)) {
          /* These are all inserted by the compiler and don't represent
             explicit constructs in the source code. */
          expr = child;
        } else if (expr->compiler_generated) {
          /* Remove various compiler-generated operations so the mangling
             accurately reflects the original source. */
#if ABI_COMPATIBILITY_VERSION >= 402
          if (expr->variant.operation.is_conversion_call) {
            /* Remove a compiler-generated conversion operation.  Go to the
               second operand (the input to the conversion function). */
            check_assertion(child != NULL && child->next != NULL);
            expr = child->next;
          } else
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
          /* Do not insert code here. */
          {
            if (op == (an_expr_operator_kind)eok_dot_field &&
                is_variable_node(child) &&
                node_variable(child)->is_anonymous_parent_object) {
              /* Remove a compiler-generated "." operation added to access a
                 member of an anonymous union. */
              expr = child->next;
#if IA64_ABI
            } else if (op == (an_expr_operator_kind)eok_indirect &&
                is_typeid_template_param(child)) {
              /* Suppress the implicit "&" operation on a typeid template
                 parameter constant if it is under a compiler-generated "*". */
              check_assertion(!*suppress_address_of);
              *suppress_address_of = TRUE;
              expr = child;
              break;
            } else if (is_operation_node(child)) {
              if (child->compiler_generated &&
                  ((op == eok_address_of &&
                    child->variant.operation.kind == eok_indirect) ||
                   (op == eok_indirect &&
                    child->variant.operation.kind == eok_address_of))) {
                /* Remove compiler-generated "&*" or "*&" sequences. */
                expr = child->variant.operation.operands;
              }  /* if */
#endif /* IA64_ABI */
            } else if (op == (an_expr_operator_kind)eok_vector_fill) {
              /* Compiler-generated vector fill operations don't appear in the
                 source.*/
              expr = child;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    } else if (expr->kind == (an_expr_node_kind)enk_temp_init) {
      a_dynamic_init_ptr  dip = expr->variant.init.dynamic_init;
      dip = skip_compiler_generated_initialization(dip);
      if (is_generated_dynamic_init(dip)) {
        /* Remove implicit operations. */
        if (dip->kind == (a_dynamic_init_kind)dik_constant ||
            dip->kind == (a_dynamic_init_kind)dik_nonconstant_aggregate ||
            dip->kind == (a_dynamic_init_kind)dik_lambda) {
          /* Allocate an expression node to hold the constant that needs
             mangling (no substitutions are used for constants, so this should
             be okay). */
          expr = alloc_expr_node((an_expr_node_kind)enk_constant);
          node_constant(expr) = dip->variant.constant.ptr;
          expr->type = node_constant(expr)->type;
        } else {
          /* Note that expr may be set to NULL here in some cases (e.g.,
             dik_constructor where the constructor has no arguments). */
          expr = arg_list_from_dyn_init(dip);
        }  /* if */
      }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED && CHECKING
    } else if (expr->kind == (an_expr_node_kind)enk_gcnew) {
      /* Compiler-generated gcnew shouldn't get to mangling. */
      check_assertion(!expr->compiler_generated);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED && CHECKING */
    }  /* if */
  }  /* while */
  return skip_parens(expr);
}  /* skip_compiler_generated_expressions */


static void mangled_expression_list(an_expr_node_ptr         expr,
                                    a_boolean                in_dependent_expr,
                                    a_mangling_control_block *mctl)
/*
Utility to emit mangled encodings for the given expr and all that follow
it (often arguments to some type of call operand).  In the IA-64 ABI,
in_dependent_expr is TRUE if this expression is part of a template-dependent
expression. 
*/
{
  for (; expr != NULL && !expr->generated_default_arg; expr = expr->next) {
    if (expr->is_pack_expansion) {
      /* This expression represents a pack expansion; add the appropriate
         mangling to indicate such. */
#if IA64_ABI
      add_str_to_mangled_name("sp", mctl);
#else /* !IA64_ABI */
      /* Pack expansion operation.  Output has the form
           Osp_1_Z1O <-- "Z1..."
                   ^---- "O" to end the operation encoding.
                 ^^----- First (and only) operand.
              ^^^------- Count of operands (always one).
            ^^---------- Pack expansion operation.
           ^------------ "O" for operation.
      */
      /* Put out the initial "O". */
      add_to_mangled_name('O', mctl);
      add_str_to_mangled_name("sp", mctl);
      store_digits_and_underscore((unsigned long)1, /*old_form=*/FALSE, mctl);
      /* Close the operation after the operand is emitted. */
#endif /* IA64_ABI */
    }  /* if */
    mangled_encoding_for_expression(expr, in_dependent_expr, mctl);
#if !IA64_ABI
    if (expr->is_pack_expansion) {
      add_to_mangled_name('O', mctl);
    }  /* if */
#endif /* !IA64_ABI */
  }  /* for */
}  /* mangled_expression_list */


static void mangled_simple_id(a_source_correspondence_ptr scp,
                              a_template_arg_ptr          template_arg_list,
                              a_name_reference_ptr        name_reference,
                              ARG_UNUSED a_boolean        include_length,
                              a_mangling_control_block    *mctl)
/*
Add to the mangled name the source name of the entity specified by scp.  
This is used to implement the <simple-id> production that is part of the
<unresolved-name> IA-64 rule and is not meant to be a general purpose
mechanism for mangling an entity.  The same mangling method is used for
both IA-64 and Cfront ABIs (i.e., length followed by name and template
arguments).  If template_arg_list is non-NULL, template arguments are also
mangled.  name_reference (when non-NULL) is used to ensure that the mangled
list of template arguments accurately represents those that appeared in the
source form.  In the Cfront ABI, if include_length is TRUE, the length of the
mangled name (including any template arguments) is prefixed to the name.
*/
{
  a_const_char         *str;
#if !IA64_ABI
  a_length_reservation length_reservation;

  if (include_length) reserve_space_for_length(&length_reservation, mctl);
#endif /* !IA64_ABI */
  str = unmangled_or_fabricated_name_of(scp);
  check_assertion(str != NULL);
#if IA64_ABI
  add_number_to_mangled_name((unsigned long)strlen(str), mctl);
#endif /* IA64_ABI */
  add_str_to_mangled_name(str, mctl);
  if (name_reference == NULL ? template_arg_list != NULL :
                               name_reference->is_template_id) {
    /* Put out the template argument list (or a null list), if any. */
    mangled_template_arguments(template_arg_list, /*partial_spec=*/FALSE,
                               /*old_form=*/FALSE, name_reference, mctl);
  }  /* if */
#if !IA64_ABI
  if (include_length) fill_in_length(&length_reservation, mctl);
#endif /* !IA64_ABI */
}  /* mangled_simple_id */


static void mangled_simple_id_or_name(
                              a_source_correspondence_ptr scp,
                              a_const_char                *name,
                              a_template_arg_ptr          template_arg_list,
                              a_name_reference_ptr        name_reference,
                              a_boolean                   include_length,
                              a_mangling_control_block    *mctl)
/*
In cases where a mangled name is needed for an unnamed entity, the name of
the entity may have been captured in a name reference (but not in the entity
itself).  For such cases (when scp->name is NULL and "name" is not), use the
name reference name when mangling the simple id.  Other than "name", the
other arguments are the same as described in mangled_simple_id.
*/
{
  a_boolean restore_name = FALSE;

  if (scp->name == NULL && name != NULL) {
    scp->name = name;
    restore_name = TRUE;
  }  /* if */
  mangled_simple_id(scp, template_arg_list, name_reference, include_length,
                    mctl);
  if (restore_name) {
    scp->name = NULL;
  }  /* if */
}  /* mangled_simple_id_or_name */

#if ABI_COMPATIBILITY_VERSION >= 402
#if IA64_ABI

static void is_gnu_dependent_expr_node(
                                    an_expr_node_ptr                    expr,
                                    an_expr_or_stmt_traversal_block_ptr tblock)
/*
Used during an expression traversal to skip subexpressions that are never
type-dependent (see [temp.dep.expr]).  GNU doesn't consider these
subexpressions when considering whether or not the top level expression is
dependent.
*/
{
  if (is_template_dependent_type(expr->type)) {
    /* Found a dependent type, terminate the search and return TRUE. */
    tblock->result = TRUE;
    tblock->terminate = TRUE;
  } else if (expr->kind == (an_expr_node_kind)enk_sizeof ||
             expr->kind == (an_expr_node_kind)enk_alignof ||
             expr->kind == (an_expr_node_kind)enk_typeid ||
             (expr->kind == (an_expr_node_kind)enk_new_delete &&
              !expr->variant.new_delete->is_new) ||
             expr->kind == (an_expr_node_kind)enk_throw ||
             expr->kind == (an_expr_node_kind)enk_temp_init ||
             (is_cast_operation_node(expr) && !expr->compiler_generated) ||
             (is_operation_node(expr) &&
              (node_operator_is(expr, eok_dot_vacuous_destructor_call) ||
               node_operator_is(expr, eok_points_to_vacuous_destructor_call))))
                                                                              {
    /* Ignore any subexpressions under a sizeof, typeid, delete, or throw. 
       Also, ignore any cast subexpressions to non-dependent types
       (compiler-generated casts are ignored for mangling purposes, so these
       are handled elsewhere) as well as subexpressions under a pseudo
       destructor. */
    tblock->suppress_subtree_walk = TRUE;
  }  /* if */
}  /* is_gnu_dependent_expr_node */


static void is_gnu_dependent_constant(
                                    a_constant_ptr                      con,
                                    an_expr_or_stmt_traversal_block_ptr tblock)
/*
Used during a constant traversal to skip subexpressions that are never
type-dependent (see [temp.dep.expr]).  GNU doesn't consider these
subexpressions when considering whether or not the top level expression is
dependent.
*/
{
  if (constant_is(con, ck_template_param) &&
      (tpck_is(con, tpck_sizeof) ||
       tpck_is(con, tpck_alignof) ||
       tpck_is(con, tpck_typeid) ||
       tpck_is(con, tpck_noexcept))) {
    /* Don't look under a sizeof, alignof, typeid, or noexcept. */
    tblock->suppress_subtree_walk = TRUE;
  } else if (constant_is(con, ck_template_param) &&
             (tpck_is(con, tpck_dependent_constant) ||
              tpck_is(con, tpck_concat_string_literals))) {
    /* These wrappers explicitly make the constant dependent. */
    tblock->result = TRUE;
    tblock->terminate = TRUE;
  } else if (is_template_dependent_type(con->type)) {
    /* Found a dependent type, terminate the search and return TRUE. */
    tblock->result = TRUE;
    tblock->terminate = TRUE;
  }  /* if */
}  /* is_gnu_dependent_constant */


static a_boolean is_gnu_dependent_expression(an_expr_node_ptr expr)
/*
Returns TRUE if the given expression should be considered dependent by
GNU's standards when mangling decltype expressions.  This routine traverses
the expression and returns TRUE if any type within the expression is
dependent, but doesn't walk any subtrees of expressions listed in 
[temp.dep.expr] that are listed as never type-dependent (e.g., sizeof, alignof,
typeid, etc.).  For example, this routine would return FALSE for the
expression "sizeof(T)+1" even though it is instantiation-dependent.
*/
{
  an_expr_or_stmt_traversal_block tblock;

  clear_expr_or_stmt_traversal_block(&tblock);
  tblock.result = FALSE;
  tblock.process_expressions_for_constants = TRUE;
  tblock.process_expr = is_gnu_dependent_expr_node;
  tblock.process_constant = is_gnu_dependent_constant;
  traverse_expr(expr, &tblock);
  return tblock.result;
}  /* is_gnu_dependent_expression */


static a_boolean args_are_dependent(an_expr_node_ptr arguments)
/*
Returns TRUE if any of the arguments are dependent (according to g++'s
definition for mangling purposes).  g++ mangles functions with dependent
arguments using <simple-id> and others with <expr-primary>.
*/
{
  a_boolean     result = FALSE;

  for (; !result && arguments != NULL; arguments = arguments->next) {
    a_boolean         suppress_address_of = FALSE;
    an_expr_node_ptr  arg = skip_compiler_generated_expressions(arguments,
                                                         &suppress_address_of);
    check_assertion(arg != NULL);
    result = is_gnu_dependent_expression(arg);
  }  /* for */
  return result;
}  /* args_are_dependent */


static a_boolean gnu_requires_decltype_mangling(a_type_ptr type)
/*
Returns TRUE if the specified type (a decltype typeref) requires decltype
mangling according to the rules that GNU uses.  The IA-64 ABI states that
instantiation-dependent operands of decltype are required to be mangled as
expressions (otherwise the known type can be used).  Early versions of GNU
used a slightly different criterion for deciding when decltype expression
mangling was needed and that logic is reflected in this routine.
*/
{
  a_boolean         result;
  an_expr_node_ptr  expr = decltype_arg(type);

  check_assertion(is_typeref_kind(type, trk_is_decltype));
  if (expr == NULL) {
    result = FALSE;
  } else {
    if (type->variant.typeref.is_dependent_type_operator) {
      /* The front end believes this decltype is instantiation-dependent, i.e.,
         it or one of its subexpressions is dependent.  There are some cases
         where GNU believes such types do not need decltype mangling; each
         of these is handled below. */
      if (expr->kind == (an_expr_node_kind)enk_param_ref &&
          type->variant.typeref.decltype_expr_not_parenthesized) {
        /* GNU treats an unparenthesized parameter reference as not needing
           decltype mangling. */
        result = FALSE;
      } else if (expr->kind == (an_expr_node_kind)enk_temp_init) {
        /* Conversions don't need mangling, unless the type they're converting
           to is dependent.  For example, A() doesn't require mangling, but
           A<sizeof(p)>() does. */
        result = is_template_dependent_type(expr->type);
      } else if (is_operation_node(expr) &&
                 node_operator_is(expr, eok_call)) {
        /* Call operations are always mangled. */
        result = TRUE;
      } else {
        /* For most expressions, look at the expression to see if it meets
           GNU's requirements for decltype mangling. */
        result = is_gnu_dependent_expression(expr);
      }  /* if */
    } else {
      /* The expression is not dependent, nor does it contain any dependent
         subexpressions, nevertheless, in some cases, GNU uses decltype
         mangling anyway. */
      result = FALSE;
      if (is_operation_node(expr)) {
        if (node_operator_is(expr, eok_call) &&
            !expr->variant.operation.call_uses_operator_syntax) {
          /* Calls (other than explicit calls to operator routines) are
             mangled using decltype mangling. */
          result = TRUE;
        } else if ((node_operator_is(expr, eok_dot_field) ||
                    node_operator_is(expr, eok_dot_static) ||
                    node_operator_is(expr, eok_points_to_static) ||
                    node_operator_is(expr, eok_points_to_field)) &&
                   type->variant.typeref.decltype_expr_not_parenthesized) {
          /* a.m and a->m are mangled even when the types for a and m are known
             (except when parenthesized). */
          result = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* gnu_requires_decltype_mangling */

#endif /* IA64_ABI */

static void mangled_operator_or_special_function(
                         an_opname_kind           kind,
                         unsigned int             num_operands,
                         a_type_ptr               conversion_type,
                         a_const_char             *ud_suffix,
                         a_template_arg_ptr       template_arg_list,
                         a_name_reference_ptr     name_reference,
                         ARG_UNUSED a_boolean     suppress_operation_indicator,
                         ARG_UNUSED a_boolean     suppress_underscores,
                         a_mangling_control_block *mctl)
/*
This routine adds the proper mangling for a special function which can be the
operator specified by kind or for a conversion operation if conversion_type is
not NULL, or for a literal operator when ud_suffix is non-NULL.  num_operands
is the number of operands that the operator takes (and is used to differentiate
unary/binary versions of the operator).  If template_arg_list is non-NULL,
include a mangling for the specified template arguments.  name_reference (when
non-NULL) is used to ensure that the mangled list of template arguments
accurately represents those that appeared in the source form.  In the IA-64
ABI, the "on" prefix is suppressed when suppress_operation_indicator is TRUE.
In the Cfront ABI, the "__" prefix is suppressed when suppress_underscores is
TRUE.
*/
{
  check_assertion(conversion_type == NULL || ud_suffix == NULL);
#if IA64_ABI
  if (!suppress_operation_indicator) add_str_to_mangled_name("on", mctl);
#else /* !IA64_ABI */
  if (!suppress_underscores) add_str_to_mangled_name("__", mctl);
#endif /* IA64_ABI */
  if (conversion_type != NULL) {
    /* A conversion operation; include the type being converted to. */
    add_str_to_mangled_name(MANGLING_STRING_FOR_CONVERSION_FUNC, mctl);
    mangled_encoding_for_type(conversion_type, mctl);
  } else if (ud_suffix != NULL) {
    /* A user-defined literal operator. */
    add_str_to_mangled_name(MANGLING_STRING_FOR_LITERAL_OPERATORS, mctl);
    mangled_name_with_length(ud_suffix, mctl);
  } else {
    /* An operator. */
    add_str_to_mangled_name(mangled_operator_name(kind, num_operands), mctl);
  }  /* if */
  if (name_reference == NULL ? template_arg_list != NULL :
                               name_reference->is_template_id) {
    /* Put out the template argument list (or a null list), if any. */
    mangled_template_arguments(template_arg_list, /*partial_spec=*/FALSE,
                               /*old_form=*/FALSE, (a_name_reference_ptr)NULL,
                               mctl);
  }  /* if */
}  /* mangled_operator_or_special_function */


static void mangled_name_reference(a_name_reference_ptr        name_reference,
                                   ARG_UNUSED a_type_ptr       dtor_type,
                                   a_mangling_control_block    *mctl)
/*
Add the qualifiers as specified by name_reference to the mangled name.
This is used to implement the scope resolution portion of <unresolved-name>
mangling.  In the Cfront ABI, if dtor_type is non-NULL, create a mangling that
incorporates this type as the last component of a qualified name.  This is
used to create a "destructor name" qualifier for the __dn mangling, for cases
like T::~X where the type name is different in the qualifier and the type.
In the IA-64 ABI, the destructor type is simply appended as another level by
the caller.
*/
{
  a_boolean               need_close = FALSE;
  unsigned long           nesting_level = 1;
  a_scope_resolution_step step;

  if (name_reference != NULL &&
      (name_reference->qualifier != NULL ||
       name_reference->is_global_qualified_name)) {
    /* There is some qualification that needs to be represented. */
    step.uses_qualifiers = TRUE;
    step.variant.qualifier = name_reference->qualifier;
    step.is_global_qualified_name = name_reference->is_global_qualified_name;
#if !IA64_ABI
    if (dtor_type != NULL) {
      /* Mangle the destructor type as though it were part of the qualified
         name. */
      nesting_level++;
    }  /* if */
#endif /* !IA64_ABI */
    mangled_scope_resolution(&step, &need_close, nesting_level, mctl);
#if IA64_ABI
    if (need_close) {
      add_to_mangled_name('E', mctl);
    }  /* if */
#endif /* IA64_ABI */
  }  /* if */
#if !IA64_ABI
  if (dtor_type != NULL) {
    /* If the caller specified a dtor_type, include it in the qualified
       mangled type name. */
    mangled_encoding_for_type(dtor_type, mctl);
  }  /* if */
#endif /* !IA64_ABI */
}  /* mangled_name_reference */


static void mangled_destructor_name(a_type_ptr               type,
                                    a_name_reference_ptr     name_reference,
                                    a_mangling_control_block *mctl)
/*
Add an encoding for a destructor of the specified type.  The name_reference
details any qualification that applies to the destructor.
*/
{
  a_type_ptr  destructor_type;

  if (name_reference != NULL &&
      special_kind_is(name_reference, sfk_none) &&
      name_reference->variant.destructor_type != NULL) {
    /* Use the destructor_type from the name_reference if it's available. */
    destructor_type = name_reference->variant.destructor_type;
  } else {
    /* In some cases (i.e., eok_points_to_vacuous_destructor_call), the type
       passed in is a pointer to a class. */
    if (is_pointer_or_handle_type(type)) type = type_pointed_to(type);
    destructor_type = type;
  }  /* if */
#if IA64_ABI
  if (emulate_gnu_abi_bugs) {
    if (is_template_dependent_type(destructor_type)) {
      /* g++ encodes dependent destructors with "co" followed by the type,
         but apparently has a bug where that type doesn't participate in the
         substitution processing. */
      if (name_reference != NULL && name_reference->qualifier != NULL) {
        add_str_to_mangled_name("sr", mctl);
        mangled_encoding_for_type(destructor_type, mctl);
      }  /* if */
      /* When g++ uses the "co" mangling, it doesn't use or record
         substitutions. */
      add_str_to_mangled_name("co", mctl);
      mctl->suppress_substitutions++;
      mangled_encoding_for_type(destructor_type, mctl);
      mctl->suppress_substitutions--;
    } else {
      /* g++ encodes non-dependent destructors with <expr-primary>.  In cases
         where such a destructor is vacuous, there isn't a routine entry
         to mangle, so just provide the required mangling here. */
      add_str_to_mangled_name("L_ZN", mctl);
      mangled_encoding_for_type(destructor_type, mctl);
      add_str_to_mangled_name(MANGLING_STRING_FOR_DESTRUCTOR, mctl);
      add_str_to_mangled_name("EvE", mctl);
    }  /* if */
  } else {
    /* Precede the destructor indication with any qualification that is
       appropriate. */
    mangled_name_reference(name_reference, (a_type_ptr)NULL, mctl);
    add_str_to_mangled_name("dn", mctl);
    mangled_encoding_for_type(destructor_type, mctl);
  }  /* if */
#else /* !IA64_ABI */
  /* Pass the destructor type to incorporate it as part of the
     "destructor name". */
  add_str_to_mangled_name("__dn__", mctl);
  mangled_name_reference(name_reference, destructor_type, mctl);
  add_str_to_mangled_name("__", mctl);
#endif /* IA64_ABI */
}  /* mangled_destructor_name */


static void mangled_unresolved_name(
                                 an_expr_node_ptr            expr,
                                 an_expr_node_ptr            arguments,
                                 ARG_UNUSED an_expr_node_ptr selector,
                                 a_boolean                   in_dependent_expr,
                                 a_mangling_control_block    *mctl)
/*
Add to the mangled name an encoding for the entity represented by the
expression node.  This is not a general purpose routine for representing
an expression; rather it is used when mangling a call operand or the field of a
selection (e.g., "." or "->") operation.  Often, these entities are unknown
(e.g., because they are members of a dependent type) so mangle them with a
"spelling" of the entity name rather than their usual mangling.  arguments
refers to a (possibly NULL) set of arguments being passed to this entity, and
is used to differentiate unary/binary operators.  selector refers to the
expression that was used to select expr (NULL if no selector was used).
*/
{
  a_source_correspondence_ptr scp = NULL;
  a_template_arg_ptr          template_arg_list = NULL;
  a_boolean                   mangle_as_operator = FALSE;
  a_boolean                   suppress_operation_indicator = FALSE;
  a_boolean                   suppress_address_of = FALSE;
  a_boolean                   needs_qualification = FALSE;
  a_boolean                   suppress_qualification = FALSE;
  a_name_reference_ptr        name_reference;
  an_opname_kind              opname = (an_opname_kind)onk_none;
  a_const_char                *ud_suffix = NULL;
  a_type_ptr                  conversion_type = NULL, destructor_type = NULL;
#if IA64_ABI
  a_boolean                   dummy, selector_has_known_type = FALSE;
#else /* !IA64_ABI */
  a_length_reservation        length_reservation;
#endif /* IA64_ABI */

  /* Skip any expressions (e.g., compiler added) that don't belong in the
     mangled output. */
  expr = skip_compiler_generated_expressions(expr, &suppress_address_of);
  check_assertion(expr != NULL);
  name_reference = name_ref_for_node(expr);
#if IA64_ABI
  if (emulate_gnu_abi_bugs && selector != NULL) {
    /* g++ seems to add the "on" mangling to operator names only when there
       is no selector (i.e., for non-member operators). */
    suppress_operation_indicator = TRUE;
    selector = skip_compiler_generated_expressions(selector, &dummy);
    check_assertion(selector != NULL);
    if (!is_template_dependent_type(selector->type)) {
      /* g++ provides different manglings if the selector has a known type. */
      selector_has_known_type = TRUE;
    }  /* if */
  }  /* if */
#endif /* IA64_ABI */
  if (is_constant_node(expr)) {
    a_constant_ptr con = node_constant(expr);
    if (con->kind == (a_constant_repr_kind)ck_template_param) {
      if (con->variant.template_param.kind ==
                           (a_template_param_constant_kind)tpck_template_ref) {
        /* An unknown function template with a list of explicit template
           arguments.  The template is given by an underlying
           tpck_unknown_function constant. */
        template_arg_list = con->variant.template_param.variant.
                                                         template_ref.arg_list;
        con = con->variant.template_param.variant.template_ref.con;
      }  /* if */
      if (con->variant.template_param.kind ==
                       (a_template_param_constant_kind)tpck_unknown_function) {
        /* An unknown function, which may be a member of a class or namespace,
           and may be a conversion function or operator function. */
        a_symbol_ptr   sym =
                   con->variant.template_param.variant.unknown_function.symbol;
        opname =
              con->variant.template_param.variant.unknown_function.opname_kind;
#if IA64_ABI
        if (emulate_gnu_abi_bugs && selector == NULL) {
          /* GNU doesn't add any qualification to calls of non-member
             functions (e.g., N::f(p1) is simply mangled as though it were
             f(p1)). */
          name_reference = NULL;
        }  /* if */
#endif /* IA64_ABI */
#if CHECKING
        if (sym != NULL && sym->header != NULL &&
#if MICROSOFT_EXTENSIONS_ALLOWED
            !(cli_or_cx_enabled && sym->header->is_cli_operator) &&
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
            sym->header->variant.opname != (an_opname_kind)onk_none) {
          /* Make sure the symbol and constant agree. */
          check_assertion(opname == sym->header->variant.opname);
        }  /* if */
#endif /* CHECKING */
        if (opname != (an_opname_kind)onk_none ||
            con->variant.template_param.variant.unknown_function.
                                                     conversion_type != NULL) {
          /* An operator or a conversion function. */
          mangle_as_operator = TRUE;
          conversion_type = con->
               variant.template_param.variant.unknown_function.conversion_type;
        } else if (sym != NULL && sym->header != NULL &&
                   strncmp(sym->header->identifier,
                           CANONICAL_LITERAL_OPERATOR_INTRO,
                           LENGTH_CANONICAL_LITERAL_OPERATOR_INTRO) == 0) {
          /* A literal operator function. */
          mangle_as_operator = TRUE;
          ud_suffix =
                   ud_suffix_from_literal_operator_id(sym->header->identifier);
        } else {
          scp = &con->source_corresp;
        }  /* if */
      } else if (con->variant.template_param.kind ==
                             (a_template_param_constant_kind)tpck_destructor) {
        destructor_type = con->variant.template_param.variant.destructor.type;
      } else if (con->variant.template_param.kind ==
                                 (a_template_param_constant_kind)tpck_member) {
        if (con->source_corresp.name != NULL &&
            con->source_corresp.name[0] == '~') {
          /* A destructor. */
          check_assertion(name_reference != NULL &&
                          special_kind_is(name_reference, sfk_none) &&
                          name_reference->variant.destructor_type != NULL);
          destructor_type = name_reference->variant.destructor_type;
        } else {
          scp = &con->source_corresp;
        }  /* if */
      }  /* if */
    }  /* if */
  } else if (is_variable_node(expr)) {
    /* Static data members are mangled with <simple-id>; others are
       mangled using <expr-primary>. */
    if (node_variable(expr)->source_corresp.is_class_member) {
      scp = &node_variable(expr)->source_corresp;
    }  /* if */
  } else if (is_routine_node(expr)) {
#if IA64_ABI
    if (emulate_gnu_abi_bugs &&
        !(name_reference != NULL && name_reference->is_template_id) &&
        (selector_has_known_type ||
         (selector == NULL &&
          !args_are_dependent(arguments)))) {
      /* In cases where a (non-member) function is called with non-dependent
         arguments, or a member function is being called and the type of the
         selection is known, g++ uses <expr-primary> for mangling.  
         <expr-primary> mangling is not used for template-ids. */
      suppress_address_of = TRUE;
    } else
#endif /* IA64_ABI */
    /* Do not insert code here. */
    {
      a_routine_ptr rp = node_routine(expr);
      template_arg_list = rp->template_arg_list;
      if (rp->special_kind != (a_special_function_kind)sfk_none) {
        /* See if this routine requires special handling. */
        if (rp->special_kind == (a_special_function_kind)sfk_destructor) {
          destructor_type = scp_parent_class(&rp->source_corresp);
        } else if (rp->special_kind == (a_special_function_kind)sfk_operator) {
          mangle_as_operator = TRUE;
          opname = rp->variant.opname_kind;
        } else if (rp->special_kind ==
                                   (a_special_function_kind)sfk_udl_operator) {
          mangle_as_operator = TRUE;
          opname = (an_opname_kind)onk_none;
          ud_suffix = ud_suffix_for_routine(rp);
        } else if (rp->special_kind ==
                                     (a_special_function_kind)sfk_conversion) {
          /* Compiler-generated conversion operations have been stripped. */
#if IA64_ABI
          if (!emulate_gnu_abi_bugs) {
            /* Mangle as a conversion operation. */
            mangle_as_operator = TRUE;
            opname = (an_opname_kind)onk_none;
            conversion_type = rp->type->variant.routine.return_type;
          }  /* if */
          suppress_address_of = TRUE;
#endif /* IA64_ABI */
        } else {
          unexpected_condition();
        }  /* if */
      } else {
        /* Provide a spelling for the routine. */
        scp = &rp->source_corresp;
#if IA64_ABI
        if (emulate_gnu_abi_bugs &&
            (name_reference != NULL && name_reference->is_template_id)) {
          /* Suppress any qualification, but don't set name_reference to
             NULL as we need the information about the template arguments
             so the template arguments are mangled properly. */
          suppress_qualification = TRUE;
        }  /* if */
#endif /* IA64_ABI */
      }  /* if */
    }  /* if */
  } else if (is_field_node(expr)) {
    /* A field (possibly of an anonymous union). */
    scp = &node_field(expr)->source_corresp;
#if IA64_ABI
    if (emulate_gnu_abi_bugs && name_reference != NULL &&
        name_reference->qualifier != NULL &&
        skip_typerefs(name_reference->qualifier->qualifier.class_type) ==
                                                       scp_parent_class(scp)) {
      /* For items (that were qualified in the source) like p.B::A::m, GNU
         mangles the field as "sr1A1m"; emulate that here.  Note that such a
         mangled name cannot be demangled using the existing IA-64 ABI
         rules. */
      add_str_to_mangled_name("sr", mctl);
      mangled_encoding_for_type(scp_parent_class(scp), mctl);
      /* Mangled name for the field is emitted below, but we've already
         emitted the necessary qualification, so don't do that below. */
      name_reference = NULL;
    }  /* if */
#endif /* IA64_ABI */
    if (selector == NULL &&
        class_type_supp(scp_parent_class(scp))->anonymous_union_kind !=
                                           (an_anonymous_union_kind)auk_none) {
      /* This is a field of an anonymous union (whose compiler-generated
         member selection has been stripped off); mangle it as an entity
         through the normal mechanism. */
      name_reference = NULL;
      scp = NULL;
    } else if (unmangled_or_fabricated_name_of(scp) == NULL &&
               node_field(expr)->is_captured_this) {
      /* This unnamed field represents the captured "this" of a lambda.  Give
         the field a (non-standard) name so that it can be mangled. */
      scp->name = alloc_lowered_name("__captured_this");
    }  /* if */
  }  /* if */
  /* The code above has determined how to mangle the entity (i.e., whether
     it should be mangled as an operand, destructor, source name, or
     otherwise).  Now do the actual mangling.  In the IA-64 ABI, any
     qualification occurs before the actual entity, but qualification follows
     the mangled name in the Cfront ABI. */
  if (!suppress_qualification &&
      name_reference != NULL &&
      (name_reference->qualifier != NULL ||
       name_reference->is_global_qualified_name) &&
      (mangle_as_operator || scp != NULL)) {
    /* This entity needs name qualification. */
    needs_qualification = TRUE;
  }  /* if */
  if (needs_qualification) {
#if IA64_ABI
    /* Emit any qualification that is necessary for this entity. */
    mangled_name_reference(name_reference, (a_type_ptr)NULL, mctl);
#else /* !IA64_ABI */
    reserve_space_for_length(&length_reservation, mctl);
#endif /* IA64_ABI */
  }  /* if */
  if (destructor_type != NULL) {
    /* Mangle the entity as a destructor. */
    mangled_destructor_name(destructor_type, name_reference, mctl);
  } else if (mangle_as_operator) {
    /* Mangle the entity as an operator. */
    unsigned long num_operands= number_of_operands_in_operator_list(opname,
                                                                    arguments);
    mangled_operator_or_special_function(opname,
                                         (unsigned int)num_operands,
                                         conversion_type,
                                         ud_suffix,
                                         template_arg_list,
                                         name_reference,
                                         suppress_operation_indicator,
                                         /*suppress_underscores=*/FALSE,
                                         mctl);
#if !IA64_ABI
    if (!needs_qualification) {
      /* If the operator will be qualified, a pair of underscores will be
         added prior to the qualification, otherwise emit them here. */
      add_str_to_mangled_name("__", mctl);
    }  /* if */
#endif /* !IA64_ABI */
  } else if (scp != NULL) {
    /* Encode this entity with a "spelling" (i.e., <simple-id> for IA-64
       ABI). */
    mangled_simple_id(scp, template_arg_list, name_reference,
                      /*include_length=*/!needs_qualification, mctl);
  } else {
    /* This isn't a special case, provide usual mangling for the expression. */
    mangled_encoding_for_expression_full(expr, in_dependent_expr, 
                                         suppress_address_of, mctl);
  }  /* if */
#if !IA64_ABI
  if (needs_qualification) {
    /* Emit any qualification that is necessary for this entity. */
    add_str_to_mangled_name("__", mctl);
    mangled_name_reference(name_reference, (a_type_ptr)NULL, mctl);
    fill_in_length(&length_reservation, mctl);
  }  /* if */
#endif /* !IA64_ABI */
}  /* mangled_unresolved_name */


static void mangled_selection_operation(
                                    an_expr_node_ptr         expr,
                                    an_expr_node_ptr         arguments,
                                    a_boolean                in_dependent_expr,
                                    a_mangling_control_block *mctl)
/*
Utility to mangle (part of) a selection operation expression as specified
by expr.  In some cases, the selection operation also involves a call
operation; in those cases, the caller is responsible for emitting the mangling
for the call operation.  arguments specifies a list of arguments to the
call (and should be NULL if there is no call).  In the IA-64 ABI,
in_dependent_expr is TRUE if this expression is part of a template-dependent
expression. 
*/
{
  an_expr_node_ptr      selector = NULL, selection = NULL, operand;
  a_boolean             use_unresolved_name_mangling = TRUE;
  a_name_reference_ptr  nrp = NULL;
#if !IA64_ABI && ABI_COMPATIBILITY_VERSION < 404
  a_constant_ptr        dummy_constant = local_constant();
  an_expr_node          dummy_expr;
#endif /* !IA64_ABI && ABI_COMPATIBILITY_VERSION < 404 */

  check_assertion(is_operation_node(expr));
  operand = expr->variant.operation.operands;
  /* Determine where the operands are in the expression. */
  switch (expr->variant.operation.kind) {
    case eok_dot_field:
    case eok_dot_static:
    case eok_points_to_field:
    case eok_points_to_static:
      selector = operand;
      selection = operand->next;
      nrp = name_ref_for_node(selection);
      break;
    case eok_dot_member_call:
    case eok_points_to_member_call:
    case eok_dot_pm_call:
    case eok_points_to_pm_call:
      selector = operand->next;
      selection = operand;
      nrp = name_ref_for_node(selection);
      break;
    case eok_dot_vacuous_destructor_call:
    case eok_points_to_vacuous_destructor_call:
      selector = operand;
      selection = NULL;
      if (operand->next != NULL) {
        /* A NULL enk_routine entry was appended to record the form used to
           denote the destructor. */
        nrp = name_ref_for_node(operand->next);
      }  /* if */
      break;
    default:
      unexpected_condition();
  }  /* switch */
  if (expr->compiler_generated) {
    if (expr->is_objectless_nonstatic_data_mem_ref
#if !IA64_ABI
        && (nrp != NULL && nrp->qualifier != NULL)
#endif /* !IA64_ABI */
                                                  ) {
      /* A case like decltype(p.x+A::x); remove the "((A *)0)->" portion of
         the expression used in the internal representation. */
      selector = NULL;
    } else {
      if (operand->kind == enk_param_ref &&
          operand->variant.param_ref.param_num == 0) {
        /* A compiler-generated implicit use of "this". */
#if IA64_ABI
        /* Always remove an implicit "this" in the IA-64 ABI (where we try
           to produce a mangling that mimics the source as written). */
        selector = NULL;
#else /* !IA64_ABI */
        if (nrp != NULL && nrp->qualifier != NULL) {
          /* The selection has a qualifier (e.g., A::m), so use the qualifier
             in the mangling (rather than the implicit "this"). */
          selector = NULL;
        } else {
          /* No qualifier in the selection. */
#if ABI_COMPATIBILITY_VERSION >= 404
          /* Produce a mangling that contains "this" by leaving the expression
             as is.  This can lead to ambiguous mangled names (i.e., explicit
             and implicit uses of "this" are mangled the same), but the front
             end won't allow those to be overloaded, so it should be okay. */
#else /* ABI_COMPATIBILITY_VERSION < 404 */
          /* Originally, an enk_param_ref wasn't used in the internal
             representation for implicit "this", so recreate the mangling for
             the old internal representation (i.e., "((A *)0)->"). */
          make_zero_of_proper_type(operand->type, dummy_constant);
          clear_expr_node(&dummy_expr, (an_expr_node_kind)enk_constant);
          dummy_expr.variant.constant = dummy_constant;
          dummy_expr.type = dummy_constant->type;
          selector = &dummy_expr;
#endif /* ABI_COMPATIBILITY_VERSION >= 404 */
        }  /* if */
#endif /* IA64_ABI */
      }  /* if */
    }  /* if */
  }  /* if */
  if (selector != NULL) {
#if !IA64_ABI
    add_to_mangled_name('O', mctl);
#endif /* !IA64_ABI */
    /* Determine the appropriate mangling for the expression.  Note that these
       mangled operator names are the same in both the IA-64 and Cfront
       ABIs. */
    switch (expr->variant.operation.kind) {
      case eok_dot_field:
      case eok_dot_static:
      case eok_dot_member_call:
      case eok_dot_vacuous_destructor_call:
        add_str_to_mangled_name(MANGLING_STRING_FOR_OPERATOR_DOT, mctl);
        break;
      case eok_points_to_field:
      case eok_points_to_static:
      case eok_points_to_member_call:
      case eok_points_to_vacuous_destructor_call:
        add_str_to_mangled_name(MANGLING_STRING_FOR_OPERATOR_ARROW, mctl);
        break;
      case eok_dot_pm_call:
        add_str_to_mangled_name(MANGLING_STRING_FOR_OPERATOR_DOT_STAR, mctl);
        use_unresolved_name_mangling = FALSE;
        break;
      case eok_points_to_pm_call:
        add_str_to_mangled_name(MANGLING_STRING_FOR_OPERATOR_ARROW_STAR, mctl);
        use_unresolved_name_mangling = FALSE;
        break;
      default:
        unexpected_condition();
    }  /* switch */
#if !IA64_ABI
    /* Count of operands. */
    store_digits_and_underscore((unsigned long)2, /*old_form=*/FALSE, mctl);
#endif /* !IA64_ABI */
    mangled_encoding_for_expression(selector, in_dependent_expr, mctl);
  }  /* if */
  if (selection != NULL) {
    if (use_unresolved_name_mangling) {
      mangled_unresolved_name(selection, arguments, selector,
                              in_dependent_expr, mctl);
    } else {
      /* Pointer-to-member calls don't use an id-expression and aren't 
         subject to <unresolved-name> mangling; just mangle the "selection"
         as an expression. */
      mangled_encoding_for_expression(selection, in_dependent_expr, mctl);
    }  /* if */
  } else {
    /* A vacuous destructor.  Vacuous destructors of the type int::~int()
       don't get a name_reference structure, so they appear in demangled
       names as ~int(). */
    check_assertion(selector != NULL);
    mangled_destructor_name(selector->type, nrp, mctl);
  }  /* if */
#if !IA64_ABI
  if (selector != NULL) add_to_mangled_name('O', mctl);
#if ABI_COMPATIBILITY_VERSION < 404
  release_local_constant(&dummy_constant);
#endif /* ABI_COMPATIBILITY_VERSION < 404 */
#endif /* !IA64_ABI */
}  /* mangled_selection_operation */


static void mangled_call_operation(an_expr_node_ptr         expr,
                                   a_boolean                in_dependent_expr,
                                   a_mangling_control_block *mctl)
/*
Utility to emit a mangling for the given expr, which is an expression involving
some type of call operation.  In the IA-64 ABI, in_dependent_expr is TRUE if
this expression is part of a template-dependent expression.
*/
{
  an_expr_node_ptr  call_operand = NULL, arguments = NULL, child;
  an_expr_node_ptr  member = NULL;
  a_routine_ptr     rp;

  check_assertion(is_operation_node(expr));
  child = expr->variant.operation.operands;
  switch (expr->variant.operation.kind) {
    case eok_call:
      call_operand = child;
      arguments = call_operand->next;
      break;
    case eok_dot_member_call:
    case eok_points_to_member_call:
      call_operand = child;
      member = call_operand->next;
      arguments = member->next;
      break;
    case eok_dot_pm_call:
    case eok_points_to_pm_call:
      call_operand = child;
      arguments = call_operand->next->next;
      break;
    case eok_dot_vacuous_destructor_call:
    case eok_points_to_vacuous_destructor_call:
      call_operand = child;
      arguments = NULL;
      break;
    default:
      unexpected_condition();
  }  /* switch */
  rp = routine_from_function_expr(call_operand);
  if (expr->variant.operation.call_uses_operator_syntax &&
      !(rp->special_kind == (a_special_function_kind)sfk_udl_operator)) {
    /* This is a call operator that was added by the compiler, for example,
       for a+a, and for mangling purposes needs to be represented as it
       appeared in the source code (i.e., a+a, not operator+(a,a)).  Note that
       not all operators can have the call_uses_operator_syntax field set to
       TRUE (for example, those operators represented by enk_new_delete
       won't get here). */
    unsigned long    num_arguments = number_of_operands_in_list(arguments);
    a_boolean        remove_last_arg = FALSE;
#if IA64_ABI
    a_boolean        is_prefix = FALSE;
#else /* !IA64_ABI */
    a_const_char     *name = NULL;
#endif /* IA64_ABI */
#if MICROSOFT_EXTENSIONS_ALLOWED
    check_assertion(!is_delegate_invocation_function(rp));
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (member != NULL) num_arguments++;
    if (rp->variant.opname_kind == (an_opname_kind)onk_plus_plus ||
        rp->variant.opname_kind == (an_opname_kind)onk_minus_minus) {
      if (num_arguments == 1) {
        /* This is a prefix increment/decrement.  The Cfront ABI has
           separate operators for prefix increment/decrements, but the IA-64
           ABI uses a special flag. */
#if IA64_ABI
        is_prefix = TRUE;
#else /* !IA64_ABI */
        /* There is a separate mangled name for Cfront prefix increment/
           decrements. */
        if (rp->variant.opname_kind == (an_opname_kind)onk_plus_plus) {
          name = MANGLING_STRING_FOR_OPERATOR_PLUS_PLUS_PREFIX;
        } else {
          check_assertion(rp->variant.opname_kind ==
                                              (an_opname_kind)onk_minus_minus);
          name = MANGLING_STRING_FOR_OPERATOR_MINUS_MINUS_PREFIX;
        }  /* if */
#endif /* IA64_ABI */
      } else {
        /* This is a postfix increment/decrement.  The front end has added
           an extra argument (a constant zero) which should be removed for
           mangling purposes. */
        check_assertion(num_arguments == 2);
        num_arguments--;
        remove_last_arg = TRUE;
      }  /* if */
    }  /* if */
#if !IA64_ABI
    add_to_mangled_name('O', mctl);
    if (name != NULL) {
      add_str_to_mangled_name(name, mctl);
    } else
#endif /* !IA64_ABI */
    /* Do not insert code here. */
    {
      mangled_operator_or_special_function(rp->variant.opname_kind,
                                         (unsigned int)num_arguments,
                                         (a_type_ptr)NULL,
                                         ud_suffix_for_routine(rp),
                                         (a_template_arg_ptr)NULL,
                                         (a_name_reference_ptr)NULL,
                                         /*suppress_operation_indicator=*/TRUE,
                                         /*suppress_underscores=*/TRUE,
                                         mctl);
    }  /* if */
#if IA64_ABI
    if (is_prefix && !emulate_gnu_abi_bugs) add_to_mangled_name('_', mctl);
#else /* !IA64_ABI */
    store_digits_and_underscore(num_arguments, /*old_form=*/FALSE, mctl);
#endif /* IA64_ABI */
    if (member != NULL) {
      mangled_encoding_for_expression(member, in_dependent_expr, mctl);
    }  /* if */
    for (; arguments != NULL; arguments = arguments->next) {
      if (remove_last_arg && arguments->next == NULL) break;
      mangled_encoding_for_expression(arguments, in_dependent_expr, mctl);
    }  /* for */
#if IA64_ABI
    if (rp->variant.opname_kind == (an_opname_kind)onk_function_call) {
      /* Need to close a "cl" mangling. */
      add_to_mangled_name('E', mctl);
    }  /* if */
#else /* !IA64_ABI */
    add_to_mangled_name('O', mctl);
#endif /* IA64_ABI */
  } else {
    /* Mangle as a call of some type. */
#if IA64_ABI
    if (expr->variant.operation.arg_dependent_lookup_suppressed_on_call) {
      add_str_to_mangled_name("cp", mctl);
    } else {
      add_str_to_mangled_name("cl", mctl);
    }  /* if */
#else /* !IA64_ABI */
    /* Call.  Output has the form
         Ocl_1_1fI1IO <-- encoding for "f(p1)"
                    ^---- "O" to end the operation encoding.
                 ^^^----- First argument.
               ^^-------- Call operand.
            ^^^---------- Count of arguments to call (with underscores).
          ^^------------- Call operation ("cl" or "cp" -- if ADL suppressed).
         ^--------------- "O" for operation.
    */
    add_to_mangled_name('O', mctl);
    if (expr->variant.operation.arg_dependent_lookup_suppressed_on_call) {
      add_str_to_mangled_name("cp", mctl);
    } else {
      add_str_to_mangled_name("cl", mctl);
    }  /* if */
    store_digits_and_underscore(1 + number_of_operands_in_list(arguments),
                                /*old_form=*/FALSE, mctl);
#endif /* IA64_ABI */
    if (node_operator_is(expr, eok_call)) {
      /* Source form doesn't have a selection operation. */
#if IA64_ABI
      if (expr->variant.operation.call_uses_operator_syntax &&
          rp->special_kind == (a_special_function_kind)sfk_udl_operator) {
        /* This is a compiler-generated call for a user-defined literal.
           Most compiler-generated calls are suppressed during mangling, but
           these are not; they're mangled as "L <mangled-name> E" with the
           mangled name of the literal operator. */
        add_str_to_mangled_name("L_Z", mctl);
        mangled_function_name(rp,
                              /*suppress_param_encoding=*/FALSE,
                              /*suppress_parent_encoding=*/TRUE,
                              /*force_primary_name=*/TRUE,
                              /*force_individuation=*/FALSE,
                              /*base_name_offset=*/(sizeof_t *)NULL,
                              mctl);
        add_to_mangled_name('E', mctl);
      } else
#endif /* IA64_ABI */
      /* Do not insert code here. */
      {
        mangled_unresolved_name(call_operand, arguments, /*selector=*/NULL,
                                in_dependent_expr, mctl);
      }  /* if */
    } else {
      /* Mangle the selection operation. */
      mangled_selection_operation(expr, arguments, in_dependent_expr, mctl);
    }  /* if */
    mangled_expression_list(arguments, in_dependent_expr, mctl);
#if IA64_ABI
    /* Close the "cl" mangling. */
    add_to_mangled_name('E', mctl);
#else /* !IA64_ABI */
    add_to_mangled_name('O', mctl);
#endif /* IA64_ABI */
  }  /* if */
}  /* mangled_call_operation */

#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
#if !IA64_ABI

static a_boolean dip_has_args_that_need_mangling(a_dynamic_init_ptr  dip)
/*
Returns TRUE if the dik_constructor dynamic initialization has at least
one non-default argument (FALSE otherwise).
*/
{
  an_expr_node_ptr args;
  a_boolean        result = FALSE;

  check_assertion (dip != NULL &&
                   dip->kind == (a_dynamic_init_kind)dik_constructor);
  for (args = arg_list_from_dyn_init(dip); args != NULL; args = args->next) {
    if (!args->generated_default_arg) {
      result = TRUE;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* dip_has_args_that_need_mangling */

#endif /* !IA64_ABI */

static void add_mangling_for_array_element(unsigned long            number,
                                           a_mangling_control_block *mctl)
/*
Emit the proper mangling for an unsigned constant with "integer" type.  Note
that in the IA-64 no substitution is registered for this case.
*/
{
#if IA64_ABI
  add_to_mangled_name('L', mctl);
  add_str_to_mangled_name(MANGLING_STRING_FOR_INT, mctl);
  add_number_to_mangled_name(number, mctl);
  add_to_mangled_name('E', mctl);
#else /* !IA64_ABI */
  char     buffer[50];
  sizeof_t len = unsigned_to_string_buf((a_host_large_unsigned)number, buffer);
  add_to_mangled_name('C', mctl);
  add_str_to_mangled_name(MANGLING_STRING_FOR_INT, mctl);
  add_to_mangled_name('L', mctl);
  store_digits_and_underscore((unsigned long)len, /*old_form=*/FALSE, mctl);
  add_str_to_mangled_name(buffer, mctl);
#endif /* IA64_ABI */
}  /* add_mangling_for_array_element */


static a_constant_ptr mangled_braced_expression(a_constant_ptr           con,
                                                a_mangling_control_block *mctl)
/*
Designated initializers can appear in the list of constants pointed to by con
and those are mangled as <braced-expression>s in the IA-64 (and as an
extension, similarly in the Cfront ABI):

  <braced-expression>
    ::= <expression>
    ::= di <field source-name> <braced-expression>    # .name = expr
    ::= dx <index expression> <braced-expression>     # [expr] = expr
    ::= dX <range begin expression> <range end expression> <braced-expression>
                                                      # [expr ... expr] = expr

This routine returns the next constant to be mangled (the "next" pointer cannot
reliably be used by the caller since ck_designator entries just modify
constants that follow, i.e., multiple a_constant_ptr entities may be mangled
together here).
*/
{
  a_constant_ptr result = con->next;
  a_const_char   *name;
  a_constant_ptr repeated_con = NULL;

  switch (con->kind) {
    case ck_designator:
      if (con->variant.designator.is_field_designator) {
        /* A field designator (e.g., ".x ="). */
        add_str_to_mangled_name("di", mctl);
        if (con->variant.designator.is_generic) {
          name = con->variant.designator.variant.field_name;
        } else {
          name = con->variant.designator.variant.field->source_corresp.name;
        }  /* if */
        mangled_name_with_length(name, mctl);
      } else {
        /* An array element (or elements) designator. */
        if (con->next != NULL && constant_is(con->next, ck_init_repeat)) {
          /* A range of array elements, e.g., "[5 ... 10]". */
          add_str_to_mangled_name("dX", mctl);
          repeated_con = con->next;
        } else {
          add_str_to_mangled_name("dx", mctl);
        }  /* if */
        if (con->variant.designator.is_generic) {
          check_assertion(repeated_con == NULL);
          mangled_encoding_for_constant(
                                     con->variant.designator.variant.subscript,
                                     /*old_form=*/FALSE,
                                     /*in_dependent_expr=*/TRUE,
                                     /*suppress_address_of=*/FALSE,
                                     mctl);
        } else {
          /* Determine the beginning element. */
          unsigned long element =
                  (unsigned long)con->variant.designator.variant.array_element;
          add_mangling_for_array_element(element, mctl);
          if (repeated_con != NULL) {
            /* Emit the range for the "dX" mangling. */
            check_assertion(repeated_con->variant.init_repeat.constant->kind ==
                                             (a_constant_repr_kind)ck_integer);
            a_boolean ovflo = FALSE;
            unsigned long count =
                (unsigned long)unsigned_value_of_integer_constant(repeated_con,
                                                                  &ovflo);
            add_mangling_for_array_element(element + count, mctl);
            check_assertion(!ovflo);
          }  /* if */
        }  /* if */
      }  /* if */
      if (repeated_con != NULL) {
        /* Produce a mangled representation of the repeated constant, but
           follow the "next" link for subsequent constants in the aggregate. */
        (void)mangled_braced_expression(
                                    repeated_con->variant.init_repeat.constant,
                                    mctl);
        result = repeated_con->next;
      } else if (con->next != NULL) {
        result = mangled_braced_expression(con->next, mctl);
      }  /* if */
      break;
    default:
      /* Perform normal mangling on the constant. */
      mangled_encoding_for_constant(con,
                                    /*old_form=*/FALSE,
                                    /*in_dependent_expr=*/TRUE,
                                    /*suppress_address_of=*/FALSE,
                                    mctl);
      break;
  }  /* switch */
  return result;
}  /* mangled_braced_expression */

#if !IA64_ABI

static inline a_boolean constant_doesnt_generate_mangling(
                                                       const a_constant_ptr cp)
/*
Return TRUE if cp is a constant for which no mangling will be generated in the
Cfront ABI, i.e., a case like "new A[1]{}", for which there is a constructor
call (with no arguments -- or at least no non-default arguments).
*/
{
   return (cp->kind == ck_dynamic_init &&
           cp->variant.dynamic_init.ptr != NULL &&
           cp->variant.dynamic_init.ptr->kind == dik_constructor &&
           !dip_has_args_that_need_mangling(cp->variant.dynamic_init.ptr));
}  /* constant_doesnt_generate_mangling */

#endif /* !IA64_ABI */

static void mangled_list(an_expr_node_ptr         expr_list,
                         a_constant_ptr           con,
                         a_mangling_control_block *mctl)
/*
Provide mangling for a list of expressions or a constant (which may be
an aggregate -- in which case each element of the aggregate is mangled --
to emulate GNU's mangling of compound literals).  When con is non-NULL, a
mangling for the constant, is provided; otherwise, the list of expressions
(which may be NULL) is mangled.
*/
{
  a_constant_ptr  cp;
#if !IA64_ABI
  unsigned long   count;
#endif /* !IA64_ABI */

  check_assertion(expr_list == NULL || con == NULL);
#if !IA64_ABI
  /* Compute the count of expressions/constants that will be mangled. */
  if (con == NULL) {
    count = number_of_operands_in_list(expr_list);
  } else {
    if (con->kind == ck_aggregate) {
      count = 0;
      for (cp = con->variant.aggregate.first_constant;
           cp != NULL;
           cp = cp->next) {
        if (cp->implicit_aggr_element || cp->kind == ck_designator) {
          /* Designators and implicit aggregate element initializers are
             ignored for the purposes of counting the number of elements in the
             list. */
        } else if (constant_doesnt_generate_mangling(cp)) {
          /* A case like "new A[1]{}", for which there is a constructor call
             (with no arguments -- or at least no non-default arguments).  When
             mangled later, this constant produces no mangled output, so
             don't include it in the constant count for this aggregate. */
        } else if (cp->kind == ck_init_repeat) {
          if (constant_doesnt_generate_mangling(
                                           cp->variant.init_repeat.constant)) {
            /* No mangling to repeat. */
          } else {
            count += (unsigned long)cp->variant.init_repeat.count;
          }  /* if */
        } else {
          count++;
        }  /* if */
      }  /* for */
    } else {
      count = 1;
    }  /* if */
  }  /* if */
  store_digits_and_underscore(count, /*old_form=*/FALSE, mctl);
#endif /* !IA64_ABI */
  if (con == NULL) {
    /* Mangle a list of expressions (that may be NULL). */
    mangled_expression_list(expr_list, /*in_dependent_expr=*/TRUE, mctl);
  } else {
    if (con->kind == (a_constant_repr_kind)ck_aggregate) {
      /* Mangle a list of constants in the aggregate. */
      for (cp = con->variant.aggregate.first_constant; cp != NULL; ) {
        if (cp->implicit_aggr_element) {
          /* Skip implicit elements. */
          cp = cp->next;
        } else {
          cp = mangled_braced_expression(cp, mctl);
        }  /* if */
      }  /* for */
    } else {
      /* Just one constant. */
      mangled_encoding_for_constant(con,
                                    /*old_form=*/FALSE,
                                    /*in_dependent_expr=*/TRUE,
                                    /*suppress_address_of=*/FALSE,
                                    mctl);
    }  /* if */
  }  /* if */
}  /* mangled_list */


static void mangled_braced_init_list(an_expr_node_ptr         expr_list,
                                     a_constant_ptr           con,
                                     a_type_ptr               type,
                                     a_mangling_control_block *mctl)
/*
Provide mangling for a brace-enclosed initializer list for the given list of
expressions or constant (which may be an aggregate).  When con is non-NULL,
the constant is emitted as a mangled initializer list (to emulate GNU's
mangling of compound literals), otherwise, the list of expressions (which may
be NULL) is mangled as an initializer list.  An optional type is mangled (when
non-NULL) when the brace-enclosed list is a cast variant.  Note that this is
also used in the IA-64 ABI (but not the Cfront ABI) to mangle a brace-enclosed
<initializer> production (it can't be used in the Cfront ABI because the 'O'
characters that open this "operation" are seen as closing the new/gcnew
operation by the demangler).  Also used (though not yet standardized) to
mangle nontype template parameters of class type.
*/
{
  check_assertion(expr_list == NULL || con == NULL);
#if !IA64_ABI
  /* Brace-enclosed initializer list (EDG-specific):
       OtlZ1Z_1_I1IO <-- encoding for "T1{param#1}"
                   ^---- "O" to end the operation encoding.
                ^^^----- Expression(s)/constant(s) in the list.
             ^^^-------- Expression or constant count.
          ^^^----------- Type of the list (only with "tl" encoding).
        ^^-------------- Brace-enclosed list ("il" or "tl").
       ^---------------- "O" for operation.
  */
  add_to_mangled_name('O', mctl);
#endif /* !IA64_ABI */
  if (type == NULL ||
      (type->kind == tk_template_param &&
       type->variant.template_param.kind == tptk_unknown)) {
    /* Mangle as an initializer list. */
    add_str_to_mangled_name("il", mctl);
  } else {
    /* Mangle as an initializer list with a conversion. */
    add_str_to_mangled_name("tl", mctl);
    mangled_encoding_for_type(type, mctl);
  }  /* if */
  /* Provide a mangling for a list of expressions or constants. */
  mangled_list(expr_list, con, mctl);
#if IA64_ABI
  add_to_mangled_name('E', mctl);
#else /* !IA64_ABI */
  add_to_mangled_name('O', mctl);
#endif /* IA64_ABI */
}  /* mangled_braced_init_list */


static void get_expr_or_constant_list_from_dip(a_dynamic_init_ptr dip,
                                               an_expr_node_ptr   *expr_list,
                                               a_constant_ptr     *con_list)
/*
Utility to examine a dynamic initializer and return either a list of
expressions or a constant (which may be an aggregate) that need to appear in a
mangled list of some sort.  At least one of *expr_list or *con_list will be
NULL (and in some cases both will be NULL).
*/
{
  *expr_list = NULL;
  *con_list = NULL;
  switch (dip->kind) {
    case dik_constant:
    case dik_nonconstant_aggregate:
      /* Mangle as a constant or aggregate constant. */
      *con_list = dip->variant.constant.ptr;
      break;
    case dik_expression:
    case dik_constructor:
      /* Mangle a list of expressions. */
      *expr_list = arg_list_from_dyn_init(dip);
      break;
    case dik_zero:
      /* Neither a constant nor an expression. */
      break;
    default:
      unexpected_condition();
  }  /* switch */
}  /* get_expr_or_constant_list_from_dip */


static void mangled_dynamic_init(a_dynamic_init_ptr       dip,
                                 a_type_ptr               type,
                                 a_boolean                is_static_cast,
                                 a_mangling_control_block *mctl)

/*
Provide a mangled encoding for a dynamic initialization.  The mangling used to
represent the dynamic init varies depending upon the type of dynamic
initialization being performed.  When dip->is_explicit_cast is set, provide a
mangling for a cast (static cast if is_static_cast is TRUE).  When
dip->is_braced_initializer is TRUE, an initializer list mangling is used (also
used for compound literals).  The caller should set is_static_cast to TRUE if
the dynamic initialization is the result of a static_cast.
*/
{
  an_expr_node_ptr    expr_list;
  a_constant_ptr      con_list;
  an_expr_node_ptr    args;
  unsigned long       num_operands;
  a_const_char        *str;

  check_assertion(dip != NULL);
  dip = skip_compiler_generated_initialization(dip);
  if (dip->is_braced_initializer) {
    /* Mangle as an initializer-list (even if is_explicit_cast is also set). */
    get_expr_or_constant_list_from_dip(dip, &expr_list, &con_list);
    mangled_braced_init_list(expr_list, con_list, type, mctl);
  } else if (dip->is_explicit_cast) {
    args = arg_list_from_dyn_init(dip);
    num_operands = number_of_operands_in_list(args);
    /* Determine whether to mangle this as a static cast or a conversion. */
    if (is_static_cast
#if IA64_ABI
        && !emulate_gnu_abi_bugs
#endif /* !IA64_ABI */
                                ) {
      check_assertion(num_operands == 1);
      str = MANGLING_STRING_FOR_STATIC_CAST;
    } else {
      str = MANGLING_STRING_FOR_CAST;
    }  /* if */
#if IA64_ABI
    add_str_to_mangled_name(str, mctl);
    mangled_encoding_for_type(type, mctl);
    if (num_operands != 1) {
      /* Zero or more than one argument (args with generated_default_arg
         are ignored). */
      add_to_mangled_name('_', mctl);
      mangled_expression_list(args, /*in_dependent_expr=*/TRUE, mctl);
      add_to_mangled_name('E', mctl);
    } else {
      /* Exactly one argument. */
      mangled_expression_list(args, /*in_dependent_expr=*/TRUE, mctl);
    }  /* if */
#else /* !IA64_ABI */
    /* Conversion.  Output has the form
         Ocs1A_1_I1IO <-- encoding for "A(p1)"
                    ^---- "O" to end the operation encoding.
                 ^^^----- Argument(s) to conversion.
              ^^^-------- Argument count.
            ^^----------- Type to convert to.
          ^^------------- Conversion operation ("cs" or "sc").
         ^--------------- "O" for operation.
    */
    add_to_mangled_name('O', mctl);
    add_str_to_mangled_name(str, mctl);
    mangled_encoding_for_type(type, mctl);
    store_digits_and_underscore(num_operands, /*old_form=*/FALSE, mctl);
    mangled_expression_list(args, /*in_dependent_expr=*/TRUE, mctl);
    add_to_mangled_name('O', mctl);
#endif /* !IA64_ABI */
  } else {
    /* Likely a ck_dynamic_init from an aggregate in a compound literal,
       or an argument to throw; in both cases (in the Cfront ABI) the
       operand count (if any) has already been emitted. */
    switch (dip->kind) {
      case dik_expression:
        mangled_encoding_for_expression(arg_list_from_dyn_init(dip),
                                        /*in_dependent_expr=*/TRUE, mctl);
        break;
      case dik_constructor:
        args = arg_list_from_dyn_init(dip);
        if (args != NULL) {
          mangled_expression_list(args, /*in_dependent_expr=*/TRUE, mctl);
        }  /* if */
        break;
      case dik_zero:
        break;
      case dik_constant:
        mangled_encoding_for_constant(dip->variant.constant.ptr,
                                      /*old_form=*/FALSE,
                                      /*in_dependent_expr=*/TRUE,
                                      /*suppress_address_of=*/FALSE,
                                      mctl);
        break;
      default:
        unexpected_condition();
    }  /* switch */
  }  /* if */
}  /* mangled_dynamic_init */


static void mangled_encoding_for_initializer(
                                  a_dynamic_init_ptr       dip,
                                  a_mangling_control_block *mctl)
/*
Provide a mangled encoding for the initializer in a new/gcnew expression.
dip specifies the initialization that is being performed.  For the IA-64 ABI,
if there is no <initializer>, an 'E' is emitted.

  <initializer> ::= pi <expression>* E    # parenthesized initialization
  <initializer> ::= il <expression>* E    # braced-init list

Note that the Cfront and IA-64 ABIs diverge somewhat here (because the "O"
characters that open this "operation" don't lend themselves to nesting, so
in the Cfront ABI a "bi" flag is used instead).
*/
{
  an_expr_node_ptr  expr_list;
  a_constant_ptr    con_list;

  if (dip != NULL) {
    /* We need to include an initializer expression list. */
    dip = skip_compiler_generated_initialization(dip);
#if IA64_ABI
    if (dip->is_braced_initializer) {
      /* Use braced-enclosed initializer list mangling. */
      get_expr_or_constant_list_from_dip(dip, &expr_list, &con_list);
      mangled_braced_init_list(expr_list, con_list, (a_type_ptr)NULL, mctl);
    } else
#endif /* IA64_ABI */
    /* Do not insert code here. */
    {
#if IA64_ABI
      /* In the IA-64 ABI, must be parenthesized initialization. */
      add_str_to_mangled_name("pi", mctl);
#else /* !IA64_ABI */
      if (dip->is_braced_initializer) {
        /* Indicate brace-enclosed list (otherwise parenthesized list). */
        add_str_to_mangled_name("bi", mctl);
      }  /* if */
#endif /* IA64_ABI */
      /* Mangle the list of expressions/constants. */
      get_expr_or_constant_list_from_dip(dip, &expr_list, &con_list);
      mangled_list(expr_list, con_list, mctl);
#if IA64_ABI
      add_to_mangled_name('E', mctl);
#endif /* IA64_ABI */
    }  /* if */
#if IA64_ABI
  } else {
    /* No <initializer>; indicate such with an "E". */
    add_to_mangled_name('E', mctl);
#endif /* IA64_ABI */
  }  /* if */
}  /* mangled_encoding_for_initializer */


static void mangled_encoding_for_concept_id(an_expr_node_ptr         expr,
                                            a_mangling_control_block *mctl)
/*
Add mangling for the concept-id expression to the current mangled name.
*/
{
  a_source_correspondence *scp =
                    &expr->variant.concept_id.concept_template->source_corresp;

  check_assertion(expr->kind == (an_expr_node_kind)enk_concept_id);
#if IA64_ABI
  /* There are no IA-64-specific rules for mangling concept-ids; presumably
     they should be mangled as template-ids.  That said, there are differences
     in how gcc and clang do the mangling; clang allocates a substitution and
     gcc appears not to.  Also, clang mangles them as an "external name".
     gcc doesn't appear to mangle the parent information (which can't be
     demangled because <expression> doesn't allow a <nested-name>, only an
     <unresolved-name>).  Since the omission of a parent in the mangled name
     seems like a bug, use the clang mangling scheme by default. */
  a_boolean need_nested_name_close = FALSE;
  a_boolean use_substitution = TRUE;
  a_boolean use_external_encoding = TRUE;
  a_boolean use_parent_mangling = TRUE;
  if (gnu_mode && !clang_mode) {
    use_parent_mangling = FALSE;
    use_substitution = FALSE;
    use_external_encoding = FALSE;
  }  /* if */
  a_source_correspondence *discriminator_scp = NULL;
  if (use_external_encoding) {
    add_str_to_mangled_name("L_Z", mctl);
  }  /* if */
  if (!use_substitution ||
      !add_substitution_if_available(
                            (char *)expr->variant.concept_id.concept_template,
                            iek_template, /*is_pack_expansion=*/FALSE, mctl)) {
    if (use_parent_mangling) {
      mangled_ia64_parent_qualifier(scp, iek_template, &need_nested_name_close,
                                    &discriminator_scp,
                                    /*force_individuation=*/FALSE, mctl);
    }  /* if */
    mangled_name_with_length(scp->name, mctl);
    if (use_substitution) {
      alloc_substitution((char *)expr->variant.concept_id.concept_template,
                         iek_template, /*is_pack_expansion=*/FALSE, mctl);
    }  /* if */
  }  /* if */
  mangled_template_arguments(expr->variant.concept_id.args,
                             /*partial_spec=*/FALSE, /*old_form=*/FALSE,
                             (a_name_reference_ptr)NULL, mctl);
  close_ia64_nested_name(need_nested_name_close, discriminator_scp, mctl);
  if (use_external_encoding) {
    add_to_mangled_name('E', mctl);
  }  /* if */
#else /* !IA64_ABI */
  a_length_reservation length_reservation;
  a_const_char *str = unmangled_or_fabricated_name_of(scp);
  check_assertion(str != NULL);
  if (scp_is_namespace_member(scp)) {
    /* Add parent qualification. */
    r_mangled_parent_qualifier(scp, iek_template, (unsigned long)2,
                               /*needs_to_be_individuated=*/FALSE,
                               (a_source_correspondence **)NULL, mctl);
  }  /* if */
  reserve_space_for_length(&length_reservation, mctl);
  add_str_to_mangled_name(str, mctl);
  mangled_template_arguments(expr->variant.concept_id.args,
                             /*partial_spec=*/FALSE, /*old_form=*/FALSE,
                             (a_name_reference_ptr)NULL, mctl);
  fill_in_length(&length_reservation, mctl);
#endif /* IA64_ABI */
}  /* mangled_encoding_for_concept_id */


static void mangled_encoding_for_lambda(an_expr_node_ptr         expr,
                                        a_mangling_control_block *mctl)
/*
Add mangling for a lambda expression.  This doesn't appear in the IA-64
ABI spec, but sometimes comes up when mangling names in cp_gen_be
configurations.  Follow GCC's lead and mangle it as a braced-init list
with no arguments.
*/
{
  mangled_braced_init_list((an_expr_node_ptr)NULL, (a_constant_ptr)NULL,
                           expr->type, mctl);
}  /* mangled_encoding_for_lambda */


static void mangled_encoding_for_expression_full(
                                  an_expr_node_ptr         expr,
                                  a_boolean                in_dependent_expr,
                                  a_boolean                suppress_address_of,
                                  a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the expression pointed to by expr.
These expressions come up in ck_template_param expressions as template
arguments, as dimensions of arrays in template signatures, and in decltype
expressions.  In the IA-64 ABI, in_dependent_expr is TRUE if this expression is
part of a template-dependent expression.  In the IA-64 ABI, suppress an
initial "address of" mangling on certain expressions when suppress_address_of
is TRUE.
*/
{
  a_const_char     *operation_name;
  an_expr_node_ptr operand;
#if IA64_ABI
  a_boolean        add_address_of;
  unsigned long    cli_subscript_op_count = 0;
#else /* !IA64_ABI */
  unsigned long    num_operands;
#endif /* IA64_ABI */

  expr = skip_compiler_generated_expressions(expr, &suppress_address_of);
  if (expr == NULL) goto done;
  switch (expr->kind) {
    case enk_error:
      check_assertion(is_at_least_one_error());
      add_to_mangled_name('?', mctl);
      break;
    case enk_constant:
#if IA64_ABI
      if (is_typeid_template_param(expr) && !suppress_address_of) {
        /* Add an implicit "&" to a tpck_typeid template parameter constant. */
        mangled_entity_reference(&node_constant(expr)->source_corresp,
                                 (an_il_entry_kind)iek_constant,
                                 (a_routine_info_block *)NULL, 
                                 /*add_address_of=*/TRUE, mctl);
      } else
#endif /* IA64_ABI */
      /* Do not insert code here. */
      {
        mangled_encoding_for_constant(node_constant(expr),
                                      /*old_form=*/FALSE,
                                      in_dependent_expr,
                                      suppress_address_of,
                                      mctl);
      }  /* if */
      break;
    case enk_operation:
#if MICROSOFT_EXTENSIONS_ALLOWED
      check_assertion(expr->variant.operation.kind !=
                                            (an_expr_operator_kind)eok_assume);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      operand = expr->variant.operation.operands;
#if ABI_COMPATIBILITY_VERSION >= 402
      if (is_call_node(expr) ||
          node_operator_is(expr, eok_dot_vacuous_destructor_call) ||
          node_operator_is(expr, eok_points_to_vacuous_destructor_call)) {
        /* Mangle some type of call operation. */
        mangled_call_operation(expr, in_dependent_expr, mctl);
      } else if (node_operator_is(expr, eok_dot_field) ||
                 node_operator_is(expr, eok_dot_static) ||
                 node_operator_is(expr, eok_points_to_field) ||
                 node_operator_is(expr, eok_points_to_static)) {
        /* Mangle a "." or "->" field operation. */
        mangled_selection_operation(expr, /*arguments=*/NULL,
                                    in_dependent_expr, mctl);
      } else
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
      /* Do not insert code here. */
      {
        /* Generic operation. */
        a_boolean bad_operator, is_cast;
#if !IA64_ABI
        /* Operation.  Output has the form
             Opl2Z1ZZ2ZO <-- "Z1 + Z2", Z1/Z2 indicating nontype template
                             parameters.
                       ^---- "O" to end the operation encoding.
                    ^^^----- Second operand.
                 ^^^-------- First operand.
                ^----------- Count of operands.
              ^^------------ Operation, using same encoding as for operator
                             function names.
             ^-------------- "O" for operation.
        */
        /* Put out the initial "O". */
        add_to_mangled_name('O', mctl);
#endif /* !IA64_ABI */
        /* Get the operator name and put it out. */
        operation_name = mangled_expr_operator_name(expr, &bad_operator,
                                                    &is_cast);
        if (bad_operator) {
          /* Unexpected operator.  These are allowed in some cases for
             expressions under sizeof in the IA-64 ABI prior to version 4.2. */
#if CHECKING
#if IA64_ABI && ABI_COMPATIBILITY_VERSION < 402
          if (!mctl->mangling_sizeof_expression)
#endif /* IA64_ABI && ABI_COMPATIBILITY_VERSION < 402 */
          /* Do not insert code here. */
          {
            internal_error(
                         "mangled_encoding_for_expression_full: bad operator");
          }  /* if */
#endif /* CHECKING */
#if IA64_ABI && ABI_COMPATIBILITY_VERSION < 402
          Small_string<50> bad_name = bad_mangled_expr_operator_name(expr);
          add_str_to_mangled_name(bad_name.as_temp_characters(), mctl);
#endif /* IA64_ABI && ABI_COMPATIBILITY_VERSION < 402 */
        } else {
          add_str_to_mangled_name(operation_name, mctl);
        }  /* if */
#if !IA64_ABI
        /* The Cfront mangling has different mangled names to distinguish
           prefix and postfix unary operators. */
#else /* IA64_ABI */
        if (node_operator_is(expr, eok_pre_incr) ||
            node_operator_is(expr, eok_pre_decr)) {
          /* Need to differentiate prefix versions from postfix versions of
             these operations. */
          add_to_mangled_name('_', mctl);
        }  /* if */
#endif /* !IA64_ABI */
        /* For cast operations, put out the type cast to. */
        if (is_cast) {
          if (node_operator_is(expr, eok_ref_cast) ||
              node_operator_is(expr, eok_ref_dynamic_cast)) {
            /* If we're casting to a reference type, the reference isn't
               indicated in the type itself, so we create a new type for
               mangling purposes and mangle that here. */
            a_type_ptr ref_cast = alloc_type((a_type_kind)tk_pointer);
            destination_type_for_reference_cast(expr, ref_cast);
            mangled_encoding_for_type(ref_cast, mctl);
          } else {
            mangled_encoding_for_type(expr->type, mctl);
          }  /* if */
        }  /* if */
#if !IA64_ABI
        /* Put out the count of operands.  Note that the old form (single
           digit length) is used here when compatibility with older versions
           is required. */
        num_operands = number_of_operands_in_list(
                                             expr->variant.operation.operands);
#if ABI_COMPATIBILITY_VERSION < 402
        check_assertion(num_operands <= 9);
#endif /* ABI_COMPATIBILITY_VERSION < 402 */
        store_digits_and_underscore(num_operands,
#if ABI_COMPATIBILITY_VERSION >= 402
                                    /*old_form=*/FALSE,
#else /* ABI_COMPATIBILITY_VERSION < 402 */
                                    /*old_form=*/TRUE,
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
                                    mctl);
#endif /* !IA64_ABI */
        /* Put out the operands. */
        for (operand = expr->variant.operation.operands;
             operand != NULL;
             operand = operand->next) {
#if GNU_EXTENSIONS_ALLOWED
          if (expr->variant.operation.is_gnu_two_operand_question_mark &&
              operand == expr->variant.operation.operands->next) {
            /* Put out a dummy expression for the synthesized second operand
               of the GNU two-operand "?".  This preserves the number of
               operands the demangler expects. */
            add_mangling_for_placeholder_expression(mctl);
          } else
#endif /* GNU_EXTENSIONS_ALLOWED */
          /* Do not insert code here. */
          {
            mangled_encoding_for_expression(operand, in_dependent_expr, mctl);
          }  /* if */
#if IA64_ABI
          if (node_operator_is(expr, eok_cli_subscript) &&
              ++cli_subscript_op_count >= 9) {
            /* The mangling for the C++/CLI subscript operator uses the IA-64
               ABI vendor extended operator mangling, which restricts the
               number of operands to a single digit, hence any additional
               subscripts are discarded here. */
            break;
          }  /* if */
#endif /* IA64_ABI */
        }  /* for */
#if !IA64_ABI
        /* Put out the closing "O". */
        add_to_mangled_name('O', mctl);
#endif /* IA64_ABI */
      }  /* if */
      break;
#if IA64_ABI
    case enk_sizeof:
      if (expr->variant.sizeof_info.is_type) {
        mangled_encoding_for_sizeof(expr->variant.sizeof_info.variant.type,
                                    (an_expr_node_ptr)NULL,
                                   (a_template_param_constant_kind)tpck_sizeof,
                                    expr,
                                    mctl);
      } else {
        check_assertion(!expr->is_lvalue);
        mangled_encoding_for_sizeof((a_type_ptr)NULL,
                                    expr->variant.sizeof_info.variant.expr,
                                   (a_template_param_constant_kind)tpck_sizeof,
                                    expr,
                                    mctl);
      }  /* if */
      break;
    case enk_alignof:
      if (expr->variant.sizeof_info.is_type) {
        mangled_encoding_for_sizeof(expr->variant.sizeof_info.variant.type,
                                    (an_expr_node_ptr)NULL,
                                  (a_template_param_constant_kind)tpck_alignof,
                                    expr,
                                    mctl);
      } else {
        check_assertion(!expr->is_lvalue);
        mangled_encoding_for_sizeof((a_type_ptr)NULL,
                                    expr->variant.sizeof_info.variant.expr,
                                  (a_template_param_constant_kind)tpck_alignof,
                                    expr,
                                    mctl);
      }  /* if */
      break;
#endif /* IA64_ABI */
    case enk_typeid:
      { an_expr_node_ptr  opnds = expr->variant.typeid_info.type_with_opt_expr,
                          arg = opnds->next;
        a_type_ptr        tp = NULL;
        if (arg == NULL) tp = opnds->variant.type_operand.type;
        mangled_encoding_for_sizeof(tp, arg, tpck_typeid, expr, mctl);
      }
      break;
    case enk_sizeof_pack:
      /* Mangle sizeof...(T). */
      mangled_encoding_for_sizeof_pack(expr, mctl);
      break;
    case enk_variable:
      if (node_variable(expr)->is_this_parameter) {
        /* When "this" is used explicitly in a trailing return type, it is
           mangled as a special type of parameter reference.  Seeing a
           "this" variable here should occur only in prototype instantiations;
           use the same mangling as a "this" parameter reference. */
        check_assertion(prototype_instantiations_in_il);
#if IA64_ABI
        add_str_to_mangled_name("fpT", mctl);
#else /* !IA64_ABI */
        add_str_to_mangled_name("I0I", mctl);
#endif /* IA64_ABI */
      } else {
#if IA64_ABI
        mangled_entity_reference(&node_variable(expr)->source_corresp,
                                 (an_il_entry_kind)iek_variable,
                                 (a_routine_info_block *)NULL,
                                 /*add_address_of=*/FALSE, mctl);
#else /* !IA64_ABI */
        /* Encode the variable name, but precede it with a length
           indication. */
        a_length_reservation    length_reservation;
        reserve_space_for_length(&length_reservation, mctl);
        mangled_variable_name_with_possible_qualification(node_variable(expr),
                                                          mctl);
        fill_in_length(&length_reservation, mctl);
#endif /* IA64_ABI */
      }  /* if */
      break;
    case enk_field:
      /* This should only happen for fields of anonymous unions. */
      check_assertion(class_type_supp(
                      scp_parent_class(&node_field(expr)->source_corresp))->
                                                        anonymous_union_kind !=
                                            (an_anonymous_union_kind)auk_none);
#if IA64_ABI
      mangled_entity_reference(&node_field(expr)->source_corresp,
                               (an_il_entry_kind)iek_field,
                               (a_routine_info_block *)NULL,
                               /*add_address_of=*/FALSE, mctl);
#else /* !IA64_ABI */
      mangled_simple_id(&node_field(expr)->source_corresp,
                        (a_template_arg_ptr)NULL,
                        expr->variant.field.name_reference,
                        /*include_length=*/TRUE,
                        mctl);
#endif /* IA64_ABI */
      break;
    case enk_routine:
#if IA64_ABI
      if (suppress_address_of) {
        /* When mangling routines in decltype expressions, the implicit
           "address of" may be suppressed. */
        add_address_of = FALSE;
#if ABI_COMPATIBILITY_VERSION >= 402
      } else if (expr->is_lvalue) {
        /* An "&" was mistakenly added to rvalue routines prior to
           release 4.2. */
        add_address_of = FALSE;
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
      } else {
        add_address_of = TRUE;
      }  /* if */
      mangled_entity_reference(&node_routine(expr)->source_corresp,
                               (an_il_entry_kind)iek_routine,
                               (a_routine_info_block *)NULL,
                               add_address_of,
                               mctl);
#else /* !IA64_ABI */
      mangled_routine_name(node_routine(expr), mctl);
#endif /* IA64_ABI */
      break;
    case enk_reuse_value:  /* Not expected generally, but can come up
                              in Microsoft property expansions. */
      add_mangling_for_placeholder_expression(mctl);
      break;
    case enk_builtin_operation:
      mangled_encoding_for_builtin_operation(expr, mctl);
      break;
    case enk_new_delete:
      /* Mangling for a new/delete operation. */
      { a_const_char     *name; 
        an_expr_node_ptr args = expr->variant.new_delete->arg;
#if !IA64_ABI
        /* new/delete.  new operation encodes placement new arguments (if any),
           followed by the type, followed by initializer arguments (if any).
           delete is a standard operation with one expression.
             
                v--------------vvvvvvvvvv----- These are optional.
             Onwg_1_CiL_2_10Z1Z_1_CiL_1_0O <-- encoding for "::new (10) T (0)"
                                         ^---- "O" to end the encoding.
                                  ^^^^^^^----- Initializers (if any).  An
                                               optional "bi" indicates the
                                               use of a brace-enclosed
                                               initializer list.
                               ^^^------------ Optional initializer count (zero
                                               or more).  Omitted (along with
                                               initializers) if none were
                                               specified.
                            ^^^--------------- Type of new operation.
                    ^^^^^^^^------------------ Placement arguments (if any).
                 ^^^-------------------------- Placement argument count (zero
                                               or more).
                ^----------------------------- "g" indicates global new.
              ^^------------------------------ new operation (nw, nwa).
             ^-------------------------------- "O" for operation.
             
                v----------- This is optional.
             Odlg_1_I1IO <-- encoding for "::delete p1"
                       ^---- "O" to end the operation encoding.
                    ^^^----- Argument to delete.
                 ^^^-------- Argument count (always one for delete).
                ^----------- "g" indicates global delete.
              ^^------------ delete operation (dl, dla).
             ^-------------- "O" for operation.
        */
        add_to_mangled_name('O', mctl);
#else /* IA64_ABI */
        if (expr->variant.new_delete->global_new_or_delete) {
          /* Indicate that this is a global new/delete. */
          add_str_to_mangled_name("gs", mctl);
        }  /* if */
#endif /* !IA64_ABI */
        if (expr->variant.new_delete->is_new) {
          if (is_array_type(expr->variant.new_delete->type)) {
            name = MANGLING_STRING_FOR_OPERATOR_ARRAY_NEW;
          } else {
            name = MANGLING_STRING_FOR_OPERATOR_NEW;
          }  /* if */
        } else if (expr->variant.new_delete->array_delete) {
          name = MANGLING_STRING_FOR_OPERATOR_ARRAY_DELETE;
        } else {
          name = MANGLING_STRING_FOR_OPERATOR_DELETE;
        }  /* if */
        add_str_to_mangled_name(name, mctl);
#if !IA64_ABI
        if (expr->variant.new_delete->global_new_or_delete) {
          /* Indicate that this is a global new/delete. */
          add_to_mangled_name('g', mctl);
        }  /* if */
        store_digits_and_underscore(number_of_operands_in_list(args), 
                                    /*old_form=*/FALSE, mctl);
#endif /* !IA64_ABI */
        /* Note that the first argument of a "new" operator (the size argument)
           is not mangled. */
        if (args != NULL) {
          mangled_expression_list(args, in_dependent_expr, mctl);
        }  /* if */
        if (expr->variant.new_delete->is_new) {
          /* New operations have a type and optional initializer list that
             also have to be mangled. */
#if IA64_ABI
          add_to_mangled_name('_', mctl);
#endif /* IA64_ABI */
          mangled_encoding_for_type(expr->variant.new_delete->type, mctl);
          mangled_encoding_for_initializer(
                                        expr->variant.new_delete->dynamic_init,
                                        mctl);
        }  /* if */
#if !IA64_ABI
        add_to_mangled_name('O', mctl);
#endif /* !IA64_ABI */
      }
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case enk_gcnew:
      /* Mangle a gcnew expression (C++/CLI only). */
      { an_expr_node_ptr dimensions =
                         expr->variant.gcnew_info->cli_array_dimension_lengths;
#if IA64_ABI
        /* There is no IA-64 ABI standard mangling for C++/CLI gcnew, and
           the vendor extended operator syntax isn't flexible enough to
           support gcnew, so add an EDG-specific mangling (patterned after
           the mangling for "new"):
             ::= gc _ <type> E                 # gcnew type
             ::= gc _ <type> <initializer>     # gcnew type (init)
             ::= gc <expression>* _ <type> E
                                               # gcnew array<type>(dims)
             ::= gc <expression>* _ <type> <initializer>
                                               # gcnew array<type>(dims) {init}
        */
#else /* !IA64_ABI */
        /* gcnew is mangled with initial dimension expressions (which may
           be a list of length zero if not an array), followed by the type,
           followed by the initializers (which may be entirely omitted if
           there are no initializers).

             Ogc_1_CiL_1_2Z1ZO <-- encoding for "gcnew array<int>(2)"
                             ^---- "O" to end the encoding.
                          ^^^----- Type.
                   ^^^^^^^-------- Dimension expressions (if any).
                ^^^--------------- Dimension count (zero or more).
              ^^------------------ gcnew operation.
             ^-------------------- "O" for operation.
        */
        add_to_mangled_name('O', mctl);
#endif /* IA64_ABI */
        add_str_to_mangled_name(MANGLING_STRING_FOR_OPERATOR_GCNEW, mctl);
#if !IA64_ABI
        store_digits_and_underscore(number_of_operands_in_list(dimensions),
                                    /*old_form=*/FALSE, mctl);
#endif /* !IA64_ABI */
        if (dimensions != NULL) {
          check_assertion(expr->variant.gcnew_info->is_cli_array);
          mangled_expression_list(dimensions, in_dependent_expr, mctl);
        }  /* if */
#if IA64_ABI
        add_to_mangled_name('_', mctl);
#endif /* IA64_ABI */
        mangled_encoding_for_type(expr->variant.gcnew_info->type, mctl);
        mangled_encoding_for_initializer(
                                        expr->variant.gcnew_info->dynamic_init,
                                        mctl);
#if !IA64_ABI
        add_to_mangled_name('O', mctl);
#endif /* !IA64_ABI */
      }
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case enk_throw:
      /* Mangle a throw-expression (or a rethrow). */
#if !IA64_ABI
      /* Throw.  Output has the form
           Otw1I1IO <-- encoding for "throw p1"
                  ^---- "O" to end the operation encoding.
               ^^^----- Throw expression.
              ^-------- Count of expressions (zero for a rethrow otherwise
                        one).
            ^^--------- Throw operation.
           ^----------- "O" for operation.
      */
      add_to_mangled_name('O', mctl);
      add_str_to_mangled_name("tw", mctl);
#endif /* IA64_ABI */
      if (expr->variant.throw_info == NULL) {
        /* A rethrow; indicate that there is no expression. */
#if IA64_ABI
        add_str_to_mangled_name("tr", mctl);
#else /* !IA64_ABI */
        add_to_mangled_name('0', mctl);
#endif /* IA64_ABI */
      } else {
#if IA64_ABI
        add_str_to_mangled_name("tw", mctl);
#else /* !IA64_ABI */
        add_to_mangled_name('1', mctl);
#endif /* !IA64_ABI */
        check_assertion(expr->variant.throw_info->dynamic_init != NULL);
        mangled_dynamic_init(expr->variant.throw_info->dynamic_init,
                             expr->variant.throw_info->type,
                             /*is_static_cast=*/FALSE, mctl);
      }  /* if */
#if !IA64_ABI
      add_to_mangled_name('O', mctl);
#endif /* !IA64_ABI */
      break;
    case enk_param_ref:
      mangled_encoding_for_param_reference(expr, mctl);
      break;
    case enk_temp_init:
      /* Mangle an enk_temp_init as a conversion operation.  Note that some
         (implicit) enk_temp_init operations are stripped and aren't part of
         the mangled name. */
      {
        a_dynamic_init_ptr  dip = expr->variant.init.dynamic_init;
        check_assertion(dip != NULL);
        mangled_dynamic_init(dip, expr->type, expr->is_static_cast, mctl);
      }
      break;
    case enk_braced_init_list:
      /* Mangling for a brace-enclosed initializer list. */
      mangled_braced_init_list(expr->variant.braced_init_list,
                               (a_constant_ptr)NULL,
                               (a_type_ptr)NULL, mctl);
      break;
    case enk_await:
      /* Mangling for "co_await <operand>". */
#if !IA64_ABI
      /* Put out the initial "O". */
      add_to_mangled_name('O', mctl);
#endif /* !IA64_ABI */
      add_str_to_mangled_name(MANGLING_STRING_FOR_OPERATOR_AWAIT, mctl);
#if !IA64_ABI
      add_to_mangled_name('1', mctl);
#endif /* !IA64_ABI */
      mangled_encoding_for_expression(expr->variant.await_info.operand,
                                      in_dependent_expr, mctl);
#if !IA64_ABI
      /* Put out the closing "O". */
      add_to_mangled_name('O', mctl);
#endif /* IA64_ABI */
      break;
    case enk_fold:
      /* Mangling for fold-expressions. */
      { a_boolean       unary, left_assoc;
        a_const_char    *opcode;
        unary = expr->variant.fold.operands->next == NULL;
        left_assoc = expr->variant.fold.left_associative;
#if !IA64_ABI
        /* Put out the initial "O". */
        add_to_mangled_name('O', mctl);
#endif /* !IA64_ABI */
        /* Add a code corresponding to the kind of folding... */
        add_str_to_mangled_name(
          unary ? (left_assoc ? MANGLING_STRING_FOR_LEFT_UNARY_FOLD
                              : MANGLING_STRING_FOR_RIGHT_UNARY_FOLD)
                : (left_assoc ? MANGLING_STRING_FOR_LEFT_BINARY_FOLD
                              : MANGLING_STRING_FOR_RIGHT_BINARY_FOLD),
          mctl);
        /* ... followed by a code for the binary operator that is folded. */
        if (expr->variant.fold.operator_token ==
                                              (a_token_kind)tok_period_star) {
          /* For most operators, we can retrieve the encoding via their
             "opname kind".  However, ".*" isn't overloadable and therefore
             has no associated "opname kind". */
          opcode = MANGLING_STRING_FOR_OPERATOR_DOT_STAR;
        } else {
          an_opname_kind  opname =
                opname_kind_for_token[(int)expr->variant.fold.operator_token];
          opcode = mangled_operator_name(opname, 2);
        }  /* if */
        add_str_to_mangled_name(opcode, mctl);
        mangled_encoding_for_expression(expr->variant.fold.operands,
                                        in_dependent_expr, mctl);
        if (!unary) {
          mangled_encoding_for_expression(expr->variant.fold.operands->next,
                                          in_dependent_expr, mctl);
        }  /* if */
#if !IA64_ABI
        /* Put out the closing "O". */
        add_to_mangled_name('O', mctl);
#endif /* IA64_ABI */
      }
      break;
    case enk_concept_id:
      mangled_encoding_for_concept_id(expr, mctl);
      break;
    case enk_lambda:
      mangled_encoding_for_lambda(expr, mctl);
      break;
    case enk_requires:
      /* Mangling for requires expressions is not needed for ABI purposes
         but may be requested in some C++-generating configurations when
         MANGLE_ALL_NAMES is TRUE.  Issue a discretionary error and add
         an indication to the mangled name that this is not supported. */
      pos_diagnostic(es_discretionary_error, ec_mangling_for_requires,
                     &expr->position);
      (void)set_severity_for_error_number((int)ec_mangling_for_requires,
                                          es_once, /*make_default=*/FALSE);
      add_to_mangled_name('?', mctl);
      break;
    case enk_pack_index:
      /* The IA-64 ABI does not specify how to mangle a pack index expression
         yet, so just use the pack for now. */
      mangled_encoding_for_expression(expr->variant.pack_index.expr,
                                      in_dependent_expr, mctl);
      break;
#if VLA_DEALLOCATIONS_IN_IL
    case enk_vla_dealloc:
#endif /* VLA_DEALLOCATIONS_IN_IL */
    case enk_type_operand:  /* Only expected under enk_builtin_operation. */
    case enk_initializer:
    default:
      /* Unexpected expression kind. */
#if DEBUG
/* GCOVR_EXCL_START */
      db_expression(expr);
/* GCOVR_EXCL_STOP */
#endif  /* DEBUG */
      unexpected_condition_str(
                             "mangled_encoding_for_expression_full: bad kind");
  }  /* switch */
done:;
}  /* mangled_encoding_for_expression_full */


static a_type_ptr call_operator_function_type_for_lambda(a_type_ptr lambda)
/*
Returns the type of the operator() member function of the specified
lambda closure type.
*/
{
  a_type_ptr    type = NULL;
  a_symbol_ptr  closure_sym = symbol_for(lambda), symbols, sym;
  a_class_symbol_supplement_ptr
                cssp;

  /* Search through the list of symbols on the lambda class' symbol list to
     find the operator() routine (the routine may have already been promoted
     but this search method will work in either case). */
  check_assertion(type_is_lambda_closure(lambda) && closure_sym != NULL);
  cssp = class_symbol_supp(closure_sym);
  symbols = cssp->pointers_block.symbols;
  for (sym = symbols; sym != NULL; sym = sym->next_in_scope) {
    a_routine_ptr  rp = NULL;
    if (symbol_is(sym, sk_member_function)) {
      rp = sym->variant.routine.ptr;
    } else if (symbol_is(sym, sk_function_template)) {
      rp = sym->variant.template_info->variant.function.routine;
    }  /* if */
    if (rp != NULL) {
      if (rp->special_kind == (a_special_function_kind)sfk_operator &&
          rp->variant.opname_kind == (an_opname_kind)onk_function_call) {
        type = rp->type;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  check_assertion(type != NULL);
  return type;
}  /* call_operator_function_type_for_lambda */


static a_boolean unnamed_type_has_no_discriminator(a_type_ptr type)
/*
Returns TRUE if the specified unnamed type has no discriminator.
This can be called for class or enum types, though currently no enum types are
compiler-generated.  Compiler-generated class types (e.g., for exception
handling) as well as anonymous unions are class types for which the front end
does not generate discriminators.  Such class types are assigned a unique __Cnn
name (as the encoding for unnamed class types relies on a unique discriminator
which these types are lacking).
*/
{
  a_boolean   result = FALSE;

  check_assertion((is_immediate_class_type(type) ||
                   is_immediate_enum_type(type)) &&
                  (symbol_for(type) == NULL ||
                   type_is_unnamed(type)));
  if (symbol_for(type) == NULL) {
    /* A compiler-generated type (e.g., exception handling). */
    result = TRUE;
  } else if (is_immediate_class_type(type) &&
             class_type_supp(type)->anonymous_union_kind !=
                                           (an_anonymous_union_kind)auk_none) {
    /* An anonymous union.  The front end does not generate discriminators
       for these. */
    result = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (is_immediate_class_type(type) &&
             cli_class_type_kind_is(type, cctk_value) &&
             class_type_supp(type)->corresponding_basic_type != NULL) {
    /* Box types created for C++/CLI enum types are not given discriminators
       by the front end. */
    result = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  return result;
}  /* unnamed_type_has_no_discriminator */


static a_boolean type_is_lambda_in_initializer(a_type_ptr type)
/*
Returns TRUE if the specified type is a lambda closure that was defined
in a (static or nonstatic) data member or variable template initializer.
*/
{
  return type_is_lambda_closure(type) &&
         (class_type_supp(type)->defined_in_variable_initializer ||
          class_type_supp(type)->defined_in_field_initializer);
}  /* type_is_lambda_in_initializer */


static a_boolean type_is_lambda_in_default_argument(a_type_ptr type)
/*
Returns TRUE if the specified type is a lambda closure that was defined
in a default argument of a function.
*/
{
  a_boolean result = FALSE;

  if (type_is_lambda_closure(type)) {
    a_class_symbol_supplement_ptr cssp = symbol_supplement_for_class(type);
    check_assertion(cssp != NULL);
    if (cssp->lambda_immediately_inside_default_arg_expression) {
      a_class_type_supplement_ptr  ctsp = class_type_supp(type);
      check_assertion(!type_is_lambda_in_initializer(type));
      if (ctsp->lambda_parent.routine != NULL) {
        result = TRUE;
      } else {
        /* The lambda was recorded as appearing in a default argument, but the
           function whose default argument it is was not recorded.  This can
           happen in severe error cases. */
        check_assertion(is_at_least_one_error());
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* type_is_lambda_in_default_argument */

/*
Returns TRUE if the specified type is a lambda that is defined in a
default argument and the mangling produced should reflect that.  Lambdas
that are defined in default arguments in declarations at function scope (but
not those defined in a default argument of a local class member function),
are mangled as though the lambda is defined in the local scope (this avoids
a potential conflict with lambdas defined in default arguments in other
functions with the same signature).
*/

#define mangle_as_lambda_in_default_argument(type)                      \
  (type_is_lambda_in_default_argument(type) &&                          \
   (!(type)->source_corresp.is_local_to_function ||                     \
    (type)->source_corresp.is_class_member))

STATIC_THREAD a_const_char
		*placeholder_name = "";
                        /* A character string that is returned in place of a
                           module id, namespace name, or type name when no
                           module id is required (i.e., in a mangling
                           pre-pass).  Returning a non-NULL string in these
                           cases allows the mangling of the name to proceed
                           without undue error checking; the mangled name will
                           be discarded by end_mangling_full when it sees that
                           mctl->lacking_module_id is TRUE. */

#if !IA64_ABI

static void mangled_specialization_indication(a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding qualifier that indicates specialization.
*/
{
  add_str_to_mangled_name("__S", mctl);
}  /* mangled_specialization_indication */


static void add_local_name_suffix(unsigned long            id_number,
                                  a_routine_ptr            routine,
                                  a_mangling_control_block *mctl)
/*
Add a suffix to the current name for a function-local entity.  id_number
is an id number for the entity (usually a scope number within the routine)
and "routine" is the routine to which the entity is local.
*/
{
  /* The mangling is "__Lnn", where "nn: is the id number, followed by the
     mangled name of the function. */
  add_str_to_mangled_name("__L", mctl);
  add_number_to_mangled_name(id_number, mctl);
  add_str_to_mangled_name("__", mctl);
  if (routine->source_corresp.name != NULL) {
    mangled_function_name_externalized_if_necessary(
                                         routine,
                                         /*suppress_param_encoding=*/FALSE,
                                         /*suppress_parent_encoding=*/FALSE,
                                         /*force_primary_name=*/TRUE,
                                         /*base_name_offset=*/(sizeof_t *)NULL,
                                         mctl);
  }  /* if */
}  /* add_local_name_suffix */


static a_const_char *fabricate_name_for_unnamed_type(
                                                a_type_ptr               type,
                                                a_mangling_control_block *mctl)
/*
Return a fabricated name (already allocated in the file scope memory region)
for the specified unnamed type.  If the fabricated name for the type requires a
module id (as happens during a mangling pre-pass), a placeholder name is
returned and mctl->lacking_module_id is set to TRUE.
*/
{
  a_mangling_control_block local_mctl;
  a_const_char             *fabricated_name, *name;
  unsigned long            discriminator;

  /* Note that the fabricated name is created using the mangling text buffer
     utilities (e.g., start_mangling, add.*to_mangled_name, etc.) rather than
     string utilities (because some of the functions that are called already
     place their output into mangling text buffers).  The fabricated name
     created here is placed in a separate mangling text buffer than the name
     currently being mangled (as represented by the argument mctl), though
     presumably the caller will then add this name to that buffer as well
     (after saving the fabricated name for later use). */
  /* Note further that this scheme (of using nested start_mangling calls)
     couldn't be used in the IA-64 ABI because the substitutions would be
     affected. */
  start_mangling(&local_mctl);
  if (type_is_lambda_closure(type)) {
    /* Generate a class name for the lambda closure class. */
    a_type_ptr    call_operator_type;
    call_operator_type = call_operator_function_type_for_lambda(type);
    check_assertion(symbol_supplement_for_class(type) != NULL);
    discriminator = symbol_supplement_for_class(type)->discriminator;
    check_assertion(discriminator != 0 || is_at_least_one_error());
    if (mangle_as_lambda_in_default_argument(type)) {
      /* A lambda used in a default argument is given a name with the
         following format:

          __Ud1_2_Fv__L0__f__FiT1 <-- name given to lambda in default arg:
                                      void f(int i=[]{return 0;}(),int k=0)
                    ^^^^^^^^^^^^^---- Local to function f(int,int).
                  ^^----------------- Lambda operator() function type.
                ^-------------------- Declared in trailing parameter 2.
              ^---------------------- Instance 1 (within parameter).
          ^^^^----------------------- __Ud indicates lambda in default arg.

         Note that not all lambdas defined in default arguments are
         mangled as such -- lambdas defined in default arguments of
         functions declared in the function scope are mangled as though
         they appear in the function scope (to avoid possible conflicts
         with similarly defined lambdas).
         */
      a_routine_ptr enclosing_routine;
      add_str_to_mangled_name("__Ud", &local_mctl);
      add_number_to_mangled_name(discriminator, &local_mctl);
      add_to_mangled_name('_', &local_mctl);
      add_mangling_for_default_arg_in_local_type(type, &enclosing_routine,
                                                 &local_mctl);
      add_to_mangled_name('_', &local_mctl);
      mangled_encoding_for_function_type(call_operator_type,
                                         /*do_return_type=*/FALSE,
                                         /*mangling_function_name=*/FALSE,
                                         &local_mctl);
      add_local_name_suffix((unsigned long)0, enclosing_routine, &local_mctl);
    } else {
      /* All other lambdas are given a name with the following format:

           __Ul1_Fif <-- name given to lambda:
                         [](int, float)->float {return 0.0;}(i, f);
                 ^^^---- Lambda operator() function type (int, float).
               ^-------- Instance (within parent scope).
           ^^^^--------- "__Ul" indicates lambda, "__Um" indicates
                         lambda in a member initializer.
         */
      if (type_is_lambda_in_initializer(type)) {
        add_str_to_mangled_name("__Um", &local_mctl);
      } else {
        add_str_to_mangled_name("__Ul", &local_mctl);
      }  /* if */
      add_number_to_mangled_name(discriminator, &local_mctl);
      add_to_mangled_name('_', &local_mctl);
      mangled_encoding_for_function_type(call_operator_type,
                                         /*do_return_type=*/FALSE,
                                         /*mangling_function_name=*/FALSE,
                                         &local_mctl);
    }  /* if */
  } else {
    /* All other unnamed types are simply mangled with __Ut followed by
       the discriminator number (as calculated by the front end). */
    add_str_to_mangled_name("__Ut", &local_mctl);
    if (is_immediate_class_type(type)) {
      discriminator = symbol_supplement_for_class(type)->discriminator;
    } else {
      check_assertion(is_immediate_enum_type(type));
      discriminator =
               symbol_for(type)->variant.enumeration.extra_info->discriminator;
    }  /* if */
    check_assertion(discriminator != 0);
    add_number_to_mangled_name(discriminator, &local_mctl);
  }  /* if */
  fabricated_name = end_mangling_full((a_source_correspondence *)NULL,
                                      /*final=*/FALSE, &local_mctl);
  if (local_mctl.lacking_module_id) {
    /* We can't fabricate the type's mangled name; alert the caller that
       the name we're returning is not the final name. */
    name = placeholder_name;
    mctl->lacking_module_id = TRUE;
  } else {
    /* Allocate space for the generated name and copy it. */
    name = alloc_lowered_name_string(local_mctl.length);
    (void)strcpy((char *)name, fabricated_name);
  }  /* if */
  return name;
}  /* fabricate_name_for_unnamed_type */

#endif /* !IA64_ABI */

/*
Seed number for unnamed type names.
*/
STATIC_THREAD unsigned long
		unnamed_type_seed;

static a_const_char *give_unnamed_class_or_enum_a_name(
                                     a_type_ptr                          type,
                                     ARG_UNUSED a_mangling_control_block *mctl)
/*
If the indicated class or enum type is unnamed, fabricate a name (if it needs
one) and return that name.  In the Cfront ABI, all unnamed types are given a
fabricated name here (see the exception for the mangling pre-pass below).
Mangling then uses this name as it would any other class or enum type name.
In the IA-64 ABI, only a small set of unnamed types are given a name (those
generated by the compiler and anonymous unions).  The rest of the unnamed types
have encodings specified by the IA-64 ABI and are encoded as needed.  Can
return NULL in the IA-64 ABI.  In the Cfront ABI, during a mangling pre-pass,
the type remains unnamed (until a future call) and a placeholder name is
returned (so mangling can proceed), but mctl->lacking_module_id is set so the
mangled name will eventually be discarded.  
*/
{
  a_const_char     *name;
  Small_string<50> buffer;

  check_assertion(is_immediate_class_type(type) ||
                  is_immediate_enum_type(type));
  /* Note that we may be changing a type that is not being lowered yet, but
     that's okay -- the name in the IL entry is not used by the front end. */
  name = type->source_corresp.name;
  if (name == NULL) {
    if (unnamed_type_has_no_discriminator(type)) {
      unsigned long num;
      /* If there is no discriminator, generate a unique __Cnn or __Enn name
         using the next number in sequence. */
      num = ++unnamed_type_seed;
      /* Set name_has_been_mangled to indicate that the generated name is the
         complete name.  No parent information, for example, will be added.
         The generated name by itself is unique across the whole compilation.
         */
      type->source_corresp.name_has_been_mangled = TRUE;
      type->source_corresp.unnamed_entity_given_fabricated_name = TRUE;
      if (is_immediate_class_type(type)) {
        buffer.reset_to("__C", num);
      } else {
        buffer.reset_to("__E", num);
      }  /* if */

      char *allocated_name = alloc_lowered_name_string(buffer.length() + 1);
      buffer.write_to_buffer(allocated_name, buffer.length() + 1);
      type->source_corresp.name = allocated_name;
      name = allocated_name;
#if !IA64_ABI
    } else {
      /* In the Cfront ABI, all unnamed types are assigned generated names
         and the normal mangling mechanism for class/enum types is used for
         the generation of mangled names. */
      name = fabricate_name_for_unnamed_type(type, mctl);
      if (mctl->lacking_module_id) {
        /* The mangled name requires a module id. */
        check_assertion(in_mangling_pre_pass);
        name = placeholder_name;
      } else {
        type->source_corresp.name = name;
        type->source_corresp.unnamed_entity_given_fabricated_name = TRUE;
      }  /* if */
#endif /* !IA64_ABI */
    }  /* if */
  }  /* if */
  return name;
}  /* give_unnamed_class_or_enum_a_name */


static a_const_char *module_id_for_source_corresp(
                                                a_source_correspondence  *scp,
                                                a_mangling_control_block *mctl)
/*
Return the module id for the translation unit which the given source
correspondence is part of.  For a source correspondence with no
associated symbol, use the current translation unit.  During the mangling
pre-pass, return a placeholder name and set mctl->lacking_module_id to alert
the caller that the mangled name under construction should be discarded.  Note
that even in cases where a module id may be available, when we're in the
mangling pre-pass, we don't want to mangle names with this module id -- it's
unique to a translation unit and by creating mangled names that may then be
included using the PCH mechanism into another translation unit, we could end up
with multiply defined symbols.  
*/
{
  a_translation_unit_ptr tup;
  a_const_char           *module_id;

  if (in_mangling_pre_pass) {
    /* In the mangling pre-pass, return a placeholder and set a flag to
       indicate to the caller that the mangled name should be discarded. */
    mctl->lacking_module_id = TRUE;
    module_id = placeholder_name;
  } else {
    tup = (scp->assoc_info != NULL) ? trans_unit_for_source_corresp(scp) :
                                      curr_translation_unit;
    module_id = *tup->module_id_ptr;
    if (module_id == NULL) {
      /* Although we'd prefer to base the module id on a routine/variable
         defined in this translation unit (so it is guaranteed to be unique),
         we'll have to settle for a weaker module id in this case. */
      module_id = make_module_id(NULL);
      check_assertion(module_id != NULL);
    }  /* if */
  }  /* if */
  return module_id;
}  /* module_id_for_source_corresp */


static a_const_char *give_unnamed_namespace_a_name(
                                                a_namespace_ptr          nsp,
                                                a_mangling_control_block *mctl)
/*
If the indicated namespace is unnamed, give it a fabricated name (if possible).
If we're in the mangling pre-pass, mctl->lacking_module_id will be set to TRUE
and a placeholder name will be returned.  A subsequent call (once the module id
has been chosen) will give the namespace an appropriate name.
*/
{
  a_const_char *name, *prefix;
  sizeof_t     name_len;

  /* Note that we may be changing a namespace that is not being lowered yet,
     but that's okay -- the name in the IL entry is not used by the front
     end. */
  name = nsp->source_corresp.name;
  if (name == NULL) {
    /* The namespace is unnamed, so fabricate a name and use it as both
       the "mangled" name and the fabricated name. */
    a_const_char    *module_id;
    a_namespace_ptr parent_nsp;
    a_boolean       lacking_module_id = FALSE;
    /* The name is either __N or _GLOBAL__N_ followed by the module id. */
    check_assertion(!nsp->source_corresp.is_class_member);
    parent_nsp = parent_namespace_or_null(nsp);
    if (parent_nsp != NULL &&
        unmangled_name_of(&parent_nsp->source_corresp) == NULL) {
      /* A nested unnamed namespace within an unnamed namespace.
         Forgo the module id; the name will be unique within the parent
         namespace. */
      module_id = "";
    } else {
      check_assertion(!nsp->is_namespace_alias);
      module_id = module_id_for_source_corresp(&nsp->source_corresp, mctl);
      lacking_module_id = mctl->lacking_module_id;
    }  /* if */
    if (lacking_module_id) {
      /* No module id is available yet; return a temporary name for now
         so mangling can continue. */
      name = placeholder_name;
    } else {
#if IA64_ABI
      /* g++ uses "_GLOBAL__N_" and recognizes that in its demangler. */
      prefix = "_GLOBAL__N_";
#else /* !IA64_ABI */
      prefix = "__N";
#endif /* IA64_ABI */
      name_len = strlen(prefix) + strlen(module_id) + 1;
      name = alloc_lowered_name_string(name_len);
      (void)strcpy((char *)name, prefix);
      (void)strcpy((char *)name+strlen(prefix), module_id);
      nsp->source_corresp.name = name;
      nsp->source_corresp.name_has_been_mangled = TRUE;
      nsp->source_corresp.unmangled_name_or_mangled_encoding = name;
      nsp->source_corresp.unnamed_entity_given_fabricated_name = TRUE;
    }  /* if */
  }  /* if */
  return name;
}  /* give_unnamed_namespace_a_name */

static void mangle_type_name(a_type_ptr type);

static void give_unnamed_template_param_member_a_name(
                                                a_type_ptr               type,
                                                a_mangling_control_block *mctl)
/*
Give an unnamed template param member type that refers to an unnamed enum or
class member the same name as the original type (which has previously or will
now most likely become named).  Set its name mangling fields to match that of
the original type (which can be NULL in the mangling pre-pass).  If the name
cannot be determined yet, set mctl->lacking_module_id to TRUE to alert the
caller.
*/
{
  a_type_ptr nested_type;

  check_assertion(!has_name(type) &&
                  type->kind == tk_template_param &&
                  type->variant.template_param.kind ==  tptk_member);
  nested_type = type->variant.template_param.extra_info->orig_nested_type;
  if (nested_type != NULL) {
    if (is_immediate_class_type(nested_type) ||
        is_immediate_enum_type(nested_type)) {
      /* First, give the original type a name if possible (in the mangling
         pre-pass a name may not be available yet), then copy the
         relevant pieces to the template parameter. */
      mangle_type_name(nested_type);
      if (has_name(nested_type)) {
        type->source_corresp.name = nested_type->source_corresp.name;
        type->source_corresp.unmangled_name_or_mangled_encoding =
                nested_type->source_corresp.unmangled_name_or_mangled_encoding;
        type->source_corresp.name_has_been_mangled =
                             nested_type->source_corresp.name_has_been_mangled;
        type->source_corresp.unnamed_entity_given_fabricated_name =
              nested_type->source_corresp.unnamed_entity_given_fabricated_name;
      } else {
        /* Name can't be determined yet.  Alert the caller. */
        check_assertion(in_mangling_pre_pass);
        mctl->lacking_module_id = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
}  /* give_unnamed_template_param_member_a_name */


/*
Seed number for unnamed member variable names.
*/
STATIC_THREAD unsigned long
		unnamed_member_variable_name_seed;


static void give_unnamed_member_variable_a_name(a_variable_ptr var)
/*
If the indicated member variable is unnamed, give it a name.
*/
{
  if (var->source_corresp.name == NULL) {
    /* The member variable is unnamed, so make up a name. */
    /* The name is __Vnn, where nn is a unique number for the
       member variable.  This is not from the ARM or cfront. */
    unnamed_member_variable_name_seed++;

    Small_string<50> buffer("__V", unnamed_member_variable_name_seed);
    char             *name = alloc_lowered_name_string(buffer.length() + 1);
    buffer.write_to_buffer(name, buffer.length() + 1);
    var->source_corresp.name = name;
    var->source_corresp.unmangled_name_or_mangled_encoding = name;
    var->source_corresp.name_has_been_mangled = TRUE;
  }  /* if */
}  /* give_unnamed_member_variable_a_name */


static void mangled_encoding_for_template_template_argument(
                                                a_template_arg_ptr       tap,
                                                a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the template template argument
given by tap.
*/
{
  a_template_ptr temp = skip_simple_alias_templates(tap->variant.templ.ptr);

  if (temp->template_info != NULL && temp->template_info->is_error) {
    /* Don't get confused on error cases. */
  } else if (temp->kind == (a_template_kind)templk_template_template_param) {
    /* The value of the argument is itself a template template parameter. */
#if IA64_ABI && ABI_COMPATIBILITY_VERSION >= 414
    if (!add_substitution_if_available((char *)temp, iek_template,
                                       /*is_pack_expansion=*/FALSE, mctl))
#endif /* IA64_ABI && ABI_COMPATIBILITY_VERSION >= 414 */
    /* Do not insert code here. */
    {
      mangled_encoding_for_template_parameter(
                                       &temp->coordinates,
                                       (a_template_arg *)NULL,
                                       mctl);
#if IA64_ABI && ABI_COMPATIBILITY_VERSION >= 414
      alloc_substitution((char *)temp, iek_template,
                         /*is_pack_expansion=*/FALSE, mctl);
#endif /* IA64_ABI && ABI_COMPATIBILITY_VERSION >= 414 */
    }  /* if */
  } else {
    /* The value of the argument is a template. */
    a_source_correspondence *scp = &temp->source_corresp;
#if !IA64_ABI
    a_length_reservation    length_reservation;

    /* Name of template.  The encoding is like
         4abcd <-- encoding for template "abcd"
          ^^^^---- Name of entity.
         ^-------- Length of the name.
    */
    check_assertion(scp->name != NULL);
    reserve_space_for_length(&length_reservation, mctl);
    /* Put out the base part of the name. */
    add_str_to_mangled_name(scp->name, mctl);
    if (scp_is_class_or_namespace_member(scp)) {
      /* Add two underscores after the name. */
      add_str_to_mangled_name("__", mctl);
      /* Put out the name of the class or namespace of which this template
         is a member. */
      mangled_parent_qualifier(scp, iek_template, mctl);
    }  /* if */
    fill_in_length(&length_reservation, mctl);
#else /* IA64_ABI */
    if (!add_substitution_if_available((char *)temp, iek_template,
                                       /*is_pack_expansion=*/FALSE, mctl)) {
      a_boolean need_nested_name_close = FALSE;
      a_source_correspondence *discriminator_scp;
      /* Add a parent qualifier if needed. */
      mangled_ia64_parent_qualifier(scp, iek_template,
                                    &need_nested_name_close, 
                                    &discriminator_scp,
                                    /*force_individuation=*/FALSE,
                                    mctl);
      /* Add the name for the template itself. */
      mangled_name_with_length(scp->name, mctl);
      close_ia64_nested_name(need_nested_name_close, discriminator_scp, mctl);
      alloc_substitution((char *)temp, iek_template,
                         /*is_pack_expansion=*/FALSE, mctl);
    }  /* if */
#endif /* IA64_ABI */
  }  /* if */
}  /* mangled_encoding_for_template_template_argument */


static void mangled_template_arguments_or_parameter_pack(
                                   a_template_arg_ptr       *template_arg,
                                   ARG_UNUSED a_boolean     partial_spec,
                                   a_boolean                old_form,
                                   a_name_reference_ptr     name_reference,
                                   a_boolean                is_pack,
                                   a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the template arguments or parameter
pack given by *template_arg.  If partial_spec is TRUE, this argument list is
the first one on a partial specialization.  If old_form is TRUE, use the old
form of length specification in the mangling for lengths of literals.
name_reference (when non-NULL) is used to ensure that the mangled list of
template arguments accurately represents those that appeared in the source form
(this is used when mangling template arguments for function templates that
appear in a <simple-id>).  If name_reference is NULL, all of the arguments
pointed to by template_arg are mangled.  If is_pack is TRUE, *template_arg
(which may be NULL) is the beginning of a parameter pack (which is mangled as a
nested template argument list).  In this case, *template_arg is set on return
to the argument that follows the parameter pack (and may be NULL if it was the
last argument in the list).
*/
{
  a_template_arg_ptr   tap;
  long                 tap_no;
#if !IA64_ABI
  a_const_char         *str;
  a_length_reservation length_reservation;
  a_boolean            saved_suppress_partial_spec_args =
                                              mctl->suppress_partial_spec_args;
  /* The mangled form of template arguments is something like
       __tm__3_ii
               ^^--- Two template arguments of type int.
             ^------ Total length of template argument list string,
                     including the underscore.
         ^^--------- Fixed string, indicates "parameterized type".
     When distinct_template_signatures is FALSE, "__pt__" is used instead
     of "__tm__".  For the first argument list of a partial specialization,
     "__ps__" is used.  The same encoding is used for template argument packs
     with the string "__pk__" being used to introduce the pack.
  */
  if (is_pack) {
    str = "__pk__";
  } else if (!distinct_template_signatures) {
    str = "__pt__";
  } else if (partial_spec) {
    str = "__ps__";
  } else {
    str = "__tm__";
  }  /* if */
  add_str_to_mangled_name(str, mctl);
#if ABI_COMPATIBILITY_VERSION > 245
  /* Suppress information on partial specializations in any parent types
     referenced in the template arguments. */
  mctl->suppress_partial_spec_args = TRUE;
#endif /* ABI_COMPATIBILITY_VERSION > 245 */
  reserve_space_for_length(&length_reservation, mctl);
  add_to_mangled_name('_', mctl);
#else /* IA64_ABI */
  if (is_pack &&
      (!emulate_gnu_abi_bugs
#if ABI_COMPATIBILITY_VERSION > 411
                             || gnu_version >= 50000 || clang_mode
#endif /* ABI_COMPATIBILITY_VERSION > 411 */
                                                                  )) {
    /* Mark the start of the argument pack.  GNU versions prior to 5.0
       used "I" here. */
    add_to_mangled_name('J', mctl);
  } else {
    /* Mark the start of the template arguments.  The original IA-64 ABI
       specification mangled argument packs with an "I" rather than a "J". */
    add_to_mangled_name('I', mctl);
  }  /* if */
#endif /* IA64_ABI */
  /* Run through the template argument list, determining the representation
     for each argument. */
  /*lint -e{440}*/
  for (tap = *template_arg, tap_no = 0;
       tap != NULL;
       tap_no++) {
    if (name_reference != NULL &&
        tap_no >= (name_reference->is_template_id ? 
                                       name_reference->num_template_arguments :
                                       0)) {
      /* If a name_reference has been specified, it specifies the number
         of arguments to emit (which could be zero).  In the case where the
         name reference is not a template-id, no arguments are emitted. */
      break;
    }  /* if */
    if (is_pack && !tap->is_pack_element) {
      /* We've reached the end of a pack; end this parameter pack and process
         any remaining arguments in the caller. */
      break;
    }  /* if */
    check_assertion(is_pack || !tap->is_pack_element);
    if (is_type_templ_arg(tap)) {
      /* Type argument. */
      /* Avoid problems on weird case of missing type in Microsoft mode
         prototype instantiations. */
      if (tap->variant.type != NULL) {
        /* Mangle the type for the template argument, including an indication
           of whether or not the type is a pack expansion. */
        mangled_encoding_for_type_with_pack_expansion(tap->variant.type,
                                             (tap->pack_expansion_descr != NULL
#if ABI_COMPATIBILITY_VERSION >= 520
                                              || tap->is_pack
#endif /* ABI_COMPATIBILITY_VERSION >= 520 */
                                                             ),
                                             mctl);
      }  /* if */
    } else if (is_template_templ_arg(tap)) {
      /* A template template argument. */
      mangled_encoding_for_template_template_argument(tap, mctl);
    } else if (is_start_of_pack_expansion_templ_arg(tap)) {
      check_assertion(!is_pack);
      /* The beginning of a pack expansion.  A parameter pack is mangled as
         a nested set of template arguments.  Skip the start-of-arguments
         marker and recurse with an indication that we're in a pack.  Set the
         name reference to NULL (so only top-level arguments are counted). */
      tap = tap->next;
      mangled_template_arguments_or_parameter_pack(&tap,
                                                   /*partial_spec=*/FALSE,
                                                   old_form,
                                                   (a_name_reference_ptr)NULL,
                                                   /*is_pack=*/TRUE,
                                                   mctl);
      /* On return, tap is set to the next argument to process (or NULL), so
         process that argument now. */
      continue;
    } else if (is_nontype_templ_arg(tap)) {
      a_constant_ptr con = tap->variant.constant;
#if IA64_ABI
      a_boolean      is_expression = FALSE;
      sizeof_t       save_location = 0;
#endif /* IA64_ABI */
      check_assertion_str2(!tap->is_array_bound_of_unknown_type,
                           "mangled_template_arguments_or_parameter_pack:",
                           "is_array_bound_of_unknown_type set");
#if !DO_IL_LOWERING && ABI_COMPATIBILITY_VERSION >= 602
      if (constant_is(con, ck_template_param) &&
          con->variant.template_param.kind ==
                             (a_template_param_constant_kind)tpck_param &&
          !con->type->is_instantiation_dependent &&
          mctl->mangling_prototype_instantiation) {
        /* For template parameters in a prototype instantiation, include a
           mangled encoding for the type of the parameter rather than the
           template parameter itself.  That allows, e.g., differentiation
           between these:
             template <int *> void f() {}
             template <char *> void f() {}
           */
        mangled_encoding_for_type(con->type, mctl);
      } else
#endif /* !DO_IL_LOWERING && ABI_COMPATIBILITY_VERSION >= 602 */
      /* Do not insert code here. */
      {
#if !IA64_ABI
        /* Constant argument.  The encoding for the constant begins with
           an "X". */
        if (constant_is(con, ck_template_param) &&
            con->variant.template_param.kind ==
                             (a_template_param_constant_kind)tpck_expression &&
            expr_node_from_tpck_expression(con) != NULL &&
            expr_node_from_tpck_expression(con)->kind ==
                                           (an_expr_node_kind)enk_concept_id) {
          /* Suppress the 'X' when mangling a concept-id (otherwise the
             demangled name will contain a spurious & prior to the concept-id).
             */
        } else {
          add_to_mangled_name('X', mctl);
        }  /* if */
#else /* IA64_ABI */
#if ABI_COMPATIBILITY_VERSION >= 402
        if (constant_is(con, ck_template_param)) {
          a_constant_ptr  base_con;
          a_boolean       explicit_cast;
          if (is_template_param_cast_constant(con, &base_con, &explicit_cast)&&
              !explicit_cast) {
            /* If the constant is an implicit cast (presumably to the template
               parameter type), the cast shouldn't be part of the mangled name,
               so remove it. */
            con = base_con;
          }  /* if */
        }  /* if */
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
        /* If this argument is an expression, mark it accordingly.  A
           ck_address of reference type doesn't qualify as an expression
           (unless we're trying to be compatible with GNU 3.3 or earlier). */
        if (constant_is(con, ck_template_param) ||
            constant_is(con, ck_ptr_to_member) ||
            constant_is(con, ck_aggregate) ||
            (constant_is(con, ck_address)
#if ABI_COMPATIBILITY_VERSION >= 402
             && (!is_reference_type(con->type) ||
#if ABI_COMPATIBILITY_VERSION >= 607
                 con->variant.address.subobject_path != NULL ||
#endif /* ABI_COMPATIBILITY_VERSION >= 607 */
                 (emulate_gnu_abi_bugs && gnu_abi_version < 30400))
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
                                                                   )) {
          /* These are treated as expressions. */
          is_expression = TRUE;
          /* Mark the start of the expression. */
          add_to_mangled_name('X', mctl);
          /* It's difficult to know ahead of time whether the constant can
             be mangled using the <expr-primary> production or not.  Assume
             that it can't (the usual case), and keep a pointer to the 'X' that
             was just added in case we were wrong. */
          save_location = mangling_text_buffer->size-1;
        }  /* if */
#endif /* IA64_ABI */
        mangled_encoding_for_constant(con,
                                      old_form,
                                      /*in_dependent_expr=*/FALSE,
                                      /*suppress_address_of=*/FALSE,
                                      mctl);
#if IA64_ABI
        if (is_expression) {
          /* If the constant was mangled using an <expr-primary> production
             in the grammar (i.e., mangled name of the constant starts with
             an L), then there is no need to bracket the expression with
             X ... E.  In that case, remove the X that had been put there;
             otherwise close the expression with an E. */
          check_assertion(mangling_text_buffer->buffer[save_location] == 'X');
          if (mangling_text_buffer->buffer[save_location+1] != 'L' ||
              (emulate_gnu_abi_bugs && gnu_abi_version < 30400)) {
            /* In some cases, when emulating older GNU bugs, the extra X ... E
               is required for compatibility. */
            /* Mark the end of the expression. */
            add_to_mangled_name('E', mctl);
          } else if (mangling_text_buffer->buffer[save_location+1] == 'L' &&
                     mangling_text_buffer->buffer[save_location+2] == '_' &&
                     mangling_text_buffer->buffer[save_location+3] == 'Z' &&
                     clang_mode) {
            /* Clang appears to allow "XL_Z...E" mangling for concept-ids. */
            /* Mark the end of the expression. */
            add_to_mangled_name('E', mctl);
          } else {
            /* Overwrite the X with a space (which will be removed at the end
               of mangling for this entity). */
            mangling_text_buffer->buffer[save_location] = ' ';
            mctl->num_leftover_spaces++;
            mctl->length--;
          }  /* if */
        }  /* if */
#endif /* IA64_ABI */
      }  /* if */
    } else {
      unexpected_condition();
    }  /* if */
    tap = tap->next;
  }  /* for */
#if !IA64_ABI
  /* Go back and fill in the length. */
  fill_in_length(&length_reservation, mctl);
  mctl->suppress_partial_spec_args = saved_suppress_partial_spec_args;
#else /* IA64_ABI */
  /* Mark the end of the template arguments. */
  add_to_mangled_name('E', mctl);
#endif /* IA64_ABI */
  if (is_pack) {
    /* Return to the caller the next argument to process
       (which may be NULL).  */
    *template_arg = tap;
  }  /* if */
}  /* mangled_template_arguments_or_parameter_pack */


static void mangled_template_arguments(
                                    a_template_arg_ptr       template_arg_list,
                                    a_boolean                partial_spec,
                                    a_boolean                old_form,
                                    a_name_reference_ptr     name_reference,
                                    a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the template arguments given
by template_arg_list.  If partial_spec is TRUE, this argument list is
the first one on a partial specialization.  If old_form is TRUE, use
the old form of length specification in the mangling for lengths of
literals.  name_reference (when non-NULL) is used to ensure that the
mangled list of template arguments accurately represents those that
appeared in the source form (this is used when mangling template arguments
for function templates that appear in a <simple-id>).  If name_reference
is NULL, all of the arguments pointed to by template_arg_list are mangled.
*/
{
  mangled_template_arguments_or_parameter_pack(&template_arg_list,
                                               partial_spec,
                                               old_form,
                                               name_reference,
                                               /*is_pack=*/FALSE,
                                               mctl);
}  /* mangled_template_arguments */

#if IA64_ABI
  
static a_boolean is_self_discriminated_type(a_type_ptr type)
/*
Return TRUE if discrimination for this type "binds tightly".  This is the
case for unnamed types in the <entity name> portion of <local-rule> where
the discriminator is closely bound to the unnamed type as opposed to appearing
at the end of the <entity name> production.
*/
{
  return (type_is_unnamed(type) &&
          !unnamed_type_has_no_discriminator(type));
}  /* is_self_discriminated_type */


static void add_discriminator(a_discriminator          discriminator,
                              a_boolean                emit_underscore,
                              a_mangling_control_block *mctl)
/*
Add the encoding for a discriminator (used to distinguish like-named
local entities in the IA-64 ABI) to the mangled name.  If emit_underscore
is TRUE, a leading underscore is emitted before the discriminator.
Internally, discriminator is 1-based (zero is used to indicate that a
discriminator hasn't been assigned), but in the IA-64 ABI mangled name, the
number n-2 is used to represent the nth occurrence (and no discriminator
is emitted for the first occurrence).

In some manglings, a discriminator can immediately precede a source name,
making it impossible to determine which digits belong to the discriminator
and which to the length of the source name.  As a result, the following rule
was added to the Itanium ABI in 2009 to make discriminators unambiguous:

  <discriminator> := _ <non-negative number>      # when number < 10
                  := __ <non-negative number> _   # when number >= 10

*/
{
#if ABI_COMPATIBILITY_VERSION >= 401
  /* Make sure the discriminator has been computed. */
  check_assertion(discriminator != 0 || is_at_least_one_error());
#else /* ABI_COMPATIBILITY_VERSION < 401 */
  /* When emulating older ABI versions, this routine may be called for
     an entity whose discriminator is zero, indicating that no discriminator
     has been computed (because discriminators aren't computed for all
     objects).  In this case, simply omit the discriminator from the mangled
     name. */
#endif /* ABI_COMPATIBILITY_VERSION >= 401 */
  if (discriminator > 1) {
    unsigned long n = discriminator - 2;
    if (emit_underscore) {
      add_to_mangled_name('_', mctl);
#if ABI_COMPATIBILITY_VERSION >= 401
      if (n >= 10) add_to_mangled_name('_', mctl);
#endif /* ABI_COMPATIBILITY_VERSION >= 401 */
    }  /* if */
    add_number_to_mangled_name(n, mctl);
#if ABI_COMPATIBILITY_VERSION >= 401
    if (emit_underscore && n >= 10) add_to_mangled_name('_', mctl);
#endif /* ABI_COMPATIBILITY_VERSION >= 401 */
  }  /* if */
}  /* add_discriminator */


static void add_discriminator_if_necessary(a_source_correspondence  *scp,
                                           a_mangling_control_block *mctl)
/*
The entity (of kind entry_kind) whose source correspondence entry is
scp is local to the function "routine".  Add a discriminator to the
mangled name if necessary.  A discriminator is a number used in the
IA-64 ABI to distinguish function-local entities with the same name.
*/
{
  a_discriminator discriminator = 0;
  a_symbol_ptr  sym = (a_symbol_ptr)scp->assoc_info;

  if (sym != NULL &&
      (scp->is_local_to_function
#if DO_IL_LOWERING
       || (sym->kind == (a_symbol_kind)sk_variable &&
           sym->variant.variable.ptr->promoted_local_static)
#endif /* DO_IL_LOWERING */
                                                            )) {
    if (sym->kind == (a_symbol_kind)sk_constant) {
      if (is_enum_constant(sym->variant.constant)) {
        /* This is an enumerator constant.  The constant itself never appears
           to ABI consumers (only its value as a template argument), but for
           the purpose of generating C code, we do need to ensure uniqueness.
           To that end, use the declaration sequence number of the
           enumerator. */
        discriminator = sym->decl_seq;
      } else {
        /* No discriminators are computed for local constants. */
        goto done;
      }  /* if */
    } else if (sym->kind == (a_symbol_kind)sk_variable) {
      discriminator = sym->variant.variable.discriminator;
    } else if (is_class_struct_union_symbol(sym) &&
               sym->variant.class_struct_union.extra_info != NULL) {
      discriminator = sym->variant.class_struct_union.extra_info
                         ->discriminator;
    } else if (sym->kind == (a_symbol_kind)sk_enum_tag) {
      discriminator = sym->variant.enumeration.extra_info->discriminator;
    } else if (sym->kind == (a_symbol_kind)sk_type) {
      discriminator = sym->variant.type.discriminator;
    }  /* if */
    add_discriminator(discriminator, /*emit_underscore=*/TRUE, mctl);
  }  /* if */
done:;
}  /* add_discriminator_if_necessary */

#endif /* IA64_ABI */

static void parent_for_lambda_in_initializer(a_type_ptr              lambda,
                                             a_source_correspondence **scp,
                                             an_il_entry_kind        *kind)
/*
Returns the variable (if the lambda is in a static data member or variable
template initializer) or the field (if the lambda is in a nonstatic data member
initializer) in which the lambda is defined.  The variable or field is used as
a "pseudo-parent" for the lambda for mangling purposes.  *scp is set to the
source correspondence of the variable or field and *kind is set to iek_variable
or iek_field as appropriate.
*/
{
  a_class_type_supplement_ptr ctsp = class_type_supp(lambda);

  check_assertion(type_is_lambda_in_initializer(lambda));
  if (ctsp->defined_in_field_initializer) {
    *scp = &(class_type_supp(lambda)->lambda_parent.field->source_corresp);
    *kind = iek_field;
  } else {
    check_assertion(ctsp->defined_in_variable_initializer);
    *scp = &(class_type_supp(lambda)->lambda_parent.variable->source_corresp);
    *kind = iek_variable;
  }  /* if */
}  /* parent_for_lambda_in_initializer */


static void mangled_unnamed_type_encoding(a_type_ptr               type,
                                          a_mangling_control_block *mctl)
/*
Generate an encoding for the specified unnamed (class or enum) type.
*/
{
  a_const_char *name;
  check_assertion(is_immediate_class_type(type) ||
                  is_immediate_enum_type(type));
#if IA64_ABI
  if (type_is_lambda_closure(type)) {
    /* An unnamed lambda closure type is mangled as:
         Ul <operator() bare-function-type> E [<nonnegative number>] _ */
    a_type_ptr    call_operator_type;
    call_operator_type = call_operator_function_type_for_lambda(type);
    check_assertion(symbol_supplement_for_class(type) != NULL);
    add_str_to_mangled_name("Ul", mctl);
    mangled_encoding_for_function_type(call_operator_type,
                                       /*do_return_type=*/FALSE,
                                       /*mangling_function_name=*/TRUE,
                                       mctl);
    add_str_to_mangled_name("E", mctl);
    add_discriminator(symbol_supplement_for_class(type)->discriminator,
                      /*emit_underscore=*/FALSE, mctl);
    add_to_mangled_name('_', mctl);
  } else {
    if (unnamed_type_has_no_discriminator(type)) {
      name = give_unnamed_class_or_enum_a_name(type, mctl);
      /* For compiler-generated class/enums, generate an encoding based on the
         unique name that has just been assigned. */
      add_number_to_mangled_name((unsigned long)strlen(name), mctl);
      add_str_to_mangled_name(name, mctl);
    } else {
      /* Unnamed class or enum type (where a discriminator is available).
         These are mangled as: Ut [<nonnegative number>] _ */
      add_str_to_mangled_name("Ut", mctl);
      if (is_immediate_class_type(type)) {
        check_assertion(symbol_supplement_for_class(type) != NULL);
        add_discriminator(symbol_supplement_for_class(type)->discriminator,
                          /*emit_underscore=*/FALSE, mctl);
      } else {
        check_assertion(is_immediate_enum_type(type));
        add_discriminator(symbol_for(type)->variant.enumeration.extra_info->
                                                                 discriminator,
                          /*emit_underscore=*/FALSE, mctl);
      }  /* if */
      add_to_mangled_name('_', mctl);
    }  /* if */
  }  /* if */
#else  /* !IA64_ABI */
  /* In the Cfront ABI, all unnamed types are assigned a name, and then are
     mangled just like other class/struct/union types (i.e., the type name
     prefixed with the length of the type).  The length of the type name is
     added by the caller. */
  name = unmangled_or_fabricated_name_of(&type->source_corresp);
  if (name == NULL) {
    name = give_unnamed_class_or_enum_a_name(type, mctl);
  }  /* if */
  add_str_to_mangled_name(name, mctl);
#endif /* IA64_ABI */
}  /* mangled_unnamed_type_encoding */


static void mangled_encoding_for_class_or_enum_type(
                                                a_type_ptr type,
                                                a_mangling_control_block *mctl)
/*
Add to the mangled name the basic mangled encoding for type (a class or enum
type).  This routine doesn't handle nesting or template parameters (the caller
handles this).  A length is prefixed to named classes in the IA-64 ABI (but
not in the Cfront ABI -- this is the responsibility of the caller and may
often be unknown at the time of the call, requiring a length reservation).
*/
{
  a_const_char *name = unmangled_or_fabricated_name_of(&type->source_corresp);
  check_assertion(is_immediate_class_type(type) ||
                  is_immediate_enum_type(type));
#if !IA64_ABI && GNU_EXTENSIONS_ALLOWED
  if (type->has_gnu_abi_tag_attribute) {
    /* The Cfront ABI adds a prefix to indicate the presence of "abi_tag"
       attributes. */
    add_abi_tag_mangling(type->source_corresp.attributes, mctl);
  }  /* if */
#endif /* !IA64_ABI && GNU_EXTENSIONS_ALLOWED */
  if (name == NULL) {
    /* For an unnamed type, special encodings apply. */
    mangled_unnamed_type_encoding(type, mctl);
  } else {
    /* Use the class name (preceded by its length in the IA-64 ABI). */
#if IA64_ABI
    add_number_to_mangled_name((unsigned long)strlen(name), mctl);
#endif /* IA64_ABI */
    add_str_to_mangled_name(name, mctl);
  }  /* if */
#if IA64_ABI && GNU_EXTENSIONS_ALLOWED
  if (type->has_gnu_abi_tag_attribute) {
    /* The IA-64 ABI adds a suffix to indicate the presence of "abi_tag"
       attributes. */
    add_abi_tag_mangling(type->source_corresp.attributes, mctl);
  }  /* if */
#endif /* IA64_ABI && GNU_EXTENSIONS_ALLOWED */
}  /* mangled_encoding_for_class_or_enum_type */


static void mangled_full_class_name(
                         a_type_ptr               type,
                         ARG_UNUSED a_boolean     show_partial_spec_args,
                         ARG_UNUSED a_boolean     show_template_specialization,
                         ARG_UNUSED a_boolean     show_specialization,
                         a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of the class "type".
This is not the version that contains a leading count of the number
of characters in the name; here, the name is usually just the original
name, but is different if the class is a template class or is unnamed.
Also, this routine does not do anything special with nested types.
show_partial_spec_args is TRUE if template arguments for a partial
specialization should be put out.  show_template_specialization is TRUE
if the class is generated from a specialization of a template and an
indication of that fact should be put out.  show_specialization is TRUE
if the class is itself a specialization and an indication of that fact
should be put out.
*/
{
  a_class_type_supplement_ptr ctsp;
  a_template_arg_ptr          template_args;

  check_assertion(is_immediate_class_type(type));
  ctsp = type->variant.class_struct_union.extra_info;
  check_assertion_str(ctsp != NULL,
                      "mangled_full_class_name: no class type supplement");
  /* See if template arguments are needed.  For partial specializations,
     there are two argument lists. */
  template_args = ctsp->template_arg_list;
  /* Always start with the name of the class, which applies even in the
     template class case. */
  mangled_encoding_for_class_or_enum_type(type, mctl);
#if !IA64_ABI
#if ABI_COMPATIBILITY_VERSION < 241
  /* Before this change, all names included partial specialization
     arguments. */
  show_partial_spec_args = (distinct_template_signatures &&
                            ctsp->partial_spec_template_arg_list != NULL);
#endif /* ABI_COMPATIBILITY_VERSION < 241 */
  if (show_partial_spec_args) {
    /* A partial specialization.  The first list is the argument list
       from the prototype instantiation of the partial specialization.
         template <class T> struct A { ... };
         template <class T> struct A<T *> { ... };
                                     ^^^this argument list
    */
    a_class_symbol_supplement_ptr cssp = symbol_supplement_for_class(type);
    a_class_type_supplement_ptr   proto_ctsp;

    check_assertion(ctsp->partial_spec_template_arg_list != NULL);
    if (type->variant.class_struct_union.is_prototype_instantiation) {
      proto_ctsp = ctsp;
    } else {
      a_symbol_ptr proto_sym = cssp->corresp_prototype_sym;
      a_type_ptr   proto_type = proto_sym->variant.class_struct_union.type;
      proto_ctsp = proto_type->variant.class_struct_union.extra_info;
    }  /* if */
    mangled_template_arguments(proto_ctsp->template_arg_list,
                               /*partial_spec=*/TRUE,
                               /*old_form=*/FALSE,
                               (a_name_reference_ptr)NULL,
                               mctl);
    /* The second argument list is the deduced argument values for the
       template parameter list of the partial specialization. */
    template_args = ctsp->partial_spec_template_arg_list;
  }  /* if */
  if (show_template_specialization) {
    /* Put out an indication of the fact the template from which this
       class is generated is specialized. */
    mangled_specialization_indication(mctl);
  }  /* if */
#endif /* !IA64_ABI */
  if (template_args != NULL) {
    /* A template class.  Add information on template arguments. */
    /* old_form=TRUE forces use of the cfront-compatible mangling convention
       for lengths on literals, which though ambiguous is okay here because
       the class cannot be followed by an "_". */
    a_boolean old_form = !distinct_template_signatures;
#if ABI_COMPATIBILITY_VERSION < 235
    old_form = TRUE;
#endif /* ABI_COMPATIBILITY_VERSION < 235 */
    mangled_template_arguments(template_args,
                               /*partial_spec=*/FALSE,
                               old_form,
                               (a_name_reference_ptr)NULL,
                               mctl);
  }  /* if */
#if !IA64_ABI
  if (show_specialization) {
    /* Put out an indication of the fact that this class is specialized. */
    mangled_specialization_indication(mctl);
  }  /* if */
  /* If the class is a local class, put out a suffix identifying the
     function and the class number. */
  /* Don't do this for nested classes. */
  if (type->source_corresp.is_local_to_function &&
      !type->source_corresp.is_class_member) {
    /* This is a local name. */
    a_class_symbol_supplement_ptr ssp = symbol_supplement_for_class(type);
    a_routine_ptr enclosing_routine = enclosing_routine_for_local_type(type);
    check_assertion(ssp->discriminator != 0);
    add_local_name_suffix(ssp->discriminator, enclosing_routine, mctl);
  }  /* if */
#else /* IA64 */
#if ABI_COMPATIBILITY_VERSION < 401
  /* Earlier versions mistakenly emitted a discriminator at this point
     (rather than at the end of the nested name of a local type). */
  add_discriminator_if_necessary(&type->source_corresp, mctl);
#endif /* ABI_COMPATIBILITY_VERSION < 401 */
#endif /* !IA64_ABI */
}  /* mangled_full_class_name */


#if !IA64_ABI
/*
Interface to mangled_full_class_name for the case where
show_partial_spec_args, show_template_specialization, and show_specialization
are FALSE (meaning no information about those things should be put out).
*/
#define mangled_basic_class_name(type, mctl)                          \
  mangled_full_class_name((type), FALSE, FALSE, FALSE, (mctl))

#endif /* !IA64_ABI */


static void mangled_class_encoding(
                         a_type_ptr               type,
                         a_boolean                show_partial_spec_args,
                         a_boolean                show_template_specialization,
                         a_boolean                show_specialization,
                         a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of the class "type".
This is the version that contains a leading count of the number of
characters in the name, but not information on parents.  If the class
is a proxy class for a template parameter, the encoding for the template
parameter is put out (without a length).  If the class is a template
template parameter with a template argument list, put out an encoding
for that.  show_partial_spec_args is TRUE if template arguments for a
partial specialization should be put out.  show_template_specialization
is TRUE if the class is generated from a specialization of a template
and an indication of that fact should be put out.  show_specialization
is TRUE if the class is itself a specialization and an indication of
that fact should be put out.
*/
{
  a_type_ptr   template_param = NULL;
  a_const_char *name;

  check_assertion(is_immediate_class_type(type));
  if (is_proxy_class(type)) {
    /* This class is the proxy for a template parameter.  Use the encoding
       for the template parameter as the name for the class. */
    template_param = class_type_supp(type)->proxy_of_type;
    if (template_param->kind == (a_type_kind)tk_template_param) {
      switch (template_param->variant.template_param.kind) {
        case tptk_param:
          mangled_encoding_for_template_parameter(
               &template_param->variant.template_param.extra_info->coordinates,
               (a_template_arg *)NULL,
               mctl);
          break;
        case tptk_member:
          /* For something like T::x, where T is a template parameter, just
             put out "x" here. */
          name = unmangled_or_fabricated_name_of(&type->source_corresp);
          if (name == NULL) {
            /* Unnamed classes can appear in prototype instantiations; give
               them a bogus name. */
            check_assertion(type->variant.class_struct_union.is_nonreal_class);
            name = "?";
          }  /* if */
          mangled_name_with_length(name, mctl);
          break;
        case tptk_unknown:
          /* For something like T::x, where T is a template parameter and the
             type of "x" isn't known here, just put out a "?".  This should
             occur only in nonreal classes in configurations that generate
             prototype instantiations. */
          check_assertion(type->variant.class_struct_union.is_nonreal_class);
          mangled_name_with_length("?", mctl);
          break;
        default:
          unexpected_condition_str(
                            "mangled_class_encoding: bad template param kind");
      }  /* switch */
    } else {
      /* It's possible for a proxy class to be a typeref (most likely a
         decltype), so provide an encoding for that.  The caller of this
         routine has already made arrangements for a substitution for the
         type to be recorded, so suppress that (in the IA-64 ABI) when
         a proxy class is being used. */
      check_assertion(template_param->kind == (a_type_kind)tk_typeref);
      mangled_encoding_for_type_full(template_param,
#if ABI_COMPATIBILITY_VERSION >= 406
                                     /*suppress_substititions=*/TRUE,
#else /* ABI_COMPATIBILITY_VERSION < 406 */
                                     /*suppress_substititions=*/FALSE,
#endif /* ABI_COMPATIBILITY_VERSION >= 406 */
                                     mctl);
    }  /* if */
  } else {
    /* Not a proxy for a template parameter. */
    /* See whether this is the proxy for a template template parameter. */
    a_boolean    is_template_template_param = FALSE;
    a_symbol_ptr template_sym = class_template_for_type(type);
    if (template_sym != NULL) {
      a_template_symbol_supplement_ptr tssp =
                                           template_sym->variant.template_info;
      check_assertion(tssp != NULL);
      if (tssp->variant.class_template.template_template_param) {
        /* Yes, this is a template template parameter. */
        is_template_template_param = TRUE;
        mangled_encoding_for_template_parameter(
                                     &tssp->il_template_entry->coordinates,
                                     type->variant.class_struct_union.
                                                 extra_info->template_arg_list,
                                     mctl);
      }  /* if */
    }  /* if */
    if (!is_template_template_param) {
      /* Not a template template parameter. */
      /* Put out the class name preceded by its length. */
#if !IA64_ABI
      a_length_reservation length_reservation;
      reserve_space_for_length(&length_reservation, mctl);
#endif /* !IA64_ABI */
      mangled_full_class_name(type,
                              show_partial_spec_args,
                              show_template_specialization,
                              show_specialization,
                              mctl);
#if !IA64_ABI
      fill_in_length(&length_reservation, mctl);
#endif /* !IA64_ABI */
    }  /* if */
  }  /* if */
}  /* mangled_class_encoding */

#if !IA64_ABI

static void add_nesting_level_encoding(unsigned long            nesting_level,
                                       a_mangling_control_block *mctl)
/*
Add to the mangled name the "Q" encoding (Cfront-like ABI) that
indicates the beginning of a qualified name.  nesting_level is the
number of levels in the qualified name.
*/
{
  /* This uses the ARM (7.2.1c) encoding for nested class names, like
     "outer::inner", using a "Q" description:
       Q2_5outer5inner
          ^-----^-----mangled class names, outer to inner
        ^----count of levels of qualification
     Note that the ARM description does not include the underscore, which
     is necessary if you allow more than 9 levels of nesting.
     The same scheme is used for namespace names. */
  add_to_mangled_name('Q', mctl);
  add_number_to_mangled_name(nesting_level, mctl);
  add_to_mangled_name('_', mctl);
}  /* add_nesting_level_encoding */


static a_boolean parents_have_partial_spec_args(a_type_ptr type)
/*
Return TRUE if any of the parents of the indicated type is a class
with partial specialization arguments.
*/
{
  a_boolean has_partial_spec_args = FALSE;

  if (type->source_corresp.is_class_member) {
    a_type_ptr parent_type = parent_class_of(type);
    a_class_type_supplement_ptr
               ctsp = class_type_supp(parent_type);
    if (ctsp->partial_spec_template_arg_list != NULL) {
      has_partial_spec_args = TRUE;
    } else {
      has_partial_spec_args = parents_have_partial_spec_args(parent_type);
    }  /* if */
  }  /* if */
  return has_partial_spec_args;
}  /* parents_have_partial_spec_args */


/*
The prefix put on the front of the type encoding for a nested type to get
the name placed in the nested type itself.  Also used for classes with
template argument lists, and types promoted out of functions.
*/
#define PREFIX_ON_NESTED_TYPE_NAME "__"

#endif /* !IA64_ABI */

static a_boolean entity_needs_parent_qualifier(
                                              a_source_correspondence     *scp,
                                              ARG_UNUSED an_il_entry_kind kind)
/*
Return TRUE if the indicated entity (as identified by scp and kind) needs a
parent qualifier (typically a class, namespace, or scoped enum, but could also
be a static data member or variable template for lambdas in initializers).  In
the IA-64 ABI, lambda closures defined in default arguments of member functions
or functions in a namespace don't need their parent entity (their actual
class/namespace parent is replaced by a reference to a default argument in a
local function).
*/
{
  a_boolean result = FALSE;

  if (((scp_is_class_or_namespace_member(scp)
#if IA64_ABI
        && !(kind == (an_il_entry_kind)iek_type &&
             mangle_as_lambda_in_default_argument((a_type_ptr)scp))
#endif /* IA64_ABI */
                                                                     ) ||
    scp_is_enum_member(scp))
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
    && !((kind == (an_il_entry_kind)iek_type) &&
         ((a_type *)scp)->use_cfront_transitional_nested_type_name_mangling)
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
                                                                            ) {
    result = TRUE;
  } else if (kind == (an_il_entry_kind)iek_type &&
             type_is_lambda_in_initializer((a_type_ptr)scp)) {
    /* A lambda that occurs in an initializer is mangled as though the entity
       being initialized is its parent.  For example:
         template <class T> int V = []{ return 0; }();
       the mangled name is effectively V::<lambda> to differentiate it from
       other lambdas. */
    result = TRUE;
  }  /* if */
  return result;
}  /* entity_needs_parent_qualifier */


static a_boolean entity_needs_to_be_individuated(a_source_correspondence *scp,
                                                 an_il_entry_kind        kind)
/*
Returns TRUE if the specified entity (as indicated by scp and kind) needs to
have an "individuated" mangled name.  Such names are needed to prevent
collisions of similarly named (or unnamed) entities in multiple translation
units.  This can occur when an unnamed type (or named type of a static
function) is used as a template type argument.  Unnamed types that are class
members or local to a function are already mangled in a unique manner (relative
to their class or function -- this includes lambdas defined in default
arguments or initializers).  Names of routines and/or variables may need to
be individuated in late-specified return types as well.
*/
{
  a_boolean result = FALSE;

  if (local_types_as_template_args_enabled) {
    if (kind == iek_type) {
      if (((is_immediate_class_type((a_type_ptr)scp) &&
            scp->assoc_info != NULL &&
            symbol_supplement_for_class((a_type_ptr)scp) != NULL &&
            !is_proxy_class((a_type_ptr)scp)) ||
           is_immediate_enum_type((a_type_ptr)scp)) &&
          type_is_unnamed((a_type_ptr)scp) &&
          !scp->is_class_member &&
          !(scp->is_local_to_function ||
            mangle_as_lambda_in_default_argument((a_type_ptr)scp))) {
        /* An unnamed type that isn't specific to a class or function is
           individuated.  Lambdas mangled as default arguments are mangled
           as though they are local (so the function mangling serves to
           make them unique). */
        result = TRUE;
      }  /* if */
      if (type_is_lambda_closure((a_type_ptr)scp)) {
        a_class_symbol_supplement_ptr cssp;
        cssp = symbol_supplement_for_class((a_type_ptr)scp);
        check_assertion(cssp != NULL);
        if (cssp->lambda_subject_to_trans_unit_corresp) {
          /* The front end has determined that this lambda is used in a context
             where the closure type must be reproducible across translation
             units -- don't individuate. */
          result = FALSE;
        }  /* if */
      }  /* if */
    } else if (kind == iek_routine &&
               !scp->is_class_member &&
               ((a_routine_ptr)scp)->storage_class ==
                                                     (a_storage_class)sc_static
#if DO_IL_LOWERING
                && !routine_should_be_externalized_for_exported_templates(
                                                            (a_routine_ptr)scp)
#endif /* DO_IL_LOWERING */
                                                                            ) {
      /* Static functions may contain local types that are mangled relative to
         the function name, so the function name must be individuated to
         prevent possible name collisions. */
      result = TRUE;
#if ABI_COMPATIBILITY_VERSION >= 402
    } else if (kind == iek_variable &&
               !(scp->is_local_to_function || scp->is_class_member) &&
               ((a_variable_ptr)scp)->storage_class ==
                                                  (a_storage_class)sc_static) {
      /* Static variable names can appear in decltype expressions and need
         to be individuated (unless they are already unique because they are
         local to a function or appear in a class). */
      result = TRUE;
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
    }  /* if */
    if (!result && scp->is_class_member
#if ABI_COMPATIBILITY_VERSION >= 402
        && (kind != iek_field ||
            class_type_supp(scp_parent_class(scp))->anonymous_union_kind ==
                                          (an_anonymous_union_kind)auk_none)
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
                                                                            ) {
      /* If we haven't determined yet if the entity needs to be individuated
         and the entity is a class member, recurse to see if the parent needs
         to be individuated.  Fields of anonymous unions do not need to be
         individuated. */
      result = entity_needs_to_be_individuated(
                                        &scp_parent_class(scp)->source_corresp,
                                        iek_type);
    }  /* if */
  }  /* if */
  return result;
}  /* entity_needs_to_be_individuated */


static a_boolean ttt_type_needs_to_be_individuated(a_type_ptr tp,
                                                   a_boolean  *end_traversal)
/*
Return TRUE (and stop the type traversal) if the specified type (tp)
needs to be individuated.
*/
{
  a_boolean result = FALSE;

  if (entity_needs_to_be_individuated(&tp->source_corresp, iek_type)) {
    result = TRUE;
    *end_traversal = TRUE;
  }  /* if */
  return result;
}  /* ttt_type_needs_to_be_individuated */


a_boolean exception_specification_contains_an_individuated_entity(
                                                       a_type_ptr routine_type)
/*
Returns TRUE if any of the types in the specified routine type's exception
specification need to be individuated.
*/
{
  a_type_tree_traversal_flag_set  tt_flags = TTT_EXCEPTION_SPECS |
                                             TTT_SKIP_TYPEREFS;
  return traverse_type_tree(routine_type, ttt_type_needs_to_be_individuated,
                            tt_flags);
}  /* exception_specification_contains_an_individuated_entity */


a_boolean routine_contains_an_individuated_entity(a_routine_ptr routine)
/*
Returns TRUE if the specified routine needs to be individuated, or contains
any components that need to be individuated.
*/
{
  a_boolean result;

  if (entity_needs_to_be_individuated(&routine->source_corresp, iek_routine)) {
    result = TRUE;
  } else {
    /* Do a type traversal to see if any elements of the type need to be
       individuated. */
    a_type_tree_traversal_flag_set  tt_flags = TTT_RETURN_TYPE |
                                               TTT_PARAM_TYPES |
                                               TTT_THIS_PARAM_TYPE |
                                               TTT_TEMPLATE_ARGS |
                                               TTT_EXCEPTION_SPECS |
                                               TTT_SKIP_TYPEREFS;
    result = traverse_type_tree(routine->type,
                                ttt_type_needs_to_be_individuated, tt_flags);
  }  /* if */
  return result;
}  /* routine_contains_an_individuated_entity */

#if !IA64_ABI
/*
Macro used to test whether an entity's name has been mangled, but not
yet finalized (assuming finalization is necessary).
*/
#define name_has_been_mangled_but_not_finalized(scp) \
  ((scp)->name_has_been_mangled &&                   \
   (!final_name_mangling_needed ||                   \
    (scp)->final_name_mangling_pending))
#endif /* !IA64_ABI */

static a_namespace_ptr make_individuated_namespace(
                                                a_source_correspondence  *scp,
                                                a_mangling_control_block *mctl)
/*
Return a pointer to a dummy namespace used for mangling of entities that
need to be individuated.  These entities are mangled "as if" they were part
of a top-level namespace whose name is the concatenation of "_INTERNAL"
(or "__INTERNAL" for the Cfront ABI) and the module id.  This is an EDG
extension.  scp is the source correspondence of (a component) of the entity and
is used to generate the correct module id for the entity.  In cases where
the module id has not yet been determined, the dummy namespace is unnamed
and mctl->lacking_module_id is set to TRUE (signaling that any mangled names
that use this namespace should be discarded).  A subsequent call will give the
namespace an appropriate name once the module id is chosen.
*/
{
  STATIC_THREAD a_namespace_ptr nsp;
  a_translation_unit_ptr tup;

  /* Each translation unit is individuated with a different name, make
     sure we use the correct one. */
  tup = (scp->assoc_info != NULL) ? trans_unit_for_source_corresp(scp) :
                                    curr_translation_unit;
  nsp = tup->individuated_namespace;
  if (nsp == NULL) {
    /* Allocate the individuated namespace for this translation unit. */
    nsp = alloc_fe_of_type(a_namespace);
    clear_namespace(nsp, /*is_alias=*/FALSE);
    tup->individuated_namespace = nsp;
  }  /* if */
  if (nsp->source_corresp.name == NULL) {
    char         *name;
    a_const_char *module_id = module_id_for_source_corresp(scp, mctl);
    /* Namespace needs a real name, see if we can give it one. */
    if (!mctl->lacking_module_id) {
      /* Construct a real name for the namespace. */
#if IA64_ABI
      name = (char *)alloc_fe(strlen(module_id)+1+9);
      (void)strcpy(name, "_INTERNAL");
#else /* !IA64_ABI */
      name = (char *)alloc_fe(strlen(module_id)+1+10);
      (void)strcpy(name, "__INTERNAL");
#endif /* IA64_ABI */
      (void)strcat(name, module_id);
      nsp->source_corresp.name = name;
    }  /* if */
  }  /* if */
  return tup->individuated_namespace;
}  /* make_individuated_namespace */


static void r_mangled_parent_qualifier(
                             a_source_correspondence  *scp,
                             an_il_entry_kind         kind,
                             unsigned long            nesting_level,
                             a_boolean                needs_to_be_individuated,
                             a_source_correspondence  **discriminator_scp,
                             a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the parent qualifier needed in the
mangled name for an entity whose source correspondence is pointed to by scp and
whose kind is given by "kind".  Typically, scp is a member of a class,
namespace member, or scoped enumerator, but when needs_to_be_individuated is
TRUE, the entity can be a type or routine.  nesting_level is used to track
recursive calls of this routine to deal with multiple levels of parents.
nesting_level == 1 refers to the innermost qualifier of a type, nesting_level
== 2 is the next level out, etc.  The value is not used in the IA-64 ABI.
needs_to_be_individuated is TRUE when then entity needs to be "individuated",
that is, mangled "as if" the entity were part of a top-level namespace with a
unique name.  See the macro mangled_parent_qualifier, which supplies the usual
nesting_level == 1 and needs_to_be_individuated == FALSE.  If discriminator_scp
is not NULL, *discriminator_scp is set to the source correspondence of the
entity to be used in the IA-64 ABI as a discriminator (if necessary) if the
entity is local to a function.  Typically this is the topmost entity, unless
the topmost entity is an unnamed type in which case the discriminator is used
in differentiating the unnamed type (and *discriminator_scp is set to NULL).

For most cases, the parent entity as specified by the IL is used for
mangling purposes, but in two special cases the parent processing is
non-standard.  In the first case (needs_to_be_individuated == TRUE), a
top-level namespace is used as a parent entity (see above).  In the case
of a lambda defined in the initializer list of a static data member or variable
template, the static data member or variable template is used as the parent
entity for mangling purposes.
*/
{
  a_type_ptr              type = NULL;
  a_class_type_supplement_ptr
                          ctsp = NULL;
  a_source_correspondence *parent_scp;
  an_il_entry_kind        parent_kind;
  a_boolean               more_levels;
  a_boolean               show_partial_spec_args = FALSE;
  a_boolean               is_template_specialization = FALSE;
  a_boolean               is_specialization = FALSE;
  a_boolean               use_individuated_namespace = FALSE;
  a_const_char            *name;

  if (kind == iek_type &&
      active_parents->map_or_replace(scp, true)) {
    /* Add this type to the list of parent entities we've seen while mangling
       the original child.  If the type is already in the map, then we're in
       an unbounded loop.  That can happen for cases like:
         auto x = [](decltype([]{}) y) { return y; };
       Here, the type for the parameter in the outer lambda depends on the type
       of the inner lambda, but the inner lambda type uses its parent's type
       to disambiguate it. */
#if EXPENSIVE_CHECKING && IA64_ABI && ABI_COMPATIBILITY_VERSION >= 405
    /* This can cause issues with the IA-64 ABI substitution scheme, so skip
       the check that is performed in certain configurations. */
    skip_substitution_check = TRUE;
#endif /* EXPENSIVE_CHECKING && IA64_ABI && ABI_COMPATIBILITY_VERSION >= 405 */
    goto done_no_unmap;
  }  /* if */
  /* See if the present level is nested inside some other level (class,
     scoped enum, or namespace), or is logically nested inside some other
     entity (lambdas in initializers, individuated entities) for the purposes
     of mangling. */
  if (kind == iek_type &&
      type_is_lambda_in_initializer((a_type_ptr)scp)) {
    /* This lambda closure was defined in an initializer for a data member or
       variable template.  Use the data member or variable template as its
       "parent" for mangling purposes. */
    parent_for_lambda_in_initializer((a_type_ptr)scp, &parent_scp,
                                     &parent_kind);
    if (parent_kind == iek_variable &&
        ((a_variable*)parent_scp)->is_struct_binding_container) {
      /* This lambda appears in the initialization portion of a decomposition
         declaration.  For lambdas appearing in initializers, the variable
         is used as the "parent" for mangling purposes, but in this case the
         "parent" (i.e., structured binding container) is unnamed.  Use the
         first binding instead. */
      parent_scp = (a_source_correspondence_ptr)
                       ((a_variable*)parent_scp)->variant.bindings->entity.ptr;
    }  /* if */
    more_levels = entity_needs_parent_qualifier(parent_scp, parent_kind);
  } else if (scp->is_class_member) {
    /* Class member. */
    type = scp_parent_class(scp);
    ctsp = class_type_supp(type);
#if CHECKING
    if (!class_type_has_body(type) &&
        !type->variant.class_struct_union.is_nonreal_class) {
#if DEBUG
/* GCOVR_EXCL_START */
      (void)fprintf(f_debug, "Parent class = ");
      db_abbr_type(type);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
      unexpected_condition_str(
                       "r_mangled_parent_qualifier: parent class has no body");
    }  /* if */
#endif /* CHECKING */
#if !IA64_ABI
    if (distinct_template_signatures) {
      /* When templates get distinct mangling from normal functions,
         information is included for specialization in parent classes. */
      /* See if the class comes from a template and that template is
         specialized. */
      a_symbol_ptr template_sym =
                             symbol_supplement_for_class(type)->class_template;
      if (template_sym != NULL) {
        /* This class is an instance of a template. */
        if (template_sym->variant.template_info->is_specific_definition) {
          /* The template is specialized. */
          is_template_specialization = TRUE;
        }  /* if */
      }  /* if */
      /* See if the class itself is specialized (but not with the old
         syntax). */
      if (type->variant.class_struct_union.is_specialized &&
          !type->variant.class_struct_union.specialized_with_old_syntax) {
        is_specialization = TRUE;
      }  /* if */
      /* See if the class is a partial specialization. */
      if (ctsp->partial_spec_template_arg_list != NULL &&
          !mctl->suppress_partial_spec_args) {
        show_partial_spec_args = TRUE;
      }  /* if */
    }  /* if */
    if (name_has_been_mangled_but_not_finalized(&type->source_corresp) &&
        !type->source_corresp.unnamed_entity_given_fabricated_name &&
        !show_partial_spec_args &&
        !is_template_specialization &&
        !is_specialization &&
        (!mctl->suppress_partial_spec_args ||
         !parents_have_partial_spec_args(type))) {
      /* The form of the name needed matches the form saved by
         mangle_type_name, so use the saved form.  This includes all
         the parents of the type as well.  Note that the tricky test here
         is that we only include specialization information in parents, so
         the saved version has no specialization information in the final
         component of the name.  If there wouldn't be any of that information
         anyway (e.g., because the final component is not a partial
         specialization), the final component matches what we want.  The
         other components are usually okay, because they're generated as
         parent qualifiers and include the specialization information if
         appropriate (but we can't use them if we're in a context where
         all partial specialization information is suppressed, unless
         there are no partial specializations involved). */
      /* Find the nested type name prefix if present, determine the existing
         nesting level and construct the correct "Q" qualifier with the right
         total nesting level. */
      unsigned long type_nesting_level = 0;
      name = type->source_corresp.name;
      /* Skip the prefix. */
      check_assertion(strncmp(name,
                              PREFIX_ON_NESTED_TYPE_NAME,
                              sizeof(PREFIX_ON_NESTED_TYPE_NAME)-1) == 0);
      name += sizeof(PREFIX_ON_NESTED_TYPE_NAME) - 1;
      if (entity_needs_parent_qualifier(&type->source_corresp, iek_type) ||
          entity_needs_to_be_individuated(&type->source_corresp, iek_type)) {
        check_assertion(*name == 'Q');
        /* Skip the "Q". */
        name++;
        /* Determine the nesting level of the existing parent type. */
        while (isdigit((unsigned char)*name)) {
          type_nesting_level = type_nesting_level * 10 + (*name - '0');
          name++;
        }  /* while */
        type_nesting_level--;
        check_assertion(*name == '_');
        name++;
      }  /* if */
      nesting_level += type_nesting_level;
      if (nesting_level > 1) {
        /* Output the "Q" qualifier with the correct (total) nesting level. */
        add_nesting_level_encoding(nesting_level, mctl);
      }  /* if */
      /* Copy the previously mangled name to the output. */
      add_str_to_mangled_name(name, mctl);
      goto done;
    }  /* if */
#endif /* !IA64_ABI */
    parent_scp = &type->source_corresp;
    parent_kind = iek_type;
    more_levels = entity_needs_parent_qualifier(&type->source_corresp,
                                                iek_type);
  } else if (scp_is_enum_member(scp)) {
    /* Scoped enumerator. */
    type = scp_parent_scoped_enum_type(scp);
    parent_scp = &type->source_corresp;
    parent_kind = iek_type;
    more_levels = entity_needs_parent_qualifier(&type->source_corresp,
                                                iek_type);
  } else if (scp_is_namespace_member(scp)) {
    /* Namespace member. */
    parent_scp = &scp_parent_namespace(scp)->source_corresp;
    parent_kind = iek_namespace;
    more_levels = scp_is_namespace_member(parent_scp);
  } else {
    /* A topmost entity needing individuation. */
    check_assertion(needs_to_be_individuated);
    more_levels = FALSE;
    parent_scp = NULL;
    parent_kind = iek_none;
    use_individuated_namespace = TRUE;
  }  /* if */
  if (!use_individuated_namespace &&
      !more_levels &&
      needs_to_be_individuated) {
    /* This is the topmost namespace/class (but not the individuated
       namespace), and the entity needs to be individuated, so recurse once
       more to add the individuated namespace. */
    more_levels = TRUE;
  }  /* if */
  if (!more_levels && discriminator_scp != NULL) {
#if IA64_ABI
    if (type != NULL && is_self_discriminated_type(type)) {
      /* If the topmost entity is an unnamed type, don't bother setting
         discriminator_scp (the discriminator will be emitted as part of the
         unnamed type rather than as part of the local-name discriminator). */
      *discriminator_scp = NULL;
    } else
#endif /* IA64_ABI */
    /* Do not insert code here. */
    {
      /* No more levels, return the source correspondence of the top most
         level. */
      *discriminator_scp = parent_scp;
    }  /* if */
  }  /* if */
#if !IA64_ABI
  if (more_levels) {
    /* This level is nested inside something else.  Do a recursive call
       to put out all of the parents. */
    r_mangled_parent_qualifier(parent_scp, parent_kind, nesting_level + 1,
                               needs_to_be_individuated, discriminator_scp,
                               mctl);
  } else {
    /* This is the topmost qualifier. */
    if (nesting_level > 1) {
      add_nesting_level_encoding(nesting_level, mctl);
    }  /* if */
  }  /* if */
#endif /* !IA64_ABI */
  /* Now that we've identified the proper parent (actual or logical)
     for the specified entity, emit the proper encoding for the parent. */
  if (kind == iek_type &&
      type_is_lambda_in_initializer((a_type_ptr)scp)) {
    /* Lambda in initializer list.  Mangle as though the variable template
       or static data member is the "parent" of the lambda. */
#if IA64_ABI
    a_boolean variable_template_case = FALSE;
    if (add_substitution_if_available((char *)parent_scp, parent_kind,
                                      /*is_pack_expansion=*/FALSE, mctl)) {
      goto done;
    } else {
      if (more_levels) {
        /* This level is nested inside something else.  Do a recursive call to
           deal with all of the parents. */
        r_mangled_parent_qualifier(parent_scp, parent_kind, nesting_level + 1,
                                   needs_to_be_individuated, discriminator_scp,
                                   mctl);
      }  /* if */
    }  /* if */
    mangled_name_with_length(unmangled_or_fabricated_name_of(parent_scp),
                             mctl);
    /* Add a substitution for this variable/field. */
    alloc_substitution((char *)parent_scp, parent_kind,
                       /*is_pack_expansion=*/FALSE, mctl);
    if (parent_kind == iek_variable) {
      a_variable_ptr vp = (a_variable_ptr)parent_scp;
      if (vp->is_template_variable &&
          vp->template_info->template_arg_list != NULL) {
        /* Mangle the template arguments for a variable template. */
        add_variable_template_indication(vp, mctl);
        variable_template_case = TRUE;
      }  /* if */
    }  /* if */
    if (!variable_template_case) {
      /* Mangling for lambda in static data initializer. */
      add_to_mangled_name('M', mctl);
    }  /* if */
#else /* !IA64_ABI */
    a_length_reservation  length_reservation;
    reserve_space_for_length(&length_reservation, mctl);
    add_str_to_mangled_name(unmangled_or_fabricated_name_of(parent_scp), mctl);
    if (parent_kind == iek_variable) {
      add_variable_template_indication((a_variable_ptr)parent_scp, mctl);
    }  /* if */
    fill_in_length(&length_reservation, mctl);
#endif /* IA64_ABI */
  } else if (scp->is_class_member) {
    /* Class name. */
#if IA64_ABI
    if (add_substitution_if_available((char *)type, iek_type,
                                      /*is_pack_expansion=*/FALSE, mctl)) {
      goto done;
    } else {
      a_template_ptr              tmpl;
      tmpl = class_template_of(type);
      if (tmpl != NULL &&
          add_substitution_if_available((char *)tmpl, iek_template,
                                        /*is_pack_expansion=*/FALSE, mctl)) {
        mangled_template_arguments(ctsp->template_arg_list,
                                   /*partial_spec=*/FALSE,
                                   /*old_form=*/FALSE,
                                   (a_name_reference_ptr)NULL,
                                   mctl);
        goto new_substitution;
      }  /* if */
      if (more_levels) {
        /* This level is nested inside something else.  Do a recursive call to
           deal with all of the parents. */
        r_mangled_parent_qualifier(parent_scp, parent_kind, nesting_level + 1,
                                   needs_to_be_individuated, discriminator_scp,
                                   mctl);
      }  /* if */
      if (tmpl != NULL) {
        alloc_substitution((char *)tmpl, iek_template,
                           /*is_pack_expansion=*/FALSE, mctl);
      }  /* if */
    }  /* if */
    if (emulate_gnu_abi_bugs && gnu_abi_version < 30400) {
      /* g++ versions prior to 3.4.0 had a bug with template parameters as
         parents: they used the parameter name instead of a template parameter
         encoding. */
      mangled_full_class_name(type,
                              show_partial_spec_args,
                              is_template_specialization,
                              is_specialization,
                              mctl);
      goto new_substitution;
    }  /* if */
#endif /* IA64_ABI */
    mangled_class_encoding(type,
                           show_partial_spec_args,
                           is_template_specialization,
                           is_specialization,
                           mctl);
#if IA64_ABI
new_substitution:
    /* Add a substitution for this type. */
    alloc_substitution((char *)type, iek_type,
                       /*is_pack_expansion=*/FALSE, mctl);
#endif /* IA64_ABI */
  } else if (scp_is_enum_member(scp)) {
    /* Scoped enumerator. */
#if IA64_ABI
    check_assertion(type != NULL);
    if (add_substitution_if_available((char *)type, iek_type,
                                      /*is_pack_expansion=*/FALSE, mctl)) {
      goto done;
    } else {
      if (more_levels) {
        /* This level is nested inside something else.  Do a recursive call to
           deal with all of the parents. */
        r_mangled_parent_qualifier(parent_scp, parent_kind, nesting_level + 1,
                                   needs_to_be_individuated, discriminator_scp,
                                   mctl);
      }  /* if */
    }  /* if */
    mangled_encoding_for_class_or_enum_type(type, mctl);
    /* Add a substitution for this type. */
    alloc_substitution((char *)type, iek_type,
                       /*is_pack_expansion=*/FALSE, mctl);
#else /* !IA64_ABI */
    a_length_reservation  length_reservation;
    /* Put out the enum name (and optional unique indicator) along with its
       length. */
    reserve_space_for_length(&length_reservation, mctl);
    mangled_encoding_for_class_or_enum_type(type, mctl);
    if (scp->is_local_to_function && !type->source_corresp.is_class_member) {
      /* Use the discriminator computed by the front end to make a local
         scoped enumerator unique. */
      a_routine_ptr enclosing_routine = enclosing_routine_for_local_type(type);
      an_enum_symbol_supplement_ptr ssp = symbol_for(type)->
                                                variant.enumeration.extra_info;
      check_assertion(ssp->discriminator != 0);
      add_local_name_suffix(ssp->discriminator, enclosing_routine, mctl);
    }  /* if */
    fill_in_length(&length_reservation, mctl);
#endif /* IA64_ABI */
  } else if (scp_is_namespace_member(scp) ||
             use_individuated_namespace) {
    /* Namespace or "individuated" namespace name. */
    a_namespace_ptr nsp;
    if (use_individuated_namespace) {
      /* Use a dummy namespace pointer for all individuated namespace
         manglings.  Having a namespace pointer ensures that substitutions
         (for the IA-64 ABI) will be handled properly. */
      nsp = make_individuated_namespace(scp, mctl);
      if (mctl->lacking_module_id) {
        /* If the individuated namespace doesn't yet have a name, no
           sense in continuing. */
        goto done;
      }  /* if */
    } else {
      nsp = scp_parent_namespace_or_null(scp);
    }  /* if */
#if IA64_ABI
    if (needs_to_be_individuated &&
        substitution_available((char *)nsp, iek_namespace,
                               /*is_pack_expansion=*/FALSE, mctl) &&
        is_namespace_std(nsp)) {
      /* This is an entity in the std namespace that needs to be individuated
         (e.g., "namespace std { enum {} e; }").  The "St" substitution is
         typically used to mangle this, but we need to add the individuated
         namespace first before that substitution is performed below. */
      a_namespace_ptr insp = make_individuated_namespace(scp, mctl);
      if (mctl->lacking_module_id) {
        /* If the individuated namespace doesn't yet have a name, no
           sense in continuing. */
        goto done;
      }  /* if */
      mangled_name_with_length(unmangled_or_fabricated_name_of(
                                                        &insp->source_corresp),
                               mctl);
    }  /* if */
    if (add_substitution_if_available((char *)nsp, iek_namespace,
                                      /*is_pack_expansion=*/FALSE, mctl)) {
      goto done;
    } else if (more_levels) {
      /* This level is nested inside something else.  Do a recursive call to
         deal with all of the parents. */
      r_mangled_parent_qualifier(parent_scp, parent_kind, nesting_level + 1,
                                 needs_to_be_individuated, discriminator_scp,
                                 mctl);
    }  /* if */
#endif /* IA64_ABI */
    name = unmangled_or_fabricated_name_of(&nsp->source_corresp);
    if (name == NULL) {
      /* For an unnamed namespace, generate a name (or use the name previously
         generated). */
      name = give_unnamed_namespace_a_name(nsp, mctl);
    }  /* if */
    /* Put out the namespace name preceded by the length of the name, e.g.,
       "NNN" --> "3NNN". */
    mangled_name_with_length(name, mctl);
#if IA64_ABI
    /* Add a substitution for this namespace. */
    alloc_substitution((char *)nsp, iek_namespace, /*is_pack_expansion=*/FALSE,
                       mctl);
#endif /* IA64_ABI */
  }  /* if */
done:
  /* Remove entry for the current entity. */
  if (kind == iek_type) active_parents->unmap(scp);
done_no_unmap:;
}  /* r_mangled_parent_qualifier */

#if IA64_ABI

static void mangled_ia64_parent_qualifier(
                              a_source_correspondence  *scp,
                              an_il_entry_kind         kind,
                              a_boolean                *need_nested_name_close,
                              a_source_correspondence  **discriminator_scp,
                              a_boolean                force_individuation,
                              a_mangling_control_block *mctl)
/*
Add to the IA-64 mangled name the encoding for a parent qualifier if
one is needed in the mangled name for the entity whose source
correspondence is pointed to by scp and whose kind is given by "kind".
*need_nested_name_close is returned TRUE if a nested name has been
started and must be closed later.  This routine is used at the top
level for a complete name, and not recursively for each level of the
parent qualifiers.  It also handles the qualifier for local
entities that indicates the enclosing function.  When a qualifier has been
added for a local entity, return (in *discriminator_scp) the source
correspondence of the entity that should be used when emitting a
discriminator (if necessary).  When force_individuation is TRUE, the caller
is requesting that the entity be individuated (presumably because it's a
static entity being used in a manner that requires differentiation from
similarly named entities in other translation units); typically that
determination is made by the callee.
*/
{
  a_type_ptr        local_type = NULL;
  a_boolean         needs_to_be_individuated;

  check_assertion(discriminator_scp != NULL);
  if (force_individuation) {
    needs_to_be_individuated = TRUE;
  } else {
    needs_to_be_individuated = entity_needs_to_be_individuated(scp, kind);
  }  /* if */
  *discriminator_scp = NULL;
  *need_nested_name_close = FALSE;
  /* For entities defined in a type that is local to a function (or mangled
     as such -- see below for lambdas in default arguments), a special
     encoding is used to indicate such. */
  /* Lambdas defined in default arguments are mangled as though they are
     local to the function in whose declaration they are defined.  The
     is_local_to_function flag is not set in the IL, so this case is handled
     specially here.  Note that lambdas defined in a declaration at function
     scope are mangled as though they appear at function scope (and not mangled
     as described above). */
  if (kind == iek_type &&
      mangle_as_lambda_in_default_argument((a_type_ptr)scp)) {
    local_type = (a_type_ptr)scp;
  } else if (scp->is_class_member &&
             mangle_as_lambda_in_default_argument(
                                        (a_type_ptr)scp_parent_class(scp))) {
    /* Lambdas appearing in default arguments don't have captured entities, so
       a nested class can't appear here, but the lambda can have members
       (e.g., operator()).  Any types local to those functions are mangled
       separately (within that function). */
    local_type = (a_type_ptr)scp_parent_class(scp);
  }  /* if */ 
  if (local_type != NULL) {
    /* Encode as a local type with default argument information. */
    add_mangling_for_default_arg_in_local_type(local_type,
                                               (a_routine_ptr*)NULL, mctl);
  } else if (scp->is_local_to_function) {
    /* Add the encoding for the function if this is a local type,
       member of a local class, or a scoped enumerator. */
    if (kind == iek_type) {
      local_type = (a_type_ptr)scp;
    } else if (scp->is_class_member) {
      local_type = (a_type_ptr)scp_parent_class(scp);
    } else if (scp_is_enum_member(scp)) {
      local_type = (a_type_ptr)scp_parent_scoped_enum_type(scp);
    }  /* if */
    if (local_type != NULL) {
      check_assertion(!needs_to_be_individuated);
      add_prefix_for_local_type(local_type, mctl);
      if (!is_self_discriminated_type(local_type) &&
          kind != iek_routine) {
        /* For local entities, use a discriminator to differentiate entities
           that might otherwise have the same name (i.e., because they are
           declared in separate blocks).  Use scp as the entity whose
           discriminator will be used, but note that this may be overwritten
           below if the entity needs qualification.  Unnamed types are
           already discriminated so they aren't further discriminated here.
           Routines (only member functions here) don't have discriminators
           (they rely on their parent class to be discriminated). */
        *discriminator_scp = scp;
      }  /* if */
    }  /* if */
  }  /* if */
  if (is_source_corresp_in_namespace_std(scp)) {
    /* Special encoding for "std::".*/
    if (needs_to_be_individuated) {
      /* This is an entity in the std namespace that needs to be individuated
         (e.g., "namespace std { enum {} e; }").  The "St" substitution is
         typically used to mangle this, but we need to add the individuated
         namespace first before that substitution is performed below. */
      a_namespace_ptr nsp = make_individuated_namespace(scp, mctl);
      if (mctl->lacking_module_id) {
        /* If the individuated namespace doesn't yet have a name, no
           sense in continuing. */
        goto done;
      }  /* if */
      add_to_mangled_name('N', mctl);
      *need_nested_name_close = TRUE;
      mangled_name_with_length(unmangled_or_fabricated_name_of(
                                                         &nsp->source_corresp),
                               mctl);
    }  /* if */
    add_str_to_mangled_name("St", mctl);
  } else if (entity_needs_parent_qualifier(scp, kind) ||
             needs_to_be_individuated) {
    /* The entity is a class member, namespace member, or scoped enumerator
       and needs a parent qualifier or the entity needs to be individuated. */
    /* Mark the start of the nested name. */
    add_to_mangled_name('N', mctl);
    *need_nested_name_close = TRUE;
    if (kind == iek_routine) {
      /* Some type of function.  Put out the qualifiers on the function
         type. */
      a_routine_ptr routine = (a_routine_ptr)scp;
      mangled_encoding_for_function_qualifiers(routine->type,
                                               scp->is_class_member,
                                               mctl);
      if (scp->is_class_member) {
        /* Mangle a ref-qualifier (if present) in a nonstatic member function
           name. */
        mangled_encoding_for_ref_qualifier(skip_typerefs(routine->type), mctl);
      }  /* if */
    }  /* if */
    /* Put out the components of the nested name except for the final one.
       The caller will put out the final name and then close the nested
       name. */
    r_mangled_parent_qualifier(scp, kind, (unsigned long)1,
                               needs_to_be_individuated,
                               discriminator_scp, mctl);
    if (local_type == NULL) {
      /* If we don't have a local type, we don't need a discriminator. */
      *discriminator_scp = NULL;
    }  /* if */
  }  /* if */
done:;
}  /* mangled_ia64_parent_qualifier */


static void close_ia64_nested_name(
                               a_boolean                need_nested_name_close,
                               a_source_correspondence  *discriminator_scp,
                               a_mangling_control_block *mctl)
/*
If need_nested_name_close is TRUE, put out the sequence to close a
nested name in the IA-64 ABI encoding.  If we're closing a nested name
that is an entity in a local scope, discriminator_scp (if non-NULL) will
point to the source correspondence of an entity to be used as a discriminator.
*/
{
  if (need_nested_name_close) {
    add_to_mangled_name('E', mctl);
  }  /* if */
#if ABI_COMPATIBILITY_VERSION >= 401
  if (discriminator_scp != NULL) {
    /* In versions of the ABI prior to 401, the discriminator was mistakenly
       emitted earlier in the mangling. */
    add_discriminator_if_necessary(discriminator_scp, mctl);
  }  /* if */
#endif /* ABI_COMPATIBILITY_VERSION >= 401 */
}  /* close_ia64_nested_name */

#endif /* IA64_ABI */

static void mangled_template_alias_encoding(a_type_ptr               type,
                                            a_mangling_control_block *mctl)
/*
Create a mangled encoding for the template alias specified by type.  The ABI
does not specify a mangling for template aliases as they do not appear in
externally visible mangled names.  Mangle a template alias as though it were a
class template (there should not be a class template with the same name as
an alias template in the same scope).  No demangling changes are required for
the template alias case.  Note that this provides a mangled name for the
template alias itself, but when a template alias appears as part of a
mangled name, the typeref indicating the alias is stripped and the underlying
type is mangled in its place.
*/
{
  a_const_char         *name =
                        unmangled_or_fabricated_name_of(&type->source_corresp);
#if !IA64_ABI
  a_length_reservation length_reservation;
#endif /* !IA64_ABI */

  check_assertion(type->kind == (a_type_kind)tk_typeref &&
                  is_typeref_kind(type, trk_is_template_alias) &&
                  type->variant.typeref.extra_info->template_arg_list != NULL);
#if !IA64_ABI
  reserve_space_for_length(&length_reservation, mctl);
#endif /* !IA64_ABI */
  /* Use the alias name (preceded by its length in the IA-64 ABI). */
#if IA64_ABI
#if ABI_COMPATIBILITY_VERSION >= 411
  /* Allocate a substitution for the template alias. */
  alloc_substitution((char *)type, iek_type, /*is_pack_expansion=*/TRUE, mctl);
#endif /* ABI_COMPATIBILITY_VERSION >= 411 */
  add_number_to_mangled_name((unsigned long)strlen(name), mctl);
#endif /* IA64_ABI */
  add_str_to_mangled_name(name, mctl);
  /* Add the template arguments. */
  mangled_template_arguments(
                           type->variant.typeref.extra_info->template_arg_list,
                           /*partial_spec=*/FALSE,
                           /*old_form=*/FALSE,
                           (a_name_reference_ptr)NULL,
                           mctl);
#if !IA64_ABI
  fill_in_length(&length_reservation, mctl);
#endif /* !IA64_ABI */
}  /* mangled_template_alias_encoding */


static void mangled_type_name_full(a_type_ptr               type,
                                   ARG_UNUSED a_boolean     check_for_subst,
                                   ARG_UNUSED a_boolean     ok_to_mangle_type,
                                   a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of the type "type".
This routine is used for named types (classes, enums, template parameters
members, and typedefs; typedefs come up when doing final name mangling for
nested types) and for unnamed classes and enums.  Nested types are encoded as
such.  ok_to_mangle_type is TRUE if it's okay to give the specified type
a mangled name in the process of adding the encoded type name (Cfront ABI
only).  Setting ok_to_mangle_type to FALSE is a safe value (TRUE enables a
potential performance improvement, allowing re-use of a mangled name).
*/
{
  a_const_char                *name;
#if IA64_ABI
  a_source_correspondence     *discriminator_scp;
  a_template_ptr              tmpl;
  a_class_type_supplement_ptr ctsp;
  a_boolean                   need_nested_name_close = FALSE;
#else /* !IA64_ABI */
  a_length_reservation        length_reservation;
  sizeof_t                    encoding_start = 0, save_num_leftover_spaces = 0;
  a_boolean                   reusable_form;
#endif /* IA64_ABI */

  /* cv-qualifiers are not allowed here. */
  check_assertion(type->kind != (a_type_kind)tk_typeref ||
                  typeref_is_typedef(type));
#if IA64_ABI
  /* Note that we can't use a possibly previously created mangled type
     name in the IA-64 ABI because of the way substitutions are handled
     in that ABI. */
  tmpl = NULL;  
  /* Don't do substitutions for typedefs passed from mangle_type_name. */
  if (type->kind != (a_type_kind)tk_typeref) {
#if ABI_COMPATIBILITY_VERSION >= 303
    if (check_for_subst) {
      /* Check whether a substitution is available for this entire type.
         Do not do this if the caller has already done it. */
      if (add_substitution_if_available((char *)type, iek_type,
                                        /*is_pack_expansion=*/FALSE, mctl)) {
        goto done;
      }  /* if */
    }  /* if */
#endif /* ABI_COMPATIBILITY_VERSION */
    /* Check here to see if the type is an instantiation of a template for
       which a substitution is available. */
    tmpl = class_template_of(type);
    if (tmpl != NULL &&
        /* Test done separately from output to allow the opportunity to
           put out "N...E" below. */
        substitution_available((char *)tmpl, iek_template,
                               /*is_pack_expansion=*/FALSE, mctl)) {
      a_boolean need_close = FALSE;
      if (is_class_or_namespace_member(tmpl) && !is_in_namespace_std(tmpl)) {
        /* The template is nested, so put "N...E" around the substitution
           and template arguments.  The ABI spec is ambiguous about this,
           but g++ 3.2 does it this way, including the special case
           of the "std" namespace. */
        add_to_mangled_name('N', mctl);
        need_close = TRUE;
      }  /* if */
      (void)add_substitution_if_available((char *)tmpl, iek_template,
                                          /*is_pack_expansion=*/FALSE, mctl);
      ctsp = type->variant.class_struct_union.extra_info;
      mangled_template_arguments(ctsp->template_arg_list,
                                 /*partial_spec=*/FALSE,
                                 /*old_form=*/FALSE,
                                 (a_name_reference_ptr)NULL,
                                 mctl);
      if (need_close) add_to_mangled_name('E', mctl);
      goto done;
    }  /* if */
  }  /* if */
  mangled_ia64_parent_qualifier(&type->source_corresp, iek_type,
                                &need_nested_name_close, 
                                &discriminator_scp,
                                /*force_individuation=*/FALSE,
                                mctl);
  if (tmpl != NULL) {
    alloc_substitution((char *)tmpl, iek_template, /*is_pack_expansion=*/FALSE,
                       mctl);
  }  /* if */
#else /* !IA64_ABI */
  /* The mangled name/encoding includes partial specialization arguments on
     parents of the type, so it can be reused only if we want those
     arguments or if there aren't any so it doesn't make a difference. */
  reusable_form = (!mctl->suppress_partial_spec_args ||
                   !parents_have_partial_spec_args(type));
  if (!type->source_corresp.name_has_been_mangled && reusable_form) {
    /* This type doesn't have a mangled name; either it needs a mangled
       name and somehow hasn't been mangled yet (e.g., during pre-lowering of
       a class or promotion of an entity), or it doesn't need a mangled
       name (and will never be given one).  To determine which case we have,
       try to mangle the type name (if possible) and if it produces a mangled
       name, see if we can use that (minus the prefix).  If a mangled name
       isn't produced, continue with the encoding for this type, but mark
       the location in the mangling buffer and keep this type encoding for
       possible later use. */
    if (type->source_corresp.unmangled_name_or_mangled_encoding != NULL) {
      /* This type doesn't have a mangled name, but it does have a mangled
         encoding; use it. */
      add_str_to_mangled_name(
                type->source_corresp.unmangled_name_or_mangled_encoding, mctl);
      goto done;
    }  /* if */
    if (ok_to_mangle_type) {
      /* No mangled name or mangled encoding, try to mangle the type name
         (if it's possible to do so). */
      mangle_type_name(type);
    }  /* if */
    if (!type->source_corresp.name_has_been_mangled) {
      /* This type doesn't need a mangled name; save some context so we can
         pull the encoding for this type out of the mangling buffer. */
      encoding_start = mangling_text_buffer->size;
      save_num_leftover_spaces = mctl->num_leftover_spaces;
      check_assertion(encoding_start != 0);
    }  /* if */
  }  /* if */
  if (name_has_been_mangled_but_not_finalized(&type->source_corresp) &&
      !type->source_corresp.unnamed_entity_given_fabricated_name &&
      reusable_form) {
    /* The type name has been mangled already (in mangle_type_name), so
       reuse the form we already have.  Skip the prefix at the
       beginning of the name. */
    name = type->source_corresp.name;
    check_assertion(name != NULL &&
                    strncmp(name,
                            PREFIX_ON_NESTED_TYPE_NAME,
                            sizeof(PREFIX_ON_NESTED_TYPE_NAME)-1) == 0);
    name += sizeof(PREFIX_ON_NESTED_TYPE_NAME) - 1;
    add_str_to_mangled_name(name, mctl);
    goto done;
  } else if (entity_needs_parent_qualifier(&type->source_corresp, iek_type) ||
             entity_needs_to_be_individuated(&type->source_corresp, iek_type))
                                                                              {
    /* The type is a member of a class or namespace (or an entity that needs
       to be individuated), so put out a qualifier.  Note that the count starts
       at 2 because the type name itself is level 1. */
    r_mangled_parent_qualifier(&type->source_corresp, iek_type,
                               (unsigned long)2,
                               entity_needs_to_be_individuated(
                                              &type->source_corresp, iek_type),
                               (a_source_correspondence **)NULL, mctl);
  }  /* if */
#endif /* IA64_ABI */
  /* Put out the type name itself. */
  /* The mangled form of a type name is the type name with a length
       preceding it:
         AB          --> 2AB
         ABCDEFGHIJK --> 11ABCDEFGHIJK
  */
  if (is_immediate_class_type(type)) {
    /* Class name. */
    mangled_class_encoding(type,
                           /*show_partial_spec_args=*/FALSE,
                           /*show_template_specialization*/FALSE,
                           /*show_specialization=*/FALSE,
                           mctl);
  } else if (type->kind == (a_type_kind)tk_typeref &&
             is_typeref_kind(type, trk_is_template_alias)) {
    /* Template alias. */
    mangled_template_alias_encoding(type, mctl);
  } else {
    /* Not a class name (typedef, template parameter member or enum). */
#if !IA64_ABI
    reserve_space_for_length(&length_reservation, mctl);
#endif /* !IA64_ABI */
    name = unmangled_or_fabricated_name_of(&type->source_corresp);
    if (name == NULL) {
      /* For an unnamed enum type, special encodings apply. */
      check_assertion(is_immediate_enum_type(type));
      mangled_unnamed_type_encoding(type, mctl);
    } else {
      /* Named enum or typedef. */
#if IA64_ABI
      mangled_name_with_length(name, mctl);
#if ABI_COMPATIBILITY_VERSION < 401
      /* Earlier versions mistakenly emitted a discriminator at this point
         (rather than at the end of the nested name of a local type). */
      add_discriminator_if_necessary(&type->source_corresp, mctl);
#endif /* ABI_COMPATIBILITY_VERSION < 401 */
#else /* !IA64_ABI */
      add_str_to_mangled_name(name, mctl);
#endif /* IA64_ABI */
    }  /* if */
#if !IA64_ABI
    if (is_enum_type(type) &&
        type->source_corresp.is_local_to_function &&
        !type->source_corresp.is_class_member) {
      /* If the enum is a local (non-member) enum, put out a suffix
         identifying the function.  The id_number is arbitrarily specified as
         zero, relying on the name above to differentiate from other local
         enums. */
      a_routine_ptr enclosing_routine =
                                      enclosing_routine_for_local_type(type);
      add_local_name_suffix((unsigned long)0, enclosing_routine, mctl);
    }  /* if */
    fill_in_length(&length_reservation, mctl);
#endif /* IA64_ABI */
  }  /* if */
#if IA64_ABI
  close_ia64_nested_name(need_nested_name_close, discriminator_scp, mctl);
#else /* !IA64_ABI */
  if (encoding_start != 0 && !mctl->lacking_module_id) {
    /* We've just put an encoding for the type into the mangling_text_buffer;
       pull it out, remove any spaces and save it for future use. */
    char ch, *src, *dest;
    sizeof_t len = (mangling_text_buffer->size - encoding_start) - 
                   (mctl->num_leftover_spaces - save_num_leftover_spaces);
    type->source_corresp.unmangled_name_or_mangled_encoding =
                                            alloc_lowered_name_string(len + 1);
    src = &mangling_text_buffer->buffer[encoding_start];
    dest = (char *)type->source_corresp.unmangled_name_or_mangled_encoding;
    do {
      ch = *src++;
      if (ch != ' ') {
        *dest++ = ch;
      } else {
        len++;
      }  /* if */
    } while (--len);
    *dest = '\0';
  }  /* if */
#endif /* IA64_ABI */
done:;
}  /* mangled_type_name_full */


static void mangled_class_name_internal(a_type_ptr               type,
                                        a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of the class "type".
This is the encoding used for the name of the class as opposed to
the encoding for the class as a type (for example, it has no length
preceding a simple class name).  This routine has the name "_internal"
because it's intended to be called from inside a name mangling
operation; compare mangled_class_name (no "_internal").
*/
{
#if !IA64_ABI
  if (entity_needs_parent_qualifier(&type->source_corresp, iek_type)) {
#endif /* !IA64_ABI */
    /* For a nested class, use the nested type encoding for the class. */
    mangled_type_name(type, mctl);
#if !IA64_ABI
  } else {
    /* For a non-nested class, use the simple form of the name (with
       no preceding length). */
    mangled_basic_class_name(type, mctl);
  }  /* if */
#endif /* !IA64_ABI */
}  /* mangled_class_name_internal */

#if IA64_ABI

static a_boolean record_substitution_for_type(a_type_ptr type)
/*
This routine returns TRUE if a substitution should be allocated for the
specified type.  Substitutions are not allocated for <builtin-type>s
(except for any vendor extended types).
*/
{
  a_boolean result = TRUE;

  switch (type->kind) {
    case tk_error:
      check_assertion(is_at_least_one_error());
      FALLTHROUGH
    case tk_void:
    case tk_float:
    case tk_nullptr:
    case tk_reflection:
      /* These are <builtin-type>s according to the IA-64 ABI, so no
         substitution is allocated for them. */
      result = FALSE;
      break;
    case tk_integer:
      /* Integers are <builtin-type>s, enums are not. */
      if (type->variant.integer.enum_type) {
        result = TRUE;
      } else {
        result = FALSE;
      }  /* if */
      break;
    case tk_routine:
    case tk_array:
    case tk_class:
    case tk_struct:
    case tk_union:
    case tk_ptr_to_member:
      result = TRUE;
      break;
#if GNU_VECTOR_TYPES_ALLOWED
    case tk_vector:
    case tk_scalable_vector:
    case tk_scalable_vector_count:
    case tk_riscv_vector:
      /* The original mangling for vector types used a vendor extension
         (which required a substitution).  The newer mangling (i.e., "Dv")
         seems to also record a substitution (even though the "Dv" string
         technically looks like a <builtin-type> and should not require one).
         */
      result = TRUE;
      break;
    case tk_mfp8:
    case tk_float8e4m3:
    case tk_float8e5m2:
      result = TRUE;
      break;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
    case tk_template_param:
      if (is_auto_type(type)) {
        /* This occurs, for example, when mangling decltype(new auto(p1)). */
        result = FALSE;
      } else {
        result = TRUE;
      }  /* if */
      break;
    case tk_typeref:
      /* typedefs, aliases, and non-dependent decltypes/typeofs
         should have been stripped, leaving only dependent decltype/typeof
         typerefs (for which substitutions are created) or cv-qualifiers
         (which also trigger substitutions). */
      check_assertion(is_qualified_type(type) ||
                      (typeref_is_type_operator(type) ||
                       type->variant.typeref.is_dependent ||
                       is_typeref_kind(type, trk_is_deduced_auto) ||
                       is_typeref_kind(type, trk_is_deduced_decltype_auto)) ||
                      (type->variant.typeref.is_dependent_type_operator &&
                       typeref_is_type_transforming_intrinsic(type)));
      result = TRUE;
      break;
    case tk_pointer:
      /* Pointers, reference, rvalue reference all get substitutions
         (unless it's a lowered nullptr type). */
#if DO_IL_LOWERING
      if (is_or_was_nullptr_type(type)) {
        result = FALSE;
      } else
#endif /* DO_IL_LOWERING */
      /* Do not insert code here. */
      {
        result = TRUE;
      }  /* if */
      break;
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_complex:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      /* A complex type has a substitution recorded. */
#if ABI_COMPATIBILITY_VERSION >= 402
      result = TRUE;
#else /* ABI_COMPATIBILITY_VERSION < 402 */
      /* Versions prior to 4.2 mistakenly omitted this substitution. */
      result = FALSE;
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
      break;
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
#if FIXED_POINT_ALLOWED
    case tk_fixed_point:
#endif /* FIXED_POINT_ALLOWED */
    case tk_unknown:
    default:
      /* These are unexpected. */
      unexpected_condition();
  }  /* switch */
  return result;
}  /* record_substitution_for_type */

#endif /* IA64_ABI */

static void mangled_encoding_for_type_with_pack_expansion(
                                    a_type_ptr               type,
                                    a_boolean                is_pack_expansion,
                                    a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the type "type", which may or may not
be used in the context of a pack expansion (as specified by is_pack_expansion).
*/
{
  if (is_pack_expansion) {
    /* Mangle the type as a pack expansion. */
#if IA64_ABI
    if (add_substitution_if_available((char *)type, iek_type,
                                      /*is_pack_expansion=*/TRUE, mctl)) {
      /* A substitution has been used for the pack expansion type. */
    } else
#endif /* !IA64_ABI */
    /* Do not insert code here. */
    {
      /* Add the pack expansion indication, then mangle the underlying type.
         The same encoding is used for both ABIs. */
      add_str_to_mangled_name("Dp", mctl);
      mangled_encoding_for_type(type, mctl);
#if IA64_ABI
      alloc_substitution((char *)type, iek_type, /*is_pack_expansion=*/TRUE,
                         mctl);
#endif /* !IA64_ABI */
    }  /* if */
  } else {
    /* No pack expansion, do normal mangling for the type. */
    mangled_encoding_for_type(type, mctl);
  }  /* if */
}  /* mangled_encoding_for_type_with_pack_expansion */


static void add_str_for_type_returing_type_trait(
                                                a_type_ptr               type,
                                                a_mangling_control_block *mctl)
/*
Add a mangling string for a type-returning type trait (e.g., __remove_cv(T)) to
the name currently being mangled.  Note that in both ABIs, the "underlying"
type is emitted in the mangling as a template argument (so, e.g., in a
demangling __remove_cv(T) will look more like __remove_cv<T>).
*/
{
  a_const_char *name =
                  type_transforming_intrinsic_name(type->variant.typeref.kind);

#if IA64_ABI
  add_to_mangled_name('u', mctl);
  mangled_name_with_length(name, mctl);
  add_to_mangled_name('I', mctl);
  mangled_encoding_for_type(type->variant.typeref.type, mctl);
  add_to_mangled_name('E', mctl);
#else /* !IA64_ABI */
  a_length_reservation res1, res2;
  reserve_space_for_length(&res1, mctl);
  add_str_to_mangled_name(name, mctl);
  add_str_to_mangled_name("__tm__", mctl);
  reserve_space_for_length(&res2, mctl);
  add_to_mangled_name('_', mctl);
  mangled_encoding_for_type(type->variant.typeref.type, mctl);
  fill_in_length(&res2, mctl);
  fill_in_length(&res1, mctl);
#endif /* IA64_ABI */
}  /* add_str_for_type_returing_type_trait */

#if GNU_VECTOR_TYPES_ALLOWED

static a_const_char *choose_scalable_vector_name_for_tuple_elements(
                               uint8_t                          tuple_elements,
                               const a_const_char_ptr_array<4>  &names)
/*
Returns the name of a scalable vector type for the specified number of tuple
elements.  The names for each possible number of tuple elements are supplied in
the array names.
*/
{
  check_assertion(tuple_elements >= 1 && tuple_elements <= 4);
  return names[tuple_elements - 1];
}  /* choose_scalable_vector_name_for_tuple_elements */


static a_const_char *mangled_scalable_vector_name(a_type_ptr  element_type,
                                                  uint8_t     tuple_elements)
/*
Return the name to be used for mangling for a scalable vector type of the given
element type and number of tuple elements.
*/
{
  a_const_char  *s = NULL;
  element_type = skip_typerefs(element_type);
  switch (element_type->kind) {
    case tk_integer:
      {
        an_integer_kind  int_kind = element_type->variant.integer.int_kind;
        if (is_bool_type(element_type)) {
          s = choose_scalable_vector_name_for_tuple_elements(
                                             tuple_elements,
                                             {"__SVBool_t", "svboolx2_t",
                                              NULL, "svboolx4_t"});
        } else {
          switch (int_kind) {
            case ik_signed_char:
              s = choose_scalable_vector_name_for_tuple_elements(
                                             tuple_elements,
                                             {"__SVInt8_t", "svint8x2_t",
                                              "svint8x3_t", "svint8x4_t"});
              break;
            case ik_unsigned_char:
              s = choose_scalable_vector_name_for_tuple_elements(
                                             tuple_elements,
                                             {"__SVUint8_t", "svuint8x2_t",
                                              "svuint8x3_t", "svuint8x4_t"});
              break;
            case ik_short:
              s = choose_scalable_vector_name_for_tuple_elements(
                                             tuple_elements,
                                             {"__SVInt16_t", "svint16x2_t",
                                             "svint16x3_t", "svint16x4_t"});
              break;
            case ik_unsigned_short:
              s = choose_scalable_vector_name_for_tuple_elements(
                                             tuple_elements,
                                             {"__SVUint16_t", "svuint16x2_t",
                                              "svuint16x3_t", "svuint16x4_t"});
              break;
            case ik_int:
              s = choose_scalable_vector_name_for_tuple_elements(
                                             tuple_elements,
                                             {"__SVInt32_t", "svint32x2_t",
                                              "svint32x3_t", "svint32x4_t"});
              break;
            case ik_unsigned_int:
              s = choose_scalable_vector_name_for_tuple_elements(
                                             tuple_elements,
                                             {"__SVUint32_t", "svuint32x2_t",
                                              "svuint32x3_t", "svuint32x4_t"});
              break;
            case ik_long:
              s = choose_scalable_vector_name_for_tuple_elements(
                                             tuple_elements,
                                             {"__SVInt64_t", "svint64x2_t",
                                              "svint64x3_t", "svint64x4_t"});
              break;
            case ik_unsigned_long:
              s = choose_scalable_vector_name_for_tuple_elements(
                                             tuple_elements,
                                             {"__SVUint64_t", "svuint64x2_t",
                                              "svuint64x3_t", "svuint64x4_t"});
              break;
            default:
              unexpected_condition();
              break;
          }  /* switch */
        }  /* if */
      }
      break;
    case tk_float:
      switch (element_type->variant.float_kind) {
        case fk_fp16:
          s = choose_scalable_vector_name_for_tuple_elements(
                                         tuple_elements,
                                         {"__SVFloat16_t", "svfloat16x2_t",
                                          "svfloat16x3_t", "svfloat16x4_t"});
          break;
        case fk_std_bfloat16:
          s = choose_scalable_vector_name_for_tuple_elements(
                                         tuple_elements,
                                         {"__SVBfloat16_t", "svbfloat16x2_t",
                                          "svbfloat16x3_t", "svbfloat16x4_t"});
          break;
        case fk_float:
          s = choose_scalable_vector_name_for_tuple_elements(
                                         tuple_elements,
                                         {"__SVFloat32_t", "svfloat32x2_t",
                                          "svfloat32x3_t", "svfloat32x4_t"});
          break;
        case fk_double:
          s = choose_scalable_vector_name_for_tuple_elements(
                                         tuple_elements,
                                         {"__SVFloat64_t", "svfloat64x2_t",
                                          "svfloat64x3_t", "svfloat64x4_t"});
          break;
        default:
          unexpected_condition();
      }  /* switch */
      break;
    case tk_mfp8:
      s = choose_scalable_vector_name_for_tuple_elements(
                                           tuple_elements,
                                           {"__SVMFloat8_t", "svmfloat8x2_t",
                                            "svmfloat8x3_t", "svmfloat8x4_t"});
      break;
    default:
      unexpected_condition_str("mangled_scalable_vector_name: bad type kind "
                               " for scalable vector element type");
  }  /* switch */
  return s;
}  /* mangled_scalable_vector_name */

#endif /* GNU_VECTOR_TYPES_ALLOWED */

static void mangled_encoding_for_type_full(
                                a_type_ptr               type,
                                ARG_UNUSED a_boolean     suppress_substitution,
                                a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the type "type".
If suppress_substitution is TRUE, no substitution is recorded for type
(in the IA-64 ABI), relying instead on the caller to record the substitution
as needed.
*/
{
  a_type_ptr   named_type, pm_base_type;
#if ABI_COMPATIBILITY_VERSION < 230
  a_type_ptr   named_typedef = NULL;
#endif /* ABI_COMPATIBILITY_VERSION < 230 */
  a_const_char *s = NULL;
  a_type_qualifier_set
               qualifiers = TQ_NONE;
#if IA64_ABI
  a_type_ptr   qualified_type = type;
  a_boolean    saved_force_dependent_array_mangling;
#endif /* IA64_ABI */

  if (is_at_least_one_error()) {
    /* When there are errors in the front end, some of the data structures
       may contain incomplete information, so simply give this type a
       bogus encoding. */
    mangled_name_with_length("?", mctl);
    goto end_of_routine;
  }  /* if */
#if IA64_ABI
  /* If the type has appeared previously, use a substitution for it. */
  if (add_substitution_if_available((char *)type, iek_type,
                                    /*is_pack_expansion=*/FALSE, mctl)) {
    goto end_of_routine;
  }  /* if */
#endif /* IA64_ABI */
  /* Walk through typerefs above the type, remembering qualifiers and moving
     down to the underlying type.  Dependent typeref-based type operators
     (e.g., decltype/typeof/splice/pack-index) are handled here.  Template
     alias typerefs are stripped so the underlying type is mangled. */
  /*lint --e{446} type modified in loop (LINTBUG) */
  for (; type_is(type, tk_typeref); type = type->variant.typeref.type) {
#if IA64_ABI && ABI_COMPATIBILITY_VERSION >= 402
top_of_loop:
#endif /* IA64_ABI && ABI_COMPATIBILITY_VERSION >= 402 */
    /* Remember type qualifiers encountered. */
    qualifiers |= (type->variant.typeref.qualifiers & ~TRANSPARENT_QUALIFIERS);
#if ABI_COMPATIBILITY_VERSION < 230
    /* Remember the bottommost named typedef encountered. */
    if (has_name(type)) named_typedef = type;
#endif /* ABI_COMPATIBILITY_VERSION < 230 */
#if DO_IL_LOWERING
    if (type->variant.typeref.orig_type != NULL) {
      /* A type like a pointer-to-member, which has been lowered.
         Switch to the original type. */
      type = type->variant.typeref.orig_type;
      break;
    }  /* if */
#endif /* DO_IL_LOWERING */
    /* Preserve decltypes that require mangling (i.e., those that the front end
       has determined are instantiation-dependent).  GNU has a slightly
       different interpretation of when decltype mangling is needed. */
    if (is_typeref_kind(type, trk_is_decltype)) {
#if IA64_ABI && ABI_COMPATIBILITY_VERSION >= 402
      if (emulate_gnu_abi_bugs) {
        if (gnu_requires_decltype_mangling(type)) {
          /* This decltype needs to appear in the mangled name. */
          break;
        } else {
          /* To be GNU compatible, don't use decltype mangling for this
             type. */
          if (type->variant.typeref.is_dependent_type_operator) {
            /* Typically, using the type under the decltype typeref is the
               correct thing to do, but if the decltype is marked as dependent,
               get the type from the expression under the decltype and
               mangle that instead. */
            an_expr_node_ptr decltype_expr = decltype_arg(type);
            if (decltype_expr == NULL) {
              check_assertion(prototype_instantiations_in_il);
              /* Can happen in configurations where we're mangling the
                 name of a prototype instantiation. */
              break;
            }  /* if */
            type = decltype_expr->type;
#if ABI_COMPATIBILITY_VERSION >= 405
            /* If the type has appeared previously, use a substitution. */
            if (add_substitution_if_available((char *)type, iek_type,
                                              /*is_pack_expansion=*/FALSE,
                                              mctl)) {
              goto end_of_routine;
            }  /* if */
#endif /* ABI_COMPATIBILITY_VERSION >= 405 */
            if (type_is(type, tk_typeref)) {
              /* The type is a typeref of sorts; jump to the top of this
                 loop to process it. */
              goto top_of_loop;
            } else {
              /* Not a typeref; break out of the loop. */
              break;
            }  /* if */
          }  /* if */
        }  /* if */
      } else
#endif /* IA64_ABI && ABI_COMPATIBILITY_VERSION >= 402 */
      {
        if (type->variant.typeref.is_dependent_type_operator) {
          /* This decltype needs to appear in the mangled name. */
          break;
        }  /* if */
      }  /* if */
    } else if (is_typeref_kind(type, trk_is_splice) &&
               type->variant.typeref.is_dependent_type_operator) {
      /* This splice needs to appear in the mangled name. */
      break;
    } else if (is_typeref_kind(type, trk_pack_index) &&
               type->variant.typeref.is_dependent_type_operator) {
      /* This pack-index-specifier needs to appear in the mangled name. */
      break;
    } else if (is_typeref_kind(type, trk_is_underlying_type) &&
               type->variant.typeref.is_dependent_type_operator) {
      /* This __underlying_type needs to appear in the mangled name. */
      break;
#if ABI_COMPATIBILITY_VERSION >= 411
    } else if (is_typeref_kind(type, trk_is_deduced_auto) &&
               mctl->mangle_auto_placeholder) {
      /* Mangling for a deduced auto type (e.g., "operator auto()") needs
         to appear in the mangled name. */
      break;
    } else if (is_typeref_kind(type, trk_is_deduced_decltype_auto) &&
               mctl->mangle_auto_placeholder) {
      /* Mangling for a decltype(auto) needs to appear in the mangled name. */
      break;
#endif /* ABI_COMPATIBILITY_VERSION >= 411 */
    } else if (type->variant.typeref.is_dependent_type_operator &&
               typeref_is_type_transforming_intrinsic(type)) {
      /* Mangling for a dependent type-returning type trait needs to appear in
         the mangled name. */
      break;
    }  /* if */
#if GNU_EXTENSIONS_ALLOWED
    if ((is_typeref_kind(type, trk_is_typeof_with_expression) ||
         is_typeref_kind(type, trk_is_typeof_with_type_operand)) &&
        type->variant.typeref.is_dependent_type_operator) {
      /* This typeof needs to appear in the mangled name. */
      break;
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  }  /* for */
#if GNU_EXTENSIONS_ALLOWED
  if (gpp_mode && is_function_type(type)) {
    /* In g++ versions, function types with the "noreturn" or "volatile"
       attributes are mangled as though declared with the volatile keyword. */
    a_routine_type_supplement_ptr rtsp = type->variant.routine.extra_info;
    check_assertion(rtsp != NULL);
    if (rtsp->does_not_return) {
      /* Add a "volatile" qualifier to the mangled type, but also create
         a new type that represents the same type without the "noreturn"
         attribute (since these get separate substitutions).  E.g.,
          void f(void (*)() __attribute__((noreturn)), void (*)()) ; */
      a_type_ptr new_type = alloc_type((a_type_kind)tk_routine);
      qualifiers |= TQ_VOLATILE;
      copy_type(type, new_type);
#if DO_IL_LOWERING
      il_lowering_flag_of(new_type) = il_lowering_flag_of(type);
#endif /* DO_IL_LOWERING */
      new_type->variant.routine.extra_info->does_not_return = FALSE;
      type = new_type;
    }  /* if */
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  /* Put out type qualifiers, if any. */
  if (qualifiers != 0) {
    mangled_encoding_for_type_qualifiers(qualifiers, mctl);
#if IA64_ABI
    /* Check for another substitution for the unqualified type. */
    if (add_substitution_if_available((char *)type, iek_type,
                                      /*is_pack_expansion=*/FALSE, mctl)) {
      goto add_substitution_for_qualified_type;
    }  /* if */
#endif /* IA64_ABI */
  }  /* if */
  /* See if the type is a named class or enum. */
  named_type = NULL;
  if (has_name(type) &&
      (is_immediate_class_type(type) || is_immediate_enum_type(type))) {
    /* Named class or enum type. */
    named_type = type;
#if ABI_COMPATIBILITY_VERSION < 230
  } else if (named_typedef != NULL && is_immediate_enum_type(type)) {
    /* Unnamed enum with a typedef above it.  Use the typedef name for the
       enum even though it's the name of a qualified version of the enum.
       In ABI versions >= 2.30, the processing for this was moved
       to decls.c for greater compatibility with cfront when
       CFRONT_OBJECT_CODE_COMPATIBILITY is TRUE, and eliminated otherwise. */
    named_type = named_typedef;
#endif /* ABI_COMPATIBILITY_VERSION < 230 */
  }  /* if */
  /* If the type is named, use the name. */
  if (named_type != NULL) {
    /* Put out the mangled form of the name, e.g., "2AB" for "AB". */
    /* The possibility of an IA-64 ABI substitution was already checked for
       above. */
    mangled_type_name_full(named_type, /*check_for_subst=*/FALSE,
                           /*ok_to_mangle_type=*/TRUE, mctl);
  } else {
    /* The type is not named, so develop a description string. */
    switch (type->kind) {
      case tk_error:
      case tk_unknown:
        /* This might come up in mangling names for template instantiation
           after errors have been detected. */
        check_assertion(is_at_least_one_error());
        s = "?";
        break;
      case tk_void:
        s = MANGLING_STRING_FOR_VOID;
        break;
      case tk_integer:
        if (type->variant.integer.enum_type) {
          /* Unnamed enum.  mangled_type_name_full will make up a name. */
          mangled_type_name_full(type, /*check_for_subst=*/FALSE, 
                                 /*ok_to_mangle_type=*/TRUE, mctl);
          goto have_whole_mangled_name;
        } else if (is_bit_precise_kind(type->variant.integer.int_kind)) {
          add_str_to_mangled_name(
              type->variant.integer.int_kind == ik_bit_precise ? "DB" : "DU",
              mctl);
          add_number_to_mangled_name(
                    (a_host_large_unsigned)integer_type_supp(type)->bit_width,
                    mctl);
          add_to_mangled_name('_', mctl);
          goto have_whole_mangled_name;
        }  /* if */
        if (type->variant.integer.wchar_t_type) {
          s = MANGLING_STRING_FOR_WCHAR_T;
        } else if (type->variant.integer.char8_t_type) {
          s = MANGLING_STRING_FOR_CHAR8_T;
        } else if (type->variant.integer.char16_t_type) {
          s = MANGLING_STRING_FOR_CHAR16_T;
        } else if (type->variant.integer.char32_t_type) {
          s = MANGLING_STRING_FOR_CHAR32_T;
        } else if (type->variant.integer.bool_type) {
          s = MANGLING_STRING_FOR_BOOL;
#if MICROSOFT_EXTENSIONS_ALLOWED && !IA64_ABI
        } else if (type->variant.integer.microsoft_sized_int_type) {
          /* Mangling of __intN types in certain Microsoft modes (Visual C++
             6.0 treated these as new intrinsic types; 7.0 went back to
             treating them as the same as the corresponding integral type). */
          an_integer_kind  kind = type->variant.integer.int_kind;
          if (kind == targ_int8_int_kind) {
            s = "m1";
          } else if (kind == targ_unsigned_int8_int_kind) {
            s = "Um1";
          } else if (kind == targ_int16_int_kind) {
            s = "m2";
          } else if (kind == targ_unsigned_int16_int_kind) {
            s = "Um2";
          } else if (kind == targ_int32_int_kind) {
            s = "m4";
          } else if (kind == targ_unsigned_int32_int_kind) {
            s = "Um4";
          } else if (kind == targ_int64_int_kind) {
            s = "m8";
          } else if (kind == targ_unsigned_int64_int_kind) {
            s = "Um8";
          }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED && !IA64_ABI */
        } else {
          switch (type->variant.integer.int_kind) {
            case ik_char:           
              s = MANGLING_STRING_FOR_CHAR;
              break;
            case ik_signed_char:    
              s = MANGLING_STRING_FOR_SIGNED_CHAR;
              break;
            case ik_unsigned_char:  
              s = MANGLING_STRING_FOR_UNSIGNED_CHAR;
              break;
            case ik_short:          
              s = MANGLING_STRING_FOR_SHORT;
              break;
            case ik_unsigned_short: 
              s = MANGLING_STRING_FOR_UNSIGNED_SHORT;
              break;
            case ik_int:            
              s = MANGLING_STRING_FOR_INT;
              break;
            case ik_unsigned_int:   
              s = MANGLING_STRING_FOR_UNSIGNED_INT;
              break;
            case ik_long:           
              s = MANGLING_STRING_FOR_LONG;
              break;
            case ik_unsigned_long:  
              s = MANGLING_STRING_FOR_UNSIGNED_LONG;
              break;
#if LONG_LONG_ALLOWED
            case ik_long_long:      
              s = MANGLING_STRING_FOR_LONG_LONG;
              break;
            case ik_unsigned_long_long:
              s = MANGLING_STRING_FOR_UNSIGNED_LONG_LONG;
              break;
#endif /* LONG_LONG_ALLOWED */
#if INT128_EXTENSIONS_ALLOWED
            case ik_int128:      
              s = MANGLING_STRING_FOR_INT128;
              break;
            case ik_unsigned_int128:
              s = MANGLING_STRING_FOR_UNSIGNED_INT128;
              break;
#endif /* INT128_EXTENSIONS_ALLOWED */
            default:
              unexpected_condition_str(
                                    "mangled_encoding_for_type: bad int kind");
          }  /* switch */
        }  /* if */
        break;
#if FIXED_POINT_ALLOWED
      case tk_fixed_point:
        unexpected_condition_str("mangling of fixed-point types unsupported");
        break;
#endif /* FIXED_POINT_ALLOWED */
      case tk_float:
        switch (type->variant.float_kind) {
          case fk_float16:
            s = MANGLING_STRING_FOR_FLOAT16;
            break;
          case fk_fp16:
            s = MANGLING_STRING_FOR_FP16;
            break;
          case fk_float:
            s = MANGLING_STRING_FOR_FLOAT;
            break;
          case fk_float32x:
            s = MANGLING_STRING_FOR_FLOAT32X;
            break;
          case fk_double:
            s = MANGLING_STRING_FOR_DOUBLE;
            break;
          case fk_float64x:
            s = MANGLING_STRING_FOR_FLOAT64X;
            break;
          case fk_long_double:
            s = MANGLING_STRING_FOR_LONG_DOUBLE;
            break;
          case fk_float80:
            s = MANGLING_STRING_FOR_FLOAT80;
            break;
          case fk_float128:
            s = MANGLING_STRING_FOR_FLOAT128;
            break;
          case fk_std_bfloat16:
#if IA64_ABI
            if (clang_mode || (gnu_mode && target_is_arm_based())) {
              /* Use "u6__bf16" mangling. */
              s = MANGLING_STRING_FOR_STD_BFLOAT16_ARM;
            } else {
              /* Use "DF16b" mangling. */
              s = MANGLING_STRING_FOR_STD_BFLOAT16_X86;
            }  /* if */
#else /* !IA64_ABI */
            s = MANGLING_STRING_FOR_STD_BFLOAT16;
#endif /* IA64_ABI */
            break;
          case fk_std_float16:
            s = MANGLING_STRING_FOR_STD_FLOAT16;
            break;
          case fk_std_float32:
            s = MANGLING_STRING_FOR_STD_FLOAT32;
            break;
          case fk_std_float64:
            s = MANGLING_STRING_FOR_STD_FLOAT64;
            break;
          case fk_std_float128:
            s = MANGLING_STRING_FOR_STD_FLOAT128;
            break;
          default:
            unexpected_condition_str(
                                  "mangled_encoding_for_type: bad float kind");
        }  /* switch */
        break;
#if C99_IL_EXTENSIONS_SUPPORTED
      case tk_complex:
        switch (type->variant.float_kind) {
          case fk_float16:
            s = MANGLING_STRING_FOR_COMPLEX_FLOAT16;
            break;
          case fk_float:
            s = MANGLING_STRING_FOR_COMPLEX_FLOAT;
            break;
          case fk_float32x:
            s = MANGLING_STRING_FOR_COMPLEX_FLOAT32X;
            break;
          case fk_double:
            s = MANGLING_STRING_FOR_COMPLEX_DOUBLE;
            break;
          case fk_float64x:
            s = MANGLING_STRING_FOR_COMPLEX_FLOAT64X;
            break;
          case fk_long_double:
            s = MANGLING_STRING_FOR_COMPLEX_LONG_DOUBLE;
            break;
          case fk_float80:    
            s = MANGLING_STRING_FOR_COMPLEX_FLOAT80;
            break;
          case fk_float128:
            s = MANGLING_STRING_FOR_COMPLEX_FLOAT128;
            break;
          case fk_std_bfloat16:
#if IA64_ABI
            if (clang_mode || (gnu_mode && target_is_arm_based())) {
              /* Use "u6__bf16" mangling. */
              s = MANGLING_STRING_FOR_COMPLEX_STD_BFLOAT16_ARM;
            } else {
              /* Use "DF16b" mangling. */
              s = MANGLING_STRING_FOR_COMPLEX_STD_BFLOAT16_X86;
            }  /* if */
#else /* !IA64_ABI */
            s = MANGLING_STRING_FOR_COMPLEX_STD_BFLOAT16;
#endif /* IA64_ABI */
            break;
          case fk_std_float16:
            s = MANGLING_STRING_FOR_COMPLEX_STD_FLOAT16;
            break;
          case fk_std_float32:
            s = MANGLING_STRING_FOR_COMPLEX_STD_FLOAT32;
            break;
          case fk_std_float64:
            s = MANGLING_STRING_FOR_COMPLEX_STD_FLOAT64;
            break;
          case fk_std_float128:
            s = MANGLING_STRING_FOR_COMPLEX_STD_FLOAT128;
            break;
          default:
            unexpected_condition_str(
                                  "mangled_encoding_for_type: bad float kind");
        }  /* switch */
        break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      case tk_pointer:
#if DO_IL_LOWERING
        if (is_or_was_nullptr_type(type)) {
          /* A nullptr type is not itself a pointer type and is handled in
             the tk_nullptr case below, but it is lowered to a pointer
             type; the lowered type is handled here. */
#if MICROSOFT_EXTENSIONS_ALLOWED
          if (is_managed_nullptr_type(type)) {
            /* Use a different mangling for C++/CLI managed __nullptr. */
            s = MANGLING_STRING_FOR_MANAGED_NULLPTR;
          } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          /* Do not insert code here. */
          {
            s = MANGLING_STRING_FOR_NULLPTR;
          }  /* if */
        } else 
#endif /* DO_IL_LOWERING */
        /* Do not insert code here. */
        if (type->variant.pointer.is_reference) {
          if (type->variant.pointer.is_rvalue_reference) {
            s = MANGLING_STRING_FOR_RVALUE_REFERENCE;
#if MICROSOFT_EXTENSIONS_ALLOWED
          } else if (type->variant.pointer.is_handle) {
            s = MANGLING_STRING_FOR_TRACKING_REFERENCE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          } else {
            s = MANGLING_STRING_FOR_REFERENCE;
          }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
        } else if (type->variant.pointer.is_handle) {
          s = MANGLING_STRING_FOR_HANDLE;
        } else if (type->variant.pointer.is_interior_ptr) {
          s = MANGLING_STRING_FOR_INTERIOR_PTR;
        } else if (type->variant.pointer.is_pin_ptr) {
          s = MANGLING_STRING_FOR_PIN_PTR;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        } else {
          s = MANGLING_STRING_FOR_POINTER;
        }  /* if */
        /* More of this below -- the "P" or "R" is followed by the
           type pointed to/referenced. */
        break;
      case tk_ptr_to_member:
        /* Pointer to member.  int S::* is put out as M1Si (Cfront-style). */
        s = MANGLING_STRING_FOR_POINTER_TO_MEMBER;
        /* More of this below -- the "M" is followed by the class name and
           the type pointed to. */
        break;
      case tk_array:
        s = MANGLING_STRING_FOR_ARRAY;
        /* More of this below -- int[10] is put out as A10_i (Cfront-style). */
        break;
      case tk_routine:
        /* Emit function qualifiers if necessary. */
        mangled_encoding_for_function_qualifiers(type,
                                                 /*is_class_member=*/FALSE,
                                                 mctl);
        /* Function.  Put out "F" and the argument types. */
        mangled_encoding_for_function_type(type,
                                           /*do_return_type=*/TRUE,
                                           /*mangling_function_name=*/FALSE,
                                           mctl);
        goto have_whole_mangled_name;
      case tk_class:
      case tk_struct:
      case tk_union:
        /* Unnamed classes.  mangled_type_name_full will make up a name. */
        mangled_type_name_full(type, /*check_for_subst=*/FALSE, 
                               /*ok_to_mangle_type=*/TRUE, mctl);
        goto have_whole_mangled_name;
      case tk_template_param:
        /* This comes up when mangling the names for template entities using
           the modern mangling approach. */
        if (is_auto_type(type)) {
          /* This occurs, for example, when mangling decltype(new auto(p1)). */
#if ABI_COMPATIBILITY_VERSION >= 411
          if (type->variant.template_param.extra_info->
                            coordinates.position == DECLTYPE_AUTO_POS_NUMBER) {
            /* decltype(auto). */
            s = MANGLING_STRING_FOR_DECLTYPE_AUTO;
          } else
#endif /* ABI_COMPATIBILITY_VERSION >= 411 */
          /* Do not insert code here. */
          {
            s = MANGLING_STRING_FOR_AUTO;
          }  /* if */
        } else {
          switch (type->variant.template_param.kind) {
            case tptk_param:
              mangled_encoding_for_template_parameter_with_ctad_check(
                         type,
                         (a_template_arg *)NULL,
                         mctl);
              break;
            case tptk_member:
              /* Type selected from a template parameter type, e.g., T::x. */
              if (!has_name(type)) {
                give_unnamed_template_param_member_a_name(type, mctl);
              }  /* if */
              mangled_type_name_full(type, /*check_for_subst=*/FALSE, 
                                     /*ok_to_mangle_type=*/TRUE, mctl);
              break;
            case tptk_unknown:
              /* An unknown template parameter can appear in prototype
                 instantiations (e.g., in expressions); give it a bogus
                 name. */
              mangled_name_with_length("?", mctl);
              break;
            case tptk_bit_precise_int:
              add_str_to_mangled_name(
                      type->variant.template_param.is_unsigned_bit_precise_int
                                                            ? "DU" : "DB",
                      mctl);
              mangled_encoding_for_constant(
                 type->variant.template_param.extra_info
                     ->constraint.bit_width_constant,
                 /*old_form=*/FALSE, /*in_dependent_expr=*/TRUE,
                 /*suppress_address_of=*/FALSE, mctl);
              add_to_mangled_name('_', mctl);
              break;
            default:
              unexpected_condition_str(
                      "mangled_encoding_for_type: bad tk_template_param kind");
          }  /* switch */
          goto have_whole_mangled_name;
        }  /* if */
        break;
#if GNU_VECTOR_TYPES_ALLOWED
      case tk_vector:
        if (type->variant.vector.kind == vk_neon_builtin) {
          s = get_predefined_name_for_builtin_neon_vector_type(
                                             type->variant.vector.element_type,
                                             num_vector_elements(type));
          check_assertion(s != NULL);
          mangled_name_with_length(s, mctl);
          goto have_whole_mangled_name;
        } else if (type->variant.vector.kind == vk_neon ||
                   type->variant.vector.kind == vk_neon_poly) {
          a_type_ptr  element_type = skip_typerefs(type->variant.vector.
                                                                 element_type);
          s = get_predefined_name_for_neon_vector_type(
                                                    element_type,
                                                    num_vector_elements(type),
                                                    type->variant.vector.kind);
          check_assertion(s != NULL);
          mangled_name_with_length(s, mctl);
          goto have_whole_mangled_name;
        } else {
          s = MANGLING_STRING_FOR_VECTOR;
#if ABI_COMPATIBILITY_VERSION >= 415 && IA64_ABI
          if (!(gnu_mode && !clang_mode && gnu_abi_version < 50000)) {
            /* Later versions of GCC use the IA-64 ABI standard way to mangle
               a vector type. */
            s = "Dv";
          }  /* if */
#endif /* ABI_COMPATIBILITY_VERSION >= 415 && IA64_ABI */
          /* More of this below. */
        }  /* if */
        break;
      case tk_mfp8:
        s = MANGLING_STRING_FOR_MFP8;
        break;
      case tk_float8e4m3:
        s = MANGLING_STRING_FOR_FLOAT8E4M3;
        break;
      case tk_float8e5m2:
        s = MANGLING_STRING_FOR_FLOAT8E5M2;
        break;
      case tk_scalable_vector_count:
        s = MANGLING_STRING_FOR_SCALABLE_VECTOR_COUNT;
        break;
      case tk_scalable_vector:
        {
          uint8_t  tuple_elements =
                                  type->variant.scalable_vector.tuple_elements;
#if IA64_ABI
          if (tuple_elements == 1) {
            /* For a single tuple element, a scalable vector type is mangled as
               a vendor extended builtin type; otherwise it is mangled as its
               name. */
            add_to_mangled_name('u', mctl);
          }  /* if */
#endif /* IA64_ABI */
          s = mangled_scalable_vector_name(
                                    type->variant.scalable_vector.element_type,
                                    tuple_elements);
          mangled_name_with_length(s, mctl);
          goto have_whole_mangled_name;
        }
        break;
      case tk_riscv_vector:
#if IA64_ABI
        /* Mangle a RISC-V vector type as a vendor extended builtin type. */
        add_to_mangled_name('u', mctl);
#endif /* IA64_ABI */
        mangled_name_with_length(
                     get_name_for_riscv_vector_type("__rvv_",
                                                    type).as_temp_characters(),
                     mctl);
        goto have_whole_mangled_name;
        break;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
      case tk_nullptr:
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (is_managed_nullptr_type(type)) {
          /* Use a different mangling for C++/CLI managed __nullptr. */
          s = MANGLING_STRING_FOR_MANAGED_NULLPTR;
        } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Do not insert code here. */
        {
          s = MANGLING_STRING_FOR_NULLPTR;
        }  /* if */
        break;
      case tk_typeref:
        /* Typedefs, cv-qualifiers, aliases, and nondependent type operators
           should have been stripped, leaving only dependent operator typerefs
           (e.g., decltype/splice/pack-index/__underlying_type/typeof),
           type-returning type traits, and deduced-auto placeholders. */
        check_assertion(typeref_is_type_operator(type) ||
                        typeref_is_type_transforming_intrinsic(type) ||
                        is_typeref_kind(type, trk_is_deduced_auto) ||
                        is_typeref_kind(type, trk_is_deduced_decltype_auto));
        if (is_typeref_kind(type, trk_is_decltype)) {
          /* Provide mangling for decltype. */
          an_expr_node_ptr decltype_expr = decltype_arg(type);
          if (type->variant.typeref.decltype_expr_not_parenthesized) {
            add_str_to_mangled_name(MANGLING_STRING_FOR_DECLTYPE_TYPE, mctl);
          } else {
            add_str_to_mangled_name(MANGLING_STRING_FOR_DECLTYPE_EXPR, mctl);
          }  /* if */
          if (decltype_expr == NULL) {
            /* Should happen only for prototype instantiations. */
            check_assertion(prototype_instantiations_in_il);
            add_to_mangled_name('?', mctl);
          } else {
            mangled_encoding_for_expression(decltype_expr,
                                            /*in_dependent_expr=*/TRUE, mctl);
          }  /* if */
#if IA64_ABI
          add_to_mangled_name('E', mctl);
#endif /* IA64_ABI */
          goto have_whole_mangled_name;
        } else if (is_typeref_kind(type, trk_is_splice)) {
          add_str_to_mangled_name(MANGLING_STRING_FOR_TYPE_SPLICE, mctl);
          mangled_encoding_for_expression(decltype_arg(type),
                                          /*in_dependent_expr=*/TRUE, mctl);
#if IA64_ABI
          add_to_mangled_name('E', mctl);
#endif /* IA64_ABI */
          goto have_whole_mangled_name;
        } else if (is_typeref_kind(type, trk_pack_index)) {
          /* The IA-64 ABI does not specify how to mangle a pack index type
             specifier yet, so just use the pack for now (which is what Clang
             does). */
          mangled_encoding_for_type(typeref_supp(type)->operator_type_arg,
                                    mctl);
          goto have_whole_mangled_name;
        } else if (is_typeref_kind(type, trk_is_underlying_type)) {
          /* Provide mangling for __underlying_type.  Note that in the IA-64
             case, the encoding used will vary depending on the value of
             ABI_COMPATIBILITY_VERSION (see the definition of
             MANGLING_STRING_FOR_UNDERLYING_TYPE above). */
          add_str_to_mangled_name(MANGLING_STRING_FOR_UNDERLYING_TYPE, mctl);
          mangled_encoding_for_type(type->variant.typeref.type, mctl);
#if IA64_ABI
          add_to_mangled_name('E', mctl);
#endif /* IA64_ABI */
          goto have_whole_mangled_name;
#if ABI_COMPATIBILITY_VERSION >= 411
        } else if (is_typeref_kind(type, trk_is_deduced_auto)) {
          /* Provide mangling for a deduced auto type (e.g.,
             "operator auto()"). */
          add_str_to_mangled_name(MANGLING_STRING_FOR_AUTO, mctl);
          goto have_whole_mangled_name;
        } else if (is_typeref_kind(type, trk_is_deduced_decltype_auto)) {
          /* Provide mangling for a decltype(auto). */
          add_str_to_mangled_name(MANGLING_STRING_FOR_DECLTYPE_AUTO, mctl);
          goto have_whole_mangled_name;
#endif /* ABI_COMPATIBILITY_VERSION >= 411 */
#if GNU_EXTENSIONS_ALLOWED
        } else if (is_typeref_kind(type, trk_is_typeof_with_expression) ||
                   is_typeref_kind(type, trk_is_typeof_with_type_operand)) {
          /* Provide mangling for typeof. */
#if ABI_COMPATIBILITY_VERSION >= 402
          /* There is no IA-64 ABI encoding for typeof (a GNU extension) and
             GNU doesn't provide a mangling for typeof that we can emulate, so
             these "Dy" and "DY" manglings are an EDG extension:

             <type> ::= Dy <type> E       # typeof(type)
                    ::= DY <expression> E # typeof(expression)
             */
          if (is_typeref_kind(type, trk_is_typeof_with_type_operand)) {
            add_str_to_mangled_name(MANGLING_STRING_FOR_TYPEOF_TYPE, mctl);
            mangled_encoding_for_type(type->variant.typeref.type, mctl);
          } else if (decltype_arg(type) != NULL) {
            add_str_to_mangled_name(MANGLING_STRING_FOR_TYPEOF_EXPR, mctl);
            mangled_encoding_for_expression(decltype_arg(type),
                                            /*in_dependent_expr=*/TRUE, mctl);
          } else {
            /* This should occur only in configurations that generate prototype
               instantiations. */
            check_assertion(prototype_instantiations_in_il);
            add_str_to_mangled_name(MANGLING_STRING_FOR_TYPEOF_TYPE, mctl);
            mangled_name_with_length("?", mctl);
          }  /* if */
#if IA64_ABI
          add_to_mangled_name('E', mctl);
#endif /* IA64_ABI */
#else /* ABI_COMPATIBILITY_VERSION < 402 */
#if IA64_ABI
          /* Use a vendor extension for typeof. */
          add_str_to_mangled_name("u6typeof", mctl);
#else /* !IA64_ABI */
          mangled_name_with_length("__typeof", mctl);
#endif /* IA64_ABI */
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
          goto have_whole_mangled_name;
#endif /* GNU_EXTENSIONS_ALLOWED */
        } else if (type->variant.typeref.is_dependent_type_operator &&
                   typeref_is_type_transforming_intrinsic(type)) {
          /* Provide mangling for a type-transforming type trait. */
          add_str_for_type_returing_type_trait(type, mctl);
          goto have_whole_mangled_name;
        }  /* if */
        break;
      case tk_reflection:
        s = MANGLING_STRING_FOR_META_INFO;
        break;
      default:
        unexpected_condition_str("mangled_encoding_for_type: bad type kind");
    }  /* switch */
    /* s is now set to a type description string to be output. */
    check_assertion(s != NULL);
    add_str_to_mangled_name(s, mctl);
    /* Do any processing needed after the description letter. */
    switch (type->kind) {
      case tk_pointer:
#if DO_IL_LOWERING
        /* The lowered nullptr type is a pointer, but is mangled by 
           itself and should not be decorated by the type pointed to. */
        if (!is_or_was_nullptr_type(type)) 
#endif /* DO_IL_LOWERING */
        /* Do not insert code here. */
        {
          /* Put out the type pointed to. */
          mangled_encoding_for_type(type->variant.pointer.type, mctl);
        }  /* if */
        break;
      case tk_ptr_to_member:
        /* Put out the mangled name of the class for which this is a member
           pointer. */
        mangled_encoding_for_type(type->variant.ptr_to_member.
                                                       class_of_which_a_member,
                                  mctl);
        pm_base_type = type->variant.ptr_to_member.type;
        /* Put out the type pointed to. */
        mangled_encoding_for_type(pm_base_type, mctl);
        break;
      case tk_array:
        /* Put out the array size, an underscore, and then the element type,
           i.e., int[10] is put out as A10_i. */
        check_assertion(!type->variant.array.is_vla);
#if IA64_ABI
        saved_force_dependent_array_mangling =
                                          mctl->force_dependent_array_mangling;
#endif /* IA64_ABI */
        if (type->variant.array.is_template_dependent_size_array) {
          /* Template-dependent size arrays are possible when putting out
             function prototypes. */
          a_constant_ptr elem_con =
                            type->variant.array.variant.element_count_constant;
          if (elem_con != NULL) {
#if !IA64_ABI 
            /* For that case the prefix is "A_". */
#endif /* !IA64_ABI */
            check_assertion(distinct_template_signatures);
#if !IA64_ABI
            add_to_mangled_name('_', mctl);
#else /* IA64_ABI */
            if (emulate_gnu_abi_bugs &&
#if ABI_COMPATIBILITY_VERSION >= 402
                gnu_abi_version < 30400 &&
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
                elem_con->kind == (a_constant_repr_kind)ck_template_param) {
              /* Force bounds under this one to be mangled as expressions
                 to match an early g++ bug (which was fixed in 3.4.0). */
              mctl->force_dependent_array_mangling = TRUE;
            }  /* if */
#endif /* !IA64_ABI */
            /* Put out an encoding for the bound. */
            mangled_encoding_for_constant(elem_con,
                                          /*old_form=*/FALSE,
                                          /*in_dependent_expr=*/FALSE,
                                          /*suppress_address_of=*/FALSE,
                                          mctl);
#if !IA64_ABI
          } else {
            /* There's no way to mangle "[]" in the Cfront ABI, so mangle this
               as "[0]" instead. */
            add_to_mangled_name('_', mctl);
            add_number_to_mangled_name((unsigned long)0, mctl);
#endif /* !IA64_ABI */
          }  /* if */
#if IA64_ABI
        } else if (!type->variant.array.bound_is_zero && 
                   type->variant.array.variant.number_of_elements == 0) {
          /* If there is no bound, nothing is output.  */
        } else if (mctl->force_dependent_array_mangling) {
          /* Put out a constant bound as an expression to emulate an early
             g++ bug. */
          check_assertion(emulate_gnu_abi_bugs);
          add_mangling_for_array_element(
                 (unsigned long)type->variant.array.variant.number_of_elements,
                 mctl);
#endif /* IA64_ABI */
        } else if (type->variant.array.is_variable_size_array) {
          /* Put out the mangled expression for the number of elements. */
          mangled_encoding_for_expression(
                               type->variant.array.variant.element_count_expr,
                               /*in_dependent_expr=*/TRUE, mctl);
        } else {
          /* Put out the (constant) number of elements. */
          add_number_to_mangled_name((unsigned long)type->variant.array.
                                                    variant.number_of_elements,
                                     mctl);
        }  /* if */
        add_to_mangled_name('_', mctl);
        /* Put out the element type. */
        mangled_encoding_for_type(type->variant.array.element_type, mctl);
#if IA64_ABI
        /* Note that this is restored AFTER the element type is mangled. */
        mctl->force_dependent_array_mangling = 
                                          saved_force_dependent_array_mangling;
#endif /* IA64_ABI */
        break;
#if GNU_VECTOR_TYPES_ALLOWED
      case tk_vector:
#if IA64_ABI
        /* For the IA-64 ABI, use either the old (i.e., U8__vector) mangling
           or the newer (i.e., "Dv<expression>_<type>") mangling. */
#if ABI_COMPATIBILITY_VERSION >= 415
        if (!(gnu_mode && !clang_mode && gnu_abi_version < 50000)) {
          /* Later versions of GCC use the IA-64 ABI standard way to mangle
             a vector type. */
          add_number_to_mangled_name((unsigned long)num_vector_elements(type),
                                     mctl);
          add_to_mangled_name('_', mctl);
        }  /* if */
#endif /* ABI_COMPATIBILITY_VERSION >= 415 */
#else /* !IA64_ABI */
        /* With the Cfront ABI we have no compatibility constraints, so the
           type is unambiguously encoded. */
        add_number_to_mangled_name((unsigned long)type->size, mctl);
        add_to_mangled_name('_', mctl);
#endif /* if */
        mangled_encoding_for_type(type->variant.vector.element_type, mctl);
        break;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
      default:;
        /* Many cases don't require any handling. */
    }  /* switch */
  }  /* if */
have_whole_mangled_name:;
#if IA64_ABI
  /* Create a substitution for the unqualified type.  No substitutions are
     created for <builtin-type>s (with the exception of vendor extended
     types). */
  if (!suppress_substitution && record_substitution_for_type(type)) {
    alloc_substitution((char *)type, iek_type, /*is_pack_expansion=*/FALSE,
                       mctl);
  }  /* if */
add_substitution_for_qualified_type:
  /* Create a substitution for the original type, if it was qualified. */
  if (!suppress_substitution && qualifiers != TQ_NONE) {
    alloc_substitution((char *)qualified_type, iek_type,
                       /*is_pack_expansion=*/FALSE, mctl);
  }  /* if */
#endif /* IA64_ABI */
end_of_routine:;
}  /* mangled_encoding_for_type_full */


static void mangled_encoding_for_type(a_type_ptr               type,
                                      a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the type "type".  In the IA-64 ABI,
a substitution is recorded for the type.
*/
{
  mangled_encoding_for_type_full(type, /*suppress_substitution=*/FALSE, mctl);
}  /* mangled_encoding_for_type */


static a_const_char *mangled_operator_name(
                                          an_opname_kind          kind,
                                          ARG_UNUSED unsigned int num_operands)
/*
Return the string used to indicate the indicated operator name in mangled
names.  The string does not have the leading "__" used in some cases.  The
number of operands is given by num_operands; in some configurations unary and
binary versions of operators are mangled differently.
*/
{
  a_const_char *name = NULL;

  switch (kind) {
    case onk_new:               /* "new" */
      name = MANGLING_STRING_FOR_OPERATOR_NEW;
      break;
    case onk_delete:            /* "delete" */
      name = MANGLING_STRING_FOR_OPERATOR_DELETE;
      break;
    case onk_array_new:         /* "new[]" */
      name = MANGLING_STRING_FOR_OPERATOR_ARRAY_NEW;
      break;
    case onk_array_delete:      /* "delete[]" */
      name = MANGLING_STRING_FOR_OPERATOR_ARRAY_DELETE;
      break;
    case onk_plus:              /* "+" */
#ifdef MANGLING_STRING_FOR_OPERATOR_UNARY_PLUS
      if (num_operands == 1) {
        name = MANGLING_STRING_FOR_OPERATOR_UNARY_PLUS;
      } else 
#endif /* ifdef MANGLING_STRING_FOR_OPERATOR_UNARY_PLUS */
      /* Do not add code here. */
      {
        name = MANGLING_STRING_FOR_OPERATOR_PLUS;
      }
      break;
    case onk_minus:             /* "-" */
#ifdef MANGLING_STRING_FOR_OPERATOR_NEGATE
      if (num_operands == 1) {
        name = MANGLING_STRING_FOR_OPERATOR_NEGATE;
      } else 
#endif /* ifdef MANGLING_STRING_FOR_OPERATOR_NEGATE */
      /* Do not add code here. */
      {
        name = MANGLING_STRING_FOR_OPERATOR_MINUS;
      }  /* if */
      break;
    case onk_star:              /* "*" */
#ifdef MANGLING_STRING_FOR_OPERATOR_DEREFERENCE
      if (num_operands == 1) {
        name = MANGLING_STRING_FOR_OPERATOR_DEREFERENCE;
      } else 
#endif /* ifdef MANGLING_STRING_FOR_OPERATOR_DEREFERENCE */
      /* Do not add code here. */
      {
        name = MANGLING_STRING_FOR_OPERATOR_MULT;
      }  /* if */
      break;
    case onk_divide:            /* "/" */
      name = MANGLING_STRING_FOR_OPERATOR_DIVIDE;
      break;
    case onk_remainder:         /* "%" */
      name = MANGLING_STRING_FOR_OPERATOR_REMAINDER;
      break;
    case onk_excl_or:           /* "^" */
      name = MANGLING_STRING_FOR_OPERATOR_EXCL_OR;
      break;
    case onk_ampersand:         /* "&" */
#ifdef MANGLING_STRING_FOR_OPERATOR_ADDRESS
      if (num_operands == 1) {
        name = MANGLING_STRING_FOR_OPERATOR_ADDRESS;
      } else
#endif /* ifdef MANGLING_STRING_FOR_OPERATOR_ADDRESS */
      /* Do not add code here. */
      {
        name = MANGLING_STRING_FOR_OPERATOR_AND;
      }  /* if */
      break;
    case onk_or:                /* "|" */
      name = MANGLING_STRING_FOR_OPERATOR_OR;
      break;
    case onk_compl:             /* "~" */
      name = MANGLING_STRING_FOR_OPERATOR_COMPLEMENT;
      break;
    case onk_not:               /* "!" */
      name = MANGLING_STRING_FOR_OPERATOR_NOT;
      break;
    case onk_assign:            /* "=" */
      name = MANGLING_STRING_FOR_OPERATOR_ASSIGN;
      break;
    case onk_lt:                /* "<" */
      name = MANGLING_STRING_FOR_OPERATOR_LT;
      break;
    case onk_gt:                /* ">" */
      name = MANGLING_STRING_FOR_OPERATOR_GT;
      break;
    case onk_plus_assign:       /* "+=" */
      name = MANGLING_STRING_FOR_OPERATOR_PLUS_ASSIGN;
      break;
    case onk_minus_assign:      /* "-=" */
      name = MANGLING_STRING_FOR_OPERATOR_MINUS_ASSIGN;
      break;
    case onk_times_assign:      /* "*=" */
      name = MANGLING_STRING_FOR_OPERATOR_TIMES_ASSIGN;
      break;
    case onk_divide_assign:     /* "/=" */
      name = MANGLING_STRING_FOR_OPERATOR_DIVIDE_ASSIGN;
      break;
    case onk_remainder_assign:  /* "%=" */
      name = MANGLING_STRING_FOR_OPERATOR_REMAINDER_ASSIGN;
      break;
    case onk_excl_or_assign:    /* "^=" */
      name = MANGLING_STRING_FOR_OPERATOR_EXCL_OR_ASSIGN;
      break;
    case onk_and_assign:        /* "&=" */
      name = MANGLING_STRING_FOR_OPERATOR_AND_ASSIGN;
      break;
    case onk_or_assign:         /* "|=" */
      name = MANGLING_STRING_FOR_OPERATOR_OR_ASSIGN;
      break;
    case onk_shift_left:        /* "<<" */
      name = MANGLING_STRING_FOR_OPERATOR_SHIFT_LEFT;
      break;
    case onk_shift_right:       /* ">>" */
      name = MANGLING_STRING_FOR_OPERATOR_SHIFT_RIGHT;
      break;
    case onk_shift_right_assign:/* ">>=" */
      name = MANGLING_STRING_FOR_OPERATOR_SHIFT_RIGHT_ASSIGN;
      break;
    case onk_shift_left_assign: /* "<<=" */
      name = MANGLING_STRING_FOR_OPERATOR_SHIFT_LEFT_ASSIGN;
      break;
    case onk_eq:                /* "==" */
      name = MANGLING_STRING_FOR_OPERATOR_EQ;
      break;
    case onk_ne:                /* "!=" */
      name = MANGLING_STRING_FOR_OPERATOR_NE;
      break;
    case onk_le:                /* "<=" */
      name = MANGLING_STRING_FOR_OPERATOR_LE;
      break;
    case onk_ge:                /* ">=" */
      name = MANGLING_STRING_FOR_OPERATOR_GE;
      break;
    case onk_spaceship:         /* "<=>" */
      name = MANGLING_STRING_FOR_OPERATOR_SPACESHIP;
      break;
    case onk_and_and:           /* "&&" */
      name = MANGLING_STRING_FOR_OPERATOR_AND_AND;
      break;
    case onk_or_or:             /* "||" */
      name = MANGLING_STRING_FOR_OPERATOR_OR_OR;
      break;
    case onk_plus_plus:         /* "++" */
      name = MANGLING_STRING_FOR_OPERATOR_PLUS_PLUS;
      break;
    case onk_minus_minus:       /* "--" */
      name = MANGLING_STRING_FOR_OPERATOR_MINUS_MINUS;
      break;
    case onk_comma:             /* "," */
      name = MANGLING_STRING_FOR_OPERATOR_COMMA;
      break;
    case onk_arrow_star:        /* "->*" */
      name = MANGLING_STRING_FOR_OPERATOR_ARROW_STAR;
      break;
    case onk_arrow:             /* "->" */
      name = MANGLING_STRING_FOR_OPERATOR_ARROW;
      break;
    case onk_function_call:     /* "()" */
      name = MANGLING_STRING_FOR_OPERATOR_CALL;
      break;
    case onk_subscript:         /* "[]" */
      name = MANGLING_STRING_FOR_OPERATOR_SUBSCRIPT;
      break;
    case onk_question:          /* "?" */
      name = MANGLING_STRING_FOR_OPERATOR_QUESTION;
      break;
    case onk_await:             /* "co_await" */
      name = MANGLING_STRING_FOR_OPERATOR_AWAIT;
      break;
#if GNU_EXTENSIONS_ALLOWED
    case onk_gnu_min:           /* "<?" */
      name = MANGLING_STRING_FOR_OPERATOR_GNU_MIN;
      break;
    case onk_gnu_max:           /* ">?" */
      name = MANGLING_STRING_FOR_OPERATOR_GNU_MAX;
      break;
#endif /* GNU_EXTENSIONS_ALLOWED */
    default:
      unexpected_condition_str("mangled_operator_name: bad kind");
  }  /* switch */
  return name;
}  /* mangled_operator_name */


static a_const_char *mangled_expr_operator_name(an_expr_node_ptr expr,
                                                a_boolean        *bad_operator,
                                                a_boolean        *is_cast)
/*
Return the string used to mangle the operator in the indicated expression.
Operators that don't appear in mangled names should already have been stripped.
If the operator is unrecognized, return *bad_operator TRUE (this can only
happen when ABI_COMPATIBILITY_VERSION < 402 -- in later versions an assertion
is triggered if the operator can't be mangled).  If the operator is some type
of cast (which requires mangling of a type as well as an expression), return
*is_cast TRUE.  Note that in some cases, the returned string may be a
pointer to statically allocated storage, so the caller should copy the
returned string to an appropriate buffer before this routine is invoked again.
*/
{
  a_const_char   *name = NULL;
  an_opname_kind opkind = (an_opname_kind)onk_none;
  unsigned int   num_operands = 2;

  *bad_operator = FALSE;
  *is_cast = FALSE;
  check_assertion(is_operation_node(expr));
  switch (expr->variant.operation.kind) {
#ifdef MANGLING_STRING_FOR_OPERATOR_ADDRESS
    case eok_address_of:
      opkind = (an_opname_kind)onk_ampersand;
      num_operands = 1;
      break;
#endif /* ifdef MANGLING_STRING_FOR_OPERATOR_ADDRESS */
#ifdef MANGLING_STRING_FOR_OPERATOR_DEREFERENCE
    case eok_indirect:
      opkind = (an_opname_kind)onk_star;
      num_operands = 1;
      break;
#endif /* ifdef MANGLING_STRING_FOR_OPERATOR_DEREFERENCE */
    case eok_negate:
      opkind = (an_opname_kind)onk_minus;
      num_operands = 1;
      break;
    case eok_unary_plus:
      opkind = (an_opname_kind)onk_plus;
      num_operands = 1;
      break;
    case eok_not:
    case eok_vector_not:
      opkind = (an_opname_kind)onk_not;
      num_operands = 1;
      break;
    case eok_cast:
    case eok_ref_cast:
    case eok_base_class_cast:
    case eok_derived_class_cast:
    case eok_pm_base_class_cast:
    case eok_pm_derived_class_cast:
    case eok_lvalue_cast:
    case eok_bool_cast:
    case eok_box:
    case eok_unbox:
#if ABI_COMPATIBILITY_VERSION >= 402
#if IA64_ABI
      if (emulate_gnu_abi_bugs) {
        /* GNU doesn't distinguish between cast types. */
        name = MANGLING_STRING_FOR_CAST;
      } else
#endif /* IA64_ABI */
      /* Do not insert code here. */
      if (expr->is_static_cast) {
        name = MANGLING_STRING_FOR_STATIC_CAST;
      } else if (expr->variant.operation.is_const_cast) {
        name = MANGLING_STRING_FOR_CONST_CAST;
      } else if (expr->variant.operation.is_reinterpret_cast) {
        name = MANGLING_STRING_FOR_REINTERPRET_CAST;
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (expr->is_safe_cast) {
        name = MANGLING_STRING_FOR_SAFE_CAST;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      } else 
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
      /* Do not insert code here. */
      {
        name = MANGLING_STRING_FOR_CAST;
      }  /* if */
      *is_cast = TRUE;
      break;
    case eok_dynamic_cast:
    case eok_ref_dynamic_cast:
#if IA64_ABI
      if (emulate_gnu_abi_bugs) {
        /* GNU doesn't distinguish between cast types. */
        name = MANGLING_STRING_FOR_CAST;
      } else
#endif /* IA64_ABI */
      /* Do not insert code here. */
      {
        name = MANGLING_STRING_FOR_DYNAMIC_CAST;
      }  /* if */
      *is_cast = TRUE;
      break;
#if C99_IL_EXTENSIONS_SUPPORTED
    case eok_xconj:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case eok_complement:
      opkind = (an_opname_kind)onk_compl;
      num_operands = 1;
      break;
#if ABI_COMPATIBILITY_VERSION >= 402
    case eok_assign:
      opkind = (an_opname_kind)onk_assign;
      break;
    case eok_add_assign:
      opkind = (an_opname_kind)onk_plus_assign;
      break;
    case eok_subtract_assign:
      opkind = (an_opname_kind)onk_minus_assign;
      break;
    case eok_multiply_assign:
      opkind = (an_opname_kind)onk_times_assign;
      break;
    case eok_divide_assign:
      opkind = (an_opname_kind)onk_divide_assign;
      break;
    case eok_remainder_assign:
      opkind = (an_opname_kind)onk_remainder_assign;
      break;
    case eok_xor_assign:
      opkind = (an_opname_kind)onk_excl_or_assign;
      break;
    case eok_and_assign:
      opkind = (an_opname_kind)onk_and_assign;
      break;
    case eok_or_assign:
      opkind = (an_opname_kind)onk_or_assign;
      break;
    case eok_shiftr_assign:
      opkind = (an_opname_kind)onk_shift_right_assign;
      break;
    case eok_shiftl_assign:
      opkind = (an_opname_kind)onk_shift_left_assign;
      break;
    case eok_pre_incr:
#ifdef MANGLING_STRING_FOR_OPERATOR_PLUS_PLUS_PREFIX
      /* Use a special mangling for the prefix version if one is defined,
         otherwise fall through and use the generic operator version. */
      name = MANGLING_STRING_FOR_OPERATOR_PLUS_PLUS_PREFIX;
      break;
#endif /* ifdef MANGLING_STRING_FOR_OPERATOR_PLUS_PLUS_PREFIX */
    case eok_post_incr:
      opkind = (an_opname_kind)onk_plus_plus;
      break;
    case eok_pre_decr:
#ifdef MANGLING_STRING_FOR_OPERATOR_MINUS_MINUS_PREFIX
      /* Use a special mangling for the prefix version if one is defined,
         otherwise fall through and use the generic operator version. */
      name = MANGLING_STRING_FOR_OPERATOR_MINUS_MINUS_PREFIX;
      break;
#endif /* ifdef MANGLING_STRING_FOR_OPERATOR_MINUS_MINUS_PREFIX */
    case eok_post_decr:
      opkind = (an_opname_kind)onk_minus_minus;
      break;
    case eok_subscript:
      opkind = (an_opname_kind)onk_subscript;
      break;
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
    case eok_add:
#if ABI_COMPATIBILITY_VERSION >= 402
    case eok_padd:
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
      opkind = (an_opname_kind)onk_plus;
      break;
    case eok_subtract:
#if ABI_COMPATIBILITY_VERSION >= 402
    case eok_psubtract:
    case eok_pdiff:
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
      opkind = (an_opname_kind)onk_minus;
      break;
    case eok_multiply:
      opkind = (an_opname_kind)onk_star;
      break;
    case eok_divide:
      opkind = (an_opname_kind)onk_divide;
      break;
    case eok_eq:
    case eok_vector_eq:
      opkind = (an_opname_kind)onk_eq;
      break;
    case eok_ne:
    case eok_vector_ne:
      opkind = (an_opname_kind)onk_ne;
      break;
    case eok_gt:
    case eok_vector_gt:
      opkind = (an_opname_kind)onk_gt;
      break;
    case eok_lt:
    case eok_vector_lt:
      opkind = (an_opname_kind)onk_lt;
      break;
    case eok_ge:
    case eok_vector_ge:
      opkind = (an_opname_kind)onk_ge;
      break;
    case eok_le:
    case eok_vector_le:
      opkind = (an_opname_kind)onk_le;
      break;
    case eok_spaceship:
      opkind = (an_opname_kind)onk_spaceship;
      break;
#if GNU_EXTENSIONS_ALLOWED
    case eok_gnu_min:
      opkind = (an_opname_kind)onk_gnu_min;
      break;
    case eok_gnu_max:
      opkind = (an_opname_kind)onk_gnu_max;
      break;
#endif /* GNU_EXTENSIONS_ALLOWED */
    case eok_remainder:
      opkind = (an_opname_kind)onk_remainder;
      break;
    case eok_shiftl:
      opkind = (an_opname_kind)onk_shift_left;
      break;
    case eok_shiftr:
      opkind = (an_opname_kind)onk_shift_right;
      break;
    case eok_and:
      opkind = (an_opname_kind)onk_ampersand;
      break;
    case eok_or:
      opkind = (an_opname_kind)onk_or;
      break;
    case eok_xor:
      opkind = (an_opname_kind)onk_excl_or;
      break;
    case eok_comma:
      opkind = (an_opname_kind)onk_comma;
      break;
    case eok_land:
    case eok_vector_land:
      opkind = (an_opname_kind)onk_and_and;
      break;
    case eok_lor:
    case eok_vector_lor:
      opkind = (an_opname_kind)onk_or_or;
      break;
    case eok_question:
    case eok_vector_question:
      opkind = (an_opname_kind)onk_question;
      num_operands = 3;
      break;
#if ABI_COMPATIBILITY_VERSION >= 402
    case eok_pm_points_to_field:
    case eok_points_to_pm_func_ptr:
      opkind = (an_opname_kind)onk_arrow_star;
      break;
    case eok_pm_field:
    case eok_dot_pm_func_ptr:
      name = MANGLING_STRING_FOR_OPERATOR_DOT_STAR;
      break;
#if C99_IL_EXTENSIONS_SUPPORTED
    case eok_real_part:
      name = MANGLING_STRING_FOR_OPERATOR_REAL_PART;
      break;
    case eok_imag_part:
      name = MANGLING_STRING_FOR_OPERATOR_IMAG_PART;
      break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case eok_handle_to:
    case eok_handle_to_box:
      name = MANGLING_STRING_FOR_OPERATOR_HANDLE_TO;
      break;
    case eok_cli_subscript:
#if IA64_ABI
      { /* The mangling for this operation uses the IA-64 ABI vendor extended
           operator mangling which imposes a limit that the number of operands
           must fit in a single digit (i.e., be no greater than 9). */
        num_operands = (unsigned int)number_of_operands_in_list(
                                             expr->variant.operation.operands);
        if (num_operands > 9) num_operands = 9;
        switch (num_operands) {
          case 0:
            name = "v012clisubscript";
            break;
          case 1:
            name = "v112clisubscript";
            break;
          case 2:
            name = "v212clisubscript";
            break;
          case 3:
            name = "v312clisubscript";
            break;
          case 4:
            name = "v412clisubscript";
            break;
          case 5:
            name = "v512clisubscript";
            break;
          case 6:
            name = "v612clisubscript";
            break;
          case 7:
            name = "v712clisubscript";
            break;
          case 8:
            name = "v812clisubscript";
            break;
          case 9:
            name = "v912clisubscript";
            break;
          default_is_unexpected();
        }  /* switch */
      }
#else /* !IA64_ABI */
      name = MANGLING_STRING_FOR_OPERATOR_CLI_SUBSCRIPT;
#endif /* IA64_ABI */
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case eok_noexcept:
      name = MANGLING_STRING_FOR_OPERATOR_NOEXCEPT;
      break;
    case eok_splice:
      name = MANGLING_STRING_FOR_SPLICE;
      break;
    case eok_lvalue:                     /* Handled higher up */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case eok_assume:                     /* Handled higher up */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if C99_IL_EXTENSIONS_SUPPORTED
    case eok_fjadd:
    case eok_jfadd:
    case eok_fjsubtract:
    case eok_jfsubtract:
    case eok_jmultiply:
    case eok_jdivide:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    default:
#if ABI_COMPATIBILITY_VERSION >= 402
      /* Operators that don't occur in mangled names should have been stripped
         previously, so if we get here, we need a mangling for the operator. */
      unexpected_condition_str("mangled_expr_operator_name: bad operator");
#else /* ABI_COMPATIBILITY_VERSION < 402 */
      *bad_operator = TRUE;
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
  }  /* switch */
  if (name == NULL && !*bad_operator) {
    /* Convert opkind to a name. */
    name = mangled_operator_name(opkind, num_operands);
  }  /* if */
  return name;
}  /* mangled_expr_operator_name */

#if GNU_EXTENSIONS_ALLOWED

/*
A local structure to keep a list of "abi_tag" attribute values (ck_string
constants).
*/
typedef struct an_abi_tag_string *an_abi_tag_string_ptr;
typedef struct an_abi_tag_string {
  an_abi_tag_string_ptr
              next;     /* A pointer to the next entry. */
  a_constant_ptr
              constant; /* A pointer to a ck_string constant. */
} an_abi_tag_string;

STATIC_THREAD an_abi_tag_string_ptr
                avail_abi_tag_strings;
                        /* A list of available abi_tag entries. */
STATIC_THREAD a_constant_list_entry_ptr
                implicit_tag_list;
                        /* A list of string constants from abi_tag attributes
                           of enclosing inline namespace(s) and local routines
                           that apply to the entity being mangled.  Any abi_tag
                           that exists on this list precludes the abi_tag from
                           being an implicit abi_tag for the entity being
                           mangled. */

#if DO_IL_LOWERING

STATIC_THREAD a_routine_list_entry_ptr
                avail_rlep_entries;
                        /* A list of available rlep entries. */

static a_routine_list_entry_ptr alloc_rlep_entry(void)
/*
Get an rlep entry from a local pool or allocate one if necessary.
*/
{
  a_routine_list_entry_ptr rlep;

  if (avail_rlep_entries == NULL) {
    rlep = alloc_list_entry_for_routine();
  } else {
    rlep = avail_rlep_entries;
    avail_rlep_entries = rlep->next;
    rlep->next = NULL;
  }  /* if */
  return rlep;
}  /* alloc_rlep_entry */


static void free_rlep_list(a_routine_list_entry_ptr list)
/*
Return the list of rlep entries to the pool of available entries.
*/
{
  a_routine_list_entry_ptr rlep;

  if (avail_rlep_entries == NULL) {
    avail_rlep_entries = list;
  } else {
    for (rlep = avail_rlep_entries;
         rlep->next != NULL;
         rlep = rlep->next) {}
    rlep->next = list;
  }  /* if */
}  /* free_rlep_list */

#endif /* DO_IL_LOWERING */

static void add_abi_tag_mangling(an_attribute_ptr         ap,
                                 a_mangling_control_block *mctl)
/*
Add a mangled encoding for any GNU "abi_tag" attribute(s) as specified in
the attribute list.  Note that the mangled encoding is based on a sorted
list of all "abi_tag" attribute strings in the last "abi_tag" __attribute
(to match GNU's behavior).  For the IA-64 ABI case, use the (GNU-specific)
"B" suffix ("B" is already used as a prefix for EDG-specific module id
mangling).  For the Cfront case, use "__ab".  Note that the mangled encoding
is added as a prefix in the Cfront case and a suffix in the IA-64 ABI case
(as controlled by the caller).
*/
{
  an_abi_tag_string_ptr list = NULL, last = NULL, ptr;
  an_attribute_arg_ptr  aap;

  check_assertion(ap != NULL);
  for (; ap != NULL; ap = ap->next) {
    if (ap->kind != ak_abi_tag) {
      /* Ignore non-abi_tag attributes. */
    } else {
      for (aap = ap->arguments; aap != NULL; aap = aap->next) {
        an_abi_tag_string_ptr atsp, prev = NULL;
        check_assertion(aap->kind == (an_attribute_arg_kind)aak_constant &&
                        aap->variant.constant->kind ==
                                              (a_constant_repr_kind)ck_string);
        if (avail_abi_tag_strings != NULL) {
          atsp = avail_abi_tag_strings;
          avail_abi_tag_strings = atsp->next;
        } else {
          atsp = alloc_general_of_type(an_abi_tag_string);
        }  /* if */
        atsp->constant = aap->variant.constant;
        if (list == NULL) {
          /* First attribute on the list. */
          list = atsp;
          atsp->next = NULL;
        } else {
          /* Keep the abi_tag attributes in ascending order.  It is expected
             that the number of "abi_tag" strings will be small, so an in-line
             "sort" is used. */
          for (ptr = list; ptr != NULL; ptr = ptr->next) {
            if (strcmp(atsp->constant->variant.string.value,
                       ptr->constant->variant.string.value) <= 0) {
              break;
            }  /* if */
            prev = ptr;
          }  /* for */
          if (ptr == list) {
            atsp->next = list;
            list = atsp;
          } else {
            check_assertion(prev != NULL);
            prev->next = atsp;
            atsp->next = ptr;
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* for */
  /* Now that the abi_tag strings have been identified and sorted, add them
     to the mangled name. */
  for (ptr = list; ptr != NULL; ptr = ptr->next) {
#if IA64_ABI
    add_to_mangled_name('B', mctl);
#else /* !IA64_ABI */
    add_str_to_mangled_name("__ab", mctl);
#endif /* IA64_ABI */
    mangled_name_with_length(ptr->constant->variant.string.value, mctl);
    last = ptr;
  }  /* for */
  /* Return entries to the available list. */
  if (last != NULL) {
    last->next = avail_abi_tag_strings;
    avail_abi_tag_strings = list;
  }  /* if */
}  /* add_abi_tag_mangling */


STATIC_THREAD a_source_correspondence
                *ttt_scp_for_implicit_abi_tags;
                        /* Used by calculate_implicit_abi_tags during a
                           type traversal to store the source correspondence
                           for the IL entity whose implicit abi_tags are
                           being computed. */
STATIC_THREAD an_il_entry_kind
                ttt_kind_for_implicit_abi_tags;
                        /* Used by calculate_implicit_abi_tags during a
                           type traversal to store the IL entity kind
                           (either iek_routine or iek_variable). */
STATIC_THREAD a_boolean
                ttt_mark_value;
                        /* Used during the walk of entities in the mangled
                           signature to store the value to set the
                           entity_marked field to (TRUE during marking, FALSE
                           when un-marking). */

/*
Shorthand for a_type_tree_traversal_flag_set used during abi_tag processing.
Note that parameter types are now included when looking for abi_tags (and
weren't in initial implementations).
*/
#if ABI_COMPATIBILITY_VERSION >= 510
#define ABI_TAG_TTT_FLAGS                                                     \
  (TTT_SKIP_TYPEREFS | TTT_RETURN_TYPE | TTT_TEMPLATE_ARGS | TTT_PARAM_TYPES)
#else /* ABI_COMPATIBILITY_VERSION < 510 */
#define ABI_TAG_TTT_FLAGS                                                     \
  (TTT_SKIP_TYPEREFS | TTT_RETURN_TYPE | TTT_TEMPLATE_ARGS)
#endif /* ABI_COMPATIBILITY_VERSION >= 510 */

/*
Utility that returns TRUE if the given IL entity is "marked".  Note that for
the purposes of this test, a class template is considered "marked" if its
associated template is marked.  This is to catch cases like:
  X<int> f(X<double>);
where any abi_tags for "X" would not be implicitly added.
*/
#define entity_is_marked(scp, kind)                                           \
  ((scp)->entity_marked ? TRUE :                                              \
     ((kind) == iek_type &&                                                   \
      is_immediate_class_type((a_type_ptr)(scp)) &&                           \
      ((a_type_ptr)(scp))->variant.class_struct_union.is_template_class &&    \
      class_type_supp((a_type_ptr)(scp))->                                    \
                                 assoc_template->source_corresp.entity_marked))


/* Entities of interest for mangling during walk_parents traversal. */
#define MY_WP (WP_NAMESPACE | WP_TYPE | WP_ROUTINE)

static a_boolean abi_tag_is_on_list(a_constant_ptr            abi_tag_con,
                                    a_constant_list_entry_ptr list)
/*
Return TRUE if the string constant specified by abi_tag_con is on the specified
list.
*/
{
  a_constant_list_entry_ptr clep;
  a_boolean                 result = FALSE;

  for (clep = list; clep != NULL; clep = clep->next) {
    if (string_constants_are_the_same(abi_tag_con, clep->constant)) {
      result = TRUE;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* abi_tag_is_on_list */


static void add_abi_tag_attributes_to_list(an_attribute_ptr          ap,
                                           a_constant_list_entry_ptr *list)
/*
Add the string constants for the abi_tag attribute ap to the list pointed to
by *list.  Any constant that is already on the list is skipped.
*/
{
  an_attribute_arg_ptr  aap;

  check_assertion(ap->kind == ak_abi_tag);
  for (aap = ap->arguments; aap != NULL; aap = aap->next) {
    check_assertion(aap->kind == (an_attribute_arg_kind)aak_constant &&
                    aap->variant.constant->kind ==
                                              (a_constant_repr_kind)ck_string);
    if (!abi_tag_is_on_list(aap->variant.constant, *list)) {
      a_constant_list_entry_ptr clep = alloc_list_entry_for_constant();
      clep->next = *list;
      clep->constant = aap->variant.constant;
      *list = clep;
    }  /* if */
  }  /* for */
}  /* add_abi_tag_attributes_to_list */


static void apply_implicit_abi_tags_from_entity(
                                            a_source_correspondence      *scp,
                                            an_il_entry_kind             kind,
                                            a_walk_parents_control_block *wpcb)
/*
Called as a callback from walk_parents.  The given entity is part of the
signature.  If unmarked, apply any applicable abi_tags to the entity given
by ttt_scp_for_implicit_abi_tags, otherwise terminate the walk.
*/
{
  a_boolean has_gnu_abi_tag_attribute;

  if (entity_is_marked(scp, kind)) {
    /* No need to look any further. */
    wpcb->terminate = TRUE;
  } else {
#if DEBUG
/* GCOVR_EXCL_START */
    if (db_flag_is_set("abi_tag")) {
      (void)fputs("Considering unmarked entity ", f_debug);
      db_name(scp);
      (void)fputs("\n", f_debug);
    }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
    check_assertion(!entity_is_marked(scp, kind));
    if (kind == iek_type) {
      has_gnu_abi_tag_attribute = ((a_type_ptr)scp)->has_gnu_abi_tag_attribute;
    } else if (kind == iek_namespace) {
      has_gnu_abi_tag_attribute =
                             ((a_namespace_ptr)scp)->has_gnu_abi_tag_attribute;
    } else {
      check_assertion(kind == iek_routine &&
                      ((a_routine_ptr)scp)->storage_class !=
                                                   (a_storage_class)sc_static);
      has_gnu_abi_tag_attribute =
                               ((a_routine_ptr)scp)->has_gnu_abi_tag_attribute;
    }  /* if */
    if (has_gnu_abi_tag_attribute) {
      /* Apply any abi_tag attributes from scp as implicit abi_tag attributes
         for ttt_scp_for_implicit_abi_tags. */
      an_attribute_arg_ptr  aap;
      an_attribute_ptr      ap;
#if DEBUG
/* GCOVR_EXCL_START */
      if (db_flag_is_set("abi_tag")) {
        (void)fputs("Adding implicit abi_tags from ", f_debug);
        db_name(scp);
        (void)fputs("\n", f_debug);
      }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
      for (ap = scp->attributes; ap != NULL; ap = ap->next) {
        if (ap->kind == ak_abi_tag) {
          for (aap = ap->arguments; aap != NULL; aap = aap->next) {
            check_assertion(aap->kind == (an_attribute_arg_kind)aak_constant &&
                            aap->variant.constant->kind ==
                                              (a_constant_repr_kind)ck_string);
            /* Any implicit abi_tag attributes from enclosing routines
               or inline namespaces should not appear as implicit abi_tag
               attributes for this entity.  Go through the list and skip any
               that already appear in the mangled name. */
            if (abi_tag_is_on_list(aap->variant.constant, implicit_tag_list)) {
#if DEBUG
/* GCOVR_EXCL_START */
              if (db_flag_is_set("abi_tag")) {
                (void)fputs("Ignoring duplicate abi_tag\n", f_debug);
              }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
            } else {
              add_implicit_abi_tag_attribute(ttt_scp_for_implicit_abi_tags,
                                             ttt_kind_for_implicit_abi_tags,
                                             aap->variant.constant);
            }  /* if */
          }  /* for */
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
}  /* apply_implicit_abi_tags_from_entity */


static a_boolean ttt_add_implicit_abi_tags_for_type(
                                           a_type_ptr           type,
                                           ARG_UNUSED a_boolean *end_traversal)
/*
Called during a type traversal by calculate_implicit_abi_tags.  If the
type has not previously been marked as part of the mangled signature, and
the type has abi_tags, or may be in an inline namespace whose abi_tags apply,
then apply those abi_tags as implicit abi_tags to the entity described by
ttt_scp_for_implicit_abi_tags.
*/
{
  if (is_immediate_class_type(type) || is_immediate_enum_type(type)) {
    /* Only class and enum types can have abi_tag attributes. */
    if ((type->has_gnu_abi_tag_attribute || type->in_gnu_abi_tag_namespace)) {
      /* This type has an explicit abi_tag or some parent that is an inline
         namespace with an abi_tag attribute. */
      a_walk_parents_control_block wpcb;
      walk_parents(&type->source_corresp, iek_type,
                   apply_implicit_abi_tags_from_entity, &wpcb,
                   MY_WP | WP_SELF);
    }  /* if */
  }  /* if */
  return FALSE;
}  /* ttt_add_implicit_abi_tags_for_type */


static void calculate_implicit_abi_tags_for_routine(a_routine_ptr routine);

static void mark_entry(a_source_correspondence      *scp,
                       an_il_entry_kind             kind,
                       a_walk_parents_control_block *wpcb);


static a_boolean ttt_mark_entry(a_type_ptr           type,
                                ARG_UNUSED a_boolean *end_traversal)
/*
Called during a type traversal from mark_entry to set the entity_marked field
of the specified type to ttt_mark_value.
*/
{
  a_walk_parents_control_block wpcb;

#if DEBUG
/* GCOVR_EXCL_START */
  if (db_flag_is_set("abi_tag")) {
    (void)fprintf(f_debug, "%s type ", ttt_mark_value ? "Marking" :
                                                        "Unmarking");
    db_name(&type->source_corresp);
    (void)fputs("\n", f_debug);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  type->source_corresp.entity_marked = ttt_mark_value;
  if (is_immediate_class_type(type)) {
    a_class_type_supplement_ptr   ctsp = class_type_supp(type);
    if (type->variant.class_struct_union.is_template_class) {
      /* Also mark the template class to catch cases like:
           X<int> f(X<double>);
         where X has an abi_tag.  The abi_tag for X<int> does not appear
         in the implicit abi_tags for "f" in this case. */
      check_assertion(ctsp->assoc_template != NULL);
      ctsp->assoc_template->source_corresp.entity_marked = ttt_mark_value;
    }  /* if */
  }  /* if */
  /* Walk the parents of this entry (the entry itself is marked here to
     prevent an unbounded loop). */
  walk_parents(&type->source_corresp, iek_type, mark_entry, &wpcb, MY_WP);
  return FALSE;
}  /* ttt_mark_entry */


static void mark_entry(a_source_correspondence                 *scp,
                       an_il_entry_kind                        kind,
                       ARG_UNUSED a_walk_parents_control_block *wpcb)
/*
Called as a callback from walk_parents.  Mark (or unmark, depending on the
value of ttt_mark_value) the IL entity specified by scp and kind, as well as
any related entities that would appear in the mangled name of the entity.
*/
{
  if (scp->entity_marked == ttt_mark_value) {
    /* Entity is already marked (or unmarked). */
  } else {
    scp->entity_marked = ttt_mark_value;
#if DEBUG
/* GCOVR_EXCL_START */
    if (db_flag_is_set("abi_tag")) {
      (void)fprintf(f_debug, "%s entity ", ttt_mark_value ? "Marking" :
                                                            "Unmarking");
      db_name(scp);
      (void)fputs("\n", f_debug);
    }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
    if (scp == ttt_scp_for_implicit_abi_tags) {
      /* If we're marking the entity for which we're computing the implicit
         abi_tags, don't mark that in the signature. */
    } else if (kind == iek_routine) {
      a_routine_ptr rp = (a_routine_ptr)scp;
      if (!rp->is_template_function) {
        /* The mangled name will incorporate an encoding for this local
           function.  Any implicit abi_tags that apply to the function should
           not appear in the entity's mangled name.  If the function has
           implicit abi_tags, add the function to a list so its abi_tags can be
           checked later.  Note that the abi_tags for the function have
           been previously calculated in most cases, but calculate them
           now if they haven't been. */
        if (!rp->implicit_abi_tags_added) {
          calculate_implicit_abi_tags_for_routine(rp);
        }  /* if */
        if (scp->attributes != NULL &&
            scp->attributes->is_implicit_abi_tag_attribute) {
          add_abi_tag_attributes_to_list(scp->attributes, &implicit_tag_list);
        }  /* if */
      }  /* if */
    } else if (kind == iek_type) {
      /* Also include any types referenced in this type (e.g., "A*"), but
         also class templates. */
      (void)traverse_type_tree((a_type_ptr)scp, ttt_mark_entry,
                               ABI_TAG_TTT_FLAGS);
#if ABI_COMPATIBILITY_VERSION >= 510
    } else if (kind == iek_namespace &&
               ((a_namespace_ptr)(scp))->is_inline) {
      /* The entity being mangled is contained within an inline namespace.
         Record any abi_tag attributes on this namespace so they will be
         suppressed. */
      an_attribute_ptr ap;
      for (ap = scp->attributes; ap != NULL; ap = ap->next) {
        if (ap->kind == ak_abi_tag) {
          add_abi_tag_attributes_to_list(ap, &implicit_tag_list);
        }  /* if */
      }  /* for */
#endif /* ABI_COMPATIBILITY_VERSION >= 510 */
    }  /* if */
  }  /* if */
}  /* mark_entry */


static void set_signature_mark(a_source_correspondence *scp,
                               an_il_entry_kind        kind,
                               a_boolean               mark)
/*
Identify every element of the "mangled signature" for scp and mark those
elements with the value specified by "mark".  scp can be either a variable or
a routine.
*/
{
  a_routine_ptr       rp;
  a_template_arg_ptr  template_arg = NULL;
  a_walk_parents_control_block wpcb;

  ttt_mark_value = mark;
  check_assertion(!scp->name_has_been_mangled);
  walk_parents(scp, kind, mark_entry, &wpcb, MY_WP | WP_SELF);
  if (kind == iek_variable) {
    /* For variables, only template arguments (for variable templates) appear
       as part of the signature. */
    a_variable_ptr vp = (a_variable_ptr)scp;
    if (vp->is_template_variable) {
      /* Get the template arguments for a variable template. */
      check_assertion(vp->template_info != NULL);
      template_arg = vp->template_info->template_arg_list;
    }  /* if */
  } else {
    /* For routines, the parameters and template arguments are part of the
       mangled signature. */
    a_param_type_ptr param;
    check_assertion(kind == iek_routine);
    rp = (a_routine_ptr)scp;
    template_arg = rp->template_arg_list;
    a_type_ptr rp_type = skip_typerefs(rp->type);
    for (param = rp_type->variant.routine.extra_info->param_type_list;
         param != NULL;
         param = param->next) {
      walk_parents(&param->type->source_corresp, iek_type, mark_entry,
                   &wpcb, MY_WP | WP_SELF);
    }  /* for */
  }  /* if */
  for (; template_arg != NULL; template_arg = template_arg->next) {
    if (is_type_templ_arg(template_arg)) {
      walk_parents(&template_arg->variant.type->source_corresp, iek_type,
                   mark_entry, &wpcb, MY_WP | WP_SELF);
    }  /* if */
  }  /* for */
}  /* set_signature_mark */


static void calculate_implicit_abi_tags(a_source_correspondence *scp,
                                        an_il_entry_kind        kind)
/*
Determine the implicit abi_tags that apply to the given IL entity (a variable
or routine -- as specified by "kind"), if any, and create a new abi_tag
attribute with their values.

The method for determining how to compute implicit abi_tags is not well
documented, consisting of a single sentence: "When a type involving an ABI tag
is used as the type of a variable or return type of a function where that tag
is not already present in the signature of the function, the tag is
automatically applied to the variable or function."

As implemented here, this involves three steps:

  1. Marking every entity that contributes to the "mangled signature" of
     the entity.
  2. Visiting all components of the type (return type for routines and variable
     type for variables) and adding implicit abi_tags for any component that
     is not marked in step 1 and has an explicit abi_tag.
  3. Re-visiting the entities in step 1 to reset the marks.
*/
{
#if ABI_COMPATIBILITY_VERSION >= 411
  a_variable_ptr      vp;
  a_routine_ptr       rp;
  a_type_ptr          tp;

  if (scp->attributes != NULL &&
      scp->attributes->is_implicit_abi_tag_attribute) {
    /* Implicit attributes need be computed only once.  In some cases (where
       mangling is performed early because of PCH files), it's possible that
       the implicit abi_tags have already been computed; if so, there's nothing
       to do here. */
  } else {
    if (kind == iek_variable) {
      vp = (a_variable_ptr)scp;
      tp = vp->type;
    } else {
      check_assertion(kind == iek_routine);
      rp = (a_routine_ptr)scp;
#if ABI_COMPATIBILITY_VERSION >= 415
      if ((clang_mode || gnu_version_is(>= 70000)) &&
          rp->special_kind == (a_special_function_kind)sfk_conversion) {
        /* Beginning with GCC 7.0.0, the abi_tags (if any) for the return type
           of a conversion function are omitted from the mangled name of the
           conversion routine.  This mimics clang's behavior. */
        tp = NULL;
      } else
#endif /* ABI_COMPATIBILITY_VERSION >= 415 */
      /* Do not insert code here. */
      {
        tp = rp->type->variant.routine.return_type;
      }  /* if */
    }  /* if */
    if (tp != NULL) {
      tp = skip_typedefs(tp);
      if (is_void_type(tp) || is_integral_type(tp) || is_floating_type(tp) ||
          is_void_star_type(tp)) {
        /* These types will never have abi_tag components, so skip the
           expensive processing. */
      } else {
        check_assertion(implicit_tag_list == NULL);
        /* Mark entities in the signature. */
        ttt_scp_for_implicit_abi_tags = scp;
        ttt_kind_for_implicit_abi_tags = kind;
        set_signature_mark(scp, kind, TRUE);
        /* Add implicit abi_tag attributes for the type. */
        (void)traverse_type_tree(tp, ttt_add_implicit_abi_tags_for_type,
                                 ABI_TAG_TTT_FLAGS);
        /* Unmark entries in the signature. */
        set_signature_mark(scp, kind, FALSE);
        ttt_scp_for_implicit_abi_tags = NULL;
        ttt_kind_for_implicit_abi_tags = iek_none;
        if (implicit_tag_list != NULL) {
          /* Free the list of implicit abi_tag constants. */
          free_list_of_constant_list_entries(implicit_tag_list);
          implicit_tag_list = NULL;
        }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
        if (db_flag_is_set("abi_tag")) {
          (void)fputs("Implicit abi_tags for ", f_debug);
          db_name(scp);
          (void)fputs(": ", f_debug);
          if (scp->attributes == NULL ||
              !scp->attributes->is_implicit_abi_tag_attribute) {
            (void)fputs("none\n", f_debug);
          } else {
            db_attribute(scp->attributes);
            (void)fputs("\n", f_debug);
          }  /* if */
        }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* ABI_COMPATIBILITY_VERSION >= 411 */
}  /* calculate_implicit_abi_tags */


static void calculate_implicit_abi_tags_for_routine(a_routine_ptr routine)
/*
Determine whether or not implicit abi_tags need to be calculated for the
given routine.  Calculate them if need be, but in either case, set a flag so
they aren't calculated more than once.
*/
{
  a_boolean mangle_as_template;

  if (gnu_abi_tag_attribute_seen && !routine->implicit_abi_tags_added) {
    routine->implicit_abi_tags_added = TRUE;
    mangle_as_template = (distinct_template_signatures &&
#if IA64_ABI
                          /* Member functions of template classes are not
                             considered templates for mangling purposes. */
                          routine->template_arg_list != NULL &&
#endif /* IA64_ABI */
                          routine->is_template_function);
    if (mangle_as_template) {
      /* Note that this processing is skipped for templates because the return
         type is included in the mangled name, so it's already part of the
         function's signature. */
    } else if (routine->storage_class == (a_storage_class)sc_static &&
               !routine_might_exist_in_multiple_copies(routine)) {
      /* GNU doesn't generate implicit abi_tags for local routines, except
         those with vague linkage. */
    } else if (routine->is_template_function && gnu_version < 60000) {
      /* Early versions didn't add implicit abi_tags for function templates. */
    } else {
      calculate_implicit_abi_tags(&routine->source_corresp, iek_routine);
    }  /* if */
  }  /* if */
}  /* calculate_implicit_abi_tags_for_routine */

#if DO_IL_LOWERING

static void wp_queue_routine(a_source_correspondence      *scp,
                             an_il_entry_kind             kind,
                             a_walk_parents_control_block *wpcb)
/*
Callback from walk_parents that queues the routine on a list pointed to by
wpcb->ptr if the routine has not yet had its implicit abi_tags calculated.
*/
{
  a_routine_list_entry_ptr rlep;
  a_routine_ptr routine = (a_routine_ptr)scp;

  check_assertion(kind == iek_routine);
  if (!routine->implicit_abi_tags_added) {
    rlep = alloc_rlep_entry();
    rlep->next = (a_routine_list_entry_ptr)wpcb->ptr;
    rlep->routine = routine;
    wpcb->ptr = (void *)rlep;
  }  /* if */
}  /* wp_queue_routine */


static void calculate_implicit_abi_tags_for_enclosing_routines(
                                                         a_routine_ptr routine)
/*
Calculates the implicit abi_tags for routine and all of the routines that
enclose routine (as necessary).
*/
{
  a_walk_parents_control_block wpcb;
  a_routine_list_entry_ptr rlep;

  /* Note that the list of routines must be visited from outermost to innermost
     so walk the parents to get a list of routines that need to be invoked,
     then invoke them in the proper order. */
  wpcb.ptr = NULL;
  walk_parents(&routine->source_corresp, iek_routine, wp_queue_routine,
               &wpcb, WP_ROUTINE | WP_SELF);
  for (rlep = (a_routine_list_entry_ptr)wpcb.ptr;
       rlep != NULL;
       rlep = rlep->next) {
    calculate_implicit_abi_tags_for_routine(rlep->routine);
  }  /* for */
  rlep = (a_routine_list_entry_ptr)wpcb.ptr;
  if (rlep != NULL) {
    free_rlep_list(rlep);
  }  /* if */
}  /* calculate_implicit_abi_tags_for_enclosing_routines */

#endif /* DO_IL_LOWERING */
#endif /* GNU_EXTENSIONS_ALLOWED */

static void mangled_function_base_name(
                                a_source_correspondence        *scp,
                                a_special_function_kind        special_kind,
                                an_opname_kind                 opname_kind,
                                ARG_UNUSED a_ctor_or_dtor_kind ctor_dtor_kind,
                                unsigned int                   num_operands,
                                a_type_ptr                     conversion_type,
                                a_const_char                   *ud_suffix,
                                a_mangling_control_block       *mctl)
/*
Add to the mangled name the encoding for the base name of the function
indicated by scp.  special_kind, opname_kind, ctor_dtor_kind, num_operands,
and conversion_type give additional information for special functions like
constructors and conversion functions.  When special_kind is sfk_udl_operator,
ud_suffix is a non-NULL string that specifies the suffix for the user-defined
literal operator.
*/
{
  a_const_char *name = NULL;
  a_type_ptr   inherited_ctor_base_class_type = NULL;

  if (special_kind == (a_special_function_kind)sfk_none
      || special_kind == (a_special_function_kind)sfk_lambda_entry_point
#if MICROSOFT_EXTENSIONS_ALLOWED
      || special_kind == (a_special_function_kind)sfk_idisposable_dispose
      || special_kind == (a_special_function_kind)sfk_dispose_bool
      || special_kind == (a_special_function_kind)sfk_object_finalize
      || special_kind == (a_special_function_kind)sfk_property_get
      || special_kind == (a_special_function_kind)sfk_property_set
      || special_kind == (a_special_function_kind)sfk_event_add
      || special_kind == (a_special_function_kind)sfk_event_remove
      || special_kind == (a_special_function_kind)sfk_event_raise
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                                                  ) {
    /* Normal name. */
    name = unmangled_or_fabricated_name_of(scp);
    check_assertion_str(name != NULL,
                        "mangled_function_base_name: unnamed routine");
#if IA64_ABI
    add_number_to_mangled_name((unsigned long)strlen(name), mctl);
#endif /* IA64_ABI */
  } else {
    /* Use a special name for the routine. */
#if !IA64_ABI
    /* Add leading underscores to the "special" name. */
    add_str_to_mangled_name("__", mctl);
#endif /* !IA64_ABI */
    switch (special_kind) {
#if MICROSOFT_EXTENSIONS_ALLOWED
      case sfk_static_constructor:
        name = MANGLING_STRING_FOR_STATIC_CONSTRUCTOR;
        break;
      case sfk_finalizer:
        name = MANGLING_STRING_FOR_FINALIZER;
        break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      case sfk_constructor:
        name = MANGLING_STRING_FOR_CONSTRUCTOR;
#if IA64_ABI
        { a_routine_ptr routine = (a_routine_ptr)scp;
          if (routine->is_inheriting_ctor) {
            if (ctor_dtor_kind == (a_ctor_or_dtor_kind)cdk_complete
#if !DO_IL_LOWERING
                || ctor_dtor_kind == (a_ctor_or_dtor_kind)cdk_none
#endif /* !DO_IL_LOWERING */
                                                                  ) {
              /* Use "CI1" for "complete object inheriting constructor"
                 (or in configurations where lowering isn't done). */
              name = MANGLING_STRING_FOR_INHERITING_CONSTRUCTOR "1";
            } else if (ctor_dtor_kind == (a_ctor_or_dtor_kind)cdk_subobject) {
              /* Use "CI2" for "base object inheriting constructor". */
              name = MANGLING_STRING_FOR_INHERITING_CONSTRUCTOR "2";
            } else {
              unexpected_condition();
            }  /* if */
            a_routine_ptr rout = get_inh_ctor_originator(routine);
            inherited_ctor_base_class_type = parent_class_of(rout);
          } else {
            switch (ctor_dtor_kind) {
              case cdk_none:                   break;
              case cdk_complete:               break;
              case cdk_subobject: name = "C2"; break;
              case cdk_delegation:name = "C9"; break;
              default:            unexpected_condition();
            }  /* switch */
          }  /* if */
        }
#endif /* IA64_ABI */
        break;
      case sfk_destructor:
        name = MANGLING_STRING_FOR_DESTRUCTOR;
#if IA64_ABI
        switch (ctor_dtor_kind) {
          case cdk_none:                   break;
          case cdk_complete:               break;
          case cdk_deleting:  name = "D0"; break;
          case cdk_subobject: name = "D2"; break;
          case cdk_delegation:name = "D9"; break;
          default:            unexpected_condition();
        }  /* switch */
#endif /* IA64_ABI */
        break;
      case sfk_conversion:
        name = MANGLING_STRING_FOR_CONVERSION_FUNC;
        /* Type signature is put out below. */
        break;
      case sfk_udl_operator:
        /* A literal operator (i.e., operator "") for user-defined literals. */
        check_assertion(ud_suffix != NULL && *ud_suffix != '\0');
        name = MANGLING_STRING_FOR_LITERAL_OPERATORS;
        /* The user-defined suffix for the literal operator is emitted
           below. */
        break;
      case sfk_operator:
        name = mangled_operator_name(opname_kind, num_operands);
        break;
      default:
        unexpected_condition_str(
                               "mangled_function_base_name: bad special kind");
    }  /* switch */
  }  /* if */
  /* Copy the name. */
  add_str_to_mangled_name(name, mctl);
  if (special_kind == (a_special_function_kind)sfk_conversion) {
    /* For a conversion function, add the type signature. */
    a_boolean save_mangle_auto_placeholder;
    check_assertion(conversion_type != NULL);
    save_mangle_auto_placeholder = mctl->mangle_auto_placeholder;
    mctl->mangle_auto_placeholder = TRUE;
    mangled_encoding_for_type(conversion_type, mctl);
    mctl->mangle_auto_placeholder = save_mangle_auto_placeholder;
  } else if (ud_suffix != NULL) {
    /* For a literal operator, add the ud-suffix to the mangled name. */
    mangled_name_with_length(ud_suffix, mctl);
  } else if (inherited_ctor_base_class_type != NULL) {
    /* For an inherited constructor, add the base class type. */
    mangled_type_name(inherited_ctor_base_class_type, mctl);
  }  /* if */
}  /* mangled_function_base_name */

#if GNU_FUNCTION_MULTIVERSIONING

/*
In configurations where periods are not allowed in mangled names, use
underscores instead.
*/
#if IA64_ABI
#if REPLACE_SPECIAL_CHARACTERS_IN_MANGLED_NAMES
#define MANGLING_MULTIVERSION_SEPARATOR '_'
#if USE_X86_FUNCTION_MULTIVERSIONING && DO_IL_LOWERING
#define MANGLING_SUFFIX_FOR_RESOLVER "_resolver"
#define MANGLING_SUFFIX_FOR_IFUNC "_ifunc"
#endif /* USE_X86_FUNCTION_MULTIVERSIONING && DO_IL_LOWERING */
#else /* !REPLACE_SPECIAL_CHARACTERS_IN_MANGLED_NAMES */
#define MANGLING_MULTIVERSION_SEPARATOR '.'
#if USE_X86_FUNCTION_MULTIVERSIONING && DO_IL_LOWERING
#define MANGLING_SUFFIX_FOR_RESOLVER ".resolver"
#define MANGLING_SUFFIX_FOR_IFUNC ".ifunc"
#endif /* USE_X86_FUNCTION_MULTIVERSIONING && DO_IL_LOWERING */
#endif /* REPLACE_SPECIAL_CHARACTERS_IN_MANGLED_NAMES */
#endif /* IA64_ABI */

#if USE_X86_FUNCTION_MULTIVERSIONING && IA64_ABI && DO_IL_LOWERING


char *mangled_resolver_name(a_routine_ptr representative)
/*
Return a suitable mangled name for the resolver function of a GNU multiversion
group (represented by the representative routine) and also create a suitable
mangled name for the ifunc routine.  The timing of the call to this routine
is very specific: the representative routine must already have been mangled
(with no multiversion decoration), but the representative routine itself
must already be lowered.  The returned string is in IL memory.

The IA-64 ABI doesn't specify how to mangle multiversion functions, but
through reverse-engineering it appears that the resolver function's mangled
name is simply the mangled name of the function with ".resolver" appended.

The mangling for the ifunc is somewhat more complicated.  It appears that
the name is created through a three step process: the function name is mangled,
".ifunc" is appended, then a new mangled name is created using the output
from the second step as the function's original name.  One other oddity: the
second mangling is performed using the "lowered type", e.g., if mangling
a member function, the second mangling will have a "this" parameter.

Note that the mangled names created here cannot be decoded (nor can those
generated by g++).
*/
{
  char                *ifunc_name, *result;
  a_const_char        *orig_mangled_name = representative->source_corresp.name;
  size_t              len = strlen(orig_mangled_name);
  a_name_linkage_kind save_linkage;
  a_special_function_kind
                      save_special_kind;

  check_assertion(representative->source_corresp.name_has_been_mangled &&
                  gnu_routine_supp(representative)->is_representative);
  /* The resolver name is the original name + ".resolver". */
  result = alloc_lowered_name_string(len +
                                     sizeof(MANGLING_SUFFIX_FOR_RESOLVER));
  (void)strcpy(result, orig_mangled_name);
  (void)strcpy(&result[len], MANGLING_SUFFIX_FOR_RESOLVER);
  /* The first step is to append ".ifunc" to the original name. */
  ifunc_name = alloc_lowered_name_string(len +
                                         sizeof(MANGLING_SUFFIX_FOR_IFUNC));
  (void)strcpy(ifunc_name, orig_mangled_name);
  (void)strcpy(&ifunc_name[len], MANGLING_SUFFIX_FOR_IFUNC);
  /* Now, generate a second mangled name for this function.  Since the
     function has been lowered, the linkage needs to be set to C++ linkage
     in order for the function to be mangled (there's no need to mangle
     functions with C linkage). */
  check_assertion(il_lowering_flag_of(representative->type));
  representative->source_corresp.name = ifunc_name;
  representative->source_corresp.name_has_been_mangled = FALSE;
  save_linkage = enum_cast<a_name_linkage_kind>(
                                  representative->source_corresp.name_linkage);
  representative->source_corresp.name_linkage = nlk_cplusplus_external;
  il_lowering_flag_of(representative->type) = FALSE;
  save_special_kind = representative->special_kind;
  representative->special_kind = sfk_none;
  mangle_function_name(representative, /*suppress_parent_encoding=*/TRUE);
  representative->source_corresp.name_linkage = save_linkage;
  il_lowering_flag_of(representative->type) = TRUE;
  representative->special_kind = save_special_kind;
  return result;
}  /* mangled_resolver_name */

#endif /* USE_X86_FUNCTION_MULTIVERSIONING && IA64_ABI && DO_IL_LOWERING */

static void add_mv_distinction(a_routine_ptr            routine,
                               a_mangling_control_block *mctl)
/*
If the specified routine is part of a GNU function multiversion group,
add an appropriate string to the current mangled name to identify it.
This routine is called at different points in the mangling of the name
depending on the ABI.  Note that due to the unique mangling constraints
that matching the GNU mangling presents, the resolver and ifunc mangled
names are generated at a different time (see mangled_resolver_name).
*/
{
#if USE_X86_FUNCTION_MULTIVERSIONING
  a_gnu_routine_supplement_ptr grsp = gnu_routine_supp(routine);
  if (grsp->is_target_specific_version) {
    /* All target-specific versions share the same unmangled name and must be
       differentiated somehow.  For compatibility reasons, use the same
       naming scheme as GNU (though that's not possible when using the
       C-generating back end because the names contain "."). */
    if (is_mv_default_routine(routine)) {
      /* No suffix is added for the "default" routine. */
    } else if (grsp->mv_info.targeted_version.representative == NULL) {
      /* Shouldn't happen (but does in some configurations when there are
         errors).  Mangled name doesn't really matter in this case, so don't
         emit anything. */
      check_assertion(is_at_least_one_error());
    } else if (has_exactly_one_target_specific_routine(
                              grsp->mv_info.targeted_version.representative)) {
      /* No suffix is added if there is only a single target-specific
         routine. */
    } else {
      /* Add a target-specific suffix at this point. */
      a_const_char *str = target_specific_distinction(routine);
#if IA64_ABI
      /* Add a separator before the target info. */
      add_to_mangled_name(MANGLING_MULTIVERSION_SEPARATOR, mctl);
      add_str_to_mangled_name(str, mctl);
#else /* !IA64_ABI */
      /* Add a target-specific prefix so the demangler knows what's coming. */
      add_str_to_mangled_name("__TGT__", mctl);
      add_str_to_mangled_name(str, mctl);
      /* Separate prefix from name. */
      add_str_to_mangled_name("__", mctl);
#endif /* IA64_ABI */
    }  /* if */
#if !IA64_ABI
  } else if (grsp->mv_resolver_required) {
    /* The ifunc that is created to invoke the resolver needs a mangled
       name. */
    check_assertion(grsp->is_representative && routine->is_ifunc);
    add_str_to_mangled_name("__IFC__", mctl);
#endif /* !IA64_ABI */
  }  /* if */
#endif /* USE_X86_FUNCTION_MULTIVERSIONING */
}  /* add_mv_distinction */

#endif /* GNU_FUNCTION_MULTIVERSIONING */

static void mangled_function_name(
                             a_routine_ptr            routine,
                             a_boolean                suppress_param_encoding,
                             a_boolean                suppress_parent_encoding,
                             ARG_UNUSED a_boolean     force_primary_name,
                             a_boolean                force_individuation,
                             ARG_UNUSED sizeof_t      *base_name_offset,
                             a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of the function "routine".
If suppress_param_encoding is TRUE, suppress the information on parameter
types; just put out the base encoded name.  If suppress_parent_encoding
is TRUE, suppress the parent qualifier for members of classes and
namespaces.  If force_primary_name is TRUE, use the primary entry point
name (instead of any alternate entry point) for a constructor or
destructor (for the IA-64 ABI).  If base_name_offset is not NULL,
*base_name_offset is set to the offset from the start of the mangling
to the point where the base name appears.  When force_individuation is TRUE,
the caller is requesting that the entity be individuated (presumably because
it's a static entity being used in a manner that requires differentiation from
similarly named entities in other translation units); typically that
determination is made by the callee.
*/
{
  a_type_ptr       conversion_type, routine_type;
  a_boolean        mangle_as_template;
#if !IA64_ABI
  a_boolean        is_member;
  a_boolean        is_specialization = FALSE;
  a_boolean        is_template_specialization = FALSE;
  a_boolean        needs_to_be_individuated;
#else /* IA64_ABI */
  a_boolean        need_nested_name_close = FALSE;
  a_template_ptr   tmpl = NULL;
  a_source_correspondence
                   *discriminator_scp = NULL;
#endif /* !IA64_ABI */
  unsigned int     num_operands;
  an_opname_kind   opname_kind = (an_opname_kind)onk_none;
  a_ctor_or_dtor_kind
                   ctor_dtor_kind = (a_ctor_or_dtor_kind)cdk_none;

  routine_type = skip_typerefs(routine->type);
  /* See if the function should be mangled as a template.  In the modern C++
     language, template functions are mangled using the template arguments
     and the prototype for the function.  This allows overloading of function
     templates (the instances have the same function parameter types, but
     one can be chosen over the other based on whether it is more
     specialized). */
  mangle_as_template = (distinct_template_signatures &&
#if IA64_ABI
                        /* Member functions of template classes are not
                           considered templates for mangling purposes. */
                        routine->template_arg_list != NULL &&
#endif /* IA64_ABI */
                        routine->is_template_function);
  if (mangle_as_template) {
    /* See if the function comes from a template and that template is
       specialized. */
    a_symbol_ptr sym = (a_symbol_ptr)(routine->source_corresp.assoc_info);
    if (sym->variant.routine.instance_ptr != NULL) {
      /* This function is an instance of a template. */
      a_symbol_ptr template_sym =
                               sym->variant.routine.instance_ptr->template_sym;
      a_template_symbol_supplement_ptr tssp =
                                  template_supplement_for_symbol(template_sym);
      check_assertion(tssp != NULL);
#if !IA64_ABI
      if (tssp->is_specific_definition) {
        /* The template is specialized. */
        is_template_specialization = TRUE;
      }  /* if */
#endif /* !IA64_ABI */
      /* Use the type of the prototype routine from the template as the
         routine type for the rest of the mangling. */
      /* Note that "routine" is not updated. */
      routine_type = tssp->variant.function.routine->type;
      routine_type = skip_typerefs(routine_type);
#if IA64_ABI
      tmpl = tssp->il_template_entry;
      if (add_substitution_if_available((char *)tmpl, iek_template,
                                        /*is_pack_expansion=*/FALSE, mctl)) {
        goto mangle_template;
      }  /* if */
#endif /* IA64_ABI */
    }  /* if */
#if !IA64_ABI
    /* See if the function itself is specialized (but not with the old
       syntax). */
    if (routine->is_specialized && !routine->specialized_with_old_syntax) {
      is_specialization = TRUE;
    }  /* if */
#endif /* !IA64_ABI */
  }  /* if */
#if IA64_ABI
  if (!suppress_parent_encoding) {
    /* Add a parent qualifier for a member if needed. */
    mangled_ia64_parent_qualifier(&routine->source_corresp, iek_routine,
                                  &need_nested_name_close, 
                                  &discriminator_scp,
                                  force_individuation,
                                  mctl);
  }  /* if */
  if (tmpl != NULL) {
    alloc_substitution((char *)tmpl, iek_template, /*is_pack_expansion=*/FALSE,
                       mctl);
  }  /* if */
#endif /* IA64_ABI */
  /* Put out the base name of the function. */
  conversion_type = NULL;
  if (routine->special_kind == (a_special_function_kind)sfk_conversion) {
    conversion_type = routine_type->variant.routine.return_type;
  }  /* if */
  num_operands = (unsigned int)number_of_parameters(routine);
  if (routine->special_kind == (a_special_function_kind)sfk_operator) {
    opname_kind = routine->variant.opname_kind;
  }  /* if */
#if IA64_ABI && DO_IL_LOWERING
  if (routine->special_kind == (a_special_function_kind)sfk_constructor ||
      routine->special_kind == (a_special_function_kind)sfk_destructor) {
    if (routine->primary_ctor_or_dtor == NULL &&
        routine->ctor_dtor_kind == cdk_none) {
      /* Set the kind if it hasn't been set yet. */
      set_primary_ctor_or_dtor_kind(routine);
    }  /* if */
    ctor_dtor_kind = enum_cast<a_ctor_or_dtor_kind>(routine->ctor_dtor_kind);
    if (force_primary_name) {
      /* Use the "C1" or "D1" primary entry point for a constructor or
         destructor. */
      ctor_dtor_kind = cdk_complete;
    }  /* if */
  }  /* if */
  if (base_name_offset != NULL) {
    *base_name_offset = mctl->length;
  }  /* if */
#endif /* IA64_ABI && DO_IL_LOWERING */
#if GNU_FUNCTION_MULTIVERSIONING && !IA64_ABI
  if (has_gnu_routine_supp(routine)) add_mv_distinction(routine, mctl);
#endif /* GNU_FUNCTION_MULTIVERSIONING && !IA64_ABI */
#if GNU_EXTENSIONS_ALLOWED
  /* If necessary, calculate the implicit abi_tags for this routine. */
  calculate_implicit_abi_tags_for_routine(routine);
#if !IA64_ABI
  if (routine->has_gnu_abi_tag_attribute) {
    /* The Cfront ABI adds "abi_tag" mangling as a prefix. */
    add_abi_tag_mangling(routine->source_corresp.attributes, mctl);
  }  /* if */
#endif /* !IA64_ABI */
#endif /* GNU_EXTENSIONS_ALLOWED */
  mangled_function_base_name(&routine->source_corresp, routine->special_kind,
                             opname_kind, ctor_dtor_kind,
                             num_operands, conversion_type,
                             ud_suffix_for_routine(routine), mctl);
#if IA64_ABI && GNU_EXTENSIONS_ALLOWED
  if (routine->has_gnu_abi_tag_attribute) {
    /* The IA-64 ABI adds "abi_tag" mangling as a suffix. */
    add_abi_tag_mangling(routine->source_corresp.attributes, mctl);
  }  /* if */
#endif /* IA64_ABI && GNU_EXTENSIONS_ALLOWED */
  if (mangle_as_template) {
#if IA64_ABI
mangle_template:
#else /* !IA64_ABI */
    if (is_template_specialization) {
      /* Put out an indication of the fact the template from which this
         function is generated is specialized. */
      mangled_specialization_indication(mctl);
    }  /* if */
#endif /* !IA64_ABI */
    if (routine->template_arg_list != NULL) {
      /* Put out the template arguments. */
      mangled_template_arguments(routine->template_arg_list,
                                 /*partial_spec=*/FALSE,
                                 /*old_form=*/FALSE,
                                 (a_name_reference_ptr)NULL,
                                 mctl);
    }  /* if */
#if !IA64_ABI
    if (is_specialization) {
      /* Put out an indication of the fact that this function is
         specialized. */
      mangled_specialization_indication(mctl);
    }  /* if */
#endif /* !IA64_ABI */
  }  /* if */
#if !IA64_ABI
  if (force_individuation) {
    needs_to_be_individuated = TRUE;
  } else {
    needs_to_be_individuated = entity_needs_to_be_individuated(
                                                      &routine->source_corresp,
                                                      iek_routine);
  }  /* if */
  /* See if the function is a class member function or a member of a
     namespace. */
  is_member = is_class_or_namespace_member(routine) &&
              !suppress_parent_encoding;
  /* If we will be adding the class or namespace name or the parameter types,
     put out two underscores to separate the function name from the rest. */
  if (is_member || needs_to_be_individuated || !suppress_param_encoding) {
    /* Add two underscores after the name. */
    add_str_to_mangled_name("__", mctl);
  }  /* if */
  if (is_member) {
    /* Put out the name of the class or namespace of which this function
       is a member. */
    r_mangled_parent_qualifier(&routine->source_corresp, iek_routine,
                               /*nesting_level=*/1,
                               needs_to_be_individuated,
                               (a_source_correspondence **)NULL, mctl);
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* If this function explicitly overrides a function, add the class of
       the overridden function (unless it's in a managed class). */
    if (routine->overridden_functions != NULL &&
        (is_class_or_namespace_member(routine) &&
         !is_immediate_managed_class_type(parent_class_of(routine)))) {
      a_routine_ptr  overridden_function =
                                     selectively_overridden_function(routine);
      if (overridden_function != NULL) {
        /* The encoding is O <type>. */
        add_to_mangled_name('O', mctl);
        mangled_type_name(parent_class_of(overridden_function), mctl);
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else if (!suppress_parent_encoding && needs_to_be_individuated) {
    /* Add individuation. */
    r_mangled_parent_qualifier(&routine->source_corresp, iek_routine,
                               /*nesting_level=*/1,
                               /*needs_to_be_individuated=*/TRUE,
                               (a_source_correspondence **)NULL, mctl);
  }  /* if */
#endif /* !IA64_ABI */
#if IA64_ABI
  close_ia64_nested_name(need_nested_name_close, discriminator_scp, mctl);
#if MICROSOFT_EXTENSIONS_ALLOWED
  /* If this function explicitly overrides a function, add the class of
     the overridden function (unless it's in a managed class). */
  if (routine->overridden_functions != NULL &&
      (is_class_or_namespace_member(routine) &&
       !is_immediate_managed_class_type(parent_class_of(routine)))) {
    a_routine_ptr  overridden_function =
                                     selectively_overridden_function(routine);
    if (overridden_function != NULL) {
      /* The encoding is Q <nested-name>.  This is an extension to the IA-64
         ABI spec. */
      add_to_mangled_name('Q', mctl);
      mangled_function_name(overridden_function,
                            /*suppress_param_encoding=*/TRUE,
                            /*suppress_parent_encoding=*/FALSE,
                            /*force_primary_name=*/TRUE,
                            /*force_individuation=*/FALSE,
                            /*base_name_offset=*/(sizeof_t *)NULL,
                            mctl);
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* !IA64_ABI */
  if (!suppress_param_encoding) {
    a_boolean do_return_type, save_mangle_auto_placeholder;
#if !IA64_ABI
    /* Put out the qualifiers on the function type (if applicable).  Only
       applicable to the Cfront ABI. */
    mangled_encoding_for_function_qualifiers(routine_type,
                                       routine->source_corresp.is_class_member,
                                       mctl);
#endif /* !IA64_ABI */
    /* Templates have their return types included. */
    do_return_type = mangle_as_template;
    if (routine->special_kind == (a_special_function_kind)sfk_constructor ||
#if ABI_COMPATIBILITY_VERSION >= 243
        routine->special_kind == (a_special_function_kind)sfk_conversion ||
#endif /* ABI_COMPATIBILITY_VERSION >= 243 */
        routine->special_kind == (a_special_function_kind)sfk_destructor) {
      /* No return type on constructors, destructors, or conversion
         functions. */
      do_return_type = FALSE;
    }  /* if */
    /* Output the function type, including the parameter types. */
    save_mangle_auto_placeholder = mctl->mangle_auto_placeholder;
    mctl->mangle_auto_placeholder = routine->has_deduced_return_type;
    mangled_encoding_for_function_type(routine_type, do_return_type,
                                       /*mangling_function_name=*/TRUE,
                                       mctl);
    mctl->mangle_auto_placeholder = save_mangle_auto_placeholder;
  }  /* if */
#if GNU_FUNCTION_MULTIVERSIONING && IA64_ABI
  if (has_gnu_routine_supp(routine)) add_mv_distinction(routine, mctl);
#endif /* GNU_FUNCTION_MULTIVERSIONING && IA64_ABI */
}  /* mangled_function_name */


static a_boolean function_name_mangling_needed(
                                        a_routine_ptr routine,
                                        a_boolean     *suppress_param_encoding)
/*
Return TRUE if the name of the indicated routine needs to be mangled.
If so, also return *suppress_param_encoding TRUE if the name should be
mangled without parameter encoding.
*/
{
  a_boolean mangling_needed = FALSE;

  *suppress_param_encoding = FALSE;
  /* All names except C external names must be mangled, because they might
     be overloaded.  All member function names must be mangled because
     they exist in a scope that does not exist in the C version of the
     program (of course, none of them have C external linkage, so no
     separate test is needed). */
  if (!has_name(routine)) {
    /* Unnamed routines generally do not need mangled names. */
    /* Compiler-generated routines have no name, and they are left alone.
       But all constructors (e.g., for unnamed classes) should get mangled
       names. */
    if (special_kind_is(routine, sfk_constructor)) {
      mangling_needed = TRUE;
    }  /* if */
  } else if (routine == il_header.main_routine) {
    /* Don't mangle "main" regardless of its linkage. */
  } else if (special_kind_is(routine, sfk_deduction_guide)) {
    /* Don't mangle deduction guides. */
  } else if (is_name_linkage_kind_subject_to_name_mangling(
                                       routine->source_corresp.name_linkage)) {
    /* Routines other than extern "C" routines need to be mangled. */
    mangling_needed = TRUE;
  } else if (routine->special_kind != (a_special_function_kind)sfk_none) {
    /* Operator function names must be somewhat mangled even if they are
       not C++ external, because their names are not normal C names --
       they contain special characters, etc. */
    mangling_needed = TRUE;
    *suppress_param_encoding = TRUE;
  }  /* if */
  return mangling_needed;
}  /* function_name_mangling_needed */

#if DO_IL_LOWERING

static void start_externalized_name(
                                ARG_UNUSED a_source_correspondence *scp,
                                ARG_UNUSED a_boolean               is_variable,
                                a_mangling_control_block           *mctl)
/*
Begin the output of the externalized mangled name for the entity with
the indicated source correspondence.  The entity is a variable if
is_variable is TRUE, a routine otherwise.
*/
{
#if !IA64_ABI
  char *prefix = (is_variable ? (char *)"__STV__" : (char *)"__STF__");

  /* The generated name has the form
       __STV__name__module_id  (variable)
       __STF__name__module_id  (function)
     Only the prefix is put out here.
  */
  add_str_to_mangled_name(prefix, mctl);
#else  /* IA64_ABI */
  /* The qualifier for an externalized name is
       B <length> <module-id>
     This is not in the ABI spec.  It's an EDG extension.  It can appear
     as a prefix to a name. */
  a_const_char *module_id = module_id_for_source_corresp(scp, mctl);
  check_assertion(!mctl->lacking_module_id);
  add_to_mangled_name('B', mctl);
  mangled_name_with_length(module_id, mctl);
#endif /* !IA64_ABI */
}  /* start_externalized_name */


/*lint -ecall(523,end_externalized_name)*/
static void end_externalized_name(ARG_UNUSED a_source_correspondence  *scp,
                                  ARG_UNUSED a_mangling_control_block *mctl)
/*
End the output of the externalized mangled name for the entity with
the indicated source correspondence.
*/
{
#if !IA64_ABI
  a_const_char *module_id;

  /* The generated name has the form
       __STV__name__module_id  (variable)
       __STF__name__module_id  (function)
     Only the part after "name" is put out here.
  */
  module_id = module_id_for_source_corresp(scp, mctl);
  check_assertion(!mctl->lacking_module_id);
  add_str_to_mangled_name("__", mctl);
  add_str_to_mangled_name(module_id, mctl);
#else /* IA64_ABI */
  /* No suffix required for IA-64 ABI. */
#endif /* !IA64_ABI */
}  /* end_externalized_name */


void externalize_mangled_name(a_source_correspondence  *scp,
                              a_boolean                is_variable)
/*
Replace the (already mangled) name for the entity referred to by scp
with an externalized name.  An externalized name is a name given
to a static entity when it is made external so that its name will remain
unique across the program.  The entity is a variable if is_variable
is TRUE, a routine otherwise.
*/
{
  a_mangling_control_block mctl;
  char                     *externalized_name;
  a_const_char             *name = scp->name;
  char                     buffer[50];
  a_source_correspondence  *module_scp = scp;
  sizeof_t                 name_len;

  check_assertion(!scp->externalized);
  /* This routine is called after name mangling has been done, and
     sometimes very late in the compilation (e.g., because of
     the needed-flag sweep in one-instantiation-per-object mode),
     where the entity name cannot be mangled again.  Therefore we
     must use the existing mangled name and modify it as necessary. */
#if CHECKING
  /* If the name needs to be mangled, the mangling should have been done
     already.  Note that that does not mean that the entity has been
     lowered yet: in secondary translation units statics get mangled
     and externalized (to get the right module id) before they are copied
     to the primary IL and lowered. */
  { a_boolean dummy;
    if (scp->name_has_been_mangled) {
      /* Okay, mangling already done. */
      /* Compression and truncation shouldn't have been done already,
         however. */
      check_assertion_str(!scp->mangled_name_cannot_be_included_in_other_name,
                       "externalize_mangled_name: mangled name already final");
    } else if (is_variable ?
                           variable_name_mangling_needed((a_variable_ptr)scp) :
                           function_name_mangling_needed((a_routine_ptr)scp,
                                                         &dummy)) {
#if DEBUG
/* GCOVR_EXCL_START */
      db_entity_info((char *)scp, is_variable ? iek_variable : iek_routine);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
      internal_error("externalize_mangled_name: name not mangled");
    }  /* if */
  }
#endif /* CHECKING */
  start_mangling(&mctl);
#if IA64_ABI
  add_mangled_name_prefix(&mctl);
#endif /* IA64_ABI */
  start_externalized_name(scp, is_variable, &mctl);
  if (name == NULL) {
    /* Entity has no name, e.g., a generated routine.  Generate one. */
    if (is_variable) {
      name = unmangled_or_fabricated_name_of_variable((a_variable_ptr)scp);
    }  /* if */
    if (name == NULL) {
      /* Generate a name. */
      (void)snprintf(buffer, sizeof(buffer), "%llu",
                    (unsigned long long)unique_id_for_il_pointer(scp));
      name = buffer;
    }  /* if */
  }  /* if */
#if IA64_ABI
  if (name[0] == '_' && name[1] == 'Z') {
    /* The name is mangled.  Skip the "_Z" prefix. */
    add_str_to_mangled_name(name+2, &mctl);
  } else {
    /* The name is not mangled (e.g., a variable).  Precede the name by
       its length. */
    mangled_name_with_length(name, &mctl);
  }  /* if */
#else /* !IA64_ABI */
  /* Cfront-like ABI. */
  add_str_to_mangled_name(name, &mctl);
#endif /* IA64_ABI */
  end_externalized_name(module_scp, &mctl);
  add_to_mangled_name('\0', &mctl);
  /* Copy the externalized name into the entity's source correspondence. */
  name_len = mangling_text_buffer->size - 1;
  externalized_name = alloc_lowered_name_string(name_len + 1);
  (void)strcpy(externalized_name, mangling_text_buffer->buffer);
#if IA64_ABI
  if (!is_variable) {
    a_routine_ptr rout = (a_routine_ptr)scp;
    if (rout->special_kind == (a_special_function_kind)sfk_constructor ||
        rout->special_kind == (a_special_function_kind)sfk_destructor) {
      /* Keep the base_name_offset up to date.  Assume that the change
         made to externalize the name is an insertion at the beginning
         of the name. */
      sizeof_t old_name_len = strlen(name);
#if CHECKING
      char     cdchar = (rout->special_kind ==
                         (a_special_function_kind)sfk_constructor ? 'C' : 'D');
      check_assertion(name[rout->variant.ctor_dtor.base_name_offset] ==
                                                                       cdchar);
#endif /* CHECKING */
      rout->variant.ctor_dtor.base_name_offset += name_len - old_name_len;
#if CHECKING
      check_assertion(externalized_name[
                          rout->variant.ctor_dtor.base_name_offset] == cdchar);
#endif /* CHECKING */
    }  /* if */
  }  /* if */
#endif /* IA64_ABI */
  scp->name = externalized_name;
  scp->externalized = TRUE;
  /* We're done with this mangling text buffer. */
  pop_mangling_text_buffer();
  return;
}  /* externalize_mangled_name */

#endif /* DO_IL_LOWERING */

static void mangled_function_name_externalized_if_necessary(
                             a_routine_ptr            routine,
                             a_boolean                suppress_param_encoding,
                             a_boolean                suppress_parent_encoding,
                             a_boolean                force_primary_name,
                             sizeof_t                 *base_name_offset,
                             a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of the function "routine".
If suppress_param_encoding is TRUE, suppress the information on parameter
types; just put out the base encoded name.  If suppress_parent_encoding
is TRUE, suppress the parent qualifier for members of classes and
namespaces.  If force_primary_name is TRUE, use the primary entry point
name (instead of any alternate entry point) for a constructor or
destructor (for the IA-64 ABI).  If base_name_offset is not NULL,
*base_name_offset is set to the offset from the start of the mangling
to the point where the base name appears.  If the routine will be
externalized, use the encoding for the externalized form.
*/
{
#if DO_IL_LOWERING
  a_boolean needs_to_be_externalized;

  /* Some static entities are potentially referenced from other translation
     units (e.g., because of exported templates) and therefore get
     externalized, which gives them a different kind of mangled name. */
  /* Note that if the name has been externalized already it fails the
     "should be externalized" test, but we still need to generate an
     externalized name here (and the lower-level routine will fetch the
     non-externalized name). */
  needs_to_be_externalized =
                routine->source_corresp.externalized ||
                routine_should_be_externalized_for_exported_templates(routine);
  if (needs_to_be_externalized) {
    start_externalized_name(&routine->source_corresp, /*is_variable=*/FALSE,
                            mctl);
  }  /* if */
#endif /* DO_IL_LOWERING */
  mangled_function_name(routine,
                        suppress_param_encoding,
                        suppress_parent_encoding,
                        force_primary_name,
                        /*force_individuation=*/FALSE,
                        base_name_offset,
                        mctl);
#if DO_IL_LOWERING
  if (needs_to_be_externalized) {
    end_externalized_name(&routine->source_corresp, mctl);
  }  /* if */
#endif /* DO_IL_LOWERING */
}  /* mangled_function_name_externalized_if_necessary */


#if TEMPLATE_LOOKUP_NEEDED || MICROSOFT_EXTENSIONS_ALLOWED || MODULE_ID_NEEDED

char *get_mangled_function_name_full(a_routine_ptr routine,
                                     a_boolean     force_primary_name,
                                     a_boolean     externalize_if_necessary)
/*
Get the mangled name for the indicated routine, and return a pointer
to it.  If the routine name has not been mangled yet, create a copy
of the mangled name in a temporary buffer but do not change the
name in the routine entry.  In the IA-64 ABI, if force_primary_name
is TRUE the routine is a constructor or destructor and the primary
entry point name should be returned.  If externalize_if_necessary is
TRUE, externalize the name (give it the name a static gets when
made into an external) if necessary.
*/
{
  a_mangling_control_block mctl;
  a_boolean                suppress_param_encoding = FALSE;
  char                     *mangled_name;
  a_boolean                needs_to_be_externalized = FALSE;

#if DO_IL_LOWERING
  /* Some static entities are potentially referenced from other translation
     units (e.g., because of exported templates) and therefore get
     externalized, which gives them a different kind of mangled name. */
  needs_to_be_externalized = externalize_if_necessary &&
                routine_should_be_externalized_for_exported_templates(routine);
#endif /* DO_IL_LOWERING */
  /* Coverity: Part of the test can't be reached when not doing IL lowering. */
  /* coverity[dead_error_line] */ /* coverity[dead_error_condition] */
  if ((routine->source_corresp.name_has_been_mangled &&
       !routine->source_corresp.final_name_mangling_pending &&
       (!needs_to_be_externalized || routine->source_corresp.externalized)) ||
      !function_name_mangling_needed(routine, &suppress_param_encoding)) {
    /* The name has already been (completely) mangled, or it doesn't need
       to be mangled, so just return it. */
    mangled_name = (char *)routine->source_corresp.name;
    /* The routine should not be unnamed. */
    check_assertion(mangled_name != NULL);
#if IA64_ABI && DO_IL_LOWERING
    if (force_primary_name) {
      /* Change the mangled name of a constructor or destructor to the
         complete-object version instead of the internal name (e.g.,
         "C1" in the mangled name instead of "C9"). */
      check_assertion(routine->special_kind == sfk_constructor ||
                      routine->special_kind == sfk_destructor);
      /* Copy the name to a mangling buffer so we can change it. */
      push_mangling_text_buffer();
      reset_text_buffer(mangling_text_buffer);
      add_to_text_buffer(mangling_text_buffer, mangled_name,
                         strlen(mangled_name)+1);
      mangled_name = mangling_text_buffer->buffer;
      a_ctor_or_dtor_kind save_ctor_dtor_kind =
                       enum_cast<a_ctor_or_dtor_kind>(routine->ctor_dtor_kind);
      routine->ctor_dtor_kind = cdk_complete;
      overwrite_ctor_dtor_mangled_name_kind(mangled_name, routine,
                                            ctor_dtor_kind_char(routine));
      routine->ctor_dtor_kind = save_ctor_dtor_kind;
      pop_mangling_text_buffer();
    }  /* if */
#endif /* IA64_ABI && DO_IL_LOWERING */
  } else {
    /* Generate the mangled name in a buffer. */
    start_mangling(&mctl, routine->is_prototype_instantiation);
    add_mangled_name_prefix(&mctl);
    /* Create the name. */
    if (externalize_if_necessary) {
      mangled_function_name_externalized_if_necessary(
                                            routine,
                                            suppress_param_encoding,
                                            /*suppress_parent_encoding=*/FALSE,
                                            force_primary_name,
                                            /*base_name_offset=*/(size_t*)NULL,
                                            &mctl);
    } else {
      mangled_function_name(
                                            routine,
                                            suppress_param_encoding,
                                            /*suppress_parent_encoding=*/FALSE,
                                            force_primary_name,
                                            /*force_individuation=*/FALSE,
                                            /*base_name_offset=*/(size_t*)NULL,
                                            &mctl);
    }  /* if */
#if IA64_ABI && DO_IL_LOWERING
    if (special_kind_is(routine, sfk_constructor) ||
        special_kind_is(routine, sfk_destructor)) {
      /* For a constructor or destructor, save the unique mangling character
         in case it needs to be used for differentiating alternate entry points
         if the mangled name is truncated. */
      mctl.ctor_dtor_char = ctor_dtor_kind_char(routine);
    }  /* if */
#endif /* IA64_ABI && DO_IL_LOWERING */
    mangled_name = end_mangling(/*final=*/TRUE, &mctl);
  }  /* if */
  return mangled_name;
}  /* get_mangled_function_name_full */


char *get_mangled_function_name(a_routine_ptr routine)
/*
Get the mangled name for the indicated routine, and return a pointer
to it.  If the routine name has not been mangled yet, create a copy
of the mangled name in a temporary buffer but do not change the
name in the routine entry.  In the IA-64 ABI, if the routine is
a constructor or destructor, return the primary entry point name.
*/
{
  char      *mangled_name;
  a_boolean force_primary_name = FALSE;

#if IA64_ABI
  if (routine->special_kind == (a_special_function_kind)sfk_constructor ||
      routine->special_kind == (a_special_function_kind)sfk_destructor) {
    /* Use the primary entry point name for constructor and destructors.
       Alternate entry points for non-inline template constructors and
       destructors are instantiated when the primary entry point is
       instantiated (there's only one entry on the instantiation list
       representing all the entry points). */
    force_primary_name = TRUE;
  }  /* if */
#endif /* IA64_ABI */
  mangled_name = get_mangled_function_name_full(routine, force_primary_name,
                                          /*externalize_if_necessary=*/TRUE);
  return mangled_name;
}  /* get_mangled_function_name */

#endif /* TEMPLATE_LOOKUP_NEEDED || MICROSOFT_EXTENSIONS_ALLOWED ||
          MODULE_ID_NEEDED */

static void add_variable_template_indication(a_variable_ptr           vp,
                                             a_mangling_control_block *mctl)
/*
If the specified variable is a variable template, add the template argument
list to the mangled name.
*/
{
  if (vp->is_template_variable &&
      vp->template_info->template_arg_list != NULL) {
    mangled_template_arguments(vp->template_info->template_arg_list,
                               /*partial_spec=*/FALSE,
                               /*old_form=*/FALSE,
                               (a_name_reference_ptr)NULL,
                               mctl);
  }  /* if */
}  /* add_variable_template_indication */


static void mangled_name_with_possible_qualification(
                                               a_source_correspondence   *scp,
                                               an_il_entry_kind          kind,
                                               ARG_UNUSED a_template_ptr tmpl,
                                               a_mangling_control_block  *mctl)
/*
Add to the mangled name the encoding for the name of the class, namespace
member, scoped enum type, or variable whose source correspondence is given by
scp and whose kind is given by "kind".  For the IA-64 ABI, if tmpl is non-NULL,
it specifies the template for a variable template.  This routine is called for
static data member variables, namespace member variables, some file scope
variables, scoped enumerators, and class and namespace member constants.  If
the entity is a namespace or class member, appropriate qualification is added
to the mangled name.
*/
{
  an_il_entity_list_entry_ptr sb_entity;
#if !IA64_ABI
  a_const_char *name;

  /* The mangled name of a static data member or member constant is the
     original name followed by two underscores followed by the mangled
     class name.  For example:
       AB::xy --> xy__2AB
     The same encoding is used for members of namespaces.
  */
  name = unmangled_or_fabricated_name_of(scp);
  if (name == NULL) {
    /* For an unnamed member, use the generated name.  This can happen for
       an anonymous union in a namespace. */
    name = scp->name;
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  if (kind == iek_variable &&
      ((a_variable_ptr)scp)->has_gnu_abi_tag_attribute) {
    /* The Cfront ABI adds "abi_tag" mangling as a prefix. */
    add_abi_tag_mangling(scp->attributes, mctl);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (kind == iek_variable &&
      struct_binding_container_needs_mangling((a_variable_ptr)scp)) {
    /* Give structured binding containers their own mangled name based on the
       binding variables it refers to.  Separate binding variable names with
       double underscores and add an extra double-underscore to mark the
       end. */
    add_str_to_mangled_name("__SBC__", mctl);
    for (sb_entity = ((a_variable_ptr)scp)->variant.bindings;
         sb_entity != NULL;
         sb_entity = sb_entity->next) {
      a_source_correspondence *sb_scp =
                               (a_source_correspondence*)sb_entity->entity.ptr;
      check_assertion(sb_entity->entity.kind == iek_variable &&
                      unmangled_name_of(sb_scp) != NULL);
      add_str_to_mangled_name(unmangled_name_of(sb_scp), mctl);
      add_str_to_mangled_name("__", mctl);
    }  /* for */
    add_str_to_mangled_name("__", mctl);
  } else if (kind == iek_variable &&
             ((a_variable_ptr)scp)->is_template_param_object) {
    /* Special mangling for a template parameter object. */
    add_str_to_mangled_name("__TPO__", mctl);
    check_assertion(((a_variable_ptr)scp)->init_kind ==
                                                   (an_init_kind)initk_static);
    mangled_encoding_for_constant(((a_variable_ptr)scp)->initializer.constant,
                                  /*old_form=*/FALSE,
                                  /*in_dependent_expr=*/FALSE,
                                  /*suppress_address_of=*/FALSE,
                                  mctl);
  } else {
    /* Copy the name. */
    check_assertion(name != NULL);
    add_str_to_mangled_name(name, mctl);
  }  /* if */
  if (kind == iek_variable) {
#if ABI_COMPATIBILITY_VERSION >= 520
    a_symbol_ptr  sym = (a_symbol_ptr)scp->assoc_info;
    if (sym != NULL &&
        (scp->is_local_to_function
#if DO_IL_LOWERING
         || (sym->kind == (a_symbol_kind)sk_variable &&
             sym->variant.variable.ptr->promoted_local_static)
#endif /* DO_IL_LOWERING */
                                                              )) {
      /* Add an indication that the variable is local to a function
         (in some error cases, no enclosing_routine is set). */
      if (scp->enclosing_routine != NULL) {
        add_local_name_suffix(sym->variant.variable.discriminator,
                              scp->enclosing_routine, mctl);
      } else {
        check_assertion(is_at_least_one_error());
      }  /* if */
    }  /* if */
#endif /* ABI_COMPATIBILITY_VERSION >= 520 */
    /* For variable templates, emit the template argument list now. */
    add_variable_template_indication((a_variable_ptr)scp, mctl);
  }  /* if */
  if (scp_is_class_or_namespace_member(scp) ||
      scp_is_enum_member(scp)) {
    if (scp->member_of_unknown_base) {
      /* We're pretending that we found the member in a dependent
         base class.  That means the original form of reference
         was unqualified.  Don't put out the parent qualifier. */
    } else {
      a_boolean is_specialization = FALSE;
      if (kind == iek_variable) {
        a_variable_ptr variable = (a_variable_ptr)scp;
        is_specialization = (variable->is_specialized &&
                             !variable->specialized_with_old_syntax);
      }  /* if */
      if (distinct_template_signatures && is_specialization) {
        /* Put out an indication of the fact that a static data member is
           specialized. */
        mangled_specialization_indication(mctl);
      }  /* if */
      /* Add two underscores after the name. */
      add_str_to_mangled_name("__", mctl);
      /* Output the mangled parent name. */
      mangled_parent_qualifier(scp, kind, mctl);
    }  /* if */
  } else {
    /* Entity needs no qualification (because it's not a member of a
       class or namespace). */
#if ABI_COMPATIBILITY_VERSION >= 402
    if (kind == iek_variable &&
        entity_needs_to_be_individuated(scp, iek_variable)) {
      /* A file-scope variable that needs to be to be individuated gets a
         special suffix. */
      add_str_to_mangled_name("__", mctl);
      r_mangled_parent_qualifier(scp, iek_variable,
                                 /*nesting_level=*/1,
                                 /*needs_to_be_individuated=*/TRUE,
                                 (a_source_correspondence **)NULL, mctl);
    }  /* if */
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
  }  /* if */
#else /* IA64_ABI */
  a_boolean               need_nested_name_close = FALSE;
  a_source_correspondence *discriminator_scp;
  /* Add a parent qualifier for a member if needed. */
  mangled_ia64_parent_qualifier(scp, kind,
                                &need_nested_name_close, 
                                &discriminator_scp,
                                /*force_individuation=*/FALSE,
                                mctl);
  if (kind == iek_variable &&
      struct_binding_container_needs_mangling((a_variable_ptr)scp)) {
    /* Structured binding containers get their own name mangling. */
    add_str_to_mangled_name("DC", mctl);
    for (sb_entity = ((a_variable_ptr)scp)->variant.bindings;
         sb_entity != NULL;
         sb_entity = sb_entity->next) {
      a_source_correspondence *sb_scp =
                               (a_source_correspondence*)sb_entity->entity.ptr;
      check_assertion(sb_entity->entity.kind == iek_variable &&
                      unmangled_name_of(sb_scp) != NULL);
      mangled_name_with_length(unmangled_name_of(sb_scp), mctl);
    }  /* for */
    add_to_mangled_name('E', mctl);
  } else if (kind == iek_variable &&
             ((a_variable_ptr)scp)->is_template_param_object) {
    /* Special "TA" mangling for a template parameter object.  We know the
       <template-arg> is a constant, so add the 'X'...'E' mangling here. */
    add_str_to_mangled_name("TAX", mctl);
    check_assertion(((a_variable_ptr)scp)->init_kind ==
                                                   (an_init_kind)initk_static);
    mangled_encoding_for_constant(((a_variable_ptr)scp)->initializer.constant,
                                  /*old_form=*/FALSE,
                                  /*in_dependent_expr=*/FALSE,
                                  /*suppress_address_of=*/FALSE,
                                  mctl);
    add_to_mangled_name('E', mctl);
  } else {
    /* Output the name of the member/variable. */
#if IA64_ABI && ABI_COMPATIBILITY_VERSION >= 510
    /* Variable templates are mangled as <unscoped-template-name> which
       requires allocating a substitution. */
    if (tmpl != NULL) {
      if (add_substitution_if_available((char *)tmpl, iek_template,
                                        /*is_pack_expansion=*/FALSE, mctl)) {
        goto skip_mangling;
      }  /* if */
    }  /* if */
#endif /* IA64_ABI && ABI_COMPATIBILITY_VERSION >= 510 */
    mangled_name_with_length(unmangled_or_fabricated_name_of(scp), mctl);
#if IA64_ABI && ABI_COMPATIBILITY_VERSION >= 510
    if (tmpl != NULL) {
      alloc_substitution((char *)tmpl, iek_template,
                         /*is_pack_expansion=*/FALSE, mctl);
    }  /* if */
#if ABI_COMPATIBILITY_VERSION >= 520
    { a_symbol_ptr  sym = (a_symbol_ptr)scp->assoc_info;
      if (!(sym != NULL &&
            sym->kind == (a_symbol_kind)sk_constant &&
            is_enum_constant(sym->variant.constant))) {
        /* In most cases, add a discriminator if appropriate, but suppress
           that for enumerator constants here (if necessary, the discrimination
           will be provided at a higher level). */
        add_discriminator_if_necessary(scp, mctl);
      }  /* if */
    }
#endif /* ABI_COMPATIBILITY_VERSION >= 520 */
skip_mangling:;
#endif /* IA64_ABI && ABI_COMPATIBILITY_VERSION >= 510 */
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  if (kind == iek_variable &&
      ((a_variable_ptr)scp)->has_gnu_abi_tag_attribute) {
    add_abi_tag_mangling(scp->attributes, mctl);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (kind == iek_variable) {
    /* For variable templates, emit the template argument list now. */
    add_variable_template_indication((a_variable_ptr)scp, mctl);
  }  /* if */
  close_ia64_nested_name(need_nested_name_close, discriminator_scp, mctl);
#endif /* !IA64_ABI */
}  /* mangled_name_with_possible_qualification */


static void mangled_variable_name_with_possible_qualification(
                                             a_variable_ptr           variable,
                                             a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of the variable
"variable" (a static data member, namespace member variable, or file scope
variable).
*/
{
  a_template_ptr                   tmpl = NULL;
#if IA64_ABI
  a_symbol_ptr                     sym, template_sym;
  a_template_symbol_supplement_ptr tssp;
#endif /* IA64_ABI */

  if (!has_name(variable) &&
      !struct_binding_container_needs_mangling(variable)) {
    /* An anonymous union can cause an unnamed member of a namespace:
         namespace {
           static union {float bf;};
         }
    */
    check_assertion(!variable->source_corresp.is_class_member);
    give_unnamed_member_variable_a_name(variable);
  }  /* if */
#if IA64_ABI
  sym = (a_symbol_ptr)(variable->source_corresp.assoc_info);
  if (sym != NULL &&
      sym->variant.variable.instance_ptr != NULL &&
      variable->is_template_variable &&
      variable->template_info->template_arg_list != NULL) {
    /* This is a variable template which (in the IA-64 ABI) requires a
       substitution, so pass the template pointer for that purpose. */
    template_sym = sym->variant.variable.instance_ptr->template_sym;
    tssp = template_supplement_for_symbol(template_sym);
    check_assertion(tssp != NULL);
    tmpl = tssp->il_template_entry;
  }  /* if */
#endif /* IA64_ABI */
  mangled_name_with_possible_qualification(&variable->source_corresp,
                                           iek_variable, tmpl, mctl);
}  /* mangled_variable_name_with_possible_qualification */


a_const_char *get_mangled_variable_name(a_variable_ptr variable)
/*
Get the mangled name for the indicated variable or static data member, and
return a pointer to it.  If the variable name has not been mangled yet,
create a copy of the mangled name in a temporary buffer but do not change
the name in the variable entry.
*/
{
  a_mangling_control_block mctl;
  a_const_char             *mangled_name;
  a_boolean                needs_to_be_externalized = FALSE;

#if DO_IL_LOWERING
  /* See if the variable is static and needs to be made external. */
  needs_to_be_externalized =
              variable_should_be_externalized_for_exported_templates(variable);
#endif /* DO_IL_LOWERING */
  /* Coverity: Part of the test can't be reached when not doing IL lowering. */
  /* coverity[dead_error_line] */ /* coverity[dead_error_condition] */
  if (variable->source_corresp.name_has_been_mangled &&
      !variable->source_corresp.final_name_mangling_pending &&
      (!needs_to_be_externalized || variable->source_corresp.externalized)) {
    /* The name has already been completely mangled, so just return it. */
    mangled_name = variable->source_corresp.name;
    /* The variable should not be unnamed. */
    check_assertion(mangled_name != NULL);
  } else {
    /* Generate the mangled name in a buffer. */
    start_mangling(&mctl, variable->is_prototype_instantiation);
    add_mangled_name_prefix(&mctl);
#if DO_IL_LOWERING
    if (needs_to_be_externalized) {
      start_externalized_name(&variable->source_corresp, /*is_variable=*/TRUE,
                              &mctl);
    }  /* if */
#endif /* DO_IL_LOWERING */
    mangled_variable_name_with_possible_qualification(variable, &mctl);
#if DO_IL_LOWERING
    if (needs_to_be_externalized) {
      end_externalized_name(&variable->source_corresp, &mctl);
    }  /* if */
#endif /* DO_IL_LOWERING */
    mangled_name = end_mangling(/*final=*/TRUE, &mctl);
  }  /* if */
  return mangled_name;
}  /* get_mangled_variable_name */


char *make_prefixed_object_name(a_const_char            *prefix,
                                a_source_correspondence *scp,
                                an_il_entry_kind        kind)
/*
Allocate (in the file scope) and return a string that incorporates the
given prefix along with the mangled name of the specific entity.
This is used in cases where a unique (and sometimes well-known) name is
needed for cases like a guard variable for a local static or the
wrapper functions for a thread_local variable.  This is also used to create
names for instantiation flag variables in some template instantiation modes.
Can be used in C mode (though that's not typical).
*/
{
  a_const_char              *mangled_name = NULL;
  char                      *prefixed_name;
  sizeof_t                  mangled_name_length, info_name_length;
  sizeof_t                  prefix_length, alloc_length;

  if (scp->name_has_been_mangled || C_mode()) {
    /* In many cases the object's name has already been mangled (e.g.,
       for a local static variable that has been promoted).  In C mode,
       there's no mangling, so just use the existing name. */
    mangled_name = scp->name;
  } else {
    if (kind == (an_il_entry_kind)iek_variable) {
      mangled_name = get_mangled_variable_name((a_variable_ptr)scp);
    } else if (kind == (an_il_entry_kind)iek_routine) {
      mangled_name = get_mangled_function_name((a_routine_ptr)scp);
    } else {
      unexpected_condition();
    }  /* if */
  }  /* if */
#if IA64_ABI
  if (mangled_name[0] == '_' && mangled_name[1] == 'Z') {
    /* Skip the '_Z' prefix. */
    mangled_name += 2;
  }  /* if */
#endif /* IA64_ABI */
  check_assertion(mangled_name != NULL);
  mangled_name_length = strlen(mangled_name);
  prefix_length = strlen(prefix);
  info_name_length = prefix_length + mangled_name_length;
  /* Allocate space for the prefixed name, including the final null. */
  alloc_length = info_name_length + 1;
  prefixed_name = alloc_lowered_name_string(alloc_length);
  /* Build the mangled name. */
  (void)strcpy(prefixed_name, prefix);
  (void)strcpy(prefixed_name+prefix_length, mangled_name);
  return prefixed_name;
}  /* make_prefixed_object_name */


static void do_local_name_mangling(
                      a_type_list_processing_routine_ptr list_mangling_routine)
/*
For local types in the current translation unit, call the indicated
mangling routine for type lists.  When orphan lists have been generated,
use them; otherwise, visit the local scopes from the routine scope.
*/
{
  process_local_types(il_header.primary_scope, list_mangling_routine);
}  /* do_local_name_mangling */

#if ONLY_MANGLE_TYPES_NEEDED_FOR_EXTERNAL_NAMES

static inline void make_address_based_identifier(a_source_correspondence *scp)
/*
Rather than mangling the specified IL entity to come up with a unique
identifier, use the entity's address as a unique identifier.  This is much
faster than mangling the entity and can be used in cases where a unique
identifier is required and the identifier does not need to be decoded.
*/
{
  char buffer[50];
  int  size = snprintf(buffer, sizeof(buffer), "__t%llX",
                       (unsigned long long)unique_id_for_il_pointer(scp));

  scp->unmangled_name_or_mangled_encoding = scp->name;
  scp->name = alloc_lowered_name_string((sizeof_t)(unsigned)size);
  (void)strcpy((char *)scp->name, buffer);
  scp->name_has_been_mangled = TRUE;
  scp->final_name_mangling_pending = FALSE;
}  /* make_address_based_identifier */

#endif /* ONLY_MANGLE_TYPES_NEEDED_FOR_EXTERNAL_NAMES */

static void mangle_type_name(a_type_ptr type)
/*
Mangle the name of the indicated type, if necessary.  Mangling is
necessary for nested types and for classes with mangled names.
This is done early for the Cfront-like ABI to generate a mangled
version of the type name that can be reused when building up
other mangled names, thus saving time.  The mangled form saved
is what mangled_type_name generates, plus a prefix.
*/
{
  a_source_correspondence_ptr scp = &type->source_corresp;

#if ONLY_MANGLE_TYPES_NEEDED_FOR_EXTERNAL_NAMES
  if (!scp->name_has_been_mangled) {
    if (type->kind == (a_type_kind)tk_typeref &&
        type->variant.typeref.predeclared) {
      /* Predeclared typerefs (e.g., for __int128_t) are kept as is. */
    } else {
      /* Use an address-based identifier for this type. */
      make_address_based_identifier(scp);
    }  /* if */
  }  /* if */
#else /* !ONLY_MANGLE_TYPES_NEEDED_FOR_EXTERNAL_NAMES */
  a_mangling_control_block mctl;

  clear_mangling_control_block(&mctl);
  if (!has_name(type) &&
      !type->source_corresp.name_has_been_mangled) {
    /* Give an unnamed class or enum a name if necessary.  This must be done
       early because in some cases it suppresses the need for a parent
       qualifier (name_has_been_mangled is set to TRUE).  If a module id
       is required (and not available), mctl.lacking_module_id will be set
       to TRUE (and mangling of this type will be postponed). */
    if ((is_immediate_class_type(type) || is_immediate_enum_type(type)) &&
        unnamed_type_has_no_discriminator(type)) {
      (void)give_unnamed_class_or_enum_a_name(type, &mctl);
    } else if (type->kind == tk_template_param &&
               type->variant.template_param.kind == tptk_member) {
      give_unnamed_template_param_member_a_name(type, &mctl);
    }  /* if */
  }  /* if */
  /* Generally speaking, types don't need to be given mangled names (they
     have specified encodings that are used when creating mangled names for
     other entities).  In certain cases though, mangled names are necessary
     to avoid collisions in generated C code.  For example, a class in
     a namespace can have the same name as a class at file scope and they
     need to be differentiated in lowered code. */
  /* do_type_name_mangling gets called twice, once from template processing
     and once from lowering itself.  Do nothing for names that have already
     been mangled on the previous call.  Type names can also have been
     previously mangled (in the Cfront ABI) when they are used as a component
     of another mangled name. */
  if (!type->source_corresp.name_has_been_mangled &&
      /* Skip types needing a module id for now. */
      !mctl.lacking_module_id &&
      /* Mangle nested types. */
      (entity_needs_parent_qualifier(scp, iek_type) ||
       /* Mangle types that need to be individuated. */
       entity_needs_to_be_individuated(scp, iek_type) ||
       /* Mangle unnamed types. */
       !has_name(type) ||
       /* Mangle template aliases. */
       (type->kind == (a_type_kind)tk_typeref &&
        is_typeref_kind(type, trk_is_template_alias)) ||
       /* Mangle class types with template arguments. */
       (is_immediate_class_type(type) &&
        type->variant.class_struct_union.extra_info->
                                                template_arg_list != NULL))) {
    if (is_immediate_class_type(type)) {
      start_mangling(&mctl,
                  type->variant.class_struct_union.is_prototype_instantiation);
    } else {
      start_mangling(&mctl);
    }  /* if */
#if IA64_ABI
    add_str_to_mangled_name("_Z", &mctl);
#else /*!IA64_ABI */
    add_str_to_mangled_name(PREFIX_ON_NESTED_TYPE_NAME, &mctl);
#endif /* IA64_ABI */
    mangled_type_name_full(type, /*check_for_subst=*/TRUE,
                           /*ok_to_mangle_type=*/FALSE, &mctl);
    /* Note final=FALSE to prevent compression and truncation at this
       time, so that the name can be reused.  final_entity_name_mangling
       will do the compression or truncation if necessary. */
    (void)end_mangling_full(scp, /*final=*/FALSE, &mctl);
  }  /* if */
#endif /* ONLY_MANGLE_TYPES_NEEDED_FOR_EXTERNAL_NAMES */
}  /* mangle_type_name */


static void do_type_list_type_name_mangling(a_type_ptr type_list)
/*
Do type name mangling for the types on the indicated type list and subscopes
thereunder.  Note that this does not include final processing like
compression and truncation.
*/
{
  a_type_ptr  type;
  a_scope_ptr class_scope;

  /* Visit all types on the list. */
  for (type = type_list; type != NULL; type = type->next) {
#if DO_IL_LOWERING
    /* Ignore types such as prototype instantiations. */
    if (ignore_type_in_back_end(type)) continue;
#else /* !DO_IL_LOWERING */
    if (is_immediate_class_type(type) &&
        type->variant.class_struct_union.is_nonreal_class &&
        !type->variant.class_struct_union.is_prototype_instantiation) {
      /* Ignore nonreal types that are not prototype instantiations.  These
         may contain "partially substituted" types that are used during the
         substitution and deduction process and whose template arguments
         may have inconsistent values. */
      continue;
    }  /* if */
#endif /* DO_IL_LOWERING */
    mangle_type_name(type);
    /* If the type is a class, process its scope. */
    if (is_immediate_class_type(type)) {
      a_class_type_supplement_ptr ctsp =
                                   type->variant.class_struct_union.extra_info;
      class_scope = ctsp->assoc_scope;
      if (!scope_is_null_or_placeholder(class_scope)) {
        do_type_list_type_name_mangling(class_scope->types);
      }  /* if */
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
      /* If some local types of member functions were promoted into the
         class on their way to the file scope, mangle them now too. */
      do_type_list_type_name_mangling(ctsp->promoted_local_types);
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
    }  /* if */
  }  /* for */
}  /* do_type_list_type_name_mangling */


static void do_scope_type_name_mangling(a_scope_ptr scope)
/*
Do name mangling for type names in the indicated scope (a file or
namespace scope) and all subscopes.  Note that this does not include
final processing like compression and truncation.
*/
{
  a_namespace_ptr nsp;

  /* Process the types in the scope. */
  do_type_list_type_name_mangling(scope->types);
  /* Process the namespaces in the scope. */
  for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
    if (!nsp->is_namespace_alias) {
      do_scope_type_name_mangling(nsp->variant.assoc_scope);
    }  /* if */
  }  /* for */
}  /* do_scope_type_name_mangling */


void do_type_name_mangling(void)
/*
Do name mangling for all type names.  Note that this does not include
final processing like compression and truncation.
*/
{
  /* Process the file scope and all subscopes in the file-scope memory
     region. */
  do_scope_type_name_mangling(il_header.primary_scope);
  /* Process local types. */
  do_local_name_mangling(do_type_list_type_name_mangling);
}  /* do_type_name_mangling */


static void mangle_member_constant_name(a_constant_ptr con)
/*
Mangle the name of the indicated member constant, if necessary.  con
is either an enumerator constant, a scoped enumerator constant, a namespace
member constant, or (as an extension) a declared class member constant.
*/
{
  a_mangling_control_block mctl;

  if (!con->source_corresp.name_has_been_mangled) {
    start_mangling(&mctl);
    /* Strictly speaking, the "_Z" prefix that is added in the IA-64 ABI by
       the call below could be any prefix that would cause the name to
       be unique (it had been "__").  Using "_Z" allows these names to
       be demangled and matches the mangling for promoted entities of the
       same type. */
    add_mangled_name_prefix(&mctl);
    mangled_name_with_possible_qualification(&con->source_corresp,
                                             iek_constant,
                                             (a_template_ptr)NULL, &mctl);
#if IA64_ABI
    if (scp_is_enum_member(&con->source_corresp) &&
        con->source_corresp.is_local_to_function &&
        !con->type->source_corresp.is_class_member) {
      /* A discriminator is necessary if this is a local scoped enumerator. */
      add_discriminator_if_necessary(&con->source_corresp, &mctl);
    }  /* if */
#endif /* IA64_ABI */
    (void)end_mangling_full(&con->source_corresp, /*final=*/TRUE, &mctl);
  }  /* if */
}  /* mangle_member_constant_name */


static void do_type_list_other_name_mangling(a_type_ptr type_list)
/*
Do name mangling for things other than classes (e.g., functions, static
data members) for the types on the indicated type list and subscopes
thereunder.  For the IA-64 ABI, this also does name mangling for types,
including classes.
*/
{
  a_type_ptr  type;
  a_scope_ptr class_scope;

  /* Visit all types on the list. */
  for (type = type_list; type != NULL; type = type->next) {
    /* If the type is a class, do its scope. */
    if (is_immediate_class_type(type) &&
        !ignore_type_in_back_end(type)) {
      a_class_type_supplement_ptr ctsp =
                                   type->variant.class_struct_union.extra_info;
      class_scope = ctsp->assoc_scope;
      if (!scope_is_null_or_placeholder(class_scope)) {
        do_scope_other_name_mangling(class_scope);
      }  /* if */
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
      /* If some local types of member functions were promoted into the
         class on their way to the file scope, mangle them now too. */
      do_type_list_other_name_mangling(ctsp->promoted_local_types);
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
    } else if (is_immediate_enum_type(type) &&
               (is_class_or_namespace_member(type) ||
                integer_type_is_scoped_enum(type))) {
      /* Mangle the names of member enum constants as well as scoped
         enum constants. */
      a_constant_ptr enum_con;
      for (enum_con = enum_constants(type);
           enum_con != NULL;
           enum_con = enum_con->next) {
        mangle_member_constant_name(enum_con);
      }  /* for */
    }  /* if */
  }  /* for */
}  /* do_type_list_other_name_mangling */


void mangle_function_name(a_routine_ptr routine,
                          a_boolean     suppress_parent_encoding)
/*
Mangle the name of the indicated function, if necessary.  Suppress the parent
encoding if suppress_parent_encoding is TRUE.
*/
{
  a_boolean                suppress_param_encoding;
  a_mangling_control_block mctl;
  sizeof_t                 *base_name_offset = NULL;

  /* Skip mangling of routines on the placeholder move list (this can happen
     if we're in a mangling pre-pass). */
  if (!routine->source_corresp.name_has_been_mangled &&
      function_name_mangling_needed(routine, &suppress_param_encoding) &&
      routine->source_corresp.name != routine_move_placeholder_name) {
    /* Mangle the function name. */
    start_mangling(&mctl, routine->is_prototype_instantiation);
    add_mangled_name_prefix(&mctl);
#if IA64_ABI && DO_IL_LOWERING
    if (routine->special_kind == (a_special_function_kind)sfk_constructor ||
        routine->special_kind == (a_special_function_kind)sfk_destructor) {
      base_name_offset = &routine->variant.ctor_dtor.base_name_offset;
    }  /* if */
#endif /* IA64_ABI && DO_IL_LOWERING */
    /* Note that this does not request the externalized version on purpose.
       The externalization part of the name is added later.  If we did
       externalize the name here, we'd have to do the other things that
       get done when a name is externalized. */
    mangled_function_name(routine,
                          suppress_param_encoding, 
                          suppress_parent_encoding,
                          /*force_primary_name=*/FALSE,
                          /*force_individuation=*/FALSE,
                          base_name_offset,
                          &mctl);
#if !IA64_ABI
    /* Note final=FALSE to prevent compression and truncation at this
       time, so that the name can be used in building names of types
       and variables promoted out of the routine.  do_final_name_mangling
       will do the compression or truncation if necessary. */
    (void)end_mangling_full(&routine->source_corresp, /*final=*/FALSE, &mctl);
#else /* IA64_ABI */
#if DO_IL_LOWERING
    /* In the IA64 ABI, the final mangled name should be computed
       immediately. */
    if (special_kind_is(routine, sfk_constructor) ||
        special_kind_is(routine, sfk_destructor)) {
      /* For a constructor or destructor, save the unique mangling character
         in case it needs to be used for differentiating alternate entry points
         if the mangled name is truncated. */
      mctl.ctor_dtor_char = ctor_dtor_kind_char(routine);
    }  /* if */
#endif /* DO_IL_LOWERING */
    (void)end_mangling_full(&routine->source_corresp, /*final=*/TRUE, &mctl);
#endif /* IA64_ABI */
  }  /* if */
}  /* mangle_function_name */


a_boolean variable_name_mangling_needed(a_variable_ptr variable)
/*
Return TRUE if the name of the indicated variable needs to be mangled.
Also determines any implicit abi_tags for the variable when mangling is needed.
*/
{
  a_boolean mangling_needed = FALSE;
#if ABI_COMPATIBILITY_VERSION >= 411 && GNU_EXTENSIONS_ALLOWED
  a_boolean file_scope_case = FALSE;
#endif /* ABI_COMPATIBILITY_VERSION >= 411 && GNU_EXTENSIONS_ALLOWED */

  if (variable->is_template_param_object) {
    /* Template parameter objects (unnamed) get their own mangling. */
    mangling_needed = TRUE;
  } else if (!has_name(variable)) {
    /* Unnamed variables do not need mangled names (except some structured
       binding containers are given mangled names). */
    if (struct_binding_container_needs_mangling(variable)) {
      mangling_needed = TRUE;
    }  /* if */
  } else if (!is_name_linkage_kind_subject_to_name_mangling(
                                      variable->source_corresp.name_linkage)) {
    /* Do not mangle namespace members with extern "C" linkage. */
  } else if (variable->is_template_variable) {
    /* Variable templates are mangled. */
    mangling_needed = TRUE;
  } else if (is_class_or_namespace_member(variable)) {
    /* Static data members and members of namespaces need mangled names. */
    mangling_needed = TRUE;
#if ABI_COMPATIBILITY_VERSION >= 411 && GNU_EXTENSIONS_ALLOWED
  } else {
    /* A file-scope variable that usually wouldn't need mangling, but it may
       have a GNU abi_tag attribute. */
    file_scope_case = TRUE;
  }  /* if */
  if (gnu_abi_tag_attribute_seen && (file_scope_case || mangling_needed)) {
    /* Generally speaking, file-scope variables do not need mangling, but they
       do if they have explicit or implicit abi_tags. */
    calculate_implicit_abi_tags(&variable->source_corresp, iek_variable);
    if (variable->has_gnu_abi_tag_attribute) {
      mangling_needed = TRUE;
    }  /* if */
#endif /* ABI_COMPATIBILITY_VERSION >= 411 && GNU_EXTENSIONS_ALLOWED */
  }  /* if */
  return mangling_needed;
}  /* variable_name_mangling_needed */


static void mangle_variable_name(a_variable_ptr variable)
/*
Mangle the name of the indicated static data member, namespace member variable,
or file scope variable with abi_tags (explicit or implicit).
*/
{
  a_mangling_control_block mctl;

  if (!variable->source_corresp.name_has_been_mangled &&
      variable_name_mangling_needed(variable)) {
    start_mangling(&mctl, variable->is_prototype_instantiation);
    add_mangled_name_prefix(&mctl);
    mangled_variable_name_with_possible_qualification(variable, &mctl);
    /* Note final=FALSE to prevent compression and truncation at this
       time, in case the name is externalized later.  do_final_name_mangling
       will do the compression or truncation if necessary. */
    (void)end_mangling_full(&variable->source_corresp, /*final=*/FALSE, &mctl);
  }  /* if */
}  /* mangle_variable_name */


void do_scope_other_name_mangling(a_scope_ptr scope)
/*
Do name mangling for things other than classes (e.g., functions, static
data members) in the indicated scope and its subscopes.  The scope is
a file, namespace, or class scope.  If the scope is the file scope,
function-local entities are also processed.  For the IA-64 ABI, this
also does type name mangling.
*/
{
  a_namespace_ptr nsp;
  a_routine_ptr   routine;
  a_variable_ptr  variable;
  a_constant_ptr  con;

  /* Visit all class and enum types. */
  do_type_list_other_name_mangling(scope->types);
  if (scope->kind == (a_scope_kind)sck_file) {
    /* When processing the file scope, also process function-local types. */
    do_local_name_mangling(do_type_list_other_name_mangling);
    /* Visit all constants.  This is generally useless, but there might be
       constants that were promoted out of a local class into the file
       scope. */
    /* Look for member constants (an extension in classes) and mangle their
       names. */
    for (con = scope->constants; con != NULL; con = con->next) {
      if (con->source_corresp.is_class_member &&
          !ignore_constant_in_back_end(con)) {
        mangle_member_constant_name(con);
      }  /* if */
    }  /* for */
    if (
#if GNU_EXTENSIONS_ALLOWED
        gnu_abi_tag_attribute_seen ||
#endif /* GNU_EXTENSIONS_ALLOWED */
        variable_templates_may_be_enabled) {
      /* Generally, file-scope variables are not mangled, but variable
         templates, variables with the GNU abi_tag attribute, and structured
         bindings require mangling. */
      for (variable = scope->variables;
           variable != NULL;
           variable = variable->next) {
        if (!variable->source_corresp.name_has_been_mangled) {
          mangle_variable_name(variable);
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  /* Visit all namespaces. */
  for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
    if (!nsp->is_namespace_alias) {
      a_mangling_control_block  mctl;
      clear_mangling_control_block(&mctl);
      /* Make sure that an unnamed namespace in a secondary translation unit
         is given a mangled name in that translation unit, so it has the
         right module id.  If the namespace contains only types, the name
         wouldn't otherwise be mangled at this time. */
      (void)give_unnamed_namespace_a_name(nsp, &mctl);
      if (mctl.lacking_module_id) {
        /* The name depends on a module id; this is okay when we're mangling
           early (e.g., before PCH file generation), but there's no need to
           mangle anything else in this unnamed namespace at this time (all of
           the manglings depend on a module id that hasn't been selected yet).
           */
        check_assertion(in_mangling_pre_pass);
      } else {
        do_scope_other_name_mangling(nsp->variant.assoc_scope);
      }  /* if */
    }  /* if */
  }  /* for */
  /* Visit all routines. */
  for (routine = scope->routines; routine != NULL; routine = routine->next) {
    if (!ignore_routine_in_back_end(routine)) {
      mangle_function_name(routine, /*suppress_parent_encoding=*/FALSE);
    }  /* if */
  }  /* for */
  if (scope->kind == (a_scope_kind)sck_class_struct_union ||
      scope->kind == (a_scope_kind)sck_namespace) {
    /* For a class or namespace scope, visit the member variables
       and constants. */
    /* Look for static data members/namespace member variables and mangle
       their names. */
    for (variable = scope->variables;
         variable != NULL;
         variable = variable->next) {
      mangle_variable_name(variable);
    }  /* for */
    /* Look for member constants (an extension in classes) and mangle their
       names. */
    for (con = scope->constants; con != NULL; con = con->next) {
      mangle_member_constant_name(con);
    }  /* for */
  }  /* if */
}  /* do_scope_other_name_mangling */


void do_all_name_mangling(a_boolean mangling_pre_pass)
/*
Do any required name mangling.  This is called at the beginning of lowering of
the file scope (mangling_pre_pass is FALSE) as well as before writing a PCH
file (mangling_pre_pass is TRUE).  It processes everything in the file scope
and also function-local entities that require mangling.  Final name mangling is
not done yet -- see do_final_name_mangling.
*/
{
  /* If we're doing the mangling pre-pass, set a flag that indicates that
     module ids are most likely not available and any names that depend on 
     them will not be mangled at this time. */
  check_assertion(!in_mangling_pre_pass);
  in_mangling_pre_pass = mangling_pre_pass;
  /* Mangle type names, not including final mangling.  This is done first
     so that the mangled names of classes can be used from the stored
     form (in the Cfront-like ABI) and not regenerated each time they are
     needed. */
  do_type_name_mangling();
  /* Do function, namespace, and static data member name mangling, not
     including some final mangling. */
  do_scope_other_name_mangling(il_header.primary_scope);
  in_mangling_pre_pass = FALSE;
}  /* do_all_name_mangling */


static void final_entity_name_mangling(a_source_correspondence *scp)
/*
Do any final name mangling processing required on the entity with
the indicated source correspondence.  This means checking for
compression and truncation.  Final name mangling is not needed in all
configurations.
*/
{
  if (scp->final_name_mangling_pending) {
    a_mangling_control_block mctl;
    a_const_char             *name = scp->name;
    sizeof_t                 length = strlen(name)+1;

    check_assertion(name != NULL && final_name_mangling_needed);
    /* One reason for calling start_mangling here is to zero
       mangling_text_buffer->size. */
    /* If neither compression nor truncation is done, the name pointer
       is passed through unchanged.  If compression is done, the compressed
       name is allocated in IL memory.  If truncation is done, the existing
       name is truncated in place. */
    start_mangling(&mctl);
    mctl.length = length;
#if !IA64_ABI
    name = compress_mangled_name(name, scp, &mctl);
#endif /* !IA64_ABI */
    name = truncate_mangled_name((char *)name, scp, &mctl);
    /* Signal that we're done using mangling_text_buffer. */
    pop_mangling_text_buffer();
    scp->name = name;
    scp->final_name_mangling_pending = FALSE;
  }  /* if */
}  /* final_entity_name_mangling */


static void do_scope_final_name_mangling(a_scope_ptr scope);


static void do_type_list_final_name_mangling(a_type_ptr type_list)
/*
Do final name mangling for the types on the indicated type list
and subscopes thereunder.  Functions and variables in the subscopes are
also processed.
*/
{
  a_type_ptr  type;
  a_scope_ptr class_scope;

  /* Visit all types on the list. */
  for (type = type_list; type != NULL; type = type->next) {
    /* Ignore types such as prototype instantiations. */
    if (ignore_type_in_back_end(type)) continue;
    /* If the type is a class, do its scope. */
    if (is_immediate_class_type(type)) {
      a_class_type_supplement_ptr ctsp =
                                   type->variant.class_struct_union.extra_info;
      class_scope = ctsp->assoc_scope;
      if (!scope_is_null_or_placeholder(class_scope)) {
        do_scope_final_name_mangling(class_scope);
      }  /* if */
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
      /* If some local types of member functions were promoted into the
         class on their way to the file scope, mangle them now too. */
      do_type_list_final_name_mangling(ctsp->promoted_local_types);
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
    }  /* if */
    /* Do name mangling on the type. */
    final_entity_name_mangling(&type->source_corresp);
  }  /* for */
}  /* do_type_list_final_name_mangling */


static void do_scope_final_name_mangling(a_scope_ptr scope)
/*
Do final name mangling for all type, function, and variable names in the
indicated scope (a file, namespace, or class scope) and all subscopes.
*/
{
  a_namespace_ptr nsp;
  a_routine_ptr   routine;
  a_variable_ptr  variable;

  /* Process the types in the scope. */
  do_type_list_final_name_mangling(scope->types);
  /* Process the namespaces in the scope. */
  for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
    if (!nsp->is_namespace_alias) {
      do_scope_final_name_mangling(nsp->variant.assoc_scope);
    }  /* if */
  }  /* for */
  /* Visit all routines. */
  for (routine = scope->routines; routine != NULL; routine = routine->next) {
    if (!ignore_routine_in_back_end(routine)) {
      final_entity_name_mangling(&routine->source_corresp);
    }  /* if */
  }  /* for */
  /* Visit all variables. */
  for (variable = scope->variables;
       variable != NULL;
       variable = variable->next) {
    final_entity_name_mangling(&variable->source_corresp);
  }  /* for */
}  /* do_scope_final_name_mangling */


void do_final_name_mangling(void)
/*
Do final name mangling for all type, function, and variable names.  This
must be done separately from and later than normal name mangling because
the simple form of the name must remain available for use in mangled names
(e.g., virtual function table variable names).  Final name mangling is needed
only in configurations where mangled names need to be compressed or
truncated; skip it otherwise.
*/
{
  if (final_name_mangling_needed) {
    /* Process the file scope and all subscopes in the file-scope memory
       region. */
    do_scope_final_name_mangling(il_header.primary_scope);
    /* Process local types. */
    do_local_name_mangling(do_type_list_final_name_mangling);
    check_assertion(mangling_buffers_in_use == NULL);
  }  /* if */
}  /* do_final_name_mangling */

#if ABI_COMPATIBILITY_VERSION >= 230 && CFRONT_OBJECT_CODE_COMPATIBILITY

static a_boolean base_class_of_same_name_exists(a_base_class_ptr orig_bcp)
/*
Return TRUE if in the base class list of which orig_bcp is a part there is
another base class with the same name.  Note that this is "same name," not
necessarily "same type."
*/
{
  a_boolean        same_name_exists = FALSE;
  a_base_class_ptr bcp;

  for (bcp = orig_bcp->derived_class->variant.class_struct_union.extra_info->
                                                                  base_classes;
       bcp != NULL;
       bcp = bcp->next) {
    if (bcp != orig_bcp) {
      a_const_char *bcp_name = unmangled_or_fabricated_name_of(
                                                   &bcp->type->source_corresp);
      a_const_char *orig_bcp_name = unmangled_or_fabricated_name_of(
                                              &orig_bcp->type->source_corresp);
      if (bcp_name != NULL && orig_bcp_name != NULL &&
          strcmp(bcp_name, orig_bcp_name) == 0) {
        /* Found another base class with the same name. */
        same_name_exists = TRUE;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return same_name_exists;
}  /* base_class_of_same_name_exists */

#endif /* ABI_COMPATIBILITY_VERSION >= 230 && ... */

static void mangled_derivation_name(a_derivation_path        path,
                                    a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of the indicated
derivation.  This is used for the base class part of virtual function
table names.
*/
{
  a_derivation_step_ptr  dsp = path.tail;

  /* The name must be put out backwards, so iterate through the list
     backwards. */
  do {
    /* Put out the name on the first derivation step. */
    a_type_ptr  class_type = dsp->base_class->type;
#if ABI_COMPATIBILITY_VERSION >= 230 && CFRONT_OBJECT_CODE_COMPATIBILITY
    /* cfront doesn't encode nested class information in base class names.
       This doesn't work in general, because it is possible to have base
       classes with the same basic name and different qualified names (e.g.,
       "A" and "A::B").  cfront doesn't seem to work right on all the cases
       with repeated basic names, so it gets away with it.  We use the
       cfront-compatible mangling only if there is no other base class
       with the same name. */
    if (!base_class_of_same_name_exists(dsp->base_class)) {
      mangled_basic_class_name(class_type, mctl);
    } else
#endif /* ABI_COMPATIBILITY_VERSION >= 230  && ... */
    /* Do not insert code here -- this is the "else" of an "if". */
    {
      /* Note the use of mangled_class_name_internal instead of
         mangled_vtbl_class_name because we do not want two lengths on
         the front of nested class names. */
      mangled_class_name_internal(class_type, mctl);
    }
#if !IA64_ABI
    if (dsp != path.head) {
      /* Add two underscores to separate names. */
      add_str_to_mangled_name("__", mctl);
    }  /* if */
#endif /* !IA64_ABI */
    dsp = dsp->prev;
  } while (dsp != path.head->prev);
}  /* mangled_derivation_name */


static a_boolean virtual_base_class_of_same_name_exists(
                                                      a_base_class_ptr dir_bcp)
/*
Return TRUE if in the base class list of which dir_bcp (a direct, nonvirtual
base class) is a part there is also a virtual base class of the same name.
*/
{
  a_boolean        same_name_exists = FALSE;
  a_base_class_ptr bcp;

  for (bcp = dir_bcp->derived_class->variant.class_struct_union.extra_info->
                                                                  base_classes;
       bcp != NULL;
       bcp = bcp->next) {
    if (bcp->is_virtual && same_entities(bcp->type, dir_bcp->type)) {
      same_name_exists = TRUE;
      break;
    }  /* if */
  }  /* for */
  return same_name_exists;
}  /* virtual_base_class_of_same_name_exists */


static long ambiguous_base_class_number(a_base_class_ptr bcp)
/*
Return an "ambiguous base class number" for the (ambiguous) base class bcp,
to be used in qualifying its name in a virtual function table mangled
name.  A return value of -1 indicates the base class that cfront discards,
which gets special treatment; otherwise, the value is non-negative.
*/
{
  long             num, count = -1;
  a_base_class_ptr test_bcp;
  a_type_ptr       class_type = bcp->derived_class;

  check_assertion(bcp->ambiguous);
  for (test_bcp = base_classes_of(class_type);
       ;
       test_bcp = test_bcp->next) {
    check_assertion(test_bcp != NULL);
    /* Count ambiguous base classes with the same name, in order. */
    if (test_bcp->ambiguous &&
        same_entities(test_bcp->type, bcp->type)) {
      if (bcp->direct && !bcp->is_virtual &&
          virtual_base_class_of_same_name_exists(bcp)) {
        /* This base class is a direct nonvirtual base class and there is
           a virtual base class with the same name.  cfront discards this
           base class (and therefore its virtual function table too), so
           this one is always qualified, even if it is the first one.
           That allows us to generate the same mangled name (an unqualified
           one) for the base class that cfront does keep (at least, if there
           is only one of those). */
        num = -1;
      } else {
        count++;
        num = count;
      }  /* if */
      /* Stop if we have found the base class we were looking for.  num is
         the number to use for it. */
      if (test_bcp == bcp) break;
    }  /* if */
  }  /* for */
  return num;
}  /* ambiguous_base_class_number */


static void mangled_vtbl_base_class_name(a_base_class_ptr         bcp,
                                         a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of a base class in
a virtual function table.  The name describes the base class given by bcp.
*/
{
#if !IA64_ABI
  a_length_reservation  length_reservation;
#endif /* !IA64_ABI */

  /* The form of the name is (for the Cfront-like ABI) like
       4abcd
     or
       8abcd__ef  (this for base class "abcd" in "ef")
     For virtual base classes, or nonvirtual base classes within virtual
     base classes, the first step is directly to the virtual base class.

     For the IA-64 ABI, the name is like
       4abcd2ef
     (This is not part of the ABI spec, because the names of the tables
     pointed to by the VTT are not prescribed.)
  */
#if !IA64_ABI
  /* Put out the name length. */
  reserve_space_for_length(&length_reservation, mctl);
#endif /* !IA64_ABI */
  /* Put out the sequence of base class names for the derivation. */
  mangled_derivation_name({ cast_derivation_path_of(bcp),
                            bcp->derivation->path_tail }, mctl);
#if !IA64_ABI
  fill_in_length(&length_reservation, mctl);
#endif /* !IA64_ABI */
  if (bcp->ambiguous) {
    /* Ambiguous base classes get a suffix to differentiate the different
       like-named base classes. */
    long num = ambiguous_base_class_number(bcp);
    if (num == 0) {
      /* The first ambiguous base class gets no suffix. */
    } else {
      add_str_to_mangled_name("__A", mctl);
      if (num < 0) {
        /* The base class that cfront discards (a direct nonvirtual base class
           with the same name as a virtual base class) gets the simple "__A"
           encoding.  This is for historical reasons: until version 3.0 of
           the EDG C++ Front End, this was the only ambiguity qualifier
           (we hadn't realized that there were other possibilities), so
           this one is kept the same to avoid an ABI change. */
      } else {
        /* For other base classes, use a __Ann encoding, where nn is the
           base class number. */
        add_number_to_mangled_name((unsigned long)num, mctl);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* mangled_vtbl_base_class_name */


static void mangled_vtbl_class_name(a_type_ptr               type,
                                    a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of the class "type"
for use in a virtual function table name.
*/
{
  /* When mangling the type, make sure we don't assign a mangled name to
     it (some code in define_typeinfo_var temporarily copies a name to a
     type_info type and then mangles it which can cause problems). */
#if ABI_COMPATIBILITY_VERSION >= 230 && CFRONT_OBJECT_CODE_COMPATIBILITY
  /* cfront mode. */
  if (entity_needs_parent_qualifier(&type->source_corresp, iek_type)) {
    /* The type is a nested type.  Add a length in front of the mangled
       form (e.g., "7Q2_1A1B" instead of "Q2_1A1B"). */
    a_length_reservation length_reservation;
    reserve_space_for_length(&length_reservation, mctl);
    mangled_type_name_full(type, /*check_for_subst=*/TRUE,
                           /*ok_to_mangle_type=*/FALSE, mctl);
    fill_in_length(&length_reservation, mctl);
  } else {
    /* Not a nested type name; just put out the type encoding. */
    mangled_type_name_full(type, /*check_for_subst=*/TRUE,
                           /*ok_to_mangle_type=*/FALSE, mctl);
  }  /* if */
#else /* ABI_COMPATIBILITY_VERSION < 230 || ... */
  /* In non-cfront mode, or in old ABI versions, just pass through to
     mangled_type_name_full. */
  mangled_type_name_full(type, /*check_for_subst=*/TRUE,
                         /*ok_to_mangle_type=*/FALSE, mctl);
#endif /* ABI_COMPATIBILITY_VERSION >= 230 && ... */
}  /* mangled_vtbl_class_name */


char *mangled_vtbl_name(a_type_ptr                  class_type,
                        a_base_class_ptr            bcp,
                        ARG_UNUSED a_base_class_ptr ctor_bcp)
/*
Return the mangled name for the virtual function table for base class
bcp of class class_type.  If bcp == NULL, the virtual function table is
for class_type itself.  If ctor_bcp is non-NULL, it is the base class
for class_type as a subobject of some larger class type that is the
actual complete object type (used in determining layout); class_type
in that case is the type considered to be the complete object type
for purposes of overriding (this is used during constructors and
destructors).  The name returned is in a temporary buffer and must
be copied elsewhere.
*/
{
  a_mangling_control_block mctl;
  char                     *buffer;

  start_mangling(&mctl);
  /* Determine the mangled name.  For the Cfront-like ABI, it is
       __vtbl__<mangled-base-class-name>__<mangled-class-name> or
       __vtbl__<mangled-class-name>
     The mangled-base-class-name is really a sort of pathname for the
     base class, giving the base class names from base to derived.
     For example, __vtbl__5X__X1__1B for base class X inside X1 inside
     a whole object of type B.

     For the IA-64 ABI, the prefix is "_ZTV", and the rest is the
     same as given above (modulo the different mangling of types).
     In the simple case (bcp == NULL and ctor_bcp == NULL), the prefix
     is followed by just a type name, as required by the ABI spec.
     In other cases, the virtual function table is a construction
     vtable, and the name is not dictated by the spec (so the form
     with multiple names that falls out of the Cfront-like ABI case
     is acceptable): the spec requires only that the VTT contain
     pointers to the construction vtables, but it doesn't require
     that the individual vtables pointed to have externally-known
     names.
  */
#if !IA64_ABI
  add_str_to_mangled_name("__vtbl__", &mctl);
#else /* IA64_ABI */
  add_mangled_name_prefix(&mctl);
  add_str_to_mangled_name("TV", &mctl);
  /* bcp should be non-NULL only for construction vtables. */
  check_assertion(ctor_bcp != NULL || bcp == NULL);
#endif /* IA64_ABI */
  if (bcp != NULL) {
    /* Add the base class name. */
    mangled_vtbl_base_class_name(bcp, &mctl);
    /* Add two underscores after the name. */
    add_str_to_mangled_name("__", &mctl);
  }  /* if */
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
  if (ctor_bcp != NULL) {
    /* There is a complete class type, so the name looks like
       __vtbl__<mangled-base-class-name>__<mangled-base-class-name>
                                        __<mangled-complete-class-name>
       Or, in the IA-64 ABI,
       _ZTV<mangled-base-class-name>__<mangled-base-class-name>
                                    __<mangled-complete-class-name>
    */
    /* Add the second base class name. */
    mangled_vtbl_base_class_name(ctor_bcp, &mctl);
    /* Add two underscores after the name. */
    add_str_to_mangled_name("__", &mctl);
    class_type = ctor_bcp->derived_class;
  }  /* if */
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
  /* Add the derived class name. */
  mangled_vtbl_class_name(class_type, &mctl);
  buffer = end_mangling(/*final=*/TRUE, &mctl);
  return buffer;
}  /* mangled_vtbl_name */


char *mangled_class_name(a_type_ptr type)
/*
Return the mangled name of the class "type".  This is the encoding used
for the name of the class as opposed to the encoding for the class as
a type (for example, it has no length preceding a simple class name).
The name returned is in a temporary buffer and must be copied elsewhere.
*/
{
  a_mangling_control_block mctl;
  char                     *buffer;

  start_mangling(&mctl);
  mangled_class_name_internal(type, &mctl);
  buffer = end_mangling(/*final=*/TRUE, &mctl);
  return buffer;
}  /* mangled_class_name */

#if !ONLY_MANGLE_TYPES_NEEDED_FOR_EXTERNAL_NAMES

void mangle_subobject_class_name(a_type_ptr class_type,
                                 a_type_ptr subobject_type)
/*
Set the mangled name of a type generated by IL lowering as the
type-as-subobject of class_type.
*/
{
  a_mangling_control_block mctl;
  char                     *temp_name, *new_name_ptr;

  if (has_name(class_type)) {
    start_mangling(&mctl);
    add_str_to_mangled_name("__SO__", &mctl);
    mangled_type_name_full(class_type, /*check_for_subst=*/TRUE,
                           /*ok_to_mangle_type=*/FALSE, &mctl);
    /* Not "final" because this type will go through the final processing
       later.  We don't want to (e.g.) compress twice. */
    temp_name = end_mangling(/*final=*/FALSE, &mctl);
    new_name_ptr = alloc_lowered_name_string((sizeof_t)strlen(temp_name)+1);
    (void)strcpy(new_name_ptr, temp_name);
    subobject_type->source_corresp.name = new_name_ptr;
    subobject_type->source_corresp.name_has_been_mangled = TRUE;
  }  /* if */
}  /* mangle_subobject_class_name */

#endif /* !ONLY_MANGLE_TYPES_NEEDED_FOR_EXTERNAL_NAMES */

static char *mangled_prefixed_type_encoding(a_const_char *prefix,
                                            a_type_ptr   type)
/*
Return a mangled name that is the indicated prefix followed by the encoding
for the indicated type.  The name returned is in a temporary buffer and must
be copied elsewhere.
*/
{
  a_mangling_control_block mctl;
  char                     *buffer;

  start_mangling(&mctl);
  add_mangled_name_prefix(&mctl);
  /* Start with the prefix. */
  add_str_to_mangled_name(prefix, &mctl);
  /* Add the mangled name of the type. */
  mangled_encoding_for_type(type, &mctl);
  buffer = end_mangling(/*final=*/TRUE, &mctl);
  return buffer;
}  /* mangled_prefixed_type_encoding */


char *mangled_typeinfo_name(a_type_ptr type)
/*
Return the mangled name for the typeinfo variable for type "type".
A typeinfo variable is used to describe runtime type information.
The name returned is in a temporary buffer and must be copied elsewhere.
*/
{
#if !IA64_ABI
  /* The mangled name looks like
       __T_<mangled-type-name>
  */
  return mangled_prefixed_type_encoding("__T_", type);
#else /* IA64_ABI */
  return mangled_prefixed_type_encoding("TI", type);
#endif /* IA64_ABI */
}  /* mangled_typeinfo_name */

#if !IA64_ABI

char *mangled_id_object_name(a_type_ptr type)
/*
Return the mangled name for the id object variable for type "type".
The id object variable is pointed to by the typeinfo variable used
to provide runtime type information.  The name returned is in
a temporary buffer and must be copied elsewhere.
*/
{
  /* The mangled name looks like
       __TID_<mangled-type-name>
  */
  return mangled_prefixed_type_encoding("__TID_", type);
}  /* mangled_id_object_name */

#else /* IA64_ABI */

char *mangled_typeinfo_string(a_type_ptr type)
/*
Return the mangled name for type, as suitable for using in a typeinfo string.
The name returned is in a temporary buffer and must be copied elsewhere.
*/
{
  a_mangling_control_block mctl;
  char                     *buffer;

  start_mangling(&mctl);
  /* Add the mangled name of the type. */
  mangled_encoding_for_type(type, &mctl);
  buffer = end_mangling(/*final=*/TRUE, &mctl);
  return buffer;
}  /* mangled_typeinfo_string */


char *mangled_typeinfo_string_name(a_type_ptr type)
/*
Return the mangled name for the typeinfo string variable for type "type".
The name returned is in a temporary buffer and must be copied elsewhere.
*/
{
  /* The mangled name looks like
       _ZTS<mangled-type-name>
  */
  return mangled_prefixed_type_encoding("TS", type);
}  /* mangled_typeinfo_string_name */


char *mangled_virtual_table_table_name(a_type_ptr type)
/*
Return the mangled name for the virtual table table variable for type
"type". The name returned is in a temporary buffer and must be copied
elsewhere.
*/
{
  /* The mangled name looks like
       _ZTT<mangled-type-name>
  */
  return mangled_prefixed_type_encoding("TT", type);
}  /* mangled_virtual_table_table_name */

#endif /* IA64_ABI */

#if DO_IL_LOWERING

#if !IA64_ABI

static unsigned long search_scope_list(a_scope_ptr scope,
                                       a_scope_ptr scope_to_search,
                                       a_boolean   *found)
/*
Look for "scope" in "scope_to_search".  If it is found, set *found to TRUE
and return the position where it was found: 0 means scope and scope_to_search
are the same scope; scopes under scope_to_search are numbered in tree
traversal order starting from 1.  If the scope is not found, *found is not
changed (it is expected to be FALSE) and the count of scopes in the tree is
returned.
*/
{
  unsigned long scope_number;

  if (scope == scope_to_search) {
    scope_number = 0;
    *found = TRUE;
  } else {
    a_scope_ptr sp;
    scope_number = 1;
    for (sp = scope_to_search->scopes; sp != NULL; sp = sp->next) {
      scope_number += search_scope_list(scope, sp, found);
      if (*found) break;
    }  /* for */
  }  /* if */
  return scope_number;
}  /* search_scope_list */

#endif /* !IA64_ABI */

void mangle_promoted_entity_name(a_source_correspondence *scp,
                                 an_il_entry_kind        kind,
                                 a_boolean               final,
                                 a_routine_ptr           routine,
                                 ARG_UNUSED a_scope_ptr  scope)
/*
scp points to the source correspondence field of an entity that is
being promoted out of the routine "routine" (or one of its block
scopes) to the file scope.  kind indicates the kind of entity (type,
constant, or variable; not routine).  scope indicates the scope out
of which the entity is being promoted (a function or block scope).
Give the entity a mangled name if necessary.  If final is TRUE, do
the final name mangling, which may produce a name that can no longer
be embedded in other mangled names.
*/
{
  a_mangling_control_block mctl;
  a_boolean                is_string = FALSE;
#if IA64_ABI || ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS
  unsigned long            sequence_number = 0;
#endif /* IA64_ABI || ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS */
  a_variable_ptr           var = (a_variable_ptr)scp;

  check_assertion(kind == iek_variable ||
                  kind == iek_constant ||
                  kind == iek_type);
  if (kind == iek_variable) {
    check_assertion(!var->is_template_variable);
    if (var->is_anonymous_parent_object) {
      /* Give an anonymous union variable a name based on the name of
         the first member of the anonymous union.  Note that this is
         required by the IA-64 ABI spec. */
      a_source_correspondence *field_scp;
      a_const_char            *name = first_field_name(var->type, &field_scp);
      if (name != NULL) {
        scp->name = name;
      }  /* if */
#if ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS
    } else if (scp->name == NULL) {
      /* This is probably a variable created to represent a string literal
         (see rewrite_address_of_string_as_address_of_variable), but we
         have to rule out variables generated for __FUNCTION__ and the like. */
      an_init_kind       init_kind;
      an_initializer_ptr initializer;
      a_constant_ptr     string_con;
      get_variable_initializer(var, scope, &init_kind, &initializer);
      if (init_kind == (an_init_kind)initk_static) {
        string_con = initializer->constant;
        if (string_con->kind == (a_constant_repr_kind)ck_string) {
          sequence_number = string_con->variant.string.sequence_number;
          if (sequence_number != 0) {
            /* Yes, this is a variable for a string literal. */
            is_string = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
#endif /* ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS */
    }  /* if */
#if GNU_EXTENSIONS_ALLOWED
    if (gnu_abi_tag_attribute_seen) {
      /* Determine any implicit abi_tags for this promoted static variable,
         but first make sure the implicit abi_tags for all enclosing routines
         have been calculated. */
      calculate_implicit_abi_tags_for_enclosing_routines(routine);
      calculate_implicit_abi_tags(scp, kind);
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  }  /* if */
  if (!scp->name_has_been_mangled &&
      (scp->name != NULL ||
       (kind == iek_variable &&
        struct_binding_container_needs_mangling(var)) ||
       is_string /*lint --e(845)*/)) {
    /* Leave the name alone if the entity is unnamed or if it has been
       mangled already (e.g., for a class name-as-subobject).  The lint
       comment indicates that is_string is known to be FALSE in some
       configurations. */
    start_mangling(&mctl);
    /* Name mangling is needed. */
#if !IA64_ABI
    { unsigned long        unique_number = 0;
      a_length_reservation length_reservation;
      /* The encoding is the original name, followed by "__Lnn", where "nn"
         is the scope number within the function, followed by two underscores,
         followed by the mangled name of the routine. */
      if (kind == iek_type) {
        /* For types with names, add a prefix and a length. */
        add_str_to_mangled_name(PREFIX_ON_NESTED_TYPE_NAME, &mctl);
        reserve_space_for_length(&length_reservation, &mctl);
      }  /* if */
      if (kind == iek_type && is_immediate_class_type((a_type_ptr)scp)) {
        /* Use the discriminator generated by the front end to differentiate
           within a function. */
        check_assertion(symbol_supplement_for_class((a_type_ptr)scp)->
                                                           discriminator != 0);
        unique_number = symbol_supplement_for_class((a_type_ptr)scp)->
                                                                 discriminator;
        add_str_to_mangled_name(scp->name, &mctl);
      } else if (scp_is_enum_member(scp)) {
        /* Use the discriminator generated by the front end to differentiate
           within a function. */
        an_enum_symbol_supplement_ptr ssp =
          symbol_for((a_type_ptr)scp_parent_scoped_enum_type(scp))->
                                                variant.enumeration.extra_info;
        check_assertion(ssp->discriminator != 0);
        unique_number = ssp->discriminator;
        add_str_to_mangled_name(scp->name, &mctl);
      } else if (kind == iek_variable &&
                 struct_binding_container_needs_mangling(var)){
        /* Structured bindings get their own mangling. */
        mangled_variable_name_with_possible_qualification(var, &mctl);
      } else if (!is_string) {
        /* Develop a scope number for the scope in which the entity appears.
           This number must be relative to the function rather than to the
           whole compilation so that if a given function (e.g., an extern
           inline function) is compiled in more than one compilation unit
           the scope number -- and therefore the mangled name -- will be
           the same in each compilation. */
        a_scope_ptr rout_scope = scope_for_routine(routine);
        a_boolean   found = FALSE;
        unique_number = search_scope_list(scope, rout_scope, &found);
        check_assertion_str(found,
                            "mangle_promoted_entity_name: scope not found");
#if GNU_EXTENSIONS_ALLOWED
        if (kind == iek_variable && var->has_gnu_abi_tag_attribute) {
          /* The Cfront ABI adds "abi_tag" mangling as a prefix. */
          add_abi_tag_mangling(scp->attributes, &mctl);
        }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
        add_str_to_mangled_name(scp->name, &mctl);
#if IA64_ABI
      } else {
        /* String literal -- add "__string", and the sequence number is
           used for the scope number (strings are numbered across the entire
           function). */
        add_str_to_mangled_name("__string", &mctl);
        unique_number = sequence_number;
#endif /* IA64_ABI */
      }  /* if */
      if (scp_is_enum_member(scp)) {
        /* Add the scoped enumeration type name.  Scoped enumerators aren't
           promoted out of their scope and so must be mangled here. */
        a_type_ptr   scoped_enum_type = scp_parent_scoped_enum_type(scp);
        a_const_char *scoped_enum_type_name = unmangled_or_fabricated_name_of(
                                            &scoped_enum_type->source_corresp);
        check_assertion(kind == iek_constant &&
                        scoped_enum_type_name != NULL);
        add_str_to_mangled_name(PREFIX_ON_NESTED_TYPE_NAME, &mctl);
        reserve_space_for_length(&length_reservation, &mctl);
        add_str_to_mangled_name(scoped_enum_type_name, &mctl);
      }  /* if */
      add_local_name_suffix(unique_number, routine, &mctl);
      if (kind == iek_type || scp_is_enum_member(scp)) {
        /* Fill in the length for a type or scoped enumerator. */
        fill_in_length(&length_reservation, &mctl);
      }  /* if */
    }
#else /* IA64_ABI */
    add_mangled_name_prefix(&mctl);
    /* See if the routine is static and needs to be externalized.  If so,
       start the mangled name with the externalizing prefix. */
    if (routine->source_corresp.externalized ||
        routine_should_be_externalized_for_exported_templates(routine)) {
      start_externalized_name(&routine->source_corresp, /*is_variable=*/FALSE,
                              &mctl);
      end_externalized_name(&routine->source_corresp, &mctl);
    }  /* if */
    add_prefix_for_local_entity(routine, &mctl);
    if (!is_string) {
      if (scp_is_enum_member(scp)) {
        /* Create a nested name with the scoped enumeration type and the
           scoped enumerator name.  This isn't specified in the IA-64 ABI
           and is mostly to avoid conflicts in generated C code
           (enumerators don't have external linkage).  No need to worry about
           enclosing classes or namespaces since the enum type is local
           to a function. */
        add_to_mangled_name('N', &mctl);
        mangled_encoding_for_class_or_enum_type(
                                      scp_parent_scoped_enum_type(scp), &mctl);
        mangled_name_with_length(scp->name, &mctl);
        /* Close the nested name. */
        add_to_mangled_name('E', &mctl);
      } else if (kind == iek_variable &&
                 struct_binding_container_needs_mangling(var)){
        /* Structured bindings get their own mangling. */
        mangled_variable_name_with_possible_qualification(var, &mctl);
      } else {
        mangled_name_with_length(scp->name, &mctl);
#if GNU_EXTENSIONS_ALLOWED
        if (kind == iek_variable && var->has_gnu_abi_tag_attribute) {
          /* The IA-64 ABI adds "abi_tag" mangling as a suffix. */
          add_abi_tag_mangling(scp->attributes, &mctl);
        }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
      }  /* if */
      add_discriminator_if_necessary(scp, &mctl);
    } else {
      /* String literal.  The name is "s" and the discriminator encodes the
         sequence number.  Note that sequence numbers and discriminator values
         both start with one. */
      add_to_mangled_name('s', &mctl);
      add_discriminator((a_discriminator)sequence_number,
                        /*emit_underscore=*/TRUE, &mctl);
    }  /* if */
#endif /* !IA64_ABI */
    (void)end_mangling_full(scp, final, &mctl);
#if !IA64_ABI
  } else if (kind == iek_type &&
             !scp->name_has_been_mangled &&
             scp->name == NULL) {
    /* Mangle unnamed types. */
    mangle_type_name((a_type_ptr)scp);
#endif /* !IA64_ABI */
  }  /* if */
}  /* mangle_promoted_entity_name */

#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN

void mangle_wrapper_name(a_routine_ptr entry_routine)
/*
entry_routine points to a routine that represents an entry point of
another routine (which is a virtual function with a covariant return type, 
and/or which requires adjustments to "this" on entry).  The entry routine is
like the primary routine, but does a derived-to-base cast on the returned
pointer, or performs the "this" adjustments.
*/
{
  a_mangling_control_block mctl;
  a_routine_ptr            prim_routine;
#if !IA64_ABI
  a_type_ptr               overridden_class;
#endif /* !IA64_ABI */

  prim_routine = entry_routine->overriding_function_for_wrapper;
  start_mangling(&mctl);
#if !IA64_ABI
  overridden_class = parent_class_of(
                               entry_routine->overridden_function_for_wrapper);
  /* The mangled name has the form
       __VFE__<overridden_class>__<prim_routine>
     where <overridden_class> and <prim_routine> are the mangled names for
     those entities. */
  add_str_to_mangled_name("__VFE__", &mctl);
  /* Add the class name. */
  mangled_type_name(overridden_class, &mctl);
  /* Add two underscores after the class name. */
  add_str_to_mangled_name("__", &mctl);
#else /* IA64_ABI */
  add_mangled_name_prefix(&mctl);
  /* Distinguish between covariant returns and ordinary thunks. */
  if (entry_routine->return_delta != 0 ||
      entry_routine->vbase_index != 0) {
    add_str_to_mangled_name("Tc", &mctl);
  } else {
    add_to_mangled_name('T', &mctl);
  }  /* if */
  /* Add the this-adjustment information. */
  if (entry_routine->vcall_index != 0) {
    add_to_mangled_name('v', &mctl);
  } else {
    add_to_mangled_name('h', &mctl);
  }  /* if */
  add_signed_number_to_mangled_name((long)entry_routine->delta, &mctl);
  add_to_mangled_name('_', &mctl);
  if (entry_routine->vcall_index != 0) {
    add_signed_number_to_mangled_name((long)(entry_routine->vcall_index *
                                       (long)vtbl_entry_size()),
                                      &mctl);
    add_to_mangled_name('_', &mctl);
  }  /* if */
  /* Add the return-adjustment information. */
  if (entry_routine->return_delta != 0 ||
      entry_routine->vbase_index != 0) {
    if (entry_routine->vbase_index != 0) {
      add_to_mangled_name('v', &mctl);
    } else {
      add_to_mangled_name('h', &mctl);
    }  /* if */
    add_signed_number_to_mangled_name((long)entry_routine->return_delta,
                                      &mctl);
    add_to_mangled_name('_', &mctl);
    if (entry_routine->vbase_index != 0) {
      add_signed_number_to_mangled_name((long)(entry_routine->vbase_index * 
                                         (long)vtbl_entry_size()), 
                                        &mctl);
      add_to_mangled_name('_', &mctl);
    }  /* if */
  }  /* if */
  if (prim_routine->source_corresp.name_has_been_mangled) {
    /* The name of the primary routine has already been mangled, so we can
       reuse it.  This is not just an optimization; if the primary routine is
       an alternate entry point for a destructor, there will be no unmangled
       name and mangled_function_name would abort. */
    add_str_to_mangled_name(prim_routine->source_corresp.name + 2,
                            &mctl);
  } else 
#endif /* IA64_ABI */
  /* Do not add code here. */
  {
    /* Add the routine name. */
    mangled_function_name(prim_routine,
                          /*suppress_param_encoding=*/FALSE,
                          /*suppress_parent_encoding=*/FALSE,
                          /*force_primary_name=*/FALSE,
                          /*force_individuation=*/FALSE,
                          /*base_name_offset=*/(sizeof_t *)NULL,
                          &mctl);
  }  /* if */
  (void)end_mangling_full(&entry_routine->source_corresp, /*final=*/TRUE,
                          &mctl);
}  /* mangle_wrapper_name */

#endif /* ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
#if IA64_ABI

static void overwrite_ctor_dtor_mangled_name_kind(char          *name,
                                                  a_routine_ptr routine,
                                                  char          ch)
/*
Overwrite a single character of the mangled name to indicate which type of
constructor/destructor the routine is.  "ch" is the character that replaces
the existing character (see set_ctor_dtor_mangled_name_kind for the mapping).
Be careful in cases where the mangled name has been truncated not to overwrite
past the end of the mangled name.
*/
{
  if (routine->source_corresp.mangled_name_cannot_be_included_in_other_name) {
    /* The mangled name has been truncated so the computed base_name_offset
       likely points past the end of the string.  We still need to make this
       mangled name different than the primary routine, so overwrite the
       second underscore that separates the truncated name from the CRC
       (a truncated mangled name cannot be demangled anyway). */
    sizeof_t len = strlen(name);
    len -= SIZE_OF_TRUNCATED_SUFFIX;
    check_assertion(name[len] == '_');
    name[len + 1] = ch;
  } else {
#if EXPENSIVE_CHECKING
    check_assertion(routine->variant.ctor_dtor.base_name_offset< strlen(name));
#endif /* EXPENSIVE_CHECKING */
    sizeof_t idx = routine->variant.ctor_dtor.base_name_offset + 1;
    if (name[idx] == 'I') {
      /* Inheriting constructors have "CI" before the unique character. */
      idx++;
    }  /* if */
    name[idx] = ch;
  }  /* if */
}  /* overwrite_ctor_dtor_mangled_name_kind */


static char ctor_dtor_kind_char(a_routine_ptr routine)
/*
Returns the character to be used in mangling to identify the type of
constructor/destructor alternate entry point specified by ctor_dtor_kind.
E.g., "C1" is used for a complete constructor/destructor, so '1' is returned
when ctor_dtor_kind is cdk_complete.
*/
{
  char ch;

  switch (routine->ctor_dtor_kind) {
    case cdk_complete:  ch = '1';               break;
    case cdk_subobject: ch = '2';               break;
    case cdk_deleting:  ch = '0';               break;
    case cdk_delegation:ch = '9';               break;
    default:            unexpected_condition();
  }  /* switch */
  return ch;
}  /* ctor_dtor_kind_char */


void set_ctor_dtor_mangled_name_kind(a_routine_ptr routine)
/*
The constructor or destructor already has a mangled name (which has likely
been copied from the mangled name of the primary routine).  Modify the name
to reflect the type of constructor/destructor that has been assigned to
the routine (e.g., complete, subobject, etc.).  This is done by changing
a single character of the mangled name (as pointed to by base_name_offset).
*/
{
  check_assertion(routine->source_corresp.name_has_been_mangled &&
                  (routine->special_kind ==
                                   (a_special_function_kind)sfk_constructor ||
                   routine->special_kind ==
                                   (a_special_function_kind)sfk_destructor) &&
                  routine->variant.ctor_dtor.base_name_offset != 0);
  overwrite_ctor_dtor_mangled_name_kind((char *)routine->source_corresp.name,
                                        routine,
                                        ctor_dtor_kind_char(routine));
}  /* set_ctor_dtor_mangled_name_kind */


void mangle_alternate_entry_point_name(a_routine_ptr routine,
                                       a_routine_ptr prim_routine)
/*
Give a mangled name to the alternate entry point whose routine entry
is given by routine.  It is an alternate entry for the primary entry
point identified by prim_routine.  Alternate entry points are used
for constructors and destructors in the IA-64 ABI.  The ctor_dtor_kind
in the routine must be set already.
*/
{
  a_boolean dummy;

  /* Leave the entry point unnamed if the primary entry point will
     stay unnamed, e.g., for an unnamed class. */
  if (prim_routine->source_corresp.name_has_been_mangled ||
      function_name_mangling_needed(prim_routine, &dummy)) {
    char *name, *mangled_name;
    /* Compute the mangled name for this new entry point.  It's the same as
       the routine -- but "Cx" or "Dx" needs to become "C1", "C2", etc. */
    /* The name cannot be generated by building it up from scratch, because
       this routine may be called at a point where the primary routine
       has already been lowered, and therefore correct mangling for its
       parameter types is no longer possible.  Therefore we get the
       primary routine's name (which will have been generated already
       if the routine has been lowered) and change one character. */
    /* The name will be externalized if necessary later when it's lowered.
       Don't try to do it here, because we may be lowering a function body
       in the middle of the compilation and the module id may not be
       set yet. */
    /* Ensure that the primary routine has been given a mangled name since
       the alternate entry point name will be based on that.  Note that
       it is possible that the primary routine may later be determined to
       be a delegating constructor, in which case its name will change
       (and a re-mangling will be required). */
    mangle_function_name(prim_routine, /*suppress_parent_encoding=*/FALSE);
    check_assertion(prim_routine->source_corresp.name_has_been_mangled &&
                    prim_routine->variant.ctor_dtor.base_name_offset != 0);
    mangled_name = (char *)prim_routine->source_corresp.name;
    name = alloc_lowered_name_string(strlen(mangled_name) + 1);
    (void)strcpy(name, mangled_name);
    routine->source_corresp.name = name;
    routine->source_corresp.name_has_been_mangled = TRUE;
    routine->variant.ctor_dtor.base_name_offset =
                              prim_routine->variant.ctor_dtor.base_name_offset;
    routine->source_corresp.mangled_name_cannot_be_included_in_other_name =
      prim_routine->
                  source_corresp.mangled_name_cannot_be_included_in_other_name;
    set_ctor_dtor_mangled_name_kind(routine);
  }  /* if */
}  /* mangle_alternate_entry_point_name */

#endif /* IA64_ABI */
#endif /* DO_IL_LOWERING */

#if !IA64_ABI

static a_compressible_string_pos_ptr alloc_compressible_string_pos(void)
/*
Allocate compressible string position entry, set its fields to default
values, and return a pointer to it.
*/
{
  a_compressible_string_pos_ptr cspp;

  if (avail_compressible_string_pos != NULL) {
    /* Reuse a freed entry. */
    cspp = avail_compressible_string_pos;
    avail_compressible_string_pos = cspp->next;
  } else {
    /* Allocate a new entry. */
    cspp = (a_compressible_string_pos_ptr)alloc_general(
                                            sizeof(a_compressible_string_pos));
#if DEBUG
/* GCOVR_EXCL_START */
    num_compressible_string_pos_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  }  /* if */
  cspp->next = NULL;
  cspp->str_pos = 0;
  return cspp;
}  /* alloc_compressible_string_pos */


static void free_compressible_string_pos(a_compressible_string_pos_ptr cspp)
/*
Free the indicated compressible string position entry by returning it
to the available list for reuse.
*/
{
  cspp->next = avail_compressible_string_pos;
  avail_compressible_string_pos = cspp;
}  /* free_compressible_string_pos */


static char *compress_mangled_name(a_const_char             *mangled_name,
                                   a_source_correspondence  *scp,
                                   a_mangling_control_block *mctl)
/*
Compress the mangled name that's been built up (pointed to by mangled_name,
with length given by mctl->length, including a terminating null).
If mangled_name is NULL, the mangled name is in mangling_text_buffer, starting
at offset 0.  It is important to pass NULL, and not a pointer to
mangling_text_buffer, in that case.  Return a pointer to the name, either
the original one or a compressed version.  The compressed version is
in mangling_text_buffer if mangled_name is NULL, and allocated in IL memory if
mangled_name is non-NULL.  mangling_text_buffer->size must indicate the
first available position in mangling_text_buffer (e.g., after the terminating
null of the mangled name).  scp, if non-NULL, points to the source
correspondence entry for the entity whose name this is.
*/
{
  char *compr_name = NULL;

/* Macro to determine the input buffer address.  This is recomputed each
   time it is needed because the mangling_text_buffer might move. */
#define src_mangled_name \
  ((mangled_name == NULL) ? mangling_text_buffer->buffer : mangled_name)

  /* See whether the name should be examined to see if it is
     compressible.  mctl->length indicates the length of the name,
     including the terminating null.  Don't try compression if the name
     is already fairly small.  Note that one advantage of avoiding
     compression on relatively small names is allowing more compatibility
     with libraries compiled by cfront.  The largest name noted in the
     iostream package had 56 characters. */
  if (compress_mangled_names && mctl->length >= 60) {
    /* Build up the compressed name in the mangling_text_buffer, following
       anything already in there (e.g., after the null character at the
       end of the mangled name). */
    /* Note that positions in the mangling_text_buffer are kept as offsets
       rather than pointers because the mangling_text_buffer may get moved
       if it is resized. */
    sizeof_t start_of_compressed_name = mangling_text_buffer->size;
    sizeof_t src_pos = 0;
    a_compressible_string_pos_ptr
             cspp = NULL;
    sizeof_t size_of_mangled_name = mctl->length; /* Including final null. */
    sizeof_t size_of_compressed_name, prefix_length;
    sizeof_t i;
    Small_string<64>
             buffer;
#define NUM_BUCKETS_IN_COMPRESSION_HASH_TABLE 64
    a_compressible_string_pos_ptr
             hash_table[NUM_BUCKETS_IN_COMPRESSION_HASH_TABLE];
    /* Clear the hash table used to keep track of the position of
       compressible strings in the original mangled name. */
    memzero((char *)hash_table, sizeof(hash_table));
    
    for (;;) {
      /* Copy characters from the original name to the mangled name, looking
         for a string of digits (which starts a compressible section). */
      char ch = src_mangled_name[src_pos];
      if (ch == '\0') break;
      if (!isdigit((unsigned char)ch)) {
        add_char_to_text_buffer(mangling_text_buffer, ch);
        /* If a "J" appears, copy it as "JJ" to avoid confusion with the
           "J" markers used to indicate compression. */
        if (ch == 'J') add_char_to_text_buffer(mangling_text_buffer, 'J');
        src_pos++;
      } else {
        /* A digit.  This may be the start of a compressible string. */
        sizeof_t      num_digits = 1;
        sizeof_t      digit, length = 0, hash_value = 0;
        unsigned long value = (ch - '0');
        a_boolean     ovflo = FALSE, valid, compressed = FALSE;
        /* Determine the number of digits in the digit string and accumulate
           its value. */
        for (;;) {
          ch = src_mangled_name[src_pos+num_digits];
          if (!isdigit((unsigned char)ch)) break;
          digit = ch - '0';
          num_digits++;
          if (value > ULONG_MAX / 10) ovflo = TRUE;
          value *= 10;
          if (value > ULONG_MAX-digit) ovflo = TRUE;
          value += (unsigned long)digit;
        }  /* for */
        /* See whether the length is valid. */
        if (ovflo) {
          valid = FALSE;
        } else if (value < 4) {
          /* Don't compress very small strings like "3ABC", because
             the compressed form is probably not smaller.  This also
             discards cases where a user variable has a name like "f2",
             which are not worth examining. */
          valid = FALSE;
        } else if ((length = num_digits + value),
                   size_of_mangled_name-src_pos <= length) {
          /* The length is too long -- it runs off the end of the mangled
             name.  That means it can't be a real length. */
          valid = FALSE;
        } else {
          /* The length is okay. */
          valid = TRUE;
        }  /* if */
        if (valid) {
          /* See whether the string has appeared previously by comparing
             it against the strings in the hash table. */
          hash_value = value % NUM_BUCKETS_IN_COMPRESSION_HASH_TABLE;
          for (cspp = hash_table[hash_value];
               cspp != NULL;
               cspp = cspp->next) {
            /* Compare the previous string to this new string. */
            if (strncmp(src_mangled_name+cspp->str_pos,
                        src_mangled_name+src_pos,
                        size_t_arg(length)) == 0) {
              /* Found an identical previous string, so we can compress it. */
              compressed = TRUE;
              break;
            }  /* if */
          }  /* for */
        }  /* if */
        if (compressed) {
          /* Replace the string by "JnnnJ", where "nnn" is the position of
             the previous identical string. */
          buffer.reset_to("J", cspp->str_pos, "J");
          add_string_to_text_buffer(mangling_text_buffer,
                                    buffer.as_temp_characters());
          /* Continue scanning the original string after the full string
             that was compressed away. */
          src_pos += length;
        } else {
          /* The string has not appeared previously.  If it's a valid
             string, remember it for possible later reuse. */
          if (valid) {
            cspp = alloc_compressible_string_pos();
            cspp->str_pos = src_pos;
            cspp->next = hash_table[hash_value];
            hash_table[hash_value] = cspp;
          }  /* if */
          /* Put out the digit string. */
          for (i = 0; i < num_digits; i++) {
            add_char_to_text_buffer(mangling_text_buffer,
                                    src_mangled_name[src_pos]);
            src_pos++;
          }  /* for */
          /* Continue scanning the original string after the digit
             string. */
        }  /* if */
      }  /* if */
    }  /* for */
    /* Add the final null. */
    add_char_to_text_buffer(mangling_text_buffer, '\0');
    /* Free the entries in the hash table. */
    for (i = 0; i < NUM_BUCKETS_IN_COMPRESSION_HASH_TABLE; i++) {
      a_compressible_string_pos_ptr cspp_next;
      for (cspp = hash_table[i]; cspp != NULL; cspp = cspp_next) {
        cspp_next = cspp->next;
        free_compressible_string_pos(cspp);
      }  /* for */
    }  /* for */
    /* The prefix on the compressed form is "__CPR" followed by the size
       of the original (uncompressed) name, not counting the final null. */
    buffer.reset_to("__CPR", size_of_mangled_name-1, "__");
    prefix_length = buffer.length();
#if EXPENSIVE_CHECKING
    /* Make sure the name does not already have the compression prefix in
       it.  If it does, we've used a previously compressed name in building
       up this name, and that won't work. */
    check_assertion_str(strstr(mangling_text_buffer->buffer +
                                                      start_of_compressed_name,
                               "__CPR") == NULL,
                        "compress_mangled_name: double compression");
#endif /* EXPENSIVE_CHECKING */
    size_of_compressed_name = (mangling_text_buffer->size -
                               start_of_compressed_name) +
                              prefix_length;
    /* Note that both size_of_compressed_name and size_of_mangled_name
       include the terminating null. */
    if (size_of_compressed_name < size_of_mangled_name) {
      /* The compressed name is shorter, so use it.  (There are some
         pathological cases where the compressed version might be larger.) */
      if (mangled_name == NULL) {
        /* The original mangled name is in mangling_text_buffer, preceding
           the compressed form. */
        /* Put the prefix out in front of the compressed name, and return
           the position of the prefix in that position as the address of
           the full compressed name. */
        check_assertion(start_of_compressed_name >= prefix_length);
        compr_name = mangling_text_buffer->buffer+start_of_compressed_name -
                     prefix_length;
        (void)memcpy(compr_name, buffer.as_temp_characters(),
                     size_t_arg(prefix_length));
      } else {
        /* The mangled name is not in mangling_text_buffer.  Allocate new IL
           memory for the compressed name, including the prefix. */
        compr_name = alloc_lowered_name_string(size_of_compressed_name);
        (void)memcpy(compr_name, buffer.as_temp_characters(),
                     size_t_arg(prefix_length));
        (void)strcpy(compr_name+prefix_length,
                     mangling_text_buffer->buffer+start_of_compressed_name);
      }  /* if */
      mangled_name = compr_name;
      /* Update the length, including the null terminator. */
      mctl->length = size_of_compressed_name;
      if (scp != NULL) {
        /* A compressed name cannot be used as part of another mangled name. */
        scp->mangled_name_cannot_be_included_in_other_name = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  /* If the name was not compressed, return the source mangled name address. */
  if (compr_name == NULL) compr_name = (char *)src_mangled_name;
  return compr_name;
#undef get_char_from_mangled_name
}  /* compress_mangled_name */

#endif /* !IA64_ABI */

static char *truncate_mangled_name(char                     *mangled_name,
                                   a_source_correspondence  *scp,
                                   a_mangling_control_block *mctl)
/*
If necessary, truncate the mangled name that has been built up.  The
name is pointed to by mangled_name.  Its length (with terminating
null) is given by mctl->length.  If the name is longer than
max_mangled_name_length (and the latter is greater than zero), truncate it
by computing a CRC checksum and putting the checksum, in hex, at the end
of as much of the name as will fit along with the checksum.  Such
a truncated name is short enough, and likely to be unique, but it
cannot be demangled.  scp, if non-NULL, points to the source
correspondence entry for the entity whose name this is.
*/
{
  if (max_mangled_name_length != 0 &&
      mctl->length-1 > max_mangled_name_length) {
    /* The name must be truncated. */
    /* The suffix is of the form "__abcdabcd", i.e., one needs 10 characters
       for it. */
    char ctor_dtor_char = '_';
    sizeof_t max_allowed_length =
                            max_mangled_name_length - SIZE_OF_TRUNCATED_SUFFIX;
#if IA64_ABI && ABI_COMPATIBILITY_VERSION >= 520
    if (mctl->ctor_dtor_char != '\0') {
      /* If we're truncating a mangled name for a constructor or destructor,
         the differentiation between alternate entry points (e.g.,
         "C0" vs "C1") may be past the point of truncation, which would result
         in the same mangled name.  To prevent that, replace the second
         underscore (just before the CRC) with the distinguishing character. */
      ctor_dtor_char = mctl->ctor_dtor_char;
    }  /* if */
#endif /* IA64_ABI && ABI_COMPATIBILITY_VERSION >= 520 */

    auto        hex_view = hex_view_of(crc_32(mangled_name, (unsigned long)0));
    Small_string<SIZE_OF_TRUNCATED_SUFFIX + 1>
                buffer("_", a_string_view(&ctor_dtor_char, 1),
                       left_pad(8, '0', hex_view));
    buffer.write_to_buffer(mangled_name + max_allowed_length,
                           SIZE_OF_TRUNCATED_SUFFIX + 1);
    mctl->length = max_mangled_name_length+1;
    if (scp != NULL) {
      /* A truncated name cannot be used as part of another mangled name. */
      scp->mangled_name_cannot_be_included_in_other_name = TRUE;
    }  /* if */
  }  /* if */
  return mangled_name;
}  /* truncate_mangled_name */


void lower_name_one_time_init(void)
/*
Do one-time initialization of variables related to name mangling.
*/
{
  mangling_text_buffer = NULL;
  mangling_buffer_free_list = NULL;
  mangling_buffers_in_use = NULL;
  in_mangling_pre_pass = FALSE;
#if IA64_ABI
  avail_substitutions = NULL;
#endif /* !IA64_ABI */
#if GNU_EXTENSIONS_ALLOWED
  avail_abi_tag_strings = NULL;
  implicit_tag_list = NULL;
#if DO_IL_LOWERING
  avail_rlep_entries = NULL;
#endif /* DO_IL_LOWERING */
  ttt_scp_for_implicit_abi_tags = NULL;
  ttt_kind_for_implicit_abi_tags = iek_none;
  ttt_mark_value = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
  /* Save variables from lower_name.c that are needed for precompiled
     headers. */
  if (precompiled_header_processing_required) {
    STATIC_THREAD a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(unnamed_type_seed),
      pch_saved_var_array_elem(unnamed_member_variable_name_seed),
      pch_saved_var_array_elem(active_parents),
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  /* Register variables that must be saved and restored when switching
     between translation units. */
  register_trans_unit_variable(unnamed_type_seed);
  register_trans_unit_variable(unnamed_member_variable_name_seed);
}  /* lower_name_one_time_init */


void lower_name_init(void)
/*
Initialize static variables related to name mangling that must be
initialized for each compilation.
*/
{
  unnamed_type_seed = 0;
  unnamed_member_variable_name_seed = 0;
#if !IA64_ABI
  avail_compressible_string_pos = NULL;
#if DEBUG
/* GCOVR_EXCL_START */
  num_compressible_string_pos_allocated = 0;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
#endif /* !IA64_ABI */
  active_parents = alloc_fe_of_type(an_active_parent_map);
  construct(active_parents, /*mask_width=*/8u);
}  /* lower_name_init */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* NEED_NAME_MANGLING */


