/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

error.h -- Declarations related to error reporting.

*/

/* Avoid including these declarations more than once: */
#ifndef ERROR_H
#define ERROR_H 1
#ifndef LANG_FEAT_H
#include "lang_feat.h"
#endif /* LANG_FEAT_H */

/* Note that an_error_severity is defined in host_envir.h. */
#ifndef HOST_ENVIR_H
#include "host_envir.h"
#endif /* ifndef HOST_ENVIR_H */

/*
Include the file that defines the enumeration an_error_code.
*/
#include "err_codes.h"

/* General utility components for headers. */
#ifndef EDG_HEADER_UTIL_H
#include "header_util.h"
#endif /* ifndef EDG_HEADER_UTIL_H */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
Diagnostic entries can be returned to routines in other files, but they
can only use those as opaque types.
*/
typedef struct a_diagnostic *a_diagnostic_ptr;
typedef struct a_template_arg *a_template_arg_ptr;

/*
Structure used to represent a list of diagnostic entries.
*/
typedef struct a_diag_list *a_diag_list_ptr;
typedef struct a_diag_list {
  a_diagnostic_ptr
		head;
			/* The start of the list. */
  a_diagnostic_ptr
		tail;
			/* The end of the list. */
  void append(a_diagnostic_ptr  dp);
} a_diag_list;


/*
Initialize the fields of the given diagnostic list entry.
*/
#define clear_diag_list(dlp) \
  { (dlp)->head = NULL; (dlp)->tail = NULL; }

inline a_boolean is_empty_diag_list(a_diag_list_ptr  dlp)
/*
Return TRUE if the given diagnostic list is empty.
*/
{
  return dlp->head == NULL;
}  /* is_empty_diag_list */


/*
Structure used to map error tags into error codes.  An array of these
entries is used.  The array is sorted by tag so that a binary search
may be used to look up a given tag.
*/
typedef struct an_error_tag_entry *an_error_tag_entry_ptr;
typedef struct an_error_tag_entry {
  a_const_char	*tag;
			/* The character string to be used as a tag
			   for a given error. */
  an_error_code	code;
			/* The error code that this tag refers to. */
} an_error_tag_entry;

EXTERN_THREAD FILE
		*f_error;
			/* The file to which error output is written. */

/*
Current error position, used as default in error reporting.  Set
implicitly to the start of a construct whenever one is scanned (e.g.,
when a token is gotten, error_position is set to the start of the
token).
*/
EXTERN_THREAD a_source_position
		error_position;

/*
A class used to represent counts of remark, warning, error, and catastrophic
error diagnostics.
*/
struct a_diagnostic_counter {
  unsigned long remarks;
  unsigned long warnings;
  unsigned long errors;
  unsigned long catastrophes;

  a_diagnostic_counter()
    : remarks(0), warnings(0), errors(0), catastrophes(0)
    {}

  unsigned long all_error_types() const
    { return errors + catastrophes; }
};  /* a_diagnostic_counter */

typedef struct a_diagnostic_counter *a_diagnostic_counter_ptr;

/*
A class used to represent all diagnostic counters associated with a given
compilation.
*/
struct a_diagnostic_counter_set {
  a_diagnostic_counter
                total;
                        /* A diagnostic counter aggregating all diagnostic
                           counts (excluding diagnostics that are suppressed
                           because they've already been encountered). */
  a_diagnostic_counter
                suppressed;
                        /* A diagnostic counter aggregating all suppressed
                           counts (excluding diagnostics that are suppressed
                           because they've already been encountered). */
  a_diagnostic_counter_ptr
                local;
                        /* A pointer to a temporary diagnostic counter
                           aggregating all diagnostic counts (reported and
                           suppressed) while it's set (excluding diagnostics
                           that are suppressed because they've already been
                           encountered). */
  a_diagnostic_counter
                repeated;
                        /* A diagnostic counter aggregating counts of all
                           diagnostics that have been previously encountered,
                           and thus were not rereported. */

  a_diagnostic_counter_set()
    : total(), suppressed(), local(NULL), repeated()
    {}
};  /* a_diagnostic_counter_set */


EXTERN_THREAD a_diagnostic_counter_set
                diagnostic_counters;
                        /* The global set of active diagnostic counters. */

EXTERN_THREAD a_boolean
                globally_suppress_diagnostics;
                        /* TRUE if encountered diagnostics should not be
                           reported or counted in the global counters.
                           Suppressed diagnostics can still be observed by
                           setting the diagnostic_counter.  Typically,
                           management of diagnostic_counter and
                           globally_suppress_diagnostics should be performed by
                           the class a_diagnostic_suppression. */

/*
A class used to temporarily suppress diagnostics and record counts of
suppressed diagnostics in the given diagnostic counter.
*/
struct a_diagnostic_suppression {
  inline a_diagnostic_suppression(
                                a_diagnostic_counter_ptr counter,
                                a_boolean                suppress_diagnostics);
private:
  Value_saver<a_diagnostic_counter_ptr>
                prev_counter;
                        /* The previous "local" diagnostic counter. */
  Value_saver<a_boolean>
                prev_suppression;
                        /* The previous "globally_suppress_diagnostics"
                           state. */
};  /* a_diagnostic_suppression */


inline a_diagnostic_suppression::a_diagnostic_suppression(
                                 a_diagnostic_counter_ptr counter,
                                 a_boolean                suppress_diagnostics)
  : prev_counter(&diagnostic_counters.local),
    prev_suppression(&globally_suppress_diagnostics)
/*
Begin a new diagnostic suppression scope managed by the associated class
instance lifetime.  Upon construction if diagnostic suppression is requested
(via a TRUE suppress_diagnostics value), set the provided counter as the
"local" diagnostic counter, and enable diagnostics suppression.  If diagnostic
suppression was not requested, construction is a no op.  Upon destruction the
previous values will always be restored.
*/
{
  if (suppress_diagnostics) {
    diagnostic_counters.local = counter;
    globally_suppress_diagnostics = TRUE;
  }  /* if */
}  /* a_diagnostic_suppression */


/*
A class used to capture the current state of the "total" diagnostic counters.
Typically paired with expect_error_since to check for new diagnostics in a
particular context.
*/
struct a_diag_count_snapshot {
  a_diag_count_snapshot()
    : captured_total_state(diagnostic_counters.total),
      captured_repeated_state(diagnostic_counters.repeated)
    {}
  a_diagnostic_counter
                captured_total_state;
                        /* The captured state of the "total" diagnostic counter
                           when this object was constructed. */
  a_diagnostic_counter
                captured_repeated_state;
                        /* The captured state of the "repeated" diagnostic
                           counter when this object was constructed. */
};  /* a_diag_count_snapshot */


inline void expect_error_since(const a_diag_count_snapshot &snapshot,
                               a_const_char                *err_msg)
/*
Assert that at least one new error has occurred since the original snapshot was
taken.
*/
{
  check_assertion_str(((snapshot.captured_total_state.all_error_types() <
                        diagnostic_counters.total.all_error_types()) ||
                       (snapshot.captured_repeated_state.all_error_types() <
                        diagnostic_counters.repeated.all_error_types())),
                      err_msg);
}  /* expect_error_since */


inline a_boolean is_at_least_one_error()
/*
This function is defined for easily checking if there was at least one error.
*/
{
  return diagnostic_counters.total.errors > 0;
}  /* is_at_least_one_error */


inline a_boolean is_at_least_one_warning()
/*
This function is defined for easily checking if there was at least one warning.
*/
{
  return diagnostic_counters.total.warnings > 0;
}  /* is_at_least_one_warning */


EXTERN_THREAD an_error_severity
		error_threshold;
			/* Messages at or above this severity level should
			   be displayed; those below are suppressed. */
EXTERN_THREAD an_error_severity
		error_promotion_threshold;
			/* Messages at or above this severity level will be
			   promoted to discretionary errors, unless they are
			   already more severe than that. */
EXTERN_THREAD unsigned long
		error_limit;
			/* Compilation is abandoned when this many errors
			   are detected. */

EXTERN_THREAD int
		context_limit;

			/* The maximum number of context lines to be
			   emitted as part of an error message. */

EXTERN_THREAD an_error_severity
                strict_ansi_error_severity;
                        /* Strict ANSI mode violations are reported at this
                           error severity.  This must either be es_error
                           or es_warning. */

EXTERN_THREAD an_error_severity
                strict_ansi_discretionary_severity;
                        /* Strict ANSI mode violations that may be
                           discretionary errors are reported at this
                           error severity.  This must either be
                           es_discretionary_error or es_warning. */


EXTERN_THREAD an_error_severity
                anachronism_error_severity;
                        /* Use of anachronisms are reported at this
                           error severity.  It is expected that this will
                           either be es_error or es_warning.  This can be
                           modified by a command line option. */

EXTERN_THREAD a_boolean
                brief_diagnostics;
                        /* TRUE if diagnostic output should omit the
			   source line information and suppress wrapping
			   of the error message text. */

EXTERN_THREAD a_boolean
                do_not_wrap_diagnostics;
                        /* TRUE if diagnostic output should suppress
			   wrapping of the error message text. */

EXTERN_THREAD a_boolean
                display_error_context_on_catastrophe;
                        /* TRUE if error context information should be
			   displayed following a catastrophic error. */

EXTERN_THREAD a_boolean
		display_template_typedefs_in_diagnostics;
			/* TRUE if typedefs from class templates should be
			   included in diagnostic output.  When this is FALSE
			   the underlying type is displayed in place of the
			   typedef. */

#if FULLY_RESOLVED_MACRO_POSITIONS
EXTERN_THREAD a_boolean
		macro_positions_in_diagnostics;
			/* TRUE if diagnostic output referring to text in
			   macro expansions should display original position
			   and (if RECORD_MACRO_INVOCATIONS is TRUE) macro
			   invocation context information. */
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */

/*
Error routines.
*/

NORETURN extern void insufficient_address_space();

#if CHECKING

EXTERN_THREAD a_boolean
		suppress_assertion_line_number;
			/* TRUE if the line number portion of an "assertion
			   failed" message should be suppressed. */


/*lint -sem(internal_error, r_no)*/
NORETURN extern void internal_error(a_const_char *error_message);

extern void record_expected_error(a_const_char *filename,
                                  int          line_number,
			          a_const_char *function,
                                  a_const_char *string1,
                                  a_const_char *string2);

extern void check_expected_errors(void);

/* Macro to test an assertion or ensure that errors will be issued before
   a back end is invoked (more specifically: when check_expected_errors is
   called). */
#define check_assertion_or_expect_error(test)                                \
  if (/*lint --e(774)*/!(test) && !is_at_least_one_error()) {                \
    record_expected_error(__FILE__, __LINE__, __EDG_func__, (char *)NULL,    \
                          (char *)NULL);                                     \
  }
/* Same as check_assertion_or_expect_error, but only check for errors (no
   other condition). */
#define expect_error()                                                       \
  if (!is_at_least_one_error()) {                                            \
    record_expected_error(__FILE__, __LINE__, __EDG_func__, (char *)NULL,    \
                          (char *)NULL);                                     \
  }
#define check_assertion_or_expect_error_str(test, string)                    \
  if (/*lint --e(774)*/!(test) && !is_at_least_one_error()) {                \
    record_expected_error(__FILE__, __LINE__, __EDG_func__, string,          \
                          (char *)NULL);\
  }
#define expect_error_str(string)                                             \
  if (!is_at_least_one_error()) {                                            \
    record_expected_error(__FILE__, __LINE__, __EDG_func__, string,          \
                          (char *)NULL);\
  }
/* Macros that are the same as above except that two strings are provided.
   This is simply done to make it easier to use long strings as arguments. */
/*lint -emacro(774 506, check_assertion_str2)*/
#define check_assertion_str2(test, string1, string2)          \
  if (!(test))                                                \
    assertion_failed(__FILE__, __LINE__, __EDG_func__, string1, string2)
#define check_assertion_or_expect_error_str2(test, string1, string2)         \
  if (/*lint --e(774)*/!(test) && !is_at_least_one_error()) {                \
    record_expected_error(__FILE__, __LINE__, __EDG_func__, string1, string2);\
  }
#define expect_error_str2(string1, string2)                                  \
  if (!is_at_least_one_error()) {                                            \
    record_expected_error(__FILE__, __LINE__, __EDG_func__, string1, string2);\
  }
#else /* !CHECKING */
#define check_assertion_or_expect_error(test) /* Nothing */
#define check_assertion_or_expect_error_str(test, string) /* Nothing */
#define check_assertion_or_expect_error_str2(test, string1, string2) /* */
#define expect_error() /* Nothing */
#define expect_error_str(string) /* Nothing */
#endif /* CHECKING */

/* Forward declare some IL and front end types to avoid having to
   include symbol_tbl.h and il_def.h in this file. */
struct a_module;
struct a_symbol;
struct a_type;
struct a_reflection_value;
struct a_source_file;
struct a_pending_pragma;


extern void error_early_init(void);
extern void error_one_time_init(void);
extern void error_init(void);
extern void error_trans_unit_init(void);
#if !STANDALONE_UTILITY_PROGRAM
extern void clear_file_index_list(void);
#if MAKE_FRONT_END_CALLABLE
extern void error_cleanup(void);
extern void error_late_cleanup();
#endif /* MAKE_FRONT_END_CALLABLE */

extern a_line_number initialize_file_index(struct a_source_file *src_file);
extern a_line_number update_file_index(struct a_source_file *src_file,
                                       a_line_number        physical_line,
                                       long                 file_pos);
#endif /* !STANDALONE_UTILITY_PROGRAM */

extern void record_prototype_diagnostic(an_error_code      error_code,
                                        an_error_severity  severity,
                                        a_source_position  *error_pos);

extern a_boolean find_prototype_diagnostic(an_error_code      error_code,
                                           an_error_severity  severity,
                                           a_source_position  *error_pos);

extern a_boolean set_severity_for_error_tag(a_const_char	*tag,
				            an_error_severity	severity,
					    a_boolean		make_default);
extern
a_boolean set_severity_for_error_number(int		  error_number,
			                an_error_severity severity,
				        a_boolean	  make_default);

extern a_boolean is_effective_error(an_error_code	error_code,
                                    an_error_severity	severity,
                                    a_source_position	*pos);

extern a_boolean is_effective_sfinae_error(an_error_code	error_code,
                                           an_error_severity	severity,
                                           a_source_position	*pos);

extern a_boolean is_effective_diagnostic(an_error_code     error_code,
                                         an_error_severity severity,
                                         a_source_position *pos);

/*lint -sem(command_line_error, r_no)*/
NORETURN extern void command_line_error(an_error_code error_code);
/*lint -sem(str_command_line_error, r_no)*/
NORETURN extern void str_command_line_error(an_error_code error_code,
                                            a_const_char  *fill_in_string);

#if !STANDALONE_UTILITY_PROGRAM
extern void str_command_line_warning(an_error_code error_code,
                                     a_const_char  *concat_string);
#endif /* !STANDALONE_UTILITY_PROGRAM */
extern
void file_open_error(an_error_severity		severity,
		     an_error_code		file_kind,
                     a_const_char		*file_name,
		     an_open_file_result	*open_result);

/*lint -sem(output_file_open_error, r_no)*/
NORETURN extern void output_file_open_error(a_boolean         bad_name,
                                            an_error_code     file_kind,
                                            a_const_char      *file_name,
                                            an_error_severity severity);
/*lint -sem(file_write_error, r_no)*/
NORETURN extern void file_write_error(an_error_code file_kind,
                                      int           errno_value);
extern void pos_st_diagnostic(an_error_severity error_severity,
                              an_error_code     error_code,
                              a_source_position *error_pos,
                              a_const_char      *error_string);
extern void pos_diagnostic(an_error_severity  error_severity,
                           an_error_code      error_code,
                           a_source_position  *error_pos);
extern void diagnostic(an_error_severity  error_severity,
                       an_error_code      error_code);
extern void pos_ty_diagnostic(an_error_severity  error_severity,
                              an_error_code      error_code,
                              a_source_position  *error_pos,
                              struct a_type      *type);
extern void pos_ty2_diagnostic(an_error_severity  error_severity,
                               an_error_code      error_code,
                               a_source_position  *error_pos,
                               struct a_type      *type1,
                               struct a_type      *type2);
#if !STANDALONE_UTILITY_PROGRAM
extern void st_num_diagnostic(an_error_severity error_severity,
                              an_error_code     error_code,
                              a_const_char      *error_string,
                              int32_t           num);
extern void pos_sy_diagnostic(an_error_severity  error_severity,
                              an_error_code      error_code,
                              a_source_position  *error_pos,
                              struct a_symbol    *symbol);
extern void pos2_diagnostic(an_error_severity  error_severity,
                            an_error_code      error_code,
                            a_source_position  *error_pos,
                            a_source_position  *other_pos);
extern void pos2_st_diagnostic(an_error_severity  error_severity,
                               an_error_code      error_code,
                               a_source_position  *error_pos,
                               a_source_position  *other_pos,
                               a_const_char       *error_string);
extern void pos2_sy_diagnostic(an_error_severity  error_severity,
                               an_error_code      error_code,
                               a_source_position  *error_pos,
                               a_source_position  *other_pos,
                               struct a_symbol    *symbol);
extern void pos_sy_ty2_diagnostic(an_error_severity  error_severity,
                                  an_error_code      error_code,
                                  a_source_position  *error_pos,
                                  struct a_symbol    *symbol,
                                  struct a_type      *type1,
                                  struct a_type      *type2);
extern void pos_sy2_diagnostic(an_error_severity  error_severity,
                               an_error_code      error_code,
                               a_source_position  *error_pos,
                               struct a_symbol    *symbol1,
                               struct a_symbol    *symbol2);
extern void sym_diagnostic(an_error_severity  error_severity,
                           an_error_code      error_code,
                           struct a_symbol    *symbol);
extern void pos_syty_diagnostic(an_error_severity  error_severity,
                                an_error_code      error_code,
                                a_source_position  *error_pos,
                                struct a_symbol    *symbol,
                                struct a_type      *type);
extern void pos_stsy_diagnostic(an_error_severity  error_severity,
                                an_error_code      error_code,
                                a_source_position  *error_pos,
                                a_const_char       *error_string,
                                struct a_symbol    *symbol);
extern void pos_st_num2_diagnostic(an_error_severity error_severity,
                                   an_error_code     error_code,
                                   a_source_position *error_pos,
                                   a_const_char      *error_string,
                                   int32_t           num1,
                                   int32_t           num2);
extern void pos_num2_diagnostic(an_error_severity error_severity,
                                an_error_code     error_code,
                                a_source_position *error_pos,
                                int32_t           num1,
                                int32_t           num2);
extern void st_num_add_diag_info(a_diagnostic_ptr primary_dp,
                                 an_error_code    error_code,
                                 a_const_char     *error_string,
                                 int32_t          num);
#endif /* !STANDALONE_UTILITY_PROGRAM */
extern void pos_st_remark(an_error_code     error_code,
                          a_source_position *error_pos,
                          a_const_char      *error_string);
extern void pos_remark(an_error_code     error_code,
                       a_source_position *error_pos);
extern void str_remark(an_error_code error_code,
                       a_const_char  *error_string);
extern void pos_ty_remark(an_error_code     error_code,
                          a_source_position *error_pos,
                          struct a_type     *type);
#if 0
/* These routines are not currently used by the compiler. */
extern void pos_ty2_remark(an_error_code     error_code,
                           a_source_position *error_pos,
                           struct a_type     *type1,
                           struct a_type     *type2);
extern void type_remark(an_error_code error_code,
                        struct a_type *type);
#endif /* 0 */
#if !STANDALONE_UTILITY_PROGRAM
extern void pos_sy_remark(an_error_code     error_code,
                          a_source_position *error_pos,
                          struct a_symbol   *symbol);
extern void sym_remark(an_error_code   error_code,
                       struct a_symbol *symbol);
extern void pos_stsy_remark(an_error_code     error_code,
                            a_source_position *error_pos,
                            a_const_char      *error_string,
                            struct a_symbol   *symbol);
#endif /* !STANDALONE_UTILITY_PROGRAM */
extern void pos_st_warning(an_error_code     error_code,
                           a_source_position *error_pos,
                           a_const_char      *error_string);
extern void pos_warning(an_error_code     error_code,
                        a_source_position *error_pos);
extern void str_warning(an_error_code error_code,
                        a_const_char  *error_string);
#if MICROSOFT_EXTENSIONS_ALLOWED
extern void pos_st2_warning(an_error_code     error_code,
                            a_source_position *error_pos,
                            a_const_char      *error_string1,
                            a_const_char      *error_string2);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
extern void pos_ty_warning(an_error_code     error_code,
                           a_source_position *error_pos,
                           struct a_type     *type);
extern void pos_ty2_warning(an_error_code     error_code,
                            a_source_position *error_pos,
                            struct a_type     *type1,
                            struct a_type     *type2);
extern void pos_opt_ty2_warning(an_error_code     error_code,
                                a_source_position *error_pos,
                                struct a_type     *type1,
                                struct a_type     *type2);
extern void type_warning(an_error_code error_code,
                         struct a_type *type);
#if !STANDALONE_UTILITY_PROGRAM
extern void pos_syty_warning(an_error_code     error_code,
                             a_source_position *error_pos,
                             struct a_symbol   *symbol,
                             struct a_type     *type);
extern void pos_sy_warning(an_error_code     error_code,
                           a_source_position *error_pos,
                           struct a_symbol   *symbol);
extern void sym_warning(an_error_code   error_code,
                        struct a_symbol *symbol);
extern void pos_stsy_warning(an_error_code     error_code,
                             a_source_position *error_pos,
                             a_const_char      *error_string,
                             struct a_symbol   *symbol);
#endif /* !STANDALONE_UTILITY_PROGRAM */
extern void pos_stty_warning(an_error_code     error_code,
                             a_source_position *error_pos,
                             a_const_char      *error_string,
                             struct a_type     *type);
extern void pos_st_error(an_error_code     error_code,
                         a_source_position *error_pos,
                         a_const_char      *error_string);
extern void pos_st2_error(an_error_code     error_code,
                          a_source_position *error_pos,
                          a_const_char      *error_string1,
                          a_const_char      *error_string2);
extern void pos_st2_diagnostic(an_error_severity error_severity,
                               an_error_code     error_code,
                               a_source_position *error_pos,
                               a_const_char      *error_string1,
                               a_const_char      *error_string2);
extern void pos_opt_ty2_diagnostic(an_error_severity sev,
                                   an_error_code     error_code,
                                   a_source_position *error_pos,
                                   struct a_type     *type1,
                                   struct a_type     *type2);
extern void pos_stty_error(an_error_code     error_code,
                           a_source_position *error_pos,
                           a_const_char      *error_string,
                           struct a_type     *type);
extern void pos_error(an_error_code     error_code,
                      a_source_position *error_pos);
extern void str_error(an_error_code error_code,
                      a_const_char  *error_string);
extern void pos_ty_error(an_error_code     error_code,
                         a_source_position *error_pos,
                         struct a_type     *type);
extern void pos_ty2_error(an_error_code     error_code,
                          a_source_position *error_pos,
                          struct a_type     *type1,
                          struct a_type     *type2);
extern void pos_ty_str_error(an_error_code     error_code,
                             a_source_position *error_pos,
                             struct a_type     *type,
                             a_const_char      *error_string);
#if MICROSOFT_EXTENSIONS_ALLOWED
extern void pos_ty3_error(an_error_code      error_code,
                          a_source_position  *error_pos,
                          struct a_type      *type1,
                          struct a_type      *type2,
                          struct a_type      *type3);
void pos2_ty_diagnostic(an_error_severity  error_severity,
                        an_error_code      error_code,
                        a_source_position  *error_pos,
                        a_source_position  *other_pos,
                        struct a_type      *type);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
extern void pos_opt_ty2_error(an_error_code     error_code,
                              a_source_position *error_pos,
                              struct a_type     *type1,
                              struct a_type     *type2);
extern void type_error(an_error_code error_code,
                       struct a_type *type);
#if !STANDALONE_UTILITY_PROGRAM
extern void pos_stsy_error(an_error_code     error_code,
                           a_source_position *error_pos,
                           a_const_char      *error_string,
                           struct a_symbol   *symbol);
extern void pos_sy_error(an_error_code     error_code,
                         a_source_position *error_pos,
                         struct a_symbol   *symbol);
extern void pos_sy2_error(an_error_code     error_code,
                          a_source_position *error_pos,
                          struct a_symbol   *symbol1,
                          struct a_symbol   *symbol2);
extern void pos_syty_error(an_error_code     error_code,
                           a_source_position *error_pos,
                           struct a_symbol   *symbol,
                           struct a_type     *type);
extern void sym_error(an_error_code   error_code,
                      struct a_symbol *symbol);
#endif /* !STANDALONE_UTILITY_PROGRAM */
#if DO_IL_LOWERING
/*lint -sem(pos_st_catastrophe, r_no)*/
NORETURN extern void pos_ty_catastrophe(an_error_code     error_code,
                                        a_source_position *error_pos,
                                        struct a_type     *type);
#endif /* DO_IL_LOWERING */
/*lint -sem(pos_st_catastrophe, r_no)*/
NORETURN extern void pos_st_catastrophe(an_error_code     error_code,
                                        a_source_position *error_pos,
                                        a_const_char      *error_string);
/*lint -sem(str_catastrophe, r_no)*/
NORETURN extern void str_catastrophe(an_error_code error_code,
                                     a_const_char  *error_string);
/*lint -sem(catastrophe, r_no)*/
NORETURN extern void catastrophe(an_error_code error_code);

/*lint -sem(pos_str2_catastrophe, r_no)*/
NORETURN extern void pos_str2_catastrophe(an_error_code     error_code,
                                          a_const_char      *error_string1,
                                          a_const_char      *error_string2,
                                          a_source_position *error_pos);
#if EDG_WIN32
#if !STANDALONE_UTILITY_PROGRAM
/*lint -sem(win32_catastrophe, r_no)*/
NORETURN extern void win32_catastrophe(an_ms_dword   error_code,
                                       a_const_char  *error_string);

/*lint -sem(hresult_catastrophe, r_no)*/
NORETURN extern void hresult_catastrophe(a_const_char *error_string);
#endif /* !STANDALONE_UTILITY_PROGRAM */
#endif /* EDG_WIN32 */

/*lint -sem(str_errno_catastrophe, r_no)*/
NORETURN extern void str_errno_catastrophe(an_error_code error_code,
                                           a_const_char  *error_string,
                                           int           errno_value);
/* Interfaces for producing multiple message diagnostics. */
extern a_diagnostic_ptr pos_start_diagnostic(an_error_severity error_severity,
                                             an_error_code     error_code,
                                             a_source_position *error_pos);
extern a_diagnostic_ptr pos_ty_start_diagnostic(
                                    an_error_severity  error_severity,
                                    an_error_code      error_code,
                                    a_source_position *error_pos,
                                    struct a_type     *type);
extern a_diagnostic_ptr pos_start_error(an_error_code     error_code,
                                        a_source_position *error_pos);
extern a_diagnostic_ptr pos_st_start_error(an_error_code     error_code,
                                           a_source_position *error_pos,
                                           a_const_char      *error_string);
extern a_diagnostic_ptr pos_ty_start_error(an_error_code     error_code,
                                           a_source_position *error_pos,
                                           struct a_type     *type);
extern a_diagnostic_ptr pos_ty2_start_error(an_error_code     error_code,
                                            a_source_position *error_pos,
                                            struct a_type     *type1,
                                            struct a_type     *type2);
extern void ty_add_diag_info(a_diagnostic_ptr primary_dp,
                             an_error_code error_code,
                             struct a_type *type);
extern void str_add_diag_info(a_diagnostic_ptr primary_dp,
                              an_error_code    error_code,
                              a_const_char     *error_string);
extern void copy_str_add_diag_info(a_diagnostic_ptr primary_dp,
                                   an_error_code    error_code,
                                   a_const_char     *error_string);
extern void add_diag_info(a_diagnostic_ptr primary_dp,
                          an_error_code    error_code);
extern void add_diag_info_with_pos_insert(a_diagnostic_ptr   primary_dp,
                                          an_error_code      error_code,
                                          a_source_position  *pos);
extern
FILE *fopen_with_error(a_const_char		*file_name,
		       a_const_char		*mode,
		       an_open_file_flag_set	open_flags,
		       an_error_code		file_kind);

extern
FILE *open_output_file_with_error_handling(
					a_const_char		*file_name,
					a_boolean		binary_file,
					a_boolean		update_mode,
					an_open_file_flag_set	open_flags,
					an_error_code		file_kind);

extern
FILE *open_input_file_with_error_handling(
				a_const_char		*file_name,
				a_boolean		binary_file,
				an_open_file_flag_set	open_flags,
				an_error_code		file_kind);

extern void close_output_file_with_error_handling(FILE		**f_output,
						  an_error_code	file_kind);

extern
FILE *open_source_file_with_error_handling(
				a_const_char		*file_name,
				an_open_file_flag_set	open_flags,
				an_open_file_result	*open_result,
				a_unicode_source_kind	*unicode_source_kind);

extern a_const_char *error_text(an_error_code error_code);

#if !STANDALONE_UTILITY_PROGRAM
extern a_diagnostic_ptr pos_sy_start_diagnostic(
                                    an_error_severity  error_severity,
                                    an_error_code      error_code,
                                    a_source_position *error_pos,
                                    struct a_symbol   *symbol);
extern a_diagnostic_ptr pos_stsy_start_diagnostic(
                                             an_error_severity  error_severity,
                                             an_error_code      error_code,
                                             a_source_position  *error_pos,
                                             a_const_char       *error_string,
                                             struct a_symbol    *symbol);
extern a_diagnostic_ptr pos_sy_start_error(an_error_code     error_code,
                                           a_source_position *error_pos,
                                           struct a_symbol   *symbol);
extern a_diagnostic_ptr pos_stsy_start_error(an_error_code     error_code,
                                             a_source_position *error_pos,
                                             a_const_char      *error_string,
                                             struct a_symbol   *symbol);
extern void pos_sy2_warning(an_error_code     error_code,
                            a_source_position *error_pos,
                            struct a_symbol   *symbol1,
                            struct a_symbol   *symbol2);

extern void sym_add_diag_info(a_diagnostic_ptr primary_dp,
                              an_error_code    error_code,
                              struct a_symbol  *symbol);

extern void pos_sy_add_diag_info(a_diagnostic_ptr  primary_dp,
                                 an_error_code     error_code,
                                 a_source_position *pos,
                                 struct a_symbol   *symbol);

extern void more_info_diagnostic(an_error_code     error_code,
                                 a_source_position *error_pos,
                                 a_diag_list_ptr   diag_list);

extern void more_info_type_diagnostic(an_error_code     error_code,
                                      a_source_position *error_pos,
                                      struct a_type     *tp,
                                      a_diag_list_ptr   diag_list);

extern void more_info_type2_diagnostic(an_error_code     error_code,
                                       a_source_position *error_pos,
                                       struct a_type     *tp1,
                                       struct a_type     *tp2,
                                       a_diag_list_ptr   diag_list);

extern void more_info_sym_diagnostic(an_error_code     error_code,
                                     a_source_position *error_pos,
                                     struct a_symbol   *sym,
                                     a_diag_list_ptr   diag_list);

extern void more_info_sym_type_diagnostic(an_error_code     error_code,
                                          a_source_position *error_pos,
                                          struct a_symbol   *sym,
                                          struct a_type     *type,
                                          a_diag_list_ptr   diag_list);

extern void more_info_sym2_diagnostic(an_error_code     error_code,
                                      a_source_position *error_pos,
                                      struct a_symbol   *sym1,
                                      struct a_symbol   *sym2,
                                      a_diag_list_ptr   diag_list);

extern void more_info_num_diagnostic(an_error_code     error_code,
                                     a_source_position *error_pos,
                                     int32_t           num,
                                     a_diag_list_ptr   diag_list);

extern void more_info_num2_diagnostic(an_error_code     error_code,
                                      a_source_position *error_pos,
                                      int32_t           num1,
                                      int32_t           num2,
                                      a_diag_list_ptr   diag_list);

extern void more_info_st_diagnostic(an_error_code     error_code,
                                    a_source_position *error_pos,
                                    a_const_char      *fill_in_str,
                                    a_diag_list_ptr   diag_list);

extern void more_info_st3_diagnostic(an_error_code     error_code,
                                     a_source_position *error_pos,
                                     a_const_char      *str1,
                                     a_const_char      *str2,
                                     a_const_char      *str3,
                                     a_diag_list_ptr   diag_list);

extern void more_info_sym_num_diagnostic(an_error_code     error_code,
                                         a_source_position *error_pos,
                                         struct a_symbol   *sym,
                                         int32_t           num,
                                         a_diag_list_ptr   diag_list);

extern void more_info_sym_num_ty_diagnostic(an_error_code     error_code,
                                            a_source_position *error_pos,
                                            struct a_symbol   *sym,
                                            int32_t           num,
                                            struct a_type     *tp,
                                            a_diag_list_ptr   diag_list);

extern void more_info_st_num_diagnostic(an_error_code     error_code,
                                        a_source_position *error_pos,
                                        a_const_char      *fill_in_str,
                                        int32_t           num,
                                        a_diag_list_ptr   diag_list);

extern void more_info_tap_diagnostic(an_error_code      error_code,
                                     a_source_position  *error_pos,
                                     a_template_arg_ptr tap,
                                     a_diag_list_ptr    diag_list);

extern void more_info_sym_tap_diagnostic(an_error_code     error_code,
                                         a_source_position *error_pos,
                                         struct a_symbol   *sym,
                                         a_template_arg    *tap,
                                         a_diag_list_ptr   diag_list);

extern void add_more_info_list(a_diagnostic_ptr		dp,
			       a_diag_list_ptr		dlp);

extern void splice_diag_list(a_diag_list_ptr   src,
                             a_diag_list_ptr   dst,
                             a_diagnostic_ptr  insert_after);

extern void discard_more_info_list(a_diag_list_ptr	dlp);

extern void pch_message(an_error_code error_code,
   		        a_const_char  *fill_in_str);

extern void diag_pragma(struct a_pending_pragma *ppp);

extern void diagnostic_pragma(struct a_pending_pragma *ppp);

extern void embedded_cplusplus_noncompliance_diagnostic(
                                              a_source_position  *error_pos,
                                              an_error_code      error_code);

/* Macro that determines whether to report a violation of the Embedded C++
   subset. */
#define feature_is_not_part_of_embedded_cplusplus_subset(pos, error_code) \
  { if (report_embedded_cplusplus_noncompliance)                          \
      embedded_cplusplus_noncompliance_diagnostic((pos), (error_code)); }

#if GNU_EXTENSIONS_ALLOWED

/* Macro to report uses of GNU extensions if needed. */
#define report_gnu_extension_if_needed(pos, error_code)                     \
  { if (report_gnu_extensions) {                                            \
      pos_warning((error_code), (pos));                                     \
    }  /* if */                                                             \
  }

/* Macro to warn about C++11 features enabled in default non-C++11 GNU C++
   modes. */
#define report_gnu_cpp11_extension_if_needed(pos, error_code)               \
  { if (gpp_mode && !cpp11_mode) {                                          \
      f_report_gnu_cpp11_extensions_if_needed((pos), (error_code));         \
    }  /* if */                                                             \
  }

/* Macro to warn about C++17 features enabled in default non-C++17 GNU C++
   modes. */
#define report_gnu_cpp17_extension_if_needed(pos, error_code)               \
  { if (gpp_mode && !cpp17_mode) {                                          \
      f_report_gnu_cpp11_extensions_if_needed((pos), (error_code));         \
    }  /* if */                                                             \
  }

/* Macro to warn about C++20 features enabled in default non-C++20 GNU C++
   modes. */
#define report_gnu_cpp20_extension_if_needed(pos, error_code)               \
  { if (gpp_mode && !cpp20_mode) {                                          \
      f_report_gnu_cpp11_extensions_if_needed((pos), (error_code));         \
    }  /* if */                                                             \
  }

extern void f_report_gnu_cpp11_extensions_if_needed(
                                               a_source_position  *pos,
                                               an_error_code      error_code);
#else /* !GNU_EXTENSIONS_ALLOWED */

#define report_gnu_extension_if_needed(pos, error_code)  /* Nothing */
#define report_gnu_cpp11_extension_if_needed(pos, error_code)  /* Nothing */
#define report_gnu_cpp17_extension_if_needed(pos, error_code)  /* Nothing */
#define report_gnu_cpp20_extension_if_needed(pos, error_code)  /* Nothing */

#endif /* GNU_EXTENSIONS_ALLOWED */

#endif /* !STANDALONE_UTILITY_PROGRAM */

extern void end_diagnostic(a_diagnostic_ptr dp);

extern a_diagnostic_ptr start_command_line_error(an_error_code error_code,
			                         a_const_char  *error_string);

/*lint -sem(end_command_line_error, r_no)*/
NORETURN extern void end_command_line_error(a_diagnostic_ptr dp);

/* Report a syntax error, flush to a token in the stop set. */
extern void syntax_error(an_error_code error_code);

#if DEBUG
/* GCOVR_EXCL_START */
unsigned long show_error_space_used(void);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

namespace detail {

/*
A forward declaration of a type specialized to handle adding a fill-in value to
a diagnostic.

Each specialization should implement the function:

  static void add(a_diagnostic_ptr diag,
                  a_Type           value);

*/
template<typename a_Type>
struct Fill_in;

#if !STANDALONE_UTILITY_PROGRAM

/*
A diagnostic fill-in for signed integer values.
*/
template<>
struct Fill_in<long long> {
  static void add(a_diagnostic_ptr diag,
                  long long        value);
};  /* Fill_in */


template<>
struct Fill_in<long> : Fill_in<long long> {
};  /* Fill_in */

template<>
struct Fill_in<int> : Fill_in<long long> {
};  /* Fill_in */


/*
A diagnostic fill-in for unsigned integer values.
*/
template<>
struct Fill_in<unsigned long long> {
  static void add(a_diagnostic_ptr   diag,
                  unsigned long long value);
};  /* Fill_in */


template<>
struct Fill_in<unsigned long> : Fill_in<unsigned long long> {
};  /* Fill_in */

template<>
struct Fill_in<unsigned> : Fill_in<unsigned long long> {
};  /* Fill_in */

#endif /* !STANDALONE_UTILITY_PROGRAM */

/*
A diagnostic fill-in for const source positions.
*/
template<>
struct Fill_in<const a_source_position*> {
  static void add(a_diagnostic_ptr        diag,
                  const a_source_position *value);
};  /* Fill_in */

/*
A diagnostic fill-in for source positions.
*/
template<>
struct Fill_in<a_source_position*> {
  static void add(a_diagnostic_ptr  diag,
                  a_source_position *value)
    { Fill_in<const a_source_position*>::add(diag, value); }
};  /* Fill_in */

/*
A diagnostic fill-in for a_const_char* (C-string) values.
*/
template<>
struct Fill_in<a_const_char*> {
  static void add(a_diagnostic_ptr diag,
                  a_const_char     *value);
};  /* Fill_in */

/*
A diagnostic fill-in for char* (C-string) values.
*/
template<>
struct Fill_in<char*> {
  static void add(a_diagnostic_ptr diag,
                  a_const_char     *value)
    { Fill_in<a_const_char*>::add(diag, value); }
};  /* Fill_in */

/*
A diagnostic fill-in for modules.
*/
template<>
struct Fill_in<a_module*> {
  static void add(a_diagnostic_ptr diag,
                  a_module         *mod);
};  /* Fill_in */

/*
A diagnostic fill-in for string view values.
*/
template<>
struct Fill_in<a_string_view> {
  static void add(a_diagnostic_ptr diag,
                  a_string_view    value);
};  /* Fill_in */

/*
A diagnostic fill-in for symbols.
*/
template<>
struct Fill_in<a_symbol*> {
  static void add(a_diagnostic_ptr diag,
                  a_symbol         *value);
};  /* Fill_in */

/*
A diagnostic fill-in for types.
*/
template<>
struct Fill_in<a_type*> {
  static void add(a_diagnostic_ptr diag,
                  a_type           *value);
};  /* Fill_in */

/*
A diagnostic fill-in for reflections.
*/
template<>
struct Fill_in<struct a_reflection_value> {
  static void add(a_diagnostic_ptr           diag,
                  struct a_reflection_value  rv);
};  /* Fill_in */

extern a_diagnostic_ptr create_primary_diagnostic(an_error_code     error_code,
                                                  a_source_position *position,
                                                  an_error_severity severity);

extern a_diagnostic_ptr create_sub_message(a_diagnostic_ptr primary_dp,
                                           an_error_code    error_code);

extern void append_to_diag_list(a_diag_list_ptr  diag_list,
                                a_diagnostic_ptr new_diag);

}  /* namespace detail */

template<typename... a_Fill_in_type>
inline a_diagnostic_ptr pos_start_diagnostic(
                                        an_error_severity       error_severity,
                                        an_error_code           error_code,
                                        const a_source_position *error_pos,
                                        a_Fill_in_type...       fill_ins)
/*
Begin a multiple message diagnostic with the specified severity, error code,
source position, and fill-ins.  Return a pointer to the diagnostic entry that
will be passed in for the subsequent messages.
*/
{
  a_diagnostic_ptr diag = detail::create_primary_diagnostic(
                                     error_code,
                                     const_cast<a_source_position*>(error_pos),
                                     error_severity);

  /* The following expression is expanded to effectively evaluate as:

       detail::Fill_in<type_1>::add(diag, arg_1);
       detail::Fill_in<type_2>::add(diag, arg_2);
       ...
       detail::Fill_in<type_n>::add(diag, arg_n);
   */
  PACK_EXPAND_VOID_EXPR(detail::Fill_in<a_Fill_in_type>::add(diag, fill_ins))
  return diag;
}  /* pos_start_diagnostic */


template<typename... a_Fill_in_type>
inline a_diagnostic_ptr pos_start_remark(an_error_code     error_code,
                                         a_source_position *error_pos,
                                         a_Fill_in_type... fill_ins)
/*
Begin a multiple message remark with the specified error code, source position,
and fill-ins.  Return a pointer to the diagnostic entry that will be passed in
for the subsequent messages.
*/
{
  return pos_start_diagnostic(es_remark, error_code, error_pos, fill_ins...);
}  /* pos_start_remark */


template<typename... a_Fill_in_type>
inline a_diagnostic_ptr pos_start_warning(an_error_code     error_code,
                                          a_source_position *error_pos,
                                          a_Fill_in_type... fill_ins)
/*
Begin a multiple message warning with the specified error code, source
position, and fill-ins.  Return a pointer to the diagnostic entry that will be
passed in for the subsequent messages.
*/
{
  return pos_start_diagostic(es_warning, error_code, error_pos, fill_ins...);
}  /* pos_start_warning */


template<typename... a_Fill_in_type>
inline a_diagnostic_ptr pos_start_error(an_error_code     error_code,
                                        a_source_position *error_pos,
                                        a_Fill_in_type... fill_ins)
/*
Begin a multiple message error with the specified error code, source position,
and fill-ins.  Return a pointer to the diagnostic entry that will be passed in
for the subsequent messages.
*/
{
  return pos_start_diagnostic(es_error, error_code, error_pos, fill_ins...);
}  /* pos_start_error */


template<typename... a_Fill_in_type>
inline a_diagnostic_ptr pos_start_catastrophe(an_error_code     error_code,
                                              a_source_position *error_pos,
                                              a_Fill_in_type... fill_ins)
/*
Begin a multiple message catastrophe with the specified error code, source
position, and fill-ins.  Return a pointer to the diagnostic entry that will be
passed in for the subsequent messages.
*/
{
  return pos_start_diagnostic(es_catastrophe, error_code, error_pos,
                              fill_ins...);
}  /* pos_start_catastrophe */


template<typename... a_Fill_in_type>
inline a_diagnostic_ptr start_diagnostic(an_error_severity error_severity,
                                         an_error_code     error_code,
                                         a_Fill_in_type... fill_ins)
/*
Begin a multiple message diagnostic with the specified error severity, error
code, and fill-ins.  Return a pointer to the diagnostic entry that will be
passed in for the subsequent messages.
*/
{
  return pos_start_diagnostic(error_severity, error_code,
                              &null_source_position,
                              fill_ins...);
}  /* start_diagnostic */


template<typename... a_Fill_in_type>
inline a_diagnostic_ptr start_remark(an_error_code     error_code,
                                     a_Fill_in_type... fill_ins)
/*
Begin a multiple message remark with the specified error code, and fill-ins.
Return a pointer to the diagnostic entry that will be passed in for the
subsequent messages.
*/
{
  return start_diagnostic(es_remark, error_code, fill_ins...);
}  /* start_remark */


template<typename... a_Fill_in_type>
inline a_diagnostic_ptr start_warning(an_error_code     error_code,
                                      a_Fill_in_type... fill_ins)
/*
Begin a multiple message warning with the specified error code, and fill-ins.
Return a pointer to the diagnostic entry that will be passed in for the
subsequent messages.
*/
{
  return start_diagnostic(es_warning, error_code, fill_ins...);
}  /* start_warning */


template<typename... a_Fill_in_type>
inline a_diagnostic_ptr start_error(an_error_code     error_code,
                                    a_Fill_in_type... fill_ins)
/*
Begin a multiple message error with the specified error code, and fill-ins.
Return a pointer to the diagnostic entry that will be passed in for the
subsequent messages.
*/
{
  return start_diagnostic(es_error, error_code, fill_ins...);
}  /* start_error */


template<typename... a_Fill_in_type>
inline a_diagnostic_ptr start_catastrophe(an_error_code     error_code,
                                          a_Fill_in_type... fill_ins)
/*
Begin a multiple message catastrophe with the specified error code, and
fill-ins.  Return a pointer to the diagnostic entry that will be passed in for
the subsequent messages.
*/
{
  return start_diagnostic(es_catastrophe, error_code, fill_ins...);
}  /* start_catastrophe */


template<typename... a_Fill_in_type>
inline void pos_diagnostic(an_error_severity       error_severity,
                           an_error_code           error_code,
                           const a_source_position *error_pos,
                           a_Fill_in_type...       fill_ins)
/*
Report the indicated diagnostic with the specified error severity, error code,
source position, and fill-ins.
*/
{
  a_diagnostic_ptr diag = pos_start_diagnostic(error_severity, error_code,
                                               error_pos, fill_ins...);

  end_diagnostic(diag);
}  /* pos_diagnostic */


template<typename... a_Fill_in_type>
inline void pos_remark(an_error_code           error_code,
                       const a_source_position *error_pos,
                       a_Fill_in_type...       fill_ins)
/*
Report the indicated remark with the specified error code, source position, and
fill-ins.
*/
{
  pos_diagnostic(es_remark, error_code, error_pos, fill_ins...);
}  /* pos_remark */


template<typename... a_Fill_in_type>
inline void pos_warning(an_error_code           error_code,
                        const a_source_position *error_pos,
                        a_Fill_in_type...       fill_ins)
/*
Report the indicated warning with the specified error code, source position,
and fill-ins.
*/
{
  pos_diagnostic(es_warning, error_code, error_pos, fill_ins...);
}  /* pos_warning */


template<typename... a_Fill_in_type>
inline void pos_error(an_error_code           error_code,
                      const a_source_position *error_pos,
                      a_Fill_in_type...       fill_ins)
/*
Report the indicated error with the specified error code, source position, and
fill-ins.
*/
{
  pos_diagnostic(es_error, error_code, error_pos, fill_ins...);
}  /* pos_error */


template<typename... a_Fill_in_type>
inline void pos_catastrophe(an_error_code           error_code,
                            const a_source_position *error_pos,
                            a_Fill_in_type...       fill_ins)
/*
Report the indicated catastrophe with the specified error code, source
position, and fill-ins.
*/
{
  pos_diagnostic(es_catastrophe, error_code, error_pos, fill_ins...);
}  /* pos_catastrophe */


template<typename... a_Fill_in_type>
inline void diagnostic(an_error_severity error_severity,
                       an_error_code     error_code,
                       a_Fill_in_type... fill_ins)
/*
Report the indicated diagnostic with the specified error severity, error code,
and fill-ins.
*/
{
  pos_diagnostic(error_severity, error_code,
                 &null_source_position,
                 fill_ins...);
}  /* diagnostic */


template<typename... a_Fill_in_type>
inline void remark(an_error_code     error_code,
                   a_Fill_in_type... fill_ins)
/*
Report the indicated remark with the specified error code and fill-ins.
*/
{
  diagnostic(es_remark, error_code, fill_ins...);
}  /* remark */


template<typename... a_Fill_in_type>
inline void warning(an_error_code     error_code,
                    a_Fill_in_type... fill_ins)
/*
Report the indicated warning with the specified error code and fill-ins.
*/
{
  diagnostic(es_warning, error_code, fill_ins...);
}  /* warning */


template<typename... a_Fill_in_type>
inline void error(an_error_code     error_code,
                  a_Fill_in_type... fill_ins)
/*
Report the indicated error with the specified error code and fill-ins.
*/
{
  diagnostic(es_error, error_code, fill_ins...);
}  /* error */


template<typename... a_Fill_in_type>
inline void catastrophe(an_error_code     error_code,
                        a_Fill_in_type... fill_ins)
/*
Report the indicated catastrophe with the specified error code and fill-ins.
*/
{
  diagnostic(es_catastrophe, error_code, fill_ins...);
}  /* catastrophe */


template<typename... a_Fill_in_type>
inline void more_info(a_diag_list_ptr   diag_list,
                      an_error_code     error_code,
                      a_source_position *error_pos,
                      a_Fill_in_type... fill_ins)
/*
Add a new diagnostic to the diagnostic list with the specified error code,
error position, and fill-ins.
*/
{
  a_diagnostic_ptr diag = detail::create_primary_diagnostic(error_code,
                                                            error_pos,
                                                            es_more_info);

  /* The following expression is expanded to effectively evaluate as:

       detail::Fill_in<type_1>::add(diag, arg_1);
       detail::Fill_in<type_2>::add(diag, arg_2);
       ...
       detail::Fill_in<type_n>::add(diag, arg_n);
   */
  PACK_EXPAND_VOID_EXPR(detail::Fill_in<a_Fill_in_type>::add(diag, fill_ins))
  detail::append_to_diag_list(diag_list, diag);
}  /* more_info */


template<typename... a_Fill_in_type>
inline void add_diag_info(a_diagnostic_ptr  primary_dp,
                          an_error_code     error_code,
                          a_Fill_in_type... fill_ins)
/*
Add the specified diagnostic message to the primary_dp with the given fill-ins.
*/
{
  a_diagnostic_ptr diag = detail::create_sub_message(primary_dp, error_code);

  /* The following expression is expanded to effectively evaluate as:

       detail::Fill_in<type_1>::add(diag, arg_1);
       detail::Fill_in<type_2>::add(diag, arg_2);
       ...
       detail::Fill_in<type_n>::add(diag, arg_n);
   */
  PACK_EXPAND_VOID_EXPR(detail::Fill_in<a_Fill_in_type>::add(diag, fill_ins))
}  /* add_diag_info */

#if !STANDALONE_UTILITY_PROGRAM

extern void issue_redef_diag(a_source_position *new_pos,
                             a_symbol          *prev_decl_sym,
                             an_error_severity severity = es_error);

extern an_error_code incomplete_type_error_code(a_type *type);

extern void issue_incomplete_type_diag(an_error_code     error_code,
                                       a_source_position *pos,
                                       a_type            *type,
                                       an_error_severity severity);


inline void issue_incomplete_type_diag(a_source_position *pos,
                                       a_type            *type,
                                       an_error_severity severity = es_error)
/*
Emit a diagnostic given the position where the incomplete type is used, the
type that's incomplete, and the severity of the diagnostic.
*/
{
  an_error_code error_code = incomplete_type_error_code(type);

  issue_incomplete_type_diag(error_code, pos, type, severity);
}  /* issue_incomplete_type_diag */

#endif /* !STANDALONE_UTILITY_PROGRAM */

/*
A character that cannot otherwise appear in diagnostic messages that is used
to indicate that the character that follows indicates the type of annotation
(and has a value from the a_diagnostic_annotation_kind enumeration).
Default to use the ESCAPE character.
*/
#define DIAG_ANNOTATION_INDICATOR '\033'

/*
An indicator of the type of annotation.  In a diagnostic, will follow the
DIAG_ANNOTATION_INDICATOR character.  The da_reset always indicates the
end of a previous annotation (i.e., annotations are not nested).
*/
enum a_diagnostic_annotation_kind : a_byte {
  da_reset = 1, /* Indicates end of a previous annotation (avoid NULL). */
  da_error,     /* Beginning of "error" or error-like word. */
  da_warning,   /* Beginning of "warning". */
  da_note,      /* Beginning of "note" or "remark". */
  da_locus,     /* Beginning of source position. */
  da_quote,     /* Beginning of certain fill-in portions of diagnostics. */
  da_range1,    /* Beginning of subsequent source position. */
  da_last       /* Must be last. */
};

/*
For colorization of diagnostics, this maps an annotatable entity to
the SGR character codes that should be used to display that entity.
*/
typedef struct an_sgr_string {
    a_const_char
                *ptr;   /* A pointer into sgr_string_for_colored_diagnostics
                           that corresponds to the annotation kind. */
    sizeof_t    length; /* The length of the SGR code. */
} an_sgr_string;

EXTERN_THREAD an_sgr_string
                sgr_map[(int)da_last];
                        /* A mapping of annotation kinds to SGR strings for
                           colorized diagnostics. */

EXTERN_THREAD a_boolean
                annotate_diagnostics;
                        /* If TRUE, portions of diagnostic messages are
                           annotated (i.e., delimited by markers) so that
                           an implementation can display various portions of
                           the diagnostic in different ways.  This must be
                           TRUE if colorize_diagnostics is TRUE. */

EXTERN_THREAD a_boolean
                colorize_diagnostics;
                        /* If TRUE, portions of diagnostic messages are
                           potentially highlighted (e.g., by the use of colors)
                           to enhance readability. */

EXTERN_THREAD a_const_char
                *sgr_string_for_colored_diagnostics;
                        /* A string that contains the SGR codes for all of the
                           highlightable diagnostic entities.  Entities that
                           are not specified in the string are not highlighted.
                           */
extern void init_colorization(void);


/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef ERROR_H */

