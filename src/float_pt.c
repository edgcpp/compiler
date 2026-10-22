/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

float_pt.c -- Routines that manipulate internal floating-point quantities.

The versions in this file are for prototyping only, and should be replaced
for a production version.

*/

/* Header files common to all files. */
#include "fe_common.h"

#if !USE_HOST_FP_CONVERSION_ROUTINES || \
    (USE_FLOAT128_FOR_HOST_FP_VALUE && !USE_QUADMATH_LIBRARY)
#include "floating.h"
#endif /* !USE_HOST_FP_CONVERSION_ROUTINES || (USE_FLOAT128...) */

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Additional header files. */
#include "folding.h"
#include <float.h>
#if USE_HOST_FP_CONVERSION_ROUTINES
#include <errno.h>
#include <math.h>
#endif /* USE_HOST_FP_CONVERSION_ROUTINES */

#if USE_QUADMATH_LIBRARY
#include <quadmath.h>
#endif /* USE_QUADMATH_LIBRARY */

#if __BSD__
/* BSD errno.h doesn't define "errno". */
extern "C" int errno;
#endif /* __BSD__ */
#ifndef STDLIB_H_INCLUDED
extern "C" double strtod(char *, char **);
#endif /* ifndef STDLIB_H_INCLUDED */
#if TARG_HAS_IEEE_FLOATING_POINT
/* Define is_NaN and is_finite.  They must work on an argument of type
   a_host_fp_value (typically double or long double). */
#define NEED_EDG_ISNAN 0
#if USE_SOFTFLOAT
/* SoftFloat does not provide routines to detect NaN or infinities. */
#define is_NaN(x) (do_softfloat_is_nan(x))
#define is_finite(x) (host_fp_value_is_finite(x))
#define NEED_HOST_FP_VALUE_IS_FINITE 1
#else /* !USE_SOFTFLOAT */
#if EDG_WIN32
/* Windows, all versions. */

#ifdef __MWERKS__
#include <math.h>
#define is_NaN(x) (isnan(x))
#define is_finite(x) (isfinite(x))
#else /* !defined(__MWERKS__) */
#include <float.h>
#define is_NaN(x) (_isnan((double)x))
/* Note that MSVC has long double the same size as double so _finite
   will work for long double also. */
#if USE_DOUBLE_FOR_HOST_FP_VALUE || \
    (USE_LONG_DOUBLE_FOR_HOST_FP_VALUE && DBL_MAX_EXP == LDBL_MAX_EXP)
#define is_finite(x) (_finite((double)(x))) 
#else /* !(USE_DOUBLE_FOR_HOST_FP_VALUE ... ) */
/* This must be a compiler other than MSVC++ on Windows, likely one that
   uses 80-bit long doubles. */
/* See definition of host_fp_value_is_finite below. */
#define is_finite(x) (host_fp_value_is_finite(x))
#define NEED_HOST_FP_VALUE_IS_FINITE 1
#endif /* !USE_LONG_DOUBLE_FOR_HOST_FP_VALUE ... */
#endif /* ifdef __MWERKS__ */
#else /* !EDG_WIN32 */
#ifdef __sun
/* SunOS, Solaris, including Solaris on Intel X86. */
#ifndef isnan
/*
isnan is a macro in some Solaris versions.  Don't provide an extern
declaration in such cases.
*/
extern "C" int isnan(double x);
#endif /* isnan */
#define is_NaN(x) (isnan((x)))
#if !USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
/* The "finite" function takes a double argument, so it doesn't work
   for long double (the conversion to double could produce an infinity
   for a too-large value). */
extern "C" int finite(double x);
#define is_finite(x) (finite(x))
#else /* USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
/* See definition of host_fp_value_is_finite below. */
#define is_finite(x) (host_fp_value_is_finite(x))
#define NEED_HOST_FP_VALUE_IS_FINITE 1
#endif /* !USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
#else /* !defined(__sun) */
/* Not Windows, not Solaris, not SunOS. */
#include <math.h>
#ifdef isnan
#define is_NaN(x) (isnan(x))
#else /* !defined(isnan) */
#if __linux__
#define is_NaN(x) (__isnan((double)(x)))
#else /* !__linux__ */
#if USE_FLOAT128_FOR_HOST_FP_VALUE && defined(__CYGWIN__)
/* Cygwin does not have a __float128 version of isnan, so provide our own. */
#undef NEED_EDG_ISNAN
#define NEED_EDG_ISNAN 1
#define is_NaN(x) (edg_isnan((x)))
#else  /* !(USE_FLOAT128_FOR_HOST_FP_VALUE && defined(__CYGWIN__)) */
#define is_NaN(x) (isnan((x)))
#endif /* USE_FLOAT128_FOR_HOST_FP_VALUE && defined(__CYGWIN__) */
#endif /* __linux__ */
#endif /* ifdef isnan */
#if USE_FLOAT128_FOR_HOST_FP_VALUE
/* <math.h> doesn't have a __float128 version, so use our own. */
/* See definition of host_fp_value_is_finite below. */
#define is_finite(x) (host_fp_value_is_finite(x))
#define NEED_HOST_FP_VALUE_IS_FINITE 1
#else /* !USE_FLOAT128_FOR_HOST_FP_VALUE */
/* C99 has the "isfinite" macro.  Linux headers do, too.  Cygwin has it, but
   it is unreliable.  The HP PA headers have isfinite, but it does not accept
   a long double argument. */
#if defined(isfinite) && !defined(__CYGWIN__) && !defined(__hppa)
#define is_finite(x) (isfinite(x))
#else /* !defined(isfinite) */
/* The "finite" function takes a double argument, so it doesn't work
   for long double (the conversion to double could produce an infinity
   for a too-large value). */
#if USE_DOUBLE_FOR_HOST_FP_VALUE
#if __linux__
#define is_finite(x) (__finite(x))
#elif defined(__APPLE__)
#define is_finite(x) (isfinite(x))
#else /* !__linux__ */
#define is_finite(x) (finite(x))
#endif /* __linux__ */
#else /* !USE_DOUBLE_FOR_HOST_FP_VALUE */
/* See definition of host_fp_value_is_finite below. */
#define is_finite(x) (host_fp_value_is_finite(x))
#define NEED_HOST_FP_VALUE_IS_FINITE 1
#endif /* USE_DOUBLE_FOR_HOST_FP_VALUE */
#endif /* ifdef isfinite */
#endif /* USE_FLOAT128_FOR_HOST_FP_VALUE */
#endif /* ifdef __sun */
#endif /* EDG_WIN32 */
#endif /* USE_SOFTFLOAT */
#endif /* TARG_HAS_IEEE_FLOATING_POINT */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

#ifdef _lint
/*
When using lint, just use versions of is_finite and is_NaN that won't cause
diagnostics.
*/
#undef is_finite /*lint !e750*/
#undef is_NaN /*lint !e750*/
#undef NEED_HOST_FP_VALUE_IS_FINITE  /*lint !e750*/
#define is_finite(x) lint_is_finite((long double)x)
#define is_NaN(x) lint_is_NaN((long double)x)
static a_boolean lint_is_finite(long double x) /*lint !e528*/
{return x == 0.0; }
static a_boolean lint_is_NaN(long double x) /*lint !e528*/
{return x == 0.0; }
#endif /* ifdef _lint */

/*
Macro that returns TRUE if the representation of the floating-point kind is
that of double.
*/
#define repr_is_double(kind)                                                  \
  ((kind) == fk_float32x || (kind) == fk_double)

/*
Macro that returns TRUE if the representation of the floating-point kind is
that of long double.
*/
#define repr_is_long_double(kind)                                             \
  ((kind) == fk_float64x || (kind) == fk_long_double)

/*
Macro that returns TRUE if the floating-point kind is binary 64 (i.e.,
64-bit floating-point).  That's always the case for "double", but may also
be the case for "long double" in some configurations (but that must be
determined at run time because the sizes of these types can be part of
a target configuration).
*/
/*lint -emacro(506,kind_is_binary64)*/
#define kind_is_binary64(kind)                                                \
  (repr_is_double(kind) ||                                                   \
   (repr_is_long_double(kind) &&                    \
    (long_double_is_double || !FP_HAS_LONG_DOUBLE)) ||                        \
   (is_extended_flt_kind(kind) && flt_type_size[(int)kind] == 8))

/*
Macro that returns TRUE if the floating-point kind is binary 16 (i.e.,
follows the IEEE 754 "half precision" model).
*/
#define kind_is_binary16(kind)                                                \
  ((kind) == fk_float16 || (kind) == fk_fp16 || kind == fk_std_float16)

/*
Macro that returns TRUE if the floating-point kind represents any 16-bit
floating-point format.
*/
#define kind_is_16bit(kind)                                                   \
  (kind_is_binary16(kind) || (kind) == fk_std_bfloat16)

/*
Macro that returns TRUE if the most-significant bit of the floating-point
kind's mantissa is an implicit 1 bit.  That is true of all supported
floating-point kinds except fk_float80 and fk_long_double when long double
has a float80 representation.
*/
#define kind_has_implicit_mantissa_bit(kind)                                  \
  !((kind) == fk_float80 ||                                                   \
    (repr_is_long_double(kind) && targ_ldbl_mant_dig == 64))

STATIC_THREAD a_boolean
                long_double_is_double;
                        /* TRUE in configurations where the "long double" and
                           "double" types have the same representation. */

STATIC_THREAD sizeof_t
		data_size_of_host_fp_value;
			/* The number of bytes of the host floating point
			   value that actually contain data.  This is
			   smaller than the actual size on some systems
			   (e.g., Intel long doubles use 10 bytes of the
			   12 bytes of allocated space). */

STATIC_THREAD a_host_fp_value
		fp_zero;
			/* The value 0.0 in internal representation. */

#if USE_SOFTFLOAT
/* _Float16 values are represented as the SoftFloat float16_t type. */
typedef float16_t EDG_float16_t;
#else /* !USE_SOFTFLOAT */
#if HOST_HAS_FLOAT16_TYPE
/* _Float16 values are represented using the host _Float16 type. */
typedef _Float16 EDG_float16_t;
#else /* !HOST_HAS_FLOAT16_TYPE */
/* _Float16 values are represented using the host "float" type. */
typedef float EDG_float16_t;
#endif /* HOST_HAS_FLOAT16_TYPE */
static void store_host_fp_value(a_host_fp_value         temp,
                                a_float_kind            kind,
                                an_internal_float_value *float_value,
                                a_boolean               *err);
#endif /* USE_SOFTFLOAT */

#if USE_SOFTFLOAT

/* A union to map the float32_t SoftFloat type to "float". */
typedef union softfloat32_t {
  float32_t     soft;
  float         hard;
} softfloat32_t;

/* A union to map the float64_t SoftFloat type to "double". */
typedef union softfloat64_t {
  float64_t     soft;
  double        hard;
} softfloat64_t;

STATIC_THREAD float16_t
		f16_zero;
			/* The value 0.0F16. */
STATIC_THREAD float32_t
		f32_zero;
			/* The value 0.0F. */
STATIC_THREAD float64_t
		f64_zero;
			/* The value 0.0. */

/*
Utility macros to perform low-level operations on a_host_fp_value operands.
These SoftFloat versions assume that all floating-point operations are
performed with 128-bit floating point values (i.e., that a_host_fp_value
is float128_t).  They could be rewritten to assume, e.g. 64-bit floating
point values, but since SoftFloat provides 128-bit, we use that.
*/
#define do_fp_add(op1, op2, result)      f128M_add(&(op1), &(op2), &(result))
#define do_fp_subtract(op1, op2, result) f128M_sub(&(op1), &(op2), &(result))
#define do_fp_multiply(op1, op2, result) f128M_mul(&(op1), &(op2), &(result))
#define do_fp_divide(op1, op2, result)   f128M_div(&(op1), &(op2), &(result))
#define do_fp_negate(op1, result)        do_softfloat_negate(&(op1), &(result))
#define do_fp_eq_zero(op)                (f128M_eq(&(op), &fp_zero))
#define do_fp_lt_zero(op)                (f128M_lt(&(op), &fp_zero))

static void do_softfloat_negate(a_host_fp_value *op1,
                                a_host_fp_value *result)
/*
SoftFloat doesn't have a unary negate routine, so create one.  Note that
subtracting from zero doesn't work for our purposes (e.g., to represent -0.0
the lexical routines scan 0.0 and then call this routine, that would result in
0.0-0.0 which yields a positive 0.0, not the desired -0.0).
*/
{
  an_internal_float_value *fp = (an_internal_float_value*)result;
  a_byte                  *sign_byte;

  *result = *op1;
  if (host_little_endian) {
    sign_byte = &fp->bytes[data_size_of_host_fp_value-1];
  } else {
    sign_byte = &fp->bytes[0];
  }  /* if */
  *sign_byte = (~(*sign_byte & 0x80) & 0x80) | (*sign_byte & 0x7F);
}  /* do_softfloat_negate */


static a_boolean do_softfloat_is_nan(a_host_fp_value value)
/*
SoftFloat doesn't have a routine to detect NaN values, so use this routine.
Note that this routine doesn't differentiate between signaling and quiet NaNs.
Note also that this works only for 128-bit floating-point values.
*/
{
  a_boolean     result = FALSE;
  unsigned char *p = (unsigned char *)&value;
  unsigned int  exponent, i;

  if (host_little_endian) {
    exponent = (p[15] << CHAR_BIT) | p[14];
  } else {
    exponent = (p[0] << CHAR_BIT) | p[1];
  }  /* if */
  if ((exponent & 0x7fff) == 0x7fff) {
    /* All ones in the exponent field means a NaN or infinity.  Any non-zero
       byte in the fraction indicates a NaN. */
    for (i = 2; i < 14; i++) {
      if (p[i] != 0) {
        result = TRUE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return result;
}  /* do_softfloat_is_nan */

#if USE_HEX_FP_CONSTANTS_IN_GENERATED_CODE

template<typename a_Dyn_array>
static void append_softfloat_hex_constant_string(
                                     a_Dyn_array             &underlying_array,
                                     a_float_kind            kind,
                                     an_internal_float_value *float_value)
/*
Append the characters forming the hexadecimal floating-point representation of
float_value to underlying_array.  kind represents the kind of floating-point
value (which must be an 80-bit or 128-bit floating type).

Note that float and double are handled by the caller (through
append_using_c_formatting) as they are supported on all platforms.
*/
{
  uint32_t  exponent, exponent_bias = 16383;
  a_byte    *p;
  int       i, offset, bytes, trailing_zeros = 0, left;
  a_boolean leading_zeros = TRUE;
  a_boolean implied_hidden_bit = kind_has_implicit_mantissa_bit(kind);

  /* This routine only handles 80 and 128-bit float (everything else should
     be handled by the caller). */
  if (kind == fk_float80 ||
      (repr_is_long_double(kind) && targ_ldbl_mant_dig == 64)) {
      /* 80 bits. */
    bytes = 10;
  } else if (kind == fk_float128 || kind == fk_std_float128 ||
             (repr_is_long_double(kind) && targ_ldbl_mant_dig == 113)) {
      /* 128 bits. */
    bytes = 16;
  } else {
    unexpected_condition();
  }  /* if */
  if (host_little_endian) {
    exponent = (float_value->bytes[bytes-1] << CHAR_BIT) |
                float_value->bytes[bytes-2];
    p = &float_value->bytes[bytes-3];
    offset = -1;
  } else {
    exponent = (float_value->bytes[0] << CHAR_BIT) | float_value->bytes[1];
    p = &float_value->bytes[2];
    offset = 1;
  }  /* if */
  if (exponent & 0x8000) {
    underlying_array.push_back('-');
  }  /* if */
  underlying_array.push_back('0');
  underlying_array.push_back('x');
  exponent = exponent & 0x7fff;
  /* Only concerned with mantissa bytes now. */
  bytes -= 2;
  /* Count the number of trailing zero nibbles (we suppress these later). */
  for (i = 0; i < bytes; i++) {
    if ((*p & 0xf0) == 0) {
      trailing_zeros++;
    } else {
      trailing_zeros = 0;
    }  /* if */
    if ((*p & 0x0f) == 0) {
      trailing_zeros++;
    } else {
      trailing_zeros = 0;
    }  /* if */
    p += offset;
  }  /* for */
  /* Restore pointer. */
  p -= (bytes * offset);
  left = bytes*2;
  if (exponent == 0 && left == trailing_zeros) {
    /* Handle zero as a special case. */
    detail::append_string_literal(underlying_array, "0p0");
  } else {
    if (implied_hidden_bit) {
      /* The value of the implied hidden bit is determined by the exponent. */
      if (exponent == 0) {
        underlying_array.push_back('0');
      } else {
        underlying_array.push_back('1');
      }  /* if */
    } else {
      underlying_array.push_back('0');
      exponent_bias--;
    }  /* if */
    underlying_array.push_back('.');
    /* Use 16382 as the exponent for denormalized values. */
    if (exponent == 0) exponent_bias--;
    for (i = 0; i < bytes; i++) {
      constexpr char hex_to_ascii[17] = "0123456789abcdef";
#define write_nibble(n)                                                      \
  {                                                                          \
    if (i == 0 || (n) != 0 || !leading_zeros) {                              \
      underlying_array.push_back(hex_to_ascii[(n)]);                         \
      leading_zeros = FALSE;                                                 \
    }  /* if */                                                              \
    if (--left <= trailing_zeros) break;                                     \
  }
      write_nibble((*p & 0xf0) >> 4);
      write_nibble(*p & 0x0f);
#undef write_nibble
      p += offset;
    }  /* for */
    underlying_array.push_back('p');

    int    appended_int = (int)exponent - exponent_bias;
    size_t size_hint = detail::String_formatter<int>::size_hint_of(
                                                                 appended_int);
    detail::String_formatter<int>::append_into(underlying_array, appended_int,
                                               size_hint);
  }  /* if */
}  /* append_softfloat_hex_constant_string */

#endif /* USE_HEX_FP_CONSTANTS_IN_GENERATED_CODE */

#else /* !USE_SOFTFLOAT */
/*
Use the host-provided floating-point support to perform basic functions.
*/
#define do_fp_add(op1, op2, result)          ((result) = (op1) + (op2))
#define do_fp_subtract(op1, op2, result)     ((result) = (op1) - (op2))
#define do_fp_multiply(op1, op2, result)     ((result) = (op1) * (op2))
#define do_fp_divide(op1, op2, result)       ((result) = (op1) / (op2))
#define do_fp_negate(op1, result)            ((result) = -(op1))
#define do_fp_eq_zero(op)                    ((op) == 0.0)
#define do_fp_lt_zero(op)                    ((op) < 0.0)
#endif /* USE_SOFTFLOAT */

#ifdef NEED_HOST_FP_VALUE_IS_FINITE

static a_boolean host_fp_value_is_finite(a_host_fp_value  value)
/*
Test a floating-point value (long double or __float128) to see whether it is
finite (i.e., not a NaN or infinity).  Used when standard approaches like the
C99 macro isfinite are not available.  Note that if this routine returns
FALSE, it does not necessarily mean the value is infinite (it could be a NaN).
*/
{
  a_boolean     ld_finite;
  unsigned char *p = (unsigned char *)&value;
  unsigned int  exponent;

  /* As written, this routine supports only the size of exponent that
     comes up commonly in long doubles and __float128. */
  check_assertion_str(LDBL_MAX_EXP == 16384 || /*lint !e506*/
                      HOST_FP_VALUE_IS_128BIT,
                      "host_fp_value_is_finite: unsupported exponent size");
  if (host_little_endian) {
    /* Some long doubles don't use all of the allocated space.  This routine
       is only used when the host floating point value is long double, so we
       can assume that a property of a host floating point value applies to
       a long double value too. */
    p += data_size_of_host_fp_value - (sizeof(unsigned char) * 2);
    exponent = ((unsigned)p[1] << CHAR_BIT) | p[0];
  } else {
    /* Big-endian host. */
    exponent = ((unsigned)p[0] << CHAR_BIT) | p[1];
  }  /* if */
  /* Drop the sign bit, then all ones in the exponent field means a NaN
     or infinity. */
  ld_finite = (exponent & 0x7fff) != 0x7fff;
  return ld_finite;
}  /* host_fp_value_is_finite */

#endif /* ifdef NEED_HOST_FP_VALUE_IS_FINITE */
#if NEED_EDG_ISNAN

a_boolean edg_isnan(__float128 x)
/*
The host does not provide an isnan overload for 128-bit floating-point values
so convert the value to long double and use that.
*/
{
  return isnan((long double)x);
}  /* edg_isnan(__float128) */


a_boolean edg_isnan(long double x)
/*
Wrapper for the long double overload of isnan to prevent unnecessary
conversions.
*/
{
  return isnan(x);
}  /* edg_isnan(long double) */


a_boolean edg_isnan(double x)
/*
Wrapper for the double overload of isnan to prevent unnecessary conversions.
*/
{
  return isnan(x);
}  /* edg_isnan(double) */


a_boolean edg_isnan(float x)
/*
Wrapper for the float overload of isnan to prevent unnecessary conversions.
*/
{
  return isnan(x);
}  /* edg_isnan(float) */

#endif /* NEED_EDG_ISNAN */

#if USE_DOUBLE_FOR_HOST_FP_VALUE && USE_HOST_FP_CONVERSION_ROUTINES
#ifdef SUNOS_STRTOD_BUG

static void init_strtod(void)
/*
Under SunOS, 4.0 at least, strtod has a bug -- an uninitialized stack
variable is referenced.  Calling this routine ensures that the variable
is cleared.
*/
{
  int temp[200]; /* Magic numbers. */
  temp[55] = 0;
}  /* init_strtod */

#endif /* ifdef SUNOS_STRTOD_BUG */

static double strtod_interface(a_const_char *str)
/*
Interface routine to call strtod.  Converts the string str to double, and
returns the converted value.  errno is set to zero for no error, a non-zero
value for any error.
*/
{
  double temp;

  errno = 0;
#ifdef SUNOS_STRTOD_BUG
  /* Under SunOS, 4.0 at least, strtod has a bug -- an uninitialized stack
     variable is referenced.  Calling this routine ensures that the variable
     is cleared. */
  init_strtod();
#endif /* ifdef SUNOS_STRTOD_BUG */
  /* strtod is used instead of atof because of a report that on some SGI
     systems errno==ERANGE is not set properly by atof. */
  temp = strtod(str, (char **)NULL);
  return temp;
}  /* strtod_interface */

#endif /* USE_DOUBLE_FOR_HOST_FP_VALUE && USE_HOST_FP_CONVERSION_ROUTINES */
#if USE_LONG_DOUBLE_FOR_HOST_FP_VALUE || APPROXIMATE_QUADMATH

#if DEBUG
/* GCOVR_EXCL_START */
void db_long_double(long double d)
/*
Display a long double, for debugging purposes.
*/
{
  fprintf(f_debug, "%.40Le\n", d);
}  /* db_long_double */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

#endif /* USE_LONG_DOUBLE_FOR_HOST_FP_VALUE || APPROXIMATE_QUADMATH */

#if USE_HOST_FP_CONVERSION_ROUTINES
#if USE_LONG_DOUBLE_FOR_HOST_FP_VALUE

static long double str_to_long_double(a_const_char *str)
/*
Convert a string to a long double.  Note that this routine uses host routines
sscanf/snprintf/sprintf_s to do the floating-point conversion and these library
routines typically can be configured to use different "locales".  For their use
in the front end, the LC_NUMERIC portion of the locale must specify that "." is
the radix point (set in host_envir_early_init).
*/
{
  long double   temp;
  char          buf[60];
  a_boolean     err = FALSE;
  a_const_char  *ptr;

  (void)sscanf(str, "%Lf", &temp);
  /* Check for overflow or underflow by converting the number back to a
     string. */
  if (detail::snprintf_impl(buf, sizeof(buf), "%.*Le", LDBL_DIG, temp) < 0) {
    err = TRUE;
  } else if (temp == 0.0L) {
    a_boolean	nonzero = FALSE;
    ptr = str;
    if (*ptr == '-') ptr++;
    /* The result value is zero, make sure the input string was all zeros. */
    for (;;) {
      char	ch = *ptr++;
      if (ch == '\0') break;
      if (ch == '.') continue;
      if (!isdigit((unsigned char)ch)) break;
      if (ch != '0') {
        nonzero = TRUE;
        break;
      }  /* if */
    }  /* for */
    err = nonzero;
  } else {
    /* If the result string is not numeric, assume it is something like
       "infinity". */
    ptr = buf;
    if (*ptr == '-') ptr++;
    err = !isdigit((unsigned char)*ptr);
  }  /* if */
  /* Set errno to indicate an error. */
  errno = err ? ERANGE : 0;
  return temp;
}  /* str_to_long_double */

#endif /* USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
#if USE_FLOAT128_FOR_HOST_FP_VALUE

static a_host_fp_value str_to_float128(a_const_char *str)
/*
Convert a string to a 128-bit floating point value.  This routine either relies
on the GNU quadmath library (when USE_QUADMATH_LIBRARY is TRUE) or it uses the
internal floating point routines (when USE_QUADMATH_LIBRARY is FALSE).
*/
{
  a_host_fp_value  result;
  a_boolean        err = FALSE;
#if USE_QUADMATH_LIBRARY
  a_const_char     *ptr;

  result = strtoflt128(str, (char**)NULL);
  if (result == 0.0L) {
    /* Check for underflow by checking whether the input string was all
       zeros. */
    a_boolean	nonzero = FALSE;
    ptr = str;
    if (*ptr == '-') ptr++;
    for (;;) {
      char	ch = *ptr++;
      if (ch == '\0') break;
      if (ch == '.') continue;
      if (!isdigit((unsigned char)ch)) break;
      if (ch != '0') {
        nonzero = TRUE;
        break;
      }  /* if */
    }  /* for */
    err = nonzero;
  } else {
    /* Check for overflow. */
    err = !is_finite(result);
  }  /* if */
#else /* !USE_QUADMATH_LIBRARY */
  an_fp_return_type res = read_float128((unsigned char *)&result, str,
                                        (int)strlen(str));
  check_assertion(!fp_is_error(res));
  if (gnu_mode) {
    /* For compatibility, ignore any errors in GNU mode. */
    err = FALSE;
  } else if (microsoft_mode && res == (an_fp_return_type)fp_ret_underflow) {
    /* Ignore underflow condition in Microsoft emulation mode. */
    err = FALSE;
  } else {
    err = fp_is_unusual(res);
  }  /* if */
#endif /* USE_QUADMATH_LIBRARY */
  /* Set errno to indicate an error. */
  errno = err ? ERANGE : 0;
  return result;
}  /* str_to_float128 */

#endif /* USE_FLOAT128_FOR_HOST_FP_VALUE */

static void str_to_host_float(a_const_char    *str,
                              a_host_fp_value *value)
/*
Convert the given string to a host floating point value (stored in *value).
*/
{
#if USE_FLOAT128_FOR_HOST_FP_VALUE
  *value = str_to_float128(str);
#else /* !USE_FLOAT_128_FOR_HOST_FP_VALUE */
#if USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
  /* Clear temp: Don't use assignment because on some platforms the
     non-significant bytes wouldn't be cleared. */
  memzero((char *)value, sizeof(a_host_fp_value));
  *value = str_to_long_double(str);
#else /* !USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
  *value = strtod_interface(str);
#endif /* USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
#endif /* USE_FLOAT128_FOR_HOST_FP_VALUE */
}  /* str_to_host_float */

#endif /* USE_HOST_FP_CONVERSION_ROUTINES */
#if DEBUG
/* GCOVR_EXCL_START */

void db_internal_float_value(an_internal_float_value *ifv)
/*
Display an internal floating-point value, for debugging purposes.
*/
{
  unsigned int i;

  for (i = 0; i < sizeof(a_host_fp_value); ++i) {
    fprintf(f_debug, "%02x ", (unsigned int)ifv->bytes[i]);
  }  /* for */
  fprintf(f_debug, "\n");
}  /* db_internal_float_value */

/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

static void conv_host_fp_to_float(a_host_fp_value	temp,
				  a_boolean		*err,
				  float			*result)
/*
Convert "temp" from a_host_fp_value (double, long double, or __float128) to
float.  Set "err" to TRUE if the conversion would result in overflow or
underflow.  If the conversion can be done, return the result in "result".
*/
{
#if USE_SOFTFLOAT
  /* Convert 128-bit floating-point number to 32-bit and set *err on
     overflow or underflow. */
  softfloat32_t f32_temp;
  softfloat_exceptionFlags = 0;
  f32_temp.soft = f128M_to_f32(&temp);
  if ((softfloat_exceptionFlags & softfloat_flag_overflow) != 0) {
    if (gnu_mode && is_finite(temp)) {
      /* GNU C and C++ silently use infinity for values that are too large. */
    } else {
      /* An overflow. */
      *err = TRUE;
    }  /* if */
  } else if (((softfloat_exceptionFlags & softfloat_flag_underflow) != 0) &&
             f32_eq(f32_temp.soft, f32_zero)) {
    /* An underflow to zero. */
    *err = TRUE;
  }  /* if */
  if (!*err) {
    *result = f32_temp.hard;
  }  /* if */
#else /* !USE_SOFTFLOAT */
  /* Ideally, we'd like to check that the conversion will not overflow before
     performing the conversion (to avoid floating-point exceptions).  If we
     have FLT_MAX (which we can stringize) and a routine to convert a string
     into a host floating-point value, we do the "up conversion" of FLT_MAX
     and compare it to the given value to detect overflow.  If the host type
     is __float128, we currently have no string-to-value conversion routine
     and that approach is not viable. */
#if USE_FLOAT128_FOR_HOST_FP_VALUE
#define CAN_DO_FLT_MAX_TEST FALSE
#else /* !USE_FLOAT128_FOR_HOST_FP_VALUE */
#if USING_ISO_C
#ifdef FLT_MAX
#define CAN_DO_FLT_MAX_TEST TRUE
#endif /* ifdef FLT_MAX */
#endif /* USING_ISO_C */
#ifndef CAN_DO_FLT_MAX_TEST
#define CAN_DO_FLT_MAX_TEST FALSE
#endif /* ifndef CAN_DO_FLT_MAX_TEST */
#endif /* USE_FLOAT128_FOR_HOST_FP_VALUE */

#if CAN_DO_FLT_MAX_TEST
  /* We can test for conversion overflow before doing the actual conversion,
     as outlined above. */
  STATIC_THREAD a_boolean	init_done = FALSE;
  STATIC_THREAD a_host_fp_value	host_fp_flt_max;
  STATIC_THREAD float			float_flt_max;
  /* Initialize host_fp_flt_max to FLT_MAX converted as a_host_fp_value.  This
     might be slightly larger than FLT_MAX evaluated as a float (because
     of greater precision), but it's what the conversion of the actual
     FLT_MAX will yield, so it's the right value to use for the overflow
     comparison.  float_flt_max is that value converted to float. */
  if (!init_done) {
    /* Macros to turn FLT_MAX into a string: */
#define str2_flt_max(x) #x
#define str1_flt_max(x) str2_flt_max(x)
    char buf_flt_max[] = str1_flt_max(FLT_MAX);
    char *str_flt_max = buf_flt_max;
    a_boolean strip_trailing_paren = FALSE;
#undef str2_flt_max
#undef str1_flt_max
    if (strncmp(str_flt_max, "((float)", 8) == 0 ||
        strncmp(str_flt_max, "float(", 6) == 0) {
      /* Some systems, e.g., HP-UX, define FLT_MAX with a cast, e.g.,
         "((float)3.40282347e+38)".  Also accept a function-style cast form.
         strtod cannot deal with the parentheses or the cast, so skip past
         them. */
      if (str_flt_max[0] == '(') {
        str_flt_max += 8;
      } else {
        str_flt_max += 6;
      }  /* if */
      strip_trailing_paren = TRUE;
    } else if (str_flt_max[0] == '(') {
      /* Look for the case where it's a parenthesized number. */
      str_flt_max++;
      strip_trailing_paren = TRUE;
    }  /* if */
    if (strip_trailing_paren) {
      char *tmp = strchr(str_flt_max, ')');
      check_assertion_str(tmp != NULL && tmp[1] == '\0' &&
                          isdigit((unsigned char)str_flt_max[0]),
                          "conv_host_fp_to_float: bad FLT_MAX definition");
      *tmp = '\0';
    }  /* if */
    /* Make sure str_flt_max is something that sscanf will parse. */
    check_assertion(isdigit((unsigned char)str_flt_max[0]));
#if USE_HOST_FP_CONVERSION_ROUTINES
#if USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
    host_fp_flt_max = str_to_long_double(str_flt_max);
#else /* !USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
    host_fp_flt_max = strtod_interface(str_flt_max);
#endif /* USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
    check_assertion_str2(errno == 0, "conv_host_fp_to_float:",
                         "error on conversion of FLT_MAX");
#else /* !USE_HOST_FP_CONVERSION_ROUTINES */
    { size_t            len = strlen(str_flt_max);
      check_assertion(len > 0);
      if (str_flt_max[len-1] == 'F') {
        /* Remove a trailing 'F', if any. */
        str_flt_max[len-1] = '\0';
        len--;
      }  /* if */
#if USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
      (void)read_long_double((unsigned char *)&host_fp_flt_max, str_flt_max,
                             len);
#else /* !USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
      (void)read_double((unsigned char *)&host_fp_flt_max, str_flt_max,
                        (int)len);
#endif /* USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
    }
#endif /* USE_HOST_FP_CONVERSION_ROUTINES */
    float_flt_max = (float)host_fp_flt_max;
    init_done = TRUE;
  }  /* if */
  if (
#if TARG_HAS_IEEE_FLOATING_POINT
      /* Don't test NaNs and infinities. */
      is_finite(temp) &&
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
      ((temp >= 0.0) ? temp > host_fp_flt_max : temp < -host_fp_flt_max)) {
#if __MSC__
    /* The Microsoft compiler (VC 6.0) produces incorrect code when
       compiling with optimization if this variable is not declared
       volatile. */
    volatile
#endif /* __MSC__ */
    float float_temp = (float)temp;
    if ((temp >= 0.0) ? (float_temp == float_flt_max) :    /*lint !e777*/
                        (float_temp == -float_flt_max)) {  /*lint !e777*/
      /* The number is slightly larger than the official maximum float, but
         on conversion to float it rounds to the maximum float, so it's
         okay. */
    } else if (gnu_mode) {
      /* GNU C and C++ silently use infinity for values that are too large. */
    } else {
      /* Overflow. */
      *err = TRUE;
    }  /* if */
  }  /* if */
#endif /* CAN_DO_FLT_MAX_TEST */
  if (!*err) {
    /* Convert to float and store a float in float_value. */
    float float_temp = (float)temp;
    *result = float_temp;
    /* The last condition in the test below is to prevent spurious underflow
       errors when -ffast-math is used.  The use of -ffast-math is not
       recommended. */
    if (float_temp == 0.0 && temp != 0.0 && float_temp != temp) {
      /* Underflow. */
      *err = TRUE;
#if !CAN_DO_FLT_MAX_TEST
    } else {
      /* FLT_MAX is not available.  Check for overflow.  This is crude,
         but it's hard to do much here that is portable. */
#if __MSC__
    /* The Microsoft compiler (VC 6.0) produces incorrect code when
       compiling with optimization if this variable is not declared
       volatile. */
      volatile
#endif /* __MSC__ */
      double double_temp;
      /* Convert back to double again to see if we get the same thing. */
      double_temp = (double)float_temp;
      if (double_temp == temp) {
        /* Got the original number back, so everything is okay.  This also
           handles NaNs and infinities in the source double, so they do not
           get into the tests below. */
      } else if (temp < 10000.0 && temp > -10000.0) {
        /* Assume that numbers in the range -10000.0 .. +10000.0 cannot
           overflow. */
#if TARG_HAS_IEEE_FLOATING_POINT
      } else if (!is_finite(temp)) {
        /* Don't test NaNs and infinities. */
      } else if (gnu_mode && is_finite(temp)) {
        /* GNU C and C++ silently uses infinity for values that are too
           large. */
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
      } else {
        /* One last shot -- on machines with NaNs and infinities, printing
           such a thing often prints "infinity" or the like.  Print the
           number and see if the first character is a digit.  Note that
           above we ruled out the case where the source double is a NaN
           or infinity. */
        char float_string[15];

        if (detail::snprintf_impl(float_string, sizeof(float_string), "%.2e",
                                                            float_temp) >= 0) {
          char *ptr = float_string;

          if (*ptr == '-') ptr++;
          if (!isdigit((unsigned char)*ptr)) {
            /* Probably overflow. */
            *err = TRUE;
          }  /* if */
        } else {
          *err = TRUE;
        }  /* if */
      }  /* if */
#endif /* !CAN_DO_FLT_MAX_TEST */
    }  /* if */
  }  /* if */
#undef CAN_DO_FLT_MAX_TEST
#endif /* USE_SOFTFLOAT */
}  /* conv_host_fp_to_float */


static void conv_host_fp_to_float16(a_host_fp_value temp,
                                    a_boolean       *err,
                                    EDG_float16_t   *result)
/*
Convert "temp" from a_host_fp_value (double, long double, or __float128) to
_Float16.  Set "err" to TRUE if the conversion would result in overflow or
underflow.  If the conversion can be done, return the result in "result".
*/
{
#if USE_SOFTFLOAT
  float16_t f16_temp;
  softfloat_exceptionFlags = 0;
  f16_temp = f128M_to_f16(&temp);
  if ((softfloat_exceptionFlags & softfloat_flag_overflow) != 0) {
    if (gnu_mode && is_finite(temp)) {
      /* GNU C and C++ silently uses infinity for values that are too
         large. */
    } else {
      /* An overflow. */
      *err = TRUE;
    }  /* if */
  } else if (((softfloat_exceptionFlags & softfloat_flag_underflow) != 0) &&
             f16_eq(f16_temp, f16_zero)) {
    /* An underflow to zero. */
    *err = TRUE;
  }  /* if */
  if (!*err) {
    *result = f16_temp;
  }  /* if */
#else /* !USE_SOFTFLOAT */
  /* Leverage the support for various configurations in
     conv_host_fp_to_float, then check the resulting value against the
     _Float16 range limitations. */
  float float_temp;
  conv_host_fp_to_float(temp, err, &float_temp);
  if (!*err) {
    /* The conversion to float succeeded. */
#if HOST_HAS_FLOAT16_TYPE
    /* Check the resulting value against the _Float16 range limits. */
#define MAX_FLOAT16_VAL 65504
    float abs_value = (float)fabs(float_temp);
    if (abs_value > MAX_FLOAT16_VAL ||
        ((EDG_float16_t)abs_value == 0.0 && abs_value != 0.0)) {
      /* The value would overflow or underflow. */
      *err = TRUE;
    } else {
      /* The value is within the representable range for _Float16. */
      *result = (EDG_float16_t)float_temp;
    }  /* if */
#undef MAX_FLOAT16_VAL
#else /* !HOST_HAS_FLOAT16_TYPE */
    /* Do a round-trip conversion through the mantissa/exponent
       representation to perform rounding to the _Float16 precision and
       check the resulting value against the _Float16 range limits. */
    an_internal_float_value val;
    long                    exponent = 0;
    a_mantissa              mantissa;
    a_boolean               is_negative;
    a_boolean               inexact;
    a_host_fp_value         host_val;
    store_host_fp_value(float_temp, fk_float, &val, err);
    load_hex_fp_value(&val, fk_float, &mantissa, &exponent, &is_negative,
                      /*restore_implicit_bit=*/TRUE);
    conv_mantissa_to_floating_point(&mantissa, &exponent, is_negative,
                                    fk_float16, &val,
                                    /*exponent_overfloat=*/FALSE, err,
                                    &inexact);
    host_val = fetch_host_fp_value(fk_float16, &val);
    *result = (EDG_float16_t)host_val;
#endif /* HOST_HAS_FLOAT16_TYPE */
  }  /* if */
#endif /* USE_SOFTFLOAT */
}  /* conv_host_fp_to_float16 */

#if !USE_DOUBLE_FOR_HOST_FP_VALUE

static void conv_host_fp_to_double(a_host_fp_value	temp,
				   a_boolean		*err,
		 		   double		*result)
/*
Convert "temp" from a_host_fp_value (which is long double or __float128 in
this case) to double.  Set "err" if the conversion would result in overflow or
underflow.  If the conversion can be done, return the result in "result".
*/
{
#if USE_SOFTFLOAT
  /* Convert 128-bit floating-point number to 64-bit and set *err on
     overflow or underflow. */
  softfloat64_t f64_temp;
  softfloat_exceptionFlags = 0;
  f64_temp.soft = f128M_to_f64(&temp);
  if ((softfloat_exceptionFlags & softfloat_flag_overflow) != 0) {
    if (gnu_mode && is_finite(temp)) {
      /* GNU C and C++ silently uses infinity for values that are too
         large. */
    } else {
      /* An overflow. */
      *err = TRUE;
    }  /* if */
  } else if (((softfloat_exceptionFlags & softfloat_flag_underflow) != 0) &&
             f64_eq(f64_temp.soft, f64_zero)) {
    /* An underflow to zero. */
    *err = TRUE;
  }  /* if */
  if (!*err) {
    *result = f64_temp.hard;
  }  /* if */
#else /* !USE_SOFTFLOAT */
  /* Ideally, we'd like to check that the conversion will not overflow before
     performing the conversion (to avoid floating-point exceptions).  If we
     have DBL_MAX (which we can stringize) and a routine to convert a string
     into a host floating-point value, we do the "up conversion" of DBL_MAX
     and compare it to the given value to detect overflow.  If the host type
     is __float128, we currently have no string-to-value conversion routine
     and that approach is not viable. */
#if USE_FLOAT128_FOR_HOST_FP_VALUE
#define CAN_DO_DBL_MAX_TEST FALSE
#else /* !USE_FLOAT128_FOR_HOST_FP_VALUE */
#if USING_ISO_C
#ifdef DBL_MAX
#define CAN_DO_DBL_MAX_TEST TRUE
#endif /* ifdef DBL_MAX */
#endif /* USING_ISO_C */
#ifndef CAN_DO_DBL_MAX_TEST
#define CAN_DO_DBL_MAX_TEST FALSE
#endif /* ifndef CAN_DO_DBL_MAX_TEST */
#endif /* USE_FLOAT128_FOR_HOST_FP_VALUE */

#if CAN_DO_DBL_MAX_TEST
  /* DBL_MAX is available, so we can use it to test for overflow.  We do
     this before converting to double in case an overflow on such a
     conversion would cause a float exception. */
  STATIC_THREAD a_boolean	init_done = FALSE;
  STATIC_THREAD a_host_fp_value	host_fp_dbl_max;
  STATIC_THREAD double			double_dbl_max;
  /* Initialize host_fp_dbl_max to DBL_MAX converted as a_host_fp_value.
     This might be slightly larger than DBL_MAX evaluated as a double (because
     of greater precision), but it's what the conversion of the actual
     DBL_MAX will yield, so it's the right value to use for the overflow
     comparison.  double_dbl_max is that value converted to double. */
  if (!init_done) {
    /* Macros to turn DBL_MAX into a string: */
#define str2_dbl_max(x) #x
#define str1_dbl_max(x) str2_dbl_max(x)
    char buf_dbl_max[] = str1_dbl_max(DBL_MAX);
    char *str_dbl_max = buf_dbl_max;
    a_boolean strip_trailing_paren = FALSE;
#undef str2_dbl_max
#undef str1_dbl_max
    if (strncmp(str_dbl_max, "((double)", 9) == 0 ||
        strncmp(str_dbl_max, "double(", 7) == 0) {
      /* Some systems, e.g., Linux with gcc 4.5 and later, define DBL_MAX with
         a cast, e.g., "((double)1.79769313486231570815e+308L)".  Starting
         with g++ 4.6.0, the string "double(1.79769313486231570815e+308L)"
         is used.  strtod cannot deal with the parentheses or the cast, so
         skip past them. */
      if (str_dbl_max[0] == '(') {
        str_dbl_max += 9;
      } else {
        str_dbl_max += 7;
      }  /* if */
      strip_trailing_paren = TRUE;
    } else if (str_dbl_max[0] == '(') {
      /* Look for the case where it's a parenthesized number. */
      str_dbl_max++;
      strip_trailing_paren = TRUE;
    }  /* if */
    if (strip_trailing_paren) {
      char *tmp = strchr(str_dbl_max, ')');
      check_assertion_str(tmp != NULL && tmp[1] == '\0' &&
                          isdigit((unsigned char)str_dbl_max[0]),
                          "conv_host_fp_to_double: bad DBL_MAX definition");
      *tmp = '\0';
    }  /* if */
    /* Make sure str_dbl_max is something that sscanf will parse. */
    check_assertion(isdigit((unsigned char)str_dbl_max[0]));
#if USE_HOST_FP_CONVERSION_ROUTINES
    str_to_host_float(str_dbl_max, &host_fp_dbl_max);
    check_assertion_str2(errno == 0, "conv_host_fp_to_double:",
                         "error on conversion of DBL_MAX");
#else /* !USE_HOST_FP_CONVERSION_ROUTINES */
    { an_fp_return_type ret;
      size_t            len = strlen(str_dbl_max);
      check_assertion(len > 0);
      if (str_dbl_max[len-1] == 'L') {
        /* Remove a trailing 'L', if any. */
        str_dbl_max[len-1] = '\0';
        len--;
      }  /* if */
#if USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
      ret = read_long_double((unsigned char *)&host_fp_dbl_max, str_dbl_max,
                             len);
#else /* !USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
      ret = read_double((unsigned char *)&host_fp_dbl_max, str_dbl_max, len);
#endif /* USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
      check_assertion_str2(ret == fp_ret_valid, "conv_host_fp_to_double:",
                           "error on conversion of DBL_MAX");
    }
#endif /* USE_HOST_FP_CONVERSION_ROUTINES */
    double_dbl_max = (double)host_fp_dbl_max;
    init_done = TRUE;
  }  /* if */
  if (
#if TARG_HAS_IEEE_FLOATING_POINT
      /* Don't test NaNs and infinities. */
      is_finite(temp) &&
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
      ((temp >= 0.0) ? temp > host_fp_dbl_max
                     : temp < -host_fp_dbl_max)) {
    double double_temp = (double)temp;
    if ((temp >= 0.0) ? (double_temp == double_dbl_max) :    /*lint !e777*/
                        (double_temp == -double_dbl_max)) {  /*lint !e777*/
      /* The number is slightly larger than the official maximum double, but
         on conversion to double it rounds to the maximum double, so it's
         okay. */
    } else if (gnu_mode) {
      /* GNU C and C++ silently uses infinity for values that are too large. */
    } else {
      /* Overflow. */
      *err = TRUE;
    }  /* if */
  }  /* if */
#endif /* CAN_DO_DBL_MAX_TEST */
  if (!*err) {
    /* Convert to double and store a double in double_value. */
    double double_temp = (double)temp;
    *result = double_temp;
    /* The last condition in the test below is to prevent spurious underflow
       errors when -ffast-math is used.  The use of -ffast-math is not
       recommended. */
    if (double_temp == 0.0 && temp != 0.0 && double_temp != temp) {
      /* Underflow. */
      *err = TRUE;
#if !CAN_DO_DBL_MAX_TEST
    } else {
      /* DBL_MAX is not available.  Check for overflow.  This is crude,
         but it's hard to do much here that is portable. */
      double long_double_temp;
      /* Convert back to long double again to see if we get the same thing. */
      long_double_temp = (double)(long double)double_temp;
      if (long_double_temp == temp) {
        /* Got the original number back, so everything is okay.  This also
           handles NaNs and infinities in the source long double, so they
           do not get into the tests below. */
      } else if (temp < 10000.0 && temp > -10000.0) {
        /* Assume that numbers in the range -10000.0 .. +10000.0 cannot
           overflow. */
#if TARG_HAS_IEEE_FLOATING_POINT
      } else if (!is_finite(temp)) {
        /* Don't test NaNs and infinities. */
      } else if (gnu_mode && is_finite(temp)) {
        /* GNU C and C++ silently uses infinity for values that are too
           large. */
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
      } else {
        /* One last shot -- on machines with NaNs and infinities, printing
           such a thing often prints "infinity" or the like.  Print the
           number and see if the first character is a digit.  Note that
           above we ruled out the case where the source long double is a NaN
           or infinity. */
        char dbl_string[45];

        if (detail::snprintf_impl(dbl_string, sizeof(dbl_string), "%.2e",
                                                           double_temp) >= 0) {
          char *ptr = dbl_string;

          if (*ptr == '-') ptr++;
          if (!isdigit((unsigned char)*ptr)) {
            /* Probably overflow. */
            *err = TRUE;
          }  /* if */
        } else {
          *err = TRUE;
        }  /* if */
      }  /* if */
#endif /* !CAN_DO_DBL_MAX_TEST */
    }  /* if */
  }  /* if */
#undef CAN_DO_DBL_MAX_TEST
#endif /* USE_SOFTFLOAT */
}  /* conv_host_fp_to_double */

#endif /* !USE_DOUBLE_FOR_HOST_FP_VALUE */
#if HOST_FP_VALUE_IS_128BIT
#if !USE_SOFTFLOAT

static inline void zero_unused_bits_in_long_double(long double *ldbl)
/*
If the configuration is such that the host long double format has unused bits,
zero those so that a reliable hash can be computed from the resulting value.
*/
{
  if (targ_ldbl_mant_dig == 64) {
    /* Zero any unused bytes that exist in the 80-bit extended long double
       (that's 6 unused bytes in a 64-bit config and 2 bytes in a 32-bit
       config). */
    char *unused = (char *)ldbl + (host_little_endian ? 10 : 0);
    memzero(unused, sizeof(*ldbl) - 10U);
  }  /* if */
}  /* zero_unused_bits_in_long_double */

#endif /* !USE_SOFTFLOAT */

static void conv_host_fp_to_long_double(a_host_fp_value         val,
                                        a_boolean               *err,
                                        an_internal_float_value *result)
/*
Convert val from a_host_fp_value (__float128 or float128_t in this case) to a
target long double format.  Set "err" if the conversion would result in
overflow or underflow.  If the conversion can be done, return the result in
"result".
*/
{
#if USE_SOFTFLOAT
  /* "long double" can have various formats; handle the 80-, and 128-bit
     cases here (this should not be called for the 64-bit case). */
  softfloat_exceptionFlags = 0;
  check_assertion(targ_ldbl_mant_dig != 53);
  if (targ_ldbl_mant_dig == 64) {
    /* long double is 80 bits. */
    f128M_to_extF80M(&val, (extFloat80_t*)result);
  } else if (targ_ldbl_mant_dig == 113) {
    /* long double is 128 bits. */
    (void)memcpy((char *)result, (char *)&val, sizeof(val));
  } else {
    unexpected_condition();
  }  /* if */
  /* Use the same overflow condition as below. */
  if (is_finite(val) &&
      (softfloat_exceptionFlags & softfloat_flag_overflow) != 0 &&
      !gnu_mode) {
    /* An overflow. */
    *err = TRUE;
  }  /* if */
#else /* !USE_SOFTFLOAT */
  /* Use host conversion routines to convert from 128-bit float to long double.
     Note that this assumes that the host's long double format is the
     same as the target.  For cases where this doesn't hold (e.g., if the
     host is using 64-bit long double), the SoftFloat routines need to be used
     to do that conversion (a check is made in check_target_configuration). */
  long double     ldbl_val = (long double)val;
#if TARG_HAS_IEEE_FLOATING_POINT
  a_host_fp_value round_trip_val = ldbl_val;
  if (is_finite(val) && !is_finite(round_trip_val) && !gnu_mode) {
    *err = TRUE;
  }  /* if */
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
  /* In cases where 80-bit extended long double is used, the unused bits
     are indeterminate after the assignment above (especially in configurations
     that do optimization).  Zero those bits so that a reliable hash can be
     constructed (as even the unused bits are used by fp_hash). */
  zero_unused_bits_in_long_double(&ldbl_val);
  if (!*err) {
    /* Use memcpy to copy all of the bits (an assignment may copy 12 bytes
       in some cases). */
    (void)memcpy(result, &ldbl_val, sizeof(ldbl_val));
  }  /* if */
#endif /* USE_SOFTFLOAT */
}  /* conv_host_fp_to_long_double */

#endif /* HOST_FP_VALUE_IS_128BIT */

static void store_host_fp_value(a_host_fp_value         temp,
	                        a_float_kind            kind,
	                        an_internal_float_value *float_value,
	                        a_boolean               *err)
/*
Store the value in temp into float_value.  float_value has float_kind
kind.  Set *err TRUE if there is an error.  If *err is already TRUE,
do nothing.

Note that if the default versions of fp_same_representation and
fp_hash are used, this routine should zero the entire float_value
before setting it if there are unused bits.
*/
{
  if (!*err) {
    /* Zero the memory so that comparisons are easy even if we do not
       fill the whole area reserved for the float value. */
    memzero((char *)float_value, sizeof(an_internal_float_value));
    if (kind_is_binary16(kind)) {
      /* Converting to binary16. */
      EDG_float16_t float16_temp;
      conv_host_fp_to_float16(temp, err, &float16_temp);
      if (!*err) {
        (void)memcpy((char *)float_value, (char *)&float16_temp,
                     sizeof(EDG_float16_t));
      }  /* if */
    } else if (kind == fk_float || kind == fk_std_float32 ||
               kind == fk_std_bfloat16) {
      /* Converting to float or std::float32_t.  (Although std::bfloat16
         is a 16-bit type, it is internally represented as a float, so it
         is handled here also.) */
      float	float_temp;
      conv_host_fp_to_float(temp, err, &float_temp);
      if (!*err) {
        (void)memcpy((char *)float_value, (char *)&float_temp, sizeof(float));
      }  /* if */
#if !USE_DOUBLE_FOR_HOST_FP_VALUE
    } else if (kind_is_binary64(kind)) {
      /* Convert from an internal long double or __float128 to a double. */
      double	double_temp;
      conv_host_fp_to_double(temp, err, &double_temp);
      if (!*err) {
        (void)memcpy((char *)float_value, (char *)&double_temp,
                     sizeof(double));
      }  /* if */
#endif /* USE_DOUBLE_FOR_HOST_FP_VALUE */
#if HOST_FP_VALUE_IS_128BIT
    } else if (repr_is_long_double(kind)) {
      /* Convert from an internal __float128 to a target long double.  This
         can't be converted to a host long double (similar to the float and
         double cases above) because the long double format may differ between
         the host and the target. */
      conv_host_fp_to_long_double(temp, err, float_value);
#endif /* HOST_FP_VALUE_IS_128BIT */
    } else {
      /* Store a host floating value into a float_value of the same kind
         (either double or long double). */
      /* Use memcpy to copy the value since float_value might not be correctly
         aligned.  Also, we only copy the actual data bytes because the other
         bytes are unpredictable: Copying them would result in unreliable
         hash values for floating point a_constant entries. */
      (void)memcpy((char *)float_value, (char *)&temp,
                   data_size_of_host_fp_value);
    }  /* if */
  }  /* if */
}  /* store_host_fp_value */


/* This routine is external so that back ends can use it. */
a_host_fp_value fetch_host_fp_value(
				a_float_kind            kind,
				an_internal_float_value *float_value)
/*
Fetch the value from float_value (of kind kind) and return it.
*/
{
  a_host_fp_value	temp;

  /* Zero all bits in result (the assignments that follow may not set all
     bits in some cases). */
  memzero((char *)&temp, sizeof(temp));
  if (kind_is_binary16(kind)) {
    EDG_float16_t float16_temp;
    /* Convert from binary16 to a_host_fp_value. */
    /* Use memcpy to copy the value since float_value might not be correctly
       aligned. */
    (void)memcpy((char *)&float16_temp, (char *)float_value,
                 sizeof(EDG_float16_t));
#if USE_SOFTFLOAT
    f16_to_f128M(float16_temp, &temp);
#else /* !USE_SOFTFLOAT */
    temp = float16_temp;
#endif /* USE_SOFTFLOAT */
  } else if (kind == fk_float || kind == fk_std_float32 ||
             kind == fk_std_bfloat16) {
    float	float_temp;
    /* Convert from float or std::float32_t to a_host_fp_value.  (Although
       std::bfloat16 is a 16-bit type, it is represented internally as a
       float, so it is handled here also.) */
    /* Use memcpy to copy the value since float_value might not be correctly
       aligned. */
    (void)memcpy((char *)&float_temp, (char *)float_value, sizeof(float));
#if USE_SOFTFLOAT
    { softfloat32_t soft_temp;
      soft_temp.hard = float_temp;
      f32_to_f128M(soft_temp.soft, &temp);
    }
#else /* !USE_SOFTFLOAT */
    temp = float_temp;
#endif /* USE_SOFTFLOAT */
#if !USE_DOUBLE_FOR_HOST_FP_VALUE
  } else if (kind_is_binary64(kind)) {
    double	double_temp;
    /* Convert from double to a_host_fp_value. */
    /* Use memcpy to copy the value since float_value might not be correctly
       aligned. */
    (void)memcpy((char *)&double_temp, (char *)float_value, sizeof(double));
#if USE_SOFTFLOAT
    { softfloat64_t soft_temp;
      soft_temp.hard = double_temp;
      f64_to_f128M(soft_temp.soft, &temp);
    }
#else /* !USE_SOFTFLOAT */
    temp = double_temp;
#endif /* USE_SOFTFLOAT */
#endif /* !USE_DOUBLE_FOR_HOST_FP_VALUE */
#if HOST_FP_VALUE_IS_128BIT
  } else if (repr_is_long_double(kind)) {
    /* Convert from long double to a_host_fp_value (e.g., __float128). */
#if USE_SOFTFLOAT
    check_assertion(targ_ldbl_mant_dig != 53);
    if (targ_ldbl_mant_dig == 64) {
      /* long double is 80 bits. */
      extF80M_to_f128M((extFloat80_t *)float_value, &temp);
    } else if (targ_ldbl_mant_dig == 113) {
      /* long double is 128 bits. */
      (void)memcpy((char *)&temp, (char *)float_value,
                   targ_sizeof_long_double);
    } else {
      unexpected_condition();
    }  /* if */
#else /* !USE_SOFTFLOAT */
    long double	long_double_temp;
    /* Use memcpy to copy the value since float_value might not be correctly
       aligned. */
    (void)memcpy((char *)&long_double_temp, (char *)float_value,
                 sizeof(long double));
    temp = long_double_temp;
#endif /* USE_SOFTFLOAT */
#endif /* HOST_FP_VALUE_IS_128BIT */
  } else {
    /* float_value can be double, long double, or __float128. */
    /* Use memcpy to copy the value since float_value might not be correctly
       aligned. */
    (void)memcpy((char *)&temp, (char *)float_value, sizeof(a_host_fp_value));
  }  /* if */
  return temp;
}  /* fetch_host_fp_value */

#if TARG_HAS_IEEE_FLOATING_POINT

a_boolean make_fp_nan(an_internal_float_value *value,
                      a_float_kind             kind,
                      a_boolean	               signaling,
                      an_fp_value_part         mantissa)
/*
Make a Not-a-Number value of the given floating-point kind in *value.
Return FALSE if the operation did not succeed or if it is mode-dependent;
return TRUE otherwise.  If signaling is TRUE, a signaling Nan is created,
otherwise a quiet NaN is created.  When mantissa is non-zero, its value
is used for the mantissa portion of the NaN.  Note that this routine
limits the number of bits in the mantissa to 32 bits (or 23 bits for
float and 11 bits for _Float16).
*/
{
  a_boolean  err = FALSE, fp_mode_dependent = FALSE;
  union {
    float f;
    uint32_t u32;
  } u;

  /* Generate a positive NaN bit pattern. */
  if (signaling && !microsoft_bugs) {
    /* The exponent has all 1s and the most significant bit of the mantissa
       is 0 to indicate a signaling NaN.  The rest of the mantissa
       ("payload") will be set below. */
    u.u32 = 0x7f800000;
  } else {
    /* The exponent is all 1s and the most significant bit of the mantissa
       is 1 to indicate a quiet Nan. */
    u.u32 = 0x7fc00000;
  }  /* if */
  memzero((char *)value, sizeof(an_internal_float_value));
  (void)memcpy((char *)value, (char *)&u.f, sizeof(float));
  if (kind != (a_float_kind)fk_float
#if !(HOST_HAS_FLOAT16_TYPE || USE_SOFTFLOAT)
      && !kind_is_binary16(kind)
#endif /* !(HOST_HAS_FLOAT16_TYPE || USE_SOFTFLOAT) */
                                ) {
    /* Convert the NaN to the right type. */
    fp_change_kind(value, (a_float_kind)fk_float, value, kind,
                   &err, &fp_mode_dependent);
  }  /* if */
  if (mantissa != 0) {
    /* Set the mantissa portion of the floating-point value to the specified
       payload.  This code only sets the low-order 32 bits (23 bits in the
       case of a float type and 11 for _Float16). */
    an_fp_value_part *part, val;
    a_targ_size_t    size;
    /* Pointer to the first word. */
    part = (an_fp_value_part *)&value->bytes[0];
    if (!host_little_endian) {
      /* Use the last word. */
      if (kind_is_16bit(kind)) {
        size = 2;
      } else if (kind == fk_float) {
        size = targ_sizeof_float;
      } else if (repr_is_double(kind)) {
        size = targ_sizeof_double;
      } else if (repr_is_long_double(kind)) {
        size = targ_sizeof_long_double;
      } else if (kind == fk_float80) {
        size = targ_sizeof_float80;
      } else if (kind == fk_float128) {
        size = targ_sizeof_float128;
      } else if (kind == fk_std_float32) {
        size = 4;
      } else if (kind == fk_std_float64) {
        size = 8;
      } else if (kind == fk_std_float128) {
        size = 16;
      } else {
        size = 0;
        unexpected_condition_str("make_fp_nan: invalid float kind");
      }  /* if */
      part += size/4 - 1;
    }  /* if */
    /* Use memcpy to extract the appropriate 32-bit value, operate on it,
       then replace it (to avoid alignment issues). */
    (void)memcpy((char*)&val, (char*)part, sizeof(val));
#if HOST_HAS_FLOAT16_TYPE || USE_SOFTFLOAT
    if (kind_is_binary16(kind)) {
      /* Don't disturb non-mantissa bits. */
      val = val | (mantissa & 0x7ff);
    } else
#endif /* HOST_HAS_FLOAT16_TYPE || USE_SOFTFLOAT */
    /* Do not insert code here. */
    if (kind_is_16bit(kind) ||
        kind == fk_float || kind == fk_std_float32) {
      /* Don't disturb non-mantissa bits. */
      val = val | (mantissa & 0x7fffff);
    } else {
      /* Set the entire 32-bit piece of the mantissa. */
      val = mantissa;
    }  /* if */
    (void)memcpy((char*)part, (char*)&val, sizeof(val));
  } else if (signaling) {
    /* The mantissa of a NaN must be non-zero to distinguish it from an
       infinity.  If no explicit payload is specified, GNU and clang set
       the bit following the most significant bit of the mantissa to 1.
       Set byte_no and bit to designate the requisite bit in the result
       value and then set that bit to 1. */
    int           byte_no;
    unsigned char bit;
#if HOST_HAS_FLOAT16_TYPE || USE_SOFTFLOAT
    if (kind_is_binary16(kind)) {
      byte_no = host_little_endian ? 1 : 0;
      bit = 0x01;
    } else
#endif /* HOST_HAS_FLOAT16_TYPE || USE_SOFTFLOAT */
    /* Do not insert code here. */
    if (kind_is_16bit(kind) || kind == fk_float || kind == fk_std_float32) {
      byte_no = host_little_endian ? 2 : 1;
      bit = 0x20;
    } else if (num_mantissa_bits[(int)kind] == 53) {
      byte_no = host_little_endian ? 6 : 1;
      bit = 0x04;
    } else if (num_mantissa_bits[(int)kind] == 64) {
      byte_no = host_little_endian ? 7 : 2;
      bit = 0x20;
    } else {
#if HOST_FP_VALUE_IS_128BIT
      check_assertion(num_mantissa_bits[(int)kind] == 113);
      byte_no = host_little_endian ? 13 : 2;
      bit = 0x40;
#else /* !HOST_FP_VALUE_IS_128BIT */
      unexpected_condition();
#endif /* HOST_FP_VALUE_IS_128BIT */
    }  /* if */
    value->bytes[byte_no] |= bit;
  }  /* if */
  return !err && !fp_mode_dependent;
}  /* make_fp_nan */


a_boolean make_fp_infinity(an_internal_float_value *value,
                           a_float_kind             kind)
/*
Make a positive infinity value of the given floating-point kind in *value.
Return FALSE if the operation did not succeed or if it is mode-dependent;
return TRUE otherwise.
*/
{
  a_boolean  err = FALSE, fp_mode_dependent = FALSE;

  union {
    float f;
    uint32_t u32;
  } u;
  u.u32 = 0x7f800000;
  memzero((char *)value, sizeof(an_internal_float_value));
  (void)memcpy((char *)value, (char *)&u.f, sizeof(float));
  if (kind != (a_float_kind)fk_float
#if !(HOST_HAS_FLOAT16_TYPE || USE_SOFTFLOAT)
      && !kind_is_binary16(kind)
#endif /* !(HOST_HAS_FLOAT16_TYPE || USE_SOFTFLOAT) */
                                ) {
    /* Convert the infinity to the right type. */
    fp_change_kind(value, (a_float_kind)fk_float, value, kind,
                   &err, &fp_mode_dependent);
  }  /* if */
  return !err && !fp_mode_dependent;
}  /* make_fp_infinity */


a_boolean fp_is_nan(an_internal_float_value  *value,
                    a_float_kind             kind)
/*
Return TRUE if value is not-a-number.  kind specifies the floating-point kind
of value.
*/
{
  a_boolean		result = FALSE;
  a_host_fp_value	temp;

  temp = fetch_host_fp_value(kind, value);
  if (is_NaN(temp)) {
    result = TRUE;
  }  /* if */
  return result;
}  /* fp_is_nan */


a_boolean fp_is_infinity(an_internal_float_value  *value,
                         a_float_kind             kind)
/*
Return TRUE if value is infinity.  kind specifies the floating-point kind of
value.
*/
{
  a_boolean		result = FALSE;
  a_host_fp_value	temp;

  if (fp_is_nan(value, kind)) {
    result = FALSE;
  } else {
    temp = fetch_host_fp_value(kind, value);
    if (!is_finite(temp)) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* fp_is_infinity */

#if BUILTIN_FUNCTIONS_ENABLED

static a_boolean get_biased_exponent_if_possible(
                                         an_internal_float_value  *value,
                                         a_float_kind             kind,
                                         long                     *biased_exp)
/*
If the given floating point value has a known encoding format, set *biased_exp
to the biased exponent value encoded in that format and return TRUE.
Otherwise, return FALSE.
*/
{
  a_boolean         success = TRUE;
  an_fp_value_part  fp_part;
  char              *fp_bytes = (char*)value;

#if HOST_HAS_FLOAT16_TYPE || USE_SOFTFLOAT
  if (kind_is_binary16(kind)) {
    /* A 16-bit floating-point value. */
    memcpy((char*)&fp_part, fp_bytes, 2);
    *biased_exp = (long)((fp_part & 0x7c000000) >> 26);
  } else
#endif /* HOST_HAS_FLOAT16_TYPE || USE_SOFTFLOAT */
  /* Do not insert code here. */
  if (kind_is_16bit(kind) ||
      kind == fk_float || kind == fk_std_float32) {
    /* A single-precision floating-point value. */
    memcpy((char*)&fp_part, fp_bytes, sizeof(fp_part));
    *biased_exp = (long)((fp_part & 0x7f800000) >> 23);
  } else if (kind_is_binary64(kind)) {
    /* A 64-bit floating-point representation (with 53 mantissa bits).
       On little-endian systems, the most significant part is the second
       (i.e., last) word. */
    if (host_little_endian) fp_bytes += sizeof(fp_part);
    memcpy((char*)&fp_part, fp_bytes, sizeof(fp_part));
    *biased_exp = (long)((fp_part & 0x7fffffff) >> 20);
#if !USE_DOUBLE_FOR_HOST_FP_VALUE
  } else if (repr_is_long_double(kind)) {
    if (targ_ldbl_mant_dig == 64) {
      /* In little-endian 80/96-bit long double representations, the most
         significant part is the third (i.e., last) word. */
      if (host_little_endian) fp_bytes += 2*sizeof(fp_part);
      memcpy((char*)&fp_part, fp_bytes, sizeof(fp_part));
      *biased_exp = (long)(fp_part & 0x7fff);
    } else if (targ_ldbl_mant_dig == 113) {
      /* In little-endian 128-bit long double representations, the most
         significant part is the fourth (i.e., last) word. */
      if (host_little_endian) fp_bytes += 3*sizeof(fp_part);
      memcpy((char*)&fp_part, fp_bytes, sizeof(fp_part));
      *biased_exp = (long)((fp_part & 0x7fffffff) >> 16);
    } else {
      *biased_exp = -1;
      success = FALSE;
    }  /* if */
  } else if (kind == (a_float_kind)fk_float80) {
    if (targ_flt80_mant_dig == 64) {
      /* In little-endian 80/96-bit __float80 representations, the most
         significant part is the third (i.e., last) word. */
      if (host_little_endian) fp_bytes += 2*sizeof(fp_part);
      memcpy((char*)&fp_part, fp_bytes, sizeof(fp_part));
      *biased_exp = (long)(fp_part & 0x7fff);
    } else {
      *biased_exp = -1;
      success = FALSE;
    }  /* if */
  } else if (kind == fk_float128 || kind == fk_std_float128) {
    if (targ_flt128_mant_dig == 113) {
      /* In little-endian __float128 representations, the most significant
         part is the fourth (i.e., last) word. */
      if (host_little_endian) fp_bytes += 3*sizeof(fp_part);
      memcpy((char*)&fp_part, fp_bytes, sizeof(fp_part));
      *biased_exp = (long)((fp_part & 0x7fffffff) >> 16);
    } else {
      *biased_exp = -1;
      success = FALSE;
    }  /* if */
#endif /* !USE_DOUBLE_FOR_HOST_FP_VALUE */
  } else {
    *biased_exp = -1;
    success = FALSE;
  }  /* if */
  return success;
}  /* get_biased_exponent_if_possible */


a_boolean fp_is_normalized(an_internal_float_value  *value,
                           a_float_kind             kind,
                           a_boolean                *unknown)
/*
Return TRUE if value is known to have a normalized floating point
representation (i.e., it is not the encoding of an infinity or NaN, and the
encoded exponent is not zero).  In such cases also set *unknown to FALSE.
Return TRUE also if the host encoding of floating-point value is not
sufficiently known to determine whether the given value is normalized; in
that case set *unknown to TRUE.  Otherwise, return FALSE.
*/
{
  a_boolean  result;

  *unknown = FALSE;
  if (fp_is_infinity(value, kind)) {
    result = FALSE;
  } else if (fp_is_nan(value, kind)) {
    result = FALSE;
  } else {
    /* Check if the exponent is encoded as zeros.  The location of the zeros
       depends on the precision (i.e., the IEEE encoding format). */
    long  biased_exp = 0;
    if (get_biased_exponent_if_possible(value, kind, &biased_exp)) {
      result = (biased_exp > 0);
    } else {
      result = TRUE;
      *unknown = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* fp_is_normalized */
  
#endif /* BUILTIN_FUNCTIONS_ENABLED */
#if FIXED_POINT_ALLOWED

a_boolean fp_is_nan_or_infinity(an_internal_float_value	*value,
				a_float_kind		kind)
/*
Return TRUE if value is not-a-number or infinity.  kind specifies the
floating-point kind of value.
*/
{
  a_boolean		result = FALSE;
  a_host_fp_value	temp;

  temp = fetch_host_fp_value(kind, value);
  if (is_NaN(temp) || !is_finite(temp)) {
    result = TRUE;
  }  /* if */
  return result;
}  /* fp_is_nan_or_infinity */

#endif /* FIXED_POINT_ALLOWED */
#endif /* TARG_HAS_IEEE_FLOATING_POINT */

void fp_change_kind(an_internal_float_value *old_value,
                    a_float_kind            old_kind,
                    an_internal_float_value *new_value,
                    a_float_kind            new_kind,
                    a_boolean               *err,
                    a_boolean               *depends_on_fp_mode)
/*
Move *old_value to *new_value, changing the float kind from old_kind to
new_kind.  If there is an error, return *err TRUE.  If the result
depends on the floating-point mode, *depends_on_fp_mode is returned TRUE
(*new_value is set anyway).
*/
{
  a_host_fp_value temp;

  /* Note that conversion between float and double must not be done unless
     it is required, since the contents of the value might be hollerith
     or hex/octal data which might be disturbed by the unnecessary
     conversion.  If a conversion is required, we can assume the value
     is a floating-point constant. */
  *err = FALSE;
  *depends_on_fp_mode = FALSE;
  if (old_kind != new_kind) {
    /* There is a change of size.  Fetch the old, convert, store the new. */
    temp = fetch_host_fp_value(old_kind, old_value);
    store_host_fp_value(temp, new_kind, new_value, err);
  } else {
    /* There is no change of size, so just copy. */
    /* Use memcpy to copy the value since the values might not be correctly
       aligned. */
    (void)memcpy((char *)new_value, (char *)old_value,
                 sizeof(an_internal_float_value));
  }  /* if */
}  /* fp_change_kind */


a_boolean make_huge_fp_val(an_internal_float_value  *value,
                           a_float_kind             kind)
/*
Store the maximum floating-point value of the given kind in *value.  Return
FALSE if no such value can be produced; TRUE otherwise.  On IEEE floating-
point targets, the maximum value is positive infinity.
*/
{
  a_boolean  result;

  if (long_double_is_double && repr_is_long_double(kind)) {
    /* When long double is mapped onto double, store this value as a double. */
    kind = (a_float_kind)fk_double;
  }  /* if */
#if TARG_HAS_IEEE_FLOATING_POINT
  {
    /* With IEEE floating point, the generated value should be positive
       infinity. */
    result = make_fp_infinity(value, kind);
  }
#else /* !TARG_HAS_IEEE_FLOATING_POINT */
  /* This is not an IEEE floating-point platform.  Use the configured
     maximum floating-point values if available. */
  memzero((char *)value, sizeof(an_internal_float_value));
  switch (kind) {
#ifdef TARG_FLT_MAX
    case fk_float:
      {
        float  max_float_value = TARG_FLT_MAX;
        (void)memcpy((char *)value, (char *)&max_float_value, sizeof(float));
        result = TRUE;
      }
      break;
#endif /* TARG_FLT_MAX */
#ifdef TARG_DBL_MAX
    case fk_float32x:
    case fk_double:
      {
        double  max_double_value = TARG_DBL_MAX;
        (void)memcpy((char *)value, (char *)&max_double_value, sizeof(double));
        result = TRUE;
      }
      break;
#endif /* TARG_DBL_MAX */
#ifdef TARG_LDBL_MAX
    case fk_float64x:
    case fk_long_double:
      {
        long double  max_long_double_value = TARG_LDBL_MAX;
        (void)memcpy((char *)value, (char *)&max_long_double_value,
                     sizeof(long double));
        result = TRUE;
      }
      break;
#endif /* TARG_LDBL_MAX */
    default:
      result = FALSE;
  }  /* switch */
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
  return result;
}  /* make_huge_fp_val */


void init_mantissa(a_mantissa_ptr	mp)
/*
Clear the fields of a mantissa entry.
*/
{
  memzero((char*)mp->parts, sizeof(mp->parts));
  mp->underflow = FALSE;
}  /* init_mantissa */

#if DEBUG
/* GCOVR_EXCL_START */

void db_mantissa(a_mantissa_ptr	mp)
/*
Display a mantissa value, for debugging purposes.
*/
{
  int	i;

  for (i = 0; i < MANTISSA_PARTS; i++) {
    fprintf(f_debug, "%08lx", (unsigned long)mp->parts[i]);
  }  /* for */
  fprintf(f_debug, "\n");
}  /* db_mantissa */

/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

void shift_left_mantissa(a_mantissa_ptr	mp,
			 int		bits)
/*
Shift the mantissa in "mp" left by "bits".  "bits" must be less than 32.
*/
{
  int	part;

  check_assertion(bits < 32);
  for (part = 0; part < MANTISSA_PARTS; part++) {
    int			next_part_number = part + 1;
    an_fp_value_part	next_part;
    /* Get the next part value, or use zero if we're on the last part. */
    next_part = next_part_number == MANTISSA_PARTS ?
                                               0 : mp->parts[next_part_number];
    mp->parts[part] = mp->parts[part] << bits | next_part >> (32 - bits);
  }  /* for */
}  /* shift_left_mantissa */


void shift_right_mantissa(a_mantissa_ptr	mp,
			  int			bits)
/*
Shift the mantissa in "mp" right by "bits".
*/
{
  int	part;

  /* If the shift count is greater than 32, shift entire parts until the
     shift count is in range. */
  for (; bits >= 32; bits -= 32) {
    /* Determine whether any bits are being shifted out of the mantissa. */
    if (mp->parts[MANTISSA_PARTS - 1] != 0) mp->underflow = TRUE;
    for (part = MANTISSA_PARTS - 1; part > 0; part--) {
      mp->parts[part] = mp->parts[part - 1];
    }  /* for */
    mp->parts[0] = 0;
  }  /* for */
  if (bits != 0) {
    if ((mp->parts[MANTISSA_PARTS - 1] << (32 - bits)) != 0) {
      /* Determine whether any bits are being shifted out of the mantissa. */
      mp->underflow = TRUE;
    }  /* if */
    for (part = MANTISSA_PARTS - 1; part >= 0; part--) {
      an_fp_value_part	prev_part;
      /* Get the previous part value, or use zero if we're on the first
         part. */
      prev_part = part == 0 ? 0 : mp->parts[part - 1];
      mp->parts[part] = mp->parts[part] >> bits | prev_part << (32 - bits);
    }  /* for */
  }  /* if */
}  /* shift_right_mantissa */


static an_fp_value_part get_mask_for_bit(a_targ_size_t bit)
/*
Return a mask that can be used to test bit number "bit" of a mantissa.
*/
{
  return 0x80000000 >> (bit % 32);
}  /* get_mask_for_bit */


void round_hex_fp_value(a_mantissa_ptr  mp,
                        long            *exponent,
                        a_targ_size_t   value_bits,
                        a_boolean       is_fixed_point,
                        a_boolean       is_signed,
                        a_boolean       *inexact)
/*
Round the floating point value specified by "mp" and "exponent" to the
nearest value that can be represented by "value_bits" bits.  "is_fixed_point"
is TRUE when the value being rounded represents a fixed-point value.
"is_signed" is TRUE if the mantissa has a high-order sign bit.  This is
only used when rounding fixed-point values.
*/
{
  an_fp_value_part part;
  an_fp_value_part part_mask;
  an_fp_value_part half_way_value;
  a_targ_size_t    half_way_part_number;
  a_boolean        round_up = FALSE;
  a_targ_size_t    part_number;

  /* Determine whether to round up or down.  First, get the part that
     contains the high order bit on which the rounding begins. */
  half_way_part_number = value_bits / 32;
  part = mp->parts[half_way_part_number];
  half_way_value = get_mask_for_bit(value_bits);
  /* Mask off the portion of "part" above the bit on which the rounding
     starts. */
  part_mask = 0xffffffff >> (value_bits % 32);
  part = part & part_mask;
  if (part < half_way_value) {
    /* No rounding neeed. */
  } else if (part > half_way_value) {
    /* Round up. */
    round_up = TRUE;
  } else if (is_fixed_point) {
    /* Always round fixed-point values up if the value is equal to the
       half-way value. */
    round_up = TRUE;
  } else {
    /* This part is equal to the half-way value.  Check the remaining
       parts to see which way to round. */
    an_fp_value_part	lsb_mask;
    for (part_number = half_way_part_number + 1; part_number < MANTISSA_PARTS;
         ++part_number) {
      if (mp->parts[part_number] > 0) {
        round_up = TRUE;
        break;
      }  /* if */
    }  /* for */
    /* If there were bits that were discarded, those would have caused us to
       round up at this point.  */
    if (!round_up && mp->underflow) round_up = TRUE;
    if (!round_up) {
      /* We reached the end of the number and still don't know which way to
         round.  Round in the direction that will make the last significant
         bit a zero. */
      lsb_mask = get_mask_for_bit(value_bits - 1);
      if ((mp->parts[(value_bits - 1) / 32] & lsb_mask) != 0) round_up = TRUE;
    }  /* if */
  }  /* if */
  if (round_up) {
    an_fp_value_part	orig_part;
    an_fp_value_part	increment_value;
    a_boolean		saved_underflow;
    /* Save the current underflow status of the mantissa in case it is
       modified by the shift that follows. */
    saved_underflow = mp->underflow;
    /* Shift the mantissa one bit to the right to guard against overflow
       in the rounding process. */
    shift_right_mantissa(mp, 1);
    part_number = half_way_part_number;
    part = mp->parts[part_number];
    /* Adjust the part mask to operate on the shifted value. */
    part_mask >>= 1;
    orig_part = part;
    /* Get the value that should be added to round up the value.  Because we've
       shifted the mantissa, this turns out to be the same as the half way
       value determined above. */
    increment_value = half_way_value;
    /* Increment the value.  Mask off the lower order bits for neatness. */
    part = (part + increment_value) & ~part_mask;
    mp->parts[part_number] = part;
    if (part < orig_part) {
      /* The rounding needs to propagate to the next part.  Note that this
         cannot occur when incrementing the first part because of the
         shift done above. */
      for (; part_number > 0; --part_number) {
        part = mp->parts[part_number - 1];
        ++part;
        mp->parts[part_number - 1] = part;
        if (part != 0) break;
      }  /* for */
    }  /* if */
    /* If we didn't overflow into the high order bit of the mantissa,
       shift the mantissa back to its original position.  For signed values,
       we need to consider overflow into the sign bit as an overflow. */
    if ((mp->parts[0] & (is_signed ? 0x40000000 : 0x80000000)) == 0) {
      shift_left_mantissa(mp, 1);
      /* Restore the previously saved underflow state. */
      mp->underflow = saved_underflow;
    } else {
      /* We're keeping the shifted value -- adjust the exponent. */
      (*exponent)++;
    }  /* if */
    /* Indicate that the rounding discarded some information. */
    *inexact = TRUE;
  }  /* if */
}  /* round_hex_fp_value */


unsigned number_of_bits_in_mantissa(a_mantissa_ptr mp,
                                    a_boolean      normalize)
/*
Compute the number of bits actually used to represent the mantissa
value.  If "normalize" is TRUE, don't count any zero bits before the
first bit that is set.
*/
{
  int			part;
  int			bits = 0;
  an_fp_value_part	part_val;

  /* Compute the bit number of the last bit. */
  for (part = MANTISSA_PARTS - 1; part >= 0; part--) {
    /* Find the first part with some nonzero bits. */
    part_val = mp->parts[part];
    if (part_val == 0) continue;
    /* Compute the number of bits present in this part. */
    bits = 32;
    if ((part_val & 0xffff) == 0) { part_val >>= 16; bits -= 16; }
    if ((part_val & 0xff) == 0) { part_val >>= 8; bits -= 8; }
    if ((part_val & 0xf) == 0) { part_val >>= 4; bits -= 4; }
    if ((part_val & 0x3) == 0) { part_val >>= 2; bits -= 2; }
    if ((part_val & 0x1) == 0) { bits -= 1; }
    /* Include the bits represented by the earlier parts of the exponent. */
    bits += part * 32;
    /* Exit the loop once we've found a non-zero part. */
    break;
  }  /* for */
  if (normalize && bits != 0 && (mp->parts[0] & 0x8000000) == 0) {
    /* The mantissa is not normalized.  Compute the number of the first bit
       that is set. */
    int	first_bit = 0;
    for (part = 0; part < MANTISSA_PARTS; part++) {
      part_val = mp->parts[part];
      if (part_val == 0) {
        /* The entire part has no bits set.  Move on to the next part. */
        first_bit += 32;
        continue;
      }  /* if */
      if ((part_val & 0xffff0000) == 0) { part_val <<= 16; first_bit += 16; }
      if ((part_val & 0xff000000) == 0) { part_val <<= 8; first_bit += 8; }
      if ((part_val & 0xf0000000) == 0) { part_val <<= 4; first_bit += 4; }
      if ((part_val & 0xC0000000) == 0) { part_val <<= 2; first_bit += 2; }
      if ((part_val & 0x80000000) == 0) { part_val <<= 1; first_bit += 1; }
      /* Stop once we've reached a non-zero part. */
      break;
    }  /* for */
    bits -= first_bit;
  }  /* if */
  check_assertion(bits >= 0);
  return (unsigned)bits;
}  /* number_of_bits_in_mantissa */

#if FIXED_POINT_ALLOWED

a_boolean mantissa_is_zero(a_mantissa_ptr	mp)
/*
Return TRUE if the mantissa is zero.
*/
{
  a_boolean	result = TRUE;
  int		part;

  for (part = 0; part < MANTISSA_PARTS; part++) {
    if (mp->parts[part] != 0) {
      result = FALSE;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* mantissa_is_zero */

#endif /* FIXED_POINT_ALLOWED */

static void check_and_denormalize_hex_fp_value(
			  a_mantissa_ptr		mp,
			  long				*exponent,
			  a_float_kind			kind,
	                  a_boolean			*err,
			  a_boolean			*inexact,
			  an_internal_float_value	*float_value)
/*
mp contains the mantissa of a floating point value.  Exponent is the
effective exponent to be used (the combination of an explicit exponent
and the implied exponent based on the position of the decimal point).
kind specifies the type of floating point value being used.

If the number of mantissa bits of the result type is insufficient to
represent the value exactly or if the value is too small to be represented
even by a denormalized value, set *inexact to TRUE.  If the exponent is too
large, set *err to TRUE.
*/
{
  int min_exp = 0;
  int max_exp = 0;
  int mant_dig = 0;
  int bits;

  if (long_double_is_double && repr_is_long_double(kind)) {
    /* When long double is mapped onto double, store this value as a double. */
    kind = (a_float_kind)fk_double;
  }  /* if */
  min_exp = min_exponent[(int)kind];
  max_exp = max_exponent[(int)kind];
  mant_dig = (int)num_mantissa_bits[(int)kind];
  /* Note that the minimum and maximum exponent values are actually both
     one greater than the values that should be used.  This is strange, but
     it is the way those values are specified by the C and C++ standards. */
  min_exp--;
  max_exp--;
  /* Compute the number of bits of mantissa that are present. */
  bits = (int)number_of_bits_in_mantissa(mp, /*normalize=*/FALSE);
  /* If the exponent is too small, see if we can represent the value by
     denormalizing it. */
  if (*exponent < min_exp) {
    int bits_needed, bits_total;
    int implicit_bits = kind_has_implicit_mantissa_bit(kind) ? 1 : 0;
    /* Compute the number of mantissa bits needed to represent the value in
       denormalized form.  bits_needed is the number of bits to shift in
       order to represent the difference between the specified exponent and
       the minimum normalized exponent.  bits_total is that bit-shift plus
       the number of bits in the specified mantissa value. */
    bits_needed = (int)(min_exp - *exponent);
    bits_total = bits_needed + bits + implicit_bits;
    if (bits_needed <= mant_dig) {
      /* We can denormalize the number (potentially losing some bits of
         precision).  Do the first shift and make the implicit first bit of
         the mantissa explicit. */
      if (implicit_bits != 0) {
        shift_right_mantissa(mp, 1);
        mp->parts[0] |= 0x80000000;
      }  /* if */
      /* Do the rest of the shift operation. */
      if (bits_needed > implicit_bits) {
        shift_right_mantissa(mp, (int)(bits_needed - implicit_bits));
      }  /* if */
      if (bits_total > mant_dig) {
        /* The denormalized form does not have enough bits of precision to
           represent the value exactly.  Round the value. */
        round_hex_fp_value(mp, exponent,
                           (a_targ_size_t)(mant_dig - implicit_bits),
                           /*is_fixed_point=*/FALSE, /*is_signed=*/FALSE,
                           inexact);
        /* Warn about the loss of precision. */
        *inexact = TRUE;
      }  /* if */
      /* Assign the special exponent used with denormalized values. */
      *exponent = min_exp - 1;
    }  /* if */
  }  /* if */
  /* See if the number of mantissa bits provided exceeds the mantissa size.
     mant_dig includes the implicit bit. */
  {
    /* Some long double kinds do not make use of an implicit mantissa bit. */
    int implicit_bits = kind_has_implicit_mantissa_bit(kind) ? 1 : 0;
    int value_bits = bits + implicit_bits;
    if (value_bits > mant_dig) {
      *inexact = TRUE;
    }  /* if */
  }
  /* Check for a value that cannot be represented.  The "min_exp - 1" is
     used to permit the special denormalized value. */
  if (*exponent < (min_exp - 1)) {
    /* Set the value to 0 and issue a warning about the underflow. */
    init_mantissa(mp);
    *exponent = min_exp - 1;
    *inexact = TRUE;
  } else if (*exponent > max_exp) {
#if TARG_HAS_IEEE_FLOATING_POINT
    if (gnu_mode) {
      /* gcc silently uses infinity for values out of range.  The error flag is
         still returned, but will be cleared. */
      (void)make_fp_infinity(float_value, kind);
    }  /* if */
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
    *err = TRUE;
  }  /* if */
}  /* check_and_denormalize_hex_fp_value */


void load_hex_fp_value(an_internal_float_value	*float_value,
		       a_float_kind		kind,
		       a_mantissa_ptr		mp,
		       long			*exponent,
		       a_boolean		*is_negative,
		       a_boolean		restore_implicit_bit)
/*
float_value contains an internal floating-point value.  Convert that value
into a mantissa and exponent.  kind specifies the type of floating point
value being used.  If restore_implicit_bit is TRUE and the representation
makes use of an implicit mantissa bit, the mantissa and exponent are
adjusted to make the implicit bit explicit.
*/
{
  int			offset;
  an_fp_value_part	*fp_ptr;
  an_fp_value_part	val;
  an_fp_value_part	fp_temp[4];
  a_boolean		is_zero = TRUE;

  /* Clear the mantissa value. */
  init_mantissa(mp);
  if (long_double_is_double && repr_is_long_double(kind)) {
    /* When long double is mapped onto double, load this value as a double. */
    kind = (a_float_kind)fk_double;
  }  /* if */
  fp_ptr = &fp_temp[0];
  if (host_little_endian) {
    /* On little endian systems, we start storing with the last 32-bit value
       and work backward. */
    offset = -1;
  } else {
    /* On big endian systems, we start storing with the first 32-bit value
       and work forward. */
    offset = 1;
  }  /* if */
  if (kind == (a_float_kind)fk_float) {
    memcpy((char*)&val, (char*)float_value, sizeof(val));
    mp->parts[0] = (val & 0x07ffffff) << 9;
    *exponent = (long)((val & 0x7f800000) >> 23) - 127;
    *is_negative = (val & 0x80000000) != 0;
    if ((val & 0x7fffffff) != 0) is_zero = FALSE;
  } else if (kind_is_binary64(kind)) {
    /* A double value or a long double that is being represented by a
       double value. */
    /* The code below extracts the value from fp_temp.  Copy the source to
       fp_temp. */
    memcpy((char*)fp_temp, (char*)float_value, sizeof(val) * 2);
    /* On little endian systems, the most significant part of the
       number is fetched in the second four bytes.  Note that when the
       long value is stored in memory, its byte order will be right for
       either kind of system. */
    /* Update the pointer to refer to the last 32-bit word of the value. */
    if (host_little_endian) fp_ptr += 1;
    val = *fp_ptr;
    if ((val & 0x7fffffff) != 0) is_zero = FALSE;
    mp->parts[0] = val << 12;
    *exponent = ((long)((val & 0x7fffffff) >> 20)) - 1023;
    *is_negative = (val & 0x80000000) != 0;
    fp_ptr += offset;
    val = *fp_ptr;
    if (val != 0) is_zero = FALSE;
    mp->parts[0] |= (val >> 20);
    mp->parts[1] = val << 12;
  } else if (((repr_is_long_double(kind) && targ_ldbl_mant_dig == 64) ||
              (kind == (a_float_kind)fk_float80 &&
               targ_flt80_mant_dig == 64)) &&
             /*lint --e(506)*/sizeof(a_host_fp_value) >= sizeof(val)*3) {
    /* 80-bit representation in a 96-bit container. */
    /* The code below constructs the value from fp_temp.  Copy the source to
       fp_temp. */
    memcpy((char*)fp_temp, (char*)float_value, sizeof(val) * 3);
    /* Update the pointer to refer to the last 32-bit word of the value. */
    if (host_little_endian) fp_ptr += 2;
    val = *fp_ptr;
    if ((val & 0x7fffffff) != 0) is_zero = FALSE;
    *exponent = (long)((val & 0x7fff)) - 16383;
    *is_negative = (val & 0x8000) != 0;
    fp_ptr += offset;
    if (*fp_ptr != 0) is_zero = FALSE;
    mp->parts[0] = *fp_ptr;
    fp_ptr += offset;
    if (*fp_ptr != 0) is_zero = FALSE;
    mp->parts[1] = *fp_ptr;
  } else if (((repr_is_long_double(kind) && targ_ldbl_mant_dig == 113) ||
             (kind == (a_float_kind)fk_float128 &&
              targ_flt128_mant_dig == 113)) &&
             /*lint --e(506)*/sizeof(a_host_fp_value) == sizeof(val)*4) {
    /* 128-bit representation. */
    /* The code below constructs the value from fp_temp.  Copy the source to
       fp_temp. */
    memcpy((char*)fp_temp, (char*)float_value, sizeof(val) * 4);
    /* Update the pointer to refer to the last 32-bit word of the value. */
    if (host_little_endian) fp_ptr += 3;
    val = *fp_ptr;
    if ((val & 0x7fffffff) != 0) is_zero = FALSE;
    *exponent = (long)(((val & 0x7fffffff) >> 16)) - 16383;
    *is_negative = (val & 0x80000000) != 0;
    mp->parts[0] = val << 16;
    fp_ptr += offset;
    if (*fp_ptr != 0) is_zero = FALSE;
    val = *fp_ptr;
    mp->parts[0] |= val >> 16;
    mp->parts[1] = val << 16;
    fp_ptr += offset;
    if (*fp_ptr != 0) is_zero = FALSE;
    val = *fp_ptr;
    mp->parts[1] |= val >> 16;
    mp->parts[2] = val << 16;
    val = *fp_ptr;
    fp_ptr += offset;
    if (*fp_ptr != 0) is_zero = FALSE;
    val = *fp_ptr;
    mp->parts[2] |= val >> 16;
    mp->parts[3] = val << 16;
    val = *fp_ptr;
  } else {
    unexpected_condition_str(repr_is_long_double(kind) ?
                                "load_hex_fp_value: bad long double size" :
                                "load_hex_fp_value: bad float kind");
  }  /* if */
  if (is_zero) {
    /* Reset the exponent and the is_negative flag if the value is zero. */
    *exponent = 0;
    *is_negative = FALSE;
  } else {
    if (restore_implicit_bit && kind_has_implicit_mantissa_bit(kind)) {
      /* Make explicit the implicit bit of the mantissa. */
      shift_right_mantissa(mp, 1);
      mp->parts[0] |= 0x80000000;
    }  /* if */
    /* The exponent as indicated needs to be adjusted for the implicit bit.
       Oddly, this must even be done when long double has no implicit bit. */
    (*exponent)++;
  }  /* if */
}  /* load_hex_fp_value */


static void store_hex_fp_value(a_mantissa_ptr		mp,
			       long			exponent,
			       a_boolean		is_negative,
			       a_float_kind		kind,
			       an_internal_float_value	*float_value,
			       a_boolean		any_digits)
/*
mp contains the hexadecimal digits specified as the mantissa value
of a floating point value.  Exponent is the effective exponent to
be used (the combination of an explicit exponent and the implied
exponent based on the position of the decimal point).  kind specifies
the type of floating point value being used.  any_digits is TRUE if
there were any non-zero digits specified (i.e., it is FALSE if the
value is zero).

Store the value into "float_value".  The value stored is of the kind
specified by kind.  When USE_LONG_DOUBLE_FOR_HOST_FP_VALUE is FALSE,
the long double kind will have already been mapped to double by the caller.
*/
{
  int			offset;
  an_fp_value_part	*fp_ptr;
  an_fp_value_part	val;
  an_fp_value_part	fp_temp[4];

  fp_ptr = &fp_temp[0];
  if (host_little_endian) {
    /* On little endian systems, we start storing with the last 32-bit value
       and work backward. */
    offset = -1;
  } else {
    /* On big endian systems, we start storing with the first 32-bit value
       and work forward. */
    offset = 1;
  }  /* if */
  /* Zero the memory so that comparisons are easy even if we do not
     fill the whole area reserved for the float value. */
  memzero((char *)float_value, sizeof(an_internal_float_value));
  if (!any_digits) {
    /* We need a flag to indicate that we had a zero, because we can end up
       with a zero mantissa because of the possible presence of an implicit
       bit. */
#if HOST_HAS_FLOAT16_TYPE || USE_SOFTFLOAT
  } else if (kind_is_binary16(kind)) {
    val = (an_fp_value_part)((mp->parts[0] >> 6) | ((exponent + 15) << 26));
    if (is_negative) {
      val |= 0x80000000;
    }  /* if */
    val >>= 16;
    memcpy((char*)float_value, (char*)&val, sizeof(val));
#endif /* HOST_HAS_FLOAT16_TYPE || USE_SOFTFLOAT */
  } else if (kind_is_binary16(kind) || kind == fk_float) {
    val = (an_fp_value_part)((mp->parts[0] >> 9) | ((exponent + 127) << 23));
    if (is_negative) val |= 0x80000000;
    memcpy((char*)float_value, (char*)&val, sizeof(val));
  } else if (kind_is_binary64(kind)) {
    /* A double value or a long double that is being represented by a
       double value. */
    /* On little endian systems, the most significant part of the
       number is stored in the second four bytes.  Note that when the
       long value is stored in memory, its byte order will be right for
       either kind of system. */
    /* Update the pointer to refer to the last 32-bit word of the value. */
    if (host_little_endian) fp_ptr += 1;
    val = (an_fp_value_part)(((exponent + 1023) << 20) | (mp->parts[0] >> 12));
    if (is_negative) val |= 0x80000000;
    *fp_ptr = val;
    val = (mp->parts[0] << 20) | (mp->parts[1] >> 12);
    fp_ptr += offset;
    *fp_ptr = val;
    /* The code above constructs the value in fp_temp.  Copy this to the
       destination value. */
    memcpy((char*)float_value, (char*)fp_temp, sizeof(val) * 2);
  } else if ((repr_is_long_double(kind) && targ_ldbl_mant_dig == 64) ||
             (kind == (a_float_kind)fk_float80 &&
              targ_flt80_mant_dig == 64)) {
    /* 80-bit representation in a 96-bit container. */
    /* Update the pointer to refer to the last 32-bit word of the value. */
    if (host_little_endian) fp_ptr += 2;
    val = (an_fp_value_part)((exponent + 16383));
    if (is_negative) val |= 0x8000;
    *fp_ptr = val;
    fp_ptr += offset;
    val = mp->parts[0];
    *fp_ptr = val;
    fp_ptr += offset;
    val = mp->parts[1];
    *fp_ptr = val;
    /* The code above constructs the value in fp_temp.  Copy this to the
       destination value. */
    memcpy((char*)float_value, (char*)fp_temp, sizeof(val) * 3);
  } else if (((repr_is_long_double(kind) && targ_ldbl_mant_dig == 113) ||
              (kind == (a_float_kind)fk_float128 &&
               targ_flt128_mant_dig == 113) ||
              is_extended_flt_kind(kind)) &&
             /*lint --e(506)*/sizeof(a_host_fp_value) == sizeof(val)*4) {
    /* 128-bit representation. */
    /* Update the pointer to refer to the last 32-bit word of the value. */
    if (host_little_endian) fp_ptr += 3;
    val = (an_fp_value_part)(((exponent + 16383) << 16) |
                             (mp->parts[0] >> 16));
    if (is_negative) val |= 0x80000000;
    *fp_ptr = val;
    fp_ptr += offset;
    val = (mp->parts[0] << 16) | (mp->parts[1] >> 16);
    *fp_ptr = val;
    fp_ptr += offset;
    val = (mp->parts[1] << 16) | (mp->parts[2] >> 16);
    *fp_ptr = val;
    fp_ptr += offset;
    val = (mp->parts[2] << 16) | (mp->parts[3] >> 16);
    *fp_ptr = val;
    /* The code above constructs the value in fp_temp.  Copy this to the
       destination value. */
    memcpy((char*)float_value, (char*)fp_temp, sizeof(val) * 4);
  } else {
    unexpected_condition_str("store_hex_fp_value: bad long double size");
  }  /* if */
}  /* store_hex_fp_value */


void conv_hex_string_to_mantissa_and_exponent(
				a_const_char		*str,
				a_mantissa_ptr		mantissa,
				long			*p_exponent,
				a_boolean		*exponent_overflow)
/*
Convert a hexadecimal floating-point number in the null-terminated
string str to internal form in mantissa and p_exponent.

The number is known to be syntactically correct, but may not be representable
(it may be too large or too small).  Most errors must be detected later when
we know what kind of constant we are dealing with.  exponent_overflow is
set to TRUE if the exponent is too large to represent.
*/
{
  long				exponent = 0;
  a_boolean			after_decimal = FALSE;
  int				part = 0;
  int				nibble_in_part = 0;
  a_boolean			too_many_digits = FALSE;
  a_boolean			bits_discarded = FALSE;

  *exponent_overflow = FALSE;
  /* Start by extracting the hex digits from the string.  An entry of
     kind a_mantissa is used to hold the mantissa information while
     building the floating point value.  While copying the hex digits we
     compute the exponent implied by the position of the decimal point. */
  check_assertion(*str == '0');
  str++;
  check_assertion(*str == 'x'|| *str == 'X');
  str++;
  init_mantissa(mantissa);
  /* Discard leading zeros. */
  while (*str == '0') str++;
  /*  Check for a decimal point. */
  if (*str == '.') {
    /* Discard the decimal point and any leading zeros after it.
       Adjust the implied exponent for zeros discarded after the decimal
       point. */
    after_decimal = TRUE;
    str++;
    while (*str == '0') {
      str++;
      exponent -= 4;
    }  /* while */
  }  /* if */
  for (; isxdigit((unsigned char)*str) || *str == '.'; str++) {
    if (*str == '.') {
      after_decimal = TRUE;
    } else if (too_many_digits) {
      /* The buffer is already full.  Discard any additional digits.
         Record whether any non-zero bits were discarded. */
      if (*str != '0') bits_discarded = TRUE;
    } else {
      /* Store the value into the buffer.  first_nibble is used to
         determine whether to update the first or second 4 bits of the
         byte. */
      an_fp_value_part	value;
      an_fp_value_part	shifted_value;
      value = hexvalue((unsigned char)(*str));
      /* Shift the value to the appropriate position based on which nibble
         of the part is being processed. */
      shifted_value = value << ((7 - nibble_in_part) * 4);
      mantissa->parts[part] |= shifted_value;
      /* If we've filled this part, move to the next one. */
      if (++nibble_in_part == 8) {
        part++;
        nibble_in_part = 0;
        if (part >= MANTISSA_PARTS) too_many_digits = TRUE;
      }  /* if */
    }  /* if */
    /* If this character precedes the decimal point, update the implied
       exponent. */
    if (!after_decimal) exponent += 4;
  }  /* for */
  /* Check for the presence of an exponent. */
  if (*str == 'p' || *str == 'P') {
    /* Bypass the 'p' or 'P'. */
    long      value = 0;
    a_boolean is_negative = FALSE;
    long      max_exp = 0;
    long      min_exp = 0;
    str++;
    /* Check for a sign on the exponent. */
    if (*str == '-') {
      is_negative = TRUE;
      str++;
    } else if (*str == '+') {
      str++;
    }  /* if */
#if USE_DOUBLE_FOR_HOST_FP_VALUE
    max_exp = max_exponent[(int)fk_double];
    min_exp = -min_exponent[(int)fk_double];
#endif /* USE_DOUBLE_FOR_HOST_FP_VALUE */
#if USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
    max_exp = max_exponent[(int)fk_long_double];
    min_exp = -min_exponent[(int)fk_long_double];
#endif /* USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
#if USE_FLOAT128_FOR_HOST_FP_VALUE || USE_SOFTFLOAT
    max_exp = max_exponent[(int)fk_float128];
    min_exp = -min_exponent[(int)fk_float128];
#endif /* USE_FLOAT128_FOR_HOST_FP_VALUE || USE_SOFTFLOAT */
    check_assertion(max_exp != 0);
    for (; isdigit((unsigned char)*str); str++) {
      if (value > (is_negative ? min_exp : max_exp)) {
        /* The value exceeds the largest possible exponent.  Stop
           accumulating the exponent at this point.  Note that this
           assumes that the exponent calculation will not exceed the
           size of a long, which should be safe given that even an
	   IEEE 128 bit floating-point value has only a 15 bit
           exponent. */
        *exponent_overflow = TRUE;
      } else {
        value = value * 10 + (*str - '0');
      }  /* if */
    }  /* for */
    if (is_negative) value = -value;
    /* Compute the actual exponent using the implied exponent and the
       explicitly specified one. */
    exponent += value;
  }  /* if */
  if (bits_discarded) {
    /* More bits were specified than can be represented in a mantissa.
       Set the underflow bit so that a warning will be issued. */
    mantissa->underflow = TRUE;
  }  /* if */
  *p_exponent = exponent;
}  /* conv_hex_string_to_mantissa_and_exponent */


void conv_mantissa_to_floating_point(
				a_mantissa_ptr			mp,
				long				*p_exponent,
				a_boolean			is_negative,
				a_float_kind			kind,
				an_internal_float_value		*float_value,
				a_boolean			overflow,
				a_boolean			*err,
				a_boolean			*inexact)
/*
Given a mantissa (mp) and exponent (*p_exponent) that represent a
floating-point value, check that the value is representable in the
destination type specified by kind.  is_negative is TRUE if the value to be
stored must be created as a negative value.  Update *p_exponent to reflect
adjustments to the exponent value.  Set *err on overflow.  Set *inexact if
any bits are lost because the precision of the destination type.  overflow
is TRUE if the value is already known to be too large (i.e., because the
exponent was out of range).
*/
{
  a_boolean     any_digits;
  a_targ_size_t mant_dig = 0;
  long          exponent = *p_exponent;

  *err = FALSE;
  if (long_double_is_double && repr_is_long_double(kind)) {
    /* When long double is mapped onto double, use double. */
    kind = (a_float_kind)fk_double;
  }  /* if */
  mant_dig = num_mantissa_bits[(int)kind];
  any_digits = number_of_bits_in_mantissa(mp, /*normalize=*/FALSE) != 0;
  /* Normalize the mantissa. */
  if (any_digits) {
    while ((mp->parts[0] & 0x80000000) == 0) {
      shift_left_mantissa(mp, 1);
      exponent--;
    }  /* while */
  }  /* if */
  if (any_digits) {
    /* Round the value to the nearest representable value. */
    round_hex_fp_value(mp, &exponent, (a_targ_size_t)mant_dig,
                       /*is_fixed_point=*/FALSE, /*is_signed=*/FALSE, inexact);
    if (kind_has_implicit_mantissa_bit(kind)) {
      /* Shift one bit further to have an implied initial one bit.  This is
         only done for floating point representations that use an implicit
         bit. */
      shift_left_mantissa(mp, 1);
    }  /* if */
    exponent--;
  } else {
    /* There were no digits specified.  Reset the exponent. */
    exponent = 0;
    overflow = FALSE;
  }  /* if */
  /* Set the error flag if the exponent was too large. */
  if (overflow) *err = TRUE;
#if DEBUG
/* GCOVR_EXCL_START */
  if (db_flag_is_set("fp_hex_string_to_float")) {
    fprintf(f_debug, "fp hex value: ");
    db_mantissa(mp);
    fprintf(f_debug, "exponent=%ld\n", exponent);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  /* Check whether the resulting value fits in the type being used. */
  check_and_denormalize_hex_fp_value(mp, &exponent, kind, err, inexact,
                                     float_value);
  /* Store the value in the appropriate kind of floating point value.  If
     the value is out of range, float_value will have already been set to
     infinity, so it is not updated here. */
  if (!*err) {
    store_hex_fp_value(mp, exponent, is_negative, kind, float_value,
                       any_digits);
  }  /* if */
  /* If an underflow occurred, set the flag that indicates that the resulting
     value is not an exact representation of the specified value. */
  if (mp->underflow) *inexact = mp->underflow;
  /* Update the exponent value to reflect any adjustments made above. */
  *p_exponent = exponent;
}  /* conv_mantissa_to_floating_point */


void fp_hex_string_to_float(a_float_kind		kind,
	                    a_const_char		*str,
	                    an_internal_float_value	*float_value,
	                    a_boolean			*err,
			    a_boolean			*inexact)
/*
Convert a hexadecimal floating-point number in the null-terminated
string str to internal form in *float_value.

The number is known to be syntactically correct, but may not be representable
(it may be too large or too small); if there's an error, return *err = TRUE.
The precision of the value is indicated by kind (float, double, long double);
full precision will be kept, but the value is checked to see that it will
fit in the indicated type.
*/
{
  long		exponent = 0;
  a_mantissa	mantissa;
  a_boolean	exponent_overflow = FALSE;

  *inexact = FALSE;
  /* Convert the string into a mantissa and exponent. */
  conv_hex_string_to_mantissa_and_exponent(str, &mantissa, &exponent,
                                           &exponent_overflow);
  conv_mantissa_to_floating_point(&mantissa, &exponent, /*is_negative=*/FALSE,
                                  kind, float_value, exponent_overflow,
                                  err, inexact);
#if TARG_HAS_IEEE_FLOATING_POINT
  if (*err) {
    /* Reset the error flag to prevent the overflow from being diagnosed.
       This only done when using IEEE floating point because the value
       is replaced with infinity when using IEEE floating point. */
    if (gnu_mode) *err = FALSE;
  }  /* if */
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
  if (!*err && is_extended_flt_kind(kind) &&
      exponent >= max_exponent[(int)kind]) {
    *err = TRUE;
  }  /* if */
}  /* fp_hex_string_to_float */


void fp_string_to_float(a_float_kind            kind,
                        a_const_char            *str,
                        an_internal_float_value *float_value,
                        a_boolean               *err)
/*
Convert the floating-point number in the null-terminated string str to
internal form in *float_value.  The number is known to be syntactically
correct, but may not be representable (it may be too large or too small);
if there's an error, return *err = TRUE.  The precision of the value
is indicated by kind (float, double, long double).  The value is checked
to see that it will fit in the indicated type.  The string need not have a
decimal point or exponent (it can look like an integer).  It may have a
leading "-" sign.

Note that if the default versions of fp_same_representation and
fp_hash are used, this routine should zero the entire float_value
before setting it if there are unused bits.
*/
{
#if USE_HOST_FP_CONVERSION_ROUTINES
  /* This is a simplistic version, which should probably be replaced by
     something "real" for a given implementation. */
  a_host_fp_value	temp;

  /* Convert the number to a host floating-point value first. */
  str_to_host_float(str, &temp);
  if (errno == ERANGE) {
    if (gnu_mode) {
      errno = 0;
    } else if (temp != 0.0 || microsoft_mode) {
      /* Do not give an error on cases that involve partial loss of
         significance,  e.g., extremely small values like 4.9e-324.
         In Microsoft mode, do not give an error on a small number that
         was converted to zero. */
      /* Do not clear the error for large values that overflow. */
      if ((temp >= 0.0) ? temp < 1.0 : temp > -1.0) errno = 0;
    }  /* if */
  }  /* if */
  if (errno == 0 && is_extended_flt_kind(kind) &&
      max_exponent[(int)kind] <= max_exponent[(int)fk_std_float64] &&
      min_exponent[(int)kind] >= min_exponent[(int)fk_std_float64]) {
    /* Check for overflow. */
    int exp;
    (void)frexpl(temp, &exp);

    /* Check against the maximum and (denormalized) minimum values. */
    int denormalized_min = (min_exponent[(int)kind] -
                            (int)num_mantissa_bits[(int)kind] - 1);
    if (exp > max_exponent[(int)kind] || exp < denormalized_min) {
      errno = ERANGE;
    }  /* if */
  }  /* if */
  *err = (errno != 0);
  store_host_fp_value(temp, kind, float_value, err);
#else /* !USE_HOST_FP_CONVERSION_ROUTINES */
  { an_fp_return_type       res;
    an_internal_float_value float_value_temp;
    /* Convert the string to the appropriate binary floating-point format and
       store the result (if there was no underflow or overflow on the
       conversion).  Separate routines are used for each floating-point type
       (rather than using the largest floating-point type and then calling
       store_host_fp_value to see if the value fits in the specified type). */
    /* Clear &float_value_temp: Don't use assignment because on some platforms
       the non-significant bytes wouldn't be cleared. */
    memzero((char *)&float_value_temp, sizeof(an_internal_float_value));
    if (kind_is_binary16(kind)) {
      res = read_float16((unsigned char *)&float_value_temp, str,
                         (int)strlen(str));
#if !HOST_HAS_FLOAT16_TYPE && !USE_SOFTFLOAT
      {
        /* Do a round-trip conversion through the mantissa/exponent
           representation to perform rounding to the _Float16 precision and
           check the resulting value against the _Float16 range limits. */
        long            exponent = 0;
        a_mantissa      mantissa;
        a_boolean       is_negative;
        a_boolean       inexact;
        load_hex_fp_value(&float_value_temp, fk_float, &mantissa, &exponent,
                          &is_negative, /*restore_implicit_bit=*/TRUE);
        conv_mantissa_to_floating_point(&mantissa, &exponent, is_negative,
                                        fk_float16, &float_value_temp,
                                        /*exponent_overfloat=*/FALSE, err,
                                        &inexact);
      }
#endif /* !HOST_HAS_FLOAT16_TYPE && !USE_SOFTFLOAT */
#if DEBUG
/* GCOVR_EXCL_START */
      if (db_flag_is_set("fp")) {
        fprintf(f_debug, "read_float16: res=%d\n", (int)res);
        fprintf(f_debug, "  %s\n  ", str);
        db_binary_float16((unsigned char *)&float_value_temp);
      }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
    } else if (kind == fk_float || kind == fk_std_float32 ||
               kind == fk_std_bfloat16) {
      res = read_float((unsigned char *)&float_value_temp, str,
                       (int)strlen(str));
#if DEBUG
/* GCOVR_EXCL_START */
      if (db_flag_is_set("fp")) {
        fprintf(f_debug, "read_float: res=%d\n", (int)res);
        fprintf(f_debug, "  %s\n  ", str);
        db_binary_float((unsigned char *)&float_value_temp);
      }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
    } else if (kind_is_binary64(kind)) {
      /* Either "double" or "long double", where "double" and "long double"
         are configured as binary64. */
      res = read_double((unsigned char *)&float_value_temp, str,
                        (int)strlen(str));
#if DEBUG
/* GCOVR_EXCL_START */
      if (db_flag_is_set("fp")) {
        fprintf(f_debug, "read_double: res=%d\n", (int)res);
        fprintf(f_debug, "  %s\n  ", str);
        db_binary_double((unsigned char *)&float_value_temp);
      }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
#if FLOAT80_ENABLING_POSSIBLE
    } else if (kind == (a_float_kind)fk_float80) {
      res = read_float80((unsigned char *)&float_value_temp, str,
                         (int)strlen(str));
#if DEBUG
/* GCOVR_EXCL_START */
      if (db_flag_is_set("fp")) {
        fprintf(f_debug, "read_float80: res=%d\n", (int)res);
        fprintf(f_debug, "  %s\n  ", str);
        db_binary_float80((unsigned char *)&float_value_temp);
      }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
#endif /* FLOAT80_ENABLING_POSSIBLE */
#if FLOAT128_ENABLING_POSSIBLE
    } else if (kind == fk_float128 || kind == fk_std_float128) {
      res = read_float128((unsigned char *)&float_value_temp, str,
                          (int)strlen(str));
#if DEBUG
/* GCOVR_EXCL_START */
      if (db_flag_is_set("fp")) {
        fprintf(f_debug, "read_float128: res=%d\n", (int)res);
        fprintf(f_debug, "  %s\n  ", str);
        db_binary_float128((unsigned char *)&float_value_temp);
      }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
#endif /* FLOAT128_ENABLING_POSSIBLE */
    } else {
#if FP_HAS_LONG_DOUBLE
      check_assertion(repr_is_long_double(kind));
      res = read_long_double((unsigned char *)&float_value_temp, str,
                             (int)strlen(str));
#if DEBUG
/* GCOVR_EXCL_START */
      if (db_flag_is_set("fp")) {
        fprintf(f_debug, "read_long_double: res=%d\n", (int)res);
        fprintf(f_debug, "  %s\n  ", str);
        db_binary_long_double((unsigned char *)&float_value_temp);
      }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
#else /* !FP_HAS_LONG_DOUBLE */
      unexpected_condition();
#endif /* FP_HAS_LONG_DOUBLE */
    }  /* if */
    check_assertion(!fp_is_error(res));
    if (gnu_mode) {
      /* For compatibility, ignore any errors in GNU mode. */
      *err = FALSE;
    } else if (microsoft_mode && res == (an_fp_return_type)fp_ret_underflow) {
      /* Ignore underflow condition in Microsoft emulation mode. */
      *err = FALSE;
    } else {
      *err = fp_is_unusual(res);
    }  /* if */
    /* Only do the assignment if there was no error. */
    if (!*err) {
      *float_value = float_value_temp;
    }  /* if */
  }
#endif /* USE_HOST_FP_CONVERSION_ROUTINES */
}  /* fp_string_to_float */


template<typename a_Dyn_array>
static a_boolean handle_fp_to_string_special_cases(
                                      a_float_kind               kind,
                                      an_internal_float_value    *float_value,
                                      a_boolean                  *pos_infinity,
                                      a_boolean                  *neg_infinity,
                                      a_boolean                  *not_a_number,
                                      ARG_UNUSED a_Dyn_array     *str,
                                      a_host_fp_value            *temp)
/*
The float value in float_value (with precision as indicated by kind) is being
converted by the caller into either a decimal or hexadecimal string; this
routine handles special cases which are common and returns TRUE if the
conversion is indeed a special case.  If the floating-point value is positive
infinity or negative infinity, return *pos_infinity or *neg_infinity set to
TRUE.  If the floating-point value is a NaN, return *not_a_number set to TRUE.
In these cases, an appropriate display string is returned in str (e.g., "NaN")
and TRUE is returned.  pos_infinity, neg_infinity, and not_a_number can be NULL
if the corresponding return value is not needed.  *temp is set to the value of
the floating-point value in internal host representation form.  The contents of
str will be unmodified if the routine returns FALSE.
*/
{
  a_boolean result = TRUE;

  if (pos_infinity != NULL) *pos_infinity = FALSE;
  if (neg_infinity != NULL) *neg_infinity = FALSE;
  if (not_a_number != NULL) *not_a_number = FALSE;
  *temp = fetch_host_fp_value(kind, float_value);
#if TARG_HAS_IEEE_FLOATING_POINT
  if (is_NaN(*temp)) {
    /* Not-a-number. */
    detail::append_string_literal(*str, "NaN");
    if (not_a_number != NULL) *not_a_number = TRUE;
  } else if (!is_finite(*temp)) {
    /* infinity. */
    if (do_fp_lt_zero(*temp)) {
      detail::append_string_literal(*str, "-Infinity");
      if (neg_infinity != NULL) *neg_infinity = TRUE;
    } else {
      detail::append_string_literal(*str, "+Infinity");
      if (pos_infinity != NULL) *pos_infinity = TRUE;
    }  /* if */
  } else if (do_fp_eq_zero(*temp) &&
             /*lint --e(2499)*/
             memcmp((char *)temp, (char *)&fp_zero,
                    size_t_arg(data_size_of_host_fp_value)) != 0) {
    /* Special handling to ensure that -0.0 comes out with the leading "-";
       some snprintf/sprintf_s implementations do not process that
       correctly. */
    detail::append_string_literal(*str, "-0.0");
  } else
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
  /* Do not insert code here. */
  {
    /* Not a special case. */
    result = FALSE;
  }  /* if */
  return result;
}  /* handle_fp_to_string_special_cases */

#if USE_QUADMATH_LIBRARY

template<typename a_Dyn_array, typename ...a_Format_arg>
static void append_using_quadmath_formatting(a_const_char *formatting_str,
                                             a_Dyn_array  &underlying_array,
                                             size_t       size_hint,
                                             a_Format_arg ...args)
/*
Append the characters of the formatted string produced by the given formatting
string and associated arguments into the underlying array.  size_hint is an
overestimate (i.e., maximum) number of characters this value might use (plus a
temporary null character -- for use by quadmath_snprintf).
*/
{
  size_t orig_size = underlying_array.length();

  /* Create space in the underlying array to write the arguments. */
  underlying_array.resize(orig_size + size_hint, '\0');

  /* Write the formatted string. */
  auto buff_ptr = &underlying_array[orig_size];
  int  chars_written = quadmath_snprintf(buff_ptr, size_hint, formatting_str,
                                         args...);
  /* If this assertion fails, there was an error writing the string. */
  check_assertion(chars_written > 0);
  /* Remove any extra characters (including the terminating null character
     added by quadmath_snprintf). */
  underlying_array.resize(orig_size + (size_t)chars_written, '\0');
}  /* append_using_quadmath_formatting */

#endif /* USE_QUADMATH_LIBRARY */

namespace {

/*
A struct used for designating an IL floating point value.
*/
struct an_il_fp_value {
  a_float_kind  kind;   /* The kind of front end floating point value
                           represented by this struct. */
  an_internal_float_value
                *float_value;
                        /* The front end floating point value. */
  a_boolean     *pos_infinity;
                        /* A pointer that if non-NULL will be set to TRUE if
                           the floating point value is a positive infinity. */
  a_boolean     *neg_infinity;
                        /* A pointer that if non-NULL will be set to TRUE if
                           the floating point value is a negative infinity. */
  a_boolean     *not_a_number;
                        /* A pointer that if non-NULL will be set to TRUE if
                           the floating point value is not a number. */
  an_il_fp_value(a_float_kind            kind_val,
                 an_internal_float_value *float_value_val,
                 a_boolean               *pos_infinity_val,
                 a_boolean               *neg_infinity_val,
                 a_boolean               *not_a_number_val)
    : kind(kind_val), float_value(float_value_val),
      pos_infinity(pos_infinity_val), neg_infinity(neg_infinity_val),
      not_a_number(not_a_number_val)
    {}
};  /* an_il_fp_value */

#if USE_HEX_FP_CONSTANTS_IN_GENERATED_CODE

/*
A struct used for designating an IL floating point constant hex value.
*/
struct an_il_hex_constant_fp_value {
  a_float_kind  kind;   /* The kind of front end floating point value
                           represented by this struct. */
  an_internal_float_value
                *float_value;
                        /* The front end floating point value. */
  a_boolean     *pos_infinity;
                        /* A pointer that if non-NULL will be set to TRUE if
                           the floating point value is a positive infinity. */
  a_boolean     *neg_infinity;
                        /* A pointer that if non-NULL will be set to TRUE if
                           the floating point value is a negative infinity. */
  a_boolean     *not_a_number;
                        /* A pointer that if non-NULL will be set to TRUE if
                           the floating point value is not a number. */
  an_il_hex_constant_fp_value(a_float_kind            kind_val,
                              an_internal_float_value *float_value_val,
                              a_boolean               *pos_infinity_val,
                              a_boolean               *neg_infinity_val,
                              a_boolean               *not_a_number_val)
    : kind(kind_val), float_value(float_value_val),
      pos_infinity(pos_infinity_val), neg_infinity(neg_infinity_val),
      not_a_number(not_a_number_val)
    {}
};  /* an_il_hex_constant_fp_value */

#endif /* USE_HEX_FP_CONSTANTS_IN_GENERATED_CODE */

#if IA64_ABI

/*
A struct used for designating an IL floating point hex value.
*/
struct an_il_hex_fp_value {
  a_float_kind  kind;   /* The kind of front end floating point value
                           represented by this struct. */
  an_internal_float_value
                *float_value;
                        /* The front end floating point value. */
  an_il_hex_fp_value(a_float_kind            kind_val,
                     an_internal_float_value *float_value_val)
    : kind(kind_val), float_value(float_value_val)
    {}
};  /* an_il_hex_fp_value */

#endif /* IA64_ABI */

}  /* namespace */

#if USE_HOST_FP_CONVERSION_ROUTINES && \
    !(USE_FLOAT128_FOR_HOST_FP_VALUE && !USE_QUADMATH_LIBRARY)

template<typename a_Dyn_array>
static inline void append_float_using_host_routines(
                                       a_Dyn_array           &underlying_array,
                                       size_t                size_hint,
                                       a_float_kind          float_kind,
                                       const a_host_fp_value &value)
/*
Append the characters for the given floating point value (of the given float
kind) to the underlying array.  size_hint is an overestimate (i.e., maximum)
number of characters this value might use (plus a temporary null character).
*/
{
#if USE_FLOAT128_FOR_HOST_FP_VALUE && USE_QUADMATH_LIBRARY
  if (kind_is_16bit(float_kind)) {
    append_using_quadmath_formatting("%.8Qg", underlying_array, size_hint,
                                     value);
  } else if (float_kind == fk_float || float_kind == fk_std_float32) {
    append_using_quadmath_formatting("%.10Qg", underlying_array, size_hint,
                                     value);
  } else if (repr_is_double(float_kind) || float_kind == fk_std_float64) {
    append_using_quadmath_formatting("%.19Qg", underlying_array, size_hint,
                                     value);
  } else if (float_kind == fk_float128 || float_kind == fk_std_float128) {
    append_using_quadmath_formatting("%.34Qg", underlying_array, size_hint,
                                     value);
  } else {
    /* fk_long_double or fk_float80. */
    /* In theory LDBL_DIG+1 digits should be enough as the precision, but
       LDBL_DIG+2 seems to help on some systems.  However, on Solaris, with
       128-bit long doubles, LDBL_DIG+2 hits the conversion of LDBL_MIN in a
       funny place with regard to rounding and the Sun CC compiler doesn't
       accept that value converted in that way.  So on systems with 128-bit
       long double, just stick with LDBL_DIG+1 when using the C++-generating
       back end. */
    int ldbl_digits = LDBL_DIG + 2;
#if BACK_END_IS_CP_GEN_BE
    if (LDBL_DIG > 30) ldbl_digits = LDBL_DIG + 1;
#endif /* BACK_END_IS_CP_GEN_BE */
    append_using_quadmath_formatting("%.*Qg", underlying_array, size_hint,
                                     ldbl_digits, value);
  }  /* if */
#else /* !(USE_FLOAT128_FOR_HOST_FP_VALUE && USE_QUADMATH_LIBRARY) */
#if USE_LONG_DOUBLE_FOR_HOST_FP_VALUE || APPROXIMATE_QUADMATH
  /* Make sure we have a long double value (value can be a __float128). */
  long double  fpval = (long double)value;
  if (kind_is_16bit(float_kind)) {
    detail::append_using_c_formatting("%.8Lg", underlying_array, size_hint,
                                      fpval);
  } else if (float_kind == fk_float || float_kind == fk_std_float32) {
    detail::append_using_c_formatting("%.10Lg", underlying_array, size_hint,
                                      fpval);
  } else if (repr_is_double(float_kind) || float_kind == fk_std_float64) {
    detail::append_using_c_formatting("%.19Lg", underlying_array, size_hint,
                                      fpval);
  } else {
    /* fk_long_double or fk_float80 or fk_std_float128. */
    /* In theory LDBL_DIG+1 digits should be enough as the precision, but
       LDBL_DIG+2 seems to help on some systems.  However, on Solaris, with
       128-bit long doubles, LDBL_DIG+2 hits the conversion of LDBL_MIN in a
       funny place with regard to rounding and the Sun CC compiler doesn't
       accept that value converted in that way.  So on systems with 128-bit
       long double, just stick with LDBL_DIG+1 when using the C++-generating
       back end. */
    int ldbl_digits = LDBL_DIG + 2;
#if BACK_END_IS_CP_GEN_BE
    if (LDBL_DIG > 30) ldbl_digits = LDBL_DIG + 1;
#endif /* BACK_END_IS_CP_GEN_BE */
    detail::append_using_c_formatting("%.*Lg", underlying_array, size_hint,
                                      ldbl_digits, fpval);
  }  /* if */
#endif /* USE_LONG_DOUBLE_FOR_HOST_FP_VALUE || APPROXIMATE_QUADMATH */
#if USE_DOUBLE_FOR_HOST_FP_VALUE
  if (kind_is_16bit(float_kind)) {
    detail::append_using_c_formatting("%.8g", underlying_array, size_hint,
                                      value);
  } else if (float_kind == fk_float || float_kind == fk_std_float32) {
    detail::append_using_c_formatting("%.10g", underlying_array, size_hint,
                                      value);
  } else {
    detail::append_using_c_formatting("%.19g", underlying_array, size_hint,
                                      value);
  }  /* if */
#endif /* USE_DOUBLE_FOR_HOST_FP_VALUE */
#endif /* USE_FLOAT128_FOR_HOST_FP_VALUE && USE_QUADMATH_LIBRARY */
  /* Add trailing ".0" if no decimal point was put out (meaning the value is a
     whole number). */
  if (strchr(underlying_array.begin(), '.') == NULL &&
      strchr(underlying_array.begin(), 'e') == NULL) {
    underlying_array.push_back('.');
    underlying_array.push_back('0');
  }  /* if */
}  /* append_float_using_host_routines */

#else /* !(USE_HOST_FP_CONVERSION_ROUTINES && !(USE_FLOAT128...))*/

template<typename a_Dyn_array>
static inline void append_float_using_internal_routines(
                                        a_Dyn_array          &underlying_array,
                                        size_t               size_hint,
                                        const an_il_fp_value &value)
/*
Append the characters for the given floating point value to the underlying
array.  size_hint is an overestimate (i.e., maximum) number of characters this
value might use (plus a temporary null character).
*/
{
  size_t orig_size = underlying_array.length();

  /* Create space in the underlying array to write the arguments. */
  underlying_array.resize(orig_size + size_hint, '\0');

  /* Append the float. */
  auto              buff_ptr = &underlying_array[orig_size];
  an_fp_return_type res;
  unsigned char     *float_as_char = (unsigned char *)value.float_value;
  if (kind_is_binary16(value.kind)) {
    res = write_float16(buff_ptr, (int)size_hint, float_as_char);
#if DEBUG
/* GCOVR_EXCL_START */
    if (db_flag_is_set("fp")) {
      fprintf(f_debug, "write_float16: res=%d\n  ", (int)res);
      db_binary_float16(float_as_char);
      fprintf(f_debug, "  %s\n", buff_ptr);
    }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  } else if (value.kind == fk_float || value.kind == fk_std_float32 ||
             value.kind == fk_std_bfloat16) {
    res = write_float(buff_ptr, (int)size_hint, float_as_char);
#if DEBUG
/* GCOVR_EXCL_START */
    if (db_flag_is_set("fp")) {
      fprintf(f_debug, "write_float: res=%d\n  ", (int)res);
      db_binary_float(float_as_char);
      fprintf(f_debug, "  %s\n", buff_ptr);
    }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  } else if (kind_is_binary64(value.kind)) {
    /* Either "double" or "long double", where "double" and "long double" are
       configured as binary64. */
    res = write_double(buff_ptr, (int)size_hint, float_as_char);
#if DEBUG
/* GCOVR_EXCL_START */
    if (db_flag_is_set("fp")) {
      fprintf(f_debug, "write_double: res=%d\n  ", (int)res);
      db_binary_double(float_as_char);
      fprintf(f_debug, "  %s\n", buff_ptr);
    }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
#if FLOAT80_ENABLING_POSSIBLE
  } else if (value.kind == fk_float80) {
    res = write_float80(buff_ptr, (int)size_hint, float_as_char);
#if DEBUG
/* GCOVR_EXCL_START */
    if (db_flag_is_set("fp")) {
      fprintf(f_debug, "write_float80: res=%d\n  ", (int)res);
      db_binary_float80(float_as_char);
      fprintf(f_debug, "  %s\n", buff_ptr);
    }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
#endif /* FLOAT80_ENABLING_POSSIBLE */
#if FLOAT128_ENABLING_POSSIBLE
  } else if (value.kind == fk_float128 || value.kind == fk_std_float128) {
    res = write_float128(buff_ptr, (int)size_hint, float_as_char);
#if DEBUG
/* GCOVR_EXCL_START */
    if (db_flag_is_set("fp")) {
      fprintf(f_debug, "write_float128: res=%d\n  ", (int)res);
      db_binary_float128(float_as_char);
      fprintf(f_debug, "  %s\n", buff_ptr);
    }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
#endif /* FLOAT128_ENABLING_POSSIBLE */
  } else {
#if FP_HAS_LONG_DOUBLE
    check_assertion(repr_is_long_double(value.kind));
    res = write_long_double(buff_ptr, (int)size_hint, float_as_char);
#if DEBUG
/* GCOVR_EXCL_START */
    if (db_flag_is_set("fp")) {
      fprintf(f_debug, "write_long_double: res=%d\n  ", (int)res);
      db_binary_long_double(float_as_char);
      fprintf(f_debug, "  %s\n", buff_ptr);
    }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
#else /* !FP_HAS_LONG_DOUBLE */
    unexpected_condition();
#endif /* FP_HAS_LONG_DOUBLE */
  }  /* if */
  switch (res) {
    case fp_ret_nan:
      underlying_array.resize(orig_size, '\0');
      detail::append_string_literal(underlying_array, "NaN");
      if (value.not_a_number != NULL) *value.not_a_number = TRUE;
      break;
    case fp_ret_pos_infinity:
      underlying_array.resize(orig_size, '\0');
      detail::append_string_literal(underlying_array, "+Infinity");
      if (value.pos_infinity != NULL) *value.pos_infinity = TRUE;
      break;
    case fp_ret_neg_infinity:
      underlying_array.resize(orig_size, '\0');
      detail::append_string_literal(underlying_array, "-Infinity");
      if (value.neg_infinity != NULL) *value.neg_infinity = TRUE;
      break;
    default:
      check_assertion(res != fp_ret_too_small);
      /* Drop unused space. */
      while (underlying_array.back_elem() == '\0') {
        underlying_array.pop_back();
      }  /* if */
      break;
  }  /* switch */
}  /* append_float_using_internal_routines */

#endif /* USE_HOST_FP_CONVERSION_ROUTINES && !(USE_FLOAT128...) */

namespace detail {

/*
A string formatter for an_il_fp_value values.  Note this string formatter has
side effects passed through an_il_fp_value (the pointees of the pos_infinity,
neg_infinity, and not_a_number data members will all be updated if not-NULL).
*/
template<>
struct String_formatter<an_il_fp_value> {
  static size_t size_hint_of(an_il_fp_value value)
    { return 49; }
  template<typename a_Dyn_array>
  static inline void append_into(a_Dyn_array          &underlying_array,
                                 const an_il_fp_value &value,
                                 size_t               size_hint);
};  /* String_formatter */


template<typename a_Dyn_array>
void String_formatter<an_il_fp_value>::append_into(
                                        a_Dyn_array          &underlying_array,
                                        const an_il_fp_value &value,
                                        size_t               size_hint)
/*
Append the characters for the given floating point value to the underlying
array.  size_hint is an overestimate (i.e., maximum) number of characters this
value might use (plus a temporary null character).
*/
{
  a_host_fp_value temp;

  if (!handle_fp_to_string_special_cases(value.kind, value.float_value,
                                         value.pos_infinity,
                                         value.neg_infinity,
                                         value.not_a_number, &underlying_array,
                                         &temp)) {
#if USE_HOST_FP_CONVERSION_ROUTINES && \
    !(USE_FLOAT128_FOR_HOST_FP_VALUE && !USE_QUADMATH_LIBRARY)
    /* The call to handle_fp_to_string_special_cases has loaded float_value
       into temp.  Use host conversion routines to format the value. */
    append_float_using_host_routines(underlying_array, size_hint, value.kind,
                                     temp);
#else /* !(USE_HOST_FP_CONVERSION_ROUTINES && !(USE_FLOAT128...)) */
    /* Use internal routines for doing the binary to string conversion. */
    append_float_using_internal_routines(underlying_array, size_hint, value);
#endif /* USE_HOST_FP_CONVERSION_ROUTINES && !(USE_FLOAT128...) */
  }  /* if */
}  /* append_into */

#if USE_HEX_FP_CONSTANTS_IN_GENERATED_CODE

/*
A string formatter for an_il_hex_constant_fp_value values.
*/
template<>
struct String_formatter<an_il_hex_constant_fp_value> {
  static size_t size_hint_of(an_il_hex_constant_fp_value value)
    { return 49; }
  template<typename a_Dyn_array>
  static inline void append_into(a_Dyn_array                 &underlying_array,
                                 an_il_hex_constant_fp_value value,
                                 size_t                      size_hint);
};  /* String_formatter */


template<typename a_Dyn_array>
void String_formatter<an_il_hex_constant_fp_value>::append_into(
                                 a_Dyn_array                 &underlying_array,
                                 an_il_hex_constant_fp_value value,
                                 size_t                      size_hint)
/*
Append the characters for the given floating point value to the underlying
array.  size_hint is an overestimate (i.e., maximum) number of characters this
value might use (plus a temporary null character).
*/
{
  a_host_fp_value temp;

  if (!handle_fp_to_string_special_cases(value.kind, value.float_value,
                                         value.pos_infinity,
                                         value.neg_infinity,
                                         value.not_a_number, &underlying_array,
                                         &temp)) {
    /* Copy the value to a properly aligned floating-point type and
       use C-formatting to generate the appropriate hexadecimal string. */
    if (kind_is_binary16(value.kind)) {
#if USE_SOFTFLOAT
      float16_t     f16_temp;
      softfloat32_t f32_temp;
      (void)memcpy((char *)&f16_temp, (char *)value.float_value, 2);
      f32_temp.soft = f16_to_f32(f16_temp);
      append_using_c_formatting("%a", underlying_array, size_hint,
                                f32_temp.hard);
#else /* !USE_SOFTFLOAT */
      EDG_float16_t float16_temp;
      (void)memcpy((char *)&float16_temp, (char *)value.float_value,
                   sizeof(EDG_float16_t));
      append_using_c_formatting("%a", underlying_array, size_hint,
                                (double)float16_temp);
#endif /* USE_SOFTFLOAT */
    } else if (value.kind == fk_float || value.kind == fk_std_float32 ||
               value.kind == fk_std_bfloat16) {
      float  float_temp;
      (void)memcpy((char *)&float_temp, (char *)value.float_value,
                   sizeof(float));
      append_using_c_formatting("%a", underlying_array, size_hint,
                                (double)float_temp);
    } else if (kind_is_binary64(value.kind)) {
      double  double_temp;
      (void)memcpy((char *)&double_temp, (char *)value.float_value,
                   sizeof(double));
      append_using_c_formatting("%la", underlying_array, size_hint,
                                double_temp);
#if USE_FLOAT128_FOR_HOST_FP_VALUE
    } else if (repr_is_long_double(value.kind) || value.kind == fk_float80 ||
               value.kind == fk_std_float128) {
      long double ld_temp;
      (void)memcpy((char *)&ld_temp, (char *)value.float_value,
                   sizeof(long double));
      append_using_c_formatting("%La", underlying_array, size_hint,
                                ld_temp);
#endif /* USE_FLOAT128_FOR_HOST_FP_VALUE */
    } else {
      (void)memcpy((char *)&temp, (char *)value.float_value,
                   sizeof(a_host_fp_value));
#if USE_SOFTFLOAT
      append_softfloat_hex_constant_string(underlying_array, value.kind,
                                           value.float_value);
#else /* !USE_SOFTFLOAT */
#if USE_DOUBLE_FOR_HOST_FP_VALUE
      append_using_c_formatting("%la", underlying_array, size_hint, temp);
#endif /* USE_DOUBLE_FOR_HOST_FP_VALUE */
#if USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
      append_using_c_formatting("%La", underlying_array, size_hint, temp);
#endif /* USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
#if USE_FLOAT128_FOR_HOST_FP_VALUE
#if USE_QUADMATH_LIBRARY
      append_using_quadmath_formatting("%Qa", underlying_array, size_hint,
                                       temp);
#else /* !USE_QUADMATH_LIBRARY */
      append_using_c_formatting("%La", underlying_array, size_hint,
                                (long double)temp);
#endif /* USE_QUADMATH_LIBRARY */
#endif /* USE_FLOAT128_FOR_HOST_FP_VALUE */
#endif /* USE_SOFTFLOAT */
    }  /* if */
  }  /* if */
}  /* append_into */

#endif /* USE_HEX_FP_CONSTANTS_IN_GENERATED_CODE */

#if IA64_ABI

/*
A string formatter for an_il_hex_fp_value values.
*/
template<>
struct String_formatter<an_il_hex_fp_value> {
  static size_t size_hint_of(an_il_hex_fp_value value)
    { return 49; }
  template<typename a_Dyn_array>
  static inline void append_into(a_Dyn_array        &underlying_array,
                                 an_il_hex_fp_value value,
                                 size_t             size_hint);
};  /* String_formatter */


template<typename a_Dyn_array>
void String_formatter<an_il_hex_fp_value>::append_into(
                                          a_Dyn_array        &underlying_array,
                                          an_il_hex_fp_value value,
                                          size_t             size_hint)
/*
Append the characters representing in the given floating-point value into the
underlying array using its associated formatting specification.  size_hint is
an overestimate (i.e., maximum) number of characters this value might use.
*/
{
  size_t data_size;

  /* Determine the size of the data in the floating-point value. */
  if (kind_is_16bit(value.kind)) {
    data_size = 2;
  } else if (value.kind == fk_float) {
    data_size = sizeof(float);
  } else if (repr_is_double(value.kind)) {
    data_size = sizeof(double);
  } else if (value.kind == fk_std_float32) {
    data_size = 4;
  } else if (value.kind == fk_std_float64) {
    data_size = 8;
  } else {
    data_size = data_size_of_host_fp_value;
  }  /* if */
#if ABI_COMPATIBILITY_VERSION >= 402
  /* The long double format sometimes contains some unused bytes.
     Put out zeros for the padding space. */
  if (repr_is_long_double(value.kind)) {
    check_assertion(data_size <= sizeof(long double));
    size_t pad_size = sizeof(long double) - data_size;
    underlying_array.resize(underlying_array.length() + (pad_size * 2), '0');
  }  /* if */
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
  /* The IA-64 ABI requires that the output be high-order bytes first,
     and it must use lower-case characters. */
  for (size_t j = 0; j < data_size; j++) {
    unsigned char byte;
    if (host_little_endian) {
      byte = value.float_value->bytes[data_size-1-j];
    } else {
      byte = value.float_value->bytes[j];
    }  /* if */
    append_using_c_formatting("%02x", underlying_array, /*size_hint=*/2,
                              (unsigned int)byte);
  }  /* for */
}  /* append_into */

#endif /* IA64_ABI */

}  /* namespace detail */

a_number_buffer fp_to_string(a_float_kind            kind,
                             an_internal_float_value *float_value,
                             a_boolean               *pos_infinity,
                             a_boolean               *neg_infinity,
                             a_boolean               *not_a_number)
/*
Return a string representation of the float value float_value (with precision
as indicated by kind).  If the floating-point value is positive infinity or
negative infinity, set *pos_infinity or *neg_infinity to TRUE.  If the
floating-point value is a NaN, set *not_a_number to TRUE.  In the above special
cases, a display string is still returned (e.g., "NaN").  pos_infinity,
neg_infinity, and not_a_number can be NULL if the corresponding return value is
not needed.
*/
{
  a_number_buffer result(an_il_fp_value(kind, float_value, pos_infinity,
                                        neg_infinity, not_a_number));

  return result;
}  /* fp_to_string */

#if USE_HEX_FP_CONSTANTS_IN_GENERATED_CODE

a_number_buffer fp_to_hex_constant_string(
                                         a_float_kind            kind,
                                         an_internal_float_value *float_value,
                                         a_boolean               *pos_infinity,
                                         a_boolean               *neg_infinity,
                                         a_boolean               *not_a_number)
/*
Return a C99-style hexadecimal string representation of the float value
float_value (with precision as indicated by kind).  If the floating-point value
is positive infinity or negative infinity, set *pos_infinity or *neg_infinity
to TRUE.  If the floating-point value is a NaN, set *not_a_number to TRUE.  In
the above special cases, a display string is still returned (e.g., "NaN").
pos_infinity, neg_infinity, and not_a_number can be NULL if the corresponding
return value is not needed.
*/
{
  a_number_buffer result(an_il_hex_constant_fp_value(kind, float_value,
                                                     pos_infinity,
                                                     neg_infinity,
                                                     not_a_number));

  return result;
}  /* fp_to_hex_constant_string */

#endif /* USE_HEX_FP_CONSTANTS_IN_GENERATED_CODE */

#if IA64_ABI

a_number_buffer fp_to_hex_string(a_float_kind            kind,
                                 an_internal_float_value *float_value)
/*
Return a string of hex digits representation of the float value float_value
(with precision as indicated by kind).  This is used in the IA-64 ABI for the
representation of floating-point values in mangled names.
*/
{
  a_number_buffer result(an_il_hex_fp_value(kind, float_value));

  return result;
}  /* fp_to_hex_string */

#endif /* IA64_ABI */

void fp_host_large_integer_to_float(a_float_kind            kind,
		                    a_host_large_integer    int_value,
                                    an_internal_float_value *float_value,
                                    a_boolean               *err)
/*
Convert a host large integer (int_value) to a floating-point value of
kind "kind" in *float_value. Return *err TRUE if there is some error.
*/
{
  a_host_fp_value fp_value;

  *err = FALSE;
#if USE_SOFTFLOAT
  i64_to_f128M((int64_t)int_value, &fp_value);
#else /* !USE_SOFTFLOAT */
  fp_value = (a_host_fp_value)int_value;
#endif /* USE_SOFTFLOAT */
  store_host_fp_value(fp_value, kind, float_value, err);
}  /* fp_host_large_integer_to_float */


void fp_host_large_unsigned_to_float(
                      a_float_kind            kind, 
                      a_host_large_unsigned   unsigned_value,
                      an_internal_float_value *float_value,
                      a_boolean               *err)
/*
Convert unsigned_value to a floating-point value of kind "kind" in
*float_value.  Return *err TRUE if there is some error.
*/
{
  a_host_fp_value	fp_value;

  *err = FALSE;
#if USE_SOFTFLOAT
  ui64_to_f128M((uint64_t)unsigned_value, &fp_value);
#else /* !USE_SOFTFLOAT */
  fp_value = (a_host_fp_value)unsigned_value;
#if __MSC__
  /* The Microsoft compiler (as of Visual C++ 6.0) cannot convert an
     unsigned __int64 to double.  The conversion is done as a signed
     conversion instead.  If the value is larger than the largest
     signed, it is reduced to a value that can be represented as
     a signed and adjusted back after the conversion. */
  if (unsigned_value > MAX_HOST_LARGE_INTEGER) {
    a_host_large_integer	signed_value;
    unsigned_value = unsigned_value - MAX_HOST_LARGE_INTEGER;
    unsigned_value = unsigned_value - 1;
    signed_value = (a_host_large_integer)unsigned_value;
    fp_value = (a_host_fp_value)signed_value;
    fp_value = fp_value + MAX_HOST_LARGE_INTEGER;
    fp_value = fp_value + 1;
  } else {
    /* The value in known to be representable as a host large integer. */
    fp_value = (a_host_fp_value)(a_host_large_integer)unsigned_value;
  }  /* if */
#endif /* __MSC__ */
#endif /* USE_SOFTFLOAT */
  store_host_fp_value(fp_value, kind, float_value, err);
}  /* fp_host_large_unsigned_to_float */


void fp_to_host_large_integer(
			a_float_kind            kind,
			an_internal_float_value *float_value,
			a_host_large_integer    *int_value,
			a_boolean               *err,
			a_boolean               *depends_on_fp_mode)
/*
Convert float_value to a host large integer value in int_value.  Return
*err TRUE if there is some error.  If the result depends on the floating-point
mode, *depends_on_fp_mode is returned TRUE (*int_value is set anyway).
*/
{
  a_host_fp_value temp;

  *err = FALSE;
  *depends_on_fp_mode = FALSE;
  temp = fetch_host_fp_value(kind, float_value);
#if USE_SOFTFLOAT
  /* SoftFloat can only convert to 32-bit or 64-bit integer values.  Convert to
     a 64-bit integer here.  That could result in unnecessary truncation if the
     host integer is larger than that. */
  softfloat_exceptionFlags = 0;
  *int_value = (a_host_large_integer)f128M_to_i64_r_minMag(&temp,
                                                           /*exact=*/FALSE);
  if ((softfloat_exceptionFlags & softfloat_flag_invalid) != 0) {
    *err = TRUE;
  }  /* if */
#else /* !USE_SOFTFLOAT */
#if TARG_HAS_IEEE_FLOATING_POINT
  if (!is_finite(temp)) {
    /* A NaN or infinity. */
    *err = TRUE;
  } else
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
  /* Do not insert code here; this is the "else" of an "if". */
  if (temp > (a_host_fp_value)MAX_HOST_LARGE_INTEGER ||
      temp < (a_host_fp_value)MIN_HOST_LARGE_INTEGER) {
    /* Floating value is too big or too small. */
    *err = TRUE;
  }  /* if */
  /* Note that we produce a result even in the event of an error.  This
     value may be used in some modes. */
  *int_value = (a_host_large_integer)temp;
#endif /* USE_SOFTFLOAT */
}  /* fp_to_host_large_integer */


#if !STANDALONE_UTILITY_PROGRAM

void make_saturated_integer_for_float(
			a_float_kind            kind,
			an_internal_float_value *float_value,
			an_integer_value	*result,
			a_constant_ptr		result_constant)
/*
Set "result" to the largest or smallest possible value depending on whether
float_value is positive or negative.  result_constant is the constant in
which the value will ultimately be stored, which is used to determine
the appropriate largest or smallest value for the destination type.
*/
{
  an_integer_value min_value, max_value;
  a_host_fp_value  temp;

  integer_value_range_for_type(result_constant->type, &min_value, &max_value);
  temp = fetch_host_fp_value(kind, float_value);
  if (do_fp_lt_zero(temp)) {
    *result = min_value;
  } else {
    *result = max_value;
  }  /* if */
}  /* make_saturated_integer_for_float */

#endif /* !STANDALONE_UTILITY_PROGRAM */

void fp_to_host_large_unsigned(
			a_float_kind            kind,
			an_internal_float_value *float_value,
			a_host_large_unsigned   *unsigned_value,
			a_boolean               *err,
			a_boolean               *depends_on_fp_mode)
/*
Convert float_value to a host large unsigned value in unsigned_value.
Return *err TRUE if there is some error.  If the result depends on the
floating-point mode, *depends_on_fp_mode is returned TRUE
(*unsigned_value is set anyway).
*/
{
  a_host_fp_value temp;

  *err = FALSE;
  *depends_on_fp_mode = FALSE;
  temp = fetch_host_fp_value(kind, float_value);
#if USE_SOFTFLOAT
  /* SoftFloat can only convert to 32-bit or 64-bit integer values.  Convert to
     a 64-bit integer here.  That could result in unnecessary truncation if the
     host integer is larger than that. */
  softfloat_exceptionFlags = 0;
  *unsigned_value = (a_host_large_unsigned)f128M_to_ui64_r_minMag(&temp,
                                                              /*exact=*/FALSE);
  if ((softfloat_exceptionFlags & softfloat_flag_invalid) != 0) {
    *err = TRUE;
  }  /* if */
#else /* !USE_SOFTFLOAT */
  if (temp > (a_host_fp_value)MAX_HOST_LARGE_UNSIGNED ||
      temp < (a_host_fp_value)0) {
    /* Floating value is too big or too small. */
    *err = TRUE;
  }  /* if */
  /* Note that we produce a result even in the event of an error.  This
     value may be used in some modes. */
  *unsigned_value = (a_host_large_unsigned)temp;
#endif /* USE_SOFTFLOAT */
}  /* fp_to_host_large_unsigned */


static a_boolean fp_value_is_zero(a_host_fp_value val)
/*
Returns TRUE if the specified floating-point value is zero.
*/
{
  return do_fp_eq_zero(val);
}  /* fp_value_is_zero */


a_boolean fp_is_zero_constant(a_float_kind            kind,
                              an_internal_float_value *float_value)
/*
Return TRUE if the constant (a float constant) is a floating zero of
any precision.
*/
{
  return fp_value_is_zero(fetch_host_fp_value(kind, float_value));
}  /* fp_is_zero_constant */


void fp_add(a_float_kind            kind,
            an_internal_float_value *value_1,
            an_internal_float_value *value_2,
            an_internal_float_value *result,
            a_boolean               *err,
            a_boolean               *depends_on_fp_mode)
/*
Add the floating-point values value_1 and value_2 and put the result in
result.  The result has kind "kind".  If there is any error, set *err
to TRUE.  If the result depends on the floating-point mode,
*depends_on_fp_mode is returned TRUE (*result is set anyway).
*/
{
  a_host_fp_value	tempr, temp1, temp2;

  *err = FALSE;
  *depends_on_fp_mode = FALSE;
  temp1 = fetch_host_fp_value(kind, value_1);
  temp2 = fetch_host_fp_value(kind, value_2);
  do_fp_add(temp1, temp2, tempr);
  store_host_fp_value(tempr, kind, result, err);
#if TARG_HAS_IEEE_FLOATING_POINT
  if (!is_finite(temp1) || !is_finite(temp2)) *depends_on_fp_mode = TRUE;
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
}  /* fp_add */


void fp_subtract(a_float_kind            kind,
                 an_internal_float_value *value_1,
                 an_internal_float_value *value_2,
                 an_internal_float_value *result,
                 a_boolean               *err,
                 a_boolean               *depends_on_fp_mode)
/*
Subtract the floating-point values value_1 and value_2 and put the result in
result.  The result has kind "kind".  If there is any error, set *err
to TRUE.  If the result depends on the floating-point mode,
*depends_on_fp_mode is returned TRUE (*result is set anyway).
*/
{
  a_host_fp_value tempr, temp1, temp2;

  *err = FALSE;
  *depends_on_fp_mode = FALSE;
  temp1 = fetch_host_fp_value(kind, value_1);
  temp2 = fetch_host_fp_value(kind, value_2);
  do_fp_subtract(temp1, temp2, tempr);
  store_host_fp_value(tempr, kind, result, err);
#if TARG_HAS_IEEE_FLOATING_POINT
  if (!is_finite(temp1) || !is_finite(temp2)) *depends_on_fp_mode = TRUE;
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
}  /* fp_subtract */


void fp_negate(a_float_kind            kind,
               an_internal_float_value *value_1,
               an_internal_float_value *result,
               a_boolean               *err,
               a_boolean               *depends_on_fp_mode)
/*
Negate the floating-point value value_1 and put the result in result.
The result has kind "kind".  If there is any error, set *err to TRUE.
If the result depends on the floating-point mode, *depends_on_fp_mode
is returned TRUE (*result is set anyway).  There is a separate routine
for this (rather than using fp_subtract and a zero constant) because
of IEEE floating-point requirements.
*/
{
  a_host_fp_value tempr, temp1;

  *err = FALSE;
  *depends_on_fp_mode = FALSE;
  temp1 = fetch_host_fp_value(kind, value_1);
  do_fp_negate(temp1, tempr);
  store_host_fp_value(tempr, kind, result, err);
#if TARG_HAS_IEEE_FLOATING_POINT
  if (!is_finite(temp1)) *depends_on_fp_mode = TRUE;
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
}  /* fp_negate */


void fp_multiply(a_float_kind            kind,
                 an_internal_float_value *value_1,
                 an_internal_float_value *value_2,
                 an_internal_float_value *result,
                 a_boolean               *err,
                 a_boolean               *depends_on_fp_mode)
/*
Multiply the floating-point values value_1 and value_2 and put the result in
result.  The result has kind "kind".  If there is any error, set *err
to TRUE.  If the result depends on the floating-point mode,
*depends_on_fp_mode is returned TRUE (*result is set anyway).
*/
{
  a_host_fp_value tempr, temp1, temp2;

  *err = FALSE;
  *depends_on_fp_mode = FALSE;
  temp1 = fetch_host_fp_value(kind, value_1);
  temp2 = fetch_host_fp_value(kind, value_2);
  do_fp_multiply(temp1, temp2, tempr);
  store_host_fp_value(tempr, kind, result, err);
#if TARG_HAS_IEEE_FLOATING_POINT
  if (!is_finite(temp1) || !is_finite(temp2)) *depends_on_fp_mode = TRUE;
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
}  /* fp_multiply */


void fp_divide(a_float_kind            kind,
               an_internal_float_value *value_1,
               an_internal_float_value *value_2,
               an_internal_float_value *result,
               a_boolean               *err,
               a_boolean               *depends_on_fp_mode)
/*
Divide the floating-point values value_1 and value_2 and put the result in
result.  The result has kind "kind".  If there is any error, set *err
to TRUE.  If the result depends on the floating-point mode,
*depends_on_fp_mode is returned TRUE (*result is set anyway).
*/
{
  a_host_fp_value tempr, temp1, temp2;

  *err = FALSE;
  *depends_on_fp_mode = FALSE;
  temp1 = fetch_host_fp_value(kind, value_1);
  temp2 = fetch_host_fp_value(kind, value_2);
#if !TARG_HAS_IEEE_FLOATING_POINT
  if (fp_value_is_zero(temp2)) {
    /* Division by zero.  This is also checked by the caller for a specific
       error message. */
    *err = TRUE;
  } else
#endif /* !TARG_HAS_IEEE_FLOATING_POINT */
  /* Do not insert code here; this is the "else" of an "if". */
  {
    /* The following divide can produce NaN/infinities, but should not
       produce any host errors. */
    do_fp_divide(temp1, temp2, tempr);
    store_host_fp_value(tempr, kind, result, err);
#if TARG_HAS_IEEE_FLOATING_POINT
    if (!is_finite(temp1) || !is_finite(temp2) ||
        fp_value_is_zero(temp2)) *depends_on_fp_mode = TRUE;
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
  }  /* if */
}  /* fp_divide */


int fp_compare(a_float_kind            kind,
               an_internal_float_value *value_1,
               an_internal_float_value *value_2,
               a_boolean               *unord)
/*
Compare two floating-point values.  Return *unord set to TRUE if they
are unordered with respect to each other.  Otherwise, return strcmp-like
values:
       value_1 > value_2   1
       value_1 = value_2   0
       value_1 < value_2  -1
*/
{
  int    cmp;
  a_host_fp_value temp1, temp2;
  a_boolean       test;

  temp1 = fetch_host_fp_value(kind, value_1);
  temp2 = fetch_host_fp_value(kind, value_2);
  *unord = FALSE;
#if TARG_HAS_IEEE_FLOATING_POINT
  if (is_NaN(temp1) || is_NaN(temp2)) {
    *unord = TRUE;
    cmp = 0;
  } else
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
  /* Do not insert code here. */
  {
#if USE_SOFTFLOAT
    test = !f128M_le(&temp1, &temp2);
#else /* !USE_SOFTFLOAT */
    test = (temp1 > temp2);
#endif /* USE_SOFTFLOAT */
    if (test) {
      /* temp1 > temp2 */
      cmp = 1;
    } else {
#if USE_SOFTFLOAT
      test = f128M_lt(&temp1, &temp2);
#else /* !USE_SOFTFLOAT */
      test = (temp1 < temp2);
#endif /* USE_SOFTFLOAT */
      if (test) {
        /* temp1 < temp2 */
        cmp = -1;
      } else {
        /* temp1 == temp2 */
        cmp = 0;
      }  /* if */
    }  /* if */
  }  /* if */
  return cmp;
}  /* fp_compare */


void fp_ceil(a_float_kind            kind,
             an_internal_float_value *value,
             an_internal_float_value *result,
             a_boolean               *err)
/*
Compute the "ceiling" (the least integer greater than or equal to *value)
and return a floating-point representation of that integer in *result.
Both *value and *result have "kind" floating type.  *err is set to TRUE if an
error is detected (and is set to FALSE otherwise).
*/
{
  a_boolean    depends_on_fp_mode = FALSE;

  *err = FALSE;
  if (fp_is_zero_constant(kind, value)
#if TARG_HAS_IEEE_FLOATING_POINT
      || fp_is_nan(value, kind)
      || fp_is_infinity(value, kind)
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
                                    ) {
    /* For zeros, NaNs, and infinities, the result is the same as the input. */
    *result = *value;
  } else {
    /* Truncate to an integer value. */
    a_host_large_integer    int_value;
    an_internal_float_value truncated_value;
    a_boolean               unordered;
    fp_to_host_large_integer(kind, value, &int_value, err,
                             &depends_on_fp_mode);
    if (depends_on_fp_mode) *err = TRUE;
    /* Convert back to a floating-point value. */
    fp_host_large_integer_to_float(kind, int_value, &truncated_value, err);
    if (fp_compare(kind, value, &truncated_value, &unordered) == 0) {
      /* The round-trip comparison was equal (so there must not be a decimal
         fraction portion) so the result is the same as the input. */
      *result = *value;
      if (unordered) *err = TRUE;
    } else {
      a_boolean sign_bit = fp_signbit(kind, value);
      if (!sign_bit) {
        /* Add one to the integer value (if it won't overflow). */
        if (int_value == MAX_HOST_LARGE_INTEGER) {
          *err = TRUE;
        } else {
          int_value++;
        }  /* if */
      }  /* if */
      /* Convert the integer value back to a float. */
      fp_host_large_integer_to_float(kind, int_value, result, err);
      if (sign_bit && int_value == 0) {
        /* Make sure the sign bit is correct for negative zeros. */
        fp_negate(kind, result, result, err, err);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* fp_ceil */


#if !STANDALONE_UTILITY_PROGRAM
#if C99_IL_EXTENSIONS_SUPPORTED

void cx_add(a_float_kind              kind,
            an_internal_complex_value *value_1,
            an_internal_complex_value *value_2,
            an_internal_complex_value *result,
            a_boolean                 *err,
            a_boolean                 *depends_on_fp_mode)
/*
Add the complex floating-point values value_1 and value_2 and put the result
in result.  The result has kind "kind".  If there is any error, set *err to
TRUE.  If the result depends on the floating-point mode, *depends_on_fp_mode
is returned TRUE (*result is set anyway).
*/
{
  a_boolean  r_err, i_err, r_mode_dep, i_mode_dep;

  fp_add(kind, &value_1->real, &value_2->real, &result->real,
         &r_err, &r_mode_dep);
  fp_add(kind, &value_1->imag, &value_2->imag, &result->imag,
         &i_err, &i_mode_dep);
  *err = r_err || i_err;
  *depends_on_fp_mode = r_mode_dep || i_mode_dep;
}  /* cx_add */


void cx_subtract(a_float_kind              kind,
                 an_internal_complex_value *value_1,
                 an_internal_complex_value *value_2,
                 an_internal_complex_value *result,
                 a_boolean                 *err,
                 a_boolean                 *depends_on_fp_mode)
/*
Subtract the complex floating-point values value_1 and value_2 and put the
result in result.  The result has kind "kind".  If there is any error, set
*err to TRUE.  If the result depends on the floating-point mode,
*depends_on_fp_mode is returned TRUE (*result is set anyway).
*/
{
  a_boolean  r_err, i_err, r_mode_dep, i_mode_dep;

  fp_subtract(kind, &value_1->real, &value_2->real, &result->real,
              &r_err, &r_mode_dep);
  fp_subtract(kind, &value_1->imag, &value_2->imag, &result->imag,
              &i_err, &i_mode_dep);
  *err = r_err || i_err;
  *depends_on_fp_mode = r_mode_dep || i_mode_dep;
}  /* cx_subtract */


void cx_negate(a_float_kind              kind,
               an_internal_complex_value *value_1,
               an_internal_complex_value *result,
               a_boolean                 *err,
               a_boolean                 *depends_on_fp_mode)
/*
Negate the complex floating-point value value_1 and put the result in result.
The result has kind "kind".  If there is any error, set *err to TRUE.  If the
result depends on the floating-point mode, *depends_on_fp_mode is returned
TRUE (*result is set anyway).
*/
{
  a_boolean  r_err, i_err, r_mode_dep, i_mode_dep;

  fp_negate(kind, &value_1->real, &result->real, &r_err, &r_mode_dep);
  fp_negate(kind, &value_1->imag, &result->imag, &i_err, &i_mode_dep);
  *err = r_err || i_err;
  *depends_on_fp_mode = r_mode_dep || i_mode_dep;
}  /* cx_negate */


void cx_multiply(a_float_kind              kind,
                 an_internal_complex_value *value_1,
                 an_internal_complex_value *value_2,
                 an_internal_complex_value *result,
                 a_boolean                 *err,
                 a_boolean                 *depends_on_fp_mode)
/*
Multiply the complex floating-point values value_1 and value_2 and put the
result in result.  The result has kind "kind".  If there is any error, set
*err to TRUE.  If the result depends on the floating-point mode,
*depends_on_fp_mode is returned TRUE (*result is set anyway).
*/
{
  a_boolean                  op_err, accum_err, depends_on_mode;
  an_internal_float_value    temp_value;
  an_internal_complex_value  result_value;

#if 0
  /* This is an oversimplified algorithm that can exhibit dynamic range
     problems (e.g., catastrophic cancellation). */
#endif /* 0 */
  /* (a1 + b1*i) * (a2 + b2*i) = (a1a2 - b1b2) + (b1a2 + a1b2)i */
  /* Compute the real part of the result. */
  fp_multiply(kind, &value_1->real, &value_2->real, &result_value.real,
              &op_err, &depends_on_mode);
  accum_err = op_err;
  *depends_on_fp_mode = depends_on_mode;
  fp_multiply(kind, &value_1->imag, &value_2->imag, &temp_value,
              &op_err, &depends_on_mode);
  accum_err |= op_err;
  *depends_on_fp_mode |= depends_on_mode;
  fp_subtract(kind, &result_value.real, &temp_value, &result_value.real,
              &op_err, &depends_on_mode);
  accum_err |= op_err;
  *depends_on_fp_mode |= depends_on_mode;
  /* Compute the imaginary part of the result. */
  fp_multiply(kind, &value_1->real, &value_2->imag, &result_value.imag,
              &op_err, &depends_on_mode);
  accum_err |= op_err;
  *depends_on_fp_mode |= depends_on_mode;
  fp_multiply(kind, &value_1->imag, &value_2->real, &temp_value,
              &op_err, &depends_on_mode);
  accum_err |= op_err;
  *depends_on_fp_mode |= depends_on_mode;
  fp_add(kind, &result_value.imag, &temp_value, &result_value.imag,
         &op_err, &depends_on_mode);
  *result = result_value;
  accum_err |= op_err;
  *err = accum_err;
  *depends_on_fp_mode |= depends_on_mode;
}  /* cx_multiply */


void cx_divide(a_float_kind              kind,
               an_internal_complex_value *value_1,
               an_internal_complex_value *value_2,
               an_internal_complex_value *result,
               a_boolean                 *err,
               a_boolean                 *depends_on_fp_mode)
/*
Divide the complex floating-point values value_1 and value_2 and put the
result in result.  The result has kind "kind".  If there is any error, set
*err to TRUE.  If the result depends on the floating-point mode,
*depends_on_fp_mode is returned TRUE (*result is set anyway).
*/
{
  a_boolean                op_err, accum_err, depends_on_mode;
  an_internal_float_value  temp_value, quad_norm;

#if 0
  /* This is an oversimplified algorithm that can exhibit dynamic range
     problems (e.g., catastrophic cancellation). */
#endif /* 0 */
  /* Compute the real value quad_norm = real_2*real_2 + imag_2*imag_2. */
  fp_multiply(kind, &value_2->real, &value_2->real,
              &quad_norm, &op_err, &depends_on_mode);
  accum_err = op_err;
  *depends_on_fp_mode = depends_on_mode;
  fp_multiply(kind, &value_2->imag, &value_2->imag,
              &temp_value, &op_err, &depends_on_mode);
  accum_err |= op_err;
  *depends_on_fp_mode |= depends_on_mode;
  fp_add(kind, &quad_norm, &temp_value, &quad_norm, &op_err, &depends_on_mode);
  accum_err |= op_err;
  *depends_on_fp_mode |= depends_on_mode;
  if (!IEEE_handling_on_float_operation_exceptions &&
      fp_is_zero_constant(kind, &quad_norm)) {
    *err = TRUE;
  } else {
    an_internal_complex_value  result_value;
    /* Compute the real part of the result. */
    fp_multiply(kind, &value_1->real, &value_2->real,
                &result_value.real, &op_err, &depends_on_mode);
    accum_err |= op_err;
    *depends_on_fp_mode |= depends_on_mode;
    fp_multiply(kind, &value_1->imag, &value_2->imag,
                &temp_value, &op_err, &depends_on_mode);
    accum_err |= op_err;
    *depends_on_fp_mode |= depends_on_mode;
    fp_add(kind, &result_value.real, &temp_value,
           &result_value.real, &op_err, &depends_on_mode);
    accum_err |= op_err;
    *depends_on_fp_mode |= depends_on_mode;
    fp_divide(kind, &result_value.real, &quad_norm,
              &result_value.real, &op_err, &depends_on_mode);
    accum_err |= op_err;
    *depends_on_fp_mode |= depends_on_mode;
    /* Compute the imaginary part of the result. */
    fp_multiply(kind, &value_1->real, &value_2->imag,
                &result_value.imag, &op_err, &depends_on_mode);
    accum_err |= op_err;
    *depends_on_fp_mode |= depends_on_mode;
    fp_multiply(kind, &value_1->imag, &value_2->real,
                &temp_value, &op_err, &depends_on_mode);
    accum_err |= op_err;
    *depends_on_fp_mode |= depends_on_mode;
    fp_subtract(kind, &temp_value, &result_value.imag,
                &result_value.imag, &op_err, &depends_on_mode);
    accum_err |= op_err;
    *depends_on_fp_mode |= depends_on_mode;
    fp_divide(kind, &result_value.imag, &quad_norm,
              &result_value.imag, &op_err, &depends_on_mode);
    *result = result_value;
    accum_err |= op_err;
    *err = accum_err;
    *depends_on_fp_mode |= depends_on_mode;
  }  /* if */
}  /* cx_divide */


a_boolean cx_equal(a_float_kind              kind,
                   an_internal_complex_value *value_1,
                   an_internal_complex_value *value_2)
/*
Return TRUE if the given complex floating-point values (of the given kind) are
equal.  Return FALSE otherwise.
*/
{
  int          real_cmp, imag_cmp;
  a_boolean    result_value, real_unordered, imag_unordered;

  real_cmp = fp_compare(kind, &value_1->real, &value_2->real, &real_unordered);
  imag_cmp = fp_compare(kind, &value_1->imag, &value_2->imag, &imag_unordered);
  /* If two values are unordered, they are unequal.  This is needed for
     NaN != NaN. */
  result_value = (real_cmp == 0) && (imag_cmp == 0) &&
                 !real_unordered && !imag_unordered;
  return result_value;
}  /* cx_equal */

#endif /* C99_IL_EXTENSIONS_SUPPORTED */
#endif /* !STANDALONE_UTILITY_PROGRAM */

a_boolean fp_signbit(a_float_kind            kind,
                     an_internal_float_value *value)
/*
Returns TRUE if the sign bit of the floating-point value represented by
"value" and "kind" is set.
*/
{
  an_fp_value_part	*fp_ptr;
  an_fp_value_part	val;
  an_fp_value_part	fp_temp[4];
  a_boolean		is_negative = FALSE;

  if (long_double_is_double && repr_is_long_double(kind)) {
    /* When long double is mapped onto double, use double. */
    kind = (a_float_kind)fk_double;
  }  /* if */
  fp_ptr = &fp_temp[0];
  if (kind_is_16bit(kind) ||
      kind == fk_float || kind == fk_std_float32) {
    memcpy((char*)&val, (char*)value, sizeof(val));
    is_negative = (val & 0x80000000) != 0;
  } else if (kind_is_binary64(kind)) {
    /* A double value or a long double that is being represented by a
       double value. */
    /* The code below extracts the value from fp_temp.  Copy the source to
       fp_temp. */
    memcpy((char*)fp_temp, (char*)value, sizeof(val) * 2);
    /* On little endian systems, the most significant part of the
       number is fetched in the second four bytes.  Note that when the
       long value is stored in memory, its byte order will be right for
       either kind of system. */
    /* Update the pointer to refer to the last 32-bit word of the value. */
    if (host_little_endian) fp_ptr += 1;
    val = *fp_ptr;
    is_negative = (val & 0x80000000) != 0;
  } else if (((repr_is_long_double(kind) && targ_ldbl_mant_dig == 64) ||
              (kind == (a_float_kind)fk_float80 &&
               targ_flt80_mant_dig == 64)) &&
             /*lint --e(506)*/sizeof(a_host_fp_value) >= sizeof(val)*3) {
    /* 80-bit representation in a 96-bit container. */
    /* The code below constructs the value from fp_temp.  Copy the source to
       fp_temp. */
    memcpy((char*)fp_temp, (char*)value, sizeof(val) * 3);
    /* Update the pointer to refer to the last 32-bit word of the value. */
    if (host_little_endian) fp_ptr += 2;
    val = *fp_ptr;
    is_negative = (val & 0x8000) != 0;
  } else if (((repr_is_long_double(kind) && targ_ldbl_mant_dig == 113) ||
              (kind == fk_float128 &&
               targ_flt128_mant_dig == 113) ||
              kind == fk_std_float128) &&
             /*lint --e(506)*/sizeof(a_host_fp_value) == sizeof(val)*4) {
    /* 128-bit representation. */
    /* The code below constructs the value from fp_temp.  Copy the source to
       fp_temp. */
    memcpy((char*)fp_temp, (char*)value, sizeof(val) * 4);
    /* Update the pointer to refer to the last 32-bit word of the value. */
    if (host_little_endian) fp_ptr += 3;
    val = *fp_ptr;
    is_negative = (val & 0x80000000) != 0;
  } else {
    unexpected_condition();
  }  /* if */
  return is_negative;
}  /* fp_signbit */


a_boolean fp_is_negative(a_float_kind            kind,
                         an_internal_float_value *value)
/*
Return TRUE if "value" is negative.  If "value" is positive or a NaN,
return FALSE.  Returns FALSE for -0.0 (use fp_signbit to test for this case).
*/
{
  a_host_fp_value	temp;
  a_boolean		result = FALSE;

  temp = fetch_host_fp_value(kind, value);
#if TARG_HAS_IEEE_FLOATING_POINT
  if (is_NaN(temp)) {
  } else
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
  /* Do not insert code here. */
  {
    result = do_fp_lt_zero(temp);
  }  /* if */
  return result;
}  /* fp_is_negative */


a_boolean fp_same_representation(ARG_UNUSED a_float_kind            kind,
                                 an_internal_float_value            *value_1,
                                 an_internal_float_value            *value_2)
/*
Compare two floating-point values.  Return TRUE if they have the same
representation.  This differs from fp_compare, other than the
FALSE/TRUE versus -1/0/+1 return, for example, by the fact that -0.0
and 0.0 might not compare equal.
*/
{
  /* Note that the whole float was zeroed in initialization, so any gaps
     have predictable values. */
  a_boolean same = (memcmp((char *)value_1, (char *)value_2,
                           size_t_arg(data_size_of_host_fp_value)) == 0);
  return same;
}  /* fp_same_representation */

#if !USE_SOFTFLOAT && !HOST_HAS_FLOAT16_TYPE

void adjust_float16_representation_if_needed(a_byte *rep)
/*
Adjust the internal 32-bit representation, rep, of a std::float16_t value,
assumed to be in IEEE binary32 format, to be a 16-bit IEEE binary16
representation.
*/
{
  union {
    a_byte   bytes[4];
    uint32_t float_ovl;
    uint16_t short_ovl;
  } u;
  uint16_t exp;

  /* IEEE binary32 representation:

        s eee  eeee  e mmm  mmmm  mmmm  mmmm  mmmm  mmmm
        | -----+------ ----------------+----------------
        |      |                       |
        |      |                       +-- mantissa
        |      +-------------------------- exponent
        +--------------------------------- sign

      IEEE binary16 representation:

        s eee  ee mm  mmmm  mmmm
        | ---+--- -------+------
        |    |           |
        |    |           +-- mantissa
        |    +-------------- exponent
        +------------------- sign

      Converting between the representations involves copying the sign bit;
      de-biasing (by 127) the binary32 exponent, extracting the low-order
      five bits, and re-biasing (by 15) the value for the binary16 exponent;
      and copying the high-order ten bits of the binary32 mantissa for the
      binary16 mantissa. */

  /* Copy the binary32 representation. */
  memcpy(u.bytes, rep, 4);
  /* Calculate the binary16 biased exponent. */
  exp = ((((u.float_ovl >> 23) & 0xff) - 127 + 15) & 0x1f);
  /* Assemble the binary16 representation. */
  u.short_ovl = (uint16_t)(((u.float_ovl >> 16) & 0x8000) | (exp << 10) |
                           ((u.float_ovl >> 13) & 0x3ff));
  /* Copy the binary16 representation. */
  memcpy(rep, u.bytes, 2);
  rep[2] = 0;
  rep[3] = 0;
}  /* adjust_float16_representation_if_needed */

#endif /* !USE_SOFTFLOAT && !HOST_HAS_FLOAT16_TYPE */

unsigned int fp_hash(an_internal_float_value *value)
/*
Return a hash value derived from the floating-point value "value".  This
is used in building the hash table for shareable constants.
*/
{
  unsigned int hash = 0;
  char         *cptr;
  sizeof_t     n;

  /* It's hard to do something machine-independent for floats.  Add 
     together the bytes that make up the float.  Note that the whole float
     was zeroed in initialization, so any gaps have predictable values. */
  cptr = (char *)value;
  for (n = sizeof(an_internal_float_value); n > 0; n--) {
    hash += (unsigned char)*cptr++;
  }  /* for */
  return hash;
}  /* fp_hash */


void float_pt_init(void)
/*
Initialize static variables related to float_pt.c.
*/
{
  /* Compute the number of bytes of the host floating point value that are
     actually used to represent the value.  This is often the same size as
     the host floating point value, but on some systems may be smaller if
     the host floating point value type is long double.  For example, the
     Intel long double (aka. __float80) uses only 10 bytes (80 bits) of the
     12 bytes of allocated space. */
#if USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
  data_size_of_host_fp_value = /*lint --e(506)*/ (LDBL_MANT_DIG == 64
                              ? ((LDBL_MANT_DIG + 16) / CHAR_BIT)
                              : sizeof(a_host_fp_value));
  /* The routines that handle hex floating point constants must know the
     bit layout of the floating point values.  Make sure the configuration
     is for one of the supported layouts. */
  check_assertion_str2((targ_ldbl_mant_dig == 64 &&
                        (targ_sizeof_long_double == 12 ||
                         targ_sizeof_long_double == 16)) ||
                       (targ_ldbl_mant_dig == 113 &&
                        targ_sizeof_long_double == 16) ||
                       (targ_ldbl_mant_dig == 53 &&
                        targ_sizeof_long_double == 8),
                       "float_pt_init:",
                       "unsupported long double mantissa size");
#else /* !USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
  data_size_of_host_fp_value = sizeof(a_host_fp_value);
#if USE_FLOAT128_FOR_HOST_FP_VALUE && FP_HAS_LONG_DOUBLE && \
    !FP_LONG_DOUBLE_IS_80BIT_EXTENDED &&                    \
    !FP_LONG_DOUBLE_IS_BINARY64
  /* Presumably this configuration means that long double and
     a_host_fp_value are both binary128.  Check to make sure we're not on a
     platform that uses an 80-bit representation for long double. */
  long double     ld = 1234.5678L;
  a_host_fp_value f128 = ld;
  check_assertion_str(memcmp(&ld, &f128, sizeof(f128)) == 0,
                      "FP_LONG_DOUBLE_IS_80BIT_EXTENDED should be TRUE");
#endif /* USE_FLOAT128_FOR_HOST_FP_VALUE && ... */
#endif /* USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
#if USE_SOFTFLOAT
  /* Initialize SoftFloat global variables to default values.  See the
     SoftFloat documentation for available settings. */
  softfloat_roundingMode = softfloat_round_near_even;
  softfloat_detectTininess = softfloat_tininess_afterRounding;
  extF80_roundingPrecision = 80;
  softfloat_exceptionFlags = 0;
  f16_zero = ui32_to_f16((uint32_t)0);
  f32_zero = ui32_to_f32((uint32_t)0);
  f64_zero = ui32_to_f64((uint32_t)0);
  ui32_to_f128M((uint32_t)0, &fp_zero);
#else /* !USE_SOFTFLOAT */
  fp_zero = 0.0;
#endif /* USE_SOFTFLOAT */
  /* Make sure that an_fp_value_part is 32 bits. */
  check_assertion_str(sizeof(an_fp_value_part) == 4,
         "float_pt_init: bad size for an_fp_value_part");  /*lint !e774*/
#if FP_HAS_LONG_DOUBLE
  long_double_is_double = (targ_ldbl_mant_dig == 53);
#else /* !FP_HAS_LONG_DOUBLE */
  long_double_is_double = TRUE;
#endif /* FP_HAS_LONG_DOUBLE */
}  /* float_pt_init */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

