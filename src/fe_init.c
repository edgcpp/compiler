/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

fe_init.c -- Initialization for the front end.

*/

#ifdef PCH_PRAGMA_GUARD
/* Suppress generation of a precompiled header file -- fe_init.c cannot
   share its precompiled header with any other file.  (The only utility from
   generating a precompiled header file would be for recompilation; for
   that, the no_pch pragma should be removed and a hdrstop pragma added
   after the #include of fe_common.h.)  */
#pragma no_pch
#endif /* PCH_PRAGMA_GUARD */

/*
Force definition in this compilation of external variables declared
in .h files.
*/
#define VAR_INITIALIZERS 1

#include "basic_hdrs.h"
#include "fe_common.h"
#include "fe_init.h"
#if MICROSOFT_EXTENSIONS_ALLOWED
#include "ms_metadata.h"
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#include "ifc_modules.h"
#if NEED_IL_DISPLAY
#include "il_display.h"
#endif /* NEED_IL_DISPLAY */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

#if STANDALONE_UTILITY_PROGRAM

END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
/* Header files used by standalone utility programs. */
#include "il_walk.h"
#if IL_SHOULD_BE_WRITTEN_TO_FILE
#include "il_file.h"
#include "il_read.h"
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */

void standalone_utility_early_init(void)
/*
Early initialization of global variables that may be used by standalone utility
programs.  Note that the il_header has not been read yet, and as such the
front end configuration is not yet known (i.e., the various emulation modes
and any run-time target configuration), so these routines cannot rely on
those being set.
*/
{
#if CHECKING
  il_header_has_been_read = FALSE;
#endif /* CHECKING */
  mem_manage_early_init();
  host_envir_early_init();
  target_early_init();
  error_early_init();
}  /* standalone_utility_early_init */


void standalone_utility_late_init(void)
/*
Additional initialization of global variables that may be used by standalone
utility programs.  This routine should only be called after il_read has been
called (and init_flags_and_types has parsed the information in il_header).
*/
{
  check_assertion(il_header_has_been_read);
  target_one_time_init();
  il_to_str_one_time_init();
  error_one_time_init();
  target_init();
  il_to_str_init();
  error_init();
  float_pt_init();
#if !USE_HOST_FP_CONVERSION_ROUTINES || \
    (USE_FLOAT128_FOR_HOST_FP_VALUE && !USE_QUADMATH_LIBRARY)
  floating_one_time_init();
#endif /* !USE_HOST_FP_CONVERSION_ROUTINES || (USE_FLOAT128...) */
}  /* standalone_utility_late_init */

#else /* !STANDALONE_UTILITY_PROGRAM */

#if __BSD__
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
#include <sys/time.h>
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */
#else  /* !__BSD__ */
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
#include <time.h>
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */
#endif  /* __BSD__ */
#if __SYSV__ || __BSD__
EXTERN_C time_t time(time_t *timer);
#endif /* __SYSV__ || __BSD__ */

END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
#include <errno.h>
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */

/*
Note: EVERY .h file that includes an external variable must be included in
fe_init.c.  (Those which are already specified in fe_common.h are omitted in
the following list.)  By defining the macro EXTERN as an empty string, the
declarations in the include files will become external definitions for the
symbols.  il.h, symbol_tbl.h, lexical.h, and types.h will already have
been included by the inclusion of fe_common.h.
*/
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
/*lint --e{766}*/ /* <-- No warning in this file on unneeded includes. */
#include "class_decl.h"
#include "decl_inits.h"
#include "decl_spec.h"
#include "decls.h"
#include "declarator.h"
#include "def_arg.h"
#include "expr.h"
#include "exprutil.h"
#include "fe_init.h"
#include "fe_wrapup.h"
#include "folding.h"
#include "interpret.h"
#include "layout.h"
#include "lexical.h"
#include "literals.h"
#include "macro.h"
#include "ms_attrib.h"
#include "overload.h"
#include "pch.h"
#include "pragma.h"
#include "preproc.h"
#include "statements.h"
#include "symbol_ref.h"
#include "sys_predef.h"
#include "templates.h"
#include "trans_copy.h"
#include "trans_corresp.h"

#if IL_WALK_NEEDED || MAINTAIN_NEEDED_FLAGS
#include "il_walk.h"
#endif /* IL_WALK_NEEDED || MAINTAIN_NEEDED_FLAGS */

#if IL_SHOULD_BE_WRITTEN_TO_FILE
#include "il_file.h"
#include "il_write.h"
#if BACK_END_SHOULD_BE_CALLED
#include "il_read.h"
#endif /* BACK_END_SHOULD_BE_CALLED */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
#if BACK_END_IS_C_GEN_BE
#include "c_gen_be.h"
#endif /* BACK_END_IS_C_GEN_BE */
#if BACK_END_IS_CP_GEN_BE
#include "cp_gen_be.h"
#endif /* BACK_END_IS_CP_GEN_BE */

#if NEED_NAME_MANGLING
#include "lower_il.h"
#include "lower_name.h"
#endif /* NEED_NAME_MANGLING */
#if DO_IL_LOWERING
#include "lower_init.h"
#include "lower_eh.h"
#if MINIMAL_INLINING
#include "inline.h"
#endif /* MINIMAL_INLINING */
#include "lower_c99.h"
#endif /* DO_IL_LOWERING */

#if !USE_HOST_FP_CONVERSION_ROUTINES || \
    (USE_FLOAT128_FOR_HOST_FP_VALUE && !USE_QUADMATH_LIBRARY)
/* Internal floating point conversion routines. */
#include "floating.h"
#endif /* !USE_HOST_FP_CONVERSION_ROUTINES || (USE_FLOAT128...) */
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */

static void last(void)
/*
Dummy function to fill last entry in function_pointers array.
*/
{
}  /* last */


/*
Statically initialize the list of function pointers that corresponds to
the a_function_number enumeration.  This is a list of functions that are
referred to by data structures that might appear in a pre-compiled header file,
and therefore whose address might change from one invocation of the front end
to another on operating systems that implement Address Space Layout
Randomization (ASLR).
*/

CONSTINIT_ARRAY(/* none */, a_function_pointer, function_pointers, fn_last + 1)
#if VAR_INITIALIZERS
= {
  (a_function_pointer)NULL,              /* fn_null */
  (a_function_pointer)hash_attribute_kind,
  (a_function_pointer)compare_for_attr_corresp_checking_map,
  (a_function_pointer)hash_source_string,
  (a_function_pointer)compare_for_attr_name_map,
#if GNU_EXTENSIONS_ALLOWED
  (a_function_pointer)compare_for_asm_name_map,
#endif /* GNU_EXTENSIONS_ALLOWED */
  (a_function_pointer)hash_include_search_result,
  (a_function_pointer)compare_include_search_result,
  (a_function_pointer)hash_include_file_history,
  (a_function_pointer)compare_include_file_history,
#if UNIQUE_FILE_IDENTIFIER_AVAILABLE
  (a_function_pointer)hash_unique_file_id_for_table,
  (a_function_pointer)compare_unique_file_id,
#endif /* UNIQUE_FILE_IDENTIFIER_AVAILABLE */
#if MICROSOFT_EXTENSIONS_ALLOWED
  (a_function_pointer)hash_include_alias,
  (a_function_pointer)compare_include_alias,
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  (a_function_pointer)hash_instantiation,
  (a_function_pointer)compare_instantiation,
#if MICROSOFT_EXTENSIONS_ALLOWED
  (a_function_pointer)hash_prop_or_event_accessor_header_lookup,
  (a_function_pointer)compare_prop_or_event_accessor_header_lookup,
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  (a_function_pointer)hash_symbol_header_lookup_entry,
  (a_function_pointer)compare_symbol_header_lookup_entry,
  (a_function_pointer)record_arg_pragma,
  (a_function_pointer)instantiation_pragma,
  (a_function_pointer)pack_pragma,
#if IDENT_DIRECTIVE_AND_PRAGMA
  (a_function_pointer)ident_pragma,
  (a_function_pointer)ident_directive,
#endif /* IDENT_DIRECTIVE_AND_PRAGMA */
#if PRAGMA_WEAK_ALLOWED
  (a_function_pointer)weak_pragma,
#endif /* PRAGMA_WEAK_ALLOWED */
  (a_function_pointer)once_pragma,
  (a_function_pointer)hdrstop_or_no_pch_pragma,
  (a_function_pointer)define_type_info_pragma,
  (a_function_pointer)stdc_pragma,
#if UPC_EXTENSIONS_ALLOWED
  (a_function_pointer)upc_pragma,
#endif /* UPC_EXTENSIONS_ALLOWED */
#if REDEFINE_EXTNAME_PRAGMA_ENABLED
  (a_function_pointer)redefine_extname_pragma,
#endif /* REDEFINE_EXTNAME_PRAGMA_ENABLED */
#if SUN_EXTENSIONS_ALLOWED
  (a_function_pointer)ldscope_pragma,
#endif /* SUN_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  (a_function_pointer)gcc_pragma,
#if GNU_VECTOR_TYPES_ALLOWED && BUILTIN_FUNCTIONS_ENABLED
  (a_function_pointer)gnu_riscv_pragma,
  (a_function_pointer)clang_riscv_pragma,
#endif /* GNU_VECTOR_TYPES_ALLOWED && BUILTIN_FUNCTIONS_ENABLED */
#endif /* GNU_EXTENSIONS_ALLOWED */
  (a_function_pointer)diag_pragma,
  (a_function_pointer)diagnostic_pragma,
#if INCLUDE_EDG_TEST_PRAGMAS
  (a_function_pointer)test_immediate_pragma,
  (a_function_pointer)test_next_construct_pragma,
#endif /* INCLUDE_EDG_TEST_PRAGMAS */
#if DEBUG
  (a_function_pointer)db_opt_pragma,
  (a_function_pointer)db_name_pragma,
#endif /* DEBUG */
#if NEED_IL_DISPLAY
  (a_function_pointer)pragma_il_display,
#endif /* NEED_IL_DISPLAY */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
  (a_function_pointer)if_exists_pragma,
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
  (a_function_pointer)push_macro_pragma,
  (a_function_pointer)pop_macro_pragma,
#if MICROSOFT_EXTENSIONS_ALLOWED
  (a_function_pointer)microsoft_start_map_region_pragma,
  (a_function_pointer)microsoft_stop_map_region_pragma,
#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
  (a_function_pointer)setlocale_pragma,
#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
  (a_function_pointer)microsoft_comment_pragma,
  (a_function_pointer)microsoft_conform_pragma,
  (a_function_pointer)microsoft_include_alias_pragma,
  (a_function_pointer)hash_unresolved_type_map_key,
  (a_function_pointer)compare_for_unresolved_type_map,
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  (a_function_pointer)hash_void_pointer,
  (a_function_pointer)compare_for_pointer_pair_map,
  (a_function_pointer)compare_substituted_type_list_entry,
  (a_function_pointer)hash_token_sequence_xref,
  (a_function_pointer)compare_token_sequence_xref,
#if UNICODE_VULNERABILITY_DETECTION_SUPPORTED
  (a_function_pointer)hash_id_representation,
  (a_function_pointer)id_representations_match,
#endif /* UNICODE_VULNERABILITY_DETECTION_SUPPORTED */
  (a_function_pointer)hash_name_reference,
  (a_function_pointer)compare_name_reference,
#if CREATE_LEXICAL_TYPEREFS
  (a_function_pointer)hash_type_and_name_qualifier,
  (a_function_pointer)compare_type_and_name_qualifier,
  (a_function_pointer)hash_type_and_template_arg_list,
  (a_function_pointer)compare_type_and_template_arg_list,
#endif /* CREATE_LEXICAL_TYPEREFS */
  (a_function_pointer)last               /* fn_last */
}
#endif /* VAR_INITIALIZERS */
CONSTINIT_ARRAY_END(function_pointers)

/*
Date/time of compilation, in ctime format ("Sun Sep 16 01:03:52 1973\n"):
*/
STATIC_THREAD char
		curr_date_time[128];


static void host_init(void)
/*
Do required initialization for host-dependent things.
*/
{
#if CHECKING
  /* Check that CHAR_MIN is set right for the host char signedness. */
  { char c;
#if CHAR_MIN == 0
    /* Host should have unsigned characters. */
    c = (1 << CHAR_BIT) - 1;
    if (c < 0) internal_error("host_init: CHAR_MIN in basics.h is set wrong");
#else /* CHAR_MIN != 0 */
    /* Host should have signed characters. */
    c = -1;
    if (c > 0) { /*lint !e774*/
      internal_error("host_init: CHAR_MIN in basics.h is set wrong");
    }  /* if */
#endif /* CHAR_MIN == 0 */
  }
#endif /* CHECKING */

#if CHAR_MAX-CHAR_MIN != ((1 << CHAR_BIT) - 1)
    /* Check that CHAR_MIN and CHAR_MAX add up to the right power of two. */
 #error -- CHAR_MIN or CHAR_MAX in basics.h is set wrong
#endif /* CHAR_MAX ... */

  /* Generate the object file name from the primary source file name.
     This name is used in generating makefile dependency lines. */
  object_file_name = derived_name(primary_source_file_name,
                                  OBJECT_FILE_SUFFIX);
}  /* host_init */


static a_type_ptr make_and_enter_align_val_type(void)
/*
Create a symbol and type for std::align_val_t and enter it into namespace
std.
*/
{
  a_type_ptr       type;
  a_symbol_locator loc;
  a_symbol_ptr     sym;
  a_const_char     *name = "align_val_t";
  a_namespace_ptr  std_namespace;
  a_type_ptr       size_t_type = integer_type(targ_size_t_int_kind);

  /* Create the align_val_t type: a scoped enumeration based on size_t. */
  type = alloc_type((a_type_kind)tk_enum);
  type->source_corresp.name_linkage =
                                   (a_name_linkage_kind)nlk_cplusplus_external;
  type->size = size_t_type->size;
  type->variant.integer.int_kind = targ_size_t_int_kind;
  type->variant.integer.enum_type = TRUE;
  type->variant.integer.is_scoped_enum = TRUE;
  type->variant.integer.has_explicit_enum_base = TRUE;
  type->variant.integer.extra_info->base_type = size_t_type;
  /* Create a symbol for align_val_t. */
  clear_locator(&loc, &null_source_position);
  (void) find_symbol(name, (sizeof_t)strlen(name), &loc);
  sym = alloc_symbol((a_symbol_kind)sk_enum_tag, loc.symbol_header,
                     &null_source_position);
  set_source_corresp(&type->source_corresp, sym);
  sym->variant.enumeration.type = type;
  /* Add the type to namespace std. */
  std_namespace = symbol_for_namespace_std->variant.namespace_info.ptr;
  (void)push_namespace_scope((a_scope_kind)sck_namespace_extension,
                             std_namespace);
  enter_predeclared_class(type, depth_scope_stack, &null_source_position);
  pop_namespace_scope();
  /* Make sure it cannot be used until actually declared.  g++ and
     Microsoft make the name visible in modes where aligned new is
     allowed. */
  sym->is_invisible = !(gpp_mode || microsoft_mode);
  return type;
}  /* make_and_enter_align_val_type */


static inline void predeclare_std_type_info(void)
/*
Pre-declare type_info in the std namespace.
*/
{
  a_namespace_ptr  std_namespace =
                          symbol_for_namespace_std->variant.namespace_info.ptr;
  (void)push_namespace_scope(sck_namespace_extension, std_namespace);
  enter_predeclared_class(type_of_type_info, depth_scope_stack,
                          &null_source_position);
  pop_namespace_scope();
}  /* predeclare_std_type_info */


static void predeclare_entities(void)
/*
Several modes "pre-declare" various entities.  For example, in C++ mode,
namespace std is commonly predeclared (as are some of its members, like
type_info).  Other platform-specific entities are also be predeclared here.
(Additional custom pre-declarations can be added in the function
enter_system_specific_predeclared_symbols; see sys_predef.c.)
*/
{
  if (!C_mode()) {
    a_boolean need_std = namespaces_enabled || type_info_in_namespace_std;
    int       i;
#if RUNTIME_USES_NAMESPACES
    need_std = TRUE;
#endif /* RUNTIME_USES_NAMESPACES */
    /* coverity[dead_error_line] */
    if (need_std || ignore_std_namespace ||
        va_list_in_std_namespace) {  /*lint !e774*/
      /* Predeclare namespace "std" and create a symbol for it.  Note that
         the symbol is not actually added to the symbol table until namespace
         "std" is explicitly declared (unless the --ignore_std option is
         used or we are in g++ mode). */
      make_symbol_for_namespace_std();
      if (ignore_std_namespace || gpp_mode || sun_mode || microsoft_mode) {
        /* In --ignore_std mode, enter "std" so it can be used as a
           synonym for the global namespace.  In g++ and Sun modes, "std" is
           predeclared. */
        clear_locator(&locator_for_curr_id, &null_source_position);
        enter_symbol_for_namespace_std(&locator_for_curr_id);
      }  /* if */
      /* Ensure a symbol header exists for "initializer_list".  This permits
         efficient identification of the std::initializer_list template. */
      make_symbol_header_for_initializer_list();
    }  /* if */
#if IA64_ABI
    /* Predeclare the namespace defined by the IA-64 ABI, which contains
       the derived classes of type_info, among other things.   As with
       namespace std, the symbol is not entered in the symbol table until
       namespace std is explicitly defined, except in GNU C++ mode. */
    make_symbol_for_namespace_abi();
    if (gpp_mode) {
      clear_locator(&locator_for_curr_id, &null_source_position);
      enter_symbol_for_namespace_abi(&locator_for_curr_id);
    }  /* if */
#endif /* IA64_ABI */
    /* This is done even when RTTI is not enabled because the type_info
       struct may still be defined when RTTI is disabled. */
    for (i = 0; i < (int)tik_last; ++i) {
      if (type_info_names[i] != NULL) {
        types_of_type_info[i] = init_predeclared_class((a_type_kind)tk_class,
                                                       type_info_names[i]);
      }  /* if */
    }  /* for */
    type_of_type_info = types_of_type_info[(int)tik_user];
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (ms_compat) {
      /* Microsoft compilers (and Clang, with -fms-compatibility) make the
         (incomplete) class type_info visible in the global namespace.  We
         emulate this only if type_info is configured to reside in the global
         namespace. */
      if (!type_info_in_namespace_std || ignore_std_namespace) {
        enter_predeclared_class(type_of_type_info, DEPTH_OF_FILE_SCOPE,
                                &null_source_position);
      }  /* if */
    }  /* if */
    if (ms_extensions) {
      type_of_guid = init_predeclared_class((a_type_kind)tk_struct, "_GUID");
      enter_predeclared_class(type_of_guid, DEPTH_OF_FILE_SCOPE,
                              &null_source_position);
      if (nullptr_enabled) {
        /* Microsoft compilers that support nullptr predeclare
           std::nullptr_t. */
        make_predeclared_nullptr_t_symbol();
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
    if (gpp_mode && !clang_mode && symbol_for_namespace_std != NULL) {
      /* g++ pre-declares std::type_info as an incomplete type (clang does
         not). */
      if (type_info_in_namespace_std) {
        if (ignore_std_namespace) {
          /* The std namespace is to be viewed as a synonym for the global
             namespace, so type_info should be predeclared there rather
             than as a member of std. */
          enter_predeclared_class(type_of_type_info, DEPTH_OF_FILE_SCOPE,
                                  &null_source_position);
        } else {
          /* The type should be a member of namespace std. */
          predeclare_std_type_info();
        }  /* if */
      }  /* if */
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    if (overaligned_allocation_enabled) {
      /* Create the symbol and type for std::align_val_t. */
      type_of_align_val_t = make_and_enter_align_val_type();
    }  /* if */
    /* Add symbols for ::operator new and ::operator delete to the symbol
       table.  This is delayed till now (rather than done with other symbol
       table initialization) because routine entries are also created. */
    make_global_operator_new_or_delete_symbol((an_opname_kind)onk_new,
                                              /*sized_version=*/FALSE,
                                              /*aligned_version=*/FALSE);
    make_global_operator_new_or_delete_symbol((an_opname_kind)onk_delete,
                                              /*sized_version=*/FALSE,
                                              /*aligned_version=*/FALSE);
    if (sized_deallocation_enabled) {
      make_global_operator_new_or_delete_symbol((an_opname_kind)onk_delete,
                                                /*sized_version=*/TRUE,
                                                /*aligned_version=*/FALSE);
      if (overaligned_allocation_enabled) {
        /* Add the aligned versions of both allocation and deallocation
           functions (which are only enabled when sized deallocation
           support is enabled). */
        make_global_operator_new_or_delete_symbol((an_opname_kind)onk_new,
                                                  /*sized_version=*/FALSE,
                                                  /*aligned_version=*/TRUE);
        make_global_operator_new_or_delete_symbol((an_opname_kind)onk_delete,
                                                  /*sized_version=*/TRUE,
                                                  /*aligned_version=*/TRUE);
        make_global_operator_new_or_delete_symbol((an_opname_kind)onk_delete,
                                                  /*sized_version=*/FALSE,
                                                  /*aligned_version=*/TRUE);
      }  /* if */
    }  /* if */
    if (!ms_extensions && array_new_and_delete_enabled) {
      /* Add symbols for the array versions, too.  (Although this is not done
         explicitly in Microsoft mode, the symbols are sometimes created
         implicitly when the corresponding non-array versions are created.) */
      make_global_operator_new_or_delete_symbol((an_opname_kind)onk_array_new,
                                                /*sized_version=*/FALSE,
                                                /*aligned_version=*/FALSE);
      make_global_operator_new_or_delete_symbol(
                                             (an_opname_kind)onk_array_delete,
                                             /*sized_version=*/FALSE,
                                             /*aligned_version=*/FALSE);
      if (sized_deallocation_enabled) {
        make_global_operator_new_or_delete_symbol(
                                             (an_opname_kind)onk_array_delete,
                                             /*sized_version=*/TRUE,
                                             /*aligned_version=*/FALSE);
        if (overaligned_allocation_enabled) {
          make_global_operator_new_or_delete_symbol(
                                                 (an_opname_kind)onk_array_new,
                                                 /*sized_version=*/FALSE,
                                                 /*aligned_version=*/TRUE);
          make_global_operator_new_or_delete_symbol(
                                              (an_opname_kind)onk_array_delete,
                                              /*sized_version=*/TRUE,
                                              /*aligned_version=*/TRUE);
          make_global_operator_new_or_delete_symbol(
                                              (an_opname_kind)onk_array_delete,
                                              /*sized_version=*/FALSE,
                                              /*aligned_version=*/TRUE);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  /* Enter other predeclared symbols, as required by the implementation. */
  enter_system_specific_predeclared_symbols();
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ms_extensions) {
    if (C_mode()) {
      /* Add a symbol for predeclared _alloca (C mode only). */
      make_predeclared_alloca_symbol();
    } else {
      /* Add a symbol for predeclared size_t (C++ mode only). */
      make_predeclared_size_t_symbol();
      if (cli_or_cx_enabled) {
        /* Add symbol for ::cli namespace. */
        if (cppcx_enabled) {
          /* In C++/CX mode, the counterpart to the "cli" namesapce is the 
             "default" namespace, which is located within platform.winmd, so
             it is not created explicitly here. */
        } else {
          make_symbol_for_namespace_cli();
        }  /* if */
      }  /* if */
    }  /* if */
    if (bool_is_keyword && microsoft_version < 1310) {
      /* MSVC++ 6.0 and 7.0 treat "bool" as a predeclared typedef name, not
         a keyword.  This means it can be redeclared to something else in
         other scopes. */
      make_predeclared_bool_symbol();
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  init_alias_templ_intrinsic_descriptions();
  init_var_templ_intrinsic_descriptions();
  init_templ_type_member_intrinsic_descriptions();
}  /* predeclare_entities */


#if MICROSOFT_EXTENSIONS_ALLOWED

static void enter_underscore_keywords(a_token_kind token,
                                      a_const_char *keyword)
/*
This routine is called in Microsoft compatibility mode.  The string pointed
to by keyword has a double-underscore prefix (e.g., __cdecl), and an
alternate version with only one underscore is also allowed (e.g., _cdecl).
token is the lexical token that corresponds to both.  Enter both keywords.
*/
{
  check_assertion(ms_extensions);
  check_assertion_str(keyword[0] == '_' && keyword[1] == '_',
                      "enter_underscore_keywords: expected \"__\" prefix");
  enter_keyword(token, keyword);
  enter_keyword(token, ++keyword);
}  /* enter_underscore_keywords */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */


static void enter_gnu_keyword(a_token_kind token,
                              a_const_char *keyword)
/*
The Gnu compiler accepts some keywords in two forms and some in
three.  For example, typeof can be specified as "typeof", "__typeof",
or "__typeof__", but others like __alignof__ can be specified only
as "__alignof" or "__alignof__" (i.e., no plain form is allowed).

If "keyword" does not begin with an underscore, enter all three forms
of the name.  If it does begin with an underscore, enter the form
provided (which is presumed to be the __name form) and also the
name with two underscores appended.

This function is also called in Sun C++ mode.
*/
{
  char     buffer[50];
  sizeof_t length;

  if (keyword[0] != '_' && !nonstd_gnu_keywords_enabled) {
    /* When nonstd_gnu_keywords_enabled is FALSE, do not enter a GNU keyword
       that doesn't start with an underscore. */
  } else {
    /* Enter the keyword as provided. */
    enter_keyword(token, keyword);
  }  /* if */
  if (keyword[0] != '_') {
    /* A plain name was provided -- add the leading underscores. */
    /* We need room for five extra characters: four for the underscores
       and one for the trailing NULL. */
    length = (sizeof_t)strlen(keyword);
    check_assertion((length + 5) < sizeof(buffer));
    /* Register the variant with two leading underscores. */
    buffer[0] = buffer[1] = '_';
    strcpy(buffer + 2, keyword);
    enter_keyword(token, buffer);
    /* And with two trailing underscores. */
    buffer[length + 2] = buffer[length + 3] = '_';
    buffer[length + 4] = '\0';
  } else {
    /* A __name was provided. */
    /* We need room for three extra characters: two for the underscores
       and one for the trailing NULL. */
    length = (sizeof_t)strlen(keyword);
    check_assertion((length + 3) < sizeof(buffer));
    strcpy(buffer, keyword);
    buffer[length] = buffer[length + 1] = '_'; /*lint !e448*/
    buffer[length + 2] = '\0'; /*lint !e448*/
  }  /* if */
  enter_keyword(token, buffer);
}  /* enter_gnu_keyword */


static void enter_unimplemented_keyword(a_const_char  *keyword,
					an_error_code error_code)
/*
Enter a keyword for a token that is not yet implemented.  error_code
specifies a diagnostic message to be issued if the keyword is used.
*/
{
  a_symbol_ptr sym_ptr;

  sym_ptr = full_enter_symbol(keyword, (sizeof_t)(strlen(keyword)),
			      (a_symbol_kind)sk_keyword, NO_SCOPE_DEPTH);
  sym_ptr->variant.keyword.token = tok_unimplemented;
  sym_ptr->variant.keyword.diagnostic_issued_if_used = error_code;
}  /* enter_unimplemented_keyword */


static void enter_preproc_op_keyword(a_token_kind token,
                                     a_const_char *keyword)
/*
Like enter_keyword but for keywords that also have a meaning when parsing
preprocessing directives (i.e., operators like "and").
*/
{
  a_symbol_ptr sym_ptr;

  sym_ptr = full_enter_symbol(keyword, (sizeof_t)(strlen(keyword)),
			      (a_symbol_kind)sk_keyword, NO_SCOPE_DEPTH);
  sym_ptr->variant.keyword.token = token;
  sym_ptr->variant.keyword.is_preprocessing_op_or_punc = TRUE;
}  /* enter_preproc_op_keyword */


static void enter_type_traits_helpers(void)
/*
Enter the names of "type trait pseudo-functions" as keywords.  They are
patterned after the similar extensions introduced by Microsoft's Visual
C++ 8.0.  They provide direct support for the C++ committee's "Library TR1"
(ISO/IEC TR 19768) and for related features added to C++11 and later revisions.
While only supported when type_traits_helpers_enabled is TRUE (normally, in
most C++ modes), they are recognized and diagnosed as errors in Microsoft C
mode when microsoft_version >= 1400.  A few of the pseudo-functions do not
correspond to any standard facilities: Those are only recognized in Microsoft
modes.
*/
{
  if (ms_extensions) {
    enter_keyword(tok_has_assign, "__has_assign");
    enter_keyword(tok_has_copy, "__has_copy");
    enter_keyword(tok_has_user_destructor, "__has_user_destructor");
#if MICROSOFT_EXTENSIONS_ALLOWED
    enter_keyword(tok_has_finalizer, "__has_finalizer");
    enter_keyword(tok_is_delegate, "__is_delegate");
    enter_keyword(tok_is_interface_class, "__is_interface_class");
    enter_keyword(tok_is_ref_array, "__is_ref_array");
    enter_keyword(tok_is_ref_class, "__is_ref_class");
    enter_keyword(tok_is_sealed, "__is_sealed");
    enter_keyword(tok_is_simple_value_class, "__is_simple_value_class");
    enter_keyword(tok_is_value_class, "__is_value_class");
    enter_keyword(tok_is_win_class, "__is_win_class");
    enter_keyword(tok_is_win_interface, "__is_win_interface");
    enter_keyword(tok_is_valid_winrt_type, "__is_valid_winrt_type");
    enter_keyword(tok_is_trivially_copy_assignable,
                  "__is_trivially_copy_assignable");
    enter_keyword(tok_is_assignable_no_precondition_check,
                  "__is_assignable_no_precondition_check");
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  if (ms_extensions || gnu_version_is(>=120000) ||
      clang_version_is(>=190000)) {
    /* Enter keywords in support of P0466R5 ("Layout-compatibility and
       Pointer-interconvertibility Traits", part of C++20). */
    enter_keyword(tok_is_layout_compatible, "__is_layout_compatible");
    enter_keyword(tok_is_pointer_interconvertible_base_of,
                  "__is_pointer_interconvertible_base_of");
    /* GCC and Visual Studio have different names and signatures for these. */
    if (gnu_version_is(>=120000)) {
      enter_keyword(tok_builtin_is_corresponding_member,
                    "__builtin_is_corresponding_member");
      enter_keyword(tok_builtin_is_pointer_interconvertible_with_class,
                    "__builtin_is_pointer_interconvertible_with_class");
    } else if (ms_extensions) {
      enter_keyword(tok_is_corresponding_member, "__is_corresponding_member");
      enter_keyword(tok_is_pointer_interconvertible_with_class,
                    "__is_pointer_interconvertible_with_class");
    }  /* if */
  }  /* if */
  enter_keyword(tok_has_nothrow_assign, "__has_nothrow_assign");
  enter_keyword(tok_has_nothrow_constructor, "__has_nothrow_constructor");
  enter_keyword(tok_has_nothrow_copy, "__has_nothrow_copy");
  enter_keyword(tok_has_trivial_assign, "__has_trivial_assign");
  enter_keyword(tok_has_trivial_constructor, "__has_trivial_constructor");
  enter_keyword(tok_has_trivial_copy, "__has_trivial_copy");
  enter_keyword(tok_has_trivial_destructor, "__has_trivial_destructor");
  enter_keyword(tok_has_virtual_destructor, "__has_virtual_destructor");
  enter_keyword(tok_is_abstract, "__is_abstract");
  enter_keyword(tok_is_base_of, "__is_base_of");
  enter_keyword(tok_is_class, "__is_class");
  enter_keyword(tok_is_convertible_to, "__is_convertible_to");
  enter_keyword(tok_is_empty, "__is_empty");
  enter_keyword(tok_is_enum, "__is_enum");
  enter_keyword(tok_is_function, "__is_function");
  enter_keyword(tok_is_pod, "__is_pod");
  enter_keyword(tok_is_polymorphic, "__is_polymorphic");
  enter_keyword(tok_is_union, "__is_union");
  enter_keyword(tok_is_trivial, "__is_trivial");
  enter_keyword(tok_is_standard_layout, "__is_standard_layout");
  enter_keyword(tok_is_trivially_copyable, "__is_trivially_copyable");
  enter_keyword(tok_is_literal_type, "__is_literal_type");
  /* Intrinsics common to both Clang and later GCC versions. */
  if (clang_mode || gnu_version_is(>=130000)) {
    enter_keyword(tok_is_convertible, "__is_convertible");
  }  /* if */
  if (clang_mode || gnu_version_is(>=140000)) {
    enter_keyword(tok_is_array, "__is_array");
    enter_keyword(tok_is_bounded_array, "__is_bounded_array");
    enter_keyword(tok_is_const, "__is_const");
    enter_keyword(tok_is_member_function_pointer,
                  "__is_member_function_pointer");
    enter_keyword(tok_is_member_object_pointer, "__is_member_object_pointer");
    enter_keyword(tok_is_member_pointer, "__is_member_pointer");
    enter_keyword(tok_is_object, "__is_object");
    enter_keyword(tok_is_reference, "__is_reference");
  }  /* if */
  if (clang_mode || gnu_version_is(>=150000)) {
    /* Note: tok_is_pointer is handled in a context-sensitive way in expr.c
       because some GCC headers use it as an ordinary identifier. */
    enter_keyword(tok_array_rank, "__array_rank");
    enter_keyword(tok_is_unbounded_array, "__is_unbounded_array");
    enter_keyword(tok_is_volatile, "__is_volatile");
  }  /* if */
  if (clang_mode) {
    enter_keyword(tok_array_extent, "__array_extent");
    enter_keyword(tok_is_arithmetic, "__is_arithmetic");
    enter_keyword(tok_is_complete_type, "__is_complete_type");
    enter_keyword(tok_is_compound, "__is_compound");
    enter_keyword(tok_is_floating_point, "__is_floating_point");
    enter_keyword(tok_is_fundamental, "__is_fundamental");
    enter_keyword(tok_is_integral, "__is_integral");
    enter_keyword(tok_is_lvalue_reference, "__is_lvalue_reference");
    enter_keyword(tok_is_rvalue_reference, "__is_rvalue_reference");
    enter_keyword(tok_is_pointer, "__is_pointer");
    enter_keyword(tok_is_scalar, "__is_scalar");
    enter_keyword(tok_is_unsigned, "__is_unsigned");
    /* Note: tok_is_signed is handled in a context-sensitive way in expr.c
       because some GCC headers use it as an ordinary identifier. */
    enter_keyword(tok_is_void, "__is_void");
    enter_keyword(tok_is_same_as, "__is_same_as");
    enter_keyword(tok_reference_binds_to_temporary,
                  "__reference_binds_to_temporary");
    enter_keyword(tok_is_referenceable, "__is_referenceable");
    enter_keyword(tok_is_literal_type, "__is_literal");
    if (clang_version >= 150000) {
      enter_keyword(tok_is_trivially_relocatable,
                    "__is_trivially_relocatable");
    }  /* if */
    if (clang_version >= 170000) {
      enter_keyword(tok_is_trivially_equality_comparable,
                    "__is_trivially_equality_comparable");
      if (clang_version >= 190000) {
        enter_keyword(tok_is_bitwise_cloneable, "__is_bitwise_cloneable");
      }  /* if */
    }  /* if */
  }  /* if */
  if (gnu_version_is(>=130000) || clang_version_is(>=180000) ||
      ms_version_is(>=1951)) {
    enter_keyword(tok_reference_constructs_from_temporary,
                  "__reference_constructs_from_temporary");
  }  /* if */
  if (gnu_version_is(>=130000) || clang_version_is(>=190000) ||
      ms_version_is(>=1951)) {
    enter_keyword(tok_reference_converts_from_temporary,
                  "__reference_converts_from_temporary");
  }  /* if */
  if (gnu_version_is(>=130000) || clang_version_is(>=190000)) {
    enter_keyword(tok_is_nothrow_convertible, "__is_nothrow_convertible");
  }  /* if */
  if (gnu_version_is(>=140000) || clang_version_is(>=160000)) {
    enter_keyword(tok_is_scoped_enum, "__is_scoped_enum");
  }  /* if */
  if (gnu_version_is(>=150000) || clang_version_is(>=200000)) {
    enter_keyword(tok_builtin_is_virtual_base_of,
                  "__builtin_is_virtual_base_of");
  }  /* if */
  if (gnu_version_is(>=160000) || clang_version_is(>=200000) ||
      ms_version_is(>=1951)) {
    enter_keyword(tok_builtin_is_implicit_lifetime,
                  "__builtin_is_implicit_lifetime");
  }  /* if */
  if (gnu_version_is(>=160000)) {
    enter_keyword(tok_builtin_is_structural, "__builtin_is_structural");
  }  /* if */
  enter_keyword(tok_has_trivial_move_constructor,
                "__has_trivial_move_constructor");
  enter_keyword(tok_has_trivial_move_assign, "__has_trivial_move_assign");
  enter_keyword(tok_has_nothrow_move_assign, "__has_nothrow_move_assign");
  enter_keyword(tok_is_constructible, "__is_constructible");
  enter_keyword(tok_is_nothrow_constructible, "__is_nothrow_constructible");
  enter_keyword(tok_is_trivially_constructible,
                "__is_trivially_constructible");
  enter_keyword(tok_is_destructible, "__is_destructible");
  enter_keyword(tok_is_nothrow_destructible, "__is_nothrow_destructible");
  enter_keyword(tok_is_trivially_destructible, "__is_trivially_destructible");
  enter_keyword(tok_is_assignable, "__is_assignable");
  enter_keyword(tok_is_nothrow_assignable, "__is_nothrow_assignable");
  enter_keyword(tok_is_trivially_assignable, "__is_trivially_assignable");
  enter_keyword(tok_underlying_type, "__underlying_type");
  enter_keyword(tok_is_final, "__is_final");
  enter_keyword(tok_has_unique_object_representations,
                "__has_unique_object_representations");
  enter_keyword(tok_is_aggregate, "__is_aggregate");
  enter_keyword(tok_edg_is_deducible, "__edg_is_deducible");
}  /* enter_type_traits_helpers */


static void enter_c23_keyword(a_token_kind token,
                              a_const_char *old_keyword,
                              a_const_char *new_keyword)
/*
A utility routine to handle keywords that have been deemed obsolescent in C23.
old_keyword indicates the pre-C23 spelling of the keyword and new_keyword
indicates the C23 spelling.  Both keywords are currently enabled (in C23 mode)
and a diagnostic is given on uses of the old keyword.
*/
{
  enter_keyword(token, old_keyword);
  if (c23_mode) {
    enter_keyword(token, new_keyword);
  }  /* if */
}  /* enter_c23_keyword */


static void keyword_init(void)
/*
Install the keywords in the symbol table.
*/
{
  db_enter(5, "keyword_init");

  enter_keyword((a_token_kind)tok_auto,      "auto");
  enter_keyword((a_token_kind)tok_break,     "break");
  enter_keyword((a_token_kind)tok_case,      "case");
  enter_keyword((a_token_kind)tok_char,      "char");
  enter_keyword((a_token_kind)tok_continue,  "continue");
  if (!(microsoft_mode && microsoft_version >= 1400 && ms_permissive)) {
    /* Newer Microsoft compilers treat "default" as an ordinary identifier in
       most contexts, and turn it into a keyword only when it is followed by
       a colon. */
    enter_keyword((a_token_kind)tok_default,   "default");
  }  /* if */
  enter_keyword((a_token_kind)tok_do,        "do");
  enter_keyword((a_token_kind)tok_double,    "double");
  enter_keyword((a_token_kind)tok_else,      "else");
  enter_keyword((a_token_kind)tok_enum,      "enum");
  enter_keyword((a_token_kind)tok_extern,    "extern");
  enter_keyword((a_token_kind)tok_float,     "float");
  enter_keyword((a_token_kind)tok_for,       "for");
  enter_keyword((a_token_kind)tok_goto,      "goto");
  enter_keyword((a_token_kind)tok_if,        "if");
  enter_keyword((a_token_kind)tok_int,       "int");
  if (bit_precise_int_enabled) {
    enter_keyword(tok_bit_precise_int, "_BitInt");
  }  /* if */
  enter_keyword((a_token_kind)tok_long,      "long");
  enter_keyword((a_token_kind)tok_register,  "register");
  enter_keyword((a_token_kind)tok_return,    "return");
  enter_keyword((a_token_kind)tok_short,     "short");
  enter_keyword((a_token_kind)tok_sizeof,    "sizeof");
  enter_keyword((a_token_kind)tok_static,    "static");
  enter_keyword((a_token_kind)tok_struct,    "struct");
  enter_keyword((a_token_kind)tok_switch,    "switch");
  enter_keyword((a_token_kind)tok_typedef,   "typedef");
  enter_keyword((a_token_kind)tok_union,     "union");
  enter_keyword((a_token_kind)tok_unsigned,  "unsigned");
  enter_keyword((a_token_kind)tok_void,      "void");
  enter_keyword((a_token_kind)tok_while,     "while");

  if (C_dialect != C_dialect_pcc) {
    /* Disable keywords that were ANSI C inventions.  The other non-K&R
       keywords (enum and void) are judged to have existed already
       in code. */
    enter_keyword((a_token_kind)tok_const,     "const");
    enter_keyword((a_token_kind)tok_signed,    "signed");
    enter_keyword((a_token_kind)tok_volatile,  "volatile");
  }  /* if */
  if (c99_bool_is_keyword) {
    /* Enable keywords available in both C99 and GNU C mode. */
    enter_c23_keyword(tok_c99_bool, "_Bool", "bool");
    if (c23_mode) {
      enter_keyword(tok_false, "false");
      enter_keyword(tok_true, "true");
    }  /* if */
  }  /* if */
  if (gnu_mode) {
    /* In GNU C/C99/C++ modes, the availability of complex type operations is
       controlled by the configuration macro C99_IL_EXTENSIONS_SUPPORTED.
       The _Imaginary types are not allowed in those modes. */
#if C99_IL_EXTENSIONS_SUPPORTED && GNU_EXTENSIONS_ALLOWED
    enter_keyword((a_token_kind)tok_c99_complex, "_Complex");
    /* GNU compilers also accept __complex and __complex__ to denote
       complex types.  In addition, they provide operators to extract
       the real and imaginary part of a complex value. */
    enter_gnu_keyword((a_token_kind)tok_c99_complex, "__complex");
    enter_gnu_keyword((a_token_kind)tok_gnu_real, "__real");
    enter_gnu_keyword((a_token_kind)tok_gnu_imag, "__imag");
    /* EDG-specific token representing the imaginary number "i" (i*i == -1). */
    enter_keyword((a_token_kind)tok_imaginary_unit, "__I__");
#endif /* C99_IL_EXTENSIONS_SUPPORTED && GNU_EXTENSIONS_ALLOWED */
#if C99_IL_EXTENSIONS_SUPPORTED
  } else if (c99_mode) {
    /* Non-GNU C99 modes support both _Complex and _Imaginary types, provided
       C99_IL_EXTENSIONS_ALLOWED is set to TRUE. */
    enter_keyword((a_token_kind)tok_c99_complex, "_Complex");
    enter_keyword((a_token_kind)tok_c99_imaginary, "_Imaginary");
    /* EDG-specific token representing the imaginary number "i" (i*i == -1). */
    enter_keyword((a_token_kind)tok_imaginary_unit, "__I__");
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  }  /* if */
  if (c99_mode) {
    /* Enable keywords required in C99 mode. */
    enter_keyword((a_token_kind)tok_inline, "inline");
    /* "__generic" is used in the implementation of type-generic functions. */
    enter_keyword((a_token_kind)tok_c99_generic, "__generic");
  }  /* if */
#if C99_IL_EXTENSIONS_SUPPORTED
  if (c99_mode || gnu_mode) {
    enter_keyword((a_token_kind)tok_builtin_complex, "__builtin_complex");
  }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  if (noreturn_keyword_enabled) {
    /* Enable the C11 _Noreturn keyword (accepted by default in some GNU C
       modes as well as some clang C and C++ modes).  This has been deprecated
       in C23 (in favor of [[noreturn]]) but the keyword is still enabled
       (so a diagnostic can be given). */
    enter_keyword((a_token_kind)tok_noreturn, "_Noreturn");
  }  /* if */
  if (c11_mode || gcc_version_is(>= 40900) || clang_version_is(>= 30000) ||
      msc_version_is(>=1926)) {
    /* Enable the C11 _Generic keyword in appropriate emulation modes. */
    enter_keyword((a_token_kind)tok_c11_generic, "_Generic");
  }  /* if */
  if (c11_atomic_enabled) {
    enter_keyword((a_token_kind)tok_c11_atomic, "_Atomic");
  }  /* if */
  if (C_mode() || clang_version_is(>= 30200)) {
    /* Enabled in C mode as well as most C++ clang modes. */
    if (alignof_enabled) {
      enter_c23_keyword(tok_alignof, "_Alignof", "alignof");
    }  /* if */
  }  /* if */
  if (C_mode() || clang_version_is(>= 30300)) {
    /* Enabled in C mode as well as most C++ clang modes. */
    if (std_thread_local_storage_specifier_enabled) {
      enter_c23_keyword(tok_c11_thread_local, "_Thread_local", "thread_local");
    }  /* if */
  }  /* if */
  if (C_mode()) {
    if (alignas_enabled) {
      enter_c23_keyword(tok_alignas, "_Alignas", "alignas");
    }  /* if */
    if (static_assert_enabled) {
      /* Enter the C version of "static_assert", except in non-C11 Microsoft
         modes. */
      if (!(microsoft_mode && !c11_mode)) {
        enter_c23_keyword(tok_static_assert,"_Static_assert", "static_assert");
      }  /* if */
      /* In some Microsoft modes, the C++ version is also enabled in C mode. */
      if (microsoft_mode && microsoft_version >= 1600) {
        enter_keyword((a_token_kind)tok_static_assert, "static_assert");
      }  /* if */
    }  /* if */
  }  /* if */
#if TARG_HAS_IEEE_FLOATING_POINT
  /* EDG-specific token for Not-a-Number constant. */
  enter_keyword((a_token_kind)tok_nan, "__NAN__");
  /* EDG-specific token for Infinity constant. */
  enter_keyword((a_token_kind)tok_infinity, "__INFINITY__");
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
#if FIXED_POINT_ALLOWED
  if (fixed_point_enabled) {
    enter_keyword((a_token_kind)tok_fract, "_Fract");
    enter_keyword((a_token_kind)tok_accum, "_Accum");
    enter_keyword((a_token_kind)tok_sat, "_Sat");
    if (c99_mode) {
      /* "__genericfx" is used in the implementation of type-generic functions
         for fixed-point types. */
      enter_keyword((a_token_kind)tok_c99_genericfx, "__genericfx");
    }  /* if */
  }  /* if */
#endif /* FIXED_POINT_ALLOWED */
  /* __ALIGNOF__(type) returns the alignment requirement for a type (the
     lower case spelling __alignof__ is also accepted).
     __INTADDR__(addr_expr) scans its argument as an initializer expression
     and converts it to integer.  It is used in the definition of offsetof. */
  enter_keyword((a_token_kind)tok_ext_alignof, "__ALIGNOF__");
  enter_keyword((a_token_kind)tok_ext_alignof, "__alignof__");
  enter_keyword((a_token_kind)tok_intaddr, "__INTADDR__");
  if (restrict_keyword_enabled) {
    enter_keyword((a_token_kind)tok_restrict, "restrict");
  }  /* if */
  if (gnu_restrict_keyword_enabled) {
    if (microsoft_mode) {
      /* Microsoft compilers accept "__restrict" but not "__restrict__". */
      enter_keyword((a_token_kind)tok_gnu_restrict, "__restrict");
    } else {
      enter_gnu_keyword((a_token_kind)tok_gnu_restrict, "__restrict");
    }  /* if */
  }  /* if */
  /* Define __func__ unconditionally. */
  enter_keyword((a_token_kind)tok_func_name, "__func__");
  /* These gcc/g++ features are accepted in all modes.  __FUNCTION__
     is also a Microsoft feature. */
  enter_keyword((a_token_kind)tok_function_name, "__FUNCTION__");
  enter_keyword((a_token_kind)tok_pretty_function_name, "__PRETTY_FUNCTION__");
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ms_extensions && (!gnu_mode || clang_mode)) {
    /* Enter keywords that are Microsoft "extensions", i.e., those that MSVC
       recognizes by default, and Clang recognizes in -fms-extensions mode.

       This specific branch is only valid for non-GNU modes, as GCC does not
       implement any of these keywords in their implementation of
       -fms-extensions (though __align_of and __inline appear for other
       reasons).  That said, there are a small number of keywords that are
       supported in Microsoft, GNU, and Clang mode that are handled in a branch
       following this one.

       Note that as Clang mode is based off of GNU mode, we must explicitly
       allow Clang to enter this branch. */
    if (microsoft_version >= 1300 || clang_mode) {
      /* __LPREFIX is generated by token pasting like L##__FUNCTION__ and
         similarly for __lPREFIX (u8), __UPREFIX (U), and __uPREFIX (u). */
      enter_keyword((a_token_kind)tok_microsoft_Lprefix, "__LPREFIX");
      enter_keyword((a_token_kind)tok_microsoft_lprefix, "__lPREFIX");
      enter_keyword((a_token_kind)tok_microsoft_Uprefix, "__UPREFIX");
      enter_keyword((a_token_kind)tok_microsoft_uprefix, "__uPREFIX");
    }  /* if */
    if (!clang_mode) {
      /* The following keywords are only valid in Microsoft mode (or when the
         front end is not running under Microsoft mode, but has support enabled
         for ms_extensions), and have not been implemented by clang. */
      enter_keyword((a_token_kind)tok_clrcall, "__clrcall");
      if (C_dialect == C_dialect_cplusplus) {
        enter_keyword((a_token_kind)tok_event, "__event");
      }  /* if */
    }  /* if */
    enter_underscore_keywords((a_token_kind)tok_microsoft_try, "__try");
    enter_underscore_keywords((a_token_kind)tok_finally, "__finally");
    enter_underscore_keywords((a_token_kind)tok_leave, "__leave");
    enter_underscore_keywords((a_token_kind)tok_except, "__except");
    enter_underscore_keywords((a_token_kind)tok_cdecl, "__cdecl");
    enter_underscore_keywords((a_token_kind)tok_fastcall, "__fastcall");
    enter_underscore_keywords((a_token_kind)tok_stdcall, "__stdcall");
    enter_keyword((a_token_kind)tok_thiscall, "__thiscall");
    enter_underscore_keywords((a_token_kind)tok_vectorcall, "__vectorcall");
    enter_underscore_keywords((a_token_kind)tok_microsoft_inline, "__inline");
    enter_underscore_keywords((a_token_kind)tok_forceinline, "__forceinline");
    enter_underscore_keywords((a_token_kind)tok_assume, "__assume");
    enter_keyword((a_token_kind)tok_unaligned, "__unaligned");
    enter_underscore_keywords((a_token_kind)tok_ext_alignof, "__alignof");
    enter_keyword((a_token_kind)tok_ext_alignof, "__builtin_alignof");
    enter_keyword((a_token_kind)tok_pretty_function_name, "__FUNCSIG__");
    enter_keyword((a_token_kind)tok_decorated_function_name, "__FUNCDNAME__");
    if (targ_int8_int_kind != (an_integer_kind)ik_none) {
      /* There is a 8 bit target integer kind to which __int8 can map. */
      enter_underscore_keywords((a_token_kind)tok_int8, "__int8");
    }  /* if */
    if (targ_int16_int_kind != (an_integer_kind)ik_none) {
      /* There is a 16 bit target integer kind to which __int16 can map. */
        enter_underscore_keywords((a_token_kind)tok_int16, "__int16");
      }  /* if */
    if (targ_int32_int_kind != (an_integer_kind)ik_none) {
      /* There is a 32 bit target integer kind to which __int32 can map. */
      enter_underscore_keywords((a_token_kind)tok_int32, "__int32");
    }  /* if */
    if (targ_int64_int_kind != (an_integer_kind)ik_none) {
      /* There is a 64 bit target integer kind to which __int64 can map. */
      enter_underscore_keywords((a_token_kind)tok_int64, "__int64");
    }  /* if */
    if (microsoft_64bit_pointer_extensions_enabled) {
      enter_underscore_keywords((a_token_kind)tok_microsoft_ptr32, "__ptr32");
      enter_underscore_keywords((a_token_kind)tok_microsoft_ptr64, "__ptr64");
      enter_underscore_keywords((a_token_kind)tok_microsoft_sptr, "__sptr");
      enter_underscore_keywords((a_token_kind)tok_microsoft_uptr, "__uptr");
    }  /* if */
    /* __w64 is enabled even when 64-bit pointer extensions are not enabled
       because it is useful for diagnosing porting problems in 32-bit-only
       code. */
    enter_underscore_keywords((a_token_kind)tok_microsoft_w64, "__w64");
    enter_keyword((a_token_kind)tok_noop, "__noop");
    if (C_dialect == C_dialect_cplusplus) {
      enter_underscore_keywords((a_token_kind)tok_uuidof, "__uuidof");
      enter_keyword((a_token_kind)tok_super, "__super");
      enter_keyword((a_token_kind)tok_interface, "__interface");
    }  /* if */
    if (microsoft_version >= 1300) {
      enter_keyword((a_token_kind)tok_microsoft_identifier, "__identifier");
    }  /* if */
    if (nullptr_enabled) {
      /* In C++/CLI, the nullptr keyword has the managed nullptr type, which
         has slightly different semantics from those of std::nullptr_t.  The
         Microsoft compiler (in both C++/CLI and native modes) defines the
         __nullptr keyword to designate the standard nullptr type. */
      enter_keyword((a_token_kind)tok_native_nullptr, "__nullptr");
    }  /* if */
  }  /* if */
  if (ms_extensions) {
    /* Enter keywords that are Microsoft "extensions", i.e., those that MSVC
       recognizes by default, and that GCC as well as Clang recognize in their
       respective -fms-extensions modes. */
    enter_underscore_keywords((a_token_kind)tok_declspec, "__declspec");
    if (C_dialect == C_dialect_cplusplus) {
      enter_keyword((a_token_kind)tok_if_exists, "__if_exists");
      enter_keyword((a_token_kind)tok_if_not_exists, "__if_not_exists");
    }  /* if */
  }
  if (microsoft_mode) {
    /* Enter Microsoft-specific keywords that aren't recognized in clang's
       -fms-extensions mode. */
    enter_keyword((a_token_kind)tok_cdecl, "cdecl");
    enter_underscore_keywords((a_token_kind)tok_based, "__based");
  }  /* if */
  init_whitespace_keywords();
  if (cli_or_cx_enabled) {
    /* Keywords that can be the first word of a whitespace keyword.  They
       are never returned by get_token() but are transformed either into
       the associated whitespace keyword or are treated as ordinary
       identifiers. */
    enter_keyword((a_token_kind)tok_prefix_interface, "interface");
    enter_keyword((a_token_kind)tok_prefix_ref, "ref");
    enter_keyword((a_token_kind)tok_prefix_value, "value");
    if (cppcx_enabled) {
      /* "partial" can be the first word of a whitespace keyword. */
      enter_keyword((a_token_kind)tok_prefix_partial, "partial");
    } else {
      /* "gcnew" is only available in true C++/CLI mode, not C++/CX mode. */
      enter_keyword((a_token_kind)tok_gcnew, "gcnew");
    }  /* if */
    { a_symbol_locator locator;
      /* safe_cast is a contextual keyword. */
      clear_locator(&locator, &null_source_position);
      safe_cast_symbol_header = find_symbol_header("safe_cast",
                                                   sizeof("safe_cast")-1,
                                                   &locator);
    }
    /* A keyword used only in classes read from CLI metadata. */
    enter_keyword((a_token_kind)tok_implements, "__implements");
    /* A keyword used only to denote certain incomplete types read from CLI
       metadata. */
    enter_keyword((a_token_kind)tok_unresolved_type, "__unresolved_type");
    init_cli_operator_headers();
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (!C_mode() &&
      ((cli_or_cx_enabled || (microsoft_mode && microsoft_version >= 1900)) ||
        clang_mode || gnu_version_is(>=140000))) {
    internal_templates_enabled = TRUE;
    /* A keyword used to predefine alias templates even when alias declarations
       are not otherwise enabled.  This is used, e.g., to map interior_ptr<T>
       to the appropriate tk_pointer entry and to create builtin alias
       templates (e.g., __make_integer_seq) in various emulation modes. */
    enter_keyword((a_token_kind)tok_internal_alias_decl,
                  "__internal_alias_decl");
  }  /* if */
  if (type_traits_helpers_enabled ||
      (microsoft_mode && microsoft_version >= 1400)) {
    enter_type_traits_helpers();
  }  /* if */
  if ((gcc_mode ||
       (C_mode() && microsoft_mode && microsoft_version >= 1900)) &&
      !c99_mode) {
    /* "inline" will already have been entered in C99 mode. */
    enter_keyword((a_token_kind)tok_inline, "inline");
  } else if (gpp_mode) {
    enter_keyword((a_token_kind)tok_null, "__null");
  }  /* if */
  if (gnu_version_is(>= 40000) || ms_version_is(>= 1910) || clang_mode) {
    /* Enable __builtin_offsetof in various emulation modes. */
    enter_keyword((a_token_kind)tok_builtin_offsetof, "__builtin_offsetof");
  }  /* if */
  if (gnu_mode) {
    enter_gnu_keyword((a_token_kind)tok_typeof, "typeof");
  } else if (c23_typeof_enabled) {
    enter_keyword((a_token_kind)tok_typeof, "typeof");
  }  /* if */
  if (c23_typeof_enabled) {
    enter_keyword((a_token_kind)tok_typeof_unqual, "typeof_unqual");
  }  /* if */
  if (gnu_mode || msc_version_is(>=1939)) {
    enter_keyword((a_token_kind)tok_extension, "__extension__");
  }  /* if */
  if (gnu_mode) {
    if (gcc_mode && gnu_version >= 40900) {
      enter_keyword((a_token_kind)tok_auto_type, "__auto_type");
    }  /* if */
#if GNU_EXTENSIONS_ALLOWED
    /* g++ 3.4 and later support an __offsetof keyword that appears to be
       identical to our __INTADDR__.  Only g++ 3.4(.x) appears to use this
       for the implementation of the offsetof macro, however.  Later versions
       of both gcc and g++ use another new construct: __builtin_offsetof. */
    if (gpp_mode && gnu_version >= 30400) {
      enter_gnu_keyword((a_token_kind)tok_intaddr, "__offsetof");
    }  /* if */
    enter_gnu_keyword((a_token_kind)tok_builtin_types_compatible,
                      "__builtin_types_compatible_p");
#if INT128_EXTENSIONS_ALLOWED
    if (gnu_version >= 40600 && int128_extensions_enabled) {
      enter_keyword((a_token_kind)tok_int128, "__int128");
    }  /* if */
#endif /*  INT128_EXTENSIONS_ALLOWED */
    if (gnu_version >= 40700) {
      enter_keyword((a_token_kind)tok_bases, "__bases");
      enter_keyword((a_token_kind)tok_direct_bases, "__direct_bases");
    }  /* if */
    if (gpp_version_is(>= 70000)) {
      enter_keyword((a_token_kind)tok_is_same_as, "__is_same_as");
      if (gpp_version_is(>= 80000)) {
        enter_keyword((a_token_kind)tok_integer_pack, "__integer_pack");
        if (gpp_version_is(>= 100000)) {
          enter_keyword((a_token_kind)tok_is_same, "__is_same");
        }  /* if */
      }  /* if */
    }  /* if */
    if (gnu_version >= 90000) {
      enter_gnu_keyword((a_token_kind)tok_builtin_has_attribute,
                        "__builtin_has_attribute");
    }  /* if */
    if (gcc_version_is(>= 70000) || gpp_version_is(>= 130000)) {
      enter_keyword(tok_float32, "_Float32");
      enter_keyword(tok_float32x, "_Float32x");
      enter_keyword(tok_float64, "_Float64");
      enter_keyword(tok_float64x, "_Float64x");
#if FLOAT128_ENABLING_POSSIBLE
      enter_keyword(tok_float128, "_Float128");
#endif /* FLOAT128_ENABLING_POSSIBLE */
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    /* Enable alternative token spellings. */
    enter_gnu_keyword((a_token_kind)tok_inline, "__inline");
    enter_gnu_keyword((a_token_kind)tok_asm, "__asm");
    enter_gnu_keyword((a_token_kind)tok_const, "__const");
    enter_gnu_keyword((a_token_kind)tok_signed, "__signed");
    enter_gnu_keyword((a_token_kind)tok_volatile, "__volatile");
    enter_keyword((a_token_kind)tok_ext_alignof, "__alignof");
#if GNU_VECTOR_TYPES_ALLOWED
    /* Note that some "__builtin_" builtins are treated as keywords to mimic
       the way they are implemented in GCC. */
    if (gnu_version >= (unsigned long)(gcc_mode ? 40700 : 40800)) {
      enter_keyword((a_token_kind)tok_builtin_shuffle, "__builtin_shuffle");
    }  /* if */
    if (clang_mode || gnu_version_is(>=120000)) {
      /* Strictly speaking, __builtin_shufflevector is not a keyword, but
         treating it as such makes the implementation easier (i.e., it can
         be shared with __builtin_shuffle). */
      enter_keyword((a_token_kind)tok_builtin_shufflevector,
                    "__builtin_shufflevector");
    }  /* if */
    if (clang_mode || gnu_version >= 90000) {
      enter_keyword((a_token_kind)tok_builtin_convertvector,
                    "__builtin_convertvector");
    }  /* if */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
    /* Note that the __edg_vector_type__, __edg_neon_vector_type__,
       __edg_neon_polyvector_type__, and __edg_scalable_vector_type__ are
       defined even when GNU_VECTOR_TYPES_ALLOWED is FALSE; they will become
       error types in that case. */
    enter_keyword(tok_edg_vector_type, "__edg_vector_type__");
    enter_keyword(tok_edg_neon_vector_type, "__edg_neon_vector_type__");
    enter_keyword(tok_edg_neon_polyvector_type,
                  "__edg_neon_polyvector_type__");
    enter_keyword(tok_edg_scalable_vector_type,
                  "__edg_scalable_vector_type__");
  }  /* if */
  if (ms_extensions || clang_mode || gnu_version_is(>=70000)) {
    enter_keyword((a_token_kind)tok_builtin_addressof, "__builtin_addressof");
  }  /* if */
  if (mscpp_version_is(>=1926) || clang_version_is(>=90000) ||
      gpp_version_is(>=110000)) {
    enter_builtin_keyword((a_token_kind)tok_builtin_bit_cast,
                          "__builtin_bit_cast");
  }  /* if */
  if (nullability_qualifiers_enabled) {
    enter_keyword((a_token_kind)tok_nullable, "_Nullable");
    enter_keyword((a_token_kind)tok_nonnull, "_Nonnull");
    enter_keyword((a_token_kind)tok_null_unspecified, "_Null_unspecified");
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  if (gnu_attributes_enabled) {
    enter_gnu_keyword((a_token_kind)tok_attribute, "__attribute");
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if NEAR_AND_FAR_ALLOWED
  if (near_and_far_enabled()) {
    /* Enter "near" and "far" keywords. */
    enter_keyword((a_token_kind)tok_near, "near");
    enter_keyword((a_token_kind)tok_far, "far");
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode) {
      enter_underscore_keywords((a_token_kind)tok_near, "__near");
      enter_underscore_keywords((a_token_kind)tok_far, "__far");
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
#endif /* NEAR_AND_FAR_ALLOWED */
  /* "asm" is a C++ keyword that is treated as a keyword in C mode, too,
     because, even though not part of the ANSI C language, it is used widely
     in C programs. */
  if (C_dialect == C_dialect_ANSI && (strict_ansi_mode || microsoft_mode)) {
    /* Strict ANSI C or Microsoft C mode -- do not enter "asm". */
  } else {
    enter_keyword((a_token_kind)tok_asm, "asm");
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ms_extensions) {
    /* "__asm" and "_asm" are accepted in Microsoft mode. */
    enter_underscore_keywords((a_token_kind)tok_microsoft_asm, "__asm");
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if ASM_FUNCTION_ALLOWED
  if (!ms_extensions) {
    /* Enter "__asm" as a synonym for "asm" -- it too maps to tok_asm.  Note
       that in strict ANSI C mode, "__asm" is recognized but "asm" is not. */
    enter_keyword((a_token_kind)tok_asm, "__asm");
  }  /* if */
#endif /* ASM_FUNCTION_ALLOWED */
  if (nullptr_enabled) {
    enter_keyword((a_token_kind)tok_nullptr, "nullptr");
    if (c23_mode) {
      /* Only enter nullptr_t as a keyword in C23 mode so as not to conflict
         with legitimate uses in other language dialects. */
      enter_keyword((a_token_kind)tok_nullptr_t, "nullptr_t");
    }  /* if */
  }  /* if */
  if (C_dialect == C_dialect_cplusplus) {
    /* Enter C++ keywords that are not also C keywords. */
    enter_keyword((a_token_kind)tok_catch,     "catch");
    enter_keyword((a_token_kind)tok_class,     "class");
    enter_keyword((a_token_kind)tok_friend,    "friend");
    enter_keyword((a_token_kind)tok_inline,    "inline");
    enter_keyword((a_token_kind)tok_mutable,   "mutable");
    enter_keyword((a_token_kind)tok_operator,  "operator");
    enter_keyword((a_token_kind)tok_private,   "private");
    enter_keyword((a_token_kind)tok_protected, "protected");
    enter_keyword((a_token_kind)tok_public,    "public");
    enter_keyword((a_token_kind)tok_template,  "template");
    enter_keyword((a_token_kind)tok_this,      "this");
    enter_keyword((a_token_kind)tok_throw,     "throw");
    enter_keyword((a_token_kind)tok_try,       "try");
    enter_keyword((a_token_kind)tok_virtual,   "virtual");
    enter_keyword((a_token_kind)tok_const_cast,       "const_cast");
    enter_keyword((a_token_kind)tok_static_cast,      "static_cast");
    enter_keyword((a_token_kind)tok_reinterpret_cast, "reinterpret_cast");
    /* Operators new and delete are also recognized during preprocessing. */
    enter_preproc_op_keyword((a_token_kind)tok_delete,    "delete");
    enter_preproc_op_keyword((a_token_kind)tok_new,       "new");
    if (allow_anachronisms) {
      enter_keyword((a_token_kind)tok_overload, "overload");
    }  /* if */
    if (wchar_t_is_keyword) {
      enter_keyword((a_token_kind)tok_wchar_t, "wchar_t");
    }  /* if */
    if (char8_t_enabled) {
      enter_keyword((a_token_kind)tok_char8_t, "char8_t");
    }  /* if */
    if (char16_t_and_char32_t_are_keywords) {
      enter_keyword((a_token_kind)tok_char16_t, "char16_t");
      enter_keyword((a_token_kind)tok_char32_t, "char32_t");
    }  /* if */
    if (clang_mode) {
      /* Clang defines __char16_t/__char32_t as aliases for char16_t/char32_t
         respectively (in all C++ modes, not just C++11 mode). */
      enter_keyword((a_token_kind)tok_char16_t, "__char16_t");
      enter_keyword((a_token_kind)tok_char32_t, "__char32_t");
    }  /* if */
    if (bool_is_keyword) {
      /* Enter C++ keywords used for the bool type.  This is only
         done when bool_is_keyword is TRUE.  When bool_is_keyword is FALSE,
         "bool", "true", and "false" are not entered as unimplemented
         keywords because it is anticipated that most current usage
         will be compatible with the new language feature when it is
         implemented so a diagnostic would not, in general, be
         helpful. */
      if (microsoft_mode && microsoft_version < 1310) {
        /* MSVC++ 6.0 and 7.0 treat "bool" as a predeclared typedef name, not
           a keyword.  When emulating those compilers, we enter bool into the
           symbol table later. */
      } else {
        enter_keyword((a_token_kind)tok_bool,  "bool");
      }  /* if */
      enter_keyword((a_token_kind)tok_false, "false");
      enter_keyword((a_token_kind)tok_true,  "true");
    }  /* if */
    /* Enter C++ keywords used as synonyms for operators.  In the C case, these
       are defined as macros (in iso646.h), so no keywords are needed. */
    if (alternative_tokens_allowed) {
      enter_preproc_op_keyword((a_token_kind)tok_and_and,        "and");
      enter_preproc_op_keyword((a_token_kind)tok_and_assign,     "and_eq");
      enter_preproc_op_keyword((a_token_kind)tok_ampersand,      "bitand");
      enter_preproc_op_keyword((a_token_kind)tok_or,             "bitor");
      enter_preproc_op_keyword((a_token_kind)tok_compl,          "compl");
      enter_preproc_op_keyword((a_token_kind)tok_not,            "not");
      enter_preproc_op_keyword((a_token_kind)tok_ne,             "not_eq");
      enter_preproc_op_keyword((a_token_kind)tok_or_or,          "or");
      enter_preproc_op_keyword((a_token_kind)tok_or_assign,      "or_eq");
      enter_preproc_op_keyword((a_token_kind)tok_excl_or,        "xor");
      enter_preproc_op_keyword((a_token_kind)tok_excl_or_assign, "xor_eq");
    }  /* if */
    /* Enter keywords connected with RTTI only if RTTI support is enabled.
       Otherwise treat them as "unimplemented keywords".

       Note that some kinds of dynamic_cast can be done without RTTI
       information, so the checking for those is done in the dynamic_cast
       scanning.

       Additionally, note that Microsoft permits typeid even when RTTI is
       disabled.  Thus, the typeid keyword is unconditionally entered in
       Microsoft mode. */
    enter_keyword((a_token_kind)tok_dynamic_cast, "dynamic_cast");
    if (rtti_enabled || microsoft_mode) {
      enter_keyword((a_token_kind)tok_typeid, "typeid");
    } else {
      enter_unimplemented_keyword("typeid", ec_unimplemented_keyword);
    }  /* if */
    /* Enter keywords connected with namespaces only if namespace support
       is enabled.  Otherwise treat them as "unimplemented keywords". */
    if (namespaces_enabled) {
      enter_keyword((a_token_kind)tok_namespace, "namespace");
      enter_keyword((a_token_kind)tok_using,     "using");
    } else {
      enter_unimplemented_keyword("namespace", ec_unimplemented_keyword);
      enter_unimplemented_keyword("using",     ec_unimplemented_keyword);
    }  /* if */
    /* Enter typename keyword only if typename support is enabled.
       Otherwise treat it as an "unimplemented keyword". */
    if (typename_enabled) {
      enter_keyword((a_token_kind)tok_typename, "typename");
    } else {
      enter_unimplemented_keyword("typename", ec_unimplemented_keyword);
    }  /* if */
    /* Recognition of "explicit" as a keyword may be enabled or disabled by
       command line options. */
    if (explicit_keyword_enabled) {
      enter_keyword((a_token_kind)tok_explicit, "explicit");
    }  /* if */
    /* Recognition of C++98 exported templates "export" as a keyword is
       disabled in certain modes and incompatible with C++20 modules "export".
    */
    check_assertion(!(export_keyword_enabled && module_keywords_enabled));
    if (module_keywords_enabled) {
      enter_keyword((a_token_kind)tok_export, "export");
    } else if (export_keyword_enabled) {
      enter_keyword((a_token_kind)tok_cpp98_export, "export");
    }  /* if */
    if (static_assert_enabled) {
      enter_keyword((a_token_kind)tok_static_assert, "static_assert");
    }  /* if */
    if (clang_mode) {
      /* clang allows _Static_assert in all C++ modes. */
      enter_keyword((a_token_kind)tok_static_assert, "_Static_assert");
      enter_keyword((a_token_kind)tok_is_same, "__is_same");
    }  /* if */
    if (decltype_enabled) {
      /* In some GNU C++ modes, the decltype feature is only available via the
         alternative spelling "__decltype".  In other GNU C++ modes, both
         spellings are available. */
      if (gpp_mode) enter_keyword((a_token_kind)tok_decltype, "__decltype");
      if (!enable_underscore_decltype_only) {
        enter_keyword((a_token_kind)tok_decltype, "decltype");
      }  /* if */
    }  /* if */
    if (clang_mode) {
      /* Clang defines a __nullptr token that matches nullptr even in modes
         that don't allow nullptr (like C++03). */
      enter_keyword((a_token_kind)tok_nullptr, "__nullptr");
    }  /* if */
    if (noexcept_enabled) {
      enter_keyword((a_token_kind)tok_noexcept, "noexcept");
    }  /* if */
    if (constexpr_enabled) {
      enter_keyword((a_token_kind)tok_constexpr, "constexpr");
    }  /* if */
    if (consteval_enabled) {
      enter_keyword((a_token_kind)tok_consteval, "consteval");
    }  /* if */
    if (constinit_enabled) {
      enter_keyword((a_token_kind)tok_constinit, "constinit");
    }  /* if */
    if (gpp_version_is(>= 100000)) {
      enter_keyword((a_token_kind)tok_constinit, "__constinit");
    }  /* if */
    if (alignas_enabled) {
      enter_keyword((a_token_kind)tok_alignas, "alignas");
    }  /* if */
    if (alignof_enabled &&
        !(clang_mode && !cpp11_mode)) {
      /* clang seems to only enable this version of the keyword in C++11
         and later modes. */
      enter_keyword((a_token_kind)tok_alignof, "alignof");
    }  /* if */
    if (clang_version_is(>= 180000)) {
      enter_keyword(tok_datasizeof, "__datasizeof");
      if (clang_version_is(>= 220000)) {
        enter_keyword(tok_builtin_lt_synthesizes_from_spaceship,
                      "__builtin_lt_synthesizes_from_spaceship");
        enter_keyword(tok_builtin_gt_synthesizes_from_spaceship,
                      "__builtin_gt_synthesizes_from_spaceship");
        enter_keyword(tok_builtin_le_synthesizes_from_spaceship,
                      "__builtin_le_synthesizes_from_spaceship");
        enter_keyword(tok_builtin_ge_synthesizes_from_spaceship,
                      "__builtin_ge_synthesizes_from_spaceship");
      }  /* if */
    }  /* if */
    if (coroutines_enabled) {
      enter_keyword((a_token_kind)tok_coroutine_yield, "co_yield");
      enter_keyword((a_token_kind)tok_coroutine_return, "co_return");
      enter_keyword((a_token_kind)tok_coroutine_await, "co_await");
    }  /* if */
    if (concepts_enabled) {
      enter_keyword((a_token_kind)tok_requires, "requires");
      enter_keyword((a_token_kind)tok_concept, "concept");
    }  /* if */
  }  /* if */
  if (ms_extensions && microsoft_version >= 1300) {
    /* The __wchar_t keyword is entered even when wchar_t_is_keyword is FALSE.
       It exists even in C mode, and produces a type distinct from the normal
       C integral types (it isn't a synonym for "short" or "unsigned short",
       but a distinct type just like wchar_t in C++). */
    enter_keyword((a_token_kind)tok_wchar_t, "__wchar_t");
  }  /* if */
#if SUN_EXTENSIONS_ALLOWED
  if (sun_linker_scope_allowed) {
    enter_keyword((a_token_kind)tok_global_link_scope, "__global");
    enter_keyword((a_token_kind)tok_symbolic_link_scope, "__symbolic");
    enter_keyword((a_token_kind)tok_hidden_link_scope, "__hidden");
  }  /* if */
  if (sun_mode) {
    enter_gnu_keyword((a_token_kind)tok_ext_alignof, "__alignof");
    enter_gnu_keyword((a_token_kind)tok_typeof, "__typeof");
  }  /* if */
#endif /* SUN_EXTENSIONS_ALLOWED */
  if (thread_local_storage_specifier_enabled) {
    enter_keyword((a_token_kind)tok_thread, "__thread");
  }  /* if */
  if (!C_mode() && std_thread_local_storage_specifier_enabled) {
    enter_keyword((a_token_kind)tok_thread_local, "thread_local");
  }  /* if */
#if UPC_EXTENSIONS_ALLOWED
  if (upc_mode) {
    enter_keyword((a_token_kind)tok_upc_shared,      "shared");
    enter_keyword((a_token_kind)tok_upc_strict,      "strict");
    enter_keyword((a_token_kind)tok_upc_relaxed,     "relaxed");
    enter_keyword((a_token_kind)tok_upc_forall,      "upc_forall");
    enter_keyword((a_token_kind)tok_upc_barrier,     "upc_barrier");
    enter_keyword((a_token_kind)tok_upc_notify,      "upc_notify");
    enter_keyword((a_token_kind)tok_upc_wait,        "upc_wait");
    enter_keyword((a_token_kind)tok_upc_fence,       "upc_fence");
    if (upc_dynamic_threads()) {
      enter_keyword((a_token_kind)tok_upc_threads,     "THREADS");
    }  /* if */
    enter_keyword((a_token_kind)tok_upc_mythread,    "MYTHREAD");
    enter_keyword((a_token_kind)tok_upc_blocksizeof, "upc_blocksizeof");
    enter_keyword((a_token_kind)tok_upc_localsizeof, "upc_localsizeof");
    enter_keyword((a_token_kind)tok_upc_elemsizeof,  "upc_elemsizeof");
  }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
  enter_keyword((a_token_kind)tok_edg_internal_type,  "__edg_type__");
  enter_keyword((a_token_kind)tok_edg_size_type,      "__edg_size_type__");
  enter_keyword((a_token_kind)tok_edg_ptrdiff_type,   "__edg_ptrdiff_type__");
  enter_keyword((a_token_kind)tok_edg_bool_type,      "__edg_bool_type__");
  enter_keyword((a_token_kind)tok_edg_wchar_type,     "__edg_wchar_type__");
  enter_keyword((a_token_kind)tok_edg_internal_opnd,  "__edg_opnd__");
  /* __edg_throw__ is an alias for "throw" in C++ mode and is effectively
     discarded (along with any arguments) in C mode.  This is used for builtin
     function declarations that can appear in both modes. */
  enter_keyword(tok_edg_throw,                        "__edg_throw__");
  db_exit();
}  /* keyword_init */


static void open_pp_output_file(void)
/*
Open the preprocessing output file.
*/
{
  if (pp_file_name == NULL) {
    /* If no name was specified, default is stdout. */
    f_pp_output = default_preproc_output_file();
  } else {
    /* An explicit name was specified. */
    f_pp_output = open_output_file_with_error_handling(
                    pp_file_name, /*binary_file=*/FALSE, /*update_mode=*/FALSE,
                    OFF_COMMAND_LINE, ec_preprocessing_output);
  }  /* if */
}  /* open_pp_output_file */


#if IL_SHOULD_BE_WRITTEN_TO_FILE
static void open_il_file(void)
/*
Open the intermediate language file.
*/
{
  if (il_file_name == NULL) {
    /* No explicit IL file name was specified. */
#if BACK_END_SHOULD_BE_CALLED
#if DO_IL_LOWERING
    if (suppress_il_lowering) {
      /* By suppressing IL lowering, the back end cannot be run as part
         of the current program.  The IL should be written to a default
         IL file.  If the input file is stdin, the name cannot be
         generated. */
      if (strcmp(primary_source_file_name, FILE_NAME_FOR_STDIN) == 0) {
        str_command_line_error(ec_cl_il_file_must_be_specified,
                               primary_source_file_name);
      }  /* if */
      il_file_name = derived_name(primary_source_file_name, IL_FILE_SUFFIX);
    } else {
#endif /* DO_IL_LOWERING */
      /* The back end will be run as part of the current program.
         Use a temporary file. */
      f_il_output = open_temp_file(/*binary_file=*/TRUE);
      goto have_il_file;
#if DO_IL_LOWERING
    }  /* if */
#endif /* DO_IL_LOWERING */
#else /* !BACK_END_SHOULD_BE_CALLED */
    /* The back end is in another program.  Generate a default
       IL file name.  If the input file is stdin, the name cannot be
       generated. */
    if (strcmp(primary_source_file_name, FILE_NAME_FOR_STDIN) == 0) {
      str_command_line_error(ec_cl_il_file_must_be_specified,
                             primary_source_file_name);
    }  /* if */
    il_file_name = derived_name(primary_source_file_name, IL_FILE_SUFFIX);
#endif /* BACK_END_SHOULD_BE_CALLED */
  }  /* if */
  f_il_output = open_output_file_with_error_handling(
                    il_file_name, /*binary_file=*/TRUE,
                    /*update_mode=*/BACK_END_SHOULD_BE_CALLED,
                    OFF_COMMAND_LINE, ec_il_output);
#if BACK_END_SHOULD_BE_CALLED
have_il_file:;
#endif /* BACK_END_SHOULD_BE_CALLED */
}  /* open_il_file */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

#if CHECKING
static void check_int_type_config(void)
/*
Make sure the specific size integer typedefs are defined correctly.
*/
{
  check_assertion_str2(sizeof(int8_t) == 1, "check_int_type_config:",
                       "int8_t not defined correctly");
  check_assertion_str2(sizeof(uint8_t) == 1, "check_int_type_config:",
                       "uint8_t not defined correctly");
  check_assertion_str2(sizeof(int16_t) == 2, "check_int_type_config:",
                       "int16_t not defined correctly");
  check_assertion_str2(sizeof(uint16_t) == 2, "check_int_type_config:",
                       "uint16_t not defined correctly");
  check_assertion_str2(sizeof(int32_t) == 4, "check_int_type_config:",
                       "int32_t not defined correctly");
  check_assertion_str2(sizeof(uint32_t) == 4, "check_int_type_config:",
                       "uint32_t not defined correctly");
}  /* check_int_type_config */
#endif /* CHECKING */


void fe_early_init(void)
/*
Do initialization that needs to be done very early, specifically before command
line processing is done.
*/
{
#if DEBUG
  /* Initialize debugging data structures.  This must be done first in this
     routine. */
  debug_early_init();
#endif /* DEBUG */
#if CHECKING
  check_int_type_config();
#endif /* CHECKING */
  mem_manage_early_init();
  /* Do host-specific initialization.  Except for debug and memory management
     initialization, this must be done first in this routine. */
  host_envir_early_init();
  target_early_init();
  cmd_line_early_init();
  error_early_init();
  pch_early_init();
  modules_early_init();
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  il_write_early_init();
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
  depth_scope_stack = NO_SCOPE_DEPTH;
  depth_innermost_instantiation_scope = NO_SCOPE_DEPTH;
#if NEAR_AND_FAR_ALLOWED
  il_header.near_and_far_are_enabled = DEFAULT_NEAR_AND_FAR_ENABLED;
  il_header.far_data_pointers = DEFAULT_FAR_DATA_POINTERS;
  il_header.far_code_pointers = DEFAULT_FAR_CODE_POINTERS;
#endif /* NEAR_AND_FAR_ALLOWED */
  /* Early initialization of the translation unit information.  This must
     be done before fe_one_time_init is started. */
  trans_unit_early_init();
  types_early_init();
#if ALWAYS_SET_MULTIBYTE_LOCALE
  /* Set the locale to be used for multibyte character translation. */
  set_multibyte_locale();
#endif /* ALWAYS_SET_MULTIBYTE_LOCALE */
}  /* fe_early_init */


void fe_one_time_init(void)
/*
Do initialization that does not have to be redone with each translation
unit, in case multiple source files are allowed.  This initialization is done
after the command-line processing has been done.
*/
{
  /* Set a current position indicating we are still in initialization. */
  set_position_to(pos_curr_token, 0, SP_COL_UNKNOWN);
  set_err_pos_to_curr_token();
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
  if (multibyte_chars_in_source_enabled) {
    /* Set the locale to be used for multibyte character translation.  This
       will normally have already been done either above (in fe_early_init)
       or in command-line processing.  But if it has not been done, do it
       now. */
    set_multibyte_locale();
  }  /* if */
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
  target_one_time_init();
  host_envir_one_time_init();
  class_decl_one_time_init();
  decl_spec_one_time_init();
  decls_one_time_init();
  declarator_one_time_init();
  decl_inits_one_time_init();
  def_arg_one_time_init();
  error_one_time_init();
  expr_one_time_init();
  exprutil_one_time_init();
  folding_one_time_init();
  il_to_str_one_time_init();
  il_one_time_init();
  interpret_one_time_init();
  lookup_one_time_init();
  layout_one_time_init();
  lexical_one_time_init();
  literals_one_time_init();
  macro_one_time_init();
  mem_manage_one_time_init();
  pch_one_time_init();
  pragma_one_time_init();
  preproc_one_time_init();
  statements_one_time_init();
  symbol_ref_one_time_init();
  symbol_tbl_one_time_init();
  scope_stk_one_time_init();
  templates_one_time_init();
  trans_copy_one_time_init();
  trans_unit_one_time_init();
  corresp_one_time_init();
#if DO_IL_LOWERING
  /* IL lowering is initialized even when IL lowering is suppressed.  This
     is done because some of the variables that are initialized in IL
     lowering are used elsewhere even when IL lowering is not being done. */
  il_lower_one_time_init();
  lower_c99_one_time_init();
#endif /* DO_IL_LOWERING */
#if NEED_NAME_MANGLING
  lower_name_one_time_init();
#endif /* NEED_NAME_MANGLING */
  attribute_one_time_init();
#if GNU_EXTENSIONS_ALLOWED
  extasm_one_time_init();
#endif /* GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  ms_attrib_one_time_init();
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  sys_predef_one_time_init();
  modules_one_time_init();
#if !USE_HOST_FP_CONVERSION_ROUTINES || \
    (USE_FLOAT128_FOR_HOST_FP_VALUE && !USE_QUADMATH_LIBRARY)
  floating_one_time_init();
#endif /* !USE_HOST_FP_CONVERSION_ROUTINES || (USE_FLOAT128...) */
#if CHECKING
  /* Verify that the type used to store a function pointer index is
     large enough. */
  /*lint -e{506}*/
  if (1 << (sizeof(a_function_number)*CHAR_BIT) < (int)fn_last) {
    internal_error("a_function_number is too small");
  }  /* if */
#endif /* CHECKING */
#if DEBUG
  mem_manage_one_time_init_done();
#endif /* DEBUG */
}  /* fe_one_time_init */


void fe_init_part_1(void)
/*
Do the first phase of front end initialization to be done for each source
file compiled.  This part does everything except opening the source file.
This is the initialization that occurs before determining whether a
precompiled header can be used to replace the initial portion of this
source file's compilation.
*/
{
  time_t  timer;
  char    *time_str;
#if DEBUG
  int     save_debug_level;

  /* Drop the debug level to 0 during initialization.  If debug output
     is desired in initialization, it can be explicitly requested by
     name (of this routine, "fe_init_part_1"). */
  debug_level = 0;
#endif /* DEBUG */
  db_enter(5, "fe_init_part_1");

  /* Get current date and time in proper form for __DATE__ and __TIME__. */
  /* curr_date_time will be like "Sun Sep 16 01:03:52 1973\n". */
  (void)time(&timer);
  time_str = ctime(&timer);
  if (time_str == NULL) {
    /* In the unlikely event that ctime returns NULL, use a properly-formatted
       string so that __DATE__ and __TIME__ will have sane (though incorrect)
       values. */
    time_str = (char*)"Sun Jan 01 00:00:00 1900\n";
  }  /* if */
  check_assertion(strlen(time_str) < sizeof(curr_date_time));
  (void)strcpy(curr_date_time, time_str);

  in_front_end = TRUE;

  /* statements.h: */
  depth_stmt_stack = -1;

  error_init();
  folding_init();
  interpret_init();
  mem_manage_init();
  host_envir_init();
  host_init();
  il_to_str_init();
  il_init();
#if IL_WALK_NEEDED || MAINTAIN_NEEDED_FLAGS
  il_walk_init();
#endif /* IL_WALK_NEEDED || MAINTAIN_NEEDED_FLAGS */
  lexical_init();
  symbol_tbl_init();
  scope_stk_init();
  decls_init();
  declarator_init();
  decl_spec_init();
  decl_inits_init();
  class_decl_init();
  layout_init();
  def_arg_init();
  templates_init();
  types_init();
  corresp_init();
  exprutil_init();
  lookup_init();
  macro_init();
  statements_init();
  pch_init();
  pragma_init();
  preproc_init();
  target_init();
  /* const_ints_init must be called after target_init so that
     int_kind_is_signed is properly initialized. */
  const_ints_init();
  float_pt_init();
#if DO_IL_LOWERING
  /* IL lowering is initialized even when IL lowering is suppressed.  This
     is done because some of the variables that are initialized in
     IL lowering are used elsewhere even when IL lowering is not being done.
     (for example, null_eh_region_number when the --building_runtime
     option is used). */
  il_lower_init();
  lower_c99_init();
#endif /* DO_IL_LOWERING */
#if NEED_NAME_MANGLING
  /* Do lower_name.c initialization.  Name mangling can be included
     independently of the rest of IL lowering. */
  lower_name_init();
#endif /* NEED_NAME_MANGLING */
  attribute_init();
#if MICROSOFT_EXTENSIONS_ALLOWED
  ms_attrib_init();
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING
  if (!C_mode() && make_all_functions_unprototyped) {
    /* <stdarg.h> cannot be treated as a builtin if IL lowering will
       eliminate ellipsis argument lists. */
    pass_stdarg_references_to_generated_code = FALSE;
  }  /* if */
#endif /* DO_IL_LOWERING */
#if COMPILE_MULTIPLE_SOURCE_FILES
  /* If more than one source file is being compiled, identify each
     source file as compilation starts. */
  identify_source_file();
#endif /* COMPILE_MULTIPLE_SOURCE_FILES */
#if DEBUG
  /* Establish the initial debug level (from the command line, or 0 by
     default) at this point so that the first line will be read with the
     same debug level as the other lines.  This avoids the confusion of
     not seeing the debug output for the first source line when all the
     other lines appear. */
  save_debug_level = debug_level;
  if (init_debug_level > debug_level) debug_level = init_debug_level;
#endif /* DEBUG */
#if DEBUG
  /* Restore the debug level fe_init is supposed to have (0 unless
     there's a command-line request to change the debug level in fe_init). */
  debug_level = save_debug_level;
#endif /* DEBUG */

  il_header.plain_chars_are_signed = targ_has_signed_chars;
  /* il_header.region_scope_entry was initialized in mem_manage.c and already
     has meaningful value. */
  il_header.source_language =
                      (C_dialect == C_dialect_cplusplus) ? sl_Cplusplus : sl_C;
  il_header.std_version = std_version;
  il_header.pcc_compatibility_mode = (C_dialect == C_dialect_pcc);
  il_header.enum_type_is_integral = enum_type_is_integral;
  il_header.default_max_member_alignment = default_max_member_alignment;
#if MICROSOFT_EXTENSIONS_ALLOWED
  il_header.microsoft_mode = microsoft_mode;
  il_header.cppcli_enabled = cppcli_enabled;
  il_header.cppcx_enabled = cppcx_enabled;
  il_header.microsoft_version = microsoft_version;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  il_header.gcc_mode = gcc_mode;
  il_header.gpp_mode = gpp_mode;
  il_header.clang_mode = clang_mode;
  il_header.gnu_version = gnu_version;
  il_header.clang_version = clang_version;
  /* il_header.short_enums and il_header.default_nocommon are initialized
     during command-line processing. */
#endif /* GNU_EXTENSIONS_ALLOWED */
  il_header.num_seq_number_lookup_entries = 0;
  il_header.seq_number_lookup_entries = NULL;
#if NEAR_AND_FAR_ALLOWED
  /* near_and_far_enabled, far_data_pointers, and far_code_pointers are
     initialized in fe_early_init and changed if necessary in cmd_line.c. */
#endif /* NEAR_AND_FAR_ALLOWED */
  il_header.UCN_identifiers_used = FALSE;
  il_header.vla_used = FALSE;
  il_header.any_templates_seen = FALSE;
  il_header.prototype_instantiations_in_il = prototype_instantiations_in_il;
  il_header.il_has_all_prototype_instantiations =
                                            all_template_info_in_il &&
                                            nonclass_prototype_instantiations;
  il_header.il_has_C_semantics = C_mode();
#if ONE_INSTANTIATION_PER_OBJECT
  il_header.number_of_external_nonclass_template_entities = 0;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
  il_header.file_scope_dynamic_init_routines = NULL;
#if !USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES
  il_header.thread_local_dynamic_init_routines = NULL;
#endif /* !USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES */
#endif /* SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */
  if (pp_output_file_needed) {
    /* Open the preprocessing output file. */
    open_pp_output_file();
  }  /* if */
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  if (do_preprocessing_only) {
    /* IL file not needed. */
  } else {
    if (!suppress_il_file_write) {
      /* Open the IL file. */
      open_il_file();
    }  /* if */
    /* Write the beginning of the IL file if one is to be generated. */
    start_il_file();
  }  /* if */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
  /* The primary source file pointer is updated when the file is opened. */
  il_header.primary_source_file = NULL;
  write_init();
  db_exit();
#if DEBUG
  /* Restore the initial debug level (from the command line, or 0 by
     default). */
  debug_level = init_debug_level;
#endif /* DEBUG */
}  /* fe_init_part_1 */


static void open_primary_source_file(a_boolean pch_prefix_scan)
/*
Open the primary source file, push the input stack, and get the
first line of the file.  Note that "primary" in this case means "the
top one in a translation unit", including a secondary translation unit.

pch_prefix_scan is TRUE when this routine is called during the initial
scan of a file to build the PCH prefix information.
*/
{
  if (is_primary_translation_unit) {
    /* Clear the primary source file pointer.  This is done just in case the
       primary source file has already been opened for PCH prefix processing.
       If we don't reset the primary source file, push_input_stack
       will try to use the old source file as the parent. */
    il_header.primary_source_file = NULL;
  }  /* if */
  open_file_and_push_input_stack(
               strcpy(alloc_primary_file_scope_il(
                                   (sizeof_t)(strlen(trans_unit_file_name)+1)),
                      trans_unit_file_name),
               /*use_search_path=*/FALSE,
               /*is_include_file=*/FALSE,
               /*is_system_include=*/FALSE,
               /*is_preinclude=*/FALSE,
               /*preinclude_macros=*/FALSE,
               /*is_implicit_include=*/FALSE,
               /*is_include_next=*/FALSE,
               /*continue_on_open_failure=*/FALSE,
               (a_boolean*)NULL);
  /* Save the source file pointer for this translation unit. */
  curr_translation_unit->source_file = curr_ise->assoc_actual_il_file;
  if (!pch_prefix_scan && !using_a_pch_file) {
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (cli_or_cx_enabled && !do_preprocessing_only) {
      /* If there were any preusing directives to implicitly #using one
         or more assemblies, process them now.  Note, #using do not
         depend on macro states.  Thus, they can be processed before the
         macro preinclude files. */
      process_preusings();
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* If there are preinclude files to be included at the beginning of
       the compilation, push the first file onto the stack.  The
       macro-only files must be scanned first. */
    next_preinclude_file = macro_preinclude_file_list;
    processing_macro_preincludes = TRUE;
    push_next_preinclude_file();
  }  /* if */
  /* Read the first line. */
  (void)read_logical_source_line(/*do_pop_on_end_of_file=*/TRUE,
                                 /*extend_current_line=*/FALSE);
}  /* open_primary_source_file */


void fe_init_for_pch_prefix_scan(void)
/*
Do initialization that is only required when precompiled header processing
is being done.  This is called prior to the initial scan of the
file prefix done by the precompiled header processing routines.
*/
{
  open_primary_source_file(/*pch_prefix_scan=*/TRUE);
}  /* fe_init_for_pch_prefix_scan */


void fe_init_part_2(void)
/*
Do the second phase of front end initialization to be done for each source
file compiled.  This part opens the primary source file to do the actual
compilation.  This is the initialization that occurs after determining whether
a precompiled header can be used to replace the initial portion of this
source file's compilation (whether or not a precompiled header ends up
being used).
*/
{
  /* The following (source file initialization) is done last so that any
     initialization errors or uses of source position will correctly
     identify the position as before the start of source. */
  /* Push the primary source input file onto the input stack.  Make
     a copy of the file name in IL storage. */
  open_primary_source_file(/*pch_prefix_scan=*/FALSE);
  if (is_primary_translation_unit) {
    /* This IL header initialization is not done earlier because the values
       from a precompiled header file cannot be used (as is done with the
       other allocated fields in the IL header). */
    /* Put the compiler version number into the IL header. */
    il_header.compiler_version = strcpy(
                                alloc_il((sizeof_t)(strlen(VERSION_NUMBER)+1)),
                                VERSION_NUMBER);
    /* Put the compilation time into the IL header. */
    il_header.time_of_compilation = strcpy(
                                alloc_il((sizeof_t)(strlen(curr_date_time)+1)),
                                curr_date_time);
#if ONE_INSTANTIATION_PER_OBJECT
    il_header.instantiation_dir_name = instantiation_dir_name == NULL ? NULL :
            strcpy(alloc_il((sizeof_t)(strlen(instantiation_dir_name)+1)),
                   instantiation_dir_name);
#endif /* ONE_INSTANTIATION_PER_OBJECT */
    il_header.target_configuration_index = target_configuration_index;
  }  /* if */
  if (using_a_pch_file) {
    /* The symbol table has been restored from a precompiled header file, so
       the symbols for the __DATE__ and __TIME__ macros have to be updated. */
    fixup_predefined_macros(curr_date_time);
    /* Since we are using input from a precompiled header file, we need
       to skip over the initial portion of the primary input file that
       corresponds to what has been obtained from the PCH.  Go into
       the "prefix scanning" mode.  This flag will be reset when we
       get to the last event obtained from the PCH. */
    building_pch_prefix = TRUE;
  }  /* if */
  /* The initial get_token call is not done yet because we may be doing
     preprocessing only, and the proper mode flags (like fetch_pp_tokens)
     are not yet set. */
}  /* fe_init_part_2 */


void fe_translation_unit_init(void)
/*
This routine is called to reinitialize variables that are specific to
a given translation unit, when multiple translation units are being
compiled (e.g., for export template processing).  This initialization
is also done implicitly during part 1 of the normal front end
initialization (i.e., by fe_init_part_1).  is_primary_translation_unit
is TRUE when the current translation is a primary file, and FALSE
when it is a secondary file.
*/
{
  /* Set a current position indicating we are still in initialization. */
  set_position_to(pos_curr_token, 0, SP_COL_UNKNOWN);
  set_err_pos_to_curr_token();

  attribute_trans_unit_init();
  mem_manage_trans_unit_init();
  host_envir_trans_unit_init();
  error_trans_unit_init();
  folding_trans_unit_init();
  il_trans_unit_init();
  interpret_trans_unit_init();
  decls_trans_unit_init();
  decl_inits_trans_unit_init();
  lexical_trans_unit_init();
  symbol_tbl_trans_unit_init();
  scope_stk_trans_unit_init();
  modules_trans_unit_init();
  templates_trans_unit_init();
  corresp_trans_unit_init();
  expr_trans_unit_init();
  exprutil_trans_unit_init();
  statements_trans_unit_init();
  class_decl_trans_unit_init();
  layout_trans_unit_init();
  macro_trans_unit_init();
  preproc_trans_unit_init();
#if DO_IL_LOWERING
  il_lower_trans_unit_init();
  lower_c99_trans_unit_init();
#endif /* DO_IL_LOWERING */
#if CPPCLI_ENABLING_POSSIBLE
  if (cli_or_cx_enabled) {
    ms_metadata_trans_unit_init(trans_unit_file_name);
  }  /* if */
#endif /* CPPCLI_ENABLING_POSSIBLE */
  sys_predef_trans_unit_init();
#if RECORD_MACROS_IN_IL
  il_header.macros = NULL;
#endif /* RECORD_MACROS_IN_IL */
#if MACRO_INVOCATION_TREE_IN_IL
  il_header.num_macro_invocation_records = 0;
  il_header.max_macro_invocation_depth = 0;
  il_header.root_macro_invocation_record_block = NULL;
#endif /* MACRO_INVOCATION_TREE_IN_IL */
#if MULTIPLE_THREAD_COMPILATION
  /* In multi-threaded environments consistent memory addresses cannot be
     reliably acquired, so PCH processing is immediately abandoned. */
  abandon_pch_processing();
#else /* !MULTIPLE_THREAD_COMPILATION */
  /* Suppress PCH processing on secondary translation units. */
  if (!is_primary_translation_unit) {
    abandon_pch_processing();
    using_a_pch_file = FALSE;
  }  /* if */
#endif /* MULTIPLE_THREAD_COMPILATION */
  /* Initialize the symbol table (keywords and predefined macros).  Note that
     keyword_init is called first, so that predefined macros will have
     priority over keywords.  Also, macro_init must have been called, so
     that predefined #assert predicates (if any) are entered after
     assert_predicates has been cleared.  Also, in Microsoft mode,
     target_init must have been called for correct handling of __int32 and
     __int64. */
  keyword_init();
  init_predefined_macros(curr_date_time);
  /* Create the file scope for this translation unit. */
  curr_translation_unit->primary_scope = new_file_scope(file_scope_number);
  /* Push an entry for the file scope onto the scope stack, saving the
     pointer to the scope in the translation unit entry.  This is done after
     the entry of keywords and predefined macros, because they do not belong
     to the file scope. */
  push_file_scope(/*is_reactivation=*/FALSE);
  check_assertion(curr_translation_unit->primary_scope->number ==
                                                           file_scope_number);
  /* il_header fields that are per-translation-unit: */
  il_header.primary_scope = curr_translation_unit->primary_scope;
  il_header.file_scope_statements = NULL;
  il_header.main_routine = NULL;
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  il_header.scope_orphaned_list_headers = NULL;
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
  il_header.nontag_types_used_in_exception_or_rtti = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  il_header.cli_metadata_files = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  il_header.imported_modules = NULL;
  predeclare_entities();
  if (is_primary_translation_unit) {
    /* We have to wait until now to create name linkage constants to
       ensure that the builtin types that are created for them do not
       get lost because of the per-translation-unit initializations above. */
    init_name_linkage_constants();
  } else {
    /* Preprocessing output cannot be generated for secondary translation
       units. */
    generate_pp_output = FALSE;
    do_preprocessing_only = FALSE;
  }  /* if */
#if DEBUG
  mem_manage_trans_unit_init_done();
#endif /* DEBUG */
}  /* fe_translation_unit_init */

#endif /* STANDALONE_UTILITY_PROGRAM */

void initialize_opname_names(void)
/*
Initialize opname_names from opname_kind_for_token and token_names.  (This
is done here instead of in lexical.c, where it would otherwise logically
belong, because the table is used by the C++-generating back end when built
as a standalone program, but nothing else from lexical.c is needed in that
configuration.)
*/
{
  int          tok_kind, opname_kind;
  a_const_char *str;

  (void)memzero((char *)opname_names, sizeof(opname_names));
  for (tok_kind = 0; tok_kind < (int)tok_last; tok_kind++) {
    opname_kind = opname_kind_for_token[tok_kind];
    if (opname_kind != (int)onk_none) {
      str = token_names[tok_kind];
      /* A few opname kinds are made up of two tokens and require some
         special handling. */
      if (opname_kind == (int)onk_function_call) {
        str = "()";
      } else if (opname_kind == (int)onk_subscript) {
        str = "[]";
      }  /* if */
      opname_names[opname_kind] = str;
    }  /* if */
  }  /* for */
  /* new[] and delete[] do not map to a single token. */
  opname_names[(int)onk_array_new] = "new[]";
  opname_names[(int)onk_array_delete] = "delete[]";
#if CHECKING
  /* Make sure all the slots were initialized. */
  for (opname_kind = (int)onk_none+1;
       opname_kind < (int)onk_last;
       opname_kind++) {
    if (opname_names[opname_kind] == NULL) {
      internal_error("initialize_opname_kinds: bad init of opname_names");
    }  /* if */
  }  /* for */
#endif /* CHECKING */
}  /* initialize_opname_names */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

