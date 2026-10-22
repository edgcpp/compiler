/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

ifc_modules.h -- Declarations relating to IFC modules made available to the
                 rest of the front end.  For declarations related only to the
                 implementation of IFC modules itself see
                 ifc_modules_internal.h.

*/

/* Avoid including these declarations more than once: */
#ifndef IFC_MODULES_H
#define IFC_MODULES_H 1

#if !STANDALONE_UTILITY_PROGRAM

#include "ifc_map.h"

#include "util.h"

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

typedef struct a_tmpl_decl_state *a_tmpl_decl_state_ptr;

/* FIXME: Temporarily disable "not referenced" warnings until completed. */
/*lint -save -e755 -e758 -e768 -e769*/

typedef uint64_t a_module_ref_key;

/*
Magic numbers that identify the beginning of a Microsoft IFC file.
*/
constexpr a_byte ms_ifc_magic_numbers[] = { 0x54, 0x51, 0x45, 0x1A };

/*
Magic numbers that identify the beginning of an EDG IFC file.
*/
constexpr a_byte edg_ifc_magic_numbers[] = { 0x54, 0x51, 0x45, 0x2C };

namespace detail {

/*
The following specializations provide Is_trivially_copyable and
Is_trivially_destructible support for ifc_map.h types.
*/

template<>
struct Is_trivially_copyable_edg_impl<an_ifc_attr_index> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_copyable_edg_impl */

template<>
struct Is_trivially_copyable_edg_impl<an_ifc_decl_index> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_copyable_edg_impl */

template<>
struct Is_trivially_copyable_edg_impl<an_ifc_edg_basic_token_sort> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_destructible_edg_impl */

template<>
struct Is_trivially_copyable_edg_impl<an_ifc_edg_complex_token_index> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_destructible_edg_impl */

template<>
struct Is_trivially_copyable_edg_impl<an_ifc_type_index> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_copyable_edg_impl */

template<>
struct Is_trivially_destructible_edg_impl<an_ifc_attr_index> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_destructible_edg_impl */

template<>
struct Is_trivially_destructible_edg_impl<an_ifc_decl_index> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_destructible_edg_impl */

template<>
struct Is_trivially_destructible_edg_impl<an_ifc_edg_basic_token_sort> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_destructible_edg_impl */

template<>
struct Is_trivially_destructible_edg_impl<an_ifc_edg_complex_token_index> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_destructible_edg_impl */

template<>
struct Is_trivially_destructible_edg_impl<an_ifc_type_index> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_destructible_edg_impl */

}  /* namespace detail */

template<typename an_ifc_Index_type>
extern an_ifc_Index_type from_lexical_index(a_lexical_ifc_index_reference idx);

template<typename an_ifc_Index_type>
extern a_lexical_ifc_index_reference to_lexical_index(an_ifc_Index_type idx);

extern Opt<a_string> get_name_of_ifc_module(a_const_char *file_name);

extern void process_ifc_declaration(a_module_entity_ptr mep);

extern a_boolean has_variable_initializer_from_ifc_module(a_variable_ptr  vp);

extern a_boolean load_variable_initializer_from_ifc_module(a_variable_ptr  vp);

extern a_boolean has_routine_definition_from_ifc_module(a_routine_ptr  rp);

extern a_boolean load_routine_definition_from_ifc_module(a_routine_ptr  rp);

extern a_boolean has_template_definition_from_ifc_module(a_template_ptr templ);

extern
a_boolean load_template_definition_from_ifc_module(a_template_ptr  templ);

extern a_boolean has_template_specializations_from_ifc_module(
                                                        a_template_ptr  templ);

extern
a_boolean load_template_specializations_from_ifc_module(a_template_ptr  templ);

extern a_boolean has_type_definition_from_ifc_module(a_type_ptr  ty);

extern a_boolean load_type_definition_from_ifc_module(a_type_ptr  ty);

extern a_module_entity_ptr locate_ifc_module_entity(
                                                   a_module_entry_locator loc);

extern void load_namespace_elements_from_ifc_locator(
                                                   a_module_entity_ptr    mep,
                                                   a_module_entry_locator loc);

extern void update_entity_from_new_ifc_locator(a_module_entity_ptr    mep,
                                               a_module_entry_locator new_loc);

extern Opt<a_source_position> source_position_from_ifc_of(
                                                      a_module_entity_ptr mep);

extern a_dynamic_init_ptr load_variable_init_from_ifc_module(
                                                a_type_ptr        tp,
                                                an_ifc_expr_index init_expr);

extern a_boolean extract_tokens_for_ifc_module_expr(
                              a_lexical_ifc_index_reference *index,
                              a_token_sequence_number       *expected_end_tsn);

extern void record_symbol_for_ifc_decl(a_symbol_ptr  sym);

extern a_symbol_ptr load_tok_ifc_entity_ref();

extern a_symbol_ptr load_tok_ifc_decl_ref();

extern a_type_ptr load_tok_ifc_type_ref();

extern void scan_ifc_param_ref_expr(an_operand *result);

extern a_boolean import_ifc_module_file(a_module_import_decl_ptr midp);

extern void ifc_modules_report_suppressed_diagnostics();

extern void ifc_modules_pch_read_reset();

extern void ifc_modules_one_time_init();

extern void require_ifc_modules();

extern void ifc_modules_trans_unit_init();

extern void ifc_modules_trans_unit_wrapup();

#if MAKE_FRONT_END_CALLABLE
extern void ifc_modules_cleanup();
#endif /* MAKE_FRONT_END_CALLABLE */

extern void ifc_modules_write_out();

#if DEBUG
/* GCOVR_EXCL_START */

extern a_string s_db_ifc_locator(a_module_entry_locator loc);

extern a_string s_db_id_of_ifc_mep(a_module_entity_ptr mep);

extern a_string s_db_lexical_ifc_index(a_lexical_ifc_index_reference idx);

extern void db_mep_stack();

extern void db_node_at_tsn(a_token_cache_ptr        cache,
                           a_token_sequence_number  tsn);

extern void db_node_at_tsn(a_module_token_cache_ptr cache,
                           a_token_sequence_number  tsn);

extern void db_locus(const an_ifc_source_location &locus);

/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

/*lint -restore*/ /* FIXME: temporary. */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* !STANDALONE_UTILITY_PROGRAM */

#endif /* ifndef IFC_MODULES_H */

