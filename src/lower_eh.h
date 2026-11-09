/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

lower_eh.h -- Declarations related to lower_eh.c (having to do with IL
              lowering for exception handling constructs).

*/

/* Avoid including these declarations more than once: */
#ifndef LOWER_EH_H
#define LOWER_EH_H 1

/* Only include this code if it is needed: */
#if DO_IL_LOWERING

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */
#ifndef LOWER_IL_H
#include "lower_il.h"
#endif /* ifndef LOWER_IL_H */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

extern a_type_ptr make_runtime_typeinfo_type(void);

extern a_type_ptr make_typeinfo_type(a_type_info_kind kind, 
                                     a_type_ptr       type);

extern void generate_typeinfo_vars(void);

extern a_variable_ptr make_typeinfo_var(a_type_ptr type);

extern a_variable_ptr get_typeinfo_var(a_type_ptr type);

#if ABI_CHANGES_FOR_RTTI
EXTERN_THREAD a_variable_ptr 
		vtbls_for_type_info[(int)tik_last];
			/* The variables for the virtual function tables for
			   the user-visible typeinfo types, once created.
			   NULL until then. */
extern void lower_typeid(an_expr_node_ptr expr);

extern a_type_info_kind is_type_info_type(a_type_ptr type);

#if CHECKING
extern a_boolean is_generated_typeinfo_type(a_type_ptr  type);
#endif /* CHECKING */

extern a_type_info_kind is_type_info_vtbl(a_variable_ptr vtbl);
#endif /* ABI_CHANGES_FOR_RTTI */

#if GENERATE_EH_TABLES
/*
Value used to indicate "no region number" for exception handling regions.
It's all one bits, truncated to fit in a TARG_REGION_NUMBER_INT_KIND integer.
*/
EXTERN_THREAD a_cleanup_region_number
		null_eh_region_number;


#if DO_FULL_PORTABLE_EH_LOWERING
extern a_handle_number object_addr_table_index(void);

extern void init_object_addr_table_entry(
                                       an_init_pos_descr_ptr ipdp,
                                       a_handle_number       entry_number,
                                       an_insert_location    *insert_location);
#endif /* DO_FULL_PORTABLE_EH_LOWERING */

extern a_cleanup_region_number cleanup_region_number(a_dynamic_init_ptr dip);

extern void make_dyn_init_region_table_entry(
                                          a_dynamic_init_ptr dip,
                                          a_dynamic_init_ptr next_dip,
                                          an_insert_location *insert_location);

extern void clone_region_table_entry_list(a_dynamic_init_ptr dip,
                                          a_dynamic_init_ptr stop_before);

#endif /* GENERATE_EH_TABLES */

extern a_boolean has_destructions(an_object_lifetime_ptr lifetime);

extern void add_eh_function_prologue(a_scope_ptr scope);

extern an_expr_node_ptr make_caught_object_address_node(void);

extern void begin_catch_clause(a_handler_ptr handler);

extern void cleanup_on_exit_from_try_block(
                                        a_context_ptr        context_ptr,
                                        a_try_supplement_ptr try_block,
                                        an_insert_location   *insert_location);

extern void cleanup_on_exit_from_catch(a_handler_ptr      handler,
                                       an_insert_location *insert_location);

extern void lower_try_block(
                         a_statement_ptr                 statement,
                         a_boolean                       is_function_try_block,
                         a_destructor_wrapper_info_block *dtor_info);

extern an_expr_node_ptr make_internal_try_expr(an_expr_node_ptr try_expr,
                                               an_expr_node_ptr catch_expr);

#if DO_FULL_PORTABLE_EH_LOWERING
extern a_boolean is_eh_setjmp_routine(a_routine_ptr routine);
#endif /* DO_FULL_PORTABLE_EH_LOWERING */

#if !DO_FULL_PORTABLE_EH_LOWERING
extern an_expr_node_ptr make_thrown_object_address_node(void);
#endif /* !DO_FULL_PORTABLE_EH_LOWERING */

extern void lower_throw(an_expr_node_ptr expr);

#if ABI_COMPATIBILITY_VERSION >= 233
extern void record_exception_started(an_insert_location *insert_location);
#endif /* ABI_COMPATIBILITY_VERSION >= 233 */

extern void insert_code_to_indicate_cleanup_state(
                                           a_dynamic_init_ptr cleanup_state,
                                           an_insert_location *insert_location,
                                           a_boolean          unreachable);

/*
Data structure used to save state information for lowering of exception
handling.
*/
typedef struct an_eh_lowering_context {
#if DO_FULL_PORTABLE_EH_LOWERING
  a_variable_ptr
		object_addr_table_var;
#define AT_LEAST_ONE_FIELD_IN_AN_EH_LOWERING_CONTEXT 1
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
#if GENERATE_EH_TABLES
  a_variable_ptr
		array_table_var;
  a_constant_ptr
		array_table_aggr_con;
  a_variable_ptr
		region_table_var;
  a_constant_ptr
		region_table_aggr_con;
  a_cleanup_region_number
		next_avail_region_number;
#ifndef AT_LEAST_ONE_FIELD_IN_AN_EH_LOWERING_CONTEXT
#define AT_LEAST_ONE_FIELD_IN_AN_EH_LOWERING_CONTEXT 1
#endif /* ifndef AT_LEAST_ONE_FIELD_IN_AN_EH_LOWERING_CONTEXT */
#endif /* GENERATE_EH_TABLES */
#ifndef AT_LEAST_ONE_FIELD_IN_AN_EH_LOWERING_CONTEXT
  char		dummy;	/* Dummy field if structure would otherwise be
			   empty. */
#endif /* ifndef AT_LEAST_ONE_FIELD_IN_AN_EH_LOWERING_CONTEXT */
} an_eh_lowering_context;

extern void save_eh_lowering_context(an_eh_lowering_context *ehcontext);

extern void restore_eh_lowering_context(an_eh_lowering_context *ehcontext);

extern void eh_function_lower_init(void);

extern void eh_lower_one_time_init(void);

extern void eh_lower_trans_unit_init(void);

extern void eh_lower_init(void);

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* DO_IL_LOWERING */
#endif /* ifndef LOWER_EH_H */

