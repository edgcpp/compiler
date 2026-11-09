/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

folding.c -- Folding routines.

*/

/* Header files common to all files. */
#include "fe_common.h"
/* Header files used by files involved in declaration processing. */
#include "decl_hdrs.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

#include "folding.h"
#include "interpret.h"
#include "layout.h"
#include "exprutil.h"
#if DO_IL_LOWERING
#include "lower_il.h"
#endif /* DO_IL_LOWERING */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
Determine the severity (error or warning) to be used for integer operation
overflows.  This severity may be overridden in issue_folding_diagnostic.
*/
#ifndef ES_INT_OVERFLOW
#if TARG_NO_ERROR_ON_INTEGER_OVERFLOW
#define ES_INT_OVERFLOW                                                       \
  ((constexpr_enabled && !ms_version_is(<=1940)) ? es_error                   \
  :                             strict_ansi_mode ? strict_ansi_error_severity \
  :                                                es_warning)
#else /* !TARG_NO_ERROR_ON_INTEGER_OVERFLOW */
#define ES_INT_OVERFLOW es_error
#endif /* TARG_NO_ERROR_ON_INTEGER_OVERFLOW */
#endif /* ifndef ES_INT_OVERFLOW */

#if FIXED_POINT_ALLOWED
/*
Determine the severity (error or warning) to be used for fixed-point
operation overflows.  This really has to be a warning even in strict
mode (unless we were to add a check for the current pragma state),
because some pragmas mandate saturating behavior (in which overflow
is by definition not an error).
*/
#ifndef ES_FIXED_POINT_OVERFLOW
#define ES_FIXED_POINT_OVERFLOW es_warning
#endif /* ifndef ES_FIXED_POINT_OVERFLOW */
#endif /* FIXED_POINT_ALLOWED */

#if C99_IL_EXTENSIONS_SUPPORTED

static void get_complex_val(a_constant_ptr             con,
                            an_internal_complex_value  *cx_val)
/*
con represents a complex value in ck_complex or ck_aggregate form.
Retrieve the complex value of the constant into *cx_val.
*/
{
  if (constant_is(con, ck_complex)) {
    *cx_val = *con->variant.complex_value;
  } else {
    a_constant_ptr  part;
    check_assertion(constant_is(con, ck_aggregate));
    part = con->variant.aggregate.first_constant;
    check_assertion(constant_is(part, ck_float));
    cx_val->real = part->variant.float_value;
    part = part->next;
    check_assertion(part != NULL && constant_is(part, ck_float));
    cx_val->imag = part->variant.float_value;
  }  /* if */
}  /* get_complex_val */

#endif /* C99_IL_EXTENSIONS_SUPPORTED */

a_boolean variable_has_non_null_address(ARG_UNUSED a_variable_ptr vp)
/*
Return TRUE if the indicated variable has a non-NULL address.  That's usually
TRUE; the exceptions are weak-linkage variables (in principle, even
weak-linkage variables are known to have a non-NULL address if they are known
to be defined, but GCC does not implement that optimization).
*/
{
  a_boolean has_non_null_addr = TRUE;

#if GNU_EXTENSIONS_ALLOWED
  if (vp->is_weak || vp->is_weakref) {
    has_non_null_addr = FALSE;
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  return has_non_null_addr;
}  /* variable_has_non_null_address */


a_boolean routine_has_non_null_address(ARG_UNUSED a_routine_ptr rp)
/*
Return TRUE if the indicated routine has a non-NULL address.  That's usually
TRUE; the exceptions are weak-linkage routines (in principle, even weak-linkage
routines are known to have a non-NULL address if they are known to be defined,
but GCC does not implement that optimization).
*/
{
  a_boolean has_non_null_addr = TRUE;

#if GNU_EXTENSIONS_ALLOWED
  if (rp->is_weak || rp->is_weakref) {
    has_non_null_addr = FALSE;
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  return has_non_null_addr;
}  /* routine_has_non_null_address */


a_boolean constant_bool_value_known_at_compile_time(a_constant_ptr con)
/*
con is a constant of a scalar type.  Return TRUE if the bool value it would
convert to is known at compile time.  (It might not be known if the
constant is an address that is not known until link time.)
*/
{
  a_boolean known_bool = TRUE;

  if (con->kind == (a_constant_repr_kind)ck_address) {
    an_address_base_kind kind = con->variant.address.kind;
    /* Addresses are non-null except possibly for extern variables and
       routines, which might have zero addresses because of linker magic
       like weak externals. */
    if (kind == (an_address_base_kind)abk_variable) {
      a_variable_ptr  vp = con->variant.address.variant.variable;
      known_bool = variable_has_non_null_address(vp);
    } else if (kind == (an_address_base_kind)abk_routine) {
      a_routine_ptr  rp = con->variant.address.variant.routine;
      known_bool = routine_has_non_null_address(rp);
    }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  } else if (con->kind == (a_constant_repr_kind)ck_label_difference) {
    a_constant_ptr  from = con->variant.label_difference.from_address;
    a_constant_ptr  to = con->variant.label_difference.to_address;
    if (constant_is_address_of_label(from) &&
        constant_is_address_of_label(to)) {
      known_bool = from->variant.address.variant.label ==
                                            to->variant.address.variant.label;
    } else {
      known_bool = FALSE;
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  } else if (con->kind == (a_constant_repr_kind)ck_template_param) {
    known_bool = FALSE;
#if UPC_EXTENSIONS_ALLOWED
  } else if (con->kind == (a_constant_repr_kind)ck_upc_mythread ||
             con->kind == (a_constant_repr_kind)ck_upc_threads) {
    /* The UPC pseudo-constants THREADS and MYTHREAD are not true constants. */
    known_bool = FALSE;
#endif /* UPC_EXTENSIONS_ALLOWED */
  }  /* if */
  return known_bool;
}  /* constant_bool_value_known_at_compile_time */


void make_template_param_expr_constant(an_expr_node_ptr node,
                                       a_constant       *con)
/*
Create a template parameter constant that represents the indicated
expression.
*/
{
  clear_constant(con, (a_constant_repr_kind)ck_template_param);
  set_template_param_constant_kind(con,
                              (a_template_param_constant_kind)tpck_expression);
  con->variant.template_param.variant.expr = node;
  con->type = node->type;
}  /* make_template_param_expr_constant */


void force_constant_to_be_dependent(a_constant  *con)
/*
Force the indicated constant to appear template-dependent by wrapping it in a
ck_template_param constant.
*/
{
  a_constant_ptr    wrapped_constant = alloc_shareable_constant(con);
  a_character_kind  character_kind = (a_character_kind)con->character_kind;

  clear_constant(con, ck_template_param);
  set_template_param_constant_kind(con, tpck_dependent_constant);
  con->variant.template_param.variant.constant = wrapped_constant;
  con->type = wrapped_constant->type;
  con->character_kind = character_kind;
}  /* force_constant_to_be_dependent */


void make_template_param_cast_constant(a_constant  *old_constant,
                                       a_constant  *new_constant,
                                       a_type_ptr  new_type,
                                       a_boolean   is_explicit)
/*
Make, in *new_constant, a ck_template_param/tpck_expression constant that
represents *old_constant cast to the type new_type.  is_explicit is set to
TRUE if the cast actually appeared in the source.
*/
{
  a_boolean              is_reference_cast = is_any_reference_type(new_type);
  an_expr_node_ptr       node = alloc_node_for_constant(old_constant),
                         unwrapped_node;
  an_expr_operator_kind  op;

  if (is_reference_cast) {
    op = (an_expr_operator_kind)eok_ref_cast;
  } else {
    op = (an_expr_operator_kind)eok_cast;
  }  /* if */
  /* If the existing constant is already a tpck_expression, get the underlying
     expression.  This is needed to ensure that equivalent expressions compare
     equal independently of whether they were obtained through instantiation
     or substitution.  With locally-allocated expressions this cannot be done
     since it would lead to a memory region violation when we add a file-scope
     memory entry on top of it below. */
  unwrapped_node = unwrap_if_tpck_expression(node);
  if (in_file_scope(unwrapped_node)) {
    node = unwrapped_node;
  }  /* if */
  if (node->compiler_generated &&
      is_operation_node(node) && node_operator_is(node, op)) {
    /* Drop a pre-existing implicit cast.  This is needed for deduction to
       work in some cases. */
    node = node->variant.operation.operands;
  }  /* if */
  node = make_operator_node(op, new_type, node);
  if (!is_explicit) {
    node->compiler_generated = TRUE;
  } else if (expr_stack != NULL && expr_stack->possible_rescan_context) {
    /* Ensure rescan information is recorded. */
    an_operand  opnd;
    make_expression_operand(node, &opnd);
    node = make_node_from_operand(&opnd);
  }  /* if */
  make_template_param_expr_constant(node, new_constant);
  new_constant->variant.template_param.do_not_rescan = TRUE;
}  /* make_template_param_cast_constant */


static void implicit_or_explicit_cast(a_constant_ptr cp,
                                      a_type_ptr     new_type,
                                      a_boolean      is_implicit_cast)
/*
Set the implicit_cast flag to indicate a type change of the indicated
constant to the indicated new type.  No representation change is implied.
The cast is implicit if is_implicit_cast is TRUE.
*/
{
  if (cp->expr != NULL &&
      (!is_implicit_cast || !identical_types(cp->type, new_type))) {
    cp->expr = make_operator_node((an_expr_operator_kind)eok_cast, new_type,
                                  cp->expr);
    cp->expr->compiler_generated = is_implicit_cast;
  }  /* if */
  cp->implicit_cast = TRUE;
  if (!is_implicit_cast) {
    /* Note that the TRUE setting of explicit_cast_applied is sticky. */
    cp->explicit_cast_applied = TRUE;
  }  /* if */
  if (cp->orig_type == NULL) {
    /* Record that some conversions are not normally valid constant-expressions
       (i.e., they're "reinterpret-like") so the interpreter can fail
       evaluation if needed. */
    a_type_ptr  src_tp = skip_typerefs(cp->type),
                dst_tp = skip_typerefs(new_type);
    if (types_are_interpreter_compatible(src_tp, dst_tp)) {
      /* Catch the case where exception specifications are strengthened. */
      if (type_is(src_tp, tk_pointer) && type_is(dst_tp, tk_pointer)) {
        src_tp = skip_typerefs(src_tp->variant.pointer.type);
        dst_tp = skip_typerefs(dst_tp->variant.pointer.type);
        if (type_is(src_tp, tk_routine) && type_is(dst_tp, tk_routine) &&
            !exception_spec_conversion_possible(src_tp, dst_tp)) {
          cp->is_reinterpret_like_cast = TRUE;
        }  /* if */
      }  /* if */
    } else {
      /* The types are fundamentally different to the interpreter.  Even in
         that case, do not mark as "reinterpret-like" the conversion of
         nullptr_t to a pointer or pointer-to-member type, nor of a T* to
         void*. */
      if (type_is(dst_tp, tk_pointer) &&
          (is_pointer_type(src_tp) && is_pointer_to_void_type(dst_tp))) {
        /* A conversion from a T* to a void* type can be handled by the
           interpreter. */
      } else if (type_is(src_tp, tk_nullptr) &&
                 (is_pointer_type(dst_tp) || is_ptr_to_member_type(dst_tp))) {
        /* A conversion from a nullptr_t type to a pointer or pointer-to-member
           type can be handled by the interpreter. */
      } else {
        cp->is_reinterpret_like_cast = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  cp->type = new_type;
  /* Clear the source correspondence information.  If this was a named
     constant, the new constant should no longer be
     associated with the original constant. */
  break_source_corresp(&cp->source_corresp);
}  /* implicit_or_explicit_cast */


void implicit_cast(a_constant_ptr cp,
                   a_type_ptr     new_type)
/*
Do an implicit cast of the indicated constant to the indicated new type.
No representation change is implied.  This is used for casting one
pointer type to another and casting integer constants to pointer types.
*/
{
  implicit_or_explicit_cast(cp, new_type, /*is_implicit_cast=*/TRUE);
}  /* implicit_cast */


void get_integer_attributes(a_constant      *cp,
                            an_integer_kind *ikind,
                            a_boolean       *is_signed,
                            size_t          *bit_size)
/*
For the integer type given by cp->type, return in *ikind the integer kind,
in *is_signed whether or not the type is signed, and in *bit_size the
size in bits of the integral type.
*/
{
  a_type_ptr    int_type = skip_typerefs(cp->type);

  check_assertion_str(type_is(int_type, tk_integer),
                      "get_integer_attributes: not integral type");
  *ikind = int_type->variant.integer.int_kind;
  *is_signed = int_kind_is_signed[*ikind];
  check_assertion_str(is_bit_precise_kind(*ikind) || int_type->size != 0,
                      "get_integer_attributes: zero-sized integer");
  *bit_size = size_t_arg(integer_value_bit_size_for_type(int_type));
}  /* get_integer_attributes */


void trunc_and_set_integer(an_integer_value  *result_value,
                           a_constant        *result,
                           a_boolean         check_overflow,
			   a_boolean	     saturate_on_overflow,
                           an_error_code     *err_code,
                           an_error_severity *err_severity)
/*
Truncate the integer result_value and store it in *result.  result->type
indicates the desired result type.  If check_overflow is TRUE and
*err_code indicates no previous error, check that the value fits in
the result type; if it does not, set *err_code and *err_severity to
indicate the error.  Whether or not the check is done, and whether or
not it succeeds, the value will be adjusted if necessary to ensure
that it fits.  saturate_on_overflow is TRUE if an overflow should
produce the largest (or smallest) value that will fit in the destination
type.
*/
{
  an_integer_kind  ikind;
  a_boolean        is_signed;
  size_t           bit_size;
  an_integer_value mask, min_value, max_value;

  /* Put the integer value into the result constant. */
  set_constant_kind(result, ck_integer);
  result->variant.integer_value = *result_value;
  get_integer_attributes(result, &ikind, &is_signed, &bit_size);
  /* Do the overflow check if necessary and if there's been no previous
     error. */
  if (integer_constant_in_range_for_type(result, result, result->type)) {
    /* The value is in the right range.  No truncation is needed. */
    goto after_truncation;
  }  /* if */
  /* The value will not fit in the destination integer type. */
  if (check_overflow && *err_code == ec_no_error) {
    /* Return an error code if the caller requested that we check for
       overflow. */
    *err_code = ec_integer_overflow;
    *err_severity = ES_INT_OVERFLOW;
  }  /* if */
  if (!integer_value_can_represent_type_width(result->type)) {
    if (*err_code == ec_no_error) {
      *err_code = ec_integer_overflow;
      *err_severity = ES_INT_OVERFLOW;
    }  /* if */
    goto after_truncation;
  }  /* if */
  /* Truncate the value to the right size.  When saturate_on_overflow is
     TRUE, return the largest or smallest value that can be represented. */
  if (saturate_on_overflow) {
    integer_value_range_for_type(result->type, &min_value, &max_value);
    if (sign_of_integer_constant(result) < 0) {
      result->variant.integer_value = min_value;
    } else {
      result->variant.integer_value = max_value;
    }  /* if */
  } else {
    make_integer_value_mask(&mask, bit_size);
    and_integer_values(&result->variant.integer_value, &mask);
  }  /* if */
  /* Sign-extend a signed result. */
  if (is_signed) {
    sign_extend_integer_value(&result->variant.integer_value, bit_size);
  }  /* if */
after_truncation:;
}  /* trunc_and_set_integer */


void conv_integer_to_integer(a_constant        *old_constant,
                             a_constant        *new_constant,
                             a_boolean         is_implicit_cast,
                             an_error_code     *err_code,
                             an_error_severity *err_severity)
/*
Convert an integral constant of some kind (in *old_constant) to a new
integral constant in *new_constant, with type as indicated therein.  Return
*err_code and *err_severity set to indicate any error/warning detected,
or *err_code == ec_no_error if everything went fine.  If is_implicit_cast
is FALSE, suppress any warnings.  The old_constant can have kind ck_integer,
ck_upc_threads, or ck_label_difference.  Generally it must have integral
type, but it may be an integer cast to a pointer type.
*/
{
  an_integer_value mask, old_value_copy;
  an_integer_kind  new_ikind, old_ikind;
  a_boolean        new_signed, old_signed;
  size_t           new_bit_size, old_bit_size;
  a_boolean        is_sign_change;

  *err_code = ec_no_error;
  *err_severity = es_warning;
  /* Copy the old value to the new value. */
  switch (old_constant->kind) {
    case ck_integer:
      set_constant_kind(new_constant, ck_integer);
      break;
#if GNU_EXTENSIONS_ALLOWED
    case ck_label_difference:
      check_assertion(gnu_mode);
      set_constant_kind(new_constant,
                        (a_constant_repr_kind)ck_label_difference);
      /* This constant entry doesn't use variant.integer_value; so don't copy
         that variant field.  Instead copy the variant.label_difference
         part. */
      new_constant->variant.label_difference.from_address =
                          old_constant->variant.label_difference.from_address;
      new_constant->variant.label_difference.to_address =
                            old_constant->variant.label_difference.to_address;
      goto done;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if UPC_EXTENSIONS_ALLOWED
    case ck_upc_threads:
      check_assertion(upc_mode);
      set_constant_kind(new_constant,
                        (a_constant_repr_kind)ck_upc_threads);
      break;
#endif /* UPC_EXTENSIONS_ALLOWED */
    default:
      unexpected_condition();
  }  /* switch */
  new_constant->variant.integer_value = old_constant->variant.integer_value;
  /* Determine attributes (size, signedness) of the new integer kind. */
  get_integer_attributes(new_constant, &new_ikind, &new_signed, &new_bit_size);
  /* Truncate the new value to the right size. */
  /* Note that the mask created here is used again later in this routine. */
  if (new_bit_size < BITS_IN_AN_INTEGER_VALUE) {
    make_integer_value_mask(&mask, new_bit_size);
    and_integer_values(&new_constant->variant.integer_value, &mask);
    /* Sign-extend the new value if necessary. */
    if (new_signed) {
      sign_extend_integer_value(&new_constant->variant.integer_value,
                                new_bit_size);
    }  /* if */
  } else {
    set_integer_value(&mask, (a_host_large_integer)0);
    complement_integer_value(&mask);
  }  /* if */
  if (is_implicit_cast) {
    /* If the value changed, a warning is in order. */
    if (cmp_integer_constants(new_constant, old_constant) != 0 &&
        /* In some modes (e.g., Microsoft C mode), it is possible to
           implicitly convert a pointer to an integer type, so the old
           constant could be something like (void *)1.  Avoid the
           checking in such cases. */
        !is_pointer_type(old_constant->type)) {
      /* The new value is different than the old value.  See if the change
         is a truncation (dropping bits) or a sign change. */
      is_sign_change = FALSE;
      get_integer_attributes(old_constant, &old_ikind, &old_signed,
                             &old_bit_size);
      if (new_bit_size >= old_bit_size) {
        /* The new size is at least as big as the old size, so no truncation
           is possible.  Therefore, this must be a sign change. */
        is_sign_change = TRUE;
      } else if (new_bit_size >= BITS_IN_AN_INTEGER_VALUE) {
        /* The destination is narrower than the source, but it still includes
           every value bit that an_integer_value can currently store. */
        is_sign_change = TRUE;
      } else {
        /* The new size is smaller than the old size, which means truncation
           is possible.  See if the significant part of the new value is the
           same as the old value.  If so, no bits have been lost, and
           this is a sign change. */
        old_value_copy = old_constant->variant.integer_value;
        if (old_signed && sign_of_integer_constant(old_constant) < 0) {
          /* Old constant is negative.  Turn on all the bits of the copy of
             the old constant down to where the sign bit is (or would be) in
             the new size.  If that gives a value that is equal to the
             old constant, then no information was lost, i.e., there is
             no interesting information -- just sign extension -- in the
             bits that don't fit into the new size. */
          make_integer_value_mask(&mask, new_bit_size-1);
          complement_integer_value(&mask);
          or_integer_values(&old_value_copy, &mask);
        } else {
          /* Old constant is unsigned or nonnegative.  Mask off all the
             bits of the old constant that do not fit in the new size.
             If that gives a value that is equal to the old constant,
             then no information was lost. */
          /* make_integer_value_mask(&mask, new_bit_size); -- already set. */
          and_integer_values(&old_value_copy, &mask);
        }  /* if */
        if (cmp_integer_values(&old_value_copy, old_signed,
                               &old_constant->variant.integer_value,
                               old_signed) == 0) {
          /* No significant bits were dropped, so this must be a sign
             change. */
          is_sign_change = TRUE;
        }  /* if */
      }  /* if */
      if (is_sign_change) {
        /* Sign change. */
        /* Do not issue this warning for non-arithmetic constants. */
        if (!old_constant->non_arithmetic) {
          *err_code = ec_integer_sign_change;
          *err_severity = es_warning;
        }  /* if */
      } else {
        /* Truncation. */
        *err_code = ec_integer_truncated;
        *err_severity = es_warning;
      }  /* if */
    }  /* if */
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
done:;
#endif /* GNU_EXTENSIONS_ALLOWED */
}  /* conv_integer_to_integer */


static void conv_integer_to_float(a_constant        *old_constant,
			          a_constant        *new_constant,
			          an_error_code     *err_code,
			          an_error_severity *err_severity)
/*
Convert an integer of some kind (in *old_constant) to a float constant
in *new_constant, with type as indicated therein.  Return *err_code and
*err_severity set to indicate any error/warning detected, or
*err_code == ec_no_error if everything went fine.
*/
{
  a_boolean               err;
  a_type_ptr              float_tp = skip_typerefs(new_constant->type);
  a_constant_repr_kind    constant_kind = (a_constant_repr_kind)ck_float;
  a_float_kind            float_kind = float_tp->variant.float_kind;
  an_internal_float_value *float_value;

  *err_code = ec_no_error;
  *err_severity = es_warning;
#if C99_IL_EXTENSIONS_SUPPORTED
  /* We may be converting to a nonreal floating type. */
  if (float_tp->kind == (a_type_kind)tk_complex) {
    constant_kind = (a_constant_repr_kind)ck_complex;
  } else if (float_tp->kind == (a_type_kind)tk_imaginary) {
    constant_kind = (a_constant_repr_kind)ck_imaginary;
  }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */

  set_constant_kind(new_constant, constant_kind);

#if C99_IL_EXTENSIONS_SUPPORTED
  if (float_tp->kind == (a_type_kind)tk_complex) {
    /* Converting to complex.  The integer value is converted into the real
       part, and the imaginary part is set to zero. */
    float_value = &new_constant->variant.complex_value->real;
    fp_host_large_integer_to_float(float_kind, (a_host_large_integer)0,
                                   &new_constant->variant.complex_value->imag,
                                   &err);
    check_assertion_str2(!err, "conv_integer_to_float: cannot create zero",
                               "floating-point representation");
  } else if (float_tp->kind == (a_type_kind)tk_imaginary) {
    /* Converting to imaginary.  The result is zero. */
    fp_host_large_integer_to_float(float_kind, (a_host_large_integer)0,
                                   &new_constant->variant.float_value, &err);
    check_assertion_str2(!err, "conv_integer_to_float: cannot create zero",
                               "floating-point representation");
    goto conversion_done;
  } else
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  /* Do not insert code here. */
  {
    float_value = &new_constant->variant.float_value;
  }  /* if */
  /* Do the actual conversion of the integer constant to a float value. */
  conv_integer_value_to_float(&old_constant->variant.integer_value,
                              int_constant_is_signed(old_constant),
                              float_value, float_kind, &err);
#if C99_IL_EXTENSIONS_SUPPORTED
conversion_done:;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */

  if (err) {
    /* Some error. */
    *err_code = ec_integer_to_float_conversion;
    *err_severity = es_error;
  }  /* if */
}  /* conv_integer_to_float */


a_boolean conv_float_value_to_int_value(
                                  an_internal_float_value  *float_value,
                                  a_float_kind             float_kind,
                                  an_integer_value         *result_value,
                                  a_boolean                is_signed,
                                  a_boolean                *depends_on_fp_mode)
/*
Convert the given floating-point value (of the given kind) to an integer value
of the given signedness stored in *result_value if possible.  If successful,
return TRUE and set *depends_on_fp_mode to indicate whether the result depends
on the floating-point mode.  Otherwise, return FALSE.
*/
{
  a_boolean  err = FALSE;
  a_boolean  is_negative = fp_is_negative(float_kind, float_value);

  if (is_signed || is_negative) {
    /* Destination is a signed integer or the source value is negative.
       When the source value is negative, we convert to a signed value
       because we may use the resulting bit pattern as an unsigned value. */
    a_host_large_integer  int_value;
    fp_to_host_large_integer(float_kind, float_value,
                             &int_value, &err, depends_on_fp_mode);
    /* We set the result value even if an error occurred.  This value
       is used in some modes. */
    set_integer_value(result_value, int_value);
    if (!is_signed && sign_of(*result_value)) {
      /* Set the error flag if the converted source value would be negative
         and the result was intended to be unsigned. */
      err = TRUE;
    }  /* if */
  } else {
    /* Destination is an unsigned integer. */
    a_host_large_unsigned  unsigned_int_value;
    fp_to_host_large_unsigned(float_kind, float_value,
                              &unsigned_int_value, &err,
                              depends_on_fp_mode);
    /* We set the result value even if an error occurred.  This value
       is used in some modes. */
    set_unsigned_integer_value(result_value, unsigned_int_value);
  }  /* if */
#if !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER
  if (err) {
    /* Try again with larger precision by converting the float to a
       character string then converting the string to an integer value. */
    a_boolean       pos_infinity, neg_infinity, not_a_number;
    a_number_buffer str = fp_to_string(float_kind, float_value,
                                       &pos_infinity, &neg_infinity,
                                       &not_a_number);

    if (pos_infinity || neg_infinity || not_a_number) {
      err = TRUE;
    } else {
      if (!is_signed && is_negative) {
        /* The source value is negative but the result value is unsigned.
           Do the conversion to a signed value because the resulting bit
           pattern may be used later in some modes. */
        conv_float_string_to_integer_value(str.as_temp_characters(),
                                           result_value,
                                           /*is_signed=*/TRUE, &err);
        /* Always set the error flag in this case. */
        err = TRUE;
      } else {
        conv_float_string_to_integer_value(str.as_temp_characters(),
                                           result_value,
                                           is_signed, &err);
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
  return !err;
}  /* conv_float_value_to_int_value */


void conv_float_to_integer(a_constant        *old_constant,
                           a_constant        *new_constant,
                           an_error_code     *err_code,
                           an_error_severity *err_severity,
                           a_boolean         *depends_on_fp_mode,
                           a_boolean         constant_context)
/*
Convert a float of some kind (in *old_constant) to an integer constant
in *new_constant, with type as indicated therein.  Return *err_code and
*err_severity set to indicate any error/warning detected, or
*err_code == ec_no_error if everything went fine.  *depends_on_fp_mode
is returned TRUE if the result has been determined but might be different
depending on the floating-point mode.  If constant_context is FALSE, this
operation is being evaluated as part of a nonconstant expression.
*/
{
  an_integer_value          result_value;
  a_boolean                 err, is_signed;
  a_type_ptr                float_tp = skip_typerefs(old_constant->type);
  a_float_kind              float_kind = float_tp->variant.float_kind;
  an_internal_float_value   *float_value;
#if C99_IL_EXTENSIONS_SUPPORTED
  an_internal_float_value   zero;
  an_internal_complex_value  cx;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */

#if C99_IL_EXTENSIONS_SUPPORTED
  if (float_tp->kind == (a_type_kind)tk_complex) {
    /* Converting from complex to integer.  The real part of the
       constant is converted to integer, and the imaginary part is
       discarded. */
    get_complex_val(old_constant, &cx);
    float_value = &cx.real;
  } else if (float_tp->kind == (a_type_kind)tk_imaginary) {
    /* Converting from imaginary to integer.  The result is zero. */
    fp_host_large_integer_to_float(float_kind, (a_host_large_integer)0,
                                   &zero, &err);
    float_value = &zero;
  } else
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  /* Do not insert code here. */
  {
    float_value = &old_constant->variant.float_value;
  }  /* if */

  *err_code = ec_no_error;
  *err_severity = es_warning;

  is_signed = int_constant_is_signed(new_constant);
  err = !conv_float_value_to_int_value(float_value, float_kind, &result_value,
                                       is_signed, depends_on_fp_mode);
  if (err && gcc_mode && gnu_version >= 30400) {
    /* The float value cannot be represented as an integer value (or an
       unsigned integer value).  Use the largest or smallest (depending on
       the sign of the float) value that can be represented. */
    make_saturated_integer_for_float(float_kind, float_value, &result_value,
                                     new_constant);
  }  /* if */
  trunc_and_set_integer(&result_value, new_constant,
                        /*check_overflow=*/!err,
                        /*saturate_on_overflow=*/gcc_mode &&
                                                 gnu_version >= 30400,
                        err_code, err_severity);
  if (err || *err_code != ec_no_error) {
    /* Float value is too big to fit in the integer. */
    *err_code = ec_float_to_integer_conversion;
    /* In GNU C and Microsoft C mode, only give a warning on an out-of-range
       value in a constant context.  An es_error severity is returned in
       non-constant contexts.  This causes the folded value to be discarded
       and the operation to be evaluated at run time.  In such cases the
       severity is reduced to a warning by issue_folding_diagnostic. */
    *err_severity = constant_context &&
                    (gcc_mode || (microsoft_mode && C_mode())) ? es_warning
                                                               : es_error;
  }  /* if */
}  /* conv_float_to_integer */


static void conv_float_to_float(a_constant           *old_constant,
			        a_constant           *new_constant,
			        an_error_code        *err_code,
				an_error_severity    *err_severity,
                                a_boolean            *depends_on_fp_mode)
/*
Convert a float of some kind (in *old_constant) to a float constant
in *new_constant, with type as indicated therein.  Return *err_code and
*err_severity set to indicate any error/warning detected, or
*err_code == ec_no_error if everything went fine.  *depends_on_fp_mode
is returned TRUE if the result has been determined but might be different
depending on the floating-point mode.
*/
{
  a_boolean            err;
  a_type_ptr           old_type = skip_typerefs(old_constant->type);
  a_type_ptr           new_type = skip_typerefs(new_constant->type);
  a_float_kind         old_kind = old_type->variant.float_kind;
  a_float_kind         new_kind = new_type->variant.float_kind;
  a_constant_repr_kind new_constant_kind = (a_constant_repr_kind)ck_float;

  *err_code = ec_no_error;
  *err_severity = es_warning;

#if C99_IL_EXTENSIONS_SUPPORTED
  /* We may be converting to a nonreal floating type. */
  if (new_type->kind == (a_type_kind)tk_complex) {
    new_constant_kind = (a_constant_repr_kind)ck_complex;
  } else if (new_type->kind == (a_type_kind)tk_imaginary) {
    new_constant_kind = (a_constant_repr_kind)ck_imaginary;
  }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */

  set_constant_kind(new_constant, new_constant_kind);

#if C99_IL_EXTENSIONS_SUPPORTED
  if ((old_type->kind != (a_type_kind)tk_float ||
       new_type->kind != (a_type_kind)tk_float) &&
      (old_type->kind != (a_type_kind)tk_imaginary ||
       new_type->kind != (a_type_kind)tk_imaginary)) {
    /* Conversion involving complex or imaginary types, but not the
       simple imaginary --> imaginary case. */
    switch (old_type->kind) {
      case tk_float:
        switch (new_type->kind) {
          case tk_imaginary:
            /* Float to imaginary.  The result is zero. */
            fp_host_large_integer_to_float(new_kind, (a_host_large_integer)0,
                                           &new_constant->variant.float_value,
                                           &err);
            break;
          case tk_complex:
            /* Float to complex. */
            fp_change_kind(&old_constant->variant.float_value, old_kind,
                           &new_constant->variant.complex_value->real,
                           new_kind, &err, depends_on_fp_mode);
            fp_host_large_integer_to_float(
                                    new_kind, (a_host_large_integer)0,
                                    &new_constant->variant.complex_value->imag,
                                    &err);
            break;
          default:
            unexpected_condition_str(
                                "conv_float_to_float: from float to bad type");
        }  /* switch */
        break;
      case tk_imaginary:
        switch (new_type->kind) {
          case tk_float:
            /* Imaginary to float.  Result is zero. */
            fp_host_large_integer_to_float(new_kind, (a_host_large_integer)0,
                                           &new_constant->variant.float_value,
                                           &err);
            break;
          case tk_complex:
            /* Imaginary to complex. */
            fp_host_large_integer_to_float(
                                    new_kind, (a_host_large_integer)0,
                                    &new_constant->variant.complex_value->real,
                                    &err);
            fp_change_kind(&old_constant->variant.float_value, old_kind,
                           &new_constant->variant.complex_value->imag,
                           new_kind, &err, depends_on_fp_mode);
            break;
          default:
            unexpected_condition_str(
                            "conv_float_to_float: from imaginary to bad type");
        }  /* switch */
        break;
      case tk_complex:
        { an_internal_complex_value  cx;
          get_complex_val(old_constant, &cx);
          switch (new_type->kind) {
            case tk_float:
              /* Complex to float.  Retain the real part only. */
              fp_change_kind(&cx.real, old_kind,
                             &new_constant->variant.float_value, new_kind,
                             &err, depends_on_fp_mode);
              break;
            case tk_imaginary:
              /* Complex to imaginary.  Retain the imaginary part only. */
              fp_change_kind(&cx.imag, old_kind,
                             &new_constant->variant.float_value, new_kind,
                             &err, depends_on_fp_mode);
              break;
            case tk_complex:
              /* Complex to complex. */
              /* This is similar to the float-float or imaginary-imaginary
                 cases, but both the real and the imaginary components must
                 change. */
              fp_change_kind(&cx.real, old_kind,
                             &new_constant->variant.complex_value->real,
                             new_kind, &err, depends_on_fp_mode);
              fp_change_kind(&cx.imag, old_kind,
                             &new_constant->variant.complex_value->imag,
                             new_kind, &err, depends_on_fp_mode);
              break;
            default:
              unexpected_condition_str(
                              "conv_float_to_float: from complex to bad type");
          }  /* switch */
        }
        break;
      default:
        unexpected_condition_str(
                               "conv_float_to_float: bad floating-point type");
    }  /* switch */
  } else
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  {
    /* Singular floating-point types in the same domain (i.e., two real or
       imaginary constants). */
    fp_change_kind(&old_constant->variant.float_value, old_kind,
                   &new_constant->variant.float_value, new_kind,
                   &err, depends_on_fp_mode);
    if (err && C_mode() && !strict_ansi_mode &&
        curr_expr_kind_is(ek_init_constant)) {
      /* GCC and Clang issue a warning but not an error in this case. */
      *err_code = ec_float_to_float_conversion;
      err = FALSE;
    }  /* if */
  }  /* if */
  if (err) {
    *err_code = ec_float_to_float_conversion;
    *err_severity = es_error;
  }  /* if */
}  /* conv_float_to_float */


static void get_pointer_offset(a_constant_ptr constant,
                               a_constant_ptr offset)
/*
Retrieve and return the offset part of the given pointer constant in
integer constant form.  Note that this routine works when
applied to an address constant that has been cast to an integral type.
*/
{
  switch (constant->kind) {
    case ck_address:
      /* Address of a routine, variable, or constant, plus some offset. */
      set_integer_constant(
                offset, (a_host_large_integer)constant->variant.address.offset,
                targ_ptrdiff_t_int_kind);
      break;
    case ck_integer:
      /* Integer cast to a pointer type (probably 0/NULL). */
      *offset = *constant;
      break;
    default:
      unexpected_condition_str("get_pointer_offset: bad kind");
  }  /* switch */
}  /* get_pointer_offset */


static void set_pointer_offset(a_constant_ptr constant,
                               a_constant_ptr offset,
                               a_boolean      *err)
/*
Put the indicated offset into the pointer constant.  Return *err TRUE
if the value will not fit in the pointer constant.  Note that this routine
works when applied to an address constant that has been cast to an
integral type.
*/
{
  switch (constant->kind) {
    case ck_address:
      constant->variant.address.offset= value_of_integer_constant(offset, err);
      break;
    case ck_integer:
      *constant = *offset;
      break;
    default:
      unexpected_condition_str(
                              "set_pointer_offset: bad pointer constant kind");
  }  /* switch */
}  /* set_pointer_offset */


static char *base_object(a_constant  *constant,
                         a_boolean   *unknown)
/*
Return a pointer to the "base object" that underlies the pointer constant.
This is NULL if the pointer is an integer cast to a pointer type, or if it
is an abk_param_ref (there is no IL entity for that parameter).  Otherwise,
it points to the variable, routine, or constant entry.  There are exceptions
whose "base object" is considered unknown, and for which *unknown is set to
TRUE (it is left unchanged otherwise): pointers to "weak" variables or
functions, constants standing for the object a reference or a "this"
pointer of unknown value designates, and abk_param_ref constants.
*/
{
  char *object = NULL;

  if (constant->kind == (a_constant_repr_kind)ck_integer) {
    /* No base object. */
    object = NULL;
  } else {
#if CHECKING
    if (constant->kind != (a_constant_repr_kind)ck_address) {
      internal_error("base_object: not ck_integer or ck_address");
    }  /* if */
#endif /* CHECKING */
    switch (constant->variant.address.kind) {
      case abk_variable:
        { a_variable_ptr  vp = constant->variant.address.variant.variable;
          object = (char *)vp;
          if (is_any_reference_type(vp->type) || vp->is_this_parameter) {
            /* The constant stands for the object a reference or a "this"
               pointer designates, which is not known where the reference or
               pointer itself does not have a constant value. */
            *unknown = TRUE;
          }  /* if */
#if GNU_EXTENSIONS_ALLOWED
          if (vp->is_weak) {
            *unknown = TRUE;
          }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
        }
        break;
      case abk_routine:
        { a_routine_ptr  rp = constant->variant.address.variant.routine;
          object = (char *)rp;
#if GNU_EXTENSIONS_ALLOWED
          if (rp->is_weak) {
            *unknown = TRUE;
          }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
        }
        break;
      case abk_constant:
      case abk_temporary:
        object = (char *)constant->variant.address.variant.constant;
        break;
      case abk_typeid:
      case abk_uuidof:
#if MICROSOFT_EXTENSIONS_ALLOWED
      case abk_cli_typeid:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        object = (char *)constant->variant.address.variant.type;
        break;
#if MICROSOFT_EXTENSIONS_ALLOWED
      case abk_cli_array:
        /* Use the address constant as the "base object" for a C++/CLI
           constant array construct.  It's weird, but we need to return a
           non-NULL base object for this case, and the constant seems like
           the best of the possibilities. */
        object = (char *)constant;
        break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      case abk_label:
        object = (char *)constant->variant.address.variant.label;
        break;
      case abk_param_ref:
        /* There is no IL entity for the parameter. */
        *unknown = TRUE;
        break;
      default:
        unexpected_condition_str("base_object: bad address constant kind");
    }  /* switch */
  }  /* if */
  return object;
}  /* base_object */


static a_boolean same_param_ref_base(a_constant_ptr  cp1,
                                     a_constant_ptr  cp2)
/*
Return TRUE if cp1 and cp2 are both abk_param_ref address constants for the
same parameter.
*/
{
  a_boolean  result = FALSE;

  if (constant_is(cp1, ck_address) && constant_is(cp2, ck_address) &&
      address_base_is(cp1, abk_param_ref) &&
      address_base_is(cp2, abk_param_ref) &&
      cp1->variant.address.variant.param_ref.param_num ==
                           cp2->variant.address.variant.param_ref.param_num) {
    result = TRUE;
  }  /* if */
  return result;
}  /* same_param_ref_base */


static a_boolean same_address_base(a_constant_ptr  cp1,
                                   a_constant_ptr  cp2,
                                   a_boolean       *unknown_base)
/*
cp1 and cp2 are address constants (ck_address or ck_integer).  Return TRUE if
they have the same address base entity.  If one of the base entities is
unknown, return FALSE and set *unknown_base to TRUE; otherwise set
*unknown_base to FALSE.
*/
{
  char       *base_1, *base_2;
  a_boolean  result;

  *unknown_base = FALSE;
  base_1 = base_object(cp1, unknown_base);
  base_2 = base_object(cp2, unknown_base);
  if (*unknown_base) {
    /* Constexpr-unknown addresses (P2280R4) of the same entity are the
       same base; distinct unknown entities are not comparable.  Clear
       *unknown_base when they match so callers such as do_pdiff do not
       treat a successful same-base result as un-foldable. */
    if (reference_to_unknown_object_allowed) {
      result = (base_1 != NULL && base_1 == base_2) ||
               same_param_ref_base(cp1, cp2);
      if (result) *unknown_base = FALSE;
    } else {
      result = FALSE;
    }  /* if */
  } else if (base_1 == base_2) {
    result = TRUE;
  } else {
    if (constant_is(cp1, ck_address) && constant_is(cp2, ck_address) &&
        cp1->variant.address.kind == cp2->variant.address.kind) {
      if (address_base_is(cp1, abk_constant)) {
        a_constant_ptr  base_cp1 = cp1->variant.address.variant.constant;
        a_constant_ptr  base_cp2 = cp2->variant.address.variant.constant;
        if (constant_is(base_cp1, ck_string) &&
            constant_is(base_cp2, ck_string) &&
            base_cp1->variant.string.value == base_cp2->variant.string.value) {
          result = TRUE;
        } else {
          result = FALSE;
        }  /* if */
      } else if (address_base_is(cp1, abk_typeid) ||
                 address_base_is(cp1, abk_uuidof)
                 if_microsoft_extensions(
                   || address_base_is(cp1, abk_cli_typeid))) {
        result = identical_types(cp1->variant.address.variant.type,
                                 cp2->variant.address.variant.type);
      } else {
        result = FALSE;
      }  /* if */
    } else {
      result = FALSE;
    }  /* if */
  }  /* if */
  return result;
}  /* same_address_base */


static void implicit_or_explicit_base_cast(a_constant_ptr  cp,
                                           a_type_ptr      new_type,
                                           a_boolean       is_implicit_cast)
/*
cp is a constant representing a folded base class cast.  new_type is the base
class type (with matching qualifiers applied).  is_implicit_cast is TRUE if
the cast is implicit.  Update some flags and the type of the constant.  This
is similar to implicit_or_explicit_cast.
*/
{
  a_type_ptr  prev_type = skip_typerefs(cp->type);

  check_assertion(prev_type->kind == (a_type_kind)tk_pointer);
  if (prev_type->variant.pointer.is_reference) {
    if (prev_type->variant.pointer.is_rvalue_reference) {
      new_type = make_rvalue_reference_type(new_type);
    } else {
      new_type = make_reference_type(new_type);
    }  /* if */
  } else {
    new_type = make_pointer_type(new_type);
  }  /* if */
  cp->type = new_type;
  cp->implicit_cast = TRUE;
  if (!is_implicit_cast) {
    /* Note that the TRUE setting of explicit_cast_applied is sticky. */
    cp->explicit_cast_applied = TRUE;
  }  /* if */
}  /* implicit_or_explicit_base_cast */


void fold_base_class_cast(a_constant        *constant_1,
                          a_base_class      *bcp,
                          a_type_ptr        qualifiers_model,
                          a_constant        *result,
                          a_boolean         check_cast_access,
                          a_boolean         check_ambiguity,
                          a_boolean         is_implicit_cast,
                          a_boolean         is_object_pointer,
                          a_boolean         omit_backing_expr,
                          a_boolean         *did_not_fold,
                          a_source_position *err_pos,
                          an_error_code     *error_detected)
/*
Fold a C++ cast of a class pointer to a base class pointer.  constant_1 is
an address of a class object or an error constant.  In the former case, it
is converted to a pointer to the base class indicated by bcp and the new
constant is returned in *result.  qualifiers_model is a class type whose
cv-qualification indicates the cv-qualification desired on the result
(i.e., the result type is the base class type of bcp and the cv-qualifiers
of qualifiers_model).  result->type need not be set on entry.  Do access
control on the cast if check_cast_access is TRUE.  Check for ambiguity on the
cast if check_ambiguity is TRUE.  The cast is implicit if is_implicit_cast is
TRUE.  The pointer is known to point to an object if is_object_pointer is TRUE.
If omit_backing_expr is TRUE, do not record a backing expression.  If the
operation cannot be folded, *did_not_fold is returned TRUE.  If there is an
error, issue it at *err_pos.  If error_detected is non-NULL, set
*error_detected to the code for any error detected, and do not issue the
diagnostic, or set it to ec_no_error if there was no error.  An error constant
is (successfully) folded to another error constant.
*/
{
  a_boolean             err;
  a_type_ptr            orig_type, curr_type, new_type;
  an_integer_value      base_class_offset;
  a_base_class_ptr      base_class;

  *did_not_fold = FALSE;
  if (error_detected != NULL) *error_detected = ec_no_error;
  /* The code here looks like add_base_class_casts. */
  if (bcp->ambiguous && check_ambiguity) {
    /* The base class is ambiguous. */
    if (error_detected != NULL) {
      *error_detected = ec_ambiguous_base_class;
    } else {
      pos_ty_error(ec_ambiguous_base_class, err_pos, bcp->type);
    }  /* if */
    set_error_constant(result);
  } else if (bcp->derivation == NULL) {
    /* Do not fold in the case of a dummy base class invented for a projection
       of a member of a nonreal class into another class, e.g., via a
       using-declaration. */
    *did_not_fold = TRUE;
  } else if (constant_1->kind == (a_constant_repr_kind)ck_template_param) {
    /* Can't fold a dependent case. */
    *did_not_fold = TRUE;
  } else if (is_error_constant(constant_1)) {
    /* An upstream error occurred.  Just propagate the error constant as
       the result. */
    set_error_constant(result);
  } else {
    a_constant_ptr         offset = local_constant();
    an_expr_node_ptr       expr = constant_1->expr;
    a_derivation_step_ptr  dsp, tail;
    constant_1->expr = NULL;
    copy_constant(constant_1, result);
    /* Loop through the classes between the derived class and the
       base class.  Check accessibility at each step and generate the
       necessary casts. */
    /* No access checking in prototype instantiations. */
    if (in_front_end &&
        scope_stack[depth_scope_stack].in_prototype_instantiation) {
      check_cast_access = FALSE;
    }  /* if */
    orig_type = type_pointed_to(constant_1->type);
    curr_type = skip_typerefs(orig_type);
    tail = bcp->derivation->path_tail;
    for (dsp = cast_derivation_path_of(bcp);
         dsp != tail->next;
         dsp = dsp->next) {
      base_class = dsp->base_class;
      /* Check that the base class is accessible from the current class.
         Accessibility is not checked if the cast is explicit. */
      if (check_cast_access) {
        if (!is_accessible_imm_base_class(base_class, curr_type, bcp)) {
          /* The base class is inaccessible. */
          if (error_detected != NULL) {
            if (is_effective_sfinae_error(ec_inaccessible_base_class,
                                          es_discretionary_error, err_pos)) {
              *error_detected = ec_inaccessible_base_class;
            }  /* if */
          } else {
            pos_ty_diagnostic(es_discretionary_error,
                              ec_inaccessible_base_class, err_pos,
                              base_class->type);
          }  /* if */
          /* Keep going, but don't check access any further to avoid putting
             out more than one error. */
          check_cast_access = FALSE;
        }  /* if */
      }  /* if */
      /* Adjust the address to reflect the cast to the next level. */
      curr_type = base_class->type;
      if (!is_object_pointer &&
          is_null_pointer_value(constant_1)) {
        /* Preserve a NULL pointer.  Note that we suppress this test when
           is_object_pointer is TRUE, to allow the usual idiom for the
           offsetof macro to work. */
      } else {
        a_base_class_ptr  prev_base = NULL;
        a_targ_size_t     base_offset;
        a_boolean         subtract = FALSE;
        a_subobject_path_ptr
                          spp = NULL;
        if (constant_is(result, ck_address)) {
          /* Update the subobject path. */
          result->variant.address.subobject_path =
                  copy_subobject_path(result->variant.address.subobject_path);
          spp = get_trailing_subobject_path_entry(result, /*is_offset=*/FALSE,
                                                  /*is_base_class=*/TRUE);
          prev_base = spp->variant.base_class;
          if (prev_base == NULL) {
            spp->variant.base_class = base_class;
          } else {
            spp->variant.base_class = corresponding_base_class(
                                         base_class, prev_base->derived_class,
                                         prev_base);
          }  /* if */
        }  /* if */
        get_pointer_offset(constant_1, offset);
        if (!any_virtual_steps_in_derivation(base_class)) {
          base_offset = base_class->offset;
        } else {
          /* Casting to a virtual base class.  This can only be folded if we
             know the "most derived" object type. */
          if (pointer_con_complete_object_type(constant_1) != NULL) {
            /* The constant is the unmodified address of a variable.  We know
               the variable has the proper class type or we wouldn't have
               identified the cast as a base class cast.  We don't try to
               handle any cases where the address has been cast to another
               type because we don't have the history of casts -- there may
               have been several, and they might not all have been base
               class casts. */
            base_offset = base_class->offset;
          } else if (!constant_is(result, ck_address)) {
            /* We cannot fold the cast because the most derived type is not
               known. */
            *did_not_fold = TRUE;
            break;
          } else {
            /* A subobject path tracks the base class of the most-derived type.
               That is sufficient to determine the actual offset needed. */
            a_base_class_ptr  new_base = spp->variant.base_class;
            if (prev_base == NULL) {
              base_offset = new_base->offset;
            } else if (prev_base->offset <= new_base->offset) {
              base_offset = new_base->offset - prev_base->offset;
            } else {
              subtract = TRUE;
              base_offset = prev_base->offset - new_base->offset;
            }  /* if */
          }  /* if */
        }  /* if */
        /* Take the pointer offset, ... */
        set_unsigned_integer_value(&base_class_offset, base_offset);
        /* ... add or subtract the offset to the base class, ... */
        if (subtract) {
          subtract_integer_values(&offset->variant.integer_value,
                                  &base_class_offset,
                                  int_constant_is_signed(offset), &err);
        } else {
          add_integer_values(&offset->variant.integer_value,
                             &base_class_offset,
                             int_constant_is_signed(offset), &err);
        }  /* if */
        /* ... and put the offset into the result pointer constant.  Note
           that no overflow/object-size checking is needed, since the base
           class has to be within the underlying object. */
        set_pointer_offset(result, offset, &err);
      }  /* if */
    }  /* for */
    /* Set the constant type.  It includes all the type qualifiers from the
       qualifiers_model. */
    new_type = make_identically_qualified_type(curr_type, qualifiers_model);
    implicit_or_explicit_base_cast(result, new_type, is_implicit_cast);
    /* Record the backing expression if the folding was successful. */
    if (*did_not_fold) {
      expr = NULL;
    } else if (!omit_backing_expr) {
      if (expr == NULL && constant_is(constant_1, ck_address) &&
          constant_1->variant.address.kind ==
                                         (an_address_base_kind)abk_variable &&
          !constant_1->implicit_cast) {
        expr = var_lvalue_expr(constant_1->variant.address.variant.variable);
        if (is_array_type(expr->type)) {
          /* If we are dealing with the address of an array variable, apply
             the necessary array-to-pointer conversion.  E.g., in C++14 mode:
               struct B { virtual double f(); };
               struct D: B {};
               int main () {
                 D d[2];
                 d[1] = d[0];
                 return 0;
               }
             interpretation of D::operator= may get us here with such a
             situation. */
          expr = conv_array_expr_to_pointer(expr);
        }  /* if */
      }  /* if */
      if (expr != NULL) {
        a_boolean local_error_detected;
        add_base_class_casts(bcp, qualifiers_model,
                             /*check_cast_access=*/FALSE,
                             /*check_ambiguity=*/FALSE,
                             /*allow_ambiguity=*/FALSE,
                             is_implicit_cast, /*implicit_in_naming=*/FALSE,
                             &expr, err_pos, &local_error_detected);
        check_assertion(!local_error_detected);
        result->expr = expr;
      }  /* if */
    }  /* if */
    release_local_constant(&offset);
  }  /* if */
}  /* fold_base_class_cast */


static void fold_derived_class_cast(a_constant        *constant_1,
                                    a_base_class      *bcp,
                                    a_constant        *result,
                                    a_source_position *err_pos,
                                    an_error_code     *error_detected)
/*
Fold a C++ cast of a class pointer to a derived class pointer.  constant_1 is
an address of a class object.  It is converted to point to the pointer type
indicated by result->type, and the new constant is returned in *result.
bcp points to the base class entry for the current type relative to the
desired derived type.  If there is an error, it is issued at *err_pos.
If error_detected is non-NULL, set *error_detected to the code for any
error detected, and do not issue the diagnostic, or set it to
ec_no_error if there was no error.
*/
{
  a_type_ptr       new_type = result->type, derived_class_type;
  an_integer_value base_class_offset;
  a_boolean        err;

  /* The code here looks like add_derived_class_casts. */
  if (error_detected != NULL) *error_detected = ec_no_error;
  derived_class_type = f_skip_typerefs(type_pointed_to(new_type));
  if (bcp->ambiguous) {
    /* The cast is ambiguous. */
    if (error_detected != NULL) {
      *error_detected = ec_ambiguous_derived_class;
    } else {
      pos_ty2_error(ec_ambiguous_derived_class, err_pos, derived_class_type,
                    bcp->type);
    }  /* if */
    set_error_constant(result);
  } else if (any_virtual_steps_in_derivation(bcp)) {
    /* The base class is a virtual base of the derived class, or there's a
       virtual step on the derivation path. */
    if (error_detected != NULL) {
      *error_detected = ec_derived_class_from_virtual_base;
    } else {
      pos_ty2_error(ec_derived_class_from_virtual_base, err_pos,
                    derived_class_type, bcp->type);
    }  /* if */
    set_error_constant(result);
  } else {
    an_expr_node_ptr expr = constant_1->expr;
    constant_1->expr = NULL;
    copy_constant(constant_1, result);
    if (constant_is(result, ck_address)) {
      result->variant.address.subobject_path =
                  copy_subobject_path(result->variant.address.subobject_path);
    }  /* if */
    if (is_null_pointer_value(constant_1)) {
      /* Preserve a NULL pointer. */
    } else {
      a_constant_ptr offset = local_constant();
      /* Determine the pointer offset, ... */
      get_pointer_offset(result, offset);
      /* ... subtract the offset to the base class, ... */
      set_unsigned_integer_value(&base_class_offset, bcp->offset);
      subtract_integer_values(&offset->variant.integer_value,
                              &base_class_offset,
                              int_constant_is_signed(offset), &err);
      /* ... and put the offset into the result pointer constant.  Note
         that no overflow/object-size checking is needed, since the base
         class has to be within the underlying object. */
      set_pointer_offset(result, offset, &err);
      release_local_constant(&offset);
      if (constant_is(result, ck_address)) {
        /* Update the subobject path. */
        a_subobject_path_ptr  spp;
        a_base_class_ptr      new_base_class = NULL;
        spp = get_trailing_subobject_path_entry(result, /*is_offset=*/FALSE,
                                                /*is_base_class=*/TRUE);
        if (spp->variant.base_class == NULL) {
          /* No base class recorded yet: A derived-class cast is not possible
             since it would cast beyond the most-derived class. */
        } else {
          a_derivation_step_ptr  dsp, tail;
          a_base_class_derivation_ptr
                                 bcdp = spp->variant.base_class->derivation;
          check_assertion(bcdp != NULL && bcdp->next == NULL);
          dsp = bcdp->path;
          tail = bcdp->path_tail;
          for (; dsp != tail->next; dsp = dsp->next) {
            if (same_entities(dsp->base_class->type, new_type)) {
              new_base_class = dsp->base_class;
              break;
            }  /* if */
          }  /* for */
        }  /* if */
        if (new_base_class != NULL) {
          spp->variant.base_class = new_base_class;
        } else if (spp->variant.base_class != NULL &&
                   same_entities(spp->variant.base_class->derived_class,
                                 derived_class_type)) {
          /* We're casting back to the most derived class.  Drop any final
             base-class casts. */
          a_subobject_path_ptr  *p_spp;
          p_spp = &result->variant.address.subobject_path;
          for (spp = *p_spp; spp != NULL; spp = spp->next) {
            if (!spp->is_base_class) p_spp = &spp->next;
          }  /* for */
          *p_spp = NULL;
        } else {
          /* Invalid cast. */
          if (error_detected != NULL) {
            *error_detected = ec_derived_class_too_far;
          } else {
            pos_ty2_error(ec_derived_class_too_far, err_pos,
                          derived_class_type, bcp->type);
          }  /* if */
          set_error_constant(result);
          goto done;
        }  /* if */
      }  /* if */
    }  /* if */
    result->type = new_type;
    result->implicit_cast = TRUE;
    result->explicit_cast_applied = TRUE;
    /* Update the backing expression if one was present. */
    if (expr != NULL) {
      a_boolean local_error_detected;
      add_derived_class_casts(type_pointed_to(new_type), bcp,
                              /*check_ambiguity=*/FALSE,
                              /*requires_runtime_check=*/FALSE,
                              &expr, err_pos,
                              &local_error_detected);
      check_assertion(!local_error_detected);
    }  /* if */
    result->expr = expr;
  }  /* if */
done:;
}  /* fold_derived_class_cast */


static void conv_pointer_to_whatever(
                                    a_constant        *old_constant,
                                    a_constant        *new_constant,
                                    a_boolean         check_cast_access,
                                    a_boolean         check_ambiguity,
                                    a_boolean         is_implicit_cast,
                                    a_boolean         fold_constant_addr_exprs,
                                    a_boolean         is_reinterpret_cast,
                                    a_boolean         is_object_pointer,
                                    a_boolean         *did_not_fold,
                                    a_source_position *err_pos,
                                    an_error_code     *err_code,
                                    an_error_severity *err_severity)
/*
Convert a pointer constant to a constant of type as specified by
"new_constant".  If check_cast_access is TRUE, do access checking.
If check_ambiguity is TRUE, check for ambiguous base class casts.
If is_implicit_cast is TRUE, the cast is implicit.  If
fold_constant_addr_exprs is TRUE, fold related class casts in constant
form; if it's FALSE, do not do such folding and return *did_not_fold
TRUE.  If is_reinterpret_cast is TRUE, this is a reinterpret_cast;
related class casts are treated like casts between unrelated classes.
If is_object_pointer is TRUE, the pointer is considered a pointer to
an object; that's used to implement offsetof, where we want a zero
pointer not to be treated like a null pointer.
If there is an error, either issue it immediately at *err_pos (if it
cannot be reduced to a warning in a nonconstant context), or return
*err_code and *err_severity set appropriately.  Note that this routine
is also called when the old constant is an address constant that has
previously been cast to an integral type, and so does not have pointer
type.
*/
{
  a_type_ptr       new_type = new_constant->type;
  a_type_ptr       old_type = old_constant->type;
  a_boolean        conversion_handled = FALSE, baseward_cast;
  a_base_class_ptr bcp;

  *did_not_fold = FALSE;
  *err_code = ec_no_error;
  *err_severity = es_warning;
#if CHECKING
  if (old_constant->kind != (a_constant_repr_kind)ck_address &&
      old_constant->kind != (a_constant_repr_kind)ck_integer &&
      old_constant->kind != (a_constant_repr_kind)ck_template_param) {
    internal_error("conv_pointer_to_whatever: invalid constant kind");
  }  /* if */
#endif /* CHECKING */
  /* Change of pointer type for an address or template parameter constant. */
  /* Change of pointer type for an integer cast to a pointer type. */
  /* Change of an address constant previously cast to integer to another
     type. */
  if (is_integral_or_enum_type(new_type)) {
    /* Pointer value being forced into an integral type. */
    if (old_constant->kind == (a_constant_repr_kind)ck_integer) {
      /* A constant that is an integer, cast to some pointer type and back
         to integer, as in (int)(void*)-1: make sure the integer is
         truncated and sign-extended properly, with warnings if
         appropriate. */
      conv_integer_to_integer(old_constant, new_constant, is_implicit_cast,
                              err_code, err_severity);
      conversion_handled = TRUE;
    } else if (skip_typerefs(new_type)->size < skip_typerefs(old_type)->size) {
      /* The integral type is not large enough to hold a pointer. */
      *err_code = ec_integer_truncated;
      *err_severity = es_error;
    }  /* if */
  } else if (is_floating_type(new_type)) {
    /* Converting an address to a floating-point type cannot be done at
       compile-time. */
    *did_not_fold = TRUE;
  } else if (is_reinterpret_cast) {
    /* Suppress the related-class processing for reinterpret_casts.  If
       constant addressing expressions are not being folded, keep the
       reinterpret_cast in executable form.  But do fold casts of
       a null pointer value. */
    if (!fold_constant_addr_exprs && !is_null_pointer_value(old_constant)) {
      *did_not_fold = TRUE;
    } else if (constexpr_enabled && !microsoft_mode && expr_stack != NULL &&
               (int)expr_stack->expression_kind <= (int)ek_template_arg) {
      /* reinterpret_cast is not permitted in constant-expression contexts.
         Avoid folding such casts, so that the interpreter can easily
         recognize attempts at evaluation them.  MSVC does fold those
         casts (and thus accepts nonstandard cases). */ 
      *did_not_fold = TRUE;
    }  /* if */
  } else if (related_class_pointers(old_type, new_type,
                                    &baseward_cast, &bcp)) {
    /* In C++, a cast of a pointer to a class to a pointer to a base class
       or derived class. */
    conversion_handled = TRUE;
    /* Do not fold such casts in constant form unless told to.  That's to
       preserve detailed addressing information in the IL. */
    if (!fold_constant_addr_exprs) {
      *did_not_fold = TRUE;
    } else if (baseward_cast) {
      /* Derived --> base.  Valid unless the cast is ambiguous or the base
         class is inaccessible. */
      fold_base_class_cast(old_constant, bcp, type_pointed_to(new_type),
                           new_constant,
                           check_cast_access, check_ambiguity,
                           is_implicit_cast, is_object_pointer,
                           /*omit_back_expr=*/FALSE,
                           did_not_fold, err_pos, err_code);
      if (*err_code != ec_no_error) {
        /* Treat failing cases as failing to fold, instead of errors. */
        *err_code = ec_no_error;
        *did_not_fold = TRUE;
      }  /* if */
    } else {
      /* Base --> derived.  Valid unless the cast is ambiguous or the base
         class is a virtual base of the derived class. */
      fold_derived_class_cast(old_constant, bcp, new_constant, err_pos,
                              err_code);
      if (*err_code != ec_no_error) {
        /* Treat failing cases as failing to fold, instead of errors. */
        *err_code = ec_no_error;
        *did_not_fold = TRUE;
      }  /* if */
    }  /* if */
    /* If the qualifiers aren't right, adjust them. */
    if (!*did_not_fold && 
        !is_error_type(new_constant->type) &&
        !identical_types(new_constant->type, new_type)) {
      implicit_or_explicit_cast(new_constant, new_type, is_implicit_cast);
    }  /* if */
  }  /* if */
  /* Do the cast (by calling implicit_cast) unless there was an error or
     the cast has already been handled. */
  if (!conversion_handled && !*did_not_fold &&
      (*err_code == ec_no_error || *err_severity != es_error)) {
    copy_constant(old_constant, new_constant);
    implicit_or_explicit_cast(new_constant, new_type, is_implicit_cast);
  }  /* if */
}  /* conv_pointer_to_whatever */


static a_type_ptr pm_constant_member_class(a_constant_ptr constant)
/*
constant is a non-NULL pointer-to-member constant.  Return the class
of which the underlying member is a member.
*/
{
  a_type_ptr class_type;

  if (constant->variant.ptr_to_member.is_function_ptr) {
    /* The underlying member is a function. */
    class_type = parent_class_of(
                             constant->variant.ptr_to_member.variant.routine);
  } else {
    /* The underlying member is a nonstatic data member. */
    class_type =
               parent_class_of(constant->variant.ptr_to_member.variant.field);
  }  /* if */
  return class_type;
}  /* pm_constant_member_class */


static void set_pm_cast_base_class(a_constant_ptr   constant, 
                                   a_type_ptr       new_type,
                                   a_base_class_ptr bcp,
                                   a_boolean        cast_to_base,
                                   a_boolean        is_implicit_cast,
                                   a_boolean        *did_not_fold)
/*
constant is a pointer-to-member constant.  Cast it to new_type, which is
a pointer to member of the class indicated by bcp.  If cast_to_base is TRUE,
this cast is toward a base class; otherwise, it is toward a derived class
(in which case bcp gives the base class entry for the current class as
a base class of the derived class).  is_implicit_cast is TRUE if the
cast is implicit.  If the cast cannot be folded, *did_not_fold is
returned TRUE.
*/
{
  a_type_ptr       member_class, new_class;
  a_targ_ptrdiff_t offset;
  a_base_class_ptr casting_base_class;

  *did_not_fold = FALSE;
  if (pm_constant_is_null(constant)) {
    /* A NULL pointer-to-member keeps a NULL casting_base_class even
       when cast to another type. */
    implicit_or_explicit_cast(constant, new_type, is_implicit_cast);
  } else {
    /* Determine the class type we're casting to. */
    if (cast_to_base) {
      new_class = bcp->type;
    } else {
      new_class = bcp->derived_class;
    }  /* if */
    /* Determine the offset for any casting already done to the constant. */
    casting_base_class = constant->variant.ptr_to_member.casting_base_class;
    if (casting_base_class == NULL) {
      offset = 0;
    } else {
      offset = (a_targ_ptrdiff_t)casting_base_class->offset;
      if (constant->variant.ptr_to_member.cast_to_base) offset = -offset;
    }  /* if */
    /* Add the offset for the new cast. */
    if (cast_to_base) {
      offset -= (a_targ_ptrdiff_t)bcp->offset;
    } else {
      offset += (a_targ_ptrdiff_t)bcp->offset;
    }  /* if */
    /* Find the original class of the member. */
    member_class = pm_constant_member_class(constant);
    /* Find the base class to use as casting_base_class. */
    if (same_entities(new_class, member_class)) {
      /* The casts take us back to the original member class, so no
         casting is needed. */
      casting_base_class = NULL;
      cast_to_base = FALSE;
      constant->implicit_cast = FALSE;
      constant->type = new_type;
    } else {
      /* Look for a base class of the new type which is the member class.
         If one is found, the cast is to a derived class.  Use the offset
         to distinguish different instances of the same class. */
      for (casting_base_class = base_classes_of(new_class);
           casting_base_class != NULL;
           casting_base_class = casting_base_class->next) {
        if (same_entities(casting_base_class->type, member_class) &&
            casting_base_class->offset == (a_targ_size_t)offset) {
          cast_to_base = FALSE;
          goto have_base_class;
        }  /* if */
      }  /* for */
      /* Look for a base class of the member class which is the new class.
         if one if found, the cast is to a base class.  This is an unusual
         case. */
      for (casting_base_class = base_classes_of(member_class);
           casting_base_class != NULL;
           casting_base_class = casting_base_class->next) {
        if (same_entities(casting_base_class->type, new_class) &&
            casting_base_class->offset == (a_targ_size_t)(-offset)) {
          cast_to_base = TRUE;
          goto have_base_class;
        }  /* if */
      }  /* for */
      /* Weird case (undefined behavior), e.g., member cast from base class
         A to derived class D, then to base class B (which does not contain
         the member). */
      *did_not_fold = TRUE;
      goto end_of_routine;
have_base_class:
      implicit_or_explicit_cast(constant, new_type, is_implicit_cast);
    }  /* if */
    constant->variant.ptr_to_member.casting_base_class = casting_base_class;
    constant->variant.ptr_to_member.cast_to_base = cast_to_base;
  }  /* if */
end_of_routine:;
}  /* set_pm_cast_base_class */


static void fold_pm_base_class_cast(a_constant        *constant_1,
                                    a_base_class      *bcp,
                                    a_constant        *result,
                                    a_boolean         *did_not_fold,
                                    a_source_position *err_pos,
                                    an_error_code     *error_detected)
/*
Fold a C++ cast of a pointer to a member of a class to pointer to a member
of a base class.  constant_1 is a pointer-to-member constant.  It is converted
to a pointer-to-member for the base class indicated by bcp and the new
constant is returned in *result.  result->type on entry indicates the
desired pointer-to-member type, possibly with qualifiers.  If there is an
error, issue it at *err_pos.  If the cast cannot be folded, *did_not_fold
is returned TRUE.  If error_detected is non-NULL, set *error_detected
to the code for any error detected, and do not issue the diagnostic,
or set it to ec_no_error if there was no error.  Note that casts of
this type always come from explicit casts, so checking for
accessibility of base classes is not necessary.
*/
{
  a_type_ptr new_type = result->type;

  /* The code here looks like add_pm_base_class_casts. */
  *did_not_fold = FALSE;
  if (error_detected != NULL) *error_detected = ec_no_error;
  if (bcp->ambiguous) {
    /* The base class is ambiguous. */
    if (error_detected != NULL) {
      *error_detected = ec_ambiguous_base_class;
    } else {
      pos_ty_error(ec_ambiguous_base_class, err_pos, bcp->type);
    }  /* if */
    set_error_constant(result);
  } else if (any_virtual_steps_in_derivation(bcp) && !any_cfront_mode()) {
    /* The base class is a virtual base of the derived class, or there's a
       virtual step on the derivation path. */
    if (error_detected != NULL) {
      *error_detected = ec_pm_virtual_base_from_derived_class;
    } else {
      pos_ty2_error(ec_pm_virtual_base_from_derived_class, err_pos,
                    pm_class_type(constant_1->type), bcp->type);
    }  /* if */
    set_error_constant(result);
  } else {
    copy_constant(constant_1, result);
    /* Set the constant to indicate the cast. */
    set_pm_cast_base_class(result, new_type, bcp, /*cast_to_base=*/TRUE,
                           /*is_implicit_cast=*/FALSE, did_not_fold);
  }  /* if */
}  /* fold_pm_base_class_cast */


static void fold_pm_derived_class_cast(a_constant        *constant_1,
                                       a_base_class      *bcp,
                                       a_constant        *result,
                                       a_boolean         is_implicit_cast,
                                       a_boolean         check_cast_access,
                                       a_boolean         *did_not_fold,
                                       a_source_position *err_pos,
                                       an_error_code     *error_detected)
/*
Fold a C++ cast of a pointer to a member of a class to pointer to member
of a derived class.  constant_1 is a pointer-to-member constant.  It is
converted to a pointer-to-member for the derived class (given by
result->type) and the new constant is returned in *result.  bcp points
to the base class entry for the current type relative to the desired
derived type.  The cast is implicit if is_implicit_cast is TRUE.
Access should be checked if check_cast_access is TRUE.  If there is an
error, it is issued at *err_pos.  If the cast cannot be folded,
*did_not_fold is returned TRUE.  If error_detected is non-NULL, set
*error_detected to the code for any error detected, and do not issue
the diagnostic, or set it to ec_no_error if there was no error.
*/
{
  a_type_ptr            new_type = result->type, curr_type;
  a_type_ptr            derived_class_type;
  a_derivation_step_ptr dsp, tail;
  a_base_class_ptr      base_class;

  /* The code here looks like add_pm_derived_class_casts. */
  *did_not_fold = FALSE;
  if (error_detected != NULL) *error_detected = ec_no_error;
  derived_class_type = pm_class_type(new_type);
  if (bcp->ambiguous) {
    /* The cast is ambiguous. */
    if (error_detected != NULL) {
      *error_detected = ec_ambiguous_derived_class;
    } else {
      pos_ty2_error(ec_ambiguous_derived_class, err_pos, derived_class_type,
                    bcp->type);
    }  /* if */
    set_error_constant(result);
  } else if (!(microsoft_mode &&
          PTR_TO_MEMBER_REPR_SUPPORTS_CAST_FROM_VIRTUAL_BASE) && /*lint !e506*/
             any_virtual_steps_in_derivation(bcp)) {
    /* The base class is a virtual base of the derived class. */
    if (error_detected != NULL) {
      *error_detected = ec_pm_derived_class_from_virtual_base;
    } else {
      pos_ty2_error(ec_pm_derived_class_from_virtual_base, err_pos,
                    derived_class_type, bcp->type);
    }  /* if */
    set_error_constant(result);
  } else {
    /* No access checking in prototype instantiations. */
    if (in_front_end &&
        scope_stack[depth_scope_stack].in_prototype_instantiation) {
      check_cast_access = FALSE;
    }  /* if */
    if (check_cast_access) {
      /* Check the accessibility of the base class.  (Recall that casts
         to derived types can be done implicitly.) */
      curr_type = derived_class_type;
      tail = bcp->derivation->path_tail;
      for (dsp = cast_derivation_path_of(bcp);
           dsp != tail->next;
           dsp = dsp->next) {
        /* Check that the base class is accessible from the current class. */
        base_class = dsp->base_class;
        if (!is_accessible_imm_base_class(base_class, curr_type, bcp)) {
          /* The base class is inaccessible. */
          if (error_detected != NULL) {
            if (is_effective_sfinae_error(ec_conv_from_inaccessible_base_class,
                                          es_discretionary_error, err_pos)) {
              *error_detected = ec_conv_from_inaccessible_base_class;
            }  /* if */
          } else {
            pos_ty_diagnostic(es_discretionary_error,
                              ec_conv_from_inaccessible_base_class,
                              err_pos, base_class->type);
          }  /* if */
          break;
        }  /* if */
        curr_type = base_class->type;
      }  /* for */
    }  /* if */
    copy_constant(constant_1, result);
    /* Set the constant to indicate the cast. */
    set_pm_cast_base_class(result, new_type, bcp, /*cast_to_base=*/FALSE,
                           is_implicit_cast, did_not_fold);
  }  /* if */
}  /* fold_pm_derived_class_cast */

#if MICROSOFT_EXTENSIONS_ALLOWED

static a_boolean related_classes_single_inh(a_type_ptr class_1,
                                            a_type_ptr class_2)
/*
Return TRUE if the class types given are related by inheritance or are
the same, and if there's inheritance the Microsoft inheritance kind is
single inheritance.
*/
{
  a_boolean result;

  result = (identical_types(class_1, class_2) ||
            (find_base_class_of(class_1, class_2) != NULL &&
             class_2->variant.class_struct_union.extra_info->inheritance_kind
                                         == (an_inheritance_kind)ihk_single) ||
            (find_base_class_of(class_2, class_1) != NULL &&
             class_1->variant.class_struct_union.extra_info->inheritance_kind
                                         == (an_inheritance_kind)ihk_single));
  return result;
}  /* related_classes_single_inh */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void conv_ptr_to_member_to_ptr_to_member(
                                      a_constant        *old_constant,
                                      a_constant        *new_constant,
                                      a_boolean         is_implicit_cast,
                                      a_boolean         check_cast_access,
                                      a_boolean         is_reinterpret_cast,
                                      a_boolean         *did_not_fold,
                                      a_source_position *err_pos,
                                      an_error_code     *err_code,
                                      an_error_severity *err_severity,
                                      a_boolean         suppress_complex_diags)
/*
Convert a pointer-to-member constant to a pointer-to-member constant of
a different type.  old_constant is the original constant.  new_constant->type
indicates the desired new type.  The converted constant is put into
*new_constant.  This is an implicit cast if is_implicit_cast is TRUE.
Check access if check_cast_access is TRUE.  This is a reinterpret_cast
if is_reinterpret_cast is TRUE.  If the cast cannot be folded,
*did_not_fold is returned TRUE.  Return err_code and *err_severity set
*to indicate any error/warning detected, or *err_code == ec_no_error
*if everything went fine.  If suppress_complex_diags is TRUE, suppress
(and return in err_code/err_severity) also those complex diagnostics
(e.g., those for access errors) that can't be issued simply from the
error code.
*/
{
  a_type_ptr       new_type = new_constant->type, new_class;
  a_type_ptr       old_type = old_constant->type, old_class;
  a_base_class_ptr bcp;
  an_error_code    *p_err_code = NULL;

  *err_code = ec_no_error;
  *err_severity = es_warning;
  *did_not_fold = FALSE;
  if (suppress_complex_diags) p_err_code = err_code;
  old_class = pm_class_type(old_type);
  new_class = pm_class_type(new_type);
  if (is_reinterpret_cast) {
    /* A reinterpret_cast. */
    if (!old_constant->is_reinterpret_cast &&
        old_constant->variant.ptr_to_member.casting_base_class != NULL) {
      /* The constant entry can't represent a static_cast followed by
         a reinterpret_cast. */
      *did_not_fold = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (microsoft_mode &&
               !related_classes_single_inh(old_class, new_class)) {
      /* MSVC++ only allows reinterpret_casts like this when the offset
         is zero and inheritance is single.  Otherwise they get an error.
         We don't give an error but we don't fold them at compile time. */
      *did_not_fold = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else {
      /* Our interpretation is that a reinterpret_cast leaves the bits
         of the pointer-to-member alone.  That's what Cfront 3.0 does.
         It's not what g++ 3.n, for example, does. */
      copy_constant(old_constant, new_constant);
      implicit_or_explicit_cast(new_constant, new_type, is_implicit_cast);
      new_constant->is_reinterpret_cast = TRUE;
    }  /* if */
  } else if (old_constant->is_reinterpret_cast && !is_reinterpret_cast) {
    /* The constant entry can't represent a reinterpret_cast followed
       by a static_cast. */
    *did_not_fold = TRUE;
  /* Using types_are_compatible so that A<x> and A<error> are considered
     the same type. */
  } else if (types_are_compatible(old_class, new_class)) {
    /* The classes are the same, so no error check is needed. */
    /* The fact that the class types are the same does not mean the
       pointer-to-member types are the same; the member type may be
       changing. */
    copy_constant(old_constant, new_constant);
    implicit_or_explicit_cast(new_constant, new_type, is_implicit_cast);
  } else if ((bcp = find_base_class_of(old_class, new_class)) != NULL) {
    /* Derived --> base (allowed only as an explicit cast).  Valid unless
       the cast is ambiguous. */
    fold_pm_base_class_cast(old_constant, bcp, new_constant, did_not_fold,
                            err_pos, p_err_code);
    if (p_err_code != NULL && *err_code != ec_no_error) {
      *err_severity = es_error;
    }  /* if */
  } else if ((bcp = find_base_class_of(new_class, old_class)) != NULL) {
    /* Base --> derived (allowed as an implicit or explicit cast).  Valid
       unless the cast is ambiguous, the base class is inaccessible (if
       the cast is implicit), or the base class is a virtual base of the
       derived class. */
    fold_pm_derived_class_cast(old_constant, bcp, new_constant,
                               is_implicit_cast, check_cast_access,
                               did_not_fold, err_pos, p_err_code);
    if (p_err_code != NULL && *err_code != ec_no_error) {
      *err_severity = es_error;
    }  /* if */
  } else {
    *err_severity = es_error;
    *err_code = ec_bad_cast;
    expect_error();
  }  /* if */
}  /* conv_ptr_to_member_to_ptr_to_member */


static void conv_integer_to_pointer(a_constant        *old_constant,
				    a_constant        *new_constant,
			  	    a_boolean         is_implicit_cast,
				    an_error_code     *err_code,
				    an_error_severity *err_severity)
/*
Convert an integer constant to a pointer constant of type as specified by
"new_constant".
*/
{
  a_type_ptr       new_type = new_constant->type;
  an_integer_value mask;
  a_boolean        is_label_diff = FALSE;

#if GNU_EXTENSIONS_ALLOWED
  if (old_constant->kind == (a_constant_repr_kind)ck_label_difference) {
    is_label_diff = TRUE;
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  *err_code = ec_no_error;
  *err_severity = es_warning;
  if (is_implicit_cast) {
    if (is_label_diff ||
        cmplit_integer_constant(old_constant, (a_host_large_integer)0) != 0) {
      /* Any value other than zero (NULL).  Issue a warning. */
      *err_code = ec_non_zero_int_conv_to_pointer;
      *err_severity = es_warning;
    }  /* if */
  }  /* if */
  /* Make a new constant that is the old constant implicitly cast to the
     pointer type. */
  copy_constant(old_constant, new_constant);
  implicit_or_explicit_cast(new_constant, new_type, is_implicit_cast);
  /* Mask the integer down to the size of pointer. */
  if (new_constant->kind == (a_constant_repr_kind)ck_integer) {
    make_integer_value_mask(&mask,
                            size_t_arg(skip_typerefs(new_type)->size *
                                       targ_char_bit));
    and_integer_values(&new_constant->variant.integer_value, &mask);
  } else if (!is_label_diff) {
    unexpected_condition_str("conv_integer_to_pointer: not integer constant");
  }  /* if */
}  /* conv_integer_to_pointer */


static void conv_integer_to_ptr_to_member(
                                        ARG_UNUSED a_constant *old_constant,
                                        a_constant            *new_constant,
                                        a_boolean             is_implicit_cast)
/*
Convert an integer constant to a pointer to member.  is_implicit_cast
is TRUE if the cast is implicit.
*/
{
  a_type_ptr new_type = new_constant->type;
  a_boolean  is_function_ptr;

#if CHECKING
  /* The only valid constants are zero or nullptr. */
  if ((old_constant->kind != (a_constant_repr_kind)ck_integer ||
       old_constant->implicit_cast ||
       !is_zero_constant(old_constant)) &&
      !is_nullptr_type(old_constant->type)) {
    internal_error("conv_integer_to_ptr_to_member: bad source constant");
  }  /* if */
#endif /* CHECKING */
  set_constant_kind(new_constant, (a_constant_repr_kind)ck_ptr_to_member);
  new_constant->variant.ptr_to_member.is_function_ptr = is_function_ptr =
                                    is_function_type(pm_member_type(new_type));
  /* NULL pointer implies a NULL pointer-to-member constant. */
  if (is_function_ptr) {
    new_constant->variant.ptr_to_member.variant.routine = NULL;
  } else {
    new_constant->variant.ptr_to_member.variant.field = NULL;
  }  /* if */
  implicit_or_explicit_cast(new_constant, new_type, is_implicit_cast);
}  /* conv_integer_to_ptr_to_member */


static void issue_folding_diagnostic(an_error_code     err_code,
                                     an_error_severity err_severity,
                                     a_boolean         constant_context,
                                     a_boolean         evaluated_context,
                                     a_boolean         silence_warning,
                                     a_boolean         *did_not_fold,
                                     an_error_code     *error_detected,
                                     a_source_position *err_pos,
                                     a_constant        *result)
/*
An error or warning has been detected in a folding operation; err_code
and err_severity indicate what it is.  If not in a constant_context, reduce
an error to a warning and set *did_not_fold to TRUE.  If not in an
evaluated_context, throw away the error and set *did_not_fold to TRUE.
silence_warning indicates that a warning should not be emitted.
If error_detected is non-NULL, the caller would like to know that an
error was detected but does not want it issued at this level.  Return
*error_detected set to the code for the error detected, or ec_no_error
if no error was detected, and suppress any diagnostic.  If a
diagnostic is issued, do so with source position *err_pos.  Set
*result to the proper result (often, an error constant).
*/
{
  if (error_detected != NULL) *error_detected = ec_no_error;
  if (!evaluated_context) {
    /* Discard a warning or error in a not-evaluated context. */
    err_severity = es_none;
    *did_not_fold = TRUE;
  } else if (!constant_context ||
             (expr_stack != NULL && curr_expr_is_potentially_unevaluated())) {
    /* Nonconstant or potentially unevaluated context, so an error will not
       be issued.  It will be downgraded to a warning.  If the expression
       actually does end up being evaluated in a constant context, the fact
       that folding was not done will result in an error at that point. */
    if (err_severity == es_error) {
      /* Reduce an error to a warning. */
      err_severity = es_warning;
      *did_not_fold = TRUE;
    }  /* if */
  } else {
    /* Constant context, so errors can be issued.  Check for a requested
       increase of severity on a warning. */
    if (err_severity != es_error &&
        is_effective_sfinae_error(err_code, err_severity, err_pos)) {
      err_severity = es_error;
    } else if (clang_version_is(<190000) &&
               expr_stack != NULL && expr_stack->is_enumerator_value) {
      err_severity = es_warning;
    }  /* if */
  }  /* if */
  if (err_severity == es_error) {
    /* We have an error. */
    if (error_detected != NULL) {
      /* The caller wants an error indication rather than a diagnostic. */
      *error_detected = err_code;
    } else {
      pos_error(err_code, err_pos);
    }  /* if */
    set_error_constant(result);
    *did_not_fold = FALSE;
  } else if (error_detected != NULL) {
    /* At most we have a warning, and we're suppressing diagnostics, so
       skip the rest of the checks. */
  } else if (err_severity == es_warning && !silence_warning) {
    pos_warning(err_code, err_pos);
  }  /* if */
}  /* issue_folding_diagnostic */


void type_change_constant_full(a_constant           *constant,
                               a_type_ptr           new_type,
                               a_boolean            is_implicit_cast,
                               a_boolean            constant_context,
                               a_boolean            evaluated_context,
                               a_boolean            fold_constant_addr_exprs,
                               ARG_UNUSED a_boolean is_cli_attr_arg_expression,
                               a_boolean            check_cast_access,
                               a_boolean            check_ambiguity,
                               a_boolean            is_reinterpret_cast,
                               a_boolean            maintain_expression,
                               a_boolean            *did_not_fold,
                               an_error_code        *error_detected,
                               a_source_position    *err_pos)
/*
Convert the indicated constant to "new_type".  If is_implicit_cast is
TRUE, this is an implicit cast; more warnings are given.  If
constant_context is FALSE, this operation is being evaluated as part
of a nonconstant expression, so any error is reduced to a warning and
*did_not_fold is returned TRUE.  If evaluated_context is FALSE, this
operation is being done in a not-evaluated context (e.g., a sizeof or
a dead branch of a "?" operator), so any error is thrown away and
*did_not_fold is returned TRUE.  *did_not_fold is also returned TRUE
in other cases where the folding cannot be done.
fold_constant_addr_exprs is TRUE if constant address expressions
should be folded (e.g., base class casts); if it is FALSE,
*did_not_fold is set instead for those.  check_cast_access is TRUE if
access checking should be done on related-class casts.
check_ambiguity is TRUE if ambiguity checking should be done on
related-class casts.  If is_reinterpret_cast is TRUE, this cast is a
reinterpret_cast; related-class casts are treated like casts between
unrelated classes.  If maintain_expression is TRUE, any backing expression
attached to the constant is maintained, by adding a cast if necessary.
If error_detected is non-NULL, set *error_detected to the code for any
error detected, and do not issue the diagnostic, or set it to
ec_no_error if there was no error.  *err_pos is used as the position
for any diagnostics issued.
*/
{
  a_type_ptr        constant_type, new_type_with_typedefs;
  a_constant_ptr    new_constant;
  an_error_code     err_code;
  an_error_severity err_severity;
  a_boolean         depends_on_fp_mode = FALSE;
  a_boolean         template_case;
  a_boolean         suppress_diags = (error_detected != NULL);

  db_enter(5, "type_change_constant_full");
  if (error_detected != NULL) *error_detected = ec_no_error;
  *did_not_fold = FALSE;
  err_code = ec_no_error;
  err_severity = es_warning;
  new_constant = local_constant();
  clear_constant(new_constant, (a_constant_repr_kind)ck_error);
  /* Preserve the null_pointer_constant_ruled_out flag. */
  if (cpp11_mode && !is_implicit_cast &&
      !(microsoft_mode && ms_permissive) &&
      !gpp_version_is(any_version)) {
    /* The resolution of Core issue 903 only allows zero literals to produce
       null pointer constants.  Explicit casts are not permitted (i.e.,
       something like "int(0)" is not a null pointer constant).  GNU C++
       modes are excluded here because g++ still treats some casts of zero
       literals as null pointer constants; Clang does not. */
    new_constant->null_pointer_constant_ruled_out = TRUE;
  } else {
    new_constant->null_pointer_constant_ruled_out =
                                     constant->null_pointer_constant_ruled_out;
  }  /* if */
  /* Put the new type in the destination constant (preserving typedefs
     if any; that's important). */
  new_constant->type = new_type_with_typedefs = new_type;
  /* Remove any type qualifiers or typedefs from the types involved. */
  constant_type = skip_typerefs(constant->type);
  new_type = skip_typerefs(new_type);

  if (is_error_constant(constant) || is_error_type(new_type) ||
      is_or_contains_error_type(constant_type)) {
    /* Changing to an error type, or the old constant is an error constant,
       so produce an error constant as result. */
    set_error_constant(new_constant);
    goto done_with_folding;
  }  /* if */
  /* Not using context_may_have_dependent_types here because we can get
     "auto" from type deductions in initializations. */
  template_case = (!C_mode() &&
                   (constant_is(constant, ck_template_param) ||
                    (in_front_end && is_template_dependent_type(new_type))));
  if (identical_types(constant_type, new_type) &&
      (is_implicit_cast || !template_case)) {
    /* The current and new types are the same, so no change is required.
       Preserve null_pointer_constant_ruled_out: copy_constant would
       otherwise restore the source constant's flag and undo the Core
       issue 903 handling above (e.g., for an explicit cast "(int)0"). */
    a_boolean  ruled_out = new_constant->null_pointer_constant_ruled_out;
    copy_constant(constant, new_constant);
    new_constant->null_pointer_constant_ruled_out = ruled_out;
    /* Put in the actual type wanted, as it may have typedefs. */
    new_constant->type = new_type_with_typedefs;
    goto done_with_folding;
  }  /* if */
  if (expr_stack != NULL && !expr_stack->potentially_evaluated &&
      !evaluated_context && !expr_stack->favor_constant_result &&
      !((type_is(new_type, tk_pointer) || type_is(new_type, tk_nullptr)) &&
        constant_is(constant, ck_integer))) {
    /* No need to fold the result if this is an unevaluated context, except
       possibly when creating pointer constants (like the null pointer).
       In constraint-expressions (which are unevaluated) we want to avoid
       folding because that makes recovering the original type more difficult,
       and later processing has to ensure that the original type was bool. */
    *did_not_fold = TRUE;
    goto done_with_folding;
  }  /* if */
  if (template_case) {
    /* Casting a template parameter constant, or casting to a template
       parameter type.  Use a tpck_expression constant. */
    make_template_param_cast_constant(constant, new_constant, new_type,
                                      !is_implicit_cast);
    goto done_with_folding;
  }  /* if */
  if (is_bool_type(new_type) || enum_has_bool_underlying_type(new_type)) {
    /* Conversion of any type to bool, or to an enumeration with a bool
       underlying type.  In the latter case the value is first converted to
       the underlying bool type (N5014 [expr.static.cast]/7.8), so the result
       is false or true rather than the unconverted integer value.  Set the
       boolean value to zero if the source constant is some form of "false".
       Otherwise, set it to 1.  The enum type itself is preserved in
       new_constant->type, which was set above. */
    if (!constant_bool_value_known_at_compile_time(constant)) {
      /* The constant's value is not known until link time, so the conversion
         cannot be folded at this time. */
      *did_not_fold = TRUE;
      goto done_with_folding;
    }  /* if */
    set_constant_kind(new_constant, ck_integer);
    set_integer_value(&new_constant->variant.integer_value,
                      (a_host_large_integer)!is_false_constant(constant));
    goto done_with_folding;
  }  /* if */
  if (is_nullptr_type(new_type)) {
    /* Conversion to a nullptr type.  There is only one "value" of a
       nullptr type, so the result is an integer with value 0, just like
       old-style null pointer constants. */
    set_constant_kind(new_constant, (a_constant_repr_kind)ck_integer);
    set_integer_value(&new_constant->variant.integer_value,
                      (a_host_large_integer)0);
    new_constant->implicit_cast = TRUE;
    goto done_with_folding;
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cli_or_cx_enabled && is_handle_type(new_type)) {
    a_type_ptr  underlying_new_type;
    underlying_new_type = type_pointed_to(new_type);
    underlying_new_type = skip_typerefs(underlying_new_type);
    if (is_cli_attr_arg_expression && is_nullptr_type(constant_type) &&
        !is_valid_cli_attribute_parameter_type(new_type)) {
      /* Within the context of a C++/CLI attribute argument expression,
         values of nullptr types can only be converted to valid attribute
         parameter types. */
      err_code = ec_cli_attribute_invalid_argument;
      err_severity = es_error;
      *did_not_fold = TRUE;
      goto done_with_folding;
    } else if (boxing_conversion_possible(constant_type, new_type,
                                          (a_std_conv_descr *)NULL)) {
      if (is_cli_attr_arg_expression &&
          is_valid_cli_attribute_parameter_type(constant_type)) {
        /* Within the context of a C++/CLI attribute argument expression,
           boxing conversions from a valid attribute parameter type are
           folded to a constant. */
        copy_constant(constant, new_constant);
        implicit_or_explicit_cast(new_constant, new_type, is_implicit_cast);
      } else {
        /* A C++/CLI boxing conversion cannot be folded to a constant. */
        *did_not_fold = TRUE;
      }  /* if */
      goto done_with_folding;
    } else if (is_cli_attr_arg_expression) {
      if (impl_handle_conversion(constant_type, new_type,
                                 /*allow_qualifier_or_eh_mismatch=*/FALSE,
                                 (a_std_conv_descr *)NULL)) {
        copy_constant(constant, new_constant);
        implicit_or_explicit_cast(new_constant, new_type, is_implicit_cast);
        goto done_with_folding;
      } else if (is_handle_type(constant_type)) {
        *did_not_fold = TRUE;
        goto done_with_folding;
      }  /* if */
    }  /* if */
  }  /* if */
#endif /*MICROSOFT_EXTENSIONS_ALLOWED */
  if (vla_enabled && !is_implicit_cast &&
      is_directly_variably_modified_type(new_type)) {
    /* A cast to a variably-modified type where the variable bound appears
       in the cast (an opposed to inside a typedef declared elsewhere) is
       a non-constant operation and cannot be folded. */
    *did_not_fold = TRUE;
    goto done_with_folding;
  }  /* if */
#if GNU_VECTOR_TYPES_ALLOWED
  if (type_is(new_type, tk_vector) || type_is(constant_type, tk_vector)) {
    /* A conversion to or from a vector can be folded only if the other type
       is a vector of equal length and whose elements are of the same nature
       (integer vs. floating-point).  A conversion from a compatible scalar
       value acts like a "vector fill" operation. */
    a_type_ptr  new_etp = NULL, old_etp = NULL;
    a_boolean   boolean = FALSE;
    if (type_is(new_type, tk_vector)) {
      new_etp = skip_typerefs(new_type->variant.vector.element_type);
      boolean = new_type->variant.vector.is_boolean_vector;
    }  /* if */
    if (type_is(constant_type, tk_vector)) {
      old_etp = skip_typerefs(constant_type->variant.vector.element_type);
    }  /* if */
    if (!type_is(constant_type, tk_vector) &&
        new_etp->kind == constant_type->kind && !is_reinterpret_cast) {
      /* Converting a scalar to a vector can be done via an eok_vector_fill
         operation. */
      *new_constant = *constant;
      type_change_constant_full(new_constant, new_etp,
                                /*is_implicit_cast=*/TRUE,
                                constant_context, evaluated_context,
                                fold_constant_addr_exprs,
                                is_cli_attr_arg_expression, check_cast_access,
                                check_ambiguity, is_reinterpret_cast,
                                maintain_expression, did_not_fold,
                                error_detected, err_pos);
      if (boolean) {
        /* A boolean vector has elements normalized to "all ones". */
        an_integer_value  cmpl = new_constant->variant.integer_value;
        complement_integer_value(&cmpl);
        xor_integer_values(&new_constant->variant.integer_value, &cmpl);
      }  /* if */
      if ((error_detected == NULL || !*error_detected) && !*did_not_fold) {
        an_expr_node  *node = alloc_node_for_constant(new_constant);
        node = make_operator_node(eok_vector_fill, new_type, node);
        *did_not_fold = !fold_expr(node, new_constant);
      }  /* if */
    } else if (new_type->kind != constant_type->kind ||
               new_etp->kind != old_etp->kind ||
               num_vector_elements(new_type) !=
                                         num_vector_elements(constant_type)) {
      *did_not_fold = TRUE;
    } else {
      copy_constant(constant, new_constant);
      /* Put in the actual type wanted, as it may have typedefs. */
      new_constant->type = new_type_with_typedefs;
    }  /* if */
    goto done_with_folding;
  }  /* if */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
#if UPC_EXTENSIONS_ALLOWED
  if (upc_mode &&
      (constant->kind == (a_constant_repr_kind)ck_upc_threads ||
       constant->kind == (a_constant_repr_kind)ck_upc_mythread)) {
    /* THREADS and MYTHREAD are not compile-time constants and should
       therefore not be folded. */
    *did_not_fold = TRUE;
    goto done_with_folding;
  }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
  if (constant->kind == (a_constant_repr_kind)ck_address) {
    /* Any case where the constant is represented as an address should be
       converted by setting the implicit_cast flag.  This test has to be
       early -- like this -- to catch ((unsigned)((int)&x)).  That case
       would have constant_type->kind == tk_integer and new_type->kind
       == tk_integer, and so would not look like it involves pointers. */
    conv_pointer_to_whatever(constant, new_constant, check_cast_access,
                             check_ambiguity, is_implicit_cast,
                             fold_constant_addr_exprs, is_reinterpret_cast,
                             /*is_object_pointer=*/FALSE,
                             did_not_fold, err_pos, &err_code, &err_severity);
    goto done_with_folding;
  }  /* if */
  if (is_incomplete_type(new_constant->type)) {
    /* In some severe error cases, the destination type may be an incomplete
       enum type. */
    issue_incomplete_type_diag(err_pos, new_constant->type);
    *did_not_fold = TRUE;
    goto done_with_folding;
  }  /* if */
  /* Determine the type we are converting from. */
  switch (constant_type->kind) {

    case tk_integer:
      /* Converting from integer. */
      switch(new_type->kind) {
        case tk_integer:
          /* Converting integer to integer. */
          conv_integer_to_integer(constant, new_constant, is_implicit_cast,
                                  &err_code, &err_severity);
          break;
        case tk_float:
#if C99_IL_EXTENSIONS_SUPPORTED
        case tk_imaginary:
        case tk_complex:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
          /* Converting integer to float. */
          conv_integer_to_float(constant, new_constant,
                                &err_code, &err_severity);
          break;
#if FIXED_POINT_ALLOWED
        case tk_fixed_point:
          /* Converting integer to fixed-point. */
          conv_integer_to_fixed_point(constant, new_constant,
                                      &err_code, &err_severity);
          break;
#endif /* FIXED_POINT_ALLOWED */
        case tk_pointer:
          /* Converting integer to pointer. */
          conv_integer_to_pointer(constant, new_constant, is_implicit_cast,
                                  &err_code, &err_severity);
          break;
        case tk_ptr_to_member:
          /* Converting integer to pointer-to-member. */
          conv_integer_to_ptr_to_member(constant, new_constant,
                                        is_implicit_cast);
          break;
#if GNU_VECTOR_TYPES_ALLOWED
        case tk_mfp8:
          /* Converting an integer to the ARM opaque __mfp8 type.  Currently,
             this is only used to create zero-initialization constants, but we
             permit folding any value that will fit in eight bits. */
          { an_integer_value  mask;
            set_constant_kind(new_constant, ck_integer);
            new_constant->variant.integer_value =
                                              constant->variant.integer_value;
            make_integer_value_mask(&mask, 8);
            and_integer_values(&new_constant->variant.integer_value, &mask);
            if (cmp_integer_constants(new_constant, constant) != 0) {
              *did_not_fold = TRUE;
            }  /* if */
          }
          break;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
        default:
          unexpected_condition_str(
                             "type_change_constant_full: integer to bad type");
      }  /* switch */
      break;

    case tk_float:
      /* Converting from float. */
      switch (new_type->kind) {
        case tk_integer:
          /* Converting float to integer. */
          conv_float_to_integer(constant, new_constant,
                                &err_code, &err_severity,
                                &depends_on_fp_mode, constant_context);
          break;
        case tk_float:
          /* Converting float to float. */
#if C99_IL_EXTENSIONS_SUPPORTED
        case tk_imaginary:
          /* Converting float to imaginary (produces zero). */
        case tk_complex:
          /* Converting float to complex. */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
          conv_float_to_float(constant, new_constant,
                              &err_code, &err_severity,
                              &depends_on_fp_mode);
          break;
#if FIXED_POINT_ALLOWED
        case tk_fixed_point:
          /* Converting float to fixed-point. */
          conv_float_to_fixed_point(constant, new_constant,
                                    &err_code, &err_severity);
          break;
#endif /* FIXED_POINT_ALLOWED */
        default:
          unexpected_condition_str(
                               "type_change_constant_full: float to bad type");
      }  /* switch */
      break;

#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_imaginary:
      switch (new_type->kind) {
        case tk_integer:
          /* Converting imaginary to integer (produces zero). */
          conv_float_to_integer(constant, new_constant,
                                &err_code, &err_severity,
                                &depends_on_fp_mode, constant_context);
          break;
        case tk_float:
          /* Converting imaginary to float (produces zero). */
        case tk_imaginary:
          /* Converting imaginary to imaginary. */
        case tk_complex:
          /* Converting imaginary to complex. */
          conv_float_to_float(constant, new_constant,
                              &err_code, &err_severity,
                              &depends_on_fp_mode);
          break;
#if FIXED_POINT_ALLOWED
        case tk_fixed_point:
          /* Imaginary to fixed-point. */
          conv_float_to_fixed_point(constant, new_constant,
                                    &err_code, &err_severity);
          break;
#endif /* FIXED_POINT_ALLOWED */
        default:
          unexpected_condition_str(
                           "type_change_constant_full: imaginary to bad type");
      }  /* switch */
      break;

    case tk_complex:
      switch (new_type->kind) {
        case tk_integer:
          /* Converting complex to integer. */
          conv_float_to_integer(constant, new_constant,
                                &err_code, &err_severity,
                                &depends_on_fp_mode, constant_context);
          break;
        case tk_float:
          /* Converting complex to float. */
        case tk_imaginary:
          /* Converting complex to imaginary. */
        case tk_complex:
          /* Converting complex to complex. */
          conv_float_to_float(constant, new_constant,
                              &err_code, &err_severity,
                              &depends_on_fp_mode);
          break;
#if FIXED_POINT_ALLOWED
        case tk_fixed_point:
          /* Complex to fixed-point. */
          conv_float_to_fixed_point(constant, new_constant,
                                    &err_code, &err_severity);
          break;
#endif /* FIXED_POINT_ALLOWED */
        default:
          unexpected_condition_str(
                             "type_change_constant_full: complex to bad type");
      }  /* switch */
      break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */

#if FIXED_POINT_ALLOWED
    case tk_fixed_point:
      switch (new_type->kind) {
        case tk_integer:
          /* Converting fixed-point to integer. */
          conv_fixed_point_to_integer(constant, new_constant,
                                      &err_code, &err_severity);
          break;
        case tk_float:
          /* Converting fixed-point to floating-point. */
#if C99_IL_EXTENSIONS_SUPPORTED
        case tk_imaginary:
          /* Fixed-point to imaginary. */
        case tk_complex:
          /* Fixed-point to complex. */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
          conv_fixed_point_to_float(constant, new_constant,
                                    &err_code, &err_severity);
          break;
        case tk_fixed_point:
          /* Converting fixed-point to fixed-point. */
          conv_fixed_point_to_fixed_point(constant, new_constant,
                                          &err_code, &err_severity);
          break;
        default:
          unexpected_condition_str(
                         "type_change_constant_full: fixed-point to bad type");
      }  /* switch */
      break;
#endif /* FIXED_POINT_ALLOWED */

    case tk_pointer:
      /* Converting from pointer. */
      conv_pointer_to_whatever(constant, new_constant, check_cast_access,
                               check_ambiguity, is_implicit_cast,
                               fold_constant_addr_exprs, is_reinterpret_cast,
                               /*is_object_pointer=*/FALSE,
                               did_not_fold, err_pos,
                               &err_code, &err_severity);
      break;

    case tk_ptr_to_member:
      /* Converting from pointer-to-member to pointer-to-member. */
      conv_ptr_to_member_to_ptr_to_member(constant, new_constant,
                                          is_implicit_cast,
                                          check_cast_access,
                                          is_reinterpret_cast,
                                          did_not_fold,
                                          err_pos, &err_code, &err_severity,
                                          suppress_diags);
      break;

    case tk_error:
      /* The old constant is an error constant. */
      /* Change the type of the new constant back to the original type of the
	 error constant, i.e., error. */
      new_constant->type = constant->type;
      break;

    case tk_nullptr:
      /* The old constant is a null pointer constant (the C++ "nullptr"
         keyword or another value with a nullptr type).  This is treated
         effectively like converting an integer 0, i.e., an old-style
         null pointer constant. */
      check_assertion(constant->kind == (a_constant_repr_kind)ck_integer);
      if (new_type->kind == tk_pointer) {
        conv_integer_to_pointer(constant, new_constant, is_implicit_cast,
                                &err_code, &err_severity);
      } else if (new_type->kind == tk_ptr_to_member) {
        conv_integer_to_ptr_to_member(constant, new_constant,
                                      is_implicit_cast);
      } else if (new_type->kind == tk_integer) {
        conv_integer_to_integer(constant, new_constant, is_implicit_cast,
                                &err_code, &err_severity);
      } else {
        unexpected_condition_str(
                      "type_change_constant_full: nullptr to bad type");
      }  /* if */
      break;

    case tk_class:
    case tk_struct:
    case tk_union:
      /* Comes up in some constexpr error cases.  Note that class cases with
         identical types are handled earlier in this routine. */
      check_assertion(is_or_contains_error_type(constant_type) ||
                      is_or_contains_error_type(new_type));
      new_constant->type = new_type_with_typedefs;
      break;

    case tk_template_param:
      /* Don't attempt to fold a template-dependent constant. */
      *did_not_fold = TRUE;
      break;

    default:
      unexpected_condition_str("type_change_constant_full: from bad type");
  }  /* switch */

done_with_folding:
  if (!new_constant->null_pointer_constant_ruled_out) {
    /* Look for casts that rule out use of a constant as part of a null
       pointer constant.  In a null pointer constant, only casts from
       arithmetic to integral types, or, in C, from integral to "void *",
       are allowed.  This processing is to rule out things like
       (int)(float)0, which are not valid null pointer constants.
       Also really obscure things like (int)(float)2 - 2.  This
       processing is more or less tracking whether a constant could
       be an integral constant expression, even when it is scanned
       in other modes. */
    if (microsoft_bugs && !C_mode() && !is_implicit_cast &&
        microsoft_version <= 1300) {
      /* Microsoft C++ mode: any explicit cast makes a constant not a null
         pointer constant.  In particular, (int)0 is not a null pointer
         constant.  This was fixed in MSVC++ 7.1. */
      new_constant->null_pointer_constant_ruled_out = TRUE;
    } else if (is_integral_or_enum_type(new_type) &&
               is_arithmetic_or_enum_type(constant_type)) {
      /* Arithmetic --> integral.  Okay. */
    } else if (C_mode() &&
               is_void_star_type(new_type) &&
               is_integral_or_enum_type(constant_type)) {
      /* Integral --> void* in C mode, okay. */
    } else if (gcc_mode && gnu_version < 40500 &&
               (constant_type->kind == (a_type_kind)tk_integer ||
                constant_type->kind == (a_type_kind)tk_pointer) &&
               new_type->kind == (a_type_kind)tk_pointer) {
      /* In some GNU C modes, casting to, e.g., "int*" and then to "void*"
         produces a null pointer constant, too.  We leave the flag cleared
         even though the constant is not itself a null pointer constant if
         the resulting constant is not a pointer to "void". */
    } else {
     /* Anything else: this constant cannot be part of a null pointer
        constant. */
      new_constant->null_pointer_constant_ruled_out = TRUE;
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "type_change_constant of ");
    db_constant(constant);
    fprintf(f_debug, ", result = ");
    db_constant(new_constant);
    if (err_code != ec_no_error) {
      fprintf(f_debug, " with ");
      if (err_severity == es_error) {
        fprintf(f_debug, "error");
      } else if (err_severity == es_warning) {
        fprintf(f_debug, "warning");
      } else {
        fprintf(f_debug, "diagnostic");
      }  /* if */
    }  /* if */
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  if (err_code != ec_no_error) {
    /* There was an error or warning. */
    issue_folding_diagnostic(err_code, err_severity, constant_context,
                             evaluated_context,
                             /*silence_warning=*/!is_implicit_cast,
                             did_not_fold, error_detected, err_pos,
                             new_constant);
    if (err_severity == es_error) depends_on_fp_mode = FALSE;
  }  /* if */
  if (depends_on_fp_mode && !constant_context) {
    /* In a non-constant context, leave an operation to be done at runtime
       if its result depends on the floating-point mode. */
    *did_not_fold = TRUE;
  }  /* if */
  if (maintain_expression && constant->expr != NULL &&
      (int)err_severity < (int)es_error && !*did_not_fold) {
    /* Transfer the source expression from the old constant to the new one,
       adding a cast if there was a type change.  Note that the cast added
       is always an eok_cast, so this shouldn't be used if there's the
       possibility that a base-class cast or the like is involved. */
    if (is_implicit_cast &&
        identical_types(constant->type, new_constant->type)) {
      new_constant->expr = constant->expr;
    } else {
      an_expr_node_ptr cast_expr =
                            make_operator_node((an_expr_operator_kind)eok_cast,
                                               new_constant->type,
                                               constant->expr);
      cast_expr->compiler_generated = is_implicit_cast;
      cast_expr->variant.operation.is_reinterpret_cast = is_reinterpret_cast;
      new_constant->expr = cast_expr;
    }  /* if */
  } else {
    new_constant->expr = NULL;
  }  /* if */
  if (constant->folded_statement_expression) {
    new_constant->folded_statement_expression = TRUE;
  }  /* if */
  /* Return the new constant value. */
  copy_constant(new_constant, constant);
  release_local_constant(&new_constant);
  db_exit();
}  /* type_change_constant_full */


void type_change_constant(a_constant        *constant,
                          a_type_ptr        new_type,
                          a_boolean         is_implicit_cast,
                          a_boolean         maintain_expression,
                          a_boolean         *did_not_fold,
                          a_source_position *err_pos)
/*
Simple interface to type_change_constant_full.  See that routine for the
description of the parameters.
*/
{
  type_change_constant_full(constant, new_type, is_implicit_cast,
                            /*constant_context=*/TRUE,
                            /*evaluated_context=*/TRUE,
                            /*fold_constant_addr_exprs=*/TRUE,
                            /*is_cli_attr_arg_expression=*/FALSE,
                            /*check_cast_access=*/is_implicit_cast,
                            /*check_ambiguity=*/TRUE,
                            /*is_reinterpret_cast=*/FALSE,
                            maintain_expression, did_not_fold,
                            /*error_detected=*/(an_error_code *)NULL,
                            err_pos);
}  /* type_change_constant */


a_boolean is_null_pointer_value(a_constant *constant)
/*
Return TRUE if the given constant is a null pointer value (a pointer
with a null value, produced by casting 0 to a pointer type).
*/
{
  a_boolean is_null = FALSE;

  if (is_pointer_type(constant->type) &&
      constant->kind == (a_constant_repr_kind)ck_integer &&
      cmplit_integer_constant(constant, (a_host_large_integer)0) == 0) {
    is_null = TRUE;
  }  /* if */
  return is_null;
}  /* is_null_pointer_value */


a_boolean is_false_constant(a_constant *constant)
/*
Return TRUE if the constant is an integer, floating, pointer, or
pointer to member zero.  This is supposed to duplicate the test on
the boolean controlling expressions in statements and the ?:, &&, and ||
operators.  Can also be used to test for a NULL pointer or pointer to member.
*/
{
  a_boolean is_false = FALSE;

  /* The value of a link-time constant is not known until link time, so
     one cannot decide whether it is true or false.  Such constants should
     not get here. */
  check_assertion_str(constant_bool_value_known_at_compile_time(constant),
                      "is_false_constant: link-time constant");
  /* ck_address constants that aren't link-time constants are assumed to
     be non-NULL.  For example, the address of an auto variable. */
  if (is_zero_constant(constant)) {
    /* Zero integral, fixed-point, or floating constant. */
    is_false = TRUE;
  } else if (is_null_pointer_value(constant)) {
    /* A NULL pointer value (0 cast to a pointer type). */
    is_false = TRUE;
  } else if (constant->kind == (a_constant_repr_kind)ck_integer &&
             constant->implicit_cast &&
             is_nullptr_type(constant->type)) {
    /* The nullptr keyword or a nullptr_t value. */
    is_false = TRUE;
  } else if (constant->kind == (a_constant_repr_kind)ck_ptr_to_member) {
    /* Pointer to member constant.  See if null. */
    is_false = pm_constant_is_null(constant);
  }  /* if */
  return is_false;
}  /* is_false_constant */


a_boolean is_null_pointer_constant(a_constant *constant)
/*
Return TRUE if the given constant is a null pointer constant.
*/
{
  a_boolean is_null_pointer = FALSE;

  if (constant_is(constant, ck_integer)) {
    /* A null pointer constant either has a nullptr type or it has the
       value zero, perhaps cast to "void *" in C.  Only certain kinds of
       casts are allowed. */
    a_type_ptr  tp = skip_typerefs(constant->type);
    if (type_is(tp, tk_nullptr)) {
      is_null_pointer = TRUE;
    } else if (((!constant->null_pointer_constant_ruled_out &&
                 (!type_is(tp, tk_pointer) || !gcc_mode ||
                  is_void_star_type(tp))) ||
                (gnu_mode && gnu_version < 40500 &&
                  /* g++/gcc allow (int)(int *)0 as a null pointer
                     constant.  Fixed in 4.2, but some variants of that
                     linger until eliminated in 4.5.  */
                 is_integral_type(tp))) &&
               cmplit_integer_constant(constant,
                                       (a_host_large_integer)0) == 0) {
      if (!enum_type_is_integral && is_enum_type(tp)) {
        /* In C++ (except for cfront compatibility) an enumerator with value
           zero is not a null pointer constant. */
      } else if (false_literal_is_not_null_pointer_constant &&
                 is_bool_type(tp)) {
        /* The resolution of Core issue 903 removed "false" from the set of
           valid null pointer constants. */
      } else if (cpp11_mode && (clang_mode || gpp_version_is(>=70000)) &&
                 (is_character_type(tp) ||
                  (is_integral_type(tp) &&
                   (tp->variant.integer.wchar_t_type ||
                    tp->variant.integer.char8_t_type ||
                    tp->variant.integer.char16_t_type ||
                    tp->variant.integer.char32_t_type)))) {
        /* Beginning with GCC 7, and in Clang, a zero of character type
           (including wide-character types) is not a null pointer
           constant.  Microsoft still treats such zeros as null pointer
           constants. */
      } else {
        is_null_pointer = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */

  return is_null_pointer;
}  /* is_null_pointer_constant */


a_boolean is_or_might_be_null_pointer_constant(a_constant *constant)
/*
Return TRUE if the given constant is a null pointer constant or is a template
parameter constant that might be a null pointer constant.
*/
{
  a_boolean might_be_null_pointer = FALSE;

  if (!constant_is(constant, ck_template_param)) {
    might_be_null_pointer = is_null_pointer_constant(constant);
  } else {
    /* Template parameter constant.  This might be a null pointer constant
       if its type is integral or a template parameter type (so not, for
       example, if it's a pointer to a template parameter type). */
    a_type_ptr type = skip_typerefs(constant->type);
    if (type->kind == (a_type_kind)tk_integer ||
        type->kind == (a_type_kind)tk_template_param) {
      a_constant_ptr eff_constant = constant;
      might_be_null_pointer = TRUE;
      /* Drop casts to get to the underlying constant. */
      while (constant_is(eff_constant, ck_template_param) &&
             tpck_is(eff_constant, tpck_expression)) {
        a_constant_ptr  base_con;
        a_boolean       explicit_cast;
        if (is_template_param_cast_constant(eff_constant, &base_con,
                                            &explicit_cast)) {
          eff_constant = base_con;
        } else {
          break;
        }  /* if */
      }  /* while */
      if (constant_is(eff_constant, ck_template_param) &&
          (tpck_is(eff_constant, tpck_sizeof) ||
           tpck_is(eff_constant, tpck_datasizeof))) {
        /* A sizeof or __datasizeof constant should never be treated as a null
           pointer constant. */
        might_be_null_pointer = FALSE;
      }  /* if */
    }  /* if */
  }  /* if */
  return might_be_null_pointer;
}  /* is_or_might_be_null_pointer_constant */


#if DEBUG
static void db_unary_operation(a_const_char  *operation,
			       a_constant    *operand,
                               a_constant    *result,
			       an_error_code err_code)
/*
Do a debug print giving the result of folding a one-operand constant operation.
*/
{
  if (db_flag_is_set("folding") || debug_level >= 5) {
    fprintf(f_debug, "%s ", operation);
    db_constant(operand);
    fprintf(f_debug, ", result = ");
    db_constant(result);
    if (err_code != ec_no_error) fprintf(f_debug, " with error");
    fprintf(f_debug, "\n");
  }  /* if */
}  /* db_unary_operation */
#endif /* DEBUG */


static void do_inegate(a_constant        *constant,
		       a_constant        *result,
		       an_error_code     *err_code,
		       an_error_severity *err_severity)
/*
Do the negate operation on all types of integers.
*/
{
  an_integer_value result_value;
  a_boolean        err, is_signed;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  /* Compute 0 - constant. */
  set_integer_value(&result_value, (a_host_large_integer)0);
  is_signed = int_constant_is_signed(constant);
  subtract_integer_values(&result_value, &constant->variant.integer_value,
                          is_signed, &err);
  if (is_signed) {
    /* Negation of a signed integer. */
    if (err) {
      /* Folding error.  Adjust the severity to an error in various modes. */
      if (C_dialect == C_dialect_pcc && !constant->non_arithmetic) {
        /* Do not make this an error for non-arithmetic constants in K&R
           mode. */
      } else {
        *err_code = ec_integer_overflow;
        *err_severity = ES_INT_OVERFLOW;
      }  /* if */
    }  /* if */
  } else {
    /* Negation of an unsigned integer. */
    /* Negation of an unsigned quantity, producing as it does a value that
       can't be negative, is considered a non-arithmetic operation.  This
       is also convenient when -(INT_MAX+1) is used as a constant on
       twos complement machines; it avoids a warning. */
    result->non_arithmetic = TRUE;
  }  /* if */
  trunc_and_set_integer(&result_value, result, /*check_overflow=*/is_signed,
                        /*saturate_on_overflow=*/FALSE,
                        err_code, err_severity);
  if (microsoft_mode && *err_code != ec_no_error) {
    /* Do not make this an error in Microsoft mode. */
    *err_severity = es_warning;
  }  /* if */
#if DEBUG
  db_unary_operation("i-", constant, result, *err_code);
#endif /* DEBUG */
}  /* do_inegate */


static void do_fnegate(a_constant        *constant,
                       a_constant        *result,
                       an_error_code     *err_code,
                       an_error_severity *err_severity,
                       a_boolean         *depends_on_fp_mode)
/*
Do the negate operation on all types of float and imaginary values.
*/
{
  a_type_ptr   constant_type = skip_typerefs(constant->type);
  a_float_kind float_kind = constant_type->variant.float_kind;
  a_boolean    err;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  /* Use original constant kind in case it is ck_imaginary. */
  set_constant_kind(result, constant->kind);

  fp_negate(float_kind, &constant->variant.float_value,
            &result->variant.float_value, &err, depends_on_fp_mode);
  if (err) {
    *err_code = ec_bad_float_operation_result;
    *err_severity = es_error;
  }  /* if */

#if DEBUG
  db_unary_operation("f-", constant, result, *err_code);
#endif /* DEBUG */
}  /* do_fnegate */

#if FIXED_POINT_ALLOWED

static void do_fxnegate(a_constant        *constant,
                        a_constant        *result,
                        an_error_code     *err_code,
                        an_error_severity *err_severity)
/*
Do the negate operation on all types of fixed-point values.
*/
{
  a_boolean err;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  check_assertion(constant->kind == (a_constant_repr_kind)ck_fixed_point);
  set_constant_kind(result, (a_constant_repr_kind)ck_fixed_point);
  fxp_negate(&constant->variant.fixed_point_value,
             fxp_descr_for_constant(constant),
             &result->variant.fixed_point_value,
             fxp_descr_for_constant(result),
             &err);
  if (err) {
    *err_code = ec_bad_fixed_operation_result;
    *err_severity = ES_FIXED_POINT_OVERFLOW;
  }  /* if */

#if DEBUG
  db_unary_operation("fx-", constant, result, *err_code);
#endif /* DEBUG */
}  /* do_fxnegate */

#endif /* FIXED_POINT_ALLOWED */
#if C99_IL_EXTENSIONS_SUPPORTED

static void do_xnegate(a_constant        *constant,
                       a_constant        *result,
                       an_error_code     *err_code,
                       an_error_severity *err_severity,
                       a_boolean         *depends_on_fp_mode)
/*
Do the negate operation on all types of complex.
*/
{
  a_boolean    err;
  a_type_ptr   constant_type = skip_typerefs(constant->type);
  a_float_kind float_kind = constant_type->variant.float_kind;
  an_internal_complex_value
               cx;

  get_complex_val(constant, &cx);
  *err_code = ec_no_error;
  *err_severity = es_warning;
  set_constant_kind(result, (a_constant_repr_kind)ck_complex);
  cx_negate(float_kind, &cx, result->variant.complex_value,
            &err, depends_on_fp_mode);
  if (err) {
    *err_code = ec_bad_complex_operation_result;
    *err_severity = es_error;
  }  /* if */
#if DEBUG
  db_unary_operation("x-", constant, result, *err_code);
#endif /* DEBUG */
}  /* do_xnegate */

#endif /* C99_IL_EXTENSIONS_SUPPORTED */

static void do_complement(a_constant        *constant,
		          a_constant        *result,
			  an_error_code     *err_code,
			  an_error_severity *err_severity)
/*
Do the complement operation on all types of integers.
*/
{
  an_integer_value result_value;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  result_value = constant->variant.integer_value;
  complement_integer_value(&result_value);
  trunc_and_set_integer(&result_value, result, /*check_overflow=*/FALSE,
                        /*saturate_on_overflow=*/FALSE,
                        err_code, err_severity);
  result->non_arithmetic = TRUE;

#if DEBUG
  db_unary_operation("~", constant, result, *err_code);
#endif /* DEBUG */
}  /* do_complement */


static void do_not(a_constant        *constant,
		   a_constant        *result,
                   a_boolean         *did_not_fold)
/*
Do the "!" (not) operation on all types of scalars.
*/
{
  *did_not_fold = FALSE;
  if (!constant_bool_value_known_at_compile_time(constant)) {
    /* The constant's value is not known until link time, so the conversion
       cannot be folded at this time. */
    *did_not_fold = TRUE;
  } else {
    set_constant_kind(result, (a_constant_repr_kind)ck_integer);
    set_integer_value(&result->variant.integer_value,
                      (a_host_large_integer)is_false_constant(constant));
  }  /* if */
#if DEBUG
  if (*did_not_fold) {
    if (debug_level >= 5) fprintf(f_debug, "! did not fold\n");
  } else {
    db_unary_operation("!", constant, result, ec_no_error);
  }  /* if */
#endif /* DEBUG */
}  /* do_not */

#if C99_IL_EXTENSIONS_SUPPORTED

static void do_xconj(a_constant        *constant,
                     a_constant        *result,
                     an_error_code     *err_code,
                     an_error_severity *err_severity,
                     a_boolean         *depends_on_fp_mode)
/*
Do the complex conjugation operation (i.e., negate the imaginary part) on all
types of complex values.
*/
{
  a_type_ptr   constant_type = skip_typerefs(constant->type);
  a_float_kind float_kind = constant_type->variant.float_kind;
  a_boolean    err;
  an_internal_complex_value
               cx;

  get_complex_val(constant, &cx);

  *err_code = ec_no_error;
  *err_severity = es_warning;

  set_constant_kind(result, (a_constant_repr_kind)ck_complex);
  result->variant.complex_value->real = cx.real;
  fp_negate(float_kind, &cx.imag,
            &result->variant.complex_value->imag, &err, depends_on_fp_mode);
  if (err) {
    *err_code = ec_bad_complex_operation_result;
    *err_severity = es_error;
  }  /* if */

#if DEBUG
  db_unary_operation("x~", constant, result, *err_code);
#endif /* DEBUG */
}  /* do_xconj */


static void do_complex_projection(an_expr_operator_kind  op,
                                  a_constant             *constant,
                                  a_constant             *result)
/*
Extract the real or imaginary part of a complex constant.
*/
{
  an_internal_complex_value  cx;

  check_assertion(is_complex_type(constant->type) &&
                  is_real_floating_type(result->type));
  get_complex_val(constant, &cx);
  set_constant_kind(result, (a_constant_repr_kind)ck_float);
  if (op == (an_expr_operator_kind)eok_real_part) {
    result->variant.float_value = cx.real;
  } else {
    result->variant.float_value = cx.imag;
  }  /* if */
}  /* do_complex_projection */

#endif /* C99_IL_EXTENSIONS_SUPPORTED */

/*
Return TRUE if the indicated constant is an address constant cast to
an integral type.  Such a constant is a link-time constant but not a
compile-time constant.
*/
#define is_addr_constant_cast_to_integral_type(constant)              \
  ((constant)->kind == (a_constant_repr_kind)ck_address &&            \
   (constant)->implicit_cast &&                                       \
   is_integral_or_enum_type((constant)->type))

#if GNU_VECTOR_TYPES_ALLOWED

static void decompose_vector_unary_operation(
                                      an_expr_operator_kind op,
                                      a_constant            *constant,
                                      a_type_ptr            result_type,
                                      a_constant            *result,
                                      a_boolean             constant_context,
                                      a_boolean             evaluated_context,
                                      a_boolean             *did_not_fold,
                                      a_boolean             *template_constant,
                                      an_error_code         *error_detected,
                                      a_source_position     *err_pos)
/*
Called by unary_operation when the result type is a GNU vector type to
recursively perform the specified operation on each of the vector's
elements.  Produces in *result a vector ck_aggregate constant with the
number of elements specified by the vector type, allocating the element
constants in the current memory region.  See unary_operation for a full
description of the parameters.
*/
{
  a_constant_ptr opnd_elem;
  a_boolean      opnd_local_constant = FALSE;
  a_boolean      local_not_folded = FALSE;
  sizeof_t       num_result_elements;
  a_type_ptr     result_elem_type;
  sizeof_t       elem_no;

  /* Clone the operand constant and remove the operand elements to form the
     basis for the result. */
  copy_constant(constant, result);
  result->type = result_type;
  result->variant.aggregate.first_constant = NULL;
  result->variant.aggregate.last_constant = NULL;
  result_type = skip_typerefs(result_type);
  check_assertion(type_is(result_type, tk_vector) &&
                  constant_is(constant, ck_aggregate));
  result_elem_type = skip_typerefs(result_type->variant.vector.element_type);
  num_result_elements = num_vector_elements(result_type);
  if (op == eok_vector_not) {
    op = eok_not;
  } else if (op == eok_complement && is_bool_type(result_elem_type)) {
    /* A normal bool complement operation goes through integral promotion.
       Thus, ~true is equivalent to ~1, and if converted to bool would produce
       a true value again.  However, with Clang bool vectors, which represent
       elements as bits, eok_compl has the same effect as eok_not. */
    op = eok_not;
  }  /* if */
  /* Loop over the operand elements, calling unary_operation to compute
     the values of the result elements. */
  opnd_elem = constant->variant.aggregate.first_constant;
  for (elem_no = 0; elem_no < num_result_elements; ++elem_no) {
    /* Allocate the constant for this element of the result.  The constant
       will be overwritten by unary_operation below, so just call it an
       error constant for now to minimize the cost of initialization. */
    a_constant_ptr  result_elem = alloc_constant(ck_error);
    if (opnd_elem == NULL) {
      /* A vector aggregate may be partially- or value-initialized.  If so,
         the operand element will be NULL at this point if we've stepped
         past the end of the list, and we need to create a zero of the
         element type to use for the rest of the loop. */
      opnd_elem = local_constant();
      opnd_local_constant = TRUE;
      make_zero_of_proper_type(result_elem_type, opnd_elem);
    }  /* if */
    /* Recursively call unary_operation to compute the result for this
       element and add the element to the result aggregate. */
    unary_operation(op, opnd_elem, result_elem_type, result_elem,
                    constant_context, evaluated_context, &local_not_folded,
                    template_constant, error_detected, err_pos);
    if (local_not_folded) break;
    if (op == eok_not && !is_zero_constant(result_elem) &&
        !is_bool_type(result_elem_type)) {
      /* Turn "1" into "-1", because that appears to be the behavior of GCC.
         Do not do this with Clang "ext_vector_type" vectors of bool (GCC
         vectors never have bool element type). */
      check_assertion(constant_is(result_elem, ck_integer));
      set_integer_value(&result_elem->variant.integer_value,
                        (a_host_large_integer)-1);
    }  /* if */
    add_constant_to_aggregate(result_elem, result, (a_base_class_ptr)NULL,
                              (a_field_ptr)NULL);
    /* Step to the next element, unless we already ran off the end of the
       operand aggregate and are using a local zero. */
    if (!opnd_local_constant) {
      opnd_elem = opnd_elem->next;
    }  /* if */
  }  /* for */
  /* Release the local constant, if one was created. */
  if (opnd_local_constant) {
    release_local_constant(&opnd_elem);
  }  /* if */
  /* Propagate the result back to the caller. */
  *did_not_fold = local_not_folded;
}  /* decompose_vector_unary_operation */

#endif /* GNU_VECTOR_TYPES_ALLOWED */

void unary_operation(an_expr_operator_kind op,
		     a_constant            *constant,
                     a_type_ptr            result_type,
		     a_constant            *result,
                     a_boolean             constant_context,
                     a_boolean             evaluated_context,
                     a_boolean             *did_not_fold,
                     a_boolean             *template_constant,
                     an_error_code         *error_detected,
                     a_source_position     *err_pos)
/*
Fold unary operations on constants.  op indicates the operation,
constant the operand.  result_type indicates the desired result type.
The result constant is put into result.  If constant_context is FALSE,
this operation is being evaluated as part of a nonconstant expression,
so any error is reduced to a warning and *did_not_fold is returned TRUE.
If evaluated_context is FALSE, this operation is being done in a
not-evaluated context (e.g., a sizeof or a dead branch of a "?" operator),
so any error is thrown away and *did_not_fold is returned TRUE.
*did_not_fold is also returned TRUE if the operation could not be
folded for any other reason (*template_constant is returned TRUE if
the reason is that the constant is a template parameter constant).
If error_detected is non-NULL, set *error_detected to the code for any
error detected, and do not issue the diagnostic, or set it to
ec_no_error if there was no error.  *err_pos is used as the position
for any diagnostics issued.
*/
{
  an_error_code     err_code;
  an_error_severity err_severity;
  a_boolean         depends_on_fp_mode = FALSE;

  db_enter(5, "unary_operation");

  *did_not_fold = FALSE;
  *template_constant = FALSE;
  if (error_detected != NULL) *error_detected = ec_no_error;
  err_code = ec_no_error;
  err_severity = es_warning;
  if (is_error_constant(constant)) {
    /* The constant is an error constant; set the result to an error
       constant and return. */
    set_error_constant(result);
  } else if (!C_mode() &&
             (constant_is(constant, ck_template_param) ||
              (context_may_have_dependent_types() &&
               is_template_dependent_type(constant->type)))) {
    /* An operation on a template parameter constant cannot be folded. */
    *did_not_fold = TRUE;
    *template_constant = TRUE;
#if UPC_EXTENSIONS_ALLOWED  
  } else if (constant_is(constant, ck_upc_mythread) ||  
             constant_is(constant, ck_upc_threads)) {  
    /* The UPC pseudo-constants are not true constants.  As a result, we do
       not fold unary operations involving these constants. */
    *did_not_fold = TRUE;  
#endif /* UPC_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  } else if (constant_is(constant, ck_label_difference)) {
    /* The representation for a GNU label difference (&&K-&&L) is not
       a constant known at compile time. */
    *did_not_fold = TRUE;
#if GNU_VECTOR_TYPES_ALLOWED
  } else if (is_vector_type(result_type)) {
    /* Perform the operation recursively on each of the elements of the
       vector. */
    decompose_vector_unary_operation(op, constant, result_type, result,
                                     constant_context, evaluated_context,
                                     did_not_fold, template_constant,
                                     error_detected, err_pos);
#endif /* GNU_VECTOR_TYPES_ALLOWED */
#endif /* GNU_EXTENSIONS_ALLOWED */
  } else {
    clear_constant(result, (a_constant_repr_kind)ck_error);
    result->type = result_type;
    if (is_addr_constant_cast_to_integral_type(constant)) {
      /* An address constant cast to an integral type is a link-time
         constant, not a compile-time constant.  We cannot do operations
         on it. */
      *did_not_fold = TRUE;
    } else {
      a_type_kind  type_kind = skip_typerefs(constant->type)->kind;
      switch (op) {
        case eok_negate:
          switch (type_kind) {
            case tk_integer:
              do_inegate(constant, result, &err_code, &err_severity);
              break;
#if FIXED_POINT_ALLOWED
            case tk_fixed_point:
              do_fxnegate(constant, result, &err_code, &err_severity);
              break;
#endif /* FIXED_POINT_ALLOWED */
            case tk_float:
#if C99_IL_EXTENSIONS_SUPPORTED
            case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
              do_fnegate(constant, result, &err_code, &err_severity,
                         &depends_on_fp_mode);
              break;
#if C99_IL_EXTENSIONS_SUPPORTED
            case tk_complex:
              do_xnegate(constant, result, &err_code, &err_severity,
                         &depends_on_fp_mode);
              break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
            default:
              unexpected_condition();
          }  /* switch */
          break;
        case eok_unary_plus:
          copy_constant(constant, result);
          break;
        case eok_complement:
          do_complement(constant, result, &err_code, &err_severity);
          break;
        case eok_not:
          do_not(constant, result, did_not_fold);
          break;
#if C99_IL_EXTENSIONS_SUPPORTED
        case eok_xconj:
          do_xconj(constant, result, &err_code, &err_severity,
                   &depends_on_fp_mode);
          break;
        case eok_real_part:
        case eok_imag_part:
          do_complex_projection(op, constant, result);
          break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
        default:
          unexpected_condition_str("unary_operation: bad unary operator");
      }  /* switch */
    }  /* if */
    if (err_code != ec_no_error) {
      /* There was an error or warning. */
      issue_folding_diagnostic(err_code, err_severity, constant_context,
                               evaluated_context, /*silence_warning=*/FALSE,
                               did_not_fold, error_detected, err_pos, result);
      if (err_severity == es_error) depends_on_fp_mode = FALSE;
    }  /* if */
    /* If the source constant was formed using operations that are not allowed
       in forming a null pointer constant, the result cannot be used as a null
       pointer constant.  In C++11 mode, only a literal can produce a null
       pointer constant. */
    result->null_pointer_constant_ruled_out =
                          (cpp11_mode && !(microsoft_mode && ms_permissive) &&
                           !gpp_version_is(<60000)) ||
                          constant->null_pointer_constant_ruled_out ||
                          !constant_is(constant, ck_integer) ||
                          constant->implicit_cast;
    if (depends_on_fp_mode && !constant_context) {
      /* In a non-constant context, leave an operation to be done at runtime
         if its result depends on the floating-point mode. */
      *did_not_fold = TRUE;
    }  /* if */
  }  /* if */

  db_exit();
}  /* unary_operation */


#if DEBUG
static void db_binary_operation(a_const_char   *operation,
				a_constant_ptr constant_1,
				a_constant_ptr constant_2,
				a_constant_ptr result,
				an_error_code  err_code)
/*
Do a debug print giving the result of folding a two-operand constant operation.
*/
{
  if (db_flag_is_set("folding") || debug_level >= 5) {
    db_constant(constant_1);
    fprintf(f_debug, " %s ", operation);
    db_constant(constant_2);
    fprintf(f_debug, ", result = ");
    db_constant(result);
    if (err_code != ec_no_error) {
      fprintf(f_debug, " with ");
      if (err_code == ec_integer_overflow) {
        fprintf(f_debug, "integer overflow");
      } else if (err_code == ec_divide_by_zero) {
        fprintf(f_debug, "divide by zero");
      } else if (err_code == ec_mod_by_zero) {
        fprintf(f_debug, "mod by zero");
      } else {
        fprintf(f_debug, "error");
      }  /* if */
    }  /* if */
    fprintf(f_debug, "\n");
  }  /* if */
}  /* db_binary_operation */
#endif /* DEBUG */


static void do_iadd(a_constant        *constant_1,
		    a_constant        *constant_2,
		    a_constant        *result,
		    an_error_code     *err_code,
		    an_error_severity *err_severity)
/*
Do the addition operation on all types of integers.
*/
{
  an_integer_value result_value;
  a_boolean        is_signed, err;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  result_value = constant_1->variant.integer_value;
  is_signed = int_constant_is_signed(constant_1);
  add_integer_values(&result_value, &constant_2->variant.integer_value,
                     is_signed, &err);
  if (err && is_signed) {
    *err_code = ec_integer_overflow;
    *err_severity = ES_INT_OVERFLOW;
  }  /* if */
  trunc_and_set_integer(&result_value, result, /*check_overflow=*/is_signed,
                        /*saturate_on_overflow=*/FALSE,
                        err_code, err_severity);

#if DEBUG
  db_binary_operation("i+", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_iadd */


static void do_isubtract(a_constant        *constant_1,
		         a_constant        *constant_2,
		         a_constant        *result,
		         an_error_code     *err_code,
			 an_error_severity *err_severity)
/*
Do the subtract operation on all types of integers.
*/
{
  an_integer_value result_value;
  a_boolean        is_signed, err;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  result_value = constant_1->variant.integer_value;
  is_signed = int_constant_is_signed(constant_1);
  subtract_integer_values(&result_value, &constant_2->variant.integer_value,
                          is_signed, &err);
  if (err && is_signed) {
    *err_code = ec_integer_overflow;
    *err_severity = ES_INT_OVERFLOW;
  }  /* if */
  trunc_and_set_integer(&result_value, result, /*check_overflow=*/is_signed,
                        /*saturate_on_overflow=*/FALSE,
                        err_code, err_severity);

#if DEBUG
  db_binary_operation("i-", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_isubtract */


static void do_imultiply(a_constant        *constant_1,
		         a_constant        *constant_2,
		         a_constant        *result,
		         an_error_code     *err_code,
			 an_error_severity *err_severity)
/*
Do the multiply operation on all types of integers.
*/
{
  an_integer_value result_value;
  a_boolean        is_signed, err;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  result_value = constant_1->variant.integer_value;
  is_signed = int_constant_is_signed(constant_1);
  multiply_integer_values(&result_value, &constant_2->variant.integer_value,
                          is_signed, &err);
  if (err && is_signed) {
    *err_code = ec_integer_overflow;
    *err_severity = ES_INT_OVERFLOW;
  }  /* if */
  trunc_and_set_integer(&result_value, result, /*check_overflow=*/is_signed,
                        /*saturate_on_overflow=*/FALSE,
                        err_code, err_severity);

#if DEBUG
  db_binary_operation("i*", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_imultiply */


static void do_idivide(a_constant        *constant_1,
		       a_constant        *constant_2,
		       a_constant        *result,
		       an_error_code     *err_code,
		       an_error_severity *err_severity)
/*
Do the divide operation on all types of integers.
*/
{
  an_integer_value result_value;
  a_boolean        is_signed, err;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  result_value = constant_1->variant.integer_value;
  is_signed = int_constant_is_signed(constant_1);
  divide_integer_values(&result_value, &constant_2->variant.integer_value,
                        is_signed, &err);
  if (err) {
    if (cmplit_integer_constant(constant_2, (a_host_large_integer)0) == 0) {
      /* Division by zero. */
      *err_code = ec_divide_by_zero;
      *err_severity = es_error;
    } else if (is_signed) {
      /* Other overflow. */
      *err_code = ec_integer_overflow;
      *err_severity = ES_INT_OVERFLOW;
    }  /* if */
  }  /* if */
  trunc_and_set_integer(&result_value, result, /*check_overflow=*/is_signed,
                        /*saturate_on_overflow=*/FALSE,
                        err_code, err_severity);

#if DEBUG
  db_binary_operation("i/", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_idivide */


static void do_remainder(a_constant        *constant_1,
		         a_constant        *constant_2,
		         a_constant        *result,
		         an_error_code     *err_code,
			 an_error_severity *err_severity)
/*
Do the remainder operation ("%") on all types of integers.
*/
{
  an_integer_value result_value;
  a_boolean        is_signed, err;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  result_value = constant_1->variant.integer_value;
  is_signed = int_constant_is_signed(constant_1);
  remainder_integer_values(&result_value, &constant_2->variant.integer_value,
                           is_signed, &err);
  if (err) {
    if (cmplit_integer_constant(constant_2, (a_host_large_integer)0) == 0) {
      /* Division (remainder) by zero. */
      *err_code = ec_mod_by_zero;
      *err_severity = es_error;
    } else if (is_signed) {
      /* Other overflow. */
      *err_code = ec_integer_overflow;
      *err_severity = ES_INT_OVERFLOW;
    }  /* if */
  }  /* if */
  trunc_and_set_integer(&result_value, result, /*check_overflow=*/is_signed,
                        /*saturate_on_overflow=*/FALSE,
                        err_code, err_severity);

#if DEBUG
  db_binary_operation("%", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_remainder */


void check_shift_count(a_constant    *shift_count_constant,
                       a_type_ptr    operand_type,
                       an_error_code *err_code)
/*
shift_count_constant is the constant shift count for a shift operation.
The entity being shifted has the type operand_type.  Check the shift
count to see if it is valid.  If so, return *err_code set to ec_no_error;
if not, return *err_code set to the proper error code.
*/
{
  a_targ_size_t size;

  *err_code = ec_no_error;

  if (constant_is(shift_count_constant, ck_integer)) {
    /* Determine the size of the operand being shifted. */
    operand_type = skip_typerefs(operand_type);
#if CHECKING
    if (!type_is(operand_type, tk_integer)
#if FIXED_POINT_ALLOWED
        && !type_is(operand_type, tk_fixed_point)
#endif /* FIXED_POINT_ALLOWED */
#if GNU_VECTOR_TYPES_ALLOWED
        && !type_is(operand_type, tk_vector)
#endif /* GNU_VECTOR_TYPES_ALLOWED */
                                                       ) {
      internal_error("check_shift_count: operand_type not integer");
    } else if (operand_type->size == 0) {
      internal_error("check_shift_count: integer type has size 0");
    }  /* if */
#endif /* CHECKING */
#if GNU_VECTOR_TYPES_ALLOWED
    if (type_is(operand_type, tk_vector)) {
      a_type_ptr element_type = operand_type->variant.vector.element_type;
      element_type = skip_typerefs(element_type);
      if (type_is(element_type, tk_integer)) {
        size = integer_value_bit_size_for_type(element_type);
      } else {
        size = element_type->size * targ_char_bit;
      }  /* if */
    } else
#endif /* GNU_VECTOR_TYPES_ALLOWED */
    /* Do not insert code here. */
    {
      if (type_is(operand_type, tk_integer)) {
        size = integer_value_bit_size_for_type(operand_type);
      } else {
        size = operand_type->size * targ_char_bit;
      }  /* if */
    }  /* if */

    if (sign_of_integer_constant(shift_count_constant) < 0) {
      /* Negative shift count. */
      *err_code = ec_negative_shift_count;
    } else if (cmplit_integer_constant(shift_count_constant,
                                       (a_host_large_integer)size) >= 0) {
      /* Shift count is too large. */
      *err_code = ec_shift_count_too_large;
    }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  } else if (constant_is(shift_count_constant, ck_label_difference)) {
    /* Unknown value: No check possible. */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if UPC_EXTENSIONS_ALLOWED
  } else if (constant_is(shift_count_constant, ck_upc_threads)) {
    /* Unknown value: No check possible. */
#endif /* UPC_EXTENSIONS_ALLOWED */
  } else {
    unexpected_condition_str("check_shift_count: unexpected constant kind");
  }  /* if */
}  /* check_shift_count */


static void do_shift(a_constant        *constant_1,
		     a_constant        *constant_2,
		     a_constant        *result,
		     a_boolean         shift_right,
		     an_error_code     *err_code,
		     an_error_severity *err_severity)
/*
Low-level routine to do a left or right shift on an integer.  Shift
*constant_1 by *constant_2 (right if shift_right is TRUE, left otherwise),
and put the result in *result.  *err_code and *err_severity are set to
indicate any error/warning detected, or *err_code == ec_no_error if
everything went fine.
*/
{
  an_integer_value result_value;
  a_boolean        is_signed, err, too_large = FALSE;
  int              shift_count, extra_shift_count = 0;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  check_shift_count(constant_2, constant_1->type, err_code);
  if (*err_code != ec_no_error) {
    /* Something wrong with the shift count. */
    if (*err_code == ec_shift_count_too_large) {
      /* The shift count is too large. */
      too_large = TRUE;
      if (microsoft_mode || gnu_mode) {
        /* Too-large shift counts are only warnings in Microsoft and GNU
           modes. */
      } else {
        *err_severity = es_error;
      }  /* if */
    } else {
      /* Other errors (e.g., negative shift count) are always errors.
         Microsoft doesn't give errors, but it's not clear what they do,
         so we don't try to emulate the folding. */
      *err_severity = es_error;
    }  /* if */
  }  /* if */
  if (*err_severity != es_error) {
    /* Fold the shift. */
    result_value = constant_1->variant.integer_value;
    if (too_large) {
      /* Adjust the shift count for a too-large value. */
      a_type_ptr object_type = skip_typerefs(constant_1->type);
      int        object_bit_size =
                             (int)integer_value_bit_size_for_type(object_type);
      if (targ_too_large_shift_count_is_taken_modulo_size) {
        /* We're supposed to reduce the shift count modulo the bit size
           of the object. */
        shift_count = (int)value_of_integer_constant(constant_2, &err);
        if (err) {
          /* The number is huge.  Give up. */
          *err_severity = es_error;
          goto end_of_folding;
        }  /* if */
        shift_count %= object_bit_size;
      } else {
        /* We're supposed to treat the shift count as if we really shift
           that many bits.  Just shift the amount beyond which we would
           not get any further change, i.e., the number of bits in the
           object.  Do it in two steps so that the low-level routines
           need not deal with the odd cases. */
        shift_count = object_bit_size-1;
        extra_shift_count = 1;
      }  /* if */
    } else {
      /* Normal shift count, not too big. */
      shift_count = (int)value_of_integer_constant(constant_2, &err);
      /* No need to check err because check_shift_count has already
         established that the shift count is reasonable. */
    }  /* if */
    if (shift_right) {
      /* Shift right. */
      is_signed = int_constant_is_signed(constant_1);
      /* If the operation is signed but the shift is unsigned, mask off the
         high order bits of the integer value that are not actually part of
         the value to be shifted.  This prevents those high order bits from
         being shifted in to the result. */
      if (is_signed && !targ_right_shift_is_arithmetic) {
        an_integer_kind  tmp_ikind;
        a_boolean        tmp_is_signed;
        size_t           tmp_bit_size;
        an_integer_value mask;
        /* Determine attributes (size, signedness) of the new integer kind. */
        get_integer_attributes(result, &tmp_ikind, &tmp_is_signed,
                               &tmp_bit_size);
        if (tmp_bit_size < BITS_IN_AN_INTEGER_VALUE) {
          make_integer_value_mask(&mask, tmp_bit_size);
          and_integer_values(&result_value, &mask);
        }  /* if */
      }  /* if */
      shift_right_integer_value(&result_value, shift_count, is_signed,
                               /*sign_extend=*/targ_right_shift_is_arithmetic);
      if (extra_shift_count != 0) {
        shift_right_integer_value(&result_value, extra_shift_count, is_signed,
                               /*sign_extend=*/targ_right_shift_is_arithmetic);
      }  /* if */
    } else {
      /* Shift left. */
      shift_left_integer_value(&result_value, shift_count, &err);
      if (extra_shift_count != 0) {
        shift_left_integer_value(&result_value, extra_shift_count, &err);
      }  /* if */
      if (err && !integer_value_can_represent_type_width(result->type)) {
        *err_code = ec_integer_overflow;
        *err_severity = ES_INT_OVERFLOW;
        goto end_of_folding;
      }  /* if */
    }  /* if */
    trunc_and_set_integer(&result_value, result, /*check_overflow=*/FALSE,
                          /*saturate_on_overflow=*/FALSE,
                          err_code, err_severity);
  }  /* if */
end_of_folding:;
}  /* do_shift */


static void do_shiftr(a_constant        *constant_1,
		      a_constant        *constant_2,
		      a_constant        *result,
		      an_error_code     *err_code,
		      an_error_severity *err_severity)
/*
Do the shift right operation on all types of integers.
*/
{
  do_shift(constant_1, constant_2, result, /*shift_right=*/TRUE,
	   err_code, err_severity);

#if DEBUG
  db_binary_operation(">>", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_shiftr */


static void do_shiftl(a_constant        *constant_1,
		      a_constant        *constant_2,
		      a_constant        *result,
		      an_error_code     *err_code,
		      an_error_severity *err_severity)
/*
Do the shift left operation on all types of integers.
*/
{
  do_shift(constant_1, constant_2, result, /*shift_right=*/FALSE,
	   err_code, err_severity);

#if DEBUG
  db_binary_operation("<<", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_shiftl */

#if FIXED_POINT_ALLOWED

static void do_fxshift(a_constant        *constant_1,
		       a_constant        *constant_2,
		       a_constant        *result,
		       a_boolean         shift_right,
		       an_error_code     *err_code,
		       an_error_severity *err_severity)
/*
Low-level routine to do a left or right shift on a fixed-point value.
Shift *constant_1 by *constant_2 (right if shift_right is TRUE, left
otherwise), and put the result in *result.  *err_code and *err_severity are
set to indicate any error/warning detected, or *err_code == ec_no_error if
everything went fine.
*/
{
  int		shift_count;
  a_boolean	err;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  check_shift_count(constant_2, constant_1->type, err_code);
  if (*err_code != ec_no_error) {
    /* Something wrong with the shift count. */
    *err_severity = es_error;
  }  /* if */
  if (*err_severity != es_error) {
    /* Fold the shift. */
    shift_count = (int)value_of_integer_constant(constant_2, &err);
    /* No need to check err because check_shift_count has already
       established that the shift count is reasonable. */
    fxp_shift(constant_1, shift_count, result, shift_right, &err);
    if (err) {
      *err_code = ec_bad_fixed_operation_result;
      *err_severity = es_error;
    }  /* if */
  }  /* if */
}  /* do_fxshift */


static void do_fxshiftr(a_constant        *constant_1,
		        a_constant        *constant_2,
		        a_constant        *result,
		        an_error_code     *err_code,
		        an_error_severity *err_severity)
/*
Do the shift right operation on fixed-point values.
*/
{
  do_fxshift(constant_1, constant_2, result, /*shift_right=*/TRUE,
	     err_code, err_severity);

#if DEBUG
  db_binary_operation("fx>>", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_fxshiftr */


static void do_fxshiftl(a_constant        *constant_1,
		        a_constant        *constant_2,
		        a_constant        *result,
		        an_error_code     *err_code,
		        an_error_severity *err_severity)
/*
Do the shift left operation on fixed-point values.
*/
{
  do_fxshift(constant_1, constant_2, result, /*shift_right=*/FALSE,
	     err_code, err_severity);

#if DEBUG
  db_binary_operation("fx<<", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_fxshiftl */

#endif /* FIXED_POINT_ALLOWED */

static void do_icompare(a_constant            *constant_1,
                        an_expr_operator_kind op,
                        a_constant            *constant_2,
                        a_constant            *result)
/*
Compare integers constant_1 and constant_2 according to the relational
operator "op", and return a 0 or 1 integer in "result".
*/
{
  int	cmp;
  int	result_value = 0;

  /* Develop a strcmp-like relation value in cmp:
       constant_1 > constant_2   1
       constant_1 = constant_2   0
       constant_1 < constant_2  -1
  */
  cmp = cmp_integer_constants(constant_1, constant_2);
  /* Now determine the result value for this particular operator. */
  switch (op) {
    case eok_eq:  result_value = (cmp == 0); break;
    case eok_ne:  result_value = (cmp != 0); break;
    case eok_gt:  result_value = (cmp >  0); break;
    case eok_lt:  result_value = (cmp <  0); break;
    case eok_ge:  result_value = (cmp >= 0); break;
    case eok_le:  result_value = (cmp <= 0); break;
    default:      unexpected_condition_str("do_icompare: bad operator");
  }  /* switch */
  set_constant_kind(result, (a_constant_repr_kind)ck_integer);
  set_integer_value(&result->variant.integer_value,
                    (a_host_large_integer)result_value);

#if DEBUG
  db_binary_operation(db_operator_names[op],
                      constant_1, constant_2, result, ec_no_error);
#endif /* DEBUG */
}  /* do_icompare */

#if GNU_EXTENSIONS_ALLOWED

static void do_ignu_min_max(a_constant            *constant_1,
                            an_expr_operator_kind op,
                            a_constant            *constant_2,
                            a_constant            *result)
/*
Compare integers constant_1 and constant_2 and return the minimum or maximum
in result (depending on which operator is indicated by op).  This folds the
GNU C++ minimum and maximum operators ("<?" and ">?").
*/
{
  if (cmp_integer_constants(constant_1, constant_2) <= 0) {
    /* The first constant is no larger than the second one. */
    if (op == (an_expr_operator_kind)eok_gnu_min) {
      copy_constant(constant_1, result);
    } else {
      copy_constant(constant_2, result);
    }  /* if */
  } else {
    /* The first constant is larger. */
    if (op == (an_expr_operator_kind)eok_gnu_min) {
      copy_constant(constant_2, result);
    } else {
      copy_constant(constant_1, result);
    }  /* if */
  }  /* if */

#if DEBUG
  db_binary_operation(db_operator_names[op],
                      constant_1, constant_2, result, ec_no_error);
#endif /* DEBUG */
}  /* do_ignu_min_max */

#endif /* GNU_EXTENSIONS_ALLOWED */

static void do_and(a_constant    *constant_1,
		   a_constant    *constant_2,
		   a_constant    *result)
/*
Do the bitwise "and" operation on all types of integers.
*/
{
  an_integer_value result_value;

  result_value = constant_1->variant.integer_value;
  and_integer_values(&result_value, &constant_2->variant.integer_value);
  set_constant_kind(result, (a_constant_repr_kind)ck_integer);
  result->variant.integer_value = result_value;
  result->non_arithmetic = TRUE;
#if DEBUG
  db_binary_operation("&", constant_1, constant_2, result, ec_no_error);
#endif /* DEBUG */
}  /* do_and */


static void do_or(a_constant    *constant_1,
		  a_constant    *constant_2,
		  a_constant    *result)
/*
Do the bitwise "or" operation on all types of integers.
*/
{
  an_integer_value result_value;

  result_value = constant_1->variant.integer_value;
  or_integer_values(&result_value, &constant_2->variant.integer_value);
  set_constant_kind(result, (a_constant_repr_kind)ck_integer);
  result->variant.integer_value = result_value;
  result->non_arithmetic = TRUE;
#if DEBUG
  db_binary_operation("|", constant_1, constant_2, result, ec_no_error);
#endif /* DEBUG */
}  /* do_or */


static void do_xor(a_constant    *constant_1,
		   a_constant    *constant_2,
		   a_constant    *result)
/*
Do the bitwise "xor" operation on all types of integers.
*/
{
  an_integer_value result_value;

  result_value = constant_1->variant.integer_value;
  xor_integer_values(&result_value, &constant_2->variant.integer_value);
  set_constant_kind(result, (a_constant_repr_kind)ck_integer);
  result->variant.integer_value = result_value;
  result->non_arithmetic = TRUE;
#if DEBUG
  db_binary_operation("^", constant_1, constant_2, result, ec_no_error);
#endif /* DEBUG */
}  /* do_xor */


static void do_land(a_constant    *constant_1,
		    a_constant    *constant_2,
		    a_constant    *result,
                    a_boolean     *did_not_fold)
/*
Do the logical "and" (&&) operation on integers, floats, and pointers.
*/
{
  int res = 0;

  *did_not_fold = FALSE;
  /* Fold the operation.  If either constant is a link-time constant, it
     may not be possible to fold at this time. */
  if (!constant_bool_value_known_at_compile_time(constant_1)) {
    *did_not_fold = TRUE;
  } else if (is_false_constant(constant_1)) {
    res = 0;
  } else if (!constant_bool_value_known_at_compile_time(constant_2)) {
    *did_not_fold = TRUE;
  } else if (is_false_constant(constant_2)) {
    res = 0;
  } else {
    res = 1;
  }  /* if */
  if (!*did_not_fold) {
    set_constant_kind(result, (a_constant_repr_kind)ck_integer);
    set_integer_value(&result->variant.integer_value,
                      (a_host_large_integer)res);
  }  /* if */
#if DEBUG
  if (*did_not_fold) {
    if (debug_level >= 5) fprintf(f_debug, "&& did not fold\n");
  } else {
    db_binary_operation("&&", constant_1, constant_2, result, ec_no_error);
  }  /* if */
#endif /* DEBUG */
}  /* do_land */


static void do_lor(a_constant    *constant_1,
		   a_constant    *constant_2,
		   a_constant    *result,
                   a_boolean     *did_not_fold)
/*
Do the logical "or" (||) operation on integers, floats, and pointers.
*/
{
  int res = 0;

  *did_not_fold = FALSE;
  /* Fold the operation.  If either constant is a link-time constant, it
     may not be possible to fold at this time. */
  if (!constant_bool_value_known_at_compile_time(constant_1)) {
    *did_not_fold = TRUE;
  } else if (!is_false_constant(constant_1)) {
    res = 1;
  } else if (!constant_bool_value_known_at_compile_time(constant_2)) {
    *did_not_fold = TRUE;
  } else if (!is_false_constant(constant_2)) {
    res = 1;
  } else {
    res = 0;
  }  /* if */
  if (!*did_not_fold) {
    set_constant_kind(result, (a_constant_repr_kind)ck_integer);
    set_integer_value(&result->variant.integer_value,
                      (a_host_large_integer)res);
  }  /* if */
#if DEBUG
  if (*did_not_fold) {
    if (debug_level >= 5) fprintf(f_debug, "|| did not fold\n");
  } else {
    db_binary_operation("||", constant_1, constant_2, result, ec_no_error);
  }  /* if */
#endif /* DEBUG */
}  /* do_lor */


static void do_fadd(a_constant        *constant_1,
		    a_constant        *constant_2,
		    a_constant        *result,
		    an_error_code     *err_code,
		    an_error_severity *err_severity,
                    a_boolean         *depends_on_fp_mode)
/*
Do the addition operation on all types of float and imaginary values.
*/
{
  a_boolean    err;
  a_type_ptr   constant_type = skip_typerefs(constant_1->type);
  a_float_kind float_kind = constant_type->variant.float_kind;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  check_assertion(constant_1->kind == constant_2->kind);
  set_constant_kind(result, constant_1->kind);
  fp_add(float_kind,
         &constant_1->variant.float_value,
         &constant_2->variant.float_value,
         &result->variant.float_value, &err,
         depends_on_fp_mode);
  if (err) {
    *err_code = ec_bad_float_operation_result;
    *err_severity = es_error;
  }  /* if */

#if DEBUG
  db_binary_operation("f+", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_fadd */


static void do_fsubtract(a_constant        *constant_1,
		         a_constant        *constant_2,
		         a_constant        *result,
		         an_error_code     *err_code,
			 an_error_severity *err_severity,
                         a_boolean         *depends_on_fp_mode)
/*
Do the subtraction operation on all types of float and imaginary values.
*/
{
  a_boolean    err;
  a_type_ptr   constant_type = skip_typerefs(constant_1->type);
  a_float_kind float_kind = constant_type->variant.float_kind;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  check_assertion(constant_1->kind == constant_2->kind);
  set_constant_kind(result, constant_1->kind);
  fp_subtract(float_kind,
              &constant_1->variant.float_value,
              &constant_2->variant.float_value,
              &result->variant.float_value, &err,
              depends_on_fp_mode);
  if (err) {
    *err_code = ec_bad_float_operation_result;
    *err_severity = es_error;
  }  /* if */

#if DEBUG
  db_binary_operation("f-", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_fsubtract */


static void do_fmultiply(a_constant        *constant_1,
                         a_constant        *constant_2,
                         a_constant        *result,
                         an_error_code     *err_code,
                         an_error_severity *err_severity,
                         a_boolean         *depends_on_fp_mode)
/*
Do the multiplication operation on all types of float.
*/
{
  a_boolean    err;
  a_type_ptr   constant_type = skip_typerefs(constant_1->type);
  a_float_kind float_kind = constant_type->variant.float_kind;
  a_constant_repr_kind
               result_kind = (a_constant_repr_kind)ck_float;

  *err_code = ec_no_error;
  *err_severity = es_warning;

#if C99_IL_EXTENSIONS_SUPPORTED
  /* Imaginary times float gives an imaginary result. */
  if ((constant_1->kind == (a_constant_repr_kind)ck_imaginary) !=
      (constant_2->kind == (a_constant_repr_kind)ck_imaginary)) {
    result_kind = (a_constant_repr_kind)ck_imaginary;
  }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  set_constant_kind(result, result_kind);
  fp_multiply(float_kind,
              &constant_1->variant.float_value,
              &constant_2->variant.float_value,
              &result->variant.float_value, &err,
              depends_on_fp_mode);
  if (err) {
    *err_code = ec_bad_float_operation_result;
    *err_severity = es_error;
  }  /* if */

#if DEBUG
  db_binary_operation("f*", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_fmultiply */


static void do_fdivide(a_constant        *constant_1,
		       a_constant        *constant_2,
		       a_constant        *result,
		       an_error_code     *err_code,
		       an_error_severity *err_severity,
                       a_boolean         *depends_on_fp_mode)
/*
Do the division operation on all types of float.
*/
{
  a_boolean    err;
  a_type_ptr   constant_type = skip_typerefs(constant_1->type);
  a_float_kind float_kind = constant_type->variant.float_kind;
  a_constant_repr_kind
               result_kind = (a_constant_repr_kind)ck_float;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  /* Check for division by zero to give a specific error message. */
  if (!IEEE_handling_on_float_operation_exceptions &&
      fp_is_zero_constant(float_kind, &constant_2->variant.float_value)) {
    *err_code = ec_divide_by_zero;
    *err_severity = es_error;
  } else {
#if C99_IL_EXTENSIONS_SUPPORTED
    /* Imaginary divided by float gives an imaginary result. */
    if ((constant_1->kind == (a_constant_repr_kind)ck_imaginary) !=
        (constant_2->kind == (a_constant_repr_kind)ck_imaginary)) {
      result_kind = (a_constant_repr_kind)ck_imaginary;
    }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    set_constant_kind(result, result_kind);
    fp_divide(float_kind,
              &constant_1->variant.float_value,
              &constant_2->variant.float_value,
              &result->variant.float_value, &err,
              depends_on_fp_mode);
    if (err) {
      *err_code = ec_bad_float_operation_result;
      *err_severity = es_error;
    }  /* if */
  }  /* if */

#if DEBUG
  db_binary_operation("f/", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_fdivide */


static void do_fcompare(a_constant            *constant_1,
                        an_expr_operator_kind op,
                        a_constant            *constant_2,
                        a_constant            *result,
                        a_boolean             *depends_on_fp_mode)
/*
Compare floating constants constant_1 and constant_2 according to the
relational operator "op", and return a 0 or 1 integer in "result".
*/
{
  int          cmp;
  int          result_value = 0;
  a_boolean    unordered;
  a_float_kind float_kind =
                           skip_typerefs(constant_1->type)->variant.float_kind;

   *depends_on_fp_mode = FALSE;
  /* Develop a strcmp-like relation value in cmp:
       constant_1 > constant_2   1
       constant_1 = constant_2   0
       constant_1 < constant_2  -1
     "unordered" is set if the two values are unordered with respect to one
     another.
  */
  cmp = fp_compare(float_kind,
                   &constant_1->variant.float_value,
                   &constant_2->variant.float_value,
                   &unordered);
  /* Now determine the result value for this particular operator. */
  if (unordered) {
   *depends_on_fp_mode = TRUE;
   if (op == (an_expr_operator_kind)eok_ne) {
     /* If two values are unordered, they are unequal.  This is needed for
        NaN != NaN. */
     result_value = 1;
   } else {
     result_value = 0;
   }  /* if */
  } else {
    switch (op) {
      case eok_eq:  result_value = (cmp == 0); break;
      case eok_ne:  result_value = (cmp != 0); break;
      case eok_gt:  result_value = (cmp >  0); break;
      case eok_lt:  result_value = (cmp <  0); break;
      case eok_ge:  result_value = (cmp >= 0); break;
      case eok_le:  result_value = (cmp <= 0); break;
      default:      unexpected_condition_str("do_fcompare: bad operator");
    }  /* switch */
  }  /* if */
  set_constant_kind(result, (a_constant_repr_kind)ck_integer);
  set_integer_value(&result->variant.integer_value,
                    (a_host_large_integer)result_value);

#if DEBUG
  db_binary_operation(db_operator_names[op],
                      constant_1, constant_2, result, ec_no_error);
#endif /* DEBUG */
}  /* do_fcompare */

#if FIXED_POINT_ALLOWED

static void do_fxadd(a_constant        *constant_1,
                     a_constant        *constant_2,
                     a_constant        *result,
		     a_boolean	       *did_not_fold,
                     an_error_code     *err_code,
                     an_error_severity *err_severity)
/*
Do the addition operation on all types of fixed-point values, and
combinations of fixed-point and integer values.
*/
{
  a_boolean err;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  set_constant_kind(result, (a_constant_repr_kind)ck_fixed_point);
  fxp_add(constant_1, constant_2, result, did_not_fold, &err);
  if (err) {
    *err_code = ec_bad_fixed_operation_result;
    *err_severity = ES_FIXED_POINT_OVERFLOW;
  }  /* if */

#if DEBUG
  db_binary_operation("fx+", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_fxadd */


static void do_fxsubtract(a_constant        *constant_1,
                          a_constant        *constant_2,
                          a_constant        *result,
		          a_boolean	       *did_not_fold,
                          an_error_code     *err_code,
                          an_error_severity *err_severity)
/*
Do the subtraction operation on all types of fixed-point values, and
combinations of fixed-point and integer values.
*/
{
  a_boolean err;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  set_constant_kind(result, (a_constant_repr_kind)ck_fixed_point);
  fxp_subtract(constant_1, constant_2, result, did_not_fold, &err);
  if (err) {
    *err_code = ec_bad_fixed_operation_result;
    *err_severity = ES_FIXED_POINT_OVERFLOW;
  }  /* if */

#if DEBUG
  db_binary_operation("fx-", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_fxsubtract */


static void do_fxmultiply(a_constant        *constant_1,
                          a_constant        *constant_2,
                          a_constant        *result,
		          a_boolean	    *did_not_fold,
                          an_error_code     *err_code,
                          an_error_severity *err_severity)
/*
Do the multiplication operation on all types of fixed-point values, and
combinations of fixed-point and integer values.
*/
{
  a_boolean err;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  set_constant_kind(result, (a_constant_repr_kind)ck_fixed_point);
  fxp_multiply(constant_1, constant_2, result, did_not_fold, &err);
  if (err) {
    *err_code = ec_bad_fixed_operation_result;
    *err_severity = ES_FIXED_POINT_OVERFLOW;
  }  /* if */

#if DEBUG
  db_binary_operation("fx*", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_fxmultiply */


static void do_fxdivide(a_constant        *constant_1,
                        a_constant        *constant_2,
                        a_constant        *result,
                        a_boolean         *did_not_fold,
                        an_error_code     *err_code,
                        an_error_severity *err_severity)
/*
Do the division operation on all types of fixed-point values, and
combinations of fixed-point and integer values.
*/
{
  a_boolean err;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  /* Check for division by zero to give a specific error message. */
  if (fxp_value_is_zero(&constant_2->variant.fixed_point_value)) {
    *err_code = ec_divide_by_zero;
    *err_severity = es_error;
  } else {
    set_constant_kind(result, (a_constant_repr_kind)ck_fixed_point);
    fxp_divide(constant_1, constant_2, result, did_not_fold, &err);
    if (err) {
      *err_code = ec_bad_fixed_operation_result;
      *err_severity = ES_FIXED_POINT_OVERFLOW;
    }  /* if */
  }  /* if */

#if DEBUG
  db_binary_operation("fx/", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_fxdivide */


static void do_fxcompare(a_constant            *constant_1,
                         an_expr_operator_kind op,
                         a_constant            *constant_2,
                         a_constant            *result)
/*
Compare fixed-point constants constant_1 and constant_2 according to the
relational operator "op", and return a 0 or 1 integer in "result".
*/
{
  int cmp;
  int result_value = 0;

  /* Develop a strcmp-like relation value in cmp:
       constant_1 > constant_2   1
       constant_1 = constant_2   0
       constant_1 < constant_2  -1
  */
  check_assertion(constant_1->kind == constant_2->kind &&
                  constant_1->kind == (a_constant_repr_kind)ck_fixed_point);
  cmp = fxp_compare(constant_1, constant_2);
  /* Now determine the result value for this particular operator. */
  switch (op) {
    case eok_eq:  result_value = (cmp == 0); break;
    case eok_ne:  result_value = (cmp != 0); break;
    case eok_gt:  result_value = (cmp >  0); break;
    case eok_lt:  result_value = (cmp <  0); break;
    case eok_ge:  result_value = (cmp >= 0); break;
    case eok_le:  result_value = (cmp <= 0); break;
    default:      unexpected_condition_str("do_fxcompare: bad operator");
  }  /* switch */
  set_constant_kind(result, (a_constant_repr_kind)ck_integer);
  set_integer_value(&result->variant.integer_value,
                    (a_host_large_integer)result_value);

#if DEBUG
  db_binary_operation(db_operator_names[op],
                      constant_1, constant_2, result, ec_no_error);
#endif /* DEBUG */
}  /* do_fxcompare */

#endif /* FIXED_POINT_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED

static void do_fgnu_min_max(a_constant            *constant_1,
                            an_expr_operator_kind op,
                            a_constant            *constant_2,
                            a_constant            *result)
/*
Compare floating constant_1 and constant_2 and return the minimum or maximum
in result (depending on which operator is indicated by op).  This folds the
GNU C++ minimum and maximum operators ("<?" and ">?").
*/
{
  a_boolean    unordered;
  a_float_kind float_kind =
                           skip_typerefs(constant_1->type)->variant.float_kind;
  int          order = fp_compare(float_kind,
                                  &constant_1->variant.float_value,
                                  &constant_2->variant.float_value,
                                  &unordered);

  if (op == (an_expr_operator_kind)eok_gnu_min) {
    if (!unordered && order < 0) {
      /* The first constant is less than the second. */
      copy_constant(constant_1, result);
    } else {
      copy_constant(constant_2, result);
    }  /* if */
  } else {
    /* Evaluate the C++ maximum operator. */
    if (!unordered && order > 0) {
      copy_constant(constant_1, result);
    } else {
      copy_constant(constant_2, result);
    }  /* if */
  }  /* if */

#if DEBUG
  db_binary_operation(db_operator_names[op],
                      constant_1, constant_2, result, ec_no_error);
#endif /* DEBUG */
}  /* do_fgnu_min_max */

#endif /* GNU_EXTENSIONS_ALLOWED */

#if C99_IL_EXTENSIONS_SUPPORTED

static void do_xadd(a_constant        *constant_1,
		    a_constant        *constant_2,
		    a_constant        *result,
		    an_error_code     *err_code,
		    an_error_severity *err_severity,
                    a_boolean         *depends_on_fp_mode)
/*
Do the addition operation on all types of complex.
*/
{
  a_boolean    err;
  a_type_ptr   constant_type = skip_typerefs(constant_1->type);
  a_float_kind float_kind = constant_type->variant.float_kind;
  an_internal_complex_value
               cx1, cx2;

  get_complex_val(constant_1, &cx1);
  get_complex_val(constant_2, &cx2);
  *err_code = ec_no_error;
  *err_severity = es_warning;
  set_constant_kind(result, (a_constant_repr_kind)ck_complex);
  cx_add(float_kind, &cx1, &cx2,
         result->variant.complex_value, &err, depends_on_fp_mode);
  if (err) {
    *err_code = ec_bad_complex_operation_result;
    *err_severity = es_error;
  }  /* if */

#if DEBUG
  db_binary_operation("x+", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_xadd */


static void do_xsubtract(a_constant        *constant_1,
                         a_constant        *constant_2,
                         a_constant        *result,
                         an_error_code     *err_code,
                         an_error_severity *err_severity,
                         a_boolean         *depends_on_fp_mode)
/*
Do the subtraction operation on all types of complex.
*/
{
  a_boolean    err;
  a_type_ptr   constant_type = skip_typerefs(constant_1->type);
  a_float_kind float_kind = constant_type->variant.float_kind;
  an_internal_complex_value
               cx1, cx2;

  get_complex_val(constant_1, &cx1);
  get_complex_val(constant_2, &cx2);
  *err_code = ec_no_error;
  *err_severity = es_warning;
  set_constant_kind(result, (a_constant_repr_kind)ck_complex);
  cx_subtract(float_kind, &cx1, &cx2,
              result->variant.complex_value, &err, depends_on_fp_mode);
  if (err) {
    *err_code = ec_bad_complex_operation_result;
    *err_severity = es_error;
  }  /* if */

#if DEBUG
  db_binary_operation("x-", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_xsubtract */


static void do_xmultiply(a_constant        *constant_1,
                         a_constant        *constant_2,
                         a_constant        *result,
                         an_error_code     *err_code,
                         an_error_severity *err_severity,
                         a_boolean         *depends_on_fp_mode)
/*
Do the multiplication operation on all types of complex.
*/
{
  a_boolean     err;
  a_type_ptr    constant_type = skip_typerefs(constant_1->type);
  a_float_kind  float_kind = constant_type->variant.float_kind;
  an_internal_complex_value
               cx1, cx2;

  get_complex_val(constant_1, &cx1);
  get_complex_val(constant_2, &cx2);
  *err_code = ec_no_error;
  *err_severity = es_warning;
  set_constant_kind(result, (a_constant_repr_kind)ck_complex);
  cx_multiply(float_kind, &cx1, &cx2,
              result->variant.complex_value, &err, depends_on_fp_mode);
  if (err) {
    *err_code = ec_bad_complex_operation_result;
    *err_severity = es_error;
  }  /* if */
#if DEBUG
  db_binary_operation("x*", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_xmultiply */


static void do_xdivide(a_constant        *constant_1,
                       a_constant        *constant_2,
                       a_constant        *result,
                       an_error_code     *err_code,
                       an_error_severity *err_severity,
                       a_boolean         *depends_on_fp_mode)
/*
Do the division operation on all types of complex.
*/
{
  a_boolean                err, accum_err = FALSE, depends_on_mode;
  a_type_ptr               constant_type = skip_typerefs(constant_1->type);
  a_float_kind             float_kind = constant_type->variant.float_kind;
  an_internal_complex_value
               cx1, cx2;
  an_internal_float_value  quad_norm, temp_value;

  get_complex_val(constant_1, &cx1);
  get_complex_val(constant_2, &cx2);
  *err_code = ec_no_error;
  *err_severity = es_warning;

  set_constant_kind(result, (a_constant_repr_kind)ck_complex);
#if 0
  /* This is an oversimplified algorithm that can exhibit dynamic range
     problems (e.g., catastrophic cancellation).  Also, this is nearly
     identical to cx_divide, except is creates an ec_divide_by_zero error
     code in some cases. */
#endif /* 0 */
  /* Compute the real value quad_norm = real_2*real_2 + imag_2*imag_2. */
  fp_multiply(float_kind, &cx2.real, &cx2.real,
              &quad_norm, &err, &depends_on_mode);
  accum_err |= err;
  *depends_on_fp_mode = depends_on_mode;
  fp_multiply(float_kind, &cx2.imag, &cx2.imag,
              &temp_value, &err, &depends_on_mode);
  accum_err |= err;
  *depends_on_fp_mode |= depends_on_mode;
  fp_add(float_kind, &quad_norm, &temp_value, &quad_norm,
         &err, &depends_on_mode);
  accum_err |= err;
  *depends_on_fp_mode |= depends_on_mode;
  if (!IEEE_handling_on_float_operation_exceptions &&
      fp_is_zero_constant(float_kind, &quad_norm)) {
    *err_code = ec_divide_by_zero;
    *err_severity = es_error;
  } else {
    /* Compute real part of the result. */
    fp_multiply(float_kind, &cx1.real, &cx2.real,
                &result->variant.complex_value->real, &err, &depends_on_mode);
    accum_err |= err;
    *depends_on_fp_mode |= depends_on_mode;
    fp_multiply(float_kind, &cx1.imag, &cx2.imag,
                &temp_value, &err, &depends_on_mode);
    accum_err |= err;
    *depends_on_fp_mode |= depends_on_mode;
    fp_add(float_kind, &result->variant.complex_value->real, &temp_value,
           &result->variant.complex_value->real,
           &err, &depends_on_mode);
    accum_err |= err;
    *depends_on_fp_mode |= depends_on_mode;
    fp_divide(float_kind, &result->variant.complex_value->real, &quad_norm,
              &result->variant.complex_value->real,
              &err, &depends_on_mode);
    accum_err |= err;
    *depends_on_fp_mode |= depends_on_mode;
    /* Compute imaginary part of the result. */
    fp_multiply(float_kind, &cx1.real, &cx2.imag,
                &result->variant.complex_value->imag, &err, &depends_on_mode);
    accum_err |= err;
    *depends_on_fp_mode |= depends_on_mode;
    fp_multiply(float_kind, &cx1.imag, &cx2.real,
                &temp_value, &err, &depends_on_mode);
    accum_err |= err;
    *depends_on_fp_mode |= depends_on_mode;
    fp_subtract(float_kind, &temp_value, &result->variant.complex_value->imag,
                &result->variant.complex_value->imag,
                &err, &depends_on_mode);
    accum_err |= err;
    *depends_on_fp_mode |= depends_on_mode;
    fp_divide(float_kind, &result->variant.complex_value->imag, &quad_norm,
              &result->variant.complex_value->imag,
              &err, &depends_on_mode);
    accum_err |= err;
    *depends_on_fp_mode |= depends_on_mode;
    if (accum_err) {
      *err_code = ec_bad_complex_operation_result;
      *err_severity = es_error;
    }  /* if */
  }  /* if */

#if DEBUG
  db_binary_operation("x/", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_xdivide */


static void do_xcompare(a_constant            *constant_1,
                        an_expr_operator_kind op,
                        a_constant            *constant_2,
                        a_constant            *result)
/*
Compare complex constants constant_1 and constant_2 according to the
relational operator "op", and return a 0 or 1 integer in "result".
Unlike real values, no ordering can be tested, only equality (or lack
thereof).
*/
{
  int          result_value;
  a_float_kind float_kind =
                          skip_typerefs(constant_1->type)->variant.float_kind;
  an_internal_complex_value
               cx1, cx2;

  get_complex_val(constant_1, &cx1);
  get_complex_val(constant_2, &cx2);

  result_value = cx_equal(float_kind, &cx1, &cx2);
  if (op == (an_expr_operator_kind)eok_ne) {
    result_value = !result_value;
  } else {
    check_assertion(op == (an_expr_operator_kind)eok_eq);
  }  /* if */
  set_constant_kind(result, (a_constant_repr_kind)ck_integer);
  set_integer_value(&result->variant.integer_value,
                    (a_host_large_integer)result_value);

#if DEBUG
  db_binary_operation(db_operator_names[op],
                      constant_1, constant_2, result, ec_no_error);
#endif /* DEBUG */
}  /* do_xcompare */


static void do_jmultiply(a_constant        *constant_1,
                         a_constant        *constant_2,
                         a_constant        *result,
                         an_error_code     *err_code,
                         an_error_severity *err_severity,
                         a_boolean         *depends_on_fp_mode)
/*
Do the multiplication operation on two imaginary numbers (any precision).
*/
{
  a_boolean    err, accum_err = FALSE, depends_on_mode;
  a_type_ptr   constant_type = skip_typerefs(constant_1->type);
  a_float_kind float_kind = constant_type->variant.float_kind;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  set_constant_kind(result, (a_constant_repr_kind)ck_float);
  fp_multiply(float_kind,
              &constant_1->variant.float_value,
              &constant_2->variant.float_value,
              &result->variant.float_value, &err,
              &depends_on_mode);
  accum_err |= err;
  *depends_on_fp_mode = depends_on_mode;
  fp_negate(float_kind, &result->variant.float_value,
            &result->variant.float_value, &err, &depends_on_mode);
  accum_err |= err;
  *depends_on_fp_mode |= depends_on_mode;
  if (accum_err) {
    *err_code = ec_bad_complex_operation_result;
    *err_severity = es_error;
  }  /* if */

#if DEBUG
  db_binary_operation("j*", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_jmultiply */


static void do_jdivide(a_constant        *constant_1,
                       a_constant        *constant_2,
                       a_constant        *result,
                       an_error_code     *err_code,
                       an_error_severity *err_severity,
                       a_boolean         *depends_on_fp_mode)
/*
Do the division of a real number by an imaginary number (any precision).
*/
{
  a_boolean    err, accum_err = FALSE, depends_on_mode;
  a_type_ptr   constant_type = skip_typerefs(constant_1->type);
  a_float_kind float_kind = constant_type->variant.float_kind;

  *err_code = ec_no_error;
  *err_severity = es_warning;
  *depends_on_fp_mode = FALSE;

  /* Check for division by zero to give a specific error message. */
  if (!IEEE_handling_on_float_operation_exceptions &&
      fp_is_zero_constant(float_kind, &constant_2->variant.float_value)) {
    *err_code = ec_divide_by_zero;
    *err_severity = es_error;
  } else {
    set_constant_kind(result, (a_constant_repr_kind)ck_imaginary);
    fp_divide(float_kind,
              &constant_1->variant.float_value,
              &constant_2->variant.float_value,
              &result->variant.float_value, &err,
              &depends_on_mode);
    accum_err |= err;
    *depends_on_fp_mode = depends_on_mode;
    fp_negate(float_kind, &result->variant.float_value,
              &result->variant.float_value, &err, &depends_on_mode);
    accum_err |= err;
    *depends_on_fp_mode |= depends_on_mode;
    if (accum_err) {
      *err_code = ec_bad_complex_operation_result;
      *err_severity = es_error;
    }  /* if */
  }  /* if */

#if DEBUG
  db_binary_operation("j/", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_jdivide */


static void do_real_imag_add_subtract(
                                     a_constant            *constant_1,
                                     an_expr_operator_kind op,
                                     a_constant            *constant_2,
                                     a_constant            *result,
                                     an_error_code         *err_code,
                                     an_error_severity     *err_severity,
                                     a_boolean             *depends_on_fp_mode)
/*
Do mixed real/imaginary addition and subtraction, i.e.,

  eok_fjadd      real      + imaginary
  eok_jfadd      imaginary + real
  eok_fjsubtract real      - imaginary
  eok_jfsubtract imaginary - real

These differ from simply converting to complex and adding, by the
preservation of negative zeroes.
*/
{
  a_boolean    err = FALSE;
  a_type_ptr   constant_type = skip_typerefs(constant_1->type);
  a_float_kind float_kind = constant_type->variant.float_kind;

  *err_code = ec_no_error;
  *err_severity = es_warning;
  *depends_on_fp_mode = FALSE;

  set_constant_kind(result, (a_constant_repr_kind)ck_complex);
  switch (op) {
    case eok_fjadd:
      /* Real + imaginary. */
      result->variant.complex_value->real = constant_1->variant.float_value;
      result->variant.complex_value->imag = constant_2->variant.float_value;
      break;
    case eok_jfadd:
      /* Imaginary + real. */
      result->variant.complex_value->imag = constant_1->variant.float_value;
      result->variant.complex_value->real = constant_2->variant.float_value;
      break;
    case eok_fjsubtract:
      /* Real - imaginary. */
      result->variant.complex_value->real = constant_1->variant.float_value;
      fp_negate(float_kind,
                &constant_2->variant.float_value,
                &result->variant.complex_value->imag,
                &err, depends_on_fp_mode);
      break;
    case eok_jfsubtract:
      /* Imaginary - real. */
      result->variant.complex_value->imag = constant_1->variant.float_value;
      fp_negate(float_kind,
                &constant_2->variant.float_value,
                &result->variant.complex_value->real,
                &err, depends_on_fp_mode);
      break;
    default:
      unexpected_condition_str("do_real_imag_add_subtract: bad operator");
  }  /* switch */
  if (err) {
    *err_code = ec_bad_complex_operation_result;
    *err_severity = es_error;
  }  /* if */
#if DEBUG
  db_binary_operation(db_operator_names[op],
                      constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_real_imag_add_subtract */

#endif /* C99_IL_EXTENSIONS_SUPPORTED */

static a_boolean valid_address_constant(a_constant *constant)
/*
Return TRUE if the given address constant is valid.  Specifically, check that
the offset in it falls within the base object.  This is used for subscript
checking.
*/
{
  a_boolean      valid;
  a_targ_size_t  object_size = 0;
  a_type_ptr     tp;
  a_constant_ptr cp;

  if (constant->kind == (a_constant_repr_kind)ck_integer) {
    /* Integer cast to a pointer.  Don't know the underlying
       object.  Assume the pointer is okay. */
    valid = TRUE;
  } else {
#if CHECKING
    if (constant->kind != (a_constant_repr_kind)ck_address) {
      internal_error("valid_address_constant: not ck_address or ck_integer");
    }  /* if */
#endif /* CHECKING */
    /* Determine the size of the object pointed to. */
    switch(constant->variant.address.kind) {
      case abk_variable:
        tp = skip_typerefs(constant->variant.address.variant.variable->type);
        /* Ignore incomplete arrays and flexible arrays. */
        if (!is_incomplete_type(tp) &&
            !(is_immediate_class_type(tp) &&
              tp->variant.class_struct_union.contains_flexible_array_member)) {
          object_size = tp->size;
        }  /* if */
        break;
      case abk_routine:
      case abk_label:
      case abk_param_ref:
        /* No size to check. */
        break;
      case abk_constant:
        cp = constant->variant.address.variant.constant;
        if (cp->kind == (a_constant_repr_kind)ck_string) {
          object_size = cp->variant.string.length;
        } else {
          object_size = skip_typerefs(cp->type)->size;
        }  /* if */
        break;
      case abk_temporary:
        cp = constant->variant.address.variant.constant;
        object_size = skip_typerefs(cp->type)->size;
        break;
      case abk_uuidof:
        tp = type_pointed_to(constant->type);
        object_size = tp->size;
        break;
      case abk_typeid:
#if MICROSOFT_EXTENSIONS_ALLOWED
      case abk_cli_typeid:
      case abk_cli_array:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* The object is std::type_info or a class derived from it, or a
           handle to a C++/CLI System::String or System::Array.  Therefore, we
           don't really know the actual size. */
        break;
      default:
        unexpected_condition_str(
                          "valid_address_constant: bad address constant kind");
    }  /* switch */
    /* See if the offset is valid given the size. */
    if (constant->variant.address.offset < 0) {
      /* A negative offset is never valid. */
      valid = FALSE;
    } else if (object_size != 0) {
      /* The offset right after the object is allowed.
         ANSI C allows that for arrays to simplify some coding.  That
         subscript value is flagged on a check of the subscript in expression
         form, which would be the form of any reference of the entity as
         an lvalue (see valid_node_if_subscript). */
      a_targ_size_t offset = (a_targ_size_t)constant->variant.address.offset;
      valid = (offset <= object_size);
    } else {
      /* Don't know what the size is, so assume the offset is valid. */
      valid = TRUE;
    }  /* if */
  }  /* if */

  return valid;
}  /* valid_address_constant */


static a_targ_size_t gcc_stride_size(a_type_ptr type)
/*
Return the stride size to be used for a pointer operation on pointer-to-type
in gcc mode.  gcc allows pointer addition and subtraction on pointer-to-void
and pointer-to-function.
*/
{
  a_targ_size_t size;

  type = skip_typerefs(type);
  if (is_void_type(type) ||
      is_function_type(type)) {
    size = 1;
  } else {
    size = type->size;
  }  /* if */
  return size;
}  /* gcc_stride_size */


static void accum_array_offset(a_constant_ptr  total_offset,
                               a_boolean       offset_is_signed,
                               a_boolean       subtract,
                               a_constant_ptr  count,
                               a_targ_size_t   elem_size,
                               a_boolean       no_ovflo_on_unsigned_add,
                               a_boolean       *ovflo,
                               a_boolean       *did_not_fold)
/*
Perform the multiply-add or multiply-subtract implied by array subscripting
or pointer arithmetic.  *total_offset is a constant to/from which the implied
offset must be added/subtracted (subtract determines which operation it is).
offset_is_signed determines whether *total_offset should be treated as a
signed value (this can be different from the signedness implied by the type).
count describes the number of array elements "added" or "subtracted", and
elem_size is the size of each of those elements.  *ovflo is set to TRUE if an
overflow occurs.  If an overflow resulting from an unsigned addition should
be ignored, no_ovflo_on_unsigned_add should be set to TRUE.  If the
operation could not be folded (because count is not a known integer
constant), return *did_not_fold TRUE.
*/
{
  *ovflo = FALSE;
  *did_not_fold = FALSE;
  if (count->kind != (a_constant_repr_kind)ck_integer) {
    *did_not_fold = TRUE;
  } else {
    an_integer_value  array_offset;
    a_boolean         count_is_signed = int_constant_is_signed(count);

    set_unsigned_integer_value(&array_offset, elem_size);
    multiply_integer_values(&array_offset, &count->variant.integer_value,
                            int_constant_is_signed(count), ovflo);
    if (!*ovflo) {
      /* Add/subtract the increment to/from the original offset. */
      if (subtract) {
        subtract_mixed_signed_integer_values(
            &total_offset->variant.integer_value, offset_is_signed,
            &array_offset, count_is_signed, ovflo);
      } else {
        add_mixed_signed_integer_values(
            &total_offset->variant.integer_value, offset_is_signed,
            &array_offset, count_is_signed, ovflo);
      }  /* if */
      /* If this was an unsigned integer operation, overflow is ignored. */
      if (no_ovflo_on_unsigned_add && !offset_is_signed) *ovflo = FALSE;
    }  /* if */
  }  /* if */
}  /* accum_array_offset */


static void do_padd(a_constant            *constant_1,
                    an_expr_operator_kind op,
                    a_constant            *constant_2,
                    a_constant            *result,
                    a_boolean             *did_not_fold,
                    an_error_code         *err_code,
                    an_error_severity     *err_severity)
/*
Do addition or subtraction on one pointer (constant_1) and one integer
(constant_2).  op indicates whether the source form was "+"
(eok_padd), "[]" (eok_subscript), or "-" (eok_psubtract).
Note that the integer can be of any type, specifically unsigned.
Also used to add or subtract a constant from an address constant
that has been cast to an integral type, as in "int i = (int)&j + 1;";
in that case, the operator is eok_add or eok_subtract.
*did_not_fold is returned TRUE if the operation cannot be folded.
*err_code and *err_severity are set to indicate any error/warning
detected, or *err_code == ec_no_error if everything went fine.
*/
{
  a_targ_size_t    size;
  a_constant_ptr   offset = local_constant();
  a_boolean        err = FALSE, offset_is_signed = FALSE;
  a_boolean        integer_case = FALSE;

  *did_not_fold = FALSE;
  *err_code = ec_no_error;
  *err_severity = es_warning;

  if (op == (an_expr_operator_kind)eok_add ||
      op == (an_expr_operator_kind)eok_subtract) {
    /* For the (int)address +- constant case, the size (scaling) is 1. */
    integer_case = TRUE;
    size = 1;
  } else {
    /* Get the size of the thing pointed to. */
    a_type_ptr  object_type =
                           f_skip_typerefs(type_pointed_to(constant_1->type));
    if (is_vla_type(object_type)) {
      /* Can't do pointer arithmetic on a VLA types, since the size is not
         known at compile time. */
      *did_not_fold = TRUE;
      goto have_result;
    } else if (gcc_mode) {
      size = gcc_stride_size(object_type);
    } else {
      size = object_type->size;
    }  /* if */
    /* gnu mode allows empty classes with size zero, so pointers to
       such classes produce size zero here.  Likewise for some cases
       of arrays with zero bounds in gnu mode. */
    check_assertion_str(size != 0 || gnu_mode, "do_padd: size is zero");
  }  /* if */
  /* Get the offset from the first constant. */
  get_pointer_offset(constant_1, offset);
  /* When dealing with an address cast to an integral type, treat the
     offset as having the signedness of the type cast to. */
  offset_is_signed = integer_case ? int_constant_is_signed(constant_1) :
                                    int_constant_is_signed(offset);
  /* Perform the necessary multiply-add or multiply-subtract. */
  accum_array_offset(offset, offset_is_signed,
                     (op == (an_expr_operator_kind)eok_psubtract ||
                      op == (an_expr_operator_kind)eok_subtract),
                      constant_2, size, (integer_case && !offset_is_signed),
                      &err, did_not_fold);
  if (!err && !*did_not_fold) {
    /* Build the result pointer constant. */
    copy_constant(constant_1, result);
    set_pointer_offset(result, offset, &err);
    if (integer_case && !offset_is_signed) {
      /* If this was an unsigned integer operation, overflow is ignored. */
      err = FALSE;
    } else if (constant_is(result, ck_address)) {
      /* Record the change in the associated subobject path. */
      a_targ_ptrdiff_t      offset_change;
      a_subobject_path_ptr  spp;
      spp = get_trailing_subobject_path_entry(result, /*is_offset=*/TRUE,
                                              /*is_base_class=*/FALSE);
      offset_change = value_of_integer_constant(constant_2, &err);
      if (op == (an_expr_operator_kind)eok_psubtract ||
          op == (an_expr_operator_kind)eok_subtract) {
        spp->variant.ptr_offset -= offset_change;
      } else {
        spp->variant.ptr_offset += offset_change;
      }  /* if */
    }  /* if */
  }  /* if */
have_result:
  if (err) {
    /* Some folding error. */
    *err_code = ec_integer_overflow;
    *err_severity = es_error;
  } else if (*did_not_fold) {
    set_error_constant(result);
  } else {
    /* Check that the offset lies within the base object. */
    if (!integer_case && !valid_address_constant(result)) {
      if (cpp11_mode && !gnu_mode && !microsoft_mode) {
        /* An out-of-bound address should not be folded to a constant in
           C++11 (although g++ and MSVC do). */
        *did_not_fold = TRUE;
      }  /* if */
      /* Use a different error message for cases where the original pointer
         addition was coded in [] form. */
      if (op == (an_expr_operator_kind)eok_subscript) {
        *err_code = ec_subscript_out_of_range;
      } else {
        *err_code = ec_pointer_outside_base_object;
      }  /* if */
      *err_severity = es_warning;
    }  /* if */
  }  /* if */

#if DEBUG
  db_binary_operation(db_operator_names[op],
                      constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
  release_local_constant(&offset);
}  /* do_padd */


void do_pdiff(a_constant        *constant_1,
              a_constant        *constant_2,
              a_constant        *result,
              a_boolean         *did_not_fold,
              an_error_code     *err_code,
              an_error_severity *err_severity)
/*
Do the pointer subtraction "pointer - pointer": pointer difference.
constant_1 and constant_2 are the two pointer constants.  The result
is returned in result.  If the operation cannot be folded to
a constant (because the pointers do not point to the same object),
*did_not_fold is returned TRUE.  *err_code and *err_severity are set
to indicate any error/warning detected, or *err_code == ec_no_error
if everything went fine.  Also handles address constants cast to an
integral type, as in "(int)&x - (int)&x".
*/
{
  a_constant_ptr   offset_2 = local_constant(), offset_1 = local_constant();
  an_integer_value difference, size_intval;
  a_type_ptr       object_type;
  a_boolean        err, offset_1_is_signed, offset_2_is_signed, cannot_fold;

  *err_code = ec_no_error;
  *err_severity = es_warning;
  /* The two pointers must be in the same base object, or the operation
     cannot be folded. */
  if (!same_address_base(constant_1, constant_2, &cannot_fold)) {
    if (cannot_fold) {
      /* Nothing more to do. */
#if GNU_EXTENSIONS_ALLOWED
    } else if (gnu_mode && constant_is_address_of_label(constant_1) &&
               constant_is_address_of_label(constant_2)) {
      /* An exception is the difference of two label addresses in GNU mode. */
      clear_constant(result, (a_constant_repr_kind)ck_label_difference);
      result->variant.label_difference.from_address =
                                         alloc_shareable_constant(constant_2);
      result->variant.label_difference.to_address =
                                         alloc_shareable_constant(constant_1);
      result->type = integer_type(targ_ptrdiff_t_int_kind);
    } else if ((gnu_mode || microsoft_mode) &&
               constant_bool_value_known_at_compile_time(constant_2) &&
               is_false_constant(constant_2) &&
               is_pointer_type(constant_1->type) &&
               is_character_type(type_pointed_to(constant_1->type))) {
      /* Also a "char *" address minus a "char *" zero.  Since the stride is 1,
         the result is the first constant cast to the integral result type. */
      copy_constant(constant_1, result);
      implicit_cast(result, integer_type(targ_ptrdiff_t_int_kind));
#endif /* GNU_EXTENSIONS_ALLOWED */
    } else {
      cannot_fold = TRUE;
    }  /* if */
  } else {
    /* The pointers are in the same base object, so the difference of
       their offsets can be taken. */
    get_pointer_offset(constant_1, offset_1);
    offset_1_is_signed = int_constant_is_signed(offset_1);
    get_pointer_offset(constant_2, offset_2);
    offset_2_is_signed = int_constant_is_signed(offset_2);
    difference = offset_1->variant.integer_value;
    subtract_mixed_signed_integer_values(&difference,
                                         offset_1_is_signed,
                                         &offset_2->variant.integer_value,
                                         offset_2_is_signed, &err);
    if (!err) {
      /* Divide the difference by the size of the objects pointed to.
         The caller has already checked that the type pointed to is
         not incomplete, so the size is not zero (except possibly
         in gcc mode).  If the address constants have been cast to
         integer, there is no scaling. */
      if (!is_integral_type(constant_1->type)) {
        a_targ_size_t  object_size;
        object_type = type_pointed_to(constant_1->type);
        object_type = skip_typerefs(object_type);
        if (gcc_mode) {
          object_size = gcc_stride_size(object_type);
        } else {
          object_size = object_type->size;
        }  /* if */
        /* Division by zero can come up in GNU mode with pointers to empty
           class types or pointers to zero-length arrays. */
        check_assertion_str(object_size != 0 || gnu_mode,
                            "do_pdiff: size of object pointed to is zero");
        set_unsigned_integer_value(&size_intval, object_size);
        divide_integer_values(&difference, &size_intval,
                              int_constant_is_signed(result), &err);
      }  /* if */
    }  /* if */
    if (!err) {
      trunc_and_set_integer(&difference, result, /*check_overflow=*/TRUE,
                            /*saturate_on_overflow=*/FALSE,
                            err_code, err_severity);
    } else {
      *err_code = ec_integer_overflow;
      *err_severity = es_error;
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level  >= 5) {
    if (cannot_fold) {
      fprintf(f_debug, "do_pdiff: did not fold\n");
    } else {
      db_binary_operation("pd", constant_1, constant_2, result, *err_code);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  release_local_constant(&offset_2);
  release_local_constant(&offset_1);
  *did_not_fold = cannot_fold;
}  /* do_pdiff */


a_boolean compare_address_constants(a_constant_ptr  con1,
                                    a_constant_ptr  con2,
                                    int             *p_cmp)
/*
If the given address constants are not comparable return FALSE.  Otherwise,
return TRUE and set *p_cmp to zero if the addresses are equal, to -1 if
the first constant is less than the second, and to 1 otherwise.
*/
{
  a_boolean  result = TRUE, cannot_fold;

  if (!same_address_base(con1, con2, &cannot_fold)) {
    result = FALSE;
  } else if (constant_is(con1, ck_integer)) {
    *p_cmp = cmp_integer_constants(con1, con2);
  } else {
    /* Compare the subobject paths: If base classes are involved, the
       subobject paths have to designate the same base class for the
       addresses to be comparable. */
    a_subobject_path_ptr  spp1, spp2;
    a_boolean             paths_differ = FALSE;
    check_assertion(constant_is(con1, ck_address));
    spp1 = con1->variant.address.subobject_path;
    spp2 = con2->variant.address.subobject_path;
    while (spp1 != NULL && spp2 != NULL) {
      if (spp1->is_base_class || spp2->is_base_class) {
        if (paths_differ || spp1->is_base_class != spp2->is_base_class ||
            !same_base_classes(spp1->variant.base_class,
                               spp2->variant.base_class)) {
          result = FALSE;
          goto done;
        }  /* if */
      } else if (spp1->is_offset || spp2->is_offset) {
        if (spp1->is_offset != spp2->is_offset ||
            spp1->variant.ptr_offset != spp2->variant.ptr_offset) {
          paths_differ = TRUE;
        }  /* if */
      } else {
        if (!same_entities(spp1->variant.field, spp2->variant.field)) {
          paths_differ = TRUE;
        }  /* if */
      }  /* if */
      spp1 = spp1->next;
      spp2 = spp2->next;
    }  /* while */
    if (con1->variant.address.offset == con2->variant.address.offset) {
      *p_cmp = 0;
    } else if (con1->variant.address.offset < con2->variant.address.offset) {
      *p_cmp = -1;
    } else {
      *p_cmp = 1;
    }  /* if */
  }  /* if */
done:
  return result;
}  /* compare_address_constants */


a_boolean compare_address_constants_equality(a_constant_ptr  con1,
                                             a_constant_ptr  con2,
                                             int             *p_cmp)
/*
If the given address constants are not comparable for equality return FALSE.
Otherwise, return TRUE and set *p_cmp to one if the addresses are equal and
to zero otherwise.
*/
{
  a_boolean  result = TRUE, unknown_base = FALSE;
  char       *base_1, *base_2;

  base_1 = base_object(con1, &unknown_base);
  base_2 = base_object(con2, &unknown_base);
  if (unknown_base) {
    if (reference_to_unknown_object_allowed &&
        ((base_1 != NULL && base_1 == base_2) ||
         same_param_ref_base(con1, con2))) {
      unknown_base = FALSE;
    }  /* if */
  }  /* if */
  if (unknown_base) {
    /* This can happen with weak variables or distinct unknown objects. */
    result = FALSE;
  } else if (constant_is(con1, ck_integer) || constant_is(con2, ck_integer)) {
    /* Integers cast to pointer types. */
    if (con1->kind == con2->kind) {
      *p_cmp = (int)(cmp_integer_constants(con1, con2) == 0);
    } else {
      if (constant_is(con1, ck_integer)) swap_at(&con1, &con2);
      if (!is_null_pointer_value(con2)) {
        result = FALSE;
      } else {
        /* We are comparing against the null pointer value.  This is false,
           unless we're comparing a weak symbol address. */
        if (con1->variant.address.kind == abk_variable) {
          if (variable_has_non_null_address(
                                    con1->variant.address.variant.variable)) {
            *p_cmp = FALSE;
          } else {
            result = FALSE;
          }  /* if */
        } else if (con1->variant.address.kind == abk_routine) {
          if (routine_has_non_null_address(
                                     con1->variant.address.variant.routine)) {
            *p_cmp = FALSE;
          } else {
            result = FALSE;
          }  /* if */
        } else {
          *p_cmp = FALSE;
        }  /* if */
      }  /* if */
    }  /* if */
  } else if (base_1 != base_2 ||
             con1->variant.address.offset != con2->variant.address.offset) {
    /* If the offsets are different, the addresses are definitely not equal. */
    *p_cmp = 0;
  } else {
    /* The offsets are equal.  That doesn't mean the addresses are "equal":
       they have to designate the same subobjects.  If they don't, the
       comparison is meaningless.  Compare the subobject paths. */
    a_subobject_path_ptr  spp1, spp2;
    a_boolean             paths_differ = FALSE;
    check_assertion(constant_is(con1, ck_address));
    spp1 = con1->variant.address.subobject_path;
    spp2 = con2->variant.address.subobject_path;
    while (spp1 != NULL && spp2 != NULL) {
      if (spp1->is_base_class || spp2->is_base_class) {
        if (paths_differ || spp1->is_base_class != spp2->is_base_class ||
            !same_base_classes(spp1->variant.base_class,
                               spp2->variant.base_class)) {
          result = FALSE;
          goto done;
        }  /* if */
      } else if (spp1->is_offset || spp2->is_offset) {
        if (spp1->is_offset != spp2->is_offset ||
            spp1->variant.ptr_offset != spp2->variant.ptr_offset) {
          paths_differ = TRUE;
        }  /* if */
      } else {
        if (!same_entities(spp1->variant.field, spp2->variant.field)) {
          paths_differ = TRUE;
        }  /* if */
      }  /* if */
      spp1 = spp1->next;
      spp2 = spp2->next;
    }  /* while */
    *p_cmp = 1;
  }  /* if */
done:
  return result;
}  /* compare_address_constants_equality */


static void do_pcompare(a_constant            *constant_1,
			an_expr_operator_kind op,
			a_constant            *constant_2,
			a_constant            *result,
                        a_boolean             *did_not_fold,
			an_error_code         *err_code,
			an_error_severity     *err_severity)
/*
Fold a relational operation on two pointer constants.  constant_1 and
constant_2 are compared according to the indicated operator, and
*result is set to an integer 0 or 1 for the result.  *err_code and
*err_severity are set if there are warnings or errors, or
*err_code == ec_no_error is there were no problems.  *did_not_fold is
set if the operation cannot be folded.
*/
{
  a_constant_ptr offset_1 = local_constant(), offset_2 = local_constant();
  int            result_value = 0, cmp;
  a_boolean      cannot_fold;

  *err_code = ec_no_error;
  *err_severity = es_warning;
  if (!same_address_base(constant_1, constant_2, &cannot_fold)) {
    /* The pointers are in different objects.  In C++11 and following,
       equality comparisons of constant addresses are required to work in
       the obvious fashion.  In earlier versions of the language, however,
       we should not fold even equality comparisons:
         int i, j;
         if (&i != &j) { ... }  <--- probably different
       However, that seems pointless, and could actually cause problems
       (maybe a smart compiler puts i and j at the same address because
       their lifetimes are disjoint). */
    if (cannot_fold) {
      /* The base object of at least one of the constants is not known for
         sure: Do not attempt to fold the result. */
    } else if (constexpr_enabled &&
               (op == (an_expr_operator_kind)eok_eq ||
                op == (an_expr_operator_kind)eok_ne)) {
      /* Constant pointers to different objects compare unequal. */
      result_value = (op == (an_expr_operator_kind)eok_ne);
      set_constant_kind(result, (a_constant_repr_kind)ck_integer);
      set_integer_value(&result->variant.integer_value,
                        (a_host_large_integer)result_value);
    } else {
      /* Pre-C++11 or relational comparison (which cannot be folded, even
         in C++11). */
      cannot_fold = TRUE;
      if ((op == (an_expr_operator_kind)eok_eq ||
           op == (an_expr_operator_kind)eok_ne) &&
          (constexpr_enabled || !strict_ansi_mode)) {
        /* However, "&var != NULL" or "&var == NULL" can often be folded. */
        a_variable_ptr var = NULL;
        if (is_null_pointer_value(constant_2) &&
            constant_1->kind == (a_constant_repr_kind)ck_address &&
            constant_1->variant.address.kind ==
                                          (an_address_base_kind)abk_variable) {
          var = constant_1->variant.address.variant.variable;
        } else if (is_null_pointer_value(constant_1) &&
                   constant_2->kind == (a_constant_repr_kind)ck_address &&
                   constant_2->variant.address.kind ==
                                          (an_address_base_kind)abk_variable) {
          var = constant_2->variant.address.variant.variable;
        }  /* if */
        if (var != NULL && variable_has_non_null_address(var)) {
          cannot_fold = FALSE;
          result_value = (op == (an_expr_operator_kind)eok_ne);
          set_constant_kind(result, (a_constant_repr_kind)ck_integer);
          set_integer_value(&result->variant.integer_value,
                            (a_host_large_integer)result_value);
        } else {
          /* Similarly, routine addresses are usually known to be non-null. */
          a_routine_ptr  rp = NULL;
          if (is_null_pointer_value(constant_2) &&
              constant_is(constant_1, ck_address) &&
              constant_1->variant.address.kind ==
                                          (an_address_base_kind)abk_routine) {
            rp = constant_1->variant.address.variant.routine;
          } else if (is_null_pointer_value(constant_1) &&
                     constant_is(constant_2, ck_address) &&
                     constant_2->variant.address.kind ==
                                          (an_address_base_kind)abk_routine) {
            rp = constant_2->variant.address.variant.routine;
          }  /* if */
          if (rp != NULL && routine_has_non_null_address(rp)) {
            cannot_fold = FALSE;
            result_value = (op == (an_expr_operator_kind)eok_ne);
            set_constant_kind(result, (a_constant_repr_kind)ck_integer);
            set_integer_value(&result->variant.integer_value,
                              (a_host_large_integer)result_value);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  } else {
    /* The pointers are in the same base object, so they can be compared. */
    get_pointer_offset(constant_1, offset_1);
    get_pointer_offset(constant_2, offset_2);
    /* Compare the offsets, then generate a result value. */
    cmp = cmp_integer_constants(offset_1, offset_2);
    switch (op) {
      case eok_eq:  result_value = (cmp == 0); break;
      case eok_ne:  result_value = (cmp != 0); break;
      case eok_gt:  result_value = (cmp >  0); break;
      case eok_lt:  result_value = (cmp <  0); break;
      case eok_ge:  result_value = (cmp >= 0); break;
      case eok_le:  result_value = (cmp <= 0); break;
      default:      unexpected_condition_str("do_pcompare: bad operator");
    }  /* switch */
    set_constant_kind(result, (a_constant_repr_kind)ck_integer);
    set_integer_value(&result->variant.integer_value,
                      (a_host_large_integer)result_value);
  }  /* if */
#if DEBUG
  if (debug_level  >= 5) {
    if (cannot_fold) {
      fprintf(f_debug, "do_pcompare: did not fold\n");
    } else {
      db_binary_operation(db_operator_names[op],
                          constant_1, constant_2, result, *err_code);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  release_local_constant(&offset_1);
  release_local_constant(&offset_2);
  *did_not_fold = cannot_fold;
}  /* do_pcompare */


static void do_pmcompare(a_constant            *constant_1,
                         an_expr_operator_kind op,
                         a_constant            *constant_2,
                         a_constant            *result)
/*
Fold a relational operation on two pointer-to-member constants.
constant_1 and constant_2 are compared according to the indicated operator,
and *result is set to an integer 0 or 1 for the result.
*/
{
  int result_value = FALSE;

  if (constant_1->variant.ptr_to_member.casting_base_class ==
                        constant_2->variant.ptr_to_member.casting_base_class &&
      constant_1->variant.ptr_to_member.is_function_ptr ==
                           constant_2->variant.ptr_to_member.is_function_ptr) {
    if (constant_1->variant.ptr_to_member.is_function_ptr) {
      result_value = (constant_1->variant.ptr_to_member.variant.routine ==
                      constant_2->variant.ptr_to_member.variant.routine);
    } else {
      /* For fields, test offsets instead of just field because of
         union fields. */
      a_field_ptr field1 = constant_1->variant.ptr_to_member.variant.field;
      a_field_ptr field2 = constant_2->variant.ptr_to_member.variant.field;
      result_value = (field1 == field2 ||
                      (field1 != NULL && field2 != NULL &&
                       field1->offset == field2->offset &&
                       field1->offset_bit_remainder ==
                                                field2->offset_bit_remainder));
    }  /* if */
  }  /* if */
  /* result_value is now set for the "==" case.  Complement it for the "!="
     case. */
  if (op == (an_expr_operator_kind)eok_ne) result_value = !result_value;
  set_constant_kind(result, (a_constant_repr_kind)ck_integer);
  set_integer_value(&result->variant.integer_value,
                    (a_host_large_integer)result_value);
#if DEBUG
  if (debug_level  >= 5) {
    db_binary_operation(db_operator_names[op],
                        constant_1, constant_2, result, ec_no_error);
  }  /* if */
#endif /* DEBUG */
}  /* do_pmcompare */


static void do_reflection_compare(a_constant            *constant_1,
                                  an_expr_operator_kind op,
                                  a_constant            *constant_2,
                                  a_constant            *result)
/*
Fold an equality operation on two pointer-to-member constants.  constant_1 and
constant_2 are compared according to the indicated operator (which must be
eok_eq or eok_ne) and *result is set to an integer 0 or 1 for the result.
*/
{
  a_boolean  result_value;

  check_assertion(is_reflection_type(constant_1->type) &&
                  is_reflection_type(constant_2->type));
  check_assertion(op == eok_eq || op == eok_ne);
  result_value = eq_constants(constant_1, constant_2);
  /* result_value is now set for the "==" case.  Complement it for the "!="
     case. */
  if (op == eok_ne) result_value = !result_value;
  set_constant_kind(result, (a_constant_repr_kind)ck_integer);
  set_integer_value(&result->variant.integer_value,
                    (a_host_large_integer)result_value);
}  /* do_reflection_compare */

#if UPC_EXTENSIONS_ALLOWED

static void set_integer_constant_to_upc_threads(a_constant  *ic)
/*
Change the given integer constant N to N*THREADS (unless N is zero).
*/
{
  if (!is_zero_constant(ic)) {
    ic->kind = (a_constant_repr_kind)ck_upc_threads;
  }  /* if */
}  /* set_integer_constant_to_upc_threads */


static void convert_upc_threads_constant_to_integer(a_constant  *tc,
                                                    a_constant  *ic)
/*
tc is a constant representing N*THREADS.  Set ic to N.
*/
{
  copy_constant(tc, ic);
  ic->kind = (a_constant_repr_kind)ck_integer;
}  /* convert_upc_threads_constant_to_integer */


static void binary_upc_threads_operation(
                                  an_expr_operator_kind op,
                                  a_constant            *constant_1,
                                  a_constant            *constant_2,
                                  a_type_ptr            result_type,
                                  a_constant            *result,
                                  a_boolean             constant_context,
                                  a_boolean             evaluated_context,
                                  a_boolean             *did_not_fold,
                                  a_boolean             *template_constant,
                                  a_source_position     *err_pos)
/*
Attempt to fold an operation (op) on two constants (constant_1, constant_2),
at least one of which is a UPC THREADS-based constant.  See binary_operation
(below) for the meaning of the other parameters.  Operations on UPC THREADS-
based constants are handled by converting them to integer constants, and
then converting the result back to being THREADS-based if appropriate.
*/
{
  a_constant_ptr  tmp = local_constant();

  check_assertion(upc_dynamic_threads());
  if (constant_1->kind == (a_constant_repr_kind)ck_upc_threads &&
      constant_2->kind == (a_constant_repr_kind)ck_upc_threads) {
    /* E.g., THREADS/THREADS. */
    /* This case is not folded because the non-dynamic case might overflow.
       E.g., "3*THREADS/THREADS" cannot be folded to "3" because "3*THREADS"
       may be an overflow when THREADS is statically specified. */
    *did_not_fold = TRUE;
  } else {
    a_constant_ptr  nonthread_constant;
    if (constant_2->kind == (a_constant_repr_kind)ck_upc_threads) {
      /* E.g., 3*THREADS. */
      convert_upc_threads_constant_to_integer(constant_2, tmp);
      constant_2 = tmp;
      nonthread_constant = constant_1;
    } else {
      /* E.g., THREADS*3. */
      check_assertion(constant_1->kind ==
                                        (a_constant_repr_kind)ck_upc_threads);
      convert_upc_threads_constant_to_integer(constant_1, tmp);
      constant_1 = tmp;
      nonthread_constant = constant_2;
    }  /* if */
    switch (op) { 
      case eok_shiftl:
        if (nonthread_constant == constant_1) {
          *did_not_fold = TRUE;
          break;
        }  /* if */
        FALLTHROUGH
      case eok_multiply: 
        check_assertion(C_mode());  /* Would need error_detected in C++. */
        binary_operation(op, constant_1, constant_2, result_type, result, 
                         constant_context, evaluated_context, did_not_fold,
                         template_constant, (an_error_code *)NULL, err_pos); 
        if (!*did_not_fold) { 
          /* Convert the folded result back to a multiple of THREADS (unless
             it is zero). */
          set_integer_constant_to_upc_threads(result); 
        }  /* if */ 
        break; 
      case eok_add: 
      case eok_subtract: 
        /* Check for adding or subtracting zero */ 
        if (is_zero_constant(nonthread_constant)) {
          check_assertion(C_mode());  /* Would need error_detected in C++. */
          binary_operation(op, constant_1, constant_2, result_type, result, 
                           constant_context, evaluated_context, did_not_fold, 
                           template_constant, (an_error_code *)NULL, err_pos); 
          if (!*did_not_fold) { 
            set_integer_constant_to_upc_threads(result); 
          }  /* if */ 
        } else { 
          *did_not_fold = TRUE; 
        }  /* if */ 
        break; 
      default: 
        /* Cannot fold other operations */ 
        *did_not_fold = TRUE; 
        break; 
    }  /* switch */ 
  }  /* if */
  release_local_constant(&tmp);
}  /* binary_upc_threads_operation */

#endif /* UPC_EXTENSIONS_ALLOWED */
#if GNU_VECTOR_TYPES_ALLOWED

static void decompose_vector_binary_operation(
                                      an_expr_operator_kind op,
                                      a_constant            *constant_1,
                                      a_constant            *constant_2,
                                      a_type_ptr            result_type,
                                      a_constant            *result,
                                      a_boolean             constant_context,
                                      a_boolean             evaluated_context,
                                      a_boolean             *did_not_fold,
                                      a_boolean             *template_constant,
                                      an_error_code         *error_detected,
                                      a_source_position     *err_pos)
/*
Called by binary_operation when the result type is a GNU vector type to
recursively perform the specified operation on each of the vector's
elements.  Produces in *result a vector ck_aggregate constant with the
number of elements specified by the vector type, allocating the element
constants in the current memory region.  See binary_operation for a full
description of the parameters.
*/
{
  a_constant_ptr opnd1_elem;
  a_constant_ptr opnd2_elem;
  a_boolean      opnd1_elem_from_aggr;
  a_boolean      opnd2_elem_from_aggr;
  a_boolean      opnd1_local_constant = FALSE;
  a_boolean      opnd2_local_constant = FALSE;
  a_boolean      local_not_folded = FALSE;
  sizeof_t       num_result_elements;
  a_type_ptr     result_elem_type;
  sizeof_t       elem_no;

  check_assertion(result_type->kind == (a_type_kind)tk_vector);
  result_elem_type = result_type->variant.vector.element_type;
  num_result_elements =
                     result_type->size / skip_typerefs(result_elem_type)->size;
  /* Change vector operators into their corresponding scalar operators. */
  switch (op) {
    case eok_vector_eq:    op = eok_eq;      break;
    case eok_vector_ne:    op = eok_ne;      break;
    case eok_vector_lt:    op = eok_lt;      break;
    case eok_vector_gt:    op = eok_gt;      break;
    case eok_vector_le:    op = eok_le;      break;
    case eok_vector_ge:    op = eok_ge;      break;
    case eok_vector_land:  op = eok_land;    break;
    case eok_vector_lor:   op = eok_lor;     break;
    default:               /* No change. */  break;
  }  /* switch */
  /* Either of the operands may be a scalar or a vector.  If the operand
     is an aggregate (vector), use the first element constant as the
     operand; otherwise, use the constant itself. */
  if (constant_1->kind == (a_constant_repr_kind)ck_aggregate) {
    opnd1_elem_from_aggr = TRUE;
    opnd1_elem = constant_1->variant.aggregate.first_constant;
  } else {
    opnd1_elem_from_aggr = FALSE;
    opnd1_elem = constant_1;
  }  /* if */
  if (constant_2->kind == (a_constant_repr_kind)ck_aggregate) {
    opnd2_elem_from_aggr = TRUE;
    opnd2_elem = constant_2->variant.aggregate.first_constant;
  } else {
    opnd2_elem_from_aggr = FALSE;
    opnd2_elem = constant_2;
  }  /* if */
  /* Initialize the result. */
  clear_constant(result, (a_constant_repr_kind)ck_aggregate);
  result->type = result_type;
  /* Step through the elements of the vector, calling binary_operation to
     compute the result vector elements. */
  for (elem_no = 0; elem_no < num_result_elements && !local_not_folded;
       ++elem_no) {
    /* Allocate the constant for this element of the result.  The constant
       will be overwritten by binary_operation below, so just call it an
       error constant for now to minimize the cost of initialization. */
    a_constant_ptr result_elem =
                                alloc_constant((a_constant_repr_kind)ck_error);
    /* A vector aggregate may be partially- or value-initialized.  If so,
       the operand element will be NULL at this point if we've stepped past
       the end of the list, and we need to create a zero of the element
       type to use for the rest of the loop. */
    if (opnd1_elem == NULL) {
      opnd1_elem = local_constant();
      opnd1_local_constant = TRUE;
      opnd1_elem_from_aggr = FALSE;
      make_zero_of_proper_type(result_elem_type, opnd1_elem);
    }  /* if */
    if (opnd2_elem == NULL) {
      opnd2_elem = local_constant();
      opnd2_local_constant = TRUE;
      opnd2_elem_from_aggr = FALSE;
      make_zero_of_proper_type(result_elem_type, opnd2_elem);
    }  /* if */
    /* Recursively call binary_operation to compute the result for this
       element and add the element to the result aggregate. */
    binary_operation(op, opnd1_elem, opnd2_elem, result_elem_type, result_elem,
                     constant_context, evaluated_context, &local_not_folded,
                     template_constant, error_detected, err_pos);
    add_constant_to_aggregate(result_elem, result, (a_base_class_ptr)NULL,
                              (a_field_ptr)NULL);
    /* Step to the next element in both operands, unless the operand is a
       scalar or we ran off the end of the operand aggregate and are using
       a local zero. */
    if (opnd1_elem_from_aggr) {
      opnd1_elem = opnd1_elem->next;
    }  /* if */
    if (opnd2_elem_from_aggr) {
      opnd2_elem = opnd2_elem->next;
    }  /* if */
  }  /* for */
  /* Release any local constants. */
  if (opnd1_local_constant) {
    release_local_constant(&opnd1_elem);
  }  /* if */
  if (opnd2_local_constant) {
    release_local_constant(&opnd2_elem);
  }  /* if */
  /* Propagate the result to the caller. */
  *did_not_fold = local_not_folded;
}  /* decompose_vector_binary_operation */

#endif /* GNU_VECTOR_TYPES_ALLOWED */

void binary_operation(an_expr_operator_kind op,
		      a_constant            *constant_1,
		      a_constant            *constant_2,
		      a_type_ptr            result_type,
		      a_constant            *result,
                      a_boolean             constant_context,
                      a_boolean             evaluated_context,
		      a_boolean             *did_not_fold,
                      a_boolean             *template_constant,
                      an_error_code         *error_detected,
                      a_source_position     *err_pos)
/*
Fold a two-operand constant operation.  op indicates the operation,
and constant_1 and constant_2 are the operands.  result_type indicates
the desired result type.  The result constant is placed in *result.
If constant_context is FALSE, this operation is being evaluated as
part of a nonconstant expression, so any error is reduced to a
warning and *did_not_fold is returned TRUE.  If evaluated_context
is FALSE, this operation is being done in a not-evaluated context
(e.g., a sizeof or a dead branch of a "?" operator), so any error
is thrown away and *did_not_fold is returned TRUE. *did_not_fold is
also returned TRUE if the operation could not be folded for any other
reason (*template_constant is returned TRUE if the reason is that
the constant is a template parameter constant).  If error_detected is
non-NULL, set *error_detected to the code for any error detected, and
do not issue the diagnostic, or set it to ec_no_error if there was no
error.  *err_pos is used as the position for any diagnostics issued.
*/
{
  an_error_code     err_code;
  an_error_severity err_severity;
  a_boolean         depends_on_fp_mode = FALSE;

  db_enter(5, "binary_operation");

  *did_not_fold = FALSE;
  *template_constant = FALSE;
  if (error_detected != NULL) *error_detected = ec_no_error;
  err_code = ec_no_error;
  err_severity = es_warning;

  if (is_error_constant(constant_1) || is_error_constant(constant_2)) {
    /* One and/or the other of the constants is an error constant; set the
       result to an error constant and return. */
    set_error_constant(result);
  } else if (!C_mode() &&
             (constant_1->kind == (a_constant_repr_kind)ck_template_param ||
              constant_2->kind == (a_constant_repr_kind)ck_template_param ||
              (context_may_have_dependent_types() &&
               ((constant_1->kind == (a_constant_repr_kind)ck_aggregate &&
                 is_template_dependent_type(constant_1->type)) ||
                (constant_2->kind == (a_constant_repr_kind)ck_aggregate &&
                 is_template_dependent_type(constant_2->type)) ||
                is_template_dependent_type(result_type))))) {
    /* An operation on a template parameter constant cannot be folded. */
    *did_not_fold = TRUE;
    *template_constant = TRUE;
#if GNU_EXTENSIONS_ALLOWED
  } else if (constant_1->kind == (a_constant_repr_kind)ck_label_difference ||
             constant_2->kind == (a_constant_repr_kind)ck_label_difference) {
    /* The representation for a GNU label difference (&&K-&&L) is not
       a constant known at compile time. */
    *did_not_fold = TRUE;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if UPC_EXTENSIONS_ALLOWED
  } else if (upc_mode &&
             (constant_1->kind == (a_constant_repr_kind)ck_upc_mythread || 
              constant_2->kind == (a_constant_repr_kind)ck_upc_mythread ||
              is_ptr_to_shared_type(constant_1->type) ||
              is_ptr_to_shared_type(constant_2->type))) {
    /* Operations involving MYTHREAD-based constants cannot be folded.
       Operations on addresses of shared data should not be folded in the
       front end either (though a back end might do so). */
    *did_not_fold = TRUE;
  } else if (upc_mode &&
             (constant_1->kind == (a_constant_repr_kind)ck_upc_threads ||
              constant_2->kind == (a_constant_repr_kind)ck_upc_threads)) {
    binary_upc_threads_operation(op, constant_1, constant_2, result_type,
                                 result, constant_context, evaluated_context,
                                 did_not_fold, template_constant, err_pos);
#endif /* UPC_EXTENSIONS_ALLOWED */
#if FIXED_POINT_ALLOWED
  } else if ((constant_1->kind == (a_constant_repr_kind)ck_fixed_point ||
              constant_2->kind == (a_constant_repr_kind)ck_fixed_point) &&
             (constant_1->kind != (a_constant_repr_kind)ck_fixed_point ||
              constant_2->kind != (a_constant_repr_kind)ck_fixed_point) &&
             (op != (an_expr_operator_kind)eok_shiftl) &&
             (op != (an_expr_operator_kind)eok_shiftr) &&
             (op != (an_expr_operator_kind)eok_add) &&
             (op != (an_expr_operator_kind)eok_subtract) &&
             (op != (an_expr_operator_kind)eok_multiply) &&
             (op != (an_expr_operator_kind)eok_divide)) {
    /* Fixed-point operations, except for the ones listed above,
       are not folded if the other operand is not also fixed-point. */
    *did_not_fold = TRUE;
#endif /* FIXED_POINT_ALLOWED */
#if GNU_VECTOR_TYPES_ALLOWED
  } else if (result_type->kind == (a_type_kind)tk_vector) {
    /* Perform the operation recursively on each of the elements of the
       vector. */
    decompose_vector_binary_operation(op, constant_1, constant_2, result_type,
                                      result, constant_context,
                                      evaluated_context, did_not_fold,
                                      template_constant, error_detected,
                                      err_pos);
#endif /* GNU_VECTOR_TYPES_ALLOWED */
  } else {
    clear_constant(result, (a_constant_repr_kind)ck_error);
    result->type = result_type;
    if (is_addr_constant_cast_to_integral_type(constant_1)) {
      /* The first constant is an address constant cast to an integral
         type.  Such a constant is a link-time constant, not a compile-time
         constant.  We cannot in general do operations on it.  However,
         we can handle the special cases
           (int)addr_constant + int_constant
           (int)addr_constant - int_constant
         by using the pointer add/subtract routine. */
      if ((op == (an_expr_operator_kind)eok_add ||
           op == (an_expr_operator_kind)eok_subtract) &&
          constant_2->kind == (a_constant_repr_kind)ck_integer) {
#if CHECKING
        if (!is_integral_or_enum_type(constant_2->type)) {
          internal_error("binary_operation: address constant +- non-integer");
        }  /* if */
#endif /* CHECKING */
        do_padd(constant_1, op, constant_2, result, did_not_fold,
                &err_code, &err_severity);
#if GNU_EXTENSIONS_ALLOWED
      } else if ((gcc_mode ||
                  (gpp_mode && gnu_version < 40000)) &&
                 op == (an_expr_operator_kind)eok_subtract &&
                 is_addr_constant_cast_to_integral_type(constant_2)) {
        /* Allow
             (int)addr_constant - (int)addr_constant
           in GNU mode. */
        do_pdiff(constant_1, constant_2, result, did_not_fold,
                 &err_code, &err_severity);
        if (!*did_not_fold &&
            result->kind == (a_constant_repr_kind)ck_label_difference) {
          /* do_pdiff produces a result of ptrdiff_t type, but in cases like
             this the type cast to should be preserved (taking into account
             the usual arithmetic promotions). */
          result->type = result_type;
        }  /* if */
      } else if (gnu_mode &&
                 op == (an_expr_operator_kind)eok_and &&
                 is_zero_constant(constant_2)) {
        /* gcc allows (int)"abc" & 0 as an integral constant. */
        do_and(constant_2 /* sic */, constant_2, result);
#endif /* GNU_EXTENSIONS_ALLOWED */
      } else {
        *did_not_fold = TRUE;
      }  /* if */
    } else if (is_addr_constant_cast_to_integral_type(constant_2)) {
      /* The second constant is an address constant cast to an integral
         type.  Such a constant is a link-time constant, not a compile-time
         constant.  We cannot in general do operations on it.  However,
         we can handle the special case
           int_constant + (int)addr_constant
         by using the pointer add routine. */
      if (op == (an_expr_operator_kind)eok_add &&
          constant_1->kind == (a_constant_repr_kind)ck_integer) {
#if CHECKING
        if (!is_integral_or_enum_type(constant_1->type)) {
          internal_error("binary_operation: non-integer + address constant");
        }  /* if */
#endif /* CHECKING */
        /* Note that we reverse the operands in the call so that the address
           constant is first. */
        do_padd(constant_2, op, constant_1, result, did_not_fold, &err_code,
                &err_severity);
      } else if (gnu_mode &&
                 op == (an_expr_operator_kind)eok_and &&
                 is_zero_constant(constant_1)) {
        /* gcc allows 0 & (int)"abc" as an integral constant. */
        do_and(constant_1, constant_1 /* sic */, result);
      } else {
        *did_not_fold = TRUE;
      }  /* if */
    } else {
      a_type_kind  operation_type_kind =
            binary_operation_type_kind(op, constant_1->type, constant_2->type);
      switch (op) {
        case eok_add:
          switch (operation_type_kind) {
            case tk_integer:
              do_iadd(constant_1, constant_2, result, &err_code,
                      &err_severity);
              break;
#if FIXED_POINT_ALLOWED
            case tk_fixed_point:
              do_fxadd(constant_1, constant_2, result,
                       did_not_fold, &err_code, &err_severity);
              break;
#endif /* FIXED_POINT_ALLOWED */
            case tk_float:
#if C99_IL_EXTENSIONS_SUPPORTED
            case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
              do_fadd(constant_1, constant_2, result, &err_code,
                      &err_severity, &depends_on_fp_mode);
              break;
#if C99_IL_EXTENSIONS_SUPPORTED
            case tk_complex:
              do_xadd(constant_1, constant_2, result, &err_code,
                      &err_severity, &depends_on_fp_mode);
              break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
            default:
              unexpected_condition();
          }  /* switch */
          break;
        case eok_subtract:
          switch (operation_type_kind) {
            case tk_integer:
              do_isubtract(constant_1, constant_2, result, &err_code,
                           &err_severity);
              break;
#if FIXED_POINT_ALLOWED
            case tk_fixed_point:
              do_fxsubtract(constant_1, constant_2, result,
                            did_not_fold, &err_code, &err_severity);
              break;
#endif /* FIXED_POINT_ALLOWED */
            case tk_float:
#if C99_IL_EXTENSIONS_SUPPORTED
            case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
              do_fsubtract(constant_1, constant_2, result, &err_code,
                           &err_severity, &depends_on_fp_mode);
              break;
#if C99_IL_EXTENSIONS_SUPPORTED
            case tk_complex:
              do_xsubtract(constant_1, constant_2, result, &err_code,
                      &err_severity, &depends_on_fp_mode);
              break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
            default:
              unexpected_condition();
          }  /* switch */
          break;
        case eok_multiply:
          switch (operation_type_kind) {
            case tk_integer:
              do_imultiply(constant_1, constant_2, result, &err_code,
                           &err_severity);
              break;
#if FIXED_POINT_ALLOWED
            case tk_fixed_point:
              do_fxmultiply(constant_1, constant_2, result,
                            did_not_fold, &err_code, &err_severity);
              break;
#endif /* FIXED_POINT_ALLOWED */
            case tk_float:
#if C99_IL_EXTENSIONS_SUPPORTED
            case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
              do_fmultiply(constant_1, constant_2, result,
                           &err_code, &err_severity, &depends_on_fp_mode);
              break;
#if C99_IL_EXTENSIONS_SUPPORTED
            case tk_complex:
              do_xmultiply(constant_1, constant_2, result,
                           &err_code, &err_severity, &depends_on_fp_mode);
              break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
            default:
              unexpected_condition();
          }  /* switch */
          break;
        case eok_divide:
          switch (operation_type_kind) {
            case tk_integer:
              do_idivide(constant_1, constant_2, result, &err_code,
                         &err_severity);
              break;
#if FIXED_POINT_ALLOWED
            case tk_fixed_point:
              do_fxdivide(constant_1, constant_2, result,
                          did_not_fold, &err_code, &err_severity);
              break;
#endif /* FIXED_POINT_ALLOWED */
            case tk_float:
#if C99_IL_EXTENSIONS_SUPPORTED
            case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
              do_fdivide(constant_1, constant_2, result,
                         &err_code, &err_severity, &depends_on_fp_mode);
              break;
#if C99_IL_EXTENSIONS_SUPPORTED
            case tk_complex:
              do_xdivide(constant_1, constant_2, result,
                         &err_code, &err_severity, &depends_on_fp_mode);
              break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
            default:
              unexpected_condition();
          }  /* switch */
          break;
        case eok_remainder:
          do_remainder(constant_1, constant_2, result, &err_code,
                       &err_severity);
          break;
        case eok_shiftl:
#if FIXED_POINT_ALLOWED
          if (operation_type_kind == (a_type_kind)tk_fixed_point) {
            do_fxshiftl(constant_1, constant_2, result, &err_code,
                        &err_severity);
          } else
#endif /* FIXED_POINT_ALLOWED */
          /* Do not insert code here. */
          {
            do_shiftl(constant_1, constant_2, result, &err_code,
                      &err_severity);
          }  /* if */
          break;
        case eok_shiftr:
#if FIXED_POINT_ALLOWED
          if (operation_type_kind == (a_type_kind)tk_fixed_point) {
            do_fxshiftr(constant_1, constant_2, result, &err_code,
                        &err_severity);
          } else
#endif /* FIXED_POINT_ALLOWED */
          /* Do not insert code here. */
          {
            do_shiftr(constant_1, constant_2, result, &err_code,
                      &err_severity);
          }  /* if */
          break;
        case eok_eq:
        case eok_ne:
        case eok_gt:
        case eok_lt:
        case eok_ge:
        case eok_le:
          switch (operation_type_kind) {
            case tk_integer:
              do_icompare(constant_1, op, constant_2, result);
              break;
#if FIXED_POINT_ALLOWED
            case tk_fixed_point:
              do_fxcompare(constant_1, op, constant_2, result);
              break;
#endif /* FIXED_POINT_ALLOWED */
            case tk_float:
              do_fcompare(constant_1, op, constant_2, result,
                          &depends_on_fp_mode);
              break;
#if C99_IL_EXTENSIONS_SUPPORTED
            case tk_complex:
              do_xcompare(constant_1, op, constant_2, result);
              break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
            case tk_pointer:
              do_pcompare(constant_1, op, constant_2, result, did_not_fold,
                          &err_code, &err_severity);
              break;
            case tk_ptr_to_member:
              do_pmcompare(constant_1, op, constant_2, result);
              break;
            case tk_nullptr:
              /* This is handled as an integer comparison, like an old-style
                 null pointer constant. */
              do_icompare(constant_1, op, constant_2, result);
              break;
            case tk_reflection:
              do_reflection_compare(constant_1, op, constant_2, result);
              break;
            default:
              unexpected_condition();
          }  /* switch */
          break;
#if GNU_EXTENSIONS_ALLOWED
        case eok_gnu_max:
        case eok_gnu_min:
          switch (operation_type_kind) {
            case tk_integer:
              do_ignu_min_max(constant_1, op, constant_2, result);
              break;
            case tk_float:
              do_fgnu_min_max(constant_1, op, constant_2, result);
              break;
            case tk_pointer:
              *did_not_fold = TRUE;
              break;
            default:
              unexpected_condition();
          }  /* switch */
          break;
#endif /* GNU_EXTENSIONS_ALLOWED */
        case eok_and:
          do_and(constant_1, constant_2, result);
          break;
        case eok_or:
          do_or(constant_1, constant_2, result);
          break;
        case eok_xor:
          do_xor(constant_1, constant_2, result);
          break;
        case eok_land:
          do_land(constant_1, constant_2, result, did_not_fold);
          break;
        case eok_lor:
          do_lor(constant_1, constant_2, result, did_not_fold);
          break;
        case eok_comma:
          copy_constant(constant_2, result);
          break;
#if C99_IL_EXTENSIONS_SUPPORTED
        case eok_jmultiply:
          do_jmultiply(constant_1, constant_2, result,
                       &err_code, &err_severity, &depends_on_fp_mode);
          break;
        case eok_jdivide:
          do_jdivide(constant_1, constant_2, result,
                     &err_code, &err_severity, &depends_on_fp_mode);
          break;
        case eok_fjadd:
        case eok_jfadd:
        case eok_fjsubtract:
        case eok_jfsubtract:
          /* Mixed real/imaginary addition/subtraction. */
          do_real_imag_add_subtract(constant_1, op, constant_2, result,
                                    &err_code, &err_severity,
                                    &depends_on_fp_mode);
          break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */

        case eok_pdiff:
          do_pdiff(constant_1, constant_2, result, did_not_fold, &err_code,
                   &err_severity);
          break;
        case eok_padd:
          { a_constant_ptr ptr_con = constant_1;
            a_constant_ptr int_con = constant_2;
            /* The operands of pointer "+" can be in either order. */
            if (is_pointer_type(constant_2->type)) {
              ptr_con = constant_2;
              int_con = constant_1;
            }  /* if */
            do_padd(ptr_con, op, int_con, result, did_not_fold,
                    &err_code, &err_severity);
          }
          break;
        case eok_psubtract:
          do_padd(constant_1, op, constant_2, result, did_not_fold,
                  &err_code, &err_severity);
          break;
        default:
          unexpected_condition_str("binary_operation: bad binary operator");
      }  /* switch */
    }  /* if */
    if (err_code != ec_no_error) {
      /* There was an error or warning. */
      issue_folding_diagnostic(err_code, err_severity, constant_context,
                               evaluated_context, /*silence_warning=*/FALSE,
                               did_not_fold, error_detected, err_pos, result);
      if (err_severity == es_error) depends_on_fp_mode = FALSE;
    }  /* if */
    /* If either constant was formed using operations that are not allowed in
       forming a null pointer constant, the result cannot be used as a null
       pointer constant.  In C++11 mode, only a literal can produce a null
       pointer constant. */
    result->null_pointer_constant_ruled_out =
                        (cpp11_mode && !(microsoft_mode && ms_permissive) &&
                         !gpp_version_is(<60000)) ||
                        constant_1->null_pointer_constant_ruled_out ||
                        constant_1->kind != (a_constant_repr_kind)ck_integer ||
                        (constant_1->implicit_cast &&
                         !is_nullptr_type(constant_1->type)) ||
                        constant_2->null_pointer_constant_ruled_out ||
                        constant_2->kind != (a_constant_repr_kind)ck_integer ||
                        (constant_2->implicit_cast &&
                         !is_nullptr_type(constant_2->type));
    if (depends_on_fp_mode && !constant_context) {
      /* In a non-constant context, leave an operation to be done at runtime
         if its result depends on the floating-point mode. */
      *did_not_fold = TRUE;
    }  /* if */
  }  /* if */

  db_exit();
}  /* binary_operation */


a_boolean fold_expr(an_expr_node_ptr             expr,
                    a_constant                   *result_con)
/*
Attempt to fold the expression "expr" to a constant as part of a
constexpr evaluation, by substituting argument constant values for
parameters.  If the expression folds to a constant, place the constant
in *result_con and return TRUE; otherwise, return FALSE.  If the
expression is a glvalue, do not fold (see fold_glvalue_expr instead).
*/
{
  a_boolean    folded;

  if (is_glvalue_node(expr)) {
    /* Only fold expressions that produce prvalue results. */
    folded = FALSE;
  } else if (is_template_dependent_context() && !scope_stack_top().is_rescan &&
             expr_is_instantiation_dependent(expr)) {
    /* Don't attempt to fold expressions that are dependent. */
    folded = FALSE;
  } else {
    a_diag_list  diag_list;
    clear_diag_list(&diag_list);
    folded = interpret_expr(expr, /*is_constant_evaluated=*/FALSE,
                            /*force_rvalue=*/FALSE, result_con, &diag_list);
    if (folded && address_con_is_unknown_object(result_con)) {
      /* P2280R4 allows a reference to an unspecified object as a constant
         expression, but that object's identity is not a constant address
         and must not be recorded for lowering. */
      folded = FALSE;
    }  /* if */
    discard_more_info_list(&diag_list);
  }  /* if */
  return folded;
}  /* fold_expr */


static a_boolean fold_glvalue_expr(an_expr_node_ptr             expr,
                                   a_constant                   *result_con)
/*
Attempt to fold the glvalue expression "expr" to a constant address as
part of constexpr evaluation, by substituting argument constant values for
parameters.  If the expression folds to a constant address, place the
constant in *result_con and return TRUE; otherwise, return FALSE.  If
the expression is not a glvalue, do not fold (see fold_expr instead).
*/
{
  a_boolean  folded;

  expr = skip_parens(expr);
  if (!is_glvalue_node(expr)) {
    /* Do not fold expressions that are not glvalues. */
    folded = FALSE;
#if DO_IL_LOWERING
  } else if (il_lowering_underway) {
    /* Don't attempt expression folding during lowering. */
    folded = FALSE;
#endif /* DO_IL_LOWERING */
  } else {
    a_diag_list  diag_list;
    clear_diag_list(&diag_list);
    folded = interpret_expr(expr, /*is_constant_evaluated=*/FALSE,
                            /*force_rvalue=*/FALSE, result_con, &diag_list);
    if (folded && address_con_is_unknown_object(result_con)) {
      /* P2280R4 allows a reference to an unspecified object as a constant
         expression, but that object's identity is not a constant address. */
      folded = FALSE;
    } else if (folded && is_reference_type(result_con->type)) {
      /* The interpreter will produce a reference constant when folding a
         glvalue.  Make it a pointer constant instead. */
      a_type_ptr  tpt = type_pointed_to(result_con->type);
      result_con->type = make_pointer_type(tpt);
    }  /* if */
    discard_more_info_list(&diag_list);
  }  /* if */
  return folded;
}  /* fold_glvalue_expr */


static void accum_field_offset(a_constant_ptr        total_offset,
                               a_field_ptr           field,
                               a_subobject_path_ptr  *p_subobject_path,
                               a_boolean             *ovflo)
/*
total_offset represents an offset: Add to it the offset of the given field,
and set *ovflo to TRUE if an overflow occurred.
*/
{
  an_integer_value            field_offset;
  a_type_ptr                  field_class;
  a_class_type_supplement_ptr ctsp;

  for (;;) {
    set_unsigned_integer_value(&field_offset, field->offset);
    add_mixed_signed_integer_values(&total_offset->variant.integer_value,
                                    int_constant_is_signed(total_offset),
                                    &field_offset, /*is_signed=*/FALSE, ovflo);
    /* See if the field is from an anonymous union. */
    field_class = parent_class_of(field);
    ctsp = class_type_supp(field_class);
    if (p_subobject_path != NULL) {
      a_subobject_path_ptr  path = alloc_subobject_path();
      path->next = *p_subobject_path;
      path->variant.field = field;
      *p_subobject_path = path;
    }  /* if */
    if (ctsp->anonymous_union_kind != (an_anonymous_union_kind)auk_field) {
      break;
    }  /* if */
    /* Yes, it's from a (standard) anonymous union.  Loop to add in the offset
       of the parent field.  (Nonstandard anonymous unions/structs have an
       explicit field selection in the tree, and should not be expanded
       here.) */
    field = ctsp->anonymous_union_field;
  }  /* for */
}  /* accum_field_offset */


a_boolean fold_field_selection(a_constant  *constant_1,
                               a_field_ptr field,
                               a_type_ptr  result_type,
                               a_constant  *result)
/*
Fold a constant field selection operation.  constant_1 is the pointer to the
struct/union; field is the selected field.  The result type (pointer to the
field type) is given by result_type.  The result is put in *result.
Return TRUE if the selection can be folded, FALSE if not (the latter is
returned for template-dependent cases).  This folding operation is not done
through the usual interface because a field cannot be passed as a constant.
*/
{
  a_boolean      is_constant = TRUE;
  a_constant_ptr offset = local_constant();
  a_boolean      err;

  copy_constant(constant_1, result);
  if (is_error_constant(constant_1)) {
    /* An error constant stays the same. */
  } else if (constant_is(constant_1, ck_template_param)) {
    /* A template parameter constant.  This shows up in cases like
         ((T *)0)->x
       which can come up as part of the expansion of offsetof. */
    is_constant = FALSE;
  } else {
    a_subobject_path_ptr  field_path = NULL, *p_field_path = NULL;
    if (constant_is(result, ck_address)) {
      /* If the field is an inherited member of the class pointed to (possible
         with pointer-to-member operations), apply a base-class cast first. */
      a_type_ptr  ptr_class = type_pointed_to(constant_1->type),
                  uptr_class = skip_typerefs(ptr_class),
                  fld_class = parent_class_of(field);
      if (type_is(uptr_class, tk_pointer)) {
        /* The constant may have reference to pointer type. */
        ptr_class = type_pointed_to(uptr_class);
        uptr_class = skip_typerefs(ptr_class);
      }  /* if */
      while (class_type_supp(fld_class)->anonymous_union_kind == auk_field) {
        fld_class = parent_class_of(fld_class);
      }  /* while */
      if (!same_entities(uptr_class, fld_class)) {
        /* The field is either in a base or in a derived class of the object
           pointed to.  Apply the needed cast. */
        a_boolean         did_not_fold = FALSE;
        an_error_code     err_code = ec_no_error;
        a_base_class_ptr  bcp = find_base_class_of(ptr_class, fld_class);
        if (bcp != NULL) {
          fold_base_class_cast(constant_1, bcp, ptr_class, result,
                               /*check_cast_access=*/FALSE,
                               /*check_ambiguity=*/TRUE,
                               /*is_implicit_cast=*/TRUE,
                               /*is_object_pointer=*/TRUE,
                               /*omit_backing_expr=*/TRUE,
                               &did_not_fold, &error_position, &err_code);
        } else {
          bcp = find_base_class_of(fld_class, uptr_class);
          if (bcp == NULL) {
            is_constant = FALSE;
            goto done;
          }  /* if */
          result->type = make_identically_qualified_type(fld_class, ptr_class);
          result->type = make_pointer_type(result->type);
          fold_derived_class_cast(constant_1, bcp, result, &error_position,
                                  &err_code);
        }  /* if */
        if (did_not_fold || err_code != ec_no_error ||
            constant_is(result, ck_error)) {
          is_constant = FALSE;
          goto done;
        }  /* if */
      }  /* if */
      p_field_path = &field_path;
    }  /* if */
    /* Take the pointer offset, ... */
    get_pointer_offset(result, offset);
    /* ... and add the offset of the field. */
    accum_field_offset(offset, field, p_field_path, &err);
    /* Put the offset into the result pointer constant.  Note that no
       overflow/object-size checking is needed, since the field has to be
       within the underlying object. */
    set_pointer_offset(result, offset, &err);
    result->type = result_type;
    result->implicit_cast = TRUE;
    /* Update the subobject path if applicable. */
    if (field_path != NULL) {
      *last_subobject_path_link(result) = field_path;
    }  /* if */
  }  /* if */
done:
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "fold_field_selection: offset = ");
    if (is_constant) {
      db_constant(offset);
    } else {
      fprintf(f_debug, "<nonconstant>");
    }  /* if */
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* CHECKING */
  release_local_constant(&offset);
  return is_constant;
}  /* fold_field_selection */


static a_boolean constant_padd_or_subscript(
                             an_expr_node_ptr              expr,
                             a_constant                    *con,
                             a_boolean                     address_escapes,
                             a_constant_address_option_set options,
                             a_boolean                     *template_constant)
/*
expr is an expression for an eok_padd, eok_psubtract, or eok_subscript
operation.  If its result (eok_padd, eok_psubtract) or address (lvalue
eok_subscript) is constant, return the value/address in *con, and return TRUE.
address_escapes, options, and template_constant are as for
constant_glvalue_address_full (except that template_constant is always
non-NULL).
*/
{
  a_boolean        is_constant = FALSE;
  an_expr_node_ptr ptr_op = expr->variant.operation.operands;
  an_expr_node_ptr int_op = ptr_op->next;
  a_constant_ptr   ptr_con = local_constant();
  a_constant_ptr   int_con = local_constant();
  a_constant_ptr   int_con_ptr = NULL;

  *template_constant = FALSE;
  if (expr->variant.operation.pointer_operand_is_second) {
    /* The operands are in the order integer + pointer or integer[pointer]. */
    int_op = expr->variant.operation.operands;
    ptr_op = int_op->next;
  } else if (is_template_param_type(ptr_op->type)) {
    /* This case cannot be folded and, in C++23, may have ptr_op be a glvalue,
       which would be unexpected in processing below: Shortcut the folding
       process. */
    *template_constant = TRUE;
    goto done;
  }  /* if */
  /* See if we have or can get a constant for the integer operand. */
  if (constexpr_enabled) {
    if (fold_expr(int_op, int_con)) {
      int_con_ptr = int_con;
    }  /* if */
  } else if (is_constant_node(int_op)) {
    int_con_ptr = node_constant(int_op);
  }  /* if */
  if (int_con_ptr != NULL &&
      constant_prvalue_pointer_full(ptr_op, ptr_con, address_escapes,
                                    options, template_constant)) {
    /* Both operands are constant; fold to a constant address. */
    an_error_code     err_code;
    an_error_severity err_severity;
    a_boolean         did_not_fold;
    if (int_con_ptr->kind == (a_constant_repr_kind)ck_template_param ||
        ptr_con->kind  == (a_constant_repr_kind)ck_template_param) {
      /* At least one constant is a template parameter, so we're not going
         to fold this to a constant address. */
    } else if (is_error_constant(ptr_con) ||
               is_error_constant(int_con_ptr)) {
      /* At least one constant is an error.  Set the result to be an
         error constant as well and indicate that the expression is a
         constant to reduce error cascades. */
      set_error_constant(con);
      is_constant = TRUE;
    } else {
      do_padd(ptr_con, expr->variant.operation.kind, int_con_ptr, con,
              &did_not_fold, &err_code, &err_severity);
      if (!did_not_fold &&
          (err_code == ec_no_error || err_severity == es_warning)) {
        is_constant = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
done:
  release_local_constant(&ptr_con);
  release_local_constant(&int_con);
  return is_constant;
}  /* constant_padd_or_subscript */


static void make_constant_routine_address(a_routine_ptr  rout,
                                          a_constant_ptr con,
                                          a_boolean      address_escapes,
                                          a_boolean      *template_constant)
/*
Helper routine for constant_glvalue_address_full and
constant_prvalue_pointer_full to make a constant for the address of a routine.
address_escapes and template_constant are as for constant_glvalue_address_full
(except that template_constant is always non-NULL).
*/
{
  set_routine_address_constant(rout, con,
                               /*set_address_taken=*/address_escapes);
  if (!routine_type_is_nonstatic_member_function(rout->type) &&
      rout->source_corresp.is_class_member &&
      scp_parent_class(&rout->source_corresp)->
                                 variant.class_struct_union.is_nonreal_class) {
    /* In a prototype instantiation, a static member function of the
       current class is template-dependent. */
    *template_constant = TRUE;
  } else if (context_may_have_dependent_types() &&
             is_template_dependent_type(rout->type)) {
    *template_constant = TRUE;
  }  /* if */
}  /* make_constant_routine_address */


static a_boolean fold_object_expr(an_expr_node_ptr             expr,
                                  a_boolean                    want_addr,
                                  a_constant                   *result_con);


static a_boolean constant_glvalue_address_full(
                             an_expr_node_ptr              expr,
                             a_constant                    *con,
                             a_boolean                     address_escapes,
                             a_constant_address_option_set options,
                             a_boolean                     *template_constant)
/*
expr is a glvalue expression.  If it has a constant address, put that
address in *con and return TRUE.  Otherwise, return FALSE.  address_escapes is
TRUE if the address might escape from its immediate context and get saved
somewhere (if in doubt, the safe value is TRUE).  options contains a
set of additional options.  *template_constant is returned TRUE if the
constant is template-dependent.  If template_constant is NULL, a
template-dependent constant is labeled as such at this level.  Passing
it in as non-NULL is a signal that the caller would prefer to handle
that higher up.
*/
{
  a_boolean is_constant_addr = FALSE;
  a_boolean local_template_constant;

  if (template_constant == NULL) {
    template_constant = &local_template_constant;
  }  /* if */
  *template_constant = FALSE;
start_underlying_expression:
  expr = skip_parens(expr);
  if (constexpr_enabled && is_glvalue_node(expr) &&
      (is_operation_node(expr) ||
       expr->kind == (an_expr_node_kind)enk_builtin_operation) &&
      !is_template_dependent_context()) {
    /* Use the interpreter to fold the expression. */
    is_constant_addr = fold_glvalue_expr(expr, con);
    goto have_result;
  }  /* if */
  check_assertion(is_glvalue_node(expr) || is_error_node(expr) ||
                  is_template_dependent_context());
  switch (expr->kind) {
    case enk_error:
      /* Assume an error expression could have been an lvalue with a
         constant address. */
      is_constant_addr = TRUE;
      set_error_constant(con);
      break;
    case enk_variable:
      /* An lvalue for a variable. */
      { a_variable_ptr var = node_variable(expr);
        if (var->init_kind == initk_binding) {
          /* A variable that represents an "alias" for the underlying
             lvalue expression. */
          expr = var->initializer.bound_expr;
          goto start_underlying_expression;
        }  /* if */
        if ((variable_has_constant_address(var) ||
             ((options & CAO_TREAT_LOCAL_VAR_ADDR_AS_CONSTANT) &&
              var->storage_class == sc_auto)) &&
            !(reference_to_unknown_object_allowed &&
              is_any_reference_type(var->type))) {
          /* The variable has a constant address.  (Or we're pretending it
             has a static address when it's a local variable.  In that case,
             the caller should make sure not to save the resulting
             constant.)  A reference variable's own address is not the
             address of the object it is bound to. */
          a_storage_class sc = sc_unspecified;
          a_boolean       auto_case = (var->storage_class == sc_auto);
          if (auto_case) {
            /* Save and restore the storage class so the variable looks
               static. */
            sc = var->storage_class;
            var->storage_class = sc_static;
          }  /* if */
          is_constant_addr = TRUE;
          set_variable_address_constant(var, con,
                                        /*set_address_taken=*/address_escapes);
          if (auto_case) var->storage_class = sc;
          if (var->source_corresp.is_class_member &&
              scp_parent_class(&var->source_corresp)
                     ->variant.class_struct_union.is_nonreal_class) {
            /* In a prototype instantiation, a static data member of the
               current class is template-dependent. */
            *template_constant = TRUE;
          } else if (is_template_dependent_type(var->type)) {
            /* The variable has a template-dependent type. */
            *template_constant = TRUE;
          }  /* if */
        }  /* if */
      }
      break;
    case enk_routine:
      { /* An lvalue for a function.  Do not fold the case of a consteval
           function because it makes it harder to track invalid uses of such
           functions. */
        a_routine_ptr  rp = expr->variant.routine.ptr;
        if (!rp->is_consteval) {
          make_constant_routine_address(node_routine(expr), con,
                                        address_escapes, template_constant);
          is_constant_addr = TRUE;
        }  /* if */
      }
      break;
    case enk_constant:
      { a_constant_ptr econ = node_constant(expr);
        if (econ->kind == (a_constant_repr_kind)ck_string) {
          /* The address of a string is a constant. */
          is_constant_addr = TRUE;
          set_constant_address_constant(econ, con);
        } else if (econ->kind == (a_constant_repr_kind)ck_address) {
          is_constant_addr = TRUE;
          copy_constant(econ, con);
        } else if (econ->kind == (a_constant_repr_kind)ck_template_param) {
          /* A dependent constant lvalue.  Just cast it to give it the
             appropriate pointer type to simulate the lvalue-to-rvalue
             conversion. */
          a_type_ptr tp = expr->type;
          if (is_array_type(tp)) {
            /* An array lvalue decays to a pointer to its element type. */
            tp = array_element_type(tp);
          }  /* if */
          make_template_param_cast_constant(econ, con, make_pointer_type(tp),
                                            /*is_explicit=*/FALSE);
          *template_constant = TRUE;
          is_constant_addr = TRUE;
        }  /* if */
      }
      break;
    case enk_typeid:
      { an_expr_node_ptr  opnds = expr->variant.typeid_info.type_with_opt_expr;
        if (opnds->next == NULL
            if_microsoft_extensions(&& !expr->is_cli_typeid)) {
          /* The type is known at compile time, so the address of the
             std::type_info object is a compile-time constant. */
          is_constant_addr = TRUE;
          make_typeid_constant(opnds->variant.type_operand.type,
                               /*is_cli_typeid*/FALSE, con);
        }  /* if */
      }
      break;
    case enk_operation:
      { an_expr_node_ptr      op1 = expr->variant.operation.operands;
        an_expr_node_ptr      op2 = op1->next;
        an_expr_operator_kind op = expr->variant.operation.kind;
        a_constant_ptr        conaddr1 = local_constant();
        op1 = skip_parens(op1);
        if (op2 != NULL) op2 = skip_parens(op2);
        switch (op) {
          case eok_dot_field:
          case eok_pm_field:
            /* Field selection, x.y, or pointer-to-member field selection,
               x.*y.  If the left operand is a glvalue with a constant
               address, we can develop an address for the field. */
            if ((is_glvalue_node(op1) &&
                 constant_glvalue_address_full(op1, conaddr1, address_escapes,
                                               options, template_constant)) ||
                (constexpr_enabled &&
                 fold_object_expr(op1, /*want_addr=*/TRUE, conaddr1))) {
              if (op == (an_expr_operator_kind)eok_dot_field) {
                goto handle_field_selection;
              } else {
                goto handle_pm_field_selection;
              }  /* if */
            }  /* if */
            break;
          case eok_points_to_field:
          case eok_pm_points_to_field:
            { a_constant_address_option_set local_options;
              /* Field selection, p->y, or pointer-to-member field
                 selection, p->*y.  If the left operand is a constant
                 address, we can develop an address for the field.  Note
                 that an lvalue member access expression in which the
                 object expression is "this" or a parameter, represented by
                 an enk_param_ref node, cannot be a constant expression. */
              local_options = options | CAO_IS_OBJECT_POINTER;
              if (expr->is_lvalue) {
                local_options |= CAO_FOR_LVALUE_MEMBER_ACCESS;
              }  /* if */
              if (is_pointer_type(op1->type) &&
                  !(expr->is_lvalue &&
                    op1->kind == (an_expr_node_kind)enk_param_ref) &&
                  constant_prvalue_pointer_full(op1, conaddr1, address_escapes,
                                                local_options,
                                                template_constant)) {
                if (op == (an_expr_operator_kind)eok_points_to_field) {
                  goto handle_field_selection;
                } else {
                  goto handle_pm_field_selection;
                }  /* if */
              }  /* if */
            }
            break;
handle_field_selection:
            { a_field_ptr field;
              check_assertion(op2 != NULL && is_field_node(op2));
              field = node_field(op2);
              if (field->is_bit_field &&
                  !is_bit_field_whose_address_can_be_taken(field)) {
                /* You can't take the address of a bit field.  The error is
                   detected somewhere else.  Here, we just conclude we
                   can't produce a constant address. */
              } else {
                /* Not a bit field, or a bit field whose address can be taken
                   because it falls on byte boundaries. */
                if (fold_field_selection(conaddr1, field,
                                         make_pointer_type(expr->type),
                                         con)) {
                  is_constant_addr = TRUE;
                }  /* if */
              }  /* if */
            }
            break;
handle_pm_field_selection:
            { a_constant_ptr pm_constant = local_constant();
              a_constant_ptr op2_con = NULL;
              if (fold_expr(op2, pm_constant)) {
                /* We're in a constexpr function and the operand can be
                   folded to a constant. */
                op2_con = pm_constant;
              } else if (is_constant_node(op2)) {
                /* We're not in a constexpr function, so we can't call
                   fold_expr, but we can fold this expression if the
                   operand is already a constant. */
                op2_con = node_constant(op2);
              }  /* if */
              if (op2_con != NULL &&
                  op2_con->kind == (a_constant_repr_kind)ck_ptr_to_member &&
                  !op2_con->variant.ptr_to_member.is_function_ptr &&
                  op2_con->variant.ptr_to_member.variant.field != NULL) {
                /* The second operand is a constant, so we can fold the
                   access. */
                if (fold_field_selection(
                               conaddr1,
                               op2_con->variant.ptr_to_member.variant.field,
                               make_pointer_type(expr->type), con)) {
                  is_constant_addr = TRUE;
                }  /* if */
              }  /* if */
              release_local_constant(&pm_constant);
            }  /* if */
            break;
          case eok_subscript:
            /* Subscript operation. */
            if (constant_padd_or_subscript(expr, con, address_escapes,
                                           options, template_constant)) {
              is_constant_addr = TRUE;
            }  /* if */
            break;
          case eok_indirect:
            /* "*" operation.  If the operand is a constant address, we can
               use it as the address of the lvalue. */
            if (is_pointer_type(op1->type) &&
                constant_prvalue_pointer_full(op1, con, address_escapes,
                                              options, template_constant)) {
              is_constant_addr = TRUE;
            }  /* if */
            break;
          case eok_ref_indirect:
            /* Reference "*" operation.  If the operand is a constant
               address, we can use it as the address of the lvalue. */
            if (is_reference_type(op1->type) &&
                constant_prvalue_pointer_full(op1, conaddr1, address_escapes,
                                              options, template_constant)) {
              is_constant_addr = TRUE;
              copy_constant(conaddr1, con);
              con->type = make_pointer_type(type_pointed_to(op1->type));
              /* The backing expression of con, if any, is that of op1,
                 designating the reference.  That is not consistent with a
                 ck_address constant, which conceptually represents a
                 pointer, so do not propagate the backing expression. */
              con->expr = NULL;
            }  /* if */
            break;
          case eok_base_class_cast:
            /* A cast of a class glvalue to a base class. */
            check_assertion(is_glvalue_node(op1));
            if (constant_glvalue_address_full(op1, conaddr1, address_escapes,
                                              options, template_constant)) {
              /* The operand has a constant address.  Fold the base class
                 cast into it. */
              if (is_template_dependent_type(expr->type) ||
                  *template_constant) {
                /* The type cast to is dependent or the source is dependent,
                   so add a template param cast. */
                make_template_param_cast_constant(conaddr1, con, expr->type,
                                                  !expr->compiler_generated);
                *template_constant = TRUE;
                is_constant_addr = TRUE;
              } else {
                a_boolean        did_not_fold;
                an_error_code    error_detected;
                a_base_class_ptr bcp;
                check_assertion(is_class_struct_union_type(op1->type) &&
                                is_class_struct_union_type(expr->type));
                bcp = find_base_class_of(op1->type, expr->type);
                check_assertion(bcp != NULL);
                fold_base_class_cast(conaddr1, bcp, expr->type, con,
                                     /*check_cast_access=*/FALSE,
                                     /*check_ambiguity=*/FALSE,
                                     (a_boolean)expr->compiler_generated,
                                     (options & CAO_IS_OBJECT_POINTER) != 0,
                                     /*omit_backing_expr=*/FALSE,
                                     &did_not_fold, &error_position,
                                     &error_detected);
                /* A cast to a virtual base class might not fold to a
                   constant even if the original pointer is a constant. */
                if (error_detected == ec_no_error && !did_not_fold) {
                  is_constant_addr = TRUE;
                }  /* if */
              }  /* if */
            }  /* if */
            break;
          case eok_ref_cast:
          case eok_lvalue_adjust:
            /* These operations are used to adjust the type of a glvalue. */
            if ((!(constexpr_enabled &&
                   expr->variant.operation.is_reinterpret_cast) ||
                 (microsoft_mode ||(gpp_mode && gnu_version >= 40600))) &&
                constant_glvalue_address_full(op1, conaddr1, address_escapes,
                                              options, template_constant)) {
              /* The address of the operand is constant.  Adjust its type
                 and it is also the address of the result glvalue.  (Recent
                 g++ versions allow reinterpret_cast in C++11 mode, as does
                 MSVC, but otherwise it cannot be part of a C++11 constant
                 expression.) */
              a_type_ptr new_type = make_pointer_type(expr->type);
              if (is_template_dependent_type(expr->type) ||
                  *template_constant) {
                /* The type cast to is dependent or the source is dependent,
                   so add a template param cast. */
                make_template_param_cast_constant(conaddr1, con, new_type,
                                                  !expr->compiler_generated);
                *template_constant = TRUE;
              } else {
                copy_constant(conaddr1, con);
                implicit_or_explicit_cast(
                                     con, new_type, expr->compiler_generated);
              }  /* if */
              if (expr->variant.operation.is_reinterpret_cast) {
                con->is_reinterpret_cast = TRUE;
              }  /* if */
              is_constant_addr = TRUE;
            }  /* if */
            break;
          case eok_lvalue:
            /* The address of an eok_lvalue applied to a ck_template_param
               constant is sometimes a constant. */
            if (is_constant_node(op1)) {
              a_constant_ptr acon = node_constant(op1);
              if (acon->kind == (a_constant_repr_kind)ck_template_param) {
                a_constant_ptr rcon = acon;
                if (tpck_is(rcon, tpck_template_ref)) {
                  rcon = rcon->variant.template_param.variant.template_ref.con;
                }  /* if */
                if (tpck_is(rcon, tpck_member)) {
                  /* The address of an lvalue for a tpck_member constant can
                     be represented by a tpck_address constant. */
                  clear_constant(con, (a_constant_repr_kind)ck_template_param);
                  set_template_param_constant_kind(
                                 con,
                                 (a_template_param_constant_kind)tpck_address);
                  con->variant.template_param.variant.constant = acon;
                  /* Note that we don't know whether the address is a pointer
                     or pointer to member. */
                  con->type = type_of_unknown_templ_param_nontype;
                  is_constant_addr = TRUE;
                  *template_constant = TRUE;
                } else if (tpck_is(rcon, tpck_unknown_function)) {
                  /* The address of an lvalue based on an unknown function
                     constant is the constant itself (which represents a
                     prvalue for the "address" of the function). */
                  copy_constant(acon, con);
                  is_constant_addr = TRUE;
                  *template_constant = TRUE;
                }  /* if */
              }  /* if */
            }  /* if */
            break;
          case eok_cli_subscript:  /* A C++/CLI array element is on the managed
                                      heap and therefore does not have a
                                      constant address. */
          default:
            /* Other operators cannot be folded. */
            break;
        }  /* switch */
        release_local_constant(&conaddr1);
      }
      break;
    case enk_param_ref:
      /* A reference to a parameter is similar to a variable with automatic
         storage duration: Its address is not a constant. */
      break;
    case enk_c11_generic:
      /* Look to the selected underlying expression. */
      expr = expr->variant.c11_generic.result;
      goto start_underlying_expression;
    default:
      /* Other expression kinds cannot be folded. */
      break;
  }  /* switch */
have_result:
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cli_or_cx_enabled && is_constant_addr) {
    /* A C++/CLI gc-lvalue should not ever be treated as a constant address,
       as its address might change if the garbage collector moves the
       underlying object. */
    if (is_gc_lvalue_expr(expr)) is_constant_addr = FALSE;
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (template_constant == &local_template_constant && is_constant_addr) {
    /* Handle tagging a template constant locally. */
    if (local_template_constant &&
        con->kind != (a_constant_repr_kind)ck_template_param) {
      /* Make sure there is a ck_template_param on top of a template-dependent
         case. */
      a_constant_ptr local_con = local_constant();
      copy_constant(con, local_con);
      make_template_param_cast_constant(local_con, con, con->type,
                                        /*is_explicit=*/FALSE);
      release_local_constant(&local_con);
    }  /* if */
  }  /* if */
  return is_constant_addr;
}  /* constant_glvalue_address_full */                                


a_boolean constant_glvalue_address(an_expr_node_ptr expr,
                                   a_constant       *con,
                                   a_boolean        address_escapes)
/*
expr is a glvalue expression.  If it has a constant address, put that
address in *con and return TRUE.  Otherwise, return FALSE.  address_escapes is
TRUE if the address might escape from its immediate context and get saved
somewhere (if in doubt, the safe value is TRUE).
*/
{
  a_boolean is_constant_addr = constant_glvalue_address_full(
                                          expr, con, address_escapes, CAO_NONE,
                                          (a_boolean *)NULL);
  return is_constant_addr;
}  /* constant_glvalue_address */


/*
Top of the stack of aggregate constants being initialized.
*/
STATIC_THREAD an_aggr_init_con_elem_ptr
                curr_init_aggr_con;


void push_aggr_init_constant(a_constant_ptr            aggr_con,
                             an_aggr_init_con_elem_ptr aggr_init_con_elem)
/*
Update *aggr_init_con_elem to link to the current top of the stack of
aggregate constants, making it the new top of the stack, and setting it to
point to aggr_con as the associated aggregate constant being initialized.
*/
{
  aggr_init_con_elem->next = curr_init_aggr_con;
  curr_init_aggr_con = aggr_init_con_elem;
  aggr_init_con_elem->constant = aggr_con;
}  /* push_aggr_init_constant */


void pop_aggr_init_constant(an_aggr_init_con_elem_ptr aggr_init_con_elem)
/*
Pop aggr_init_con_elem from the stack of aggregate constants being
initialized.
*/
{
  curr_init_aggr_con = aggr_init_con_elem->next;
}  /* pop_aggr_init_constant */


static a_boolean is_obj_expr_of_stacked_aggr_con(an_expr_node_ptr expr,
                                                 a_constant_ptr   con)
/*
If expr (an enk_param_ref node) is the object expression for a field member
access in an aggregate initializer and the parent class of the field is
represented in the aggregate initialization stack, set *con to be an
address constant designating the appropriate aggregate constant from the
stack and return TRUE.  Otherwise, return FALSE.
*/
{
  a_boolean result = FALSE;

  check_assertion(expr->kind == (an_expr_node_kind)enk_param_ref);
  if (curr_init_aggr_con != NULL && expr->variant.param_ref.param_num == 0 &&
      expr->next != NULL && is_field_node(expr->next)) {
    /* The enk_param_ref is used as the object expression for a field
       member access expression in an aggregate initializer.  Search the
       stack of aggregate constants being initialized for a constant whose
       type is the direct or indirect parent of the field. */
    a_type_ptr                parent_class;
    an_aggr_init_con_elem_ptr init_con;
    for (parent_class = parent_class_or_null(node_field(expr->next));
         !result && parent_class != NULL;
         parent_class = parent_class_or_null(parent_class)) {
      for (init_con = curr_init_aggr_con; !result && init_con != NULL;
           init_con = init_con->next) {
        if (init_con->constant != NULL &&
            init_con->constant->type == parent_class) {
          /* We've found an aggregate containing the field in the member
             access expression.  Create an address constant in *con that
             points to that aggregate and return TRUE. */
          set_constant_address_constant(init_con->constant, con);
          result = TRUE;
        }  /* if */
      }  /* for */
    }  /* for */
  }  /* if */
  return result;
}  /* is_obj_expr_of_stacked_aggr_con */


a_boolean constant_prvalue_pointer_full(
                             an_expr_node_ptr              expr,
                             a_constant                    *con,
                             a_boolean                     address_escapes,
                             a_constant_address_option_set options,
                             a_boolean                     *template_constant)
/*
expr is a prvalue expression of pointer type.  If it has a constant pointer
value, put that value in *con and return TRUE.  Otherwise, return FALSE.
address_escapes is TRUE if the address might escape from its immediate
context and get saved somewhere (if in doubt, the safe value is TRUE).
options contains a set of additional options.  *template_constant is
returned TRUE if the constant is template-dependent.  If template_constant
is NULL, a template-dependent constant is labeled as such at this
level.  Passing it in as non-NULL is a signal that the caller would
prefer to handle that higher up.
*/
{
  a_boolean is_constant_ptr = FALSE;
  a_boolean local_template_constant;

  if (template_constant == NULL) {
    template_constant = &local_template_constant;
  }  /* if */
  *template_constant = FALSE;
  expr = skip_parens(expr);
  check_assertion(!is_glvalue_node(expr) &&
                  ((is_pointer_type(expr->type) ||
                    is_reference_type(expr->type) ||
                    is_template_param_type(expr->type) ||
                    is_error_type(expr->type)) ||
                   is_error_node(expr)));
#if DO_IL_LOWERING
  if (il_lowering_underway) {
    /* Don't attempt expression folding during lowering. */
  } else
#endif /* DO_IL_LOWERING */
  /* Do not insert code here. */
  if (fold_expr(expr, con)) {
    /* The expression could be folded to a constant. */
    is_constant_ptr = TRUE;
    goto have_result;
  }  /* if */
  switch (expr->kind) {
    case enk_error:
      /* Assume an error expression could have been a prvalue constant
         pointer. */
      is_constant_ptr = TRUE;
      set_error_constant(con);
      break;
    case enk_variable:
      /* An rvalue for a variable can only be a pointer-typed constant in
         C++11. */
      if (constexpr_enabled) {
        a_constant_ptr var_con = var_constant_value(node_variable(expr));
        if (var_con != NULL &&
            !address_con_is_unknown_object(var_con)) {
          copy_constant(var_con, con);
          is_constant_ptr = TRUE;
        }  /* if */
      }  /* if */
      break;
    case enk_param_ref:
      /* An enk_param_ref can be used in the initializer expression of a
         class member to refer to the value of an already-initialized
         member.  In that case, the enk_param_ref is encoded as a "this"
         pointer (param_num == 0) and designates the aggregate constant
         currently being initialized.  If the member access expression is
         in an lvalue context (e.g., to initialize a reference), this is
         not a constant address.  In a prvalue context, where only the
         value from the aggregate is needed, the result should be an
         address constant for that aggregate constant.  Otherwise, an
         enk_param_ref designates a function parameter and thus cannot be a
         pointer constant. */
      if (constexpr_enabled && !(options & CAO_FOR_LVALUE_MEMBER_ACCESS) &&
          is_obj_expr_of_stacked_aggr_con(expr, con)) {
        is_constant_ptr = TRUE;
      }  /* if */
      break;
    case enk_routine:
      {
        /* An rvalue for a function.  That's a function pointer, which can
           be rendered as a constant.  However, don't fold pointers to
           consteval functions here because it makes it harder to track
           invalid uses of such functions. */
        a_routine_ptr  rp = expr->variant.routine.ptr;
        if (!rp->is_consteval) {
          make_constant_routine_address(expr->variant.routine.ptr, con,
                                        address_escapes, template_constant);
          is_constant_ptr = TRUE;
        }  /* if */
      }
      break;
    case enk_constant:
      /* A constant with pointer type is a constant pointer value. */
      copy_constant(node_constant(expr), con);
      is_constant_ptr = TRUE;
      break;
    case enk_operation:
      { an_expr_node_ptr op1 = expr->variant.operation.operands;
        a_constant_ptr   conaddr1 = local_constant();
        op1 = skip_parens(op1);
        switch (expr->variant.operation.kind) {
          case eok_ref_indirect:
            if (is_constant_node(op1) &&
                node_constant(op1)->kind == (a_constant_repr_kind)ck_address &&
                node_constant(op1)->variant.address.kind ==
                                           (an_address_base_kind)abk_routine) {
              /* The operand is a constant reference to a function, so the
                 resulting address is that constant. */
              copy_constant(node_constant(op1), con);
              is_constant_ptr = TRUE;
            }  /* if */
            break;
          case eok_address_of:
            /* "&" operation.  If the operand is an lvalue with a constant
               address, the result is a constant pointer. */
            if (constant_glvalue_address_full(op1, con,  address_escapes,
                                              options, template_constant)) {
              is_constant_ptr = TRUE;
            }  /* if */
            break;
          case eok_array_to_pointer:
            /* Array-to-pointer decay operation.  If the operand is a glvalue
               array with a constant address, the result is a constant
               pointer. */
            if (is_glvalue_node(op1) &&
                constant_glvalue_address_full(op1, con,  address_escapes,
                                              options, template_constant) &&
                is_pointer_type(con->type)) {
              a_type_ptr atype = type_pointed_to(con->type);
              if (is_array_type(atype)) {
                con->type = type_after_array_to_pointer_transformation(atype);
                con->implicit_cast = TRUE;
                decay_subobject_path(con);
                is_constant_ptr = TRUE;
              }  /* if */
            }  /* if */
            break;
          case eok_padd:
          case eok_psubtract:
            /* p + i or i + p, or p - i.  These are constant if i is constant
               and p is or can be made constant. */
            if (constant_padd_or_subscript(expr, con, address_escapes,
                                           options, template_constant)) {
              is_constant_ptr = TRUE;
            }  /* if */
            break;
          case eok_cast:
            /* Pointer cast that passes through an address. */
            if (is_pointer_type(expr->type) &&
                is_pointer_type(op1->type)) {
              /* Allow only an identity cast or a cv-qualification change. */
              a_type_ptr target_type =
                                  f_skip_typerefs(type_pointed_to(expr->type));
              a_type_ptr source_type =
                                  f_skip_typerefs(type_pointed_to(op1->type));
              if (identical_types(target_type, source_type) ||
                  /* Also allow a cast to char *. */
                  is_character_type(target_type) ||
                  /* Also allow a cast to void * in gcc mode. */
                  (gcc_mode && is_void_type(target_type))) {
                goto cast_case;
              }  /* if */
            }  /* if */
            break;
          case eok_base_class_cast:
            /* Cast of a pointer to a base class pointer. */
            /* Casts of a class lvalue or rvalue shouldn't get here. */
cast_case:
            if (constant_prvalue_pointer_full(op1, conaddr1, address_escapes,
                                              options, template_constant) &&
                !*template_constant) {
              an_error_code     err_code;
              an_error_severity err_severity;
              a_boolean         did_not_fold;
              clear_constant(con, (a_constant_repr_kind)ck_error);
              con->type = expr->type;
              /* Access checking is not done because it was already
                 done when the expression was put together. */
              conv_pointer_to_whatever(conaddr1, con,
                                       /*check_cast_access=*/FALSE,
                                       /*check_ambiguity=*/FALSE,
                                       (a_boolean)expr->compiler_generated,
                                       /*fold_constant_addr_exprs=*/TRUE,
                                       (a_boolean)expr->variant.operation.
                                                           is_reinterpret_cast,
                                       (options & CAO_IS_OBJECT_POINTER) != 0,
                                       &did_not_fold,
                                       &error_position,
                                       &err_code, &err_severity);
              /* A cast to a virtual base class might not fold to a
                 constant even if the original pointer is a constant. */
              if (err_code == ec_no_error && !did_not_fold) {
                is_constant_ptr = TRUE;
              }  /* if */
            }  /* if */
            break;
          default:
            /* Other operators cannot be folded. */
            break;
        }  /* switch */
        release_local_constant(&conaddr1);
      }
      break;
    default:
      /* Other expression kinds cannot be folded. */
      break;
  }  /* switch */
have_result:
  if (template_constant == &local_template_constant && is_constant_ptr) {
    /* Handle tagging a template constant locally. */
    if (local_template_constant &&
        con->kind != (a_constant_repr_kind)ck_template_param) {
      /* Make sure there is a ck_template_param on top of a template-dependent
         case. */
      a_constant_ptr local_con = local_constant();
      copy_constant(con, local_con);
      make_template_param_cast_constant(local_con, con, con->type,
                                        /*is_explicit=*/FALSE);
      release_local_constant(&local_con);
    }  /* if */
  }  /* if */
  /* Check to make sure a constant result is actually a pointer (and not,
     for example, a ck_dynamic_init that was not folded). */
  if (is_constant_ptr && con->kind != (a_constant_repr_kind)ck_address &&
      con->kind != (a_constant_repr_kind)ck_integer &&
      con->kind != (a_constant_repr_kind)ck_template_param) {
    is_constant_ptr = FALSE;
  }  /* if */
  return is_constant_ptr;
}  /* constant_prvalue_pointer_full */


a_boolean constant_prvalue_pointer(an_expr_node_ptr expr,
                                   a_constant       *con,
                                   a_boolean        address_escapes)
/*
expr is a prvalue expression of pointer type.  If it has a constant pointer
value, put that value in *con and return TRUE.  Otherwise, return FALSE.
address_escapes is TRUE if the address might escape from its immediate context
and get saved somewhere (if in doubt, the safe value is TRUE).
*/
{
  a_boolean is_constant_ptr = constant_prvalue_pointer_full(
                                                 expr, con, address_escapes,
                                                 CAO_NONE, (a_boolean *)NULL);
  return is_constant_ptr;
}  /* constant_prvalue_pointer */


static a_boolean identical_pointer_types_ignoring_qualifiers(a_type_ptr type1,
                                                             a_type_ptr type2)
/*
Return TRUE if the two given types are pointer types whose underlying types
are the same ignoring cv-qualifiers.
*/
{
  a_boolean result = FALSE;

  if (is_pointer_type(type1) && is_pointer_type(type2)) {
    a_type_ptr under1 = type_pointed_to(type1);
    a_type_ptr under2 = type_pointed_to(type2);
    result = identical_types_ignoring_qualifiers(under1, under2);
  }  /* if */
  return result;
}  /* identical_pointer_types_ignoring_qualifiers */


static a_boolean pointer_to_string_literal(a_constant  *con,
                                           a_constant  **scon,
                                           a_boolean   allow_interior)
/*
Shared implementation of constant_is_pointer_to_string_literal and
constant_is_pointer_into_string_literal: Return TRUE if con is a pointer to the
first character of a string literal or, when allow_interior is TRUE, to any of
its characters.  *scon is set as described for those routines.
*/
{
  a_boolean result = FALSE;

  if (scon != NULL) *scon = NULL;
  if (constant_is(con, ck_address) &&
      address_base_is(con, abk_constant) &&
      con->implicit_cast) {
    a_constant_ptr    acon = con->variant.address.variant.constant;
    a_targ_ptrdiff_t  offset = con->variant.address.offset;
    if (constant_is(acon, ck_string) &&
        (offset == 0 ||
         (allow_interior && offset > 0 &&
          offset < (a_targ_ptrdiff_t)acon->variant.string.length))) {
      /* We have a ck_address constant pointing to a ck_string constant.
         Make sure the ck_address type is the type of the string after
         array-to-pointer decay. */
      a_type_ptr ts = type_after_array_to_pointer_transformation(acon->type);
      if (identical_pointer_types_ignoring_qualifiers(con->type, ts)) {
        result = TRUE;
        if (scon != NULL) *scon = acon;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* pointer_to_string_literal */


a_boolean constant_is_pointer_to_string_literal(a_constant *con,
                                                a_constant **scon)
/*
Return TRUE if the indicated constant is a pointer to a string literal,
i.e., a string literal that has decayed (or been cast to) a pointer to
the underlying character type (possibly with different cv-qualifiers,
e.g., a const string could be cast to plain char *).  The string need not
be a narrow string literal.  If scon is non-NULL, *scon is set to point
to the string literal constant if there is one.
*/
{
  return pointer_to_string_literal(con, scon, /*allow_interior=*/FALSE);
}  /* constant_is_pointer_to_string_literal */


a_boolean constant_is_pointer_into_string_literal(a_constant *con,
                                                  a_constant **scon)
/*
Like constant_is_pointer_to_string_literal, except that the pointer may
designate any character of the string literal rather than only the first one.
A pointer one past the last character does not qualify, since it does not
designate a character.
*/
{
  return pointer_to_string_literal(con, scon, /*allow_interior=*/TRUE);
}  /* constant_is_pointer_into_string_literal */


a_boolean expr_is_pointer_to_string_literal(an_expr_node_ptr expr,
                                            a_constant       **scon)
/*
Return TRUE if the indicated expression is a pointer to a string literal,
i.e., a string literal that has decayed (or been cast to) a pointer to
the underlying character type (possibly with different cv-qualifiers,
e.g., a const string could be cast to plain char *).  The string need not
be a narrow string literal.  If scon is non-NULL, *scon is set to point
to the string literal constant if there is one.
*/
{
  a_boolean result = FALSE;

  if (scon != NULL) *scon = NULL;
  expr = skip_parens(expr);
  if (is_constant_node(expr)) {
    if (constant_is_pointer_to_string_literal(node_constant(expr), scon)) {
      /* A constant for the address of a string literal, decayed to
         a pointer to the underlying type. */
      result = TRUE;
    }  /* if */
  } else if (is_operation_node(expr)) {
    an_expr_node_ptr cast_expr = NULL;
    if (node_operator_is(expr, eok_cast)) {
      /* Remember a cast on top of the expression for later testing. */
      cast_expr = expr;
      expr = skip_parens(expr->variant.operation.operands);
    }  /* if */
    if (is_operation_node(expr) &&
        node_operator_is(expr, eok_array_to_pointer)) {
      an_expr_node_ptr op1 = skip_parens(expr->variant.operation.operands);
      if (op1->is_lvalue && is_constant_node(op1) &&
          node_constant_is(op1, ck_string)) {
        /* An expression for a string literal, decayed to a pointer to
           the underlying type. */
        result = TRUE;
        if (cast_expr != NULL) {
          /* For the cast case, make sure the cast type is the proper decayed
             type. */
          a_type_ptr decayed_type =
                         type_after_array_to_pointer_transformation(op1->type);
          if (!identical_pointer_types_ignoring_qualifiers(decayed_type,
                                                           cast_expr->type)) {
            /* The cast is to the wrong type. */
            result = FALSE;
          }  /* if */
        }  /* if */
        if (result && scon != NULL) *scon = node_constant(op1);
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* expr_is_pointer_to_string_literal */


static a_boolean add_offset_of_accessed_member(an_expr_node_ptr   expr,
                                               a_constant_ptr     offset,
                                               a_source_position  *pos)
/*
expr represents the element access operation of a builtin offsetof operator
(or a part thereof in multilevel cases).  Add to *offset the offset implied
by this access operation.  Multilevel cases (e.g., "offsetof(T, x[3].y)") are
handled through recursion.  Error cases can occur when accessing a member of a
virtual base, or when dealing with subscripts that are too large (overflow).
In such cases, return FALSE and issue a diagnostic at the given position (if
it is non-NULL).  Otherwise, return TRUE.
*/
{
  a_boolean         okay = TRUE, ovflo = FALSE;
  an_expr_node_ptr  args;
  an_integer_value  int_val;

  /* eok_parens shouldn't appear in these generated operations. */
  if (is_constant_node(expr)) {
    /* Presumably the null constant that is the root of the tree. */
    check_assertion(is_false_constant(node_constant(expr)));
    goto done;
  } else {
    check_assertion(is_operation_node(expr));
    /* Do a recursive call to process the bottom of the tree first. */
    args = expr->variant.operation.operands;
    okay = add_offset_of_accessed_member(args, offset, pos);
  }  /* if */
  switch (expr->variant.operation.kind) {
    case eok_dot_field:
    case eok_points_to_field:
      check_assertion(is_field_node(args->next));
      accum_field_offset(offset, node_field(args->next),
                         (a_subobject_path_ptr*)NULL, &ovflo);
      break;
    case eok_subscript:
      { a_type_ptr       elem_type = type_pointed_to(args->type);
        an_expr_node_ptr arg2 = skip_parens(args->next);
        a_boolean        did_not_fold;
        check_assertion(is_constant_node(arg2));
        /* Note that while eok_subscript in general allows operands in
           either order, in offsetof the subscript is always the second
           operand. */
        accum_array_offset(offset, /*offset_is_signed=*/FALSE,
                           /*subtract=*/FALSE, node_constant(arg2),
                           skip_typerefs(elem_type)->size,
                           /*no_ovflo_on_unsigned_add=*/FALSE, &ovflo,
                           &did_not_fold);
        if (did_not_fold) {
          okay = FALSE;
          if (pos != NULL) pos_error(ec_nonconstant_offsetof, pos);
        }  /* if */
      }
      break;
    case eok_base_class_cast:
      { a_type_ptr        dtype = args->type;
        a_type_ptr        btype = expr->type;
        a_base_class_ptr  bcp;
        /* Look for the base class to which the cast refers, and update
           *offset accordingly.  Since the field was unambiguous, the
           base class should be unambiguous too. */
        if (is_pointer_type(dtype)) {
          dtype = type_pointed_to(dtype);
          btype = type_pointed_to(btype);
        }  /* if */
        bcp = find_base_class_of(dtype, btype);
        check_assertion(bcp != NULL && !bcp->ambiguous);
        if (bcp->is_virtual) {
          /* We don't currently allow the offset of a member of a virtual
             base class to be taken (the GNU compiler produces a somewhat
             strange value). */
          okay = FALSE;
          if (pos != NULL) {
            pos_error(ec_offsetof_virtual_base_member, pos);
          }  /* if */
        } else {
          set_unsigned_integer_value(&int_val, bcp->offset);
          add_integer_values(&offset->variant.integer_value, &int_val,
                             /*is_signed=*/FALSE, &ovflo);
        }  /* if */
      }
      break;
    case eok_array_to_pointer:
    case eok_indirect:
    case eok_address_of:
      /* Just continue for these. */
      break;
    default:
      unexpected_condition();
  }  /* switch */
  if (okay && ovflo) {
    okay = FALSE;
    pos_error(ec_integer_overflow_internal, pos);
  }  /* if */
done:
  return okay;
}  /* add_offset_of_accessed_member */


static a_boolean is_template_dependent_offsetof_member(
                                            an_expr_node_ptr  expr,
                                            a_boolean         *not_a_constant)
/*
The given expression is the second operand of a bok_offsetof operation.
Return TRUE if that expression contains a template-dependent operation.
If it contains a non-constant subscript operation, set *not_a_constant to TRUE.
*/
{
  a_boolean  template_dependent = FALSE;

  while (!is_constant_node(expr)) {
    an_expr_node_ptr  args;
    check_assertion(is_operation_node(expr));
    args = expr->variant.operation.operands;
    if (node_operator_is(expr, eok_subscript)) {
      if (!is_constant_node(args->next)) {
        *not_a_constant = TRUE;
      } else if (node_constant_is(args->next, ck_template_param)) {
        template_dependent = TRUE;
      }  /* if */
    } else if (node_operator_is(expr, eok_dot_static)) {
      /* A dot-static operation comes up for something like
           __builtin_offsetof(A, T::m)
         in the prototype instantiation where we can't tell what kind of thing
         the lookup will find. */
      check_assertion(is_constant_node(args->next) &&
                      node_constant_is(args->next, ck_template_param));
      template_dependent = TRUE;
    }  /* if */
    expr = args;
  }  /* while */
  return template_dependent;
}  /* is_template_dependent_offsetof_member */


static void fold_offsetof(an_expr_node_ptr   expr,
                          a_constant_ptr     constant,
                          a_boolean          maintain_expression,
                          a_source_position  *pos,
                          a_boolean          *not_a_constant)
/*
expr is an enk_builtin_operation node for a __builtin_offsetof operation
(currently only accepted in some GNU modes).  If any of the operands is
template-dependent, store a ck_template_param constant in *constant (the
constant will be of the tpck_expression variant and will point to the given
expression).  Otherwise, if the second operand contains a nonconstant
subscript, set *not_a_constant to TRUE and leave *constant unchanged.  In all
other cases, store the integer value of the offset being represented in
*constant (if maintain_expression is TRUE, the backing expression for the
returned constant will be set as well).  If a constant is returned through
*constant, *not_a_constant is set to FALSE.  If pos is non-NULL, diagnostics
are issued at the position it indicates.
*/
{
  an_expr_node_ptr  arg1 = expr->variant.builtin_operation.operands,
                    arg2 = arg1->next;

  /* Start with the assumption that a constant will be produced. */
  *not_a_constant = FALSE;
  /* eok_parens shouldn't appear here, since the construct is generated. */
  check_assertion(arg1 != NULL && arg2 != NULL && arg2->next == NULL &&
                  is_type_node(arg1));
  if (is_template_dependent_type(type_operand_type(arg1)) ||
      is_template_dependent_offsetof_member(arg2, not_a_constant)) {
    /* The template-dependent case. */
    clear_constant(constant, (a_constant_repr_kind)ck_template_param);
    set_template_param_constant_kind(
                   constant, (a_template_param_constant_kind)tpck_expression);
    constant->variant.template_param.variant.expr = expr;
    constant->type = expr->type;
  } else if (!*not_a_constant) {
    /* The foldable case. */
    set_unsigned_integer_constant(constant, (a_host_large_unsigned)0,
                                  targ_size_t_int_kind);
    if (add_offset_of_accessed_member(arg2, constant, pos)) {
      arg1->type_definition_needed = TRUE;
    } else {
      clear_constant(constant, (a_constant_repr_kind)ck_error);
    }  /* if */
    if (maintain_expression) constant->expr = expr;
    constant->type = expr->type;
  } else if (gpp_mode && gnu_version < 40600) {
    /* Early versions of GNU C++ (as opposed to GNU C) do not allow
       nonconstant __builtin_offsetof operations. */
    if (pos != NULL) {
      pos_diagnostic(es_discretionary_error, ec_nonconstant_offsetof, pos);
    }  /* if */
  }  /* if */
}  /* fold_offsetof */


static void fold_is_base_of(an_expr_node_ptr   expr,
                            a_constant_ptr     constant,
                            a_boolean          maintain_expression,
                            a_boolean          is_virtual_base_of)
/*
expr is an enk_builtin_operation node for an __is_base_of or
__builtin_is_virtual_base_of operation.  If the operand types are nondependent,
store a boolean constant in *constant.  The boolean constant will have value
"true" if the operand types are (possibly qualified) class types the first of
which is a base class (or a virtual base class if is_virtual_base_of is TRUE)
of the second one; otherwise, the constant will have value "false".  If either
of the operand types is dependent, store a ck_template_param constant in
*constant.  The constant will be of the tpck_expression variant and will point
to the given expression.  If maintain_expression is TRUE, the backing
expression for the returned constant will be set as well.
*/
{
  an_expr_node_ptr  arg1 = expr->variant.builtin_operation.operands,
                    arg2 = arg1->next;
  a_type_ptr        type1, type2;

  /* eok_parens shouldn't appear here, since the construct is generated. */
  check_assertion(arg1 != NULL && arg2 != NULL && arg2->next == NULL &&
                  arg1->kind == (an_expr_node_kind)enk_type_operand &&
                  arg2->kind == (an_expr_node_kind)enk_type_operand);
  type1 = arg1->variant.type_operand.type;
  type2 = arg2->variant.type_operand.type;
  if (is_template_dependent_type(type1) ||
      is_template_dependent_type(type2)) {
    make_template_param_expr_constant(expr, constant);
  } else {
    a_boolean  result = FALSE;
    type1 = skip_typerefs(type1);
    type2 = skip_typerefs(type2);
    if (type1->kind != (a_type_kind)tk_union &&
        is_immediate_class_type(type1) && is_immediate_class_type(type2)) {
      if (is_virtual_base_of) {
        complete_class_type_is_needed(type2);
        if (!is_incomplete_type(type1) && !is_incomplete_type(type2)) {
          /* See if the base class appears on the base class list for the
             derived type. */
          for (a_base_class_ptr bcp = base_classes_of(type2);
               !result && bcp != NULL;
               bcp = bcp->next) {
            if (same_entities(bcp->type, type1)) {
              if (bcp->is_virtual) {
                result = TRUE;
              } else if (gnu_version_is(any_version)) {
                /* GCC also considers non-virtual base classes of virtual base
                   classes to be virtual base classes. */
                a_base_class_derivation_ptr  bcdp = bcp->derivation;
                a_derivation_step_ptr        tail = bcdp->path_tail;
                for (a_derivation_step_ptr  dsp = bcdp->path;
                     !result && dsp != tail->next;
                     dsp = dsp->next) {
                  if (dsp->base_class->is_virtual) result = TRUE;
                }  /* for */
              }  /* if */
            }  /* if */
          }  /* for */
        }  /* if */
      } else {
        result = (same_entities(type2, type1) ||
                  find_base_class_of(type2, type1) != NULL);
      }  /* if */
    }  /* if */
    arg1->type_definition_needed = TRUE;
    arg2->type_definition_needed = TRUE;
    clear_constant(constant, (a_constant_repr_kind)ck_integer);
    set_integer_value(&constant->variant.integer_value,
                      (a_host_large_integer)result);
    if (maintain_expression) constant->expr = expr;
  }  /* if */
  constant->type = expr->type;
}  /* fold_is_base_of */


static void fold_is_convertible_to(an_expr_node_ptr   expr,
                                   a_constant_ptr     constant,
                                   a_boolean          maintain_expression)
/*
expr is an enk_builtin_operation node for an __is_convertible[_to] or
__is_nothrow_convertible operation, which implement the C++ standard library
is_convertible/is_nothrow_convertible type relationship predicates (see
[lib.meta.rel]).  Store a boolean constant in *constant whose value is "true"
if the first operand type is "implicitly convertible to" the second operand
type.  If either of the operand types is dependent, store a ck_template_param
constant in *constant.  The constant will be of the tpck_expression variant and
will point to the given expression.  If maintain_expression is TRUE, the
backing expression for the returned constant will be set as well.

(Note: __is_convertible can also be spelled __is_convertible_to.)
*/
{
  an_expr_node_ptr  arg1 = expr->variant.builtin_operation.operands,
                    arg2 = arg1->next;
  a_type_ptr        type1, type2;

  /* eok_parens shouldn't appear here, since the construct is generated. */
  check_assertion(arg1 != NULL && arg2 != NULL && arg2->next == NULL &&
                  arg1->kind == (an_expr_node_kind)enk_type_operand &&
                  arg2->kind == (an_expr_node_kind)enk_type_operand);
  type1 = arg1->variant.type_operand.type;
  type2 = arg2->variant.type_operand.type;
  if (is_template_dependent_type(type1) ||
      is_template_dependent_type(type2)) {
    clear_constant(constant, (a_constant_repr_kind)ck_template_param);
    set_template_param_constant_kind(
                   constant, (a_template_param_constant_kind)tpck_expression);
    constant->variant.template_param.variant.expr = expr;
  } else {
    a_boolean  result;
    if (microsoft_mode) {
      a_boolean  force_array_to_reference = FALSE;
      if (microsoft_version < 1800) {
        a_boolean   is_rvalue_ref1 = is_rvalue_reference_type(type1), 
                    is_rvalue_ref2 = is_rvalue_reference_type(type2);
        /* MSVC 11 and earlier appear to treat rvalue reference types a little
           strangely.  If both the source and destination type are rvalue
           reference types, the result is always false.  Otherwise, an rvalue
           reference on the source type appears to be treated like an lvalue
           reference, and an rvalue reference on the destination type is
           treated as the underlying type without a reference. */
        if (is_rvalue_ref1 && is_rvalue_ref2) {
          result = FALSE;
          goto result_known;
        } else if (is_rvalue_ref1) {
          type1 = make_reference_type(type_pointed_to(type1));
        } else if (is_rvalue_ref2 && !is_reference_type(type1)) {
          type2 = type_pointed_to(type2);
        }  /* if */
      }  /* if */
      if (microsoft_version < 1900 || (microsoft_version == 1900 &&
                                       microsoft_build_number <= 22129)) {
        /* Prior to a recent build of MSVC "19.00", Microsoft implicitly added
           an "lvalue reference" layer on top of array types. */
        force_array_to_reference = TRUE;
      }  /* if */
      if (is_function_type(type1) ||
          (force_array_to_reference && is_array_type(type1))) {
        /* Microsoft appears to treat conversions from functions and arrays as
           conversions from lvalue references to those types. */
        type1 = make_reference_type(type1);
      } else if (!force_array_to_reference && is_array_type(type1) &&
                 is_pointer_type(type2)) {
        /* __is_convertible_to(int[], int*) is true. */
        type1 = type_after_array_to_pointer_transformation(type1);
      }  /* if */
      if (force_array_to_reference && is_array_type(type2)) {
        /* Microsoft appears to treat a conversion to an array type as a
           conversion to an lvalue reference to that array type. */
        type2 = make_reference_type(type2);
      } else if (is_void_type(type2) && microsoft_version < 1800) {
        /* MSVC++ prior to version 12 disallowed even void->void conversions
           (which compute_is_convertible allows). */
        result = FALSE;
        goto result_known;
      }  /* if */       
    }  /* if */
    result = compute_is_convertible(type1, type2,
                                    expr->variant.builtin_operation.kind);
result_known:
    arg1->type_definition_needed = TRUE;
    arg2->type_definition_needed = TRUE;
    clear_constant(constant, (a_constant_repr_kind)ck_integer);
    set_integer_value(&constant->variant.integer_value,
                      (a_host_large_integer)result);
    if (maintain_expression) constant->expr = expr;
  }  /* if */
  constant->type = expr->type;
}  /* fold_is_convertible_to */


static void fold_reference_binds_to_temporary(
                                       an_expr_node_ptr   expr,
                                       a_constant_ptr     constant,
                                       a_boolean          maintain_expression)
/*
expr is an enk_builtin_operation node for a __reference_binds_to_temporary,
__reference_constructs_from_temporary, or __reference_converts_from_temporary
operation.  If the operand types are nondependent, store a boolean constant in
*constant.  The boolean constant will have value "true" if the first operand
is a reference type and binding a value of the second type to that reference
is valid and causes the reference to be bound to a temporary.  If either of
the operand types is dependent, store a ck_template_param constant in
*constant.  The constant will be of the tpck_expression variant and will point
to the given expression.  If maintain_expression is TRUE, the backing
expression for the returned constant will be set as well.
*/
{
  an_expr_node_ptr  arg1 = expr->variant.builtin_operation.operands,
                    arg2 = arg1->next;
  a_type_ptr        type1, type2;

  /* eok_parens shouldn't appear here, since the construct is generated. */
  check_assertion(arg1 != NULL && arg2 != NULL && arg2->next == NULL &&
                  arg1->kind == (an_expr_node_kind)enk_type_operand &&
                  arg2->kind == (an_expr_node_kind)enk_type_operand);
  type1 = arg1->variant.type_operand.type;
  type2 = arg2->variant.type_operand.type;
  if (is_template_dependent_type(type1) ||
      is_template_dependent_type(type2)) {
    make_template_param_expr_constant(expr, constant);
  } else {
    a_boolean  result = compute_reference_binds_to_temporary(
                          type1, type2, expr->variant.builtin_operation.kind);
    clear_constant(constant, (a_constant_repr_kind)ck_integer);
    set_integer_value(&constant->variant.integer_value,
                      (a_host_large_integer)result);
    if (maintain_expression) constant->expr = expr;
  }  /* if */
  constant->type = expr->type;
}  /* fold_reference_binds_to_temporary */


static void fold_is_invocable(an_expr_node_ptr   expr,
                              a_constant_ptr     constant,
                              a_boolean          maintain_expression)
/*
expr is an enk_builtin_operation node for an __is_invocable or
__is_nothrow_invocable operation.  Store a boolean constant in *constant
corresponding to the evaluation of expr.

If any of the operand types is dependent, store a ck_template_param constant
in *constant.  The constant will be of the tpck_expression variant and will
point to the given expression.  If maintain_expression is TRUE, the backing
expression for the returned constant will be set as well.
*/
{
  a_builtin_operation_kind
                    kind = expr->variant.builtin_operation.kind;
  an_expr_node_ptr  arg1 = expr->variant.builtin_operation.operands,
                    argn;
  a_type_ptr        type1, typen;
  a_boolean         dependent = FALSE, result;

  /* eok_parens shouldn't appear here, since the construct is generated. */
  check_assertion(arg1 != NULL && is_type_node(arg1));
  type1 = type_operand_type(arg1);
  if (is_template_dependent_type(type1)) {
    dependent = TRUE;
  } else {
    for (argn = arg1->next; argn != NULL; argn = argn->next) {
      check_assertion(is_type_node(argn));
      typen = type_operand_type(argn);
      if (is_template_dependent_type(typen)) {
        dependent = TRUE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  if (dependent) {
    /* One or more of the types is dependent, so the result is still
       unknown. */
    clear_constant(constant, (a_constant_repr_kind)ck_template_param);
    set_template_param_constant_kind(
                   constant, (a_template_param_constant_kind)tpck_expression);
    constant->variant.template_param.variant.expr = expr;
  } else {
    result = compute_is_invocable(kind, type1, expr);
    arg1->type_definition_needed = TRUE;
    for (argn = arg1->next; argn != NULL; argn = argn->next) {
      check_assertion(is_type_node(argn));
      argn->type_definition_needed = TRUE;
    }  /* for */
    clear_constant(constant, ck_integer);
    set_integer_value(&constant->variant.integer_value,
                      (a_host_large_integer)result);
    if (maintain_expression) constant->expr = expr;
  }  /* if */
  constant->type = expr->type;
}  /* fold_is_invocable */


static void fold_is_constructible(an_expr_node_ptr   expr,
                                  a_constant_ptr     constant,
                                  a_boolean          maintain_expression)
/*
expr is an enk_builtin_operation node for an __is_constructible,
__is_nothrow_constructible, or __is_trivially_constructible operation.  Store
a boolean constant in *constant whose value is "true" if the following
variable definition would be well-formed for some invented variable t:
      T t(create<Args>()...);
with
      template<class T>
        typename add_rvalue_reference<T>::type create();
If the built-in operation kind is bok_is_nothrow_constructible, the definition
must be known not to throw any exceptions.  If the built-in operation kind is
bok_is_trivially_constructible, the definition must be known not to involve a
non-trivial (copy) construction.  If any of the operand types is dependent,
store a ck_template_param constant in *constant.  The constant will be of the
tpck_expression variant and will point to the given expression.
If maintain_expression is TRUE, the backing expression for the returned
constant will be set as well.
*/
{
  a_builtin_operation_kind
                    kind = expr->variant.builtin_operation.kind;
  an_expr_node_ptr  arg1 = expr->variant.builtin_operation.operands,
                    argn;
  a_type_ptr        type1, typen;
  a_boolean         dependent = FALSE, result;

  /* eok_parens shouldn't appear here, since the construct is generated. */
  check_assertion(arg1 != NULL && is_type_node(arg1));
  type1 = type_operand_type(arg1);
  if (is_template_dependent_type(type1)) {
    dependent = TRUE;
  } else {
    for (argn = arg1->next; argn != NULL; argn = argn->next) {
      check_assertion(is_type_node(argn));
      typen = type_operand_type(argn);
      if (is_template_dependent_type(typen)) {
        dependent = TRUE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  if (dependent) {
    /* One or more of the types is dependent, so the result is still
       unknown. */
    clear_constant(constant, (a_constant_repr_kind)ck_template_param);
    set_template_param_constant_kind(
                   constant, (a_template_param_constant_kind)tpck_expression);
    constant->variant.template_param.variant.expr = expr;
  } else {
    result = compute_is_constructible(kind, type1, expr);
    if (result && !ms_version_is(<=1910)) {
      a_builtin_operation_kind  dtor_kind;
      if (kind == (a_builtin_operation_kind)bok_is_trivially_constructible) {
        dtor_kind = (a_builtin_operation_kind)bok_is_trivially_destructible;
      } else if (kind ==
                     (a_builtin_operation_kind)bok_is_nothrow_constructible) {
        dtor_kind = (a_builtin_operation_kind)bok_is_nothrow_destructible;
      } else {
        dtor_kind = (a_builtin_operation_kind)bok_is_destructible;
      }  /* if */
      result = compute_is_destructible(dtor_kind, type1);
    }  /* if */
    arg1->type_definition_needed = TRUE;
    for (argn = arg1->next; argn != NULL; argn = argn->next) {
      check_assertion(is_type_node(argn));
      argn->type_definition_needed = TRUE;
    }  /* for */
    clear_constant(constant, (a_constant_repr_kind)ck_integer);
    set_integer_value(&constant->variant.integer_value,
                      (a_host_large_integer)result);
    if (maintain_expression) constant->expr = expr;
  }  /* if */
  constant->type = expr->type;
}  /* fold_is_constructible */


static void fold_is_destructible(an_expr_node_ptr   expr,
                                 a_constant_ptr     constant,
                                 a_boolean          maintain_expression)
/*
expr is an enk_builtin_operation node for an __is_destructible,
__is_nothrow_destructible, or __is_trivially_assignable operation.  Store a
boolean constant in *constant whose value is "true" if
      declval<U&>().~U()
is well-formed, where U is the argument type minus any top-level "array of"
layers and the declval template is declared as follows:
      template<class T>
        typename add_rvalue_reference<T>::type declval() noexcept;
In the case of __is_nothrow_destructible, a true result also means it is known
that no exception is thrown for the destruction, and in the case of
__is_trivially_destructible the destructor is trivial.  If any of the operand
types is dependent, store a ck_template_param constant in *constant.  The
constant will be of the tpck_expression variant and will point to the given
expression.  If maintain_expression is TRUE, the backing expression for the
returned constant will be set as well.
*/
{
  a_builtin_operation_kind
                    kind = expr->variant.builtin_operation.kind;
  an_expr_node_ptr  arg1 = expr->variant.builtin_operation.operands;
  a_type_ptr        type1;

  /* eok_parens shouldn't appear here, since the construct is generated. */
  check_assertion(arg1 != NULL && is_type_node(arg1));
  type1 = type_operand_type(arg1);
  if (is_template_dependent_type(type1)) {
    /* The type is dependent, so the result is still unknown. */
    clear_constant(constant, (a_constant_repr_kind)ck_template_param);
    set_template_param_constant_kind(
                   constant, (a_template_param_constant_kind)tpck_expression);
    constant->variant.template_param.variant.expr = expr;
  } else {
    a_boolean  result = compute_is_destructible(kind, type1);
    arg1->type_definition_needed = TRUE;
    clear_constant(constant, (a_constant_repr_kind)ck_integer);
    set_integer_value(&constant->variant.integer_value,
                      (a_host_large_integer)result);
    if (maintain_expression) constant->expr = expr;
  }  /* if */
  constant->type = expr->type;
}  /* fold_is_destructible */


static void fold_is_assignable(an_expr_node_ptr   expr,
                               a_constant_ptr     constant,
                               a_boolean          maintain_expression)
/*
expr is an enk_builtin_operation node for an __is_nothrow_assignable or
__is_trivially_assignable operation.  Store a boolean constant in *constant
whose value is "true" if
      declval<T>() = declval<U>()
with
      template<class T>
        typename add_rvalue_reference<T>::type declval() noexcept;
is well-formed, and it is known no exception is thrown for the assignment (for
the __is_nothrow_assignable case) or all calls involved are to trivial special
members (in the __is_trivially_assignable case).  If any of the operand types
is dependent, store a ck_template_param constant in *constant.  The constant
will be of the tpck_expression variant and will point to the given expression.
If maintain_expression is TRUE, the backing expression for the returned
constant will be set as well.
*/
{
  a_builtin_operation_kind
                    kind = expr->variant.builtin_operation.kind;
  an_expr_node_ptr  arg1 = expr->variant.builtin_operation.operands, arg2;
  a_type_ptr        type1, type2;

  /* eok_parens shouldn't appear here, since the construct is generated. */
  check_assertion(arg1 != NULL && is_type_node(arg1));
  arg2 = arg1->next;
  check_assertion(arg2 != NULL && is_type_node(arg2));
  type1 = type_operand_type(arg1);
  type2 = type_operand_type(arg2);
  if (is_template_dependent_type(type1) || is_template_dependent_type(type2)) {
    /* One or more of the types is dependent, so the result is still
       unknown. */
    clear_constant(constant, (a_constant_repr_kind)ck_template_param);
    set_template_param_constant_kind(
                   constant, (a_template_param_constant_kind)tpck_expression);
    constant->variant.template_param.variant.expr = expr;
  } else {
    a_boolean  result = compute_is_assignable(kind, type1, type2);
    arg1->type_definition_needed = TRUE;
    arg2->type_definition_needed = TRUE;
    clear_constant(constant, (a_constant_repr_kind)ck_integer);
    set_integer_value(&constant->variant.integer_value,
                      (a_host_large_integer)result);
    if (maintain_expression) constant->expr = expr;
  }  /* if */
  constant->type = expr->type;
}  /* fold_is_assignable */


static a_boolean compute_has_nothrow_assign(a_type_ptr  class_type)
/*
Return TRUE if (and only if) this class' copy assignment operators are known
not to throw exceptions.  This can be used to determine the value of the type
trait pseudo-function __has_nothrow_assign, but in Microsoft mode, additional
checking is needed (see microsoft_has_assign_predicate). 
*/
{
  a_field_ptr       fp;
  a_base_class_ptr  bcp;
  a_type_ptr        tp;
  a_symbol_ptr      sym;
  a_boolean         is_list;
  a_boolean         result = TRUE;

  /* First examine any copy assignment operators. */
  sym = class_symbol_supp(symbol_for(class_type))->assignment_operator;
  if (sym != NULL) {
    a_boolean  found_copy_assign = FALSE,
               found_nonthrowing_copy_assign = FALSE;
    if (symbol_is(sym, sk_overloaded_function)) {
      is_list = TRUE;
      sym = sym->variant.overloaded_function.symbols;
    } else {
      is_list = FALSE;
    }  /* if */
    for (; sym != NULL; sym = is_list ? sym->next : NULL) {
      if (symbol_is(sym, sk_member_function)) {
        a_type_qualifier_set  qualifiers;
        a_boolean             ref_param, is_base_class_match;
        a_routine_ptr         rp = sym->variant.routine.ptr;
        if (!rp->compiler_generated &&
            is_assignment_operator_for_copy(
                                  sym, /*move_assign_okay=*/FALSE, &ref_param,
                                  &qualifiers, &is_base_class_match)) {
          found_copy_assign = TRUE;
          if (microsoft_mode && microsoft_version >= 1800 && rp->is_deleted) {
            /* MSVC doesn't consider deleted operators. */
          } else if (is_non_throwing_routine(rp)) {
            /* This copy assignment operator is known not to throw exceptions:
               Continue checking other operators (if any). */
            found_nonthrowing_copy_assign = TRUE;
          } else {
            /* A throwing copy assignment operator. */
            break;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
    if (found_copy_assign) {
      /* There were user-declared copy assignment operators.  If any throws,
         sym points to the first one encountered. */
      result = sym == NULL && found_nonthrowing_copy_assign;
      goto done;
    }  /* if */
  }  /* if */
  /* The copy assignment operator is generated: Look through the fields and
     direct base classes to see if any of them make the result FALSE. */
  fp = class_type->variant.class_struct_union.field_list;
  for (; fp != NULL; fp = fp->next) {
    if (fp->compiler_generated && !fp->is_anonymous_parent_object) {
      /* Ignore fields generated by prelowering. */
      continue;
    }  /* if */
    tp = skip_array_types(fp->type);
    tp = skip_typerefs(tp);
    if (is_immediate_class_type(tp) && !compute_has_nothrow_assign(tp)) {
      result = FALSE;
      goto done;
    }  /* if */
  }  /* for */
  bcp = base_classes_of(class_type);
  for (; bcp != NULL; bcp = bcp->next) {
    if (bcp->direct) {
      tp = skip_typerefs(bcp->type);
      if (is_immediate_class_type(tp) && !compute_has_nothrow_assign(tp)) {
        result = FALSE;
        goto done;
      }  /* if */
    }  /* if */
  }  /* for */
done:
  return result;
}  /* compute_has_nothrow_assign */


static a_boolean compute_has_nothrow_copy(a_type_ptr  class_type)
/*
Return TRUE if (and only if) this class' copy constructors are known not to
throw exceptions.  This can be used to determine the value of the type trait
pseudo-function __has_nothrow_copy, but in Microsoft mode, additional checking
is needed (see microsoft_has_copy_predicate). 
*/
{
  a_field_ptr       fp;
  a_base_class_ptr  bcp;
  a_type_ptr        tp;
  a_symbol_ptr      sym;
  a_boolean         is_list;
  a_boolean         result = TRUE;

  /* First examine any copy constructors. */
  sym = class_symbol_supp(symbol_for(class_type))->constructor;
  if (sym != NULL) {
    a_boolean  found_copy_ctor = FALSE;
    if (symbol_is(sym, sk_overloaded_function)) {
      is_list = TRUE;
      sym = sym->variant.overloaded_function.symbols;
    } else {
      is_list = FALSE;
    }  /* if */
    for (; sym != NULL; sym = is_list ? sym->next : NULL) {
      if (symbol_is(sym, sk_member_function)) {
        a_routine_ptr  rp = sym->variant.routine.ptr;
        a_type_ptr     rtp = skip_typerefs(rp->type);
        if (!rp->compiler_generated &&
            is_copy_constructor_type(rtp, class_type,
                                     (a_type_qualifier_set *)NULL,
                                     /*include_move_ctors=*/FALSE,
                                     /*is_declarative_context=*/TRUE)) {
          found_copy_ctor = TRUE;
          if (is_non_throwing_routine(rp)) {
            /* This copy constructor is known not to throw exceptions:
               Continue checking other constructors (if any). */
          } else {
            /* A throwing copy constructor. */
            break;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
    if (found_copy_ctor) {
      /* There were user-declared copy constructors.  If any throws, sym points
         to the first one encountered. */
      result = sym == NULL;
      goto done;
    }  /* if */
  }  /* if */
  /* The copy constructor is generated: Look through the fields and direct
     base classes to see if any of them make the result FALSE. */
  fp = class_type->variant.class_struct_union.field_list;
  for (; fp != NULL; fp = fp->next) {
    if (fp->compiler_generated && !fp->is_anonymous_parent_object) {
      /* Ignore fields generated by prelowering. */
      continue;
    }  /* if */
    tp = skip_array_types(fp->type);
    tp = skip_typerefs(tp);
    if (is_immediate_class_type(tp) && !compute_has_nothrow_copy(tp)) {
      result = FALSE;
      goto done;
    }  /* if */
  }  /* for */
  bcp = base_classes_of(class_type);
  for (; bcp != NULL; bcp = bcp->next) {
    if (bcp->direct) {
      tp = skip_typerefs(bcp->type);
      if (is_immediate_class_type(tp) && !compute_has_nothrow_copy(tp)) {
        result = FALSE;
        goto done;
      }  /* if */
    }  /* if */
  }  /* for */
done:
  return result;
}  /* compute_has_nothrow_copy */

#if MICROSOFT_EXTENSIONS_ALLOWED

static a_boolean microsoft_has_assign_predicate(a_type_ptr                type,
                                                a_builtin_operation_kind  kind)
/*
Determine the value of the __has_assign or __has_nothrow_assign pseudo-function
(as indicated by kind) applied to the given type in Microsoft mode.
Ordinarily, the result for __has_nothrow_assign is determined by a call to
compute_has_nothrow_assign, but in Microsoft mode, the result can depend on
the order of declaration of the assignment operators.
*/
{
  a_class_symbol_supplement_ptr
                cssp = symbol_supplement_for_class(type);
  a_symbol_ptr  sym = cssp->assignment_operator;
  a_boolean     is_list = FALSE, result = FALSE, found_copy_assign = FALSE;

  if (sym != NULL) {
    if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
      is_list = TRUE;
      sym = sym->variant.overloaded_function.symbols;
    }  /* if */
    for (; sym != NULL; sym = is_list ? sym->next : NULL) {
      if (sym->kind == (a_symbol_kind)sk_member_function) {
        a_type_qualifier_set  qualifiers;
        a_boolean             ref_param, is_base_class_match;
        if (is_assignment_operator_for_copy(
                    sym, /*move_assign_okay=*/FALSE, &ref_param,
                    &qualifiers, &is_base_class_match)) {
          a_routine_ptr  rp = sym->variant.routine.ptr;
          if (kind == (a_builtin_operation_kind)bok_has_assign) {
            /* __has_assign returns true for any user-declared or nontrivial
               copy assignment (MSVC++ does not currently support defaulted
               assignment operators; we treat them like any other user-declared
               operators in that respect). */
            found_copy_assign = TRUE;
            if (!rp->compiler_generated || !rp->is_trivial_copy_function) {
              result = TRUE;
              break;
            }  /* if */
          } else if (!rp->compiler_generated) {
            /* __has_nothrow_assign: Return TRUE if the copy assignment
               operators are declared with "throw()" or a "nothrow"
               attribute. */
            found_copy_assign = TRUE;
            result = is_non_throwing_routine(rp);
            /* Microsoft compilers only consider the first declared copy
               assignment operator.  Since we store those operators in reverse
               order of declaration, continue the loop in case another such
               operator appears on the list. */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  if (!found_copy_assign &&
      kind == (a_builtin_operation_kind)bok_has_nothrow_assign) {
    /* If no copy assignment operator was found in the class, return the
       normal value (which is independent of the declaration order of, e.g.,
       operator= in base classes). */
    result = compute_has_nothrow_assign(type);
  }  /* if */
  return result;
}  /* microsoft_has_assign_predicate */


static a_boolean microsoft_has_copy_predicate(a_type_ptr                type,
                                              a_builtin_operation_kind  kind)
/*
Determine the value of the __has_copy or __has_nothrow_copy pseudo-function
(as indicated by kind) applied to the given type in Microsoft mode.
Ordinarily, the result for __has_nothrow_copy is determined by a call to
compute_has_nothrow_copy, but in Microsoft mode, the result can depend on the
order of declaration of the constructors.
*/
{
  a_class_symbol_supplement_ptr
                cssp = symbol_supplement_for_class(type);
  a_symbol_ptr  sym = cssp->constructor;
  a_boolean     is_list = FALSE, result = FALSE, found_copy_ctor = FALSE;

  if (sym != NULL) {
    if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
      is_list = TRUE;
      sym = sym->variant.overloaded_function.symbols;
    }  /* if */
    for (; sym != NULL; sym = is_list ? sym->next : NULL) {
      if (sym->kind == (a_symbol_kind)sk_member_function) {
        a_routine_ptr  rp = sym->variant.routine.ptr;
        a_type_ptr     rtp = skip_typerefs(rp->type);
        if (is_copy_constructor_type(rtp, type, (a_type_qualifier_set *)NULL,
                                     /*include_move_ctors=*/FALSE,
                                     /*is_declarative_context=*/TRUE)) {
          if (kind == (a_builtin_operation_kind)bok_has_copy) {
            /* __has_copy returns true for any user-declared or nontrivial
               copy constructor (MSVC++ does not currently support defaulted
               copy constructors; we treat them like any other user-declared
               constructors in that respect). */
            found_copy_ctor = TRUE;
            if (!rp->compiler_generated || !rp->is_trivial_copy_function) {
              result = TRUE;
              break;
            }  /* if */
          } else if (!rp->compiler_generated) {
            /* __has_nothrow_copy: Return TRUE if the copy constructors are
               declared with "throw()" or a "nothrow" attribute. */
            found_copy_ctor = TRUE;
            result = is_non_throwing_routine(rp);
            /* Microsoft compilers only consider the first declared copy
               constructor.  Since we store the constructors in reverse order
               of declaration, continue the loop in case another copy
               constructor appears on the list. */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  if (!found_copy_ctor &&
      kind == (a_builtin_operation_kind)bok_has_nothrow_copy) {
    /* If no copy constructor was found in the class, return the normal value
       (which is independent of the declaration order of, e.g., constructors
       in base classes). */
    result = compute_has_nothrow_copy(type);
  }  /* if */
  return result;
}  /* microsoft_has_copy_predicate */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static a_boolean has_trivial_move_constructor(a_type_ptr  type)
/*
Return TRUE if the given type is a class type that has a trivial move
constructor or if it is an array of such a class type.
*/
{
  a_boolean  result;

  type = skip_array_types(type);
  type = skip_typerefs(type);
  if (is_immediate_class_type(type)) {
    /* We currently do not implicitly generate move constructors.  A trivial
       move therefore corresponds to a trivial copy. */
    a_class_symbol_supplement_ptr  cssp = symbol_supplement_for_class(type);
    if (cssp->construction_by_bitwise_copy_allowed) {
      result = TRUE;
    } else {
      result = FALSE;
    }  /* if */
  } else {
    result = FALSE;
  }  /* if */
  return result;
}  /* has_trivial_move_constructor */


static a_boolean has_trivial_move_assign(a_type_ptr  type)
/*
Return TRUE if the given type is a class type that has a trivial move
assignment operator or if it is an array of such a class type.
*/
{
  a_boolean  result;

  type = skip_array_types(type);
  type = skip_typerefs(type);
  if (is_immediate_class_type(type)) {
    /* We currently do not implicitly generate move assignment operators.
       A trivial move therefore corresponds to a trivial copy. */
    a_class_symbol_supplement_ptr  cssp = symbol_supplement_for_class(type);
    if (cssp->assignment_by_bitwise_copy_allowed) {
      result = TRUE;
    } else {
      result = FALSE;
    }  /* if */
  } else {
    result = FALSE;
  }  /* if */
  return result;
}  /* has_trivial_move_assign */


static a_boolean has_nothrow_move_assign(a_type_ptr  type)
/*
Return TRUE if the given type is a class type whose move assignment operators
are known not to throw exceptions, or if it is an array of such a class type.
*/
{
  a_boolean  result;

  type = skip_array_types(type);
  type = skip_typerefs(type);
  if (is_immediate_class_type(type)) {
    a_class_symbol_supplement_ptr  cssp = symbol_supplement_for_class(type);
    if (cssp->assignment_by_bitwise_copy_allowed) {
      result = TRUE;
    } else {
      a_symbol_ptr  sym = cssp->assignment_operator;
      a_boolean     overloaded = symbol_is(sym, sk_overloaded_function);
      a_boolean     has_move_assign = FALSE;
      if (overloaded) sym = sym->variant.overloaded_function.symbols;
      result = TRUE;
      /* Look among the assignment operators for one that moves but doesn't
         throw. */
      for (; sym != NULL; sym = overloaded ? sym->next : NULL) {
        if (sym->kind == (a_symbol_kind)sk_member_function) {
          a_routine_ptr  rp = sym->variant.routine.ptr;
          if (routine_is_move_assignment_operator(rp)) {
            has_move_assign = TRUE;
            if (!is_non_throwing_routine(rp)) {
              /* We found a move-assignment operator that might throw. */
              result = FALSE;
              break;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* for */
      /* If there was no move assignment operator at all, the result should be
         FALSE. */
      result = result && has_move_assign;
    }  /* if */
  } else {
    /* The argument type is not a class type and therefore has no copy
       assignment operators. */
    result = FALSE;
  }  /* if */
  return result;
}  /* has_nothrow_move_assign */


static a_boolean all_copy_assignment_operators_trivial(
                                          a_class_symbol_supplement_ptr  cssp)
/*
Return TRUE if all the copy (not move!) assignment operators of the class
associated with cssp are trivial.
*/
{
  a_boolean     result = FALSE, is_list = FALSE;
  a_symbol_ptr  sym = cssp->assignment_operator;

  if (symbol_is(sym, sk_overloaded_function)) {
    is_list = TRUE;
    sym = sym->variant.overloaded_function.symbols;
  }  /* if */
  for (; sym != NULL; sym = is_list ? sym->next : NULL) {
    if (symbol_is(sym, sk_member_function)) {
      a_type_qualifier_set  tqs;
      a_boolean             is_move;
      a_routine_ptr         rp = sym->variant.routine.ptr;
      if (routine_is_copy_or_move_assign_operator(rp, &tqs, &is_move) &&
          !is_move) {
        if (!rp->is_trivial_copy_function ||
            (microsoft_mode && microsoft_version >= 1800 && rp->is_deleted) ||
            (microsoft_bugs && tqs != TQ_CONST)) {
          /* MSVC fails the following assertion:
               struct S { S& operator=(S&) = default; };
               static_assert(__has_trivial_assign(S));
          */
          result = FALSE;
          break;
        } else {
          /* We found a trivial copy assignment operator.  Assume therefore
             that the result is TRUE, and turn it back to FALSE if we find a
             nontrivial operator. */
          result = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  return result;
}  /* all_copy_assignment_operators_trivial */


static a_boolean all_copy_constructors_trivial(
                                          a_class_symbol_supplement_ptr  cssp)
/*
Return TRUE if all the copy (not move!) constructors of the class associated
with cssp are trivial.
*/
{
  a_boolean     result = FALSE, is_list = FALSE;
  a_symbol_ptr  sym = cssp->constructor;

  if (symbol_is(sym, sk_overloaded_function)) {
    is_list = TRUE;
    sym = sym->variant.overloaded_function.symbols;
  }  /* if */
  for (; sym != NULL; sym = is_list ? sym->next : NULL) {
    if (symbol_is(sym, sk_member_function)) {
      a_type_qualifier_set  tqs;
      a_routine_ptr         rp = sym->variant.routine.ptr;
      if (is_copy_constructor(rp, parent_class_of(rp), &tqs,
                              /*include_move_ctors=*/FALSE,
                              /*is_declarative_context=*/TRUE)) {
        if (!rp->is_trivial_copy_function) {
          result = FALSE;
          break;
        } else {
          /* We found a trivial copy constructor.  Assume therefore that the
             result is TRUE, and turn it back to FALSE if we find a nontrivial
             copy constructor. */
          result = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  return result;
}  /* all_copy_constructors_trivial */


static a_boolean unnamed_bit_field_is_padding(void)
/*
Return TRUE if an unnamed bit-field of non-zero width is considered padding
by type_has_unique_object_representations.  That is the standard behavior;
it is not done by Clang before version 17 or by g++ before 13.1 (except for
the backports to g++ 10.5, 11.4, and 12.3).
*/
{
  a_boolean  result = TRUE;

  if (clang_mode) {
    result = clang_version >= 170000;
  } else if (gpp_mode) {
    result = gnu_version >= 120300 ||
             (gnu_version >= 110400 && gnu_version < 120000) ||
             (gnu_version >= 100500 && gnu_version < 110000);
  }  /* if */
  return result;
}  /* unnamed_bit_field_is_padding */


static a_boolean type_has_unique_object_representations(
                                   a_type_ptr     type,
                                   a_targ_size_t  *after_base_members,
                                   a_boolean      require_trivial_copy = TRUE)
/*
Return TRUE if type satisfies the std::has_unique_object_representations
trait as described in the C++17 Standard. If type is a class type, it must
be complete and if require_trivial_copy is TRUE it must be trivially copyable. 
If after_base_members is non-NULL, type is a base class of a class type and
tail padding is permitted; in that case, *after_base_members is set to the
offset following the last non-static data member of the class.

The result of this predicate is largely left implementation-defined in the
C++ Standard; the code below reflects the values for the Microsoft and g++
compilers.  This function will need to be customized for ABIs and
architectures for which these assumptions are not valid.
*/
{
  a_boolean  result = TRUE;
  a_type_ptr orig_type = type;

  type = skip_typerefs(type);
  if (is_immediate_class_type(type)) {
    if (!clang_mode) {
      /* We're about to check trivial copyability, which can only be TRUE for
         complete class types.  So perform an instantiation if needed.  Clang
         appears not to do that (checked in version 12).  For example:
             template<typename> struct S { int i; };
             static_assert(__has_unique_object_representations(S<int>));
                 // Okay with MSVC and GCC, but not with Clang.
         Clang accepts the assertion if, e.g., "S<int> s;" precedes it. */
      complete_type_is_needed(type);
    }  /* if */
    if (require_trivial_copy && !is_trivially_copyable_type(type)) {
      /* The result is false for a type that is not trivially copyable. */
      result = FALSE;
    } else {
      /* A trivially-copyable class type has unique object representations
         if all of its subobjects do and it has no padding bytes
         anywhere. */
      a_targ_size_t    end_of_last_subobject = 0;
      unsigned int     bit_offset_of_end = 0;
      a_base_class_ptr bcp;
      a_field_ptr      field;
      a_field_ptr      prev_field = NULL;
      a_targ_size_t    prev_field_size = 0;
#if IA64_ABI
      a_targ_size_t    base_size = 0;
      a_targ_size_t    *base_size_p = targ_reuse_tail_padding ? &base_size
                                                              : NULL;
#endif /* IA64_ABI */
      /* First check all direct base subobjects to see if they have unique
         object representations and if there is any padding between
         them. */
      for (bcp = type->variant.class_struct_union.extra_info->base_classes;
           result && bcp != NULL; bcp = bcp->next) {
        if (bcp->direct) {
          if (bcp->offset != end_of_last_subobject) {
            /* There's padding between base class subobjects. */
            result = FALSE;
          } else if (is_empty_class_type(bcp->type)) {
            /* An empty base class: treat it as having size 0 and do not
               check type_has_unique_object_representation, since an empty
               class will consist of nothing but padding.  If it was not
               optimized as an empty base class, the offset checks below
               will catch it and give a FALSE result. */
          } else {
#if IA64_ABI
            if (!type_has_unique_object_representations(bcp->type,
                                                        base_size_p)) {
              result = FALSE;
            }  /* if */
            end_of_last_subobject += targ_reuse_tail_padding ? base_size
                                                             : bcp->type->size;
#else /* !IA64_ABI */
            if (!type_has_unique_object_representations(bcp->type, NULL)) {
              result = FALSE;
            }  /* if */
            end_of_last_subobject += bcp->type->size;
#endif /* IA64_ABI */
          }  /* if */
        }  /* if */
      }  /* for */
      /* Now check all nonstatic data members for the same. */
      for (field = type->variant.class_struct_union.field_list;
           result && field != NULL; field = field->next) {
        a_type_ptr field_type = skip_typerefs(field->type);
        if (type_is(type, tk_union) &&
            ((prev_field != NULL && field_type->size != prev_field_size) ||
             (field->is_bit_field && !clang_mode && !microsoft_mode &&
              field->bit_size < type->size * targ_char_bit))) {
          /* There will be padding in a union if not all fields are the
             same size or (except for Clang and Microsoft) if a bit-field
             is shorter than the size of the union. */
          result = FALSE;
        } else if (!type_is(type, tk_union) &&
                   field->is_bit_field && field->bit_size != 0 &&
                   unnamed_bit_field_is_padding() &&
                   symbol_for(field) == get_unnamed_field_symbol()) {
          /* An unnamed bit-field of non-zero width is padding.  (In a
             union, an unnamed bit-field overlaps the other members, so
             it is handled by the size test above instead.) */
          result = FALSE;
        } else if (field->offset != end_of_last_subobject ||
            field->offset_bit_remainder != bit_offset_of_end) {
          /* There's padding between nonstatic data members. */
          result = FALSE;
        } else {
          if (!type_has_unique_object_representations(field_type, NULL)) {
            result = FALSE;
          }  /* if */
          if (type_is(type, tk_union)) {
            /* Do not update end_of_last_subobject: each subobject begins
               at offset 0.  The previous field size is the same as that
               of the union; otherwise, it would have failed the bit_size
               test above. */
            prev_field = field;
            prev_field_size = field->is_bit_field ? type->size
                                                  : field_type->size;
          } else if (field->is_bit_field) {
            bit_offset_of_end += field->bit_size;
            end_of_last_subobject += bit_offset_of_end / targ_char_bit;
            bit_offset_of_end %= targ_char_bit;
          } else {
            end_of_last_subobject += field_type->size;
          }  /* if */
        }  /* if */
      }  /* for */
      /* Finally, check for tail padding. */
      if (type_is(type, tk_union)) {
        if (type->variant.class_struct_union.field_list == NULL) {
          /* An empty union still has has a non-zero size, which counts as
             padding. */
          result = FALSE;
        }  /* if */
      } else {
        if (after_base_members != NULL) {
          /* Inform the caller about any tail padding. */
          *after_base_members = end_of_last_subobject;
        } else if (end_of_last_subobject != type->size) {
          /* If this is a most-derived class, tail padding means that the
             type does not have unique object representations. */
          result = FALSE;
        }  /* if */
      }  /* if */
    }  /* if */
  } else if (gpp_version_is(< 100000) &&
             is_volatile_qualified_type(orig_type)) {
    /* g++ before version 10 treats volatile-qualified non-class types as
       not having unique object representations. */
    result = FALSE;
  } else {
    /* A non-class type. */
    switch (type->kind) {
      /* Types with no padding and no non-canonical representations. */
      case tk_error:
      case tk_integer:
      case tk_ptr_to_member:
      case tk_template_param:
        break;
      /* Types with padding and/or non-canonical representations. */
#if FIXED_POINT_ALLOWED
      case tk_fixed_point:
#endif /* FIXED_POINT_ALLOWED */
      case tk_float:
#if C99_IL_EXTENSIONS_SUPPORTED
      case tk_imaginary:
      case tk_complex:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      case tk_nullptr:
        result = FALSE;
        break;
      case tk_void:
        /* A void type has no fixed representation. */
        result = FALSE;
        break;
      case tk_pointer:
        /* Reference types are not object types. */
        result = !type->variant.pointer.is_reference;
        break;
#if GNU_EXTENSIONS_ALLOWED && GNU_VECTOR_TYPES_ALLOWED
      case tk_vector:
        if (clang_mode) {
          /* A vector does not have unique object representations. */
          result = FALSE;
        } else {
          /* A vector has the same result as its element type. */
          result = type_has_unique_object_representations(
                                      type->variant.vector.element_type, NULL);
        }  /* if */
        break;
#endif /* GNU_EXTENSIONSz_ALLOWED && GNU_VECTOR_TYPES_ALLOWED */
      case tk_array:
        /* An array has the same result as its element type. */
        result = type_has_unique_object_representations(
                                    underlying_array_element_type(type), NULL);
        break;
      default:
        /* Non-object types, such as function types, should return
           FALSE. */
        result = FALSE;
        break;
    }  /* switch */
  }  /* if */
  return result;
}  /* type_has_unique_object_representations */


static a_boolean type_is_trivially_equality_comparable(
                                          a_type_ptr  type,
                                          a_boolean   check_unique_rep = TRUE)
/*
Return TRUE if it is known that values of the given type can be compared
using memcmp applied to their representation.  A necessary condition for this
is that values of that type have a unique representation (e.g., no padding or
floating-point components).  If the caller already has established that,
check_unique_rep can be passed FALSE to skip checking that condition.
*/
{
  a_boolean   result;
  a_type_ptr  utype = skip_typerefs(type);

  if (is_immediate_enum_type(utype)) {
    /* Enum types are presumed not to be trivially comparable because a user-
       defined operator== could be provided for them. */
    result = FALSE;
  } else if (type_is(utype, tk_array)) {
    /* Arrays do not have equality operators. */
    result = FALSE;
  } else if (check_unique_rep &&
             !type_has_unique_object_representations(
                                            type, (a_targ_size_t*)NULL,
                                            /*require_trivial_copy=*/FALSE)) {
    /* If the byte representation for a given value can vary (due to padding,
       NaN-like states, etc.) memcmp cannot be used for comparison.  (Note:
       This also causes reference types not to be trivially comparable.) */
    result = FALSE;
  } else if (is_immediate_class_type(utype)) {
    if (!class_has_default_equality_operator(utype) ||
        is_polymorphic_class_type(utype) ||
        utype->variant.class_struct_union.any_virtual_base_classes ||
        !spaceship_enabled) {
      result = FALSE;
    } else {
      /* Check whether all the subobjects are trivially comparable. */
      a_field_ptr  fp = next_proper_initializable_field(fields_of(utype));
      /* Assume the result will be TRUE, and clear it back to FALSE if any
         subobject is not trivially comparable.  Since we already established
         that the object type as a whole has unique representations, we need
         not repeat that check for the subobject types. */
      result = TRUE;
      for (; fp != NULL; fp = next_proper_initializable_field(fp->next)) {
        a_type_ptr  tp = skip_array_types(fp->type);
        if (!type_is_trivially_equality_comparable(
                                            tp, /*check_unique_rep=*/FALSE)) {
          result = FALSE;
        }  /* if */
      }  /* for */
      if (result) {
        a_base_class_ptr  bcp = direct_base_classes_of(utype);
        for (; bcp != NULL; bcp = bcp->next_direct) {
          if (!type_is_trivially_equality_comparable(
                                     bcp->type, /*check_unique_rep=*/FALSE)) {
            result = FALSE;
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
  } else {
    result = TRUE;
  }  /* if */
  return result;
}  /* type_is_trivially_equality_comparable */


static a_boolean type_is_trivially_relocatable(a_type_ptr  type)
/*
Return TRUE if the given type is trivially relocatable.  A class type is
trivially relocatable if it has a trivial, non-deleted destructor, at least one
eligible trivial copy or move constructor, and no eligible non-trivial copy or
move constructors.  A non-class type is trivially relocatable if it is an
object type.
*/
{
  a_boolean   result;
  a_type_ptr  utype = skip_typerefs(skip_array_types(type));

  if (is_immediate_class_type(utype)) {
    a_class_symbol_supplement_ptr  cssp = symbol_supplement_for_class(utype);
    if (!has_nontrivial_destructor(cssp)) {
      a_boolean     is_list;
      a_symbol_ptr  sym = cssp->constructor;
      if (sym != NULL && symbol_is(sym, sk_overloaded_function)) {
        is_list = TRUE;
        sym = sym->variant.overloaded_function.symbols;
      } else {
        is_list = FALSE;
      }  /* if */
      if (sym == NULL && cssp->construction_by_bitwise_copy_allowed) {
        result = TRUE;
      } else {
        result = FALSE;
        for (; sym != NULL; sym = is_list ? sym->next : NULL) {
          a_routine_ptr     rp;
          if (symbol_is(sym, sk_function_template)) continue;
          check_assertion(symbol_is(sym, sk_member_function));
          rp = sym->variant.routine.ptr;
          /* Ignore any ineligible functions. */
          if (rp->is_deleted || (in_front_end && is_ineligible(sym))) continue;
          if (rp->is_trivial_copy_function) {
            /* We need at least one trivial copy or move constructor. */
            result = TRUE;
          } else if (is_copy_constructor(rp, utype,
                                         (a_type_qualifier_set*)NULL,
                                         /*include_move_ctors=*/TRUE,
                                         /*is_declarative_context=*/TRUE)) {
            /* If there are any non-trivial copy or move constructors, the
               class is not trivially relocatable. */
            result = FALSE;
            break;
          }  /* if */
        }  /* for */
      }  /* if */
    } else {
      result = FALSE;
    }  /* if */
  } else {
    result = is_object_type(utype);
  }  /* if */
  return result;
}  /* type_is_trivially_relocatable */


static void fold_unary_type_trait_helper(
                                    an_expr_node_ptr   expr,
                                    a_constant_ptr     constant,
                                    a_boolean          maintain_expression,
                                    a_source_position  *pos,
                                    a_boolean          complete_class_property)
/*
expr is an enk_builtin_operation node representing a boolean type predicate
(as described in ISO/IEC 19768) with a single type operand (e.g.,
"__is_union").  If the operand type is nondependent, store a boolean
constant in *constant.  The boolean constant will have value "true" if the
associated type predicate is true for the type represented by its operand.
otherwise, the constant will have value "false".  If the operand type is
dependent, store a ck_template_param constant in *constant.  The constant
will be of the tpck_expression variant and will point to the given
expression.  If maintain_expression is TRUE, the backing expression for the
returned constant will be set as well.  If complete_class_property is TRUE
and the operand type is an incomplete class type, the result will be FALSE
and, if pos is not NULL, an error will be reported.
*/
{
  an_expr_node_ptr  arg = expr->variant.builtin_operation.operands;
  a_type_ptr        type;

  /* eok_parens shouldn't appear here, since the construct is generated. */
  check_assertion(arg != NULL && arg->next == NULL && is_type_node(arg));
  type = type_operand_type(arg);
  if (is_template_dependent_type(type) ||
      (microsoft_mode && in_ms_nonreal_class_instantiation())) {
    /* For template-dependent types, create a ck_template_param result.
       In Microsoft nonreal instantiations, we may end up with incomplete-but-
       nondependent types but they shouldn't trigger an error; therefore, we
       create a ck_template_param result for all types in such contexts. */
    clear_constant(constant, (a_constant_repr_kind)ck_template_param);
    set_template_param_constant_kind(
                   constant, (a_template_param_constant_kind)tpck_expression);
    constant->variant.template_param.variant.expr = expr;
  } else {
    a_boolean                 result = FALSE, incomplete_class_error = FALSE;
    a_boolean                 is_list = FALSE;
    a_boolean                 non_move_assign_case = FALSE;
    a_builtin_operation_kind  kind = expr->variant.builtin_operation.kind;
    a_symbol_ptr              sym = NULL;
    a_class_symbol_supplement_ptr
                              cssp = NULL;
    a_boolean                 is_const = is_const_qualified_type(type);
    a_type_ptr                orig_type = type;
    if (kind == bok_is_trivial ||
        kind == bok_is_standard_layout ||
        kind == bok_is_literal_type ||
        kind == bok_is_pod ||
        kind == bok_has_copy ||
        kind == bok_has_nothrow_copy ||
        kind == bok_has_trivial_copy ||
        kind == bok_has_trivial_destructor ||
        kind == bok_has_trivial_move_constructor ||
        kind == bok_has_nothrow_constructor ||
        kind == bok_has_trivial_constructor ||
        kind == bok_is_trivially_copyable ||
        kind == bok_has_user_destructor ||
        kind == bok_is_trivially_relocatable ||
        kind == bok_is_trivially_equality_comparable ||
        kind == bok_is_bitwise_cloneable) {
      if (is_array_type(type)) {
        type = skip_array_types(type);
        if ((gpp_version_is(any_version) || microsoft_mode) &&
            (kind == bok_has_copy ||
             kind == bok_has_nothrow_copy ||
             kind == bok_has_trivial_copy ||
             kind == bok_has_trivial_destructor ||
             kind == bok_has_trivial_move_constructor ||
             kind == bok_has_nothrow_constructor ||
             kind == bok_has_trivial_constructor ||
             kind == bok_is_trivially_copyable ||
             kind == bok_has_user_destructor)) {
          /* GCC and MSVC appear to accept these intrinsics applied to
             incomplete array types (if they are recognized at all).  However,
             Microsoft produces a false value while GCC produces a true value
             in that case.  */
          complete_type_is_needed(type);
          if (is_incomplete_type(type)) {
            result = gpp_mode;
            goto result_known;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    type = skip_typerefs(type);
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (cli_or_cx_enabled &&
        (kind == (a_builtin_operation_kind)bok_is_sealed ||
         kind == (a_builtin_operation_kind)bok_is_simple_value_class ||
         kind == (a_builtin_operation_kind)bok_is_value_class)) {
      /* These operators apply to the boxed version of non-pointer value
         types. */
      if (is_boxable_type(type)) {
        type = boxed_type_for(type);
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (!is_immediate_class_type(type)) {
      /* Non-class types. */
      /* Note that g++ (checked in 4.5) treats scoped enums the same as
         unscoped enums. */
      switch (kind) {
        case bok_has_copy:
        case bok_has_nothrow_copy:
        case bok_has_trivial_copy:
        case bok_has_trivial_destructor:
        case bok_has_trivial_move_constructor:
          if (microsoft_mode && microsoft_version < 1700) {
            /* MSVC returns FALSE for all of these (which is, at least in
               some cases, weird, but there you have it). */
            result = FALSE;
          } else if (is_function_type(type) || is_void_type(type)) {
            /* Function types and "void" aren't variable types or object types.
               So these predicates always produce FALSE for those cases. */
            result = FALSE;
          } else {
            result = TRUE;
          }  /* if */
          break;
        case bok_is_pod:
        case bok_has_nothrow_constructor:
        case bok_has_trivial_constructor:
          if (microsoft_mode && microsoft_version < 1800) {
            /* Early versions of MSVC returned FALSE for nonclass types. */
            result = FALSE;
          } else if (is_reference_type(type) || is_function_type(type)) {
            /* References and functions cannot be default-initialized and
               aren't considered PODs.  They aren't really "copyable"
               either. */
            result = FALSE;
          } else if (is_void_type(type)) {
            /* Type "void" (with or without qualifiers) isn't considered a
               POD. */
            result = FALSE;
          } else {
            result = TRUE;
          }  /* if */
          break;
        case bok_is_trivially_copyable:
          if (microsoft_mode && microsoft_version < 1800) {
            /* Early versions of MSVC returned FALSE for nonclass types. */
            result = FALSE;
          } else {
            result = is_trivially_copyable_type(orig_type);
          }  /* if */
          break;
        case bok_has_assign:
          if (microsoft_mode) {
            /* MSVC always returns FALSE for nonclass types. */
            result = FALSE;
            break;
          }
          FALLTHROUGH
        case bok_has_trivial_assign:
        case bok_has_nothrow_assign:
          non_move_assign_case = TRUE;
          FALLTHROUGH
        case bok_has_trivial_move_assign:
        case bok_has_nothrow_move_assign:
          if (microsoft_mode && microsoft_version < 1800) {
            /* Early versions of MSVC always return FALSE for nonclass types.
             */
            result = FALSE;
          } else if (microsoft_mode && microsoft_version < 1900 &&
                     !is_enum_type(type) && non_move_assign_case) {
            /* Earlier versions of MSVC always returned FALSE for non-enums
               and non-move assigns. */
            result = FALSE;
          } else if (is_reference_type(type) || is_function_type(type) ||
                     is_void_type(type) || is_const) {
            /* References, const objects, functions, and void expressions
               cannot be assigned to. */
            result = FALSE;
          } else if (is_array_type(type)) {
            a_type_ptr element_type = underlying_array_element_type(type);
            if (microsoft_mode &&
                (microsoft_version < 1800 || microsoft_version >= 1900)) {
              /* All versions of MSVC outside of an intermediate range
                 always return FALSE for arrays. */
              result = FALSE;
            } else if (!is_immediate_class_type(element_type) ||
                       is_pod_class(element_type) ||
                       !non_move_assign_case) {
              /* Arrays of PODs and nonclass types have trivial/nothrow
                 assigns. All arrays have trivial/nothrow move assigns. */
              result = TRUE;
            } else {
              result = FALSE;
            }  /* if */
          } else {
            result = TRUE;
          }  /* if */
          break;
        case bok_has_user_destructor:
        case bok_has_virtual_destructor:
        case bok_is_abstract:
        case bok_is_class:
        case bok_is_empty:
        case bok_is_polymorphic:
        case bok_is_union:
#if MICROSOFT_EXTENSIONS_ALLOWED
        case bok_has_finalizer:
        case bok_is_delegate:
        case bok_is_interface_class:
        case bok_is_ref_array:
        case bok_is_ref_class:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          result = FALSE;
          break;
#if MICROSOFT_EXTENSIONS_ALLOWED
        case bok_is_simple_value_class:
        case bok_is_value_class:
          /* Microsoft compilers appear to use the boxed type when there is
             one (see above).  However, in non-C++/CLI modes, they also report
             enumerations as value classes. */
          check_assertion(!(cli_or_cx_enabled &&
                            is_immediate_enum_type(type)));
          result = is_immediate_enum_type(type);
          break;
        case bok_is_win_class:
        case bok_is_win_interface:
          result = FALSE;
          break;
        case bok_is_valid_winrt_type:
          /* The result of __is_valid_winrt_type is somewhat complex.  For now,
             we always produce TRUE. */
          result = TRUE;
          break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        case bok_is_enum:
          result = is_immediate_enum_type(type);
          break;
        case bok_is_scoped_enum:
          result = is_scoped_enum_type(type);
          break;
        case bok_is_function:
          result = is_function_type(type);
          break;
        case bok_is_array:
          result = is_array_type(type);
          break;
        case bok_is_trivial:
          result = is_object_type(type);
          break;
        case bok_is_standard_layout:
          result = (is_object_type(type) && !is_sizeless_type(type)) ||
                   (microsoft_mode && is_function_type(type));
          break;
        case bok_is_literal_type:
          result = is_literal_type(type);
          break;
#if MICROSOFT_EXTENSIONS_ALLOWED
        case bok_is_sealed:
          result = TRUE;
          break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        case bok_is_final:
          result = FALSE;
          break;
        case bok_is_trivially_copy_assignable:
          result = TRUE;
          break;
        case bok_has_unique_object_representations:
          result = type_has_unique_object_representations(
                                            skip_array_types(orig_type), NULL);
          break;
        case bok_is_aggregate:
          if (type->kind == (a_type_kind)tk_array) {
            if (is_incomplete_type(array_element_type(type))) {
              /* An array type with an incomplete element type can be
                 declared, but using such a type with is_aggregate violates
                 the "shall" requirement in [meta.unary.prop]. */
              incomplete_class_error = TRUE;
            } else {
              result = TRUE;
            }  /* if */
#if GNU_VECTOR_TYPES_ALLOWED
          } else if (is_vector_type(type)) {
            /* g++ treats vector types as aggregates. */
            result = TRUE;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
#if C99_IL_EXTENSIONS_SUPPORTED
          } else if (is_complex_type(type)) {
            /* Complex types should be treated as aggregates. */
            result = TRUE;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
          } else {
            /* Non-class, non-array types are not aggregates. */
            result = FALSE;
          }  /* if */
          break;
        case bok_is_arithmetic:
          result = is_arithmetic_type(type);
          break;
        case bok_is_complete_type:
          result = is_complete_object_type(type) ||
                   is_reference_type(type) ||
                   is_function_type(type);
          break;
        case bok_is_compound:
          result = !is_fundamental_type(type);
          break;
        case bok_is_const:
          result = is_const;
          break;
        case bok_is_floating_point:
          result = is_floating_type(type);
          break;
        case bok_is_fundamental:
          result = is_fundamental_type(type);
          break;
        case bok_is_integral:
          result = is_integral_type(type);
          break;
        case bok_is_lvalue_reference:
          result = is_lvalue_reference_type(type);
          break;
        case bok_is_member_function_pointer:
          result = is_ptr_to_member_type(type) &&
                   is_function_type(pm_member_type(type));
          break;
        case bok_is_member_object_pointer:
          result = is_ptr_to_member_type(type) &&
                   !is_function_type(pm_member_type(type));
          break;
        case bok_is_member_pointer:
          result = is_ptr_to_member_type(type);
          break;
        case bok_is_object:
          result = is_object_type(type);
          break;
        case bok_is_pointer:
          result = is_pointer_type(type);
          break;
        case bok_is_reference:
          result = is_reference_type(type);
          break;
        case bok_is_rvalue_reference:
          result = is_rvalue_reference_type(type);
          break;
        case bok_is_scalar:
          result = is_scalar_type(type);
          break;
        case bok_is_signed:
          result = is_arithmetic_type(type) && !is_bool_type(type) &&
                   (is_signed_integral_type(type) || is_floating_type(type));
          break;
        case bok_is_unsigned:
          result = is_bool_type(type) ||
                   (is_arithmetic_type(type) &&
                    !(is_signed_integral_type(type) ||
                      is_floating_type(type)));
          break;
        case bok_is_void:
          result = is_void_type(type);
          break;
        case bok_is_volatile:
          result = is_volatile_qualified_type(orig_type);
          break;
        case bok_is_bounded_array:
          if (is_array_type(orig_type) &&
              !is_incomplete_array_type(orig_type)) {
            result = TRUE;
          }  /* if */
          break;
        case bok_is_unbounded_array:
          if (is_array_type(orig_type) && is_incomplete_array_type(orig_type)){
            result = TRUE;
          }  /* if */
          break;
        case bok_is_referenceable:
          result = is_referenceable_type(orig_type);
          break;
        case bok_is_trivially_equality_comparable:
          result = type_is_trivially_equality_comparable(orig_type);
          break;
        case bok_is_trivially_relocatable:
          result = type_is_trivially_relocatable(type);
          break;
        case bok_is_bitwise_cloneable:
          /* Non-class types are always bitwise cloneable. */
          result = TRUE;
          break;
        case bok_builtin_is_implicit_lifetime:
          result = is_scalar_type(type) || is_array_type(type)
#if GNU_VECTOR_TYPES_ALLOWED
                   || is_vector_type(type)
#endif /* GNU_VECTOR_TYPES_ALLOWED */
                                          ;
          break;
        case bok_builtin_is_structural:
          result = is_structural_type(orig_type);
          break;
        default:
          unexpected_condition();
      }  /* switch */
      goto result_known;
    } else if (complete_class_property) {
      /* An incomplete class type is invalid. */
      complete_type_is_needed(type);
      if (is_incomplete_type(type)) {
        incomplete_class_error = TRUE;
        goto result_known;
      } else {
        arg->type_definition_needed = TRUE;
        cssp = symbol_supplement_for_class(type);
      }  /* if */
    }  /* if */
    switch (kind) {
      case bok_has_assign:
      case bok_has_nothrow_assign:
        if (!microsoft_mode ||
            (microsoft_version >= 1800 &&
             kind == (a_builtin_operation_kind)bok_has_nothrow_assign)) {
          result = !is_const && compute_has_nothrow_assign(type);
#if MICROSOFT_EXTENSIONS_ALLOWED
        } else {
          result = microsoft_has_assign_predicate(type, kind);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        }  /* if */
        break;
      case bok_has_copy:
      case bok_has_nothrow_copy:
        if (!microsoft_mode) {
          check_assertion(kind ==
                              (a_builtin_operation_kind)bok_has_nothrow_copy);
          result = compute_has_nothrow_copy(type);
#if MICROSOFT_EXTENSIONS_ALLOWED
        } else {
          result = microsoft_has_copy_predicate(type, kind);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        }  /* if */
        break;
      case bok_has_nothrow_constructor:
        check_assertion(cssp != NULL);  /* For Coverity. */
        sym = cssp->constructor;
        if (sym == NULL) {
         /* __has_nothrow_constructor returns true if there is no recorded
            constructor (which implies that the default constructor is
            implicitly declared and trivial). */
          result = TRUE;
          goto result_known;
        } else if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
          is_list = TRUE;
          sym = sym->variant.overloaded_function.symbols;
        }  /* if */
        for (; sym != NULL; sym = is_list ? sym->next : NULL) {
          if (sym->kind == (a_symbol_kind)sk_member_function) {
            a_routine_ptr  rp = sym->variant.routine.ptr;
            if (is_default_constructor(rp, /*is_declarative_context=*/TRUE)) {
              /* There may be more than one default constructor.  E.g.:
                   struct S { S(int = 0); S(short = 0); };  */
              result = is_non_throwing_routine(rp);
              if (microsoft_mode) {
                /* Microsoft compilers only consider the first declared default
                   constructor.  Since we store the constructors in reverse
                   order of declaration, continue the loop in case another
                   default constructor appears on the list. */
              } else if (!result) {
                /* If any of the default constructors may throw an exception,
                   __has_nothrow_constructor should return FALSE (in non-
                   Microsoft modes). */
                goto result_known;
              }  /* if */
            }  /* if */
          }  /* if */
        }  /* for */
        break;
      case bok_has_trivial_assign:
        check_assertion(cssp != NULL);  /* For Coverity. */
        result = !is_const &&
                 (all_copy_assignment_operators_trivial(cssp) &&
                  !cssp->contains_vtable);
        break;
      case bok_has_trivial_constructor:
        check_assertion(cssp != NULL);  /* For Coverity. */
        result = cssp->is_cpp03_POD ||
                 cssp->trivial_default_constructor != NULL;
        break;
      case bok_has_trivial_copy:
        check_assertion(cssp != NULL);  /* For Coverity. */
        result = cssp->construction_by_bitwise_copy_allowed ||
                 (all_copy_constructors_trivial(cssp) &&
                  !cssp->contains_vtable);
        break;
      case bok_has_trivial_destructor:
        check_assertion(cssp != NULL);  /* For Coverity. */
        result = cssp->has_trivial_destructor;
        break;
      case bok_has_user_destructor:
        check_assertion(microsoft_mode);
        check_assertion(cssp != NULL);  /* For Coverity. */
        result = cssp->destructor != NULL &&
                 !cssp->destructor->variant.routine.ptr->compiler_generated;
        break;
      case bok_has_virtual_destructor:
        check_assertion(cssp != NULL);  /* For Coverity. */
        /* In C++/CLI mode, ref class destructors are never virtual; however,
           because they are only ever called by Dispose(bool), which is
           virtual, they are considered virtual. */
        result = cssp->destructor != NULL &&
                 (cssp->destructor->variant.routine.ptr->is_virtual
                  if_microsoft_extensions(
                                  || cli_class_type_kind_is(type, cctk_ref)));
        break;
      case bok_is_abstract:
        result = type->variant.class_struct_union.abstract;
        break;
      case bok_is_class:
        /* In C++/CLI mode this really means: is_native_class. */
        result = is_class_or_struct(type)
                 if_microsoft_extensions(
                              && cli_class_type_kind_is(type, cctk_standard));
        break;
      case bok_is_empty:
        /* The standard "std::is_empty" trait is always false for union types.
           However, Microsoft didn't implement that part of the standard until
           the final "19.00" release (early "preview" releases stuck to the
           earlier behavior). */
        result = ((microsoft_mode &&
                   (microsoft_version < 1900 ||
                    (microsoft_version == 1900 &&
                     microsoft_build_number <= 22129))) ||
                  !is_union_type(type)) &&
                 is_empty_class_type(type);
        break;
      case bok_is_enum:
      case bok_is_scoped_enum:
        result = FALSE;
        break;
      case bok_is_function:
        result = FALSE;
        break;
      case bok_is_array:
        result = FALSE;
        break;
      case bok_is_pod:
        /* Note that only class types are considered by Microsoft compilers. */
        check_assertion(cssp != NULL);  /* For Coverity. */
        result = is_pod_class(type);
        break;
      case bok_is_polymorphic:
        /* C++/CLI value classes are not polymorphic even though they can
           implement interfaces. */
        result = is_polymorphic_class_type(type)
                 if_microsoft_extensions(
                                && !cli_class_type_kind_is(type, cctk_value));
        break;
      case bok_is_union:
        result = (type->kind == (a_type_kind)tk_union);
        break;
      case bok_is_trivial:
        result = is_trivial_class(type); 
        break;
      case bok_is_standard_layout:
        check_assertion(cssp != NULL);  /* For Coverity. */
        result = cssp->standard_layout;
        break;
      case bok_is_trivially_copyable:
        result = is_trivially_copyable_type(orig_type);
        break;
      case bok_is_literal_type:
        result = is_literal_type(type);
        break;
      case bok_has_trivial_move_constructor:
        result = has_trivial_move_constructor(type);
        break;
      case bok_has_trivial_move_assign:
        result = !is_const && has_trivial_move_assign(type);
        break;
      case bok_has_nothrow_move_assign:
        result = !is_const && has_nothrow_move_assign(type);
        break;
#if MICROSOFT_EXTENSIONS_ALLOWED
      case bok_has_finalizer:
        check_assertion(cssp != NULL);  /* For Coverity. */
        result = cssp->finalizer != NULL;
        break;
      case bok_is_delegate:
        if (cli_or_cx_enabled) {
          a_type_ptr  delegate_tp = cli_class_type_for(csk_system_delegate);
          a_type_ptr  multicast_delegate_tp = NULL;
          if (!cppcx_enabled) {
            multicast_delegate_tp =
                            cli_class_type_for(csk_system_multicast_delegate);
          }  /* if */
          result = (type->variant.class_struct_union.is_delegate_class ||
                    identical_types(type, delegate_tp) ||
                    (multicast_delegate_tp != NULL &&
                     identical_types(type, multicast_delegate_tp)));
        }  else {
          result = FALSE;
        }  /* if */
        break;
      case bok_is_interface_class:
        result = cli_class_type_kind_is(type, cctk_interface);
        break;
      case bok_is_ref_array:
        if (cli_or_cx_enabled && class_type_supp(type)->is_cli_array) {
          result = TRUE;
        } else if (cppcli_enabled) {
          /* System::Array isn't technically a ref array, but it supports the
             subscript operator, and ref arrays all derive from it, so it is
             considered a ref array. */
          a_type_ptr  array_tp = cli_class_type_for(csk_system_array);
          result = identical_types(type, array_tp);
        } else {
          result = FALSE;
        }  /* if */
        break;
      case bok_is_ref_class:
        result = cli_class_type_kind_is(type, cctk_ref) && 
                 !class_type_supp(type)->is_cli_array;
        break;
      case bok_is_sealed:
        result = type->variant.class_struct_union.final;
        break;
      case bok_is_simple_value_class:
        result = is_simple_value_class_type(type);
        break;
      case bok_is_value_class:
        result = cli_class_type_kind_is(type, cctk_value);
        break;
      case bok_is_win_class:
        result = cppcx_enabled && cli_class_type_kind_is(type, cctk_ref);
        break;
      case bok_is_win_interface:
        result = cppcx_enabled && cli_class_type_kind_is(type, cctk_interface);
        break;
      case bok_is_valid_winrt_type:
          /* The result of __is_valid_winrt_type is somewhat complex.  For now,
             we always produce TRUE. */
        result = TRUE;
        break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      case bok_is_final:
        result = type->variant.class_struct_union.final;
        break;
      case bok_is_trivially_copy_assignable:
        check_assertion(cssp != NULL);  /* For Coverity. */
        result = cssp->assignment_by_bitwise_copy_allowed &&
                 !cssp->has_deleted_copy_or_move_assign_operator;
        break;
      case bok_has_unique_object_representations:
        result = type_has_unique_object_representations(type, NULL);
        break;
      case bok_is_aggregate:
        result = class_symbol_supp(symbol_for(type))->is_class_aggregate;
        break;
      case bok_is_complete_type:
        result = is_complete_object_type(type);
        break;
      case bok_is_compound:
      case bok_is_object:
        result = TRUE;
        break;
      case bok_is_const:
        result = is_const;
        break;
      case bok_is_volatile:
        result = is_volatile_qualified_type(orig_type);
        break;
      case bok_is_bounded_array:
        if (is_array_type(orig_type) && !is_incomplete_array_type(orig_type)) {
          result = TRUE;
        }  /* if */
        break;
      case bok_is_unbounded_array:
        if (is_array_type(orig_type) && is_incomplete_array_type(orig_type)) {
          result = TRUE;
        }  /* if */
        break;
      case bok_is_referenceable:
        result = is_referenceable_type(orig_type);
        break;
      case bok_is_trivially_equality_comparable:
        result = type_is_trivially_equality_comparable(orig_type);
        break;
      case bok_is_trivially_relocatable:
        result = type_is_trivially_relocatable(type);
        break;
      case bok_is_bitwise_cloneable:
        check_assertion(cssp != NULL);
        result = cssp->assignment_by_bitwise_copy_allowed ||
                 cssp->construction_by_bitwise_copy_allowed ||
                 !cssp->makes_copy_construction_nontrivial ||
                 !cssp->makes_copy_assignment_nontrivial;
        break;
      case bok_builtin_is_implicit_lifetime:
        result = is_implicit_lifetime_class(type);
        break;
      case bok_builtin_is_structural:
        result = is_structural_type(orig_type);
        break;
      case bok_is_arithmetic:
      case bok_is_floating_point:
      case bok_is_fundamental:
      case bok_is_integral:
      case bok_is_lvalue_reference:
      case bok_is_member_function_pointer:
      case bok_is_member_object_pointer:
      case bok_is_member_pointer:
      case bok_is_pointer:
      case bok_is_reference:
      case bok_is_rvalue_reference:
      case bok_is_scalar:
      case bok_is_signed:
      case bok_is_unsigned:
      case bok_is_void:
        result = FALSE;
        break;
      default:
        unexpected_condition();
    }  /* switch */
result_known:
    if (incomplete_class_error) {
      clear_constant(constant, (a_constant_repr_kind)ck_error);
      if (pos != NULL) {
        pos_error((kind == (a_builtin_operation_kind)bok_is_aggregate
                                                  ? ec_element_type_incomplete
                                                  : ec_incomplete_class_type),
                  pos);
      }  /* if */
    } else {
      clear_constant(constant, (a_constant_repr_kind)ck_integer);
      set_integer_value(&constant->variant.integer_value,
                        (a_host_large_integer)result);
    }  /* if */
    if (maintain_expression) constant->expr = expr;
  }  /* if */
  constant->type = expr->type;
}  /* fold_unary_type_trait_helper */

#if GNU_EXTENSIONS_ALLOWED

static void fold_types_compatible(an_expr_node_ptr   expr,
                                  a_constant_ptr     constant,
                                  a_boolean          maintain_expression)
/*
expr is an enk_builtin_operation node for a GNU C __builtin_types_compatible
operation.  If the operand types are nondependent, store a boolean constant in
*constant.  The boolean constant will have value "true" if the operand types
are "compatible" (ignoring top-level qualifiers); otherwise, the constant will
have value "false".  If either of the operand types is dependent, store a
ck_template_param constant in *constant.  The constant will be of the
tpck_expression variant and will point to the given expression.
If maintain_expression is TRUE, the backing expression for the returned
constant will be set as well.
*/
{
  an_expr_node_ptr  arg1 = expr->variant.builtin_operation.operands,
                    arg2 = arg1->next;
  a_type_ptr        type1, type2;

  /* eok_parens shouldn't appear here, since the construct is generated. */
  check_assertion(arg1 != NULL && arg2 != NULL && arg2->next == NULL &&
                  arg1->kind == (an_expr_node_kind)enk_type_operand &&
                  arg2->kind == (an_expr_node_kind)enk_type_operand);
  type1 = arg1->variant.type_operand.type;
  type2 = arg2->variant.type_operand.type;
  if (is_template_dependent_type(type1) ||
      is_template_dependent_type(type2)) {
    clear_constant(constant, (a_constant_repr_kind)ck_template_param);
    set_template_param_constant_kind(
                   constant, (a_template_param_constant_kind)tpck_expression);
    constant->variant.template_param.variant.expr = expr;
  } else {
    a_type_compat_flags_set  flags = TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING |
                                     TCF_IGNORE_TYPE_QUALIFIERS;
    a_boolean                result;
    if (gcc_mode && gnu_version >= 40000) {
      /* Starting with GCC 4.0, "int[3]" and "int const[3]" are considered
         compatible in GNU C mode also (in C++ mode, this already falls out
         of the C++ type qualifier rules). */
      flags |= TCF_USE_CPP_QUALIFIER_RULES;
    }  /* if */
    result = f_types_are_compatible(type1, type2, flags);
    clear_constant(constant, (a_constant_repr_kind)ck_integer);
    set_integer_value(&constant->variant.integer_value,
                      (a_host_large_integer)result);
    if (maintain_expression) constant->expr = expr;
  }  /* if */
  constant->type = expr->type;
}  /* fold_types_compatible */

#endif /* GNU_EXTENSIONS_ALLOWED */

static void fold_is_same(an_expr_node_ptr   expr,
                         a_constant_ptr     constant,
                         a_boolean          maintain_expression)
/*
expr is an enk_builtin_operation node for an __is_same (or __is_same_as)
operation.  If the operand types are nondependent, store a boolean constant in
*constant.  The boolean constant will have value "true" if the operand types
are identical; otherwise, the constant will have value "false".  If either of
the operand types is dependent, store a ck_template_param constant in
*constant.  The constant will be of the tpck_expression variant and will point
to the given expression.  If maintain_expression is TRUE, the backing
expression for the returned constant will be set as well.
*/
{
  an_expr_node_ptr  arg1 = expr->variant.builtin_operation.operands,
                    arg2 = arg1->next;
  a_type_ptr        type1, type2;

  /* eok_parens shouldn't appear here, since the construct is generated. */
  check_assertion(arg1 != NULL && arg2 != NULL && arg2->next == NULL &&
                  arg1->kind == (an_expr_node_kind)enk_type_operand &&
                  arg2->kind == (an_expr_node_kind)enk_type_operand);
  type1 = arg1->variant.type_operand.type;
  type2 = arg2->variant.type_operand.type;
  if (is_template_dependent_type(type1) ||
      is_template_dependent_type(type2)) {
    clear_constant(constant, (a_constant_repr_kind)ck_template_param);
    set_template_param_constant_kind(
                   constant, (a_template_param_constant_kind)tpck_expression);
    constant->variant.template_param.variant.expr = expr;
  } else {
    a_boolean  result = identical_types(type1, type2);
    clear_constant(constant, (a_constant_repr_kind)ck_integer);
    set_integer_value(&constant->variant.integer_value,
                      (a_host_large_integer)result);
    if (maintain_expression) constant->expr = expr;
  }  /* if */
  constant->type = expr->type;
}  /* fold_is_same */


static void fold_edg_is_deducible(an_expr_node_ptr   expr,
                                  a_constant_ptr     constant)
/*
expr is an enk_builtin_operation node for an __edg_is_deducible operation.  If
the operand types are nondependent, store a boolean constant in *constant.  The
boolean constant will have value "true" if the template arguments for the
template operand are deducible from the operand type; otherwise, the constant
will have value "false".  If either of the operands is dependent, store a
ck_template_param constant in *constant.  The constant will be of the
tpck_expression variant and will point to the given expression.
*/
{
  an_expr_node_ptr  arg1 = expr->variant.builtin_operation.operands,
                    arg2 = arg1->next;
  a_template_ptr    tmpl;
  a_type_ptr        type;
  a_boolean         is_deducible_template = FALSE;

  /* eok_parens shouldn't appear here, since the construct is generated. */
  check_assertion(arg2 != NULL && arg2->next == NULL &&
                  arg1->kind == enk_template_name &&
                  arg2->kind == enk_type_operand);
  tmpl = arg1->variant.template_name;
  type = arg2->variant.type_operand.type;
  if (is_template_dependent_type(type) ||
      is_nonreal_template_symbol(symbol_for(tmpl)) ||
      (tmpl->kind == templk_template_template_param &&
       bound_template_template_argument(symbol_for(tmpl),
                                        &is_deducible_template) == NULL)) {
    /* The type or template operand is still dependent for deduction. */
    clear_constant(constant, ck_template_param);
    set_template_param_constant_kind(constant, tpck_expression);
    constant->variant.template_param.variant.expr = expr;
  } else {
    clear_constant(constant, ck_integer);
    set_integer_value(&constant->variant.integer_value,
                      is_template_deducible_from(tmpl, type));
  }  /* if */
  constant->type = expr->type;
}  /* fold_edg_is_deducible */


static void fold_builtin_has_attribute(an_expr_node_ptr   expr,
                                       a_constant_ptr     constant,
                                       a_boolean          maintain_expression)
/*
expr is an enk_builtin_operation node for a __builtin_has_attribute operation.
The returned boolean constant will have value "true" if the attribute(s)
attached to the second operand appertain to the first operand; otherwise, the
constant will have value "false".  Note that a template-dependent first
operand is checked for attributes (i.e., the operation applies to the template
parameter, not the template argument it represents).  If maintain_expression
is TRUE, the backing expression for the returned constant will be set as well.
*/
{
  an_expr_node_ptr  arg1 = expr->variant.builtin_operation.operands,
                    arg2 = arg1->next;
  a_boolean         result = FALSE;
  an_attribute_ptr  target_ap, ap;
  a_source_correspondence
                    *scp = NULL;

  /* eok_parens shouldn't appear here, since the construct is generated. */
  check_assertion(arg1 != NULL && arg2 != NULL && arg2->next == NULL &&
                  arg2->kind == (an_expr_node_kind)enk_constant);
  target_ap = arg2->variant.constant.ptr->source_corresp.attributes;
  arg1 = skip_parens(arg1);
  if (arg1 != NULL && is_operation_node(arg1) &&
      (node_operator_is(arg1, eok_dot_field) ||
       node_operator_is(arg1, eok_points_to_field) ||
       node_operator_is(arg1, eok_dot_static) ||
       node_operator_is(arg1, eok_points_to_static))) {
    /* GCC uses constructs such as "((A*)0)->m" to see if an attribute is
       applied to a field. */
    arg1 = skip_parens(arg1->variant.operation.operands->next);
  }  /* if */
  switch (arg1->kind) {
    case enk_routine:
      scp = &arg1->variant.routine.ptr->source_corresp;
      break;
    case enk_variable:
      scp = &arg1->variant.variable.ptr->source_corresp;
      break;
    case enk_type_operand:
      scp = &arg1->variant.type_operand.type->source_corresp;
      break;
    case enk_field:
      scp = &arg1->variant.field.ptr->source_corresp;
      break;
    case enk_operation:
      /* Not really sure what entity is being tested in a generic operation,
         so return false. */
      break;
    default:
      unexpected_condition();
  }  /* switch */
  if (scp != NULL) {
    for (ap = scp->attributes; ap != NULL; ap = ap->next) {
      if (target_ap->kind == ap->kind) {
        if (target_ap->arguments == NULL) {
          /* If the attribute we're looking for doesn't have any arguments,
             consider it a match (e.g., "aligned" matches "aligned(X)"). */
          result = TRUE;
          break;
        } else if (ap->arguments != NULL &&
                   target_ap->arguments->kind == ap->arguments->kind) {
          /* See if the two arguments are the same (e.g., "aligned(4)" and
             "aligned(4)".  Consider a template-dependent argument to be a
             match. */
          if (attribute_is_template_dependent(target_ap) ||
              equivalent_attributes(target_ap, ap, /*ignore_family=*/TRUE)) {
            result = TRUE;
            break;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  clear_constant(constant, (a_constant_repr_kind)ck_integer);
  set_integer_value(&constant->variant.integer_value,
                    (a_host_large_integer)result);
  if (maintain_expression) constant->expr = expr;
  constant->type = expr->type;
}  /* fold_builtin_has_attribute */


static void fold_is_layout_compatible(an_expr_node_ptr   expr,
                                      a_constant_ptr     constant,
                                      a_boolean          maintain_expression)
/*
expr is an enk_builtin_operation node for an __is_layout_compatible
operation.  If the operand types are nondependent, store a boolean constant in
*constant.  The boolean constant will have value "true" if the two operands
(types) are layout-compatible.  If either of the operand types is dependent,
store a ck_template_param constant in *constant.  The constant will be of the
tpck_expression variant and will point to the given expression.  If
maintain_expression is TRUE, the backing expression for the returned constant
will be set as well.
*/
{
  an_expr_node_ptr  arg1 = expr->variant.builtin_operation.operands,
                    arg2 = arg1->next;
  a_type_ptr        type1, type2;

  /* eok_parens shouldn't appear here, since the operands are types. */
  check_assertion(arg1 != NULL && arg2 != NULL && arg2->next == NULL &&
                  arg1->kind == (an_expr_node_kind)enk_type_operand &&
                  arg2->kind == (an_expr_node_kind)enk_type_operand);
  type1 = arg1->variant.type_operand.type;
  type2 = arg2->variant.type_operand.type;
  if (is_template_dependent_type(type1) ||
      is_template_dependent_type(type2)) {
    make_template_param_expr_constant(expr, constant);
  } else {
    a_boolean  result = types_are_layout_compatible(type1, type2);
    clear_constant(constant, (a_constant_repr_kind)ck_integer);
    set_integer_value(&constant->variant.integer_value,
                      (a_host_large_integer)result);
    if (maintain_expression) constant->expr = expr;
  }  /* if */
  constant->type = expr->type;
}  /* fold_is_layout_compatible */


static void fold_is_pointer_interconvertible_base_of(
                                       an_expr_node_ptr   expr,
                                       a_constant_ptr     constant,
                                       a_boolean          maintain_expression)
/*
expr is an enk_builtin_operation node for a
__is_pointer_interconvertible_base_of operation.  If the operand types
are nondependent, store a boolean constant in *constant.  Let B denote the
first operand (type) and D the second one.  The boolean constant will have
value "true" if:
  - B and D are identical non-union class types (ignoring qualifiers), or
  - B is an unambiguous base of standard-layout class D, and each D object is
    pointer-interconvertible with its B subobject.
If either of the operand types is dependent, store a ck_template_param constant
in *constant.  The constant will be of the tpck_expression variant and will
point to the given expression.  If maintain_expression is TRUE, the backing
expression for the returned constant will be set as well.
*/
{
  an_expr_node_ptr  arg1 = expr->variant.builtin_operation.operands,
                    arg2 = arg1->next;
  a_type_ptr        type1, type2;

  /* eok_parens shouldn't appear here, since the operands are types. */
  check_assertion(arg1 != NULL && arg2 != NULL && arg2->next == NULL &&
                  arg1->kind == (an_expr_node_kind)enk_type_operand &&
                  arg2->kind == (an_expr_node_kind)enk_type_operand);
  type1 = arg1->variant.type_operand.type;
  type2 = arg2->variant.type_operand.type;
  if (is_template_dependent_type(type1) ||
      is_template_dependent_type(type2)) {
    make_template_param_expr_constant(expr, constant);
  } else {
    a_boolean  result = FALSE;
    type1 = skip_typerefs(type1);
    type2 = skip_typerefs(type2);
    if (is_class_or_struct(type1) && is_class_or_struct(type2)) {
      if (same_entities(type1, type2)) {
        result = TRUE;
      } else {
        /* Pointer-interconvertibility is characterized by a zero offset in
           this case (when the derived class has standard layout). */
        a_base_class  *bcp = find_base_class_of(type2, type1);
        if (bcp != NULL && !bcp->ambiguous && !bcp->is_virtual &&
            symbol_supplement_for_class(type2)->standard_layout &&
            bcp->offset == 0) {
          result = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
    clear_constant(constant, (a_constant_repr_kind)ck_integer);
    set_integer_value(&constant->variant.integer_value,
                      (a_host_large_integer)result);
    if (maintain_expression) constant->expr = expr;
  }  /* if */
  constant->type = expr->type;
}  /* fold_is_pointer_interconvertible_base_of */


static void fold_is_pointer_interconvertible_with_class(
                                          an_expr_node_ptr expr,
                                          a_constant_ptr   constant,
                                          a_boolean        maintain_expression,
                                          a_boolean        *not_a_constant)
/*
expr is an enk_builtin_operation node for an
__is_pointer_interconvertible_with_class (MS) or
__builtin_is_pointer_interconvertible_with_class (GCC) operation.  If the
operand types are such that the operation can be folded, store a boolean
constant in *constant.  If the pointer-to-member operand is non-constant (e.g.,
a variable), set *not_a_constant to indicate that the builtin cannot be folded
here (though it may be folded later in the interpreter when the value of the
variable is known).

The caller has verified that the proper number of arguments are present but
additional error checking is performed here.
*/
{
  an_expr_node_ptr  pm_arg, arg = expr->variant.builtin_operation.operands;
  a_type_ptr        type1, type2;
  a_boolean         result, err = FALSE;
  a_builtin_operation_kind
                    op = expr->variant.builtin_operation.kind;

  if (op == bok_is_pointer_interconvertible_with_class) {
    check_assertion(arg != NULL && arg->next != NULL &&
                    arg->next->next == NULL &&
                    arg->kind == (an_expr_node_kind)enk_type_operand);
    type1 = arg->variant.type_operand.type;
    pm_arg = arg->next;
  } else {
    check_assertion(op == bok_builtin_is_pointer_interconvertible_with_class &&
                    arg != NULL && arg->next == NULL);
    pm_arg = arg;
    type1 = skip_typerefs(pm_arg->type);
    if (type_is(type1, tk_ptr_to_member)) {
      type1 = type1->variant.ptr_to_member.class_of_which_a_member;
    } else {
      type1 = error_type(); /* Error issued below. */
    }  /* if */
  }  /* if */
  type2 = pm_arg->type;
  /* Caller has already checked for error nodes. */
  if (is_template_dependent_type(type1) ||
      is_template_dependent_type(type2)) {
    make_template_param_expr_constant(expr, constant);
  } else {
    type1 = skip_typerefs(type1);
    type2 = skip_typerefs(type2);
    if (!is_class_struct_union_type(type1)) {
      expr_pos_error(ec_exp_class_type, &arg->position);
      err = TRUE;
    } else if (is_incomplete_type(type1)) {
      expr_issue_incomplete_type_diag(&arg->position, type1);
      err = TRUE;
    } else if (!type_is(type2, tk_ptr_to_member)) {
      expr_pos_error(ec_exp_pointer_to_member, &pm_arg->position);
      err = TRUE;
    } else {
      err = FALSE;
      a_type_ptr class_type =
                          type2->variant.ptr_to_member.class_of_which_a_member;
      a_constant_ptr pmcon = NULL;
      if (!is_constant_node(pm_arg) ||
          !constant_is(node_constant(pm_arg), ck_ptr_to_member)) {
        /* Can't fold if the argument isn't constant. */
        *not_a_constant = TRUE;
        result = FALSE;
      } else if (find_base_class_of(type1, class_type) != NULL) {
        /* Types are not related. */
        result = FALSE;
      } else if ((pmcon = node_constant(pm_arg),
                  pmcon->variant.ptr_to_member.is_function_ptr ||
                  pmcon->variant.ptr_to_member.variant.field == NULL ||
                  pmcon->variant.ptr_to_member.variant.field->offset != 0 ||
                  (!is_union_type(class_type) &&
                   (!class_symbol_supp(symbol_for(class_type))->
                                                             standard_layout ||
                   !class_symbol_supp(symbol_for(type1))->standard_layout)))) {
        /* Non-standard-layout classes and pointer-to-member functions
           elicit a "false" result.  If a field is designated but its
           offset is not zero, its address is not "interconvertible"
           with that of its parent object. */
        result = FALSE;
      } else {
        result = TRUE;
      }  /* if */
      clear_constant(constant, (a_constant_repr_kind)ck_integer);
      set_integer_value(&constant->variant.integer_value,
                        (a_host_large_integer)result);
      if (maintain_expression) constant->expr = expr;
    }  /* if */
  }  /* if */
  if (err) {
    set_error_constant(constant);
  }  /* if */
  constant->type = expr->type;
}  /* fold_is_pointer_interconvertible_with_class */


static void fold_is_corresponding_member(an_expr_node_ptr expr,
                                         a_constant_ptr   constant,
                                         a_boolean        maintain_expression,
                                         a_boolean        *not_a_constant)
/*
expr is an enk_builtin_operation node for an __is_corresponding_member (MS) or
__builtin_is_corresponding_member (GCC) operation.  If the operand types are
nondependent, store a boolean constant in *constant that corresponds to the
result of the std::is_corresponding_member function.  If the pointer-to-member
operands are non-constant (e.g., a variable), set *not_a_constant to indicate
that the builtin cannot be folded here (though it may be folded later in the
interpreter when the value of the variable is known).

The caller has verified that there are the proper number of arguments but
additional error checking is performed here.
*/
{
  an_expr_node_ptr  arg = expr->variant.builtin_operation.operands;
  an_expr_node_ptr  pm1, pm2, pm_args;
  a_type_ptr        class1 = error_type(), class2 = error_type();
  a_type_ptr        pm_type1, pm_type2;
  a_boolean         result, err = FALSE;
  a_builtin_operation_kind
                    op = expr->variant.builtin_operation.kind;

  check_assertion(arg != NULL && arg->next != NULL);
  if (op == bok_is_corresponding_member) {
    /* Four arguments, first two are types, next two are pointer-to-members. */
    check_assertion(arg->next->next != NULL &&
                    arg->next->next->next != NULL &&
                    arg->next->next->next->next == NULL &&
                    arg->kind == (an_expr_node_kind)enk_type_operand &&
                    arg->next->kind == (an_expr_node_kind)enk_type_operand);
    class1 = arg->variant.type_operand.type;
    class2 = arg->next->variant.type_operand.type;
    pm_args = arg->next->next;
  } else {
    /* Two pointer-to-member arguments. */
    check_assertion(op == bok_builtin_is_corresponding_member &&
                    arg->next->next == NULL);
    pm_args = arg;
  }  /* if */
  pm1 = pm_args;
  pm2 = pm1->next;
  pm_type1 = skip_typerefs(pm1->type);
  pm_type2 = skip_typerefs(pm2->type);
  if (op == bok_builtin_is_corresponding_member) {
    /* Get class types from the pointer-to-member arguments. */
    if (type_is(pm_type1, tk_ptr_to_member)) {
      class1 = pm_type1->variant.ptr_to_member.class_of_which_a_member;
    }  /* if */
    if (type_is(pm_type2, tk_ptr_to_member)) {
      class2 = pm_type2->variant.ptr_to_member.class_of_which_a_member;
    }  /* if */
  }  /* if */
  if (is_template_dependent_type(class1) ||
      is_template_dependent_type(class2) ||
      is_template_dependent_type(pm_type1) ||
      is_template_dependent_type(pm_type2)) {
    make_template_param_expr_constant(expr, constant);
  } else {
    class1 = skip_typerefs(class1);
    class2 = skip_typerefs(class2);
    if (!is_class_struct_union_type(class1)) {
      expr_pos_error(ec_exp_class_type, &arg->position);
      err = TRUE;
    } else if (is_incomplete_type(class1)) {
      expr_issue_incomplete_type_diag(&arg->position, class1);
      err = TRUE;
    } else if (!is_class_struct_union_type(class2)) {
      expr_pos_error(ec_exp_class_type, &arg->next->position);
      err = TRUE;
    } else if (is_incomplete_type(class2)) {
      expr_issue_incomplete_type_diag(&arg->next->position, class2);
      err = TRUE;
    } else if (!type_is(pm_type1, tk_ptr_to_member)) {
      expr_pos_error(ec_exp_pointer_to_member, &pm1->position);
      err = TRUE;
    } else if (!type_is(pm_type2, tk_ptr_to_member)) {
      expr_pos_error(ec_exp_pointer_to_member, &pm2->position);
      err = TRUE;
    } else {
      err = FALSE;
      if (!is_constant_node(pm1) || !is_constant_node(pm2) ||
          !constant_is(node_constant(pm1), ck_ptr_to_member) ||
          !constant_is(node_constant(pm2), ck_ptr_to_member)) {
        /* Can't fold if the arguments aren't constant. */
        *not_a_constant = TRUE;
        result = FALSE;
      } else if (find_base_class_of(pm_type1, class1) != NULL ||
                 find_base_class_of(pm_type2, class2) != NULL) {
        /* Types are not related. */
        result = FALSE;
      } else {
        a_constant_ptr pmcon1 = node_constant(pm1);
        a_constant_ptr pmcon2 = node_constant(pm2);
        if (pmcon1->variant.ptr_to_member.is_function_ptr ||
            pmcon2->variant.ptr_to_member.is_function_ptr ||
            pmcon1->variant.ptr_to_member.variant.field == NULL ||
            pmcon2->variant.ptr_to_member.variant.field == NULL ||
            !class_symbol_supp(symbol_for(class1))->standard_layout ||
            !class_symbol_supp(symbol_for(class2))->standard_layout ||
            pmcon1->variant.ptr_to_member.variant.field->offset !=
                         pmcon2->variant.ptr_to_member.variant.field->offset ||
            pmcon1->variant.ptr_to_member.variant.field->offset >=
                               common_initial_sequence_limit(class1, class2)) {
          /* Non-standard-layout classes and pointer-to-member functions
             elicit a "false" result.  Members "correspond" if they are
             within the "common initial sequence" and have the same offset. */
          result = FALSE;
        } else {
          result = TRUE;
        }  /* if */
      }  /* if */
      clear_constant(constant, (a_constant_repr_kind)ck_integer);
      set_integer_value(&constant->variant.integer_value,
                        (a_host_large_integer)result);
      if (maintain_expression) constant->expr = expr;
    }  /* if */
  }  /* if */
  if (err) {
    set_error_constant(constant);
  }  /* if */
  constant->type = expr->type;
}  /* fold_is_corresponding_member */


static void fold_array_intrinsic(an_expr_node_ptr   expr,
                                 a_constant_ptr     constant,
                                 a_boolean          maintain_expression)
/*
expr is an enk_builtin_operation node for an __array_rank or __array_extent
operation.  *constant is set to a (size_t) number that represents the rank
(i.e., number of dimensions) in the array type for __array_rank and the number
of elements in the specified dimension for __array_extent.  If the type is not
an array type a zero is stored.  A template parameter constant is returned for
template dependent cases.  If maintain_expression is TRUE, the backing
expression for the returned constant will be set as well.  Always folds to
a constant (though it may be an error constant in some __array_extent cases).
*/
{
  an_expr_node_ptr  arg = expr->variant.builtin_operation.operands;
  a_type_ptr        type;
  a_boolean         is_array_extent = expr->variant.builtin_operation.kind ==
                                    (a_builtin_operation_kind)bok_array_extent;
  a_boolean         err = FALSE;

  /* eok_parens shouldn't appear here, since the construct is generated. */
  check_assertion(arg != NULL && node_is(arg, enk_type_operand) &&
                  (arg->next == NULL || is_array_extent));
  type = type_operand_type(arg);
  type = skip_typerefs(type);
  if (is_template_dependent_type(type) ||
      (is_array_extent && is_template_dependent_type(arg->next->type))) {
    clear_constant(constant, (a_constant_repr_kind)ck_template_param);
    set_template_param_constant_kind(
                   constant, (a_template_param_constant_kind)tpck_expression);
    constant->variant.template_param.variant.expr = expr;
  } else {
    a_targ_size_t    result;
    an_expr_node_ptr dim = arg->next;
    a_constant_ptr   con;
    if (is_array_extent) {
      /* Return the number of elements in the specified dimension. */
      if (is_constant_node(dim) &&
          constant_is((con = node_constant(dim)), ck_integer) &&
          is_integral_type(con->type)) {
        if (sign_of_integer_constant(con) < 0) {
          /* Can't have a negative dimension. */
          err = TRUE;
        } else {
          a_host_large_unsigned val =
                                 unsigned_value_of_integer_constant(con, &err);
          result = array_extent(type, val);
        }  /* if */
      } else {
        /* Only a constant unsigned integral argument can be folded. */
        err = TRUE;
      }  /* if */
    } else {
      result = array_rank(type);
    }  /* if */
    if (err) {
      expr_pos_error(ec_dim_not_const_unsigned_int, &dim->position);
      clear_constant(constant, (a_constant_repr_kind)ck_error);
    } else {
      clear_constant(constant, (a_constant_repr_kind)ck_integer);
      set_unsigned_integer_value(&constant->variant.integer_value,
                                 (a_host_large_unsigned)result);
      if (maintain_expression) constant->expr = expr;
    }  /* if */
  }  /* if */
  constant->type = expr->type;
}  /* fold_array_intrinsic */


static void fold_synthesizes_from_spaceship(an_expr_node_ptr   expr,
                                            a_constant_ptr     constant,
                                            a_boolean          maintain_expr)
/*
expr is an enk_builtin_operation node for one of the following Clang 22
intrinsics:
    __builtin_lt_synthesized_from_spaceship
    __builtin_gt_synthesized_from_spaceship
    __builtin_le_synthesized_from_spaceship
    __builtin_ge_synthesized_from_spaceship
Store a boolean constant in *constant whose value is "true" if the indicated
operator ("<" for "lt", ">" for "gt", "<=" for "le", or ">=" for "ge") applied
to the operand types would involve a spaceship operator.  If either of the
operand types is dependent, store a ck_template_param constant in *constant.
The constant will be of the tpck_expression variant and will point to the
given expression.  If maintain_expr is TRUE, the backing expression for the
returned constant will be set as well.
*/
{
  an_expr_node_ptr  arg1 = expr->variant.builtin_operation.operands,
                    arg2 = arg1->next;
  a_type_ptr        type1, type2;

  /* eok_parens shouldn't appear here, since types cannot be parenthesized. */
  check_assertion(arg1 != NULL && arg2 != NULL && arg2->next == NULL &&
                  arg1->kind == enk_type_operand &&
                  arg2->kind == enk_type_operand);
  type1 = arg1->variant.type_operand.type;
  type2 = arg2->variant.type_operand.type;
  if (is_template_dependent_type(type1) ||
      is_template_dependent_type(type2)) {
    clear_constant(constant, ck_template_param);
    set_template_param_constant_kind(constant, tpck_expression);
    constant->variant.template_param.variant.expr = expr;
  } else {
    an_opname_kind  rel_op;
    switch (expr->variant.builtin_operation.kind) {
      case bok_builtin_lt_synthesizes_from_spaceship:
        rel_op = onk_lt;
        break;
      case bok_builtin_gt_synthesizes_from_spaceship:
        rel_op = onk_gt;
        break;
      case bok_builtin_le_synthesizes_from_spaceship:
        rel_op = onk_le;
        break;
      case bok_builtin_ge_synthesizes_from_spaceship:
        rel_op = onk_ge;
        break;
      default:
        unexpected_condition();
    }  /* switch */
    arg1->type_definition_needed = TRUE;
    arg2->type_definition_needed = TRUE;
    clear_constant(constant, ck_integer);
    set_integer_value(&constant->variant.integer_value,
                      (a_host_large_integer)rel_op_synthesizes_from_spaceship(
                                                       type1, type2, rel_op));
    if (maintain_expr) constant->expr = expr;
  }  /* if */
  constant->type = expr->type;
}  /* fold_synthesizes_from_spaceship */


void fold_builtin_operation_if_possible(
                              an_expr_node_ptr             expr,
                              a_constant_ptr               constant,
                              a_boolean                    maintain_expression,
                              a_source_position            *pos,
                              a_boolean                    *not_a_constant)
/*
The given expression is a node of kind enk_builtin_operation.  If any of its
operands are template-dependent, the result is not foldable and a
ck_template_param constant (of the tpck_expression variant) is stored in
*constant (*not_a_constant is set to FALSE in such cases).  Similarly, the
result is not foldable if the operands are such that the result is not a
constant (*not_a_constant is set to TRUE in those cases).  Otherwise, an
attempt is made to fold the operation and *not_a_constant is set to FALSE.  If
the folding is successful, the result is returned through *constant.  If the
folding fails, an error constant is returned through *constant and if pos is
non-NULL diagnostics are issued at the indicated position.
If maintain_expression is TRUE, the backing expression for the returned
constant is set as well.
*/
{
  a_boolean         has_error = FALSE, is_dependent = FALSE;
  an_expr_node_ptr  arg = expr->variant.builtin_operation.operands;

  /* Most built-in operations result in constants.  So we start with that
     assumption. */
  *not_a_constant = FALSE;
  check_assertion(expr->kind == (an_expr_node_kind)enk_builtin_operation);
  /* Check if an error was already encountered.  In that case, we silently
     produce an error constant. */
  for (; arg != NULL; arg =  arg->next) {
    if (arg->kind == (an_expr_node_kind)enk_error) {
      has_error = TRUE;
      break;
    } else if (!is_dependent && is_template_dependent_context() &&
               expr_is_instantiation_dependent(arg)) {
      is_dependent = TRUE;
    }  /* if */
  }  /* for */
  if (has_error) {
    clear_constant(constant, (a_constant_repr_kind)ck_error);
  } else if (is_dependent && !scope_stack_top().is_rescan) {
    /* Unsubstituted parameters may make actual folding impossible.  Produce
       a ck_template_param/tpck_expression entry instead. */
    make_template_param_expr_constant(expr, constant);
  } else {
    switch (expr->variant.builtin_operation.kind) {
      case bok_offsetof:
        fold_offsetof(expr, constant, maintain_expression, pos,
                      not_a_constant);
        break;
#if GNU_EXTENSIONS_ALLOWED
      case bok_types_compatible:
        fold_types_compatible(expr, constant, maintain_expression);
        break;
#endif /* GNU_EXTENSIONS_ALLOWED */
      case bok_has_assign:
      case bok_has_copy:
      case bok_has_nothrow_assign:
      case bok_has_nothrow_constructor:
      case bok_has_nothrow_copy:
      case bok_has_trivial_assign:
      case bok_has_trivial_constructor:
      case bok_has_trivial_copy:
      case bok_has_trivial_destructor:
      case bok_has_user_destructor:
      case bok_has_virtual_destructor:
      case bok_is_abstract:
      case bok_is_empty:
      case bok_is_pod:
      case bok_is_polymorphic:
      case bok_is_trivial:
      case bok_is_standard_layout:
      case bok_is_trivially_copyable:
      case bok_is_literal_type:
      case bok_has_trivial_move_constructor:
      case bok_has_trivial_move_assign:
      case bok_has_nothrow_move_assign:
#if MICROSOFT_EXTENSIONS_ALLOWED
      case bok_has_finalizer:
      case bok_is_sealed:
      case bok_is_simple_value_class:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      case bok_is_final:
      case bok_is_trivially_copy_assignable:
      case bok_has_unique_object_representations:
      case bok_is_aggregate:
      case bok_is_trivially_relocatable:
      case bok_is_bitwise_cloneable:
      case bok_is_trivially_equality_comparable:
      case bok_builtin_is_implicit_lifetime:
      case bok_builtin_is_structural:
        /* Various type trait helpers that require their single argument not to
           be an incomplete class type (or array thereof). */
        fold_unary_type_trait_helper(expr, constant, maintain_expression, pos,
                                     /*complete_class_property=*/TRUE);
        break;
      case bok_is_class:
      case bok_is_enum:
      case bok_is_scoped_enum:
      case bok_is_function:
      case bok_is_array:
      case bok_is_union:
#if MICROSOFT_EXTENSIONS_ALLOWED
      case bok_is_delegate:
      case bok_is_interface_class:
      case bok_is_ref_array:
      case bok_is_ref_class:
      case bok_is_value_class:
      case bok_is_win_class:
      case bok_is_win_interface:
      case bok_is_valid_winrt_type:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      case bok_is_arithmetic:
      case bok_is_complete_type:
      case bok_is_compound:
      case bok_is_const:
      case bok_is_floating_point:
      case bok_is_fundamental:
      case bok_is_integral:
      case bok_is_lvalue_reference:
      case bok_is_member_function_pointer:
      case bok_is_member_object_pointer:
      case bok_is_member_pointer:
      case bok_is_object:
      case bok_is_pointer:
      case bok_is_reference:
      case bok_is_rvalue_reference:
      case bok_is_scalar:
      case bok_is_signed:
      case bok_is_unsigned:
      case bok_is_void:
      case bok_is_volatile:
      case bok_is_bounded_array:
      case bok_is_unbounded_array:
      case bok_is_referenceable:
        /* Various type trait helpers that take a single argument. */
        fold_unary_type_trait_helper(expr, constant, maintain_expression, pos,
                                     /*complete_class_property=*/FALSE);
        break;
      case bok_is_base_of:
        fold_is_base_of(expr, constant, maintain_expression,
                        /*is_virtual_base_of=*/FALSE);
        break;
      case bok_builtin_is_virtual_base_of:
        fold_is_base_of(expr, constant, maintain_expression,
                        /*is_virtual_base_of=*/TRUE);
        break;
      case bok_is_convertible_to:
      case bok_is_convertible:
      case bok_is_nothrow_convertible:
        fold_is_convertible_to(expr, constant, maintain_expression);
        break;
      case bok_reference_binds_to_temporary:
      case bok_reference_constructs_from_temporary:
      case bok_reference_converts_from_temporary:
        fold_reference_binds_to_temporary(expr, constant, maintain_expression);
        break;
      case bok_is_invocable:
      case bok_is_nothrow_invocable:
        fold_is_invocable(expr, constant, maintain_expression);
        break;
      case bok_is_constructible:
      case bok_is_nothrow_constructible:
      case bok_is_trivially_constructible:
        fold_is_constructible(expr, constant, maintain_expression);
        break;
      case bok_is_destructible:
      case bok_is_nothrow_destructible:
      case bok_is_trivially_destructible:
        fold_is_destructible(expr, constant, maintain_expression);
        break;
      case bok_is_nothrow_assignable:
      case bok_is_trivially_assignable:
      case bok_is_assignable:
      case bok_is_assignable_no_precondition_check:
        fold_is_assignable(expr, constant, maintain_expression);
        break;
      case bok_builtin_addressof:
        *not_a_constant = !fold_constexpr_expr(expr, constant,
                                               /*is_constant_evaluated=*/FALSE,
                                               /*force_prvalue=*/FALSE);
        break;
      case bok_is_same:
      case bok_is_same_as:
        fold_is_same(expr, constant, maintain_expression);
        break;
      case bok_builtin_has_attribute:
        fold_builtin_has_attribute(expr, constant, maintain_expression);
        break;
      case bok_is_layout_compatible:
        fold_is_layout_compatible(expr, constant, maintain_expression);
        break;
      case bok_is_pointer_interconvertible_base_of:
        fold_is_pointer_interconvertible_base_of(
                                          expr, constant, maintain_expression);
        break;
      case bok_is_pointer_interconvertible_with_class:
      case bok_builtin_is_pointer_interconvertible_with_class:
        fold_is_pointer_interconvertible_with_class(expr, constant,
                                                    maintain_expression,
                                                    not_a_constant);
        break;
      case bok_is_corresponding_member:
      case bok_builtin_is_corresponding_member:
        fold_is_corresponding_member(expr, constant, maintain_expression,
                                     not_a_constant);
        break;
      case bok_edg_is_deducible:
        fold_edg_is_deducible(expr, constant);
        break;
      case bok_array_rank:
      case bok_array_extent:
        /* Some clang-specific intrinsics can't be handled as normal type
           traits (because their return is size_t instead of boolean and
           __array_extent takes a second argument). */
        fold_array_intrinsic(expr, constant, maintain_expression);
        break;
      case bok_builtin_lt_synthesizes_from_spaceship:
      case bok_builtin_gt_synthesizes_from_spaceship:
      case bok_builtin_le_synthesizes_from_spaceship:
      case bok_builtin_ge_synthesizes_from_spaceship:
        fold_synthesizes_from_spaceship(expr, constant, maintain_expression);
        break;
      default:
        unexpected_condition();
    }  /* switch */
  }  /* if */
}  /* fold_builtin_operation_if_possible */

#if BUILTIN_FUNCTIONS_ENABLED

a_boolean is_foldable_gnu_builtin_function(a_routine_ptr rp,
                                           a_boolean     *pseudo_call)
/*
Return TRUE if and only if the routine rp is a GNU-style built-in function
and calls to that function might be valid constant-expressions.
*pseudo_call is set to TRUE if the arguments to the built-in function call
are not treated like standard call arguments (e.g., if they behave like
sizeof arguments); otherwise, *pseudo_call is set to FALSE.
pseudo_call can be NULL if that information is not needed.
*/
{
  a_boolean  result = FALSE;

  if (pseudo_call != NULL) *pseudo_call = FALSE;
  if (rp != NULL && is_gnu_builtin_function(rp)) {
    switch (rp->variant.builtin_function_kind) {
      case bfk_classify_type:
      case bfk_constant_p:
      case bufk_choose_expr:
#if GCC_BUILTIN_VARARGS
      case bfk_stdarg_start:
      case bfk_va_start:
      case bfk_va_arg:
      case bfk_va_end:
      case bfk_va_copy:
      case bfk_varargs_start:
#endif /* GCC_BUILTIN_VARARGS */
        if (pseudo_call != NULL) *pseudo_call = TRUE;
        FALLTHROUGH
      case bfk_huge_valf:
      case bfk_huge_val:
      case bfk_huge_vall:
#if TARG_HAS_IEEE_FLOATING_POINT
      case bfk_nanf:
      case bfk_nan:
      case bfk_nanl:
      case bfk_nansf:
      case bfk_nans:
      case bfk_nansl:
      case bfk_inff:
      case bfk_inf:
      case bfk_infl:
      case bfk_isnan:
      case bfk_isnanf:
      case bfk_isnanl:
      case bfk_isinf:
      case bfk_isinff:
      case bfk_isinfl:
      case bfk_isfinite:
      case bfk_isnormal:
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
      case bfk_fpclassify:
      case bfk_ffs:
      case bfk_ffsl:
      case bfk_clz:
      case bfk_clzl:
      case bfk_ctz:
      case bfk_ctzl:
      case bfk_expect:
      case bfk_expect_with_probability:
      case bfk_popcount:
      case bfk_popcountl:
      case bfk_parity:
      case bfk_parityl:
#if LONG_LONG_ALLOWED
      case bfk_ffsll:
      case bfk_clzll:
      case bfk_ctzll:
      case bfk_popcountll:
      case bfk_parityll:
#endif /* LONG_LONG_ALLOWED */
      case bfk_strlen:
      case bfk_is_string_literal:
      case bfk_abs:
      case bfk_fabs:
      case bfk_fabsf:
      case bfk_fabsl:
      case bfk_pow:
      case bfk_powf:
      case bfk_powl:
      case bfk_signbit:
      case bfk_signbitf:
      case bfk_signbitl:
      case bfk_atomic_always_lock_free:
      case bfk_atomic_is_lock_free:
      case bfk___c11_atomic_is_lock_free:
      case bfk_bswap16:
      case bfk_bswap32:
      case bfk_bswap64:
      case bfk_ceil:
      case bfk_ceilf:
      case bfk_ceill:
      case bfk_is_constant_evaluated:
      case bfk_copysign:
      case bfk_copysignf:
      case bfk_copysignl:
      case bfk_nondeterministic_value:
      case bfk_COLUMN:
      case bfk_LINE:
      case bfk_FILE:
      case bufk_FILE_NAME:
      case bfk_FUNCTION:
      case bufk_FUNCSIG:
      case bfk_add_overflow_p:
      case bfk_add_overflow:
      case bfk_uadd_overflow:
      case bfk_uaddl_overflow:
      case bfk_uaddll_overflow:
      case bfk_sadd_overflow:
      case bfk_saddl_overflow:
      case bfk_saddll_overflow:
      case bfk_sub_overflow_p:
      case bfk_sub_overflow:
      case bfk_ssub_overflow:
      case bfk_ssubl_overflow:
      case bfk_ssubll_overflow:
      case bfk_usub_overflow:
      case bfk_usubl_overflow:
      case bfk_usubll_overflow:
      case bfk_mul_overflow_p:
      case bfk_mul_overflow:
      case bfk_smul_overflow:
      case bfk_smull_overflow:
      case bfk_smulll_overflow:
      case bfk_umul_overflow:
      case bfk_umull_overflow:
      case bfk_umulll_overflow:
        result = TRUE;
        break;
      case bfk_assume_aligned:
      case bfk_cpu_supports:
      case bfk_cpu_is:
        /* These are never actually "folded", but the "folding" mechanism
           allows for error checking of arguments (in
           fold_gnu_builtin_function_call_if_possible). */
        result = TRUE;
        break;
      default:
        /* Nothing to be done. */
        break;
    }  /* switch */
  }  /* if */
  return result;
}  /* is_foldable_gnu_builtin_function */


static
a_boolean fold_bit_count_operation_if_possible(a_routine_ptr     rp,
                                               an_expr_node_ptr  arg,
                                               a_constant        *result_con)
/*
rp represents a GNU builtin bit counting function which is being applied to
the given argument.  If the argument is a constant integer, set *result_con
to the result of that count and return TRUE.  Otherwise, return FALSE.

The bit counting functions are (for "unsigned int" arguments):
  ffs: index of the least-significant 1 (or zero if there is none)
  clz: number of leading zeros
  ctz: number of trailing zeros
  popcount: number of ones
  parity: number of ones modulo 2
Variants for unsigned long (e.g., ffsl) and unsigned long long (e.g., ctzll)
arguments are available in all cases.

This routine will fail to fold the operation (and hence return FALSE) if the
argument cannot be represented in a_host_large_unsigned.
*/
{
  a_boolean   success = FALSE;
  a_type_ptr  result_type;

  check_assertion(is_gnu_builtin_function(rp));
  result_type = return_type_of(rp->type);
  result_type = skip_typerefs(result_type);
  check_assertion(result_type->kind == (a_type_kind)tk_integer);
  if (is_constant_node(arg) && node_constant_is(arg, ck_integer)) {
    a_constant_ptr         cp = node_constant(arg);
    a_boolean              err;
    a_host_large_unsigned  val = unsigned_value_of_integer_constant(cp, &err);
    if (!err) {
      a_targ_size_t  n_bits = skip_typerefs(cp->type)->size*targ_char_bit;
      a_targ_size_t  k, result = 0;
      /* Traverse the bits of the argument.  n_bits may be larger than the
         number of bits in a_host_large_unsigned but that is not a problem:
         The overflow case was avoided with the test for !err above and so
         the excess bits can be assumed to be zeros. */
      for (k = 0; k < n_bits; ++k, val >>= 1) {
        a_boolean  bit = ((val & 1) != 0);
        switch (rp->variant.builtin_function_kind) {
          case bfk_ffs:
          case bfk_ffsl:
#if LONG_LONG_ALLOWED
          case bfk_ffsll:
#endif /* LONG_LONG_ALLOWED */
            /* Index of the least significant 1-bit. */
            if (bit) {
              result = k+1;
              goto count_done;
            }  /* if */
            break;
          case bfk_clz:
          case bfk_clzl:
#if LONG_LONG_ALLOWED
          case bfk_clzll:
#endif /* LONG_LONG_ALLOWED */
            /* Count of leading zeros. */
            result = bit ? 0 : result+1;
            break;
          case bfk_ctz:
          case bfk_ctzl:
#if LONG_LONG_ALLOWED
          case bfk_ctzll:
#endif /* LONG_LONG_ALLOWED */
            /* Count of trailing zeros. */
            if (bit) {
              goto count_done;
            } else {
              ++result;
            }  /* if */
            break;
          case bfk_popcount:
          case bfk_popcountl:
#if LONG_LONG_ALLOWED
          case bfk_popcountll:
#endif /* LONG_LONG_ALLOWED */
            /* Count of ones. */
            if (bit) result += 1;
            break;
          case bfk_parity:
          case bfk_parityl:
#if LONG_LONG_ALLOWED
          case bfk_parityll:
#endif /* LONG_LONG_ALLOWED */
            /* Count of ones. */
            if (bit) result = (result+1) & 1;
            break;
          default:
            unexpected_condition();
        }  /* switch */
      }  /* for */
count_done:
      set_unsigned_integer_constant(result_con, (a_host_large_unsigned)result,
                                    result_type->variant.integer.int_kind);
      success = TRUE;
    }  /* if */
  }  /* if */
  return success;
}  /* fold_bit_count_operation_if_possible */

#if TARG_HAS_IEEE_FLOATING_POINT
#if C99_IL_EXTENSIONS_SUPPORTED

static a_boolean fold_fptest_if_possible(a_routine_ptr     rp,
                                         an_expr_node_ptr  arg,
                                         a_constant        *result_con)
/*
rp represents a GNU builtin floating-point test function (__builtin_isnan or
__builtin_isinf) which is being applied to the given argument.  If the argument
is a constant, set *result_con to the result of the test and return TRUE.
Otherwise, return FALSE.
*/
{
  a_boolean   success = FALSE, unknown_result = FALSE;
  a_type_ptr  result_type;

  check_assertion(is_gnu_builtin_function(rp));
  result_type = return_type_of(rp->type);
  result_type = skip_typerefs(result_type);
  check_assertion(result_type->kind == (a_type_kind)tk_integer);
  if (is_constant_node(arg) && node_constant_is(arg, ck_float)) {
    a_constant_ptr         cp = node_constant(arg);
    a_float_kind           float_kind =
                                   skip_typerefs(cp->type)->variant.float_kind;
    a_host_large_unsigned  result = 0;
    switch (rp->variant.builtin_function_kind) {
      case bfk_isnan:
      case bfk_isnanf:
      case bfk_isnanl:
        result = fp_is_nan(&cp->variant.float_value, float_kind);
        break;
      case bfk_isinf:
      case bfk_isinff:
      case bfk_isinfl:
        result = fp_is_infinity(&cp->variant.float_value, float_kind);
        break;
      case bfk_isfinite:
        result = !fp_is_infinity(&cp->variant.float_value, float_kind) &&
                 !fp_is_nan(&cp->variant.float_value, float_kind);
        break;
      case bfk_isnormal:
        result = fp_is_normalized(&cp->variant.float_value, float_kind,
                                  &unknown_result);
        break;
      case bfk_signbit:
      case bfk_signbitf:
      case bfk_signbitl:
        if (fp_is_nan(&cp->variant.float_value, float_kind)) {
          /* We don't currently attempt to determine the sign bit of a NaN
             value.  (This matches Clang but not GCC.) */
          unknown_result = TRUE;
        } else {
          result = fp_signbit(float_kind, &cp->variant.float_value);
        }  /* if */
        break;
      default:
        unexpected_condition();
    }  /* switch */
    if (!unknown_result) {
      set_integer_constant(result_con, (a_host_large_integer)result,
                           result_type->variant.integer.int_kind);
      success = TRUE;
    }  /* if */
  }  /* if */
  return success;
}  /* fold_fptest_if_possible */


static a_boolean fold_copysign_if_possible(a_routine_ptr     rp,
                                           an_expr_node_ptr  arg1,
                                           an_expr_node_ptr  arg2,
                                           a_constant        *result_con)
/*
rp represents a builtin __builtin_copysign* routine that is being applied to
the given two arguments.  "Copy" the sign bit from the second argument to the
first (if both are constants) and set *result_con to the resulting value and
return TRUE.  Otherwise, return FALSE.
*/
{
  a_boolean   success = FALSE;

  check_assertion(is_gnu_builtin_function(rp));
  if (is_constant_node(arg1) && node_constant_is(arg1, ck_float) &&
      is_constant_node(arg2) && node_constant_is(arg2, ck_float)) {
    a_constant_ptr  cp1 = node_constant(arg1);
    a_constant_ptr  cp2 = node_constant(arg2);
    a_float_kind    fkind = skip_typedefs(cp1->type)->variant.float_kind;
    check_assertion(fkind == skip_typedefs(cp2->type)->variant.float_kind &&
                    fkind ==
                         skip_typedefs(result_con->type)->variant.float_kind);
    /* If the sign bit is the same in the two constants, then there's no
       need to change, otherwise negate the first argument. */
    success = TRUE;
    if (fp_signbit(fkind, &cp1->variant.float_value) ==
        fp_signbit(fkind, &cp2->variant.float_value)) {
      result_con->variant.float_value = cp1->variant.float_value;
    } else {
      a_boolean err, dep;
      fp_negate(fkind, &cp1->variant.float_value,
                &result_con->variant.float_value, &err, &dep);
      if (err || dep) {
        success = FALSE;
      }  /* if */
    }  /* if */
  }  /* if */
  return success;
}  /* fold_copysign_if_possible */

#endif /* C99_IL_EXTENSIONS_SUPPORTED */

static a_boolean is_empty_string_literal(a_constant_ptr  cp)
/*
Return TRUE if the given constant is the empty string literal ("") or the
address thereof.
*/
{
  a_boolean  result;

  if (cp->kind == (a_constant_repr_kind)ck_address &&
      cp->variant.address.kind == (an_address_base_kind)abk_constant &&
      cp->variant.address.offset == 0) {
    cp = cp->variant.address.variant.constant;
  }  /* if */
  if (cp->kind == (a_constant_repr_kind)ck_string &&
      cp->variant.string.length == 1 &&
      cp->variant.string.value[0] == '\0') {
    result = TRUE;
  } else {
    result = FALSE;
  }  /* if */
  return result;
}  /* is_empty_string_literal */

#endif /* TARG_HAS_IEEE_FLOATING_POINT */
#if C99_IL_EXTENSIONS_SUPPORTED

static a_boolean fold_pow_if_possible(a_constant_ptr  base,
                                      a_constant_ptr  exp,
                                      a_constant_ptr  result,
                                      a_type_ptr      result_type)
/*
base and exp are floating-point constants.  If exp represents a small
nonnegative integer value, store the value of base raised to the power
indicated by exp in result and return TRUE (the final result is a
floating-point value of the given type).  Otherwise, return FALSE.
*/
{
  a_boolean   folded = FALSE, err = FALSE, mode_dep;
  a_host_large_integer
              e;
  an_internal_float_value
              b, acc, v;
  a_type_ptr  exp_type = skip_typerefs(exp->type),
              base_type = skip_typerefs(base->type);
  
  check_assertion(base->kind == (a_constant_repr_kind)ck_float &&
                  is_real_floating_type(base_type) &&
                  exp->kind == (a_constant_repr_kind)ck_float &&
                  is_real_floating_type(exp_type) &&
                  is_real_floating_type(result_type));
  /* First check whether exp represents a small integer. */
  fp_to_host_large_integer(exp_type->variant.float_kind,
                           &exp->variant.float_value, &e, &err, &mode_dep);
  if (!err && e >= 0 && e < 256) {
    /* Check whether exp represents an integer by converting e back to a
       floating-point value a testing if it remains unmodified. */
    fp_host_large_integer_to_float(exp_type->variant.float_kind, e, &v, &err);
    if (fp_compare(exp_type->variant.float_kind,
                   &exp->variant.float_value, &v, &err) == 0 && !err) {
      folded = TRUE;
    }  /* if */
  }  /* if */
  if (folded) {
    /* The actual computation will be done with "long double" precision. */
    fp_change_kind(&base->variant.float_value, base_type->variant.float_kind,
                   &b, (a_float_kind)fk_long_double,
                   &err, &mode_dep);
    if (err) folded = FALSE;
  }  /* if */
  if (folded) {
    fp_host_large_integer_to_float((a_float_kind)fk_long_double,
                                   (a_host_large_integer)1, &acc, &err);
    check_assertion(!err);
    /* The initial value b0 of b is the "base" value.  It is subsequently
       squared to compute powers b1 = b0^2, b2 = b1^2 = b0^4, etc.  Every
       time bit n is set in e (n = 0 for the least significant bit), an
       accumulator (starting with value 1) is multiplied by bn =  b0^(2^n). */
    while (e != 0) {
      if (e & 1) {
        /* Update the accumulator. */
        fp_multiply((a_float_kind)fk_long_double,
                    &b, &acc, &acc, &err, &mode_dep);
        if (err) {
          folded = FALSE;
          break;
        }  /* if */
      }  /* if */
      e = e/2;
      if (e != 0) {
        /* Compute the next value of b: b <- b*b. */
        fp_multiply((a_float_kind)fk_long_double, &b, &b, &b, &err, &mode_dep);
        if (err) {
          folded = FALSE;
          break;
        }  /* if */
      }  /* if */
    }  /* while */
    if (folded) {
      /* Store the final result with the required precision. */
      clear_constant(result, (a_constant_repr_kind)ck_float);
      result->type = result_type;
      fp_change_kind(&acc, (a_float_kind)fk_long_double,
                     &result->variant.float_value,
                     skip_typerefs(result_type)->variant.float_kind,
                     &err, &mode_dep);
      folded = !err;
    }  /* if */
  }  /* if */
  return folded;
}  /* fold_pow_if_possible */

#endif /* C99_IL_EXTENSIONS_SUPPORTED */

static a_boolean fold_lock_free_query_if_possible(
                                         a_builtin_function_kind  bfk,
                                         an_expr_node_ptr         size_arg,
                                         an_expr_node_ptr         ptr_arg,
                                         a_constant_ptr           result,
                                         a_type_ptr               result_type)
/*
bfk is either bfk_atomic_always_lock_free, bfk_atomic_is_lock_free, or
bfk__c11_atomic_is_lock_free.  If possible, fold a call to the corresponding
built-in functions with the given arguments (ptr_arg is NULL if bfk is
bfk__c11_atomic_is_lock_free), and return the result in *result.  If
successful, return TRUE.
*/
{
  a_boolean  folded = FALSE, err;

  check_assertion(size_arg != NULL);
  if (is_constant_node(size_arg) && node_constant_is(size_arg, ck_integer)) {
    /* These queries can only be folded if the first argument is a
       constant. */
    a_constant_ptr         size_con = node_constant(size_arg);
    a_host_large_unsigned  size;
    a_boolean              size_8_foldable = FALSE;
    if (bfk == (a_builtin_function_kind)bfk_atomic_always_lock_free ||
        (ptr_arg != NULL && is_constant_node(ptr_arg))) {
      /* For __atomic_is_lock_free, GCC appears to always fold the cases where
         the size argument is 1, 2, or 4.  The case where it is 8, is only
         folded if ptr_arg represents an address constant expression or a
         null pointer constant. */
      size_8_foldable = TRUE;
    }  /* if */
    size = unsigned_value_of_integer_constant(size_con, &err);
    if (size == 1 || size == 2 || size == 4 ||
        (size == 8 && size_8_foldable)) {
      set_unsigned_integer_constant(result, (a_host_large_unsigned)1,
                                    result_type->variant.integer.int_kind);
      folded = TRUE;
    } else if (bfk == (a_builtin_function_kind)bfk_atomic_always_lock_free) {
      set_unsigned_integer_constant(result, (a_host_large_unsigned)0,
                                    result_type->variant.integer.int_kind);
      folded = TRUE;
    }  /* if */
  }  /* if */
  return folded;
}  /* fold_lock_free_query_if_possible */


static a_boolean is_dependent_list_of_constant_nodes(an_expr_node_ptr  list)
/*
Return TRUE if every node in the given list is a constant, and at least one is
either a ck_template_param constant or has a template-dependent type.
*/
{
  a_boolean        is_constant = TRUE, is_dependent = FALSE;
  an_expr_node_ptr node;

  for (node = list; node != NULL; node = node->next) {
    an_expr_node_ptr expr = skip_parens(node);
    if (!is_constant_node(expr)) {
      is_constant = FALSE;
      break;
    } else if (expr_is_instantiation_dependent(expr)) {
      is_dependent = TRUE;
    }  /* if */
  }  /* for */
  return is_constant && is_dependent;
}  /* is_dependent_list_of_constant_nodes */


a_boolean fold_gnu_builtin_function_call_if_possible(
                                                  a_routine_ptr    rp,
                                                  an_expr_node_ptr args,
                                                  an_expr_node_ptr call_expr,
                                                  a_constant       *result_con,
                                                  an_error_code    *err_code)
/*
If "rp" is a GNU builtin function, see whether a call of
"rp" with the argument list "args" can be folded to a constant.
If so, set *result_con to the constant result and return TRUE.
Otherwise, return FALSE.  If there's an error, set *err_code to the
error code and return FALSE.  Otherwise, *err_code is set to
ec_no_error.  call_expr is the original call node, which may be
used to create a ck_template_param result for a dependent case.
Note that in some cases, the builtin function is never folded;
the folding mechanism is used as a way to validate argument values.
*/
{
  a_boolean      folded = FALSE;
  a_constant_ptr result = local_constant();

  *err_code = ec_no_error;
#if GNU_EXTENSIONS_ALLOWED
  if (rp->implicit_alias) {
    /* A call to a user-defined routine that is implicitly assumed equivalent
       to a built-in function (recorded in
       rp->gnu_extra_info->aliased_routine). */
    rp = gnu_routine_supp(rp)->aliased_routine;
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (is_gnu_builtin_function(rp)) {
    a_type_ptr       result_type = f_skip_typerefs(return_type_of(rp->type));
    an_expr_node_ptr args2 = NULL;
    if (args != NULL) {
      args2 = args->next;
      args = skip_parens(args);
    }  /* if */
    switch (rp->variant.builtin_function_kind) {
      case bfk_classify_type:
        /* Pseudo-calls to this function should have been scanned and
           folded in scan_gnu_builtin_pseudo_call. */
        unexpected_condition();
        break;
      case bfk_constant_p:
        /* Usually folded in scan_gnu_builtin_pseudo_call, but for
           folding of constexpr calls we can get here. */
        check_assertion(is_integral_type(result_type) && args != NULL);
        set_integer_constant(result,
                             (a_host_large_integer)is_constant_node(args),
                             result_type->variant.integer.int_kind);
        folded = TRUE;
        break;
      case bfk_huge_valf:
      case bfk_huge_val:
      case bfk_huge_vall:
        /* A "huge" floating-point value.  (I.e., positive Infinity if
           that is available, or the largest possible value of the
           associated floating-point type.) */
        if (args == NULL && is_floating_type(result_type)) {
          clear_constant(result, (a_constant_repr_kind)ck_float);
          result->type = result_type;
          folded = make_huge_fp_val(&result->variant.float_value,
                                    result_type->variant.float_kind);
        }  /* if */
        break;
#if TARG_HAS_IEEE_FLOATING_POINT
      case bfk_nansf:
      case bfk_nans:
      case bfk_nansl:
      case bfk_nanf:
      case bfk_nan:
      case bfk_nanl:
        /* A signaling or non-signaling (i.e., "quiet") Not-a-Number value. */
        { a_constant_ptr scon;
          a_boolean      err = FALSE;
          unsigned long  mantissa = 0;
          if (args != NULL && args2 == NULL &&
              expr_is_pointer_to_string_literal(args, &scon) &&
              is_floating_type(result_type)) {
            a_builtin_function_kind kind = rp->variant.builtin_function_kind;
            a_boolean               signaling = FALSE;
            if (kind == (a_builtin_function_kind)bfk_nansf ||
                kind == (a_builtin_function_kind)bfk_nans ||
                kind == (a_builtin_function_kind)bfk_nansl) {
              signaling = TRUE;
            }  /* if */
            clear_constant(result, (a_constant_repr_kind)ck_float);
            result->type = result_type;
            if (!is_empty_string_literal(scon)) {
              /* The string specifies the bits that should be used in the
                 mantissa portion of the NaN. */
              mantissa = strtoul_interface(scon->variant.string.value, &err);
            }  /* if */
            if (!err) {
              folded = make_fp_nan(&result->variant.float_value,
                                   result_type->variant.float_kind,
                                   signaling, (an_fp_value_part)mantissa);
            }  /* if */
          }  /* if */
        }
        break;
      case bfk_inff:
      case bfk_inf:
      case bfk_infl:
        /* A positive infinity value. */
        if (args == NULL && is_floating_type(result_type)) {
          clear_constant(result, (a_constant_repr_kind)ck_float);
          result->type = result_type;
          folded = make_fp_infinity(&result->variant.float_value,
                                    result_type->variant.float_kind);
        }  /* if */
        break;
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
      case bfk_ffs:
      case bfk_ffsl:
      case bfk_clz:
      case bfk_clzl:
      case bfk_ctz:
      case bfk_ctzl:
      case bfk_popcount:
      case bfk_popcountl:
      case bfk_parity:
      case bfk_parityl:
#if LONG_LONG_ALLOWED
      case bfk_ffsll:
      case bfk_clzll:
      case bfk_ctzll:
      case bfk_popcountll:
      case bfk_parityll:
#endif /* LONG_LONG_ALLOWED */
        /* Bit counting functions. */
        if (args != NULL && args2 == NULL) {
          folded = fold_bit_count_operation_if_possible(rp, args, result);
        }  /* if */
        break;
#if C99_IL_EXTENSIONS_SUPPORTED
#if TARG_HAS_IEEE_FLOATING_POINT
      case bfk_isnan:
      case bfk_isnanf:
      case bfk_isnanl:
      case bfk_isinf:
      case bfk_isinff:
      case bfk_isinfl:
      case bfk_isfinite:
      case bfk_isnormal:
      case bfk_signbit:
      case bfk_signbitf:
      case bfk_signbitl:
        /* Unlike some other functions handled here, __builtin_isnan and
           __builtin_isinf are ellipsis functions, and hence ordinary call
           processing will not diagnose invalid arguments.  GCC, however,
           does check that there is exactly one argument of a real floating-
           point type. */
        if (args == NULL || args2 != NULL) {
          *err_code = ec_call_requires_one_argument;
        } else if (!is_real_floating_type(args->type) &&
                   !is_template_param_type(args->type)) {
          *err_code = ec_call_requires_floating_point_argument;
        } else {
          folded = fold_fptest_if_possible(rp, args, result);
        }  /* if */
        break;
    case bfk_copysign:
    case bfk_copysignf:
    case bfk_copysignl:
        if (args == NULL || args2 == NULL || args2->next != NULL) {
          *err_code = ec_wrong_number_of_arguments;
        } else if (!is_real_floating_type(args->type) &&
                   !is_template_param_type(args->type)) {
          *err_code = ec_call_requires_floating_point_argument;
        } else if (!is_real_floating_type(args2->type) &&
                   !is_template_param_type(args2->type)) {
          *err_code = ec_call_requires_floating_point_argument;
        } else {
          folded = fold_copysign_if_possible(rp, args, args2, result);
        }  /* if */
        break;
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
      case bfk_fpclassify:
        /* We currently never fold calls to __builtin_fpclassify, but we do
           check that the last argument has a floating-point type. */
        { an_expr_node_ptr   arg = args;
          unsigned long      n = 0;
          for (; arg != NULL; n += 1, arg = arg->next) {
            if (is_error_type(arg->type)) break;
          }  /* for */
          if (arg != NULL || n < 5) {
            /* The routine type of __builtin_fpclassify is
                 int (int, int, int, int, int, ...);
               So if we have less than five arguments, an error should be
               issued elsewhere.  If we ran into an error type, no other error
               should be emitted either. */
          } else if (n == 5 || n > 6) {
            /* Five arguments or more than six: Issue an error. */
            *err_code = ec_invalid_builtin_fpclassify_args;
          } else {
            /* Check that the last (sixth) argument has floating-point type. */
            an_expr_node_ptr  fparg = args2->next->next->next->next;
            if (!is_real_floating_type(fparg->type) &&
                !is_template_param_type(fparg->type)) {
              if (!is_error_type(fparg->type)) {
                *err_code = ec_bad_final_builtin_fpclassify_arg;
              }  /* if */
            }  /* if */
          }  /* if */
        }
        break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      case bfk_strlen:
        /* strlen of a constant string can be folded in C mode. */
        { a_constant_ptr scon;
          if (C_mode() && args != NULL && args2 == NULL &&
              expr_is_pointer_to_string_literal(args, &scon) &&
              is_normal_character_kind(scon->character_kind) &&
              is_integral_type(result_type)) {
            a_targ_size_t len;
            check_assertion(scon->kind == (a_constant_repr_kind)ck_string);
            /* Watch out for strings that don't have a terminating null. */
            for (len = 0; len < scon->variant.string.length; len++) {
              if (scon->variant.string.value[len] == '\0') {
                /* Found first null character, so we know the length. */
                folded = TRUE;
                set_integer_constant(result,
                                     (a_host_large_integer)len,
                                     skip_typerefs(result_type)->
                                                     variant.integer.int_kind);
                break;
              }  /* if */
            }  /* for */
          }  /* if */
        }
        break;
      case bfk_abs:
        /* abs of a constant integer can be folded. */
        { a_constant_ptr con;
          if (args != NULL && args2 == NULL &&
              is_constant_node(args) &&
              constant_is((con = node_constant(args)), ck_integer) &&
              is_integral_type(con->type) &&
              is_integral_type(result_type)) {
            a_boolean err = FALSE;
            copy_constant(con, result);
            /* Probably no type difference between the parameter type and
               the result type, but change it just in case. */
            result->type = result_type;
            if (sign_of_integer_constant(con) < 0) {
              /* Negate a negative value. */
              negate_integer_value(&result->variant.integer_value, &err);
              if (!err &&
                  !integer_constant_in_range_for_type(result, result,
                                                     result->type)) {
                err = TRUE;
              }  /* if */
            }  /* if */
            if (!err) folded = TRUE;
          }  /* if */
        }
        break;
#if C99_IL_EXTENSIONS_SUPPORTED
      case bfk_fabs:
      case bfk_fabsf:
      case bfk_fabsl:
        /* abs of a constant float can be folded. */
        { a_constant_ptr con;
          if (args != NULL && args2 == NULL &&
              is_constant_node(args) &&
              constant_is((con = node_constant(args)), ck_float) &&
              is_real_floating_type(con->type) &&
              is_real_floating_type(result_type)) {
            a_boolean    err = FALSE, depends_on_fp_mode = FALSE;
            a_type_ptr   float_tp = skip_typerefs(result_type);
            a_float_kind float_kind = float_tp->variant.float_kind;
            copy_constant(con, result);
            /* Probably no type difference between the parameter type and
               the result type, but change it just in case. */
            result->type = float_tp;
            if (fp_signbit(float_kind, &result->variant.float_value)) {
              /* Negate a negative value. */
              a_constant_ptr fp_con = local_constant();
              copy_constant(con, fp_con);
              fp_negate(float_kind,
                        &fp_con->variant.float_value,
                        &result->variant.float_value,
                        &err,
                        &depends_on_fp_mode);
              if (depends_on_fp_mode) err = TRUE;
              release_local_constant(&fp_con);
            }  /* if */
            if (!err) folded = TRUE;
          }  /* if */
        }
        break;
      case bfk_pow:
      case bfk_powf:
      case bfk_powl:
        /* pow(x, y) can sometimes be folded in gcc mode. */
        { check_assertion(is_real_floating_type(result_type));
          if (gcc_mode && gnu_version >= 30400 &&
              args != NULL && args2 != NULL && args2->next == NULL) {
            args2 = skip_parens(args2);
            if (is_constant_node(args) && is_constant_node(args2)) {
              /* GCC folds only certain combinations of values.  E.g., if the
                 base is not an integer, it would appear that only raising to
                 the power of -1, 0, 1, 2, and 3 is folded.  If the base is a
                 power of 2, many more powers are folded.  The call to
                 fold_pow_if_possible folds a different set of combinations,
                 but the cases somewhat likely to show up in real code should
                 be covered. */
              folded = fold_pow_if_possible(node_constant(args),
                                            node_constant(args2),
                                            result, result_type);
            }  /* if */
          }  /* if */
        }
        break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      case bfk_ceil:
      case bfk_ceilf:
      case bfk_ceill:
        /* Compute the ceiling of the given argument. */
        { a_constant_ptr con;
          if (args != NULL && args2 == NULL &&
              is_constant_node(args) &&
              constant_is((con = node_constant(args)), ck_float) &&
              is_real_floating_type(con->type) &&
              is_real_floating_type(result_type)) {
            a_boolean    err = FALSE;
            a_type_ptr   float_tp = skip_typerefs(result_type);
            copy_constant(con, result);
            /* Probably no type difference between the parameter type and
               the result type, but change it just in case. */
            result->type = float_tp;
            fp_ceil(float_tp->variant.float_kind,
                    &con->variant.float_value,
                    &result->variant.float_value,
                    &err);
            if (!err) folded = TRUE;
          }  /* if */
        }
        break;
      case bfk_atomic_always_lock_free:
        if (args == NULL || !is_constant_node(args)) {
          /* __atomic_always_lock_free's first argument must be a
             constant. */
          *err_code = ec_first_arg_must_be_integer_constant;
          break;
        }  /* if */
        FALLTHROUGH
      case bfk_atomic_is_lock_free:
      case bfk___c11_atomic_is_lock_free:
        folded = fold_lock_free_query_if_possible(
                                           rp->variant.builtin_function_kind,
                                           args, args2, result, result_type);
        break;
      case bfk_assume_aligned:
        /* Calls to __builtin_assume_aligned are never actually folded, but
           we treat it as "potentially folded" to simplify checking for
           extraneous call arguments. */
        if (args2 != NULL && args2->next != NULL) {
          /* A optional third argument is permitted but must be of integer
             type.  A fourth argument is not permitted. */
          if (args2->next->next != NULL) {
            *err_code = ec_too_many_arguments;
          } else if (!(is_integral_type(args2->next->type) ||
                       is_template_dependent_type(args2->next->type))) {
            *err_code = ec_3rd_arg_of_assume_aligned_must_be_integral;
          }  /* if */
        }  /* if */
        folded = FALSE;
        break;
      case bfk_cpu_supports:
      case bfk_cpu_is:
        /* These aren't actually folded, rather the "folding" mechanism is used
           to perform a check that the argument is a string literal.  Note
           that the value of the string literal is not checked here (that is
           left to the back end). */
        if (args == NULL ||
            !expr_is_pointer_to_string_literal(args, (a_constant **)NULL)) {
          *err_code = ec_call_requires_string_literal;
        }  /* if */
        folded = FALSE;
        break;
      case bfk_bswap16:
      case bfk_bswap32:
      case bfk_bswap64:
        /* Byte swap functions. */
        if (args != NULL && args2 == NULL &&
            targ_char_bit == 8 &&
            is_constant_node(args) &&
            args->variant.constant.ptr->kind ==
                                            (a_constant_repr_kind)ck_integer) {
          unsigned int bytes;
          check_assertion(result_type->kind == (a_type_kind)tk_integer);
          switch (rp->variant.builtin_function_kind) {
            case bfk_bswap16:
              bytes = 2;
              break;
            case bfk_bswap32:
              bytes = 4;
              break;
            case bfk_bswap64:
              bytes = 8;
              break;
            default:
              unexpected_condition();
          }  /* switch */
          folded = swap_bytes_in_unsigned_integer(bytes,
                            &args->variant.constant.ptr->variant.integer_value,
                            &result->variant.integer_value);
        }  /* if */
        break;
      default:
        /* Nothing to be done. */
        { a_diag_list  diag_list;
          clear_diag_list(&diag_list);
          folded = interpret_constexpr_call(call_expr, /*is_consteval=*/FALSE,
                                            result, &diag_list);
        }
        break;
    }  /* switch */
  }  /* if */
  if (!folded && is_dependent_list_of_constant_nodes(args)) {
    /* A call on a list of dependent constants can be folded to a
       ck_template_param constant. */
    make_template_param_expr_constant(call_expr, result);
    folded = TRUE;
  }  /* if */
  /* If folding was successful, store the result in *result_con.  If it wasn't
     successful, but the call does not seem malformed, try evaluating it with
     the interpreter (which can handle additional cases). */
  if (folded) {
    copy_constant(result, result_con);
  } else if (*err_code == ec_no_error && fold_expr(call_expr, result_con)) {
    folded = TRUE;
  }  /* if */
  release_local_constant(&result);
#if DEBUG
  if (folded && db_flag_is_set("folded_builtin")) {
    fprintf(f_debug, "folded builtin: ");
    db_constant(result_con);
#if C99_IL_EXTENSIONS_SUPPORTED
    if (is_real_floating_type(result_con->type)) {
      /* In addition to the representation printed above, also print out a
         hexadecimal representation (for NaNs, infinities, etc.). */
      fprintf(f_debug, " ");
      db_internal_float_value(&result_con->variant.float_value);
    } else
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    /* Do not insert code here. */
    {
      fprintf(f_debug, "\n");
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  return folded;
}  /* fold_gnu_builtin_function_call_if_possible */

#endif /* BUILTIN_FUNCTIONS_ENABLED */

static void copy_constant_for_constexpr_evaluation(a_constant *con,
                                                   a_constant *result_con)
/*
Do a deep copy of a constant entry for the purposes of constexpr
evaluation.  The result_con is not allocated in the IL (it's probably
in the stack).
*/
{
  (void)copy_constant_full(con, result_con,
                           (CE_DEST_CONSTANT_IS_NOT_ALLOC_IN_IL |
                            CE_COPYING_FOR_CONSTEXPR_FOLDING));
}  /* copy_constant_for_constexpr_evaluation */


/*lint -ecall(523,*folding_fails)*/
static void folding_fails(void)
/*
Exists as a useful place to set a breakpoint to catch the first point
where folding fails.
*/
{
}  /* folding_fails */


static a_boolean folding_result(a_boolean folded)
/*
Pass-through routine used to return the result from a folding routine.
Used to call folding_fails, a useful place to set a breakpoint to
catch the first point where folding fails.
*/
{
  if (!folded) {
    folding_fails();
  }  /* if */
  return folded;
}  /* folding_result */


a_constant_ptr constant_value_at_address(a_constant_ptr  addr_con,
                                         a_constant_ptr  target_con)
/*
If addr_con is a ck_address constant designating a constant, a variable
with a constant value, or a subobject of one of those, return the value of
that constant, variable, or subobject; otherwise, return NULL.  If
target_con is non-NULL, the value is copied into the designated constant
and target_con is returned; otherwise, a new unshared constant will be
allocated and returned.
*/
{
  a_constant_ptr result_con = NULL;
  a_constant_ptr char_con = local_constant();
  a_boolean      type_mismatch = FALSE;
  a_boolean      err = FALSE;

  if (is_error_constant(addr_con)) {
    /* There was an error upstream.  Return an error constant. */
    if (target_con != NULL) {
      set_error_constant(target_con);
      result_con = target_con;
    } else {
      result_con = alloc_error_constant();
    }  /* if */
  } else if (constant_is(addr_con, ck_address) &&
             (address_base_is(addr_con, abk_variable) ||
              address_base_is(addr_con, abk_constant) ||
              address_base_is(addr_con, abk_temporary))) {
    an_address_base_kind abkind = addr_con->variant.address.kind;
    a_type_ptr           target_type = type_pointed_to(addr_con->type);
    a_type_ptr           val_type;
    target_type = skip_typerefs(target_type);
    if (abkind == abk_variable) {
      val_type = addr_con->variant.address.variant.variable->type;
    } else {
      check_assertion(abkind == abk_constant || abkind == abk_temporary);
      val_type = addr_con->variant.address.variant.constant->type;
    }  /* if */
    if (addr_con->variant.address.offset < 0 ||
        (a_targ_size_t)addr_con->variant.address.offset >=
                                              skip_typerefs(val_type)->size) {
      /* The address is outside the bounds of the object, so this is not a
         constant expression.  (A warning will have been issued earlier, so
         no diagnostic is needed here.) */
    } else if (abkind == abk_variable) {
      /* The constant is the address of a variable, possibly with an offset
         designating a subobject.  See if it has a constant value and, if
         so, use it. */
      a_variable  *vp = addr_con->variant.address.variant.variable;
      a_boolean   allow_C_mode_const_var = gcc_mode;
      result_con = var_constant_value_full(vp, /*copy_for_reuse=*/FALSE,
                                           /*clear_backing_expr=*/FALSE,
                                           allow_C_mode_const_var);
    } else if (abkind == abk_temporary && !is_const_qualified_type(val_type)) {
      /* The temporary is mutable.  The constant is only its initial value. */
    } else {
      /* The constant is the address of a constant, possibly with an offset
         designating a subobject.  Use it. */
      result_con = addr_con->variant.address.variant.constant;
    }  /* if */
    if (result_con == NULL) {
      /* The address constant does not designate a constant value -- just
         return NULL. */
    } else if (is_error_constant(result_con)) {
      /* There was an error upstream.  Return an error constant. */
      if (target_con != NULL) {
        set_error_constant(target_con);
        result_con = target_con;
      } else {
        result_con = alloc_error_constant();
      }  /* if */
    } else {
      /* Either the value is result_con or some subobject thereof or it's a
         zero value resulting from an aggregate initializer with fewer
         elements than the object being initialized.  Scan through the type
         of the constant and its value in parallel to match the initial
         value with the specified offset. */
      a_type_ptr       curr_type = skip_typerefs(result_con->type);
      a_targ_ptrdiff_t offset = addr_con->variant.address.offset;
      a_boolean        found_value = FALSE;
      a_targ_ptrdiff_t cum_offset = 0;

      /* Define a macro for accessing the current type's size as a signed
         integer.  This macro is undefined at the end of this scope. */
#define curr_type_size (a_targ_ptrdiff_t)(curr_type->size)

      while (!err && !found_value && result_con != NULL) {
        if (cum_offset == offset &&
            identical_types(target_type, curr_type)) {
          /* result_con is the value we're looking for. */
          found_value = TRUE;
        } else if (cum_offset == offset &&
                   result_con->kind != (a_constant_repr_kind)ck_aggregate &&
                   result_con->kind != (a_constant_repr_kind)ck_string) {
          /* We've found the desired offset, but there's a type mismatch,
             possibly because of selecting the non-active member of a
             union. */
          found_value = TRUE;
          type_mismatch = TRUE;
        } else if (result_con->kind == (a_constant_repr_kind)ck_string) {
          /* result_con is a string; offset designates either a character
             within the constant or a character beyond the length of the
             initializer that was implicitly value-initialized. */
          found_value = TRUE;
          if (offset >= (cum_offset + curr_type_size)) {
            /* Return a value-initialized constant of the element type. */
            result_con = NULL;
          }  /* if */
        } else if (is_error_type(curr_type) || is_error_constant(result_con) ||
                   result_con->kind != (a_constant_repr_kind)ck_aggregate) {
          /* There was an error upstream. */
          err = TRUE;
        } else {
          /* curr_type is either an array or a class type, and offset
             represents one of its subobjects.  Step into curr_type and
             continue scanning for the matching offset. */
          a_boolean      may_have_designator =
                                      result_con->uses_designated_initializers;
          a_constant_ptr possible_result_con = NULL;
          check_assertion(cum_offset + curr_type_size > offset);
          result_con = result_con->variant.aggregate.first_constant;
          if (is_array_type(curr_type)) {
            /* Find the element of the array that is at or contains the
               specified offset.  We'll then go back through the main loop
               again looking at that element. */
            a_boolean        found_element = FALSE;
            a_targ_ptrdiff_t array_offset = cum_offset;
            a_targ_ptrdiff_t possible_result_offset = 0;

            curr_type = skip_typerefs(curr_type->variant.array.element_type);
            while (result_con != NULL && !found_element) {
              if (result_con->kind == (a_constant_repr_kind)ck_designator) {
                /* Compute the offset of the designated element and advance
                   result_con to point to the associated value. */
                check_assertion(!result_con->
                                       variant.designator.is_field_designator);
                cum_offset = (array_offset +
                         ((a_targ_ptrdiff_t)result_con->variant.designator.
                                                        variant.array_element *
                                                              curr_type_size));
                result_con = result_con->next;
              }  /* if */
              if (result_con->kind == (a_constant_repr_kind)ck_init_repeat) {
                /* This constant represents some number of elements of the
                   array.  Get the cumulative size of those elements and
                   check if the offset designates one of them.  Because the
                   init-repeat may represent the elements of a subarray
                   rather than the elements of the array at this level, set
                   curr_type to the type of the repeated elements instead
                   of the elements at this level. */
                a_targ_ptrdiff_t this_initializer_size;
                curr_type = skip_typerefs(
                               result_con->variant.init_repeat.constant->type);
                this_initializer_size =
                    ((a_targ_ptrdiff_t)result_con->variant.init_repeat.count *
                     curr_type_size);
                if (cum_offset <= offset &&
                    (cum_offset + this_initializer_size) > offset) {
                  /* The offset lies within this repeated group.  Set
                     result_con to that repeated constant and adjust
                     cum_offset to reflect its position in the array. */
                  if (may_have_designator) {
                    /* Record the repeated constant as a possible result
                       and continue to loop in case of a later
                       designator. */
                    possible_result_con =
                                      result_con->variant.init_repeat.constant;
                    possible_result_offset = (cum_offset +
                                              (((offset - cum_offset) /
                                               curr_type_size) *
                                               curr_type_size));
                    result_con = result_con->variant.init_repeat.constant;
                    cum_offset += (((offset - cum_offset) / curr_type_size) *
                                   curr_type_size);
                  } else {
                    /* This is the result. */
                    found_element = TRUE;
                    result_con = result_con->variant.init_repeat.constant;
                    cum_offset += (((offset - cum_offset) / curr_type_size) *
                                   curr_type_size);
                  }  /* if */
                } else {
                  /* Skip over this group and continue with the next array
                     element. */
                  result_con = result_con->next;
                  cum_offset += this_initializer_size;
                }  /* if */
              } else if (cum_offset <= offset &&
                         (cum_offset + curr_type_size) > offset) {
                /* The offset designates this array element or a subobject
                   therein. */
                if (may_have_designator) {
                  /* Record this constant as a possible result and continue
                     to loop in case of a later designator. */
                  possible_result_con = result_con;
                  possible_result_offset = cum_offset;
                  result_con = result_con->next;
                  cum_offset += curr_type_size;
                } else {
                  found_element = TRUE;
                }  /* if */
              } else {
                /* Step to the next element. */
                result_con = result_con->next;
                cum_offset += curr_type_size;
              }  /* if */
            }  /* while */
            if (!found_element && possible_result_con != NULL) {
              /* Take the last matching constant that was found as the
                 result. */
              result_con = possible_result_con;
              cum_offset = possible_result_offset;
            }  /* if */
          } else {
            /* A class type.  Scan through its subobjects (base classes and
               members) to find which is at or contains the specified
               offset. */
            a_base_class_ptr bp;
            check_assertion(is_immediate_class_type(curr_type));
            /* First examine the base class subobjects, if any. */
            for (bp = base_classes_of(curr_type);
                 bp != NULL && result_con != NULL;
                 bp = bp->next) {
              /* Virtual bases cannot appear in literal types. */
              check_assertion(!bp->is_virtual);
              if (bp->direct) {
                /* Only direct base classes are represented at this level
                   in the constant; indirect base classes are in nested
                   elements of the aggregate. */
                a_type_ptr    base_class = bp->type;
                a_type_ptr    base_class_for_size = base_class;
                a_targ_size_t base_class_size;
#if DO_IL_LOWERING
                if (class_has_been_prelowered(base_class)) {
                  /* The size of a base class subobject can be different
                     from that of a standalone object with that type, so
                     use the subobject type for size calculations. */
                  base_class_for_size = subobject_for_class(base_class);
                }  /* if */
#endif /* DO_IL_LOWERING */
                if (bp->is_optimized_empty_base) {
                  base_class_size = 0;
                } else {
                  base_class_size = class_type_supp(base_class_for_size)->
                                             size_without_virtual_base_classes;
                }  /* if */
                /* The order in which base class subobjects appear in the
                   derived class object can be different from the order in
                   which they appear in the base class list.  The
                   subobjects in the ck_aggregate follow the ordering of
                   the base class list, but we need to check both the
                   starting and ending offsets of the base class object to
                   determine whether the address lies within it or not. */
                if (offset == (cum_offset + (a_targ_ptrdiff_t)bp->offset) ||
                    (offset > (cum_offset + (a_targ_ptrdiff_t)bp->offset) &&
                     offset < (cum_offset + (a_targ_ptrdiff_t)bp->offset +
                               (a_targ_ptrdiff_t)base_class_size))) {
                  if (base_class_size != 0 ||
                      identical_types(base_class, target_type)) {
                    /* The address designates or lies within this base
                       class subobject. */
                    break;
                  }  /* if */
                }  /* if */
                /* The address is not in this base class subobject; step to
                   the next base class and subobject in the value. */
                result_con = result_con->next;
              }  /* if */
            }  /* for */
            if (bp != NULL) {
              /* The offset designates or lies within a base class
                 subobject.  Go back through the main loop to examine that
                 class. */
              cum_offset += (a_targ_ptrdiff_t)bp->offset;
              curr_type = bp->type;
            } else if (result_con != NULL &&
                       result_con->kind ==
                                         (a_constant_repr_kind)ck_designator &&
                       curr_type->kind == (a_type_kind)tk_union) {
              /* The value is that of a specified union member. */
              check_assertion(result_con->
                                       variant.designator.is_field_designator);
              curr_type =
                    skip_typerefs(result_con->variant.designator.variant.field
                                            ->type);
              result_con = result_con->next;
            } else {
              /* The offset is in a member subobject.  Scan for it and then
                 go back through the main loop. */
              a_field_ptr curr_field;
              a_field_ptr possible_field = NULL;
              a_boolean   found_field = FALSE;
              curr_field = next_proper_initializable_field(
                             curr_type->variant.class_struct_union.field_list);
              while (!found_field &&
                                  (result_con != NULL || curr_field != NULL)) {
                a_targ_ptrdiff_t field_offset;
                if (result_con != NULL &&
                    result_con->kind == (a_constant_repr_kind)ck_designator) {
                  /* Set curr_field to the designated field and advance
                     result_con to the associated value. */
                  check_assertion(result_con->
                                       variant.designator.is_field_designator);
                  curr_field = result_con->variant.designator.variant.field;
                  result_con = result_con->next;
                }  /* if */
                if (curr_field == NULL) {
                  /* We've run out of fields. */
                  break;
                }  /* if */
                field_offset = ((a_targ_ptrdiff_t)curr_field->offset +
                                cum_offset);
                if (curr_field->bit_size == 0 && field_offset <= offset &&
                    (field_offset +
                       (a_targ_ptrdiff_t)skip_typerefs(curr_field->type)->size)
                                                                    > offset) {
                  /* The specified offset denotes this field or a subobject
                     thereof. */
                  if (may_have_designator) {
                    /* Record this field as a possible result and continue
                       to loop in case of a later designator. */
                    possible_result_con = result_con;
                    possible_field = curr_field;
                    curr_field =
                      next_proper_initializable_field(curr_field->next);
                    if (result_con != NULL) {
                      result_con = result_con->next;
                    }  /* if */
                  } else {
                    found_field = TRUE;
                  }  /* if */
                } else {
                  /* This is not the field for the specified offset. */
                  curr_field =
                      next_proper_initializable_field(curr_field->next);
                  if (result_con != NULL) {
                    result_con = result_con->next;
                  }  /* if */
                }  /* if */
              }  /* while */
              if (!found_field && possible_field != NULL) {
                result_con = possible_result_con;
                curr_field = possible_field;
              }  /* if */
              check_assertion(curr_field != NULL);
              curr_type = skip_typerefs(curr_field->type);
              cum_offset += (a_targ_ptrdiff_t)curr_field->offset;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* while */
#undef curr_type_size
      if (!err && result_con != NULL && !type_mismatch &&
          !identical_types(target_type, curr_type)) {
        /* The requested offset designates a character within a string. */
        a_type_ptr elem_type;
        check_assertion(result_con->kind == (a_constant_repr_kind)ck_string);
        elem_type = array_element_type(result_con->type);
        if (!identical_types_ignoring_qualifiers(target_type, elem_type) &&
            !(is_plain_char_type(elem_type) &&
              is_character_type(target_type))) {
          /* The requested type is not the string element type and also not
             a signed or unsigned char type initialized from a narrow
             string literal, probably as the result of a cast to reference
             type or the like. */
          type_mismatch = TRUE;
        } else if (offset >= (cum_offset +
                        (a_targ_ptrdiff_t)result_con->variant.string.length)) {
          /* The requested character is beyond the length of the constant,
             i.e., was implicitly value-initialized.  set result_con to NULL
             so that a zero constant will be synthesized, */
          result_con = NULL;
        } else {
          /* Copy the character into char_con and use that as the result. */
          a_host_large_integer char_val;
          a_const_char         *start_of_char_within_string;
          check_assertion(target_type->kind == (a_type_kind)tk_integer);
          start_of_char_within_string =
                      result_con->variant.string.value + (offset - cum_offset);
          char_val = (a_host_large_integer)extract_character_from_string(
                                            start_of_char_within_string,
                                            (unsigned int)(target_type->size));
          set_integer_constant(char_con, char_val,
                               target_type->variant.integer.int_kind);
          if (is_plain_char_type(elem_type) && targ_has_signed_chars) {
            sign_extend_integer_value(&char_con->variant.integer_value,
                                      targ_char_bit);
          }  /* if */
          result_con = char_con;
        }  /* if */
      }  /* if */
      if (err) {
        /* There was an error of some sort and we cannot fold. */
        result_con = NULL;
      } else if (type_mismatch) {
        /* Cannot fold -- type punning is not allowed in constant
           expressions. */
        result_con = NULL;
      } else if (result_con != NULL) {
        /* result_con points to the requested value. */
        if (target_con != NULL) {
          copy_constant_for_constexpr_evaluation(result_con, target_con);
          result_con = target_con;
        } else {
          result_con = copy_constant_full(result_con, (a_constant *)NULL,
                                          CE_COPYING_FOR_CONSTEXPR_FOLDING);
        }  /* if */
      } else {
        /* We ran off the end of the aggregate initializer, so the
           subobject was implicitly value-initialized.  Make a constant of
           the requisite type and use that. */
        if (target_con != NULL) {
          if (make_value_initialized_constant(target_type, target_con)) {
            result_con = target_con;
          }  /* if */
        } else {
          a_constant_ptr zero_con = local_constant();
          if (make_value_initialized_constant(target_type, zero_con)) {
            result_con = copy_unshared_constant(zero_con);
          }  /* if */
          release_local_constant(&zero_con);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  release_local_constant(&char_con);
  return result_con;
}  /* constant_value_at_address */


a_constant_ptr constant_value_addressed_by_node(an_expr_node_ptr  expr)
/*
If expr (which must be a glvalue) is a constant address of a constant
value, return that value; otherwise, return NULL.  For example, if the
expression is something like *p, the value of p is an address constant, and
the variable to which p points has a constant value, return that value.
*/
{
  a_constant_ptr result_con = NULL;

  if (constexpr_enabled) {
    a_diag_list    diag_list;
    result_con = local_constant();
    clear_diag_list(&diag_list);
    if (interpret_expr(expr, /*is_constant_evaluated=*/FALSE,
                       /*force_prvalue=*/TRUE, result_con, &diag_list)) {
      result_con = move_local_constant_to_il(&result_con);
    } else {
      release_local_constant(&result_con);
    }  /* if */
    discard_more_info_list(&diag_list);
  }  /* if */
  return result_con;
}  /* constant_value_addressed_by_node */


static a_boolean fold_object_expr(an_expr_node_ptr             expr,
                                  a_boolean                    want_addr,
                                  a_constant                   *result_con)
/*
Attempt to fold the expression "expr", a class or array object that
might be in lvalue or rvalue form, to either a constant value for the
object (want_addr == FALSE) or a constant address for the object
(want_addr == TRUE), by substituting argument constant values for
parameters.  If a constant result is possible, place the constant
value in *result_con and return TRUE; otherwise, return FALSE.
*/
{
  a_boolean  folded = FALSE;

  check_assertion(is_class_struct_union_type(expr->type) ||
                  is_array_type(expr->type) ||
                  is_template_param_type(expr->type) ||
                  is_error_type(expr->type));
  if (!is_glvalue_node(expr)) {
    if (fold_expr(expr, result_con)) {
      /* The object is a prvalue constant (probably a ck_aggregate). */
      folded = TRUE;
      if (want_addr) {
        /* Return the address of a temporary containing that constant. */
        a_constant_ptr con = alloc_shareable_constant(result_con);
        set_temporary_address_constant(con, result_con);
      }  /* if */
    }  /* if */
  } else {
    /* Try to fold a glvalue to a constant address. */
    if (fold_glvalue_expr(expr, result_con)) {
      folded = TRUE;
      if (!want_addr) {
        a_constant_ptr pointed_to_con = local_constant();
        if (constant_value_at_address(result_con, pointed_to_con) != NULL) {
          copy_constant(pointed_to_con, result_con);
        } else {
          folded = FALSE;
        }  /* if */
        release_local_constant(&pointed_to_con);
      }  /* if */
    }  /* if */
  }  /* if */
  return folding_result(folded);
}  /* fold_object_expr */


a_boolean fold_constexpr_expr(an_expr_node_ptr  expr,
                              a_constant        *result_con,
                              a_boolean         is_constant_evaluated,
                              a_boolean         force_prvalue)
/*
Attempt to fold the expression "expr" to a constant as part of a constexpr
evaluation.  If the expression folds to a constant, place the constant in
*result_con and return TRUE; otherwise, return FALSE.  is_constant_evaluated
determines the value of a call to std::is_constant_evaluated() during this
folding.  The expression can be an lvalue, xvalue, or prvalue.  If
force_prvalue is TRUE, the value is computed as if expr were converted to a
prvalue.
*/
{
  a_boolean    folded;
  a_diag_list  diag_list;

  clear_diag_list(&diag_list);
  folded = interpret_expr(expr, is_constant_evaluated, force_prvalue,
                          result_con, &diag_list);
  if (folded && address_con_is_unknown_object(result_con)) {
    /* P2280R4 unknown-object addresses are not usable constant addresses
       for code generation. */
    folded = FALSE;
  }  /* if */
  discard_more_info_list(&diag_list);
  return folded;
}  /* fold_constexpr_expr */


void add_temp_init_backing_expression(a_constant         *con,
                                      a_dynamic_init_ptr dip)
/*
Add a backing expression to the constant "con" that points to a temp init
node pointing to the dynamic init "dip".
*/
{
  an_expr_node_ptr expr = alloc_expr_node((an_expr_node_kind)enk_temp_init);

  expr->variant.init.dynamic_init = dip;
  expr->type = con->type;
  con->expr = expr;
}  /* add_temp_init_backing_expression */


a_boolean fold_constexpr_ctor(a_dynamic_init_ptr ctor_dip,
                              a_boolean          record_backing_expr,
                              a_boolean          check_constexpr,
                              a_boolean          is_constant_evaluated,
                              a_source_position  *pos,
                              a_constant         *result_con)
/*
ctor_dip is a dik_constructor dynamic initialization.  If the constructor
invoked is declared constexpr, try to fold the construction to a constant
class object.  If that's possible, place the constant in *result_con and
return TRUE; otherwise, return FALSE.  pos gives the source position of the
initialization.  If record_backing_expr is TRUE, record a temp-init over
ctor_dip as a backing expression for the resulting constant.  If
check_constexpr is TRUE, call call_did_not_fold_to_constant if folding did
not succeed.  is_constant_evaluated determines the value produced by calls to
std::is_constant_evaluated() during this folding.
*/
{
  a_boolean    folded;
  a_diag_list  diag_list;

  check_assertion(ctor_dip != NULL &&
                  ctor_dip->kind == (a_dynamic_init_kind)dik_constructor);
  clear_diag_list(&diag_list);
  folded = interpret_constexpr_ctor(ctor_dip, is_constant_evaluated, pos,
                                    result_con, &diag_list);
  if (folded) {
    if (record_backing_expr) {
      add_temp_init_backing_expression(result_con, ctor_dip);
    }  /* if */
  } else {
    a_routine_ptr  rp = ctor_dip->variant.constructor.ptr;
    if (rp != NULL) {
      if (rp->is_consteval && consteval_failure(rp, result_con, pos,
          &diag_list)) {
        /* Proceed as if folding succeeded. */
        folded = TRUE;
      } else if (check_constexpr) {
        if (call_did_not_fold_to_constant(rp, (an_operand *)NULL,
                                          /*no_diagnostic=*/FALSE,
                                          &diag_list, pos)) {
          folded = TRUE;
          set_error_constant(result_con);
        }  /* if */
      }  /* if */
    } else {
      expect_error();
    }  /* if */
  }  /* if */
  discard_more_info_list(&diag_list);
  return folded;
}  /* fold_constexpr_ctor */


a_boolean fold_constexpr_member_selection(an_expr_node_ptr  expr,
                                          a_constant        *result_con)
/*
expr points to a field selection operation node (eok_dot_field or
eok_points_to_field).  If the object expression is a constant object of
literal type, set *result_con to the value of the field designated by
the second operand and return TRUE; otherwise, return FALSE.
Whether expr is an lvalue or not, the returned constant is the
prvalue result of the field selection.
*/
{
  a_boolean        folded = FALSE;
  a_type_ptr       obj_expr_type;
  an_expr_node_ptr obj_expr;

  check_assertion(constexpr_enabled &&
                  is_operation_node(expr) &&
                  (node_operator_is(expr, eok_dot_field) ||
                   node_operator_is(expr, eok_points_to_field)));
  /* Get the operands and the type of the object expression. */
  obj_expr = expr->variant.operation.operands;
  obj_expr_type = obj_expr->type;
  /* Watch out for dependent types. */
  if (is_template_param_or_nonreal_class_type(obj_expr_type)) {
    obj_expr_type = NULL;
  } else {
    if (node_operator_is(expr, eok_points_to_field)) {
      obj_expr_type = type_pointed_to(obj_expr->type);
      if (is_template_param_or_nonreal_class_type(obj_expr_type)) {
        obj_expr_type = NULL;
      }  /* if */
    }  /* if */
  }  /* if */
  if (obj_expr_type != NULL) {
    /* Not a dependent type. */
    obj_expr_type = skip_typerefs(obj_expr_type);
    check_assertion(is_immediate_class_type(obj_expr_type));
    if (!obj_expr_type->incomplete && is_literal_type(obj_expr_type)) {
      /* Interpret the member selection operation. */
      a_diag_list  diag_list;
      clear_diag_list(&diag_list);
      folded = interpret_expr(expr, /*is_constant_evaluated=*/FALSE,
                              /*force_prvalue=*/TRUE, result_con, &diag_list);
      discard_more_info_list(&diag_list);
    }  /* if */
  }  /* if */
  return folding_result(folded);
}  /* fold_constexpr_member_selection */


a_boolean is_static_init_constant(a_constant_ptr  con)
/*
Return TRUE if the given constant can be used for static initialization.  Most
constants fall into this category, but with constexpr support, an address
constant can refer to the address of a local variable, which cannot be used for
static initialization.
*/
{
  a_boolean  result = TRUE;

  if (con->kind == (a_constant_repr_kind)ck_address) {
    if (address_base_is(con, abk_variable) &&
        con->variant.address.variant.variable
                                      ->source_corresp.is_local_to_function) {
      result = FALSE;
    } else if (address_base_is(con, abk_temporary) &&
               !in_file_scope(con->variant.address.variant.constant)) {
      /* An abk_temporary entry for a constant allocated in function scope
         memory is equivalent to the address of a local static variable. */
      result = FALSE;
    }  /* if */
  }  /* if */
  return result;
}  /* is_static_init_constant */

#if DEBUG

unsigned long db_show_folding_fe_space_used(unsigned long grand_total)
/*
Display memory use for entities in front end memory in this file (folding.c).
*/
{
  return grand_total;
}  /* db_show_folding_fe_space_used */

#endif /* DEBUG */

void folding_one_time_init(void)
/*
Do one-time initialization of variables related to folding. (Variables
that need to be reinitialized with each new translation unit are handled
in folding_init.)
*/
{
  /* Save variables that are needed for precompiled headers */
  if (precompiled_header_processing_required) {
    STATIC_THREAD a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  /* Register variables (and arrays) that have distinct copies for distinct
     compilation units. */
  register_trans_unit_variable(curr_init_aggr_con);
}  /* folding_one_time_init */


void folding_trans_unit_init(void)
/*
Initialize static variables related to folding.  These are variables that
need initialization for every (primary and secondary) translation unit.
*/
{
  curr_init_aggr_con = NULL;
}  /* folding_trans_unit_init */


void folding_init(void)
/*
Initialize static variables related to folding.  This is done as a
subroutine (rather than relying on static initialization) so that it
can be redone to compile more than one source file in a single invocation
of the front end.
*/
{
}  /* folding_init */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

