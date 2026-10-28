/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

il_to_str.c -- Produce an external string-form representation for various
               IL entries.

*/

/* Header files common to all files. */
#include "fe_common.h"

#if BACK_END_IS_CP_GEN_BE
#include "cp_gen_be.h"
#endif /* BACK_END_IS_CP_GEN_BE */
#include "il_walk.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */


/* Macro that returns TRUE if the Microsoft form of output should
   be used for certain features. */
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
#define use_microsoft_form() (octl->gen_compilable_code ? \
                                      msvc_is_generated_code_target : \
                                      microsoft_mode)
#else /* !(BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE) */
#define use_microsoft_form() microsoft_mode
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
/*lint -esym(750,use_microsoft_form)*/


/* Macro that returns TRUE if the GNU form of output should
   be used for certain features. */
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
#define use_gnu_form() (octl->gen_compilable_code ? \
                                gcc_or_clang_is_generated_code_target : \
                                gnu_mode)
#else /* !(BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE) */
#define use_gnu_form() gnu_mode
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
/*lint -esym(750,use_gnu_form)*/


/* Macro that returns TRUE if the Sun form of output should
   be used for certain features. */
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
#define use_sun_form() (octl->gen_compilable_code ? \
                                sun_is_generated_code_target : \
                                sun_mode)
#else /* !(BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE) */
#define use_sun_form() sun_mode
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
/*lint -esym(750,use_sun_form)*/

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE


/* Forward declarations. */
static void form_qualifier(a_scope_ptr                            scope,
                           an_il_to_str_output_control_block_ptr  octl);
static void form_expression(an_expr_node_ptr                      expr,
                            an_il_to_str_output_control_block_ptr octl);
static a_boolean is_type_operator_to_be_rendered(
                                   a_type_ptr                            type,
                                   an_il_to_str_output_control_block_ptr octl);


static inline a_boolean is_for_c_gen_be(
                                    an_il_to_str_output_control_block_ptr octl)
/*
Return TRUE if the output targeted by octl is aimed at the C-generating back
end; otherwise, return FALSE.
*/
{
  return octl->gen_compilable_code && octl->c_generating_back_end;
}  /* is_for_c_gen_be */


static inline a_boolean is_for_cp_gen_be(
                                    an_il_to_str_output_control_block_ptr octl)
/*
Return TRUE if the output targeted by octl is aimed at the C++-generating back
end; otherwise, return FALSE.
*/
{
  return octl->cpp_generating_back_end;
}  /* is_for_cp_gen_be */


void clear_il_to_str_output_control_block(
                                    an_il_to_str_output_control_block_ptr octl)
/*
Clear an output control block to default values.
*/
{
  octl->output_str                = NULL;
  octl->output_partial_token_str  = NULL;
  octl->text_buffer               = NULL;
  octl->output_name               = NULL;
  octl->output_template_name      = NULL;
  octl->output_class_qualifier    = NULL;
  octl->output_enum_qualifier     = NULL;
  octl->output_temp_name          = NULL;
  octl->output_func_declarator    = NULL;
  octl->output_expression         = NULL;
  octl->output_name_reference     = NULL;
  octl->output_attributes         = NULL;
  octl->is_typedef_invisible      = NULL;
  octl->has_unprotected_gt_or_comma_operation = NULL;
  octl->expr_is_unusable          = NULL;
  octl->skip_implicit_steps       = NULL;
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
  octl->func_prototype_stack      = NULL;
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
#if BACK_END_IS_CP_GEN_BE
  octl->curr_template_arg         = NULL;
#endif /* BACK_END_IS_CP_GEN_BE */
  octl->max_template_arg_depth    = 0;
  octl->curr_template_arg_depth   = 0;
  octl->incomplete_output         = FALSE;
  octl->gen_compilable_code       = FALSE;
  octl->gen_pcc_code              = FALSE;
  octl->suppress_typedefs         = FALSE;
  octl->suppress_local_typedefs   = FALSE;
  octl->render_c99_bool           = FALSE;
  octl->c_generating_back_end     = FALSE;
  octl->cpp_generating_back_end   = FALSE;
  octl->for_diagnostics           = FALSE;
  octl->reflection_display_form   = FALSE;
#if DEBUG
  octl->debug_output              = FALSE;
#endif /* DEBUG */
  octl->force_qualified_name      = FALSE;
  octl->gen_vla_array_as_asterisk_bound_array = FALSE;
  octl->gen_raw_tab_in_literals   = FALSE;
  octl->keep_template_typedefs    = TRUE;
  octl->suppress_line_breaking    = FALSE;
  octl->suppress_cast_on_short_integral_const = FALSE;
  octl->suppress_name_in_template_cast_enum_const = FALSE;
  octl->render_auto_deduction_typerefs = FALSE;
#if GNU_VECTOR_TYPES_ALLOWED
  octl->suppress_cast_on_vector_const = FALSE;
  octl->defer_vector_attribute    = FALSE;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
  octl->suppress_template_args    = FALSE;
  octl->suppress_typedef_names    = FALSE;
  octl->suppress_ptr_to_data_member_parens = FALSE;
  octl->suppress_compiler_generated_parameters = FALSE;
  octl->processing_nontype_template_argument = FALSE;
  octl->part_of_ud_literal        = FALSE;
  octl->pending_right_paren       = FALSE;
  octl->suppress_expr_in_nontype_arg = FALSE;
  octl->name_is_dependent_conversion_type_id = FALSE;
  octl->use_microsoft_format      = FALSE;
  octl->type_context              = FALSE;
}  /* clear_il_to_str_output_control_block */


/*
Macro that is TRUE if debug output is being generated.
*/
#if DEBUG
#define generating_debug_output(octl) ((octl)->debug_output)
#else /* !DEBUG */
#define generating_debug_output(octl) (FALSE) /*lint --e(506)*/
#endif /* DEBUG */


static void output_partial_token_str(
                                    a_const_char                          *str,
                                    an_il_to_str_output_control_block_ptr octl)
/*
Output the null-terminated string str in the way indicated by octl.
The string may be only part of a token.
*/
{
  an_output_str_function_ptr rout;

  /* See if there's a special routine for partial token output.  If so, use
     it.  If not, use the output_str routine. */
  rout = octl->output_partial_token_str;
  if (rout == NULL) rout = octl->output_str;
  rout(str, octl);
}  /* output_partial_token_str */

#if BACK_END_IS_C_GEN_BE

static void output_temp_name(char                                  *entry,
                             an_il_to_str_output_control_block_ptr octl)
/*
Output a compiler-generated temporary name based on "entry".  Do this by
using a callback routine provided for this purpose.  Do the output as
indicated by octl.
*/
{
  an_output_temp_name_function_ptr rout;

  rout = octl->output_temp_name;
  check_assertion_str(rout != NULL, "output_temp_name: no routine provided");
  rout(entry);
}  /* output_temp_name */

#endif /* BACK_END_IS_C_GEN_BE */

static void form_num(a_host_large_integer                  num,
                     an_il_to_str_output_control_block_ptr octl)
/*
Output a signed number as indicated by octl.
*/
{
  char buffer[50];

  (void)signed_to_string_buf(num, buffer);
  octl->output_str(buffer, octl);
}  /* form_num */


static void form_unsigned_num(a_host_large_unsigned                 num,
                              an_il_to_str_output_control_block_ptr octl)
/*
Output an unsigned number as indicated by octl.
*/
{
  char buffer[50];

  (void)unsigned_to_string_buf(num, buffer);
  octl->output_str(buffer, octl);
}  /* form_unsigned_num */

#if DEBUG

static void form_unsigned_hex(unsigned long                         num,
                              an_il_to_str_output_control_block_ptr octl)
/*
Output an unsigned number in hexadecimal form, as indicated by octl.
*/
{
  a_number_buffer num_buff{hex_view_of(num)};

  octl->output_str(num_buff.as_temp_characters(), octl);
}  /* form_unsigned_hex */

#endif /* DEBUG */

static void form_attribute_arguments(
                                    an_attribute_ptr                      ap,
                                    an_il_to_str_output_control_block_ptr octl)
/*
Emit the attribute arguments, if any, associated with ap.
*/
{
  if (ap->arguments != NULL) {
    /* An attribute with arguments: Render them. */
    an_attribute_arg_ptr  aap = ap->arguments;
    octl->output_str("(", octl);
    for (; aap != NULL; aap = aap->next) {
      /* Emit the attribute argument. */
      switch (aap->kind) {
        case aak_empty:
          /* Nothing to emit. */
          break;
        case aak_token:
        case aak_raw_token:
          octl->output_str(aap->variant.token, octl);
          break;
        case aak_constant:
          form_constant(aap->variant.constant, /*need_parens=*/FALSE, octl);
          break;
        case aak_type:
          form_type(aap->variant.type, octl);
          break;
        default:
          unexpected_condition();
      }  /* for */
      /* Check if a separator must be issued. */
      if (aap->next != NULL) {
        if (aap->kind != (an_attribute_arg_kind)aak_raw_token) {
          octl->output_str(", ", octl);
        }  /* if */
      }  /* if */
    }  /* for */
    octl->output_str(")", octl);
  }  /* if */
}  /* form_attribute_arguments */


a_boolean form_alignas_attributes(
                      an_attribute_ptr                      ap,
                      a_boolean                             need_leading_space,
                      an_il_to_str_output_control_block_ptr octl)
/*
Output any _Alignas attributes in the list.  If need_leading_space is TRUE,
precede the first attribute with a leading space.  If an attribute is output or
if need_leading_space is TRUE, return TRUE (this allows the caller to determine
if a leading space is still needed).  Do the output in the way described by
octl.
*/
{
  for (; ap != NULL; ap = ap->next) {
    if (ap->family == af_alignas) {
      if (need_leading_space) {
        octl->output_str(" ", octl);
      }  /* if */
      octl->output_str("_Alignas", octl);
      form_attribute_arguments(ap, octl);
      need_leading_space = TRUE;
    }  /* if */
  }  /* for */
  return need_leading_space;
}  /* form_alignas_attributes */


static void form_attributes_for_type(
                                    a_type_ptr                            type,
                                    an_il_to_str_output_control_block_ptr octl)
/*
Render type attributes associated with the given type, unless they are
rendered elsewhere.  Do the output in the way described by octl.
*/
{
#if GNU_EXTENSIONS_ALLOWED
  if (type->may_alias) {
    octl->output_str(" __attribute((__may_alias__))", octl);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (c11_mode) {
    /* _Alignas attributes can be part of a compound literal. */
    (void)form_alignas_attributes(type->source_corresp.attributes,
                                  /*need_leading_space=*/TRUE, octl);
  }  /* if */
}  /* form_attributes_for_type */


static void output_type_attributes(
                         a_type_ptr                            type,
                         a_type_ptr                            stop_type,
                         an_il_to_str_output_control_block_ptr octl)
/*
The given type may contain typerefs that carry attributes: Render those
attributes.  If octl->output_attributes is non-NULL, use that routine to
output the attributes.  Otherwise, only output type attributes that are not
handled elsewhere.  stop_type is a type along the typeref chain (or
stop_type == type if there are no typerefs): Do not render attributes
associated with that type entry or entries under it, except that if stop_type
is a tk_routine entry, attributes on that routine type entry are rendered.
*/
{
  while (type != stop_type) {
    check_assertion(type->kind == (a_type_kind)tk_typeref);
    if (is_typeref_kind(type, trk_for_type_attributes)) {
      if (octl->output_attributes != NULL) {
        octl->output_attributes(type->source_corresp.attributes,
                                al_explicit, /*primary_only=*/FALSE);
      } else {
        form_attributes_for_type(type, octl);
      }  /* if */
    }  /* if */
    type = type->variant.typeref.type;
  }  /* while */
  if (type->kind == (a_type_kind)tk_routine &&
      type->source_corresp.attributes != NULL) {
    if (octl->output_attributes != NULL) {
      octl->output_attributes(type->source_corresp.attributes,
                              al_explicit, /*primary_only=*/FALSE);
    } else {
      form_attributes_for_type(type, octl);
    }  /* if */
  }  /* if */
}  /* output_type_attributes */


static void output_predeclarator_attributes(
                                   a_type_ptr                            type,
                                   an_il_to_str_output_control_block_ptr octl)
/*
Render any "predeclarator" attributes (a GNU feature) for the given type.
Do the output in the way described by octl.
*/
{
  if (octl->output_attributes != NULL) {
    octl->output_attributes(type->source_corresp.attributes,
                            al_predeclarator, /*primary_only=*/FALSE);
  }  /* if */
}  /* output_predeclarator_attributes */


static a_boolean has_predeclarator_attribute(a_type_ptr  type)
/*
Return TRUE if the given type has a "predeclarator" attribute attached to it.
*/
{
  a_boolean         result = FALSE;
  an_attribute_ptr  ap = type->source_corresp.attributes;

  for (; ap != NULL; ap = ap->next) {
    if (ap->syntactic_location == al_predeclarator) {
      result = TRUE;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* has_predeclarator_attribute */


#if !PROTOTYPE_INSTANTIATIONS_IN_IL
static a_source_correspondence_ptr source_corresp_for_template_param(
                                        a_template_param_coordinate_ptr coord);
#endif /* !PROTOTYPE_INSTANTIATIONS_IN_IL */

static void form_template(a_template_ptr	tp,
                          an_il_to_str_output_control_block_ptr octl)

/*
Output a string for a template name.  Do the output in the way described
by octl.
*/
{
  a_source_correspondence_ptr scp = &tp->source_corresp;
  an_il_entry_kind            kind = iek_template;

  /* See whether the template parameter name is remapped in the current
     context. */
  { a_source_correspondence_ptr new_scp;
    new_scp = source_corresp_for_template_param(&tp->coordinates);
    if (new_scp != NULL) {
      scp = new_scp;
      kind = iek_template_parameter;
    }  /* if */
  }
  /* If there is a special output routine for template names, use that.
     Otherwise go through the normal processing. */
  if (octl->output_template_name != NULL) {
    octl->output_template_name((char *)scp, kind);
  } else {
    form_name(scp, kind, octl);
  }  /* if */
}  /* form_template */


#if BACK_END_IS_CP_GEN_BE
static a_routine_type_supplement_ptr suppress_trailing_return_type_for_msvc(
                                    a_type_ptr                            tp,
                                    an_il_to_str_output_control_block_ptr octl)
/*
If the type designated by tp is or points/refers to a function type that
uses the trailing return type syntax and we are generating compilable code
for MSVC, clear the trailing_return_type flag for that function type and
return a pointer to its a_routine_type_supplement entry.  Otherwise, return
NULL.
*/
{
  a_routine_type_supplement_ptr result = NULL;

  if (octl->gen_compilable_code && msvc_is_generated_code_target) {
    /* Skip over cv-qualifiers and pointer/reference types to see if this
       is a type that will result in putting out a function declarator. */
    for (tp = skip_typerefs_not_typedefs_or_type_operators(tp);
         tp->kind == (a_type_kind)tk_pointer;
         tp = skip_typerefs_not_typedefs_or_type_operators(
                                                     tp->variant.pointer.type))
      {}
    if (tp->kind == (a_type_kind)tk_routine) {
      /* This is a function type.  Suppress the trailing return type
         syntax if necessary and return a pointer to the routine type
         supplement so the syntax can be reenabled. */
      a_routine_type_supplement_ptr rtsp = tp->variant.routine.extra_info;
      if (rtsp->trailing_return_type) {
        rtsp->trailing_return_type = FALSE;
        result = rtsp;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* suppress_trailing_return_type_for_msvc */
#else /* !BACK_END_IS_CP_GEN_BE */
#define suppress_trailing_return_type_for_msvc(tp, octl) NULL
#endif /* BACK_END_IS_CP_GEN_BE */


void form_a_template_arg(a_template_arg_ptr                    tap,
                         an_il_to_str_output_control_block_ptr octl)
/*
Output the indicated template argument in the way described by octl.
*/
{
  switch (tap->kind) {
    case tak_type:
      /* Type argument. */
      { a_type_ptr                    tp = tap->variant.type;
        a_routine_type_supplement_ptr rtsp = NULL;
        if (tp == NULL) {
          octl->output_str(error_text(ec_undetermined_type), octl);
          break;
        } else if (is_immediate_class_type(tp) &&
                   tp->variant.class_struct_union.proxy_class) {
          /* Use the original dependent type and not the nonreal proxy
             class for the display.  This matters in cases where the type
             has a template argument list, which would not appear if the
             proxy class were used. */
          tp = class_type_supp(tp)->proxy_of_type;
        } else {
          /* MSVC has a bug that causes spurious errors in some cases when
             a function declarator in a template argument uses a trailing
             return type.  If this type would result in such a declarator,
             temporarily turn off the trailing_return_type flag for the
             type so the declarator will be put out in the traditional form
             and restore it afterward. */
          rtsp = suppress_trailing_return_type_for_msvc(tp, octl);
        }  /* if */
        form_type(tp, octl);
        if (rtsp != NULL) {
          /* coverity[dead_error_line] */
          rtsp->trailing_return_type = TRUE;
        }  /* if */
      }
      break;
    case tak_nontype:
      /* Nontype argument. */
      octl->processing_nontype_template_argument = TRUE;
      if (tap->is_array_bound_of_unknown_type) {
        /* The template argument is a deduced array bound whose type is not
           yet known (we know its value, but we don't yet know its type). */
        check_assertion(!octl->gen_compilable_code);
        octl->output_str("array-bound=", octl);
        form_unsigned_num((a_host_large_unsigned)tap->variant.integer_value,
                          octl);
      } else {
        a_constant_ptr   con = tap->variant.constant;
        an_expr_node_ptr compiler_generated_node = NULL;
        a_boolean        release_con = FALSE;
#if !STANDALONE_UTILITY_PROGRAM
        if (tap->arg_operand != NULL && con == NULL) {
          con = template_arg_operand_constant(tap);
          release_con = con != NULL;
        }  /* if */
#endif /* !STANDALONE_UTILITY_PROGRAM */
        if (tap->arg_operand != NULL && con == NULL) {
          /* The template argument is given by an expression operand (front
             end only). */
          check_assertion(!octl->gen_compilable_code);
          octl->output_str(error_text(ec_quoted_expression), octl);
        } else if (con == NULL) {
          octl->output_str(error_text(ec_undetermined_constant), octl);
        } else {
          a_boolean        need_parens, saved_local_expr_ref;
          an_expr_node_ptr expr, saved_expr;
          a_type_ptr       saved_type;
          check_assertion(con != NULL);
          saved_type = con->type;
          if (type_is(saved_type, tk_typeref) &&
              saved_type->variant.typeref.qualifiers == TQ_CONST) {
            /* Template parameters are implicitly const.  Temporarily drop
               the const-qualification to better render some class-type
               template arguments. */
            con->type = saved_type->variant.typeref.type;
          }  /* if */
          saved_expr = con->expr;
          saved_local_expr_ref = con->local_expr_ref;
          expr = expr_node_from_constant(con);
          /* Check for some cases where the backing expression should be
             ignored. */
          if (constant_is(con, ck_template_param)) {
            /* Never ignore the backing expression of a dependent argument. */
          } else if (octl->suppress_expr_in_nontype_arg
#if BACK_END_IS_CP_GEN_BE
                     || (expr != NULL && !expr->needed_in_cp_gen_be)
#endif /* BACK_END_IS_CP_GEN_BE */
                                                                    ) {
            /* We should just put out the constant value, not the backing
               expression. */
            expr = NULL;
            con->expr = NULL;
            con->local_expr_ref = FALSE;
#if BACK_END_IS_CP_GEN_BE
          } else if (expr != NULL && is_for_cp_gen_be(octl) &&
                     !(constant_is(con, ck_address) && con->implicit_cast)) {
            /* In most cases, we ensure the backing expression is
               suppressed in subsequent references to avoid the overhead of
               repetitively generating the backing expression, which can be
               prohibitive in some cases involving deeply-nested template
               instantiation.  However, casts involving address constants
               are often not valid constant expressions, so we must
               continue to put out the backing expression for an address
               constant with a cast. */
            expr->needed_in_cp_gen_be = FALSE;
            if (constant_is(con, ck_address) &&
                address_base_is(con, abk_variable)) {
              a_variable  *vp = con->variant.address.variant.variable;
              if (!vp->source_corresp.is_class_member ||
                  vp->source_corresp.access == as_public) {
                expr = NULL;
                con->expr = NULL;
                con->local_expr_ref = FALSE;
              }  /*if */
            }  /* if */
#endif /* BACK_END_IS_CP_GEN_BE */
          }  /* if */
          while (expr != NULL && is_constant_node(expr) &&
                 constant_should_be_put_out_as_expr(node_constant(expr))) {
            /* Use expr_node_from_constant so we also reach a backing
               expression referenced indirectly via the local-expr-node-ref
               mechanism (lerk_constant_expr). */
            an_expr_node_ptr backing_expr =
                                  expr_node_from_constant(node_constant(expr));
            if (backing_expr == NULL) break;
            expr = backing_expr;
          }  /* while */
          if (expr != NULL && expr->compiler_generated &&
              node_is_operator(expr, eok_cast) &&
              (tap->param_is_auto || tap->param_is_decltype_auto)) {
            /* Make the cast explicit to ensure that the generated argument
               expression has the correct type for deduction. */
            compiler_generated_node = expr;
            expr->compiler_generated = FALSE;
          }  /* if */
          if (expr != NULL && octl->expr_is_unusable != NULL &&
              octl->expr_is_unusable(expr)) {
            /* The expression uses an undefined, out-of-scope, or
               inaccessible entity.  Just use the constant value. */
            expr = NULL;
            con->expr = NULL;
            con->local_expr_ref = FALSE;
          }  /* if */
          if (expr != NULL && octl->skip_implicit_steps != NULL) {
            expr = octl->skip_implicit_steps(expr);
          }  /* if */
          /* See whether we need parentheses around the argument to prevent
             a ">" operator from being interpreted as the end of the
             argument list or to ensure that an ellipsis applies to the
             entire expression.  (enk_temp_init nodes are exempted because
             parentheses are not needed and because older versions of g++
             have a bug that causes errors compiling the resulting code if
             parentheses are used in that context.) */
          need_parens = octl->gen_compilable_code &&
                        (octl->has_unprotected_gt_or_comma_operation == NULL ||
                         octl->has_unprotected_gt_or_comma_operation(expr) ||
                         (tap->is_pack &&
                          (expr == NULL ||
                           (!node_is(expr, enk_temp_init) &&
                            !(is_constant_node(expr) &&
                              (node_constant_is(expr, ck_aggregate) ||
                               (node_constant_is(expr, ck_template_param) &&
                                tpck_is(node_constant(expr),
                                        tpck_param))))))));
          if (is_any_reference_type(con->type)) {
            /* A reference parameter.  Display specially -- one level of
               indirection must be removed. */
            a_type_ptr targ_type = type_pointed_to(con->type);
            a_boolean  need_closing_paren = FALSE;
            if (tap->param_is_decltype_auto &&
                (is_function_type(targ_type) || is_array_type(targ_type))) {
              /* Enclose the argument in parentheses to prevent the type
                 from decaying to a pointer, which would cause
                 decltype(auto) to deduce the wrong type. */
              octl->output_str("(", octl);
              need_closing_paren = TRUE;
            }  /* if */
            form_lvalue_address_constant(con, need_parens, octl);
            if (need_closing_paren) {
              octl->output_str(")", octl);
            }  /* if */
          } else {
            /* Normal (non-reference) case. */
            a_boolean saved_implicit_cast = con->implicit_cast;
            if (con->kind == (a_constant_repr_kind)ck_ptr_to_member &&
                !pm_constant_is_null(con)) {
              /* Except in a null pointer-to-member constant, a cast is not
                 permitted in a pointer-to-member template argument; ensure
                 that form_pm_constant does not add one to represent an
                 implicit conversion. */
              con->implicit_cast = FALSE;
            }  /* if */
            form_constant(con, need_parens, octl);
            con->implicit_cast = saved_implicit_cast;
          }  /* if */
          con->expr = saved_expr;
          con->local_expr_ref = saved_local_expr_ref;
          con->type = saved_type;
        }  /* if */
        if (compiler_generated_node != NULL) {
          /* Restore the compiler_generated flag that was reset above. */
          compiler_generated_node->compiler_generated = TRUE;
        }  /* if */
        if (release_con) release_local_constant(&con);
      }
      octl->processing_nontype_template_argument = FALSE;
      break;
    case tak_template:
      /* A template template argument. */
      if (tap->variant.templ.ptr == NULL) {
        octl->output_str(error_text(ec_undetermined_template), octl);
      } else {
        form_template(tap->variant.templ.ptr, octl);
      }  /* if */
      break;
    case tak_start_of_pack_expansion:
      break;
    default:
      unexpected_condition();
      break;
  }  /* switch */
  if (tap->is_pack || tap->has_pack_ellipsis) octl->output_str("...", octl);
}  /* form_a_template_arg */


static void skip_start_of_pack_markers(a_template_arg_ptr        *tap_p,
                                       a_template_parameter_ptr  *tpp_p)
/*
Advance *tap_p over any tak_start_of_pack_expansion markers, updating *tpp_p
(if non-NULL) if a skipped marker has no actual pack expansion arguments.
*/
{
  a_template_arg_ptr       tap = *tap_p;
  a_template_parameter_ptr tpp = *tpp_p;

  while (tap != NULL && tap->kind == tak_start_of_pack_expansion) {
    tap = tap->next;
    if (tap == NULL || !(tap->is_pack_element || tap->is_pack)) {
      /* An empty pack expansion.  Advance to the next parameter. */
      if (tpp != NULL) {
        tpp = tpp->next;
      }  /* if */
    }  /* if */
  }  /* while */
  *tap_p = tap;
  *tpp_p = tpp;
}  /* skip_start_of_pack_markers */


static void next_template_arg_and_param(a_template_arg_ptr       *tap_p,
                                        a_template_parameter_ptr *tpp_p)
/*
Advance *tap_p to point to the next template argument (skipping over
tak_start_of_pack_expansion markers) and, if *tpp_p is not NULL, advance it
to the corresponding template parameter.
*/
{
  a_template_arg_ptr       tap = *tap_p;
  a_template_arg_ptr       prev_tap = tap;
  a_template_parameter_ptr tpp = *tpp_p;

  tap = tap->next;
  if (tpp != NULL) {
    /* Adjust the parameter pointer as necessary. */
    if (prev_tap->is_pack) {
      /* The previous argument was a dependent pack, which can match either
         a parameter pack or several non-pack parameters of the same kind
         as the argument. */
      if (tpp->is_pack) {
        tpp = tpp->next;
      } else {
        /* Advance over all template parameters that match the kind of the
           argument pack. */
        a_template_parameter_kind kind;
        if (prev_tap->kind == tak_type) {
          kind = tpk_type;
        } else if (prev_tap->kind == tak_nontype) {
          kind = tpk_nontype;
        } else {
          kind = tpk_template;
        }  /* if */
        while (tpp != NULL && tpp->kind == kind) {
          tpp = tpp->next;
        }  /* while */
      }  /* if */
    } else if (!prev_tap->is_pack_element) {
      /* The previous argument was not part of a pack, so advance to the
         next parameter. */
      tpp = tpp->next;
    } else if (tap == NULL || !(tap->is_pack_element || tap->is_pack)) {
      /* The previous argument was the end of a pack expansion, so advance
         to the next parameter. */
      tpp = tpp->next;
    }  /* if */
  }  /* if */
  skip_start_of_pack_markers(&tap, &tpp);
  *tap_p = tap;
  *tpp_p = tpp;
}  /* next_template_arg_and_param */


void form_template_args(a_template_arg_ptr                    tap,
                        a_template_parameter_ptr              tpp,
                        an_il_to_str_output_control_block_ptr octl)
/*
Output the indicated template argument list (e.g., something like <int,
float>) matching the indicated template parameter list (which may be NULL),
in the way described by octl.  If tap is NULL or if octl indicates that
template arguments should be suppressed, nothing is put out.
*/
{
  ++octl->curr_template_arg_depth;
  if (!octl->suppress_template_args && tap != NULL) {
    a_boolean saved_nontype_tpl_arg =
                                    octl->processing_nontype_template_argument;
#if BACK_END_IS_CP_GEN_BE
    an_output_name_reference_function_ptr saved_output_name_reference =
                                                   octl->output_name_reference;
    a_template_arg_ptr                    parent_arg = octl->curr_template_arg;
    if (octl->gen_compilable_code) {
      /* Name references in template arguments are captured from the first
         use of the instance.  If that use was nested inside a class or
         namespace, the names may have been unqualified or partially
         qualified references to members of that class or namespace or its
         parents.  However, the instance can be used outside that context,
         in which case the names in template arguments would need to be
         fully qualified -- the form captured in the name reference won't
         work.  For safety's sake, we effectively turn off the name
         reference facility for the duration of the template argument
         list. */
      octl->output_name_reference = NULL;
    }  /* if */
#endif /* BACK_END_IS_CP_GEN_BE */
    octl->processing_nontype_template_argument = FALSE;
    octl->output_str("<", octl);
    if (octl->gen_compilable_code) {
      /* When generating compilable code, put out a space after the
         opening "<" to avoid an accidental digraph if the first
         argument begins with a "::" global qualifier. */
      octl->output_str(" ", octl);
    }  /* if */
    if (octl->max_template_arg_depth != 0 &&
        octl->curr_template_arg_depth - 1 == octl->max_template_arg_depth) {
      octl->output_str("/* etc... */", octl);
      octl->incomplete_output = TRUE;
    } else {
      skip_start_of_pack_markers(&tap, &tpp);
      if (tap != NULL) {
        for (;;) {
#if BACK_END_IS_CP_GEN_BE
          tap->parent_arg = parent_arg;
          octl->curr_template_arg = tap;
#endif /* BACK_END_IS_CP_GEN_BE */
          if (tap->kind == tak_nontype && tpp != NULL) {
            a_type_ptr param_type = tpp->variant.nontype.constant->type;
            if (is_auto_template_param_type(param_type)) {
              /* Mark the argument as matching an auto template parameter for
                 possible special processing. */
              tap->param_is_auto = TRUE;
            }  /* if */
            if (is_decltype_auto_template_param_type(param_type)) {
              /* Mark the argument as matching a decltype(auto) template
                 parameter for possible special processing. */
              tap->param_is_decltype_auto = TRUE;
            }  /* if */
          }  /* if */
          form_a_template_arg(tap, octl);
          next_template_arg_and_param(&tap, &tpp);
          /* Stop after the last argument. */
          if (tap == NULL) break;
          /* Put a comma between arguments. */
          octl->output_str(", ", octl);
        }  /* for */
      }  /* if */
    }  /* if */
    octl->output_str(">", octl);
    if (octl->gen_compilable_code) {
      /* When generating compilable code, put out a space after the
         final ">" avoid the possibility of getting ">>" with nested
         template references or with a nontype expression that ends
         with ">". */
      octl->output_str(" ", octl);
    }  /* if */
#if BACK_END_IS_CP_GEN_BE
    octl->output_name_reference = saved_output_name_reference;
    octl->curr_template_arg = parent_arg;
#endif /* BACK_END_IS_CP_GEN_BE */
    octl->processing_nontype_template_argument = saved_nontype_tpl_arg;
  }  /* if */
  --octl->curr_template_arg_depth;
}  /* form_template_args */


static void form_conversion_function_name(
                                    a_routine_ptr                         rout,
                                    an_il_to_str_output_control_block_ptr octl)
/*
Generate the name of the indicated conversion function.  This is done
by generating "operator" followed by the result type.  This may
differ from the name as it appears in the source_corresp.name field
in that it includes typedef names as they appeared in the original
source.
*/
{
  a_type_ptr type = rout->type;
  a_boolean  saved_render_auto_deduction_typerefs =
                                         octl->render_auto_deduction_typerefs;

  octl->output_str("operator ", octl);
  type = skip_typerefs(type);
  octl->render_auto_deduction_typerefs = TRUE;
  type = type->variant.routine.return_type;
  octl->render_auto_deduction_typerefs = saved_render_auto_deduction_typerefs;
  form_type(type, octl);
}  /* form_conversion_function_name */


static a_boolean scp_is_lambda_closure_class(
                              a_source_correspondence               *scp,
                              an_il_entry_kind                      entry_kind)
/*
Return TRUE if the IL entry specified by scp and entry_kind is a class type
for a lambda closure class.
*/
{
  a_boolean	result = FALSE;

  if (entry_kind == iek_type) {
    a_type_ptr	type = (a_type_ptr)scp;
    if (is_immediate_class_type(type) &&
        class_type_supp(type)->is_lambda_closure_class) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* scp_is_lambda_closure_class */


static
a_boolean form_name_if_lambda(a_source_correspondence               *scp,
                              an_il_entry_kind                      entry_kind,
                              an_il_to_str_output_control_block_ptr octl)
/*
Determine whether the entity is the closure class for a lambda, and if so,
generate a name for it and return TRUE (FALSE otherwise).
*/
{
  a_boolean  result = FALSE;

  if (entry_kind == iek_type && !generating_debug_output(octl)) {
    a_type_ptr	type = (a_type_ptr)scp;
    if (is_immediate_class_type(type) &&
        class_type_supp(type)->is_lambda_closure_class) {
      a_boolean  gen_signature = FALSE;
#if !STANDALONE_UTILITY_PROGRAM
      gen_signature = in_front_end;
#endif /* !STANDALONE_UTILITY_PROGRAM */
      result = TRUE;
      octl->output_str("lambda []", octl);
      if (!gen_signature) {
        /* Generating the signature requires the front end's symbol table.
           In back ends and stand-alone utilities we therefore do not render
           the signature and instead identify the lambda through the source
           position. */
        a_source_position  *pos = &type->source_corresp.decl_position;
        octl->output_str(" type at line ", octl);
        form_unsigned_num((a_host_large_unsigned)pos->seq, octl);
        octl->output_str(", col. ", octl);
        form_unsigned_num((a_host_large_unsigned)pos->column, octl);
      } else {
#if STANDALONE_UTILITY_PROGRAM
        unexpected_condition();
#else /* !STANDALONE_UTILITY_PROGRAM */
        /* Get the routine entry for the lambda body. */
        a_routine_ptr  rp = lambda_body_for_closure(type);
        /* Add the routine type of the lambda routine to the output.  The
           routine pointer for the lambda body can be NULL if this routine is
           called after the closure class has been created but before the
           complete lambda parameter list and return type have been scanned. */
        if (rp != NULL) {
          /* Avoid parameters added by lowering such as "this" and a pointer
             to the return value.  This both causes the result to look more
             like it would before lowering and also avoids a potential
             infinite recursion on the "this" parameter: since "this" is a
             pointer to the closure class, displaying its type would invoke
             this code again. */
          a_boolean saved_suppress_flag =
                                  octl->suppress_compiler_generated_parameters;
          octl->suppress_compiler_generated_parameters = TRUE;
          form_type(rp->type, octl);
          octl->suppress_compiler_generated_parameters = saved_suppress_flag;
        }  /* if */
#endif /* STANDALONE_UTILITY_PROGRAM */
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* form_name_if_lambda */


void form_unqualified_name(a_source_correspondence               *scp,
                           an_il_entry_kind                      entry_kind,
                           an_il_to_str_output_control_block_ptr octl)
/*
Output the (unqualified) name of the IL entity whose source correspondence
entry is pointed to by scp.  The IL entry is of the indicated kind.
The output includes template arguments on template classes.
*/
{
  a_const_char *name = unmangled_name_of(scp);

  if (name == NULL) {
    if (form_name_if_lambda(scp, entry_kind, octl)) {
      /* For a lambda, the name will be emitted by form_name_if_lambda. */
    } else {
      /* For entities without names, use <unnamed>. */
      check_assertion(!octl->gen_compilable_code);
      octl->output_str("<", octl);
      octl->output_str(error_text(ec_unnamed), octl);
#if DEBUG
      if (octl->debug_output) {
        octl->output_str("@", octl);
        form_unsigned_hex(possible_lossy_cast_from_pointer(scp), octl);
      }  /* if */
#endif /* DEBUG */
      octl->output_str(">", octl);
    }  /* if */
  } else if (entry_kind == iek_routine &&
             ((a_routine_ptr)scp)->special_kind ==
                                     (a_special_function_kind)sfk_conversion) {
    /* For conversion functions, generate the routine name from the type
       name, to get original typedefs. */
    form_conversion_function_name((a_routine_ptr)scp, octl);
  } else if (entry_kind == iek_routine &&
             ((a_routine_ptr)scp)->is_inheriting_ctor) {
    /* For inheriting constructors, output the base name of the inherited
       constructor. */
    a_routine_ptr rp = (a_routine_ptr)scp;
    rp = get_inh_ctor_originator(rp);
    name = unmangled_name_of(&rp->source_corresp);
    octl->output_str(name, octl);
  } else {
    /* Output the base name. */
    octl->output_str(name, octl);
  }  /* if */
  /* Check for template arguments. */
  if (il_header.source_language == sl_Cplusplus && 
      !octl->suppress_template_args) {
    a_template_arg_ptr	tap = NULL;
    if (entry_kind == iek_type) {
      a_type_ptr  type = (a_type_ptr)scp;
      /* Ignore template parameters and classes whose bodies have been
         eliminated. */
      if (is_immediate_class_type(type)) {
        tap = class_type_supp(type)->template_arg_list;
      } else if (type->kind == (a_type_kind)tk_typeref) {
        tap = type->variant.typeref.extra_info->orig_template_arg_list;
      }  /* if */
    } else if (entry_kind == iek_variable) {
      a_variable_ptr var_ptr = (a_variable_ptr)scp;

      if (var_ptr->template_info != NULL &&
          var_ptr->template_info->template_arg_list != NULL) {
        tap = var_ptr->template_info->template_arg_list;
      }  /* if */
#if DEBUG
    } else if (octl->debug_output && entry_kind == iek_routine) {
      tap = ((a_routine_ptr)scp)->template_arg_list;
#endif /* DEBUG */
    }  /* if */
    if (tap != NULL) {
      /* This is a template class name or template alias name.  Put out the
         template argument list, e.g., "<int, float>". */
      form_template_args(tap, /*tpp=*/NULL, octl);
    }  /* if */
  }  /* if */
}  /* form_unqualified_name */


static void form_namespace_qualifier(
                                    a_namespace_ptr                       nsp,
                                    an_il_to_str_output_control_block_ptr octl)
/*
Output a namespace qualifier (e.g., "N::") that identifies the indicated
namespace.  Do the output in the way described by octl.  Note that the
output_name routine in the control block (if there is one) will not be used
to output any part of the name.  Called only for C++.
*/
{
  if (!nsp->is_namespace_alias && is_namespace_member(nsp)) {
    /* Use recursion to handle nested namespaces. */
    form_namespace_qualifier(parent_namespace_of(nsp), octl);
  }  /* if */
  /* Do the last level. */
  form_unqualified_name(&nsp->source_corresp, iek_namespace, octl);
  octl->output_str("::", octl);
}  /* form_namespace_qualifier */


static void form_class_qualifier(
                  a_type_ptr                            class_type,
                  a_boolean                             for_ptr_to_data_member,
                  an_il_to_str_output_control_block_ptr octl)
/*
Output a class qualifier (e.g., "A::B::") that identifies the indicated
class type.  If for_ptr_to_data_member is TRUE, class_type is the qualifier
in a pointer to data member type.  Do the output in the way described by
octl.  Called only for C++.  */
{
  /* Use the special routine if there is one. */
  if (octl->output_class_qualifier != NULL) {
    octl->output_class_qualifier(class_type, for_ptr_to_data_member);
  } else {
    /* Default processing. */
    a_source_correspondence     *scp = &class_type->source_corresp;
    a_class_type_supplement_ptr ctsp;
    a_boolean                   output_base_name = TRUE;

    /* Use recursion to handle multiple levels of nesting. */
    form_qualifier(scp->parent_scope, octl);
    /* Do the last level. */
    /* Ignore anonymous unions. */
    ctsp = class_type->variant.class_struct_union.extra_info;
#if CHECKING || DEBUG
    if (ctsp == NULL) {
      /* Avoid abort on error case where parent has no supplement, so
         debug output will still come out okay. */
#if DEBUG
      if (octl->debug_output) {
        octl->output_str("<parent with missing IL supplement>", octl);
      } else
#endif /* DEBUG */
      {
        unexpected_condition_str("form_class_qualifier: missing supplement");
      }  /* if */
    } else
#endif /* CHECKING || DEBUG */
    /* Do not insert code here. */
    if (ctsp->anonymous_union_kind != (an_anonymous_union_kind)auk_none) {
      output_base_name = FALSE;
    }  /* if */
    if (output_base_name) {
      if (ctsp != NULL && ctsp->proxy_of_type != NULL) {
        /* Use the original dependent type and not the nonreal proxy class
           for the display.  This matters in cases where the type has a
           template argument list, which would not appear if the proxy
           class were used. */
        scp = &ctsp->proxy_of_type->source_corresp;
      }  /* if */
      form_unqualified_name(scp, iek_type, octl);
      octl->output_str("::", octl);
    }  /* if */
  }  /* if */
}  /* form_class_qualifier */


static void form_enum_qualifier(
                               a_type_ptr                            enum_type,
                               an_il_to_str_output_control_block_ptr octl)
/*
Output an enum qualifier (e.g., "A::B::") that identifies the indicated
enum type.  Do the output in the way described by octl.  Called only for
C++.
*/
{
  /* Use the special routine if there is one. */
  if (octl->output_enum_qualifier != NULL) {
    octl->output_enum_qualifier(enum_type);
  } else {
    /* Default processing. */
    a_source_correspondence *scp = &enum_type->source_corresp;

    /* Use recursion to handle multiple levels of nesting. */
    form_qualifier(scp->parent_scope, octl);
    /* Do the last level. */
    form_unqualified_name(scp, iek_type, octl);
    octl->output_str("::", octl);
  }  /* if */
}  /* form_enum_qualifier */


static void form_qualifier(a_scope_ptr                            scope,
                           an_il_to_str_output_control_block_ptr  octl)
/*
Output a qualifier for an entity declared in the given scope (e.g., the "X::"
in "X::f()").  If scope is NULL or the file scope, no qualifier is emitted.
Note that the output_name routine in the control block (if there is one) will
not be used to output all of the name.  Called only for C++.
*/
{
  if (scope != NULL) {
    switch (scope->kind) {
      case sck_namespace:
        form_namespace_qualifier(scope->variant.assoc_namespace, octl);
        break;
      case sck_class_struct_union:
        form_class_qualifier(scope->variant.assoc_type,
                             /*for_ptr_to_data_member=*/FALSE, octl);
        break;
      case sck_enum:
        form_enum_qualifier(scope->variant.assoc_type, octl);
        break;
      default:
        /* Nothing to be done. */
        break;
    }  /* switch */
  }  /* if */
}  /* form_qualifier */


void form_class_or_namespace_qualifier(
                         a_boolean                             is_class_member,
                         a_parent_class_or_namespace           parent,
                         an_il_to_str_output_control_block_ptr octl)
/*
Output a class or namespace qualifier for an entity, if necessary.
is_class_member and parent give the class/namespace membership information
for the entity: if is_class_member is TRUE, the entity is a member of the
class indicated by parent.class_type.  If is_class_member is FALSE, and
parent.namespace_ptr is non-NULL, the entity is a member of a namespace,
and parent.namespace_ptr points to the namespace.  Note that the
output_name routine in the control block (if there is one) will not
be used to output all of the name.  Called only for C++.
*/
{
  if (is_class_member) {
    form_class_qualifier(parent.class_type, /*for_ptr_to_data_member=*/FALSE,
                         octl);
  } else if (parent.namespace_ptr != NULL) {
    form_namespace_qualifier(parent.namespace_ptr, octl);
  }  /* if */
}  /* form_class_or_namespace_qualifier */

#if MICROSOFT_EXTENSIONS_ALLOWED

void form_property_or_event_name_as_qualifier_if_needed(
                              a_source_correspondence               *scp,
                              an_il_entry_kind                      entry_kind,
                              an_il_to_str_output_control_block_ptr octl)
/*
If the indicated entity is a C++/CLI accessor, output the property or
event name as a qualifier.
*/
{
  a_property_or_event_descr_ptr pedp = NULL;

  if (entry_kind == iek_routine) {
    a_routine_ptr rp = (a_routine_ptr)scp;
    if (rout_is_cli_accessor(rp)) {
      pedp = rp->variant.property_or_event_descr;
    }  /* if */
  } else if (entry_kind == iek_constant) {
    a_constant_ptr con = (a_constant_ptr)scp;
    if (is_unknown_function_constant(con)) {
      pedp = con->variant.template_param.variant
                                     .unknown_function.property_or_event_descr;
    }  /* if */
  }  /* if */
  /* If the entity has an associated property, output a qualifier for the
     property name. */
  if (pedp != NULL) {
    if (pedp->is_static) {
      form_unqualified_name(&pedp->variant.variable->source_corresp,
                            iek_variable, octl);
    } else {
      form_unqualified_name(&pedp->variant.field->source_corresp,
                            iek_field, octl);
    }  /* if */
    octl->output_str("::", octl);
  }  /* if */
}  /* form_property_or_event_name_as_qualifier_if_needed */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

void form_name(a_source_correspondence               *scp,
               an_il_entry_kind                      kind,
               an_il_to_str_output_control_block_ptr octl)
/*
Output the name of the IL entity whose source correspondence
entry is pointed to by scp.  The IL entry is of the indicated kind.
If the entity is a class member, generate a qualified name.  Do the
output in the way described by octl.
*/
{
  /* See if there is a routine to do specialized name output. */
  if (octl->output_name != NULL) {
    /* Use the specialized routine. */
    octl->output_name((char *)scp, kind);
  } else {
    /* Default handling. */
    /* This code isn't suitable for generating compilable output. */
    check_assertion_str(!octl->gen_compilable_code,
                        "form_name: doesn't handle compilable output");
    /* If the name is a member of a class or namespace in C++, output the
       qualifier. */
    if (il_header.source_language == sl_Cplusplus &&
        !scp_is_lambda_closure_class(scp, kind)) {
      /* Suppress the qualifier for lambda closure classes. */
      form_qualifier(scp->parent_scope, octl);
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    form_property_or_event_name_as_qualifier_if_needed(scp, kind, octl);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* Output the base name. */
    form_unqualified_name(scp, kind, octl);
  }  /* if */
}  /* form_name */


static void form_tag_kind(a_type_kind                           kind,
                          an_il_to_str_output_control_block_ptr octl)
/*
Output a string that describes the tag kind for the indicated type, i.e.,
"class" or "enum".  Do the output in the way described by octl.
*/
{
  a_const_char *str = NULL;

  switch (kind) {
    case tk_enum:   str = "enum";   break;
    case tk_class:  str = "class";  break;
    case tk_struct: str = "struct"; break;
    case tk_union:  str = "union";  break;
    default:
#if DEBUG
      if (octl->debug_output) {
        str = "**BAD-TAG-KIND**";
        break;
      }  /* if */
#endif /* DEBUG */
      unexpected_condition_str("form_tag_kind: bad type kind");
  }  /* switch */
  octl->output_str(str, octl);
}  /* form_tag_kind */


static void form_tag_reference(a_type_ptr                            type,
                               an_il_to_str_output_control_block_ptr octl)
/*
Output a reference to a tag, doing output in the way described by octl.
*/
{
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (is_immediate_class_type(type) &&
      class_type_supp(type)->corresponding_basic_type != NULL &&
      is_immediate_enum_type(
                           class_type_supp(type)->corresponding_basic_type)) {
    /* The given type represents a boxed enum type in C++/CLI mode.  The type
       cannot be written explicitly in source form.  Use the unboxed type
       instead. */
    type = class_type_supp(type)->corresponding_basic_type;
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* See if there is a routine to do specialized name output. */
  if (octl->output_name != NULL) {
    /* Use the specialized routine. */
    octl->output_name((char *)type, iek_type);
  } else {
    /* Default handling. */
    if (il_header.source_language == sl_C ||
        ((type_is_unnamed(type) || octl->use_microsoft_format) &&
         !type_is_lambda_closure(type))) {
      /* In C, put "struct", "union", or "enum" on tags.  In C++, do it
         only for unnamed tags or when emulating the Microsoft __FUNCSIG__
         format (but not lambda closure classes). */
      form_tag_kind(type->kind, octl);
      octl->output_str(" ", octl);
    }  /* if */
    form_name(&type->source_corresp, iek_type, octl);
  }  /* if */
}  /* form_tag_reference */


a_const_char *int_kind_name_full(an_integer_kind      kind,
                                 ARG_UNUSED a_boolean for_generated_code)
/*
Return a string for the name of an integer kind.  Return a string beginning
with "**BAD" for a bad integer kind.  for_generated_code is TRUE if the
name is intended for use in code generated by the C-generating back end or
C++-generating back end.
*/
{
  a_const_char *p;

#if !STANDALONE_UTILITY_PROGRAM
  /* In some modes, plain char is equivalent to signed char.  In such
     modes, output just "char" for the equivalent type. */
  if (kind == plain_char_int_kind) kind = (an_integer_kind)ik_char;
#endif /* !STANDALONE_UTILITY_PROGRAM */
  switch (kind) {
    case ik_char:               p = "char";               break;
    case ik_signed_char:        p = "signed char";        break;
    case ik_unsigned_char:      p = "unsigned char";      break;
    case ik_short:              p = "short";              break;
    case ik_unsigned_short:     p = "unsigned short";     break;
    case ik_int:                p = "int";                break;
    case ik_unsigned_int:       p = "unsigned int";       break;
    case ik_long:               p = "long";               break;
    case ik_unsigned_long:      p = "unsigned long";      break;
#if LONG_LONG_ALLOWED
    case ik_long_long:          p = "long long";
                                goto common_long_long_processing;
    case ik_unsigned_long_long: p = "unsigned long long";
common_long_long_processing:
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
                                /* When generating code for MSVC++,
                                   use "__int64" for "long long". */
                                if (for_generated_code &&
                                    msvc_is_generated_code_target) {
                                  if (kind == (an_integer_kind)ik_long_long) {
                                    p = "__int64";
                                  } else {
                                    p = "unsigned __int64";
                                  }  /* if */
                                }  /* if */
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
                                break;
#endif /* LONG_LONG_ALLOWED */
#if INT128_EXTENSIONS_ALLOWED
    /* Currently, we only accept 128-bit integer types in GNU modes, where
       such types can be denoted using the predeclared typedefs __int128_t and
       __uint128_t.  Starting with version 4.6, however, "__int128" and
       "unsigned __int128" are also accepted.  This code will need revision
       when such types are enabled in other modes. */
    case ik_int128:
                                if (gnu_mode) {
#if !STANDALONE_UTILITY_PROGRAM
                                  check_assertion(int128_extensions_enabled);
#endif /* !STANDALONE_UTILITY_PROGRAM */
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
                                  if (clang_is_generated_code_target ||
                                      (gcc_is_generated_code_target &&
                                       gnu_target_version_number >= 40600)) {
                                    p = "__int128";
                                  } else
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
                                  /* Do not insert code here. */
                                  {
                                    p = "__int128_t";
                                  }  /* if */
                                } else {
                                  p = "**128-BIT SIGNED INTEGER**";
                                }  /* if */
                                break;
    case ik_unsigned_int128:
                                if (gnu_mode) {
#if !STANDALONE_UTILITY_PROGRAM
                                  check_assertion(int128_extensions_enabled);
#endif /* !STANDALONE_UTILITY_PROGRAM */
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
                                  if (clang_is_generated_code_target ||
                                      (gcc_is_generated_code_target &&
                                       gnu_target_version_number >= 40600)) {
                                    p = "unsigned __int128";
                                  } else
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
                                  /* Do not insert code here. */
                                  {
                                    p = "__uint128_t";
                                  }  /* if */
                                } else {
                                  p = "**128-BIT UNSIGNED INTEGER**";
                                }  /* if */
                                break;
#endif /* INT128_EXTENSIONS_ALLOWED */
    case ik_bit_precise:        p = "_BitInt";            break;
    case ik_unsigned_bit_precise:
                                p = "unsigned _BitInt";   break;
    default:                    p = "**BAD-INT-KIND**";
  }  /* switch */
  return p;
}  /* int_kind_name_full */


a_const_char *int_kind_name(an_integer_kind kind)
/*
Return a string for the name of an integer kind.  This is an interface
to int_kind_name_full for the normal case, i.e., when the name is not
intended for use in code generated by the C-generating back end or
C++-generating back end (for that, see int_kind_name_full).
*/
{
  a_const_char *p = int_kind_name_full(kind, /*for_generated_code=*/FALSE);
  return p;
}  /* int_kind_name */


static a_const_char *int_type_name_full(a_type_ptr type,
                                        a_boolean  for_generated_code)
/*
Return a string for the name of the given integer type.  The standard cases
are delegated to int_kind_name_full, but for intrinsic Microsoft __intN types
(Visual C++ 6.0) the work is done here.  for_generated_code is TRUE if the
name is intended for use in code generated by the C-generating back end or
C++-generating back end.
*/
{
  a_const_char     *result;

  check_assertion(type_is(type, tk_integer));
  if (is_bit_precise_kind(type->variant.integer.int_kind)) {
    pos_in_temp_text_buffer = 0;
    if (type->variant.integer.int_kind == ik_unsigned_bit_precise) {
      put_str_to_temp_text_buffer("unsigned ");
    }  /* if */
    put_str_to_temp_text_buffer("_BitInt(");
    put_uint_to_temp_text_buffer(
                    (unsigned long long)integer_type_supp(type)->bit_width);
    put_ch_to_temp_text_buffer(')');
    put_ch_to_temp_text_buffer('\0');
    result = temp_text_buffer;
  } else
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (type->variant.integer.microsoft_sized_int_type) {
    an_integer_kind  kind = type->variant.integer.int_kind;

    if (kind == targ_int8_int_kind) {
      result = "__int8";
    } else if (kind == targ_unsigned_int8_int_kind) {
      result = "unsigned __int8";
    } else if (kind == targ_int16_int_kind) {
      result = "__int16";
    } else if (kind == targ_unsigned_int16_int_kind) {
      result = "unsigned __int16";
    } else if (kind == targ_int32_int_kind) {
      result = "__int32";
    } else if (kind == targ_unsigned_int32_int_kind) {
      result = "unsigned __int32";
    } else if (kind == targ_int64_int_kind) {
      result = "__int64";
    } else if (kind == targ_unsigned_int64_int_kind) {
      result = "unsigned __int64";
    } else {
      result = "**BAD-SIZED-INT-KIND**";
    }  /* if */
  } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Do not insert code here. */
  {
    result = int_kind_name_full(type->variant.integer.int_kind,
                                for_generated_code);
  }  /* if */
  return result;
}  /* int_type_name_full */


a_const_char *int_type_name(a_type_ptr type)
/*
Return a string for the name of the given integer type.  This is an interface
to int_type_name_full for the normal case, i.e., when the name is not intended
for use in code generated by the C-generating back end or C++-generating
back end (for that, see int_type_name_full).
*/
{
  a_const_char *result = int_type_name_full(type,
                                            /*for_generated_code=*/FALSE);
  return result;
}  /* int_type_name */


static void form_int_type_name(a_type_ptr                            type,
                               an_il_to_str_output_control_block_ptr octl)
/*
Output a string for the name of an integer kind, doing the output in the
way described by octl.
*/
{
  a_const_char     *str = NULL;
  an_integer_kind  kind = type->variant.integer.int_kind;

  if (octl->gen_pcc_code) {
    if (kind == (an_integer_kind)ik_signed_char) {
      /* In pcc mode, "signed" doesn't exist, so this must be a plain
         char. */
      str = "char";
    } else if (kind == (an_integer_kind)ik_unsigned_char &&
               !il_header.plain_chars_are_signed) {
      /* In pcc mode, "char" is turned into signed char or unsigned char.
         If unsigned char is the default, we don't have to say "unsigned". */
      str = "char";
    }  /* if */
  }  /* if */
  if (kind == (an_integer_kind)ik_unsigned_int && octl->gen_compilable_code
#if MICROSOFT_EXTENSIONS_ALLOWED
      && !type->variant.integer.microsoft_sized_int_type
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                                        ) {
    /* When generating compilable code, use "unsigned" instead of
       "unsigned int".  This is necessary when doing vacuous destructors. */
    str = "unsigned";
  } else if (str == NULL) {
    str = int_type_name_full(type, (a_boolean)octl->gen_compilable_code);
  }  /* if */
#if CHECKING
  if (*str == '*'
#if DEBUG
      && !octl->debug_output
#endif /* DEBUG */
                            ) {
    internal_error("form_int_type_name: bad integer kind");
  }  /* if */
#endif /* CHECKING */
  octl->output_str(str, octl);
}  /* form_int_type_name */


a_const_char *float_kind_name(a_float_kind kind,
                              a_boolean    use_C_form)
/*
Return a string for the name of a float kind.  In the case of C++23
extended floating point types, return the corresponding C form (e.g.,
_Float32 instead of std::float32_t) if use_C_form is TRUE.  Return a string
beginning with "**BAD" for a bad float kind.
*/
{
  a_const_char *p;

  if (clang_version_is(any_version)) {
    /* The standard extended floating point typedefs are (as of version
       19.1.0) not supported by clang. */
    use_C_form = TRUE;
  }  /* if */
  switch (kind) {
    case fk_float16:      p = "_Float16";           break;
    case fk_fp16:         p = "__fp16";             break;
    case fk_float:        p = "float";              break;
    case fk_float32x:     p = "_Float32x";          break;
    case fk_double:       p = "double";             break;
    case fk_float64x:     p = "_Float64x";          break;
    case fk_long_double:  p = "long double";        break;
    case fk_float80:      p = "__float80";          break;
    case fk_float128:     p = "__float128";         break;
    case fk_std_bfloat16: p = (use_C_form
#if !STANDALONE_UTILITY_PROGRAM
                               || !extended_float_types
#endif /* !STANDALONE_UTILITY_PROGRAM */
                                                       )
                            ? "__bf16"
                            : "std::bfloat16_t";    break;
    case fk_std_float16:  p = use_C_form
                            ? "_Float16"
                            : "std::float16_t";     break;
    case fk_std_float32:  p = use_C_form
                            ? "_Float32"
                            : "std::float32_t";     break;
    case fk_std_float64:  p = use_C_form
                            ? "_Float64"
                            : "std::float64_t";     break;
    case fk_std_float128: p = use_C_form
                            ? "_Float128"
                            : "std::float128_t";    break;
    default:              p = "**BAD-FLOAT-KIND**";
  }  /* switch */
  return p;
}  /* float_kind_name */


a_const_char *type_transforming_intrinsic_name(a_typeref_kind kind)
/*
Return a string for the name of type type-transforming intrinsic specified by
kind.
*/
{
  a_const_char *p = NULL;

  switch (kind) {
    case trk_add_lvalue_reference: p = "__add_lvalue_reference"; break;
    case trk_add_pointer:          p = "__add_pointer";          break;
    case trk_add_rvalue_reference: p = "__add_rvalue_reference"; break;
    case trk_decay:                p = "__decay";                break;
    case trk_is_underlying_type:   p = "__underlying_type";      break;
    case trk_make_signed:          p = "__make_signed";          break;
    case trk_make_unsigned:        p = "__make_unsigned";        break;
    case trk_remove_all_extents:   p = "__remove_all_extents";   break;
    case trk_remove_const:         p = "__remove_const";         break;
    case trk_remove_cv:            p = "__remove_cv";            break;
    case trk_remove_cvref:         p = "__remove_cvref";         break;
    case trk_remove_extent:        p = "__remove_extent";        break;
    case trk_remove_pointer:       p = "__remove_pointer";       break;
    case trk_remove_reference:     p = "__remove_reference";     break;
    case trk_remove_reference_t:   p = "__remove_reference_t";   break;
    case trk_remove_restrict:      p = "__remove_restrict";      break;
    case trk_remove_volatile:      p = "__remove_volatile";      break;
    default:                       unexpected_condition();       break;
  }  /* switch */
  return p;
}  /* type_transforming_intrinsic_name */


#if BACK_END_IS_C_GEN_BE
#if LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C
#if ISSUE_WARNING_ON_LONG_DOUBLE_AS_DOUBLE
STATIC_THREAD a_boolean double_for_long_double_warning_issued;
#endif /* ISSUE_WARNING_ON_LONG_DOUBLE_AS_DOUBLE */
#endif /* LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C */
#endif /* BACK_END_IS_C_GEN_BE */


static void form_float_kind_name(a_float_kind                          kind,
                                 an_il_to_str_output_control_block_ptr octl)
/*
Output a string for the name of a float kind, doing the output in the
way described by octl.
*/
{
  a_const_char *str;

#if BACK_END_IS_C_GEN_BE
#if LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C
  if (is_for_c_gen_be(octl)) {
    if (kind == (a_float_kind)fk_long_double) {
      /* When generating K&R C from the C-generating back end, put out
         "double" for "long double" and issue a one-time-only warning. */
#if ISSUE_WARNING_ON_LONG_DOUBLE_AS_DOUBLE
      if (!double_for_long_double_warning_issued) {
        pos_warning(ec_double_for_long_double, &null_source_position);
        double_for_long_double_warning_issued = TRUE;
      }  /* if */
#endif /* ISSUE_WARNING_ON_LONG_DOUBLE_AS_DOUBLE */
      kind = (a_float_kind)fk_double;
    }  /* if */
  }  /* if */
#endif /* LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C */
#endif /* BACK_END_IS_C_GEN_BE */
  str = float_kind_name(kind, is_for_c_gen_be(octl) || C_mode()
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
                        || (octl->gen_compilable_code &&
                            gcc_is_generated_code_target &&
                            (gcc_version_is(>= 70000) ||
                             gpp_version_is(>= 130000)))
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
                                                                );
#if CHECKING
  if (*str == '*'
#if DEBUG
      && !octl->debug_output
#endif /* DEBUG */
                            ) {
    internal_error("form_float_kind_name: bad float kind");
  }  /* if */
#endif /* CHECKING */
  octl->output_str(str, octl);
}  /* form_float_kind_name */


void form_type_qualifier(
                     a_type_qualifier_set                  qualifiers,
                     ARG_UNUSED a_upc_block_size           upc_block_size,
                     a_boolean                             need_trailing_space,
                     an_il_to_str_output_control_block_ptr octl)
/*
Output a string for the type qualifiers in the given qualifier set.
If the qualifier set is empty, put out nothing.  If need_trailing_space
is TRUE, put out a space after the type qualifier (if one is put out).
Do the output in the way described by octl.
*/
{
  a_boolean qualifier_put_out = FALSE;
#if NAMED_ADDRESS_SPACES_ALLOWED
  a_named_address_space_id
            nas_id = named_address_space_from_qualifier_set(qualifiers);
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */

/* Local macro that determines whether a given qualifier is present,
   and if so outputs the appropriate string. */
#define output_qualifier(flag, string)					\
{									\
  if ((qualifiers & flag) != 0) {					\
    if (qualifier_put_out) octl->output_str(" ", octl);			\
    qualifier_put_out = TRUE;						\
    octl->output_str(string, octl);					\
  }  /* if */								\
}  /* output_qualifier */

  if (octl->gen_pcc_code) {
    /* Qualifiers are suppressed when generating K&R C. */
  } else {
#if BACK_END_IS_C_GEN_BE && SUPPRESS_CONST_IN_GENERATED_C
    /* Suppress "const" in the output of the C-generating back end. */
    if (is_for_c_gen_be(octl)) qualifiers &= ~TQ_CONST;
#endif /* BACK_END_IS_C_GEN_BE && SUPPRESS_CONST_IN_GENERATED_C */
    output_qualifier(TQ_C11_ATOMIC, "_Atomic");
    output_qualifier(TQ_CONST, "const"); /*lint !e774*/
    output_qualifier(TQ_VOLATILE, "volatile");
#if SUPPRESS_RESTRICT_IN_GENERATED_CODE
    /* Suppress "restrict" in generated compilable code. */
    if (octl->gen_compilable_code) qualifiers &= ~TQ_RESTRICT;
#endif /* SUPPRESS_RESTRICT_IN_GENERATED_CODE */
    output_qualifier(TQ_RESTRICT, (char *)(use_gnu_form() ? "__restrict__" :
                                                            "restrict"));
    { a_boolean  output_nullability = !octl->gen_compilable_code;
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
      /* Nullability qualifiers are only accepted by Clang and can safely be
         ignored.  So when generating compilable code, do not render them
         unless we are generating code for Clang. */
      output_nullability |= clang_is_generated_code_target;
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
      if (output_nullability) {
        output_qualifier(TQ_NULLABLE, "_Nullable");
        output_qualifier(TQ_NONNULL, "_Nonnull");
        output_qualifier(TQ_NULL_UNSPECIFIED, "_Null_unspecified");
      }  /* if */
    }
#if MICROSOFT_EXTENSIONS_ALLOWED
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
    if (octl->gen_compilable_code &&
        !microsoft_dialect_is_generated_code_target) {
      /* Suppress "__unaligned" in generated compilable code. */
      qualifiers &= ~TQ_UNALIGNED;
    }  /* if */
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
    output_qualifier(TQ_UNALIGNED, "__unaligned");
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NEAR_AND_FAR_ALLOWED
#if SUPPRESS_NEAR_AND_FAR_IN_GENERATED_CODE
    if (octl->gen_compilable_code) {
      /* Suppress "__near" and "__far" in generated compilable code. */
      qualifiers &= ~(TQ_NEAR | TQ_FAR);
    }  /* if */
#endif /* SUPPRESS_NEAR_AND_FAR_IN_GENERATED_CODE */
    output_qualifier(TQ_NEAR,
                     (char *)(use_microsoft_form() ? "__near" : "near"));
    output_qualifier(TQ_FAR,
                     (char *)(use_microsoft_form() ? "__far" : "far"));
#endif /* NEAR_AND_FAR_ALLOWED */
#if UPC_EXTENSIONS_ALLOWED
    output_qualifier(TQ_UPC_STRICT, "strict");
    output_qualifier(TQ_UPC_RELAXED, "relaxed");
    if (qualifiers & TQ_UPC_SHARED) {
      output_qualifier(TQ_UPC_SHARED, "shared");
      if (upc_block_size == UPC_BLOCK_SIZE_NONE) {
        /* Nothing to be done. */
      } else if (upc_block_size == UPC_BLOCK_SIZE_BLOCK) {
        octl->output_str("[*]", octl);
      } else {
        octl->output_str("[", octl);
        form_unsigned_num((a_host_large_unsigned)upc_block_size, /*lint !e571*/
                          octl);
        octl->output_str("]", octl);
      }  /* if */
    }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
#if NAMED_ADDRESS_SPACES_ALLOWED
    if (nas_id != 0) {
      if (qualifier_put_out) octl->output_str(" ", octl);
      octl->output_str(named_address_spaces[nas_id].name, octl);
      qualifier_put_out = TRUE;
    }  /* if */
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
    /* Put out a trailing space if required. */
    if (need_trailing_space && qualifier_put_out) octl->output_str(" ", octl);
  }  /* if */
#undef output_qualifier
}  /* form_type_qualifier */

#if MICROSOFT_EXTENSIONS_ALLOWED

a_const_char *cli_managed_class_tag_keyword(a_type_ptr type)
/*
Return a string that describes the tag kind for the indicated CLI class type.
The caller should have already determined that type is a CLI class type.
*/
{
  a_const_char                 *result = NULL;
  a_class_type_supplement_ptr  ctsp = class_type_supp(type);

  switch (type->kind) {
    case tk_class:
      switch (ctsp->cli_class_type_kind) {
        case cctk_ref:       result = "ref class";       break;
        case cctk_value:     result = "value class";     break;
        case cctk_interface: result = "interface class"; break;
        default:             unexpected_condition();
      }  /* switch */
      break;
    case tk_struct:
      switch (ctsp->cli_class_type_kind) {
        case cctk_ref:       result = "ref struct";        break;
        case cctk_value:     result = "value struct";      break;
        case cctk_interface: result = "interface struct";  break;
        default:             unexpected_condition();
      }  /* switch */
      break;
    default:
      unexpected_condition();
  }  /* switch */
  return result;
}  /* cli_managed_class_tag_keyword */


void form_pointer_modifiers(a_pointer_modifier_set                 modifiers,
                            an_il_to_str_output_control_block_ptr  octl)
/*
Output a string for the pointer modifiers in the given modifier set.  If the
set is empty, put out nothing.  No trailing space is added to the output.
Do the output in the way described by octl.
*/
{
  if (modifiers != PM_NONE) {
    a_boolean  modifier_put_out = FALSE;
/* Local macro that determines whether a given qualifier is present,
   and if so outputs the appropriate string. */
#define output_modifier(flag, string)                                        \
{                                                                            \
  if ((modifiers & flag) != 0) {                                             \
    if (modifier_put_out) octl->output_str(" ", octl);                       \
    modifier_put_out = TRUE;                                                 \
    octl->output_str(string, octl);                                          \
  }  /* if */                                                                \
}  /* output_modifier */
    output_modifier(PM_PTR32, "__ptr32");
    output_modifier(PM_PTR64, "__ptr64");
    output_modifier(PM_SPTR, "__sptr");
    output_modifier(PM_UPTR, "__uptr");
#undef output_modifier
  }  /* if */
}  /* form_pointer_modifiers */


void form_calling_convention(
                     a_calling_convention                  calling_convention,
                     an_il_to_str_output_control_block_ptr octl)
/*
Output a string for a Microsoft-specific calling convention.
Put out a space after the calling convention (if one is put out).
Do the output in the way described by octl.  If GNU C code is generated,
nothing should be done here since the calling convention will be issued as
an attribute (in that case microsoft_dialect_is_generated_code_target should
be FALSE).
*/
{
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
  if (octl->gen_compilable_code &&
      !microsoft_dialect_is_generated_code_target) {
    /* The Microsoft keywords should only be suppressed in compilable code.
       Not, for example, in diagnostics. */
  } else
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
  /* Do not insert code here. */
  if (calling_convention != (a_calling_convention)cc_default) {
    /* Put out nothing for the default calling convention. */
    if (calling_convention == (a_calling_convention)cc_thiscall &&
        is_for_c_gen_be(octl)) {
      /* Suppress __thiscall in generated C code (it's not valid in C because
         there's no "this" pointer). */
    } else {
      octl->output_str(calling_convention_names[(int)calling_convention],
                       octl);
      /* Put out a trailing space. */
      octl->output_str(" ", octl);
    }  /* if */
  }  /* if */
}  /* form_calling_convention */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Template parameters are primarily characterized by their coordinates: the
template nesting depth at which they are introduced and the position in the
associated template declaration clause.  Such parameters are represented by
a_constant or a_type entries whose source_correspondence entry may not always
contain the correct name, since the name can vary for a given coordinate set.
For example:
   template<int I> struct A {};
   template<int I> struct B { A<I> *a; };  // (1)
   template<int K> struct C { A<K> *a; };  // (2)
Only one type entry represents the type A<#1> in (1) and (2).  The source
correspondence entry for the first argument "#1" will name "I" because that
was the name used at the first point of instantiation of A<#1>.  However,
the C++ generating back (for example) needs to produce a name that is valid in
the context of use.  To achieve this, we establish a mapping from coordinates
to source correspondence entries, and we consult the map prior to emitting
a_type or a_constant entries that stand for template parameters.
*/

#if !BACK_END_IS_CP_GEN_BE
/*lint -esym(751,*a_template_param_map_level)*/
#endif /* !BACK_END_IS_CP_GEN_BE */
typedef struct a_template_param_map_level *a_template_param_map_level_ptr;
typedef struct a_template_param_map_level {
  /* A growable structure mapping (for a certain template nesting depth) the
     position of a template parameter to its current name. */
  a_template_param_list_pos
		max_position;
			/* The largest position ordinal for which a mapping is
			   stored at this depth. */
  a_source_correspondence_ptr
		*source_corresp;
			/* Points to an array of max_position possibly NULL
			   pointers to source correspondence entries. */
} a_template_param_map_level;


STATIC_THREAD a_template_param_map_level_ptr template_param_map;
			/* A pointer to the two-level lookup structure. */

STATIC_THREAD a_template_nesting_depth
                template_param_map_max_level;
			/* The size of the first level (i.e., the maximum
			   template nesting depth for which a parameter
			   coordinate has been mapped). */

#if BACK_END_IS_CP_GEN_BE

/*
The following structure is used to record the previous mapping for a
template parameter name so it can be restored after being overwritten by a
friend declaration inside a class template.  This happens when the friend
declaration is matched to an existing template declaration: the coordinates
will be those of the original declaration of the friend, so the mappings
from the friend declaration overwrite those of the class template
containing the friend declaration, and they must be restored so that
subsequent references to the containing class template's parameters will
use the correct names.
*/
typedef struct a_saved_template_param_mapping
                                           *a_saved_template_param_mapping_ptr;
typedef struct a_saved_template_param_mapping {
  a_saved_template_param_mapping_ptr
		next;	/* Points to the next saved mapping, either on the
			   active or the free list. */
  a_template_param_coordinate
		coord;	/* The coordinates of the mapped parameter. */
  a_source_correspondence_ptr
		scp;	/* The source correspondence used for the mapped
			   parameter. */
} a_saved_template_param_mapping;


STATIC_THREAD a_saved_template_param_mapping_ptr
		saved_template_param_mappings;
			/* A stack of template parameter mappings to be
			   restored by restore_template_param_mapping.
			   Each call to save_template_param_mappings begins
			   the mappings for a new template or generic
			   lambda, which will be pushed on top of the
			   mappings for any containing templates or
			   lambdas. */

STATIC_THREAD a_saved_template_param_mapping_ptr
		avail_template_param_mappings;
			/* Free template parameter mappings that are
			   available for reuse. */

STATIC_THREAD int
		depth_saved_templ_param_mappings;
			/* When non-zero, remap_template_param will save the
			   existing mapping before overwriting it with the
			   new mapping. */


a_saved_template_param_mapping_ptr save_template_param_mappings(void)
/*
Begin saving the existing template parameter mappings before overwriting
them in remap_template_param.  Return the current top of the mapping stack,
which should be passed to restore_template_param_mappings to pop the stack
back to this point.
*/
{
  ++depth_saved_templ_param_mappings;
  return saved_template_param_mappings;
}  /* save_template_param_mappings */


void restore_template_param_mappings(
                                   a_saved_template_param_mapping_ptr prev_top)
/*
Restore any template parameter mappings that were overwritten by
remap_template_param for the most recent template or generic lambda.  The
mappings for the template or lambda containing the most recent one begin
with prev_top, which was returned by save_template_param_mappings, so the
stack is popped back to that point.
*/
{
  a_template_param_map_level_ptr level;

  check_assertion(depth_saved_templ_param_mappings > 0 &&
                  (depth_saved_templ_param_mappings > 1 ||
                   prev_top == NULL));
  --depth_saved_templ_param_mappings;
  /* Loop through the active mappings and restore the table to its previous
     contents. */
  while (saved_template_param_mappings != prev_top) {
    a_saved_template_param_mapping_ptr saved_mapping =
                                                 saved_template_param_mappings;
    saved_template_param_mappings = saved_mapping->next;
    check_assertion(saved_mapping->coord.depth <=
                                                 template_param_map_max_level);
    level = &template_param_map[saved_mapping->coord.depth-1];
    check_assertion(saved_mapping->coord.position <= level->max_position);
    level->source_corresp[saved_mapping->coord.position-1] =
                                                            saved_mapping->scp;
    /* Put the entry on the free list for later reuse. */
    saved_mapping->next = avail_template_param_mappings;
    avail_template_param_mappings = saved_mapping;
  }  /* while */
}  /* restore_template_param_mappings */


void remap_template_param(a_template_param_coordinate_ptr  coord,
                          a_source_correspondence_ptr      scp)
/*
Associate the given template parameter coordinate with the given source
correspondence entry.
*/
{
  a_template_param_map_level_ptr     level;
  a_saved_template_param_mapping_ptr saved_mapping;

  if (coord->depth == 0) {
    /* A parameter of a template template parameter; no remapping needed. */
  } else {
    /* First find/create the appropriate depth/level: */
    if (template_param_map == NULL) {
      template_param_map_max_level = (coord->depth > 5) ? 2*coord->depth : 10;
      template_param_map = (a_template_param_map_level_ptr)
         alloc_resizable_buffer(sizeof(a_template_param_map_level) *
                                size_t_arg(template_param_map_max_level));
      memzero(template_param_map,
              (sizeof(a_template_param_map_level) *
               size_t_arg(template_param_map_max_level)));
    } else if (coord->depth > template_param_map_max_level) {
      a_template_nesting_depth new_max_level = 2*coord->depth;
      template_param_map =
          (a_template_param_map_level_ptr)realloc_buffer(
                                    (char*)template_param_map,
                                    (sizeof(a_template_param_map_level) *
                                     size_t_arg(template_param_map_max_level)),
                                    (sizeof(a_template_param_map_level) *
                                     size_t_arg(new_max_level)));
      memzero(&template_param_map[template_param_map_max_level],
              (sizeof(a_template_param_map_level) *
               size_t_arg(new_max_level)) -
              (sizeof(a_template_param_map_level) *
               size_t_arg(template_param_map_max_level)));
      template_param_map_max_level = new_max_level;
    }  /* if */
    level = &template_param_map[coord->depth-1];
    /* Then add the new mapping at the right position: */
    if (level->max_position == 0) {
      level->max_position = (coord->position > 5) ? 2*coord->position : 10;
      level->source_corresp = (a_source_correspondence_ptr*)
                  alloc_resizable_buffer(
                      sizeof(a_source_correspondence_ptr)*level->max_position);
      memzero(level->source_corresp,
              sizeof(a_source_correspondence_ptr)*level->max_position);
    } else if (coord->position > level->max_position) {
      a_template_param_list_pos new_max_pos = 2*coord->position;
      level->source_corresp = (a_source_correspondence_ptr*)realloc_buffer(
                       (char*)level->source_corresp,
                       sizeof(a_source_correspondence_ptr)*level->max_position,
                       sizeof(a_source_correspondence_ptr)*new_max_pos);
      memzero(&level->source_corresp[level->max_position],
              sizeof(a_source_correspondence_ptr)*new_max_pos -
                      sizeof(a_source_correspondence_ptr)*level->max_position);
      level->max_position = new_max_pos;
    }  /* if */
    if (depth_saved_templ_param_mappings > 0) {
      /* Save the old mapping so it can be restored later. */
      if (avail_template_param_mappings != NULL) {
        /* Reuse an existing entry. */
        saved_mapping = avail_template_param_mappings;
        avail_template_param_mappings = saved_mapping->next;
      } else {
        /* The free list is empty, so allocate a new entry. */
        saved_mapping = (a_saved_template_param_mapping_ptr)
                         alloc_general(sizeof(a_saved_template_param_mapping));
      }  /* if */
      /* Link the entry to the active list. */
      saved_mapping->next = saved_template_param_mappings;
      saved_template_param_mappings = saved_mapping;
      saved_mapping->coord = *coord;
      saved_mapping->scp = level->source_corresp[coord->position-1];
    }  /* if */
    level->source_corresp[coord->position-1] = scp;
  }  /* if */
}  /* remap_template_param */


void unmap_template_param(a_template_param_coordinate_ptr  coord)
/*
Uninstall any mapping for the given template parameter coordinate.  Presumably
this will cause the source correspondence of the a_type or a_constant entry
for the template parameter to be used.
*/
{
  remap_template_param(coord, (a_source_correspondence_ptr)NULL);
}  /* unmap_template_param */

#endif /* BACK_END_IS_CP_GEN_BE */

#if !PROTOTYPE_INSTANTIATIONS_IN_IL
/* This routine is used outside il_to_str.c only with prototype
   instantiations. */
static
#endif /* !PROTOTYPE_INSTANTIATIONS_IN_IL */
a_source_correspondence_ptr source_corresp_for_template_param(
                                        a_template_param_coordinate_ptr coord)
/*
Look up the given template parameter coordinates in the template parameter map
to find a source correspondence entry that will produce a meaningful name in
the current context.  Return NULL if the template parameter is not remapped
in the current context.
*/
{
  a_source_correspondence_ptr     result;

  if (template_param_map == NULL ||
      coord->depth > template_param_map_max_level ||
      coord->depth == 0 || /* Template template parameter. */
      coord->position > template_param_map[coord->depth - 1].max_position) {
    result = NULL;
  } else {
    result = template_param_map[coord->depth - 1].
                                          source_corresp[coord->position - 1];
  }  /* if */
  return result;
}  /* source_corresp_for_template_param */


an_expr_node_ptr decltype_arg(a_type_ptr  type)
/*
The given type represents a typeref-based type operator (e.g., decltype,
typeof, splice, or pack-index).  Return its argument expression if
available, or NULL otherwise.
*/
{
  an_expr_node_ptr  expr = type->variant.typeref.extra_info->expr;

  if (expr == NULL &&
      (is_typeref_kind(type, trk_is_decltype) ||
       is_typeref_kind(type, trk_is_splice) ||
       is_typeref_kind(type, trk_is_typeof_with_expression) ||
       is_typeref_kind(type, trk_pack_index))) {
    /* See if the expression can be found in a local function scope. */
    a_scope_ptr  scope;
    if (type->source_corresp.enclosing_routine != NULL &&
        type->source_corresp.enclosing_routine->function_def_number !=
                                                    NULL_function_def_number) {
      /* If the type is defined in a function or block scope, search in
         that routine's scope (if it has been defined and the memory region
         is available). */
      scope= scope_for_routine_or_null(type->source_corresp.enclosing_routine);
    } else {
      /* Assume we should use the current function. */
      scope = innermost_function_scope;
    }  /* if */
    if (scope != NULL) {
      expr = find_local_expr_node_in_scope((char*)type, lerk_decltype, scope);
    }  /* if */
  }  /* if */
  return expr;
}  /* decltype_arg */

#if GNU_EXTENSIONS_ALLOWED && GNU_VECTOR_TYPES_ALLOWED

#if !BACK_END_IS_C_GEN_BE
static
#endif /* !BACK_END_IS_C_GEN_BE */
void form_vector_type_attribute(
                     a_type_ptr                            type,
                     a_boolean                             *need_leading_space,
                     an_il_to_str_output_control_block_ptr octl)
/*
Output a GNU vector type attribute as required by the specified type, which
must be a tk_vector, in the way described by octl.  If *need_leading_space is
TRUE, precede the attribute with a leading space.  *need_leading_space is set
to TRUE to indicate that a space will be needed after the attribute.
*/
{
  a_vector_kind  kind;
  check_assertion(type->kind == (a_type_kind)tk_vector);
  kind = type->variant.vector.kind;
  if (*need_leading_space) {
    octl->output_str(" ", octl);
  }  /* if */
  switch (kind) {
    case vk_gnu:
      octl->output_str("__attribute((vector_size(", octl);
      break;
    case vk_ext:
      octl->output_str("__attribute((ext_vector_type(", octl);
      break;
    case vk_neon:
      octl->output_str("__attribute((neon_vector_type(", octl);
      break;
    case vk_neon_poly:
      octl->output_str("__attribute((neon_polyvector_type(", octl);
      break;
    case vk_neon_builtin:
      octl->output_str("__attribute((neon_builtinvector_type(", octl);
      break;
    case vk_last:
      /* vk_last should never be used. */
      unexpected_condition();
    default_is_unexpected_str("form_vector_type_attribute: bad vector kind");
  }  /* switch */
  if (type->variant.vector.size_constant != NULL) {
    form_constant(type->variant.vector.size_constant,
                  /*need_parens=*/FALSE, octl);
  } else {
    a_host_large_unsigned  num;
    /* Output the type size for a GNU "vector_size" attribute, otherwise output
       the number of elements. */
    if (kind == vk_gnu) {
      num = type->size;
    } else {
      num = type->size/skip_typerefs(type->variant.vector.element_type)->size;
    }
    form_unsigned_num(num, octl);
  }  /* if */
  octl->output_str(")))", octl);
  *need_leading_space = TRUE;
}  /* form_vector_type_attribute */


Small_string<16> get_name_for_riscv_vector_type(a_const_char  *name_prefix,
                                                a_type_ptr    vector_type)
/*
Return the name of the given RISC-V vector type with the specified name prefix.
*/
{
  Small_string<16>  name(name_prefix);
  a_type_ptr        element_type;
  int               length_multiplier;

  check_assertion(vector_type->kind == tk_riscv_vector);
  element_type = vector_type->variant.riscv_vector.element_type;
  length_multiplier = vector_type->variant.riscv_vector.length_multiplier;
  if (is_bool_type(element_type)) {
    name.append("bool", length_multiplier, "_t");
  } else {
    unsigned tuple_elements = vector_type->variant.riscv_vector.tuple_elements;
    if (element_type->kind == tk_integer) {
      if (!is_signed_integral_type(element_type)) {
        name.append("u");
      }  /* if */
      name.append("int", 8*element_type->size);
    } else if (element_type->kind == tk_float) {
      if (element_type->variant.float_kind == fk_std_bfloat16) {
        name.append("bfloat16");
      } else {
        name.append("float", 8*element_type->size);
      }  /* if */
    } else if (type_is(element_type, tk_float8e4m3)) {
      name.append("float8e4m3");
    } else if (type_is(element_type, tk_float8e5m2)) {
      name.append("float8e5m2");
    } else {
      unexpected_condition_str("unexpected element type kind");
    }  /* if */
    if (length_multiplier > 0) {
      name.append("m", length_multiplier);
    } else {
      name.append("mf", -length_multiplier);
    }  /* if */
    if (tuple_elements != 1) {
      name.append("x", tuple_elements);
    }  /* if */
    name.append("_t");
  }  /* if */
  return name;
}  /* get_name_for_riscv_vector_type */

#endif /* GNU_EXTENSIONS_ALLOWED && GNU_VECTOR_TYPES_ALLOWED */

static void form_type_specifier(a_type_ptr                            type,
                                an_il_to_str_output_control_block_ptr octl)
/*
Output a string for a type specifier.  Do the output in the way described
by octl.
*/
{
  if (octl->for_diagnostics) {
    /* Ensure diagnostic output has access to the names of types. */
    type = skip_lexical_typerefs(type);
  }  /* if */
#if BACK_END_IS_CP_GEN_BE
  if (type->replace_by_generated_typedef) {
    /* Just put out a temporary name. */
    a_const_char *saved_name = type->source_corresp.name;
    type->source_corresp.name = NULL;
    form_name(&type->source_corresp, iek_type, octl);
    type->source_corresp.name = saved_name;
  } else
#endif /* BACK_END_IS_CP_GEN_BE */
  /* Do not insert code here. */
  switch (type->kind) {
    case tk_error:
      check_assertion(!octl->gen_compilable_code);
      octl->output_str(error_text(ec_error_type), octl);
      break;
    case tk_void:
      octl->output_str("void", octl);
      break;
    case tk_integer:
      /* Enum types are often handled specially. */
      if (type->variant.integer.enum_type &&
          /* Some enumeration types cannot be rendered in the C-generating
             back end.  Specifically:
               - don't generate enums when generating pcc mode
               - don't generate enums with an explicit underlying type
               - don't generate empty enums (valid in C++, but not in C)
             In these cases, the underlying integer type is used instead. */
          !(is_for_c_gen_be(octl) &&
            (octl->gen_pcc_code ||
             type->variant.integer.has_explicit_enum_base ||
             enum_constants(type) == NULL))) {
        /* Output a reference to the enum type. */
        form_tag_reference(type, octl);
      } else if (type->variant.integer.wchar_t_type &&
                 !is_for_c_gen_be(octl)) {
        /* Output a wchar_t type as "wchar_t", except in the C generating
           back end, where it is output as its underlying type. */
        if (ms_extensions && microsoft_version >= 1300) {
          /* In Microsoft mode, when microsoft_version is >= 1300 __wchar_t
             can be used as a keyword even when wchar_t is not recognized.
             We don't know how the type was originally specified, so output it
             as __wchar_t or wchar_t based on microsoft_version. */
          octl->output_str("__wchar_t", octl);
        } else {
          octl->output_str("wchar_t", octl);
        }  /* if */
      } else if (type->variant.integer.char8_t_type &&
                 !is_for_c_gen_be(octl)) {
        /* Output a char8_t type as "char8_t", except in the C-generating
           back end, where it is output as its underlying type. */
        octl->output_str("char8_t", octl);
      } else if (type->variant.integer.char16_t_type &&
                 !is_for_c_gen_be(octl)) {
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
        /* Output a char16_t type as "char16_t", except in the C generating
           back end, where it is output as its underlying type. */
        if (clang_is_generated_code_target && octl->gen_compilable_code) {
          /* Use __char16_t when targeting clang; it is available in all
             C++ modes. */
          octl->output_str("__char16_t", octl);
        } else
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
        /* Do not insert code here. */
        {
          octl->output_str("char16_t", octl);
        }  /* if */
      } else if (type->variant.integer.char32_t_type &&
                 !is_for_c_gen_be(octl)) {
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
        /* Output a char32_t type as "char32_t", except in the C generating
           back end, where it is output as its underlying type. */
        if (clang_is_generated_code_target && octl->gen_compilable_code) {
          /* Use __char32_t when targeting clang; it is available in all
             C++ modes. */
          octl->output_str("__char32_t", octl);
        } else
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
        /* Do not insert code here. */
        {
          octl->output_str("char32_t", octl);
        }  /* if */
      } else if (type->variant.integer.bool_type &&
                 (!is_for_c_gen_be(octl) || octl->render_c99_bool)) {
        /* Output a bool type as "bool", except in the C generating
           back end, where it is output as its underlying type. */
        octl->output_str((char *)(octl->render_c99_bool ? "_Bool" : "bool"),
                         octl);
      } else {
        /* Normal integer type. */
        if (type->variant.integer.explicitly_signed &&
            /* "signed" is not allowed when generating pcc code. */
            !octl->gen_pcc_code) {
          octl->output_str("signed ", octl);
        }  /* if */
        form_int_type_name(type, octl);
      }  /* if */
      break;
#if FIXED_POINT_ALLOWED
    case tk_fixed_point:
      {
        a_fixed_point_precision  prec = type->variant.fixed_point.precision;
        if (type->variant.fixed_point.saturating) {
          octl->output_str("_Sat ", octl);
        }  /* if */
        if (type->variant.fixed_point.is_unsigned) {
          octl->output_str("unsigned ", octl);
        }  /* if */
        if (prec == (a_fixed_point_precision)fpp_short) {
          octl->output_str("short ", octl);
        } else if (prec == (a_fixed_point_precision)fpp_long) {
          octl->output_str("long ", octl);
        } else {
          check_assertion(prec == (a_fixed_point_precision)fpp_default);
        }  /* if */
        if (type->variant.fixed_point.is_fract_type) {
          octl->output_str("_Fract", octl);
        } else {
          octl->output_str("_Accum", octl);
        }  /* if */
      }
      break;
#endif /* FIXED_POINT_ALLOWED */
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_complex:
      form_float_kind_name(type->variant.float_kind, octl);
      if (use_gnu_form()) {
        octl->output_str(" __complex__", octl);
      } else {
        /* Put out the C99 form. */
        octl->output_str(" _Complex", octl);
      }  /* if */
      break;
    case tk_imaginary:
      form_float_kind_name(type->variant.float_kind, octl);
      octl->output_str(" _Imaginary", octl);
      break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case tk_float:
      form_float_kind_name(type->variant.float_kind, octl);
      break;
    case tk_class:
    case tk_struct:
    case tk_union:
      form_tag_reference(type, octl);
      break;
    case tk_typeref:
      /* A typeref here should be a typedef or a typeref-based type
         operator. */
      if (is_type_operator_to_be_rendered(type, octl) &&
          octl->gen_compilable_code && octl->output_name != NULL &&
          typeref_is_type_operator(type, /*include_traits=*/TRUE)) {
        /* This is an actual type operator (as opposed to the type and
           class template deduction typerefs, which are handled below).
           The C++-generating back end is needed to handle name references
           in the operand. */
        octl->output_name((char*)type, iek_type);
      } else if (is_typeref_kind(type, trk_is_decltype)) {
        if (octl->gen_compilable_code && octl->output_name != NULL) {
          /* It may seem strange to use "output_name" to render a type that
             doesn't really have a name.  However, this uses the same
             mechanisms required to render non-autonomous unnamed tag types,
             and those use "output_name" for uniformity with named tag
             types. */
          octl->output_name((char*)type, iek_type);
        } else {
          an_expr_node_ptr expr = decltype_arg(type);
          char             *kwd;
          if (use_gnu_form() && octl->gen_compilable_code) {
            /* Current versions of g++ only accept the decltype keyword
               with -std=c++0x, but they accept __decltype in both modes,
               so use the safer spelling. */
            kwd = (char *)"__decltype(";
          } else {
            kwd = (char *)"decltype(";
          }  /* if */
          octl->output_str(kwd, octl);
          if (expr != NULL) {
            if (!type->variant.typeref.decltype_expr_not_parenthesized) {
              octl->output_str("(", octl);
            }  /* if */
            if (octl->output_expression != NULL) {
              /* Extra top-level parentheses should be suppressed since they
                 may change the meaning of the decltype construct. */
              octl->output_expression(expr, /*suppress_parens=*/TRUE);
            } else {
              form_expression(expr, octl);
            }  /* if */
            if (!type->variant.typeref.decltype_expr_not_parenthesized) {
              octl->output_str(")", octl);
            }  /* if */
          } else {
            /* No expression is available: Just emit a placeholder for the
               expression.  (This should only occur when not emitting
               compilable output.) */
            check_assertion(!octl->gen_compilable_code);
            octl->output_str("<expr>", octl);
          }  /* if */
          octl->output_str(")", octl);
        }  /* if */
      } else if (is_typeref_kind(type, trk_pack_index)) {
        /* C++26 type pack-index-specifier:
             type-name ... [ constant-expression ] */
        if (octl->gen_compilable_code && octl->output_name != NULL) {
          octl->output_name((char*)type, iek_type);
        } else {
          a_type_ptr        pack_type = type->variant.typeref.extra_info
                                                           ->operator_type_arg;
          an_expr_node_ptr  index_expr = decltype_arg(type);
          form_type(pack_type, octl);
          octl->output_str("...[", octl);
          if (index_expr != NULL) {
            if (octl->output_expression != NULL) {
              octl->output_expression(index_expr, /*suppress_parens=*/TRUE);
            } else {
              form_expression(index_expr, octl);
            }  /* if */
          } else {
            check_assertion(!octl->gen_compilable_code);
            octl->output_str("<expr>", octl);
          }  /* if */
          octl->output_str("]", octl);
        }  /* if */
      } else if (typeref_is_type_transforming_intrinsic(type) ||
                 is_typeref_kind(type, trk_bases) ||
                 is_typeref_kind(type, trk_direct_bases)) {
        if (octl->gen_compilable_code && octl->output_name != NULL) {
          /* It may seem strange to use "output_name" to render a type that
             doesn't really have a name.  However, this uses the same
             mechanisms required to render non-autonomous unnamed tag types,
             and those use "output_name" for uniformity with named tag
             types. */
          octl->output_name((char*)type, iek_type);
        } else {
          if (typeref_is_type_transforming_intrinsic(type)) {
            a_typeref_kind  kind = type->variant.typeref.kind;
            octl->output_str(type_transforming_intrinsic_name(kind), octl);
            octl->output_str("(", octl);
          } else {
            octl->output_str(type->variant.typeref.kind == trk_direct_bases
                                                   ? (char *)"__direct_bases("
                                                   : (char *)"__bases(",
                             octl);
          }  /* if */
          form_type(type->variant.typeref.extra_info->operator_type_arg, octl);
          octl->output_str(")", octl);
        }  /* if */
#if GNU_EXTENSIONS_ALLOWED
      } else if (is_typeref_kind(type, trk_is_typeof_with_expression) ||
                 is_typeref_kind(type, trk_is_typeof_with_type_operand)) {
        if (octl->gen_compilable_code && octl->output_name != NULL) {
          /* It may seem strange to use "output_name" to render a type that
             doesn't really have a name.  However, this uses the same
             mechanisms required to render non-autonomous unnamed tag types,
             and those use "output_name" for uniformity with named tag
             types. */
          octl->output_name((char*)type, iek_type);
        } else {
          octl->output_str("__typeof__(", octl);
          if (!is_typeref_kind(type, trk_is_typeof_with_type_operand)) {
            /* typeof(expression). */
            an_expr_node_ptr expr = decltype_arg(type);
            if (expr != NULL) {
              if (octl->output_expression != NULL) {
                /* Unlike decltype, typeof is not affected by extra top-level
                   parentheses.  Suppress them anyway to produce more pleasing
                   output. */
                octl->output_expression(expr, /*suppress_parens=*/TRUE);
              } else {
                form_expression(expr, octl);
              }  /* if */
            } else {
              /* No expression is available: Just emit a placeholder for the
                 expression.  (This should only occur when not emitting
                 compilable output.) */
              check_assertion(!octl->gen_compilable_code);
              octl->output_str("<expr>", octl);
            }  /* if */
          } else {
            /* typeof(type-name). */
            form_type(type->variant.typeref.type, octl);
          }  /* if */
          octl->output_str(")", octl);
        }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
      } else if (is_typeref_kind(type, trk_is_deduced_decltype_auto)) {
        an_expr_node_ptr  constraint = type->variant.typeref.extra_info->expr;
        if (constraint != NULL) {
          form_expression(constraint, octl);
          octl->output_str(" ", octl);
        }  /* if */
        octl->output_str("decltype(auto)", octl);
      } else if (is_typeref_kind(type, trk_is_deduced_auto)) {
        an_expr_node_ptr  constraint = type->variant.typeref.extra_info->expr;
        if (constraint != NULL) {
          form_expression(constraint, octl);
          octl->output_str(" ", octl);
        }  /* if */
        octl->output_str("auto", octl);
      } else if (is_typeref_kind(type, trk_is_deduced_class)) {
        a_type_ptr      instance = skip_lexical_typerefs(
                                                   type->variant.typeref.type);
        a_template_ptr  templ;
        check_assertion(instance != NULL && is_immediate_class_type(instance));
        templ = class_type_supp(instance)->assoc_template;
        check_assertion(templ != NULL);
        form_name(&templ->source_corresp, iek_template, octl);
      } else {
        check_assertion_str(
                         typeref_is_typedef(type) ||
                         type->variant.typeref.kind == trk_template_arg_list ||
                         type->variant.typeref.kind == trk_name_qualifier,
                         "form_type_specifier: unexpected typeref kind");
        form_name(&type->source_corresp, iek_type, octl);
      }  /* if */
      break;
    case tk_template_param:
      {
        if (is_auto_type(type) || type->variant.template_param.is_auto_param) {
          /* A type entry representing an "auto" or "decltype(auto)" type
             specifier. */
          /* If the placeholder is constrained, render that constraint. */
          an_expr_node_ptr
                    constraint = type->variant.template_param.extra_info
                                     ->constraint.type_constraint;
          if (constraint != NULL) {
            form_expression(constraint, octl);
            octl->output_str(" ", octl);
          }  /* if */
          if (type->variant.template_param.extra_info->coordinates.depth ==
                                                     AUTO_TYPE_NESTING_DEPTH &&
              type->variant.template_param.extra_info->coordinates.position ==
                                                    DECLTYPE_AUTO_POS_NUMBER) {
            octl->output_str("decltype(auto)", octl);
          } else {
            octl->output_str("auto", octl);
          }  /* if */
        } else if (tptk_is(type, tptk_bit_precise_int)) {
          if (type->variant.template_param.is_unsigned_bit_precise_int) {
            octl->output_str("unsigned ", octl);
          }  /* if */
          octl->output_str("_BitInt(", octl);
          form_constant(type->variant.template_param.extra_info
                                     ->constraint.bit_width_constant,
                        /*need_parens=*/FALSE, octl);
          octl->output_str(")", octl);
        } else {
          a_source_correspondence_ptr scp = &type->source_corresp;
          an_il_entry_kind            scp_kind = iek_type;
          /* See whether the template parameter name is remapped in the
             current context. */
          if (type_is_actual_template_parameter(type)) {
            a_source_correspondence_ptr new_scp;
            new_scp = source_corresp_for_template_param(
                       &type->variant.template_param.extra_info->coordinates);
            if (new_scp != NULL) {
              scp = new_scp;
              scp_kind = iek_template_parameter;
            }  /* if */
          }  /* if */
          if (scp->member_of_unknown_base &&
              !scp->qualified_unknown_base_member) {
            /* We're pretending that we found the member in a dependent
               base class and the original form of reference was
               unqualified. */
            form_unqualified_name(scp, scp_kind, octl);
          } else {
            form_name(scp, (an_il_entry_kind)scp_kind, octl);
          }  /* if */
#if DEBUG
          if (octl->debug_output) {
            if (type->variant.template_param.kind ==
                                     (a_template_param_type_kind)tptk_param) {
              Small_string<100> buf("#(",
                                    type->variant.template_param.extra_info
                                        ->coordinates.depth,
                                    ",",
                                    type->variant.template_param.extra_info
                                        ->coordinates.position,
                                    ")");

              octl->output_str(buf.as_temp_characters(), octl);
            }  /* if */
          }  /* if */
#endif /* DEBUG */
        }  /* if */
      }
      break;
#if GNU_VECTOR_TYPES_ALLOWED
    case tk_vector:
      /* GNU vector types are formed by associating the "vector_size" or
         "mode" attribute with the underlying element type.  If attributes
         are rendered from an_attribute entries, we only render the element
         type here.  Otherwise, we form the attribute also, unless it must
         be deferred to meet GCC requirements (which happens when vector_size
         occurs in a typedef definition).  It is also possible to have vector
         types rendered as "__edg_vector_type__(T, N)" by setting the global
         variable gen_edg_special_types to TRUE. */
      if (gen_edg_special_types) {
        a_type_ptr  etype = type->variant.vector.element_type;
        octl->output_str("__edg_vector_type__(", octl);
        form_type(etype, octl);
        octl->output_str(", ", octl);
        if (type->variant.vector.size_constant != NULL) {
          form_constant(type->variant.vector.size_constant,
                        /*need_parens=*/FALSE, octl);
        } else {
          a_targ_size_t  vlen = type->size/skip_typerefs(etype)->size;
          form_unsigned_num((a_host_large_unsigned)vlen, octl);
        }  /* if */
        octl->output_str(")", octl);
      } else {
        if (!octl->defer_vector_attribute && octl->output_attributes == NULL) {
          a_boolean need_leading_space = FALSE;
          form_vector_type_attribute(type, &need_leading_space, octl);
          octl->output_str(" ", octl);
        }  /* if */
        form_type(type->variant.vector.element_type, octl);
      }  /* if */
      break;
    case tk_scalable_vector:
      {
        a_type_ptr  etype = type->variant.scalable_vector.element_type;
        octl->output_str("__edg_scalable_vector_type__(", octl);
        form_type(etype, octl);
        octl->output_str(", ", octl);
        form_unsigned_num(type->variant.scalable_vector.tuple_elements, octl);
        octl->output_str(")", octl);
      }
      break;
    case tk_scalable_vector_count:
      octl->output_str("__SVCount_t", octl);
      break;
    case tk_riscv_vector:
      octl->output_str(get_name_for_riscv_vector_type(
                                          "__rvv_", type).as_temp_characters(),
                       octl);
      break;
    case tk_mfp8:
      octl->output_str("__mfp8", octl);
      break;
    case tk_float8e4m3:
      octl->output_str("__float8e4m3", octl);
      break;
    case tk_float8e5m2:
      octl->output_str("__float8e5m2", octl);
      break;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tk_pointer:
      /* C++/CLI interior_ptr<T> and pin_ptr<T> */
      if (type->variant.pointer.is_interior_ptr) {
        octl->output_str("interior_ptr<", octl);
      } else if (type->variant.pointer.is_pin_ptr) {
        octl->output_str("pin_ptr<", octl);
      } else {
        unexpected_condition();
      }  /* if */
      form_type(type->variant.pointer.type, octl);
      octl->output_str(">", octl);
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tk_nullptr:
      check_assertion(!is_for_c_gen_be(octl));
      /* If this is the standard nullptr type, put it out as the name of
         the standard C++ typedef or the C23 keyword; otherwise, use
         decltype. */
      if (is_standard_nullptr_type(type)) {
        if (!c23_mode) {
          octl->output_str("std::", octl);
        }  /* if */
        octl->output_str("nullptr_t", octl);
      } else {
        octl->output_str("decltype(nullptr)", octl);
      }  /* if */
      break;
    case tk_reflection:
      if (octl->gen_compilable_code) {
        octl->output_str("decltype(^::)", octl);
      } else {
        octl->output_str("std::meta::info", octl);
      }  /* if */
      break;
    case tk_unknown:
      check_assertion(!octl->gen_compilable_code);
      octl->output_str(error_text(ec_unknown_type), octl);
      break;
    default:
#if DEBUG
      if (octl->debug_output) {
        octl->output_str("**BAD-TYPE-KIND**", octl);
        break;
      }  /* if */
#endif /* DEBUG */
      unexpected_condition_str("form_type_specifier: bad type kind");
  }  /* switch */
}  /* form_type_specifier */


static inline a_boolean typedef_is_invisible(
                          a_type_ptr                            type,
                          a_type_ptr                            *resolved_type,
                          a_boolean                             suppress_const,
                          an_il_to_str_output_control_block_ptr octl)
/*
Return TRUE if the indicated typedef is "invisible" now because (a) it's
local to a function and we're suppressing local typedefs, or
(b) suppress_const is TRUE (we're suppressing top-level "const") and the
typedef contains a const qualifier, or (c) suppress_typedefs is TRUE, or
(d) a user visibility test routine (pointed to by the is_typedef_visible
control field) returns TRUE.
*/
{
  a_boolean result = FALSE;

  if (type->source_corresp.is_local_to_function &&
      octl->suppress_local_typedefs) {
    result = TRUE;
  } else if ((suppress_const) && is_const_qualified_type(type)) {
    result = TRUE;
  } else if (octl->suppress_typedefs) {
    result = TRUE;
  } else if (octl->is_typedef_invisible != NULL &&
             octl->is_typedef_invisible(type, resolved_type)) {
    result = TRUE;
  }  /* if */
  return result;
}  /* typedef_is_invisible */


static a_boolean can_use_qualified_array_typedef(
                          a_type_ptr                            *p_type,
                          a_type_qualifier_set                  *p_qualifiers,
                          a_boolean                             suppress_const,
                          an_il_to_str_output_control_block_ptr octl)
/*
If *p_type (an array type) was generated by adding a qualifier to a typedef
for an array type, and the qualified typedef can be used, return TRUE and
update *p_type and *p_qualifiers accordingly.  If suppress_const is TRUE,
we're supposed to suppress top-level "const".  octl is the output control
block, needed because it indicates whether local typedefs are invisible.
*/
{
  a_boolean            can_use_typedef = FALSE;
  a_type_ptr           type = *p_type, unqual_array_type;
  a_type_qualifier_set qualifiers;

  if (is_qualified_version_of_array_typedef(type, &unqual_array_type)) {
    /* This array type was generated by applying a type qualifier to
       a typedef of an array type. */
    if (typedef_is_invisible(unqual_array_type, NULL, suppress_const, octl)) {
      /* We can't use this typedef. */
    } else {
      /* We can use the array typedef. */
      /* The qualifiers to be added are those on the type we have here minus
         those that appeared on the element type of the array before
         qualification. */
      a_type_ptr           unqual_array_element_type =
                              underlying_array_element_type(unqual_array_type);
      a_type_qualifier_set unqual_array_qualifiers =
                      get_top_level_type_qualifiers(unqual_array_element_type);
      type = underlying_array_element_type(type);
      qualifiers = get_top_level_type_qualifiers(type);
      if (suppress_const) {
        qualifiers &= ~TQ_CONST;
      }  /* if */
      *p_qualifiers = qualifiers & ~unqual_array_qualifiers;
      *p_type = unqual_array_type;
      can_use_typedef = TRUE;
    }  /* if */
  }  /* if */
  return can_use_typedef;
}  /* can_use_qualified_array_typedef */


static inline a_boolean is_typedef_in_dealiasable_scope(a_type_ptr type)
/*
Return TRUE if the namespace of the given typedef-type is in a namespace
(including the global namespace) that is dealiasable; otherwise, return FALSE.
*/
{
  check_assertion(type_is_typedef(type));
  a_boolean result = TRUE;

  if (is_namespace_member(type)) {
    a_namespace_ptr nsp = parent_namespace_of(type);

    while (nsp->is_inline) {
      nsp = parent_namespace_of(nsp);
    }  /* while */
    if (symbol_for(nsp) == symbol_for_namespace_std) {
      result = FALSE;
    }  /* if */
  }  /* if */
  return result;
}  /* is_typedef_in_dealiasable_scope */


static inline a_boolean typedef_should_be_dealiased(
                                    a_type_ptr                            type,
                                    an_il_to_str_output_control_block_ptr octl)
/*
Return TRUE if the given type is a typedef that should be dealiased; otherwise,
return FALSE.

Note: Some additional legacy cases are handled through
is_member_typedef_that_should_be_ignored.
*/
{
  a_boolean result = TRUE;

  if (!octl->suppress_typedef_names) {
    result = FALSE;
  } else if (!type_is_typedef(type)) {
    result = FALSE;
  } else if (is_unknown_type(type)) {
    result = FALSE;
  } else if (is_error_type(type)) {
    result = FALSE;
  } else if (!is_typedef_in_dealiasable_scope(type)) {
    result = FALSE;
  }  /* if */
  return result;
}  /* typedef_should_be_dealiased */


static inline a_type_ptr skip_dealiasable_typedefs(
                                    a_type_ptr                            type,
                                    an_il_to_str_output_control_block_ptr octl)
/*
Given a type pointer, skip any top level dealiasable typedefs and return the
resulting type, except that if the output is for the C++-generating back end
and the type is a trk_template_arg_list or trk_name_qualifier typeref,
return the original type.
*/
{
  a_type_ptr result = type;

  if (result != NULL &&
      !(is_for_cp_gen_be(octl) && type_is(result, tk_typeref) &&
        (is_typeref_kind(result, trk_template_arg_list) ||
         is_typeref_kind(result, trk_name_qualifier)))) {
    result = skip_lexical_typerefs(result);
    while (typedef_should_be_dealiased(result, octl)) {
      result = skip_lexical_typerefs(result->variant.typeref.type);
    }  /* while */
  }  /* if */
  return result;
}  /* skip_dealiasable_typedefs */


static a_boolean is_member_typedef_that_should_be_ignored(
				a_type_ptr				type,
				an_il_to_str_output_control_block_ptr	octl)
/*
"type" is a typedef.  Return TRUE if the typedef is one that should be
replaced with the underlying type.  This is done for typedefs that are
members of template classes.
*/
{
  a_boolean	result = FALSE;

  if (!octl->keep_template_typedefs &&
      (!octl->suppress_typedef_names ||
       typedef_should_be_dealiased(type, octl))) {
    if (is_typeref_kind(type, trk_is_template_alias) &&
        !type->variant.typeref.is_dependent) {
      /* Drop the alias, unless the alias is dependent. */
      result = TRUE;
    } else if (type->variant.typeref.is_intrinsic_member) {
      /* The synthesized leaf for an intrinsically-resolved xyz<A...>::member
         names a member of a template class without instantiating it; like an
         ordinary member typedef of a template class, drop it in favor of the
         underlying type. */
      result = TRUE;
    } else if (type->source_corresp.is_class_member) {
      /* Drop the typedef if it was defined in a template class.  This is
         done even if the class was specialized. */
      a_type_ptr	parent_type = parent_class_of(type);
      if (parent_type->variant.class_struct_union.is_template_class) {
        result = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* is_member_typedef_that_should_be_ignored */


static a_boolean is_type_operator_to_be_rendered(
                                   a_type_ptr                            type,
                                   an_il_to_str_output_control_block_ptr octl)
/*
Return TRUE if the given typeref type is a construct that should be
rendered.  Otherwise, the underlying type should be rendered (e.g., in
diagnostics the underlying type is more helpful, and in the C-generating
back end type-operator constructs are either not available or not
portable).
*/
{
  a_boolean render = FALSE;

  if (is_for_cp_gen_be(octl) &&
      typeref_is_type_operator(type, /*include_intrinsics=*/TRUE)) {
    /* The C++-generating back end should include these operators in the
       generated code. */
    render = TRUE;
  } else if (typeref_is_type_operator(type, /*include_intrinsics=*/TRUE)) {
    an_expr_node_ptr expr = decltype_arg(type);
    if (is_for_c_gen_be(octl)) {
      /* Never render a type operator in the C-generating back end. */
      render = FALSE;
    } else if (typeref_is_type_transforming_intrinsic(type) ||
               is_typeref_kind(type, trk_pack_index) ||
               is_typeref_kind(type, trk_bases) ||
               is_typeref_kind(type, trk_direct_bases) ||
               (!is_typeref_kind(type, trk_is_decltype) && expr == NULL)) {
      /* A case that is always represented syntactically as an operator form
         here (e.g., __underlying_type, __bases, or type pack-index).  Render
         the operator in the C++-generating back end (to match the source
         form) or when the argument is template-dependent.  Otherwise, render
         the underlying type. */
      render = octl->gen_compilable_code ||
               type->variant.typeref.is_dependent_type_operator;
    } else {
      /* The type operator is based on an expression. */
      a_type_ptr underlying_type = type->variant.typeref.type;
      underlying_type = skip_typerefs(underlying_type);
      if (underlying_type->kind == (a_type_kind)tk_template_param &&
          underlying_type->variant.template_param.kind ==
                                    (a_template_param_type_kind)tptk_unknown) {
        /* With unknown template cases, you always need the underlying
           expression to make sense of things. */
        render = TRUE;
      } else if (octl->gen_compilable_code && expr != NULL &&
                 (octl->expr_is_unusable == NULL ||
                  !octl->expr_is_unusable(expr))) {
        /* We're generating compilable code, and the type operator is based on
           a usable expression.  Render it. */
        render = TRUE;
      }  /* if */
    }  /* if */
  } else if (!is_for_c_gen_be(octl) &&
             octl->render_auto_deduction_typerefs &&
             (is_typeref_kind(type, trk_is_deduced_decltype_auto) ||
              is_typeref_kind(type, trk_is_deduced_auto) ||
              is_typeref_kind(type, trk_is_deduced_class))) {
    /* "auto" and "decltype(auto)" should only appear in declarative contexts,
       and should be rendered there.  Similarly for deduced class templates. */
    render = TRUE;
  }  /* if */
  return render;
}  /* is_type_operator_to_be_rendered */

#if GNU_EXTENSIONS_ALLOWED && C99_IL_EXTENSIONS_SUPPORTED

a_type_ptr complex_type_needs_modification(a_type_ptr   orig_type,
                                           a_float_kind *orig_float_kind)
/*
Determine if the specified type refers to an 80- or 128-bit complex type
(either directly or through a series of typerefs, some of which may be
for_type_attributes).  If such a type is found, the underlying complex type
is changed by this routine to have its float_kind be "fk_float" and a pointer
to this type is returned as well as the original float_kind.  Otherwise NULL
is returned.

Internally, 80- and 128-bit complex numbers are represented by
__float80/__float128 types, and the il_to_str routines would normally generate
"__float128 _Complex" for such types, but gcc only accepts "float _Complex".
*/
{
  a_type_ptr type = orig_type;
  a_boolean  has_mode_attribute = FALSE;

  check_assertion(gnu_mode);
  while (type->kind == (a_type_kind)tk_typeref) {
    if (type->source_corresp.attributes != NULL &&
        find_attribute(ak_mode, type->source_corresp.attributes) != NULL) {
      has_mode_attribute = TRUE;
    }  /* if */
    type = type->variant.typeref.type;
  }  /* while */
  if (type->kind == (a_type_kind)tk_complex &&
      (type->variant.float_kind == (a_float_kind)fk_float80 ||
       type->variant.float_kind == (a_float_kind)fk_float128)) {
    if (has_mode_attribute ||
        (type->source_corresp.attributes != NULL &&
         find_attribute(ak_mode, type->source_corresp.attributes) != NULL)) {
      /* The type is a complex float80 or complex float128 type (or
         typedef for the same), but the existence of the "mode" attribute
         indicates that the source specified that type using the
         "mode(XC)" or "mode(TC)" attribute.  gcc rejects the resulting
         declaration because of the combination of "complex" and
         extended-precision floating type specifiers.  Change the
         floating point kind temporarily to fk_float to work around this
         problem. */
      *orig_float_kind = type->variant.float_kind;
      type->variant.float_kind = (a_float_kind)fk_float;
    } else {
      type = NULL;
    }  /* if */
  } else {
    type = NULL;
  }  /* if */
  return type;
}  /* complex_type_needs_modification */

#endif /* GNU_EXTENSIONS_ALLOWED && C99_IL_EXTENSIONS_SUPPORTED */

void form_type_first_part(
                    a_type_ptr                            type,
                    a_boolean                             under_lhs_declarator,
                    a_boolean                             need_trailing_space,
                    a_type_qualifier_set                  added_qualifiers,
                    a_form_type_options_set               options,
                    an_il_to_str_output_control_block_ptr octl)
/*
For the indicated type, output the specifiers and the part of the declarator
that precedes the name.  If under_lhs_declarator is TRUE, this type is
directly under a type that uses a left-side declarator, e.g., a pointer type.
(That's used to control use of parentheses around parts of the declarator.)
If need_trailing_space is TRUE, put a space at the end of the specifiers
part (needed if the declarator part is not empty, because it contains a
name or a derived type).  added_qualifiers contains a set of type qualifiers
to be added on top of the type.  options contains options as bits in a set:
If FTO_SUPPRESS_CONST is TRUE, suppress generation of top-level "const";
if FTO_SUPPRESS_SPECIFIERS is TRUE, suppress generation of the type specifiers
(put out only the declarator).  Do the output in the way described by octl.
*/
{
  a_type_kind kind;
  a_boolean   suppress_const = (options & FTO_SUPPRESS_CONST) != 0;
  a_type_qualifier_set
              qualifiers = TQ_NONE;
#if NEAR_AND_FAR_ALLOWED
  a_type_qualifier_set
              near_and_far_qualifiers;
  a_boolean   near_and_far_need_trailing_space = FALSE;
#endif /* NEAR_AND_FAR_ALLOWED */
  a_upc_block_size
              upc_block_size = UPC_BLOCK_SIZE_NONE;
  a_type_ptr  orig_type = type;
  a_type_ptr  attrib_stop_type = type;
  a_type_ptr  resolved_type = NULL;
#if GNU_EXTENSIONS_ALLOWED && C99_IL_EXTENSIONS_SUPPORTED
  a_type_ptr  complex_type = NULL;
#endif /* GNU_EXTENSIONS_ALLOWED && C99_IL_EXTENSIONS_SUPPORTED */

  if (type == NULL) {
    /* NULL type pointer. */
#if DEBUG
    if (octl->debug_output) {
      octl->output_str("**NULL-TYPE-POINTER**", octl);
      goto end_of_routine;
    }  /* if */
#endif /* DEBUG */
    check_assertion_str(!octl->gen_compilable_code,
                        "form_type_first_part: NULL type");
    octl->output_str(error_text(ec_something), octl);
    goto end_of_routine;
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED && C99_IL_EXTENSIONS_SUPPORTED
  /* An 80-/128-bit complex type needs to be modified before
     it is emitted.  The original float_kind will be restored later. */
  a_float_kind orig_float_kind;
  if (gnu_mode) {
    complex_type = complex_type_needs_modification(type, &orig_float_kind);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED && C99_IL_EXTENSIONS_SUPPORTED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (is_cli_generic_definition_argument_type(type) &&
      is_handle_type(type)) {
    /* For a generic definition argument, strip the handle type if one is
       present. */
    type = type->variant.pointer.type;
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  options &= ~FTO_SUPPRESS_CONST;
  /* Remove type qualifiers but not typedefs.  Also drop typedefs
     that aren't visible here.  Accumulate the type qualifier set. */
  while (type->kind == (a_type_kind)tk_typeref) {
    /* If resolved_type is non-NULL, the call to typedef_is_invisible in a
       previous iteration determined the non-typeref type that should
       ultimately be used, so we skip over any typedef and type operator
       typerefs and just accumulate qualifiers and attributes. */
    if (resolved_type == NULL && typeref_is_typedef(type)) {
      /* Typedef.  Stop unless it's invisible, or if it is a typedef that
         should be dropped in diagnostic output. */
      if (!typedef_is_invisible(type, &resolved_type, suppress_const, octl) &&
          !is_member_typedef_that_should_be_ignored(type, octl)) {
        break;
      }  /* if */
    } else if (resolved_type == NULL &&
               is_type_operator_to_be_rendered(type, octl)) {
      /* A type operator that should be rendered in its original form
         (instead of rendering the underlying type). */
      break;
    } else if (is_for_cp_gen_be(octl) &&
               (type->variant.typeref.kind == trk_template_arg_list ||
                type->variant.typeref.kind == trk_name_qualifier)) {
      /* A typeref that gives an alternative template argument list for a
         given occurrence of the type or that specifies the qualifiers used
         in the reference to the type.  Do not step over it. */
      break;
    } else {
      /* Type qualifier typeref.  Accumulate the qualifiers. */
      qualifiers |= type->variant.typeref.qualifiers;
      /* If we're supposed to suppress "const" and this typeref has it, we
         can take care of the suppression now.  This has to be done inside
         the loop because the "typedef_is_invisible" test uses the
         suppress_const flag. */
      if (suppress_const && (qualifiers & TQ_CONST)) {
        qualifiers &= ~TQ_CONST;
        suppress_const = FALSE;
      }  /* if */
#if UPC_EXTENSIONS_ALLOWED
      if (type->variant.typeref.qualifiers & TQ_UPC_SHARED) {
        upc_block_size = type->variant.typeref.extra_info->upc_block_size;
      }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
      if (is_typeref_kind(type, trk_for_type_attributes)) {
        /* The underlying type was modified with an attribute.  Record
           the target of the typeref as the end of the typeref chain for
           output_type_attributes. */
        attrib_stop_type = type->variant.typeref.type;
      }  /* if */
    }  /* if */
    type = type->variant.typeref.type;
  }  /* while */
  /* Add top-level qualifiers if told to. */
  qualifiers |= added_qualifiers;
#if NEAR_AND_FAR_ALLOWED
  /* Look for any qualifiers (like "near") that are displayed specially. */
  near_and_far_qualifiers = qualifiers & (TQ_NEAR | TQ_FAR);
  if (near_and_far_qualifiers != TQ_NONE) {
    qualifiers -= near_and_far_qualifiers;
    near_and_far_need_trailing_space = need_trailing_space;
    need_trailing_space = TRUE;
  }  /* if */
#endif /* NEAR_AND_FAR_ALLOWED */
  kind = type->kind;
#if BACK_END_IS_CP_GEN_BE
  if (type->replace_by_generated_typedef) {
    /* Ignore the actual type, just put out a name. */
    kind = (a_type_kind)tk_typeref;
  }  /* if */
#endif /* BACK_END_IS_CP_GEN_BE */
  if (kind == (a_type_kind)tk_pointer
#if MICROSOFT_EXTENSIONS_ALLOWED
      /* C++/CLI interior_ptr and pin_ptr are handled as specifier types. */
      && !type->variant.pointer.is_interior_ptr
      && !type->variant.pointer.is_pin_ptr
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                          ) {
    a_type_ptr pointee =
                   skip_dealiasable_typedefs(type->variant.pointer.type, octl);
    /* Pointer or reference type. */
    form_type_first_part(pointee,
                         /*under_lhs_declarator=*/TRUE,
                         /*need_trailing_space=*/TRUE,
                         TQ_NONE, options, octl);
    /* Output "*" or "&" for pointer or reference. */
    /* Or, "^" or "%" for C++/CLI handles and references. */
    if (type->variant.pointer.is_reference && !is_for_c_gen_be(octl)) {
      if (type->variant.pointer.is_rvalue_reference) {
        octl->output_str("&&", octl);
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (type->variant.pointer.is_handle) {
        octl->output_str("%", octl);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      } else {
        octl->output_str("&", octl);
      }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (type->variant.pointer.is_handle) {
      octl->output_str("^", octl);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else {
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (type->variant.pointer.base_variable != NULL) {
        /* This is a Microsoft based pointer -- add "__based(var-name) "
           before the asterisk. */
        octl->output_str("__based(", octl);
        form_name(&type->variant.pointer.base_variable->source_corresp,
                  iek_variable, octl);
        octl->output_str(") ", octl);
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      octl->output_str("*", octl);
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (type->has_microsoft_w64_specifier && !is_for_c_gen_be(octl)) {
        /* Do not propagate the "__w64" specifier to the C-generating back
           end. */
        octl->output_str("__w64", octl);
        if (need_trailing_space || qualifiers != TQ_NONE ||
            type->variant.pointer.modifiers != PM_NONE) {
          octl->output_str(" ", octl);
        }  /* if */
      }  /* if */
      if (type->variant.pointer.modifiers != PM_NONE) {
        form_pointer_modifiers(type->variant.pointer.modifiers, octl);
        if (need_trailing_space || qualifiers != TQ_NONE) {
          octl->output_str(" ", octl);
        }  /* if */
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    }  /* if */
    /* Output the type qualifiers on the pointer, if any. */
    if (qualifiers != TQ_NONE) {
      form_type_qualifier(qualifiers, upc_block_size, need_trailing_space,
                          octl);
    }  /* if */
    if (attrib_stop_type != orig_type) {
      if (octl->gen_compilable_code) {
        octl->output_str(" ", octl);
      }  /* if */
      output_type_attributes(orig_type, attrib_stop_type, octl);
    }  /* if */
  } else if (kind == (a_type_kind)tk_ptr_to_member) {
    /* Pointer-to-member type. */
    a_type_ptr mem_type = skip_dealiasable_typedefs(
                                              type->variant.ptr_to_member.type,
                                              octl);
    a_type_ptr class_type = is_for_cp_gen_be(octl)
                     ? type->variant.ptr_to_member.orig_class_of_which_a_member
                     : type->variant.ptr_to_member.class_of_which_a_member;
    form_type_first_part(mem_type,
                         /*under_lhs_declarator=*/TRUE,
                         /*need_trailing_space=*/TRUE,
                         TQ_NONE, options, octl);
    /* Output Classname::*. */
    if (octl->gen_compilable_code && mem_type->kind != tk_routine &&
        !octl->suppress_ptr_to_data_member_parens) {
      /* The class name might be put out as a qualified name with a leading
         "::", so the declarator must be enclosed in parentheses to prevent
         something like "T (::C::*p)" from being interpreted as "T::C::*p".
         If the type is a pointer to member function, the parentheses will
         be put out automatically because of the declarator operator
         precedence, but we need to supply the "(" here explicitly in the
         pointer to data member case.  The matching ")" will be put out in
         form_type_second_part. */
      octl->output_str("(", octl);
    }  /* if */
    form_class_qualifier(class_type, mem_type->kind != tk_routine, octl);
    /* form_class_qualifier put out "::".  Add the final "*" here.  That's
       okay; it's a separate token. */
    octl->output_str("*", octl);
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (type->variant.ptr_to_member.modifiers != PM_NONE) {
      form_pointer_modifiers(type->variant.ptr_to_member.modifiers, octl);
      if (need_trailing_space || qualifiers != TQ_NONE) {
        octl->output_str(" ", octl);
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* Output the type qualifiers on the pointer, if any. */
    if (qualifiers != TQ_NONE) {
      form_type_qualifier(qualifiers, upc_block_size, need_trailing_space,
                          octl);
    }  /* if */
    if (attrib_stop_type != orig_type) {
      if (octl->gen_compilable_code) {
        octl->output_str(" ", octl);
      }  /* if */
      output_type_attributes(orig_type, attrib_stop_type, octl);
    }  /* if */
  } else if (kind == (a_type_kind)tk_routine) {
    /* Function type. */
    a_boolean                  is_lambda = is_lambda_body_routine_type(type);
    a_boolean                  is_deduction_guide = FALSE;
    a_routine_type_supplement  *rtsp = type->variant.routine.extra_info;
    /* A qualifier on a function type shouldn't be possible in compilable
       code without a typedef; however, it shouldn't be a fatal error in
       diagnostic or debugging output. */
    check_assertion_str(qualifiers == TQ_NONE || !octl->gen_compilable_code,
                        "form_type_first_part: qualifier on function type");
    if (rtsp->assoc_routine != NULL &&
        special_kind_is(rtsp->assoc_routine, sfk_deduction_guide)) {
      is_deduction_guide = TRUE;
    }  /* if */
    if ((rtsp->trailing_return_type || is_lambda || is_deduction_guide) &&
        !is_for_c_gen_be(octl)) {
      /* For a routine type specified with a trailing return type, the 
         type specifiers are simply "auto", except for lambda expressions
         where the specifiers are omitted altogether.  (The C-generating back
         end does not attempt to render routine types with trailing return
         types, since those are a C++ feature.)   Deduction guides have
         trailing return types, but don't use the "auto" keyword. */
      if (!is_lambda && !is_deduction_guide &&
          !(options & FTO_SUPPRESS_SPECIFIERS)) {
        octl->output_str("auto ", octl);
      }  /* if */
    } else {
      a_type_ptr return_type = skip_dealiasable_typedefs(
                                             type->variant.routine.return_type,
                                             octl);
      form_type_first_part(return_type,
                           /*under_lhs_declarator=*/FALSE,
                           /*need_trailing_space=*/TRUE,
                           TQ_NONE, options, octl);
    }  /* if */
    /* This is a right-side declarator, so if it's under a left-side
       declarator parentheses are needed. */
    if (under_lhs_declarator) {
      octl->output_str("(", octl);
      output_predeclarator_attributes(type, octl);
    }  /* if */
    if (qualifiers != TQ_NONE) {
      /* As noted above, qualifiers cannot appear in compilable code, only
         in diagnostic and debugging output. */
      form_type_qualifier(qualifiers, upc_block_size, need_trailing_space,
                          octl);
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* A calling convention specifier is put out as a left-hand-side
       declarator. */
    if (rtsp->explicit_calling_convention) {
      form_calling_convention(rtsp->calling_convention, octl);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else if (kind == (a_type_kind)tk_array) {
    /* Array type. */
    a_type_ptr elem_type = skip_dealiasable_typedefs(
                                              type->variant.array.element_type,
                                              octl);
    /* A qualifier on an array type shouldn't be possible, period. */
    check_assertion_str(qualifiers == TQ_NONE,
                        "form_type_first_part: qualifier on array type");
    if (can_use_qualified_array_typedef(&type, &qualifiers, suppress_const,
                                        octl)) {
      /* This array type was generated by applying a type qualifier to
         a typedef of an array type.  Use the typedef. */
      goto handle_specifiers_type;
    }  /* if */
    if (suppress_const) options |= FTO_SUPPRESS_CONST;
    form_type_first_part(elem_type,
                         /*under_lhs_declarator=*/FALSE,
                         /*need_trailing_space=*/TRUE,
                         TQ_NONE,
                         options,
                         octl);
    /* This is a right-side declarator, so if it's under a left-side
       declarator parentheses are needed. */
    if (under_lhs_declarator) {
      octl->output_str("(", octl);
      output_predeclarator_attributes(type, octl);
    }  /* if */
  } else {
handle_specifiers_type:
    /* No declarator part to process.  Handle the specifier type. */
    if ((options & FTO_SUPPRESS_SPECIFIERS) == 0) {
      a_boolean  c11_atomic = FALSE;
      if (qualifiers != TQ_NONE) {
        if (qualifiers & TQ_C11_ATOMIC) {
          /* We should be able to emit _Atomic like other type qualifiers, but
             early Clang versions only accepted the "_Atomic(T)" form; not
             "_Atomic T".  We therefore handle TQ_C11_ATOMIC separately. */
          c11_atomic = TRUE;
          qualifiers &= ~TQ_C11_ATOMIC;
        }  /* if */
        form_type_qualifier(qualifiers, upc_block_size,
                            /*need_trailing_space=*/TRUE, octl);
        if (c11_atomic) {
          octl->output_str("_Atomic(", octl);
        }  /* if */
      }  /* if */
      form_type_specifier(type, octl);
      if (c11_atomic) {
        octl->output_str(")", octl);
      }  /* if */
      if (attrib_stop_type != orig_type) {
        if (octl->gen_compilable_code) {
          octl->output_str(" ", octl);
        }  /* if */
        output_type_attributes(orig_type, attrib_stop_type, octl);
      }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (type->has_microsoft_w64_specifier && !is_for_c_gen_be(octl)) {
        /* Do not propagate the "__w64" specifier to the C-generating back
           end. */
        octl->output_str(" __w64", octl);
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      if (has_predeclarator_attribute(orig_type)) {
        octl->output_str(" (", octl);
        output_predeclarator_attributes(orig_type, octl);
      }  /* if */
      /* Put out a trailing space if required. */
      if (need_trailing_space) octl->output_str(" ", octl);
    }  /* if */
  }  /* if */
#if NEAR_AND_FAR_ALLOWED
  if (near_and_far_qualifiers != TQ_NONE) {
    /* "near" or "far": display it next to the declarator name. */
    form_type_qualifier(near_and_far_qualifiers, upc_block_size,
                        near_and_far_need_trailing_space, octl);
  }  /* if */
#endif /* NEAR_AND_FAR_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED && C99_IL_EXTENSIONS_SUPPORTED
  if (complex_type != NULL) {
    complex_type->variant.float_kind = orig_float_kind;
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED && C99_IL_EXTENSIONS_SUPPORTED */
end_of_routine:;
}  /* form_type_first_part */


static
void form_exception_specification(a_type_ptr                            type,
                                  an_il_to_str_output_control_block_ptr octl)
/*
Output the exception specification (as can appear after a function declarator),
if any, recorded in the given routine type in the way described by octl.
*/
{
  an_exception_specification_ptr  esp;

  type = skip_typerefs(type);
  esp = type->variant.routine.extra_info->exception_specification;
  if (esp == NULL || esp->throw_any || esp->indeterminate) {
    /* Nothing to output. */
  } else if (esp->is_noexcept) {
    octl->output_str(" noexcept", octl);
    while (esp->copy_from_prototype) {
      an_exception_specification_ptr proto_esp =
                           skip_typerefs(esp->variant.routine->type)->
                           variant.routine.extra_info->exception_specification;
      if (proto_esp == esp) {
        /* This can occur in error cases; avoid an endless loop. */
        break;
      }  /* if */
      esp = proto_esp;
    }  /* while */
    if (esp->arg_cached || esp->copy_from_prototype) {
      /* We don't have a usable constant for the operand. */
      check_assertion(!octl->gen_compilable_code);
      octl->output_str("(<expr>)", octl);
    } else if (esp->variant.noexcept_arg != NULL) {
      octl->output_str("(", octl);
      form_constant(esp->variant.noexcept_arg, /*need_parens=*/FALSE, octl);
      octl->output_str(")", octl);
    }  /* if */
  } else {
    an_exception_specification_type_ptr  estp;
    estp = esp->variant.exception_specification_type_list;
    octl->output_str(" throw(", octl);
    for (; estp != NULL; estp = estp->next) {
      form_type(estp->type, octl);
      if (estp->next != NULL) {
        octl->output_str(", ", octl);
      }  /* if */
    }  /* for */
    octl->output_str(")", octl);
  }  /* if */
}  /* form_exception_specification */


void form_function_declarator(a_type_ptr                            type,
                              an_il_to_str_output_control_block_ptr octl)
/*
Output a function declarator for the indicated routine type.  Do the output
in the way described by octl.
*/
{
  a_routine_type_supplement_ptr rtsp = type->variant.routine.extra_info;
  a_param_type_ptr              param;

  /* See if there's a special routine to output function declarators.
     If so, use it. */
  if (octl->output_func_declarator != NULL) {
    octl->output_func_declarator(type);
  } else {
    a_boolean            is_lambda = is_lambda_body_routine_type(type);
    a_type_qualifier_set qualifiers =
            rtsp->this_class != NULL ? rtsp->qualifiers | rtsp->this_qualifiers
                                     : TQ_NONE;
    /* Default processing. */
    octl->output_str("(", octl);
    if ((!rtsp->prototyped || rtsp->old_style_params_scanned) &&
        (il_header.source_language != sl_Cplusplus ||
         octl->gen_compilable_code)) {
      /* Unprototyped function.  Put out nothing between the parentheses. */
      /* Note that in C++ the parameter types for old-style functions are
         listed when generating human-readable output. */
    } else {
      /* Prototyped list. */
      param = rtsp->param_type_list;
      if (param == NULL) {
        if (!rtsp->has_ellipsis) {
          /* The first argument is NULL, so this is a "void" parameter
             list.  Write it as void in C or when emulating the Microsoft
             __FUNCSIG__ format and as empty in C++. */
          if (il_header.source_language == sl_C ||
              octl->use_microsoft_format) {
            octl->output_str("void", octl);
          }  /* if */
        } else {
          /* This is a parameter list consisting of only an ellipsis, which
             is standard in C++ and C23 and may be accepted as an extension
             in C earlier C modes. */
          octl->output_str("...", octl);
        }  /* if */
      } else {
        /* List the parameter types. */
        for (; param != NULL; param = param->next) {
          if (octl->suppress_compiler_generated_parameters &&
              param->param_num == 0) {
            /* This parameter was added by lowering and thus should not be
               put out. */
          } else {
#if MICROSOFT_EXTENSIONS_ALLOWED
            if (param->is_cli_param_array) octl->output_str("... ", octl);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
            form_type(param->type, octl);
            if (param->is_parameter_pack) octl->output_str("...", octl);
            /* Default argument expressions are not put out. */
            if (param->next != NULL &&
                !(octl->suppress_compiler_generated_parameters &&
                  param->next->param_num == 0)) {
              /* There is another unsuppressed parameter, so output a
                 separator. */
              octl->output_str(", ", octl);
            }  /* if */
          }  /* if */
        }  /* for */
        /* Put out the ellipsis if there is one. */
        if (rtsp->has_ellipsis) octl->output_str(", ...", octl);
      }  /* if */
    }  /* if */
    octl->output_str(")", octl);
    /* If the function type has a linkage that's not compatible with the
       default, add the linkage string after the closing parenthesis.  This
       is done only for non-compilable code. */
    if (!octl->gen_compilable_code) {
      a_name_linkage_kind linkage =
                    enum_cast<a_name_linkage_kind>(rtsp->routine_name_linkage);
      if (linkage != (a_name_linkage_kind)nlk_internal &&
          linkage != (a_name_linkage_kind)nlk_none &&
          !routine_linkages_are_compatible(linkage,
                                           default_routine_name_linkage,
                                           /*is_impl_conv=*/FALSE)) {
        octl->output_str(" ", octl);
        octl->output_str(name_linkage_kind_names[linkage], octl);
      }  /* if */
    }  /* if */
    /* Output a cv-qualifier for a member function, if there is one.  For a
       lambda body, however, output "static" if the call operator is a static
       member, or "mutable" if it is a nonstatic member that is not const. */
    if (is_lambda) {
      if (rtsp->this_class == NULL) {
        octl->output_str(" static", octl);
      } else if ((qualifiers & TQ_CONST) == 0) {
        octl->output_str(" mutable", octl);
      } else {
        check_assertion(qualifiers == TQ_CONST);
      }  /* if */
    } else if (qualifiers != TQ_NONE) {
      octl->output_str(" ", octl);
      form_type_qualifier(qualifiers, UPC_BLOCK_SIZE_NONE,
                         /*need_trailing_space=*/FALSE, octl);
    }  /* if */
    if (rtsp->ref_qualifiers == (a_ref_qualifier_kind)rqk_lvalue) {
      octl->output_str(" &", octl);
    } else if (rtsp->ref_qualifiers == (a_ref_qualifier_kind)rqk_rvalue) {
      octl->output_str(" &&", octl);
    }  /* if */
    if ((rtsp->trailing_return_type || is_lambda) && !is_for_c_gen_be(octl)) {
      octl->output_str("->", octl);
      form_type(type->variant.routine.return_type, octl);
    }  /* if */
    if (!octl->gen_compilable_code && exc_spec_in_func_type) {
      form_exception_specification(type, octl);
    }  /* if */
  }  /* if */
}  /* form_function_declarator */


static void form_array_declarator(a_type_ptr                            type,
                                  an_il_to_str_output_control_block_ptr octl)
/*
Output an array declarator for the indicated array type.  Do the output in
the way described by octl.
*/
{
  octl->output_str("[", octl);
  form_type_qualifier(type->variant.array.qualifiers, UPC_BLOCK_SIZE_NONE,
                      /*need_trailing_space=*/TRUE, octl);
#if !SUPPRESS_ARRAY_STATIC_IN_GENERATED_CODE
  if (type->variant.array.is_static) {
    /* C99 static. */
    octl->output_str("static ", octl);
  }  /* if */
#endif /* !SUPPRESS_ARRAY_STATIC_IN_GENERATED_CODE */
  if (type->variant.array.is_vla) {
    /* Variable-length array. */
    if (!type->variant.array.has_assoc_vla_dimension ||
        octl->gen_vla_array_as_asterisk_bound_array) {
      /* Array[*] case. */
      octl->output_str("*", octl);
    } else if (innermost_function_scope == NULL) {
      /* find_vla_dimension requires that innermost function scope be set.
         Since this is not the case, we just emit a placeholder.  This should
         only happen when called from the stand-alone IL display code. */
      check_assertion(!octl->gen_compilable_code);
      octl->output_str("<expr>", octl);
    } else {
      /* Variable-length array with an associated expression. */
      a_vla_dimension_ptr vlap = find_vla_dimension(type);
#if DO_IL_LOWERING && !LOWER_VARIABLE_LENGTH_ARRAYS
      if (octl->gen_compilable_code && vlap->dimension_variable != NULL) {
        /* The expression was fixed in a separate variable.  Use that instead
           of the expression (to avoid potential duplicate side-effects). */
        form_name(&vlap->dimension_variable->source_corresp, iek_variable,
                  octl);
      } else
#endif /* DO_IL_LOWERING && !LOWER_VARIABLE_LENGTH_ARRAYS */
      /* Do not insert code here. */
      {
        form_expression(vlap->dimension_expr, octl);
      }  /* if */
    }  /* if */      
  } else if (type->variant.array.is_variable_size_array) {
    an_expr_node_ptr count = type->variant.array.variant.element_count_expr;
    form_expression(count, octl);
#if BACK_END_IS_CP_GEN_BE && BACK_END_SHOULD_BE_CALLED
    /* Do not add code here. */
  } else if (type->variant.array.constant_bound_expr_in_local_expr_node_ref &&
             innermost_function_scope != NULL &&
             is_for_cp_gen_be(octl)) {
    /* The bound expression has a reference to a local variable and is
       consequently represented by an a_local_expr_node_ref entry. */
    an_expr_node_ptr expr = find_local_expr_node(
                                 (char *)type,
                                 (a_local_expr_node_ref_kind)lerk_array_bound);
    check_assertion(expr != NULL);
    form_expression(expr, octl);
  } else if (type->variant.array.bound_constant != NULL &&
             !type->variant.array.is_template_dependent_size_array &&
             is_for_cp_gen_be(octl) &&
             octl->output_expression != NULL) {
    /* Use the recorded a_constant entry rather than a plain integer.  This
       allows the output to be closer to the original bound expression when
       the bound is more than just a literal (e.g., "2*2" instead of "4"). */
    a_constant_ptr con = type->variant.array.bound_constant;
    a_boolean      need_parens = FALSE;

    if (con->expr != NULL) {
      /* It is an error if a comma appears outside of parentheses in a
         bound expression. */
      need_parens = expr_has_comma_operation(con->expr);
    }  /* if */
    form_constant(con, need_parens, octl);
#endif /* BACK_END_IS_CP_GEN_BE && BACK_END_SHOULD_BE_CALLED */
    /* Do not add code here. */
  } else if (type->variant.array.is_template_dependent_size_array) {
    a_constant_ptr constant =
                            type->variant.array.variant.element_count_constant;
    if (constant != NULL) {
      an_expr_node_ptr *expr_ptr = NULL;
      if (type->variant.array.dep_constant_bound_expr_in_local_expr_node_ref &&
          innermost_function_scope != NULL) {
        /* The expression associated with the element count constant referred
           to local variables and thus could not be copied into the file
           scope.  Retrieve it and temporarily restore it to the constant so
           we can print it. */
        a_template_param_constant_kind tkind;
        check_assertion(constant->kind ==
                                      (a_constant_repr_kind)ck_template_param);
        tkind = constant->variant.template_param.kind;
        if (tkind == tpck_expression) {
          expr_ptr = &constant->variant.template_param.variant.expr;
        } else if (tkind == tpck_sizeof ||
                   tkind == tpck_datasizeof ||
                   tkind == tpck_alignof ||
                   tkind == tpck_uuidof ||
                   tkind == tpck_typeid ||
                   tkind == tpck_noexcept) {
          expr_ptr = &constant
                           ->variant.template_param.variant.templ_sizeof.expr;
        }  /* if */
        check_assertion(expr_ptr != NULL && *expr_ptr == NULL);
        *expr_ptr = find_local_expr_node(
                             (char *)type,
                             (a_local_expr_node_ref_kind)lerk_dep_array_bound);
        check_assertion(*expr_ptr != NULL);
      }  /* if */
      form_constant(constant, /*need_parens=*/FALSE, octl);
      if (expr_ptr != NULL) {
        *expr_ptr = NULL;
      }  /* if */
    }  /* if */
  } else if (type->variant.array.variant.number_of_elements == 0 &&
             !type->variant.array.bound_is_zero) {
    /* For unknown-bound arrays, put nothing between the []. */
  } else {
    form_unsigned_num((a_host_large_unsigned)type->
                                     variant.array.variant.number_of_elements,
                      octl);
#if UPC_EXTENSIONS_ALLOWED
    if (type->variant.array.is_threads_dimension) {
      octl->output_str("*THREADS", octl);
    }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
  }  /* if */
  octl->output_str("]", octl);
}  /* form_array_declarator */


void form_type_second_part(
                    a_type_ptr                            type,
                    a_boolean                             under_lhs_declarator,
                    a_form_type_options_set               options,
                    an_il_to_str_output_control_block_ptr octl)
/*
Output the second part of a type reference, the part of the declarator
that follows the name.  If under_lhs_declarator is TRUE, this type is
directly under a type that uses a left-side declarator, e.g., a pointer type.
(That's used to control use of parentheses around parts of the declarator.)
If options contains FTO_SUPPRESS_CONST, suppress generation of top-level
"const".  Do the output in the way described by octl.
*/
{
  a_type_kind kind;
  a_boolean   suppress_const = (options & FTO_SUPPRESS_CONST) != 0;
  a_type_qualifier_set
              qualifiers = TQ_NONE;
  a_type_ptr  orig_type = type;
  a_type_ptr  attrib_stop_type = type;
  a_type_ptr  resolved_type = NULL;

  if (type == NULL
#if BACK_END_IS_CP_GEN_BE
      || type->replace_by_generated_typedef
#endif /* BACK_END_IS_CP_GEN_BE */
                                           ) {
    /* NULL type pointer or generated typedef.  Handled in
       form_type_first_part. */
    goto end_of_routine;
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (is_cli_generic_definition_argument_type(type) &&
      is_handle_type(type)) {
    /* For a generic definition argument, strip the handle type if one is
       present. */
    type = type->variant.pointer.type;
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  options &= ~FTO_SUPPRESS_CONST;
  /* Remove type qualifiers but not typedefs.  Also drop typedefs that aren't
     visible here.  Type operators that are rendered are treated like visible
     typedefs.  Accumulate the type qualifier set. */
  while (type->kind == (a_type_kind)tk_typeref) {
    /* If resolved_type is non-NULL, the call to typedef_is_invisible in a
       previous iteration determined the non-typeref type that should
       ultimately be used, so we skip over any typedef and type operator
       typerefs and just accumulate qualifiers and attributes. */
    if (resolved_type == NULL && typeref_is_typedef(type)) {
      /* Typedef.  Stop unless it's invisible, or if it is a typedef that
         should be dropped in diagnostic output. */
      if (!typedef_is_invisible(type, &resolved_type, suppress_const, octl) &&
          !is_member_typedef_that_should_be_ignored(type, octl)) {
        break;
      }  /* if */
    } else if (resolved_type == NULL &&
               is_type_operator_to_be_rendered(type, octl)) {
      /* A type operator that should be rendered in its original form
         (instead of rendering the underlying type). */
      break;
    } else if (resolved_type == NULL && is_for_cp_gen_be(octl) &&
               (type->variant.typeref.kind == trk_template_arg_list ||
                type->variant.typeref.kind == trk_name_qualifier)) {
      /* Do not scan past alternative template arguments or a name
         qualifier. */
      break;
    } else {
      /* Type qualifier typeref.  Accumulate the qualifiers. */
      qualifiers |= type->variant.typeref.qualifiers;
      /* If we're supposed to suppress "const" and this typeref has it, we
         can take care of the suppression now.  This has to be done inside
         the loop because the "typedef_is_invisible" test uses the
         suppress_const flag. */
      if (suppress_const && (qualifiers & TQ_CONST)) {
        qualifiers &= ~TQ_CONST;
        suppress_const = FALSE;
      }  /* if */
      if (is_typeref_kind(type, trk_for_type_attributes)) {
        /* The underlying type was modified with an attribute.  Record
           the target of the typeref as the end of the typeref chain for
           output_type_attributes. */
        attrib_stop_type = type->variant.typeref.type;
      }  /* if */
    }  /* if */
    type = type->variant.typeref.type;
  }  /* while */
  kind = type->kind;
  if (kind == (a_type_kind)tk_pointer
#if MICROSOFT_EXTENSIONS_ALLOWED
      /* C++/CLI interior_ptr and pin_ptr are handled as specifier types. */
      && !type->variant.pointer.is_interior_ptr
      && !type->variant.pointer.is_pin_ptr
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                          ) {
    /* Pointer or reference type. */
    a_type_ptr pointee =
                   skip_dealiasable_typedefs(type->variant.pointer.type, octl);
    form_type_second_part(pointee,
                          /*under_lhs_declarator=*/TRUE,
                          options, octl);
  } else if (kind == (a_type_kind)tk_ptr_to_member) {
    /* Pointer-to-member type. */
    a_type_ptr mem_type = skip_dealiasable_typedefs(
                                              type->variant.ptr_to_member.type,
                                              octl);
    if (octl->gen_compilable_code && mem_type->kind != tk_routine &&
        !octl->suppress_ptr_to_data_member_parens) {
      /* When we put out the first part of the type, we added a "(" to
         separate the member type from a possible leading global qualifier
         "::" on the class type.  We need to put out the matching ")"
         here. */
      octl->output_str(")", octl);
    }  /* if */
    form_type_second_part(mem_type,
                          /*under_lhs_declarator=*/TRUE,
                          options, octl);
  } else if (kind == (a_type_kind)tk_routine) {
    /* Function type. */
    /* This is a right-side declarator, so if it's under a left-side
       declarator parentheses are needed. */
    if (under_lhs_declarator) octl->output_str(")", octl);
    form_function_declarator(type, octl);
    if (attrib_stop_type != orig_type) {
      output_type_attributes(orig_type, attrib_stop_type, octl);
    }  /* if */
    if ((type->variant.routine.extra_info->trailing_return_type ||
         is_lambda_body_routine_type(type)) && !is_for_c_gen_be(octl)) {
      /* Suppress the normal return type for trailing return types.  (The C-
         generating back end does not attempt to render routine types with
         trailing return types, since those are a C++ feature.) */
    } else {
      a_type_ptr return_type = skip_dealiasable_typedefs(
                                             type->variant.routine.return_type,
                                             octl);

      form_type_second_part(return_type,
                            /*under_lhs_declarator=*/FALSE,
                            options, octl);
    }  /* if */
  } else if (kind == (a_type_kind)tk_array) {
    /* Array type. */
    if (can_use_qualified_array_typedef(&type, &qualifiers, suppress_const,
                                        octl)) {
      /* This array type was generated by applying a type qualifier to
         a typedef of an array type.  The type was handled as a specifiers
         type. */
    } else {
      a_type_ptr elem_type = skip_dealiasable_typedefs(
                                              type->variant.array.element_type,
                                              octl);

      /* This is a right-side declarator, so if it's under a left-side
         declarator parentheses are needed. */
      if (under_lhs_declarator) octl->output_str(")", octl);
      form_array_declarator(type, octl);
      if (attrib_stop_type != orig_type) {
        output_type_attributes(orig_type, attrib_stop_type, octl);
      }  /* if */
      if (suppress_const) options |= FTO_SUPPRESS_CONST;
      form_type_second_part(elem_type,
                            /*under_lhs_declarator=*/FALSE,
                            options, octl);
    }  /* if */
  } else {
    if (has_predeclarator_attribute(orig_type)) {
      /* form_type_first_part opened a parenthesis to render predeclarator
         attributes.  Close that parenthesis now. */
      octl->output_str(")", octl);
    }  /* if */
  }  /* if */
end_of_routine:;
}  /* form_type_second_part */


void form_type(a_type_ptr                            type,
               an_il_to_str_output_control_block_ptr octl)
/*
Output a string for a type.  Do the output in the way described by octl.
*/
{
  type = skip_dealiasable_typedefs(type, octl);
  if (type == NULL) {
    check_assertion(!octl->gen_compilable_code);
    octl->output_str(error_text(ec_null_type), octl);
  } else {
    a_boolean saved_type_context = octl->type_context;
    octl->type_context = TRUE;
    /* Write the specifiers and the first part of the declarator. */
    form_type_first_part_simple(type, /*under_lhs_declarator=*/FALSE,
                                /*need_trailing_space=*/FALSE, octl);
    /* Write the second part of the declarator. */
    form_type_second_part_simple(type, /*under_lhs_declarator=*/FALSE, octl);
    octl->type_context = saved_type_context;
  }  /* if */
}  /* form_type */


static void form_cast(a_type_ptr                            type,
                      an_il_to_str_output_control_block_ptr octl)
/*
Output a cast to the indicated type.  Do the output in the way described
by octl.
*/
{
  octl->output_str("(", octl);
  form_type(type, octl);
  octl->output_str(")", octl);
}  /* form_cast */


static void form_general_cast(
                     a_type_ptr                            type,
                     a_boolean                             is_reinterpret_cast,
                     an_il_to_str_output_control_block_ptr octl)
/*
Output a cast to the indicated type.  If is_reinterpret_cast is TRUE,
output a reinterpret_cast (note that a closing parenthesis will have to
be output later).  Do the output in the way described by octl.
*/
{
  if (is_reinterpret_cast) {
    octl->output_str("reinterpret_cast<", octl);
    form_type(type, octl);
    octl->output_str(">(", octl);
  } else {
    form_cast(type, octl);
  }  /* if */
}  /* form_general_cast */


static void output_optional_open_paren(
                       a_boolean                             *need_parens,
                       a_boolean                             *need_close_paren,
                       an_il_to_str_output_control_block_ptr octl)
/*
Output an opening parenthesis and set *need_close_paren to indicate that
the closing parenthesis is needed later.  However, if *need_parens is
FALSE, the parenthesis can be optimized away: don't generate it,
leave *need_close_paren set to FALSE, and set *need_parens to TRUE to
prevent doing the optimization more than once.
*/
{
  if (*need_parens) {
    octl->output_str("(", octl);
    *need_close_paren = TRUE;
  } else {
    /* Suppress the parenthesis. */
    *need_parens = TRUE;
  }  /* if */
}  /* output_optional_open_paren */


static void output_optional_close_paren(
                        a_boolean                             need_close_paren,
                        an_il_to_str_output_control_block_ptr octl)
/*
Output the closing parenthesis that is part of an optional set of
parentheses begun by output_optional_open_paren.  The parenthesis is
output only if need_close_paren is TRUE.
*/
{
  if (need_close_paren) octl->output_str(")", octl);
}  /* output_optional_close_paren */

#if GNU_EXTENSIONS_ALLOWED

static void form_label_difference_constant(
                            a_constant_ptr                         constant,
                            a_boolean                              need_parens,
                            an_il_to_str_output_control_block_ptr  octl)
/*
Output the given constant, which represents a difference between two label
addresses (possible in GNU mode).  The output has the form "&&x - &&y"
(enclosed in parentheses if need_parens is TRUE) where x and y are label
names.  Do the output in the way described by octl.
*/
{
  if (need_parens) octl->output_str("(", octl);
  form_constant(constant->variant.label_difference.to_address,
                /*need_parens=*/FALSE, octl);
  octl->output_str(" - ", octl);
  form_constant(constant->variant.label_difference.from_address,
                /*need_parens=*/FALSE, octl);
  if (need_parens) octl->output_str(")", octl);
}  /* form_label_difference_constant */

#endif /* GNU_EXTENSIONS_ALLOWED */

void form_integer_constant(a_constant_ptr                        constant,
                           a_boolean                             suppress_cast,
                           a_boolean                             need_parens,
                           an_il_to_str_output_control_block_ptr octl)
/*
Output a string for an integer constant (i.e., a constant with an integral
type; this includes ck_integer, ck_label_difference, and ck_upc_threads
constants).  The constant is written in integer form even if it has been
cast to another type (e.g., a pointer type); the caller must handle
the implicit cast for that case if appropriate.  If suppress_cast is TRUE,
suppress any cast of the constant to another type.  If need_parens is TRUE,
parentheses are placed around the constant if there's any possibility of
precedence confusion.  Do the output in the way described by octl.
*/
{
  a_boolean       need_cast_close_paren = FALSE;
  a_boolean       need_negative_close_paren = FALSE;
  a_boolean       err, minus_1_trick = FALSE;
  a_constant_ptr  eff_constant = constant;
  a_constant_ptr  local_con = local_constant();
  a_type_ptr      con_type = skip_typerefs(constant->type);
  a_boolean       integer_type_constant =
                                   (con_type->kind == (a_type_kind)tk_integer);
  an_integer_kind ikind = (an_integer_kind)ik_none;
  a_boolean       signed_constant = FALSE;
  a_number_buffer literal_form;
  a_boolean       is_negative = FALSE;

  /* See if the constant is signed. */
  if (integer_type_constant) {
    ikind = con_type->variant.integer.int_kind;
    signed_constant = int_kind_is_signed[(int)ikind];
#if INT128_EXTENSIONS_ALLOWED
    if ((ikind == ik_int128 || ikind == ik_unsigned_int128) &&
        octl->gen_compilable_code) {
      /* Clang and GCC do not permit 128-bit integer literals.  So the
         alternative is to render a large 128-bit value X of type T as
         ((T)X_ms << 64 | (T)X_ls) where _ms and _ls indicate the most and
         least significant halves of the value representation. */
      an_integer_value  val, zero, mask;
      octl->output_str("(", octl);
      need_cast_close_paren = TRUE;
      form_cast(constant->type, octl);
      set_integer_value(&zero, (a_host_large_integer)0);
      set_integer_value(&val, constant->variant.integer_value);
      shift_right_integer_value(&val, 64, signed_constant,
                                /*sign_extend=*/TRUE);
      if (cmp_integer_values(&val, signed_constant, &zero, signed_constant)
                                                                       != 0) {
        /* The upper 64 bits are nonzero. */
        a_number_buffer str_rep = str_for_integer_value(
                                                     &val,
                                                     signed_constant,
                                                     constant->non_arithmetic,
                                                     sizeof(an_integer_value));

        octl->output_str(str_rep.as_temp_characters(), octl);
        octl->output_str("<<64 | ", octl);
        form_cast(constant->type, octl);
      }  /* if */
      set_integer_value(&val, constant->variant.integer_value);
      set_integer_value(&mask, (a_host_large_unsigned)0);
      complement_integer_value(&mask);
      shift_left_integer_value(&mask, 64, &err);
      complement_integer_value(&mask);
      and_integer_values(&val, &mask);

      a_number_buffer str_rep = str_for_integer_value(
                                                     &val,
                                                     signed_constant,
                                                     constant->non_arithmetic,
                                                     sizeof(an_integer_value));
      octl->output_str(str_rep.as_temp_characters(), octl);
      goto close_paren_if_needed;
    }  /* if */
#endif /* INT128_EXTENSIONS_ALLOWED */
  } else {
    /* Treat null pointer constants as signed since it doesn't change the
       meaning and looks nicer. */
    if (cmplit_integer_constant(constant, (a_host_large_integer)0) == 0) {
      signed_constant = TRUE;
    }  /* if */
  }  /* if */
  if (!suppress_cast &&
      /* If this is an integer value or enumerator constant cast to
         an enum type in C mode, or an integer value cast to an enum
         type in C++ mode (note that real enumerator constants don't
         get here), ... */
      ((integer_type_constant && con_type->variant.integer.enum_type &&
      /* Don't do this in the C-generating back end when generating K&R C,
         because enum types don't appear. */
        !(is_for_c_gen_be(octl) && octl->gen_pcc_code)) ||
      /* ... or, it's a constant that's shorter than int, ... */
       (integer_type_constant && (int)ikind < (int)ik_int &&
        !octl->suppress_cast_on_short_integral_const) ||
      /* ... or, we're generating K&R C and it's an unsigned constant
         (pcc doesn't support unsigned integral constants), ... */
        (!signed_constant && octl->gen_pcc_code))) {
    /* ... then prefix the constant with an explicit cast. */
    output_optional_open_paren(&need_parens, &need_cast_close_paren, octl);
    form_cast(constant->type, octl);
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  if (constant->kind == (a_constant_repr_kind)ck_label_difference) {
    form_label_difference_constant(constant, need_parens, octl);
    goto close_paren_if_needed;
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if UPC_EXTENSIONS_ALLOWED
  if (constant->kind == (a_constant_repr_kind)ck_upc_threads) {
    octl->output_str("(", octl);
  } else
#endif /* UPC_EXTENSIONS_ALLOWED */
  /* Do not insert code here. */
  {
    check_assertion(constant->kind == (a_constant_repr_kind)ck_integer);
  }  /* if */
  if (signed_constant && sign_of_integer_constant(constant) < 0) {
    /* Negative value.  Put in parentheses. */
    is_negative = TRUE;
    output_optional_open_paren(&need_parens, &need_negative_close_paren, octl);
    if (octl->gen_compilable_code) {
      /* Check for cases on two's complement machines where the constant
         cannot be represented as a positive constant preceded by a minus
         sign.  For those cases, use the -INT_MAX-1 trick.  One reason
         we do this is so that the type of the constant is right. */
      *local_con = *constant;
      negate_integer_value(&local_con->variant.integer_value, &err);
      /* coverity[uninit_use_in_call] */
      if (!err &&
          le_max_integer_value_of_kind(&local_con->variant.integer_value,
                                       /*is_signed=*/TRUE, ikind)) {
        /* The negative of the constant is a legal constant. */
      } else if (constant->non_arithmetic) {
        /* This constant appeared in the source in a nonstandard way.  Keep
           a single-token form because using the -INT_MAX-1 trick can lead to
           errors.  E.g., -9223372036854775808L would be rendered as
           -(-9223372036854775808L-1) which a constexpr interpreter will
           diagnose.  Instead, render it as 0x8000000000000000L. */
        is_negative = FALSE;
      } else {
        /* The negative of the constant is not legal.  Use the -INT_MAX-1
           trick. */
        minus_1_trick = TRUE;
        *local_con = *constant;
        eff_constant = local_con;
        incr_integer_value(&local_con->variant.integer_value);
      }  /* if */
    }  /* if */
  }  /* if */
  /* Write the literal form of the constant. */
  if (octl->gen_compilable_code && !is_negative) {
    /* In compilable code, we want to represent non-negative non-arithmetic
       integer constants in hexadecimal. */
    literal_form = str_for_integer_constant(eff_constant);
  } else {
    /* In diagnostics and debugging output, always use decimal. */
    literal_form = decimal_str_for_integer_constant(eff_constant);
  }  /* if */
  output_partial_token_str(literal_form.as_temp_characters(), octl);
  if (!octl->part_of_ud_literal) {
    /* Put out a suffix if needed.  The suffix must be suppressed for the
       numeric part of a user-defined literal lest it be considered part of
       the literal suffix. */
    /* Unsigned suffix is only valid in ANSI C.  When generating K&R C,
       a prefix cast is used (see above). */
    if (!signed_constant && !octl->gen_pcc_code) {
      /* Unsigned constant. */
      output_partial_token_str("U", octl);
    }  /* if */
    if (integer_type_constant) {
      /* Add length suffixes if appropriate. */
      if (ikind == (an_integer_kind)ik_long           ||
          ikind == (an_integer_kind)ik_unsigned_long) {
        output_partial_token_str("L", octl);
  #if LONG_LONG_ALLOWED
      } else if (ikind == (an_integer_kind)ik_long_long ||
                 ikind == (an_integer_kind)ik_unsigned_long_long) {
        if (use_microsoft_form()) {
          output_partial_token_str("i64", octl);
        } else {
          output_partial_token_str("LL", octl);
        }  /* if */
  #endif /* LONG_LONG_ALLOWED */
      }  /* if */
    }  /* if */
  }  /* if */
  if (minus_1_trick) octl->output_str("-1", octl);
  output_optional_close_paren(need_negative_close_paren, octl);
#if UPC_EXTENSIONS_ALLOWED
  if (constant->kind == (a_constant_repr_kind)ck_upc_threads) {
    octl->output_str("*THREADS)", octl);
  }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
#if INT128_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED
close_paren_if_needed:
#endif /* INT128_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED */
  output_optional_close_paren(need_cast_close_paren, octl);
  release_local_constant(&local_con);
}  /* form_integer_constant */


int form_char(char                                  ch,
              an_il_to_str_output_control_block_ptr octl)
/*
Output the indicated character as part of a string literal or character
constant.  Handle unprintable characters and necessary escapes.  Do the
output in the way described by octl.  Return the number of characters
output.
*/
{
  Small_string<10> buffer;

  if ((isprint((unsigned char)ch) &&
       /* Render extended characters as octal escapes in compilable code to
          avoid potential misinterpretation in a different code page: */
       !(octl->gen_compilable_code && ((unsigned char)ch) > 0x7f)
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
       /* The Sun cc (4.1.2) in -O mode when outputting assembly language
          has a bug that transforms quote into accent grave.  Avoid it. */
       && !(sun_is_generated_code_target && ch == '\'')
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
                                                       ) ||
      (ch == '\t' && octl->gen_raw_tab_in_literals)) {
    a_string_view ch_value(&ch, 1);

    /* Escape some characters, e.g., quotes. */
    if (ch == '"' || ch == '\'' || ch == '\\' ||
        /* Avoid accidentally putting out trigraphs by escaping "?". */
        (ch == '?' && octl->gen_compilable_code && !octl->gen_pcc_code)) {
      buffer.reset_to("\\", ch_value);
    } else {
      buffer.reset_to(ch_value);
    }  /* if */
  } else {
    char c = 0;
    /* Look for unprintable characters with specific escape codes. */
    switch (ch) {
                                  /* pcc does not recognize \a.  Also, some
                                     SVR4 compilers give a warning on it. */
      case TARG_ALERT_CHAR:       if (!is_for_c_gen_be(octl) &&
                                      !octl->gen_pcc_code) c = 'a';
                                  break;
      case TARG_BACKSPACE_CHAR:   c = 'b'; break;
      case TARG_FORM_FEED_CHAR:   c = 'f'; break;
      case TARG_NEWLINE_CHAR:     c = 'n'; break;
      case TARG_CARR_RETURN_CHAR: c = 'r'; break;
      case TARG_HORIZ_TAB_CHAR:   c = 't'; break;
      case TARG_VERT_TAB_CHAR:    c = 'v'; break;
      /* Default case: no escape sequence is defined. */
      default:                    break;
    }  /* switch */
    if (c != 0) {
      /* Use a defined escape code. */
      a_string_view escape_code(&c, 1);

      buffer.reset_to("\\", escape_code);
    } else {
      /* Use the \nnn form for other unprintable characters. */
      unsigned long octal_value(
                       (unsigned long)(ch&((1<<targ_host_string_char_bit)-1)));

      buffer.reset_to("\\", left_pad(3, '0', octal_view_of(octal_value)));
    }  /* if */
  }  /* if */
  /* Output the character. */
  output_partial_token_str(buffer.as_temp_characters(), octl);
  return (int)buffer.length();
}  /* form_char */


static int form_wide_char(unsigned long                         wc,
                          an_il_to_str_output_control_block_ptr octl)
/*
Output the indicated wide character (which may be a wchar_t, char16_t, or
char32_t character) or UTF-8 character as part of a string literal or
character constant.  Handle unprintable characters and necessary escapes.
Do the output in the way described by octl.  Return the number of
characters output.
*/
{
  /* Use hex escapes always to avoid having to convert the wide character
     back to a multibyte character string. */
  a_number_buffer buffer("\\x", hex_view_of(wc));

  /* Output the character. */
  output_partial_token_str(buffer.as_temp_characters(), octl);
  return (int)buffer.length();
}  /* form_wide_char */


static void form_pm_base_casts(a_derivation_step_ptr                path,
                              a_type_ptr                            pm_type,
                              an_il_to_str_output_control_block_ptr octl)
/*
Output casts to cast a pointer-to-member constant to a pointer-to-member
of a base class (this requires an explicit cast).  path is the derivation
path to the base class.  pm_type is the pointer-to-member type we want
to end up with.  Do the output in the way described by octl.
*/
{
  a_type temp_type;

  check_assertion(pm_type->kind == (a_type_kind)tk_ptr_to_member);
  /* Generate a cast for each derivation step in case there are ambiguities
     etc.  They have to be put out in reverse order, since the last cast
     comes first in the source. */
  if (path->next == NULL) {
    /* Don't do the last step, since it is done at the top of form_constant
       because the implicit_cast flag is set. */
  } else {
    /* Use a recursive call to process all the steps after the first one. */
    form_pm_base_casts(path->next, pm_type, octl);
    /* Make a temporary pointer-to-member type with the right class type by
       modifying a copy of the pm_type. */
    temp_type = *pm_type;
    temp_type.variant.ptr_to_member.class_of_which_a_member =
                                                        path->base_class->type;
    /* Generate the cast for the first step. */
    form_cast(&temp_type, octl);
  }  /* for */
}  /* form_pm_base_casts */


static void form_pm_derived_casts(
                                 a_derivation_step_ptr                 path,
                                 a_type_ptr                            pm_type,
                                 an_il_to_str_output_control_block_ptr octl)
/*
Output casts to cast a pointer-to-member constant to a pointer-to-member
of a derived class.  path is the derivation to the base class.  pm_type
is the pointer-to-member type we want to end up with.  Do the output in
the way described by octl.
*/
{
  a_type temp_type;

  check_assertion(pm_type->kind == (a_type_kind)tk_ptr_to_member);
  /* Generate a cast for each derivation step in case there are ambiguities
     etc.  The derivation is in reverse order, but we want to put it
     out in reverse order because the last cast comes first in the source,
     so a simple loop works right. */
  /* Don't do the last step, since it is done at the top of form_constant
     because the implicit_cast flag is set. */
  for (; path->next != NULL; path = path->next) {
    /* Make a temporary pointer-to-member type with the right class type by
       modifying a copy of the pm_type. */
    temp_type = *pm_type;
    temp_type.variant.ptr_to_member.class_of_which_a_member =
                                                        path->base_class->type;
    /* Generate the cast for the first step. */
    form_cast(&temp_type, octl);
  }  /* for */
}  /* form_pm_derived_casts */


void form_pm_constant(a_constant_ptr                        constant,
                      a_boolean                             minimal_casts,
                      a_boolean                             need_parens,
                      an_il_to_str_output_control_block_ptr octl)
/*
Output a pointer-to-member constant.  If minimal_casts is TRUE, suppress
any unnecessary casts in the generated form of the constant (casts that
serve just to disambiguate).  If need_parens is TRUE, parentheses are
placed around the constant if there's any possibility of precedence confusion.
Do the output in the way described by octl.
*/
{
  a_type_ptr              orig_type = constant->type;
  a_type_ptr              con_type = skip_typerefs(orig_type);
  a_source_correspondence *scp = NULL;
  a_boolean               need_cast_close_paren = FALSE;
  an_il_entry_kind        entry_kind;
  a_base_class_ptr        bcp =
                            constant->variant.ptr_to_member.casting_base_class;
  a_boolean               function_case =
                               constant->variant.ptr_to_member.is_function_ptr;

  /* See if this is a pointer to data member or pointer to member function. */
  if (function_case) {
    a_routine_ptr rout = constant->variant.ptr_to_member.variant.routine;
    if (rout != NULL) scp = &rout->source_corresp;
    entry_kind = iek_routine;
  } else {
    a_field_ptr field = constant->variant.ptr_to_member.variant.field;
    if (field != NULL) scp = &field->source_corresp;
    entry_kind = iek_field;
  }  /* if */
  /* If the constant is implicitly cast to another type, ... */
  if (constant->implicit_cast) {
    /* ... then optionally prefix the constant with an explicit cast. */
    if (!minimal_casts ||
        constant->variant.ptr_to_member.cast_to_base ||
        (bcp != NULL && any_nonpublic_steps_in_derivation(bcp)) ||
        scp == NULL) {
      /* If minimal_casts is TRUE, we only output the explicit cast if:
         1) the cast is from a pointer-to-derived-member to a
            pointer-to-base-member, or
         2) there's a chance the base class might be inaccessible, or
         3) the operand of the cast is 0 rather than the address of a data
            member or member function. */
      output_optional_open_paren(&need_parens, &need_cast_close_paren, octl);
      form_cast(orig_type, octl);
    }  /* if */
  }  /* if */
  if (scp == NULL) {
    /* A null pointer-to-member.  implicit_cast will be TRUE, so a cast
       to the right type has been put out above. */
    octl->output_str("0", octl);
  } else {
    /* A non-null pointer-to-member. */
    a_boolean need_pm_close_paren = FALSE;
    a_boolean force_qualified_name;
    output_optional_open_paren(&need_parens, &need_pm_close_paren, octl);
    if (!minimal_casts && bcp != NULL) {
      /* The pointer-to-member has been cast to another class.  Put in
         proper casts.  Note that implicit_cast will be set and therefore
         the final cast has already been issued above. */
      if (bcp->is_virtual) {
        /* For virtual base classes, the single cast generated above is
           enough.  In fact, we don't want to choose among the possible
           paths to the virtual base class if there are several. */
      } else {
        a_derivation_step_ptr path = bcp->derivation->path;
        /* Cast to the proper result type. */
        if (constant->variant.ptr_to_member.cast_to_base) {
          form_pm_base_casts(path, con_type, octl);
        } else {
          form_pm_derived_casts(path, con_type, octl);
        }  /* if */
      }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (microsoft_mode && microsoft_version < 1100 &&
          octl->gen_compilable_code) {
        /* MSVC++ 4.2 doesn't like pointer-to-member casts that adjust both
           the base class and the member type, so add an extra cast to
           adjust the member type, if necessary. */
        a_type_ptr new_member_type = pm_member_type(con_type);
        a_type_ptr member_type = NULL, member_class = NULL;
        if (function_case) {
          a_routine_ptr rout = constant->variant.ptr_to_member.variant.routine;
          if (rout != NULL) {
            member_type = rout->type;
            member_class = parent_class_of(rout);
          }  /* if */
        } else {
          a_field_ptr field = constant->variant.ptr_to_member.variant.field;
          if (field != NULL) {
            member_type = field->type;
            member_class = parent_class_of(field);
          }  /* if */
        }  /* if */
        if (member_type != NULL &&
            standalone_identical_types(member_type, new_member_type)) {
          /* The cast is not needed if the old and new types are the
             same. */
          member_type = NULL;
        }  /* if */
        /* No cast is needed for a null pointer-to-member constant. */
        if (member_type != NULL) {
          /* Make a local pointer to member type and cast to it. */
          a_type temp_type;
          /* Can't call clear_type in a standalone program, so copy and
             modify an existing type. */
          temp_type = *con_type;
          temp_type.variant.ptr_to_member.type = new_member_type;
          temp_type.variant.ptr_to_member.class_of_which_a_member=member_class;
          form_cast(&temp_type, octl);
        }  /* if */
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    }  /* if */
    octl->output_str("&", octl);
    /* Output the name, either in the original form or as a qualified
       name. */
    if (constant->variant.ptr_to_member.name_reference != NULL) {
#if BACK_END_IS_CP_GEN_BE
      if (constant->variant.ptr_to_member.name_reference->qualifier == NULL &&
          use_microsoft_form() && msvc_target_version_number < 1310) {
        /* MSVC++ versions before 7.1 sometimes get confused with an
           unqualified name in a pointer-to-member constant, so for those
           versions we always use a qualified name, even if the source did
           not. */
        force_qualified_name = TRUE;
      } else
#endif /* BACK_END_IS_CP_GEN_BE */
      /* Do not insert code here. */
      {
        /* If there is a special routine for name reference output, attempt
           to use it to put out the name.  If there is no special routine,
           or if the name was not emitted, put it out as a qualified
           name. */
        force_qualified_name =
                   !(octl->output_name_reference != NULL &&
                     octl->output_name_reference(
                                constant->variant.ptr_to_member.name_reference,
                                scp, entry_kind,
                                /*is_declaration=*/FALSE,
                                /*suppress_declarator_parens=*/FALSE));
      }
    } else {
      /* There's no name reference available; use a qualified name. */
      force_qualified_name = TRUE;
    }  /* if */
    if (force_qualified_name) {
      a_boolean saved_force_qualified_name = octl->force_qualified_name;
      octl->force_qualified_name = TRUE;
      form_name(scp, entry_kind, octl);
      octl->force_qualified_name = saved_force_qualified_name;
    }  /* if */
    output_optional_close_paren(need_pm_close_paren, octl);
  }  /* if */
  output_optional_close_paren(need_cast_close_paren, octl);
}  /* form_pm_constant */


static a_boolean types_match_ignoring_qualifiers(a_type_ptr type_1,
                                                 a_type_ptr type_2)
/*
Return TRUE if type_1 and type_2 are the same type ignoring type qualifiers.
This is used in deciding whether a particular lvalue formulation should
be used for an address constant; the differences allowed are ones that
can be bridged by a cast on an lvalue address.  This routine is similar to
same_type_with_added_qualifiers, but simpler, and needed here because
in il_to_str we can't use routines that aren't available to back ends
and standalone utility programs.
*/
{
  a_boolean types_match = FALSE;

  type_1 = skip_typerefs(type_1);
  type_2 = skip_typerefs(type_2);
  if (same_entities(type_1, type_2)) {
    types_match = TRUE;
  } else if (type_1->kind != type_2->kind) {
    /* Type kinds do not match, so types do not match. */
    /* types_match = FALSE; -- already set. */
  } else if (type_1->kind == (a_type_kind)tk_pointer
#ifdef pointer_types_have_same_repr
             && pointer_types_have_same_repr(type_1, type_2)
#endif /* ifdef pointer_types_have_same_repr */
             && type_1->variant.pointer.is_reference ==
                type_2->variant.pointer.is_reference
#if MICROSOFT_EXTENSIONS_ALLOWED
             && type_1->variant.pointer.is_handle == 
                type_2->variant.pointer.is_handle
             && type_1->variant.pointer.is_interior_ptr == 
                type_2->variant.pointer.is_interior_ptr
             && type_1->variant.pointer.is_pin_ptr == 
                type_2->variant.pointer.is_pin_ptr
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
            ) {
    /* Continue at the next level for pointers. */
    types_match = types_match_ignoring_qualifiers(type_pointed_to(type_1),
                                                  type_pointed_to(type_2));
  } else if (type_1->kind == (a_type_kind)tk_ptr_to_member) {
    a_type_ptr  class_type_1 = pm_class_type(type_1);
    a_type_ptr  class_type_2 = pm_class_type(type_2);
    if (same_entities(class_type_1, class_type_2)) {
      /* Continue at the next level for pointers to members. */
      types_match = types_match_ignoring_qualifiers(pm_member_type(type_1),
                                                    pm_member_type(type_2));
    }  /* if */
  } else if (!C_mode() &&
             type_1->kind == (a_type_kind)tk_array &&
             !has_unknown_specified_bound(type_1) &&
             !has_unknown_specified_bound(type_2) &&
             type_1->variant.array.variant.number_of_elements ==
                            type_2->variant.array.variant.number_of_elements) {
    /* Continue at the next level for arrays (in C++ mode, the qualifiers on
       array element types count as qualifiers on the array). */
    types_match = types_match_ignoring_qualifiers(array_element_type(type_1),
                                                  array_element_type(type_2));
  }  /* if */
  return types_match;
}  /* types_match_ignoring_qualifiers */


static a_boolean type_matches_desired_type(a_type_ptr type,
                                           a_type_ptr desired_type,
                                           a_boolean  will_use_as_addr,
                                           a_boolean  *type_decay_used)
/*
Return TRUE if type is the same as desired_type, for the purpose of
forming an lvalue as part of generating an address constant.
If will_use_as_addr is TRUE, array-to-pointer decay can be considered
in the match-up.  If type decay is used in making the match, return
*type_decay_used TRUE.
*/
{
  a_boolean type_matches = FALSE;

  *type_decay_used = FALSE;
  /* Check for the same type, ignoring qualifier differences. */
  if (type == desired_type ||  /* For speed. */
      types_match_ignoring_qualifiers(type, desired_type)) {
    type_matches = TRUE;
  } else if (will_use_as_addr) {
    /* Check for the decay cases.  Note that this is checked only when the
       result will be used as an address, and therefore will decay from an
       lvalue to an rvalue.  Note that desired_type is an lvalue type, but
       when will_use_as_addr is TRUE what we're really aiming for is
       pointer-to that type, e.g., if type is "int[3]" and desired_type
       is "int" there's a match, because the type after decay ("int *")
       matches the type when you take the address of the lvalue.
       Note that under this formulation there's no need to check for
       the function decay, since it comes out the same as the first test
       above. */
    if (is_array_type(type)) {
      a_type_ptr element_type = array_element_type(type);
      if (types_match_ignoring_qualifiers(element_type, desired_type)) {
        type_matches = TRUE;
        *type_decay_used = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return type_matches;
}  /* type_matches_desired_type */


static a_field_ptr select_union_field_for_addr_constant(
                                                   a_type_ptr union_type,
                                                   a_type_ptr desired_type,
                                                   a_boolean  will_use_as_addr)
/*
union_type is a union type.  Try to find a field of that union that has
type desired_type, and return a pointer to it.  If desired_type is NULL or
if no field matches, return NULL.  If will_use_as_addr is TRUE,
array-to-pointer decay can be considered in matching the type.
*/
{
  a_field_ptr field, selected_field = NULL;
  a_boolean   type_decay_used;

  if (desired_type != NULL) {
    /* Go through the fields, looking for one with the right type. */
    for (field = union_type->variant.class_struct_union.field_list;
         field != NULL;
         field = field->next) {
      if (field->is_bit_field) {
        /* Ignore bit fields. */
        continue;
      }  /* if */
      if (type_matches_desired_type(field->type, desired_type,
                                    will_use_as_addr, &type_decay_used)) {
        /* If there are several fields with the same type, favor the one with
           the most access. */
        if (field->source_corresp.access == (an_access_specifier)as_public) {
          selected_field = field;
          break;
        }  /* if */
        if (selected_field == NULL ||
            is_more_accessible(field->source_corresp.access,
                               selected_field->source_corresp.access)) {
          /* This field is not public, but it's the most accessible field of
             the right type we've seen so far, so remember it and keep
             looking. */
          selected_field = field;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  return selected_field;
}  /* select_union_field_for_addr_constant */


static a_field_ptr select_arbitrary_field_of_union(a_type_ptr union_type)
/*
union_type points to a union.  Select an arbitrary field from that union
and return a pointer to it.  But try to pick a field that does not have
a class type, because putting a "&" in front of a class can run into
problems if the class overloads operator&.  Generates an internal error
if given an empty union.
*/
{
  a_field_ptr field;

  /* Go though the fields, looking for one with a non-class type. */
  for (field = union_type->variant.class_struct_union.field_list;
       field != NULL;
       field = field->next) {
    a_type_ptr ftype = skip_typerefs(field->type);
    if (ftype->kind != (a_type_kind)tk_class &&
        ftype->kind != (a_type_kind)tk_struct) break;
  }  /* for */
  if (field == NULL) {
    /* No field had a non-class type, so use the first field. */
    field = union_type->variant.class_struct_union.field_list;
    /* Unions can be empty, but we shouldn't be taking the address of
       something inside an empty one. */
    check_assertion(field != NULL);
  }  /* if */
  return field;
}  /* select_arbitrary_field_of_union */


void form_uuidof_reference(a_constant_ptr                        con,
                           an_il_to_str_output_control_block_ptr octl)
/*
Output a Microsoft __uuidof reference implied by the given constant (which is a
ck_address or ck_template_param constant).  Do the output in the way described
by octl.
*/
{
  a_type_ptr        uuid_type = NULL;
  an_expr_node_ptr  uuid_expr = NULL;

  switch (con->kind) {
    case ck_address:
      check_assertion_str(con->variant.address.kind ==
                                              (an_address_base_kind)abk_uuidof,
                          "form_uuidof_reference: bad kind");
      uuid_type = con->variant.address.variant.type;
      break;
    case ck_template_param:
      uuid_expr = generic_sizeof_arg_expr(con);
      break;
    default:
      unexpected_condition();
  }  /* switch */
  octl->output_str("__uuidof(", octl);
  if (uuid_expr != NULL) {
    form_expression(uuid_expr, octl);
  } else if (uuid_type != NULL) {
    form_type(uuid_type, octl);
  } else {
    /* Zero GUID. */
    octl->output_str("0", octl);
  }  /* if */
  octl->output_str(")", octl);
}  /* form_uuidof_reference */


void form_typeid_reference(a_constant_ptr                        con,
                           an_il_to_str_output_control_block_ptr octl)
/*
Output a  typeid reference implied by the given constant (which is a
ck_address constant).  Do the output in the way described by octl.
*/
{
  a_type_ptr        typeid_type = NULL;
  an_expr_node_ptr  typeid_expr = NULL;
  a_boolean         is_cli_typeid = FALSE;

  switch (con->kind) {
    case ck_address:
#if MICROSOFT_EXTENSIONS_ALLOWED
      is_cli_typeid =
            cli_or_cx_enabled &&
            con->variant.address.kind == (an_address_base_kind)abk_cli_typeid;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      check_assertion_str(is_cli_typeid ||
                          con->variant.address.kind ==
                                             (an_address_base_kind)abk_typeid,
                          "form_typeid_reference: bad kind");
      typeid_type = con->variant.address.variant.type;
      break;
    case ck_template_param:
      typeid_expr = generic_sizeof_arg_expr(con);
      typeid_type = con->variant.template_param.variant.templ_sizeof.type;
      break;
    default:
      unexpected_condition();
  }  /* switch */
  if (!is_cli_typeid) {
    octl->output_str("typeid(", octl);
  }  /* if */
  if (typeid_expr != NULL) {
    form_expression(typeid_expr, octl);
  } else if (typeid_type != NULL) {
    form_type(typeid_type, octl);
  } else {
    unexpected_condition();
  }  /* if */
  if (is_cli_typeid) {
    /* coverity[dead_error_line] */
    octl->output_str("::typeid", octl);
  } else {
    octl->output_str(")", octl);
  }  /* if */
}  /* form_typeid_reference */


static a_type_ptr static_unknown_type(void)
/*
Return a pointer to an unknown type.  Don't do allocation, because that is not
allowed in a back end.  This is used to produce a type that will not
match other types in the IL.
*/
{
  a_type_ptr    type;
  STATIC_THREAD a_type unkn_type;

  type = &unkn_type;
  /* Can't use clear_type here either.  This is unfortunate, but all we
     need is a type that won't match other types. */
  memzero((char *)&unkn_type, sizeof(unkn_type));
  unkn_type.kind = (a_type_kind)tk_unknown;
  return type;
}  /* static_unknown_type */


static void form_lvalue_for_addressed_entity(
                   a_constant_ptr                        constant,
                   a_type_ptr                            desired_type,
                   a_boolean                             will_use_as_addr,
                   a_boolean                             base_entity_only,
                   a_boolean                             gen_output,
                   a_type_ptr                            *achieved_type,
                   a_boolean                             *type_decay_used,
                   a_targ_ptrdiff_t                      *offset,
                   a_boolean                             *formed_useful_lvalue,
                   an_il_to_str_output_control_block_ptr octl)
/*
Output code that is an lvalue for the entity addressed by the ck_address
constant "constant".  This is done as part of outputting an address constant.
The output can be as simple as "x" or something more complicated like
"x.a.b[5]".  desired_type is the lvalue type ultimately wanted, or NULL
if no preference is indicated; will_use_as_addr is TRUE if the lvalue
will be used as an address (rather than directly as an lvalue).  If
base_entity_only is TRUE, the base entity is put out but no attempt is
made to add addressing modifiers to it.  On return, *achieved_type is set
to the type of the lvalue.  If type decay was used in making the match,
*type_decay_used is returned TRUE, and *achieved_type is the type after
the type decay, minus the top-level pointer-to.  *offset is set to
whatever part of the ck_address constant offset couldn't be dealt with
in the lvalue.  If the type achieved is the desired type, ignoring
type qualifiers, and the offset was dealt with in some appropriate way,
*formed_useful_lvalue is returned TRUE.  If it's returned FALSE,
*achieved_type is set to the base entity type.  Do the output in the
way described by octl, but output nothing if gen_output == FALSE; that
is used for a first exploratory pass.  Note that the addressing
operators put out by this routine are never affected by overloading.
That is, this routine never puts out something like a "&" on a class
that might have operator& overloaded.  However, this routine still
generates code that is not compilable in some obscure C++ cases (e.g.,
taking the address of an inaccessible nonstatic data member of a class);
the C++-generating back end avoids that by arranging to have constant
addressing operations not folded to constants so that such cases won't
come here.  Parentheses are not put around the output; all the
operations generated bind very tightly to the identifier, so
parentheses are not needed.
*/
{
  a_type_ptr              type = NULL, orig_type;
  a_constant_ptr          con = NULL;
  a_source_correspondence *entity_scp = NULL;
  an_il_entry_kind        entity_kind = iek_none;
  a_field_ptr             field;
  a_boolean               proper_type = FALSE;
  a_boolean               local_type_decay_used;
  a_targ_ptrdiff_t        orig_offset = constant->variant.address.offset;
  an_address_base_kind    special_address_kind =
                                               (an_address_base_kind)abk_last;

  *formed_useful_lvalue = FALSE;
  *type_decay_used = FALSE;
  *offset = orig_offset;
  /* Get the type of the underlying entity. */
  switch (constant->variant.address.kind) {
    case abk_routine:
      /* A routine. */
      { a_routine_ptr rout = constant->variant.address.variant.routine;
        type = rout->type;
        entity_kind = iek_routine;
        entity_scp = &rout->source_corresp;
        *type_decay_used = TRUE;
      }
      break;
    case abk_variable:
      /* A variable. */
      { a_variable_ptr var = constant->variant.address.variant.variable;
        /* In C++, an anonymous union cannot be named directly, so we have to
           find a field within the union. */
        if (var->is_anonymous_parent_object && !is_for_c_gen_be(octl)) {
          a_type_ptr  union_type = skip_typerefs(var->type);
          field = select_union_field_for_addr_constant(union_type,
                                                       desired_type,
                                                       will_use_as_addr);
          if (field == NULL) {
            /* If no field matches, choose one mostly arbitrarily. */
            /* Note that we're lucky that anonymous unions cannot have
               nonpublic members. */
            field = select_arbitrary_field_of_union(union_type);
          }  /* if */
          type = field->type;
          entity_kind = iek_field;
          entity_scp = &field->source_corresp;
        } else {
          /* Normal variable, not anonymous union. */
          type = var->type;
          entity_kind = iek_variable;
          entity_scp = &var->source_corresp;
        }  /* if */
      }
      break;
    case abk_constant:
      /* Address of a constant, specifically a string. */
      con = constant->variant.address.variant.constant;
#if !STANDALONE_UTILITY_PROGRAM
      check_assertion_str(con->kind == (a_constant_repr_kind)ck_string ||
                          con->kind == (a_constant_repr_kind)ck_error ||
                          constexpr_enabled, 
                 "form_lvalue_for_addressed_entity: address of nonstring con");
#endif /* !STANDALONE_UTILITY_PROGRAM */
      type = con->type;
      break;
    case abk_temporary:
      /* Temporary with a constant value. */
      con = constant->variant.address.variant.constant;
      type = con->type;
      if (gen_output && constant_is_recursive(con)) {
        /* Avoid runaway recursion. */
        special_address_kind = constant->variant.address.kind;
        check_assertion(!octl->gen_compilable_code);
        con = NULL;
      }  /* if */
      break;
    case abk_uuidof:
    case abk_typeid:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case abk_cli_typeid:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      /* Address of a structure that represents the uuid or typeid information
         for a given type. */
      special_address_kind = constant->variant.address.kind;
      if (!constant->implicit_cast) {
        type = type_pointed_to(constant->type);
      } else {
        /* The uuidof address is cast to some other type (e.g., int). */
        type = static_unknown_type();
      }  /* if */
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case abk_cli_array:
      special_address_kind = constant->variant.address.kind;
      type = type_pointed_to(constant->type);
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case abk_label:
      /* Address of a label (GNU C extension). */
      { a_label_ptr label = constant->variant.address.variant.label;
        if (!constant->implicit_cast) {
          type = type_pointed_to(constant->type);
        } else {
          /* The label address is cast to some other type (e.g., int). */
          type = static_unknown_type();
        }  /* if */
        entity_kind = iek_label;
        entity_scp = &label->source_corresp;
      }
      break;
    case abk_param_ref:
    default:
      unexpected_condition_str(
                   "form_lvalue_for_addressed_entity: bad addr constant kind");
  }  /* switch */
  orig_type = type;
  /* If the desired type was not specified, use the entity type. */
  if (desired_type == NULL) desired_type = type;
  /* Put out the base entity name or constant. */
  if (gen_output) {
    if (entity_scp != NULL) {
      form_name(entity_scp, entity_kind, octl);
    } else if (con != NULL) {
      /* Constant case. */
      form_constant(con, /*need_parens=*/FALSE, octl);
    } else if (special_address_kind == abk_uuidof) {
      /* Microsoft __uuidof. */
      form_uuidof_reference(constant, octl);
    } else if (special_address_kind == abk_typeid
#if MICROSOFT_EXTENSIONS_ALLOWED
               || constant->variant.address.kind == abk_cli_typeid
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                                                  ) {
      form_typeid_reference(constant, octl);
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (special_address_kind == abk_cli_array) {
      /* A C++/CLI array constant, used only in custom attribute argument
         expressions, is represented with an enk_gcnew expression node
         in constant->expr.  That expression is emitted by form_constant,
         so this code path should not be hit. */
      unexpected_condition();
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else if (special_address_kind == abk_temporary) {
      /* We get here when taking the address of a self-referencing
         temporary. */
      octl->output_str("<", octl);
      octl->output_str(error_text(ec_self_referencing_temporary_object), octl);
      octl->output_str(">", octl);
    } else {
      unexpected_condition();
    }  /* if */
  }  /* if */
  /* If the type is right and the offset is zero, we have what we need. */
  local_type_decay_used = FALSE;
  if (!constant->implicit_cast || /* For speed. */
      type_matches_desired_type(type, desired_type, will_use_as_addr,
                                &local_type_decay_used)) {
    proper_type = TRUE;
  }  /* if */
  if (proper_type && orig_offset == 0) {
    /* The base entity has the type and offset we need. */
    *formed_useful_lvalue = TRUE;
    /* Note we're careful not to clear *type_decay_used if it was set above
       for the function case. */
    if (local_type_decay_used) *type_decay_used = TRUE;
  } else if (base_entity_only) {
    /* We've been told not to look at addressing modifiers, so stop here. */
#if !DO_IL_LOWERING
  } else if (constant->variant.address.subobject_path != NULL) {
    /* Use the subobject path to form the lvalue. */
    a_subobject_path_ptr  path = constant->variant.address.subobject_path;
    while (path != NULL) {
      if (is_array_type(type) && !path->is_offset) {
        /* An array subscripting operation with offset 0 was elided. */
        while (is_array_type(type)) {
          if (gen_output) {
            octl->output_str("[0]", octl);
          }  /* if */
          type = array_element_type(type);
        }  /* while */
      }  /* if */
      if (path->is_offset) {
        if (is_array_type(type)) {
          if (gen_output) {
            octl->output_str("[", octl);
            form_num(path->variant.ptr_offset, octl);
            octl->output_str("]", octl);
          }  /* if */
          type = array_element_type(type);
          *offset -= (path->variant.ptr_offset *
                      (a_targ_ptrdiff_t)size_of_type(type));
        } else {
          /* Can only handle array subscripting operations. */
          break;
        }  /* if */
      } else if (path->is_base_class) {
        type = path->variant.base_class->type;
        *offset -= (a_targ_ptrdiff_t)path->variant.base_class->offset;
      } else {
        field = path->variant.field;
        if (gen_output) {
          octl->output_str(".", octl);
          form_unqualified_name(&field->source_corresp, iek_field, octl);
        }  /* if */
        type = field->type;
        *offset -= (a_targ_ptrdiff_t)field->offset;
      }  /* if */
      path = path->next;
    }  /* while */
    *formed_useful_lvalue = TRUE;
#endif /* !DO_IL_LOWERING */
  } else {
    /* Loop, refining the lvalue each time around, until we get something with
       the right type and right address, or until we decide to give up. */
#if DO_IL_LOWERING
    a_boolean prev_field_was_class_subobject_with_tail_padding = FALSE;
#endif /* DO_IL_LOWERING */
    for (;;) {
      a_type_ptr unqual_type = skip_typerefs(type);
      if (unqual_type->kind == (a_type_kind)tk_array) {
        /* Array. */
        /* When forming an address for an array element, it sometimes makes
           sense to leave part of the offset to be done by the caller.
             A arr[4];
             A *p = arr + 2;
           This form works better than "&arr[2]", which might be taking the
           address of a class object whose operator& is overloaded. */
        if (proper_type && will_use_as_addr) {
          *formed_useful_lvalue = TRUE;
          *type_decay_used = TRUE;
          break;
        } else {
          /* Add a subscripting operation. */
          a_type_ptr       element_type = array_element_type(unqual_type);
          a_targ_ptrdiff_t element_size =
                         (a_targ_ptrdiff_t)f_skip_typerefs(element_type)->size;
          a_targ_ptrdiff_t idx;
          /* g++ allows zero-length arrays. */
          if (element_size == 0) element_size = 1;
          idx = *offset / element_size;
          /* C division of negative numbers does not necessarily truncate
             towards zero.  If it doesn't, adjust to the result one would get
             if it did.  See comments in the routine divide_integers. */
          if (*offset < 0 && (*offset % element_size) > 0) idx++;
          /* Put out the subscripting operation. */
          if (gen_output) {
            octl->output_str("[", octl);
            form_num(idx, octl);
            octl->output_str("]", octl);
          }  /* if */
          type = element_type;
          *offset -= idx * element_size;
        }  /* if */
#if DO_IL_LOWERING
        prev_field_was_class_subobject_with_tail_padding = FALSE;
#endif /* DO_IL_LOWERING */
      } else if (unqual_type->kind == (a_type_kind)tk_class ||
                 unqual_type->kind == (a_type_kind)tk_struct) {
        /* A class; try to find a field with the right offset, or at least
           get closer. */
        /* Give up if the offset is outside the class bounds. */
        if (*offset < 0 ||
            *offset >= (a_targ_ptrdiff_t)unqual_type->size) break;
        for (field = unqual_type->variant.class_struct_union.field_list;
             field != NULL;
             field = field->next) {
          /* Look for a field with the right offset.  We do a full check
             on the bounds of the field because there might be holes between
             fields (e.g., base classes). */
          if ((a_targ_ptrdiff_t)field->offset <= *offset &&
              *offset < (a_targ_ptrdiff_t)(field->offset +
                                      skip_typerefs(field->type)->size) &&
              /* Ignore bit fields. */
              field->bit_size == 0) break;
        }  /* for */
        /* Watch out for classes with no fields. */
        if (field == NULL) break;
        if (!has_name(field)) {
          /* An anonymous union field.  Usually, the field selection for such
             a field is just omitted.  In the C-generating back end, however,
             references to anonymous union fields can't be omitted, so give
             up. */
          if (is_for_c_gen_be(octl)) break;
#if DO_IL_LOWERING
        } else if (field->is_optimized_empty_class &&
                   is_for_c_gen_be(octl)) {
          /* If the field has been eliminated from the lowered struct,
             we can't use that field in the generated output.  Let the caller
             use an appropriate cast instead. */
          break;
#endif /* DO_IL_LOWERING */
        } else {
          /* Normal field (not anonymous union field). */
          /* Put out the field selection. */
          if (gen_output) {
#if DO_IL_LOWERING
            if (prev_field_was_class_subobject_with_tail_padding) {
              /* This field is a member of a subobject class type that was
                 promoted by the C-generating back end into the containing
                 class.  Instead of subobj.mem, it must therefore be put out
                 as subobj_mem. */
              octl->output_str("_", octl);
            } else
#endif /* DO_IL_LOWERING */
            /* Do not insert code here. */
            {
              octl->output_str(".", octl);
            }  /* if */
#if DO_IL_LOWERING
            if (is_for_c_gen_be(octl)) {
              /* Set up for the next pass to use "_" instead of ".". */
              prev_field_was_class_subobject_with_tail_padding =
                                      field->class_subobject_with_tail_padding;
            }  /* if */
#endif /* DO_IL_LOWERING */
            form_unqualified_name(&field->source_corresp, iek_field, octl);
          }  /* if */
        }  /* if */
        type = field->type;
        *offset -= (a_targ_ptrdiff_t)field->offset;
      } else if (unqual_type->kind == (a_type_kind)tk_union) {
        /* For a union, try to find a field with the right type.  If there's
           no match at the top level, don't try to find some sub-aggregate
           of those fields that will give the right offset; just give up. */
        if (*offset != 0) break;
        field = select_union_field_for_addr_constant(unqual_type,
                                                     desired_type,
                                                     will_use_as_addr);
        if (field == NULL) break;
        /* Found a field with the right type, so use it. */
        /* Put out the field selection. */
        if (gen_output) {
          octl->output_str(".", octl);
          form_unqualified_name(&field->source_corresp, iek_field, octl);
        }  /* if */
        type = field->type;
#if DO_IL_LOWERING
        prev_field_was_class_subobject_with_tail_padding = FALSE;
#endif /* DO_IL_LOWERING */
      } else {
        /* Some other type (not an aggregate); we can't adjust the offset or
           type.  Give up. */
        break;
      }  /* if */
      /* If the offset is now zero, and the type is right, we have what we
         need. */
      if (type_matches_desired_type(type, desired_type, will_use_as_addr,
                                    &local_type_decay_used)) {
        proper_type = TRUE;
      }  /* if */
      if (proper_type && *offset == 0) {
        *formed_useful_lvalue = TRUE;
        *type_decay_used = local_type_decay_used;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  if (*formed_useful_lvalue) {
    if (*type_decay_used &&
        constant->variant.address.kind != (an_address_base_kind)abk_routine) {
      /* Adjust the type to reflect that fact that type decay was used.
         The type returned is the type under the resulting pointer type.
         Note that because of the convention used for the type, function
         to pointer decay does not change the type. */
      type = array_element_type(type);
    }  /* if */
  } else {
    /* If we didn't succeed in getting the right type and offset, roll the
       type and offset back to the original from the base entity. */
    type = orig_type;
    *offset = orig_offset;
  }  /* if */
  *achieved_type = type;
}  /* form_lvalue_for_addressed_entity */


static a_boolean ttt_is_dependent_type(a_type_ptr tp,
                                       a_boolean  *end_traversal)
/*
This function is called via traverse_type_tree from
standalone_is_dependent_type.  If tp designates a tk_template_param type, a
nonreal type, or a dependent type operator, it sets *end_traversal to TRUE
and returns TRUE; otherwise, it returns FALSE.
*/
{
  a_boolean result = FALSE;

  if (type_is(tp, tk_template_param) ||
      (is_immediate_class_type(tp) &&
       tp->variant.class_struct_union.is_nonreal_class) ||
      (type_is(tp, tk_typeref) &&
       (tp->variant.typeref.is_nonreal || tp->variant.typeref.is_dependent)) ||
      (type_is(tp, tk_integer) &&
       (tp->variant.integer.is_nonreal))) {
    result = TRUE;
    *end_traversal = TRUE;
  }  /* if */
  return result;
}  /* ttt_is_dependent_type */


static a_boolean standalone_is_dependent_type(a_type_ptr  type_ptr)
/*
Return TRUE if the type pointed to by type_ptr is template-dependent, i.e.,
it is a tk_template_param type, a nonreal type, or a dependent typeref.
*/
{
  a_boolean result = FALSE;

  /* Template parameter types come up only in C++ mode. */
  if (!C_mode()) {
    a_type_tree_traversal_flag_set ttt_flags = (TTT_RETURN_TYPE |
                                                TTT_THIS_PARAM_TYPE |
                                                TTT_PARAM_TYPES |
                                                TTT_NONREAL_TEMPLATE_ARGS |
                                                TTT_PARENT_CLASSES);

    result = traverse_type_tree(type_ptr, ttt_is_dependent_type,
                                ttt_flags);
  }  /* if */
  return result;
}  /* standalone_is_dependent_type */


static a_boolean base_entity_is_dependent_class_member_function(
                                                            a_constant_ptr con)
/*
Return TRUE if con (a ck_address constant) is based on a member function of
a class that is dependent.
*/
{
  a_boolean result = FALSE;

  check_assertion(constant_is(con, ck_address));
  if (con->variant.address.kind == abk_routine &&
      con->variant.address.variant.routine->source_corresp.is_class_member) {
    a_type_ptr parent = parent_class_of(con->variant.address.variant.routine);
    result = standalone_is_dependent_type(parent);
  }  /* if */
  return result;
}  /* base_entity_is_dependent_class_member_function */


static void form_address_constant(
                          a_constant_ptr                        constant,
                          a_boolean                             form_lvalue,
                          a_boolean                             need_parens,
                          an_il_to_str_output_control_block_ptr octl)
/*
Output the value of a ck_address constant.  If form_lvalue is TRUE,
put out an lvalue for the thing at that address.  If need_parens is TRUE,
parentheses are placed around the constant if there's any possibility of
precedence confusion.  Do the output in the way described by octl.
*/
{
  a_type_ptr       orig_type = constant->type;
  a_type_ptr       con_type, desired_type, achieved_type;
  a_type_ptr       direct_desired_type, direct_achieved_type;
  a_targ_ptrdiff_t offset, dummy_offset;
  a_boolean        cast_to_nonpointer = FALSE, type_decay_used;
  a_boolean        need_ampersand_paren = FALSE;
  a_boolean        final_cast_needed = FALSE;
  a_boolean        need_final_cast_close_paren = FALSE;
  a_boolean        need_offset_addition_close_paren = FALSE;
  a_boolean        need_char_star_cast_close_paren = FALSE;
  a_boolean        reinterpret_cast_needed = FALSE;
  a_boolean        need_reinterpret_cast_close_paren = FALSE;
  a_boolean        formed_useful_lvalue, need_char_star_cast = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_boolean        string_handle_case = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

  con_type = skip_typerefs(orig_type);
  /* When the address constant is cast to some strange type (e.g., long),
     do that as a final cast and don't try to accommodate it in the normal
     processing. */
  /* Note that the test for pointer includes reference as well. */
  if (constant->implicit_cast && con_type->kind != (a_type_kind)tk_pointer) {
    check_assertion(!form_lvalue);
    cast_to_nonpointer = TRUE;
    final_cast_needed = TRUE;
    desired_type = NULL;
  } else if (
#if DEBUG
             !octl->debug_output &&
#endif /* DEBUG */
             !is_for_c_gen_be(octl) &&
             constant->implicit_cast &&
             !(constant->explicit_cast_applied ||
               constant->is_compound_literal)) {
    /* The constant was cast, but only implicitly, so leave off the cast.
       Don't do this in the C-generating back end, because some casts
       added there are "implicit" and yet they have to be put out. */
    a_boolean need_desired_type = FALSE;
    if (constant->variant.address.kind == (an_address_base_kind)abk_variable) {
      a_type_ptr var_type =
               skip_typerefs(constant->variant.address.variant.variable->type);
      a_type_ptr target_type = f_skip_typerefs(type_pointed_to(con_type));
      if (is_array_type(var_type)) {
        /* The decay of an array variable to a pointer is implicit, but
         requires a non-NULL desired_type in the form_lvalue... routine
         to get the right result. */
        need_desired_type = TRUE;
      } else if (is_class_struct_union_type(var_type) &&
                 !standalone_identical_types(target_type, var_type)) {
        /* Similarly, if the variable is of a class type and the type of the
           constant is different (indicating that the constant addresses a
           member of the class/struct/union object and not the object itself),
           the target type is potentially needed to address the correct
           member. */
        need_desired_type = TRUE;
      }  /* if */
    }
    if (need_desired_type) {
      desired_type = type_pointed_to(con_type);
    } else {
      /* Normal implicit cast on a constant. */
      desired_type = NULL;
    }  /* if */
  } else {
    desired_type = con_type;
    desired_type = type_pointed_to(desired_type);
  }  /* if */
  if (constant->is_reinterpret_cast && !is_for_c_gen_be(octl)) {
    reinterpret_cast_needed = TRUE;
  }  /* if */
  /* Examine the addressed entity (without generating any code) to
     determine how it will be put out as an lvalue.  This lets us decide
     on putting out a leading cast, etc. before the lvalue is put out. */
  form_lvalue_for_addressed_entity(constant, desired_type,
                                   /*will_use_as_addr=*/!form_lvalue,
                                   /*base_entity_only=*/FALSE,
                                   /*gen_output=*/FALSE,
                                   &achieved_type, &type_decay_used, &offset,
                                   &formed_useful_lvalue, octl);
  if (!type_decay_used && is_array_type(achieved_type) && !form_lvalue) {
    /* Array for which type decay is not being used.  This means, so far,
       that we plan to put a "&" in front of the array.  See if there's a
       reason not to. */
    if (octl->gen_pcc_code) {
      /* Don't do address-of-array in pcc mode, because pcc gives warnings
         on that and uses the pointer-to-element type anyway. */
      type_decay_used = TRUE;
    } else if (octl->gen_compilable_code &&
               constant->variant.address.kind ==
                                          (an_address_base_kind)abk_constant &&
               constant->variant.address.variant.constant->kind ==
                                             (a_constant_repr_kind)ck_string) {
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (is_handle_type(con_type)) {
        /* Omit the cast from a string literal to a handle type. */
        string_handle_case = TRUE;
      } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      /* Do not insert code here. */
      {
        /* Address of a string constant, e.g., &"abc".  Some ANSI/ISO C
           compilers have difficulty with that, perhaps because they don't
           believe a string is an lvalue.  Force type decay and a cast,
           unless the string is part of a user-defined literal, in which
           case the cast must not be put out. */
        type_decay_used = !octl->part_of_ud_literal;
      }
    } else if (offset != 0) {
      /* Some compilers have difficulty with getting the size right when
         adding an offset to the address of an array. */
      type_decay_used = TRUE;
    }  /* if */
    if (type_decay_used) {
      /* If we've turned on array type decay, adjust the type.  Note that
         because of the convention used for the achieved type, the
         pointer-to part of the type is not needed. */
      achieved_type = array_element_type(achieved_type);
      final_cast_needed = TRUE;
    }  /* if */
  } else if (type_decay_used && is_pointer_type(orig_type) &&
             desired_type != NULL && is_function_type(desired_type) &&
             !standalone_is_dependent_type(desired_type) &&
             !base_entity_is_dependent_class_member_function(constant)) {
    /* In case this constant is being used for type deduction, add the "&"
       to ensure that the constant has a pointer type in the generated
       code.  (The exclusion of dependent types and member functions is
       because an "&" in the original source would be represented in the IL
       as an eok_address_of applied to an enk_routine node, not as a
       ck_address constant; i.e., if we are here with a pointer to a
       dependent function type or member function of a dependent class,
       there was no "&" in the original source and we should not add one
       here.) */
    type_decay_used = FALSE;
  }  /* if */
  if (offset != 0) {
    /* The offset is nonzero.  The general way of dealing with this is to cast
       to "char *" and add in the offset.  However, in the right situation
       the addition can be done without going to "char *". */
    need_char_star_cast = TRUE;
    if (!form_lvalue) {
      a_targ_ptrdiff_t size =
                        (a_targ_ptrdiff_t)f_skip_typerefs(achieved_type)->size;
      if (size != 0 && (offset % size) == 0) {
        need_char_star_cast = FALSE;
        offset /= size;
      }  /* if */
    }  /* if */
    if (need_char_star_cast) final_cast_needed = TRUE;
  }  /* if */
  /* See if we need a final cast to the desired type. */
  direct_achieved_type = skip_typedefs(achieved_type);
  direct_desired_type = (desired_type == NULL) ? (a_type_ptr)NULL
                                               : skip_typedefs(desired_type);
  if (desired_type != NULL &&
      !standalone_identical_types(direct_achieved_type, direct_desired_type)) {
    if (!constant->implicit_cast &&
        constant->variant.address.kind == (an_address_base_kind)abk_routine) {
      /* Function declarators don't get shared, so a pointer equality test
         doesn't work well.  We also can't use a routine like
         types_are_compatible to do a full test because those routines are
         not available in standalone utility programs.  It's okay to err on
         the side of putting out the cast, but in the most common case we
         can know that no cast is needed. */
    } else if (C_mode() && is_directly_variably_modified_type(desired_type)) {
      /* Eliminate an implicit cast to a variably-modified type.  We know
         the cast is implicit because explicit casts to directly
         variably-modified types are not folded into the constant. */
      final_cast_needed = FALSE;
      /* Cast to "(void *)" in case there were intervening casts on the
         original entity before the implicit cast to a variably-modified
         type. */
      output_optional_open_paren(&need_parens,
                                 &need_final_cast_close_paren, octl);
      octl->output_str("(void *)", octl);
    } else if (octl->processing_nontype_template_argument &&
               octl->gen_compilable_code
#if BACK_END_IS_CP_GEN_BE
               && !microsoft_dialect_is_generated_code_target
#endif /* BACK_END_IS_CP_GEN_BE */
                                                ) {
      /* MSVC accepts a cast in a non-type template argument; otherwise, a
         non-type template argument that is an address constant must not
         have a cast applied. */
    } else {
      /* The proper type couldn't be achieved with address operators, so we
         need a final cast to adjust the type.  One important category of cases
         this handles is cases that require just qualification adjustments. */
      final_cast_needed = TRUE;
    }  /* if */
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
  } else if (constant->kind == (a_constant_repr_kind)ck_address &&
             constant->variant.address.kind ==
                                          (an_address_base_kind)abk_variable &&
             is_array_type(direct_achieved_type) &&
             !has_unknown_specified_bound(direct_achieved_type) &&
             direct_achieved_type->variant.array.variant.number_of_elements
                                                                        != 0) {
#if BACK_END_IS_C_GEN_BE
    a_type_ptr elem_type = underlying_array_element_type(direct_achieved_type);
    if (get_top_level_type_qualifiers(elem_type) & TQ_CONST) {
      /* In some cases const qualifiers are removed from arrays to permit
         initialization by assignment.  We need to restore the qualifier by
         means of a cast so this address constant will have the proper type
         when used.  For example:

             struct A {
               const char ccarray[32];
             } a = {"constant string literal"};
             void f() {
               const char (&rconst)[32] = a.ccarray;
             }

         A::ccarray is generated as non-const, so the reference to a.array
         must be cast to "const char (*)[32]" to have the required type. */
      final_cast_needed = TRUE;
    } else
#endif /* BACK_END_IS_C_GEN_BE */
    /* Do not insert code here. */
    if (msvc_is_generated_code_target &&
        constant->variant.address.variant.variable->is_template_variable &&
        constant->variant.address.offset == 0) {
      /* The Microsoft compiler cannot complete the type of a static data
         member of a template instance in cases like the following:

             template<typename T> struct S {
               static int arr[];
             };
             template<typename T> int S<T>::arr[4];
             int (&r)[4] = &S<int>::arr;

         We must therefore add a cast to the final type in such cases (but not
         if the array type is still incomplete, as MSVC++ cannot handle a cast
         to a reference to an array of unknown bound). */
      final_cast_needed = TRUE;
    }  /* if */
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
  }  /* if */
  if (final_cast_needed) {
    /* Generate a final cast to the constant type. */
    output_optional_open_paren(&need_parens,
                               &need_final_cast_close_paren, octl);
    if (!form_lvalue) {
      /* Forming an address, not an lvalue. */
      form_general_cast(con_type, reinterpret_cast_needed, octl);
      if (reinterpret_cast_needed) need_reinterpret_cast_close_paren = TRUE;
      if (cast_to_nonpointer) {
        a_targ_alignment alignment;
        /* This is a case where the final type is a nonpointer.  See if an
           extra cast to unsigned long is needed. */
        if (is_integral_or_enum_type(con_type) &&
            con_type->size >= size_of_pointer_to(achieved_type, &alignment)) {
          /* Cast to large-enough integral type.  No extra cast needed. */
        } else {
          /* Anything else (e.g., cast to float).  Go by way of unsigned long
             first. */
          octl->output_str("(unsigned long)", octl);
        }  /* if */
      }  /* if */
    } else {
      a_type type_copy;
      /* When forming an lvalue (C++ only), generate a reference cast.
         Make a copy of the type so it can be changed to a reference type. */
      check_assertion(con_type->kind == (a_type_kind)tk_pointer);
      type_copy = *con_type;
      if (offset != 0 || il_header.source_language != sl_Cplusplus) {
        /* However, that's not possible when the offset is nonzero, or in
           C.  For those cases, use "*(type *)&x". */
        octl->output_str("*", octl);
        type_copy.variant.pointer.is_reference = FALSE;
        form_cast(&type_copy, octl);
        form_lvalue = FALSE;
        type_decay_used = FALSE;
      } else {
        /* Offset is zero, so use reference cast. */
        type_copy.variant.pointer.is_reference = TRUE;
        form_general_cast(&type_copy, reinterpret_cast_needed, octl);
        if (reinterpret_cast_needed) need_reinterpret_cast_close_paren = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (offset != 0) {
    /* The offset couldn't be handled with addressing operators, so it
       will be added in later.  Put parentheses around the offset
       computation. */
    output_optional_open_paren(&need_parens,
                               &need_offset_addition_close_paren, octl);
  }  /* if */
  if (need_char_star_cast) {
    /* Cast to "char *" because the scaling on the offset addition is wrong
       otherwise. */
    output_optional_open_paren(&need_parens,
                               &need_char_star_cast_close_paren, octl);
    octl->output_str("(char *)", octl);
  }  /* if */
  if (!form_lvalue && !octl->part_of_ud_literal) {
    /* Forming an address, not an lvalue. */
    if (is_reference_type(con_type) && !octl->gen_compilable_code) {
      /* Explicitly identify a reference type instead of using "&". */
      if (is_rvalue_reference_type(con_type)) {
        octl->output_str("rvalue reference to ", octl);
      } else {
        octl->output_str("reference to ", octl);
      }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (is_tracking_reference_type(con_type) &&
               !octl->gen_compilable_code) {
        octl->output_str("tracking reference to ", octl);
    } else if (is_handle_type(con_type) && !octl->gen_compilable_code) {
      octl->output_str("handle to ", octl);
    } else if (string_handle_case) {
      /* No "&" for implicit cast of string literal to handle. */
    } else if (constant->variant.address.kind ==
                                       (an_address_base_kind)abk_cli_typeid ||
               constant->variant.address.kind ==
                                        (an_address_base_kind)abk_cli_array) {
      /* No "&" for C++/CLI T::typeid or array constants. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else if (type_decay_used) {
      /* Using type decay to get a pointer. */
    } else {
      output_optional_open_paren(&need_parens, &need_ampersand_paren, octl);
      if (constant->kind == (a_constant_repr_kind)ck_address &&
	  constant->variant.address.kind == (an_address_base_kind)abk_label) {
	/* The address of a label is taken with "&&", rather than the
	   ordinary "&". */
	octl->output_str("&&", octl);
      } else {
	octl->output_str("&", octl);
      }  /* if */
    }  /* if */
  }  /* if */
  /* Generate code for the lvalue for the entity. */
  form_lvalue_for_addressed_entity(constant, desired_type,
                                   /*will_use_as_addr=*/!form_lvalue,
                                   /*base_entity_only=*/!formed_useful_lvalue,
                                   /*gen_output=*/TRUE,
                                   &achieved_type, &type_decay_used,
                                   &dummy_offset,
                                   &formed_useful_lvalue, octl);
  if (need_ampersand_paren) {
    octl->output_str(")", octl);
  }  /* if */
  output_optional_close_paren(need_char_star_cast_close_paren, octl);
  if (offset != 0) {
    /* Add in the (signed) offset. */
    if (offset >= 0) {
      octl->output_str(" + ", octl);
    } else {
      /* For negative numbers, the sign on the number will be the operator. */
      octl->output_str(" ", octl);
    }  /* if */
    form_num((a_host_large_integer)offset, octl);
    output_optional_close_paren(need_offset_addition_close_paren, octl);
  }  /* if */
  if (need_reinterpret_cast_close_paren) octl->output_str(")", octl);
  output_optional_close_paren(need_final_cast_close_paren, octl);
}  /* form_address_constant */


static a_boolean is_enum_constant_equivalent(a_constant_ptr constant,
                                             a_constant_ptr *equiv_constant)
/*
Given a constant for which is_enum_constant is TRUE, see if it is
an equivalent of a named enum constant.  If it is, set *equiv_constant to
point to the enum constant and return TRUE.  An equivalent of an enum constant
is a copy of the enum constant made to be used in an initializer (because
an initializer requires an unshared copy of the constant).  It can be put
out as the original enum constant.
*/
{
  a_boolean      is_enum_equiv = FALSE;
  a_type_ptr     con_type = constant->type, enum_type;
  a_constant_ptr con;

  *equiv_constant = NULL;
  con_type = skip_typerefs(con_type);
  /* Get the enum type. */
  if (il_header.source_language == sl_Cplusplus) {
    enum_type = con_type;
  } else {
    /* In C, enum constants have type int but an affiliated type that is the
       enum type. */
    enum_type = con_type->variant.integer.enum_info.affiliated_type;
  }  /* if */
  check_assertion(enum_type->kind == (a_type_kind)tk_integer &&
                  enum_type->variant.integer.enum_type);
  /* Go through the list of enum constants and compare each one to the
     constant we want. */
  for (con = enum_constants(enum_type); con != NULL; con = con->next) {
    /* Compare the constant on the list to the one we want. */
    if (cmp_integer_constants(con, constant) == 0) {
      /* Equal, so we found the constant we want. */
      is_enum_equiv = TRUE;
      *equiv_constant = con;
      break;
    }  /* if */
    /* Keep looking.  Note that there is no guarantee that the constants are
       in ascending order, so we can't stop on a too-large constant. */
  }  /* for */
  return is_enum_equiv;
}  /* is_enum_constant_equivalent */


void form_unknown_lvalue_constant(
                             a_constant_ptr                        constant,
                             an_il_to_str_output_control_block_ptr octl)
/*
Output the name indicated by a ck_template_param/tpck_unknown_function,
.../tpck_member, or .../tpck_template_ref constant.  Note that while the
constant may represent the address of the templated entity, whether to put out
the "&" operator is decided by the caller.  Do the output in the way described
by octl.
*/
{
  a_boolean      is_template = FALSE;
  a_constant_ptr con = constant;

  check_assertion(constant_is(con, ck_template_param));
  if (tpck_is(con, tpck_template_ref)) {
    is_template = TRUE;
    con = constant->variant.template_param.variant.template_ref.con;
  }  /* if */
  check_assertion(tpck_is(con, tpck_unknown_function) ||
                  tpck_is(con, tpck_member));
  if (tpck_is(con, tpck_unknown_function) &&
      con->variant.template_param.variant.unknown_function.conversion_type !=
                                                                        NULL) {
    /* The associated function is a conversion function.  Generate
       its name from the type. */
    check_assertion(con->source_corresp.is_class_member);
    if (con->source_corresp.parent_scope == NULL) {
      /* This should never happen with well-formed IL, but it can occur in
         error situations and is useful for debug output. */
      check_assertion(!octl->gen_compilable_code);
      octl->output_str("<null parent scope>::", octl);
    } else if ((!use_microsoft_form()
#if BACK_END_IS_CP_GEN_BE
                && !(gcc_is_generated_code_target &&
                     gnu_target_version_number >= 120100)
#endif /* BACK_END_IS_CP_GEN_BE */
                                                         ) ||
               constant->variant.template_param.is_qualified_name) {
      /* MSVC, as well as g++ in versions 12.1 and later, have a bug
         causing them not to accept a qualified name for a dependent
         conversion function in a member access expression, so we suppress
         the qualifier in that case.  (Pointer-to-member constants, where
         the qualified name is required, will have the is_qualified_name
         flag set to TRUE.) */
      form_class_qualifier(parent_class_of(con),
                           /*for_ptr_to_data_member=*/FALSE, octl);
    } else  {
      /* Notify subsequent processing that the context is a dependent
         conversion-type-id for which the usual qualification of the
         operator name was suppressed. */
      octl->name_is_dependent_conversion_type_id = TRUE;
    }  /* if */
    octl->output_str("operator ", octl);
    form_type(con->variant.template_param.variant.
                                              unknown_function.conversion_type,
              octl);
    octl->name_is_dependent_conversion_type_id = FALSE;
  } else {
    /* Normal case (not a conversion function). */
    a_boolean saved_force_qualified_name = octl->force_qualified_name;
    octl->force_qualified_name = con->variant.template_param.is_qualified_name;
    if (is_template && octl->output_template_name != NULL) {
      octl->output_template_name((char *)&con->source_corresp,
                                 iek_constant);
    } else if (con->source_corresp.member_of_unknown_base &&
               !con->source_corresp.qualified_unknown_base_member) {
      /* We're pretending that we found the member in a dependent base
         class and the original form of the reference was unqualified. */
      form_unqualified_name(&con->source_corresp, iek_constant, octl);
    } else {
      form_name(&con->source_corresp, iek_constant, octl);
    }  /* if */
    octl->force_qualified_name = saved_force_qualified_name;
  }  /* if */
  if (is_template) {
    /* Add the template arguments. */
    if (constant->variant.template_param.variant.template_ref.arg_list ==
                                                                        NULL) {
      /* A ck_template_param/tpck_template_ref constant is only created if
         there is an explicit template argument list, so put out an empty
         list if there were no arguments (form_template_args will put out
         nothing for an empty list). */
      octl->output_str("<>", octl);
    } else {
      form_template_args(constant->variant.template_param.variant.
                                                         template_ref.arg_list,
                         /*tpp=*/NULL, octl);
    }  /* if */
  }  /* if */
}  /* form_unknown_lvalue_constant */

#if FIXED_POINT_ALLOWED

static void form_fixed_point_constant(
                            a_fixed_point_value                    *value,
                            a_fixed_point_type_descr               *fxp_descr,
                            an_il_to_str_output_control_block_ptr  octl)
/*
Output the given fixed-point value with the proper suffix.
*/
{
  a_number_buffer str = fxp_to_string(fxp_descr, value);

  octl->output_str(str.as_temp_characters(), octl);
}  /* form_fixed_point_constant */

#endif /* FIXED_POINT_ALLOWED */

static void form_float_constant(
                           an_internal_float_value               *float_value,
                           a_float_kind                          fkind,
                           ARG_UNUSED an_expr_node_ptr           expr,
                           an_il_to_str_output_control_block_ptr octl)
/*
Output the given floating-point value with the proper suffix (or cast in
K&R/pcc mode) determined by fkind.  Typically a decimal string is generated,
but when generating compilable output, a hexadecimal string will be
generated (in configurations that support that).  When expr is non-NULL,
it represents a backing expression for the floating-point constant value.
*/
{
  a_const_char  *suffix = "";
  Small_string<64>
                buf;
  a_boolean pos_infinity, neg_infinity, not_a_number;
#if (BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE) && \
    BUILTIN_FUNCTIONS_ENABLED
  a_routine_ptr rp;
  an_expr_node_ptr arg;
  a_constant_ptr string_con = NULL;
  a_const_char  *gnu_builtin_suffix = "";
  int           max_exp = targ_dbl_max_exp;
  unsigned long gnu_targ_version = gnu_target_version_number;
#endif /* (BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE) && BUILTIN_... */

  if (!octl->gen_pcc_code) {
    /* Determine the suffix. */
    if (fkind == (a_float_kind)fk_float16) {
      suffix = "F16";
    } else if (fkind == (a_float_kind)fk_float) {
      suffix = "F";
#if (BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE) && \
    BUILTIN_FUNCTIONS_ENABLED
      gnu_builtin_suffix = "f";
      max_exp = targ_flt_max_exp;
#endif /* (BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE) && BUILTIN_... */
    } else if (fkind == fk_float32x) {
      suffix = "f32x";
#if (BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE) && \
    BUILTIN_FUNCTIONS_ENABLED
      gnu_builtin_suffix = "f32x";
      max_exp = targ_dbl_max_exp;
#endif /* (BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE) && BUILTIN_... */
    } else if (fkind == (a_float_kind)fk_long_double) {
      suffix = "L";
#if BACK_END_IS_C_GEN_BE
#if LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C
      /* No suffix when generating long double as double in the
         C-generating back end. */
      if (is_for_c_gen_be(octl)) suffix = "";
#endif /* LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C */
#endif /* BACK_END_IS_C_GEN_BE */
#if (BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE) && \
    BUILTIN_FUNCTIONS_ENABLED
      gnu_builtin_suffix = "l";
      max_exp = targ_ldbl_max_exp;
#endif /* (BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE) && BUILTIN_... */
    } else if (fkind == fk_float64x) {
      suffix = "f64x";
#if (BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE) && \
    BUILTIN_FUNCTIONS_ENABLED
      gnu_builtin_suffix = "f64x";
      max_exp = targ_flt80_max_exp;
#endif /* (BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE) && BUILTIN_... */
    } else if (fkind == (a_float_kind)fk_float80) {
      suffix = "W";
#if (BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE) && \
    BUILTIN_FUNCTIONS_ENABLED
      gnu_builtin_suffix = "w";
      max_exp = targ_flt80_max_exp;
#endif /* (BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE) && BUILTIN_... */
    } else if (fkind == (a_float_kind)fk_float128) {
      suffix = "Q";
#if (BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE) && \
    BUILTIN_FUNCTIONS_ENABLED
      gnu_builtin_suffix = "q";
      max_exp = targ_flt128_max_exp;
#endif /* (BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE) && BUILTIN_... */
    } else if (fkind == (a_float_kind)fk_std_bfloat16) {
      suffix = "bf16";
    } else if (fkind == (a_float_kind)fk_std_float16) {
      suffix = "f16";
    } else if (fkind == (a_float_kind)fk_std_float32) {
      suffix = "f32";
    } else if (fkind == (a_float_kind)fk_std_float64) {
      suffix = "f64";
    } else if (fkind == (a_float_kind)fk_std_float128) {
      suffix = "f128";
    }  /* if */
    if (octl->part_of_ud_literal) {
      /* Suppress the suffix on the numeric part of a user-defined literal
         lest it be considered part of the literal suffix. */
      suffix = "";
    }  /* if */
  } else {
    /* Generating K&R C.  Suffixes are not allowed. */
    /* Cast to float if type is float (by default it would be double). */
    if (fkind == (a_float_kind)fk_float) {
      octl->output_str("(float)", octl);
    }  /* if */
  }  /* if */
#if USE_HEX_FP_CONSTANTS_IN_GENERATED_CODE
  if (octl->gen_compilable_code && !octl->gen_pcc_code) {
    /* When generating code that will be compiled by a back end, don't
       bother to do the conversion from floating-point to decimal string
       conversion (which may lose precision -- and requires the opposite
       conversion by the back end).  Instead, use a hexadecimal
       floating-point string (when the back end supports that). */
    buf = fp_to_hex_constant_string(fkind, float_value,
                                    &pos_infinity, &neg_infinity,
                                    &not_a_number);
  } else
#endif /* USE_HEX_FP_CONSTANTS_IN_GENERATED_CODE */
  /* Do not insert code here. */
  {
    /* Generate a decimal string that best represents the floating-point
       value. */
    buf = fp_to_string(fkind, float_value,
                       &pos_infinity, &neg_infinity, &not_a_number);
  }  /* if */
  if (octl->gen_compilable_code &&
      (pos_infinity || neg_infinity || not_a_number)) {
    /* In compilable code, generate NaNs and Infinities as expressions.
       0.0/0.0 gives a NaN, 1.0/0.0 gives an infinity. */
    a_const_char *dividend;
    if (not_a_number) {
      dividend = "0.0";
    } else if (pos_infinity) {
      dividend = "1.0";
    } else {
      dividend = "-1.0";
    }  /* if */
#if (BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE) && \
    BUILTIN_FUNCTIONS_ENABLED
    if (not_a_number && expr != NULL) {
      /* Strip any compiler-generated casts from the backing expression before
         testing it below. */
      for (;;) {
        if (is_operation_node(expr) && expr->compiler_generated &&
           is_cast_operation_node(expr)) {
          /* A compiler generated cast. */
          expr = expr->variant.operation.operands;
        } else if (is_constant_node(expr) &&
                   node_constant(expr)->expr != NULL) {
          /* A constant with a backing expression (presumably introduced by
             an intervening constant-folding operation). */
          expr = node_constant(expr)->expr;
        } else {
          break;
        }  /* if */
      }  /* for */
    }  /* if */
    if (not_a_number &&
        (clang_is_generated_code_target ||
         (msvc_is_generated_code_target &&
          msvc_target_version_number >= 1900) ||
         (gcc_is_generated_code_target &&
          gnu_targ_version >= 30300)) &&
        expr != NULL &&
        is_operation_node(expr) &&
        node_operator_is(expr, eok_call) &&
        is_routine_node(expr->variant.operation.operands) &&
        (rp = expr->variant.operation.operands->variant.routine.ptr,
         arg = expr->variant.operation.operands->next,
         is_gnu_builtin_function(rp) &&
         arg != NULL &&
         is_constant_node(arg) &&
         node_constant(arg)->kind == (a_constant_repr_kind)ck_address &&
         (string_con = node_constant(arg)->variant.address.variant.constant,
          string_con->kind == (a_constant_repr_kind)ck_string))) {
      /* NaNs can have various bit patterns; to most accurately recreate
         this particular NaN pattern, see if the NaN constant has a backing
         expression that specifies a builtin call.  If so, use that call
         (and argument) to recreate it in the back end. */
      check_assertion(strlen(unmangled_or_fabricated_name_of(
                                                        &rp->source_corresp)) +
                      string_con->variant.string.length + 7 < sizeof(buf));
      buf.reset_to("(", unmangled_or_fabricated_name_of(&rp->source_corresp),
                   "(\"", string_con->variant.string.value, "\"))");
    } else if (msvc_is_generated_code_target) {
      /* MSVC++ gives an error on (x/0.0), so use a comma operator to
         fool it. */
      buf.reset_to("(", dividend, suffix, "/(0,0.0", suffix, "))");
    } else if (clang_is_generated_code_target ||
               (gcc_is_generated_code_target &&
                gnu_targ_version >= 30300)) /*lint !e845*/ {
      /* Use the builtin function. */
      if (not_a_number) {
        buf.reset_to("(__builtin_nan", gnu_builtin_suffix, "(\"\"))");
      } else {
        buf.reset_to("(", (neg_infinity ? "-" : ""),
                     "__builtin_huge_val", gnu_builtin_suffix, "())");
      }  /* if */
    } else if (gcc_is_generated_code_target && gnu_targ_version >= 29600 &&
               !not_a_number) {
      /* Use a large hexadecimal floating-point constant. */
      buf.reset_to("(", (neg_infinity ? "-" : ""),
                   "(__extension__ 0x1.0p", (2 * max_exp - 1), suffix, "))");
    } else
#endif /* (BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE) && BUILTIN_... */
    {
      buf.reset_to("(", dividend, suffix, "/", "0.0", suffix, ")");
    }  /* if */
    suffix = "";
  }  /* if */
  if (suffix[0] == '\0') {
    octl->output_str(buf.as_temp_characters(), octl);
  } else {
    output_partial_token_str(buf.as_temp_characters(), octl);
    output_partial_token_str(suffix, octl);
  }  /* if */
}  /* form_float_constant */


static void form_dynamic_init(a_dynamic_init_ptr                    dip,
                              an_il_to_str_output_control_block_ptr octl)
/*
Output the indicated dynamic initialization.  Do the output in the way
described by octl.  This is used only for non-compilable output (e.g.,
for debug output).
*/
{
  switch (dip->kind) {
    case dik_none:
      octl->output_str(error_text(ec_no_init), octl);
      break;
    case dik_zero:
      octl->output_str(error_text(ec_zero_init), octl);
      break;
    case dik_bitwise_copy:
      if (dip->variant.bitwise_copy.source != NULL) {
        octl->output_str(error_text(ec_bitwise_copy_of), octl);
        form_expression(dip->variant.bitwise_copy.source, octl);
      } else {
        octl->output_str(error_text(ec_bitwise_copy), octl);
      }  /* if */
      break;
    case dik_constant:
    case dik_nonconstant_aggregate:
    case dik_lambda:
      form_constant(dip->variant.constant.ptr, /*need_parens=*/TRUE, octl);
      break;
    case dik_class_result_via_ctor:
      octl->output_str(error_text(ec_class_result_via_ctor), octl);
      FALLTHROUGH
    case dik_expression:
      form_expression(dip->variant.expression, octl);
      break;
    case dik_constructor:
      octl->output_str(error_text(ec_constructor_call), octl);
      break;
    default:
      unexpected_condition_str("form_dynamic_init: bad kind");
  }  /* switch */
}  /* form_dynamic_init */


static inline a_boolean template_con_is_ampersand_operand(a_constant_ptr cp)
/*
Return TRUE if the specified constant (either a tpck_unknown_function or a
tpck_template_ref template parameter constant) is the operand of an "&"
operator.
*/
{
  check_assertion(constant_is(cp, ck_template_param));
  if (tpck_is(cp, tpck_template_ref)) {
    cp = cp->variant.template_param.variant.template_ref.con;
  }  /* if */
  return tpck_is(cp, tpck_unknown_function) &&
         cp->variant.template_param.has_address_of;
}  /* template_con_is_ampersand_operand */


static void form_expression(an_expr_node_ptr                      expr,
                            an_il_to_str_output_control_block_ptr octl)
/*
Output the indicated expression.  Do the output in the way described by
octl.  When there is a user-provided output_expression routine, it is
used.  Otherwise, this routine produces non-compilable output (e.g.,
for debug output), and it doesn't have to provide detailed information
on every expression.
*/
{
  a_boolean  saved_render_auto_deduction_typerefs =
                                         octl->render_auto_deduction_typerefs;

  /* Don't form "auto" or "decltype(auto)" instead of the deduced types when
     in expression contexts. */
  octl->render_auto_deduction_typerefs = FALSE;
  if (octl->output_expression != NULL) {
    /* Output the expression using a special routine. */
    octl->output_expression(expr, /*suppress_parens=*/FALSE);
  } else {
    /* No routine to do the expression output.  Do default
       non-compilable output. */
    check_assertion(!octl->gen_compilable_code);
    if (expr == NULL) {
      /* This can occur in array bound constants in which the expression
         involves a local variable and is thus represented via the local
         expr node reference mechanism instead of as a direct expression
         node. */
      octl->output_str(error_text(ec_null_expression), octl);
    } else switch (expr->kind) {
      case enk_error:
        octl->output_str(error_text(ec_quoted_error), octl);
        break;
      case enk_operation:
        { an_expr_node_ptr      operand = expr->variant.operation.operands;
          an_expr_operator_kind op = expr->variant.operation.kind;
          a_constant_ptr        con;
          if (op == (an_expr_operator_kind)eok_parens) {
            /* Parentheses. */
            octl->output_str("(", octl);
            form_expression(operand, octl);
            octl->output_str(")", octl);
#if DEBUG
          } else if (octl->debug_output) {
            a_const_char *op_str =
                               db_operator_names[expr->variant.operation.kind];
            octl->output_str("(", octl);
            if (is_call_node(expr)) {
              /* Calls. */
              form_expression(operand, octl);
              octl->output_str("(", octl);
              while ((operand = operand->next) != NULL) {
                form_expression(operand, octl);
                if (operand->next != NULL) octl->output_str(", ", octl);
              }  /* while */
              octl->output_str(")", octl);
            } else if (op == (an_expr_operator_kind)eok_subscript) {
              /* Subscripting. */
              form_expression(operand, octl);
              octl->output_str("[", octl);
              form_expression(operand->next, octl);
              octl->output_str("]", octl);
            } else if (is_cast_operation_node(expr)) {
              /* Casts. */
              a_const_char *new_style_op = NULL;
              a_type       ref_type;
              a_type_ptr   dest_type = expr->type;
              if (expr->variant.operation.is_reference_cast ||
                  op == (an_expr_operator_kind)eok_ref_cast ||
                  op == (an_expr_operator_kind)eok_ref_dynamic_cast) {
                /* A cast to a reference type. */
                destination_type_for_reference_cast(expr, &ref_type);
                dest_type = &ref_type;
              }  /* if */
              if (expr->is_static_cast) {
                new_style_op = "static_cast";
#if MICROSOFT_EXTENSIONS_ALLOWED
              } else if (expr->is_safe_cast) {
                new_style_op = "safe_cast";
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
              } else if (expr->variant.operation.is_const_cast) {
                new_style_op = "const_cast";
              } else if (expr->variant.operation.is_reinterpret_cast) {
                new_style_op = "reinterpret_cast";
              }  /* if */
              if (new_style_op != NULL) {
                octl->output_str(new_style_op, octl);
                octl->output_str("<", octl);
                form_type(dest_type, octl);
                octl->output_str(">(", octl);
                form_expression(operand, octl);
                octl->output_str(")", octl);
              } else {
                octl->output_str("(", octl);
                form_type(dest_type, octl);
                octl->output_str(")", octl);
                form_expression(operand, octl);
              }  /* if */
            } else if (operand->next == NULL) {
              /* Unary operators. */
              octl->output_str(op_str, octl);
              octl->output_str(" ", octl);
              form_expression(operand, octl);
            } else if (operand->next->next == NULL) {
              /* Binary operators. */
              form_expression(operand, octl);
              octl->output_str(" ", octl);
              octl->output_str(op_str, octl);
              octl->output_str(" ", octl);
              form_expression(operand->next, octl);
            } else {
              /* Other operators, e.g., "?".  Use generic form. */
              octl->output_str(op_str, octl);
              octl->output_str("(", octl);
              while (operand != NULL) {
                form_expression(operand, octl);
                if (operand->next != NULL) octl->output_str(", ", octl);
                operand = operand->next;
              }  /* while */
              octl->output_str(")", octl);
            }  /* if */
            octl->output_str(")", octl);
#endif /* DEBUG */
          } else if (op == (an_expr_operator_kind)eok_array_to_pointer ||
                     op == (an_expr_operator_kind)eok_lvalue_adjust ||
                     op == (an_expr_operator_kind)eok_class_rvalue_adjust ||
                     op == (an_expr_operator_kind)eok_unbox_lvalue ||
                     (expr->compiler_generated &&
                      is_cast_operation_node(expr))) {
            /* Elide certain implicit operations. */
            form_expression(operand, octl);
          } else if (op == (an_expr_operator_kind)eok_address_of &&
                     is_operation_node(operand) &&
                     node_operator_is(operand, eok_lvalue) &&
                     is_constant_node(operand->variant.operation.operands) &&
                     (con = node_constant(operand->variant.operation.operands))
                           ->kind == (a_constant_repr_kind)ck_template_param &&
                     (tpck_is(con, tpck_unknown_function) ||
                      tpck_is(con, tpck_member) ||
                      tpck_is(con, tpck_template_ref))) {
            octl->output_str("&", octl);
            form_unknown_lvalue_constant(con, octl);
          } else {
            octl->output_str(error_text(ec_quoted_expression), octl);
          }  /* if */
        }
        break;
      case enk_constant:
        form_constant(node_constant(expr), /*need_parens=*/TRUE, octl);
        break;
      case enk_variable:
        form_name(&node_variable(expr)->source_corresp,
                  (an_il_entry_kind)iek_variable, octl);
        break;
      case enk_routine:
        { a_routine_ptr  rp = node_routine(expr);
          if (rp == NULL) {
            check_assertion(!octl->gen_compilable_code);
            octl->output_str(error_text(ec_null_routine), octl);
          } else {
            form_name(&rp->source_corresp, (an_il_entry_kind)iek_routine,
                      octl);
          }  /* if */
        }
        break;
      case enk_field:
        form_name(&node_field(expr)->source_corresp,
                  (an_il_entry_kind)iek_field, octl);
        break;
      case enk_temp_init:
#if DEBUG
        if (octl->debug_output) {
          octl->output_str("temp(", octl);
          form_type(expr->type, octl);
          octl->output_str("):(", octl);
          form_dynamic_init(expr->variant.init.dynamic_init, octl);
          octl->output_str(")", octl);
        } else
#endif /* DEBUG */
        /* Do not insert code here */
        { a_dynamic_init_ptr  dip = expr->variant.init.dynamic_init;
          if (has_name_before_mangling(expr->type) &&
              (dip->kind == (a_dynamic_init_kind)dik_none ||
               dip->kind == (a_dynamic_init_kind)dik_zero)) {
            form_type(expr->type, octl);
            octl->output_str("()", octl);
          } else {
            octl->output_str(error_text(ec_quoted_expression), octl);
          }  /* if */
        }  /* if */
        break;
      case enk_type_operand:
        if (expr->variant.type_operand.type != NULL) {
          form_type(expr->variant.type_operand.type, octl);
        } else {
          octl->output_str(error_text(ec_quoted_default), octl);
        }  /* if */
        break;
      case enk_builtin_operation:
#if DEBUG
        if (octl->debug_output) {
          an_expr_node_ptr  operand = expr->variant.builtin_operation.operands;
          a_const_char      *op_str = builtin_operation_names[
                                         expr->variant.builtin_operation.kind];
          octl->output_str(op_str, octl);
          octl->output_str("(", octl);
          while (operand != NULL) {
            form_expression(operand, octl);
            if (operand->next != NULL) octl->output_str(", ", octl);
            operand = operand->next;
          }  /* while */
          octl->output_str(")", octl);
        } else
#endif /* DEBUG */
        /* Do not insert code here. */
        {
          octl->output_str(error_text(ec_quoted_expression), octl);
        }  /* if */
        break;
      case enk_param_ref:
        if (expr->variant.param_ref.param_num == 0) {
          /* A zero parameter number indicates "this". */
          octl->output_str("this", octl);
        } else {
          /* Describe a parameter for non-compilable output.  If the reference
             is to the innermost function parameter list or to a parameter list
             preceding a trailing return type (presumably the most common
             cases), just give the parameter number.  Otherwise, also indicate
             how many "levels up" the function prototype scope is. */
          octl->output_str("<", octl);
          octl->output_str(error_text(ec_parameter_number), octl);
          form_unsigned_num((a_host_large_unsigned)
                                            expr->variant.param_ref.param_num,
                            octl);
          if (expr->variant.param_ref.levels_up == 2) {
            octl->output_str(error_text(ec_one_level_up), octl);
          } else if (expr->variant.param_ref.levels_up > 2) {
            octl->output_str(" (", octl);
            form_unsigned_num((a_host_large_unsigned)
                                          expr->variant.param_ref.levels_up-1,
                               octl);
            octl->output_str(error_text(ec_levels_up), octl);
            octl->output_str(")", octl);
          }  /* if */
          octl->output_str(">", octl);
        }  /* if */
        break;
      case enk_braced_init_list:
        octl->output_str("{ ... }", octl);
        break;
      case enk_c11_generic:
#if DEBUG
        if (octl->debug_output) {
          an_expr_node_ptr  operand = expr->variant.c11_generic.operands;
          octl->output_str("_Generic", octl);
          octl->output_str("(", octl);
          form_expression(operand, octl);
          octl->output_str(", ", octl);
          while (operand != NULL) {
            form_expression(operand, octl);
            octl->output_str(": ", octl);
            operand = operand->next;
            if (operand != NULL) {
              form_expression(operand, octl);
              operand = operand->next;
              if (operand != NULL) octl->output_str(", ", octl);
            }  /* if */
          }  /* while */
          octl->output_str(")", octl);
        } else
#endif /* DEBUG */
        /* Do not insert code here. */
        {
          octl->output_str(error_text(ec_quoted_expression), octl);
        }  /* if */
        break;
      case enk_concept_id:
        /* Output the concept-id (but abbreviate its arguments if any). */
        { a_template_arg_ptr  tap = expr->variant.concept_id.args;
          form_name(&expr->variant.concept_id.concept_template->source_corresp,
                    (an_il_entry_kind)iek_template, octl);
          if (tap != NULL) {
            octl->output_str("<...>", octl);
          }  /* if */
        }
        break;
      case enk_pack_index:
        /* C++26 pack index expression: pack...[index] */
        form_expression(expr->variant.pack_index.expr, octl);
        octl->output_str("...[", octl);
        form_expression(expr->variant.pack_index.index_expr, octl);
        octl->output_str("]", octl);
        break;
      default:
        octl->output_str(error_text(ec_quoted_expression), octl);
        break;
    }  /* switch */
    if (expr != NULL && expr->is_pack_expansion) {
      octl->output_str("...", octl);
    }  /* if */
  }  /* if */
  octl->render_auto_deduction_typerefs = saved_render_auto_deduction_typerefs;
}  /* form_expression */

#if DEBUG

void db_abbr_expr(an_expr_node_ptr                       expr,
                  an_il_to_str_output_control_block_ptr  octl)
/*
Output the given expression in compact form.  This is only for use by the
debug output routines.
*/
{
  form_expression(expr, octl);
}  /* db_abbr_expr */

#endif /* DEBUG */


static void form_dynamic_init_constant(
                                a_constant_ptr                        constant,
                                an_il_to_str_output_control_block_ptr octl)
/*
Output the indicated ck_dynamic_init constant.  Do the output in the way
described by octl.  This is used only for non-compilable output (e.g.,
for debug output).
*/
{
  a_dynamic_init_ptr dip;

  check_assertion(!octl->gen_compilable_code &&
                  constant->kind == (a_constant_repr_kind)ck_dynamic_init);
  octl->output_str(error_text(ec_dynamic_init), octl);
  dip = constant->variant.dynamic_init.ptr;
  form_dynamic_init(dip, octl);
}  /* form_dynamic_init_constant */


static void form_reflection_prefix(an_error_code                          ec,
                                   an_il_to_str_output_control_block_ptr  octl)
/*
This function should be called from form_reflection only.

If generating compilable code, render "(^^" to start a reflection expression.
Otherwise, render the text for the given code, followed by a space.  If we're
generating compilable code, turn off the "compilable code" flag to ensure that
form_name can be called.  The caller will restore the flag.  Nothing is
rendered in the display form of a reflection, which names only the entity.
*/
{
  if (octl->reflection_display_form) {
    /* No prefix naming the kind of entity is wanted. */
  } else if (!octl->gen_compilable_code) {
    octl->output_str(error_text(ec), octl);
    octl->output_str(" ", octl);
  } else {
    /* FIXME reflection: Turning off gen_compilable_code here is a
       temporary kludge. */
    octl->output_str("(^^", octl);
    octl->gen_compilable_code = FALSE;
  }  /* if */
}  /* form_reflection_prefix */


void form_reflection(a_reflection_value                     rv,
                     an_il_to_str_output_control_block_ptr  octl)
/*
Render the entity designated by the given reflection.  When octl requests the
display form (see reflection_display_form), the rendering is the one
std::meta::display_string_of specifies: A type appears as its type spelling, a
function as its full signature, and any other named entity as its qualified
name, with no word naming the kind of entity.
*/
{
  a_boolean  saved_gen_compilable_code = octl->gen_compilable_code,
             saved_suppress_typedefs = octl->suppress_typedefs;

  if (octl->gen_compilable_code) {
    if (octl->cpp_generating_back_end) {
      /* Reflection values sometimes leak into the C++-generating back end,
         but those values are not actually used.  Render a null reflection
         value. */
      octl->output_str("(decltype(^^0){})", octl);
      goto done;
    } else if (octl->c_generating_back_end) {
      unexpected_condition();
    }  /* if */
    /* We may still get here when rendering strings from token caches. */
  }  /* if */
  if (rv.entity.kind != iek_type ||
      !type_is_typedef((a_type*)rv.entity.ptr)) {
    octl->suppress_typedefs = TRUE;
  }  /* if */
  strip_template_arg(&rv);
  switch (rv.entity.kind) {
    case iek_none:
      if (!octl->gen_compilable_code) {
        octl->output_str(error_text(ec_null_reflection), octl);
      } else {
        octl->output_str("(decltype(^^void){}", octl);
      }  /* if */
      break;
    case iek_base_class:
      if (octl->reflection_display_form) {
        form_type(((a_base_class*)rv.entity.ptr)->type, octl);
      } else if (!octl->gen_compilable_code) {
        a_base_class  *bcp = (a_base_class*)rv.entity.ptr;
        form_type(bcp->type, octl);
        octl->output_str(" ", octl);
        octl->output_str(error_text(ec_in), octl);
        octl->output_str(" ", octl);
        form_type(bcp->derived_class, octl);
      } else {
	unexpected_condition();
      }  /* if */
      break;
    case iek_type:
      form_reflection_prefix(ec_type, octl);
      form_type((a_type*)rv.entity.ptr, octl);
      break;
    case iek_constant:
      { a_source_correspondence_ptr  scp;
        scp = octl->reflection_display_form ?
	                            source_corresp_for_reflection(&rv) : NULL;
        if (scp != NULL && scp->name != NULL) {
          /* A named constant, such as an enumerator, displays as its
             qualified name rather than as its value. */
          form_name(scp, rv.entity.kind, octl);
        } else {
          if (octl->gen_compilable_code) {
            form_reflection_prefix(ec_no_error, octl);
          }  /* if */
          form_constant((a_constant*)rv.entity.ptr, /*need_parens=*/FALSE,
                        octl);
        }  /* if */
      }
      break;
    case iek_expr_node:
      if (octl->gen_compilable_code) {
        form_reflection_prefix(ec_no_error, octl);
      }  /* if */
      octl->output_str("(", octl);
      form_expression((an_expr_node*)rv.entity.ptr,  octl);
      octl->output_str(")", octl);
      break;
    case iek_field:
      form_reflection_prefix(ec_field, octl);
      form_name(&((a_field*)rv.entity.ptr)->source_corresp, iek_field, octl);
      break;
    case iek_routine:
      { a_routine_ptr  rp = (a_routine*)rv.entity.ptr;
        a_type_ptr     ftp = skip_typerefs(rp->type);
        form_reflection_prefix(ec_function, octl);
        if (octl->reflection_display_form && type_is(ftp, tk_routine)) {
          /* The display form of a function is its whole signature, which is
             rendered as a declaration of its name: the declarator surrounds
             the name, as in "int (*f())()" for a function returning a pointer
             to a function. */
          form_type_first_part_simple(ftp, /*under_lhs_declarator=*/FALSE,
                                      /*need_trailing_space=*/FALSE, octl);
          form_name(&rp->source_corresp, iek_routine, octl);
          form_type_second_part_simple(ftp, /*under_lhs_declarator=*/FALSE,
                                       octl);
        } else {
          form_name(&rp->source_corresp, iek_routine, octl);
        }  /* if */
      }
      break;
    case iek_variable:
      form_reflection_prefix(ec_variable, octl);
      form_name(&((a_variable*)rv.entity.ptr)->source_corresp, iek_variable,
                octl);
      break;
    case iek_template:
      form_reflection_prefix(ec_template, octl);
      form_name(&((a_template*)rv.entity.ptr)->source_corresp, iek_template,
                octl);
      break;
    case iek_namespace:
      form_reflection_prefix(ec_namespace_alias, octl);
      form_name(&((a_namespace*)rv.entity.ptr)->source_corresp,
                iek_namespace, octl);
      break;
    case iek_scope:
      { a_scope*  scope = (a_scope*)rv.entity.ptr;
        if (scope_is(scope, sck_namespace) ||
            scope_is(scope, sck_namespace_extension)) {
          form_reflection_prefix(ec_namespace, octl);
          form_name(&scope->variant.assoc_namespace->source_corresp,
                    iek_namespace, octl);
          break;
        }  /* if */
      }
      FALLTHROUGH
    default:
      { a_source_correspondence_ptr
                    scp = octl->reflection_display_form
                            ? source_corresp_for_reflection(&rv) : NULL;
        if (scp != NULL && scp->name != NULL) {
          /* Named constants such as enumerators: render the qualified name
             rather than the underlying value.  Pass the entity's own kind so
             form_unqualified_name does not misinterpret the entry (only the
             type/variable/routine kinds are treated specially there). */
          form_name(scp, rv.entity.kind, octl);
        } else if (!octl->gen_compilable_code) {
          octl->output_str(error_text(ec_unspecified_reflection), octl);
        } else {
          unexpected_condition();
        }  /* if */
      }
      break;
  }  /* switch */
  octl->gen_compilable_code = saved_gen_compilable_code;
  octl->suppress_typedefs = saved_suppress_typedefs;
  if (octl->gen_compilable_code) {
    octl->output_str(")", octl);
  }  /* if */
done:;
}  /* form_reflection */


void form_constant(a_constant_ptr                        constant,
                   a_boolean                             need_parens,
                   an_il_to_str_output_control_block_ptr octl)
/*
Output the indicated constant.  If an expression node corresponding to the
operation that resulted in the constant is available, output the expression
instead of just the result of the operation.  If need_parens is TRUE,
parentheses are placed around the constant if there's any possibility of
precedence confusion.  Do the output in the way described by octl.
*/
{
  a_constant_repr_kind kind = constant->kind;
  a_type_ptr           con_type = NULL, orig_type;
  a_boolean            need_cast_close_paren = FALSE, is_enum;
  a_boolean            need_reinterpret_cast = FALSE;
  a_constant_ptr       equiv_constant;
  a_boolean            cast_already_put_out = FALSE;
  a_boolean            is_undefined_opaque_enum = FALSE;
  an_expr_node_ptr     expr;

  orig_type = constant->type;
  /* Watch out for constants (like ck_init_repeat) that have no type. */
  if (orig_type == NULL) {
#if CHECKING
    if (kind != (a_constant_repr_kind)ck_init_repeat &&
	kind != (a_constant_repr_kind)ck_designator) {
#if DEBUG
      if (octl->debug_output) {
        octl->output_str("**NULL-CONSTANT-TYPE**", octl);
      } else
#endif /* DEBUG */
      /* Do not insert code here.  This is the else of an "if". */
      {
        unexpected_condition_str("form_constant: constant with null type");
      }
    }  /* if */
#endif /* CHECKING */
  } else if (constant_should_be_put_out_as_expr(constant) &&
             !is_for_c_gen_be(octl) &&
             octl->output_expression != NULL &&
             (expr = expr_node_from_constant(constant)) != NULL &&
             !(octl->expr_is_unusable != NULL &&
               octl->expr_is_unusable(expr))) {
    /* A usable expression was recorded for this constant.  Output that
       expression rather than the folded constant. */
    octl->output_expression(expr, !need_parens);
    goto done;
  } else {
    con_type = skip_typerefs(orig_type);
    /* See if we need a cast to the constant result type. */
    if (kind == (a_constant_repr_kind)ck_address ||
        kind == (a_constant_repr_kind)ck_ptr_to_member) {
      /* Don't do this here for address constants or pointer-to-member
         constants (they're handled in the subroutines). */
    } else {
      /* If the constant is implicitly cast to another type, prefix the
         constant with an explicit cast. */
      a_boolean need_cast = FALSE;
#if BACK_END_IS_CP_GEN_BE
      if (octl->gen_compilable_code && is_enum_type(con_type) &&
          !constant_is(constant, ck_template_param) &&
          con_type->has_been_declared && !con_type->has_been_defined &&
          !con_type->variant.integer.originally_unnamed) {
        /* The type is an opaque enumeration whose enumerators have not yet
           been defined, so even if the value is the same as an enumerator
           of the type, we must put it out as a cast of a numeric constant
           rather than as the name of that enumerator.  (This does not
           apply when the constant is a non-type template argument of the
           enumeration's type; in that case, an explicit cast can result in
           uncompilable code.) */
        need_cast = TRUE;
        is_undefined_opaque_enum = TRUE;
      } else
#endif /* BACK_END_IS_CP_GEN_BE */
      /* Do not insert code here. */
      if (constant->is_reinterpret_cast && !is_for_c_gen_be(octl)) {
        /* The source form used reinterpret_cast, so a cast is needed. */
        need_cast = TRUE;
        need_reinterpret_cast = TRUE;
      } else if (constant->explicit_cast_applied ||
                 constant->is_compound_literal) {
        /* The source form involved an explicit cast. */
        if (kind == (a_constant_repr_kind)ck_string) {
          /* This was a compound literal in the source, but the generated
             C code should just have the string form. */
          need_cast = !is_for_c_gen_be(octl);
#if GNU_VECTOR_TYPES_ALLOWED
        } else if (kind == (a_constant_repr_kind)ck_aggregate &&
                   octl->suppress_cast_on_vector_const &&
                   is_vector_type(con_type)) {
          need_cast = FALSE;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
        } else {
          need_cast = TRUE;
        }  /* if */
      } else if (constant->implicit_cast) {
        if (
#if DEBUG
            (octl->debug_output && !is_nullptr_type(con_type)) ||
#endif /* DEBUG */
            is_for_c_gen_be(octl)) {
#if GCC_BUILTIN_VARARGS
          a_type_ptr tp;
          a_boolean  builtin_va_list = FALSE;
          for (tp = orig_type;
               !builtin_va_list && tp != NULL &&
                                           tp->kind == (a_type_kind)tk_typeref;
               tp = tp->variant.typeref.type) {
            builtin_va_list = tp->is_builtin_va_list;
          }  /* for */
          if (builtin_va_list) {
            /* We must avoid casting to __builtin_va_list because it might
               be an array type on some systems. */
          } else
#endif /* GCC_BUILTIN_VARARGS */
          /* Do not insert code here. */
          {
            /* Give full information about implicit casts when generating
               debug output and in the C generating back end (for the
               latter, because casts added by IL lowering are "implicit"
               but they need to be put out). */
            need_cast = TRUE;
            if (octl->gen_compilable_code && C_mode() &&
                is_directly_variably_modified_type(orig_type)) {
              /* Casts to directly variably-modified types must be
                 suppressed.  That's possible because they are folded into
                 the constant only if they are implicit.  However, we must
                 still deal with the fact that the constant may have been
                 explicitly cast to some other pointer type before it was
                 cast to the variably-modified type. */
              check_assertion(is_pointer_type(orig_type));
              need_cast = FALSE;
              /* For null pointer constants, the extra cast to "void *" is
                 not necessary. */
              if (constant->kind != (a_constant_repr_kind)ck_integer ||
                  cmplit_integer_constant(constant,
                                          (a_host_large_integer)0) != 0) {
                output_optional_open_paren(&need_parens,
                                           &need_cast_close_paren, octl);
                octl->output_str("(void *)", octl);
                cast_already_put_out = TRUE;
              }  /* if */
            }  /* if */
          }  /* if */
        } else if (is_pointer_type(con_type) &&
                   kind == (a_constant_repr_kind)ck_integer &&
                   cmplit_integer_constant(constant,
                                           (a_host_large_integer)0) == 0) {
          /* Always put casts on null pointers, because when "x" is
             changed to "x != (T *)0" because it appears in a condition
             context, you don't want to put out "x != 0" which might be
             ambiguous when x has a class type. */
          need_cast = TRUE;
        }  /* if */
#if GNU_VECTOR_TYPES_ALLOWED
      } else if (is_vector_type(con_type)) {
        /* Vector constants must be generated as a compound literal (e.g.,
           (VF2){ 1.0, 2.0 }), except in some cases where they should just
           be rendered as a braced initializer (e.g., { 1, 2 }). */
        need_cast = !octl->suppress_cast_on_vector_const;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
      }  /* if */
      if (need_cast) {
        /* Prefix the constant with an explicit cast. */
        output_optional_open_paren(&need_parens, &need_cast_close_paren, octl);
        if (constant->kind == (a_constant_repr_kind)ck_aggregate &&
            !constant->is_compound_literal &&
            !is_for_c_gen_be(octl)) {
          /* This must have been a cast like T{}, so just put out the type
             name (without any compiler-generated cv-qualifiers) here; the
             ck_aggregate output will provide the braces. */
          form_type(skip_typerefs_not_typedefs_or_type_operators(orig_type),
                    octl);
        } else {
          /* Put out either a reintepret_cast or a C-style cast. */
          form_general_cast(orig_type, need_reinterpret_cast, octl);
        }  /* if */
        cast_already_put_out = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  switch (kind) {
    case ck_error:
      check_assertion_str(!octl->gen_compilable_code,
                          "form_constant: error constant");
      octl->output_str(error_text(ec_error_constant), octl);
      break;
#if UPC_EXTENSIONS_ALLOWED
    case ck_upc_mythread:
      octl->output_str("MYTHREAD", octl);
      break;
    case ck_upc_threads:
#endif /* UPC_EXTENSIONS_ALLOWED */
    case ck_integer:
      /* See if the constant is an enum constant, but don't emit enum
         constants when generating K&R C from the C-generating back end. */
      is_enum = !(is_for_c_gen_be(octl) && octl->gen_pcc_code) &&
                is_enum_constant(constant) && !is_undefined_opaque_enum;
      if (is_enum && has_name(constant) &&
          !(is_for_c_gen_be(octl) &&
            !constant->is_named_constant_definition)) {
        /* A named enum constant.  The original constant entry used to
           represent the enumerator constant declaration can always just be
           rendered.  However, for copies of that entry (used in expression
           contexts), only the unmangled name is available, which may not
           be appropriate when generating code in the C-generating back
           end. */
        a_scope_ptr orig_parent_scope = constant->source_corresp.parent_scope;
        a_boolean   orig_class_member =
                                      constant->source_corresp.is_class_member;
        if (orig_parent_scope == NULL) {
          /* The parent scope can be lost when copying an enumerator
             constant.  Temporarily set the enumerator's parent scope as
             determined by its type (the enumeration's parent scope or, for
             a scoped enumeration, the associated scope of the type itself)
             so that the name's qualification will be correct. */
          a_type_ptr tp = skip_typerefs(constant->type);
          if (tp->variant.integer.is_scoped_enum) {
            constant->source_corresp.parent_scope =
                                     tp->variant.integer.enum_info.assoc_scope;
          } else {
            constant->source_corresp.parent_scope =
                                               tp->source_corresp.parent_scope;
            constant->source_corresp.is_class_member =
                                            tp->source_corresp.is_class_member;
          }  /* if */
        }  /* if */
        form_name(&constant->source_corresp, iek_constant, octl);
        /* Restore the enumerator's parent scope information in case it was
           overwritten above. */
        constant->source_corresp.parent_scope = orig_parent_scope;
        constant->source_corresp.is_class_member = orig_class_member;
      } else if (is_enum && il_header.source_language == sl_Cplusplus &&
#if DEBUG
                 !octl->debug_output &&
#endif /* DEBUG */
                 is_enum_constant_equivalent(constant, &equiv_constant)) {
        /* The equivalent of an enum constant (an enum constant used in
           an initializer; it's a nonshared constant with the same value as
           the named enumeration constant). */
        check_assertion(equiv_constant != NULL);
        form_name(&equiv_constant->source_corresp, iek_constant, octl);
#if GNU_EXTENSIONS_ALLOWED
      } else if (!is_for_c_gen_be(octl) && constant->null_keyword) {
        /* The GNU C++ __null keyword. */
        octl->output_str("__null", octl);
#endif /* GNU_EXTENSIONS_ALLOWED */
      } else if (!is_for_c_gen_be(octl) &&
                 (is_nullptr_type(con_type) || constant->nullptr_keyword)) {
        /* The C++ and C++/CLI nullptr keyword.  (We test the type in
           addition to checking constant->nullptr_keyword in order to
           handle the case of a call to a constexpr function resulting in a
           constant for which the call was not kept as the constant's
           backing expression, e.g., in a template argument.  Both tests
           are necessary as sometimes the type of the constant will have
           been adjusted to something other than std:nullptr_t but the
           constant should still be put out as the nullptr keyword.) */
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
        if (clang_is_generated_code_target && octl->gen_compilable_code) {
          /* Clang accepts "__nullptr" in all C++ modes. */
          octl->output_str("__nullptr", octl);
        } else
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
        /* Do not insert code here. */
        {
          octl->output_str("nullptr", octl);
        }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (!is_for_c_gen_be(octl) && constant->native_nullptr_keyword) {
        /* The Microsoft __nullptr keyword. */
        octl->output_str("__nullptr", octl);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* coverity[var_deref_model] */
      } else if (!is_for_c_gen_be(octl) &&
                 il_header.source_language == sl_Cplusplus &&
                 is_bool_type(con_type)) {
        /* A bool constant. */
        octl->output_str((char *)(
               cmplit_integer_constant(constant,
                            (a_host_large_integer)0) != 0 ? "true" : "false"),
                         octl);
        /* coverity[var_deref_model] */
      } else if (!is_for_c_gen_be(octl) &&
                 il_header.source_language == sl_Cplusplus &&
                 is_character_type(con_type)) {
        /* In C++, character constants have char type. */
        a_boolean       ovflo, need_char_cast_close_paren = FALSE;
        /* coverity[var_deref_op] */
        an_integer_kind ikind = con_type->variant.integer.int_kind;
        /* Use a cast if the constant is signed or unsigned, e.g.,
           (unsigned char)'a'. */
        if (ikind == (an_integer_kind)ik_signed_char ||
            ikind == (an_integer_kind)ik_unsigned_char) {
          output_optional_open_paren(&need_parens,
                                     &need_char_cast_close_paren,
                                     octl);
          form_cast(orig_type, octl);
        }  /* if */
        output_partial_token_str("'", octl);
        /* Coverity complains because form_char passes the character value
           to isprint, but it casts it to unsigned char; the complaint is
           spurious. */
        /* coverity[negative_returns] */  /* Coverity bug. */
        (void)form_char((char)value_of_integer_constant(constant, &ovflo),
                        octl);
        output_partial_token_str("'", octl);
        output_optional_close_paren(need_char_cast_close_paren, octl);
        /* coverity[var_deref_op] */
      } else if (!is_for_c_gen_be(octl) &&
                 con_type->kind == (a_type_kind)tk_integer &&
                 (constant->character_kind == (a_character_kind)chk_char8_t ||
                  !is_normal_character_kind(constant->character_kind))) {
        /* A wide character literal (wchar_t, char16_t, or char32_t) or a
           char8_t literal. */
        a_boolean    ovflo;
        a_const_char *prefix = NULL;
        switch (constant->character_kind) {
          case chk_wchar_t:   prefix = "L'";     break;
          case chk_char8_t:   prefix = "u8'";    break;
          case chk_char16_t:  prefix = "u'";     break;
          case chk_char32_t:  prefix = "U'";     break;
          default:            unexpected_condition();
        }  /* switch */
        output_partial_token_str(prefix, octl);
        (void)form_wide_char(
                    (unsigned long)unsigned_value_of_integer_constant(constant,
                                                                      &ovflo),
                    octl);
        output_partial_token_str("'", octl);
#if GNU_VECTOR_TYPES_ALLOWED
      } else if (type_is(con_type, tk_mfp8)) {
        /* Currently, the only constant value of type __mfp8 generated in the
           IL is "zero", which can be rendered as "__mfp8()". */
        an_integer_value  zero;
        set_integer_value(&zero, (a_host_large_integer)0);
        if (cmp_integer_values(&constant->variant.integer_value,
                               /*op_1_signed=*/FALSE,
                               &zero, /*op_2_signed=*/FALSE) == 0) {
          octl->output_str("__mfp8()", octl);
        } else {
          unexpected_condition();
        }  /* if */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
      } else {
        /* A normal integer constant. */
        form_integer_constant(constant, cast_already_put_out,
                              need_parens, octl);
      }  /* if */
      break;
#if FIXED_POINT_ALLOWED
    case ck_fixed_point:
      /* Fixed-point constant. */
      /* Put parentheses around the constant in case it's negative. */
      check_assertion(is_fixed_point_type(constant->type));
      octl->output_str("(", octl);
      /* coverity[var_deref_model] */
      form_fixed_point_constant(&constant->variant.fixed_point_value,
                                &con_type->variant.fixed_point,
                                octl);
      octl->output_str(")", octl);
      break;
#endif /* FIXED_POINT_ALLOWED */
    case ck_string:
      /* String constant. */
      { a_targ_size_t a;
        char          ch;
        unsigned long wc;
        a_const_char  *str = constant->variant.string.value, *prefix = NULL;
        a_targ_size_t len = constant->variant.string.length;
        int           out_len = 0;
        a_character_kind
                      character_kind =
                         enum_cast<a_character_kind>(constant->character_kind);
#if BACK_END_IS_C_GEN_BE
        if (is_for_c_gen_be(octl) && constant->assoc_var != NULL &&
            !is_normal_character_kind(character_kind)) {
          /* The C-generating back end transforms wide string literals: it
             creates a variable initialized with the string value and then
             uses the variable instead of the string.  This ensures proper
             alignment for the string.  The temp name for the variable is
             derived from the address of the constant; there is no actual
             variable entry. */
          output_temp_name((char *)constant, octl);
        } else
#endif /* BACK_END_IS_C_GEN_BE */
        /* Do not insert code here.  This is the "else" of an "if". */
        {
          if (constant->is_compound_literal && !is_for_c_gen_be(octl)) {
            /* The string was originally a compound literal and, except in
               generated C code, should be put out that way. */
            octl->output_str("{", octl);
          }  /* if */
          if (!is_normal_character_kind(character_kind) ||
              character_kind == (a_character_kind)chk_char8_t) {
            /* A string literal with a prefix, e.g., L"abc" or U"xyz". */
            /* The processing here must invert the processing done in
               conv_single_wide_char.  Do something that's right for the
               default (simple-minded) implementation, which maps one input
               character to one wide character. */
            a_targ_size_t char_size = character_size[character_kind];
            switch (character_kind) {
              case chk_wchar_t:   prefix = "L\"";     break;
              case chk_char8_t:   prefix = "u8\"";    break;
              case chk_char16_t:  prefix = "u\"";     break;
              case chk_char32_t:  prefix = "U\"";     break;
              default:            unexpected_condition();
            }  /* switch */
            output_partial_token_str(prefix, octl);
            for (a = 0; a < len; a += char_size) {
              /* When generating output for humans to read, abbreviate
                 long strings. */
              if (!octl->gen_compilable_code && a > 20*char_size &&
                  len > 25*char_size) {
                output_partial_token_str("...", octl);
                break;
              } else if (out_len >= 128 && octl->gen_compilable_code &&
                         !octl->gen_pcc_code &&
                         !octl->suppress_line_breaking) {
                /* Break long string constants by using concatenation.  This
                   allows the output routine to begin a new line. */
                output_partial_token_str("\"", octl);
                octl->output_str(" ", octl);
                output_partial_token_str(prefix, octl);
                out_len = 0;
              }  /* if */
              wc = extract_character_from_string(str+a,
                                                 (unsigned int)char_size);
              /* Suppress the last character if it is a null. */
              if (a != (len - char_size) || wc != '\0') {
                out_len += form_wide_char(wc, octl);
              }  /* if */
            }  /* for */
            output_partial_token_str("\"", octl);
          } else {
            /* Normal (non-wide) string. */
            if (constant->variant.string.embed_expansion) {
              if (!constant->is_compound_literal) {
                /* In the compound literal case, the "{" will have already
                   been put out. */
                octl->output_str("{ ", octl);
              }  /* if */
            } else if (!constant->variant.string.func_name_tok) {
              output_partial_token_str("\"", octl);
            }  /* if */
            for (a = 0; a < len; a++) {
              /* When generating output for humans to read, abbreviate
                 long strings. */
              if (!octl->gen_compilable_code && a > 20 && len > 25) {
                output_partial_token_str("...", octl);
                break;
              }  /* if */
              if (constant->variant.string.embed_expansion) {
                form_unsigned_num(
                           (a_host_large_unsigned)(unsigned char)str[a], octl);
                if (a == len - 1) {
                  octl->output_str(" }", octl);
                } else {
                  octl->output_str(", ", octl);
                  out_len += 5;
                }  /* if */
              } else {
                if (out_len >= 128 && octl->gen_compilable_code &&
                    !octl->gen_pcc_code && !octl->suppress_line_breaking) {
                  /* Break long string constants by using concatenation.
                     This allows the output routine to begin a new line. */
                  output_partial_token_str("\"", octl);
                  octl->output_str(" ", octl);
                  output_partial_token_str("\"", octl);
                  out_len = 0;
                }  /* if */
                ch = str[a];
                /* Suppress the last character if it is a null. */
                if (a != (len - 1) || ch != '\0') {
                  out_len += form_char(ch, octl);
                }  /* if */
              }  /* if */
            }  /* for */
            if (!constant->variant.string.func_name_tok &&
                !constant->variant.string.embed_expansion) {
              output_partial_token_str("\"", octl);
            }  /* if */
          }  /* if */
          if (constant->is_compound_literal && !is_for_c_gen_be(octl) &&
              !constant->variant.string.embed_expansion) {
            /* The string was originally a compound literal and, except in
               generated C code, should be put out that way. */
            octl->output_str("}", octl);
          }  /* if */
        }
      }
      break;
    case ck_float:
#if C99_IL_EXTENSIONS_SUPPORTED
    case ck_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      /* Floating-point constant. */
      /* Put parentheses around the constant in case it's negative. */
      octl->output_str("(", octl);
      /* coverity[var_deref_op] */
      form_float_constant(&constant->variant.float_value,
                          con_type->variant.float_kind,
                          constant->expr,
                          octl);
#if C99_IL_EXTENSIONS_SUPPORTED
      if (kind == (a_constant_repr_kind)ck_imaginary) {
        /* Imaginary constants are constructed with the EDG-specific __I__. */
        octl->output_str("*__I__", octl);
      }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      if (octl->part_of_ud_literal) {
        /* This constant is the literal portion of a C++11 user-defined
           literal.  The ud-suffix must follow the literal immediately, so
           the right parenthesis cannot be put out now. */
        octl->pending_right_paren = TRUE;
      } else {
        octl->output_str(")", octl);
      }  /* if */
      break;
#if C99_IL_EXTENSIONS_SUPPORTED
    case ck_complex:
      /* Complex constant. */
      /* Put parentheses around the constant and use the form
         ( A + B*__I__ ). */
      octl->output_str("(", octl);
      /* coverity[var_deref_op] */
      form_float_constant(&constant->variant.complex_value->real,
                          con_type->variant.float_kind,
                          (an_expr_node_ptr)NULL,
                          octl);
      octl->output_str(" + ", octl);
      /* coverity[var_deref_op] */
      form_float_constant(&constant->variant.complex_value->imag,
                          con_type->variant.float_kind,
                          (an_expr_node_ptr)NULL,
                          octl);
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
      if (!octl->gen_compilable_code ||
          gcc_or_clang_is_generated_code_target) {
        /* GNU compilers can parse complex constants like 1.0+2.0i.  That
           form is also used in contexts that aren't actual code. */
        octl->output_str("i", octl);
      } else
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
      /* Do not insert code here. */
      {
        octl->output_str("*__I__", octl);
      }  /* if */
      octl->output_str(")", octl);
      break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case ck_address:
      /* Address constant. */
      form_address_constant(constant,
                            (!is_for_c_gen_be(octl) &&
                             is_reference_type(constant->type)),
                            need_parens, octl);
      break;
    case ck_ptr_to_member:
      /* Pointer-to-member constant. */
      form_pm_constant(constant, /*minimal_casts=*/!octl->gen_compilable_code,
                       need_parens, octl);
      break;
#if GNU_EXTENSIONS_ALLOWED
    case ck_label_difference:
      form_label_difference_constant(constant, need_parens, octl);
      break;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING && GENERATE_EH_TABLES && !DO_FULL_PORTABLE_EH_LOWERING
    case ck_stack_offset:
      octl->output_str(error_text(ec_stack_offset_of), octl);
      form_name(&constant->variant.stack_offset.variable->source_corresp,
                iek_variable, octl);
      if (constant->variant.stack_offset.offset != 0) {
        octl->output_str("+", octl);
        form_unsigned_num(
                  (a_host_large_unsigned)constant->variant.stack_offset.offset,
                  octl);
      }  /* if */
      octl->output_str(">", octl);
      break;
#endif /* DO_IL_LOWERING && ... */
    case ck_dynamic_init:
      if (octl->gen_compilable_code &&
          constant->variant.dynamic_init.ptr->kind ==
                                         (a_dynamic_init_kind)dik_expression) {
        form_expression(constant->variant.dynamic_init.ptr->variant.expression,
                        octl);
      } else {
        form_dynamic_init_constant(constant, octl);
      }  /* if */
      break;
    case ck_aggregate:
      { a_constant_ptr sub_con = constant->variant.aggregate.first_constant;
        a_boolean saved_in_nontype_arg =
                                    octl->processing_nontype_template_argument;
        if (octl->processing_nontype_template_argument &&
            is_immediate_class_type(constant->type)) {
          /* This is a class-typed nontype template argument.  Put out the
             class name before the braced list (but not for any subaggregates).
             Don't put it out if we already rendered the cast type above. */
          if (!cast_already_put_out) {
            form_name(&constant->type->source_corresp, iek_type, octl);
          }  /* if */
          octl->processing_nontype_template_argument = FALSE;
        }  /* if */
        octl->output_str("{", octl);
        for (; sub_con != NULL; sub_con = sub_con->next) {
          if (sub_con->implicit_aggr_element && !is_for_c_gen_be(octl)) {
#if DEBUG
            if (octl->debug_output) {
              /* Emit the constant (with an indication that it is implicit). */
              octl->output_str(error_text(ec_implicit_element), octl);
            } else
#endif /* DEBUG */
            /* Do not insert code here. */
            {
              /* Don't render implicit elements unless the C-generating back
                 end has to compile them. */
              continue;
            }  /* if */
          }  /* if */
          form_constant(sub_con, /*need_parens=*/FALSE, octl);
          if (sub_con->next != NULL &&
              sub_con->kind != (a_constant_repr_kind)ck_designator) {
            octl->output_str(", ", octl);
          }  /* if */
        }  /* for */
        octl->output_str("}", octl);
        octl->processing_nontype_template_argument = saved_in_nontype_arg;
      }
      break;
    case ck_init_repeat:
      if (!octl->gen_compilable_code) {
        octl->output_str("<", octl);
        form_unsigned_num(
             (a_host_large_unsigned)constant->variant.init_repeat.count, octl);
        octl->output_str(error_text(ec_repetitions_of), octl);
        form_constant(constant->variant.init_repeat.constant,
                      /*need_parens=*/FALSE, octl);
        octl->output_str(">", octl);
      } else {
        /* Generate compilable code by expanding the repetition. */
        a_host_large_unsigned
             k, n = (a_host_large_unsigned)constant->variant.init_repeat.count;
        for (k = 0; k<n; ++k) {
          if (k > 0) octl->output_str(", ", octl);
          form_constant(constant->variant.init_repeat.constant,
                        /*need_parens=*/FALSE, octl);
        }  /* for */
      }
      break;
    case ck_template_param:
      check_assertion(!octl->gen_compilable_code ||
                      prototype_instantiations_in_il);
      switch (constant->variant.template_param.kind) {
        case tpck_unknown_function:
        case tpck_template_ref:
          /* Address of an unknown function, or of an unknown function template
             with an explicit template argument list. */
          if (need_parens) octl->output_str("(", octl);
          if (template_con_is_ampersand_operand(constant)) {
            octl->output_str("&", octl);
          }  /* if */
          form_unknown_lvalue_constant(constant, octl);
          if (need_parens) octl->output_str(")", octl);
          break;
        case tpck_param:
          {
            a_source_correspondence_ptr scp = &constant->source_corresp;
            an_il_entry_kind            scp_kind = iek_constant;
            a_source_correspondence_ptr new_scp;
            /* See whether the template parameter name is remapped in the
               current context. */
            new_scp = source_corresp_for_template_param(
                       &constant->variant.template_param.variant.coordinates);
            if (new_scp != NULL) {
              scp = new_scp;
              scp_kind = iek_template_parameter;
            }  /* if */
            form_name(scp, scp_kind, octl);
          }
          break;
        case tpck_member:
          form_unknown_lvalue_constant(constant, octl);
          break;
        case tpck_expression:
          if (constant->type->kind == tk_integer &&
              constant->type->variant.integer.enum_type &&
              has_name(constant) &&
              !octl->suppress_name_in_template_cast_enum_const) {
            /* This is an alias for a named enumerator -- just put out the
               name. */
            form_name(&constant->source_corresp, iek_constant, octl);
          } else if (octl->output_expression != NULL) {
            /* Do not add parentheses gratuitously. */
            octl->output_expression(expr_node_from_tpck_expression(constant),
                                    !need_parens);
          } else {
            form_expression(expr_node_from_tpck_expression(constant), octl);
          }  /* if */
          break;
        case tpck_dependent_constant:
          form_constant(constant->variant.template_param.variant.constant,
                        need_parens, octl);
          break;
        case tpck_concat_string_literals:
          { a_constant_ptr  elem_cp = constant->variant.template_param
                                               .variant.string_literal_list;
            for (; elem_cp != NULL; elem_cp = elem_cp->next) {
              form_constant(elem_cp, /*need_parens=*/FALSE, octl);
              if (elem_cp->next != NULL) octl->output_str(" ", octl);
            }  /* for */
          }
          break;
        case tpck_address:
          if (need_parens) octl->output_str("(", octl);
          octl->output_str("&", octl);
          form_constant(constant->variant.template_param.variant.constant,
                        /*need_parens=*/FALSE, octl);
          if (need_parens) octl->output_str(")", octl);
          break;
        case tpck_sizeof:
          octl->output_str("sizeof(", octl);
          goto do_sizeof_cases;
        case tpck_datasizeof:
          octl->output_str("__datasizeof(", octl);
          goto do_sizeof_cases;
        case tpck_alignof:
          if (use_microsoft_form() || use_sun_form()) {
            octl->output_str("__alignof(", octl);
          } else if (use_gnu_form()) {
            octl->output_str("__alignof__(", octl);
          } else if (constant->variant.template_param
                              .variant.templ_sizeof.is_std_alignof) {
            octl->output_str("alignof(", octl);
          } else {
            octl->output_str("__ALIGNOF__(", octl);
          }  /* if */
          goto do_sizeof_cases;
        case tpck_noexcept:
          octl->output_str("noexcept(", octl);
do_sizeof_cases:
          { expr = generic_sizeof_arg_expr(constant);
            if (expr != NULL) {
              a_boolean parens_needed = FALSE;
              if (is_constant_node(expr)) {
                an_expr_node_ptr e2 =
                                  expr_node_from_constant(node_constant(expr));
                if (e2 != NULL && node_is(e2, enk_temp_init) &&
                    dyn_init_is(e2->variant.init.dynamic_init, dik_zero)) {
                  /* Avoid putting out something like "sizeof(T())", which
                     would be the size of a function type. */
                  parens_needed = TRUE;
                  octl->output_str("(", octl);
                }  /* if */
              }  /* if */
              form_expression(expr, octl);
              if (parens_needed) {
                octl->output_str(")", octl);
              }  /* if */
            } else {
              form_type(
                    constant->variant.template_param.variant.templ_sizeof.type,
                    octl);
            }  /* if */
            octl->output_str(")", octl);
          }
          break;
        case tpck_uuidof:
          /* The constant represents the address of the __uuidof, so add
             a "&". */
          if (need_parens) octl->output_str("(", octl);
          octl->output_str("&", octl);
          form_uuidof_reference(constant, octl);
          if (need_parens) octl->output_str(")", octl);
          break;
        case tpck_typeid:
          /* The constant represents the address of a typeid result, so add
             a "&". */
          if (need_parens) octl->output_str("(", octl);
          octl->output_str("&", octl);
          form_typeid_reference(constant, octl);
          if (need_parens) octl->output_str(")", octl);
          break;
        case tpck_integer_pack:
          octl->output_str("__integer_pack(", octl);
          form_constant(constant->variant.template_param.variant.bound,
                        /*need_parens=*/FALSE, octl);
          octl->output_str(")...", octl);
          break;
        case tpck_destructor:
          { a_type_ptr dtor_type =
                     constant->variant.template_param.variant.destructor.type;
            if (need_parens) octl->output_str("(", octl);
            if (has_name(constant) &&
                constant->source_corresp.name[0] == '~' &&
                constant->source_corresp.name[1] != '<') {
              /* The constant's name reflects the way it actually appeared
                 in the original source code, which might involve typedefs
                 or dependent names that should be preserved, rather than
                 using resolved types.  (The check to exclude names
                 beginning with "~<" is so that destructors involving type
                 operators like decltype will use form_type.) */
              octl->output_str(constant->source_corresp.name, octl);
            } else {
              octl->output_str("~", octl);
              if (has_name(dtor_type) &&
                  constant
                    ->variant.template_param.variant.destructor.unqualified) {
                /* Render the type with an unqualified name (form_type has
                   insufficient information to determine this, so we handle
                   that case at this level).  Note that an unqualified
                   destructor invocation like p->~decltype(...) should
                   still go through form_type (hence the "has_name" test
                   above). */
                a_boolean  saved_suppress_template_args =
                                                  octl->suppress_template_args;
                if (is_immediate_class_type(dtor_type) &&
                    dtor_type->
                       variant.class_struct_union.is_prototype_instantiation) {
                  /* A destructor prototype instantiation can only appear
                     in the prototype instantiation of its parent, so no
                     template argument list is needed (and, if a template
                     parameter is unnamed, couldn't be put out in any
                     case). */
                  octl->suppress_template_args = TRUE;
                }  /* if */
                form_unqualified_name(&dtor_type->source_corresp, iek_type,
                                      octl);
                octl->suppress_template_args = saved_suppress_template_args;
              } else {
                form_type(dtor_type, octl);
              }  /* if */
            }  /* if */
            if (need_parens) octl->output_str(")", octl);
          }
          break;
        default:
          octl->output_str("**BAD-TEMPLATE-PARAM-CONSTANT-KIND**", octl);
      }  /* switch */
      break;
    case ck_designator:
      if (constant->variant.designator.is_field_designator) {
        a_const_char  *name;
        if (constant->variant.designator.is_generic) {
          name = constant->variant.designator.variant.field_name;
        } else {
          name = unmangled_name_of(&constant->variant.designator.variant.field
                                            ->source_corresp);
        }  /* if */
#if BACK_END_IS_CP_GEN_BE
        if (il_header.source_language == sl_Cplusplus &&
            gcc_or_clang_is_generated_code_target &&
            octl->gen_compilable_code) {
          /* g++ does not accept the C99 syntax for designated initializers
             but does accept a nonstandard variant:
                 struct S s = { m: 0 }; */
          check_assertion(name != NULL);
          octl->output_str(name, octl);
          octl->output_str(": ", octl);
        } else
#endif /* BACK_END_IS_CP_GEN_BE */
        /* Do not insert code here. */
        {
          /* Use the C99 designated initializer syntax. */
          octl->output_str(".", octl);
          if (name == NULL) {
            /* An unnamed designated initializer.  Can occur when a member of
               an anonymous union is the recipient of a designated initializer.
               This is appropriate in diagnostic and debugging output, but
               shouldn't make its way into generated code. */
            check_assertion(!octl->gen_compilable_code);
            octl->output_str(error_text(ec_quoted_unnamed), octl);
          } else {
            octl->output_str(name, octl);
          }  /* if */
          octl->output_str(" = ", octl);
        }  /* if */
      } else {
        octl->output_str("[", octl);
        if (constant->variant.designator.is_generic) {
          a_constant  *idx = constant->variant.designator.variant.subscript;
          form_constant(idx, /*need_parens=*/FALSE, octl);
          if (idx->next != NULL) {
            octl->output_str(" ... ", octl);
            form_constant(idx->next, /*need_parens=*/FALSE, octl);
          }  /* if */
        } else {
          form_unsigned_num(constant->variant.designator.variant.array_element,
                            octl);
        }  /* if */
        octl->output_str("] = ", octl);
      }  /* if */
      break;
    case ck_void:
      octl->output_str("((void)0)", octl);
      break;
    case ck_reflection:
      if (is_for_c_gen_be(octl)) {
        /* Reflection currently sometimes leak into the C-generating back end,
           which treats them as void* pointers.  The values are not used by
           the generated C code, however.  Just render a null pointer. */
        octl->output_str("((void*)0)", octl);
      } else {
        form_reflection(constant->variant.reflection, octl);
      }  /* if */
      break;
    default:
#if DEBUG
      if (octl->debug_output) {
        octl->output_str("**BAD-CONSTANT-KIND**", octl);
        break;
      }  /* if */
#endif /* DEBUG */
      unexpected_condition_str("form_constant: bad constant kind");
  }  /* switch */
  if (need_reinterpret_cast) octl->output_str(")", octl);
  if (need_cast_close_paren) octl->output_str(")", octl);
done:;
}  /* form_constant */


void form_lvalue_address_constant(
                          a_constant_ptr                        constant,
                          a_boolean                             need_parens,
                          an_il_to_str_output_control_block_ptr octl)
/*
Output the indicated constant.  It's the initial value of a reference,
so remove one level of indirection (basically, the constant is an address).
If need_parens is TRUE, parentheses are placed around the constant if
there's any possibility of precedence confusion.  Do the output in the
way described by octl.
*/
{
  a_type_ptr  object_type, element_type;
  if (constant->kind == (a_constant_repr_kind)ck_address &&
      /* Suppress this special processing on something like *"abcd", because
         gcc 2.95.2 issues a warning on storing into "abcd"[0] but not on
         storing into *"abcd".  The other form is correct; it's avoided
         only because it draws this warning in one form and not in the
         other.  (The issue is differences in test suite runs, not
         the warning per se.) */
      !(constant->implicit_cast &&
        constant->variant.address.kind == (an_address_base_kind)abk_constant &&
        constant->variant.address.variant.constant->kind ==
                                             (a_constant_repr_kind)ck_string &&
        constant->variant.address.offset == 0 &&
        is_pointer_type(constant->type) &&
        (object_type = type_pointed_to(constant->type),
         element_type = array_element_type(
                            constant->variant.address.variant.constant->type),
         standalone_identical_types(object_type, element_type)))) {
    /* An address constant (the usual case).  Drop one level of "&". */
    form_address_constant(constant, /*form_lvalue=*/TRUE, need_parens, octl);
  } else if (constant->kind == (a_constant_repr_kind)ck_template_param &&
             constant->variant.template_param.kind ==
                                  (a_template_param_constant_kind)tpck_param) {
    /* This is a template parameter list in a prototype instantiation.
       The parameter has a reference type, so no adjustment is needed. */
    form_name(&constant->source_corresp, iek_constant, octl);
  } else {
    /* For other cases, e.g.,
         int &r = *(int *)5;
       just display the constant. */
    form_constant(constant, need_parens, octl);
  }  /* if */
}  /* form_lvalue_address_constant */

#if GNU_EXTENSIONS_ALLOWED
#if BACK_END_IS_C_GEN_BE

static void form_simple_attribute(
                   a_const_char                           *attribute_name,
                   a_boolean                              *need_leading_space,
                   an_il_to_str_output_control_block_ptr  octl)
/*
Output a simple GNU attribute described by attribute_name in the way
described by octl.  If *need_leading_space is TRUE, precede the attribute
with a leading space.  *need_leading_space is set to TRUE in all cases,
to indicate that a space will be needed after the attribute.
*/
{
  if (*need_leading_space) {
    octl->output_str(" ", octl);
  }  /* if */
  octl->output_str("__attribute__((", octl);
  octl->output_str(attribute_name, octl);
  octl->output_str("))", octl);
  *need_leading_space = TRUE;
}  /* form_simple_attribute */
                                  

static void form_string_argument_attribute(
                   a_const_char                           *attribute_name,
                   a_const_char                           *argument,
                   a_boolean                              *need_leading_space,
                   an_il_to_str_output_control_block_ptr  octl)
/*
Output an attribute that takes a string as an argument.  The attribute_name
is assumed to have no characters that require escapes, but the argument might
have characters like "\n" or "\t" that need to be handled specially.
If *need_leading_space is TRUE, precede the attribute with a leading space.
*need_leading_space is set to TRUE in all cases, to indicate that a
space will be needed after the attribute.  Do the output in the way
described by octl.
*/
{
  a_const_char *c;

  if (*need_leading_space) {
    octl->output_str(" ", octl);
  }  /* if */
  octl->output_str("__attribute__((", octl);
  octl->output_str(attribute_name, octl);
  octl->output_str("(", octl);
  output_partial_token_str("\"", octl);
  for (c = argument; *c != '\0'; c++) {
    (void)form_char(*c, octl);
  }  /* for */
  output_partial_token_str("\"", octl);
  octl->output_str(")))", octl);
  *need_leading_space = TRUE;
}  /* form_string_argument_attribute */


static void form_unsigned_argument_attribute(
                   a_const_char                           *attribute_name,
                   a_host_large_unsigned                  argument,
                   a_boolean                              *need_leading_space,
                   an_il_to_str_output_control_block_ptr  octl)
/*
Output an attribute that takes an unsigned integer as an argument.  The
attribute_name is assumed to have no characters that require escapes.
If *need_leading_space is TRUE, precede the attribute with a leading space.
*need_leading_space is set to TRUE in all cases, to indicate that a
space will be needed after the attribute.  Do the output in the way
described by octl.
*/
{
  if (*need_leading_space) {
    octl->output_str(" ", octl);
  }  /* if */
  octl->output_str("__attribute__((", octl);
  octl->output_str(attribute_name, octl);
  octl->output_str("(", octl);
  form_unsigned_num((a_host_large_unsigned)argument, octl);
  octl->output_str(")))", octl);
  *need_leading_space = TRUE;
}  /* form_unsigned_argument_attribute */


static void form_recorded_gnu_attribute(
                   an_attribute_kind                      kind,
                   an_attribute_ptr                       attributes,
                   a_boolean                              *need_leading_space,
                   an_il_to_str_output_control_block_ptr  octl)
/*
If the given list of attributes contains an attribute of the given kind,
render that attribute as a GNU attribute.  If *need_leading_space is TRUE,
precede the attribute with a leading space.  *need_leading_space is set to
TRUE if an attribute is emitted, to indicate that a space will be needed after
the attribute.  Do the output in the way described by octl.
*/
{
  an_attribute_ptr  ap = find_attribute(kind, attributes);

  if (ap != NULL) {
    if (*need_leading_space) {
      octl->output_str(" ", octl);
    }  /* if */
    octl->output_str("__attribute__((", octl);
    octl->output_str(ap->name, octl);
    form_attribute_arguments(ap, octl);
    octl->output_str("))", octl);
    *need_leading_space = TRUE;
  }  /* if */
}  /* form_recorded_gnu_attribute */

#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED

static void form_ELF_visibility_attribute(
                   an_ELF_visibility_kind                 visibility,
                   a_boolean                              *need_leading_space,
                   an_il_to_str_output_control_block_ptr  octl)
/*
Output the given visibility as an attribute specification (provided it is
not evk_unspecified).  If *need_leading_space is TRUE, precede the attribute
with a leading space.  If an attribute is output, set *need_leading_space to
TRUE.  Do the output in the way described by octl.
*/
{
  switch (visibility) {
    case evk_unspecified:
      /* No visibility attribute. */
      break;
    case evk_hidden:
      form_simple_attribute("visibility(\"hidden\")", need_leading_space,
                            octl);
      break;
    case evk_protected:
      form_simple_attribute("visibility(\"protected\")", need_leading_space,
                            octl);
      break;
    case evk_internal:
      form_simple_attribute("visibility(\"internal\")", need_leading_space,
                            octl);
      break;
    case evk_default:
      form_simple_attribute("visibility(\"default\")", need_leading_space,
                            octl);
      break;
    default:
      unexpected_condition();
  }  /* switch */
}  /* form_ELF_visibility_attribute */

#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */

static void form_alignment_attributes(
                   a_type_ptr                             type,
                   a_boolean                              *need_leading_space,
                   an_il_to_str_output_control_block_ptr  octl)
/*
Output GNU "aligned" and "packed" attributes as recorded in the given type.
If *need_leading_space is TRUE, precede the attribute with a leading space.
If an attribute is output, set *need_leading_space to TRUE.  Do the output
in the way described by octl.
*/
{
  if (type->alignment_set_explicitly) {
    /* Output an attribute to indicate the explicit alignment. */
    form_unsigned_argument_attribute("__aligned__",
                                     (a_host_large_unsigned)type->alignment,
                                     need_leading_space, octl);
  }  /* if */
  if ((is_immediate_class_type(type) &&
       type->variant.class_struct_union.is_packed) ||
      (is_immediate_enum_type(type) && type->variant.integer.packed)) {
    form_simple_attribute("__packed__", need_leading_space, octl);
  }  /* if */
}  /* form_alignment_attributes */


static void form_routine_type_attributes(
                   a_type_ptr                             type,
                   a_boolean                              *need_leading_space,
                   an_il_to_str_output_control_block_ptr  octl)
/*
Output GNU attributes that apply to the indicated type (which must be a
routine type).  If *need_leading_space is TRUE, precede the attribute with
a leading space.  If an attribute is output, set *need_leading_space to TRUE.
Do the output in the way described by octl.
*/
{
  a_routine_type_supplement_ptr 
                      rtsp = skip_typerefs(type)->variant.routine.extra_info;

  form_alignment_attributes(type, need_leading_space, octl);
  if (rtsp->result_should_be_used && !is_for_c_gen_be(octl)) {
    /* If we're generating output for the C-generating back end, we do not
       output the attribute __warn_unused_result__ because any diagnostics it
       might trigger were already issued by the front end. */
    form_simple_attribute("__warn_unused_result__", need_leading_space, octl);
  }  /* if */
  if (rtsp->does_not_return) {
    form_simple_attribute("__noreturn__", need_leading_space, octl);
  }  /* if */
  if (rtsp->is_const) {
    form_simple_attribute("__const__", need_leading_space, octl);
  }  /* if */
#if GNU_X86_ATTRIBUTES_ALLOWED
  if (target_is_32_bit_x86_based()) {
    switch (rtsp->calling_convention) {
      case cc_default:
        /* No attribute to generate. */
        break;
      case cc_cdecl:
        form_simple_attribute("__cdecl__", need_leading_space, octl);
        break;
      case cc_fastcall:
        if (!octl->gen_compilable_code
#if GCC_IS_GENERATED_CODE_TARGET
            || gnu_target_version_number >= 40200
#endif /* GCC_IS_GENERATED_CODE_TARGET */
                                                 ) {
          form_simple_attribute("__fastcall__", need_leading_space, octl);
        }  /* if */
        break;
      case cc_stdcall:
        form_simple_attribute("__stdcall__", need_leading_space, octl);
        break;
      case cc_thiscall:
        /* A Microsoft-only calling convention.  These aren't generated for
           the GNU C compiler. */
        break;
      case cc_vectorcall:
        /* A Microsoft-only calling convention.  These aren't generated for
           the GNU C compiler. */
        break;
      case cc_clrcall:
        /* A Microsoft-only calling convention.  These aren't generated for
           the GNU C compiler. */
        break;
      default:
        unexpected_condition();
    }  /* switch */
  }  /* if */
#endif /* GNU_X86_ATTRIBUTES_ALLOWED */
  if (!is_for_c_gen_be(octl)) {
    /* Don't emit the following attributes in generated C code to avoid having
       a back-end C compiler duplicate a diagnostic already emitted by the
       front end. */
    /* Generate any needed "nonnull" attributes: */
    a_param_type_ptr  ptp = rtsp->param_type_list;
    unsigned int      p = 1;
    for (; ptp != NULL; ptp = ptp->next, ++p) {
      if (ptp->nonnull) {
        form_unsigned_argument_attribute("nonnull", (a_host_large_unsigned)p,
                                         need_leading_space, octl);
      }  /* if */
    }  /* for */
    /* Generate the "sentinel" attribute if needed: */
    if (rtsp->sentinel_pos != 0) {
      /* Note that our representation is "one off" compared to the source form:
         I.e., "sentinel(0)" in the source is corresponds to sentinel_pos == 1
         to reserve sentinel_pos == 0 as a representation for "no sentinel". */
      form_unsigned_argument_attribute(
               "sentinel",
               (a_host_large_unsigned)(rtsp->sentinel_pos-1) /*lint --e(571)*/,
               need_leading_space, octl);
    }  /* if */
  }  /* if */
}  /* form_routine_type_attributes */


static inline void form_deprecated_or_unavailable_attribute(
                     a_source_correspondence               *scp,
                     a_boolean                             *need_leading_space,
                     an_il_to_str_output_control_block_ptr octl)
/*
Output "deprecated" and/or "unavailable" attributes if applicable.
*/
{
  if (scp->is_deprecated_or_unavailable && !is_for_c_gen_be(octl)) {
    /* If we're generating output for the C-generating back end, we do not
       output the __deprecated__ or __unavailable__ attributes because any
       diagnostics it might trigger were already issued by the front end. */
    if (find_attribute(ak_deprecated, scp->attributes)) {
      form_simple_attribute("__deprecated__", need_leading_space, octl);
    }  /* if */
    if (find_attribute(ak_unavailable, scp->attributes)) {
      form_simple_attribute("__unavailable__", need_leading_space, octl);
    }  /* if */
  }  /* if */
}  /* form_deprecated_or_unavailable_attribute */


a_boolean form_type_attributes(
                    a_type_ptr                             type,
                    a_boolean                              need_leading_space,
                    an_il_to_str_output_control_block_ptr  octl)
/*
Output GNU attributes that apply to the indicated type.  If need_leading_space
is TRUE, precede the first attribute with a leading space.  If an attribute is
output or if need_leading_space is TRUE, return TRUE (this allows the caller
to determine if a leading space is still needed).  Do the output in the way
described by octl.
*/
{
  if (!octl->gen_compilable_code || gcc_or_clang_is_generated_code_target) {
    /* First emit the attributes that when appearing on a typedef would be
       recorded in the typedef entry itself (as opposed to the underlying
       type). */
    if (type->kind != (a_type_kind)tk_routine) {
      form_alignment_attributes(type, &need_leading_space, octl);
      if (type->may_alias) {
        form_simple_attribute("__may_alias__", &need_leading_space, octl);
      }  /* if */
    } else {
      /* For routine types, the alignment attributes are emitted by the call
         to form_routine_type_attributes (below) and the may_alias attribute
         is rendered by output_type_attributes. */
    }  /* if */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
    if (is_immediate_class_type(type) && !is_for_c_gen_be(octl)) {
      form_ELF_visibility_attribute(enum_cast<an_ELF_visibility_kind>(
                                        class_type_supp(type)->ELF_visibility),
                                    &need_leading_space, octl);
    } else if (is_immediate_enum_type(type) && !is_for_c_gen_be(octl)) {
      form_ELF_visibility_attribute(enum_cast<an_ELF_visibility_kind>(
                                         type->variant.integer.ELF_visibility),
                                    &need_leading_space, octl);
    }  /* if */
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
    if (type->variables_are_implicitly_referenced) {
      /* Output the "unused" attribute. */
      form_simple_attribute("__unused__", &need_leading_space, octl);
    }  /* if */
    form_deprecated_or_unavailable_attribute(&type->source_corresp,
                                             &need_leading_space, octl);
    form_recorded_gnu_attribute(ak_alloc_size, type->source_corresp.attributes,
                                &need_leading_space, octl);
    form_recorded_gnu_attribute(ak_mode, type->source_corresp.attributes,
                                &need_leading_space, octl);
    /* The following attributes would be recorded on the underlying type
       if the attribute appeared on a typedef. */
    type = skip_typerefs(type);
    if (type->kind == (a_type_kind)tk_union &&
        type->variant.class_struct_union.is_transparent) {
      form_simple_attribute("__transparent_union__", &need_leading_space,
                            octl);
    }  /* if */
    if (is_pointer_type(type) &&
        is_function_type(type_pointed_to(type))) {
      form_routine_type_attributes(f_skip_typerefs(type_pointed_to(type)),
                                   &need_leading_space, octl);
    }  /* if */
  }  /* if */
  return need_leading_space;
}  /* form_type_attributes */


a_boolean form_variable_attributes(
                    a_variable_ptr                         var,
                    a_boolean                              need_leading_space,
                    an_il_to_str_output_control_block_ptr  octl)
/*
Output the GNU attributes that apply to the indicated variable.
If need_leading_space is TRUE, precede the first attribute with a leading
space.  If an attribute is output or if need_leading_space is TRUE, return
TRUE (this allows the caller to determine if a leading space is still needed).
Do the output in the way described by octl.
*/
{
  if (!octl->gen_compilable_code || gcc_or_clang_is_generated_code_target) {
    form_recorded_gnu_attribute(ak_alloc_size, var->source_corresp.attributes,
                                &need_leading_space, octl);
    if (var->alignment != 0) {
      /* Output the alignment attribute. */
      form_unsigned_argument_attribute("__aligned__",
                                       (a_host_large_unsigned)var->alignment,
                                       &need_leading_space, octl);
    }  /* if */
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
    if (var->init_priority != 0 && !is_for_c_gen_be(octl)) {
      /* The init_priority is a C++-only attribute; it is ignored with a
         warning by GNU C compilers.  To avoid the warning, we do not emit it
         in the C-generating back end.  (IL lowering ensures the
         initializations are performed in the right order.) */
      form_unsigned_argument_attribute(
                "__init_priority__", (a_host_large_unsigned)var->init_priority,
                &need_leading_space, octl);
    }  /* if */
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
    if (var->cleanup_routine != NULL) {
      if (need_leading_space) octl->output_str(" ", octl);
      need_leading_space = TRUE;
      octl->output_str("__attribute__((cleanup(", octl);
      a_source_correspondence_ptr cleanup_scp =
                                         &var->cleanup_routine->source_corresp;
      if (C_mode() || !is_for_c_gen_be(octl)) {
        form_unqualified_name(cleanup_scp, iek_routine, octl);
      } else {
        /* Need mangled name. */
        check_assertion(cleanup_scp->name_has_been_mangled);
        octl->output_str(cleanup_scp->name, octl);
      }  /* if */
      octl->output_str(")))", octl);
    }  /* if */
    form_recorded_gnu_attribute(ak_externally_visible,
                                var->source_corresp.attributes,
                                &need_leading_space, octl);
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
    form_ELF_visibility_attribute(enum_cast<an_ELF_visibility_kind>(
                                                          var->ELF_visibility),
                                  &need_leading_space, octl);
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
    if (var->is_weak && !var->is_weakref) {
      /* The "weakref" attribute implies the "weak" attribute: We don't need
         to emit both. */
      form_simple_attribute("__weak__", &need_leading_space, octl);
    }  /* if */
    if (var->source_corresp.maybe_unused) {
      form_simple_attribute("__unused__", &need_leading_space, octl);
    }  /* if */
    if (var->has_gnu_used_attribute) {
      form_simple_attribute("__used__", &need_leading_space, octl);
    }  /* if */
    form_deprecated_or_unavailable_attribute(&var->source_corresp,
                                             &need_leading_space, octl);
    if (var->is_not_common) {
      form_simple_attribute("__nocommon__", &need_leading_space, octl);
    }  /* if */
    if (var->is_parameter &&
        var->variant.assoc_param_type != NULL &&
        var->variant.assoc_param_type->is_transparent) {
      form_simple_attribute("__transparent_union__", &need_leading_space,
                            octl);
    }  /* if */
    if (var->section != NULL) {
      form_string_argument_attribute("__section__", var->section,
                                     &need_leading_space, octl);
    }  /* if */
#if THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
    if ((var->decl_modifiers & DM_THREAD) != 0) {
      form_recorded_gnu_attribute(ak_tls_model, var->source_corresp.attributes,
                                  &need_leading_space, octl);
    }  /* if */
#endif /* THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED */
    if (var->is_gnu_alias) {
      form_recorded_gnu_attribute(ak_alias, var->source_corresp.attributes,
                                  &need_leading_space, octl);
    } else if (var->is_weakref) {
      form_recorded_gnu_attribute(ak_weakref, var->source_corresp.attributes,
                                  &need_leading_space, octl);
    }  /* if */
    if (is_pointer_type(var->type) &&
        is_function_type(type_pointed_to(var->type))) {
      form_routine_type_attributes(f_skip_typerefs(type_pointed_to(var->type)),
                                   &need_leading_space, octl);
    }  /* if */
  }  /* if */
  return need_leading_space;
}  /* form_variable_attributes */


a_boolean form_field_attributes(
                   a_field_ptr                            field,
                   a_boolean                              need_leading_space,
                   an_il_to_str_output_control_block_ptr  octl)
/*
Output the GNU attributes that apply to the indicated field.
If need_leading_space is TRUE, precede the first attribute with a leading
space.  If an attribute is output or if need_leading_space is TRUE, return
TRUE (this allows the caller to determine if a leading space is still needed).
Do the output in the way described by octl.
*/
{
  if (!octl->gen_compilable_code || gcc_or_clang_is_generated_code_target) {
    form_recorded_gnu_attribute(ak_alloc_size,
                                field->source_corresp.attributes,
                                &need_leading_space, octl);
    form_deprecated_or_unavailable_attribute(&field->source_corresp,
                                             &need_leading_space, octl);
    if (field->is_packed) {
      form_simple_attribute("__packed__", &need_leading_space, octl);
    }  /* if */
    if (field->alignment != 0) {
      form_unsigned_argument_attribute("__aligned__",
                                       (a_host_large_unsigned)field->alignment,
                                       &need_leading_space, octl);
    }  /* if */
    if (is_pointer_type(field->type) &&
        is_function_type(type_pointed_to(field->type))) {
      form_routine_type_attributes(
                                f_skip_typerefs(type_pointed_to(field->type)),
                                &need_leading_space, octl);
    }  /* if */
  }  /* if */
  return need_leading_space;
}  /* form_field_attributes */


a_boolean form_routine_attributes(
                   a_routine_ptr                          rout,
                   a_boolean                              need_leading_space,
                   an_il_to_str_output_control_block_ptr  octl)
/*
Output the GNU attributes that apply to the indicated routine.
If need_leading_space is TRUE, precede the first attribute with a leading
space.  If an attribute is output or if need_leading_space is TRUE, return
TRUE (this allows the caller to determine if a leading space is still needed).
Do the output in the way described by octl.
*/
{
  if (!octl->gen_compilable_code || gcc_or_clang_is_generated_code_target) {
    an_attribute_ptr  attributes = rout->source_corresp.attributes;
    if (attributes != NULL) {
      /* Check for attributes that are recorded only as attribute entries
         (i.e., without additional fields in the IL entry for the routine). */
      form_recorded_gnu_attribute(ak_alloc_size, attributes,
                                  &need_leading_space, octl);
      form_recorded_gnu_attribute(ak_artificial, attributes,
                                  &need_leading_space, octl);
      form_recorded_gnu_attribute(ak_cold, attributes,
                                  &need_leading_space, octl);
      form_recorded_gnu_attribute(ak_error, attributes,
                                  &need_leading_space, octl);
      form_recorded_gnu_attribute(ak_externally_visible,
                                  attributes,
                                  &need_leading_space, octl);
      form_recorded_gnu_attribute(ak_flatten, attributes,
                                  &need_leading_space, octl);
      form_recorded_gnu_attribute(ak_hot, attributes,
                                  &need_leading_space, octl);
      form_recorded_gnu_attribute(ak_warning, attributes,
                                  &need_leading_space, octl);
    }  /* if */
    if (rout->is_initialization_routine) {
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
      if (rout->has_ctor_priority) {
        form_unsigned_argument_attribute(
               "__constructor__",
               (a_host_large_unsigned)gnu_routine_supp(rout)->ctor_priority,
               &need_leading_space, octl);
      } else
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
      /* Do not insert code here. */
      {
        form_simple_attribute("__constructor__", &need_leading_space, octl);
      }  /* if */
    }  /* if */
    if (rout->is_finalization_routine) {
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
      if (rout->has_dtor_priority) {
        form_unsigned_argument_attribute(
               "__destructor__",
               (a_host_large_unsigned)gnu_routine_supp(rout)->dtor_priority,
               &need_leading_space, octl);
      } else
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
      /* Do not insert code here. */
      {
        form_simple_attribute("__destructor__", &need_leading_space, octl);
      }  /* if */
    }  /* if */
    if (rout->is_pure) {
      form_simple_attribute("__pure__", &need_leading_space, octl);
    }  /* if */
    if (rout->is_weak && !rout->is_weakref) {
      /* The "weakref" attribute implies the "weak" attribute: We don't need
         to emit both. */
      form_simple_attribute("__weak__", &need_leading_space, octl);
    }  /* if */
    if (rout->source_corresp.maybe_unused) {
      form_simple_attribute("__unused__", &need_leading_space, octl);
    }  /* if */
    if (rout->has_gnu_used_attribute) {
      form_simple_attribute("__used__", &need_leading_space, octl);
    }  /* if */
    form_deprecated_or_unavailable_attribute(&rout->source_corresp,
                                             &need_leading_space, octl);
    if (rout->allocates_memory) {
      form_simple_attribute("__malloc__", &need_leading_space, octl);
    }  /* if */
#if GNU_NAKED_ATTRIBUTE_ALLOWED
    if (rout->is_naked) {
      form_simple_attribute("__naked__", &need_leading_space, octl);
    }  /* if */
#endif /* GNU_NAKED_ATTRIBUTE_ALLOWED */
    if (rout->no_instrument_function) {
      form_simple_attribute("__no_instrument_function__", &need_leading_space,
                            octl);
    }  /* if */
    if (rout->no_check_memory_usage) {
      form_simple_attribute("__no_check_memory_usage__", &need_leading_space,
                            octl);
    }  /* if */
    if (rout->never_inline) {
      form_simple_attribute("__noinline__", &need_leading_space, octl);
    }  /* if */
    if (rout->always_inline) {
      form_simple_attribute("__always_inline__", &need_leading_space, octl);
    }  /* if */
    /* The "gnu_inline" attribute isn't recognized by older GNU compilers, but
       on those compilers the associated semantics are enabled by default. */
    if (rout->gnu_c89_inline &&
        (clang_is_generated_code_target ||
         (gcc_is_generated_code_target
#if GCC_IS_GENERATED_CODE_TARGET
          && gnu_target_version_number >= 40200
#endif /* GCC_IS_GENERATED_CODE_TARGET */
                                               ))) {
      form_simple_attribute("__gnu_inline__", &need_leading_space, octl);
    }  /* if */
    if (rout->never_throws &&
        (clang_is_generated_code_target ||
         (gcc_is_generated_code_target
#if GCC_IS_GENERATED_CODE_TARGET
          && gnu_target_version_number >= 30300
#endif /* GCC_IS_GENERATED_CODE_TARGET */
                                               ))) {
      form_simple_attribute("__nothrow__", &need_leading_space, octl);
    }  /* if */
    if (rout->type->kind == (a_type_kind)tk_routine) {
      /* If this routine is declared using ordinary function declarator
         syntax (i.e., not using a typedef), generate the associated
         routine type attributes. */
      form_routine_type_attributes(rout->type, &need_leading_space, octl);
    }  /* if */
    if (has_gnu_routine_supp(rout) &&
        gnu_routine_supp(rout)->section != NULL) {
      form_string_argument_attribute("__section__",
                                     gnu_routine_supp(rout)->section,
                                     &need_leading_space, octl);
    }  /* if */
    if (rout->is_gnu_alias) {
      form_recorded_gnu_attribute(ak_alias, rout->source_corresp.attributes,
                                  &need_leading_space, octl);
    } else if (rout->is_weakref) {
      form_recorded_gnu_attribute(ak_weakref, rout->source_corresp.attributes,
                                  &need_leading_space, octl);
    }  /* if */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
    form_ELF_visibility_attribute(enum_cast<an_ELF_visibility_kind>(
                                                         rout->ELF_visibility),
                                  &need_leading_space, octl);
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
  }  /* if */
  return need_leading_space;
}  /* form_routine_attributes */


a_boolean form_label_attributes(
                   a_label_ptr                            label,
                   a_boolean                              need_leading_space,
                   an_il_to_str_output_control_block_ptr  octl)
/*
Output the GNU attributes that apply to the indicated label in the way
described by octl.  If need_leading_space is TRUE, precede the first 
attribute with a leading space.  If an attribute is output or if
need_leading_space is TRUE, return TRUE (this allows the caller to
determine if a leading space is still needed).
*/
{
  if (!octl->gen_compilable_code || gcc_or_clang_is_generated_code_target) {
    if (label->source_corresp.maybe_unused) {
      form_simple_attribute("__unused__", &need_leading_space, octl);
    }  /* if */
  }  /* if */
  return need_leading_space;
}  /* form_label_attributes */

#endif /* BACK_END_IS_C_GEN_BE */
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE

void form_asm_name(a_const_char                           *asm_name,
                   an_il_to_str_output_control_block_ptr  octl)
/*
Output an asm name for a routine or variable in the way described by octl.
asm_name is allowed to be NULL.
*/
{
  a_const_char *c;

  if (gcc_or_clang_is_generated_code_target && asm_name != NULL) {
    octl->output_str(" __asm__(", octl);
    output_partial_token_str("\"", octl);
    for (c = asm_name; *c != '\0'; c++) {
      (void)form_char(*c, octl);
    }  /* for */
    output_partial_token_str("\"", octl);
    octl->output_str(")", octl);
  }  /* if */
}  /* form_asm_name */


void form_var_reg_name(a_named_register                       reg,
                       an_il_to_str_output_control_block_ptr  octl)
/*
Output an asm register name for a variable in the way described by octl.
*/
{
  if (gcc_or_clang_is_generated_code_target) {
    octl->output_str(" __asm__(", octl);
    output_partial_token_str("\"", octl);
    octl->output_str(named_register_names[(int)reg], octl);
    output_partial_token_str("\"", octl);
    octl->output_str(")", octl);
  }  /* if */
}  /* form_var_reg_name */

#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */

#endif /* GNU_EXTENSIONS_ALLOWED */
#if SUN_EXTENSIONS_ALLOWED
#if (BACK_END_IS_C_GEN_BE && C_GEN_BE_GENERATES_ANSI_C) || \
    BACK_END_IS_CP_GEN_BE

void form_sun_link_scope_specifiers(
                                 a_decl_modifier_set                    flags,
                                 an_il_to_str_output_control_block_ptr  octl)
/*
Output the Sun link scope specifiers as indicated by flags (in the way
described by octl).
*/
{
  if (flags & DM_GLOBAL_LINK_SCOPE) {
    octl->output_str("__global ", octl);
  }  /* if */
  if (flags & DM_SYMBOLIC_LINK_SCOPE) {
    octl->output_str("__symbolic ", octl);
  }  /* if */
  if (flags & DM_HIDDEN_LINK_SCOPE) {
    octl->output_str("__hidden ", octl);
  }  /* if */
}  /* form_sun_link_scope_specifiers */

#endif /* (BACK_END_IS_C_GEN_BE && C_GEN_BE_GENERATES_ANSI_C) || ... */
#endif /* SUN_EXTENSIONS_ALLOWED */
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE

void push_function_prototype(a_func_prototype_stack_entry_ptr       fpsep,
                             an_il_to_str_output_control_block_ptr  octl)
/*
Add the given function prototype stack entry on top of the function prototype
stack associated with octl.
*/
{
  fpsep->next = octl->func_prototype_stack;
  octl->func_prototype_stack = fpsep;
}  /* push_function_prototype */


void pop_function_prototype(an_il_to_str_output_control_block_ptr  octl)
/*
Pop a function prototype stack entry from the top of the function prototype
stack associated with octl.
*/
{
  check_assertion(octl->func_prototype_stack != NULL);
  octl->func_prototype_stack = octl->func_prototype_stack->next;
}  /* pop_function_prototype */


a_param_type_ptr get_param_for_param_ref(
                                      an_expr_node_ptr                  expr,
                                      a_func_prototype_stack_entry_ptr  fpsep)
/*
Return the a_param_type entry indicated by the given enk_param_ref node expr
for the function prototype scope context described by fpsep.
*/
{
  unsigned          k, levels_up = expr->variant.param_ref.levels_up;
  a_param_type_ptr  ptp;

  /* enk_param_ref nodes representing "this" should not get here. */
  check_assertion(fpsep != NULL &&
                  expr->kind == (an_expr_node_kind)enk_param_ref &&
                  expr->variant.param_ref.param_num != 0);
  if (levels_up == 0) {
    /* We're outside the parameter list containing the parameter of interest,
       but we may be inside a parameter list of a function declarator appearing
       in a type-id.  For example:
         template<class T> struct S {};
         auto f(int p) -> S<void (decltype(p))>;
       Here, levels_up is 0, but fpsep represents the function declarator in
       the template argument for S<...>.  Skip any such prototypes. */
    while (!fpsep->outside_parameter_list) {
      fpsep = fpsep->next;
    }  /* if */
  } else if (!fpsep->outside_parameter_list) {
    /* levels_up includes the function prototype enclosing the parameter of
       interest.  Since we only want to skip to that level, decrease the count
       by one. */
    levels_up -= 1;
  }  /* if */
  for (k = 0; k<levels_up; ++k) {
    fpsep = fpsep->next;
    check_assertion(fpsep != NULL);
  }  /* for */
  ptp = fpsep->params;
  check_assertion(ptp != NULL);
  for (k = 1; k<expr->variant.param_ref.param_num; ++k) {
    ptp = ptp->next;
    check_assertion(ptp != NULL);
  }  /* for */
  return ptp;
}  /* get_param_for_param_ref */

#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
#if BACK_END_IS_C_GEN_BE

void form_param_ref(an_expr_node_ptr                       expr,
                    an_il_to_str_output_control_block_ptr  octl)
/*
Render the name of the parameter described by the given enk_param_ref node.
The node itself only indicates the position and "level" of the parameter in
the function prototype stack.  Callers must therefore ensure that the stack
is properly maintained.  Do the output as indicated by octl.
*/
{
  check_assertion(expr->kind == (an_expr_node_kind)enk_param_ref);
  if (expr->variant.param_ref.param_num == 0) {
    /* This node represents "this". */
    octl->output_str("this", octl);
  } else {
    a_param_type_ptr ptp = get_param_for_param_ref(
                                            expr, octl->func_prototype_stack);
    check_assertion(ptp->name != NULL);
    octl->output_str(ptp->name, octl);
  }  /* if */
}  /* form_param_ref */

#endif /* BACK_END_IS_C_GEN_BE */

#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE

STATIC_THREAD uintptr_t
                unique_id_counter;
                        /* A ticket counter used by
                           generate_unique_id_for_temporary to generate stable
                           unique identifiers for a given pointer. */

namespace {

enum class a_temporary_key : uintptr_t {};
                        /* The key type of ptr_to_unique_id: an IL pointer
                           converted to an integer.  It is a distinct type
                           only so that it can have its own hash_ptr. */

inline uintptr_t hash_ptr(a_temporary_key key)
/*
Return a hash value for the given temporary key that is appropriate for
Ptr_map.  IL entries of a given kind tend to be allocated at regular intervals,
so the default pointer hash (essentially the address itself) clusters badly in
the low-order bits that Ptr_map uses for indexing.  Mix all the bits of the
address into the low-order bits (Fibonacci hashing).  The multiplier is the
golden-ratio constant scaled to the width of uintptr_t (simply truncating the
64-bit constant would not preserve that ratio).
*/
{
  constexpr unsigned  num_bits_total = sizeof(uintptr_t) * 8;
  constexpr uintptr_t multiplier =
                         (uintptr_t)(0x9E3779B97F4A7C15ull >>
                                     (64 - num_bits_total));
  uintptr_t           hash = (uintptr_t)key * multiplier;

  return hash ^ (hash >> (num_bits_total / 2));
}  /* hash_ptr */

}  /* namespace */

using a_ptr_unique_id_map = Ptr_map<a_temporary_key, uintptr_t,
                                    General_allocator>;
                        /* The type of ptr_to_unique_id used to map an
                           arbitrary pointer to a unique identifier from the
                           unique_id_counter. */

STATIC_THREAD a_ptr_unique_id_map
                *ptr_to_unique_id;
                        /* A map of a given pointer to its unique identifiers
                           from the unique_id_counter used by
                           generate_unique_id_for_temporary. */


static uintptr_t generate_unique_id_for_temporary(void *ptr)
/*
Return an identifier unique to the current generated file for the given IL
pointer.  Identifiers are assigned in order of first use, so (unlike the IL
pointer itself) they do not depend on where the IL happens to be allocated,
and the generated output is reproducible from run to run.
*/
{
  uintptr_t result;

  if (ptr == NULL) {
    result = 0;
  } else {
    a_temporary_key key = (a_temporary_key)(uintptr_t)ptr;

    result = ptr_to_unique_id->get(key);
    if (result == 0) {
      result = ++unique_id_counter;
      ptr_to_unique_id->map(key, result);
    }  /* if */
  }  /* if */
  return result;
}  /* generate_unique_id_for_temporary */


a_temp_var_name_buffer form_temporary_name_for_back_end(void *ptr)
/*
Return a temporary name generated for the given IL pointer.
*/
{
  a_temp_var_name_buffer result("__T", generate_unique_id_for_temporary(ptr));
  return result;
}  /* form_temporary_name_for_back_end */


void il_to_str_back_end_file_init()
/*
Initialize the common logic for a C-like language generating back end.  These
are initializations that must be redone for each generated file.
*/
{
  unique_id_counter = 0;
  if (ptr_to_unique_id == NULL) {
    ptr_to_unique_id = alloc_general_of_type(a_ptr_unique_id_map);
    construct(ptr_to_unique_id, /*mask_width=*/10u);
  } else {
    /* Reuse the map from the previous generated file rather than leaving it
       allocated until general memory is released. */
    ptr_to_unique_id->clear();
  }  /* if */
}  /* il_to_str_back_end_file_init */

#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */

void il_to_str_one_time_init(void)
/*
One-time initialization for il_to_str static variables.
*/
{
  template_param_map = NULL;
  template_param_map_max_level = 0;
#if BACK_END_IS_CP_GEN_BE
  avail_template_param_mappings = NULL;
#endif /* BACK_END_IS_CP_GEN_BE */
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
  ptr_to_unique_id = NULL;
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
#if BACK_END_IS_C_GEN_BE
#if LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C
#if ISSUE_WARNING_ON_LONG_DOUBLE_AS_DOUBLE
  double_for_long_double_warning_issued = FALSE;
#endif /* ISSUE_WARNING_ON_LONG_DOUBLE_AS_DOUBLE */
#endif /* LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C */
#endif /* BACK_END_IS_C_GEN_BE */
}  /* il_to_str_one_time_init */


void il_to_str_init(void)
/*
Initialization for il_to_str that must be performed before each source
file is processed.
*/
{
  if (template_param_map != NULL) {
    /* Make sure there are no leftover mappings from previous processing. */
    a_template_param_map_level_ptr level;
    a_template_nesting_depth       depth;
    a_template_param_list_pos      pos;

    for (depth = 0; depth < template_param_map_max_level; ++depth) {
      level = &template_param_map[depth];
      for (pos = 0; pos < level->max_position; ++pos) {
        level->source_corresp[pos] = NULL;
      }  /* for */
    }  /* for */
  }  /* if */
#if BACK_END_IS_CP_GEN_BE
  saved_template_param_mappings = NULL;
  depth_saved_templ_param_mappings = 0;
#endif /* BACK_END_IS_CP_GEN_BE */
}  /* il_to_str_init */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

