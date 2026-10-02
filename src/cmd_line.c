/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

cmd_line.c -- Command-line parsing.

*/

/* Preserve macro definitions in targ_def.h so that dump_configuration_macros
   below can access them. */
#define DO_NOT_UNDEF_TARGET_MACROS 1

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Additional header files. */
#include "interpret.h"
#include "macro.h"
#include "pch.h"
#if IL_SHOULD_BE_WRITTEN_TO_FILE
#include "il_write.h"
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
#if BACK_END_IS_C_GEN_BE
/* c_gen_be.h is needed only for module_list_for_union_init, and that's
   used only when generating K&R C. */
#if !C_GEN_BE_GENERATES_ANSI_C
#include "c_gen_be.h"
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
#endif /* BACK_END_IS_C_GEN_BE */
#include "layout.h"

#ifdef HOSTID
extern long gethostid(void);
#endif /* HOSTID */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE


STATIC_THREAD a_def_undef_string_ptr
		last_defs_from_cmd_line;
			/* Points to the last element in the list
			   defs_from_cmd_line. */

/*
Structure used to build a list of the severities requested for the
diagnostics that calls of __builtin_constexpr_diag produce with a given tag.
*/
typedef struct a_constexpr_diag_tag *a_constexpr_diag_tag_ptr;
struct a_constexpr_diag_tag {
  a_constexpr_diag_tag_ptr
		next;	/* Pointer to the next entry in a linked list of
			   requested severities. */
  a_const_char	*tag;
			/* The tag to which this entry applies. */
  an_error_severity
		severity;
			/* The severity to be used for a diagnostic carrying
			   this tag, or es_none to suppress it. */
};

STATIC_THREAD a_constexpr_diag_tag_ptr
		constexpr_diag_tags,
			/* Points to the list of the severities requested by
			   --constexpr_diag_* options. */
		last_constexpr_diag_tag;
			/* Points to the last element in the list
			   constexpr_diag_tags. */

/*
Structure used to map keyword and/or letter options into the
corresponding option kind.  There may be multiple option descriptions
that map to the same option kind.
*/
typedef struct an_option_description *an_option_description_ptr;
typedef struct an_option_description {
  an_option_kind
		kind;
			/* Code that indicates the action to be taken
			   when this option is used. */
  a_const_char	*keyword;
			/* The keyword option used to specify this option.
			   May be NULL if a keyword option may not be used. */
  char		letter;
			/* A single character that may be used to specify
			   this option.  May be the null character if
			   a single character option may not be used. */
  a_byte_boolean
	       	value;
			/* TRUE if the option is used to enable the
			   option, FALSE if it should disable it. */
  a_byte_boolean
	       	arg_required;
			/* TRUE if this option requires that an argument be
			   specified. */
  a_byte_boolean
		enabled;
			/* TRUE if the feature with which this option is
			   associated is enabled in the current
			   configuration. */
  sizeof_t	keyword_length;
			/* Length of the keyword (not including the null
			   terminator). */
  a_pch_event_kind
		pch_event_kind;
			/* Indicates how a given command line is to be
			   handled for purposes of precompiled header
			   prefix matching. */
} an_option_description;

#define SIZE_OF_OPTION_DESCRIPTIONS ((int)optk_last*2)
 			/* The number of entries in the option descriptions
			   array.  This is larger than the number of options
			   because some options have entries to both enable
			   and disable the option.  Also, it is possible
			   to have synonyms for options. */

STATIC_THREAD an_option_description
		option_descriptions[SIZE_OF_OPTION_DESCRIPTIONS];
			/* Pointer to a linked list of option descriptions. */

STATIC_THREAD int
		option_descriptions_used;
			/* The number of entries that are used in the option
			   description array. */

STATIC_THREAD a_byte_boolean
		option_kind_used[(int)optk_last+1];
			/* An array indexed by option kind that indicates
			   whether the option kind has been specified in
			   the command line. */

STATIC_THREAD a_boolean
		old_style_preprocessing;
			/* TRUE if old-style preprocessing should be
			   used in ANSI C or C++ mode. */

#if MICROSOFT_EXTENSIONS_ALLOWED
STATIC_THREAD a_boolean
                force_ms_type_info_not_in_namespace_std;
                        /* Used as the target of a --set_flag command-line
                           option to force type_info_in_namespace_std to a
                           value of FALSE. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */


static void add_config_dependent_option_description(
				an_option_kind		kind,
				a_const_char		*keyword,
				char			letter,
				a_boolean		value,
				a_boolean		arg_required,
				a_pch_event_kind	pch_event_kind,
				a_boolean		enabled)

/*
Add an entry to the linked list of option descriptions.  "keyword" is
the string to be used as the keyword form of the option and may be
NULL if there is no keyword version of the option.  "letter" is the
option letter to be used for letter style options.  It may be
the null character (\0) if no letter form of the option exists.
"value" indicates whether this option is used to turn the option
on (TRUE) or off (FALSE).  "arg_required" indicates whether an
option must be followed by an argument.  Note that optional arguments
are not supported.  "pch_event_kind" specifies how this argument should
be compared with a similar argument for precompiled header prefix
matching.  "enabled" indicates whether or not the feature with which this
option is associated is enabled in the current configuration.  If "enabled"
is FALSE an error will be issued if the option is used.
*/
{
  int				option_description_number;
  an_option_description_ptr	odp;
#if EXPENSIVE_CHECKING
  {
    int	n;
    /* Make sure the option keyword and/or letter are not already in use. */
    for (n = 0; n < option_descriptions_used; ++n) {
      odp = &option_descriptions[n];
      if ((keyword != NULL && odp->keyword != NULL &&
           strcmp(keyword, odp->keyword) == 0) ||
          (letter != '\0' && letter == odp->letter)) {
        unexpected_condition_str2("add_option_description:",
                                  "duplicate option keyword or letter");
      }  /* if */
    }  /* for */
  }
#endif /* EXPENSIVE_CHECKING */
  /* alloc_general is called (rather than alloc_fe) because the general
     mem_manage.c routines are not yet initialized. */
  option_description_number = option_descriptions_used++;
  if (option_description_number == SIZE_OF_OPTION_DESCRIPTIONS) {
    /* There are more entries than fit in the array. */
#if DEBUG
    fprintf(f_debug, "Too many options descriptions.  Current limit is %d\n",
            SIZE_OF_OPTION_DESCRIPTIONS);
#endif /* DEBUG */
    unexpected_condition();
  } else {
    odp = &option_descriptions[option_description_number];
    odp->kind = kind;
    odp->keyword = keyword;
    odp->keyword_length = keyword == NULL ? 0 : strlen(keyword);
    odp->letter = letter;
    odp->value = value;
    odp->arg_required = arg_required;
    odp->pch_event_kind = pch_event_kind;
    odp->enabled = enabled;
  }  /* if */
}  /* add_config_dependent_option_description */


static void add_option_description(an_option_kind	kind,
				   a_const_char		*keyword,
				   char			letter,
				   a_boolean		value,
				   a_boolean		arg_required,
				   a_pch_event_kind	pch_event_kind)
/*
Interface to add_config_dependent_option_description that provides
a default value for the "enabled" parameter.
*/
{
  add_config_dependent_option_description(kind, keyword, letter, value,
                                          arg_required, pch_event_kind,
                                          /*enabled=*/TRUE);
}  /* add_option_description */


static void initialize_option_descriptions(void)
/*
Initialize the option information table.
*/
{
  add_option_description(optk_strict_ansi_error, "strict", 'A',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_strict_ansi_warning, "strict_warnings", 'a',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_preprocess_only_no_line_dirs, "no_line_commands",
                         'P', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_preprocess_only_emit_line_dirs, "preprocess",
                         'E', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_keep_comments_in_pp_output, "comments", 'C',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
  add_option_description(optk_old_line_dirs, "old_line_commands", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
  add_option_description(optk_C_dialect_pcc, "old_c", 'K',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_list_makefile_dependencies, "dependencies", 'M',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_list_include_files, "trace_includes", 'H',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
#if DO_IL_LOWERING
  add_option_description(optk_write_unlowered_il, "no_il_lowering", 'N',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* DO_IL_LOWERING */
#if NEED_IL_DISPLAY
  add_option_description(optk_il_display, "il_display", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* NEED_IL_DISPLAY */
  add_option_description(optk_cplusplus_anachronisms, "anachronisms", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_cplusplus_anachronisms, "no_anachronisms", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_cfront_2_1_mode, "cfront_2.1", 'b',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_cfront_3_0_mode, "cfront_3.0", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_front_end_only, "no_code_gen", 'n',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_use_signed_chars, "signed_chars", 's',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_use_signed_chars, "unsigned_chars", 'u',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_template_instantiation_mode, "instantiate", 't',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  add_option_description(optk_automatic_template_instantiation,
                         "auto_instantiation", 'T',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_automatic_template_instantiation,
                         "no_auto_instantiation", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_ii_file_name,
                         "ii_file", '\0',
                         /*value=*/FALSE, /*arg_required=*/TRUE,
                         pchek_none);
  add_option_description(optk_suppress_instantiation_flags,
                         "suppress_instantiation_flags", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_template_info_file,
                         "template_info_file",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
  add_option_description(optk_definition_list_file_name,
                         "definition_list_file",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
#if EXPORT_ENABLING_POSSIBLE
  add_option_description(optk_exported_template_file_name,
                         "exported_template_file",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
  add_option_description(optk_template_directory, "template_directory", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
#endif /* EXPORT_ENABLING_POSSIBLE */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
  add_option_description(optk_implicit_template_inclusion,
                         "implicit_include", 'B',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_implicit_template_inclusion,
                         "no_implicit_include", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
  add_option_description(optk_virtual_function_table_definition,
                         "suppress_vtbl", 'V',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_virtual_function_table_definition,
                         "force_vtbl", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_allow_dollar_in_id_chars,
                         "dollar", '$',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_display_compilation_time, "timing", '#',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_display_compiler_version, "version", 'v',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_suppress_warnings, "no_warnings", 'w',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_promote_warnings, "promote_warnings", 'W',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_enable_remarks, "remarks", 'r',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_C_mode, "c", 'm',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_C_dialect_cplusplus, "c++", 'p',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_exception_handling, "exceptions", 'x',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_exception_handling, "no_exceptions", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_suppress_used_before_set_warnings,
                         "no_use_before_set_warnings", 'j',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_include_directory, "include_directory", 'I',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_embed_dir, "embed_directory", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_define_macro, "define_macro", 'D',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_undefine_macro, "undefine_macro", 'U',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_set_error_limit, "error_limit", 'e',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
  add_option_description(optk_generate_raw_listing, "list", 'L',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
  add_option_description(optk_generate_cross_reference, "xref", 'X',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
  add_option_description(optk_stderr_file_name, "error_output", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
  add_option_description(optk_output_file_name, "output", 'o',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
#if BACK_END_IS_C_GEN_BE
  add_option_description(optk_module_list_for_union_init, "module_init", 'i',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
#endif /* BACK_END_IS_C_GEN_BE */
#if DEBUG
  add_option_description(optk_debug, "db", 'd',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
  add_option_description(optk_debug_name, "db_name", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
#if MAINTAIN_ALLOCATION_SEQUENCE_NUMBER
  add_option_description(optk_debug_alloc_seq, "db_alloc_seq", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
#endif /* MAINTAIN_ALLOCATION_SEQUENCE_NUMBER */
#endif /* DEBUG */
#if !EDG_WIN32 && !MULTIPLE_THREAD_COMPILATION
  /* This option is only available on Unix and only works at the process
     level. */
  add_option_description(optk_time_limit, "time_limit", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
#endif /* !EDG_WIN32 && !MULTIPLE_THREAD_COMPILATION */
  add_option_description(optk_diag_suppress, "diag_suppress", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_diag_remark, "diag_remark", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_diag_warning, "diag_warning", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_diag_error, "diag_error", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_diag_once, "diag_once", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_constexpr_diag_suppress,
                         "constexpr_diag_suppress", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_constexpr_diag_remark,
                         "constexpr_diag_remark", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_constexpr_diag_warning,
                         "constexpr_diag_warning", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_constexpr_diag_error,
                         "constexpr_diag_error", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_display_error_number, "display_error_number",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_display_error_number, "no_display_error_number",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_none);
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
  add_option_description(optk_gen_c_file_name, "gen_c_file_name",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
  /* --msvc_target_version is supported for compatibility but deprecated
     in favor of regularity with --gen_c_{msvc|gnu|clang}_version. */
  add_option_description(optk_msvc_target_version, "msvc_target_version",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_msvc_target_version, "gen_c_msvc_version",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_gnu_target_version, "gen_c_gnu_version",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_clang_target_version, "gen_c_clang_version",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
  add_option_description(optk_create_pch, "create_pch",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
  add_option_description(optk_use_pch, "use_pch",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
  add_option_description(optk_pch, "pch",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_pch_messages, "pch_messages",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_pch_messages, "no_pch_messages",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_pch_verbose, "pch_verbose",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_pch_verbose, "no_pch_verbose",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_none);
#if USE_FIXED_ADDRESS_FOR_MMAP
  add_option_description(optk_fixed_address_for_mmap, "mmap_address",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
#endif /* USE_FIXED_ADDRESS_FOR_MMAP */
#if !USE_MMAP_FOR_MEMORY_REGIONS
  add_option_description(optk_pch_mem, "pch_mem",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
#endif /* !USE_MMAP_FOR_MEMORY_REGIONS */
  add_option_description(optk_pch_dir, "pch_dir",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_wdir, "wdir",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_create_module_header_unit, "create_header_unit",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
  add_option_description(optk_create_module_interface_unit,
                         "create_module_interface",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
  add_option_description(optk_create_module_internal_unit,
                         "create_module_internal_partition",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
  add_option_description(optk_map_module_header_unit, "header_unit", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_restrict, "restrict",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_restrict, "no_restrict",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_long_lifetime_temps, "long_lifetime_temps",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_long_lifetime_temps, "short_lifetime_temps",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if MICROSOFT_EXTENSIONS_ALLOWED
  add_option_description(optk_microsoft_mode, "microsoft",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_microsoft_mode, "no_microsoft",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_microsoft_version, "microsoft_version",
                         '\0', /*value=*/FALSE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_microsoft_build_number, "microsoft_build_number",
                         '\0', /*value=*/FALSE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_microsoft_bugs, "microsoft_bugs",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_microsoft_bugs, "no_microsoft_bugs",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_microsoft_compatibility, "ms_compatibility",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_microsoft_compatibility, "no_ms_compatibility",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_microsoft_extensions, "ms_extensions",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_microsoft_extensions, "no_ms_extensions",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_microsoft_cpp14_mode, "ms_c++14",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_microsoft_cpp17_mode, "ms_c++17",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_microsoft_cpp20_mode, "ms_c++20",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_microsoft_cpp23_mode, "ms_c++23",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_microsoft_cpplatest_mode, "ms_c++latest",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_microsoft_c11, "ms_c11",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_microsoft_c17, "ms_c17",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_microsoft_c23, "ms_c23",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_microsoft_await, "ms_await",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_microsoft_await_strict, "ms_await_strict",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if NEAR_AND_FAR_ALLOWED
  add_option_description(optk_microsoft_16_mode, "microsoft_16",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* NEAR_AND_FAR_ALLOWED */
#if CPPCLI_ENABLING_POSSIBLE
  add_option_description(optk_cppcli, "cppcli",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_cppcli, "no_cppcli",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_cppcli, "c++cli",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_cppcli, "no_c++cli",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_cppcli, "clr",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* CPPCLI_ENABLING_POSSIBLE */
#if CPPCX_ENABLING_POSSIBLE
  add_option_description(optk_cppcx, "cppcx",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_cppcx, "no_cppcx",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_cppcx, "c++cx",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_cppcx, "no_c++cx",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_vcmeta_directory_name, "vcmeta_directory",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
#endif /* CPPCX_ENABLING_POSSIBLE */
#if CPPCLI_ENABLING_POSSIBLE
  add_option_description(optk_preusing, "preusing",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_assembly_using_dir, "using_directory",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_using_framework_directory,
                         "using_framework_directory",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_using_framework_directory,
                         "no_using_framework_directory",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_mscorlib_file_name, "mscorlib_file_name",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
#endif /* CPPCLI_ENABLING_POSSIBLE */
  add_option_description(optk_ms_permissive, "ms_permissive",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_ms_permissive, "no_ms_permissive",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_ms_rvalue_cast, "ms_rvalue_cast",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_ms_rvalue_cast, "no_ms_rvalue_cast",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_ms_strict_ternary, "ms_strict_ternary",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_ms_strict_ternary, "no_ms_strict_ternary",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_ms_cplusplus_std_value,
                         "ms_cplusplus_std_value",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_ms_cplusplus_std_value,
                         "no_ms_cplusplus_std_value",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_ms_stdc, "ms_stdc",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_ms_stdc, "no_ms_stdc",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NEAR_AND_FAR_ALLOWED
  add_option_description(optk_far_data_pointers, "far_data_pointers",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_far_data_pointers, "near_data_pointers",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_far_code_pointers, "far_code_pointers",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_far_code_pointers, "near_code_pointers",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* NEAR_AND_FAR_ALLOWED */
#if WCHAR_T_ENABLING_POSSIBLE
  add_option_description(optk_wchar_t_is_keyword, "wchar_t_keyword",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* WCHAR_T_ENABLING_POSSIBLE */
  add_option_description(optk_wchar_t_is_keyword, "no_wchar_t_keyword",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  /* Note -- the Microsoft-style "-Zpn" option is not supported.  The driver
     that invokes the front end may convert it to "--pack_alignment=n". */
  add_option_description(optk_pack_alignment, "pack_alignment",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_alternative_tokens,
			 "alternative_tokens",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_alternative_tokens,
			 "no_alternative_tokens",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if MINIMAL_INLINING
  add_option_description(optk_inlining, "inlining",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_inlining, "no_inlining",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_inline_statement_limit, "inline_statement_limit",
                         '\0', /*value=*/FALSE, /*arg_required=*/TRUE,
                         pchek_command_line);
#endif /* MINIMAL_INLINING */
  add_option_description(optk_SVR4_C_mode,
			 "svr4",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_SVR4_C_mode,
			 "no_svr4",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_brief_diagnostics,
			 "brief_diagnostics",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_brief_diagnostics,
			 "no_brief_diagnostics",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_nonconst_ref_anachronism,
                         "nonconst_ref_anachronism",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_nonconst_ref_anachronism,
                         "no_nonconst_ref_anachronism",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_no_preproc_only,
                         "no_preproc_only",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if RTTI_ENABLING_POSSIBLE
  add_option_description(optk_rtti, "rtti", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* RTTI_ENABLING_POSSIBLE */
  add_option_description(optk_rtti, "no_rtti", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_building_runtime, "building_runtime", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if BOOL_ENABLING_POSSIBLE
  add_option_description(optk_bool_is_keyword, "bool",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* BOOL_ENABLING_POSSIBLE */
  add_option_description(optk_bool_is_keyword, "no_bool",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE
  add_option_description(optk_array_new_and_delete,
                         "array_new_and_delete", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE */
  add_option_description(optk_array_new_and_delete,
                         "no_array_new_and_delete", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_explicit,
                         "explicit", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_explicit,
                         "no_explicit", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_namespaces,
                         "namespaces", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_namespaces,
                         "no_namespaces", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_implicit_using_std,
                         "using_std", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_implicit_using_std,
                         "no_using_std", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if MAINTAIN_NEEDED_FLAGS
  add_option_description(optk_remove_unneeded_entities,
                         "remove_unneeded_entities", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* MAINTAIN_NEEDED_FLAGS */
  add_option_description(optk_remove_unneeded_entities,
                         "no_remove_unneeded_entities", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_typename,
                         "typename", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_typename,
                         "no_typename", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_implicit_typename,
                         "implicit_typename", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_implicit_typename,
                         "no_implicit_typename", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_special_subscript_cost,
                         "special_subscript_cost", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_special_subscript_cost,
                         "no_special_subscript_cost", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_old_style_preprocessing,
                         "old_style_preprocessing", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_old_for_init,
                         "old_for_init", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_old_for_init,
                         "new_for_init", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_for_init_diff_warning,
                         "for_init_diff_warning", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_for_init_diff_warning,
                         "no_for_init_diff_warning", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_distinct_template_signatures,
                         "distinct_template_signatures", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_distinct_template_signatures,
                         "no_distinct_template_signatures", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_guiding_decls,
                         "guiding_decls", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_guiding_decls,
                         "no_guiding_decls", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_old_specializations,
                         "old_specializations", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_old_specializations,
                         "no_old_specializations", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_wrap_diagnostics,
			 "wrap_diagnostics",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_wrap_diagnostics,
			 "no_wrap_diagnostics",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_none);
#if IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE
  add_option_description(optk_implicit_extern_c_type_conversion,
			 "implicit_extern_c_type_conversion",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_implicit_extern_c_type_conversion,
			 "no_implicit_extern_c_type_conversion",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE */
  add_option_description(optk_long_preserving_rules,
                         "long_preserving_rules",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_long_preserving_rules,
                         "no_long_preserving_rules",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_extern_inline,
			 "extern_inline",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_extern_inline,
			 "no_extern_inline",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
  add_option_description(optk_multibyte_chars,
                         "multibyte_chars",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_multibyte_chars,
                         "no_multibyte_chars",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
  add_option_description(optk_embedded_cplusplus,
                         "embedded_c++",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if VLA_ALLOWED
  add_option_description(optk_vla,
			 "vla",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_vla,
			 "no_vla",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* VLA_ALLOWED */
  add_option_description(optk_enum_overloading,
                         "enum_overloading",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_enum_overloading,
                         "no_enum_overloading",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_nonstandard_qualifier_deduction,
                         "nonstd_qualifier_deduction",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_nonstandard_qualifier_deduction,
                         "no_nonstd_qualifier_deduction",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if ONE_INSTANTIATION_PER_OBJECT
  add_option_description(optk_one_instantiation_per_object,
                         "one_instantiation_per_object",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_instantiation_dir,
                         "instantiation_dir",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  add_option_description(optk_late_tiebreaker,
                         "late_tiebreaker",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_late_tiebreaker,
                         "early_tiebreaker",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_preinclude, "preinclude", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_preinclude_macros, "preinclude_macros", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_pending_instantiations,
                         "pending_instantiations",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
#if MICROSOFT_EXTENSIONS_ALLOWED
  add_option_description(optk_import_dir,
                         "import_dir",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  add_option_description(optk_const_string_literals,
                         "const_string_literals",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_const_string_literals,
                         "no_const_string_literals",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_class_name_injection,
                         "class_name_injection",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_class_name_injection,
                         "no_class_name_injection",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_arg_dependent_lookup,
                         "arg_dep_lookup",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_arg_dependent_lookup,
                         "no_arg_dep_lookup",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_friend_injection,
                         "friend_injection",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_friend_injection,
                         "no_friend_injection",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_nonstandard_using_decl,
                         "nonstd_using_decl",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_nonstandard_using_decl,
                         "no_nonstd_using_decl",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_designators,
                         "designators",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_designators,
                         "no_designators",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_extended_designators,
                         "extended_designators",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_extended_designators,
                         "no_extended_designators",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_variadic_macros,
                         "variadic_macros",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_variadic_macros,
                         "no_variadic_macros",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_extended_variadic_macros,
                         "extended_variadic_macros",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_extended_variadic_macros,
                         "no_extended_variadic_macros",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_system_include_dir, "sys_include", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_include_file_suffixes, "incl_suffixes", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_module_dir, "modules_directory", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_ms_module_file_map, "ms_mod_file_map", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_ms_header_unit, "ms_header_unit", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_ms_header_unit_quote, "ms_header_unit_quote",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_ms_header_unit_angle, "ms_header_unit_angle",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_ms_mod_translate_include, "ms_translate_include",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_ms_mod_translate_include,
                         "no_ms_translate_include", '\0', /*value=*/FALSE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_modules, "modules", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_modules, "no_modules", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_module_import_diagnostics,
                         "module_import_diagnostics",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_module_import_diagnostics,
                         "no_module_import_diagnostics",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_module_interface, "module_interface",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_module_internal_partition,
                         "module_internal_partition",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
#if COMPOUND_LITERAL_ENABLING_POSSIBLE
  add_option_description(optk_compound_literals,
                         "compound_literals",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_compound_literals,
                         "no_compound_literals",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* COMPOUND_LITERAL_ENABLING_POSSIBLE */
  add_option_description(optk_base_assign_op_is_default,
                         "base_assign_op_is_default",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_base_assign_op_is_default,
                         "no_base_assign_op_is_default",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if SUN_EXTENSIONS_ALLOWED
  add_option_description(optk_sun_mode,
                         "sun",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_sun_mode,
                         "no_sun",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_sun_linker_scope,
                         "sun_linker_scope",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_sun_linker_scope,
                         "no_sun_linker_scope",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* SUN_EXTENSIONS_ALLOWED */
  add_option_description(optk_dependent_name_processing,
                         "dep_name",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_dependent_name_processing,
                         "no_dep_name",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_ignore_namespace_std,
                         "ignore_std",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_parse_nonclass_templates,
                         "parse_templates",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_parse_nonclass_templates,
                         "no_parse_templates",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if C99_IL_EXTENSIONS_SUPPORTED
  /* The C99 IL extensions are required to provide full C99 support.
     The C99 command-line options are only enabled when the front end
     is configured to provide full support. */
  add_option_description(optk_c99_mode,
                         "c99",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_c99_mode,
                         "no_c99",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  /* C11 is a successor of C99, and hence also requires C99 IL extensions. */
  add_option_description(optk_c11_mode,
                         "c11",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  /* C18 is similarly a successor of C11.  (C18 was planned to be "C17" and
     it is sometimes referred to as such.  Therefore, we accept both "--c17"
     and "--c18" to enable C18 mode.) */
  add_option_description(optk_c18_mode,
                         "c18",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_c18_mode,
                         "c17",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_c23_mode,
                         "c23",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  /* Usually, C89 is the default C mode.  However, in configurations that
     enable another ANSI-based dialect by default (e.g., C99 or SVR4 C), the
     option --c89 is a convenient way to ensure that only C89 constructs are
     allowed. */
  add_option_description(optk_c89_mode,
                         "c89",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if EXPORT_ENABLING_POSSIBLE
  add_option_description(optk_export_template,
                         "export",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_export_template,
                         "no_export",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* EXPORT_ENABLING_POSSIBLE */
#if DEFAULT_PASS_STDARG_REFERENCES_TO_GENERATED_CODE
  /* When passing stdarg references to the back end by default, give the
     ability to turn off this feature.  When not passing such references,
     we do not give the ability to turn the feature on. */
  add_option_description(optk_stdarg_builtin,
                         "no_stdarg_builtin",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* DEFAULT_PASS_STDARG_REFERENCES_TO_GENERATED_CODE */
#if ENABLE_TRANS_UNIT_TEST_MODE
  add_option_description(optk_trans_unit_test_mode,
                         "trans_unit_test",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* ENABLE_TRANS_UNIT_TEST_MODE */
#if GNU_EXTENSIONS_ALLOWED
  add_option_description(optk_gcc_mode,
                         "gcc",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_gcc_mode,
                         "no_gcc",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_gpp_mode,
                         "g++",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_gpp_mode,
                         "no_g++",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_clang_mode, "clang", '\0', /*value=*/TRUE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_clang_mode, "no_clang", '\0', /*value=*/FALSE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_gnu_version, "gnu_version",
                         '\0', /*value=*/FALSE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_clang_version, "clang_version", '\0',
                         /*value=*/FALSE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_report_gnu_extensions, "report_gnu_extensions",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_short_enums,
                         "short_enums",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_short_enums,
                         "no_short_enums",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_strict_gnu, "strict_gnu",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_strict_gnu, "no_strict_gnu",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* GNU_EXTENSIONS_ALLOWED */
  add_option_description(optk_long_long,
                         "long_long",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_context_limit, "context_limit", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
  add_option_description(optk_set_flag, "set_flag", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_set_flag, "clear_flag", '\0',
                         /*value=*/FALSE, /*arg_required=*/TRUE,
                         pchek_command_line);
#if UPC_EXTENSIONS_ALLOWED
  add_option_description(optk_upc_mode,
                         "upc",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_upc_mode,
                         "no_upc",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_upc_strict_access,
                         "upc_strict",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_upc_strict_access,
                         "upc_relaxed",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_upc_threads,
                         "upc_threads",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
#endif /* UPC_EXTENSIONS_ALLOWED */
#if FIXED_POINT_ALLOWED
  add_option_description(optk_fixed_point, "fixed_point", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_fixed_point, "no_fixed_point", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* FIXED_POINT_ALLOWED */
#if NAMED_ADDRESS_SPACES_ALLOWED
  add_option_description(optk_named_address_spaces, "named_address_spaces",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_named_address_spaces, "no_named_address_spaces",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
  add_option_description(optk_edg_base_directory,
                         "edg_base_dir",
                         '\0', /*value=*/FALSE, /*arg_required=*/TRUE,
                         pchek_command_line);
#if NAMED_REGISTERS_ALLOWED
  add_option_description(optk_named_registers, "named_registers", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_named_registers, "no_named_registers", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* NAMED_REGISTERS_ALLOWED */
  add_config_dependent_option_description(
               optk_embedded_c, "embedded_c", '\0', /*value=*/TRUE,
               /*arg_required=*/FALSE, pchek_command_line, EMBEDDED_C_ALLOWED);
  add_config_dependent_option_description(
               optk_embedded_c, "no_embedded_c", '\0', /*value=*/FALSE,
               /*arg_required=*/FALSE, pchek_command_line, EMBEDDED_C_ALLOWED);
  add_option_description(optk_thread_local_storage, "thread_local_storage",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_thread_local_storage, "no_thread_local_storage",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if FULLY_RESOLVED_MACRO_POSITIONS
  add_option_description(optk_macro_positions_in_diagnostics,
                         "macro_positions_in_diagnostics",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_macro_positions_in_diagnostics,
                         "no_macro_positions_in_diagnostics",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_none);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  add_option_description(optk_trigraphs,
			 "trigraphs",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_trigraphs,
			 "no_trigraphs",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_nonstandard_default_arg_deduction,
                         "nonstd_default_arg_deduction",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_nonstandard_default_arg_deduction,
                         "no_nonstd_default_arg_deduction",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_stdc_zero_in_system_headers,
                         "stdc_zero_in_system_headers",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_stdc_zero_in_system_headers,
                         "no_stdc_zero_in_system_headers",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_template_typedefs_in_diagnostics,
                         "template_typedefs_in_diagnostics",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_template_typedefs_in_diagnostics,
                         "no_template_typedefs_in_diagnostics",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if FUNCTION_PROTOTYPE_INSTANTIATION_DEFERRAL_ALLOWED
  add_option_description(optk_defer_parse_function_templates,
                         "defer_parse_function_templates",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_defer_parse_function_templates,
                         "no_defer_parse_function_templates",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* FUNCTION_PROTOTYPE_INSTANTIATION_DEFERRAL_ALLOWED */
  add_option_description(optk_uliterals,
                         "uliterals",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_uliterals,
                         "no_uliterals",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if MICROSOFT_EXTENSIONS_ALLOWED
  add_option_description(optk_default_calling_convention,
                         "default_calling_convention",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  add_option_description(optk_type_traits_helpers,
                         "type_traits_helpers",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_type_traits_helpers,
                         "no_type_traits_helpers",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if CPP11_IL_EXTENSIONS_SUPPORTED
  /* These C++11 command-line options are only enabled when the back end
     (and possibly runtime library) can provide support for the features. */
  add_option_description(optk_cpp11_mode, "c++11", '\0', /*value=*/TRUE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_cpp11_mode, "no_c++11", '\0', /*value=*/FALSE,
                         /*arg_required=*/FALSE, pchek_command_line);
  /* Before the C++11 name was adopted, C++0x was used.  Keep the old
     option names for compatibility. */
  add_option_description(optk_cpp11_mode, "c++0x", '\0', /*value=*/TRUE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_cpp11_mode, "no_c++0x", '\0', /*value=*/FALSE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_lambdas, "lambdas", '\0', /*value=*/TRUE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_lambdas, "no_lambdas", '\0', /*value=*/FALSE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_rvalue_references, "rvalue_refs",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_rvalue_references, "no_rvalue_refs",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_nullptr, "nullptr", '\0', /*value=*/TRUE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_nullptr, "no_nullptr", '\0', /*value=*/FALSE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_rvalue_ctor_is_copy_ctor,
                         "rvalue_ctor_is_copy_ctor",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_rvalue_ctor_is_copy_ctor,
                         "rvalue_ctor_is_not_copy_ctor",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_gen_move_operations,
                         "gen_move_operations",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_gen_move_operations,
                         "no_gen_move_operations",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_c23_typeof,
                         "c23_typeof", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_c23_typeof,
                         "no_c23_typeof", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_cpp11_sfinae,
                         "c++11_sfinae", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_cpp11_sfinae,
                         "no_c++11_sfinae", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_cpp11_sfinae_ignore_access,
                         "c++11_sfinae_ignore_access", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_cpp11_sfinae_ignore_access,
                         "no_c++11_sfinae_ignore_access", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_variadic_templates,
                         "variadic_templates", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_variadic_templates,
                         "no_variadic_templates", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  /* C++14 and C++17 are successors of C++11, and hence also require C++11 IL
     extensions. */
  add_option_description(optk_cpp14_mode, "c++14", '\0', /*value=*/TRUE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_cpp17_mode, "c++17", '\0', /*value=*/TRUE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_cpp20_mode, "c++20", '\0', /*value=*/TRUE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_cpp23_mode, "c++23", '\0', /*value=*/TRUE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_cpp26_mode, "c++26", '\0', /*value=*/TRUE,
                         /*arg_required=*/FALSE, pchek_command_line);
#endif /* CPP11_IL_EXTENSIONS_SUPPORTED */
  add_option_description(optk_list_macros, "list_macros", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE, pchek_none);
  add_option_description(optk_top_templates, "top_templates", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE, pchek_none);
#if DUMP_CONFIG_ENABLED
  add_option_description(optk_dump_configuration, "dump_configuration",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_dump_legacy_as_target,
                         "dump_legacy_as_target",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
#endif /* DUMP_CONFIG_ENABLED */
  add_option_description(optk_signed_bit_fields, "signed_bit_fields",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_unsigned_bit_fields, "unsigned_bit_fields",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_check_concatenations, "check_concatenations",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_check_concatenations, "no_check_concatenations",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if UNICODE_SOURCE_SUPPORTED
  add_option_description(optk_unicode_source_kind,
                         "unicode_source_kind",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
#endif /* UNICODE_SOURCE_SUPPORTED */
  add_option_description(optk_auto_type, "auto_type", '\0', /*value=*/TRUE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_auto_type, "no_auto_type", '\0', /*value=*/FALSE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_auto_storage, "auto_storage",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_auto_storage, "no_auto_storage",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_nonstandard_instantiation_lookup,
                         "nonstd_instantiation_lookup",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_nonstandard_instantiation_lookup,
                         "no_nonstd_instantiation_lookup",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if GNU_EXTENSIONS_ALLOWED
  add_option_description(optk_gnu_c89_inlining, "gcc89_inlining", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_nonstd_gnu_keywords, "nonstd_gnu_keywords", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_nonstd_gnu_keywords, "no_nonstd_gnu_keywords",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_default_nocommon,
                         "default_nocommon_tentative_definitions", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_default_nocommon,
                         "default_common_tentative_definitions", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* GNU_EXTENSIONS_ALLOWED */
  add_option_description(optk_token_separators_in_pp_output,
                         "no_token_separators_in_pp_output", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_cpp03_mode, "c++03", '\0', /*value=*/TRUE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_func_prototype_tags, "func_prototype_tags", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_func_prototype_tags, "no_func_prototype_tags",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_require_func_prototypes,
                         "require_func_prototypes", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_require_func_prototypes,
                         "no_require_func_prototypes", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_implicit_noexcept, "implicit_noexcept", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_implicit_noexcept, "no_implicit_noexcept",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_unrestricted_unions, "unrestricted_unions", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_unrestricted_unions, "no_unrestricted_unions",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_max_depth_constexpr_call,
                         "max_depth_constexpr_call",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
  add_option_description(optk_max_cost_constexpr_call,
                         "max_cost_constexpr_call",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
  add_option_description(optk_delegating_constructors,
                         "delegating_constructors", '\0', /*value=*/TRUE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_delegating_constructors,
                         "no_delegating_constructors", '\0', /*value=*/FALSE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_lossy_warning, "lossy_conversion_warning", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_lossy_warning, "no_lossy_conversion_warning",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_deprecated_string_conv, "deprecated_string_conv",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_deprecated_string_conv,
                         "no_deprecated_string_conv",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_user_defined_literals, "user_defined_literals",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_user_defined_literals,
                         "no_user_defined_literals", '\0', /*value=*/FALSE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_preserve_lvalues_with_same_type_casts,
                         "preserve_lvalues_with_same_type_casts", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_preserve_lvalues_with_same_type_casts,
                         "no_preserve_lvalues_with_same_type_casts", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_nonstd_anonymous_unions,
                         "nonstd_anonymous_unions", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_nonstd_anonymous_unions,
                         "no_nonstd_anonymous_unions", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_digit_separators, "digit_separators", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_digit_separators, "no_digit_separators",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_target, "target",
                         '\0', /*value=*/FALSE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_utf8_char_literals, "utf8_char_literals",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_utf8_char_literals, "no_utf8_char_literals",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_stricter_template_checking,
                         "stricter_template_checking",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if EXC_SPEC_IN_FUNC_TYPE_ENABLING_POSSIBLE
  add_option_description(optk_exc_spec_in_func_type,
                         "exc_spec_in_func_type",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_exc_spec_in_func_type,
                         "no_exc_spec_in_func_type",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* EXC_SPEC_IN_FUNC_TYPE_ENABLING_POSSIBLE */
  add_option_description(optk_aligned_new, "aligned_new", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_aligned_new, "no_aligned_new", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_ms_std_preproc,
                         "ms_std_preprocessor", '\0', /*value=*/TRUE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_ms_std_preproc,
                         "no_ms_std_preprocessor", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_char8_t, "char8_t", '\0', /*value=*/TRUE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_char8_t, "no_char8_t", '\0', /*value=*/FALSE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_bit_precise_integers, "bit_precise_integers",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_bit_precise_integers, "no_bit_precise_integers",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_relaxed_abstract_checking,
                         "relaxed_abstract_checking", '\0', /*value=*/TRUE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_relaxed_abstract_checking,
                         "no_relaxed_abstract_checking", '\0', /*value=*/FALSE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_concepts, "concepts", '\0', /*value=*/TRUE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_concepts, "no_concepts", '\0', /*value=*/FALSE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_colors, "colors", '\0', /*value=*/TRUE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_colors, "no_colors", '\0', /*value=*/FALSE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_keep_restrict_in_signatures,
                         "keep_restrict_in_signatures", '\0', /*value=*/TRUE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_keep_restrict_in_signatures,
                         "no_keep_restrict_in_signatures", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if UNICODE_VULNERABILITY_DETECTION_SUPPORTED
  add_option_description(optk_check_unicode_security,
                         "check_unicode_security", '\0', /*value=*/TRUE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_check_unicode_security,
                         "no_check_unicode_security", '\0', /*value=*/FALSE,
                         /*arg_required=*/FALSE, pchek_command_line);
#endif /* UNICODE_VULNERABILITY_DETECTION_SUPPORTED */
  add_option_description(optk_old_id_chars, "old_id_chars", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_old_id_chars, "no_old_id_chars", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_add_match_notes, "add_match_notes", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_add_match_notes, "no_add_match_notes", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_dump_command_options, "dump_command_options",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_output_mode, "output_mode", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
  add_option_description(optk_incognito, "incognito", '\0', /*value=*/TRUE,
                         /*arg_required=*/FALSE, pchek_command_line);
  add_option_description(optk_incognito, "no_incognito", '\0', /*value=*/FALSE,
                         /*arg_required=*/FALSE, pchek_command_line);
}  /* initialize_option_descriptions */


static void initialize_command_line_variables()
/*
Initialize any variables that are needed for processing the command line (and
therefore must be initialized before most initialization occurs).
*/
{
  mod_map = new_general<a_module_file_map>(/*mask_width=*/4u);
  lazy_mod_map_arr = new_general<a_lazy_module_file_arr>(/*mask_width=*/4u);
  header_unit_map = new_general<a_header_unit_map>(/*mask_width=*/4u);
  header_unit_quote_map = new_general<a_header_unit_map>(/*mask_width=*/4u);
  header_unit_angle_map = new_general<a_header_unit_map>(/*mask_width=*/4u);
}  /* initialize_command_line_variables */


static an_option_description_ptr look_up_option_description
					(char		*optchar,
					 a_boolean	is_keyword_option,
					 sizeof_t	keyword_length)
/*
Go through the linked list of option descriptions and look for one that
matches the specified option.  If is_keyword_option is TRUE then optchar
points to the keyword string, otherwise the character that optchar points
to is the option letter. 
*/
{
  an_option_description_ptr	odp = NULL;
  a_boolean			match = FALSE;
  a_boolean			ambiguous = FALSE;
  int				n;

  for (n = 0; n < option_descriptions_used; ++n) {
    odp = &option_descriptions[n];
    if (is_keyword_option) {
      match = odp->keyword != NULL &&
              keyword_length == odp->keyword_length &&
              strncmp(optchar, odp->keyword, size_t_arg(keyword_length)) == 0;
    } else {
      match = odp->letter != '\0' && *optchar == odp->letter;
    }  /* if */
    if (match) break;
  }  /* for */
  /* If no match was found for a keyword option, look again to see if
     the option is an abbreviation. */
  if (!match && is_keyword_option) {
    an_option_description_ptr	odp_found = NULL;
    for (n = 0; n < option_descriptions_used; ++n) {
      odp = &option_descriptions[n];
      if (odp->keyword != NULL &&
          strncmp(optchar, odp->keyword, size_t_arg(keyword_length)) == 0) {
        if (match) ambiguous = TRUE;
        odp_found = odp;
        match = TRUE;
      }  /* if */
    }  /* for */
    if (ambiguous) {
      /* If the command line option was ambiguous, issue an error and list
         the possible options. */
      a_diagnostic_ptr dp;
      dp = start_command_line_error(ec_cl_ambiguous_option, optchar);
      for (n = 0; n < option_descriptions_used; ++n) {
        odp = &option_descriptions[n];
        if (odp->keyword != NULL &&
            strncmp(optchar, odp->keyword, size_t_arg(keyword_length)) == 0) {
          str_add_diag_info(dp, ec_cl_ambiguous_fill_in, odp->keyword);
        }  /* if */
      }  /* for */
      /* Note that this routine does not return. */
      end_command_line_error(dp);
    }  /* if */
    odp = odp_found;
  }  /* if */
  check_assertion(!ambiguous);
  if (!match) odp = NULL;
  /* Record the fact that this option kind has been used. */
  if (odp != NULL) option_kind_used[(int)odp->kind] = TRUE;
  return odp;
}  /* look_up_option_description */


STATIC_THREAD a_const_char
		*opt_arg;
			/* Returned from get_option -- Pointer to the current
			   option argument. */
STATIC_THREAD int
		opt_ind;
			/* Index of the current option in argv. */

NORETURN static void invalid_argument_error(int  argc,
                                            char **argv)
/*
Issue a invalid command line argument diagnostic.
*/
{
  /* Reset opt_ind in the case of an option requiring
     an argument where the argument is missing. */
  if (opt_ind >= argc) opt_ind = argc-1;
  opt_arg = argv[opt_ind];
  /* This call terminates the program. */
  str_command_line_error(ec_cl_invalid_option, opt_arg);
}  /* invalid_argument_error */


STATIC_THREAD char
		*optchar;
				/* The character position containing the
				   next option letter to be examined, or NULL
				   if a new argument should be begun. */


static an_option_description_ptr get_option(int     argc,
                                            char    **argv)
/*
Fetch a command-line option.  argc and argv are the count of
command-line arguments and the array containing the command-line
argument strings.  This routine accepts both single character options
and keyword options.  One option is returned on each call.  The option
is looked up in the options_descriptions list and a pointer to the
option description structure is returned to the caller.  A NULL
pointer is returned when the end of the option list is found.  opt_ind
at that point indicates the argv index of the non-option argument.  If
an invalid option is used, a command line error will be issued (and
the compilation will be terminated).

The following option formats are supported:

	-abcxxx		-- turns on the "a" and "b" options and supplies the
			   argument "xxx" to the "c" option.

	-abc xxx	-- same as above

	--option_a	-- keyword option with no argument

	--option_b xxx	-- keyword option with an argument

	--option_b=xxx	-- keyword option with an argument (note that no
			   spaces are allowed on either side of the
			   equals sign).
*/
{
  a_boolean			is_keyword_option = FALSE;
  sizeof_t			keyword_length = 0;
  an_option_description_ptr	odp = NULL;
  char				*after_keyword = NULL;

  /* See if a new argument must be begun (i.e., there is not
     part of an existing option to finish). */
  while (optchar == NULL || *optchar == '\0') {
    /* Here, optchar points to the next option character.  See if the
       current option letter list has been exhausted. */
    if (optchar != NULL && *optchar == '\0') {
      /* Start the next argument. */
      opt_ind++;
    }  /* if */
    if (opt_ind >= argc) {
      /* No more arguments. */
      goto end_of_routine;
    } else {
      optchar = argv[opt_ind];
      if (*optchar != '-') {
        /* The argument string does not begin with a "-". */
        goto end_of_routine;
      } else if (*(optchar+1) == '-') {
        /* Either the beginning of a keyword option, or "--", which marks the
           end of the options. */
        if (*(optchar+2) == '\0') {
          /* The argument is "--", which marks the end of the options.
             Swallow this argument. */
          opt_ind++;
          goto end_of_routine;
        } else {
          /* The beginning of a keyword option (e.g., --exceptions).
             Update optchar to point to the keyword after the "--". */
          is_keyword_option = TRUE;
          optchar += 2;
          /* Find the end of the keyword.  The keyword ends either at
             the end of the current command line argument or when an
             equals sign is found.  The equals sign separates the
             keyword from its argument. */
          after_keyword = optchar;
          while (*after_keyword != '\0' && *after_keyword != '=') {
            after_keyword++;
          }  /* while */
          keyword_length = (sizeof_t)(after_keyword - optchar);
        }  /* if */
      } else if (*(optchar+1) == '\0') {
        /* The argument is "-", which is used to indicate stdin as a
           file name.  Return without swallowing this argument. */
        goto end_of_routine;
      } else {
        /* We have the start of a new option.  Advance past the "-". */
        optchar++;
      }  /* if */
    }  /* if */
  }  /* while */
  /* See if the option letter or keyword is valid. */
  odp = look_up_option_description(optchar, is_keyword_option,
                                   keyword_length);
  /* See if the option letter appears in the string of legal options. */
  if (odp == NULL || !odp->enabled) invalid_argument_error(argc, argv);
  /* See if the option takes an argument. */
  if (odp->arg_required) {
    if (is_keyword_option) {
      /* Keyword options may be specified as either:

		--keyword=argument        (with no spaces)
         or     --keyword argument
      */
      if (*after_keyword == '=') {
        /* Set the option pointer to the character after the "=". */
        opt_arg = after_keyword + 1;
        if (*opt_arg == '\0') invalid_argument_error(argc, argv);
      } else {
        /* Use the next argument as the option value, as in "--output xxx". */
        opt_ind++;
        /* If there are no more arguments, the option is missing. */
        if (opt_ind >= argc)  {
          str_command_line_error(ec_cl_missing_argument, argv[opt_ind-1]);
        }  /* if */
        opt_arg = argv[opt_ind];
      }  /* if */
    } else {
      /* Not a keyword option, the argument may immediately following the
         letter or may be in the next argv element. */
      if (*(optchar+1) == '\0') {
        /* The option letter is the last thing in the argument, so use the
           next argument as the option value, as in "-I xxx". */
        opt_ind++;
        /* If there are no more arguments, the option is missing. */
        if (opt_ind >= argc) invalid_argument_error(argc, argv);
        opt_arg = argv[opt_ind];
      } else {
        /* The option argument is the remainder of the current argument,
           as in "-Ixxx". */
        opt_arg = optchar+1;
      }  /* if */
    }  /* if */
    /* In any case, take no more characters of the current argument. */
    optchar = NULL;
    opt_ind++;
  } else {
    /* The option does not take an argument. */
    if (after_keyword != NULL && *after_keyword != '\0') {
      /* User mistakenly gave an argument to a option that doesn't take one. */
      invalid_argument_error(argc, argv);
    }  /* if */
    opt_arg = NULL;
    if (is_keyword_option) {
      /* Skip to the next element of argv. */
      optchar = NULL;
      opt_ind++;
    } else {
      /* Skip to the next character of the current element of argv. */
      optchar++;
    }  /* if */
  }  /* if */
end_of_routine:
  return odp;
}  /* get_option */


void add_to_def_undef_list(a_const_char           *str,
                           a_def_undef_string_ptr *du_list,
                           a_def_undef_string_ptr *du_list_end,
                           a_boolean              is_undef)
/*
Add the string pointed to by str (which comes from a command-line -D
or -U macro define/undefine option) to the list of def/undef strings
pointed to by *du_list.  The end of the list is pointed to by
*du_list_end.  The new entry is added to the end of the list.
*/
{
  a_def_undef_string_ptr du_new;

  /* alloc_general is called (rather than alloc_fe) because the general
     mem_manage.c routines are not yet initialized, and because the
     strings may be used several times if several files are compiled. */
  du_new = (a_def_undef_string_ptr)alloc_general(sizeof(a_def_undef_string));
  du_new->next = NULL;
  du_new->text = str;
  du_new->is_undef = is_undef;
  /* Add the new entry to the end of the list of defs or undefs. */
  if (*du_list_end != NULL) (*du_list_end)->next = du_new;
  *du_list_end = du_new;
  if (*du_list == NULL) *du_list = du_new;
}  /* add_to_def_undef_list */


template<typename an_Integral_type>
static void scan_opt_arg_number(an_Integral_type *result,
                                a_const_char     *optstr)
/*
Scan an argument option as a decimal number, check for overflow, and if no
overflow occurs set *result to the value.
*/
{
  uint64_t curr_total = 0;
  uint64_t max_value = (uint64_t)max_integral_value<an_Integral_type>();

  for (a_const_char *arg_ptr = optstr; *arg_ptr != '\0'; arg_ptr++) {
    if (!isdigit((unsigned char)*arg_ptr)) {
      goto number_error;
    }  /* if */
    if (!checked_multiplication(&curr_total, curr_total, 10)) {
      goto number_error;
    }  /* if */

    uint64_t digit = (uint64_t)(*arg_ptr - '0');
    if (!checked_addition(&curr_total, curr_total, digit)) {
      goto number_error;
    }  /* if */
  }  /* for */
  if (curr_total > max_value) {
    goto number_error;
  }  /* if */
  goto return_point;
number_error:
  str_command_line_error(ec_cl_invalid_number, optstr);
return_point:
  *result = (an_Integral_type)curr_total;
}  /* scan_opt_arg_number */

#if USE_FIXED_ADDRESS_FOR_MMAP

static char *scan_address(a_const_char *optstr)
/*
Scan an argument option as an address (either decimal or hexadecimal),
and return its value.
*/
{
  a_const_char *arg_ptr = optstr;
  size_t       result = 0;
  unsigned     digit, base;

  if (*arg_ptr == '0' &&
      (arg_ptr[1] == 'x' || arg_ptr[1] == 'X')) {
    base = 16;
    arg_ptr += 2;
  } else {
    base = 10;
  }  /* if */
  for (; *arg_ptr != '\0'; arg_ptr++) {
    if (isdigit((unsigned char)*arg_ptr)) {
      digit = (unsigned)(*arg_ptr - '0');
    } else {
      if (base == 10) goto number_error;
      if (*arg_ptr >= 'a' && *arg_ptr <= 'f') {
        digit = (unsigned)(*arg_ptr - 'a' + 0xA);
      } else if (*arg_ptr >= 'A' && *arg_ptr <= 'F') {
        digit = (unsigned)(*arg_ptr - 'A' + 0xA);
      } else {
        goto number_error;
      }  /* if */
    }  /* if */
    if (result > (size_t)(-1) / (size_t)base) goto number_error;
    result *= base;
    if (result > (size_t)(-1) - digit) goto number_error;
    result += digit;
  }  /* for */
  goto return_point;
number_error:
  str_command_line_error(ec_cl_invalid_number, optstr);
return_point:
  return (char *)result;
}  /* scan_address */

#endif /* USE_FIXED_ADDRESS_FOR_MMAP */

static a_const_char *file_name_from_opt_arg(a_const_char *optstr)
/*
The command-line argument given by optstr is a file name or directory
name.  Return the string to be used for the file name, translated if
necessary for character set issues.  The original pointer is returned
if no change is needed, and that's allocated wherever
command-line arguments are allocated.  If this routine allocates
a new string, it does so in general memory, in a unique allocation
for this call, so the string will be available throughout this
compilation.
*/
{
  a_const_char *file_name = file_name_in_internal_encoding(optstr);
  return file_name;
}  /* file_name_from_opt_arg */


static void split_opt_arg_on_char(a_const_char *optstr,
                                  a_const_char **str1,
                                  a_const_char **str2,
                                  char         split_char,
                                  a_boolean    respect_quotes)
/*
The command-line argument given by optstr is really a pair of arguments
separated by split_char.  Find the first instance of split_char in optstr and
divide the argument in two, with the first part returned in *str1 and the
second part returned in *str2.  If respect_quotes is TRUE, all characters
inside a quote pair are considered part of the string and cannot match
split_char.  If there is no match to split_char, set *str2 to NULL and return
the entirety of optstr in *str1.
*/
{
  sizeof_t optlen = strlen(optstr);
  char     cur_quote = 0, cur_char;

  *str2 = NULL;
  *str1 = optstr;
  for (sizeof_t idx = 0; idx < optlen; ++idx) {
    cur_char = optstr[idx];
    if (respect_quotes) {
      if (cur_char == '\\') {
        /* Some character is escaped.  Skip the next character. */
        ++idx;/*lint !e850*/
        continue;
      }  /* if */
      if (cur_char == cur_quote) {
        /* End of this quote pairing. */
        cur_quote = 0;
        continue;
      }  /* if */
      if (cur_quote == 0 && (cur_char == '"' || cur_char == '\'')) {
        /* Start of a quote pairing. */
        cur_quote = cur_char;
        continue;
      }  /* if */
    }  /* if */
    if (cur_char == split_char) {
      /* We've found our split point. */
      char* new_str = alloc_general(idx + 1);
      strncpy(new_str, optstr, idx);
      new_str[idx] = '\0';
      *str1 = new_str;
      *str2 = &optstr[idx + 1];
      break;
    }  /* if */
  }  /* for */
  /* At this point either split_char was encountered in which case *str1 and
     *str2 are set appropriately, or it wasn't encountered and they retain
     their original settings.  It's entirely possible that an unmatched quote
     was encountered - allow the consumer of the resulting strings to issue any
     errors on that, as appropriate. */
}  /* split_opt_arg_on_char */


static void process_diag_override_option(an_option_kind kind,
					 a_const_char	*arg)
/*
Go through a comma-separated list of error tags and call an error
processing routine to update the severity.
*/
{
  char			*local_arg;
  int			number_of_arguments = 0;
  int			i;
  an_error_severity	severity = es_none;
  char			*ptr;

  /* Make a local copy of the option string.  Remove any blanks and replace
     commas with null characters.  Note that this copy is simply discarded
     after it is used. */
  local_arg = (char *)alloc_general((sizeof_t)(strlen(arg) + 1));
  {
    a_const_char *src = arg;
    char         *dest = local_arg;
    char         ch;
    do {
      ch = *src;
      /* Remove blanks. */
      if (ch == ' ') continue;
      /* Replace commas with null characters. */
      if (ch == ',') ch = '\0';
      /* Keep track of the number of arguments found. */
      if (ch == '\0') number_of_arguments++;
      *dest++ = ch;
    } while (*src++ != '\0');
  }
  /* Convert the option kind into an error severity. */
  switch (kind) {
    case optk_diag_suppress: severity = es_none;                break;
    case optk_diag_remark:   severity = es_remark;              break;
    case optk_diag_warning:  severity = es_warning;             break;
    case optk_diag_error:    severity = es_discretionary_error; break;
    case optk_diag_once:     severity = es_once;                break;
    default: unexpected_condition();
  }  /* switch */
  /* Loop through the arguments and call a routine to update the
     error severity for the specified tag. */
  ptr = local_arg;
  for (i = 0; i < number_of_arguments; ++i) {
    char	*opt_start = ptr;
    char	*opt_end = strchr(ptr, '\0');
    a_boolean	err;
#if DEBUG
    if (debug_level >= 4) {
      fprintf(f_debug, "Setting error severity for: %s\n", opt_start);
    }  /* if */
#endif /* DEBUG */
    if (isdigit((unsigned char)*opt_start)) {
      int error_number;

      scan_opt_arg_number(&error_number, opt_start);
      err = set_severity_for_error_number(error_number, severity,
                                          /*make_default=*/TRUE);
      if (err) {
        str_command_line_error(ec_cl_invalid_error_number, opt_start);
      }  /* if */
    } else {
      err = set_severity_for_error_tag(opt_start, severity,
                                       /*make_default=*/TRUE);
      if (err) {
        str_command_line_error(ec_cl_invalid_error_tag, opt_start);
      }  /* if */
    }  /* if */
    ptr = opt_end + 1;
  }  /* for */
}  /* process_diag_override_option */


static void process_constexpr_diag_override_option(an_option_kind  kind,
                                                   a_const_char    *arg)
/*
Process one of the --constexpr_diag_* options, indicated by kind, whose
argument arg is a comma-separated list of the tags whose diagnostics are to
get the severity that kind requests.  Record the tags for
severity_for_constexpr_diag_tag, complaining about any of them that no call
of __builtin_constexpr_diag could supply.
*/
{
  an_error_severity  severity = es_none;
  char               *copy, *ptr;

  switch (kind) {
    case optk_constexpr_diag_suppress: severity = es_none;    break;
    case optk_constexpr_diag_remark:   severity = es_remark;  break;
    case optk_constexpr_diag_warning:  severity = es_warning; break;
    case optk_constexpr_diag_error:    severity = es_error;   break;
    default: unexpected_condition();
  }  /* switch */
  /* Make a copy of the option argument in which each comma is replaced by a
     null character, so that every tag becomes a string of its own.  The
     entries recorded below point into this copy, which is therefore kept. */
  copy = (char *)alloc_general((sizeof_t)(strlen(arg) + 1));
  (void)strcpy(copy, arg);
  ptr = copy;
  while (ptr != NULL) {
    char                      *comma = strchr(ptr, ',');
    a_constexpr_diag_tag_ptr  cdtp;

    if (comma != NULL) *comma++ = '\0';
    if (*ptr == '\0' ||
        !constexpr_diag_tag_is_valid(ptr, (a_targ_size_t)strlen(ptr))) {
      /* Note that this routine does not return. */
      str_command_line_error(ec_cl_invalid_constexpr_diag_tag, ptr);
    }  /* if */
    cdtp = alloc_general_of_type(a_constexpr_diag_tag);
    cdtp->next = NULL;
    cdtp->tag = ptr;
    cdtp->severity = severity;
    /* Keep the entries in command-line order, which is the order in which
       severity_for_constexpr_diag_tag expects to find them. */
    if (constexpr_diag_tags == NULL) {
      constexpr_diag_tags = cdtp;
    } else {
      last_constexpr_diag_tag->next = cdtp;
    }  /* if */
    last_constexpr_diag_tag = cdtp;
    ptr = comma;
  }  /* while */
}  /* process_constexpr_diag_override_option */


an_error_severity severity_for_constexpr_diag_tag(
                                              a_const_char       *tag,
                                              a_targ_size_t      tag_len,
                                              an_error_severity  severity)
/*
tag, whose length is tag_len, is the tag supplied by a call of
__builtin_constexpr_diag that asks for a diagnostic of the given severity.
Return the severity that should actually be used, which is es_none when the
diagnostic is to be suppressed.  A --constexpr_diag_* option naming the tag
decides it; when several do, the last one on the command line wins.  The
severity that was asked for is returned when no option applies.
*/
{
  an_error_severity         result = severity;
  a_constexpr_diag_tag_ptr  cdtp;

  for (cdtp = constexpr_diag_tags; cdtp != NULL; cdtp = cdtp->next) {
    if (strncmp(cdtp->tag, tag, size_t_arg(tag_len)) == 0 &&
        cdtp->tag[tag_len] == '\0') {
      result = cdtp->severity;
    }  /* if */
  }  /* for */
  return result;
}  /* severity_for_constexpr_diag_tag */


static void process_preinclude_option(an_option_kind	kind,
				      a_const_char	*arg)
/*
Process a preinclude or preinclude_macros option (determined by
"kind").  "arg" is the file name.
*/
{
  a_preinclude_file_ptr	pfp;

  pfp = alloc_preinclude_file();
  pfp->file_name = arg;
  /* Add this entry to the list of preinclude files. */
  if (kind == optk_preinclude_macros) {
    if (macro_preinclude_file_list == NULL) {
      macro_preinclude_file_list = pfp;
    } else {
      macro_preinclude_file_tail->next = pfp;
    }  /* if */
    macro_preinclude_file_tail = pfp;
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (kind == optk_preusing) {
    if (preusing_file_list == NULL) {
      preusing_file_list = pfp;
    } else {
      preusing_file_tail->next = pfp;
    }  /* if */
    preusing_file_tail = pfp;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else {
    if (preinclude_file_list == NULL) {
      preinclude_file_list = pfp;
    } else {
      preinclude_file_tail->next = pfp;
    }  /* if */
    preinclude_file_tail = pfp;
  }  /* if */
}  /* process_preinclude_option */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void add_vccorlib_preinclude(void)
/*
Add a preinclude entry for vccorlib.h at the front of the preinclude list.
(Used in C++/CX mode.)
*/
{
  a_preinclude_file_ptr	pfp = alloc_preinclude_file();

  pfp->file_name = "vccorlib.h";
  pfp->next = preinclude_file_list;
  preinclude_file_list = pfp;
  if (preinclude_file_tail == NULL) preinclude_file_list = pfp;
}  /* add_vccorlib_preinclude */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Structure used for an array of flag names that can be set using a
command-line option.
*/
typedef struct a_flag_name *a_flag_name_ptr;
typedef struct a_flag_name {
  a_const_char	*name;
			/* Name used to set the flag. */
  a_boolean	*variable;
			/* Pointer to the variable to be set. */
} a_flag_name;


/*
Array of flag names that may be set on the command-line.
*/
STATIC_THREAD a_flag_name
		flag_names[] = {
  { "suppress_inline_corresp_check", &suppress_inline_corresp_check },
  { "allow_anon_types_in_anon_unions", &allow_anon_types_in_anon_unions },
#if EXPENSIVE_CHECKING
  { "eager_load_modules", &eager_load_modules },
#endif /* EXPENSIVE_CHECKING */
#if IA64_ABI
  { "emulate_gnu_abi_bugs", &emulate_gnu_abi_bugs },
  { "emulate_unsafe_gnu_abi_bugs", &emulate_unsafe_gnu_abi_bugs },
  { "warn_about_tail_padding_use", &warn_about_tail_padding_use },
  { "reuse_tail_padding", &targ_reuse_tail_padding },
#endif /* IA64_ABI */
  { "packing_applies_to_base_classes", &packing_applies_to_base_classes },
  { "stack_referenced_include_directories",
    &stack_referenced_include_directories },
#if DEBUG
  { "space_used", &display_space_used },
#endif /* DEBUG */
  { "use_nonstd_partial_ordering", &use_nonstd_partial_ordering },
  { "no_checking_pragmas", &no_checking_pragmas },
  { "no_find_pragma_validation", &no_find_pragma_validation },
  { "warn_on_try_statement", &warn_on_try_statement },
#if WRITE_CPPCLI_PORTABLE_ASSEMBLIES
  { "generate_portable_assemblies", &generate_portable_assemblies },
#endif /* WRITE_CPPCLI_PORTABLE_ASSEMBLIES */
#if MICROSOFT_EXTENSIONS_ALLOWED
  { "disable_access_checking_in_microsoft_enum_bases",
    &disable_access_checking_in_microsoft_enum_bases },
  { "pending_generic_constraint_specifier_enabled",
    &pending_generic_constraint_specifier_enabled },
  { "generic_arity_overload_allowed",
    &generic_arity_overload_allowed },
  { "no_ms_nonreal_base_classes",
    &no_ms_nonreal_base_classes },
  { "force_ms_type_info_not_in_namespace_std",
    &force_ms_type_info_not_in_namespace_std },
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  { "terse_range_based_for_enabled", &terse_range_based_for_enabled },
  { "relaxed_constexpr", &relaxed_constexpr_enabled },
  { "constexpr_implies_const", &constexpr_implies_const },
  { "mangle_had_been_implicitly_const", &mangle_had_been_implicitly_const },
  { "coroutines", &coroutines_enabled },
  { "suppress_deferral_on_partial_spec_members",
    &suppress_deferral_on_partial_spec_members },
  { "no_very_expensive_checking", &no_very_expensive_checking },
  { "diag_override_does_not_affect_sfinae",
    &diag_override_does_not_affect_sfinae },
#if BUILTIN_FUNCTIONS_ENABLED
  { "preload_builtin_functions", &preload_builtin_functions },
#endif /* BUILTIN_FUNCTIONS_ENABLED */
  { "gen_edg_special_types", &gen_edg_special_types },
#if REFLECTION_ENABLING_POSSIBLE
  { "reflection", &reflection_enabled },
  { "injection", &injection_enabled },
#endif /* REFLECTION_ENABLING_POSSIBLE */
  { "alias_templ_intrinsics", &alias_templ_intrinsics_enabled },
  { "var_templ_intrinsics", &var_templ_intrinsics_enabled },
  { "templ_type_member_intrinsics", &templ_type_member_intrinsics_enabled },
  { "lazy_field_initializers", &always_delay_field_initializer_processing },
  { "core_constant_expr_is_noexcept", &core_constant_expr_is_noexcept },
  { "null_template_ptr_arg_enabled", &null_template_ptr_arg_enabled },
  { "skip_module_imports", &skip_module_imports },
  { "skip_module_version_check", &skip_module_version_check },
  { "ignore_absolute_paths_for_header_units",
    &ignore_absolute_paths_for_header_units },
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  { "skip_il_read", &skip_il_read },
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
  { "extended_float_types", &extended_float_types },
  { "use_predefined_macro_file", &use_predefined_macro_file },
  { NULL, NULL }  /* must be last */
};


static void set_flag_value(a_const_char	*flag_name,
			   a_boolean	value)
/*
Set or clear the flag specified by "flag_name".  "value" is the value to
be given to the flag.
*/
{
  a_flag_name_ptr	fnp;
  a_boolean		*flag_var = NULL;

  for (fnp = flag_names; fnp->name != NULL; fnp++) {
    if (strcmp(fnp->name, flag_name) == 0) {
      flag_var = fnp->variable;
    }  /* if */
  }  /* for */
  if (flag_var == NULL) {
    str_command_line_error(ec_cl_invalid_flag_name, flag_name);
  }  /* if */
  *flag_var = value;
}  /* set_flag_value */


static inline a_boolean c_mode_specified(void)
/*
Returns TRUE if an option designating a particular version of the C Standard
is specified.
*/
{
  a_boolean result = FALSE;

  if (option_kind_used[(int)optk_c89_mode] ||
      option_kind_used[(int)optk_c99_mode] ||
      option_kind_used[(int)optk_c11_mode] ||
      option_kind_used[(int)optk_c18_mode] ||
      option_kind_used[(int)optk_c23_mode]) {
    /* C mode was enabled by a command line option. */
    result = TRUE;
  }  /* if */
  return result;
}  /* c_mode_specified */


static inline a_boolean cpp_mode_specified(void)
/*
Returns TRUE if an option designating a particular version of the C++ Standard
is specified.
*/
{
  a_boolean result = FALSE;

  if (option_kind_used[(int)optk_cpp03_mode] ||
      option_kind_used[(int)optk_cpp11_mode] ||
      option_kind_used[(int)optk_cpp14_mode] ||
      option_kind_used[(int)optk_cpp17_mode] ||
      option_kind_used[(int)optk_cpp20_mode] ||
      option_kind_used[(int)optk_cpp23_mode] ||
      option_kind_used[(int)optk_cpp26_mode]) {
    /* C++ mode was enabled by a command line option. */
    result = TRUE;
  }  /* if */
  return result;
}  /* cpp_mode_specified */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void set_microsoft_mode_flags(void)
/*
Set other options whose values should be changed when Microsoft mode is enabled
or --ms_extensions or --ms_compatibility have been specified.  Only set the
option values if they were not already set by a command line option.
*/
{
  /* Microsoft's compilers use a "stack model" for include directories.
     Clang emulates that (but only when -fms-compatibility is specified, not
     -fms-extensions).  GNU doesn't emulate this in any mode. */
  stack_referenced_include_directories = (clang_mode ? ms_compat :
                                          gnu_mode ?   FALSE :
                                                       TRUE);
  allow_nonstandard_anonymous_unions = TRUE;
  if (!option_kind_used[(int)optk_allow_dollar_in_id_chars]) {
    allow_dollar_in_id_chars = TRUE;
  }  /* if */
  /* '//' is accepted as a comment delimiter in both C and C++. */
  end_of_line_comments_allowed = TRUE;
  IEEE_handling_on_float_operation_exceptions = FALSE;
  /* Floating-point template parameters are supported by MSVC++ through
     version 7.0.  These are also supported in later versions in some cases
     (see below for specifics).  The flag has already been set when --c++20
     is specified. */
  if (!cpp20_mode) {
    floating_point_template_parameters_allowed = microsoft_version <= 1300;
  }  /* if */
  equiv_typedefs_are_lookup_equivalent = FALSE;
  null_chars_allowed_in_source = TRUE;
  /* This will be enabled below in some cases.  Note: Microsoft does not
     support this in C++14 mode. */
  generalized_template_template_matching = cpp17_mode;
  if (!(option_kind_used[(int)optk_trigraphs])) {
    /* Early versions of Microsoft have trigraphs enabled by default. */
    trigraphs_allowed = microsoft_version < 1600;
  }  /* if */
  /* Set global variables that are independent of C or C++ mode. */
  if (microsoft_version >= 1900) {
    va_copy_macro_allowed = TRUE;
    alignof_enabled = TRUE;
    binary_literals_allowed = TRUE;
    digit_separators_enabled = TRUE;
    long_long_is_standard = TRUE;
    if (!option_kind_used[(int)optk_uliterals]) {
      uliterals_enabled = TRUE;
    }  /* if */
  }  /* if */
  if (microsoft_version >= 1925) {
    /* Visual Studio version 16.5. */
    pragma_operator_allowed = TRUE;
  }  /* if */
  if (C_mode()) {
    /* Microsoft C mode. */
    a_boolean ms_c11 = FALSE;
    a_boolean ms_c17 = FALSE;
    a_boolean ms_c23 = FALSE;
    /* Allow nonconstant expressions in aggregate initializers for automatic
       variables. */
    allow_nonconstant_auto_aggr_init_in_c_mode = TRUE;
    if (!option_kind_used[(int)optk_func_prototype_tags]) {
      /* Microsoft C never enters tag names in function prototype scopes. */
      func_prototype_tags_enabled = FALSE;
    }  /* if */
    if (option_kind_used[(int)optk_microsoft_c11]) {
      ms_c11 = TRUE;
    }  /* if */
    if (option_kind_used[(int)optk_microsoft_c17]) {
      ms_c17 = ms_c11 = TRUE;
    }  /* if */
    if (option_kind_used[(int)optk_microsoft_c23]) {
      ms_c23 = ms_c17 = ms_c11 = TRUE;
    }  /* if */
    if (ms_c23 && !option_kind_used[(int)optk_require_func_prototypes]) {
      /* MSVC does not currently require function prototypes in its C23
         mode. */
      require_func_prototypes = FALSE;
    }  /* if */
    if (ms_c11 && microsoft_version >= 1928 &&
        !option_kind_used[(int)optk_ms_std_preproc]) {
      /* MSVC version 1928 uses the conforming preprocessor for C++11 and
         higher. */
      ms_std_preproc = TRUE;
    }  /* if */
#if VLA_ALLOWED
    if (!(option_kind_used[(int)optk_vla]) && !clang_mode) {
      /* Support for VLAs is not yet enabled (but is when running in
         clang emulation mode with --ms_extensions). */
      vla_enabled = FALSE;
    }  /* if */
#endif /* VLA_ALLOWED */
    if (!gnu_mode) {
      /* GNU/Clang allow Atomic with -fms-extensions. */
      c11_atomic_enabled = FALSE;
    }  /* if */
    /* Recent Microsoft C compilers enable the C++ spelling of static_assert
       (i.e., not _Static_assert). */
    static_assert_enabled = microsoft_version >= 1600;
    if (microsoft_version >= 1800) {
      /* MSVC 12 (part of Visual Studio 2013) adds a number of C99-based
         features in its C mode. */
      if (!(option_kind_used[(int)optk_designators])) {
        designators_allowed = TRUE;
      }  /* if */
#if COMPOUND_LITERAL_ENABLING_POSSIBLE
      if (!(option_kind_used[(int)optk_compound_literals])) {
        compound_literals_allowed = TRUE;
      }  /* if */
#endif /* COMPOUND_LITERAL_ENABLING_POSSIBLE */
      c99_bool_is_keyword = TRUE;
      allow_decl_after_stmt = TRUE;
      if (microsoft_version >= 1912) {
        hex_floating_point_constants_allowed = TRUE;
      }  /* if */
    }  /* if */
    universal_character_names_allowed = TRUE;
    if (ms_c11 && microsoft_version >= 1927) {
      /* Visual Studio version 16.7. */
      /* Note that compound literals are already enabled above. */
      if (!(option_kind_used[(int)optk_restrict])) {
        restrict_keyword_enabled = TRUE;
      }  /* if */
      noreturn_keyword_enabled = TRUE;
      alignas_enabled = TRUE;
      alignof_enabled = TRUE;
    }  /* if */
    if (ms_c17 && microsoft_version >= 1928) {
      /* Visual Studio version 16.8. */
      /* Currently equivalent to --ms_c11. */
    }  /* if */
    if (ms_c23 && microsoft_version < 1940) {
      /* MSVC only began supporting #elifdef/#elifndef in version 1940.
         However, check_and_set_new_c_mode_options set the corresponding
         global flag to TRUE.  Clear it now. */
      elifdef_enabled = FALSE;
    }  /* if */
    if (microsoft_version >= 1934) {
      std_attributes_enabled = TRUE;
      nodiscard_attribute_enabled = TRUE;
      enumerator_attributes_enabled = TRUE;
    }  /* if */
  } else {
    /* Microsoft C++ mode. */
    a_boolean ms_cpp14_mode = !cpp_mode_specified() &&
                              microsoft_version >= 1903;
    a_boolean ms_cpp17_mode = FALSE;
    a_boolean ms_cpp20_mode = FALSE;
    a_boolean ms_cpp23_mode = FALSE;
    a_boolean ms_cpplatest_mode = FALSE;
    if (option_kind_used[(int)optk_microsoft_cpp14_mode]) {
      ms_cpp14_mode = TRUE;
      if (microsoft_version < 1903) {
        command_line_error(ec_microsoft_version_doesnt_support_cpp14_mode);
      }  /* if */
    }  /* if */
    if (option_kind_used[(int)optk_microsoft_cpp17_mode]) {
      ms_cpp14_mode = ms_cpp17_mode = TRUE;
      if (microsoft_version < 1911) {
        command_line_error(ec_microsoft_version_doesnt_support_cpp17_mode);
      }  /* if */
    }  /* if */
    if (option_kind_used[(int)optk_microsoft_cpp20_mode]) {
      ms_cpp14_mode = ms_cpp17_mode = ms_cpp20_mode = TRUE;
      if (microsoft_version < 1920) {
        command_line_error(ec_microsoft_version_doesnt_support_cpp20_mode);
      }  /* if */
    }  /* if */
    if (option_kind_used[(int)optk_microsoft_cpp23_mode]) {
      ms_cpp14_mode = ms_cpp17_mode = ms_cpp20_mode = ms_cpp23_mode = TRUE;
      if (microsoft_version < 1943) {
        command_line_error(ec_microsoft_version_doesnt_support_cpp23_mode);
      }  /* if */
    }  /* if */
    if (option_kind_used[(int)optk_microsoft_cpplatest_mode]) {
      ms_cpp14_mode = ms_cpp17_mode = ms_cpp20_mode = ms_cpp23_mode =
                                                      ms_cpplatest_mode = TRUE;
      if (microsoft_version < 1903) {
        command_line_error(ec_microsoft_version_doesnt_support_cpplatest_mode);
      }  /* if */
    }  /* if */
    if (microsoft_version >= 1928 && ms_cpp20_mode &&
        !option_kind_used[(int)optk_ms_permissive]) {
      /* Beginning with 16.8, /std:c++latest or /std:c++20 imply /permissive-
         but that can be overridden using the /permissive flag. */
        ms_permissive = FALSE;
    }  /* if */
    enum_types_can_be_smaller_than_int = FALSE;
    enum_types_can_be_larger_than_int = !ms_permissive &&
                                        microsoft_version >= 1934;
    if (force_ms_type_info_not_in_namespace_std) {
      type_info_in_namespace_std = FALSE;
    } else if (ms_compat) {
      type_info_in_namespace_std = MICROSOFT_MODE_TYPE_INFO_IN_NAMESPACE_STD;
    }  /* if */
    if (!type_info_in_namespace_std) {
      /* We will presumably want to pick ::type_info from the Microsoft
         headers.  In that case, we cannot expect an EDG-specific pragma. */
      pragma_define_type_info_is_required = FALSE;
    }  /* if */
    if (!option_kind_used[(int)optk_exception_handling]) {
      exceptions_enabled = TRUE;
    }  /* if */
    if (!option_kind_used[(int)optk_bool_is_keyword]) {
      /* The bool keyword is supported by Microsoft Visual C++ 5.0. */
      bool_is_keyword = microsoft_version >= 1100;
    }  /* if */
    if (!option_kind_used[(int)optk_wchar_t_is_keyword]) {
      wchar_t_is_keyword = microsoft_version >= 1400;
    }  /* if */
    if (!option_kind_used[(int)optk_explicit]) {
      /* The explicit keyword is supported by Microsoft Visual C++ 5.0. */
      explicit_keyword_enabled = microsoft_version >= 1100;
    }  /* if */
#if !RUNTIME_USES_TYPENAME
    if (!option_kind_used[(int)optk_typename]) {
      /* The typename keyword is supported by Microsoft Visual C++ 5.0. */
      typename_enabled = microsoft_version >= 1100;
    }  /* if */
#endif /* !RUNTIME_USES_TYPENAME */
    if (!option_kind_used[(int)optk_nonstandard_instantiation_lookup]) {
      /* If nonstandard instantiation lookup was not set on the command line,
         turn it off now. */
      nonstandard_instantiation_lookup_enabled = FALSE;
    }  /* if */
    if (!option_kind_used[(int)optk_guiding_decls]) {
      /* Guiding declarations are supported by MSVC++ through version 7.0. */
      guiding_decls_allowed = microsoft_version <= 1300;
    }  /* if */
    if (!option_kind_used[(int)optk_old_specializations]) {
      old_specializations_allowed = microsoft_version < 1310;
    }  /* if */
    c_and_cpp_function_types_are_distinct = FALSE;
    if (!option_kind_used[(int)optk_extern_inline]) {
      extern_inline_allowed = TRUE;
    }  /* if */
    if (!option_kind_used[(int)optk_base_assign_op_is_default]) {
      allow_copy_assignment_op_with_base_class_param = FALSE;
    }  /* if */
    if (!option_kind_used[(int)optk_old_for_init]) {
      /* MSVC++ 7.0 and earlier always use the old for-init scoping rule.
         MSVC++ 7.1 uses the new rule for for-init variables with nontrivial
         destructors and the old rule for other variables.  We emulate the
         MSVC++ 7.1 behavior using the standard scope stack setup.   MSVC++ 8
         implements the C++ standard rule. */
      if (microsoft_version < 1310) {
        /* MSVC++ 7.0 or earlier. */
        use_nonstandard_for_init_scope = TRUE;
      } else if (microsoft_version >= 1400) {
        /* MSVC++ 8 and later. */
        use_nonstandard_for_init_scope = FALSE;
      } else {
        /* MSVC++ 7.1. */
        use_nonstandard_for_init_scope = FALSE;
        microsoft_type_dependent_for_init_scope = TRUE;
      }  /* if */
    }  /* if */
    if (!option_kind_used[(int)optk_enum_overloading]) {
      /* Enum overloading is supported by Microsoft Visual C++ 4.x. */
      operator_overloading_on_enums_enabled = microsoft_version >= 1000;
    }  /* if */
    if (!option_kind_used[(int)optk_class_name_injection]) {
      class_name_injection_enabled = TRUE;
    }  /* if */
    if (!option_kind_used[(int)optk_arg_dependent_lookup]) {
      /* MSVC++ versions before 7.1 did not support argument-dependent
         lookup. */
      arg_dependent_lookup_enabled = (microsoft_version >= 1310);
    }  /* if */
    if (!option_kind_used[(int)optk_friend_injection]) {
      /* In non-permissive mode, do not inject friend function names. */
      if (ms_permissive) {
        friend_function_injection_enabled = TRUE;
      } else {
        friend_function_injection_enabled = FALSE;
      }  /* if */
      /* Friend class name injection appears to still be performed in all
         MSVC modes. */
      if (ms_compat) {
        friend_class_injection_enabled = TRUE;
      }  /* if */
    }  /* if */
    if (!option_kind_used[(int)optk_dependent_name_processing]) {
      /* In non-permissive mode, do normal dependent name processing. */
      if (ms_permissive) {
        do_dependent_name_processing = FALSE;
      } else {
        do_dependent_name_processing = TRUE;
      }  /* if */
    }  /* if */
    if (!option_kind_used[(int)optk_parse_nonclass_templates]) {
      /* Parse nonclass templates if dependent name processing is being
         done or C++26 is enabled.  Note that --no_ms_permissive will
         force dependent name processing to be done unless it is disabled
         explicitly by the --no_dep_name option. */
      nonclass_prototype_instantiations = do_dependent_name_processing ||
                                          cpp26_mode;
    }  /* if */
    if (!option_kind_used[(int)optk_implicit_typename]) {
      /* Implicit typename processing is not done unless dependent name
         processing was disabled. */
      implicit_typename_enabled = !do_dependent_name_processing;
    }  /* if */
    if (!microsoft_mode ||
        !(ms_permissive && !do_dependent_name_processing &&
         !nonclass_prototype_instantiations)) {
      /* Only do the special nonreal base class processing in permissive
         mode, if dependent name processing was not enabled, and if
         parsing nonclass templates is not enabled.  The microsoft_mode test
         is used to make sure we do not enable this processing in
         ms_extensions mode. */
      no_ms_nonreal_base_classes = TRUE;
    }  /* if */
    dependent_lookup_finds_static_functions = TRUE;
    if (!option_kind_used[(int)optk_nonstandard_using_decl]) {
      nonstandard_using_decl_allowed = FALSE;
    }  /* if */
    if (!option_kind_used[(int)optk_nonstandard_default_arg_deduction]) {
      /* Versions prior to 7.1 include the default arguments as part of the
         deduced function type. */
      nonstandard_default_arg_deduction = microsoft_version <= 1300;
    }  /* if */
    if (!option_kind_used[(int)optk_export_template]) {
      export_template_allowed = FALSE;
      export_keyword_enabled = FALSE;
    }  /* if */
    if (!option_kind_used[(int)optk_late_tiebreaker]) {
      do_late_ovl_res_tiebreaker = (microsoft_bugs &&
                                    microsoft_version <= 1300);
    }  /* if */
    if (!(option_kind_used[(int)optk_const_string_literals])) {
      /* String literals are const starting with version 7.1. */
      string_literals_are_const = microsoft_version >= 1310;
    }  /* if */
    if (!option_kind_used[(int)optk_deprecated_string_conv]) {
      /* A conversion from string literal to char is allowed in older
         versions of Visual Studio or in permissive mode. */
      deprecated_string_literal_conv_allowed = (microsoft_version < 1910 ||
                                                ms_permissive);
    }  /* if */
    single_ref_qual_ovl_res_tiebreaker = (microsoft_bugs &&
                                          microsoft_version < 1300);
    if (microsoft_version >= 1310) {
      late_template_ovl_res_tiebreaker = TRUE;
    } else if (microsoft_version == 1300) {
      late_template_ovl_res_tiebreaker = FALSE;
    }  /* if */
    if (!(option_kind_used[(int)optk_nonconst_ref_anachronism]) &&
        ms_permissive) {
      allow_nonconst_ref_anachronism = TRUE;
    }  /* if */
    allow_nonconst_call_anachronism = (microsoft_version < 1000);
    flexible_array_members_allowed = TRUE;
    /* Default arguments on template members are diagnosed with a warning
       (instead of the usual error) when scanned.  The actual default is
       ignored. */
    allow_default_arg_on_template_member_definition = FALSE;
    /* Make template parameters visible in specialization scopes. */
    use_microsoft_specialization_scope = microsoft_version < 1310;
    /* A friend class declaration finds names made visible by
       using-directives. */
    friend_class_decl_can_find_using_dir = TRUE;
    /* Extended friend class declaration syntax (standard in C++11) is
       accepted for all values of microsoft_version. */
    extended_friends_enabled = TRUE;
    extern_template_allowed = TRUE;
    /* Qualifying with an enum type is enabled in Microsoft C++ mode, but if
       microsoft_version < 1400, an error is issued if the enumeration is not
       a class member. */
    enum_qualifiers_enabled = TRUE;
    explicit_enum_base_enabled = (microsoft_version >= 1400) ||
                                 cppcli_enabled || cpp11_mode;
    /* Don't enable this in C++/CLI mode.  It causes issues with special
       identifiers such as safe_cast. */
    if (cppcli_enabled) adl_for_non_visible_templates = FALSE;
    if (microsoft_version >= 1700) {
      opaque_enum_decls_enabled = TRUE;
    }  /* if */
    if (microsoft_version >= 1400) {
      if (!option_kind_used[(int)optk_type_traits_helpers]) {
        type_traits_helpers_enabled = TRUE;
      }  /* if */
      /* MSVC++ 8 follows the C++11 rules for treating the single ">>" token as
         two ">" tokens in angle bracket contexts. */
      right_shift_can_be_angle_brackets = TRUE;
      local_types_as_template_args_enabled = TRUE;
      decls_using_types_without_linkage_allowed = TRUE;
      for_each_statement_enabled = (microsoft_version < 1910 || ms_permissive);
    }  /* if */
    if (microsoft_version >= 1600) {
      /* If the treatment of "auto" was not explicitly specified by the
         user, set it appropriately now. */
      if (!option_kind_used[(int)optk_auto_type] &&
          !option_kind_used[(int)optk_auto_storage]) {
        auto_type_specifier_enabled = TRUE;
        auto_storage_class_specifier_enabled = FALSE;
      }  /* if */
      decltype_enabled = TRUE;
      if (microsoft_version >= 1800) {
        /* Note that MSVC++ 12 accepts decltype as a base-specifier, but
           not as a mem-initializer, so this setting is more lenient. */
        enable_decltype_in_base_specifier_and_mem_initializer = TRUE;
      }  /* if */
      if (!option_kind_used[(int)optk_rvalue_ctor_is_copy_ctor]) {
        /* Microsoft MSVC++10 generates an implicit traditional copy
           constructor even when a move constructor was explicitly declared. */
        rvalue_ctor_is_copy_ctor = microsoft_version >= 1900;
      }  /* if */
      if (!option_kind_used[(int)optk_gen_move_operations]) {
        /* Early Microsoft compilers do not generate move constructors or
           move assign operations. */
        generate_move_operations = microsoft_version >= 1900;
      }  /* if */
      trailing_return_types_enabled = TRUE;
#if CPP11_IL_EXTENSIONS_SUPPORTED
      /* These options require back end support that may not be available. */
      static_assert_enabled = TRUE;
      if (!option_kind_used[(int)optk_lambdas]) {
        lambdas_enabled = TRUE;
        if (microsoft_version >= 1900) {
          lambda_default_args_enabled = TRUE;
          generic_lambdas_enabled = TRUE;
          generic_lambdas_can_implicitly_capture = TRUE;
          init_capture_enabled = TRUE;
        }  /* if */
      }  /* if */
      if (!option_kind_used[(int)optk_rvalue_references]) {
        rvalue_references_enabled = TRUE;
      }  /* if */
#endif /* CPP11_IL_EXTENSIONS_SUPPORTED */
    }  /* if */
#if CPP11_IL_EXTENSIONS_SUPPORTED
    if (!option_kind_used[(int)optk_nullptr]) {
      nullptr_enabled = (microsoft_version >= 1600 || cppcli_enabled);
    }  /* if */
#endif /* CPP11_IL_EXTENSIONS_SUPPORTED */
    if (!option_kind_used[(int)optk_cpp11_sfinae] &&
        !option_kind_used[(int)optk_cpp11_mode]) {
      cpp11_sfinae_enabled = (microsoft_version >= 1600);
    }  /* if */
    if (!option_kind_used[(int)optk_cpp11_sfinae_ignore_access]) {
      if (cpp11_sfinae_enabled && microsoft_mode && microsoft_version < 1910) {
        /* Older versions of MSVC appear not to implement access SFINAE yet.
           However, Clang with the -fms-extensions flag does; so don't change
           modes that are not actually full Microsoft modes. */
        cpp11_sfinae_ignore_access = TRUE;
      }  /* if */
    }  /* if */
    if (microsoft_version >= 1700 || cppcli_enabled) {
      range_based_for_enabled = TRUE;
    }  /* if */
    /* The Microsoft headers put va_list in the global namespace but create
       a using-declaration in namespace std.  Do the same thing unless
       the front end has been configured to put va_list in std. */
    if (!va_list_in_std_namespace) {
      va_list_using_using_decl_in_std_namespace = TRUE;
    }  /* if */
    if (cppcli_enabled || microsoft_version >= 1700) {
      explicit_conversion_functions_enabled = TRUE;
    }  /* if */
    if (microsoft_version >= 1700) {
      raw_string_literals_enabled = TRUE;
      list_init_enabled = TRUE;
      if (!option_kind_used[(int)optk_delegating_constructors]) {
        delegating_constructors_enabled = TRUE;
      }  /* if */
      if (!option_kind_used[(int)optk_variadic_templates]) {
        variadic_templates_enabled = TRUE;
#if FUNCTION_PROTOTYPE_INSTANTIATION_DEFERRAL_ALLOWED
        if (!option_kind_used[(int)optk_defer_parse_function_templates] &&
            !option_kind_used[(int)optk_parse_nonclass_templates]) {
          /* Defer function prototype instantiations except in modes where
             they should be done unconditionally. */
          if (microsoft_mode && !cppcli_enabled && ms_permissive &&
              !stricter_template_checking) {
            defer_function_prototype_instantiations = TRUE;
          }  /* if */
        }  /* if */
#endif /* FUNCTION_PROTOTYPE_INSTANTIATION_DEFERRAL_ALLOWED */
      }  /* if */
      this_in_trailing_return_types_enabled = TRUE;
    }  /* if */
    if (microsoft_version >= 1800) {
      deleted_functions_enabled = TRUE;
      defaulted_special_members_enabled = TRUE;
      field_initializers_enabled = TRUE;
      alias_declarations_enabled = TRUE;
    }  /* if */
    if (microsoft_version >= 1900) {
      if (!cpp_mode_specified()) {
        /* Most C++11 features are accepted by MSVS 2015, so implicitly enable
           all C++11 features and handle exceptions explicitly. */
        implicit_microsoft_cpp11_mode = TRUE;
      }  /* if */
      noexcept_enabled = TRUE;
      constexpr_enabled = TRUE;
      unrestricted_unions_enabled = TRUE;
      aggregate_classes_can_have_field_initializers = TRUE;
      inheriting_constructors_enabled = TRUE;
      ref_qualifiers_enabled = rvalue_references_enabled;
      alignas_enabled = TRUE;
      inline_namespaces_enabled = TRUE;
      if (!option_kind_used[(int)optk_user_defined_literals]) {
        user_defined_literals_enabled = TRUE;
      }  /* if */
      deduced_return_types_enabled = TRUE;
      if (auto_type_specifier_enabled) {
        decltype_auto_enabled = TRUE;
      }  /* if */
      if (!option_kind_used[(int)optk_thread_local_storage]) {
        std_thread_local_storage_specifier_enabled = TRUE;
      }  /* if */
      std_attributes_enabled = TRUE;
      namespace_attributes_enabled = TRUE;
      enumerator_attributes_enabled = TRUE;
      sized_deallocation_enabled = RUNTIME_SUPPORTS_SIZED_DEALLOCATION;
      mixed_string_concat_enabled = TRUE;
      std_override_modifiers_enabled = TRUE;
      selection_from_prvalue_is_xvalue = TRUE;
      if (!coroutines_enabled) {
        /* This may have been enabled already via --set_flag; we don't want to
           override that. */
        if (ms_cpp20_mode ||
            option_kind_used[(int)optk_microsoft_await] ||
            option_kind_used[(int)optk_microsoft_await_strict]) {
          /* Enabled with /std:c++latest or /await or /await:strict. */
          coroutines_enabled = COROUTINE_ENABLING_POSSIBLE;
        }  /* if */
      }  /* if */
      if (!option_kind_used[(int)optk_utf8_char_literals]) {
        utf8_char_literals_enabled = TRUE;
      }  /* if */
      if (microsoft_version >= 1901) {
        /* Emulate Visual Studio 2015 Update 1. */
        if (!option_kind_used[(int)optk_microsoft_build_number]) {
          microsoft_build_number = 23506;
        }  /* if */
      }  /* if */
      if (microsoft_version >= 1902) {
        /* Emulate Visual Studio 2015 Update 2. */
        if (!option_kind_used[(int)optk_microsoft_build_number]) {
          microsoft_build_number = 23918;
        }  /* if */
        variable_templates_enabled = TRUE;
      }  /* if */
      if (microsoft_version >= 1903) {
        /* Emulate Visual Studio 2015 Update 3. */
        /* With the advent of Update 3, Visual Studio supports the /std:c++14
           and /std:c++latest command-line options (which are emulated by
           the --ms_c++14 and --ms_c++latest front end options, respectively).
           Note that /std:c++14 is the default when microsoft_version is
           1903. */
        if (ms_cpp14_mode) {
          msvc_lang = "201402L";
          /* Enable emulation of Visual Studio's /std:c++14 mode.  Note that
             internally most C++14 features are enabled via global variables,
             but for those that aren't, set std_version to the value for
             C++14. */
          std_version = 201402;
          relaxed_range_based_for_enabled = TRUE;
        }  /* if */
        if (ms_cpp17_mode) {
          std_version = 201403;
          msvc_lang = "201403L";
          nested_namespace_definitions_enabled = TRUE;
        }  /* if */
      }  /* if */
      if (microsoft_version >= 1910) {
        /* Emulate Visual Studio "15". */
        relaxed_constexpr_enabled = TRUE;
        constexpr_implies_const = FALSE;
        if (!(option_kind_used[(int)optk_alternative_tokens])) {
          alternative_tokens_allowed = !ms_permissive;
        }  /* if */
        if (ms_cpp17_mode) {
          terse_static_assert_enabled = TRUE;
        }  /* if */
        nodiscard_attribute_enabled = TRUE;
      }  /* if */
      if (microsoft_version >= 1911) {
        /* Visual Studio 2017 version 15.3. */
        if (ms_cpp14_mode) {
          /* Enable "if constexpr" (used in Microsoft system header files). */
          constexpr_if_enabled = TRUE;
        }  /* if */
        if (ms_cpp17_mode) {
          /* Microsoft is now (generally) enabling C++17 features only
             when one of /std:c++17 or /std:c++latest is specified. */
          register_is_deprecated = TRUE;
          register_is_disallowed = FALSE;
          using_attribute_namespaces_enabled = TRUE;
          operator_bool_increment_allowed = FALSE;
          struct_bindings_enabled = TRUE;
          selection_initializers_enabled = TRUE;
          direct_init_fixed_base_enum_enabled = TRUE;
          constexpr_if_enabled = TRUE;
          constexpr_lambdas_enabled = TRUE;
          /* Note that msvc_lang has already been set above for the
             ms_cpp14_mode case (to "201402L"). */
          if (ms_cpp17_mode) {
            /* Enable emulation of Visual Studio's /std:c++17 mode.  Note that
               internally most C++17 features are enabled via global
               variables, but for those that aren't, set std_version to the
               value for C++17. */
            std_version = 201703;
            msvc_lang = "201703L";
          }  /* if */
          if (ms_cpplatest_mode) {
            std_version = 201704;
            msvc_lang = "201704L";
          }  /* if */
        }  /* if */
      }  /* if */
      if (microsoft_version >= 1912) {
        /* Visual Studio 2017 version 15.5. */
        if (ms_cpp17_mode) {
          capture_star_this_enabled = TRUE;
          inline_variables_allowed = TRUE;
          fold_expressions_enabled = TRUE;
#if EXC_SPEC_IN_FUNC_TYPE_ENABLING_POSSIBLE
          if (!option_kind_used[(int)optk_exc_spec_in_func_type]) {
            exc_spec_in_func_type = TRUE;
          }  /* if */
#endif /* EXC_SPEC_IN_FUNC_TYPE_ENABLING_POSSIBLE */
          deduction_from_exc_spec_allowed = microsoft_version >= 1951;
          if (!option_kind_used[(int)optk_aligned_new]) {
            overaligned_allocation_enabled = TRUE;
          }  /* if */
          generalized_nontype_arguments = TRUE;
          hex_floating_point_constants_allowed = TRUE;
          generalized_template_template_matching = TRUE;
          nested_namespace_definitions_enabled = TRUE;
        }  /* if */
      }  /* if */
      if (microsoft_version >= 1913) {
        /* Visual Studio 2017 version 15.6. */
        hex_floating_point_constants_allowed = TRUE;
        if (ms_cpp17_mode) {
          mandatory_copy_elision = TRUE;
          class_template_arg_deduction_enabled = TRUE;
        }  /* if */
      }  /* if */
      if (microsoft_version >= 1914) {
        /* Visual Studio 2017 version 15.7. */
        if (ms_cpp17_mode) {
          variadic_using_decls_enabled = TRUE;
          strict_cpp17_eval_order = TRUE;
          aggregate_classes_can_have_bases = TRUE;
          auto_template_params_enabled = TRUE;
        }  /* if */
      }  /* if */
      if (microsoft_version >= 1920) {
        /* Visual Studio 2019 version 16.0. */
        if (ms_cpp20_mode) {
          lambda_allowed_in_uneval_context = TRUE;
          cpp20_designators_restriction = TRUE;
          aggregate_classes_can_have_user_ctors = FALSE;
          rvalue_allowed_with_const_qual_memptr = TRUE;
          /* Enable emulation of Visual Studio's /std:c++20 and /std:c++latest
             command-line options.  Note that internally most C++20 features
             are enabled via global variables, but for those that aren't, set
             std_version to the value for C++20. */
          std_version = 202002;
          if (ms_cpplatest_mode) {
            msvc_lang = "201705L";
          } else {
            msvc_lang = "202002L";
          }  /* if */
        }  /* if */
      }  /* if */
      if (microsoft_version >= 1921) {
        /* Visual Studio 2019 version 16.1. */
        if (ms_cpp20_mode) {
          adl_for_non_visible_templates = TRUE;
          designators_allowed = TRUE;
          cpp20_designators_restriction = TRUE;
          spaceship_enabled = TRUE;
        }  /* if */
      }  /* if */
      if (microsoft_version >= 1922) {
        /* Visual Studio 2019 version 16.2. */
        if (ms_cpp14_mode) {
          conditional_explicit_enabled = TRUE;
        }  /* if */
        if (ms_cpp20_mode) {
          explicit_copy_this_capture_enabled = TRUE;
          lambda_template_param_list_enabled = TRUE;
          if (!option_kind_used[(int)optk_char8_t]) {
            char8_t_enabled = TRUE;
          }  /* if */
          pack_init_capture_enabled = TRUE;
        }  /* if */
      }  /* if */
      if (microsoft_version >= 1923) {
        /* Visual Studio 2019 version 16.3. */
        if (ms_cpp20_mode) {
          relaxed_typename_enabled = TRUE;
          nested_inline_namespace_definitions_enabled = TRUE;
          concepts_enabled = TRUE;
          abbr_func_templates_enabled = TRUE;
        }  /* if */
      }  /* if */
      if (microsoft_version >= 1924) {
        /* Visual Studio 2019 version 16.4. */
        if (ms_cpp20_mode) {
          using_enum_enabled = TRUE;
        }  /* if */
        if (!ms_permissive) {
          long_long_promotion_allowed = TRUE;
        }  /* if */
      }  /* if */
      if (microsoft_version >= 1925) {
        /* Visual Studio 2019 version 16.5. */
        if (ms_cpp20_mode) {
          init_statement_allowed_in_range_based_for = TRUE;
          constexpr_try_enabled = TRUE;
        }  /* if */
      }  /* if */
      if (microsoft_version >= 1926) {
        /* Visual Studio 2019 version 16.6. */
        if (ms_cpp20_mode) {
          consteval_enabled = TRUE;
          constinit_enabled = TRUE;
          floating_point_template_parameters_allowed = TRUE;
        }  /* if */
      }  /* if */
      if (microsoft_version >= 1927) {
        /* Visual Studio version 16.7. */
        if (ms_cpp20_mode) {
          destroying_operator_delete_enabled = TRUE;
          aggregate_ctad_enabled = TRUE;
          alias_ctad_enabled = TRUE;
        }  /* if */
      }  /* if */
      if (microsoft_version >= 1928) {
        /* Visual Studio versions 16.8 and 16.9. */
        /* If necessary, the build number can be used to distinguish these
           (with 16.9 starting at build number 29500). */
        if (ms_cpp20_mode) {
          module_keywords_enabled = TRUE;
          if (!option_kind_used[(int)optk_modules]) {
            modules_enabled = TRUE;
          }  /* if */
          constexpr_dynamic_alloc_enabled = TRUE;
          constexpr_virtual_enabled = TRUE;
          allow_parenthesized_aggregate_init = TRUE;
        }  /* if */
      }  /* if */
      if (microsoft_version >= 1929) {
        /* Visual Studio versions 16.10 and 16.11. */
        if (ms_cpplatest_mode) {
          msvc_lang = "202004L";
        }  /* if */
      }  /* if */
      if (microsoft_version >= 1932) {
        if (ms_cpplatest_mode) {
          /* These C++23 features are enabled in this mode. */
          if_consteval_enabled = TRUE;
          explicit_this_param_enabled = TRUE;
        }  /* if */
      }  /* if */
      if (microsoft_version >= 1940 && ms_cpp23_mode) {
        elifdef_enabled = TRUE;
      }  /* if */
      if (microsoft_version >= 1942 && ms_cpp23_mode) {
        multi_subscript_enabled = TRUE;
      }  /* if */
      if (microsoft_version >= 1943) {
        if (ms_cpp23_mode) {
          /* These C++23 features are enabled in this mode. */
          if_consteval_enabled = TRUE;
          explicit_this_param_enabled = TRUE;
          size_suffix_enabled = TRUE;
          lambda_declarator_params_optional = TRUE;
          msvc_lang = "202302L";
        }  /* if */
        if (ms_cpplatest_mode) {
          msvc_lang = "202400L";
        }  /* if */
      }  /* if */
      if (microsoft_version >= 1944) {
        if (ms_cpp17_mode) {
          /* This C++23 feature is enabled in C++17 mode. */
          if (relaxed_range_based_for_enabled) {
            extended_range_based_for_lifetime = TRUE;
          }  /* if */
        }  /* if */
        if (ms_cpp23_mode) {
          /* These C++23 features are enabled in this mode. */
          lambda_attributes_allowed = TRUE;
          static_call_operator_enabled = TRUE;
          if (relaxed_constexpr_enabled) {
            local_static_constexpr_enabled = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
      if (microsoft_version >= 1950) {
        if (ms_cpp23_mode) {
          auto_cast_enabled = TRUE;
        }  /* if */
      }  /* if */
      if (microsoft_version >= 1951) {
        if (ms_cpp23_mode) {
          inheriting_ctor_ctad_enabled = TRUE;
        }  /* if */
      }  /* if */
    } else {
      /* Disable unrestricted unions because they involve making some special
         member functions "deleted", whereas Microsoft compilers prior to 1900
         have a different (i.e., nonstandard) behavior wrt. special members
         that would be implicitly deleted. */
      if (option_kind_used[(int)optk_unrestricted_unions] &&
          unrestricted_unions_enabled) {
        command_line_error(ec_cl_unrestricted_unions_in_microsoft_mode);
      }  /* if */
      unrestricted_unions_enabled = FALSE;
    }  /* if */
    /* Microsoft allows specializations to use inaccessible members. */
    relaxed_specialization_access_checking = TRUE;
  }  /* if */
  /* In C++ mode, the Microsoft compiler sometimes finds typedefs when
     looking up names in elaborated type specifiers.  This flag causes
     the lookup routines to find such typedefs, which are then sometimes
     discarded in Microsoft mode. */
  elab_type_lookup_finds_typedefs = !C_mode();
  /* Value-initialization is not implemented as of MSVC++ 7.1 */
  if (emulate_msvc_value_initialization_bugs) {
    value_initialization_enabled = FALSE;
  }  /* if */
  /* The Microsoft C++ compiler does not check accessibility of friend function
     declarations. */
  no_access_check_on_friend_declarator_ids = TRUE;
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
  create_microsoft_if_exists_entries = TRUE;
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
  if (microsoft_version >= 1400) {
    if (!option_kind_used[(int)optk_variadic_macros]) {
      /* Variadic macros are supported in version 8.0 and later. */
      variadic_macros_allowed = TRUE;
    }  /* if */
    if (!(option_kind_used[(int)optk_restrict])) {
      /* The keyword "__restrict" (i.e., the GNU variant of the C99 keyword
         "restrict") is enabled in version 8.0. */
      gnu_restrict_keyword_enabled = TRUE;
    }  /* if */
  }  /* if */
  va_arg_returns_lvalue = TRUE;
  assume_references_cannot_be_null = FALSE;
#if DO_IL_LOWERING
  assume_this_cannot_be_null_in_conditional_operators = FALSE;
#endif /* DO_IL_LOWERING */
  ms_declspec_attributes_enabled = TRUE;
  if (!option_kind_used[(int)optk_implicit_noexcept]) {
    /* Microsoft compilers that implement noexcept, also treat destructors and
       operator delete as implicitly noexcept. */
    implicit_noexcept_enabled = noexcept_enabled;
  }  /* if */
  /* Microsoft compilers by default treat same-type casts as no-ops, thereby
     preserving lvalueness of the operand.  (Recent versions of the compiler
     have an option -- /Zc:rvalueCast -- to enable the standard behavior
     instead, but that option only applies to C++ mode.) */
  if (!option_kind_used[(int)optk_preserve_lvalues_with_same_type_casts] &&
      !option_kind_used[(int)optk_ms_rvalue_cast]) {
    preserve_lvalues_with_same_type_casts = ms_permissive;
  }  /* if */
  if (!ms_permissive && !option_kind_used[(int)optk_ms_strict_ternary]) {
    /* In non-permissive mode, the standard behavior of the ?: operator is
       enforced, unless overridden with --no_ms_strict_ternary. */
    ms_strict_ternary = TRUE;
  }  /* if */
  /* MSVC doesn't treat bit fields in any special way wrt. promotion, much
     less operations applied to bit fields. */
  bit_field_promotion_applies_to_some_operations = FALSE;
  /* MSVC still accepts "false" as a null pointer constant. */
  false_literal_is_not_null_pointer_constant = FALSE;
  if (microsoft_bugs && ms_permissive) {
    ms_treat_copy_init_as_direct_init = TRUE;
  }  /* if */
  if (ms_std_preproc) {
    pragma_operator_allowed = TRUE;
    va_opt_enabled = TRUE;
  }  /* if */
  if (!option_kind_used[(int)optk_relaxed_abstract_checking]) {
    /* Support for P0929R2 began in version 19.25. */
    relaxed_abstract_checking = microsoft_version >= 1925;
  }  /* if */
  if (!option_kind_used[(int)optk_old_id_chars]) {
    /* MSVC has not (as of version 19.33) implemented Unicode-based
       identifier characters. */
    old_id_chars = TRUE;
  }  /* if */
}  /* set_microsoft_mode_flags */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void set_cfront_mode_flags(void)
/*
Set other options whose values should be changed when cfront mode
is enabled.  Only set the option values if they were not already set
by a command line option.
*/
{
  check_assertion(any_cfront_mode());
  if (cfront_2_1_mode) {
    /* cfront 2.1 compatibility mode. */
    if (!(option_kind_used[(int)optk_special_subscript_cost])) {
      special_subscript_cost = FALSE;
    }  /* if */
    allow_nonconst_call_anachronism = TRUE;
  } else {
    /* cfront 3.0 compatibility mode. */
    if (!(option_kind_used[(int)optk_special_subscript_cost])) {
      special_subscript_cost = TRUE;
    }  /* if */
    allow_nonconst_call_anachronism = FALSE;
  }  /* if */
  /* Processing common to both cfront modes. */
  /* Set flags to the appropriate mode unless they have been explicitly
     set by other command line options. */
  if (!(option_kind_used[(int)optk_cplusplus_anachronisms])) {
    allow_anachronisms = TRUE;
  }  /* if */
  if (!(option_kind_used[(int)optk_nonconst_ref_anachronism])) {
    allow_nonconst_ref_anachronism = TRUE;
  }  /* if */
  if (!(option_kind_used[(int)optk_long_lifetime_temps])) {
    long_lifetime_temps = TRUE;
  }  /* if */
  if (!(option_kind_used[(int)optk_bool_is_keyword])) {
    bool_is_keyword = FALSE;
  }  /* if */
  if (!(option_kind_used[(int)optk_explicit])) {
    explicit_keyword_enabled = FALSE;
  }  /* if */
#if !RUNTIME_USES_NAMESPACES
  if (!(option_kind_used[(int)optk_arg_dependent_lookup])) {
    arg_dependent_lookup_enabled = FALSE;
  }  /* if */
  if (!(option_kind_used[(int)optk_namespaces])) {
    namespaces_enabled = FALSE;
  }  /* if */
#endif /* !RUNTIME_USES_NAMESPACES */
#if !RUNTIME_USES_TYPENAME
  if (!(option_kind_used[(int)optk_typename])) {
    typename_enabled = FALSE;
  }  /* if */
#endif /* !RUNTIME_USES_TYPENAME */
  if (!(option_kind_used[(int)optk_implicit_typename])) {
    implicit_typename_enabled = TRUE;
  }  /* if */
  if (!(option_kind_used[(int)optk_old_for_init])) {
    use_nonstandard_for_init_scope = TRUE;
  }  /* if */
  if (!(option_kind_used[(int)optk_base_assign_op_is_default])) {
    allow_copy_assignment_op_with_base_class_param = TRUE;
  }  /* if */
  if (!(option_kind_used[(int)optk_guiding_decls])) {
    guiding_decls_allowed = FALSE;
  }  /* if */
  if (!(option_kind_used[(int)optk_old_specializations])) {
    old_specializations_allowed = TRUE;
  }  /* if */
#if IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE
  if (!(option_kind_used[(int)optk_implicit_extern_c_type_conversion])) {
    impl_conv_between_c_and_cpp_function_ptrs_allowed = TRUE;
  }  /* if */
#endif /* IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE */
  if (!(option_kind_used[(int)optk_extern_inline])) {
    extern_inline_allowed = FALSE;
  }  /* if */
  if (!(option_kind_used[(int)optk_enum_overloading])) {
    operator_overloading_on_enums_enabled = FALSE;
  }  /* if */
  if (!(option_kind_used[(int)optk_const_string_literals])) {
    string_literals_are_const = FALSE;
  }  /* if */
  if (!option_kind_used[(int)optk_deprecated_string_conv]) {
    deprecated_string_literal_conv_allowed = TRUE;
  }  /* if */
  if (!(option_kind_used[(int)optk_late_tiebreaker])) {
    do_late_ovl_res_tiebreaker = TRUE;
  }  /* if */
  if (!(option_kind_used[(int)optk_friend_injection])) {
    friend_class_injection_enabled = TRUE;
    friend_function_injection_enabled = TRUE;
  }  /* if */
  if (!(option_kind_used[(int)optk_dependent_name_processing])) {
    do_dependent_name_processing = FALSE;
  }  /* if */
  if (!option_kind_used[(int)optk_export_template]) {
    export_template_allowed = FALSE;
    export_keyword_enabled = FALSE;
  }  /* if */
  if (!option_kind_used[(int)optk_parse_nonclass_templates]) {
    nonclass_prototype_instantiations = FALSE;
  }  /* if */
  if (!option_kind_used[(int)optk_nonstandard_default_arg_deduction]) {
    nonstandard_default_arg_deduction = TRUE;
  }  /* if */
  /* Set flags that cannot be overridden by command line options. */
  /* Cfront does not check accessibility of friend function declarations. */
  no_access_check_on_friend_declarator_ids = TRUE;
  if (!option_kind_used[(int)optk_implicit_noexcept]) {
    /* Keep the traditional relaxed semantics for destructors and
       operator delete. */
    implicit_noexcept_enabled = FALSE;
  }  /* if */
  type_keyword_in_dtor_allowed = TRUE;
  template_linkage_depends_on_instantiation_args = FALSE;
}  /* set_cfront_mode_flags */


static void check_pch_file_name(a_const_char *file_name)
/*
Make sure the specified file name is acceptable as an output file.
If it is not acceptable, issue an error.
*/
{
  if (!okay_as_output_file(file_name)) {
    output_file_open_error(/*bad_name=*/TRUE, ec_precompiled_header, file_name,
                           es_command_line_error);
  }  /* if */
}  /* check_pch_file_name */


static void check_module_header_unit_file_name(a_const_char *file_name)
/*
Make sure the specified file name is acceptable as an output file.
If it is not acceptable, issue an error.
*/
{
  if (!okay_as_output_file(file_name)) {
    output_file_open_error(/*bad_name=*/TRUE, ec_edg_ifc_header_unit,
                           file_name, es_command_line_error);
  }  /* if */
}  /* check_module_header_unit_file_name */


static void check_module_unit_file_name(a_const_char *file_name)
/*
Make sure the specified file name is acceptable as an output file.
If it is not acceptable, issue an error.
*/
{
  if (!okay_as_output_file(file_name)) {
    output_file_open_error(/*bad_name=*/TRUE, ec_edg_ifc_interface_unit,
                           file_name, es_command_line_error);
  }  /* if */
}  /* check_module_unit_file_name */


static void check_module_unit_partition_file_name(a_const_char *file_name)
/*
Make sure the specified file name is acceptable as an output file.
If it is not acceptable, issue an error.
*/
{
  if (!okay_as_output_file(file_name)) {
    output_file_open_error(/*bad_name=*/TRUE, ec_edg_ifc_partition_unit,
                           file_name, es_command_line_error);
  }  /* if */
}  /* check_module_unit_partition_file_name */


#if COMPILE_MULTIPLE_SOURCE_FILES || COMPILE_MULTIPLE_TRANSLATION_UNITS
STATIC_THREAD char
		**argv_file_list;
STATIC_THREAD int
		argc_file_list;
			/* When multiple source input files are accepted,
			   argc_file_list is the count of files remaining
			   after the current one, and argv_file_list
			   points to the argv entry for the first
			   remaining file. */
#endif /* COMPILE_MULTIPLE_SOURCE_FILES ||
          COMPILE_MULTIPLE_TRANSLATION_UNITS */


static void check_and_set_new_c_mode_options(void)
/*
Set the various flags appropriate to C99 mode or later standard modes.
*/
{
  check_assertion(std_version >= 199901);
#if VLA_ALLOWED
  if (!(option_kind_used[(int)optk_vla])) {
    /* Support for VLAs is turned on by default in C99 mode. */
    vla_enabled = TRUE;
  }  /* if */
#endif /* VLA_ALLOWED */
  if (!(option_kind_used[(int)optk_restrict])) {
    /* Support for restricted pointers is turned on by default in C99 mode. */
    restrict_keyword_enabled = TRUE;
  }  /* if */
  if (!(option_kind_used[(int)optk_designators])) {
    /* Support for designators is turned on by default in C99 mode. */
    designators_allowed = TRUE;
  }  /* if */
#if COMPOUND_LITERAL_ENABLING_POSSIBLE
  if (!(option_kind_used[(int)optk_compound_literals])) {
    /* Support for compound literals is turned on by default in C99 mode. */
    compound_literals_allowed = TRUE;
  }  /* if */
#endif /* COMPOUND_LITERAL_ENABLING_POSSIBLE */
  if (!(option_kind_used[(int)optk_variadic_macros])) {
    /* Support for variadic macros is turned on by default in C99 mode. */
    variadic_macros_allowed = TRUE;
  }  /* if */
  if (!(option_kind_used[(int)optk_alternative_tokens])) {
    /* Support for alternative tokens is turned on by default in C99 mode. */
    alternative_tokens_allowed = TRUE;
  }  /* if */
  /* The _Pragma operator is allowed. */
  pragma_operator_allowed = TRUE;
  /* In C99 mode, strict or otherwise, // comments are allowed. */
  end_of_line_comments_allowed = TRUE;
  /* The final field of a struct may be an incomplete array. */
  flexible_array_members_allowed = TRUE;
  /* Universal character names are allowed. */
  universal_character_names_allowed = TRUE;
  /* The va_copy macro should be recognized. */
  va_copy_macro_allowed = TRUE;
  /* The long long data type is not an extension in C99. */
  long_long_is_standard = TRUE;
  long_long_promotion_allowed = TRUE;
  /* Hexadecimal floating point constants are permitted. */
  hex_floating_point_constants_allowed = TRUE;
  /* Allow nonconstant expressions in aggregate initializers for automatic
     variables. */
  allow_nonconstant_auto_aggr_init_in_c_mode = TRUE;
  mixed_string_concat_enabled = TRUE;
  std_c99_inlining = TRUE;
  gnu_c89_inlining = FALSE;
  c99_bool_is_keyword = TRUE;
  allow_decl_after_stmt = TRUE;
  if (c11_mode) {
    static_assert_enabled = TRUE;
    allow_c11_anonymous_unions = TRUE;
    alignas_enabled = TRUE;
    alignof_enabled = TRUE;
    std_thread_local_storage_specifier_enabled = TRUE;
    if (!option_kind_used[(int)optk_uliterals]) {
      /* Unicode literals are supported in C11 mode unless explicitly
         suppressed. */
      uliterals_enabled = TRUE;
    }  /* if */
    c11_atomic_enabled = TRUE;
    noreturn_keyword_enabled = TRUE;
  }  /* if */
  if (c23_mode) {
    std_attributes_enabled = TRUE;
    nodiscard_attribute_enabled = TRUE;
    enumerator_attributes_enabled = TRUE;
    explicit_enum_base_enabled = TRUE;
    opaque_enum_decls_enabled = TRUE;
    if (!option_kind_used[(int)optk_utf8_char_literals]) {
      utf8_char_literals_enabled = TRUE;
    }  /* if */
    binary_literals_allowed = TRUE;
    if (!option_kind_used[(int)optk_trigraphs]) {
      trigraphs_allowed = FALSE;
    }  /* if */
    va_opt_enabled = TRUE;
    allow_ellipsis_only_param_in_C_mode = TRUE;
    if (!option_kind_used[(int)optk_digit_separators]) {
      digit_separators_enabled = TRUE;
    }  /* if */
    elifdef_enabled = TRUE;
    terse_static_assert_enabled = TRUE;
    if (!option_kind_used[(int)optk_nullptr]) {
      nullptr_enabled = TRUE;
    }  /* if */
    embed_enabled = TRUE;
  }  /* if */
}  /* check_and_set_new_c_mode_options */


static void set_c_mode_flags(void)
/*
Set the various flags appropriate for the specific C mode we are going to
process.
*/
{
  /* Turn off language features that must not be on in C mode, in case
     the default value is on. */
  exceptions_enabled = FALSE;
  rtti_enabled = FALSE;
  array_new_and_delete_enabled = FALSE;
  explicit_keyword_enabled = FALSE;
  namespaces_enabled = FALSE;
  wchar_t_is_keyword = FALSE;
  bool_is_keyword = FALSE;
  record_form_of_name_reference = FALSE;
  /* Set global flags having to do with the potential sizes of enum types.
     They must be no larger than int in C. */
  enum_types_can_be_larger_than_int = FALSE;
  if (C_dialect == C_dialect_pcc || SVR4_C_mode) {
    enum_types_can_be_smaller_than_int = FALSE;
  } else {
    enum_types_can_be_smaller_than_int =
                            targ_enum_types_can_be_smaller_than_int;
  }  /* if */
  if (C_dialect == C_dialect_pcc) {
    /* Alternative tokens are not recognized in PCC mode. */
    alternative_tokens_allowed = FALSE;
    /* PCC mode doesn't treat bit fields in any special way wrt. promotion,
       much less operations applied to bit fields. */
    bit_field_promotion_applies_to_some_operations = FALSE;
  }  /* if */
  special_subscript_cost = FALSE;  /* Not really needed. */
  use_nonstandard_for_init_scope = TRUE;  /* Not really needed. */
  nonstandard_qualifier_deduction = FALSE;  /* Not really needed. */
  warning_on_for_init_difference = FALSE;
  remove_qualifiers_from_param_types = FALSE;
  keep_restrict_in_signatures = TRUE;
  impl_conv_between_c_and_cpp_function_ptrs_allowed = FALSE;
  extern_inline_allowed = FALSE;
  operator_overloading_on_enums_enabled = FALSE;  /* Not really needed. */
  string_literals_are_const = FALSE;
  deprecated_string_literal_conv_allowed = FALSE;
  arg_dependent_lookup_enabled = FALSE;
  instantiate_before_pch_creation = FALSE;
  instantiate_extern_inline = FALSE;
  instantiate_inline_variables = FALSE;
  do_dependent_name_processing = FALSE;
  nonstandard_instantiation_lookup_enabled = FALSE;
  export_template_allowed = FALSE;
  export_keyword_enabled = FALSE;
  va_list_in_std_namespace = FALSE;
  /* The final field of a struct may be an incomplete array. */
  flexible_array_members_allowed = TRUE;
  /* Set the variable that controls whether "//" is allowed as a comment
     delimiter. */
  if (c99_mode || microsoft_mode) {
    /* The variable is set elsewhere. */
  } else if (strict_ansi_mode) {
    /* Strict ANSI/ISO C (not C99): // comments are not allowed. */
    end_of_line_comments_allowed = FALSE;
  } else if (C_dialect == C_dialect_pcc) {
    /* pcc mode: // comments are not allowed. */
    end_of_line_comments_allowed = FALSE;
  } else {
    /* Normal C mode. */
    end_of_line_comments_allowed = END_OF_LINE_COMMENTS_ALLOWED_IN_C_MODE;
  }  /* if */
  if (SVR4_C_mode) {
    /* Turn on features implied by SVR4 C mode. */
    address_of_ellipsis_allowed = TRUE;
    allow_ellipsis_only_param_in_C_mode = TRUE;
  } else if (c99_mode) {
    /* Turn on features implied by C99 mode and later C standard modes. */
    check_and_set_new_c_mode_options();
  }  /* if */
  elab_type_lookup_finds_typedefs = FALSE;
  if (option_kind_used[(int)optk_type_traits_helpers]) {
    command_line_error(ec_cl_type_traits_helpers_option_only_in_cplusplus);
  }  /* if */
  type_traits_helpers_enabled = FALSE;
  lambdas_enabled = FALSE;
  rvalue_references_enabled = FALSE;
  rvalue_ctor_is_copy_ctor = FALSE;
  local_types_as_template_args_enabled = FALSE;
  decls_using_types_without_linkage_allowed = FALSE;
  if (!option_kind_used[(int)optk_auto_type]) {
    /* In C23 "auto" can be a type specifier, in which case the type of the
       object is deduced from its initializer.  Earlier C modes give "auto"
       only its traditional storage class meaning, which C23 retains as well;
       the two meanings are told apart by the presence of a type specifier. */
    auto_type_specifier_enabled = c23_mode;
  }  /* if */
  auto_storage_class_specifier_enabled = TRUE;
  trailing_return_types_enabled = FALSE;
  this_in_trailing_return_types_enabled = FALSE;
  cpp11_sfinae_enabled = FALSE;
  cpp11_sfinae_ignore_access = FALSE;
  diag_override_does_not_affect_sfinae = FALSE;
  variadic_templates_enabled = FALSE;
  gnu_bases_operators_enabled = FALSE;
  inline_namespaces_enabled = FALSE;
  assume_references_cannot_be_null = FALSE;
#if DO_IL_LOWERING
  assume_this_cannot_be_null_in_conditional_operators = FALSE;
#endif /* DO_IL_LOWERING */
  char16_t_and_char32_t_are_keywords = FALSE;
  range_based_for_enabled = FALSE;
  if (!option_kind_used[(int)optk_func_prototype_tags]) {
    /* In standard C tag names are sometimes entered in function prototype
       scopes.  Microsoft C mode will override this. */
    func_prototype_tags_enabled = TRUE;
  }  /* if */
  relaxed_abstract_checking = FALSE;
  if (!option_kind_used[(int)optk_require_func_prototypes]) {
    require_func_prototypes = c23_mode;
  }  /* if */
}  /* set_c_mode_flags */


static void check_and_set_c_mode_options(void)
/*
This routine is called in C mode to check that no C++-only command-line
setting is used, and to set various unmentioned settings as needed.
*/
{
  check_assertion(C_mode());
  if (std_version == 0) {
    /* No specific version of C has been established yet: Use the appropriate
       default. */
#if DEFAULT_C99_MODE
    std_version = 199901;
#else /* !DEFAULT_C99_MODE */
    std_version = 199000;
#endif /* DEFAULT_C99_MODE */
    if (!c_mode_specified() && gcc_mode) {
      /* Set the version of C based on the version of GNU/clang being
         emulated. */
      if (clang_mode) {
        if (clang_version >= 110000) {
          std_version = 201710;
        } else if (clang_version >= 30600) {
          std_version = 201112;
        }  /* if */
      } else {
        if (gnu_version >= 150000) {
          std_version = 202311;
        } else if (gnu_version >= 80000) {
          std_version = 201710;
        } else if (gnu_version >= 50000) {
          std_version = 201112;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (option_kind_used[(int)optk_cplusplus_anachronisms]) {
    command_line_error(ec_cl_anachronism_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_virtual_function_table_definition]) {
    command_line_error(ec_cl_vtbl_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_template_instantiation_mode]) {
    command_line_error(ec_cl_instantiation_option_only_in_cplusplus);
  }  /* if */
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  if (option_kind_used[(int)optk_automatic_template_instantiation]) {
    command_line_error(ec_cl_auto_instantiation_option_only_in_cplusplus);
  }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
  if (option_kind_used[(int)optk_implicit_template_inclusion]) {
    command_line_error(ec_cl_implicit_inclusion_option_only_in_cplusplus);
  }  /* if */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
  if (option_kind_used[(int)optk_exception_handling]) {
    command_line_error(ec_cl_exceptions_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_rtti]) {
    command_line_error(ec_cl_rtti_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_array_new_and_delete]) {
    command_line_error(ec_cl_array_new_and_delete_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_explicit]) {
    command_line_error(ec_cl_explicit_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_namespaces]) {
    command_line_error(ec_cl_namespaces_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_wchar_t_is_keyword]) {
    command_line_error(ec_cl_wchar_t_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_bool_is_keyword]) {
    command_line_error(ec_cl_bool_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_special_subscript_cost]) {
    command_line_error(
                      ec_cl_special_subscript_cost_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_typename]) {
    command_line_error(ec_cl_typename_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_implicit_typename]) {
    command_line_error(ec_cl_implicit_typename_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_old_for_init]) {
    command_line_error(ec_cl_old_for_init_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_for_init_diff_warning]) {
    command_line_error(ec_cl_for_init_diff_warning_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_guiding_decls]) {
    command_line_error(ec_cl_guiding_decls_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_old_specializations]) {
    command_line_error(ec_cl_old_specializations_option_only_in_cplusplus);
  }  /* if */
#if IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE
  if (option_kind_used[(int)optk_implicit_extern_c_type_conversion]) {
    command_line_error(ec_cl_impl_extern_c_conv_option_only_in_cplusplus);
  }  /* if */
#endif /* IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE */
  if (option_kind_used[(int)optk_extern_inline]) {
    command_line_error(ec_cl_extern_inline_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_embedded_cplusplus]) {
    command_line_error(ec_cl_embedded_cplusplus_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_enum_overloading]) {
    command_line_error(ec_cl_enum_overloading_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_nonstandard_qualifier_deduction]) {
    command_line_error(
            ec_cl_nonstandard_qualifier_deduction_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_nonstandard_using_decl]) {
    command_line_error(
            ec_cl_nonstd_using_decl_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_class_name_injection]) {
    command_line_error(ec_cl_class_name_injection_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_arg_dependent_lookup]) {
    command_line_error(ec_cl_arg_dependent_lookup_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_friend_injection]) {
    command_line_error(ec_cl_friend_injection_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_dependent_name_processing]) {
    command_line_error(ec_cl_dep_name_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_parse_nonclass_templates]) {
    command_line_error(
                     ec_cl_parse_nonclass_templates_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_export_template]) {
    command_line_error(ec_cl_export_template_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_ignore_namespace_std]) {
    command_line_error(ec_cl_ignore_std_option_only_in_cplusplus);
  }  /* if */
#if ONE_INSTANTIATION_PER_OBJECT
  if (option_kind_used[(int)optk_one_instantiation_per_object]) {
    command_line_error(
            ec_cl_one_instantiation_per_object_option_only_in_cplusplus);
  }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  if (option_kind_used[(int)optk_late_tiebreaker]) {
    command_line_error(ec_cl_late_tiebreaker_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_pending_instantiations]) {
    command_line_error(
                     ec_cl_pending_instantiations_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_lambdas]) {
    command_line_error(ec_cl_lambdas_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_rvalue_references]) {
    command_line_error(ec_cl_rvalue_references_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_rvalue_ctor_is_copy_ctor]) {
    command_line_error(
                     ec_cl_rvalue_ctor_is_copy_ctor_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_gen_move_operations]) {
    command_line_error(ec_cl_gen_move_operations_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_auto_type]) {
    command_line_error(ec_cl_auto_type_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_auto_storage]) {
    command_line_error(ec_cl_auto_storage_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_variadic_templates]) {
    command_line_error(ec_cl_variadic_templates_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_max_depth_constexpr_call] ||
      option_kind_used[(int)optk_max_cost_constexpr_call]) {
    command_line_error(ec_cl_max_constexpr_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_unrestricted_unions]) {
    command_line_error(ec_cl_unrestricted_unions_option_only_in_cplusplus);
  }  /* if */
#if SUN_EXTENSIONS_ALLOWED
  if (!(option_kind_used[(int)optk_sun_linker_scope]) &&
      !microsoft_mode && !strict_ansi_mode) {
    /* If the Sun linker scope option was not set on the command line, set
       its value now based on the configuration macros. */
#if DEFAULT_SUN_LINKER_SCOPE_ALLOWED && DEFAULT_SUN_COMPATIBILITY
    sun_linker_scope_allowed = TRUE;
#else /* !(DEFAULT_SUN_LINKER_SCOPE_ALLOWED && DEFAULT_SUN_COMPATIBILITY) */
    sun_linker_scope_allowed = FALSE;
#endif /* DEFAULT_SUN_LINKER_SCOPE_ALLOWED && DEFAULT_SUN_COMPATIBILITY */
  }  /* if */
#endif /* SUN_EXTENSIONS_ALLOWED */
  if (option_kind_used[(int)optk_relaxed_abstract_checking]) {
    command_line_error(ec_cl_relaxed_abstract_checking_only_in_cplusplus);
  }  /* if */
  set_c_mode_flags();
}  /* check_and_set_c_mode_options */


static void check_and_set_default_cpp_standard_version(void)
/*
This routine is called in C++ mode to set the default C++ standard version
taking into account any compiler emulation and the version of the emulation.
*/
{
  if (!cpp_mode_specified()) {
    if (clang_mode) {
      /* Handle Clang mode C++ standard version defaults. */
      if (clang_version >= 160000) {
        /* Beginning with Clang 16.0.0, C++17 features are enabled by
           default. */
        std_version = 201703;
      } else if (clang_version >= 60000) {
        /* Beginning with Clang 6.0.0, C++14 features are enabled by
           default. */
        std_version = 201402;
      }  /* if */
    } else if (gnu_mode) {
      /* Handle GNU mode C++ standard version defaults. */
      if (gnu_version >= 110000) {
        /* Beginning with g++ 11.1.0 (the first g++ release in the 11.x
           series), C++17 features are enabled by default. */
        std_version = 201703;
      } else if (gnu_version >= 60000) {
        /* Beginning with g++ 6.1.0 (the first g++ release in the 6.x series),
           C++14 features are enabled by default. */
        std_version = 201402;
      }  /* if */
    }  /* if */
    if (std_version == 0) {
      /* No specific version of C++ has been established yet: Use the
         appropriate default. */
      std_version = DEFAULT_CPP_MODE;
    }  /* if */
  }  /* if */
}  /* check_and_set_default_cpp_standard_version */


static void check_and_set_cpp11_mode_options(a_boolean value)
/*
Explicitly enable (when value is TRUE) or disable (when value is FALSE) any
features specific to C++11 and later C++ standards.  In addition, enable or
disable some pre-C++11 standard features that are not always enabled in
default mode (e.g., exception handling).
*/
{
  if (!(option_kind_used[(int)optk_alternative_tokens])) {
    /* Enable alternative tokens by default in C++11 mode. */
    alternative_tokens_allowed = TRUE;
  }  /* if */
  if (!option_kind_used[(int)optk_exception_handling]) {
    /* Enable exceptions by default in C++11 mode. */
    exceptions_enabled = value;
  }  /* if */
  right_shift_can_be_angle_brackets = value;
  extended_friends_enabled = value;
  mixed_string_concat_enabled = value;
  if (!option_kind_used[(int)optk_long_long]) {
    long_long_is_standard = value;
  }  /* if */
  long_long_promotion_allowed = value;
  if (!option_kind_used[(int)optk_variadic_macros]) {
    variadic_macros_allowed = value;
  }  /* if */
  pragma_operator_allowed = value;
  static_assert_enabled = value;
  if (!option_kind_used[(int)optk_auto_type]) {
    auto_type_specifier_enabled = value;
  }  /* if */
  if (!option_kind_used[(int)optk_auto_storage]) {
    auto_storage_class_specifier_enabled = !value;
  }  /* if */
  if (!option_kind_used[(int)optk_dependent_name_processing]) {
    do_dependent_name_processing = value ?
                                      DEFAULT_CPP11_DEPENDENT_NAME_PROCESSING :
                                      FALSE;
  }  /* if */
  if (!option_kind_used[(int)optk_parse_nonclass_templates]) {
    nonclass_prototype_instantiations = value ?
                                      DEFAULT_CPP11_DEPENDENT_NAME_PROCESSING :
                                      FALSE;
  }  /* if */
  extern_template_allowed = value;
  standard_form_of_extern_template = value;
  decltype_enabled = value;
  enable_decltype_in_base_specifier_and_mem_initializer = value;
  explicit_enum_base_enabled = value;
  enum_qualifiers_enabled = value;
  opaque_enum_decls_enabled = value;
  if (!option_kind_used[(int)optk_lambdas]) {
    lambdas_enabled = value;
  }  /* if */
  if (!option_kind_used[(int)optk_rvalue_references]) {
    rvalue_references_enabled = value;
  }  /* if */
  if (!rvalue_references_enabled) {
    /* Without rvalue references there cannot be move-operations. */
    rvalue_ctor_is_copy_ctor = FALSE;
    generate_move_operations = FALSE;
  } else {
    if (!rvalue_ctor_is_copy_ctor ||
        !option_kind_used[(int)optk_rvalue_ctor_is_copy_ctor]) {
      /* Very late in the standardization of C++11, rules were added to
         generate move constructors and have user-declared move constructors
         cause implicitly-declared copy constructors to be "deleted".  That
         standard behavior is the default in C++11 mode.  However, before that
         change in rules, a user-declared move constructor simply inhibited
         the implicit declaration of a copy constructor altogether (the effect
         obtained by setting rvalue_ctor_is_copy_ctor to TRUE). */
      generate_move_operations = TRUE;
      rvalue_ctor_is_copy_ctor = FALSE;
    }  /* if */
  }  /* if */
  local_types_as_template_args_enabled = value;
  decls_using_types_without_linkage_allowed = value;
  if (!option_kind_used[(int)optk_unrestricted_unions]) {
    unrestricted_unions_enabled = value;
  }  /* if */
  defaulted_special_members_enabled = value;
  deleted_functions_enabled = value;
  trailing_return_types_enabled = value;
  this_in_trailing_return_types_enabled = value;
  list_init_enabled = value;
  field_initializers_enabled = value;
  std_attributes_enabled = value;
  alignas_enabled = value;
  alignof_enabled = value;
  alias_declarations_enabled = value;
  inline_namespaces_enabled = value;
  if (!option_kind_used[(int)optk_variadic_templates]) {
    variadic_templates_enabled = value;
  }  /* if */
  if (!option_kind_used[(int)optk_nullptr]) {
    nullptr_enabled = value;
  }  /* if */
  if (!option_kind_used[(int)optk_cpp11_sfinae]) {
    cpp11_sfinae_enabled = value;
  }  /* if */
  if (!option_kind_used[(int)optk_cpp11_sfinae_ignore_access]) {
    if (cpp11_sfinae_enabled) {
      cpp11_sfinae_ignore_access = value ? DEFAULT_CPP11_SFINAE_IGNORE_ACCESS :
                                           FALSE;
    }  /* if */
  }  /* if */
  if (!(option_kind_used[(int)optk_export_template])) {
    /* If export template processing was not explicitly set by a command line
       option, disable it in C++11 mode. */
    export_template_allowed = EXPORT_ENABLING_POSSIBLE && !value;/*lint !e506*/
  }  /* if */
  explicit_conversion_functions_enabled = value;
  if (!option_kind_used[(int)optk_uliterals]) {
    /* Enable U-literals by default in C++11 mode (unless they were explicitly
       mentioned on the command line).  This also has the effect of enabling
       char16_t/char32_t keywords. */
    uliterals_enabled = value;
  }  /* if */
  range_based_for_enabled = value;
  if (!option_kind_used[(int)optk_friend_injection] && value) {
    /* Disable friend injection in C++11 mode. */
    friend_class_injection_enabled = FALSE;
    friend_function_injection_enabled = FALSE;
  }  /* if */
  noexcept_enabled = value;
  if (exceptions_enabled && noexcept_enabled &&
      !option_kind_used[(int)optk_implicit_noexcept]) {
    if (strict_ansi_mode) {
      implicit_noexcept_enabled = TRUE;
    } else {
      implicit_noexcept_enabled = DEFAULT_IMPLICIT_NOEXCEPT_ENABLED;
    }  /* if */
  }  /* if */
  delegating_constructors_enabled = value;
  inheriting_constructors_enabled = value;
  ref_qualifiers_enabled = rvalue_references_enabled;
  constexpr_enabled = value;
  if (!option_kind_used[(int)optk_deprecated_string_conv]) {
    deprecated_string_literal_conv_allowed = value ?
                               DEFAULT_DEPRECATED_STRING_LITERAL_CONV_ALLOWED :
                               TRUE;
  }  /* if */
  if (!option_kind_used[(int)optk_user_defined_literals]) {
    user_defined_literals_enabled = value;
  }  /* if */
  raw_string_literals_enabled = value;
  std_thread_local_storage_specifier_enabled = value;
  std_override_modifiers_enabled = value;
  constexpr_implies_const = value;
  register_is_deprecated = TRUE;
  if (cpp14_mode) {
    /* Features enabled in C++14 mode. */
    if (auto_type_specifier_enabled) {
      decltype_auto_enabled = TRUE;
    }  /* if */
    deduced_return_types_enabled = TRUE;
    generic_lambdas_enabled = TRUE;
    generic_lambdas_can_implicitly_capture = TRUE;
    init_capture_enabled = TRUE;
    if (field_initializers_enabled && list_init_enabled) {
      aggregate_classes_can_have_field_initializers = TRUE;
    }  /* if */
    binary_literals_allowed = TRUE;
    if (!option_kind_used[(int)optk_digit_separators]) {
      digit_separators_enabled = TRUE;
    }  /* if */
    if (rvalue_references_enabled && !gpp_mode && !clang_mode) {
      selection_from_prvalue_is_xvalue = TRUE;
    }  /* if */
    relaxed_constexpr_enabled = TRUE;
    sized_deallocation_enabled = RUNTIME_SUPPORTS_SIZED_DEALLOCATION;
    if (gpp_mode && !clang_mode && gnu_version < 50000) {
      constexpr_implies_const = TRUE;
    } else {
      constexpr_implies_const = FALSE;
    }  /* if */
    variable_templates_enabled = TRUE;
    /* This was added as a DR to C++14. */
    generalized_template_template_matching = TRUE;
  }  /* if */
  if (cpp17_mode) {
    /* Features enabled in C++17 mode. */
    namespace_attributes_enabled = TRUE;
    nested_namespace_definitions_enabled = TRUE;
    enumerator_attributes_enabled = TRUE;
    terse_static_assert_enabled = TRUE;
    if (!option_kind_used[(int)optk_utf8_char_literals]) {
      utf8_char_literals_enabled = TRUE;
    }  /* if */
    if (range_based_for_enabled) {
      relaxed_range_based_for_enabled = TRUE;
    }  /* if */
    register_is_disallowed = TRUE;
    operator_bool_increment_allowed = FALSE;
    using_attribute_namespaces_enabled = TRUE;
    nodiscard_attribute_enabled = TRUE;
    hex_floating_point_constants_allowed = TRUE;
    struct_bindings_enabled = TRUE;
    selection_initializers_enabled = TRUE;
    direct_init_fixed_base_enum_enabled = TRUE;
    constexpr_if_enabled = TRUE;
    constexpr_lambdas_enabled = TRUE;
    capture_star_this_enabled = TRUE;
    fold_expressions_enabled = TRUE;
    variadic_using_decls_enabled = TRUE;
    inline_variables_allowed = TRUE;
#if EXC_SPEC_IN_FUNC_TYPE_ENABLING_POSSIBLE
    if (!option_kind_used[(int)optk_exc_spec_in_func_type]) {
      exc_spec_in_func_type = TRUE;
    }  /* if */
#endif /* EXC_SPEC_IN_FUNC_TYPE_ENABLING_POSSIBLE */
    if (!option_kind_used[(int)optk_aligned_new]) {
      overaligned_allocation_enabled = TRUE;
    }  /* if */
    mandatory_copy_elision = TRUE;
    generalized_nontype_arguments = TRUE;
    strict_cpp17_eval_order = TRUE;
    if (!option_kind_used[(int)optk_trigraphs]) {
      trigraphs_allowed = FALSE;
    }  /* if */
    aggregate_classes_can_have_bases = TRUE;
    generalized_template_template_matching = TRUE;
    class_template_arg_deduction_enabled = TRUE;
    auto_template_params_enabled = TRUE;
  }  /* if */
  if (cpp20_mode) {
    conditional_explicit_enabled = TRUE;
    constexpr_virtual_enabled = TRUE;
    constexpr_try_enabled = TRUE;
    constexpr_dynamic_alloc_enabled = TRUE;
    consteval_enabled = TRUE;
    constinit_enabled = TRUE;
    if (!coroutines_enabled) {
      /* This may have been enabled already via --set_flag; we don't want
         to override that. */
      coroutines_enabled = COROUTINE_ENABLING_POSSIBLE;
    }  /* if */
    explicit_copy_this_capture_enabled = TRUE;
    lambda_template_param_list_enabled = TRUE;
    lambda_allowed_in_uneval_context = TRUE;
    aggregate_classes_can_have_user_ctors = FALSE;
    spaceship_enabled = TRUE;
    adl_for_non_visible_templates = TRUE;
    relaxed_typename_enabled = TRUE;
    relaxed_specialization_access_checking = TRUE;
    pack_init_capture_enabled = TRUE;
    using_enum_enabled = TRUE;
    aggregate_ctad_enabled = TRUE;
    alias_ctad_enabled = TRUE;
    rvalue_allowed_with_const_qual_memptr = TRUE;
    va_opt_enabled = !microsoft_mode || ms_std_preproc;
    nested_inline_namespace_definitions_enabled = TRUE;
    allow_parenthesized_aggregate_init = TRUE;
    init_statement_allowed_in_range_based_for = TRUE;
    if (!option_kind_used[(int)optk_char8_t]) {
      char8_t_enabled = TRUE;
    }  /* if */
    destroying_operator_delete_enabled = TRUE;
    if (!option_kind_used[(int)optk_concepts]) {
      concepts_enabled = TRUE;
    }  /* if */
    abbr_func_templates_enabled = TRUE;
    module_keywords_enabled = TRUE;
    if (!option_kind_used[(int)optk_modules]) {
      modules_enabled = DEFAULT_MODULES_ENABLED;
    }  /* if */
    export_keyword_enabled = FALSE;
    floating_point_template_parameters_allowed = TRUE;
  }  /* if */
  if (cpp23_mode) {
    if_consteval_enabled = TRUE;
    explicit_this_param_enabled = TRUE;
    if (!clang_mode) {
      /* As of version 21, clang does not yet support the extended
         floating-point types. */
      extended_float_types = TRUE;
    }  /* if */
    elifdef_enabled = TRUE;
    size_suffix_enabled = TRUE;
    named_unicode_chars_allowed = TRUE;
    delimited_escape_seqs_allowed = TRUE;
    lambda_attributes_allowed = TRUE;
    lambda_declarator_params_optional = TRUE;
    if (relaxed_range_based_for_enabled) {
      extended_range_based_for_lifetime = TRUE;
    }  /* if */
    multi_subscript_enabled = TRUE;
    if (relaxed_constexpr_enabled) {
      local_static_constexpr_enabled = TRUE;
    }  /* if */
    static_call_operator_enabled = TRUE;
    auto_cast_enabled = TRUE;
    inheriting_ctor_ctad_enabled = TRUE;
  }  /* if */
  if (cpp26_mode) {
    embed_enabled = TRUE;
    struct_binding_packs_enabled = TRUE;
    pack_indexing_enabled = TRUE;
  }  /* if */
  /* Disable "false" as a null pointer constant in C++11 mode (as per Core
     issue 903). */
  false_literal_is_not_null_pointer_constant = value;
}  /* check_and_set_cpp11_mode_options */


static void check_and_set_default_cpp11_extensions(void)
/*
Some C++11 features are enabled in default (i.e., non-C++11) C++ mode, but
not in other non-C++11 modes (like non-C++11 Microsoft mode).  This routine
enables the appropriate extensions in default C++ mode.  Individual features
may get enabled in the other non-C++11 modes.
*/
{
  check_assertion(!C_mode() && !cpp11_mode &&
                  !option_kind_used[(int)optk_cpp03_mode]);
  if (!strict_ansi_mode &&
      !microsoft_mode && !gpp_mode && !sun_mode && !any_cfront_mode()) {
    right_shift_can_be_angle_brackets =
                                    DEFAULT_RIGHT_SHIFT_CAN_BE_ANGLE_BRACKETS;
    mixed_string_concat_enabled = TRUE;
    extended_friends_enabled = TRUE;
    variadic_macros_allowed = TRUE;
    pragma_operator_allowed = TRUE;
    extern_template_allowed = TRUE;
    standard_form_of_extern_template = TRUE;
  }  /* if */
}  /* check_and_set_default_cpp11_extensions */


static void check_rvalue_ref_options(void)
/*
Several options are available to control the behavior of rvalue references and
move semantics.  Ensure that they are consistent.
*/
{
  if (!rvalue_references_enabled) {
    if (generate_move_operations || rvalue_ctor_is_copy_ctor) {
      /* These require that rvalue references be enabled.  If they were set
         implicitly, turn them off and keep rvalue references disabled.
         Otherwise, implicitly enable rvalue references. */
      if (!option_kind_used[(int)optk_rvalue_references]) {
        if ((generate_move_operations &&
             option_kind_used[(int)optk_gen_move_operations]) ||
            (rvalue_ctor_is_copy_ctor &&
             option_kind_used[(int)optk_rvalue_ctor_is_copy_ctor])) {
          rvalue_references_enabled = TRUE;
        } else {
          generate_move_operations = FALSE;
          rvalue_ctor_is_copy_ctor = FALSE;
        }  /* if */
      } else if (generate_move_operations &&
                 option_kind_used[(int)optk_gen_move_operations]) {
        command_line_error(ec_cl_move_operations_require_rvalue_references);
      } else if (rvalue_ctor_is_copy_ctor &&
                 option_kind_used[(int)optk_rvalue_ctor_is_copy_ctor]) {
        command_line_error(ec_cl_move_operations_require_rvalue_references);
      } else {
        generate_move_operations = FALSE;
        rvalue_ctor_is_copy_ctor = FALSE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (generate_move_operations && rvalue_ctor_is_copy_ctor) {
    /* These two are mutually exclusive.  If they were both explicitly
       enabled, issue a command-line error. */
    if (option_kind_used[(int)optk_gen_move_operations] &&
        option_kind_used[(int)optk_rvalue_ctor_is_copy_ctor]) {
      command_line_error(
                      ec_cl_gen_move_operations_and_rvalue_ctor_is_copy_ctor);
    } else if (!option_kind_used[(int)optk_rvalue_ctor_is_copy_ctor]) {
      rvalue_ctor_is_copy_ctor = FALSE;
    } else {
      generate_move_operations = FALSE;
    }  /* if */
  }  /* if */
}  /* check_rvalue_ref_options */


static void check_and_set_cplusplus_mode_options(void)
/*
This routine is called in C++ mode to check that no non-C++ command-line
setting is used, and to set various unmentioned settings as needed.
*/
{
  check_assertion(!C_mode());
  /* Update the default C++ standard version accounting for any compiler
     emulation and the version of the emulation. */
  check_and_set_default_cpp_standard_version();
  /* Reset the SVR4 C compatibility flag just in case it is set by
     default. */
  SVR4_C_mode = FALSE;
  /* Set global flags having to do with potential size of enum types. */
  enum_types_can_be_smaller_than_int =
                          targ_enum_types_can_be_smaller_than_int;
  enum_types_can_be_larger_than_int = TRUE;
  /* The default for --long_preserving_rules in C++ is FALSE. */
  if (!option_kind_used[(int)optk_long_preserving_rules]) {
    long_preserving_rules = FALSE;
  }  /* if */
#if VLA_ALLOWED
  if (!(option_kind_used[(int)optk_vla])) {
    /* Support for VLAs is turned off by default in C++ mode. */
    vla_enabled = FALSE;
  }  /* if */
#endif /* VLA_ALLOWED */
  /* VLA deallocations are implicit in C++ and tied to object lifetimes,
     so do not generate the explicit C-mode deallocations. */
  vla_deallocations_in_il = FALSE;
  designators_allowed = FALSE;
  /* If the designators option has not been explicitly specified and we are in 
     C++20 mode enable a restricted form of designated initializers. */
  if (cpp20_mode && !option_kind_used[(int)optk_designators]) {
    designators_allowed = TRUE;
    cpp20_designators_restriction = TRUE;
  }  /* if */
  if (option_kind_used[(int)optk_extended_designators]) {
    command_line_error(ec_cl_extended_designators_option_only_in_C);
  }  /* if */
  extended_designators_allowed = FALSE;
  if (option_kind_used[(int)optk_compound_literals]) {
    command_line_error(ec_cl_compound_literals_option_only_in_C);
  }  /* if */
  compound_literals_allowed = FALSE;
#if FIXED_POINT_ALLOWED
  /* Fixed-point types are not currently supported in C++ modes. */
  if (option_kind_used[(int)optk_fixed_point]) {
    command_line_error(ec_cl_fixed_point_option_only_in_C);
  }  /* if */
  fixed_point_enabled = FALSE;
#endif /* FIXED_POINT_ALLOWED */
#if NAMED_ADDRESS_SPACES_ALLOWED
  /* Named address spaces are not currently supported in C++ modes. */
  if (option_kind_used[(int)optk_named_address_spaces]) {
    command_line_error(ec_cl_named_address_spaces_option_only_in_C);
  }  /* if */
  named_address_spaces_enabled = FALSE;
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
#if NAMED_REGISTERS_ALLOWED
  /* Named registers are not currently supported in C++ modes. */
  if (option_kind_used[(int)optk_named_registers]) {
    command_line_error(ec_cl_named_registers_option_only_in_C);
  }  /* if */
  named_registers_enabled = FALSE;
#endif /* NAMED_REGISTERS_ALLOWED */
  if (option_kind_used[(int)optk_embedded_c]) {
    command_line_error(ec_cl_embedded_c_option_only_in_C);
  }  /* if */
  if (option_kind_used[(int)optk_c23_typeof]) {
    command_line_error(ec_cl_c23_typeof_option_only_in_C);
  }  /* if */
  /* "//" is allowed as a comment delimiter. */
  end_of_line_comments_allowed = TRUE;
  /* Universal character names are allowed. */
  universal_character_names_allowed = TRUE;
  elab_type_lookup_finds_typedefs = TRUE;
  if (!option_kind_used[(int)optk_variadic_templates] && !gpp_mode &&
      !ms_extensions && !sun_mode) {
    variadic_templates_enabled = DEFAULT_VARIADIC_TEMPLATES_ENABLED;
  }  /* if */
  if (cpp11_mode) {
    /* Enable C++11 extensions. */
    check_and_set_cpp11_mode_options(/*value=*/TRUE);
  } else if (option_kind_used[(int)optk_cpp03_mode]) {
    /* Disable all C++11 extensions. */
    check_and_set_cpp11_mode_options(/*value=*/FALSE);
  } else {
    /* Set default C++11 extensions. */
    check_and_set_default_cpp11_extensions();
  }  /* if */
  if (lambdas_enabled && !microsoft_mode) {
    /* Originally, the C++11 standard did not permit lambda expressions with
       default arguments.  The resolution for Core issue 974 changed that.
       Enable the feature in all modes that accept lambdas, except certain
       Microsoft modes (we enable the feature for specific Microsoft modes in
       set_microsoft_mode_flags). */
    lambda_default_args_enabled = TRUE;
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  if (option_kind_used[(int)optk_gnu_c89_inlining]) {
    command_line_error(ec_cl_gnu_c89_inlining_option_only_in_C);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (func_prototype_tags_enabled) {
    /* In C++, tags are never allowed in function prototype scopes.  Diagnose
       an explicit option attempting to make it otherwise. */
    if (option_kind_used[(int)optk_func_prototype_tags]) {
      command_line_error(ec_cl_func_prototype_tags_option_only_in_C);
    }  /* if */
    func_prototype_tags_enabled = FALSE;
  }  /* if */
  if (!require_func_prototypes) {
    /* In C++, non-prototype function declarations do not exist.  Diagnose an
       explicit option attempting to make it otherwise. */
    if (option_kind_used[(int)optk_require_func_prototypes]) {
      command_line_error(ec_cl_require_func_prototypes_option_only_in_C);
    }  /* if */
    require_func_prototypes = TRUE;
  }  /* if */
  if (!option_kind_used[(int)optk_relaxed_abstract_checking]) {
    relaxed_abstract_checking = TRUE;
  }  /* if */
  if (!option_kind_used[(int)optk_old_specializations]) {
    /* Require the "template<>" prefix for explicit specializations by default
       in C++11 mode. */
    old_specializations_allowed = !cpp11_mode;
  }  /* if */
  func_prototype_tags_enabled = FALSE;
}  /* check_and_set_cplusplus_mode_options */


static void exclude_cfront_mode(an_error_code  error_code)
/*
Cfront mode is incompatible with other settings.  Either issue the given
diagnostic (error_code) if the conflict is explicit, or silently turn off an
otherwise implicitly enabled cfront mode.
*/
{
  if (any_cfront_mode()) {
    if (option_kind_used[(int)optk_cfront_2_1_mode] ||
        option_kind_used[(int)optk_cfront_3_0_mode]) {
      /* cfront mode was enabled by a command line option. */
      command_line_error(error_code);
    } else {
      /* cfront mode enabled by default.  Silently disable it. */
      cfront_2_1_mode = FALSE;
      cfront_3_0_mode = FALSE;
    }  /* if */
  }  /* if */
}  /* exclude_cfront_mode */


/*lint -ecall(523,*exclude_microsoft_mode)*/
static void exclude_microsoft_mode(ARG_UNUSED an_error_code  error_code)
/*
Microsoft mode is incompatible with other settings.  Either issue the given
diagnostic (error_code) if the conflict is explicit, or silently turn off an
otherwise implicitly enabled Microsoft mode.
*/
{
#if MICROSOFT_EXTENSIONS_ALLOWED
  /* Microsoft mode is not compatible with another mode set on the command
     line. */
  if (microsoft_mode) {
    if (option_kind_used[(int)optk_microsoft_mode] ||
        (option_kind_used[(int)optk_microsoft_version] && !clang_mode) ||
        option_kind_used[(int)optk_microsoft_build_number] ||
#if NEAR_AND_FAR_ALLOWED
        option_kind_used[(int)optk_microsoft_16_mode] ||
#endif /* NEAR_AND_FAR_ALLOWED */
        option_kind_used[(int)optk_microsoft_bugs] ||
        option_kind_used[(int)optk_cppcli] ||
        option_kind_used[(int)optk_ms_permissive] ||
        option_kind_used[(int)optk_ms_rvalue_cast] ||
        option_kind_used[(int)optk_ms_strict_ternary] ||
        option_kind_used[(int)optk_ms_cplusplus_std_value] ||
        option_kind_used[(int)optk_ms_std_preproc]) {
      /* Microsoft mode was explicitly or implicitly enabled by a command-line
         option.  Allow setting microsoft_version in clang mode when
         DEFAULT_MICROSOFT_MODE is TRUE. */
      command_line_error(error_code);
    } else {
      /* Microsoft mode enabled by default.  Silently disable it since the
         explicit mode setting on the command line overrides it (unless
         explicit options were specified). */
      microsoft_mode = FALSE;
      if (!option_kind_used[(int)optk_microsoft_bugs]) {
        microsoft_bugs = FALSE;
      }  /* if */
      if (!option_kind_used[(int)optk_microsoft_extensions] &&
          !option_kind_used[(int)optk_microsoft_compatibility]) {
        ms_extensions = FALSE;
      }  /* if */
      if (!option_kind_used[(int)optk_microsoft_compatibility]) {
        ms_compat = FALSE;
      }  /* if */
      /* Make sure ms_extensions is set if ms_compat is. */
      check_assertion(!(ms_compat && !ms_extensions));
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* exclude_microsoft_mode */


static void exclude_sun_mode(ARG_UNUSED an_error_code  error_code)
/*
Sun mode is incompatible with other settings.  Either issue the given
diagnostic (error_code) if the conflict is explicit, or silently turn off
an otherwise implicitly enabled Sun mode.
*/
{
  if (sun_mode) {
#if SUN_EXTENSIONS_ALLOWED
    if (option_kind_used[(int)optk_sun_mode]) {
      /* Sun mode was enabled by a command line option. */
      command_line_error(error_code);
    } else {
      /* Sun mode was enabled by default.  Silently disable it since an
         explicit mode setting on the command line overrides it. */
      sun_mode = FALSE;
    }  /* if */
#else /* !SUN_EXTENSIONS_ALLOWED */
    /* If Sun extensions are disabled, sun_mode should be a FALSE constant. */
    unexpected_condition();
#endif /* SUN_EXTENSIONS_ALLOWED */
  }  /* if */
}  /* exclude_sun_mode */


static void exclude_SVR4_C_mode(an_error_code  error_code)
/*
SVR4-C mode is incompatible with other settings.  Either issue the given
diagnostic (error_code) if the conflict is explicit, or silently turn off
an otherwise implicitly enabled SVR4-C mode.
*/
{
  if (SVR4_C_mode) {
    if (option_kind_used[(int)optk_SVR4_C_mode]) {
      /* SVR4-C mode was enabled by a command line option. */
      command_line_error(error_code);
    } else {
      /* SVR4-C mode was enabled by default.  Silently disable it since an
         explicit mode setting on the command line overrides it. */
      SVR4_C_mode = FALSE;
    }  /* if */
  }  /* if */
}  /* exclude_SVR4_C_mode */


static void exclude_c99_mode(void)
/*
C99 mode is incompatible with other settings: Issue a command-line error if it
was enabled explicitly.  Since C11 mode implies C99 mode, also issue an error
if C11 mode was enabled explicitly.
*/
{
  if (c99_mode) {
    if (option_kind_used[(int)optk_c99_mode] ||
        option_kind_used[(int)optk_c11_mode] ||
        option_kind_used[(int)optk_c18_mode] ||
        option_kind_used[(int)optk_c23_mode]) {
      /* C99 mode was enabled by a command line option. */
      command_line_error(ec_cl_incompatible_language_modes);
    }  /* if */
  }  /* if */
}  /* exclude_c99_mode */


/*lint -esym(523,*exclude_gcc_mode)*/
static void exclude_gcc_mode(ARG_UNUSED an_error_code  error_code)
/*
GNU C mode is incompatible with other settings.  Either issue the given
diagnostic (error_code) if the conflict is explicit, or silently turn off
an otherwise implicitly enabled GNU C mode.
*/
{
#if GNU_EXTENSIONS_ALLOWED
  if (gcc_mode) {
    if (option_kind_used[(int)optk_gcc_mode] ||
        option_kind_used[(int)optk_gnu_version] ||
        option_kind_used[(int)optk_clang_mode] ||
        option_kind_used[(int)optk_clang_version]) {
      /* GNU C mode was enabled by a command line option. */
      command_line_error(error_code);
    } else {
      /* GNU C mode was enabled by default.  Silently disable it since an
         explicit mode setting on the command line overrides it. */
      gcc_mode = FALSE;
      gnu_mode = FALSE;
    }  /* if */
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
}  /* exclude_gcc_mode */


/*lint -esym(523,*exclude_gpp_mode)*/
static void exclude_gpp_mode(ARG_UNUSED an_error_code  error_code)
/*
GNU C++ mode is incompatible with other settings.  Either issue the given
diagnostic (error_code) if the conflict is explicit, or silently turn off
an otherwise implicitly enabled GNU C++ mode.
*/
{
#if GNU_EXTENSIONS_ALLOWED
  if (gpp_mode) {
    if (option_kind_used[(int)optk_gpp_mode] ||
        option_kind_used[(int)optk_gnu_version] ||
        option_kind_used[(int)optk_clang_mode] ||
        option_kind_used[(int)optk_clang_version]) {
      /* GNU C++ mode was enabled by a command line option. */
      command_line_error(error_code);
    } else {
      /* GNU C++ mode was enabled by default.  Silently disable it since an
         explicit mode setting on the command line overrides it. */
      gpp_mode = FALSE;
      gnu_mode = FALSE;
    }  /* if */
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
}  /* exclude_gpp_mode */


static void exclude_cpp_mode(void)
/*
C++ mode is incompatible with other settings.  Issue a command-line error if
the conflict is explicit.
*/
{
  if (cpp_mode_specified()) {
    /* C++ mode was enabled by a command line option. */
    command_line_error(ec_cl_incompatible_language_modes);
  }  /* if */
}  /* exclude_cpp_mode */


static void check_and_set_ansi_mode_options(void)
/*
Both for strict ANSI C and C++ modes, check that no command-line setting
conflicts with the ANSI mode and set various unmentioned settings as needed.
*/
{
#if NEAR_AND_FAR_ALLOWED
  /* If near and far were enabled by default, turn off support. */
  il_header.near_and_far_are_enabled = FALSE;
#endif /* NEAR_AND_FAR_ALLOWED */
  /* Strict ANSI mode is incompatible with allowing anachronisms. */
  if (allow_anachronisms) {
    if (option_kind_used[(int)optk_cplusplus_anachronisms]) {
      /* Anachronisms were enabled by a command line option. */
      command_line_error(ec_cl_strict_mode_incompatible_with_anachronisms);
    } else {
      /* Anachronisms enabled by default.  Silently disable them in
         strict mode. */
      allow_anachronisms = FALSE;
    }  /* if */
  }  /* if */
  if (allow_nonconst_ref_anachronism) {
    if (option_kind_used[(int)optk_nonconst_ref_anachronism]) {
      /* The nonconst ref anachronism was enabled by a command line
         option. */
      command_line_error(ec_cl_strict_mode_incompatible_with_anachronisms);
    } else {
      /* The nonconst ref anachronism was enabled by default.
         Silently disable it in strict mode. */
      allow_nonconst_ref_anachronism = FALSE;
    }  /* if */
  }  /* if */
  if (long_preserving_rules) {
    if (option_kind_used[(int)optk_long_preserving_rules]) {
      command_line_error(
                        ec_cl_strict_mode_incompatible_with_long_preserving);
    } else {
      /* Long preserving rules enabled by default.  Silently disable them. */
	long_preserving_rules = FALSE;
    }  /* if */
  }  /* if */
  if (!option_kind_used[(int)optk_trigraphs] && !cpp17_mode && !c23_mode) {
    /* Trigraphs should be allowed if not disabled by a command-line option,
       except in C++17 and C23 (and later revisions). */
    trigraphs_allowed = TRUE;
  }  /* if */
  if (!(option_kind_used[(int)optk_extended_designators])) {
    /* Support for extended designators is turned off by default in
       strict mode. */
    extended_designators_allowed = FALSE;
  }  /* if */
  if (!(option_kind_used[(int)optk_extended_variadic_macros])) {
    /* Support for extended variadic macros is turned off by default
       in strict mode. */
    extended_variadic_macros_allowed = FALSE;
  }  /* if */
  if (!option_kind_used[(int)optk_allow_dollar_in_id_chars]) {
    /* Support for dollar signs in identifiers is turned off by default in
       strict mode. */
    allow_dollar_in_id_chars = FALSE;
  }  /* if */
  if (!option_kind_used[(int)optk_uliterals] && !cpp11_mode && !c11_mode) {
    /* Support for U-literals (U... and u...) is off by default in strict
       mode (but not in strict C++11 or C11 mode). */
    uliterals_enabled = FALSE;
  }  /* if */
  if (!option_kind_used[(int)optk_check_concatenations]) {
    /* Macro concatenation ("a ## b") resulting in an invalid token is
       undefined behavior -- check for it and issue a diagnostic if it
       occurs. */
    check_concatenations = TRUE;
  }  /* if */
  if (C_mode() && !c99_mode) {
    /* In strict mode the final field of a struct may not be an incomplete
       array, except in strict C99 mode. */
    flexible_array_members_allowed = FALSE;
    if (!(option_kind_used[(int)optk_variadic_macros])) {
      /* Support for variadic macros is turned off by default except in
         strict C99 mode. */
      variadic_macros_allowed = FALSE;
    }  /* if */
    if (!(option_kind_used[(int)optk_restrict])) {
      /* Support for restricted pointers is turned off by default except
         in strict C99 mode. */
      restrict_keyword_enabled = FALSE;
    }  /* if */
  }  /* if */
  if (C_mode()) {
    /* Set optional features to standard settings for strict C mode. */
    if (!(option_kind_used[(int)optk_alternative_tokens])) {
      /* If alternative_tokens was not explicitly set by a command line
         option, set it now. */
      alternative_tokens_allowed = TRUE;
    }  /* if */
    /* Features enabled in C99 but not in older C are handled in
       check_and_set_new_c_mode_options. */
    if (!c99_mode) {
      /* Features listed here are those that can be turned on in pre-C99 C
         mode but not in C++ mode. */
#if VLA_ALLOWED
      if (!(option_kind_used[(int)optk_vla])) {
        /* Support for VLAs is turned off by default in strict C mode. */
        vla_enabled = FALSE;
      }  /* if */
#endif /* VLA_ALLOWED */
      if (!(option_kind_used[(int)optk_designators])) {
        /* Support for designators is turned off by default in strict C
           mode. */
        designators_allowed = FALSE;
      }  /* if */
      if (!(option_kind_used[(int)optk_compound_literals])) {
        /* Support for compound literals is turned off by default in strict
           C mode. */
        compound_literals_allowed = FALSE;
      }  /* if */
    }  /* if */
  } else {
    /* Set optional features to standard settings for strict C++ mode. */
    single_ref_qual_ovl_res_tiebreaker = FALSE;
    floating_point_template_parameters_allowed = cpp20_mode;
    no_access_check_on_friend_declarator_ids = FALSE;
    if (!(option_kind_used[(int)optk_alternative_tokens])) {
      /* If alternative_tokens was not explicitly set by a command line
         option, set it now. */
      alternative_tokens_allowed = TRUE;
    }  /* if */
    if (!(option_kind_used[(int)optk_wchar_t_is_keyword])) {
      /* If wchar_t_is_keyword was not explicitly set by a command line
         option, set it now. */
      wchar_t_is_keyword = WCHAR_T_ENABLING_POSSIBLE;
    }  /* if */
    if (!(option_kind_used[(int)optk_bool_is_keyword])) {
      /* If bool_is_keyword was not explicitly set by a command line
         option, set it now. */
      bool_is_keyword = BOOL_ENABLING_POSSIBLE;
    }  /* if */
    if (!(option_kind_used[(int)optk_long_lifetime_temps])) {
      /* Temporary lifetime is short. */
      long_lifetime_temps = FALSE;
    }  /* if */
    if (!(option_kind_used[(int)optk_rtti])) {
      /* If rtti_enabled was not explicitly set by a command line
         option, set it now. */
      rtti_enabled = RTTI_ENABLING_POSSIBLE;
    }  /* if */
    if (!(option_kind_used[(int)optk_array_new_and_delete])) {
      /* If array_new_and_delete_enabled was not explicitly set by a
         command line option, set it now. */
      array_new_and_delete_enabled = ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE;
    }  /* if */
    if (!(option_kind_used[(int)optk_explicit])) {
      /* If explicit_keyword_enabled was not explicitly set by a command
         line option, set it now. */
      explicit_keyword_enabled = TRUE;
    }  /* if */
    if (!(option_kind_used[(int)optk_namespaces])) {
      /* If namespaces_enabled was not explicitly set by a command line
         option, set it now. */
      namespaces_enabled = TRUE;
    }  /* if */
    if (!(option_kind_used[(int)optk_implicit_typename])) {
      /* If implicit_typename was not explicitly set by a command line
         option, set it now. */
      implicit_typename_enabled = FALSE;
    }  /* if */
    if (!(option_kind_used[(int)optk_typename])) {
      /* If typename_enabled was not explicitly set by a command line
         option, set it now. */
      typename_enabled = TRUE;
    }  /* if */
    if (!(option_kind_used[(int)optk_special_subscript_cost])) {
      /* If special_subscript_cost was not explicitly set by a command line
         option, turn it off now. */
      special_subscript_cost = FALSE;
    }  /* if */
    if (!(option_kind_used[(int)optk_old_for_init])) {
      /* If old/new_for_init was not specified on the command line, turn
         off use_nonstandard_for_init_scope now. */
      use_nonstandard_for_init_scope = FALSE;
    }  /* if */
    if (!(option_kind_used[(int)optk_for_init_diff_warning])) {
      /* If for_init_diff_warning was not specified on the command line, turn
         off warning_on_for_init_difference now. */
      warning_on_for_init_difference = FALSE;
    }  /* if */
    if (!(option_kind_used[(int)optk_guiding_decls])) {
      /* If guiding_decls_allowed was not set on the command line, turn it
         off now. */
      guiding_decls_allowed = FALSE;
    }  /* if */
    if (!(option_kind_used[(int)optk_old_specializations])) {
      /* If old_specializations_allowed was not set on the command line,
         turn it off now. */
      old_specializations_allowed = FALSE;
    }  /* if */
#if IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE
    if (!(option_kind_used[(int)optk_implicit_extern_c_type_conversion])) {
      /* If impl_conv_between_c_and_cpp_function_ptrs_allowed was not set
         on the command line, turn it off now. */
      impl_conv_between_c_and_cpp_function_ptrs_allowed = FALSE;
    }  /* if */
#else /* !IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE */
    /* Set it FALSE in case the implicit conversion between extern "C" and
       extern "C++" function pointers is allowed by default. */
    impl_conv_between_c_and_cpp_function_ptrs_allowed = FALSE;
#endif /* IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE */
    if (!(option_kind_used[(int)optk_extern_inline])) {
      /* If extern_inline_allowed was not explicitly set by a command line
         option, set it now. */
      extern_inline_allowed = TRUE;
    }  /* if */
    if (!(option_kind_used[(int)optk_enum_overloading])) {
      /* If enum_overloading was not explicitly set by a command line
         option, set it now. */
      operator_overloading_on_enums_enabled = TRUE;
    }  /* if */
    if (!(option_kind_used[(int)optk_const_string_literals])) {
      /* If string_literals_are_const was not explicitly set by a
         command line option, set it now. */
      string_literals_are_const = TRUE;
    }  /* if */
    if (!option_kind_used[(int)optk_deprecated_string_conv]) {
      deprecated_string_literal_conv_allowed = !cpp11_mode;
    }  /* if */
    assume_references_cannot_be_null = TRUE;
    if (!(option_kind_used[(int)optk_class_name_injection])) {
      /* If class name injection was not explicitly set by a command
         line option, set it now. */
      class_name_injection_enabled = TRUE;
    }  /* if */
    if (!(option_kind_used[(int)optk_arg_dependent_lookup])) {
      /* If argument dependent lookup not explicitly set by a command
         line option, set it now. */
      arg_dependent_lookup_enabled = TRUE;
    }  /* if */
    if (!(option_kind_used[(int)optk_friend_injection])) {
      /* If friend injection was not explicitly set by a command line
         option, set it now. */
      friend_class_injection_enabled = FALSE;
      friend_function_injection_enabled = FALSE;
    }  /* if */
    if (!(option_kind_used[(int)optk_dependent_name_processing])) {
      /* If dependent name processing was not explicitly set by a command line
         option, set it now. */
      do_dependent_name_processing = TRUE;
    }  /* if */
    /* C++03 dependent lookup did not find static functions.  C++11 changed
       that. */
    dependent_lookup_finds_static_functions = cpp11_mode;
    if (!(option_kind_used[(int)optk_parse_nonclass_templates])) {
      /* If prototype instantiation of nonclasses was not explicitly set by a
         command line option, set it now. */
      nonclass_prototype_instantiations = TRUE;
    }  /* if */
    if (!(option_kind_used[(int)optk_export_template])) {
      /* If export template processing was not explicitly set by a command line
         option, set it now. */
      export_template_allowed = /*lint -e(506)*/EXPORT_ENABLING_POSSIBLE &&
                                !cpp11_mode;
    }  /* if */
    if (!(option_kind_used[(int)optk_nonstandard_using_decl])) {
      /* If nonstandard using-decl was not explicitly set by a command line
         option, set it now. */
      nonstandard_using_decl_allowed = FALSE;
    }  /* if */
    if (!(option_kind_used[(int)optk_nonstandard_qualifier_deduction])) {
      /* If nonstandard_qualifier_deduction was not set on the command line,
         turn it off now. */
      nonstandard_qualifier_deduction = FALSE;
    }  /* if */
    if (!option_kind_used[(int)optk_nonstandard_default_arg_deduction]) {
      /* If nonstandard_default_arg_deduction was not set on the command line,
         turn it off now. */
      nonstandard_default_arg_deduction = FALSE;
    }  /* if */
    if (!option_kind_used[(int)optk_nonstandard_instantiation_lookup]) {
      /* If nonstandard instantiation lookup was not set on the command line,
         turn it off now. */
      nonstandard_instantiation_lookup_enabled = FALSE;
    }  /* if */
    if (!(option_kind_used[(int)optk_late_tiebreaker])) {
      /* If late tiebreaker was not explicitly set by a command line
         option, force it off. */
      do_late_ovl_res_tiebreaker = FALSE;
    }  /* if */
    if (!option_kind_used[(int)optk_base_assign_op_is_default]) {
      allow_copy_assignment_op_with_base_class_param = FALSE;
    }  /* if */
    if (!option_kind_used[(int)optk_exception_handling]) {
      exceptions_enabled = TRUE;
    }  /* if */
    if (!option_kind_used[(int)optk_cpp11_sfinae] &&
        !cpp11_mode) {
      cpp11_sfinae_enabled = FALSE;
      cpp11_sfinae_ignore_access = FALSE;
    }  /* if */
    if (ignore_std_namespace) {
      /*  An option to treat namespace std as an alias for the global
          namespace is contradictory to strict ANSI adherence.  Issue an
          error if this was explicitly requested on the command line, or
          silently ignore the option otherwise. */
      if (option_kind_used[(int)optk_ignore_namespace_std]) {
        command_line_error(ec_cl_strict_mode_incompatible_with_ignore_std);
      
      } else {
        ignore_std_namespace = FALSE;
      }  /* if */
    }  /* if */
  }  /* if */
  /* This should be FALSE in C mode and all strict C++ modes. */
  inexact_ptr_to_member_deduction_enabled = FALSE;
  if (!option_kind_used[(int)optk_nonstd_anonymous_unions]) {
    allow_nonstandard_anonymous_unions = FALSE;
  }  /* if */
}  /* check_and_set_ansi_mode_options */


static void check_and_set_sun_mode_options(void)
/*
Set the option needed to emulate the peculiarities of the Sun CC 5.x compiler,
and check that no other modes conflict with this one.  (The processing of
some modes, like ANSI, exclude the Sun mode already.  Hence those are not
checked again here.)
*/
{
  if (!option_kind_used[(int)optk_exception_handling]) {
    exceptions_enabled = TRUE;
  }  /* if */
  if (!option_kind_used[(int)optk_guiding_decls]) {
    /* If guiding_decls_allowed was not set on the command line, turn it
       off now. */
    guiding_decls_allowed = FALSE;
  }  /* if */
  /* Sun compilers use a template instantiation model close to that of Cfront:
     No dependent name processing, no prototype instantiations, and "typename"
     is implicit in many cases.  (What's more, their standard headers rely on
     that behavior.) */
  if (!option_kind_used[(int)optk_dependent_name_processing]) {
    do_dependent_name_processing = FALSE;
  }  /* if */
  if (!option_kind_used[(int)optk_parse_nonclass_templates]) {
    nonclass_prototype_instantiations = FALSE;
  }  /* if */
  dependent_lookup_finds_static_functions = FALSE;
  if (!option_kind_used[(int)optk_implicit_typename]) {
    implicit_typename_enabled = TRUE;
  }  /* if */
  if (!(option_kind_used[(int)optk_nonstandard_using_decl])) {
    /* If nonstandard using-decl was not explicitly set by a command line
       option, set it now. */
    nonstandard_using_decl_allowed = TRUE;
  }  /* if */
  if (!(option_kind_used[(int)optk_extern_inline])) {
    /* If extern_inline_allowed was not explicitly set by a command line
       option, turn it on now. */
    extern_inline_allowed = TRUE;
  }  /* if */
#if THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
  if (!(option_kind_used[(int)optk_thread_local_storage])) {
    /* Support for "__thread" is turned on by default in Sun mode. */
    thread_local_storage_specifier_enabled = TRUE;
  }  /* if */
#endif /* THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED */
#if SUN_EXTENSIONS_ALLOWED
  if (!(option_kind_used[(int)optk_sun_linker_scope])) {
    /* If the Sun linker scope option was not set on the command line, set
       its value now based on the configuration macros. */
    sun_linker_scope_allowed = DEFAULT_SUN_LINKER_SCOPE_ALLOWED;
  }  /* if */
#endif /* SUN_EXTENSIONS_ALLOWED */
  if (!(option_kind_used[(int)optk_trigraphs])) {
    /* Trigraphs should be allowed if not disabled by a command-line option. */
    trigraphs_allowed = TRUE;
  }  /* if */
  if (!option_kind_used[(int)optk_nonstandard_default_arg_deduction]) {
    /* Default arguments are part of the deduced function type in Sun mode. */
    nonstandard_default_arg_deduction = TRUE;
  }  /* if */
  /* The Sun compiler suffers from the same problem as the Microsoft
     compiler with respect to making template parameters visible in
     specializations. */
  use_microsoft_specialization_scope = TRUE;
  allow_default_arg_on_template_member_definition = TRUE;
  type_keyword_in_dtor_allowed = TRUE;
  if (!option_kind_used[(int)optk_type_traits_helpers]) {
    type_traits_helpers_enabled = FALSE;
  }  /* if */
  if (!option_kind_used[(int)optk_nonstandard_instantiation_lookup]) {
    /* If nonstandard instantiation lookup was not set on the command line,
       turn it off now. */
    nonstandard_instantiation_lookup_enabled = FALSE;
  }  /* if */
  if (!option_kind_used[(int)optk_const_string_literals]) {
    string_literals_are_const = TRUE;
  }  /* if */
  if (!option_kind_used[(int)optk_deprecated_string_conv]) {
    deprecated_string_literal_conv_allowed = TRUE;
  }  /* if */
  if (!(option_kind_used[(int)optk_nonconst_ref_anachronism])) {
    /* Versions 5.3, 5.5 and 5.8 (at least) of the Sun compiler allow this
       particular anachronism. */
    allow_nonconst_ref_anachronism = TRUE;
  }  /* if */
  va_arg_returns_lvalue = TRUE;
  if (!(option_kind_used[(int)optk_variadic_macros])) {
    /* The Sun compiler accepts variadic and (as of Studio 12 update 1)
       extended variadic macros. */
    variadic_macros_allowed = TRUE;
    extended_variadic_macros_allowed = TRUE;
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  /* Recent Sun compilers accept some GNU attributes. */
  gnu_attributes_enabled = TRUE;
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (!option_kind_used[(int)optk_implicit_noexcept]) {
    /* Keep the traditional relaxed semantics for destructors and
       operator delete. */
    implicit_noexcept_enabled = FALSE;
  }  /* if */
  if (!option_kind_used[(int)optk_preserve_lvalues_with_same_type_casts]) {
    preserve_lvalues_with_same_type_casts = TRUE;
  }  /* if */
}  /* check_and_set_sun_mode_options */


static void check_and_set_gnu_mode_options(void)
/*
Set the options common to both GNU C and C++ modes, making sure that no other
options conflict with them.  (The processing of some modes, like ANSI,
exclude the GNU modes already.  Hence those are not checked again here.)
This function is also called in clang mode.
*/
{
#if GNU_EXTENSIONS_ALLOWED
  if (clang_mode && !option_kind_used[(int)optk_gnu_version] &&
      gnu_version < 40800) {
    /* The feature set of clang mode corresponds best to gcc 4.8. */
    gnu_version = 40800;
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if TIE_DEFAULT_GNU_ABI_VERSION_TO_GNU_VERSION
  /* This is a configuration that emulates recent GNU C/C++ ABIs and the
     specific ABI version should match the version of the GNU dialect being
     emulated. */
  gnu_abi_version = gnu_version;
  check_assertion(gnu_abi_version >= 30200);
#endif /* TIE_DEFAULT_GNU_ABI_VERSION_TO_GNU_VERSION */
  if (!(option_kind_used[(int)optk_extended_designators])) {
    /* If extended designators were not enabled or disabled on the command
       line, enable them now. */
    designators_allowed = TRUE;
    extended_designators_allowed = TRUE;
  }  /* if */
#if COMPOUND_LITERAL_ENABLING_POSSIBLE
  if (!(option_kind_used[(int)optk_compound_literals])) {
    /* If compound literals were not enabled or disabled on the command line,
       enable them now. */
    compound_literals_allowed = TRUE;
  }  /* if */
#endif /* COMPOUND_LITERAL_ENABLING_POSSIBLE */
  if (!(option_kind_used[(int)optk_extended_variadic_macros])) {
    /* If extended variadic macros were not enabled or disabled on the command
       line, enable them now. */
    variadic_macros_allowed = TRUE;
    extended_variadic_macros_allowed = TRUE;
  }  /* if */
  pragma_operator_allowed = TRUE;
  if (!(option_kind_used[(int)optk_allow_dollar_in_id_chars])) {
    /* If identifiers with dollar signs were not enabled or disabled on the
       command line, enable them now. */
    allow_dollar_in_id_chars = TRUE;
  }  /* if */
  /* <stdarg.h> should always be included as a normal header file, because
     the GNU version of <stdarg.h> contains definitions not available in our
     builtin stdarg processing (see proc_stdarg_include).  Various
     __builtin_... entities may be predefined to accommodate it (if
     GCC_BUILTIN_VARARGS is TRUE). */
  pass_stdarg_references_to_generated_code = FALSE;
  va_arg_returns_lvalue = FALSE;
  /* Enable the use of __restrict__ in GNU mode. */
  gnu_restrict_keyword_enabled = TRUE;
  /* Enable flexible array member support. */
  flexible_array_members_allowed = TRUE;
  /* Enable // comments. */
  end_of_line_comments_allowed = TRUE;
  if (!(option_kind_used[(int)optk_alternative_tokens])) {
    /* Enable recognition of digraphs. */
    alternative_tokens_allowed = TRUE;
  }  /* if */
  if (!(option_kind_used[(int)optk_trigraphs])) {
    /* Disable trigraphs. */
    trigraphs_allowed = FALSE;
  }  /* if */
#if THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
  if (!(option_kind_used[(int)optk_thread_local_storage])) {
    /* Support for "__thread" is turned on by default when emulating GNU C/C++
       versions 3.3 and higher. */
    thread_local_storage_specifier_enabled = (gnu_version >= 30300);
  }  /* if */
#endif /* THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED */
  /* Treat "long long" as a standard feature. */
  long_long_is_standard = TRUE;
  if (c99_mode ||
      (cpp11_mode && gnu_version >= 40700)) {
    /* If we're emulating gcc's -std=c99 mode or, beginning with gcc
       version 4.7, -std=c++11 mode, allow promotion to long long. */
    long_long_promotion_allowed = TRUE;
  } else {
    long_long_promotion_allowed = FALSE;
  }  /* if */
  /* Hexadecimal floating point constants are permitted. */
  hex_floating_point_constants_allowed = TRUE;
  /* Binary literals are allowed for gnu_version 40300 and above. */
  binary_literals_allowed = gnu_version >= 40300;
  null_chars_allowed_in_source = TRUE;
  allow_nonstandard_anonymous_unions = TRUE;
  /* In some configurations, special processing is done for references
     to __STDC__ in system header files. */
  if (!(option_kind_used[(int)optk_stdc_zero_in_system_headers])) {
    stdc_zero_in_system_headers =
                     /*lint -e(506)*/DEFAULT_GNU_STDC_ZERO_IN_SYSTEM_HEADERS ||
                     /*lint -e(506)*/DEFAULT_STDC_ZERO_IN_SYSTEM_HEADERS;
  }  /* if */
  mixed_string_concat_enabled = TRUE;
  if (!option_kind_used[(int)optk_check_concatenations]) {
    /* The GNU preprocessor disallows macro concatenation ("a ## b") that
       results in an invalid token. */
    check_concatenations = TRUE;
  }  /* if */
#if GNU_VECTOR_TYPES_ALLOWED
  permissive_gnu_vector_conversions_enabled = (gnu_version >= 40000 &&
                                               gnu_version < 40300);
#endif /* GNU_VECTOR_TYPES_ALLOWED */
  gnu_attributes_enabled = TRUE;
#if GNU_EXTENSIONS_ALLOWED
  if (!option_kind_used[(int)optk_nonstd_gnu_keywords]) {
    nonstd_gnu_keywords_enabled = TRUE;
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if INT128_EXTENSIONS_ALLOWED
  int128_extensions_enabled = TRUE;
#endif /* INT128_EXTENSIONS_ALLOWED */
  carriage_return_is_line_terminator =
                                    ACCEPT_GNU_CARRIAGE_RETURN_LINE_TERMINATOR;
#if GNU_EXTENSIONS_ALLOWED
  if (!option_kind_used[(int)optk_clang_mode] &&
      !option_kind_used[(int)optk_clang_version]) {
    clang_mode = DEFAULT_CLANG_COMPATIBILITY;
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (gnu_version < 40000) {
    /* GCC versions prior to 4.0 don't treat any operations applied to bit
       fields in any special way wrt. promotion. */
    bit_field_promotion_applies_to_some_operations = FALSE;
  }  /* if */
  if (clang_mode) {
    if (clang_version >= 30300) {
      noreturn_keyword_enabled = TRUE;
    }  /* if */
    if (clang_version >= 30700) {
      nullability_qualifiers_enabled = TRUE;
    }  /* if */
    /* Clang 3.9 added support for __float128, but not __float80. */
    float80_enabled = FALSE;
    if (clang_version < 30900) {
      float128_enabled = FALSE;
    }  /* if */
    if (clang_version >= 60000) {
      float16_enabled = TRUE;
    }  /* if */
    if (clang_version >= 120000) {
      /* As of clang 12.0.0, __VA_OPT__ is supported in both C and C++. */
      va_opt_enabled = TRUE;
    }  /* if */
    if (clang_version >= 130000) {
      /* As of version 13.0.0, clang supports #elifdef/#elifndef. */
      elifdef_enabled = TRUE;
      lambda_attributes_allowed = TRUE;
    }  /* if */
    if (clang_version < 140000 && !option_kind_used[(int)optk_old_id_chars]) {
      /* Clang implemented Unicode-based identifier characters in version
         14.0.0. */
      old_id_chars = TRUE;
    }  /* if */
    if (clang_version >= 140000) {
      /* As of version 14, clang accepts delimited escape sequences in all
         language versions. */
      delimited_escape_seqs_allowed = TRUE;
    }  /* if */
    if (clang_version >= 150000) {
      /* As of version 15, clang accepts named Unicode characters in all
         language versions. */
      named_unicode_chars_allowed = TRUE;
    }  /* if */
    if (clang_version >= 160000) {
      /* As of version 16, clang can evaluate some local static variables in
         constexpr/consteval functions. */
      local_static_constexpr_enabled = TRUE;
    }  /* if */
    if (clang_version >= 190000) {
      /* As of version 19, clang supports #embed. */
      embed_enabled = TRUE;
    }  /* if */
  } else {
#if USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES
    /* GNU produces wrappers only for dynamically-initialized thread_local
       variables; clang provides wrappers for all. */
    all_thread_locals_have_wrappers = FALSE;
#endif /* USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES */
    if (gnu_version < 30400) {
      /* GCC has supported __float80 and __float128 since GCC 3.4. */
      float80_enabled = FALSE;
      float128_enabled = FALSE;
    }  /* if */
    if (gnu_version >= 80000)  {
      /* Beginning with version 8.1.0, the __VA_OPT__ preprocessor operator
         (a C++20 feature) is accepted by gcc in both C and C++ modes in
         all standard versions. */
      va_opt_enabled = TRUE;
    }  /* if */
    if (gnu_version >= 100000) {
      lambda_attributes_allowed = TRUE;
    }  /* if */
    if (gnu_version >= 120000) {
      /* As of version 12.1.0, gcc supports #elifdef/#elifndef. */
      elifdef_enabled = TRUE;
    }  /* if */
    if (!option_kind_used[(int)optk_old_id_chars]) {
      /* gcc has not (as of version 12.2) implemented Unicode-based
         identifier characters. */
      old_id_chars = TRUE;
    }  /* if */
    if (gnu_version >= 130000) {
      /* As of version 13.1, g++ accepts delimited escape sequences in all
         language versions. */
      delimited_escape_seqs_allowed = TRUE;
      /* As of version 13.1, g++ accepts named Unicode characters in all
         language versions. */
      named_unicode_chars_allowed = TRUE;
    }  /* if */
    if (gnu_version >= 150000) {
      /* As of version 15.1, gcc accepts #embed in all language versions. */
      embed_enabled = TRUE;
    }  /* if */
  }  /* if */
}  /* check_and_set_gnu_mode_options */


static void check_and_set_gcc_mode_options(void)
/*
Set the options needed to emulate GNU C compilers, and check that no other
modes conflict with this one.  (The processing of some modes, like ANSI,
exclude the GNU C mode already.  Hence those are not checked again here.)
This function is also called in clang mode.
*/
{
  check_and_set_gnu_mode_options();
#if VLA_ALLOWED
  if (!(option_kind_used[(int)optk_vla])) {
    /* Support for VLAs is turned on by default in gcc mode. */
    vla_enabled = TRUE;
  }  /* if */
#endif /* VLA_ALLOWED */
  /* The underlying type for an enum could be long long. */
  enum_types_can_be_larger_than_int = TRUE;
  /* Allow nonconstant expressions in aggregate initializers for automatic
     variables. */
  allow_nonconstant_auto_aggr_init_in_c_mode = TRUE;
  allow_decl_after_stmt = TRUE;
  /* GNU's C89 conventions for the inline keyword are the opposite of those
     later standardized in C99.  GCC held onto its conventions in C99 mode also
     until GCC 4.3 (where an option exists to revert to the GNU C89 rules). */
  if (c99_mode &&
#if GNU_EXTENSIONS_ALLOWED
      !option_kind_used[(int)optk_gnu_c89_inlining] &&
#endif /* GNU_EXTENSIONS_ALLOWED */
      gnu_version >= 40300) {
    std_c99_inlining = TRUE;
    gnu_c89_inlining = FALSE;
  } else {
    std_c99_inlining = FALSE;
    gnu_c89_inlining = TRUE;
#if GNU_EXTENSIONS_ALLOWED
    il_header.gnu_c89_inlining = TRUE;
#endif /* GNU_EXTENSIONS_ALLOWED */
  }  /* if */
  c99_bool_is_keyword = TRUE;
  if (gnu_version_is(>=40700)) {
    alignof_enabled = TRUE;
    alignas_enabled = TRUE;
  }  /* if */
  if (clang_mode) {
    enumerator_attributes_enabled = TRUE;
    if (clang_version >= 30100) {
      /* Clang enables _Atomic support in all C modes. */
      c11_atomic_enabled = TRUE;
      std_thread_local_storage_specifier_enabled = TRUE;
    }  /* if */
    if (clang_version >= 30300) {
      noreturn_keyword_enabled = TRUE;
    }  /* if */
    if (clang_version >= 30500) {
      terse_static_assert_enabled = TRUE;
    }  /* if */
    if (clang_version >= 80000) {
      /* Clang 8.0 (and later) accepts the C23 feature enabling explicit
         underlying types for enumeration types. */
      explicit_enum_base_enabled = TRUE;
      opaque_enum_decls_enabled = TRUE;
    }  /* if */
    if (clang_version >= 170000) {
      std_attributes_enabled = TRUE;
    }  /* if */
  } else {
    /* GCC (not Clang) mode. */
    if (gnu_version >= 40600) {
      static_assert_enabled = TRUE;
    }  /* if */
    if (gnu_version >= 40700) {
      noreturn_keyword_enabled = TRUE;
    }  /* if */
    if (gnu_version >= 40900) {
      std_thread_local_storage_specifier_enabled = TRUE;
      c11_atomic_enabled = TRUE;
    }  /* if */
    if (gnu_version >= 60000) {
      enumerator_attributes_enabled = TRUE;
    }  /* if */
    if (gnu_version >= 70000) {
      float16_enabled = TRUE;
    }  /* if */
    if (gnu_version >= 90000) {
      terse_static_assert_enabled = TRUE;
    }  /* if */
    if (gnu_version >= 100000) {
      std_attributes_enabled = TRUE;
      nodiscard_attribute_enabled = TRUE;
      enumerator_attributes_enabled = TRUE;
    }  /* if */
    if (gnu_version >= 130000) {
      /* GCC 13 (and later) accepts the C23 feature enabling explicit
         underlying types for enumeration types. */
      explicit_enum_base_enabled = TRUE;
      opaque_enum_decls_enabled = TRUE;
    }  /* if */
  }  /* if */
}  /* check_and_set_gcc_mode_options */


static void check_and_set_gpp_mode_options(void)
/*
Set the options needed to emulate GNU C++ compilers, and check that no other
modes conflict with this one.  (The processing of some modes, like ANSI,
exclude the GNU C++ mode already.  Hence those are not checked again here.)
This function is also called in clang mode.  Note that when gnu_version >=
60000, the default C++ mode is to enable C++14 features, but that is set
before this routine is called.
*/
{
  check_and_set_gnu_mode_options();
  if (!option_kind_used[(int)optk_exception_handling]) {
    /* Enable exceptions by default in GNU C++ mode. */
    exceptions_enabled = TRUE;
  }  /* if */
  if (!option_kind_used[(int)optk_extern_inline]) {
    extern_inline_allowed = TRUE;
  }  /* if */
  /* Enable recognition of digraphs. */
  if (!(option_kind_used[(int)optk_alternative_tokens])) {
    alternative_tokens_allowed = TRUE;
  }  /* if */
  if (!option_kind_used[(int)optk_old_specializations]) {
    /* Explicit specializations can omit the "template<>" prefix in GCC 3.3.x
       and earlier. */
    old_specializations_allowed = gpp_version_is(<30400);
  }  /* if */
  if (gnu_version >= 30400) {
    /* Version 3.4 of GNU C++ introduces standard parsing for templates. */
    if (!(option_kind_used[(int)optk_dependent_name_processing])) {
      /* If dependent name processing was not explicitly set by a command line
         option, set it now. */
      do_dependent_name_processing = TRUE;
    }  /* if */
    if (!(option_kind_used[(int)optk_parse_nonclass_templates])) {
      /* If prototype instantiation of nonclasses was not explicitly set by a
         command line option, set it now. */
      nonclass_prototype_instantiations = TRUE;
    }  /* if */
  }  /* if */
  if (!option_kind_used[(int)optk_nonstandard_default_arg_deduction]) {
    /* Default arguments are part of the deduced function type in g++ mode
       prior to 4.3. */
    nonstandard_default_arg_deduction = (gnu_version < 40300);
  }  /* if */
  if (!option_kind_used[(int)optk_nonstandard_instantiation_lookup]) {
    /* If nonstandard instantiation lookup was not set on the command line,
       turn it off now. */
    nonstandard_instantiation_lookup_enabled = FALSE;
  }  /* if */
  if (!option_kind_used[(int)optk_friend_injection]) {
    /* g++ versions prior to 4.0.1 do injection of friend classes.  Versions
       prior to 4.1 do friend function injection. */
    friend_class_injection_enabled = gnu_version < 40001;
    friend_function_injection_enabled = gnu_version < 40100;
  }  /* if */
#if VLA_ALLOWED
  if (!(option_kind_used[(int)optk_vla])) {
    /* Support for VLAs is turned on by default in g++ mode. */
    vla_enabled = TRUE;
  }  /* if */
#endif /* VLA_ALLOWED */
  /* Even though g++ version 3.4 is more standard conforming with respect to
     name lookup in templates, many of the idiosyncrasies of earlier g++
     versions are still present in g++ 3.4.  Some of the tests of
     gpp_dependent_name_lookup also test gnu_version in cases where 3.4
     has fixed a lookup problem present in earlier versions. */
  gpp_dependent_name_lookup = TRUE;
  /* Earlier versions of GCC use special rules for determining which
     using-directives should be visible during template instantiations. */
  gpp_using_directive_lookup = gnu_version < 120000;
  /* g++ 4.3.x and earlier do not make parameters visible in their own
     function prototype scope.  (Later versions still keep them invisible
     in default argument expressions, but not in other contexts.)  We disable
     the emulation of that feature also if trailing return types or lambdas
     (which include trailing return type syntax) are enabled since it is not
     that unusual for a trailing return type to refer to a parameter. */
  parameters_visible_late = gnu_version < 40400 &&
                            !(trailing_return_types_enabled ||
                              lambdas_enabled);
  /* Some versions of g++ allow a namespace and class with the same name
     to be declared in a scope. */
  gnu_namespace_and_class_in_same_scope = (gnu_version < 40300);
  /* A friend class declaration finds names made visible by
     using-directives. */
  friend_class_decl_can_find_using_dir = TRUE;
  /* We will presumably want to pick std::type_info from the GNU headers.
     In that case, we cannot expect an EDG-specific pragma. */
  pragma_define_type_info_is_required = FALSE;
  /* Guiding declarations should be disabled in g++ mode. */
  if (!option_kind_used[(int)optk_guiding_decls]) {
    guiding_decls_allowed = FALSE;
  }  /* if */
  if (!(option_kind_used[(int)optk_const_string_literals])) {
    string_literals_are_const = TRUE;
  }  /* if */
  if (!option_kind_used[(int)optk_deprecated_string_conv]) {
    deprecated_string_literal_conv_allowed = TRUE;
  }  /* if */
  if (!cpp11_mode && !clang_mode && gnu_version >= 40300) {
    /* GCC accepts list initializers with a warning in pre-C++11 modes. */
    list_init_enabled = TRUE;
    pre_cpp11_list_init = TRUE;
  }  /* if */
  if (!option_kind_used[(int)optk_user_defined_literals]) {
    user_defined_literals_enabled = (cpp11_mode && gnu_version >= 40700);
  }  /* if */
  string_literal_operator_template_allowed = (user_defined_literals_enabled &&
                                              ((cpp14_mode &&
                                                gnu_version_is(>= 40900)) ||
                                               (cpp11_mode &&
                                                clang_version_is(>= 30400))));
  macro_preempts_udl_suffix = (gnu_version >= 40800 && !clang_mode);
  if (!option_kind_used[(int)optk_type_traits_helpers]) {
    /* g++ supports type traits in versions 4.3 and later.  Earlier versions
       use those identifiers in system headers, so type_traits_helpers_enabled
       should be FALSE when gnu_version < 40300 (even if the associated macro
       DEFAULT_TYPE_TRAITS_HELPERS_ENABLED is TRUE). */
    type_traits_helpers_enabled = (gnu_version >= 40300);
  }  /* if */
  c_and_cpp_function_types_are_distinct = FALSE;
  allow_default_arg_on_template_member_definition = TRUE;
  if (gnu_version_is(<110000) || clang_version_is(<180000) || !cpp20_mode) {
    /* GNU 11 and Clang 18 support floating-point template parameters, but only
       in C++20 mode. */
    floating_point_template_parameters_allowed = FALSE;
  }  /* if */
  equiv_typedefs_are_lookup_equivalent = FALSE;
  type_keyword_in_dtor_allowed = (gnu_version <= 30300);
  /* Early GNU C++ compilers do not check accessibility of friend function
     declarations. */
  no_access_check_on_friend_declarator_ids = (gnu_version < 30400);
  extern_template_allowed = TRUE;
  inline_template_allowed = TRUE;
  assume_references_cannot_be_null = FALSE;
#if DO_IL_LOWERING
  assume_this_cannot_be_null_in_conditional_operators = FALSE;
#endif /* DO_IL_LOWERING */
  if (!option_kind_used[(int)optk_cpp11_sfinae] &&
      !option_kind_used[(int)optk_cpp11_mode]) {
    cpp11_sfinae_enabled = (gnu_version >= 30400);
  }  /* if */
  if (!option_kind_used[(int)optk_cpp11_sfinae_ignore_access]) {
    /* g++ seems to ignore access checking until 4.8. */
    if (cpp11_sfinae_enabled) {
      cpp11_sfinae_ignore_access = (gnu_version < 40800);
    }  /* if */
  }  /* if */
  if (!cpp11_mode && gnu_version >= 40300) {
    /* g++ 4.3 enabled the decltype feature unconditionally via the __decltype
       keyword (the decltype keyword is only enabled in C++11 mode). */
    decltype_enabled = TRUE;
    if (!ms_extensions) {
      enable_underscore_decltype_only = TRUE;
    }  /* if */
  }  /* if */
  if (gnu_version >= 40500) {
    /* GCC 4.5 and later accept explicit conversion functions even in non-C++11
       mode (with a warning, which we don't issue). */
    explicit_conversion_functions_enabled = TRUE;
  }  /* if */
  if (!option_kind_used[(int)optk_rvalue_ctor_is_copy_ctor] &&
      gnu_version < 40600) {
    /* GCC versions prior to 4.6 generate an implicit traditional copy
       constructor even when a move constructor was explicitly declared
       (if they support move constructors at all, that is). */
    rvalue_ctor_is_copy_ctor = FALSE;
  }  /* if */
  if (gnu_version >= 40400) {
    /* g++ versions 4.4 and above support inline namespaces in all modes
       (not just C++11 mode). */
    inline_namespaces_enabled = TRUE;
    if (!cpp11_mode) {
      /* g++ versions 4.4 and above support explicit enum bases in all modes,
         but report it as nonstandard in non-C++11 mode. */
      explicit_enum_base_enabled = TRUE;
      if (!ms_extensions) {
        report_explicit_enum_base_as_nonstandard = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (cpp11_mode &&
      !option_kind_used[(int)optk_gen_move_operations]) {
    generate_move_operations = gnu_version >= 40600 &&
                               !rvalue_ctor_is_copy_ctor;
  }  /* if */
  if (gnu_version >= 40400 &&
      !option_kind_used[(int)optk_variadic_templates]) {
    /* GCC 4.4 and later accept variadic templates even in non-C++0x mode
       (with a warning, which we don't issue). */
    variadic_templates_enabled = TRUE;
  }  /* if */
  if (cpp11_mode && gnu_version >= 40500) {
    /* g++ 4.5 and later accept constexpr in C++11 mode. */
    constexpr_enabled =  TRUE;
  }  /* if */
  if (cpp11_mode &&
      (gpp_version_is(>= 70000) || clangcpp_version_is(>= 40000))) {
    /* g++ 7.0 and clang 4.0 and later accept if constexpr in C++11 mode. */
    constexpr_if_enabled =  TRUE;
  }  /* if */
  if (cpp11_mode &&
      (gpp_version_is(>= 90000) || clangcpp_version_is(>= 90000))) {
    /* g++ 9.0 and clang 9.0 and later accept pack expansions in an
       init-capture in C++11 mode. */
    pack_init_capture_enabled = TRUE;
  }  /* if */
  if (gnu_version >= 40700 && variadic_templates_enabled && !ms_compat &&
      !(option_kind_used[(int)optk_parse_nonclass_templates] &&
        !nonclass_prototype_instantiations)) {
    /* g++ 4.7 and later support __bases and __direct_bases.  This uses the
       variadic mechanism, so don't enable them if variadic templates have
       been disabled.  Also, this feature causes all templates to be treated
       as potentially variadic, and therefore requires that their prototype
       instantiations be done.  We therefore don't enable the feature if a
       command-line option explicitly requests not to perform nonclass
       prototype instantiations. */
    gnu_bases_operators_enabled = TRUE;
  }  /* if */
#if FUNCTION_PROTOTYPE_INSTANTIATION_DEFERRAL_ALLOWED
  if (gnu_version >= 30400) {
    if (!(option_kind_used[(int)optk_defer_parse_function_templates]) &&
        !ms_compat) {
      /* Only do function prototype instantiations for functions that actually
         need to be instantiated.  This is done to avoid diagnostics on unused
         functions.  Prototype instantiations cannot be deferred in some
         modes.  This is set when variadic templates are enabled because
         variadic templates always have prototype instantiations done. */
      defer_function_prototype_instantiations =
                                          nonclass_prototype_instantiations ||
                                          variadic_templates_enabled;
    }  /* if */
  }  /* if */
#endif /* FUNCTION_PROTOTYPE_INSTANTIATION_DEFERRAL_ALLOWED */
  if (!option_kind_used[(int)optk_implicit_noexcept]) {
    /* GCC 4.8 and later implement the C++11 rules that make a destructor or
       operator delete implicitly "noexcept" (unless otherwise specified). */
    implicit_noexcept_enabled = exceptions_enabled && noexcept_enabled &&
                                gnu_version >= 40800;
  }  /* if */
  if (clang_mode && !option_kind_used[(int)optk_designators]) {
    /* Clang does not (yet) apply this restriction in C++20 mode. */
    cpp20_designators_restriction = FALSE;
  }  /* if */
  if (!cpp11_mode) {
    /* Some C++11 extensions are enabled by default in some non-C++11 GNU C++
       and clang C++ modes.  A warning is issued on the first use (if any). */
    if (clang_mode ? clang_version >= 30000 : gnu_version >= 40400) {
      deleted_functions_enabled = TRUE;
      defaulted_special_members_enabled = TRUE;
    }  /* if */
    if (!option_kind_used[(int)optk_unrestricted_unions] &&
        (gpp_version_is(>= 40600) || clangcpp_version_is(>= 30100))) {
      /* GCC (since 4.6) and Clang (since 3.1) accept unrestricted unions with
         a warning in their pre-C++11 modes. */
      unrestricted_unions_enabled = TRUE;
    }  /* if */
    if (!option_kind_used[(int)optk_lambdas] &&
        (gpp_version_is(>= 40500) || clangcpp_version_is(>= 190000))) {
      /* Some versions of GCC and Clang accept (non-generic) lambda
         expressions with a warning in their pre-C++11 modes. */
      lambdas_enabled = TRUE;
    }  /* if */
    if (gnu_version >= 40700 && !clang_mode) {
      if (!option_kind_used[(int)optk_delegating_constructors]) {
        delegating_constructors_enabled = TRUE;
      }  /* if */
    }  /* if */
    if (clang_mode ? clang_version >= 30000 : gnu_version >= 40700) {
      field_initializers_enabled = TRUE;
      std_override_modifiers_enabled = TRUE;
    }  /* if */
    if (clang_mode ? clang_version >= 30000 : gnu_version >= 40800) {
      if (!option_kind_used[(int)optk_rvalue_references]) {
        /* GCC doesn't enable rvalue references by default, but it does enable
           inheriting constructors, which in theory requires rvalue references.
           We pair both features.  Clang does enable rvalue references by
           default. */
        rvalue_references_enabled = TRUE;
      }  /* if */
    }  /* if */
    if (gnu_version >= 40800 && !clang_mode) {
      if (rvalue_references_enabled) {
        inheriting_constructors_enabled = TRUE;
      }  /* if */
      std_attributes_enabled = TRUE;
    }  /* if */
    if (gnu_version >= 40700 || clang_mode) {
      /* GCC versions since 4.7, as well as clang, accept the extended
         friend syntax in non-C++11 mode, as well as decltype as a
         base specifier. */
      extended_friends_enabled = TRUE;
      enable_decltype_in_base_specifier_and_mem_initializer = TRUE;
    }  /* if */
  }  /* if */
  if (cpp14_mode) {
    if (gpp_version_is(>=40900)) {
      abbr_func_templates_enabled = TRUE;
    }  /* if */
  } else {
    /* Pre-C++14 mode. */
    if (lambdas_enabled) {
      /* GCC versions that support lambdas also support generalized lambda
         captures. */
      init_capture_enabled = TRUE;
    }  /* if */
    if (!deduced_return_types_enabled && auto_type_specifier_enabled &&
        !clang_mode && gnu_version >= 40800 && gnu_version < 40900) {
      /* GCC 4.8.x accepts deduced return types with a warning (on every
         occurrence) in its C++11 mode. */
      deduced_return_types_enabled = TRUE;
      warn_on_deduced_return_types = TRUE;
    }  /* if */
  }  /* if */
  if (!cpp17_mode) {
    /* g++ and clang do not implement this as a DR in C++14 mode.  Note that
       clang has this disabled by default even in C++17 mode (as of 6.0.1).
       But we don't have a separate option to enable this, so we still
       enable it in clang C++17 mode. */
    generalized_template_template_matching = FALSE;
  }  /* if */
  if (clang_mode) {
    /* All versions of clang appear to accept attributes on enumerators. */
    enumerator_attributes_enabled = TRUE;
    attributes_on_using_declarations = TRUE;
    /* Clang 7 and later allow in-class explicit specializations. */
    allow_in_class_specializations = clang_version >= 70000;
    if (cpp11_mode) {
      /* Clang enables terse static assert (with a warning) in C++11 mode. */
      terse_static_assert_enabled = TRUE;
    } else {
      /* Clang allows alias declarations (with a warning) in non-C++11 modes */
      alias_declarations_enabled = TRUE;
    }  /* if */
    if (clang_version >= 30000) {
      inline_namespaces_enabled = TRUE;
      if (clang_version >= 30100) {
        /* Clang enables _Atomic support in all C++ modes. */
        c11_atomic_enabled = TRUE;
      }  /* if */
      if (clang_version >= 30200) {
        alignof_enabled = TRUE;
      }  /* if */
      if (clang_version >= 30300) {
        std_thread_local_storage_specifier_enabled = TRUE;
      }  /* if */
      if (clang_version >= 30600) {
        nested_namespace_definitions_enabled = TRUE;
      }  /* if */
      if (clang_version >= 30700) {
        /* Clang accepts (with a warning) enum types as name qualifiers in
           pre-C++11 modes. */
        enum_qualifiers_enabled = TRUE;
      }  /* if */
    }  /* if */
    if (clang_version >= 60000) {
      /* Enabled by default (with a warning if in non-C++17 mode). */
      using_attribute_namespaces_enabled = TRUE;
      namespace_attributes_enabled = TRUE;
      fold_expressions_enabled = TRUE;
    }  /* if */
    if (clang_version >= 80000) {
      /* Enabled by default (with a warning if in non-C++20 mode). */
      nested_inline_namespace_definitions_enabled = TRUE;
    }  /* if */
    if (clang_version >= 90000) {
      /* Clang 9 enables a number of C++14 and C++17 features in earlier
         language modes (much like GCC).  A warning will be issued when
         the corresponding constructs are encountered in those earlier
         modes. */
      selection_initializers_enabled = TRUE;
      constexpr_if_enabled = TRUE;
      if (cpp11_mode) {
        /* Generic lambdas are not accepted in the pre-C++11 modes in which
           Clang 19 (and later) accepts ordinary lambdas. */
        generic_lambdas_enabled = TRUE;
        generic_lambdas_can_implicitly_capture = TRUE;
      }  /* if */
      init_capture_enabled = TRUE;
    }  /* if */
    if (clang_version >= 130000) {
      /* As of version 13.0.0, clang recognizes the "z" integer suffix. */
      size_suffix_enabled = TRUE;
    }  /* if */
    if (clang_version >= 170000) {
      /* Clang 17 (and later) accepts standard syntax attributes. */
      std_attributes_enabled = TRUE;
    }  /* if */
  } else {
    /* Not Clang mode. */
    /* Early template test for g++ prior to 4.7 (a TRUE value corresponds to
       standard behavior). */
    late_template_ovl_res_tiebreaker = gnu_version >= 40700;
    if (gnu_version < 60000) {
      /* GCC 5.x and earlier accept "false" as a null pointer constant. */
      false_literal_is_not_null_pointer_constant = FALSE;
    } else {
      enumerator_attributes_enabled = TRUE;
      fold_expressions_enabled = TRUE;
      namespace_attributes_enabled = TRUE;
      nested_namespace_definitions_enabled = TRUE;
    }  /* if */
    if (cpp11_mode && gnu_version >= 60000) {
      /* Later versions of GNU appear to enable this by default. */
      terse_static_assert_enabled = TRUE;
    }  /* if */
    if (gnu_version >= 70000) {
      /* GCC 7.x and later enable selection initializers in all C++ modes
         that accept lambdas. */
      capture_star_this_enabled = TRUE;
      /* GCC 7.x and later enable selection initializers in all C++ modes. */
      selection_initializers_enabled = TRUE;
      using_attribute_namespaces_enabled = TRUE;
    }  /* if */
    register_is_deprecated = FALSE;
    register_is_disallowed = FALSE;
    operator_bool_increment_allowed = TRUE;
    /* g++ (12.1) does not yet allow in-class explicit specializations. */
    allow_in_class_specializations = FALSE;
    if (gnu_version >= 90000) {
      nested_inline_namespace_definitions_enabled = TRUE;
      /* Accepted with a warning in pre-C++20 modes. */
      init_statement_allowed_in_range_based_for = TRUE;
      destroying_operator_delete_enabled = TRUE;
    }  /* if */
    if (gnu_version >= 60000) {
      /* g++ allows specializations to use inaccessible members. */
      relaxed_specialization_access_checking = TRUE;
    }  /* if */
    if (gnu_version >= 110000) {
      /* As of version 11.1.0, g++ recognizes the "z" integer suffix. */
      size_suffix_enabled = TRUE;
    }  /* if */
    if (gnu_version >= 120000) {
      float16_enabled = TRUE;
    }  /* if */
    if (!(option_kind_used[(int)optk_modules] && modules_enabled)) {
      /* No version of g++ supports module keywords by default.  Disable the
         modules keywords to match g++ parsing unless the modules option was
         used to enable support for C++20 modules. */
      module_keywords_enabled = FALSE;
    }  /* if */
  }  /* if */
  if (!option_kind_used[(int)optk_relaxed_abstract_checking] &&
      !gpp_version_is(>= 110000)) {
    /* GCC does not implement P0929R2 until version 11.x. */
    relaxed_abstract_checking = FALSE;
  }  /* if */
  if (gnu_version_is(<110000) || clang_version_is(<90000)) {
    deduction_guide_redeclaration_allowed = TRUE;
  }  /* if */
}  /* check_and_set_gpp_mode_options */


/*lint -esym(523,*exclude_gnu_specific_options)*/
static void exclude_gnu_specific_options(void)
/*
No GNU mode was selected: Make sure no option specific to GNU mode is
selected either.
*/
{
#if GNU_EXTENSIONS_ALLOWED
  if (il_header.short_enums) {
    command_line_error(ec_cl_short_enums_requires_gcc_mode);
  }  /* if */
  if (report_gnu_extensions) {
    if (option_kind_used[(int)optk_report_gnu_extensions]) {
      command_line_error(ec_cl_report_gnu_extensions_requires_gnu_mode);
    }  /* if */
    report_gnu_extensions = FALSE;
  }  /* if */
  if (nonstd_gnu_keywords_enabled &&
      option_kind_used[(int)optk_nonstd_gnu_keywords]) {
    command_line_error(ec_cl_nonstd_gnu_keywords_requires_gnu_mode);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
}  /* exclude_gnu_specific_options */


/*lint -ecall(523,*exclude_sun_specific_options)*/
static void exclude_sun_specific_options(void)
/*
Sun mode is not selected: Make sure no option specific to Sun mode was
selected either.
*/
{
}  /* exclude_sun_specific_options */


/*lint -ecall(523,*check_embedded_c_options)*/
static void check_embedded_c_options(void)
/*
An ANSI C dialect has been selected.  If any options were selected to enable
Embedded C (TR 18037) extensions, check them for consistency.
*/
{
  if (option_kind_used[(int)optk_embedded_c]) {
#if EMBEDDED_C_ALLOWED
    /* The options "--embedded_c" and "--no_embedded_c" should not be combined
       with the options to select individual Embedded C extensions. */
    a_boolean	individual_used = FALSE;
#if FIXED_POINT_ALLOWED
    if (option_kind_used[(int)optk_fixed_point]) individual_used = TRUE;
#endif /* FIXED_POINT_ALLOWED */
#if NAMED_ADDRESS_SPACES_ALLOWED
    if (option_kind_used[(int)optk_named_address_spaces]) {
      individual_used = TRUE;
    }  /* if */
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
#if NAMED_REGISTERS_ALLOWED
    if (option_kind_used[(int)optk_named_registers]) individual_used = TRUE;
#endif /* NAMED_REGISTERS_ALLOWED */
    if (individual_used) {
      command_line_error(
           ec_embedded_c_option_incompatible_with_individual_feature_options);
    }  /* if */
#endif /* EMBEDDED_C_ALLOWED */
  } else if (strict_ansi_mode) {
    /* Disable any Embedded C features not explicitly requested on the
       command line. */
#if FIXED_POINT_ALLOWED
    if (!option_kind_used[(int)optk_fixed_point]) {
      fixed_point_enabled = FALSE;
    }  /* if */
#endif /* FIXED_POINT_ALLOWED */
#if NAMED_ADDRESS_SPACES_ALLOWED
    if (!option_kind_used[(int)optk_named_address_spaces]) {
      named_address_spaces_enabled = FALSE;
    }  /* if */
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
#if NAMED_REGISTERS_ALLOWED
    if (!option_kind_used[(int)optk_named_registers]) {
      named_registers_enabled = FALSE;
    }  /* if */
#endif /* NAMED_REGISTERS_ALLOWED */
  }  /* if */
}  /* check_embedded_c_options */


static void check_dialect_and_language_modes(void)
/*
Check for consistent specification of dialects and language modes.  Dialect
inconsistencies are not allowed (and have been diagnosed previously).
Language modes that are specified are required to be consistent with the
operative dialect.

Here is a summary of the dialects and modes, along with the associated
command line switches.

    dialect/mode        associated global variable       option
    ============        ==========================       ======
  C                                                      --c, -m
    pcc mode            C_dialect == C_dialect_pcc       --old_c, -K
    "ANSI" [= not pcc]  C_dialect == C_dialect_ANSI      (default)
      SVR4 mode         SVR4_C_mode                      --svr4
      microsoft mode    ms_extensions, ms_compat, microsoft_mode
                                                         --microsoft
        MS compatibility ms_extensions, ms_compat        --ms_compatibility
        MS extensions   ms_extensions                    --ms_extensions
        bugs mode       microsoft_bugs                   --microsoft_bugs
        16-bit mode     il_header.near_and_far_allowed   --microsoft_16
      C99               std_version >= 199901            --c99
      C11               std_version >= 201112            --c11
      C18               std_version >= 201710            --c18, --c17
      C23               std_version >= 202311            --c23,
        strict          strict_ansi_mode                 -A, -a, etc.
      "normal"            
        strict          strict_ansi_mode                 -A, -a, etc.
      GNU C             gcc_mode                         --gcc
      clang C           gcc_mode && clang_mode           --c --clang

  C++                   C_dialect == C_dialect_cplusplus --c++, -p
    cfront mode
      2.1 mode          cfront_2_1_mode                  --cfront_2.1
      3.0 mode          cfront_3_0_mode                  --cfront_3.0
    microsoft mode      ms_extensions, ms_compat, microsoft_mode
                                                         --microsoft
      MS compatibility  ms_extensions, ms_compat         --ms_compatibility
      MS extensions     ms_extensions                    --ms_extensions
      bugs mode         microsoft_bugs                   --microsoft_bugs
      16-bit mode       il_header.near_and_far_allowed   --microsoft_16
    sun mode            sun_mode                         --sun
    GNU C++             gpp_mode                         --g++
    clang C++           gpp_mode && clang_mode           --clang
    C++03               std_version >= 199711            --c++03
    C++11               std_version >= 201103            --c++11
    C++14               std_version >= 201402            --c++14
    C++17               std_version >= 201703            --c++17
    C++20               std_version >= 202002            --c++20
    C++23               std_version >= 202302            --c++23
    C++26               std_version >= 202603            --c++26
    "normal"
      strict            strict_ansi_mode                 -A, -a, etc.

The major C dialect (K&R, ANSI, or C++) is determined by a command line option
(if any) that selects a major dialect, either implicitly (e.g., --g++ or
--embedded_c) or explicitly (e.g., --c++ or --c).  The specification of two or
more command line options that either implicitly or explicitly set the major
dialect to different values is an error.  No major dialect is implicitly
specified with --microsoft et al. or --strict et al.  --sun cannot be combined
with command-line options to select a C mode, but otherwise it implies C++ mode
(even in the somewhat unusual event that the front end were modified to compile
C code by default).

Beginning with Visual Studio 2015 Update 3 (aka, microsoft_version 1903
internally), the command-line options --ms_c++14 and --ms_c++latest have been
introduced to emulate the corresponding /std:c++14 and /std:c++latest
command-line options.  Strictly speaking these are not treated as "modes";
rather those command-line options turn on individual features (via global
variables) to enable the same set of features that would be available in
the specified version of Visual Studio.  Note that --c++14 (i.e., the ISO set
of C++14 features) is not guaranteed to be the same as --ms_c++14 (i.e.,
Microsoft's implemented set) though these may converge in the future.

Microsoft emulation has been split into three "tiers": --ms_extensions
emulates clang's -fms-extensions mode, --ms_compatibility emulates clang's
-fms-compatibility mode, and --microsoft enables full Microsoft compatibility.
Only the last of these modes qualifies as a "major dialect", thereby allowing
--ms_extensions, and/or --ms_compatibility to be specified with other major
dialects (e.g., --clang).

A mode for a newer standard (like C99 or C++11) is in some ways considered
both a dialect and a mode.  For example, with --c99 C_dialect is still
C_dialect_ANSI, but it can also be used in conjunction with Microsoft mode.
Likewise for --c++11 which implicitly sets the dialect to C_dialect_cplusplus
and also sets std_version.

clang mode is a variant of GNU mode.  Specifying --clang or --clang_version
will implicitly set gcc_mode or gpp_mode.

Although not recommended, some conflicting language modes can be specified on
the command line with the last option being effective (e.g., --c89 --c99
results in C99 mode).

Note that the fact that K&R C is its own major dialect, rather than being a
minor dialect under C mode, is a historical accident of the order of
development of this front end, and is inconsistent and strange.
*/
{
  int	pass;

#if GNU_EXTENSIONS_ALLOWED
  /* Set the default values of gcc_mode and gpp_mode if necessary. */
  if (!option_kind_used[(int)optk_gcc_mode] &&
      !option_kind_used[(int)optk_gpp_mode]) {
    a_boolean  enable_gnu_mode;
    enable_gnu_mode = /*lint -e(506)*/(DEFAULT_GNU_COMPATIBILITY) ||
                      option_kind_used[(int)optk_gnu_version] ||
                      option_kind_used[(int)optk_gnu_c89_inlining] ||
                      option_kind_used[(int)optk_clang_mode] ||
                      option_kind_used[(int)optk_clang_version];
    if (enable_gnu_mode) {
      if (C_dialect == C_dialect_cplusplus) {
        gpp_mode = gnu_mode = TRUE;
      } else {
        gcc_mode = gnu_mode = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (C_dialect != C_dialect_ANSI) {
    /* Issue an error for specifying a language mode that is valid only
       when the dialect is ANSI C. */
    exclude_SVR4_C_mode(ec_cl_SVR4_C_option_only_in_ansi_C);
    exclude_c99_mode();
    exclude_gcc_mode(ec_cl_incompatible_language_modes);
  } else {
    /* C99 and SVR4 C modes are mutually exclusive. */
    if (c99_mode) exclude_SVR4_C_mode(ec_cl_incompatible_language_modes);
    if (SVR4_C_mode) exclude_c99_mode();
    check_embedded_c_options();
  }  /* if */
  if (C_dialect != C_dialect_cplusplus) {
    /* Issue an error for specifying a language mode that is valid only
       when the dialect is C++. */
    exclude_cfront_mode(ec_cl_incompatible_language_modes);
    exclude_sun_mode(ec_cl_sun_mode_only_in_cplusplus);
    exclude_gpp_mode(ec_cl_incompatible_language_modes);
    exclude_cpp_mode();
  }  /* if */
  if (C_dialect == C_dialect_pcc) {
    /* Issue an error for specifying a language mode that is valid only
       in ANSI C or C++ modes. */
    exclude_microsoft_mode(ec_cl_incompatible_language_modes);
  }  /* if */
  if (strict_ansi_mode) {
    /* Strict ANSI mode is incompatible with K&R/pcc mode. */
    if (C_dialect == C_dialect_pcc) {
      command_line_error(ec_cl_strict_mode_incompatible_with_pcc);
    }  /* if */
    /* Strict ANSI mode is incompatible with cfront compatibility mode. */
    exclude_cfront_mode(ec_cl_strict_mode_incompatible_with_cfront);
    exclude_microsoft_mode(ec_cl_strict_mode_incompatible_with_microsoft);
    exclude_sun_mode(ec_cl_strict_mode_incompatible_with_sun);
    exclude_SVR4_C_mode(ec_cl_strict_mode_incompatible_with_SVR4);
    exclude_gcc_mode(ec_cl_incompatible_language_modes);
    exclude_gpp_mode(ec_cl_incompatible_language_modes);
  }  /* if */
  if (any_cfront_mode()) {
    /* Issue an error for specifying any other language mode.  Strict mode
       has already been checked for. */
    check_assertion(C_dialect == C_dialect_cplusplus);
    exclude_sun_mode(ec_cl_sun_incompatible_with_cfront);
    exclude_microsoft_mode(ec_cl_cfront_incompatible_with_microsoft);
  }  /* if */
  /* Check for incompatibilities for dialects that can be selected as the
     default.  The two passes are done so that the non-default dialects will
     be checked first.  This ensures, for example, that if g++ mode is being
     used, the exclude_microsoft_mode call will be done before the Microsoft
     mode test calls exclude_gpp_mode. */
  for (pass = 0; pass <= 1; pass++) {
#if SUN_EXTENSIONS_ALLOWED
    if (sun_mode &&
        (int)(DEFAULT_SUN_COMPATIBILITY != 0) == pass) {
      /* Issue an error for specifying any other language mode.  Strict mode
         has already been checked for. */
      exclude_microsoft_mode(ec_cl_incompatible_language_modes);
      exclude_gcc_mode(ec_cl_incompatible_language_modes);
      exclude_gpp_mode(ec_cl_incompatible_language_modes);
    }  /* if */
#endif /* SUN_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode &&
        (int)(DEFAULT_MICROSOFT_MODE != 0) == pass) {
      /* Issue an error for specifying any other language mode.  Strict mode,
         K&R mode, and cfront mode have already been checked for. */
      exclude_SVR4_C_mode(ec_cl_incompatible_language_modes);
      exclude_gcc_mode(ec_cl_incompatible_language_modes);
      exclude_gpp_mode(ec_cl_incompatible_language_modes);
      exclude_sun_mode(ec_cl_incompatible_language_modes);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (gnu_mode && (int)(DEFAULT_GNU_COMPATIBILITY != 0) == pass) {
      /* Issue an error for specifying any other language mode.  Strict mode,
         K&R mode, and cfront mode have already been checked for. */
      exclude_SVR4_C_mode(ec_cl_incompatible_language_modes);
      exclude_microsoft_mode(ec_cl_incompatible_language_modes);
      exclude_sun_mode(ec_cl_incompatible_language_modes);
    }  /* if */
  }  /* for */
}  /* check_dialect_and_language_modes */

#if UPC_EXTENSIONS_ALLOWED

static void check_upc_mode(void)
/*
Check that UPC mode is enabled only in ANSI C or C99 mode; i.e., not K&R C
(which has no type qualifiers) and not C++.
*/
{
  check_assertion(upc_mode);
  if (C_dialect != C_dialect_ANSI) {
    if (option_kind_used[(int)optk_upc_mode]) {
      /* UPC mode was explicitly enabled: Issue an error. */
      command_line_error(ec_cl_upc_requires_ansi_c_dialect);
    }  /* if */
    /* If UPC mode is the default (and not mentioned on the command line),
       this will silently turn it off. */
    upc_mode = FALSE;
  }  /* if */
}  /* check_upc_mode */

#endif /* UPC_EXTENSIONS_ALLOWED */

#if COMPILE_MULTIPLE_TRANSLATION_UNITS

static void check_for_duplicated_file_names(void)
/*
Make sure a given file name was not specified more than once.
*/
{
  int	arg1;
  int	arg2;
  char	**file_list;

  /* argv_file_list already points to the second file.  Get a pointer
     to the array of file names, starting with the first one. */
  file_list = argv_file_list - 1;
  for (arg1 = 0; arg1 < argc_file_list; arg1++) {
    for (arg2 = arg1 + 1; arg2 <= argc_file_list; arg2++) {
      if (compare_file_names(file_list[arg1],
                             file_list[arg2]) == 0) {
        str_command_line_error(ec_cl_duplicate_file_name,
                               file_list[arg1]);
      }  /* if */
    }  /* for */
  }  /* for */
}  /* check_for_duplicated_file_names */

#endif /* COMPILE_MULTIPLE_TRANSLATION_UNITS */

static void set_default_message_severities(void)
/*
Some messages are given different severities by default.  This routine
assigns those severities.
*/
{
  /* Unless requested otherwise (using a command-line option or a pragma),
     ILP64 porting diagnostics should be remarks. */
  (void)set_severity_for_error_number((int)ec_ilp64_will_narrow, es_remark,
                                      /*make_default=*/TRUE);
  /* Certain warnings related to 64-bit porting should be disabled
     by default. */
  (void)set_severity_for_error_number((int)ec_impl_narrowing_64_bit_int,
                                      es_none,
                                      /*make_default=*/TRUE);
  (void)set_severity_for_error_number((int)ec_expl_narrowing_64_bit_int,
                                      es_none,
                                      /*make_default=*/TRUE);
  (void)set_severity_for_error_number(
                                   (int)ec_pointer_conversion_to_same_size_int,
                                      es_none,
                                      /*make_default=*/TRUE);
  /* In Microsoft mode, converting an implicitly signed 32-bit pointer to an
     explicitly or implicitly signed 64-bit pointer should be a remark by
     default. */
  (void)set_severity_for_error_number((int)ec_microsoft_ptr_sign_extension,
                                      es_remark, /*make_default=*/TRUE);
  /* Diagnostics on comparing a known non-null pointer value to NULL
     should be suppressed unless requested. */
  (void)set_severity_for_error_number((int)ec_known_comparison_with_null,
                                      es_none, /*make_default=*/TRUE);
}  /* set_default_message_severities */


#if DUMP_CONFIG_ENABLED
static void dump_configuration_macros(void)
/*
Display the values of all the configuration macros with which this
executable was built in a form suitable for capture and use as a defines.h
file.
*/
{
/* Macros used to display the various options. */
/* Write a #define directive for the option, which has a non-numeric value: */
#define define_string_valued_macro(X) \
  fprintf(f_error, "#define %s %s\n", #X, stringize(X))
/* Write a comment giving the option name and its (non-numeric) value: */
/*lint -esym(750,*comment_string_valued_macro)*/
#define comment_string_valued_macro(X) \
  fprintf(f_error, "/*      %s %s */\n", #X, stringize(X))
/* Write a #define directive for the option, which has a numeric value: */
#define define_numeric_valued_macro(X)                                       \
  { a_number_buffer tmp_str((a_host_large_integer)(X));                      \
    fprintf(f_error, "#define %s %s\n", #X, tmp_str.as_temp_characters());   \
  }
/* Write a comment giving the option name and its (numeric) value: */
#define comment_numeric_valued_macro(X)                                       \
  { a_number_buffer tmp_str((a_host_large_integer)(X));                       \
    fprintf(f_error, "/*      %s %s */\n", #X, tmp_str.as_temp_characters()); \
  }
/* Write a comment giving the option name and noting that it is undefined: */
#define comment_undefined_macro_name(X) \
  fprintf(f_error, "/*      %s not defined */\n", #X)

/* Print a banner. */
  fprintf(f_error,
          "/* Configuration data for Edison Design Group C/C++ Front End */\n"
          "/* version %s, built on %s at %s. */\n\n",
          VERSION_NUMBER, build_date, build_time);
/* Print the values of all configuration macros. */
#if defined(ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE)
  define_numeric_valued_macro(ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE);
#else /* !defined(ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE) */
  comment_undefined_macro_name(ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE);
#endif /* defined(ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE) */
#if defined(ABI_CHANGES_FOR_CONSTRUCTION_VTBLS)
  define_numeric_valued_macro(ABI_CHANGES_FOR_CONSTRUCTION_VTBLS);
#else /* !defined(ABI_CHANGES_FOR_CONSTRUCTION_VTBLS) */
  comment_undefined_macro_name(ABI_CHANGES_FOR_CONSTRUCTION_VTBLS);
#endif /* defined(ABI_CHANGES_FOR_CONSTRUCTION_VTBLS) */
#if defined(ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN)
  define_numeric_valued_macro(ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN);
#else /* !defined(ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN) */
  comment_undefined_macro_name(ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN);
#endif /* defined(ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN) */
#if defined(ABI_CHANGES_FOR_PLACEMENT_DELETE)
  define_numeric_valued_macro(ABI_CHANGES_FOR_PLACEMENT_DELETE);
#else /* !defined(ABI_CHANGES_FOR_PLACEMENT_DELETE) */
  comment_undefined_macro_name(ABI_CHANGES_FOR_PLACEMENT_DELETE);
#endif /* defined(ABI_CHANGES_FOR_PLACEMENT_DELETE) */
#if defined(ABI_CHANGES_FOR_RTTI)
  define_numeric_valued_macro(ABI_CHANGES_FOR_RTTI);
#else /* !defined(ABI_CHANGES_FOR_RTTI) */
  comment_undefined_macro_name(ABI_CHANGES_FOR_RTTI);
#endif /* defined(ABI_CHANGES_FOR_RTTI) */
#if defined(ABI_COMPATIBILITY_VERSION)
  define_numeric_valued_macro(ABI_COMPATIBILITY_VERSION);
#else /* !defined(ABI_COMPATIBILITY_VERSION) */
  comment_undefined_macro_name(ABI_COMPATIBILITY_VERSION);
#endif /* defined(ABI_COMPATIBILITY_VERSION) */
#if defined(ABORT_ON_INIT_COMPONENT_LEAKAGE)
  define_numeric_valued_macro(ABORT_ON_INIT_COMPONENT_LEAKAGE);
#else /* !defined(ABORT_ON_INIT_COMPONENT_LEAKAGE) */
  comment_undefined_macro_name(ABORT_ON_INIT_COMPONENT_LEAKAGE);
#endif /* defined(ABORT_ON_INIT_COMPONENT_LEAKAGE) */
#if defined(ACCEPT_GNU_CARRIAGE_RETURN_LINE_TERMINATOR)
  define_numeric_valued_macro(ACCEPT_GNU_CARRIAGE_RETURN_LINE_TERMINATOR);
#else /* !defined(ACCEPT_GNU_CARRIAGE_RETURN_LINE_TERMINATOR) */
  comment_undefined_macro_name(ACCEPT_GNU_CARRIAGE_RETURN_LINE_TERMINATOR);
#endif /* defined(ACCEPT_GNU_CARRIAGE_RETURN_LINE_TERMINATOR) */
#if defined(ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS)
  define_numeric_valued_macro(ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS);
#else /* !defined(ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS) */
  comment_undefined_macro_name(ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS);
#endif /* defined(ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS) */
#if defined(ADDRS_NOT_IN_SAME_ARRAY_CAN_BE_COMPARED)
  define_numeric_valued_macro(ADDRS_NOT_IN_SAME_ARRAY_CAN_BE_COMPARED);
#else /* !defined(ADDRS_NOT_IN_SAME_ARRAY_CAN_BE_COMPARED) */
  comment_undefined_macro_name(ADDRS_NOT_IN_SAME_ARRAY_CAN_BE_COMPARED);
#endif /* defined(ADDRS_NOT_IN_SAME_ARRAY_CAN_BE_COMPARED) */
#if defined(ADDR_OF_BIT_FIELD_ALLOWED)
  define_numeric_valued_macro(ADDR_OF_BIT_FIELD_ALLOWED);
#else /* !defined(ADDR_OF_BIT_FIELD_ALLOWED) */
  comment_undefined_macro_name(ADDR_OF_BIT_FIELD_ALLOWED);
#endif /* defined(ADDR_OF_BIT_FIELD_ALLOWED) */
#if defined(ADD_BRACES_TO_AVOID_DANGLING_ELSE_IN_GENERATED_C)
  define_numeric_valued_macro(
                             ADD_BRACES_TO_AVOID_DANGLING_ELSE_IN_GENERATED_C);
#else /* !defined(ADD_BRACES_TO_AVOID_DANGLING_ELSE_IN_GENERATED_C) */
  comment_undefined_macro_name(
                             ADD_BRACES_TO_AVOID_DANGLING_ELSE_IN_GENERATED_C);
#endif /* defined(ADD_BRACES_TO_AVOID_DANGLING_ELSE_IN_GENERATED_C) */
#if defined(ADD_CHECKING_PRAGMAS_FOR_INTERNAL_TESTING)
  define_numeric_valued_macro(ADD_CHECKING_PRAGMAS_FOR_INTERNAL_TESTING);
#else /* !defined(ADD_CHECKING_PRAGMAS_FOR_INTERNAL_TESTING) */
  comment_undefined_macro_name(ADD_CHECKING_PRAGMAS_FOR_INTERNAL_TESTING);
#endif /* ADD_CHECKING_PRAGMAS_FOR_INTERNAL_TESTING */
#if defined(ALIAS_DIRECTIVE)
  define_numeric_valued_macro(ALIAS_DIRECTIVE);
#else /* !defined(ALIAS_DIRECTIVE) */
  comment_undefined_macro_name(ALIAS_DIRECTIVE);
#endif /* defined(ALIAS_DIRECTIVE) */
#if defined(ALLOW_ADDR_OF_REGISTER_IN_GENERATED_C)
  define_numeric_valued_macro(ALLOW_ADDR_OF_REGISTER_IN_GENERATED_C);
#else /* !defined(ALLOW_ADDR_OF_REGISTER_IN_GENERATED_C) */
  comment_undefined_macro_name(ALLOW_ADDR_OF_REGISTER_IN_GENERATED_C);
#endif /* defined(ALLOW_ADDR_OF_REGISTER_IN_GENERATED_C) */
#if defined(ALLOW_CPPCLI_AND_CPPCX_WITH_LOWERING)
  define_numeric_valued_macro(ALLOW_CPPCLI_AND_CPPCX_WITH_LOWERING);
#else /* !defined(ALLOW_CPPCLI_AND_CPPCX_WITH_LOWERING) */
  comment_undefined_macro_name(ALLOW_CPPCLI_AND_CPPCX_WITH_LOWERING);
#endif /* defined(ALLOW_CPPCLI_AND_CPPCX_WITH_LOWERING) */
#if defined(ALLOW_ELLIPSIS_ONLY_PARAM_IN_GENERATED_C)
  define_numeric_valued_macro(ALLOW_ELLIPSIS_ONLY_PARAM_IN_GENERATED_C);
#else /* !defined(ALLOW_ELLIPSIS_ONLY_PARAM_IN_GENERATED_C) */
  comment_undefined_macro_name(ALLOW_ELLIPSIS_ONLY_PARAM_IN_GENERATED_C);
#endif /* defined(ALLOW_ELLIPSIS_ONLY_PARAM_IN_GENERATED_C) */
#if defined(ALLOW_ENABLING_OF_SSI_MODE)
  define_numeric_valued_macro(ALLOW_ENABLING_OF_SSI_MODE);
#else /* !defined(ALLOW_ENABLING_OF_SSI_MODE) */
  comment_undefined_macro_name(ALLOW_ENABLING_OF_SSI_MODE);
#endif /* defined(ALLOW_ENABLING_OF_SSI_MODE) */
#if defined(ALLOW_HIDDEN_NAMES_IN_IL_WITH_IL_LOWERING)
  define_numeric_valued_macro(ALLOW_HIDDEN_NAMES_IN_IL_WITH_IL_LOWERING);
#else /* !defined(ALLOW_HIDDEN_NAMES_IN_IL_WITH_IL_LOWERING) */
  comment_undefined_macro_name(ALLOW_HIDDEN_NAMES_IN_IL_WITH_IL_LOWERING);
#endif /* defined(ALLOW_HIDDEN_NAMES_IN_IL_WITH_IL_LOWERING) */
#if defined(ALLOW_HOST_FP_TOO_SMALL_FOR_LARGEST_FIXED_POINT_TYPE)
  define_numeric_valued_macro(
                         ALLOW_HOST_FP_TOO_SMALL_FOR_LARGEST_FIXED_POINT_TYPE);
#else /* !defined(ALLOW_HOST_FP_TOO_SMALL_FOR_LARGEST_FIXED_POINT_TYPE) */
  comment_undefined_macro_name(
                         ALLOW_HOST_FP_TOO_SMALL_FOR_LARGEST_FIXED_POINT_TYPE);
#endif /* defined(ALLOW_HOST_FP_TOO_SMALL_FOR_LARGEST_FIXED_POINT_TYPE) */
#if defined(ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C)
  define_numeric_valued_macro(
                             ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C);
#else /* !defined(ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C) */
  comment_undefined_macro_name(
                             ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C);
#endif /* defined(ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C) */
#if defined(ALLOW_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING)
  define_numeric_valued_macro(ALLOW_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING);
#else /* !defined(ALLOW_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING) */
  comment_undefined_macro_name(ALLOW_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING);
#endif /* defined(ALLOW_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING) */
#if defined(ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C)
  define_numeric_valued_macro(ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C);
#else /* !defined(ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C) */
  comment_undefined_macro_name(ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C);
#endif /* defined(ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C) */
#if defined(ALL_TEMPLATE_INFO_IN_IL)
  define_numeric_valued_macro(ALL_TEMPLATE_INFO_IN_IL);
#else /* !defined(ALL_TEMPLATE_INFO_IN_IL) */
  comment_undefined_macro_name(ALL_TEMPLATE_INFO_IN_IL);
#endif /* defined(ALL_TEMPLATE_INFO_IN_IL) */
#if defined(ALTERNATE_IL_FILE_FORMAT)
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  define_numeric_valued_macro(ALTERNATE_IL_FILE_FORMAT);
#else /* !IL_SHOULD_BE_WRITTEN_TO_FILE */
  /* ALTERNATE_IL_FILE_FORMAT is defined unconditionally in host_envir.h
     when IL_SHOULD_BE_WRITTEN_TO_FILE is FALSE, so just comment the value
     instead of writing a #define for it. */
  comment_numeric_valued_macro(ALTERNATE_IL_FILE_FORMAT);
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
#else /* !defined(ALTERNATE_IL_FILE_FORMAT) */
  comment_undefined_macro_name(ALTERNATE_IL_FILE_FORMAT);
#endif /* defined(ALTERNATE_IL_FILE_FORMAT) */
#if defined(ALWAYS_SET_MULTIBYTE_LOCALE)
  define_numeric_valued_macro(ALWAYS_SET_MULTIBYTE_LOCALE);
#else /* !defined(ALWAYS_SET_MULTIBYTE_LOCALE) */
  comment_undefined_macro_name(ALWAYS_SET_MULTIBYTE_LOCALE);
#endif /* defined(ALWAYS_SET_MULTIBYTE_LOCALE) */
#if defined(APPROXIMATE_QUADMATH)
  define_numeric_valued_macro(APPROXIMATE_QUADMATH);
#else /* !defined(APPROXIMATE_QUADMATH) */
  comment_undefined_macro_name(APPROXIMATE_QUADMATH);
#endif /* defined(APPROXIMATE_QUADMATH) */
#if defined(ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE)
  define_numeric_valued_macro(ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE);
#else /* !defined(ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE) */
  comment_undefined_macro_name(ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE);
#endif /* defined(ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE) */
#if defined(ASM_FUNCTION_ALLOWED)
  define_numeric_valued_macro(ASM_FUNCTION_ALLOWED);
#else /* !defined(ASM_FUNCTION_ALLOWED) */
  comment_undefined_macro_name(ASM_FUNCTION_ALLOWED);
#endif /* defined(ASM_FUNCTION_ALLOWED) */
#if defined(ASSIGNMENT_TO_THIS_ALLOWED)
  define_numeric_valued_macro(ASSIGNMENT_TO_THIS_ALLOWED);
#else /* !defined(ASSIGNMENT_TO_THIS_ALLOWED) */
  comment_undefined_macro_name(ASSIGNMENT_TO_THIS_ALLOWED);
#endif /* defined(ASSIGNMENT_TO_THIS_ALLOWED) */
#if defined(ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS)
  define_numeric_valued_macro(ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS);
#else /* !defined(ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS) */
  comment_undefined_macro_name(ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS);
#endif /* defined(ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS) */
#if defined(ASSUME_LITTLE_ENDIAN_IFC_MODULES)
  define_numeric_valued_macro(ASSUME_LITTLE_ENDIAN_IFC_MODULES);
#else /* !defined(ASSUME_LITTLE_ENDIAN_IFC_MODULES) */
  comment_undefined_macro_name(ASSUME_LITTLE_ENDIAN_IFC_MODULES);
#endif /* defined(ASSUME_LITTLE_ENDIAN_IFC_MODULES) */
#if defined(ASSUME_REFERENCES_CANNOT_BE_NULL)
  define_numeric_valued_macro(ASSUME_REFERENCES_CANNOT_BE_NULL);
#else /* !defined(ASSUME_REFERENCES_CANNOT_BE_NULL) */
  comment_undefined_macro_name(ASSUME_REFERENCES_CANNOT_BE_NULL);
#endif /* defined(ASSUME_REFERENCES_CANNOT_BE_NULL) */
#if defined(ASSUME_THIS_CANNOT_BE_NULL_IN_CONDITIONAL_OPERATORS)
  define_numeric_valued_macro(
                          ASSUME_THIS_CANNOT_BE_NULL_IN_CONDITIONAL_OPERATORS);
#else /* !defined(ASSUME_THIS_CANNOT_BE_NULL_IN_CONDITIONAL_OPERATORS) */
  comment_undefined_macro_name(
                          ASSUME_THIS_CANNOT_BE_NULL_IN_CONDITIONAL_OPERATORS);
#endif /* defined(ASSUME_THIS_CANNOT_BE_NULL_IN_CONDITIONAL_OPERATORS) */
#if defined(ATT_PREPROCESSING_EXTENSIONS_ALLOWED)
  define_numeric_valued_macro(ATT_PREPROCESSING_EXTENSIONS_ALLOWED);
#else /* !defined(ATT_PREPROCESSING_EXTENSIONS_ALLOWED) */
  comment_undefined_macro_name(ATT_PREPROCESSING_EXTENSIONS_ALLOWED);
#endif /* defined(ATT_PREPROCESSING_EXTENSIONS_ALLOWED) */
#if defined(AUTOMATIC_TEMPLATE_INSTANTIATION)
  define_numeric_valued_macro(AUTOMATIC_TEMPLATE_INSTANTIATION);
#else /* !defined(AUTOMATIC_TEMPLATE_INSTANTIATION) */
  comment_undefined_macro_name(AUTOMATIC_TEMPLATE_INSTANTIATION);
#endif /* defined(AUTOMATIC_TEMPLATE_INSTANTIATION) */
#if defined(BACKING_EXPR_FOR_NONTYPE_TEMPL_ARG)
  define_numeric_valued_macro(BACKING_EXPR_FOR_NONTYPE_TEMPL_ARG);
#else /* !defined(BACKING_EXPR_FOR_NONTYPE_TEMPL_ARG) */
  comment_undefined_macro_name(BACKING_EXPR_FOR_NONTYPE_TEMPL_ARG);
#endif /* defined(BACKING_EXPR_FOR_NONTYPE_TEMPL_ARG) */
#if defined(BACKSLASH_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR)
  define_numeric_valued_macro(BACKSLASH_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR);
#else /* !defined(BACKSLASH_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR) */
  comment_undefined_macro_name(BACKSLASH_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR);
#endif /* defined(BACKSLASH_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR) */
#if defined(BACKSLASH_IS_ALSO_DIR_SEPARATOR)
  define_numeric_valued_macro(BACKSLASH_IS_ALSO_DIR_SEPARATOR);
#else /* !defined(BACKSLASH_IS_ALSO_DIR_SEPARATOR) */
  comment_undefined_macro_name(BACKSLASH_IS_ALSO_DIR_SEPARATOR);
#endif /* defined(BACKSLASH_IS_ALSO_DIR_SEPARATOR) */
#if defined(BACK_END_IS_CP_GEN_BE)
  define_numeric_valued_macro(BACK_END_IS_CP_GEN_BE);
#else /* !defined(BACK_END_IS_CP_GEN_BE) */
  comment_undefined_macro_name(BACK_END_IS_CP_GEN_BE);
#endif /* defined(BACK_END_IS_CP_GEN_BE) */
#if defined(BACK_END_IS_C_GEN_BE)
  define_numeric_valued_macro(BACK_END_IS_C_GEN_BE);
#else /* !defined(BACK_END_IS_C_GEN_BE) */
  comment_undefined_macro_name(BACK_END_IS_C_GEN_BE);
#endif /* defined(BACK_END_IS_C_GEN_BE) */
#if defined(BACK_END_SHOULD_BE_CALLED)
  define_numeric_valued_macro(BACK_END_SHOULD_BE_CALLED);
#else /* !defined(BACK_END_SHOULD_BE_CALLED) */
  comment_undefined_macro_name(BACK_END_SHOULD_BE_CALLED);
#endif /* defined(BACK_END_SHOULD_BE_CALLED) */
#if defined(BITS_IN_AN_INTEGER_VALUE)
#if INTEGER_VALUE_REPR_IS_A_HOST_INTEGER
  define_numeric_valued_macro(BITS_IN_AN_INTEGER_VALUE);
#else /* !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
  /* BITS_IN_AN_INTEGER_VALUE is set unconditionally in targ_def.h to match
     the definition of struct an_integer_value when
     INTEGER_VALUE_REPR_IS_A_HOST_INTEGER is FALSE, so just comment the
     value instead of printing a #define for it. */
  comment_numeric_valued_macro(BITS_IN_AN_INTEGER_VALUE);
#endif /* INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
#else /* !defined(BITS_IN_AN_INTEGER_VALUE) */
  comment_undefined_macro_name(BITS_IN_AN_INTEGER_VALUE);
#endif /* defined(BITS_IN_AN_INTEGER_VALUE) */
#if defined(BOOL_ENABLING_POSSIBLE)
  define_numeric_valued_macro(BOOL_ENABLING_POSSIBLE);
#else /* !defined(BOOL_ENABLING_POSSIBLE) */
  comment_undefined_macro_name(BOOL_ENABLING_POSSIBLE);
#endif /* defined(BOOL_ENABLING_POSSIBLE) */
#if defined(BUILTIN_FUNCTIONS_ENABLED)
  define_string_valued_macro(BUILTIN_FUNCTIONS_ENABLED);
#else /* !defined(BUILTIN_FUNCTIONS_ENABLED) */
  comment_undefined_macro_name(BUILTIN_FUNCTIONS_ENABLED);
#endif /* defined(BUILTIN_FUNCTIONS_ENABLED) */
#if defined(BUILTIN_VA_LIST_OVERRIDE_TYPE_NAME)
  define_string_valued_macro(BUILTIN_VA_LIST_OVERRIDE_TYPE_NAME);
#else /* !defined(BUILTIN_VA_LIST_OVERRIDE_TYPE_NAME) */
  comment_undefined_macro_name(BUILTIN_VA_LIST_OVERRIDE_TYPE_NAME);
#endif /* defined(BUILTIN_VA_LIST_OVERRIDE_TYPE_NAME) */
#if defined(BUILTIN_VA_START_TAKES_ADDRESS_OF_VARIABLE)
  define_numeric_valued_macro(BUILTIN_VA_START_TAKES_ADDRESS_OF_VARIABLE);
#else /* !defined(BUILTIN_VA_START_TAKES_ADDRESS_OF_VARIABLE) */
  comment_undefined_macro_name(BUILTIN_VA_START_TAKES_ADDRESS_OF_VARIABLE);
#endif /* defined(BUILTIN_VA_START_TAKES_ADDRESS_OF_VARIABLE) */
#if defined(C99_IL_EXTENSIONS_SUPPORTED)
  define_numeric_valued_macro(C99_IL_EXTENSIONS_SUPPORTED);
#else /* !defined(C99_IL_EXTENSIONS_SUPPORTED) */
  comment_undefined_macro_name(C99_IL_EXTENSIONS_SUPPORTED);
#endif /* defined(C99_IL_EXTENSIONS_SUPPORTED) */
#if defined(CFRONT_2_1_OBJECT_CODE_COMPATIBILITY)
  define_numeric_valued_macro(CFRONT_2_1_OBJECT_CODE_COMPATIBILITY);
#else /* !defined(CFRONT_2_1_OBJECT_CODE_COMPATIBILITY) */
  comment_undefined_macro_name(CFRONT_2_1_OBJECT_CODE_COMPATIBILITY);
#endif /* defined(CFRONT_2_1_OBJECT_CODE_COMPATIBILITY) */
#if defined(CFRONT_3_0_OBJECT_CODE_COMPATIBILITY)
  define_numeric_valued_macro(CFRONT_3_0_OBJECT_CODE_COMPATIBILITY);
#else /* !defined(CFRONT_3_0_OBJECT_CODE_COMPATIBILITY) */
  comment_undefined_macro_name(CFRONT_3_0_OBJECT_CODE_COMPATIBILITY);
#endif /* defined(CFRONT_3_0_OBJECT_CODE_COMPATIBILITY) */
#if defined(CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG)
  define_numeric_valued_macro(CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG);
#else /* !defined(CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG) */
  comment_undefined_macro_name(CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG);
#endif /* defined(CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG) */
#if defined(CHECKING)
  define_numeric_valued_macro(CHECKING);
#else /* !defined(CHECKING) */
  comment_undefined_macro_name(CHECKING);
#endif /* defined(CHECKING) */
#if defined(CHECK_SWITCH_DEFAULT_UNEXPECTED)
  define_numeric_valued_macro(CHECK_SWITCH_DEFAULT_UNEXPECTED);
#else /* !defined(CHECK_SWITCH_DEFAULT_UNEXPECTED) */
  comment_undefined_macro_name(CHECK_SWITCH_DEFAULT_UNEXPECTED);
#endif /* defined(CHECK_SWITCH_DEFAULT_UNEXPECTED) */
#if defined(CLANG_IS_GENERATED_CODE_TARGET)
  define_numeric_valued_macro(CLANG_IS_GENERATED_CODE_TARGET);
#else /* !defined(CLANG_IS_GENERATED_CODE_TARGET) */
  comment_undefined_macro_name(CLANG_IS_GENERATED_CODE_TARGET);
#endif /* defined(CLANG_IS_GENERATED_CODE_TARGET) */
#if defined(CLANG_TARGET_VERSION_NUMBER)
  define_numeric_valued_macro(CLANG_TARGET_VERSION_NUMBER);
#else /* !defined(CLANG_TARGET_VERSION_NUMBER) */
  comment_undefined_macro_name(CLANG_TARGET_VERSION_NUMBER);
#endif /* defined(CLANG_TARGET_VERSION_NUMBER) */
#if defined(CLANG_VERSION_STRING)
#if !defined(_lint) && !(defined(_MSC_VER) && _MSC_VER < 1300)
  /* Microsoft version 6.0 and some lint versions have a preprocessor bug
     that creates an invalid result when the '#' operator is applied to the
     default value of CLANG_VERSION_STRING. */
  define_string_valued_macro(CLANG_VERSION_STRING);
#endif /* ifndef _lint */
#else /* !defined(CLANG_VERSION_STRING) */
  comment_undefined_macro_name(CLANG_VERSION_STRING);
#endif /* defined(CLANG_VERSION_STRING) */
#if defined(CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS)
  define_numeric_valued_macro(
                       CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS);
#else /* !defined(CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS) */
  comment_undefined_macro_name(
                       CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS);
#endif /* defined(CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS) */
#if defined(CLR_FALLBACK_VERSION)
  define_string_valued_macro(CLR_FALLBACK_VERSION);
#else /* !defined(CLR_FALLBACK_VERSION) */
  comment_undefined_macro_name(CLR_FALLBACK_VERSION);
#endif /* defined(CLR_FALLBACK_VERSION) */
#if defined(COLUMN_NUMBER_IN_BRIEF_DIAGNOSTICS)
  define_numeric_valued_macro(COLUMN_NUMBER_IN_BRIEF_DIAGNOSTICS);
#else /* !defined(COLUMN_NUMBER_IN_BRIEF_DIAGNOSTICS) */
  comment_undefined_macro_name(COLUMN_NUMBER_IN_BRIEF_DIAGNOSTICS);
#endif /* defined(COLUMN_NUMBER_IN_BRIEF_DIAGNOSTICS) */
#if defined(COMPILE_MULTIPLE_SOURCE_FILES)
  define_numeric_valued_macro(COMPILE_MULTIPLE_SOURCE_FILES);
#else /* !defined(COMPILE_MULTIPLE_SOURCE_FILES) */
  comment_undefined_macro_name(COMPILE_MULTIPLE_SOURCE_FILES);
#endif /* defined(COMPILE_MULTIPLE_SOURCE_FILES) */
#if defined(COMPILE_MULTIPLE_TRANSLATION_UNITS)
  define_numeric_valued_macro(COMPILE_MULTIPLE_TRANSLATION_UNITS);
#else /* !defined(COMPILE_MULTIPLE_TRANSLATION_UNITS) */
  comment_undefined_macro_name(COMPILE_MULTIPLE_TRANSLATION_UNITS);
#endif /* defined(COMPILE_MULTIPLE_TRANSLATION_UNITS) */
#if defined(COMPOUND_LITERAL_ENABLING_POSSIBLE)
  define_numeric_valued_macro(COMPOUND_LITERAL_ENABLING_POSSIBLE);
#else /* !defined(COMPOUND_LITERAL_ENABLING_POSSIBLE) */
  comment_undefined_macro_name(COMPOUND_LITERAL_ENABLING_POSSIBLE);
#endif /* defined(COMPOUND_LITERAL_ENABLING_POSSIBLE) */
#if defined(COROUTINE_ENABLING_POSSIBLE)
  define_numeric_valued_macro(COROUTINE_ENABLING_POSSIBLE);
#else /* !defined(COROUTINE_ENABLING_POSSIBLE) */
  comment_undefined_macro_name(COROUTINE_ENABLING_POSSIBLE);
#endif /* defined(COROUTINE_ENABLING_POSSIBLE) */
#if defined(CPP11_IL_EXTENSIONS_SUPPORTED)
  define_numeric_valued_macro(CPP11_IL_EXTENSIONS_SUPPORTED);
#else /* !defined(CPP11_IL_EXTENSIONS_SUPPORTED) */
  comment_undefined_macro_name(CPP11_IL_EXTENSIONS_SUPPORTED);
#endif /* defined(CPP11_IL_EXTENSIONS_SUPPORTED) */
#if defined(CPPCLI_ENABLING_POSSIBLE)
  define_numeric_valued_macro(CPPCLI_ENABLING_POSSIBLE);
#else /* !defined(CPPCLI_ENABLING_POSSIBLE) */
  comment_undefined_macro_name(CPPCLI_ENABLING_POSSIBLE);
#endif /* defined(CPPCLI_ENABLING_POSSIBLE) */
#if defined(CPPCLI_PORTABLE_ASSEMBLY_PATH)
  define_string_valued_macro(CPPCLI_PORTABLE_ASSEMBLY_PATH);
#else /* !defined(CPPCLI_PORTABLE_ASSEMBLY_PATH) */
  comment_undefined_macro_name(CPPCLI_PORTABLE_ASSEMBLY_PATH);
#endif /* defined(CPPCLI_PORTABLE_ASSEMBLY_PATH) */
#if defined(CPPCX_ENABLING_POSSIBLE)
  define_numeric_valued_macro(CPPCX_ENABLING_POSSIBLE);
#else /* !defined(CPPCX_ENABLING_POSSIBLE) */
  comment_undefined_macro_name(CPPCX_ENABLING_POSSIBLE);
#endif /* defined(CPPCX_ENABLING_POSSIBLE) */
#if defined(CPPCX_INCLUDE_PATH)
  define_string_valued_macro(CPPCX_INCLUDE_PATH);
#else /* !defined(CPPCX_INCLUDE_PATH) */
  comment_undefined_macro_name(CPPCX_INCLUDE_PATH);
#endif /* defined(CPPCX_INCLUDE_PATH) */
#if defined(CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT)
  define_numeric_valued_macro(CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT);
#else /* !defined(CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT) */
  comment_undefined_macro_name(CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT);
#endif /* defined(CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT) */
#if defined(CREATE_LEXICAL_TYPEREFS)
  define_numeric_valued_macro(CREATE_LEXICAL_TYPEREFS);
#else /* !defined(CREATE_LEXICAL_TYPEREFS) */
  comment_undefined_macro_name(CREATE_LEXICAL_TYPEREFS);
#endif /* defined(CREATE_LEXICAL_TYPEREFS) */
#if defined(CUSTOM_DEFAULT_OUTPUT_FILES)
  define_numeric_valued_macro(CUSTOM_DEFAULT_OUTPUT_FILES);
#else /* !defined(CUSTOM_DEFAULT_OUTPUT_FILES) */
  comment_undefined_macro_name(CUSTOM_DEFAULT_OUTPUT_FILES);
#endif /* defined(CUSTOM_DEFAULT_OUTPUT_FILES) */
#if defined(CUSTOM_NAME_LINKAGE_KINDS)
  /* We cannot conveniently display the value of CUSTOM_NAME_LINKAGE_KINDS
     because it contains embedded commas (it's inserted into the middle of
     a list of enumerators in il_def.h), which makes it unsuitable for
     passing as an argument to the output macros.  Instead, we output a
     #error directive so that if the output is captured and used blindly as
     a defines.h file, the user will be notified that the definition is
     missing. */
  fprintf(f_error,
          "#error -- CUSTOM_NAME_LINKAGE_KINDS must be set manually\n");
#else /* !defined(CUSTOM_NAME_LINKAGE_KINDS) */
  comment_undefined_macro_name(CUSTOM_NAME_LINKAGE_KINDS);
#endif /* defined(CUSTOM_NAME_LINKAGE_KINDS) */
#if defined(CUSTOM_NAME_LINKAGE_KIND_NAMES)
  /* We cannot conveniently display the value of
     CUSTOM_NAME_LINKAGE_KIND_NAMES because it contains embedded commas
     (it's inserted into the middle of an initializer list in il_def.h),
     which makes it unsuitable for passing as an argument to the output
     macros.  Instead, we output a #error directive so that if the output
     is captured and used blindly as a defines.h file, the user will be
     notified that the definition is missing. */
  fprintf(f_error,
          "#error -- CUSTOM_NAME_LINKAGE_KIND_NAMES must be set manually\n");
#else /* !defined(CUSTOM_NAME_LINKAGE_KIND_NAMES) */
  comment_undefined_macro_name(CUSTOM_NAME_LINKAGE_KIND_NAMES);
#endif /* defined(CUSTOM_NAME_LINKAGE_KIND_NAMES) */
#if defined(C_ANACHRONISMS_ALLOWED)
  define_numeric_valued_macro(C_ANACHRONISMS_ALLOWED);
#else /* !defined(C_ANACHRONISMS_ALLOWED) */
  comment_undefined_macro_name(C_ANACHRONISMS_ALLOWED);
#endif /* defined(C_ANACHRONISMS_ALLOWED) */
#if defined(C_GEN_BE_GENERATES_ANSI_C)
  define_numeric_valued_macro(C_GEN_BE_GENERATES_ANSI_C);
#else /* !defined(C_GEN_BE_GENERATES_ANSI_C) */
  comment_undefined_macro_name(C_GEN_BE_GENERATES_ANSI_C);
#endif /* defined(C_GEN_BE_GENERATES_ANSI_C) */
#if defined(C_GEN_BE_GENERATES_C23)
  define_numeric_valued_macro(C_GEN_BE_GENERATES_C23);
#else /* !defined(C_GEN_BE_GENERATES_C23) */
  comment_undefined_macro_name(C_GEN_BE_GENERATES_C23);
#endif /* defined(C_GEN_BE_GENERATES_C23) */
#if defined(DEBUG)
  define_numeric_valued_macro(DEBUG);
#else /* !defined(DEBUG) */
  comment_undefined_macro_name(DEBUG);
#endif /* defined(DEBUG) */
#if defined(DECL_MODIFIERS_IN_USE)
  define_numeric_valued_macro(DECL_MODIFIERS_IN_USE);
#else /* !defined(DECL_MODIFIERS_IN_USE) */
  comment_undefined_macro_name(DECL_MODIFIERS_IN_USE);
#endif /* defined(DECL_MODIFIERS_IN_USE) */
#if defined(DEFAULT_ADDRESS_OF_ELLIPSIS_ALLOWED)
  define_numeric_valued_macro(DEFAULT_ADDRESS_OF_ELLIPSIS_ALLOWED);
#else /* !defined(DEFAULT_ADDRESS_OF_ELLIPSIS_ALLOWED) */
  comment_undefined_macro_name(DEFAULT_ADDRESS_OF_ELLIPSIS_ALLOWED);
#endif /* defined(DEFAULT_ADDRESS_OF_ELLIPSIS_ALLOWED) */
#if defined(DEFAULT_ADD_MATCH_NOTES)
  define_numeric_valued_macro(DEFAULT_ADD_MATCH_NOTES);
#else /* !defined(DEFAULT_ADD_MATCH_NOTES) */
  comment_undefined_macro_name(DEFAULT_ADD_MATCH_NOTES);
#endif /* defined(DEFAULT_ADD_MATCH_NOTES) */
#if defined(DEFAULT_ALIAS_TEMPL_INTRINSICS_ENABLED)
  define_numeric_valued_macro(DEFAULT_ALIAS_TEMPL_INTRINSICS_ENABLED);
#else /* !defined(DEFAULT_ALIAS_TEMPL_INTRINSICS_ENABLED) */
  comment_undefined_macro_name(DEFAULT_ALIAS_TEMPL_INTRINSICS_ENABLED);
#endif /* defined(DEFAULT_ALIAS_TEMPL_INTRINSICS_ENABLED) */
#if defined(DEFAULT_TEMPL_TYPE_MEMBER_INTRINSICS_ENABLED)
  define_numeric_valued_macro(DEFAULT_TEMPL_TYPE_MEMBER_INTRINSICS_ENABLED);
#else /* !defined(DEFAULT_TEMPL_TYPE_MEMBER_INTRINSICS_ENABLED) */
  comment_undefined_macro_name(DEFAULT_TEMPL_TYPE_MEMBER_INTRINSICS_ENABLED);
#endif /* defined(DEFAULT_TEMPL_TYPE_MEMBER_INTRINSICS_ENABLED) */
#if defined(DEFAULT_ALLOW_ANACHRONISMS)
  define_numeric_valued_macro(DEFAULT_ALLOW_ANACHRONISMS);
#else /* !defined(DEFAULT_ALLOW_ANACHRONISMS) */
  comment_undefined_macro_name(DEFAULT_ALLOW_ANACHRONISMS);
#endif /* defined(DEFAULT_ALLOW_ANACHRONISMS) */
#if defined(DEFAULT_ALLOW_COPY_ASSIGNMENT_OP_WITH_BASE_CLASS_PARAM)
  define_numeric_valued_macro(
                       DEFAULT_ALLOW_COPY_ASSIGNMENT_OP_WITH_BASE_CLASS_PARAM);
#else /* !defined(DEFAULT_ALLOW_COPY_ASSIGNMENT_OP_WITH_BASE_CLASS_PARAM) */
  comment_undefined_macro_name(
                       DEFAULT_ALLOW_COPY_ASSIGNMENT_OP_WITH_BASE_CLASS_PARAM);
#endif /* defined(DEFAULT_ALLOW_COPY_ASSIGNMENT_OP_WITH_BASE_CLASS_PARAM) */
#if defined(DEFAULT_ALLOW_DOLLAR_IN_ID_CHARS)
  define_numeric_valued_macro(DEFAULT_ALLOW_DOLLAR_IN_ID_CHARS);
#else /* !defined(DEFAULT_ALLOW_DOLLAR_IN_ID_CHARS) */
  comment_undefined_macro_name(DEFAULT_ALLOW_DOLLAR_IN_ID_CHARS);
#endif /* defined(DEFAULT_ALLOW_DOLLAR_IN_ID_CHARS) */
#if defined(DEFAULT_ALLOW_ELLIPSIS_ONLY_PARAM_IN_C_MODE)
  define_numeric_valued_macro(DEFAULT_ALLOW_ELLIPSIS_ONLY_PARAM_IN_C_MODE);
#else /* !defined(DEFAULT_ALLOW_ELLIPSIS_ONLY_PARAM_IN_C_MODE) */
  comment_undefined_macro_name(DEFAULT_ALLOW_ELLIPSIS_ONLY_PARAM_IN_C_MODE);
#endif /* defined(DEFAULT_ALLOW_ELLIPSIS_ONLY_PARAM_IN_C_MODE) */
#if defined(DEFAULT_ALLOW_NONCONST_CALL_ANACHRONISM)
  define_numeric_valued_macro(DEFAULT_ALLOW_NONCONST_CALL_ANACHRONISM);
#else /* !defined(DEFAULT_ALLOW_NONCONST_CALL_ANACHRONISM) */
  comment_undefined_macro_name(DEFAULT_ALLOW_NONCONST_CALL_ANACHRONISM);
#endif /* defined(DEFAULT_ALLOW_NONCONST_CALL_ANACHRONISM) */
#if defined(DEFAULT_ALLOW_NONCONST_REF_ANACHRONISM)
  define_numeric_valued_macro(DEFAULT_ALLOW_NONCONST_REF_ANACHRONISM);
#else /* !defined(DEFAULT_ALLOW_NONCONST_REF_ANACHRONISM) */
  comment_undefined_macro_name(DEFAULT_ALLOW_NONCONST_REF_ANACHRONISM);
#endif /* defined(DEFAULT_ALLOW_NONCONST_REF_ANACHRONISM) */
#if defined(DEFAULT_ALLOW_NONSTANDARD_ANONYMOUS_UNIONS)
  define_numeric_valued_macro(DEFAULT_ALLOW_NONSTANDARD_ANONYMOUS_UNIONS);
#else /* !defined(DEFAULT_ALLOW_NONSTANDARD_ANONYMOUS_UNIONS) */
  comment_undefined_macro_name(DEFAULT_ALLOW_NONSTANDARD_ANONYMOUS_UNIONS);
#endif /* defined(DEFAULT_ALLOW_NONSTANDARD_ANONYMOUS_UNIONS) */
#if defined(DEFAULT_ALTERNATIVE_TOKENS_ALLOWED)
  define_numeric_valued_macro(DEFAULT_ALTERNATIVE_TOKENS_ALLOWED);
#else /* !defined(DEFAULT_ALTERNATIVE_TOKENS_ALLOWED) */
  comment_undefined_macro_name(DEFAULT_ALTERNATIVE_TOKENS_ALLOWED);
#endif /* defined(DEFAULT_ALTERNATIVE_TOKENS_ALLOWED) */
#if defined(DEFAULT_ALWAYS_FOLD_CALLS_TO_BUILTIN_CONSTANT_P)
  define_numeric_valued_macro(DEFAULT_ALWAYS_FOLD_CALLS_TO_BUILTIN_CONSTANT_P);
#else /* !defined(DEFAULT_ALWAYS_FOLD_CALLS_TO_BUILTIN_CONSTANT_P) */
  comment_undefined_macro_name(
                              DEFAULT_ALWAYS_FOLD_CALLS_TO_BUILTIN_CONSTANT_P);
#endif /* defined(DEFAULT_ALWAYS_FOLD_CALLS_TO_BUILTIN_CONSTANT_P) */
#if defined(DEFAULT_ARG_DEPENDENT_LOOKUP)
  define_numeric_valued_macro(DEFAULT_ARG_DEPENDENT_LOOKUP);
#else /* !defined(DEFAULT_ARG_DEPENDENT_LOOKUP) */
  comment_undefined_macro_name(DEFAULT_ARG_DEPENDENT_LOOKUP);
#endif /* defined(DEFAULT_ARG_DEPENDENT_LOOKUP) */
#if defined(DEFAULT_ARRAY_NEW_AND_DELETE_ENABLED)
  define_numeric_valued_macro(DEFAULT_ARRAY_NEW_AND_DELETE_ENABLED);
#else /* !defined(DEFAULT_ARRAY_NEW_AND_DELETE_ENABLED) */
  comment_undefined_macro_name(DEFAULT_ARRAY_NEW_AND_DELETE_ENABLED);
#endif /* defined(DEFAULT_ARRAY_NEW_AND_DELETE_ENABLED) */
#if defined(DEFAULT_AUTOMATIC_INSTANTIATION_MODE)
  define_numeric_valued_macro(DEFAULT_AUTOMATIC_INSTANTIATION_MODE);
#else /* !defined(DEFAULT_AUTOMATIC_INSTANTIATION_MODE) */
  comment_undefined_macro_name(DEFAULT_AUTOMATIC_INSTANTIATION_MODE);
#endif /* defined(DEFAULT_AUTOMATIC_INSTANTIATION_MODE) */
#if defined(DEFAULT_AUTO_STORAGE_CLASS_SPECIFIER_ENABLED)
  define_numeric_valued_macro(DEFAULT_AUTO_STORAGE_CLASS_SPECIFIER_ENABLED);
#else /* !defined(DEFAULT_AUTO_STORAGE_CLASS_SPECIFIER_ENABLED) */
  comment_undefined_macro_name(DEFAULT_AUTO_STORAGE_CLASS_SPECIFIER_ENABLED);
#endif /* defined(DEFAULT_AUTO_STORAGE_CLASS_SPECIFIER_ENABLED) */
#if defined(DEFAULT_AUTO_TYPE_SPECIFIER_ENABLED)
  define_numeric_valued_macro(DEFAULT_AUTO_TYPE_SPECIFIER_ENABLED);
#else /* !defined(DEFAULT_AUTO_TYPE_SPECIFIER_ENABLED) */
  comment_undefined_macro_name(DEFAULT_AUTO_TYPE_SPECIFIER_ENABLED);
#endif /* defined(DEFAULT_AUTO_TYPE_SPECIFIER_ENABLED) */
#if defined(DEFAULT_BOOL_IS_KEYWORD)
  define_numeric_valued_macro(DEFAULT_BOOL_IS_KEYWORD);
#else /* !defined(DEFAULT_BOOL_IS_KEYWORD) */
  comment_undefined_macro_name(DEFAULT_BOOL_IS_KEYWORD);
#endif /* defined(DEFAULT_BOOL_IS_KEYWORD) */
#if defined(DEFAULT_BRIEF_DIAGNOSTICS)
  define_numeric_valued_macro(DEFAULT_BRIEF_DIAGNOSTICS);
#else /* !defined(DEFAULT_BRIEF_DIAGNOSTICS) */
  comment_undefined_macro_name(DEFAULT_BRIEF_DIAGNOSTICS);
#endif /* defined(DEFAULT_BRIEF_DIAGNOSTICS) */
#if defined(DEFAULT_C99_MODE)
  define_numeric_valued_macro(DEFAULT_C99_MODE);
#else /* !defined(DEFAULT_C99_MODE) */
  comment_undefined_macro_name(DEFAULT_C99_MODE);
#endif /* defined(DEFAULT_C99_MODE) */
#if defined(DEFAULT_CHECK_CONCATENATIONS)
  define_numeric_valued_macro(DEFAULT_CHECK_CONCATENATIONS);
#else /* !defined(DEFAULT_CHECK_CONCATENATIONS) */
  comment_undefined_macro_name(DEFAULT_CHECK_CONCATENATIONS);
#endif /* defined(DEFAULT_CHECK_CONCATENATIONS) */
#if defined(DEFAULT_CHECK_FOR_BYTE_ORDER_MARK)
  define_numeric_valued_macro(DEFAULT_CHECK_FOR_BYTE_ORDER_MARK);
#else /* !defined(DEFAULT_CHECK_FOR_BYTE_ORDER_MARK) */
  comment_undefined_macro_name(DEFAULT_CHECK_FOR_BYTE_ORDER_MARK);
#endif /* defined(DEFAULT_CHECK_FOR_BYTE_ORDER_MARK) */
#if defined(DEFAULT_CHECK_PRINTF_SCANF_POSITIONAL_ARGS)
  define_numeric_valued_macro(DEFAULT_CHECK_PRINTF_SCANF_POSITIONAL_ARGS);
#else /* !defined(DEFAULT_CHECK_PRINTF_SCANF_POSITIONAL_ARGS) */
  comment_undefined_macro_name(DEFAULT_CHECK_PRINTF_SCANF_POSITIONAL_ARGS);
#endif /* defined(DEFAULT_CHECK_PRINTF_SCANF_POSITIONAL_ARGS) */
#if defined(DEFAULT_CLANG_COMPATIBILITY)
  define_numeric_valued_macro(DEFAULT_CLANG_COMPATIBILITY);
#else /* !defined(DEFAULT_CLANG_COMPATIBILITY) */
  comment_undefined_macro_name(DEFAULT_CLANG_COMPATIBILITY);
#endif /* defined(DEFAULT_CLANG_COMPATIBILITY) */
#if defined(DEFAULT_CLANG_VERSION)
  define_numeric_valued_macro(DEFAULT_CLANG_VERSION);
#else /* !defined(DEFAULT_CLANG_VERSION) */
  comment_undefined_macro_name(DEFAULT_CLANG_VERSION);
#endif /* defined(DEFAULT_CLANG_VERSION) */
#if defined(DEFAULT_CLASS_NAME_INJECTION)
  define_numeric_valued_macro(DEFAULT_CLASS_NAME_INJECTION);
#else /* !defined(DEFAULT_CLASS_NAME_INJECTION) */
  comment_undefined_macro_name(DEFAULT_CLASS_NAME_INJECTION);
#endif /* defined(DEFAULT_CLASS_NAME_INJECTION) */
#if defined(DEFAULT_COMPOUND_LITERALS_ALLOWED)
  define_numeric_valued_macro(DEFAULT_COMPOUND_LITERALS_ALLOWED);
#else /* !defined(DEFAULT_COMPOUND_LITERALS_ALLOWED) */
  comment_undefined_macro_name(DEFAULT_COMPOUND_LITERALS_ALLOWED);
#endif /* defined(DEFAULT_COMPOUND_LITERALS_ALLOWED) */
#if defined(DEFAULT_COMPRESS_MANGLED_NAMES)
  define_numeric_valued_macro(DEFAULT_COMPRESS_MANGLED_NAMES);
#else /* !defined(DEFAULT_COMPRESS_MANGLED_NAMES) */
  comment_undefined_macro_name(DEFAULT_COMPRESS_MANGLED_NAMES);
#endif /* defined(DEFAULT_COMPRESS_MANGLED_NAMES) */
#if defined(DEFAULT_CONTEXT_LIMIT)
  define_numeric_valued_macro(DEFAULT_CONTEXT_LIMIT);
#else /* !defined(DEFAULT_CONTEXT_LIMIT) */
  comment_undefined_macro_name(DEFAULT_CONTEXT_LIMIT);
#endif /* defined(DEFAULT_CONTEXT_LIMIT) */
#if defined(DEFAULT_CPP11_DEPENDENT_NAME_PROCESSING)
  define_numeric_valued_macro(DEFAULT_CPP11_DEPENDENT_NAME_PROCESSING);
#else /* !defined(DEFAULT_CPP11_DEPENDENT_NAME_PROCESSING) */
  comment_undefined_macro_name(DEFAULT_CPP11_DEPENDENT_NAME_PROCESSING);
#endif /* defined(DEFAULT_CPP11_DEPENDENT_NAME_PROCESSING) */
#if defined(DEFAULT_CPP11_SFINAE_ENABLED)
  define_numeric_valued_macro(DEFAULT_CPP11_SFINAE_ENABLED);
#else /* !defined(DEFAULT_CPP11_SFINAE_ENABLED) */
  comment_undefined_macro_name(DEFAULT_CPP11_SFINAE_ENABLED);
#endif /* defined(DEFAULT_CPP11_SFINAE_ENABLED) */
#if defined(DEFAULT_CPP11_SFINAE_IGNORE_ACCESS)
  define_numeric_valued_macro(DEFAULT_CPP11_SFINAE_IGNORE_ACCESS);
#else /* !defined(DEFAULT_CPP11_SFINAE_IGNORE_ACCESS) */
  comment_undefined_macro_name(DEFAULT_CPP11_SFINAE_IGNORE_ACCESS);
#endif /* defined(DEFAULT_CPP11_SFINAE_IGNORE_ACCESS) */
#if defined(DEFAULT_CPPCLI_CPPCX_VERSION)
  define_numeric_valued_macro(DEFAULT_CPPCLI_CPPCX_VERSION);
#else /* !defined(DEFAULT_CPPCLI_CPPCX_VERSION) */
  comment_undefined_macro_name(DEFAULT_CPPCLI_CPPCX_VERSION);
#endif /* defined(DEFAULT_CPPCLI_CPPCX_VERSION) */
#if defined(DEFAULT_CPPCLI_ENABLED)
  define_numeric_valued_macro(DEFAULT_CPPCLI_ENABLED);
#else /* !defined(DEFAULT_CPPCLI_ENABLED) */
  comment_undefined_macro_name(DEFAULT_CPPCLI_ENABLED);
#endif /* defined(DEFAULT_CPPCLI_ENABLED) */
#if defined(DEFAULT_CPPCX_ENABLED)
  define_numeric_valued_macro(DEFAULT_CPPCX_ENABLED);
#else /* !defined(DEFAULT_CPPCX_ENABLED) */
  comment_undefined_macro_name(DEFAULT_CPPCX_ENABLED);
#endif /* defined(DEFAULT_CPPCX_ENABLED) */
#if defined(DEFAULT_CPP_MODE)
  define_numeric_valued_macro(DEFAULT_CPP_MODE);
#else /* !defined(DEFAULT_CPP_MODE) */
  comment_undefined_macro_name(DEFAULT_CPP_MODE);
#endif /* defined(DEFAULT_CPP_MODE) */
#if defined(DEFAULT_C_AND_CPP_FUNCTION_TYPES_ARE_DISTINCT)
  define_numeric_valued_macro(DEFAULT_C_AND_CPP_FUNCTION_TYPES_ARE_DISTINCT);
#else /* !defined(DEFAULT_C_AND_CPP_FUNCTION_TYPES_ARE_DISTINCT) */
  comment_undefined_macro_name(DEFAULT_C_AND_CPP_FUNCTION_TYPES_ARE_DISTINCT);
#endif /* defined(DEFAULT_C_AND_CPP_FUNCTION_TYPES_ARE_DISTINCT) */
#if defined(DEFAULT_DEPENDENT_LOOKUP_FINDS_STATIC_FUNCTIONS)
  define_numeric_valued_macro(DEFAULT_DEPENDENT_LOOKUP_FINDS_STATIC_FUNCTIONS);
#else /* !defined(DEFAULT_DEPENDENT_LOOKUP_FINDS_STATIC_FUNCTIONS) */
  comment_undefined_macro_name(
                              DEFAULT_DEPENDENT_LOOKUP_FINDS_STATIC_FUNCTIONS);
#endif /* defined(DEFAULT_DEPENDENT_LOOKUP_FINDS_STATIC_FUNCTIONS) */
#if defined(DEFAULT_DEPENDENT_NAME_PROCESSING)
  define_numeric_valued_macro(DEFAULT_DEPENDENT_NAME_PROCESSING);
#else /* !defined(DEFAULT_DEPENDENT_NAME_PROCESSING) */
  comment_undefined_macro_name(DEFAULT_DEPENDENT_NAME_PROCESSING);
#endif /* defined(DEFAULT_DEPENDENT_NAME_PROCESSING) */
#if defined(DEFAULT_DEPRECATED_STRING_LITERAL_CONV_ALLOWED)
  define_numeric_valued_macro(DEFAULT_DEPRECATED_STRING_LITERAL_CONV_ALLOWED);
#else /* !defined(DEFAULT_DEPRECATED_STRING_LITERAL_CONV_ALLOWED) */
  comment_undefined_macro_name(DEFAULT_DEPRECATED_STRING_LITERAL_CONV_ALLOWED);
#endif /* defined(DEFAULT_DEPRECATED_STRING_LITERAL_CONV_ALLOWED) */
#if defined(DEFAULT_DESIGNATORS_ALLOWED)
  define_numeric_valued_macro(DEFAULT_DESIGNATORS_ALLOWED);
#else /* !defined(DEFAULT_DESIGNATORS_ALLOWED) */
  comment_undefined_macro_name(DEFAULT_DESIGNATORS_ALLOWED);
#endif /* defined(DEFAULT_DESIGNATORS_ALLOWED) */
#if defined(DEFAULT_DIAG_OVERRIDE_DOES_NOT_AFFECT_SFINAE)
  define_numeric_valued_macro(DEFAULT_DIAG_OVERRIDE_DOES_NOT_AFFECT_SFINAE);
#else /* !defined(DEFAULT_DIAG_OVERRIDE_DOES_NOT_AFFECT_SFINAE) */
  comment_undefined_macro_name(DEFAULT_DIAG_OVERRIDE_DOES_NOT_AFFECT_SFINAE);
#endif /* defined(DEFAULT_DIAG_OVERRIDE_DOES_NOT_AFFECT_SFINAE) */
#if defined(DEFAULT_DISABLE_ACCESS_CHECKING_IN_MICROSOFT_ENUM_BASES)
  define_numeric_valued_macro(
                     DEFAULT_DISABLE_ACCESS_CHECKING_IN_MICROSOFT_ENUM_BASES);
#else /* !defined(DEFAULT_DISABLE_ACCESS_CHECKING_IN_MICROSOFT_ENUM_BASES) */
  comment_undefined_macro_name(
                     DEFAULT_DISABLE_ACCESS_CHECKING_IN_MICROSOFT_ENUM_BASES);
#endif /* defined(DEFAULT_DISABLE_ACCESS_CHECKING_IN_MICROSOFT_ENUM_BASES) */
#if defined(DEFAULT_DISPLAY_ERROR_CONTEXT_ON_CATASTROPHE)
  define_numeric_valued_macro(DEFAULT_DISPLAY_ERROR_CONTEXT_ON_CATASTROPHE);
#else /* !defined(DEFAULT_DISPLAY_ERROR_CONTEXT_ON_CATASTROPHE) */
  comment_undefined_macro_name(DEFAULT_DISPLAY_ERROR_CONTEXT_ON_CATASTROPHE);
#endif /* defined(DEFAULT_DISPLAY_ERROR_CONTEXT_ON_CATASTROPHE) */
#if defined(DEFAULT_DISPLAY_ERROR_NUMBER)
  define_numeric_valued_macro(DEFAULT_DISPLAY_ERROR_NUMBER);
#else /* !defined(DEFAULT_DISPLAY_ERROR_NUMBER) */
  comment_undefined_macro_name(DEFAULT_DISPLAY_ERROR_NUMBER);
#endif /* defined(DEFAULT_DISPLAY_ERROR_NUMBER) */
#if defined(DEFAULT_DISPLAY_TEMPLATE_TYPEDEFS_IN_DIAGNOSTICS)
  define_numeric_valued_macro(
                             DEFAULT_DISPLAY_TEMPLATE_TYPEDEFS_IN_DIAGNOSTICS);
#else /* !defined(DEFAULT_DISPLAY_TEMPLATE_TYPEDEFS_IN_DIAGNOSTICS) */
  comment_undefined_macro_name(
                             DEFAULT_DISPLAY_TEMPLATE_TYPEDEFS_IN_DIAGNOSTICS);
#endif /* defined(DEFAULT_DISPLAY_TEMPLATE_TYPEDEFS_IN_DIAGNOSTICS) */
#if defined(DEFAULT_DISTINCT_TEMPLATE_SIGNATURES)
  define_numeric_valued_macro(DEFAULT_DISTINCT_TEMPLATE_SIGNATURES);
#else /* !defined(DEFAULT_DISTINCT_TEMPLATE_SIGNATURES) */
  comment_undefined_macro_name(DEFAULT_DISTINCT_TEMPLATE_SIGNATURES);
#endif /* defined(DEFAULT_DISTINCT_TEMPLATE_SIGNATURES) */
#if defined(DEFAULT_DO_LATE_OVL_RES_TIEBREAKER)
  define_numeric_valued_macro(DEFAULT_DO_LATE_OVL_RES_TIEBREAKER);
#else /* !defined(DEFAULT_DO_LATE_OVL_RES_TIEBREAKER) */
  comment_undefined_macro_name(DEFAULT_DO_LATE_OVL_RES_TIEBREAKER);
#endif /* defined(DEFAULT_DO_LATE_OVL_RES_TIEBREAKER) */
#if defined(DEFAULT_EDG_BASE)
  define_string_valued_macro(DEFAULT_EDG_BASE);
#else /* !defined(DEFAULT_EDG_BASE) */
  comment_undefined_macro_name(DEFAULT_EDG_BASE);
#endif /* defined(DEFAULT_EDG_BASE) */
#if defined(DEFAULT_EDG_COLORS)
  define_string_valued_macro(DEFAULT_EDG_COLORS);
#else /* !defined(DEFAULT_EDG_COLORS) */
  comment_undefined_macro_name(DEFAULT_EDG_COLORS);
#endif /* defined(DEFAULT_EDG_COLORS) */
#if defined(DEFAULT_EMBEDDED_C_ENABLED)
  define_numeric_valued_macro(DEFAULT_EMBEDDED_C_ENABLED);
#else /* !defined(DEFAULT_EMBEDDED_C_ENABLED) */
  comment_undefined_macro_name(DEFAULT_EMBEDDED_C_ENABLED);
#endif /* defined(DEFAULT_EMBEDDED_C_ENABLED) */
#if defined(DEFAULT_EMULATE_GNU_ABI_BUGS)
  define_numeric_valued_macro(DEFAULT_EMULATE_GNU_ABI_BUGS);
#else /* !defined(DEFAULT_EMULATE_GNU_ABI_BUGS) */
  comment_undefined_macro_name(DEFAULT_EMULATE_GNU_ABI_BUGS);
#endif /* defined(DEFAULT_EMULATE_GNU_ABI_BUGS) */
#if defined(DEFAULT_EMULATE_GNU_VALUE_INITIALIZATION_BUGS)
  define_numeric_valued_macro(DEFAULT_EMULATE_GNU_VALUE_INITIALIZATION_BUGS);
#else /* !defined(DEFAULT_EMULATE_GNU_VALUE_INITIALIZATION_BUGS) */
  comment_undefined_macro_name(DEFAULT_EMULATE_GNU_VALUE_INITIALIZATION_BUGS);
#endif /* defined(DEFAULT_EMULATE_GNU_VALUE_INITIALIZATION_BUGS) */
#if defined(DEFAULT_EMULATE_MSVC_VALUE_INITIALIZATION_BUGS)
  define_numeric_valued_macro(DEFAULT_EMULATE_MSVC_VALUE_INITIALIZATION_BUGS);
#else /* !defined(DEFAULT_EMULATE_MSVC_VALUE_INITIALIZATION_BUGS) */
  comment_undefined_macro_name(DEFAULT_EMULATE_MSVC_VALUE_INITIALIZATION_BUGS);
#endif /* defined(DEFAULT_EMULATE_MSVC_VALUE_INITIALIZATION_BUGS) */
#if defined(DEFAULT_ENABLE_COLORIZED_DIAGNOSTICS)
  define_numeric_valued_macro(DEFAULT_ENABLE_COLORIZED_DIAGNOSTICS);
#else /* !defined(DEFAULT_ENABLE_COLORIZED_DIAGNOSTICS) */
  comment_undefined_macro_name(DEFAULT_ENABLE_COLORIZED_DIAGNOSTICS);
#endif /* defined(DEFAULT_ENABLE_COLORIZED_DIAGNOSTICS) */
#if defined(DEFAULT_EXCEPTIONS_ENABLED)
  define_numeric_valued_macro(DEFAULT_EXCEPTIONS_ENABLED);
#else /* !defined(DEFAULT_EXCEPTIONS_ENABLED) */
  comment_undefined_macro_name(DEFAULT_EXCEPTIONS_ENABLED);
#endif /* defined(DEFAULT_EXCEPTIONS_ENABLED) */
#if defined(DEFAULT_EXPLICIT_KEYWORD_ENABLED)
  define_numeric_valued_macro(DEFAULT_EXPLICIT_KEYWORD_ENABLED);
#else /* !defined(DEFAULT_EXPLICIT_KEYWORD_ENABLED) */
  comment_undefined_macro_name(DEFAULT_EXPLICIT_KEYWORD_ENABLED);
#endif /* defined(DEFAULT_EXPLICIT_KEYWORD_ENABLED) */
#if defined(DEFAULT_EXPORT_TEMPLATE_ALLOWED)
  define_numeric_valued_macro(DEFAULT_EXPORT_TEMPLATE_ALLOWED);
#else /* !defined(DEFAULT_EXPORT_TEMPLATE_ALLOWED) */
  comment_undefined_macro_name(DEFAULT_EXPORT_TEMPLATE_ALLOWED);
#endif /* defined(DEFAULT_EXPORT_TEMPLATE_ALLOWED) */
#if defined(DEFAULT_EXTENDED_DESIGNATORS_ALLOWED)
  define_numeric_valued_macro(DEFAULT_EXTENDED_DESIGNATORS_ALLOWED);
#else /* !defined(DEFAULT_EXTENDED_DESIGNATORS_ALLOWED) */
  comment_undefined_macro_name(DEFAULT_EXTENDED_DESIGNATORS_ALLOWED);
#endif /* defined(DEFAULT_EXTENDED_DESIGNATORS_ALLOWED) */
#if defined(DEFAULT_EXTENDED_VARIADIC_MACROS_ALLOWED)
  define_numeric_valued_macro(DEFAULT_EXTENDED_VARIADIC_MACROS_ALLOWED);
#else /* !defined(DEFAULT_EXTENDED_VARIADIC_MACROS_ALLOWED) */
  comment_undefined_macro_name(DEFAULT_EXTENDED_VARIADIC_MACROS_ALLOWED);
#endif /* defined(DEFAULT_EXTENDED_VARIADIC_MACROS_ALLOWED) */
#if defined(DEFAULT_EXTERN_INLINE_ALLOWED)
  define_numeric_valued_macro(DEFAULT_EXTERN_INLINE_ALLOWED);
#else /* !defined(DEFAULT_EXTERN_INLINE_ALLOWED) */
  comment_undefined_macro_name(DEFAULT_EXTERN_INLINE_ALLOWED);
#endif /* defined(DEFAULT_EXTERN_INLINE_ALLOWED) */
#if defined(DEFAULT_FAR_CODE_POINTERS)
  define_numeric_valued_macro(DEFAULT_FAR_CODE_POINTERS);
#else /* !defined(DEFAULT_FAR_CODE_POINTERS) */
  comment_undefined_macro_name(DEFAULT_FAR_CODE_POINTERS);
#endif /* defined(DEFAULT_FAR_CODE_POINTERS) */
#if defined(DEFAULT_FAR_DATA_POINTERS)
  define_numeric_valued_macro(DEFAULT_FAR_DATA_POINTERS);
#else /* !defined(DEFAULT_FAR_DATA_POINTERS) */
  comment_undefined_macro_name(DEFAULT_FAR_DATA_POINTERS);
#endif /* defined(DEFAULT_FAR_DATA_POINTERS) */
#if defined(DEFAULT_FIXED_POINT_ENABLED)
  define_numeric_valued_macro(DEFAULT_FIXED_POINT_ENABLED);
#else /* !defined(DEFAULT_FIXED_POINT_ENABLED) */
  comment_undefined_macro_name(DEFAULT_FIXED_POINT_ENABLED);
#endif /* defined(DEFAULT_FIXED_POINT_ENABLED) */
#if defined(DEFAULT_FLOATING_POINT_TEMPLATE_PARAMETERS_ALLOWED)
  define_numeric_valued_macro(
                           DEFAULT_FLOATING_POINT_TEMPLATE_PARAMETERS_ALLOWED);
#else /* !defined(DEFAULT_FLOATING_POINT_TEMPLATE_PARAMETERS_ALLOWED) */
  comment_undefined_macro_name(
                           DEFAULT_FLOATING_POINT_TEMPLATE_PARAMETERS_ALLOWED);
#endif /* defined(DEFAULT_FLOATING_POINT_TEMPLATE_PARAMETERS_ALLOWED) */
#if defined(DEFAULT_FLOAT_KIND_FOR_FLOAT128)
  define_string_valued_macro(DEFAULT_FLOAT_KIND_FOR_FLOAT128);
#else /* !defined(DEFAULT_FLOAT_KIND_FOR_FLOAT128) */
  comment_undefined_macro_name(DEFAULT_FLOAT_KIND_FOR_FLOAT128);
#endif /* defined(DEFAULT_FLOAT_KIND_FOR_FLOAT128) */
#if defined(DEFAULT_FLOAT_KIND_FOR_FLOAT80)
  define_string_valued_macro(DEFAULT_FLOAT_KIND_FOR_FLOAT80);
#else /* !defined(DEFAULT_FLOAT_KIND_FOR_FLOAT80) */
  comment_undefined_macro_name(DEFAULT_FLOAT_KIND_FOR_FLOAT80);
#endif /* defined(DEFAULT_FLOAT_KIND_FOR_FLOAT80) */
#if defined(DEFAULT_FRIEND_INJECTION)
  define_numeric_valued_macro(DEFAULT_FRIEND_INJECTION);
#else /* !defined(DEFAULT_FRIEND_INJECTION) */
  comment_undefined_macro_name(DEFAULT_FRIEND_INJECTION);
#endif /* defined(DEFAULT_FRIEND_INJECTION) */
#if defined(DEFAULT_GCC_CONST_VARIABLES_ALLOWED)
  define_numeric_valued_macro(DEFAULT_GCC_CONST_VARIABLES_ALLOWED);
#else /* !defined(DEFAULT_GCC_CONST_VARIABLES_ALLOWED) */
  comment_undefined_macro_name(DEFAULT_GCC_CONST_VARIABLES_ALLOWED);
#endif /* defined(DEFAULT_GCC_CONST_VARIABLES_ALLOWED) */
#if defined(DEFAULT_GENERIC_ARITY_OVERLOAD_ALLOWED)
  define_numeric_valued_macro(DEFAULT_GENERIC_ARITY_OVERLOAD_ALLOWED);
#else /* !defined(DEFAULT_GENERIC_ARITY_OVERLOAD_ALLOWED) */
  comment_undefined_macro_name(DEFAULT_GENERIC_ARITY_OVERLOAD_ALLOWED);
#endif /* defined(DEFAULT_GENERIC_ARITY_OVERLOAD_ALLOWED) */
#if defined(DEFAULT_GNU_ABI_VERSION)
  define_numeric_valued_macro(DEFAULT_GNU_ABI_VERSION);
#else /* !defined(DEFAULT_GNU_ABI_VERSION) */
  comment_undefined_macro_name(DEFAULT_GNU_ABI_VERSION);
#endif /* defined(DEFAULT_GNU_ABI_VERSION) */
#if defined(DEFAULT_GNU_COMPATIBILITY)
  define_numeric_valued_macro(DEFAULT_GNU_COMPATIBILITY);
#else /* !defined(DEFAULT_GNU_COMPATIBILITY) */
  comment_undefined_macro_name(DEFAULT_GNU_COMPATIBILITY);
#endif /* defined(DEFAULT_GNU_COMPATIBILITY) */
#if defined(DEFAULT_GNU_STDC_ZERO_IN_SYSTEM_HEADERS)
  define_numeric_valued_macro(DEFAULT_GNU_STDC_ZERO_IN_SYSTEM_HEADERS);
#else /* !defined(DEFAULT_GNU_STDC_ZERO_IN_SYSTEM_HEADERS) */
  comment_undefined_macro_name(DEFAULT_GNU_STDC_ZERO_IN_SYSTEM_HEADERS);
#endif /* defined(DEFAULT_GNU_STDC_ZERO_IN_SYSTEM_HEADERS) */
#if defined(DEFAULT_GNU_VERSION)
  define_numeric_valued_macro(DEFAULT_GNU_VERSION);
#else /* !defined(DEFAULT_GNU_VERSION) */
  comment_undefined_macro_name(DEFAULT_GNU_VERSION);
#endif /* defined(DEFAULT_GNU_VERSION) */
#if defined(DEFAULT_GNU_VISIBILITY_ATTRIBUTE_ENABLED)
  define_numeric_valued_macro(DEFAULT_GNU_VISIBILITY_ATTRIBUTE_ENABLED);
#else /* !defined(DEFAULT_GNU_VISIBILITY_ATTRIBUTE_ENABLED) */
  comment_undefined_macro_name(DEFAULT_GNU_VISIBILITY_ATTRIBUTE_ENABLED);
#endif /* defined(DEFAULT_GNU_VISIBILITY_ATTRIBUTE_ENABLED) */
#if defined(DEFAULT_GUIDING_DECLS_ALLOWED)
  define_numeric_valued_macro(DEFAULT_GUIDING_DECLS_ALLOWED);
#else /* !defined(DEFAULT_GUIDING_DECLS_ALLOWED) */
  comment_undefined_macro_name(DEFAULT_GUIDING_DECLS_ALLOWED);
#endif /* defined(DEFAULT_GUIDING_DECLS_ALLOWED) */
#if defined(DEFAULT_IMPLICIT_NOEXCEPT_ENABLED)
  define_numeric_valued_macro(DEFAULT_IMPLICIT_NOEXCEPT_ENABLED);
#else /* !defined(DEFAULT_IMPLICIT_NOEXCEPT_ENABLED) */
  comment_undefined_macro_name(DEFAULT_IMPLICIT_NOEXCEPT_ENABLED);
#endif /* defined(DEFAULT_IMPLICIT_NOEXCEPT_ENABLED) */
#if defined(DEFAULT_IMPLICIT_TEMPLATE_INCLUSION_MODE)
  define_numeric_valued_macro(DEFAULT_IMPLICIT_TEMPLATE_INCLUSION_MODE);
#else /* !defined(DEFAULT_IMPLICIT_TEMPLATE_INCLUSION_MODE) */
  comment_undefined_macro_name(DEFAULT_IMPLICIT_TEMPLATE_INCLUSION_MODE);
#endif /* defined(DEFAULT_IMPLICIT_TEMPLATE_INCLUSION_MODE) */
#if defined(DEFAULT_IMPLICIT_TYPENAME_ENABLED)
  define_numeric_valued_macro(DEFAULT_IMPLICIT_TYPENAME_ENABLED);
#else /* !defined(DEFAULT_IMPLICIT_TYPENAME_ENABLED) */
  comment_undefined_macro_name(DEFAULT_IMPLICIT_TYPENAME_ENABLED);
#endif /* defined(DEFAULT_IMPLICIT_TYPENAME_ENABLED) */
#if defined(DEFAULT_IMPLICIT_USING_STD)
  define_numeric_valued_macro(DEFAULT_IMPLICIT_USING_STD);
#else /* !defined(DEFAULT_IMPLICIT_USING_STD) */
  comment_undefined_macro_name(DEFAULT_IMPLICIT_USING_STD);
#endif /* defined(DEFAULT_IMPLICIT_USING_STD) */
#if defined(DEFAULT_IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_ALLOWED)
  define_numeric_valued_macro(
                    DEFAULT_IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_ALLOWED);
#else /* !defined(DEFAULT_IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_ALLOWED) */
  comment_undefined_macro_name(
                    DEFAULT_IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_ALLOWED);
#endif /* defined(DEFAULT_IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_ALLOWED) */
#if defined(DEFAULT_INCLUDE_FILE_SUFFIX_LIST)
  define_string_valued_macro(DEFAULT_INCLUDE_FILE_SUFFIX_LIST);
#else /* !defined(DEFAULT_INCLUDE_FILE_SUFFIX_LIST) */
  comment_undefined_macro_name(DEFAULT_INCLUDE_FILE_SUFFIX_LIST);
#endif /* defined(DEFAULT_INCLUDE_FILE_SUFFIX_LIST) */
#if defined(DEFAULT_INCOGNITO)
  define_numeric_valued_macro(DEFAULT_INCOGNITO);
#else /* !defined(DEFAULT_INCOGNITO) */
  comment_undefined_macro_name(DEFAULT_INCOGNITO);
#endif /* defined(DEFAULT_INCOGNITO) */
#if defined(DEFAULT_INJECTION_ENABLED)
  define_numeric_valued_macro(DEFAULT_INJECTION_ENABLED);
#else /* !defined(DEFAULT_INJECTION_ENABLED) */
  comment_undefined_macro_name(DEFAULT_INJECTION_ENABLED);
#endif /* defined(DEFAULT_INJECTION_ENABLED) */
#if defined(DEFAULT_INLINE_STATEMENT_LIMIT)
  define_numeric_valued_macro(DEFAULT_INLINE_STATEMENT_LIMIT);
#else /* !defined(DEFAULT_INLINE_STATEMENT_LIMIT) */
  comment_undefined_macro_name(DEFAULT_INLINE_STATEMENT_LIMIT);
#endif /* defined(DEFAULT_INLINE_STATEMENT_LIMIT) */
#if defined(DEFAULT_INSTANTIATIONS_PERMITTED_IN_CLASS_SRC_SEQ_LIST)
  define_numeric_valued_macro(
                       DEFAULT_INSTANTIATIONS_PERMITTED_IN_CLASS_SRC_SEQ_LIST);
#else /* !defined(DEFAULT_INSTANTIATIONS_PERMITTED_IN_CLASS_SRC_SEQ_LIST) */
  comment_undefined_macro_name(
                       DEFAULT_INSTANTIATIONS_PERMITTED_IN_CLASS_SRC_SEQ_LIST);
#endif /* defined(DEFAULT_INSTANTIATIONS_PERMITTED_IN_CLASS_SRC_SEQ_LIST) */
#if defined(DEFAULT_INSTANTIATION_FILE_SUFFIX_LIST)
  define_string_valued_macro(DEFAULT_INSTANTIATION_FILE_SUFFIX_LIST);
#else /* !defined(DEFAULT_INSTANTIATION_FILE_SUFFIX_LIST) */
  comment_undefined_macro_name(DEFAULT_INSTANTIATION_FILE_SUFFIX_LIST);
#endif /* defined(DEFAULT_INSTANTIATION_FILE_SUFFIX_LIST) */
#if defined(DEFAULT_INSTANTIATION_MODE)
  define_string_valued_macro(DEFAULT_INSTANTIATION_MODE);
#else /* !defined(DEFAULT_INSTANTIATION_MODE) */
  comment_undefined_macro_name(DEFAULT_INSTANTIATION_MODE);
#endif /* defined(DEFAULT_INSTANTIATION_MODE) */
#if defined(DEFAULT_LAMBDAS_ENABLED)
  define_numeric_valued_macro(DEFAULT_LAMBDAS_ENABLED);
#else /* !defined(DEFAULT_LAMBDAS_ENABLED) */
  comment_undefined_macro_name(DEFAULT_LAMBDAS_ENABLED);
#endif /* defined(DEFAULT_LAMBDAS_ENABLED) */
#if defined(DEFAULT_LONG_PRESERVING_RULES)
  define_numeric_valued_macro(DEFAULT_LONG_PRESERVING_RULES);
#else /* !defined(DEFAULT_LONG_PRESERVING_RULES) */
  comment_undefined_macro_name(DEFAULT_LONG_PRESERVING_RULES);
#endif /* defined(DEFAULT_LONG_PRESERVING_RULES) */
#if defined(DEFAULT_MACRO_POSITIONS_IN_DIAGNOSTICS)
  define_numeric_valued_macro(DEFAULT_MACRO_POSITIONS_IN_DIAGNOSTICS);
#else /* !defined(DEFAULT_MACRO_POSITIONS_IN_DIAGNOSTICS) */
  comment_undefined_macro_name(DEFAULT_MACRO_POSITIONS_IN_DIAGNOSTICS);
#endif /* defined(DEFAULT_MACRO_POSITIONS_IN_DIAGNOSTICS) */
#if defined(DEFAULT_MAX_COST_CONSTEXPR_CALL)
  define_numeric_valued_macro(DEFAULT_MAX_COST_CONSTEXPR_CALL);
#else /* !defined(DEFAULT_MAX_COST_CONSTEXPR_CALL) */
  comment_undefined_macro_name(DEFAULT_MAX_COST_CONSTEXPR_CALL);
#endif /* defined(DEFAULT_MAX_COST_CONSTEXPR_CALL) */
#if defined(DEFAULT_MAX_DEPTH_CONSTEXPR_CALL)
  define_numeric_valued_macro(DEFAULT_MAX_DEPTH_CONSTEXPR_CALL);
#else /* !defined(DEFAULT_MAX_DEPTH_CONSTEXPR_CALL) */
  comment_undefined_macro_name(DEFAULT_MAX_DEPTH_CONSTEXPR_CALL);
#endif /* defined(DEFAULT_MAX_DEPTH_CONSTEXPR_CALL) */
#if defined(DEFAULT_MAX_MANGLED_NAME_LENGTH)
  define_numeric_valued_macro(DEFAULT_MAX_MANGLED_NAME_LENGTH);
#else /* !defined(DEFAULT_MAX_MANGLED_NAME_LENGTH) */
  comment_undefined_macro_name(DEFAULT_MAX_MANGLED_NAME_LENGTH);
#endif /* defined(DEFAULT_MAX_MANGLED_NAME_LENGTH) */
#if defined(DEFAULT_MAX_PENDING_INSTANTIATIONS)
  define_numeric_valued_macro(DEFAULT_MAX_PENDING_INSTANTIATIONS);
#else /* !defined(DEFAULT_MAX_PENDING_INSTANTIATIONS) */
  comment_undefined_macro_name(DEFAULT_MAX_PENDING_INSTANTIATIONS);
#endif /* defined(DEFAULT_MAX_PENDING_INSTANTIATIONS) */
#if defined(DEFAULT_MICROSOFT_64BIT_POINTER_EXTENSIONS_ENABLED)
  define_numeric_valued_macro(
                           DEFAULT_MICROSOFT_64BIT_POINTER_EXTENSIONS_ENABLED);
#else /* !defined(DEFAULT_MICROSOFT_64BIT_POINTER_EXTENSIONS_ENABLED) */
  comment_undefined_macro_name(
                           DEFAULT_MICROSOFT_64BIT_POINTER_EXTENSIONS_ENABLED);
#endif /* defined(DEFAULT_MICROSOFT_64BIT_POINTER_EXTENSIONS_ENABLED) */
#if defined(DEFAULT_MICROSOFT_BUGS)
  define_numeric_valued_macro(DEFAULT_MICROSOFT_BUGS);
#else /* !defined(DEFAULT_MICROSOFT_BUGS) */
  comment_undefined_macro_name(DEFAULT_MICROSOFT_BUGS);
#endif /* defined(DEFAULT_MICROSOFT_BUGS) */
#if defined(DEFAULT_MICROSOFT_COMPATIBILITY)
  define_numeric_valued_macro(DEFAULT_MICROSOFT_COMPATIBILITY);
#else /* !defined(DEFAULT_MICROSOFT_COMPATIBILITY) */
  comment_undefined_macro_name(DEFAULT_MICROSOFT_COMPATIBILITY);
#endif /* defined(DEFAULT_MICROSOFT_COMPATIBILITY) */
#if defined(DEFAULT_MICROSOFT_EXTENSIONS)
  define_numeric_valued_macro(DEFAULT_MICROSOFT_EXTENSIONS);
#else /* !defined(DEFAULT_MICROSOFT_EXTENSIONS) */
  comment_undefined_macro_name(DEFAULT_MICROSOFT_EXTENSIONS);
#endif /* defined(DEFAULT_MICROSOFT_EXTENSIONS) */
#if defined(DEFAULT_MICROSOFT_MODE)
  define_numeric_valued_macro(DEFAULT_MICROSOFT_MODE);
#else /* !defined(DEFAULT_MICROSOFT_MODE) */
  comment_undefined_macro_name(DEFAULT_MICROSOFT_MODE);
#endif /* defined(DEFAULT_MICROSOFT_MODE) */
#if defined(DEFAULT_MICROSOFT_VERSION)
  define_numeric_valued_macro(DEFAULT_MICROSOFT_VERSION);
#else /* !defined(DEFAULT_MICROSOFT_VERSION) */
  comment_undefined_macro_name(DEFAULT_MICROSOFT_VERSION);
#endif /* defined(DEFAULT_MICROSOFT_VERSION) */
#if defined(DEFAULT_MODULES_ENABLED)
  define_numeric_valued_macro(DEFAULT_MODULES_ENABLED);
#else /* !defined(DEFAULT_MODULES_ENABLED) */
  comment_undefined_macro_name(DEFAULT_MODULES_ENABLED);
#endif /* defined(DEFAULT_MODULES_ENABLED) */
#if defined(DEFAULT_MODULE_IMPORT_DIAG_ENABLED)
  define_numeric_valued_macro(DEFAULT_MODULE_IMPORT_DIAG_ENABLED);
#else /* !defined(DEFAULT_MODULE_IMPORT_DIAG_ENABLED) */
  comment_undefined_macro_name(DEFAULT_MODULE_IMPORT_DIAG_ENABLED);
#endif /* defined(DEFAULT_MODULE_IMPORT_DIAG_ENABLED) */
#if defined(DEFAULT_MSVC_EXECUTION_CHARACTER_SET)
  define_string_valued_macro(DEFAULT_MSVC_EXECUTION_CHARACTER_SET);
#else /* !defined(DEFAULT_MSVC_EXECUTION_CHARACTER_SET) */
  comment_undefined_macro_name(DEFAULT_MSVC_EXECUTION_CHARACTER_SET);
#endif /* defined(DEFAULT_MSVC_EXECUTION_CHARACTER_SET) */
#if defined(DEFAULT_MS_PERMISSIVE)
  define_numeric_valued_macro(DEFAULT_MS_PERMISSIVE);
#else /* !defined(DEFAULT_MS_PERMISSIVE) */
  comment_undefined_macro_name(DEFAULT_MS_PERMISSIVE);
#endif /* defined(DEFAULT_MS_PERMISSIVE) */
#if defined(DEFAULT_MULTIBYTE_CHARS_IN_SOURCE_ENABLED)
  define_numeric_valued_macro(DEFAULT_MULTIBYTE_CHARS_IN_SOURCE_ENABLED);
#else /* !defined(DEFAULT_MULTIBYTE_CHARS_IN_SOURCE_ENABLED) */
  comment_undefined_macro_name(DEFAULT_MULTIBYTE_CHARS_IN_SOURCE_ENABLED);
#endif /* defined(DEFAULT_MULTIBYTE_CHARS_IN_SOURCE_ENABLED) */
#if defined(DEFAULT_NAMED_ADDRESS_SPACES_ENABLED)
  define_numeric_valued_macro(DEFAULT_NAMED_ADDRESS_SPACES_ENABLED);
#else /* !defined(DEFAULT_NAMED_ADDRESS_SPACES_ENABLED) */
  comment_undefined_macro_name(DEFAULT_NAMED_ADDRESS_SPACES_ENABLED);
#endif /* defined(DEFAULT_NAMED_ADDRESS_SPACES_ENABLED) */
#if defined(DEFAULT_NAMED_REGISTERS_ENABLED)
  define_numeric_valued_macro(DEFAULT_NAMED_REGISTERS_ENABLED);
#else /* !defined(DEFAULT_NAMED_REGISTERS_ENABLED) */
  comment_undefined_macro_name(DEFAULT_NAMED_REGISTERS_ENABLED);
#endif /* defined(DEFAULT_NAMED_REGISTERS_ENABLED) */
#if defined(DEFAULT_NAMESPACES_ENABLED)
  define_numeric_valued_macro(DEFAULT_NAMESPACES_ENABLED);
#else /* !defined(DEFAULT_NAMESPACES_ENABLED) */
  comment_undefined_macro_name(DEFAULT_NAMESPACES_ENABLED);
#endif /* defined(DEFAULT_NAMESPACES_ENABLED) */
#if defined(DEFAULT_NEAR_AND_FAR_ENABLED)
  define_numeric_valued_macro(DEFAULT_NEAR_AND_FAR_ENABLED);
#else /* !defined(DEFAULT_NEAR_AND_FAR_ENABLED) */
  comment_undefined_macro_name(DEFAULT_NEAR_AND_FAR_ENABLED);
#endif /* defined(DEFAULT_NEAR_AND_FAR_ENABLED) */
#if defined(DEFAULT_NONSTANDARD_DEFAULT_ARG_DEDUCTION)
  define_numeric_valued_macro(DEFAULT_NONSTANDARD_DEFAULT_ARG_DEDUCTION);
#else /* !defined(DEFAULT_NONSTANDARD_DEFAULT_ARG_DEDUCTION) */
  comment_undefined_macro_name(DEFAULT_NONSTANDARD_DEFAULT_ARG_DEDUCTION);
#endif /* defined(DEFAULT_NONSTANDARD_DEFAULT_ARG_DEDUCTION) */
#if defined(DEFAULT_NONSTANDARD_INSTANTIATION_LOOKUP)
  define_numeric_valued_macro(DEFAULT_NONSTANDARD_INSTANTIATION_LOOKUP);
#else /* !defined(DEFAULT_NONSTANDARD_INSTANTIATION_LOOKUP) */
  comment_undefined_macro_name(DEFAULT_NONSTANDARD_INSTANTIATION_LOOKUP);
#endif /* defined(DEFAULT_NONSTANDARD_INSTANTIATION_LOOKUP) */
#if defined(DEFAULT_NONSTANDARD_QUALIFIER_DEDUCTION)
  define_numeric_valued_macro(DEFAULT_NONSTANDARD_QUALIFIER_DEDUCTION);
#else /* !defined(DEFAULT_NONSTANDARD_QUALIFIER_DEDUCTION) */
  comment_undefined_macro_name(DEFAULT_NONSTANDARD_QUALIFIER_DEDUCTION);
#endif /* defined(DEFAULT_NONSTANDARD_QUALIFIER_DEDUCTION) */
#if defined(DEFAULT_NONSTANDARD_USING_DECL_ALLOWED)
  define_numeric_valued_macro(DEFAULT_NONSTANDARD_USING_DECL_ALLOWED);
#else /* !defined(DEFAULT_NONSTANDARD_USING_DECL_ALLOWED) */
  comment_undefined_macro_name(DEFAULT_NONSTANDARD_USING_DECL_ALLOWED);
#endif /* defined(DEFAULT_NONSTANDARD_USING_DECL_ALLOWED) */
#if defined(DEFAULT_NO_ACCESS_CHECK_ON_FRIEND_DECLARATOR_IDS)
  define_numeric_valued_macro(
                             DEFAULT_NO_ACCESS_CHECK_ON_FRIEND_DECLARATOR_IDS);
#else /* !defined(DEFAULT_NO_ACCESS_CHECK_ON_FRIEND_DECLARATOR_IDS) */
  comment_undefined_macro_name(
                             DEFAULT_NO_ACCESS_CHECK_ON_FRIEND_DECLARATOR_IDS);
#endif /* defined(DEFAULT_NO_ACCESS_CHECK_ON_FRIEND_DECLARATOR_IDS) */
#if defined(DEFAULT_NULLPTR_ENABLED)
  define_numeric_valued_macro(DEFAULT_NULLPTR_ENABLED);
#else /* !defined(DEFAULT_NULLPTR_ENABLED) */
  comment_undefined_macro_name(DEFAULT_NULLPTR_ENABLED);
#endif /* defined(DEFAULT_NULLPTR_ENABLED) */
#if defined(DEFAULT_NULL_CHARS_ALLOWED_IN_SOURCE)
  define_numeric_valued_macro(DEFAULT_NULL_CHARS_ALLOWED_IN_SOURCE);
#else /* !defined(DEFAULT_NULL_CHARS_ALLOWED_IN_SOURCE) */
  comment_undefined_macro_name(DEFAULT_NULL_CHARS_ALLOWED_IN_SOURCE);
#endif /* defined(DEFAULT_NULL_CHARS_ALLOWED_IN_SOURCE) */
#if defined(DEFAULT_OLD_SPECIALIZATIONS_ALLOWED)
  define_numeric_valued_macro(DEFAULT_OLD_SPECIALIZATIONS_ALLOWED);
#else /* !defined(DEFAULT_OLD_SPECIALIZATIONS_ALLOWED) */
  comment_undefined_macro_name(DEFAULT_OLD_SPECIALIZATIONS_ALLOWED);
#endif /* defined(DEFAULT_OLD_SPECIALIZATIONS_ALLOWED) */
#if defined(DEFAULT_OLD_SPECIALIZATIONS_FOR_GENERATED_INSTANCES)
  define_numeric_valued_macro(
                          DEFAULT_OLD_SPECIALIZATIONS_FOR_GENERATED_INSTANCES);
#else /* !defined(DEFAULT_OLD_SPECIALIZATIONS_FOR_GENERATED_INSTANCES) */
  comment_undefined_macro_name(
                          DEFAULT_OLD_SPECIALIZATIONS_FOR_GENERATED_INSTANCES);
#endif /* defined(DEFAULT_OLD_SPECIALIZATIONS_FOR_GENERATED_INSTANCES) */
#if defined(DEFAULT_OPERATOR_OVERLOADING_ON_ENUMS)
  define_numeric_valued_macro(DEFAULT_OPERATOR_OVERLOADING_ON_ENUMS);
#else /* !defined(DEFAULT_OPERATOR_OVERLOADING_ON_ENUMS) */
  comment_undefined_macro_name(DEFAULT_OPERATOR_OVERLOADING_ON_ENUMS);
#endif /* defined(DEFAULT_OPERATOR_OVERLOADING_ON_ENUMS) */
#if defined(DEFAULT_OUTPUT_MODE)
  define_string_valued_macro(DEFAULT_OUTPUT_MODE);
#else /* !defined(DEFAULT_OUTPUT_MODE) */
  comment_undefined_macro_name(DEFAULT_OUTPUT_MODE);
#endif /* defined(DEFAULT_OUTPUT_MODE) */
#if defined(DEFAULT_PASS_STDARG_REFERENCES_TO_GENERATED_CODE)
  define_numeric_valued_macro(
                             DEFAULT_PASS_STDARG_REFERENCES_TO_GENERATED_CODE);
#else /* !defined(DEFAULT_PASS_STDARG_REFERENCES_TO_GENERATED_CODE) */
  comment_undefined_macro_name(
                             DEFAULT_PASS_STDARG_REFERENCES_TO_GENERATED_CODE);
#endif /* defined(DEFAULT_PASS_STDARG_REFERENCES_TO_GENERATED_CODE) */
#if defined(DEFAULT_POINTER_TO_MEMBER_CALL_OPTIMIZATION_ALLOWED)
  define_numeric_valued_macro(
                          DEFAULT_POINTER_TO_MEMBER_CALL_OPTIMIZATION_ALLOWED);
#else /* !defined(DEFAULT_POINTER_TO_MEMBER_CALL_OPTIMIZATION_ALLOWED) */
  comment_undefined_macro_name(
                          DEFAULT_POINTER_TO_MEMBER_CALL_OPTIMIZATION_ALLOWED);
#endif /* defined(DEFAULT_POINTER_TO_MEMBER_CALL_OPTIMIZATION_ALLOWED) */
#if defined(DEFAULT_PREALLOCATED_PCH_MEM_SIZE)
  define_numeric_valued_macro(DEFAULT_PREALLOCATED_PCH_MEM_SIZE);
#else /* !defined(DEFAULT_PREALLOCATED_PCH_MEM_SIZE) */
  comment_undefined_macro_name(DEFAULT_PREALLOCATED_PCH_MEM_SIZE);
#endif /* defined(DEFAULT_PREALLOCATED_PCH_MEM_SIZE) */
#if defined(DEFAULT_RANGE_BASED_FOR_ENABLED)
  define_numeric_valued_macro(DEFAULT_RANGE_BASED_FOR_ENABLED);
#else /* !defined(DEFAULT_RANGE_BASED_FOR_ENABLED) */
  comment_undefined_macro_name(DEFAULT_RANGE_BASED_FOR_ENABLED);
#endif /* defined(DEFAULT_RANGE_BASED_FOR_ENABLED) */
#if defined(DEFAULT_RECORD_FORM_OF_NAME_REFERENCE)
  define_numeric_valued_macro(DEFAULT_RECORD_FORM_OF_NAME_REFERENCE);
#else /* !defined(DEFAULT_RECORD_FORM_OF_NAME_REFERENCE) */
  comment_undefined_macro_name(DEFAULT_RECORD_FORM_OF_NAME_REFERENCE);
#endif /* defined(DEFAULT_RECORD_FORM_OF_NAME_REFERENCE) */
#if defined(DEFAULT_REFLECTION_ENABLED)
  define_numeric_valued_macro(DEFAULT_REFLECTION_ENABLED);
#else /* !defined(DEFAULT_REFLECTION_ENABLED) */
  comment_undefined_macro_name(DEFAULT_REFLECTION_ENABLED);
#endif /* defined(DEFAULT_REFLECTION_ENABLED) */
#if defined(DEFAULT_REMOVE_QUALIFIERS_FROM_PARAM_TYPES)
  define_numeric_valued_macro(DEFAULT_REMOVE_QUALIFIERS_FROM_PARAM_TYPES);
#else /* !defined(DEFAULT_REMOVE_QUALIFIERS_FROM_PARAM_TYPES) */
  comment_undefined_macro_name(DEFAULT_REMOVE_QUALIFIERS_FROM_PARAM_TYPES);
#endif /* defined(DEFAULT_REMOVE_QUALIFIERS_FROM_PARAM_TYPES) */
#if defined(DEFAULT_REMOVE_UNNEEDED_ENTITIES)
  define_numeric_valued_macro(DEFAULT_REMOVE_UNNEEDED_ENTITIES);
#else /* !defined(DEFAULT_REMOVE_UNNEEDED_ENTITIES) */
  comment_undefined_macro_name(DEFAULT_REMOVE_UNNEEDED_ENTITIES);
#endif /* defined(DEFAULT_REMOVE_UNNEEDED_ENTITIES) */
#if defined(DEFAULT_RESTRICT_ENABLED)
  define_numeric_valued_macro(DEFAULT_RESTRICT_ENABLED);
#else /* !defined(DEFAULT_RESTRICT_ENABLED) */
  comment_undefined_macro_name(DEFAULT_RESTRICT_ENABLED);
#endif /* defined(DEFAULT_RESTRICT_ENABLED) */
#if defined(DEFAULT_RIGHT_SHIFT_CAN_BE_ANGLE_BRACKETS)
  define_numeric_valued_macro(DEFAULT_RIGHT_SHIFT_CAN_BE_ANGLE_BRACKETS);
#else /* !defined(DEFAULT_RIGHT_SHIFT_CAN_BE_ANGLE_BRACKETS) */
  comment_undefined_macro_name(DEFAULT_RIGHT_SHIFT_CAN_BE_ANGLE_BRACKETS);
#endif /* defined(DEFAULT_RIGHT_SHIFT_CAN_BE_ANGLE_BRACKETS) */
#if defined(DEFAULT_RTTI_ENABLED)
  define_numeric_valued_macro(DEFAULT_RTTI_ENABLED);
#else /* !defined(DEFAULT_RTTI_ENABLED) */
  comment_undefined_macro_name(DEFAULT_RTTI_ENABLED);
#endif /* defined(DEFAULT_RTTI_ENABLED) */
#if defined(DEFAULT_RVALUE_REFERENCES_ENABLED)
  define_numeric_valued_macro(DEFAULT_RVALUE_REFERENCES_ENABLED);
#else /* !defined(DEFAULT_RVALUE_REFERENCES_ENABLED) */
  comment_undefined_macro_name(DEFAULT_RVALUE_REFERENCES_ENABLED);
#endif /* defined(DEFAULT_RVALUE_REFERENCES_ENABLED) */
#if defined(DEFAULT_SINGLE_REF_QUAL_OVL_RES_TIEBREAKER)
  define_numeric_valued_macro(DEFAULT_SINGLE_REF_QUAL_OVL_RES_TIEBREAKER);
#else /* !defined(DEFAULT_SINGLE_REF_QUAL_OVL_RES_TIEBREAKER) */
  comment_undefined_macro_name(DEFAULT_SINGLE_REF_QUAL_OVL_RES_TIEBREAKER);
#endif /* defined(DEFAULT_SINGLE_REF_QUAL_OVL_RES_TIEBREAKER) */
#if defined(DEFAULT_SPECIAL_SUBSCRIPT_COST)
  define_numeric_valued_macro(DEFAULT_SPECIAL_SUBSCRIPT_COST);
#else /* !defined(DEFAULT_SPECIAL_SUBSCRIPT_COST) */
  comment_undefined_macro_name(DEFAULT_SPECIAL_SUBSCRIPT_COST);
#endif /* defined(DEFAULT_SPECIAL_SUBSCRIPT_COST) */
#if defined(DEFAULT_STDC_ZERO_IN_SYSTEM_HEADERS)
  define_numeric_valued_macro(DEFAULT_STDC_ZERO_IN_SYSTEM_HEADERS);
#else /* !defined(DEFAULT_STDC_ZERO_IN_SYSTEM_HEADERS) */
  comment_undefined_macro_name(DEFAULT_STDC_ZERO_IN_SYSTEM_HEADERS);
#endif /* defined(DEFAULT_STDC_ZERO_IN_SYSTEM_HEADERS) */
#if defined(DEFAULT_STRING_LITERALS_ARE_CONST)
  define_numeric_valued_macro(DEFAULT_STRING_LITERALS_ARE_CONST);
#else /* !defined(DEFAULT_STRING_LITERALS_ARE_CONST) */
  comment_undefined_macro_name(DEFAULT_STRING_LITERALS_ARE_CONST);
#endif /* defined(DEFAULT_STRING_LITERALS_ARE_CONST) */
#if defined(DEFAULT_SUN_COMPATIBILITY)
  define_numeric_valued_macro(DEFAULT_SUN_COMPATIBILITY);
#else /* !defined(DEFAULT_SUN_COMPATIBILITY) */
  comment_undefined_macro_name(DEFAULT_SUN_COMPATIBILITY);
#endif /* defined(DEFAULT_SUN_COMPATIBILITY) */
#if defined(DEFAULT_SUN_LINKER_SCOPE_ALLOWED)
  define_numeric_valued_macro(DEFAULT_SUN_LINKER_SCOPE_ALLOWED);
#else /* !defined(DEFAULT_SUN_LINKER_SCOPE_ALLOWED) */
  comment_undefined_macro_name(DEFAULT_SUN_LINKER_SCOPE_ALLOWED);
#endif /* defined(DEFAULT_SUN_LINKER_SCOPE_ALLOWED) */
#if defined(DEFAULT_SUPPRESS_DEFERRAL_ON_PARTIAL_SPEC_MEMBERS)
  define_numeric_valued_macro(
                            DEFAULT_SUPPRESS_DEFERRAL_ON_PARTIAL_SPEC_MEMBERS);
#else /* !defined(DEFAULT_SUPPRESS_DEFERRAL_ON_PARTIAL_SPEC_MEMBERS) */
  comment_undefined_macro_name(
                            DEFAULT_SUPPRESS_DEFERRAL_ON_PARTIAL_SPEC_MEMBERS);
#endif /* defined(DEFAULT_SUPPRESS_DEFERRAL_ON_PARTIAL_SPEC_MEMBERS) */
#if defined(DEFAULT_SVR4_C_MODE)
  define_numeric_valued_macro(DEFAULT_SVR4_C_MODE);
#else /* !defined(DEFAULT_SVR4_C_MODE) */
  comment_undefined_macro_name(DEFAULT_SVR4_C_MODE);
#endif /* defined(DEFAULT_SVR4_C_MODE) */
#if defined(DEFAULT_TARGET_CONFIGURATION_NAME)
  define_string_valued_macro(DEFAULT_TARGET_CONFIGURATION_NAME);
#else /* !defined(DEFAULT_TARGET_CONFIGURATION_NAME) */
  comment_undefined_macro_name(DEFAULT_TARGET_CONFIGURATION_NAME);
#endif /* defined(DEFAULT_TARGET_CONFIGURATION_NAME) */
#if defined(DEFAULT_THREAD_LOCAL_STORAGE_SPECIFIER_ENABLED)
  define_numeric_valued_macro(DEFAULT_THREAD_LOCAL_STORAGE_SPECIFIER_ENABLED);
#else /* !defined(DEFAULT_THREAD_LOCAL_STORAGE_SPECIFIER_ENABLED) */
  comment_undefined_macro_name(DEFAULT_THREAD_LOCAL_STORAGE_SPECIFIER_ENABLED);
#endif /* defined(DEFAULT_THREAD_LOCAL_STORAGE_SPECIFIER_ENABLED) */
#if defined(DEFAULT_TMPDIR)
  define_string_valued_macro(DEFAULT_TMPDIR);
#else /* !defined(DEFAULT_TMPDIR) */
  comment_undefined_macro_name(DEFAULT_TMPDIR);
#endif /* defined(DEFAULT_TMPDIR) */
#if defined(DEFAULT_TRIGRAPHS_ALLOWED)
  define_numeric_valued_macro(DEFAULT_TRIGRAPHS_ALLOWED);
#else /* !defined(DEFAULT_TRIGRAPHS_ALLOWED) */
  comment_undefined_macro_name(DEFAULT_TRIGRAPHS_ALLOWED);
#endif /* defined(DEFAULT_TRIGRAPHS_ALLOWED) */
#if defined(DEFAULT_TYPENAME_ENABLED)
  define_numeric_valued_macro(DEFAULT_TYPENAME_ENABLED);
#else /* !defined(DEFAULT_TYPENAME_ENABLED) */
  comment_undefined_macro_name(DEFAULT_TYPENAME_ENABLED);
#endif /* defined(DEFAULT_TYPENAME_ENABLED) */
#if defined(DEFAULT_TYPE_INFO_IN_NAMESPACE_STD)
  define_numeric_valued_macro(DEFAULT_TYPE_INFO_IN_NAMESPACE_STD);
#else /* !defined(DEFAULT_TYPE_INFO_IN_NAMESPACE_STD) */
  comment_undefined_macro_name(DEFAULT_TYPE_INFO_IN_NAMESPACE_STD);
#endif /* defined(DEFAULT_TYPE_INFO_IN_NAMESPACE_STD) */
#if defined(DEFAULT_TYPE_TRAITS_HELPERS_ENABLED)
  define_numeric_valued_macro(DEFAULT_TYPE_TRAITS_HELPERS_ENABLED);
#else /* !defined(DEFAULT_TYPE_TRAITS_HELPERS_ENABLED) */
  comment_undefined_macro_name(DEFAULT_TYPE_TRAITS_HELPERS_ENABLED);
#endif /* defined(DEFAULT_TYPE_TRAITS_HELPERS_ENABLED) */
#if defined(DEFAULT_ULITERALS_ENABLED)
  define_numeric_valued_macro(DEFAULT_ULITERALS_ENABLED);
#else /* !defined(DEFAULT_ULITERALS_ENABLED) */
  comment_undefined_macro_name(DEFAULT_ULITERALS_ENABLED);
#endif /* defined(DEFAULT_ULITERALS_ENABLED) */
#if defined(DEFAULT_UNICODE_SOURCE_KIND)
  define_string_valued_macro(DEFAULT_UNICODE_SOURCE_KIND);
#else /* !defined(DEFAULT_UNICODE_SOURCE_KIND) */
  comment_undefined_macro_name(DEFAULT_UNICODE_SOURCE_KIND);
#endif /* defined(DEFAULT_UNICODE_SOURCE_KIND) */
#if defined(DEFAULT_UPC_MODE)
  define_numeric_valued_macro(DEFAULT_UPC_MODE);
#else /* !defined(DEFAULT_UPC_MODE) */
  comment_undefined_macro_name(DEFAULT_UPC_MODE);
#endif /* defined(DEFAULT_UPC_MODE) */
#if defined(DEFAULT_USE_NONSTANDARD_FOR_INIT_SCOPE)
  define_numeric_valued_macro(DEFAULT_USE_NONSTANDARD_FOR_INIT_SCOPE);
#else /* !defined(DEFAULT_USE_NONSTANDARD_FOR_INIT_SCOPE) */
  comment_undefined_macro_name(DEFAULT_USE_NONSTANDARD_FOR_INIT_SCOPE);
#endif /* defined(DEFAULT_USE_NONSTANDARD_FOR_INIT_SCOPE) */
#if defined(DEFAULT_USE_PREDEFINED_MACRO_FILE)
  define_numeric_valued_macro(DEFAULT_USE_PREDEFINED_MACRO_FILE);
#else /* !defined(DEFAULT_USE_PREDEFINED_MACRO_FILE) */
  comment_undefined_macro_name(DEFAULT_USE_PREDEFINED_MACRO_FILE);
#endif /* defined(DEFAULT_USE_PREDEFINED_MACRO_FILE) */
#if defined(DEFAULT_USR_INCLUDE)
  define_string_valued_macro(DEFAULT_USR_INCLUDE);
#else /* !defined(DEFAULT_USR_INCLUDE) */
  comment_undefined_macro_name(DEFAULT_USR_INCLUDE);
#endif /* defined(DEFAULT_USR_INCLUDE) */
#if defined(DEFAULT_VAR_TEMPL_INTRINSICS_ENABLED)
  define_numeric_valued_macro(DEFAULT_VAR_TEMPL_INTRINSICS_ENABLED);
#else /* !defined(DEFAULT_VAR_TEMPL_INTRINSICS_ENABLED) */
  comment_undefined_macro_name(DEFAULT_VAR_TEMPL_INTRINSICS_ENABLED);
#endif /* defined(DEFAULT_VAR_TEMPL_INTRINSICS_ENABLED) */
#if defined(DEFAULT_VARIADIC_MACROS_ALLOWED)
  define_numeric_valued_macro(DEFAULT_VARIADIC_MACROS_ALLOWED);
#else /* !defined(DEFAULT_VARIADIC_MACROS_ALLOWED) */
  comment_undefined_macro_name(DEFAULT_VARIADIC_MACROS_ALLOWED);
#endif /* defined(DEFAULT_VARIADIC_MACROS_ALLOWED) */
#if defined(DEFAULT_VARIADIC_TEMPLATES_ENABLED)
  define_numeric_valued_macro(DEFAULT_VARIADIC_TEMPLATES_ENABLED);
#else /* !defined(DEFAULT_VARIADIC_TEMPLATES_ALLOWED) */
  comment_undefined_macro_name(DEFAULT_VARIADIC_TEMPLATES_ENABLED);
#endif /* defined(DEFAULT_VARIADIC_TEMPLATES_ENABLED) */
#if defined(DEFAULT_VA_LIST_IN_STD_NAMESPACE)
  define_numeric_valued_macro(DEFAULT_VA_LIST_IN_STD_NAMESPACE);
#else /* !defined(DEFAULT_VA_LIST_IN_STD_NAMESPACE) */
  comment_undefined_macro_name(DEFAULT_VA_LIST_IN_STD_NAMESPACE);
#endif /* defined(DEFAULT_VA_LIST_IN_STD_NAMESPACE) */
#if defined(DEFAULT_VLA_ENABLED)
  define_numeric_valued_macro(DEFAULT_VLA_ENABLED);
#else /* !defined(DEFAULT_VLA_ENABLED) */
  comment_undefined_macro_name(DEFAULT_VLA_ENABLED);
#endif /* defined(DEFAULT_VLA_ENABLED) */
#if defined(DEFAULT_WARNING_ON_FOR_INIT_DIFFERENCE)
  define_numeric_valued_macro(DEFAULT_WARNING_ON_FOR_INIT_DIFFERENCE);
#else /* !defined(DEFAULT_WARNING_ON_FOR_INIT_DIFFERENCE) */
  comment_undefined_macro_name(DEFAULT_WARNING_ON_FOR_INIT_DIFFERENCE);
#endif /* defined(DEFAULT_WARNING_ON_FOR_INIT_DIFFERENCE) */
#if defined(DEFAULT_WARNING_ON_LOSSY_CONVERSION)
  define_numeric_valued_macro(DEFAULT_WARNING_ON_LOSSY_CONVERSION);
#else /* !defined(DEFAULT_WARNING_ON_LOSSY_CONVERSION) */
  comment_undefined_macro_name(DEFAULT_WARNING_ON_LOSSY_CONVERSION);
#endif /* defined(DEFAULT_WARNING_ON_LOSSY_CONVERSION) */
#if defined(DEFAULT_WARNING_ON_NON_TEMPLATE_FRIEND)
  define_numeric_valued_macro(DEFAULT_WARNING_ON_NON_TEMPLATE_FRIEND);
#else /* !defined(DEFAULT_WARNING_ON_NON_TEMPLATE_FRIEND) */
  comment_undefined_macro_name(DEFAULT_WARNING_ON_NON_TEMPLATE_FRIEND);
#endif /* defined(DEFAULT_WARNING_ON_NON_TEMPLATE_FRIEND) */
#if defined(DEFAULT_WCHAR_T_IS_KEYWORD)
  define_numeric_valued_macro(DEFAULT_WCHAR_T_IS_KEYWORD);
#else /* !defined(DEFAULT_WCHAR_T_IS_KEYWORD) */
  comment_undefined_macro_name(DEFAULT_WCHAR_T_IS_KEYWORD);
#endif /* defined(DEFAULT_WCHAR_T_IS_KEYWORD) */
#if defined(DEFINE_MACRO_WHEN_ARRAY_NEW_AND_DELETE_ENABLED)
  define_numeric_valued_macro(DEFINE_MACRO_WHEN_ARRAY_NEW_AND_DELETE_ENABLED);
#else /* !defined(DEFINE_MACRO_WHEN_ARRAY_NEW_AND_DELETE_ENABLED) */
  comment_undefined_macro_name(DEFINE_MACRO_WHEN_ARRAY_NEW_AND_DELETE_ENABLED);
#endif /* defined(DEFINE_MACRO_WHEN_ARRAY_NEW_AND_DELETE_ENABLED) */
#if defined(DEFINE_MACRO_WHEN_BOOL_IS_KEYWORD)
  define_numeric_valued_macro(DEFINE_MACRO_WHEN_BOOL_IS_KEYWORD);
#else /* !defined(DEFINE_MACRO_WHEN_BOOL_IS_KEYWORD) */
  comment_undefined_macro_name(DEFINE_MACRO_WHEN_BOOL_IS_KEYWORD);
#endif /* defined(DEFINE_MACRO_WHEN_BOOL_IS_KEYWORD) */
#if defined(DEFINE_MACRO_WHEN_CHAR16_T_AND_CHAR32_T_ARE_KEYWORDS)
  define_numeric_valued_macro(
                         DEFINE_MACRO_WHEN_CHAR16_T_AND_CHAR32_T_ARE_KEYWORDS);
#else /* !defined(DEFINE_MACRO_WHEN_CHAR16_T_AND_CHAR32_T_ARE_KEYWORDS) */
  comment_undefined_macro_name(
                         DEFINE_MACRO_WHEN_CHAR16_T_AND_CHAR32_T_ARE_KEYWORDS);
#endif /* defined(DEFINE_MACRO_WHEN_CHAR16_T_AND_CHAR32_T_ARE_KEYWORDS) */
#if defined(DEFINE_MACRO_WHEN_EXCEPTIONS_ENABLED)
  define_numeric_valued_macro(DEFINE_MACRO_WHEN_EXCEPTIONS_ENABLED);
#else /* !defined(DEFINE_MACRO_WHEN_EXCEPTIONS_ENABLED) */
  comment_undefined_macro_name(DEFINE_MACRO_WHEN_EXCEPTIONS_ENABLED);
#endif /* defined(DEFINE_MACRO_WHEN_EXCEPTIONS_ENABLED) */
#if defined(DEFINE_MACRO_WHEN_LONG_LONG_IS_DISABLED)
  define_numeric_valued_macro(DEFINE_MACRO_WHEN_LONG_LONG_IS_DISABLED);
#else /* !defined(DEFINE_MACRO_WHEN_LONG_LONG_IS_DISABLED) */
  comment_undefined_macro_name(DEFINE_MACRO_WHEN_LONG_LONG_IS_DISABLED);
#endif /* defined(DEFINE_MACRO_WHEN_LONG_LONG_IS_DISABLED) */
#if defined(DEFINE_MACRO_WHEN_PLACEMENT_DELETE_ENABLED)
  define_numeric_valued_macro(DEFINE_MACRO_WHEN_PLACEMENT_DELETE_ENABLED);
#else /* !defined(DEFINE_MACRO_WHEN_PLACEMENT_DELETE_ENABLED) */
  comment_undefined_macro_name(DEFINE_MACRO_WHEN_PLACEMENT_DELETE_ENABLED);
#endif /* defined(DEFINE_MACRO_WHEN_PLACEMENT_DELETE_ENABLED) */
#if defined(DEFINE_MACRO_WHEN_RTTI_ENABLED)
  define_numeric_valued_macro(DEFINE_MACRO_WHEN_RTTI_ENABLED);
#else /* !defined(DEFINE_MACRO_WHEN_RTTI_ENABLED) */
  comment_undefined_macro_name(DEFINE_MACRO_WHEN_RTTI_ENABLED);
#endif /* defined(DEFINE_MACRO_WHEN_RTTI_ENABLED) */
#if defined(DEFINE_MACRO_WHEN_VARIADIC_TEMPLATES_ENABLED)
  define_numeric_valued_macro(DEFINE_MACRO_WHEN_VARIADIC_TEMPLATES_ENABLED);
#else /* !defined(DEFINE_MACRO_WHEN_VARIADIC_TEMPLATES_ENABLED) */
  comment_undefined_macro_name(DEFINE_MACRO_WHEN_VARIADIC_TEMPLATES_ENABLED);
#endif /* defined(DEFINE_MACRO_WHEN_VARIADIC_TEMPLATES_ENABLED) */
#if defined(DEFINE_MACRO_WHEN_WCHAR_T_IS_KEYWORD)
  define_numeric_valued_macro(DEFINE_MACRO_WHEN_WCHAR_T_IS_KEYWORD);
#else /* !defined(DEFINE_MACRO_WHEN_WCHAR_T_IS_KEYWORD) */
  comment_undefined_macro_name(DEFINE_MACRO_WHEN_WCHAR_T_IS_KEYWORD);
#endif /* defined(DEFINE_MACRO_WHEN_WCHAR_T_IS_KEYWORD) */
#if defined(DEFINE_STDC_IN_MICROSOFT_MODE)
  define_numeric_valued_macro(DEFINE_STDC_IN_MICROSOFT_MODE);
#else /* !defined(DEFINE_STDC_IN_MICROSOFT_MODE) */
  comment_undefined_macro_name(DEFINE_STDC_IN_MICROSOFT_MODE);
#endif /* defined(DEFINE_STDC_IN_MICROSOFT_MODE) */
#if defined(DELETED_COPY_FUNCTION_CLEARS_BITWISE_COPY_FLAG)
  define_numeric_valued_macro(DELETED_COPY_FUNCTION_CLEARS_BITWISE_COPY_FLAG);
#else /* !defined(DELETED_COPY_FUNCTION_CLEARS_BITWISE_COPY_FLAG) */
  comment_undefined_macro_name(DELETED_COPY_FUNCTION_CLEARS_BITWISE_COPY_FLAG);
#endif /* defined(DELETED_COPY_FUNCTION_CLEARS_BITWISE_COPY_FLAG) */
#if defined(DELETE_CAN_BE_FOLDED_INTO_DTOR)
  define_numeric_valued_macro(DELETE_CAN_BE_FOLDED_INTO_DTOR);
#else /* !defined(DELETE_CAN_BE_FOLDED_INTO_DTOR) */
  comment_undefined_macro_name(DELETE_CAN_BE_FOLDED_INTO_DTOR);
#endif /* defined(DELETE_CAN_BE_FOLDED_INTO_DTOR) */
#if defined(DEMO_VERSION_ID)
  define_string_valued_macro(DEMO_VERSION_ID);
#else /* !defined(DEMO_VERSION_ID) */
  comment_undefined_macro_name(DEMO_VERSION_ID);
#endif /* defined(DEMO_VERSION_ID) */
#if defined(DIRECTORY_SEPARATOR)
  define_string_valued_macro(DIRECTORY_SEPARATOR);
#else /* !defined(DIRECTORY_SEPARATOR) */
  comment_undefined_macro_name(DIRECTORY_SEPARATOR);
#endif /* defined(DIRECTORY_SEPARATOR) */
#if defined(DIRECTORY_SEPARATOR_STRING)
  define_string_valued_macro(DIRECTORY_SEPARATOR_STRING);
#else /* !defined(DIRECTORY_SEPARATOR_STRING) */
  comment_undefined_macro_name(DIRECTORY_SEPARATOR_STRING);
#endif /* defined(DIRECTORY_SEPARATOR_STRING) */
#if defined(DIRECT_ERROR_OUTPUT_TO_STDOUT)
  define_numeric_valued_macro(DIRECT_ERROR_OUTPUT_TO_STDOUT);
#else /* !defined(DIRECT_ERROR_OUTPUT_TO_STDOUT) */
  comment_undefined_macro_name(DIRECT_ERROR_OUTPUT_TO_STDOUT);
#endif /* defined(DIRECT_ERROR_OUTPUT_TO_STDOUT) */
#if defined(DOING_SOURCE_ANALYSIS)
  define_numeric_valued_macro(DOING_SOURCE_ANALYSIS);
#else /* !defined(DOING_SOURCE_ANALYSIS) */
  comment_undefined_macro_name(DOING_SOURCE_ANALYSIS);
#endif /* defined(DOING_SOURCE_ANALYSIS) */
#if defined(DO_FULL_PORTABLE_EH_LOWERING)
  define_numeric_valued_macro(DO_FULL_PORTABLE_EH_LOWERING);
#else /* !defined(DO_FULL_PORTABLE_EH_LOWERING) */
  comment_undefined_macro_name(DO_FULL_PORTABLE_EH_LOWERING);
#endif /* defined(DO_FULL_PORTABLE_EH_LOWERING) */
#if defined(DO_IL_LOWERING)
  define_numeric_valued_macro(DO_IL_LOWERING);
#else /* !defined(DO_IL_LOWERING) */
  comment_undefined_macro_name(DO_IL_LOWERING);
#endif /* defined(DO_IL_LOWERING) */
#if defined(DO_NOT_ASSUME_QUESTION_IS_LARGER_THAN_END_OF_LINE)
  define_numeric_valued_macro(
                           DO_NOT_ASSUME_QUESTION_IS_LARGER_THAN_END_OF_LINE);
#else /* !defined(DO_NOT_ASSUME_QUESTION_IS_LARGER_THAN_END_OF_LINE) */
  comment_undefined_macro_name(
                           DO_NOT_ASSUME_QUESTION_IS_LARGER_THAN_END_OF_LINE);
#endif /* defined(DO_NOT_ASSUME_QUESTION_IS_LARGER_THAN_END_OF_LINE) */
#if defined(DO_RETURN_VALUE_OPTIMIZATION_IN_LOWERING)
  define_numeric_valued_macro(DO_RETURN_VALUE_OPTIMIZATION_IN_LOWERING);
#else /* !defined(DO_RETURN_VALUE_OPTIMIZATION_IN_LOWERING) */
  comment_undefined_macro_name(DO_RETURN_VALUE_OPTIMIZATION_IN_LOWERING);
#endif /* defined(DO_RETURN_VALUE_OPTIMIZATION_IN_LOWERING) */
#if defined(DO_UNORDERED_EH_PROCESSING)
  define_numeric_valued_macro(DO_UNORDERED_EH_PROCESSING);
#else /* !defined(DO_UNORDERED_EH_PROCESSING) */
  comment_undefined_macro_name(DO_UNORDERED_EH_PROCESSING);
#endif /* defined(DO_UNORDERED_EH_PROCESSING) */
#if defined(DRIVER_COMPATIBILITY_VERSION)
  define_numeric_valued_macro(DRIVER_COMPATIBILITY_VERSION);
#else /* !defined(DRIVER_COMPATIBILITY_VERSION) */
  comment_undefined_macro_name(DRIVER_COMPATIBILITY_VERSION);
#endif /* defined(DRIVER_COMPATIBILITY_VERSION) */
#if defined(DUMP_CONFIG_ENABLED)
  define_numeric_valued_macro(DUMP_CONFIG_ENABLED);
#else /* !defined(DUMP_CONFIG_ENABLED) */
  comment_undefined_macro_name(DUMP_CONFIG_ENABLED);
#endif /* defined(DUMP_CONFIG_ENABLED) */
#if defined(DUMP_LOWERED_EH_CONSTRUCTS_IN_C_GEN_BE)
  define_numeric_valued_macro(DUMP_LOWERED_EH_CONSTRUCTS_IN_C_GEN_BE);
#else /* !defined(DUMP_LOWERED_EH_CONSTRUCTS_IN_C_GEN_BE) */
  comment_undefined_macro_name(DUMP_LOWERED_EH_CONSTRUCTS_IN_C_GEN_BE);
#endif /* defined(DUMP_LOWERED_EH_CONSTRUCTS_IN_C_GEN_BE) */
#if defined(DUPLICATE_SPECIAL_STATICS_IN_INSTANTIATION_SLICES)
  define_numeric_valued_macro(
                            DUPLICATE_SPECIAL_STATICS_IN_INSTANTIATION_SLICES);
#else /* !defined(DUPLICATE_SPECIAL_STATICS_IN_INSTANTIATION_SLICES) */
  comment_undefined_macro_name(
                            DUPLICATE_SPECIAL_STATICS_IN_INSTANTIATION_SLICES);
#endif /* defined(DUPLICATE_SPECIAL_STATICS_IN_INSTANTIATION_SLICES) */
#if defined(EDG_AUXILIARY_INFO_DIR_NAME)
  define_string_valued_macro(EDG_AUXILIARY_INFO_DIR_NAME);
#else /* !defined(EDG_AUXILIARY_INFO_DIR_NAME) */
  comment_undefined_macro_name(EDG_AUXILIARY_INFO_DIR_NAME);
#endif /* defined(EDG_AUXILIARY_INFO_DIR_NAME) */
#if defined(EDG_FRONT_END_DAEMON)
  define_numeric_valued_macro(EDG_FRONT_END_DAEMON);
#else /* !defined(EDG_FRONT_END_DAEMON) */
  comment_undefined_macro_name(EDG_FRONT_END_DAEMON);
#endif /* defined(EDG_FRONT_END_DAEMON) */
#if defined(EDG_MAIN)
  define_string_valued_macro(EDG_MAIN);
#else /* !defined(EDG_MAIN) */
  comment_undefined_macro_name(EDG_MAIN);
#endif /* defined(EDG_MAIN) */
#if defined(EDG_MULTIBYTE_CHAR_TEST_MODE)
  define_numeric_valued_macro(EDG_MULTIBYTE_CHAR_TEST_MODE);
#else /* !defined(EDG_MULTIBYTE_CHAR_TEST_MODE) */
  comment_undefined_macro_name(EDG_MULTIBYTE_CHAR_TEST_MODE);
#endif /* defined(EDG_MULTIBYTE_CHAR_TEST_MODE) */
#if defined(EDG_NATIVE_MULTIBYTE_TEST_MODE)
  define_numeric_valued_macro(EDG_NATIVE_MULTIBYTE_TEST_MODE);
#else /* !defined(EDG_NATIVE_MULTIBYTE_TEST_MODE) */
  comment_undefined_macro_name(EDG_NATIVE_MULTIBYTE_TEST_MODE);
#endif /* defined(EDG_NATIVE_MULTIBYTE_TEST_MODE) */
#if defined(EDG_WIN32)
  define_numeric_valued_macro(EDG_WIN32);
#else /* !defined(EDG_WIN32) */
  comment_undefined_macro_name(EDG_WIN32);
#endif /* defined(EDG_WIN32) */
#if defined(EMBEDDED_C_ALLOWED)
  define_numeric_valued_macro(EMBEDDED_C_ALLOWED);
#else /* !defined(EMBEDDED_C_ALLOWED) */
  comment_undefined_macro_name(EMBEDDED_C_ALLOWED);
#endif /* defined(EMBEDDED_C_ALLOWED) */
#if defined(ENABLE_TRANS_UNIT_TEST_MODE)
  define_numeric_valued_macro(ENABLE_TRANS_UNIT_TEST_MODE);
#else /* !defined(ENABLE_TRANS_UNIT_TEST_MODE) */
  comment_undefined_macro_name(ENABLE_TRANS_UNIT_TEST_MODE);
#endif /* defined(ENABLE_TRANS_UNIT_TEST_MODE) */
#if defined(END_OF_LINE_COMMENTS_ALLOWED_IN_C_MODE)
  define_numeric_valued_macro(END_OF_LINE_COMMENTS_ALLOWED_IN_C_MODE);
#else /* !defined(END_OF_LINE_COMMENTS_ALLOWED_IN_C_MODE) */
  comment_undefined_macro_name(END_OF_LINE_COMMENTS_ALLOWED_IN_C_MODE);
#endif /* defined(END_OF_LINE_COMMENTS_ALLOWED_IN_C_MODE) */
#if defined(ENSURE_LOWERED_TYPE_LIST_ORDERING)
  define_numeric_valued_macro(ENSURE_LOWERED_TYPE_LIST_ORDERING);
#else /* !defined(ENSURE_LOWERED_TYPE_LIST_ORDERING) */
  comment_undefined_macro_name(ENSURE_LOWERED_TYPE_LIST_ORDERING);
#endif /* defined(ENSURE_LOWERED_TYPE_LIST_ORDERING) */
#if defined(ENTRY_NUMBER_SHARES_BITS_IN_PREFIX)
  define_numeric_valued_macro(ENTRY_NUMBER_SHARES_BITS_IN_PREFIX);
#else /* !defined(ENTRY_NUMBER_SHARES_BITS_IN_PREFIX) */
  comment_undefined_macro_name(ENTRY_NUMBER_SHARES_BITS_IN_PREFIX);
#endif /* defined(ENTRY_NUMBER_SHARES_BITS_IN_PREFIX) */
#if defined(ERROR_SEVERITY_EXPLICIT_IN_ERROR_MESSAGES)
  define_numeric_valued_macro(ERROR_SEVERITY_EXPLICIT_IN_ERROR_MESSAGES);
#else /* !defined(ERROR_SEVERITY_EXPLICIT_IN_ERROR_MESSAGES) */
  comment_undefined_macro_name(ERROR_SEVERITY_EXPLICIT_IN_ERROR_MESSAGES);
#endif /* defined(ERROR_SEVERITY_EXPLICIT_IN_ERROR_MESSAGES) */
#if defined(EXC_SPEC_IN_FUNC_TYPE_ENABLING_POSSIBLE)
  define_numeric_valued_macro(EXC_SPEC_IN_FUNC_TYPE_ENABLING_POSSIBLE);
#else /* !defined(EXC_SPEC_IN_FUNC_TYPE_ENABLING_POSSIBLE) */
  comment_undefined_macro_name(EXC_SPEC_IN_FUNC_TYPE_ENABLING_POSSIBLE);
#endif /* defined(EXC_SPEC_IN_FUNC_TYPE_ENABLING_POSSIBLE) */
#if defined(EXIT_ON_INTERNAL_ERROR)
  define_numeric_valued_macro(EXIT_ON_INTERNAL_ERROR);
#else /* !defined(EXIT_ON_INTERNAL_ERROR) */
  comment_undefined_macro_name(EXIT_ON_INTERNAL_ERROR);
#endif /* defined(EXIT_ON_INTERNAL_ERROR) */
#if defined(EXPENSIVE_CHECKING)
  define_numeric_valued_macro(EXPENSIVE_CHECKING);
#else /* !defined(EXPENSIVE_CHECKING) */
  comment_undefined_macro_name(EXPENSIVE_CHECKING);
#endif /* defined(EXPENSIVE_CHECKING) */
#if defined(EXPLICITLY_UNROLL_CRITICAL_LOOPS)
  define_numeric_valued_macro(EXPLICITLY_UNROLL_CRITICAL_LOOPS);
#else /* !defined(EXPLICITLY_UNROLL_CRITICAL_LOOPS) */
  comment_undefined_macro_name(EXPLICITLY_UNROLL_CRITICAL_LOOPS);
#endif /* defined(EXPLICITLY_UNROLL_CRITICAL_LOOPS) */
#if defined(EXPORTED_TEMPLATE_FILE_SUFFIX)
  define_string_valued_macro(EXPORTED_TEMPLATE_FILE_SUFFIX);
#else /* !defined(EXPORTED_TEMPLATE_FILE_SUFFIX) */
  comment_undefined_macro_name(EXPORTED_TEMPLATE_FILE_SUFFIX);
#endif /* defined(EXPORTED_TEMPLATE_FILE_SUFFIX) */
#if defined(EXPORT_ENABLING_POSSIBLE)
  define_numeric_valued_macro(EXPORT_ENABLING_POSSIBLE);
#else /* !defined(EXPORT_ENABLING_POSSIBLE) */
  comment_undefined_macro_name(EXPORT_ENABLING_POSSIBLE);
#endif /* defined(EXPORT_ENABLING_POSSIBLE) */
#if defined(EXPORT_INFO_FILE_NAME)
  define_string_valued_macro(EXPORT_INFO_FILE_NAME);
#else /* !defined(EXPORT_INFO_FILE_NAME) */
  comment_undefined_macro_name(EXPORT_INFO_FILE_NAME);
#endif /* defined(EXPORT_INFO_FILE_NAME) */
#if defined(EXTRA_SOURCE_POSITIONS_IN_IL)
  define_numeric_valued_macro(EXTRA_SOURCE_POSITIONS_IN_IL);
#else /* !defined(EXTRA_SOURCE_POSITIONS_IN_IL) */
  comment_undefined_macro_name(EXTRA_SOURCE_POSITIONS_IN_IL);
#endif /* defined(EXTRA_SOURCE_POSITIONS_IN_IL) */
#if defined(FAVOR_CONSTANT_RESULT_FOR_NONSTATIC_INIT)
  define_numeric_valued_macro(FAVOR_CONSTANT_RESULT_FOR_NONSTATIC_INIT);
#else /* !defined(FAVOR_CONSTANT_RESULT_FOR_NONSTATIC_INIT) */
  comment_undefined_macro_name(FAVOR_CONSTANT_RESULT_FOR_NONSTATIC_INIT);
#endif /* defined(FAVOR_CONSTANT_RESULT_FOR_NONSTATIC_INIT) */
#if defined(FILE_NAME_FOR_STDIN)
  define_string_valued_macro(FILE_NAME_FOR_STDIN);
#else /* !defined(FILE_NAME_FOR_STDIN) */
  comment_undefined_macro_name(FILE_NAME_FOR_STDIN);
#endif /* defined(FILE_NAME_FOR_STDIN) */
#if defined(FIXED_ADDRESS_FOR_MMAP)
  define_numeric_valued_macro(FIXED_ADDRESS_FOR_MMAP);
#else /* !defined(FIXED_ADDRESS_FOR_MMAP) */
  comment_undefined_macro_name(FIXED_ADDRESS_FOR_MMAP);
#endif /* defined(FIXED_ADDRESS_FOR_MMAP) */
#if defined(FIXED_POINT_ALLOWED)
  define_numeric_valued_macro(FIXED_POINT_ALLOWED);
#else /* !defined(FIXED_POINT_ALLOWED) */
  comment_undefined_macro_name(FIXED_POINT_ALLOWED);
#endif /* defined(FIXED_POINT_ALLOWED) */
#if defined(FLOAT128_ENABLING_POSSIBLE)
  define_numeric_valued_macro(FLOAT128_ENABLING_POSSIBLE);
#else /* !defined(FLOAT128_ENABLING_POSSIBLE) */
  comment_undefined_macro_name(FLOAT128_ENABLING_POSSIBLE);
#endif /* defined(FLOAT128_ENABLING_POSSIBLE) */
#if defined(FLOAT80_ENABLING_POSSIBLE)
  define_numeric_valued_macro(FLOAT80_ENABLING_POSSIBLE);
#else /* !defined(FLOAT80_ENABLING_POSSIBLE) */
  comment_undefined_macro_name(FLOAT80_ENABLING_POSSIBLE);
#endif /* defined(FLOAT80_ENABLING_POSSIBLE) */
#if defined(FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS)
  define_numeric_valued_macro(FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS);
#else /* !defined(FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS) */
  comment_undefined_macro_name(FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS);
#endif /* defined(FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS) */
#if defined(FORCE_VARIABLE_DEFINITION_VIA_ZEROING)
  define_numeric_valued_macro(FORCE_VARIABLE_DEFINITION_VIA_ZEROING);
#else /* !defined(FORCE_VARIABLE_DEFINITION_VIA_ZEROING) */
  comment_undefined_macro_name(FORCE_VARIABLE_DEFINITION_VIA_ZEROING);
#endif /* defined(FORCE_VARIABLE_DEFINITION_VIA_ZEROING) */
#if defined(FP_HAS_LONG_DOUBLE)
  define_numeric_valued_macro(FP_HAS_LONG_DOUBLE);
#else /* !defined(FP_HAS_LONG_DOUBLE) */
  comment_undefined_macro_name(FP_HAS_LONG_DOUBLE);
#endif /* defined(FP_HAS_LONG_DOUBLE) */
#if defined(FP_LONG_DOUBLE_IS_80BIT_EXTENDED)
  define_numeric_valued_macro(FP_LONG_DOUBLE_IS_80BIT_EXTENDED);
#else /* !defined(FP_LONG_DOUBLE_IS_80BIT_EXTENDED) */
  comment_undefined_macro_name(FP_LONG_DOUBLE_IS_80BIT_EXTENDED);
#endif /* defined(FP_LONG_DOUBLE_IS_80BIT_EXTENDED) */
#if defined(FP_LONG_DOUBLE_IS_BINARY128)
  define_numeric_valued_macro(FP_LONG_DOUBLE_IS_BINARY128);
#else /* !defined(FP_LONG_DOUBLE_IS_BINARY128) */
  comment_undefined_macro_name(FP_LONG_DOUBLE_IS_BINARY128);
#endif /* defined(FP_LONG_DOUBLE_IS_BINARY128) */
#if defined(FP_LONG_DOUBLE_IS_BINARY64)
  define_numeric_valued_macro(FP_LONG_DOUBLE_IS_BINARY64);
#else /* !defined(FP_LONG_DOUBLE_IS_BINARY64) */
  comment_undefined_macro_name(FP_LONG_DOUBLE_IS_BINARY64);
#endif /* defined(FP_LONG_DOUBLE_IS_BINARY64) */
#if defined(FP_USE_EMULATION)
  define_numeric_valued_macro(FP_USE_EMULATION);
#else /* !defined(FP_USE_EMULATION) */
  comment_undefined_macro_name(FP_USE_EMULATION);
#endif /* defined(FP_USE_EMULATION) */
#if defined(FREE_MEMORY_REGIONS_EARLY)
  define_numeric_valued_macro(FREE_MEMORY_REGIONS_EARLY);
#else /* !defined(FREE_MEMORY_REGIONS_EARLY) */
  comment_undefined_macro_name(FREE_MEMORY_REGIONS_EARLY);
#endif /* defined(FREE_MEMORY_REGIONS_EARLY) */
#if defined(FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS)
  define_numeric_valued_macro(
                      FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS);
#else /* !defined(FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS) */
  comment_undefined_macro_name(
                      FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS);
#endif /* defined(FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS) */
#if defined(FULLY_RESOLVED_MACRO_POSITIONS)
  define_numeric_valued_macro(FULLY_RESOLVED_MACRO_POSITIONS);
#else /* !defined(FULLY_RESOLVED_MACRO_POSITIONS) */
  comment_undefined_macro_name(FULLY_RESOLVED_MACRO_POSITIONS);
#endif /* defined(FULLY_RESOLVED_MACRO_POSITIONS) */
#if defined(FUNCTION_PROTOTYPE_INSTANTIATION_DEFERRAL_ALLOWED)
  define_numeric_valued_macro(
                            FUNCTION_PROTOTYPE_INSTANTIATION_DEFERRAL_ALLOWED);
#else /* !defined(FUNCTION_PROTOTYPE_INSTANTIATION_DEFERRAL_ALLOWED) */
  comment_undefined_macro_name(
                            FUNCTION_PROTOTYPE_INSTANTIATION_DEFERRAL_ALLOWED);
#endif /* defined(FUNCTION_PROTOTYPE_INSTANTIATION_DEFERRAL_ALLOWED) */
#if defined(FUNC_AVAILABLE)
  define_numeric_valued_macro(FUNC_AVAILABLE);
#else /* !defined(FUNC_AVAILABLE) */
  comment_undefined_macro_name(FUNC_AVAILABLE);
#endif /* defined(FUNC_AVAILABLE) */
#if defined(GCC_BUILTIN_VARARGS)
  define_numeric_valued_macro(GCC_BUILTIN_VARARGS);
#else /* !defined(GCC_BUILTIN_VARARGS) */
  comment_undefined_macro_name(GCC_BUILTIN_VARARGS);
#endif /* defined(GCC_BUILTIN_VARARGS) */
#if defined(GCC_BUILTIN_VARARGS_IN_GENERATED_CODE)
  define_numeric_valued_macro(GCC_BUILTIN_VARARGS_IN_GENERATED_CODE);
#else /* !defined(GCC_BUILTIN_VARARGS_IN_GENERATED_CODE) */
  comment_undefined_macro_name(GCC_BUILTIN_VARARGS_IN_GENERATED_CODE);
#endif /* defined(GCC_BUILTIN_VARARGS_IN_GENERATED_CODE) */
#if defined(GCC_IS_GENERATED_CODE_TARGET)
  define_numeric_valued_macro(GCC_IS_GENERATED_CODE_TARGET);
#else /* !defined(GCC_IS_GENERATED_CODE_TARGET) */
  comment_undefined_macro_name(GCC_IS_GENERATED_CODE_TARGET);
#endif /* defined(GCC_IS_GENERATED_CODE_TARGET) */
#if defined(GCC_VERSION_STRING)
#if !defined(_lint) && !(defined(_MSC_VER) && _MSC_VER < 1300)
  /* Microsoft version 6.0 and some lint versions have a preprocessor bug
     that creates an invalid result when the '#' operator is applied to the
     default value of GCC_VERSION_STRING. */
  define_string_valued_macro(GCC_VERSION_STRING);
#endif /* ifndef _lint */
#else /* !defined(GCC_VERSION_STRING) */
  comment_undefined_macro_name(GCC_VERSION_STRING);
#endif /* defined(GCC_VERSION_STRING) */
#if defined(GENERATE_EH_TABLES)
  define_numeric_valued_macro(GENERATE_EH_TABLES);
#else /* !defined(GENERATE_EH_TABLES) */
  comment_undefined_macro_name(GENERATE_EH_TABLES);
#endif /* defined(GENERATE_EH_TABLES) */
#if defined(GENERATE_LINKAGE_SPEC_BLOCKS)
  define_numeric_valued_macro(GENERATE_LINKAGE_SPEC_BLOCKS);
#else /* !defined(GENERATE_LINKAGE_SPEC_BLOCKS) */
  comment_undefined_macro_name(GENERATE_LINKAGE_SPEC_BLOCKS);
#endif /* defined(GENERATE_LINKAGE_SPEC_BLOCKS) */
#if defined(GENERATE_MICROSOFT_IF_EXISTS_ENTRIES)
  define_numeric_valued_macro(GENERATE_MICROSOFT_IF_EXISTS_ENTRIES);
#else /* !defined(GENERATE_MICROSOFT_IF_EXISTS_ENTRIES) */
  comment_undefined_macro_name(GENERATE_MICROSOFT_IF_EXISTS_ENTRIES);
#endif /* defined(GENERATE_MICROSOFT_IF_EXISTS_ENTRIES) */
#if defined(GENERATE_SOURCE_SEQUENCE_LISTS)
  define_numeric_valued_macro(GENERATE_SOURCE_SEQUENCE_LISTS);
#else /* !defined(GENERATE_SOURCE_SEQUENCE_LISTS) */
  comment_undefined_macro_name(GENERATE_SOURCE_SEQUENCE_LISTS);
#endif /* defined(GENERATE_SOURCE_SEQUENCE_LISTS) */
#if defined(GEN_CPP_FILE_SUFFIX)
  define_string_valued_macro(GEN_CPP_FILE_SUFFIX);
#else /* !defined(GEN_CPP_FILE_SUFFIX) */
  comment_undefined_macro_name(GEN_CPP_FILE_SUFFIX);
#endif /* defined(GEN_CPP_FILE_SUFFIX) */
#if defined(GEN_C_FILE_SUFFIX)
  define_string_valued_macro(GEN_C_FILE_SUFFIX);
#else /* !defined(GEN_C_FILE_SUFFIX) */
  comment_undefined_macro_name(GEN_C_FILE_SUFFIX);
#endif /* defined(GEN_C_FILE_SUFFIX) */
#if defined(GEN_EXTRA_LINE_ID_INFO)
  define_numeric_valued_macro(GEN_EXTRA_LINE_ID_INFO);
#else /* !defined(GEN_EXTRA_LINE_ID_INFO) */
  comment_undefined_macro_name(GEN_EXTRA_LINE_ID_INFO);
#endif /* defined(GEN_EXTRA_LINE_ID_INFO) */
#if defined(GET_DEFINITION_OF_CLASS_NEEDED)
  define_numeric_valued_macro(GET_DEFINITION_OF_CLASS_NEEDED);
#else /* !defined(GET_DEFINITION_OF_CLASS_NEEDED) */
  comment_undefined_macro_name(GET_DEFINITION_OF_CLASS_NEEDED);
#endif /* defined(GET_DEFINITION_OF_CLASS_NEEDED) */
#if defined(GNU_EXTENSIONS_ALLOWED)
  define_numeric_valued_macro(GNU_EXTENSIONS_ALLOWED);
#else /* !defined(GNU_EXTENSIONS_ALLOWED) */
  comment_undefined_macro_name(GNU_EXTENSIONS_ALLOWED);
#endif /* defined(GNU_EXTENSIONS_ALLOWED) */
#if defined(GNU_FUNCTION_MULTIVERSIONING)
  define_numeric_valued_macro(GNU_FUNCTION_MULTIVERSIONING);
#else /* !defined(GNU_FUNCTION_MULTIVERSIONING) */
  comment_undefined_macro_name(GNU_FUNCTION_MULTIVERSIONING);
#endif /* defined(GNU_FUNCTION_MULTIVERSIONING) */
#if defined(GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED)
  define_numeric_valued_macro(GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED);
#else /* !defined(GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED) */
  comment_undefined_macro_name(GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED);
#endif /* defined(GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED) */
#if defined(GNU_NAKED_ATTRIBUTE_ALLOWED)
  define_numeric_valued_macro(GNU_NAKED_ATTRIBUTE_ALLOWED);
#else /* !defined(GNU_NAKED_ATTRIBUTE_ALLOWED) */
  comment_undefined_macro_name(GNU_NAKED_ATTRIBUTE_ALLOWED);
#endif /* defined(GNU_NAKED_ATTRIBUTE_ALLOWED) */
#if defined(GNU_TARGET_VERSION_NUMBER)
  define_numeric_valued_macro(GNU_TARGET_VERSION_NUMBER);
#else /* !defined(GNU_TARGET_VERSION_NUMBER) */
  comment_undefined_macro_name(GNU_TARGET_VERSION_NUMBER);
#endif /* defined(GNU_TARGET_VERSION_NUMBER) */
#if defined(GNU_VECTOR_TYPES_ALLOWED)
  define_numeric_valued_macro(GNU_VECTOR_TYPES_ALLOWED);
#else /* !defined(GNU_VECTOR_TYPES_ALLOWED) */
  comment_undefined_macro_name(GNU_VECTOR_TYPES_ALLOWED);
#endif /* defined(GNU_VECTOR_TYPES_ALLOWED) */
#if defined(GNU_VISIBILITY_ATTRIBUTE_ALLOWED)
  define_numeric_valued_macro(GNU_VISIBILITY_ATTRIBUTE_ALLOWED);
#else /* !defined(GNU_VISIBILITY_ATTRIBUTE_ALLOWED) */
  comment_undefined_macro_name(GNU_VISIBILITY_ATTRIBUTE_ALLOWED);
#endif /* defined(GNU_VISIBILITY_ATTRIBUTE_ALLOWED) */
#if defined(GNU_X86_ASM_EXTENSIONS_ALLOWED)
  define_numeric_valued_macro(GNU_X86_ASM_EXTENSIONS_ALLOWED);
#else /* !defined(GNU_X86_ASM_EXTENSIONS_ALLOWED) */
  comment_undefined_macro_name(GNU_X86_ASM_EXTENSIONS_ALLOWED);
#endif /* defined(GNU_X86_ASM_EXTENSIONS_ALLOWED) */
#if defined(GNU_X86_ATTRIBUTES_ALLOWED)
  define_numeric_valued_macro(GNU_X86_ATTRIBUTES_ALLOWED);
#else /* !defined(GNU_X86_ATTRIBUTES_ALLOWED) */
  comment_undefined_macro_name(GNU_X86_ATTRIBUTES_ALLOWED);
#endif /* defined(GNU_X86_ATTRIBUTES_ALLOWED) */
#if defined(GUARD_MACRO2_FOR_VA_LIST)
  define_string_valued_macro(GUARD_MACRO2_FOR_VA_LIST);
#else /* !defined(GUARD_MACRO2_FOR_VA_LIST) */
  comment_undefined_macro_name(GUARD_MACRO2_FOR_VA_LIST);
#endif /* defined(GUARD_MACRO2_FOR_VA_LIST) */
#if defined(GUARD_MACRO_FOR_VA_LIST)
  define_string_valued_macro(GUARD_MACRO_FOR_VA_LIST);
#else /* !defined(GUARD_MACRO_FOR_VA_LIST) */
  comment_undefined_macro_name(GUARD_MACRO_FOR_VA_LIST);
#endif /* defined(GUARD_MACRO_FOR_VA_LIST) */
#if defined(HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS)
  define_numeric_valued_macro(HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS);
#else /* !defined(HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS) */
  comment_undefined_macro_name(HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS);
#endif /* defined(HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS) */
#if defined(HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS)
  define_numeric_valued_macro(HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS);
#else /* !defined(HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS) */
  comment_undefined_macro_name(HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS);
#endif /* defined(HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS) */
#if defined(HOSTID)
  define_string_valued_macro(HOSTID);
#else /* !defined(HOSTID) */
  comment_undefined_macro_name(HOSTID);
#endif /* defined(HOSTID) */
#if defined(HOSTID2)
  define_string_valued_macro(HOSTID2);
#else /* !defined(HOSTID2) */
  comment_undefined_macro_name(HOSTID2);
#endif /* defined(HOSTID2) */
#if defined(HOST_ALIGNMENT_REQUIRED)
  define_numeric_valued_macro(HOST_ALIGNMENT_REQUIRED);
#else /* !defined(HOST_ALIGNMENT_REQUIRED) */
  comment_undefined_macro_name(HOST_ALIGNMENT_REQUIRED);
#endif /* defined(HOST_ALIGNMENT_REQUIRED) */
#if defined(HOST_ALLOCATION_INCREMENT)
  define_numeric_valued_macro(HOST_ALLOCATION_INCREMENT);
#else /* !defined(HOST_ALLOCATION_INCREMENT) */
  comment_undefined_macro_name(HOST_ALLOCATION_INCREMENT);
#endif /* defined(HOST_ALLOCATION_INCREMENT) */
#if defined(HOST_COMPILER_SUPPORTS_BFLOAT16)
  define_numeric_valued_macro(HOST_COMPILER_SUPPORTS_BFLOAT16);
#else /* !defined(HOST_COMPILER_SUPPORTS_BFLOAT16) */
  comment_undefined_macro_name(HOST_COMPILER_SUPPORTS_BFLOAT16);
#endif /* defined(HOST_COMPILER_SUPPORTS_BFLOAT16) */
#if defined(HOST_FP_VALUE_IS_128BIT)
  define_numeric_valued_macro(HOST_FP_VALUE_IS_128BIT);
#else /* !defined(HOST_FP_VALUE_IS_128BIT) */
  comment_undefined_macro_name(HOST_FP_VALUE_IS_128BIT);
#endif /* defined(HOST_FP_VALUE_IS_128BIT) */
#if defined(HOST_HAS_FLOAT16_TYPE)
  define_numeric_valued_macro(HOST_HAS_FLOAT16_TYPE);
#else /* !defined(HOST_HAS_FLOAT16_TYPE) */
  comment_undefined_macro_name(HOST_HAS_FLOAT16_TYPE);
#endif /* defined(HOST_HAS_FLOAT16_TYPE) */
#if defined(HOST_HAS_INT128_EXTENSIONS)
  define_numeric_valued_macro(HOST_HAS_INT128_EXTENSIONS);
#else /* !defined(HOST_HAS_INT128_EXTENSIONS) */
  comment_undefined_macro_name(HOST_HAS_INT128_EXTENSIONS);
#endif /* defined(HOST_HAS_INT128_EXTENSIONS) */
#if defined(HOST_IL_ENTRY_PREFIX_ALIGNMENT)
  define_numeric_valued_macro(HOST_IL_ENTRY_PREFIX_ALIGNMENT);
#else /* !defined(HOST_IL_ENTRY_PREFIX_ALIGNMENT) */
  comment_undefined_macro_name(HOST_IL_ENTRY_PREFIX_ALIGNMENT);
#endif /* defined(HOST_IL_ENTRY_PREFIX_ALIGNMENT) */
#if defined(HOST_POINTER_ALIGNMENT)
  define_numeric_valued_macro(HOST_POINTER_ALIGNMENT);
#else /* !defined(HOST_POINTER_ALIGNMENT) */
  comment_undefined_macro_name(HOST_POINTER_ALIGNMENT);
#endif /* defined(HOST_POINTER_ALIGNMENT) */
#if defined(HOST_TARGET_ENDIAN_MISMATCH_OKAY)
  define_numeric_valued_macro(HOST_TARGET_ENDIAN_MISMATCH_OKAY);
#else /* !defined(HOST_TARGET_ENDIAN_MISMATCH_OKAY) */
  comment_undefined_macro_name(HOST_TARGET_ENDIAN_MISMATCH_OKAY);
#endif /* defined(HOST_TARGET_ENDIAN_MISMATCH_OKAY) */
#if defined(IA64_ABI)
  define_numeric_valued_macro(IA64_ABI);
#else /* !defined(IA64_ABI) */
  comment_undefined_macro_name(IA64_ABI);
#endif /* defined(IA64_ABI) */
#if defined(IDENTIFIER_STRINGS_ALLOW_MULTIBYTE_CHARS)
  define_numeric_valued_macro(IDENTIFIER_STRINGS_ALLOW_MULTIBYTE_CHARS);
#else /* !defined(IDENTIFIER_STRINGS_ALLOW_MULTIBYTE_CHARS) */
  comment_undefined_macro_name(IDENTIFIER_STRINGS_ALLOW_MULTIBYTE_CHARS);
#endif /* defined(IDENTIFIER_STRINGS_ALLOW_MULTIBYTE_CHARS) */
#if defined(IDENT_DIRECTIVE_AND_PRAGMA)
  define_numeric_valued_macro(IDENT_DIRECTIVE_AND_PRAGMA);
#else /* !defined(IDENT_DIRECTIVE_AND_PRAGMA) */
  comment_undefined_macro_name(IDENT_DIRECTIVE_AND_PRAGMA);
#endif /* defined(IDENT_DIRECTIVE_AND_PRAGMA) */
#if defined(IGNORE_CARRIAGE_RETURN_IN_SOURCE)
#if READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS
  /* IGNORE_CARRIAGE_RETURN_IN_SOURCE is defined unconditionally in
     host_envir.h when READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS is TRUE, so just
     comment its value instead of printing a #define for it. */
  comment_numeric_valued_macro(IGNORE_CARRIAGE_RETURN_IN_SOURCE);
#else /* !READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS */
  define_numeric_valued_macro(IGNORE_CARRIAGE_RETURN_IN_SOURCE);
#endif /* READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS */
#else /* !defined(IGNORE_CARRIAGE_RETURN_IN_SOURCE) */
  comment_undefined_macro_name(IGNORE_CARRIAGE_RETURN_IN_SOURCE);
#endif /* defined(IGNORE_CARRIAGE_RETURN_IN_SOURCE) */
#if defined(IL_FILE_SUFFIX)
  define_string_valued_macro(IL_FILE_SUFFIX);
#else /* !defined(IL_FILE_SUFFIX) */
  comment_undefined_macro_name(IL_FILE_SUFFIX);
#endif /* defined(IL_FILE_SUFFIX) */
#if defined(IL_LOWERING_INIT_ROUTINE_PREFIX)
  define_string_valued_macro(IL_LOWERING_INIT_ROUTINE_PREFIX);
#else /* !defined(IL_LOWERING_INIT_ROUTINE_PREFIX) */
  comment_undefined_macro_name(IL_LOWERING_INIT_ROUTINE_PREFIX);
#endif /* defined(IL_LOWERING_INIT_ROUTINE_PREFIX) */
#if defined(IL_SHOULD_BE_WRITTEN_TO_FILE)
  define_numeric_valued_macro(IL_SHOULD_BE_WRITTEN_TO_FILE);
#else /* !defined(IL_SHOULD_BE_WRITTEN_TO_FILE) */
  comment_undefined_macro_name(IL_SHOULD_BE_WRITTEN_TO_FILE);
#endif /* defined(IL_SHOULD_BE_WRITTEN_TO_FILE) */
#if defined(IL_VERSION_NUMBER)
  define_string_valued_macro(IL_VERSION_NUMBER);
#else /* !defined(IL_VERSION_NUMBER) */
  comment_undefined_macro_name(IL_VERSION_NUMBER);
#endif /* defined(IL_VERSION_NUMBER) */
#if defined(IL_WALK_NEEDED)
  define_numeric_valued_macro(IL_WALK_NEEDED);
#else /* !defined(IL_WALK_NEEDED) */
  comment_undefined_macro_name(IL_WALK_NEEDED);
#endif /* defined(IL_WALK_NEEDED) */
#if defined(IMPLEMENTATION_SUPPORTS_MULTIPLE_THREADS)
  define_numeric_valued_macro(IMPLEMENTATION_SUPPORTS_MULTIPLE_THREADS);
#else /* !defined(IMPLEMENTATION_SUPPORTS_MULTIPLE_THREADS) */
  comment_undefined_macro_name(IMPLEMENTATION_SUPPORTS_MULTIPLE_THREADS);
#endif /* defined(IMPLEMENTATION_SUPPORTS_MULTIPLE_THREADS) */
#if defined(IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE)
  define_numeric_valued_macro(
                           IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE);
#else /* !defined(IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE) */
  comment_undefined_macro_name(
                           IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE);
#endif /* defined(IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE) */
#if defined(INCLUDE_COMMENTS_IN_ASM_FUNC_BODY)
  define_numeric_valued_macro(INCLUDE_COMMENTS_IN_ASM_FUNC_BODY);
#else /* !defined(INCLUDE_COMMENTS_IN_ASM_FUNC_BODY) */
  comment_undefined_macro_name(INCLUDE_COMMENTS_IN_ASM_FUNC_BODY);
#endif /* defined(INCLUDE_COMMENTS_IN_ASM_FUNC_BODY) */
#if defined(INCLUDE_EDG_TEST_ATTRIBUTES)
  define_numeric_valued_macro(INCLUDE_EDG_TEST_ATTRIBUTES);
#else /* !defined(INCLUDE_EDG_TEST_ATTRIBUTES) */
  comment_undefined_macro_name(INCLUDE_EDG_TEST_ATTRIBUTES);
#endif /* defined(INCLUDE_EDG_TEST_ATTRIBUTES) */
#if defined(INCLUDE_EDG_TEST_NAMED_ADDRESS_SPACES)
  define_numeric_valued_macro(INCLUDE_EDG_TEST_NAMED_ADDRESS_SPACES);
#else /* !defined(INCLUDE_EDG_TEST_NAMED_ADDRESS_SPACES) */
  comment_undefined_macro_name(INCLUDE_EDG_TEST_NAMED_ADDRESS_SPACES);
#endif /* defined(INCLUDE_EDG_TEST_NAMED_ADDRESS_SPACES) */
#if defined(INCLUDE_EDG_TEST_NAMED_REGISTERS)
  define_numeric_valued_macro(INCLUDE_EDG_TEST_NAMED_REGISTERS);
#else /* !defined(INCLUDE_EDG_TEST_NAMED_REGISTERS) */
  comment_undefined_macro_name(INCLUDE_EDG_TEST_NAMED_REGISTERS);
#endif /* defined(INCLUDE_EDG_TEST_NAMED_REGISTERS) */
#if defined(INCLUDE_EDG_TEST_PRAGMAS)
  define_numeric_valued_macro(INCLUDE_EDG_TEST_PRAGMAS);
#else /* !defined(INCLUDE_EDG_TEST_PRAGMAS) */
  comment_undefined_macro_name(INCLUDE_EDG_TEST_PRAGMAS);
#endif /* defined(INCLUDE_EDG_TEST_PRAGMAS) */
#if defined(INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL)
  define_numeric_valued_macro(INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL);
#else /* !defined(INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL) */
  comment_undefined_macro_name(INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL);
#endif /* defined(INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL) */
#if defined(INDICATE_CLEANUP_STATE_IN_UNREACHABLE_CODE)
  define_numeric_valued_macro(INDICATE_CLEANUP_STATE_IN_UNREACHABLE_CODE);
#else /* !defined(INDICATE_CLEANUP_STATE_IN_UNREACHABLE_CODE) */
  comment_undefined_macro_name(INDICATE_CLEANUP_STATE_IN_UNREACHABLE_CODE);
#endif /* defined(INDICATE_CLEANUP_STATE_IN_UNREACHABLE_CODE) */
#if defined(INSTANTIATE_BEFORE_PCH_CREATION)
  define_numeric_valued_macro(INSTANTIATE_BEFORE_PCH_CREATION);
#else /* !defined(INSTANTIATE_BEFORE_PCH_CREATION) */
  comment_undefined_macro_name(INSTANTIATE_BEFORE_PCH_CREATION);
#endif /* defined(INSTANTIATE_BEFORE_PCH_CREATION) */
#if defined(INSTANTIATE_EXTERN_INLINE)
  define_numeric_valued_macro(INSTANTIATE_EXTERN_INLINE);
#else /* !defined(INSTANTIATE_EXTERN_INLINE) */
  comment_undefined_macro_name(INSTANTIATE_EXTERN_INLINE);
#endif /* defined(INSTANTIATE_EXTERN_INLINE) */
#if defined(INSTANTIATE_INLINE_VARIABLES)
  define_numeric_valued_macro(INSTANTIATE_INLINE_VARIABLES);
#else /* !defined(INSTANTIATE_INLINE_VARIABLES) */
  comment_undefined_macro_name(INSTANTIATE_INLINE_VARIABLES);
#endif /* defined(INSTANTIATE_INLINE_VARIABLES) */
#if defined(INSTANTIATE_TEMPLATES_EVERYWHERE_USED)
  define_numeric_valued_macro(INSTANTIATE_TEMPLATES_EVERYWHERE_USED);
#else /* !defined(INSTANTIATE_TEMPLATES_EVERYWHERE_USED) */
  comment_undefined_macro_name(INSTANTIATE_TEMPLATES_EVERYWHERE_USED);
#endif /* defined(INSTANTIATE_TEMPLATES_EVERYWHERE_USED) */
#if defined(INSTANTIATION_BY_IMPLICIT_INCLUSION)
  define_numeric_valued_macro(INSTANTIATION_BY_IMPLICIT_INCLUSION);
#else /* !defined(INSTANTIATION_BY_IMPLICIT_INCLUSION) */
  comment_undefined_macro_name(INSTANTIATION_BY_IMPLICIT_INCLUSION);
#endif /* defined(INSTANTIATION_BY_IMPLICIT_INCLUSION) */
#if defined(INSTANTIATION_FILE_SUFFIX)
  define_string_valued_macro(INSTANTIATION_FILE_SUFFIX);
#else /* !defined(INSTANTIATION_FILE_SUFFIX) */
  comment_undefined_macro_name(INSTANTIATION_FILE_SUFFIX);
#endif /* defined(INSTANTIATION_FILE_SUFFIX) */
#if defined(INSTANTIATION_FLAGS_IN_TEMPLATE_INFO_FILE)
  define_numeric_valued_macro(INSTANTIATION_FLAGS_IN_TEMPLATE_INFO_FILE);
#else /* !defined(INSTANTIATION_FLAGS_IN_TEMPLATE_INFO_FILE) */
  comment_undefined_macro_name(INSTANTIATION_FLAGS_IN_TEMPLATE_INFO_FILE);
#endif /* defined(INSTANTIATION_FLAGS_IN_TEMPLATE_INFO_FILE) */
#if defined(INSTANTIATION_REQUEST_LINES_RESERVED)
  define_numeric_valued_macro(INSTANTIATION_REQUEST_LINES_RESERVED);
#else /* !defined(INSTANTIATION_REQUEST_LINES_RESERVED) */
  comment_undefined_macro_name(INSTANTIATION_REQUEST_LINES_RESERVED);
#endif /* defined(INSTANTIATION_REQUEST_LINES_RESERVED) */
#if defined(INT128_EXTENSIONS_ALLOWED)
  define_numeric_valued_macro(INT128_EXTENSIONS_ALLOWED);
#else /* !defined(INT128_EXTENSIONS_ALLOWED) */
  comment_undefined_macro_name(INT128_EXTENSIONS_ALLOWED);
#endif /* defined(INT128_EXTENSIONS_ALLOWED) */
#if defined(INTEGER_VALUE_REPR_IS_A_HOST_INTEGER)
  define_numeric_valued_macro(INTEGER_VALUE_REPR_IS_A_HOST_INTEGER);
#else /* !defined(INTEGER_VALUE_REPR_IS_A_HOST_INTEGER) */
  comment_undefined_macro_name(INTEGER_VALUE_REPR_IS_A_HOST_INTEGER);
#endif /* defined(INTEGER_VALUE_REPR_IS_A_HOST_INTEGER) */
#if defined(ISSUE_WARNING_ON_LONG_DOUBLE_AS_DOUBLE)
  define_numeric_valued_macro(ISSUE_WARNING_ON_LONG_DOUBLE_AS_DOUBLE);
#else /* !defined(ISSUE_WARNING_ON_LONG_DOUBLE_AS_DOUBLE) */
  comment_undefined_macro_name(ISSUE_WARNING_ON_LONG_DOUBLE_AS_DOUBLE);
#endif /* defined(ISSUE_WARNING_ON_LONG_DOUBLE_AS_DOUBLE) */
#if defined(KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED)
  define_numeric_valued_macro(
                      KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED);
#else /* !defined(KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED) */
  comment_undefined_macro_name(
                      KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED);
#endif /* defined(KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED) */
#if defined(LARGE_IL_FILE_SUPPORT)
  define_numeric_valued_macro(LARGE_IL_FILE_SUPPORT);
#else /* !defined(LARGE_IL_FILE_SUPPORT) */
  comment_undefined_macro_name(LARGE_IL_FILE_SUPPORT);
#endif /* defined(LARGE_IL_FILE_SUPPORT) */
#if defined(LAZY_INITIALIZATION_USES_WEAK_REFERENCES)
  define_numeric_valued_macro(LAZY_INITIALIZATION_USES_WEAK_REFERENCES);
#else /* !defined(LAZY_INITIALIZATION_USES_WEAK_REFERENCES) */
  comment_undefined_macro_name(LAZY_INITIALIZATION_USES_WEAK_REFERENCES);
#endif /* defined(LAZY_INITIALIZATION_USES_WEAK_REFERENCES) */
#if defined(LINKER_CAN_DISCARD_DUPLICATE_DEFINITIONS)
  define_numeric_valued_macro(LINKER_CAN_DISCARD_DUPLICATE_DEFINITIONS);
#else /* !defined(LINKER_CAN_DISCARD_DUPLICATE_DEFINITIONS) */
  comment_undefined_macro_name(LINKER_CAN_DISCARD_DUPLICATE_DEFINITIONS);
#endif /* defined(LINKER_CAN_DISCARD_DUPLICATE_DEFINITIONS) */
#if defined(LOCALE_TO_SET_WHEN_MULTIBYTE_CHARS_ENABLED)
  define_string_valued_macro(LOCALE_TO_SET_WHEN_MULTIBYTE_CHARS_ENABLED);
#else /* !defined(LOCALE_TO_SET_WHEN_MULTIBYTE_CHARS_ENABLED) */
  comment_undefined_macro_name(LOCALE_TO_SET_WHEN_MULTIBYTE_CHARS_ENABLED);
#endif /* defined(LOCALE_TO_SET_WHEN_MULTIBYTE_CHARS_ENABLED) */
#if defined(LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C)
  define_numeric_valued_macro(LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C);
#else /* !defined(LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C) */
  comment_undefined_macro_name(LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C);
#endif /* defined(LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C) */
#if defined(LONG_LONG_ALLOWED)
  define_numeric_valued_macro(LONG_LONG_ALLOWED);
#else /* !defined(LONG_LONG_ALLOWED) */
  comment_undefined_macro_name(LONG_LONG_ALLOWED);
#endif /* defined(LONG_LONG_ALLOWED) */
#if defined(LOWERING_NORMALIZES_BOOLEAN_CONTROLLING_EXPRESSIONS)
  define_numeric_valued_macro(
                         LOWERING_NORMALIZES_BOOLEAN_CONTROLLING_EXPRESSIONS);
#else /* !defined(LOWERING_NORMALIZES_BOOLEAN_CONTROLLING_EXPRESSIONS) */
  comment_undefined_macro_name(
                         LOWERING_NORMALIZES_BOOLEAN_CONTROLLING_EXPRESSIONS);
#endif /* defined(LOWERING_NORMALIZES_BOOLEAN_CONTROLLING_EXPRESSIONS) */
#if defined(LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS)
  define_numeric_valued_macro(
                    LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS);
#else /* !defined(LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS) */
  comment_undefined_macro_name(
                    LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS);
#endif /* defined(LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS) */
#if defined(LOWER_CLASS_RVALUE_ADJUST)
  define_numeric_valued_macro(LOWER_CLASS_RVALUE_ADJUST);
#else /* !defined(LOWER_CLASS_RVALUE_ADJUST) */
  comment_undefined_macro_name(LOWER_CLASS_RVALUE_ADJUST);
#endif /* defined(LOWER_CLASS_RVALUE_ADJUST) */
#if defined(LOWER_COMPLEX)
  define_numeric_valued_macro(LOWER_COMPLEX);
#else /* !defined(LOWER_COMPLEX) */
  comment_undefined_macro_name(LOWER_COMPLEX);
#endif /* defined(LOWER_COMPLEX) */
#if defined(LOWER_DESIGNATED_INITIALIZERS)
  define_numeric_valued_macro(LOWER_DESIGNATED_INITIALIZERS);
#else /* !defined(LOWER_DESIGNATED_INITIALIZERS) */
  comment_undefined_macro_name(LOWER_DESIGNATED_INITIALIZERS);
#endif /* defined(LOWER_DESIGNATED_INITIALIZERS) */
#if defined(LOWER_EXTERN_INLINE)
  define_numeric_valued_macro(LOWER_EXTERN_INLINE);
#else /* !defined(LOWER_EXTERN_INLINE) */
  comment_undefined_macro_name(LOWER_EXTERN_INLINE);
#endif /* defined(LOWER_EXTERN_INLINE) */
#if defined(LOWER_FIXED_POINT)
  define_numeric_valued_macro(LOWER_FIXED_POINT);
#else /* !defined(LOWER_FIXED_POINT) */
  comment_undefined_macro_name(LOWER_FIXED_POINT);
#endif /* defined(LOWER_FIXED_POINT) */
#if defined(LOWER_IFUNC)
  define_numeric_valued_macro(LOWER_IFUNC);
#else /* !defined(LOWER_IFUNC) */
  comment_undefined_macro_name(LOWER_IFUNC);
#endif /* defined(LOWER_IFUNC) */
#if defined(LOWER_LVALUE_RETURNING_OPERATIONS)
  define_numeric_valued_macro(LOWER_LVALUE_RETURNING_OPERATIONS);
#else /* !defined(LOWER_LVALUE_RETURNING_OPERATIONS) */
  comment_undefined_macro_name(LOWER_LVALUE_RETURNING_OPERATIONS);
#endif /* defined(LOWER_LVALUE_RETURNING_OPERATIONS) */
#if defined(LOWER_MICROSOFT_NONCONSTANT_AGGREGATE)
  define_numeric_valued_macro(LOWER_MICROSOFT_NONCONSTANT_AGGREGATE);
#else /* !defined(LOWER_MICROSOFT_NONCONSTANT_AGGREGATE) */
  comment_undefined_macro_name(LOWER_MICROSOFT_NONCONSTANT_AGGREGATE);
#endif /* defined(LOWER_MICROSOFT_NONCONSTANT_AGGREGATE) */
#if defined(LOWER_STRING_LITERALS_TO_NON_CONST)
  define_numeric_valued_macro(LOWER_STRING_LITERALS_TO_NON_CONST);
#else /* !defined(LOWER_STRING_LITERALS_TO_NON_CONST) */
  comment_undefined_macro_name(LOWER_STRING_LITERALS_TO_NON_CONST);
#endif /* defined(LOWER_STRING_LITERALS_TO_NON_CONST) */
#if defined(LOWER_VARIABLE_LENGTH_ARRAYS)
  define_numeric_valued_macro(LOWER_VARIABLE_LENGTH_ARRAYS);
#else /* !defined(LOWER_VARIABLE_LENGTH_ARRAYS) */
  comment_undefined_macro_name(LOWER_VARIABLE_LENGTH_ARRAYS);
#endif /* defined(LOWER_VARIABLE_LENGTH_ARRAYS) */
#if defined(MACRO_DEFINED_WHEN_ARRAY_NEW_AND_DELETE_ENABLED)
  define_string_valued_macro(MACRO_DEFINED_WHEN_ARRAY_NEW_AND_DELETE_ENABLED);
#else /* !defined(MACRO_DEFINED_WHEN_ARRAY_NEW_AND_DELETE_ENABLED) */
  comment_undefined_macro_name(
                              MACRO_DEFINED_WHEN_ARRAY_NEW_AND_DELETE_ENABLED);
#endif /* defined(MACRO_DEFINED_WHEN_ARRAY_NEW_AND_DELETE_ENABLED) */
#if defined(MACRO_DEFINED_WHEN_BOOL_IS_KEYWORD)
  define_string_valued_macro(MACRO_DEFINED_WHEN_BOOL_IS_KEYWORD);
#else /* !defined(MACRO_DEFINED_WHEN_BOOL_IS_KEYWORD) */
  comment_undefined_macro_name(MACRO_DEFINED_WHEN_BOOL_IS_KEYWORD);
#endif /* defined(MACRO_DEFINED_WHEN_BOOL_IS_KEYWORD) */
#if defined(MACRO_DEFINED_WHEN_CHAR16_T_AND_CHAR32_T_ARE_KEYWORDS)
  define_string_valued_macro(
                        MACRO_DEFINED_WHEN_CHAR16_T_AND_CHAR32_T_ARE_KEYWORDS);
#else /* !defined(MACRO_DEFINED_WHEN_CHAR16_T_AND_CHAR32_T_ARE_KEYWORDS) */
  comment_undefined_macro_name(
                        MACRO_DEFINED_WHEN_CHAR16_T_AND_CHAR32_T_ARE_KEYWORDS);
#endif /* defined(MACRO_DEFINED_WHEN_CHAR16_T_AND_CHAR32_T_ARE_KEYWORDS) */
#if defined(MACRO_DEFINED_WHEN_EXCEPTIONS_ENABLED)
  define_string_valued_macro(MACRO_DEFINED_WHEN_EXCEPTIONS_ENABLED);
#else /* !defined(MACRO_DEFINED_WHEN_EXCEPTIONS_ENABLED) */
  comment_undefined_macro_name(MACRO_DEFINED_WHEN_EXCEPTIONS_ENABLED);
#endif /* defined(MACRO_DEFINED_WHEN_EXCEPTIONS_ENABLED) */
#if defined(MACRO_DEFINED_WHEN_IA64_ABI)
  define_string_valued_macro(MACRO_DEFINED_WHEN_IA64_ABI);
#else /* !defined(MACRO_DEFINED_WHEN_IA64_ABI) */
  comment_undefined_macro_name(MACRO_DEFINED_WHEN_IA64_ABI);
#endif /* defined(MACRO_DEFINED_WHEN_IA64_ABI) */
#if defined(MACRO_DEFINED_WHEN_IA64_CTORS_DTORS_RETURN_THIS)
  define_string_valued_macro(MACRO_DEFINED_WHEN_IA64_CTORS_DTORS_RETURN_THIS);
#else /* !defined(MACRO_DEFINED_WHEN_IA64_CTORS_DTORS_RETURN_THIS) */
  comment_undefined_macro_name(
                              MACRO_DEFINED_WHEN_IA64_CTORS_DTORS_RETURN_THIS);
#endif /* defined(MACRO_DEFINED_WHEN_IA64_CTORS_DTORS_RETURN_THIS) */
#if defined(MACRO_DEFINED_WHEN_IA64_USE_INT_STATIC_INIT_GUARD)
  define_string_valued_macro(
                            MACRO_DEFINED_WHEN_IA64_USE_INT_STATIC_INIT_GUARD);
#else /* !defined(MACRO_DEFINED_WHEN_IA64_USE_INT_STATIC_INIT_GUARD) */
  comment_undefined_macro_name(
                            MACRO_DEFINED_WHEN_IA64_USE_INT_STATIC_INIT_GUARD);
#endif /* defined(MACRO_DEFINED_WHEN_IA64_USE_INT_STATIC_INIT_GUARD) */
#if defined(MACRO_DEFINED_WHEN_IMPLICITLY_USING_STD)
  define_string_valued_macro(MACRO_DEFINED_WHEN_IMPLICITLY_USING_STD);
#else /* !defined(MACRO_DEFINED_WHEN_IMPLICITLY_USING_STD) */
  comment_undefined_macro_name(MACRO_DEFINED_WHEN_IMPLICITLY_USING_STD);
#endif /* defined(MACRO_DEFINED_WHEN_IMPLICITLY_USING_STD) */
#if defined(MACRO_DEFINED_WHEN_LONG_LONG_IS_DISABLED)
  define_string_valued_macro(MACRO_DEFINED_WHEN_LONG_LONG_IS_DISABLED);
#else /* !defined(MACRO_DEFINED_WHEN_LONG_LONG_IS_DISABLED) */
  comment_undefined_macro_name(MACRO_DEFINED_WHEN_LONG_LONG_IS_DISABLED);
#endif /* defined(MACRO_DEFINED_WHEN_LONG_LONG_IS_DISABLED) */
#if defined(MACRO_DEFINED_WHEN_PLACEMENT_DELETE_ENABLED)
  define_string_valued_macro(MACRO_DEFINED_WHEN_PLACEMENT_DELETE_ENABLED);
#else /* !defined(MACRO_DEFINED_WHEN_PLACEMENT_DELETE_ENABLED) */
  comment_undefined_macro_name(MACRO_DEFINED_WHEN_PLACEMENT_DELETE_ENABLED);
#endif /* defined(MACRO_DEFINED_WHEN_PLACEMENT_DELETE_ENABLED) */
#if defined(MACRO_DEFINED_WHEN_RTTI_ENABLED)
  define_string_valued_macro(MACRO_DEFINED_WHEN_RTTI_ENABLED);
#else /* !defined(MACRO_DEFINED_WHEN_RTTI_ENABLED) */
  comment_undefined_macro_name(MACRO_DEFINED_WHEN_RTTI_ENABLED);
#endif /* defined(MACRO_DEFINED_WHEN_RTTI_ENABLED) */
#if defined(MACRO_DEFINED_WHEN_RUNTIME_USES_NAMESPACES)
  define_string_valued_macro(MACRO_DEFINED_WHEN_RUNTIME_USES_NAMESPACES);
#else /* !defined(MACRO_DEFINED_WHEN_RUNTIME_USES_NAMESPACES) */
  comment_undefined_macro_name(MACRO_DEFINED_WHEN_RUNTIME_USES_NAMESPACES);
#endif /* defined(MACRO_DEFINED_WHEN_RUNTIME_USES_NAMESPACES) */
#if defined(MACRO_DEFINED_WHEN_TYPE_TRAITS_HELPERS_ENABLED)
  define_string_valued_macro(MACRO_DEFINED_WHEN_TYPE_TRAITS_HELPERS_ENABLED);
#else /* !defined(MACRO_DEFINED_WHEN_TYPE_TRAITS_HELPERS_ENABLED) */
  comment_undefined_macro_name(MACRO_DEFINED_WHEN_TYPE_TRAITS_HELPERS_ENABLED);
#endif /* defined(MACRO_DEFINED_WHEN_TYPE_TRAITS_HELPERS_ENABLED) */
#if defined(MACRO_DEFINED_WHEN_VARIADIC_TEMPLATES_ENABLED)
  define_string_valued_macro(MACRO_DEFINED_WHEN_VARIADIC_TEMPLATES_ENABLED);
#else /* !defined(MACRO_DEFINED_WHEN_VARIADIC_TEMPLATES_ENABLED) */
  comment_undefined_macro_name(MACRO_DEFINED_WHEN_VARIADIC_TEMPLATES_ENABLED);
#endif /* defined(MACRO_DEFINED_WHEN_VARIADIC_TEMPLATES_ENABLED) */
#if defined(MACRO_DEFINED_WHEN_WCHAR_T_IS_KEYWORD)
  define_string_valued_macro(MACRO_DEFINED_WHEN_WCHAR_T_IS_KEYWORD);
#else /* !defined(MACRO_DEFINED_WHEN_WCHAR_T_IS_KEYWORD) */
  comment_undefined_macro_name(MACRO_DEFINED_WHEN_WCHAR_T_IS_KEYWORD);
#endif /* defined(MACRO_DEFINED_WHEN_WCHAR_T_IS_KEYWORD) */
#if defined(MACRO_INVOCATION_TREE_IN_IL)
  define_numeric_valued_macro(MACRO_INVOCATION_TREE_IN_IL);
#else /* !defined(MACRO_INVOCATION_TREE_IN_IL) */
  comment_undefined_macro_name(MACRO_INVOCATION_TREE_IN_IL);
#endif /* defined(MACRO_INVOCATION_TREE_IN_IL) */
#if defined(MAINTAIN_ALLOCATION_SEQUENCE_NUMBER)
  define_numeric_valued_macro(MAINTAIN_ALLOCATION_SEQUENCE_NUMBER);
#else /* !defined(MAINTAIN_ALLOCATION_SEQUENCE_NUMBER) */
  comment_undefined_macro_name(MAINTAIN_ALLOCATION_SEQUENCE_NUMBER);
#endif /* defined(MAINTAIN_ALLOCATION_SEQUENCE_NUMBER) */
#if defined(MAINTAIN_CLASS_MEMBER_LIST)
  define_numeric_valued_macro(MAINTAIN_CLASS_MEMBER_LIST);
#else /* !defined(MAINTAIN_CLASS_MEMBER_LIST) */
  comment_undefined_macro_name(MAINTAIN_CLASS_MEMBER_LIST);
#endif /* defined(MAINTAIN_CLASS_MEMBER_LIST) */
#if defined(MAINTAIN_NEEDED_FLAGS)
  define_numeric_valued_macro(MAINTAIN_NEEDED_FLAGS);
#else /* !defined(MAINTAIN_NEEDED_FLAGS) */
  comment_undefined_macro_name(MAINTAIN_NEEDED_FLAGS);
#endif /* defined(MAINTAIN_NEEDED_FLAGS) */
#if defined(MAKE_ALL_FUNCTIONS_UNPROTOTYPED)
  define_numeric_valued_macro(MAKE_ALL_FUNCTIONS_UNPROTOTYPED);
#else /* !defined(MAKE_ALL_FUNCTIONS_UNPROTOTYPED) */
  comment_undefined_macro_name(MAKE_ALL_FUNCTIONS_UNPROTOTYPED);
#endif /* defined(MAKE_ALL_FUNCTIONS_UNPROTOTYPED) */
#if defined(MAKE_FRONT_END_CALLABLE)
  define_numeric_valued_macro(MAKE_FRONT_END_CALLABLE);
#else /* !defined(MAKE_FRONT_END_CALLABLE) */
  comment_undefined_macro_name(MAKE_FRONT_END_CALLABLE);
#endif /* defined(MAKE_FRONT_END_CALLABLE) */
#if defined(MANGLE_ALL_NAMES)
  define_numeric_valued_macro(MANGLE_ALL_NAMES);
#else /* !defined(MANGLE_ALL_NAMES) */
  comment_undefined_macro_name(MANGLE_ALL_NAMES);
#endif /* defined(MANGLE_ALL_NAMES) */
#if defined(MAX_CHAR16_T_ENCODING_LENGTH)
  define_numeric_valued_macro(MAX_CHAR16_T_ENCODING_LENGTH);
#else /* !defined(MAX_CHAR16_T_ENCODING_LENGTH) */
  comment_undefined_macro_name(MAX_CHAR16_T_ENCODING_LENGTH);
#endif /* defined(MAX_CHAR16_T_ENCODING_LENGTH) */
#if defined(MAX_ERROR_OUTPUT_LINE_LENGTH)
  define_numeric_valued_macro(MAX_ERROR_OUTPUT_LINE_LENGTH);
#else /* !defined(MAX_ERROR_OUTPUT_LINE_LENGTH) */
  comment_undefined_macro_name(MAX_ERROR_OUTPUT_LINE_LENGTH);
#endif /* defined(MAX_ERROR_OUTPUT_LINE_LENGTH) */
#if defined(MAX_ERROR_TEMPLATE_ARG_DEPTH)
  define_numeric_valued_macro(MAX_ERROR_TEMPLATE_ARG_DEPTH);
#else /* !defined(MAX_ERROR_TEMPLATE_ARG_DEPTH) */
  comment_undefined_macro_name(MAX_ERROR_TEMPLATE_ARG_DEPTH);
#endif /* defined(MAX_ERROR_TEMPLATE_ARG_DEPTH) */
#if defined(MAX_INCLUDE_FILES_OPEN_AT_ONCE)
  define_numeric_valued_macro(MAX_INCLUDE_FILES_OPEN_AT_ONCE);
#else /* !defined(MAX_INCLUDE_FILES_OPEN_AT_ONCE) */
  comment_undefined_macro_name(MAX_INCLUDE_FILES_OPEN_AT_ONCE);
#endif /* defined(MAX_INCLUDE_FILES_OPEN_AT_ONCE) */
#if defined(MAX_MULTIBYTE_CHAR_LENGTH)
  define_numeric_valued_macro(MAX_MULTIBYTE_CHAR_LENGTH);
#else /* !defined(MAX_MULTIBYTE_CHAR_LENGTH) */
  comment_undefined_macro_name(MAX_MULTIBYTE_CHAR_LENGTH);
#endif /* defined(MAX_MULTIBYTE_CHAR_LENGTH) */
#if defined(MAX_SIZEOF_LARGEST_FIXED_POINT)
  define_numeric_valued_macro(MAX_SIZEOF_LARGEST_FIXED_POINT);
#else /* !defined(MAX_SIZEOF_LARGEST_FIXED_POINT) */
  comment_undefined_macro_name(MAX_SIZEOF_LARGEST_FIXED_POINT);
#endif /* defined(MAX_SIZEOF_LARGEST_FIXED_POINT) */
#if defined(MAX_SIZEOF_LARGEST_INTEGER)
  define_numeric_valued_macro(MAX_SIZEOF_LARGEST_INTEGER);
#else /* !defined(MAX_SIZEOF_LARGEST_INTEGER) */
  comment_undefined_macro_name(MAX_SIZEOF_LARGEST_INTEGER);
#endif /* defined(MAX_SIZEOF_LARGEST_INTEGER) */
#if defined(MAX_TOTAL_PENDING_INSTANTIATIONS)
  define_numeric_valued_macro(MAX_TOTAL_PENDING_INSTANTIATIONS);
#else /* !defined(MAX_TOTAL_PENDING_INSTANTIATIONS) */
  comment_undefined_macro_name(MAX_TOTAL_PENDING_INSTANTIATIONS);
#endif /* defined(MAX_TOTAL_PENDING_INSTANTIATIONS) */
#if defined(MAX_UNUSED_ALL_MODE_INSTANTIATIONS)
  define_numeric_valued_macro(MAX_UNUSED_ALL_MODE_INSTANTIATIONS);
#else /* !defined(MAX_UNUSED_ALL_MODE_INSTANTIATIONS) */
  comment_undefined_macro_name(MAX_UNUSED_ALL_MODE_INSTANTIATIONS);
#endif /* defined(MAX_UNUSED_ALL_MODE_INSTANTIATIONS) */
#if defined(METADATA_IMPORT_BUFFER_ALLOCATION_INCREMENT)
  define_numeric_valued_macro(METADATA_IMPORT_BUFFER_ALLOCATION_INCREMENT);
#else /* !defined(METADATA_IMPORT_BUFFER_ALLOCATION_INCREMENT) */
  comment_undefined_macro_name(METADATA_IMPORT_BUFFER_ALLOCATION_INCREMENT);
#endif /* defined(METADATA_IMPORT_BUFFER_ALLOCATION_INCREMENT) */
#if defined(METADATA_IMPORT_BUFFER_SIZE)
  define_numeric_valued_macro(METADATA_IMPORT_BUFFER_SIZE);
#else /* !defined(METADATA_IMPORT_BUFFER_SIZE) */
  comment_undefined_macro_name(METADATA_IMPORT_BUFFER_SIZE);
#endif /* defined(METADATA_IMPORT_BUFFER_SIZE) */
#if defined(MICROSOFT_DIALECT_IS_GENERATED_CODE_TARGET)
  define_numeric_valued_macro(MICROSOFT_DIALECT_IS_GENERATED_CODE_TARGET);
#else /* !defined(MICROSOFT_DIALECT_IS_GENERATED_CODE_TARGET) */
  comment_undefined_macro_name(MICROSOFT_DIALECT_IS_GENERATED_CODE_TARGET);
#endif /* defined(MICROSOFT_DIALECT_IS_GENERATED_CODE_TARGET) */
#if defined(MICROSOFT_EXTENSIONS_ALLOWED)
  define_numeric_valued_macro(MICROSOFT_EXTENSIONS_ALLOWED);
#else /* !defined(MICROSOFT_EXTENSIONS_ALLOWED) */
  comment_undefined_macro_name(MICROSOFT_EXTENSIONS_ALLOWED);
#endif /* defined(MICROSOFT_EXTENSIONS_ALLOWED) */
#if defined(MICROSOFT_MODE_TYPE_INFO_IN_NAMESPACE_STD)
  define_numeric_valued_macro(MICROSOFT_MODE_TYPE_INFO_IN_NAMESPACE_STD);
#else /* !defined(MICROSOFT_MODE_TYPE_INFO_IN_NAMESPACE_STD) */
  comment_undefined_macro_name(MICROSOFT_MODE_TYPE_INFO_IN_NAMESPACE_STD);
#endif /* defined(MICROSOFT_MODE_TYPE_INFO_IN_NAMESPACE_STD) */
#if defined(MINIMAL_INLINING)
  define_numeric_valued_macro(MINIMAL_INLINING);
#else /* !defined(MINIMAL_INLINING) */
  comment_undefined_macro_name(MINIMAL_INLINING);
#endif /* defined(MINIMAL_INLINING) */
#if defined(MIN_GNU_VERSION)
  define_numeric_valued_macro(MIN_GNU_VERSION);
#else /* !defined(MIN_GNU_VERSION) */
  comment_undefined_macro_name(MIN_GNU_VERSION);
#endif /* defined(MIN_GNU_VERSION) */
#if defined(MODULE_MAX_LINE_NUMBER)
  define_numeric_valued_macro(MODULE_MAX_LINE_NUMBER);
#else /* !defined(MODULE_MAX_LINE_NUMBER) */
  comment_undefined_macro_name(MODULE_MAX_LINE_NUMBER);
#endif /* defined(MODULE_MAX_LINE_NUMBER) */
#if defined(MSVC_IS_GENERATED_CODE_TARGET)
  define_numeric_valued_macro(MSVC_IS_GENERATED_CODE_TARGET);
#else /* !defined(MSVC_IS_GENERATED_CODE_TARGET) */
  comment_undefined_macro_name(MSVC_IS_GENERATED_CODE_TARGET);
#endif /* defined(MSVC_IS_GENERATED_CODE_TARGET) */
#if defined(MSVC_TARGET_VERSION_NUMBER)
  define_numeric_valued_macro(MSVC_TARGET_VERSION_NUMBER);
#else /* !defined(MSVC_TARGET_VERSION_NUMBER) */
  comment_undefined_macro_name(MSVC_TARGET_VERSION_NUMBER);
#endif /* defined(MSVC_TARGET_VERSION_NUMBER) */
#if defined(MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED)
  define_numeric_valued_macro(MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED);
#else /* !defined(MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED) */
  comment_undefined_macro_name(MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED);
#endif /* defined(MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED) */
#if defined(NAMED_ADDRESS_SPACES_ALLOWED)
  define_numeric_valued_macro(NAMED_ADDRESS_SPACES_ALLOWED);
#else /* !defined(NAMED_ADDRESS_SPACES_ALLOWED) */
  comment_undefined_macro_name(NAMED_ADDRESS_SPACES_ALLOWED);
#endif /* defined(NAMED_ADDRESS_SPACES_ALLOWED) */
#if defined(NAMED_REGISTERS_ALLOWED)
  define_numeric_valued_macro(NAMED_REGISTERS_ALLOWED);
#else /* !defined(NAMED_REGISTERS_ALLOWED) */
  comment_undefined_macro_name(NAMED_REGISTERS_ALLOWED);
#endif /* defined(NAMED_REGISTERS_ALLOWED) */
#if defined(NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE)
  define_numeric_valued_macro(NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE);
#else /* !defined(NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE) */
  comment_undefined_macro_name(NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE);
#endif /* defined(NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE) */
#if defined(NEAR_AND_FAR_ALLOWED)
  define_numeric_valued_macro(NEAR_AND_FAR_ALLOWED);
#else /* !defined(NEAR_AND_FAR_ALLOWED) */
  comment_undefined_macro_name(NEAR_AND_FAR_ALLOWED);
#endif /* defined(NEAR_AND_FAR_ALLOWED) */
#if defined(NEED_DECLARATIVE_WALK)
  define_numeric_valued_macro(NEED_DECLARATIVE_WALK);
#else /* !defined(NEED_DECLARATIVE_WALK) */
  comment_undefined_macro_name(NEED_DECLARATIVE_WALK);
#endif /* defined(NEED_DECLARATIVE_WALK) */
#if defined(NEED_IL_DISPLAY)
  define_numeric_valued_macro(NEED_IL_DISPLAY);
#else /* !defined(NEED_IL_DISPLAY) */
  comment_undefined_macro_name(NEED_IL_DISPLAY);
#endif /* defined(NEED_IL_DISPLAY) */
#if defined(NEED_NAME_MANGLING)
  define_numeric_valued_macro(NEED_NAME_MANGLING);
#else /* !defined(NEED_NAME_MANGLING) */
  comment_undefined_macro_name(NEED_NAME_MANGLING);
#endif /* defined(NEED_NAME_MANGLING) */
#if defined(NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE)
  define_numeric_valued_macro(
                  NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE);
#else /* !defined(
                NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE) */
  comment_undefined_macro_name(
                  NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE);
#endif /* defined(
                NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE) */
#if defined(NEW_CAN_BE_FOLDED_INTO_CTOR)
  define_numeric_valued_macro(NEW_CAN_BE_FOLDED_INTO_CTOR);
#else /* !defined(NEW_CAN_BE_FOLDED_INTO_CTOR) */
  comment_undefined_macro_name(NEW_CAN_BE_FOLDED_INTO_CTOR);
#endif /* defined(NEW_CAN_BE_FOLDED_INTO_CTOR) */
#if defined(NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS)
  define_numeric_valued_macro(
                    NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS);
#else /* !defined(NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS) */
  comment_undefined_macro_name(
                    NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS);
#endif /* defined(NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS) */
#if defined(NO_USR_INCLUDE)
  define_numeric_valued_macro(NO_USR_INCLUDE);
#else /* !defined(NO_USR_INCLUDE) */
  comment_undefined_macro_name(NO_USR_INCLUDE);
#endif /* defined(NO_USR_INCLUDE) */
#if defined(NO_VLA_DIMENSION_TEMPORARIES_IN_FUNCTION_PROTOTYPES)
  define_numeric_valued_macro(
                          NO_VLA_DIMENSION_TEMPORARIES_IN_FUNCTION_PROTOTYPES);
#else /* !defined(NO_VLA_DIMENSION_TEMPORARIES_IN_FUNCTION_PROTOTYPES) */
  comment_undefined_macro_name(
                          NO_VLA_DIMENSION_TEMPORARIES_IN_FUNCTION_PROTOTYPES);
#endif /* defined(NO_VLA_DIMENSION_TEMPORARIES_IN_FUNCTION_PROTOTYPES) */
#if defined(NULL_POINTER_IS_ZERO)
  define_numeric_valued_macro(NULL_POINTER_IS_ZERO);
#else /* !defined(NULL_POINTER_IS_ZERO) */
  comment_undefined_macro_name(NULL_POINTER_IS_ZERO);
#endif /* defined(NULL_POINTER_IS_ZERO) */
#if defined(NUM_BITS_FOR_CHARACTER_KIND)
  define_numeric_valued_macro(NUM_BITS_FOR_CHARACTER_KIND);
#else /* !defined(NUM_BITS_FOR_CHARACTER_KIND) */
  comment_undefined_macro_name(NUM_BITS_FOR_CHARACTER_KIND);
#endif /* defined(NUM_BITS_FOR_CHARACTER_KIND) */
#if defined(NUM_BITS_FOR_EXPR_NODE_KIND)
  define_numeric_valued_macro(NUM_BITS_FOR_EXPR_NODE_KIND);
#else /* !defined(NUM_BITS_FOR_EXPR_NODE_KIND) */
  comment_undefined_macro_name(NUM_BITS_FOR_EXPR_NODE_KIND);
#endif /* defined(NUM_BITS_FOR_EXPR_NODE_KIND) */
#if defined(NUM_BITS_FOR_NAMED_ADDRESS_SPACE)
  define_numeric_valued_macro(NUM_BITS_FOR_NAMED_ADDRESS_SPACE);
#else /* !defined(NUM_BITS_FOR_NAMED_ADDRESS_SPACE) */
  comment_undefined_macro_name(NUM_BITS_FOR_NAMED_ADDRESS_SPACE);
#endif /* defined(NUM_BITS_FOR_NAMED_ADDRESS_SPACE) */
#if defined(NUM_BITS_FOR_NAME_LINKAGE)
  define_numeric_valued_macro(NUM_BITS_FOR_NAME_LINKAGE);
#else /* !defined(NUM_BITS_FOR_NAME_LINKAGE) */
  comment_undefined_macro_name(NUM_BITS_FOR_NAME_LINKAGE);
#endif /* defined(NUM_BITS_FOR_NAME_LINKAGE) */
#if defined(NUM_BITS_FOR_STDC_PRAGMA_VALUE)
  define_numeric_valued_macro(NUM_BITS_FOR_STDC_PRAGMA_VALUE);
#else /* !defined(NUM_BITS_FOR_STDC_PRAGMA_VALUE) */
  comment_undefined_macro_name(NUM_BITS_FOR_STDC_PRAGMA_VALUE);
#endif /* defined(NUM_BITS_FOR_STDC_PRAGMA_VALUE) */
#if defined(NUM_NAMED_REGISTERS)
  define_numeric_valued_macro(NUM_NAMED_REGISTERS);
#else /* !defined(NUM_NAMED_REGISTERS) */
  comment_undefined_macro_name(NUM_NAMED_REGISTERS);
#endif /* defined(NUM_NAMED_REGISTERS) */
#if defined(OBJECT_FILE_SUFFIX)
  define_string_valued_macro(OBJECT_FILE_SUFFIX);
#else /* !defined(OBJECT_FILE_SUFFIX) */
  comment_undefined_macro_name(OBJECT_FILE_SUFFIX);
#endif /* defined(OBJECT_FILE_SUFFIX) */
#if defined(OLD_STYLE_PREPROCESSING_IN_CFRONT_MODE)
  define_numeric_valued_macro(OLD_STYLE_PREPROCESSING_IN_CFRONT_MODE);
#else /* !defined(OLD_STYLE_PREPROCESSING_IN_CFRONT_MODE) */
  comment_undefined_macro_name(OLD_STYLE_PREPROCESSING_IN_CFRONT_MODE);
#endif /* defined(OLD_STYLE_PREPROCESSING_IN_CFRONT_MODE) */
#if defined(ONE_INSTANTIATION_PER_OBJECT)
  define_numeric_valued_macro(ONE_INSTANTIATION_PER_OBJECT);
#else /* !defined(ONE_INSTANTIATION_PER_OBJECT) */
  comment_undefined_macro_name(ONE_INSTANTIATION_PER_OBJECT);
#endif /* defined(ONE_INSTANTIATION_PER_OBJECT) */
#if defined(ONLY_MANGLE_TYPES_NEEDED_FOR_EXTERNAL_NAMES)
  define_numeric_valued_macro(ONLY_MANGLE_TYPES_NEEDED_FOR_EXTERNAL_NAMES);
#else /* !defined(ONLY_MANGLE_TYPES_NEEDED_FOR_EXTERNAL_NAMES) */
  comment_undefined_macro_name(ONLY_MANGLE_TYPES_NEEDED_FOR_EXTERNAL_NAMES);
#endif /* defined(ONLY_MANGLE_TYPES_NEEDED_FOR_EXTERNAL_NAMES) */
#if defined(OPTIMIZE_VIRTUAL_FUNCTION_CALLS)
  define_numeric_valued_macro(OPTIMIZE_VIRTUAL_FUNCTION_CALLS);
#else /* !defined(OPTIMIZE_VIRTUAL_FUNCTION_CALLS) */
  comment_undefined_macro_name(OPTIMIZE_VIRTUAL_FUNCTION_CALLS);
#endif /* defined(OPTIMIZE_VIRTUAL_FUNCTION_CALLS) */
#if defined(OVERWRITE_FREED_MEM_BLOCKS)
  define_numeric_valued_macro(OVERWRITE_FREED_MEM_BLOCKS);
#else /* !defined(OVERWRITE_FREED_MEM_BLOCKS) */
  comment_undefined_macro_name(OVERWRITE_FREED_MEM_BLOCKS);
#endif /* defined(OVERWRITE_FREED_MEM_BLOCKS) */
#if defined(PARENS_IN_IL)
  define_numeric_valued_macro(PARENS_IN_IL);
#else /* !defined(PARENS_IN_IL) */
  comment_undefined_macro_name(PARENS_IN_IL);
#endif /* defined(PARENS_IN_IL) */
#if defined(PASS_ELEM_COUNT_TO_RUNTIME_AS_PTRDIFF_T)
  define_numeric_valued_macro(PASS_ELEM_COUNT_TO_RUNTIME_AS_PTRDIFF_T);
#else /* !defined(PASS_ELEM_COUNT_TO_RUNTIME_AS_PTRDIFF_T) */
  comment_undefined_macro_name(PASS_ELEM_COUNT_TO_RUNTIME_AS_PTRDIFF_T);
#endif /* defined(PASS_ELEM_COUNT_TO_RUNTIME_AS_PTRDIFF_T) */
#if defined(PCH_DECL_SEQ_THRESHOLD)
  define_numeric_valued_macro(PCH_DECL_SEQ_THRESHOLD);
#else /* !defined(PCH_DECL_SEQ_THRESHOLD) */
  comment_undefined_macro_name(PCH_DECL_SEQ_THRESHOLD);
#endif /* defined(PCH_DECL_SEQ_THRESHOLD) */
#if defined(PCH_FILE_SUFFIX)
  define_string_valued_macro(PCH_FILE_SUFFIX);
#else /* !defined(PCH_FILE_SUFFIX) */
  comment_undefined_macro_name(PCH_FILE_SUFFIX);
#endif /* defined(PCH_FILE_SUFFIX) */
#if defined(PRAGMA_DEFINE_TYPE_INFO_IS_REQUIRED)
  define_numeric_valued_macro(PRAGMA_DEFINE_TYPE_INFO_IS_REQUIRED);
#else /* !defined(PRAGMA_DEFINE_TYPE_INFO_IS_REQUIRED) */
  comment_undefined_macro_name(PRAGMA_DEFINE_TYPE_INFO_IS_REQUIRED);
#endif /* defined(PRAGMA_DEFINE_TYPE_INFO_IS_REQUIRED) */
#if defined(PRAGMA_WEAK_ALLOWED)
  define_numeric_valued_macro(PRAGMA_WEAK_ALLOWED);
#else /* !defined(PRAGMA_WEAK_ALLOWED) */
  comment_undefined_macro_name(PRAGMA_WEAK_ALLOWED);
#endif /* defined(PRAGMA_WEAK_ALLOWED) */
#if defined(PREDEFINED_MACRO_FILE_NAME)
  define_string_valued_macro(PREDEFINED_MACRO_FILE_NAME);
#else /* !defined(PREDEFINED_MACRO_FILE_NAME) */
  comment_undefined_macro_name(PREDEFINED_MACRO_FILE_NAME);
#endif /* defined(PREDEFINED_MACRO_FILE_NAME) */
#if defined(PRESERVE_EMBED_DIRECTIVE_WHEN_OPTIMIZED)
  define_numeric_valued_macro(PRESERVE_EMBED_DIRECTIVE_WHEN_OPTIMIZED);
#else /* !defined(PRESERVE_EMBED_DIRECTIVE_WHEN_OPTIMIZED) */
  comment_undefined_macro_name(PRESERVE_EMBED_DIRECTIVE_WHEN_OPTIMIZED);
#endif /* defined(PRESERVE_EMBBED_DIRECTIVE_WHEN_OPTIMIZED) */
#if defined(PRESERVE_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING)
  define_numeric_valued_macro(PRESERVE_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING);
#else /* !defined(PRESERVE_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING) */
  comment_undefined_macro_name(
                              PRESERVE_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING);
#endif /* defined(PRESERVE_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING) */
#if defined(PRESERVE_TOP_LEVEL_CASTS_TO_VOID_IN_IL)
  define_numeric_valued_macro(PRESERVE_TOP_LEVEL_CASTS_TO_VOID_IN_IL);
#else /* !defined(PRESERVE_TOP_LEVEL_CASTS_TO_VOID_IN_IL) */
  comment_undefined_macro_name(PRESERVE_TOP_LEVEL_CASTS_TO_VOID_IN_IL);
#endif /* defined(PRESERVE_TOP_LEVEL_CASTS_TO_VOID_IN_IL) */
#if defined(PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE)
  define_numeric_valued_macro(PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE);
#else /* !defined(PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE) */
  comment_undefined_macro_name(PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE);
#endif /* defined(PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE) */
#if defined(PROTOTYPED_INT_ARGS_PASSED_LIKE_UNPROTOTYPED)
  define_numeric_valued_macro(PROTOTYPED_INT_ARGS_PASSED_LIKE_UNPROTOTYPED);
#else /* !defined(PROTOTYPED_INT_ARGS_PASSED_LIKE_UNPROTOTYPED) */
  comment_undefined_macro_name(PROTOTYPED_INT_ARGS_PASSED_LIKE_UNPROTOTYPED);
#endif /* defined(PROTOTYPED_INT_ARGS_PASSED_LIKE_UNPROTOTYPED) */
#if defined(PROTOTYPE_INSTANTIATIONS_IN_IL)
  define_numeric_valued_macro(PROTOTYPE_INSTANTIATIONS_IN_IL);
#else /* !defined(PROTOTYPE_INSTANTIATIONS_IN_IL) */
  comment_undefined_macro_name(PROTOTYPE_INSTANTIATIONS_IN_IL);
#endif /* defined(PROTOTYPE_INSTANTIATIONS_IN_IL) */
#if defined(PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED)
  define_numeric_valued_macro(PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED);
#else /* !defined(PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED) */
  comment_undefined_macro_name(PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED);
#endif /* defined(PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED) */
#if defined(PTR_TO_MEMBER_REPR_SUPPORTS_CAST_FROM_VIRTUAL_BASE)
  define_numeric_valued_macro(
                           PTR_TO_MEMBER_REPR_SUPPORTS_CAST_FROM_VIRTUAL_BASE);
#else /* !defined(PTR_TO_MEMBER_REPR_SUPPORTS_CAST_FROM_VIRTUAL_BASE) */
  comment_undefined_macro_name(
                           PTR_TO_MEMBER_REPR_SUPPORTS_CAST_FROM_VIRTUAL_BASE);
#endif /* defined(PTR_TO_MEMBER_REPR_SUPPORTS_CAST_FROM_VIRTUAL_BASE) */
#if defined(QUESTION_MARK_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR)
  define_numeric_valued_macro(
                            QUESTION_MARK_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR);
#else /* !defined(QUESTION_MARK_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR) */
  comment_undefined_macro_name(
                            QUESTION_MARK_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR);
#endif /* defined(QUESTION_MARK_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR) */
#if defined(READ_CPPCLI_PORTABLE_ASSEMBLIES)
  define_numeric_valued_macro(READ_CPPCLI_PORTABLE_ASSEMBLIES);
#else /* !defined(READ_CPPCLI_PORTABLE_ASSEMBLIES) */
  comment_undefined_macro_name(READ_CPPCLI_PORTABLE_ASSEMBLIES);
#endif /* defined(READ_CPPCLI_PORTABLE_ASSEMBLIES) */
#if defined(READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS)
  define_numeric_valued_macro(READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS);
#else /* !defined(READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS) */
  comment_undefined_macro_name(READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS);
#endif /* defined(READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS) */
#if defined(RECOGNIZE_MICROSOFT_ATTRIBUTES)
  define_numeric_valued_macro(RECOGNIZE_MICROSOFT_ATTRIBUTES);
#else /* !defined(RECOGNIZE_MICROSOFT_ATTRIBUTES) */
  comment_undefined_macro_name(RECOGNIZE_MICROSOFT_ATTRIBUTES);
#endif /* defined(RECOGNIZE_MICROSOFT_ATTRIBUTES) */
#if defined(RECORD_BACKING_EXPRS_WITH_IL_LOWERING)
  define_numeric_valued_macro(RECORD_BACKING_EXPRS_WITH_IL_LOWERING);
#else /* !defined(RECORD_BACKING_EXPRS_WITH_IL_LOWERING) */
  comment_undefined_macro_name(RECORD_BACKING_EXPRS_WITH_IL_LOWERING);
#endif /* defined(RECORD_BACKING_EXPRS_WITH_IL_LOWERING) */
#if defined(RECORD_BIT_FIELD_CONTAINER_OFFSETS_IN_IL)
  define_numeric_valued_macro(RECORD_BIT_FIELD_CONTAINER_OFFSETS_IN_IL);
#else /* !defined(RECORD_BIT_FIELD_CONTAINER_OFFSETS_IN_IL) */
  comment_undefined_macro_name(RECORD_BIT_FIELD_CONTAINER_OFFSETS_IN_IL);
#endif /* defined(RECORD_BIT_FIELD_CONTAINER_OFFSETS_IN_IL) */
#if defined(RECORD_HIDDEN_NAMES_IN_IL)
  define_numeric_valued_macro(RECORD_HIDDEN_NAMES_IN_IL);
#else /* !defined(RECORD_HIDDEN_NAMES_IN_IL) */
  comment_undefined_macro_name(RECORD_HIDDEN_NAMES_IN_IL);
#endif /* defined(RECORD_HIDDEN_NAMES_IN_IL) */
#if defined(RECORD_MACROS_IN_IL)
  define_numeric_valued_macro(RECORD_MACROS_IN_IL);
#else /* !defined(RECORD_MACROS_IN_IL) */
  comment_undefined_macro_name(RECORD_MACROS_IN_IL);
#endif /* defined(RECORD_MACROS_IN_IL) */
#if defined(RECORD_MACRO_ARGS)
  define_numeric_valued_macro(RECORD_MACRO_ARGS);
#else /* !defined(RECORD_MACRO_ARGS) */
  comment_undefined_macro_name(RECORD_MACRO_ARGS);
#endif /* defined(RECORD_MACRO_ARGS) */
#if defined(RECORD_MACRO_INVOCATIONS)
  define_numeric_valued_macro(RECORD_MACRO_INVOCATIONS);
#else /* !defined(RECORD_MACRO_INVOCATIONS) */
  comment_undefined_macro_name(RECORD_MACRO_INVOCATIONS);
#endif /* defined(RECORD_MACRO_INVOCATIONS) */
#if defined(RECORD_RAW_ASM_OPERAND_DESCRIPTIONS)
  define_numeric_valued_macro(RECORD_RAW_ASM_OPERAND_DESCRIPTIONS);
#else /* !defined(RECORD_RAW_ASM_OPERAND_DESCRIPTIONS) */
  comment_undefined_macro_name(RECORD_RAW_ASM_OPERAND_DESCRIPTIONS);
#endif /* defined(RECORD_RAW_ASM_OPERAND_DESCRIPTIONS) */
#if defined(RECORD_SCOPE_DEPTH_IN_IL)
  define_numeric_valued_macro(RECORD_SCOPE_DEPTH_IN_IL);
#else /* !defined(RECORD_SCOPE_DEPTH_IN_IL) */
  comment_undefined_macro_name(RECORD_SCOPE_DEPTH_IN_IL);
#endif /* defined(RECORD_SCOPE_DEPTH_IN_IL) */
#if defined(RECORD_TEMPLATE_STRINGS)
  define_numeric_valued_macro(RECORD_TEMPLATE_STRINGS);
#else /* !defined(RECORD_TEMPLATE_STRINGS) */
  comment_undefined_macro_name(RECORD_TEMPLATE_STRINGS);
#endif /* defined(RECORD_TEMPLATE_STRINGS) */
#if defined(RECORD_UNRECOGNIZED_ATTRIBUTES)
  define_numeric_valued_macro(RECORD_UNRECOGNIZED_ATTRIBUTES);
#else /* !defined(RECORD_UNRECOGNIZED_ATTRIBUTES) */
  comment_undefined_macro_name(RECORD_UNRECOGNIZED_ATTRIBUTES);
#endif /* defined(RECORD_UNRECOGNIZED_ATTRIBUTES) */
#if defined(REDEFINE_EXTNAME_PRAGMA_ENABLED)
  define_numeric_valued_macro(REDEFINE_EXTNAME_PRAGMA_ENABLED);
#else /* !defined(REDEFINE_EXTNAME_PRAGMA_ENABLED) */
  comment_undefined_macro_name(REDEFINE_EXTNAME_PRAGMA_ENABLED);
#endif /* defined(REDEFINE_EXTNAME_PRAGMA_ENABLED) */
#if defined(REDUCE_BACKING_EXPRESSION_USE)
  define_numeric_valued_macro(REDUCE_BACKING_EXPRESSION_USE);
#else /* !defined(REDUCE_BACKING_EXPRESSION_USE) */
  comment_undefined_macro_name(REDUCE_BACKING_EXPRESSION_USE);
#endif /* defined(REDUCE_BACKING_EXPRESSION_USE) */
#if defined(REFLECTION_ENABLING_POSSIBLE)
  define_numeric_valued_macro(REFLECTION_ENABLING_POSSIBLE);
#else /* !defined(REFLECTION_ENABLING_POSSIBLE) */
  comment_undefined_macro_name(REFLECTION_ENABLING_POSSIBLE);
#endif /* defined(REFLECTION_ENABLING_POSSIBLE) */
#if defined(REF_DESTRUCTORS_FOR_PARAMETER_VARIABLES)
  define_numeric_valued_macro(REF_DESTRUCTORS_FOR_PARAMETER_VARIABLES);
#else /* !defined(REF_DESTRUCTORS_FOR_PARAMETER_VARIABLES) */
  comment_undefined_macro_name(REF_DESTRUCTORS_FOR_PARAMETER_VARIABLES);
#endif /* defined(REF_DESTRUCTORS_FOR_PARAMETER_VARIABLES) */
#if defined(REMOVE_INLINE_BODIES_FROM_CLASS_TEMPLATE_DEFINITIONS)
  define_numeric_valued_macro(
                         REMOVE_INLINE_BODIES_FROM_CLASS_TEMPLATE_DEFINITIONS);
#else /* !defined(REMOVE_INLINE_BODIES_FROM_CLASS_TEMPLATE_DEFINITIONS) */
  comment_undefined_macro_name(
                         REMOVE_INLINE_BODIES_FROM_CLASS_TEMPLATE_DEFINITIONS);
#endif /* defined(REMOVE_INLINE_BODIES_FROM_CLASS_TEMPLATE_DEFINITIONS) */
#if defined(REPLACE_SPECIAL_CHARACTERS_IN_MANGLED_NAMES)
  define_numeric_valued_macro(REPLACE_SPECIAL_CHARACTERS_IN_MANGLED_NAMES);
#else /* !defined(REPLACE_SPECIAL_CHARACTERS_IN_MANGLED_NAMES) */
  comment_undefined_macro_name(REPLACE_SPECIAL_CHARACTERS_IN_MANGLED_NAMES);
#endif /* defined(REPLACE_SPECIAL_CHARACTERS_IN_MANGLED_NAMES) */
#if defined(REPRESENT_C11_GENERIC_CONSTRUCT_IN_IL)
  define_numeric_valued_macro(REPRESENT_C11_GENERIC_CONSTRUCT_IN_IL);
#else /* !defined(REPRESENT_C11_GENERIC_CONSTRUCT_IN_IL) */
  comment_undefined_macro_name(REPRESENT_C11_GENERIC_CONSTRUCT_IN_IL);
#endif /* defined(REPRESENT_C11_GENERIC_CONSTRUCT_IN_IL) */
#if defined(REWRITE_UCN_ESCAPE_CHAR_IN_LOWERING)
  define_numeric_valued_macro(REWRITE_UCN_ESCAPE_CHAR_IN_LOWERING);
#else /* !defined(REWRITE_UCN_ESCAPE_CHAR_IN_LOWERING) */
  comment_undefined_macro_name(REWRITE_UCN_ESCAPE_CHAR_IN_LOWERING);
#endif /* defined(REWRITE_UCN_ESCAPE_CHAR_IN_LOWERING) */
#if defined(RISCV_VECTOR_BUILTINS_ENABLED)
  define_string_valued_macro(RISCV_VECTOR_BUILTINS_ENABLED);
#else /* !defined(RISCV_VECTOR_BUILTINS_ENABLED) */
  comment_undefined_macro_name(RISCV_VECTOR_BUILTINS_ENABLED);
#endif /* defined(RISCV_VECTOR_BUILTINS_ENABLED) */
#if defined(RTTI_ENABLING_POSSIBLE)
  define_numeric_valued_macro(RTTI_ENABLING_POSSIBLE);
#else /* !defined(RTTI_ENABLING_POSSIBLE) */
  comment_undefined_macro_name(RTTI_ENABLING_POSSIBLE);
#endif /* defined(RTTI_ENABLING_POSSIBLE) */
#if defined(RUNTIME_SUPPORTS_ARRAY_LENGTH_CHECK)
  define_numeric_valued_macro(RUNTIME_SUPPORTS_ARRAY_LENGTH_CHECK);
#else /* !defined(RUNTIME_SUPPORTS_ARRAY_LENGTH_CHECK) */
  comment_undefined_macro_name(RUNTIME_SUPPORTS_ARRAY_LENGTH_CHECK);
#endif /* defined(RUNTIME_SUPPORTS_ARRAY_LENGTH_CHECK) */
#if defined(RUNTIME_SUPPORTS_SIZED_DEALLOCATION)
  define_numeric_valued_macro(RUNTIME_SUPPORTS_SIZED_DEALLOCATION);
#else /* !defined(RUNTIME_SUPPORTS_SIZED_DEALLOCATION) */
  comment_undefined_macro_name(RUNTIME_SUPPORTS_SIZED_DEALLOCATION);
#endif /* defined(RUNTIME_SUPPORTS_SIZED_DEALLOCATION) */
#if defined(RUNTIME_USES_NAMESPACES)
  define_numeric_valued_macro(RUNTIME_USES_NAMESPACES);
#else /* !defined(RUNTIME_USES_NAMESPACES) */
  comment_undefined_macro_name(RUNTIME_USES_NAMESPACES);
#endif /* defined(RUNTIME_USES_NAMESPACES) */
#if defined(RUNTIME_USES_TYPENAME)
  define_numeric_valued_macro(RUNTIME_USES_TYPENAME);
#else /* !defined(RUNTIME_USES_TYPENAME) */
  comment_undefined_macro_name(RUNTIME_USES_TYPENAME);
#endif /* defined(RUNTIME_USES_TYPENAME) */
#if defined(SAME_REPR_INTS_INTERCHANGEABLE_IN_IL)
  define_numeric_valued_macro(SAME_REPR_INTS_INTERCHANGEABLE_IN_IL);
#else /* !defined(SAME_REPR_INTS_INTERCHANGEABLE_IN_IL) */
  comment_undefined_macro_name(SAME_REPR_INTS_INTERCHANGEABLE_IN_IL);
#endif /* defined(SAME_REPR_INTS_INTERCHANGEABLE_IN_IL) */
#if defined(SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS)
  define_numeric_valued_macro(SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS);
#else /* !defined(SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS) */
  comment_undefined_macro_name(SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS);
#endif /* defined(SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS) */
#if defined(SEQUENCING_DIAGNOSTICS_ENABLED)
  define_numeric_valued_macro(SEQUENCING_DIAGNOSTICS_ENABLED);
#else /* !defined(SEQUENCING_DIAGNOSTICS_ENABLED) */
  comment_undefined_macro_name(SEQUENCING_DIAGNOSTICS_ENABLED);
#endif /* defined(SEQUENCING_DIAGNOSTICS_ENABLED) */
#if defined(STACK_REFERENCED_INCLUDE_DIRECTORIES)
  define_numeric_valued_macro(STACK_REFERENCED_INCLUDE_DIRECTORIES);
#else /* !defined(STACK_REFERENCED_INCLUDE_DIRECTORIES) */
  comment_undefined_macro_name(STACK_REFERENCED_INCLUDE_DIRECTORIES);
#endif /* defined(STACK_REFERENCED_INCLUDE_DIRECTORIES) */
#if defined(STATEMENTS_INSERTED_FOR_INLINING_HAVE_INVOCATION_POSITION)
  define_numeric_valued_macro(
                    STATEMENTS_INSERTED_FOR_INLINING_HAVE_INVOCATION_POSITION);
#else /* !defined(STATEMENTS_INSERTED_FOR_INLINING_HAVE_INVOCATION_POSITION) */
  comment_undefined_macro_name(
                    STATEMENTS_INSERTED_FOR_INLINING_HAVE_INVOCATION_POSITION);
#endif /* defined(STATEMENTS_INSERTED_FOR_INLINING_HAVE_INVOCATION_POSITION) */
#if defined(STAT_AVAILABLE)
  define_numeric_valued_macro(STAT_AVAILABLE);
#else /* !defined(STAT_AVAILABLE) */
  comment_undefined_macro_name(STAT_AVAILABLE);
#endif /* defined(STAT_AVAILABLE) */
#if defined(STAT_FIRST_PARAM_IS_CONST)
  define_numeric_valued_macro(STAT_FIRST_PARAM_IS_CONST);
#else /* !defined(STAT_FIRST_PARAM_IS_CONST) */
  comment_undefined_macro_name(STAT_FIRST_PARAM_IS_CONST);
#endif /* defined(STAT_FIRST_PARAM_IS_CONST) */
#if defined(STDC_HOSTED)
  define_numeric_valued_macro(STDC_HOSTED);
#else /* !defined(STDC_HOSTED) */
  comment_undefined_macro_name(STDC_HOSTED);
#endif /* defined(STDC_HOSTED) */
#if defined(STDC_IEC_559)
  define_numeric_valued_macro(STDC_IEC_559);
#else /* !defined(STDC_IEC_559) */
  comment_undefined_macro_name(STDC_IEC_559);
#endif /* defined(STDC_IEC_559) */
#if defined(STDC_IEC_559_COMPLEX)
  define_numeric_valued_macro(STDC_IEC_559_COMPLEX);
#else /* !defined(STDC_IEC_559_COMPLEX) */
  comment_undefined_macro_name(STDC_IEC_559_COMPLEX);
#endif /* defined(STDC_IEC_559_COMPLEX) */
#if defined(STDC_ISO_10646)
  define_numeric_valued_macro(STDC_ISO_10646);
#else /* !defined(STDC_ISO_10646) */
  comment_undefined_macro_name(STDC_ISO_10646);
#endif /* defined(STDC_ISO_10646) */
#if defined(STDC_ISO_10646_VALUE)
  define_numeric_valued_macro(STDC_ISO_10646_VALUE);
#else /* !defined(STDC_ISO_10646_VALUE) */
  comment_undefined_macro_name(STDC_ISO_10646_VALUE);
#endif /* defined(STDC_ISO_10646_VALUE) */
#if defined(STDC_MB_MIGHT_NEQ_WC)
  define_numeric_valued_macro(STDC_MB_MIGHT_NEQ_WC);
#else /* !defined(STDC_MB_MIGHT_NEQ_WC) */
  comment_undefined_macro_name(STDC_MB_MIGHT_NEQ_WC);
#endif /* defined(STDC_MB_MIGHT_NEQ_WC) */
#if defined(STDC_ZERO_IN_NONSTRICT_MODE)
  define_numeric_valued_macro(STDC_ZERO_IN_NONSTRICT_MODE);
#else /* !defined(STDC_ZERO_IN_NONSTRICT_MODE) */
  comment_undefined_macro_name(STDC_ZERO_IN_NONSTRICT_MODE);
#endif /* defined(STDC_ZERO_IN_NONSTRICT_MODE) */
#if defined(SUN_EXTENSIONS_ALLOWED)
  define_numeric_valued_macro(SUN_EXTENSIONS_ALLOWED);
#else /* !defined(SUN_EXTENSIONS_ALLOWED) */
  comment_undefined_macro_name(SUN_EXTENSIONS_ALLOWED);
#endif /* defined(SUN_EXTENSIONS_ALLOWED) */
#if defined(SUN_IS_GENERATED_CODE_TARGET)
  define_numeric_valued_macro(SUN_IS_GENERATED_CODE_TARGET);
#else /* !defined(SUN_IS_GENERATED_CODE_TARGET) */
  comment_undefined_macro_name(SUN_IS_GENERATED_CODE_TARGET);
#endif /* defined(SUN_IS_GENERATED_CODE_TARGET) */
#if defined(SUN_TARGET_VERSION_NUMBER)
  define_numeric_valued_macro(SUN_TARGET_VERSION_NUMBER);
#else /* !defined(SUN_TARGET_VERSION_NUMBER) */
  comment_undefined_macro_name(SUN_TARGET_VERSION_NUMBER);
#endif /* defined(SUN_TARGET_VERSION_NUMBER) */
#if defined(SUPPRESS_ARRAY_STATIC_IN_GENERATED_CODE)
  define_numeric_valued_macro(SUPPRESS_ARRAY_STATIC_IN_GENERATED_CODE);
#else /* !defined(SUPPRESS_ARRAY_STATIC_IN_GENERATED_CODE) */
  comment_undefined_macro_name(SUPPRESS_ARRAY_STATIC_IN_GENERATED_CODE);
#endif /* defined(SUPPRESS_ARRAY_STATIC_IN_GENERATED_CODE) */
#if defined(SUPPRESS_CONST_IN_GENERATED_C)
  define_numeric_valued_macro(SUPPRESS_CONST_IN_GENERATED_C);
#else /* !defined(SUPPRESS_CONST_IN_GENERATED_C) */
  comment_undefined_macro_name(SUPPRESS_CONST_IN_GENERATED_C);
#endif /* defined(SUPPRESS_CONST_IN_GENERATED_C) */
#if defined(SUPPRESS_MICROSOFT_ATTRIBUTE_PROCESSING)
  define_numeric_valued_macro(SUPPRESS_MICROSOFT_ATTRIBUTE_PROCESSING);
#else /* !defined(SUPPRESS_MICROSOFT_ATTRIBUTE_PROCESSING) */
  comment_undefined_macro_name(SUPPRESS_MICROSOFT_ATTRIBUTE_PROCESSING);
#endif /* defined(SUPPRESS_MICROSOFT_ATTRIBUTE_PROCESSING) */
#if defined(SUPPRESS_NEAR_AND_FAR_IN_GENERATED_CODE)
  define_numeric_valued_macro(SUPPRESS_NEAR_AND_FAR_IN_GENERATED_CODE);
#else /* !defined(SUPPRESS_NEAR_AND_FAR_IN_GENERATED_CODE) */
  comment_undefined_macro_name(SUPPRESS_NEAR_AND_FAR_IN_GENERATED_CODE);
#endif /* defined(SUPPRESS_NEAR_AND_FAR_IN_GENERATED_CODE) */
#if defined(SUPPRESS_RESTRICT_IN_GENERATED_CODE)
  define_numeric_valued_macro(SUPPRESS_RESTRICT_IN_GENERATED_CODE);
#else /* !defined(SUPPRESS_RESTRICT_IN_GENERATED_CODE) */
  comment_undefined_macro_name(SUPPRESS_RESTRICT_IN_GENERATED_CODE);
#endif /* defined(SUPPRESS_RESTRICT_IN_GENERATED_CODE) */
#if defined(SUPPRESS_TYPEINFO_VARIABLES_WHEN_RTTI_DISABLED)
  define_numeric_valued_macro(SUPPRESS_TYPEINFO_VARIABLES_WHEN_RTTI_DISABLED);
#else /* !defined(SUPPRESS_TYPEINFO_VARIABLES_WHEN_RTTI_DISABLED) */
  comment_undefined_macro_name(SUPPRESS_TYPEINFO_VARIABLES_WHEN_RTTI_DISABLED);
#endif /* defined(SUPPRESS_TYPEINFO_VARIABLES_WHEN_RTTI_DISABLED) */
#if defined(SVR4_TRAP_NULL_POINTER_REFERENCES)
  define_numeric_valued_macro(SVR4_TRAP_NULL_POINTER_REFERENCES);
#else /* !defined(SVR4_TRAP_NULL_POINTER_REFERENCES) */
  comment_undefined_macro_name(SVR4_TRAP_NULL_POINTER_REFERENCES);
#endif /* defined(SVR4_TRAP_NULL_POINTER_REFERENCES) */
#if defined(TARG_ALERT_CHAR)
  define_string_valued_macro(TARG_ALERT_CHAR);
#else /* !defined(TARG_ALERT_CHAR) */
  comment_undefined_macro_name(TARG_ALERT_CHAR);
#endif /* defined(TARG_ALERT_CHAR) */
#if defined(TARG_ALIGNOF_DOUBLE)
  define_numeric_valued_macro(TARG_ALIGNOF_DOUBLE);
#else /* !defined(TARG_ALIGNOF_DOUBLE) */
  comment_undefined_macro_name(TARG_ALIGNOF_DOUBLE);
#endif /* defined(TARG_ALIGNOF_DOUBLE) */
#if defined(TARG_ALIGNOF_FAR_POINTER)
  define_numeric_valued_macro(TARG_ALIGNOF_FAR_POINTER);
#else /* !defined(TARG_ALIGNOF_FAR_POINTER) */
  comment_undefined_macro_name(TARG_ALIGNOF_FAR_POINTER);
#endif /* defined(TARG_ALIGNOF_FAR_POINTER) */
#if defined(TARG_ALIGNOF_FLOAT)
  define_numeric_valued_macro(TARG_ALIGNOF_FLOAT);
#else /* !defined(TARG_ALIGNOF_FLOAT) */
  comment_undefined_macro_name(TARG_ALIGNOF_FLOAT);
#endif /* defined(TARG_ALIGNOF_FLOAT) */
#if defined(TARG_ALIGNOF_FLOAT128)
  define_numeric_valued_macro(TARG_ALIGNOF_FLOAT128);
#else /* !defined(TARG_ALIGNOF_FLOAT128) */
  comment_undefined_macro_name(TARG_ALIGNOF_FLOAT128);
#endif /* defined(TARG_ALIGNOF_FLOAT128) */
#if defined(TARG_ALIGNOF_FLOAT80)
  define_numeric_valued_macro(TARG_ALIGNOF_FLOAT80);
#else /* !defined(TARG_ALIGNOF_FLOAT80) */
  comment_undefined_macro_name(TARG_ALIGNOF_FLOAT80);
#endif /* defined(TARG_ALIGNOF_FLOAT80) */
#if defined(TARG_ALIGNOF_INT)
  define_numeric_valued_macro(TARG_ALIGNOF_INT);
#else /* !defined(TARG_ALIGNOF_INT) */
  comment_undefined_macro_name(TARG_ALIGNOF_INT);
#endif /* defined(TARG_ALIGNOF_INT) */
#if defined(TARG_ALIGNOF_INT128)
  define_numeric_valued_macro(TARG_ALIGNOF_INT128);
#else /* !defined(TARG_ALIGNOF_INT128) */
  comment_undefined_macro_name(TARG_ALIGNOF_INT128);
#endif /* defined(TARG_ALIGNOF_INT128) */
#if defined(TARG_ALIGNOF_LONG)
  define_numeric_valued_macro(TARG_ALIGNOF_LONG);
#else /* !defined(TARG_ALIGNOF_LONG) */
  comment_undefined_macro_name(TARG_ALIGNOF_LONG);
#endif /* defined(TARG_ALIGNOF_LONG) */
#if defined(TARG_ALIGNOF_LONG_DOUBLE)
  define_numeric_valued_macro(TARG_ALIGNOF_LONG_DOUBLE);
#else /* !defined(TARG_ALIGNOF_LONG_DOUBLE) */
  comment_undefined_macro_name(TARG_ALIGNOF_LONG_DOUBLE);
#endif /* defined(TARG_ALIGNOF_LONG_DOUBLE) */
#if defined(TARG_ALIGNOF_LONG_LONG)
  define_numeric_valued_macro(TARG_ALIGNOF_LONG_LONG);
#else /* !defined(TARG_ALIGNOF_LONG_LONG) */
  comment_undefined_macro_name(TARG_ALIGNOF_LONG_LONG);
#endif /* defined(TARG_ALIGNOF_LONG_LONG) */
#if defined(TARG_ALIGNOF_NEAR_POINTER)
  define_numeric_valued_macro(TARG_ALIGNOF_NEAR_POINTER);
#else /* !defined(TARG_ALIGNOF_NEAR_POINTER) */
  comment_undefined_macro_name(TARG_ALIGNOF_NEAR_POINTER);
#endif /* defined(TARG_ALIGNOF_NEAR_POINTER) */
#if defined(TARG_ALIGNOF_POINTER)
  define_numeric_valued_macro(TARG_ALIGNOF_POINTER);
#else /* !defined(TARG_ALIGNOF_POINTER) */
  comment_undefined_macro_name(TARG_ALIGNOF_POINTER);
#endif /* defined(TARG_ALIGNOF_POINTER) */
#if defined(TARG_ALIGNOF_PTR_TO_DATA_MEMBER)
  define_numeric_valued_macro(TARG_ALIGNOF_PTR_TO_DATA_MEMBER);
#else /* !defined(TARG_ALIGNOF_PTR_TO_DATA_MEMBER) */
  comment_undefined_macro_name(TARG_ALIGNOF_PTR_TO_DATA_MEMBER);
#endif /* defined(TARG_ALIGNOF_PTR_TO_DATA_MEMBER) */
#if defined(TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION)
  define_numeric_valued_macro(TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION);
#else /* !defined(TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION) */
  comment_undefined_macro_name(TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION);
#endif /* defined(TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION) */
#if defined(TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS)
  define_numeric_valued_macro(TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS);
#else /* !defined(TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS) */
  comment_undefined_macro_name(TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS);
#endif /* defined(TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS) */
#if defined(TARG_ALIGNOF_SHORT)
  define_numeric_valued_macro(TARG_ALIGNOF_SHORT);
#else /* !defined(TARG_ALIGNOF_SHORT) */
  comment_undefined_macro_name(TARG_ALIGNOF_SHORT);
#endif /* defined(TARG_ALIGNOF_SHORT) */
#if defined(TARG_ALIGNOF_SIGNED_ACCUM)
  define_numeric_valued_macro(TARG_ALIGNOF_SIGNED_ACCUM);
#else /* !defined(TARG_ALIGNOF_SIGNED_ACCUM) */
  comment_undefined_macro_name(TARG_ALIGNOF_SIGNED_ACCUM);
#endif /* defined(TARG_ALIGNOF_SIGNED_ACCUM) */
#if defined(TARG_ALIGNOF_SIGNED_FRACT)
  define_numeric_valued_macro(TARG_ALIGNOF_SIGNED_FRACT);
#else /* !defined(TARG_ALIGNOF_SIGNED_FRACT) */
  comment_undefined_macro_name(TARG_ALIGNOF_SIGNED_FRACT);
#endif /* defined(TARG_ALIGNOF_SIGNED_FRACT) */
#if defined(TARG_ALIGNOF_SIGNED_LONG_ACCUM)
  define_numeric_valued_macro(TARG_ALIGNOF_SIGNED_LONG_ACCUM);
#else /* !defined(TARG_ALIGNOF_SIGNED_LONG_ACCUM) */
  comment_undefined_macro_name(TARG_ALIGNOF_SIGNED_LONG_ACCUM);
#endif /* defined(TARG_ALIGNOF_SIGNED_LONG_ACCUM) */
#if defined(TARG_ALIGNOF_SIGNED_LONG_FRACT)
  define_numeric_valued_macro(TARG_ALIGNOF_SIGNED_LONG_FRACT);
#else /* !defined(TARG_ALIGNOF_SIGNED_LONG_FRACT) */
  comment_undefined_macro_name(TARG_ALIGNOF_SIGNED_LONG_FRACT);
#endif /* defined(TARG_ALIGNOF_SIGNED_LONG_FRACT) */
#if defined(TARG_ALIGNOF_SIGNED_SHORT_ACCUM)
  define_numeric_valued_macro(TARG_ALIGNOF_SIGNED_SHORT_ACCUM);
#else /* !defined(TARG_ALIGNOF_SIGNED_SHORT_ACCUM) */
  comment_undefined_macro_name(TARG_ALIGNOF_SIGNED_SHORT_ACCUM);
#endif /* defined(TARG_ALIGNOF_SIGNED_SHORT_ACCUM) */
#if defined(TARG_ALIGNOF_SIGNED_SHORT_FRACT)
  define_numeric_valued_macro(TARG_ALIGNOF_SIGNED_SHORT_FRACT);
#else /* !defined(TARG_ALIGNOF_SIGNED_SHORT_FRACT) */
  comment_undefined_macro_name(TARG_ALIGNOF_SIGNED_SHORT_FRACT);
#endif /* defined(TARG_ALIGNOF_SIGNED_SHORT_FRACT) */
#if defined(TARG_ALIGNOF_UNSIGNED_ACCUM)
  define_numeric_valued_macro(TARG_ALIGNOF_UNSIGNED_ACCUM);
#else /* !defined(TARG_ALIGNOF_UNSIGNED_ACCUM) */
  comment_undefined_macro_name(TARG_ALIGNOF_UNSIGNED_ACCUM);
#endif /* defined(TARG_ALIGNOF_UNSIGNED_ACCUM) */
#if defined(TARG_ALIGNOF_UNSIGNED_FRACT)
  define_numeric_valued_macro(TARG_ALIGNOF_UNSIGNED_FRACT);
#else /* !defined(TARG_ALIGNOF_UNSIGNED_FRACT) */
  comment_undefined_macro_name(TARG_ALIGNOF_UNSIGNED_FRACT);
#endif /* defined(TARG_ALIGNOF_UNSIGNED_FRACT) */
#if defined(TARG_ALIGNOF_UNSIGNED_LONG_ACCUM)
  define_numeric_valued_macro(TARG_ALIGNOF_UNSIGNED_LONG_ACCUM);
#else /* !defined(TARG_ALIGNOF_UNSIGNED_LONG_ACCUM) */
  comment_undefined_macro_name(TARG_ALIGNOF_UNSIGNED_LONG_ACCUM);
#endif /* defined(TARG_ALIGNOF_UNSIGNED_LONG_ACCUM) */
#if defined(TARG_ALIGNOF_UNSIGNED_LONG_FRACT)
  define_numeric_valued_macro(TARG_ALIGNOF_UNSIGNED_LONG_FRACT);
#else /* !defined(TARG_ALIGNOF_UNSIGNED_LONG_FRACT) */
  comment_undefined_macro_name(TARG_ALIGNOF_UNSIGNED_LONG_FRACT);
#endif /* defined(TARG_ALIGNOF_UNSIGNED_LONG_FRACT) */
#if defined(TARG_ALIGNOF_UNSIGNED_SHORT_ACCUM)
  define_numeric_valued_macro(TARG_ALIGNOF_UNSIGNED_SHORT_ACCUM);
#else /* !defined(TARG_ALIGNOF_UNSIGNED_SHORT_ACCUM) */
  comment_undefined_macro_name(TARG_ALIGNOF_UNSIGNED_SHORT_ACCUM);
#endif /* defined(TARG_ALIGNOF_UNSIGNED_SHORT_ACCUM) */
#if defined(TARG_ALIGNOF_UNSIGNED_SHORT_FRACT)
  define_numeric_valued_macro(TARG_ALIGNOF_UNSIGNED_SHORT_FRACT);
#else /* !defined(TARG_ALIGNOF_UNSIGNED_SHORT_FRACT) */
  comment_undefined_macro_name(TARG_ALIGNOF_UNSIGNED_SHORT_FRACT);
#endif /* defined(TARG_ALIGNOF_UNSIGNED_SHORT_FRACT) */
#if defined(TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO)
  define_numeric_valued_macro(TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO);
#else /* !defined(TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO) */
  comment_undefined_macro_name(TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO);
#endif /* defined(TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO) */
#if defined(TARG_ALL_POINTERS_SAME_SIZE)
  define_numeric_valued_macro(TARG_ALL_POINTERS_SAME_SIZE);
#else /* !defined(TARG_ALL_POINTERS_SAME_SIZE) */
  comment_undefined_macro_name(TARG_ALL_POINTERS_SAME_SIZE);
#endif /* defined(TARG_ALL_POINTERS_SAME_SIZE) */
#if defined(TARG_BACKSPACE_CHAR)
  define_string_valued_macro(TARG_BACKSPACE_CHAR);
#else /* !defined(TARG_BACKSPACE_CHAR) */
  comment_undefined_macro_name(TARG_BACKSPACE_CHAR);
#endif /* defined(TARG_BACKSPACE_CHAR) */
#if defined(TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT)
  define_numeric_valued_macro(TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT);
#else /* !defined(TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT) */
  comment_undefined_macro_name(TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT);
#endif /* defined(TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT) */
#if defined(TARG_BIT_FIELD_CONTAINER_SIZE)
  define_numeric_valued_macro(TARG_BIT_FIELD_CONTAINER_SIZE);
#else /* !defined(TARG_BIT_FIELD_CONTAINER_SIZE) */
  comment_undefined_macro_name(TARG_BIT_FIELD_CONTAINER_SIZE);
#endif /* defined(TARG_BIT_FIELD_CONTAINER_SIZE) */
#if defined(TARG_BOOL_INT_KIND)
  define_string_valued_macro(TARG_BOOL_INT_KIND);
#else /* !defined(TARG_BOOL_INT_KIND) */
  comment_undefined_macro_name(TARG_BOOL_INT_KIND);
#endif /* defined(TARG_BOOL_INT_KIND) */
#if defined(TARG_CARR_RETURN_CHAR)
  define_string_valued_macro(TARG_CARR_RETURN_CHAR);
#else /* !defined(TARG_CARR_RETURN_CHAR) */
  comment_undefined_macro_name(TARG_CARR_RETURN_CHAR);
#endif /* defined(TARG_CARR_RETURN_CHAR) */
#if defined(TARG_CASE_SENSITIVE_EXTERNAL_NAMES)
  define_numeric_valued_macro(TARG_CASE_SENSITIVE_EXTERNAL_NAMES);
#else /* !defined(TARG_CASE_SENSITIVE_EXTERNAL_NAMES) */
  comment_undefined_macro_name(TARG_CASE_SENSITIVE_EXTERNAL_NAMES);
#endif /* defined(TARG_CASE_SENSITIVE_EXTERNAL_NAMES) */
#if defined(TARG_CHAR16_T_INT_KIND)
  define_string_valued_macro(TARG_CHAR16_T_INT_KIND);
#else /* !defined(TARG_CHAR16_T_INT_KIND) */
  comment_undefined_macro_name(TARG_CHAR16_T_INT_KIND);
#endif /* defined(TARG_CHAR16_T_INT_KIND) */
#if defined(TARG_CHAR32_T_INT_KIND)
  define_string_valued_macro(TARG_CHAR32_T_INT_KIND);
#else /* !defined(TARG_CHAR32_T_INT_KIND) */
  comment_undefined_macro_name(TARG_CHAR32_T_INT_KIND);
#endif /* defined(TARG_CHAR32_T_INT_KIND) */
#if defined(TARG_CHAR_BIT)
  define_numeric_valued_macro(TARG_CHAR_BIT);
#else /* !defined(TARG_CHAR_BIT) */
  comment_undefined_macro_name(TARG_CHAR_BIT);
#endif /* defined(TARG_CHAR_BIT) */
#if defined(TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT)
  define_numeric_valued_macro(TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT);
#else /* !defined(TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT) */
  comment_undefined_macro_name(TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT);
#endif /* defined(TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT) */
#if defined(TARG_CPP_COMPILER_DOES_NOT_VISIBLY_INJECT_FRIEND_NAMES)
  define_numeric_valued_macro(
                       TARG_CPP_COMPILER_DOES_NOT_VISIBLY_INJECT_FRIEND_NAMES);
#else /* !defined(TARG_CPP_COMPILER_DOES_NOT_VISIBLY_INJECT_FRIEND_NAMES) */
  comment_undefined_macro_name(
                       TARG_CPP_COMPILER_DOES_NOT_VISIBLY_INJECT_FRIEND_NAMES);
#endif /* defined(TARG_CPP_COMPILER_DOES_NOT_VISIBLY_INJECT_FRIEND_NAMES) */
#if defined(TARG_C_BOOL_INT_KIND)
  define_string_valued_macro(TARG_C_BOOL_INT_KIND);
#else /* !defined(TARG_C_BOOL_INT_KIND) */
  comment_undefined_macro_name(TARG_C_BOOL_INT_KIND);
#endif /* defined(TARG_C_BOOL_INT_KIND) */
#if defined(TARG_DBL_MANT_DIG)
  define_numeric_valued_macro(TARG_DBL_MANT_DIG);
#else /* !defined(TARG_DBL_MANT_DIG) */
  comment_undefined_macro_name(TARG_DBL_MANT_DIG);
#endif /* defined(TARG_DBL_MANT_DIG) */
#if defined(TARG_DBL_MAX_EXP)
  define_numeric_valued_macro(TARG_DBL_MAX_EXP);
#else /* !defined(TARG_DBL_MAX_EXP) */
  comment_undefined_macro_name(TARG_DBL_MAX_EXP);
#endif /* defined(TARG_DBL_MAX_EXP) */
#if defined(TARG_DBL_MIN_EXP)
  define_numeric_valued_macro(TARG_DBL_MIN_EXP);
#else /* !defined(TARG_DBL_MIN_EXP) */
  comment_undefined_macro_name(TARG_DBL_MIN_EXP);
#endif /* defined(TARG_DBL_MIN_EXP) */
#if defined(TARG_DEFAULT_NEW_ALIGNMENT)
  define_numeric_valued_macro(TARG_DEFAULT_NEW_ALIGNMENT);
#else /* !defined(TARG_DEFAULT_NEW_ALIGNMENT) */
  comment_undefined_macro_name(TARG_DEFAULT_NEW_ALIGNMENT);
#endif /* defined(TARG_DEFAULT_NEW_ALIGNMENT) */
#if defined(TARG_DELTA_INT_KIND)
  define_string_valued_macro(TARG_DELTA_INT_KIND);
#else /* !defined(TARG_DELTA_INT_KIND) */
  comment_undefined_macro_name(TARG_DELTA_INT_KIND);
#endif /* defined(TARG_DELTA_INT_KIND) */
#if defined(TARG_DOUBLE_FIELD_ALIGNMENT)
  define_numeric_valued_macro(TARG_DOUBLE_FIELD_ALIGNMENT);
#else /* !defined(TARG_DOUBLE_FIELD_ALIGNMENT) */
  comment_undefined_macro_name(TARG_DOUBLE_FIELD_ALIGNMENT);
#endif /* defined(TARG_DOUBLE_FIELD_ALIGNMENT) */
#if defined(TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES)
  define_numeric_valued_macro(TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES);
#else /* !defined(TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES) */
  comment_undefined_macro_name(TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES);
#endif /* defined(TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES) */
#if defined(TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED)
  define_numeric_valued_macro(TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED);
#else /* !defined(TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED) */
  comment_undefined_macro_name(TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED);
#endif /* defined(TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED) */
#if defined(TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT)
  define_numeric_valued_macro(TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT);
#else /* !defined(TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT) */
  comment_undefined_macro_name(TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT);
#endif /* defined(TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT) */
#if defined(TARG_ESC_CHAR)
  define_string_valued_macro(TARG_ESC_CHAR);
#else /* !defined(TARG_ESC_CHAR) */
  comment_undefined_macro_name(TARG_ESC_CHAR);
#endif /* defined(TARG_ESC_CHAR) */
#if defined(TARG_ETS_FLAG_TYPE_INT_KIND)
  define_string_valued_macro(TARG_ETS_FLAG_TYPE_INT_KIND);
#else /* !defined(TARG_ETS_FLAG_TYPE_INT_KIND) */
  comment_undefined_macro_name(TARG_ETS_FLAG_TYPE_INT_KIND);
#endif /* defined(TARG_ETS_FLAG_TYPE_INT_KIND) */
#if defined(TARG_EXTERNAL_NAMES_GET_UNDERSCORE_ADDED)
  define_numeric_valued_macro(TARG_EXTERNAL_NAMES_GET_UNDERSCORE_ADDED);
#else /* !defined(TARG_EXTERNAL_NAMES_GET_UNDERSCORE_ADDED) */
  comment_undefined_macro_name(TARG_EXTERNAL_NAMES_GET_UNDERSCORE_ADDED);
#endif /* defined(TARG_EXTERNAL_NAMES_GET_UNDERSCORE_ADDED) */
#if defined(TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE)
  define_numeric_valued_macro(TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE);
#else /* !defined(TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE) */
  comment_undefined_macro_name(TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE);
#endif /* defined(TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE) */
#if defined(TARG_FLOAT128_FIELD_ALIGNMENT)
  define_numeric_valued_macro(TARG_FLOAT128_FIELD_ALIGNMENT);
#else /* !defined(TARG_FLOAT128_FIELD_ALIGNMENT) */
  comment_undefined_macro_name(TARG_FLOAT128_FIELD_ALIGNMENT);
#endif /* defined(TARG_FLOAT128_FIELD_ALIGNMENT) */
#if defined(TARG_FLOAT80_FIELD_ALIGNMENT)
  define_numeric_valued_macro(TARG_FLOAT80_FIELD_ALIGNMENT);
#else /* !defined(TARG_FLOAT80_FIELD_ALIGNMENT) */
  comment_undefined_macro_name(TARG_FLOAT80_FIELD_ALIGNMENT);
#endif /* defined(TARG_FLOAT80_FIELD_ALIGNMENT) */
#if defined(TARG_FLOAT_FIELD_ALIGNMENT)
  define_numeric_valued_macro(TARG_FLOAT_FIELD_ALIGNMENT);
#else /* !defined(TARG_FLOAT_FIELD_ALIGNMENT) */
  comment_undefined_macro_name(TARG_FLOAT_FIELD_ALIGNMENT);
#endif /* defined(TARG_FLOAT_FIELD_ALIGNMENT) */
#if defined(TARG_FLT128_MANT_DIG)
  define_numeric_valued_macro(TARG_FLT128_MANT_DIG);
#else /* !defined(TARG_FLT128_MANT_DIG) */
  comment_undefined_macro_name(TARG_FLT128_MANT_DIG);
#endif /* defined(TARG_FLT128_MANT_DIG) */
#if defined(TARG_FLT128_MAX_EXP)
  define_numeric_valued_macro(TARG_FLT128_MAX_EXP);
#else /* !defined(TARG_FLT128_MAX_EXP) */
  comment_undefined_macro_name(TARG_FLT128_MAX_EXP);
#endif /* defined(TARG_FLT128_MAX_EXP) */
#if defined(TARG_FLT128_MIN_EXP)
  define_numeric_valued_macro(TARG_FLT128_MIN_EXP);
#else /* !defined(TARG_FLT128_MIN_EXP) */
  comment_undefined_macro_name(TARG_FLT128_MIN_EXP);
#endif /* defined(TARG_FLT128_MIN_EXP) */
#if defined(TARG_FLT80_MANT_DIG)
  define_numeric_valued_macro(TARG_FLT80_MANT_DIG);
#else /* !defined(TARG_FLT80_MANT_DIG) */
  comment_undefined_macro_name(TARG_FLT80_MANT_DIG);
#endif /* defined(TARG_FLT80_MANT_DIG) */
#if defined(TARG_FLT80_MAX_EXP)
  define_numeric_valued_macro(TARG_FLT80_MAX_EXP);
#else /* !defined(TARG_FLT80_MAX_EXP) */
  comment_undefined_macro_name(TARG_FLT80_MAX_EXP);
#endif /* defined(TARG_FLT80_MAX_EXP) */
#if defined(TARG_FLT80_MIN_EXP)
  define_numeric_valued_macro(TARG_FLT80_MIN_EXP);
#else /* !defined(TARG_FLT80_MIN_EXP) */
  comment_undefined_macro_name(TARG_FLT80_MIN_EXP);
#endif /* defined(TARG_FLT80_MIN_EXP) */
#if defined(TARG_FLT_MANT_DIG)
  define_numeric_valued_macro(TARG_FLT_MANT_DIG);
#else /* !defined(TARG_FLT_MANT_DIG) */
  comment_undefined_macro_name(TARG_FLT_MANT_DIG);
#endif /* defined(TARG_FLT_MANT_DIG) */
#if defined(TARG_FLT_MAX_EXP)
  define_numeric_valued_macro(TARG_FLT_MAX_EXP);
#else /* !defined(TARG_FLT_MAX_EXP) */
  comment_undefined_macro_name(TARG_FLT_MAX_EXP);
#endif /* defined(TARG_FLT_MAX_EXP) */
#if defined(TARG_FLT_MIN_EXP)
  define_numeric_valued_macro(TARG_FLT_MIN_EXP);
#else /* !defined(TARG_FLT_MIN_EXP) */
  comment_undefined_macro_name(TARG_FLT_MIN_EXP);
#endif /* defined(TARG_FLT_MIN_EXP) */
#if defined(TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED)
  define_numeric_valued_macro(TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED);
#else /* !defined(TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED) */
  comment_undefined_macro_name(TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED);
#endif /* defined(TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED) */
#if defined(TARG_FORM_FEED_CHAR)
  define_string_valued_macro(TARG_FORM_FEED_CHAR);
#else /* !defined(TARG_FORM_FEED_CHAR) */
  comment_undefined_macro_name(TARG_FORM_FEED_CHAR);
#endif /* defined(TARG_FORM_FEED_CHAR) */
#if defined(TARG_FRACTIONAL_BITS_FOR_SIGNED_ACCUM)
  define_numeric_valued_macro(TARG_FRACTIONAL_BITS_FOR_SIGNED_ACCUM);
#else /* !defined(TARG_FRACTIONAL_BITS_FOR_SIGNED_ACCUM) */
  comment_undefined_macro_name(TARG_FRACTIONAL_BITS_FOR_SIGNED_ACCUM);
#endif /* defined(TARG_FRACTIONAL_BITS_FOR_SIGNED_ACCUM) */
#if defined(TARG_FRACTIONAL_BITS_FOR_SIGNED_FRACT)
  define_numeric_valued_macro(TARG_FRACTIONAL_BITS_FOR_SIGNED_FRACT);
#else /* !defined(TARG_FRACTIONAL_BITS_FOR_SIGNED_FRACT) */
  comment_undefined_macro_name(TARG_FRACTIONAL_BITS_FOR_SIGNED_FRACT);
#endif /* defined(TARG_FRACTIONAL_BITS_FOR_SIGNED_FRACT) */
#if defined(TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_ACCUM)
  define_numeric_valued_macro(TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_ACCUM);
#else /* !defined(TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_ACCUM) */
  comment_undefined_macro_name(TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_ACCUM);
#endif /* defined(TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_ACCUM) */
#if defined(TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_FRACT)
  define_numeric_valued_macro(TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_FRACT);
#else /* !defined(TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_FRACT) */
  comment_undefined_macro_name(TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_FRACT);
#endif /* defined(TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_FRACT) */
#if defined(TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_ACCUM)
  define_numeric_valued_macro(TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_ACCUM);
#else /* !defined(TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_ACCUM) */
  comment_undefined_macro_name(TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_ACCUM);
#endif /* defined(TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_ACCUM) */
#if defined(TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_FRACT)
  define_numeric_valued_macro(TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_FRACT);
#else /* !defined(TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_FRACT) */
  comment_undefined_macro_name(TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_FRACT);
#endif /* defined(TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_FRACT) */
#if defined(TARG_FRACTIONAL_BITS_FOR_UNSIGNED_ACCUM)
  define_numeric_valued_macro(TARG_FRACTIONAL_BITS_FOR_UNSIGNED_ACCUM);
#else /* !defined(TARG_FRACTIONAL_BITS_FOR_UNSIGNED_ACCUM) */
  comment_undefined_macro_name(TARG_FRACTIONAL_BITS_FOR_UNSIGNED_ACCUM);
#endif /* defined(TARG_FRACTIONAL_BITS_FOR_UNSIGNED_ACCUM) */
#if defined(TARG_FRACTIONAL_BITS_FOR_UNSIGNED_FRACT)
  define_numeric_valued_macro(TARG_FRACTIONAL_BITS_FOR_UNSIGNED_FRACT);
#else /* !defined(TARG_FRACTIONAL_BITS_FOR_UNSIGNED_FRACT) */
  comment_undefined_macro_name(TARG_FRACTIONAL_BITS_FOR_UNSIGNED_FRACT);
#endif /* defined(TARG_FRACTIONAL_BITS_FOR_UNSIGNED_FRACT) */
#if defined(TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_ACCUM)
  define_numeric_valued_macro(TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_ACCUM);
#else /* !defined(TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_ACCUM) */
  comment_undefined_macro_name(TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_ACCUM);
#endif /* defined(TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_ACCUM) */
#if defined(TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_FRACT)
  define_numeric_valued_macro(TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_FRACT);
#else /* !defined(TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_FRACT) */
  comment_undefined_macro_name(TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_FRACT);
#endif /* defined(TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_FRACT) */
#if defined(TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_ACCUM)
  define_numeric_valued_macro(TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_ACCUM);
#else /* !defined(TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_ACCUM) */
  comment_undefined_macro_name(TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_ACCUM);
#endif /* defined(TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_ACCUM) */
#if defined(TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_FRACT)
  define_numeric_valued_macro(TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_FRACT);
#else /* !defined(TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_FRACT) */
  comment_undefined_macro_name(TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_FRACT);
#endif /* defined(TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_FRACT) */
#if defined(TARG_HAS_IEEE_FLOATING_POINT)
  define_numeric_valued_macro(TARG_HAS_IEEE_FLOATING_POINT);
#else /* !defined(TARG_HAS_IEEE_FLOATING_POINT) */
  comment_undefined_macro_name(TARG_HAS_IEEE_FLOATING_POINT);
#endif /* defined(TARG_HAS_IEEE_FLOATING_POINT) */
#if defined(TARG_HAS_SIGNED_CHARS)
  define_numeric_valued_macro(TARG_HAS_SIGNED_CHARS);
#else /* !defined(TARG_HAS_SIGNED_CHARS) */
  comment_undefined_macro_name(TARG_HAS_SIGNED_CHARS);
#endif /* defined(TARG_HAS_SIGNED_CHARS) */
#if defined(TARG_HORIZ_TAB_CHAR)
  define_string_valued_macro(TARG_HORIZ_TAB_CHAR);
#else /* !defined(TARG_HORIZ_TAB_CHAR) */
  comment_undefined_macro_name(TARG_HORIZ_TAB_CHAR);
#endif /* defined(TARG_HORIZ_TAB_CHAR) */
#if defined(TARG_HOST_STRING_CHAR_BIT)
  define_numeric_valued_macro(TARG_HOST_STRING_CHAR_BIT);
#else /* !defined(TARG_HOST_STRING_CHAR_BIT) */
  comment_undefined_macro_name(TARG_HOST_STRING_CHAR_BIT);
#endif /* defined(TARG_HOST_STRING_CHAR_BIT) */
#if defined(TARG_IA64_ABI_USE_GUARD_ACQUIRE_RELEASE)
  define_numeric_valued_macro(TARG_IA64_ABI_USE_GUARD_ACQUIRE_RELEASE);
#else /* !defined(TARG_IA64_ABI_USE_GUARD_ACQUIRE_RELEASE) */
  comment_undefined_macro_name(TARG_IA64_ABI_USE_GUARD_ACQUIRE_RELEASE);
#endif /* defined(TARG_IA64_ABI_USE_GUARD_ACQUIRE_RELEASE) */
#if defined(TARG_IA64_ABI_USE_INT_STATIC_INIT_GUARD)
  define_numeric_valued_macro(TARG_IA64_ABI_USE_INT_STATIC_INIT_GUARD);
#else /* !defined(TARG_IA64_ABI_USE_INT_STATIC_INIT_GUARD) */
  comment_undefined_macro_name(TARG_IA64_ABI_USE_INT_STATIC_INIT_GUARD);
#endif /* defined(TARG_IA64_ABI_USE_INT_STATIC_INIT_GUARD) */
#if defined(TARG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES)
  define_numeric_valued_macro(TARG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES);
#else /* !defined(TARG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES) */
  comment_undefined_macro_name(TARG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES);
#endif /* defined(TARG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES) */
#if defined(TARG_IA64_ABI_USE_VARIANT_PTR_TO_MEMBER_FUNCTION_REPR)
  define_numeric_valued_macro(
                        TARG_IA64_ABI_USE_VARIANT_PTR_TO_MEMBER_FUNCTION_REPR);
#else /* !defined(TARG_IA64_ABI_USE_VARIANT_PTR_TO_MEMBER_FUNCTION_REPR) */
  comment_undefined_macro_name(
                        TARG_IA64_ABI_USE_VARIANT_PTR_TO_MEMBER_FUNCTION_REPR);
#endif /* defined(TARG_IA64_ABI_USE_VARIANT_PTR_TO_MEMBER_FUNCTION_REPR) */
#if defined(TARG_IA64_ABI_VARIANT_CTORS_AND_DTORS_RETURN_THIS)
  define_numeric_valued_macro(
                            TARG_IA64_ABI_VARIANT_CTORS_AND_DTORS_RETURN_THIS);
#else /* !defined(TARG_IA64_ABI_VARIANT_CTORS_AND_DTORS_RETURN_THIS) */
  comment_undefined_macro_name(
                            TARG_IA64_ABI_VARIANT_CTORS_AND_DTORS_RETURN_THIS);
#endif /* defined(TARG_IA64_ABI_VARIANT_CTORS_AND_DTORS_RETURN_THIS) */
#if defined(TARG_IA64_ABI_VARIANT_KEY_FUNCTION)
  define_numeric_valued_macro(TARG_IA64_ABI_VARIANT_KEY_FUNCTION);
#else /* !defined(TARG_IA64_ABI_VARIANT_KEY_FUNCTION) */
  comment_undefined_macro_name(TARG_IA64_ABI_VARIANT_KEY_FUNCTION);
#endif /* defined(TARG_IA64_ABI_VARIANT_KEY_FUNCTION) */
#if defined(TARG_IA64_VTABLE_ENTRY_INT_KIND)
  define_string_valued_macro(TARG_IA64_VTABLE_ENTRY_INT_KIND);
#else /* !defined(TARG_IA64_VTABLE_ENTRY_INT_KIND) */
  comment_undefined_macro_name(TARG_IA64_VTABLE_ENTRY_INT_KIND);
#endif /* defined(TARG_IA64_VTABLE_ENTRY_INT_KIND) */
#if defined(TARG_INT128_FIELD_ALIGNMENT)
  define_numeric_valued_macro(TARG_INT128_FIELD_ALIGNMENT);
#else /* !defined(TARG_INT128_FIELD_ALIGNMENT) */
  comment_undefined_macro_name(TARG_INT128_FIELD_ALIGNMENT);
#endif /* defined(TARG_INT128_FIELD_ALIGNMENT) */
#if defined(TARG_INT_FIELD_ALIGNMENT)
  define_numeric_valued_macro(TARG_INT_FIELD_ALIGNMENT);
#else /* !defined(TARG_INT_FIELD_ALIGNMENT) */
  comment_undefined_macro_name(TARG_INT_FIELD_ALIGNMENT);
#endif /* defined(TARG_INT_FIELD_ALIGNMENT) */
#if defined(TARG_JMP_BUF_ELEMENTS_ARE_FLOAT)
  define_numeric_valued_macro(TARG_JMP_BUF_ELEMENTS_ARE_FLOAT);
#else /* !defined(TARG_JMP_BUF_ELEMENTS_ARE_FLOAT) */
  comment_undefined_macro_name(TARG_JMP_BUF_ELEMENTS_ARE_FLOAT);
#endif /* defined(TARG_JMP_BUF_ELEMENTS_ARE_FLOAT) */
#if defined(TARG_JMP_BUF_ELEMENT_FLOAT_KIND)
  define_string_valued_macro(TARG_JMP_BUF_ELEMENT_FLOAT_KIND);
#else /* !defined(TARG_JMP_BUF_ELEMENT_FLOAT_KIND) */
  comment_undefined_macro_name(TARG_JMP_BUF_ELEMENT_FLOAT_KIND);
#endif /* defined(TARG_JMP_BUF_ELEMENT_FLOAT_KIND) */
#if defined(TARG_JMP_BUF_ELEMENT_INT_KIND)
  define_string_valued_macro(TARG_JMP_BUF_ELEMENT_INT_KIND);
#else /* !defined(TARG_JMP_BUF_ELEMENT_INT_KIND) */
  comment_undefined_macro_name(TARG_JMP_BUF_ELEMENT_INT_KIND);
#endif /* defined(TARG_JMP_BUF_ELEMENT_INT_KIND) */
#if defined(TARG_JMP_BUF_NUM_ELEMENTS)
  define_numeric_valued_macro(TARG_JMP_BUF_NUM_ELEMENTS);
#else /* !defined(TARG_JMP_BUF_NUM_ELEMENTS) */
  comment_undefined_macro_name(TARG_JMP_BUF_NUM_ELEMENTS);
#endif /* defined(TARG_JMP_BUF_NUM_ELEMENTS) */
#if defined(TARG_LDBL_MANT_DIG)
  define_numeric_valued_macro(TARG_LDBL_MANT_DIG);
#else /* !defined(TARG_LDBL_MANT_DIG) */
  comment_undefined_macro_name(TARG_LDBL_MANT_DIG);
#endif /* defined(TARG_LDBL_MANT_DIG) */
#if defined(TARG_LDBL_MAX_EXP)
  define_numeric_valued_macro(TARG_LDBL_MAX_EXP);
#else /* !defined(TARG_LDBL_MAX_EXP) */
  comment_undefined_macro_name(TARG_LDBL_MAX_EXP);
#endif /* defined(TARG_LDBL_MAX_EXP) */
#if defined(TARG_LDBL_MIN_EXP)
  define_numeric_valued_macro(TARG_LDBL_MIN_EXP);
#else /* !defined(TARG_LDBL_MIN_EXP) */
  comment_undefined_macro_name(TARG_LDBL_MIN_EXP);
#endif /* defined(TARG_LDBL_MIN_EXP) */
#if defined(TARG_LIBGCC_CMP_RETURN_MODE)
  define_string_valued_macro(TARG_LIBGCC_CMP_RETURN_MODE);
#else /* !defined(TARG_LIBGCC_CMP_RETURN_MODE) */
  comment_undefined_macro_name(TARG_LIBGCC_CMP_RETURN_MODE);
#endif /* defined(TARG_LIBGCC_CMP_RETURN_MODE) */
#if defined(TARG_LIBGCC_SHIFT_COUNT_MODE)
  define_string_valued_macro(TARG_LIBGCC_SHIFT_COUNT_MODE);
#else /* !defined(TARG_LIBGCC_SHIFT_COUNT_MODE) */
  comment_undefined_macro_name(TARG_LIBGCC_SHIFT_COUNT_MODE);
#endif /* defined(TARG_LIBGCC_SHIFT_COUNT_MODE) */
#if defined(TARG_LITTLE_ENDIAN)
  define_numeric_valued_macro(TARG_LITTLE_ENDIAN);
#else /* !defined(TARG_LITTLE_ENDIAN) */
  comment_undefined_macro_name(TARG_LITTLE_ENDIAN);
#endif /* defined(TARG_LITTLE_ENDIAN) */
#if defined(TARG_LONG_DOUBLE_FIELD_ALIGNMENT)
  define_numeric_valued_macro(TARG_LONG_DOUBLE_FIELD_ALIGNMENT);
#else /* !defined(TARG_LONG_DOUBLE_FIELD_ALIGNMENT) */
  comment_undefined_macro_name(TARG_LONG_DOUBLE_FIELD_ALIGNMENT);
#endif /* defined(TARG_LONG_DOUBLE_FIELD_ALIGNMENT) */
#if defined(TARG_LONG_FIELD_ALIGNMENT)
  define_numeric_valued_macro(TARG_LONG_FIELD_ALIGNMENT);
#else /* !defined(TARG_LONG_FIELD_ALIGNMENT) */
  comment_undefined_macro_name(TARG_LONG_FIELD_ALIGNMENT);
#endif /* defined(TARG_LONG_FIELD_ALIGNMENT) */
#if defined(TARG_LONG_LONG_FIELD_ALIGNMENT)
  define_numeric_valued_macro(TARG_LONG_LONG_FIELD_ALIGNMENT);
#else /* !defined(TARG_LONG_LONG_FIELD_ALIGNMENT) */
  comment_undefined_macro_name(TARG_LONG_LONG_FIELD_ALIGNMENT);
#endif /* defined(TARG_LONG_LONG_FIELD_ALIGNMENT) */
#if defined(TARG_MAXIMUM_INTRINSIC_ALIGNMENT)
  define_numeric_valued_macro(TARG_MAXIMUM_INTRINSIC_ALIGNMENT);
#else /* !defined(TARG_MAXIMUM_INTRINSIC_ALIGNMENT) */
  comment_undefined_macro_name(TARG_MAXIMUM_INTRINSIC_ALIGNMENT);
#endif /* defined(TARG_MAXIMUM_INTRINSIC_ALIGNMENT) */
#if defined(TARG_MAXIMUM_PACK_ALIGNMENT)
  define_numeric_valued_macro(TARG_MAXIMUM_PACK_ALIGNMENT);
#else /* !defined(TARG_MAXIMUM_PACK_ALIGNMENT) */
  comment_undefined_macro_name(TARG_MAXIMUM_PACK_ALIGNMENT);
#endif /* defined(TARG_MAXIMUM_PACK_ALIGNMENT) */
#if defined(TARG_MAX_BASE_CLASS_OFFSET)
  define_numeric_valued_macro(TARG_MAX_BASE_CLASS_OFFSET);
#else /* !defined(TARG_MAX_BASE_CLASS_OFFSET) */
  comment_undefined_macro_name(TARG_MAX_BASE_CLASS_OFFSET);
#endif /* defined(TARG_MAX_BASE_CLASS_OFFSET) */
#if defined(TARG_MAX_CLASS_OBJECT_SIZE)
  define_numeric_valued_macro(TARG_MAX_CLASS_OBJECT_SIZE);
#else /* !defined(TARG_MAX_CLASS_OBJECT_SIZE) */
  comment_undefined_macro_name(TARG_MAX_CLASS_OBJECT_SIZE);
#endif /* defined(TARG_MAX_CLASS_OBJECT_SIZE) */
#if defined(TARG_MICROSOFT_BIT_FIELD_ALLOCATION)
  define_numeric_valued_macro(TARG_MICROSOFT_BIT_FIELD_ALLOCATION);
#else /* !defined(TARG_MICROSOFT_BIT_FIELD_ALLOCATION) */
  comment_undefined_macro_name(TARG_MICROSOFT_BIT_FIELD_ALLOCATION);
#endif /* defined(TARG_MICROSOFT_BIT_FIELD_ALLOCATION) */
#if defined(TARG_MICROSOFT_PTR_TO_MEMBER_SIZING)
  define_numeric_valued_macro(TARG_MICROSOFT_PTR_TO_MEMBER_SIZING);
#else /* !defined(TARG_MICROSOFT_PTR_TO_MEMBER_SIZING) */
  comment_undefined_macro_name(TARG_MICROSOFT_PTR_TO_MEMBER_SIZING);
#endif /* defined(TARG_MICROSOFT_PTR_TO_MEMBER_SIZING) */
#if defined(TARG_MINIMUM_PACK_ALIGNMENT)
  define_numeric_valued_macro(TARG_MINIMUM_PACK_ALIGNMENT);
#else /* !defined(TARG_MINIMUM_PACK_ALIGNMENT) */
  comment_undefined_macro_name(TARG_MINIMUM_PACK_ALIGNMENT);
#endif /* defined(TARG_MINIMUM_PACK_ALIGNMENT) */
#if defined(TARG_MINIMUM_STRUCT_ALIGNMENT)
  define_numeric_valued_macro(TARG_MINIMUM_STRUCT_ALIGNMENT);
#else /* !defined(TARG_MINIMUM_STRUCT_ALIGNMENT) */
  comment_undefined_macro_name(TARG_MINIMUM_STRUCT_ALIGNMENT);
#endif /* defined(TARG_MINIMUM_STRUCT_ALIGNMENT) */
#if defined(TARG_NEWLINE_CHAR)
  define_string_valued_macro(TARG_NEWLINE_CHAR);
#else /* !defined(TARG_NEWLINE_CHAR) */
  comment_undefined_macro_name(TARG_NEWLINE_CHAR);
#endif /* defined(TARG_NEWLINE_CHAR) */
#if defined(TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED)
  define_numeric_valued_macro(TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED);
#else /* !defined(TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED) */
  comment_undefined_macro_name(TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED);
#endif /* defined(TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED) */
#if defined(TARG_NO_ERROR_ON_INTEGER_OVERFLOW)
  define_numeric_valued_macro(TARG_NO_ERROR_ON_INTEGER_OVERFLOW);
#else /* !defined(TARG_NO_ERROR_ON_INTEGER_OVERFLOW) */
  comment_undefined_macro_name(TARG_NO_ERROR_ON_INTEGER_OVERFLOW);
#endif /* defined(TARG_NO_ERROR_ON_INTEGER_OVERFLOW) */
#if defined(TARG_NULL_IS_ALL_BITS_ZERO)
  define_numeric_valued_macro(TARG_NULL_IS_ALL_BITS_ZERO);
#else /* !defined(TARG_NULL_IS_ALL_BITS_ZERO) */
  comment_undefined_macro_name(TARG_NULL_IS_ALL_BITS_ZERO);
#endif /* defined(TARG_NULL_IS_ALL_BITS_ZERO) */
#if defined(TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT)
  define_numeric_valued_macro(TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT);
#else /* !defined(TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT) */
  comment_undefined_macro_name(TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT);
#endif /* defined(TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT) */
#if defined(TARG_PAD_ALLOCATED_EMPTY_BASE)
  define_numeric_valued_macro(TARG_PAD_ALLOCATED_EMPTY_BASE);
#else /* !defined(TARG_PAD_ALLOCATED_EMPTY_BASE) */
  comment_undefined_macro_name(TARG_PAD_ALLOCATED_EMPTY_BASE);
#endif /* defined(TARG_PAD_ALLOCATED_EMPTY_BASE) */
#if defined(TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE)
  define_numeric_valued_macro(TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE);
#else /* !defined(TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE) */
  comment_undefined_macro_name(TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE);
#endif /* defined(TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE) */
#if defined(TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED)
  define_numeric_valued_macro(TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED);
#else /* !defined(TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED) */
  comment_undefined_macro_name(TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED);
#endif /* defined(TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED) */
#if defined(TARG_POINTER_MODE)
  define_string_valued_macro(TARG_POINTER_MODE);
#else /* !defined(TARG_POINTER_MODE) */
  comment_undefined_macro_name(TARG_POINTER_MODE);
#endif /* defined(TARG_POINTER_MODE) */
#if defined(TARG_PTRDIFF_T_INT_KIND)
  define_string_valued_macro(TARG_PTRDIFF_T_INT_KIND);
#else /* !defined(TARG_PTRDIFF_T_INT_KIND) */
  comment_undefined_macro_name(TARG_PTRDIFF_T_INT_KIND);
#endif /* defined(TARG_PTRDIFF_T_INT_KIND) */
#if defined(TARG_REGION_NUMBER_INT_KIND)
  define_string_valued_macro(TARG_REGION_NUMBER_INT_KIND);
#else /* !defined(TARG_REGION_NUMBER_INT_KIND) */
  comment_undefined_macro_name(TARG_REGION_NUMBER_INT_KIND);
#endif /* defined(TARG_REGION_NUMBER_INT_KIND) */
#if defined(TARG_REUSE_TAIL_PADDING)
  define_numeric_valued_macro(TARG_REUSE_TAIL_PADDING);
#else /* !defined(TARG_REUSE_TAIL_PADDING) */
  comment_undefined_macro_name(TARG_REUSE_TAIL_PADDING);
#endif /* defined(TARG_REUSE_TAIL_PADDING) */
#if defined(TARG_RIGHT_SHIFT_IS_ARITHMETIC)
  define_numeric_valued_macro(TARG_RIGHT_SHIFT_IS_ARITHMETIC);
#else /* !defined(TARG_RIGHT_SHIFT_IS_ARITHMETIC) */
  comment_undefined_macro_name(TARG_RIGHT_SHIFT_IS_ARITHMETIC);
#endif /* defined(TARG_RIGHT_SHIFT_IS_ARITHMETIC) */
#if defined(TARG_RUNTIME_ELEM_COUNT_INT_KIND)
  comment_string_valued_macro(TARG_RUNTIME_ELEM_COUNT_INT_KIND);
#else /* !defined(TARG_RUNTIME_ELEM_COUNT_INT_KIND) */
  comment_undefined_macro_name(TARG_RUNTIME_ELEM_COUNT_INT_KIND);
#endif /* defined(TARG_RUNTIME_ELEM_COUNT_INT_KIND) */
#if defined(TARG_SETJMP_FUNC)
  define_string_valued_macro(TARG_SETJMP_FUNC);
#else /* !defined(TARG_SETJMP_FUNC) */
  comment_undefined_macro_name(TARG_SETJMP_FUNC);
#endif /* defined(TARG_SETJMP_FUNC) */
#if defined(TARG_SHORT_FIELD_ALIGNMENT)
  define_numeric_valued_macro(TARG_SHORT_FIELD_ALIGNMENT);
#else /* !defined(TARG_SHORT_FIELD_ALIGNMENT) */
  comment_undefined_macro_name(TARG_SHORT_FIELD_ALIGNMENT);
#endif /* defined(TARG_SHORT_FIELD_ALIGNMENT) */
#if defined(TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME)
  define_numeric_valued_macro(TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME);
#else /* !defined(TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME) */
  comment_undefined_macro_name(TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME);
#endif /* defined(TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME) */
#if defined(TARG_SIZEOF_DOUBLE)
  define_numeric_valued_macro(TARG_SIZEOF_DOUBLE);
#else /* !defined(TARG_SIZEOF_DOUBLE) */
  comment_undefined_macro_name(TARG_SIZEOF_DOUBLE);
#endif /* defined(TARG_SIZEOF_DOUBLE) */
#if defined(TARG_SIZEOF_FAR_POINTER)
  define_numeric_valued_macro(TARG_SIZEOF_FAR_POINTER);
#else /* !defined(TARG_SIZEOF_FAR_POINTER) */
  comment_undefined_macro_name(TARG_SIZEOF_FAR_POINTER);
#endif /* defined(TARG_SIZEOF_FAR_POINTER) */
#if defined(TARG_SIZEOF_FLOAT)
  define_numeric_valued_macro(TARG_SIZEOF_FLOAT);
#else /* !defined(TARG_SIZEOF_FLOAT) */
  comment_undefined_macro_name(TARG_SIZEOF_FLOAT);
#endif /* defined(TARG_SIZEOF_FLOAT) */
#if defined(TARG_SIZEOF_FLOAT128)
  define_numeric_valued_macro(TARG_SIZEOF_FLOAT128);
#else /* !defined(TARG_SIZEOF_FLOAT128) */
  comment_undefined_macro_name(TARG_SIZEOF_FLOAT128);
#endif /* defined(TARG_SIZEOF_FLOAT128) */
#if defined(TARG_SIZEOF_FLOAT80)
  define_numeric_valued_macro(TARG_SIZEOF_FLOAT80);
#else /* !defined(TARG_SIZEOF_FLOAT80) */
  comment_undefined_macro_name(TARG_SIZEOF_FLOAT80);
#endif /* defined(TARG_SIZEOF_FLOAT80) */
#if defined(TARG_SIZEOF_INT)
  define_numeric_valued_macro(TARG_SIZEOF_INT);
#else /* !defined(TARG_SIZEOF_INT) */
  comment_undefined_macro_name(TARG_SIZEOF_INT);
#endif /* defined(TARG_SIZEOF_INT) */
#if defined(TARG_SIZEOF_INT128)
  define_numeric_valued_macro(TARG_SIZEOF_INT128);
#else /* !defined(TARG_SIZEOF_INT128) */
  comment_undefined_macro_name(TARG_SIZEOF_INT128);
#endif /* defined(TARG_SIZEOF_INT128) */
#if defined(TARG_SIZEOF_LARGEST_ATOMIC)
  define_numeric_valued_macro(TARG_SIZEOF_LARGEST_ATOMIC);
#else /* !defined(TARG_SIZEOF_LARGEST_ATOMIC) */
  comment_undefined_macro_name(TARG_SIZEOF_LARGEST_ATOMIC);
#endif /* defined(TARG_SIZEOF_LARGEST_ATOMIC) */
#if defined(TARG_SIZEOF_LARGEST_FIXED_POINT)
  define_numeric_valued_macro(TARG_SIZEOF_LARGEST_FIXED_POINT);
#else /* !defined(TARG_SIZEOF_LARGEST_FIXED_POINT) */
  comment_undefined_macro_name(TARG_SIZEOF_LARGEST_FIXED_POINT);
#endif /* defined(TARG_SIZEOF_LARGEST_FIXED_POINT) */
#if defined(TARG_SIZEOF_LARGEST_INTEGER)
  define_numeric_valued_macro(TARG_SIZEOF_LARGEST_INTEGER);
#else /* !defined(TARG_SIZEOF_LARGEST_INTEGER) */
  comment_undefined_macro_name(TARG_SIZEOF_LARGEST_INTEGER);
#endif /* defined(TARG_SIZEOF_LARGEST_INTEGER) */
#if defined(TARG_SIZEOF_LONG)
  define_numeric_valued_macro(TARG_SIZEOF_LONG);
#else /* !defined(TARG_SIZEOF_LONG) */
  comment_undefined_macro_name(TARG_SIZEOF_LONG);
#endif /* defined(TARG_SIZEOF_LONG) */
#if defined(TARG_SIZEOF_LONG_DOUBLE)
  define_numeric_valued_macro(TARG_SIZEOF_LONG_DOUBLE);
#else /* !defined(TARG_SIZEOF_LONG_DOUBLE) */
  comment_undefined_macro_name(TARG_SIZEOF_LONG_DOUBLE);
#endif /* defined(TARG_SIZEOF_LONG_DOUBLE) */
#if defined(TARG_SIZEOF_LONG_LONG)
  define_numeric_valued_macro(TARG_SIZEOF_LONG_LONG);
#else /* !defined(TARG_SIZEOF_LONG_LONG) */
  comment_undefined_macro_name(TARG_SIZEOF_LONG_LONG);
#endif /* defined(TARG_SIZEOF_LONG_LONG) */
#if defined(TARG_SIZEOF_NEAR_POINTER)
  define_numeric_valued_macro(TARG_SIZEOF_NEAR_POINTER);
#else /* !defined(TARG_SIZEOF_NEAR_POINTER) */
  comment_undefined_macro_name(TARG_SIZEOF_NEAR_POINTER);
#endif /* defined(TARG_SIZEOF_NEAR_POINTER) */
#if defined(TARG_SIZEOF_POINTER)
  define_numeric_valued_macro(TARG_SIZEOF_POINTER);
#else /* !defined(TARG_SIZEOF_POINTER) */
  comment_undefined_macro_name(TARG_SIZEOF_POINTER);
#endif /* defined(TARG_SIZEOF_POINTER) */
#if defined(TARG_SIZEOF_PTR_TO_DATA_MEMBER)
  define_numeric_valued_macro(TARG_SIZEOF_PTR_TO_DATA_MEMBER);
#else /* !defined(TARG_SIZEOF_PTR_TO_DATA_MEMBER) */
  comment_undefined_macro_name(TARG_SIZEOF_PTR_TO_DATA_MEMBER);
#endif /* defined(TARG_SIZEOF_PTR_TO_DATA_MEMBER) */
#if defined(TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION)
  define_numeric_valued_macro(TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION);
#else /* !defined(TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION) */
  comment_undefined_macro_name(TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION);
#endif /* defined(TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION) */
#if defined(TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS)
  define_numeric_valued_macro(TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS);
#else /* !defined(TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS) */
  comment_undefined_macro_name(TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS);
#endif /* defined(TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS) */
#if defined(TARG_SIZEOF_SHORT)
  define_numeric_valued_macro(TARG_SIZEOF_SHORT);
#else /* !defined(TARG_SIZEOF_SHORT) */
  comment_undefined_macro_name(TARG_SIZEOF_SHORT);
#endif /* defined(TARG_SIZEOF_SHORT) */
#if defined(TARG_SIZEOF_SIGNED_ACCUM)
  define_numeric_valued_macro(TARG_SIZEOF_SIGNED_ACCUM);
#else /* !defined(TARG_SIZEOF_SIGNED_ACCUM) */
  comment_undefined_macro_name(TARG_SIZEOF_SIGNED_ACCUM);
#endif /* defined(TARG_SIZEOF_SIGNED_ACCUM) */
#if defined(TARG_SIZEOF_SIGNED_FRACT)
  define_numeric_valued_macro(TARG_SIZEOF_SIGNED_FRACT);
#else /* !defined(TARG_SIZEOF_SIGNED_FRACT) */
  comment_undefined_macro_name(TARG_SIZEOF_SIGNED_FRACT);
#endif /* defined(TARG_SIZEOF_SIGNED_FRACT) */
#if defined(TARG_SIZEOF_SIGNED_LONG_ACCUM)
  define_numeric_valued_macro(TARG_SIZEOF_SIGNED_LONG_ACCUM);
#else /* !defined(TARG_SIZEOF_SIGNED_LONG_ACCUM) */
  comment_undefined_macro_name(TARG_SIZEOF_SIGNED_LONG_ACCUM);
#endif /* defined(TARG_SIZEOF_SIGNED_LONG_ACCUM) */
#if defined(TARG_SIZEOF_SIGNED_LONG_FRACT)
  define_numeric_valued_macro(TARG_SIZEOF_SIGNED_LONG_FRACT);
#else /* !defined(TARG_SIZEOF_SIGNED_LONG_FRACT) */
  comment_undefined_macro_name(TARG_SIZEOF_SIGNED_LONG_FRACT);
#endif /* defined(TARG_SIZEOF_SIGNED_LONG_FRACT) */
#if defined(TARG_SIZEOF_SIGNED_SHORT_ACCUM)
  define_numeric_valued_macro(TARG_SIZEOF_SIGNED_SHORT_ACCUM);
#else /* !defined(TARG_SIZEOF_SIGNED_SHORT_ACCUM) */
  comment_undefined_macro_name(TARG_SIZEOF_SIGNED_SHORT_ACCUM);
#endif /* defined(TARG_SIZEOF_SIGNED_SHORT_ACCUM) */
#if defined(TARG_SIZEOF_SIGNED_SHORT_FRACT)
  define_numeric_valued_macro(TARG_SIZEOF_SIGNED_SHORT_FRACT);
#else /* !defined(TARG_SIZEOF_SIGNED_SHORT_FRACT) */
  comment_undefined_macro_name(TARG_SIZEOF_SIGNED_SHORT_FRACT);
#endif /* defined(TARG_SIZEOF_SIGNED_SHORT_FRACT) */
#if defined(TARG_SIZEOF_UNSIGNED_ACCUM)
  define_numeric_valued_macro(TARG_SIZEOF_UNSIGNED_ACCUM);
#else /* !defined(TARG_SIZEOF_UNSIGNED_ACCUM) */
  comment_undefined_macro_name(TARG_SIZEOF_UNSIGNED_ACCUM);
#endif /* defined(TARG_SIZEOF_UNSIGNED_ACCUM) */
#if defined(TARG_SIZEOF_UNSIGNED_FRACT)
  define_numeric_valued_macro(TARG_SIZEOF_UNSIGNED_FRACT);
#else /* !defined(TARG_SIZEOF_UNSIGNED_FRACT) */
  comment_undefined_macro_name(TARG_SIZEOF_UNSIGNED_FRACT);
#endif /* defined(TARG_SIZEOF_UNSIGNED_FRACT) */
#if defined(TARG_SIZEOF_UNSIGNED_LONG_ACCUM)
  define_numeric_valued_macro(TARG_SIZEOF_UNSIGNED_LONG_ACCUM);
#else /* !defined(TARG_SIZEOF_UNSIGNED_LONG_ACCUM) */
  comment_undefined_macro_name(TARG_SIZEOF_UNSIGNED_LONG_ACCUM);
#endif /* defined(TARG_SIZEOF_UNSIGNED_LONG_ACCUM) */
#if defined(TARG_SIZEOF_UNSIGNED_LONG_FRACT)
  define_numeric_valued_macro(TARG_SIZEOF_UNSIGNED_LONG_FRACT);
#else /* !defined(TARG_SIZEOF_UNSIGNED_LONG_FRACT) */
  comment_undefined_macro_name(TARG_SIZEOF_UNSIGNED_LONG_FRACT);
#endif /* defined(TARG_SIZEOF_UNSIGNED_LONG_FRACT) */
#if defined(TARG_SIZEOF_UNSIGNED_SHORT_ACCUM)
  define_numeric_valued_macro(TARG_SIZEOF_UNSIGNED_SHORT_ACCUM);
#else /* !defined(TARG_SIZEOF_UNSIGNED_SHORT_ACCUM) */
  comment_undefined_macro_name(TARG_SIZEOF_UNSIGNED_SHORT_ACCUM);
#endif /* defined(TARG_SIZEOF_UNSIGNED_SHORT_ACCUM) */
#if defined(TARG_SIZEOF_UNSIGNED_SHORT_FRACT)
  define_numeric_valued_macro(TARG_SIZEOF_UNSIGNED_SHORT_FRACT);
#else /* !defined(TARG_SIZEOF_UNSIGNED_SHORT_FRACT) */
  comment_undefined_macro_name(TARG_SIZEOF_UNSIGNED_SHORT_FRACT);
#endif /* defined(TARG_SIZEOF_UNSIGNED_SHORT_FRACT) */
#if defined(TARG_SIZEOF_VIRTUAL_FUNCTION_INFO)
  define_numeric_valued_macro(TARG_SIZEOF_VIRTUAL_FUNCTION_INFO);
#else /* !defined(TARG_SIZEOF_VIRTUAL_FUNCTION_INFO) */
  comment_undefined_macro_name(TARG_SIZEOF_VIRTUAL_FUNCTION_INFO);
#endif /* defined(TARG_SIZEOF_VIRTUAL_FUNCTION_INFO) */
#if defined(TARG_SIZE_T_INT_KIND)
  define_string_valued_macro(TARG_SIZE_T_INT_KIND);
#else /* !defined(TARG_SIZE_T_INT_KIND) */
  comment_undefined_macro_name(TARG_SIZE_T_INT_KIND);
#endif /* defined(TARG_SIZE_T_INT_KIND) */
#if defined(TARG_SIZE_T_MAX)
  define_string_valued_macro(TARG_SIZE_T_MAX);
#else /* !defined(TARG_SIZE_T_MAX) */
  comment_undefined_macro_name(TARG_SIZE_T_MAX);
#endif /* defined(TARG_SIZE_T_MAX) */
#if defined(TARG_SSIZE_T_INT_KIND)
  define_string_valued_macro(TARG_SSIZE_T_INT_KIND);
#else /* !defined(TARG_SSIZE_T_INT_KIND) */
  comment_undefined_macro_name(TARG_SSIZE_T_INT_KIND);
#endif /* defined(TARG_SSIZE_T_INT_KIND) */
#if defined(TARG_SUPPORTS_ARM32)
  define_numeric_valued_macro(TARG_SUPPORTS_ARM32);
#else /* !defined(TARG_SUPPORTS_ARM32) */
  comment_undefined_macro_name(TARG_SUPPORTS_ARM32);
#endif /* defined(TARG_SUPPORTS_ARM32) */
#if defined(TARG_SUPPORTS_ARM64)
  define_numeric_valued_macro(TARG_SUPPORTS_ARM64);
#else /* !defined(TARG_SUPPORTS_ARM64) */
  comment_undefined_macro_name(TARG_SUPPORTS_ARM64);
#endif /* defined(TARG_SUPPORTS_ARM64) */
#if defined(TARG_SUPPORTS_RISCV32)
  define_numeric_valued_macro(TARG_SUPPORTS_RISCV32);
#else /* !defined(TARG_SUPPORTS_RISCV32) */
  comment_undefined_macro_name(TARG_SUPPORTS_RISCV32);
#endif /* defined(TARG_SUPPORTS_RISCV32) */
#if defined(TARG_SUPPORTS_RISCV64)
  define_numeric_valued_macro(TARG_SUPPORTS_RISCV64);
#else /* !defined(TARG_SUPPORTS_RISCV64) */
  comment_undefined_macro_name(TARG_SUPPORTS_RISCV64);
#endif /* defined(TARG_SUPPORTS_RISCV64) */
#if defined(TARG_SUPPORTS_X86_64)
  define_numeric_valued_macro(TARG_SUPPORTS_X86_64);
#else /* !defined(TARG_SUPPORTS_X86_64) */
  comment_undefined_macro_name(TARG_SUPPORTS_X86_64);
#endif /* defined(TARG_SUPPORTS_X86_64) */
#if defined(TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE)
  define_numeric_valued_macro(TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE);
#else /* !defined(TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE) */
  comment_undefined_macro_name(
                              TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE);
#endif /* defined(TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE) */
#if defined(TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT)
  define_numeric_valued_macro(TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT);
#else /* !defined(TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT) */
  comment_undefined_macro_name(
                              TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT);
#endif /* defined(TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT) */
#if defined(TARG_UNWIND_WORD_MODE)
  define_string_valued_macro(TARG_UNWIND_WORD_MODE);
#else /* !defined(TARG_UNWIND_WORD_MODE) */
  comment_undefined_macro_name(TARG_UNWIND_WORD_MODE);
#endif /* defined(TARG_UNWIND_WORD_MODE) */
#if defined(TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES)
  define_numeric_valued_macro(
                     TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES);
#else /* !defined(TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES) */
  comment_undefined_macro_name(
                     TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES);
#endif /* defined(TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES) */
#if defined(TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS)
  define_numeric_valued_macro(
                       TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS);
#else /* !defined(TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS) */
  comment_undefined_macro_name(
                       TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS);
#endif /* defined(TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS) */
#if defined(TARG_VAR_HANDLE_INT_KIND)
  define_string_valued_macro(TARG_VAR_HANDLE_INT_KIND);
#else /* !defined(TARG_VAR_HANDLE_INT_KIND) */
  comment_undefined_macro_name(TARG_VAR_HANDLE_INT_KIND);
#endif /* defined(TARG_VAR_HANDLE_INT_KIND) */
#if defined(TARG_VERT_TAB_CHAR)
  define_string_valued_macro(TARG_VERT_TAB_CHAR);
#else /* !defined(TARG_VERT_TAB_CHAR) */
  comment_undefined_macro_name(TARG_VERT_TAB_CHAR);
#endif /* defined(TARG_VERT_TAB_CHAR) */
#if defined(TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND)
  define_string_valued_macro(TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND);
#else /* !defined(TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND) */
  comment_undefined_macro_name(TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND);
#endif /* defined(TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND) */
#if defined(TARG_WCHAR_T_INT_KIND)
  define_string_valued_macro(TARG_WCHAR_T_INT_KIND);
#else /* !defined(TARG_WCHAR_T_INT_KIND) */
  comment_undefined_macro_name(TARG_WCHAR_T_INT_KIND);
#endif /* defined(TARG_WCHAR_T_INT_KIND) */
#if defined(TARG_WINT_T_INT_KIND)
  define_string_valued_macro(TARG_WINT_T_INT_KIND);
#else /* !defined(TARG_WINT_T_INT_KIND) */
  comment_undefined_macro_name(TARG_WINT_T_INT_KIND);
#endif /* defined(TARG_WINT_T_INT_KIND) */
#if defined(TARG_WORD_MODE)
  define_string_valued_macro(TARG_WORD_MODE);
#else /* !defined(TARG_WORD_MODE) */
  comment_undefined_macro_name(TARG_WORD_MODE);
#endif /* defined(TARG_WORD_MODE) */
#if defined(TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT)
  define_numeric_valued_macro(
                           TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT);
#else /* !defined(TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT) */
  comment_undefined_macro_name(
                           TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT);
#endif /* defined(TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT) */
#if defined(TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT)
  define_numeric_valued_macro(TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT);
#else /* !defined(TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT) */
  comment_undefined_macro_name(TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT);
#endif /* defined(TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT) */
#if defined(TEMPLATE_INFO_FILE_SUFFIX)
  define_string_valued_macro(TEMPLATE_INFO_FILE_SUFFIX);
#else /* !defined(TEMPLATE_INFO_FILE_SUFFIX) */
  comment_undefined_macro_name(TEMPLATE_INFO_FILE_SUFFIX);
#endif /* defined(TEMPLATE_INFO_FILE_SUFFIX) */
#if defined(TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE)
  define_numeric_valued_macro(TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE);
#else /* !defined(TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE) */
  comment_undefined_macro_name(TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE);
#endif /* defined(TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE) */
#if defined(TEMPORARILY_EXTEND_USE_OF_SSI_CP_GEN_BE)
  define_numeric_valued_macro(TEMPORARILY_EXTEND_USE_OF_SSI_CP_GEN_BE);
#else /* !defined(TEMPORARILY_EXTEND_USE_OF_SSI_CP_GEN_BE) */
  comment_undefined_macro_name(TEMPORARILY_EXTEND_USE_OF_SSI_CP_GEN_BE);
#endif /* defined(TEMPORARILY_EXTEND_USE_OF_SSI_CP_GEN_BE) */
#if defined(THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED)
  define_numeric_valued_macro(THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED);
#else /* !defined(THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED) */
  comment_undefined_macro_name(THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED);
#endif /* defined(THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED) */
#if defined(TIE_DEFAULT_GNU_ABI_VERSION_TO_GNU_VERSION)
  define_numeric_valued_macro(TIE_DEFAULT_GNU_ABI_VERSION_TO_GNU_VERSION);
#else /* !defined(TIE_DEFAULT_GNU_ABI_VERSION_TO_GNU_VERSION) */
  comment_undefined_macro_name(TIE_DEFAULT_GNU_ABI_VERSION_TO_GNU_VERSION);
#endif /* defined(TIE_DEFAULT_GNU_ABI_VERSION_TO_GNU_VERSION) */
#if defined(TRACK_INTERPRETER_ALLOCATIONS)
  define_numeric_valued_macro(TRACK_INTERPRETER_ALLOCATIONS);
#else /* !defined(TRACK_INTERPRETER_ALLOCATIONS) */
  comment_undefined_macro_name(TRACK_INTERPRETER_ALLOCATIONS);
#endif /* defined(TRACK_INTERPRETER_ALLOCATIONS) */
#if defined(TYPE_FOR_AN_FP_VALUE_PART)
  define_string_valued_macro(TYPE_FOR_AN_FP_VALUE_PART);
#else /* !defined(TYPE_FOR_AN_FP_VALUE_PART) */
  comment_undefined_macro_name(TYPE_FOR_AN_FP_VALUE_PART);
#endif /* defined(TYPE_FOR_AN_FP_VALUE_PART) */
#if defined(TYPE_FOR_AN_INTEGER_VALUE)
  define_string_valued_macro(TYPE_FOR_AN_INTEGER_VALUE);
#else /* !defined(TYPE_FOR_AN_INTEGER_VALUE) */
  comment_undefined_macro_name(TYPE_FOR_AN_INTEGER_VALUE);
#endif /* defined(TYPE_FOR_AN_INTEGER_VALUE) */
#if defined(TYPE_FOR_A_FIXED_POINT_VALUE)
  define_string_valued_macro(TYPE_FOR_A_FIXED_POINT_VALUE);
#else /* !defined(TYPE_FOR_A_FIXED_POINT_VALUE) */
  comment_undefined_macro_name(TYPE_FOR_A_FIXED_POINT_VALUE);
#endif /* defined(TYPE_FOR_A_FIXED_POINT_VALUE) */
#if defined(TYPE_FOR_A_SIGNED_INTEGER_VALUE)
  define_string_valued_macro(TYPE_FOR_A_SIGNED_INTEGER_VALUE);
#else /* !defined(TYPE_FOR_A_SIGNED_INTEGER_VALUE) */
  comment_undefined_macro_name(TYPE_FOR_A_SIGNED_INTEGER_VALUE);
#endif /* defined(TYPE_FOR_A_SIGNED_INTEGER_VALUE) */
#if defined(TYPE_FOR_PREFIX_ENTRY_NUMBER)
  define_string_valued_macro(TYPE_FOR_PREFIX_ENTRY_NUMBER);
#else /* !defined(TYPE_FOR_PREFIX_ENTRY_NUMBER) */
  comment_undefined_macro_name(TYPE_FOR_PREFIX_ENTRY_NUMBER);
#endif /* defined(TYPE_FOR_PREFIX_ENTRY_NUMBER) */
#if defined(TYPE_FOR_TARG_ALIGNMENT)
  define_string_valued_macro(TYPE_FOR_TARG_ALIGNMENT);
#else /* !defined(TYPE_FOR_TARG_ALIGNMENT) */
  comment_undefined_macro_name(TYPE_FOR_TARG_ALIGNMENT);
#endif /* defined(TYPE_FOR_TARG_ALIGNMENT) */
#if defined(UCN_ESCAPE_REWRITE_CHAR)
  define_string_valued_macro(UCN_ESCAPE_REWRITE_CHAR);
#else /* !defined(UCN_ESCAPE_REWRITE_CHAR) */
  comment_undefined_macro_name(UCN_ESCAPE_REWRITE_CHAR);
#endif /* defined(UCN_ESCAPE_REWRITE_CHAR) */
#if defined(UNICODE_SOURCE_SUPPORTED)
  define_numeric_valued_macro(UNICODE_SOURCE_SUPPORTED);
#else /* !defined(UNICODE_SOURCE_SUPPORTED) */
  comment_undefined_macro_name(UNICODE_SOURCE_SUPPORTED);
#endif /* defined(UNICODE_SOURCE_SUPPORTED) */
#if defined(UNICODE_VULNERABILITY_DETECTION_SUPPORTED)
  define_numeric_valued_macro(UNICODE_VULNERABILITY_DETECTION_SUPPORTED);
#else /* !defined(UNICODE_VULNERABILITY_DETECTION_SUPPORTED) */
  comment_undefined_macro_name(UNICODE_VULNERABILITY_DETECTION_SUPPORTED);
#endif /* defined(UNICODE_VULNERABILITY_DETECTION_SUPPORTED) */
#if defined(UNIQUE_FILE_IDENTIFIER_AVAILABLE)
  define_numeric_valued_macro(UNIQUE_FILE_IDENTIFIER_AVAILABLE);
#else /* !defined(UNIQUE_FILE_IDENTIFIER_AVAILABLE) */
  comment_undefined_macro_name(UNIQUE_FILE_IDENTIFIER_AVAILABLE);
#endif /* defined(UNIQUE_FILE_IDENTIFIER_AVAILABLE) */
#if defined(UPC_EXTENSIONS_ALLOWED)
  define_numeric_valued_macro(UPC_EXTENSIONS_ALLOWED);
#else /* !defined(UPC_EXTENSIONS_ALLOWED) */
  comment_undefined_macro_name(UPC_EXTENSIONS_ALLOWED);
#endif /* defined(UPC_EXTENSIONS_ALLOWED) */
#if defined(USE_BOOL_FOR_BOOLEAN_IN_CPLUSPLUS)
  define_numeric_valued_macro(USE_BOOL_FOR_BOOLEAN_IN_CPLUSPLUS);
#else /* !defined(USE_BOOL_FOR_BOOLEAN_IN_CPLUSPLUS) */
  comment_undefined_macro_name(USE_BOOL_FOR_BOOLEAN_IN_CPLUSPLUS);
#endif /* defined(USE_BOOL_FOR_BOOLEAN_IN_CPLUSPLUS) */
#if defined(USE_CCTOR_TO_PASS_CLASS_TO_ELLIPSIS)
  define_numeric_valued_macro(USE_CCTOR_TO_PASS_CLASS_TO_ELLIPSIS);
#else /* !defined(USE_CCTOR_TO_PASS_CLASS_TO_ELLIPSIS) */
  comment_undefined_macro_name(USE_CCTOR_TO_PASS_CLASS_TO_ELLIPSIS);
#endif /* defined(USE_CCTOR_TO_PASS_CLASS_TO_ELLIPSIS) */
#if defined(USE_DOUBLE_FOR_HOST_FP_VALUE)
  define_numeric_valued_macro(USE_DOUBLE_FOR_HOST_FP_VALUE);
#else /* !defined(USE_DOUBLE_FOR_HOST_FP_VALUE) */
  comment_undefined_macro_name(USE_DOUBLE_FOR_HOST_FP_VALUE);
#endif /* defined(USE_DOUBLE_FOR_HOST_FP_VALUE) */
#if defined(USE_EDG_NAMESPACE)
  define_numeric_valued_macro(USE_EDG_NAMESPACE);
#else /* !defined(USE_EDG_NAMESPACE) */
  comment_undefined_macro_name(USE_EDG_NAMESPACE);
#endif /* defined(USE_EDG_NAMESPACE) */
#if defined(USE_EMPTY_STRUCT_IN_GENERATED_C)
  define_numeric_valued_macro(USE_EMPTY_STRUCT_IN_GENERATED_C);
#else /* !defined(USE_EMPTY_STRUCT_IN_GENERATED_C) */
  comment_undefined_macro_name(USE_EMPTY_STRUCT_IN_GENERATED_C);
#endif /* defined(USE_EMPTY_STRUCT_IN_GENERATED_C) */
#if defined(USE_ENUMS_IN_BITFIELDS)
  define_numeric_valued_macro(USE_ENUMS_IN_BITFIELDS);
#else /* !defined(USE_ENUMS_IN_BITFIELDS) */
  comment_undefined_macro_name(USE_ENUMS_IN_BITFIELDS);
#endif /* defined(USE_ENUMS_IN_BITFIELDS) */
#if defined(USE_FIXED_ADDRESS_FOR_MMAP)
  define_numeric_valued_macro(USE_FIXED_ADDRESS_FOR_MMAP);
#else /* !defined(USE_FIXED_ADDRESS_FOR_MMAP) */
  comment_undefined_macro_name(USE_FIXED_ADDRESS_FOR_MMAP);
#endif /* defined(USE_FIXED_ADDRESS_FOR_MMAP) */
#if defined(USE_FLOAT128_FOR_HOST_FP_VALUE)
  define_numeric_valued_macro(USE_FLOAT128_FOR_HOST_FP_VALUE);
#else /* !defined(USE_FLOAT128_FOR_HOST_FP_VALUE) */
  comment_undefined_macro_name(USE_FLOAT128_FOR_HOST_FP_VALUE);
#endif /* defined(USE_FLOAT128_FOR_HOST_FP_VALUE) */
#if defined(USE_HEX_FP_CONSTANTS_IN_GENERATED_CODE)
  define_numeric_valued_macro(USE_HEX_FP_CONSTANTS_IN_GENERATED_CODE);
#else /* !defined(USE_HEX_FP_CONSTANTS_IN_GENERATED_CODE) */
  comment_undefined_macro_name(USE_HEX_FP_CONSTANTS_IN_GENERATED_CODE);
#endif /* defined(USE_HEX_FP_CONSTANTS_IN_GENERATED_CODE) */
#if defined(USE_HOST_FP_CONVERSION_ROUTINES)
  define_numeric_valued_macro(USE_HOST_FP_CONVERSION_ROUTINES);
#else /* !defined(USE_HOST_FP_CONVERSION_ROUTINES) */
  comment_undefined_macro_name(USE_HOST_FP_CONVERSION_ROUTINES);
#endif /* defined(USE_HOST_FP_CONVERSION_ROUTINES) */
#if defined(USE_HOST_TMPFILE_FACILITIES)
  define_numeric_valued_macro(USE_HOST_TMPFILE_FACILITIES);
#else /* !defined(USE_HOST_TMPFILE_FACILITIES) */
  comment_undefined_macro_name(USE_HOST_TMPFILE_FACILITIES);
#endif /* defined(USE_HOST_TMPFILE_FACILITIES) */
#if defined(USE_INIT_SECTION_IN_GENERATED_C)
  define_numeric_valued_macro(USE_INIT_SECTION_IN_GENERATED_C);
#else /* !defined(USE_INIT_SECTION_IN_GENERATED_C) */
  comment_undefined_macro_name(USE_INIT_SECTION_IN_GENERATED_C);
#endif /* defined(USE_INIT_SECTION_IN_GENERATED_C) */
#if defined(USE_INLINE_EXPANSION)
  define_numeric_valued_macro(USE_INLINE_EXPANSION);
#else /* !defined(USE_INLINE_EXPANSION) */
  comment_undefined_macro_name(USE_INLINE_EXPANSION);
#endif /* defined(USE_INLINE_EXPANSION) */
#if defined(USE_INLINING_HINTS)
  define_numeric_valued_macro(USE_INLINING_HINTS);
#else /* !defined(USE_INLINING_HINTS) */
  comment_undefined_macro_name(USE_INLINING_HINTS);
#endif /* defined(USE_INLINING_HINTS) */
#if defined(USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES)
  define_numeric_valued_macro(
                           USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES);
#else /* !defined(USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES) */
  comment_undefined_macro_name(
                           USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES);
#endif /* defined(USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES) */
#if defined(USE_LONG_DOUBLE_FOR_HOST_FP_VALUE)
  define_numeric_valued_macro(USE_LONG_DOUBLE_FOR_HOST_FP_VALUE);
#else /* !defined(USE_LONG_DOUBLE_FOR_HOST_FP_VALUE) */
  comment_undefined_macro_name(USE_LONG_DOUBLE_FOR_HOST_FP_VALUE);
#endif /* defined(USE_LONG_DOUBLE_FOR_HOST_FP_VALUE) */
#if defined(USE_MMAP_FOR_MEMORY_REGIONS)
  define_numeric_valued_macro(USE_MMAP_FOR_MEMORY_REGIONS);
#else /* !defined(USE_MMAP_FOR_MEMORY_REGIONS) */
  comment_undefined_macro_name(USE_MMAP_FOR_MEMORY_REGIONS);
#endif /* defined(USE_MMAP_FOR_MEMORY_REGIONS) */
#if defined(USE_MMAP_FOR_MODULES)
  define_numeric_valued_macro(USE_MMAP_FOR_MODULES);
#else /* !defined(USE_MMAP_FOR_MODULES) */
  comment_undefined_macro_name(USE_MMAP_FOR_MODULES);
#endif /* defined(USE_MMAP_FOR_MODULES) */
#if defined(USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING)
  define_numeric_valued_macro(USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING);
#else /* !defined(USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING) */
  comment_undefined_macro_name(USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING);
#endif /* defined(USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING) */
#if defined(USE_PATCH_INIT_STARTUP)
  define_numeric_valued_macro(USE_PATCH_INIT_STARTUP);
#else /* !defined(USE_PATCH_INIT_STARTUP) */
  comment_undefined_macro_name(USE_PATCH_INIT_STARTUP);
#endif /* defined(USE_PATCH_INIT_STARTUP) */
#if defined(USE_QUADMATH_LIBRARY)
  define_numeric_valued_macro(USE_QUADMATH_LIBRARY);
#else /* !defined(USE_QUADMATH_LIBRARY) */
  comment_undefined_macro_name(USE_QUADMATH_LIBRARY);
#endif /* defined(USE_QUADMATH_LIBRARY) */
#if defined(USE_SOFTFLOAT)
  define_numeric_valued_macro(USE_SOFTFLOAT);
#else /* !defined(USE_SOFTFLOAT) */
  comment_undefined_macro_name(USE_SOFTFLOAT);
#endif /* defined(USE_SOFTFLOAT) */
#if defined(USE_TEMPLATE_INFO_FILE)
  define_numeric_valued_macro(USE_TEMPLATE_INFO_FILE);
#else /* !defined(USE_TEMPLATE_INFO_FILE) */
  comment_undefined_macro_name(USE_TEMPLATE_INFO_FILE);
#endif /* defined(USE_TEMPLATE_INFO_FILE) */
#if defined(USE_VIRTUAL_FUNCTIONS)
  define_numeric_valued_macro(USE_VIRTUAL_FUNCTIONS);
#else /* !defined(USE_VIRTUAL_FUNCTIONS) */
  comment_undefined_macro_name(USE_VIRTUAL_FUNCTIONS);
#endif /* defined(USE_VIRTUAL_FUNCTIONS) */
#if defined(USE_X86_FUNCTION_MULTIVERSIONING)
  define_numeric_valued_macro(USE_X86_FUNCTION_MULTIVERSIONING);
#else /* !defined(USE_X86_FUNCTION_MULTIVERSIONING) */
  comment_undefined_macro_name(USE_X86_FUNCTION_MULTIVERSIONING);
#endif /* defined(USE_X86_FUNCTION_MULTIVERSIONING) */
#if defined(USING_DECLARATIONS_IN_GENERATED_CODE)
  define_numeric_valued_macro(USING_DECLARATIONS_IN_GENERATED_CODE);
#else /* !defined(USING_DECLARATIONS_IN_GENERATED_CODE) */
  comment_undefined_macro_name(USING_DECLARATIONS_IN_GENERATED_CODE);
#endif /* defined(USING_DECLARATIONS_IN_GENERATED_CODE) */
#if defined(USING_DRIVER)
  define_numeric_valued_macro(USING_DRIVER);
#else /* !defined(USING_DRIVER) */
  comment_undefined_macro_name(USING_DRIVER);
#endif /* defined(USING_DRIVER) */
#if defined(USING_KAI_INLINER)
  define_numeric_valued_macro(USING_KAI_INLINER);
#else /* !defined(USING_KAI_INLINER) */
  comment_undefined_macro_name(USING_KAI_INLINER);
#endif /* defined(USING_KAI_INLINER) */
#if defined(VERSION_NUMBER)
  define_string_valued_macro(VERSION_NUMBER);
#else /* !defined(VERSION_NUMBER) */
  comment_undefined_macro_name(VERSION_NUMBER);
#endif /* defined(VERSION_NUMBER) */
#if defined(VERSION_NUMBER_FOR_MACRO)
  define_numeric_valued_macro(VERSION_NUMBER_FOR_MACRO);
#else /* !defined(VERSION_NUMBER_FOR_MACRO) */
  comment_undefined_macro_name(VERSION_NUMBER_FOR_MACRO);
#endif /* defined(VERSION_NUMBER_FOR_MACRO) */
#if defined(VLA_ALLOWED)
  define_numeric_valued_macro(VLA_ALLOWED);
#else /* !defined(VLA_ALLOWED) */
  comment_undefined_macro_name(VLA_ALLOWED);
#endif /* defined(VLA_ALLOWED) */
#if defined(VLA_DEALLOCATIONS_IN_IL)
  define_numeric_valued_macro(VLA_DEALLOCATIONS_IN_IL);
#else /* !defined(VLA_DEALLOCATIONS_IN_IL) */
  comment_undefined_macro_name(VLA_DEALLOCATIONS_IN_IL);
#endif /* defined(VLA_DEALLOCATIONS_IN_IL) */
#if defined(VLA_DEALLOCATION_REQUIRED)
  define_numeric_valued_macro(VLA_DEALLOCATION_REQUIRED);
#else /* !defined(VLA_DEALLOCATION_REQUIRED) */
  comment_undefined_macro_name(VLA_DEALLOCATION_REQUIRED);
#endif /* defined(VLA_DEALLOCATION_REQUIRED) */
#if defined(WCHAR_T_ENABLING_POSSIBLE)
  define_numeric_valued_macro(WCHAR_T_ENABLING_POSSIBLE);
#else /* !defined(WCHAR_T_ENABLING_POSSIBLE) */
  comment_undefined_macro_name(WCHAR_T_ENABLING_POSSIBLE);
#endif /* defined(WCHAR_T_ENABLING_POSSIBLE) */
#if defined(WINDOWS_PATHS_ALLOWED)
  define_numeric_valued_macro(WINDOWS_PATHS_ALLOWED);
#else /* !defined(WINDOWS_PATHS_ALLOWED) */
  comment_undefined_macro_name(WINDOWS_PATHS_ALLOWED);
#endif /* defined(WINDOWS_PATHS_ALLOWED) */
#if defined(WRITE_CPPCLI_PORTABLE_ASSEMBLIES)
  define_numeric_valued_macro(WRITE_CPPCLI_PORTABLE_ASSEMBLIES);
#else /* !defined(WRITE_CPPCLI_PORTABLE_ASSEMBLIES) */
  comment_undefined_macro_name(WRITE_CPPCLI_PORTABLE_ASSEMBLIES);
#endif /* defined(WRITE_CPPCLI_PORTABLE_ASSEMBLIES) */
#if defined(WRITE_SIGNOFF_MESSAGE)
  define_numeric_valued_macro(WRITE_SIGNOFF_MESSAGE);
#else /* !defined(WRITE_SIGNOFF_MESSAGE) */
  comment_undefined_macro_name(WRITE_SIGNOFF_MESSAGE);
#endif /* defined(WRITE_SIGNOFF_MESSAGE) */
/* Undefine local macros. */
#undef comment_undefined_macro_name
#undef comment_numeric_valued_macro /*lint !e750*/
#undef define_numeric_valued_macro
#undef comment_string_valued_macro
#undef define_string_valued_macro
  /* Dump target configurations (if any). */
  dump_target_configurations();
}  /* dump_configuration_macros */
#endif /* DUMP_CONFIG_ENABLED */


static void dump_command_options(void)
/*
Output all of the command-line options that can be specified using the
--name style.  This is output to stdout to make it simple to use with
things like the bash "complete" command.  The "--" is included to simplify
its use in such cases.
*/
{
  an_option_description_ptr odp = NULL;
  int                       n;

  for (n = 0; n < option_descriptions_used; ++n) {
    odp = &option_descriptions[n];
    if (odp->keyword) {
      printf("--%s ", odp->keyword);
    }  /* if */
  }  /* for */
}  /* dump_command_options */

STATIC_THREAD a_boolean
                C_dialect_has_been_set;
                        /* Flag that is TRUE if the C_dialect has been
                           set (either implicitly or explicitly) via a
                           command line option. */

static void set_C_dialect(a_C_dialect dialect)
/*
Set the C_dialect to dialect and issue a command line error if the
C_dialect has (implicitly or explicitly) been previously set to
an incompatible value.  For the purposes of this check, treat the two
"C" dialects as the same (i.e., look only for changes between C and C++).
*/
{
  if (C_dialect_has_been_set &&
      ((C_dialect == C_dialect_cplusplus) !=
       (dialect == C_dialect_cplusplus))) {
    command_line_error(ec_cl_incompatible_language_modes);
  }  /* if */
  C_dialect = dialect;
  C_dialect_has_been_set = TRUE;
}  /* set_C_dialect */


void proc_command_line(int argc, char *argv[])
/*
Process the arguments on the command line that invoked the compiler.
*/
{
  an_option_description_ptr	odp;
  a_const_char		        *ofile_name = NULL;
  a_boolean			source_file_name_optional = FALSE;
  a_const_char			*instantiation_mode_string = NULL;
  a_directory_name_entry_ptr	include_path_boundary = NULL;
#if !USE_MMAP_FOR_MEMORY_REGIONS
  a_boolean			non_pch_option_used = FALSE;
#endif /* !USE_MMAP_FOR_MEMORY_REGIONS */
  a_boolean                     suppress_do_preprocessing_only = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  int                           i;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_const_char			*error_file_name = NULL;
  a_const_char			*xref_file_name = NULL;
  a_const_char			*listing_file_name = NULL;

  /* Set a current position indicating we are looking at the command line. */
  set_position_to(pos_curr_token, 0, SP_COL_CMD_LINE);
  set_err_pos_to_curr_token();

  /* Initialize the table of option descriptions used by the options
     processing routine. */
  initialize_option_descriptions();
  initialize_command_line_variables();
#ifdef HOSTID
  /* Check that this code is running on an acceptable CPU. */
  /* Use an exclusive-or to make it harder to find and patch the host id
     in the compiler executable. */
#define HOSTID_MASK 0x08251954
  { long lhostid = gethostid() ^ HOSTID_MASK;
    if (lhostid != (HOSTID ^ HOSTID_MASK) && 
#ifdef HOSTID2
        lhostid != (HOSTID2 ^ HOSTID_MASK) &&
#endif /* ifdef HOSTID2 */
                                             ) {
      command_line_error(ec_cl_incorrect_host_id);
    }  /* if */
  }
#endif /* ifdef HOSTID */

  /* Some messages have their severity overridden by default.  Set those
     values now. */
  set_default_message_severities();
  /* Scan the command-line options. */
  while ((odp = get_option(argc, argv)) != NULL) {
    an_option_kind	kind = odp->kind;
    a_boolean		opt_value = odp->value;
    /* Record information about this option for precompiled header
       processing. */
    if (odp->pch_event_kind != pchek_none) {
      add_command_line_pch_event(odp->pch_event_kind, kind, opt_value,
                                 opt_arg);
    }  /* if */
#if !USE_MMAP_FOR_MEMORY_REGIONS
    {
      a_boolean		is_pch_option;
      /* See if this is a PCH option. */
      switch (kind) {
        case optk_create_pch:
        case optk_use_pch:
        case optk_pch:
        case optk_pch_messages:
        case optk_pch_verbose:
        case optk_pch_mem:
        case optk_pch_dir:
          is_pch_option = TRUE;
          break;
        default:
          is_pch_option = FALSE;
         break;
      }  /* switch */
      /* When using preallocated memory for PCH processing, the PCH options
         must be first on the command line. */
      if (non_pch_option_used && is_pch_option) {
        /* The PCH options must precede all other options. */
        command_line_error(ec_cl_pch_must_be_first);
      }  /* if */
      if (!non_pch_option_used && !is_pch_option) {
        /* When we have processed all of the PCH options, do the preallocation
           of the PCH memory. */
        if (precompiled_header_processing_required) {
          preallocate_pch_memory();
        }  /* if */
        non_pch_option_used = TRUE;
      }  /* if */
    }
#endif /* !USE_MMAP_FOR_MEMORY_REGIONS */
    switch (kind) {
      case optk_strict_ansi_error:
      case optk_strict_ansi_warning:
        /* Warn on non-ANSI features, disable features that conflict
           with ANSI.  Note that "ANSI" means ANSI C or ANSI C++, depending
           on the C_dialect setting. */
        check_assertion(opt_value == TRUE);
        strict_ansi_mode = TRUE;
        if (kind == optk_strict_ansi_error) {
          strict_ansi_error_severity = es_error;
          strict_ansi_discretionary_severity = es_discretionary_error;
        } else {
          strict_ansi_error_severity = es_warning;
          strict_ansi_discretionary_severity = es_warning;
        }  /* if */
        /* Make sure that strict ANSI messages come out even if the
           error threshold was set at a higher level.  Note that this may
           be changed by a later option that modifies the error threshold. */
        if ((int)error_threshold > (int)strict_ansi_error_severity) {
          error_threshold = strict_ansi_error_severity;
        }  /* if */
        break;
      case optk_preprocess_only_emit_line_dirs:
        /* Do preprocessing only, output to stdout, with #line information. */
        check_assertion(opt_value == TRUE);
        do_preprocessing_only = TRUE;
        generate_pp_output = TRUE;
        gen_line_info_in_pp_output = TRUE;
        break;
      case optk_preprocess_only_no_line_dirs:
        /* Do preprocessing only, output to stdout (driver remaps to .i file),
           without #line information. */
        check_assertion(opt_value == TRUE);
        do_preprocessing_only = TRUE;
        generate_pp_output = TRUE;
        gen_line_info_in_pp_output = FALSE;
        break;
      case optk_keep_comments_in_pp_output:
        /* Keep comments in preprocessing output. */
        check_assertion(opt_value == TRUE);
        keep_comments_in_pp_output = TRUE;
        break;
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
      case optk_old_line_dirs:
        /* Generate old-style line directives in generated C/C++ output,
           i.e., "# nnn" instead of "#line nnn". */
        check_assertion(opt_value == TRUE);
        gen_old_style_line_dirs = TRUE;
        break;
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
      case optk_C_dialect_pcc:
        /* Compile K&R/pcc dialect of C. */
        check_assertion(opt_value == TRUE);
        set_C_dialect(C_dialect_pcc);
        break;
      case optk_list_makefile_dependencies:
        /* Generate makefile dependency lines for #include files encountered,
           but do not compile. */
        check_assertion(opt_value == TRUE);
        do_preprocessing_only = TRUE;
        generate_pp_output = FALSE;
        list_included_files = FALSE;
        list_makefile_dependencies = TRUE;
        break;
      case optk_list_include_files:
        /* Generate on stdout a list of the names of the #include files
           processed. */
        check_assertion(opt_value == TRUE);
        list_included_files = TRUE;
        list_makefile_dependencies = FALSE;
        break;
#if DO_IL_LOWERING
      case optk_write_unlowered_il:
	/* Suppress IL lowering and write an unlowered IL file. */
        check_assertion(opt_value == TRUE);
	suppress_il_lowering = TRUE;
        suppress_back_end = TRUE;
        /* Note that suppress_il_file_write is not set. */
	break;
#endif /* DO_IL_LOWERING */
#if NEED_IL_DISPLAY
      case optk_il_display:
        /* Display IL before back end processing. */
        check_assertion(opt_value == TRUE);
        il_display = TRUE;
#if IL_SHOULD_BE_WRITTEN_TO_FILE
        skip_il_read = TRUE;
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
        break;
#endif /* NEED_IL_DISPLAY */
      case optk_cplusplus_anachronisms:
        /* Enable or disable acceptance of anachronisms. */
        allow_anachronisms = opt_value;
        break;
      case optk_cfront_2_1_mode:
        /* cfront 2.1 compatibility mode.  If both 2.1 and 3.0 modes are
           selected, only the most recent applies. */
        check_assertion(opt_value == TRUE);
        cfront_2_1_mode = TRUE;
        cfront_3_0_mode = FALSE;
        /* This option implies C++ dialect. */
        set_C_dialect(C_dialect_cplusplus);
        break;
      case optk_cfront_3_0_mode:
        check_assertion(opt_value == TRUE);
        /* cfront 3.0 compatibility mode.  If both 2.1 and 3.0 modes are
           selected, only the most recent applies. */
        cfront_3_0_mode = TRUE;
        cfront_2_1_mode = FALSE;
        /* This option implies C++ dialect. */
        set_C_dialect(C_dialect_cplusplus);
        break;
      case optk_front_end_only:
        /* Run just the front end to do syntax checking; do not run the back
           end. */
        check_assertion(opt_value == TRUE);
        suppress_back_end = TRUE;
#if IL_SHOULD_BE_WRITTEN_TO_FILE
        suppress_il_file_write = TRUE;
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
#if DO_IL_LOWERING
	suppress_il_lowering = TRUE;
#endif /* DO_IL_LOWERING */
        break;
      case optk_use_signed_chars:
        /* Use signed or unsigned chars. */
        targ_has_signed_chars = opt_value;
        break;
      case optk_template_instantiation_mode:
        /* Template instantiation mode. */
        instantiation_mode_string = opt_arg;
        /* Determine the template instantiation mode to be used. */
        if (instantiation_mode_string != NULL) {
          if (strcmp(instantiation_mode_string, "none") == 0) {
            instantiation_mode = tim_none;
          } else if (strcmp(instantiation_mode_string, "all") == 0) {
            instantiation_mode = tim_all;
          } else if (strcmp(instantiation_mode_string, "used") == 0) {
            instantiation_mode = tim_used;
          } else if (strcmp(instantiation_mode_string, "local") == 0) {
            instantiation_mode = tim_local;
          } else {
            str_command_line_error(ec_cl_invalid_instantiation_mode,
      			           instantiation_mode_string);
          }  /* if */
        }  /* if */
        break;
#if AUTOMATIC_TEMPLATE_INSTANTIATION
      case optk_automatic_template_instantiation:
        /* Enable or disable automatic instantiation processing. */
        automatic_instantiation_mode = opt_value;
        break;
      case optk_ii_file_name:
        /* The name of the instantiation information file to be used. */
        ii_file_name = file_name_from_opt_arg(opt_arg);
        break;
      case optk_suppress_instantiation_flags:
        /* Enable or disable automatic instantiation processing. */
        suppress_instantiation_flags = opt_value;
        break;
      case optk_template_info_file:
        template_info_file_name = file_name_from_opt_arg(opt_arg);
        break;
      case optk_definition_list_file_name:
        definition_list_file_name = file_name_from_opt_arg(opt_arg);
        break;
      case optk_exported_template_file_name:
        exported_template_file_name = file_name_from_opt_arg(opt_arg);
        break;
      case optk_template_directory:
        /* A directory name to be added to the template search path.*/
        if (!is_directory(opt_arg)) {
          str_command_line_error(ec_cl_invalid_template_directory, opt_arg);
        }  /* if */
        add_to_template_search_path(file_name_from_opt_arg(opt_arg));
        break;
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
      case optk_implicit_template_inclusion:
        /* Enable or disable implicit inclusion of template definition source
           files. */
        implicit_template_inclusion_mode = opt_value;
        break;
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
      case optk_virtual_function_table_definition:
        /* Control generation of a virtual function table if unable to
	   determine absolute means to avoid duplicate virtual function 
	   table entries in separate compilations.  --force_vtbl gives
           opt_value TRUE; --suppress_vtbl gives opt_value FALSE. */
        virtual_function_table_definition =
                                          opt_value ? vfd_force : vfd_suppress;
	break;
      case optk_allow_dollar_in_id_chars:
        /* Determines whether dollar signs are accepted in identifiers. */
        allow_dollar_in_id_chars = opt_value;
        break;
      case optk_display_compilation_time:
        /* Generate compilation timing information. */
        check_assertion(opt_value == TRUE);
        display_compilation_time = TRUE;
        break;
      case optk_display_compiler_version:
        /* Print out compiler version. */
        check_assertion(opt_value == TRUE);
        fprintf(f_error,
                "Edison Design Group C/C++ Front End, version %s (%s %s)\n",
                VERSION_NUMBER, build_date, build_time);
        fprintf(f_error, "Copyright 1988-2026 Edison Design Group, Inc.\n");
#ifdef DEMO_VERSION_ID
        fprintf(f_error, "Demonstration version for %s\n", DEMO_VERSION_ID);
#endif /* ifdef DEMO_VERSION_ID */
        fputc('\n', f_error);
        source_file_name_optional = TRUE;
        break;
      case optk_suppress_warnings:
        /* Suppress warnings. */
        check_assertion(opt_value == TRUE);
        error_threshold = es_discretionary_error;
        break;
      case optk_promote_warnings:
        /* Promote warnings to errors. */
        check_assertion(opt_value == TRUE);
        error_promotion_threshold = es_warning;
        break;
      case optk_enable_remarks:
        /* Enable remarks. */
        check_assertion(opt_value == TRUE);
        error_threshold = es_remark;
        break;
      case optk_C_mode:
        /* Compile C code.  Only has an effect if the current (perhaps default)
           major dialect is C_dialect_cplusplus: In that case, it makes the
           selected dialect C_dialect_ANSI. */
        check_assertion(opt_value == TRUE);
        if (C_dialect == C_dialect_cplusplus) {
          set_C_dialect(C_dialect_ANSI);
        }  /* if */
        break;
      case optk_C_dialect_cplusplus:
        /* Compile C++. */
        check_assertion(opt_value == TRUE);
        set_C_dialect(C_dialect_cplusplus);
        break;
      case optk_exception_handling:
        /* Enables or disables support for exceptions. */
        exceptions_enabled = opt_value;
        break;
      case optk_suppress_used_before_set_warnings:
        /* Suppress used-before-set warnings. */
        check_assertion(opt_value == TRUE);
        suppress_used_before_set_warnings = TRUE;
        break;
      case optk_include_directory:
      case optk_system_include_dir:
        /* Include file directory, add to list. */
        if (strcmp(opt_arg, "-") == 0) {
          /* -I- marks the dividing line between directories for "..."
             includes and those for <...> includes.  It also suppresses
             pushing the directory of each source file onto the search
             path, which is useful for viewpathing. */
          include_path_boundary = end_incl_search_path;
          put_dir_of_each_opened_source_file_on_incl_search_path = FALSE;
        } else {
          /* Normal -I directive. */
          add_to_include_search_path(file_name_from_opt_arg(opt_arg),
                                     kind == optk_system_include_dir);
        }  /* if */
        break;
      case optk_embed_dir:
        /* Add the directory to the search path when searching for #embed
           files. */
        add_to_specified_include_search_path(file_name_from_opt_arg(opt_arg),
                                             /*system_dir=*/FALSE,
                                             &embed_search_path,
                                             &end_embed_search_path);
        break;
      case optk_module_dir:
        /* Add the directory to the search path when searching for module
           files. */
        add_to_specified_include_search_path(file_name_from_opt_arg(opt_arg),
                                             /*system_dir=*/FALSE,
                                             &module_search_path,
                                             &end_module_search_path);
        break;
      case optk_ms_module_file_map:
        { a_const_char *part_1;
          a_const_char *part_2;

          split_opt_arg_on_char(opt_arg, &part_1, &part_2, '=',
                                /*respect_quotes=*/TRUE);
          if (part_2 == NULL) {
            /* Register a module with an unknown name for potential
               consideration. */
            a_const_char *path = file_name_from_opt_arg(part_1);

            lazy_mod_map_arr->push_back(path);
          } else {
            /* Split the parts to get the module name and path. */
            a_string_view  module_name(part_1);
            a_const_char   *path = file_name_from_opt_arg(part_2);

            /* Add this module name to the map. */
            if (module_name.length() == 0) {
              str_command_line_error(ec_cl_unnamed_module_map, part_2);
            } else if (mod_map->get(module_name) != NULL) {
              str_command_line_error(ec_duplicate_module_map, part_1);
            } else {
              mod_map->map(module_name, path);
            }  /* if */
          }  /* if */
        }
        break;
      case optk_map_module_header_unit:
      case optk_ms_header_unit:
        { a_path_handle header_path;
          a_const_char  *module_path;
          split_opt_arg_on_char(opt_arg, &header_path.ptr, &module_path, '=',
                                /*respect_quotes=*/TRUE);
          header_path.ptr = file_name_from_opt_arg(header_path.ptr);
          if (module_path == NULL) {
            str_command_line_error(ec_missing_header_unit_map,
                                   header_path.ptr);
          } else {
            /* Add this header->module mapping. */
            if (header_unit_map->get(header_path) != NULL) {
              str_command_line_error(ec_duplicate_header_unit_map,
                                     header_path.ptr);
            } else {
              header_unit_map->map(header_path,
                                   file_name_from_opt_arg(module_path));
            }  /* if */
          }  /* if */
        }
        break;
      case optk_ms_header_unit_quote:
      case optk_ms_header_unit_angle:
        { a_header_unit_map *header_map = (kind == optk_ms_header_unit_angle) ?
                                 header_unit_angle_map : header_unit_quote_map;
          a_path_handle header_path;
          a_const_char  *module_path;
          split_opt_arg_on_char(opt_arg, &header_path.ptr, &module_path, '=',
                                /*respect_quotes=*/TRUE);
          header_path.ptr = file_name_from_opt_arg(header_path.ptr);
          if (module_path == NULL) {
            str_command_line_error(ec_missing_header_unit_map,
                                   header_path.ptr);
          } else {
            /* Add this header->module mapping. */
            if (header_map->get(header_path) != NULL) {
              str_command_line_error(ec_duplicate_header_unit_map,
                                     header_path.ptr);
            } else {
              header_map->map(header_path,
                              file_name_from_opt_arg(module_path));
            }  /* if */
          }  /* if */
        }
        break;
      case optk_ms_mod_translate_include:
        import_includes_from_header_map = opt_value;
        break;
      case optk_modules:
        /* Enable/disable support for modules. */
        modules_enabled = opt_value;
        if (modules_enabled) {
          if (!option_kind_used[(int)optk_export_template]) {
            /* These are mutually exclusive.  If exported templates was enabled
               via the command line, don't change the value - an error will be
               issued later. */
            export_template_allowed = FALSE;
          }  /* if */
          export_keyword_enabled = FALSE;
        }  /* if */
        break;
      case optk_module_import_diagnostics:
        /* Enable or disable the display of diagnostics issued during module
           import operations. */
        display_module_import_diagnostics = opt_value;
        break;
      case optk_module_interface:
        /* Create a module interface unit file as part of this compilation.
           The file name will be inferred from the module declaration. */
        create_module_unit = TRUE;
        module_unit_output_file_name = NULL;
        break;
      case optk_module_internal_partition:
        /* Create an internal module partition unit file as part of this
           compilation.  The file name will be inferred from the module
           declaration. */
        create_module_unit = TRUE;
        created_module_unit_is_internal = TRUE;
        module_unit_output_file_name = NULL;
        break;
      case optk_preinclude:
      case optk_preinclude_macros:
#if MICROSOFT_EXTENSIONS_ALLOWED
      case optk_preusing:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* File to include at the beginning of compilation. */
        process_preinclude_option(kind, file_name_from_opt_arg(opt_arg));
        break;
      case optk_define_macro:
        /* Define a macro symbol.  Just save the string for later
           processing. */
        add_to_def_undef_list(opt_arg, &defs_from_cmd_line,
                              &last_defs_from_cmd_line, /*is_undef=*/FALSE);
        break;
      case optk_undefine_macro:
        /* Undefine a macro symbol.  Just save the string for later
           processing. */
        add_to_def_undef_list(opt_arg, &defs_from_cmd_line,
                              &last_defs_from_cmd_line, /*is_undef=*/TRUE);
        break;
      case optk_set_error_limit:
        /* Set error limit (numbers of errors at which to give up on
           compilation). */
        scan_opt_arg_number(&error_limit, opt_arg);
        if (error_limit == 0) {
          str_command_line_error(ec_cl_invalid_error_limit, opt_arg);
        }  /* if */
        break;
      case optk_generate_raw_listing:
        /* Generate a file of raw listing information (source lines,
           file/line information, and indications of which lines are which,
           to be read later by a program that will generate an
           interspersed listing). */
        listing_file_name = file_name_from_opt_arg(opt_arg);
        break;
      case optk_generate_cross_reference:
        /* Generate a file of cross-reference information (locations and
	   kinds of references to symbols) */
        xref_file_name = file_name_from_opt_arg(opt_arg);
        break;
      case optk_stderr_file_name:
        /* Redirect error output to a file.  This is useful on systems where
           redirection is not well supported. */
        error_file_name = file_name_from_opt_arg(opt_arg);
        break;
      case optk_output_file_name:
        /* Specify output file for preprocessing output or IL. */
        ofile_name = file_name_from_opt_arg(opt_arg);
        break;
#if BACK_END_IS_C_GEN_BE
      case optk_module_list_for_union_init:
#if !C_GEN_BE_GENERATES_ANSI_C
        /* Save a string of comma-separated module names that will be linked
           with this one.  This is used by c_gen_be to generate calls
           to file-scope initialization routines that handle union
           initialization.  This option is only needed if c_gen_be is being
           used to generate C output for testing, and then only if K&R
           C is being generated. */
        module_list_for_union_init = opt_arg;
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
        break;
#endif /* BACK_END_IS_C_GEN_BE */
#if DEBUG
      case optk_debug:
        /* Set debug level. */
        if (proc_debug_option(opt_arg)) {
	  command_line_error(ec_cl_error_in_debug_option_argument);
	}  /* if */
        init_debug_level = debug_level;
        break;
      case optk_debug_name:
        /* Set debug name (name to be traced). */
        if (proc_debug_name_option(opt_arg)) {
	  command_line_error(ec_cl_error_in_debug_option_argument);
	}  /* if */
        break;
#if MAINTAIN_ALLOCATION_SEQUENCE_NUMBER
      case optk_debug_alloc_seq:
        /* Set debug allocation sequence number to be traced. */
        if (proc_debug_alloc_seq_option(opt_arg)) {
          command_line_error(ec_cl_error_in_debug_option_argument);
        }  /* if */
        break;
#endif /* MAINTAIN_ALLOCATION_SEQUENCE_NUMBER */
#endif /* DEBUG */
#if !EDG_WIN32 && !MULTIPLE_THREAD_COMPILATION
      case optk_time_limit:
        /* Option to limit the amount of CPU time used during a compilation. */
        { int time_limit;
          scan_opt_arg_number(&time_limit, opt_arg);
          set_cpu_time_limit(time_limit);
        }
        break;
#endif /* !EDG_WIN32 && !MULTIPLE_THREAD_COMPILATION */
      case optk_diag_suppress:
      case optk_diag_remark:
      case optk_diag_warning:
      case optk_diag_error:
      case optk_diag_once:
        /* Options that override the severity of a given diagnostic.  The
           option argument contains a comma-separated list of error tags. */
        process_diag_override_option(kind, opt_arg);
        break;
      case optk_constexpr_diag_suppress:
      case optk_constexpr_diag_remark:
      case optk_constexpr_diag_warning:
      case optk_constexpr_diag_error:
        /* Options that override the severity of the diagnostics that calls
           of __builtin_constexpr_diag produce with a given tag.  The option
           argument contains a comma-separated list of tags. */
        process_constexpr_diag_override_option(kind, opt_arg);
        break;
      case optk_display_error_number:
        /* Enable or disable display of error number in diagnostic messages. */
        display_error_number = opt_value;
        break;
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
      case optk_gen_c_file_name:
        /* The name to be used for the generated C file. */
        gen_c_file_name = file_name_from_opt_arg(opt_arg);
        break;
      case optk_msvc_target_version:
        /* The Microsoft C/C++ compiler being targeted. */
        scan_opt_arg_number(&msvc_target_version_number, opt_arg);
        microsoft_dialect_is_generated_code_target = TRUE;
        msvc_is_generated_code_target = MSVC_IS_GENERATED_CODE_TARGET;
        break;
      case optk_gnu_target_version:
        /* The GNU C/C++ compiler being targeted. */
        scan_opt_arg_number(&gnu_target_version_number, opt_arg);
        gcc_is_generated_code_target = TRUE;
        break;
      case optk_clang_target_version:
        /* The clang C/C++ compiler being targeted. */
        scan_opt_arg_number(&clang_target_version_number, opt_arg);
        clang_is_generated_code_target = TRUE;
        break;
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
      case optk_create_pch:
        /* Create precompiled header file as part of this compilation. */
        check_assertion(opt_value == TRUE);
        create_precompiled_header = TRUE;
        precompiled_header_processing_required = TRUE;
        pch_output_file_name = file_name_from_opt_arg(opt_arg);
        automatic_pch_processing = FALSE;
        use_precompiled_header = FALSE;
        /* Make sure the specified name is acceptable as a PCH file name. */
        check_pch_file_name(pch_output_file_name);
        break;
      case optk_use_pch:
        /* Use a precompiled header file as part of this compilation. */
        check_assertion(opt_value == TRUE);
        use_precompiled_header = TRUE;
        pch_input_file_name = file_name_from_opt_arg(opt_arg);
        precompiled_header_processing_required = TRUE;
        automatic_pch_processing = FALSE;
        create_precompiled_header = FALSE;
        /* Make sure the specified name is acceptable as a PCH file name. */
        check_pch_file_name(pch_input_file_name);
        break;
      case optk_pch:
        /* Do automatic precompiled header processing as part of this
           compilation. */
        check_assertion(opt_value == TRUE);
        automatic_pch_processing = TRUE;
        use_precompiled_header = FALSE;
        create_precompiled_header = FALSE;
        precompiled_header_processing_required = TRUE;
        break;
      case optk_pch_messages:
        /* Enable or suppress PCH messages. */
        suppress_pch_messages = !opt_value;
        break;
      case optk_pch_verbose:
        /* Enable or suppress verbose PCH messages. */
        verbose_pch_messages = opt_value;
        break;
#if !USE_MMAP_FOR_MEMORY_REGIONS
      case optk_pch_mem:
        /* Specify the size of the preallocated memory to be used for
           PCH processing.  The value specified on the command line is
           size in 1k (1024) units to be allocated. */
        scan_opt_arg_number(&pch_mem_size, opt_arg);
        if (!checked_multiplication(&pch_mem_size, pch_mem_size, 1024)) {
          str_command_line_error(ec_cl_invalid_pch_size, opt_arg);
        }  /* if */
        if (pch_mem_size > (SIZE_OF_MEM_ALLOC_HISTORY *
                                                 HOST_ALLOCATION_INCREMENT)) {
          str_command_line_error(ec_cl_invalid_pch_size, opt_arg);
        }  /* if */
        break;
#endif /* !USE_MMAP_FOR_MEMORY_REGIONS */
      case optk_pch_dir:
        /* Directory to be used for PCH files. */
        pch_dir_name = file_name_from_opt_arg(opt_arg);
        if (!is_directory(pch_dir_name)) {
          str_command_line_error(ec_cl_invalid_pch_directory, pch_dir_name);
        }  /* if */
        break;
#if USE_FIXED_ADDRESS_FOR_MMAP
      case optk_fixed_address_for_mmap:
        /* Specify the address to use for the mmap system call.  Specifying
           an arbitrary address will likely result in a catastrophic failure
           or even an abort, but there's no good mechanism to verify that the
           address given is reasonable -- though the address should be on
           a page boundary. */
        fixed_address_for_mmap = scan_address(opt_arg);
        if (((size_t)fixed_address_for_mmap & (get_page_size()-1)) != 0) {
          str_command_line_error(ec_invalid_mmap_address, opt_arg);
        }  /* if */
        break;
#endif /* USE_FIXED_ADDRESS_FOR_MMAP */
      case optk_wdir:
        { a_const_char *wdir = file_name_from_opt_arg(opt_arg);

          reset_working_directory();
          if (is_directory(wdir)) {
            set_working_directory(wdir);
          } else {
            str_command_line_error(ec_cl_invalid_cwd, wdir);
          }  /* if */
        }
        break;
      case optk_create_module_header_unit:
        /* Create a module header unit file as part of this compilation. */
        create_module_unit = TRUE;
        created_module_unit_is_header = TRUE;
        module_unit_output_file_name = file_name_from_opt_arg(opt_arg);
        /* Make sure the specified name is acceptable as a module header unit
           file name. */
        check_module_header_unit_file_name(module_unit_output_file_name);
        break;
      case optk_create_module_interface_unit:
        /* Create a module interface unit file as part of this compilation. */
        create_module_unit = TRUE;
        module_unit_output_file_name = file_name_from_opt_arg(opt_arg);
        /* Make sure the specified name is acceptable as a module unit file
           name. */
        check_module_unit_file_name(module_unit_output_file_name);
        break;
      case optk_create_module_internal_unit:
        /* Create an internal module partition unit file as part of this
           compilation. */
        create_module_unit = TRUE;
        created_module_unit_is_internal = TRUE;
        module_unit_output_file_name = file_name_from_opt_arg(opt_arg);
        /* Make sure the specified name is acceptable as a module unit
           partition file name. */
        check_module_unit_partition_file_name(module_unit_output_file_name);
        break;
      case optk_restrict:
        /* Enables or disables recognition of the restrict token. */
        restrict_keyword_enabled = opt_value;
        break;
      case optk_long_lifetime_temps:
        /* Long or short lifetime temporaries. */
        long_lifetime_temps = opt_value;
        break;
#if MICROSOFT_EXTENSIONS_ALLOWED
      case optk_microsoft_bugs:
        /* Enable or disable the emulation of Microsoft bugs. */
        microsoft_bugs = opt_value;
        /* Now enable Microsoft mode. */
        opt_value = TRUE;
        goto enable_microsoft_mode;
      case optk_microsoft_version:
        /* The version of the Microsoft compiler being emulated. */
        scan_opt_arg_number(&microsoft_version, opt_arg);
        if (microsoft_version < 700 || microsoft_version > 3000) {
          str_command_line_error(ec_cl_invalid_microsoft_version, opt_arg);
        }  /* if */
        opt_value = TRUE;
        if (!option_kind_used[(int)optk_microsoft_extensions] &&
            !option_kind_used[(int)optk_microsoft_compatibility]) {
          /* By itself, specifying --microsoft_version implies enabling
             Microsoft emulation mode, but not if using --ms_extensions or
             --ms_compatibility. */
          goto enable_microsoft_mode;
        }  /* if */
        break;
      case optk_microsoft_build_number:
        /* The build number of the Microsoft compiler being emulated. */
        scan_opt_arg_number(&microsoft_build_number, opt_arg);
        opt_value = TRUE;
        goto enable_microsoft_mode;
      case optk_microsoft_mode:
        /* Enable or disable Microsoft extensions, in 32-bit mode.
           Note that 16/32 bit mode is not reset when enable_microsoft_mode
           is branched to by other options. */
#if NEAR_AND_FAR_ALLOWED
        il_header.near_and_far_are_enabled = FALSE;
#endif /* NEAR_AND_FAR_ALLOWED */
enable_microsoft_mode:
        microsoft_mode = opt_value;
        ms_extensions = opt_value;
        ms_compat = opt_value;
        if (!option_kind_used[(int)optk_cppcli] &&
            /* Make sure not to disable --cppcli mode if --cppcx has been
               specified. */
            !option_kind_used[(int)optk_cppcx]) {
          cppcli_enabled =
                      /*lint -e(506)*/DEFAULT_CPPCLI_ENABLED && microsoft_mode;
        }  /* if */
        if (!option_kind_used[(int)optk_microsoft_bugs]) {
          microsoft_bugs =
                     /*lint -e(506)*/DEFAULT_MICROSOFT_BUGS && microsoft_mode;
        }  /* if */
        break;
#if NEAR_AND_FAR_ALLOWED
      case optk_microsoft_16_mode:
        /* Enable or disable Microsoft extensions, in 16-bit mode. */
        check_assertion(opt_value == TRUE);
        il_header.near_and_far_are_enabled = TRUE;
        goto enable_microsoft_mode;
#endif /* NEAR_AND_FAR_ALLOWED */
      case optk_cppcli:
        cppcli_enabled = opt_value;
        set_C_dialect(C_dialect_cplusplus);
        if (opt_value && !option_kind_used[(int)optk_microsoft_mode]) {
          goto enable_microsoft_mode;
        }  /* if */
        break;
      case optk_cppcx:
        cppcx_enabled = opt_value;
        set_C_dialect(C_dialect_cplusplus);
        if (opt_value && !option_kind_used[(int)optk_microsoft_mode]) {
          goto enable_microsoft_mode;
        }  /* if */
        break;
      case optk_mscorlib_file_name:
        /* Specify the file name to be used to provide the system types
           normally provided by mscorlib.dll. */
        mscorlib_file_name = file_name_from_opt_arg(opt_arg);
        break;
      case optk_vcmeta_directory_name:
        /* Specify the directory name in which to find vcmeta.dll. */
        vcmeta_directory_name = file_name_from_opt_arg(opt_arg);
        break;
      case optk_using_framework_directory:
        /* Enable or disable searching for assemblies (#using) in the
           installation directory for the CLR. */
        using_framework_directory = opt_value;
        break;
      case optk_assembly_using_dir:
        /* Add the directory to the search path when searching for assemblies
           via #using. */
        add_to_specified_include_search_path(opt_arg,  /*system_dir=*/FALSE,
                                             &assembly_search_path,
                                             &end_assembly_search_path);
        break;
      case optk_microsoft_compatibility:
        /* Enable/disable clang's idea of Microsoft "compatibility" (also
           implies setting "extensions"). */
        ms_compat = opt_value;
        ms_extensions = opt_value;
        break;
      case optk_microsoft_extensions:
        /* Enable/disable clang's idea of Microsoft "extensions". */
        ms_extensions = opt_value;
        break;
      case optk_microsoft_cpp14_mode:
      case optk_microsoft_cpp17_mode:
      case optk_microsoft_cpp20_mode:
      case optk_microsoft_cpp23_mode:
      case optk_microsoft_cpplatest_mode:
        set_C_dialect(C_dialect_cplusplus);
        opt_value = TRUE;
        goto enable_microsoft_mode;
      case optk_microsoft_c11:
        /* Emulate the Microsoft /std:c11 option. */
        std_version = 201112;
        set_C_dialect(C_dialect_ANSI);
        opt_value = TRUE;
        goto enable_microsoft_mode;
      case optk_microsoft_c17:
        /* Emulate the Microsoft /std:c17 option. */
        std_version = 201710;
        set_C_dialect(C_dialect_ANSI);
        opt_value = TRUE;
        goto enable_microsoft_mode;
      case optk_microsoft_c23:
        /* Emulate the Microsoft /std:c23 option. */
        std_version = 202311;
        set_C_dialect(C_dialect_ANSI);
        opt_value = TRUE;
        goto enable_microsoft_mode;
      case optk_ms_permissive:
        /* Emulate Microsoft's /permissive[-] switch (which also implies
           Microsoft mode). */
        ms_permissive = opt_value;
        opt_value = TRUE;
        goto enable_microsoft_mode;
      case optk_ms_rvalue_cast:
        /* Emulate Microsoft's /Zc:rvalueCast[-] switch (which also implies
           Microsoft mode). */
        preserve_lvalues_with_same_type_casts = !opt_value;
        opt_value = TRUE;
        goto enable_microsoft_mode;
      case optk_ms_strict_ternary:
        /* Emulate Microsoft's /Zc:ternary[-] switch (which also implies
           Microsoft mode). */
        ms_strict_ternary = opt_value;
        opt_value = TRUE;
        goto enable_microsoft_mode;
      case optk_ms_cplusplus_std_value:
        /* Emulate Microsoft's /Zc:__cplusplus[-] switch (which also implies
           Microsoft mode). */
        ms_cplusplus_std_value = opt_value;
        opt_value = TRUE;
        goto enable_microsoft_mode;
      case optk_ms_stdc:
        /* Emulate Microsoft's /Zc:__STDC__ switch (which also implies
           Microsoft mode). */
        ms_stdc = opt_value;
        opt_value = TRUE;
        goto enable_microsoft_mode;
      case optk_ms_std_preproc:
        ms_std_preproc = opt_value;
        opt_value = TRUE;
        goto enable_microsoft_mode;
      case optk_microsoft_await_strict:
        /* Emulate /await:strict to explicitly request experimental pre-C++20
           coroutine behavior in C++14 and C++17 modes. */
        ms_await_strict = TRUE;
        FALLTHROUGH
      case optk_microsoft_await:
        /* Emulate Microsoft's /await command-line option to enable
           experimental pre-C++20 coroutine behavior. */
        check_assertion(opt_value == TRUE);
        ms_await = TRUE;
        break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NEAR_AND_FAR_ALLOWED
      case optk_far_data_pointers:
        /* Set size (either near or far) of data pointers when near/far
           support is enabled. */
        il_header.far_data_pointers = opt_value;
        break;
      case optk_far_code_pointers:
        /* Set size (either near or far) of code pointers when near/far
           support is enabled. */
        il_header.far_code_pointers = opt_value;
        break;
#endif /* NEAR_AND_FAR_ALLOWED */
      case optk_wchar_t_is_keyword:
        /* wchar_t is or is not a keyword. */
        wchar_t_is_keyword = opt_value;
        break;
      case optk_pack_alignment:
        /* If a pack alignment value is given, it means the alignment of
           nonstatic data members may be smaller than what is dictated by
           the member's type.  In effect, the pack alignment value is the
           maximum alignment permitted for a nonstatic data member.  This
           default value may be overridden by #pragma pack. */
        { a_host_large_integer pack_alignment_arg;

          scan_opt_arg_number(&pack_alignment_arg, opt_arg);
          if (!check_pack_alignment_value(pack_alignment_arg,
                                          &default_max_member_alignment)) {
            /* Invalid value. */
            command_line_error(ec_bad_pack_alignment);
          }  /* if */
        }
        break;
      case optk_alternative_tokens:
        /* Digraphs should or should not be allowed.  This also controls
           recognition of operator keywords (e.g., "not", "and") in C++. */
        alternative_tokens_allowed = opt_value;
        break;
#if MINIMAL_INLINING
      case optk_inlining:
        /* Minimal inlining should or should not be done. */
        inlining_enabled = opt_value;
        break;
     case optk_inline_statement_limit:
        /* The maximum number of (lowered) statements to allow in a routine
           that can be inlined. */
        scan_opt_arg_number(&inline_statement_limit, opt_arg);
        break;
#endif /* MINIMAL_INLINING */
      case optk_SVR4_C_mode:
        /* SVR4 C compatibility mode should or should not be used.  This
           option implies ANSI C mode, even in the "--no_svr4" form.
           In other words, --[no_]svr4 is short for --c --[no_]svr4.
           See --c99 and --sun for similar behavior. */
        SVR4_C_mode = opt_value;
        set_C_dialect(C_dialect_ANSI);
        break;
      case optk_brief_diagnostics:
        /* Diagnostics should or should not be emitted in a form that
	   omits the source information and suppresses wrapping of
	   the error message text. */
	brief_diagnostics = opt_value;
        break;
      case optk_wrap_diagnostics:
        /* Diagnostics should or should not be emitted in a form that
	   suppresses wrapping of the error message text. */
	do_not_wrap_diagnostics = !opt_value;
        break;
      case optk_nonconst_ref_anachronism:
        /* A reference to nonconst is allowed to bind to a class rvalue. */
        allow_nonconst_ref_anachronism = opt_value;
        break;
      case optk_no_preproc_only:
        /* Override the automatic setting of do_preprocessing_only.  This
	   can be used to force full compilation when it would not normally
	   be done. */
        suppress_do_preprocessing_only = opt_value;
        break;
      case optk_rtti:
        /* Enable/disable runtime type information (RTTI). */
        rtti_enabled = opt_value;
        break;
      case optk_building_runtime:
        /* We are building the runtime library for the compiler. */
        check_assertion(opt_value == TRUE);
        building_runtime = TRUE;
        break;
      case optk_bool_is_keyword:
        /* bool is or is not a keyword. */
        bool_is_keyword = opt_value;
        break;
      case optk_array_new_and_delete:
        /* Enable/disable array new and delete. */
        array_new_and_delete_enabled = opt_value;
        break;
      case optk_explicit:
        /* Enable/disable recognition of the keyword "explicit". */
        explicit_keyword_enabled = opt_value;
        break;
      case optk_namespaces:
        /* Enable/disable namespaces. */
        namespaces_enabled = opt_value;
        break;
      case optk_implicit_using_std:
        /* Enable/disable implicit use of the std namespace by the runtime. */
        implicit_using_std = opt_value;
        break;
      case optk_remove_unneeded_entities:
        /* If FALSE, suppress elimination of unneeded IL entries. */
        remove_unneeded_entities = opt_value;
        break;
      case optk_typename:
        /* Enable/disable typename. */
        typename_enabled = opt_value;
        break;
      case optk_implicit_typename:
        /* Enable/disable implicit determination of whether a template
           dependent name is a type or nontype. */
        implicit_typename_enabled = opt_value;
        force_implicit_typename = opt_value;
        break;
      case optk_special_subscript_cost:
        /* Enable/disable a special weighting for the conversion to the
           integral operand of [] in overload resolution. */
        special_subscript_cost = opt_value;
        break;
      case optk_old_style_preprocessing:
        /* Enable PCC style preprocessing. */
        old_style_preprocessing = TRUE;
        break;
      case optk_old_for_init:
        /* Enable/disable old-style scoping for for-init declarations. */
        use_nonstandard_for_init_scope = opt_value;
        break;
      case optk_for_init_diff_warning:
        /* Enable/disable warnings when new for-init scoping gives different
           visibility than old rules. */
        warning_on_for_init_difference = opt_value;
        break;
      case optk_distinct_template_signatures:
        /* Enable distinct name mangling for templates and nontemplates. */
        distinct_template_signatures = opt_value;
        break;
      case optk_guiding_decls:
        /* Enable/disable guiding declarations of template functions. */
        guiding_decls_allowed = opt_value;
        break;
      case optk_old_specializations:
        /* Enable/disable old-style specialization declarations. */
        old_specializations_allowed = opt_value;
        break;
#if IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE
      case optk_implicit_extern_c_type_conversion:
        /* Enable/disable conversions between extern "C" and extern "C++"
           function pointers. */
        impl_conv_between_c_and_cpp_function_ptrs_allowed = opt_value;
        break;
#endif /* IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE */
      case optk_long_preserving_rules:
        long_preserving_rules = opt_value;
        break;
      case optk_extern_inline:
        extern_inline_allowed = opt_value;
        break;
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
      case optk_multibyte_chars:
        multibyte_chars_in_source_enabled = opt_value;
        /* If multibyte characters have been enabled, make sure the multibyte
           locale has been set. */
        if (opt_value) set_multibyte_locale();
        break;
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
      case optk_embedded_cplusplus:
        report_embedded_cplusplus_noncompliance = opt_value;
        break;
#if VLA_ALLOWED
      case optk_vla:
        vla_enabled = opt_value;
        break;
#endif /* VLA_ALLOWED */
      case optk_enum_overloading:
        operator_overloading_on_enums_enabled = opt_value;
        break;
      case optk_nonstandard_qualifier_deduction:
        nonstandard_qualifier_deduction = opt_value;
        break;
#if ONE_INSTANTIATION_PER_OBJECT
      case optk_one_instantiation_per_object:
        /* Enable one instantiation per object file mode. */
        one_instantiation_per_object = opt_value;
        break;
      case optk_instantiation_dir:
        instantiation_dir_name = file_name_from_opt_arg(opt_arg);
        if (!is_directory(instantiation_dir_name)) {
          str_command_line_error(ec_cl_invalid_instantiation_directory,
                                 instantiation_dir_name);
        }  /* if */
        break;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
      case optk_late_tiebreaker:
        /* Early vs. late overload resolution tiebreaker. */
        do_late_ovl_res_tiebreaker = opt_value;
        break;
      case optk_pending_instantiations:
        /* The number of instantiations of a given template that may be in
           progress at any given time, or zero for an unlimited number. */
        scan_opt_arg_number(&max_pending_instantiations, opt_arg);
        if (max_pending_instantiations == 0) {
          max_pending_instantiations = ULONG_MAX;
        }  /* if */
        break;
#if MICROSOFT_EXTENSIONS_ALLOWED
      case optk_import_dir:
        import_dir_name = file_name_from_opt_arg(opt_arg);
        if (!is_directory(import_dir_name)) {
          str_command_line_error(ec_cl_invalid_import_directory,
                                 import_dir_name);
        }  /* if */
        break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      case optk_const_string_literals:
        /* String literals are const. */
        string_literals_are_const = opt_value;
        break;
      case optk_class_name_injection:
        /* Class names should or should not be injected into the scope of
           the class. */
        class_name_injection_enabled = opt_value;
        break;
      case optk_arg_dependent_lookup:
        /* Argument dependent lookup should or should not be performed. */
        arg_dependent_lookup_enabled = opt_value;
        break;
      case optk_friend_injection:
        /* Class and function names declared only in friend declarations
           should or should not be visible to normal lookups. */
        friend_class_injection_enabled = opt_value;
        friend_function_injection_enabled = opt_value;
        break;
      case optk_nonstandard_using_decl:
        /* A nonmember using-declaration that specifies an unqualified name
           should or should not be accepted. */
        nonstandard_using_decl_allowed = opt_value;
        break;
      case optk_designators:
        /* Ordinary designators should or should not be accepted. */
        designators_allowed = opt_value;
        break;
      case optk_extended_designators:
        /* Extended designators should or should not be accepted. */
        extended_designators_allowed = opt_value;
        break;
      case optk_variadic_macros:
        /* Ordinary variadic macros should or should not be accepted. */
        variadic_macros_allowed = opt_value;
        break;
      case optk_extended_variadic_macros:
        /* Extended variadic macros should or should not be accepted. */
        extended_variadic_macros_allowed = opt_value;
        break;
      case optk_include_file_suffixes:
        /* Specifies the list of suffixes to be used when searching for an
           include file name specified with no suffix. */
        include_file_suffixes = opt_arg;
        break;
      case optk_compound_literals:
        /* Compound literals should or should not be accepted. */
        compound_literals_allowed = opt_value;
        break;
      case optk_base_assign_op_is_default:
        /* A copy assignment operator that takes a base class as input should
           or should not be considered to be the default copy assignment
           operator for a class. */
        allow_copy_assignment_op_with_base_class_param = opt_value;
        break;
#if SUN_EXTENSIONS_ALLOWED
      case optk_sun_mode:
        /* Compatibility with Sun CC 5.x (various extensions/bugs) should or
           should not be provided.  This is a C++-mode option. */
        sun_mode = opt_value;
        set_C_dialect(C_dialect_cplusplus);
        break;
      case optk_sun_linker_scope:
        /* Sun CC 5.5 introduced the linker scope specifiers __global,
           __symbolic, and __hidden.  This option controls whether the front
           end should accept those specifiers.  ("__global" is unfortunately
           used in the standard header files shipped with Sun CC versions
           prior to 5.5.)  These keywords can also be enabled in C mode,
           because the Sun C compiler also accepts them. */
        sun_linker_scope_allowed = opt_value;
        break;
#endif /* SUN_EXTENSIONS_ALLOWED */
      case optk_dependent_name_processing:
        /* Enable dependent name processing for templates. */
        do_dependent_name_processing = opt_value;
        break;
      case optk_ignore_namespace_std:
        /* Enable the g++ compatibility option that treats "std" as an
           alias for the global namespace. */
        ignore_std_namespace = opt_value;
        break;
      case optk_parse_nonclass_templates:
        /* Enable prototype instantiation of nonclass templates. */
        nonclass_prototype_instantiations = opt_value;
        break;
      case optk_c23_mode:
        /* Enable C23 mode.  This option implies ANSI C mode. */
        check_assertion(opt_value == TRUE);
        std_version = 202311;
        set_C_dialect(C_dialect_ANSI);
        break;
      case optk_c18_mode:
        /* Enable C18 mode.  This option implies ANSI C mode. */
        check_assertion(opt_value == TRUE);
        std_version = 201710;
        set_C_dialect(C_dialect_ANSI);
        break;
      case optk_c11_mode:
        /* Enable C11 mode.  This option implies ANSI C mode. */
        check_assertion(opt_value == TRUE);
        std_version = 201112;
        set_C_dialect(C_dialect_ANSI);
        break;
      case optk_c99_mode:
        /* C99 mode should or should not be used.  This option implies
           ANSI C mode, even in the "--no_c99" form. In other words,
           --[no_]c99 is short for --c --[no_]c99.  See --svr4 and --sun
           for similar behavior. */
        std_version = 199901;
        set_C_dialect(C_dialect_ANSI);
        break;
      case optk_c89_mode:
        /* Compile ANSI C89/ISO C90 code.  This option is convenient if
           another ANSI C dialect (SVR4 C or C99) is selected by default. */
        check_assertion(opt_value == TRUE);
        std_version = 199000;
        SVR4_C_mode = FALSE;
        set_C_dialect(C_dialect_ANSI);
        break;
      case optk_export_template:
        /* Enable use of exported templates. */
        export_template_allowed = opt_value;
        /* Make sure the keyword is enabled if export support is being
           enabled. */
        if (export_template_allowed) export_keyword_enabled = TRUE;
        break;
      case optk_stdarg_builtin:
        /* Enable passing of references to stdarg.h macros to the output
           unchanged. */
        pass_stdarg_references_to_generated_code = opt_value;
        break;
#if ENABLE_TRANS_UNIT_TEST_MODE
      case optk_trans_unit_test_mode:
        /* Enable mode to test the compilation of multiple (possibly identical)
           translation units. */
        trans_unit_test_mode = opt_value;
        break;
#endif /* ENABLE_TRANS_UNIT_TEST_MODE */
#if GNU_EXTENSIONS_ALLOWED
      case optk_gcc_mode:
        /* GNU C mode should or should not be used.  This option implies
           ANSI C mode, even in the "--no_gcc" form. In other words,
           --[no_]gcc is short for --c --[no_]gcc.  See --svr4, --c99 and
           --sun for similar behavior.  The clang dialect will be selected
           if DEFAULT_CLANG_COMPATIBILITY is TRUE. */
        gcc_mode = gnu_mode = opt_value;
        set_C_dialect(C_dialect_ANSI);
        break;
      case optk_gpp_mode:
        /* GNU C++ mode should or should not be used.  This option implies
           C++ mode, even in the "--no_g++" form. In other words,
           --[no_]g++ is short for --c++ --[no_]g++.  See --sun, --c99 and
           --svr4 for similar behavior.  The clang dialect will be selected
           if DEFAULT_CLANG_COMPATIBILITY is TRUE. */
        gpp_mode = gnu_mode = opt_value;
        set_C_dialect(C_dialect_cplusplus);
        break;
      case optk_clang_mode:
        /* clang mode is a dialect of GNU mode.  gcc_mode or gpp_mode will
           be set by check_dialect_and_language_modes as a consequence of
           optk_clang_mode having been used.  Just set the global variable
           for the dialect here. */
        clang_mode = opt_value;
        break;
      case optk_gnu_version:
        /* The version of the GNU compiler being emulated.  If specified
           without one of the options --gcc, --no_gcc, --g++, or --no_g++,
           then --gcc is implied if --c is specified, and --g++ is implied
           otherwise. */
        scan_opt_arg_number(&gnu_version, opt_arg);
        gnu_mode = TRUE;
        if (gnu_version < MIN_GNU_VERSION || gnu_version > 999999) {
          str_command_line_error(ec_cl_invalid_gnu_version, opt_arg);
        }  /* if */
        break;
      case optk_clang_version:
        /* The version of clang being emulated.  Currently, there are no
           version dependencies for clang mode, so the version number is
           not restricted here.  Implies the clang dialect of GNU mode.
           gcc_mode or gpp_mode will be set by
           check_dialect_and_language_modes as a consequence of
           optk_clang_version having been used.  Just set the version and
           the dialect flag here. */
        scan_opt_arg_number(&clang_version, opt_arg);
        clang_mode = TRUE;
        if (clang_version > 999999) {
          str_command_line_error(ec_cl_invalid_clang_version, opt_arg);
        }  /* if */
        break;
      case optk_report_gnu_extensions:
        /* An option to request that the use of GNU extensions outside system
           headers be diagnosed with a warning. */
        check_assertion(opt_value == TRUE);
        report_gnu_extensions = TRUE;
        break;
      case optk_short_enums:
        /* An option to specify that all enumeration types should be
           treated as if they were declared with the "packed" attribute. */
        il_header.short_enums = opt_value;
        break;
      case optk_strict_gnu:
        /* When set, acts like -std=c* rather than -std=gnu*. */
        strict_gnu = opt_value;
        break;
#endif /* GNU_EXTENSIONS_ALLOWED */
      case optk_long_long:
        /* The long long feature cannot be disabled when the front end is
           configured to use it, but its use results in an error in certain
           modes.  This option is used to suppress such errors. */
        long_long_is_standard = opt_value;
        break;
      case optk_context_limit:
        /* Specify the maximum number of instantiation contexts that should be
           displayed as part of a diagnostic message. */
        scan_opt_arg_number(&context_limit, opt_arg);
        /* The smallest allowed value is 2.  If 1 is specified, use 2
           instead.  A value of zero is permitted and is interpreted as
           no limit. */
        if (context_limit == 1) context_limit = 2;
        /* Convert odd numbers to the next lower even number.  We display
           limit/2 lines of initial and trailing context so we can only
           really handle even numbers. */
        context_limit = context_limit & (~1);
        break;
      case optk_set_flag:
        /* Set the value of a specified flag name. */
        set_flag_value(opt_arg, opt_value);
        break;
#if UPC_EXTENSIONS_ALLOWED
      case optk_upc_mode:
        /* Enable (or disable) support for Unified Parallel C.  Specifying
           these options also implies C mode. */
        upc_mode = opt_value;
        set_C_dialect(C_dialect_ANSI);
        break;
      case optk_upc_strict_access:
        /* Set the default UPC access mode. */
        il_header.default_upc_strict_access = opt_value;
        break;
      case optk_upc_threads:
        /* Set the number of UPC threads at compile time. */
        scan_opt_arg_number(&upc_num_threads, opt_arg);
        break;
#endif /* UPC_EXTENSIONS_ALLOWED */
#if FIXED_POINT_ALLOWED
      case optk_fixed_point:
        /* Enable (or disable) support for fixed-point extensions. */
        fixed_point_enabled = opt_value;
        break;
#endif /* FIXED_POINT_ALLOWED */
#if NAMED_ADDRESS_SPACES_ALLOWED
      case optk_named_address_spaces:
        /* Enable (or disable) support for named address spaces. */
        named_address_spaces_enabled = opt_value;
        break;
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
      case optk_edg_base_directory:
        edg_base_directory = file_name_from_opt_arg(opt_arg);
        if (!is_directory(edg_base_directory)) {
          str_command_line_error(ec_cl_invalid_edg_base_directory,
                                 edg_base_directory);
        }  /* if */
        break;
#if NAMED_REGISTERS_ALLOWED
      case optk_named_registers:
        /* Enable (or disable) support for named-register storage classes. */
        named_registers_enabled = opt_value;
        break;
#endif /* NAMED_REGISTERS_ALLOWED */
#if EMBEDDED_C_ALLOWED
      case optk_embedded_c:
        /* Enable (or disable) all the Embedded C (TR 18037) extensions
           that are supported in the current configuration.  This option
           implies ANSI C mode, even in the "--no_embedded_c" form. */
#if FIXED_POINT_ALLOWED
        fixed_point_enabled = opt_value;
#endif /* FIXED_POINT_ALLOWED */
#if NAMED_ADDRESS_SPACES_ALLOWED
        named_address_spaces_enabled = opt_value;
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
#if NAMED_REGISTERS_ALLOWED
        named_registers_enabled = opt_value;
#endif /* NAMED_REGISTERS_ALLOWED */
        set_C_dialect(C_dialect_ANSI);
        break;
#endif /* EMBEDDED_C_ALLOWED */
      case optk_thread_local_storage:
        /* Enable (or disable) support for the C++11 thread_local or C11
           _Thread_local keyword.  Also enable (or disable) support for
           __thread in configurations where that is supported. */
        std_thread_local_storage_specifier_enabled = opt_value;
#if THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
        thread_local_storage_specifier_enabled = opt_value;
#endif /* THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED */
        break;
#if FULLY_RESOLVED_MACRO_POSITIONS
      case optk_macro_positions_in_diagnostics:
        /* Diagnostics that refer to text in macro expansions should or should
           not contain information about the original location from which that
           text was copied and (if RECORD_MACRO_INVOCATIONS is TRUE) the
           stack of macro invocations in effect at that point. */
        macro_positions_in_diagnostics = opt_value;
        break;
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
      case optk_trigraphs:
        /* Trigraphs should or should not be allowed. */
        trigraphs_allowed = opt_value;
        break;
      case optk_nonstandard_default_arg_deduction:
        nonstandard_default_arg_deduction = opt_value;
        break;
      case optk_stdc_zero_in_system_headers:
        stdc_zero_in_system_headers = opt_value;
        break;
      case optk_template_typedefs_in_diagnostics:
        display_template_typedefs_in_diagnostics = opt_value;
        break;
      case optk_defer_parse_function_templates:
        /* Defer prototype instantiation of function templates. */
        defer_function_prototype_instantiations = opt_value;
        break;
      case optk_uliterals:
        /* U... and u... literals should or should not be allowed.  This also
           has the effect of enabling or disabling char16_t/char32_t
           keywords (C++ only). */
        uliterals_enabled = opt_value;
        break;
#if MICROSOFT_EXTENSIONS_ALLOWED
      case optk_default_calling_convention:
        /* The calling convention used for functions not explicitly declared
           with one. */
        for (i = 0; i < (int)cc_last; ++i) {
          if (i != (int)cc_default &&
              strcmp(calling_convention_names[i], opt_arg) == 0) {
            break;
          }  /* if */
        }  /* for */
        if (i != (int)cc_last) {
          default_calling_convention = (a_calling_convention)i;
        } else {
          /* Report an invalid calling convention. */
          a_diagnostic_ptr dp;
          dp = start_command_line_error(ec_cl_unrecognized_calling_convention,
                                        opt_arg);
          for (i = 0; i < (int)cc_last; ++i) {
            if (i != (int)cc_default) {
              str_add_diag_info(dp, ec_cl_calling_convention_list,
                                calling_convention_names[i]);
            }  /* if */
          }  /* for */
          end_command_line_error(dp);
        }  /* if */
        break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      case optk_type_traits_helpers:
        /* Enable or disable __is_union, __has_virtual_destructor, etc. */
        type_traits_helpers_enabled = opt_value;
        break;
      case optk_cpp26_mode:
        /* Enable C++ features added as part of C++26. */
        std_version = 202603;
        set_C_dialect(C_dialect_cplusplus);
        break;
      case optk_cpp23_mode:
        /* Enable C++ features added as part of C++23. */
        std_version = 202302;
        set_C_dialect(C_dialect_cplusplus);
        break;
      case optk_cpp20_mode:
        /* Enable C++ features added as part of C++20. */
        std_version = 202002;
        set_C_dialect(C_dialect_cplusplus);
        break;
      case optk_cpp17_mode:
        /* Enable C++ features added as part of C++17. */
        std_version = 201703;
        set_C_dialect(C_dialect_cplusplus);
        break;
      case optk_cpp14_mode:
        /* Enable C++ features added as part of C++14. */
        std_version = 201402;
        set_C_dialect(C_dialect_cplusplus);
        break;
      case optk_cpp11_mode:
        /* Enable or disable C++ features added as part of C++11. */
        std_version = opt_value ? 201103 : 199711;
        set_C_dialect(C_dialect_cplusplus);
        break;
      case optk_cpp03_mode:
        /* Compile ISO/IEC 14882:2003 C++ code.  This option explicitly
           disables all C++11 extensions. */
        check_assertion(opt_value == TRUE);
        std_version = 199711;
        set_C_dialect(C_dialect_cplusplus);
        break;
      case optk_list_macros:
        /* Do preprocessing only; list all macro definitions to stdout or
           the preprocessor output file. */
        check_assertion(opt_value == TRUE);
        do_preprocessing_only = TRUE;
        generate_pp_output = FALSE;
        list_macro_definitions = TRUE;
        break;
      case optk_top_templates:
        /* Request a report of the most-substituted templates.  The argument
           is the number of templates to report; zero means report every
           template that has a nonzero number of substitutions. */
        scan_opt_arg_number(&top_templates_count, opt_arg);
        collect_top_templates = TRUE;
        break;
#if DUMP_CONFIG_ENABLED
      case optk_dump_configuration:
        /* Display the values of all configuration macros with which this
           executable was built. */
        dump_configuration_macros();
        source_file_name_optional = TRUE;
        break;
      case optk_dump_legacy_as_target:
        /* Display a new target configuration based on the legacy target
           configuration used when this executable was built. */
        dump_legacy_config_as_target_config(opt_arg);
        source_file_name_optional = TRUE;
        break;
#endif /* DUMP_CONFIG_ENABLED */
      case optk_signed_bit_fields:
        targ_plain_int_bit_field_is_unsigned = FALSE;
        break;
      case optk_unsigned_bit_fields:
        targ_plain_int_bit_field_is_unsigned = TRUE;
        break;
      case optk_check_concatenations:
        check_concatenations = opt_value;
        break;
#if UNICODE_SOURCE_SUPPORTED
      case optk_unicode_source_kind:
        /* Specify default Unicode source kind. */
        if (strcmp(opt_arg, "UTF-8") == 0) {
          default_unicode_source_kind = usk_utf8;
        } else if (strcmp(opt_arg, "UTF-16") == 0) {
          default_unicode_source_kind = host_little_endian ? usk_utf16LE :
                                                             usk_utf16BE;
        } else if (strcmp(opt_arg, "UTF-16LE") == 0) {
          default_unicode_source_kind = usk_utf16LE;
        } else if (strcmp(opt_arg, "UTF-16BE") == 0) {
          default_unicode_source_kind = usk_utf16BE;
        } else if (strcmp(opt_arg, "none") == 0) {
          default_unicode_source_kind = usk_none;
        } else {
          str_command_line_error(ec_cl_unrecognized_unicode_source_kind,
                                 opt_arg);
        }  /* if */
        break;
#endif /* UNICODE_SOURCE_SUPPORTED */
      case optk_lambdas:
        lambdas_enabled = opt_value;
        break;
      case optk_rvalue_references:
        rvalue_references_enabled = opt_value;
        break;
      case optk_rvalue_ctor_is_copy_ctor:
        rvalue_ctor_is_copy_ctor = opt_value;
        break;
      case optk_gen_move_operations:
        generate_move_operations = opt_value;
        break;
      case optk_auto_type:
        auto_type_specifier_enabled = opt_value;
        break;
      case optk_auto_storage:
        auto_storage_class_specifier_enabled = opt_value;
        break;
      case optk_nonstandard_instantiation_lookup:
        nonstandard_instantiation_lookup_enabled = opt_value;
        break;
      case optk_nullptr:
        nullptr_enabled = opt_value;
        break;
#if GNU_EXTENSIONS_ALLOWED
      case optk_gnu_c89_inlining:
        std_c99_inlining = FALSE;
        gnu_c89_inlining = TRUE;
        break;
      case optk_nonstd_gnu_keywords:
        nonstd_gnu_keywords_enabled = opt_value;
        break;
      case optk_default_nocommon:
        il_header.default_nocommon = opt_value;
        break;
#endif /* GNU_EXTENSIONS_ALLOWED */
      case optk_token_separators_in_pp_output:
        check_assertion(opt_value == TRUE);
        no_token_separators_in_pp_output = TRUE;
        break;
      case optk_c23_typeof:
        c23_typeof_enabled = opt_value;
        break;
      case optk_cpp11_sfinae:
        cpp11_sfinae_enabled = opt_value;
        break;
      case optk_cpp11_sfinae_ignore_access:
        cpp11_sfinae_ignore_access = opt_value;
        break;
      case optk_variadic_templates:
        variadic_templates_enabled = opt_value;
        break;
      case optk_func_prototype_tags:
        func_prototype_tags_enabled = opt_value;
        break;
      case optk_require_func_prototypes:
        require_func_prototypes = opt_value;
        break;
      case optk_implicit_noexcept:
        implicit_noexcept_enabled = opt_value;
        break;
      case optk_unrestricted_unions:
        unrestricted_unions_enabled = opt_value;
        break;
      case optk_max_depth_constexpr_call:
        scan_opt_arg_number(&max_depth_constexpr_call, opt_arg);
        break;
      case optk_max_cost_constexpr_call:
        scan_opt_arg_number(&max_cost_constexpr_call, opt_arg);
        break;
      case optk_delegating_constructors:
        delegating_constructors_enabled = opt_value;
        break;
      case optk_lossy_warning:
        warning_on_lossy_conversion = opt_value;
        break;
      case optk_deprecated_string_conv:
        deprecated_string_literal_conv_allowed = opt_value;
        break;
      case optk_user_defined_literals:
        user_defined_literals_enabled = opt_value;
        break;
      case optk_preserve_lvalues_with_same_type_casts:
        preserve_lvalues_with_same_type_casts = opt_value;
        break;
      case optk_nonstd_anonymous_unions:
        allow_nonstandard_anonymous_unions = opt_value;
        break;
      case optk_digit_separators:
        digit_separators_enabled = opt_value;
        break;
      case optk_target:
        /* Verify that the specified target configuration string is valid.
           Note that it is possible to specify multiple --target options
           (in which case the last one "wins"). */
        target_configuration_index = find_target_configuration(opt_arg);
        if (target_configuration_index != NO_TARGET_CONFIG) {
          /* Set the target-specific values now.  Note that if there are
             target-specific global variables that are modified by other
             command-line options, the global variable will be set according
             to the last command-line argument that effects it.  No warning
             or error is given. */
          set_target_configuration(target_configuration_index);
        } else {
          str_command_line_error(ec_cl_invalid_target, opt_arg);
        }  /* if */
        break;
      case optk_utf8_char_literals:
        utf8_char_literals_enabled = opt_value;
        break;
      case optk_stricter_template_checking:
        stricter_template_checking = TRUE;
        break;
      case optk_exc_spec_in_func_type:
        exc_spec_in_func_type = opt_value;
        break;
      case optk_aligned_new:
        overaligned_allocation_enabled = opt_value;
        break;
      case optk_char8_t:
        char8_t_enabled = opt_value;
        break;
      case optk_bit_precise_integers:
        bit_precise_int_enabled = opt_value;
        break;
      case optk_relaxed_abstract_checking:
        relaxed_abstract_checking = opt_value;
        break;
      case optk_concepts:
        concepts_enabled = opt_value;
        break;
      case optk_colors:
        colorize_diagnostics = opt_value;
        break;
      case optk_keep_restrict_in_signatures:
        keep_restrict_in_signatures = opt_value;
        break;
#if UNICODE_VULNERABILITY_DETECTION_SUPPORTED
      case optk_check_unicode_security:
        check_unicode_security = opt_value;
        break;
#endif /* UNICODE_VULNERABILITY_DETECTION_SUPPORTED */
      case optk_old_id_chars:
        old_id_chars = opt_value;
        break;
      case optk_add_match_notes:
        add_match_notes = opt_value;
        break;
      case optk_dump_command_options:
        /* Display all of the command-line options that can be specified
           using the --name format. */
        dump_command_options();
        source_file_name_optional = TRUE;
        break;
      case optk_output_mode:
        { a_const_char *output_mode_string = opt_arg;

          if (strcmp(output_mode_string, "text") == 0) {
            output_mode = om_text;
          } else if (strcmp(output_mode_string, "sarif") == 0) {
            output_mode = om_sarif;
          } else {
            str_command_line_error(ec_cl_invalid_output_mode,
                                   output_mode_string);
          }  /* if */
        }
        break;
      case optk_incognito:
        incognito = opt_value;
        break;
     default:
        /* It should not be possible to get here. */
        unexpected_condition();
    }  /* switch */
  }  /* while */
#if !USE_MMAP_FOR_MEMORY_REGIONS
  if (!non_pch_option_used) {
    /* If all of the command line options are PCH options, then the PCH
       memory may not have been allocated yet. */
    if (precompiled_header_processing_required) {
      preallocate_pch_memory();
    }  /* if */
  }  /* if */
#endif /* !USE_MMAP_FOR_MEMORY_REGIONS */
#if IA64_ABI
  if (emulate_unsafe_gnu_abi_bugs) {
    /* A request to emulate the unsafe GNU ABI bugs is also a request to
       emulate the safer GNU ABI bugs. */
    emulate_gnu_abi_bugs = TRUE;
  }  /* if */
#endif /* IA64_ABI */
  /* Check for consistent specification of dialects and language modes. */
  check_dialect_and_language_modes();
  /* Based on dialect and language mode settings, check for consistency of
     other options, and set global variables as appropriate. */
  if (C_dialect != C_dialect_cplusplus) {
    /* Check for the use of C++ options when the dialect being compiled
       is not C++. */
    check_and_set_c_mode_options();
  } else {
    /* The dialect is C++. */
    if (any_cfront_mode()) {
      /* Turn on cfront features. */
      set_cfront_mode_flags();
    }  /* if */
    check_and_set_cplusplus_mode_options();
  }  /* if */
  if (strict_ansi_mode) {
    check_and_set_ansi_mode_options();
  }  /* if */
  if (extended_designators_allowed) {
    /* If extended designators are allowed, the normal designators must
       be allowed also. */
    designators_allowed = TRUE;
  }  /* if */
  if (extended_variadic_macros_allowed) {
    /* If extended variadic macros are allowed, the normal variadic macros
       must be allowed also. */
    variadic_macros_allowed = TRUE;
  }  /* if */
  if (!do_dependent_name_processing && export_template_allowed) {
    /* We're not doing dependent name processing, but export template
       is specified.  If export template processing was not explicitly
       requested, turn it off. */
    if (!option_kind_used[(int)optk_export_template]) {
      export_template_allowed = FALSE;
    }  /* if */
  }  /* if */
  if (!distinct_template_signatures && export_template_allowed) {
    /* We're not generating distinct signatures for template instances, but
       export template is enabled.  If export template processing was not
       explicitly requested, turn it off. */
    if (!option_kind_used[(int)optk_export_template]) {
      export_template_allowed = FALSE;
    }  /* if */
  }  /* if */
  if (modules_enabled) {
    module_keywords_enabled = TRUE;
  }  /* if */
  if (export_template_allowed) {
    /* Export template processing requires dependent name processing. */
    if (option_kind_used[(int)optk_dependent_name_processing] &&
        !do_dependent_name_processing) {
      /* The option --no_dep_name was used: export template requires
         that dependent name processing is done. */
      command_line_error(ec_cl_export_template_requires_dep_name);
    }  /* if */
    /* Export template processing requires distinct template signatures. */
    if (!distinct_template_signatures) {
      /* Distinct template signatures are not being used: export template
         requires distinct template signatures. */
      command_line_error(ec_cl_export_template_requires_distinct_templ_sigs);
    }  /* if */
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
    if (option_kind_used[(int)optk_implicit_template_inclusion] &&
        implicit_template_inclusion_mode) {
      /* The option --implicit_include was used: it cannot be used with
         export template processing. */
      command_line_error(ec_cl_export_template_requires_no_implicit_include);
    }  /* if */
    implicit_template_inclusion_mode = FALSE;
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
    do_dependent_name_processing = TRUE;
    if (module_keywords_enabled) {
      /* This should not be enabled unless someone explicitly enabled it via
         the command line. */
      check_assertion(option_kind_used[(int)optk_export_template]);
      command_line_error(
                       ec_cl_export_template_option_incompatible_with_modules);
    }  /* if */
  }  /* if */
  if (trans_unit_test_mode) {
    /* Exported templates cannot be used in trans_unit_test mode.  Turn
       off the feature but reduce the diagnostic to a warning. */
    export_template_allowed = FALSE;
    (void)set_severity_for_error_number((int)ec_no_export_support, es_warning,
                                        /*make_default=*/TRUE);
  }  /* if */
  if (!nonclass_prototype_instantiations && do_dependent_name_processing) {
    /* We're not doing nonclass prototype instantiations, but dependent
       name processing is specified.  If dependent name processing was not
       explicitly requested, turn it off. */
    if (!option_kind_used[(int)optk_dependent_name_processing]) {
      do_dependent_name_processing = FALSE;
    }  /* if */
  }  /* if */
  if (do_dependent_name_processing) {
    /* Do nonclass prototype instantiations when dependent name processing
       is being done. */
    if (option_kind_used[(int)optk_parse_nonclass_templates] &&
        !nonclass_prototype_instantiations) {
      /* The option --no_parse_templates was used: it implies that
         no dependent name processing is done. */
      command_line_error(ec_cl_dep_name_requires_parse_nonclass_templates);
    }  /* if */
    nonclass_prototype_instantiations = TRUE;
    /* Do argument dependent lookup when doing dependent name processing. */
    arg_dependent_lookup_enabled = TRUE;
  }  /* if */
  if (cpp26_mode) {
    /* Do nonclass prototype instantiations when C++26 is enabled. */
    if (option_kind_used[(int)optk_parse_nonclass_templates] &&
        !nonclass_prototype_instantiations) {
      /* The option --no_parse_templates was used: it is incompatible with
         C++26 features like structured binding packs. */
      command_line_error(ec_cl_cpp26_requires_parse_nonclass_templates);
    }  /* if */
    nonclass_prototype_instantiations = TRUE;
  }  /* if */
  if (lambdas_enabled) {
    /* If lambdas are allowed, enable local types as template arguments too. */
    local_types_as_template_args_enabled = TRUE;
    decls_using_types_without_linkage_allowed = TRUE;
  }  /* if */
  if (trailing_return_types_enabled || decltype_enabled ||
      variadic_templates_enabled) {
    /* Turn on C++11 SFINAE if trailing return types, decltype, or variadic
       templates are enabled, since we're likely to need it. */
    if (!option_kind_used[(int)optk_cpp11_sfinae] &&
        !option_kind_used[(int)optk_cpp11_sfinae_ignore_access]) {
      cpp11_sfinae_enabled = TRUE;
      cpp11_sfinae_ignore_access = DEFAULT_CPP11_SFINAE_IGNORE_ACCESS;
    }  /* if */
  }  /* if */
  if (c23_mode && !option_kind_used[(int)optk_c23_typeof]) {
    c23_typeof_enabled = TRUE;
  }  /* if */
  if (!option_kind_used[(int)optk_bit_precise_integers]) {
    /* _BitInt types are a C23 feature (we enable them in all C23 dialects,
       even though Clang and GCC started supporting them in their respective
       versions 14).  GCC accepts them in their pre-C23 C modes and Clang
       accepts them in all their C and C++ modes. */
    bit_precise_int_enabled = c23_mode ||
                              gcc_version_is(>=140000) ||
                              clang_version_is(>=140000);
  }  /* if */
  if (unrestricted_unions_enabled) {
    /* Unrestricted unions require the ability to mark special member functions
       as "deleted". */
    deleted_functions_enabled = TRUE;
  }  /* if */
  if (long_lifetime_temps) {
    /* Don't allow long lifetime temps with some newer language features. */
    if (lambdas_enabled || cpp11_sfinae_enabled) {
      command_line_error(
                       ec_cl_long_lifetime_temps_incompat_with_newer_features);
    }  /* if */
  }  /* if */
  if (auto_type_specifier_enabled && option_kind_used[(int)optk_auto_type] &&
      !option_kind_used[(int)optk_auto_storage]) {
    /* If "auto" is explicitly enabled as a type specifier and not explicitly
       enabled as a storage class specifier, disable it as a storage class
       specifier: That corresponds to the standard C++11 meaning. */
    auto_storage_class_specifier_enabled = FALSE;
  } else if (!auto_type_specifier_enabled &&
             !auto_storage_class_specifier_enabled) {
    /* "auto" cannot be entirely disabled. */
    if (option_kind_used[(int)optk_auto_type] &&
        option_kind_used[(int)optk_auto_storage]) {
      /* Both were explicitly disabled: Issue an error. */
      command_line_error(ec_cl_auto_cannot_be_disabled);
    } else if (option_kind_used[(int)optk_auto_type]) {
      /* --no_auto_type appeared explicitly: Enable auto as a storage class. */
      auto_storage_class_specifier_enabled = TRUE;
    } else {
      /* --no_auto_storage appeared explicitly: Enable auto as a type
         specifier. */
      check_assertion(option_kind_used[(int)optk_auto_storage]);
      auto_type_specifier_enabled = TRUE;
    }  /* if */
  }  /* if */
  if (utf8_char_literals_enabled && !uliterals_enabled) {
    /* UTF-8 literals require u-literal support. */
    if (option_kind_used[(int)optk_uliterals]) {
      /* U-literals were explicitly disabled. */
      if (option_kind_used[(int)optk_utf8_char_literals]) {
        /* Report conflicting command-line options. */
        command_line_error(ec_utf8_lit_no_ulit);
      } else {
        /* UTF-8 literals were implicitly enabled but u-literals were
           explicitly disabled. */
        utf8_char_literals_enabled = FALSE;
      }  /* if */
    } else {
      uliterals_enabled = TRUE;
    }  /* if */
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cppcli_enabled || cppcx_enabled) {
    if (!option_kind_used[(int)optk_microsoft_version]) {
      /* If no Microsoft version was explicitly specified, use the default
         version that is to be used in C++/CLI and C++/CX modes. */
      microsoft_version = DEFAULT_CPPCLI_CPPCX_VERSION;
    }  /* if */
    if (cppcli_enabled && cppcx_enabled) {
      /* C++/CLI and C++/CX cannot both be enabled.  If one is enabled by
         default, and the other through the command-line, the latter takes
         precedence. */
      if (!option_kind_used[(int)optk_cppcx]) {
        cppcx_enabled = FALSE;
        check_assertion(option_kind_used[(int)optk_cppcli]);
      } else if (!option_kind_used[(int)optk_cppcli]) {
        cppcli_enabled = FALSE;
      } else {
        command_line_error(ec_cl_cppcx_and_cppcli);
      }  /* if */
    }  /* if */
    if (!microsoft_mode) {
      if ((option_kind_used[(int)optk_cppcli] ||
           option_kind_used[(int)optk_cppcx]) &&
          option_kind_used[(int)optk_microsoft_mode]) {
        /* Issue an error if Microsoft mode is explicitly turned off and
           C++/CLI or C++/CX mode is explicitly turned on. */
        command_line_error(cppcx_enabled ?
                               ec_cl_cppcx_only_in_microsoft_cplusplus :
                               ec_cl_cppcli_only_in_microsoft_cplusplus);
      }  /* if */
      cppcli_enabled = FALSE;
      cppcx_enabled = FALSE;
    } else if (C_mode()) {
      if (option_kind_used[(int)optk_cppcli] ||
          option_kind_used[(int)optk_cppcx]) {
        /* Issue an error if C++/CLI or C++/CX was turned on explicitly in
           C mode. */
        command_line_error(cppcx_enabled ?
                               ec_cl_cppcx_only_in_microsoft_cplusplus :
                               ec_cl_cppcli_only_in_microsoft_cplusplus);
      }  /* if */
      cppcli_enabled = FALSE;
      cppcx_enabled = FALSE;
    } else if (microsoft_version < 1600) {
      /* microsoft_version must be at least 1600 for C++/CLI and C++/CX
         features. */
      if (option_kind_used[(int)optk_microsoft_version]) {
        /* Issue an error if microsoft_version is explicitly set to a low
           value. */
        command_line_error(cppcx_enabled ?
                             ec_cl_microsoft_version_insufficient_for_cppcx :
                             ec_cl_microsoft_version_insufficient_for_cppcli);
      }  /* if */
      microsoft_version = 1600;
    }  /* if */
    cli_or_cx_enabled = cppcx_enabled || cppcli_enabled;
    if (!cppcli_enabled) use_cppcli_fill_ins = FALSE;
    if (cppcx_enabled) add_vccorlib_preinclude();
  }  /* if */
  if (microsoft_mode) {
    /* Turn on features implied by Microsoft mode. */
    set_microsoft_mode_flags();
  } else {
    /* Microsoft mode is not being used (though ms_extensions and ms_compat
       may be set -- see below). */
    microsoft_bugs = FALSE;
    if (import_dir_name != NULL) {
      /* --import_dir is allowed only in Microsoft mode. */
      command_line_error(ec_cl_import_only_in_microsoft);
    }  /* if */
  }  /* if */
  /* If no directory was specified for #import, use the current directory. */
  if (import_dir_name == NULL) {
    import_dir_name = ".";
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (sun_mode) {
    check_and_set_sun_mode_options();
  } else {
    exclude_sun_specific_options();
  }  /* if */
  if (gcc_mode) {
    check_and_set_gcc_mode_options();
  } else if (gpp_mode) {
    check_and_set_gpp_mode_options();
  } else {
    exclude_gnu_specific_options();
  }  /* if */
#if UPC_EXTENSIONS_ALLOWED
  if (upc_mode) {
    check_upc_mode();
  }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (!microsoft_mode && ms_extensions) {
    /* The Microsoft mode major dialect was not selected, but either
       --ms_extensions or --ms_compatibility has been selected.  In this case,
       select the appropriate options (any major dialect options have already
       been selected -- these will generally overwrite them). */
    if (!option_kind_used[(int)optk_microsoft_bugs]) {
      microsoft_bugs = TRUE;
    }  /* if */
    if (!option_kind_used[(int)optk_microsoft_version]) {
      /* If no version was explicitly specified, use 1900.  This seems to
         most closely match the behavior of clang though the default for its
         -fmsc-version command-line option is 1700. */
      microsoft_version = 1900;
    }  /* if */
    set_microsoft_mode_flags();
#if VLA_ALLOWED
    if (!(option_kind_used[(int)optk_vla])) {
      vla_enabled = TRUE;
    }  /* if */
#endif /* VLA_ALLOWED */
    if (!C_mode()) {
      /* Reset flags that were set in check_and_set_default_cpp11_extensions
         as appropriate. */
      pragma_operator_allowed = FALSE;
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (uliterals_enabled && !C_mode()) {
    /* U-literal enabling implies enabling char16_t and char32_t keywords
       (except in C mode). */
    char16_t_and_char32_t_are_keywords = TRUE;
  }  /* if */
  if (nonclass_prototype_instantiations && !ms_compat &&
      !option_kind_used[(int)optk_implicit_typename]) {
    /* When doing nonclass prototype instantiations, disable implicit typename
       unless it was explicitly enabled.  In Microsoft mode implicit typename
       is still used except in function prototype instantiations. */
    implicit_typename_enabled = FALSE;
  }  /* if */
  /* Set restrict_enabled if any form of the restrict keyword is allowed. */
  restrict_enabled = restrict_keyword_enabled || gnu_restrict_keyword_enabled;
  /* Ensure options related to rvalue references are now consistent. */
  check_rvalue_ref_options();
  if (relaxed_constexpr_enabled) {
    if (!bool_is_keyword) {
      if (option_kind_used[(int)optk_bool_is_keyword]) {
        command_line_error(ec_cl_relaxed_constexpr_requires_bool);
      }  /* if */
      bool_is_keyword = TRUE;
    }  /* if */
  }  /* if */
  if (constexpr_enabled) {
    /* The relaxation that lets a reference with an unknown referent be used
       in a constant expression (C++ paper P2280R4) was adopted for C++23 as
       a defect report, and so applies to every standard that has constexpr. */
    reference_to_unknown_object_allowed = TRUE;
  }  /* if */
  if (!noexcept_enabled && implicit_noexcept_enabled) {
    /* --implicit_noexcept cannot be specified in modes that don't permit
       noexcept. */
    if (option_kind_used[(int)optk_implicit_noexcept]) {
      command_line_error(ec_cl_implicit_noexcept_requires_noexcept_support);
    }  /* if */
    implicit_noexcept_enabled = FALSE;
  }  /* if */
  if (ignore_std_namespace) {
    /* In the g++ compatibility mode in which the std namespace is an alias
       for the global namespace, the va_list type should not be entered in
       the std namespace. */
    va_list_in_std_namespace = FALSE;
  }  /* if */
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  if (instantiation_mode == tim_local && automatic_instantiation_mode) {
    /* -tlocal mode cannot be used with automatic instantiation.  If
       automatic instantiation was explicitly requested on the command
       line then issue an error; otherwise disable automatic instantiation. */
    if (automatic_instantiation_mode != DEFAULT_AUTOMATIC_INSTANTIATION_MODE) {
      command_line_error(ec_cl_tim_local_conflicts_with_auto_instantiation);
    } else {
      automatic_instantiation_mode = FALSE;
    }  /* if */
  }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
#if ONE_INSTANTIATION_PER_OBJECT
  if (one_instantiation_per_object) {
    /* If "one instantiation per object" mode is being used, supply a default
       for the instantiation directory.  This should always be specified by
       the driver.  The default value is primarily for testing purposes. */
    if (instantiation_dir_name == NULL) instantiation_dir_name = ".";
  } else {
    /* If one instantiation per object mode is not being used, set the
       instantiation directory to NULL just in case one was specified on
       the command line. */
    instantiation_dir_name = NULL;
  }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  /* Determine whether enum types are considered to be integral. This global
     variable is used by is_integral_type, which returns TRUE for enum types
     in C mode and cfront mode, but otherwise returns FALSE in C++. */
  enum_type_is_integral = C_mode() || any_cfront_mode();
  /* Determine the appropriate error level for anachronism messages based
     on whether anachronisms are to be allowed. */
  anachronism_error_severity = allow_anachronisms ? es_warning : es_error;
  if (allow_anachronisms) {
    /* Enable the nonconst ref anachronism if anachronisms in general are
       enabled. */
    allow_nonconst_ref_anachronism = TRUE;
  }  /* if */
#if CPPCLI_ENABLING_POSSIBLE
  if (cli_or_cx_enabled) {
#if DO_IL_LOWERING
  /* IL lowering cannot handle C++/CLI or C++/CX constructs, so if we are
     accepting those extensions disable lowering.  (This is possible only when
     a special "trust me" macro is set explicitly.) */
#if !ALLOW_CPPCLI_AND_CPPCX_WITH_LOWERING
/* host_envir.h checks this too, so if we fail here someone has broken the
   test there. */
 #error -- IL lowering cannot be done when C++/CLI enabling is allowed.
#endif /* !ALLOW_CPPCLI_AND_CPPCX_WITH_LOWERING */
    suppress_il_lowering = TRUE;
    suppress_back_end = TRUE;
#if IL_SHOULD_BE_WRITTEN_TO_FILE
    suppress_il_file_write = TRUE;
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
#endif /* DO_IL_LOWERING */
    for_each_statement_enabled = TRUE;
  }  /* if */
#endif /* CPPCLI_ENABLING_POSSIBLE */
#if DO_IL_LOWERING && ABI_CHANGES_FOR_RTTI
#if SUPPRESS_TYPEINFO_VARIABLES_WHEN_RTTI_DISABLED
  /* Suppress typeinfo variables when RTTI is disabled. */
  generate_rtti_typeinfo = rtti_enabled;
#endif /* SUPPRESS_TYPEINFO_VARIABLES_WHEN_RTTI_DISABLED */
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_RTTI */
#if VLA_ALLOWED
#if ABI_COMPATIBILITY_VERSION < 306
  if (!C_mode() && vla_enabled) {
    /* The ABI previous to 3.06 could not support exception handling for
       VLAs.  We turn them off in C++ even if exceptions are not enabled to
       avoid confusion. */
    if (option_kind_used[(int)optk_vla]) {
      command_line_error(ec_cl_vla_option_only_in_C);
    }  /* if */
    vla_enabled = FALSE;
  }  /* if */
#endif /* ABI_COMPATIBILITY_VERSION < 306 */
#endif /* VLA_ALLOWED */
#if ABI_COMPATIBILITY_VERSION < 402
  if (cpp11_sfinae_enabled) {
    /* The ABI previous to 4.2 could not support SFINAE mangling; silently
       disable this option unless it was explicitly specified, in which case
       we give an error. */
    if (option_kind_used[(int)optk_cpp11_sfinae] ||
        option_kind_used[(int)optk_cpp11_mode]) {
      command_line_error(ec_sfinae_requires_newer_abi_version);
    }  /* if */
    cpp11_sfinae_enabled = FALSE;
  }  /* if */
#if NEED_NAME_MANGLING && MICROSOFT_EXTENSIONS_ALLOWED
  if (cppcli_enabled) {
    /* The ABI previous to 4.2 could not support C++/CLI mangling; silently
       disable this option unless it was explicitly specified, in which case
       we give an error. */
    if (option_kind_used[(int)optk_cppcli]) {
      command_line_error(ec_cppcli_requires_newer_abi_version);
    }  /* if */
    cppcli_enabled = FALSE;
  }  /* if */
#endif /* NEED_NAME_MANGLING && MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* ABI_COMPATIBILITY_VERSION < 402 */
#if ABI_COMPATIBILITY_VERSION < 407 && DO_IL_LOWERING && GENERATE_EH_TABLES
  if (delegating_constructors_enabled && exceptions_enabled &&
      !suppress_il_lowering) {
    /* The implementation of exception handling for delegating constructors
       relies on a run-time library change that is present only in version 4.7
       of the run-time library. */
    if (option_kind_used[(int)optk_delegating_constructors]) {
      command_line_error(ec_delegating_constructor_requires_newer_abi_version);
    }  /* if */
    delegating_constructors_enabled = FALSE;
  }  /* if */
#endif /* ABI_COMPATIBILITY_VERSION < 407 && DO_IL_LOWERING && ... */
  if (constexpr_enabled && max_cost_constexpr_call == 0) {
    max_cost_constexpr_call = DEFAULT_MAX_COST_CONSTEXPR_CALL;
  }  /* if */
  /* warning_on_for_init_difference may be TRUE only if the new for-init
     scoping rules are in effect. */
  if (use_nonstandard_for_init_scope) warning_on_for_init_difference = FALSE;
  /* Choose the style of preprocessing.  PCC preprocessing is always done
     in PCC mode, and may also be done in other modes if specified
     by a command line option. */
  pcc_preprocessing_mode = (C_dialect == C_dialect_pcc) ||
                           old_style_preprocessing;
#if OLD_STYLE_PREPROCESSING_IN_CFRONT_MODE
  if (any_cfront_mode()) {
    /* When configured that way, use old-style preprocessing for cfront
       compatibility mode. */
    pcc_preprocessing_mode = TRUE;
  }  /* if */
#endif /* OLD_STYLE_PREPROCESSING_IN_CFRONT_MODE */
  if (!option_kind_used[optk_token_separators_in_pp_output]) {
    /* In PCC preprocessing mode no token separators are emitted (normally
       these make sure that the preprocessor output contains the same
       sequence of tokens as its input). */
    no_token_separators_in_pp_output = pcc_preprocessing_mode;
  }  /* if */
  if (building_runtime) {
    /* Enable support for _Float16. */
    float16_enabled = TRUE;
  }  /* if */
#if FLOAT80_ENABLING_POSSIBLE
  if (building_runtime) {
    /* Make sure __float80 is enabled if the front end supports it and we're
       building the runtime library. */
    float80_enabled = TRUE;
  }  /* if */
#endif /* FLOAT80_ENABLING_POSSIBLE */
#if FLOAT128_ENABLING_POSSIBLE
  if (building_runtime) {
    /* Make sure __float128 is enabled if the front end supports it and we're
       building the runtime library. */
    float128_enabled = TRUE;
  }  /* if */
#endif /* FLOAT128_ENABLING_POSSIBLE */
#if RUNTIME_SUPPORTS_SIZED_DEALLOCATION
  if (building_runtime) {
    if (cpp14_mode) {
      /* The runtime relies on feature test macros being enabled. */
      check_assertion(define_portable_feature_test_macros);
    } else {
      /* C++14 mode must be enabled when building a runtime library that
         supports the sized global deallocation feature. */
      command_line_error(ec_cl_must_specify_cpp14_mode);
    }  /* if */
  }  /* if */
#else /* !RUNTIME_SUPPORTS_SIZED_DEALLOCATION */
  check_assertion(!sized_deallocation_enabled);
#endif /* RUNTIME_SUPPORTS_SIZED_DEALLOCATION */
#if CPP11_IL_EXTENSIONS_SUPPORTED
  if (building_runtime && !(cpp11_mode || cpp14_mode)) {
    /* If the front end is configured to allow C++11 mode constructs, the
       runtime library must be built to handle it. */
    command_line_error(ec_cl_must_specify_cpp11_mode);
  }  /* if */
#else /* !CPP11_IL_EXTENSIONS_SUPPORTED */
#if DO_IL_LOWERING
  /* If the back end doesn't support range-based-for, make sure we're
     lowering. */
  range_based_for_enabled = range_based_for_enabled && !suppress_il_lowering;
#else /* !DO_IL_LOWERING */
  /* No lowering and no back end support. */
  range_based_for_enabled = FALSE;
#endif /* DO_IL_LOWERING */
  /* Verify that no feature requiring C++11 back end support is enabled. */
  check_assertion(!(cpp11_mode || static_assert_enabled || lambdas_enabled ||
                    rvalue_references_enabled || nullptr_enabled));
#endif /* CPP11_IL_EXTENSIONS_SUPPORTED */
  if (cpp23_mode) {
    /* C++23 requires that nonstatic data members be allocated in declaration
       order. */
    if (!targ_field_alloc_sequence_equals_decl_sequence) {
      a_source_position  cmd_line_pos;
      set_position_to(cmd_line_pos, 0, SP_COL_CMD_LINE);
      pos_diagnostic(es_discretionary_error,
                     ec_out_of_order_field_allocation_nonstandard,
                     &cmd_line_pos);
    }  /* if */
  }  /* if */
  if (assignment_to_this_allowed && (cpp11_mode || exceptions_enabled)) {
    /* Silently disable the assignment to "this" anachronism in C++11 mode
       (it is incompatible with lowering of delegating constructors), or
       whenever exceptions are allowed. */
    assignment_to_this_allowed = FALSE;
  }  /* if */
#if !ASSIGNMENT_TO_THIS_ALLOWED
  check_assertion(!assignment_to_this_allowed);
#endif /* !ASSIGNMENT_TO_THIS_ALLOWED */
  /* Range-based-for relies on the std namespace being enabled. */
  check_assertion(namespaces_enabled || !range_based_for_enabled);
  /* Add the default directories to the end of the include search path.
     The list is then any -I directories, in the order they were specified,
     and the default directories at the end. */
  add_default_include_search_path(&incl_search_path, &end_incl_search_path);
  if (gnu_mode && gnu_version >= 30300) {
    /* In GNU mode, if a -I option specifies a name specified by a
       --sys_include, the -I is ignored. */
    remove_duplicate_include_dirs(&include_path_boundary,
                                  /*sys_includes_only=*/TRUE);
    remove_duplicate_include_dirs(&include_path_boundary,
                                  /*sys_includes_only=*/FALSE);
  }  /* if */
  /* If there was a -I- option, the system include search path starts at
     the indicated point.  Otherwise, the system include search path is
     the same as the normal search path. */
  if (include_path_boundary != NULL) {
    sys_incl_search_path = include_path_boundary->next;
  } else {
    sys_incl_search_path = incl_search_path;
  }  /* if */
  /* Pick up the source file name. */
  if (opt_ind >= argc) {
    /* No source file name is given. */
    if (source_file_name_optional) {
      exit_compilation(es_none);
    } else {
      command_line_error(ec_cl_missing_source_file_name);
    }  /* if */
  }  /* if */
  opt_arg = argv[opt_ind++];
  /* If the name is "-", use stdin for input. */
  if (strcmp(opt_arg, "-") == 0) opt_arg = FILE_NAME_FOR_STDIN;
  primary_source_file_name = file_name_from_opt_arg(opt_arg);
  if (put_dir_of_each_opened_source_file_on_incl_search_path) {
    /* Add the directory of the source file to the front of the include file
       search path.  gs_directory_of returns the directory part of the
       name allocated in general (not IL) storage. */
    /* If you change this, see the similar code in get_next_source_file. */
#ifdef USING_PURIFY
    /* This directory name is, under certain conditions, discarded later
       in the compilation process.  Save a pointer here to prevent
       Purify from complaining about the leaked memory. */
    static char	*dir_name;
#else /* !USING_PURIFY */
    char	*dir_name;
#endif /* USING_PURIFY */
    dir_name = gs_directory_of(primary_source_file_name);
    dir_name_of_primary_source_file = dir_name;
    add_to_front_of_include_search_path(dir_name, &incl_search_path,
                                        &end_incl_search_path);
    add_to_front_of_include_search_path(dir_name, &embed_search_path,
                                        &end_embed_search_path);
  } else {
    add_to_front_of_include_search_path("", &embed_search_path,
                                        &end_embed_search_path);
  }  /* if */
#if BACK_END_IS_CP_GEN_BE && AUTOMATIC_TEMPLATE_INSTANTIATION
  /* The C++-generating back end can't handle instantiations of exported
     templates. */
  export_template_allowed = FALSE;
#endif /* BACK_END_IS_CP_GEN_BE && AUTOMATIC_TEMPLATE_INSTANTIATION */
#if COMPILE_MULTIPLE_SOURCE_FILES || COMPILE_MULTIPLE_TRANSLATION_UNITS
  /* Multiple source files can be compiled.  Save the count and argv
     position of remaining files, if any. */
  argc_file_list = argc - opt_ind;
  argv_file_list = &argv[opt_ind];
#if COMPILE_MULTIPLE_SOURCE_FILES
  more_than_one_source_file = (argc_file_list > 0);
  if (more_than_one_source_file) {
    /* There are at least two files.  The -o, -L, and -X options (those
       that specify output files) cannot be used, because they only specify
       one file. */
    if (ofile_name != NULL || f_raw_listing != NULL || f_xref_info != NULL) {
      command_line_error(ec_cl_output_file_incompatible_with_multiple_inputs);
    }  /* if */
#if !USE_MMAP_FOR_MEMORY_REGIONS
    /* The preallocation mechanism can't be used with multiple source file
       compilation. */
    if (precompiled_header_processing_required) {
      command_line_error(ec_cl_pch_incompatible_with_multiple_inputs);
    }  /* if */
#endif /* !USE_MMAP_FOR_MEMORY_REGIONS */
#if ONE_INSTANTIATION_PER_OBJECT
    if (one_instantiation_per_object) {
      command_line_error(
         ec_cl_one_instantiation_per_object_incompatible_with_multiple_inputs);
    }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if AUTOMATIC_TEMPLATE_INSTANTIATION
    if (ii_file_name != NULL) {
      command_line_error(ec_cl_ii_file_name_incompatible_with_multiple_inputs);
    }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
  }  /* if */
#endif /* COMPILE_MULTIPLE_SOURCE_FILES */
#if COMPILE_MULTIPLE_TRANSLATION_UNITS
  more_than_one_non_export_translation_unit = (argc_file_list > 0);
  if (more_than_one_non_export_translation_unit) {
    /* Multiple translation units were specified.  Check for any options that
       cannot be used when using multiple translation units. */
    if (list_makefile_dependencies) {
      command_line_error(
          ec_cl_list_make_dependencies_incompatible_with_multiple_trans_units);
    }  /* if */
    if (list_macro_definitions) {
      command_line_error(
                     ec_cl_list_macros_incompatible_with_multiple_trans_units);
    }  /* if */
    if (generate_pp_output) {
      command_line_error(
                       ec_cl_pp_output_incompatible_with_multiple_trans_units);
    }  /* if */
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
    if (option_kind_used[(int)optk_implicit_template_inclusion] &&
        implicit_template_inclusion_mode) {
      /* The option --implicit_include was used: it cannot be used when
         compiling multiple translation units. */
      command_line_error(
                ec_cl_implicit_include_incompatible_with_multiple_trans_units);
    }  /* if */
    /* Disable implicit inclusion when using multiple translation units. */
    implicit_template_inclusion_mode = FALSE;
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
  }  /* if */
#endif /* COMPILE_MULTIPLE_TRANSLATION_UNITS */
#else /* !(COMPILE_MULTIPLE_SOURCE_FILES ||
         COMPILE_MULTIPLE_TRANSLATION_UNITS) */
  /* Multiple source files cannot be compiled. */
  /* Check that all command-line arguments were taken. */
  if (opt_ind < argc) {
    command_line_error(ec_cl_too_many_arguments);
  }  /* if */
#endif /* COMPILE_MULTIPLE_SOURCE_FILES ||
          COMPILE_MULTIPLE_TRANSLATION_UNITS */

  /* The -o option controls either the name of the preprocessing output
     file or the name of the IL file, depending on the kind of compilation. */
  if (do_preprocessing_only) {
    /* Since preprocessing output is being generated, the output file
       name can be specified by a -o option. */
    pp_file_name = ofile_name;
    pp_output_file_needed = TRUE;
    ofile_name = NULL;
#if IL_SHOULD_BE_WRITTEN_TO_FILE
    il_file_name = NULL;
  } else {
    /* Establish the intermediate file name (if it is needed). */
    if (!suppress_il_file_write) {
      /* The intermediate language should be written to a file; the file
         name can be specified by a -o option. */
      il_file_name = ofile_name;
      ofile_name = NULL;
    }  /* if */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
  }  /* if */
  if (list_macro_definitions) {
    /* --list_macros supersedes -E/-P. */
    generate_pp_output = FALSE;
  }  /* if */
  if (do_preprocessing_only && suppress_do_preprocessing_only) {
    /* The implicit setting of do_preprocessing_only can be overridden
       by the --no_preproc_only command line option.  Note that this is tested
       after setting the pp_file_name above so that the -o option specifies
       the pp_file_name even if a full compilation is being done. */
    do_preprocessing_only = FALSE;
  }  /* if */
  if (do_preprocessing_only) {
    /* Doing preprocessing only suppresses running the back end (and
       generating an IL file). */
    suppress_back_end = TRUE;
#if IL_SHOULD_BE_WRITTEN_TO_FILE
    suppress_il_file_write = TRUE;
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
#if DO_IL_LOWERING
    suppress_il_lowering = TRUE;
#endif /* DO_IL_LOWERING */
    if ((list_makefile_dependencies || list_included_files ||
         list_macro_definitions) &&
        error_threshold == es_warning &&
        do_preprocessing_only) {
      /* When preprocessing only to list makefile dependencies,
         included files, or macros, suppress warnings. */
      error_threshold = es_discretionary_error;
    }  /* if */
  }  /* if */
#if DIRECT_ERROR_OUTPUT_TO_STDOUT
  if (microsoft_mode && !do_preprocessing_only &&
      !option_kind_used[(int)optk_stderr_file_name]) {
    /* In Microsoft mode, when not doing only preprocessing, error output
       is directed to stdout instead of stderr.  Don't do this if an
       alternate error output file was specified.  This is done here,
       and not in set_microsoft_mode_flags, so that any errors issued
       during command-line processing will go to stderr. */
    f_error = stdout;
  }  /* if */
#endif /* DIRECT_ERROR_OUTPUT_TO_STDOUT */
  /* When name mangling is being done, name references are needed for entities
     from template deduction contexts. */
  create_template_deduction_name_references = NEED_NAME_MANGLING;
  /* If the -o option appeared, its file should have been taken for
     something. */
  if (ofile_name != NULL) {
    command_line_error(ec_cl_no_output_file_needed);
  }  /* if */
  /* Now that any command-line errors have been diagnosed, open files
     specified on the command-line. */
  if (listing_file_name != NULL) {
    f_raw_listing = open_output_file_with_error_handling(
                         listing_file_name, /*binary_file=*/FALSE,
                         /*update_mode=*/FALSE, OFF_COMMAND_LINE,
                         ec_raw_listing);
  }  /* if */
  if (xref_file_name != NULL) {
    f_xref_info = open_output_file_with_error_handling(
                         xref_file_name, /*binary_file=*/FALSE,
                         /*update_mode=*/FALSE, OFF_COMMAND_LINE,
                         ec_cross_reference);
  }  /* if */
  if (error_file_name != NULL) {
    /* The error file should be opened last so that any errors from the
       other file opens will be directed to the old error output file. */
    f_error = open_output_file_with_error_handling(
                         error_file_name, /*binary_file=*/FALSE,
                         /*update_mode=*/FALSE, OFF_COMMAND_LINE, ec_error);
#if DEBUG
    /* Direct debug output to the new error output file. */
    f_debug = f_error;
#endif /* DEBUG */
  }  /* if */
  if (option_kind_used[(int)optk_generate_raw_listing]) {
    /* Silently disable annotation of diagnostics (and thus colorization) if
       a raw listing file is specified. */
    colorize_diagnostics = FALSE;
    annotate_diagnostics = FALSE;
  } else if (colorize_diagnostics) {
    /* Must be initialized only after f_error is properly set. */
    init_colorization();
  }  /* if */
  /* Set the predefined macro mode values based on the command-line options
     used. */
  set_predef_macro_mode(pmm_gnu, gnu_mode && !clang_mode);
  set_predef_macro_mode(pmm_gcc, gcc_mode && !clang_mode);
  set_predef_macro_mode(pmm_gpp, gpp_mode && !clang_mode);
  set_predef_macro_mode(pmm_clang, clang_mode);
  set_predef_macro_mode(pmm_clang_c, clang_mode && C_mode());
  set_predef_macro_mode(pmm_clang_cpp, clang_mode && !C_mode());
  set_predef_macro_mode(pmm_gnu_or_clang, gnu_mode || clang_mode);
  set_predef_macro_mode(pmm_microsoft, ms_extensions);
  set_predef_macro_mode(pmm_cpp, !C_mode());
  set_predef_macro_mode(pmm_strict, strict_ansi_mode);
  set_predef_macro_mode(pmm_all, TRUE);
#if BACK_END_IS_CP_GEN_BE
  /* In some configurations, the language dialect generated by the C++-
     generating back end is tied to the source language selection. */
  select_cp_gen_be_target_dialect();
#endif /* BACK_END_IS_CP_GEN_BE */
  /* Only one C-style inlining mode can be in effect. */
  check_assertion(!(std_c99_inlining && gnu_c89_inlining));
#if GNU_EXTENSIONS_ALLOWED
  if (gnu_mode && !option_kind_used[(int)optk_strict_gnu]) {
    /* Emulate -std=gnu* if we're running in GNU/Clang emulation mode and
       no --c* or --[no_]strict_gnu command-line options were given (which
       matches gcc/clang behavior). */
    if (c_mode_specified() || cpp_mode_specified()) {
      strict_gnu = TRUE;
    } else {
      strict_gnu = FALSE;
    }  /* if */
  }  /* if */
#if C99_IL_EXTENSIONS_SUPPORTED
  if (gnu_mode &&
      (clang_mode || C_mode() || gpp_version_is(<40800) ||
       !strict_gnu || !user_defined_literals_enabled)) {
    /* Early versions of GNU enable imaginary literals (e.g., "1.0i"), as
       do later versions when -std=gnu* is specified.  Clang accepts
       these in all modes. */
    gnu_imaginary_literals_allowed = TRUE;
  }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  if (!strict_gnu && gcc_version_is(>= 50000)) {
    /* In default and std=gnu* modes, gcc versions beginning with 5.1
       accept raw string literals in C mode.  We had to wait until the
       value of strict_gnu was set above to make that determination here. */
    raw_string_literals_enabled = TRUE;
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
}  /* proc_command_line */

#if COMPILE_MULTIPLE_TRANSLATION_UNITS

void proc_secondary_translation_units(void)
/*
This is a temporary routine for testing of the routines that handle
multiple translation units.

Call the translation unit routine for the secondary translation units.
*/
{
  a_const_char *file_name;

  /* Make sure the same file name was not specified more than once.
     Duplicates are permitted in trans_unit_test_mode (because that is
     really the whole point of that mode). */
  if (!trans_unit_test_mode) check_for_duplicated_file_names();
  while (argc_file_list > 0) {
    /* There is another file. */
    argc_file_list--;
    file_name = file_name_from_opt_arg(*argv_file_list++);
    if (put_dir_of_each_opened_source_file_on_incl_search_path) {
      /* Update the first entry of the include file search list, the one
         that contains the directory of the primary source file. */
      /* If you change this, see the similar code in proc_command_line. */
      dir_name_of_primary_source_file = 
                                    gs_directory_of(file_name);
      change_primary_include_search_dir(dir_name_of_primary_source_file);
    }  /* if */
    process_translation_unit(file_name, /*is_primary=*/FALSE,
                             (an_exported_template_file_ptr)NULL);
  }  /* while */
}  /* proc_secondary_translation_units */

#endif /* COMPILE_MULTIPLE_TRANSLATION_UNITS */

#if COMPILE_MULTIPLE_SOURCE_FILES

a_boolean get_next_source_file(void)
/*
Check to see if there is another source input file on the command line.
If not, return FALSE.  If so, return TRUE and also set
primary_source_file_name appropriately.
This routine is only called for file names after the first;
proc_command_line handles the first file directly.
*/
{
  a_boolean another_file;

  another_file = (argc_file_list > 0);
  if (another_file) {
    /* There is another file. */
    argc_file_list--;
    primary_source_file_name = file_name_from_opt_arg(*argv_file_list++);
    if (put_dir_of_each_opened_source_file_on_incl_search_path) {
      /* Update the first entry of the include file search list, the one
         that contains the directory of the primary source file. */
      /* If you change this, see the similar code in proc_command_line. */
      dir_name_of_primary_source_file = 
                                    gs_directory_of(primary_source_file_name);
      change_primary_include_search_dir(dir_name_of_primary_source_file);
    }  /* if */
#if IL_SHOULD_BE_WRITTEN_TO_FILE
    il_file_name = NULL;
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
    pp_file_name = NULL;
    f_raw_listing = f_xref_info = NULL;
  }  /* if */
  return another_file;
}  /* get_next_source_file */

#endif /* COMPILE_MULTIPLE_SOURCE_FILES */

static void cmd_line_static_var_init(void)
/*
Initialize static variables declared in this file and external
variables declared in cmd_line.h.
*/
{
  /* Static variables declared in this file. */
  option_descriptions_used = 0;
  opt_ind = 1;
  last_defs_from_cmd_line = NULL;
  constexpr_diag_tags = NULL;
  last_constexpr_diag_tag = NULL;
  optchar = NULL;
  memzero((a_void_ptr)option_kind_used, sizeof(option_kind_used));
  old_style_preprocessing = FALSE;
#if COMPILE_MULTIPLE_SOURCE_FILES || COMPILE_MULTIPLE_TRANSLATION_UNITS
  argc_file_list = 0;
  argv_file_list = NULL;
#endif /* COMPILE_MULTIPLE_SOURCE_FILES ||
          COMPILE_MULTIPLE_TRANSLATION_UNITS */
  /* External variables declared in cmd_line.h. */
  strict_ansi_mode = FALSE;
  cfront_2_1_mode = FALSE;
  cfront_3_0_mode = FALSE;
  trans_unit_test_mode = FALSE;
  pcc_preprocessing_mode = FALSE;
  no_token_separators_in_pp_output = FALSE;
  allow_anachronisms = DEFAULT_ALLOW_ANACHRONISMS;
  allow_nonconst_call_anachronism = DEFAULT_ALLOW_NONCONST_CALL_ANACHRONISM;
  assignment_to_this_allowed = ASSIGNMENT_TO_THIS_ALLOWED;
#if DEBUG
  init_debug_level = 0;
#endif /* DEBUG */
  do_preprocessing_only = FALSE;
  pp_output_file_needed = FALSE;
  generate_pp_output = FALSE;
  keep_comments_in_pp_output = FALSE;
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
  gen_old_style_line_dirs = FALSE;
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
  gen_edg_special_types = FALSE;
  gen_line_info_in_pp_output = FALSE;
  f_pp_output = NULL;
  pp_file_name = NULL;
  list_included_files = FALSE;
  list_makefile_dependencies = FALSE;
  list_macro_definitions = FALSE;
  collect_top_templates = FALSE;
  top_templates_count = 0;
  f_raw_listing = NULL;
  f_xref_info = NULL;
  suppress_back_end = FALSE;
#if DO_IL_LOWERING
  suppress_il_lowering = FALSE;
#endif /* DO_IL_LOWERING */
#if NEED_IL_DISPLAY
  il_display = FALSE;
#endif /* NEED_IL_DISPLAY */
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  suppress_il_file_write = FALSE;
  skip_il_read = FALSE;
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
  mod_map = NULL;
  header_unit_map = NULL;
  header_unit_quote_map = NULL;
  header_unit_angle_map = NULL;
  virtual_function_table_definition = vfd_normal;
  suppress_used_before_set_warnings = FALSE;
  addr_of_bit_field_allowed = ADDR_OF_BIT_FIELD_ALLOWED;
  bit_field_promotion_applies_to_some_operations = TRUE;
  exceptions_enabled = DEFAULT_EXCEPTIONS_ENABLED;
  noexcept_enabled = FALSE;
  implicit_noexcept_enabled = FALSE;
  core_constant_expr_is_noexcept = FALSE;
  exc_spec_in_func_type = FALSE;
  deduction_from_exc_spec_allowed = TRUE;
  delegating_constructors_enabled = FALSE;
  inheriting_constructors_enabled = FALSE;
  constexpr_enabled = FALSE;
  consteval_enabled = FALSE;
  constinit_enabled = FALSE;
  relaxed_constexpr_enabled = FALSE;
  local_static_constexpr_enabled = FALSE;
  reference_to_unknown_object_allowed = FALSE;
  constexpr_virtual_enabled = FALSE;
  constexpr_try_enabled = FALSE;
  constexpr_dynamic_alloc_enabled = FALSE;
  constexpr_implies_const = TRUE;
  adl_for_non_visible_templates = FALSE;
  relaxed_typename_enabled = FALSE;
  relaxed_specialization_access_checking = FALSE;
  pack_init_capture_enabled = FALSE;
  using_enum_enabled = FALSE;
  aggregate_ctad_enabled = FALSE;
  alias_ctad_enabled = FALSE;
  inheriting_ctor_ctad_enabled = FALSE;
  user_defined_literals_enabled = FALSE;
  macro_preempts_udl_suffix = FALSE;
  raw_string_literals_enabled = FALSE;
  digit_separators_enabled = FALSE;
  rtti_enabled = 
#if RTTI_ENABLING_POSSIBLE
                 DEFAULT_RTTI_ENABLED;
#else /* !RTTI_ENABLING_POSSIBLE */
                 FALSE;
#endif /* RTTI_ENABLING_POSSIBLE */
#if DO_IL_LOWERING && ABI_CHANGES_FOR_RTTI
  generate_rtti_typeinfo = TRUE;
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_RTTI */
  array_new_and_delete_enabled =
#if ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE
                                 DEFAULT_ARRAY_NEW_AND_DELETE_ENABLED;
#else /* !ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE */
                                 FALSE;
#endif /* ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE */
  explicit_keyword_enabled = DEFAULT_EXPLICIT_KEYWORD_ENABLED;
  conditional_explicit_enabled = FALSE;
  namespaces_enabled = DEFAULT_NAMESPACES_ENABLED;
  implicit_using_std = DEFAULT_IMPLICIT_USING_STD;
  typename_enabled = DEFAULT_TYPENAME_ENABLED;
  implicit_typename_enabled = DEFAULT_IMPLICIT_TYPENAME_ENABLED;
  force_implicit_typename = FALSE;
  extern_inline_allowed = DEFAULT_EXTERN_INLINE_ALLOWED;
  inline_variables_allowed = FALSE;
  inline_variables_in_comdat = 
                    /*lint -e(506)*/LINKER_CAN_DISCARD_DUPLICATE_DEFINITIONS ||
                    /*lint -e(506)*/IA64_ABI;
  overaligned_allocation_enabled = FALSE;
  destroying_operator_delete_enabled = FALSE;
  floating_point_template_parameters_allowed =
                            DEFAULT_FLOATING_POINT_TEMPLATE_PARAMETERS_ALLOWED;
  vla_enabled = DEFAULT_VLA_ENABLED;
  vla_deallocations_in_il = VLA_DEALLOCATIONS_IN_IL;
  operator_overloading_on_enums_enabled =
                                         DEFAULT_OPERATOR_OVERLOADING_ON_ENUMS;
  string_literals_are_const = DEFAULT_STRING_LITERALS_ARE_CONST;
  deprecated_string_literal_conv_allowed = string_literals_are_const;
  class_name_injection_enabled = DEFAULT_CLASS_NAME_INJECTION;
  arg_dependent_lookup_enabled = DEFAULT_ARG_DEPENDENT_LOOKUP;
  friend_class_injection_enabled = DEFAULT_FRIEND_INJECTION;
  friend_function_injection_enabled = DEFAULT_FRIEND_INJECTION;
  do_dependent_name_processing = DEFAULT_DEPENDENT_NAME_PROCESSING;
  dependent_lookup_finds_static_functions =
                               DEFAULT_DEPENDENT_LOOKUP_FINDS_STATIC_FUNCTIONS;
  gpp_dependent_name_lookup = FALSE;
  gpp_using_directive_lookup = FALSE;
  parameters_visible_late = FALSE;
  gnu_namespace_and_class_in_same_scope = FALSE;
  friend_class_decl_can_find_using_dir = FALSE;
  nonclass_prototype_instantiations = DEFAULT_DEPENDENT_NAME_PROCESSING;
  defer_function_prototype_instantiations = FALSE;
  stricter_template_checking = FALSE;
  suppress_deferral_on_partial_spec_members =
                             DEFAULT_SUPPRESS_DEFERRAL_ON_PARTIAL_SPEC_MEMBERS;
  always_delay_field_initializer_processing = FALSE;
  nonstandard_instantiation_lookup_enabled =
                                      DEFAULT_NONSTANDARD_INSTANTIATION_LOOKUP;
  nonstandard_using_decl_allowed = DEFAULT_NONSTANDARD_USING_DECL_ALLOWED;
  designators_allowed = DEFAULT_DESIGNATORS_ALLOWED;
  extended_designators_allowed = DEFAULT_EXTENDED_DESIGNATORS_ALLOWED;
  cpp20_designators_restriction = FALSE;
  allow_parenthesized_aggregate_init = FALSE;
  variadic_macros_allowed = DEFAULT_VARIADIC_MACROS_ALLOWED;
  extended_variadic_macros_allowed = DEFAULT_EXTENDED_VARIADIC_MACROS_ALLOWED;
  pragma_operator_allowed = FALSE;
  compound_literals_allowed = DEFAULT_COMPOUND_LITERALS_ALLOWED;
  fixed_point_enabled =
#if FIXED_POINT_ALLOWED
		       DEFAULT_FIXED_POINT_ENABLED;
#else /* !FIXED_POINT_ALLOWED */
		       FALSE;
#endif /* FIXED_POINT_ALLOWED */
#if NAMED_ADDRESS_SPACES_ALLOWED
  named_address_spaces_enabled = DEFAULT_NAMED_ADDRESS_SPACES_ENABLED;
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
#if NAMED_REGISTERS_ALLOWED
  named_registers_enabled = DEFAULT_NAMED_REGISTERS_ENABLED;
#endif /* NAMED_REGISTERS_ALLOWED */
  register_is_deprecated = FALSE;
  register_is_disallowed = FALSE;
  operator_bool_increment_allowed = TRUE;
  using_attribute_namespaces_enabled = FALSE;
#if DO_IL_LOWERING
  pointer_to_member_call_optimization_allowed =
                           DEFAULT_POINTER_TO_MEMBER_CALL_OPTIMIZATION_ALLOWED;
#endif /* DO_IL_LOWERING */
  no_access_check_on_friend_declarator_ids =
                              DEFAULT_NO_ACCESS_CHECK_ON_FRIEND_DECLARATOR_IDS;
  special_subscript_cost = DEFAULT_SPECIAL_SUBSCRIPT_COST;
  long_preserving_rules = DEFAULT_LONG_PRESERVING_RULES;
  type_keyword_in_dtor_allowed = FALSE;
  /* Core issue 727 added in-class explicit specializations, adopted for the
     C++17 Standard and as a Defect Report against previous versions. */
  allow_in_class_specializations = TRUE;
  allow_in_class_instantiations = FALSE;
  record_form_of_name_reference = DEFAULT_RECORD_FORM_OF_NAME_REFERENCE;
  defs_from_cmd_line = NULL;
  allow_dollar_in_id_chars = DEFAULT_ALLOW_DOLLAR_IN_ID_CHARS;
  display_compilation_time = FALSE;
  instantiation_mode = DEFAULT_INSTANTIATION_MODE;
  instantiate_before_pch_creation = INSTANTIATE_BEFORE_PCH_CREATION;
  null_template_ptr_arg_enabled = FALSE;
  template_linkage_depends_on_instantiation_args = TRUE;
  no_very_expensive_checking = FALSE;
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  automatic_instantiation_mode = DEFAULT_AUTOMATIC_INSTANTIATION_MODE;
  suppress_instantiation_flags = FALSE;
  ii_file_name = NULL;
  instantiation_flags_in_template_info_file =
                                     INSTANTIATION_FLAGS_IN_TEMPLATE_INFO_FILE;
  use_template_info_file = USE_TEMPLATE_INFO_FILE;
  template_info_file_name = NULL;
  exported_template_file_name = NULL;
  definition_list_file_name = NULL;
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
  implicit_template_inclusion_mode = DEFAULT_IMPLICIT_TEMPLATE_INCLUSION_MODE;
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
  display_error_number = DEFAULT_DISPLAY_ERROR_NUMBER;
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
  gen_c_file_name = NULL;
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
  precompiled_header_processing_required = FALSE;
  create_precompiled_header = FALSE;
  use_precompiled_header = FALSE;
  pch_input_file_name = NULL;
  pch_output_file_name = NULL;
  automatic_pch_processing = FALSE;
  suppress_pch_messages = FALSE;
  verbose_pch_messages = FALSE;
#if USE_FIXED_ADDRESS_FOR_MMAP
  fixed_address_for_mmap = (char *)FIXED_ADDRESS_FOR_MMAP;
#endif /* USE_FIXED_ADDRESS_FOR_MMAP */
#if !USE_MMAP_FOR_MEMORY_REGIONS
  pch_mem_size = 0;
#endif /* !USE_MMAP_FOR_MEMORY_REGIONS */
  pch_dir_name = NULL;
  c11_atomic_enabled = FALSE;
  create_module_unit = FALSE;
  created_module_unit_is_internal = FALSE;
  created_module_unit_is_header = FALSE;
  module_unit_output_file_name = NULL;
  restrict_enabled = FALSE;
  restrict_keyword_enabled = DEFAULT_RESTRICT_ENABLED;
  noreturn_keyword_enabled = FALSE;
  gnu_restrict_keyword_enabled = FALSE;
  nonstd_gnu_keywords_enabled = FALSE;
  long_lifetime_temps = FALSE;
  explicit_conversion_functions_enabled = FALSE;
  explicit_enum_base_enabled = FALSE;
  report_explicit_enum_base_as_nonstandard = FALSE;
  enum_qualifiers_enabled = FALSE;
  opaque_enum_decls_enabled = FALSE;
  lambdas_enabled = DEFAULT_LAMBDAS_ENABLED;
  lambda_default_args_enabled = FALSE;
  generic_lambdas_enabled = FALSE;
  generic_lambdas_can_implicitly_capture = FALSE;
  init_capture_enabled = FALSE;
  rvalue_references_enabled = DEFAULT_RVALUE_REFERENCES_ENABLED;
  ref_qualifiers_enabled = rvalue_references_enabled;
  rvalue_ctor_is_copy_ctor = TRUE;
  generate_move_operations = FALSE;
  local_types_as_template_args_enabled = FALSE;
  inexact_ptr_to_member_deduction_enabled = TRUE;
  decls_using_types_without_linkage_allowed = FALSE;
  unrestricted_unions_enabled = FALSE;
  trailing_return_types_enabled = FALSE;
  this_in_trailing_return_types_enabled = FALSE;
  list_init_enabled = FALSE;
  pre_cpp11_list_init = FALSE;
  field_initializers_enabled = FALSE;
  aggregate_classes_can_have_field_initializers = FALSE;
  aggregate_classes_can_have_bases = FALSE;
  aggregate_classes_can_have_user_ctors = TRUE;
  generalized_template_template_matching = FALSE;
  selection_from_prvalue_is_xvalue = FALSE;
  alias_declarations_enabled = FALSE;
  variadic_templates_enabled = FALSE;
  inline_namespaces_enabled = FALSE;
  std_attributes_enabled = FALSE;
  namespace_attributes_enabled = FALSE;
  nested_namespace_definitions_enabled = FALSE;
  nested_inline_namespace_definitions_enabled = FALSE;
  enumerator_attributes_enabled = FALSE;
  variable_templates_enabled = FALSE;
  constexpr_if_enabled = FALSE;
  if_consteval_enabled = FALSE;
  explicit_this_param_enabled = FALSE;
  alignas_enabled = FALSE;
  alignof_enabled = FALSE;
  pragma_pack_enabled = TRUE;
  gnu_attributes_enabled = FALSE;
  ms_declspec_attributes_enabled = FALSE;
  defaulted_special_members_enabled = FALSE;
  deleted_functions_enabled = FALSE;
  mandatory_copy_elision = FALSE;
  generalized_nontype_arguments = FALSE;
  strict_cpp17_eval_order = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED
  default_calling_convention = (a_calling_convention)cc_cdecl;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  microsoft_64bit_pointer_extensions_enabled =
                            DEFAULT_MICROSOFT_64BIT_POINTER_EXTENSIONS_ENABLED;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  allow_nonstandard_anonymous_unions =
                                    DEFAULT_ALLOW_NONSTANDARD_ANONYMOUS_UNIONS;
  allow_c11_anonymous_unions = FALSE;
  wchar_t_is_keyword =
#if WCHAR_T_ENABLING_POSSIBLE
                       DEFAULT_WCHAR_T_IS_KEYWORD;
#else /* !WCHAR_T_ENABLING_POSSIBLE */
                       FALSE;
#endif /* WCHAR_T_ENABLING_POSSIBLE */
  bool_is_keyword = 
#if BOOL_ENABLING_POSSIBLE
                    DEFAULT_BOOL_IS_KEYWORD;
#else /* !BOOL_ENABLING_POSSIBLE */
                    FALSE;
#endif /* BOOL_ENABLING_POSSIBLE */
  c99_bool_is_keyword = FALSE;
  false_literal_is_not_null_pointer_constant = FALSE;
  allow_decl_after_stmt = FALSE;
  default_max_member_alignment = 0;
  alternative_tokens_allowed = DEFAULT_ALTERNATIVE_TOKENS_ALLOWED;
  trigraphs_allowed = DEFAULT_TRIGRAPHS_ALLOWED;
#if DO_IL_LOWERING && MINIMAL_INLINING
  inlining_enabled = TRUE;
  inline_statement_limit = DEFAULT_INLINE_STATEMENT_LIMIT;
#endif /* DO_IL_LOWERING && MINIMAL_INLINING */
  SVR4_C_mode = DEFAULT_SVR4_C_MODE;
  address_of_ellipsis_allowed = DEFAULT_ADDRESS_OF_ELLIPSIS_ALLOWED;
  allow_ellipsis_only_param_in_C_mode =
                                   DEFAULT_ALLOW_ELLIPSIS_ONLY_PARAM_IN_C_MODE;
  func_prototype_tags_enabled = FALSE;
  require_func_prototypes = TRUE;
  allow_nonconst_ref_anachronism = DEFAULT_ALLOW_NONCONST_REF_ANACHRONISM;
  building_runtime = FALSE;
  remove_unneeded_entities = DEFAULT_REMOVE_UNNEEDED_ENTITIES;
  use_nonstandard_for_init_scope = DEFAULT_USE_NONSTANDARD_FOR_INIT_SCOPE;
  microsoft_type_dependent_for_init_scope = FALSE;
  warning_on_for_init_difference = DEFAULT_WARNING_ON_FOR_INIT_DIFFERENCE;
  allow_copy_assignment_op_with_base_class_param =
                        DEFAULT_ALLOW_COPY_ASSIGNMENT_OP_WITH_BASE_CLASS_PARAM;
  guiding_decls_allowed = DEFAULT_GUIDING_DECLS_ALLOWED;
  warning_on_non_template_friend = DEFAULT_WARNING_ON_NON_TEMPLATE_FRIEND;
  old_specializations_allowed = DEFAULT_OLD_SPECIALIZATIONS_ALLOWED;
  impl_conv_between_c_and_cpp_function_ptrs_allowed =
                     DEFAULT_IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_ALLOWED;
  check_printf_scanf_positional_args =
                                    DEFAULT_CHECK_PRINTF_SCANF_POSITIONAL_ARGS;
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
  multibyte_chars_in_source_enabled =
                                     DEFAULT_MULTIBYTE_CHARS_IN_SOURCE_ENABLED;
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
  null_chars_allowed_in_source = DEFAULT_NULL_CHARS_ALLOWED_IN_SOURCE;
  report_embedded_cplusplus_noncompliance = FALSE;
  report_gnu_extensions = FALSE;
  nullability_qualifiers_enabled = FALSE;
  nonstandard_qualifier_deduction = DEFAULT_NONSTANDARD_QUALIFIER_DEDUCTION;
  nonstandard_default_arg_deduction =
                                     DEFAULT_NONSTANDARD_DEFAULT_ARG_DEDUCTION;
  function_template_default_args_allowed = TRUE;
  do_late_ovl_res_tiebreaker = DEFAULT_DO_LATE_OVL_RES_TIEBREAKER;
  single_ref_qual_ovl_res_tiebreaker =
                                    DEFAULT_SINGLE_REF_QUAL_OVL_RES_TIEBREAKER;
  late_template_ovl_res_tiebreaker = TRUE;
  one_instantiation_per_object = FALSE;
#if ONE_INSTANTIATION_PER_OBJECT
  instantiation_dir_name = NULL;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  stdc_zero_in_nonstrict_mode = STDC_ZERO_IN_NONSTRICT_MODE;
  stdc_zero_in_system_headers = DEFAULT_STDC_ZERO_IN_SYSTEM_HEADERS;
  max_pending_instantiations = DEFAULT_MAX_PENDING_INSTANTIATIONS;
  max_depth_constexpr_call = DEFAULT_MAX_DEPTH_CONSTEXPR_CALL;
  max_cost_constexpr_call = 0;
#if MICROSOFT_EXTENSIONS_ALLOWED
  import_dir_name = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
  create_microsoft_if_exists_entries = FALSE;
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
  enum_types_can_be_larger_than_int = FALSE;
  enum_types_can_be_smaller_than_int = FALSE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  instantiations_permitted_in_class_src_seq_list =
                        DEFAULT_INSTANTIATIONS_PERMITTED_IN_CLASS_SRC_SEQ_LIST;
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if NEED_NAME_MANGLING && !IA64_ABI
  compress_mangled_names = DEFAULT_COMPRESS_MANGLED_NAMES;
#endif /* NEED_NAME_MANGLING && !IA64_ABI */
#if NEED_NAME_MANGLING
  max_mangled_name_length = DEFAULT_MAX_MANGLED_NAME_LENGTH;
  final_name_mangling_needed = (max_mangled_name_length != 0
#if !IA64_ABI
                                || compress_mangled_names
#endif /* !IA64_ABI */
                                                         );
#endif /* NEED_NAME_MANGLING */
  include_file_suffixes = DEFAULT_INCLUDE_FILE_SUFFIX_LIST;
  curr_cmd_line_or_predef_macro_def = NULL;
  processing_predefined_macro = FALSE;
  ignore_std_namespace = FALSE;
  end_of_line_comments_allowed = FALSE;
  flexible_array_members_allowed = FALSE;
  universal_character_names_allowed = FALSE;
  va_copy_macro_allowed = FALSE;
  long_long_is_standard = FALSE;
  long_long_promotion_allowed = FALSE;
#if INT128_EXTENSIONS_ALLOWED
  int128_extensions_enabled = FALSE;
#endif /* INT128_EXTENSIONS_ALLOWED */
  extended_float_types = FALSE;
  float16_enabled = FALSE;
  float80_enabled = FLOAT80_ENABLING_POSSIBLE;
  float128_enabled = FLOAT128_ENABLING_POSSIBLE;
  hex_floating_point_constants_allowed = FALSE;
  binary_literals_allowed = FALSE;
  deduction_guide_redeclaration_allowed = FALSE;
#if EXPORT_ENABLING_POSSIBLE
  export_template_allowed = /*lint -e(506)*/DEFAULT_EXPORT_TEMPLATE_ALLOWED &&
                            /*lint -e(506)*/(DEFAULT_CPP_MODE < 201103);
#else /* !EXPORT_ENABLING_POSSIBLE */
  /* Export is not supported by this configuration -- force it to be
     disabled. */
  export_template_allowed = FALSE;
#endif /* EXPORT_ENABLING_POSSIBLE */
  export_keyword_enabled = TRUE;
  suppress_inline_corresp_check = FALSE;
  allow_anon_types_in_anon_unions = FALSE;
#if EXPENSIVE_CHECKING
  eager_load_modules = FALSE;
#endif /* EXPENSIVE_CHECKING */
  use_nonstd_partial_ordering = FALSE;
  no_checking_pragmas = FALSE;
  no_find_pragma_validation = FALSE;
#if DEBUG
  display_space_used = FALSE;
#endif /* DEBUG */
#if IA64_ABI
  emulate_gnu_abi_bugs = DEFAULT_EMULATE_GNU_ABI_BUGS;
  emulate_unsafe_gnu_abi_bugs = FALSE;
  gnu_abi_version = DEFAULT_GNU_ABI_VERSION;
  warn_about_tail_padding_use = FALSE;
#endif /* IA64_ABI */
  IEEE_handling_on_float_operation_exceptions = TARG_HAS_IEEE_FLOATING_POINT;
#if UPC_EXTENSIONS_ALLOWED
  upc_mode = DEFAULT_UPC_MODE;
  upc_num_threads = 0;
#endif /* UPC_EXTENSIONS_ALLOWED */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  gnu_visibility_attribute_enabled = DEFAULT_GNU_VISIBILITY_ATTRIBUTE_ENABLED;
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
#if GNU_VECTOR_TYPES_ALLOWED
  permissive_gnu_vector_conversions_enabled = FALSE;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
  allow_default_arg_on_template_member_definition = FALSE;
  use_microsoft_specialization_scope = FALSE;
  elab_type_lookup_finds_typedefs = FALSE;
  value_initialization_enabled = TRUE;
  emulate_msvc_value_initialization_bugs =
                                DEFAULT_EMULATE_MSVC_VALUE_INITIALIZATION_BUGS;
  emulate_gnu_value_initialization_bugs =
                                 DEFAULT_EMULATE_GNU_VALUE_INITIALIZATION_BUGS;
  thread_local_storage_specifier_enabled =
                                DEFAULT_THREAD_LOCAL_STORAGE_SPECIFIER_ENABLED;
#if USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES
  all_thread_locals_have_wrappers = TRUE;
#endif /* USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES */
  std_thread_local_storage_specifier_enabled = FALSE;
  allow_nonconstant_auto_aggr_init_in_c_mode = FALSE;
  /* Global variables from lang_feat.h. */
#if SUN_EXTENSIONS_ALLOWED || defined(_lint)
  sun_mode
#if !SUN_EXTENSIONS_ALLOWED
           = FALSE;
#else /* SUN_EXTENSIONS_ALLOWED */
           = DEFAULT_SUN_COMPATIBILITY;
#endif /* !SUN_EXTENSIONS_ALLOWED */
  /* Ensure that we are not configured to have a "Sun C" mode by default
     (since we only have a "Sun C++" mode). */
  check_assertion(!(sun_mode && C_dialect != C_dialect_cplusplus));
#endif /* SUN_EXTENSIONS_ALLOWED || defined(_lint) */
#if SUN_EXTENSIONS_ALLOWED
  sun_linker_scope_allowed = FALSE;
#endif /* SUN_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED || defined(_lint)
  gcc_mode = FALSE;
  gpp_mode = FALSE;
  gnu_mode = FALSE;
  clang_mode = FALSE;
  strict_gnu = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED || defined(_lint) */
  gnu_version = DEFAULT_GNU_VERSION;
  clang_version = DEFAULT_CLANG_VERSION;
#if MICROSOFT_EXTENSIONS_ALLOWED
  microsoft_mode = DEFAULT_MICROSOFT_MODE;
  microsoft_bugs = DEFAULT_MICROSOFT_BUGS && microsoft_mode;  /*lint !e506*/
  cppcli_enabled = DEFAULT_CPPCLI_ENABLED && microsoft_mode;  /*lint !e506*/
  cppcx_enabled = DEFAULT_CPPCX_ENABLED && microsoft_mode;  /*lint !e506*/
  if (microsoft_mode) {
    ms_extensions = TRUE;
    ms_compat = TRUE;
  } else {
    ms_extensions = DEFAULT_MICROSOFT_EXTENSIONS;
    ms_compat = DEFAULT_MICROSOFT_COMPATIBILITY;
  }  /* if */
  cli_or_cx_enabled = cppcli_enabled || cppcx_enabled;
  mscorlib_file_name = NULL;
  vcmeta_directory_name = NULL;
  /* using_framework_directory defaults to TRUE, but has no effect unless
     cppcli_enabled is TRUE. */
  using_framework_directory = TRUE;
  generic_arity_overload_allowed = DEFAULT_GENERIC_ARITY_OVERLOAD_ALLOWED;
  disable_access_checking_in_microsoft_enum_bases =
                      DEFAULT_DISABLE_ACCESS_CHECKING_IN_MICROSOFT_ENUM_BASES;
  pending_generic_constraint_specifier_enabled = FALSE;
  msvc_lang = NULL;
  ms_cplusplus_std_value = FALSE;
  force_ms_type_info_not_in_namespace_std = FALSE;
#if WRITE_CPPCLI_PORTABLE_ASSEMBLIES
  generate_portable_assemblies = FALSE;
#endif /* WRITE_CPPCLI_PORTABLE_ASSEMBLIES */
  ms_permissive = DEFAULT_MS_PERMISSIVE;
  ms_treat_copy_init_as_direct_init = FALSE;
  for_each_statement_enabled = FALSE;
  ms_stdc = DEFINE_STDC_IN_MICROSOFT_MODE;
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#ifdef _lint
  microsoft_mode = FALSE;
  microsoft_bugs = FALSE;
  ms_extensions = FALSE;
  ms_compat = FALSE;
  ms_permissive = FALSE;
  cppcli_enabled = FALSE;
  cppcx_enabled = FALSE;
  cli_or_cx_enabled = FALSE;
  scanning_generated_code_from_metadata = FALSE;
#endif /* ifdef _lint */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  ms_strict_ternary = FALSE;
  scanning_generated_code = FALSE;
  internal_templates_enabled = FALSE;
  no_ms_nonreal_base_classes = FALSE;
  use_cppcli_fill_ins = TRUE;
  microsoft_version = DEFAULT_MICROSOFT_VERSION;
  microsoft_build_number = 99999;
  implicit_microsoft_cpp11_mode = FALSE;
  std_version = 0;
  uliterals_enabled = DEFAULT_ULITERALS_ENABLED;
  char16_t_and_char32_t_are_keywords = DEFAULT_ULITERALS_ENABLED;
  type_traits_helpers_enabled = DEFAULT_TYPE_TRAITS_HELPERS_ENABLED;
  right_shift_can_be_angle_brackets = FALSE;
  extended_friends_enabled = FALSE;
  mixed_string_concat_enabled = FALSE;
  static_assert_enabled = FALSE;
  terse_static_assert_enabled = FALSE;
  auto_type_specifier_enabled = DEFAULT_AUTO_TYPE_SPECIFIER_ENABLED;
  auto_storage_class_specifier_enabled =
                                 DEFAULT_AUTO_STORAGE_CLASS_SPECIFIER_ENABLED;
  decltype_auto_enabled = FALSE;
  extern_template_allowed = FALSE;
  inline_template_allowed = FALSE;
  standard_form_of_extern_template = FALSE;
  decltype_enabled = FALSE;
  enable_decltype_in_base_specifier_and_mem_initializer = FALSE;
  enable_underscore_decltype_only = FALSE;
  check_concatenations = DEFAULT_CHECK_CONCATENATIONS;
  equiv_typedefs_are_lookup_equivalent = TRUE;
  va_arg_returns_lvalue = FALSE;
  warn_on_try_statement = FALSE;
  nullptr_enabled = DEFAULT_NULLPTR_ENABLED;
  c23_typeof_enabled = FALSE;
  cpp11_sfinae_enabled = DEFAULT_CPP11_SFINAE_ENABLED;
  cpp11_sfinae_ignore_access = cpp11_sfinae_enabled &&
                            DEFAULT_CPP11_SFINAE_IGNORE_ACCESS; /*lint !e506*/
  diag_override_does_not_affect_sfinae =
                                  DEFAULT_DIAG_OVERRIDE_DOES_NOT_AFFECT_SFINAE;
  std_c99_inlining = FALSE;
  gnu_c89_inlining = FALSE;
  range_based_for_enabled = DEFAULT_RANGE_BASED_FOR_ENABLED;
  relaxed_range_based_for_enabled = FALSE;
  extended_range_based_for_lifetime = FALSE;
  terse_range_based_for_enabled = FALSE;
  carriage_return_is_line_terminator = FALSE;
  warning_on_lossy_conversion = DEFAULT_WARNING_ON_LOSSY_CONVERSION;
  gcc_const_variables_allowed = DEFAULT_GCC_CONST_VARIABLES_ALLOWED;
  gnu_bases_operators_enabled = FALSE;
  preserve_lvalues_with_same_type_casts = FALSE;
  std_override_modifiers_enabled = FALSE;
  define_portable_feature_test_macros = TRUE;
  sized_deallocation_enabled = FALSE;
  struct_bindings_enabled = FALSE;
  selection_initializers_enabled = FALSE;
  mangle_had_been_implicitly_const = FALSE;
  coroutines_enabled = FALSE;
  concepts_enabled = FALSE;
  abbr_func_templates_enabled = FALSE;
#if BUILTIN_FUNCTIONS_ENABLED
  builtin_functions_enabled = FALSE;
  preload_builtin_functions = FALSE;
#endif /* BUILTIN_FUNCTIONS_ENABLED */
  alias_templ_intrinsics_enabled = DEFAULT_ALIAS_TEMPL_INTRINSICS_ENABLED;
  var_templ_intrinsics_enabled = DEFAULT_VAR_TEMPL_INTRINSICS_ENABLED;
  templ_type_member_intrinsics_enabled =
                              DEFAULT_TEMPL_TYPE_MEMBER_INTRINSICS_ENABLED;
  utf8_char_literals_enabled = FALSE;
  deduced_return_types_enabled = FALSE;
  warn_on_deduced_return_types = FALSE;
  direct_init_fixed_base_enum_enabled = FALSE;
  constexpr_lambdas_enabled = FALSE;
  capture_star_this_enabled = FALSE;
  explicit_copy_this_capture_enabled = FALSE;
  lambda_template_param_list_enabled = FALSE;
  lambda_allowed_in_uneval_context = FALSE;
  ms_std_preproc = FALSE;
  ms_await = FALSE;
  ms_await_strict = FALSE;
  fold_expressions_enabled = FALSE;
#if REFLECTION_ENABLING_POSSIBLE
  reflection_enabled = DEFAULT_REFLECTION_ENABLED;
  injection_enabled = DEFAULT_INJECTION_ENABLED;
#endif /* REFLECTION_ENABLING_POSSIBLE */
  variadic_using_decls_enabled = FALSE;
  class_template_arg_deduction_enabled = FALSE;
  auto_template_params_enabled = FALSE;
  string_literal_operator_template_allowed = FALSE;
  spaceship_enabled = FALSE;
  multi_subscript_enabled = FALSE;
  static_call_operator_enabled = FALSE;
  rvalue_allowed_with_const_qual_memptr = FALSE;
  va_opt_enabled = FALSE;
  char8_t_enabled = FALSE;
  bit_precise_int_enabled = FALSE;
  init_statement_allowed_in_range_based_for = FALSE;
  relaxed_abstract_checking = FALSE;
  modules_enabled = FALSE;
  display_module_import_diagnostics = DEFAULT_MODULE_IMPORT_DIAG_ENABLED;
  module_keywords_enabled = FALSE;
  import_includes_from_header_map = FALSE;
  ignore_absolute_paths_for_header_units = FALSE;
  skip_module_imports = FALSE;
  skip_module_version_check = FALSE;
  gnu_imaginary_literals_allowed = FALSE;
#if UNICODE_VULNERABILITY_DETECTION_SUPPORTED
  check_unicode_security = FALSE;
#endif /* UNICODE_VULNERABILITY_DETECTION_SUPPORTED */
  old_id_chars = FALSE;
  output_mode = DEFAULT_OUTPUT_MODE;
  incognito = DEFAULT_INCOGNITO;
  keep_restrict_in_signatures = FALSE;
  attributes_on_using_declarations = FALSE;
  elifdef_enabled = FALSE;
  size_suffix_enabled = FALSE;
  named_unicode_chars_allowed = FALSE;
  delimited_escape_seqs_allowed = FALSE;
  lambda_attributes_allowed = FALSE;
  lambda_declarator_params_optional = FALSE;
  auto_cast_enabled = FALSE;
  embed_enabled = FALSE;
  struct_binding_packs_enabled = FALSE;
  pack_indexing_enabled = FALSE;
}  /* cmd_line_static_var_init */


void cmd_line_early_init(void)
/*
One time initialization that must take place early on in the front end.
This is done before command line processing.
*/
{
  cmd_line_static_var_init();
  memzero((char*)predef_macro_mode_values, sizeof(predef_macro_mode_values));
#if GNU_EXTENSIONS_ALLOWED
  /* Most il_header fields are initialized in fe_init.c after command-line
     processing, but a few are initialized here and potentially changed
     directly by command-line options. */
  il_header.short_enums = FALSE;
  il_header.default_nocommon = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
  C_dialect_has_been_set = FALSE;
}  /* cmd_line_early_init */

#if MAKE_FRONT_END_CALLABLE

void cmd_line_cleanup(void)
/*
This routine is called at the end of compilation, or if compilation is
terminated prematurely for some reason.  It performs any cleanup operations
required.  In particular, it closes any files that may have been open at
the point at which the compilation was terminated.
*/
{
  close_file_if_open(&f_pp_output);
  close_file_if_open(&f_raw_listing);
  close_file_if_open(&f_xref_info);
}  /* cmd_line_cleanup */

#endif /* MAKE_FRONT_END_CALLABLE */
#if GNU_EXTENSIONS_ALLOWED

a_boolean cmd_line_option_inhibits_gnu_cpp11_extension_warning(
                                                    an_error_code  error_code)
/*
The given error code corresponds to a warning that a C++11 feature is being
used in a non-C++11 GNU mode.  Return TRUE if that feature was enabled using a
specific command-line option (e.g., optk_lambdas for lambda expressions), to
indicate that no warning should be issued in that case.
*/
{
  a_boolean  result;

  switch (error_code) {
    case ec_rvalue_references_is_cpp11:
      result = option_kind_used[(int)optk_rvalue_references];
      break;
    case ec_lambdas_is_cpp11:
      result = option_kind_used[(int)optk_lambdas];
      break;
    case ec_delegating_constructor_is_cpp11:
      result = option_kind_used[(int)optk_delegating_constructors];
      break;
    case ec_unrestricted_unions_is_cpp11:
      result = option_kind_used[(int)optk_unrestricted_unions];
      break;
    default:
      result = FALSE;
  }  /* switch */
  return result;
}  /* cmd_line_option_inhibits_gnu_cpp11_extension_warning */

#endif /* GNU_EXTENSIONS_ALLOWED */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

