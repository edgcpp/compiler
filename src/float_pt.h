/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

float_pt.h -- Declarations for float_pt.c (having to do with manipulation of
              internal floating-point quantities).

*/

/* Avoid including these declarations more than once: */
#ifndef FLOAT_PT_H
#define FLOAT_PT_H 1

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
The number of host longs requires to represent the largest possible
mantissa.
*/
#define MANTISSA_PARTS 4

/*
Structure used to represent an internal value of a mantissa.  Used
to convert hexadecimal floating point values to internal form.
*/
typedef struct a_mantissa *a_mantissa_ptr;
typedef struct a_mantissa {
  an_fp_value_part
		parts[MANTISSA_PARTS];
			/* The bits that make up the mantissa. */
  a_boolean	underflow;
			/* TRUE if bits have been shifted out of the
			   mantissa. */
} a_mantissa;

extern void init_mantissa(a_mantissa_ptr	mp);

extern void shift_left_mantissa(a_mantissa_ptr	mp,
				int		bits);

extern void shift_right_mantissa(a_mantissa_ptr	mp,
				 int		bits);

extern unsigned  number_of_bits_in_mantissa(a_mantissa_ptr mp,
                                            a_boolean      normalize);

extern void round_hex_fp_value(a_mantissa_ptr mp,
                               long           *exponent,
                               a_targ_size_t  value_bits,
                               a_boolean      is_fixed_point,
                               a_boolean      is_signed,
                               a_boolean      *inexact);

#if DEBUG
/* GCOVR_EXCL_START */
extern void db_mantissa(a_mantissa_ptr	mp);

extern void db_internal_float_value(an_internal_float_value *ifv);

extern void db_long_double(long double d);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

extern void conv_hex_string_to_mantissa_and_exponent(
				a_const_char		*str,
				a_mantissa_ptr		mantissa,
				long			*p_exponent,
				a_boolean		*exponent_overflow);

extern void conv_mantissa_to_floating_point(
				a_mantissa_ptr			mp,
				long				*p_exponent,
				a_boolean			is_negative,
				a_float_kind			kind,
				an_internal_float_value		*float_value,
				a_boolean			overflow,
				a_boolean			*err,
				a_boolean			*inexact);

#if FIXED_POINT_ALLOWED

extern a_boolean mantissa_is_zero(a_mantissa_ptr	mp);

#endif /* FIXED_POINT_ALLOWED */

extern void load_hex_fp_value(an_internal_float_value	*float_value,
			      a_float_kind		kind,
			      a_mantissa_ptr		mp,
			      long			*exponent,
			      a_boolean			*is_negative,
			      a_boolean			restore_implicit_bit);


extern a_host_fp_value fetch_host_fp_value(
				a_float_kind            kind,
				an_internal_float_value *float_value);

#if TARG_HAS_IEEE_FLOATING_POINT
extern a_boolean make_fp_nan(an_internal_float_value *value,
                             a_float_kind            kind,
                             a_boolean	             signaling,
                             an_fp_value_part        mantissa);

extern a_boolean make_fp_infinity(an_internal_float_value *value,
                                  a_float_kind            kind);

extern a_boolean fp_is_nan(an_internal_float_value  *value,
                           a_float_kind  kind);

extern a_boolean fp_is_infinity(an_internal_float_value  *value,
                                a_float_kind             kind);

#if BUILTIN_FUNCTIONS_ENABLED
extern a_boolean fp_is_normalized(an_internal_float_value  *value,
                                  a_float_kind             kind,
                                  a_boolean                *unknown);
#endif /* BUILTIN_FUNCTIONS_ENABLED */

#if FIXED_POINT_ALLOWED
extern a_boolean fp_is_nan_or_infinity(an_internal_float_value	*value,
				       a_float_kind		kind);
#endif /* FIXED_POINT_ALLOWED */
#endif /* TARG_HAS_IEEE_FLOATING_POINT */

extern void fp_change_kind(an_internal_float_value *old_value,
                           a_float_kind            old_kind,
                           an_internal_float_value *new_value,
                           a_float_kind            new_kind,
                           a_boolean               *err,
                           a_boolean               *depends_on_fp_mode);

extern a_boolean make_huge_fp_val(an_internal_float_value  *value,
                                  a_float_kind             kind);

extern
void fp_hex_string_to_float(a_float_kind		kind,
	                    a_const_char		*str,
	                    an_internal_float_value	*float_value,
	                    a_boolean			*err,
			    a_boolean			*inexact);

extern void fp_string_to_float(a_float_kind            kind,
                               a_const_char            *str,
                               an_internal_float_value *float_value,
                               a_boolean               *err);

extern a_number_buffer fp_to_string(a_float_kind            kind,
                                    an_internal_float_value *float_value,
                                    a_boolean               *pos_infinity,
                                    a_boolean               *neg_infinity,
                                    a_boolean               *not_a_number);

extern a_number_buffer fp_to_hex_constant_string(
                                        a_float_kind            kind,
                                        an_internal_float_value *float_value,
                                        a_boolean               *pos_infinity,
                                        a_boolean               *neg_infinity,
                                        a_boolean               *not_a_number);

#if IA64_ABI
extern a_number_buffer fp_to_hex_string(a_float_kind            kind,
                                        an_internal_float_value *float_value);
#endif /* IA64_ABI */

extern
void fp_host_large_integer_to_float(a_float_kind            kind,
		                    a_host_large_integer    int_value,
                                    an_internal_float_value *float_value,
                                    a_boolean               *err);

extern void make_saturated_integer_for_float(
			a_float_kind            kind,
			an_internal_float_value *float_value,
			an_integer_value	*result,
			a_constant_ptr		result_constant);

extern void fp_host_large_unsigned_to_float(
                      a_float_kind            kind, 
                      a_host_large_unsigned   unsigned_value,
                      an_internal_float_value *float_value,
                      a_boolean               *err);

extern void fp_to_host_large_integer(
			a_float_kind            kind,
			an_internal_float_value *float_value,
			a_host_large_integer    *int_value,
			a_boolean               *err,
			a_boolean               *depends_on_fp_mode);

extern void fp_to_host_large_unsigned(
			a_float_kind            kind,
			an_internal_float_value *float_value,
			a_host_large_unsigned   *unsigned_value,
			a_boolean               *err,
			a_boolean               *depends_on_fp_mode);

extern a_boolean fp_is_zero_constant(a_float_kind            kind,
                                     an_internal_float_value *float_value);

extern void fp_add(a_float_kind            kind,
                   an_internal_float_value *value_1,
                   an_internal_float_value *value_2,
                   an_internal_float_value *result,
                   a_boolean               *err,
                   a_boolean               *depends_on_fp_mode);

extern void fp_subtract(a_float_kind            kind,
                        an_internal_float_value *value_1,
                        an_internal_float_value *value_2,
                        an_internal_float_value *result,
                        a_boolean               *err,
                        a_boolean               *depends_on_fp_mode);

extern void fp_negate(a_float_kind            kind,
                      an_internal_float_value *value_1,
                      an_internal_float_value *result,
                      a_boolean               *err,
                      a_boolean               *depends_on_fp_mode);

extern void fp_multiply(a_float_kind            kind,
                        an_internal_float_value *value_1,
                        an_internal_float_value *value_2,
                        an_internal_float_value *result,
                        a_boolean               *err,
                        a_boolean               *depends_on_fp_mode);

extern void fp_divide(a_float_kind            kind,
                      an_internal_float_value *value_1,
                      an_internal_float_value *value_2,
                      an_internal_float_value *result,
                      a_boolean               *err,
                      a_boolean               *depends_on_fp_mode);

extern int fp_compare(a_float_kind            kind,
                      an_internal_float_value *float_value_1,
                      an_internal_float_value *float_value_2,
                      a_boolean               *unordered);

extern void fp_ceil(a_float_kind            kind,
                    an_internal_float_value *value,
                    an_internal_float_value *result,
                    a_boolean               *err);

#if C99_IL_EXTENSIONS_SUPPORTED
extern void cx_add(a_float_kind              kind,
                   an_internal_complex_value *value_1,
                   an_internal_complex_value *value_2,
                   an_internal_complex_value *result,
                   a_boolean                 *err,
                   a_boolean                 *depends_on_fp_mode);

extern void cx_subtract(a_float_kind              kind,
                        an_internal_complex_value *value_1,
                        an_internal_complex_value *value_2,
                        an_internal_complex_value *result,
                        a_boolean                 *err,
                        a_boolean                 *depends_on_fp_mode);

extern void cx_negate(a_float_kind              kind,
                      an_internal_complex_value *value_1,
                      an_internal_complex_value *result,
                      a_boolean                 *err,
                      a_boolean                 *depends_on_fp_mode);

extern void cx_multiply(a_float_kind              kind,
                        an_internal_complex_value *value_1,
                        an_internal_complex_value *value_2,
                        an_internal_complex_value *result,
                        a_boolean                 *err,
                        a_boolean                 *depends_on_fp_mode);

extern void cx_divide(a_float_kind              kind,
                      an_internal_complex_value *value_1,
                      an_internal_complex_value *value_2,
                      an_internal_complex_value *result,
                      a_boolean                 *err,
                      a_boolean                 *depends_on_fp_mode);

extern a_boolean cx_equal(a_float_kind              kind,
                          an_internal_complex_value *value_1,
                          an_internal_complex_value *value_2);
#endif /* C99_IL_EXTENSIONS_SUPPORTED */

extern a_boolean fp_signbit(a_float_kind            kind,
                            an_internal_float_value *value);

extern a_boolean fp_is_negative(a_float_kind            kind,
                                an_internal_float_value *value);

a_boolean fp_same_representation(a_float_kind            kind,
                                 an_internal_float_value *value_1,
                                 an_internal_float_value *value_2);

#if USE_SOFTFLOAT || HOST_HAS_FLOAT16_TYPE
/* In these configurations, the float16 representation is 16 bits, so no
   adjustment is required. */
#define adjust_float16_representation_if_needed(x) /* nothing */
#else /* !(USE_SOFTFLOAT || HOST_HAS_FLOAT16_TYPE) */
extern void adjust_float16_representation_if_needed(a_byte *rep);
#endif /* USE_SOFTFLOAT || HOST_HAS_FLOAT16_TYPE */
extern unsigned int fp_hash(an_internal_float_value *value);

extern void float_pt_init(void);

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef FLOAT_PT_H */

