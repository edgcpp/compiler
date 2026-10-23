/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

fixed_pt.c -- Routines that manipulate internal fixed-point quantities.

The versions in this file are for prototyping only, and should be replaced
for a production version.

*/

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

#if FIXED_POINT_ALLOWED

#include "folding.h"

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

a_boolean fxp_value_is_zero(a_fixed_point_value  *value)
/*
Return TRUE if and only if the given fixed-point value is zero.
*/
{
  an_integer_value  zero;

  set_integer_value(&zero, (a_host_large_integer)0);
  /* Use the integer comparison routine.  Note that signedness doesn't
     matter for the zero case. */
  return (cmp_integer_values(value, /*op_1_signed=*/FALSE,
                             &zero, /*op_2_signed=*/FALSE) == 0);
}  /* fxp_value_is_zero */

#if !STANDALONE_UTILITY_PROGRAM

void fxp_init_value(a_fixed_point_value  *value)
/*
Initialize the given fixed-point value to zero.
*/
{
  set_integer_value(value, (a_host_large_integer)0);
}  /* fxp_init_value */


static a_targ_size_t value_bits_for_fixed_point(
                                           a_fixed_point_type_descr *fxp_descr)
/*
Return the number of data bits in a fixed-point value (i.e., the number of
bits excluding the sign bit).
*/
{
  a_targ_size_t bits =
                  targ_sizeof_fixed_point[fxp_descr->is_unsigned]
                                         [(int)fxp_descr->precision]
                                         [fxp_descr->is_fract_type] * CHAR_BIT;

  if (!fxp_descr->is_unsigned) {
    check_assertion(bits > 0);
    bits--;
  }  /* if */
  return bits;
}  /* value_bits_for_fixed_point */


static a_targ_size_t sizeof_fixed_point(a_fixed_point_type_descr *fxp_descr)
/*
Return the number of bytes in a fixed-point value.
*/
{
  return targ_sizeof_fixed_point[fxp_descr->is_unsigned]
                                [(int)fxp_descr->precision]
                                [fxp_descr->is_fract_type];
}  /* sizeof_fixed_point */


a_targ_size_t non_fractional_bits_for_fixed_point(
                                           a_fixed_point_type_descr *fxp_descr)
/*
Return the number of bits in the non-fractional part of a fixed-point value.
The sign bit (if any) is included in the non-fractional bits.
*/
{
  a_targ_alignment fract_bits;
  a_targ_size_t    total_bits;

  fract_bits = targ_fractional_bits_for_fixed_point[fxp_descr->is_unsigned]
                                                   [(int)fxp_descr->precision]
                                                   [fxp_descr->is_fract_type];
  total_bits = targ_sizeof_fixed_point[fxp_descr->is_unsigned]
                                           [(int)fxp_descr->precision]
                                           [fxp_descr->is_fract_type] *
                                                                      CHAR_BIT;
  return total_bits - fract_bits;
}  /* non_fractional_bits_for_fixed_point */


a_fixed_point_type_descr *fxp_descr_for_constant(a_constant_ptr	cp)
/*
Return the fixed-point descriptor pointer for the specified constant.
*/
{
  a_type_ptr	type = skip_typerefs(cp->type);
  check_assertion(type->kind == (a_type_kind)tk_fixed_point);
  return &type->variant.fixed_point;
}  /* fxp_descr_for_constant */


static void set_fixed_point_to_saturated_value(
                                          a_fixed_point_value      *value,
                                          a_targ_size_t            value_bits,
                                          a_boolean                is_negative,
                                          a_fixed_point_type_descr *fxp_descr)
/*
Set value to the representation used for a saturated fixed-point
value.  is_negative is TRUE if the value should be the saturated negative
value.  The positive value is all 1 bits, except for the sign bit.  The
negative value has just the sign bit set.  value_bits is the number of bits
used to represent an fxp_descr value.  Note that it does not include the
sign bit.
*/
{
  int	shift_count;

  /* Set all of the bits. */
  set_integer_value(value, (a_host_large_integer)-1);
  if (!fxp_descr->is_unsigned) {
    /* Clear the sign bit. */
    shift_right_integer_value(value, 1,
                             /*is_signed=*/FALSE, /*sign_extend=*/FALSE);
    /* For a negative value, only the sign bit should be set. */
    if (is_negative) complement_integer_value(value);
    /* Update value_bits to reflect the bit used for the sign. */
    value_bits++;
  }  /* if */
  /* Shift the saturated value to just fill the portion of the integer
     value actually used to the representation. */
  shift_count = (int)BITS_IN_AN_INTEGER_VALUE - (int)value_bits;
  if (shift_count > 0) {
    shift_right_integer_value(value, shift_count,
                              /*is_signed=*/!fxp_descr->is_unsigned,
                              /*sign_extend=*/TRUE);
  }  /* if */
}  /* set_fixed_point_to_saturated_value */


static void negate_fixed_point_value(a_fixed_point_value	*op_1,
				     a_boolean			*err)
/*
Negate a fixed-point value.  The result is returned in the first operand
(op_1 = -op_1).  err is TRUE if an overflow occurred.

This routine requires that a_fixed_point_value be an_integer_value.  Note
that this routine only checks for error cases that cannot be represented
by an_integer_value.  Range checking for specific fixed-point types must
be done by the caller.  fxp_negate is a more general version of this
routine.
*/
{
  negate_integer_value(op_1, err);
}  /* negate_fixed_point_value */


/*
Return TRUE if the fixed-point value is negative.  The caller is responsible
for checking that the fixed-point type is signed.

This macro requires that a_fixed_point_value be an_integer_value.
*/
#define fxp_sign_of(value) (sign_of(value))

#if INTEGER_VALUE_REPR_IS_A_HOST_INTEGER

/*
Return the byte offset of a given logical byte of an integer value.  This is
a no-op when an integer value is a host integer.

This is only used on little-endian systems.
*/
#define byte_offset_in_integer_value(i) (i)

#else /* INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */

static int byte_offset_in_integer_value(unsigned int	byte)
/*
Return the byte offset of a given logical byte of an integer value.

This is only used on little-endian systems.
*/
{
  long unsigned int_value_part = INT_VALUE_PARTS_PER_INTEGER_VALUE -
                                            (byte / SIZEOF_INT_VALUE_PART) - 1;
  long unsigned offset = (int_value_part * SIZEOF_INT_VALUE_PART) +
                                                (byte % SIZEOF_INT_VALUE_PART);

  return (int)offset;
}  /* byte_offset_in_integer_value */

#endif /* INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */


static void store_hex_fxp_value(
				a_mantissa_ptr			mp,
				a_fixed_point_type_descr	*fxp_descr,
				a_fixed_point_value		*value)
/*
Store the value represented by mp in the fixed-point value "value".
fxp_descr describes the format of the value being stored.
*/
{
  a_targ_size_t parts_to_copy;
  a_targ_size_t source_size;
  a_targ_size_t part_offset;

  /* Zero the memory so that all of the space occupied by "value"
     is cleared, even if we are not storing all of the bytes of the value. */
  memzero((char *)value, sizeof(a_fixed_point_value));
  source_size = sizeof_fixed_point(fxp_descr);
  parts_to_copy = (source_size + sizeof(an_fp_value_part) - 1) /
                   sizeof(an_fp_value_part);
  /* In most cases, an entire fp_value_part is copied.  If the target is just
     a 1 or 2 byte value, however, only the high-order bytes of the source
     fp_value_part are copied. */
  part_offset = source_size < sizeof(an_fp_value_part)
                                     ? source_size  : sizeof(an_fp_value_part);
  part_offset = sizeof(an_fp_value_part) - part_offset;
  /* The source value is in the upper source_size bytes of the mantissa.
     This needs to be copied to the low order bytes of the fixed-point
     value. */
  if (host_little_endian) {
    /* For a typical system where an_fp_value_part is 4 bytes and an
       integer value is 8 bytes, a short value is copied:
         from 2 to 0
         from 3 to 1
       A 64 bit value is copied:
         from 4 to 0
         from 5 to 1
         from 6 to 2
         from 7 to 3
         from 0 to 4
         from 1 to 5
         from 2 to 6
         from 3 to 7
    */
    unsigned int i;
    for (i = 0; i < source_size; ++i) {
      char	*source;
      char	*dest;
      int	source_part;
      int	source_byte;
      dest = &((char*)value)[byte_offset_in_integer_value(i)];
      source_part = (int)((parts_to_copy - 1) - (i/sizeof(an_fp_value_part)));
      source_byte = (int)((i % sizeof(an_fp_value_part)) + part_offset);
      source = (char*)&(mp->parts[source_part]) + source_byte;
      *dest = *source;
#if DEBUG
/* GCOVR_EXCL_START */
      if (db_flag_is_set("fxp_store")) {
        long  source_offset, dest_offset;
        source_offset = (long)(source - (char *)(&mp->parts[0]));
        dest_offset = (long)(dest - (char*)&value[0]);
        fprintf(f_debug, "fxp copy from %ld to %ld, value=%x\n",
                source_offset, dest_offset,
                (unsigned int)(unsigned char)*dest);
      }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
    }  /* for */
  } else {
    /* Copy the value from the mantissa to the low order bytes of the
       fixed-point value. */
    memcpy((char*)value + sizeof(a_fixed_point_value) - source_size,
           (char*)&mp->parts[0], size_t_arg(source_size));
  }  /* if */
}  /* store_hex_fxp_value */


static void load_hex_fxp_value(a_fixed_point_value      *value,
                               a_fixed_point_type_descr *fxp_descr,
                               a_mantissa_ptr           mp,
                               long                     *exponent,
                               a_boolean                *is_negative)

/*
Create mantissa (mp), exponent, and is_negative from a_fixed_point_value
(value).  fxp_descr describes the format of the fixed-point value.
*/
{
  a_targ_size_t         parts_to_copy;
  a_targ_size_t         dest_size;
  a_targ_size_t         part_offset;
  a_fixed_point_value   local_value;

  init_mantissa(mp);
  *is_negative = !fxp_descr->is_unsigned && fxp_sign_of(*value);
  if (*is_negative) {
    a_boolean	local_err;
    local_value = *value;
    negate_fixed_point_value(&local_value, &local_err);
    /* An error should only occur on negating the smallest integer.  In
       that case, just use the original value. */
    if (!local_err) value = &local_value;
  }  /* if */
  dest_size = sizeof_fixed_point(fxp_descr);
  parts_to_copy = (dest_size + sizeof(an_fp_value_part) - 1) /
                   sizeof(an_fp_value_part);
  /* In most cases, an entire fp_value_part is copied.  If the target is just
     a 1 or 2 byte value, however, only the high-order bytes of the dest
     fp_value_part are copied. */
  part_offset = dest_size < sizeof(an_fp_value_part)
                                     ? dest_size  : sizeof(an_fp_value_part);
  part_offset = sizeof(an_fp_value_part) - part_offset;
  /* The destination value is in the upper dest_size bytes of the mantissa.
     This needs to be copied from the low order bytes of the fixed-point
     value.  See the comments in store_hex_fxp_value for more information. */
  if (host_little_endian) {
    unsigned int i;
    for (i = 0; i < dest_size; ++i) {
      char	*dest;
      char	*source;
      int	dest_part;
      int	dest_byte;
      source = &((char*)value)[byte_offset_in_integer_value(i)];
      dest_part = (int)((parts_to_copy - 1) - (i / sizeof(an_fp_value_part)));
      dest_byte = (int)((i % sizeof(an_fp_value_part)) + part_offset);
      dest = (char*)&(mp->parts[dest_part]) + dest_byte;
      *dest = *source;
    }  /* for */
  } else {
    /* Copy the value from the low order bytes for the fixed-point value
       to the mantissa. */
    memcpy((char*)&mp->parts[0],
           (char*)value + sizeof(a_fixed_point_value) - dest_size,
           size_t_arg(dest_size));
  }  /* if */
  /* The exponent is the number of non-fractional bits of the value. */
  *exponent = (long)non_fractional_bits_for_fixed_point(fxp_descr);
}  /* load_hex_fxp_value */


static void normalize_mantissa(a_mantissa_ptr mp,
                               long           *exponent)
/*
Normalize the mantissa value so that it occupies the high-order bits
of "mp".  Adjust the exponent accordingly.
*/
{
  if (!mantissa_is_zero(mp)) {
    /* Adjust the mantissa so that it is normalized in the high-order bits
       of the mantissa. */
    while ((mp->parts[0] & 0x80000000) == 0) {
      shift_left_mantissa(mp, 1);
      (*exponent)--;
    }  /* while */
  } else {
    /* Clear the exponent if the mantissa is zero. */
    *exponent = 0;
  }  /* if */
}  /* normalize_mantissa */


static void make_mantissa_from_integer_value(an_integer_value *value,
                                             a_boolean        is_negative,
                                             a_mantissa_ptr   mp,
                                             long             *exponent)
/*
Create a mantissa (mp) and exponent from an_integer_value (value).  If
value is negative, is_negative will be TRUE.
*/
{
  a_targ_size_t    parts_to_copy;
  an_integer_value local_value;

#if CHECKING
  { /* Make sure an_integer_value can be copied into a_mantissa. */
    a_boolean	okay = sizeof(an_integer_value) <= sizeof(mp->parts);
    check_assertion(okay);
  }
#endif /* CHECKING */
  /* Clear the mantissa. */
  init_mantissa(mp);
  *exponent = 0;
  if (is_negative) {
    /* If the source value is negative, get the positive version for the
       conversion. */
    a_boolean	err;
    local_value = *value;
    negate_integer_value(&local_value, &err);
    /* An error should only occur on negating the smallest integer.  In
       that case, just use the original value. */
    if (!err) value = &local_value;
  }  /* if */
  parts_to_copy = (sizeof(an_integer_value) + sizeof(an_fp_value_part) - 1) /
                   sizeof(an_fp_value_part);
  /* The source value is in the low order bytes of the integer value.
     This needs to be copied to the high order bytes of the mantissa. */
  if (host_little_endian) {
    unsigned int i;
    for (i = 0; i < sizeof(an_integer_value); ++i) {
      char	*source;
      char	*dest;
      source = &((char*)value)[byte_offset_in_integer_value(i)];
      dest = (char*)&(mp->parts[(parts_to_copy - 1) -
                                  (i / sizeof(an_fp_value_part))]) +
                       (i % sizeof(an_fp_value_part));
       *dest = *source;
    }  /* for */
  } else {
    /* Copy the value from the high-order bytes of the mantissa. */
    memcpy((char*)&mp->parts[0], (char*)value, sizeof(an_integer_value));
  }  /* if */
  *exponent = BITS_IN_AN_INTEGER_VALUE;
  /* Adjust the mantissa so that it is normalized in the high-order bits
     of the mantissa. */
  normalize_mantissa(mp, exponent);
}  /* make_mantissa_from_integer_value */


static void make_integer_value_from_mantissa(an_integer_value *value,
                                             a_boolean        is_negative,
                                             a_mantissa_ptr   mp,
                                             long             exponent,
                                             a_boolean        *err)
/*
Create an_integer_value from a mantissa (mp) and exponent.  If the value is
negative, is_negative will be TRUE.
*/
{
  int parts_to_copy;
  int shift_count;

  *err = FALSE;
  /* Compute the number of bits to shift the mantissa so that any fractional
     bits will be discarded. */
  shift_count = (int)((long)BITS_IN_AN_INTEGER_VALUE - exponent);
  if (shift_count >= 0) {
    if (shift_count > 0) shift_right_mantissa(mp, shift_count);
  } else {
    /* The value will not fit in an integer value. */
    *err = TRUE;
    goto done;
  }  /* if */
  parts_to_copy = (sizeof(an_integer_value) + sizeof(an_fp_value_part) - 1) /
                   sizeof(an_fp_value_part);
  /* The destination value is in the low order bytes of the integer value.
     This needs to be copied from the high order bytes of the mantissa. */
  if (host_little_endian) {
    unsigned int i;
    for (i = 0; i < sizeof(an_integer_value); ++i) {
      char	*dest;
      char	*source;
      dest = &((char*)value)[byte_offset_in_integer_value(i)];
      source = (char*)&(mp->parts[(size_t)(parts_to_copy - 1) -
                                  (i / sizeof(an_fp_value_part))]) +
                       (i % sizeof(an_fp_value_part));
       *dest = *source;
    }  /* for */
  } else {
    /* Copy the value to the high order bits of the mantissa. */
    memcpy((char*)value, (char*)&mp->parts[0], sizeof(an_integer_value));
  }  /* if */
  if (is_negative) {
    /* If the value should be negative, negate the resulting value. */
    a_boolean		local_err;
    an_integer_value	saved_value;
    saved_value = *value;
    negate_integer_value(value, &local_err);
    /* An error should only occur on negating the smallest integer.  In
       that case, just use the original value. */
    if (local_err) *value = saved_value;
  }  /* if */
done:
  return;
}  /* make_integer_value_from_mantissa */


static void clear_unused_mantissa_bits(a_mantissa_ptr mp,
                                       a_targ_size_t  bits_used)
/*
Zero any bits in the mantissa that are beyond the first N bits specified by
bits_used.
*/
{
  for (size_t part = 0; part < MANTISSA_PARTS; part++) {
    if (bits_used >= 32) {
      /* The whole part is used.  Skip to the next part. */
      bits_used -= 32;
      continue;
    } else if (bits_used == 0) {
      /* None of this part is used.  Clear it. */
      mp->parts[part] = 0;
    } else {
      /* A portion of the part is used.  Clear the unused part. */
      unsigned          bits_to_clear = (unsigned)(32 - bits_used);
      an_fp_value_part  mask;

      mask = 0xffffffff << bits_to_clear;
      mp->parts[part] &= mask;
      bits_used = 0;
    }  /* if */
  }  /* for */
}  /* clear_unused_mantissa_bits */


static void conv_mantissa_to_fixed_point(a_mantissa_ptr           mp,
                                         long                     exponent,
                                         a_boolean                is_negative,
                                         a_fixed_point_type_descr *fxp_descr,
                                         a_boolean                overflow,
                                         a_fixed_point_value      *value,
                                         a_boolean                *err,
                                         a_boolean                *inexact)
/*
Given a mantissa (mp) and exponent that represent a fixed-point value, check
that the value is representable in the destination type specified by
fxp_descr and shift the value as needed so that it contains the correct
number of value bits for the destination type.  is_negative is TRUE if the
value to be stored must be created as a negative value.  overflow is TRUE if
the value is already known to be too large.  Set *err on overflow.  Set
*inexact if any bits are lost because of scaling or rounding.
*/
{
  long          nonfract_bits = 0;
  long          shift_count = 0;
  a_targ_size_t value_bits = 0;
  unsigned      mantissa_bits = 0;
  unsigned      sign_bits = 0;

  *err = FALSE;
  *inexact = FALSE;
  if (!overflow) {
    if (is_negative && fxp_descr->is_unsigned) {
      /* A negative value being stored in an unsigned value.  Set it to
         zero. */
      init_mantissa(mp);
      exponent = 0;
    }  /* if */
    /* Adjust the mantissa so that it is normalized in the high-order bits
       of the mantissa. */
    normalize_mantissa(mp, &exponent);
    /* Compute the number of bits to shift the mantissa so that it contains
       the right number of fractional and non-fractional bits. */
    nonfract_bits = (long)non_fractional_bits_for_fixed_point(fxp_descr);
    value_bits = value_bits_for_fixed_point(fxp_descr);
    shift_count = nonfract_bits - exponent;
    mantissa_bits = number_of_bits_in_mantissa(mp, /*normalize=*/FALSE);
    sign_bits = fxp_descr->is_unsigned ? 0 : 1;
    if (shift_count > 0) shift_right_mantissa(mp, (int)shift_count);
    /* See if the result value has more bits of precision than fit into
       the destination type. */
    if (mantissa_bits > value_bits) *inexact = TRUE;
    /* Round the value to the nearest representable value. */
    round_hex_fp_value(mp, &exponent, value_bits + sign_bits,
                       /*is_fixed_point=*/TRUE, !fxp_descr->is_unsigned,
                       inexact);
    /* Zero any bits in the mantissa that are not actually used in the
       value. */
    clear_unused_mantissa_bits(mp, value_bits + sign_bits);
    /* Recompute the shift count and mantissa bits after rounding. */
    shift_count = nonfract_bits - exponent;
    mantissa_bits = number_of_bits_in_mantissa(mp, /*normalize=*/TRUE);
    /* A shift count of zero represents an overflow for a signed value because
       the sign bit would be needed for the representation. */
    if (shift_count < (long)sign_bits) {
      /* We would be shifting bits out of the high end of this mantissa. */
      overflow = TRUE;
    }  /* if */
  }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
  if (db_flag_is_set("fxp_conv")) {
    fprintf(f_debug, "fxp hex value: ");
    db_mantissa(mp);
    fprintf(f_debug, "exponent=%ld, nonfract=%ld, shift=%ld\n",
            exponent, nonfract_bits, shift_count);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  if (overflow) {
    /* On overflow, return an error flag and set the result value to a
       saturated value. */
    if (fxp_descr->is_fract_type && mantissa_bits == 1 && exponent == 1) {
      /* The input value is 1 or -1.  Return the saturated value, but don't
         set the error flag. */
    } else if (!fxp_descr->is_fract_type &&
               mantissa_bits == 1 && exponent == nonfract_bits) {
      /* The input is the smallest value of an _Accum type, or the positive
         version of the same value.  Also return the saturated value without
         setting the error flag. */
    } else if (fxp_descr->saturating) {
      /* If this is a saturating type, silently saturate. */
    } else {
      /* Except for the case above, set the error flag on overflow. */
      *err = TRUE;
    }  /* if */
    set_fixed_point_to_saturated_value(value, value_bits,
                                       is_negative, fxp_descr);
  } else {
    /* No overflow.  Store the result in the appropriate form. */
    store_hex_fxp_value(mp, fxp_descr, value);
    /* Negate the value, if necessary.  If the source is negative and the
       destination is unsigned, a diagnostic will be issued by the caller
       and the value set to zero above.  On overflow, the saturated value
       will have already been created with the appropriate sign above. */
    if (is_negative && !fxp_descr->is_unsigned) {
      a_boolean	negate_err;
      negate_fixed_point_value(value, &negate_err);
      if (negate_err) *err = TRUE;
    }  /* if */
  }  /* if */
  /* If an underflow occurred, set the flag that indicates that the resulting
     value is not an exact representation of the specified value. */
  if (mp->underflow) *inexact = TRUE;
}  /* conv_mantissa_to_fixed_point */


void conv_integer_to_fixed_point(a_constant_ptr		old_constant,
			         a_constant_ptr		new_constant,
			         an_error_code		*err_code,
			         an_error_severity	*err_severity)
/*
Convert the integer constant "old_constant" to a fixed-point constant
in "new_constant.  If, as a result of the conversion, a diagnostic should
be issued, set err_code and err_severity to the values for the message
to be issued; otherwise set err_code to ec_no_error.
*/
{
  a_mantissa		mantissa;
  long			exponent;
  a_boolean		is_negative;
  a_boolean		err;
  a_boolean		inexact;
  an_integer_kind	ikind;
  a_boolean		is_signed;
  size_t		bit_size;
  a_fixed_point_type_descr
			*fxp_descr;

  check_assertion(old_constant->kind == (a_constant_repr_kind)ck_integer);
  set_constant_kind(new_constant, (a_constant_repr_kind)ck_fixed_point);
  *err_code = ec_no_error;
  /* Determine attributes (size, signedness) of the integer kind. */
  get_integer_attributes(old_constant, &ikind, &is_signed, &bit_size);
  /* Determine if the value is negative. */
  is_negative = is_signed && sign_of_integer_constant(old_constant) < 0;
  /* Convert the integer into the internal mantissa representation. */
  make_mantissa_from_integer_value(&old_constant->variant.integer_value,
                                   is_negative, &mantissa, &exponent);
  fxp_descr = fxp_descr_for_constant(new_constant);
  /* Convert and store the mantissa as a fixed-point value. */
  conv_mantissa_to_fixed_point(&mantissa, exponent, is_negative,
                               fxp_descr,
                               /*overflow=*/FALSE,
                               &new_constant->variant.fixed_point_value,
                               &err, &inexact);
  if (err) {
    /* The conversion to fixed-point does not fit in the result type. */
    *err_code = ec_integer_to_fixed_conversion;
    *err_severity = es_error;
  } else if (is_negative && fxp_descr->is_unsigned) {
    /* The conversion results in a negative value being converted to
       unsigned. */
    *err_code = ec_fixed_sign_change;
    *err_severity = es_error;
  } else if (inexact) {
    /* The conversion loses precision.  This doesn't seem like it should be
       possible for an integer to fixed conversion, but is provided for
       in case there is some fixed format where this would be possible. */
    *err_code = ec_inexact_fixed_conversion;
    *err_severity = es_warning;
  }  /* if */
}  /* conv_integer_to_fixed_point */


void conv_fixed_point_to_integer(a_constant_ptr		old_constant,
			         a_constant_ptr		new_constant,
			         an_error_code		*err_code,
			         an_error_severity	*err_severity)
/*
Convert the fixed-point constant "old_constant" to an integer constant
in "new_constant.  If, as a result of the conversion, a diagnostic should
be issued, set err_code and err_severity to the values for the message
to be issued; otherwise set err_code to ec_no_error.
*/
{
  a_mantissa		mantissa;
  long			exponent;
  a_boolean		is_negative;
  a_boolean		err;
  a_fixed_point_type_descr
			*fxp_descr;
  an_integer_value	int_value;
  an_integer_kind	ikind;
  a_boolean		is_signed;
  size_t		bit_size;

  check_assertion(old_constant->kind == (a_constant_repr_kind)ck_fixed_point);
  set_constant_kind(new_constant, (a_constant_repr_kind)ck_integer);
  *err_code = ec_no_error;
  fxp_descr = fxp_descr_for_constant(old_constant);
  /* Convert the fixed-point value into the internal mantissa
     representation. */
  load_hex_fxp_value(&old_constant->variant.fixed_point_value,
                     fxp_descr, &mantissa, &exponent, &is_negative);
  /* Convert and store the mantissa as an integer value. */
  make_integer_value_from_mantissa(&int_value, is_negative,
                                   &mantissa, exponent, &err);
  /* Determine attributes (size, signedness) of the integer kind. */
  get_integer_attributes(new_constant, &ikind, &is_signed, &bit_size);
  /* If the value is negative and the destination is unsigned, issue as
     diagnostic. */
  if (is_negative && !is_signed) err = TRUE;
  if (err) {
    /* The conversion to integer does not fit in the result type.  This case
       occurs only if the fixed-point type does not fit in even the largest
       integer type, which is unlikely. */
    *err_code = ec_fixed_to_integer_conversion;
    *err_severity = es_error;
  } else {
    /* Truncate the value to the size of the destination integer type. */
    trunc_and_set_integer(&int_value, new_constant, /*check_overflow=*/TRUE,
                          /*saturate_on_overflow=*/FALSE,
                          err_code, err_severity);
    if (*err_code != ec_no_error) {
      /* If an error occurred while storing the value, remap the error code
         into a more appropriate one for this particular case. */
      *err_code = ec_fixed_to_integer_conversion;
      *err_severity = es_error;
    }  /* if */
  }  /* if */
}  /* conv_fixed_point_to_integer */


void conv_float_to_fixed_point(a_constant_ptr		old_constant,
			       a_constant_ptr		new_constant,
			       an_error_code		*err_code,
			       an_error_severity	*err_severity)
/*
Convert the floating-point constant "old_constant" to a fixed-point constant
in "new_constant.  If, as a result of the conversion, a diagnostic should
be issued, set err_code and err_severity to the values for the message
to be issued; otherwise set err_code to ec_no_error.
*/
{
  a_mantissa	mantissa;
  long		exponent;
  a_boolean	is_negative;
  a_boolean	err;
  a_boolean	inexact;
  a_fixed_point_type_descr
		*fxp_descr;
  a_boolean	overflow = FALSE;
  a_type_ptr	float_tp = skip_typerefs(old_constant->type);
  a_float_kind	float_kind = float_tp->variant.float_kind;
  an_internal_float_value
		*float_value;

#if C99_IL_EXTENSIONS_SUPPORTED
  an_internal_float_value zero;

  if (float_tp->kind == (a_type_kind)tk_complex) {
    /* Converting from complex to fixed-point.  The real part of the
       constant is converted to fixed-point, and the imaginary part is
       discarded. */
    float_value = &old_constant->variant.complex_value->real;
  } else if (float_tp->kind == (a_type_kind)tk_imaginary) {
    /* Converting from imaginary to fixed-point.  The result is zero. */
    fp_host_large_integer_to_float(float_kind, (a_host_large_integer)0,
                                   &zero, &err);
    float_value = &zero;
  } else
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  /* Do not insert code here. */
  {
    check_assertion(old_constant->kind == (a_constant_repr_kind)ck_float);
    float_value = &old_constant->variant.float_value;
  }  /* if */
  set_constant_kind(new_constant, (a_constant_repr_kind)ck_fixed_point);
  *err_code = ec_no_error;
  fxp_descr = fxp_descr_for_constant(new_constant);
  /* Convert the floating-point value into the internal mantissa
     representation.  This is done even in the NaN and infinity case
     to set is_negative flag, etc. */
  load_hex_fp_value(float_value, float_kind,
                    &mantissa, &exponent, &is_negative,
                    /*restore_implicit_bit=*/TRUE);
#if TARG_HAS_IEEE_FLOATING_POINT
  if (fp_is_nan_or_infinity(float_value, float_kind)) {
    /* Not-a-number or infinity.  Treat this as an overflow. */
    overflow = TRUE;
  }  /* if */
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
  /* The fixed-point representation has no negative zero, so clear the
     is_negative flag if the mantissa is zero. */
  if (is_negative && mantissa_is_zero(&mantissa)) is_negative = FALSE;
  /* Convert and store the mantissa as a fixed-point value. */
  conv_mantissa_to_fixed_point(&mantissa, exponent, is_negative,
                               fxp_descr, overflow,
                               &new_constant->variant.fixed_point_value,
                               &err, &inexact);
  if (err) {
    /* The conversion to fixed-point does not fit in the result type. */
    *err_code = ec_float_to_fixed_conversion;
    *err_severity = es_error;
  } else if (is_negative && fxp_descr->is_unsigned) {
    /* The conversion results in a negative value being converted to
       unsigned. */
    *err_code = ec_fixed_sign_change;
    *err_severity = es_error;
  } else if (inexact) {
    /* The conversion loses precision. */
    *err_code = ec_inexact_fixed_conversion;
    *err_severity = es_warning;
  }  /* if */
}  /* conv_float_to_fixed_point */


void conv_fixed_point_to_float(a_constant_ptr		old_constant,
			       a_constant_ptr		new_constant,
			       an_error_code		*err_code,
			       an_error_severity	*err_severity)
/*
Convert the fixed-point constant "old_constant" to a floating-point constant
in "new_constant.  If, as a result of the conversion, a diagnostic should
be issued, set err_code and err_severity to the values for the message
to be issued; otherwise set err_code to ec_no_error.
*/
{
  a_mantissa		mantissa;
  long			exponent;
  a_boolean		is_negative;
  a_boolean		err;
  a_boolean		inexact;
  a_fixed_point_type_descr
			*fxp_descr;
  a_type_ptr		float_tp = skip_typerefs(new_constant->type);
#if C99_IL_EXTENSIONS_SUPPORTED
  a_float_kind		float_kind = float_tp->variant.float_kind;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  an_internal_float_value
			*float_value = NULL;
  a_boolean		skip_conversion = FALSE;
  a_constant_repr_kind	constant_kind = (a_constant_repr_kind)ck_float;

  check_assertion(old_constant->kind == (a_constant_repr_kind)ck_fixed_point);
#if C99_IL_EXTENSIONS_SUPPORTED
  /* We may be converting to a nonreal floating type. */
  if (float_tp->kind == (a_type_kind)tk_complex) {
    constant_kind = (a_constant_repr_kind)ck_complex;
  } else if (float_tp->kind == (a_type_kind)tk_imaginary) {
    constant_kind = (a_constant_repr_kind)ck_imaginary;
  }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  set_constant_kind(new_constant, constant_kind);
  *err_code = ec_no_error;
  fxp_descr = fxp_descr_for_constant(old_constant);
#if C99_IL_EXTENSIONS_SUPPORTED
  if (float_tp->kind == (a_type_kind)tk_complex) {
    /* Converting to complex.  The value is converted into the real
       part, and the imaginary part is set to zero. */
    float_value = &new_constant->variant.complex_value->real;
    fp_host_large_integer_to_float(float_kind, (a_host_large_integer)0,
                                   &new_constant->variant.complex_value->imag,
                                   &err);
    check_assertion(!err);
  } else if (float_tp->kind == (a_type_kind)tk_imaginary) {
    /* Converting to imaginary.  The result is zero. */
    fp_host_large_integer_to_float(float_kind, (a_host_large_integer)0,
                                   &new_constant->variant.float_value, &err);
    check_assertion(!err);
    skip_conversion = TRUE;
  } else
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  /* Do not insert code here. */
  {
    float_value = &new_constant->variant.float_value;
  }  /* if */
  if (!skip_conversion) {
    /* Convert the fixed-point value into the internal mantissa
      representation. */
    load_hex_fxp_value(&old_constant->variant.fixed_point_value,
                       fxp_descr, &mantissa, &exponent, &is_negative);
    /* Convert and store the mantissa as a floating-point value. */
    conv_mantissa_to_floating_point(&mantissa, &exponent, is_negative,
                                    float_tp->variant.float_kind,
                                    float_value,
                                    /*overflow=*/FALSE, &err, &inexact);
    /* No diagnostic is given for an inexact result. */
    if (err) {
      /* The conversion to floating-point does not fit in the result type.
         This should not occur unless there are fixed-point or floating-point
         types with unusual sizes. */
      *err_code = ec_fixed_to_float_conversion;
      *err_severity = es_error;
    }  /* if */
  }  /* if */
}  /* conv_fixed_point_to_float */


void conv_fixed_point_to_fixed_point(
				a_constant_ptr		old_constant,
				a_constant_ptr		new_constant,
				an_error_code		*err_code,
				an_error_severity	*err_severity)
/*
Convert the fixed-point constant "old_constant" to a fixed-point constant
in "new_constant.  If, as a result of the conversion, a diagnostic should
be issued, set err_code and err_severity to the values for the message
to be issued; otherwise set err_code to ec_no_error.
*/
{
  a_mantissa	mantissa;
  long		exponent;
  a_boolean	is_negative;
  a_boolean	err;
  a_boolean	inexact;
  a_fixed_point_type_descr
		*old_fxp_descr;
  a_fixed_point_type_descr
		*new_fxp_descr;

  check_assertion(old_constant->kind == (a_constant_repr_kind)ck_fixed_point);
  set_constant_kind(new_constant, (a_constant_repr_kind)ck_fixed_point);
  *err_code = ec_no_error;
  old_fxp_descr = fxp_descr_for_constant(old_constant);
  new_fxp_descr = fxp_descr_for_constant(new_constant);
  /* Convert the fixed-point value into the internal mantissa
     representation. */
  load_hex_fxp_value(&old_constant->variant.fixed_point_value,
                     old_fxp_descr, &mantissa, &exponent, &is_negative);
  /* Convert and store the mantissa as a fixed-point value. */
  conv_mantissa_to_fixed_point(&mantissa, exponent, is_negative,
                               new_fxp_descr, /*overflow=*/FALSE,
                               &new_constant->variant.fixed_point_value,
                               &err, &inexact);
  if (err) {
    /* The conversion to fixed-point does not fit in the result type. */
    *err_code = ec_fixed_to_fixed_conversion;
    *err_severity = es_error;
  } else if (is_negative && new_fxp_descr->is_unsigned) {
    /* The conversion results in a negative value being converted to
       unsigned. */
    *err_code = ec_fixed_sign_change;
    *err_severity = es_error;
  }  /* if */
}  /* conv_fixed_point_to_fixed_point */


void fxp_hex_string_to_fixed_point(a_fixed_point_type_descr  *fxp_descr,
                                   a_const_char              *str,
                                   a_fixed_point_value       *value,
                                   a_boolean                 *err,
                                   a_boolean                 *inexact)
/*
Convert the hexadecimal fixed-point number in the null-terminated string str
to internal form in *value.  The number is known to be syntactically correct,
but may not be representable (it may be too large or too small); if there's
an error, return *err = TRUE.  The specific fixed-point kind is indicated by
*fxp_descr (an will typically affect the representation in *value).
*inexact is set to TRUE if *value does not exactly represent the value
indicated by the given string.  Otherwise, it is set to FALSE.
*/
{
  a_boolean	overflow = FALSE;
  long		exponent = 0;
  a_mantissa	mantissa;

  conv_hex_string_to_mantissa_and_exponent(str, &mantissa, &exponent,
                                           &overflow);
  conv_mantissa_to_fixed_point(&mantissa, exponent, /*is_negative=*/FALSE,
                               fxp_descr, overflow, value, err, inexact);
}  /* fxp_hex_string_to_fixed_point */


void fxp_string_to_fixed_point(a_fixed_point_type_descr  *fxp_descr,
                               a_const_char              *str,
                               a_fixed_point_value       *value,
                               a_boolean                 *err)
/*
Convert the decimal fixed-point number in the null-terminated string str to
internal form in *value.  The number is known to be syntactically correct,
but may not be representable (it may be too large or too small); if there's
an error, return *err = TRUE.  The specific fixed-point kind is indicated by
*fxp_descr (and will typically affect the representation in *value).
The string need not have a decimal point or exponent (it can look like an
integer).

This implementation is for demonstration purposes only: It is known to be
imprecise.  Specifically, this implementation scans the string as a floating-
point value.  It relies on a_fixed_point_value being identical to
an_integer_value.
*/
{
  an_internal_float_value
		fp_value;
  a_boolean	inexact;
  long		exponent;
  a_boolean	is_negative;
  a_mantissa	mantissa;

  *err = FALSE;
  /* Convert the given string to a floating point value. */
  fp_string_to_float((a_float_kind)fk_long_double, str, &fp_value, err);
  if (!*err) {
    /* Convert the floating-point value into a fixed-point. */
    load_hex_fp_value(&fp_value,
                      (a_float_kind)fk_long_double,
                      &mantissa, &exponent, &is_negative,
                      /*restore_implicit_bit=*/TRUE);
    /* Convert and store the mantissa as a fixed-point value. */
    conv_mantissa_to_fixed_point(&mantissa, exponent, is_negative,
                                 fxp_descr,
                                 /*overflow=*/FALSE,
                                 value, err, &inexact);
  }  /* if */
}  /* fxp_string_to_fixed_point */

#endif /* !STANDALONE_UTILITY_PROGRAM */

static void conv_integer_value_to_long_double_value(
                                           an_integer_value         *ival,
                                           a_boolean                is_signed,
                                           an_internal_float_value  *fval,
                                           a_boolean                *err)
/*
Convert the integer value in *ival to a floating-point value (of type long
double) in *fval.  Set *err to TRUE if this does not work.

(In some cases, the conversion may be done through a conversion to string
representation: It may not be exact.  This routine is used for processing
the representation of fixed-point values as implicitly scaled integer values.)
*/
{
  conv_integer_value_to_float(ival, is_signed, fval,
                              (a_float_kind)fk_long_double, err);
}  /* conv_integer_value_to_long_double_value */


static void construct_fxp_scale_factor(a_fixed_point_type_descr  *fxp_descr,
                                       an_internal_float_value   *fp_scale)
/*
Construct a long double scaling factor 2^F where F is the number of fractional
bits in the fixed-point type represented by fxp_descr.  Place the result in
fp_scale.
*/
{
  int               fract_bits;
  an_integer_value  int_scale;  
  a_boolean         err = FALSE;

  fract_bits = targ_fractional_bits_for_fixed_point[fxp_descr->is_unsigned]
                                                   [(int)fxp_descr->precision]
                                                   [fxp_descr->is_fract_type];
  /* Shift the value "1" fract_bits to the left and convert the result to
     type "long double" using a string as an intermediate representation. */
  set_integer_value(&int_scale, (a_host_large_integer)1);
  shift_left_integer_value(&int_scale, fract_bits, &err);
  check_assertion(!err);
  conv_integer_value_to_long_double_value(&int_scale, /*is_signed=*/FALSE,
                                          fp_scale, &err);
  check_assertion(!err);
}  /* construct_fxp_scale_factor */


a_number_buffer fxp_to_string(a_fixed_point_type_descr  *fxp_descr,
                              a_fixed_point_value       *value)
/*
Return a string representation of the given value as informed by the given
accompanying fixed-point type description.  Suffixes are appended as needed.

(This implementation assumes a_fixed_point_value is a synonym for
an_integer_value and may produce slightly inaccurate results.)
*/
{
  a_number_buffer
              result_str;
  an_internal_float_value
              fp_scale, fp_value, fp_scaled_value;
  a_boolean   depends_on_fp_mode;
  a_boolean   pos_infinity, neg_infinity, not_a_number;
  a_boolean   err = FALSE;

  construct_fxp_scale_factor(fxp_descr, &fp_scale);
  /* Treat the given fixed-point as an integer (not yet scaled) and convert
     is to type "long double". */
  conv_integer_value_to_long_double_value(value, !fxp_descr->is_unsigned,
                                          &fp_value, &err);
  check_assertion(!err);
  /* Scale the result and convert it to a string. */
  fp_divide((a_float_kind)fk_long_double,
            &fp_value, &fp_scale, &fp_scaled_value,
            &err, &depends_on_fp_mode);
  result_str = fp_to_string((a_float_kind)fk_long_double, &fp_scaled_value,
                            &pos_infinity, &neg_infinity, &not_a_number);
  check_assertion(!(pos_infinity || neg_infinity || not_a_number));
  /* Add all the needed suffixes. */
  if (fxp_descr->is_unsigned) {
    result_str.append("u");
  }  /* if */
  if (fxp_descr->precision == fpp_short) {
    result_str.append("h");
  } else if (fxp_descr->precision == fpp_long) {
    result_str.append("l");
  }  /* if */
  if (fxp_descr->is_fract_type) {
    result_str.append("r");
  } else {
    result_str.append("k");
  }  /* if */
  return result_str;
#undef BUF_LENGTH
}  /* fxp_to_string */

#if !STANDALONE_UTILITY_PROGRAM

static void conv_fixed_point_to_long_double(
				a_fixed_point_value		*fxp_value,
				a_fixed_point_type_descr	*fxp_descr,
				an_internal_float_value		*fp_value)
/*
Convert the fixed-point fxp_value to a long double, stored into fp_value.
fxp_descr specifies the format of the fixed-point value.
*/
{
  a_mantissa mantissa;
  long       exponent;
  a_boolean  is_negative;
  a_boolean  err;
  a_boolean  inexact;

  load_hex_fxp_value(fxp_value, fxp_descr, &mantissa, &exponent, &is_negative);
  conv_mantissa_to_floating_point(&mantissa, &exponent, is_negative,
                                  (a_float_kind)fk_long_double, fp_value,
                                  /*overflow=*/FALSE, &err, &inexact);
}  /* conv_fixed_point_to_long_double */




static void conv_long_double_to_fixed_point(
				an_internal_float_value		*fp_value,
				a_fixed_point_value		*fxp_value,
				a_fixed_point_type_descr	*fxp_descr,
				a_boolean			*err)
/*
Convert the long double fp_value to fixed-point and store it in fxp_value.
fxp_descr specifies the format of the fixed-point value.  Set err if
the value cannot be converted to the destination type.
*/
{
  a_mantissa	mantissa;
  long		exponent;
  a_boolean	is_negative;
  a_boolean	inexact;

  *err = FALSE;
  load_hex_fp_value(fp_value, (a_float_kind)fk_long_double, &mantissa,
                    &exponent, &is_negative, /*restore_implicit_bit=*/TRUE);
  conv_mantissa_to_fixed_point(&mantissa, exponent, is_negative, fxp_descr,
                               /*overflow=*/FALSE, fxp_value, err, &inexact);
#if TARG_HAS_IEEE_FLOATING_POINT
  if (fp_is_nan_or_infinity(fp_value, (a_float_kind)fk_long_double)) {
    /* Not-a-number or infinity.  Treat this as an error. */
    *err = TRUE;
  }  /* if */
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
}  /* conv_long_double_to_fixed_point */


void fxp_negate(a_fixed_point_value      *value,
                a_fixed_point_type_descr *fxp_descr,
                a_fixed_point_value      *result,
                a_fixed_point_type_descr *fxp_descr_result,
                a_boolean                *err)
/*
Negate the fixed-point value and to store the value in result.  fxp_descr
and fxp_descr_result describe the format of the fixed-point values of value,
and result.  If an error occurs (e.g., overflow), err is set to TRUE.
*/
{
  an_internal_float_value	fp;
  an_internal_float_value	fp_result;
  a_boolean			depends_on_fp_mode;
  a_boolean			conv_err;

  *err = FALSE;
  conv_fixed_point_to_long_double(value, fxp_descr, &fp);
  fp_negate((a_float_kind)fk_long_double, &fp, &fp_result, err,
           &depends_on_fp_mode);
  conv_long_double_to_fixed_point(&fp_result, result, fxp_descr_result,
                                  &conv_err);
  if (conv_err) *err = TRUE;
}  /* fxp_negate */


static a_boolean conv_constant_to_long_double(a_constant_ptr          cp,
					      an_internal_float_value *result)
/*
Convert the constant "cp" to a long double value in "result".  Return TRUE
if the conversion was successful.  The conversion is only attempted if
the constant is a fixed-point or integer value.
*/
{
  a_boolean	err;
  a_boolean	conversion_done = TRUE;

  if (cp->kind == (a_constant_repr_kind)ck_fixed_point) {
    conv_fixed_point_to_long_double(&cp->variant.fixed_point_value,
                                    fxp_descr_for_constant(cp), result);
  } else if (cp->kind == (a_constant_repr_kind)ck_integer) {
    conv_integer_value_to_long_double_value(&cp->variant.integer_value,
                                            int_constant_is_signed(cp),
                                            result, &err);
    check_assertion(!err);
  } else {
    conversion_done = FALSE;
  }  /* if */
  return conversion_done;
}  /* conv_constant_to_long_double */


void fxp_shift(a_constant		*constant,
	       int			shift_count,
	       a_constant		*result,
	       a_boolean		shift_right,
	       a_boolean		*err)
/*
Shift a fixed-point constant by shift_count bits, producing result.  Do
a right shift if shift_right is TRUE, left otherwise.  If an error occurs
(such as overflow) set err.
*/
{
  a_mantissa		mantissa;
  long			exponent;
  a_boolean		is_negative;
  a_boolean		local_err;
  a_boolean		inexact;
  a_fixed_point_type_descr
			*fxp_descr;
  a_fixed_point_type_descr
			*result_fxp_descr;

  *err = FALSE;
  check_assertion(constant->kind == (a_constant_repr_kind)ck_fixed_point);
  set_constant_kind(result, (a_constant_repr_kind)ck_fixed_point);
  fxp_descr = fxp_descr_for_constant(constant);
  result_fxp_descr = fxp_descr_for_constant(result);
  /* Convert the fixed-point value into the internal mantissa
     representation. */
  load_hex_fxp_value(&constant->variant.fixed_point_value,
                     fxp_descr, &mantissa, &exponent, &is_negative);
  /* Increment or decrement the exponent by the shift count. */
  if (shift_right) exponent -= shift_count; else exponent += shift_count;
  /* Convert and store the mantissa as a fixed-point value. */
  conv_mantissa_to_fixed_point(&mantissa, exponent, is_negative,
                               result_fxp_descr, /*overflow=*/FALSE,
                               &result->variant.fixed_point_value,
                               &local_err, &inexact);
  if (local_err) {
    /* If an error (overflow) occurred, indicate that the operation was not
       folded. */
    *err = TRUE;
  }  /* if */
}  /* fxp_shift */


void fxp_add(a_constant		*constant_1,
	     a_constant		*constant_2,
	     a_constant		*result,
	     a_boolean		*did_not_fold,
	     a_boolean		*err)
/*
Add the constants value_1 and value_2 and store the value in
result.  One or both of the constants are fixed-point values.
One of the operands can be an integer.  The result is a fixed-point
constant.  If the operation cannot be folded, did_not_fold is set to TRUE.
If an error occurs (e.g., overflow), err is set to TRUE.
*/
{
  an_internal_float_value	fp_1;
  an_internal_float_value	fp_2;
  an_internal_float_value	fp_result;
  a_boolean			depends_on_fp_mode;
  a_boolean			conv_err;

  *err = FALSE;
  check_assertion(result->kind == (a_constant_repr_kind)ck_fixed_point);
  /* Convert the operands to long double.  If either of the operands is
     of a type that can't be converted, the folding won't be done. */
  if (conv_constant_to_long_double(constant_1, &fp_1) &&
      conv_constant_to_long_double(constant_2, &fp_2)) {
    fp_add((a_float_kind)fk_long_double, &fp_1, &fp_2, &fp_result, err,
           &depends_on_fp_mode);
    conv_long_double_to_fixed_point(&fp_result,
                                    &result->variant.fixed_point_value,
                                    fxp_descr_for_constant(result), &conv_err);
    if (conv_err) *err = TRUE;
  } else {
    *did_not_fold = TRUE;
  }  /* if */
}  /* fxp_add */


void fxp_subtract(a_constant	*constant_1,
		  a_constant	*constant_2,
		  a_constant	*result,
		  a_boolean	*did_not_fold,
		  a_boolean	*err)
/*
Subtract the constants value_1 and value_2 and store the value in
result.  One or both of the constants are fixed-point values.
One of the operands can be an integer.  The result is a fixed-point
constant.  If the operation cannot be folded, did_not_fold is set to TRUE.
If an error occurs (e.g., overflow), err is set to TRUE.
*/
{
  an_internal_float_value	fp_1;
  an_internal_float_value	fp_2;
  an_internal_float_value	fp_result;
  a_boolean			depends_on_fp_mode;
  a_boolean			conv_err;

  *err = FALSE;
  check_assertion(result->kind == (a_constant_repr_kind)ck_fixed_point);
  /* Convert the operands to long double.  If either of the operands is
     of a type that can't be converted, the folding won't be done. */
  if (conv_constant_to_long_double(constant_1, &fp_1) &&
      conv_constant_to_long_double(constant_2, &fp_2)) {
    fp_subtract((a_float_kind)fk_long_double, &fp_1, &fp_2, &fp_result, err,
                &depends_on_fp_mode);
    conv_long_double_to_fixed_point(&fp_result,
                                    &result->variant.fixed_point_value,
                                    fxp_descr_for_constant(result), &conv_err);
    if (conv_err) *err = TRUE;
  } else {
    *did_not_fold = TRUE;
  }  /* if */
}  /* fxp_subtract */


void fxp_multiply(a_constant	*constant_1,
		  a_constant	*constant_2,
		  a_constant	*result,
		  a_boolean	*did_not_fold,
		  a_boolean	*err)
/*
Multiply the constants value_1 and value_2 and store the value in
result.  One or both of the constants are fixed-point values.
One of the operands can be an integer.  The result is a fixed-point
constant.  If the operation cannot be folded, did_not_fold is set to TRUE.
If an error occurs (e.g., overflow), err is set to TRUE.
*/
{
  an_internal_float_value	fp_1;
  an_internal_float_value	fp_2;
  an_internal_float_value	fp_result;
  a_boolean			depends_on_fp_mode;
  a_boolean			conv_err;

  *err = FALSE;
  check_assertion(result->kind == (a_constant_repr_kind)ck_fixed_point);
  /* Convert the operands to long double.  If either of the operands is
     of a type that can't be converted, the folding won't be done. */
  if (conv_constant_to_long_double(constant_1, &fp_1) &&
      conv_constant_to_long_double(constant_2, &fp_2)) {
    fp_multiply((a_float_kind)fk_long_double, &fp_1, &fp_2, &fp_result, err,
                &depends_on_fp_mode);
    conv_long_double_to_fixed_point(&fp_result,
                                    &result->variant.fixed_point_value,
                                    fxp_descr_for_constant(result), &conv_err);
    if (conv_err) *err = TRUE;
  } else {
    *did_not_fold = TRUE;
  }  /* if */
}  /* fxp_multiply */


void fxp_divide(a_constant	*constant_1,
		a_constant	*constant_2,
		a_constant	*result,
		a_boolean	*did_not_fold,
		a_boolean	*err)
/*
Divide the constants value_1 and value_2 and store the value in
result.  One or both of the constants are fixed-point values.
One of the operands can be an integer.  The result is a fixed-point
constant.  If the operation cannot be folded, did_not_fold is set to TRUE.
If an error occurs (e.g., overflow), err is set to TRUE.
*/
{
  an_internal_float_value	fp_1;
  an_internal_float_value	fp_2;
  an_internal_float_value	fp_result;
  a_boolean			depends_on_fp_mode;
  a_boolean			conv_err;

  *err = FALSE;
  check_assertion(result->kind == (a_constant_repr_kind)ck_fixed_point);
  /* Convert the operands to long double.  If either of the operands is
     of a type that can't be converted, the folding won't be done. */
  if (conv_constant_to_long_double(constant_1, &fp_1) &&
      conv_constant_to_long_double(constant_2, &fp_2)) {
    fp_divide((a_float_kind)fk_long_double, &fp_1, &fp_2, &fp_result, err,
                &depends_on_fp_mode);
    /* Don't store the result if an error (e.g., divide by zero) occurred. */
    if (!*err) {
      conv_long_double_to_fixed_point(&fp_result,
                                    &result->variant.fixed_point_value,
                                    fxp_descr_for_constant(result), &conv_err);
      if (conv_err) *err = TRUE;
    }  /* if */
  } else {
    *did_not_fold = TRUE;
  }  /* if */
}  /* fxp_divide */


int fxp_compare(a_constant	*constant_1,
		a_constant	*constant_2)
/*
Compare two fixed-point values and return

       value_1 > value_2   1
       value_1 = value_2   0
       value_1 < value_2  -1

*/
{
  an_internal_float_value	fp_1;
  an_internal_float_value	fp_2;
  a_boolean			unord;
  int				result = 0;

  /* Convert the operands to long double. */
  if (conv_constant_to_long_double(constant_1, &fp_1) &&
      conv_constant_to_long_double(constant_2, &fp_2)) {
    result = fp_compare((a_float_kind)fk_long_double, &fp_1, &fp_2, &unord);
    check_assertion(!unord);
  } else {
    unexpected_condition();
  }  /* if */
  return result;
}  /* fxp_compare */


a_hash_value fxp_hash(a_fixed_point_value  *value)
/*
Return a hash value derived from the given fixed-point value.  This is used
in building the hash table for shareable constants.  (This implementation
assumes a_fixed_point_value is a synonym for an_integer_value.)
*/
{
  a_boolean   ovflo;

  return (a_hash_value)
                     value_of_integer_value(value, /*is_signed=*/TRUE, &ovflo);
}  /* fxp_hash */

#endif /* !STANDALONE_UTILITY_PROGRAM */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* FIXED_POINT_ALLOWED */

