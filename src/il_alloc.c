/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

il_alloc.c -- Allocation of intermediate language entries.

*/

/* Header files common to all files. */
#include "fe_common.h"
#include "lower_il.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Additional header files. */
#if !STANDALONE_UTILITY_PROGRAM
#include "pch.h"
#endif /* !STANDALONE_UTILITY_PROGRAM */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

#if !STANDALONE_UTILITY_PROGRAM
#if DEBUG

using an_allocation_counter = unsigned long;
		/* The type used for an allocation count. */

STATIC_THREAD an_allocation_counter
		string_literal_text_space_allocated;
			/* The number of bytes allocated for string
			   literals. */

STATIC_THREAD an_allocation_counter
		num_il_entry_prefixes_allocated;
			/* The number of IL entry prefixes allocated (this
			   should be equivalent to the number of IL entries
			   allocated). */

STATIC_THREAD an_allocation_counter
		num_fs_expr_nodes_allocated;
			/* The number of expression nodes that have been
			   allocated in file scope. */

STATIC_THREAD an_allocation_counter
		num_rescan_fs_expr_nodes_allocated;
			/* The number of file scope expression nodes that have
			   been allocated while doing expression rescanning. */

STATIC_THREAD an_allocation_counter
		num_trans_unit_copy_address_pointers_allocated;
			/* The number of copy address pointers (there should be
			   one for each IL entry in any secondary translation
			   units when doing multi-translation unit
			   compilation). */

#if ORPHAN_PROCESSING_NEEDED

STATIC_THREAD an_allocation_counter
		num_fs_orphan_pointers_allocated;
			/* The number of orphan pointers (there should be one
			   for each IL entry in file scope when orphan
			   processing is enabled). */

#endif /* ORPHAN_PROCESSING_NEEDED */
#if ASM_SUPPORT_NEEDED

STATIC_THREAD an_allocation_counter
		asm_function_body_space_allocated;
			/* The number of bytes allocated for storing asm
			   function bodies. */

#endif /* ASM_SUPPORT_NEEDED */

STATIC_THREAD an_allocation_counter
		il_allocation_counter[iek_last];
			/* An array of counters keeping track of the number of
			   IL entries that have been allocated for each of the
			   respective IL entry kinds.

			   Some IL entries kinds are untracked (this is done to
			   simplify indexing into the array at a trivial cost
			   of extra memory in DEBUG builds); see is_tallied for
			   the logic specifying which IL entry kinds are
			   tracked. */


static inline a_boolean is_tallied(an_il_entry_kind kind)
/*
Return TRUE if the given IL entry kind is tallied in the il_allocation_counter
array; otherwise, return FALSE.
*/
{
  return kind != iek_none && kind != iek_last && !is_string_entry_kind(kind);
}  /* is_tallied */


void f_tally_alloc(an_il_entry_kind kind)
/*
Track an allocation of an IL entry with the given IL entry kind.
*/
{
  /* This function should only be called on IL entry kinds that are tallied. */
  check_assertion(is_tallied(kind));
  ++il_allocation_counter[kind];
}  /* f_tally_alloc */

#endif /* DEBUG */

/* Static variable and macro for quickly initializing the source_corresp
   field of an IL entry to default values. */
STATIC_THREAD a_source_correspondence
		def_source_corresp;


static inline void set_default_source_corresp(a_source_correspondence_ptr scp)
/*
Clear the given source correspondence value, and associate this entity with the
module entity that declared it (if any).
*/
{
  (*scp) = def_source_corresp;

  /* Mark this declaration as declared by the current module entity
     (if any). */
  if (module_entity_stack != NULL && !module_entity_stack->is_empty()) {
    a_module_entity_stack_entry &mese = module_entity_stack->back_elem();

    scp->module_entity = mese.mep;
  }  /* if */
}  /* set_default_source_corresp */


STATIC_THREAD size_t
		file_scope_entry_prefix_size;
			/* The size of the entry prefix for IL entries
			   allocated in the file scope of the current
			   translation unit. */

STATIC_THREAD size_t
		file_scope_entry_prefix_alignment_offset;
			/* The offset from the beginning of the space allocated
			   for an IL entry to where the prefix actually
			   begins for file scope allocations.  This is a
			   translation unit variable. */

STATIC_THREAD size_t
		non_file_scope_entry_prefix_size;
			/* The size of the entry prefix for IL entries
			   not allocated in the file scope.  This is not a
			   translation unit variable. */

STATIC_THREAD size_t
		non_file_scope_entry_prefix_alignment_offset;
			/* The offset from the beginning of the space allocated
			   for an IL entry to where the prefix actually
			   begins for non-file-scope allocations.  This is
			   not a translation unit variable. */

/*
Macro to increment the entry prefix allocation count only if DEBUG
is TRUE.  Used in do_alloc.
*/
#if DEBUG
#define incr_num_il_entry_prefixes_allocated()                        \
  num_il_entry_prefixes_allocated++
#else /* !DEBUG */
#define incr_num_il_entry_prefixes_allocated() /* Nothing */
#endif /* DEBUG */


// coverity[ -taint_source ]
static inline char *do_alloc(a_memory_region_number region_number,
                             a_boolean              is_in_file_scope,
                             sizeof_t               size)
/*
Allocate an IL entry of size "size" preceded by an_il_entry_prefix, and
initialize the latter to default values.  Return a pointer to the storage for
the IL entry.  The allocation is done in the memory region region_number.
is_in_file_scope is TRUE if the allocation is in the file scope.  (Yes, that
could be determined from region_number, but it happens that it is usually known
by the caller).
*/
{
  char *ptr = alloc_in_region(region_number,
                              size + non_file_scope_entry_prefix_size);

  /* There may be padding before the prefix if needed for alignment. */
  ptr += non_file_scope_entry_prefix_alignment_offset;
  incr_num_il_entry_prefixes_allocated();
  clear_il_entry_prefix(ptr, is_in_file_scope, !is_primary_translation_unit);
  init_memory_region_metadata(ptr,
                              curr_translation_unit->file_scope_region_number,
                              region_number);
  ptr += SPACE_FOR_IL_ENTRY_PREFIX;
  return ptr;
}  /* do_alloc */


/*
Macro to increment the count of next-orphan pointers allocated only if
DEBUG is TRUE.  Used in do_fs_alloc.
*/
#if ORPHAN_PROCESSING_NEEDED && DEBUG
#define incr_num_fs_orphan_pointers_allocated()                       \
  num_fs_orphan_pointers_allocated++
#else /* !(ORPHAN_PROCESSING_NEEDED && DEBUG) */
#define incr_num_fs_orphan_pointers_allocated() /* Nothing */
#endif /* ORPHAN_PROCESSING_NEEDED && DEBUG */

/*
Macro to increment the count of translation unit copy address pointers
allocated.  When not generating debugging code, this expands to nothing.
*/
#if DEBUG
#define incr_num_trans_unit_copy_address_pointers_allocated()              \
  num_trans_unit_copy_address_pointers_allocated++
#else /* !DEBUG */
#define incr_num_trans_unit_copy_address_pointers_allocated() /* Nothing */
#endif /* DEBUG */

/*
Macro that clears the orphan pointer, increments the count of orphan
pointers allocated, and updates the pointer provided to point past
the orphan pointer.  When orphan pointers are not used, this macro
expands to nothing.
*/
#if ORPHAN_PROCESSING_NEEDED
#define clear_and_incr_past_orphan_pointer(ptr)				\
  incr_num_fs_orphan_pointers_allocated();                            \
  *(char **)ptr = NULL;                                               \
  ptr += SPACE_FOR_FS_ORPHAN_POINTER
#else /* !ORPHAN_PROCESSING_NEEDED */
#define clear_and_incr_past_orphan_pointer(ptr) /* nothing */
#endif /* ORPHAN_PROCESSING_NEEDED */

/*
Macro that clears the translation unit copy address pointer when compiling
multiple translation units.
*/
#define clear_and_incr_past_trans_unit_copy_address_pointer(ptr)	      \
  incr_num_trans_unit_copy_address_pointers_allocated();                      \
  *(char **)ptr = NULL;                                               \
  ptr += SPACE_FOR_TRANS_UNIT_COPY_ADDRESS_POINTER


// coverity[ -taint_source ]
static inline char *do_fs_alloc(a_memory_region_number fs_region_number,
                                sizeof_t               size)

/*
Allocate a file-scope IL entry of size "size" preceded by an_il_entry_prefix
and (if appropriate) an orphan list pointer, and initialize the prefix and
orphan pointer to default values.  Return a pointer to the storage for the IL
entry.  fs_region_number indicates the file scope region number to be used
(there can be several, when secondary translation units are involved).
*/
{
  char *ptr = alloc_in_region(fs_region_number,
                              size + file_scope_entry_prefix_size);

  /* There may be padding before the prefix if needed for alignment. */
  ptr += file_scope_entry_prefix_alignment_offset;
  if (!is_primary_translation_unit) {
    clear_and_incr_past_trans_unit_copy_address_pointer(ptr);
  }  /* if */
  clear_and_incr_past_orphan_pointer(ptr);
  incr_num_il_entry_prefixes_allocated();
  clear_il_entry_prefix(ptr, TRUE, !is_primary_translation_unit);
  init_memory_region_metadata(ptr, fs_region_number, fs_region_number);
  ptr += SPACE_FOR_IL_ENTRY_PREFIX;
  return ptr;
}  /* do_fs_alloc */


static inline char* do_any_alloc(a_memory_region_number region_number,
                                 sizeof_t               size)
/*
Allocate space in an arbitrary memory region (i.e., choose between the
file-scope and normal allocation methods as necessary).
*/
{
  if (region_number == file_scope_region_number) {
    return do_fs_alloc(file_scope_region_number, size);
  } else {
    return do_alloc(region_number, FALSE, size);
  }  /* if */
}  /* do_any_alloc */


#ifdef TRACE_ALLOC
/*
If a problem is found with a node allocated at address A, it is often useful
to find where that node was created.  The following simple facility allows
this to be traced as follows:
   (a) determine the suspect node address A in a debugger
   (b) set a breakpoint on main() and on alloc_intercept()
   (c) rerun the same binary with the same options and input
   (d) when the breakpoint on main() is hit, set trace_alloc_ptr to A
           (e.g., "p trace_alloc_ptr=0x123456" in gdb)
   (e) continue execution: the breakpoint on alloc_intercept() will be hit
       when the suspect node is created

Note that this may not work as expected for addresses determined after the IL
has been read from a file, because the nodes were allocated at a different
address before the IL was written out.

Note that this variable is not re-initialized if the front end is called
multiple times.

On many modern operating systems, repeated runs of the same binary may not
yield the same addresses.  In that case, in a configuration with
MAINTAIN_ALLOCATION_SEQUENCE_NUMBER set to TRUE, the technique above can be
used instead by setting the variable trace_seq_number to the entry to be
traced.  The sequence number of an allocated entry can be determined from
the debugger using the db_prefix debug function.
*/
void *trace_alloc_ptr = NULL;

#if MAINTAIN_ALLOCATION_SEQUENCE_NUMBER
STATIC_THREAD unsigned long
		trace_seq_number = 0;
#endif /* MAINTAIN_ALLOCATION_SEQUENCE_NUMBER */

static void alloc_intercept(void *ptr)
/*
This routine's main purpose is to have a breakpoint set on it from a symbolic
debugger.  The routine is called if memory is allocated at the address pointed
to by trace_alloc_ptr.
*/
{
#if DEBUG
  fprintf(f_debug, "Created node at %p.\n", (void*)ptr);
#endif /* DEBUG */
}  /* alloc_intercept */


void trace_alloc_check(void *ptr)
/*
Check if the given pointer ptr matches the address stored in trace_alloc_ptr.
If so, call alloc_intercept.
*/
{
  if (ptr == trace_alloc_ptr
#if MAINTAIN_ALLOCATION_SEQUENCE_NUMBER
      || (trace_seq_number != 0 &&
          il_entry_prefix_of(ptr).alloc_seq_number == trace_seq_number)
#endif /* MAINTAIN_ALLOCATION_SEQUENCE_NUMBER */
                                                                       ) {
    alloc_intercept(ptr);
  }  /* if */
}  /* trace_alloc_check */

#else /* !TRACE_ALLOC */

#define trace_alloc_check(ptr)  /* Nothing */

#endif /* TRACE_ALLOC */

char *alloc_il(sizeof_t size)
/*
Allocate and return "size" bytes of storage in the file scope memory region.
*/
{
  /* If this assertion fails an IL allocation occurred outside of the front end
     that was targeted towards a non-primary file scope memory region.  In
     other words, an attempt was made to allocate IL for a secondary
     translation unit outside of the front end.  This should not happen as all
     translation units should have been merged into the primary translation
     unit. */
  check_assertion(in_front_end ||
                  file_scope_region_number == FILE_SCOPE_REGION_NUMBER);
  char *ptr = do_fs_alloc(file_scope_region_number, size);
  trace_alloc_check(ptr);
  return ptr;
}  /* alloc_il */


char *alloc_primary_file_scope_il(sizeof_t size)
/*
Allocate and return "size" bytes of storage in the file scope memory region
of the primary translation unit.
*/
{
  char      *ptr;
  a_boolean saved_is_primary_translation_unit = is_primary_translation_unit;

  is_primary_translation_unit = TRUE;
  if (!saved_is_primary_translation_unit) compute_il_prefix_size();
  ptr = do_fs_alloc(FILE_SCOPE_REGION_NUMBER, size);
  is_primary_translation_unit = saved_is_primary_translation_unit;
  if (!saved_is_primary_translation_unit) compute_il_prefix_size();
  trace_alloc_check(ptr);
  return ptr;
}  /* alloc_primary_file_scope_il */


static char *alloc_secondary_file_scope_il(sizeof_t               size,
                                           a_translation_unit_ptr tup)
/*
Allocate and return "size" bytes of storage in the file scope memory region
of the secondary translation unit identified by tup.
*/
{
  char      *ptr;
  a_boolean saved_is_primary_translation_unit = is_primary_translation_unit;

  is_primary_translation_unit = FALSE;
  if (saved_is_primary_translation_unit) compute_il_prefix_size();
  ptr = do_fs_alloc(tup->file_scope_region_number, size);
  is_primary_translation_unit = saved_is_primary_translation_unit;
  if (saved_is_primary_translation_unit) compute_il_prefix_size();
  trace_alloc_check(ptr);
  return ptr;
}  /* alloc_secondary_file_scope_il */


static char *alloc_cil(sizeof_t size)
/*
Allocate and return "size" bytes of storage in the current IL memory region.
*/
{
  char *ptr = do_any_alloc(curr_il_region_number, size);

  trace_alloc_check(ptr);
  return ptr;
}  /* alloc_cil */


static char *alloc_in_same_region_as(a_source_correspondence *scp,
                                     sizeof_t                size)
/*
Allocate and return "size" bytes of storage in the same memory region
as the entity whose source correspondence is given by scp.  If scp is
NULL, allocate the space in the current file scope memory region.
*/
{
  char *ptr;

  if (scp == NULL) {
    ptr = alloc_il(size);
  } else if (!in_file_scope(scp)) {
    ptr = alloc_cil(size);
  } else if (!in_secondary_trans_unit(scp)) {
    ptr = alloc_primary_file_scope_il(size);
  } else {
    /* Allocate in some secondary translation unit's file scope memory
       region. */
    a_translation_unit_ptr tup;
    check_assertion(in_front_end);
    if (scp->assoc_info != NULL) {
      tup = trans_unit_for_source_corresp(scp);
      check_assertion(tup != translation_units);
    } else {
      /* No associated symbol, so pick an arbitrary secondary translation
         unit. */
      if (!is_primary_translation_unit) {
        tup = curr_translation_unit;
      } else {
        tup = translation_units->next;
      }  /* if */
    }  /* if */
    ptr = alloc_secondary_file_scope_il(size, tup);
  }  /* if */
  return ptr;
}  /* alloc_in_same_region_as */


char *alloc_text_of_string_literal(sizeof_t size)
/*
Allocate space for the text of a string literal, and return a pointer to it.
The space allocated is large enough to contain "size" characters.
This routine exists as a way of tracking the space use.  The space is always
allocated at the file scope, because string values can be shared (at
least in non-pcc mode).
*/
{
#if DEBUG
  string_literal_text_space_allocated += (unsigned long)size;
#endif /* DEBUG */
  return alloc_il(size);
}  /* alloc_text_of_string_literal */


char *copy_string_to_region(a_memory_region_number region,
                            a_const_char           *string)
/*
Make a copy of the specified string in the memory region indicated by
"region" (which must be the front end region, file scope region, or
NO_MEMORY_REGION_NUMBER for general memory).
*/
{
  char *new_string;

  if (region == FRONT_END_REGION_NUMBER) {
    new_string = new_copy_of_string(string, FE_allocator<char>());
  } else if (region == file_scope_region_number) {
    new_string = new_copy_of_string(string, IL_allocator<char>());
  } else {
    check_assertion(region == NO_MEMORY_REGION_NUMBER);
    new_string = new_copy_of_string(string, General_allocator<char>());
  }  /* if */
  return new_string;
}  /* copy_string_to_region */


char *copy_string_of_length_to_region(a_memory_region_number region,
				      a_const_char           *string,
				      sizeof_t		     length)
/*
Make a copy of the specified string, whose length is specified by "length"
in the memory region indicated by "region" (which must be the front end
region, file scope region, or NO_MEMORY_REGION_NUMBER for general memory).
*/
{
  char		*new_string;

  if (region == FRONT_END_REGION_NUMBER) {
    new_string = (char *)alloc_fe(length+1);
  } else if (region == file_scope_region_number) {
    new_string = alloc_il(length+1);
  } else {
    check_assertion(region == NO_MEMORY_REGION_NUMBER);
    new_string = alloc_general(length+1);
  }  /* if */
  (void)strncpy(new_string, string, size_t_arg(length));
  /* Terminate the string. */
  new_string[length] = '\0';
  return new_string;
}  /* copy_string_of_length_to_region */


#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED

a_scope_orphaned_list_header_ptr alloc_scope_orphaned_list_header(
                                                 a_routine_ptr   assoc_routine,
                                                 a_scope_number  scope_number)
/*
Allocate a scope orphaned list header, initialize it with the indicated
routine and scope number, and return a pointer to it.
*/
{
  a_scope_orphaned_list_header_ptr solhp;

  solhp = alloc_il_of_type(a_scope_orphaned_list_header);
  solhp->assoc_routine = assoc_routine;
  solhp->scope_number = scope_number;
  solhp->orphaned_types = NULL;
  solhp->orphaned_variables = NULL;
  solhp->orphaned_namespaces = NULL;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  solhp->orphaned_src_seq_sublists = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  solhp->next = NULL;

  return solhp;
}  /* alloc_scope_orphaned_list_header */

#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */

a_source_file_ptr alloc_source_file(void)
/*
Allocate a source file entry, initialize it, and return a pointer to it.
*/
{
  a_source_file_ptr  sfp;

  /* Entries for secondary translation units are allocated in the primary
     translation unit file scope memory region. */
  sfp = (a_source_file_ptr)alloc_primary_file_scope_il(sizeof(a_source_file));
#if DEBUG
  f_tally_alloc(iek_source_file);
#endif /* DEBUG */
  sfp->file_name        = NULL;
  sfp->full_name        = NULL;
  sfp->name_as_written  = NULL;
  sfp->first_seq_number = 0;
  sfp->last_seq_number  = MAX_SEQ_NUMBER;  /* Not yet entered. */
  sfp->first_line_number= 0;
  sfp->first_child_file = NULL;
  sfp->last_child_file  = NULL;
  sfp->next             = NULL;
  sfp->assoc_module     = NULL;
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
  sfp->related_file_implicit_include_done = FALSE;
  sfp->is_implicit_include = FALSE;
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
  sfp->is_include_file = FALSE;
  sfp->included_by_system_include = FALSE;
  sfp->included_by_preinclude = FALSE;
  sfp->preinclude_macros_only = FALSE;
  sfp->from_system_include_dir = FALSE;
  sfp->top_level_file = FALSE;
  sfp->top_level_file_from_pch = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  sfp->is_assembly_file = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

  return sfp;
}  /* alloc_source_file */

#if MICROSOFT_EXTENSIONS_ALLOWED

a_cli_metadata_file_ptr alloc_cli_metadata_file(void)
/*
Allocate a CLI metadata file entry, clear it to default values, and return
a pointer to it.
*/
{
  a_cli_metadata_file_ptr cmfp;

  cmfp = alloc_il_of_type(a_cli_metadata_file);
  cmfp->name_as_written = NULL;
  cmfp->full_name       = NULL;
  cmfp->next            = NULL;
  cmfp->position        = null_source_position;
  cmfp->assembly_index  = 0;
#if MICROSOFT_EXTENSIONS_ALLOWED
  cmfp->assembly_file   = NULL;
  cmfp->inserted_position = null_source_position;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  cmfp->as_friend       = FALSE;
  cmfp->referenced_by_preusing     = FALSE;
  cmfp->referenced_by_system_using = FALSE;

  return cmfp;
}  /* alloc_cli_metadata_file */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if ONE_INSTANTIATION_PER_OBJECT

a_per_instantiation_needed_flags_entry_ptr
            alloc_per_instantiation_needed_flags_entry(a_boolean at_file_scope)
/*
Allocate a per-instantiation needed flags entry, clear it to default values,
and return a pointer to it.  The entry is allocated in the file scope memory
region if at_file_scope is TRUE.
*/
{
  a_per_instantiation_needed_flags_entry_ptr pinfep;

  if (at_file_scope) {
    pinfep = (a_per_instantiation_needed_flags_entry_ptr)
                     alloc_primary_file_scope_il(
                               sizeof(a_per_instantiation_needed_flags_entry));
  } else {
    pinfep = (a_per_instantiation_needed_flags_entry_ptr)
                     alloc_cil(sizeof(a_per_instantiation_needed_flags_entry));
  }  /* if */
#if DEBUG
  f_tally_alloc(iek_per_instantiation_needed_flags_entry);
#endif /* DEBUG */
  pinfep->next = NULL;
  memzero((char *)pinfep->bytes, sizeof(pinfep->bytes));
  return pinfep;
}  /* alloc_per_instantiation_needed_flags_entry */

#endif /* ONE_INSTANTIATION_PER_OBJECT */

a_subobject_path_ptr alloc_subobject_path(void)
/*
Allocate an entry for a subobject path and return a pointer to it.  The entry
is allocated in the current memory region.
*/
{
  a_subobject_path_ptr  entry;

  entry = alloc_cil_of_type(a_subobject_path);
  entry->next = NULL;
  entry->is_offset = FALSE;
  entry->is_base_class = FALSE;
  entry->is_converted = FALSE;
  entry->variant.field = NULL;
  return entry;
}  /* alloc_subobject_path */


static a_constexpr_if_ptr alloc_constexpr_if(void)
/*
Allocate an entry for a constexpr if and return a pointer to it.  The entry
is allocated in the current memory region.
*/
{
  a_constexpr_if_ptr  entry;

  entry = alloc_cil_of_type(a_constexpr_if);
  entry->then_statement = NULL;  
  entry->else_statement = NULL;  
#if EXTRA_SOURCE_POSITIONS_IN_IL
  entry->else_position = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  entry->value_known = FALSE;
  entry->value = FALSE;
  return entry;
}  /* alloc_constexpr_if */


void set_template_param_constant_kind(a_constant                     *cp,
                                      a_template_param_constant_kind kind)
/*
Set the kind of a template parameter constant.  *cp is already a
ck_template_param constant.
*/
{
  check_assertion_str(cp->kind == (a_constant_repr_kind)ck_template_param,
                    "set_template_param_constant_kind: not ck_template_param");
  cp->variant.template_param.kind = kind;
  cp->variant.template_param.is_qualified_name = FALSE;
  cp->variant.template_param.has_address_of = FALSE;
  cp->variant.template_param.is_pack = FALSE;
  cp->variant.template_param
             .has_generic_cast_for_nontype_template_param = FALSE;
#if PROTOTYPE_INSTANTIATIONS_IN_IL
  cp->variant.template_param.local_expr_ref = FALSE;
#endif /* PROTOTYPE_INSTANTIATIONS_IN_IL */
  cp->variant.template_param.do_not_rescan = FALSE;
  switch (kind) {
    case tpck_param:
      cp->variant.template_param.variant.coordinates.position = 0;
      cp->variant.template_param.variant.coordinates.depth = NO_NESTING_DEPTH;
      break;
    case tpck_expression:
      cp->variant.template_param.variant.expr = NULL;
      break;
    case tpck_member:
      break;
    case tpck_unknown_function:
      cp->variant.template_param.variant.unknown_function.conversion_type =
                                                                          NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
      cp->variant.template_param.variant.unknown_function
                                               .property_or_event_descr = NULL;
      cp->variant.template_param.variant.unknown_function.special_kind =
                                             (a_special_function_kind)sfk_none;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      cp->variant.template_param.variant.unknown_function.symbol = NULL;
      cp->variant.template_param.variant.unknown_function.opname_kind =
                                                      (an_opname_kind)onk_none;
      break;
    case tpck_dependent_constant:
    case tpck_address:
      cp->variant.template_param.variant.constant = NULL;
      break;
    case tpck_concat_string_literals:
      cp->variant.template_param.variant.string_literal_list = NULL;
      break;
    case tpck_sizeof:
    case tpck_datasizeof:
    case tpck_alignof:
    case tpck_uuidof:
    case tpck_typeid:
    case tpck_noexcept:
      cp->variant.template_param.variant.templ_sizeof.type = NULL;
      cp->variant.template_param.variant.templ_sizeof.expr = NULL;
      cp->variant.template_param.variant.templ_sizeof.is_std_alignof = FALSE;
      break;
    case tpck_template_ref:
      cp->variant.template_param.variant.template_ref.con = NULL;
      cp->variant.template_param.variant.template_ref.arg_list = NULL;
      break;
    case tpck_integer_pack:
      cp->variant.template_param.variant.bound = NULL;
      break;
    case tpck_destructor:
      cp->variant.template_param.variant.destructor.type = NULL;
      cp->variant.template_param.variant.destructor.unqualified = FALSE;
      break;
    default:
      unexpected_condition_str("set_template_param_constant_kind: bad kind");
  }  /* switch */
}  /* set_template_param_constant_kind */


void set_constant_kind(a_constant           *cp,
                       a_constant_repr_kind kind)
/*
Set the kind of the constant to "kind", and set the associated variant
fields to default values.
*/
{
  /* When changing this routine because the structure of a_constant
     has changed, be sure to change eq_constants as well. */
  cp->kind = kind;
  switch (kind) {
    case ck_error:
    case ck_void:
      /* No variant fields to set. */
      break;
#if UPC_EXTENSIONS_ALLOWED
    case ck_upc_mythread:
      /* No variant fields to set. */
      break;
    /* Handle UPC thread constants like integers. */
    case ck_upc_threads:
#endif /* UPC_EXTENSIONS_ALLOWED */
    case ck_integer:
      set_integer_value(&cp->variant.integer_value,
                        (a_host_large_integer)0);
      break;
#if FIXED_POINT_ALLOWED
    case ck_fixed_point:
      fxp_init_value(&cp->variant.fixed_point_value);
      break;
#endif /* FIXED_POINT_ALLOWED */
    case ck_string:
      cp->variant.string.length = 0;
      cp->variant.string.value = NULL;
#if PRESERVE_EMBED_DIRECTIVE_WHEN_OPTIMIZED
      cp->variant.string.embed_directive = NULL;
#endif /* PRESERVE_EMBED_DIRECTIVE_WHEN_OPTIMIZED */
#if DO_IL_LOWERING && ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS
      cp->variant.string.sequence_number = 0;
#endif /* DO_IL_LOWERING && ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS */
      cp->variant.string.literal_kind = SCLK_NOT_A_LITERAL;
      cp->variant.string.func_name_tok = FALSE;
      cp->variant.string.embed_expansion = FALSE;
      break;
    case ck_float:
#if C99_IL_EXTENSIONS_SUPPORTED
    case ck_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      /* The entire float_value must be zeroed to allow use of memcmp
         and the like on the field. */
      memzero((char *)&cp->variant.float_value,
              sizeof(cp->variant.float_value));
      break;
#if C99_IL_EXTENSIONS_SUPPORTED
    case ck_complex:
      /* The entire float_value must be zeroed to allow use of memcmp
         and the like on the field. */
      cp->variant.complex_value = alloc_il_of_type(an_internal_complex_value);
      memzero((char *)cp->variant.complex_value,
              sizeof(an_internal_complex_value));
      break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case ck_address:
      cp->variant.address.kind = (an_address_base_kind)abk_variable;
      cp->variant.address.one_past_the_end = FALSE;
      cp->variant.address.is_object_reflection = FALSE;
      cp->variant.address.variant.variable = NULL;
      cp->variant.address.offset = 0;
      cp->variant.address.subobject_path = NULL;
      break;
    case ck_ptr_to_member:
      cp->variant.ptr_to_member.casting_base_class = NULL;
      cp->variant.ptr_to_member.name_reference     = NULL;
      cp->variant.ptr_to_member.cast_to_base    = FALSE;
      cp->variant.ptr_to_member.is_function_ptr = FALSE;
      cp->variant.ptr_to_member.variant.field   = NULL;
      break;
#if GNU_EXTENSIONS_ALLOWED
    case ck_label_difference:
      cp->variant.label_difference.from_address = NULL;
      cp->variant.label_difference.to_address = NULL;
      break;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING && GENERATE_EH_TABLES && !DO_FULL_PORTABLE_EH_LOWERING
    case ck_stack_offset:
      cp->variant.stack_offset.variable = NULL;
      cp->variant.stack_offset.offset   = 0;
      break;
#endif /* DO_IL_LOWERING && ... */
    case ck_dynamic_init:
      cp->variant.dynamic_init.ptr = NULL;
#if DO_IL_LOWERING
      cp->variant.dynamic_init.field_or_base.field = NULL;
      cp->variant.dynamic_init.field_or_base.base = NULL;
#endif /* DO_IL_LOWERING */
      break;
    case ck_aggregate:
      cp->variant.aggregate.first_constant = NULL;
      cp->variant.aggregate.last_constant  = NULL;
      cp->variant.aggregate.has_dynamic_init_component = FALSE;
      cp->variant.aggregate.added_const_for_template_param = FALSE;
#if DO_IL_LOWERING
      cp->variant.aggregate.field_or_base.field = NULL;
      cp->variant.aggregate.field_or_base.base = NULL;
#endif /* DO_IL_LOWERING */
      break;
    case ck_init_repeat:
      cp->variant.init_repeat.constant = NULL;
      cp->variant.init_repeat.count = 0;
      cp->variant.init_repeat.multidimensional_aggr_tail_not_repeated = FALSE;
      break;
    case ck_template_param:
      set_template_param_constant_kind(cp, 
                                  (a_template_param_constant_kind)tpck_param);
      break;
    case ck_designator:
      cp->variant.designator.is_field_designator = FALSE;
      cp->variant.designator.is_generic = FALSE;
      cp->variant.designator.uses_direct_init_syntax = FALSE;
      cp->variant.designator.variant.array_element = 0;
      break;
    case ck_reflection:
      clear_tagged_ptr(cp->variant.reflection.entity);
      cp->variant.reflection.local_scope_number = FILE_SCOPE_NUMBER;
      break;
    default:
      unexpected_condition_str("set_constant_kind: bad kind");
  }  /* switch */
}  /* set_constant_kind */


void clear_constant(a_constant           *cp,
                    a_constant_repr_kind kind)
/*
Clear the indicated constant entry, set the kind as given, and set the
associated variant fields to default values.
*/
{
  /* When changing this routine because the structure of a_constant
     has changed, be sure to change eq_constants as well. */
  set_default_source_corresp(&cp->source_corresp);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  cp->end_position = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  cp->next           = NULL;
  cp->type           = NULL;
  cp->orig_type      = NULL;
  cp->expr           = NULL;
  cp->rescan_info    = NULL;
#if DO_IL_LOWERING
  cp->assoc_var      = NULL;
#endif /* DO_IL_LOWERING */
  cp->character_kind = (a_character_kind)chk_default;
  cp->implicit_cast  = FALSE;
  cp->explicit_cast_applied = FALSE;
  cp->is_reinterpret_cast = FALSE;
  cp->is_reinterpret_like_cast = FALSE;
  cp->non_arithmetic = FALSE;
  cp->is_simple_zero = FALSE;
  cp->null_pointer_constant_ruled_out = FALSE;
#if GNU_EXTENSIONS_ALLOWED
  cp->null_keyword = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
  cp->nullptr_keyword = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  cp->native_nullptr_keyword = FALSE;
  cp->ptr_to_mem_constant_construct = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  cp->explicit_braces_on_aggregate = FALSE;
  cp->explicit_parentheses_on_aggregate = FALSE;
  cp->from_undefined_preproc_id = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED
  cp->flexible_array_initializer = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED */
  cp->uses_designated_initializers = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  cp->is_literal_field = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  cp->is_pack_expansion = FALSE;
#if BACK_END_IS_C_GEN_BE
  cp->elide_aggregate_braces = FALSE;
#endif /* BACK_END_IS_C_GEN_BE */
  cp->is_named_constant_definition = FALSE;
  cp->partial_aggr_value = FALSE;
  cp->is_partially_initialized = FALSE;
  cp->implicit_aggr_element = FALSE;
  cp->is_compound_literal = FALSE;
  cp->is_result_of_constexpr_call = FALSE;
  cp->is_generic_initializer = FALSE;
#if DO_IL_LOWERING
  cp->has_been_prelowered = FALSE;
  cp->vptr_has_been_lowered = FALSE;
  cp->initializes_empty_object = FALSE;
  cp->is_implicit_initialization = FALSE;
#endif /* DO_IL_LOWERING */
  cp->constant_for_base_class = FALSE;
  cp->constant_for_base_class_from_constexpr_folding = FALSE;
  cp->part_of_constexpr_master_expr = FALSE;
  cp->local_expr_ref = FALSE;
  cp->folded_statement_expression = FALSE;
  cp->formed_from_promoted_storage = FALSE;
  set_constant_kind(cp, kind);
}  /* clear_constant */


a_constant_ptr alloc_constant(a_constant_repr_kind kind)
/*
Allocate a constant entry of the indicated kind, set its fields to default
values, and return a pointer to it.
*/
{
  a_constant_ptr cp;

  db_enter(5, "alloc_constant");

  cp = alloc_cil_of_type(a_constant);
  clear_constant(cp, kind);

  db_exit();
  return cp;
}  /* alloc_constant */


a_constant_ptr fs_constant(a_constant_repr_kind kind)
/*
Same as alloc_constant, but allocates a constant in the file scope memory
region.
*/
{
  a_constant_ptr         cp;
  a_memory_region_number region_to_switch_back_to;

  switch_to_file_scope_region(&region_to_switch_back_to);
  cp = alloc_constant(kind);
  switch_back_to_original_region(region_to_switch_back_to);
  return cp;
}  /* fs_constant */

#if CHECKING
/*
Counter for the number of local constants requested but not yet released,
used for an end-of-processing check that none were leaked.
*/
STATIC_THREAD long
                local_constants_in_use;
#endif /* CHECKING */

a_constant_ptr local_constant(void)
/*
Returns a pointer to uninitialized storage in the file scope memory region
that can be used for an a_constant object, either newly-allocated or reused
via the available_local_constants list.  This is used instead of a local
automatic object so that there will be an IL entry prefix preceding the
object, which is needed in some cases when copying constants.  (The IL
entry prefix of a reused local constant will be reinitialized, including
setting a unique allocation sequence number if so configured.)  The caller
of this routine is responsible to call release_local_constant when the
object is no longer needed, to prevent memory leakage and to allow its
reuse by this routine.
*/
{
  a_constant_ptr result;

  if (available_local_constants != NULL) {
    /* Reuse a previously-allocated constant. */
    result = available_local_constants;
    available_local_constants = result->next;
    clear_il_entry_prefix(&il_entry_prefix_of_no_check(result),
                          /*is_in_file_scope=*/TRUE,
                          !is_primary_translation_unit);
  } else {
    result = alloc_il_of_type(a_constant);
  }  /* if */
#if CHECKING
  ++local_constants_in_use;
#endif /* CHECKING */
  return result;
}  /* local_constant */


void release_local_constant(a_constant_ptr *cpp)
/*
Add the constant pointed to by *cpp to the available_local_constants list
so that it can be reused by local_constant (see above) and set *cpp to NULL
to prevent inadvertent use.
*/
{
  check_assertion(*cpp != NULL && in_file_scope(*cpp));
  (*cpp)->next = available_local_constants;
  available_local_constants = *cpp;
  *cpp = NULL;
#if CHECKING
  --local_constants_in_use;
#endif /* CHECKING */
}  /* release_local_constant */


a_constant_ptr move_local_constant_to_il(a_constant_ptr *cp)
/*
The local constant *cp is to be used in the IL.  If it need not be copied,
return it directly; otherwise, make a copy in the current memory region and
return that.  Set *cp to NULL to prevent its being inadvertently reused as
a local constant.
*/
{
  a_constant_ptr   result = *cp;

  if (curr_il_region_number != file_scope_region_number) {
    /* Local constants are allocated in the file scope memory region.  If
       we are not in the file scope region, we need a new constant in this
       one. */
    result = alloc_cil_of_type(a_constant);
    copy_constant(*cp, result);
    release_local_constant(cp);
  } else if (has_non_file_scope_ref(result)) {
    /* If the (file-scope) constant refers to something in a local scope,
       we need to copy the constant. */
    release_local_constant(cp);
    result = copy_constant_full(result, (a_constant_ptr)NULL, CE_NO_OPTIONS);
  } else {
    /* Just decrement the count of outstanding local constants and set the
       caller's pointer to NULL. */
#if CHECKING
    --local_constants_in_use;
#endif /* CHECKING */
    *cp = NULL;
  }  /* if */
  /* Clear the source correspondence information.  This version of the
     constant isn't the one directly associated with the source entity,
     if any. */
  break_constant_source_corresp(result);
  fix_memory_region_problems_in_copied_constant(result);
  return result;
}  /* move_local_constant_to_il */

#if CHECKING

void check_local_constant_use(void)
/*
Check to make sure that all local constants requested were released and
that no release requests were made for unrequested local constants.
*/
{
  error_position = null_source_position;
  check_assertion(local_constants_in_use == 0);
}  /* check_local_constant_use */

#endif /* CHECKING */

STATIC_THREAD a_param_type_ptr
		avail_param_types;
			/* List of freed parameter type entries that are
			   available for reuse. */


a_param_type_ptr alloc_param_type(a_type_ptr type)
/*
Allocate a new parameter type entry and return a pointer to it.  Set its
fields to default values and its type to "type".  It is always allocated
in the file scope memory region.
*/
{
  a_param_type_ptr        ptp;

  db_enter(5, "alloc_param_type");

  if (avail_param_types != NULL) {
    ptp = avail_param_types;
    avail_param_types = avail_param_types->next;
  } else {
    ptp = alloc_il_of_type(a_param_type);
  }  /* if */
  ptp->next = NULL;
  ptp->type = type;
  ptp->declared_type = NULL;
  ptp->name = NULL;
  ptp->has_name_conflict = FALSE;
  ptp->passed_via_copy_constructor = FALSE;
  ptp->has_default_arg = FALSE;
  ptp->default_arg_appeared_in_class_definition = FALSE;
  ptp->has_unevaluated_template_default = FALSE;
  ptp->default_being_instantiated = FALSE;
  ptp->type_involves_deduced_template_param = FALSE;
  ptp->type_involves_template_param = FALSE;
  ptp->is_parameter_pack = FALSE;
  ptp->is_pack_element = FALSE;
  ptp->was_nontrailing_pack = FALSE;
  ptp->is_auto_param = FALSE;
  ptp->qualifiers = TQ_NONE;
#if GNU_EXTENSIONS_ALLOWED
  ptp->is_transparent = FALSE;
  ptp->nonnull = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
  ptp->duplicate_name = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  ptp->is_cli_param_array = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  ptp->move_ctor_or_assign_parameter = FALSE;
  ptp->copy_or_move_ctor_parameter = FALSE;
  ptp->is_requires_expr_param = FALSE;
  ptp->is_explicit_this = FALSE;
  ptp->param_num = 0;
  ptp->default_arg_expr = NULL;
  ptp->orig_param_type_for_unevaluated_default_arg_expr = NULL;
  ptp->entities_defined_in_default_arg = NULL;
  ptp->attributes = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  ptp->ms_attributes = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  ptp->decl_pos_info = NULL;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  ptp->pack_expansion_descr = NULL;
  db_exit();
  return ptp;
}  /* alloc_param_type */


void free_param_type_list(a_param_type_ptr  ptp)
/*
Return a list of parameter type entries to the available list.
*/
{
  a_param_type_ptr  next_ptp;

  while (ptp != NULL) {
    next_ptp = ptp->next;
    ptp->next = avail_param_types;
    avail_param_types = ptp;
    ptp = next_ptp;
  }  /* while */
}  /* free_param_type_list */


a_derivation_step_ptr alloc_derivation_step(void)
/*
Allocate and initialize a derivation step entry and return a pointer to it.
*/
{
  a_derivation_step_ptr  dsp;

  db_enter(5, "alloc_derivation_step");

  dsp = alloc_il_of_type(a_derivation_step);
  dsp->next       = NULL;
  dsp->prev       = NULL;
  dsp->base_class = NULL;

  db_exit();
  return dsp;
}  /* alloc_derivation_step */


a_base_class_derivation_ptr alloc_base_class_derivation(void)
/*
Allocate and initialize a base class derivation entry and return a pointer
to it.
*/
{
  a_base_class_derivation_ptr  bcdp;

  db_enter(5, "alloc_base_class_derivation");
  bcdp = alloc_il_of_type(a_base_class_derivation);
  bcdp->next       = NULL;
  bcdp->path       = NULL;
  bcdp->path_tail  = NULL;
  bcdp->preferred  = FALSE;
  bcdp->direct     = FALSE;
  bcdp->access     = (an_access_specifier)as_public;
  db_exit();
  return bcdp;
}  /* alloc_base_class_derivation */

#if DO_IL_LOWERING && IA64_ABI

a_vcall_offset_entry_ptr alloc_vcall_offset_entry(void)
/*
Allocate a vcall offset entry, initialize its fields, and return a pointer to
it.
*/
{
  a_vcall_offset_entry_ptr voep;

  voep = alloc_il_of_type(a_vcall_offset_entry);
  voep->next               = NULL;
  voep->routine            = NULL;
  voep->base_class         = NULL;
  voep->vcall_offset_index = 0;
  voep->is_primary         = FALSE;

  return voep;
}  /* alloc_vcall_offset_entry */

#endif /* DO_IL_LOWERING && IA64_ABI */

an_overriding_virtual_function_ptr alloc_overriding_virtual_function(void)
/*
Allocate an overriding-virtual-function entry, initialize its fields, and
return a pointer to it.
*/
{
  an_overriding_virtual_function_ptr ovfp;

  ovfp = alloc_il_of_type(an_overriding_virtual_function);
  ovfp->next                         = NULL;
  ovfp->overriding_function          = NULL;
  ovfp->primary_function             = NULL;
  ovfp->base_class                   = NULL;
  ovfp->return_adjustment_base_class = NULL;

  return ovfp;
}  /* alloc_overriding_virtual_function */

STATIC_THREAD a_template_arg_ptr
		avail_template_args;
			/* List of freed template arg entries that are
			   available for reuse. */


a_template_arg_ptr alloc_template_arg(a_templ_arg_kind kind)
/*
Allocate a template argument entry, initialize its fields, and return
a pointer to it.  "kind" is the kind of template argument to be
allocated.
*/
{
  a_template_arg_ptr tap;

  if (avail_template_args != NULL) {
    tap = avail_template_args;
    avail_template_args = avail_template_args->next;
  } else {
    tap = alloc_il_of_type(a_template_arg);
  }  /* if */
  tap->next = NULL;
  tap->kind = kind;
  tap->pack_expansion_descr = NULL;
  tap->is_array_bound_of_unknown_type = FALSE;
  tap->explicitly_specified = FALSE;
  tap->template_template_param_checked = FALSE;
  tap->is_pack_element = FALSE;
  tap->is_pack = FALSE;
  tap->has_pack_ellipsis = FALSE;
  tap->is_integer_pack = FALSE;
  tap->type_is_injected_class_name = FALSE;
  tap->is_provisional_value = FALSE;
  tap->is_error = FALSE;
  tap->param_is_auto = FALSE;
  tap->param_is_decltype_auto = FALSE;
#if BACK_END_IS_CP_GEN_BE
  tap->access_being_checked = FALSE;
#endif /* BACK_END_IS_CP_GEN_BE */
  switch (kind) {
    case tak_type:
      tap->variant.type = NULL;
      break;
    case tak_template:
      tap->variant.templ.ptr = NULL;
      tap->variant.templ.substituted_param_template = NULL;
      break;
    case tak_nontype:
      /* It is not really necessary to initialize all of these fields, but
         this can be important in certain debugging modes. */
      tap->variant.integer_value = 0;
      tap->variant.constant = NULL;
      break;
    case tak_start_of_pack_expansion:
      break;
    default:
      unexpected_condition_str2("alloc_template_arg:", "bad kind");
      break;
  }  /* switch */
  tap->arg_operand = NULL;
#if BACK_END_IS_CP_GEN_BE
  tap->parent_arg = NULL;
#endif /* BACK_END_IS_CP_GEN_BE */
  return tap;
}  /* alloc_template_arg */


void free_template_arg_list(a_template_arg_ptr  tap)
/*
Return a list of template argument entries to the available list.
*/
{
  a_template_arg_ptr  next_tap;

  while (tap != NULL) {
    next_tap = tap->next;
    tap->next = avail_template_args;
    avail_template_args = tap;
    tap = next_tap;
  }  /* while */
}  /* free_template_arg_list */


static
a_template_param_type_supplement_ptr alloc_template_param_type_supplement(void)
/*
Allocate a template parameter type supplement entry, initialize its fields,
and return a pointer to it.
*/
{
  a_template_param_type_supplement_ptr tptsp;

  tptsp = alloc_il_of_type(a_template_param_type_supplement);
  tptsp->class_type = NULL;
  tptsp->orig_nested_type = NULL;
  tptsp->template_symbol = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  tptsp->generic_constraints = NULL;
  tptsp->generic_param_seq_number = 0;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  tptsp->coordinates.position = 0;
  tptsp->coordinates.depth = 0;
  tptsp->constraint.type_constraint = NULL;
  return tptsp;
}  /* alloc_template_param_type_supplement */


static a_typeref_type_supplement_ptr alloc_typeref_type_supplement(void)
/*
Allocate a typeref type supplement entry, initialize its fields, and return
a pointer to it.
*/
{
  a_typeref_type_supplement_ptr ttsp;

  ttsp = alloc_il_of_type(a_typeref_type_supplement);
#if UPC_EXTENSIONS_ALLOWED
  ttsp->upc_block_size = UPC_BLOCK_SIZE_NONE;
#endif /* UPC_EXTENSIONS_ALLOWED */
  ttsp->expr = NULL;
  ttsp->template_arg_list = NULL;
  ttsp->orig_template_arg_list = NULL;
#if DEFAULT_RECORD_FORM_OF_NAME_REFERENCE
  ttsp->name_qualifier = NULL;
#endif /* DEFAULT_RECORD_FORM_OF_NAME_REFERENCE */
  ttsp->assoc_template = NULL;
  ttsp->proxy_class = NULL;
  ttsp->operator_type_arg = NULL;
  ttsp->min_template_arguments = -1;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  ttsp->type_id_range = null_source_range;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  return ttsp;
}  /* alloc_typeref_type_supplement */


static an_integer_type_supplement_ptr alloc_integer_type_supplement(void)
/*
Allocate an integer type supplement entry, initialize its fields, and return
a pointer to it.
*/
{
  an_integer_type_supplement_ptr  itsp;

  itsp = alloc_il_of_type(an_integer_type_supplement);
  itsp->enumerator_list_seen = FALSE;
  itsp->enumerator_list_complete = FALSE;
  itsp->has_nodiscard_attribute = FALSE;
#if GNU_EXTENSIONS_ALLOWED
  itsp->underlying_type_should_use_unsigned = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  itsp->declared_assembly_visibility = (an_assembly_visibility)av_none;
  itsp->assembly_visibility = (an_assembly_visibility)av_none;
  itsp->assembly_scope_index = 0;
  itsp->metadata_type_def_token = 0;
  itsp->uuid_string = NULL;
  itsp->boxed_type = NULL;
#if DO_IL_LOWERING
  itsp->uuid_variable = NULL;
#endif /* DO_IL_LOWERING */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  itsp->base_type = NULL;
  itsp->bit_width = 0;
  itsp->base_type_position = null_source_position;
  itsp->assoc_template = NULL;
  return itsp;
}  /* alloc_integer_type_supplement */

#if MICROSOFT_EXTENSIONS_ALLOWED

a_partial_class_body_ptr alloc_partial_class_body(void)
/*
Allocate a partial class body entry, initialize its fields, and return
a pointer to it.
*/
{
  a_partial_class_body_ptr  pcbp;

  pcbp = alloc_il_of_type(a_partial_class_body);
  pcbp->next = NULL;
  pcbp->start_position = null_source_position;
  pcbp->end_position = null_source_position;
  pcbp->body_cache = NULL;
  pcbp->base_cache = NULL;
  return pcbp;
}  /* alloc_partial_class_body */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

a_base_class_ptr alloc_base_class(void)
/*
Allocate a base class entry, initialize its fields, and return a pointer
to it.
*/
{
  a_base_class_ptr bcp;

  bcp = alloc_il_of_type(a_base_class);
  bcp->next                            = NULL;
  bcp->next_direct                     = NULL;
#if IA64_ABI
  bcp->next_preorder                   = NULL;
  bcp->primary_base_class              = NULL;
#endif /* IA64_ABI */
  bcp->attributes                      = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  bcp->ms_attributes                   = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  bcp->type                            = NULL;
  bcp->orig_type                       = NULL;
  bcp->derived_class                   = NULL;
  bcp->trans_unit_corresp              = NULL;
  bcp->decl_position                   = null_source_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  bcp->base_specifier_range            = null_source_range;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  bcp->is_virtual                      = FALSE;
  bcp->direct                          = FALSE;
  bcp->ambiguous                       = FALSE;
  bcp->shares_virtual_function_info    = FALSE;
  bcp->ignore_during_dependent_lookup  = FALSE;
  bcp->is_optimized_empty_base         = FALSE;
#if IA64_ABI
  bcp->offset_is_set                   = FALSE;
#endif /* IA64_ABI */
  bcp->is_pack_expansion               = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  bcp->is_implicit_direct_base         = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  bcp->has_public_derivation           = FALSE;
  bcp->direct_base_number	       = 0;
  bcp->offset                          = 0;
#if !IA64_ABI
  bcp->pointer_offset                  = 0;
  bcp->pointer_base_class              = NULL;
#endif /* !IA64_ABI */
  bcp->derivation                      = NULL;
  bcp->variant.overriding_virtual_functions = NULL;
#if CFRONT_OBJECT_CODE_COMPATIBILITY
  bcp->complete_subobject              = FALSE;
  bcp->pointer_offset_is_set           = FALSE;
  bcp->data_section_base_class         = NULL;
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
#if DO_IL_LOWERING
#if !IA64_ABI
  bcp->virtual_function_table_var      = NULL;
#else /* IA64_ABI */
  bcp->virtual_function_table_offset   = -1;
#endif /* IA64_ABI */
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
  bcp->index_in_construction_vtbl_array = 0;
  bcp->base_subarray_index_in_construction_vtbl_array = 0;
#if !IA64_ABI
  bcp->base_construction_vtbls         = NULL;
#endif /* !IA64_ABI */
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
#if IA64_ABI
  bcp->vbase_offset_index              = 0;
#endif /* IA64_ABI */
#endif /* DO_IL_LOWERING */

  return bcp;
}  /* alloc_base_class */


a_class_list_entry_ptr alloc_list_entry_for_class_full(
                                                  a_source_correspondence *scp)
/*
Allocate a class-list-entry, initialize its fields, and return a pointer to it.
If scp is non-NULL, allocate the entry in the same memory region as scp;
otherwise, allocate it in the current file-scope memory region.
*/
{
  a_class_list_entry_ptr clep;

  clep = (a_class_list_entry_ptr)alloc_in_same_region_as(
                                                   scp,
                                                   sizeof(a_class_list_entry));
#if DEBUG
  f_tally_alloc(iek_class_list_entry);
#endif /* DEBUG */
  clep->next  = NULL;
  clep->class_type = NULL;

  return clep;
}  /* alloc_list_entry_for_class_full */


a_class_list_entry_ptr alloc_list_entry_for_class(void)
/*
Allocate a class-list-entry, initialize its fields, and return a pointer to it.
*/
{
  return alloc_list_entry_for_class_full((a_source_correspondence *)NULL);
}  /* alloc_list_entry_for_class */


a_routine_list_entry_ptr alloc_list_entry_for_routine(void)
/*
Allocate a routine-list-entry, initialize its fields, and return a pointer
to it.
*/
{
  a_routine_list_entry_ptr rlep;

  rlep = alloc_il_of_type(a_routine_list_entry);
  rlep->next  = NULL;
  rlep->routine = NULL;

  return rlep;
}  /* alloc_list_entry_for_routine */


a_variable_list_entry_ptr alloc_list_entry_for_variable(void)
/*
Allocate a variable-list-entry, initialize its fields, and return a pointer
to it.
*/
{
  a_variable_list_entry_ptr vlep;

  vlep = alloc_il_of_type(a_variable_list_entry);
  vlep->next  = NULL;
  vlep->variable = NULL;

  return vlep;
}  /* alloc_list_entry_for_variable */

#if NEED_NAME_MANGLING

STATIC_THREAD a_constant_list_entry_ptr
                avail_constant_list_entries;
                        /* A list of available a_constant_list_entry
                           entries. */

a_constant_list_entry_ptr alloc_list_entry_for_constant(void)
/*
Allocate a constant-list-entry (in the file scope), initialize its fields, and
return a pointer to it.
*/
{
  a_constant_list_entry_ptr clep;

  if (avail_constant_list_entries == NULL) {
    clep = alloc_il_of_type(a_constant_list_entry);
  } else {
    clep = avail_constant_list_entries;
    avail_constant_list_entries = clep->next;
  }  /* if */
  clep->next  = NULL;
  clep->constant = NULL;

  return clep;
}  /* alloc_list_entry_for_constant */


void free_list_of_constant_list_entries(a_constant_list_entry_ptr list)
/*
Return the list of constant-list-entries to the pool of available entries.
*/
{
  a_constant_list_entry_ptr clep;

  if (avail_constant_list_entries == NULL) {
    avail_constant_list_entries = list;
  } else {
    for (clep = avail_constant_list_entries;
         clep->next != NULL;
         clep = clep->next) {}
    clep->next = list;
  }  /* if */
}  /* free_list_of_constant_list_entries */

#endif /* NEED_NAME_MANGLING */

a_based_type_list_member_ptr alloc_based_type_list_member(
                                               a_based_type_kind  kind,
                                               a_type_ptr         base_type)
/*
Allocate a based type list member, initialize it to the indicated kind, and
return a pointer to it.  Allocate the entry in the same memory region as
base_type, because base_type will point to the entry.
*/
{
  a_based_type_list_member_ptr btlmp;

  btlmp = (a_based_type_list_member_ptr)alloc_in_same_region_as(
                                             &base_type->source_corresp,
                                             sizeof(a_based_type_list_member));
#if DEBUG
  f_tally_alloc(iek_based_type_list_member);
#endif /* DEBUG */
  btlmp->next = NULL;
  btlmp->based_type = NULL;
  btlmp->kind = kind;
  btlmp->front_end_only = FALSE;

  return btlmp;
}  /* alloc_based_type_list_member */


static void clear_class_type_supplement_definition_fields(
                                            a_class_type_supplement_ptr  ctsp)
/*
Given an pointer to a class-type-supplement entry, clear its fields that are
only meaningful when a definition (as opposed to just a declaration) of the
class is available.
*/
{
  ctsp->base_classes                      = NULL;
  ctsp->direct_base_classes               = NULL;
#if IA64_ABI
  ctsp->preorder_base_classes             = NULL;
  ctsp->primary_base_class                = NULL;
#endif /* IA64_ABI */
  ctsp->size_without_virtual_base_classes = 0;
  ctsp->alignment_without_virtual_base_classes = 1;
  ctsp->highest_virtual_function_number   = VIRTUAL_FUNCTION_NUMBER_NONE;
#if DO_IL_LOWERING
  ctsp->has_subobject_type                = FALSE;
#if IA64_ABI
  /* There are always two entries below the address point of the virtual
     table: the offset-to-top and RTTI information. */
  ctsp->next_negative_virtual_table_index = -3;
  ctsp->first_vcall_offset_index          = 0;
  ctsp->vcall_offsets                     = NULL;
#endif /* IA64_ABI */
#endif /* DO_IL_LOWERING */
  ctsp->virtual_function_info_offset      = 0;
  ctsp->virtual_function_info_base_class  = NULL;
#if DECL_MODIFIERS_IN_USE
  ctsp->decl_modifiers                    = DM_NONE;
#endif /* DECL_MODIFIERS_IN_USE */
#if MICROSOFT_EXTENSIONS_ALLOWED
  ctsp->inheritance_kind                  = (an_inheritance_kind)ihk_none;
  ctsp->inheritance_kind_is_explicit      = FALSE;
  ctsp->has_direct_property_or_event      = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  ctsp->ELF_visibility                    =
                                      (an_ELF_visibility_kind)evk_unspecified;
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if BACK_END_IS_CP_GEN_BE
  ctsp->surrounding_name_linkage_state    = (a_name_linkage_kind)nlk_none;
#endif /* BACK_END_IS_CP_GEN_BE */
#if NEAR_AND_FAR_ALLOWED
  ctsp->qualifiers                        = TQ_NONE;
#endif /* NEAR_AND_FAR_ALLOWED */
#if RECORD_HIDDEN_NAMES_IN_IL
  ctsp->hidden_names_processed            = FALSE;
  ctsp->base_class_hiding_in_progress     = FALSE;
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
  ctsp->named_in_inline_template_directive
                                          = FALSE;
  ctsp->is_va_list_tag                    = FALSE;
  ctsp->defined_in_parent_class           = FALSE;
  ctsp->has_nodiscard_attribute           = FALSE;
  ctsp->has_field_initializer             = FALSE;
  ctsp->removed_from_il                   = FALSE;
  ctsp->contains_error                    = FALSE;
  ctsp->contains_error_cached             = FALSE;
  ctsp->contains_local_type               = FALSE;
  ctsp->contains_local_type_cached        = FALSE;
  ctsp->contains_unnamed_namespace_type   = FALSE;
  ctsp->contains_unnamed_namespace_type_cached
                                          = FALSE;
  ctsp->does_not_contain_parentless_lambda_in_default_argument
                                          = FALSE;
  ctsp->does_not_contain_deprecated_or_unavailable_type
                                          = FALSE;
  ctsp->anonymous_union_kind              = (an_anonymous_union_kind)auk_none;
  ctsp->anonymous_union_field             = NULL;
  ctsp->friends                           = NULL;
#if MAINTAIN_CLASS_MEMBER_LIST
  ctsp->member_declarations               = NULL;
#endif /* MAINTAIN_CLASS_MEMBER_LIST */
  ctsp->assoc_scope                       = NULL;
  ctsp->partial_spec_template_arg_list    = NULL;
#if NEW_CAN_BE_FOLDED_INTO_CTOR
  ctsp->assoc_operator_new_routine        = NULL;
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
#if DELETE_CAN_BE_FOLDED_INTO_DTOR
  ctsp->assoc_operator_delete_routine     = NULL;
#endif /* DELETE_CAN_BE_FOLDED_INTO_DTOR */
#if DO_IL_LOWERING
  ctsp->virtual_function_table_var        = NULL;
#if IA64_ABI
  ctsp->virtual_table_table_var           = NULL;
#endif /* IA64_ABI */
  ctsp->subobject_partner                 = NULL;
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
  ctsp->promoted_local_types              = NULL;
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
  ctsp->construction_vtbls                = NULL;
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
#endif /* DO_IL_LOWERING */
}  /* clear_class_type_supplement_definition_fields */


void clear_class_type_definition_fields(a_type_ptr  class_type)
/*
Given an pointer to a class type entry, clear its fields that are only
meaningful when a definition (as opposed to just a declaration) of the
class is available (including such fields in the class type supplement).
This includes discarding the field list (if any), and making the type
incomplete (which affects the recorded size and alignment).
*/
{
  clear_class_type_supplement_definition_fields(class_type_supp(class_type));
  class_type->size = 0;
  class_type->alignment = 1;
  class_type->incomplete = TRUE;
  class_type->variant.class_struct_union.field_list = NULL;
  class_type->variant.class_struct_union.any_const_member = FALSE;
  class_type->variant.class_struct_union.any_virtual_base_classes = FALSE;
  class_type->variant.class_struct_union.abstract = FALSE;
  class_type->variant.class_struct_union.any_virtual_functions = FALSE;
  class_type->variant.class_struct_union.any_pure_virtual_functions = FALSE;
  class_type->variant.class_struct_union.
                      any_virtual_functions_including_in_base_classes = FALSE;
  class_type->variant.class_struct_union.
                               nested_class_defined_outside_of_parent = FALSE;
  class_type->variant.class_struct_union.is_empty_class = FALSE;
  class_type->variant.class_struct_union.no_proper_data = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* A delegate must be a defined ref class.  If the definition is discarded,
       it should be treated as an ordinary ref class. */
    class_type->variant.class_struct_union.is_delegate_class = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* clear_class_type_definition_fields */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void clear_ms_attribute_usage(an_ms_attribute_usage_ptr msaup)
/*
Initialize the given Microsoft attribute usage descriptor.
*/
{
  msaup->valid_on = (an_ms_attribute_target)msat_invalid;
  msaup->allow_multiple = FALSE;
  msaup->inherited = TRUE;
}  /* clear_ms_attribute_usage */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

void clear_class_type_supplement(a_class_type_supplement_ptr  ctsp)
/*
Give an pointer to a class-type-supplement entry, initialize its fields.
*/
{
  clear_class_type_supplement_definition_fields(ctsp);
#if MICROSOFT_EXTENSIONS_ALLOWED
  ctsp->uuid_string                       = NULL;
  ctsp->orig_type_kind                    = (a_type_kind)tk_error;
  ctsp->declared_assembly_visibility      = (an_assembly_visibility)av_none;
  ctsp->assembly_visibility               = (an_assembly_visibility)av_none;
  ctsp->cli_class_type_kind               =
                                         (a_cli_class_type_kind)cctk_standard;
  ctsp->is_hide_by_sig                    = FALSE;
  ctsp->is_cli_array                      = FALSE;
  ctsp->is_cli_attribute                  = FALSE;
  ctsp->is_cppcx_write_only_array         = FALSE;
  ctsp->is_cppcx_box                      = FALSE;
  ctsp->is_partial                        = FALSE;
  ctsp->has_coclass_attribute             = FALSE;
  ctsp->has_explicitly_aligned_subobject  = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING
  ctsp->compiler_generated                = FALSE;
#endif /* DO_IL_LOWERING */
  ctsp->is_initializer_list               = FALSE;
  ctsp->is_lambda_closure_class           = FALSE;
  ctsp->is_generic_lambda_closure_class   = FALSE;
  ctsp->has_lambda_conversion_function    = FALSE;
  ctsp->has_initializer_list_ctor         = FALSE;
  ctsp->has_anonymous_union_member        = FALSE;
  ctsp->defined_in_variable_initializer   = FALSE;
  ctsp->defined_in_field_initializer      = FALSE;
  ctsp->befriending_classes               = NULL;
  ctsp->assoc_template                    = NULL;
  ctsp->template_arg_list                 = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  ctsp->partial_class_bodies              = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING
#if MICROSOFT_EXTENSIONS_ALLOWED
  ctsp->uuid_variable                     = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* DO_IL_LOWERING */
  ctsp->min_template_arguments            = -1;
  ctsp->lambda_parent.routine             = NULL;
  ctsp->hash_value = 0;
#if MICROSOFT_EXTENSIONS_ALLOWED
  ctsp->corresponding_basic_type          = NULL;
  ctsp->assembly_scope_index              = 0;
  ctsp->metadata_type_def_token           = 0;
  ctsp->base_dispose_bool_routine         = NULL;
  ctsp->base_idisposable_dispose_routine  = NULL;
  ctsp->base_object_finalize_routine      = NULL;
  ctsp->invocation_type                   = NULL;
  ctsp->event_interfaces                  = NULL;
  clear_ms_attribute_usage(&ctsp->attribute_usage);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  ctsp->proxy_of_type                     = NULL;
}  /* clear_class_type_supplement */


void set_type_kind(a_type_ptr  pte,
                   a_type_kind kind)
/*
Set the kind of the type to "kind", and set the associated variant fields
to default values.
*/
{
  a_routine_type_supplement_ptr rtsp;

  pte->kind = kind;
  switch (kind) {
    case tk_error:
    case tk_unknown:
    case tk_void:
    case tk_nullptr:
    case tk_reflection:
      /* No variant fields to set. */
      break;
    case tk_integer:
      pte->variant.integer.int_kind = (an_integer_kind)ik_int;
      pte->variant.integer.explicitly_signed = FALSE;
      pte->variant.integer.enum_type = FALSE;
      pte->variant.integer.is_scoped_enum = FALSE;
      pte->variant.integer.has_explicit_enum_base = FALSE;
#if GNU_EXTENSIONS_ALLOWED
      pte->variant.integer.packed = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
      pte->variant.integer.wchar_t_type = FALSE;
      pte->variant.integer.char8_t_type = FALSE;
      pte->variant.integer.char16_t_type = FALSE;
      pte->variant.integer.char32_t_type = FALSE;
      pte->variant.integer.bool_type = FALSE;
      pte->variant.integer.originally_unnamed = FALSE;
      pte->variant.integer.is_template_enum = FALSE;
      pte->variant.integer.is_prototype_instantiation = FALSE;
      pte->variant.integer.is_nonreal = FALSE;
      pte->variant.integer.is_specialized = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
      pte->variant.integer.is_ms_instantiated_nonreal_enum = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
      pte->variant.integer.ELF_visibility =
                                       (an_ELF_visibility_kind)evk_unspecified;
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
      pte->variant.integer.microsoft_sized_int_type = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      /* Clear field of all variants for union-as-struct testing. */
      pte->variant.integer.enum_info.constant_list = NULL;
      pte->variant.integer.enum_info.affiliated_type = NULL;
      pte->variant.integer.enum_info.assoc_scope = NULL;
      pte->variant.integer.extra_info = alloc_integer_type_supplement();
      break;
#if FIXED_POINT_ALLOWED
    case tk_fixed_point:
      pte->variant.fixed_point.precision = (a_fixed_point_precision)fpp_short;
      pte->variant.fixed_point.is_unsigned = FALSE;
      pte->variant.fixed_point.is_fract_type = FALSE;
      pte->variant.fixed_point.saturating = FALSE;
      break;
#endif /* FIXED_POINT_ALLOWED */
    case tk_float:
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_complex:
    case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      pte->variant.float_kind = (a_float_kind)fk_float;
      break;
    case tk_pointer:
      pte->variant.pointer.type = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
      pte->variant.pointer.base_variable = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      pte->variant.pointer.is_reference = FALSE;
      pte->variant.pointer.is_rvalue_reference = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
      pte->variant.pointer.is_handle = FALSE;
      pte->variant.pointer.is_interior_ptr = FALSE;
      pte->variant.pointer.is_pin_ptr = FALSE;
      pte->variant.pointer.modifiers = PM_NONE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      break;
    case tk_array:
      pte->variant.array.element_type = NULL;
      pte->variant.array.qualifiers = TQ_NONE;
      pte->variant.array.is_template_dependent_size_array = FALSE;
      pte->variant.array.is_variable_size_array = FALSE;
      pte->variant.array.is_vla = FALSE;
      pte->variant.array.constant_bound_expr_in_local_expr_node_ref = FALSE;
      pte->variant.array.dep_constant_bound_expr_in_local_expr_node_ref=FALSE;
      pte->variant.array.has_assoc_vla_dimension = FALSE;
      pte->variant.array.bound_is_zero = FALSE;
      pte->variant.array.is_static = FALSE;
      pte->variant.array.variant.number_of_elements = 0;
      pte->variant.array.bound_constant = NULL;
#if UPC_EXTENSIONS_ALLOWED
      pte->variant.array.is_threads_dimension = FALSE;
#endif /* UPC_EXTENSIONS_ALLOWED */
      break;
    case tk_class:
    case tk_struct:
    case tk_union:
      pte->variant.class_struct_union.field_list = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
      pte->variant.class_struct_union.is_interface = FALSE;
      pte->variant.class_struct_union.is_interface_like = FALSE;
      pte->variant.class_struct_union.is_delegate_class = FALSE;
      pte->variant.class_struct_union.is_generic_definition = FALSE;
      pte->variant.class_struct_union.is_generic_instance = FALSE;
      pte->variant.class_struct_union.is_open_constructed_type = FALSE;
      pte->variant.class_struct_union.is_generic_constraint = FALSE;
      pte->variant.class_struct_union.is_hybrid_constraint = FALSE;
      pte->variant.class_struct_union.any_interface_constraints = FALSE;
      pte->variant.class_struct_union.unconstrained = FALSE;
      pte->variant.class_struct_union.sealed = FALSE;
#if BACK_END_IS_CP_GEN_BE
      pte->variant.class_struct_union.
                 defined_with_abstract_class_modifier = FALSE;
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      pte->variant.class_struct_union.final = FALSE;
      pte->variant.class_struct_union.any_const_member = FALSE;
      pte->variant.class_struct_union.any_volatile_member = FALSE;
      pte->variant.class_struct_union.any_mutable_member = FALSE;
      pte->variant.class_struct_union.any_virtual_base_classes = FALSE;
      pte->variant.class_struct_union.abstract = FALSE;
      pte->variant.class_struct_union.any_virtual_functions = FALSE;
      pte->variant.class_struct_union.any_pure_virtual_functions = FALSE;
      pte->variant.class_struct_union.
                 any_virtual_functions_including_in_base_classes = FALSE;
      pte->variant.class_struct_union.
                 nested_class_defined_outside_of_parent = FALSE;
      pte->variant.class_struct_union.originally_unnamed = FALSE;
      pte->variant.class_struct_union.is_nonstd_anonymous_union_type = FALSE;
      pte->variant.class_struct_union.is_template_class = FALSE;
      pte->variant.class_struct_union.is_nonreal_class = FALSE;
      pte->variant.class_struct_union.is_ms_instantiated_nonreal_class = FALSE;
      pte->variant.class_struct_union.is_prototype_instantiation = FALSE;
      pte->variant.class_struct_union.is_specialized = FALSE;
      pte->variant.class_struct_union.specialized_with_old_syntax = FALSE;
      pte->variant.class_struct_union.is_in_class_specialization = FALSE;
      pte->variant.class_struct_union.explicitly_instantiated = FALSE;
      pte->variant.class_struct_union.do_not_instantiate = FALSE;
      pte->variant.class_struct_union.proxy_class = FALSE;
#if MAINTAIN_NEEDED_FLAGS
      pte->variant.class_struct_union.definition_needed = FALSE;
      pte->variant.class_struct_union.keep_definition_in_il = FALSE;
#endif /* MAINTAIN_NEEDED_FLAGS */
      pte->variant.class_struct_union.is_empty_class = FALSE;
      pte->variant.class_struct_union.no_proper_data = FALSE;
      pte->variant.class_struct_union.has_zero_init_component = FALSE;
      pte->variant.class_struct_union.has_pointer_component = FALSE;
      pte->variant.class_struct_union.contains_flexible_array_member = FALSE;
#if GNU_EXTENSIONS_ALLOWED
      pte->variant.class_struct_union.is_transparent = FALSE;
      pte->variant.class_struct_union.is_packed = FALSE;
      pte->variant.class_struct_union.has_internal_linkage_attribute = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
      pte->variant.class_struct_union.has_operator_ampersand = FALSE;
      pte->variant.class_struct_union.virtual_functions_marked_as_required =
                                                                         FALSE;
      pte->variant.class_struct_union.copy_assignment_decl_suppressed = FALSE;
      pte->variant.class_struct_union.copy_ctor_decl_suppressed = FALSE;
      pte->variant.class_struct_union.default_ctor_decl_suppressed = FALSE;
      pte->variant.class_struct_union.dtor_decl_suppressed = FALSE;
      pte->variant.class_struct_union.inc_class_used_in_array_type = FALSE;
      pte->variant.class_struct_union.max_member_alignment = 0;
#if BACK_END_IS_CP_GEN_BE
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
      pte->variant.class_struct_union.scan_record = NULL;
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
      pte->variant.class_struct_union.do_not_suppress_templ_arg = FALSE;
#endif /* BACK_END_IS_CP_GEN_BE */
      /* Allocate the class type supplement. */
      {
        a_class_type_supplement_ptr  ctsp;
        ctsp = alloc_il_of_type(a_class_type_supplement);
        clear_class_type_supplement(ctsp);
        pte->variant.class_struct_union.extra_info = ctsp;
#if MICROSOFT_EXTENSIONS_ALLOWED
        ctsp->orig_type_kind = kind;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      }  /* if */
      break;
    case tk_routine:
      pte->variant.routine.return_type = NULL;
      pte->variant.routine.extra_info = rtsp =
                                   alloc_il_of_type(a_routine_type_supplement);
#if DO_IL_LOWERING
      pte->variant.routine.unlowered_type = NULL;
#endif /* DO_IL_LOWERING */
      rtsp->param_type_list          = NULL;
      rtsp->assoc_routine            = NULL;
      rtsp->has_ellipsis             = FALSE;
      rtsp->prototyped               = FALSE;
      rtsp->old_style_params_scanned = FALSE;
      rtsp->trailing_return_type     = FALSE;
      rtsp->lint_argsused_flag       = FALSE;
      rtsp->value_returned_by_cctor  = FALSE;
#if DO_IL_LOWERING
      rtsp->value_returned_as_parameter = FALSE;
      rtsp->return_value_parameter_follows_this = FALSE;
#endif /* DO_IL_LOWERING */
      rtsp->assoc_routine_is_ctor    = FALSE;
      rtsp->assoc_routine_is_dtor    = FALSE;
      rtsp->assoc_routine_is_lambda_body = FALSE;
      rtsp->suppress_diagnostic_on_incomplete_return_type = FALSE;
      rtsp->routine_name_linkage     = default_routine_name_linkage;
      rtsp->routine_name_linkage_is_explicit = FALSE;
      rtsp->does_not_return          = FALSE;
      rtsp->has_enable_if_attribute  = FALSE;
#if GNU_EXTENSIONS_ALLOWED
      rtsp->result_should_be_used    = FALSE;
      rtsp->is_const                 = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
      rtsp->is_variadic_instance     = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED
      rtsp->explicit_calling_convention = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED */
      rtsp->had_been_implicitly_const = FALSE;
      rtsp->is_conditionally_explicit = FALSE;
      rtsp->has_this_param           = FALSE;
      rtsp->lint_varargs_count       = NOT_LINT_VARARGS;
      rtsp->arg_pragma               = (a_pragma_kind)pk_none;
#if GNU_EXTENSIONS_ALLOWED
      rtsp->fmt_arg                  = 0;
      rtsp->format_first_subst_arg   = 0;
      rtsp->sentinel_pos             = 0;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED
      rtsp->calling_convention       = (a_calling_convention)cc_default;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED */
      rtsp->this_class               = NULL;
      rtsp->qualifiers               = TQ_NONE;
      rtsp->this_qualifiers          = TQ_NONE;
      rtsp->ref_qualifiers           = (a_ref_qualifier_kind)rqk_default;
      rtsp->prototype_scope          = NULL;
      rtsp->exception_specification  = NULL;
      break;
    case tk_typeref:
      pte->variant.typeref.type        = NULL;
      pte->variant.typeref.extra_info = alloc_typeref_type_supplement();
#if DO_IL_LOWERING
      pte->variant.typeref.orig_type   = NULL;
#endif /* DO_IL_LOWERING */
      pte->variant.typeref.kind        = trk_none;
      pte->variant.typeref.qualifiers  = TQ_NONE;
      pte->variant.typeref.predeclared = FALSE;
#if NEAR_AND_FAR_ALLOWED
      pte->variant.typeref.explicit_memory_attribute_made_implicit = FALSE;
#endif /* NEAR_AND_FAR_ALLOWED */
      pte->variant.typeref.has_variably_modified_type = FALSE;
#if LOWER_VARIABLE_LENGTH_ARRAYS
      pte->variant.typeref.is_lowered_variably_modified_type = FALSE;
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
#if BACK_END_IS_CP_GEN_BE
      pte->variant.typeref.surrounding_name_linkage_state
                                       = (a_name_linkage_kind)nlk_none;
      pte->variant.typeref.is_renamed_builtin = FALSE;
#endif /* BACK_END_IS_CP_GEN_BE */
      pte->variant.typeref.decltype_expr_not_parenthesized = FALSE;
      pte->variant.typeref.is_dependent_type_operator = FALSE;
      pte->variant.typeref.is_nonreal = FALSE;
      pte->variant.typeref.is_dependent = FALSE;
      pte->variant.typeref.is_prototype_instantiation = FALSE;
#if C99_IL_EXTENSIONS_SUPPORTED && LOWER_COMPLEX
      pte->variant.typeref.is_lowered_complex_type = FALSE;
#endif /* C99_IL_EXTENSIONS_SUPPORTED && LOWER_COMPLEX */
      pte->variant.typeref.embedded_source_sequence_entries = FALSE;
      pte->variant.typeref.added_to_record_name = FALSE;
      pte->variant.typeref.has_typename_prefix = FALSE;
      pte->variant.typeref.is_global_qualified_name = FALSE;
      pte->variant.typeref.is_intrinsic_member = FALSE;
      /* Clear size and alignment because they aren't used in typerefs. */
      pte->size = 0;
      pte->alignment = 1;
      break;
    case tk_ptr_to_member:
      pte->variant.ptr_to_member.class_of_which_a_member      = NULL;
      pte->variant.ptr_to_member.orig_class_of_which_a_member = NULL;
      pte->variant.ptr_to_member.type                         = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
      pte->variant.ptr_to_member.modifiers = PM_NONE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      break;
    case tk_template_param:
      {
        a_template_param_type_supplement_ptr	tptsp;
        pte->variant.template_param.kind =
                                       (a_template_param_type_kind)tptk_param;
        pte->variant.template_param.is_pack = FALSE;
        pte->variant.template_param.is_generic_param = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
        pte->variant.template_param.being_checked = FALSE;
        pte->variant.template_param.is_generic_function_param = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        pte->variant.template_param.is_auto_param = FALSE;
        pte->variant.template_param.is_decltype_auto = FALSE;
        pte->variant.template_param.originally_class_template_param = FALSE;
        pte->variant.template_param.is_unsigned_bit_precise_int = FALSE;
        tptsp = alloc_template_param_type_supplement();
        pte->variant.template_param.extra_info = tptsp;
        tptsp->coordinates.position = 0;
        tptsp->coordinates.depth = NO_NESTING_DEPTH;
      }
      break;
#if GNU_VECTOR_TYPES_ALLOWED
    case tk_vector:
      pte->variant.vector.element_type = NULL;
      pte->variant.vector.size_constant = NULL;
      pte->variant.vector.is_boolean_vector = FALSE;
      pte->variant.vector.kind = vk_gnu;
      break;
    case tk_scalable_vector:
      pte->variant.scalable_vector.element_type = NULL;
      pte->variant.scalable_vector.tuple_elements = 0;
      break;
    case tk_riscv_vector:
      pte->variant.riscv_vector.element_type = NULL;
      pte->variant.riscv_vector.length_multiplier = 0;
      pte->variant.riscv_vector.tuple_elements = 0;
      break;
    case tk_scalable_vector_count:
    case tk_mfp8:
    case tk_float8e4m3:
    case tk_float8e5m2:
      break;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
    default:
      unexpected_condition_str("set_type_kind: bad type kind");
  }  /* switch */
}  /* set_type_kind */


void clear_type_cached_flags(a_type_ptr  pte)
/*
Clear any state flags in the type entry that might be invalidated if the
the type is copied and modified.
*/
{
  pte->is_instantiation_dependent = FALSE;
  pte->is_instantiation_dependent_cached = FALSE;
}  /* clear_type_cached_flags */


void clear_type(a_type_ptr  pte,
                a_type_kind kind)
/*
Clear the indicated type entry, set the kind as given, and set the associated
variant fields to default values.
*/
{
  set_default_source_corresp(&pte->source_corresp);
  pte->next = NULL;
  pte->based_types = NULL;
  pte->size = 0;
  pte->alignment = 1;
  if (kind == (a_type_kind)tk_class ||
      kind == (a_type_kind)tk_struct ||
      kind == (a_type_kind)tk_union ||
      kind == (a_type_kind)tk_array ||
      kind == (a_type_kind)tk_void) {
    /* These types are incomplete by default.  (void is always incomplete.) */
    pte->incomplete = TRUE;
  } else {
    /* Other types are always complete. */
    pte->incomplete = FALSE;
  }  /* if */
  pte->used_in_exception_or_rtti = FALSE;
  pte->declared_in_function_prototype = FALSE;
  pte->is_tag_redefinition = FALSE;
  clear_type_cached_flags(pte);
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
  pte->use_cfront_transitional_nested_type_name_mangling = FALSE;
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
#if BACK_END_IS_C_GEN_BE
  pte->prototype_scope_types_if_any_promoted = FALSE;
  pte->typedef_pending = FALSE;
  pte->generated_as_empty_struct = FALSE;
#endif /* BACK_END_IS_C_GEN_BE */
  pte->has_been_defined = FALSE;
  pte->typedef_definition_has_been_put_out = FALSE;
#if BACK_END_IS_CP_GEN_BE
  pte->has_been_declared = FALSE;
  pte->definition_delayed = FALSE;
  pte->elaborated_type_specifier_needed = FALSE;
  pte->elab_type_spec_needed_in_some_scope = FALSE;
  pte->replace_by_generated_typedef = FALSE;
  pte->typedef_for_vacuous_dtor_call_put_out = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  pte->emit_microsoft_class_decl_modifiers = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  pte->explicit_specialization_suppressed = FALSE;
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
  pte->suppress_operator = FALSE;
  pte->force_typename_kwd = FALSE;
#endif /* BACK_END_IS_CP_GEN_BE */
  pte->alignment_set_explicitly = FALSE;
#if GNU_EXTENSIONS_ALLOWED
  pte->variables_are_implicitly_referenced = FALSE;
  pte->may_alias = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  pte->has_microsoft_w64_specifier = FALSE;
  pte->is_microsoft_intrinsic = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  pte->autonomous_primary_tag_decl = FALSE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  pte->is_builtin_va_list = FALSE;
  pte->is_builtin_va_list_from_cstdarg = FALSE;
#ifdef GUARD_MACRO_FOR_VA_LIST
  pte->va_list_guard_macro_was_defined = FALSE;
#endif /* ifdef GUARD_MACRO_FOR_VA_LIST */
#ifdef GUARD_MACRO2_FOR_VA_LIST
  pte->va_list_guard_macro2_was_defined = FALSE;
#endif /* ifdef GUARD_MACRO2_FOR_VA_LIST */
#if GNU_EXTENSIONS_ALLOWED
  pte->has_gnu_abi_tag_attribute = FALSE;
  pte->in_gnu_abi_tag_namespace = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
  pte->definition_pending = FALSE;
#if DO_IL_LOWERING
#if ENSURE_LOWERED_TYPE_LIST_ORDERING
  pte->process_for_ordering = FALSE;
  pte->type_processed_for_ordering = FALSE;
  pte->type_processed_as_complete_for_ordering = FALSE;
#endif /* ENSURE_LOWERED_TYPE_LIST_ORDERING */
#if LOWER_VARIABLE_LENGTH_ARRAYS
  pte->visited_for_vla_lowering = FALSE;
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
  pte->typeinfo_var = NULL;
#endif /* DO_IL_LOWERING */
  set_type_kind(pte, kind);
}  /* clear_type */


a_type_ptr alloc_type(a_type_kind kind)
/*
Allocate a new type entry in the file scope memory region and return a pointer
to it.  Set general fields, set kind to the indicated value, and set the
associated variant fields to default values.
*/
{
  a_type_ptr tp;

  db_enter(5, "alloc_type");
  tp = alloc_il_of_type(a_type);
  clear_type(tp, kind);
  db_exit();
  return tp;
}  /* alloc_type */


void set_dynamic_init_kind(a_dynamic_init_ptr  dip,
                           a_dynamic_init_kind kind)
/*
Set the kind of the indicated dynamic initialization entry to "kind", and set
the associated variant fields to default values.
*/
{
  dip->kind = kind;
  switch (kind) {
    case dik_none:
    case dik_zero:
      break;
    case dik_lambda:
    case dik_constant:
    case dik_nonconstant_aggregate:
      dip->variant.constant.ptr = NULL;
      dip->variant.constant.lambda = NULL;
      dip->variant.constant.non_constant =
                       kind == (a_dynamic_init_kind)dik_nonconstant_aggregate;
      break;
    case dik_expression:
    case dik_class_result_via_ctor:
      dip->variant.expression = NULL;
      break;
    case dik_constructor:
      dip->variant.constructor.ptr = NULL;
      dip->variant.constructor.args = NULL;
      dip->variant.constructor.is_copy_constructor_with_implied_source = FALSE;
      dip->variant.constructor.is_implicit_copy_for_copy_initialization= FALSE;
      dip->variant.constructor.value_initialization = FALSE;
      dip->variant.constructor.has_sequenced_arguments = FALSE;
      dip->variant.constructor.is_array_copy = FALSE;
      break;
    case dik_bitwise_copy:
      dip->variant.bitwise_copy.source = NULL;
      break;
    default:
      unexpected_condition_str("set_dynamic_init_kind: bad kind");
  }  /* switch */
}  /* set_dynamic_init_kind */


static void clear_dynamic_init(a_dynamic_init_ptr  dip,
                               a_dynamic_init_kind kind)
/*
Initialize a dynamic_init entry of the kind specified.
*/
{
  dip->next                          = NULL;
  dip->variable                      = NULL;
  dip->destructor                    = NULL;
  dip->lifetime                      = NULL;
  dip->next_in_destruction_list      = NULL;
  dip->init_expr_lifetime            = NULL;
  dip->static_temp                   = FALSE;
  dip->follows_an_exec_statement     = FALSE;
  dip->inside_conditional_expression = FALSE;
  dip->unordered                     = FALSE;
  dip->has_temporary_lifetime        = FALSE;
  dip->is_constructor_init           = FALSE;
  dip->is_freeing_of_storage_on_exception = FALSE;
  dip->is_array_freeing              = FALSE;
  dip->destruction_is_for_partially_constructed_aggregate = FALSE;
#if DO_IL_LOWERING
  dip->is_guard_var_for_local_static_var_init = FALSE;
#endif /* DO_IL_LOWERING */
  dip->overlaps_temps_in_inner_lifetime = FALSE;
#if DO_IL_LOWERING
  dip->included_in_slice = FALSE;
#endif /* DO_IL_LOWERING */
  dip->is_explicit_cast = FALSE;
  dip->is_compound_literal = FALSE;
  dip->is_braced_initializer = FALSE;
  dip->is_partially_initialized = FALSE;
  dip->is_result_for_class_rvalue_question_mark = FALSE;
  dip->class_rvalue_initialized_through_master_entry = FALSE;
  dip->is_result_for_comma_operator = FALSE;
  dip->is_reused_value = FALSE;
#if DO_IL_LOWERING
  dip->is_vla_deallocation = FALSE;
#if GENERATE_EH_TABLES
  dip->is_freeing_of_exception_object = FALSE;
#endif /* GENERATE_EH_TABLES */
#endif /* DO_IL_LOWERING */
  dip->is_creation_of_initializer_list_object = FALSE;
  dip->is_array_for_initializer_list_object = FALSE;
  dip->is_top_temporary_for_constexpr_reference_param = FALSE;
#if BACK_END_IS_CP_GEN_BE
  dip->suppress_init_list_arg_braces = FALSE;
  dip->suppress_template_arguments_for_cast = FALSE;
#endif /* BACK_END_IS_CP_GEN_BE */
  set_dynamic_init_kind(dip, kind);
#if DO_IL_LOWERING
  dip->destructible_entity_descr     = NULL;
  dip->init_destination              = NULL;
  dip->assoc_new                     = NULL;
#endif /* DO_IL_LOWERING */
  dip->lifetime_of_overlapping_temps = NULL;
  dip->master_entry                  = NULL;
  dip->rescan_info                   = NULL;
}  /* clear_dynamic_init */


a_dynamic_init_ptr alloc_dynamic_init(a_dynamic_init_kind kind)
/*
Allocate a dynamic initialization entry, clear it to default values, set
its kind to kind, and return a pointer to it.
*/
{
  a_dynamic_init_ptr dip;

  db_enter(5, "alloc_dynamic_init");

  dip = alloc_cil_of_type(a_dynamic_init);
  clear_dynamic_init(dip, kind);

  db_exit();
  return dip;
}  /* alloc_dynamic_init */


a_local_static_variable_init_ptr alloc_local_static_variable_init(void)
/*
Allocate a_local_static_variable_init entry, initialize its fields, and
return a pointer to it.
*/
{
  a_local_static_variable_init_ptr lsvip;

  db_enter(5, "alloc_local_static_variable_init");
  lsvip = alloc_cil_of_type(a_local_static_variable_init);
  lsvip->next = NULL;
  lsvip->variable = NULL;
  lsvip->init_kind = (an_init_kind)initk_none;
  lsvip->lifetime = NULL;
  db_exit();
  return lsvip;
}  /* alloc_local_static_variable_init */


a_vla_dimension_ptr alloc_vla_dimension(void)
/*
Allocate a_vla_dimension entry, initialize its fields, and return a
pointer to it.
*/
{
  a_vla_dimension_ptr vdp;

  db_enter(5, "alloc_vla_dimension");
  vdp = alloc_cil_of_type(a_vla_dimension);
  vdp->next = NULL;
  vdp->type = NULL;
  vdp->dimension_expr = NULL;
  vdp->original_dimension = NULL;
  vdp->in_prototype_scope = FALSE;
  vdp->has_size_statement = FALSE;
  vdp->position = null_source_position;
#if DO_IL_LOWERING
#if LOWER_VARIABLE_LENGTH_ARRAYS
  vdp->total_number_of_elements = NULL;
#else /* !LOWER_VARIABLE_LENGTH_ARRAYS */
  vdp->dimension_variable = NULL;
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
#endif /* DO_IL_LOWERING */
  db_exit();
  return vdp;
}  /* alloc_vla_dimension */


void clear_variable(a_variable_ptr vp)
/*
Clear the fields of the given variable to default values.
*/
{
  set_default_source_corresp(&vp->source_corresp);
  vp->next                        = NULL;
  vp->type                        = NULL;
  vp->variant.assoc_param_type    = NULL;
  vp->storage_class               = (a_storage_class)sc_unspecified;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  vp->declared_storage_class      = (a_storage_class)sc_unspecified;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if DECL_MODIFIERS_IN_USE
  vp->decl_modifiers              = DM_NONE;
#endif /* DECL_MODIFIERS_IN_USE */
#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED || \
    NAMED_REGISTERS_ALLOWED
  vp->asm_name_or_reg.name        = NULL;
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED || ... */
  vp->alignment                   = 0;
#if GNU_EXTENSIONS_ALLOWED
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
  vp->init_priority               = 0;
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
  vp->cleanup_routine             = NULL;
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  vp->ELF_visibility              = (an_ELF_visibility_kind)evk_unspecified;
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
  vp->is_weak                     = FALSE;
  vp->is_weakref                  = FALSE;
  vp->is_gnu_alias                = FALSE;
  vp->has_gnu_used_attribute      = FALSE;
  vp->has_gnu_abi_tag_attribute   = FALSE;
  vp->is_not_common               = FALSE;
  vp->is_common                   = FALSE;
  vp->has_internal_linkage_attribute = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
  vp->asm_name_is_valid           = TRUE;
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */
#if NAMED_REGISTERS_ALLOWED
  vp->has_named_register_storage_class = FALSE;
#endif /* NAMED_REGISTERS_ALLOWED */
  vp->used                        = FALSE;
  vp->address_taken               = FALSE;
  vp->is_parameter                = FALSE;
  vp->is_struct_binding           = FALSE;
  vp->is_struct_binding_container = FALSE;
  vp->declared_using_type_without_linkage
                                  = FALSE;
  vp->is_pack                     = FALSE;
  vp->is_pack_element             = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  vp->is_initonly                 = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  vp->is_enhanced_for_iterator    = FALSE;
  vp->initializer_in_class        = FALSE;
  vp->constant_valued             = FALSE;
  vp->is_immutable                = FALSE;
  vp->is_thread_local             = FALSE;
  vp->extends_lifetime            = FALSE;
  vp->is_template_param_object    = FALSE;
  vp->compiler_generated          = FALSE;
  vp->is_in_class_specialization  = FALSE;
  vp->init_kind                   = (an_init_kind)initk_none;
  /* Clear field of all variants for union-as-struct testing. */
  vp->initializer.constant        = NULL;
  vp->initializer.dynamic         = NULL;
  vp->initializer.bound_expr      = NULL;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  vp->initializer_range           = null_source_range;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  vp->entities_defined_in_initializer = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  vp->property_or_event_descr     = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  vp->template_info               = NULL;
#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
  vp->section                     = NULL;
#endif /* GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  vp->aliased_variable            = NULL;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING
  vp->comdat_group                = NULL;
  vp->vla_element_count_variable  = NULL;
#endif /* DO_IL_LOWERING */
  vp->referenced_non_locally      = FALSE;
  vp->modified_within_try_block   = FALSE;
  vp->is_template_variable
                                  = FALSE;
  vp->is_prototype_instantiation  = FALSE;
  vp->is_nonreal                  = FALSE;
  vp->is_specialized              = FALSE;
  vp->specialized_with_old_syntax = FALSE;
  vp->explicit_instantiation      = FALSE;
  vp->class_explicitly_instantiated = FALSE;
  vp->explicit_do_not_instantiate = FALSE;
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  vp->can_be_instantiated         = FALSE;
  vp->do_not_instantiate          = FALSE;
  vp->instance_required           = FALSE;
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
  vp->param_value_has_been_changed= FALSE;
#if MINIMAL_INLINING
  vp->param_used_as_lvalue        = FALSE;
#endif /* MINIMAL_INLINING */
  vp->param_used_more_than_once   = FALSE;
  vp->is_handler_param            = FALSE;
  vp->is_this_parameter           = FALSE;
  vp->is_anonymous_parent_object  = FALSE;
  vp->is_member_constant          = FALSE;
  vp->is_constexpr                = FALSE;
  vp->declared_constinit          = FALSE;
  vp->is_inline                   = FALSE;
  vp->on_inline_variable_list     = FALSE;
  vp->suppress_inline_definition  = FALSE;
#if INSTANTIATE_INLINE_VARIABLES
  vp->inline_instance_required    = FALSE;
#endif /* INSTANTIATE_INLINE_VARIABLES */
  vp->superseded_external         = FALSE;
  vp->has_variably_modified_type  = FALSE;
  vp->is_vla                      = FALSE;
#if DO_IL_LOWERING
  vp->initialization_rewritten_as_assignment = FALSE;
#if MINIMAL_INLINING
  vp->is_temp_for_unmodified_inlined_param = FALSE;
  vp->is_temp_for_constructor_this_inlined_param = FALSE;
#endif /* MINIMAL INLINING */
  vp->promoted_local_static_init  = FALSE;
  vp->promoted_local_static       = FALSE;
  vp->is_optional_vtable          = FALSE;
  vp->vtable_defined              = FALSE;
  vp->lowering_generated          = FALSE;
#endif /* DO_IL_LOWERING */
  vp->is_compound_literal         = FALSE;
  vp->has_explicit_initializer      = FALSE;
  vp->has_parenthesized_initializer = FALSE;
  vp->has_direct_braced_initializer = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED
  vp->has_flexible_array_initializer = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED */
  vp->declared_with_auto_type_specifier = FALSE;
  vp->declared_with_decltype_auto = FALSE;
  vp->declared_with_class_template_placeholder = FALSE;
#if BACK_END_IS_CP_GEN_BE
  vp->declaration_has_been_put_out = FALSE;
  vp->definition_has_been_put_out = FALSE;
#endif /* BACK_END_IS_CP_GEN_BE */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  vp->embedded_source_sequence_entries = FALSE;
  vp->declared_type               = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if ONE_INSTANTIATION_PER_OBJECT
  vp->instantiation_needed_bit_number = 0;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if MINIMAL_INLINING
  vp->remapping_for_inlining      = NULL;
#endif /* MINIMAL_INLINING */
#if SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
  vp->init_routine.dynamic_init_routine = NULL;
#endif /* SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */
#if USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES
  vp->init_routine.thread.init_routine = NULL;
  vp->init_routine.thread.wrapper = NULL;
#endif /* USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES */
#if MAINTAIN_NEEDED_FLAGS && !GENERATE_EH_TABLES
  vp->eff_class_typeinfo_var = NULL;
#endif /* MAINTAIN_NEEDED_FLAGS && !GENERATE_EH_TABLES */
}  /* clear_variable */


a_variable_template_info_ptr alloc_variable_template_info(void)
/*
Allocate a variable template info entry, clear it to default values, and
return a pointer to it.
*/
{
  a_variable_template_info_ptr	vtip;

  /* The associated variable is template-based, which means it must have
     been allocated in the file scope memory region. */
  vtip = alloc_il_of_type(a_variable_template_info);
  vtip->template_arg_list = NULL;
  vtip->partial_spec_template_arg_list = NULL;
  vtip->assoc_template = NULL;
  return vtip;
}  /* alloc_variable_template_info */


a_variable_ptr alloc_variable(a_storage_class  storage_class)
/*
Allocate a variable entry, clear it to default values, and return a pointer
to it.
*/
{
  a_variable_ptr vp;

  db_enter(5, "alloc_variable");

  if (is_static_or_thread_storage_duration_storage_class(storage_class)) {
    /* Variable that will have static or thread storage duration should always
       be allocated in the file scope memory region. */
    vp = alloc_il_of_type(a_variable);
  } else {
    vp = alloc_cil_of_type(a_variable);
  }  /* if */
  clear_variable(vp);
  vp->storage_class = storage_class;
  db_exit();
  return vp;
}  /* alloc_variable */


a_field_ptr alloc_field(void)
/*
Allocate a field entry, clear it to default values, and return a pointer
to it.
*/
{
  a_field_ptr fp;

  db_enter(5, "alloc_field");

  fp = alloc_il_of_type(a_field);
  set_default_source_corresp(&fp->source_corresp);
  fp->next                 = NULL;
  fp->type                 = NULL;
  fp->offset               = 0;
  fp->offset_bit_remainder = 0;
  fp->bit_size             = 0;
#if RECORD_BIT_FIELD_CONTAINER_OFFSETS_IN_IL
  fp->offset_in_container  = 0;
#endif /* RECORD_BIT_FIELD_CONTAINER_OFFSETS_IN_IL */
  fp->alignment            = 0;
#if GNU_EXTENSIONS_ALLOWED
  fp->is_packed            = 0;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if IA64_ABI
  fp->offset_is_set        = FALSE;
#endif /* IA64_ABI */
  fp->is_bit_field         = FALSE;
  fp->bit_field_is_signed  = FALSE;
  fp->is_anonymous_parent_object = FALSE;
  fp->is_mutable           = FALSE;
  fp->compiler_generated   = FALSE;
  fp->is_init_capture      = FALSE;
  fp->is_captured_this     = FALSE;
  fp->is_captured_pack_element = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  fp->is_initonly          = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  fp->vla_treated_as_zero_length_array = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING
  fp->is_lowered_base_class = FALSE;
  fp->class_subobject_with_tail_padding = FALSE;
#endif /* DO_IL_LOWERING */
  fp->has_initializer      = FALSE;
  fp->init_is_ctor_dependent = FALSE;
  fp->has_direct_braced_initializer = FALSE;
  fp->has_nonconstant_initializer = FALSE;
  fp->bit_size_constant_expr_in_local_expr_node_ref = FALSE;
  fp->has_no_unique_address_attribute = FALSE;
  fp->is_optimized_empty_class = FALSE;
  fp->initializer          = NULL;
  fp->entities_defined_in_initializer = NULL;
  fp->bit_size_constant    = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  fp->property_or_event_descr = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  fp->declared_bit_size        = 0;
#if BACK_END_IS_C_GEN_BE
  fp->bit_field_alignment_type = NULL;
#endif /* BACK_END_IS_C_GEN_BE */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  fp->initializer_range           = null_source_range;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

  db_exit();
  return fp;
}  /* alloc_field */


an_exception_specification_ptr alloc_exception_specification(void)
/*
Allocate an exception specification entry, clear it to default values, and
return a pointer to it.  The entry is allocated in the file scope memory
region.
*/
{
  an_exception_specification_ptr  esp;

  esp = alloc_il_of_type(an_exception_specification);
  esp->is_noexcept = FALSE;
  esp->indeterminate = FALSE;
  esp->throw_any = FALSE;
  esp->compiler_generated = FALSE;
  esp->from_attribute = FALSE;
  esp->arg_cached = FALSE;
  esp->copy_from_prototype = FALSE;
  esp->variant.exception_specification_type_list = NULL;
  esp->variant.token_cache = NULL;
  esp->variant.noexcept_arg = NULL;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  esp->source_range = null_source_range;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  return esp;
}  /* alloc_exception_specification */


an_exception_specification_type_ptr alloc_exception_specification_type(void)
/*
Allocate an exception specification type entry, clear it to default values,
and return a pointer to it.  The entry is allocated in the file scope memory
region.
*/
{
  an_exception_specification_type_ptr  estp;

  estp = alloc_il_of_type(an_exception_specification_type);
  estp->next = NULL;
  estp->type = NULL;
  estp->redundant = FALSE;
  estp->is_pack_expansion = FALSE;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  estp->source_position = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

  return estp;
}  /* alloc_exception_specification_type */


void set_routine_special_kind(a_routine_ptr           rp,
                              a_special_function_kind special_kind)
/*
Set the special_kind field of the indicated routine to the indicated
value.  Also clear related variant fields to default values.
*/
{
  rp->special_kind = special_kind;
  switch (special_kind) {
    case sfk_conversion:
    case sfk_udl_operator:
      break;
    case sfk_operator:
      rp->variant.opname_kind = (an_opname_kind)onk_none;
      break;
    case sfk_none:
#if BUILTIN_FUNCTIONS_ENABLED
      rp->variant.builtin_function_kind = (a_builtin_function_kind)bfk_none;
#endif /* BUILTIN_FUNCTIONS_ENABLED */
      break;
    case sfk_constructor:
    case sfk_destructor:
#if IA64_ABI && DO_IL_LOWERING
      rp->variant.ctor_dtor.alternate_entry_points = 
                                                (a_routine_list_entry_ptr)NULL;
      rp->variant.ctor_dtor.base_name_offset = 0;
#endif /* IA64_ABI && DO_IL_LOWERING */
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case sfk_static_constructor:
    case sfk_finalizer:
    case sfk_idisposable_dispose:
    case sfk_dispose_bool:
    case sfk_object_finalize:
      check_assertion(cli_or_cx_enabled);
      break;
    case sfk_property_get:
    case sfk_property_set:
    case sfk_event_add:
    case sfk_event_remove:
    case sfk_event_raise:
      rp->variant.property_or_event_descr = NULL;
      check_assertion(cli_or_cx_enabled);
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case sfk_lambda_entry_point:
      rp->variant.lambda_call_operator = NULL;
      break;
    case sfk_deduction_guide:
      rp->variant.class_template = NULL;
      break;
    default:
      unexpected_condition_str("set_routine_special_kind: bad kind");
  }  /* switch */
}  /* set_routine_special_kind */


a_routine_ptr alloc_routine(void)
/*
Allocate a routine entry, clear it to default values, and return a pointer
to it.  The entry is allocated in the file scope memory region.
*/
{
  a_routine_ptr rp;

  db_enter(5, "alloc_routine");

  rp = alloc_il_of_type(a_routine);
  set_default_source_corresp(&rp->source_corresp);
  rp->next                        = NULL;
  rp->type                        = NULL;
  rp->function_def_number         = NULL_function_def_number;
  rp->memory_region               = NULL_region_number;
  rp->hash_value                  = 0;
  rp->storage_class               = (a_storage_class)sc_unspecified;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  rp->declared_storage_class      = (a_storage_class)sc_unspecified;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  set_routine_special_kind(rp, (a_special_function_kind)sfk_none);
  rp->address_taken               = FALSE;
  rp->is_virtual                  = FALSE;
  rp->overrides_base_member       = FALSE;
  rp->pure_virtual                = FALSE;
  rp->final                       = FALSE;
  rp->override                    = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  rp->abstract                    = FALSE;
  rp->sealed                      = FALSE;
  rp->new_member                  = FALSE;
  rp->interface_slot              = FALSE;
  rp->definition_cannot_be_generated = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  rp->covariant_return_virtual_override
                                  = FALSE;
  rp->is_inline                   = FALSE;
  rp->is_declared_constexpr       = FALSE;
  rp->is_constexpr                = FALSE;
  rp->is_consteval                = FALSE;
  rp->is_constexpr_intrinsic      = FALSE;
  rp->compiler_generated          = FALSE;
  rp->defined                     = FALSE;
  rp->called                      = FALSE;
  rp->is_explicit_constructor     = FALSE;
  rp->is_explicit_conversion_function = FALSE;
  rp->is_trivial_default_constructor = FALSE;
  rp->is_trivial_copy_function    = FALSE;
  rp->is_trivial_destructor       = FALSE;
  rp->is_initializer_list_ctor    = FALSE;
  rp->is_delegating_ctor          = FALSE;
  rp->is_inheriting_ctor          = FALSE;
  rp->inherits_virtually          = FALSE;
  rp->is_deduction_guide_from_inheriting_ctor = FALSE;
#if ASSIGNMENT_TO_THIS_ALLOWED
  rp->assignment_to_this_done     = FALSE;
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
  rp->is_template_function        = FALSE;
  rp->is_specialized              = FALSE;
  rp->specialized_with_old_syntax = FALSE;
  rp->is_prototype_instantiation  = FALSE;
  rp->explicit_instantiation      = FALSE;
  rp->class_explicitly_instantiated = FALSE;
  rp->explicit_do_not_instantiate = FALSE;
  rp->has_nodiscard_attribute     = FALSE;
  rp->never_throws                = FALSE;
  rp->is_in_class_specialization  = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  rp->declared_only_as_friend     = FALSE;
  rp->explicit_extern_inline      = FALSE;
  rp->direct_linkage_specifier_on_nondef_decl = FALSE;
  rp->is_reverse_conversion_function
                                  = FALSE;
  rp->is_generic_definition       = FALSE;
  rp->is_generic_instance         = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  rp->ELF_visibility              = (an_ELF_visibility_kind)evk_unspecified;
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
  rp->is_initialization_routine   = FALSE;
  rp->is_finalization_routine     = FALSE;
  rp->is_weak                     = FALSE;
  rp->is_weakref                  = FALSE;
  rp->is_gnu_alias                = FALSE;
  rp->is_ifunc                    = FALSE;
  rp->has_gnu_used_attribute      = FALSE;
  rp->has_gnu_abi_tag_attribute   = FALSE;
  rp->in_gnu_abi_tag_namespace    = FALSE;
  rp->implicit_abi_tags_added     = FALSE;
  rp->allocates_memory            = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
  rp->never_inline                = FALSE;
  rp->is_pure                     = FALSE;
#if GNU_NAKED_ATTRIBUTE_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
  rp->is_naked                    = FALSE;
#endif /* GNU_NAKED_ATTRIBUTE_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  rp->no_instrument_function      = FALSE;
  rp->no_check_memory_usage       = FALSE;
  rp->always_inline               = FALSE;
  rp->gnu_c89_inline              = FALSE;
  rp->implicit_alias              = FALSE;
  rp->has_internal_linkage_attribute = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  rp->can_be_instantiated         = FALSE;
  rp->do_not_instantiate          = FALSE;
  rp->instance_required           = FALSE;
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
  rp->contains_try_block          = FALSE;
  rp->contains_local_class_type   = FALSE;
  rp->superseded_external         = FALSE;
  rp->defined_in_friend_decl      = FALSE;
  rp->defined_outside_of_parent   = FALSE;
#if MINIMAL_INLINING
  rp->inlinable                   = FALSE;
#endif /* MINIMAL_INLINING */
#if MAINTAIN_NEEDED_FLAGS
  rp->definition_needed           = FALSE;
  rp->keep_definition_in_il       = FALSE;
#endif /* MAINTAIN_NEEDED_FLAGS */
  rp->expl_template_arg_list_used = FALSE;
#if BACK_END_IS_CP_GEN_BE
  rp->surrounding_name_linkage_state
                                  = (a_name_linkage_kind)nlk_none;
  rp->definition_C_name_linkage_specified
                                  = FALSE;
  rp->definition_has_direct_linkage_specifier
                                  = FALSE;
#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  rp->has_been_defined            = FALSE;
  rp->evaluated_in_interpreter    = FALSE;
  rp->suppress_explicit_specialization
                                  = FALSE;
  rp->need_for_template_args_determined
                                  = FALSE;
  rp->template_args_required      = FALSE;
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* BACK_END_IS_CP_GEN_BE */
  rp->definition_for_inlining_only = FALSE;
#if INSTANTIATE_EXTERN_INLINE
  rp->inline_instance_required    = FALSE;
#endif /* INSTANTIATE_EXTERN_INLINE */
  rp->suppress_inline_body        = FALSE;
  rp->on_inline_function_list     = FALSE;
  rp->need_out_of_line_copy       = FALSE;
  rp->fp_contract                 = (a_stdc_pragma_value)stdc_pv_none;
  rp->fenv_access                 = (a_stdc_pragma_value)stdc_pv_none;
  rp->cx_limited_range            = (a_stdc_pragma_value)stdc_pv_none;
#if FIXED_POINT_ALLOWED
  rp->fx_full_precision           = (a_stdc_pragma_value)stdc_pv_none;
  rp->fx_fract_overflow           = (a_stdc_pragma_value)stdc_pv_none;
  rp->fx_accum_overflow           = (a_stdc_pragma_value)stdc_pv_none;
#endif /* FIXED_POINT_ALLOWED */
#if UPC_EXTENSIONS_ALLOWED
  rp->upc_access_method = (a_upc_access_method)upc_access_unspecified;
#endif /* UPC_EXTENSIONS_ALLOWED */
  rp->contains_statement_expression = FALSE;
#if IA64_ABI
  rp->inline_in_class_definition  = FALSE;
#endif /* IA64_ABI */
#if DO_IL_LOWERING && IA64_ABI
  rp->use_comdat                  = FALSE;
  rp->ctor_dtor_kind              = (a_ctor_or_dtor_kind)cdk_none;
  rp->is_alias_entry              = FALSE;
#endif /* DO_IL_LOWERING && IA64_ABI */
#if DO_IL_LOWERING
  rp->lowering_delayed_on_nested_function = FALSE;
  rp->has_no_effect               = FALSE;
#endif /* DO_IL_LOWERING */
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
  rp->statics_have_been_promoted  = FALSE;
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
  rp->is_lambda_body              = FALSE;
  rp->declared_using_type_without_linkage
                                  = FALSE;
  rp->is_defaulted                = FALSE;
  rp->is_deleted                  = FALSE;
  rp->contains_local_static_variable = FALSE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  rp->embedded_source_sequence_entries = FALSE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  rp->considered_decider_function_at_some_point = FALSE;
  rp->is_raw_literal_operator     = FALSE;
#if USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES
  rp->is_tls_init_alias           = FALSE;
#endif /* USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES */
  rp->is_tls_init_routine         = FALSE;
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
  rp->has_ctor_priority           = FALSE;
  rp->has_dtor_priority           = FALSE;
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
  rp->has_deducible_return_type   = FALSE;
  rp->has_deduced_return_type     = FALSE;
  rp->contains_generic_lambda     = FALSE;
  rp->is_coroutine                = FALSE;
  rp->is_top_level_in_mem_region  = FALSE;
  rp->friend_defined_in_instantiation = FALSE;
  rp->is_ineligible               = FALSE;
  rp->has_pass_object_size_attr   = FALSE;
  rp->from_injected_tokens        = FALSE;
#if DECL_MODIFIERS_IN_USE
  rp->decl_modifiers              = DM_NONE;
#endif /* DECL_MODIFIERS_IN_USE */
  rp->trailing_requires_clause    = NULL;
  rp->number.virtual_function     = VIRTUAL_FUNCTION_NUMBER_NONE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  rp->overridden_functions        = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  rp->friends_or_originator.befriending_classes
                                  = NULL;
  rp->friends_or_originator.inherited_routine
                                  = NULL;
  rp->template_arg_list           = NULL;
  rp->assoc_template              = NULL;
#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
  rp->gnu_extra_info              = NULL;
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  rp->declared_type               = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
  rp->overriding_function_for_wrapper = NULL;
  rp->overridden_function_for_wrapper = NULL;
#if IA64_ABI
  rp->delta                       = 0;
  rp->vcall_index                 = 0;
  rp->return_delta                = 0;
  rp->vbase_index                 = 0;
#endif /* IA64_ABI */
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
#if DO_IL_LOWERING && IA64_ABI
  rp->primary_ctor_or_dtor        = NULL;
#endif /* DO_IL_LOWERING && IA64_ABI */
#if ONE_INSTANTIATION_PER_OBJECT
  rp->instantiation_needed_bit_number = 0;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  rp->routine_fixup = NULL;
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED && DO_IL_LOWERING
  rp->init_priority               = 0;
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED && DO_IL_LOWERING */
  rp->generating_using_decl = NULL;
  db_exit();
  return rp;
}  /* alloc_routine */

#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED

a_gnu_routine_supplement_ptr alloc_gnu_supplement_for_routine(a_routine_ptr rp)
/*
Allocate and return a_gnu_routine_supplement structure for rp (which should not
already have one).  The structure is not allocated when a_routine is
allocated and is populated only when a need for the structure arises.  The
entry is allocated in the file scope memory region.  See
ensure_gnu_routine_supp for the typical invocation.
*/
{
  a_gnu_routine_supplement_ptr grsp;
  check_assertion(rp->gnu_extra_info == NULL);
  rp->gnu_extra_info = grsp = alloc_il_of_type(a_gnu_routine_supplement);
  grsp->section              = NULL;
  grsp->aliased_routine      = NULL;
#if LOWER_IFUNC
  grsp->resolver_var         = NULL;
#endif /* LOWER_IFUNC */
  grsp->inline_partner       = NULL;
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
  grsp->ctor_priority        = 0;
  grsp->dtor_priority        = 0;
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
  grsp->asm_name             = NULL;
#if GNU_FUNCTION_MULTIVERSIONING
  grsp->is_representative    = FALSE;
  grsp->is_target_specific_version  = FALSE;
#if USE_X86_FUNCTION_MULTIVERSIONING
  grsp->mv_resolver_required = FALSE;
#endif /* USE_X86_FUNCTION_MULTIVERSIONING */
  grsp->mv_info.representative.targeted_versions = NULL;
  grsp->mv_info.targeted_version.representative = NULL;
#if USE_X86_FUNCTION_MULTIVERSIONING
  grsp->mv_info.targeted_version.target_bitset = 0;
#endif /* USE_X86_FUNCTION_MULTIVERSIONING */
#endif /* GNU_FUNCTION_MULTIVERSIONING */
  return rp->gnu_extra_info;
}  /* alloc_gnu_supplement_for_routine */

#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */

an_asm_entry_ptr alloc_asm_entry(void)
/*
Allocate an asm entry, clear it to default values, and return a pointer
to it.
*/
{
  an_asm_entry_ptr ap;

  db_enter(5, "alloc_asm_entry");
  ap = alloc_cil_of_type(an_asm_entry);
  set_default_source_corresp(&ap->source_corresp);
  ap->next = NULL;
  ap->asm_string = NULL;
#if GNU_EXTENSIONS_ALLOWED
  ap->gnu_asm_form = FALSE;
  ap->is_volatile = FALSE;
  ap->has_volatile_keyword = FALSE;
  ap->has_inline_keyword = FALSE;
  ap->is_asm_goto = FALSE;
  ap->operands = NULL;
  ap->clobbers = NULL;
  ap->labels = NULL;
#if !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
  ap->number_of_constraints = 0;
#endif /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
#endif /* GNU_EXTENSIONS_ALLOWED */
  db_exit();
  return ap;
}  /* alloc_asm_entry */

#if ASM_SUPPORT_NEEDED

char *alloc_asm_function_body(sizeof_t  len)
/*
Allocate space for an asm function body and return a pointer to it.
*/
{
#if DEBUG
  asm_function_body_space_allocated += (unsigned long)len;
#endif /* DEBUG */
  return (char *)alloc_cil(len);
}  /* alloc_asm_function_body */

#endif /* ASM_SUPPORT_NEEDED */

#if GNU_EXTENSIONS_ALLOWED
#if !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS

an_asm_operand_constraint_ptr alloc_asm_operand_constraint(
                                            an_asm_operand_constraint_kind ck)
/*
Allocate space for an asm operand constraint and return a pointer to it.
*/
{
  an_asm_operand_constraint_ptr aocp;

  aocp = alloc_cil_of_type(an_asm_operand_constraint);
  aocp->kind = ck;
#if GNU_X86_ASM_EXTENSIONS_ALLOWED
  aocp->cond_code = NULL;
#endif /* GNU_X86_ASM_EXTENSIONS_ALLOWED */
  aocp->next = NULL;

  return aocp;
}  /* alloc_asm_operand_constraint */

#endif /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */

an_asm_operand_ptr alloc_asm_operand(void)
/*
Allocate space for an asm operand and return a pointer to it.
*/
{
  an_asm_operand_ptr  aop = alloc_cil_of_type(an_asm_operand);

  aop->next = NULL;
  aop->name = NULL;
#if RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
  aop->is_output_operand = FALSE;
  aop->constraints_string = NULL;
#else /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
  aop->constraints = NULL;
  aop->modifiers = (an_asm_operand_modifier)aom_invalid;
#endif /* RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
  aop->position = null_source_position;
  aop->expression = NULL;
  return aop;
}  /* alloc_asm_operand */


a_named_register_list_ptr alloc_named_register_list(void)
/*
Allocate space for a named register list and return a pointer to
it.
*/
{
  a_named_register_list_ptr  nrl = alloc_cil_of_type(a_named_register_list);

  nrl->next = NULL;
  nrl->reg = (a_named_register)anr_invalid;
  return nrl;
}  /* alloc_named_register_list */


a_label_list_ptr alloc_label_list(void)
/*
Allocate space for a label list and return a pointer to it.
*/
{
  a_label_list_ptr  ll = alloc_cil_of_type(a_label_list);

  ll->next = NULL;
  ll->label = NULL;
  return ll;
}  /* alloc_label_list */

#endif /* GNU_EXTENSIONS_ALLOWED */

a_label_ptr alloc_label(void)
/*
Allocate a label entry, clear it to default values, and return a pointer
to it.
*/
{
  a_label_ptr lp;

  db_enter(5, "alloc_label");

  /* Labels should always be allocated in a function scope memory region. */
  check_assertion(curr_il_region_number != file_scope_region_number);
  lp = alloc_cil_of_type(a_label);
  set_default_source_corresp(&lp->source_corresp);
  lp->source_corresp.is_local_to_function = TRUE;
  lp->next = NULL;
  lp->reachable_by_fall_through = TRUE;
  lp->break_label = FALSE;
  lp->switch_break_label = FALSE;
  lp->continue_label = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  lp->leave_label = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  lp->address_taken = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  lp->locally_declared = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
  lp->is_likely = FALSE;
  lp->is_unlikely = FALSE;
  lp->exec_stmt = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  lp->num_microsoft_trys_inside_of = 0;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

  db_exit();
  return lp;
}  /* alloc_label */


a_local_expr_node_ref_ptr alloc_local_expr_node_ref(void)
/*
Allocate an entry to represent an outside reference to a local expression
node, initialize it, and return a pointer to it.
*/
{
  a_local_expr_node_ref_ptr  ptr;

  ptr = alloc_cil_of_type(a_local_expr_node_ref);
  ptr->next = NULL;
  ptr->expr = NULL;
  ptr->kind = (a_local_expr_node_ref_kind)lerk_none;
  clear_tagged_ptr(ptr->referrer);
  return ptr;
}  /* alloc_local_expr_node_ref */


a_token_sequence_entry* alloc_token_sequence_entry(void)
/*
Allocate an entry to represent a token in a token sequence, initialize it, and
return a pointer to it.
*/
{
  a_token_sequence_entry  *tsep;

  tsep = alloc_il_of_type(a_token_sequence_entry);
  memzero((char*)tsep, sizeof(*tsep));
  return tsep;
}  /* alloc_token_sequence_entry */


a_token_sequence* alloc_token_sequence(void)
/*
Allocate an entry to represent a token sequence, initialize it, and return a
pointer to it.
*/
{
  a_token_sequence  *tsp;

  tsp = alloc_il_of_type(a_token_sequence);
  memzero((char*)tsp, sizeof(*tsp));
  return tsp;
}  /* alloc_token_sequence */

#endif /* !STANDALONE_UTILITY_PROGRAM */

void set_expr_node_kind(an_expr_node_ptr  node,
                        an_expr_node_kind kind)
/*
Set the kind of the indicated expression node.  Also set associated variant
fields to default values.
*/
{
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_gcnew_supplement_ptr      gnsp;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

  node->kind = kind;
  node->orig_lvalue_type = NULL;
  switch (kind) {
    case enk_error:
    case enk_address_of_ellipsis:
      /* No variant fields. */
      break;
    case enk_operation:
      node->variant.operation.kind = (an_expr_operator_kind)eok_last;
      node->variant.operation.type_kind = (a_type_kind)tk_unknown;
      node->variant.operation.returns_lvalue_instead_of_usual_rvalue = FALSE;
      node->variant.operation.is_reinterpret_cast = FALSE;
      node->variant.operation.is_reinterpret_like_cast = FALSE;
      node->variant.operation.is_const_cast = FALSE;
      node->variant.operation.is_reference_cast = FALSE;
      node->variant.operation.is_rvalue_reference_cast = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
      node->variant.operation.is_tracking_reference_cast = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      node->variant.operation.implicit_in_member_naming = FALSE;
      node->variant.operation.implicit_step_of_explicit_cast = FALSE;
      node->variant.operation.is_conversion_call = FALSE;
      node->variant.operation.arg_dependent_lookup_suppressed_on_call = FALSE;
      node->variant.operation.call_with_qualified_function_name = FALSE;
#if BACK_END_IS_CP_GEN_BE
      node->variant.operation.only_found_through_arg_dependent_lookup = FALSE;
      node->variant.operation.called_through_address_of_overload_set = FALSE;
#endif /* BACK_END_IS_CP_GEN_BE */
      node->variant.operation.call_uses_operator_syntax = FALSE;
#if GNU_EXTENSIONS_ALLOWED
      node->variant.operation.is_gnu_two_operand_question_mark = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
      node->variant.operation.pointer_operand_is_second = FALSE;
      node->variant.operation.is_virtual_call = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
      node->variant.operation.rewritten_property_reference_kind =
                                (a_rewritten_property_reference_kind)rprk_none;
      node->variant.operation.requires_runtime_cast_check = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if BACK_END_IS_C_GEN_BE
      node->variant.operation.has_deferred_ampersand = FALSE;
#endif /* BACK_END_IS_C_GEN_BE */
      node->variant.operation.eval_left_to_right = FALSE;
      node->variant.operation.eval_right_to_left = FALSE;
      node->variant.operation.is_consteval_call = FALSE;
#if BACK_END_IS_CP_GEN_BE
      node->variant.operation.suppress_top_level_parens = FALSE;
#endif /* BACK_END_IS_CP_GEN_BE */
      node->variant.operation.operands = NULL;
      break;
    case enk_constant:
      node->variant.constant.ptr = NULL;
      node->variant.constant.name_reference = NULL;
      break;
    case enk_variable:
      node->variant.variable.ptr = NULL;
      node->variant.variable.name_reference = NULL;
      break;
    case enk_routine:
      node->variant.routine.ptr = NULL;
      node->variant.routine.name_reference = NULL;
      break;
    case enk_field:
      node->variant.field.ptr = NULL;
      node->variant.field.name_reference = NULL;
      break;
    case enk_lambda:
      node->variant.init.dynamic_init = NULL;
      node->variant.init.source.lambda = NULL;
      break;
    case enk_temp_init:
      node->variant.init.dynamic_init = NULL;
      node->variant.init.source.type = NULL;
      break;
    case enk_new_delete:
#if STANDALONE_UTILITY_PROGRAM
      /* Allocation of IL entries is not available in standalone utility
         programs. */
      node->variant.new_delete = NULL;
#else /* !STANDALONE_UTILITY_PROGRAM */
      /* Allocate the supplement for new/delete. */
      { a_new_delete_supplement_ptr ndsp =
                                    alloc_cil_of_type(a_new_delete_supplement);
        node->variant.new_delete = ndsp;
        ndsp->is_new                          = TRUE;
        ndsp->placement_new                   = FALSE;
        ndsp->aligned_version                 = FALSE;
        ndsp->array_delete                    = FALSE;
        ndsp->global_new_or_delete            = FALSE;
        ndsp->has_new_initializer             = FALSE;
        ndsp->new_initializer_is_brace_enclosed = FALSE;
        ndsp->new_initializer_is_paren_aggr_init = FALSE;
        ndsp->deducible_type                  = FALSE;
        ndsp->parenthesized_type_id           = FALSE;
        ndsp->type                            = NULL;
        ndsp->routine                         = NULL;
        ndsp->arg                             = NULL;
        ndsp->dynamic_init                    = NULL;
        ndsp->freeing_of_storage_on_exception = NULL;
        ndsp->number_of_elements              = NULL;
      }
#endif /* STANDALONE_UTILITY_PROGRAM */
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case enk_gcnew:
#if STANDALONE_UTILITY_PROGRAM
      /* Allocation of IL entries is not available in standalone utility
         programs. */
      node->variant.gcnew_info = NULL;
#else /* !STANDALONE_UTILITY_PROGRAM */
      gnsp = alloc_cil_of_type(a_gcnew_supplement);
      node->variant.gcnew_info = gnsp;
      gnsp->has_new_initializer         = FALSE;
      gnsp->is_cli_array                = FALSE;
      gnsp->type                        = NULL;
      gnsp->cli_array_dimension_lengths = NULL;
      gnsp->dynamic_init                = NULL;
#endif /* STANDALONE_UTILITY_PROGRAM */
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case enk_throw:
#if STANDALONE_UTILITY_PROGRAM
      /* Allocation of IL entries is not available in standalone utility
         programs. */
      node->variant.throw_info = NULL;
#else /* !STANDALONE_UTILITY_PROGRAM */
      /* Allocate the supplement for a throw. */
      { a_throw_supplement_ptr tsp = alloc_cil_of_type(a_throw_supplement);
        node->variant.throw_info = tsp;
        tsp->type         = NULL;
        tsp->dynamic_init = NULL;
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
        tsp->expr         = NULL;
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#if !ABI_CHANGES_FOR_RTTI
        tsp->accessible_base_classes = NULL;
#endif /* !ABI_CHANGES_FOR_RTTI */
        tsp->destructor   = NULL;
      }
#endif /* STANDALONE_UTILITY_PROGRAM */
      break;
    case enk_condition:
#if STANDALONE_UTILITY_PROGRAM
      /* Allocation of IL entries is not available in standalone utility
         programs. */
      node->variant.condition = NULL;
#else /* !STANDALONE_UTILITY_PROGRAM */
      { a_condition_supplement_ptr csp =
                                     alloc_cil_of_type(a_condition_supplement);
        node->variant.condition = csp;
        csp->scope          = NULL;
        csp->dynamic_init   = NULL;
        csp->expr           = NULL;
        csp->initialization = NULL;
      }
#endif /* STANDALONE_UTILITY_PROGRAM */
      break;
    case enk_object_lifetime:
      node->variant.object_lifetime.expr = NULL;
      node->variant.object_lifetime.ptr  = NULL;
      break;
    case enk_typeid:
      node->variant.typeid_info.type_with_opt_expr = NULL;
      node->variant.typeid_info.is_dynamic = FALSE;
      break;
    case enk_sizeof:
    case enk_datasizeof:
    case enk_alignof:
      node->variant.sizeof_info.is_type = TRUE;
      node->variant.sizeof_info.is_std_alignof = FALSE;
      node->variant.sizeof_info.variant.type = NULL;
      break;
    case enk_sizeof_pack:
      node->variant.sizeof_pack.is_type = TRUE;
      node->variant.sizeof_pack.is_template_template = FALSE;
      node->variant.sizeof_pack.variant.type = NULL;
      break;
    case enk_statement:
      node->variant.statement = NULL;
      break;
    case enk_reuse_value:
      node->variant.reused_value_init = NULL;
      break;
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
    /* Nodes generated by IL lowering for partial lowering of exception
       handling features. */
    case enk_lowered_eh_construct:
      /* See set_lowered_eh_construct_node_kind. */
      break;
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
    case enk_result_of_overriding_function:
      /* Node generated as part of the body of an entry function used
         as a wrapper for a call of an overriding virtual function
         with a covariant return type. */
      /* No variant fields. */
      break;
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
#if VLA_DEALLOCATIONS_IN_IL
    case enk_vla_dealloc:
      node->variant.vla_variable = NULL;
      break;
#endif /* VLA_DEALLOCATIONS_IN_IL */
    case enk_type_operand:
      node->variant.type_operand.type = NULL;
      node->variant.type_operand.name_reference = NULL;
      break;
    case enk_builtin_operation:
      node->variant.builtin_operation.kind =
                                           (a_builtin_operation_kind)bok_last;
      node->variant.builtin_operation.operands = NULL;
      break;
    case enk_param_ref:
      node->variant.param_ref.param_num = 0;
      node->variant.param_ref.levels_up = 0;
      break;
    case enk_braced_init_list:
      node->variant.braced_init_list = NULL;
      break;
    case enk_c11_generic:
      node->variant.c11_generic.operands = NULL;
      node->variant.c11_generic.result = NULL;
      break;
#if BUILTIN_FUNCTIONS_ENABLED
    case enk_builtin_choose_expr:
      node->variant.builtin_choose_expr.operands = NULL;
      node->variant.builtin_choose_expr.choose_first = FALSE;
      break;
#endif /* BUILTIN_FUNCTIONS_ENABLED */
    case enk_await:
    case enk_yield:
      node->variant.await_info.operand = NULL;
      node->variant.await_info.ready_resume_suspend = NULL;
      break;
    case enk_fold:
      node->variant.fold.operands = NULL;
      node->variant.fold.operator_token = (a_token_kind)tok_error;
      node->variant.fold.left_associative = FALSE;
      break;
    case enk_initializer:
      node->variant.initializer.dyn_init = NULL;
      break;
    case enk_concept_id:
      node->variant.concept_id.concept_template = NULL;
      node->variant.concept_id.args = NULL;
      break;
    case enk_requires:
      node->variant.requires_expr.requirements = NULL;
      node->variant.requires_expr.parameters = NULL;
      break;
    case enk_compound_req:
      node->variant.compound_req.expr_and_constraint = NULL;
      node->variant.compound_req.is_noexcept = FALSE;
      break;
    case enk_nested_req:
      node->variant.nested_req.constraint = NULL;
      break;
    case enk_const_eval_deferred:
      node->variant.const_eval_deferred.wrapped = NULL;
      node->variant.const_eval_deferred.reattempt_state = {};
      break;
    case enk_template_name:
      node->variant.template_name = NULL;
      break;
    case enk_token_sequence:
      node->variant.token_sequence.interpolations = NULL;
      node->variant.token_sequence.tokens = NULL;
      break;
    case enk_pack_index:
      /* C++26 pack-index-expression operands. */
      node->variant.pack_index.expr = NULL;
      node->variant.pack_index.index_expr = NULL;
      break;
    default:
      unexpected_condition_str("set_expr_node_kind: bad kind");
  }  /* switch */
}  /* set_expr_node_kind */


void clear_expr_node(an_expr_node_ptr  node,
                     an_expr_node_kind kind)
/*
Set the fixed fields of the given expression node to default values, and
its kind to the indicated kind.
*/
{
  node->type = NULL;
  node->next = NULL;
  node->is_lvalue = FALSE;
  node->is_xvalue = FALSE;
  node->result_is_not_used = FALSE;
  node->is_initialization_guard = FALSE;
  node->generated_default_arg = FALSE;
  node->marked_as_gnu_extension = FALSE;
  node->is_static_cast = FALSE;
  node->is_functional_notation_cast = FALSE;
  node->is_brace_notation_cast = FALSE;
  node->is_objectless_nonstatic_data_mem_ref = FALSE;
  node->is_pack_expansion = FALSE;
  node->is_shallow_copy = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  node->is_safe_cast = FALSE;
  node->element_of_cli_param_array_arg = FALSE;
  node->is_cli_typeid = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING
  node->is_non_normalized_boolean_controlling_expr = FALSE;
#endif /* DO_IL_LOWERING */
#if BACK_END_IS_CP_GEN_BE
  node->keep_as_cast_for_cp_gen_be = FALSE;
  node->needed_in_cp_gen_be = FALSE;
#endif /* BACK_END_IS_CP_GEN_BE */
  node->is_parenthesized = FALSE;
  node->type_definition_needed = FALSE;
  node->volatile_fetch = FALSE;
  node->do_not_interpret = FALSE;
  node->compiler_generated = FALSE;
  node->is_type_constraint = FALSE;
  node->was_lvalue_temp_initializer = FALSE;
  node->position = null_source_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  node->expr_range = null_source_range; 
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  node->extra.rescan_info = NULL;
  set_expr_node_kind(node, kind);
}  /* clear_expr_node */

#if !STANDALONE_UTILITY_PROGRAM

an_expr_node_ptr alloc_expr_node(an_expr_node_kind kind)
/*
Allocate and initialize an expression node.
*/
{
  an_expr_node_ptr ptr;

  db_enter(5, "alloc_expr_node");

  if (curr_il_region_number == file_scope_region_number) {
    if (avail_fs_nodes != NULL) {
      ptr = avail_fs_nodes;
      /* If this assertion fails some part of the front end is still using this
         reclaimed expression. */
      check_assertion_str(ptr->kind == enk_reclaimed,
                          "expected a reclaimed expression");
      avail_fs_nodes = ptr->extra.next_avail;
      trace_alloc_check(ptr);
    } else {
      ptr = alloc_il_of_type(an_expr_node);
#if DEBUG
      num_fs_expr_nodes_allocated += 1;
      { /* Increment num_rescan_fs_expr_nodes_allocated if we're in an
           expression rescan context (this is approximate). */
        /*lint -e2701 -e1798*/
        extern void count_rescan_fs_expr_nodes(unsigned long*);
        count_rescan_fs_expr_nodes(&num_rescan_fs_expr_nodes_allocated);
      }
#endif /* DEBUG */
    }  /* if */
  } else {
    ptr = alloc_cil_of_type(an_expr_node);
  }  /* if */
  clear_expr_node(ptr, kind);

  db_exit();
  return ptr;
}  /* alloc_expr_node */


an_expr_node_ptr fs_alloc_expr_node(an_expr_node_kind kind)
/*
This function is a wrapper for alloc_expr_node that ensures an expression node
is allocated in file-scope memory.  kind is the kind passed on to
alloc_expr_node.
*/
{
  an_expr_node           *result;
  a_memory_region_number region_to_switch_back_to;

  switch_to_file_scope_region(&region_to_switch_back_to);
  result = alloc_expr_node(kind);
  switch_back_to_original_region(region_to_switch_back_to);
  return result;
}  /* fs_alloc_expr_node */

#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING

void set_lowered_eh_construct_node_kind(an_expr_node_ptr node,
                                        a_lowered_eh_construct_kind kind)
/*
node is an enk_lowered_eh_construct node, used to represent a (partially)
lowered exception handling construct after IL lowering.  Set its kind field
to kind, and set dependent variant fields to default values.
*/
{
  node->variant.lowered_eh.kind = kind;
  switch (kind) {
    case leck_caught_object_address:
      node->variant.lowered_eh.variant.caught_object_handler = NULL;
      break;
    case leck_thrown_object_address:
      /* No variant fields. */
      break;
    case leck_cleanup_state:
    case leck_unreachable_cleanup_state:
#if GENERATE_EH_TABLES
      node->variant.lowered_eh.variant.cleanup_region_number = 0;
#else /* !GENERATE_EH_TABLES */
      node->variant.lowered_eh.variant.cleanup_ptr = NULL;
#endif /* GENERATE_EH_TABLES */
      break;
    case leck_function_prologue:
      { an_eh_prologue_supplement_ptr psp =
                                  alloc_cil_of_type(an_eh_prologue_supplement);
        node->variant.lowered_eh.variant.prologue_info = psp;
        psp->routine = NULL;
#if GENERATE_EH_TABLES
        psp->region_table = NULL;
        psp->array_table = NULL;
#endif /* GENERATE_EH_TABLES */
      }
      break;
    case leck_function_epilogue:
      node->variant.lowered_eh.variant.epilogue_routine = NULL;
      break;
    case leck_catch_epilogue:
      node->variant.lowered_eh.variant.epilogue_handler = NULL;
      break;
    case leck_try_epilogue:
      node->variant.lowered_eh.variant.epilogue_try_block = NULL;
      break;
    case leck_exception_caught:
    case leck_exception_started:
      /* No variant fields. */
      break;
#if !GENERATE_EH_TABLES
    case leck_initialization_completed:
      node->variant.lowered_eh.variant.dynamic_init = NULL;
      break;
#endif /* !GENERATE_EH_TABLES */
    case leck_internal_try:
      node->variant.lowered_eh.variant.try_and_catch_expr = NULL;
      break;
    default:
      unexpected_condition_str(
          "set_lowered_eh_construct_node_kind: bad lowered eh construct kind");
  }  /* switch */
}  /* set_lowered_eh_construct_node_kind */


an_expr_node_ptr alloc_lowered_eh_construct_node(
                                              a_lowered_eh_construct_kind kind)
/*
Allocate an expression node of kind enk_lowered_eh_construct, used to
represent a (partially) lowered exception handling construct.  Set its
kind field to kind, and set dependent variant fields to default values.
Return a pointer to the entry.
*/
{
  an_expr_node_ptr node =
                  alloc_expr_node((an_expr_node_kind)enk_lowered_eh_construct);

  /* For most kinds, void type is correct. */
  node->type = void_type();
  set_lowered_eh_construct_node_kind(node, kind);
  return node;
}  /* alloc_lowered_eh_construct_node */

#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */

static void clear_range_based_for_loop(a_range_based_for_loop_ptr rbflp)
/*
Clear the indicated range-based-for loop entry.
*/
{
  rbflp->initialization = NULL;
  rbflp->iterator = NULL;
  rbflp->range = NULL;
  rbflp->range_based_for_scope = NULL;
  rbflp->iterator_scope = NULL;
  rbflp->begin = NULL;
  rbflp->end = NULL;
  rbflp->ne_call_expr = NULL;
  rbflp->incr_call_expr = NULL;
  rbflp->use_await = FALSE;
}  /* clear_range_based_for_loop */

#if MICROSOFT_EXTENSIONS_ALLOWED

void set_for_each_loop_kind(a_for_each_loop_ptr     felp,
                            a_for_each_pattern_kind kind)
/*
Set the kind of the for-each loop to "kind", and set the associated variant
fields to default values.
*/
{
  felp->kind = kind;
  switch (kind) {
    case sfepk_none:
      break;
    case sfepk_stl_pattern:
    case sfepk_array_pattern:
      felp->variant.stl_array_pattern.end_variable = NULL;
      felp->variant.stl_array_pattern.ne_call_expr = NULL;
      felp->variant.stl_array_pattern.incr_call_expr = NULL;
      break;
    case sfepk_cli_pattern:
      felp->variant.cli_pattern.movenext_call_expression = NULL;
      break;
    case sfepk_cli_array_pattern:
      felp->variant.cli_array_pattern.upper_bound_vars = NULL;
      felp->variant.cli_array_pattern.loop_vars = NULL;
      break;
    default:
      unexpected_condition_str("set_for_each_loop_kind: bad kind");
  }  /* switch */
}  /* set_for_each_loop_kind */


static void clear_for_each_loop(a_for_each_loop_ptr     felp,
                                a_for_each_pattern_kind kind)
/*
Clear the indicated for-each loop entry, set the kind as given, and set the
associated variant fields to default values.
*/
{
  felp->uses_prev_decl_iterator = FALSE;
  /* Clear fields of inactive variant too for union-as-struct testing. */
  { felp->iterator.prev_decl.variable = NULL;
    felp->iterator.prev_decl.field = NULL;
    felp->iterator.prev_decl.assign_expr = NULL;
  }
  felp->iterator.variable = NULL;
  felp->collection_expr_ref = NULL;
  felp->for_each_scope = NULL;
  felp->iterator_scope = NULL;
  felp->temporary_variable = NULL;
  set_for_each_loop_kind(felp, kind);
}  /* clear_for_each_loop */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

a_switch_case_entry_ptr alloc_switch_case_entry(void)
/*
Allocate storage to describe an individual switch case, clear it to default
values, and return a pointer to it.
*/
{
  a_switch_case_entry_ptr  entry;

  entry = alloc_cil_of_type(a_switch_case_entry);
  entry->stmt = NULL;
  entry->case_value = NULL;
#if GNU_EXTENSIONS_ALLOWED
  entry->range_end = NULL;
#endif /* GNU_EXTENSIONS_ALLOWED */
  entry->next = NULL;
  entry->next_on_sorted_list = NULL;
  entry->position = null_source_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  entry->end_position = null_source_position;
  entry->colon_position = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  entry->reachable_by_fall_through = TRUE;
  return entry;
}  /* alloc_switch_case_entry */


static a_switch_stmt_descr_ptr alloc_switch_stmt_descr(void)
/*
Allocate storage to describe the details of a switch statement, clear it to
default values, and return a pointer to it.
*/
{
  a_switch_stmt_descr_ptr  descr;

  descr = alloc_cil_of_type(a_switch_stmt_descr);
  descr->cases = NULL;
  descr->default_case = NULL;
  descr->sorted_cases = NULL;
  return descr;
}  /* alloc_switch_stmt_descr */

#if !ABI_CHANGES_FOR_RTTI

an_accessible_base_class_ptr alloc_accessible_base_class(a_base_class_ptr bcp)
/*
Allocate an accessible_base_class, clear it to default values and set the
base class to bcp, and return a pointer to it.
*/
{
  an_accessible_base_class_ptr abcp;

  abcp = alloc_cil_of_type(an_accessible_base_class);
  abcp->next       = NULL;
  abcp->base_class = bcp;

  return abcp;
}  /* alloc_accessible_base_class */

#endif /* !ABI_CHANGES_FOR_RTTI */

a_handler_ptr alloc_handler(void)
/*
Allocate a handler, clear it to default values, and return a pointer to it.
*/
{
  a_handler_ptr hp;

  hp = alloc_cil_of_type(a_handler);
  hp->next         = NULL;
  hp->parameter    = NULL;
  hp->statement    = NULL;
  hp->dynamic_init = NULL;
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
  hp->typeinfo_var = NULL;
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
  hp->catch_position = null_source_position;

  return hp;
}  /* alloc_handler */


a_coroutine_descr_ptr alloc_coroutine_descr(void)
/*
Allocate a coroutine description, initialize it to default values, and return
a pointer to it.
*/
{
  a_coroutine_descr_ptr  cdp = alloc_cil_of_type(a_coroutine_descr);

  cdp->traits = NULL;
  cdp->handle = NULL;
  cdp->promise = NULL;
  cdp->init_await_resume = NULL;
  cdp->this_param_copy = NULL;
  cdp->parameter_copies = NULL;
  cdp->final_suspend_label = NULL;
  cdp->initial_suspend_call = NULL;
  cdp->final_suspend_call = NULL;
  cdp->unhandled_exception_call = NULL;
  cdp->get_return_object_call = NULL;
  cdp->alloc_failure_gro_call = NULL;
  cdp->new_routine = NULL;
  cdp->delete_routine = NULL;
  cdp->error_descr = FALSE;
  cdp->has_return_void = FALSE;
  cdp->body_generated = FALSE;

  return cdp;
}  /* alloc_coroutine_descr */


void set_statement_kind(a_statement_ptr  sp,
                        a_statement_kind stmt_kind)
/*
Set the kind of the statement sp to stmt_kind, and set the associated variant
fields to default values.
*/
{
  a_block_ptr          bp;
  a_for_loop_ptr       flip;
  a_range_based_for_loop_ptr
                       rbflp;
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_for_each_loop_ptr  felp;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_try_supplement_ptr tsp;

  sp->kind = stmt_kind;
  sp->expr = NULL;
  switch (stmt_kind) {
    case stmk_empty:
    case stmk_expr:
#if GNU_EXTENSIONS_ALLOWED
    case stmk_assigned_goto:
#endif /* GNU_EXTENSIONS_ALLOWED */
#if UPC_EXTENSIONS_ALLOWED
    case stmk_upc_notify:
    case stmk_upc_wait:
    case stmk_upc_barrier:
    case stmk_upc_fence:
#endif /* UPC_EXTENSIONS_ALLOWED */
    case stmk_coroutine_return:
      /* No variant fields. */
      break;
    case stmk_if:
    case stmk_if_consteval:
    case stmk_if_not_consteval:
      sp->variant.if_stmt.then_statement =
      sp->variant.if_stmt.else_statement = NULL;
#if EXTRA_SOURCE_POSITIONS_IN_IL
      sp->variant.if_stmt.else_position = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      break;
    case stmk_constexpr_if:
      sp->variant.constexpr_if = alloc_constexpr_if();
      break;
    case stmk_while:
    case stmk_end_test_while:
      sp->variant.loop_statement = NULL;
      break;
#if UPC_EXTENSIONS_ALLOWED
    /* The UPC forall statement is handled like the normal for statement. */
    case stmk_upc_forall:
#endif /* UPC_EXTENSIONS_ALLOWED */
    case stmk_for:
      sp->variant.for_loop.statement = NULL;
      sp->variant.for_loop.extra_info = flip = alloc_cil_of_type(a_for_loop);
      flip->initialization = NULL;
      flip->increment = NULL;
      flip->for_init_scope = NULL;
#if UPC_EXTENSIONS_ALLOWED
      flip->affinity = NULL;
#endif /* UPC_EXTENSIONS_ALLOWED */
      break;
    case stmk_range_based_for:
      sp->variant.range_based_for_loop.statement = NULL;
      sp->variant.range_based_for_loop.extra_info = rbflp =
                                     alloc_cil_of_type(a_range_based_for_loop);
      clear_range_based_for_loop(rbflp);
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case stmk_for_each:
      sp->variant.for_each_loop.statement = NULL;
      sp->variant.for_each_loop.extra_info = felp =
                                            alloc_cil_of_type(a_for_each_loop);
      clear_for_each_loop(felp, (a_for_each_pattern_kind)sfepk_none);
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case stmk_switch_case:
      sp->variant.switch_case.switch_statement = NULL;
      sp->variant.switch_case.extra_info       = NULL;
      break;
    case stmk_switch:
      sp->variant.switch_stmt.body_statement = NULL;
      sp->variant.switch_stmt.extra_info     = alloc_switch_stmt_descr();
      break;
    case stmk_goto:
    case stmk_label:
      sp->variant.label.ptr      = NULL;
      sp->variant.label.lifetime = NULL;
      break;
    case stmk_return:
      sp->variant.return_dynamic_init = NULL;
      break;
    case stmk_coroutine:
      sp->variant.coroutine.descr = NULL;
      break;
    case stmk_block:
      sp->variant.block.statements = NULL;
      sp->variant.block.extra_info = bp = alloc_cil_of_type(a_block);
      bp->final_position = null_source_position;
      bp->assoc_scope            = NULL;
      bp->lifetime               = NULL;
      bp->end_of_block_reachable = TRUE;
      bp->is_statement_expression = FALSE;
      bp->implicit_scope_not_allowed = FALSE;
#if UPC_EXTENSIONS_ALLOWED
      bp->upc_access_method      = (a_upc_access_method)upc_access_unspecified;
#endif /* UPC_EXTENSIONS_ALLOWED */
      break;
    case stmk_init:
      sp->variant.dynamic_init = NULL;
      break;
    case stmk_asm:
      sp->variant.asm_entry = NULL;
      break;
#if ASM_FUNCTION_ALLOWED
    case stmk_asm_func_body:
      sp->variant.asm_func_body = NULL;
      break;
#endif /* ASM_FUNCTION_ALLOWED */
    case stmk_try_block:
      sp->variant.try_block = tsp = alloc_cil_of_type(a_try_supplement);
      tsp->is_function_try_block = FALSE;
      tsp->statement         = NULL;
      tsp->handlers          = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
      tsp->finally_statement = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      tsp->lifetime          = NULL;
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case stmk_microsoft_try:
      { a_microsoft_try_supplement_ptr mtsp =
                                 alloc_cil_of_type(a_microsoft_try_supplement);
        sp->variant.microsoft_try = mtsp;
        mtsp->guarded_statement = NULL;
        mtsp->except_expr       = NULL;
        mtsp->cleanup_statement = NULL;
#if EXTRA_SOURCE_POSITIONS_IN_IL
        mtsp->except_or_finally_position = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      }
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case stmk_decl:
      sp->variant.decl.entities = NULL;
      sp->variant.decl.has_static_or_thread_variable = FALSE;
      break;
    case stmk_set_vla_size:
      sp->variant.vla_dimension = NULL;
      break;
    case stmk_vla_decl:
      sp->variant.vla.is_typedef_decl  = FALSE;
      sp->variant.vla.variant.variable = NULL;
      break;
    case stmk_stmt_expr_result:
      sp->variant.stmt_expr_result.dynamic_init = NULL;
      break;
    default:
      unexpected_condition_str("set_statement_kind: bad kind");
  }  /* switch */
}  /* set_statement_kind */


a_statement_ptr alloc_statement(a_statement_kind stmt_kind,
                                a_boolean        compiler_generated)
/*
Allocate a statement entry, clear it to default values, and return a pointer
to it.  The statement kind is set as indicated, as is the compiler_generated
flag.
*/
{
  a_statement_ptr sp;

  db_enter(5, "alloc_statement");

  sp = alloc_cil_of_type(a_statement);
  sp->position = null_source_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  sp->end_position = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  sp->next                    = NULL;
  sp->parent                  = NULL;
  sp->attributes              = NULL;
  sp->has_associated_pragma   = FALSE;
  sp->is_initialization_guard = FALSE;
  sp->compiler_generated      = compiler_generated;
#if DO_IL_LOWERING
  check_assertion(!(il_lowering_underway && !compiler_generated));
  sp->lowering_generated      = il_lowering_underway;
  sp->is_lowering_boilerplate = FALSE;
#endif /* DO_IL_LOWERING */
  sp->is_fallthrough_statement= FALSE;
  sp->is_likely               = FALSE;
  sp->is_unlikely             = FALSE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  sp->source_sequence_entry = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  set_statement_kind(sp, stmt_kind);
  db_exit();
  return sp;
}  /* alloc_statement */


a_constructor_init_ptr alloc_ctor_init(a_constructor_init_kind  kind)
/*
Allocate a constructor initializer entry, initialize it, and return a
pointer to it.
*/
{
  a_constructor_init_ptr  cip;

  cip = alloc_cil_of_type(a_constructor_init);
  cip->next = NULL;
  cip->kind = kind;
  cip->compiler_generated = FALSE;
  cip->is_pack_expansion = FALSE;
  cip->is_braced = FALSE;
  cip->use_field_initializer = FALSE;
  switch (kind) {
    case cik_virtual_base_class:
    case cik_direct_base_class:
      cip->variant.base_class = NULL;
      break;
    case cik_field:
      cip->variant.field = NULL;
      break;
    case cik_delegation:
      /* No variant members. */
      break;
    default:
      unexpected_condition_str("alloc_ctor_init: bad kind");
  }  /* switch */
  cip->initializer = NULL;
  cip->source_expr = NULL;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  cip->ctor_init_range = null_source_range; 
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  cip->orig_type = NULL;

  return cip;
}  /* alloc_ctor_init */

#if GNU_EXTENSIONS_ALLOWED

void clear_gcc_pragma_descr(a_gcc_pragma_descr  *gpd)
/*
Clear the given GCC pragma description.
*/
{
  gpd->kind = (a_gcc_pragma_kind)gcc_pk_none;
}  /* clear_gcc_pragma_descr */

#endif /* GNU_EXTENSIONS_ALLOWED */

a_pragma_ptr alloc_pragma(a_pragma_kind kind)
/*
Allocate a pragma entry of the required kind, initialize it, and return a
pointer to it.
*/
{
  a_pragma_ptr pp = alloc_cil_of_type(a_pragma);
  pp->next                  = NULL;
  pp->kind                  = kind;
  pp->ignore_in_back_end    = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  pp->is_microsoft_pragma_operator = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  clear_tagged_ptr(pp->entity);
  pp->position              = null_source_position;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  pp->source_sequence_entry = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  pp->pragma_text           = NULL;
  switch (kind) {
#if IDENT_DIRECTIVE_AND_PRAGMA
    case pk_ident_pragma:
      break;
    case pk_ident_directive:
      pp->variant.ident_string = NULL;
      break;
#endif /* IDENT_DIRECTIVE_AND_PRAGMA */
    case pk_none:
      break;
    case pk_pack:
#if BACK_END_IS_CP_GEN_BE
      pp->variant.alignment = 0;
#endif /* BACK_END_IS_CP_GEN_BE */
      break;
#if PRAGMA_WEAK_ALLOWED
    case pk_weak:
#endif /* PRAGMA_WEAK_ALLOWED */
#if REDEFINE_EXTNAME_PRAGMA_ENABLED
    case pk_redefine_extname:
#endif /* REDEFINE_EXTNAME_PRAGMA_ENABLED */
#if SUN_EXTENSIONS_ALLOWED
    case pk_enable_ldscope:
    case pk_disable_ldscope:
#endif /* SUN_EXTENSIONS_ALLOWED */
    case pk_diag_suppress:
    case pk_diag_remark:
    case pk_diag_warning:
    case pk_diag_error:
    case pk_diag_once:
    case pk_diag_default:
    case pk_diagnostic:
#if INCLUDE_EDG_TEST_PRAGMAS
    case pk_test_next_statement:
    case pk_test_next_decl:
    case pk_test_immediate:
    case pk_test_immediate_text:
    case pk_test_immediate_pp_text:
    case pk_test_other:
    case pk_test_bind_next_pass:
#endif /* INCLUDE_EDG_TEST_PRAGMAS */
      break;
#if ADD_CHECKING_PRAGMAS_FOR_INTERNAL_TESTING
    case pk_checking_pragma:
      break;
#endif /* ADD_CHECKING_PRAGMAS_FOR_INTERNAL_TESTING */
#if DEBUG
    case pk_db_opt:
    case pk_db_name:
      break;
#endif /* DEBUG */
    case pk_push_macro:
    case pk_pop_macro:
#if MICROSOFT_EXTENSIONS_ALLOWED
#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
    case pk_setlocale:
#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL
    case pk_unrecognized:
      /* No special initialization is required. */
      break;
#endif /* INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* The following identify pragmas that have immediate effect in the
       front end and do not get passed to the back end; therefore, no IL
       pragma entries are created for them.  The exception is when
       source-sequence lists are being put out, since all pragmas need to be
       included on such lists. */
    case pk_printf_args:
    case pk_scanf_args:
    case pk_lint_argsused:
    case pk_lint_varargs_count:
    case pk_lint_notreached:
    case pk_instantiate:
    case pk_do_not_instantiate:
    case pk_can_instantiate:
    case pk_inline_template:
    case pk_define_type_info:
      break;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
    case pk_if_exists:
      break;
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
    case pk_stdc:
      pp->variant.stdc.kind = (a_stdc_pragma_kind)stdc_pk_none;
      break;
#if UPC_EXTENSIONS_ALLOWED
    case pk_upc:
      pp->variant.upc.kind = (a_upc_pragma_kind)upc_pk_access;
      pp->variant.upc.value.access_method =
                                   (a_upc_access_method)upc_access_unspecified;
      break;
#endif /* UPC_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case pk_comment:
      pp->variant.comment.kind =
                                (a_microsoft_pragma_comment_type)mpct_compiler;
      pp->variant.comment.str = NULL;
      break;
    case pk_conform:
      pp->variant.conform.kind =
                                (a_microsoft_pragma_conform_kind)mpck_forScope;
      pp->variant.conform.on = FALSE;
      pp->variant.conform.off = FALSE;
      pp->variant.conform.show = FALSE;
      pp->variant.conform.push = FALSE;
      pp->variant.conform.pop = FALSE;
      pp->variant.conform.identifier = NULL;
      break;
    case pk_include_alias:
      pp->variant.include_alias.long_file_name = NULL;
      pp->variant.include_alias.short_file_name = NULL;
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
    case pk_gcc_immediate:
    case pk_gcc_next_token:
    case pk_gcc_next_statement:
      clear_gcc_pragma_descr(&pp->variant.gcc);
      break;
#if GNU_VECTOR_TYPES_ALLOWED && BUILTIN_FUNCTIONS_ENABLED
    case pk_gnu_riscv:
    case pk_clang_riscv:
      break;
#endif /* GNU_VECTOR_TYPES_ALLOWED && BUILTIN_FUNCTIONS_ENABLED */
#endif /* GNU_EXTENSIONS_ALLOWED */
    default:
      unexpected_condition_str("alloc_pragma: bad pragma kind");
  }  /* switch */

  return pp;
}  /* alloc_pragma */


an_object_lifetime_ptr alloc_object_lifetime(an_object_lifetime_kind  kind)
/*
Allocate an object lifetime entry, initialize its fields, and return a pointer
to it.
*/
{
  an_object_lifetime_ptr  olp, *avail_list_ptr;
  a_scope_depth           scope_depth;

  db_enter(5, "alloc_object_lifetime");
  /* Use an object lifetime entry that is on an available list, if possible;
     otherwise, allocate a new one. */
  /* Note that the file scope and every function scope (i.e., each scope for
     which there is a unique memory region) has its own available list. */
  if (curr_il_region_number == file_scope_region_number) {
    /* Use the file scope. */
    scope_depth = DEPTH_OF_FILE_SCOPE;
  } else {
    /* Use the current function scope. */
    scope_depth = depth_innermost_function_scope;
  }  /* if */
  /* When IL lowering generates routines, there is no scope stack entry,
     and therefore no available list can be maintained. */
  if (scope_depth != NO_SCOPE_DEPTH &&
      (avail_list_ptr = &scope_stack[scope_depth].object_lifetime_avail_list,
       *avail_list_ptr != NULL)) {
    /* Reuse a previously freed entry. */
    olp = *avail_list_ptr;
    *avail_list_ptr = olp->next;
  } else {
    /* Allocate a new entry. */
    olp = alloc_cil_of_type(an_object_lifetime);
  }  /* if */
  /* Set the fields to default values. */
  clear_tagged_ptr(olp->entity);
  olp->kind                       = kind;
  olp->has_block_after_label_child_lifetime
                                  = FALSE;
  olp->has_implicit_child         = FALSE;
  olp->block_lifetime_with_label_or_goto = FALSE;
  olp->destructions               = NULL;
  olp->parent_lifetime            = NULL;
  olp->parent_destruction_sublist = NULL;
  olp->child_lifetime             = NULL;
  olp->next                       = NULL;
  db_exit();
  return olp;
}  /* alloc_object_lifetime */


void clear_namespace(a_namespace_ptr nsp,
                     a_boolean       is_alias)
/*
Initialize the namespace pointed to by nsp.  The namespace is an alias if
is_alias is TRUE.
*/
{
  set_default_source_corresp(&nsp->source_corresp);
  nsp->next = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  nsp->proxy_class = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  nsp->hash_value = 0;
  nsp->is_namespace_alias = is_alias;
  nsp->is_inline = FALSE;
  nsp->has_internal_linkage = FALSE;
  nsp->named_in_strong_using = FALSE;
  nsp->is_std = FALSE;
#if BACK_END_IS_CP_GEN_BE
  nsp->shadowed_by_class = FALSE;
#endif /* BACK_END_IS_CP_GEN_BE */
#if GNU_EXTENSIONS_ALLOWED
  nsp->has_gnu_abi_tag_attribute = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (is_alias) {
    nsp->variant.assoc_namespace = NULL;
  } else {
    nsp->variant.assoc_scope = NULL;
  }  /* if */
}  /* clear_namespace */


a_namespace_ptr alloc_namespace(a_boolean  is_alias)
/*
Allocate a namespace entry, initialize its fields, and return a pointer to
it.  The entry is allocated in the file scope memory region (even when
creating an entry for a local namespace alias).
*/
{
  a_namespace_ptr nsp;

  db_enter(5, "alloc_namespace");
  nsp = alloc_il_of_type(a_namespace);
  clear_namespace(nsp, is_alias);
  db_exit();
  return nsp;
}  /* alloc_namespace */


a_using_decl_ptr alloc_using_decl(void)
/*
Allocate a using-decl entry, initialize its fields, and return a pointer to it.
*/
{
  a_using_decl_ptr  udp;

  db_enter(5, "alloc_using_decl");
  udp = alloc_cil_of_type(a_using_decl);
  udp->next                  = NULL;
  udp->position              = null_source_position;
  clear_tagged_ptr(udp->entity);
  udp->attributes            = NULL;
  udp->is_using_directive    = FALSE;
  udp->is_class_member       = FALSE;
  udp->is_inheriting_ctor    = FALSE;
  udp->hidden                = FALSE;
  udp->compiler_generated    = FALSE;
  udp->inline_namespace      = FALSE;
  udp->strong                = FALSE;
  udp->is_pack_expansion     = FALSE;
  udp->is_representative     = FALSE;
  udp->is_using_enum         = FALSE;
  udp->is_enumerator         = FALSE;
  udp->access                = (an_access_specifier)as_public;
  udp->qualifier.namespace_ptr
                             = NULL;
  udp->decl_sequence_number  = 0;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  udp->source_sequence_entry = NULL;
  udp->next_in_set           = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

  db_exit();
  return udp;
}  /* alloc_using_decl */


void set_scope_kind(a_scope_ptr    sp,
                    a_scope_kind   kind,
                    a_routine_ptr  assoc_routine)
/*
Initialize the variable fields of the scope entry pointed to by sp.
*/
{
  check_assertion_str(assoc_routine == NULL ||
                        kind == (a_scope_kind)sck_function,
                      "set_scope_kind: assoc_routine is non-NULL");
  sp->kind   = kind;
  switch (kind) {
    case sck_file:
    case sck_template_declaration:
      break;
    case sck_block:
      sp->variant.assoc_handler = NULL;
      break;
    case sck_func_prototype:
    case sck_class_struct_union:
    case sck_enum:
      sp->variant.assoc_type = NULL;
      break;
    case sck_function:
      sp->variant.routine.ptr                           = assoc_routine;
      sp->variant.routine.parameters                    = NULL;
      sp->variant.routine.constructor_inits             = NULL;
      sp->variant.routine.lifetime_of_local_static_vars = NULL;
      sp->variant.routine.this_param_variable           = NULL;
      sp->variant.routine.return_value_variable         = NULL;
      break;
    case sck_condition:
      sp->variant.assoc_statement = NULL;
      break;
    case sck_namespace:
      sp->variant.assoc_namespace = NULL;
      break;
    default:
      unexpected_condition_str("set_scope_kind: bad scope kind");
  }  /* switch */
}  /* set_scope_kind */


a_scope_ptr alloc_scope(a_scope_kind   kind,
                        a_scope_number number,
                        a_routine_ptr  assoc_routine)
/*
Allocate a scope entry, and return a pointer to it.  Set fixed fields to
default values.  kind indicates the scope kind (e.g., function, block),
number indicates the unique number for the scope, and assoc_routine
points to the associated routine if the kind is sck_function.
*/
{
  a_scope_ptr sp;

  db_enter(5, "alloc_scope");

  sp = alloc_cil_of_type(a_scope);
  sp->next   = NULL;
  sp->prev    = NULL;
  sp->parent = NULL;
  sp->number = number;
  sp->function_body_processing_finished = FALSE;
  sp->do_not_free_memory_region = FALSE;
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  sp->scope_orphaned_list_header_generated = FALSE;
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
  sp->is_constexpr_routine = FALSE;
  sp->is_stmt_expr_block = FALSE;
  sp->is_placeholder_scope = FALSE;
  sp->needed_walk_done = FALSE;
  set_scope_kind(sp, kind, assoc_routine);
  sp->assoc_block                 = NULL;
  sp->lifetime                    = NULL;
  sp->constants                   = NULL;
  sp->types                       = NULL;
  sp->variables                   = NULL;
  sp->nonstatic_variables         = NULL;
  sp->labels                      = NULL;
  sp->routines                    = NULL;
  sp->asm_entries                 = NULL;
  sp->scopes                      = NULL;
  sp->namespaces                  = NULL;
  sp->using_declarations          = NULL;
  sp->using_directives            = NULL;
  sp->dynamic_inits               = NULL;
  sp->local_static_variable_inits = NULL;
  sp->vla_dimensions              = NULL;
  sp->expr_node_refs              = NULL;
  sp->scope_refs                  = NULL;
  sp->pragmas                     = NULL;
  sp->depth_in_scope_stack        = NO_SCOPE_DEPTH;
  sp->symbols                     = NULL;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  sp->source_sequence_list        = NULL;
  sp->src_seq_sublist_list        = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if RECORD_HIDDEN_NAMES_IN_IL
  sp->hidden_names                = NULL;
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
  sp->templates                   = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  sp->ms_attributes               = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
  sp->ms_if_exists                = NULL;
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
  db_exit();
  return sp;
}  /* alloc_scope */


a_scope_ptr alloc_placeholder_scope(a_scope_kind  kind,
                                    a_routine_ptr assoc_routine)
/*
Allocate a scope entry for a placeholder scope.  kind indicates the scope kind
(e.g., function, block), and assoc_routine points to the associated routine if
the kind is sck_function.
*/
{
  a_scope_ptr sp = alloc_scope(kind, NO_SCOPE_NUMBER, assoc_routine);

  sp->is_placeholder_scope = TRUE;
  return sp;
}  /* alloc_placeholder_scope */


a_local_scope_ref_ptr alloc_local_scope_ref(void)
/*
Allocate an entry to represent an outside reference to a local scope,
initialize it, and return a pointer to it.
*/
{
  a_local_scope_ref_ptr  ptr;

  ptr = alloc_cil_of_type(a_local_scope_ref);
  ptr->next = NULL;
  ptr->scope = NULL;
  clear_tagged_ptr(ptr->referrer);
  return ptr;
}  /* alloc_local_scope_ref */


a_static_assertion_ptr alloc_static_assertion(void)
/*
Allocate a static assertion entry, initialize its fields, and return a
pointer to it.
*/
{
  a_static_assertion_ptr  entry;

  db_enter(5, "alloc_static_assertion");
  entry = alloc_cil_of_type(a_static_assertion);
  entry->condition = NULL;
  entry->string_literal = NULL;
  entry->position = null_source_position;
  db_exit();
  return entry;
}  /* alloc_static_assertion */


#if GENERATE_SOURCE_SEQUENCE_LISTS

a_source_sequence_entry_ptr alloc_source_sequence_entry(void)
/*
Allocate a source sequence entry, initialize its fields, and return a pointer
to it.
*/
{
  a_source_sequence_entry_ptr  ssep, *avail_list_ptr;
  a_scope_depth                scope_depth;

  /* Use a source sequence entry that is on an available list, if possible;
     otherwise, allocate a new one. */
  /* Note that each scope that has a source sequence list (there is one such
     scope per memory region) also has its own available list. */
  if (curr_il_region_number == file_scope_region_number) {
    /* Use the file scope. */
    scope_depth = DEPTH_OF_FILE_SCOPE;
  } else {
    /* Use the current function scope. */
    check_assertion(depth_innermost_function_scope != NO_SCOPE_DEPTH);
    scope_depth = depth_innermost_function_scope;
  }  /* if */
  /* Copy the address of the available list. */
  avail_list_ptr = &scope_stack[scope_depth].source_sequence_avail_list;
  if (*avail_list_ptr != NULL) {
    ssep = *avail_list_ptr;
    trace_alloc_check(ssep);
    *avail_list_ptr = ssep->next;
    /* If this assertion fails this source sequence entry was used after it was
       "freed" via recycle_src_seq_entry.  This needs to be corrected. */
    check_assertion(ssep->entity.kind == iek_none && ssep->entity.ptr == NULL);
  } else {
    ssep = alloc_cil_of_type(a_source_sequence_entry);
  }  /* if */
  /* Initialize the fields. */
  ssep->next        = NULL;
  ssep->prev        = NULL;
  clear_tagged_ptr(ssep->entity);

  return ssep;
}  /* alloc_source_sequence_entry */


void recycle_src_seq_entry(a_source_sequence_entry_ptr  ssep)
/*
Return the given source sequence entry to the appropriate available list.
*/
{
  a_source_sequence_entry_ptr  *avail_list_ptr;

#if CHECKING
  /* Clear the entity information so that alloc_source_sequence_entry
     can catch some instances of use "after free." */
  clear_tagged_ptr(ssep->entity);
#endif /* CHECKING */
  if (in_file_scope(ssep)) {
    avail_list_ptr = &scope_stack[DEPTH_OF_FILE_SCOPE].
                                                   source_sequence_avail_list;
  } else {
    check_assertion(depth_innermost_function_scope != NO_SCOPE_DEPTH);
    avail_list_ptr = &scope_stack[depth_innermost_function_scope].
                                                   source_sequence_avail_list;
  }  /* if */
  ssep->next = *avail_list_ptr;
  *avail_list_ptr = ssep;
}  /* recycle_src_seq_entry */


a_src_seq_secondary_decl_ptr alloc_src_seq_secondary_decl(void)
/*
Allocate a source sequence secondary declaration entry, initialize its fields,
and return a pointer to it.
*/
{
  a_src_seq_secondary_decl_ptr  sssdp;

  sssdp = alloc_cil_of_type(a_src_seq_secondary_decl);
  sssdp->decl_position               = null_source_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  sssdp->decl_pos_info               = NULL;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  clear_tagged_ptr(sssdp->entity);
  sssdp->name_reference              = NULL;
  sssdp->attributes                  = NULL;
  sssdp->declared_type               = NULL;
  sssdp->declared_storage_class      = (a_storage_class)sc_unspecified;
  sssdp->autonomous_tag_decl         = FALSE;
  sssdp->embedded_source_sequence_entries = FALSE;
  sssdp->friend_decl                 = FALSE;
  sssdp->declared_in_func_prototype  = FALSE;
  sssdp->specialized_with_new_syntax = FALSE;
  sssdp->first_declaration           = FALSE;
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  sssdp->is_partial_instantiation    = FALSE;
  sssdp->compiler_generated_forward_decl = FALSE;
  sssdp->originally_nonautonomous_definition = FALSE;
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
  sssdp->marked_as_gnu_extension     = FALSE;
  sssdp->is_decl_after_first_in_comma_list = FALSE;
  sssdp->explicit_storage_class      = FALSE;
  sssdp->is_alias                    = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  sssdp->is_event_interface          = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  return sssdp;
}  /* alloc_src_seq_secondary_decl */


a_src_seq_end_of_construct_ptr alloc_src_seq_end_of_construct(void)
/*
Allocate an end-of-construct declaration entry, initialize its fields, and
return a pointer to it.
*/
{
  a_src_seq_end_of_construct_ptr  sseocp;

  sseocp = alloc_cil_of_type(a_src_seq_end_of_construct);
  sseocp->position    = null_source_position;
  clear_tagged_ptr(sseocp->entity);

  return sseocp;
}  /* alloc_src_seq_end_of_construct */


a_src_seq_sublist_ptr alloc_src_seq_sublist(void)
/*
Allocate a source sequence sublist header, initialize its fields, and return
a pointer to it.
*/
{
  a_src_seq_sublist_ptr  sssp;

  sssp = alloc_il_of_type(a_src_seq_sublist);
  sssp->next = NULL;
  sssp->source_sequence_list = NULL;
  sssp->last_source_sequence_entry = NULL;

  return sssp;
}  /* alloc_src_seq_sublist */


an_instantiation_directive_ptr alloc_instantiation_directive(void)
/*
Allocate an instantiation-directive entry, initialize its fields, and return
a pointer to it.
*/
{
  an_instantiation_directive_ptr  idp;

  idp = alloc_il_of_type(an_instantiation_directive);
  idp->position    = null_source_position;
  clear_tagged_ptr(idp->entity);
  idp->do_not_instantiate = FALSE;
  idp->attributes = NULL;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  idp->declared_type = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  idp->decl_pos_info = NULL;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

  return idp;
}  /* alloc_instantiation_directive */

#if GENERATE_LINKAGE_SPEC_BLOCKS

a_linkage_spec_block_ptr alloc_linkage_spec_block(void)
/*
Allocate and initialize an entry representing a linkage specification block
and return a pointer to it.
*/
{
  a_linkage_spec_block_ptr  entry;

  db_enter(5, "alloc_linkage_spec_block");
  entry = alloc_cil_of_type(a_linkage_spec_block);
  entry->name_string = NULL;
  entry->name_linkage = (a_name_linkage_kind)nlk_none;
  entry->position = null_source_position;
  entry->end_position = null_source_position;
  db_exit();
  return entry;
}  /* alloc_linkage_spec_block */

#endif /* GENERATE_LINKAGE_SPEC_BLOCKS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if RECORD_HIDDEN_NAMES_IN_IL

a_hidden_name_ptr alloc_hidden_name(void)
/*
Allocate a hidden-name entry in the current memory region, initialize its
fields, and return a pointer to it.
*/
{
  a_hidden_name_ptr  hnp;

  hnp = alloc_cil_of_type(a_hidden_name);
  hnp->next                             = NULL;
  clear_tagged_ptr(hnp->entity);
  hnp->qualification_needed             = FALSE;
  hnp->elaborated_type_specifier_needed = FALSE;
  hnp->partially_hidden_by_microsoft_injected_class_name
                                        = FALSE;
  hnp->is_class_member                  = FALSE;
  hnp->hidden_by_simulated_injected_class_name
                                        = FALSE;
  hnp->hidden_by_class_name             = FALSE;
  hnp->hidden_by_template_parameter     = FALSE;

  return hnp;
}  /* alloc_hidden_name */

#endif /* RECORD_HIDDEN_NAMES_IN_IL */


a_template_parameter_ptr alloc_template_parameter(void)
/*
Allocate a template parameter entry in the file-scope memory region,
initialize its fields, and return a pointer to it.
*/
{
  a_template_parameter_ptr  tpp;

  tpp = alloc_il_of_type(a_template_parameter);
  set_default_source_corresp(&tpp->source_corresp);
  tpp->next = NULL;
  tpp->kind = (a_template_parameter_kind)tpk_error;
  tpp->is_pack = FALSE;
  tpp->is_abbreviated = FALSE;
  return tpp; 
}  /* alloc_template_parameter */


a_requires_clause_ptr alloc_requires_clause(void)
/*
Allocate a requires clause entry in the file-scope memory region,
initialize its fields, and return a pointer to it.
*/
{
  a_requires_clause_ptr  rcp;

  rcp = alloc_il_of_type(a_requires_clause);
  rcp->constraint = NULL;
  rcp->requires_pos = null_source_position;
  return rcp; 
}  /* alloc_requires_clause */


a_template_decl_ptr alloc_template_decl(void)
/*
Allocate a template declaration entry in the file-scope memory region,
initialize its fields, and return a pointer to it.
*/
{
  a_template_decl_ptr  tdp;

  tdp = alloc_il_of_type(a_template_decl);
  tdp->parent = NULL;
  tdp->param_list = NULL;
  tdp->constraint.requires_clause = NULL;
  tdp->scope = NULL;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  tdp->template_pos = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if MICROSOFT_EXTENSIONS_ALLOWED
  tdp->is_generic = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

  return tdp; 
}  /* alloc_template_decl */


a_template_ptr alloc_template(void)
/*
Allocate a template entry in the file-scope memory region, initialize its
fields, and return a pointer to it.
*/
{
  a_template_ptr  tp;

  tp = alloc_il_of_type(a_template);
  set_default_source_corresp(&tp->source_corresp);
  tp->next = NULL;
  tp->kind = (a_template_kind)templk_none;
  tp->is_exported = FALSE;
  tp->ignore_export = FALSE;
  tp->is_pack = FALSE;
  tp->is_friend_template = FALSE;
#if BACK_END_IS_CP_GEN_BE
  tp->final_alignment = 0;
  tp->min_template_arguments = -1;
#endif /* BACK_END_IS_CP_GEN_BE */
  tp->cache_checksum = 0;
  tp->coordinates.position = 0;
  tp->coordinates.depth = NO_NESTING_DEPTH;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  tp->export_position = null_source_position;
  tp->definition_range = null_source_range;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  tp->template_info = NULL;
  tp->template_decl = NULL;
  tp->prototype_instantiation.type = NULL;
  tp->prototype_instantiation.routine = NULL;
  tp->prototype_instantiation.variable = NULL;
  /* Default a template's canonical template to itself. */
  tp->canonical_template = tp;
  tp->definition_template = NULL;
  tp->prototype_template = NULL;
#if RECORD_TEMPLATE_STRINGS
  tp->text = NULL;
#endif /* RECORD_TEMPLATE_STRINGS */
  return tp;
}  /* alloc_template */

#if RECORD_MACROS_IN_IL

a_macro_ptr alloc_macro(void)
/*
Allocate a macro entry in the file-scope memory region, initialize its
fields, and return a pointer to it.
*/
{
  a_macro_ptr  mp;

  mp = alloc_il_of_type(a_macro);
  set_default_source_corresp(&mp->source_corresp);
  mp->next = NULL;
  mp->is_undef = FALSE;
  mp->is_command_line_definition = FALSE;
  mp->is_predefined = FALSE;
  mp->object_like = TRUE;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  mp->replacement_text_range = null_source_range;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  mp->text = NULL;

  return mp;
}  /* alloc_macro */

#endif /* RECORD_MACROS_IN_IL */
#if RECORD_MACRO_INVOCATIONS

a_macro_invocation_record_block_ptr alloc_macro_invocation_record_block(void)
/*
Allocate a macro invocation record block entry in the file-scope memory
region, initialize the fields, and return a pointer to it.
*/
{
  a_macro_invocation_record_block_ptr mirbp;
#if !NULL_POINTER_IS_ZERO
  int                                 i;
#endif /* !NULL_POINTER_IS_ZERO */

  mirbp = alloc_il_of_type(a_macro_invocation_record_block);
#if NULL_POINTER_IS_ZERO
  /* We can use memzero to clear the block efficiently.  Note that this
     depends on NO_PARENT_MACRO_INVOCATION and SP_COL_UNKNOWN both having
     value 0. */
  memzero((char *)mirbp, sizeof(a_macro_invocation_record_block));
#else /* !NULL_POINTER_IS_ZERO */
  /* The presence of pointers in the block means that we must do
     memberwise assignments to clear it. */
  mirbp->first_record_in_block = 0;
  mirbp->left_subtree = NULL;
  mirbp->right_subtree = NULL;
  mirbp->prev = NULL;
  mirbp->next = NULL;
  for (i = 0; i < MACRO_INVOCATION_RECORDS_PER_BLOCK; ++i) {
    mirbp->records[i].parent_macro_index = NO_PARENT_MACRO_INVOCATION;
    mirbp->records[i].assoc_macro = NULL;
    mirbp->records[i].start.seq = 0;
    mirbp->records[i].start.column = SP_COL_UNKNOWN;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    mirbp->records[i].end.seq = 0;
    mirbp->records[i].end.column = SP_COL_UNKNOWN;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  }  /* for */
#endif /* NULL_POINTER_IS_ZERO */
  return mirbp;
}  /* alloc_macro_invocation_record_block */

#endif /* RECORD_MACRO_INVOCATIONS */

an_element_position_ptr alloc_element_position(void)
/*
Allocate an element-position entry in the current memory region, initialize
its fields, and return a pointer to it.
*/
{
  an_element_position_ptr  epp;

  epp = alloc_cil_of_type(an_element_position);
  epp->next = NULL;
  epp->position = null_source_position;
  epp->kind = (an_element_position_kind)epk_error;
  return epp;
}  /* alloc_element_position */

#if EXTRA_SOURCE_POSITIONS_IN_IL

void clear_decl_position_supplement(a_decl_position_supplement_ptr  dpsp)
/*
Clear the fields of the specified decl-position-supplement entry.
*/
{
  dpsp->identifier_range = null_source_range;
  dpsp->specifiers_range = null_source_range;
  dpsp->variant.declarator_range = null_source_range;
  dpsp->extra_positions = NULL;
}  /* clear_decl_position_supplement */


a_decl_position_supplement_ptr alloc_decl_position_supplement(
                                                    a_boolean  at_file_scope)
/*
Allocate a decl-position-supplement entry in the appropriate memory region,
initialize its fields, and return a pointer to it.
*/
{
  a_decl_position_supplement_ptr  dpsp;

  if (at_file_scope) {
    dpsp = alloc_il_of_type(a_decl_position_supplement);
  } else {
    dpsp = alloc_cil_of_type(a_decl_position_supplement);
  }  /* if */
  clear_decl_position_supplement(dpsp);
  return dpsp;
}  /* alloc_decl_position_supplement */

#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */


a_name_qualifier_ptr alloc_name_qualifier(void)
/*
Allocate a name qualifier entry, initialize its fields, and return a pointer
to it.
*/
{
  a_name_qualifier_ptr nqp;

  nqp = alloc_il_of_type(a_name_qualifier);
  nqp->next = NULL;
  nqp->qualifier.class_type = NULL;
  nqp->qualifier.namespace_ptr = NULL;
  nqp->previous_qualifier = NULL;
  nqp->name = NULL;
  nqp->is_class = FALSE;
  return nqp;
}  /* alloc_name_qualifier */


void clear_name_reference(a_name_reference_ptr	nrp)
/*
Initialize the fields of a name reference entry.
*/
{
  nrp->next = NULL;
  nrp->qualifier = NULL;
  nrp->variant.destructor_type = NULL;
  nrp->num_template_arguments = -1L;
  nrp->orig_template_arg_list = NULL;
  nrp->special_kind = (a_special_function_kind)sfk_none;
  nrp->is_global_qualified_name = FALSE;
  nrp->is_template_id = FALSE;
  nrp->is_super_qualified = FALSE;
  nrp->is_decltype_qualified = FALSE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  nrp->used_in_primary_declarator = FALSE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  nrp->from_prototype_instantiation = FALSE;
}  /* clear_name_reference */


a_name_reference_ptr alloc_name_reference(void)
/*
Allocate a name reference entry, initialize its fields, and return a pointer
to it.
*/
{
  a_name_reference_ptr nrp;

  nrp = alloc_il_of_type(a_name_reference);
  clear_name_reference(nrp);
  return nrp;
}  /* alloc_name_reference */


a_seq_number_lookup_entry_ptr alloc_seq_number_lookup_entry(void)
/*
Allocate a sequence number lookup entry, initialize its fields, and return
a pointer to it.
*/
{
  a_seq_number_lookup_entry_ptr snlep;

  snlep = (a_seq_number_lookup_entry_ptr)
                alloc_primary_file_scope_il(sizeof(a_seq_number_lookup_entry));
  snlep->first = 0;
  snlep->last = MAX_SEQ_NUMBER;
  snlep->line_number = 0;
  snlep->next = NULL;
  snlep->source_file = NULL;
#if DEBUG
#if EXPENSIVE_CHECKING
  snlep->is_marked_for_recycle = FALSE;
#endif /* EXPENSIVE_CHECKING */
  f_tally_alloc(iek_seq_number_lookup_entry);
#endif /* DEBUG */
  return snlep;
}  /* alloc_seq_number_lookup_entry */

#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES

an_ms_if_exists_ptr alloc_ms_if_exists(void)
/*
Allocate a Microsoft __if_exists entry, set its fields to default values,
and return a pointer to it.
*/
{
  an_ms_if_exists_ptr msiep;

  msiep = alloc_cil_of_type(an_ms_if_exists);
  msiep->next = NULL;
  clear_tagged_ptr(msiep->entity);
  msiep->position = null_source_position;
  msiep->name_reference = NULL;
  msiep->is_if_exists = FALSE;
  msiep->pending = FALSE;
  msiep->is_this = FALSE;
  return msiep;
}  /* alloc_ms_if_exists */

#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */

#if MICROSOFT_EXTENSIONS_ALLOWED

an_ms_attribute_ptr alloc_ms_attribute(an_ms_attribute_kind kind)
/*
Allocate a Microsoft attribute entry, set its fields to default values,
and return a pointer to it.
*/
{
  an_ms_attribute_ptr msap;

  msap = alloc_cil_of_type(an_ms_attribute);
  msap->kind = kind;
  msap->next = NULL;
  msap->next_in_block = NULL;
  clear_tagged_ptr(msap->entity);
  msap->is_attribute_attribute = FALSE;
  if (kind == (an_ms_attribute_kind)msak_custom) {
    msap->variant.custom_info.type = NULL;
    msap->variant.custom_info.constructor = NULL;
    msap->variant.custom_info.args = NULL;
    msap->variant.custom_info.named_args = NULL;
  } else {
    msap->variant.info.kind_descr = NULL;
    msap->variant.info.name = NULL;
    msap->variant.info.string = NULL;
    msap->variant.info.arg_list = NULL;
  }  /* if */
  msap->position = null_source_position;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  msap->source_sequence_entry = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  msap->target = (an_ms_attribute_target)msat_invalid;
  return msap;
}  /* alloc_ms_attribute */


an_ms_attribute_arg_ptr alloc_ms_attribute_arg(an_ms_attribute_arg_kind	kind)
/*
Allocate a Microsoft attribute argument entry, set its fields to default
values, and return a pointer to it.
*/
{
  an_ms_attribute_arg_ptr msaap;

  msaap = alloc_cil_of_type(an_ms_attribute_arg);
  msaap->kind = kind;
  msaap->next = NULL;
  msaap->param_name = NULL;
  switch (kind) {
    case msaak_integer:
      msaap->variant.integer_value = 0;
      break;
    case msaak_boolean:
      msaap->variant.bool_value = 0;
      break;
    case msaak_string:
      msaap->variant.string_constant = NULL;
      break;
    case msaak_other:
      msaap->variant.other_string = NULL;
      break;
    case msaak_uuid:
      msaap->variant.uuid_string = NULL;
      break;
    case msaak_enumeration:
      msaap->variant.enum_value = 0;
      break;
    case msaak_none:
    default:
      unexpected_condition_str("alloc_ms_attribute_arg: bad kind");
      break;
  }  /* switch */
  return msaap;
}  /* alloc_ms_attribute_arg */


a_custom_ms_attribute_arg_ptr alloc_custom_ms_attribute_arg(void)
/*
Allocate a custom Microsoft attribute argument entry, set its fields to
default values, and return a pointer to it.
*/
{
  a_custom_ms_attribute_arg_ptr arg;

  arg = alloc_cil_of_type(a_custom_ms_attribute_arg);
  arg->next = NULL;
  arg->field = NULL;
  arg->expression = NULL;
  return arg;
}  /* alloc_custom_ms_attribute_arg */


a_property_index_type_ptr alloc_property_index_type(void)
/*
Allocate a property index type entry, clear it to default values, and return a
pointer to it.
*/
{
  a_property_index_type_ptr  pitp = alloc_il_of_type(a_property_index_type);
  pitp->next = NULL;
  pitp->type = NULL;
  pitp->position = null_source_position;
  return pitp;
}  /* alloc_property_index_type */


a_property_or_event_descr_ptr alloc_property_or_event_descr(
                                               a_property_or_event_kind  kind)
/*
Allocate a property/event description of the given kind, clear it to default
values, and return a pointer to it.
*/
{
  a_property_or_event_descr_ptr  pdp;

  pdp = alloc_il_of_type(a_property_or_event_descr);
  pdp->kind = kind;
  pdp->is_trivial = FALSE;
  pdp->is_default_indexed = FALSE;
  pdp->is_virtual = FALSE;
  pdp->is_static = FALSE;
  pdp->indices = NULL;
  /* Clear field of inactive variant too for union-as-struct testing. */
  pdp->variant.variable = NULL;
  pdp->variant.field = NULL;
  switch (kind) {
    case pek_declspec_property:
      pdp->get_routine.name = NULL;
      pdp->set_routine.name = NULL;
      break;
    case pek_cli_property:
      pdp->get_routine.ptr = NULL;
      pdp->set_routine.ptr = NULL;
      break;
    case pek_cli_event:
      /* get_routine/set_routine is not used for events. */
      pdp->get_routine.ptr = NULL;
      pdp->set_routine.ptr = NULL;
      break;
    default:
      unexpected_condition();
  }  /* switch */
  pdp->add_routine = NULL;
  pdp->remove_routine = NULL;
  pdp->raise_routine = NULL;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  pdp->property_or_event_position = null_source_position;
  pdp->indices_range = null_source_range;
  pdp->definition_range = null_source_range;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  return pdp;
}  /* alloc_property_or_event_descr */


a_generic_constraint_ptr alloc_generic_constraint(void)
/*
Allocate an entry describing a C++/CLI generic constraint and return a
pointer to it.
*/
{
  a_generic_constraint_ptr	gcp;

  gcp = alloc_il_of_type(a_generic_constraint);
  gcp->kind = (a_generic_constraint_kind)gck_none;
  gcp->implicit_constraint = FALSE;
  gcp->next = NULL;
  gcp->type = NULL;
  gcp->type_cache = NULL;
  gcp->position = null_source_position;
  return gcp;
}  /* alloc_generic_constraint */


void clear_generic_constraint_clause(a_generic_constraint_clause_ptr gccp)
/*
Initialize the fields of a C++/CLI generic constraint clause entry.
*/
{
  gccp->next = NULL;
  gccp->type = NULL;
  gccp->type_position = null_source_position;
  gccp->constraints = NULL;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  gccp->where_position = null_source_position;
  gccp->colon_position = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* clear_generic_constraint_clause */


a_generic_constraint_clause_ptr alloc_generic_constraint_clause(void)
/*
Allocate an entry describing a C++/CLI generic constraint clause and return a
pointer to it.
*/
{
  a_generic_constraint_clause_ptr	gccp;

  gccp = alloc_il_of_type(a_generic_constraint_clause);
  clear_generic_constraint_clause(gccp);
  return gccp;
}  /* alloc_generic_constraint_clause */


an_event_interface_ptr alloc_event_interface(void)
/*
Allocate an entry describing an "__event __interface".
*/
{
  an_event_interface_ptr eip;

  eip = alloc_il_of_type(an_event_interface);
  eip->next = NULL;
  eip->interface_type = NULL;
  eip->pos = null_source_position;
  return eip;
}  /* alloc_event_interface */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

a_lambda_ptr alloc_lambda(void)
/*
Allocate an entry describing a C++11 lambda and return a pointer to it.  The
entry is allocated in the current memory region.
*/
{
  a_lambda_ptr  entry = alloc_cil_of_type(a_lambda);

  entry->capture_list = NULL;
  entry->closure_class = NULL;
  entry->lambda_routine = NULL;
  entry->is_generic = FALSE;
  entry->is_mutable = FALSE;
  entry->constexpr_specified = FALSE;
  entry->consteval_specified = FALSE;
  entry->has_capture_default = FALSE;
  entry->default_is_by_reference = FALSE;
  entry->explicit_return_type = FALSE;
  entry->has_parameter_decl = FALSE;
  entry->has_template_param_list = FALSE;
  entry->start_position = null_source_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  entry->capture_end_position = null_source_position;
  entry->mutable_position = null_source_position;
  entry->constexpr_position = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  return entry;
}  /* alloc_lambda */


a_lambda_capture_ptr alloc_lambda_capture(void)
/*
Allocate an entry describing an entity (variable, reference, or this parameter)
captured by a C++11 lambda and return a pointer to it.  The entry is allocated
in the current memory region.
*/
{
  a_lambda_capture_ptr  entry = alloc_cil_of_type(a_lambda_capture);

  entry->next = NULL;
  entry->captured.initializer = NULL;
  entry->captured.variable = NULL;
  entry->captured.init_capture_field = NULL;
  /* Clear field of all variants for union-as-struct testing. */
  entry->capture_info.source_closure_field = NULL;
  entry->capture_info.source_capture = NULL;
  entry->capture_info.init_capture_dps = NULL;
  entry->closure_field = NULL;
  entry->is_init_capture = FALSE;
  entry->is_indirect_init_capture = FALSE;
  entry->is_param_ref_capture = FALSE;
  entry->capture_by_reference = FALSE;
  entry->is_implicit = FALSE;
  entry->is_pack_expansion = FALSE;
  entry->is_pack_element = FALSE;
  entry->direct_init = FALSE;
  entry->parenthesized_init = FALSE;
  entry->field_pending = FALSE;
  entry->const_capture = FALSE;
  entry->position = null_source_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  entry->end_position = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  return entry;
}  /* alloc_lambda_capture */

an_il_entity_list_entry_ptr alloc_il_entity_list_entry_with(
                                                  a_source_correspondence *scp)
/*
Allocate an entry for a list of arbitrary IL entries, and return a pointer to
it.  If scp is non-NULL, allocate the entry in the same memory region as scp;
otherwise, allocate it in the current file-scope memory region.
*/
{
  an_il_entity_list_entry_ptr  entry;

  entry = (an_il_entity_list_entry_ptr)alloc_in_same_region_as(
                                              scp,
                                              sizeof(an_il_entity_list_entry));
  entry->next = NULL;
  clear_tagged_ptr(entry->entity);
  return entry;
}  /* alloc_il_entity_list_entry_with */


an_il_entity_list_entry_ptr alloc_il_entity_list_entry(void)
/*
Allocate an entry for a list of arbitrary IL entries, and return a pointer to
it.  The entry is allocated in the current memory region.
*/
{
  an_il_entity_list_entry_ptr  entry;

  entry = alloc_cil_of_type(an_il_entity_list_entry);
  entry->next = NULL;
  clear_tagged_ptr(entry->entity);
  return entry;
}  /* alloc_il_entity_list_entry */


an_attribute_ptr alloc_attribute(void)
/*
Allocate an attribute in file scope memory and return a pointer to it.
*/
{
  an_attribute_ptr  ap;

  ap = alloc_il_of_type(an_attribute);
  ap->next = NULL;
  ap->kind = ak_unrecognized;
  ap->family = af_internal;
  ap->syntactic_location = al_implicit;
  ap->on_primary_declaration = FALSE;
  ap->transforms_type_specifier = FALSE;
  ap->applied_to_declared_type = FALSE;
  ap->must_be_preserved_in_trans_unit_copy = FALSE;
  ap->is_pack_expansion = FALSE;
  ap->is_std_gcc_attribute = FALSE;
#if GNU_EXTENSIONS_ALLOWED
  ap->is_implicit_abi_tag_attribute = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
  ap->namespace_from_using = FALSE;
  ap->is_invalid_namespace = FALSE;
  ap->name = NULL;
  ap->namespace_name = NULL;
  ap->arguments = NULL;
  ap->group = NULL;
  ap->assoc_info = NULL;
  ap->position = null_source_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  ap->end_position = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  ap->pack_expansion_descr = NULL;
  return ap;
}  /* alloc_attribute */


an_attribute_arg_ptr alloc_attribute_arg(void)
/*
Allocate an attribute argument in file scope memory and return a pointer to it.
*/
{
  an_attribute_arg_ptr  aap;

  aap = alloc_il_of_type(an_attribute_arg);
  aap->next = NULL;
  aap->token_kind = tok_error;
  aap->kind = (an_attribute_arg_kind)aak_empty;
  aap->is_pack_expansion = FALSE;
  aap->local_expr_ref = FALSE;
  aap->pack_expansion_descr = NULL;
  aap->position = null_source_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  aap->end_position = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  aap->variant.token = NULL;
  return aap;
}  /* alloc_attribute_arg */


an_attribute_group_ptr alloc_attribute_group(void)
/*
Allocate an attribute group in file scope memory and return a pointer to it.
*/
{
  an_attribute_group_ptr  agp;

  agp = alloc_il_of_type(an_attribute_group);
  agp->position = null_source_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  agp->end_position = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  return agp;
}  /* alloc_attribute_group */


a_module_ptr alloc_module(a_module_kind kind)
/*
Allocate and return an IL entry for a module.
*/
{
  a_module_ptr mod = alloc_il_of_type(a_module);

  mod->kind = kind;
  mod->resolved_file = NULL;
  mod->file_kind = mfk_unknown;
  mod->contains_unsupported_constructs = FALSE;
  switch (mod->kind) {
    case mk_none:
    case mk_unit:
      mod->variant.unit.name = NULL;
      break;
    case mk_header_unit:
      mod->variant.header_unit.is_sys_include = FALSE;
      mod->variant.header_unit.suppress_macro_export = FALSE;
      mod->variant.header_unit.name = NULL;
      mod->variant.header_unit.resolved_header = NULL;
      break;
    case mk_unit_partition:
      mod->variant.unit_partition.is_internal = FALSE;
      mod->variant.unit_partition.name = NULL;
      mod->variant.unit_partition.unit = NULL;
      break;
    default_is_unexpected();
  }  /* switch */
  return mod;
}  /* alloc_module */


a_module_import_decl_ptr alloc_module_import_decl(void)
/*
Allocate an entry for a module-import or module-export declaration and return a
pointer to it.  The entry is allocated in the current memory region.
*/
{
  a_module_import_decl_ptr  entry;

  entry = alloc_il_of_type(a_module_import_decl);
  entry->next = NULL;
  entry->position = null_source_position;
  entry->module_name_position = null_source_position;
  entry->attributes = NULL;
  entry->module_info = NULL;
  entry->impl_unit_importing_self = FALSE;
  return entry;
}  /* alloc_module_import_decl */


a_scoped_expression_ptr alloc_scoped_expression(void)
/*
Allocate an entry for a_scoped_expression in the file scope memory region.
*/
{
  a_scoped_expression_ptr entry = alloc_il_of_type(a_scoped_expression);
  set_default_source_corresp(&entry->source_corresp);
  entry->expr = NULL;
  return entry;
}  /* alloc_scoped_expression */

#if DEBUG

unsigned long show_il_alloc_space_used(unsigned long grand_total)
/*
Display and return the amount of space used for various IL tables.
*/
{
  unsigned long num, size, total;

  db_space_used_header("IL table use:");

  for (unsigned iek = ((unsigned)iek_none) + 1; iek < (unsigned)iek_last;
       ++iek) {
    if (!is_tallied((an_il_entry_kind)iek)) {
      continue;
    }  /* if */
    if (il_allocation_counter[iek] == 0) {
      continue;
    }  /* if */
    db_space_used_nontype(il_entry_kind_names[iek], il_allocation_counter[iek],
                          sizeof_il_entry[iek]);
    if (iek == iek_expr_node) {
      /* Report some more specific numbers for expression nodes. */
      fprintf(f_debug, "%25s %8lu %8u %8lu\n", "(fs expr node)",
              num_fs_expr_nodes_allocated, (unsigned)sizeof(an_expr_node),
              (unsigned long)(num_fs_expr_nodes_allocated *
                              sizeof(an_expr_node)));

      unsigned long     num_avail_fs_nodes = 0;
      an_expr_node_ptr  node = avail_fs_nodes;
      for (; node != NULL; node = node->extra.next_avail) {
        num_avail_fs_nodes += 1;
      }  /* for */
      fprintf(f_debug, "%25s %8lu %8u %8lu\n", "(avail. fs expr node)",
              num_avail_fs_nodes, (unsigned)sizeof(an_expr_node),
              (unsigned long)(num_avail_fs_nodes*sizeof(an_expr_node)));
      fprintf(f_debug, "%25s %8lu %8u %8lu\n", "(fs rescan expr node)",
          num_rescan_fs_expr_nodes_allocated, (unsigned)sizeof(an_expr_node),
          (unsigned long)(num_rescan_fs_expr_nodes_allocated *
                          sizeof(an_expr_node)));
    }  /* if */
  }  /* for */
  db_space_used("string literal text", string_literal_text_space_allocated,
                char);
  db_space_used("IL entry prefix", num_il_entry_prefixes_allocated,
                an_il_entry_prefix);
  db_space_used_nontype("trans. unit copy addr.",
                        num_trans_unit_copy_address_pointers_allocated,
                        SPACE_FOR_TRANS_UNIT_COPY_ADDRESS_POINTER);
#if ORPHAN_PROCESSING_NEEDED
  db_space_used_nontype("fs orphan pointers", num_fs_orphan_pointers_allocated,
                        SPACE_FOR_FS_ORPHAN_POINTER);
#endif /* ORPHAN_PROCESSING_NEEDED */
#if ASM_SUPPORT_NEEDED
  db_space_used_other("asm function bodies",
                      asm_function_body_space_allocated, "");
#endif /* ASM_SUPPORT_NEEDED */

  db_space_used_total();

  return grand_total;
}  /* show_il_alloc_space_used */
#endif /* DEBUG */

#if CHECKING && defined(offsetof)

static void check_host_alignment_parameters(void)
/*
Make sure that the host alignment macros are set to the appropriate values.

If you want to use an alignment value larger than what is actually required,
you will need to modify or remove these tests.
*/
{
  int	expected;
  /*lint -esym(754,*::dummy)*/
  struct pointer_alignment_test {
    char	dummy;
    void	*ptr;
  };
  struct il_entry_prefix_alignment_test {
    char	dummy;
    an_il_entry_prefix
		prefix;
  };
  struct host_alignment_test {
    char	dummy;
    a_constant	constant;
  };
  expected = offsetof(struct host_alignment_test, constant);  /*lint !e413*/
  if (expected != HOST_ALIGNMENT_REQUIRED) {
    fprintf(f_error, "Expected HOST_ALIGNMENT_REQUIRED is %d\n", expected);
    internal_error(
    "check_host_alignment...: HOST_ALIGNMENT_REQUIRED set incorrectly");
  }  /* if */
  expected = offsetof(struct pointer_alignment_test, ptr);  /*lint !e413*/
  if (expected != HOST_POINTER_ALIGNMENT) {
    fprintf(f_error, "Expected HOST_POINTER_ALIGNMENT is %d\n", expected);
    internal_error(
    "check_host_alignment...: HOST_POINTER_ALIGNMENT set incorrectly");
  }  /* if */
  expected = offsetof(struct il_entry_prefix_alignment_test,
                      prefix);  /*lint !e413*/
  if (expected > HOST_IL_ENTRY_PREFIX_ALIGNMENT) {
    /* The specified alignment can be greater than or equal to the expected
       alignment.  This is required because the prefix alignment must be
       a multiple of the pointer alignment. */
    fprintf(f_error, "Expected HOST_IL_ENTRY_PREFIX_ALIGNMENT is %d\n",
            expected);
    internal_error(
    "check_host_alignment...: HOST_IL_ENTRY_PREFIX_ALIGNMENT set incorrectly");
  }  /* if */
}  /* check_host_alignment_parameters */
#endif /* CHECKING && defined(offsetof) */

void il_alloc_one_time_init(void)
/*
Do one-time initialization of variables related to the IL. (Variables
that need to be reinitialized with each new compilation are handled
in il_alloc_init.)
*/
{
  /* Set the default "routine name linkage", which is the value to which the
     routine_name_linkage field of a routine type supplement is initialized. */
  default_routine_name_linkage = C_mode() ?
                                   (a_name_linkage_kind)nlk_external :
                                   (a_name_linkage_kind)nlk_cplusplus_external;

  /* Set the default source correspondence variable to default values. */
  def_source_corresp.assoc_info = NULL;
  def_source_corresp.name = NULL;
#if NEED_NAME_MANGLING
  def_source_corresp.unmangled_name_or_mangled_encoding = NULL;
#endif /* NEED_NAME_MANGLING */
  def_source_corresp.trans_unit_corresp = NULL;
  def_source_corresp.parent_scope = NULL;
  def_source_corresp.enclosing_routine = NULL;
  def_source_corresp.module_entity = NULL;
  def_source_corresp.decl_position = null_source_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  def_source_corresp.decl_pos_info = NULL;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  def_source_corresp.name_references = NULL;
  /* access is set to "public" because "no access restriction" is the default
     for everything except class members.  For the latter the field must be
     set manually. */
  def_source_corresp.access = (an_access_specifier)as_public;
#if MICROSOFT_EXTENSIONS_ALLOWED
  def_source_corresp.assembly_access = (an_access_specifier)as_public;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* referenced is set TRUE because initially the entity is not associated
     with one in the source program.  All unassociated entities are assumed
     to be referenced (otherwise, they wouldn't be created).  This does away
     with the difficult job of setting the referenced flag in a lot of
     different places for unassociated entities. set_source_corresp resets
     the flag to FALSE for associated entities, for which the flag is then
     set to TRUE (for an actual reference) by record_symbol_reference. */
  def_source_corresp.referenced = TRUE;
#if MAINTAIN_NEEDED_FLAGS
  def_source_corresp.needed = FALSE;
#endif /* MAINTAIN_NEEDED_FLAGS */
  def_source_corresp.name_linkage = (a_name_linkage_kind)nlk_none;
  def_source_corresp.has_associated_pragma = FALSE;
  def_source_corresp.is_local_to_function = FALSE;
  def_source_corresp.parent_via_local_scope_ref = FALSE;
  def_source_corresp.is_class_member = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  def_source_corresp.has_associated_attribute = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NEED_NAME_MANGLING
  def_source_corresp.name_has_been_mangled = FALSE;
  def_source_corresp.mangled_name_cannot_be_included_in_other_name = FALSE;
  def_source_corresp.final_name_mangling_pending = FALSE;
  def_source_corresp.unnamed_entity_given_fabricated_name = FALSE;
#if GNU_EXTENSIONS_ALLOWED
  def_source_corresp.entity_marked = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
#endif /* NEED_NAME_MANGLING */
#if BACK_END_IS_CP_GEN_BE
  def_source_corresp.qualification_needed = FALSE;
  def_source_corresp.partially_hidden_by_microsoft_injected_class_name = FALSE;
  def_source_corresp.visible_as_unqualified_name = FALSE;
#endif /* BACK_END_IS_CP_GEN_BE */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  def_source_corresp.is_decl_after_first_in_comma_list = FALSE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if ONE_INSTANTIATION_PER_OBJECT
  def_source_corresp.static_used_by_instantiation = FALSE;
#if DUPLICATE_SPECIAL_STATICS_IN_INSTANTIATION_SLICES
  def_source_corresp.duplicate_static_in_instantiation_slices = FALSE;
#endif /* DUPLICATE_SPECIAL_STATICS_IN_INSTANTIATION_SLICES */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if MAINTAIN_NEEDED_FLAGS
  def_source_corresp.okay_to_walk_subtree_of_local_entity = FALSE;
#endif /* MAINTAIN_NEEDED_FLAGS */
  def_source_corresp.copied_from_secondary_trans_unit = FALSE;
  def_source_corresp.same_name_as_external_entity_in_secondary_trans_unit =
                                                                        FALSE;
  def_source_corresp.member_of_unknown_base = FALSE;
  def_source_corresp.qualified_unknown_base_member = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  def_source_corresp.member_of_unknown_super = FALSE;
  def_source_corresp.microsoft_identifier_used = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  def_source_corresp.marked_as_gnu_extension = FALSE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  def_source_corresp.is_deprecated_or_unavailable = FALSE;
  def_source_corresp.externalized = FALSE;
#if IA64_ABI
  def_source_corresp.on_mangling_substitution_list = FALSE;
#endif /* IA64_ABI */
  def_source_corresp.maybe_unused = FALSE;
#if RECORD_SCOPE_DEPTH_IN_IL
  def_source_corresp.scope_depth = NO_SCOPE_DEPTH;
#endif /* RECORD_SCOPE_DEPTH_IN_IL */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  def_source_corresp.source_sequence_entry = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if ONE_INSTANTIATION_PER_OBJECT
  def_source_corresp.per_instantiation_needed_flags = NULL;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  def_source_corresp.attributes = NULL;

#if CHECKING && defined(offsetof)
  /* Make sure the host alignment macros are set properly. */
  check_host_alignment_parameters();
#endif /* CHECKING && defined(offsetof) */

  /* Save static variables that are needed for precompiled headers */
  if (precompiled_header_processing_required) {
    STATIC_THREAD a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(avail_param_types),
      pch_saved_var_array_elem(avail_template_args),
      pch_saved_var_array_elem(available_local_constants),
      pch_saved_var_array_elem(avail_fs_nodes),
#if NEED_NAME_MANGLING
      pch_saved_var_array_elem(avail_constant_list_entries),
#endif /* NEED_NAME_MANGLING */
#if DEBUG
      pch_saved_var_array_elem(string_literal_text_space_allocated),
      pch_saved_var_array_elem(num_il_entry_prefixes_allocated),
      pch_saved_var_array_elem(num_fs_expr_nodes_allocated),
      pch_saved_var_array_elem(num_rescan_fs_expr_nodes_allocated),
      pch_saved_var_array_elem(num_trans_unit_copy_address_pointers_allocated),
#if ORPHAN_PROCESSING_NEEDED
      pch_saved_var_array_elem(num_fs_orphan_pointers_allocated),
#endif /* ORPHAN_PROCESSING_NEEDED */
#if ASM_SUPPORT_NEEDED
      pch_saved_var_array_elem(asm_function_body_space_allocated),
#endif /* ASM_SUPPORT_NEEDED */
      pch_array_saved_var_array_elem(il_allocation_counter),
#endif /* DEBUG */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  register_trans_unit_variable(file_scope_entry_prefix_size);
  register_trans_unit_variable(avail_param_types);
  register_trans_unit_variable(avail_template_args);
  register_trans_unit_variable(available_local_constants);
  register_trans_unit_variable(avail_fs_nodes);
#if NEED_NAME_MANGLING
  register_trans_unit_variable(avail_constant_list_entries);
#endif /* NEED_NAME_MANGLING */
  register_trans_unit_variable(file_scope_entry_prefix_alignment_offset);
}  /* il_alloc_one_time_init */


void compute_il_prefix_size(void)
/*
Compute the size of the IL entry prefix for file scope IL entries in this
translation unit.  On the initial call, also compute the prefix size
for non-file-scope entities.
*/
{
  size_t aligned_size;

  /* All entries allocated in the file scope have a prefix.  If we are
     doing orphan processing, they also have an orphan pointer.  In
     secondary translation units they also have a translation unit
     copy address pointer. */
  file_scope_entry_prefix_size =
            (is_primary_translation_unit ? 0
                                : SPACE_FOR_TRANS_UNIT_COPY_ADDRESS_POINTER) +
#if ORPHAN_PROCESSING_NEEDED
            SPACE_FOR_FS_ORPHAN_POINTER +
#endif /* ORPHAN_PROCESSING_NEEDED */
            SPACE_FOR_IL_ENTRY_PREFIX;
  /* Compute the additional space required so that the prefix is a multiple
     of the host alignment that is required. */
  aligned_size = file_scope_entry_prefix_size;
  do_host_alignment(&aligned_size);
  file_scope_entry_prefix_alignment_offset = aligned_size - 
                                             file_scope_entry_prefix_size;
  /* Set the prefix size to the aligned size. */
  file_scope_entry_prefix_size = aligned_size;
  /* Compute the size of the non-file-scope entry prefix and the associated
     alignment offset.  This is only computed once (during the first IL prefix
     computation of the primary translation unit); however, it is used for all
     translation units. */
  if (is_primary_translation_unit) {
    non_file_scope_entry_prefix_size = SPACE_FOR_IL_ENTRY_PREFIX;
    do_host_alignment(&non_file_scope_entry_prefix_size);
    non_file_scope_entry_prefix_alignment_offset =
                  non_file_scope_entry_prefix_size - SPACE_FOR_IL_ENTRY_PREFIX;
  }  /* if */
}  /* compute_il_prefix_size */


void il_alloc_trans_unit_init(void)
/*
Initialize static variables related to IL allocation.  These are variables
that need initialization for every (primary and secondary) translation unit.
*/
{
  avail_param_types = NULL;
  avail_template_args = NULL;
  available_local_constants = NULL;
  avail_fs_nodes = NULL;
#if NEED_NAME_MANGLING
  avail_constant_list_entries = NULL;
#endif /* NEED_NAME_MANGLING */
}  /* il_alloc_trans_unit_init */


void il_alloc_init(void)
/*
Initialize static variables related to IL allocation.  These are
initializations that are done for each compilation.
*/
{
  /* Static variables. */
  available_local_constants              = NULL;
#if DEBUG
  string_literal_text_space_allocated    = 0;
  num_il_entry_prefixes_allocated        = 0;
  num_fs_expr_nodes_allocated            = 0;
  num_rescan_fs_expr_nodes_allocated     = 0;
  num_trans_unit_copy_address_pointers_allocated = 0;
#if ORPHAN_PROCESSING_NEEDED
  num_fs_orphan_pointers_allocated       = 0;
#endif /* ORPHAN_PROCESSING_NEEDED */
#if ASM_SUPPORT_NEEDED
  asm_function_body_space_allocated      = 0;
#endif /* ASM_SUPPORT_NEEDED */
  for (unsigned iek = (unsigned)iek_none; iek < (unsigned)iek_last; ++iek) {
    il_allocation_counter[iek] = 0;
  }  /* for */
#endif /* DEBUG */
#if CHECKING
  local_constants_in_use                 = 0;
#endif /* CHECKING */
}  /* il_alloc_init */
#endif /* !STANDALONE_UTILITY_PROGRAM */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

