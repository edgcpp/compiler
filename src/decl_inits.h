/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/* 
decl_inits.h -- Declarations related to decl_inits.c (having to do with
                initializers in declarations).

*/

/* Avoid including these declarations more than once: */
#ifndef DECL_INITS_H
#define DECL_INITS_H 1

#ifndef DECLS_H
#include "decls.h"
#endif /* ifndef DECLS_H */
#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

extern void update_array_var_type_from_initializer_constant(
                                                         a_variable_ptr  var);

extern a_boolean check_string_constant_initializer_full(
                                                   a_type_ptr      *dst_type,
                                                   a_constant_ptr  string_con,
                                                   a_boolean       *excess);

#define check_string_constant_initializer(dst_type, string_con)              \
  (check_string_constant_initializer_full(dst_type, string_con,              \
                                          (a_boolean*)NULL))

extern void repeat_nonconstant_init(a_dynamic_init_ptr  ctor_dip,
                                    a_type_ptr          array_type,
                                    a_type_ptr          elem_type,
                                    a_dynamic_init_ptr  new_dip,
                                    a_targ_size_t       count);

extern void prep_aggr_initializer(an_init_component_ptr        icp,
                                  a_type_ptr                   *p_type,
                                  an_init_state                *is,
                                  struct an_arg_match_summary  *arg_match,
                                  a_boolean                    fill_in_dtor);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern void aggr_init_cli_array(an_init_component_ptr  icp,
                                a_type_ptr             hatype,
                                an_init_state          *is,
                                a_dynamic_init_ptr     *result,
                                an_expr_node_ptr       *dim_exprs);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_dynamic_init_ptr prep_array_variable_initializer(
                                                a_decl_parse_state  *dps,
                                                an_operand          *operand);

extern void initializer(a_decl_parse_state  *state,
                        a_source_position   *source_pos,
                        an_id_linkage_kind  linkage,
                        a_boolean           paren_flag,
                        a_boolean           *incomplete_type_error_reported,
                        a_decl_pos_block    *decl_pos_block);

extern
void record_struct_binding_expr_for_tuple_element(a_variable_ptr     binding,
                                                  an_init_component  *icp);

extern a_constant_ptr aggr_init_constant_from_field_initializer(
                                                 a_field_ptr        fp,
                                                 a_dynamic_init     *dip,
                                                 a_type_ptr         aggr_type,
                                                 an_init_state      *is,
                                                 a_source_position  *diag_pos);

extern void field_initializer(a_decl_parse_state  *dps);

extern a_field_ptr curr_initializer_field(void);

extern a_shared_token_cache cache_inclass_initializer(a_symbol_ptr  sym);

#if NEED_NAME_MANGLING
extern a_discriminator get_discriminator_for_field_initializer(void);
#endif /* NEED_NAME_MANGLING */

extern void init_capture_initializer(a_lambda_capture  *lcp,
                                     a_decl_parse_state  *dps);

extern a_boolean def_initializer(a_symbol_ptr       sym,
                                 a_source_position  *err_pos);

extern a_constructor_init_ptr ctor_initializer(a_routine_ptr  ctor_rout,
                                               a_boolean      user_defined,
                                               a_boolean      fields_only);

extern a_constructor_init_ptr ctor_inits_for_inheriting_ctor(
                                                           a_routine_ptr ctor);

extern a_constructor_init_ptr dtor_initializer(a_routine_ptr  dtor_rout);

extern void check_for_missing_initializer_full(
                                           a_symbol_ptr  sym,
                                           a_type_ptr    type,
                                           a_boolean     explicitly_internal,
                                           a_boolean     *err);

#define check_for_missing_initializer(sym, type)                             \
  (check_for_missing_initializer_full(sym, type,                             \
                                      /*explicitly_internal=*/FALSE,         \
                                      (a_boolean*)NULL))

extern
void scan_compound_literal_initializer(a_decl_parse_state  *dps,
                                       an_init_component  *rescan_aggr,
                                       an_init_component  **return_icp);

extern a_hash_value hash_void_pointer(a_void_ptr  p);

extern a_boolean compare_for_pointer_pair_map(a_void_ptr  p1,
                                              a_void_ptr  p2);

extern void decl_inits_one_time_init(void);

extern void decl_inits_trans_unit_init(void);

extern void decl_inits_init(void);

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef DECL_INITS_H */

