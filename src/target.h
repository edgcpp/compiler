/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

target.h -- Declaration of variables specifying target machine characteristics.

Note that target.h declares configuration variables that, in principle, can
be reset whenever the compiler is invoked, whereas targ_def.h contains
configuration values that are incorporated when the compiler is built
(#define values, typedefs).

*/

/* Avoid including these declarations more than once: */
#ifndef TARGET_H
#define TARGET_H 1

#ifndef TARG_DEF_H
#include "targ_def.h"
#endif /* ifndef TARG_DEF_H */
#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
A flag that can mean that no target configuration was found or no target
configuration was specified.
*/
#define NO_TARGET_CONFIG (-1)

EXTERN_THREAD int32_t
                target_configuration_index;
                        /* Gives the index (into target_configurations[]) of
                           the target configuration that is being used
                           (either selected by the --target command-line option
                           or read in from the il_header).  Is NO_TARGET_CONFIG
                           if no --target command is specified and no default
                           target is specified. */

/*
Except as noted, the following variables are initialized to values defined
for the expected target machine but may be reset to permit reconfiguring
the EDG front end to different targets with each invocation.
*/
EXTERN_THREAD a_boolean
		targ_little_endian;
			/* When TRUE the least significant part of a multi-byte
			   target integer is at the lowest memory address. */

/*
Char types:
*/
EXTERN_THREAD unsigned int
		targ_char_bit;
			/* Number of bits in a target char. */

EXTERN_THREAD unsigned int
		targ_host_string_char_bit;
			/* The number of data bits per character used when
			   representing target characters as a string on the
			   host; dependent on targ_char_bit and CHAR_BIT.
			   One is allowed to make the target char larger than
			   the host char, but individual characters in string
			   literals will be limited by what is representable
			   in a host char. */

EXTERN_THREAD a_boolean
		targ_has_signed_chars;
			/* TRUE if the target has signed characters.  This
			   is selectable on the command line. */

EXTERN_THREAD a_boolean
		targ_char_constant_first_char_most_significant;
			/* TRUE when the first character in a target char
			   constant is most significant -- e.g., 'ab' == 0x6162
			   instead of 0x6261. */

EXTERN_THREAD an_integer_kind
		targ_wchar_t_int_kind;
			/* Integer kind associated with wchar_t.  Initialized
			   to the default value but reconfigurable. */

EXTERN_THREAD a_targ_size_t
		targ_sizeof_wchar_t;
			/* Size of a wchar_t entity.  Initialized to the
			   default value but reconfigurable. */

EXTERN_THREAD an_integer_kind
		targ_wint_t_int_kind;
			/* Integer kind associated with wint_t.  Initialized
			   to the default value but reconfigurable. */

EXTERN_THREAD an_integer_kind
		targ_char16_t_int_kind;
			/* Integer kind associated with char16_t.  Initialized
			   to the default value but reconfigurable. */
EXTERN_THREAD a_targ_size_t
		targ_sizeof_char16_t;
			/* Size of a char16_t entity.  Initialized to the
			   default value but reconfigurable. */

EXTERN_THREAD an_integer_kind
		targ_char32_t_int_kind;
			/* Integer kind associated with char32_t.  Initialized
			   to the default value but reconfigurable. */
EXTERN_THREAD a_targ_size_t
		targ_sizeof_char32_t;
			/* Size of a char32_t entity.  Initialized to the
			   default value but reconfigurable. */

EXTERN_THREAD a_targ_size_t
		character_size[(int)chk_last];
			/* A table of sizes for the various character kinds. */

EXTERN_THREAD an_integer_kind
		targ_bool_int_kind;
			/* Integer kind associated with bool in C++ mode. */
EXTERN_THREAD an_integer_kind
		targ_c_bool_int_kind;
			/* Integer kind associated with bool in C mode. */

/*
Macro to return the value of targ_c_bool_int_kind or targ_bool_int_kind
as appropriate depending on the mode.
*/
#define BOOL_INT_KIND (C_mode() ? targ_c_bool_int_kind : targ_bool_int_kind)

/*
Integer types:
*/
EXTERN_THREAD a_targ_size_t
		targ_sizeof_short;
			/* Size of a short int.  Initialized to the default
			   value but reconfigurable. */

EXTERN_THREAD a_targ_alignment
		targ_alignof_short;
			/* Alignment of a short int.  Initialized to the
			   default value but reconfigurable. */

EXTERN_THREAD a_targ_size_t
		targ_sizeof_int;
			/* Size of an int.  Initialized to the default value
			   but reconfigurable. */

EXTERN_THREAD a_targ_alignment
		targ_alignof_int;
			/* Alignment of an int.  Initialized to the default
			   value but reconfigurable. */

EXTERN_THREAD a_targ_size_t
		targ_sizeof_long;
			/* Size of a long int.  Initialized to the default
			   value but reconfigurable. */

EXTERN_THREAD a_targ_alignment
		targ_alignof_long;
			/* Alignment of a long int.  Initialized to the
			   default value but reconfigurable. */

#if LONG_LONG_ALLOWED
EXTERN_THREAD a_targ_size_t
		targ_sizeof_long_long;
			/* Size of a long long int.  Initialized to the default
			   value but reconfigurable. */

EXTERN_THREAD a_targ_alignment
		targ_alignof_long_long;
			/* Alignment of a long long int.  Initialized to the
			   default value but reconfigurable. */
#endif /* LONG_LONG_ALLOWED */
#if INT128_EXTENSIONS_ALLOWED
EXTERN_THREAD a_targ_size_t
		targ_sizeof_int128;
			/* Size of a 128-bit integer type.  Initialized to the
			   default value but reconfigurable (in practice, this
			   almost certainly equals 16). */

EXTERN_THREAD a_targ_alignment
		targ_alignof_int128;
			/* Alignment of a 128-bit integer type.  Initialized to
			   the default value but reconfigurable (in practice,
			   this almost certainly equals 16). */
#endif /* INT128_EXTENSIONS_ALLOWED */

EXTERN_THREAD a_targ_size_t
		bitint_maxwidth_value;
			/* The maximum width accepted for _BitInt. */

EXTERN_THREAD a_targ_size_t
		targ_sizeof_largest_integer;
			/* Size of the longest integer in the configuration.
			   Must be no larger than MAX_SIZEOF_LARGEST_INTEGER.*/

#if GNU_EXTENSIONS_ALLOWED
EXTERN_THREAD a_targ_size_t
		targ_sizeof_largest_atomic;
			/* Size of the largest atomic type.  Initialized to the
			   default value but reconfigurable. */
#endif /* GNU_EXTENSIONS_ALLOWED */

#if MICROSOFT_EXTENSIONS_ALLOWED
EXTERN_THREAD a_boolean
		is_64bit_target;
			/* TRUE if the target platform is a 64-bit target;
			   i.e., TRUE if size_t is 64 bits wide. */

EXTERN_THREAD an_integer_kind
		targ_int8_int_kind;
			/* Integer kind associated with __int8.  Initialized
			   to ik_none and reset later. */

EXTERN_THREAD an_integer_kind
		targ_unsigned_int8_int_kind;
			/* Integer kind associated with unsigned __int8.
			   Initialized to ik_none and reset later. */

EXTERN_THREAD an_integer_kind
		targ_int16_int_kind;
			/* Integer kind associated with __int16.  Initialized
			   to ik_none and reset later. */

EXTERN_THREAD an_integer_kind
		targ_unsigned_int16_int_kind;
			/* Integer kind associated with unsigned __int16.
			   Initialized to ik_none and reset later. */

EXTERN_THREAD an_integer_kind
		targ_int32_int_kind;
			/* Integer kind associated with __int32.  Initialized
			   to ik_none and reset later. */

EXTERN_THREAD an_integer_kind
		targ_unsigned_int32_int_kind;
			/* Integer kind associated with unsigned __int32.
			   Initialized to ik_none and reset later. */

EXTERN_THREAD an_integer_kind
		targ_int64_int_kind;
			/* Integer kind associated with __int64.  Initialized
			   to ik_none and reset later. */

EXTERN_THREAD an_integer_kind
		targ_unsigned_int64_int_kind;
			/* Integer kind associated with unsigned __int64.
			   Initialized to ik_none and reset later. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

EXTERN_THREAD an_integer_kind
		targ_intmax_kind;
			/* Integer kind associated with the largest signed
			   integer type (excluding ik_int128).  In C99, this
			   is intmax_t. */

EXTERN_THREAD an_integer_kind
		targ_uintmax_kind;
			/* Integer kind associated with the largest unsigned
			   integer type (excluding ik_unsigned_int128).  In
			   C99, this is uintmax_t. */

EXTERN_THREAD a_targ_size_t
		targ_max_class_object_size;
			/* Maximum size of a class object.  Initialized to the
			   default value but may be reset in target_init. */

EXTERN_THREAD a_targ_size_t
		targ_max_base_class_offset;
			/* Maximum offset of a base class.  Initialized to the
			   default value but may be reset in target_init. */

EXTERN_THREAD a_boolean
		targ_optimize_empty_base_class_layout;
			/* TRUE if the layout mechanism should attempt to
			   allocate empty base classes at the same offset as
			   other subobjects. */

EXTERN_THREAD int
		targ_bit_field_container_size;
			/* Container size to be used for bit-fields.  If > 0,
			   indicates the size in bytes of one of the integral
			   types.  0 means "use the smallest integral type
			   into which the field will fit".  < 0 means "use the
			   base type given in the declaration".  Initialized
			   to the default value but reconfigurable. */

EXTERN_THREAD a_boolean
		targ_microsoft_bit_field_allocation;
			/* If this flag is TRUE, bit-field allocation follows
			   the conventions of Microsoft C/C++.  The value of
			   targ_bit_field_container_size must be -1 and there
			   is a two-stage allocation: first, a bit-field
			   container based on the bit-field type is allocated
			   (as though it were a field in its own right), and
			   then bit fields are allocated within it.  When the
			   bit-field type changes or the container fills up,
			   a new container is allocated. */

EXTERN_THREAD a_boolean
		targ_plain_int_bit_field_is_unsigned;
			/* TRUE when a "plain" int bit field is to be treated
			   as unsigned.  Initialized to the default value but
			   reconfigurable. */

EXTERN_THREAD a_boolean
		targ_force_one_bit_bit_field_to_be_unsigned;
			/* TRUE when a "plain" int bit field of length 1 is
			   to be treated as unsigned regardless of the
			   setting of targ_plain_int_bit_field_is_unsigned
			   (because a bit field consisting of only a sign
			   is not very useful). */

EXTERN_THREAD a_boolean
		targ_enum_bit_fields_are_always_unsigned;
			/* Signedness for enum bit fields (an extension in C):
			   if TRUE, enum bit fields are always unsigned.  If
			   FALSE, the rules are as described in target.h: it
			   depends on the signedness and size of the values of
			   the enum and defaults to the signedness indicated
			   by targ_nonnegative_enum_bit_field_is_unsigned.
			   This needs to be FALSE to allow fully-standard
			   C++. */

EXTERN_THREAD a_boolean
		targ_nonnegative_enum_bit_field_is_unsigned;
			/* Signedness for enum bit fields whose enum types have
			   enumerators that could all fit in the nonnegative
			   range of the bit field if it were signed.  (Ignored
			   if targ_enum_bit_fields_are_always_unsigned is
			   TRUE.) */

EXTERN_THREAD int
		targ_zero_width_bit_field_alignment;
			/* Alignment adjustment to be made when a zero-width
			   (unnamed) bit field is declared.  If > 0 it is the
			   alignment to be used (typically the alignment of
			   one of the integral types, in which case the value
			   should be cast to a_targ_alignment).  A value of
			   zero means "use the minimal alignment", which is
			   single-byte alignment.  Any value less than zero
			   means "use the alignment of the base type given in
			   the declaration". */

EXTERN_THREAD int
		targ_zero_width_bit_field_affects_struct_alignment;
			/* TRUE when the alignment adjustment when a
			   zero-width (unnamed) bit-field is declared
			   affects the overall alignment of the struct as
			   well as the alignment of the next field. */

EXTERN_THREAD int
		targ_unnamed_bit_field_affects_struct_alignment;
			/* TRUE if the alignment adjustment when an unnamed
			   bit-field is declared affects the overall alignment
			   of the struct as well as the alignment of the next
			   field. */

EXTERN_THREAD a_boolean
		targ_bit_field_affects_union_alignment;
			/* TRUE if a bit field in a union type affects its
			   alignment. */

EXTERN_THREAD int
		targ_user_control_of_struct_packing_affects_bit_fields;
			/* TRUE if "#pragma pack(n)" and the command-line
			   option "--pack_alignment=n" affect the alignment of
			   bit field containers (when bit fields straddle
			   container alignment boundaries). */

EXTERN_THREAD int
		targ_pad_bit_fields_larger_than_base_type;
			/* TRUE if bit fields longer than their base types are
			   padded out to the full declared length.  FALSE
			   means allocate only as many bits as are in the
			   base type.  In either case, the bit field itself
			   has the same number of bits; the extra bits are
			   padding bits. */

EXTERN_THREAD a_boolean
                targ_supports_x86_64;
                        /* TRUE if the target supports the 64-bit version of
                           the x86 instruction set. */
EXTERN_THREAD a_boolean
                targ_supports_arm64;
                        /* TRUE if the target supports the 64-bit version of
                           the ARM instruction set. */

EXTERN_THREAD a_boolean
                targ_supports_arm32;
                        /* TRUE if the target supports the 32-bit version of
                           the ARM instruction set. */

EXTERN_THREAD a_boolean
                targ_supports_riscv64;
                        /* TRUE if the target supports the 64-bit version of
                           the RISC-V instruction set. */

EXTERN_THREAD a_boolean
                targ_supports_riscv32;
                        /* TRUE if the target supports the 32-bit version of
                           the RISC-V instruction set. */

inline a_boolean target_is_64_bits(void)
/*
Utility to return TRUE if the target has a 64-bit architecture.
*/
{
  return targ_supports_x86_64 || targ_supports_arm64 || targ_supports_riscv64;
}  /* target_is_64_bits */


inline a_boolean target_is_arm_based(void)
/*
Utility to return TRUE if the target is based on an ARM architecture.
*/
{
  return targ_supports_arm32 || targ_supports_arm64;
}  /* target_is_arm_based */


inline a_boolean target_is_riscv_based(void)
/*
Utility to return TRUE if the target is based on a RISC-V architecture.
*/
{
  return targ_supports_riscv32 || targ_supports_riscv64;
}  /* target_is_riscv_based */


inline a_boolean target_is_x86_based(void)
/*
Utility to return TRUE if the target is based on an x86 architecture.
Currently this is the default (but that may change in the future).
*/
{
  return !target_is_arm_based() && !target_is_riscv_based();
}  /* target_is_x86_based */


inline a_boolean target_is_x86_compatible(void)
/*
Utility to return TRUE if the target is based on an x86 architecture or is
emulation compatible with x86.
*/
{
  /* Note this logic is incomplete as it does not handle the ARM64EC ABI (i.e.,
     ARM64 code that's been compiled to allow emulated x86_64 code to call into
     it).  As the front end does not have an equivalent mode for ARM64EC at the
     time of writing, this function currently only returns true when the target
     is x86 based. */
  return target_is_x86_based();
}  /* target_is_x86_compatible */


inline a_boolean target_is_32_bit_x86_based(void)
/*
Utility to return TRUE if the target is based on a 32-bit x86 architecture.
Currently this is the default (but that may change in the future).
*/
{
  return target_is_x86_based() && !target_is_64_bits();
}  /* target_is_32_bit_x86_based */


EXTERN_THREAD a_boolean
                packing_applies_to_base_classes;
                        /* TRUE if "#pragma pack(n)" affects the alignment of
                           base classes (in addition to that of proper
                           fields). */

/*
Pointer types:
*/

EXTERN_THREAD a_boolean
                targ_all_pointers_same_size;
                        /* TRUE if all pointers have the same size (see the
                           caveats for TARG_ALL_POINTERS_SAME_SIZE). */

EXTERN_THREAD a_targ_size_t
		targ_sizeof_pointer;
			/* Size of a pointer.  Initialized to the default
			   value but reconfigurable.  Meaningless when
			   targ_all_pointers_same_size is FALSE. */

EXTERN_THREAD a_targ_alignment
		targ_alignof_pointer;
			/* Alignment of a pointer.  Initialized to the default
			   value but reconfigurable.  Meaningless when
			   targ_all_pointers_same_size is FALSE. */

#if NEAR_AND_FAR_ALLOWED
EXTERN_THREAD a_targ_size_t
		targ_sizeof_far_pointer;
			/* Size of a far pointer.  Initialized to the default
			   value but reconfigurable.  Used only when support
			   for near and far is enabled (e.g., in 16-bit
			   Microsoft mode). */

EXTERN_THREAD a_targ_alignment
		targ_alignof_far_pointer;
			/* Alignment of a far pointer.  Initialized to the
			   default value but reconfigurable.  Used only when
			   support for near and far is enabled (e.g., in
			   16-bit Microsoft mode). */
EXTERN_THREAD a_targ_size_t
		targ_sizeof_near_pointer;
			/* Size of a near pointer.  Initialized to the default
			   value but reconfigurable.  Used only when support
			   for near and far is enabled (e.g., in 16-bit
			   Microsoft mode). */

EXTERN_THREAD a_targ_alignment
		targ_alignof_near_pointer;
			/* Alignment of a near pointer.  Initialized to the
			   default value but reconfigurable.  Used only when
			   support for near and far is enabled (e.g., in
			   16-bit Microsoft mode). */
#endif /* NEAR_AND_FAR_ALLOWED */

EXTERN_THREAD an_integer_kind
		targ_ptrdiff_t_int_kind;
			/* Representation for ptrdiff_t -- the integer kind
			   large enough to hold a pointer value.  Initialized
			   to the default value but reconfigurable. */

EXTERN_THREAD a_targ_size_t
		targ_size_t_max;
			/* The limit of the host representation of size_t
			   constants; the range it defines can be equal to
			   or smaller than the integer size implied by
			   TARG_SIZE_T_INT_KIND.  Initialized to the default
			   value but reconfigurable. */

EXTERN_THREAD an_integer_kind
		targ_size_t_int_kind;
			/* Representation for size_t -- the integer kind
			   large enough to hold a pointer value.  Initialized
			   to the default value but reconfigurable. */

#if GNU_EXTENSIONS_ALLOWED
EXTERN_THREAD an_integer_kind
		targ_ssize_t_int_kind;
			/* Representation for ssize_t (a POSIX type used to
			   count bytes in certain I/O functions). */
#endif /* GNU_EXTENSIONS_ALLOWED */

#if FIXED_POINT_ALLOWED
/*
Fixed-point types:
*/
EXTERN_THREAD a_targ_size_t
	targ_sizeof_fixed_point[/*is_unsigned*/2]
	                       [(int)fpp_last]
	                       [/*is_fract*/2]
#if VAR_INITIALIZERS
		= { { { TARG_SIZEOF_SIGNED_SHORT_ACCUM,
		        TARG_SIZEOF_SIGNED_SHORT_FRACT },
		      { TARG_SIZEOF_SIGNED_ACCUM,
		        TARG_SIZEOF_SIGNED_FRACT },
		      { TARG_SIZEOF_SIGNED_LONG_ACCUM,
		        TARG_SIZEOF_SIGNED_LONG_FRACT } },
		    { { TARG_SIZEOF_UNSIGNED_SHORT_ACCUM,
		        TARG_SIZEOF_UNSIGNED_SHORT_FRACT },
		      { TARG_SIZEOF_UNSIGNED_ACCUM,
		        TARG_SIZEOF_UNSIGNED_FRACT },
		      { TARG_SIZEOF_UNSIGNED_LONG_ACCUM,
		        TARG_SIZEOF_UNSIGNED_LONG_FRACT } } }
#endif /* VAR_INITIALIZERS */
		                                             ;

EXTERN_THREAD a_targ_alignment
	targ_alignof_fixed_point[/*is_unsigned*/2]
	                        [(int)fpp_last]
	                        [/*is_fract*/2]
#if VAR_INITIALIZERS
		= { { { TARG_ALIGNOF_SIGNED_SHORT_ACCUM,
		        TARG_ALIGNOF_SIGNED_SHORT_FRACT },
		      { TARG_ALIGNOF_SIGNED_ACCUM,
		        TARG_ALIGNOF_SIGNED_FRACT },
		      { TARG_ALIGNOF_SIGNED_LONG_ACCUM,
		        TARG_ALIGNOF_SIGNED_LONG_FRACT } },
		    { { TARG_ALIGNOF_UNSIGNED_SHORT_ACCUM,
		        TARG_ALIGNOF_UNSIGNED_SHORT_FRACT },
		      { TARG_ALIGNOF_UNSIGNED_ACCUM,
		        TARG_ALIGNOF_UNSIGNED_FRACT },
		      { TARG_ALIGNOF_UNSIGNED_LONG_ACCUM,
		        TARG_ALIGNOF_UNSIGNED_LONG_FRACT } } }
#endif /* VAR_INITIALIZERS */
		                                              ;

EXTERN_THREAD a_targ_alignment
	targ_fractional_bits_for_fixed_point[/*is_unsigned*/2]
	                                    [(int)fpp_last]
	                                    [/*is_fract*/2]
#if VAR_INITIALIZERS
		= { { { TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_ACCUM,
		        TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_FRACT },
		      { TARG_FRACTIONAL_BITS_FOR_SIGNED_ACCUM,
		        TARG_FRACTIONAL_BITS_FOR_SIGNED_FRACT },
		      { TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_ACCUM,
		        TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_FRACT } },
		    { { TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_ACCUM,
		        TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_FRACT },
		      { TARG_FRACTIONAL_BITS_FOR_UNSIGNED_ACCUM,
		        TARG_FRACTIONAL_BITS_FOR_UNSIGNED_FRACT },
		      { TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_ACCUM,
		        TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_FRACT } } }
#endif /* VAR_INITIALIZERS */
		                                                          ;
EXTERN_THREAD a_targ_size_t
		targ_sizeof_largest_fixed_point;
                        /* Size of the longest fixed point type in the
                           configuration.  Must be no larger than
                           MAX_SIZEOF_LARGEST_FIXED_POINT. */

#endif /* FIXED_POINT_ALLOWED */

/*
Float types:
*/
EXTERN_THREAD a_targ_size_t
		targ_sizeof_float;
			/* Size of a float.  Initialized to the default
			   value but reconfigurable. */

EXTERN_THREAD a_targ_alignment
		targ_alignof_float;
			/* Alignment of a float.  Initialized to the default
			   value but reconfigurable. */

EXTERN_THREAD a_targ_size_t
		targ_sizeof_double;
			/* Size of a double.  Initialized to the default
			   value but reconfigurable. */

EXTERN_THREAD a_targ_alignment
		targ_alignof_double;
			/* Alignment of a double.  Initialized to the default
			   value but reconfigurable. */

EXTERN_THREAD a_targ_size_t
		targ_sizeof_long_double;
			/* Size of a long double.  Initialized to the default
			   value but reconfigurable. */

EXTERN_THREAD a_targ_alignment
		targ_alignof_long_double;
			/* Alignment of a long double.  Initialized to the
			   default value but reconfigurable. */

EXTERN_THREAD a_targ_size_t
		targ_sizeof_float80;
			/* Size of a __float80.  Initialized to the default
			   value but reconfigurable. */

EXTERN_THREAD a_targ_alignment
		targ_alignof_float80;
			/* Alignment of a __float80.  Initialized to the
			   default value but reconfigurable. */

EXTERN_THREAD a_targ_size_t
		targ_sizeof_float128;
			/* Size of a __float128.  Initialized to the default
			   value but reconfigurable. */

EXTERN_THREAD a_targ_alignment
		targ_alignof_float128;
			/* Alignment of a __float128.  Initialized to the
			   default value but reconfigurable. */

EXTERN_THREAD a_float_kind
		float_kind_for_float80
#if VAR_INITIALIZERS
			= (a_float_kind)DEFAULT_FLOAT_KIND_FOR_FLOAT80
#endif /* VAR_INITIALIZERS */
			                                              ;
			/* Mapping of the type denoted by __float80.  It may
			   be its own type (fk_float80), or it may be a
			   synonym for another type (e.g., fk_long_double). */

EXTERN_THREAD a_float_kind
		float_kind_for_float128
#if VAR_INITIALIZERS
			= (a_float_kind)DEFAULT_FLOAT_KIND_FOR_FLOAT128
#endif /* VAR_INITIALIZERS */
			                                               ;
			/* Mapping of the type denoted by __float128.  It may
			   be its own type (fk_float128), or it may be a
			   synonym for another type (e.g., fk_long_double). */


#if GNU_EXTENSIONS_ALLOWED

EXTERN_THREAD a_type_mode_kind
		targ_word_mode;
			/* Mode of a word, i.e., the natural integer
			   size for the target.  Initialized to the
			   default value but reconfigurable. */

EXTERN_THREAD a_type_mode_kind
		targ_unwind_word_mode;
			/* Mode of an "unwind word", i.e., the integer size
			   used in unwind descriptors for the target.
			   Initialized to the default value but
			   reconfigurable. */

EXTERN_THREAD a_type_mode_kind
		targ_libgcc_cmp_return_mode;
			/* Mode used by GNU's libgcc for compare instruction
			   results.  Initialized to the default value but
			   reconfigurable. */

EXTERN_THREAD a_type_mode_kind
		targ_libgcc_shift_count_mode;
			/* Mode used by GNU's libgcc for shift counts.
			   Initialized to the default value but
			   reconfigurable. */

EXTERN_THREAD a_type_mode_kind
		targ_pointer_mode;
			/* Mode of a pointer.  Initialized to the
			   default value but reconfigurable. */

#endif /* GNU_EXTENSIONS_ALLOWED */

EXTERN_THREAD a_boolean
                targ_dual_alignments_for_builtin_types;
                        /* TRUE if field alignments are different from those of
                           a variable of the same type. */

EXTERN_THREAD a_targ_alignment
		targ_short_field_alignment;
			/* Default alignment for fields of type short. */

EXTERN_THREAD a_targ_alignment
		targ_int_field_alignment;
			/* Default alignment for fields of type int. */

EXTERN_THREAD a_targ_alignment
		targ_long_field_alignment;
			/* Default alignment for fields of type long. */

#if LONG_LONG_ALLOWED
EXTERN_THREAD a_targ_alignment
		targ_long_long_field_alignment;
			/* Default alignment for fields of type long long. */
#endif /* LONG_LONG_ALLOWED */

#if INT128_EXTENSIONS_ALLOWED
EXTERN_THREAD a_targ_alignment
		targ_int128_field_alignment;
			/* Default alignment for fields of the 128-bit integer
			   type. */
#endif /* INT128_EXTENSIONS_ALLOWED */

EXTERN_THREAD a_targ_alignment
		targ_float_field_alignment;
			/* Default alignment for fields of type float. */

EXTERN_THREAD a_targ_alignment
		targ_double_field_alignment;
			/* Default alignment for fields of type double. */

EXTERN_THREAD a_targ_alignment
		targ_long_double_field_alignment;
			/* Default alignment for fields of type long double. */

EXTERN_THREAD a_targ_alignment
		targ_float80_field_alignment;
			/* Default alignment for fields of type __float80. */

EXTERN_THREAD a_targ_alignment
		targ_float128_field_alignment;
			/* Default alignment for fields of type __float128. */


/*
C++ pointer-to-member type.
*/
EXTERN_THREAD a_targ_size_t
		targ_sizeof_ptr_to_data_member;
			/* Size of a pointer-to-data-member.  Initialized to
			   the default value but reconfigurable. */

EXTERN_THREAD a_targ_alignment
		targ_alignof_ptr_to_data_member;
			/* Alignment of a pointer-to-data-member.  Initialized
			   to the default value but reconfigurable. */

EXTERN_THREAD a_targ_size_t
		targ_sizeof_ptr_to_member_function;
			/* Size of a ptr-to-member-function.  Initialized to
			   the default value but reconfigurable. */

EXTERN_THREAD a_targ_alignment
		targ_alignof_ptr_to_member_function;
			/* Alignment of a pointer-to-member-function.
			   Initialized to the default value but
			   reconfigurable. */

EXTERN_THREAD a_boolean
		targ_microsoft_ptr_to_member_sizing;
			/* TRUE if the size of pointer-to-member types should
			   follow the class-dependent rules implemented by
			   Microsoft compilers (see the configuration macro
			   TARG_MICROSOFT_PTR_TO_MEMBER_SIZING, which is the
			   default value for this variable). */

/*
Virtual function info.
*/
EXTERN_THREAD a_targ_size_t
		targ_sizeof_virtual_function_info;
			/* Size of a virtual-function-info entity.
			   Initialized to the default value but
			   reconfigurable. */

EXTERN_THREAD a_targ_alignment
		targ_alignof_virtual_function_info;
			/* Alignment of a virtual-function-info entity.
			   Initialized to the default value but
			   reconfigurable. */

#if !IA64_ABI
/*
Pointer to virtual base class.
*/
EXTERN_THREAD a_targ_size_t
		targ_sizeof_ptr_to_virtual_base_class;
			/* Size of a "pointer-to-virtual-base-class" member.
			   Initialized to the default value but
			   reconfigurable. */

EXTERN_THREAD a_targ_alignment
		targ_alignof_ptr_to_virtual_base_class;
			/* Alignment of a "pointer-to-virtual-base-class"
			   member.  Initialized to the default value but
			   reconfigurable. */
#endif /* !IA64_ABI */

/*
Miscellaneous
*/
EXTERN_THREAD a_boolean
		targ_enum_types_can_be_smaller_than_int;
			/* When TRUE, enum types will be allocated in the
			   smallest integral type in which they will fit; if
			   FALSE, int will be used (e.g., for cfront
			   compatibility).  Initialized to the default value
			   but reconfigurable. */

EXTERN_THREAD a_boolean
		targ_right_shift_is_arithmetic;
			/* When TRUE a right shift on a signed quantity does
			   sign extension.  Initialized to the default value
			   but reconfigurable. */

EXTERN_THREAD a_boolean
		targ_too_large_shift_count_is_taken_modulo_size;
			/* When TRUE a shift with a too-large shift count is
			   treated as if the shift count is reduced modulo
			   the bit size of the object. */

EXTERN_THREAD a_targ_alignment
		targ_minimum_struct_alignment;
			/* The minimum alignment required for objects of class,
			   struct, and union type in the target environment.
			   Initialized to the default value but
			   reconfigurable. */

EXTERN_THREAD a_boolean
                targ_field_alloc_sequence_equals_decl_sequence;
                        /* TRUE if class and struct fields are allocated in the
                           same order as they are declared, regardless of
                           access specification.  See
                           TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE. */

EXTERN_THREAD a_targ_alignment
		targ_minimum_pack_alignment;
			/* The minimum value which a "pack alignment" value
			   may have.  Initialized to the default value but
			   reconfigurable. */

EXTERN_THREAD a_targ_alignment
		targ_maximum_pack_alignment;
			/* The maximum value which a "pack alignment" value
			   may have.  Initialized to the default value but
			   reconfigurable. */

EXTERN_THREAD a_targ_alignment
		targ_maximum_intrinsic_alignment;
			/* The maximum alignment value which the target can
			   take advantage of.  Initialized to the default
			   value but reconfigurable. */

EXTERN_THREAD a_targ_alignment
		targ_default_new_alignment;
			/* In C++17 mode, the alignment beyond which new
			   and delete expressions will use the versions of
			   allocation and deallocation functions with an
			   alignment parameter; also, the value of the
			   __STDCPP_DEFAULT_NEW_ALIGNMENT__ predefined
			   macro.  Initialized to the default value but
			   reconfigurable. */

EXTERN_THREAD a_boolean
		distinct_template_signatures;
			/* If TRUE, template functions are given mangled names
			   that are distinct from the names for nontemplate
			   functions. */

EXTERN_THREAD a_boolean
		exc_spec_in_func_type;
			/* TRUE if function types should formally include
			   exception specifications.  The IL representation
			   of function types always includes exception
			   specifications, but C++17 included them
			   semantically.  This flag is TRUE if the C++17
			   semantics should be implemented. */

EXTERN_THREAD a_boolean
		deduction_from_exc_spec_allowed;
			/* TRUE if a template parameter can be deduced from
			   the noexcept flag of a parameter of function
			   type. */

EXTERN_THREAD a_boolean
		assume_references_cannot_be_null;
			/* If TRUE, C++ references are assumed never to have
			   NULL addresses in them.  That's as required by the
			   C++ standard, but some implementations allow
			   that. */

#if DO_IL_LOWERING

EXTERN_THREAD a_boolean
		force_variable_definition_via_zeroing;
			/* If TRUE, add zeroing to variable definitions
			   to make them definitions in C. */

EXTERN_THREAD a_boolean
		make_all_functions_unprototyped;
			/* If TRUE, all functions are rewritten to be
			   unprototyped. */

EXTERN_THREAD a_boolean
		assume_this_cannot_be_null_in_conditional_operators;
			/* If TRUE, assume "this" cannot be null in conditional
			   operators, allowing some additional optimizations
			   (i.e., dead code removal) in "?", "&&", and "||"
			   operations. */

#if DO_FULL_PORTABLE_EH_LOWERING

EXTERN_THREAD unsigned int
		targ_jmp_buf_num_elements;
			/* Number of elements in a jmp_buf array.  Initialized
			   to the default value but reconfigurable. */
EXTERN_THREAD a_boolean
		targ_jmp_buf_elements_are_float;
			/* Choose between integer and float members of the
			   jmp_buf array.  Initialized to the default value
			   but reconfigurable. */
EXTERN_THREAD an_integer_kind
		targ_jmp_buf_element_int_kind;
			/* Integer kind indicating the kind of element in a
			   jmp_buf array.  Initialized to the default value
			   but reconfigurable. */
EXTERN_THREAD a_float_kind
		targ_jmp_buf_element_float_kind;
			/* Float kind indicating the kind of element in a
			   jmp_buf array.  Initialized to the default value
			   but reconfigurable. */
EXTERN_THREAD a_const_char
		*targ_setjmp_func;
			/* The name of the setjmp function to call.
			   Initialized to the default value but
			   reconfigurable. */

#endif /* DO_FULL_PORTABLE_EH_LOWERING */

#if GENERATE_EH_TABLES
EXTERN_THREAD an_integer_kind
		targ_var_handle_int_kind;
			/* Integer kind to be used for a "handle" in
			   exception handling tables. */
#endif /* GENERATE_EH_TABLES */
#endif /* DO_IL_LOWERING */

EXTERN_THREAD a_targ_size_t
		targ_flt_mant_dig;
			/* The number of bits in the mantissa of a float. */

EXTERN_THREAD int
		targ_flt_min_exp;
			/* The minimum exponent value of a float. */

EXTERN_THREAD int
		targ_flt_max_exp;
			/* The maximum exponent value of a float. */

EXTERN_THREAD a_targ_size_t
		targ_dbl_mant_dig;
			/* The number of bits in the mantissa of a double. */

EXTERN_THREAD int
		targ_dbl_min_exp;
			/* The minimum exponent value of a double. */

EXTERN_THREAD int
		targ_dbl_max_exp;
			/* The maximum exponent value of a double. */

EXTERN_THREAD a_targ_size_t
		targ_ldbl_mant_dig;
			/* The number of bits in the mantissa of a long
                           double. */

EXTERN_THREAD int
		targ_ldbl_min_exp;
			/* The minimum exponent value of a long double. */

EXTERN_THREAD int
		targ_ldbl_max_exp;
			/* The maximum exponent value of a long double. */

EXTERN_THREAD a_targ_size_t
		targ_flt80_mant_dig;
			/* The number of bits in the mantissa of a
                           __float80. */

EXTERN_THREAD int
		targ_flt80_min_exp;
			/* The minimum exponent value of a __float80. */

EXTERN_THREAD int
		targ_flt80_max_exp;
			/* The maximum exponent value of a __float80. */

EXTERN_THREAD a_targ_size_t
		targ_flt128_mant_dig;
			/* The number of bits in the mantissa of a
                           __float128. */

EXTERN_THREAD int
		targ_flt128_min_exp;
			/* The minimum exponent value of a __float128. */

EXTERN_THREAD int
		targ_flt128_max_exp;
			/* The maximum exponent value of a __float128. */

EXTERN_THREAD a_boolean
		remove_qualifiers_from_param_types;
			/* TRUE when type qualifiers should be removed from
			   function parameter types (e.g., a "const int"
			   parameter is seen simply as "int"). */

EXTERN_THREAD a_boolean
		keep_restrict_in_signatures;
			/* TRUE if the "restrict" (or "__restrict") qualifier
			   should be kept in function parameter types even
			   when remove_qualifiers_from_param_types is TRUE. */

EXTERN_THREAD a_boolean
		c_and_cpp_function_types_are_distinct;
			/* If TRUE, function types are considered distinct if
			   their only difference is that one has extern "C"
			   routine linkage and the other has extern "C++"
			   routine linkage.  This affects, among other things,
			   overload resolution and name mangling.  (See also
			   impl_conv_between_c_and_cpp_function_ptrs_allowed,
			   defined in cmd_line.h.) */

#if BACK_END_IS_CP_GEN_BE
EXTERN_THREAD a_boolean
		old_specializations_for_generated_instances;
			/* If TRUE, specializations for generated template
			   instances in generated code (C++-generating back
			   end) should use the old syntax instead of the
			   modern "template <>" prefix form. */
#endif /* BACK_END_IS_CP_GEN_BE */

EXTERN_THREAD a_boolean
		type_info_in_namespace_std;
			/* If TRUE,  class type_info is defined as a member
			   of namespace "std". */

EXTERN_THREAD a_boolean
		pass_stdarg_references_to_generated_code;
			/* If TRUE, references to the macros in <stdarg.h>
			   are passed through to the output unchanged. */

EXTERN_THREAD a_boolean
		va_list_in_std_namespace;
			/* If TRUE, the va_list type created when passing
			   stdarg references to generated code is placed in
			   the std namespace. */

EXTERN_THREAD a_boolean
		va_list_using_using_decl_in_std_namespace;
			/* When va_list_in_std_namespace is FALSE, this is
			   TRUE if a using-declaration for va_list should
			   be created in the std namespace. */

EXTERN_THREAD a_boolean
		va_arg_returns_lvalue;
			/* If TRUE, the va_arg operator implemented when
			   passing stdarg references to generated code returns
			   an lvalue.  The C and C++ standards allow but
			   do not require that va_arg produce an lvalue. */

EXTERN_THREAD a_boolean
		instantiate_extern_inline;
			/* TRUE if the instantiation mechanism should be used
			   to control the definition of inline functions. */

EXTERN_THREAD a_boolean
		instantiate_inline_variables;
			/* TRUE if the instantiation mechanism should be used
			   to control the definition of inline variables. */

#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE

EXTERN_THREAD a_boolean
		sun_is_generated_code_target;
			/* TRUE if code is being generated for a Sun
			   compiler. */

#ifdef SUN_TARGET_VERSION_NUMBER
EXTERN_THREAD unsigned long
		sun_target_version_number;
			/* The version number of the Sun compiler being
			   targeted (e.g., 0x530 for version 5.3). */
#endif /* ifdef SUN_TARGET_VERSION_NUMBER */

EXTERN_THREAD a_boolean
		gcc_is_generated_code_target;
			/* TRUE if code is being generated for the GNU C or
			   C++ compiler.  See also
			   gcc_or_clang_is_generated_code_target. */

EXTERN_THREAD unsigned long
		gnu_target_version_number;
			/* The version number of the GNU compiler being
			   targeted (e.g., 30401 for GNU C/C++ 3.4.1). */

EXTERN_THREAD a_boolean
		gcc_builtin_varargs_in_generated_code;
			/* TRUE if the generated code should use vararg
			   primitives predefined by GNU compilers. */

EXTERN_THREAD a_boolean
		clang_is_generated_code_target;
			/* TRUE if code is being generated for the clang
			   (C or C++) compiler.  See also
			   gcc_or_clang_is_generated_code_target. */

EXTERN_THREAD unsigned long
		clang_target_version_number;
			/* The version number of the clang compiler being
			   targeted (e.g., 30300 for clang C/C++ 3.3). */

EXTERN_THREAD a_boolean
		gcc_or_clang_is_generated_code_target;
			/* TRUE if code is being generated for either the
			   GNU or clang (C or C++) compiler. */

EXTERN_THREAD a_boolean
		msvc_is_generated_code_target;
			/* TRUE if code is being generated for the Microsoft
			   MSVC++ compiler. */

EXTERN_THREAD unsigned long
		msvc_target_version_number;
			/* The version number (i.e., 1300 for 7.0) of the
			   Microsoft MSVC compiler being targeted. */

EXTERN_THREAD a_boolean
		microsoft_dialect_is_generated_code_target;
			/* TRUE if code is being generated for a compiler
			   accepting Microsoft extensions. */

#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */

EXTERN_THREAD a_boolean
		always_fold_calls_to_builtin_constant_p;
			/* TRUE if calls to __builtin_constant_p should always
			   be folded in the front end. */

#if BACK_END_IS_CP_GEN_BE

EXTERN_THREAD a_boolean
		cp_gen_be_target_matches_source_dialect;
			/* Flag that indicates that the C++-generating back
			   end should assume the target dialect is the same
			   as the source dialect. */

#endif /* BACK_END_IS_CP_GEN_BE */

EXTERN_THREAD an_integer_kind
		plain_char_int_kind;
			/* Integer kind for a "plain" char, dependent on
			   the setting of targ_has_signed_chars. */

EXTERN_THREAD a_boolean
		string_literals_shared;
			/* TRUE if string literals can be shared.  FALSE
			   if string literals are not shared because they
			   might be writable (as in pcc mode). */

#if !IA64_ABI
EXTERN_THREAD an_integer_kind
		targ_runtime_elem_count_int_kind;
			/* Type used for number_of_elements arguments
			   in the cfront ABI. */
#endif /* !IA64_ABI */

#if IA64_ABI
EXTERN_THREAD a_boolean
		targ_reuse_tail_padding;
			/* TRUE if the IA-64 ABI can reuse tail-padding from
			   base classes for other subobjects of the derived
			   class. */
#endif /* IA64_ABI */

EXTERN_THREAD a_boolean
		warn_on_try_statement;
			/* When TRUE (and in Microsoft emulation mode),
			   a (one time) warning is issued when a try statement
			   is encountered. */

#if BACK_END_IS_C_GEN_BE
EXTERN_THREAD a_boolean
		use_empty_struct_in_generated_c;
			/* When TRUE, the C-generating back end will use an
			   empty struct as the representation of an empty
			   class.  Otherwise, the generated struct will have
			   a one-byte padding field. */
#endif /* BACK_END_IS_C_GEN_BE */

EXTERN_THREAD char
                *auxiliary_info_dir_name;
                        /* Initialized to EDG_AUXILIARY_INFO_DIR_NAME (e.g.,
                           "lib"), when using an unnamed target configuration,
                           otherwise the target name (and a separating
                           underscore) is appended to the name. */

#if DO_IL_LOWERING
EXTERN_THREAD an_integer_kind
		targ_delta_int_kind;
                        /* Integer kind to use for an offset into a class.
                           This is used for delta fields in pointers to member
                           functions, etc., but not for pointers to data
                           members. */

EXTERN_THREAD an_integer_kind
		targ_virtual_function_index_int_kind;
                        /* Integer kind to use for an index into a virtual
                           function table.  Must be no smaller than the size of
                           a_virtual_function_number. */
EXTERN_THREAD a_boolean
                ctors_return_this;
                        /* Flag that indicates whether constructors return
                           "this".  This is TRUE in the Cfront-like ABI and
                           FALSE in the IA-64 ABI, but TRUE in the ARM EABI
                           variant of the IA-64 ABI. */
EXTERN_THREAD a_boolean
                dtors_return_this;
                        /* Flag that indicates whether destructors return
                           "this".  This is FALSE except in the ARM EABI
                           variant of the IA-64 ABI.  */

#if IA64_ABI
EXTERN_THREAD a_boolean
                targ_ia64_abi_use_guard_acquire_release;
                        /* TRUE if code should be generated to call the runtime
                           guard acquire/release/abort routines in
                           initializations of local static variables.  If the
                           flag is FALSE, the guard variables are tested/set by
                           inline code.  TRUE allows a thread-safe solution in
                           the runtime. */
EXTERN_THREAD a_boolean
                targ_ia64_abi_use_int_static_init_guard;
                        /* TRUE to use ARM EABI semantics for static
                           initialization guard variables (see section 4.4.2 of
                           version 2.02 of the ARM EABI).  The two differences
                           from the standard IA-64 ABI are: the guard variable
                           is "int"-sized, and the least significant bit of the
                           guard variable (rather than the first byte) is used
                           for the guard test. */
EXTERN_THREAD a_boolean
                targ_ia64_abi_use_variant_array_cookies;
                        /* TRUE to use the variant representation of array
                           cookies with the IA-64 ABI.  The variant form uses a
                           struct rather than the simple size_t value of the
                           standard IA-64 ABI.  This variant version is used
                           for the ARM architecture.  See 3.2.2.1 in the ARM
                           EABI document. */
EXTERN_THREAD a_boolean
                targ_ia64_abi_use_variant_ptr_to_member_function_repr;
                        /* TRUE to use the variant representation of pointers
                           to member functions with the IA-64 ABI. */
EXTERN_THREAD a_boolean
                targ_ia64_abi_variant_ctors_and_dtors_return_this;
                        /* TRUE to make constructors and destructors return the
                           "this" value in a variant of the IA-64 ABI.  This is
                           used by the ARM EABI.  Constructors return "pointer
                           to class", and destructors return "void *", except
                           deleting destructors, which return the standard
                           "void". */
EXTERN_THREAD a_boolean
                targ_ia64_abi_variant_key_function;
                        /* TRUE to select the variant rule for determining the
                           key function (decider function) for virtual function
                           tables in the IA-64 ABI.  See 3.1 in the ARM EABI
                           document. */
EXTERN_THREAD an_integer_kind
		targ_ia64_vtable_entry_int_kind;
                        /* Integer kind used for the size of a vtable entry in
                           the IA-64 ABI. */

#endif /* IA64_ABI */

#if GENERATE_EH_TABLES
EXTERN_THREAD an_integer_kind
		targ_region_number_int_kind;
                        /* The integral kind to be used for a cleanup region
                           number with exception processing. */
EXTERN_THREAD an_integer_kind
		targ_ets_flag_type_int_kind;
                        /* The integral kind to be used for flags passed to
                           the run time library for exception processing. */
#endif /* GENERATE_EH_TABLES */
#endif /* DO_IL_LOWERING */

#ifndef DO_NOT_UNDEF_TARGET_MACROS
/* Aside from occasional references in targ_def.h, the following values
   should be used *only* to initialize the variables declared in this file.
   To enforce this convention, they are undefined at this time.  (This is
   not foolproof, but it should catch most such misuses and is especially
   important for target-specific macros).  The macro DO_NOT_UNDEF_TARGET_MACROS
   is defined by target.c so that variables declared in this file may be
   initialized there. */
#undef TARG_LITTLE_ENDIAN
#undef TARG_CHAR_BIT
#undef TARG_HOST_STRING_CHAR_BIT
#undef TARG_HAS_SIGNED_CHARS
#undef TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT
#undef TARG_WCHAR_T_INT_KIND
#undef TARG_WINT_T_INT_KIND
#undef TARG_CHAR16_T_INT_KIND
#undef TARG_CHAR32_T_INT_KIND
#undef TARG_BOOL_INT_KIND
#undef TARG_C_BOOL_INT_KIND
#undef TARG_SIZEOF_SHORT
#undef TARG_ALIGNOF_SHORT
#undef TARG_SIZEOF_INT
#undef TARG_ALIGNOF_INT
#undef TARG_SIZEOF_LONG
#undef TARG_ALIGNOF_LONG
#if LONG_LONG_ALLOWED
#undef TARG_SIZEOF_LONG_LONG
#undef TARG_ALIGNOF_LONG_LONG
#endif /* LONG_LONG_ALLOWED */
#if INT128_EXTENSIONS_ALLOWED
#undef TARG_SIZEOF_INT128
#undef TARG_ALIGNOF_INT128
#endif /* INT128_EXTENSIONS_ALLOWED */
/* TARG_SIZEOF_LARGEST_INTEGER and TARG_SIZEOF_LARGEST_FIXED_POINT are not
   #undef'ed here (their definitions are used in MAX_SIZEOF_LARGEST_INTEGER and
   MAX_SIZEOF_LARGEST_FIXED_POINT. */
#if GNU_EXTENSIONS_ALLOWED
#undef TARG_SIZEOF_LARGEST_ATOMIC
#endif /* GNU_EXTENSIONS_ALLOWED */
#undef TARG_MAX_CLASS_OBJECT_SIZE
#undef TARG_MAX_BASE_CLASS_OFFSET
#undef TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT
#undef TARG_BIT_FIELD_CONTAINER_SIZE
#undef TARG_MICROSOFT_BIT_FIELD_ALLOCATION
#undef TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED
#undef TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED
#undef TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED
#undef TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED
#undef TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT
#undef TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS
#undef TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE
#undef TARG_SIZEOF_POINTER
#undef TARG_ALIGNOF_POINTER
#undef TARG_ALL_POINTERS_SAME_SIZE
#if MICROSOFT_EXTENSIONS_ALLOWED
#undef TARG_SIZEOF_FAR_POINTER
#undef TARG_ALIGNOF_FAR_POINTER
#undef TARG_SIZEOF_NEAR_POINTER
#undef TARG_ALIGNOF_NEAR_POINTER
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
#undef TARG_LIBGCC_CMP_RETURN_MODE
#undef TARG_LIBGCC_SHIFT_COUNT_MODE
#undef TARG_UNWIND_WORD_MODE
#undef TARG_POINTER_MODE
#undef TARG_WORD_MODE
#endif /* GNU_EXTENSIONS_ALLOWED */
#undef TARG_PTRDIFF_T_INT_KIND
#undef TARG_SIZE_T_MAX
#undef TARG_SIZE_T_INT_KIND
#undef TARG_SSIZE_T_INT_KIND
#undef TARG_SIZEOF_FLOAT
#undef TARG_ALIGNOF_FLOAT
#undef TARG_SIZEOF_DOUBLE
#undef TARG_ALIGNOF_DOUBLE
#undef TARG_SIZEOF_LONG_DOUBLE
#undef TARG_ALIGNOF_LONG_DOUBLE
#undef TARG_SIZEOF_PTR_TO_DATA_MEMBER
#undef TARG_ALIGNOF_PTR_TO_DATA_MEMBER
#undef TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION
#undef TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION
#undef TARG_MICROSOFT_PTR_TO_MEMBER_SIZING
#undef TARG_SIZEOF_VIRTUAL_FUNCTION_INFO
#undef TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO
#undef TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS
#undef TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS
#undef TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT
#undef TARG_RIGHT_SHIFT_IS_ARITHMETIC
#undef TARG_MINIMUM_STRUCT_ALIGNMENT
#undef TARG_MINIMUM_PACK_ALIGNMENT
#undef TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE
#undef MAKE_ALL_FUNCTIONS_UNPROTOTYPED
#undef ASSUME_THIS_CANNOT_BE_NULL_IN_CONDITIONAL_OPERATORS
#undef TARG_SETJMP_FUNC
#undef TARG_JMP_BUF_NUM_ELEMENTS
#undef TARG_JMP_BUF_ELEMENTS_ARE_FLOAT
#undef TARG_JMP_BUF_ELEMENT_INT_KIND
#undef TARG_JMP_BUF_ELEMENT_FLOAT_KIND
#undef TARG_VAR_HANDLE_INT_KIND
#undef TARG_FLT_MANT_DIG
#undef TARG_FLT_MIN_EXP
#undef TARG_FLT_MAX_EXP
#undef TARG_DBL_MANT_DIG
#undef TARG_DBL_MIN_EXP
#undef TARG_DBL_MAX_EXP
#undef TARG_LDBL_MANT_DIG
#undef TARG_LDBL_MIN_EXP
#undef TARG_LDBL_MAX_EXP
#undef MSVC_IS_GENERATED_CODE_TARGET
#undef MSVC_TARGET_VERSION_NUMBER
#if !IA64_ABI
#undef TARG_RUNTIME_ELEM_COUNT_INT_KIND
#endif /* !IA64_ABI */
#if BACK_END_IS_C_GEN_BE
#undef USE_EMPTY_STRUCT_IN_GENERATED_C
#if DO_IL_LOWERING
#undef TARG_DELTA_INT_KIND
#undef TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND
#if IA64_ABI
#undef TARG_IA64_ABI_USE_GUARD_ACQUIRE_RELEASE
#undef TARG_IA64_ABI_USE_INT_STATIC_INIT_GUARD
#undef TARG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES
#undef TARG_IA64_ABI_USE_VARIANT_PTR_TO_MEMBER_FUNCTION_REPR
#undef TARG_IA64_ABI_VARIANT_CTORS_AND_DTORS_RETURN_THIS
#undef TARG_IA64_ABI_VARIANT_KEY_FUNCTION
#undef TARG_IA64_VTABLE_ENTRY_INT_KIND
#if GENERATE_EH_TABLES
#undef TARG_REGION_NUMBER_INT_KIND
#undef TARG_ETS_FLAG_TYPE_INT_KIND
#endif /* GENERATE_EH_TABLES */
#endif /* IA64_ABI */
#endif /* DO_IL_LOWERING */
#if IA64_ABI
#undef TARG_REUSE_TAIL_PADDING
#endif /* IA64_ABI */
#endif /* BACK_END_IS_C_GEN_BE */
#undef TARG_SUPPORTS_ARM32
#undef TARG_SUPPORTS_ARM64
#undef TARG_SUPPORTS_X86_64
#undef EDG_AUXILIARY_INFO_DIR_NAME
#if FIXED_POINT_ALLOWED
#undef TARG_ALIGNOF_SIGNED_ACCUM
#undef TARG_ALIGNOF_SIGNED_FRACT
#undef TARG_ALIGNOF_SIGNED_LONG_ACCUM
#undef TARG_ALIGNOF_SIGNED_LONG_FRACT
#undef TARG_ALIGNOF_SIGNED_SHORT_ACCUM
#undef TARG_ALIGNOF_SIGNED_SHORT_FRACT
#undef TARG_ALIGNOF_UNSIGNED_ACCUM
#undef TARG_ALIGNOF_UNSIGNED_FRACT
#undef TARG_ALIGNOF_UNSIGNED_LONG_ACCUM
#undef TARG_ALIGNOF_UNSIGNED_LONG_FRACT
#undef TARG_ALIGNOF_UNSIGNED_SHORT_ACCUM
#undef TARG_ALIGNOF_UNSIGNED_SHORT_FRACT
#undef TARG_FRACTIONAL_BITS_FOR_SIGNED_ACCUM
#undef TARG_FRACTIONAL_BITS_FOR_SIGNED_FRACT
#undef TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_ACCUM
#undef TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_FRACT
#undef TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_ACCUM
#undef TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_FRACT
#undef TARG_FRACTIONAL_BITS_FOR_UNSIGNED_ACCUM
#undef TARG_FRACTIONAL_BITS_FOR_UNSIGNED_FRACT
#undef TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_ACCUM
#undef TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_FRACT
#undef TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_ACCUM
#undef TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_FRACT
#undef TARG_SIZEOF_SIGNED_ACCUM
#undef TARG_SIZEOF_SIGNED_FRACT
#undef TARG_SIZEOF_SIGNED_LONG_ACCUM
#undef TARG_SIZEOF_SIGNED_LONG_FRACT
#undef TARG_SIZEOF_SIGNED_SHORT_ACCUM
#undef TARG_SIZEOF_SIGNED_SHORT_FRACT
#undef TARG_SIZEOF_UNSIGNED_ACCUM
#undef TARG_SIZEOF_UNSIGNED_FRACT
#undef TARG_SIZEOF_UNSIGNED_LONG_ACCUM
#undef TARG_SIZEOF_UNSIGNED_LONG_FRACT
#undef TARG_SIZEOF_UNSIGNED_SHORT_ACCUM
#undef TARG_SIZEOF_UNSIGNED_SHORT_FRACT
#endif /* FIXED_POINT_ALLOWED */
#undef TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT
#undef TARG_C_BOOL_INT_KIND
#undef TARG_DOUBLE_FIELD_ALIGNMENT
#undef TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES
#undef TARG_FLOAT_FIELD_ALIGNMENT
#undef TARG_INT_FIELD_ALIGNMENT
#if INT128_EXTENSIONS_ALLOWED
#undef TARG_INT128_FIELD_ALIGNMENT
#endif /* INT128_EXTENSIONS_ALLOWED */
#undef TARG_LONG_DOUBLE_FIELD_ALIGNMENT
#undef TARG_LONG_FIELD_ALIGNMENT
#undef TARG_LONG_LONG_FIELD_ALIGNMENT
#undef TARG_MAXIMUM_INTRINSIC_ALIGNMENT
#undef TARG_DEFAULT_NEW_ALIGNMENT
#undef TARG_MAXIMUM_PACK_ALIGNMENT
#undef TARG_SHORT_FIELD_ALIGNMENT
#undef TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE
#undef TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT
#undef TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES
#undef TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT
/* MAKE_TARG_NAMES_REFER_TO_VARIABLES cannot be set when this file is included
   by target.c.  If it was previously defined, undefine it and set it to the
   value required by target.c. */
#ifdef MAKE_TARG_NAMES_REFER_TO_VARIABLES
#undef  MAKE_TARG_NAMES_REFER_TO_VARIABLES
#define MAKE_TARG_NAMES_REFER_TO_VARIABLES 0
#endif /* ifdef MAKE_TARG_NAMES_REFER_TO_VARIABLES */
#endif /* ifndef DO_NOT_UNDEF_TARGET_MACROS */

#ifndef MAKE_TARG_NAMES_REFER_TO_VARIABLES
#define MAKE_TARG_NAMES_REFER_TO_VARIABLES 0
#endif /* ifndef MAKE_TARG_NAMES_REFER_TO_VARIABLES */
/* The following macro name redefinitions are provided to help accommodate
   implementations that have code of their own that depends on these names'
   being defined.  These TARG_xxx names should not reappear in code supplied
   by EDG. */
#if MAKE_TARG_NAMES_REFER_TO_VARIABLES
#define TARG_LITTLE_ENDIAN targ_little_endian
#define TARG_CHAR_BIT targ_char_bit
#define TARG_HOST_STRING_CHAR_BIT targ_host_string_char_bit
#define TARG_HAS_SIGNED_CHARS targ_has_signed_chars
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT                   \
                        targ_char_constant_first_char_most_significant
#define TARG_WCHAR_T_INT_KIND targ_wchar_t_int_kind
#define TARG_WINT_T_INT_KIND targ_wint_t_int_kind
#define TARG_CHAR16_T_INT_KIND targ_char16_t_int_kind
#define TARG_CHAR32_T_INT_KIND targ_char32_t_int_kind
#define TARG_BOOL_INT_KIND targ_bool_int_kind
#define TARG_C_BOOL_INT_KIND targ_c_bool_int_kind
#define TARG_SIZEOF_SHORT targ_sizeof_short
#define TARG_ALIGNOF_SHORT targ_alignof_short
#define TARG_SIZEOF_INT targ_sizeof_int
#define TARG_ALIGNOF_INT targ_alignof_int
#define TARG_SIZEOF_LONG targ_sizeof_long
#define TARG_ALIGNOF_LONG targ_alignof_long
#if LONG_LONG_ALLOWED
#define TARG_SIZEOF_LONG_LONG targ_sizeof_long_long
#define TARG_ALIGNOF_LONG_LONG targ_alignof_long_long
#endif /* LONG_LONG_ALLOWED */
#if INT128_EXTENSIONS_ALLOWED
#define TARG_SIZEOF_INT128 targ_sizeof_int128
#define TARG_ALIGNOF_INT128 targ_alignof_128
#endif /* INT128_EXTENSIONS_ALLOWED */
#define TARG_MAX_CLASS_OBJECT_SIZE targ_max_class_object_size
#define TARG_MAX_BASE_CLASS_OFFSET targ_max_base_class_offset
#define TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT                           \
                        targ_optimize_empty_base_class_layout
#define TARG_BIT_FIELD_CONTAINER_SIZE targ_bit_field_container_size
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION targ_microsoft_bit_field_allocation
#define TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED                            \
                        targ_plain_int_bit_field_is_unsigned
#define TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED                     \
                        targ_force_one_bit_bit_field_to_be_unsigned
#define TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED                        \
                        targ_enum_bit_fields_are_always_unsigned
#define TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED                     \
                        targ_nonnegative_enum_bit_field_is_unsigned
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT                             \
                        targ_zero_width_bit_field_alignment
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS          \
                        targ_user_control_of_struct_packing_affects_bit_fields
#define TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE \
                        targ_pad_bit_fields_larger_than_base_type
#define TARG_SIZEOF_POINTER targ_sizeof_pointer
#define TARG_ALIGNOF_POINTER targ_alignof_pointer
#if MICROSOFT_EXTENSIONS_ALLOWED
#define TARG_SIZEOF_FAR_POINTER targ_sizeof_far_pointer
#define TARG_ALIGNOF_FAR_POINTER targ_alignof_far_pointer
#define TARG_SIZEOF_NEAR_POINTER targ_sizeof_near_pointer
#define TARG_ALIGNOF_NEAR_POINTER targ_alignof_near_pointer
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
#define TARG_POINTER_MODE targ_pointer_mode
#define TARG_WORD_MODE targ_word_mode
#endif /* GNU_EXTENSIONS_ALLOWED */
#define TARG_PTRDIFF_T_INT_KIND targ_ptrdiff_t_int_kind
#define TARG_SIZE_T_MAX targ_size_t_max
#define TARG_SIZE_T_INT_KIND targ_size_t_int_kind
#define TARG_SSIZE_T_INT_KIND targ_ssize_t_int_kind
#define TARG_SIZEOF_FLOAT targ_sizeof_float
#define TARG_ALIGNOF_FLOAT targ_alignof_float
#define TARG_SIZEOF_DOUBLE targ_sizeof_double
#define TARG_ALIGNOF_DOUBLE targ_alignof_double
#define TARG_SIZEOF_LONG_DOUBLE targ_sizeof_long_double
#define TARG_ALIGNOF_LONG_DOUBLE targ_alignof_long_double
#define TARG_SIZEOF_PTR_TO_DATA_MEMBER targ_sizeof_ptr_to_data_member
#define TARG_ALIGNOF_PTR_TO_DATA_MEMBER targ_alignof_ptr_to_data_member
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION                              \
                        targ_sizeof_ptr_to_member_function
#define TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION                             \
                        targ_alignof_ptr_to_member_function
#define TARG_MICROSOFT_PTR_TO_MEMBER_SIZING                             \
                        targ_microsoft_ptr_to_member_sizing
#define TARG_SIZEOF_VIRTUAL_FUNCTION_INFO                               \
                        targ_sizeof_virtual_function_info
#define TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO                              \
                        targ_alignof_virtual_function_info
#define TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS                           \
                        targ_sizeof_ptr_to_virtual_base_class
#define TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS                          \
                        targ_alignof_ptr_to_virtual_base_class
#define TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT                         \
                        targ_enum_types_can_be_smaller_than_int
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC targ_right_shift_is_arithmetic
#define TARG_MINIMUM_STRUCT_ALIGNMENT targ_minimum_struct_alignment
#define TARG_MINIMUM_PACK_ALIGNMENT targ_minimum_pack_alignment
#define MAKE_ALL_FUNCTIONS_UNPROTOTYPED make_all_functions_unprototyped
#define ASSUME_THIS_CANNOT_BE_NULL_IN_CONDITIONAL_OPERATORS             \
                        assume_this_cannot_be_null_in_conditional_operators
#define TARG_JMP_BUF_NUM_ELEMENTS targ_jmp_buf_num_elements
#define TARG_JMP_BUF_ELEMENTS_ARE_FLOAT targ_jmp_buf_elements_are_float
#define TARG_JMP_BUF_ELEMENT_INT_KIND targ_jmp_buf_element_int_kind
#define TARG_JMP_BUF_ELEMENT_FLOAT_KIND targ_jmp_buf_element_float_kind
#define TARG_VAR_HANDLE_INT_KIND targ_var_handle_int_kind
#define TARG_FLT_MANT_DIG targ_flt_mant_dig
#define TARG_FLT_MIN_EXP targ_flt_min_exp
#define TARG_FLT_MAX_EXP targ_flt_max_exp
#define TARG_DBL_MANT_DIG targ_dbl_mant_dig
#define TARG_DBL_MIN_EXP targ_dbl_min_exp
#define TARG_DBL_MAX_EXP targ_dbl_max_exp
#define TARG_LDBL_MANT_DIG targ_ldbl_mant_dig
#define TARG_LDBL_MIN_EXP targ_ldbl_min_exp
#define TARG_LDBL_MAX_EXP targ_ldbl_max_exp
#define TARG_FLT80_MANT_DIG targ_flt80_mant_dig
#define TARG_FLT80_MIN_EXP targ_flt80_min_exp
#define TARG_FLT80_MAX_EXP targ_flt80_max_exp
#define TARG_FLT128_MANT_DIG targ_flt128_mant_dig
#define TARG_FLT128_MIN_EXP targ_flt128_min_exp
#define TARG_FLT128_MAX_EXP targ_flt128_max_exp
#define MSVC_IS_GENERATED_CODE_TARGET msvc_is_generated_code_target
#define MSVC_TARGET_VERSION_NUMBER msvc_target_version_number
#if !IA64_ABI
#define TARG_RUNTIME_ELEM_COUNT_INT_KIND targ_runtime_elem_count_int_kind
#endif /* !IA64_ABI */
#if BACK_END_IS_C_GEN_BE
#define USE_EMPTY_STRUCT_IN_GENERATED_C use_empty_struct_in_generated_c
#if DO_IL_LOWERING
#define TARG_DELTA_INT_KIND targ_delta_int_kind
#define TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND                            \
                        targ_virtual_function_index_int_kind
#if IA64_ABI
#define TARG_IA64_ABI_USE_GUARD_ACQUIRE_RELEASE                               \
        targ_ia64_abi_use_guard_acquire_release
#define TARG_IA64_ABI_USE_INT_STATIC_INIT_GUARD                               \
        targ_ia64_abi_use_int_static_init_guard
#define TARG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES                               \
        targ_ia64_abi_use_variant_array_cookies
#define TARG_IA64_ABI_USE_VARIANT_PTR_TO_MEMBER_FUNCTION_REPR                 \
        targ_ia64_abi_use_variant_ptr_to_member_function_repr
#define TARG_IA64_ABI_VARIANT_CTORS_AND_DTORS_RETURN_THIS                     \
        targ_ia64_abi_variant_ctors_and_dtors_return_this
#define TARG_IA64_ABI_VARIANT_KEY_FUNCTION targ_ia64_abi_variant_key_function
#define TARG_IA64_VTABLE_ENTRY_INT_KIND targ_ia64_vtable_entry_int_kind
#if GENERATE_EH_TABLES
#define TARG_REGION_NUMBER_INT_KIND targ_region_number_int_kind
#define TARG_ETS_FLAG_TYPE_INT_KIND targ_ets_flag_type_int_kind
#endif /* GENERATE_EH_TABLES */
#endif /* IA64_ABI */
#endif /* DO_IL_LOWERING */
#endif /* BACK_END_IS_C_GEN_BE */
#endif /* MAKE_TARG_NAMES_REFER_TO_VARIABLES */

#if MICROSOFT_EXTENSIONS_ALLOWED
extern void init_microsoft_sized_int_types(void);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

a_targ_size_t size_of_pointer_to(a_type_ptr        type_pointed_to,
                                 a_targ_alignment  *alignment);

#if CHECKING
extern void check_target_configuration(void);
#endif /* CHECKING */

#if BACK_END_IS_CP_GEN_BE
extern void select_cp_gen_be_target_dialect(void);
#endif /* BACK_END_IS_CP_GEN_BE */

extern int32_t find_target_configuration(a_const_char *config);

extern void set_target_configuration(int32_t target_index);

#if DUMP_CONFIG_ENABLED
extern void dump_target_configurations(void);

extern void dump_legacy_config_as_target_config(a_const_char *config);
#endif /* DUMP_CONFIG_ENABLED */

extern void target_init(void);

extern void target_early_init(void);

extern void target_one_time_init(void);

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef TARGET_H */


