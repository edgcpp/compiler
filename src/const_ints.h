/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

const_ints.h -- Declarations related to manipulation of target integer
                constants.

*/

/* Avoid including these declarations more than once. */
#ifndef CONST_INTS_H
#define CONST_INTS_H 1

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

#if INTEGER_VALUE_REPR_IS_A_HOST_INTEGER
/* Do a signed right shift of an_integer_value.   If the operand is
   negative then we need to construct a mask that will produce the bits
   that would be shifted in.  C does not guarantee that a right
   shift of a signed quantity will sign extend. */
#define signed_shift_right(value, bits)                               \
  ((a_signed_integer_value)((an_integer_value)((value) >> bits) |     \
   (((a_signed_integer_value)(value) < 0) ?                           \
                     ~((~(an_integer_value)0) >> bits) : 0)))
#else /* !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
/* Do a signed right shift of a host large integer. */
#define signed_shift_right(value, bits)                               \
  ((a_host_large_integer)(((a_host_large_unsigned)(value) >> bits) |  \
   (((a_host_large_integer)(value) < 0) ?                             \
                     ~((~(a_host_large_unsigned)0) >> bits) : 0)))
#endif /* INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */

/* Return TRUE if the sign of the integer value is negative. */
#if INTEGER_VALUE_REPR_IS_A_HOST_INTEGER
#define sign_of(value) ((a_signed_integer_value)(value) < 0)
#else /* INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
#define sign_of(value)						\
  (((value).part[0] & SIGN_BIT_INT_VALUE_PART) != 0)
#endif /* INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */

#if INTEGER_VALUE_REPR_IS_A_HOST_INTEGER

/* Set the integer value entry *intval to the signed value "value". */
#define set_integer_value(intval, value)			        \
  (*(intval) = (an_integer_value)(value))


/* Set the integer value entry *intval to the unsigned value "value". */
#define set_unsigned_integer_value(intval, value)		        \
  (*(intval) = (an_integer_value)(value))


/* Extract a host large integer *val from an_integer_value *intval.  (The
   "(void)is_signed" as the left operand of a comma is there only to silence
   compiler warnings about unused variables that otherwise pop up.  Lint, on
   the other hand, warns about that useless expression instead.) */
#if defined(_lint)
#define conv_integer_value_to_host_large_integer(intval, is_signed, val, err) \
  (*(val) = *(a_host_large_integer *)(intval), *(err) = FALSE)
#else /* !defined(_lint) */
#define conv_integer_value_to_host_large_integer(intval, is_signed, val, err) \
  ((void)is_signed, *(val) = *(a_host_large_integer *)(intval), *(err) = FALSE)
#endif /* defined(_lint) */


/* Logical OR two integer values.  The result is returned in the first
   operand (op_1 = op_1 | op_2). */
#define or_integer_values(op_1, op_2)					\
  (*(op_1) = *(op_1) | *(op_2))


/* Logical AND two integer values.  The result is returned in the first
   operand (op_1 = op_1 & op_2). */
#define and_integer_values(op_1, op_2)					\
  (*(op_1) = *(op_1) & *(op_2))


/* Logical exclusive OR two integer values.  The result is returned in the
   first operand (op_1 = op_1 ^ op_2). */
#define xor_integer_values(op_1, op_2)					\
  (*(op_1) = *(op_1) ^ *(op_2))


/* Sign extend an integer value.  The current value consists of "bits"
   bits.  The high order bit of the field is the sign bit. */
#define sign_extend_integer_value(value, bits)				\
{									\
  int se_shift_bits = (int)(BITS_IN_AN_INTEGER_VALUE - (bits));         \
  a_signed_integer_value se_work;					\
  se_work = *(value) << se_shift_bits;					\
  *(value) = signed_shift_right(se_work, se_shift_bits);		\
}


/* Complement an integer value.  The result is returned in the
   operand (op_1 = ~op_1). */
#define complement_integer_value(op_1)					\
  (*(op_1) = ~*(op_1))

/* Increment the integer value *intval.  No overflow checking is done. */
#define incr_integer_value(intval)					\
 ((*(intval))++)

#else /* !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */

extern void set_integer_value(an_integer_value		*intval,
                              a_host_large_integer	value);

extern void set_integer_value(an_integer_value		*intval,
                              const an_integer_value	&value);

extern void set_unsigned_integer_value(an_integer_value		*intval,
                                       a_host_large_unsigned	value);

extern void set_unsigned_integer_value(an_integer_value		*intval,
                                       const an_integer_value	&value);

extern void conv_integer_value_to_host_large_integer(
                                             an_integer_value        *intval,
                                             a_boolean               is_signed,
                                             a_host_large_integer    *value,
                                             a_boolean               *err);

extern void or_integer_values(an_integer_value *op_1,
		              an_integer_value *op_2);

extern void and_integer_values(an_integer_value *op_1,
		               an_integer_value *op_2);

extern void xor_integer_values(an_integer_value *op_1,
		               an_integer_value *op_2);

extern void sign_extend_integer_value(an_integer_value *value,
                                      size_t            bits);

extern void complement_integer_value(an_integer_value *op_1);

extern void incr_integer_value(an_integer_value *intval);

#endif /* INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */

template<typename a_Host_integer_type>
inline void set_host_integer_value(an_integer_value     *intval,
                                   a_Host_integer_type  value)
/*
Set the integer value entry *intval to the value "value" (with the sign
automatically detected).
*/
{
  a_boolean  value_signed = a_Host_integer_type(-1) < a_Host_integer_type(0);

  /* As the value of value_signed depends only on a constant (dependent on
     instantiation), optimizers should reduce this to the correct branch. */
  if (value_signed) {
    set_integer_value(intval, (a_host_large_integer)value);
  } else {
    set_unsigned_integer_value(intval, (a_host_large_unsigned)value);
  }  /* if */
}  /* set_host_integer_value */


extern void make_integer_value_mask(an_integer_value *mask,
                                    size_t           bits);

extern a_boolean int_constant_is_signed(a_constant_ptr constant);

extern a_host_large_integer value_of_integer_value(
					an_integer_value	*int_value,
					a_boolean		is_signed,
					a_boolean		*ovflo);

extern a_host_large_unsigned unsigned_value_of_integer_value(
					    an_integer_value	*int_value,
					    a_boolean		is_signed,
					    a_boolean		*ovflo);

extern a_host_large_integer value_of_integer_constant(a_constant *cp,
                                                      a_boolean  *ovflo);

extern
a_host_large_unsigned unsigned_value_of_integer_constant(a_constant *cp,
                                                         a_boolean  *ovflo);

extern int cmp_integer_constants(a_constant *con1,
                                 a_constant *con2);

extern int cmplit_integer_constant(a_constant           *con1,
                                   a_host_large_integer value2);

extern int cmpulit_integer_constant(a_constant            *con1,
                                    a_host_large_unsigned unsigned_value2);

/* Interface to cmplit_integer_constant for the simple case of testing
   for equality. */
#define eqlit_integer_constant(con1, value2)                          \
  (cmplit_integer_constant((con1), (value2)) == 0)

/* Interface to cmplit_integer_constant for the simple case of getting
   the sign (-1, 0, +1) of an integer constant. */
#define sign_of_integer_constant(con) \
  cmplit_integer_constant((con), (a_host_large_integer)0)

extern a_boolean in_range_for_integer_kind(a_constant      *min_con,
                                           a_constant      *max_con,
                                           an_integer_kind ikind);

extern a_targ_size_t integer_value_bit_size_for_type(a_type_ptr type);

extern a_boolean integer_value_can_represent_type_width(a_type_ptr type);

extern void integer_value_range_for_type(a_type_ptr       type,
                                         an_integer_value *min_value,
                                         an_integer_value *max_value);

extern a_boolean integer_value_in_range_for_type(an_integer_value *value,
                                                 a_boolean        is_signed,
                                                 a_type_ptr       type);

extern a_boolean integer_constant_in_range_for_type(a_constant *min_con,
                                                    a_constant *max_con,
                                                    a_type_ptr type);

extern void trim_integer_value_to_type(an_integer_value *value,
                                       a_type_ptr       type);

extern a_boolean le_max_integer_value_of_kind(an_integer_value *value,
	                                      a_boolean        is_signed,
	                                      an_integer_kind  ikind);

extern a_boolean is_max_value_for_integer_kind(a_constant      *con,
                                               an_integer_kind ikind);

extern size_t bits_required_to_represent_integer_constant(a_constant *cp);

extern a_number_buffer str_for_integer_value(an_integer_value *p_value,
                                             a_boolean        is_signed,
                                             a_boolean        non_arithmetic,
                                             a_targ_size_t    size);

extern a_number_buffer str_for_integer_value(an_integer_value *value);

extern a_number_buffer str_for_integer_constant(a_constant *cp);

extern a_number_buffer decimal_str_for_integer_constant(a_constant *cp);

extern
void conv_integer_value_to_float(an_integer_value		*int_value,
				 a_boolean			is_signed,
			         an_internal_float_value	*float_value,
				 a_float_kind			float_kind,
				 a_boolean			*err);

extern void const_ints_init(void);

extern int cmp_integer_values(an_integer_value *op_1,
		  	      a_boolean	        op_1_signed,
			      an_integer_value *op_2,
			      a_boolean	        op_2_signed);

extern void add_integer_values(an_integer_value *op_1,
			       an_integer_value *op_2,
			       a_boolean	 is_signed,
			       a_boolean	 *err);

extern void add_mixed_signed_integer_values(an_integer_value *op_1,
				            a_boolean	      op_1_signed,
				            an_integer_value *op_2,
				            a_boolean	      op_2_signed,
				            a_boolean	      *err);

extern void subtract_mixed_signed_integer_values(an_integer_value *op_1,
					         a_boolean	   op_1_signed,
					         an_integer_value *op_2,
					         a_boolean	   op_2_signed,
					         a_boolean	   *err);

extern void shift_left_integer_value(an_integer_value *op_1,
				     int	      op_2,
				     a_boolean	       *err);

extern void shift_right_integer_value(an_integer_value *op_1,
				      int	       op_2,
				      a_boolean	       is_signed,
				      a_boolean	       sign_extend);

extern void subtract_integer_values(an_integer_value *op_1,
			            an_integer_value *op_2,
			            a_boolean	      is_signed,
			            a_boolean	      *err);

extern void negate_integer_value(an_integer_value *op_1,
			         a_boolean	  *err);

extern void multiply_integer_values(an_integer_value *orig_op_1,
			            an_integer_value *orig_op_2,
			            a_boolean	      is_signed,
			            a_boolean	      *err);

extern void divide_integer_values(an_integer_value *op_1,
				  an_integer_value *op_2,
				  a_boolean	   is_signed,
				  a_boolean	   *err);

extern void remainder_integer_values(an_integer_value *op_1,
				     an_integer_value *op_2,
				     a_boolean	      is_signed,
				     a_boolean	      *err);

#if !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER || FIXED_POINT_ALLOWED
extern void conv_float_string_to_integer_value
                                       (a_const_char		*float_str,
					an_integer_value	*intval,
					a_boolean		is_signed,
					a_boolean		*err);
#endif /* !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER || FIXED_POINT_ALLOWED */

extern void get_integer_size_and_alignment(an_integer_kind  ikind,
                                           a_targ_size_t    *p_size,
                                           a_targ_alignment *p_alignment);
extern
void get_bit_precise_integer_size_and_alignment(a_targ_size_t    bit_width,
                                                a_targ_size_t    *p_size,
                                                a_targ_alignment *p_alignment);

extern an_integer_kind int_kind_for_size_and_alignment(
                                                a_targ_size_t    size,
                                                a_targ_alignment alignment,
                                                a_boolean        is_signed);

extern unsigned f_unsigned_to_string_buf(a_host_large_unsigned val,
                                         char                  *buf);

#define unsigned_to_string_buf(val, buf)                                     \
  (((val) < 10) ? ((buf)[0] = (char)('0'+(val)), (buf)[1] = '\0', 1)         \
                : f_unsigned_to_string_buf(val, buf))

/*lint -emacro(2704,signed_to_string_buf)*/
#define signed_to_string_buf(val, buf)                                       \
  (((val) < 0) ? ((buf)[0] = '-',                                            \
                  1+unsigned_to_string_buf((a_host_large_unsigned)-(val),    \
                                           (buf)+1))                         \
               : unsigned_to_string_buf((a_host_large_unsigned)(val), buf))
  


#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED || IA64_ABI
extern an_integer_kind int_kind_for_bit_size(unsigned int  number_of_bits,
                                             a_boolean     is_signed);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED || IA64_ABI */

#if DEBUG
/* GCOVR_EXCL_START */
extern a_number_buffer db_format_integer_value(an_integer_value  *value);

extern void db_signed_integer_value(an_integer_value  *value);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

/*
Arrays containing the minimum and maximum values for each integer kind.
*/
EXTERN_THREAD an_integer_value
		min_integer_value_of_kind[(int)ik_last],
		max_integer_value_of_kind[(int)ik_last];

#if BUILTIN_FUNCTIONS_ENABLED
extern a_boolean swap_bytes_in_unsigned_integer(unsigned int     bytes,
                                                an_integer_value *value,
                                                an_integer_value *swapped);
#endif /* BUILTIN_FUNCTIONS_ENABLED */

extern a_boolean conv_bytes_to_integer_value(an_integer_value *value,
                                             char             *bytes,
                                             size_t           num_bytes);

/*
This structure is used to convert between integer representations composed of
"chunks" (i.e., host native integral value types) to represent a potentially
arbitrarily large integer value.

an_Integral_type is the integral type used for the target integer
representation's chunks.  a_Part_type is the integral type used for the source
integer representation's chunks.  See the documentation of
Integer_translator::parts and Integer_translator::remainder to understand the
semantics of the operation.

The given capacity value is the pre-allocated storage capacity available for
use by the dynamic array of an_Integral_type values.  This should typically
correspond with the expected number of target integer representation chunks.
*/
template<typename an_Integral_type, unsigned a_Capacity>
struct Integer_translator {
  template<typename a_Part_type>
  inline void add_part(a_Part_type part);
  const an_Integral_type& operator[](size_t idx) const
    { return this->parts[idx]; }
  size_t length() const
    { return this->parts.length(); }
private:
  Small_dyn_array<an_Integral_type, a_Capacity, General_allocator>
                parts = {};
                        /* These are the new integer values representing
                           the bytes added via add_part.

                           For instance (assuming no prior state) if
                           an_Integral_type is a 4 byte integer and a 2 byte
                           integer was just given to add_part, the value of
                           parts is one an_Integral_type value.  This value has
                           its first 2 bytes set equal to the 2 bytes of the
                           integer given to add_part. */
  size_t        remainder = 0;
                        /* This is the number of unused bytes in the most
                           recently appended part in the parts data member.

                           For instance (assuming no prior state) if
                           an_Integral_type is a 4 byte integer and a 2 byte
                           integer was just given to add_part, the value of
                           remainder is 2 (as there are 2 unused bytes in the 4
                           byte integer). */
};  /* Integer_translator */


template<typename an_Integral_type, unsigned a_Capacity>
template<typename a_Part_type>
void Integer_translator<an_Integral_type, a_Capacity>::add_part(
                                                              a_Part_type part)
/*
Add the given integer part to the parts of the output integer stream.

See the documentation of Integer_translator::parts and
Integer_translator::remainder to understand the semantics of the operation.
*/
{
  constexpr size_t num_src_bytes = sizeof(a_Part_type);
  constexpr size_t num_dst_bytes = sizeof(an_Integral_type);

  for (ptrdiff_t i = num_src_bytes - 1; i >= 0; --i) {
    if (this->remainder == 0) {
      this->parts.push_back(0);
      this->remainder = num_dst_bytes;
    }  /* if */
    --this->remainder;

    an_Integral_type
                &dest_part = this->parts.back_elem();
    an_Integral_type
                next_part_byte = (part >> (8 * i)) & 0xFF;
    size_t      dest_bit_adjustment = (this->remainder * 8);
    dest_part |= next_part_byte << dest_bit_adjustment;
  }  /* for */
}  /* Integer_translator::add_part */


using an_integer_value_translator =
#if INTEGER_VALUE_REPR_IS_A_HOST_INTEGER
                Integer_translator<an_integer_value, 1>;
#else /* !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
                Integer_translator<an_int_value_part,
                                   INT_VALUE_PARTS_PER_INTEGER_VALUE>;
#endif /* INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
                        /* This is the type of an Integer_translator targeting
                           the current front end an_integer_value
                           representation. */

inline a_boolean has_translator_overflowed(
                                        an_integer_value_translator translator)
/*
Return TRUE if the given an_integer_value translator has overflowed what can be
represented in the current front end configuration; otherwise, return FALSE.
*/
{
  a_boolean        result = FALSE;
  constexpr size_t max_length =
#if INTEGER_VALUE_REPR_IS_A_HOST_INTEGER
                1;
#else /* !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
                INT_VALUE_PARTS_PER_INTEGER_VALUE;
#endif /* INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
  if (translator.length() > max_length) {
    result = TRUE;
  }  /* if */
  return result;
}  /* has_translator_overflowed */


inline an_integer_value as_integer_value(
                                        an_integer_value_translator translator)
/*
Return the given an_integer_value translator's current state as
an_integer_value.
*/
{
  check_assertion(!has_translator_overflowed(translator));
  an_integer_value result;

#if INTEGER_VALUE_REPR_IS_A_HOST_INTEGER
  result = translator[0];
#else /* !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
  set_unsigned_integer_value(&result, (a_host_large_unsigned)0);
  for (size_t i = 0; i < translator.length(); ++i) {
    result.part[i] = translator[i];
  }  /* for */
#endif /* INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
  return result;
}  /* as_integer_value */


template<typename an_Integral_type, unsigned a_Capacity>
inline Integer_translator<an_Integral_type, a_Capacity>
integer_value_as_translator(const an_integer_value &value)
/*
Return the given an_integer_value represented as an Integer_translator value
with the given initial integral type and capacity.
*/
{
  Integer_translator<an_Integral_type, a_Capacity> result;

#if INTEGER_VALUE_REPR_IS_A_HOST_INTEGER
  result.add_part(value);
#else /* !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
  for (size_t i = 0; i < INT_VALUE_PARTS_PER_INTEGER_VALUE; ++i) {
    result.add_part(value.part[i]);
  }  /* for */
#endif /* INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
 return result;
}  /* integer_value_as_translator */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef CONST_INTS_H */


