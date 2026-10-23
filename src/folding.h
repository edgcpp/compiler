/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

folding.h -- Declarations relating to folding operations.

*/

/* Avoid including these declarations more than once: */
#ifndef FOLDING_H
#define FOLDING_H 1

#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
The following struct holds a pointer to a class aggregate constant
currently being initialized.  That pointer is used to provide an assumed
value for an enk_param_ref node in an initializer expression for a
subobject that refers to a preceding member of the class object being
initialized.
*/
typedef struct an_aggr_init_con_elem *an_aggr_init_con_elem_ptr;
typedef struct an_aggr_init_con_elem {
  an_aggr_init_con_elem_ptr
		next;	/* When a nested aggregate is being initialized,
			   points to the element for the containing
			   aggregate; NULL otherwise. */
  a_constant_ptr
		constant;
			/* Points to the aggregate constant currently being
			   initialized. */
} an_aggr_init_con_elem;


extern void push_aggr_init_constant(
                                 a_constant_ptr            aggr_con,
                                 an_aggr_init_con_elem_ptr aggr_init_con_elem);

extern void pop_aggr_init_constant(
                                 an_aggr_init_con_elem_ptr aggr_init_con_elem);

extern a_boolean variable_has_non_null_address(a_variable_ptr vp);

extern a_boolean routine_has_non_null_address(a_routine_ptr rp);

extern a_boolean constant_bool_value_known_at_compile_time(a_constant_ptr con);

extern void make_template_param_expr_constant(an_expr_node_ptr node,
                                              a_constant       *con);

extern void force_constant_to_be_dependent(a_constant  *con);

extern void make_template_param_cast_constant(a_constant  *old_constant,
                                              a_constant  *new_constant,
                                              a_type_ptr  new_type,
                                              a_boolean   is_explicit);

extern void implicit_cast(a_constant_ptr cp,
                          a_type_ptr     new_type);

extern void unary_operation(an_expr_operator_kind op,
                            a_constant            *constant,
                            a_type_ptr            result_type,
                            a_constant            *result,
                            a_boolean             constant_context,
                            a_boolean             evaluated_context,
                            a_boolean             *did_not_fold,
                            a_boolean             *template_constant,
                            an_error_code         *error_detected,
                            a_source_position     *err_pos);

extern void binary_operation(an_expr_operator_kind op,
                             a_constant            *constant_1,
                             a_constant            *constant_2,
                             a_type_ptr            result_type,
                             a_constant            *result,
                             a_boolean             constant_context,
                             a_boolean             evaluated_context,
                             a_boolean             *did_not_fold,
                             a_boolean             *template_constant,
                             an_error_code         *error_detected,
                             a_source_position     *err_pos);

extern a_boolean fold_expr(an_expr_node_ptr             expr,
                           a_constant                   *result_con);

extern void check_shift_count(a_constant    *shift_count_constant,
                              a_type_ptr    operand_type,
                              an_error_code *err_code);

extern void conv_integer_to_integer(a_constant        *old_constant,
                                    a_constant        *new_constant,
                                    a_boolean         is_implicit_cast,
                                    an_error_code     *err_code,
                                    an_error_severity *err_severity);

extern a_boolean conv_float_value_to_int_value(
                                 an_internal_float_value  *float_value,
                                 a_float_kind             float_kind,
                                 an_integer_value         *result_value,
                                 a_boolean                is_signed,
                                 a_boolean                *depends_on_fp_mode);

extern void conv_float_to_integer(a_constant        *old_constant,
                                  a_constant        *new_constant,
                                  an_error_code     *err_code,
                                  an_error_severity *err_severity,
                                  a_boolean         *depends_on_fp_mode,
                                  a_boolean         constant_context);
extern
void type_change_constant_full(a_constant        *constant,
                               a_type_ptr        new_type,
                               a_boolean         is_implicit_cast,
                               a_boolean         constant_context,
                               a_boolean         evaluated_context,
                               a_boolean         fold_constant_addr_exprs,
                               a_boolean         is_cli_attr_arg_expression,
                               a_boolean         check_cast_access,
                               a_boolean         check_ambiguity,
                               a_boolean         is_reinterpret_cast,
                               a_boolean         maintain_expression,
                               a_boolean         *did_not_fold,
                               an_error_code     *error_detected,
                               a_source_position *err_pos);

extern void type_change_constant(a_constant        *constant,
                                 a_type_ptr        new_type,
                                 a_boolean         is_implicit_cast,
                                 a_boolean         maintain_expression,
                                 a_boolean         *did_not_fold,
                                 a_source_position *err_pos);

extern a_boolean is_null_pointer_value(a_constant *constant);

extern a_boolean is_false_constant(a_constant *constant);

extern a_boolean is_null_pointer_constant(a_constant *constant);

extern a_boolean is_or_might_be_null_pointer_constant(a_constant *constant);

extern void fold_base_class_cast(a_constant        *constant_1,
                                 a_base_class      *bcp,
                                 a_type_ptr        qualifiers_model,
                                 a_constant        *result,
                                 a_boolean         check_cast_access,
                                 a_boolean         check_ambiguity,
                                 a_boolean         is_implicit_cast,
                                 a_boolean         is_object_pointer,
                                 a_boolean         omit_back_expr,
                                 a_boolean         *did_not_fold,
                                 a_source_position *err_pos,
                                 an_error_code     *error_detected);

extern a_boolean fold_field_selection(a_constant  *constant_1,
                                      a_field_ptr field,
                                      a_type_ptr  result_type,
                                      a_constant  *result);

extern void get_integer_attributes(a_constant      *cp,
                                   an_integer_kind *ikind,
                                   a_boolean       *is_signed,
                                   size_t          *bit_size);

extern void trunc_and_set_integer(an_integer_value  *result_value,
                                  a_constant        *result,
                                  a_boolean         check_overflow,
				  a_boolean	    saturate_on_overflow,
                                  an_error_code     *err_code,
                                  an_error_severity *err_severity);

extern void do_pdiff(a_constant        *constant_1,
                     a_constant        *constant_2,
                     a_constant        *result,
                     a_boolean         *did_not_fold,
                     an_error_code     *err_code,
                     an_error_severity *err_severity);

extern a_boolean compare_address_constants(a_constant_ptr  con1,
                                           a_constant_ptr  con2,
                                           int             *p_cmp);

extern a_boolean compare_address_constants_equality(a_constant_ptr  con1,
                                                    a_constant_ptr  con2,
                                                    int             *p_cmp);

/*
Options for constant_glvalue_address and constant_prvalue_pointer.
*/
typedef int a_constant_address_option_set;
#define CAO_NONE ((a_constant_address_option_set)0x0)
#define CAO_TREAT_LOCAL_VAR_ADDR_AS_CONSTANT \
                            ((a_constant_address_option_set)0x1)
			/* Pretend that local auto variables have
			   constant addresses.  This is used for a gcc
			   folding trick.  Note that address constants created
			   with this option might be invalid and therefore
			   one should be careful not to preserve them in the
			   final IL. */
#define CAO_IS_OBJECT_POINTER \
                            ((a_constant_address_option_set)0x2)
			/* The pointer value being processed is considered to
			   point to an object.  This is significant when
			   folding offsetof, where a zero pointer should be
			   considered an object pointer and not a null pointer
			   constant. */
#define CAO_FOR_LVALUE_MEMBER_ACCESS \
                            ((a_constant_address_option_set)0x4)
			/* The value being processed is the pointer in a
			   member access expression that is used as an
			   lvalue. */

extern a_boolean constant_glvalue_address(an_expr_node_ptr expr,
                                          a_constant       *con,
                                          a_boolean        address_escapes);

extern a_boolean constant_prvalue_pointer_full(
                             an_expr_node_ptr              expr,
                             a_constant                    *con,
                             a_boolean                     address_escapes,
                             a_constant_address_option_set options,
                             a_boolean                     *template_constant);

extern a_boolean constant_prvalue_pointer(an_expr_node_ptr expr,
                                          a_constant       *con,
                                          a_boolean        address_escapes);

extern a_boolean constant_is_pointer_into_string_literal(a_constant *con,
                                                         a_constant **scon);

extern a_boolean constant_is_pointer_to_string_literal(a_constant *con,
                                                       a_constant **scon);

extern a_boolean expr_is_pointer_to_string_literal(an_expr_node_ptr expr,
                                                   a_constant       **scon);

extern a_constant_ptr constant_value_at_address(a_constant_ptr  addr_con,
                                                a_constant_ptr  target_con);

extern a_constant_ptr constant_value_addressed_by_node(an_expr_node_ptr  expr);

extern void fold_builtin_operation_if_possible(
                              an_expr_node_ptr             expr,
                              a_constant_ptr               constant,
                              a_boolean                    maintain_expression,
                              a_source_position            *pos,
                              a_boolean                    *not_a_constant);

#if BUILTIN_FUNCTIONS_ENABLED
extern a_boolean is_foldable_gnu_builtin_function(a_routine_ptr rp,
                                                  a_boolean     *pseudo_call);

extern a_boolean fold_gnu_builtin_function_call_if_possible(
                                                  a_routine_ptr    rp,
                                                  an_expr_node_ptr args,
                                                  an_expr_node_ptr call_expr,
                                                  a_constant       *result_con,
                                                  an_error_code    *err_code);
#endif /* BUILTIN_FUNCTIONS_ENABLED */

extern a_boolean fold_constexpr_expr(an_expr_node_ptr  expr,
                                     a_constant        *result_con,
                                     a_boolean         is_constant_evaluated,
                                     a_boolean         force_prvalue);

extern void add_temp_init_backing_expression(a_constant         *con,
                                             a_dynamic_init_ptr dip);

extern
a_boolean fold_constexpr_ctor(a_dynamic_init_ptr ctor_dip,
                              a_boolean          record_backing_expr,
                              a_boolean          check_constexpr,
                              a_boolean          is_constant_evaluated,
                              a_source_position  *pos,
                              a_constant         *result_con);

extern
a_boolean fold_constexpr_member_selection(an_expr_node_ptr  expr,
                                          a_constant        *result_con);

extern a_boolean is_static_init_constant(a_constant_ptr  con);

#if DEBUG
/* GCOVR_EXCL_START */
extern unsigned long db_show_folding_fe_space_used(unsigned long grand_total);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

extern void folding_one_time_init(void);

extern void folding_trans_unit_init(void);

extern void folding_init(void);

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef FOLDING_H */

