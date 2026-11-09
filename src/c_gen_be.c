/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

c_gen_be.c -- C-generating back end.

Compile with STANDALONE_C_GEN_BE defined and BACK_END_IS_C_GEN_BE
defined as 1 to get a main program back end.  Otherwise, a version to be
called in the same program as the front end is produced (if needed).

If C_GEN_BE_GENERATES_ANSI_C is TRUE (see targ_def.h), ANSI C is generated
instead of K&R C.
*/

#ifdef PCH_PRAGMA_GUARD
/* Suppress generation of a precompiled header file -- c_gen_be.c cannot
   share its precompiled header with any other file.  (The only utility from
   generating a precompiled header file would be for recompilation; for
   that, the no_pch pragma should be removed and a hdrstop pragma added
   after the last #include, outside all #ifs.)  */
#pragma no_pch
#endif /* PCH_PRAGMA_GUARD */

#include "basic_hdrs.h"

#if STANDALONE_C_GEN_BE
#if !BACK_END_IS_C_GEN_BE
/* We could just set the flag here for THIS compilation, but we want to
   ensure that it's set for the compilation of the OTHER files needed
   in the standalone program version of c_gen_be. */
 #error -- BACK_END_IS_C_GEN_BE should be defined as 1 (on the command line \
           or in defines.h)
#endif /* !BACK_END_IS_C_GEN_BE */
#endif /* STANDALONE_C_GEN_BE */

/* See if this code is needed at all. */
#if BACK_END_IS_C_GEN_BE

/* Header files common to all files. */
#include "fe_common.h"

#include <errno.h>

/* Additional header files. */
#include "c_gen_be.h"

#include "il_walk.h"
#if IL_SHOULD_BE_WRITTEN_TO_FILE
#include "il_file.h"
#include "il_read.h"
#if !STANDALONE_C_GEN_BE
#include "il_write.h"
#endif /* !STANDALONE_C_GEN_BE */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

#if STANDALONE_C_GEN_BE
#include "fe_init.h"
#endif /* STANDALONE_C_GEN_BE */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

#if !LOWER_LVALUE_RETURNING_OPERATIONS
 #error -- The C-generating back end requires \
            LOWER_LVALUE_RETURNING_OPERATIONS TRUE
#endif /* !LOWER_LVALUE_RETURNING_OPERATIONS */

#if !SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
 #error -- The C-generating back end requires \
            SCOPE_ORPHANED_LIST_PROCESSING_NEEDED TRUE
#endif /* !SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */

#ifndef DUMP_LOWERED_EH_CONSTRUCTS_IN_C_GEN_BE
#if !DO_FULL_PORTABLE_EH_LOWERING
 #error -- DO_FULL_PORTABLE_EH_LOWERING required for the C-generating back end.
#endif /* !DO_FULL_PORTABLE_EH_LOWERING */
#endif /* DUMP_LOWERED_EH_CONSTRUCTS_IN_C_GEN_BE */

#if !C_GEN_BE_GENERATES_ANSI_C
#if ASM_FUNCTION_ALLOWED
/* asm functions cannot be generated if K&R C, since they require function
   prototypes (except when old-style parameters are implicitly declared). */
 #error -- When K&R C is put out the C-generating back end requires \
            ASM_FUNCTION_ALLOWED FALSE
#endif /* ASM_FUNCTION_ALLOWED */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */

#if PARENS_IN_IL
/* The C-generating back end doesn't handle the eok_parens operator. */
 #error -- PARENS_IN_IL cannot be set when the C-generating back end is used.
#endif /* PARENS_IN_IL */

#if !LOWER_CLASS_RVALUE_ADJUST
 #error -- The C-generating back end requires LOWER_CLASS_RVALUE_ADJUST TRUE
#endif /* !LOWER_CLASS_RVALUE_ADJUST */

#if IMPLEMENTATION_SUPPORTS_MULTIPLE_THREADS && \
    !USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES
/* The C-generating back end doesn't know when threads are created, so it
   can't invoke the thread_local initializations at the proper time. */
 #error -- The C-generating back end requires lazy initialization for \
            thread_local variables
#endif /* IMPLEMENTATION_SUPPORTS_MULTIPLE_THREADS && !USE_LAZY_INIT... */

/*
See if the target is the SunPro C compiler.
*/
#ifndef SUNPRO_C_IS_C_GEN_BE_TARGET
#if defined(__SUNPRO_C) || defined(__SUNPRO_CC)
#define SUNPRO_C_IS_C_GEN_BE_TARGET TRUE
#else /* ifndef __SUNPRO_C */
#define SUNPRO_C_IS_C_GEN_BE_TARGET FALSE
#endif /* !(defined(__SUNPRO_C) || defined(__SUNPRO_CC)) */
#endif /* ifndef SUNPRO_C_IS_C_GEN_BE_TARGET */

/*
See if the target is the SGI C compiler, which we know something about.
*/
#ifndef SGIC
#ifdef __sgi
#if !GCC_IS_GENERATED_CODE_TARGET
#define SGIC TRUE
#endif /* !GCC_IS_GENERATED_CODE_TARGET */
#endif /* ifdef __sgi */
#ifndef SGIC
#define SGIC FALSE
#endif /* ifndef SGIC */
#endif /* ifndef SGIC */

/*
Macro to be used for alloc_il calls in the C-generating back end.  In a
standalone program, alloc_il is not available.
*/
#if STANDALONE_UTILITY_PROGRAM
#define alloc_il_for_c_gen_be(length) alloc_general(length)
#else /* !STANDALONE_UTILITY_PROGRAM */
#define alloc_il_for_c_gen_be(length) alloc_il(length)
#endif /* STANDALONE_UTILITY_PROGRAM */

/*
Macro to check if a node has side-effects.  It's always safe to assume that
nodes do have side-effects, so return TRUE in configurations where
node_has_side_effects isn't available.
*/
#if STANDALONE_C_GEN_BE
#define c_gen_node_has_side_effects(arg1, arg2) TRUE
#else /* !STANDALONE_C_GEN_BE */
#define c_gen_node_has_side_effects(arg1, arg2) \
	node_has_side_effects(arg1, arg2)
#endif /* STANDALONE_C_GEN_BE */

/*
Macros used to determine whether a given entity is needed in the generated
C code.  They use the "needed" flag if that is being maintained and the
"referenced" flag otherwise.
*/
#if MAINTAIN_NEEDED_FLAGS
#define scp_is_needed_in_generated_code(scp) needed_flag_is_set(scp)
#else /* !MAINTAIN_NEEDED_FLAGS */
#define scp_is_needed_in_generated_code(scp) (scp)->referenced
#endif /* MAINTAIN_NEEDED_FLAGS */
/*lint -esym(750,*entity_needed_in_generated_code)*/
#define entity_needed_in_generated_code(entityp)               \
  scp_is_needed_in_generated_code(&(entityp)->source_corresp)

/*
Macro to simplify the test for cases where the module id is needed in
the C-generating back end.
*/
#if !C_GEN_BE_GENERATES_ANSI_C || \
    SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
#define C_GEN_BE_NEEDS_MODULE_ID TRUE
#else /* !(!C_GEN_BE_GENERATES_ANSI_C || SEPARATE_ROUTINES_FOR_FILE_...) */
#define C_GEN_BE_NEEDS_MODULE_ID FALSE
#endif /* (!C_GEN_BE_GENERATES_ANSI_C || SEPARATE_ROUTINES_FOR_FILE_...) */

#if !C_GEN_BE_GENERATES_ANSI_C
/*
Prefix for the name of the file-scope initialization routine generated
by c_gen_be for any required file-scope initializations.  This is not
the same as the initialization routine generated by IL lowering.
*/
#define C_GEN_BE_INIT_ROUTINE_NAME_PREFIX "__cgi__"
#endif /* !C_GEN_BE_GENERATES_ANSI_C */

#if SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
/*
Prefix for the name of the C-generating back end routines that coalesce
file-scope dynamic initializations.
*/
#define C_GEN_BE_COALESCE_INIT_ROUTINE_NAME_PREFIX "__fsi__"
#endif /* SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */


#define MAX_OUTPUT_LINE_SIZE 300 /* Arbitrary. */
			/* Maximum allowable output line size (approximate). */
STATIC_THREAD unsigned int
		line_wrapping_disabled;
			/* If 0, output lines will be wrapped to keep them
			   within MAX_OUTPUT_LINE_SIZE if possible. */
/* Macros to turn line wrapping on and off. */
#define disable_line_wrapping() (line_wrapping_disabled++)
#define enable_line_wrapping() (line_wrapping_disabled--)

STATIC_THREAD FILE
		*f_primary;
			/* Primary file to which generated C is written. */
STATIC_THREAD FILE
		*f_C_output;
			/* File to which the generated C is currently being
			   written. */
/* Current output position -- file, line, sequence number, column: */
STATIC_THREAD a_source_file_ptr
		curr_output_file;
STATIC_THREAD a_line_number
		curr_output_line;
STATIC_THREAD uint32_t
		curr_output_column;
			/* The number of characters written to the current
			   line of output.  Zero means nothing has been
			   written so far. */
#if ONE_INSTANTIATION_PER_OBJECT
STATIC_THREAD a_text_buffer_ptr
		C_output_file_name_buffer;
			/* A text buffer used to construct the name of the
			   C output file. */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
STATIC_THREAD a_boolean
		curr_output_pos_known;
			/* TRUE if the current output position is known. */
STATIC_THREAD uint32_t
		indent;
			/* Number of spaces to indent at the start of a
			   line (when annotating). */
/* Last "known good" output position, from the last call of
   set_output_position: */
STATIC_THREAD a_line_number
		last_known_good_line;
STATIC_THREAD a_source_file_ptr
		last_known_good_file;


/*
Data structure used to save information about the current output position
within a file so that we can switch between different output files and
retain information about the current position in each of those files.
*/
typedef struct an_output_file_position *an_output_file_position_ptr;
typedef struct an_output_file_position {
  a_source_file_ptr
		curr_output_file;
			/* File entry for output file. */
  a_line_number	curr_output_line;
			/* Current output line number. */
  uint32_t	curr_output_column;
			/* Current output column number. */
  a_boolean	curr_output_pos_known;
			/* Current output position is known. */
} an_output_file_position;

/*
Saved output position for each file (primary, file-scope initializations,
and routine initializations).
*/
STATIC_THREAD an_output_file_position
		primary_output_position;
#if !C_GEN_BE_GENERATES_ANSI_C
STATIC_THREAD an_output_file_position
		file_scope_inits_output_position;
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
STATIC_THREAD an_output_file_position
		rout_dynamic_inits_output_position;


/*
Temporary files used for initialization code that must be rendered as
assignment statements:
*/
#if !C_GEN_BE_GENERATES_ANSI_C
STATIC_THREAD FILE
		*f_file_scope_inits;
			/* Static initializations at the file scope. */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
STATIC_THREAD FILE
		*f_rout_dynamic_inits;
			/* Dynamic initializations at the routine level. */

STATIC_THREAD uint32_t
		in_comment;
			/* Flag indicating whether the current output is
			   inside a comment. */
STATIC_THREAD a_boolean
		annotate;
			/* Flag indicating whether or not annotations should
			   be output. */
#if ASM_FUNCTION_ALLOWED
STATIC_THREAD a_boolean
		within_asm_function_definition;
			/* Flag indicating generation of an asm function is in
			   progress (used to suppress forward declarations of
			   asm functions and for special handling with
			   implicitly declared old-style parameters). */
#endif /* ASM_FUNCTION_ALLOWED */
#if C_GEN_BE_NEEDS_MODULE_ID
STATIC_THREAD a_const_char
		*module_id;
STATIC_THREAD char
		*module_init_id;
			/* Seed for module-unique names. */
#endif /* C_GEN_BE_NEEDS_MODULE_ID */
#if !C_GEN_BE_GENERATES_ANSI_C
STATIC_THREAD a_boolean
		file_scope_init_routine_called;
			/* TRUE if a file-scope initialization routine has
			   been called. */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
STATIC_THREAD a_scope_ptr
		curr_scope;
			/* Points to the scope being processed currently
			   (file, function, or block). */

STATIC_THREAD a_constant_ptr
		wide_string_constants_to_unbind_at_end_of_scope;
			/* List of wide string literal constants whose bindings
			   to variables must be broken at the end of the
			   current scope. */

STATIC_THREAD a_constant
		wide_string_constant_marker;
			/* Used to mark the end of the list pointed to by
			   wide_string_constants_to_unbind_at_end_of_scope as
			   NULL cannot be used.  As this is basically a
			   "dummy" constant, only the address is used,
			   not the value. */

STATIC_THREAD a_scope_ptr
		entry_routine_scope;
			/* If non-NULL, we are expanding the body of an
			   overriding virtual function with a covariant return
			   type, a thunk, or an alternate entry point for a
			   constructor or destructor in the IA-64 ABI.
			   This is the top-level scope of the
			   entry/wrapper function. */
STATIC_THREAD a_scope_ptr
		master_routine_scope;
			/* If non-NULL, we are expanding the body of an
			   overriding virtual function with a covariant
			   return type, a thunk, an alternate entry point
			   for a constructor or destructor in the IA-64
			   ABI, or the alternate entry for a no-capture
			   lambda call operator.  This is the top-level
			   scope of the underlying function for which
			   entry_routine_scope gives the entry/wrapper
			   function scope. */
STATIC_THREAD a_variable_ptr
		master_routine_return_variable;
			/* If non-NULL, we are expanding the body of a
			   master routine that returns "this" and all returns
			   within this expansion should be replaced with
			   assignments of the returned value to 
			   master_routine_return_variable (passing the returned
			   value back to the alternate entry point that
			   is calling the master routine). */
STATIC_THREAD a_const_char
		*end_of_master_routine_label = "__L_end_of_master_routine";
			/* Label used to indicate the end of a master routine.
			   Used as a target for "inlined" returns from the
			   master routine. */
STATIC_THREAD int
		num_master_params_added;
			/* The number of additional parameters that the
			   master routine has relative to the entry/wrapper
			   routine. */
STATIC_THREAD a_boolean
                skip_this_parameter;
                        /* If TRUE, skip over the "this" parameter when mapping
                           between master routine parameters and wrapper
                           routine parameters.  Used when the master routine is
                           the call operator of a no-capture lambda with an
                           ellipsis and the wrapper routine is the
                           corresponding special static member function. */
STATIC_THREAD an_il_to_str_output_control_block
		octl;	/* Output control block for interface to il_to_str
			   routines. */

STATIC_THREAD a_boolean
		output_initializer_code_directly;
			/* If TRUE, initializer executable code can be
			   output directly to f_C_output instead of to
			   a temporary file. */

STATIC_THREAD a_stdc_pragma_value
		curr_default_fp_contract;
			/* The value of the last STDC FP_CONTRACT pragma
			   emitted in file scope (stdc_pv_default if none was
			   emitted so far). */

STATIC_THREAD a_stdc_pragma_value
		curr_default_fenv_access;
			/* The value of the last STDC FENV_ACCESS pragma
			   emitted in file scope (stdc_pv_default if none was
			   emitted so far). */

STATIC_THREAD a_stdc_pragma_value
		curr_default_cx_limited_range;
			/* The value of the last STDC CX_LIMITED_RANGE pragma
			   emitted in file scope (stdc_pv_default if none was
			   emitted so far). */

#if FIXED_POINT_ALLOWED && !LOWER_FIXED_POINT
STATIC_THREAD a_stdc_pragma_value
		curr_default_fx_full_precision;
			/* The value of the last STDC FX_FULL_PRECISION pragma
			   emitted in file scope (stdc_pv_default if none was
			   emitted so far). */

STATIC_THREAD a_stdc_pragma_value
		curr_default_fx_fract_overflow;
			/* The value of the last STDC FX_FRACT_OVERFLOW pragma
			   emitted in file scope (stdc_pv_default if none was
			   emitted so far). */

STATIC_THREAD a_stdc_pragma_value
		curr_default_fx_accum_overflow;
			/* The value of the last STDC FX_ACCUM_OVERFLOW pragma
			   emitted in file scope (stdc_pv_default if none was
			   emitted so far). */
#endif /* FIXED_POINT_ALLOWED && !LOWER_FIXED_POINT */

#if UPC_EXTENSIONS_ALLOWED
STATIC_THREAD a_upc_access_method
		curr_default_upc_access_method;
			/* The UPC access method as set by the last UPC
			   pragma.  If no pragma has been emitted yet,
			   the default access specified in the IL header. */
#endif /* UPC_EXTENSIONS_ALLOWED */

/*
Block of state variables used by dump_initializer and its subroutines:
*/
typedef struct an_init_control_block *an_init_control_block_ptr;
typedef struct an_init_control_block {
   a_boolean	initializer_constants_started;
			/* At least one constant has been put out in this
			   initialization. */
  uint32_t	num_initializer_open_braces_deferred;
			/* Count of the number of open braces deferred at the
			   beginning of putting out a constant initializer.
			   The braces are deferred until we see the first
			   real constant.  If that weren't done, we could
			   go down several levels into a type and then
			   discover that the first thing to be initialized
			   is a union, i.e., that we can't initialize
			   any of the entity.  We would then have put out
			   something syntactically invalid like "= {}". */
  a_boolean	initializer_assignments_started;
			/* At least one initializer assignment has been
			   put out in this initialization. */
  a_boolean	suppress_initializer_equals;
			/* Suppress the "=" at the beginning of an
			   initializer. */
  a_boolean	first_time_test_closing_needed;
			/* A first-time test was generated around the
			   assignments in this initialization.  Therefore,
			   the test must be closed at the end of the
			   assignments. */
#if CHECKING
  a_boolean     zeroed;	/* TRUE if the variable has been zeroed. */
#endif /* CHECKING */
} an_init_control_block;

/* Value to use to specify that no source correspondence is provided. */
#define NO_SCP ((a_source_correspondence *)NULL)

/* Value to use to specify no variable. */
#define NO_VARIABLE ((a_variable_ptr)NULL)

/* Value to use to specify no routine. */
#define NO_ROUTINE ((a_routine_ptr)NULL)

/* Value to use to specify no field. */
#define NO_FIELD ((a_field_ptr)NULL)

/* Value to use to specify no temporary name generated from an IL entry
   address. */
#define NO_TEMP ((char *)NULL)

/* Value to use to specify that no name is provided. */
#define NO_NAME ((char *)NULL)

/* Value to use to specify that no counter is provided. */
#define NO_COUNTER ((uint32_t)0)


/*
Data structure used to save information about a pending typedef, i.e., a
typedef whose definition has been deferred from its place in the type list.
This occurs because the typedef involves an array of a struct whose
definition follows that of the typedef in the type list, so the typedef
cannot be emitted until the struct's definition has been emitted.
*/
typedef struct a_pending_typedef *a_pending_typedef_ptr;
typedef struct a_pending_typedef {
  a_pending_typedef_ptr
		next;	/* The next pending typedef to be processed, or NULL
			   for the end of the list. */
  a_type_ptr	pending_typedef;
			/* Points to the typedef whose definition has been
			   deferred. */
  a_type_ptr	type_to_be_completed;
			/* Points to the struct type that must be complete
			   before the pending typedef can be emitted. */
} a_pending_typedef;

/*
Head and tail of a singly-linked list of pending typedefs.
*/
STATIC_THREAD a_pending_typedef_ptr
		pending_typedefs;
STATIC_THREAD a_pending_typedef_ptr
		last_pending_typedef;
/*
Head of a list of pending typedefs that have been processed and can be
reused.
*/
STATIC_THREAD a_pending_typedef_ptr
		avail_pending_typedefs;

/*
Data structures used to create the prefix for the mangled name of a data
member that is "promoted" out of a base class or no_unique_address
subobject to become a direct member of the containing class.  (This is done
to permit use of tail padding in the class type, which is not possible if
the subobject is represented as a single struct member of the containing
class.)  The names of the promoted class members must be mangled in order
to prevent collisions with the names of containing class members.
Multi-level promotion, where members of a subobject of a subobject become
members of the containing class, is possible, so the member name prefix
components for the subobject types are kept on a doubly-linked list.
*/
typedef struct a_member_name_prefix_component
                                           *a_member_name_prefix_component_ptr;
typedef struct a_member_name_prefix_component {
  a_member_name_prefix_component_ptr
		next;	/* The component corresponding to the next (i.e.,
			   less-derived or more-nested) class type for the
			   current member. */
  a_member_name_prefix_component_ptr
		prev;	/* The component corresponding to the previous
			   (i.e., more-derived or less-nested) class type
			   for the current member. */
  a_field_ptr	field;	/* The field whose name will be used for the
			   current level in the mangled name. */
  a_targ_size_t	prev_subobject_offset;
			/* The offset within the most-derived or outermost
			   class of the previous component's subobject (to
			   allow saving and restoring the cumulative offset
			   when name prefix components are pushed and
			   popped). */
} a_member_name_prefix_component;

/*
Head and tail of a doubly-linked list of member name prefix components.
*/
STATIC_THREAD a_member_name_prefix_component_ptr
		name_prefix_components;
STATIC_THREAD a_member_name_prefix_component_ptr
		last_name_prefix_component;

/*
The offset within the most-derived class of the subobject associated with
the current member name prefix, or 0 if none.  (Used to adjust the offsets
displayed in layout annotations to be relative to the complete object.)
*/
STATIC_THREAD a_targ_size_t
                subobject_offset;

/*
The following data structures are used when the Microsoft compiler is the
generated code target to determine whether padding is needed following a
bit-field.  The Microsoft compiler allocates bit-fields within a container
of the declared type of the bit-field, so no padding should be inserted if
the next field or the end of the struct occurs at the end of the current
container, regardless of how much or little of the container is used by the
bit-fields.

Each time a bit-field is declared and starts a new container (because the
preceding field, if any, was not a bit-field, or because the declared type
is different from that of the preceding bit-field, or because the new
bit-field does not fit into the current container), the declared type of
the bit-field and the offset of the container are recorded, and the next
available allocation following the last bit-field in that container will be
simply the container offset plus the size of the container type.
*/
typedef struct a_microsoft_bit_field_tracker {
  a_type_ptr	container_type;
			/* Declared type of the previous bit-field in the
			   current sequence of bit-fields.  NULL if there
			   was no previous field or if the previous field
			   was not a bit-field. */
  a_targ_size_t	container_offset;
			/* The offset of the container (of type
			   container_type) within which bit-fields are
			   currently being allocated.  Only valid if
			   container_type is non-NULL. */
} a_microsoft_bit_field_tracker;

STATIC_THREAD a_microsoft_bit_field_tracker
		msvc_bit_field_tracker;
			/* Bit-field tracker for the struct currently being
			   declared. */


/* Declarations needed because of forward references: */
static void dump_constant(a_constant_ptr constant);
static void dump_cast(a_type_ptr type);
static void dump_declaration_using_type(a_type_ptr              type,
                                        a_source_correspondence *scp);
static void dump_general_declaration_using_type(
                                      a_type_ptr              type,
                                      a_source_correspondence *scp,
                                      a_variable_ptr          var,
                                      a_routine_ptr           rout,
                                      a_field_ptr             field,
                                      char                    *temp,
                                      a_const_char            *name,
                                      a_type_qualifier_set    added_qualifiers,
                                      a_boolean               suppress_const,
                                      uint32_t                counter);
static void dump_enum_definition(a_type_ptr type,
                                 a_boolean  output_final_semi);
static void dump_struct_union_definition(a_type_ptr type,
                                         a_boolean  output_final_semi);
static void dump_statement_list(a_statement_ptr statement);
static void dump_prescan_temps(a_statement_ptr statement);
static void set_up_prescan_traversal_block(
                                   an_expr_or_stmt_traversal_block_ptr tblock);
static void dump_statement(a_statement_ptr statement);
static void dump_block(a_statement_ptr statement);

static void dump_type(a_type_ptr type,
                      a_boolean  add_pointer_to);

static void dump_expr(an_expr_node_ptr expr,
                      a_boolean        need_parens);
/* Interfaces to dump_expr for the usual cases. */
#define dump_expr_with_parens(expr) dump_expr(expr, /*need_parens=*/TRUE)
#define dump_expression(expr)       dump_expr(expr, /*need_parens=*/FALSE)
static void
dump_boolean_controlling_expression(an_expr_node_ptr node,
                                    a_boolean        wrap_with_parens = TRUE);
static void dump_compound_literal(an_expr_node_ptr expr);
#if MICROSOFT_EXTENSIONS_ALLOWED
static void dump_asm_function_body(a_const_char *p);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static a_constant_ptr constant_initializer(a_variable_ptr variable,
                                           an_init_kind   *init_kind);

static a_boolean replace_call_to_master_routine(an_expr_node_ptr expr);

static void clear_output_file_position(an_output_file_position *ofp)
/*
Clear the fields of an output file position structure to indicate an unknown
position.
*/
{
  ofp->curr_output_file = NULL;
  ofp->curr_output_line = 0;
  ofp->curr_output_column = 0;
  ofp->curr_output_pos_known = FALSE;
}  /* clear_output_file_position */


static an_output_file_position_ptr assoc_output_file_position(FILE *file)
/*
Return a pointer to the output file position structure that is associated
with the indicated file.
*/
{
  an_output_file_position_ptr ofp = NULL;

  if (file == f_primary) {
    ofp = &primary_output_position;
#if !C_GEN_BE_GENERATES_ANSI_C
  } else if (file == f_file_scope_inits) {
    ofp = &file_scope_inits_output_position;
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  } else if (file == f_rout_dynamic_inits) {
    ofp = &rout_dynamic_inits_output_position;
  }  /* if */
  check_assertion_str(ofp != NULL,
                      "assoc_output_file_position: file not found");
  return ofp;
}  /* assoc_output_file_position */


static void save_output_position(an_output_file_position_ptr ofp)
/*
Save the current output position state (in global variables) to *ofp
later restoration.
*/
{
  ofp->curr_output_file = curr_output_file;
  ofp->curr_output_line = curr_output_line;
  ofp->curr_output_column = curr_output_column;
  ofp->curr_output_pos_known = curr_output_pos_known;
}  /* save_output_position */


static void restore_output_position(an_output_file_position_ptr ofp)
/*
Restore the current output position state (in global variables) from
the information saved in *ofp.
*/
{
  curr_output_file = ofp->curr_output_file;
  curr_output_line = ofp->curr_output_line;
  curr_output_column = ofp->curr_output_column;
  curr_output_pos_known = ofp->curr_output_pos_known;
}  /* restore_output_position */


static void redirect_output_file(FILE *new_file)
/*
Change f_C_output so it is connected to the indicated file.
*/
{
  check_assertion_str(!in_comment, "redirect_output_file: in_comment");
  if (f_C_output != NULL) {
    /* Save the current output position for the current file. */
    save_output_position(assoc_output_file_position(f_C_output));
  }  /* if */
  f_C_output = new_file;
  /* Restore the current output position for the new file. */
  restore_output_position(assoc_output_file_position(f_C_output));
}  /* redirect_output_file */


static void end_output_line(void)
/*
End the current line of output.
*/
{
  if (putc('\n', f_C_output) == EOF) {
    /* Error in writing the output file.  This check supplements the check
       done when the file is closed.  The check here helps catch a disk full
       error quickly. */
    file_write_error(ec_generated_c, errno);
  }  /* if */
  /* Keep track of the current position if we know where we are. */
  if (curr_output_pos_known) curr_output_line++;
  curr_output_column = 0;
}  /* end_output_line */


/*
End the current output line if it has been started.
*/
#define end_output_line_if_begun()                                    \
{ if (curr_output_column != 0) end_output_line(); }


static void write_tok_str(a_const_char *str);

static void write_line_directive(a_line_number     line_number,
                                 a_source_file_ptr new_output_file)
/*
Write a #line directive for the indicated line number and file.
*/
{
#if STANDALONE_UTILITY_PROGRAM
  /* This is a command-line option normally, but it's not available in the
     standalone version. */
  a_boolean gen_old_style_line_dirs = FALSE;
#endif /* STANDALONE_UTILITY_PROGRAM */
  char      buf[100] = "#line ";

  /* End the previous line if there is one. */
  end_output_line_if_begun();
  curr_output_line = line_number;
  curr_output_pos_known = TRUE;
  if (gen_old_style_line_dirs || gcc_or_clang_is_generated_code_target) {
    /* Generate old-style directives, i.e., the kind output by the Reiser
       cpp and by the GNU preprocessor. */
    buf[1] = ' ';
    (void)unsigned_to_string_buf((a_host_large_unsigned)curr_output_line,
                                 buf+2);
  } else {
    (void)unsigned_to_string_buf((a_host_large_unsigned)curr_output_line,
                                 buf+6);
  }  /* if */
  write_tok_str(buf);
  if (new_output_file != curr_output_file) {
    /* The file name is put out only if it changed. */
    a_boolean process_escapes = C_GEN_BE_GENERATES_ANSI_C;
    curr_output_file = new_output_file;
    /* Put out the file name, putting escapes on characters as necessary.
       Note that in ANSI/ISO C, escapes *are* recognized in the string
       on a #line directive; in pcc mode, we assume they are not. */
    if (gen_old_style_line_dirs) process_escapes = FALSE;
    (void)putc(' ', f_C_output);
    (void)putc('"', f_C_output);
    write_file_name(curr_output_file->file_name, f_C_output,
                    process_escapes, /*escape_nonprintable_chars=*/TRUE);
    (void)putc('"', f_C_output);
    if (gcc_or_clang_is_generated_code_target &&
        new_output_file->from_system_include_dir) {
      /* When generating code to be compiled by gcc, include the system header
         flag on the line directive if the source is from a system include. */
      (void)putc(' ', f_C_output);
      (void)putc('3', f_C_output);
    }  /* if */
  }  /* if */
  (void)putc('\n', f_C_output);
  curr_output_column = 0;
}  /* write_line_directive */


static void continue_on_new_line(void)
/*
Continue the current line of output on the next line.
*/
{
  if (in_comment) {
    /* End the current comment so we don't put out a #line inside
       a comment.  We'll restart the comment afterwards. */
    (void)fputs(" */", f_C_output);
  }  /* if */
  if (curr_output_pos_known) {
    /* Continue by emitting a #line directive to repeat the current line
       number. */
    write_line_directive(curr_output_line,
                         curr_output_file);
  } else {
    /* If the output position is unknown, put out a #line directive for the
       last "known good" position to avoid wandering into line numbers that
       don't exist in the source program file. */
    write_line_directive(last_known_good_line,
                         last_known_good_file);
  }  /* if */
  if (in_comment) {
    (void)fputs("/* ", f_C_output);
  }  /* if */
}  /* continue_on_new_line */


static void wrap_overlong_line(void)
/*
Continue the current line of output on the next line because it is too long.
If line wrapping is disabled, do nothing.
*/
{
  if (!line_wrapping_disabled) continue_on_new_line();
}  /* wrap_overlong_line */


/*
Print a number of spaces for indentation.
*/
#define do_indentation()					      \
{ int a;						              \
  for (a = 0; a < (int)indent; a++) {				      \
    (void)putc(' ', f_C_output);				      \
  }  /* for */							      \
}  /* do_indentation */


static void set_output_position(a_source_position *pos)
/*
Position the output file properly for output of something at the indicated
position.  This may mean beginning a new line, putting out a #line directive,
etc.
*/
{
  a_seq_number      seq = pos->seq;
  a_boolean         line_directive_needed = FALSE, started_new_line = FALSE;
  a_source_file_ptr new_output_file;

  /* Record the position for use in internal errors. */
  error_position = *pos;
  if (seq == 0) {
    /* For an unknown position, continue on the same line. */
    if (!curr_output_pos_known || annotate) {
      /* If the current output position is unknown, start a new line with
         a #line directive for the last known good line position.
         If annotating, start a new line for the same line number. */
      continue_on_new_line();
      started_new_line = TRUE;
    }  /* if */
  } else {
    a_line_number line_number;
    a_boolean     at_end_of_source;
    /* When generating debug-oriented output, put each thing on a separate
       line. */
    if (annotate) end_output_line_if_begun();
    /* Find the file in which this sequence number lies. */
    /* physical_line == FALSE means consider information from #line
       directives as well as true file information. */
    new_output_file = source_file_for_seq(seq, &line_number,
                                          &at_end_of_source,
                                          /*physical_line=*/FALSE);
    /* Don't put out line 0 for empty files. */
    if (at_end_of_source && line_number == 0) line_number = 1;
    if (new_output_file != curr_output_file ||
        !curr_output_pos_known) {
      /* We've gone into a new file, or the current position is unknown,
         so we need a #line directive. */
      line_directive_needed = TRUE;
    } else {
      /* We're still in the same file as last time.  See if we're close enough
         that we can advance there by spacing.  If not, use a #line
         directive. */
      if (curr_output_line > line_number) {
        /* We're already too far (we're backing up -- curious, but easy
           to handle). */
        line_directive_needed = TRUE;
      } else {
        /* We're going forward.  How far? */
        if (line_number > curr_output_line + 5) {
          /* More than 5 lines (arbitrary) -- use a #line directive. */
          line_directive_needed = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
    if (line_directive_needed) {
      /* Write a #line directive for the new line position. */
      check_assertion(new_output_file != NULL);
      write_line_directive(line_number, new_output_file);
      started_new_line = TRUE;
    } else {
      check_assertion(line_number >= curr_output_line);
      while (line_number > curr_output_line) {
        /* Write blank lines until we get to the right line. */
        end_output_line();
        started_new_line = TRUE;
      }  /* while */
    }  /* if */
    /* Remember the position as a "known good" output position. */
    last_known_good_line = curr_output_line;
    last_known_good_file = curr_output_file;
  }  /* if */
  if (started_new_line || curr_output_column == 0) {
    if (annotate) {
      /* Starting a new line of output; do the current indentation. */
      do_indentation();
      curr_output_column += indent;
    }  /* if */
  } else {
    /* Continuing on the same line.  Space. */
    (void)putc(' ', f_C_output);
    curr_output_column++;
  }  /* if */
}  /* set_output_position */


static void set_unknown_output_position(void)
/*
Make the current output position unknown, which forces a #line directive
the next time a specific output position is requested.
*/
{
  end_output_line_if_begun();
  curr_output_pos_known = FALSE;
  curr_output_line = 0;
  curr_output_file = NULL;
  /* Set the position for errors to "unknown". */
  set_position_to(error_position, 0, SP_COL_UNKNOWN);
}  /* set_unknown_output_position */


/*
Write the indicated character to the output file.  It is not necessarily a
complete token.  This is the macro version.
*/
#define m_write_ch(ch)                                                \
{ (void)putc((ch), f_C_output);                                       \
  curr_output_column++;                                               \
}  /* m_write_ch */


static void write_ch(char ch)
/*
Write the indicated character to the output file.  It is not necessarily a
complete token.  This is the non-macro version.
*/
{
  m_write_ch(ch);
}  /* write_ch */


/*
Write a space to the output file.
*/
#define write_space() write_ch(' ')
#define m_write_space() m_write_ch(' ')


/*
Write the indicated string to the output file.  It is not necessarily a
complete token.  This is the macro version.
*/
#define m_write_str(str)                      \
{ a_const_char *p = (str);                    \
  char         ch;                            \
  while ((ch = *p++) != '\0') m_write_ch(ch); \
}  /* m_write_str */


static void write_str(a_const_char *str)
/*
Write the indicated string to the output file.  It is not necessarily a
complete token.  This is the non-macro version.
*/
{
  m_write_str(str);
}  /* write_str */


static void write_str_octl(
                   a_const_char                                     *str,
                   ARG_UNUSED an_il_to_str_output_control_block_ptr local_octl)
/*
Version of write_str intended to be called by the il-to-str routines.
*/
{
  m_write_str(str);
}  /* write_str_octl */


/*
Write the indicated character to the output file.  It's a complete token,
which means a long line could be broken before or after it.  This is
the macro version.
*/
#define m_write_tok_ch(ch)                                            \
{ if (curr_output_column >= MAX_OUTPUT_LINE_SIZE) {                   \
    wrap_overlong_line();                                             \
  }  /* if */                                                         \
  m_write_ch(ch);                                                     \
}  /* m_write_tok_ch */


static void write_tok_ch(char ch)
/*
Write the indicated character to the output file.  It's a complete token,
which means a long line could be broken before or after it.  This is
the non-macro version.
*/
{
  m_write_tok_ch(ch);
}  /* write_tok_ch */


/*
Start a continuation line if adding "len" characters to the current output
line would make it too long.
*/
#define ensure_enough_room_on_line(len)                               \
{ if (curr_output_column + (len) > MAX_OUTPUT_LINE_SIZE) {            \
    wrap_overlong_line();                                             \
  }  /* if */                                                         \
}  /* ensure_enough_room_on_line */


/*
Write the indicated string to the output file.  It's a complete token (or
several), which means a long line could be broken before or after it.
This is the macro version.
*/
#define m_write_tok_str(str)                                          \
{ a_const_char *p = (str);                                            \
  sizeof_t              len = (sizeof_t)strlen(p);                    \
  char         ch;                                                    \
  ensure_enough_room_on_line(len);                                    \
  while ((ch = *p++) != '\0') (void)putc(ch, f_C_output);             \
  curr_output_column += (uint32_t)len;                                \
}  /* m_write_tok_str */


static void write_tok_str(a_const_char *str)
/*
Write the indicated string to the output file.  It's a complete token (or
several), which means a long line could be broken before or after it.
This is the non-macro version.
*/
{
  m_write_tok_str(str);
}  /* write_tok_str */


static void write_tok_str_octl(
                   a_const_char                                     *str,
                   ARG_UNUSED an_il_to_str_output_control_block_ptr local_octl)
/*
Version of write_tok_str intended to be called by the il-to-str routines.
*/
{
  m_write_tok_str(str);
}  /* write_tok_str_octl */


static void write_unsigned_num(a_host_large_unsigned num)
/*
Write the indicated unsigned number to the output file.  The number is assumed
to be a complete token.
*/
{
  char         digitch;
  unsigned int digit;

  /* Do smaller numbers in a fast way. */
  if (num <= 9) {
    ensure_enough_room_on_line(1);
    goto digit1;
  }  /* if */
  if (num <= 99) {
    ensure_enough_room_on_line(2);
    goto digit2;
  }  /* if */
  if (num <= 999) {
    ensure_enough_room_on_line(3);
    goto digit3;
  }  /* if */
  if (num <= 9999) {
    ensure_enough_room_on_line(4);
    goto digit4;
  }  /* if */
  if (num <= 99999) {
    ensure_enough_room_on_line(5);
    goto digit5;
  }  /* if */
  /* General case: */
  { char buffer[50];
    (void)unsigned_to_string_buf(num, buffer);
    m_write_tok_str(buffer);
  }
  goto done;
digit5:
  digit = (unsigned int)(num/10000);
  digitch = (char)(digit + '0');
  m_write_ch(digitch);
  num = num - digit*10000; /*lint !e647*/
digit4:
  digit = (unsigned int)(num/1000);
  digitch = (char)(digit + '0');
  m_write_ch(digitch);
  num = num - digit*1000; /*lint !e647*/
digit3:
  digit = (unsigned int)(num/100);
  digitch = (char)(digit + '0');
  m_write_ch(digitch);
  num = num - digit*100; /*lint !e647*/
digit2:
  digit = (unsigned int)(num/10);
  digitch = (char)(digit + '0');
  m_write_ch(digitch);
  num = num - digit*10; /*lint !e647*/
digit1:
  digitch = (char)(num + '0');
  m_write_ch(digitch);
done:;
}  /* write_unsigned_num */

#if GNU_VECTOR_TYPES_ALLOWED

static void write_array_index(a_host_large_unsigned num)
/*
Write the indicated unsigned number as an array index (i.e., "[num]").
*/
{
  write_tok_ch('[');
  write_unsigned_num(num);
  write_tok_ch(']');
}  /* write_array_index */


static void write_vector_constant(a_type_ptr   type,
                                  a_const_char *str)
/*
Write out a compound literal vector constant of the specified vector type
whose elements all have the specified string (presumably a numeric constant)
as their value.
*/
{
  a_targ_size_t i;

  check_assertion(gcc_is_generated_code_target && is_vector_type(type));
  write_tok_ch('(');
  dump_type(type, /*add_pointer_to=*/FALSE);
  write_tok_str("){");
  for (i = num_vector_elements(type); i != 0; i--) {
    write_tok_str(str);
    if (i != 1) {
      write_tok_ch(',');
    }  /* if */
  }  /* for */
  write_tok_ch('}');
}  /* write_vector_constant */

#endif /* GNU_VECTOR_TYPES_ALLOWED */

static void write_pp_directive(a_const_char *directive,
                               a_const_char *more)
/*
Write an output line that is a preprocessing directive.  directive is the
string for the directive.  If more is non-NULL, the string it points to
is added at the end of the directive.
*/
{
  uint32_t saved_indent = indent;

  end_output_line_if_begun();
  indent = 0;
  disable_line_wrapping();
  write_str(directive);
  if (more != NULL) write_str(more);
  enable_line_wrapping();
  end_output_line();
  indent = saved_indent;
}  /* write_pp_directive */


static void write_if_0_directive(void)
/*
Write an output line that is a #if 0 directive (to comment out unreferenced
code when doing annotations, presumably).
*/
{
  write_pp_directive("#if 0", (char *)NULL);
  /* Avoid generating a #line for a line number that doesn't exist. */
  set_unknown_output_position();
}  /* write_if_0_directive */


static void write_endif_0_directive(void)
/*
Write an output line that is a #endif directive matching a #if 0 previously
written (to comment out unreferenced code when doing annotations, presumably).
*/
{
  write_pp_directive("#endif", (char *)NULL);
  /* Force a #line directive after the #endif, because #line directives
     inside the #if might change the position. */
  set_unknown_output_position();
}  /* write_endif_0_directive */


/*
Start a comment, unless we're already inside one.
*/
#define start_comment() if (!in_comment++) write_str("/*")


/*
End a comment, for real if we're at the outermost level.
*/
#define end_comment() if (!--in_comment) write_str("*/")


static a_boolean start_unreferenced_bracket(
                                      a_source_correspondence *source_corresp,
                                      a_boolean               *is_annotation_p)
/*
Return TRUE if the code for the entity with the given source correspondence
information should be put out, i.e., if it is needed in the generated code
or if we are annotating the code with unneeded declarations inside #if 0
blocks.  If is_annotation_p is non-NULL, set *is_annotation_p to TRUE if
the entity is not needed but should be put out as an annotation anyway.
*/
{
  a_boolean output_code_for_entity;

  if (is_annotation_p != NULL) {
    *is_annotation_p = FALSE;
  }  /* if */
  output_code_for_entity = scp_is_needed_in_generated_code(source_corresp);
  if (!output_code_for_entity) {
    if (annotate) {
      write_if_0_directive();
      output_code_for_entity = TRUE;
      if (is_annotation_p != NULL) {
        *is_annotation_p = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return output_code_for_entity;
}  /* start_unreferenced_bracket */


static void end_unreferenced_bracket(a_source_correspondence *source_corresp)
/*
If the corresponding call of start_unreferenced_bracket started a #if,
end it here.
*/
{
  if (annotate) {
    if (!scp_is_needed_in_generated_code(source_corresp)) {
      write_endif_0_directive();
    }  /* if */
  }  /* if */
}  /* end_unreferenced_bracket */


static a_boolean is_C_reserved_word(a_const_char *name)
/*
Return TRUE if "name" is a C reserved word.
*/
{
  a_boolean res = FALSE;

  switch (*name) {
    case 'a':
      if (strcmp(name, "auto") == 0 ||
          strcmp(name, "asm") == 0) res = TRUE;
      break;
    case 'b':
      if (strcmp(name, "break") == 0) res = TRUE;
      break;
    case 'c':
      if (strcmp(name, "case") == 0 ||
          strcmp(name, "char") == 0 ||
          strcmp(name, "const") == 0 ||
          strcmp(name, "continue") == 0) res = TRUE;
      break;
    case 'd':
      if (strcmp(name, "default") == 0 ||
          strcmp(name, "do") == 0 ||
          strcmp(name, "double") == 0) res = TRUE;
      break;
    case 'e':
      if (strcmp(name, "else") == 0 ||
          strcmp(name, "enum") == 0 ||
          strcmp(name, "extern") == 0) res = TRUE;
      break;
    case 'f':
      if (strcmp(name, "float") == 0 ||
          strcmp(name, "for") == 0 ||
          strcmp(name, "fortran") == 0) res = TRUE;
      break;
    case 'g':
      if (strcmp(name, "goto") == 0) res = TRUE;
      break;
    case 'i':
      if (strcmp(name, "if") == 0 ||
          strcmp(name, "inline") == 0 ||
          strcmp(name, "int") == 0) res = TRUE;
      break;
    case 'l':
      if (strcmp(name, "long") == 0) res = TRUE;
      break;
    case 'p':
      if (strcmp(name, "pascal") == 0) res = TRUE;
      break;
    case 'r':
      if (strcmp(name, "register") == 0 ||
          strcmp(name, "restrict") == 0 ||
          strcmp(name, "return") == 0) res = TRUE;
      break;
    case 's':
      if (strcmp(name, "short") == 0 ||
          strcmp(name, "signed") == 0 ||
          strcmp(name, "sizeof") == 0 ||
          strcmp(name, "static") == 0 ||
          strcmp(name, "struct") == 0 ||
          strcmp(name, "switch") == 0) res = TRUE;
      break;
    case 't':
      if (strcmp(name, "typedef") == 0) res = TRUE;
      break;
    case 'u':
      if (strcmp(name, "union") == 0 ||
          strcmp(name, "unix") == 0 ||
          strcmp(name, "unsigned") == 0) res = TRUE;
      break;
    case 'v':
      if (strcmp(name, "void") == 0 ||
          strcmp(name, "volatile") == 0) res = TRUE;
      break;
    case 'w':
      if (strcmp(name, "while") == 0) res = TRUE;
      break;
    case '_':
      if ((name[1] == 'A' && strcmp(name, "_Atomic") == 0) ||
          (name[1] == 'A' && strcmp(name, "_Alignof") == 0) ||
          (name[1] == 'A' && strcmp(name, "_Alignas") == 0) ||
          (name[1] == 'B' && strcmp(name, "_Bool") == 0) ||
          (name[1] == 'C' && strcmp(name, "_Complex") == 0) ||
          (name[1] == 'G' && strcmp(name, "_Generic") == 0) ||
          (name[1] == 'I' && strcmp(name, "_Imaginary") == 0) ||
          (name[1] == 'N' && strcmp(name, "_Noreturn") == 0) ||
          (name[1] == 'S' && strcmp(name, "_Static_assert") == 0) ||
          (name[1] == 'T' && strcmp(name, "_Thread_local") == 0)) {
        res = TRUE;
      } else if (sun_is_generated_code_target) {
        if (strcmp(name, "__global"  ) == 0 ||
            strcmp(name, "__symbolic") == 0 ||
            strcmp(name, "__hidden"  ) == 0) {
          res = TRUE;
        }  /* if */
      }  /* if */
      break;
    default:;
  }  /* switch */
  return res;
}  /* is_C_reserved_word */


static void dump_temp_name(char *ptr)
/*
Write a temporary name generated from the given IL pointer as a separate
token.
*/
{
  a_temp_var_name_buffer buffer = form_temporary_name_for_back_end((void*)ptr);

  m_write_tok_str(buffer.as_temp_characters());
}  /* dump_temp_name */


static void add_temp_name(char *ptr)
/*
Write a temporary name generated from the given IL pointer as part of a
longer string (i.e., not as a separate token).
*/
{
  a_temp_var_name_buffer buffer = form_temporary_name_for_back_end((void*)ptr);

  m_write_str(buffer.as_temp_characters());
}  /* add_temp_name */


static void dump_bare_name(a_source_correspondence *scp)
/*
Print the name of an entity.  This is used when the simple name
used in the source correspondence is used with no added prefixes.
If the token was named using a Microsoft __identifier operator, that
is reconstructed here.
*/
{
#if MICROSOFT_EXTENSIONS_ALLOWED
  /* Check for identifiers named with the Microsoft __identifier operator. */
  if (scp->microsoft_identifier_used) {
    write_tok_str("__identifier(");
    write_tok_str(scp->name);
    write_tok_str(")");
  } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Do not insert code here. */
  {
    m_write_tok_str(scp->name);
  }
}  /* dump_bare_name */


static void dump_name_full(a_source_correspondence *scp,
                           uint32_t                counter)
/*
Print the name of an entity.  scp is the source correspondence.  If the
entity is unnamed, generate a name.  If counter is non-zero and we are not
outputting just a bare name, counter is used in addition to the declaration
position to ensure that the name is unique.
*/
{
  a_const_char *name = scp->name;

  if (name == NULL ||
      (scp->copied_from_secondary_trans_unit &&
       (scp->name_linkage == (a_name_linkage_kind)nlk_internal ||
        (scp->name_linkage == (a_name_linkage_kind)nlk_none &&
         !scp->is_local_to_function && !scp->is_class_member))) ||
      (scp->same_name_as_external_entity_in_secondary_trans_unit &&
       !scp->is_class_member)) {
    /* For entities without names, create a name. */
    /* For non-external entities copied from a secondary translation unit,
       use a temporary name for the entity to avoid name conflicts with
       like-named entities in the primary translation unit.  Tag names
       in C mode and typedef names in C or C++ mode may conflict even
       though they have no linkage.  Similarly, external entities copied
       from a secondary translation unit may conflict with non-external
       entities in the primary translation unit.  Do not change names of
       fields in any case. */
    dump_temp_name((char *)scp);
  } else if (scp->name_linkage == (a_name_linkage_kind)nlk_internal ||
             scp->name_linkage == (a_name_linkage_kind)nlk_external) {
    /* Externally or internally-linked name. */
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* Check for identifiers named with the Microsoft __identifier
       operator. */
    if (scp->microsoft_identifier_used) {
      dump_bare_name(scp);
    } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* Do not insert code here. */
    /* Avoid problems with C reserved identifiers (and other identifiers
       likely to mean something to the underlying C compiler). */
    if (is_C_reserved_word(name)) {
      /* Add two underscores and an "x" at the start of the name. */
      ensure_enough_room_on_line(strlen(name)+3);
      write_ch('_');
      write_ch('_');
      write_ch('x');
      write_str(name);
    } else {
      m_write_tok_str(name);
    }  /* if */
  } else if (scp->is_class_member || !scp->is_local_to_function ||
             scp->name_has_been_mangled) {
    /* No prefix on members of classes or things that aren't local to
       functions (e.g., file-scope typedefs).  Also no prefix if the
       name has been mangled already. */
    dump_bare_name(scp);
  } else {
    /* Name has no linkage; add the declaration position as a prefix to
       the original name, e.g., "i" becomes "__16_12_i". */
    ensure_enough_room_on_line(strlen(name)+14+(counter != 0 ? 3 : 0));
    m_write_ch('_');
    m_write_ch('_');
    if (counter != 0) {
      write_unsigned_num((a_host_large_unsigned)counter);
      m_write_ch('_');
    }  /* if */
    write_unsigned_num((a_host_large_unsigned)scp->decl_position.seq);
    m_write_ch('_');
    write_unsigned_num((a_host_large_unsigned)scp->decl_position.column);
    m_write_ch('_');
    m_write_str(name);
  }  /* if */
}  /* dump_name_full */

/*
Interface macro to dump_name_full that supplies a default value for
the counter parameter.
*/
#define dump_name(scp) dump_name_full((scp), 0)


/*
Macro that tests for variable names that are special.  They don't get
subjected to the "fake static" transformation.  They also get put out even
if unreferenced.
*/
#define is_magic_name(name)                                           \
   (name[0] == '_' /* for speed */ &&                                 \
    (strcmp((name), "__link") == 0 ||                                 \
     (sun_is_generated_code_target &&                                 \
      strcmp((name), "__builtin_va_alist") == 0)))


static void dump_variable_name(a_variable_ptr variable)
/*
Print the name of the indicated variable.
*/
{
  if (entry_routine_scope != NULL &&
      innermost_function_scope == entry_routine_scope &&
      variable->is_parameter && !variable->is_this_parameter) {
    /* While putting out the parameters of a wrapper routine for a
       virtual function with a covariant return type, use the parameter
       names from the original routine instead of the unnamed parameters
       of the wrapper, because when the body of the original function
       is duplicated in the wrapper it will contain references to the
       parameters by their original names. */
    a_variable_ptr master_param_var, wrapper_param_var;
    check_assertion(master_routine_scope != NULL);
    master_param_var = master_routine_scope->variant.routine.parameters;
    wrapper_param_var = entry_routine_scope->variant.routine.parameters;
    for (;;) {
      check_assertion(wrapper_param_var != NULL && master_param_var != NULL);
      if (master_param_var->is_this_parameter) {
        /* Two special cases arise when mapping wrapper parameters to
           master parameters. */
        if (num_master_params_added > 0) {
          /* The master routine has extra parameters following the "this"
             parameter.  Advance over them. */
          int n;
          for (n = 1; n <= num_master_params_added; n++) {
            master_param_var = master_param_var->next;
            check_assertion(master_param_var != NULL);
          }  /* for */
        } else if (skip_this_parameter) {
          /* The master routine has a "this" parameter and the wrapper routine
             doesn't; skip the "this" parameter.  Used in cases where the
             master routine is a lambda call operator and the wrapper routine
             is a special static member function. */
          master_param_var = master_param_var->next;
          check_assertion(master_param_var != NULL);
        }  /* if */
      }  /* if */
      if (wrapper_param_var == variable) {
        /* We've found the correct location on the wrapper list; substitute
           the corresponding master parameter. */
        check_assertion(master_param_var != NULL);
        variable = master_param_var;
        break;
      }  /* if */
      wrapper_param_var = wrapper_param_var->next;
      master_param_var = master_param_var->next;
    }  /* for */
  }  /* if */
  /* Any references to bindings for structured variables should have been
     removed during lowering. */
  check_assertion(variable->init_kind != (an_init_kind)initk_binding);
  if (variable->is_this_parameter) {
    /* "this" parameter in C++. */
    m_write_tok_str("this");
#if !C_GEN_BE_GENERATES_ANSI_C
  } else if (variable->source_corresp.name_linkage ==
                                           (a_name_linkage_kind)nlk_internal &&
             !is_magic_name(variable->source_corresp.name)) {
    uint32_t len = (uint32_t)strlen(variable->source_corresp.name) + 9 +
                             strlen(module_id);
#if ONE_INSTANTIATION_PER_OBJECT
    Small_string<50> buffer;
    if (needed_flag_bit_number != 0) {
      /* Add a suffix identifying the instantiation number to make this
         name distinct from the same static in another instantiation
         object file. */
      buffer.reset_to("_", needed_flag_bit_number);
      len += buffer.length();
    }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
    
    /* Name is at file scope, but is not external.  Add a prefix/suffix so
       that it will not conflict with external names.  See dump_variable_decl.
       Leave some special names alone. */
    ensure_enough_room_on_line(len);
    m_write_str("__STV__");
    m_write_str(variable->source_corresp.name);
    m_write_ch('_');
    m_write_ch('_');
    m_write_str(module_id);
#if ONE_INSTANTIATION_PER_OBJECT
    if (needed_flag_bit_number != 0) write_str(buffer.as_temp_characters());
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  } else if (variable->is_pack_element) {
    uint32_t		counter = 0;
    a_variable_ptr	vp;
    /* Compute a counter that can be used to distinguish the variadic
       parameter names based on the number of remaining pack elements. */
    for (vp = variable; vp != NULL && vp->is_pack_element;
         vp = vp->next, counter++) {}
    dump_name_full(&variable->source_corresp, counter);
  } else {
    /* Nothing special about this case. */
    dump_name(&variable->source_corresp);
  }  /* if */
}  /* dump_variable_name */


static void dump_type_name(a_type_ptr type)
/*
Output the name of the indicated type.
*/
{
  if (type->is_builtin_va_list) {
    /* Don't let va_list copied from a secondary translation unit be
       given a generated name. */
    type->source_corresp.name_linkage = (a_name_linkage_kind)nlk_external;
    if (gcc_builtin_varargs_in_generated_code) {
      /* Use the intrinsic GNU C/C++ type __builtin_va_list.  No "std::"
         qualifier should be used. */
      write_tok_str("__builtin_va_list");
      goto done;
    } else {
      /* Make its name "va_list" if it was mangled in C++ because it's
         std::va_list.  In the generated code we're including <stdarg.h> and
         we have to refer to va_list. */
      type->source_corresp.name = "va_list";
    }  /* if */
  }  /* if */
  dump_name(&type->source_corresp);
done:;
}  /* dump_type_name */


static void dump_field_name_with_prefix(a_const_char *field_name,
                                        a_field_ptr  field)
/*
Print the supplied field name, prefixed by the current set of member name
prefix components.  field_name may be NULL, in which case a temporary
name generated from the field pointer will be used.
*/
{
  a_member_name_prefix_component_ptr pfxp;
  sizeof_t                           name_len;

  check_assertion(field_name != NULL || field != NULL);
  /* Calculate the length of the name (we assume a generated temporary name
     will be no more than 32 characters long). */
  name_len = field_name != NULL ? (sizeof_t)strlen(field_name) : 32;
  for (pfxp = name_prefix_components; pfxp != NULL; pfxp = pfxp->next) {
    name_len += (sizeof_t)strlen(pfxp->field->source_corresp.name) + 1;
    if (pfxp->field->has_no_unique_address_attribute) {
      /* We will add "__" to the promoted field name to avoid possible
         collisions with names in the containing struct. */
      name_len += 2;
    }  /* if */
  }  /* for */
  if (field != NULL && field->is_captured_pack_element) {
    /* Allow for "__" plus numbering for up to 999,999 pack elements. */
    name_len += 8;
  }  /* if */
  ensure_enough_room_on_line(name_len);
  /* Dump the component prefixes, followed by the field name. */
  for (pfxp = name_prefix_components; pfxp != NULL; pfxp = pfxp->next) {
    if (pfxp->field->has_no_unique_address_attribute) {
      /* Avoid collisions of promoted names with names in the containing
         struct. */
      write_str("__");
    }  /* if */
    write_str(pfxp->field->source_corresp.name);
    write_ch('_');
  }  /* for */
  if (field_name != NULL) {
    write_str(field_name);
    if (field != NULL && field->is_captured_pack_element) {
      /* Suffix the name with a number to distinguish the captured pack
         elements.  The numbering runs from N to 1, where N is the number
         of elements in the pack. */
      a_host_large_unsigned count = 1;
      a_field_ptr           fp;
      for (fp = field->next; fp != NULL && fp->is_captured_pack_element;
           fp = fp->next) {
        count += 1;
      }  /* for */
      write_str("__");
      write_unsigned_num(count);
    }  /* if */
  } else if (name_prefix_components != NULL) {
    /* Add a temporary name as the last component of the name. */
    add_temp_name((char *)field);
  } else {
    /* Put out a temporary name as a separate token. */
    dump_temp_name((char *)field);
  }  /* if */
}  /* dump_field_name_with_prefix */

#define dump_field_name(field) \
  dump_field_name_with_prefix((field)->source_corresp.name, field)


/* Interface routines to dump_name. */
#define dump_constant_name(constant) dump_name(&(constant)->source_corresp)


static void dump_routine_name(a_routine_ptr rout)
/*
Print the name of the indicated routine, unless it is a superseded external,
in which case a temporary name is printed.
*/
{
  if (rout->superseded_external) {
    /* A superseded external represents a declaration (block extern or
       implicit) of a function with a different type from that of the
       "official" declaration.  Because versions of gcc beginning with 3.4
       do not allow directly calling a function through a cast to a
       different function type, this situation is handled by transforming
       the declaration of the superseded function into a declaration of a
       function pointer with a temporary name, initialized to point to the
       "official" function.  Uses of the superseded function are changed
       to refer to the function pointer instead of the function name, thus
       using the superseded type instead of the "official" one. */
    dump_temp_name((char *)rout);
  } else {
    dump_name(&rout->source_corresp);
  }  /* if */
}  /* dump_routine_name */


static void dump_label_name(a_label_ptr label)
/*
Print the name of the indicated label.
*/
{
#if C_GEN_BE_GENERATES_ANSI_C
  dump_name(&label->source_corresp);
#else /* !C_GEN_BE_GENERATES_ANSI_C */
  /* K&R/pcc compilers do not provide a separate name space for labels,
     so add a disambiguating prefix. */
  a_const_char *name = label->source_corresp.name;
  if (name == NULL) {
    /* Generated labels are already unambiguous. */
    dump_name(&label->source_corresp);
  } else {
    ensure_enough_room_on_line(strlen(name)+4);
    write_str("__L_");
    write_str(name);
  }  /* if */
#endif /* C_GEN_BE_GENERATES_ANSI_C */
}  /* dump_label_name */

#if C_GEN_BE_GENERATES_ANSI_C

static a_boolean ttt_has_prototype_scope(a_type_ptr  type_ptr,
                                         a_boolean   *force_end_of_traversal)
/*
This is a service function designed to be called from traverse_type_tree
(whence the ttt_ prefix).  It returns TRUE if type_ptr is a type that
contains a prototype scope.  Since a prototype scope is created only if
something is declared in it, this means the type contains a type defined
within a prototype scope; that can happen only in C.
*/
{
  a_boolean contains_proto_scope_type = FALSE;

  if (type_ptr->kind == (a_type_kind)tk_routine) {
    if (type_ptr->variant.routine.extra_info->prototype_scope != NULL) {
      contains_proto_scope_type = TRUE;
      *force_end_of_traversal = TRUE;
    }  /* if */
  }  /* if */
  return contains_proto_scope_type;
}  /* ttt_has_prototype_scope */


static a_boolean type_contains_prototype_scope_type(a_type_ptr type)
/*
Return TRUE if the indicated type contains a type defined in a prototype
scope.
*/
{
  a_boolean contains_proto_scope_type = FALSE;

  if (traverse_type_tree(type, ttt_has_prototype_scope,
                         TTT_RETURN_TYPE | TTT_PARAM_TYPES |
                         TTT_SKIP_TYPEREFS)) {
    contains_proto_scope_type = TRUE;
  }  /* if */
  return contains_proto_scope_type;
}  /* type_contains_prototype_scope_type */

#endif /* C_GEN_BE_GENERATES_ANSI_C */

static void dump_constant(a_constant_ptr constant)
/*
Output the indicated constant.
*/
{
#if C_GEN_BE_GENERATES_ANSI_C
  if (il_header.source_language == sl_C &&
      constant->type != NULL &&
      is_pointer_type(constant->type) &&
      type_contains_prototype_scope_type(constant->type)) {
    /* When generating ANSI C, types defined in prototype scopes are kept.
       Suppress casts of constants to types containing such types,
       because they can't be written (the types defined in prototype scopes
       cannot be named elsewhere).  The cast must have been implicit
       in the original program.  Types cannot be defined in prototype scopes
       in C++, so there's no need to check in C++ mode.  When generating
       K&R C, all function declarators that involve a prototype scope are
       put out as unprototyped if the prototype scope has not been examined
       to promote out types defined therein, so a cast to such a type is
       always writable. */
    write_tok_ch('0');
  } else
#endif /* C_GEN_BE_GENERATES_ANSI_C */
  {
    /* Check that all appropriate constants have been lowered. */
    check_assertion(!(constant->kind == (a_constant_repr_kind)ck_address &&
                      constant->variant.address.kind ==
                                         (an_address_base_kind)abk_temporary));
    check_assertion(constant->kind != (a_constant_repr_kind)ck_ptr_to_member);
#if LOWER_COMPLEX
    check_assertion(constant->kind != (a_constant_repr_kind)ck_complex &&
                    constant->kind != (a_constant_repr_kind)ck_imaginary);
#endif /* LOWER_COMPLEX */
    form_constant(constant, /*need_parens=*/TRUE, &octl);
  }  /* if */
}  /* dump_constant */


static void dump_storage_class(a_storage_class storage_class)
/*
Print the storage class and a space.  If there is no printable storage class,
omit the space.
*/
{
  a_const_char *str = NULL;

  switch (storage_class) {
    case sc_extern:
      str = "extern";
      break;
    case sc_static:
      str = "static";
      break;
    case sc_auto:
      str = "auto";
      break;
    case sc_unspecified:
      /* Print nothing. */
      goto done;
    case sc_register:
      str = "register";
      break;
    case sc_typedef:
      str = "typedef";
      break;
#if ASM_FUNCTION_ALLOWED
    case sc_asm:
      str = "__asm";
      break;
#endif /* ASM_FUNCTION_ALLOWED */
    default:
      unexpected_condition_str("dump_storage_class: bad storage class");
  }  /* switch */
  write_tok_str(str);
  write_space();
done:;
}  /* dump_storage_class */


static void dump_variable_storage_class(a_variable_ptr variable)
/*
Print the storage class of the indicated variable followed by a space.
*/
{
  a_boolean  suppress_register = FALSE;
  /* If the variable has an aggregate or union type, suppress the
     "register" storage class so that we can take the address of the
     variable if necessary to zero it or copy it for an eok_bassign.
     "register" on an aggregate probably doesn't do much anyway, and
     might even confuse the underlying C compiler. */
  if (variable->storage_class == (a_storage_class)sc_register) {
    if (is_aggregate_or_union_type(variable->type)) {
      suppress_register = TRUE;
#if !ALLOW_ADDR_OF_REGISTER_IN_GENERATED_C
    } else if (variable->address_taken) {
      /* In SVR4 C compatibility mode, the address of a register variable
         can be taken.  If the underlying C compiler cannot handle this
         construct, suppress the register storage class for this variable. */
      suppress_register = TRUE;
#endif /* !ALLOW_ADDR_OF_REGISTER_IN_GENERATED_C */
    }  /* if */
  }  /* if */
  if (suppress_register) {
    if (annotate) {
      start_comment();
      write_tok_str("register");
      end_comment();
      write_space();
    }  /* if */
#if NAMED_REGISTERS_ALLOWED
  } else if (variable->has_named_register_storage_class) {
    write_tok_str("register ");
    write_tok_str(
           named_register_storage_classes[variable->asm_name_or_reg.id].name);
    write_space();
#endif /* NAMED_REGISTERS_ALLOWED */
  } else {
    /* Normal case. */
    dump_storage_class(variable->storage_class);
  }  /* if */
}  /* dump_variable_storage_class */


static a_const_char *tag_kind(a_type_kind kind)
/*
Return a string that describes the tag kind for the indicated type, i.e.,
"class" or "enum".
*/
{
  a_const_char *str = NULL;

  switch (kind) {
    case tk_enum:   str = "enum";   break;
    case tk_struct: str = "struct"; break;
    case tk_union:  str = "union";  break;
    default:        unexpected_condition_str("tag_kind: bad type kind");
  }  /* switch */
  return str;
}  /* tag_kind */


static void dump_tag_reference(a_type_ptr type)
/*
Generate a reference to the indicated type, which is a class, struct, union,
or enum.  This is always a reference/declaration, never a definition.
*/
{
#if C_GEN_BE_GENERATES_ANSI_C
  /* When generating ANSI C, a struct/union/enum defined in a function
     prototype gets put out in place (if it has not been promoted out
     of the prototype scope). */
  if (type->declared_in_function_prototype && !is_incomplete_type(type)
#if MAINTAIN_NEEDED_FLAGS
      && ((type->kind == (a_type_kind)tk_enum) ||
          class_definition_needed_flag_is_set(type))
#endif /* MAINTAIN_NEEDED_FLAGS */
                                                    ) {
    if (type->kind == (a_type_kind)tk_enum) {
      dump_enum_definition(type, /*output_final_semi=*/FALSE);
    } else {
      dump_struct_union_definition(type, /*output_final_semi=*/FALSE);
    }  /* if */
  } else
#endif /* C_GEN_BE_GENERATES_ANSI_C */
  /* Do not insert code here. */
  if (gcc_or_clang_is_generated_code_target && is_immediate_class_type(type) &&
      !target_is_32_bit_x86_based() && class_type_supp(type)->is_va_list_tag) {
    /* The predefined struct __va_list_tag or struct __va_list is necessarily
       distinct from, and hence not compatible with, the type used internally
       by gcc as the base of __builtin_va_list.  Use the typedef that was
       defined in dump_type_decl in its place. */
    if (target_is_x86_based()) {
      write_tok_str("__va_list_tag_type");
    } else {
      write_tok_str("__va_list_type");
    }  /* if */
  } else {
    /* Put out a reference to the tag by name.  Note that unnamed tags will
       have been given compiler-generated names so they can be referred to. */
    write_tok_str(tag_kind(type->kind));
    write_space();
    dump_type_name(type);
  }  /* if */
}  /* dump_tag_reference */


static void gen_name_reference(char             *entry,
                               an_il_entry_kind kind)
/*
Routine to be called by the il_to_str routines to output a name.
*/
{
  switch (kind) {
    case iek_type:
      { a_type_ptr type = (a_type_ptr)entry;
        if (is_immediate_class_type(type) || is_immediate_enum_type(type)) {
          dump_tag_reference(type);
        } else {
          /* A typedef; output its name. */
          dump_type_name(type);
        }  /* if */
      }
      break;
    case iek_variable:
      dump_variable_name((a_variable_ptr)entry);
      break;
    case iek_constant:
      dump_constant_name((a_constant_ptr)entry);
      break;
    case iek_routine:
      dump_routine_name((a_routine_ptr)entry);
      break;
    case iek_label:
      dump_label_name((a_label_ptr)entry);
      break;
    default:
      unexpected_condition_str("gen_name_reference: bad entry kind");
  }  /* switch */
}  /* gen_name_reference */


static void dump_param_id_list(a_variable_ptr param_var)
/*
Dump an old-style parameter id list.  param_var is the first old-style
parameter variable.
*/
{
  if (param_var != NULL) {
    for (;;) {
      dump_variable_name(param_var);
      /* Stop after the last parameter. */
      param_var = param_var->next;
      if (param_var == NULL) break;
      /* Put out a separator and keep looping. */
      write_tok_ch(',');
      write_space();
    }  /* for */
  }  /* if */
}  /* dump_param_id_list */


static void dump_function_declarator_with_scope(a_type_ptr  type,
                                                a_scope_ptr scope)
/*
Output a function declarator for the indicated routine type.
This is the top-level type of a function definition only if scope
is non-NULL, in which case that is the function scope.
*/
{
  a_routine_type_supplement_ptr rtsp = type->variant.routine.extra_info;
  a_param_type_ptr              param;
  a_variable_ptr                param_var = NULL;
  a_boolean                     saved_gen_vla_array_as_asterisk_bound_array =
                                    octl.gen_vla_array_as_asterisk_bound_array;
  a_func_prototype_stack_entry  fpse;

  /* Push an entry onto the function prototype stack. */
  fpse.params = rtsp->param_type_list;
  fpse.outside_parameter_list = FALSE;
  push_function_prototype(&fpse, &octl);
  if (scope != NULL) {
    param_var = scope->variant.routine.parameters;
  } else {
    /* Put out VLA dimensions as "[*]" because the expression information is
       not available. */
    octl.gen_vla_array_as_asterisk_bound_array = TRUE;
  }  /* if */
  write_tok_ch('(');
  /* A routine is put out as unprototyped if its interface is unprototyped
     or if this is the definition and the definition is old-style (i.e.,
     there was a prototyped declaration and then an old-style definition). */
#if C_GEN_BE_GENERATES_C23
  if (rtsp->old_style_params_scanned) {
    /* However, the declaration of a function with parameters cannot be
       put out as "unprototyped" in C23 since such a declaration will
       declare a function with no parameters. */
    rtsp->prototyped = TRUE;
    rtsp->old_style_params_scanned = FALSE;
  }  /* if */
#endif /* C_GEN_BE_GENERATES_C23 */
  /* If the prototype is attached to a function or variable, in C mode,
     its prototype scope if any will have been processed to promote the
     types out into the file scope.  If the prototype appears in some
     other weird context, e.g.,
       struct {
         long *(*p) (struct { int i; });
       } x;
     the prototype will not have been processed and should be put out
     here as an old-style function declarator.  This processing is only
     done when generating K&R C.  When generating ANSI C, such types in
     such unprocessed prototype scopes are put out in place. */
  /* When generating K&R C, a definition of a prototyped function is put
     out as an old-style function. */
  if (!rtsp->prototyped ||
#if !C_GEN_BE_GENERATES_ANSI_C
      (il_header.source_language == sl_C &&
       !type->prototype_scope_types_if_any_promoted) ||
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
      (scope != NULL
#if C_GEN_BE_GENERATES_ANSI_C
                     && rtsp->old_style_params_scanned
#endif /* C_GEN_BE_GENERATES_ANSI_C */
                                                      )) {
    /* Old-style list. */
    if (scope != NULL) {
      /* This is the definition of an old-style function.  Put out the
         parameter id list. */
      dump_param_id_list(param_var);
#if !C_GEN_BE_GENERATES_ANSI_C
      if (sun_is_generated_code_target && rtsp->has_ellipsis) {
        /* This takes advantage of a special feature of the Sun cc compiler
           to handle variable argument lists.  The name "__builtin_va_alist"
           is recognized by the Sun compiler to indicate the end of a variable
           argument list. */
        /* Note that one of the cases that comes here is C++ functions that
           have been turned into old-style functions by IL lowering.
           The has_ellipsis flag remains set, which is unusual but convenient
           in this case. */
        if (param_var != NULL) write_tok_str(", ");
        write_tok_str("__builtin_va_alist");
      }  /* if */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
#if defined(__hpux) || defined(__sgi)
      if (rtsp->has_ellipsis) {
	/* The HP/UX and SGI C compilers require that va_alist appear in
	   the argument list at the start of the variable portion of the
	   argument list.  As with the Sun case above, this code will also
	   be used for C++ functions that have been turned into old-style
	   functions by IL lowering. */
        if (param_var != NULL) write_tok_str(", ");
        write_tok_str("va_alist");
      }  /* if */
#endif /* defined(__hpux) || defined(__sgi) */
#if C_GEN_BE_GENERATES_C23
    } else if (rtsp->assoc_routine == NULL) {
      /* C23 requires function prototypes if arguments are passed.  Put out
         an ellipsis to allow for that case. */
      write_tok_str("...");
#endif /* C_GEN_BE_GENERATES_C23 */
    }  /* if */
  } else {
    /* Prototyped list. */
#if !C_GEN_BE_GENERATES_ANSI_C
    /* This is not the definition of the function.  If we're not writing
       annotations, there's nothing to put out.  If we are, everything
       we write is inside a comment. */
    if (annotate) {
      start_comment();
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
      param = rtsp->param_type_list;
      if (param == NULL) {
        /* The first argument is NULL, so this is a "void" parameter list. */
        if (!rtsp->has_ellipsis) {
          write_tok_str("void");
        } else {
          /* "void f(...)" is permitted in C++ and C23 modes and may be
             accepted (as a nonstandard construct) in earlier C modes as
             well.  Unless the idiom is acceptable in the generated C as
             well, this is put out as an empty old-style parameter list. */
#if ALLOW_ELLIPSIS_ONLY_PARAM_IN_GENERATED_C
          write_tok_str("...");
#endif /* ALLOW_ELLIPSIS_ONLY_PARAM_IN_GENERATED_C */
        }  /* if */
      } else {
        /* List the parameters. */
        for (;;) {
#if C_GEN_BE_GENERATES_ANSI_C
          if (scope != NULL) {
            /* This is the definition of the function, so put out the type and
               name from the parameter variable.  Note that the type in the
               variable might be slightly different than (though, of course,
               compatible with) the type in the param_type entry. */
            check_assertion(param_var != NULL);
            set_output_position(&param_var->source_corresp.decl_position);
            if (param_var->storage_class == (a_storage_class)sc_register) {
              dump_variable_storage_class(param_var);
            }  /* if */
            /* Make sure param_value_has_been_changed gets set whenever
               address_taken is set. */
            check_assertion_str2(!param_var->address_taken ||
                                 param_var->param_value_has_been_changed,
                                 "dump_function_decl...:",
                     "param addr taken, param_value_has_been_changed not set");
            /* Since we're generating C, even unnamed parameters in C++ get
               names. */
            dump_general_declaration_using_type(param_var->type,
                                                &param_var->source_corresp,
                                                param_var, NO_ROUTINE,
                                                NO_FIELD, NO_TEMP, NO_NAME,
                                                TQ_NONE,
                                                /*suppress_const=*/FALSE,
                                                NO_COUNTER);
#if GNU_EXTENSIONS_ALLOWED
            /* Output any attributes associated with the variable. */
            (void)form_variable_attributes(param_var,
                                           /*need_leading_space=*/TRUE, &octl);
#endif /* GNU_EXTENSIONS_ALLOWED */            
            param_var = param_var->next;
          } else
#endif /* C_GEN_BE_GENERATES_ANSI_C */
          {
            /* This is just a declaration, so put out the type and no name. */
            char              *temp = NULL;
            a_const_char      *name = NULL;
            uint32_t          counter = 0;
            a_param_type_ptr  ptp;
            if (param->duplicate_name && !param->is_pack_element) {
              /* The name of this parameter is the same as that of an
                 earlier one.  Use a temporary name instead to avoid
                 invalid generated code. */
              temp = (char *)param;
            } else {
              /* If we have the name, put it out (unless it is reserved). */
              if (param->name != NULL && !is_C_reserved_word(param->name)) {
                name = param->name;
              }  /* if */
              if (gcc_or_clang_is_generated_code_target &&
                  c99_mode && name == NULL) {
                /* gcc has difficulty with [*] VLA parameter types when the
                   parameter is unnamed, so generate a temporary name in C99
                   mode.  (We don't have an easy way to test whether the
                   parameter has a VLA [*] in it.) */
                temp = (char *)param;
              }  /* if */
            }  /* if */
            /* Compute a counter that can be used to distinguish the variadic
               parameter names based on the number of remaining pack
               elements. */
            for (ptp = param ; ptp != NULL && ptp->is_pack_element;
                 ptp = ptp->next, counter++) {}
            /* If the type was qualified in the original, and the qualifiers
               were removed in C++, restore them here. */
            dump_general_declaration_using_type(param->type, NO_SCP,
                                                NO_VARIABLE, NO_ROUTINE,
                                                NO_FIELD, temp, name,
                                                (a_type_qualifier_set)
                                                             param->qualifiers,
                                                /*suppress_const=*/FALSE,
                                                counter);
#if GNU_EXTENSIONS_ALLOWED
            if (is_pointer_type(param->type) &&
                is_function_type(type_pointed_to(param->type))) {
              /* The parameter is a function pointer.  Output any routine
                 type attributes associated with the parameter. */
              (void)form_type_attributes(param->type,
                                         /*need_leading_space=*/TRUE, &octl);
            }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
          }
          param = param->next;
          if (param == NULL) break;
          /* There are more parameters, so output a separator and keep
             looping. */
          write_tok_ch(',');
          write_space();
        }  /* for */
        if (rtsp->has_ellipsis) {
          /* There is an ellipsis. */
          /* Note that this is checked only for routines with a non-empty
             parameter list; see the comment above on the C++ "void f(...)"
             case. */
          write_tok_str(", ...");
        }  /* if */
      }  /* if */
#if !C_GEN_BE_GENERATES_ANSI_C
      end_comment();
    }  /* if */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  }  /* if */
  write_tok_ch(')');
  octl.gen_vla_array_as_asterisk_bound_array =
                                   saved_gen_vla_array_as_asterisk_bound_array;
  pop_function_prototype(&octl);
}  /* dump_function_declarator_with_scope */


static void dump_function_declarator(a_type_ptr type)
/*
Output a function declarator for the indicated routine type.  This is
not a function definition.  This routine is used as an interface to the
il_to_str routines.
*/
{
  dump_function_declarator_with_scope(type, (a_scope_ptr)NULL);
}  /* dump_function_declarator */

#if CHECKING

static void validate_type(a_type_ptr type)
/*
Do any desirable consistency checks on the indicated type.
*/
{
  type = f_skip_typerefs(type);
  if (is_array_type(type) &&
      !type->variant.array.is_variable_size_array) {
    /* Check that the size of an array type is the element size times
       the number of elements. */
    a_targ_size_t size = type->size;
    if (size != 0) {
      a_targ_size_t elem_size= f_skip_typerefs(array_element_type(type))->size;
      check_assertion(!type->variant.array.is_template_dependent_size_array);
      check_assertion_str(
            elem_size * type->variant.array.variant.number_of_elements == size,
            "validate_type: incorrect array size");
    }  /* if */
  } else {
    check_assertion_str(!is_nullptr_type(type),
                        "validate_type: unlowered nullptr type");
  }  /* if */
}  /* validate_type */

#endif /* CHECKING */


static void dump_general_declaration_using_type(
                                     a_type_ptr               type,
                                     a_source_correspondence  *scp,
                                     a_variable_ptr           var,
                                     ARG_UNUSED a_routine_ptr rout,
                                     a_field_ptr              field,
                                     char                     *temp,
                                     a_const_char             *name,
                                     a_type_qualifier_set     added_qualifiers,
                                     a_boolean                suppress_const,
                                     uint32_t                 counter)
/*
Output a declaration built around a type.  "type" gives the type.  The rest
of the arguments specify the name, if any, to be placed in the middle of
the type declarator.  The argument scp is the source correspondence entry
for the entity being declared, or NULL if there is no name.  If var is
non-NULL, it points to a variable being declared (and &scp ==
&var->source_corresp); var is ignored if scp is NULL.  If rout is non-NULL,
it points to a routine being declared (and &scp == &rout->source_corresp).
If field is non-NULL, it points to a field being declared (and &scp ==
&field->source_corresp).  If temp is non-NULL, it gives the address of an
IL entry from which a temporary name is to be generated.  If name is not
NULL, it gives the name to be put out.  If added_qualifiers is not zero,
the indicated qualifiers are added on top of the type.  If suppress_const
is TRUE, suppress generation of top-level "const" in ANSI C mode.  If
counter is non-zero, append it to the name.  This is only used when
name is non-NULL.
*/
{
  a_form_type_options_set options = FTO_NO_OPTIONS;

#if CHECKING
  validate_type(type);
#endif /* CHECKING */
#if C_GEN_BE_GENERATES_C23
  if (scp != NULL && scp->name != NULL && scp->name[0] == 'b' &&
      strcmp(scp->name + 1, "ool") == 0) {
    /* C23 defines a built-in "bool" type, so any declaration of an entity
       with that name will conflict.  Replace the name with an alternative
       spelling. */
    scp->name = "__EDG_user_declared_bool";
  }  /* if */
#endif /* C_GEN_BE_GENERATES_C23 */
  if (suppress_const) options = FTO_SUPPRESS_CONST;
#if GNU_EXTENSIONS_ALLOWED
  if (rout != NULL) {
    /* Any routine attributes should immediately precede the return type. */
    if (form_routine_attributes(rout, /*need_leading_space=*/FALSE, &octl)) {
      m_write_space();
    }  /* if */
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  /* Write the specifiers and the first part of the declarator. */
  form_type_first_part(type, /*under_lhs_declarator=*/FALSE,
                       /*need_trailing_space=*/
                                 (scp != NULL || temp != NULL || name != NULL),
                       added_qualifiers, options, &octl);
  /* Write the name if there is one. */
  if (name != NULL) {
    /* If a counter was provided, output it before the name. */
    ensure_enough_room_on_line(strlen(name)+(counter != 0 ? 8 : 0));
    if (counter != 0) {
      m_write_ch('_');
      m_write_ch('_');
      write_unsigned_num((a_host_large_unsigned)counter);
      m_write_ch('_');
    }  /* if */
    write_tok_str(name);
  } else if (scp != NULL) {
    /* Write the name. */
    if (var != NULL) {
      dump_variable_name(var);
    } else if (field != NULL) {
      dump_field_name(field);
    } else if (rout != NULL && rout->superseded_external) {
      /* Convert the declaration of the routine into a declaration of a
         function pointer with a temporary name.  This function pointer
         will be initialized to the address of the superseding routine,
         cast to the type of the superseded routine.  This works around an
         issue with versions of gcc beginning with 3.4, which fault when
         attempting to call a function directly through a cast to a
         different type. */
      write_tok_str("(*");
      dump_temp_name((char *)rout);
      write_tok_ch(')');
    } else {
      dump_name(scp);
    }  /* if */
  } else if (temp != NULL) {
    /* Write a generated temporary name. */
    dump_temp_name(temp);
  }  /* if */
  /* Write the second part of the declarator. */
  form_type_second_part(type, /*under_lhs_declarator=*/FALSE, options,
                        &octl);
}  /* dump_general_declaration_using_type */


static void dump_declaration_using_type(a_type_ptr              type,
                                        a_source_correspondence *scp)
/*
Output a declaration built around a type.  The argument scp is the source
correspondence entry for the entity being declared, or NULL if there is
no name.
*/
{
  dump_general_declaration_using_type(type, scp, NO_VARIABLE, NO_ROUTINE,
                                      NO_FIELD, NO_TEMP, NO_NAME, TQ_NONE,
                                      /*suppress_const=*/FALSE, NO_COUNTER);
}  /* dump_declaration_using_type */


static void dump_type(a_type_ptr type,
                      a_boolean  add_pointer_to)
/*
Output a reference to a type.  If add_pointer_to is TRUE, add an extra
"pointer to" on top of the type.
*/
{
#if CHECKING
  validate_type(type);
#endif /* CHECKING */
  /* Write the specifiers and the first part of the declarator. */
  form_type_first_part_simple(type, /*under_lhs_declarator=*/add_pointer_to,
                              /*need_trailing_space=*/FALSE, &octl);
  /* The "name" in the type declarator is null.  For the add_pointer_to
     case, add an extra "*". */
  if (add_pointer_to) write_tok_str(" *");
  /* Write the second part of the declarator. */
  form_type_second_part_simple(type, /*under_lhs_declarator=*/add_pointer_to,
                               &octl);
}  /* dump_type */


static void dump_stdc_pragma(a_stdc_pragma_kind   kind,
                             a_stdc_pragma_value  value)
/*
Dump one of the predefined C99 pragmas (which pragma to emit is determined
by "kind", while "value" specifies whether the pragma should be "ON", "OFF"
or "DEFAULT").
*/
{
  uint32_t  saved_indent = indent;

  end_output_line_if_begun();
  indent = 0;
  disable_line_wrapping();
  write_str("#pragma STDC ");
  switch (kind) {
    case stdc_pk_fp_contract:
      write_str("FP_CONTRACT ");
      if (innermost_function_scope == NULL) {
        curr_default_fp_contract = value;
      }  /* if */
      break;
    case stdc_pk_fenv_access:
      write_str("FENV_ACCESS ");
      if (innermost_function_scope == NULL) {
        curr_default_fenv_access = value;
      }  /* if */
      break;
    case stdc_pk_cx_limited_range:
      write_str("CX_LIMITED_RANGE ");
      if (innermost_function_scope == NULL) {
        curr_default_cx_limited_range = value;
      }  /* if */
      break;
#if FIXED_POINT_ALLOWED && !LOWER_FIXED_POINT
    case stdc_pk_fx_full_precision:
      write_str("FX_FULL_PRECISION ");
      if (innermost_function_scope == NULL) {
        curr_default_fx_full_precision = value;
      }  /* if */
      break;
    case stdc_pk_fx_fract_overflow:
      write_str("FX_FRACT_OVERFLOW ");
      if (innermost_function_scope == NULL) {
        curr_default_fx_fract_overflow = value;
      }  /* if */
      break;
    case stdc_pk_fx_accum_overflow:
      write_str("FX_ACCUM_OVERFLOW ");
      if (innermost_function_scope == NULL) {
        curr_default_fx_accum_overflow = value;
      }  /* if */
      break;
#endif /* FIXED_POINT_ALLOWED && !LOWER_FIXED_POINT */
    default:
      unexpected_condition_str("dump_stdc_pragma: bad kind");
      break;
  }  /* switch */
  switch (value) {
    case stdc_pv_off: write_str("OFF"); break;
    case stdc_pv_on: write_str("ON"); break;
#if FIXED_POINT_ALLOWED && !LOWER_FIXED_POINT
    case stdc_pv_sat: write_str("SAT"); break;
#endif /* FIXED_POINT_ALLOWED && !LOWER_FIXED_POINT */
    case stdc_pv_default: write_str("DEFAULT"); break;
    default: unexpected_condition_str("dump_stdc_pragma: bad value"); break;
  }  /* switch */
  enable_line_wrapping();
  end_output_line();
  indent = saved_indent;
}  /* dump_stdc_pragma */

#if UPC_EXTENSIONS_ALLOWED

static void dump_upc_pragma(a_upc_access_method  access_method)
/*
Write out a UPC pragma that establishes the given access method as the
default.  If we are not inside a function definition, update the variable
curr_default_upc_access_method to reflect the new default.
*/
{
  a_boolean      is_strict = (access_method ==
                                       (a_upc_access_method)upc_access_strict);
  uint32_t  saved_indent = indent;

  end_output_line_if_begun();
  indent = 0;
  disable_line_wrapping();
  write_str(is_strict ? (char *)"#pragma upc strict" :
                        (char *)"#pragma upc relaxed");
  enable_line_wrapping();
  end_output_line();
  indent = saved_indent;
  if (innermost_function_scope == NULL) {
    curr_default_upc_access_method = access_method;
  }  /* if */
}  /* dump_upc_pragma */

#endif /* UPC_EXTENSIONS_ALLOWED */

static void dump_pragma(a_pragma_ptr pp)
/*
Dump a single #pragma from the IL entry.
*/
{
  uint32_t      saved_indent = indent;
  a_boolean     saved_suppress_line_breaking = octl.suppress_line_breaking;

  /* Ignore this entry if told to do so. */
  if (!pp->ignore_in_back_end) {
    end_output_line_if_begun();
    set_output_position(&pp->position);
    indent = 0;
    disable_line_wrapping();
    octl.suppress_line_breaking = TRUE;
    if (pp->kind == (a_pragma_kind)pk_stdc) {
      if (innermost_function_scope == NULL) {
        /* File-scope STDC pragmas are emitted based on IL information. */
      } else {
        dump_stdc_pragma(pp->variant.stdc.kind, pp->variant.stdc.value);
      }  /* if */
#if UPC_EXTENSIONS_ALLOWED
    /* Check for #pragma upc. */
    } else if (pp->kind == (a_pragma_kind)pk_upc) {
      /* UPC pragmas are emitted based on IL information. */
#endif /* UPC_EXTENSIONS_ALLOWED */
#if IDENT_DIRECTIVE_AND_PRAGMA
    /* Check for #ident (#pragma ident is handled by the normal #pragma
       processing code). */
    } else if (pp->kind == (a_pragma_kind)pk_ident_directive) {
      write_str("#ident ");
      /* Don't escape tab characters. */
      octl.gen_raw_tab_in_literals = TRUE;
      dump_constant(pp->variant.ident_string);
      octl.gen_raw_tab_in_literals = FALSE;
#endif /* IDENT_DIRECTIVE_AND_PRAGMA */
#if SUN_EXTENSIONS_ALLOWED
    } else if (pp->kind == (a_pragma_kind)pk_enable_ldscope ||
               pp->kind == (a_pragma_kind)pk_disable_ldscope) {
      /* The Sun-specific enable_ldscope and disable_ldscope pragmas are
         not emitted in this back end because their position in the source
         is an important part of their effect and we have no mechanism to
         determine that position here.  Instead, we avoid name collisions
         with Sun link specifiers by modifying names when needed. */
#endif /* SUN_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (pp->kind == (a_pragma_kind)pk_comment) {
      /* Dump a Microsoft #pragma comment. */
      write_str("#pragma comment(");
      write_str(microsoft_pragma_comment_ids[(int)pp->variant.comment.kind]);
      if (pp->variant.comment.str != NULL) {
        write_str(", ");
        dump_constant(pp->variant.comment.str);
      }  /* if */
      write_str(")");
    } else if (pp->kind == (a_pragma_kind)pk_conform) {
      /* This pragma has no effect on C code, so we don't render it here. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else {
      check_assertion_str(pp->pragma_text != NULL,
                          "dump_pragma: NULL pragma_text");
      write_str("#pragma ");
      write_str(pp->pragma_text);
    }  /* if */
    enable_line_wrapping();
    octl.suppress_line_breaking = saved_suppress_line_breaking;
    end_output_line();
    indent = saved_indent;
  }  /* if */
}  /* dump_pragma */


static void dump_scope_pragmas(a_scope_ptr scope)
/*
Dump any pragmas in the indicated scope that are not associated with an
IL entity.
*/
{
  a_pragma_ptr pp;

  for (pp = scope->pragmas; pp != NULL; pp = pp->next) {
    /* Process only pragmas that are not bound to an entity. */
    if (pp->entity.ptr == NULL) {
      dump_pragma(pp);
    }  /* if */
  }  /* for */
}  /* dump_scope_pragmas */


static void dump_associated_pragmas(char             *entity_ptr,
                                    an_il_entry_kind entity_kind)
/*
Dump out any pragmas associated with the entity at the given address with the
given kind.  The caller is responsible for determining that the entity has at
least one associated pragma.
*/
{
  a_pragma_ptr pp, prev_pp = NULL;

  while ((pp = find_assoc_pragma(entity_ptr,
                                 entity_kind,
                                 innermost_function_scope,
                                 prev_pp)) != NULL) {
    dump_pragma(pp);
    prev_pp = pp;
  }  /* while */
  /* Make sure we found at least one pragma. */
  check_assertion_str(prev_pp != NULL,
                      "dump_associated_pragmas: assoc pragma not found");
}  /* dump_associated_pragmas */


template<typename an_IL_type>
static INLINE void dump_associated_pragmas(an_IL_type *il_entity)
/*
Dump out any pragmas associated with the given IL entity.
*/
{
  if (il_entity->source_corresp.has_associated_pragma) {
    /* The entity has one or more associated pragmas.  Dump them. */
    dump_associated_pragmas((char *)il_entity,
                            type_to_il_entry_kind<an_IL_type>());
  }  /* if */
}  /* dump_associated_pragmas */


template<>
INLINE void dump_associated_pragmas(a_statement *il_entity)
/*
Dump out any pragmas associated with the given statement.
*/
{
  if (il_entity->has_associated_pragma) {
    /* The statement has one or more associated pragmas.  Dump them. */
    dump_associated_pragmas((char *)il_entity, iek_statement);
  }  /* if */
}  /* dump_associated_pragmas */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void dump_microsoft_decl_modifiers(a_decl_modifier_set decl_modifiers)
/*
Print a set of Microsoft declaration modifiers.
*/
{
  /* __declspec(nothrow), represented by DM_NOTHROW, is a C++-only attribute
     and is therefore not put out in C code.  Likewise for
     __declspec(novtable) and DM_NOVTABLE. */
  if (decl_modifiers & (DM_DLLFLAGS | DM_THREAD | DM_SELECTANY | DM_NOALIAS |
                        DM_RESTRICT)) {
    write_tok_str("__declspec( ");
    if (decl_modifiers & DM_DLLIMPORT) {
      write_tok_str("dllimport ");
    }  /* if */
    if (decl_modifiers & DM_DLLEXPORT) {
      write_tok_str("dllexport ");
    }  /* if */
    if (decl_modifiers & DM_THREAD) {
      write_tok_str("thread ");
    }  /* if */
    if (decl_modifiers & DM_SELECTANY) {
      write_tok_str("selectany ");
    }  /* if */
    if (decl_modifiers & DM_NOALIAS) {
      write_tok_str("noalias ");
    }  /* if */
    if (decl_modifiers & DM_RESTRICT) {
      write_tok_str("restrict ");
    }  /* if */
    write_tok_str(") ");
  }  /* if */
  if (decl_modifiers & DM_MICROSOFT_INLINE) {
    write_tok_str("__inline ");
  }  /* if */
  if (decl_modifiers & DM_FORCEINLINE) {
    write_tok_str("__forceinline ");
  }  /* if */
}  /* dump_microsoft_decl_modifiers */


static void dump_microsoft_allocate_declspec(a_const_char *allocate_segname)
/*
Put out the Microsoft __declspec(allocate(...)) declaration modifier.
allocate_segname is the segment name, or NULL if the modifier does not apply.
*/
{
  if (allocate_segname != NULL) {
    write_tok_str("__declspec(allocate(");
    ensure_enough_room_on_line(strlen(allocate_segname)+2);
    write_ch('"');
    write_str(allocate_segname);
    write_ch('"');
    write_tok_str(")) ");
  }  /* if */
}  /* dump_microsoft_allocate_declspec */


static void dump_microsoft_align_declspec(a_targ_alignment alignment)
/*
Put out the Microsoft __declspec(align(...)) declaration modifier if the
given alignment value is nonzero.
*/
{
  if (microsoft_dialect_is_generated_code_target && alignment != 0) {
    write_tok_str("__declspec(align(");
    write_unsigned_num((a_host_large_unsigned)alignment);
    write_tok_str(")) ");
  }  /* if */
}  /* dump_microsoft_align_declspec */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void dump_typedef_decl(a_type_ptr type)
/*
Print a typedef declaration.
*/
{
#if GNU_EXTENSIONS_ALLOWED
  a_boolean need_leading_space = TRUE;
#endif /* GNU_EXTENSIONS_ALLOWED */

  if (start_unreferenced_bracket(&type->source_corresp, (a_boolean *)NULL)) {
    if (type->is_builtin_va_list) {
      /* This is the declaration of the builtin va_list, from <stdarg.h>. */
      if (gcc_builtin_varargs_in_generated_code) {
        /* This is the intrinsic GNU C/C++ type __builtin_va_list.
           No declaration should be generated for it. */
      } else {
        /* The va_list type was automatically generated when
           "#include <stdarg.h>" was seen (without parsing the header file).
           Put out the #include directive at this point. */
        /* If the guard macros were defined already, put out #defines so that
           the expansion of <stdarg.h> does not define va_list again. */
#ifdef GUARD_MACRO_FOR_VA_LIST
        if (type->va_list_guard_macro_was_defined) {
          write_pp_directive("#define ", GUARD_MACRO_FOR_VA_LIST);
        }  /* if */
#endif /* ifdef GUARD_MACRO_FOR_VA_LIST */
#ifdef GUARD_MACRO2_FOR_VA_LIST
        if (type->va_list_guard_macro2_was_defined) {
          write_pp_directive("#define ", GUARD_MACRO2_FOR_VA_LIST);
        }  /* if */
#endif /* ifdef GUARD_MACRO2_FOR_VA_LIST */
        write_pp_directive("#include <stdarg.h>", (char *)NULL);
      }  /* if */
    } else if (type->variant.typeref.predeclared
#if C_GEN_BE_GENERATES_C23
               || (type_is(type->variant.typeref.type, tk_integer) &&
                   type->variant.typeref.type->variant.integer.bool_type &&
                   strcmp(type->source_corresp.name, "bool") == 0)
#endif /* C_GEN_BE_GENERATES_C23 */
                                                                  ) {
      /* Don't render predeclared typedefs since the target compiler will
         (presumably) also predeclare them.  (The builtin va_list type is
         an exception in some cases, and therefore handled separately
         above.)  Also suppress the declaration if we're generating a
         typedef for the bool type if we're generating C23 code, since C23
         has a builtin bool type. */
    } else {
      /* Dump any pragmas associated with the type. */
      dump_associated_pragmas(type);
      set_output_position(&type->source_corresp.decl_position);
      write_tok_str("typedef ");
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (microsoft_dialect_is_generated_code_target &&
          type->alignment_set_explicitly) {
        dump_microsoft_align_declspec(type->alignment);
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_VECTOR_TYPES_ALLOWED
      /* Versions 4.1 and later of the GNU compilers issue an error for
         large vector sizes if the vector_size attribute appears before the
         typedef name but the corresponding alignment attribute appears
         after the typedef name. */
      octl.defer_vector_attribute = TRUE;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED && C99_IL_EXTENSIONS_SUPPORTED
      /* A typedef for an 80-/128-bit complex type needs to be modified before
         it is emitted.  The original float_kind will be restored later. */
      a_float_kind orig_float_kind;
      a_type_ptr   complex_type = NULL;
      if (gnu_mode) {
        complex_type = complex_type_needs_modification(type, &orig_float_kind);
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED && C99_IL_EXTENSIONS_SUPPORTED */
      dump_declaration_using_type(type->variant.typeref.type,
                                  &type->source_corresp);
#if GNU_EXTENSIONS_ALLOWED && C99_IL_EXTENSIONS_SUPPORTED
      if (complex_type != NULL) {
        complex_type->variant.float_kind = orig_float_kind;
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED && C99_IL_EXTENSIONS_SUPPORTED */
#if GNU_EXTENSIONS_ALLOWED
      /* Emit any attributes associated with the typedef. */
#if GNU_VECTOR_TYPES_ALLOWED
      { a_type_ptr  vtp = skip_typerefs_not_typedefs(
                                                  type->variant.typeref.type);
        octl.defer_vector_attribute = FALSE;
        if (vtp->kind == (a_type_kind)tk_vector) {
          /* Put out the vector size attribute that was deferred. */
          form_vector_type_attribute(vtp, &need_leading_space, &octl);
        }  /* if */
      }
#endif /* GNU_VECTOR_TYPES_ALLOWED */
      (void)form_type_attributes(type, need_leading_space, &octl);
#endif /* GNU_EXTENSIONS_ALLOWED */
      write_tok_ch(';');
    }  /* if */
    end_unreferenced_bracket(&type->source_corresp);
  }  /* if */
  type->typedef_definition_has_been_put_out = TRUE;
}  /* dump_typedef_decl */


static void dump_enum_definition(a_type_ptr type,
                                 a_boolean  output_final_semi)
/*
Output the definition of the indicated enum type.  Output the final semicolon
if output_final_semi is TRUE.
*/
{
  a_constant_ptr enum_con;
  a_constant_ptr next_enum_value = local_constant();

  check_assertion_str(is_immediate_enum_type(type),
                      "dump_enum_definition: not an enum type");
  enum_con = enum_constants(type);
  /* Empty enumerations are legal in C++ but not in C.  They are supposed
     to be output as the corresponding integral type, but higher up; they
     shouldn't get here. */
  check_assertion_str(enum_con != NULL, "dump_enum_definition: empty enum");
  /* start_unreferenced_bracket is not used here because the enumerator
     constants might be referenced even though the enum type itself is
     not. */
#if !C_GEN_BE_GENERATES_ANSI_C
  /* Enum types are rendered as integers in K&R C, so this definition is
     not needed when generating K&R C, except as an annotation. */
  if (!annotate) goto done;
  /* As an annotation, put out the enum inside a #if 0. */
  write_if_0_directive();
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  /* Dump any pragmas associated with the type. */
  dump_associated_pragmas(type);
  set_output_position(&type->source_corresp.decl_position);
  /* Generate "enum <name>". */
  write_tok_str("enum ");
  /* (Note that a name will be generated for an unnamed enum.  That's
     necessary in C mode to allow the necessary casts of enumerator
     constants, and it's not a bad thing in general.) */
  dump_type_name(type);
  write_tok_str(" {");
  /* Output the enumeration constants. */
  /* Start with an expected value of 0 next. */
  *next_enum_value = *enum_con;
  set_integer_value(&next_enum_value->variant.integer_value,
                    (a_host_large_integer)0);
  for (;;) {
    set_output_position(&enum_con->source_corresp.decl_position);
    /* Output the constant's name. */
    dump_constant_name(enum_con);
    /* Output the value if it's not the next value in sequence or if it
       appeared explicitly in the source as a hexadecimal or octal
       literal. */
    if (enum_con->non_arithmetic ||
        cmp_integer_constants(enum_con, next_enum_value) != 0) {
      write_tok_str(" = ");
      /* We use form_integer_constant because we want to handle the
         -INT_MAX-1 case, and we don't use gen_constant/form_constant
         because we want to suppress the cast to the enum type. */
      form_integer_constant(enum_con, /*suppress_cast=*/TRUE,
                            /*need_parens=*/TRUE, &octl);
      *next_enum_value = *enum_con;
    }  /* if */
    enum_con = enum_con->next;
    /* Stop if at the end of the list of constants. */
    if (enum_con == NULL) break;
    /* Not the end of the list, so output a separator and keep looping. */
    write_tok_ch(',');
    incr_integer_value(&next_enum_value->variant.integer_value);
  }  /* for */
  write_tok_ch('}');
#if GNU_EXTENSIONS_ALLOWED
  /* Emit any attributes associated with the type. */
  (void)form_type_attributes(type, /*need_leading_space=*/TRUE, &octl);
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (output_final_semi) write_tok_ch(';');
#if !C_GEN_BE_GENERATES_ANSI_C
  /* Close the #if 0 started above. */
  write_endif_0_directive();
done:;
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  release_local_constant(&next_enum_value);
}  /* dump_enum_definition */


static void dump_bit_field_base_type_name(a_field_ptr field)
/*
Output the name of the type to be used as the base type of the indicated
bit field in the generated code.
*/
{
  a_const_char *type_str;

  /* Note that "const" is dropped; that's important so that
     initialization code rewritten as executable code by IL lowering
     can assign to this member and the overall struct. */
#if ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C
  /* We're not limited to the standard "int" and "unsigned int", so
     put out the underlying integer type for the bit field as
     written. */
  /* Note, however, that we do not put out enum types; for those,
     we put out the underlying integer type.  Typedefs are
     dropped too, but that's just what falls out. */
  { a_type_ptr        base_type = skip_typerefs(field->type);
    an_integer_kind   base_ikind;
    a_targ_alignment  type_alignment = alignment_of_type(field->type);

    if (field->alignment == 0 && type_alignment != base_type->alignment) {
      /* The alignment of the type used to declare the bit field is different
         from the underlying type (e.g., because the original type is a typedef
         with an alignment attribute).  Compensate by issuing an explicit
         alignment attribute on the field declaration. */
#if MICROSOFT_EXTENSIONS_ALLOWED
      dump_microsoft_align_declspec(type_alignment);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
      if (gcc_or_clang_is_generated_code_target) {
        write_tok_str("__attribute((packed,aligned(");
        write_unsigned_num((a_host_large_unsigned)type_alignment);
        write_tok_str("))) ");
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    }  /* if */
    check_assertion(type_is(base_type, tk_integer));
    base_ikind = base_type->variant.integer.int_kind;
#if !C_GEN_BE_GENERATES_ANSI_C
    if (base_ikind == (an_integer_kind)ik_signed_char) {
      /* For old-style C, use "char" for "signed char". */
      base_ikind = (an_integer_kind)ik_char;
    }  /* if */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
    /* Get the type name. */
    type_str = int_kind_name_full(base_ikind,
                                  /*for_generated_code=*/TRUE);
#if C_GEN_BE_GENERATES_ANSI_C
    /* If the kind does not make the signedness explicit,
       put it out explicitly.  For example, instead of "int" put
       out "signed int" or "unsigned int" depending on the signedness
       chosen for the bit field. */
    if (type_str[0] == 's' && type_str[1] == 'i' && type_str[2] == 'g') {
      /* "signed", so already explicitly signed. */
    } else if (type_str[0] == 'u' && type_str[1] == 'n' &&
               type_str[2] == 's') {
      /* "unsigned", so already explicitly unsigned. */
    } else {
      /* The type is not explicitly signed, so put out a signedness
         indication. */
      write_tok_str(field->bit_field_is_signed ? (char *)"signed " :
                                                 (char *)"unsigned ");
    }  /* if */
#endif /* C_GEN_BE_GENERATES_ANSI_C */
  }
#else /* !ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C */
  /* Use only standard "int" or "unsigned int" base types. */
#if C_GEN_BE_GENERATES_ANSI_C
  /* If the field is signed, make that explicit, so the choice is
     not left to the underlying C compiler. */
  type_str = (char *)(field->bit_field_is_signed ? "signed int"
                                                 : "unsigned int");
#else /* !C_GEN_BE_GENERATES_ANSI_C */
  type_str = (char *)(field->bit_field_is_signed ? "int"
                                                 : "unsigned int");
#endif /* C_GEN_BE_GENERATES_ANSI_C */
#endif /* ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C */
  write_tok_str(type_str);
}  /* dump_bit_field_base_type_name */


static void dump_bit_field_padding(a_field_ptr field)
/*
Dump out declarations to describe padding after the indicated bit field,
which has a declared size that is larger than its base type.
*/
{
  uint32_t     padding = (uint32_t)field->declared_bit_size - field->bit_size;
  uint32_t     bits = field->offset_bit_remainder + field->bit_size;
  a_const_char *bf_type;

#if ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C
  bf_type = "char";
#else /* !ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C */
  bf_type = "int";
#endif /* ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C */
  bits = bits % targ_char_bit;
  /* Bits in the first chunk, to finish out the current byte. */
  bits = targ_char_bit - bits;
  while (padding > 0) {
    if (bits > padding) bits = padding;
    write_space();
    write_tok_str(bf_type);
    write_tok_ch(':');
    write_unsigned_num((a_host_large_unsigned)bits);
    write_tok_ch(';');
    padding -= bits;
    bits = targ_char_bit;
  }  /* while */
}  /* dump_bit_field_padding */


static void track_microsoft_bit_field_allocation(a_field_ptr field)
/*
If field is a bit-field, update msvc_bit_field_tracker as needed to remember
the offset of the container within which the bit-field is allocated.  If
field is not a bit-field, reset the tracker so that the next bit-field will
be recognized as the start of a new container.  This should be called for
each field of a struct being declared, whether or not it is a bit-field, but
only if the Microsoft compiler is the generated code target.
*/
{
  a_type_ptr field_type = skip_typerefs(field->type);

  check_assertion(msvc_is_generated_code_target);
  if (field->is_bit_field) {
    if (msvc_bit_field_tracker.container_type != NULL &&
        compatible_ms_bit_field_container_types(
                                      field_type,
                                      msvc_bit_field_tracker.container_type) &&
        field->offset < msvc_bit_field_tracker.container_offset +
                                                            field_type->size) {
      /* Bit-field is still in the same container. */
    } else {
      /* This bit-field starts a new container. */
      msvc_bit_field_tracker.container_type = field_type;
      msvc_bit_field_tracker.container_offset = field->offset;
    }  /* if */
  } else {
    /* Clear container_type so a subsequent bit-field will start a new
       container. */
    msvc_bit_field_tracker.container_type = NULL;
  }  /* if */
}  /* track_microsoft_bit_field_allocation */


static a_targ_size_t offset_after_field(a_field_ptr field)
/*
Return the byte offset following the end of the indicated field.
*/
{
  a_targ_size_t offset_after;
  a_type_ptr    field_type = skip_typerefs(field->type);

  if (!field->is_bit_field) {
    offset_after = field->offset;
#if IA64_ABI
    if (field->has_no_unique_address_attribute &&
        is_class_or_struct(field_type)) {
      /* A field marked with the [[no_unique_address]] attribute is allocated
         somewhat like a base class subobject, potentially allowing tail
         padding to be reused.  Specifically, max(dsize, nvsize) are allocated
         for the field, where dsize corresponds to the data size prior to
         rounding up for alignment purposes and nvsize is the size not
         including virtual base classes. */
      a_targ_size_t dsize = compute_dsize(field_type);
      a_targ_size_t nvsize = class_type_supp(field_type)
                                          ->size_without_virtual_base_classes;
      offset_after += max_val(dsize, nvsize);
    } else
#endif /* IA64_ABI */
    /* Do not insert code here. */
    {
      offset_after += field_type->size;
    }  /* if */
  } else if (msvc_is_generated_code_target &&
             field->declared_bit_size <= field->bit_size &&
             !(field->bit_size == 0 && field->next == NULL)) {
    /* The Microsoft compiler treats bit-fields as being allocated within a
       container the size of the nominal type of the bit-field.  However,
       that does not affect a bit-field that is declared to be larger than
       the size of the declared type; in that case, because of the extra
       padding added following the container to fill out the declared width
       of the bit-field, the normal calculation below applies, unless the
       bit-field is the last field and has a zero width.  In that case, the
       bit-field is ignored and the normal calculation gives the correct
       answer. */
    if (parent_class_of(field)->kind == (a_type_kind)tk_union) {
      /* In a union, field tracking is irrelevant, since all fields begin
         at offset 0.  The offset is simply the size of the container, i.e.,
         the declared type of the bit-field. */
      offset_after = field_type->size;
    } else {
      /* The offset is that following the bit-field's container. */
      offset_after = msvc_bit_field_tracker.container_offset +
                                   msvc_bit_field_tracker.container_type->size;
    }  /* if */
  } else {
    /* Non-Microsoft bit field. */
    offset_after = field->offset + (targ_char_bit - 1 + 
                                    field->declared_bit_size +
                                    field->offset_bit_remainder) /
                                                  targ_char_bit; /*lint !e776*/
  }  /* if */
  return offset_after;
}  /* offset_after_field */


static a_targ_alignment get_pack_alignment(a_type_ptr  type)
/*
Return the pack alignment, or 0 if it is the default maximum member alignment.
*/
{
  a_targ_alignment  pack_alignment;

  pack_alignment = type->variant.class_struct_union.max_member_alignment;
  if (pack_alignment != 0) {
    if (pack_alignment == il_header.default_max_member_alignment) {
      /* No need to put out a pragma to override the default value. */
      pack_alignment = 0;
    }  /* if */
  }  /* if */
  return pack_alignment;
}  /* get_pack_alignment */


static a_targ_size_t field_padding(a_field_ptr  prev_field,
                                   a_field_ptr  field,
                                   a_type_ptr   type)   
/*
Return the amount of padding needed between "prev_field" and "field".
These two fields are normally consecutive members of the given "type", but
"prev_field" may be NULL, in which case the padding starts at offset zero.
*/
{
  a_targ_size_t  padding = 0;

  if (!C_mode() && type->kind != (a_type_kind)tk_union &&
      (!field->is_bit_field ||
       (prev_field != NULL &&
        prev_field->class_subobject_with_tail_padding))) {
    /* Compute any required padding before the field.  This only comes up
       for empty/promoted base class layout and fields with
       no_unique_address attributes, so check this only when the field has
       a class type (hence also the is_bit_field test). */
    a_type_ptr  field_type = skip_typerefs(field->type);
    a_field_ptr effective_field = field;
    while (effective_field->class_subobject_with_tail_padding) {
      /* Use the first promoted field to compute the required alignment. */
      effective_field = field_type->variant.class_struct_union.field_list;
      field_type = skip_typerefs(effective_field->type);
    }  /* while */
    if (type_is(field_type, tk_array)) {
      /* Arrays of class type have to be checked as well. */
      field_type = underlying_array_element_type(field_type);
    }  /* if */
    if (is_immediate_class_type(field_type) ||
        field->class_subobject_with_tail_padding ||
        (prev_field != NULL &&
         prev_field->class_subobject_with_tail_padding)) {
      a_targ_size_t     after_field, excess_bytes, rounded_after_field;
      a_targ_alignment  alignment = field_alignment_for(effective_field->type);
      if (effective_field->alignment != 0) {
        /* The alignment of the field was explicitly specified. */
        alignment = effective_field->alignment;
#if GNU_EXTENSIONS_ALLOWED
      } else if (effective_field->is_packed) {
        /* If no alignment is explicitly specified, the GNU "packed"
           attribute implies an alignment of 1. */
        alignment = 1;
#endif /* GNU_EXTENSIONS_ALLOWED */
      } else {
        /* The field is not explicitly packed or aligned, but its type may
           have a modified alignment for field layout purposes. */
        a_targ_alignment  pack_alignment = get_pack_alignment(type);
        if (pack_alignment != 0 && pack_alignment < alignment) {
          alignment = pack_alignment;
        }  /* if */
      }  /* if */
      /* The offset after the field, rounded up for the alignment of the
         following field, should give the offset of the following field. */
      after_field = (prev_field != NULL) ? offset_after_field(prev_field) : 0;
      excess_bytes = after_field % alignment;
      rounded_after_field = after_field;
      if (excess_bytes != 0 && !effective_field->is_bit_field) {
        /* A bit field can have less than its base type's alignment, so we
           don't round the size of the preceding field to an alignment
           boundary. */
        rounded_after_field += alignment - excess_bytes;
      }  /* if */
#if CHECKING
      if (field->offset < rounded_after_field) {
#if DEBUG
        if (prev_field != NULL) {
          fprintf(f_debug, "curr field     %s\n",
                  prev_field->source_corresp.name);
          fprintf(f_debug, "curr offset    %lu\n",
                  (unsigned long)prev_field->offset);
          fprintf(f_debug, "after_field    %lu\n", (unsigned long)after_field);
        }  /* if */
        fprintf(f_debug, "next offset    %lu\n", (unsigned long)field->offset);
        fprintf(f_debug, "next alignment %lu\n", (unsigned long)alignment);
#endif /* DEBUG */
        internal_error("field_padding: negative padding required");
      }  /* if */
#endif /* CHECKING */
      padding = field->offset - rounded_after_field;
    }  /* if */
  }  /* if */
  return padding;
}  /* field_padding */


static void dump_field_padding(a_field_ptr    field,
                               a_targ_size_t  padding)
/*
Dump out "padding" bytes required after the given "field".  "field" may be
NULL, in which case the padding starts at offset zero.
*/
{
  if (padding > 0) {
    /* Some padding is required. */  
    a_targ_size_t  after_field = (field != NULL) ? offset_after_field(field)
                                                 : 0;
    write_tok_str("char ");
    disable_line_wrapping();
    dump_field_name_with_prefix("__dummy", (a_field_ptr)NULL);
    write_unsigned_num((a_host_large_unsigned)after_field);
    enable_line_wrapping();
    if (padding > 1) {
      write_tok_ch('[');
      write_unsigned_num((a_host_large_unsigned)padding);
      write_tok_ch(']');
    }  /* if */
    write_tok_ch(';');
  }  /* if */
}  /* dump_field_padding */


static void dump_field_annotation_comment(a_field_ptr  field)
/*
Emit a comment describing the layout of the given field.
*/
{
  a_host_large_unsigned temp = field->offset + subobject_offset;

  write_space();
  start_comment();
  write_tok_str(" offset = ");
  write_unsigned_num(temp);
  write_tok_str((char*)((temp == 1) ? " byte" : " bytes"));
  temp = field->offset_bit_remainder;
  if (temp != 0) {
    write_tok_str(", ");
    write_unsigned_num(temp);
    write_tok_str((char *)((temp == 1) ? " bit" : " bits"));
  }  /* if */
  write_tok_str(", type alignment = ");
  write_unsigned_num((a_host_large_unsigned)alignment_of_type(field->type));
  write_space();
  end_comment();
  write_space();
}  /* dump_field_annotation_comment */


static void push_member_name_prefix_component(
                                      a_member_name_prefix_component_ptr pfxp,
                                      a_field_ptr                        field)
/*
Link the specified member name prefix component at the end of the list of
components and set its "field" member to the specified field.
*/
{
  pfxp->field = field;
  pfxp->prev = last_name_prefix_component;
  if (name_prefix_components == NULL) {
    /* This is the first one. */
    name_prefix_components = pfxp;
  } else {
    /* Link at the end. */
    last_name_prefix_component->next = pfxp;
  }  /* if */
  last_name_prefix_component = pfxp /*lint !e733*/;
  pfxp->next = NULL;
  pfxp->prev_subobject_offset = subobject_offset;
  subobject_offset += field->offset;
}  /* push_member_name_prefix_component */


static void pop_member_name_prefix_component(
                                       a_member_name_prefix_component_ptr pfxp)
/*
Remove the specified member name prefix component (which must be the last
one) from the list of components.
*/
{
  check_assertion(pfxp == last_name_prefix_component);
  last_name_prefix_component = pfxp->prev;
  if (pfxp == name_prefix_components) {
    /* The last one remaining in the list. */
    name_prefix_components = NULL;
  } else {
    /* Unlink from the tail of the list. */
    last_name_prefix_component->next = NULL;
  }  /* if */
  subobject_offset = pfxp->prev_subobject_offset;
}  /* pop_member_name_prefix_component */


static void dump_field_list(a_type_ptr  type,
                            a_field_ptr *last_field,
                            a_boolean   *union_alignment_needed)
/*
Put out declarations for the fields of the specified class type.
*last_field is set to the last field declared for a class or struct and to
the largest field for a union.  *union_alignment_needed is set to TRUE if a
bit-field is declared that is larger than the underlying type.  If a field
representing a base class subobject with tail padding is encountered, it is
replaced (via a recursive call) by the members of the base class (with
names suitably mangled to avoid collisions) to allow for reuse of the tail
padding in the generated code.
*/
{
  a_field_ptr   field;
  a_field_ptr   prev_field = NULL;
  a_targ_size_t union_padding_needed = 0;
  a_targ_size_t max_field_size = 0;

  check_assertion(is_immediate_class_type(type));
  if (msvc_is_generated_code_target) {
    /* Ensure that an initial bit-field is recognized as starting a new
       container. */
    msvc_bit_field_tracker.container_type = NULL;
  }  /* if */
  /* Ignore any optimized empty class fields in this loop. */
  for (field = next_non_empty_field(
                                  type->variant.class_struct_union.field_list);
       field != NULL;
       field = next_non_empty_field(field->next)) {
    a_targ_size_t padding;
    /* Add any required padding before the field. */
    padding = field_padding(prev_field, field, type);
    check_assertion(padding <= type->size);
    if (padding > 0) {
      dump_field_padding(prev_field, padding);
    }  /* if */
    set_output_position(&field->source_corresp.decl_position);
    dump_associated_pragmas(field);
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_dialect_is_generated_code_target) {
      dump_microsoft_align_declspec(field->alignment);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (field->class_subobject_with_tail_padding) {
      /* This field represents a base class subobject or a
         no_unique_address field with a class type and that base or member
         class type has tail padding.  Register the name of the field for
         use in mangling, and call this routine recursively to dump the
         subobject class members instead of the subobject field declaration,
         to allow for reuse of the tail padding. */
      a_member_name_prefix_component prefix;
      a_targ_size_t                  offset_after_fields;
      a_type_ptr                     field_type = skip_typerefs(field->type);
      if (field->has_no_unique_address_attribute) {
        a_class_type_supplement_ptr ctsp = class_type_supp(field_type);
        if (ctsp->has_subobject_type) {
          /* Use the subobject type, which doesn't have the padding member
             at the end. */
          field_type = ctsp->subobject_partner;
        }  /* if */
      }  /* if */
      push_member_name_prefix_component(&prefix, field);
      dump_field_list(field_type, last_field, union_alignment_needed);
      offset_after_fields = offset_after_field(*last_field);
      if (offset_after_fields < field->type->size) {
        /* Add end-of-struct padding. */
        dump_field_padding(*last_field,
                           field_type->size - offset_after_fields);
      }  /* if */
      if (msvc_is_generated_code_target) {
        /* A bit-field following a base class subobject will be in a new
           container. */
        msvc_bit_field_tracker.container_type = NULL;
      }  /* if */
      pop_member_name_prefix_component(&prefix);
    } else if (!field->is_bit_field) {
      a_type_ptr field_type = field->type;
      /* Not a bit field. */
#if GCC_IS_GENERATED_CODE_TARGET
      /* If we are generating code for an early GNU compiler, check for a
         flexible array member and put out its bound as [0] instead of [].
         Starting with GNU C/C++ 3.0, the [] syntax is accepted (and only
         that syntax allows for the initialization of flexible array members
         using aggregate initializer syntax). */
      if (gcc_is_generated_code_target &&
          gnu_target_version_number < 30000 &&
          type->variant.class_struct_union.contains_flexible_array_member &&
          is_array_type(field_type) &&
          is_incomplete_type(field_type)) {
        skip_typerefs(field_type)->variant.array.bound_is_zero = TRUE;
      }  /* if */
#endif /* GCC_IS_GENERATED_CODE_TARGET */
      /* Note that a name will be generated for an anonymous union in C++. */
      /* Note that "const" is dropped; that's important so that
         initialization code rewritten as executable code by IL lowering
         can assign to this member and the overall struct. */
      dump_general_declaration_using_type(field_type,
                                          &field->source_corresp,
                                          NO_VARIABLE, NO_ROUTINE, field,
                                          NO_TEMP, NO_NAME, TQ_NONE,
                                          /*suppress_const=*/TRUE,
                                          NO_COUNTER);
#if GNU_EXTENSIONS_ALLOWED
      (void)form_field_attributes(field, /*need_leading_space=*/TRUE, &octl);
#endif /* GNU_EXTENSIONS_ALLOWED */
      write_tok_ch(';');
      if (skip_typerefs(field_type)->generated_as_empty_struct ||
          (is_array_type(field_type) &&
           f_skip_typerefs(underlying_array_element_type(field_type))->
                                                  generated_as_empty_struct)) {
        /* The layout of the containing struct was calculated assuming that
           the base type of this member was a one-byte struct.  Since that
           type was actually generated with zero length, we need to add a
           padding member to compensate.  (We can't use the normal field
           padding mechanism because that only adds padding before members
           of struct type and this padding must be added unconditionally.)
           For a union, to avoid multiple identical member declarations, we
           add the padding (if still needed) at the end; for a struct, we
           add the padding member immediately (and note that this test must
           be matched with a similar one in dump_initializer_part). */
        a_targ_size_t field_size = skip_typerefs(field_type)->size;
        if (type->kind == (a_type_kind)tk_union) {
          union_padding_needed = field_size;
        } else {
          write_tok_str("char ");
          disable_line_wrapping();
          dump_field_name_with_prefix("__dummy_empty", (a_field_ptr)NULL);
          write_unsigned_num(offset_after_field(field));
          enable_line_wrapping();
          if (field_size > 1) {
            write_tok_ch('[');
            write_unsigned_num((a_host_large_unsigned)field_size);
            write_tok_ch(']');
          }  /* if */
          write_tok_ch(';');
        }  /* if */
      } else {
        max_field_size = skip_typerefs(field_type)->size;
      }  /* if */
    } else {
      /* Bit field. */
#if !C_GEN_BE_GENERATES_ANSI_C
      if (type->kind == (a_type_kind)tk_union) {
        /* When generating K&R C, don't generate bit fields in unions
           because pcc doesn't allow them. */
        /* Don't put out unnamed bit fields.  That's important to keep
           the first initializable field first. */
        if (has_name(field)) {
          a_type_ptr       eff_type = field->type;
          a_type_ptr       under_type = skip_typerefs(eff_type);
          a_targ_size_t    union_size = type->size;
          a_targ_alignment union_alignment = type->alignment;
          a_type           local_type;
          /* If the underlying type is bigger than the size allocated for
             the union, use a smaller integral type. */
          if (under_type->size > union_size ||
              alignment_of_type(eff_type) > union_alignment) {
            /* Find the largest integral type with the right signedness that
               will fit in the union. */
            a_byte           ikind;
            an_integer_kind  eff_ikind;
            a_targ_size_t    int_size;
            a_targ_alignment int_alignment;
            for (ikind = ik_unsigned_int; ; ikind--) {
              get_integer_size_and_alignment((an_integer_kind)ikind, &int_size,
                                             &int_alignment);
              if (int_size <= union_size &&
                  int_alignment <= union_alignment &&
                  int_kind_is_signed[ikind] == field->bit_field_is_signed) {
                /* This size is okay. */
                eff_ikind = (an_integer_kind)ikind;
                break;
              }  /* if */
            }  /* for */
            /* Make a local type (not allocated in the IL) that is the right
               integer type.  We can't use integer_type in a "back end". */
            local_type = *under_type;
            eff_type = &local_type;
            check_assertion(local_type.kind == (a_type_kind)tk_integer);
            local_type.variant.integer.int_kind = eff_ikind;
          }  /* if */
          dump_general_declaration_using_type(eff_type,
                                              &field->source_corresp,
                                              NO_VARIABLE, NO_ROUTINE,
                                              NO_FIELD, NO_TEMP, NO_NAME,
                                              TQ_NONE,
                                              /*suppress_const=*/TRUE,
                                              NO_COUNTER);
          write_tok_ch(';');
        }  /* if */
      } else
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
      /* Don't insert code here -- this is the "else" of an "if". */
      {
        /* Put out a bit field declaration. */
        if (field->bit_field_alignment_type != NULL) {
          /* Put out an alignment indication for a field that was declared
             larger than the underlying base type. */
          if (type->kind == (a_type_kind)tk_union) {
            /* For the union case, do this at the end. */
            *union_alignment_needed = TRUE;
          } else {
            dump_type(field->bit_field_alignment_type,
                      /*add_pointer_to=*/FALSE);
            write_tok_str(": 0;");
            write_space();
          }  /* if */
        }  /* if */
        dump_bit_field_base_type_name(field);
        /* Write the name if the field is named. */
        if (has_name(field)) {
          write_space();
          dump_field_name(field);
        }  /* if */
        write_tok_str(": ");
        write_unsigned_num((a_host_large_unsigned)field->bit_size);
#if GNU_EXTENSIONS_ALLOWED
        (void)form_field_attributes(field, /*need_leading_space=*/TRUE,
                                    &octl);
#endif /* GNU_EXTENSIONS_ALLOWED */
        write_tok_ch(';');
        if (field->declared_bit_size > field->bit_size) {
          /* A bit field declared to be larger than the underlying type
             (in C++).  Emit additional padding.  Finish the current
             byte, then put out single bytes, then put out the bits
             in the final byte.  Note that we can only put out unnamed
             fields; we don't want to change the initialization order
             of the struct. */
          if (type->kind == (a_type_kind)tk_union) {
            /* Handle unions at the end.  It doesn't do any good to put
               out padding as another field -- it wouldn't go after the
               field just put out. */
            *union_alignment_needed = TRUE;
          } else {
            dump_bit_field_padding(field);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    if (annotate && !field->class_subobject_with_tail_padding) {
      /* Display the offset in an annotation comment. */
      dump_field_annotation_comment(field);
    }  /* if */
    if (type->kind != (a_type_kind)tk_union || *last_field == NULL) {
      *last_field = field;
    } else {
      /* For a union, remember the biggest field. */
      if (offset_after_field(*last_field) < offset_after_field(field)) {
        *last_field = field;
      }  /* if */
    }  /* if */
    prev_field = field;
    if (msvc_is_generated_code_target) {
      /* Register the field for bit-field allocation tracking purposes. */
      track_microsoft_bit_field_allocation(field);
    }  /* if */
  }  /* for */
  if (name_prefix_components != NULL && prev_field != NULL &&
      prev_field->is_bit_field) {
    /* The last field of a base class subobject whose members have been
       promoted into the derived class is a bit-field.  Add a dummy
       bit-field if needed to ensure that if the following derived class
       member is a bit-field, it doesn't bleed into the space left over
       from the base class field. */
    if (msvc_is_generated_code_target) {
      /* Put out a zero-length bit field to force alignment to the offset
         of the next container. */
      dump_type(prev_field->type, /*add_pointer=*/FALSE);
      write_tok_str(":0;");
    } else {
      an_offset_bit_remainder dummy_bits =
            (an_offset_bit_remainder)((targ_char_bit -
                                      (prev_field->offset_bit_remainder +
                                       prev_field->bit_size)) % targ_char_bit);
      if (dummy_bits != 0) {
        /* Make an unnamed bit field to provide the required padding. */
        write_tok_str("unsigned int:");
        write_unsigned_num((a_host_large_unsigned)dummy_bits);
        write_tok_ch(';');
      }  /* if */
    }  /* if */
  }  /* if */
  if (union_padding_needed > 0 && max_field_size <= union_padding_needed) {
    /* The union's only members were structs that have been generated as
       empty, so we need padding to make the union the correct size. */
    write_tok_str("char ");
    dump_field_name_with_prefix("__dummy_empty", (a_field_ptr)NULL);
    if (union_padding_needed > 1) {
      write_tok_ch('[');
      write_unsigned_num((a_host_large_unsigned)union_padding_needed);
      write_tok_ch(']');
    }  /* if */
    write_tok_ch(';');
  }  /* if */
}  /* dump_field_list */


static void dump_struct_union_definition(a_type_ptr type,
                                         a_boolean  output_final_semi)
/*
Output the definition of the indicated struct or union type.  Output the
final semicolon if output_final_semi is TRUE.
*/
{
  a_field_ptr field, last_field = NULL;
  a_boolean   union_alignment_needed = FALSE;
  a_boolean   need_to_restore_default_alignment = FALSE;

  if (start_unreferenced_bracket(&type->source_corresp, (a_boolean *)NULL)) {
    a_targ_alignment  pack_alignment = get_pack_alignment(type);
    if (pack_alignment != 0
#if GNU_EXTENSIONS_ALLOWED
        && !(gcc_or_clang_is_generated_code_target && pack_alignment == 1 &&
             type->variant.class_struct_union.is_packed)
#endif /* GNU_EXTENSIONS_ALLOWED */
                                                        ) {
      /* Put out a #pragma pack directive to indicate the special alignment
         requirements for this struct.  (Some GNU compilers ignore the
         pragma; attributes are issued instead.  If we know attribute packed
         will be emitted, we don't issue the pragma.) */
      uint32_t saved_indent = indent;
      end_output_line_if_begun();
      indent = 0;
      disable_line_wrapping();
      write_str("#pragma pack(");
      write_unsigned_num((a_host_large_unsigned)pack_alignment);
      write_str(")");
      enable_line_wrapping();
      end_output_line();
      indent = saved_indent;
      need_to_restore_default_alignment = TRUE;
    }  /* if */
    /* Dump any pragmas associated with the type. */
    dump_associated_pragmas(type);
    set_output_position(&type->source_corresp.decl_position);
    write_tok_str(tag_kind(type->kind));
    write_space();
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_dialect_is_generated_code_target) {
      if (type->alignment_set_explicitly) {
        dump_microsoft_align_declspec(type->alignment);
      }  /* if */
      if (type->is_microsoft_intrinsic) {
        write_tok_str("__declspec(intrin_type) ");
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    dump_type_name(type);
    write_tok_str(" {");
    if (annotate) {
      /* Display the struct alignment in an annotation comment. */
      write_space();
      start_comment();
      write_tok_str(" alignment = ");
      write_unsigned_num((a_host_large_unsigned)type->alignment);
      write_space();
      end_comment();
      write_space();
    }  /* if */
    indent += 2;
    dump_field_list(type, &last_field, &union_alignment_needed);
    if (union_alignment_needed) {
      /* Put out extra fields to force alignment for the struct when
         there are fields that did not fit in their base types. */
      for (field = type->variant.class_struct_union.field_list;
           field != NULL;
           field = field->next) {
        if (field->bit_field_alignment_type != NULL) {
          /* Put out a declaration to force alignment for the entire
             structure. */
          write_space();
          dump_type(field->bit_field_alignment_type,
                    /*add_pointer_to=*/FALSE);
          write_space();
          /* The field is named, because some compilers complain about
             unnamed fields in unions.  However, it's at the end, so it does
             not affect initialization semantics. */
          dump_temp_name((char *)field);
          write_tok_ch(';');
        }  /* if */
        if (field->declared_bit_size > field->bit_size) {
          /* Put out a struct with (we hope) the same size as the full
             declared field. */
          write_space();
          write_tok_str("struct { ");
          dump_bit_field_base_type_name(field);
          write_tok_str(":");
          write_unsigned_num((a_host_large_unsigned)field->bit_size);
          write_tok_ch(';');
          dump_bit_field_padding(field);
          write_tok_str(" } ");
          disable_line_wrapping();
          write_tok_ch('_');
          dump_temp_name((char *)field);
          enable_line_wrapping();
          write_tok_ch(';');
        }  /* if */
      }  /* for */
    }  /* if */
    {
      /* Some padding may be required to account for empty bases or for a
         completely empty class (in C++). */
      a_targ_size_t padding;
      if (last_field == NULL) {
        padding = type->size;
      } else if (type->kind != (a_type_kind)tk_union &&
                 is_array_type(last_field->type) &&
                 skip_typerefs(last_field->type)->size == 0) {
        /* A struct ending in a flexible array member or a zero-length array:
           Don't add padding in such cases since it is not generally valid to
           do so. */
        padding = 0;
      } else {
        a_targ_size_t  offset_after_fields = offset_after_field(last_field);
        check_assertion(offset_after_fields <= type->size);
        if (type->kind == (a_type_kind)tk_union) {
          /* If needed at all, the "padding" is the size of the entire
             union. */
          if (offset_after_fields < type->size) {
            padding = type->size;
          } else {
            padding = 0;
          }  /* if */
        } else {
          padding = type->size - offset_after_fields;
        }  /* if */
      }  /* if */
      if (padding > 1) {
        write_tok_str("char __dummy[");
        write_unsigned_num((a_host_large_unsigned)padding);
        write_tok_str("];");
      } else if (padding == 1 ||
                 (
#if GNU_EXTENSIONS_ALLOWED
                  !(il_header.gcc_mode &&
                    gcc_or_clang_is_generated_code_target) &&
#endif /* GNU_EXTENSIONS_ALLOWED */
                  next_non_empty_initializable_field(
                       type->variant.class_struct_union.field_list) == NULL)) {
        /* One byte of padding needed, or... */
        /* Avoid a zero-sized struct for the bizarre case "struct {int :0;}"
           (which is undefined behavior) and for fieldless classes from C++
           passed through IL lowering.  However, GNU C does accept empty
           struct types and gives them size zero (and it also gives size
           zero to "struct { int:0; }").  Note that the test here must
           match one in dump_initializer_part. */
        if (type->size <= 1 && use_empty_struct_in_generated_c) {
          /* Mark this type as having zero size in the generated code so
             offset and initialization logic can compensate. */
          type->generated_as_empty_struct = TRUE;
        } else {
          write_tok_str("char __dummy;");
        }  /* if */
      }  /* if */
    }
    indent -= 2;
    write_tok_ch('}');
#if GNU_EXTENSIONS_ALLOWED
    /* Emit any attributes associated with the type. */
    (void)form_type_attributes(type, /*need_leading_space=*/TRUE, &octl);
#endif /* GNU_EXTENSIONS_ALLOWED */
    if (output_final_semi) write_tok_ch(';');
    if (need_to_restore_default_alignment) {
      /* Restore the packing alignment to a default state. */
      uint32_t saved_indent = indent;
      end_output_line_if_begun();
      indent = 0;
      disable_line_wrapping();
      write_str("#pragma pack()");
      enable_line_wrapping();
      end_output_line();
      indent = saved_indent;
    }  /* if */
    end_unreferenced_bracket(&type->source_corresp);
    type->has_been_defined = TRUE;
  }  /* if */
}  /* dump_struct_union_definition */


static void add_pending_typedef(a_type_ptr pending_typedef,
                                a_type_ptr type_to_be_completed)
/*
Add a typedef/incomplete-struct pair to the list of pending typedefs.  The
typedef declaration will be emitted once the struct definition has been
emitted, and the typedef name will be "invisible" until that occurs.
*/
{
  a_pending_typedef_ptr ptp;

  if (avail_pending_typedefs != NULL) {
    /* Reuse an already-processed record. */
    ptp = avail_pending_typedefs;
    avail_pending_typedefs = ptp->next;
  } else {
    /* Allocate a new record. */
    ptp = (a_pending_typedef_ptr)alloc_general(
                                          (sizeof_t)sizeof(a_pending_typedef));
  }  /* if */
  /* Add the pending typedef to the list. */
  ptp->next = NULL;
  ptp->pending_typedef = pending_typedef;
  ptp->type_to_be_completed = type_to_be_completed;
  if (last_pending_typedef != NULL) {
    last_pending_typedef->next = ptp;
  } else {
    pending_typedefs = ptp;
  }  /* if */
  last_pending_typedef = ptp;
  /* Flag the types appropriately. */
  pending_typedef->typedef_pending = TRUE;
  type_to_be_completed->typedef_pending = TRUE;
}  /* add_pending_typedef */


static void dump_pending_typedefs(a_type_ptr completed_type)
/*
Emit the declaration for any typedefs that were deferred pending the
definition of completed_type and remove the corresponding records from the
list of pending typedefs.
*/
{
  a_pending_typedef_ptr ptp;
  a_pending_typedef_ptr prev_ptp = NULL;
  a_pending_typedef_ptr next_ptp;

  for (ptp = pending_typedefs; ptp != NULL; ptp = next_ptp) {
    next_ptp = ptp->next;
    if (ptp->type_to_be_completed == completed_type) {
      /* This typedef was waiting for the definition of completed_type to
         be emitted.  Emit it now and remove it from the list. */
      dump_typedef_decl(ptp->pending_typedef);
      ptp->pending_typedef->typedef_pending = FALSE;
      if (prev_ptp != NULL) {
        prev_ptp->next = ptp->next;
      } else {
        pending_typedefs = ptp->next;
      }  /* if */
      if (last_pending_typedef == ptp) {
        last_pending_typedef = prev_ptp;
      }  /* if */
      ptp->next = avail_pending_typedefs;
      avail_pending_typedefs = ptp;
    } else {
      prev_ptp = ptp;
    }  /* if */
  }  /* for */
  completed_type->typedef_pending = FALSE;
}  /* dump_pending_typedefs */


static a_boolean typedef_deferred_pending_struct_definition(a_type_ptr type)
/*
If type (which must be a typedef) is, or points to, an array of a struct
that has not yet been defined, add a record to the list of pending typedefs
and return TRUE.  Otherwise, return FALSE.
*/
{
  a_type_ptr tp = type;
  a_boolean  typedef_was_deferred = FALSE;

  for (;;) {
    if (is_array_type(tp)) {
      tp = f_skip_typerefs(underlying_array_element_type(tp));
      if (is_immediate_class_type(tp) && !tp->has_been_defined) {
        /* Defer the declaration of this typedef pending the definition of
           the struct to which it refers. */
        add_pending_typedef(type, tp);
        typedef_was_deferred = TRUE;
        break;
      }  /* if */
    } else if (is_pointer_type(tp)) {
      tp = type_pointed_to(tp);
    } else {
      break;
    }  /* if */
  }  /* for */
  return typedef_was_deferred;
}  /* typedef_deferred_pending_struct_definition */


static void dump_type_decl(a_type_ptr type,
                           int        pass)
/*
Dump out one type declaration.  The only types that can be declared
are enumerations, structs, unions, and typedefs.  When pass == 1 (first pass),
dump enums, and structs/unions as declarations.  When pass == 2 (second
pass), dump typedefs, and structs/unions as definitions (if they are defined).
*/
{
  a_boolean output_defn;

  check_assertion(!(type->size != 0 && type->incomplete));
  if (type->is_tag_redefinition) {
    /* A C23 redefinition of a tag declares the type that the tag already
       denotes, which has been declared here already. */
    goto done;
  }  /* if */
  switch (type->kind) {
    case tk_enum:
      /* Enumeration. */
      check_assertion_str(type->variant.integer.enum_type,
                          "dump_type_decl: non-enum integer type");
      /* Empty enums (valid in C++ but not in C) are put out as integral
         types, so nothing need be put out here. */
      if (enum_constants(type) == NULL) break;
      /* Output enums only on the first pass. */
      if (pass == 1) dump_enum_definition(type, /*output_final_semi=*/TRUE);
      break;
    case tk_struct:
    case tk_union:
      /* Struct or union. */
      /* Output a declaration on the first pass, and a definition on the
         second pass (if the struct/union is defined). */
      output_defn = !is_incomplete_type(type);
#if MAINTAIN_NEEDED_FLAGS
      if (!class_definition_needed_flag_is_set(type)) {
        output_defn = FALSE;
      }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
#if GNU_EXTENSIONS_ALLOWED && GCC_BUILTIN_VARARGS
      if (gcc_or_clang_is_generated_code_target &&
          !target_is_32_bit_x86_based() &&
          class_type_supp(type)->is_va_list_tag) {
        /* The predeclared struct __va_list_tag or struct __va_list is
           necessarily distinct from, and hence not compatible with, the type
           used internally by gcc as the base of __builtin_va_list.  Define a
           typedef that refers to the gcc type; it will be used instead of the
           predeclared struct __va_list_tag or struct __va_list. */
        if (pass == 2 && output_defn) {
          if (target_is_x86_based()) {
            write_tok_str("typedef typeof(((__builtin_va_list*)0)[0][0]) ");
            write_tok_str("__va_list_tag_type;");
          } else {
            write_tok_str("typedef typeof(*(__builtin_va_list*)0) ");
            write_tok_str("__va_list_type;");
          }  /* if */
        }  /* if */
      } else
#endif /* GNU_EXTENSIONS_ALLOWED && GCC_BUILTIN_VARARGS */
      /* Do not insert code here. */
      { if (pass == 1) {
          if (start_unreferenced_bracket(&type->source_corresp,
                                         (a_boolean *)NULL)) {
            if (!output_defn) {
              /* Dump any pragmas associated with the type if no definition
                 will be output on the second pass. */
              dump_associated_pragmas(type);
            }  /* if */
            set_output_position(&type->source_corresp.decl_position);
            dump_tag_reference(type);
            if (is_incomplete_type(type) &&
                type->
                     variant.class_struct_union.inc_class_used_in_array_type) {
              /* An incomplete element type is allowed in C++ but not in C.
                 Add a dummy definition; the type could not still be
                 incomplete at this point if its size or contents were
                 used, so the actual definition does not matter. */
              write_str(" { char dummy; }");
            }  /* if */
            write_tok_ch(';');
            end_unreferenced_bracket(&type->source_corresp);
          }  /* if */
        } else if (output_defn) {
          /* Put out the definition, but avoid multiple definitions
             appearing in the same output file.  In C mode, multiple
             definitions can result if a struct or union is defined in
             function prototype scope and then the function type is used
             (via the GNU typeof extension, for example) to declare a
             function pointer: both the routine type and the type of the
             function pointer will attempt to define the struct/union type
             via dump_prototype_scope_types_within_type.  Such multiple
             definitions would be an error and are consequently suppressed.
             In C++, however, this situation cannot occur (because a
             function type cannot contain a type defined in the function's
             prototype scope).  Furthermore, in
             one-instantiation-per-object mode (which only applies to C++),
             we may need to put out the definition of a struct or union
             more than once if it is needed in multiple slices; this is not
             an error because each slice appears in a different output
             file.  Because we do not reset the has_been_defined flag in
             the types at the beginning of each slice, we simply ignore the
             flag and unconditionally put out the definition of the struct
             or union in one-instantiation-per-object mode C++ translation
             units. */
          if (!type->has_been_defined
#if ONE_INSTANTIATION_PER_OBJECT
              || !C_mode()
#endif /* ONE_INSTANTIATION_PER_OBJECT */
              ) {
            dump_struct_union_definition(type, /*output_final_semi=*/TRUE);
          }  /* if */
          if (type->typedef_pending) {
            dump_pending_typedefs(type);
          }  /* if */
        }  /* if */
      }
      break;
    case tk_typeref:
      if (is_typeref_kind(type, trk_is_decltype) ||
          is_typeref_kind(type, trk_is_underlying_type)) {
        /* Decltype and __underlying_type types do not need to be declared
           separately. */
#if GNU_EXTENSIONS_ALLOWED
      } else if (is_typeref_kind(type, trk_is_typeof_with_expression) ||
                 is_typeref_kind(type, trk_is_typeof_with_type_operand)) {
        /* Typeof types do not need to be declared separately. */
#endif /* GNU_EXTENSIONS_ALLOWED */
      } else if (type->variant.typeref.is_intrinsic_member) {
        /* The synthesized entry for an intrinsically-resolved template type
           member reference (xyz<A...>::member; see
           templ_type_member_intrinsics_enabled) is transparent: Every use is
           emitted through its underlying type.  No C typedef should be
           generated for it (such references often share the member spelling --
           e.g., "type" -- and generating them would therefore produce
           conflicting file-scope typedefs). */
      } else {
        /* Output typedefs only on the second pass. */
        if (pass == 2 &&
            !typedef_deferred_pending_struct_definition(type)) {
          dump_typedef_decl(type);
        }  /* if */
      }  /* if */
      break;
    default:
      unexpected_condition_str("dump_type_decl: bad type");
  }  /* switch */
done:;
}  /* dump_type_decl */


static void mangle_promoted_name(a_source_correspondence *scp,
                                 a_routine_ptr           rout,
                                 a_scope_number          scope_number)
/*
A local entity of the indicated routine, whose source correspondence
entry is scp, is being promoted out of the routine and scope (number)
indicated.  Adjust its name so it will be unique at the file scope.
The encoding here should match mangle_promoted_entity_name.  rout can
be NULL if promoting a type out of a prototype scope that is not associated
with a routine.
*/
{
  /* Leave the name alone if the type is unnamed or if the name has
     already been mangled (e.g., for a local nested class). */
  if (scp->name != NULL && !scp->name_has_been_mangled) {
    Small_string<50> buffer;
    a_const_char     *routine_name = NULL;

    if (rout != NULL && has_name(rout)) {
      routine_name = rout->source_corresp.name;
    }  /* if */
#if !IA64_ABI
    /* Cfront-like ABI: The encoding is the original name, two
       underscores, the mangled name of the routine, and "__Lnn" where
       "nn" is the scope number. */
    buffer.append(scp->name, "__");
    if (routine_name != NULL) {
      buffer.append(routine_name);
    }  /* if */
    buffer.append("__L", scope_number);
#else /* IA64_ABI */
    /* IA-64 ABI encoding:
         _Z Z function-mangled-name E name-with-length _ discriminator
       We don't have an accurate discriminator value, so we use the
       scope number for that.  The routine name already has the "_Z"
       at the front, unless the routine is extern "C". */
    /* buffer0 will contain the _ZZ and anything else that needs to be
       added at the front of the routine name.  buffer will contain the
       "E" and the length for the entity name.  buffer2 will contain the
       "_" and the discriminator number. */
    buffer.append("_ZZ");
    if (routine_name == NULL) {
      /* For an unnamed routine, we put out no name.  That doesn't produce
         a valid mangled name but it may be the best we can do. */
    } else if (routine_name[0] == '_' && routine_name[1] == 'Z') {
      /* Usual case: the routine name is mangled.  Remove "_Z". */
      check_assertion(strlen(routine_name) >= 5);
      buffer.append(routine_name + 2);
    } else {
      /* Unmangled routine name (e.g., for an extern "C" routine).
         Add the length of the name as a prefix. */
      buffer.append(strlen(routine_name), routine_name);
    }  /* if */
    buffer.append("E", strlen(scp->name), scp->name, "_", scope_number);
#endif /* !IA64_ABI */

    /* Allocate space for the mangled name and build it. */
    sizeof_t alloc_length = buffer.length() + 1;
    /* This space is not counted under any debug output.  There shouldn't
       be too much of it. */
    char     *mangled_name = alloc_il_for_c_gen_be(alloc_length);
    buffer.write_to_buffer(mangled_name, alloc_length);
    /* Put the mangled name into the source correspondence entry. */
    /* The old name is just thrown away. */
    scp->name = mangled_name;
    scp->name_has_been_mangled = TRUE;
    scp->is_local_to_function = FALSE;
  }  /* if */
}  /* mangle_promoted_name */


static void adjust_promoted_local_type_name(a_type_ptr     type,
                                            a_routine_ptr  rout,
                                            a_scope_number scope_number)
/*
The indicated type is a local type of the indicated routine
and is being promoted out of the routine and scope (number) indicated.
Adjust its name so it will be unique at the file scope.  rout can
be NULL if promoting a type out of a prototype scope that is not associated
with a routine.
*/
{
  mangle_promoted_name(&type->source_corresp, rout, scope_number);
#if C_GEN_BE_GENERATES_ANSI_C
  /* When generating ANSI C, also mangle the names of enumerator constants
     of an enumeration type. */
  if (type->kind == (a_type_kind)tk_enum) {
    a_constant_ptr enum_con;
    for (enum_con = enum_constants(type);
         enum_con != NULL;
         enum_con = enum_con->next) {
      mangle_promoted_name(&enum_con->source_corresp, rout, scope_number);
    }  /* for */
  }  /* if */
#endif /* C_GEN_BE_GENERATES_ANSI_C */
}  /* adjust_promoted_local_type_name */


static void dump_prototype_scope_types(a_scope_ptr   proto_scope,
                                       a_routine_ptr rout,
                                       int           pass,
                                       a_boolean     *any_found)
/*
If the indicated prototype scope contains any types, output them.
The prototype scope is part of the routine indicated by rout.  rout is
NULL for a prototype scope that is not associated with a routine.
pass is 1 or 2 (declarations are output on the first pass, full definitions
on the second pass, to avoid ordering problems).  *any_found is set to
TRUE if any prototype scope types are found.  This routine is called only
when the source language is C.
*/
{
  a_type_ptr  type;
  a_scope_ptr sub_scope;

  for (type = proto_scope->types; type != NULL; type = type->next) {
    *any_found = TRUE;
    if (pass == 1) {
      /* Do some name mangling so that the name remains unique. */
      adjust_promoted_local_type_name(type, rout, proto_scope->number);
      type->declared_in_function_prototype = FALSE;
    }  /* if */
    dump_type_decl(type, pass);
  }  /* for */
  /* Process any nested prototype scopes. */
  for (sub_scope = proto_scope->scopes;
       sub_scope != NULL;
       sub_scope = sub_scope->next) {
    dump_prototype_scope_types(sub_scope, rout, pass, any_found);
  }  /* for */
}  /* dump_prototype_scope_types */


static void dump_prototype_scope_types_within_type(a_type_ptr type,
                                                   int        pass,
                                                   a_boolean  *any_found)
/*
Examine type (and, if it is a derived type, its underlying types), looking
for prototype scopes.  Whenever one is found, output all the types found
therein (thus promoting them out of the prototype scope).  pass is 1 or 2
(declarations are output on the first pass, full definitions on the
second pass, to avoid ordering problems).  Set *any_found to TRUE if
any prototype scope types are processed.  This routine is used, only
in C mode, to process types that will have to be put out more than
once (e.g., types of functions); without promotion of the prototype
scope types, the two instances of the type would not be compatible.
*/
{
  /* Do a loop so that we deal with prototype scopes at all levels in the
     type, not just on top.  For example:
       int (*f ())(enum E { e } arg) { }
  */
  do {
    if (type->kind == (a_type_kind)tk_routine) {
      a_routine_type_supplement_ptr rtsp = type->variant.routine.extra_info;
      a_scope_ptr                   proto_scope = rtsp->prototype_scope;
      a_routine_ptr                 rout = rtsp->assoc_routine;
      if (proto_scope != NULL) {
        /* This type has a prototype scope.  Output any types declared
           therein. */
        dump_prototype_scope_types(proto_scope, rout, pass, any_found);
      }  /* if */
      type->prototype_scope_types_if_any_promoted = TRUE;
    }  /* if */
    /* Move down to the underlying type.  Stop on a non-derived type. */
  } while ((type = underlying_type_of_derived_type(type)) != NULL);
}  /* dump_prototype_scope_types_within_type */


/*
Helper macro for check_membership_info; calls db_name to display the name
of an entity, but only if DEBUG code is enabled.
*/
#if DEBUG && !STANDALONE_UTILITY_PROGRAM
#define display_entity_if_debug_enabled(entity) \
{ (void)fprintf(f_debug, "\nEntity is "); \
  db_name(&(entity)->source_corresp); \
  (void)fprintf(f_debug, "\n"); \
}  /* display_entity_if_debug_enabled */
#else /* !(DEBUG && !STANDALONE_UTILITY_PROGRAM) */
#define display_entity_if_debug_enabled(entity) /* Nothing */
#endif /* DEBUG && !STANDALONE_UTILITY_PROGRAM */

/*
If "scope" is the file scope, check that the parent information for the
given entity (which is from that scope) does not indicate class or
namespace membership, or have the is_local_to_function flag TRUE.
*/
#if CHECKING && !STANDALONE_UTILITY_PROGRAM
#define check_membership_info(entity, scope) \
{ if ((scope)->kind == (a_scope_kind)sck_file) { \
    if (is_class_or_namespace_member(entity) || \
        (entity)->source_corresp.is_local_to_function) { \
      display_entity_if_debug_enabled(entity); \
      internal_error("check_membership_info: bad membership info"); \
    }  /* if */ \
  }  /* if */ \
}  /* check_membership_info */
#else /* !(CHECKING && !STANDALONE_UTILITY_PROGRAM) */
#define check_membership_info(entity, scope) /* Nothing */
#endif /* CHECKING && !STANDALONE_UTILITY_PROGRAM */


static void dump_scope_types(a_scope_ptr scope)
/*
Dump all types declared within one scope.  As this routine is used now,
the scope must be the file scope.
*/
{
  a_type_ptr                       type;
  int                              pass;
  a_boolean                        suppress_prototype_scope_pass = FALSE;
  a_scope_orphaned_list_header_ptr solhp;

  check_assertion_str(scope == il_header.primary_scope,
                      "dump_scope_types: scope not file scope");
  /* Do two iterations.  The first outputs declarations for only those types
     that can be declared before they are defined (structs, unions, and enums).
     Enums are output with definitions, since it's nonstandard to put them
     out as forward declarations, and the definitions can't depend on
     other types anyway.  The second pass dumps all types except enums,
     with definitions for structs/unions.  This two-pass process is necessary
     to get the ordering right in the output, because structs/unions/enums
     appear only once on the types list (at the point of definition) even
     though they may appear at several points in the original source
     program. */
  for (pass = 1; pass <= 2; pass++) {
    for (type = scope->types; type != NULL; type = type->next) {
      check_membership_info(type, scope);
      /* Certain types, such as a prototype instantiations, should not be
         processed by the back end. */
      if (!ignore_type_in_back_end(type)) {
        dump_type_decl(type, pass);
      }  /* if */
    }  /* for */
    /* K&R C doesn't have prototype scopes, so when generating K&R C
       promote any types defined in prototype scopes out of those scopes.
       This is done only for types directly associated with entities
       that are put out twice by the C-generating back end (i.e.,
       functions and variables) because the first and second declarations
       have to match.  In other cases, the function type is just put
       out as unprototyped so any types defined in the prototype scope are
       not visible.  When generating ANSI C, we need to do this for types
       that are put out twice, and for the others the type will be defined
       in place in the prototype scope. */
#if 0
    /* These prototype scope types should really be merged with the types
       from the top level of the function, since there can be references
       from one to the other.  That's hard to do, though, because
       the IL entry source position information is incomplete when
       entries come from macro expansions. */
#endif /* 0 */
    /* Types can't be declared/defined in a prototype scope in C++, so don't
       bother with this processing if the source was C++. */
    /* Also suppress the second pass if no prototype scope types were
       found on the first pass. */
    if (il_header.source_language == sl_C && !suppress_prototype_scope_pass) {
      a_boolean      any_found = FALSE;
      a_routine_ptr  rout;
      a_variable_ptr var;
      for (rout = scope->routines; rout != NULL; rout = rout->next) {
        /* This processing is needed even for functions without definitions
           because it's possible to call the function in some cases:
             void f(struct A { int i; } *);
             void m() { f(0); }
           We want to promote the prototype scope types so the cast on the
           call can be written.
        */
        dump_prototype_scope_types_within_type(rout->type, pass, &any_found);
      }  /* for */
      for (var = scope->variables; var != NULL; var = var->next) {
        dump_prototype_scope_types_within_type(var->type, pass, &any_found);
      }  /* for */
      for (var = scope->nonstatic_variables; var != NULL; var = var->next) {
        dump_prototype_scope_types_within_type(var->type, pass, &any_found);
      }  /* for */
      /* If no types were found in prototype scopes on the first pass,
         there's no need for the second pass. */
      if (!any_found) suppress_prototype_scope_pass = TRUE;
    }  /* if */
    /* Put out local types from functions.  This is done to avoid problems
       with extern declarations from inside functions (they are on the
       file-scope lists, but they can reference local types). */
    for (solhp = il_header.scope_orphaned_list_headers;
         solhp != NULL;
         solhp = solhp->next) {
      if (ignore_routine_in_back_end(solhp->assoc_routine)) continue;
      for (type = solhp->orphaned_types;
           type != NULL;
           type = type->next) {
        /* Don't process types that are prototype instantiations. */
        if (ignore_type_in_back_end(type)) continue;
        if (type->kind == (a_type_kind)tk_typeref &&
            type->variant.typeref.has_variably_modified_type) {
          /* Variably-modified types are put out where their stmk_vla_decl
             appears.  They cannot be the type of an entity with linkage, so
             not putting them out here is not a problem. */
#if LOWER_VARIABLE_LENGTH_ARRAYS
          unexpected_condition_str("VLA types should be lowered");
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
        } else {
          if (pass == 1) {
            /* Do some name mangling so that the name remains unique. */
            adjust_promoted_local_type_name(type, solhp->assoc_routine,
                                            solhp->scope_number);
          }  /* if */
          dump_type_decl(type, pass);
        }  /* if */
      }  /* for */
    }  /* for */
  }  /* for */
}  /* dump_scope_types */


static void dump_cast(a_type_ptr type)
/*
Generate a cast to the indicated type.
*/
{
#if C_GEN_BE_GENERATES_ANSI_C
  if (il_header.source_language == sl_C &&
      is_pointer_type(type) &&
      type_contains_prototype_scope_type(type)) {
    /* When generating ANSI C, types defined in prototype scopes are kept.
       Suppress casts to types containing such types, because they can't
       be written (the types defined in prototype scopes cannot be named
       elsewhere).  The cast must have been implicit in the original program.
       Types cannot be defined in prototype scopes in C++, so there's no
       need to check in C++ mode.  When generating K&R C, all function
       declarators that involve a prototype scope are put out as unprototyped
       if the prototype scope has not been examined to promote out types
       defined therein, so a cast to such a type is always writable. */
  } else
#endif /* C_GEN_BE_GENERATES_ANSI_C */
  {
    m_write_tok_ch('(');
    dump_type(type, /*add_pointer_to=*/FALSE);
    m_write_tok_ch(')');
  }  /* if */
}  /* dump_cast */


static void dump_cast_to_pointer_to(a_type_ptr type)
/*
Generate a cast to pointer-to the indicated type.
*/
{
  /* Can't use dump_cast because we don't have the pointer type and
     we can't call make_pointer_type in the "back end". */
  write_tok_ch('(');
  dump_type(type, /*add_pointer_to=*/TRUE);
  write_tok_ch(')');
}  /* dump_cast_to_pointer_to */


static void dump_ampersand(a_type_ptr type)
/*
Output an ampersand to indicate taking the address of something.  However,
if the something (which has type "type") is an array or function, suppress
the ampersand since C will assume one.  This routine assumes the caller will
be putting parentheses around the current code, so that the ampersand will
bind correctly to the entity whose address is taken.
*/
{
  if (is_function_type(type)) {
    /* Do nothing -- there is no need for an ampersand for the address of
       a function. */
  } else if (is_array_type(type)) {
#if C_GEN_BE_GENERATES_ANSI_C
    /* Generating ANSI C. */
    /* For some cases where const qualifiers were removed on variables
       because of initialization, the address of the variable is less-qualified
       than it should be, and in a way that cannot be bridged by an implicit
       conversion in C.  For example:
         void f() {
           int i; i = 1;
           const char a[4] = "abc";
           const char (&r)[4] = a;
         }
       Add a cast to the proper type for that case. */
    a_type_ptr elem_type = underlying_array_element_type(type);
    if (get_top_level_type_qualifiers(elem_type) & TQ_CONST) {
      dump_cast_to_pointer_to(type);
    }  /* if */
    write_tok_ch('&');
#else /* !C_GEN_BE_GENERATES_ANSI_C */
    /* pcc C compilers don't like ampersands in front of arrays.  However,
       the address we want here must have type "pointer-to-array", and
       the implicit decay to pointer will give "pointer-to-array-element",
       so cast the decayed pointer to the right type. */
    dump_cast_to_pointer_to(type);
    if (annotate) {
      start_comment();
      write_tok_ch('&');
      end_comment();
    }  /* if */
#endif /* C_GEN_BE_GENERATES_ANSI_C */
  } else {
    write_tok_ch('&');
  }  /* if */
}  /* dump_ampersand */


static void create_prefix_and_dump_field_name(a_field_ptr field)
/*
Dump the name of the specified field, recursively scanning through base
class subobject members as needed to create the member name prefix.
*/
{
  if (field->class_subobject_with_tail_padding) {
    /* The specified field represents the subobject for a base class or a
       no_unique_address field of class type and that base or field class
       type has tail padding.  This field does not exist in the generated
       derived class, so we transform this reference into a reference to
       the (mangled) name of the first member of the subobject class type,
       using recursion to build the mangling prefix by traversing the
       fields of the subobject class type. */
    a_member_name_prefix_component prefix;
    push_member_name_prefix_component(&prefix, field);
    /* Use the (mangled) name of the first non-empty member. */
    create_prefix_and_dump_field_name(next_non_empty_initializable_field(
           skip_typerefs(field->type)->variant.class_struct_union.field_list));
    pop_member_name_prefix_component(&prefix);
  } else {
    dump_field_name(field);
  }  /* if */
}  /* create_prefix_and_dump_field_name */


static void dump_field_from_second_operand(an_expr_node_ptr node)
/*
Dump the name of the field from the second operand under node (a field
selection operation).
*/
{
  an_expr_node_ptr second_operand = node->variant.operation.operands->next;
  a_field_ptr      field;

  check_assertion_str(second_operand->kind == (an_expr_node_kind)enk_field,
                    "dump_field_from_second_operand: operand 2 not enk_field");
  field = node_field(second_operand);
#if CHECKING
  { a_type_ptr field_class = parent_class_of(field);
    a_type_ptr struct_class;
    an_expr_operator_kind op;
    if (field_class == NULL) {
      internal_error("dump_field_from_second_operand: field class is NULL");
    }  /* if */
    /* Check that the field comes from the struct indicated by the first
       operand. */
    op = node->variant.operation.kind;
    if (op == (an_expr_operator_kind)eok_dot_field) {
      /* Left operand is an lvalue/rvalue of the class type. */
      struct_class = node->variant.operation.operands->type;
    } else if (op == (an_expr_operator_kind)eok_points_to_field) {
      /* Left operand is a pointer to the object. */
      struct_class = type_pointed_to(node->variant.operation.operands->type);
    } else {
      unexpected_condition_str(
                   "dump_field_from_second_operand: unexpected operator kind");
    }  /* if */
    struct_class = skip_typerefs(struct_class);
    if (struct_class != field_class) {
      internal_error("dump_field_from_second_operand: wrong field class");
    }  /* if */
  }
#endif /* CHECKING */
  create_prefix_and_dump_field_name(field);
}  /* dump_field_from_second_operand */


static void dump_variable_reference_node(an_expr_node_ptr node)
/*
Output a reference to the variable indicated by the given enk_variable
node.  The output is usually just the variable name.
*/
{
  a_variable_ptr var = node_variable(node);

  if (var->superseded_external) {
    /* Superseded variable (there are multiple incompatible block-scope
       extern declarations in SVR4 C mode, but they're all promoted to
       the file scope).  Only the primary declaration is put out, so
       references to the others need a cast to the right type. */
    write_tok_str("(*");
    dump_cast_to_pointer_to(var->type);
    dump_ampersand(var->type);
    dump_variable_name(var);
    write_tok_str(")");
  } else {
    /* Normal case.  Just put out the variable name. */
    dump_variable_name(var);
  }  /* if */
}  /* dump_variable_reference_node */


static void dump_va_arg(an_expr_node_ptr expr)
/*
Dump a va_arg operator.  Used when <stdarg.h> is treated as a builtin.
*/
{
  an_expr_node_ptr operand_1 = expr->variant.operation.operands;
  a_type_ptr       type = expr->type;

  disable_line_wrapping();
  if (gcc_builtin_varargs_in_generated_code) {
    /* Use the intrinsic GNU C/C++ "__builtin_va_arg". */
    write_tok_str("__builtin_va_arg(");
  } else {
    write_tok_str("va_arg(");
  }  /* if */
  dump_expr_with_parens(operand_1);
  write_tok_ch(',');
  dump_type(type, /*add_pointer_to=*/FALSE);
  write_tok_ch(')');
  enable_line_wrapping();
}  /* dump_va_arg */


static a_boolean optimizable_rvalue_selection(an_expr_node_ptr expr,
                                              a_boolean        *comma_case);


static a_boolean obj_expr_based_on_comma(an_expr_node_ptr expr)
/*
Return TRUE if expr is an eok_dot_field node whose left operand either is
or is a member selection from the result of an eok_comma operation.
*/
{
  a_boolean result = FALSE;

  while (is_operation_node(expr) &&
         node_operator_is(expr, eok_dot_field)) {
    /* Examine the object expression (and, possibly, set up for another
       iteration of the loop). */
    expr = expr->variant.operation.operands;
    if (is_operation_node(expr) && node_operator_is(expr, eok_comma)) {
      result = TRUE;
    }  /* if */
  }  /* while */
  return result;
}  /* obj_expr_based_on_comma */


static void dump_lvalue_cast(an_expr_node_ptr node,
                             a_boolean        suppress_indirection)
/*
Dump an expression that is an lvalue cast (eok_lvalue_cast or
eok_lvalue_adjust).  If suppress_indirection is TRUE, suppress the "*"
on top of the expansion.
*/
{
  a_boolean        bit_field_case = FALSE;
  an_expr_node_ptr operand_1 = node->variant.operation.operands;
  a_boolean        suppress_ampersand = FALSE;
  an_expr_node_ptr object_expr = NULL;
  a_targ_size_t    field_offset = 0;

  write_tok_ch('(');
  /* Generate the lvalue cast as an indirection on a pointer cast.
     This avoids depending too much on the underlying compiler's
     implementation of lvalue casts.  Note that this will not work for
     a bit-field, so generate a special-purpose expansion for those cases. */
  if (is_operation_node(operand_1) &&
      (node_operator_is(operand_1, eok_dot_field) ||
       node_operator_is(operand_1, eok_points_to_field))) {
    a_field_ptr      field;
    a_boolean        comma_case;
    a_targ_size_t    sub_object_offset = 0;
    object_expr = operand_1->variant.operation.operands;
    check_assertion(object_expr->next->kind == (an_expr_node_kind)enk_field);
    field = node_field(object_expr->next);
    while (field->class_subobject_with_tail_padding) {
      /* The specified field represents the subobject for a base class or a
         no_unique_address field of class type and that base or field class
         type has tail padding.  This field does not exist in the generated
         derived class, so we transform this reference into a reference to
         the first member of the base or field class type (skipping empty
         classes).  See create_prefix_and_dump_field_name for details. */
      sub_object_offset += field->offset;
      field = next_non_empty_initializable_field(
            skip_typerefs(field->type)->variant.class_struct_union.field_list);
    }  /* if */
    if (field->is_bit_field) {
      if (!node->is_lvalue && !node->is_xvalue) {
        /* The node has been rvalued, effectively making it a no-op.  Just
           render the underlying expression because the alternative treatment
           below will not work for rvalued accesses to bit-fields. */
        dump_expression(operand_1);
        goto after_operand_output;
      }  /* if */
      bit_field_case = TRUE;
      field_offset = sub_object_offset + field->offset;
    } else if (node_operator_is(operand_1, eok_dot_field) &&
               ((!object_expr->is_lvalue &&
                 (!optimizable_rvalue_selection(operand_1, &comma_case) ||
                  comma_case)) ||
                (object_expr->is_lvalue &&
                 obj_expr_based_on_comma(operand_1)))) {
      /* The operand is a member access expression that will be generated
         as a comma expression, to which "&" cannot be applied.  Signal
         that the ampersand should be put out on the second operand of the
         comma expression. */
      suppress_ampersand = TRUE;
      operand_1->variant.operation.has_deferred_ampersand = TRUE;
    }  /* if */
  }  /* if */
  if (!suppress_indirection) write_tok_ch('*');
  if (node->orig_lvalue_type != NULL) {
    /* If we saved the original lvalue type, use that. */
    dump_cast_to_pointer_to(node->orig_lvalue_type);
  } else if (!node->is_lvalue &&
             is_function_type(operand_1->type) &&
             is_pointer_type(node->type) &&
             is_function_type(type_pointed_to(node->type))) {
    /* If an eok_lvalue_adjust has an implicit lvalue-to-rvalue conversion
       built in, and the underlying lvalue is a function, the decay to
       rvalue adds a "pointer-to" to the type. */
    dump_cast(node->type);
  } else {
    dump_cast_to_pointer_to(node->type);
  }  /* if */
  if (bit_field_case) {
    char buf[32];
    /* It is not permitted to take the address of a bit-field, so use
       pointer arithmetic on the address of the containing object. */
    write_tok_str("(((char *)");
    if (node_operator_is(operand_1, eok_dot_field)) {
      /* Take the address of the object expression. */
      check_assertion(object_expr->is_lvalue);
      write_tok_ch('&');
    }  /* if */
    dump_expr_with_parens(object_expr);
    write_tok_ch(')');
    if (field_offset != 0) {
      /* Add the offset of the bit-field to the address of the object to
         get the pointer to be cast to the target type. */
      (void)unsigned_to_string_buf((a_host_large_unsigned)field_offset, buf);
      write_tok_ch('+');
      write_tok_str(buf);
    }  /* if */
    write_tok_ch(')');
  } else {
    if (is_operation_node(operand_1) &&
        node_operator_is(operand_1, eok_indirect)) {
      /* Cancel "&" over "*" to avoid a gcc bug when the underlying type
         is incomplete. */
      operand_1 = operand_1->variant.operation.operands;
    } else if (is_operation_node(operand_1) &&
               (node_operator_is(operand_1, eok_lvalue_cast) ||
                node_operator_is(operand_1, eok_lvalue_adjust))) {
      /* Cancel "&" over the "*" at the top of an lvalue cast to avoid a
         gcc bug when the underlying type is incomplete. */
      dump_lvalue_cast(operand_1, /*suppress_indirection=*/TRUE);
      goto after_operand_output;
    } else if (!suppress_ampersand) {
      write_tok_ch('&');
    }  /* if */
    dump_expr_with_parens(operand_1);
  }  /* if */
after_operand_output:
  write_tok_ch(')');
}  /* dump_lvalue_cast */


static a_boolean optimizable_rvalue_selection(an_expr_node_ptr expr,
                                              a_boolean        *comma_case)
/*
Return TRUE if the first operand of the given expression (an eok_dot_field
operation whose first operand is an rvalue) has one of the forms
  variable
  (something, lvalue-expression)
  *expression
*comma_case is returned TRUE to indicate the second case.  These forms can
be optimized by dump_field_selection.  (Some forms are generated only by IL
lowering and do not occur in the unlowered IL.)
*/
{
  a_boolean        optimizable = FALSE;
  an_expr_node_ptr struct_expr, comma_operand_2;

  check_assertion(is_operation_node(expr) &&
                  node_operator_is(expr, eok_dot_field) &&
                  !expr->variant.operation.operands->is_lvalue);
  *comma_case = FALSE;
  struct_expr = expr->variant.operation.operands;
  if (is_variable_node(struct_expr)) {
    /* The field is being selected from a simple variable (IL lowering
       generates some cases like this for pointer-to-member calls). */
    optimizable = TRUE;
  } else if (is_operation_node(struct_expr)) {
    an_expr_operator_kind op = struct_expr->variant.operation.kind;
    if (op == (an_expr_operator_kind)eok_comma) {
      /* The first operand is a comma expression. */
      /* Check for a second operand of the comma expression that is an
         lvalue. */
      comma_operand_2 = struct_expr->variant.operation.operands->next;
      if (comma_operand_2->is_lvalue) {
        optimizable = TRUE;
        *comma_case = TRUE;
      }  /* if */
    } else if (op == (an_expr_operator_kind)eok_indirect) {
      /* The first operand is *expression, so we can easily refer to it
         as an lvalue. */
      optimizable = TRUE;
    }  /* if */
  }  /* if */
  return optimizable;
}  /* optimizable_rvalue_selection */


static void dump_field_selection(an_expr_node_ptr expr)
/*
Generate code for expr, a field selection operation, i.e., eok_dot_field or
eok_points_to_field.  It is assumed that the caller will surround the
output with parentheses if needed.
*/
{
  an_expr_node_ptr struct_expr;
  a_field_ptr      field;
  a_boolean        mutable_case = FALSE;
  a_boolean        need_closing_paren = FALSE;
  a_boolean        promoted_bit_field_case = FALSE;

  check_assertion(is_operation_node(expr) &&
                  (node_operator_is(expr, eok_dot_field) ||
                   node_operator_is(expr, eok_points_to_field)));
  struct_expr = expr->variant.operation.operands;
  field = node_field(struct_expr->next);
  if (field->class_subobject_with_tail_padding) {
    /* The field represents a base class subobject or a no_unique_address
       field of class type and the base or field class type has tail
       padding.  The fields of such subobjects are promoted into the class
       of which the subobject is a member, so the referenced field does not
       exist in the generated code, so dump_field_from_second_operand
       translates a reference to the field into a reference to the first
       member of the class type of the subobject.  That will be the correct
       offset for the member, but the type will be wrong, so we need to add
       code that will take the address of the referenced member, cast it to
       a pointer to the subobject class type, and dereference that pointer,
       in order to make this field selection equivalent to what it would
       have been if the class members had not been promoted into the
       containing class.  For example, C++

           struct B { int i; };
           struct D: B { };

       becomes in the lowered IL

           struct D { struct B __b_1B; };

       and in the generated C code

           struct D { int __b_1B_i; };

       A field selection like dptr->__b_1B (calculating the "this" pointer
       for a B member function, for example) will be generated as

           (*(struct B*)&(dptr->__b_1B_i))

       This pattern does not work, however, if the first field of the base
       class is a bit-field, since it is not permitted to take the address
       of a bit-field.  In this case, the expression dptr->__b_1B is
       generated as

           (*(struct B*)(((char*)dptr)+N))

       where N is the offset of __b_1B in D, and x.__b_1B becomes

           (*(struct B*)(((char*)&x)+N))
       */
    a_type_ptr  subobj_class = skip_typerefs(field->type);
    a_field_ptr first_subobj_field;
    while ((first_subobj_field =
                subobj_class->variant.class_struct_union.field_list) != NULL) {
      if (first_subobj_field->class_subobject_with_tail_padding) {
        /* The field is an indirect class subobject whose members have also
           been promoted into the containing class object.  Look at the
           first field of the indirect class. */
        subobj_class = skip_typerefs(first_subobj_field->type);
      } else {
        promoted_bit_field_case = first_subobj_field->is_bit_field;
        break;
      }  /* if */
    }  /* while */
    write_tok_str("(*(");
    dump_type(field->type, /*add_pointer_to=*/TRUE);
    if (promoted_bit_field_case) {
      if (node_operator_is(expr, eok_dot_field)) {
        write_tok_str(")(((char*)&");
      } else {
        write_tok_str(")(((char*)");
      }  /* if */
    } else {
      write_tok_str(")&(");
    }  /* if */
  }  /* if */
  if (node_operator_is(expr, eok_dot_field) &&
      (!struct_expr->is_lvalue ||
       expr->variant.operation.has_deferred_ampersand)) {
    a_boolean comma_case;
    /* Because pcc compilers do not allow selection of a field from an
       rvalue struct (which is allowed in ANSI C), in most cases we
       generate this code in the form
         (_T123456 = expr, _T123456.field)
       The temporary has been generated on a pre-scan of this code.
       If the struct expression is just a variable, the field selection is
       added directly to the variable.  If the struct expression looks like
         (expr2, variable)
       (which happens, for example, when a temporary is introduced to hold the
       return value of a function returning a struct), the transformation is
       done by changing that to
         (expr2, variable.field)
       If the struct expression looks like
         *expr3
       the field selection is added directly to the expression.
    */
    write_tok_ch('(');
    need_closing_paren = TRUE;
    if (!expr->variant.operation.operands->is_lvalue &&
        optimizable_rvalue_selection(expr, &comma_case)) {
      /* This is an optimizable case.  Add the field selection to the existing
         reference to a struct/union variable. */
      if (comma_case) {
        /* (expr2, expr).field --> (expr2, expr.field) */
        an_expr_node_ptr comma_operand_1 =
                                       struct_expr->variant.operation.operands;
        an_expr_node_ptr comma_operand_2 = comma_operand_1->next;
        dump_expr_with_parens(comma_operand_1);
        write_tok_str(", ");
        if (expr->variant.operation.has_deferred_ampersand) {
          /* The address of the member is needed, but "&" cannot be applied
             to a comma expression so it must now be put out for the second
             operand of the comma. */
          write_tok_ch('&');
        }  /* if */
        dump_expr_with_parens(comma_operand_2);
      } else {
        if (struct_expr->kind == (an_expr_node_kind)enk_variable) {
          /* (variable).field -> variable.field, a normal C case. */
          dump_expression(struct_expr);
        } else {
          /* Something like (*expr3).field --> (*expr3).field */
          dump_expr_with_parens(struct_expr);
        }  /* if */
      }  /* if */
    } else {
      /* Normal non-optimizable case.  Assign the struct/union value to
         a temporary and select from the temporary. */
      dump_temp_name((char *)expr);
      write_tok_str(" = ");
      dump_expr_with_parens(struct_expr);
      write_tok_str(", ");
      if (expr->variant.operation.has_deferred_ampersand) {
        /* The address of the member is needed, but "&" cannot be applied
           to a comma expression so it must now be put out for the second
           operand of the comma. */
        write_tok_ch('&');
      }  /* if */
      dump_temp_name((char *)expr);
    }  /* if */
    if (!promoted_bit_field_case) {
      write_tok_ch('.');
    }  /* if */
  } else {
    a_type_ptr unqual_underlying_type = NULL;
    /* Look for a field selection of a mutable field from a const
       structure.  (This cannot occur in the rvalue case above because
       lowering of class rvalues removes cv-qualification.)  A cast to
       remove the const must be added to the address of the struct in that
       case so that the resulting selected field will be nonconst. */
    if (field->is_mutable) {
      a_type_ptr underlying_struct_type;
      if (node_operator_is(expr, eok_points_to_field)) {
        underlying_struct_type = type_pointed_to(struct_expr->type);
      } else {
        underlying_struct_type = struct_expr->type;
      }  /* if */
      if (is_const_qualified_type(underlying_struct_type)) {
        mutable_case = TRUE;
        unqual_underlying_type = f_skip_typerefs(underlying_struct_type);
      }  /* if */
    }  /* if */
    if (node_operator_is(expr, eok_points_to_field) || mutable_case) {
      if (mutable_case) {
        /* For the mutable case, cast away const on the struct address. */
        write_tok_ch('(');
        dump_cast_to_pointer_to(unqual_underlying_type);
        if (node_operator_is(expr, eok_dot_field)) {
          /* This was originally "x.y", so we need to convert the left
             operand into a pointer value. */
          if (is_operation_node(struct_expr) &&
              node_operator_is(struct_expr, eok_indirect)) {
            /* We can just skip the "*" to get the pointer value. */
            struct_expr = struct_expr->variant.operation.operands;
          } else {
            /* Take the address of the left operand. */
            write_tok_ch('&');
          }  /* if */
        }  /* if */
      }  /* if */
      dump_expr_with_parens(struct_expr);
      if (mutable_case) write_tok_ch(')');
      if (!promoted_bit_field_case) {
        write_tok_str("->");
      }  /* if */
    } else {
      /* "." case. */
      dump_expr_with_parens(struct_expr);
      if (!promoted_bit_field_case) {
        write_tok_ch('.');
      }  /* if */
    }  /* if */
  }  /* if */
  if (!promoted_bit_field_case) {
    dump_field_from_second_operand(expr);
  }  /* if */
  if (need_closing_paren) {
    write_tok_ch(')');
  }  /* if */
  if (field->class_subobject_with_tail_padding) {
    if (promoted_bit_field_case) {
      write_tok_ch(')');
      if (field->offset != 0) {
        write_tok_ch('+');
        write_unsigned_num((a_host_large_unsigned)field->offset);
      }  /* if */
    }  /* if */
    write_tok_str("))");
  }  /* if */
}  /* dump_field_selection */

#if !C_GEN_BE_GENERATES_ANSI_C

static void adjust_bit_field_value(an_expr_node_ptr node)
/*
node is an operation that returns an rvalue (e.g., an assignment).
If its first operand is a bit field, generate code to truncate and
sign-extend the result of the operation to match the bit field size
and signedness.  This is needed when generating K&R C to simulate
signed bit fields under pcc, which does not support them.
*/
{
  an_expr_node_ptr operand;

  /* No need to add this code if the result of the operation is not used. */
  if (!node->result_is_not_used) {
    /* See if the lvalue operand is a bit field. */
    operand = node->variant.operation.operands;
    if (is_operation_node(operand) &&
        (node_operator_is(operand, eok_dot_field) ||
         node_operator_is(operand, eok_points_to_field))) {
      a_field_ptr dest_field =
                        node_field(operand->variant.operation.operands->next);
      if (dest_field->is_bit_field) {
        /* For this case, we need to truncate the result of the assignment
           because pcc does not do it.  For a signed bit field, use __sexten;
           for an unsigned bit field, use ((i)&((1<<n)-1)). */
        if (dest_field->bit_field_is_signed) {
          write_tok_str("(__sexten((");
        } else {
          write_tok_str("((");
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* adjust_bit_field_value */

#endif /* !C_GEN_BE_GENERATES_ANSI_C */
#if !C_GEN_BE_GENERATES_ANSI_C

static void end_adjust_bit_field_value(an_expr_node_ptr node)
/*
Second half of the job begun in adjust_bit_field_lvalue; puts out the
closing parentheses needed if any code was generated there.
*/
{
  an_expr_node_ptr operand;

  /* No need to add this code if the result of the operation is not used. */
  if (!node->result_is_not_used) {
    /* See if the lvalue operand is a bit field. */
    operand = node->variant.operation.operands;
    if (is_operation_node(operand) &&
        (node_operator_is(operand, eok_dot_field) ||
         node_operator_is(operand, eok_points_to_field))) {
      a_field_ptr dest_field =
                        node_field(operand->variant.operation.operands->next);
      if (dest_field->is_bit_field) {
        if (dest_field->bit_field_is_signed) {
          /* End of __sexten call. */
          write_tok_str("),");
          write_unsigned_num((a_host_large_unsigned)dest_field->bit_size);
          write_tok_str("))");
        } else {
          /* End of truncation code: ((i)&((1<<n)-1)). */
          write_tok_str(")&((1<<");
          write_unsigned_num((a_host_large_unsigned)dest_field->bit_size);
          write_tok_str(")-1))");
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* end_adjust_bit_field_value */

#endif /* !C_GEN_BE_GENERATES_ANSI_C */
#if !C_GEN_BE_GENERATES_ANSI_C

static a_boolean expr_is_zero_constant(an_expr_node_ptr expr)
/*
Return TRUE if the indicated expression is a zero constant.
*/
{
  a_boolean is_zero =
           (expr->kind == (an_expr_node_kind)enk_constant &&
            node_constant_is(expr, ck_integer) &&
            cmplit_integer_constant(node_constant(expr),
                                    (a_host_large_integer)0) == 0);
  return is_zero;
}  /* expr_is_zero_constant */

#endif /* !C_GEN_BE_GENERATES_ANSI_C */


static void dump_routine_address(an_expr_node_ptr expr)
/*
Generate code for an enk_routine expression node, i.e., the name of a
routine.
*/
{
  a_routine_ptr rout = expr->variant.routine.ptr;
  a_type_ptr    rout_type = rout->type;
  a_type_ptr    expr_rout_type = (expr->is_lvalue) ? expr->type :
                                                   type_pointed_to(expr->type);
  a_boolean     need_parens = FALSE;

  if (!standalone_identical_types(expr_rout_type, rout_type)) {
    /* The type of the routine and the type in the call are different.
       This is probably because the call was generated and then
       the routine type was updated by a redeclaration.  Use a cast to
       be sure to get the required type at this point. */
    write_tok_ch('(');
    need_parens = TRUE;
    dump_cast_to_pointer_to(expr_rout_type);
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED && BUILTIN_FUNCTIONS_ENABLED
  if (rout->implicit_alias &&
      rout->special_kind == (a_special_function_kind)sfk_none &&
      rout->variant.builtin_function_kind !=
                                           (a_builtin_function_kind)bfk_none) {
    /* If this routine is an alias for a builtin, use the aliased routine
       name (used for __builtin_operator_new and __builtin_operator_delete
       in clang mode). */
    rout = gnu_routine_supp(rout)->aliased_routine;
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED && BUILTIN_FUNCTIONS_ENABLED */
  dump_routine_name(rout);
  if (need_parens) write_tok_ch(')');
}  /* dump_routine_address */

#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN

static void dump_result_of_overriding_function(void)
/*
Generate code for an enk_result_of_overriding_function node, which is
generated as part of the body of an entry function used as a wrapper
for a call of an overriding virtual function with a covariant return type,
and also for thunks in the IA-64 ABI.  Note that some
enk_result_of_overriding_function nodes are expanded directly in the wrapper
and don't reach here.
*/
{
  /* The overriding function body is not being expanded in the
     wrapper, so this node represents a call of the underlying
     function with arguments that are the parameters of this routine. */
  a_variable_ptr param;
  a_routine_ptr  curr_routine =
                               innermost_function_scope->variant.routine.ptr;
  a_routine_ptr  underlying_routine =
                               curr_routine->overriding_function_for_wrapper;
  check_assertion(underlying_routine != NULL);
  write_tok_ch('(');
  dump_routine_name(underlying_routine);
  write_tok_ch('(');
  for (param = innermost_function_scope->variant.routine.parameters;
       param != NULL;
       param = param->next) {
    dump_variable_name(param);
    if (param->next != NULL) write_tok_str(", ");
  }  /* for */
  write_tok_str("))");
}  /* dump_result_of_overriding_function */

#endif /* ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */

static a_boolean suppress_const_for_mutable_or_init(a_variable_ptr var)
/*
Return TRUE if the specified variable must be declared as non-const because
its type is a class with a mutable member or because its initialization will
be performed as executable code.
*/
{
  a_boolean      suppress_const = FALSE;
  a_type_ptr     var_type = var->type;
  a_type_ptr     underlying_var_type = var_type;
  a_constant_ptr init_con;
  an_init_kind   init_kind;

  init_con = constant_initializer(var, &init_kind);
  if (is_array_type(var_type)) {
    underlying_var_type = underlying_array_element_type(var_type);
  }  /* if */
  underlying_var_type = skip_typerefs(underlying_var_type);
  if (is_class_struct_union_type(underlying_var_type) &&
      underlying_var_type->variant.class_struct_union.any_mutable_member) {
    /* When the variable has a class type with a mutable member,
       suppress const so the variable will not be put into read-only
       storage. */
    suppress_const = TRUE;
  } else if (var->initialization_rewritten_as_assignment ||
             (init_kind == (an_init_kind)initk_dynamic &&
              init_con == NULL)) {
    /* When generating ANSI C, "const" will be put out.  However, if
       the variable's initialization was turned into executable code
       (either here or in IL lowering), the initialization code is
       going to have problems assigning to a "const" entity.  For those
       cases, suppress the "const" from the variable type. */
    suppress_const = TRUE;
  }  /* if */
  return suppress_const;
}  /* suppress_const_for_mutable_or_init */


/*
The variable found while traversing the expression passed to
variable_referenced_by_lvalue (see below).
*/
STATIC_THREAD a_variable_ptr
                var_seen_during_lvalue_traversal;


static void set_var_in_lvalue_traversal(
                                    an_expr_node_ptr                    expr,
                                    an_expr_or_stmt_traversal_block_ptr tblock)
/*
Called from variable_referenced_by_lvalue via traverse_expr.  If expr is
an enk_variable node with a struct or array type (i.e., a variable that
has subobjects), set var_seen_during_lvalue_traversal to point to that
variable and terminate the traversal.
*/
{
  if (is_variable_node(expr)) {
    if (expr->is_lvalue) {
      /* The processing for tblock->follow_addressing_path will visit
         a pointer variable in a member access expression ("p" in "p->x").
         That case is not of interest here, but it can be avoided by
         requiring that the enk_variable node be an lvalue. */
      a_variable_ptr var = node_variable(expr);
      if (is_array_type(var->type) ||
          is_class_struct_union_type(var->type)) {
        /* This routine is called for the purpose of determining whether a
           cast to const is needed because the declaration of a const
           variable was changed to non-const to allow for initialization
           (see the comments in case eok_address_of in dump_expr).  An
           explicit cast is never required for a variable of scalar type
           because such variables can be implicitly converted to a const
           type in the generated C code; we therefore only record variables
           of array and struct types, where an explicit cast might be
           needed. */
        var_seen_during_lvalue_traversal = var;
        tblock->terminate = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
}  /* set_var_in_lvalue_traversal */


static a_variable_ptr variable_referenced_by_lvalue(an_expr_node_ptr expr)
/*
Walk down the expression tree rooted in expr, which must be an lvalue,
looking for an enk_variable node designating a variable that is or contains
the object to which the lvalue refers.  If such a node is found, return a
pointer to the variable; otherwise, return NULL.
*/
{
  an_expr_or_stmt_traversal_block tblock;

  check_assertion(expr->is_lvalue);
  var_seen_during_lvalue_traversal = NULL;
  clear_expr_or_stmt_traversal_block(&tblock);
  tblock.process_expr = set_var_in_lvalue_traversal;
  tblock.follow_addressing_path = TRUE;
  traverse_expr(expr, &tblock);
  return var_seen_during_lvalue_traversal;
}  /* variable_referenced_by_lvalue */

#if CHECKING 
/*
The variable or routine (if any) found while traversing the expression
passed to check_address_taken_flag (see below).
*/
STATIC_THREAD a_variable_ptr
                var_for_address_taken_check;
STATIC_THREAD a_routine_ptr
                rout_for_address_taken_check;


static void set_target_of_addressing_op(
                                    an_expr_node_ptr                    expr,
                                    an_expr_or_stmt_traversal_block_ptr tblock)
/*
Called from check_address_taken_flag via traverse_expr.  If expr is an
lvalue enk_variable node or an enk_routine node, set
var_for_address_taken_check or rout_for_address_taken_check, respectively,
and terminate the traversal.
*/
{
  if (is_variable_node(expr)) {
    if (expr->is_lvalue) {
      /* We only want lvalues -- i.e., "x.i" and not "p->i". */
      var_for_address_taken_check = node_variable(expr);
    }  /* if */
    tblock->terminate = TRUE;
  } else if (is_routine_node(expr)) {
    rout_for_address_taken_check = node_routine(expr);
    tblock->terminate = TRUE;
  }  /* if */
}  /* set_target_of_addressing_op */


static void check_address_taken_flag(an_expr_node_ptr expr)
/*
Check to make sure that a variable or routine referenced by expr, which is
an addressing operation like eok_address_of or eok_array_to_pointer, has
its address_taken flag set.
*/
{
  an_expr_or_stmt_traversal_block tblock;

  var_for_address_taken_check = NULL;
  rout_for_address_taken_check = NULL;
  clear_expr_or_stmt_traversal_block(&tblock);
  tblock.process_expr = set_target_of_addressing_op;
  tblock.follow_addressing_path = TRUE;
  traverse_expr(expr, &tblock);
  if ((var_for_address_taken_check != NULL &&
       !var_for_address_taken_check->address_taken) ||
      (rout_for_address_taken_check != NULL &&
       !rout_for_address_taken_check->address_taken)) {
#if DEBUG && !STANDALONE_C_GEN_BE
    db_expression(expr);
#endif /* DEBUG && !STANDALONE_C_GEN_BE */
    internal_error("check_address_taken_flag: address_taken is FALSE");
  }  /* if */
}  /* check_address_taken_flag */

#if !STANDALONE_C_GEN_BE

static void check_type_of_variable_node(an_expr_node_ptr expr)
/*
Check the type of a variable node to make sure it is consistent with the
type of the variable to which it refers.
*/
{
  a_type_ptr     tp = expr->type, expected_type;
  a_variable_ptr vp;
  a_boolean      types_match;

  check_assertion_str(is_variable_node(expr),
                      "check_type_of_variable_node: wrong kind of node");
  vp = node_variable(expr);
  expected_type = vp->type;
  if (is_array_type(tp) && is_array_type(expected_type) &&
      (is_incomplete_array_type(tp) ||
       is_incomplete_array_type(expected_type))) {
    /* In some cases (block extern declarations, late template
       instantiation, designated initializers, etc.) a variable type
       and the result type of an enk_variable may differ in the
       presence or absence of a major array bound.  In such cases, we
       compare the element type instead. */
    tp = array_element_type(tp);
    expected_type = array_element_type(expected_type);
  } else if (vp->is_template_param_object) {
    /* Uses of a template parameter object have an implicitly "const" type. */
    expected_type = make_qualified_type(expected_type, TQ_CONST);
  } else if (!expr->is_lvalue) {
    /* Qualifiers are dropped on rvalues.  (We don't have to do this for
       types that fell into the preceding case because the lvalue-to-rvalue
       conversion would have caused the array type to decay to a pointer,
       so the cases are mutually exclusive.) */
    expected_type = make_unqualified_type(expected_type);
  } else if (vp->is_compound_literal && (clang_mode || gnu_mode)) {
    /* The variable holding a compound literal in GNU and Clang mode can be
       "const" (to model immutability) even though the compound-literal
       expression is not). */
    expected_type = make_unqualified_type(expected_type);
    tp = make_unqualified_type(tp);
  }  /* if */
  types_match = il_identical_types(tp, expected_type);
  if (!types_match) {
    /* Under some conditions, a variable that refers to a typeinfo
       class can have its type changed to refer to a different class
       after the generation of an enk_variable node, so that the
       node's type still refers to the original class.  We work
       around that case by considering two class types to be a match
       if their names both begin with "__" (making them reserved
       names that should not appear in user code). */
    tp = skip_typerefs(tp);
    expected_type = skip_typerefs(expected_type);
    if (is_immediate_class_type(tp) &&
        is_immediate_class_type(expected_type)) {
      a_const_char *name1 = tp->source_corresp.name;
      a_const_char *name2 = expected_type->source_corresp.name;
      if (name1 != NULL && name2 != NULL &&
          name1[0] == '_' && name1[1] == '_' &&
          name2[0] == '_' && name2[1] == '_') {
        types_match = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (!types_match) {
#if DEBUG
    db_expression(expr);
#endif /* DEBUG */
    internal_error("check_type_of_variable_node: enk_variable has wrong type");
  }  /* if */
}  /* check_type_of_variable_node */

#endif /* !STANDALONE_C_GEN_BE */
#endif /* CHECKING */

static a_boolean cast_to_ptr_to_empty_struct(a_type_ptr type,
                                             a_boolean  *parens_needed)
/*
If the given type (the result type of an expression) is a pointer to a
struct that has been generated as empty, put out a cast to the specified
type and return TRUE.  If *parens_needed is FALSE, precede the cast with
a left parenthesis and set *parens_needed to TRUE.  This is used to
reverse the effect of casting operands of the expression to char * to
allow them to be used in pointer arithmetic in spite of the zero size.
*/
{
  a_boolean did_cast = FALSE;

  if (is_pointer_type(type) &&
      f_skip_typerefs(type_pointed_to(type))->generated_as_empty_struct) {
    if (!*parens_needed) {
      /* We must supply parentheses to enclose the added cast. */
      *parens_needed = TRUE;
      write_tok_ch('(');
    }  /* if */
    dump_cast(type);
    write_tok_ch('(');
    did_cast = TRUE;
  }  /* if */
  return did_cast;
}  /* cast_to_ptr_to_empty_struct */


static void dump_lvalue_ptr_to_empty_struct(an_expr_node_ptr expr)
/*
expr is an lvalue node whose type is a pointer to a struct that has been
generated as empty.  In order for this lvalue to be usable in pointer
arithmetic expressions, it must be cast to an lvalue pointer to char *,
which this function does.
*/
{
  check_assertion(is_pointer_type(expr->type) &&
                  f_skip_typerefs(type_pointed_to(expr->type))->
                                                   generated_as_empty_struct &&
                  expr->is_lvalue);
  write_tok_str("(*(char **)&");
  dump_expr_with_parens(expr);
  write_tok_ch(')');
}  /* dump_lvalue_ptr_to_empty_struct */


static void dump_possible_ptr_to_empty_struct(an_expr_node_ptr expr)
/*
expr is an operand of a pointer arithmetic operation (it need not be the
pointer operand).  If it is a pointer to struct that has been generated as
empty, cast it to char * to give it the right element size for the
computation.  Otherwise, just dump the expression normally.
*/
{
  if (is_pointer_type(expr->type) &&
      f_skip_typerefs(type_pointed_to(expr->type))->
                                                   generated_as_empty_struct) {
    write_tok_str("((char *)");
    dump_expr_with_parens(expr);
    write_tok_ch(')');
  } else {
    dump_expr_with_parens(expr);
  }  /* if */
}  /* dump_possible_ptr_to_empty_struct */

#if SUNPRO_C_IS_C_GEN_BE_TARGET

static an_expr_node_ptr skip_comma_nodes(an_expr_node_ptr  opnd)
/*
If opnd is an expression node representing one or more consecutive comma
operators, return the right operand of the innermost comma node.  Otherwise,
return opnd.
*/
{
  while (opnd->kind == (an_expr_node_kind)enk_operation &&
         opnd->variant.operation.kind == (an_expr_operator_kind)eok_comma) {
    opnd = opnd->variant.operation.operands->next;
  }  /* while */
  return opnd;
}  /* skip_comma_nodes */

static void adjust_question_operand_if_necessary(an_expr_node_ptr  opnd,
                                                 an_expr_node_ptr  other_opnd)
/*
The SUNPRO C compiler doesn't like "?" operators where the branches are struct
rvalues with different type qualifiers.  That's a bug -- in standard C the
qualifiers on rvalues are dropped.  For a few simple cases, do some casting
to drop the type qualifiers on the operand indicated by opnd (which is the
second or third operand of a "?" operator; other_opnd is the other operand).
*/
{
  an_expr_node_ptr  past_commas = skip_comma_nodes(opnd),
                    other_past_commas = skip_comma_nodes(other_opnd);
  a_type_ptr        tp = past_commas->orig_lvalue_type,
                    other_tp = other_past_commas->orig_lvalue_type;

  if (tp == NULL) tp = past_commas->type;
  if (other_tp == NULL) other_tp = other_past_commas->type;
  if (get_top_level_type_qualifiers(tp) ==
                                    get_top_level_type_qualifiers(other_tp)) {
    /* The qualifiers match: Nothing to do. */
  } else if (is_class_struct_union_type(opnd->type)) {
    if ((is_variable_node(opnd) &&
         is_qualified_type(node_variable(opnd)->type)) ||
        (is_operation_node(opnd) &&
         (node_operator_is(opnd, eok_indirect) ||
          node_operator_is(opnd, eok_points_to_field)) &&
         is_qualified_type(type_pointed_to(
                                  opnd->variant.operation.operands->type)))) {
      write_tok_ch('*');
      dump_cast_to_pointer_to(opnd->type);
      write_tok_ch('&');
    } else if (opnd->orig_lvalue_type != NULL &&
               is_qualified_type(opnd->orig_lvalue_type) &&
               is_operation_node(opnd) &&
               (node_operator_is(opnd, eok_lvalue_adjust) ||
                node_operator_is(opnd, eok_lvalue_cast))) {
      /* If this node has gone through an lvalue-to-rvalue conversion and
         the original lvalue type was cv-qualified, remove the original
         lvalue type so that the node will be emitted with the
         non-cv-qualified type (to match the other operand of the "?"
         operator). */
      opnd->orig_lvalue_type = NULL;
    }  /* if */
  }  /* if */
}  /* adjust_question_operand_if_necessary */

#endif /* SUNPRO_C_IS_C_GEN_BE_TARGET */
#if CHECKING && !STANDALONE_UTILITY_PROGRAM

static void check_expression_evaluation_order(an_expr_node_ptr expr)
/*
Check expr (an enk_operation) to ensure the proper setting of the
eval_right_to_left and eval_left_to_right flags.
*/
{
  if (node_operator_is(expr, eok_call)) {
    if (is_routine_node(expr->variant.operation.operands) &&
        special_kind_is(node_routine(expr->variant.operation.operands),
                        sfk_operator)) {
      /* There's not much checking to do here; a call to an operator function
         can have either evaluation order (e.g., operator= will typically have
         right-to-left, but may have left-to-right if called explicitly).
         Also, many operations have no explicit ordering so both flags will be
         unset in those cases. */
    } else {
      /* One or the other needs to be specified. */
      check_assertion(expr->variant.operation.eval_left_to_right ^
                      expr->variant.operation.eval_right_to_left);
    }  /* if */
  } else {
    /* Lowering may change the evaluation ordering from the canonical ordering
       so there is little checking that can be done here. */
    check_assertion(!(expr->variant.operation.eval_left_to_right &&
                      expr->variant.operation.eval_right_to_left));
    /* Having a right-to-left comma operator typically indicates a bug in
       lowering. */
    check_assertion(!(node_operator_is(expr, eok_comma) &&
                      expr->variant.operation.eval_right_to_left));
  }  /* if */
}  /* check_expression_evaluation_order */

#endif /* CHECKING && !STANDALONE_UTILITY_PROGRAM */

static void dump_call(an_expr_node_ptr func_expr,
                      an_expr_node_ptr arguments)
/*
Generate code for the operator call described by func_expr and the arguments
described by arguments.
*/
{
#if CHECKING
  a_param_type_ptr              param;
#endif /* CHECKING */
#if BUILTIN_FUNCTIONS_ENABLED
  a_boolean                     remove_compiler_generated_casts = FALSE;
#endif /* BUILTIN_FUNCTIONS_ENABLED */
  an_expr_node_ptr              call_argument;

  dump_expr_with_parens(func_expr);
  write_tok_ch('(');
#if CHECKING
  /* Keep track of parameter types to check for arguments to old-style
     functions that aren't widened. */
  { a_type_ptr routine_type = type_pointed_to(func_expr->type);
    routine_type = skip_typerefs(routine_type);
    param = NULL;
    if (routine_type->variant.routine.extra_info->prototyped) {
      param= routine_type->variant.routine.extra_info->param_type_list;
    }  /* if */
  }
#endif /* CHECKING */
  /* Put out the arguments. */
#if BUILTIN_FUNCTIONS_ENABLED
  if (is_routine_node(func_expr) &&
      func_expr->variant.routine.name_reference != NULL &&
      special_kind_is(func_expr->variant.routine.name_reference,
                      sfk_gnu_atomic_generic_function)) {
    /* Omit the first argument in a GNU __atomic_... generic function
       (it is generated by the front end) as well as compiler
       generated casts. */
    check_assertion(arguments != NULL);
    arguments = arguments->next;
    remove_compiler_generated_casts = TRUE;
  }  /* if */
#endif /* BUILTIN_FUNCTIONS_ENABLED */
  for (call_argument = arguments; call_argument != NULL;) {
#if BUILTIN_FUNCTIONS_ENABLED
    if (remove_compiler_generated_casts &&
        is_operation_node(call_argument) &&
        call_argument->compiler_generated &&
        call_argument->variant.operation.kind ==
                                     (an_expr_operator_kind)eok_cast) {
      /* Remove a compiler-generated cast. */
      call_argument->variant.operation.operands->next =
                                                   call_argument->next;
      call_argument = call_argument->variant.operation.operands;
    }  /* if */
#endif /* BUILTIN_FUNCTIONS_ENABLED */
    dump_expr_with_parens(call_argument);
#if CHECKING
    /* Check for unwidened arguments to old-style functions. */
    if (param != NULL) {
      /* Check for a missing array to pointer decay. */
      if (is_array_type(call_argument->type) &&
          is_pointer_type(param->type)) {
        internal_error("dump_call: missing array to pointer decay");
      }  /* if */
      param = param->next;
#if BUILTIN_FUNCTIONS_ENABLED
    } else if (is_routine_node(func_expr) &&
               is_gnu_builtin_function(func_expr->variant.routine.ptr)) {
      /* Some GNU-style built-in functions (like __builtin_isnormal)
         have ellipsis arguments that do not undergo promotion. */
#endif /* BUILTIN_FUNCTIONS_ENABLED */
    } else {
      /* Unprototyped or ellipsis argument. */
      a_type_ptr arg_type = skip_typerefs(call_argument->type);
      if (is_integral_or_enum_type(arg_type)) {
        an_integer_kind ikind = arg_type->variant.integer.int_kind;
        if ((int)ikind < (int)ik_int) {
          internal_error("dump_call: unwidened integer argument");
        }  /* if */
      } else if (arg_type->kind == (a_type_kind)tk_float
#if C99_IL_EXTENSIONS_SUPPORTED && !LOWER_COMPLEX
                 || arg_type->kind == (a_type_kind)tk_imaginary
#endif /* C99_IL_EXTENSIONS_SUPPORTED && !LOWER_COMPLEX */
                                                               ) {
        a_float_kind fkind = arg_type->variant.float_kind;
        if (fkind == fk_float) {
          internal_error("dump_call: unwidened float argument");
        }  /* if */
      }  /* if */
    }  /* if */
#endif /* CHECKING */
    call_argument = call_argument->next;
    if (call_argument != NULL) {
      write_tok_ch(',');
      write_space();
    }  /* if */
  }  /* for */
  write_tok_ch(')');
}  /* dump_call */

#if BUILTIN_FUNCTIONS_ENABLED

static a_boolean dump_intrinsic_call(an_expr_node_ptr  call_expr)
/*
If call_expr is a direct call to a builtin function that should not be called
in the generated code return TRUE and render an equivalent expression instead.
Otherwise, return FALSE.  E.g.,
        __builtin_launder(<expr>)
should be replaced by just
        (<expr>)
 */
{
  a_boolean         result = FALSE;
  an_expr_node_ptr  func_expr = call_expr->variant.operation.operands;

  if (is_routine_node(func_expr)) {
    a_routine_ptr  rp = node_routine(func_expr);
    if (special_kind_is(rp, sfk_none)) {
      switch (rp->variant.builtin_function_kind) {
        case bufk_launder:
          dump_expr_with_parens(func_expr->next);
          result = TRUE;
          break;
        case bfk_is_constant_evaluated:
          result = TRUE;
          m_write_tok_ch('(');
          dump_cast(call_expr->type);
          m_write_tok_str("0)");
          break;
        default:
          result = FALSE;
      }  /* switch */
    }  /* if */
  }  /* if */
  return result;
}  /* dump_intrinsic_call */

#endif /* BUILTIN_FUNCTIONS_ENABLED */

static void dump_expr(an_expr_node_ptr expr,
                      a_boolean        need_parens)
/*
Generate code for the indicated expression.  Put parentheses around it if
there's some possibility of precedence confusion and need_parens is TRUE.
*/
{
  an_expr_operator_kind          op;
  a_boolean                      is_unary;
  a_const_char                   *opstr = NULL;
  an_expr_node_ptr               operand_1, operand_2, operand_3;
  a_type_ptr                     expr_type;
  a_boolean                      pointer_comparison = FALSE;
  a_const_char                   *pointer_comparison_cast = "";
  uint32_t                       comma_column = 0;
#if !C_GEN_BE_GENERATES_ANSI_C
  a_field_ptr                    field;
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
#if !ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C
  a_boolean                      void_operand;
#endif /* !ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C */
#if !C_GEN_BE_GENERATES_ANSI_C
  a_boolean                      remainder_special_case = FALSE;
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  a_boolean                      ptr_to_empty_struct_case = FALSE;
  a_boolean                      pointer_arithmetic_op = FALSE;
  an_expr_node_ptr               ptr_operand;
  a_boolean                      comma_case;

  check_assertion_str(expr != NULL, "dump_expr: NULL expression");
  check_assertion_str(!is_nullptr_type(expr->type),
                      "dump_expr: unlowered nullptr type");
  switch (expr->kind) {
    case enk_operation:
      /* Expression operation. */
      if (need_parens) m_write_tok_ch('(');
      operand_1 = expr->variant.operation.operands;
      operand_2 = operand_1->next;
      expr_type = skip_typerefs(expr->type);
      op = expr->variant.operation.kind;
      is_unary = FALSE;
      /* Lvalue cases should have been rewritten by IL lowering. */
      check_assertion_str(!expr->variant.operation.
                                        returns_lvalue_instead_of_usual_rvalue,
                          "dump_expr: lvalue-returning operation");
      check_assertion_str(!expr->is_lvalue ||
                          (!node_operator_is(expr, eok_assign) &&
                           !is_compound_assignment_operator(op)),
                          "dump_expr: lvalue assignment operator");
      /* Check for type_kinds that should have been lowered. */
      check_assertion(expr->variant.operation.type_kind !=
                                               (a_type_kind)tk_ptr_to_member &&
                      expr->variant.operation.type_kind !=
                                                      (a_type_kind)tk_nullptr);
#if LOWER_COMPLEX
      check_assertion(expr->variant.operation.type_kind !=
                                                     (a_type_kind)tk_complex &&
                      expr->variant.operation.type_kind !=
                                                    (a_type_kind)tk_imaginary);
#endif /* LOWER_COMPLEX */
#if LOWER_FIXED_POINT
      check_assertion(expr->variant.operation.type_kind != 
                                                  (a_type_kind)tk_fixed_point);
#endif /* LOWER_FIXED_POINT */
      /* Check that equality, relational, and logical operations have
         type int. */
      check_assertion(!(is_operator_returning_bool(op) &&
                       (expr->type->kind != (a_type_kind)tk_integer ||
                        expr->type->variant.integer.int_kind !=
                                                  ((an_integer_kind)ik_int))));
#if CHECKING && !STANDALONE_UTILITY_PROGRAM
      check_operation_node_consistency(expr);
      if (strict_cpp17_eval_order) {
        /* Verify that the expression order is set correctly. */
        check_expression_evaluation_order(expr);
      }  /* if */
#endif /* CHECKING && !STANDALONE_UTILITY_PROGRAM */
      switch (op) {
        /* One-operand operators. */
        case eok_address_of:
          { a_variable_ptr var = variable_referenced_by_lvalue(operand_1);
            a_type_ptr     underlying_type = type_pointed_to(expr->type);
#if CHECKING
            check_address_taken_flag(expr);
#endif /* CHECKING */
            if (is_function_type(underlying_type)) {
              /* Function types decay to pointer-to-function types
                 automatically, so the ampersand is redundant (and would
                 cause an error if an implicit cast is generated because
                 a later redeclaration of the function changed its type from
                 what it was at this point -- see dump_routine_address).
                 Just omit it and generate the operand directly. */
              dump_expression(operand_1);
              goto done_with_unary_operation;
            }  /* if */
            if (is_operation_node(operand_1) &&
                (node_operator_is(operand_1, eok_lvalue_cast) ||
                 node_operator_is(operand_1, eok_lvalue_adjust))) {
              /* To avoid a gcc bug, cancel the "&" here against the "*" at
                 the top of the expansion of an lvalue cast.  gcc has problems
                 if the underlying type is incomplete, even though C99 says
                 "&*p" should be treated as equivalent to simply "p". */
              dump_lvalue_cast(operand_1, /*suppress_indirection=*/TRUE);
              goto done_with_unary_operation;
            } else if (underlying_type != operand_1->type ||
                       (is_const_qualified_type(underlying_type) &&
                        var != NULL &&
                        suppress_const_for_mutable_or_init(var))) {
              /* For some cases where const qualifiers were removed on
                 variables because of initialization, the address of the
                 variable is less-qualified than it should be, and in a way
                 that cannot be bridged by an implicit conversion in C.  For
                 example:
                   void f() {
                     int i; i = 1;
                     const char a[4] = "abc";
                     const char (&r)[4] = a;
                   }
                 Add a cast to the proper type for that case. */
              dump_cast(expr->type);
            }  /* if */
            if (is_operation_node(operand_1) &&
                node_operator_is(operand_1, eok_dot_field) &&
                ((!operand_1->variant.operation.operands->is_lvalue &&
                  (!optimizable_rvalue_selection(operand_1, &comma_case) ||
                   comma_case)) ||
                 (operand_1->variant.operation.operands->is_lvalue &&
                  obj_expr_based_on_comma(operand_1)))) {
              /* The operand is a member access expression that will be
                 generated as a comma expression, to which "&" cannot be
                 applied.  Signal that the ampersand is to be put out on
                 the second operand of the comma expression instead. */
              operand_1->variant.operation.has_deferred_ampersand = TRUE;
              dump_expression(operand_1);
              goto done_with_unary_operation;
            }  /* if */
            is_unary = TRUE;
            opstr = "&";
            break;
          }
        case eok_indirect:
          is_unary = TRUE;
          opstr = "*";
          break;
        case eok_negate:
          is_unary = TRUE;
          opstr = "-";
          break;
        case eok_unary_plus:
          is_unary = TRUE;
          opstr = "+";
          break;
        case eok_not:
          write_tok_ch('!');
          dump_boolean_controlling_expression(operand_1);
          goto done_with_unary_operation;
#if GNU_VECTOR_TYPES_ALLOWED
        case eok_vector_not:
          /* Because gcc doesn't support !v, use (v == 0) instead. */
          check_assertion(gcc_is_generated_code_target &&
                          is_vector_type(operand_1->type));
          write_tok_ch('(');
          dump_expression(operand_1);
          write_tok_str(" == ");
          write_vector_constant(operand_1->type, "0");
          write_tok_ch(')');
          goto done_with_unary_operation;
        case eok_vector_fill:
          {
            /* In most cases when a vector and a scalar are operands in the
               same operation, the scalar is "promoted" to a vector by this
               compiler-generated operation.  Each element of the vector has
               the value of the scalar.  A compound literal is generated here,
               but if the operand has side-effects, a temporary is first
               generated for the scalar operand. */
            a_host_large_unsigned i;
            a_boolean             use_temp = FALSE;
            check_assertion(gcc_is_generated_code_target &&
                            is_vector_type(expr_type) &&
                            !is_vector_type(operand_1->type));
            if (c_gen_node_has_side_effects(operand_1, (a_boolean *)NULL)) {
              /* Expression has side-effects; use a temporary. */
              use_temp = TRUE;
              write_tok_str("({");
              dump_type(operand_1->type, /*add_pointer_to=*/FALSE);
              write_tok_ch(' ');
              dump_temp_name((char *)operand_1);
              write_tok_str(" = ");
              dump_expr_with_parens(operand_1);
              write_tok_str("; ");
            }  /* if */
            write_tok_ch('(');
            dump_type(expr->type, /*add_pointer_to=*/FALSE);
            write_tok_str("){");
            for (i = num_vector_elements(expr_type); i > 0; i--) {
              if (use_temp) {
                dump_temp_name((char *)operand_1);
              } else {
                dump_expression(operand_1);
              }  /* if */
              if (i != 1) write_tok_ch(',');
            }  /* for */
            write_tok_ch('}');
            if (use_temp) {
              write_tok_str(";})");
            }  /* if */
          }
          goto done_with_unary_operation;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
        case eok_cast:
          /* It is tempting to try to suppress all compiler-generated casts
             here.  But bear in mind the following problem cases:
               -- When IL lowering is done, some cases that are marked as
                  compiler-generated in C++ are not implicit conversions in
                  C (e.g., derived-to-base pointer conversions).
               -- The underlying C compiler may not accept exactly the same
                  set of implicit conversions (e.g., in pcc mode this front
                  end allows various implicit conversions that are
                  dubious.  Also, pcc seems to have some difficulty with
                  implicit conversions from "void *" in some cases.)
          */
          if (expr->compiler_generated && is_pointer_type(expr->type) &&
              is_directly_variably_modified_type(expr->type)) {
            /* Do not put out an implicit cast to a variably-modified type. */
          } else {
            dump_cast(expr->type);
          }  /* if */
          if (is_pointer_type(operand_1->type) &&
              is_integral_or_enum_type(expr_type) &&
              expr_type->size < skip_typerefs(operand_1->type)->size) {
            /* Casting from a pointer type to a smaller integral type.  Go by
               way of unsigned long to avoid errors or warnings from the
               underlying C compiler. */
            write_tok_str("((unsigned long)");
            dump_expr_with_parens(operand_1);
            write_tok_ch(')');
          } else {
            /* Normal case. */
            dump_expr_with_parens(operand_1);
          }  /* if */
          goto done_with_unary_operation;
        case eok_lvalue_cast:
        case eok_lvalue_adjust:
          dump_lvalue_cast(expr, /*suppress_indirection=*/FALSE);
          goto done_with_unary_operation;
#if C99_IL_EXTENSIONS_SUPPORTED
        case eok_xconj:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
        case eok_complement:
          is_unary = TRUE;
          opstr = "~";
          break;
#if C99_IL_EXTENSIONS_SUPPORTED
        case eok_real_part:
          is_unary = TRUE;
          opstr = "__real ";
          break;
        case eok_imag_part:
          is_unary = TRUE;
          opstr = "__imag ";
          break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
        case eok_post_incr:
          /* Post-increment operator. */
#if !C_GEN_BE_GENERATES_ANSI_C
          /* If the field being incremented is a bit field, generate code to
             truncate/adjust the result of the assignment. */
          adjust_bit_field_value(expr);
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
          ptr_to_empty_struct_case = cast_to_ptr_to_empty_struct(expr->type,
                                                                 &need_parens);
          if (ptr_to_empty_struct_case) {
            dump_lvalue_ptr_to_empty_struct(operand_1);
          } else {
            dump_expr_with_parens(operand_1);
          }  /* if */
          write_tok_str("++");
          if (ptr_to_empty_struct_case) {
            write_tok_ch(')');
          }  /* if */
#if !C_GEN_BE_GENERATES_ANSI_C
          end_adjust_bit_field_value(expr);
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
          goto done_with_unary_operation;
        case eok_pre_incr:
          /* Pre-increment operator. */
#if !C_GEN_BE_GENERATES_ANSI_C
          /* If the field being incremented is a bit field, generate code to
             truncate/adjust the result of the assignment. */
          adjust_bit_field_value(expr);
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
          ptr_to_empty_struct_case = cast_to_ptr_to_empty_struct(expr->type,
                                                                 &need_parens);
          write_tok_str("++");
          if (ptr_to_empty_struct_case) {
            dump_lvalue_ptr_to_empty_struct(operand_1);
            write_tok_ch(')');
          } else {
            dump_expr_with_parens(operand_1);
          }  /* if */
#if !C_GEN_BE_GENERATES_ANSI_C
          end_adjust_bit_field_value(expr);
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
          goto done_with_unary_operation;
        case eok_post_decr:
          /* Post-decrement operator. */
#if !C_GEN_BE_GENERATES_ANSI_C
          /* If the field being incremented is a bit field, generate code to
             truncate/adjust the result of the assignment. */
          adjust_bit_field_value(expr);
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
          ptr_to_empty_struct_case = cast_to_ptr_to_empty_struct(expr->type,
                                                                 &need_parens);
          if (ptr_to_empty_struct_case) {
            dump_lvalue_ptr_to_empty_struct(operand_1);
          } else {
            dump_expr_with_parens(operand_1);
          }  /* if */
          write_tok_str("--");
          if (ptr_to_empty_struct_case) {
            write_tok_ch(')');
          }  /* if */
#if !C_GEN_BE_GENERATES_ANSI_C
          end_adjust_bit_field_value(expr);
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
          goto done_with_unary_operation;
        case eok_pre_decr:
          /* Pre-decrement operator. */
#if !C_GEN_BE_GENERATES_ANSI_C
          /* If the field being incremented is a bit field, generate code to
             truncate/adjust the result of the assignment. */
          adjust_bit_field_value(expr);
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
          ptr_to_empty_struct_case = cast_to_ptr_to_empty_struct(expr->type,
                                                                 &need_parens);
          write_tok_str("--");
          if (ptr_to_empty_struct_case) {
            dump_lvalue_ptr_to_empty_struct(operand_1);
            write_tok_ch(')');
          } else {
            dump_expr_with_parens(operand_1);
          }  /* if */
#if !C_GEN_BE_GENERATES_ANSI_C
          end_adjust_bit_field_value(expr);
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
          goto done_with_unary_operation;
        case eok_array_to_pointer:
          /* Decay of an array lvalue/rvalue to a pointer to its first
             element.  Just dump the array expression. */
#if CHECKING
          if (operand_1->is_lvalue) {
            /* Do not check an rvalue array -- it's not needed (there's
               no variable to check), and the traverse_expr call used to
               find the base address does not accept an rvalue array. */
            check_address_taken_flag(expr);
          }  /* if */
#endif /* CHECKING */
          if (is_variable_node(operand_1)) {
            a_variable_ptr var = node_variable(operand_1);
            if (((f_get_type_qualifiers(var->type, /*top_level=*/FALSE) &
                                                             TQ_CONST) != 0) &&
                suppress_const_for_mutable_or_init(var)) {
              /* The array is const-qualified, but it was declared as
                 non-const in the generated code.  We need a cast back to
                 the const-qualified type. */
              dump_cast(expr->type);
            }  /* if */
          }  /* if */
          dump_expr_with_parens(operand_1);
          goto done_with_unary_operation;
#if MICROSOFT_EXTENSIONS_ALLOWED
        case eok_assume:
          write_tok_str("__assume(");
          dump_expression(operand_1);
          write_tok_str(")");
          goto done_with_unary_operation;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        case eok_padd:
          pointer_arithmetic_op = TRUE;
          FALLTHROUGH
        case eok_add:
#if C99_IL_EXTENSIONS_SUPPORTED
        case eok_fjadd:
        case eok_jfadd:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
          opstr = "+";
          break;
        case eok_psubtract:
        case eok_pdiff:
          pointer_arithmetic_op = TRUE;
          FALLTHROUGH
        case eok_subtract:
#if C99_IL_EXTENSIONS_SUPPORTED
        case eok_fjsubtract:
        case eok_jfsubtract:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
          opstr = "-";
          break;
        case eok_multiply:
#if C99_IL_EXTENSIONS_SUPPORTED
        case eok_jmultiply:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
          opstr = "*";
          break;
        case eok_divide:
#if !C_GEN_BE_GENERATES_ANSI_C
          /* If the second operand is a constant 0, put out the division as
             "op1 / (0, 0)" to avoid an error from pcc. */
          if (node_operator_type_kind_is(expr, tk_integer) &&
              expr_is_zero_constant(operand_2)) {
            dump_expr_with_parens(operand_1);
            write_tok_str(" / (0,0)");
            goto done_with_binary_operation;
          }  /* if */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
#if C99_IL_EXTENSIONS_SUPPORTED
          FALLTHROUGH
        case eok_jdivide:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
          opstr = "/";
          break;
        case eok_remainder:
#if !C_GEN_BE_GENERATES_ANSI_C
          /* If the second operand is a constant 0, put out the operation as
             "op1 % (0, 0)" to avoid an error from pcc. */
          if (expr_is_zero_constant(operand_2)) {
            dump_expr_with_parens(operand_1);
            write_tok_str(" % (0,0)");
            goto done_with_binary_operation;
          }  /* if */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
          opstr = "%";
          break;
        case eok_shiftl:
          opstr = "<<";
          break;
        case eok_shiftr:
          opstr = ">>";
          break;
        case eok_eq:
        case eok_vector_eq:
          opstr = "==";
          break;
        case eok_ne:
        case eok_vector_ne:
          opstr = "!=";
          break;
        case eok_gt:
        case eok_vector_gt:
          pointer_comparison = node_operator_type_kind_is(expr, tk_pointer);
          opstr = ">";
          break;
        case eok_lt:
        case eok_vector_lt:
          pointer_comparison = node_operator_type_kind_is(expr, tk_pointer);
          opstr = "<";
          break;
        case eok_ge:
        case eok_vector_ge:
          pointer_comparison = node_operator_type_kind_is(expr, tk_pointer);
          opstr = ">=";
          break;
        case eok_le:
        case eok_vector_le:
          pointer_comparison = node_operator_type_kind_is(expr, tk_pointer);
          opstr = "<=";
          break;
        case eok_assign:
          opstr = "=";
          goto process_assignment;
        case eok_padd_assign:
          pointer_arithmetic_op = TRUE;
          FALLTHROUGH
        case eok_add_assign:
          opstr = "+=";
          goto process_assignment;
        case eok_psubtract_assign:
          pointer_arithmetic_op = TRUE;
          FALLTHROUGH
        case eok_subtract_assign:
          opstr = "-=";
          goto process_assignment;
        case eok_multiply_assign:
          opstr = "*=";
          goto process_assignment;
        case eok_divide_assign:
          opstr = "/=";
          goto process_assignment;
        case eok_remainder_assign:
          opstr = "%=";
#if !C_GEN_BE_GENERATES_ANSI_C
          if (sun_is_generated_code_target &&
              operand_2->kind == (an_expr_node_kind)enk_constant &&
              node_constant_is(operand_2, ck_integer) &&
              eqlit_integer_constant(node_constant(operand_2),
                                     (a_host_large_integer)1)) {
            /* The SUN cc compiler has a bug with "i %= 1" -- It generates no
               code.  Generate "i %= (0, 1)" instead, which works. */
            remainder_special_case = TRUE;
          }  /* if */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
          goto process_assignment;
        case eok_shiftl_assign:
          opstr = "<<=";
          goto process_assignment;
        case eok_shiftr_assign:
          opstr = ">>=";
          goto process_assignment;
        case eok_and_assign:
          opstr = "&=";
          goto process_assignment;
        case eok_or_assign:
          opstr = "|=";
          goto process_assignment;
        case eok_xor_assign:
          opstr = "^=";
process_assignment:
          /* Generate an assignment operation. */
#if !C_GEN_BE_GENERATES_ANSI_C
          /* If the field being assigned to is a bit field, generate code to
             truncate/adjust the result of the assignment. */
          adjust_bit_field_value(expr);
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
          /* Write the left operand. */
          if (pointer_arithmetic_op) {
            ptr_to_empty_struct_case =
                                     cast_to_ptr_to_empty_struct(expr->type,
                                                                 &need_parens);
          }  /* if */
          if (ptr_to_empty_struct_case) {
            dump_lvalue_ptr_to_empty_struct(operand_1);
          } else {
            dump_expr_with_parens(operand_1);
          }  /* if */
          /* Write the operation string and the right operand. */
          m_write_space();
          m_write_tok_str(opstr);
          m_write_space();
#if !C_GEN_BE_GENERATES_ANSI_C
          if (remainder_special_case) {
            /* The Sun cc compiler has a bug with "i %= 1" -- It generates no
               code.  Generate "i %= (0, 1)" instead, which works. */
            write_tok_str("(0,");
          }  /* if */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
          dump_expr_with_parens(operand_2);
          if (ptr_to_empty_struct_case) {
            write_tok_ch(')');
          }  /* if */
#if !C_GEN_BE_GENERATES_ANSI_C
          if (remainder_special_case) write_tok_ch(')');
          /* If the destination is a bit field, finish off the sign-extension/
             truncation call started earlier. */
          end_adjust_bit_field_value(expr);
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
          goto done_with_binary_operation;
        case eok_bassign:
          /* Block assignment, generated only by IL lowering of C++ code. */
          { a_type_ptr operand_1_type = skip_typerefs(operand_1->type);
            a_type_ptr operand_2_type = skip_typerefs(operand_2->type);
            a_boolean is_vla = is_vla_type(operand_1->type);
#if LOWER_VARIABLE_LENGTH_ARRAYS
            if (operand_1->type->kind == (a_type_kind)tk_typeref &&
                operand_1->type->
                           variant.typeref.is_lowered_variably_modified_type) {
              /* This is a lowered VLA type. */
              is_vla = TRUE;
            }  /* if */
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
            /* Typically, the source and destination types are the same size,
               but when assigning to a variably-sized array the size of the
               destination is unknown (and checked at run-time).  This is
               also used to partially-initialize an array, in which case
               the source must be smaller than the destination. */
            check_assertion(is_vla ||
                            ((operand_1_type->size >= operand_2_type->size ||
                              is_incomplete_array_type(operand_1_type)) &&
                             operand_2_type->size != 0));
            if (!is_aggregate_or_union_type(operand_1_type) && !is_vla) {
              /* The copy can be done by an assignment.  (This case is here
                 for completeness; the front end doesn't actually generate any
                 of these.) */
              dump_expr_with_parens(operand_1);
              write_tok_str(" = ");
              dump_expr_with_parens(operand_2);
            } else {
              /* Use a block copy. */
#if __BSD__
              /* BSD UNIX -- use bcopy. */
              write_tok_str("(void)bcopy((char *)&");
              dump_expr_with_parens(operand_2);
              write_tok_str(", (char *)&");
              dump_expr_with_parens(operand_1);
#else  /* !__BSD__ */
              /* System V or ANSI -- use memcpy. */
              write_tok_str("(void)memcpy((char *)&");
              dump_expr_with_parens(operand_1);
              if (!operand_2->is_lvalue) {
                /* This can only arise for a structured binding, in which
                   case the second operand must be an array.  We can rely
                   on the array-to-pointer decay instead of taking its
                   address. */
                check_assertion(is_array_type(operand_2->type));
                write_tok_str(", (char *)");
              } else {
                write_tok_str(", (char *)&");
              }  /* if */
              dump_expr_with_parens(operand_2);
#endif /* __BSD__ */
              /* Add the length of the move. */
              write_tok_ch(',');
              /* No cast to size_t or the like is needed; in BSD and System V
                 the length is int, and in ANSI C the function is prototyped
                 so the conversion will be implicit. */
              /* Use the size of the source operand. */
              write_unsigned_num((a_host_large_unsigned)operand_2_type->size);
              write_tok_ch(')');
            }  /* if */
          }
          goto done_with_binary_operation;
        case eok_subscript:
          ptr_operand = subscript_or_padd_pointer_operand(expr);
          check_assertion(is_pointer_type(ptr_operand->type));
          if (f_skip_typerefs(type_pointed_to(ptr_operand->type))->
                                                   generated_as_empty_struct) {
            /* In order to do the pointer arithmetic required for the
               subscripting operation, the pointer must be cast to char *
               to get the appropriate size for the elements, and the result
               must be cast back to the appropriate element type.  It's
               easier to deal with the casts in the pointer-addition form,
               so we use that instead of the subscript notation: if p is a
               pointer to S, p[i] becomes (*(S *)(((char *)p)+i)). */
            if (!need_parens) {
              write_tok_ch('(');
              need_parens = TRUE;
            }  /* if */
            write_tok_ch('*');
            dump_cast(ptr_operand->type);
            write_tok_ch('(');
            dump_possible_ptr_to_empty_struct(operand_1);
            write_tok_str(" + ");
            dump_possible_ptr_to_empty_struct(operand_2);
            write_tok_ch(')');
          } else {
            dump_expr_with_parens(operand_1);
            write_tok_ch('[');
            dump_expr_with_parens(operand_2);
            write_tok_ch(']');
          }  /* if */
          goto done_with_binary_operation;
#if GNU_VECTOR_TYPES_ALLOWED
        case eok_vector_subscript:
          if (operand_1->is_lvalue) {
            /* GCC 4.6.0 and later accept "v[n]" as an lvalue, but earlier
               versions don't, so use "((T *)&v)[n]" instead. */
            a_type_ptr element_type;
            check_assertion(is_vector_type(operand_1->type));
            element_type = skip_typerefs(operand_1->type);
            element_type = element_type->variant.vector.element_type;
            write_tok_ch('(');
            dump_cast_to_pointer_to(element_type);
            write_tok_ch('&');
            dump_expr_with_parens(operand_1);
            write_tok_ch(')');
          } else {
            dump_expr_with_parens(operand_1);
          }  /* if */
          write_tok_ch('[');
          dump_expr_with_parens(operand_2);
          write_tok_ch(']');
          goto done_with_binary_operation;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
        case eok_dot_field:
        case eok_points_to_field:
#if !C_GEN_BE_GENERATES_ANSI_C
          field = node_field(operand_2);
          if (field->is_bit_field && field->bit_field_is_signed) {
            /* Signed bit field.  Do sign extension on the unsigned bit field
               provided by pcc. */
            write_tok_str("(__sexten(");
          }  /* if */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
          dump_field_selection(expr);
#if !C_GEN_BE_GENERATES_ANSI_C
          if (field->is_bit_field && field->bit_field_is_signed) {
            write_tok_ch(',');
            write_unsigned_num((a_host_large_unsigned)field->bit_size);
            write_tok_str("))");
          }  /* if */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
          goto done_with_binary_operation;
        case eok_and:
          opstr = "&";
          break;
        case eok_or:
          opstr = "|";
          break;
        case eok_xor:
          opstr = "^";
          break;
        case eok_comma:
#if CHECKING
#if !STANDALONE_UTILITY_PROGRAM
          { a_type_ptr op2_type = skip_typerefs(operand_2->type);
            if (!il_identical_types(op2_type, expr_type)) {
#if DEBUG
              db_expression(expr);
#endif /* DEBUG */
              internal_error("dump_expr: bad type on eok_comma");
            }  /* if */
          }
#endif /* !STANDALONE_UTILITY_PROGRAM */
          check_result_not_used_flag(operand_1);
#endif /* CHECKING */
          opstr = ",";
          break;
        case eok_land:
          dump_boolean_controlling_expression(operand_1);
          write_tok_str(" && ");
          dump_boolean_controlling_expression(operand_2);
          goto done_with_binary_operation;
        case eok_lor:
          dump_boolean_controlling_expression(operand_1);
          write_tok_str(" || ");
          dump_boolean_controlling_expression(operand_2);
          goto done_with_binary_operation;
#if GNU_VECTOR_TYPES_ALLOWED
        case eok_vector_land:
          /* Because gcc doesn't support a logical "and" operation on a
             vector argument, the expression must be rewritten.  Either
             operand (but not both) may be a scalar. */
          check_assertion(gcc_is_generated_code_target);
          write_tok_ch('(');
          if (!is_vector_type(operand_1->type)) {
            /* The "s1 && v2" case is rewritten as "s1 ? v2 != 0 : 0" so that
               the operation is short-circuited if s1 is zero. */
            dump_expr_with_parens(operand_1);
            write_tok_str(" ? ");
            dump_expr_with_parens(operand_2);
            write_tok_str(" != ");
            write_vector_constant(operand_2->type, "0");
            write_tok_str(" : ");
            write_vector_constant(expr->type, "0");
          } else if (!is_vector_type(operand_2->type)) {
            /* The "v1 && s2" case is rewritten as "v1 != 0 & (s2 ?-1:0)".
               There is no short-circuit in this case. */
            dump_expression(operand_1);
            write_tok_str(" != ");
            write_vector_constant(operand_1->type, "0");
            write_tok_str(" & (");
            dump_expression(operand_2);
            write_tok_str(" ? -1 : 0)");
          } else {
            /* Both operands are vectors; rewrite as: "v1 != 0 & v2 != 0"
               There is no short-circuit in this case. */
            dump_expression(operand_1);
            write_tok_str(" != ");
            write_vector_constant(operand_1->type, "0");
            write_tok_str(" & ");
            dump_expression(operand_2);
            write_tok_str(" != ");
            write_vector_constant(operand_2->type, "0");
          }  /* if */
          write_tok_ch(')');
          goto done_with_binary_operation;
        case eok_vector_lor:
          /* Because gcc doesn't support a logical "or" operation on a
             vector argument, the expression must be rewritten.  Either
             operand (but not both) may be a scalar. */
          check_assertion(gcc_is_generated_code_target);
          write_tok_ch('(');
          if (!is_vector_type(operand_1->type)) {
            /* The "s1 || v2" case is rewritten as "s1 ? -1 : v2 != 0" so that
               the operation is short-circuited if s1 is non-zero. */
            dump_expr_with_parens(operand_1);
            write_tok_str(" ? ");
            write_vector_constant(expr->type, "-1");
            write_tok_str(" : ");
            dump_expr_with_parens(operand_2);
            write_tok_str(" != ");
            write_vector_constant(operand_2->type, "0");
          } else if (!is_vector_type(operand_2->type)) {
            /* The "v1 || s2" case is rewritten as "v1 != 0 | (s2 ? -1 : 0)".
               There is no short-circuit in this case. */
            dump_expression(operand_1);
            write_tok_str(" != ");
            write_vector_constant(operand_1->type, "0");
            write_tok_str(" | (");
            dump_expression(operand_2);
            write_tok_str(" ? -1 : 0)");
          } else {
            /* If both operands are vectors, rewrite as: "v1 != 0 | v2 != 0"
               instead.  There is no short-circuit in this case. */
            dump_expression(operand_1);
            write_tok_str(" != ");
            write_vector_constant(operand_1->type, "0");
            write_tok_str(" | ");
            dump_expression(operand_2);
            write_tok_str(" != ");
            write_vector_constant(operand_2->type, "0");
          }  /* if */
          write_tok_ch(')');
          goto done_with_binary_operation;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
        case eok_question:
          /* Three operand operator. */
          check_assertion_str(operand_2 != NULL && operand_2->next != NULL &&
                              operand_2->next->next == NULL,
                              "dump_expr: wrong # of operands for ?");
          operand_3 = operand_2->next;
#if CHECKING
#if !STANDALONE_UTILITY_PROGRAM
          { a_type_ptr op2_type = skip_typerefs(operand_2->type);
            a_type_ptr op3_type = skip_typerefs(operand_3->type);
            if (!il_identical_types(op2_type, expr_type) ||
                !il_identical_types(op3_type, expr_type)) {
#if DEBUG
              db_expression(expr);
#endif /* DEBUG */
              internal_error("dump_expr: bad type on eok_question");
            }  /* if */
          }
#endif /* !STANDALONE_UTILITY_PROGRAM */
#endif /* CHECKING */
          dump_boolean_controlling_expression(operand_1);
          write_tok_str(" ? ");
#if !ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C
          /* pcc does not allow operands of "?" to be void expressions.
             If they are, enclose them in (expr,0). */
          void_operand = is_void_type(operand_2->type);
          if (void_operand) write_tok_ch('(');
#endif /* !ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C */
#if SUNPRO_C_IS_C_GEN_BE_TARGET
          /* Perform SunPro-specific adjustment of cv-qualified types. */
          adjust_question_operand_if_necessary(operand_2, operand_3);
#endif /* SUNPRO_C_IS_C_GEN_BE_TARGET */
          dump_expr_with_parens(operand_2);
#if !ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C
          if (void_operand) write_tok_str(",0)");
#endif /* !ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C */
          write_tok_str(" : ");
#if !ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C
          void_operand = is_void_type(operand_3->type);
          if (void_operand) write_tok_ch('(');
#endif /* !ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C */
#if SUNPRO_C_IS_C_GEN_BE_TARGET
          /* Perform SunPro-specific adjustment of cv-qualified types. */
          adjust_question_operand_if_necessary(operand_3, operand_2);
#endif /* SUNPRO_C_IS_C_GEN_BE_TARGET */
          dump_expr_with_parens(operand_3);
#if !ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C
          if (void_operand) write_tok_str(",0)");
#endif /* !ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C */
          goto done_with_operation;
#if GNU_VECTOR_TYPES_ALLOWED
        case eok_vector_question:
          {
            /* A vector conditional operator is allowed in g++ but not gcc so
               it must be rewritten from its original form.  Replace the
               conditional operation with a GNU statement expression that
               initializes a temporary of the proper vector type in an
               element-by-element fashion.  For example, for the case 
               "a ? b : c" where each operand is a vector (with N elements),
               the following statement expression is generated:

                 ({ <expr-type> temp;
                     temp[0] = a[0] ? b[0] : c[0];
                     temp[1] = a[1] ? b[1] : c[1];
                     ...
                     temp[N-1] = a[N-1] ? b[N-1] : c[N-1];
                     temp; })
                         
               If any of the operands have side-effects, replace those with a
               temporary. */
            a_host_large_unsigned i;
            a_boolean             temp_for_op1 = FALSE, temp_for_op2 = FALSE;
            a_boolean             temp_for_op3 = FALSE;
            check_assertion(gcc_is_generated_code_target &&
                            operand_2 != NULL && operand_2->next != NULL &&
                            operand_2->next->next == NULL);
            operand_3 = operand_2->next;
            write_tok_str("({ ");
            dump_type(expr->type, /*add_pointer_to=*/FALSE);
            write_space();
            dump_temp_name((char *)expr);
            write_tok_str("; ");
            if (c_gen_node_has_side_effects(operand_1, (a_boolean *)NULL)) {
              temp_for_op1 = TRUE;
              dump_type(operand_1->type, /*add_pointer_to=*/FALSE);
              write_space();
              dump_temp_name((char *)operand_1);
              write_tok_str(" = ");
              dump_expression(operand_1);
              write_tok_str("; ");
            }  /* if */
            if (c_gen_node_has_side_effects(operand_2, (a_boolean *)NULL)) {
              temp_for_op2 = TRUE;
              dump_type(operand_2->type, /*add_pointer_to=*/FALSE);
              write_space();
              dump_temp_name((char *)operand_2);
              write_tok_str(" = ");
              dump_expression(operand_2);
              write_tok_str("; ");
            }  /* if */
            if (c_gen_node_has_side_effects(operand_3, (a_boolean *)NULL)) {
              temp_for_op3 = TRUE;
              dump_type(operand_3->type, /*add_pointer_to=*/FALSE);
              write_space();
              dump_temp_name((char *)operand_3);
              write_tok_str(" = ");
              dump_expression(operand_3);
              write_tok_str("; ");
            }  /* if */
            for (i = 0; i < num_vector_elements(operand_1->type); i++) {
              dump_temp_name((char *)expr);
              write_array_index(i);
              write_tok_str(" = ");
              if (temp_for_op1) {
                dump_temp_name((char *)operand_1);
              } else {
                dump_expression(operand_1);
              }  /* if */
              write_array_index(i);
              write_tok_str(" ? ");
              if (temp_for_op2) {
                dump_temp_name((char *)operand_2);
              } else {
                dump_expression(operand_2);
              }  /* if */
              write_array_index(i);
              write_tok_str(" : ");
              if (temp_for_op3) {
                dump_temp_name((char *)operand_3);
              } else {
                dump_expression(operand_3);
              }  /* if */
              write_array_index(i);
              write_tok_str("; ");
            }  /* for */
            dump_temp_name((char *)expr);
            write_tok_str("; })");
          }
          goto done_with_operation;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
        case eok_call:
          /* N operand operator. */
          /* Put out the function to call. */
#if BUILTIN_FUNCTIONS_ENABLED
          if (dump_intrinsic_call(expr)) {
            /* A call to a C++ intrinsic function that the target C compiler is
               unlikely to recognize.  The call to dump_intrinsic_call renders
               an equivalent expression in this case. */
          } else {
#endif /* BUILTIN_FUNCTIONS_ENABLED */
            dump_call(operand_1, operand_2);
#if BUILTIN_FUNCTIONS_ENABLED
          }  /* if */
#endif /* BUILTIN_FUNCTIONS_ENABLED */
          goto done_with_operation;
        case eok_va_start:
          /* <stdarg.h> va_start macro, treated as a builtin operator. */
          disable_line_wrapping();
          if (gcc_builtin_varargs_in_generated_code) {
            /* Use the intrinsic GNU C/C++ "__builtin_va_start". */
#if GCC_IS_GENERATED_CODE_TARGET
            if (gnu_target_version_number < 30300) {
              write_tok_str((char *)"__builtin_stdarg_start(");
            } else
#endif /* GCC_IS_GENERATED_CODE_TARGET */
            /* Do not insert code here. */
            {
              write_tok_str((char *)"__builtin_va_start(");
            }  /* if */
          } else {
            write_tok_str("va_start(");
          }  /* if */
          dump_expr_with_parens(operand_1);
          write_tok_ch(',');
          dump_expr_with_parens(operand_2);
          write_tok_ch(')');
          enable_line_wrapping();
          goto done_with_operation;
        case eok_va_start_single_operand:
          /* <varargs.h> va_start macro, treated as a builtin operator. */
          disable_line_wrapping();
          if (gcc_builtin_varargs_in_generated_code) {
            /* Use the intrinsic GNU C/C++ "__builtin_varargs_start". */
            write_tok_str("__builtin_varargs_start(");
          } else {
            write_tok_str("va_start(");
          }  /* if */
          dump_expression(operand_1);
          write_tok_ch(')');
          enable_line_wrapping();
          goto done_with_operation;
        case eok_va_arg:
          /* <stdarg.h> va_arg macro, treated as a builtin operator. */
          dump_va_arg(expr);
          goto done_with_operation;
        case eok_va_end:
          /* <stdarg.h> va_end macro, treated as a builtin operator. */
          disable_line_wrapping();
          if (gcc_builtin_varargs_in_generated_code) {
            /* Use the intrinsic GNU C/C++ "__builtin_va_end". */
            write_tok_str("__builtin_va_end(");
          } else {
            write_tok_str("va_end(");
          }  /* if */
          dump_expression(operand_1);
          write_tok_ch(')');
          enable_line_wrapping();
          goto done_with_operation;
        case eok_va_copy:
          /* <stdarg.h> va_copy macro, treated as a builtin operator. */
          disable_line_wrapping();
          if (gcc_builtin_varargs_in_generated_code) {
            /* Use the intrinsic GNU C/C++ "__builtin_va_copy". */
            write_tok_str("__builtin_va_copy(");
          } else {
            write_tok_str("va_copy(");
          }  /* if */
          dump_expr_with_parens(operand_1);
          write_tok_ch(',');
          dump_expr_with_parens(operand_2);
          write_tok_ch(')');
          enable_line_wrapping();
          goto done_with_operation;
        default:
          unexpected_condition_str("dump_expr: bad expression operator");
      }  /* switch */
      if (pointer_comparison) {
        /* Comparisons of function pointers are not standard C, so put in casts
           to some large integral type. */
        a_type_ptr tp = type_pointed_to(operand_1->type);
        if (!is_function_type(tp)) {
          pointer_comparison = FALSE;
        } else {
#if LONG_LONG_ALLOWED
          if ((skip_typerefs(tp))->size > targ_sizeof_long) {
            pointer_comparison_cast = "(unsigned long long)";
          } else
#endif /* LONG_LONG_ALLOWED */
          {
            pointer_comparison_cast = "(unsigned long)";
          }
        }  /* if */
      }  /* if */
      /* General-case processing: */
      if (pointer_arithmetic_op) {
        ptr_to_empty_struct_case = cast_to_ptr_to_empty_struct(expr->type,
                                                               &need_parens);
      }  /* if */
      if (is_unary) {
        /* Unary operator; operator goes first. */
        m_write_tok_str(opstr);
      }  /* if */
      /* Generate the first operand. */
      if (pointer_comparison) write_tok_str(pointer_comparison_cast);
      if (annotate && op == (an_expr_operator_kind)eok_comma) {
        /* Remember the position of the first operand of a comma operator so
           the second can be made to line up with it. */
        comma_column = curr_output_column;
      }  /* if */
      if (pointer_arithmetic_op) {
        dump_possible_ptr_to_empty_struct(operand_1);
      } else {
        dump_expr_with_parens(operand_1);
      }  /* if */
      if (!is_unary) {
        /* Two-operand operator. */
        m_write_space();
        m_write_tok_str(opstr);
        if (annotate && op == (an_expr_operator_kind)eok_comma) {
          /* Indent the second operand of a comma operator the same as
             the first. */
          continue_on_new_line();
          while (comma_column-- > 0) write_space();
        } else {
          m_write_space();
        }  /* if */
        if (pointer_comparison) write_tok_str(pointer_comparison_cast);
        if (pointer_arithmetic_op) {
          dump_possible_ptr_to_empty_struct(operand_2);
        } else {
          dump_expr_with_parens(operand_2);
        }  /* if */
      }  /* if */
      if (ptr_to_empty_struct_case) {
        write_tok_ch(')');
      }  /* if */
#if CHECKING && !STANDALONE_UTILITY_PROGRAM
      /* Check number of operands. */
      if (is_unary) {
#endif /* CHECKING && !STANDALONE_UTILITY_PROGRAM */
done_with_unary_operation:;
#if CHECKING && !STANDALONE_UTILITY_PROGRAM
        if (operand_2 != NULL) {
#if DEBUG
          db_expression(expr);
#endif /* DEBUG */
          internal_error("dump_expr: unary operator has wrong # of operands");
        }  /* if */
      } else {
#endif /* CHECKING && !STANDALONE_UTILITY_PROGRAM */
done_with_binary_operation:;
#if CHECKING && !STANDALONE_UTILITY_PROGRAM
        if (operand_2->next != NULL) {
#if DEBUG
          db_expression(expr);
#endif /* DEBUG */
          internal_error("dump_expr: binary operator has wrong # of operands");
        }  /* if */
      }  /* if */
#endif /* CHECKING && !STANDALONE_UTILITY_PROGRAM */
done_with_operation:
      if (need_parens) m_write_tok_ch(')');
      break;
    case enk_constant:
      { a_constant  *cp = node_constant(expr);
        if (il_header.source_language == sl_C &&
            constant_is(cp, ck_aggregate)) {
          /* In some modes, const aggregate variables may be folded to
             ck_aggregate constants (and sometimes the associated variable is
             removed from the IL afterwards).  If this happens in an expression
             context, render it as a compound literal (otherwise, it will be
             rendered as an initializer list, which is not valid in a C
             expression context). */
          cp->is_compound_literal = TRUE;
        }  /* if */
        dump_constant(cp);
      }
      break;
    case enk_variable:
#if CHECKING && !STANDALONE_C_GEN_BE
      check_type_of_variable_node(expr);
#endif /* CHECKING && !STANDALONE_C_GEN_BE */
      dump_variable_reference_node(expr);
      break;
    case enk_routine:
      dump_routine_address(expr);
      break;
#if KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED
    case enk_object_lifetime:
#if CHECKING
#if !STANDALONE_UTILITY_PROGRAM
      if (!il_identical_types(expr->type,
                              expr->variant.object_lifetime.expr->type)) {
#if DEBUG
        db_expression(expr);
#endif /* DEBUG */
        internal_error("dump_expr: bad type on enk_object_lifetime");
      }  /* if */
#endif /* !STANDALONE_UTILITY_PROGRAM */
#endif /* CHECKING */
      /* Ignore this node (use what's under it). */
      dump_expr(expr->variant.object_lifetime.expr, need_parens);
      break;
#endif /* KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED */
    case enk_alignof:
      if (msvc_is_generated_code_target || sun_is_generated_code_target) {
        write_tok_str("__alignof(");
      } else if (gcc_or_clang_is_generated_code_target) {
        write_tok_str("__alignof__(");
      } else {
        write_tok_str("__ALIGNOF__(");
      }  /* if */
      goto sizeof_cases;
    case enk_datasizeof:
    case enk_sizeof:
      write_tok_str("sizeof(");
sizeof_cases:
      if (expr->variant.sizeof_info.is_type) {
        /* sizeof(type). */
        dump_type(expr->variant.sizeof_info.variant.type,
                  /*add_pointer_to=*/FALSE);
      } else {
        /* sizeof(expr). */
        if (expr->variant.sizeof_info.variant.expr->is_lvalue) {
          dump_expression(expr->variant.sizeof_info.variant.expr);
        } else {
          an_expr_node_ptr sizeof_expr =
                                        expr->variant.sizeof_info.variant.expr;
          if (is_routine_node(sizeof_expr)) {
            /* For the address of a function, we need an extra "&".  The
               normal output suppresses it as unnecessary, but in a sizeof
               there is no function-to-pointer decay. */
            write_tok_ch('&');
          }  /* if */
          dump_expression(sizeof_expr);
        }  /* if */
      }  /* if */
      write_tok_ch(')');
      break;
    case enk_address_of_ellipsis:
      write_tok_str("&...");
      break;
    case enk_statement:
      /* GNU C statement expression, ({...}). */
      write_tok_str("({");
      dump_block(expr->variant.statement);
      write_tok_str("})");
      break;
    case enk_temp_init:
      /* Used for C99 compound literals. */
      dump_compound_literal(expr);
      break;
#if !DO_FULL_PORTABLE_EH_LOWERING
    /* This code is here as a debugging aid.  Normally, these nodes are
       not seen by the C-generating back end. */
    case enk_lowered_eh_construct:
      switch (expr->variant.lowered_eh.kind) {
        case leck_caught_object_address:
          write_tok_str("caught_object_address");
          break;
        case leck_thrown_object_address:
          write_tok_str("thrown_object_address");
          break;
        case leck_unreachable_cleanup_state:
          write_tok_str("(unreachable) ");
          FALLTHROUGH
        case leck_cleanup_state:
          write_tok_str("cleanup_state");
          write_tok_str(" = ");
#if GENERATE_EH_TABLES
          write_unsigned_num((a_host_large_unsigned)expr->variant.
                                     lowered_eh.variant.cleanup_region_number);
#else /* !GENERATE_EH_TABLES */
          write_unsigned_num((a_host_large_unsigned)expr->variant.
                                     lowered_eh.variant.cleanup_ptr);
#endif /* GENERATE_EH_TABLES */
          break;
        case leck_function_prologue:
          write_tok_str("function_prologue");
          break;
        case leck_function_epilogue:
          write_tok_str("function_epilogue");
          break;
        case leck_catch_epilogue:
          write_tok_str("catch_epilogue");
          break;
        case leck_try_epilogue:
          write_tok_str("try_epilogue");
          break;
        case leck_exception_caught:
          write_tok_str("exception_caught");
          break;
        case leck_exception_started:
          write_tok_str("exception_started");
          break;
#if !GENERATE_EH_TABLES
        case leck_initialization_completed:
          write_tok_str("initialization_completed");
          write_tok_str(" = ");
          write_unsigned_num((a_host_large_unsigned)expr->variant.
                                     lowered_eh.variant.dynamic_init);
          break;
#endif /* !GENERATE_EH_TABLES */
        case leck_internal_try:
          write_tok_str("internal_try(");
          dump_expr(expr->variant.lowered_eh.variant.try_and_catch_expr,
                    /*need_parens=*/TRUE);
          write_tok_str(", ");
          dump_expr(expr->variant.lowered_eh.variant.try_and_catch_expr->next,
                    /*need_parens=*/TRUE);
          write_tok_ch(')');
          break;
        default:
          unexpected_condition_str("dump_expr: bad lowered EH construct kind");
      }  /* switch */
      break;
    case enk_throw:
      write_tok_str("throw");
      if (expr->variant.throw_info != NULL &&
          expr->variant.throw_info->expr != NULL) {
        write_tok_str(" ");
        dump_expr_with_parens(expr->variant.throw_info->expr);
      }  /* if */
      break;
#endif /* !DO_FULL_PORTABLE_EH_LOWERING */
#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
    case enk_result_of_overriding_function:
      /* Node generated as part of the body of an entry function used
         as a wrapper for a call of an overriding virtual function
         with a covariant return type. */
      dump_result_of_overriding_function();
      break;
#endif /* ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
#if VLA_DEALLOCATIONS_IN_IL
    case enk_vla_dealloc:
      /* enk_vla_dealloc nodes are implicitly generated, and therefore do not
         require any output.  If VLAs are lowered, we should not see them at
         all. */
#if LOWER_VARIABLE_LENGTH_ARRAYS
      unexpected_condition_str("VLA deallocation should be lowered");
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
      break;
#endif /* VLA_DEALLOCATIONS_IN_IL */
    case enk_type_operand:
      dump_type(expr->variant.type_operand.type, /*add_pointer_to=*/FALSE);
      break;
    case enk_builtin_operation:
      { an_expr_node_ptr  arg = expr->variant.builtin_operation.operands;
        write_tok_str(
               builtin_operation_names[expr->variant.builtin_operation.kind]);
        write_tok_ch('(');
        while (arg != NULL) {
          /* Output the arguments (if any) for the constant operation.
             Do not emit parentheses around types. */
          dump_expr(arg, arg->kind != (an_expr_node_kind)enk_type_operand);
          arg = arg->next;
          if (arg != NULL) {
            write_tok_str(", ");
          }  /* if */
        }  /* while */
        write_tok_ch(')');
      }
      break;
    case enk_param_ref:
      /* A reference to a parameter or "this" in a function signature
         (e.g., in a sizeof argument). */
      form_param_ref(expr, &octl);
      break;
#if BUILTIN_FUNCTIONS_ENABLED
    case enk_builtin_choose_expr:
      { an_expr_node_ptr  arg = expr->variant.builtin_choose_expr.operands;
        arg = arg->next;
        if (!expr->variant.builtin_choose_expr.choose_first) {
          arg = arg->next;
        }  /* if */
        dump_expr(arg, /*need_parens=*/TRUE);
      }
      break;
#endif /* BUILTIN_FUNCTIONS_ENABLED */
    case enk_yield:
    case enk_await:
      write_tok_str("co_await");
      dump_expression(expr->variant.await_info.operand);
      break;
    case enk_field:
      /* enk_field entries are supposed to be handled before this. */
      unexpected_condition_str("dump_expr: enk_field");
    case enk_new_delete:  /* enk_new_delete is used in C++ only. */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case enk_gcnew:       /* enk_gcnew is used in C++/CLI only. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case enk_condition:   /* enk_condition is used in C++ only. */
    case enk_typeid:      /* enk_typeid is used in C++ only. */
    case enk_reuse_value: /* enk_reuse_value is expected to be lowered in
                             both C and C++. */
    case enk_sizeof_pack: /* enk_sizeof_pack is used in C++ only. */
    case enk_braced_init_list:
                          /* enk_braced_init_list is used in C++ only. */
    default:
      unexpected_condition_str("dump_expr: bad expr node kind");
  }  /* switch */
}  /* dump_expr */


static void dump_expression_for_il_to_str(an_expr_node_ptr expr,
                                          a_boolean        suppress_parens)
/*
Interface routine called from the il_to_str routines to dump expressions
(e.g., the dimension expression in a variable-length array declarator).
expr is the expression to render.  suppress_parens is TRUE if top-level
parentheses should not be added to the output.
*/
{
  dump_expr(expr, !suppress_parens);
}  /* dump_expression_for_il_to_str */


static a_boolean is_typedef_invisible_in_c_gen_be(a_type_ptr type,
                                                  a_type_ptr *resolved_type)
/*
Routine called from the il_to_str routines to determine whether a typedef's
name or its underlying type should be put out.  The typedef will be
considered "invisible" if its declaration has not yet been put out (for
example, if it has been deferred pending the definition of a struct to
which it refers).  resolved_type is not needed for the C-generating back end
and, if it is not NULL, *resolved_type is set to NULL.
*/
{
  if (resolved_type != NULL) {
    *resolved_type = NULL;
  }  /* if */
  return !type->typedef_definition_has_been_put_out;
}  /* is_typedef_invisible_in_c_gen_be */

#if CHECKING

static a_boolean boolean_controlling_expr_okay(an_expr_node_ptr  node)
/*
The given expression determines the value of a boolean controlling expression
and we are in a configuration where such expressions are normalized to produce
a value of zero or one.  Return TRUE if (and only if) the expression is an
integer constant with value 0 or 1, or the expression's top-level node is an
operator that always returns a 0/1 value (e.g., "<" or "!=").
*/
{
  a_boolean  result;

#if KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED
  /* Ignore an enk_object_lifetime node if present -- look under it. */
  if (node->kind == (an_expr_node_kind)enk_object_lifetime) {
    node = node->variant.object_lifetime.expr;
  }  /* if */
#endif /* KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED */
  if (node->kind == (an_expr_node_kind)enk_constant) {
    a_constant_ptr  cp = node_constant(node);
    result = cp->kind == (a_constant_repr_kind)ck_integer &&
             (cmplit_integer_constant(cp, (a_host_large_integer)0) == 0 ||
              cmplit_integer_constant(cp, (a_host_large_integer)1) == 0);
  } else {
    check_assertion(node->kind == (an_expr_node_kind)enk_operation);
    result = is_operator_returning_bool(node->variant.operation.kind);
  }  /* if */
  return result;
}  /* boolean_controlling_expr_okay */

#endif /* CHECKING */

static a_boolean
control_operand_requires_clarifying_parens(an_expr_node_ptr node)
/*
Given a node to be used as part of a boolean controlling expression, return
TRUE if additional parens should be added to avoid ambiguity diagnostics from
the target compiler; otherwise, return FALSE.
*/
{
  a_boolean result = FALSE;

  if (node->kind == enk_operation) {
    an_expr_operator_kind op_kind = node->variant.operation.kind;

    if (op_kind == eok_assign) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* control_operand_requires_clarifying_parens */


static void
dump_boolean_controlling_expression(an_expr_node_ptr node,
                  /* Defaulted: */  a_boolean        wrap_with_parens)
/*
Generate code for the indicated expression, which is the controlling expression
of a statement or short-circuit operator.  The expression is always surrounded
by (at least one set of) parentheses when wrap_with_parens is TRUE; otherwise,
the expression may be surrounded with clarifying parentheses as necessary to
silence warnings from the target compiler.
*/
{
  /* If the boolean controlling expressions are normalized, check that they do
     in fact produce a 0/1 value. */
  check_assertion(!lowering_normalizes_boolean_controlling_expressions ||
                  boolean_controlling_expr_okay(node));
  /* Output the expression with parentheses around it. */
  if (wrap_with_parens) {
    m_write_tok_ch('(');
  }  /* if */
  /* Some conditions require additional clarifying parens so the target
     compiler won't complain about a potential typo (e.g., assignments). */
  dump_expr(node, control_operand_requires_clarifying_parens(node));
  if (wrap_with_parens) {
    m_write_tok_ch(')');
  }  /* if */
}  /* dump_boolean_controlling_expression */


/*
Type used to track current position in an initializer list:
*/
typedef struct a_gen_init_pos_descr *a_gen_init_pos_descr_ptr;
typedef struct a_gen_init_pos_descr {
  a_gen_init_pos_descr_ptr
		prev,
		next;
			/* Pointers to the similar entries at the next
			   level out (prev) and in (next). */
  a_type_ptr	type;
			/* Type of entity being initialized at this level. */
  a_targ_size_t	curr_elem;
			/* If the entity is an array, this is the number of
			   the element currently being initialized. */
  a_field_ptr	curr_field;
			/* If the entity is a struct or union, this points
			   to the field currently being initialized. */
  a_targ_size_t *repetition_count;
			/* If the constant being dumped is part of a
			   ck_init_repeat, this points to the count of the
			   number of repetitions left to be dumped.  NULL
			   otherwise. */
} a_gen_init_pos_descr;


static void copy_and_delete_file(FILE **f_ptr)
/*
Copy the contents of the file *f_ptr into the current C output, and delete
the file.
*/
{
  int   c;
  FILE  *f = *f_ptr;

  /* Seek to the beginning of the file. */
  if (fseek(f, 0L, SEEK_SET) != 0) {
    file_write_error(ec_temporary, errno);
  }  /* if */
  end_output_line_if_begun();
  /* We're counting on the fact that the initialization code will have its
     own #line directives. */
  /* Copy the file. */
  while ((c = getc(f)) != EOF) {
    (void)putc(c, f_C_output);  /* Use putc not fputc for speed. */
  }  /* while */
  /* Make sure there is a newline at the end of the copied text. */
  (void)fputc('\n', f_C_output);
  /* Force a #line directive after the code. */
  set_unknown_output_position();
  /* Close and delete the temporary file. */
  close_temp_file(f);
  *f_ptr = NULL;
}  /* copy_and_delete_file */

#if USE_INIT_SECTION_IN_GENERATED_C

static void generate_init_section_call(a_const_char *startup_routine_name)
/*
Generate a call of the startup routine with the indicated name in a .init
section.  This is available on some Unix systems as a way to get
initialization code invoked at program startup time.
*/
{
  /* Generate asm statements to switch to the .init section, call the
     routine, and switch back.  The form here works for Solaris;
     it may have to be adapted for other systems. */
  end_output_line();
  write_str("asm(\" .pushsection \\\".init\\\"\");");
  end_output_line();
  write_str("asm(\" call ");
  write_str(startup_routine_name);
  write_str(" \");");
  end_output_line();
  write_str("asm(\" nop\");");
  end_output_line();
  write_str("asm(\" .popsection\");");
  end_output_line();
}  /* generate_init_section_call */

#endif /* USE_INIT_SECTION_IN_GENERATED_C */

/*
Return TRUE if the given routine is an initialization routine generated by
IL lowering.
*/
#define routine_is_init_routine(routine) \
  (has_name(routine) && \
   strncmp(routine->source_corresp.name, \
           IL_LOWERING_INIT_ROUTINE_PREFIX, \
           strlen(IL_LOWERING_INIT_ROUTINE_PREFIX)) == 0)


static void dump_rout_initializations(ARG_UNUSED a_routine_ptr routine)
/*
Dump any initializations for the routine "routine" that must be rendered
as assignment statements (see dump_initializer).  routine == NULL if
the "routine" is a block.
*/
{
#if !C_GEN_BE_GENERATES_ANSI_C
  a_const_char *pos_in_module_list, *end_pos;
  a_boolean    call_this_module_init = FALSE;
  a_boolean    is_main = (routine != NULL &&
                          routine == il_header.main_routine);
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
#if !C_GEN_BE_GENERATES_ANSI_C || USE_INIT_SECTION_IN_GENERATED_C
  a_boolean is_init_routine;
#if SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
  /* A coalesced file-scope initialization routine takes care of this case. */
  is_init_routine = FALSE;
#else /* !SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */
  is_init_routine = (routine != NULL && routine_is_init_routine(routine));
#endif /* SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */
#endif /* !C_GEN_BE_GENERATES_ANSI_C || USE_INIT_SECTION_IN_GENERATED_C */

#if USE_INIT_SECTION_IN_GENERATED_C
  if (is_init_routine) {
    /* This routine is an initialization routine generated by IL lowering.
       Generate an .init section call that gets the initialization routine
       invoked at program startup.  Note that this magic is generated
       inside the body of the initialization routine. */
    generate_init_section_call(routine->source_corresp.name);
  }  /* if */
#endif /* USE_INIT_SECTION_IN_GENERATED_C */
#if !C_GEN_BE_GENERATES_ANSI_C
  /* Call the file-scope initialization routine generated by c_gen_be
     (for union initializations) if necessary, when generating K&R C.
     Don't call the initialization routine if it won't be generated
     because it is empty.  The initialization routine is needed only for
     file-scope variables, and they have all been processed before
     the first routine is processed, so we know by now whether the
     routine is needed. */
  if (f_file_scope_inits != NULL && !file_scope_init_routine_called) {
    if (is_main) {
      /* Routine is "main"; call the file-scope initialization routine. */
      call_this_module_init = TRUE;
    } else if (is_init_routine) {
      /* This is a file-scope initialization routine generated by the
         IL lowering phase.  Call the file-scope initialization routine
         generated by c_gen_be.  That will get the code called at start-up
         without the need for a -i option. */
      call_this_module_init = TRUE;
    }  /* if */
    if (call_this_module_init) {
      /* Generate a call of the file-scope initialization routine generated
         by the C-generating back end. */
      write_str(C_GEN_BE_INIT_ROUTINE_NAME_PREFIX);
      write_str(module_init_id);
      write_tok_str("();");
      file_scope_init_routine_called = TRUE;
    }  /* if */
  }  /* if */
  if (is_main) {
    /* Main program. */
    /* Also call the initialization routines for modules that will be linked
       with this main program, as specified by the "-i" option.  The
       option string is a list of module names separated by commas. */
    pos_in_module_list = module_list_for_union_init;
    if (pos_in_module_list != NULL) {
      for (;;) {
        char saved_ch;
        end_pos = mbc_strchr(pos_in_module_list, ',');
        if (end_pos == NULL) end_pos = strchr(pos_in_module_list, '\0');
        /* No check for the name matching the current module name.  It doesn't
           hurt to call the initialization routine twice. */
        /* Write a call of the initialization routine for the indicated
           module. */
        write_str(C_GEN_BE_INIT_ROUTINE_NAME_PREFIX);
        /* Write a piece of the list string by putting in a null, writing,
           and then restoring the original character. */
        saved_ch = *end_pos;
        *(char *)end_pos = '\0';
        write_str(pos_in_module_list);
        *(char *)end_pos = saved_ch;
        write_tok_str("();");
        if (saved_ch == '\0') break;
        pos_in_module_list = end_pos + 1;
      }  /* for */
    }  /* if */
  }  /* if */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  if (f_rout_dynamic_inits != NULL) {
    /* Copy the dynamic initializations for the current routine or block. */
    copy_and_delete_file(&f_rout_dynamic_inits);
  }  /* if */
}  /* dump_rout_initializations */


static void clear_initialization_flags(an_init_control_block_ptr icbp)
/*
Clear the flags that control dump_initializer output.
*/
{
  icbp->initializer_constants_started = FALSE;
  icbp->num_initializer_open_braces_deferred = 0;
  icbp->initializer_assignments_started = FALSE;
  icbp->suppress_initializer_equals = FALSE;
  icbp->first_time_test_closing_needed = FALSE;
#if CHECKING
  icbp->zeroed = FALSE;
#endif /* CHECKING */
}  /* clear_initialization_flags */


static void dump_var_for_init(a_variable_ptr           variable,
                              a_gen_init_pos_descr_ptr ipdp)
/*
Dump a C reference to the position in the variable "variable" described by
the list pointed to by "ipdp".  This routine is called recursively when a
field represents a base class whose members have been promoted into the
derived class, in which case variable will be NULL.
*/
{
  if (variable != NULL) {
    dump_variable_name(variable);
  }  /* if */
  while (ipdp != NULL) {
#if GNU_VECTOR_TYPES_ALLOWED
    check_assertion(!is_vector_type(ipdp->type));
#endif /* GNU_VECTOR_TYPES_ALLOWED */
    if (is_array_type(ipdp->type)) {
      write_tok_ch('[');
      write_unsigned_num((a_host_large_unsigned)ipdp->curr_elem);
      write_tok_ch(']');
      ipdp = ipdp->next;
    } else {
      if (ipdp->curr_field->class_subobject_with_tail_padding) {
        /* This field represents a base class or no_unique_address field of
           class type and the base or field class type members were
           promoted into the containing class.  Push a name component for
           the class and use recursion to put out the rest of the reference
           to the field that's being initialized. */
        a_member_name_prefix_component prefix;
        push_member_name_prefix_component(&prefix, ipdp->curr_field);
        dump_var_for_init((a_variable_ptr)NULL, ipdp->next);
        ipdp = NULL;
      } else {
        /* An ordinary field: put out the name and continue to loop. */
        write_tok_ch('.');
        dump_field_name(ipdp->curr_field);
        ipdp = ipdp->next;
        /* Make sure any name prefix for this field does not carry over
           to the next one. */
        name_prefix_components = NULL;
        last_name_prefix_component = NULL;
      }  /* if */
    }  /* if */
  }  /* for */
}  /* dump_var_for_init */


static void set_init_file(ARG_UNUSED a_variable_ptr variable,
                          FILE                      **prev_f_C_output)
/*
Set f_C_output to the temporary file to which an initialization assignment
for the indicated variable should be written.  Save the previous value
of f_C_output in *prev_f_C_output.
*/
{
  *prev_f_C_output = f_C_output;
  if (output_initializer_code_directly) {
    /* Initializer code can go directly to f_C_output.  This happens,
       for example, in stmk_init statements -- they're processed in the
       executable code section. */
  } else {
    /* A temporary file must be used. */
#if !C_GEN_BE_GENERATES_ANSI_C
    if (variable->source_corresp.name_linkage !=
                                               (a_name_linkage_kind)nlk_none) {
      /* File scope variable -- put in f_file_scope_inits. */
      if (f_file_scope_inits == NULL) {
        f_file_scope_inits = open_temp_file(/*binary_file=*/FALSE);
        clear_output_file_position(&file_scope_inits_output_position);
      }  /* if */
      redirect_output_file(f_file_scope_inits);
    } else {
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
      /* Local variable -- put in f_rout_dynamic_inits. */
      if (f_rout_dynamic_inits == NULL) {
        f_rout_dynamic_inits = open_temp_file(/*binary_file=*/FALSE);
        clear_output_file_position(&rout_dynamic_inits_output_position);
      }  /* if */
      redirect_output_file(f_rout_dynamic_inits);
#if !C_GEN_BE_GENERATES_ANSI_C
    }  /* if */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  }  /* if */
}  /* set_init_file */


static void unset_init_file(FILE *prev_f_C_output)
/*
Undo the effect of set_init_file, switching f_C_output back to the
file indicated by *prev_f_C_output.
*/
{
  /* Restore the file only if it's not already the current file. */
  if (prev_f_C_output != f_C_output) {
    /* Go back to the other file. */
    redirect_output_file(prev_f_C_output);
  }  /* if */
}  /* unset_init_file */


static void dump_init_assignment(a_variable_ptr           variable,
                                 a_gen_init_pos_descr_ptr ipdp,
                                 a_constant_ptr           constant,
                                 a_boolean                single_elem = FALSE)
/*
Generate an assignment statement to set the part of the variable "variable"
described by the list pointed to by "ipdp" to the constant pointed to by
"constant".  If single_elem is TRUE, the assignment is from a single element
of a string representing the expansion of a #embed directive and not the
entire string.
*/
{
  FILE *save_f_C_output;

  /* Find the start of the ipdp list by following the prev links. */
  if (ipdp != NULL) while (ipdp->prev != NULL) ipdp = ipdp->prev;
  /* Direct the assignment output to the proper file. */
  set_init_file(variable, &save_f_C_output);
  /* Generate an assignment.  For string initialization, generate a call
     to memcpy or bcopy instead. */
  set_output_position(&variable->source_corresp.decl_position);
  if (constant->kind == (a_constant_repr_kind)ck_string && !single_elem) {
    /* String -- Generate a move.  Note that the destination of the move is
       always an array of char, so no "&" is needed in front of the
       variable name (it is implicit).  If the string is the result of an
       optimized #embed expansion, turn off the identifying flag so that
       form_constant will put it out as a string and not as a
       brace-enclosed list of integer literals. */
    a_boolean saved_embed_expansion = constant->variant.string.embed_expansion;
    constant->variant.string.embed_expansion = FALSE;
#if __BSD__
    /* BSD UNIX -- use bcopy. */
    write_tok_str("bcopy((char *)");
    dump_constant(constant);
    write_tok_ch(',');
    dump_var_for_init(variable, ipdp);
#else /* !__BSD__ */
    /* System V or ANSI -- use memcpy. */
    write_tok_str("memcpy(");
    dump_var_for_init(variable, ipdp);
    write_tok_str(", (char *)");
    dump_constant(constant);
#endif /* __BSD__ */
    /* Add the string length as the length of the move.  strcpy cannot be
       used because the string might contain extra nulls, or none. */
    write_tok_ch(',');
    /* No cast to size_t or the like is needed; in BSD and System V
       the length is int, and in ANSI C the function is prototyped
       so the conversion will be implicit. */
    write_unsigned_num((a_host_large_unsigned)constant->variant.string.length);
    write_tok_ch(')');
    constant->variant.string.embed_expansion = saved_embed_expansion;
  } else {
    /* Normal case (not string); generate an assignment statement. */
    dump_var_for_init(variable, ipdp);
    write_tok_str(" = ");
    if (single_elem) {
      /* Initializing a single element of an array from a byte in the
         expansion of a #embed directive. */
      check_assertion(ipdp != NULL && constant_is(constant, ck_string) &&
                      constant->variant.string.embed_expansion);
      write_unsigned_num(
               (unsigned char)constant->variant.string.value[ipdp->curr_elem]);
    } else {
      dump_constant(constant);
    }  /* if */
  }  /* if */
  /* Add the final semicolon to the assigning statement. */
  write_tok_ch(';');
  unset_init_file(save_f_C_output);
}  /* dump_init_assignment */


static void zero_variable(a_variable_ptr variable)
/*
Generate code to set the indicated variable entirely to zeros.
*/
{
  FILE *save_f_C_output;

  /* Direct the assignment output to the proper file. */
  set_init_file(variable, &save_f_C_output);
  set_output_position(&variable->source_corresp.decl_position);
#if __BSD__
  /* BSD -- use bzero(variable, sizeof(variable)). */
  write_tok_str("bzero((char *)");
#else /* !__BSD__ */
  /* ANSI or System V -- use memset(variable, 0, sizeof(variable)). */
  write_tok_str("memset((char *)");
#endif /* __BSD__ */
  dump_ampersand(variable->type);
  dump_variable_name(variable);
#if !__BSD__
  write_tok_str(", 0");
#endif /* !__BSD__ */
  write_tok_str(",sizeof(");
  dump_variable_name(variable);
  write_tok_str("));");
  unset_init_file(save_f_C_output);
}  /* zero_variable */


static void start_initializer_constants(an_init_control_block_ptr icbp)
/*
An initializer constant is about to be put out.  Put out the "=" at the
start of an initializer if this is the first constant.  Also put out any
open braces that were deferred until this point.
*/
{
  if (!icbp->initializer_constants_started) {
    icbp->initializer_constants_started = TRUE;
    if (!icbp->suppress_initializer_equals) write_tok_str(" = ");
    for (; icbp->num_initializer_open_braces_deferred != 0;
         icbp->num_initializer_open_braces_deferred--) {
      write_tok_ch('{');
    }  /* if */
  }  /* if */
}  /* start_initializer_constants */


static void start_initializer_assignments(a_variable_ptr            variable,
                                          an_init_control_block_ptr icbp)
  
/*
An initializer assignment for variable "variable" is about to be put out.
If this assignment is the first one, put out anything that must precede it.
*/
{
  FILE *save_f_C_output;

  if (!icbp->initializer_assignments_started) {
    icbp->initializer_assignments_started = TRUE;
    /* If the variable is unreferenced, put out an unreferenced bracket. */
    set_init_file(variable, &save_f_C_output);
    (void)start_unreferenced_bracket(&variable->source_corresp,
                                     (a_boolean *)NULL);
    unset_init_file(save_f_C_output);
    /* If the variable is a local static variable, put in a first-time test. */
    if (variable->storage_class == (a_storage_class)sc_static &&
        variable->source_corresp.name_linkage ==
                                               (a_name_linkage_kind)nlk_none) {
      /* Direct the assignment output to the proper file. */
      set_init_file(variable, &save_f_C_output);
      write_tok_str(
             "{static int __init_done=0; if (!__init_done) {__init_done=1;");
      unset_init_file(save_f_C_output);
      icbp->first_time_test_closing_needed = TRUE;
    }  /* if */
    if (!icbp->initializer_constants_started) {
      /* There was no constant initialization at all, so we are generating
         assignments for the entire initialization of the variable.  If the
         variable is not static, start by zeroing it if it is
         incompletely initialized.  See 3.5.7.  Also do zeroing for variables
         with an explicit initk_zero initialization (there won't be any
         assignments following the zeroing in that case). */
      if (!var_has_static_or_thread_storage_duration(variable)) {
        an_init_kind       init_kind;
        an_initializer_ptr initializer;
        get_variable_initializer(variable, curr_scope, &init_kind,
                                 &initializer);
        if (init_kind == (an_init_kind)initk_zero ||
            (init_kind == (an_init_kind)initk_dynamic &&
             variable->initializer.dynamic->is_partially_initialized)) {
          zero_variable(variable);
#if CHECKING
          icbp->zeroed = TRUE;
#endif /* CHECKING */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* start_initializer_assignments */


static void end_initializer_assignments(a_variable_ptr            variable,
                                        an_init_control_block_ptr icbp)
/*
If any initializer assignments were generated, do anything needed to wrap up
at the end of the assignments.
*/
{
  FILE *save_f_C_output;
  
  if (icbp->initializer_assignments_started) {
    icbp->initializer_assignments_started = FALSE;
    /* Close off the first-time test generated for local static variables
       in start_initializer_assignments. */
    if (icbp->first_time_test_closing_needed) {
      set_init_file(variable, &save_f_C_output);
      write_tok_str("}}");
      unset_init_file(save_f_C_output);
    }  /* if */
    /* End the unreferenced #if 0 if one was started in
       start_initializer_assignments. */
    set_init_file(variable, &save_f_C_output);
    end_unreferenced_bracket(&variable->source_corresp);
    unset_init_file(save_f_C_output);
  }  /* if */
}  /* end_initializer_assignments */


static void initializer_open_brace(an_init_control_block_ptr icbp)
/*
Output an open brace for an initializer.  If no initializer constants have
been output yet, defer the output of the opening brace in case no constants
prove to be needed.
*/
{
  if (icbp->initializer_constants_started) {
    write_tok_ch('{');
  } else {
    icbp->num_initializer_open_braces_deferred++;
  }  /* if */
}  /* initializer_open_brace */
    

static void initializer_close_brace(an_init_control_block_ptr icbp)
/*
Output a closing brace for an initializer.  If the corresponding opening
brace was deferred in initializer_open_brace and then never put out,
do not put out the closing brace either.
*/
{
  if (icbp->num_initializer_open_braces_deferred != 0) {
    icbp->num_initializer_open_braces_deferred--;
  } else {
    write_tok_ch('}');
  }  /* if */
}  /* initializer_close_brace */


static void dump_exploded_string(a_constant_ptr constant)
/*
Dump the value of a string literal in exploded form, i.e., a character
at a time, for use in unusual initializations.
i.e., instead of "abc" (no final null) dump 'a','b','c'.
*/
{
  a_targ_size_t a, len;
  char          ch;
  
  len = constant->variant.string.length;
  for (a = 0; a < len; a++) {
    if (constant->variant.string.embed_expansion) {
      write_unsigned_num((unsigned char)constant->variant.string.value[a]);
    } else {
      write_ch('\'');
      ch = constant->variant.string.value[a];
      (void)form_char(ch, &octl);
      write_ch('\'');
    }  /* if */
    if (a != len-1) write_tok_ch(',');
  }  /* for */
}  /* dump_exploded_string */


static void dump_exploded_wide_string(a_constant_ptr constant)
/*
Dump out a wide string constant.  Dump each character (wchar_t, char16_t, or
char32_t) as a separate integer value.
*/
{
  a_targ_size_t  a, len;
  unsigned int   char_size;
  unsigned long  temp;
  
  len = constant->variant.string.length;
  char_size = (unsigned int)character_size[constant->character_kind];
  for (a = 0; a < len; a += char_size) {
    /* Assemble the right number of bytes into one integer. */
    temp = extract_character_from_string(constant->variant.string.value + a,
                                         char_size);
    write_unsigned_num((a_host_large_unsigned)temp);
    if (a != len-char_size) write_tok_ch(',');
  }  /* for */
}  /* dump_exploded_wide_string */


static void dump_var_for_wide_string_constant(a_constant_ptr constant)
/*
Write a definition for a static variable that contains the value of the
wide string constant given by constant.  Wide string constants are put
out in this way to guarantee their alignment.
*/
{
  /* If we've already generated the variable, don't do it again. */
  if (constant->assoc_var == NULL) {
    set_output_position(&constant->source_corresp.decl_position);
    write_tok_str("static ");
    dump_general_declaration_using_type(constant->type, NO_SCP,
                                        NO_VARIABLE, NO_ROUTINE, NO_FIELD,
                                        (char *)constant, NO_NAME, TQ_NONE,
                                        /*suppress_const=*/FALSE,
                                        NO_COUNTER);
    write_tok_str(" = {");
    dump_exploded_wide_string(constant);
    write_tok_str("};");
    /* Put the constant on a list of constants to unbind at the end of the
       scope.  Use the assoc_var field as a next pointer in order not to
       disturb the "next" field. */
    constant->assoc_var =
               (a_variable_ptr)wide_string_constants_to_unbind_at_end_of_scope;
    wide_string_constants_to_unbind_at_end_of_scope = constant;
  }  /* if */
}  /* dump_var_for_wide_string_constant */


static void unbind_wide_string_constants(a_constant_ptr saved_list)
/*
We are at the end of a function or block scope.  Visit the list of constants
headed by wide_string_constants_to_unbind_at_end_of_scope and unbind each
wide string literal constant thereon from the variable associated for it
in the current scope.  Then set wide_string_constants_to_unbind_at_end_of_scope
to saved_list, the saved value from the scope surrounding the current one.
*/
{
  a_constant_ptr next, con = wide_string_constants_to_unbind_at_end_of_scope;

  /* The constant entries are linked using the assoc_var field, and terminated
     when the address of wide_string_constant_marker is reached (NULL cannot
     be used to terminate the list since a non-NULL assoc_var field is
     already used to indicate that the entry is a member of the list). */
  for (; con != &wide_string_constant_marker; con = next) {
    check_assertion(con != NULL);
    next = (a_constant_ptr)(con->assoc_var);
    con->assoc_var = NULL;
  }  /* for */
  wide_string_constants_to_unbind_at_end_of_scope = saved_list;
}  /* unbind_wide_string_constants */


static void dump_designator(a_constant_ptr con)
/*
Generate code for a ck_designator constant, i.e., a designator in a
designated initializer.
*/
{
  check_assertion(!con->variant.designator.is_generic);
  if (con->variant.designator.is_field_designator) {
    /* Field designator. */
    write_tok_ch('.');
    dump_field_name(con->variant.designator.variant.field);
  } else {
    /* Array element designator. */
    write_tok_ch('[');
    write_unsigned_num((a_host_large_unsigned)
                               con->variant.designator.variant.array_element);
    write_tok_ch(']');
  }  /* if */
  if (con->next->kind == (a_constant_repr_kind)ck_aggregate &&
      con->next->uses_designated_initializers &&
      con->next->variant.aggregate.first_constant != NULL &&
      con->next->variant.aggregate.first_constant->kind ==
                                         (a_constant_repr_kind)ck_designator &&
      !con->next->explicit_braces_on_aggregate) {
    /* We need to suppress the "=" and braces around a sub-aggregate
       initializer that did not have them in the source; we do not want to
       turn something like [0].i=5, which just initializes the i member,
       into [0]={.i=5}, which zero-initializes all other members of the
       subaggregate (possibly superseding preceding initializations). */
    con->next->elide_aggregate_braces = TRUE;
  } else {
    write_tok_str(" = ");
  }  /* if */
}  /* dump_designator */


static void dump_initializer_part(a_variable_ptr           variable,
                                  a_type_ptr               type,
                                  a_constant_ptr           constant,
                                  a_boolean                *gen_assignments,
                                  a_gen_init_pos_descr_ptr outer_level_pos,
                                  an_init_control_block    *icbp)
/*
Dump out an initializer for part of a variable.  The variable being
initialized is "variable"; the piece of it being initialized has type
"type", and gets the value indicated by "constant" (constant may be
NULL to indicate initialization to zero); and outer_level_pos points
to a list of entries that describes the location of this
initialization within the overall variable (it is the history of
the recursive calls of this routine that got us to this point).
If *gen_assignments is TRUE, assignment statements rather than constants
must be generated for the initializer list (this flag will be set to
TRUE upon encountering something that cannot be rendered as constants
in an initializer).  The statements are written to f_C_output or a
temporary file (see start_initializer_assignments).  variable can
be NULL if no assignments will be output.  icbp points to a control
block with state information for the processing.
*/
{
  check_assertion(outer_level_pos == NULL || outer_level_pos->next == NULL);
  a_gen_init_pos_descr ipd, *ipdp = &ipd;
  a_constant_ptr       elem_con;
  a_type_ptr           elem_type = NULL;
  a_targ_size_t        element_count = 0;
  a_boolean            need_close_brace = FALSE;
  a_boolean            is_aggregate;
  a_boolean            suppress_brace_for_base_class_subobject = FALSE;
#if C99_IL_EXTENSIONS_SUPPORTED && !LOWER_COMPLEX
  a_constant_ptr       complex_constant = local_constant();
#endif /* C99_IL_EXTENSIONS_SUPPORTED && !LOWER_COMPLEX */

  type = skip_typerefs(type);
#if !C_GEN_BE_GENERATES_ANSI_C
  if (!*gen_assignments) {
    if (type->kind == (a_type_kind)tk_union) {
      /* When generating K&R C, initialization of a union must always be done
         via assignment statements. */
      *gen_assignments = TRUE;
    }  /* if */
  }  /* if */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  /* If we have a constant, be guided by the constant in choosing between
     aggregate and non-aggregate cases.  Otherwise (when initializing to
     zero), be guided by the type of the entity being initialized. */
  is_aggregate = (constant != NULL) ? 
                         constant->kind == (a_constant_repr_kind)ck_aggregate :
                         is_aggregate_or_union_type(type);
#if GNU_VECTOR_TYPES_ALLOWED
  if (is_vector_type(type)) {
    /* A vector must be handled atomically and not element-by-element. */
    is_aggregate = FALSE;
  }  /* if */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
#if C99_IL_EXTENSIONS_SUPPORTED && !LOWER_COMPLEX
  if (is_aggregate && is_complex_type(type)) {
    /* An un-lowered complex constant in the form of an aggregate; copy
       the real and imaginary parts of the aggregate constant to a complex
       constant and use that in place of the aggregate. */
    check_assertion(constant != NULL &&
                    constant->variant.aggregate.first_constant != NULL &&
                    (constant->variant.aggregate.first_constant->next ==
                     constant->variant.aggregate.last_constant));
    clear_constant(complex_constant, (a_constant_repr_kind)ck_complex);
    complex_constant->type = constant->type;
    complex_constant->variant.complex_value->real =
               constant->variant.aggregate.first_constant->variant.float_value;
    complex_constant->variant.complex_value->imag =
                constant->variant.aggregate.last_constant->variant.float_value;
    constant = complex_constant;
    is_aggregate = FALSE;
  }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED && !LOWER_COMPLEX */
  if (!is_aggregate) {
    /* Non-aggregate case (includes string literals). */
    if (*gen_assignments) {
      /* Generate an assignment statement. */
      /* Do any first-time processing necessary.  This call will force the
         call of zero_variable for the constant == NULL case. */
      check_assertion(variable != NULL);
      start_initializer_assignments(variable, icbp);
      if (constant != NULL) {
        if (constant_is(constant, ck_string) &&
            constant->variant.string.embed_expansion &&
            type_is(type, tk_array) &&
            !is_character_type(type->variant.array.element_type)) {
          /* This is an embed expansion initializing an array of a
             non-character type, so we have to generate individual
             assignments for each array element from the corresponding byte
             of the string value. */
          ipdp->prev = outer_level_pos;
          ipdp->next = NULL;
          ipdp->type = type;
          ipdp->curr_field = NULL;
          ipdp->repetition_count = NULL;
          for (ipdp->curr_elem = 0;
               ipdp->curr_elem < constant->variant.string.length;
               ++ipdp->curr_elem) {
            dump_init_assignment(variable, ipdp, constant,
                                 /*single_elem=*/TRUE);
          }  /* for */
        } else {
          /* We can handle the entire initialization from this constant
             with a single assignment. */
          dump_init_assignment(variable, outer_level_pos, constant);
        }  /* if */
      }  /* if */
    } else {
      /* Generate a constant in an initializer list. */
      /* Do any first-time processing necessary. */
      start_initializer_constants(icbp);
      if (constant == NULL) {
        /* Initialize to zero. */
#if GNU_VECTOR_TYPES_ALLOWED
        if (is_vector_type(type)) {
          a_type        *vtp = skip_typerefs(type),
                        *etp = skip_typerefs(vtp->variant.vector.element_type);
          a_boolean     mfp8_case = type_is(etp, tk_mfp8);
          a_targ_size_t i;
          write_tok_ch('{');
          for (i = num_vector_elements(type); i > 0; i--) {
            write_tok_str(mfp8_case ? "__mfp8()" : "0");
            if (i != 1) write_tok_ch(',');
          }  /* for */
          write_tok_ch('}');
        } else if (type_is(skip_typerefs(type), tk_mfp8)) {
          write_tok_str("__mfp8()");
        } else
#endif /* GNU_VECTOR_TYPES_ALLOWED */
        /* Do not insert code here. */
        {
          write_tok_ch('0');
        }  /* if */
      } else if (constant->kind == (a_constant_repr_kind)ck_string &&
                 !is_normal_character_kind(constant->character_kind)) {
        /* If the initial value is a wide string constant, the string must
           be dumped specially. */
        write_tok_ch('{');
        dump_exploded_wide_string(constant);
        write_tok_ch('}');
      } else if (constant->kind == (a_constant_repr_kind)ck_string &&
                 (constant->variant.string.embed_expansion ||
                  constant->variant.string.
                           value[constant->variant.string.length-1] != '\0')) {
        /* If the initial value is a string without the trailing null, the
           individual characters must be dumped, instead of the string
           literal.  #embed expansions are not normal character strings but
           represent lists of integers, so we treat them the same way. */
        write_tok_ch('{');
        dump_exploded_string(constant);
        write_tok_ch('}');
      } else {
        /* Normal case -- output the constant value. */
        dump_constant(constant);
      }  /* if */
    }  /* if */
  } else {
    /* Initializing a union or aggregate.  Do proper setup, and call this
       routine recursively for each initial value constant. */
    a_field_ptr  prev_field = NULL;
    /* Set the location block that indicates where we are in the
       original variable. */
    ipdp->prev = outer_level_pos;
    ipdp->next = NULL;
    ipdp->type = type;
    ipdp->curr_field = NULL;
    if (outer_level_pos != NULL) {
      /* Disable spurious GCC warning about setting outer_level_pos->next to
         the address of a local variable (ipdp points to local variable
         ipd). */
BEGIN_DISABLE_GCC_WARNING_DANGLING_PTR
      outer_level_pos->next = ipdp;
END_DISABLE_GCC_WARNING_DANGLING_PTR
      if (type->kind == (a_type_kind)tk_array) {
        /* Propagate the effect of a ck_init_repeat over multidimensional
           arrays. */
        ipdp->repetition_count = outer_level_pos->repetition_count;
      } else {
        ipdp->repetition_count = NULL;
      }  /* if */
    } else {
      ipdp->repetition_count = NULL;
    }  /* if */
    if (constant != NULL) {
      /* Ensure that a variable for which we're generating initializing
         assignments has been previously zeroed if the constant that is
         initializing it is only partially-initialized (otherwise some portion
         of the variable may be uninitialized). */
      check_assertion_str(!(*gen_assignments &&
                            constant->partial_aggr_value &&
                            !icbp->zeroed),
           "dump_initializer_part: can't generate code for partial aggregate");
      elem_con = constant->variant.aggregate.first_constant;
      check_assertion_str(constant->type != NULL,
                         "dump_initializer_part: ck_aggregate with null type");
    } else {
      /* Initializing to zero. */
      elem_con = NULL;
    }  /* if */
    /* Determine the type of the aggregate member first up to be
       initialized. */
    switch (type->kind) {
      case tk_array:
        ipdp->curr_elem = 0;
        elem_type = type->variant.array.element_type;
        element_count = type->variant.array.variant.number_of_elements;
        if (ipdp->repetition_count != NULL &&
            constant != NULL &&
            !is_array_type(constant->type)) {
          /* The repeated constant is for each element of this
             array -- use the entire constant for the initialization. */
          elem_con = constant;
        }  /* if */
        break;
      case tk_struct:
      case tk_union:
        /* Find the first field in the struct or union, skipping those that
           are ignored by initialization. */
        ipdp->curr_field = next_non_empty_initializable_field(
                                  type->variant.class_struct_union.field_list);
        if (ipdp->curr_field != NULL) {
          if (msvc_is_generated_code_target) {
            /* Initialize bit-field allocation tracker. */
            msvc_bit_field_tracker.container_type = NULL;
          }  /* if */
          elem_type = ipdp->curr_field->type;
          /* Find a previous field, if any.  Note that empty classes are
             skipped, but non-initializable fields (e.g., unnamed bit fields)
             are returned. */
          if (ipdp->curr_field != next_non_empty_field(
                                type->variant.class_struct_union.field_list)) {
            /* We're not starting with the first field.  Make sure prev_field
               points to the preceding field. */
            prev_field = next_non_empty_field(
                                 type->variant.class_struct_union.field_list);
            while (next_non_empty_field(prev_field->next) != 
                   ipdp->curr_field) {
              if (msvc_is_generated_code_target) {
                track_microsoft_bit_field_allocation(prev_field);
              }  /* if */
              prev_field = next_non_empty_field(prev_field->next);
            }  /* while */
          }  /* if */
        } else {
          /* The struct or union contains no initializable fields, e.g.,
             "struct {int :0;}", but a dummy field will have been put out
             to avoid that problem.  It will be initialized below. */
          elem_type = NULL;
        }  /* if */
        break;
      default:
        unexpected_condition_str("dump_initializer_part: bad entity type");
    }  /* switch */
    if (outer_level_pos != NULL &&
        outer_level_pos->curr_field != NULL &&
        outer_level_pos->curr_field->class_subobject_with_tail_padding) {
      /* The subobject members are promoted into the containing class, so
         we need to suppress the braces for the subobject. */
      suppress_brace_for_base_class_subobject = TRUE;
    }  /* if */
    /* If generating initializer constants and this is neither a base class
       object with tail padding whose members are promoted into the derived
       class nor a subaggregate initializer with a designated initializer
       and elided braces, output a "{". */
    if (!*gen_assignments && !suppress_brace_for_base_class_subobject &&
        !(constant != NULL && constant->elide_aggregate_braces)) {
      initializer_open_brace(icbp);
      need_close_brace = TRUE;
    }  /* if */
    if (elem_type == NULL) {
      /* This comes up for empty structs and unions, e.g., "struct {int:0;}".
         Such a thing is undefined behavior.  We accept it, but we add
         a dummy field of type char to the struct/union.  Initialize it
         to zero here.  This also comes up for structures that have leading
         padding as a result of empty base class optimizations. */
      check_assertion_str(elem_con == NULL,
                          "dump_initializer_part: constant, but no field");
      /* We don't need to do anything if we're generating assignments
         (the issue here is not initialization, it's keeping in sync). */
      if (!*gen_assignments) {
        /* Do any first-time processing necessary. */
        start_initializer_constants(icbp);
#if GNU_EXTENSIONS_ALLOWED
        /* When targeting gcc, we don't generate the dummy field, because
           gcc doesn't mind empty structs.  Note that because we did the
           start_initializer_constant call above we will get {}, which is
           appropriate.  Note that the test here must match one in
           dump_struct_union_definition. */
        if (!(il_header.gcc_mode && gcc_or_clang_is_generated_code_target))
#endif /* GNU_EXTENSIONS_ALLOWED */
        /* Do not insert code here. */
        {
          if (!type->generated_as_empty_struct) {
            write_tok_ch('0');
          }  /* if */
        }  /* if */
      }  /* if */
    } else if (elem_con == NULL &&
               ((is_array_type(type) && type->variant.array.bound_is_zero)
#if GNU_VECTOR_TYPES_ALLOWED
                || (elem_type != NULL && is_vector_type(elem_type) &&
                    !*gen_assignments)
#endif /* GNU_VECTOR_TYPES_ALLOWED */
                )) {
      /* This situation is generated in GNU C mode: a zero-length array
         is initialized with an empty aggregate initializer ("{}").  It
         also occurs when a vector is initialized in the source to {}. */
      start_initializer_constants(icbp);
    } else {
      /* Loop through the list of constants and process each one.
         Go through the loop once even if elem_con == NULL.
         That happens for initk_zero initialization to zero, and for IL
         generated by IL lowering from C++ empty initializations ("{}").
         Since C does not allow an empty set of braces ("{}"), go down
         through the type until a non-aggregate is found, and initialize
         it to zero.  An exception is caused by initializers for zero-length
         arrays in GNU C mode (handled above). */
      for (;;) {
        if (elem_con != NULL &&
            elem_con->kind == (a_constant_repr_kind)ck_designator) {
          /* Put out the introduction for a designated initializer.
             When assignments are being generated, each value is assigned
             to the right aggregate element, so the designator is just
             ignored. */
          if (!*gen_assignments) {
            start_initializer_constants(icbp);
            /* coverity[var_deref_model] */
            dump_designator(elem_con);
          }  /* if */
          if (type->kind == (a_type_kind)tk_array) {
            ipdp->curr_elem =
                           elem_con->variant.designator.variant.array_element;
          } else {
            ipdp->curr_field = elem_con->variant.designator.variant.field;
          }  /* if */
#if LOWER_DESIGNATED_INITIALIZERS
          /* If we're lowering designated initializers, designators should
             not appear except to identify a field of a union. */
          check_assertion_str(type->kind == (a_type_kind)tk_union,
                              "designator was missed by lowering");
#endif /* LOWER_DESIGNATED_INITIALIZERS */
          elem_con = elem_con->next;
          check_assertion(elem_con != NULL &&
                          elem_con->kind!=(a_constant_repr_kind)ck_designator);
        } else if (!*gen_assignments && is_immediate_class_type(type)) {
          /* Check if we added some padding before this field, and if so
             generate initializers for that padding.  See calls to
             dump_field_padding for the added fields that are initialized here.
             Empty fields are skipped, but not non-initializable fields
             (e.g., bit-fields). */
          a_targ_size_t  padding, p;
          a_boolean      advance_prev_field = FALSE;
          do {
            a_field_ptr after_prev;
            if (advance_prev_field) {
              /* For subsequent times through this loop, advance prev_field. */
              prev_field = next_non_empty_field(prev_field->next);
            }  /* if */
            advance_prev_field = FALSE;
            after_prev = (prev_field != NULL) ? prev_field->next
                                              : ipdp->curr_field;
            after_prev = next_non_empty_field(after_prev);
            padding = field_padding(prev_field, after_prev, type);
            if (padding != 0) {
              start_initializer_constants(icbp);
              for (p = 0; p < padding; ++p) {
                write_tok_str("'\\0',");
              }  /* for */
            }  /* if */
            if (after_prev != ipdp->curr_field) {
              /* Adjust prev_field if next_non_empty_field skipped some
                 non-initializable fields. */
              if (msvc_is_generated_code_target) {
                track_microsoft_bit_field_allocation(prev_field);
              }  /* if */
              advance_prev_field = TRUE;
            }  /* if */
          } while (prev_field != NULL &&
                   (next_non_empty_field(prev_field->next) !=
                    ipdp->curr_field));
        } else if (annotate && !*gen_assignments &&
                   type->kind == (a_type_kind)tk_array) {
          /* Display element numbers in arrays. */
          continue_on_new_line();
          start_comment();
          write_tok_str(" [");
          write_unsigned_num((a_host_large_unsigned)ipdp->curr_elem);
          write_tok_str("]: ");
          end_comment();
        }  /* if */
        if (is_immediate_class_type(type)) {
          /* Get the current field type. */
          check_assertion_str(ipdp->curr_field != NULL,
                              "dump_initializer_part: ran out of fields");
          elem_type = ipdp->curr_field->type;
        }  /* if */
        if (elem_con != NULL &&
            elem_con->kind == (a_constant_repr_kind)ck_init_repeat) {
          /* Repeated constant (generated by lowering of extended
             designated initializers and by folding of dynamic array
             initialization). */
          a_targ_size_t  count   = elem_con->variant.init_repeat.count;
          a_constant_ptr rep_con = elem_con->variant.init_repeat.constant;
          a_boolean      repeat_at_this_level = TRUE;
          if (!constant_fully_initializes_type(rep_con, elem_type)) {
            /* A ck_init_repeat can apply either to the leaf elements or to
               the top-level array in a multidimensional array.  If the
               repeated constant matches the type at this level, don't extend
               the count to the leaf elements.  Note that the cv-qualification
               of the underlying array element type and the repeated constant
               type may be different. */
            ipdp->repetition_count = &count;
            repeat_at_this_level = FALSE;
          }  /* if */
          if (annotate) {
            start_comment();
            write_tok_str(" ");
            write_unsigned_num((a_host_large_unsigned)count);
            write_tok_str(" repetitions: ");
            end_comment();
          }  /* if */
          check_assertion(type->kind == (a_type_kind)tk_array);
          while (count > 0) {
            /* Dump rep_con as the initializer of each element of the
               (possibly multidimensional) array.  The count will be
               decremented (via ipdp->repetition_count) as each leaf
               element initializer is dumped. */
            dump_initializer_part(variable, elem_type, rep_con,
                                  gen_assignments, ipdp, icbp);
            if (repeat_at_this_level) {
              /* The repetition applies at this level, so the count must be
                 decremented. */
              --count;
            }  /* if */
            if (count > 0) {
              if (!*gen_assignments) {
                /* Put out a comma between constants. */
                write_tok_ch(',');
              }  /* if */
              /* Advance to the next element for the ck_init_repeat loop.
                 (The increment after the final element will be done below,
                 as part of the outer loop. */
              ++ipdp->curr_elem;
            }  /* if */
          }  /* while */
          ipdp->repetition_count = NULL;
        } else if (elem_con == NULL && type_is(type, tk_array) &&
                   element_count == 0) {
          /* An empty initializer for a flexible array member.  Braces were
             already emitted by the caller. */
        } else {
          /* Normal case (not a repeated constant). */
          dump_initializer_part(variable, elem_type, elem_con, gen_assignments,
                                ipdp, icbp);
        }  /* if */
        /* Stop if we entered the loop with elem_con == NULL. */
        if (elem_con == NULL) break;
#if CHECKING
        if (elem_con->next == NULL && constant != NULL &&
            elem_con != constant) {
          check_assertion_str(elem_con ==
                                     constant->variant.aggregate.last_constant,
                         "dump_initializer_part: bad aggregate last_constant");
        }  /* if */
#endif /* CHECKING */
        /* Set up for the next constant, and stop if there is none. */
        if (type->kind == (a_type_kind)tk_array &&
            ipdp->repetition_count != NULL) {
          /* We are currently expanding a ck_init_repeat constant that
             covers this array. */
          if (!is_array_type(elem_type)) {
            /* The ck_init_repeat count is the number of leaf elements, so
               we only decrement the count for a non-array element. */
            --*ipdp->repetition_count;
          }  /* if */
          if (--element_count > 0 && *ipdp->repetition_count > 0) {
            /* Repeat the current constant. */
          } else {
            /* The repetition of this constant is finished.  Move on to
               the next one. */
            elem_con = elem_con->next;
          }  /* if */
        } else {
          /* This is a non-repeated constant.  Move on to the next one. */
          elem_con = elem_con->next;
        }  /* if */
        if (elem_con == NULL) break;
        /* Put out a comma between constants. */
        if (!*gen_assignments) write_tok_ch(',');
        /* Advance to the next element in the aggregate. */
        if (elem_con->kind == (a_constant_repr_kind)ck_designator) {
          /* Don't advance if a ck_designator is next. */
        } else {
          /* Only the first field of a union is initialized, so there shouldn't
             be more than one constant on the aggregate list for a union. */
          check_assertion_str(type->kind != (a_type_kind)tk_union,
                              "dump_initializer_part: > 1 constant for union");
          if (type->kind == (a_type_kind)tk_array
#if GNU_VECTOR_TYPES_ALLOWED
              || type->kind == (a_type_kind)tk_vector
#endif /* GNU_VECTOR_TYPES_ALLOWED */
                                                     ) {
            (ipdp->curr_elem)++;
          } else {
            check_assertion_str(type->kind == (a_type_kind)tk_struct,
                                "dump_initializer_part: bad entity kind (2)");
            prev_field = ipdp->curr_field;
            if (msvc_is_generated_code_target) {
              track_microsoft_bit_field_allocation(prev_field);
            }  /* if */
            ipdp->curr_field =
                    next_non_empty_initializable_field(ipdp->curr_field->next);
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
    /* If generating initializer constants, output a "}". */
    if (need_close_brace) {
      a_boolean need_scalar_dummy_init = FALSE;
      a_boolean need_array_dummy_init = FALSE;
      initializer_close_brace(icbp);
      /* dump_field_list adds a padding member after a member whose base
         type is an empty struct, and we need to initialize it, too. */
      if (type->generated_as_empty_struct && ipdp->prev != NULL &&
          !is_array_type(ipdp->prev->type) &&
          !is_union_type(ipdp->prev->type)) {
        need_scalar_dummy_init = TRUE;
      } else if (is_array_type(type) && ipdp->prev != NULL &&
                 !is_array_type(ipdp->prev->type) &&
                 f_skip_typerefs(underlying_array_element_type(type))->
                                                   generated_as_empty_struct) {
        if (skip_typerefs(type)->size > 1) {
          need_array_dummy_init = TRUE;
        } else {
          need_scalar_dummy_init = TRUE;
        }  /* if */
      }  /* if */
      if (need_scalar_dummy_init) {
        write_tok_str(",0");
      } else if (need_array_dummy_init) {
        write_tok_str(",{0}");
      }  /* if */
      if (msvc_is_generated_code_target) {
        /* Ensure that the next bit-field will be in a new container. */
        msvc_bit_field_tracker.container_type = NULL;
      }  /* if */
    } else  if (suppress_brace_for_base_class_subobject &&
                ipdp->curr_field != NULL) {
      if (!*gen_assignments) {
        /* We're at the end of the fields that were promoted from a base
           class subobject into the derived class.  If padding was inserted
           to make the offset equal to the base class size, we need to
           generate initializers for it. */
        a_targ_size_t offset_after_fields;
        a_field_ptr   last_field = ipdp->curr_field;
        /* ipdp->curr_field was left pointing at the last initializable
           field, but the decision regarding insertion of padding was made
           on the basis of all declared fields, not just the initializable
           ones.  Ignore an empty class object. */
        while (last_field->next != NULL) {
          if (last_field->next->is_optimized_empty_class) {
            break;
          }  /* if */
          last_field = last_field->next;
        }  /* while */
        offset_after_fields = offset_after_field(last_field);
        while (offset_after_fields++ < type->size) {
          write_tok_str(",'\\0'");
        }  /* while */
      }  /* if */
      if (msvc_is_generated_code_target) {
        /* Ensure that the next bit-field will be in a new container. */
        msvc_bit_field_tracker.container_type = NULL;
      }  /* if */
    }  /* if */
    if (outer_level_pos != NULL) outer_level_pos->next = NULL;
  }  /* if */
#if C99_IL_EXTENSIONS_SUPPORTED && !LOWER_COMPLEX
  release_local_constant(&complex_constant);
#endif /* C99_IL_EXTENSIONS_SUPPORTED && !LOWER_COMPLEX */
  check_assertion(outer_level_pos == NULL || outer_level_pos->next == NULL);
}  /* dump_initializer_part */


static void dump_initializer(a_variable_ptr variable,
                             a_constant_ptr constant)
/*
Dump out an initializer to initialize a whole variable.  The variable
being initialized is "variable"; the initial value is given by "constant".
"constant" is NULL to indicate initialization to zero.

Ordinarily, this routine outputs "= constant" as an initializer, and
therefore assumes it has been called immediately after the declaration
of the variable (and before the closing semicolon).

Executable statements will be generated instead of initializers for cases
where K&R/pcc C cannot express a constant initialization (i.e., union
initializations and initializations of non-static aggregates).  The parts
preceding the troublesome case will be written out as data declarations.
The inexpressible case and any initializations following it will be
rendered as executable code.
*/
{
  a_type_ptr type = skip_typerefs(variable->type);
  a_boolean  gen_assignments = FALSE;
  an_init_control_block
             icb;
#if GNU_VECTOR_TYPES_ALLOWED
  a_boolean  saved_suppress_cast_on_vector_const =
                                           octl.suppress_cast_on_vector_const;

  if (constant != NULL && is_vector_type(constant->type)) {
    /* Vector constants are usually rendered as compound literals (e.g.,
       (V2I){ 1, 2 }), but GCC often does not accept that syntax in initializer
       contexts (particular for static-lifetime variables).  So in this context
       we render it as just an aggregate initializer (e.g., { 1, 2 }). */
    octl.suppress_cast_on_vector_const = TRUE;
  }  /* if */
#endif /* GNU_VECTOR_TYPES_ALLOWED */

#if !C_GEN_BE_GENERATES_ANSI_C
  if (!gen_assignments) {
    if (!var_has_static_or_thread_storage_duration(variable) &&
        (type->kind == (a_type_kind)tk_struct ||
         type->kind == (a_type_kind)tk_union ||
#if GNU_VECTOR_TYPES_ALLOWED
         type->kind == (a_type_kind)tk_vector ||
#endif /* GNU_VECTOR_TYPES_ALLOWED */
         type->kind == (a_type_kind)tk_array)) {
      /* Assignment statements (rather than initializer constants) must be used
         for automatic variables with union or aggregate type, since K&R/pcc
         does not allow initializers for those. */
      gen_assignments = TRUE;
    }  /* if */
  }  /* if */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  /* Set flags to indicate that nothing (either constant or executable) has
     been put out yet for this initializer. */
  clear_initialization_flags(&icb);
  /* Generate the initialization (constants and/or assignments). */
  dump_initializer_part(variable, type, constant, &gen_assignments,
                        (a_gen_init_pos_descr_ptr)NULL, &icb);
#if GNU_VECTOR_TYPES_ALLOWED
  octl.suppress_cast_on_vector_const = saved_suppress_cast_on_vector_const;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
  /* If any assignments were generated, do any wrapup required. */
  end_initializer_assignments(variable, &icb);
}  /* dump_initializer */


static void dump_compound_literal(an_expr_node_ptr expr)
/*
Generate code for a compound literal (a C99 feature), which is represented
as an enk_temp_init expression.  Note that lowering removes enk_temp_init
nodes, so this code is never executed (though a customer could make a local
change to suppress lowering of compound literals).
*/
{
  a_dynamic_init_ptr    dip = expr->variant.init.dynamic_init;
  a_type_ptr            temp_type;
  a_boolean             gen_assignments = FALSE;
  a_boolean             is_scalar;
  an_init_control_block icb;

  /* An example of the form of a compound literal:
       (int []){1, 2, 3}
  */
  write_tok_ch('(');
  temp_type = expr->type;
  dump_cast(temp_type);
  clear_initialization_flags(&icb);
  icb.suppress_initializer_equals = TRUE;
  is_scalar = !is_aggregate_or_union_type(temp_type);
  if (is_scalar) {
    /* Scalar initialization.  Put an extra set of braces around the
       initializer. */
    initializer_open_brace(&icb);
  }  /* if */
  check_assertion(dip->kind == (a_dynamic_init_kind)dik_constant);
  dump_initializer_part((a_variable *)NULL, temp_type,
                        dip->variant.constant.ptr,
                        &gen_assignments, (a_gen_init_pos_descr_ptr)NULL,
                        &icb);
  check_assertion(!gen_assignments);
  if (is_scalar) initializer_close_brace(&icb);
  write_tok_ch(')');
}  /* dump_compound_literal */


static a_constant_ptr constant_initializer(a_variable_ptr variable,
                                           an_init_kind   *init_kind)
/*
If variable has a constant initializer return a pointer to the constant value.
Otherwise, return NULL.  Return *init_kind set to the initialization kind
for the variable.
*/
{
  an_initializer_ptr initializer;
  a_constant_ptr     init_con = NULL;

  get_variable_initializer(variable, curr_scope, init_kind, &initializer);
  if (*init_kind == (an_init_kind)initk_static) {
    /* The variable has a constant static initializer. */
    check_assertion_str(variable->storage_class != (a_storage_class)sc_auto,
                      "constant_initializer: auto variable uses initk_static");
    init_con = initializer->constant;
  } else if (*init_kind == (an_init_kind)initk_dynamic) {
    a_dynamic_init_ptr dip;
    dip = initializer->dynamic;
    if (dip->kind == (a_dynamic_init_kind)dik_constant) {
      /* The variable has a constant dynamic initializer. */
      if (dip->follows_an_exec_statement) {
        /* C++ case -- the initialization is in the middle of a block and
           should not be treated as a constant initialization. */
      } else {
        init_con = dip->variant.constant.ptr;
      }  /* if */
    }  /* if */
  }  /* if */
  return init_con;
}  /* constant_initializer */


static void dump_variable_decl(a_variable_ptr variable,
                               a_boolean      dump_vars_without_initializers,
                               a_boolean      dump_initializers)
/*
Dump one variable declaration.  Variables without initializers are dumped only
if dump_vars_without_initializers is TRUE.  Initializers on variables are
dumped only if dump_initializers is TRUE.  This routine is not used for
parameters.
*/
{
  a_constant_ptr init_con;
  a_type_ptr     var_type = variable->type;
  a_boolean      has_magic_name, suppress_const = FALSE;
  a_const_char   *name;
  an_init_kind   init_kind;
#if !C_GEN_BE_GENERATES_ANSI_C
  a_boolean      forced_static;
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  a_storage_class
                 storage_class = variable->storage_class;
  a_boolean      forced_referenced;
  a_boolean      force_zeroing_of_comdat_variable = FALSE;
#if ONE_INSTANTIATION_PER_OBJECT
  a_boolean      part_of_current_output_file = TRUE;
#endif /* ONE_INSTANTIATION_PER_OBJECT */

  /* Determine whether or not the variable has a constant initializer.
     Non-constant initializers are handled by dump_dynamic_init. */
  init_con = constant_initializer(variable, &init_kind);
  if (variable->suppress_inline_definition) {
    /* When INSTANTIATE_EXTERN_INLINE is enabled, the value of the variable
       (if constant) can be used, but the variable definition should not
       be emitted when suppress_inline_definition is TRUE. */
    storage_class = (a_storage_class)sc_extern;
    dump_initializers = FALSE;
  }  /* if */
#if ONE_INSTANTIATION_PER_OBJECT
  if (needed_flag_bit_number != 0 &&
      !variable->source_corresp.is_local_to_function
#if DUPLICATE_SPECIAL_STATICS_IN_INSTANTIATION_SLICES
      && !variable->source_corresp.duplicate_static_in_instantiation_slices
#endif /* DUPLICATE_SPECIAL_STATICS_IN_INSTANTIATION_SLICES */
                                                                           ) {
    /* We're generating separate files for each instantiation, so do not
       put instantiation definitions into the primary output file, or
       primary-file variable definitions into the instantiation files.
       (Some static variables -- like certain type_info objects -- are not
       subject to this constraint and are put out in every slice that
       references them.  Also, local variables do not have slice numbers
       assigned -- they go out with the function.) */
    if ((variable->instantiation_needed_bit_number != 0) ?
                            (needed_flag_bit_number !=
                                   variable->instantiation_needed_bit_number) :
                            (needed_flag_bit_number != 1)) {
      part_of_current_output_file = FALSE;
      if (storage_class == (a_storage_class)sc_unspecified) {
        init_con = NULL;
        init_kind = (an_init_kind)initk_none;
        storage_class = (a_storage_class)sc_extern;
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  /* See if this is a variable with a special name that shouldn't get
     changed (e.g., __link). */
  name = variable->source_corresp.name;
  has_magic_name = (variable->source_corresp.name_linkage ==
                                           (a_name_linkage_kind)nlk_internal &&
                    name != NULL &&
                    is_magic_name(name));
#if !C_GEN_BE_GENERATES_ANSI_C
  /* Special and unnamed variables must be kept static even if
     they are initialized.  When generating ANSI C, variables are emitted
     as static if they are static, so it is not necessary to undo the
     transformation in some cases. */
  forced_static = (init_con != NULL &&
                   (has_magic_name || !has_name(variable)));
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  if (!dump_vars_without_initializers && init_con == NULL) {
    /* The variable has no initializer, and we're not supposed to dump
       variables without initializers. */
#if !C_GEN_BE_GENERATES_ANSI_C
  } else if (!dump_initializers && forced_static) {
    /* Suppress the first declaration of forced-static variables. */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  } else if (variable->superseded_external) {
    /* Superseded variable (there are multiple incompatible block-scope
       extern declarations in SVR4 C mode, but they're all promoted to
       the file scope; put out only the primary one). */
  } else {
    /* See if the variable is unreferenced, but always put out magic
       variables anyway.  Putting __link out if unreferenced is necessary
       when this front end is used to compile its own output. */
    a_boolean annotation_only = FALSE;
    forced_referenced = has_magic_name;
#if ONE_INSTANTIATION_PER_OBJECT
    if (!part_of_current_output_file) forced_referenced = FALSE;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
    if (forced_referenced ||
        start_unreferenced_bracket(&variable->source_corresp,
                                   &annotation_only)) {
      /* If the variable has an initializer, see if any wide string constants
         therein need to be preprocessed. */
      if (dump_initializers && init_con != NULL && !annotation_only) {
        an_expr_or_stmt_traversal_block tblock;
        set_up_prescan_traversal_block(&tblock);
        traverse_constant(init_con, &tblock);
      }  /* if */
      /* Dump any pragmas associated with the variable on the first
         declaration of the variable. */
      if (dump_vars_without_initializers) {
        dump_associated_pragmas(variable);
      }  /* if */
      set_output_position(&variable->source_corresp.decl_position);
      if (init_con != NULL &&
	  storage_class == (a_storage_class)sc_unspecified &&
	  dump_vars_without_initializers && !dump_initializers) {
	/* Initialized file-scope variable definitions with initializers,
	   will be emitted twice, once as a declaration without an
	   initializer, and once with the initializer.  On the first
	   emit an "extern" before the declaration. */
        storage_class = (a_storage_class)sc_extern;
#if !C_GEN_BE_GENERATES_ANSI_C
      } else if (init_con != NULL &&
                 storage_class == (a_storage_class)sc_static &&
                 !forced_static &&
                 (!dump_vars_without_initializers || !dump_initializers)) {
        /* For initialized file-scope static variables, suppress the
           storage class on the second declaration of the variable,
           and use "extern" on the first.  This is because pcc will
           not allow two declarations of a static variable.  Since the
           variable will be put out as an external variable,
           dump_variable_name must modify the names of static non-external
           variables so that they will not conflict with like-named static
           variables in separately-compiled modules. */
        if (dump_vars_without_initializers && !dump_initializers) {
          /* Put out "extern" on the first declaration. */
          storage_class = (a_storage_class)sc_extern;
        } else {
          storage_class = (a_storage_class)sc_unspecified;
        }  /* if */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
      }  /* if */
#if GNU_EXTENSIONS_ALLOWED
      /* A variable assigned to a specific register must always be put out
         with the "register" keyword.  (When not targeting GNU, don't put out
         the keyword since that would result in invalid code in nonlocal
         scopes.) */
      if (gcc_or_clang_is_generated_code_target &&
          var_is_gnu_named_register(variable)) {
        storage_class = (a_storage_class)sc_register;
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
      if (storage_class == variable->storage_class
#if NAMED_REGISTERS_ALLOWED
          || variable->has_named_register_storage_class
#endif /* NAMED_REGISTERS_ALLOWED */
                                                   ) {
        /* Issue the storage class as recorded in the variable. */
        dump_variable_storage_class(variable);
      } else {
        /* The storage class to be put out is not the one in the variable
           (and it's not a named register storage class). */
        dump_storage_class(storage_class);
      }  /* if */
      if (variable->comdat_group != NULL
#if ONE_INSTANTIATION_PER_OBJECT
          && part_of_current_output_file
#endif /* ONE_INSTANTIATION_PER_OBJECT */
                                        ) {
        /* A variable in a COMDAT.  Must be a definition. */
        check_assertion_str(variable->storage_class ==
                                             (a_storage_class)sc_unspecified ||
                            variable->is_inline,
                            "dump_variable_decl: var without defn in comdat");
        if (gcc_or_clang_is_generated_code_target) {
          /* GCC does not support COMDAT, but it does support weak, which
             provides a sufficient approximation. */
          write_tok_str(" __attribute__((__weak__))");
        }  /* if */
        write_space();
        start_comment();
        write_tok_str(" COMDAT group: ");
        write_tok_str(variable->comdat_group);
        write_space();
        end_comment();
        write_space();
        if (gcc_or_clang_is_generated_code_target &&
            dump_vars_without_initializers && 
            (init_kind == (an_init_kind)initk_none ||
             init_kind == (an_init_kind)initk_zero)) {
          /* GCC does not accept weak variables that do not have explicit
             initializers, so force the variable to be initialized
             to zero. */
          force_zeroing_of_comdat_variable = TRUE;
        }  /* if */
      }  /* if */
#if SUN_EXTENSIONS_ALLOWED && C_GEN_BE_GENERATES_ANSI_C
      if (sun_is_generated_code_target) {
        /* Sun-specific "link scope specifiers" (__global, __symbol, or
           __hidden). */
        form_sun_link_scope_specifiers(variable->decl_modifiers, &octl);
      }  /* if */
#endif /* SUN_EXTENSIONS_ALLOWED && C_GEN_BE_GENERATES_ANSI_C */
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (microsoft_dialect_is_generated_code_target) {
        /* Microsoft-specific keywords. */
        a_decl_modifier_set decl_modifiers = variable->decl_modifiers;
        /* __declspec(selectany) applies only to definitions. */
        if (!dump_initializers && init_con != NULL) {
          decl_modifiers &= ~DM_SELECTANY;
        }  /* if */
        dump_microsoft_decl_modifiers(decl_modifiers);
        dump_microsoft_allocate_declspec(variable->section);
        dump_microsoft_align_declspec(variable->alignment);
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      if (variable->is_thread_local) {
#if IMPLEMENTATION_SUPPORTS_MULTIPLE_THREADS
        /* Emit an indication that this variable is thread-local. */
        if (microsoft_dialect_is_generated_code_target) {
          write_tok_str("__declspec(thread) ");
        } else {
          write_tok_str("__thread ");
        }  /* if */
#endif /* IMPLEMENTATION_SUPPORTS_MULTIPLE_THREADS */
#if DECL_MODIFIERS_IN_USE && \
    (MICROSOFT_EXTENSIONS_ALLOWED || THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED)
      } else if (!microsoft_dialect_is_generated_code_target &&
                 (variable->decl_modifiers & DM_THREAD)) {
        /* Non-Microsoft dialects usually include a "__thread" keyword to
           indicate thread-local storage.  (The Microsoft syntax will have
           been emitted by the call to dump_microsoft_decl_modifiers.) */
        write_tok_str("__thread ");
#endif /* DECL_MODIFIERS_IN_USE && (MICROSOFT_EXTENSIONS_ALLOWED || ...) */
      }  /* if */
#if C_GEN_BE_GENERATES_ANSI_C
      if (suppress_const_for_mutable_or_init(variable)) {
        /* The generated code will need write access to the variable, even
           if it was const in the source. */
        suppress_const = TRUE;
      } else if (is_void_type(var_type) && is_const_qualified_type(var_type)) {
        /* A declaration like "extern const void x;" is valid ANSI/ISO C,
           but some compilers don't like it, so remove the "const". */
        suppress_const = TRUE;
      }  /* if */
#else /* !C_GEN_BE_GENERATES_ANSI_C */
      if (is_void_type(var_type)) {
        /* A (extern) variable can have void type in ANSI C, but not in
           pcc C, so change its type to char. */
        write_tok_str("char ");
        dump_variable_name(variable);
      } else {
#endif /* C_GEN_BE_GENERATES_ANSI_C */
        /* Emit any _Alignas attributes that may be present in the type of
           a compound literal temporary. */
        if (c18_mode &&
            form_alignas_attributes(variable->source_corresp.attributes,
                                    /*need_leading_space=*/TRUE, &octl)) {
          write_space();
        }  /* if */
        dump_general_declaration_using_type(var_type,
                                            &variable->source_corresp,
                                            variable, NO_ROUTINE, NO_FIELD,
                                            NO_TEMP, NO_NAME, TQ_NONE,
                                            suppress_const,
                                            NO_COUNTER);
#if !C_GEN_BE_GENERATES_ANSI_C
      }  /* if */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
#if GNU_EXTENSIONS_ALLOWED
      /* Emit any user-specified assembly symbol for this variable. */
      if (variable->asm_name_is_valid) {
        form_asm_name(variable->asm_name_or_reg.name, &octl);
#if NAMED_REGISTERS_ALLOWED
      } else if (variable->has_named_register_storage_class) {
        /* This variable was defined with an Embedded C named-register
           storage class.  The storage class was already emitted elsewhere. */
#endif /* NAMED_REGISTERS_ALLOWED */
      } else {
        form_var_reg_name(variable->asm_name_or_reg.reg, &octl);
      }  /* if */
      /* Emit attributes associated with this variable. */
      (void)form_variable_attributes(variable, /*need_leading_space=*/TRUE,
                                     &octl);
#endif /* GNU_EXTENSIONS_ALLOWED */
      /* Dump the initializer if there is a constant one or if the
         variable should be initialized to zero. */
      /* Don't initialize static arrays to zero, because it blows up
         the size of the executable.  However, do put out definitions
         for template static data members that are arrays, or otherwise
         the template prelinker could loop. */
      if ((dump_initializers && init_con != NULL) ||
          force_zeroing_of_comdat_variable ||
          (init_kind == (an_init_kind)initk_zero &&
           !variable->suppress_inline_definition &&
           (!var_has_static_or_thread_storage_duration(variable) ||
            !is_array_type(variable->type) ||
            variable->is_template_variable ||
            variable->is_inline))) {
        dump_initializer(variable, init_con);
      }  /* if */
      write_tok_ch(';');
      if (!forced_referenced) {
        end_unreferenced_bracket(&variable->source_corresp);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* dump_variable_decl */

#if GNU_EXTENSIONS_ALLOWED

static void dump_asm_operands(an_asm_entry_ptr  aep)
/*
Dump the GNU C operand descriptions for the given asm entry.
*/
{
  a_boolean                     output;
  an_asm_operand_ptr            aop;
#if !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
  an_asm_operand_constraint_ptr c;
#endif /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */

  /* Check for the case of no operands at all, or just no outputs. */
#if RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
  output = aep->operands != NULL && aep->operands->is_output_operand;
#else /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
  output = aep->operands != NULL &&
           (aep->operands->modifiers & (an_asm_operand_modifier)aom_output);
#endif /* RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
  if (!output) {
    write_tok_str(" :");
  }  /* if */
  for (aop = aep->operands; aop != NULL;) {
    write_tok_ch(' ');
    if (aop->name != NULL) {
      /* This is a named operand. */
      write_tok_ch('[');
      write_tok_str(aop->name);
      write_tok_ch(']');
    }  /* if */
#if RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
    write_ch('"');
    write_str(aop->constraints_string);
    write_ch('"');
#else /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
    m_write_ch('"');
    if (output) {
      if (aop->modifiers & (an_asm_operand_modifier)aom_input) {
        m_write_ch('+');
      } else {
        m_write_ch('=');
      }  /* if */
    }  /* if */
    for (c = aop->constraints; c != NULL; c = c->next) {
      m_write_ch(asm_operand_constraint_letters[(int)c->kind]);
#if GNU_X86_ASM_EXTENSIONS_ALLOWED
      if (c->kind == aoc_cc) {
        m_write_str("cc");
        m_write_str(c->cond_code);
      }  /* if */
#endif /* GNU_X86_ASM_EXTENSIONS_ALLOWED */
    }  /* for */
    m_write_ch('"');
#endif /* RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
    write_tok_str(" (");
    dump_expression(aop->expression);
    m_write_ch(')');
    /* Move to the next operand (if any). */
    aop = aop->next;
    /* If this was the last output, but not the last entry, write a colon.
       Else if this was not the last operand, write a comma. */
    if (aop != NULL) {
      /* Another operand description follows. */
      a_boolean  next_is_output;
#if RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
      next_is_output = aop->is_output_operand;
#else /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
      next_is_output = (aop->modifiers & (an_asm_operand_modifier)aom_output);
#endif /* RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
      if (output && !next_is_output) {
        write_tok_str(" :");
        output = FALSE;
      } else {
        m_write_ch(',');
      }  /* if */
    }  /* if */
  }  /* for */
  if (output && (aep->clobbers != NULL || aep->gnu_asm_form)) {
    /* There were no input operands, but clobbers are about to follow (or
       the construct was written in the source using the extended syntax,
       which must be preserved).  Be sure to make the empty input
       specification explicit. */
    write_tok_str(" :");
  }  /* if */
}  /* dump_asm_operands */


static void dump_asm_clobbers(an_asm_entry_ptr aep)
/*
Dump the GNU C clobber specifications for the given asm entry.
*/
{
  a_named_register_list_ptr nrlp;

  /* GCC does not want to see empty clobbers lists. */
  if (aep->clobbers != NULL) {
    write_tok_str(" :");
    for (nrlp = aep->clobbers; nrlp != NULL; nrlp = nrlp->next) {
      /* Permit line breaking here. */
      write_tok_ch(' ');
      /* Register names are assumed not to have any characters that need
         to be escaped in string constants. */
      m_write_ch('"');
      m_write_str(named_register_names[nrlp->reg]);
      m_write_ch('"');
      if (nrlp->next != NULL) {
        m_write_ch(',');
      }  /* if */
    }  /* for */
  } else if (aep->is_asm_goto) {
    /* Emit an empty clobbers list if labels are to follow. */
    write_tok_str(" :");
  }  /* if */
}  /* dump_asm_clobbers */


static void dump_asm_labels(an_asm_entry_ptr aep)
/*
Dump the GNU label specifications for the given asm goto entry.
*/
{
  a_label_list_ptr llp;

  write_tok_str(" :");
  for (llp = aep->labels; llp != NULL; llp = llp->next) {
    /* Permit line breaking here. */
    write_tok_ch(' ');
    dump_label_name(llp->label);
    if (llp->next != NULL) {
      m_write_ch(',');
    }  /* if */
  }  /* for */
}  /* dump_asm_labels */


static void dump_asm_goto_string(an_asm_entry_ptr aep)
/*
Dump the asm_string associated with aep replacing any labels that may
appear in the string (e.g., in "%l[label]") with the appropriate label name
(which has its position added to it).
*/
{
  a_label_list_ptr llp;
  a_targ_size_t    pos = 0, end_pos;
  a_const_char     *str;

  check_assertion(aep->asm_string->kind == (a_constant_repr_kind)ck_string &&
                  is_normal_character_kind(aep->asm_string->character_kind));
  /* Permit line breaking here. */
  write_tok_ch(' ');
  m_write_ch('"');
  str = aep->asm_string->variant.string.value;
  while (pos < aep->asm_string->variant.string.length) {
    if (pos+5 < aep->asm_string->variant.string.length &&
        str[pos] == '%' && str[pos+1] == 'l' && str[pos+2] == '[') {
#if CHECKING
      a_boolean found = FALSE;
#endif /* CHECKING */
      /* Found "%l[", which indicates the beginning of a label; find the
         corresponding label argument (the front end has ensured that there
         is one) and dump that name instead. */
      m_write_str("%l[");
      pos += 3;
      end_pos = pos;
      while (end_pos < aep->asm_string->variant.string.length) {
        if (str[end_pos] == ']') {
          /* Found the closing bracket.  Now match the label. */
          sizeof_t len = (sizeof_t)(end_pos - pos);
          for (llp = aep->labels; llp != NULL; llp = llp->next) {
            if (strncmp(&str[pos], llp->label->source_corresp.name, len)
                                                                        == 0 &&
                (sizeof_t)strlen(llp->label->source_corresp.name) == len) {
#if CHECKING
              found = TRUE;
#endif /* CHECKING */
              /* Dump the label name. */
              dump_label_name(llp->label);
              /* Resume normal processing starting with the "]". */
              pos = end_pos;
              goto resume_scanning;
            }  /* if */
          }  /* for */
          unexpected_condition();
        } else {
          ++end_pos;
        }  /* if */
      }  /* while */
resume_scanning:;
      check_assertion(found);
    } else {
      if (pos+1 < aep->asm_string->variant.string.length ||
          str[pos] != '\0') {
        /* Suppress the last character in the string if it is NULL. */
        (void)form_char(str[pos], &octl);
      }  /* if */
      ++pos;
    }  /* if */
  }  /* while */
  m_write_ch('"');
}  /* dump_asm_goto_string */

#endif /* GNU_EXTENSIONS_ALLOWED */


static void dump_asm_entry(an_asm_entry_ptr aep)
/*
Generate C for an asm statement or declaration.
*/
{
  /* Dump any pragmas associated with the entry. */
  dump_associated_pragmas(aep);
  set_output_position(&aep->source_corresp.decl_position);
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode || msvc_is_generated_code_target) {
    /* If generating code for processing by the Microsoft compiler the
       form "__asm("...")" is not accepted.  Use "__asm { ... }" instead. */
    /* Note that there is an "is_asm_block" flag that indicates whether
       the source form used the braces.  However, the brace-enclosed
       form works in all cases, so it is used even if the source
       form did not include them. */
    write_tok_str("__asm");
    dump_asm_function_body(aep->asm_string->variant.string.value);
  } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Do not insert code here; this is the "else" of an "if". */
  {
    /* GNU C does not treat "asm" as a keyword in some (e.g., C99) modes. */
    write_tok_str((char *)(gcc_or_clang_is_generated_code_target ? "__asm__" :
                                                                   "asm"));
#if GNU_EXTENSIONS_ALLOWED
    if (aep->is_volatile && (aep->operands != NULL || aep->clobbers != NULL ||
                             aep->gnu_asm_form)) {
      write_tok_str(" volatile");
    }  /* if */
    if (aep->is_asm_goto) {
      write_tok_str(" goto");
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    write_tok_ch('(');
#if GNU_EXTENSIONS_ALLOWED
    if (aep->is_asm_goto) {
      /* An "asm goto" string may have label references that need special
         attention. */
      dump_asm_goto_string(aep);
    } else
#endif /* GNU_EXTENSIONS_ALLOWED */
    {
      dump_constant(aep->asm_string);
    }  /* if */
#if GNU_EXTENSIONS_ALLOWED
    if (aep->operands != NULL || aep->clobbers != NULL || !aep->is_volatile ||
        aep->gnu_asm_form) {
      write_tok_str(" :");
      dump_asm_operands(aep);
      dump_asm_clobbers(aep);
      if (aep->is_asm_goto) {
        dump_asm_labels(aep);
      }  /* if */
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    write_tok_str(");");
  }  /* if */
}  /* dump_asm_entry */


static void dump_scope_variables(a_scope_ptr scope,
                                 a_boolean   interleave_asm_decls,
                                 a_boolean   dump_vars_without_initializers,
                                 a_boolean   dump_initializers)
/*
Dump all variables on the list of variables for the given scope.  Variables
without initializers are dumped only if dump_vars_without_initializers is
TRUE.  Initializers on variables are dumped only if dump_initializers
is TRUE.  If interleave_asm_decls is TRUE, file-scope asm decls are
interleaved with the variables.
*/
{
  a_variable_ptr   var_ptr;
  an_asm_entry_ptr aep;

  /* Dump static variables. */
  aep = scope->asm_entries;
  for (var_ptr = scope->variables; var_ptr != NULL; var_ptr = var_ptr->next) {
    if (interleave_asm_decls) {
      /* Put out asm declarations (if any) interspersed with variable
         declarations. */
      for (;aep != NULL &&
            (aep->source_corresp.decl_position.seq <
                                   var_ptr->source_corresp.decl_position.seq ||
             (aep->source_corresp.decl_position.seq ==
                                   var_ptr->source_corresp.decl_position.seq &&
              aep->source_corresp.decl_position.column <=
                                var_ptr->source_corresp.decl_position.column));
           aep = aep->next) {
        check_membership_info(aep, scope);
        dump_asm_entry(aep);
      }  /* for */
    }  /* if */
    check_membership_info(var_ptr, scope);
    /* Don't output nonreal variables. */
    if (ignore_variable_in_back_end(var_ptr)) continue;
    if (var_ptr->has_variably_modified_type) {
      /* The variable has a variably modified type.  Do not put it out
         now; it will be put out where the corresponding stmk_vla_decl
         statement appears. */
#if LOWER_VARIABLE_LENGTH_ARRAYS
      unexpected_condition_str("VLA types should be lowered");
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
    } else if (!dump_vars_without_initializers &&
               var_ptr->storage_class == (a_storage_class)sc_extern) {
      /* Do not put out a declaration with an initializer for a variable
         that is not a definition, which would be indicated by
         sc_unspecified.  Some static data members with in-class
         initializers can result in an sc_extern variable with an
         initializer; putting out a declaration with an initializer would
         be treated as a definition, which would conflict with a real
         definition in another translation unit. */
#if C_GEN_BE_GENERATES_C23
    } else if (!dump_initializers && scope->kind == sck_file &&
               var_ptr->is_thread_local &&
               var_ptr->storage_class != sc_extern) {
      /* In C23, a non-extern file-scope thread_local declaration is always
         a definition, so we cannot follow the usual pattern of putting out
         a non-defining declaration followed by the definition. */
#endif /* C_GEN_BE_GENERATES_C23 */
    } else {
      a_boolean dump_no_init = dump_vars_without_initializers;
#if C_GEN_BE_GENERATES_C23
      if (scope->kind == sck_file && var_ptr->is_thread_local) {
        /* We skipped the "forward declaration" of thread-local variables,
           so we need to ensure that they are put out in the "definition"
           pass over the variable list. */
        dump_no_init = TRUE;
      }  /* if */
#endif /* C_GEN_BE_GENERATES_C23 */
      dump_variable_decl(var_ptr, dump_no_init, dump_initializers);
    }  /* if */
  }  /* for */
  if (interleave_asm_decls) {
    /* Put out asm declarations (if any) that follow all variable
       declarations. */
    for (;aep != NULL; aep = aep->next) {
      check_membership_info(aep, scope);
      dump_asm_entry(aep);
    }  /* for */
  }  /* if */
  /* Dump nonstatic variables. */
  for (var_ptr = scope->nonstatic_variables;
       var_ptr != NULL;
       var_ptr = var_ptr->next) {
    /* Don't output nonreal variables. */
    if (ignore_variable_in_back_end(var_ptr)) continue;
    if (var_ptr->has_variably_modified_type) {
      /* The variable has a variably modified type.  Do not put it out
         now; it will be put out where the corresponding stmk_vla_decl
         statement appears. */
#if LOWER_VARIABLE_LENGTH_ARRAYS
      unexpected_condition_str("VLA types should be lowered");
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
    } else {
      dump_variable_decl(var_ptr, dump_vars_without_initializers,
                         dump_initializers);
    }  /* if */
  }  /* for */
}  /* dump_scope_variables */


static void dump_constant_decl(a_constant_ptr constant)
/*
Dump out one constant declaration.
*/
{
  if (annotate) {
    /* Dump any pragmas associated with the constant. */
    dump_associated_pragmas(constant);
    set_output_position(&constant->source_corresp.decl_position);
    write_tok_str("enum {");
    dump_constant_name(constant);
    write_tok_str(" = ");
    check_assertion_str(is_integral_or_enum_type(constant->type),
                        "dump_constant_decl: non-integral constant");
    dump_constant(constant);
    write_tok_str("};");
  }  /* if */
}  /* dump_constant_decl */


static void dump_scope_constants(a_scope_ptr scope)
/*
Dump all constants in the indicated scope.
*/
{
  a_constant_ptr constant;

  for (constant = scope->constants;
       constant != NULL;
       constant = constant->next) {
    check_membership_info(constant, scope);
    dump_constant_decl(constant);
  }  /* for */
}  /* dump_scope_constants */

#if GNU_EXTENSIONS_ALLOWED

static void dump_local_label_declarations(a_scope_ptr  scope)
/*
Dump out the local label declarations (a GNU C extension) of the given scope
(if any).  Unused local labels shouldn't be dumped since GNU compilers will
issue diagnostics for them (warnings or errors depending on the version).
*/
{
  a_label_ptr  label;

  for (label = scope->labels; label != NULL; label = label->next) {
    if (label->locally_declared &&
        start_unreferenced_bracket(&label->source_corresp,
                                   (a_boolean *)NULL)) {
      write_tok_str("__label__ ");
      dump_label_name(label);
      write_tok_str(";");
      end_output_line();
      end_unreferenced_bracket(&label->source_corresp);
    }  /* if */
  }  /* for */
}  /* dump_local_label_declarations */

#endif /* GNU_EXTENSIONS_ALLOWED */

static void dump_block_declarations(a_statement_ptr statement)
/*
Dump out the declarations (if any) for a block.
*/
{
  a_scope_ptr   scope;
  a_routine_ptr rout = NULL;

  /* Recognize the top block in a function when it comes by (that statement
     does not have a scope pointer even though there is an associated
     scope). */
  if (innermost_function_scope->assoc_block == statement) {
    scope = innermost_function_scope;
    rout = innermost_function_scope->variant.routine.ptr;
  } else {
    scope = statement->variant.block.extra_info->assoc_scope;
  }  /* if */
  if (scope != NULL) {
    /* Set the new current scope.  The caller restores the old value. */
    curr_scope = scope;
    /* Constants and routines do not exist at this level and therefore
       need not be dumped. */
    /* Subscopes are processed when the associated block statement is
       encountered. */
    /* Local types are dumped out as part of the file scope. */
#if GNU_EXTENSIONS_ALLOWED
    dump_local_label_declarations(scope);
#endif /* GNU_EXTENSIONS_ALLOWED */
    dump_scope_pragmas(scope);
    dump_scope_variables(scope,
                         /*interleave_asm_decls=*/FALSE,
                         /*dump_vars_without_initializers=*/TRUE,
                         /*dump_initializers=*/TRUE);
    dump_prescan_temps(statement->variant.block.statements);
    dump_rout_initializations(rout);
    /* If the first statement in the block has no source position, set the
       output position to the beginning of the block. */
    if (statement->variant.block.statements != NULL &&
        statement->variant.block.statements->position.seq == 0) {
      set_output_position(&statement->position);
    }  /* if */
  }  /* if */
}  /* dump_block_declarations */


static void dump_block(a_statement_ptr statement)
/*
Dump out the contents of a block (but not the surrounding { and }).
*/
{
  /* curr_scope is saved and restored by this routine.  It is set to the
     new scope by dump_block_declararations, if appropriate. */
  a_scope_ptr    saved_curr_scope = curr_scope;
  a_constant_ptr saved_wide_string_constants_to_unbind_at_end_of_scope =
                               wide_string_constants_to_unbind_at_end_of_scope;

#if UPC_EXTENSIONS_ALLOWED
  if (statement->variant.block.extra_info != NULL) {
    /* If necessary, output a pragma to override the default UPC access
       method. */
    a_upc_access_method  access_method = statement->variant.block.extra_info
                                                  ->upc_access_method;
    if (access_method == (a_upc_access_method)upc_access_unspecified) {
      /* No pragma needed. */
    } else if (access_method == (a_upc_access_method)upc_access_strict) {
      write_pp_directive("#pragma upc strict", (char*)NULL);
    } else if (access_method == (a_upc_access_method)upc_access_relaxed) {
      write_pp_directive("#pragma upc relaxed", (char*)NULL);
    } else {
      unexpected_condition();
    }  /* if */
  }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
  wide_string_constants_to_unbind_at_end_of_scope =
                                                  &wide_string_constant_marker;
  dump_block_declarations(statement);
  dump_statement_list(statement->variant.block.statements);
  curr_scope = saved_curr_scope;
  unbind_wide_string_constants(
                        saved_wide_string_constants_to_unbind_at_end_of_scope);
}  /* dump_block */

#if ASM_FUNCTION_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED

static void dump_asm_function_body(a_const_char *p)
/*
Generate an asm function body, including the opening and closing braces.
p is a pointer to the start of a null-terminated string.
*/
{
  a_const_char	*eol;
  a_boolean	add_braces;

  /* Add braces unless the string already has them. */
  add_braces = *p != '{';
  write_space();
  if (add_braces) write_tok_ch('{');
  for (; (eol = strchr(p, '\n')) != NULL; p = eol+1) {
    /* Write a sequence of characters ending with a newline. */
    *(char *)eol = '\0';
    write_str(p);
    end_output_line();
    *(char *)eol = '\n';
  }  /* for */
  write_str(p);
  if (add_braces) write_tok_ch('}');
}  /* dump_asm_function_body */

#endif /* ASM_FUNCTION_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */

static void dump_switch_case(a_statement_ptr  stmt)
/*
Generate the code for a "case ... :" or "default:" label in a switch statement.
*/
{
  a_switch_case_entry_ptr  scep = stmt->variant.switch_case.extra_info;

  check_assertion_str(scep->stmt != NULL &&
                      scep->stmt->variant.switch_case.switch_statement
                                                                     != NULL &&
                      scep->stmt->variant.switch_case.switch_statement->kind
                                              == (a_statement_kind)stmk_switch,
                   "dump_switch_case: case statement doesn't point to switch");
  if (scep->case_value == NULL) {
    /* The default case. */
    write_tok_str("default:");
  } else {
    write_tok_str("case ");
    dump_constant(scep->case_value);
#if GNU_EXTENSIONS_ALLOWED
    if (scep->range_end != NULL) {
      /* A GNU case range. */
      write_tok_str(" ... ");
      dump_constant(scep->range_end);
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    write_tok_ch(':');
  }  /* if */
  /* If no statement follows, add an empty statement to make the generated
     code valid.  E.g., generate "{ case 2:; }" rather than "{ case 2: }".
     Skip over declaration statements, since they aren't rendered here. */
  while (stmt->next != NULL &&
         stmt->next->kind == (a_statement_kind)stmk_decl) {
    stmt = stmt->next;
  }  /* while */
  if (stmt->next == NULL) {
    write_tok_ch(';');
  }  /* if */
}  /* dump_switch_case */


static void dump_switch_statement(a_statement_ptr statement)
/*
Generate the code for a switch statement.
*/
{
  write_tok_str("switch (");
  dump_expression(statement->expr);
  write_tok_str(")");
  dump_statement(statement->variant.switch_stmt.body_statement);
}  /* dump_switch_statement */


static void dump_dynamic_init(a_dynamic_init_ptr        dip, 
                              an_init_control_block_ptr icbp)
/*
Dump code for a dynamic initialization operation.  This routine only emits
code for non-constant initializations; the constant initializations are
handled in declaration processing in dump_variable_decl.  icbp points to
a control block with state information about this initializer.
*/
{
  a_variable_ptr        variable = dip->variable;
  FILE                  *save_f_C_output;
  a_boolean             is_vector_constant = FALSE;

#if GNU_VECTOR_TYPES_ALLOWED
  if ((dip->kind == (a_dynamic_init_kind)dik_constant ||
       dip->kind == (a_dynamic_init_kind)dik_nonconstant_aggregate) &&
      dip->variant.constant.ptr->kind == (a_constant_repr_kind)ck_aggregate &&
      is_vector_type(dip->variant.constant.ptr->type)) {
    is_vector_constant = TRUE;
  }  /* if */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
  /* Direct the assignment output to the proper file. */
  set_init_file(variable, &save_f_C_output);
  if ((dip->kind == (a_dynamic_init_kind)dik_constant ||
       dip->kind == (a_dynamic_init_kind)dik_nonconstant_aggregate) &&
      !is_vector_constant &&
      (dip->variant.constant.ptr->kind == (a_constant_repr_kind)ck_aggregate ||
       dip->variant.constant.ptr->kind == (a_constant_repr_kind)ck_string)) {
    /* Aggregate initialization.  Only comes up in C++, for aggregate
       initializations to constants done in the middle of blocks. */
    a_boolean  gen_assignments = TRUE;
    dump_initializer_part(variable, variable->type, dip->variant.constant.ptr,
                          &gen_assignments, (a_gen_init_pos_descr_ptr)NULL,
                          icbp);
  } else {
    set_output_position(&variable->source_corresp.decl_position);
    switch (dip->kind) {
      case dik_constant:
      case dik_nonconstant_aggregate:
        /* Initialization to a simple constant.  Output
             variable = constant;
        */
        dump_variable_name(variable);
        write_tok_str(" = ");
        if (is_vector_constant) {
          /* Vector constants take the form of compound literals.  The
             call to dump_constant below will render the { ... } part of the
             literal, but a leading cast must first be emitted. */
          /* is_vector_constant is always FALSE in some configurations. */
          /* coverity[dead_error_line] */
          dump_cast(dip->variant.constant.ptr->type);
        }  /* if */
        dump_constant(dip->variant.constant.ptr);
        write_tok_ch(';');
        break;
      case dik_expression:
        /* Initialization to an expression.  Output
             variable = expression;
        */
        if (is_class_struct_union_type(dip->variant.expression->type) &&
            skip_typerefs(dip->variant.expression->type)->
                                   variant.class_struct_union.is_empty_class) {
          /* No need to assign an empty class, but make sure any side-effects
             are performed. */
          if (c_gen_node_has_side_effects(dip->variant.expression,
					   (a_boolean*)NULL)) {
            dump_expr_with_parens(dip->variant.expression);
            write_tok_ch(';');
          }  /* if */
        } else {
          dump_variable_name(variable);
          write_tok_str(" = ");
          /* Parentheses are required because of the possibility that the
             top-level operator is a ",". */
          dump_expr_with_parens(dip->variant.expression);
          write_tok_ch(';');
        }  /* if */
        break;
      default:
        unexpected_condition_str("dump_dynamic_init: bad kind");
    }  /* switch */
  }  /* if */
  unset_init_file(save_f_C_output);
}  /* dump_dynamic_init */


static void dump_whole_variable_dynamic_init(a_dynamic_init_ptr dip)
/*
Generate code for a dynamic initialization that applies to a whole variable.
This is used for stmk_init statements.
*/
{
  a_variable_ptr        whole_variable = dip->variable;
  a_boolean             init_already_done = FALSE;
  an_init_kind          init_kind;
  an_init_control_block icb;

  /* If the initial value is a constant, the initialization was
     done in dump_variable_decl and should not be done here. */
  if (constant_initializer(whole_variable, &init_kind) != NULL) {
    init_already_done = TRUE;
  } else if (dip->kind == (a_dynamic_init_kind)dik_none) {
    /* No initialization to be done. */
    init_already_done = TRUE;
  }  /* if */
  if (!init_already_done) {
    /* Initialization needs to be done.  It wasn't done by
       dump_variable_decl. */
    clear_initialization_flags(&icb);
    /* Because this initialization is being done by assignments, make sure
       that we don't inadvertently add an erroneous "=" preceding a
       compound literal. */
    icb.suppress_initializer_equals = TRUE;
    start_initializer_assignments(whole_variable, &icb);
    dump_dynamic_init(dip, &icb);
    end_initializer_assignments(whole_variable, &icb);
  }  /* if */
}  /* dump_whole_variable_dynamic_init */


static a_boolean is_implicit_return(a_statement_ptr statement)
/*
Return TRUE if the indicated return statement is an implicit return
statement which therefore should not be put out.
*/
{
  a_boolean is_implicit;

  if (statement->expr != NULL ||
      statement->position.seq != 0 ||
      statement->next != NULL) {
    /* A return with a source position, with an expression, or that
       is not the last in its statement list -- not implicit. */
    is_implicit = FALSE;
  } else {
    /* Check that the statement is the last in the last block
       of the function.  In constructors with function try blocks
       an implicit return can be nested inside the try and shouldn't
       be removed. */
    a_statement_ptr stmt = innermost_function_scope->assoc_block;
    while (stmt->kind == (a_statement_kind)stmk_block) {
      stmt = stmt->variant.block.statements;
      if (stmt == NULL) break;
      while (stmt->next != NULL) stmt = stmt->next;
    }  /* while */
    is_implicit = (stmt == statement);
  }  /* if */
  return is_implicit;
}  /* is_implicit_return */


static void dump_statement(a_statement_ptr statement)
/*
Generate C for a statement.
*/
{
  a_statement_ptr  init_stmt;
  an_expr_node_ptr init_expr;
  a_statement_kind kind;

  check_assertion(statement != NULL);
  kind = statement->kind;
  /* Dump out any pragmas associated with the statement. */
  dump_associated_pragmas(statement);
  /* Identify the line number except for lines that put out their own
     line info. */
  if (kind != (a_statement_kind)stmk_label &&
      kind != (a_statement_kind)stmk_for &&
#if UPC_EXTENSIONS_ALLOWED
      kind != (a_statement_kind)stmk_upc_forall &&
#endif /* UPC_EXTENSIONS_ALLOWED */
      kind != (a_statement_kind)stmk_init &&
      kind != (a_statement_kind)stmk_asm &&
      kind != (a_statement_kind)stmk_decl) {
    set_output_position(&statement->position);
  }  /* if */
  switch (kind) {
    case stmk_empty:
      write_tok_ch(';');
      break;
    case stmk_expr:
#if CHECKING
      check_result_not_used_flag(statement->expr);
#endif /* CHECKING */
      /* See if we need to "inline" a call to a master routine. */
      if (!replace_call_to_master_routine(statement->expr)) {
        dump_expression(statement->expr);
        write_tok_ch(';');
      }  /* if */
      break;
    case stmk_constexpr_if:
      {
        a_constexpr_if_ptr cip = statement->variant.constexpr_if;
        a_statement_ptr    then_statement, else_statement;
        then_statement = statement->variant.constexpr_if->then_statement;
        else_statement = statement->variant.constexpr_if->else_statement;
        if (cip->value) {
          dump_statement(then_statement);
        } else {
          if (else_statement != NULL) {
            dump_statement(else_statement);
          }  /* if */
        }  /* if */
      }
      break;
    case stmk_if:
      {
        a_statement_ptr then_statement, else_statement;
        then_statement = statement->variant.if_stmt.then_statement;
        else_statement = statement->variant.if_stmt.else_statement;
#if ADD_BRACES_TO_AVOID_DANGLING_ELSE_IN_GENERATED_C
        /* Add braces around an "if" without an "else" to avoid the "dangling
           else" problem.  This is necessary only if customer code modifies
           the IL tree. */
        if (else_statement == NULL) {
          write_tok_ch('{');
        }  /* if */
#endif /* ADD_BRACES_TO_AVOID_DANGLING_ELSE_IN_GENERATED_C */
        write_tok_str("if ");
        dump_boolean_controlling_expression(statement->expr);
        /* Dump the "then" part. */
        indent += 2;
        dump_statement(then_statement);
        indent -= 2;
        if (else_statement != NULL) {
          /* Use the position from the "else" statement for the keyword. */
          set_output_position(&else_statement->position);
          write_tok_str("else ");
          indent += 2;
          dump_statement(else_statement);
          indent -= 2;
#if ADD_BRACES_TO_AVOID_DANGLING_ELSE_IN_GENERATED_C
        } else {
          /* Close the set of braces begun above. */
          write_tok_ch('}');
#endif /* ADD_BRACES_TO_AVOID_DANGLING_ELSE_IN_GENERATED_C */
        }  /* if */
      }
      break;
    case stmk_if_consteval:
      /* Only dump the "else" statement (if there is one), since we are
         generating "run time" code. */
      if (statement->variant.if_stmt.else_statement != NULL) {
        dump_statement(statement->variant.if_stmt.else_statement);
      }  /* if */
      break;
    case stmk_if_not_consteval:
      /* Only dump the "then" statement, since we are generating "run time"
         code. */
      dump_statement(statement->variant.if_stmt.then_statement);
      break;
    case stmk_while:
      write_tok_str("while ");
      dump_boolean_controlling_expression(statement->expr);
      indent += 2;
      dump_statement(statement->variant.loop_statement);
      indent -= 2;
      break;
    case stmk_for:
#if UPC_EXTENSIONS_ALLOWED
    case stmk_upc_forall:
#endif /* UPC_EXTENSIONS_ALLOWED */
      /* Put the initializing statement outside the "for" if it's not
         a simple expression statement. */
      init_stmt = statement->variant.for_loop.extra_info->initialization;
      if (init_stmt == NULL) {
        init_expr = NULL;
      } else {
        /* Lowering (C++ or C99) should have moved complex initializations
           out of the statement. */
        check_assertion(init_stmt->kind == (a_statement_kind)stmk_expr &&
                        init_stmt->next == NULL);
        /* Simple C89-like initialization expression. */
        init_expr = init_stmt->expr;
      }  /* if */
      set_output_position(&statement->position);
#if UPC_EXTENSIONS_ALLOWED
      write_tok_str(kind == (a_statement_kind)stmk_for ?
                                                       (char *)"for (" :
                                                       (char *)"upc_forall (");
#else /* !UPC_EXTENSIONS_ALLOWED */
      write_tok_str("for (");
#endif /* UPC_EXTENSIONS_ALLOWED */
      if (init_expr != NULL) {
#if CHECKING
        check_result_not_used_flag(init_expr);
#endif /* CHECKING */
        dump_expression(init_expr);
      }  /* if */
      write_tok_str("; ");
      if (statement->expr != NULL) {
        /* Do not wrap the for loop condition in parens.  Unlike other control
           flow statements, the for loop does not syntactically require these
           (and thus adding them can lead to spurious diagnostics from the
           target compiler). */
        dump_boolean_controlling_expression(statement->expr,
                                            /*wrap_with_parens=*/FALSE);
      }  /* if */
      write_tok_str("; ");
      if (statement->variant.for_loop.extra_info->increment != NULL) {
        an_expr_node_ptr incr =
                             statement->variant.for_loop.extra_info->increment;
#if CHECKING
        check_result_not_used_flag(incr);
#endif /* CHECKING */
        dump_expression(incr);
      }  /* if */
#if UPC_EXTENSIONS_ALLOWED
      if (kind == (a_statement_kind)stmk_upc_forall) {
        /* Output the UPC affinity clause. */
        an_expr_node_ptr  affinity = statement->variant.for_loop.extra_info
                                              ->affinity;
        if (affinity != NULL) {
          write_tok_ch(';');
          dump_expression(affinity);
        } else {
          write_tok_str("; continue");
        }  /* if */
      }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
      write_tok_ch(')');
      indent += 2;
      dump_statement(statement->variant.for_loop.statement);
      indent -= 2;
      break;
    case stmk_goto:
      write_tok_str("goto ");
      dump_label_name(statement->variant.label.ptr);
      write_tok_ch(';');
      break;
#if GNU_EXTENSIONS_ALLOWED
    case stmk_assigned_goto:
      write_tok_str("goto *");
      dump_expr_with_parens(statement->expr);
      write_tok_ch(';');
      break;
    case stmk_stmt_expr_result:
      check_assertion(statement->variant.stmt_expr_result.dynamic_init ==
                                                                         NULL);
      dump_expression(statement->expr);
      write_tok_ch(';');
      break;
#endif /* GNU_EXTENSIONS_ALLOWED */
    case stmk_label:
      if (start_unreferenced_bracket(
                                 &statement->variant.label.ptr->source_corresp,
                                 (a_boolean *)NULL)) {
        set_output_position(&statement->position);
        dump_label_name(statement->variant.label.ptr);
        write_tok_ch(':');
#if GNU_EXTENSIONS_ALLOWED
        /* Emit attributes associated with the label. */
        (void)form_label_attributes(statement->variant.label.ptr,
                                    /*need_leading_space=*/TRUE, &octl);
#endif /* GNU_EXTENSIONS_ALLOWED */
        end_unreferenced_bracket(
                                &statement->variant.label.ptr->source_corresp);
      }  /* if */
      write_tok_ch(';');
      break;
    case stmk_return:
      check_assertion_str(statement->variant.return_dynamic_init == NULL,
                          "dump_statement: return with dyn init");
      if (master_routine_scope != NULL &&
          innermost_function_scope == master_routine_scope) {
        /* We're "inlining" a master routine within an alternate entry routine
           Replace any returns within this master routine with a jump to the
           end of the master routine. */
        if (master_routine_return_variable != NULL) {
          /* If the return specifies a value, assign the value to the return
             variable before the jump to the end of the master routine. */
          check_assertion(statement->expr != NULL &&
                          !is_implicit_return(statement));
#if !STANDALONE_C_GEN_BE
          check_assertion(il_identical_types(statement->expr->type,
                                        master_routine_return_variable->type));
#endif /* !STANDALONE_C_GEN_BE */
          /* Generate an assignment statement to assign the returned expression
             to the return variable. */
          dump_variable_name(master_routine_return_variable);
          write_tok_str(" = ");
          dump_expr_with_parens(statement->expr);
          write_tok_ch(';');
        } else {
          check_assertion(statement->expr == NULL);
        }  /* if */
        write_tok_str("goto ");
        write_tok_str(end_of_master_routine_label);
        write_tok_ch(';');
      } else {
        /* Do not put out an implicit return. */
        if (!is_implicit_return(statement)) {
          write_tok_str("return");
          if (statement->expr != NULL) {
            write_space();
            dump_expression(statement->expr);
          }  /* if */
          write_tok_ch(';');
        }  /* if */
      }  /* if */
      break;
    case stmk_block:
      write_tok_ch('{');
      indent += 2;
      dump_block(statement);
      indent -= 2;
      set_output_position(
                         &statement->variant.block.extra_info->final_position);
      write_tok_ch('}');
      break;
    case stmk_end_test_while:
      write_tok_str("do");
      indent += 2;
      dump_statement(statement->variant.loop_statement);
      indent -= 2;
      write_tok_str(" while ");
      dump_boolean_controlling_expression(statement->expr);
      write_tok_ch(';');
      break;
    case stmk_switch_case:
      dump_switch_case(statement);
      break;
    case stmk_switch:
      dump_switch_statement(statement);
      break;
    case stmk_init:
      {
        /* Dynamic initialization. */
        /* The executable code can be output directly to f_C_output instead
           of to a temporary file, because we're in the executable code part of
           the current routine. */
        a_boolean saved_output_initializer_code_directly =
                                              output_initializer_code_directly;
        output_initializer_code_directly = TRUE;
        dump_whole_variable_dynamic_init(statement->variant.dynamic_init);
        output_initializer_code_directly =
                                        saved_output_initializer_code_directly;
      }
      break;
    case stmk_asm:
      /* asm statement. */
      dump_asm_entry(statement->variant.asm_entry);
      break;
#if ASM_FUNCTION_ALLOWED
    case stmk_asm_func_body:
      /* asm function body -- generate "{ ... }". */
      dump_asm_function_body(statement->variant.asm_func_body);
      break;
#endif /* ASM_FUNCTION_ALLOWED */
#if !DO_FULL_PORTABLE_EH_LOWERING
    /* This code is here as a debugging aid.  Normally, this statement is
       not seen by the C-generating back end. */
    case stmk_try_block:
      write_tok_str("try");
      indent += 2;
      dump_statement(statement->variant.try_block->statement);
      { a_handler_ptr handler;
        for (handler = statement->variant.try_block->handlers;
             handler != NULL;
             handler = handler->next) {
          a_variable_ptr param = handler->parameter;
          set_output_position(&handler->catch_position);
          write_tok_str("catch (");
          if (param == NULL) {
            write_tok_str("...");
          } else {
            dump_general_declaration_using_type(param->type,
                                                &param->source_corresp,
                                                param, NO_ROUTINE, NO_FIELD,
                                                NO_TEMP,NO_NAME, TQ_NONE,
                                                /*suppress_const=*/FALSE,
                                                NO_COUNTER);
          }  /* if */
          write_tok_str(")");
          indent += 2;
          dump_statement(handler->statement);
          indent -= 2;
        }  /* for */
      }
      indent -= 2;
      break;
#endif /* !DO_FULL_PORTABLE_EH_LOWERING */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case stmk_microsoft_try:
      write_tok_str("__try");
      indent += 2;
      dump_statement(statement->variant.microsoft_try->guarded_statement);
      indent -= 2;
      if (statement->variant.microsoft_try->except_expr != NULL) {
        write_tok_str("__except (");
        dump_expression(statement->variant.microsoft_try->except_expr);
        write_tok_str(")");
      } else {
        write_tok_str(" __finally");
      }  /* if */
      indent += 2;
      dump_statement(statement->variant.microsoft_try->cleanup_statement);
      indent -= 2;
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case stmk_decl:
      /* Statement that marks the location of declarations.  Ignored here. */
      break;
    case stmk_set_vla_size:
#if LOWER_VARIABLE_LENGTH_ARRAYS
      unexpected_condition_str("VLA statement unexpected");
#else /* !LOWER_VARIABLE_LENGTH_ARRAYS */
      /* An unlowered C++ VLA: The dimension is usually stored in a temporary
         that must be initialized here (the exception are VLAs in prototype
         scopes).  form_array_declarator creates references to the variable.
         Doing things this way avoids duplicating side-effects when lowering
         duplicates the type.  (See also create_dimension_variable, which turns
         the expression pointed to by the dimension_expr field into an
         assignment to the variable pointed to by dimension_variable.) */
      if (statement->variant.vla_dimension->dimension_variable != NULL) {
        dump_expression(statement->variant.vla_dimension->dimension_expr);
        write_tok_ch(';');
      }  /* if */
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
      /* No output. */
      break;
    case stmk_vla_decl:
#if LOWER_VARIABLE_LENGTH_ARRAYS
      unexpected_condition_str("VLA statement unexpected");
#else /* !LOWER_VARIABLE_LENGTH_ARRAYS */
      if (statement->variant.vla.is_typedef_decl) {
        /* Dump out the declaration of a typedef for a variably-modified type
           at the point where it occurs in the executable code sequence. */
        dump_type_decl(statement->variant.vla.variant.typedef_type,
                       /*pass=*/2);
      } else {
        /* Dump out the declaration of a variable with a variably modified
           type at the point where it occurs in the executable code
           sequence. */
        dump_variable_decl(statement->variant.vla.variant.variable,
                           /*dump_vars_without_initializers=*/TRUE,
                           /*dump_initializers=*/TRUE);
      }  /* if */
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
      break;
#if UPC_EXTENSIONS_ALLOWED
    case stmk_upc_notify:
    case stmk_upc_wait:
    case stmk_upc_barrier:
      write_tok_str(kind == (a_statement_kind)stmk_upc_notify ?
                                                        (char *)"upc_notify" :
                    kind == (a_statement_kind)stmk_upc_wait ?
                                                        (char *)"upc_wait" :
                                                        (char *)"upc_barrier");
      if (statement->expr != NULL) {
        write_space();
        dump_expression(statement->expr);
      }  /* if */
      write_tok_ch(';');
      break;
    case stmk_upc_fence:
      write_tok_str("upc_fence;");
      break;
#endif /* UPC_EXTENSIONS_ALLOWED */
    case stmk_coroutine:
      /* The body should already be generated, so there's not much to do with
         this statement. */
      check_assertion(statement->variant.coroutine.descr->body_generated);
      break;
    case stmk_coroutine_return:
      /* A stmk_coroutine_return has an expression that takes one of the
         following forms:
           p.return_value(<expr>)
           <expr>, p.return_void()
           p.return_void()
         and is always followed by a goto final_suspend statement (which will
         be dumped next).  This may *not* be statement->next, however,
         depending on how the IL has been lowered up until this point. */
      dump_expression(statement->expr);
      write_tok_ch(';');
      break;
    default:
      unexpected_condition_str("dump_statement: bad statement kind");
  }  /* switch */
}  /* dump_statement */


static void dump_statement_list(a_statement_ptr statement)
/*
Generate code for the indicated list of statements.
*/
{
  a_boolean     exec_stmt_put_out = FALSE;
  uint32_t      num_closing_braces_needed = 0;

  for (; statement != NULL; statement = statement->next) {
    /* Put out extra braces before declarative statements that would
       otherwise be put out after executable statements in a block. */
    a_boolean is_exec_stmt =
                      (statement->kind != (a_statement_kind)stmk_decl &&
                       statement->kind != (a_statement_kind)stmk_vla_decl &&
                       statement->kind != (a_statement_kind)stmk_set_vla_size);
    if (statement->kind == (a_statement_kind)stmk_vla_decl &&
        exec_stmt_put_out) {
      write_tok_ch('{');
      num_closing_braces_needed++;
      exec_stmt_put_out = FALSE;
    }  /* if */
    dump_statement(statement);
    if (is_exec_stmt) exec_stmt_put_out = TRUE;
  }  /* for */
  while (num_closing_braces_needed-- > 0) write_tok_ch('}');
}  /* dump_statement_list */


static void dump_expr_prescan_temps(
                         an_expr_node_ptr                               node,
                         ARG_UNUSED an_expr_or_stmt_traversal_block_ptr tblock)
/*
Called from the expression/statement traversal routines to put out
prescan temporaries in the indicated expression.
*/
{
  if (node->kind == (an_expr_node_kind)enk_operation) {
    an_expr_operator_kind op = node->variant.operation.kind;
    an_expr_node_ptr      op1 = node->variant.operation.operands;
    a_type_ptr            op1_type = op1->type;
    a_boolean             need_temp = FALSE;
    if (op == (an_expr_operator_kind)eok_dot_field) {
      if (!op1->is_lvalue) {
        /* Selection of a field from an rvalue; may need a temp for the
           struct/union. */
        a_boolean comma_case;
        if (optimizable_rvalue_selection(node, &comma_case)) {
          /* The transformation can be optimized and does not need the temp.
             See dump_field_selection. */
        } else {
          need_temp = TRUE;
        }  /* if */
      } else {
        /* If the object expression is based on a comma node, it may need
           a temporary. */
        need_temp = obj_expr_based_on_comma(node);
      }  /* if */
      if (need_temp) {
        /* Declare the temporary. */
        dump_general_declaration_using_type(op1_type, NO_SCP, NO_VARIABLE,
                                            NO_ROUTINE, NO_FIELD, (char *)node,
                                            NO_NAME, TQ_NONE,
                                            /*suppress_const=*/FALSE,
                                            NO_COUNTER);
        write_tok_ch(';');
      }  /* if */
    }  /* if */
  }  /* if */
}  /* dump_expr_prescan_temps */


static void dump_constant_prescan_temps(
                         a_constant_ptr                                 con,
                         ARG_UNUSED an_expr_or_stmt_traversal_block_ptr tblock)
/*
Called from the expression/statement traversal routines to put out
prescan temporaries for the indicated constant.
*/
{
  if (con->kind == (a_constant_repr_kind)ck_string &&
      !is_normal_character_kind(con->character_kind)) {
    /* When the value is a wide string literal, replace it by a variable. */
    dump_var_for_wide_string_constant(con);
  }  /* if */
}  /* dump_constant_prescan_temps */


static void dump_statement_prescan_temps(
                                 a_statement_ptr                     statement,
                                 an_expr_or_stmt_traversal_block_ptr tblock)
/*
Called from the expression/statement traversal routines to put out
prescan temporaries in the indicated statement.
*/
{
  switch (statement->kind) {
    case stmk_block:
      /* If the block has its own scope, do not prescan now for temporaries;
         that should be done once the block itself is started. */
      if (statement->variant.block.extra_info->assoc_scope != NULL) {
        tblock->suppress_subtree_walk = TRUE;
      }  /* if */
      break;
    case stmk_switch:
      { a_statement_ptr body_statement =
                                 statement->variant.switch_stmt.body_statement;
        /* If the body statement has its own scope, do not prescan now for
           temporaries; that should be done once the block itself is
           started. */
        if (body_statement != NULL &&
            body_statement->kind == (a_statement_kind)stmk_block &&
            body_statement->variant.block.extra_info->assoc_scope != NULL) {
          /* The body statement is a block with a scope. */
          /* We do need to scan the expression now, because it won't be
             handled by the traversal routines given that we suppress
             the subtree walk. */
          traverse_expr(statement->expr, tblock);
          tblock->suppress_subtree_walk = TRUE;
        }  /* if */
      }
      break;
    default:
      break;
  }  /* switch */
}  /* dump_statement_prescan_temps */


static void set_up_prescan_traversal_block(
                                    an_expr_or_stmt_traversal_block_ptr tblock)
/*
Set up the control block used for the prescan traversal.
*/
{
  clear_expr_or_stmt_traversal_block(tblock);
  tblock->process_expr = dump_expr_prescan_temps;
  tblock->process_statement = dump_statement_prescan_temps;
  tblock->process_constant = dump_constant_prescan_temps;
  /* We need to look at all aggregates to rewrite wide strings. */
  tblock->process_non_dynamic_constants = TRUE;
}  /* set_up_prescan_traversal_block */


static void dump_prescan_temps(a_statement_ptr statement_list)
/*
Dump declarations for any temporaries required for the statement list and
its subtree.
*/
{
  an_expr_or_stmt_traversal_block tblock;

  set_up_prescan_traversal_block(&tblock);
  traverse_statement_list(statement_list, &tblock);
}  /* dump_prescan_temps */


static void dump_old_style_parameter_decls(a_scope_ptr           scope,
					   ARG_UNUSED a_type_ptr type)
/*
Generate parameter declarations for the definition of an unprototyped
function.  scope is the associated scope, type is the type of the
routine whose parameters are being processed.
*/
{
  a_variable_ptr param_var;

  /* Declare the parameter variables. */
  for (param_var = scope->variant.routine.parameters;
       param_var != NULL;
       param_var = param_var->next) {
    set_output_position(&param_var->source_corresp.decl_position);
    dump_general_declaration_using_type(param_var->type,
                                        &param_var->source_corresp,
                                        param_var, NO_ROUTINE, NO_FIELD,
                                        NO_TEMP, NO_NAME, TQ_NONE,
                                        /*suppress_const=*/FALSE,
                                        NO_COUNTER);
#if GNU_EXTENSIONS_ALLOWED
    (void)form_variable_attributes(param_var, /*need_leading_space=*/TRUE,
                                   &octl);
#endif /* GNU_EXTENSIONS_ALLOWED */
    write_tok_ch(';');
  }  /* for */
  {
#ifdef __hpux
    /* On HP/UX, when generating a function with a variable argument list,
       a declaration must be supplied for the special "va_alist" parameter. */
    a_routine_type_supplement_ptr rtsp = type->variant.routine.extra_info;
    if (rtsp->has_ellipsis) write_tok_str(" long va_alist;");
#endif /* ifdef __hpux */
#ifdef __sgi
    /* On SGI, when generating a function with a variable argument list,
       a declaration must be supplied for the special "va_alist" parameter. */
    a_routine_type_supplement_ptr rtsp = type->variant.routine.extra_info;
    if (rtsp->has_ellipsis) write_tok_str(" int va_alist;");
#endif /* ifdef __sgi */
  }
}  /* dump_old_style_parameter_decls */


static void dump_func_definition_type(a_routine_ptr rout,
                                      a_scope_ptr   scope)
/*
Generate the routine name and type, including the parameter declarations,
for the definition of the indicated routine.  scope is the associated scope.
*/
{
  /* There can be qualifiers above the function type for Microsoft qualifiers
     like near/far. */
  a_type_ptr type = skip_typerefs(rout->type);

  check_assertion_str(type->kind == (a_type_kind)tk_routine,
                      "dump_func_definition_type: type not routine");
  /* The storage class and similar preamble have already been written. */
#if GNU_EXTENSIONS_ALLOWED
  /* Any routine attributes should immediately precede the return type. */
  if (form_routine_attributes(rout, /*need_leading_space=*/FALSE, &octl)) {
    m_write_space();
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  /* Write the specifiers and the first part of the declarator. */
  form_type_first_part_simple(type->variant.routine.return_type,
                              /*under_lhs_declarator=*/FALSE,
                              /*need_trailing_space=*/TRUE, &octl);
  /* Write the name. */
  dump_routine_name(rout);
  /* Write the second part of the declarator. */
  dump_function_declarator_with_scope(type, scope);
  form_type_second_part_simple(type->variant.routine.return_type,
                               /*under_lhs_declarator=*/FALSE, &octl);
#if C_GEN_BE_GENERATES_ANSI_C
  /* For an old-style function, declare the parameters. */
  /* Note that this does not use the "prototyped" flag, which is inaccurate
     when there is a prototyped declaration and an old-style definition. */
  /* Note that IL lowering creates routines with prototyped FALSE but
     old_style_params_scanned also FALSE. */
  if (!type->variant.routine.extra_info->prototyped ||
      type->variant.routine.extra_info->old_style_params_scanned) {
#endif /* C_GEN_BE_GENERATES_ANSI_C */
#if ASM_FUNCTION_ALLOWED
    if (!within_asm_function_definition)
#endif /* ASM_FUNCTION_ALLOWED */
      dump_old_style_parameter_decls(scope, type);
#if C_GEN_BE_GENERATES_ANSI_C
  }  /* if */
#endif /* C_GEN_BE_GENERATES_ANSI_C */
}  /* dump_func_definition_type */


static a_scope_ptr get_scope_for_routine_definition(
                                         a_routine_ptr          rout,
                                         a_memory_region_number *region_number)
/*
Return a pointer to the top-level scope for the definition of the indicated
routine.  If an IL file is being used, read in the memory region.
Set *region_number to the function memory region number.
*/
{
  a_scope_ptr scope;

  *region_number = mem_region_for_routine(rout);
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  /* Read the information for the function from the IL file.  This must be
     read before the interface is generated in order to get the parameter
     names. */
  if (mem_region_table[*region_number] == NULL) {
    read_memory_region(*region_number);
  }  /* if */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
  scope = scope_for_routine(rout);
  return scope;
}  /* get_scope_for_routine_definition */

#if IA64_ABI

static uint32_t num_parameters(a_scope_ptr scope)
/*
Return the count of parameters for the indicated function scope.
*/
{
  a_variable_ptr param_var;
  uint32_t       num;

  for (param_var = scope->variant.routine.parameters, num = 0;
       param_var != NULL;
       param_var = param_var->next, num++) {}
  return num;
}  /* num_parameters */

#endif /* IA64_ABI */

static a_boolean replace_call_to_master_routine(an_expr_node_ptr expr)
/*
If the specified expression is a call to the master routine from an
alternate entry routine, replace the call with an "inline" version of the
master routine.  The "call" can also take the form of an
enk_result_of_overriding_function node, which is replaced when
master_routine_scope is not NULL.  This code depends on lowering generating
eok_call/enk_result_of_overriding_function expression nodes that are either at
the top of an expression statement or as the rvalue of an assignment expression
to a temporary variable (it cannot "inline" a "call" in an arbitrary expression
context).  This is used only when the alternate entry has an ellipsis
parameter.  Returns TRUE if the expression was replaced.
*/
{
  a_boolean     replaced = FALSE;
  a_boolean     replacing_roof = FALSE;

  check_assertion(expr != NULL);
  if (entry_routine_scope != NULL &&
      entry_routine_scope == innermost_function_scope) {
    an_expr_node_ptr  call_node = NULL;
    a_variable_ptr    return_variable = NULL;
    check_assertion(master_routine_scope != NULL);
    /* Scan for the "call" to the master routine.  This can take different
       forms, depending on whether ctor/dtors can return "this". */
    if (is_operation_node(expr) &&
        expr->variant.operation.kind == (an_expr_operator_kind)eok_assign &&
        expr->variant.operation.operands->next != NULL &&
        is_variable_node(expr->variant.operation.operands) &&
        is_operation_node(expr->variant.operation.operands->next) &&
        expr->variant.operation.operands->next->variant.operation.kind ==
                                             (an_expr_operator_kind)eok_call) {
      /* Assignment of call result to temporary variable. */
      call_node = expr->variant.operation.operands->next;
      return_variable = node_variable(expr->variant.operation.operands);
    } else if (is_operation_node(expr) &&
               expr->variant.operation.kind ==
                                             (an_expr_operator_kind)eok_call) {
      /* Standalone call expression. */
      call_node = expr;
#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
    } else if (is_operation_node(expr) &&
               expr->variant.operation.kind ==
                                           (an_expr_operator_kind)eok_assign &&
               expr->variant.operation.operands->next != NULL &&
               is_variable_node(expr->variant.operation.operands) &&
               expr->variant.operation.operands->next->kind ==
                        (an_expr_node_kind)enk_result_of_overriding_function) {
      /* Assignment of enk_result_of_overriding_function to temporary. */
      replacing_roof = TRUE;
      return_variable = node_variable(expr->variant.operation.operands);
    } else if (expr->kind ==
                        (an_expr_node_kind)enk_result_of_overriding_function) {
      /* Standalone enk_result_of_overriding_function. */
      replacing_roof = TRUE;
#endif /* ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
    }  /* if */
    if (replacing_roof ||
        (call_node != NULL &&
         is_routine_node(call_node->variant.operation.operands) &&
         node_routine(call_node->variant.operation.operands) ==
                                  master_routine_scope->variant.routine.ptr)) {
      a_scope_ptr      saved_curr_scope = curr_scope;
      /* We've found the "call" to the master routine.  Effectively inline
         the invocation of the master routine by replacing the statement
         that invokes the master routine with a block containing the
         contents of the master routine (along with some initialization
         for the parameters of the master routine if appropriate). */
      replaced = TRUE;
      write_tok_ch('{');
      indent += 2;
      if (replacing_roof) {
        /* Invoking overriding function with the same arguments, no
           assignments are necessary. */
        check_assertion(num_master_params_added == 0);
      } else {
        a_variable_ptr   master_param;
        an_expr_node_ptr arg;
        int              n;
        /* Skip past the "this" parameter. */
        master_param = master_routine_scope->variant.routine.parameters->next;
        /* Skip past the function address and the "this" argument. */
        arg = call_node->variant.operation.operands->next->next;
        /* Generate declarations for the added master routine parameter(s)
           and initialize them to the corresponding argument expressions
           from the call invocation. */
        for (n = 1;
             n <= num_master_params_added;
             n++, master_param = master_param->next, arg = arg->next) {
          check_assertion(master_param != NULL && arg != NULL);
          set_output_position(&master_param->source_corresp.decl_position);
          dump_declaration_using_type(master_param->type,
                                      &master_param->source_corresp);
          write_tok_str(" = ");
          dump_expr_with_parens(arg);
          write_tok_str("; ");
        }  /* for */
      }  /* if */
      /* Expand the master routine in its scope (replacing returns if
         necessary). */
      master_routine_return_variable = return_variable;
      curr_scope = innermost_function_scope = master_routine_scope;
      dump_statement(master_routine_scope->assoc_block);
      curr_scope = saved_curr_scope;
      innermost_function_scope = entry_routine_scope;
      /* Put out a label that is the target of any returns in the master
         routine.  There will only ever be one instance of this label in
         a given entry routine. */
      write_tok_str(end_of_master_routine_label);
      write_tok_str(":;");
      master_routine_return_variable = NULL;
      indent -= 2;
      write_tok_ch('}');
    }  /* if */
  }  /* if */
  return replaced;
}  /* replace_call_to_master_routine */


static void dump_routine_definition(a_routine_ptr rout)
/*
Generate the definition of the indicated routine.  The information preceding
the return type specifier (e.g., storage class) has already been put out
by dump_routine_decl.
*/
{
  a_memory_region_number scope_region_number;
  a_scope_ptr            scope, saved_curr_scope = curr_scope;
  a_routine_ptr          master_routine = NULL;
  a_memory_region_number master_scope_region_number = NO_SCOPE_NUMBER;

  /* Get the top-level scope for the routine definition.  Read it in if
     necessary. */
  scope = get_scope_for_routine_definition(rout, &scope_region_number);
  innermost_function_scope = curr_scope = scope;
  skip_this_parameter = FALSE;
  if (skip_typerefs(rout->type)->variant.routine.extra_info->has_ellipsis) {
#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
    /* Note that ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN is always
       TRUE when IA64_ABI is TRUE. */
    if (rout->overriding_function_for_wrapper != NULL) {
      /* This routine is a wrapper with a variable number of arguments.
         Wrapper functions are used (in the IA-64 ABI) as thunks to adjust
         the "this" pointer or, in the case of an overriding virtual function
         with a covariant return type, to adjust the return type.  Expand the
         overriding function reference (effectively "inlining" it) during the
         code generation for the wrapper. */
      /* We could do this for all wrappers, but we choose to do it only
         when it's necessary, i.e., for routines with variable arguments.
         Doing it in all cases causes code bloat, especially for IA-64 ABI
         destructor thunks. */
      master_routine = rout->overriding_function_for_wrapper;
#if IA64_ABI
    } else if (rout->primary_ctor_or_dtor != NULL) {
      /* This routine is an alternate entry point for a constructor or
         destructor in the IA-64 ABI with an ellipsis.  Expand the body of
         the underlying constructor because otherwise we have no way of
         passing the arguments through.  The body of the alternate entry
         point is simply a call to the underlying routine, but may we have
         to assign values to some parameters. */
      /* Note that we definitely don't want to do this for the deleting
         destructor, since it contains additional deletion code in addition
         to the call to the underlying routine. */
      master_routine = rout->primary_ctor_or_dtor;
#endif /* IA64_ABI */
    } else
#endif /* ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
    /* Do not insert code here. */
    if (rout->special_kind ==
                             (a_special_function_kind)sfk_lambda_entry_point) {
      /* This routine is the alternate entry point for the call operator of
         a no-capture lambda with an ellipsis.  As above, we will need to
         expand the body of the call routine inline. */
      master_routine = rout->variant.lambda_call_operator;
      skip_this_parameter = TRUE;
    }  /* if */
  }  /* if */
  if (master_routine != NULL) {
    /* This is a wrapper routine.  Get the body of the underlying routine. */
    entry_routine_scope = scope;
    master_routine_scope =
                 get_scope_for_routine_definition(master_routine,
                                                  &master_scope_region_number);
    num_master_params_added = 0;
#if IA64_ABI
    if (rout->primary_ctor_or_dtor != NULL) {
      /* See whether any parameters get added. */
      num_master_params_added = (int)(num_parameters(master_routine_scope) -
                                      num_parameters(entry_routine_scope));
    }  /* if */
#endif /* IA64_ABI */
    check_assertion(num_master_params_added == 0 || !skip_this_parameter);
  }  /* if */
  octl.suppress_local_typedefs = FALSE;
  /* Generate the routine name and the parameter declarations. */
  dump_func_definition_type(rout, scope);
  /* Generate the body statement. */
  dump_statement(scope->assoc_block);
  innermost_function_scope = NULL;
  octl.suppress_local_typedefs = TRUE;
  curr_scope = saved_curr_scope;
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  /* Now that we're done with the function, free its IL information if it
     is the top-level function of a memory region. */
  if (rout->is_top_level_in_mem_region
#if !STANDALONE_UTILITY_PROGRAM
      && !skip_il_read
#endif /* !STANDALONE_UTILITY_PROGRAM */
                      ) {
    free_memory_region(scope_region_number);
  }  /* if */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
  if (master_routine != NULL) {
    /* Finished a wrapper routine. */
#if IL_SHOULD_BE_WRITTEN_TO_FILE
    if (master_routine->is_top_level_in_mem_region
#if !STANDALONE_UTILITY_PROGRAM
        && !skip_il_read
#endif /* !STANDALONE_UTILITY_PROGRAM */
                        ) {
      free_memory_region(master_scope_region_number);
    }  /* if */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
    entry_routine_scope = NULL;
    master_routine_scope = NULL;
    master_routine_return_variable = NULL;
    num_master_params_added = 0;
    skip_this_parameter = FALSE;
  }  /* if */
}  /* dump_routine_definition */


#if !USE_INIT_SECTION_IN_GENERATED_C && SUNPRO_C_IS_C_GEN_BE_TARGET

static void dump_sunpro_init_pragma(a_routine_ptr	rout,
				    char		*name)
/*
Put out a "#pragma init(name)", which is used for static initialization
when using the SunPro C compiler.  If rout is non-NULL, its routine name
is put in the pragma, otherwise "name" is used.
*/
{
  end_output_line_if_begun();
  disable_line_wrapping();
  write_str("#pragma init(");
  if (rout != NULL) {
    dump_routine_name(rout);
  } else {
    write_str(name);
  }  /* if */
  write_str(")");
  enable_line_wrapping();
  end_output_line();
}  /* dump_sunpro_init_pragma */

#endif /* !USE_INIT_SECTION_IN_GENERATED_C && SUNPRO_C_IS_C_GEN_BE_TARGET */
#if SUNPRO_C_IS_C_GEN_BE_TARGET

static void dump_sunpro_weak_pragma(a_routine_ptr rout)
/*
Put out a "#pragma weak name", which is used for weak references
when using the SunPro C compiler.
*/
{
  end_output_line_if_begun();
  disable_line_wrapping();
  write_str("#pragma weak ");
  dump_routine_name(rout);
  enable_line_wrapping();
  end_output_line();
}  /* dump_sunpro_weak_pragma */

#endif /* SUNPRO_C_IS_C_GEN_BE_TARGET */

static void dump_msvc_init_pragma(a_routine_ptr rout,
			          char		*name)
/*
Put out a special code sequence which is used for static initialization
when using the Microsoft C compiler.  If rout is non-NULL, its routine name
is put in the pragma, otherwise "name" is used.
*/
{
  end_output_line_if_begun();
  disable_line_wrapping();
  write_str("#pragma section(\".CRT$XCU\",read,write)");
  end_output_line();
  enable_line_wrapping();
  write_str("__declspec(allocate(\".CRT$XCU\"))");
  write_str("static void (__cdecl *__dummy_static_init)(void) =");
  if (rout != NULL) {
    dump_routine_name(rout);
  } else {
    write_str(name);
  }  /* if */
  write_str(";");
  end_output_line();
}  /* dump_msvc_init_pragma */

#if !USE_INIT_SECTION_IN_GENERATED_C

static void dump_gcc_init_sequence(ARG_UNUSED a_routine_ptr rout,
                                   ARG_UNUSED a_const_char  *name,
                                   ARG_UNUSED unsigned long init_priority)
/*
Put out GCC-specific code to invoke the specified routine at initialization
time.  This routine should be invoked before the closing ";" of the declaration
for the routine.  If rout is non-NULL, its routine name and init_priority are
used, otherwise the name and init_priority argument values are used.
*/
{
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
  if (rout != NULL) {
    name = rout->source_corresp.name;
    init_priority = rout->init_priority;
  }  /* if */
  if (init_priority != 0) {
    /* For an initialization routine that contains initializations of
       variables with init_priority N, put out a variable in a
       section named .ctors.M, where M is 65535-N, that points
       to the routine. */
    write_tok_ch(';');
    disable_line_wrapping();
    write_str(" __attribute__((section(\".ctors.");
    write_unsigned_num((a_host_large_unsigned)(65535-init_priority));
    write_str("\"))) void *ctors");
    write_str(name);
    enable_line_wrapping();
    write_str(" = ");
    write_str(name);
  } else
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
  /* Do not insert code here. */
  {
    write_tok_str(" __attribute__((__constructor__))");
  }  /* if */
}  /* dump_gcc_init_sequence */

#endif /* !USE_INIT_SECTION_IN_GENERATED_C */
#if ONE_INSTANTIATION_PER_OBJECT

static a_boolean emit_routine_in_slice(a_routine_ptr rout)
/*
Return TRUE if the routine should be emitted in the current slice.
*/
{
  a_boolean result = TRUE;

  if (rout->instantiation_needed_bit_number != 0) {
    /* This routine is an instantiation and goes out only in its own
       file. */
    if (needed_flag_bit_number != rout->instantiation_needed_bit_number) {
      result = FALSE;
    }  /* if */
  } else {
    /* This routine belongs in the primary output file.  Don't put it out
       if the current output file is for an instantiation. */
    if (needed_flag_bit_number != 1) result = FALSE;
  }  /* if */
  return result;
}  /* emit_routine_in_slice */

#endif /* ONE_INSTANTIATION_PER_OBJECT */

static a_const_char *tls_init_name(void)
/*
Returns the name of the thread_local storage initialization routine to use.
Typically this is __tls_init, but when ONE_INSTANTIATION_PER_OBJECT is
used, each slice has the potential to have its own __tls_init routine and
these are differentiated by adding the needed flag big number (i.e.,
__tls_init__N).  __tls_init is used for the primary output file (when
needed_flag_bit_number == 1).
*/
{
  a_const_char *name = "__tls_init";
  char         *result;

#if ONE_INSTANTIATION_PER_OBJECT
  if (needed_flag_bit_number > 1) {
    Small_string<50> buffer("__", needed_flag_bit_number);

    result = alloc_il_for_c_gen_be(strlen(name) + buffer.length() + 1);
    (void)memcpy(result, name, strlen(name));
    (void)strcpy(&result[strlen(name)], buffer.as_temp_characters());
  } else
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  {
    result = (char *)name;
  }  /* if */
  return (a_const_char*)result;
}  /* tls_init_name */

#if C99_IL_EXTENSIONS_SUPPORTED && LOWER_COMPLEX && GNU_EXTENSIONS_ALLOWED

static a_boolean ttt_has_lowered_complex_type(
                                           a_type_ptr  type_ptr,
                                           a_boolean   *force_end_of_traversal)
/*
This is a service function designed to be called from traverse_type_tree
(whence the ttt_ prefix).  It returns TRUE if type_ptr is a type that
contains a lowered complex type.
*/
{
  a_boolean result = FALSE;

  if (type_ptr->kind == (a_type_kind)tk_typeref) {
    if (type_ptr->variant.typeref.is_lowered_complex_type) {
      result = TRUE;
      *force_end_of_traversal = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* ttt_has_lowered_complex_type */


static a_boolean type_has_lowered_complex_type(a_type_ptr type)
/*
Return TRUE if the indicated type contains a lowered "complex" type.
*/
{
  a_boolean result = FALSE;

  if (traverse_type_tree(type, ttt_has_lowered_complex_type,
                         TTT_RETURN_TYPE | TTT_PARAM_TYPES)) {
    result = TRUE;
  }  /* if */
  return result;
}  /* type_has_lowered_complex_type */

#endif /* C99_IL_EXTENSIONS_SUPPORTED && LOWER_COMPLEX && GNU_EXTENSIONS_... */

static void dump_routine_decl(a_routine_ptr rout,
                              a_boolean     dump_defn)
/*
Dump the information about one routine.  If dump_defn is FALSE, just dump the
interface.  If dump_defn is TRUE, dump the interface and definition, but only
if this routine has a body (dump nothing if it has no body).
*/
{
  a_boolean       has_defn = (rout->function_def_number !=
                                                      NULL_function_def_number
#if MAINTAIN_NEEDED_FLAGS
                              && rout->definition_needed
#endif /* MAINTAIN_NEEDED_FLAGS */
                                                        );
  a_boolean       is_definition;
  a_storage_class storage_class = rout->storage_class;
#if IA64_ABI && ONE_INSTANTIATION_PER_OBJECT
  a_boolean       part_of_current_output_file = TRUE;
#endif /* IA64_ABI && ONE_INSTANTIATION_PER_OBJECT */
  a_boolean       is_marked_weak = FALSE;
  a_boolean       thread_local_init_case = FALSE;
  a_boolean       for_inlining_only;

  if (c99_mode && rout->definition_for_inlining_only &&
      !gcc_version_is(< 50000)) {
    /* This is a C99 "inline definition", so we must suppress the "extern"
       keyword to avoid turning it into an external definition.  (gcc did
       not support C99 inline semantics until version 5.1.0.) */
    for_inlining_only = TRUE;
  } else {
    for_inlining_only = FALSE;
  }  /* if */
#if C_GEN_BE_GENERATES_C23
  if (rout->source_corresp.name != NULL &&
      rout->source_corresp.name[0] == 'b' &&
      strcmp(rout->source_corresp.name + 1, "ool") == 0) {
    /* C23 defines a built-in "bool" type, so any declaration of an entity
       with that name will conflict.  Replace the name with an alternative
       spelling. */
    rout->source_corresp.name = "__EDG_user_declared_bool";
  }  /* if */
#endif /* C_GEN_BE_GENERATES_C23 */
  if (rout->suppress_inline_body && has_defn) {
    /* The body is present only to be used for inlining.  This happens
       in C++ when INSTANTIATE_EXTERN_INLINE is enabled, and in C99
       for "inline definitions".  Don't put out the body. */
    has_defn = FALSE;
    if (gcc_or_clang_is_generated_code_target) {
      /* gcc has a way of indicating a function whose definition is
         provided only for the purpose of inlining -- "extern inline".
         Put out the definition in that case. */
      has_defn = TRUE;
      if (!for_inlining_only) {
        storage_class = (a_storage_class)sc_extern;
      }  /* if */
    }  /* if */
  }  /* if */
#if ONE_INSTANTIATION_PER_OBJECT
  if (has_defn && needed_flag_bit_number != 0 &&
      (rout->storage_class != (a_storage_class)sc_static &&
       rout->storage_class != (a_storage_class)sc_asm)) {
    /* We're generating separate files for each instantiation, so do not
       put instantiation definitions into the primary output file, or
       primary-file routine definitions into the instantiation files.
       (Exceptions are extern inline functions lowered to static and
       certain static and asm routines that are explicitly marked to be put
       into every slice that references them.) */
    if (!emit_routine_in_slice(rout)) has_defn = FALSE;
    if (!has_defn) {
#if IA64_ABI
      part_of_current_output_file = FALSE;
#endif /* IA64_ABI */
      storage_class = (a_storage_class)sc_extern;
    }  /* if */
  }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  if (!has_defn && dump_defn) {
    /* The routine has no body (i.e., no definition), and we're supposed
       to dump it only if it has a definition, so do nothing (except for
       the special case of a __tls_init alias below). */
#if USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES
    if (rout->is_tls_init_alias
#if ONE_INSTANTIATION_PER_OBJECT
        && (needed_flag_bit_number == 0 ||
            emit_routine_in_slice(rout))
#endif /* ONE_INSTANTIATION_PER_OBJECT */
                                                                       ) {
      /* This routine is an "alias" for the thread_local initialization
         for this translation unit (or slice within the translation unit).
         If the back end supports it, create an alias, otherwise emit a routine
         to invoke __tls_init explicitly. */
      if (gcc_or_clang_is_generated_code_target &&
          rout->storage_class == (a_storage_class)sc_unspecified) {
        set_output_position(&rout->source_corresp.decl_position);
        /* Defined in this translation unit; emit an alias indication. */
        disable_line_wrapping();
        write_str("__asm__(\".global ");
        dump_routine_name(rout);
        write_str("\");");
        end_output_line();
        write_str("__asm__(\"");
        dump_routine_name(rout);
        write_str(" = ");
        write_str(tls_init_name());
        write_str("\");");
        enable_line_wrapping();
      } else if (rout->storage_class == (a_storage_class)sc_static ||
                 rout->storage_class == (a_storage_class)sc_unspecified) {
        /* If the back end compiler has no aliasing capability, simply
           define the routine with a body that calls __tls_init. */
        thread_local_init_case = TRUE;
        goto declare_routine;
      }  /* if */
    }  /* if */
#endif /* USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES */
#if SGIC
  } else if (has_name(rout) &&
             strncmp(rout->source_corresp.name, "__builtin_", 10) == 0) {
    /* Routines with names beginning "__builtin_" should not be declared
       or defined. */
#endif /* SGIC */
#if BUILTIN_FUNCTIONS_ENABLED
  } else if (is_gnu_builtin_function(rout) &&
             rout->source_corresp.attributes == NULL
#if C99_IL_EXTENSIONS_SUPPORTED && LOWER_COMPLEX && GNU_EXTENSIONS_ALLOWED
             /* When complex types are lowered, builtin functions using
                complex types are incompatible with the lowered types and
                thus must be declared as ordinary functions if they are
                used. */
             && !(entity_needed_in_generated_code(rout) &&
                  type_has_lowered_complex_type(rout->type))
#endif /* C99_IL_EXTENSIONS_SUPPORTED && LOWER_COMPLEX && GNU_EXTENSIONS_... */
             ) {
    /* GNU-style builtin functions should otherwise not be declared or
       defined. */
#endif /* BUILTIN_FUNCTIONS_ENABLED */
#if ASM_FUNCTION_ALLOWED
  } else if (!dump_defn && storage_class == (a_storage_class)sc_asm) {
    /* Suppress forward declaration of an asm function. */
#endif /* ASM_FUNCTION_ALLOWED */
  } else if (!start_unreferenced_bracket(&rout->source_corresp,
                                         (a_boolean *)NULL)) {
    /* Unreferenced routine. */
  } else {
#if USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES
declare_routine:
#endif /* USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES */
    is_definition = (has_defn && dump_defn);
#if SGIC
    /* The SGI compiler uses a pragma to indicate "inline". */
    if (rout->is_inline && has_name(rout) && has_defn && !is_definition) {
      uint32_t saved_indent = indent;
      end_output_line_if_begun();
      indent = 0;
      disable_line_wrapping();
      write_str("#pragma inline global (");
      dump_routine_name(rout);
      write_str(")");
      enable_line_wrapping();
      end_output_line();
      indent = saved_indent;
    }
#endif /* SGIC */
    /* Dump any pragmas associated with the routine on the definition
       of the routine if it has one, otherwise on the declaration. */
    if (is_definition || !has_defn) {
      dump_associated_pragmas(rout);
      if (is_definition) {
        /* Generate any needed standard C99 pragma. */
        if (rout->fp_contract != stdc_pv_none &&
            rout->fp_contract != curr_default_fp_contract) {
          dump_stdc_pragma(stdc_pk_fp_contract,
                           enum_cast<a_stdc_pragma_value>(rout->fp_contract));
        }  /* if */
        if (rout->fenv_access != stdc_pv_none &&
            rout->fenv_access != curr_default_fenv_access) {
          dump_stdc_pragma(stdc_pk_fenv_access,
                           enum_cast<a_stdc_pragma_value>(rout->fenv_access));
        }  /* if */
        if (rout->cx_limited_range != stdc_pv_none &&
            rout->cx_limited_range != curr_default_cx_limited_range) {
          dump_stdc_pragma(stdc_pk_cx_limited_range,
                           enum_cast<a_stdc_pragma_value>(
                                                      rout->cx_limited_range));
        }  /* if */
#if FIXED_POINT_ALLOWED && !LOWER_FIXED_POINT
        /* Generate any needed fixed-point pragma. */
        if (rout->fx_full_precision != stdc_pv_none &&
            rout->fx_full_precision != curr_default_fx_full_precision) {
          dump_stdc_pragma(stdc_pk_fx_full_precision,
                           enum_cast<a_stdc_pragma_value>(
                                                     rout->fx_full_precision));
        }  /* if */
        if (rout->fx_fract_overflow != stdc_pv_none &&
            rout->fx_fract_overflow != curr_default_fx_fract_overflow) {
          dump_stdc_pragma(stdc_pk_fx_fract_overflow,
                           enum_cast<a_stdc_pragma_value>(
                                                     rout->fx_fract_overflow));
        }  /* if */
        if (rout->fx_accum_overflow != stdc_pv_none &&
            rout->fx_accum_overflow != curr_default_fx_accum_overflow) {
          dump_stdc_pragma(stdc_pk_fx_accum_overflow,
                           enum_cast<a_stdc_pragma_value>(
                                                     rout->fx_accum_overflow));
        }  /* if */
#endif /* FIXED_POINT_ALLOWED && !LOWER_FIXED_POINT */
#if UPC_EXTENSIONS_ALLOWED
        if (rout->upc_access_method !=
                                (a_upc_access_method)upc_access_unspecified &&
            rout->upc_access_method != curr_default_upc_access_method) {
          dump_upc_pragma((a_upc_access_method)rout->upc_access_method);
        }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
      }  /* if */
    }  /* if */
    /* Dump the routine interface. */
#if IA64_ABI
    if (is_definition &&
        skip_typerefs(rout->type)->variant.routine.extra_info->this_class !=
                                                                        NULL &&
        gcc_or_clang_is_generated_code_target) {
      /* On some architectures, gcc does not enforce any alignment
         requirements on the address of functions.  This conflicts with the
         use of the low-order bit of pointers-to-members to indicate
         different representations, so we must explicitly align non-static
         member functions on a 2-byte boundary. */
      write_tok_str("__asm__(\".align 2\");");
      end_output_line();
    }  /* if */
#endif /* IA64_ABI */
    set_output_position(&rout->source_corresp.decl_position);
    /* Determine the proper storage class to display. */
    if (!is_definition && !thread_local_init_case) {
      /* The function is not defined (here), so use "extern", unless this
         is a superseded external declaration.  In that case, because the
         declaration will be changed to a function pointer, use
         "static". */
      if (rout->superseded_external) {
        storage_class = (a_storage_class)sc_static;
      } else if (storage_class == (a_storage_class)sc_unspecified &&
                 !for_inlining_only) {
        storage_class = (a_storage_class)sc_extern;
      }  /* if */
    }  /* if */
    /* Output the storage class. */
    dump_storage_class(storage_class);
#if IA64_ABI
    if (rout->use_comdat
#if ONE_INSTANTIATION_PER_OBJECT
        && part_of_current_output_file
#endif /* ONE_INSTANTIATION_PER_OBJECT */
                                      ) {
      /* A routine in a COMDAT.  Must be a definition. */
      check_assertion_str(rout->storage_class ==
                                               (a_storage_class)sc_unspecified,
                          "dump_routine_decl: rout without defn in comdat");
      if (gcc_or_clang_is_generated_code_target
#if GNU_EXTENSIONS_ALLOWED && !LOWER_IFUNC
          && !rout->is_ifunc    /* gcc doesn't allow ifunc to be weak, so
                                   suppress the weak attribute (though this
                                   may result in multiple-definition errors
                                   in some cases). */
#endif /* GNU_EXTENSIONS_ALLOWED && !LOWER_IFUNC */
                            ) {
        /* GCC does not support COMDAT, but it does support weak, which
           provides a sufficient approximation. */
#if GNU_EXTENSIONS_ALLOWED
        if (rout->always_inline && gcc_is_generated_code_target) {
          /* gcc gives an error on calls to a weak function with the
             always_inline attribute (because the definition could be
             replaced at link time).  Without the weak attribute each
             translation unit that defines the routine contains a conflicting
             external definition of it, so suppress the always_inline
             attribute instead (Clang accepts both). */
          rout->always_inline = FALSE;
        }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
        write_tok_str(" __attribute__((__weak__))");
        is_marked_weak = TRUE;
      }  /* if */
      write_space();
      start_comment();
      write_tok_str(" COMDAT group: ");
      write_tok_str(rout->source_corresp.name);
      write_space();
      end_comment();
      write_space();
    }  /* if */
#endif /* IA64_ABI */
#if SUN_EXTENSIONS_ALLOWED && C_GEN_BE_GENERATES_ANSI_C
    if (sun_is_generated_code_target) {
      /* Sun-specific "link scope specifiers" (__global, __symbol, or
         __hidden). */
      form_sun_link_scope_specifiers(rout->decl_modifiers, &octl);
    }  /* if */
#endif /* SUN_EXTENSIONS_ALLOWED && C_GEN_BE_GENERATES_ANSI_C */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_dialect_is_generated_code_target) {
      /* Microsoft-specific keywords. */
      a_decl_modifier_set decl_modifiers = rout->decl_modifiers;
      if (is_definition && rout->is_inline && msvc_is_generated_code_target) {
        /* If the routine was "inline", force the keyword "__inline". */
        decl_modifiers |= DM_MICROSOFT_INLINE;
      }  /* if */
      dump_microsoft_decl_modifiers(decl_modifiers);
      if (rout->never_inline) write_tok_str("__declspec(noinline) ");
      if (rout->is_naked && is_definition) write_tok_str("__declspec(naked) ");
      if (routine_does_not_return(rout)) {
        write_tok_str("__declspec(noreturn) ");
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (gcc_or_clang_is_generated_code_target && rout->is_inline) {
      /* gcc will be used to compile this generated code, so we know how to
         indicate an inline function. */
      /* gcc ignores __inline__ on functions with ellipses, so don't
         mark such functions as inline.  Recent versions of gcc issue an
         error for a function marked both inline and weak, so don't mark a
         weak function as inline, either. */
      if (!f_skip_typerefs(rout->type)->variant.routine.extra_info->
                                                                has_ellipsis &&
          !is_marked_weak) {
        write_tok_str("__inline__ ");
      }  /* if */
    }  /* if */
#if GNU_EXTENSIONS_ALLOWED && !LOWER_IFUNC
    if (rout->is_ifunc) {
      /* A routine with an un-lowered GNU ifunc attribute; emit the appropriate
         ifunc attribute in the generated C code.  This is only supported by
         GNU compilers (and possibly only on Linux). */
      check_assertion(gcc_is_generated_code_target &&
                      gnu_routine_supp(rout)->aliased_routine != NULL);
      write_tok_str("__attribute__((ifunc(\"");
      dump_routine_name(gnu_routine_supp(rout)->aliased_routine);
      write_tok_str("\"))) ");
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED && !LOWER_IFUNC */
    if (!is_definition) {
      /* A declaration of the routine. */
#if C_GEN_BE_GENERATES_C23 && \
    (GCC_IS_GENERATED_CODE_TARGET || CLANG_IS_GENERATED_CODE_TARGET)
      a_routine_type_supplement_ptr rtsp =
                                        rout->type->variant.routine.extra_info;
      if (!rtsp->prototyped && rtsp->assoc_routine == NULL) {
        /* This function may have been implicitly declared, resulting in an
           unprototyped function type returning int.  If that declaration
           is for a function for which the target compiler provides a
           built-in equivalent, we must put out a compatible declaration
           instead of the one in the IL. */
        if (rout->source_corresp.name[0] == 'a' &&
            strcmp(rout->source_corresp.name + 1, "bort") == 0) {
          write_tok_str("void abort(void);");
          goto done;
        } else if (rout->source_corresp.name[0] == 'p' &&
                   strcmp(rout->source_corresp.name + 1, "rintf") == 0) {
          write_tok_str("int printf(const char *, ...);");
          goto done;
        }  /* if */
        if (has_attr(ak_ifunc, rout->source_corresp.attributes)) {
          /* This function was declared unprototyped, i.e., with the
             parameter list "()".  In general, unprototyped functions that
             are not defined will be declared with an ellipsis in case
             arguments are passed in a call to the function.  However, that
             declaration is not compatible with the expected type of an
             indirect function.  Ensure that it is put out with a "(void)"
             parameter list. */
          rtsp->prototyped = TRUE;
        }  /* if */
      }  /* if */
#endif /* C_GEN_BE_GENERATES_C23 && (...) */
      dump_general_declaration_using_type(rout->type, &rout->source_corresp,
                                          NO_VARIABLE, rout, NO_FIELD, NO_TEMP,
                                          NO_NAME, TQ_NONE,
                                          /*suppress_const=*/FALSE,
                                          NO_COUNTER);
#if GNU_EXTENSIONS_ALLOWED
      if (has_gnu_routine_supp(rout)) {
        form_asm_name(gnu_routine_supp(rout)->asm_name, &octl);
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if !SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
#if !USE_INIT_SECTION_IN_GENERATED_C
      if (gcc_or_clang_is_generated_code_target &&
          routine_is_init_routine(rout)) {
        /* gcc has a special way of indicating that a routine should be
           called at program startup.  If this is an initialization routine,
           arrange for it to be called. */
        /* Note that this gcc case is done before the other cases below
           because it gets put out before the closing ";" of the declaration,
           and the other cases emit pragmas. */
        dump_gcc_init_sequence(rout, NULL, (unsigned long)0);
      }  /* if */
#else /* USE_INIT_SECTION_IN_GENERATED_C */
#if !GCC_IS_GENERATED_CODE_TARGET && GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED && \
    !defined(_lint)
 #error -- GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED is only supported with \
           output to gcc
#endif /* !GCC_IS_GENERATED_CODE_TARGET && GNU_INIT_PRIORITY_ATTRIBUTE_... */
#endif /* !USE_INIT_SECTION_IN_GENERATED_C */
#endif /* !SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */
      if (rout->superseded_external) {
        /* dump_general_declaration_using_type converted this routine
           declaration to a declaration of a function pointer with a
           temporary name.  We need to initialize it to the address of the
           "official" routine (which has the same name as the superseded
           routine), cast to the appropriate type.  This works around a
           problem with versions of gcc beginning with 3.4, which do not
           allow calling a function directly through a cast to a different
           function pointer type. */
        write_tok_str(" = ");
        dump_cast_to_pointer_to(rout->type);
        dump_name(&rout->source_corresp);
      }  /* if */
      if (thread_local_init_case) {
        /* Generate a definition that just calls __tls_init. */
        write_tok_str(" { ");
        write_tok_str(tls_init_name());
        write_tok_str("(); }");
      } else {
        write_tok_ch(';');
      }  /* if */
#if !SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
      if (routine_is_init_routine(rout)) {
#if !USE_INIT_SECTION_IN_GENERATED_C && SUNPRO_C_IS_C_GEN_BE_TARGET
        /* The SunPro C compiler has a pragma that specifies that a routine
           should be called at program startup.  If this is an initialization
           routine, arrange for it to be called. */
        dump_sunpro_init_pragma(rout, (char*)NULL);
      } else {
#endif /* !USE_INIT_SECTION_IN_GENERATED_C && SUNPRO_C_IS_C_GEN_BE_TARGET */
        if (msvc_is_generated_code_target &&
            msvc_target_version_number >= 1300) {
          /* Generate a special code sequence that the Microsoft compiler uses
             to specify that a routine should be called at program startup.
             This pragma is only supported by MSVC version 7.0 and beyond. */
          dump_msvc_init_pragma(rout, (char*)NULL);
        }  /* if */
      }  /* if */
#endif /* !SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */
#if GNU_EXTENSIONS_ALLOWED && SUNPRO_C_IS_C_GEN_BE_TARGET
      if (rout->is_weak) {
        /* Indicate that the routine is a weak reference by emitting
           a "#pragma weak". */
        dump_sunpro_weak_pragma(rout);
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED && SUNPRO_C_IS_C_GEN_BE_TARGET */
    } else {
#if ASM_FUNCTION_ALLOWED
      /* If appropriate, set a flag to assure special processing for asm
         function definitions. */
      if (storage_class == (a_storage_class)sc_asm) {
        /* coverity[dead_error_line] */  /* Coverity bug. */
        within_asm_function_definition = TRUE;
      }  /* if */
#endif /* ASM_FUNCTION_ALLOWED */
      /* The definition of the routine. */
      dump_routine_definition(rout);
#if ASM_FUNCTION_ALLOWED
      within_asm_function_definition = FALSE;
#endif /* ASM_FUNCTION_ALLOWED */
    }  /* if */
    end_unreferenced_bracket(&rout->source_corresp);
  }  /* if */
#if C_GEN_BE_GENERATES_C23 && \
    (GCC_IS_GENERATED_CODE_TARGET || CLANG_IS_GENERATED_CODE_TARGET)
done:;
#endif /* C_GEN_BE_GENERATES_C23 && (...) */
}  /* dump_routine_decl */


static void dump_scope_routines(a_scope_ptr scope,
                                a_boolean   dump_defn)
/*
Dump the information about all of the routines at a particular scope.
If dump_defn == FALSE, dump interfaces for all routines on the list.
If dump_defn == TRUE, dump interfaces and definitions for just those routines
that have bodies.
*/
{
  a_routine_ptr routine;
  a_boolean     superseded_external_seen = FALSE;

  for (routine = scope->routines; routine != NULL; routine = routine->next) {
    if (ignore_routine_in_back_end(routine)) continue;
    check_membership_info(routine, scope);
    if (routine->superseded_external) {
      /* Do not put out declarations of superseded externals on this pass.
         We need to wait until all the other routines have been declared
         so that the superseded external function pointer can be initialized
         to point to the "official" routine. */
      superseded_external_seen = TRUE;
#if GNU_EXTENSIONS_ALLOWED
    } else if (has_gnu_routine_supp(routine) &&
               gnu_routine_supp(routine)->inline_partner != NULL &&
               !routine->definition_for_inlining_only) {
      /* In GNU modes a routine can have both a definition for inlining only
         and a declaration or definition for out-of-line calls.  However, the
         latter cannot precede the definition of the former.  So we don't
         dump anything if dump_defn is FALSE, and we dump the declaration or
         the definition when dump_defn is TRUE (since presumably the partner
         definition will already have been dumped at that time). */
      if (dump_defn) {
        dump_routine_decl(routine, routine->function_def_number !=
                                                     NULL_function_def_number);
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    } else {
      dump_routine_decl(routine, dump_defn);
    }  /* if */
  }  /* for */
  if (superseded_external_seen && !dump_defn) {
    /* Make another pass over the list of routines to declare the function
       pointers for superseded externals. */
    for (routine = scope->routines; routine != NULL; routine = routine->next) {
      if (routine->superseded_external) {
        dump_routine_decl(routine, dump_defn);
      }  /* if */
    }  /* for */
  }  /* if */
}  /* dump_scope_routines */


static void dump_source_file_correspondence_info(a_source_file_ptr source_file)
/*
Dump all source files at this level.
*/
{
  for (; source_file != NULL; source_file = source_file->next) {
    do_indentation();
    (void)fprintf(f_C_output,
                  "%s (from line number %lu, sequence numbers %lu-%lu%s)\n",
                  format_source_file_name(source_file,
                                          /*use_name_as_written=*/FALSE,
                                          /*quote_file_name=*/FALSE),
		  (unsigned long)source_file->first_line_number,
                  (unsigned long)source_file->first_seq_number,
		  (unsigned long)source_file->last_seq_number,
                  source_file->top_level_file_from_pch ?
                                             ", top level file from PCH" : "");
    if (source_file->first_child_file != NULL) {
      /* This file included others; dump them out indented in from this one. */
      indent += 2;
      dump_source_file_correspondence_info(source_file->first_child_file);
      indent -= 2;
    }  /* if */
  }  /* for */
}  /* dump_source_file_correspondence_info */


#if C_GEN_BE_GENERATES_ANSI_C

static void dump_size_t_type(void)
/*
Write out the size_t type of the target architecture.  Don't do allocation,
because that is not allowed in a back end.
*/
{
  a_type targ_size_type;

  /* Can't call clear_type in a standalone program.  We only need a correctly
     sized integer type though, so zero out the type object, set the type kind,
     and set the integer kind. */
  memzero((char *)&targ_size_type, sizeof(targ_size_type));
  targ_size_type.kind = (a_type_kind)tk_integer;
  targ_size_type.variant.integer.int_kind = targ_size_t_int_kind;

  /* Write out the type. */
  form_type(&targ_size_type, &octl);
}  /* dump_size_t_type */

#endif /* C_GEN_BE_GENERATES_ANSI_C */

static void dump_header_code(void)
/*
Write a header at the beginning of the generated file, containing any
definitions needed to support the generated code.
*/
{
  a_const_char *p;

  /* Put out a declaration of a variable that identifies the version number.
     This also ensures that the generated file has at least one declaration
     when generating ANSI C. */
  (void)fprintf(f_C_output, "extern int __EDGCPFE__");
  for (p = il_header.compiler_version; *p != '\0'; p++) {
    char ch = *p;
    /* Replace non-alphanumeric characters in the version number with
       an underscore. */
    if (!isalnum((unsigned char)ch)) ch = '_';
    (void)fputc(ch, f_C_output);
  }  /* for */
  (void)fprintf(f_C_output, ";\n");
#if C_GEN_BE_GENERATES_ANSI_C
#if __BSD__
  /* Get bcopy declared.
     void bcopy(const void *,void *,size_t); */
  (void)fprintf(f_C_output,
                "void bcopy(const void *,void *,");
  dump_size_t_type();
  (void)fprintf(f_C_output, ");\n");
  /* Get bzero declared.
     void bzero(void *,size_t); */
  (void)fprintf(f_C_output, "void bzero(void *,");
  dump_size_t_type();
  (void)fprintf(f_C_output, ");\n");
#else  /* !__BSD__ */
  /* Get memcpy declared.
     void *memcpy(void *,const void *,size_t); */
  (void)fprintf(f_C_output, "void *memcpy(void *,const void *,");
  dump_size_t_type();
  (void)fprintf(f_C_output, ");\n");
  /* Get memset declared.
     void *memset(void *,int,size_t); */
  (void)fprintf(f_C_output, "void *memset(void *,int,");
  dump_size_t_type();
  (void)fprintf(f_C_output, ");\n");
#endif /* __BSD__ */
#else /* !C_GEN_BE_GENERATES_ANSI_C */
  /* Routine needed to adjust the signedness of bit field accesses
     (pcc doesn't support signed bit fields). */
  (void)fprintf(f_C_output, "static int __sexten(i,n) int i,n;\n");
  (void)fprintf(f_C_output,
     "{int mask=(1<<(n-1))-1; if(i<0||i>mask)i=(i&mask)|~mask; return(i);}\n");
#endif /* C_GEN_BE_GENERATES_ANSI_C */
}  /* dump_header_code */

#if !C_GEN_BE_GENERATES_ANSI_C

static void dump_file_scope_initialization_routine(void)
/*
Generate the routine called to do file-scope dynamic initializations.
This handles things -- like initialization of unions -- that can't
be written in K&R C.  This is not the initialization routine generated
by IL lowering.
*/
{
  if (f_file_scope_inits != NULL) {
    /* Generate the name of the routine. */
    char *name = alloc_il_for_c_gen_be(
                         (sizeof_t)(sizeof(C_GEN_BE_INIT_ROUTINE_NAME_PREFIX) +
                                    strlen(module_init_id)));
    (void)strcpy(name, C_GEN_BE_INIT_ROUTINE_NAME_PREFIX);
    (void)strcat(name, module_init_id);
    /* Generate the declaration of the routine. */
    end_output_line_if_begun();
    /* An implicit "int" return type is used because the routine is
       called before it is declared. */
    write_tok_str(name);
    write_tok_str("()");
#if !USE_INIT_SECTION_IN_GENERATED_C
    /* gcc has a special way of indicating that a routine should be
       called at program startup. */
    if (gcc_or_clang_is_generated_code_target &&
        !file_scope_init_routine_called) {
      write_tok_str(" __attribute__((__constructor__))");
      file_scope_init_routine_called = TRUE;
    }  /* if */
#endif /* !USE_INIT_SECTION_IN_GENERATED_C */
    write_tok_str(" {");
#if USE_INIT_SECTION_IN_GENERATED_C
    if (!file_scope_init_routine_called) {
      /* The routine can be called from a .init section.  Note that this
         processing is done inside the routine body. */
      generate_init_section_call(name);
      file_scope_init_routine_called = TRUE;
    }  /* if */
#endif /* USE_INIT_SECTION_IN_GENERATED_C */
    end_output_line();
    /* Generate the body of the routine, by copying the file of
       previously-generated initializations. */
    copy_and_delete_file(&f_file_scope_inits);
    write_tok_ch('}');
#if SUNPRO_C_IS_C_GEN_BE_TARGET
    /* SunPro C has a special way of indicating that a routine should be
       called at program startup. */
    if (!file_scope_init_routine_called) {
      dump_sunpro_init_pragma((a_routine_ptr)NULL, name);
      file_scope_init_routine_called = TRUE;
    }  /* if */
#endif /* SUNPRO_C_IS_C_GEN_BE_TARGET */
    if (!file_scope_init_routine_called && msvc_is_generated_code_target &&
        msvc_target_version_number >= 1300) {
      /* Generate a special code sequence that the Microsoft compiler uses
         to specify that a routine should be called at program startup.
         This pragma is only supported by MSVC version 7.0 and beyond. */
      dump_msvc_init_pragma((a_routine_ptr)NULL, name);
      file_scope_init_routine_called = TRUE;
    }  /* if */
#if !USE_INIT_SECTION_IN_GENERATED_C
    if (!gcc_or_clang_is_generated_code_target &&
        !file_scope_init_routine_called) {
      /* No place (such as "main") was found to call the file-scope
         initialization routine generated by the C-generating back end.
         Find some way to get it called at startup. */
      /* In C++, generate a __link variable that will get it called.
         In C, a "-i" command-line option will be needed. */
      if (il_header.source_language == sl_Cplusplus) {
        /* C++ -- Generate an __sti__ routine and the __link variable expected
           by the "patch" program.  The __sti__ routine name is needed
           for the "munch" program. */
        /* Start the __sti__ initialization routine. */
        end_output_line_if_begun();
        write_str("void __sti__");
        write_str(module_id);
        write_tok_str("() {");
        /* Call the c_gen_be-generated initialization routine. */
        write_tok_str(name);
        write_tok_str("();}");
        end_output_line();
        write_tok_str("static struct __linkl {");
        write_tok_str(
                     "struct __linkl *next; void (*ctor)(); void (*dtor)();}");
        write_tok_str("__link = {0, ");
        write_str("__sti__");
        write_str(module_id);
        write_tok_str(", 0};");
      } else {
        /* The file-scope-init routine was not called from anywhere in
           this module.  It must be called from the main program, by using
           the appropriate option. */
        (void)fprintf(f_error,
"This file contains file-scope initializations that involve executable code.\n"
                     );
        (void)fprintf(f_error,
"For it to execute correctly, include \"%s\" in the list of modules\n",
                      module_init_id);
        (void)fprintf(f_error,
"in the \"--module_init\" option during compilation of the associated main\n");
        (void)fprintf(f_error,
"program.\n");
      }  /* if */
    }  /* if */
#endif /* !USE_INIT_SECTION_IN_GENERATED_C */
  }  /* if */
}  /* dump_file_scope_initialization_routine */

#endif /* !C_GEN_BE_GENERATES_ANSI_C */
#if SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS

static void dump_coalesced_file_scope_initialization_routine(void)
/*
This routine determines which file-scope dynamic initializations (as created
during lowering) need to be executed and creates zero or more routines to
invoke those routines.  In this configuration, each file-scope dynamic
initialization is separate and it is incumbent on a back end to invoke them
in the proper order.  Often, this routine will be called only once per
translation unit and will create a single routine to invoke all file-scope
dynamic initializations (if any).  When using one-instantiation-per-object
mode, this routine is invoked once per slice and generates initializations that
are needed only in that slice.  When using GNU init_priority, an invocation of
this routine may result in the creation of multiple routines, one for each
distinct GNU init_priority (possibly within each slice).  The generated routine
is then invoked at initialization by various means (dependent on the back end
C compiler).
*/
{
  a_routine_list_entry_ptr rlep;
  a_boolean                header_written = FALSE, skip;
  char                     *name;
  sizeof_t                 alloc_length;
  unsigned long            init_priority = 0;
#if ONE_INSTANTIATION_PER_OBJECT
  Small_string<50>         buffer;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
  a_gnu_init_priority      last_priority = 0;
  Small_string<50>         buffer2;
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */

  for (rlep = il_header.file_scope_dynamic_init_routines;
       rlep != NULL;
       rlep = rlep->next) {
    skip = FALSE;
    alloc_length = 0;
#if ONE_INSTANTIATION_PER_OBJECT
    if (needed_flag_bit_number != 0) {
      if (emit_routine_in_slice(rlep->routine)) {
        /* Add a suffix to distinguish initialization routines for
           specific instantiations. */
        buffer.reset_to("__", needed_flag_bit_number);
        alloc_length += buffer.length();
      } else {
        skip = TRUE;
      }  /* if */
    }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
    if (!skip) {
      if (rlep->routine->init_priority != last_priority) {
        /* Force a new routine to be emitted. */
        if (header_written) {
          /* Terminate the previous routine if we'd started one. */
          write_tok_ch('}');
          end_output_line();
        }  /* if */
        header_written = FALSE;
        if (rlep->routine->init_priority != 0) {
          /* Add a priority indicator to the routine name. */
          buffer2.reset_to("__prio", rlep->routine->init_priority);
          alloc_length += buffer2.length();
        }  /* if */
        last_priority = rlep->routine->init_priority;
        init_priority = rlep->routine->init_priority;
      }  /* if */
    }  /* if */
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
    if (!skip) {
      if (!header_written) {
        header_written = TRUE;
        /* Generate the name of the routine. */
        alloc_length += sizeof(C_GEN_BE_COALESCE_INIT_ROUTINE_NAME_PREFIX);
        alloc_length += strlen(module_init_id);
        name = alloc_il_for_c_gen_be(alloc_length);
        (void)strcpy(name, C_GEN_BE_COALESCE_INIT_ROUTINE_NAME_PREFIX);
        (void)strcat(name, module_init_id);
#if ONE_INSTANTIATION_PER_OBJECT
        if (needed_flag_bit_number != 0) {
          (void)strcat(name, buffer.as_temp_characters());
        }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
        if (rlep->routine->init_priority != 0) {
          (void)strcat(name, buffer2.as_temp_characters());
        }  /* if */
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
        /* Generate the declaration of the routine. */
        end_output_line_if_begun();
#if USE_INIT_SECTION_IN_GENERATED_C
        /* Generate an .init section call that gets the initialization routine
           invoked at program startup.  Note that this magic is generated
           inside the body of the initialization routine. */
        generate_init_section_call(name);
#else /* !USE_INIT_SECTION_IN_GENERATED_C */
        if (gcc_or_clang_is_generated_code_target) {
          /* gcc has a special way of indicating that a routine should be
             called at program startup.  Generate a declaration with the
             proper attributes. */
          write_tok_str("void ");
          write_tok_str(name);
          write_tok_str("()");
          dump_gcc_init_sequence((a_routine_ptr)NULL, name, init_priority);
          write_tok_ch(';');
        }  /* if */
#endif /* USE_INIT_SECTION_IN_GENERATED_C */
        write_tok_str("void ");
        write_tok_str(name);
        write_tok_str("()");
        end_output_line();
        write_tok_str("{");
        end_output_line();
      }  /* if */
      /* Emit code to call the routine. */
      write_tok_str(rlep->routine->source_corresp.name);
      write_tok_str("();");
      end_output_line();
    }  /* if */
  }  /* for */
  if (header_written) {
    write_tok_ch('}');
    end_output_line();
#if !USE_INIT_SECTION_IN_GENERATED_C
#if SUNPRO_C_IS_C_GEN_BE_TARGET
    /* SunPro C has a special way of indicating that a routine should be
       called at program startup. */
    dump_sunpro_init_pragma((a_routine_ptr)NULL, name);
#endif /* SUNPRO_C_IS_C_GEN_BE_TARGET */
    if (msvc_is_generated_code_target && msvc_target_version_number >= 1300) {
      /* Generate a special code sequence that the Microsoft compiler uses
         to specify that a routine should be called at program startup.
         This pragma is only supported by MSVC version 7.0 and beyond. */
      dump_msvc_init_pragma((a_routine_ptr)NULL, name);
    }  /* if */
#endif /* !USE_INIT_SECTION_IN_GENERATED_C */
  }  /* if */
}  /* dump_coalesced_file_scope_initialization_routine */

#endif /* SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */

static void generate_C_output_file(a_const_char *C_output_file_name)
/*
Generate a C output file (with the given name) from the intermediate language.
If C_output_file_name is NULL, use stdout for the output.
*/
{
  a_scope_ptr       scope;
  a_source_file_ptr prim_source_file;

  if (C_output_file_name == NULL) {
    /* For a NULL name, use stdout. */
    f_C_output = stdout;
  } else {
    f_C_output = open_output_file_with_error_handling(
                                     C_output_file_name, /*binary_file=*/FALSE,
                                     /*update_mode=*/FALSE, OFF_NO_OPTIONS,
                                     ec_generated_c);
  }  /* if */
  /* Remember the primary output file. */
  f_primary = f_C_output;
  /* Print an identifying heading in the output file. */
  (void)fprintf(f_C_output,
  "/* Translated by the Edison Design Group C++/C front end (version %s) */\n",
                il_header.compiler_version);
  (void)fprintf(f_C_output, "/* %.24s */\n", il_header.time_of_compilation);
  if (annotate) {
    /* Dump the names of the include files. */
    (void)fprintf(f_C_output, "/* Source file information:\n");
    dump_source_file_correspondence_info(il_header.primary_source_file);
    (void)fprintf(f_C_output, "*/\n");
  }  /* if */
#if ONE_INSTANTIATION_PER_OBJECT
  if (needed_flag_bit_number != 0) {
    (void)fprintf(f_C_output, "/* Instantiation number = %lu */\n",
                  needed_flag_bit_number);
  }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  /* Other initialization code. */
  dump_header_code();

  /* Start with a #line directive that identifies the primary file.  If the
     source file contains #line directives, start with the file indicated
     therein as the primary file. */
  prim_source_file = eff_primary_source_file();
  last_known_good_line = prim_source_file->first_line_number;
  last_known_good_file = prim_source_file;
  write_line_directive(last_known_good_line, last_known_good_file);

  /* Dump all of the declarative information at the top-most (file) level. */
  curr_scope = scope = il_header.primary_scope;
  dump_scope_pragmas(scope);
  dump_scope_constants(scope);
  dump_scope_types(scope);
  dump_scope_routines(scope, /*dump_defn=*/FALSE);
  /* Dump variables without initializers, and tentative declarations
     for those with initializers, then the initialized variables again
     with initializers.  This is to avoid forward-reference problems. */
  dump_scope_variables(scope,
                       /*interleave_asm_decls=*/TRUE,
                       /*dump_vars_without_initializers=*/TRUE,
                       /*dump_initializers=*/FALSE);
  dump_scope_variables(scope,
                       /*interleave_asm_decls=*/FALSE,
                       /*dump_vars_without_initializers=*/FALSE,
                       /*dump_initializers=*/TRUE);
  dump_scope_routines(scope, /*dump_defn=*/TRUE);

#if !C_GEN_BE_GENERATES_ANSI_C
  /* Generate the routine called to do file-scope dynamic initializations.
     This handles things -- like initialization of unions -- that can't
     be written in K&R C. */
  dump_file_scope_initialization_routine();
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  check_assertion_str(f_rout_dynamic_inits == NULL,
                      "Routine assignment inits not dumped out");
#if SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
  /* In this mode, each file-scope dynamic initialization is separate so
     we need a coalesced routine (or set of routines in some modes) to
     invoke those routines. */
  dump_coalesced_file_scope_initialization_routine();
#endif /* SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */

  /* Finish the last line, if there is one. */
  end_output_line_if_begun();
  close_output_file_with_error_handling(&f_C_output, ec_generated_c);
  f_primary = NULL;
}  /* generate_C_output_file */


static void c_gen_be_file_init(void)
/*
Initialize for the C-generating back end.  These are initializations that
must be redone for each generated C file.
*/
{
  il_to_str_back_end_file_init();
  line_wrapping_disabled = 0;
  f_C_output = NULL;
  /* Set the position for errors to "unknown". */
  set_position_to(error_position, 0, SP_COL_UNKNOWN);
  /* Output position is unknown. */
  curr_output_file = NULL;
  curr_output_line = 0;
  curr_output_column = 0;  /* Special value meaning there is no output line. */
  curr_output_pos_known = FALSE;
  indent = 0;
  last_known_good_line = 0;
  last_known_good_file = NULL;
  in_comment = FALSE;
#if DEBUG
  annotate = db_active;
#else /* !DEBUG */
  annotate = FALSE;
#endif /* DEBUG */
#if !C_GEN_BE_GENERATES_ANSI_C
  file_scope_init_routine_called = FALSE;
  f_file_scope_inits = NULL;
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  f_rout_dynamic_inits = NULL;
  output_initializer_code_directly = FALSE;
  innermost_function_scope = NULL;
  curr_scope = NULL;
  wide_string_constants_to_unbind_at_end_of_scope =
                                                  &wide_string_constant_marker;
  entry_routine_scope = NULL;
  master_routine_scope = NULL;
  master_routine_return_variable = NULL;
  num_master_params_added = 0;
  skip_this_parameter = FALSE;
#if ASM_FUNCTION_ALLOWED
  within_asm_function_definition = FALSE;
#endif /* ASM_FUNCTION_ALLOWED */
  if (pending_typedefs != NULL) {
    /* There were pending typedefs left over from the previous file (the
       struct on which they depended was never defined).  Move them to
       the available list. */
    last_pending_typedef->next = avail_pending_typedefs;
    avail_pending_typedefs = pending_typedefs;
    pending_typedefs = NULL;
    last_pending_typedef = NULL;
  }  /* if */
}  /* c_gen_be_file_init */

#if ONE_INSTANTIATION_PER_OBJECT

static void generate_one_instantiation_C_output_file(
                                     a_source_correspondence *scp,
                                     unsigned long           needed_bit_number,
                                     ARG_UNUSED char         *name_char_pos)
/*
Generate the C output file for the instantiation whose associated
routine or variable has the given source correspondence field and
"needed" flag bit number.  If name_char_pos is non-NULL (IA-64 ABI
only), it points to the character in a constructor or destructor
name that must be changed to produce the canonical form of the
name.
*/
{
  a_const_char *C_output_file_name;
#if IA64_ABI
  char orig_char = ' ';
#endif /* IA64_ABI */

  if (C_output_file_name_buffer == NULL) {
    /* On the first call, allocate a buffer used to construct the file name.
       This buffer will be resized as needed. */
    C_output_file_name_buffer = alloc_text_buffer(256);
  }  /* if */
#if IA64_ABI
  if (name_char_pos != NULL) {
    /* Change the constructor or destructor name to the canonical
       "C1" or "D1" form. */
    orig_char = *name_char_pos;
    check_assertion(orig_char == '1' || orig_char == '2' ||
                    orig_char == '0' || orig_char == '9');
    *name_char_pos = '1';
  }  /* if */
#endif /* IA64_ABI */
  /* Generate a file name based on the mangled name of the entity. */
  C_output_file_name = generate_instantiation_output_file_name(scp->name);
#if IA64_ABI
  if (name_char_pos != NULL) *name_char_pos = orig_char;
#endif /* IA64_ABI */
  /* Add the right suffix for a generated C file. */
  C_output_file_name = derived_name(C_output_file_name, GEN_C_FILE_SUFFIX);
  /* Add the directory name specified. */
  (void)combine_dir_and_file_name(il_header.instantiation_dir_name,
                                  C_output_file_name,
                                  C_output_file_name_buffer);
  needed_flag_bit_number = needed_bit_number;
  /* Do initialization. */
  c_gen_be_file_init();
  /* Generate the C output file. */
  generate_C_output_file(C_output_file_name_buffer->buffer);
  needed_flag_bit_number = 0;
}  /* generate_one_instantiation_C_output_file */

#if IA64_ABI

static a_boolean routine_slice_appears_earlier_on_list(a_routine_ptr rout)
/*
Return TRUE if a routine with the same one-instantiation-per-object slice
number as the indicated routine appears earlier on the file scope routines
list than rout does.
*/
{
  a_boolean     result = FALSE;
  a_routine_ptr orout;

  for (orout = il_header.primary_scope->routines;
       orout != NULL && orout != rout;
       orout = orout->next) {
    if (orout->instantiation_needed_bit_number ==
         rout->instantiation_needed_bit_number) {
      result = TRUE;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* routine_slice_appears_earlier_on_list */

#endif /* IA64_ABI */

static void generate_instantiation_C_output_files(void)
/*
We are putting each instantiation into its own C output file.  Generate
the C output files for all instantiations.
*/
{
  a_routine_ptr  rout;
  a_variable_ptr var;

  /* Look through the list of routines to find all instantiated functions. */
  for (rout = il_header.primary_scope->routines;
       rout != NULL;
       rout = rout->next) {
    if (rout->instantiation_needed_bit_number != 0) {
      /* Ignore generated startup initialization routines, thread_local
         initialization routines and routines whose bodies are present only for
         inlining purposes. */
      if (!routine_is_init_routine(rout) &&
          !rout->is_tls_init_routine &&
#if USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES
          !rout->is_tls_init_alias &&
#endif /* USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES */
          !rout->suppress_inline_body) {
        a_boolean generate_routine = TRUE;
        char      *char_pos = NULL;
#if IA64_ABI
        /* For constructors and destructors, the primary entry point and
           all the alternate entry points are in the same slice.
           However, they all get removed individually if unneeded, so
           any one of the routines could be on the list without the
           others, and we want to generate the slice only once.
           Check to see if another routine with the same slice
           number has already been processed because it's earlier on
           the list. */
        if (rout->special_kind == (a_special_function_kind)sfk_constructor ||
            rout->special_kind == (a_special_function_kind)sfk_destructor) {
          if (routine_slice_appears_earlier_on_list(rout)) {
            generate_routine = FALSE;
          } else {
            /* Determine the address of the character of the name to
               be changed to get the canonical routine name. */
            char_pos = (char *)&rout->source_corresp.name[
                                   rout->variant.ctor_dtor.base_name_offset+1];
          }  /* if */
        }  /* if */
#endif /* IA64_ABI */
        if (generate_routine) {
          generate_one_instantiation_C_output_file(
                                        &rout->source_corresp,
                                        rout->instantiation_needed_bit_number,
                                        char_pos);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  /* Look through the list of variables to find all instantiated static
     data members. */
  for (var = il_header.primary_scope->variables;
       var != NULL;
       var = var->next) {
    if (var->instantiation_needed_bit_number != 0 &&
        /* Ignore generated __link variables. */
        var->is_template_variable) {
      generate_one_instantiation_C_output_file(
                                         &var->source_corresp,
                                         var->instantiation_needed_bit_number,
                                         (char *)NULL);
    }  /* if */
  }  /* for */
}  /* generate_instantiation_C_output_files */

#endif /* ONE_INSTANTIATION_PER_OBJECT */

static void c_gen_be_init(void)
/*
Initialize for the C-generating back end.  These are initializations that
need to be done only once even if multiple C files are generated.
The IL is already available when this routine is called.
*/
{
  f_primary = NULL;
#if C_GEN_BE_NEEDS_MODULE_ID
  /* Make a string based on the module name that is used to qualify
     static names to make them unique. */
  module_id = make_module_id((char *)NULL);
  /* Get module name for use in name of file-scope init routine. */
  module_init_id = (char *)module_id;
#endif /* C_GEN_BE_NEEDS_MODULE_ID */
#if !C_GEN_BE_GENERATES_ANSI_C
  module_list_for_union_init = NULL;
#if !USE_INIT_SECTION_IN_GENERATED_C
  if (il_header.source_language != sl_Cplusplus) {
    /* Use shorter module id in C mode because the name might have to
       be used in a "-i" option. */
    module_init_id = derived_name(primary_source_file_name, "");
    change_non_id_characters(module_init_id);
  }  /* if */
#endif /* !USE_INIT_SECTION_IN_GENERATED_C */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
#if ONE_INSTANTIATION_PER_OBJECT
  C_output_file_name_buffer = NULL;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  /* Set out the output control block used for interface with the il_to_str
     routines. */
  clear_il_to_str_output_control_block(&octl);
  octl.output_str = write_tok_str_octl;
  octl.output_partial_token_str = write_str_octl;
  octl.output_name = gen_name_reference;
  octl.output_temp_name = dump_temp_name;
  octl.output_func_declarator = dump_function_declarator;
  octl.output_expression = dump_expression_for_il_to_str;
  octl.gen_compilable_code = TRUE;
  octl.is_typedef_invisible = is_typedef_invisible_in_c_gen_be;
#if !C_GEN_BE_GENERATES_ANSI_C
  octl.gen_pcc_code = TRUE;
#else /* C_GEN_BE_GENERATES_ANSI_C */
  octl.gen_pcc_code = il_header.pcc_compatibility_mode;
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  octl.suppress_local_typedefs = TRUE;
  octl.c_generating_back_end = TRUE;
#if !C_GEN_BE_GENERATES_ANSI_C
  /* When generating K&R C, double and long double must be the same size. */
  if (targ_sizeof_double != targ_sizeof_long_double ||
      targ_alignof_double != targ_alignof_long_double) {
    internal_error(
         "double and long double must be the same size when generating K&R C");
  }  /* if */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  /* Assume that the target compiler is at least a C99 compiler that recognizes
     "_Bool" (octl.render_c99_bool can be set to FALSE to render the
     underlying type (typically, char or int) instead). */
  octl.render_c99_bool = TRUE;
  curr_default_fp_contract = (a_stdc_pragma_value)stdc_pv_default;
  curr_default_fenv_access = (a_stdc_pragma_value)stdc_pv_default;
  curr_default_cx_limited_range = (a_stdc_pragma_value)stdc_pv_default;
#if FIXED_POINT_ALLOWED && !LOWER_FIXED_POINT
  curr_default_fx_full_precision = stdc_pv_default;
  curr_default_fx_fract_overflow = stdc_pv_default;
  curr_default_fx_accum_overflow = stdc_pv_default;
#endif /* FIXED_POINT_ALLOWED && !LOWER_FIXED_POINT */
#if UPC_EXTENSIONS_ALLOWED
  curr_default_upc_access_method = il_header.default_upc_strict_access ?
                                     (a_upc_access_method)upc_access_strict :
                                     (a_upc_access_method)upc_access_relaxed;
#endif /* UPC_EXTENSIONS_ALLOWED */
  pending_typedefs = NULL;
  last_pending_typedef = NULL;
  avail_pending_typedefs = NULL;
  name_prefix_components = NULL;
  last_name_prefix_component = NULL;
  subobject_offset = 0;
}  /* c_gen_be_init */


static void c_gen_be(void)
/*
Generate C from the intermediate language.
*/
{
  a_const_char *C_output_file_name;
#if STANDALONE_UTILITY_PROGRAM
  /* This is a command-line option normally, but it's not available in the
     standalone version. */
  a_const_char *gen_c_file_name = NULL;
#endif /* STANDALONE_UTILITY_PROGRAM */

  /* Do overall initialization. */
  c_gen_be_init();

  /* Determine the C output file name. */
  if (strcmp(primary_source_file_name, FILE_NAME_FOR_STDIN) == 0) {
    /* Primary source file is stdin, so use stdout here. */
    C_output_file_name = NULL;
  } else {
    /* If the generated C file name was specified on the command line,
       use that value.  Otherwise, generate a file name based on the
       source file name. */
    if (gen_c_file_name != NULL) {
      C_output_file_name = gen_c_file_name;
    } else {
      C_output_file_name = derived_name(primary_source_file_name,
                                        GEN_C_FILE_SUFFIX);
    }  /* if */
  }  /* if */

#if ONE_INSTANTIATION_PER_OBJECT
  if (il_header.instantiation_dir_name != NULL) {
    /* Generating one C file per instantiation.  For the primary file, use
       bit number 1 in the per-instantiation "needed" bit vector. */
    needed_flag_bit_number = 1;
  }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  /* Do per-file initialization. */
  c_gen_be_file_init();
  /* Generate the C output file. */
  generate_C_output_file(C_output_file_name);

#if ONE_INSTANTIATION_PER_OBJECT
  if (il_header.instantiation_dir_name != NULL) {
    generate_instantiation_C_output_files();
  }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
}  /* c_gen_be */

#if STANDALONE_C_GEN_BE

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

/*
The "main" routine must be outside of the EDG namespace, so do a
using-directive to make the EDG names visible.
*/
USING_NAMESPACE_EDG

int main(int argc, char *argv[])
/*
Simple "back end" that generates C.  This version is for use as a
separate program which gets an IL file from the front end.  This program
is invoked by

  c_gen_be file.cil

where file.cil specifies the IL file.  The output file name is determined 
from the primary source file name in the IL information.
*/
{
  FILE *f_il_input;
  int  optind = 1;

  /* Initialize the components of the front end needed by standalone
     utility programs. */
  standalone_utility_early_init();

  /* The source file name is unknown until the IL is read correctly. */
  primary_source_file_name = NULL;

  while (optind < argc && argv[optind][0] == '-') {
    /* Scan options.  There's a limited set, so we don't use getopt. */
    switch (argv[optind][1]) {
#if DEBUG
      case 'd':
        /* Scan debug argument */
        if (proc_debug_option(argv[optind]+2)) {
          command_line_error(ec_cl_error_in_debug_option_argument);
        }  /* if */
        break;
#endif /* DEBUG */
#if !C_GEN_BE_GENERATES_ANSI_C
      case 'i':
        /* -i option -- specifies union initialization routines to be
           called. */
        module_list_for_union_init = argv[optind]+2;
        break;
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
      default:
        str_command_line_error(ec_cl_invalid_option, argv[optind]);
    }  /* switch */
    optind++;
  }  /* while */
  if (optind != argc - 1) {
    command_line_error(ec_cl_back_end_requires_il_file);
  }  /* if */
  /* Open the IL file. */
  f_il_input = fopen(argv[optind], "rb");
  if (f_il_input == NULL) {
    str_command_line_error(ec_cl_could_not_open_il_file, argv[optind]);
  }  /* if */
  /* Read the file-scope IL. */
  il_read(f_il_input);
  /* Complete the initialization (based on il_header contents). */
  standalone_utility_late_init();
  primary_source_file_name = il_header.primary_source_file->file_name;
  /* Generate C code. */
  c_gen_be();
  (void)fclose(f_il_input);
  normal_termination();
  return 0;  /* Not reached; here to make lint et al. happy. */
}  /* main */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

#else /* !STANDALONE_C_GEN_BE */

void back_end(void)
/*
Simple "back end" that generates C.  This version is for use as a
subroutine called in the same program as the front end.
*/
{
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  /* If the intermediate language was written to a file, read it back in. */
  /* The source file name is unknown until the IL is read correctly. */
  primary_source_file_name = NULL;
  if (skip_il_read) {
    /* The IL should still be in memory. */
  } else {
    il_read(f_il_output);
  }  /* if */
  primary_source_file_name = il_header.primary_source_file->file_name;
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
  /* Generate C code. */
  c_gen_be();
  /* Note that the file scope memory region is not freed here.  It will
     be freed by the front end wrapup process. */
}  /* back_end */
#endif /* STANDALONE_C_GEN_BE */

#if MAKE_FRONT_END_CALLABLE

void c_gen_be_cleanup(void)
/*
This routine is called at the end of compilation, or if compilation is
terminated prematurely for some reason.  It performs any cleanup operations
required.  In particular, it closes any files that may have been open at
the point at which the compilation was terminated.
*/
{
  close_file_if_open(&f_primary);
#if !C_GEN_BE_GENERATES_ANSI_C
  close_file_if_open(&f_file_scope_inits);
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
  close_file_if_open(&f_rout_dynamic_inits);
}  /* c_gen_be_cleanup */

#endif /* MAKE_FRONT_END_CALLABLE */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#else /* !BACK_END_IS_C_GEN_BE */

#ifdef USING_QUANTIFY
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */
/*
Quantify has a bug that causes an error when an empty object file is used,
so generate a dummy variable.
*/
char quantify_dummy_in_c_gen_be;
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
#endif /* ifndef USING_QUANTIFY */

#endif /* BACK_END_IS_C_GEN_BE */


