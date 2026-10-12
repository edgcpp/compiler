/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

host_envir.h -- Declarations relating to host_envir.c (having to do with
                the host environment, operating system, and file names).

*/

/* Avoid including these declarations more than once: */
#ifndef HOST_ENVIR_H
#define HOST_ENVIR_H 1

/* Include lang_feat.h to get the definition of
   MICROSOFT_EXTENSIONS_ALLOWED. */
#ifndef LANG_FEAT_H
#include "lang_feat.h"
#endif /* ifndef lang_feat.h */
#include <locale.h>

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

#if defined(EDG_REDEFINE_NULL)
/*
GCC #defines NULL as __null, which allows it to distinguish the null pointer
constant "intention" even in modes that don't support nullptr.  Unfortunately,
gdb appears not to know about __null (or nullptr), which makes macros that
use NULL unusable from within gdb.  If EDG_REDEFINE_NULL is defined, we
redefine NULL to simply be 0, which improves the experience in gdb.
*/
#undef NULL
typedef decltype(nullptr) nullptr_type;
#define NULL 0
#endif /* defined(EDG_REDEFINE_NULL) */

/*
The front end provides two mutually-exclusive macros that provide strong hints
to the host compiler to inline the given function.

The first macro is EXPAND.  This macro is intended to be applied to functions
that are force-inlined primarily for improving debugging.  This macro is
enabled if USE_INLINE_EXPANSION is TRUE (defaults to TRUE only in non-DEBUG
builds).

The second macro is INLINE.  This macro is intended to be applied to functions
that are force-inlined primarily for improved performance.  This macro is
enabled if USE_INLINING_HINTS is TRUE (defaults to TRUE only in non-DEBUG
builds).

Both macros when not enabled only expand to an inline specifier.

Consider the following use cases when employing these macros:

- A developer that wishes for maximum source fidelity in their debugger may set
  USE_INLINE_EXPANSION to FALSE and USE_INLINING_HINTS to FALSE.  This
  preserves all functions and function calls.  This is the default when DEBUG
  is TRUE.
- A developer that wishes to step into fewer small functions during debugging
  may set USE_INLINE_EXPANSION to TRUE and USE_INLINING_HINTS to FALSE.  This
  allows them to step into fewer function calls but does not result in call
  stack bloat (from inlined variables in callees that are not being optimized
  away).
- An organization that wishes to produce a simple optimized build may add -O3
  to their GCC build and set USE_INLINE_EXPANSION and USE_INLINING_HINTS to
  TRUE for a reasonably-good optimization.  This is the default when DEBUG
  is FALSE.
- An organization that wishes to produce an advanced optimized build may add
  -O3 and -fprofile-generate to their GCC build.  They then run a number of use
  cases that represent their common workload to generate profile information.
  Finally they use the profile information in a final build with -O3
  -fprofile-use.  This organization should set USE_INLINE_EXPANSION and
  USE_INLINING_HINTS to FALSE as profile guided optimization can make better
  decisions than manual annotation.

*/
#ifndef USE_INLINE_EXPANSION
#if DEBUG
#define USE_INLINE_EXPANSION FALSE
#else /* !DEBUG */
#define USE_INLINE_EXPANSION TRUE
#endif /* DEBUG */
#endif /* ifndef USE_INLINE_EXPANSION */
#ifndef USE_INLINING_HINTS
#if DEBUG
#define USE_INLINING_HINTS FALSE
#else /* !DEBUG */
#define USE_INLINING_HINTS TRUE
#endif /* DEBUG */
#endif /* ifndef USE_INLINING_HINTS */

#ifndef EXPAND
#if USE_INLINE_EXPANSION
#if defined(__GNUC__)
#define EXPAND __attribute((always_inline)) inline
#else /* !defined(__GNUC__) */
#if defined(__MSC__)
#define EXPAND __forceinline
#else /* !defined(__MSC__) */
#define EXPAND inline
#endif /* defined(__MSC__) */
#endif /* defined(__GNUC__) */
#else /* !USE_INLINE_EXPANSION */
#define EXPAND inline
#endif /* USE_INLINE_EXPANSION */
#endif /* ifndef EXPAND */

#ifndef INLINE
#if USE_INLINING_HINTS
#if defined(__GNUC__)
#define INLINE __attribute((always_inline)) inline
#else /* !defined(__GNUC__) */
#if defined(__MSC__)
#define INLINE __forceinline
#else /* !defined(__MSC__) */
#define INLINE inline
#endif /* defined(__MSC__) */
#endif /* defined(__GNUC__) */
#else /* !USE_INLINING_HINTS */
#define INLINE inline
#endif /* USE_INLINING_HINTS */
#endif /* ifndef INLINE */

/* Forward declaration of a_text_buffer_ptr. */
typedef struct a_text_buffer *a_text_buffer_ptr;

/*
A sequence number is assigned to each token fetched from the input.
This is the type used to represent the sequence number.  Consecutive
tokens do not necessarily have consecutive sequence numbers; currently,
most tokens have even sequence numbers (and in the rare case of a token
being split, the second token of the split then uses an odd number).
*/
typedef uint32_t a_token_sequence_number;

/*
Vertical tab character.
*/
#if USING_ISO_C
#define VERTICAL_TAB_CHARACTER '\v'
#else /* !USING_ISO_C */
/* K&R C doesn't recognize \v. */
#ifndef VERTICAL_TAB_CHARACTER
#define VERTICAL_TAB_CHARACTER '\013'
#endif /* ifndef VERTICAL_TAB_CHARACTER */
#endif /* USING_ISO_C */

/*
Flag used to retain compatibility with a certain driver interface.
This is primarily used to control the files that are created to pass
information between the front end, driver, and prelinker.  The value is
the version number of the EDG C++ front end, e.g., 237 for version 2.37, for
which compatibility should be maintained.  Driver interface changes
made after that version will be suppressed.  Of course, that may suppress
certain language features that cannot be implemented without the
corresponding driver changes.  For example, features that use the template
information file cannot be used with driver versions prior to 2.37.
*/
#ifndef DRIVER_COMPATIBILITY_VERSION
#define DRIVER_COMPATIBILITY_VERSION 9999
#endif /* ifndef DRIVER_COMPATIBILITY_VERSION */

/*
Return codes to be used when the highest error severity is as given:
*/
#if __VMS__
#define RC_NORMAL      0x18000001
#define RC_WARNING     0x18000000
#define RC_ERROR       0x18000002
#define RC_CATASTROPHE 0x18000004
#else /* !__VMS__ */
#define RC_NORMAL      0
#define RC_WARNING     0
#define RC_ERROR       2
#define RC_CATASTROPHE 4
#endif /* __VMS__ */

/*
If this switch is set, an internal error causes an exit instead of
an abort.  When the front end is callable, this should be TRUE so that
an internal error will result in a return to the caller.
*/
#ifndef EXIT_ON_INTERNAL_ERROR
#if MAKE_FRONT_END_CALLABLE
#define EXIT_ON_INTERNAL_ERROR TRUE
#else /* !MAKE_FRONT_END_CALLABLE */
#define EXIT_ON_INTERNAL_ERROR FALSE
#endif /* MAKE_FRONT_END_CALLABLE */
#endif /* ifndef EXIT_ON_INTERNAL_ERROR */

/*
This may be set to FALSE to suppress the error context information for
catastrophic errors.
*/
#ifndef DEFAULT_DISPLAY_ERROR_CONTEXT_ON_CATASTROPHE
#define DEFAULT_DISPLAY_ERROR_CONTEXT_ON_CATASTROPHE TRUE
#endif /* DEFAULT_DISPLAY_ERROR_CONTEXT_ON_CATASTROPHE */

/*
If this flag is TRUE, the sigaction system call should be used to provide
additional information about the location at which a segmentation violation
occurred.  The sigaction system call is only available on certain systems.
Furthermore, the handler invoked currently only works on 32-bit x86 Linux
system.  This facility is intended to be used to provide additional
information for debugging purposes.  To use this facility, this macro
should be defined in defines.h.  In addition, defines.h may need to
define other macros in order for the signal.h header to provide the necessary
declarations.  For example, in some modes _GNU_SOURCE and _XOPEN_SOURCE must
be defined.
*/
#ifndef USE_SIGACTION_FOR_SEGV_FAULT_INFO
#define USE_SIGACTION_FOR_SEGV_FAULT_INFO FALSE
#endif /* ifndef USE_SIGACTION_FOR_SEGV_FAULT_INFO */

/*
Size of allocation blocks (space is requested from malloc in blocks of
this size, and is then parceled out as needed).  Should be fairly large
to reduce the work in remapping pointers in the non-alternate file
format.  Unused pieces at the ends of regions are freed when the regions
are completed, so there's no waste.  Larger blocks will be allocated if
needed (say, for incredibly large string literals formed by token
concatenation).

When USE_MMAP_FOR_MEMORY_REGIONS is TRUE, HOST_ALLOCATION_INCREMENT
must be a multiple of the host page size.  On Windows NT, when
USE_MMAP_FOR_MEMORY_REGIONS is TRUE, HOST_ALLOCATION_INCREMENT must
be a multiple of 64K.
*/
#ifndef HOST_ALLOCATION_INCREMENT
#if EDG_MSDOS
#define HOST_ALLOCATION_INCREMENT 16384
#else /* !EDG_MSDOS */
#define HOST_ALLOCATION_INCREMENT 65536
#endif /* EDG_MSDOS  */
#endif /* ifndef HOST_ALLOCATION_INCREMENT */

/*
Flag that is TRUE if memory regions should be freed early when they're no
longer needed (in part because the IL they contain has been written to file).
This flag is set to FALSE by default because it is expected that memory regions
will be phased out in the future: Any advantage gained by freeing them early is
therefore likely of limited scope.
*/
#ifndef FREE_MEMORY_REGIONS_EARLY
#define FREE_MEMORY_REGIONS_EARLY FALSE
#endif /* ifndef FREE_MEMORY_REGIONS_EARLY */
  
/*
Flag that is TRUE if zeroing the bytes of a pointer object produces a null
pointer.  (This is generally true, but not guaranteed by the C or C++
standards.)
*/
#ifndef NULL_POINTER_IS_ZERO
#define NULL_POINTER_IS_ZERO TRUE
#endif /* ifdef NULL_POINTER_IS_ZERO */

/*
Flag that is TRUE to enable some manually unrolled loops in the front end.
These are loops that are important to the overall performance of the front end,
and that are not unrolled by mainstream optimizers at somewhat high
optimization settings.
*/
#ifndef EXPLICITLY_UNROLL_CRITICAL_LOOPS
#define EXPLICITLY_UNROLL_CRITICAL_LOOPS TRUE
#endif /* EXPLICITLY_UNROLL_CRITICAL_LOOPS  */

/*
The number of include files that may be opened at any given time.
After include nesting gets this deep, the same file will be re-opened
for all other include files.  The primary source file is not included
in this count.
*/
#ifndef MAX_INCLUDE_FILES_OPEN_AT_ONCE
#define MAX_INCLUDE_FILES_OPEN_AT_ONCE 8
#endif /* ifndef MAX_INCLUDE_FILES_OPEN_AT_ONCE */

/*
Flag that is TRUE if a stack model is used to manage the include search
list and FALSE if some other model (by default, a replace-restore model)
is to be used instead.  This is the default value used to initialize
global variable stack_referenced_include_directories.

The stack model says that when an include file is opened, its directory
becomes the new primary include search directory by being added to the
front of the list of directories to search for nested include files; the
former head of the list is demoted to second place.  This model is used by
Microsoft C compilers.  An alternative model is that of pcc, in which the
current primary include search directory is removed from the search path
altogether and the new one takes its place at the head of the list; the
removed directory is then restored to the head of the list when the
include file is closed.  This is the approach that predominates on UNIX
systems.  Note that behavior in this area is left "implementation defined"
by the ANSI C standard.
*/
#ifndef STACK_REFERENCED_INCLUDE_DIRECTORIES
#if MICROSOFT_EXTENSIONS_ALLOWED
#define STACK_REFERENCED_INCLUDE_DIRECTORIES DEFAULT_MICROSOFT_MODE
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define STACK_REFERENCED_INCLUDE_DIRECTORIES FALSE
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* ifndef STACK_REFERENCED_INCLUDE_DIRECTORIES */

/*
Width at which error message lines should be wrapped to another line
(typically, a "normal" terminal width).
*/
#ifndef MAX_ERROR_OUTPUT_LINE_LENGTH
#define MAX_ERROR_OUTPUT_LINE_LENGTH 79
			/* 79, not 80, to avoid line wrap on some terminals. */
#endif /* ifndef MAX_ERROR_OUTPUT_LINE_LENGTH */

/*
The maximum depth of a template argument list to directly print.  See
an_il_to_str_output_control_block::max_template_arg_depth for more information.
*/
#ifndef MAX_ERROR_TEMPLATE_ARG_DEPTH
#define MAX_ERROR_TEMPLATE_ARG_DEPTH 6
#endif /* ifndef MAX_ERROR_TEMPLATE_ARG_DEPTH */

/*
File "name" to be used when primary input is from stdin.  This should
not be acceptable as a real file name (or at least, you should be willing to
forgo allowing an input file with this name).
*/
#define FILE_NAME_FOR_STDIN "-"

/*
Flag that is TRUE if char * pointers can be compared even if they do not
point to the same array.  This is non-ANSI, but usually okay.  It is not
okay on a PC, at least with some compilers, where only the offsets are
compared.  The ptr_in_range macro gives a convenient way
to use this flag to test that a pointer lies in a certain range.
*/
#ifndef ADDRS_NOT_IN_SAME_ARRAY_CAN_BE_COMPARED
#if EDG_MSDOS
#define ADDRS_NOT_IN_SAME_ARRAY_CAN_BE_COMPARED FALSE
#else /* !EDG_MSDOS */
#define ADDRS_NOT_IN_SAME_ARRAY_CAN_BE_COMPARED TRUE
#endif /* EDG_MSDOS */
#endif /* ifndef ADDRS_NOT_IN_SAME_ARRAY_CAN_BE_COMPARED */

/* Check that a pointer lies within a certain address range (lower bound
   included, upper bound not included, following the usual C idiom). */
#if ADDRS_NOT_IN_SAME_ARRAY_CAN_BE_COMPARED
#define ptr_in_range(ptr, start, after_end) \
  ((char *)(start) <= (char *)(ptr) && (char *)(ptr) < (char *)(after_end))
#else /* !ADDRS_NOT_IN_SAME_ARRAY_CAN_BE_COMPARED */
/* Use a cast to unsigned long if addresses cannot be directly compared. */
#define ptr_in_range(ptr, start, after_end) \
  ((unsigned long)(start) <= (unsigned long)(ptr) && \
   (unsigned long)(ptr) < (unsigned long)(after_end))
#endif /* ADDRS_NOT_IN_SAME_ARRAY_CAN_BE_COMPARED */

/*
Flag that is TRUE if multiple input files can be compiled in a single
invocation of the front end.  This is useful on systems where the cost
of forking a process is high (e.g., VMS).  Each compilation is processed
completely separately from the others (e.g., a separate IL structure is
generated for each input file).
*/
#ifndef COMPILE_MULTIPLE_SOURCE_FILES
#define COMPILE_MULTIPLE_SOURCE_FILES FALSE
#endif /* ifndef COMPILE_MULTIPLE_SOURCE_FILES */

/*
Flag that is TRUE if multiple input files can be compiled as separate
translation units of a single compilation.  This differs from
COMPILE_MULTIPLE_SOURCE_FILES in that the IL for the various translation
units is merged together to form a single IL that represents all of the
input files.
*/
#ifndef COMPILE_MULTIPLE_TRANSLATION_UNITS
#define COMPILE_MULTIPLE_TRANSLATION_UNITS FALSE
#endif /* ifndef COMPILE_MULTIPLE_TRANSLATION_UNITS */

#if COMPILE_MULTIPLE_SOURCE_FILES && COMPILE_MULTIPLE_TRANSLATION_UNITS
 #error -- COMPILE_MULTIPLE_SOURCE_FILES and \
           COMPILE_MULTIPLE_TRANSLATION_UNITS cannot both be TRUE.
#endif /* COMPILE_MULTIPLE_SOURCE_FILES &&
          COMPILE_MULTIPLE_TRANSLATION_UNITS */

#if COMPILE_MULTIPLE_TRANSLATION_UNITS && MACRO_INVOCATION_TREE_IN_IL
 #error -- COMPILE_MULTIPLE_TRANSLATION_UNITS and \
           MACRO_INVOCATION_TREE_IN_IL cannot both be TRUE.
#endif /* COMPILE_MULTIPLE_TRANSLATION_UNITS && MACRO_INVOCATION_TREE_IN_IL */

/*
Flag that is TRUE if the front end is being run from a driver program.
This suppresses sign-off messages on stderr (like "Compilation terminated."),
with the expectation that the driver will produce those.
*/
#ifndef USING_DRIVER
#define USING_DRIVER FALSE	/* Not using a driver. */
#endif /* ifndef USING_DRIVER */

/*
Flag that is TRUE if a signoff message should be written at the end of
the compilation, giving the count of errors; such a message is only
written if there are errors.
*/
#ifndef WRITE_SIGNOFF_MESSAGE
#define WRITE_SIGNOFF_MESSAGE (!USING_DRIVER)
#endif /* ifndef WRITE_SIGNOFF_MESSAGE */

/*
Flag that is TRUE if the string "error" should be included in error
messages.  If this flag is false only warnings and remarks have their
severity explicitly included in the message.
*/
#ifndef ERROR_SEVERITY_EXPLICIT_IN_ERROR_MESSAGES
#define ERROR_SEVERITY_EXPLICIT_IN_ERROR_MESSAGES TRUE
#endif /* ifndef ERROR_SEVERITY_EXPLICIT_IN_ERROR_MESSAGES */

/*
Flag that is TRUE if "brief" diagnostics (each diagnostic is all on
one line) should be put out.  This is the initial value of the variable
brief_diagnostics, which can be overridden by the --[no]brief_diagnostics
command-line option.
*/
#ifndef DEFAULT_BRIEF_DIAGNOSTICS
#define DEFAULT_BRIEF_DIAGNOSTICS FALSE
#endif /* ifndef DEFAULT_BRIEF_DIAGNOSTICS */

/*
Flag that is TRUE if notes should be added to provide details about overload
resolution failures.  (This is the initial value of the variable
add_match_notes, which can be overridden by the --[no_]add_match_notes
command-line option.
*/
#ifndef DEFAULT_ADD_MATCH_NOTES
#define DEFAULT_ADD_MATCH_NOTES TRUE
#endif /* ifndef DEFAULT_ADD_MATCH_NOTES */

/*
TRUE if the column number should be included as part of the diagnostic
output in brief diagnostics mode.
*/
#ifndef COLUMN_NUMBER_IN_BRIEF_DIAGNOSTICS
#define COLUMN_NUMBER_IN_BRIEF_DIAGNOSTICS TRUE
#endif /* ifndef COLUMN_NUMBER_IN_BRIEF_DIAGNOSTICS */

/*
Flag that is TRUE to display error numbers in diagnostics.  This is the
default value for the flag that can be modified by a command line option.
*/
#ifndef DEFAULT_DISPLAY_ERROR_NUMBER
#define DEFAULT_DISPLAY_ERROR_NUMBER FALSE
#endif /* ifndef DEFAULT_DISPLAY_ERROR_NUMBER */

/*
The maximum number of instantiation contexts that should be displayed as
part of a diagnostic.  If this limit is exceeded, the first N and last N
contexts are displayed, where N is half of the limit value.  If the limit
is specified as zero, all of the contexts are displayed.  This is the
initial value of the variable context_limit, which can be overridden by the
--context_limit command-line option.
*/
#ifndef DEFAULT_CONTEXT_LIMIT
#define DEFAULT_CONTEXT_LIMIT 10
#endif /* ifndef DEFAULT_CONTEXT_LIMIT */

/*
Flag that is TRUE if typedefs declared in template classes should not be
replaced with the underlying type in diagnostic output.  Removing such
typedefs generally produces better diagnostics.  This macro is provided
to preserve the diagnostic behavior of earlier versions of the front end.
*/
#ifndef DEFAULT_DISPLAY_TEMPLATE_TYPEDEFS_IN_DIAGNOSTICS
#define DEFAULT_DISPLAY_TEMPLATE_TYPEDEFS_IN_DIAGNOSTICS FALSE
#endif /* ifndef DEFAULT_DISPLAY_TEMPLATE_TYPEDEFS_IN_DIAGNOSTICS */

#if FULLY_RESOLVED_MACRO_POSITIONS
/*
Flag that is TRUE if diagnostics referring to text in macro expansions should
include information about the original position from which the text was copied
and, if RECORD_MACRO_INVOCATIONS is TRUE, the macro invocation stack in
effect at that point.  This is the initial value of the variable
macro_positions_in_diagnostics, which can be overridden by the
--[no_]macro_positions_in_diagnostics command-line option.
*/
#ifndef DEFAULT_MACRO_POSITIONS_IN_DIAGNOSTICS
#define DEFAULT_MACRO_POSITIONS_IN_DIAGNOSTICS FALSE
#endif /* ifndef DEFAULT_MACRO_POSITIONS_IN_DIAGNOSTICS */
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */

/*
Flag that is TRUE if diagnostic output should be directed to stdout instead
of stderr, except when doing preprocessing only (i.e., the way that the
Microsoft compiler outputs diagnostics).
*/
#ifndef DIRECT_ERROR_OUTPUT_TO_STDOUT
#define DIRECT_ERROR_OUTPUT_TO_STDOUT FALSE
#endif /* ifndef DIRECT_ERROR_OUTPUT_TO_STDOUT */

/*
Enable the EDG front end daemon implementation code instead of the traditional
single process implementation.  This configuration is intended primarily for
internal testing and as a demo of a possible deployment of the front end.
*/
#ifndef EDG_FRONT_END_DAEMON
#define EDG_FRONT_END_DAEMON FALSE
#endif /* ifndef EDG_FRONT_END_DAEMON */

/*
Is the C-generating back end being used as the back end?
See also C_GEN_BE_GENERATES_ANSI_C et al. in targ_def.h.
Note that this means we're using the C-generating back end,
but not necessarily in the current program -- e.g., it can
be TRUE in the standalone IL display program.
*/
#ifndef BACK_END_IS_C_GEN_BE
#define BACK_END_IS_C_GEN_BE TRUE  /* You can change this. */
#endif /* ifndef BACK_END_IS_C_GEN_BE */

/*
This switch controls whether a post-pass is done after IL lowering to ensure
that the types list is in order, in the sense that the C-generating back end
can generate compilable code from it.  (Other code-generating back ends may
also require such an ordering.)
*/
#ifndef ENSURE_LOWERED_TYPE_LIST_ORDERING
#define ENSURE_LOWERED_TYPE_LIST_ORDERING BACK_END_IS_C_GEN_BE
#endif /* ifndef ENSURE_LOWERED_TYPE_LIST_ORDERING */

#if BACK_END_IS_C_GEN_BE && !ENSURE_LOWERED_TYPE_LIST_ORDERING
 #error BACK_END_IS_C_GEN_BE requires ENSURE_LOWERED_TYPE_LIST_ORDERING
#endif /* BACK_END_IS_C_GEN_BE && !ENSURE_LOWERED_TYPE_LIST_ORDERING */

/*
Is the C++-generating back end being used as the back end?
Note that this means we're using the C++-generating back end,
but not necessarily in the current program -- e.g., it can
be TRUE in the standalone IL display program.
*/
#ifndef BACK_END_IS_CP_GEN_BE
#define BACK_END_IS_CP_GEN_BE FALSE  /* You can change this. */
#endif /* ifndef BACK_END_IS_CP_GEN_BE */

#if BACK_END_IS_C_GEN_BE && BACK_END_IS_CP_GEN_BE
 #error -- BACK_END_IS_C_GEN_BE and BACK_END_IS_CP_GEN_BE cannot both be TRUE.
#endif /* BACK_END_IS_C_GEN_BE && BACK_END_IS_CP_GEN_BE */

#if BACK_END_IS_CP_GEN_BE && COMPILE_MULTIPLE_TRANSLATION_UNITS
 #error -- COMPILE_MULTIPLE_TRANSLATION_UNITS cannot be used with the \
           C++-generating back end.
#endif /* BACK_END_IS_CP_GEN_BE && COMPILE_MULTIPLE_TRANSLATION_UNITS */

/*
When the C-generating back end (c_gen_be) or, in C mode, the C++/C-generating
back end (cp_gen_be) is run, this is the suffix appended to the base of the
primary source file to get the name of the generated C output file.
*/
#ifndef GEN_C_FILE_SUFFIX
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
#if EDG_MSDOS
/* File names under MSDOS cannot have multiple periods. */
#define GEN_C_FILE_SUFFIX ".ic"
#else /* !EDG_MSDOS */
#define GEN_C_FILE_SUFFIX ".int.c"
#endif /* if EDG_MSDOS */
#endif /* BACK_END_IS_C_GEN_BE || ... */
#endif /* ifndef GEN_C_FILE_SUFFIX */

/*
Flag that controls whether string literals should be lowered to non-const
types during lowering.  The goal of lowering is to produce C, and in the C
language string literals are non-const.  However, setting this flag to TRUE
may have other side-effects, for example, not being able to place strings into
read-only storage.  This flag defaults to TRUE when the C-generating back end
is used.
*/
#ifndef LOWER_STRING_LITERALS_TO_NON_CONST
#if BACK_END_IS_C_GEN_BE
#define LOWER_STRING_LITERALS_TO_NON_CONST TRUE
#else /* !BACK_END_IS_C_GEN_BE */
#define LOWER_STRING_LITERALS_TO_NON_CONST FALSE
#endif /* BACK_END_IS_C_GEN_BE */
#endif /* ifndef LOWER_STRING_LITERALS_TO_NON_CONST */

/*
When the C++/C-generating back end (cp_gen_be) is run in C++ mode, this is the
suffix appended to the base of the primary source file to get the name of the
generated C++ output file.
*/
#ifndef GEN_CPP_FILE_SUFFIX
#if BACK_END_IS_CP_GEN_BE
#define GEN_CPP_FILE_SUFFIX GEN_C_FILE_SUFFIX
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* ifndef GEN_CPP_FILE_SUFFIX */

/*
The flag STANDALONE_IL_DISPLAY is set to TRUE when compiling the standalone
IL display utility.  It should be set on the command line if needed;
the code here should not be changed.
*/
#ifndef STANDALONE_IL_DISPLAY
#define STANDALONE_IL_DISPLAY FALSE /* Do not change this. */
#endif /* ifndef STANDALONE_IL_DISPLAY */

/*
The flag STANDALONE_C_GEN_BE is set to TRUE when compiling the standalone
C-generating back end c_gen_be.  It should be set on the command 
line if needed; the code here should not be changed.
*/
#ifndef STANDALONE_C_GEN_BE
#define STANDALONE_C_GEN_BE FALSE /* Do not change this. */
#endif /* ifndef STANDALONE_C_GEN_BE */

/*
The flag STANDALONE_CP_GEN_BE is set to TRUE when compiling the standalone
C++/C-generating back end cp_gen_be.  It should be set on the command 
line if needed; the code here should not be changed.
*/
#ifndef STANDALONE_CP_GEN_BE
#define STANDALONE_CP_GEN_BE FALSE /* Do not change this. */
#endif /* ifndef STANDALONE_CP_GEN_BE */

/*
The flag STANDALONE_UTILITY_PROGRAM is set to TRUE when compiling one of
the standalone utility programs (the C-generating back end c_gen_be, the
C++/C generating back end cp_gen_be, or the IL display utility il_display).
It is forced to TRUE if STANDALONE_IL_DISPLAY, STANDALONE_C_GEN_BE,
or STANDALONE_CP_GEN_BE is TRUE.
*/
#if STANDALONE_IL_DISPLAY || STANDALONE_C_GEN_BE || STANDALONE_CP_GEN_BE
#define STANDALONE_UTILITY_PROGRAM TRUE /* Do not change this. */
#else /* !(STANDALONE_IL_DISPLAY || STANDALONE_C_GEN_BE || ...) */
#ifndef STANDALONE_UTILITY_PROGRAM
#define STANDALONE_UTILITY_PROGRAM FALSE  /* Do not change this. */
#endif /* ifndef STANDALONE_UTILITY_PROGRAM */
#endif /* STANDALONE_IL_DISPLAY || STANDALONE_C_GEN_BE || ... */

/*
Flag that is TRUE if the code necessary to display the IL in a readable
form on stdout is to be compiled.  This flag may be set on the command
line or will be forced to TRUE if STANDALONE_IL_DISPLAY is TRUE.
This must be TRUE for the --il_display command-line option to be enabled.
*/
#if STANDALONE_IL_DISPLAY
#define NEED_IL_DISPLAY TRUE /* Do not change this. */
#else /* !STANDALONE_IL_DISPLAY */
#ifndef NEED_IL_DISPLAY
#if DEBUG
#define NEED_IL_DISPLAY TRUE
#else /* !DEBUG */
#define NEED_IL_DISPLAY FALSE
#endif /* DEBUG */
#endif /* ifndef NEED_IL_DISPLAY */
#endif /* STANDALONE_IL_DISPLAY */

/*
Flag that is TRUE if the intermediate language should be written to a file.
FALSE means the IL is passed in memory to the back end.
*/
#if STANDALONE_UTILITY_PROGRAM
#ifndef IL_SHOULD_BE_WRITTEN_TO_FILE
#define IL_SHOULD_BE_WRITTEN_TO_FILE TRUE /* Do not change this. */
#else /* defined(IL_SHOULD_BE_WRITTEN_TO_FILE) */
#if !IL_SHOULD_BE_WRITTEN_TO_FILE
 #error -- IL_SHOULD_BE_WRITTEN_TO_FILE must be TRUE when \
           STANDALONE_UTILITY_PROGRAM is set.
#endif /* !IL_SHOULD_BE_WRITTEN_TO_FILE */
#endif /* !defined(IL_SHOULD_BE_WRITTEN_TO_FILE) */
#else /* !STANDALONE_UTILITY_PROGRAM */
#ifndef IL_SHOULD_BE_WRITTEN_TO_FILE
#define IL_SHOULD_BE_WRITTEN_TO_FILE FALSE
#endif /* ifndef IL_SHOULD_BE_WRITTEN_TO_FILE */
#endif /* STANDALONE_UTILITY_PROGRAM */

/*
Flag that is TRUE to cause IL lowering to be done, to lower C++ intermediate
language to C intermediate language, allowing the C++ front end to be used
with a C back end.
*/
#ifndef DO_IL_LOWERING
#if BACK_END_IS_CP_GEN_BE
#define DO_IL_LOWERING FALSE
#else /* !BACK_END_IS_CP_GEN_BE */
#if defined(DOING_SOURCE_ANALYSIS) && DOING_SOURCE_ANALYSIS
#define DO_IL_LOWERING FALSE
#else /* !(defined(DOING_SOURCE_ANALYSIS) && DOING_SOURCE_ANALYSIS) */
#define DO_IL_LOWERING TRUE
#endif /* defined(DOING_SOURCE_ANALYSIS) && DOING_SOURCE_ANALYSIS */
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* ifndef DO_IL_LOWERING */
#if BACK_END_IS_C_GEN_BE && !DO_IL_LOWERING
 #error -- IL lowering must be done for the C-generating back end.
#endif /* BACK_END_IS_C_GEN_BE && !DO_IL_LOWERING */
#ifndef ALLOW_CPPCLI_AND_CPPCX_WITH_LOWERING
#define ALLOW_CPPCLI_AND_CPPCX_WITH_LOWERING FALSE
#endif /* ALLOW_CPPCLI_AND_CPPCX_WITH_LOWERING */
#if CPPCLI_ENABLING_POSSIBLE && DO_IL_LOWERING
#if ALLOW_CPPCLI_AND_CPPCX_WITH_LOWERING
/* Okay, the user has said "trust me, I know what I'm doing."  IL lowering
   and the back end will be suppressed whenever C++/CLI or C++/CX is enabled.
   This is really intended only for testing within EDG. */
#else /* !ALLOW_CPPCLI_AND_CPPCX_WITH_LOWERING */
 #error -- IL lowering cannot be done when C++/CLI enabling is allowed.
#endif /* ALLOW_CPPCLI_AND_CPPCX_WITH_LOWERING */
#endif /* CPPCLI_ENABLING_POSSIBLE && DO_IL_LOWERING */

/*
If the IL is written to a file, this flag selects the file format.
The "usual" form (flag FALSE) is written out and read back in as
large blocks of memory, and a tree walk is required on the receiving
side.  The "alternate" form (flag TRUE) requires a tree walk on the
sending side, and is written and read in single-entry chunks, with
each entry preceded by the entry kind and an identifying number.
The advantage of the alternate form is that it allows alteration of
the entries on the receiving side (e.g., enlarging them to add extra
information required in the back end); the disadvantage is that it's 
slower.
*/
#if IL_SHOULD_BE_WRITTEN_TO_FILE
#ifndef ALTERNATE_IL_FILE_FORMAT
#define ALTERNATE_IL_FILE_FORMAT TRUE
#endif /* ifndef ALTERNATE_IL_FILE_FORMAT */
#else /* !IL_SHOULD_BE_WRITTEN_TO_FILE */
#define ALTERNATE_IL_FILE_FORMAT FALSE /* Do not change this. */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

/*
When this flag is TRUE, any prototype instantiations of function definitions
that are done are included in the IL.  When the flag is FALSE, prototype
instantiations of function definitions may or may not be done, depending
on other modes, but the definition generated (if any) will not be included
in the IL.  If PROTOTYPE_INSTANTIATIONS_IN_IL is already defined, use that
as the basis of the value for this flag.
*/
#ifndef ALL_TEMPLATE_INFO_IN_IL
#ifdef PROTOTYPE_INSTANTIATIONS_IN_IL
#if PROTOTYPE_INSTANTIATIONS_IN_IL
#define ALL_TEMPLATE_INFO_IN_IL TRUE
#else /* !PROTOTYPE_INSTANTIATIONS_IN_IL */
#define ALL_TEMPLATE_INFO_IN_IL FALSE
#endif /* PROTOTYPE_INSTANTIATIONS_IN_IL */
#else /* ifndef PROTOTYPE_INSTANTIATIONS_IN_IL */
#define ALL_TEMPLATE_INFO_IN_IL FALSE
#endif /* ifdef PROTOTYPE_INSTANTIATIONS_IN_IL */
#endif /* ifndef ALL_TEMPLATE_INFO_IN_IL */

/*
A prototype instantiation results from parsing and analyzing a template
without substituting actual template argument entities for the formal
parameters.  This flag should be set to TRUE if such structured but abstract
representations should be recorded in the IL.  (It is typically not needed
for direct code generation.)  It is the default value of the variable
prototype_instantiations_in_il.  Note that prototype instantiations cannot
be generated when doing IL lowering.

This flag must now be set to TRUE.  The flag and the tests that use it
will be removed at some point.
*/
#ifndef PROTOTYPE_INSTANTIATIONS_IN_IL
#define PROTOTYPE_INSTANTIATIONS_IN_IL TRUE
#endif /* ifndef PROTOTYPE_INSTANTIATIONS_IN_IL */

#if !PROTOTYPE_INSTANTIATIONS_IN_IL
  #error PROTOTYPE_INSTANTIATIONS_IN_IL can no longer be set to FALSE
#endif /* !PROTOTYPE_INSTANTIATIONS_IN_IL */

/*
In some modes, the prototype instantiation of functions is deferred until
the first actual instantiation of the template.  In versions that do not
include prototype instantiations in the IL, the default for this macro
is based on whether or not the C++-generating back end is being used.  This
is done so that (by default) all C++-generating back end versions will
have the same behavior with respect to deferral of prototype instantiations.
This is desirable because deferral of prototype instantiations changes the
set of programs that can be compiled without errors.
*/
#ifndef FUNCTION_PROTOTYPE_INSTANTIATION_DEFERRAL_ALLOWED
#if DO_IL_LOWERING
#define FUNCTION_PROTOTYPE_INSTANTIATION_DEFERRAL_ALLOWED TRUE
#else /* !DO_IL_LOWERING */
#if ALL_TEMPLATE_INFO_IN_IL
#define FUNCTION_PROTOTYPE_INSTANTIATION_DEFERRAL_ALLOWED FALSE
#else /* !ALL_TEMPLATE_INFO_IN_IL */
#if BACK_END_IS_CP_GEN_BE
#define FUNCTION_PROTOTYPE_INSTANTIATION_DEFERRAL_ALLOWED FALSE
#else /* !BACK_END_IS_CP_GEN_BE */
#define FUNCTION_PROTOTYPE_INSTANTIATION_DEFERRAL_ALLOWED TRUE
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* ALL_TEMPLATE_INFO_IN_IL */
#endif /* DO_IL_LOWERING */
#endif /* ifndef FUNCTION_PROTOTYPE_INSTANTIATION_DEFERRAL_ALLOWED */

/*
Flag that is TRUE if, when deferring function prototype instantiations,
partial specialization members should not have their prototype instantiations
deferred.  This feature was added to work around the fact that parsing code
in an unused function template or member function of template class can
result in the instantiation of a class, and a different result would
be obtained if the instantiation were done later.  Compilers should
complain about such position-dependencies in partial specializations, but
at this point, it appears that only the EDG front end does.  This is
essentially a heuristic to allow some open source applications to build.
This feature can be enabled by default using this macro, or it can be
enabled using a --set_flag option on the command line.  The value is
only used if defer_function_prototype_instantiations is TRUE.
*/
#ifndef DEFAULT_SUPPRESS_DEFERRAL_ON_PARTIAL_SPEC_MEMBERS
#define DEFAULT_SUPPRESS_DEFERRAL_ON_PARTIAL_SPEC_MEMBERS FALSE
#endif /* ifndef DEFAULT_SUPPRESS_DEFERRAL_ON_PARTIAL_SPEC_MEMBERS */

/*
Flag that is TRUE if object code compatibility with USL's cfront is
required.  Some features of cfront changed from release 2.1 to release 3.0,
and there are flags for compatibility with a specific version.  For
example, release 2.1 provided a special feature to ease the transition
between non-nested classes and nested classes.  This feature was removed
for release 3.0.  Either the 2.1 or 3.0 flag should be set to designate
the variety of cfront compatibility that is desired.

Note that there is also an IA64 ABI (see below), and a cfront-like
ABI that eliminates a few of the cfront weirdnesses (such as
wasteful allocation of virtual base classes), which is selected
by having both IA64_ABI and CFRONT_OBJECT_CODE_COMPATIBILITY FALSE.

This is really a target configuration macro, but it needs to be here
because of some ordering problems.
*/
#ifndef CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
#define CFRONT_2_1_OBJECT_CODE_COMPATIBILITY FALSE
#endif /* !defined(CFRONT_2_1_OBJECT_CODE_COMPATIBILITY) */
#ifndef CFRONT_3_0_OBJECT_CODE_COMPATIBILITY
#define CFRONT_3_0_OBJECT_CODE_COMPATIBILITY FALSE
#endif /* !defined(CFRONT_3_0_OBJECT_CODE_COMPATIBILITY) */
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY||CFRONT_3_0_OBJECT_CODE_COMPATIBILITY
#define CFRONT_OBJECT_CODE_COMPATIBILITY TRUE
#else /* !(CFRONT_2_1_...) */
#define CFRONT_OBJECT_CODE_COMPATIBILITY FALSE
#endif /* CFRONT_2_1_... */

/*
TRUE if the IA-64 ABI should be used.  This is a "modern" C++ object
layout standard (unlike the cfront ABI), and is a good starting point
even on architectures other than IA-64 (it's the default for a lot
of 3.x versions of g++).  See https://itanium-cxx-abi.github.io/cxx-abi/.

This is really a target configuration macro, but it needs to be here
because of some ordering problems.
*/
#ifndef IA64_ABI
#define IA64_ABI FALSE
#endif /* ifndef IA64_ABI */

#if CFRONT_OBJECT_CODE_COMPATIBILITY && IA64_ABI
 #error -- Cfront and IA-64 ABIs are mutually exclusive.
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY && IA64_ABI */

/*
Flag that is TRUE if the environment (in particular, the linker) is
capable of discarding extra copies of definitions, e.g., of functions.
In such an environment, one can use an instantiation mechanism that
instantiates templates wherever they are used, and counts on the
linker to discard the extra copies.  The fact that one can do so
does not mean one wants to -- see INSTANTIATE_TEMPLATES_EVERYWHERE_USED.
Note that a simplified interpretation of this flag is "Do we have
COMDAT sections?"
*/
#ifndef LINKER_CAN_DISCARD_DUPLICATE_DEFINITIONS
#if IA64_ABI
#define LINKER_CAN_DISCARD_DUPLICATE_DEFINITIONS TRUE
#else /* !IA64_ABI */
#define LINKER_CAN_DISCARD_DUPLICATE_DEFINITIONS FALSE
#endif /* IA64_ABI */
#endif /* ifndef LINKER_CAN_DISCARD_DUPLICATE_DEFINITIONS */

#if !LINKER_CAN_DISCARD_DUPLICATE_DEFINITIONS && IA64_ABI
 #error -- LINKER_CAN_DISCARD_DUPLICATE_DEFINITIONS must be TRUE for IA-64 ABI
#endif /* !LINKER_CAN_DISCARD_DUPLICATE_DEFINITIONS && IA64_ABI */

/*
Flag that is TRUE if templates should be instantiated everywhere they
are used, with duplicates discarded by the linker.  This approach
does not require a prelinker, and setting this mode disables the
prelinker by default.  Note that without a prelinker we cannot
implement exported templates.
*/
#ifndef INSTANTIATE_TEMPLATES_EVERYWHERE_USED
#define INSTANTIATE_TEMPLATES_EVERYWHERE_USED FALSE
#endif /* ifndef INSTANTIATE_TEMPLATES_EVERYWHERE_USED */

#if INSTANTIATE_TEMPLATES_EVERYWHERE_USED && \
    !LINKER_CAN_DISCARD_DUPLICATE_DEFINITIONS
 #error -- Cannot instantiate templates where used without linker support
#endif /* INSTANTIATE_TEMPLATES_EVERYWHERE_USED && ... */

/*
The default template instantiation mode, which controls which templates
should be instantiated without a specific request from the prelinker
or an explicit instantiation directive (none of them, those that are used,
all of them, ...).  This is the default value of the global variable
instantiation_mode, which can be changed by a command-line option.
*/
#ifndef DEFAULT_INSTANTIATION_MODE
#if INSTANTIATE_TEMPLATES_EVERYWHERE_USED
#define DEFAULT_INSTANTIATION_MODE tim_used
#else /* !INSTANTIATE_TEMPLATES_EVERYWHERE_USED */
#define DEFAULT_INSTANTIATION_MODE tim_none
#endif /* INSTANTIATE_TEMPLATES_EVERYWHERE_USED */
#endif /* ifndef DEFAULT_INSTANTIATION_MODE */

/*
The default output mode, which controls how output is reported.  This is the
default value of the global variable output_mode, which can be changed by a
command-line option.
*/
#ifndef DEFAULT_OUTPUT_MODE
#define DEFAULT_OUTPUT_MODE om_text
#endif /* ifndef DEFAULT_OUTPUT_MODE */

/*
Flag that is TRUE if the front end should default to hiding EDG-specific
macros (and thus attempt to conceal that the EDG front end is being used).
This is the default value of the global variable incognito, which can be
changed by a command-line option.
*/
#ifndef DEFAULT_INCOGNITO
#define DEFAULT_INCOGNITO FALSE
#endif /* ifndef DEFAULT_INCOGNITO */

/*
Flag that is TRUE if, when a precompiled header file is generated, any
instantiations that are needed will be done before the PCH file is generated
so that they will not have to be generated for each file that uses the PCH
file.
*/
#ifndef INSTANTIATE_BEFORE_PCH_CREATION
#if INSTANTIATE_TEMPLATES_EVERYWHERE_USED
#define INSTANTIATE_BEFORE_PCH_CREATION TRUE
#else /* !INSTANTIATE_TEMPLATES_EVERYWHERE_USED */
#define INSTANTIATE_BEFORE_PCH_CREATION FALSE
#endif /* INSTANTIATE_TEMPLATES_EVERYWHERE_USED */
#endif /* ifndef INSTANTIATE_BEFORE_PCH_CREATION */

/*
Flag that is TRUE to enable automatic instantiation support for templates.
This flag determines whether the code for automatic instantiation is
to be compiled.  When automatic instantiation is on, the front end
generates information for a prelinker, and the prelinker controls the
instantiation process at link time.
*/
#ifndef AUTOMATIC_TEMPLATE_INSTANTIATION
#if BACK_END_IS_CP_GEN_BE
/* Automatic template instantiation is generally not a good idea when doing
   source-to-source translation. */
#define AUTOMATIC_TEMPLATE_INSTANTIATION FALSE
#else /* !BACK_END_IS_CP_GEN_BE */
#if INSTANTIATE_TEMPLATES_EVERYWHERE_USED
#define AUTOMATIC_TEMPLATE_INSTANTIATION FALSE
#else /* !INSTANTIATE_TEMPLATES_EVERYWHERE_USED */
#define AUTOMATIC_TEMPLATE_INSTANTIATION TRUE
#endif /* INSTANTIATE_TEMPLATES_EVERYWHERE_USED */
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* ifndef AUTOMATIC_TEMPLATE_INSTANTIATION */

#if AUTOMATIC_TEMPLATE_INSTANTIATION
/*
Flag that is TRUE if automatic instantiation processing is to be
performed by default.  This flag does not affect whether code is
compiled but rather determines whether the automatic instantiation
processing is to be performed when the compiler is executed.  This is
the default value for the global flag automatic_instantiation_mode,
the value of which may be modified using command line options.
*/
#ifndef DEFAULT_AUTOMATIC_INSTANTIATION_MODE
#define DEFAULT_AUTOMATIC_INSTANTIATION_MODE TRUE
#endif /* !defined(DEFAULT_AUTOMATIC_INSTANTIATION_MODE) */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */

/*
There are two conventions used for template instantiation.  One mode
requires that the bodies for noninline template functions and static
data members be explicitly included by the user.  The other
causes a source file (e.g., a .c file) to be implicitly included
to provide the definitions of the noninline template functions and
static data members.  If INSTANTIATION_BY_IMPLICIT_INCLUSION is TRUE
the implicit inclusion is performed.  If it is FALSE implicit
inclusion is not performed.  This feature emulates the template
instantiation source model required by Cfront.
*/
#ifndef INSTANTIATION_BY_IMPLICIT_INCLUSION
#define INSTANTIATION_BY_IMPLICIT_INCLUSION TRUE
#endif /* ifndef INSTANTIATION_BY_IMPLICIT_INCLUSION */

#if INSTANTIATION_BY_IMPLICIT_INCLUSION
/*
Flag that is TRUE if implicit inclusion of template definition files
is to be performed by default.  This flag does not affect whether code
is compiled but rather determines whether the implicit inclusion
processing is to be performed when the compiler is executed.  This is
the default value for the global flag
implicit_template_inclusion_mode, the value of which may be modified
using command line options.
*/
#ifndef DEFAULT_IMPLICIT_TEMPLATE_INCLUSION_MODE
#define DEFAULT_IMPLICIT_TEMPLATE_INCLUSION_MODE FALSE
#endif /* !defined(DEFAULT_IMPLICIT_TEMPLATE_INCLUSION_MODE) */
#endif /* ifndef INSTANTIATION_BY_IMPLICIT_INCLUSION */

/*
The default value for the maximum number of pending instantiations of a
given template that may be in process at a given time.  This is the initial
value of the variable max_pending_instantiations, which can be overridden
by the --pending_instantiations command-line option.
*/
#ifndef DEFAULT_MAX_PENDING_INSTANTIATIONS
/* Use the old MAX_PENDING_INSTANTIATIONS value, if one is set. */
#ifdef MAX_PENDING_INSTANTIATIONS
#define DEFAULT_MAX_PENDING_INSTANTIATIONS MAX_PENDING_INSTANTIATIONS
#else /* !ifndef MAX_PENDING_INSTANTIATIONS */
#define DEFAULT_MAX_PENDING_INSTANTIATIONS 64
#endif /* ifndef MAX_PENDING_INSTANTIATIONS */
#endif /* ifndef DEFAULT_MAX_PENDING_INSTANTIATIONS */

/*
The maximum number of function instantiations that can be in progress
at a given point in time.  If this is exceeded, the instantiations are
still done, but are done later.  Note that this controls the maximum
number of function instantiations that can take place at once
regardless of whether the instantiations are from a single template or
many templates. This differs from DEFAULT_MAX_PENDING_INSTANTIATIONS
which limits the number of instantiations of a particular template
that may be in progress at a given time.

This is used to limit the amount of memory used for memory regions
while generating instantiations.  When a function definition begins, a
new memory region of HOST_ALLOCATION_INCREMENT bytes is allocated.
Most of the memory region will later be made available for reuse, but
the available memory can sometimes be exhausted if a program causes a
large number of concurrent instantiations to be performed.
*/
#ifndef MAX_TOTAL_PENDING_INSTANTIATIONS
#define MAX_TOTAL_PENDING_INSTANTIATIONS 256
#endif /* ifndef MAX_TOTAL_PENDING_INSTANTIATIONS */

/*
The maximum number of unused instantiations of a given template function
that may be generated.  Unused instantiations can be generated in tim_all.
For example, in tim_all mode uncalled member functions, and functions
for which only a declaration is seen, are instantiated.  This number
should be fairly large because, unlike true recursive instantiations,
some number of unused instantiations will be generated in normal use
of tim_all mode.
*/
#ifndef MAX_UNUSED_ALL_MODE_INSTANTIATIONS
#define MAX_UNUSED_ALL_MODE_INSTANTIATIONS 200
#endif /* ifndef MAX_UNUSED_INSTANTIATIONS */

/*
The maximum depth of constexpr function and constructor call nesting.
If we reach the maximum, the next call is considered non-foldable,
which probably makes the overall expression non-constant.  The C++
standard requires at least 512.  Initial value for the global variable
max_depth_constexpr_call.  The maximum value that can be used depends
on the size of data structures (e.g., 64-bit configurations will use
more space than 32-bit) and the compiler optimization level.  Use of
a value that is too large can result in stack overflow, so if the default
is increased, it should be done with care.

By default, DEBUG builds have a lower maximum under the assumption that
such builds will be not use optimization and consequently may not be
able to handle a large call depth.
*/
#ifndef DEFAULT_MAX_DEPTH_CONSTEXPR_CALL
#if DEBUG
#define DEFAULT_MAX_DEPTH_CONSTEXPR_CALL 256
#else /* !DEBUG */
#define DEFAULT_MAX_DEPTH_CONSTEXPR_CALL 512
#endif /* DEBUG */
#endif /* ifndef DEFAULT_MAX_DEPTH_CONSTEXPR_CALL */

/*
The default maximum cost of a C++14-style ("relaxed") constexpr function or
constructor call evaluation.  Two units of cost are counted for every call and
one unit for every loop "back" branch.    If we reach the maximum, the next
call is considered non-foldable, which probably makes the overall expression
non-constant.  The C++14 standard has no specific minimum value.  Initial
value for the global variable max_constexpr_call_cost.
*/
#ifndef DEFAULT_MAX_COST_CONSTEXPR_CALL
#define DEFAULT_MAX_COST_CONSTEXPR_CALL 2000000
#endif /* ifndef DEFAULT_MAX_COST_CONSTEXPR_CALL */


/*
Flag that is TRUE if "#pragma define_type_info" is required by default before
a declaration of class "type_info" to identify it as an explicit declaration
of the predeclared class "type_info".  This is the initial value of the global
variable pragma_define_type_info_is_required.
*/
#ifndef PRAGMA_DEFINE_TYPE_INFO_IS_REQUIRED
#if BACK_END_IS_CP_GEN_BE
#define PRAGMA_DEFINE_TYPE_INFO_IS_REQUIRED FALSE  /* You can change this. */
#else /* !BACK_END_IS_CP_GEN_BE */
#define PRAGMA_DEFINE_TYPE_INFO_IS_REQUIRED TRUE   /* You can change this. */
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* ifndef PRAGMA_DEFINE_TYPE_INFO_IS_REQUIRED */

/*
Flag that is TRUE if the source_corresp.needed flag in IL entries and the
definition_needed flag in class/struct/union type entries should be
maintained.
*/
#ifndef MAINTAIN_NEEDED_FLAGS
#if BACK_END_IS_C_GEN_BE
#define MAINTAIN_NEEDED_FLAGS TRUE  /* You can change this. */
#else /* !BACK_END_IS_C_GEN_BE */
#define MAINTAIN_NEEDED_FLAGS FALSE  /* You can change this. */
#endif /* BACK_END_IS_C_GEN_BE */
#endif /* ifndef MAINTAIN_NEEDED_FLAGS */

/*
Default setting for global variable remove_unneeded_entities, used to
initialized okay_to_eliminate_unneeded_il_entries, which in turn controls
whether the IL tree is pruned of unneeded entries.  It should only be
configured to TRUE when MAINTAIN_NEEDED_FLAGS is TRUE.  Usually it will be
FALSE when the C++-generating back end is used because needed-flag
processing cannot be done reliably on template bodies.
*/
#ifndef DEFAULT_REMOVE_UNNEEDED_ENTITIES
#if MAINTAIN_NEEDED_FLAGS
#if BACK_END_IS_CP_GEN_BE
#define DEFAULT_REMOVE_UNNEEDED_ENTITIES FALSE   /* You can change this. */
#else /* !BACK_END_IS_CP_GEN_BE */
#define DEFAULT_REMOVE_UNNEEDED_ENTITIES TRUE    /* You can change this. */
#endif /* BACK_END_IS_CP_GEN_BE */
#else /* !MAINTAIN_NEEDED_FLAGS */
#define DEFAULT_REMOVE_UNNEEDED_ENTITIES FALSE   /* Do not change this. */
#endif /* MAINTAIN_NEEDED_FLAGS */
#endif /* ifndef DEFAULT_REMOVE_UNNEEDED_ENTITIES */

#if !MAINTAIN_NEEDED_FLAGS && DEFAULT_REMOVE_UNNEEDED_ENTITIES
 #error -- DEFAULT_REMOVE_UNNEEDED_ENTITIES must be FALSE \
           when MAINTAIN_NEEDED_FLAGS is FALSE.
#endif /* !MAINTAIN_NEEDED_FLAGS && ... */

/*
Flag that is TRUE if the processing required to generate one instantiation
per object file should be included.  This involves maintaining a separate
"needed" flag for each instantiation so that each file that is output
contains only what is required.  That takes extra space and, if the
feature is enabled (via --one_instantiation_per_object), extra time.
However, it's useful bordering on essential for building libraries.
This feature is incompatible with earlier drivers, so it is only enabled
for driver versions >= 2.37.  It also requires back end support, so it
is disabled by default except when using the C generating back end.
*/
#ifndef ONE_INSTANTIATION_PER_OBJECT
#if DRIVER_COMPATIBILITY_VERSION >= 237
#if BACK_END_IS_C_GEN_BE
#define ONE_INSTANTIATION_PER_OBJECT TRUE
#else /* !BACK_END_IS_C_GEN_BE */
#define ONE_INSTANTIATION_PER_OBJECT FALSE
#endif /* BACK_END_IS_C_GEN_BE */
#else /* DRIVER_COMPATIBILITY_VERSION < 237 */
#define ONE_INSTANTIATION_PER_OBJECT FALSE
#endif /* DRIVER_COMPATIBILITY_VERSION >= 237 */
#endif /* ONE_INSTANTIATION_PER_OBJECT */

#if ONE_INSTANTIATION_PER_OBJECT && !MAINTAIN_NEEDED_FLAGS
 #error -- MAINTAIN_NEEDED_FLAGS must be TRUE \
           when ONE_INSTANTIATION_PER_OBJECT is TRUE
#endif /* !MAINTAIN_NEEDED_FLAGS && ... */
#if ONE_INSTANTIATION_PER_OBJECT && !AUTOMATIC_TEMPLATE_INSTANTIATION
/* One-instantiation-per-object mode needs the .ti file, which it
   uses to give the driver a list of the files to be compiled. */
 #error -- AUTOMATIC_TEMPLATE_INSTANTIATION must be TRUE \
           when ONE_INSTANTIATION_PER_OBJECT is TRUE
#endif /* ONE_INSTANTIATION_PER_OBJECT && !AUTOMATIC_TEMPLATE_INSTANTIATION */

/*
In the "one instantiation per object file" mode, entities with internal
linkage are usually promoted to external linkage when they are referred to
from template instantiations (since they may then need to be referred to from
different slices).  However, for some special entities with internal linkage
(like type_info objects) this is not necessary and the entity can instead be
duplicated across slices.  If this duplication is undesirable (i.e., the
externalization should happen even for these special statics) the following
flag should be set to FALSE.
*/
#ifndef DUPLICATE_SPECIAL_STATICS_IN_INSTANTIATION_SLICES
#define DUPLICATE_SPECIAL_STATICS_IN_INSTANTIATION_SLICES TRUE
#endif /* DUPLICATE_SPECIAL_STATICS_IN_INSTANTIATION_SLICES */

/*
Flag that enables recording of allocation sequence numbers in IL
entries, as an aid to debugging.
*/
#ifndef MAINTAIN_ALLOCATION_SEQUENCE_NUMBER
#define MAINTAIN_ALLOCATION_SEQUENCE_NUMBER FALSE
#endif /* MAINTAIN_ALLOCATION_SEQUENCE_NUMBER */

/*
Flag to enable tracking of object allocations in the IL interpreter, as an
aid to debugging.
*/
#ifndef TRACK_INTERPRETER_ALLOCATIONS
#define TRACK_INTERPRETER_ALLOCATIONS FALSE
#endif /* TRACK_INTERPRETER_ALLOCATIONS */

/*
The flag IL_WALK_NEEDED controls the compilation of the routines required
to walk the IL.  These routines are needed if NEED_IL_DISPLAY is TRUE or
IL_SHOULD_BE_WRITTEN_TO_FILE is TRUE.

The trans_copy.c routines need the IL walk support also, and they are
always included, so IL_WALK_NEEDED is now set to TRUE always.
*/
#ifndef IL_WALK_NEEDED
#define IL_WALK_NEEDED TRUE /* Do not change this. */
#endif /* ifndef IL_WALK_NEEDED */
#if !IL_WALK_NEEDED
 #error -- IL_WALK_NEEDED must be TRUE
#endif /* !IL_WALK_NEEDED */

/*
The flag NEED_DECLARATIVE_WALK controls the compilation of some routines
used to walk (only) declarative entities, for example to generate symbolic
debug information.
*/
#ifndef NEED_DECLARATIVE_WALK
#define NEED_DECLARATIVE_WALK FALSE
#endif /* ifndef NEED_DECLARATIVE_WALK */

/*
If the IL is written to a file, this defines the suffix to be used in
generating the default file name.
*/
#if IL_SHOULD_BE_WRITTEN_TO_FILE
#ifndef IL_FILE_SUFFIX
#define IL_FILE_SUFFIX ".cil"
#endif /* ifndef IL_FILE_SUFFIX */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

/*
Flag that is TRUE if a back end should be called.  The FALSE setting would
be used when the back end is invoked by the driver as a separate program.
*/
#ifndef BACK_END_SHOULD_BE_CALLED
#if !STANDALONE_UTILITY_PROGRAM
#define BACK_END_SHOULD_BE_CALLED TRUE  /* You can change this. */
#else /* STANDALONE_UTILITY_PROGRAM */
/* Compiling a standalone utility program, so the back end is not
   being called (not from the front end, anyway). */
#define BACK_END_SHOULD_BE_CALLED FALSE  /* Do not change this. */
#endif /* !STANDALONE_UTILITY_PROGRAM */
#endif /* ifndef BACK_END_SHOULD_BE_CALLED */
#if BACK_END_SHOULD_BE_CALLED && STANDALONE_UTILITY_PROGRAM
 #error -- Back end should not be called in standalone utility program
#endif /* BACK_END_SHOULD_BE_CALLED && STANDALONE_UTILITY_PROGRAM */

/*
Flag that is TRUE to cause the declaration scope depth to appear in the
IL entry.  When it is set to FALSE the intermediate language representation
is more compact since there is one fewer field in IL entries.
*/
#ifndef RECORD_SCOPE_DEPTH_IN_IL
#define RECORD_SCOPE_DEPTH_IN_IL FALSE
#endif /* ifndef RECORD_SCOPE_DEPTH_IN_IL */

/*
Flag that is TRUE if, by default, the front end should record information
about the form of name references in the IL.  This information is used by
the C++ generating back end so that names can be output more closely to the
form specified in the source program.  Even when not recorded by default,
name references are sometimes recorded in certain template contexts so that
the information is available for name mangling purposes.

Name references used to be conditional on RECORD_FORM_OF_NAME_REFERENCE.
If that macro is defined, use its value to define the replacement
macro DEFAULT_RECORD_FORM_OF_NAME_REFERENCE.
*/
#ifndef DEFAULT_RECORD_FORM_OF_NAME_REFERENCE
#ifdef RECORD_FORM_OF_NAME_REFERENCE
#if RECORD_FORM_OF_NAME_REFERENCE
#define DEFAULT_RECORD_FORM_OF_NAME_REFERENCE TRUE
#else /* !RECORD_FORM_OF_NAME_REFERENCE */
#define DEFAULT_RECORD_FORM_OF_NAME_REFERENCE FALSE
#endif /* RECORD_FORM_OF_NAME_REFERENCE */
#endif /* ifdef RECORD_FORM_OF_NAME_REFERENCE */
#endif /* ifndef DEFAULT_RECORD_FORM_OF_NAME_REFERENCE */


#ifndef DEFAULT_RECORD_FORM_OF_NAME_REFERENCE
#if BACK_END_IS_CP_GEN_BE
#define DEFAULT_RECORD_FORM_OF_NAME_REFERENCE TRUE
#else /* !BACK_END_IS_CP_GEN_BE */
#define DEFAULT_RECORD_FORM_OF_NAME_REFERENCE FALSE
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* DEFAULT_RECORD_FORM_OF_NAME_REFERENCE */

/*
Flag that is TRUE if typerefs should be created to record alternative
template argument lists and name qualifiers (trk_template_arg_list and
trk_name_qualifier, otherwise known as "lexical typerefs").  Ordinarily
they are created when DEFAULT_RECORD_FORM_OF_NAME_REFERENCE is TRUE, but
they can add significantly to the size and complexity of the IL; this
option provides a way of suppressing them if they are not needed.
*/
#ifndef CREATE_LEXICAL_TYPEREFS
#define CREATE_LEXICAL_TYPEREFS DEFAULT_RECORD_FORM_OF_NAME_REFERENCE
#endif /* ifndef CREATE_LEXICAL_TYPEREFS */

#if CREATE_LEXICAL_TYPEREFS && !DEFAULT_RECORD_FORM_OF_NAME_REFERENCE
#error CREATE_LEXICAL_TYPEREFS requires DEFAULT_RECORD_FORM_OF_NAME_REFERENCE
#endif /* CREATE_LEXICAL_TYPEREFS && !DEFAULT_RECORD_FORM_OF_NAME_REFERENCE */

/*
Previously, a flag REPRESENT_EMPTY_STATEMENTS_IN_IL determined how empty
statements are represented.  It has been eliminated and the behavior now
corresponds to having that flag set to TRUE in past versions.  Catch
configurations that set it to FALSE to avoid surprises.
*/
#ifdef REPRESENT_EMPTY_STATEMENTS_IN_IL
#if !REPRESENT_EMPTY_STATEMENTS_IN_IL
 #error -- REPRESENT_EMPTY_STATEMENTS_IN_IL has been eliminated and the new \
           behavior corresponds to having it set to TRUE in past versions
#endif /* !REPRESENT_EMPTY_STATEMENTS_IN_IL */
#endif /* ifdef REPRESENT_EMPTY_STATEMENTS_IN_IL */

/*
Flag that is TRUE if sequencing diagnostics (i.e., to diagnose undefined
behavior in expressions like "x=x++") should be allowed in this configuration.
Note that setting this configuration macro to TRUE doesn't necessarily
enable the diagnostics -- they are enabled only when the
ec_unsequenced_use_of_variable diagnostic is enabled (e.g., when remarks
are enabled or that diagnostic is specifically enabled).
*/
#ifndef SEQUENCING_DIAGNOSTICS_ENABLED
#define SEQUENCING_DIAGNOSTICS_ENABLED FALSE
#endif /* SEQUENCING_DIAGNOSTICS_ENABLED */

#if SEQUENCING_DIAGNOSTICS_ENABLED
#ifndef EXTRA_SOURCE_POSITIONS_IN_IL
#define EXTRA_SOURCE_POSITIONS_IN_IL TRUE
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if !EXTRA_SOURCE_POSITIONS_IN_IL
 #error -- EXTRA_SOURCE_POSITIONS_IN_IL must be TRUE when \
           SEQUENCING_DIAGNOSTICS_ENABLED is TRUE.
#endif /* !EXTRA_SOURCE_POSITIONS_IN_IL */
#endif /* SEQUENCING_DIAGNOSTICS_ENABLED */

/*
DEFAULT_ENABLE_COLORIZED_DIAGNOSTICS specifies whether colorized diagnostics
should be enabled by default and is the initial value of colorized_diagnostics.
The --[no_]colors command-line option can be used to override this value.
Note that even when colorized_diagnostics is TRUE, colorization will be
disabled when:

  - The diagnostic output is not directed to a terminal device.
  - The EDG_COLORS environment variable is set to the empty string.
  - The NOCOLOR environment variable is set.
  - The TERM environment variable is not set (or is set to "dumb").
*/
#ifndef DEFAULT_ENABLE_COLORIZED_DIAGNOSTICS
#define DEFAULT_ENABLE_COLORIZED_DIAGNOSTICS TRUE
#endif /* DEFAULT_ENABLE_COLORIZED_DIAGNOSTICS */

/*
When colorized_diagnostics is TRUE, the escape sequences emitted to achieve
the desired highlighting are specified by a sequence of Select Graphic
Rendition (SGR) escape sequences (for a full description, see
https://en.wikipedia.org/wiki/ANSI_escape_code#SGR_parameters).  These SGR
sequences are set by a user-configurable string that contains the name of the
configurable entity (e.g., "error") followed by an "=" followed by the string
of SGR characters that should be used highlight such an entity.  So,
"error=01;31" indicates that the entity should be bold ("01") and the
foreground text should be red ("31").  These entities are then catenated into
a single string, separated by ":".  The entity names can appear in any order.

If the environment variable EDG_COLORS is set, that string will be used
instead of the default string.  Similarly, if the GCC_COLORS environment
variable is set, its value will be used (although not all values set in
GCC_COLORS are supported by the front end).

The highlighted entities currently supported are: "error", "warning", "note",
"locus", "quote", and "range1".

The default SGR color map is specified by DEFAULT_EDG_COLORS.
*/
#ifndef DEFAULT_EDG_COLORS
#define DEFAULT_EDG_COLORS \
  "error=01;31:warning=01;35:note=01;36:locus=01:quote=01:range1=32"
#endif /* !(defined(DEFAULT_EDG_COLORS) */

/*
Flag that is TRUE to cause additional IL entries to contain source position
information.  Note that this can take a lot of extra space, so you should
enable this only if you really need it.
*/
#ifndef EXTRA_SOURCE_POSITIONS_IN_IL
#define EXTRA_SOURCE_POSITIONS_IN_IL FALSE
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

/*
Earlier versions of the front end could be configured to record just a
sequence number instead of a full position in IL entries representing
statements.  That option is no longer supported.
*/
#ifdef FULL_SOURCE_POS_IN_IL_STATEMENT
#if !FULL_SOURCE_POS_IN_IL_STATEMENT
 #error -- FULL_SOURCE_POS_IN_IL_STATEMENT set to FALSE is no longer supported
#endif /* !FULL_SOURCE_POS_IN_IL_STATEMENT */
#endif /* ifdef FULL_SOURCE_POS_IN_IL_STATEMENT */

/*
Flag that is TRUE if IL lowering should normalize boolean controlling
expressions (e.g., expr in "if (expr)...") to always produce 0/1.
The normalization is often achieved by adding a "!= 0" comparison to the
controlling expression.  This is the initial value of the global variable
lowering_normalizes_boolean_controlling_expressions.
*/
#ifndef LOWERING_NORMALIZES_BOOLEAN_CONTROLLING_EXPRESSIONS
#define LOWERING_NORMALIZES_BOOLEAN_CONTROLLING_EXPRESSIONS FALSE
#endif /* LOWERING_NORMALIZES_BOOLEAN_CONTROLLING_EXPRESSIONS */

#if LOWERING_NORMALIZES_BOOLEAN_CONTROLLING_EXPRESSIONS && !DO_IL_LOWERING
 #error -- LOWERING_NORMALIZES_BOOLEAN_CONTROLLING_EXPRESSIONS requires \
           DO_IL_LOWERING
#endif /* LOWERING_NORMALIZES_BOOLEAN_CONTROLLING_EXPRESSIONS && ... */

/*
If DO_IL_LOWERING is TRUE, this gives the routine name used for the
C++ file-scope initialization routine.  The name is not really
significant (except as a cfront compatibility issue), but the C-generating
back end needs to know what it is in order to recognize it for special
handling.
*/
#if DO_IL_LOWERING
#ifndef IL_LOWERING_INIT_ROUTINE_PREFIX
#define IL_LOWERING_INIT_ROUTINE_PREFIX "__sti__"
#endif /* ifndef IL_LOWERING_INIT_ROUTINE_PREFIX */
#endif /* DO_IL_LOWERING */

/*
DOING_SOURCE_ANALYSIS can be set to TRUE for configurations that are primarily
doing source analysis rather than compiling to object code.  That allows
relaxation of certain checks dependent on the capabilities of the target ABI.
*/
#ifndef DOING_SOURCE_ANALYSIS
#if DO_IL_LOWERING
#define DOING_SOURCE_ANALYSIS FALSE
#else /* !DO_IL_LOWERING */
#define DOING_SOURCE_ANALYSIS TRUE
#endif /* DO_IL_LOWERING */
#endif /* ifndef DOING_SOURCE_ANALYSIS */
#if DOING_SOURCE_ANALYSIS && DO_IL_LOWERING
 #error -- DOING_SOURCE_ANALYSIS cannot be TRUE when DO_IL_LOWERING is TRUE
#endif /* DOING_SOURCE_ANALYSIS && DO_IL_LOWERING */

/*
Flag that is TRUE if a C11 _Generic construct should be completely represented
in the IL.  If FALSE, only the selected expression is represented.
*/
#ifndef REPRESENT_C11_GENERIC_CONSTRUCT_IN_IL
#if DO_IL_LOWERING
#define REPRESENT_C11_GENERIC_CONSTRUCT_IN_IL FALSE
#else /* !DO_IL_LOWERING */
#define REPRESENT_C11_GENERIC_CONSTRUCT_IN_IL TRUE
#endif /* DO_IL_LOWERING */
#endif /* REPRESENT_C11_GENERIC_CONSTRUCT_IN_IL */

/*
Flag that is TRUE if the use of backing expression for constants should be
tempered in large aggregate initializers.  In particular, if the backing
expression only represents an implicit conversion of an arithmetic value,
no backing expression is recorded when this flag is TRUE.  For example, in

  unsigned char bytes[] = { 0x23, 0x34, ... };

where the number of elements is large (currently, at least 1000), no backing
expression will be recorded to represent the conversion from int to unsigned
char.  This considerably reduces IL memory use for some cases with unusually
large aggregate initializers.
*/
#ifndef REDUCE_BACKING_EXPRESSION_USE
#define REDUCE_BACKING_EXPRESSION_USE TRUE
#endif /* REDUCE_BACKING_EXPRESSION_USE */


/*
Flag that is TRUE to indicate that backing expressions for constants
should be recorded even though IL lowering is done.  Generally, they are
not in that case because they're not useful (IL lowering doesn't maintain
them).  This flag forces them on with IL lowering, but note that:

(a)  Extra types may be produced in the IL after removal of unneeded
entities, because those types are referenced from things like sizeof
backing expressions.
(b)  The expressions will not be lowered by IL lowering, so they will
be of limited use; they may contain operators and types that the
back end does not understand.
*/
#ifndef RECORD_BACKING_EXPRS_WITH_IL_LOWERING
#define RECORD_BACKING_EXPRS_WITH_IL_LOWERING FALSE
#endif /* RECORD_BACKING_EXPRS_WITH_IL_LOWERING */
#if RECORD_BACKING_EXPRS_WITH_IL_LOWERING && !DO_IL_LOWERING
 #error -- DO_IL_LOWERING must be TRUE if \
           RECORD_BACKING_EXPRS_WITH_IL_LOWERING is TRUE
#endif /* RECORD_BACKING_EXPRS_WITH_IL_LOWERING && !DO_IL_LOWERING */

/*
Flag that is TRUE to indicate that backing expressions should be kept for
non-type template arguments.  Except for C++-generating back end
configurations, where they are required, such backing expressions are
typically not kept because a given template instance can be referred to
many times using different expressions that fold to the same constant
value, and only one of those can be kept in the template argument for that
instance.
*/
#ifdef KEEP_TEMPLATE_ARG_EXPR_THAT_CAUSES_INSTANTIATION
/* In earlier versions of the front end, this option had a more limited
   scope and was named accordingly.  Preserve the effect of legacy
   configurations that use the previous name. */
#define BACKING_EXPR_FOR_NONTYPE_TEMPL_ARG \
                               KEEP_TEMPLATE_ARG_EXPR_THAT_CAUSES_INSTANTIATION
#endif /* defined(KEEP_TEMPLATE_ARG_EXPR_THAT_CAUSES_INSTANTIATION) */
#ifndef BACKING_EXPR_FOR_NONTYPE_TEMPL_ARG
#if BACK_END_IS_CP_GEN_BE
/* The C++-generating back end requires backing expressions for non-type
   template arguments in order to preserve the correct form and semantics of
   the original source code. */
#define BACKING_EXPR_FOR_NONTYPE_TEMPL_ARG TRUE
#else /* !BACK_END_IS_CP_GEN_BE */
#define BACKING_EXPR_FOR_NONTYPE_TEMPL_ARG FALSE
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* !defined(BACKING_EXPR_FOR_NONTYPE_TEMPL_ARG) */
#if BACK_END_IS_CP_GEN_BE && !BACKING_EXPR_FOR_NONTYPE_TEMPL_ARG
 #error -- C++-generating back end configurations require \
           BACKING_EXPR_FOR_NONTYPE_TEMPL_ARG to be TRUE
#endif /* BACK_END_IS_CP_GEN_BE && !BACKING_EXPR_FOR_NONTYPE_TEMPL_ARG */
#if BACKING_EXPR_FOR_NONTYPE_TEMPL_ARG && DO_IL_LOWERING && \
    !RECORD_BACKING_EXPRS_WITH_IL_LOWERING
 #error -- BACKING_EXPR_FOR_NONTYPE_TEMPL_ARG requires a configuration in \
           which backing expressions are recorded
#endif /* BACKING_EXPR_FOR_NONTYPE_TEMPL_ARG && ... */

/*
Flag that is TRUE to cause source-sequence lists to be generated.  These
lists are attached to scope entries and represent the sequence in which
declarations, statements, comments, macros, and pragmas appear in the
source program.  The C++ generating back-end requires source sequence
lists, so source sequence lists are enabled when the C++ generating
back end is being used.
*/
#ifndef GENERATE_SOURCE_SEQUENCE_LISTS
#if BACK_END_IS_CP_GEN_BE
#define GENERATE_SOURCE_SEQUENCE_LISTS TRUE  /* Do not change this. */
#else /* !BACK_END_IS_CP_GEN_BE */
#define GENERATE_SOURCE_SEQUENCE_LISTS FALSE
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* ifndef GENERATE_SOURCE_SEQUENCE_LISTS */
/* The C++/C-generating back end requires this feature. */
#if DO_IL_LOWERING && GENERATE_SOURCE_SEQUENCE_LISTS
/* This combination is supported, but is generally useless, since IL
   lowering does not update the source sequence information.  In fact,
   if IL lowering is run, generation of source sequence entries is
   suppressed. */
/* Special switch that says "trust me, I really want this". */
#if !defined(ALLOW_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING) || \
    !ALLOW_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING
 #error -- Source sequence lists are useless when doing IL lowering 
#endif /* !defined(ALLOW_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING) ... */
#endif /* DO_IL_LOWERING && GENERATE_SOURCE_SEQUENCE_LISTS */
/* If PRESERVE_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING is defined,
   source sequence entries are generated before and preserved by
   lowering (this mode is considered unusual and is not as well
   debugged as most modes).  Note that
   ALLOW_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING must be TRUE as well. */
#ifndef PRESERVE_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING
#define PRESERVE_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING FALSE
#endif /* ifndef PRESERVE_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING */
#if PRESERVE_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING
#if !GENERATE_SOURCE_SEQUENCE_LISTS
 #error -- PRESERVE_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING cannot be TRUE \
           if GENERATE_SOURCE_SEQUENCE_LISTS is FALSE
#endif /* !GENERATE_SOURCE_SEQUENCE_LISTS */
#if !DO_IL_LOWERING
 #error -- PRESERVE_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING cannot be TRUE \
           if DO_IL_LOWERING is FALSE
#endif /* !DO_IL_LOWERING */
#endif /* PRESERVE_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING */

/*
Flag that is TRUE if source sequence lists are being generated and if they
should include (member and nonmember) function template instantiations and
static data member template instantiations.
*/
#ifndef NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
#if GENERATE_SOURCE_SEQUENCE_LISTS
/* You can change this: */
#define NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS FALSE
#else /* !GENERATE_SOURCE_SEQUENCE_LISTS */
/* Do not change this: */
#define NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS FALSE
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#endif /* ifndef NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */

/*
Flag that is TRUE if source sequence lists are being generated and if they
should include class template instantiations.
*/
#ifndef CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
#if GENERATE_SOURCE_SEQUENCE_LISTS
/* You can change this: */
#define CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS FALSE
#else /* !GENERATE_SOURCE_SEQUENCE_LISTS */
/* Do not change this: */
#define CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS FALSE
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#endif /* ifndef CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */

/*
Union of two previous flags.
*/
#if CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS || \
    NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
#define TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS TRUE
#else /* !CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS &&
         !NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#define TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS FALSE
#endif /* CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS... */
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
#if !GENERATE_SOURCE_SEQUENCE_LISTS
 #error -- Instantiations requested in nonexistent source sequence lists
#else /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if BACK_END_IS_CP_GEN_BE
#if !TEMPORARILY_EXTEND_USE_OF_SSI_CP_GEN_BE
 #error C++-generating back end will soon stop generating explicit \
   specializations for implicit template instances; use \
   TEMPORARILY_EXTEND_USE_OF_SSI_CP_GEN_BE for transition purposes.
#endif /* !TEMPORARILY_EXTEND_USE_OF_SSI_CP_BEN_BE */
#else /* !BACK_END_IS_CP_GEN_BE */
#if !ALLOW_ENABLING_OF_SSI_MODE
 #error Including template instances in source sequence entries is \
   deprecated; use ALLOW_ENABLING_OF_SSI_MODE to continue use.
#endif /* !ALLOW_ENABLING_OF_SSI_MODE */
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* !GENERATE_SOURCE_SEQUENCE_LISTS */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */

/*
Flag that indicates whether a source sequence entry representing a template
instantiation is permitted within the portion of the source sequence list
representing a class definition; it is the default setting for global
variable instantiations_permitted_in_class_src_seq_list.  When the flag is
TRUE, it can be viewed as an annotation identifying where a (partial or full)
instantiation occurred, but it does not necessarily translate into valid C++
(e.g., when the C++-generating back end is used).  When the flag is FALSE,
the source sequence entry representing the instantiation "floats" up to the
namespace scope that contains the class, but it can happen that a template
argument will be dependent on a class member, resulting again in invalid
C++.  For example:
  template <class T> int f(T) { return 0; }
  class A {
    static class N { } n;
    void g(int = f(n));
  };
When the flag is TRUE, the C++-generating back end produces this:
  template <class T> int f(T) { return 0; }
  class A {
    static class N { } n;
    template<> int f(N);    // invalid location of explicit specialization
    void g(int = f(n));
  };
and when it is FALSE, the result is this:
  template <class T> int f(T) { ... }
  class A;
  template<> f(A::N);       // undefined name
  class A {
    static class N { } n;
    void g(int = f(n));
  };
The default value is FALSE, which, when the C++-generating back end is
used, will result in compilable code most of the time.  However, if the
generated code is being run through a compiler that accepts explicit
specializations within a class scope, then setting the value to TRUE would
probably produce compilable code more often, since dependency problems
would be less common.
*/
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
#ifndef DEFAULT_INSTANTIATIONS_PERMITTED_IN_CLASS_SRC_SEQ_LIST
/* You can change this: */
#define DEFAULT_INSTANTIATIONS_PERMITTED_IN_CLASS_SRC_SEQ_LIST FALSE
#endif /* ifndef DEFAULT_INSTANTIATIONS_PERMITTED_IN_CLASS_SRC_SEQ_LIST */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */

/*
If NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS is TRUE, the
following flag determines whether inline function bodies should be removed
from the string form of class template definitions.  This capability
provides a workaround for certain very old versions of some compilers that
issued spurious errors when an explicit specialization was provided for a
member function defined inline in a class template definition.  The flag
has no effect in other configurations.
*/
#ifndef REMOVE_INLINE_BODIES_FROM_CLASS_TEMPLATE_DEFINITIONS
#define REMOVE_INLINE_BODIES_FROM_CLASS_TEMPLATE_DEFINITIONS FALSE
#endif /* REMOVE_INLINE_BODIES_FROM_CLASS_TEMPLATE_DEFINITIONS */

/*
Flag that is TRUE to cause a list of the declarations that appear in the body
of a class to be maintained in the order in which they appeared.  The list is
attached to the class type supplement (via field member_declarations) of each
defined class.
*/
#ifndef MAINTAIN_CLASS_MEMBER_LIST
#define MAINTAIN_CLASS_MEMBER_LIST FALSE
#endif /* ifndef MAINTAIN_CLASS_MEMBER_LIST */

/*
Flag that indicates whether linkage specification blocks like
	extern "C" { ... }
should be represented explicitly in the source sequence entries list.
*/
#ifndef GENERATE_LINKAGE_SPEC_BLOCKS
#if GENERATE_SOURCE_SEQUENCE_LISTS && \
    !TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
#define GENERATE_LINKAGE_SPEC_BLOCKS TRUE
#else /* !(GENERATE_SOURCE_SEQUENCE_LISTS && ...) */
#define GENERATE_LINKAGE_SPEC_BLOCKS FALSE
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#endif /* ifndef GENERATE_LINKAGE_SPEC_BLOCKS */

/*
Ensure that GENERATE_LINKAGE_SPEC_BLOCKS is FALSE if no source sequence entries
are generated or if template instantiations are recorded in source sequence
entry lists.
*/
#if GENERATE_LINKAGE_SPEC_BLOCKS
#if !GENERATE_SOURCE_SEQUENCE_LISTS || \
    TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
 #error -- GENERATE_LINKAGE_SPEC_BLOCKS requires that \
           GENERATE_SOURCE_SEQUENCE_LISTS be TRUE and that \
           TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS be FALSE
#endif /* !GENERATE_SOURCE_SEQUENCE_LISTS || ... */
#endif /* GENERATE_LINKAGE_SPEC_BLOCKS */

/*
The Microsoft __if_exists entry is only needed when Microsoft extensions
are enabled, source sequence entries are being generated, and prototype
instantiations are included in the IL
*/
#ifndef GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
#if GENERATE_SOURCE_SEQUENCE_LISTS && ALL_TEMPLATE_INFO_IN_IL &&	\
    MICROSOFT_EXTENSIONS_ALLOWED
#define GENERATE_MICROSOFT_IF_EXISTS_ENTRIES TRUE
#else /* !(GENERATE_SOURCE_SEQUENCE_LISTS && ...) */
#define GENERATE_MICROSOFT_IF_EXISTS_ENTRIES FALSE
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS && ... */
#endif /* ifndef GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */

#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
#if !GENERATE_SOURCE_SEQUENCE_LISTS || !ALL_TEMPLATE_INFO_IN_IL
 #error -- GENERATE_MICROSOFT_IF_EXISTS_ENTRIES requires \
           GENERATE_SOURCE_SEQUENCE_LISTS and ALL_TEMPLATE_INFO_IN_IL
#endif /* !GENERATE_SOURCE_SEQUENCE_LISTS || !ALL_TEMPLATE_INFO_IN_IL */
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */

/*
Flag that indicates whether friend and member definitions that appear inside
classes may be moved outside those classes.  This flag is only applicable to
configurations where TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS is
TRUE.
*/
#ifndef FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS
#define FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS TRUE
#endif /* FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS */

/*
Flag that indicates that all names should be mangled in C++.
Set by default when IL lowering is done.  Can be set explicitly in
other cases if that's desired.
*/
#ifndef MANGLE_ALL_NAMES
#if DO_IL_LOWERING
#define MANGLE_ALL_NAMES TRUE
#else /* !DO_IL_LOWERING */
#define MANGLE_ALL_NAMES FALSE
#endif /* DO_IL_LOWERING */
#else /* defined(MANGLE_ALL_NAMES) */
#ifndef ONLY_MANGLE_TYPES_NEEDED_FOR_EXTERNAL_NAMES
#define ONLY_MANGLE_TYPES_NEEDED_FOR_EXTERNAL_NAMES FALSE
#endif /* ifndef ONLY_MANGLE_TYPES_NEEDED_FOR_EXTERNAL_NAMES */
#endif /* ifndef MANGLE_ALL_NAMES */
#if !MANGLE_ALL_NAMES && DO_IL_LOWERING
 #error -- Mangling of all names is needed with IL lowering.
#endif /* !MANGLE_ALL_NAMES && DO_IL_LOWERING */

/*
The ONLY_MANGLE_TYPES_NEEDED_FOR_EXTERNAL_NAMES configuration macro controls
whether type names themselves are mangled (and must always be FALSE for the
Cfront ABI).  When ONLY_MANGLE_TYPES_NEEDED_FOR_EXTERNAL_NAMES is TRUE,
unique identifiers are created for each type (though such identifiers
cannot be demangled and may change from one compilation to another).  The
advantage of setting this to TRUE is that it may speed up compilations for
cases that have many types.
*/
#ifndef ONLY_MANGLE_TYPES_NEEDED_FOR_EXTERNAL_NAMES
#define ONLY_MANGLE_TYPES_NEEDED_FOR_EXTERNAL_NAMES FALSE
#endif /* ifndef ONLY_MANGLE_TYPES_NEEDED_FOR_EXTERNAL_NAMES */
#if ONLY_MANGLE_TYPES_NEEDED_FOR_EXTERNAL_NAMES && !IA64_ABI
 #error -- ONLY_MANGLE_TYPES_NEEDED_FOR_EXTERNAL_NAMES must be FALSE for \
           Cfront ABI
#endif /* !ONLY_MANGLE_TYPES_NEEDED_FOR_EXTERNAL_NAMES && !IA64_ABI */

/*
Flag that is TRUE if name mangling is needed.  Automatically TRUE if
IL lowering is used or if automatic template instantiation is selected.
Enabling Microsoft extensions also requires mangling to support the special
__FUNCDNAME__ identifier.
*/
#ifndef NEED_NAME_MANGLING
#if MANGLE_ALL_NAMES || AUTOMATIC_TEMPLATE_INSTANTIATION || \
    MICROSOFT_EXTENSIONS_ALLOWED
#define NEED_NAME_MANGLING TRUE  /* Do not change this. */
#else /* !(MANGLE_ALL_NAMES ...) */
#define NEED_NAME_MANGLING FALSE
#endif /* MANGLE_ALL_NAMES ... */
#endif /* ifndef NEED_NAME_MANGLING */
#if !NEED_NAME_MANGLING && \
    (MANGLE_ALL_NAMES || AUTOMATIC_TEMPLATE_INSTANTIATION || \
     MICROSOFT_EXTENSIONS_ALLOWED)
 #error -- Name mangling code is needed.
#endif /* !NEED_NAME_MANGLING  && ... */

/*
Flag that is TRUE to enable support for processing of orphaned file scope
IL entries.  This is needed if IL lowering or IL walking is to be done.
*/
#if DO_IL_LOWERING || IL_WALK_NEEDED || MAINTAIN_NEEDED_FLAGS || \
    NEED_NAME_MANGLING
#define ORPHAN_PROCESSING_NEEDED TRUE /* Do not change this. */
#else /* !(DO_IL_LOWERING || ...) */
#ifndef ORPHAN_PROCESSING_NEEDED
#define ORPHAN_PROCESSING_NEEDED FALSE
#endif /* ifndef ORPHAN_PROCESSING_NEEDED */
#endif /* DO_IL_LOWERING || ... */

/*
Flag that is TRUE to enable support for maintenance of lists of the local
types and static variables of function and block scopes as file-scope
orphan lists.  This is needed if general orphan processing is needed,
and also if the C-generating back end is being used.
*/
#if ORPHAN_PROCESSING_NEEDED || BACK_END_IS_C_GEN_BE
#define SCOPE_ORPHANED_LIST_PROCESSING_NEEDED TRUE /* Do not change this. */
#else /* !ORPHAN_PROCESSING_NEEDED ... */
#ifndef SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
#define SCOPE_ORPHANED_LIST_PROCESSING_NEEDED FALSE
#endif /* ifndef SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
#endif /* ORPHAN_PROCESSING_NEEDED ... */

/*
Flag that is TRUE if minimal inlining should be done during IL lowering.
This is intended mostly for use with the C-generating back end, and does
not do anything very fancy.
*/
#ifndef MINIMAL_INLINING
#if BACK_END_IS_C_GEN_BE
#define MINIMAL_INLINING TRUE
#else /* !BACK_END_IS_C_GEN_BE */
#define MINIMAL_INLINING FALSE
#endif /* BACK_END_IS_C_GEN_BE */
#endif /* ifndef MINIMAL_INLINING */
#if MINIMAL_INLINING && !DO_IL_LOWERING
 #error -- MINIMAL_INLINING can be set only if DO_IL_LOWERING is set.
#endif /* MINIMAL_INLINING && !DO_IL_LOWERING */

/*
Flag that is TRUE if unrecognized pragmas should be accepted and passed
through to the back end using the characteristics specified by the
pk_unrecognized pragma kind.  The pragma is converted to a character string
that is included in the IL.  When this flag is FALSE, an unrecognized
pragma warning is issued and the pragma is discarded.
*/
#ifndef INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL
#if BACK_END_IS_CP_GEN_BE
#define INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL TRUE
#else /* !BACK_END_IS_CP_GEN_BE */
#define INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL FALSE
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* !defined(INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL) */

/*
Flag that is TRUE if names that are hidden, where the hiding can be defeated
by using global qualification or an elaborated type specifier, should be
recorded in the IL.  Automatically TRUE if BACK_END_IS_CP_GEN_BE is TRUE.
*/
#ifndef RECORD_HIDDEN_NAMES_IN_IL
#if BACK_END_IS_CP_GEN_BE
#define RECORD_HIDDEN_NAMES_IN_IL TRUE /* Do not change this. */
#else /* !BACK_END_IS_CP_GEN_BE */
#define RECORD_HIDDEN_NAMES_IN_IL FALSE
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* !defined(RECORD_HIDDEN_NAMES_IN_IL) */
#if DO_IL_LOWERING && RECORD_HIDDEN_NAMES_IN_IL
/* This combination is supported, but is generally useless, since IL lowering
   does not update the hidden-name information.  In fact, if IL lowering is
   run, generation of hidden-name entries is suppressed. */
/* Special switch that says "trust me, I really want this". */
#ifndef ALLOW_HIDDEN_NAMES_IN_IL_WITH_IL_LOWERING
 #error -- Hidden-name entries in IL are useless when doing IL lowering 
#endif /* ifndef ALLOW_HIDDEN_NAMES_IN_IL_WITH_IL_LOWERING */
#endif /* DO_IL_LOWERING && RECORD_HIDDEN_NAMES_IN_IL */


/*
Flag that is TRUE if a string representation of template declarations
should be recorded in the IL.  Automatically TRUE if BACK_END_IS_CP_GEN_BE
is TRUE.
*/
#ifndef RECORD_TEMPLATE_STRINGS
#if BACK_END_IS_CP_GEN_BE
#define RECORD_TEMPLATE_STRINGS TRUE /* Do not change this. */
#else /* !BACK_END_IS_CP_GEN_BE */
#define RECORD_TEMPLATE_STRINGS FALSE
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* !defined(RECORD_TEMPLATE_STRINGS) */

/*
Flag that is TRUE if macro declarations should be recorded in the IL.
*/
#ifndef RECORD_MACROS_IN_IL
#define RECORD_MACROS_IN_IL FALSE
#endif /* !defined(RECORD_MACROS_IN_IL) */

/*
Flag that is TRUE if unrecognized attributes should be recorded in the IL.
(This is the initial value of the global variable
record_unrecognized_attributes.)
*/
#ifndef RECORD_UNRECOGNIZED_ATTRIBUTES
#if BACK_END_IS_CP_GEN_BE
#define RECORD_UNRECOGNIZED_ATTRIBUTES TRUE
#else /* !BACK_END_IS_CP_GEN_BE */
#define RECORD_UNRECOGNIZED_ATTRIBUTES FALSE
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* ifndef RECORD_UNRECOGNIZED_ATTRIBUTES */

/*
Flag that is TRUE to include a set of EDG provided test pragmas in the
front end.
*/
#ifndef INCLUDE_EDG_TEST_PRAGMAS
#define INCLUDE_EDG_TEST_PRAGMAS FALSE
#endif /* !defined(INCLUDE_EDG_TEST_PRAGMAS) */

/*
Flag that is TRUE to include a set of EDG provided test attributes in the
front end.
*/
#ifndef INCLUDE_EDG_TEST_ATTRIBUTES
#define INCLUDE_EDG_TEST_ATTRIBUTES FALSE
#endif /* !defined(INCLUDE_EDG_TEST_ATTRIBUTES) */

/*
Flag that is TRUE to include a set of EDG-provided named address spaces 
for testing purposes.
*/
#ifndef INCLUDE_EDG_TEST_NAMED_ADDRESS_SPACES
#define INCLUDE_EDG_TEST_NAMED_ADDRESS_SPACES FALSE
#endif /* !defined(INCLUDE_EDG_TEST_NAMED_ADDRESS_SPACES) */

/*
Flag that is TRUE to include a set of EDG-provided named-register storage
classes for testing purposes.
*/
#ifndef INCLUDE_EDG_TEST_NAMED_REGISTERS
#define INCLUDE_EDG_TEST_NAMED_REGISTERS FALSE
#endif /* !defined(INCLUDE_EDG_TEST_NAMED_REGISTERS) */

/*
Flag that is TRUE to enable a command-line option to test the compilation
of multiple (possibly identical) translation units.
*/
#ifndef ENABLE_TRANS_UNIT_TEST_MODE
#define ENABLE_TRANS_UNIT_TEST_MODE FALSE
#endif /* !defined(ENABLE_TRANS_UNIT_TEST_MODE) */

/*
Flag that is TRUE to specify that source files should be read in
binary mode under MS-DOS (or Windows 95, or Windows NT).  In this mode,
carriage return and control-Z are handled by the front end instead of
the host C runtime library.

The default is TRUE for Microsoft operating systems because it allows
them to deal properly with Unix files accessed over a network, and it
sidesteps C runtime library bugs in this area.  In particular,
Windows NT seems to have some bug with ftell/fseek on files that
only have a line-feed terminator on lines (no carriage return) -- they
mostly work okay, but when, with deeply nested inclusion, a file
is closed and reopened, the ftell/fseek doesn't restore the right
position when it's at the beginning of a set of zero-length lines
(several line-feed characters in a row).
*/
#ifndef READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS
#if __MICROSOFT_OS__
#define READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS TRUE
#else /* !__MICROSOFT_OS__ */
#define READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS FALSE
#endif /* __MICROSOFT_OS__ */
#endif /* ifndef READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS */
#if READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS
#define CONTROL_Z (0x1a)
#define IGNORE_CARRIAGE_RETURN_IN_SOURCE TRUE
#endif /* READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS */

/*
Test a character to see if it is an end-of-file character.
*/
#define is_eof_char(ch) ((ch) == EOF)

/*
Flag that is TRUE to indicate that carriage return characters at the ends
of input lines should be ignored as part of the line terminator, and carriage
returns outside of comments and character/string literals should be treated
as white space, with an optional diagnostic.
*/
#ifndef IGNORE_CARRIAGE_RETURN_IN_SOURCE
#define IGNORE_CARRIAGE_RETURN_IN_SOURCE TRUE
#endif /* ifndef IGNORE_CARRIAGE_RETURN_IN_SOURCE */

/*
Flag that is TRUE to indicate that carriage return or carriage return
followed by newline can be used as a line terminator in GNU-compatible
modes.  This feature is provided to allow files with old MacOS line
terminators to be accepted.  The implementation is compatible with the way
in which the GNU compiler handles such line terminators.  It is disabled by
default because it is not required by most users.
*/
#ifndef ACCEPT_GNU_CARRIAGE_RETURN_LINE_TERMINATOR
#define ACCEPT_GNU_CARRIAGE_RETURN_LINE_TERMINATOR FALSE
#endif /* ifndef ACCEPT_GNU_CARRIAGE_RETURN_LINE_TERMINATOR */

/*
Default temporary file directory.  Use /tmp on non-Microsoft systems as it's
more likely to be faster (i.e., a tmpfs file system).
*/
#if __MICROSOFT_OS__
#ifndef DEFAULT_TMPDIR
#define DEFAULT_TMPDIR "\\tmp\\"
#endif /* ifndef DEFAULT_TMPDIR */
#else /* !__MICROSOFT_OS__ */
#ifndef DEFAULT_TMPDIR
#define DEFAULT_TMPDIR "/tmp"
#endif /* ifndef DEFAULT_TMPDIR */
#endif /* !__MICROSOFT_OS__ */

/*
Default system include directory.
*/
#ifndef DEFAULT_USR_INCLUDE
#define DEFAULT_USR_INCLUDE "/usr/include"
#endif /* ifndef DEFAULT_USR_INCLUDE */

/*
Flag that is TRUE to suppress the inclusion of DEFAULT_USR_INCLUDE (or the
value of the environment variable USR_INCLUDE) in the include file
search path.  This may be desirable for cross versions.
*/
#ifndef NO_USR_INCLUDE
#define NO_USR_INCLUDE FALSE
#endif /* ifndef NO_USR_INCLUDE */

/*
Object file suffix.  This is added to the base name of the primary input file
to get the object file name.  That name is used only for generating object
file dependencies for a makefile.
*/
#ifndef OBJECT_FILE_SUFFIX
#if __MICROSOFT_OS__
#define OBJECT_FILE_SUFFIX ".obj"
#else /* !__MICROSOFT_OS__ */
#define OBJECT_FILE_SUFFIX ".o"
#endif /* __MICROSOFT_OS__ */
#endif /* ifndef OBJECT_FILE_SUFFIX */

#if AUTOMATIC_TEMPLATE_INSTANTIATION
/*
Template instantiation request file suffix.  This is added to the base
name of the primary input file to get the request file name.
*/
#ifndef INSTANTIATION_FILE_SUFFIX
#define INSTANTIATION_FILE_SUFFIX ".ii"
#endif /* ifndef INSTANTIATION_FILE_SUFFIX */

/*
Template information file suffix.  This is added to the base name of
the primary input file to get the template information file name.
*/
#ifndef TEMPLATE_INFO_FILE_SUFFIX
#define TEMPLATE_INFO_FILE_SUFFIX ".ti"
#endif /* ifndef TEMPLATE_INFO_FILE_SUFFIX */

/*
Exported template file suffix.  This is added to the base name of
the primary input file to get the exported template file name.
The exported template file contains information about the exported
templates that are defined by a given file.
*/
#ifndef EXPORTED_TEMPLATE_FILE_SUFFIX
#define EXPORTED_TEMPLATE_FILE_SUFFIX ".et"
#endif /* ifndef EXPORTED_TEMPLATE_FILE_SUFFIX */

/*
The name used for the file found in an export template directory that
provides information about how files in that directory should be
recompiled, such as the include search paths to be used.
*/
#ifndef EXPORT_INFO_FILE_NAME
#define EXPORT_INFO_FILE_NAME "export_info"
#endif /* ifndef EXPORT_INFO_FILE_NAME */

/*
Flag that is TRUE if a template information file should be created for
information such as instantiation files (in one instantiation per object
file mode), or to contain template instantiation flags. 
*/
#ifndef USE_TEMPLATE_INFO_FILE
#if DRIVER_COMPATIBILITY_VERSION >= 237
#define USE_TEMPLATE_INFO_FILE TRUE
#else /* DRIVER_COMPATIBILITY_VERSION >= 237 */
#define USE_TEMPLATE_INFO_FILE FALSE
#endif /* !DRIVER_COMPATIBILITY_VERSION >= 237 */
#endif /* ifndef USE_TEMPLATE_INFO_FILE */

#if ONE_INSTANTIATION_PER_OBJECT && !USE_TEMPLATE_INFO_FILE
 #error -- USE_TEMPLATE_INFO_FILE must be TRUE when \
           ONE_INSTANTIATION_PER_OBJECT is TRUE.
#endif /* ONE_INSTANTIATION_PER_OBJECT && !USE_TEMPLATE_INFO_FILE */

/*
Flag that is TRUE if the instantiation flags should be written to the
template information file instead of being put in the object file.
This defaults to TRUE when a template information file is being used,
or to FALSE otherwise.
*/
#ifndef INSTANTIATION_FLAGS_IN_TEMPLATE_INFO_FILE
#if USE_TEMPLATE_INFO_FILE
#define INSTANTIATION_FLAGS_IN_TEMPLATE_INFO_FILE TRUE
#else /* USE_TEMPLATE_INFO_FILE */
#define INSTANTIATION_FLAGS_IN_TEMPLATE_INFO_FILE FALSE
#endif /* USE_TEMPLATE_INFO_FILE */
#endif /* ifndef INSTANTIATION_FLAGS_IN_TEMPLATE_INFO_FILE */

#if INSTANTIATION_FLAGS_IN_TEMPLATE_INFO_FILE && !USE_TEMPLATE_INFO_FILE
 #error -- USE_TEMPLATE_INFO_FILE must be TRUE when \
           INSTANTIATION_FLAGS_IN_TEMPLATE_INFO_FILE is TRUE.
#endif /* INSTANTIATION_FLAGS_IN_TEMPLATE_INFO_FILE ... */

/*
When using the IA-64 ABI, template information files must be used for
automatic instantiation purposes because the alternate entry point
facility is only supported with template information files.
*/
#if !USE_TEMPLATE_INFO_FILE && IA64_ABI
 #error -- USE_TEMPLATE_INFO_FILE must be TRUE when \
           using the IA-64 ABI with AUTOMATIC_TEMPLATE_INSTANTIATION.
#endif /* !USE_TEMPLATE_INFO_FILE && IA64_ABI */

/*
The number of lines of the instantiation request file that are reserved
and do not contain instantiation list entries.

If a template information file is being used, the command line, etc.
are placed in that file and are no longer in the instantiation request
file, so no lines are reserved.

Otherwise, if no value is defined for INSTANTIATION_REQUEST_LINES_RESERVED
we check for a definition of the former name of this flag, which is
INSTANTIATION_INFO_LINES_RESERVED, and use that value if it is defined.

Otherwise, we reserve 3 lines.
*/
#ifndef INSTANTIATION_REQUEST_LINES_RESERVED

#if USE_TEMPLATE_INFO_FILE
#define INSTANTIATION_REQUEST_LINES_RESERVED 0
#else /* !USE_TEMPLATE_INFO_FILE */

#ifdef INSTANTIATION_INFO_LINES_RESERVED
#define INSTANTIATION_REQUEST_LINES_RESERVED INSTANTIATION_INFO_LINES_RESERVED
#else /* ifndef INSTANTIATION_INFO_LINES_RESERVED */
#define INSTANTIATION_REQUEST_LINES_RESERVED 3
#endif /* ifdef INSTANTIATION_INFO_LINES_RESERVED */

#endif /* !USE_TEMPLATE_INFO_FILE */
#endif /* ifndef INSTANTIATION_REQUEST_LINES_RESERVED */

#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */

#if INSTANTIATION_BY_IMPLICIT_INCLUSION
/*
The suffixes to be used when searching for an instantiation source file
that is associated with a given instantiation header file.
*/
#ifndef DEFAULT_INSTANTIATION_FILE_SUFFIX_LIST
#if __MICROSOFT_OS__
/* Case is not significant in MS-DOS file names. */
#define DEFAULT_INSTANTIATION_FILE_SUFFIX_LIST "C:CPP:CXX:CC"
#else /* !__MICROSOFT_OS__ */
#define DEFAULT_INSTANTIATION_FILE_SUFFIX_LIST "c:C:cpp:CPP:cxx:CXX:cc"
#endif /* __MICROSOFT_OS__ */
#endif /* ifndef DEFAULT_INSTANTIATION_FILE_SUFFIX_LIST */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */

/*
Flag that is TRUE if the host has the __int128 and __uint128 extensions;
otherwise, FALSE.
*/
#ifndef HOST_HAS_INT128_EXTENSIONS
#if defined(__SIZEOF_INT128__)
#define HOST_HAS_INT128_EXTENSIONS TRUE
#else /* !defined(__SIZEOF_INT128__) */
#define HOST_HAS_INT128_EXTENSIONS FALSE
#endif /* defined(__SIZEOF_INT128__) */
#endif /* ifndef HOST_HAS_INT128_EXTENSIONS */

/*
Flag that is TRUE if the host system provides functions for creating temporary
files that should be used instead of the EDG temporary file creation algorithm.
*/
#ifndef USE_HOST_TMPFILE_FACILITIES
#define USE_HOST_TMPFILE_FACILITIES TRUE
#endif /* ifndef USE_HOST_TMPFILE_FACILITIES */

/*
The suffixes to be used when searching for an include file name specified
with no suffix.  This is a colon-separated list of suffixes in the order
in which they are to be searched.  A file will always have a suffix
appended, so if a null suffix is to be permitted, it must be included
in the suffix list ("::" in the list indicates a null suffix).

The default setting of "::stdh:" permits include files to have no suffix,
or to have the special suffix "stdh".

If an implementation uses only header files with no suffixes (i.e., no
suffix is to be added) this string should be set to "" or "::" to avoid the
overhead of looking for files with the ".stdh" suffix.
*/
#ifndef DEFAULT_INCLUDE_FILE_SUFFIX_LIST
#define DEFAULT_INCLUDE_FILE_SUFFIX_LIST "::stdh:"
#endif /* DEFAULT_INCLUDE_FILE_SUFFIX_LIST */

/*
Flag that is TRUE to generate trailing include file push/pop and (in GNU
modes) system-header codes (like those of the GNU preprocessor) on the ends
of the line-identifying directives generated in preprocessing output.  See
gen_pp_line_info in lexical.c.
*/
#ifndef GEN_EXTRA_LINE_ID_INFO
#define GEN_EXTRA_LINE_ID_INFO FALSE
#endif /* ifndef GEN_EXTRA_LINE_ID_INFO */

/*
Precompiled header file suffix.
*/
#ifndef PCH_FILE_SUFFIX
#define PCH_FILE_SUFFIX ".pch"
#endif /* ifndef PCH_FILE_SUFFIX */

/*
USE_MMAP_FOR_MEMORY_REGIONS is TRUE if memory mapping is available for use
in allocating memory regions.  By default, it is assumed to be available
on systems other than MS-DOS.  When compiling a standalone utility
program, use of mmap is disabled by default.
*/
#ifndef USE_MMAP_FOR_MEMORY_REGIONS
#if EDG_MSDOS || STANDALONE_UTILITY_PROGRAM
#define USE_MMAP_FOR_MEMORY_REGIONS FALSE
#else /* !(EDG_MSDOS || STANDALONE_UTILITY_PROGRAM) */
#define USE_MMAP_FOR_MEMORY_REGIONS TRUE
#endif /* EDG_MSDOS || STANDALONE_UTILITY_PROGRAM */
#endif /* ifndef USE_MMAP_FOR_MEMORY_REGIONS */

/*
When using memory mapped memory for memory regions, this flag is TRUE
if a fixed address should be supplied to mmap as the address to
which the memory should be mapped.  If this is FALSE, the system
assigns the address.
*/
#ifndef USE_FIXED_ADDRESS_FOR_MMAP
#define USE_FIXED_ADDRESS_FOR_MMAP FALSE
#endif /* ifndef USE_FIXED_ADDRESS_FOR_MMAP */

#if USE_FIXED_ADDRESS_FOR_MMAP && !USE_MMAP_FOR_MEMORY_REGIONS
  #error USE_FIXED_ADDRESS_FOR_MMAP requires USE_MMAP_FOR_MEMORY_REGIONS
#endif /* USE_FIXED_ADDRESS_FOR_MMAP && !USE_MMAP_FOR_MEMORY_REGIONS */

/*
When using mapped memory at a fixed address, the fixed address must be
specified.  FIXED_ADDRESS_FOR_MMAP is used to provide the default address
(which may be overridden on the command line with the --mmap_address option).
*/
#ifndef FIXED_ADDRESS_FOR_MMAP
#if USE_FIXED_ADDRESS_FOR_MMAP
  #error -- FIXED_ADDRESS_FOR_MMAP must be defined when \
            USE_FIXED_ADDRESS_FOR_MMAP is set.
#endif /* USE_FIXED_ADDRESS_FOR_MMAP */
#endif /* ifndef FIXED_ADDRESS_FOR_MMAP */

#if USE_FIXED_ADDRESS_FOR_MMAP
EXTERN_THREAD a_const_char
		*fixed_address_for_mmap;
			/* The fixed address to use for mmap calls;
			   initially set to FIXED_ADDRESS_FOR_MMAP and may be
			   modified by command line option. */
#endif /* USE_FIXED_ADDRESS_FOR_MMAP */

/*
When set to FALSE, accesses to the bytes of the module file are conservative in
that they access the file one byte at a time and optionally swap bytes as
needed.  When TRUE, it is assumed that the module file's architecture and
layout match those of the target and that data can accessed directly from a
memory-mapped version of the module file.
The safe value is FALSE.

Note that this does not guarantee that pointers to module entities are in a
memory mapped region - other considerations (such as whether the target and
host are big- or little-endian) are taken into account, and copies will occur
where necessary.  If this flag is TRUE, USE_MMAP_FOR_MEMORY_REGIONS must also
be TRUE.
*/
#ifndef USE_MMAP_FOR_MODULES
#if USE_MMAP_FOR_MEMORY_REGIONS
#define USE_MMAP_FOR_MODULES TRUE
#else /* !USE_MMAP_FOR_MEMORY_REGIONS */
#define USE_MMAP_FOR_MODULES FALSE
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
#endif /* ifndef USE_MMAP_FOR_MODULES */

#if USE_MMAP_FOR_MODULES && !USE_MMAP_FOR_MEMORY_REGIONS
  #error USE_MMAP_FOR_MODULES requires USE_MMAP_FOR_MEMORY_REGIONS
#endif /* USE_MMAP_FOR_MODULES && !USE_MMAP_FOR_MEMORY_REGIONS */

/*
When TRUE, assume that the contents of all Microsoft IFC files are little-
endian, regardless of the target endianness.  Otherwise, the contents of IFC
files are assumed to match the target endianness.  Setting this to TRUE may be
useful when transporting IFC files that were created on a little-endian machine
to one that's big-endian.
*/
#ifndef ASSUME_LITTLE_ENDIAN_IFC_MODULES
#define ASSUME_LITTLE_ENDIAN_IFC_MODULES FALSE
#endif /* ifndef ASSUME_LITTLE_ENDIAN_IFC_MODULES */

/*
The maximum number of lines a module is allowed to claim it contains in a
single source file.

As the front end has a finite number of sequence numbers that can be
represented, module files that claim an extreme number of lines can result in
the front end running out of sequence numbers.  This macro limits the number of
sequence numbers a module can claim by limiting the maximum representable line
number each module source file is allowed to claim (note that a binary module
interface file may reference multiple module source files).
*/
#ifndef MODULE_MAX_LINE_NUMBER
#define MODULE_MAX_LINE_NUMBER 250000u
#endif /* MODULE_MAX_LINE_NUMBER */

/*
When using precompiled headers, it must be possible to duplicate the memory
allocation done by the process that created the precompiled header.  This
may be accomplished either by allocating the IL memory blocks in separate
memory allocated by memory mapping, or by using a special block of
memory that will be used for all memory region allocations.  This
memory is allocated early in the compilation so as not to be
affected by things that may vary from one compilation to the next.
This parameter specifies the default size of the block of memory
to be allocated when precompiled headers are being used.  This value
may be overridden by a command line option.
*/
#if !USE_MMAP_FOR_MEMORY_REGIONS
#ifndef DEFAULT_PREALLOCATED_PCH_MEM_SIZE
#if EDG_MSDOS
#define DEFAULT_PREALLOCATED_PCH_MEM_SIZE ((long)0x100000)
#else /* !EDG_MSDOS */
#define DEFAULT_PREALLOCATED_PCH_MEM_SIZE (1024 * 1024 * 4)
#endif /* EDG_MSDOS */
#endif /* ifndef DEFAULT_PREALLOCATED_PCH_MEM_SIZE */
#endif /* !USE_MMAP_FOR_MEMORY_REGIONS */

/*
Argument strings for fopen.  These must come after the default definitions for
the used test macros.
*/
#if READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS
/* Read source files in binary mode for some MS-DOS cases.  Carriage return
   and control-Z are handled explicitly. */
#define FOPEN_MODE_FOR_READ "rb"
#define FOPEN_MODE_FOR_WRITE "w"
#define FOPEN_MODE_FOR_UPDATE "w+"
#define FOPEN_MODE_FOR_BINARY_WRITE "wb"
#define FOPEN_MODE_FOR_BINARY_UPDATE "w+b"
#define FOPEN_MODE_FOR_BINARY_READ "rb"
#else /* !READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS */
#if __ANSIC__ || __MICROSOFT_OS__
/* ANSI C allows binary modes.  So does MS-DOS. */
#define FOPEN_MODE_FOR_READ "r"
#define FOPEN_MODE_FOR_WRITE "w"
#define FOPEN_MODE_FOR_UPDATE "w+"
#define FOPEN_MODE_FOR_BINARY_WRITE "wb"
#define FOPEN_MODE_FOR_BINARY_UPDATE "w+b"
#define FOPEN_MODE_FOR_BINARY_READ "rb"
#else /* !(__ANSIC__ || __MICROSOFT_OS__) */
/* Assume UNIX (binary and text files the same). */
#define FOPEN_MODE_FOR_READ "r"
#define FOPEN_MODE_FOR_WRITE "w"
#define FOPEN_MODE_FOR_UPDATE "w+"
#define FOPEN_MODE_FOR_BINARY_WRITE "w"
#define FOPEN_MODE_FOR_BINARY_UPDATE "w+"
#define FOPEN_MODE_FOR_BINARY_READ "r"
#endif /* __ANSIC__  || __MICROSOFT_OS__ */
#endif /* READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS */

/*
The minimum number of declarations required in header files that qualify
for precompilation.  In other words, if the header files preceding the
header stop have fewer than PCH_DECL_SEQ_THRESHOLD declarations, creation
of a precompiled header file will be suppressed.  By default it is set to
a very low value, to give the user maximum control (using command line
options and #pragmas).
*/
#ifndef PCH_DECL_SEQ_THRESHOLD
#define PCH_DECL_SEQ_THRESHOLD 1
#endif /* ifndef PCH_DECL_SEQ_THRESHOLD */

/*
The directory in which the various components needed to use the compiler
are installed.  Typically this directory will have subdirectories such
as bin, lib, include, etc.  Currently, the front end uses this only
for the purpose of finding the predefined macro definition file.
*/
#ifndef DEFAULT_EDG_BASE
#define DEFAULT_EDG_BASE ""
#endif /* DEFAULT_EDG_BASE */

/*
The name of the directory in EDG_BASE that contains miscellaneous files
needed by the front end at execution time.  Currently, the front end uses
this only for the purpose of finding the predefined macro definition file.
When the --target command-line option is used or a default target is
specified, the name of the target configuration (and a separating underscore)
are appended to this name (this allows different predefined macros to be used
for different target configurations).
*/
#ifndef EDG_AUXILIARY_INFO_DIR_NAME
#define EDG_AUXILIARY_INFO_DIR_NAME "lib"
#endif /* EDG_AUXILIARY_INFO_DIR_NAME */

/*
The name of the predefined macro definition file to be used.
*/
#ifndef PREDEFINED_MACRO_FILE_NAME
#define PREDEFINED_MACRO_FILE_NAME "predefined_macros.txt"
#endif /* PREDEFINED_MACRO_FILE_NAME */

/*
Flag that is TRUE if the file specified by PREDEFINED_MACRO_FILE_NAME should
be used to predefine macros at the start of compilation.
*/
#ifndef DEFAULT_USE_PREDEFINED_MACRO_FILE
#define DEFAULT_USE_PREDEFINED_MACRO_FILE FALSE
#endif /* DEFAULT_USE_PREDEFINED_MACRO_FILE */

/*
Some systems don't trap NULL pointer references.  This flag may be set
TRUE on SVR4 systems (or systems with an SVR4-compatible mprotect call)
to enable trapping of NULL pointer references.  This flag must only
be set on systems that don't already trap NULL references.  The code
that sets this mode will abort on systems that already trap NULL
references.
*/
#ifndef SVR4_TRAP_NULL_POINTER_REFERENCES
#define SVR4_TRAP_NULL_POINTER_REFERENCES FALSE
#endif /* ifndef SVR4_TRAP_NULL_POINTER_REFERENCES */

/*
TRUE if the Kuck & Associates inliner is being used.  This is not
part of the EDG provided source product, but is a separate product
available from Kuck & Associates that can be linked with the EDG
front end.
*/
#ifndef USING_KAI_INLINER
#define USING_KAI_INLINER FALSE
#endif /* ifndef USING_KAI_INLINER */

#if USING_KAI_INLINER
#if IL_SHOULD_BE_WRITTEN_TO_FILE
 #error -- IL_SHOULD_BE_WRITTEN_TO_FILE must be FALSE when \
           USING_KAI_INLINER is set.
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
#endif /* USING_KAI_INLINER */

/*
If the front end is to be called as a function, the EDG_MAIN macro provides
a name for the function.

The Kuck & Associates inliner/optimizer provides its own main program and
calls the EDG main program using the name edg_main.
*/
#if MAKE_FRONT_END_CALLABLE || USING_KAI_INLINER
#ifndef EDG_MAIN
#define EDG_MAIN edg_main
#endif /* ifndef EDG_MAIN */

/*
When the front end is called as a function, provide a declaration for the
main routine.
*/
extern int EDG_MAIN(int argc, char *argv[]);

#endif /* MAKE_FRONT_END_CALLABLE || USING_KAI_INLINER */

/*
If a name of the main program was not selected above, use the default "main".
*/
#ifndef EDG_MAIN
#define EDG_MAIN main
#endif /* ifndef EDG_MAIN */

/*
The flags HOSTID and HOSTID2 can be set to host id numbers if the
front end is only allowed to be run on a few CPUs.  They should be left
undefined otherwise.  An example of proper setting is

#define HOSTID  0x12008fd2
#define HOSTID2 0x12008d32

If only one CPU id is needed, HOSTID should be set, and HOSTID2 should be
left undefined.
*/

/*
The flag DEMO_VERSION_ID can be defined with a string identifying a demo
version if this is a demo version.  The string is printed on startup with
the -v option.
*/

/*
Flag that provides the default value for null_chars_allowed_in_source,
which controls whether null (zero) characters are allowed in source lines.
*/
#ifndef DEFAULT_NULL_CHARS_ALLOWED_IN_SOURCE
#define DEFAULT_NULL_CHARS_ALLOWED_IN_SOURCE FALSE
#endif /* DEFAULT_NULL_CHARS_ALLOWED_IN_SOURCE */

/*
Flag that is TRUE if an interface should be included to allow a routine to
be called for the potential lazy loading of class definitions. See
get_definition_of_class for more information.
*/
#ifndef GET_DEFINITION_OF_CLASS_NEEDED
#if CPPCLI_ENABLING_POSSIBLE
#define GET_DEFINITION_OF_CLASS_NEEDED TRUE
#else /* !CPPCLI_ENABLING_POSSIBLE */
#define GET_DEFINITION_OF_CLASS_NEEDED FALSE
#endif /* CPPCLI_ENABLING_POSSIBLE */
#endif /* ifndef GET_DEFINITION_OF_CLASS_NEEDED */

#if CPPCLI_ENABLING_POSSIBLE && !GET_DEFINITION_OF_CLASS_NEEDED 
 #error -- CPPCLI_ENABLING_POSSIBLE requires GET_DEFINITION_OF_CLASS_NEEDED 
#endif /* CPPCLI_ENABLING_POSSIBLE && !GET_DEFINITION_OF_CLASS_NEEDED */

/*
The front end is not intended to be built in UNICODE mode on Windows.
Doing so results in warnings and can result in incorrect behavior if
those warnings are not addressed.
*/
#if EDG_WIN32
#ifdef UNICODE
 #error -- Building with UNICODE defined is not supported.
#endif /* ifdef UNICODE */
#endif /* EDG_WIN32 */


/*
Flag that is TRUE if the code point for a question mark is a smaller value
than that of end of line markers ('\n' and '\r').  Assuming the opposite
(which valid for all common character encodings) enables a valuable
optimization in the performance of read_logical_source_line.
*/
#ifndef DO_NOT_ASSUME_QUESTION_IS_LARGER_THAN_END_OF_LINE
#define DO_NOT_ASSUME_QUESTION_IS_LARGER_THAN_END_OF_LINE FALSE
#endif /* DO_NOT_ASSUME_QUESTION_IS_LARGER_THAN_END_OF_LINE */

/*
Flag that is TRUE if UTF-8 and UTF-16 encodings of Unicode should be
accepted in source code.  Note that if you set this the representation for
identifiers and file names becomes UTF-8, which may require back end or
host-interface changes (see fopen_interface if the standard fopen does not
take UTF-8 strings).  If NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE (see
below) is FALSE, setting UNICODE_SOURCE_SUPPORTED to TRUE implies that
non-Unicode files are encoded as Latin-1 and, except in GNU emulation
modes, causes universal-character-names in narrow strings to be translated
to Latin-1.
*/
#ifndef UNICODE_SOURCE_SUPPORTED
#define UNICODE_SOURCE_SUPPORTED TRUE
#endif /* UNICODE_SOURCE_SUPPORTED */

/*
Flag that is TRUE if facilities for detecting Unicode source vulnerabilities
(as described in www.trojansource.codes/trojan-source.pdf) should be
included.  It is enabled by default if Unicode support is enabled and
cannot be enabled if Unicode is not supported.
*/
#ifndef UNICODE_VULNERABILITY_DETECTION_SUPPORTED
#define UNICODE_VULNERABILITY_DETECTION_SUPPORTED UNICODE_SOURCE_SUPPORTED
#endif /* UNICODE_VULNERABILITY_DETECTION_SUPPORTED */
#if UNICODE_VULNERABILITY_DETECTION_SUPPORTED && !UNICODE_SOURCE_SUPPORTED
 #error -- UNICODE_VULNERABILITY_DETECTION_SUPPORTED requires \
           UNICODE_SOURCE_SUPPORTED
#endif /* UNICODE_VULNERABILITY_DETECTION_SUPPORTED && ... */

/*
Flag that is TRUE if multibyte characters are supported in source code,
specifically in comments, string literals, character constants, and
identifiers.  This applies to both C and C++ mode.  See also
IDENTIFIER_STRINGS_ALLOW_MULTIBYTE_CHARS.
*/
#ifndef MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
#if UNICODE_SOURCE_SUPPORTED
#define MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED TRUE
#else /* !UNICODE_SOURCE_SUPPORTED */
#define MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED FALSE
#endif /* UNICODE_SOURCE_SUPPORTED */
#endif /* ifndef MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */

#if UNICODE_SOURCE_SUPPORTED && !MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
 #error -- UNICODE_SOURCE_SUPPORTED requires \
           MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
#endif /* UNICODE_SOURCE_SUPPORTED && !MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */

/*
Flag that is TRUE if multibyte character support in source code should
be enabled by default.  This is the initial value of
multibyte_chars_in_source_enabled, which is also controlled by
--[no_]multibyte_chars.  Meaningful only if
MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED is TRUE.
*/
#ifndef DEFAULT_MULTIBYTE_CHARS_IN_SOURCE_ENABLED
#define DEFAULT_MULTIBYTE_CHARS_IN_SOURCE_ENABLED FALSE
#endif /* ifndef DEFAULT_MULTIBYTE_CHARS_IN_SOURCE_ENABLED */

/*
Flag that is TRUE if, when Unicode source is supported, files with other
multibyte characters are also supported.  In this mode, Unicode files are
processed as usual, but non-Unicode files are scanned using a particular
locale, as follows: Characters in identifiers, file names, and wide string
literals are translated into their Unicode equivalents.  Characters in
narrow literals are left in their native representation or, in the case of
a universal-character-name, translated from the Unicode code point to the
corresponding character in that locale.  (When this flag is FALSE but
UNICODE_SOURCE_SUPPORTED is TRUE, non-Unicode files are assumed to be
encoded as Latin-1, and universal-character-names in narrow strings are
translated to Latin-1.)

This facility requires the ability to translate a multibyte character
sequence from the encoding of a given locale to Unicode and vice versa.
The front end does not provide such a facility.  On Windows (i.e., when
EDG_WIN32 is TRUE) the Windows facilities are used for this translation.
Because the front end cannot be used as delivered in a non-Windows
environment, a #error directive is issued below in such cases.  The Windows
routines that are used are available starting with version 1400 (Visual
Studio 8) of the Microsoft compiler.
*/
#ifndef NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
#if EDG_WIN32 && UNICODE_SOURCE_SUPPORTED
#define NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE TRUE
#else /* !(EDG_WIN32 && UNICODE_SOURCE_SUPPORTED) */
#define NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE FALSE
#endif /* EDG_WIN32 && UNICODE_SOURCE_SUPPORTED */
#endif /* ifndef NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */

/*
Flag that is TRUE to allow NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
versions to be built on non-Windows platforms for debugging purposes.
Note that such versions will not actually do the multibyte to Unicode
conversion.  As a result, versions built with this flag will not function
properly and will often result in internal errors on source containing
multibyte characters (e.g., because the front end expects that such
characters in identifiers will have been converted to UTF-8 and the
multibyte character sequences may be invalid UTF-8 values).
*/
#ifndef EDG_NATIVE_MULTIBYTE_TEST_MODE
#define EDG_NATIVE_MULTIBYTE_TEST_MODE FALSE
#endif /* ifndef EDG_NATIVE_MULTIBYTE_TEST_MODE */

#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
#if !MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
 #error -- NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE is only supported \
           when MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED is TRUE.
#endif /* !MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */

#if !UNICODE_SOURCE_SUPPORTED
 #error -- NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE is only supported \
           when UNICODE_SOURCE_SUPPORTED is TRUE.
#endif /* !UNICODE_SOURCE_SUPPORTED */

#if !EDG_WIN32 && !EDG_NATIVE_MULTIBYTE_TEST_MODE
 #error -- NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE is only supported \
           when EDG_WIN32 is TRUE.
#endif /* !(EDG_WIN32 && !EDG_NATIVE_MULTIBYTE_TEST_MODE) */

#if EDG_WIN32
#if !defined(_MSC_VER) || _MSC_VER < 1400
 #error -- NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE is only supported \
           when building with Microsoft Visual Studio and _MSC_VER >= 1400.
#endif /* !defined(_MSC_VER) || _MSC_VER < 1400 */
#endif /* EDG_WIN32 */

#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */

/*
Indication of the kind of Unicode encoding being used for a source file.
*/
enum a_unicode_source_kind {
  usk_none,		/* Source is not Unicode.  When
			   NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
			   is TRUE, the source file may still contain
			   multibyte characters in some other encoding. */
  usk_utf8,		/* Source is UTF-8 encoded. */
  usk_utf16LE,		/* Source is UTF-16 encoded, little-endian. */
  usk_utf16BE		/* Source is UTF-16 encoded, big-endian. */
  /* NOTE: getc_source requires that the UTF-16 codes be at the end. */
};


#if UNICODE_SOURCE_SUPPORTED
/*
The kind of Unicode encoding to be assumed for a source file that has
no initial byte order mark.  usk_none means assume such a file is not
Unicode.
*/
#ifndef DEFAULT_UNICODE_SOURCE_KIND
#define DEFAULT_UNICODE_SOURCE_KIND usk_utf8
#endif /* DEFAULT_UNICODE_SOURCE_KIND */

EXTERN_THREAD a_unicode_source_kind
		default_unicode_source_kind;
			/* The kind of Unicode encoding to be assumed for a
			   source file that has no initial byte order mark.
			   usk_none means assume such a file is not Unicode. */
#endif /* UNICODE_SOURCE_SUPPORTED */

/*
Flag that provides the default value for check_for_byte_order_mark, which
is TRUE if, when opening a source file, the front end should check for the
presence of a byte order mark, used to indicate UTF-8 and UTF-16 encoded
source files.
*/
#ifndef DEFAULT_CHECK_FOR_BYTE_ORDER_MARK
#if UNICODE_SOURCE_SUPPORTED
#define DEFAULT_CHECK_FOR_BYTE_ORDER_MARK TRUE
#else /* !UNICODE_SOURCE_SUPPORTED */
#define DEFAULT_CHECK_FOR_BYTE_ORDER_MARK FALSE
#endif /* UNICODE_SOURCE_SUPPORTED */
#endif /* ifndef DEFAULT_CHECK_FOR_BYTE_ORDER_MARK */

#if DEFAULT_CHECK_FOR_BYTE_ORDER_MARK && !UNICODE_SOURCE_SUPPORTED
 #error -- DEFAULT_CHECK_FOR_BYTE_ORDER_MARK requires \
           UNICODE_SOURCE_SUPPORTED
#endif /* DEFAULT_CHECK_FOR_BYTE_ORDER_MARK && !UNICODE_SOURCE_SUPPORTED */

#if UNICODE_SOURCE_SUPPORTED
EXTERN_THREAD a_boolean
		check_for_byte_order_mark;
			/* TRUE if, when reading the first line of a source
			   file, the front end should check for the presence
			   of a byte order mark. */
#if EDG_WIN32
extern wchar_t *translate_filename_to_wchar(a_const_char *filename);
extern wchar_t *conv_utf8_to_wchar(a_const_char *buffer);

EXTERN_THREAD a_text_buffer_ptr
		wchar_translation_buffer;
			/* Text buffer used by translate_filename_to_wchar
			   and conv_utf8_to_wchar. */
#endif /* EDG_WIN32 */
#endif /* UNICODE_SOURCE_SUPPORTED */

/*
The maximum number of characters in a native multibyte character sequence
or, if multibyte characters are not supported, in a character specified as
a universal-character-name.  Use the value from the host environment
(MB_LEN_MAX) if available.
*/
#ifndef MAX_MULTIBYTE_CHAR_LENGTH
#ifdef MB_LEN_MAX
#define MAX_MULTIBYTE_CHAR_LENGTH MB_LEN_MAX
#else /* ifndef MB_LEN_MAX */
/* Pick a value that should be large enough for all encodings. */
#define MAX_MULTIBYTE_CHAR_LENGTH 16
#endif /* ifdef MB_LEN_MAX */
#endif /* ifndef MAX_MULTIBYTE_CHAR_LENGTH */

/*
Routines/macros to deal with multibyte character sequences in source code.
*/
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
/*
Flag that is TRUE to use custom code to deal with the Japanese SJIS
(shift-JIS) character encoding, instead of relying on C library routines
for that.
*/
#ifndef USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING
#define USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING FALSE
#endif /* ifndef USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING */

/*
Flag that is TRUE to enable a test version of multibyte character
handling.  This provides a simpler means of testing multibyte character
support.  A "$" is treated as the first character of a multibyte sequence.
*/
#ifndef EDG_MULTIBYTE_CHAR_TEST_MODE
#define EDG_MULTIBYTE_CHAR_TEST_MODE FALSE
#endif /* ifndef EDG_MULTIBYTE_CHAR_TEST_MODE */

#if (EDG_MULTIBYTE_CHAR_TEST_MODE && USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING)||\
    (EDG_MULTIBYTE_CHAR_TEST_MODE && UNICODE_SOURCE_SUPPORTED) || \
    (USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING && UNICODE_SOURCE_SUPPORTED)
 #error -- At most one of USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING, \
        EDG_MULTIBYTE_CHAR_TEST_MODE, and UNICODE_SOURCE_SUPPORTED can be TRUE.
#endif /* defined(EDG_MULTIBYTE_CHAR_TEST_MODE) + ... */

/*
Flag that is TRUE if the name strings for identifiers within the front
end are allowed to contain multibyte characters.  Note that this does
not control whether multibyte characters are allowed within
identifiers (they are allowed whenever multibyte characters are enabled),
but only how such characters are represented in the name strings (and
therefore how a back end will see them, and how they will appear in
diagnostic messages).  When UNICODE_SOURCE_SUPPORTED is TRUE, the
multibyte character encoding used is UTF-8.  When
IDENTIFIER_STRINGS_ALLOW_MULTIBYTE_CHARS is FALSE, multibyte
characters are represented in identifier name strings by a sequence of
the form \mXXXX or \MXXXXXXXX, where XXXX is the hexadecimal value of
the character (When UNICODE_SOURCE_SUPPORTED is TRUE, the UCN forms
\uXXXX or \UXXXXXXXX are used instead, since in that case the
encodings mean the same thing).  See also REWRITE_UCN_ESCAPE_CHAR_IN_LOWERING.
*/
#ifndef IDENTIFIER_STRINGS_ALLOW_MULTIBYTE_CHARS
#define IDENTIFIER_STRINGS_ALLOW_MULTIBYTE_CHARS FALSE
#endif /* IDENTIFIER_STRINGS_ALLOW_MULTIBYTE_CHARS */

/*
Indication of whether backslash and question mark can appear as part of
a multibyte character sequence.  If they cannot, processing for line splices
and trigraphs can be made more efficient.  For the Japanese EUC encoding,
for example, those character codes never appear as part of other sequences,
and these switches should be set to FALSE.  The safe answer, in all cases,
is TRUE: it may be slower than necessary, but it always gets the right answer.
Those characters cannot appear as part of a Unicode sequence, so the
fast setting can be used unless native multibyte characters are also
supported with Unicode.
*/
#if UNICODE_SOURCE_SUPPORTED && !NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
/* These characters do not appear in UTF-8 encoding. */
#ifndef BACKSLASH_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR
#define BACKSLASH_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR FALSE
#endif /* ifndef BACKSLASH_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR */
#ifndef QUESTION_MARK_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR
#define QUESTION_MARK_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR FALSE
#endif /* ifndef QUESTION_MARK_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR */
#endif /* UNICODE_SOURCE_SUPPORTED &&
          !NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE*/
#ifndef BACKSLASH_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR
#define BACKSLASH_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR TRUE
#endif /* ifndef BACKSLASH_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR */
#ifndef QUESTION_MARK_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR
#define QUESTION_MARK_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR TRUE
#endif /* ifndef QUESTION_MARK_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR */

/*
Macro that returns TRUE if a given character might be the beginning
of a multibyte character sequence.  This is used only for a speed
optimization (possibly significant if the C library mblen is
inefficient), and only when multibyte characters are enabled;
the macro need not be defined at all.
*/
#ifndef char_may_begin_multibyte_sequence
#if UNICODE_SOURCE_SUPPORTED
/* Version for UTF-8. */
#define char_may_begin_multibyte_sequence(ch) ((unsigned char)(ch) > 0x7f)
#else /* !UNICODE_SOURCE_SUPPORTED */
#if EDG_MULTIBYTE_CHAR_TEST_MODE
/* In EDG multibyte test mode a "$" begins a multibyte character. */
#define char_may_begin_multibyte_sequence(ch) (ch == '$')
#else /* EDG_MULTIBYTE_CHAR_TEST_MODE */
#if 'a' == 97
/* If the character set appears to contain ASCII, a safe version of
   this is to test for the printable part of the ASCII code set. */
#define char_may_begin_multibyte_sequence(ch) \
  ((unsigned char)(ch) < 0x20 || ((unsigned char)(ch) > 0x7e))
#endif /* 'a' == 97 */
#endif /* EDG_MULTIBYTE_CHAR_TEST_MODE */
#endif /* UNICODE_SOURCE_SUPPORTED */
#endif /* ifndef char_may_begin_multibyte_sequence */

/* Return the length of the multibyte character sequence beginning at ptr.
   If the sequence there is invalid, set *err to TRUE, and return 1.
   err must be non-NULL; call mbc_length_simple if you do not need the
   err parameter. */
#ifdef char_may_begin_multibyte_sequence
#define mbc_length(ptr, err) \
  (char_may_begin_multibyte_sequence(*(ptr)) ? \
     f_mbc_length((ptr), (err), /*is_native=*/FALSE) : \
     ((*(err) = FALSE), 1))
#define mbc_length_full(ptr, err, is_native) \
  (char_may_begin_multibyte_sequence(*(ptr)) ? \
     f_mbc_length((ptr), (err), (is_native)) : \
     ((*(err) = FALSE), 1))
#define mbc_length_simple(ptr) \
  (char_may_begin_multibyte_sequence(*(ptr)) ? \
     f_mbc_length((ptr), (a_boolean *)NULL, /*is_native=*/FALSE) : \
     1)
#else /* !defined(char_may_begin_multibyte_sequence) */
/* The char_may_begin_multibyte_sequence macro is not defined, so just
   call f_mbc_length. */
#define mbc_length(ptr, err) f_mbc_length((ptr), (err), /*is_native=*/FALSE)
#define mbc_length_full(ptr, err, is_native) \
  f_mbc_length((ptr), (err), (is_native))
#define mbc_length_simple(ptr) \
  f_mbc_length((ptr), (a_boolean *)NULL, /*is_native=*/FALSE)
#endif /* ifdef char_may_begin_multibyte_sequence */
extern void set_multibyte_locale(void);
extern int f_mbc_length(a_const_char *ptr,
                        a_boolean    *err,
                        a_boolean    is_native);
/* Convert multibyte character sequence to wide character. */
extern int mbc_to_wide_char(a_const_char  *mb,
                            unsigned long *wc,
                            a_boolean     *err,
                            a_boolean     is_native);

#if USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING
/* Use custom processing for SJIS instead of the C library routines. */

/* A character in the range [0x81,0x9f] or [0xe0,0xfc] is the first of a
   two-character SJIS sequence.  Other characters are single characters. */
/* Note that there is variation among implementations of SJIS, and the
   limits of those ranges can be slightly different in some cases.
   The macro here should be replaced as necessary. */
#ifndef is_first_char_of_sjis_two_char_sequence
#define is_first_char_of_sjis_two_char_sequence(ch) \
  ((0x81 <= (ch) && (ch) <= 0x9f) || (0xe0 <= (ch) && (ch) <= 0xfc))
#endif /* ifndef is_first_char_of_sjis_two_char_sequence */

/* The second character of a two-character SJIS sequence is required
   to be in the range [0x40,0xfc].  Again, there is some variation between
   implementations, and the macro here should be replaced as necessary. */
#ifndef is_valid_sjis_second_char
#define is_valid_sjis_second_char(ch) \
  (0x40 <= (ch) && (ch) <= 0xfc)
#endif /* ifndef is_valid_sjis_second_char */

/* Initialize for using mbc_length within one string of source characters. */
#define mbc_scan_init() ((void)0)
#define mbc_scan_init_if_multibyte_chars_in_source_enabled() /* Nothing. */

#else /* !USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING */
#if EDG_MULTIBYTE_CHAR_TEST_MODE
/* Initialize for using mbc_length within one string of source characters. */
#define mbc_scan_init() ((void)0)
#define mbc_scan_init_if_multibyte_chars_in_source_enabled() /* Nothing. */

#else /* !EDG_MULTIBYTE_CHAR_TEST_MODE */
#if UNICODE_SOURCE_SUPPORTED
/* Treat UTF-8 as a multibyte character sequence.  (UTF-16 is handled on
   input and converted immediately to UTF-8.) */

#ifndef LOCALE_TO_SET_WHEN_MULTIBYTE_CHARS_ENABLED
/* If possible, pick a locale that looks like ISO-8859-1/Latin-1. */
#if EDG_WIN32
/* There is a Windows Latin-1 code page, but Windows-1252 is effectively the
   same and is the default code page in lots of Western nations. */
#define LOCALE_TO_SET_WHEN_MULTIBYTE_CHARS_ENABLED ".1252"
#else /* !EDG_WIN32 */
#ifdef __sun
#define LOCALE_TO_SET_WHEN_MULTIBYTE_CHARS_ENABLED "iso_8859_1"
#endif /* __sun */
#endif /* EDG_WIN32 */
#endif /* LOCALE_TO_SET_WHEN_MULTIBYTE_CHARS_ENABLED */

#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE

END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
#include <wctype.h>
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */

extern
int unicode_to_multibyte_char(unsigned long uc,
                              unsigned char chars[MAX_MULTIBYTE_CHAR_LENGTH],
                              a_boolean     *err);

#if EDG_WIN32

EXTERN_THREAD _locale_t
		native_multibyte_locale;
			/* The locale object to be used for converting
			   multibyte characters to UTF-8. */

#define mbc_scan_init() \
  { if (curr_file_unicode_source_kind == usk_none) { \
      ((void)_mblen_l(NULL, MB_CUR_MAX, native_multibyte_locale)); \
    }  /* if */ \
  }  /* if */
#define mbc_scan_init_if_multibyte_chars_in_source_enabled() \
  { if (multibyte_chars_in_source_enabled) mbc_scan_init(); }

#else /* EDG_WIN32 */
#if EDG_NATIVE_MULTIBYTE_TEST_MODE

/* Use the C library routines. */
#define mbc_scan_init() \
  { if (curr_file_unicode_source_kind == usk_none) { \
      ((void)mblen(NULL, MB_CUR_MAX)); \
    }  /* if */ \
  }  /* if */
#define mbc_scan_init_if_multibyte_chars_in_source_enabled() \
  { if (multibyte_chars_in_source_enabled) mbc_scan_init(); }

#else /* !EDG_NATIVE_MULTIBYTE_TEST_MODE */
 #error mbc_scan_init requires customization on non-Windows platforms when \
        using NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE.
#endif /* EDG_NATIVE_MULTIBYTE_TEST_MODE */
#endif /* !EDG_WIN32 */

#else /* !NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */

/* Initialize for using mbc_length within one string of source characters. */
#define mbc_scan_init() ((void)0)
#define mbc_scan_init_if_multibyte_chars_in_source_enabled() /* Nothing. */

#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */

#else /* !UNICODE_SOURCE_SUPPORTED */

/* Use the standard C library routines. */

/*
Locale to set when multibyte characters are enabled in source code.
*/
#ifndef LOCALE_TO_SET_WHEN_MULTIBYTE_CHARS_ENABLED
#define LOCALE_TO_SET_WHEN_MULTIBYTE_CHARS_ENABLED ""
#endif /* LOCALE_TO_SET_WHEN_MULTIBYTE_CHARS_ENABLED */

#ifndef STDLIB_H_INCLUDED
#define STDLIB_H_INCLUDED 1
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
#include <stdlib.h>
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */
#endif /* STDLIB_H_INCLUDED */
#ifndef MB_CUR_MAX
/* We need setlocale, MB_CUR_MAX, and mblen to support multibyte
   characters. */
 #error -- multibyte character support requires C library multibyte support
#endif /* ifndef MB_CUR_MAX */
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
#include <locale.h>
#include <wctype.h>
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */

/* Initialize for using mbc_length within one string of source characters. */
#define mbc_scan_init() ((void)mblen(NULL, MB_CUR_MAX))
#define mbc_scan_init_if_multibyte_chars_in_source_enabled() \
  { if (multibyte_chars_in_source_enabled) mbc_scan_init(); }

#endif /* UNICODE_SOURCE_SUPPORTED */
#endif /* EDG_MULTIBYTE_CHAR_TEST_MODE */
#endif /* USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING */

#else /* !MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */

/* Allow mbc_length_simple to be used when multibyte character support
   is not enabled. */
#define mbc_length_simple(ptr) (1)

/* Allow mbc_length_full to be used when multibyte character support
   is not enabled. */
#define mbc_length_full(ptr, err, is_native) (1)

#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */

/*
Flag that is TRUE if the LOCALE_TO_SET_WHEN_MULTIBYTE_CHARS_ENABLED
should always be set, not just when multibyte characters are enabled.
This flag has no effect if LOCALE_TO_SET_WHEN_MULTIBYTE_CHARS_ENABLED
is not defined.
*/
#ifndef ALWAYS_SET_MULTIBYTE_LOCALE
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED && \
    DEFAULT_MULTIBYTE_CHARS_IN_SOURCE_ENABLED
#define ALWAYS_SET_MULTIBYTE_LOCALE TRUE
#else /* !(MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED &&
          DEFAULT_MULTIBYTE_CHARS_IN_SOURCE_ENABLED) */
#define ALWAYS_SET_MULTIBYTE_LOCALE FALSE
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED &&
          DEFAULT_MULTIBYTE_CHARS_IN_SOURCE_ENABLED */
#endif /* ifndef ALWAYS_SET_MULTIBYTE_LOCALE */

#if ALWAYS_SET_MULTIBYTE_LOCALE && !MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
 #error -- ALWAYS_SET_MULTIBYTE_LOCALE cannot be TRUE when \
           MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED is FALSE.
#endif /* ALWAYS_SET_MULTIBYTE_LOCALE &&
          !MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */


/*
Default value of the _MSVC_EXECUTION_CHARACTER_SET predefined macro (which
is defined in Microsoft emulation mode and refers to the execution character
set defined at compile time.  Note that at the current time, this value is used
only for the purposes of the _MSVC_EXECUTION_CHARACTER_SET predefined macro
(and doesn't have any effect on the front end).  Use 1252 as the default value
(ANSI Latin 1; Western European code page).
*/
#ifndef DEFAULT_MSVC_EXECUTION_CHARACTER_SET
#define DEFAULT_MSVC_EXECUTION_CHARACTER_SET "1252"
#endif /* DEFAULT_MSVC_EXECUTION_CHARACTER_SET */

extern unsigned unicode_to_utf8(unsigned long uc,
                                unsigned char chars[4]);
#if UNICODE_SOURCE_SUPPORTED
/* Data structure used by getc_source and getc_utf16 to hold characters
   queued up as source characters (because a UTF-16 sequence maps into
   multiple UTF-8 characters). */
typedef struct {
  char		chars[5];
			/* Characters queued up.  chars[count-1] is the next
			   one to be returned, then chars[count-2], etc. */
  int		count;
			/* Count of characters queued up. */
  a_unicode_source_kind
		unicode_source_kind;
			/* The kind of Unicode source characters in the
			   input file. */
} a_getc_source_state;

extern void clear_getc_source_state(a_getc_source_state   *state,
                                    a_unicode_source_kind ukind);
extern int getc_utf16(FILE                *file,
                      a_getc_source_state *state);

/* getc-like macro that fetches the next character from a source file
   (and deals with converting UTF-16). */
#define getc_source(file, state) \
  (((int)(state).unicode_source_kind < (int)usk_utf16LE) ? \
    getc((file)) : \
    getc_utf16((file), &(state)))

/* Special versions of mbc_length and mbc_to_wide_char for use in the
   lexical routines.  These consult the current setting of
   curr_file_unicode_source_kind to see if the current input is UTF-8 or
   non-Unicode.  Non-Unicode characters are assumed to be Latin-1 unless
   NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE is TRUE; in that case,
   they might be some other multibyte character set. */
#define lex_mbc_length(ptr, err) \
  (char_may_begin_multibyte_sequence(*(ptr)) ?				\
     f_mbc_length((ptr), (err), curr_file_unicode_source_kind == usk_none) : \
     ((*(err) = FALSE), 1))
#define lex_mbc_length_simple(ptr) \
  (char_may_begin_multibyte_sequence(*(ptr))				    \
    ? f_mbc_length((ptr), (a_boolean *)NULL,				    \
                   curr_file_unicode_source_kind == usk_none)		    \
    : 1)
#define lex_mbc_to_wide_char(mb, wc, err) \
  (mbc_to_wide_char((mb), (wc), (err),					\
                    curr_file_unicode_source_kind == usk_none))

#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
extern a_boolean set_windows_locale(a_const_char *locale_name);

extern char *multibyte_chars_to_utf8(a_const_char *id_ptr,
				     sizeof_t     *id_length,
				     a_boolean    *err);
#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */

#else /*!UNICODE_SOURCE_SUPPORTED */
#define getc_source(file, state) (getc(file))

#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
/* The multibyte character support is for something other than
   UTF-8.  The "lex" routines just go to the normal routines. */
#define lex_mbc_length(ptr, err) mbc_length((ptr), (err))
#define lex_mbc_length_simple(ptr) mbc_length_simple((ptr))
#define lex_mbc_to_wide_char(mb, wc, err) \
  mbc_to_wide_char((mb), (wc), (err), /*is_native=*/FALSE)
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
#endif /* UNICODE_SOURCE_SUPPORTED */

#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
extern a_const_char *mbc_strchr(a_const_char *str,
                                int          chr);
#else /* !MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
/*
When not using multibyte characters, just map this name onto the
normal C library routine.
*/
#define mbc_strchr(str, chr) strchr((str), (chr))
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */

/*
Macro used to increment a pointer to an element of a character string,
handling multibyte characters if needed.
*/
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
#define increment_mbc_ptr(ptr) (ptr += mbc_length_simple(ptr))
#else /* !MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
#define increment_mbc_ptr(ptr) (++ptr)
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */

/*
Primary source file name, as given on the command line.  FILE_NAME_FOR_STDIN
if the primary source file is stdin.  The string is allocated in general
storage, not IL storage.
*/
EXTERN_THREAD a_const_char
		*primary_source_file_name;

EXTERN_THREAD char
		*dir_name_of_primary_source_file;
			/* The directory name of the primary source file.
                           This is set by the command line processing routines
                           when the primary source file is set. */
#if COMPILE_MULTIPLE_SOURCE_FILES
EXTERN_THREAD a_boolean
		more_than_one_source_file /* = FALSE */;
			/* TRUE if more than one primary source file appears
			   on the command line. */
#endif /* COMPILE_MULTIPLE_SOURCE_FILES */

EXTERN_THREAD a_boolean
		more_than_one_non_export_translation_unit /* = FALSE */;
			/* TRUE if more than one translation unit appears
			   on the command line. */

/*
If non-NULL, a list of entries that describe files to include at the
beginning of the compilation.  There is one list of file to be preincluded
only for the purpose of setting macros, and another that can define macros
as well as include other code.  The macro preincludes are processed before
the other preincludes.  When multiple source files are compiled,
this is included at the beginning of each compilation.  The list is
allocated in general storage, not IL storage.
*/
EXTERN_THREAD struct a_preinclude_file
		*preinclude_file_list,
		*macro_preinclude_file_list;

/*
Pointer to the end of each of the preinclude lists.
*/
EXTERN_THREAD struct a_preinclude_file
		*preinclude_file_tail,
		*macro_preinclude_file_tail;

#if MICROSOFT_EXTENSIONS_ALLOWED
/*
If non-NULL, a list of entries that describe metadata files to be imported
at the beginning of the compilation.
*/
EXTERN_THREAD struct a_preinclude_file
                *preusing_file_list,
                *preusing_file_tail;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if MICROSOFT_EXTENSIONS_ALLOWED
#ifndef METADATA_IMPORT_BUFFER_SIZE
#define METADATA_IMPORT_BUFFER_SIZE 0x500000
			/* The initial size of the buffer used to import
			   metadata.  This should be large enough to
			   handle mscorlib. */
#endif /* METADATA_IMPORT_BUFFER_SIZE */

#ifndef METADATA_IMPORT_BUFFER_ALLOCATION_INCREMENT
#define METADATA_IMPORT_BUFFER_ALLOCATION_INCREMENT 0x1000
			/* The amount by which the import buffer should be
			   increased in size if it is too small. */
#endif /* METADATA_IMPORT_BUFFER_ALLOCATION_INCREMENT */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */


/*
Object file name, usually derived from the primary source file name.
Really used only in generating makefile dependency information.
The string is allocated in general storage, not IL storage.
*/
EXTERN_THREAD char
		*object_file_name;

/* Control the current working directory. */
extern void reset_working_directory();
extern void set_working_directory(a_const_char *dir_name);
extern a_const_char *get_working_directory();

/*
Data structure that defines a list of directory names (as for a search
path for include file opens).
*/
typedef struct a_directory_name_entry *a_directory_name_entry_ptr;
typedef struct a_directory_name_entry {
  a_const_char	*dir_name;
			/* The directory name. */
  a_boolean	system_include_dir;
			/* TRUE if the directory is considered a "system"
			   include directory.  Warnings are suppressed
			   when processing files from system include
			   directories. */
  a_directory_name_entry_ptr
		next;
			/* The next entry on the search path list, or NULL
			   if this is the last entry. */
} a_directory_name_entry;

/*
Search path for include files.
*/
EXTERN_THREAD a_directory_name_entry_ptr
		incl_search_path,
		end_incl_search_path;
			/* Beginning and end pointers for the list.
			   The name strings are in general storage. */

/*
Search path for #embed files.
*/
EXTERN_THREAD a_directory_name_entry_ptr
		embed_search_path,
		end_embed_search_path;
			/* Beginning and end pointers for the list.
			   The name strings are in general storage. */

/*
Search path for module files.  Each path is a directory to be searched for
files.
*/
EXTERN_THREAD a_directory_name_entry_ptr
                module_search_path,
                end_module_search_path;
                        /* Beginning and end pointers for the list.
                           The name strings are in general storage. */

#if MICROSOFT_EXTENSIONS_ALLOWED
/*
Search path for #using files.
*/
EXTERN_THREAD a_directory_name_entry_ptr
                assembly_search_path,
                end_assembly_search_path;
                        /* Beginning and end pointers for the list.
                           The name strings are in general storage. */

#if EDG_WIN32
/*
The version of the CLR runtime to be used if no version is specified by the
application configuration file.
*/
#ifndef CLR_FALLBACK_VERSION
#define CLR_FALLBACK_VERSION L"v4.0.0"
#endif /* ifndef CLR_FALLBACK_VERSION */
#endif /* EDG_WIN32 */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Search path for <...> include files (the tail of incl_search_path).
*/
EXTERN_THREAD a_directory_name_entry_ptr
		sys_incl_search_path;
			/* The name strings are in general storage. */

EXTERN_THREAD a_boolean
		put_dir_of_each_opened_source_file_on_incl_search_path;
			/* If TRUE, the directory of each source file opened
			   is put on the include search path.  Set FALSE
			   by the "-I-" option. */

EXTERN_THREAD a_directory_name_entry_ptr
		template_search_path;
			/* Search path to find exported templates.
			   The name strings are in general storage. */

EXTERN_THREAD a_directory_name_entry_ptr
		dir_name_list_il;
			/* List of all directory name strings used that have
			   been allocated in IL memory.  Used so that the
			   strings can be shared. */

/*
Flags used to indicate the behavior of the file open routines.  These flags
are used by routines such as fopen_with_error to determine whether
an error should be issued, or a NULL file pointer returned.
*/
typedef unsigned an_open_file_flag_set;

#define OFF_NO_OPTIONS		0x0u
#define OFF_OKAY_IF_NOT_FOUND	0x1u
			/* TRUE if a NULL file pointer should be returned if
			   the file does not exist.  FALSE if an error should
			   be issued. */
#define OFF_OKAY_IF_CANNOT_OPEN	0x2u
			/* TRUE if a NULL file pointer should be returned if
			   the file exists but cannot be opened.  FALSE if an
			   error should be issued. */
#define OFF_OKAY_IF_NOT_REGULAR	0x4u
			/* TRUE if a NULL file pointer should be returned if
			   the file exists but is not a regular file.  FALSE
			   if an error should be issued. */
#define OFF_OKAY_IF_DIRECTORY	0x8u
			/* TRUE if a NULL file pointer should be returned if
			   the file names a directory.  FALSE if an error
			   should be issued. */
#define OFF_COMMAND_LINE	0x10u
			/* TRUE if an open failure should be reported as a
			   command-line error. */
#if DEBUG
#define OFF_FORCE_ERROR		0x80000000u
			/* TRUE if the open operation should be considered to
			   have failed.  For debugging purposes. */
#endif /* DEBUG */

/*
Flags used to indicate the failure reason when a NULL pointer is returned
by a file open routine.
*/
typedef int an_open_file_result_set;
typedef struct an_open_file_result {
  an_open_file_result_set
		flags;
			/* Bit flags that indicate any errors that may have
			   occurred while opening the file. */
  int		errno_value;
			/* The errno value if the file could not be opened. */
} an_open_file_result;

#define OFR_NOT_FOUND		0x1
			/* The specified name was not found. */
#define OFR_CANNOT_OPEN		0x2
			/* The name exists but cannot be opened. */
#define OFR_NOT_REGULAR		0x4
			/* The name specifies a file that is not a regular
			   file. */
#define OFR_IS_DIRECTORY	0x8
			/* The name specifies a directory. */
#define OFR_BAD_NAME		0x10
			/* The name specifies an invalid file name. */


#if MAKE_FRONT_END_CALLABLE

END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
#include <setjmp.h> 
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */

EXTERN_THREAD int
		exit_status;
			/* The status value to be returned to the caller. */

EXTERN_THREAD jmp_buf
		edg_main_setjmp_buffer;
			/* The setjmp buffer used to transfer control back
			   to the main routine in the event of an error. */
#endif /* MAKE_FRONT_END_CALLABLE */

typedef struct a_source_file *a_source_file_ptr;

void clear_open_file_result(an_open_file_result	*open_result);

/* Add the default system include file search path. */
extern void add_default_include_search_path(
				a_directory_name_entry_ptr *search_path,
				a_directory_name_entry_ptr *end_search_path);
/* Add a directory to a specified search path. */
extern void add_to_specified_include_search_path(
			a_const_char			*dir_name,
			a_boolean			system_include_dir,
			a_directory_name_entry_ptr	*search_path,
			a_directory_name_entry_ptr	*end_search_path);
/* Add a directory to the end of the include file search path. */
extern void add_to_include_search_path(a_const_char	*dir_name,
                                       a_boolean	sys_include_dir);
/* Add a directory to the front of the include file search path. */
extern void add_to_front_of_include_search_path(
				a_const_char		   *dir_name,
				a_directory_name_entry_ptr *search_path,
				a_directory_name_entry_ptr *end_search_path);

/* Change the directory name in the primary include file search path entry. */
extern void change_primary_include_search_dir(a_const_char *dir_name);
/* Manage include search path when source input file is pushed or popped. */
extern void push_primary_include_search_dir(a_const_char *dir_name,
                                            a_boolean    system_include_dir);
extern void pop_primary_include_search_dir(a_const_char	*dir_name,
                                           a_boolean	system_include_dir);

extern void add_to_template_search_path(a_const_char	*dir_name);
#if !STANDALONE_UTILITY_PROGRAM
extern void remove_duplicate_include_dirs(
			a_directory_name_entry_ptr	*include_path_boundary,
			a_boolean			sys_includes_only);
#endif /* !STANDALONE_UTILITY_PROGRAM */

EXTERN_THREAD a_boolean
		stack_referenced_include_directories;
			/* If TRUE a stack model is used to manage the include
			   search list and FALSE if some other model (by
			   default, a replace-restore model) is to be used
			   instead.  Typically, this flag is TRUE when
			   microsoft_mode is TRUE. */

/* Extract the directory name from a file name. */
extern char *f_directory_of(a_const_char *file_name,
                            a_boolean	 in_general_memory);

#if !STANDALONE_UTILITY_PROGRAM
#define directory_of(file_name) f_directory_of(file_name,	\
                                              /*in_general_memory=*/FALSE)
#endif /* !STANDALONE_UTILITY_PROGRAM */
#define gs_directory_of(file_name) f_directory_of(file_name,	\
                                                  /*in_general_memory=*/TRUE)

/* Extract the base name from a file name. */
extern char *derived_name(a_const_char *file_name,
                          a_const_char *suffix);

/*
The character used to separate components of a path name.  On Microsoft
operating systems this should be '/'.  The additional '\' character is
handled separately.
*/
#ifndef DIRECTORY_SEPARATOR
#define DIRECTORY_SEPARATOR '/'
#endif /* DIRECTORY_SEPARATOR */

#ifndef DIRECTORY_SEPARATOR_STRING
#define DIRECTORY_SEPARATOR_STRING "/"
#endif /* DIRECTORY_SEPARATOR_STRING */

/*
Set a flag that indicates that Windows paths are expected to be encountered and
enables processing accordingly.  Note that this differs from __MICROSOFT_OS__,
as it's possible to have an OS that supports Windows paths but isn't Windows.
*/
#ifndef WINDOWS_PATHS_ALLOWED
#if __MICROSOFT_OS__ || defined(__CYGWIN__)
#define WINDOWS_PATHS_ALLOWED TRUE
#else /* !(__MICROSOFT_OS__ || defined(__CYGWIN__)) */
#define WINDOWS_PATHS_ALLOWED FALSE
#endif /* __MICROSOFT__OS || defined(__CYGWIN__) */
#endif /* ifndef WINDOWS_PATHS_ALLOWED */

#if __MICROSOFT_OS__
#if !WINDOWS_PATHS_ALLOWED
#error WINDOWS_PATHS_ALLOWED must be TRUE when __MICROSOFT_OS__ is TRUE
#endif /* !WINDOWS_PATHS_ALLOWED */
#endif /* MICROSOFT_OS__ */

/*
TRUE if '\' should also be treated as a directory separator in addition
to the one defined above.  This allows some of the Microsoft directory
handling code to be tested on a non-Microsoft system.
*/
#ifndef BACKSLASH_IS_ALSO_DIR_SEPARATOR
#if WINDOWS_PATHS_ALLOWED
#define BACKSLASH_IS_ALSO_DIR_SEPARATOR TRUE
#else /* !WINDOWS_PATHS_ALLOWED */
#define BACKSLASH_IS_ALSO_DIR_SEPARATOR FALSE
#endif /* WINDOWS_PATHS_ALLOWED */
#endif /* ifndef BACKSLASH_IS_ALSO_DIR_SEPARATOR */

#if WINDOWS_PATHS_ALLOWED
#if !BACKSLASH_IS_ALSO_DIR_SEPARATOR
 #error BACKSLASH_IS_ALSO_DIR_SEPARATOR must be TRUE when \
        WINDOWS_PATHS_ALLOWED is TRUE
#endif /* !BACKSLASH_IS_ALSO_DIR_SEPARATOR */
#endif /* WINDOWS_PATHS_ALLOWED */

EXTERN_THREAD a_boolean
		backslash_is_also_dir_separator;
			/* Initialized to BACKSLASH_IS_ALSO_DIR_SEPARATOR but
			   may be changed during execution. */

EXTERN_THREAD a_boolean
		windows_paths_allowed;
			/* Initialized to WINDOWS_PATHS_ALLOWED but may be
			   changed during execution. */

/* Add a component to a path name. */
extern void append_to_path_name(a_text_buffer_ptr	buffer,
				a_const_char		*name);

/* Combine a directory name and file name into a full path name. */
extern a_text_buffer_ptr combine_dir_and_file_name(
				a_const_char		*dir_name,
				a_const_char		*file_name,
				a_text_buffer_ptr	buffer);

/* Read a line from a file. */
extern char *read_line_from_file(FILE *f_file);

/* Replace the suffix of a file name with a specified suffix. */
extern void replace_file_name_suffix(a_const_char	*new_suffix,
		                     a_text_buffer_ptr	file_name_buffer);


/*
Include the files needed to define the types used with the stat()
function.  A declaration of stat() is provided in case the standard
headers fail to define the prototype.
*/
/* sys/types.h is needed, at least, on Unisys 2200 and Microsoft C 7.0. */
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
#include <sys/types.h>
#include <sys/stat.h>
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */

/* See if a file exists, if it does, return the modification time. */
extern a_boolean get_file_modification_time(a_const_char *file_name,
					    time_t       *time);

/* Get the file size. */
extern size_t get_file_size(a_const_char *file_name);

/* Get the file modification time as a string. */
extern char *get_file_modification_time_string(a_const_char	*file_name,
					       a_boolean	strip_newline);

/* Is the specified file a regular (e.g., not directory) file. */
extern a_boolean is_regular_file(a_const_char *file_name);

/* Is the specified file a directory. */
extern a_boolean is_directory(a_const_char *file_name);

extern a_boolean is_absolute_file_name(a_const_char *file_name);

/* Get path subsets. */
extern a_const_char *get_base_name(a_const_char *file_name);

/* Open a source file. */
extern
FILE *open_source_file(a_const_char          *file_name,
		       an_open_file_result   *open_result,
                       a_unicode_source_kind *unicode_source_kind);
/* Reopen a source file. */
extern FILE *reopen_source_file(a_const_char          *file_name,
                                a_unicode_source_kind *unicode_source_kind);
/* Open an output file. */
extern
FILE *open_output_file(a_const_char		*file_name,
                       a_boolean		binary_file,
                       a_boolean		update_mode,
		       an_open_file_result	*open_result);

a_boolean okay_as_output_file(a_const_char *file_name);

extern a_boolean file_exists(a_const_char *file_name);

extern FILE *fopen_with_result(a_const_char		*file_name,
                               a_const_char		*mode,
                               an_open_file_result	*open_result);

extern a_boolean close_output_file(FILE	*f_output,
				   int	*errno_value);

/* Open an input file. */
extern
FILE *open_input_file(a_const_char		*file_name,
                      a_boolean			binary_file,
		      an_open_file_result	*open_result);

extern void delete_file(a_const_char *file_name);

/* Open a temporary file. */
extern FILE *open_temp_file(a_boolean binary_file);
/* Close a temporary file. */
extern void close_temp_file(FILE *temp_file);
#if MAKE_FRONT_END_CALLABLE
/* If not NULL, close *f_file. */
extern void close_file_if_open(FILE	**f_file);
#endif /* MAKE_FRONT_END_CALLABLE */

extern a_boolean terminal_is_color_capable(void);

/*
Types used to determine the execution time of the compiler.
*/
typedef unsigned long a_cpu_time;
typedef unsigned long a_real_time;
typedef struct a_timer *a_timer_ptr;
typedef struct a_timer {
  unsigned long	cpu_time;
			/* The amount of cpu time used from the start of
			   compilation in milliseconds.  Note that this
			   value always contains milliseconds regardless of
			   the units normally used by the host. */
  unsigned long	real_time;
			/* The current system time in seconds (not necessarily
			   the return value of time()). */
} a_timer;

EXTERN void get_timer(a_timer *timer);

EXTERN void display_time_used(a_const_char	*message,
			      a_timer_ptr	start_time,
			      a_timer_ptr	end_time);


#if STANDALONE_UTILITY_PROGRAM
/*lint -sem(normal_termination, r_no)*/
NORETURN extern void normal_termination(void);
#endif /* STANDALONE_UTILITY_PROGRAM */

#if COMPILE_MULTIPLE_SOURCE_FILES
/* Identify the source file being compiled. */
extern void identify_source_file(void);
#endif /* COMPILE_MULTIPLE_SOURCE_FILES */

#ifndef COMPILING_MK_ERRINFO
/*
Internal coding used for error severities.
*/
enum an_error_severity : a_byte {
  es_default,	/* Must be zero. */
  es_once,	/* Used internally to issue certain diagnostics only once. */
  es_more_info,	/* Used to mark "more information" diagnostics. */
  es_none,
  es_remark,
  es_warning,
  es_command_line_warning,
  es_discretionary_error,
  es_error,
  es_catastrophe,
  es_command_line_error,
  es_internal_error
};

/*
The type of a (signed and unsigned) number used in a diagnostic.
*/
typedef int64_t  a_signed_diag_number;
typedef uint64_t an_unsigned_diag_number;

/* Terminate the compilation. */
/*lint -sem(term_compilation, r_no)*/
NORETURN extern void term_compilation(an_error_severity severity);
/* Write a compilation init message if appropriate. */
extern void write_init(void);
/* Write a compilation signoff message if appropriate. */
extern void write_signoff(void);
/* Terminate the compilation without a signoff message. */
/*lint -sem(exit_compilation, r_no)*/
NORETURN extern void exit_compilation(an_error_severity severity);
#endif /* !defined(COMPILING_MK_ERRINFO) */

extern a_const_char *file_name_in_internal_encoding(a_const_char *orig_name);

extern a_const_char *file_name_in_external_encoding(a_const_char *orig_name);

/* Get the next file name from the current directory. */
extern
char *get_file_name_from_dir(a_boolean    first,
			     a_const_char *dir_name,
			     a_const_char *suffix,
			     a_const_char *curr_dir_name);

/*
Define a local type for a Windows HANDLE (since HANDLE isn't defined on
non-Windows OSes).
*/
typedef void *a_windows_handle;

#if USE_MMAP_FOR_MEMORY_REGIONS
extern sizeof_t do_page_alignment(sizeof_t size);

/*
A typedef for an abstract representation of the handle used
for mmap (or equivalent) calls.
*/
#if EDG_WIN32
typedef a_windows_handle an_mmap_handle;
#else /* !EDG_WIN32 */
typedef FILE *an_mmap_handle;
#endif /* EDG_WIN32 */

extern
a_void_ptr map_memory_region_file(an_mmap_handle handle,
                                  void           *base_addr,
                                  sizeof_t       curr_size,
                                  sizeof_t       incremental_size,
                                  sizeof_t       file_offset);

/*
A type abstracting the platform specific needs of map_input_file_to_region that
should be constructed via a call to open_mapped_input_file and closed with a
call to close_mapped_input_file.
*/
struct a_mapped_input_file {
#if EDG_WIN32
  a_windows_handle
                mapped_input;
                        /* The handle produced by the call to
                           CreateFile_interface to reopen the file in a way
                           that can be used by CreateFileMapping. */
  an_mmap_handle
                map_object;
                        /* The handle produced by the call to
                           CreateFileMapping representing the actual memory
                           mapping. */
#else /* !EDG_WIN32 */
  an_mmap_handle
                file;   /* The FILE* used by the mmap call. */
#endif /* EDG_WIN32 */
};  /* a_mapped_input_file */

extern a_mapped_input_file open_mapped_input_file(a_const_char *file_name,
                                                  FILE         *open_file);
extern void close_mapped_input_file(a_mapped_input_file file);

extern
a_void_ptr map_input_file_to_region(a_mapped_input_file file,
                                    a_boolean           read_only,
                                    sizeof_t            offset,
                                    sizeof_t            size,
                                    a_void_ptr          address,
                                    a_const_char        *file_name);

extern void unmap_memory(a_void_ptr addr,
                         sizeof_t   size);

extern sizeof_t seek_to_page_alignment(FILE *file);

extern an_mmap_handle open_memory_region_tmp_file(void);

#if MAKE_FRONT_END_CALLABLE
extern void close_memory_region_tmp_file(an_mmap_handle file);
#endif /* MAKE_FRONT_END_CALLABLE */

extern size_t get_page_size();

#endif /* USE_MMAP_FOR_MEMORY_REGIONS */

/*
The type of the buffer argument in fread/fwrite calls, and the type
of the pointer passed to realloc.  This is usually void* on ANSI compilers
and char* on pcc compilers.  Sun C++ uses char* for some reason though.
*/
#if defined(__SUNPRO_CC)
typedef char *a_stdio_arg;
#else /* !defined(__SUNPRO_CC) */
typedef a_void_ptr a_stdio_arg;
#endif /* defined(__SUNPRO_CC) */

/*
Determine whether the template lookup mechanism is needed.  This is
also used as a factor in determining whether name mangling is needed.
*/
#if AUTOMATIC_TEMPLATE_INSTANTIATION || COMPILE_MULTIPLE_TRANSLATION_UNITS
#define TEMPLATE_LOOKUP_NEEDED TRUE
#else /* !(AUTOMATIC_TEMPLATE_INSTANTIATION ||
           COMPILE_MULTIPLE_TRANSLATION_UNITS) */
#define TEMPLATE_LOOKUP_NEEDED FALSE
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION ||
          COMPILE_MULTIPLE_TRANSLATION_UNITS */


/*
Flag that is TRUE if coroutines should be accepted.  Note that lowering is
currently incomplete for coroutines and the back-end must be able to handle
these constructs.  The C++-generating back end can, but the C-generating back
end cannot.
*/
#ifndef COROUTINE_ENABLING_POSSIBLE
#if DO_IL_LOWERING
#define COROUTINE_ENABLING_POSSIBLE FALSE
#else /* !DO_IL_LOWERING */
#define COROUTINE_ENABLING_POSSIBLE TRUE
#endif /* DO_IL_LOWERING */
#endif /* COROUTINE_ENABLING_POSSIBLE */

/*
Determine whether the module id routines are needed.  They are needed
if IL lowering or name mangling are used, when the C generating back end
is not generating ANSI C, when the automatic template instantiation
mechanism is enabled, or when the front end is configured to compile
multiple translation units (because name mangling is used for export
template support in such cases).  The name mangling test is sufficient to
cover the C generating back end case because name mangling is required when
using IL lowering or the C generating back end.  In cases where the C
generating back end is being used as a standalone utility, the module id
routines are needed when C_GEN_BE_GENERATES_ANSI_C is FALSE.
*/
#if NEED_NAME_MANGLING || \
    TEMPLATE_LOOKUP_NEEDED || \
    (STANDALONE_UTILITY_PROGRAM && \
     BACK_END_IS_C_GEN_BE && \
     !C_GEN_BE_GENERATES_ANSI_C)
#define MODULE_ID_NEEDED TRUE
#else /* !(NEED_NAME_MANGLING || TEMPLATE_LOOKUP_NEEDED || ...) */
#define MODULE_ID_NEEDED FALSE
#endif /* NEED_NAME_MANGLING || TEMPLATE_LOOKUP_NEEDED || ... */

#if MODULE_ID_NEEDED

extern void change_non_id_characters(char *str);

extern void set_module_id(a_const_char *new_module_id);

extern a_const_char *get_module_id(void);

extern a_const_char *make_module_id(a_const_char *external_name);

#endif /* MODULE_ID_NEEDED */

extern void write_file_name(a_const_char *name,
                            FILE         *f_output,
                            a_boolean    process_escapes,
                            a_boolean    escape_nonprintable_chars);

extern void write_file_name_to_text_buffer(
                                  a_const_char     *name,
                                  a_text_buffer_ptr buffer,
                                  a_boolean         process_escapes,
                                  a_boolean         escape_nonprintable_chars);

extern char *format_file_name(a_const_char *name);

extern char *format_source_file_name(a_source_file_ptr sfp,
                                     a_boolean         use_name_as_written,
                                     a_boolean         quote_file_name);

extern a_const_char *suffix_of(a_const_char *file_name);

extern unsigned long extract_character_from_string(a_const_char  *str,
                                                   unsigned int  char_size);

#define extract_wide_char_from_string(str)                                  \
  (extract_character_from_string(str, targ_sizeof_wchar_t))

extern int ucn_to_utf16(unsigned long   ucn,
                        unsigned short  *encoding);

/*
If defined to TRUE, alternative definitions of default_error_output_file(),
default_preproc_output_file(), and (if NEED_IL_DISPLAY is TRUE)
default_il_display_output_file() must be provided.  See the default
implementations of the respective functions in host_envir.c for more
information.
*/
#ifndef CUSTOM_DEFAULT_OUTPUT_FILES
#define CUSTOM_DEFAULT_OUTPUT_FILES FALSE
#endif /* CUSTOM_DEFAULT_OUTPUT_FILES */

extern FILE* default_error_output_file();

extern FILE* default_preproc_output_file();

#if NEED_IL_DISPLAY
extern FILE* default_il_display_output_file();
#endif /* NEED_IL_DISPLAY */

extern void host_envir_one_time_init(void);

extern void host_envir_trans_unit_init(void);

extern void host_envir_early_init(void);

extern void host_envir_init(void);

/*
Define a macro that can be used to compare the characters that make
up file names.
*/
#if __MICROSOFT_OS__
/* On MS-DOS, the comparison must be case insensitive. */
#if EDG_WIN32 && UNICODE_SOURCE_SUPPORTED
extern int compare_file_chars_case_insensitive(a_const_char *file1,
                                               a_const_char *file2);
#define compare_file_chars(s1, s2)					\
  (compare_file_chars_case_insensitive((s1), (s2)))
#else /* !(EDG_WIN32 && UNICODE_SOURCE_SUPPORTED) */
#define compare_file_chars(s1, s2) strnicmp((s1), (s2), INT_MAX)
#endif /* EDG_WIN32 && UNICODE_SOURCE_SUPPORTED */
#else /* !__MICROSOFT_OS__ */
/* On other systems, the comparison is case sensitive. */
#define compare_file_chars(s1, s2) strcmp((s1), (s2))
#endif /* __MICROSOFT_OS__ */

#if EDG_WIN32
/* Define a typedef that represents the same type as DWORD on Windows. */
typedef uint32_t an_ms_dword;
#endif /* EDG_WIN32 */

/*
Macro that is TRUE if the stat() library function can be used to get
inode information.
*/
#ifndef STAT_AVAILABLE
/*
If we are not on Windows, assume we are on a Unix-like system that supports
the stat system call if the S_ISDIR or S_IFDIR macro is defined.
*/
#if !EDG_WIN32
#if defined(S_ISDIR) || defined(S_IFDIR)
#define STAT_AVAILABLE TRUE
#else /* !(defined(S_ISDIR) || defined(S_IFDIR)) */
#define STAT_AVAILABLE FALSE
#endif /* defined(S_ISDIR) || defined(S_IFDIR) */
#endif /* !EDG_WIN32 */
#endif /* STAT_AVAILABLE */

/*
Determine if the operating system provides a mechanism to uniquely identify
a file even in the presence of symbolic and/or hard links (e.g., inode
information on Unix-like systems).

Support for this feature is available on Windows, but is disabled by
default because the Microsoft compiler does not use such a facility and
because the mechanism can produce incorrect results for network mounted
file systems.
*/
#ifndef UNIQUE_FILE_IDENTIFIER_AVAILABLE
#if EDG_WIN32
#define UNIQUE_FILE_IDENTIFIER_AVAILABLE FALSE
#else /* !EDG_WIN32 */
#if STAT_AVAILABLE
#define UNIQUE_FILE_IDENTIFIER_AVAILABLE TRUE
#else /* !STAT_AVAILABLE */
#define UNIQUE_FILE_IDENTIFIER_AVAILABLE FALSE
#endif /* STAT_AVAILABLE */
#endif /* EDG_WIN32 */
#endif /* ifndef UNIQUE_FILE_IDENTIFIER_AVAILABLE */

/*
If the operating system provides a mechanism to uniquely identify
a file even in the presence of symbolic and/or hard links (e.g., inode
information on Unix-like systems) define a structure that can store
the identifying information.
*/
#if UNIQUE_FILE_IDENTIFIER_AVAILABLE
#if EDG_WIN32
/*
On Windows, the unique file ID information contains fields copied from the
_BY_HANDLE_FILE_INFORMATION structure.  The fields have the same name
as the fields from which they are copied.
*/
typedef struct a_unique_file_id {
  an_ms_dword	dwVolumeSerialNumber;
			/* Identifier for the volume containing the file. */
  an_ms_dword	nFileIndexHigh;
			/* High-order part of the unique identifier within
			   a given volume. */
  an_ms_dword	nFileIndexLow;
			/* Low-order part of the unique identifier within
			   a given volume. */
} a_unique_file_id;
#else /* !EDG_WIN32 */
#if !STAT_AVAILABLE
 #error STAT_AVAILABLE must be TRUE when UNIQUE_FILE_IDENTIFIER_AVAILABLE \
        is TRUE on non-Windows platforms. 
#endif /* !STAT_AVAILABLE */
/*
On systems with the stat structure, the unique file ID information contains
fields copied from that structure.  The fields have the same name as the
fields from which they are copied.
*/
typedef struct a_unique_file_id {
  dev_t		st_dev;
			/* Identifier for the device containing the file. */
  ino_t		st_ino;
			/* Unique identifier (inode) within the device. */
} a_unique_file_id;
#endif /* EDG_WIN32 */
#endif /* UNIQUE_FILE_IDENTIFIER_AVAILABLE */

#if UNIQUE_FILE_IDENTIFIER_AVAILABLE
typedef struct a_unique_file_id *a_unique_file_id_ptr;

extern void clear_unique_file_id(a_unique_file_id_ptr	ufip);

extern
void get_unique_id_for_file(a_const_char		*file_name,
			    a_unique_file_id_ptr	unique_id);

extern
a_boolean same_unique_file_ids(a_unique_file_id_ptr	id1,
			       a_unique_file_id_ptr	id2);

extern a_hash_value hash_unique_file_id(a_unique_file_id_ptr	id);
#endif /* UNIQUE_FILE_IDENTIFIER_AVAILABLE */

/*
Wrapper macro that calls f_compare_file_names with a default value for
the ignore_delimiters, is_partial_file_name, and in_general_memory parameters.
*/
#define compare_file_names(s1, s2)					\
  (f_compare_file_names(s1, s2, /*ignore_delimiters=*/FALSE,		\
                        /*is_partial_file_name=*/FALSE,			\
                        /*in_general_memory=*/FALSE))

/*
Wrapper macro that calls f_compare_file_names with a default value for
the ignore_delimiters and is_partial_file_name parameters, with allocations
occurring in general memory.
*/
#define compare_file_names_general(s1, s2)				\
  (f_compare_file_names(s1, s2, /*ignore_delimiters=*/FALSE,		\
                        /*is_partial_file_name=*/FALSE,			\
                        /*in_general_memory=*/TRUE))

extern a_const_char *start_of_file_name(a_const_char *file_name);

extern char *normalize_file_name(a_const_char *file_name);

extern int f_compare_file_names(a_const_char	*file1,
	 		        a_const_char	*file2,
		                a_boolean	ignore_delimiters,
			        a_boolean	is_partial_file_name,
			        a_boolean       in_general_memory);

extern int compare_dir_names(a_const_char *dir1,
			     a_const_char *dir2,
                             a_boolean	is_partial_file_name);

#ifndef STDLIB_H_INCLUDED
/*
When stdlib.h is not used, provide a declaration for bsearch and qsort.
*/
extern "C" a_void_ptr bsearch(a_const_void_ptr key,
                              a_const_void_ptr base,
                              sizeof_t         nmemb,
                              sizeof_t         size,
                              int(*compar)(a_const_void_ptr,
                                           a_const_void_ptr));

#if __BSD__
extern "C" int qsort(a_void_ptr       base,
                     int              nmemb,
                     int              size,
                     int(*compar)(a_const_void_ptr,
                                  a_const_void_ptr));
#else /* !__BSD__ */
extern "C" void qsort(a_void_ptr       base,
                      sizeof_t         nmemb,
                      sizeof_t         size,
                      int(*compar)(a_const_void_ptr,
                                   a_const_void_ptr));
#endif /* __BSD__ */

#endif /* ifndef STDLIB_H_INCLUDED */

#if defined(__SUNPRO_CC) && __BSD__
/* Sun C++ on SunOS 4.1.3 uses const char * as the first argument of
   bsearch. */
typedef const char * a_bsearch_arg_type;
#else /* !(defined(__SUNPRO_CC) && __BSD__) */
typedef a_const_void_ptr a_bsearch_arg_type;
#endif /* defined(__SUNPRO_CC) && __BSD__ */

/* BSD systems use (signed) int to count the number of elements to be sorted
   by qsort. */
#if __BSD__
typedef int qsort_nmemb_type;
#else /* !__BSD__ */
typedef sizeof_t qsort_nmemb_type;
#endif /* __BSD__ */

#if EDG_WIN32
extern char *conv_wide_to_utf8(wchar_t *wide_str);
#if !STANDALONE_UTILITY_PROGRAM
extern a_const_char *com_error_to_str(void);
extern a_const_char *win32_error_to_str(an_ms_dword error_code);
#endif /* !STANDALONE_UTILITY_PROGRAM */
#endif /* EDG_WIN32 */

/* Custom version of memcmp. */
extern int smemcmp(a_const_char *s1,
                   a_const_char *s2,
                   sizeof_t     length);

/* Routines defined in host_util.h. */

extern unsigned long crc_32(a_const_char  *str,
                            unsigned long prev_crc);

#if ONE_INSTANTIATION_PER_OBJECT
extern a_const_char *generate_instantiation_output_file_name(
                                                   a_const_char *mangled_name);
#endif /* ONE_INSTANTIATION_PER_OBJECT */

#if !EDG_WIN32 && !MULTIPLE_THREAD_COMPILATION
extern void set_cpu_time_limit(int	seconds);
#endif /* !EDG_WIN32 && !MULTIPLE_THREAD_COMPILATION */

#if DEBUG
extern void db_incl_search_path(void);
#endif /* DEBUG */

EXTERN_THREAD a_boolean
		prototype_instantiations_in_il;
			/* If TRUE, prototype instantiations are recorded
			   in the IL tree. */

EXTERN_THREAD a_boolean
		all_template_info_in_il;
			/* If TRUE complete information about templates is
			   provided in the il.  See ALL_TEMPLATE_INFO_IN_IL
			   for more information. */

EXTERN_THREAD a_boolean
		in_front_end;
			/* TRUE while in the front end, FALSE elsewhere (e.g.,
			   in the C-generating back end).  TRUE in IL lowering,
			   if that is done.  FALSE in command-line processing
			   before the front end starts up.  Stays FALSE in
			   a standalone utility program. */

EXTERN_THREAD a_boolean
		pragma_define_type_info_is_required;
			/* TRUE if "#pragma define_type_info" is required
			   before a declaration of the standard class
			   "type_info". */

EXTERN_THREAD a_boolean
		host_little_endian;
			/* TRUE if the host system uses little-endian
			   byte ordering. */

EXTERN_THREAD a_const_char
		*edg_base_directory;
			/* The directory in which to find files needed by
			   the front end at execution time (e.g., the
			   predefined macro table). */

EXTERN_THREAD a_boolean
		use_predefined_macro_file;
			/* TRUE if the file specified by
			   PREDEFINED_MACRO_FILE_NAME should be used to
			   predefine macros at the start of compilation. */

EXTERN_THREAD a_boolean
		create_template_deduction_name_references;
			/* TRUE if name references should be created in
			   template deduction contexts. */

enum a_predef_macro_mode {
  pmm_none,
  pmm_gnu,		/* Any GNU mode. */
  pmm_gcc,		/* gcc mode. */
  pmm_gpp,		/* g++ mode. */
  pmm_clang,		/* Any clang mode. */
  pmm_clang_c,		/* clang C mode. */
  pmm_clang_cpp,	/* clang C++ mode. */
  pmm_gnu_or_clang,	/* Any GNU or clang mode. */
  pmm_microsoft,	/* Microsoft mode. */
  pmm_strict,		/* Strict mode. */
  pmm_cpp,		/* Compiling C++. */
  pmm_all,		/* Define in all modes. */
  pmm_last
};

EXTERN_THREAD a_boolean
		predef_macro_mode_values[(int)pmm_last];
			/* TRUE if a given predefined macro mode should be
			   considered to be in effect. */

EXTERN_CONSTINIT_ARRAY(a_const_char*, predef_macro_mode_names, pmm_last + 1)
			/* A list of the mode strings that may be used in
			   predefined macro definition entries. */
#if VAR_INITIALIZERS
= {
/* pmm_none */		NULL,
/* pmm_gnu */		"gnu",
/* pmm_gcc */		"gcc",
/* pmm_gpp */		"gpp",
/* pmm_clang */		"clang",
/* pmm_clang_c */	"clang_c",
/* pmm_clang_cpp */	"clang_cpp",
/* pmm_gnu_or_clang */	"gnu_or_clang",
/* pmm_microsoft */	"microsoft",
/* pmm_strict */	"strict",
/* pmm_cpp */		"cpp",
/* pmm_all */		"all",
/* pmm_last */		"last"
}
#endif /* VAR_INITIALIZERS */
EXTERN_CONSTINIT_ARRAY_END(predef_macro_mode_names)

EXTERN_THREAD a_boolean
		lowering_normalizes_boolean_controlling_expressions
#if VAR_INITIALIZERS
                         = LOWERING_NORMALIZES_BOOLEAN_CONTROLLING_EXPRESSIONS
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* TRUE if IL lowering should normalize boolean
			   controlling expressions to always produce 0/1. */

EXTERN_THREAD a_boolean
		record_unrecognized_attributes
#if VAR_INITIALIZERS
                         = RECORD_UNRECOGNIZED_ATTRIBUTES
#endif /* VAR_INITIALIZERS */
                                                         ;
			/* TRUE if unrecognized attributes should be recorded
			   in the IL.  If FALSE, a warning (by default) is
			   emitted on unrecognized attributes. */

#if READ_CPPCLI_PORTABLE_ASSEMBLIES || WRITE_CPPCLI_PORTABLE_ASSEMBLIES
typedef struct a_portable_assembly_header
                                      a_portable_assembly_header_dummy_typedef;
extern void clear_portable_assembly_header(
                                    struct a_portable_assembly_header *header);
#endif /* READ_CPPCLI_PORTABLE_ASSEMBLIES || WRITE_CPPCLI_PORTABLE_ASSEMBLIES*/

#if BUILTIN_FUNCTIONS_ENABLED
extern unsigned long strtoul_interface(a_const_char *str,
                                       a_boolean    *err);
#endif /* BUILTIN_FUNCTIONS_ENABLED */

/*
Versions of GCC older than 9.3 have a bug where enumerations with a fixed
underlying type are assumed to be able to store all possible values that the
underlying type can, regardless of what the enumerators actually specify.  This
causes spurious warnings when these enumerations are used in bit fields, and
unfortunately, the warnings are not suppressible via the command line.
*/
#if !defined(USE_ENUMS_IN_BITFIELDS)
#if defined(__GNUC__) && \
    (__GNUC__ < 9 || (__GNUC__ == 9 && __GNUC_MINOR__ < 3))
#define USE_ENUMS_IN_BITFIELDS FALSE
#else /* !defined(__GNUC__) || [GNU_VERSION >= 9.3] */
#define USE_ENUMS_IN_BITFIELDS TRUE
#endif /* defined(__GNUC__) && [GNU_VERSION < 9.3] */
#endif /* !defined(USE_ENUMS_IN_BITFIELDS) */

#if USE_ENUMS_IN_BITFIELDS

#define ENUM_TYPE_FOR_BIT_FIELD(type) type

template<typename Enum_type>
constexpr Enum_type enum_cast(Enum_type val)
/*
In this mode, enumerations can be used in bit fields, and no cast is needed.
Do nothing and return the value, unmodified.
*/
{
  return val;
}  /* enum_cast */

#else /* !USE_ENUMS_IN_BITFIELDS */

#define ENUM_TYPE_FOR_BIT_FIELD(type) a_bit_field

template<typename Enum_type, typename Param_type>
constexpr Enum_type enum_cast(Param_type val)
/*
In this mode, enumerations cannot be used in bit fields, and so those bit
fields must be cast to the enumeration before they can be assigned to an
instance of that enumeration.  Perform the cast and return the (otherwise
unmodified) value.
*/
{
  return (Enum_type)val;
}  /* enum_cast */

#endif /* USE_ENUMS_IN_BITFIELDS */

EXTERN_THREAD a_boolean
		add_match_notes;
			/* TRUE if notes should be added to error messages
			   describing failures to match overload sets. */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef HOST_ENVIR_H */

