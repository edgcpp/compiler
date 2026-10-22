/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

il_write.c -- Write the intermediate language to a file.

*/

#include "basic_hdrs.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Everything in this file has to do with writing the IL file. */
#if IL_SHOULD_BE_WRITTEN_TO_FILE

#include <errno.h>

#if !ORPHAN_PROCESSING_NEEDED
 #error -- ORPHAN_PROCESSING_NEEDED must be set if IL writing is needed.
#endif /* !ORPHAN_PROCESSING_NEEDED */

/* Header files common to all files. */
#include "fe_common.h"

/* Additional files. */
#include "il_file.h"
#include "il_walk.h"
#include "il_write.h"

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

STATIC_THREAD a_boolean
		writing_file_scope_il;
			/* TRUE if writing the file-scope IL, FALSE if writing
			   IL for a function scope. */

STATIC_THREAD a_file_position
		il_header_pos;
			/* Position where the IL header was written. */

#if ALTERNATE_IL_FILE_FORMAT
STATIC_THREAD an_il_entry_number
		entry_numbers_array[(int)iek_last],
		fs_entry_numbers_array[(int)iek_last];
			/* Arrays giving the number of entries of each
			   kind, for the current scope and the file scope.
			   For string entries, the total size of strings
			   of that kind.  Used to track the number of entries
			   and also to assign entry numbers.  Each table
			   type has entry numbers starting from 1. */
STATIC_THREAD an_il_entry_number
		max_entry_number;
			/* Maximum allowed entry number, used for overflow
			   checking. */
#if CHECKING && DEBUG
/* Debugging variables used to locate a missing (unwritten) IL entry
   by entry kind and entry number within that kind in a specific memory
   region.  By setting the variables trace_memory_region_number,
   trace_entry_kind, and trace_entry_number after loading il_write.c into
   the debugging environment, execution can be intercepted (by setting a
   breakpoint on trace_entry_assignment) when the entry number is assigned
   for the specified IL entry.

     1. set two breakpoints:
        - trace_entry_assignment
        - main (or some other early routine)
     2. run the test compilation
     3. when the debugger stops in "main" set the 3 tracing variable values
        by calling trace_entry with the appropriate arguments -- e.g.,
        "call trace_entry(265, 22, 1)" if the diagnostic from a previous
        run was:
          IL entry write-read difference: region number 265
          entry kind = 22 (object-lifetime), written = 1, read = 0
          missing entry = 1
     4. continue
     5. breakpoint in trace_entry_assignment is reached

   The contents of the IL entry and its position on the IL tree as shown
   by the stack trace can help to determine the cause of the error.
*/
STATIC_THREAD a_memory_region_number
		trace_memory_region_number;
			/* Memory region of the omitted IL entry. */
STATIC_THREAD an_il_entry_kind
		trace_entry_kind;
			/* IL entry kind of the omitted IL entry. */
STATIC_THREAD an_il_entry_number
		trace_entry_number;
			/* IL entry number of the omitted IL entry. */
STATIC_THREAD a_memory_region_number
		trace_region_being_written;
			/* Variable used by write_memory_region()
			   to record the memory region number currently
			   being written. */

static void trace_entry_assignment(void)
/*
This routine is called when the IL entry designated by the variables
trace_entry_number, trace_memory_region_number and trace_entry_kind is
assigned its entry number.  It is often useful to set a debugger breakpoint
on this routine.
*/
{
  (void)fprintf(f_debug,
                "Entry number %ld in region %ld (kind = %ld: %s).\n",
                (long)trace_entry_number, (long)trace_memory_region_number,
                (long)trace_entry_kind,
                il_entry_kind_names[(long)trace_entry_kind]);
}  /* trace_entry_assignment */


void trace_entry(a_memory_region_number memory_region_number,
                 an_il_entry_kind       entry_kind,
                 an_il_entry_number     entry_number)
/*
This function is meant to be called from a debugger to set the values of
trace_memory_region_number, trace_entry_kind and trace_entry_number with
a single debugger command.
*/
{
  trace_memory_region_number = memory_region_number;
  trace_entry_kind = entry_kind;
  trace_entry_number = entry_number;
}  /* trace_entry */

#endif /* CHECKING && DEBUG */
#endif /* ALTERNATE_IL_FILE_FORMAT */


#if ALTERNATE_IL_FILE_FORMAT
#if CHECKING && DEBUG
static void display_il_entry_kind_and_ptr (
				char             *entry_ptr,
				an_il_entry_kind entry_kind)
/*
Print additional diagnostic information about the IL entry that is
triggering an internal error.
*/
{
  (void)fprintf(f_debug, 
                "IL info: entry kind =%3ld (%s), \n",
                (long)entry_kind, il_entry_kind_names[(int)entry_kind]);
#if defined(_WIN64)
  /* Avoid data loss in casting the pointer to unsigned long. */
  (void)fprintf(f_debug, "         entry_ptr = 0x%p\n", entry_ptr);
#else /* !defined(_WIN64) */
  (void)fprintf(f_debug, "         entry_ptr = 0x%lx\n",
                (unsigned long)entry_ptr);
#endif /* defined(_WIN64) */
  (void)fprintf(f_debug, "         memory region = %4ld\n",
                (long)trace_region_being_written);
}  /* display_il_entry_kind_and_ptr */

#endif /* CHECKING && DEBUG */

static an_il_entry_prefix *assign_entry_number(
                                       char                    *entry_ptr,
                                       an_il_entry_kind        entry_kind,
                                       a_boolean               is_string_entry,
                                       sizeof_t                entry_length,
                                       an_encoded_entry_number *encoded_number)
/*
Assign an entry number to the entry pointed to entry_ptr if it does
not already have one.  Return a pointer to the entry prefix.
The entry is of kind entry_kind, and if it is a string, has length as
given by entry_length.  Return in *encoded_number the entry number in
encoded form.
*/
{
  an_il_entry_number *count_ptr, entry_number;
  a_boolean          is_file_scope_entry;
  an_il_entry_prefix *epp;
  an_il_entry_number num_entries = 1;

  /* Determine the address of the entry prefix preceding the entry. */
  epp = &il_entry_prefix_of(entry_ptr);
  /* String entries can be referenced from several places, possibly in
     different regions.  In particular, there can be file-scope strings
     referenced from function-scope entries.  In those cases, even if
     we put the string on the orphan list (which we don't), we would
     not have the length of the string entry (at least for iek_string_text)
     when we process the orphan.  To deal with this problem, strings are
     always considered honorary members of the memory region from which they
     are referenced, which means multiple copies may be written out, and
     a string that exists in one copy before the IL write may exist in
     multiple copies (in different memory regions) after the IL read. */
  if (is_string_entry && epp->entry_number != 0) {
    a_boolean out_of_date = FALSE;
    /* A string entry with an already-assigned entry number.  See if the
       entry number should be cleared and reassigned. */
    if (writing_file_scope_il) {
      /* We're writing the file scope, so an entry number in a function
         scope is out of date. */
      if (!epp->file_scope) out_of_date = TRUE;
    } else {
      /* We're writing a function scope. */
#if CHECKING
      /* An entry number in the file scope is impossible (we should only
         assign such a number while writing the file scope, and we only do
         that after all the function scopes have been written). */
      if (epp->file_scope) {
#if DEBUG
/* GCOVR_EXCL_START */
        display_il_entry_kind_and_ptr(entry_ptr, entry_kind);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
        internal_error("assign_entry_number: file-scope num in func scope");
      }  /* if */
#endif /* CHECKING */
      /* If the entry has already been written, assume that it was assigned
         and written in a previous function scope.  This assumes that a
         string entry will not be referenced twice within one function scope
         (note that multiple references within the file scope ARE possible,
         like in a_source_file references to file names). */
      /* This is the reason for the existence of the entry_written flag. */
      if (epp->entry_written) out_of_date = TRUE;
    }  /* if */
    if (out_of_date) {
      /* The entry number is out of date.  Clear it. */
      epp->entry_number = 0;
      epp->entry_written = FALSE;
    }  /* if */
  }  /* if */
  /* Only assign a number if the entry does not already have one.  A zero
     means the entry number has not been assigned yet. */
  if (epp->entry_number == 0) {
    /* Use the next available number from the array of entry counts.  Use
       the file-scope array if the entry is in the file scope, the
       function-scope array otherwise.  Note that we can encounter a 
       file-scope entry while scanning a function scope, but not the
       other way around.  Special case: a string entry is considered to
       be in the region of the entry that points to it (see comment above). */
    if (is_string_entry) {
      /* A string entry is considered to be in the region of the entry
         that points to it (see the comment above).  Use the region
         currently being written, not epp->file_scope, to choose the
         entry-number table.  A file-scope string referenced from a
         function scope must be numbered in that function's tables even
         though it remains allocated in file-scope memory. */
      is_file_scope_entry = writing_file_scope_il;
    } else if (writing_file_scope_il) {
      /* Writing the file scope, so only file-scope items should appear. */
      is_file_scope_entry = TRUE;
#if CHECKING
      if (!epp->file_scope) {
#if DEBUG
/* GCOVR_EXCL_START */
        display_il_entry_kind_and_ptr(entry_ptr, entry_kind);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
        internal_error(
         "assign_entry_number: non-file-scope ptr referenced from file scope");
      }  /* if */
#endif /* CHECKING */
    } else {
      /* Writing a function scope, so both file-scope and function-scope
         items may appear. */
      is_file_scope_entry = epp->file_scope;
    }  /* if */
    count_ptr = &(is_file_scope_entry ?
                fs_entry_numbers_array : entry_numbers_array)[(int)entry_kind];
    if (is_string_entry) {
      /* For string cases, the entry numbers are byte offsets (+1) into a
         conceptual string area.  The entry is preceded by the prefix.
         It must be suitably aligned, so the current position is adjusted
         upward if necessary to ensure alignment. */
      /* Note that if we were to write the file on one system and read
         it on another system with a different alignment requirement we
         might have a problem. */
      do_host_alignment(count_ptr);
      *count_ptr += SPACE_FOR_IL_ENTRY_PREFIX;
      num_entries = (an_il_entry_number)entry_length;
      /* Note that no orphan pointer is allocated for strings. */
    }  /* if */
    /* Check for overflow of the entry number field.  In practice, the
       field is probably close to a 32-bit field, and this should not happen
       even for very large programs. */
    if (*count_ptr > max_entry_number - num_entries) {
      catastrophe(ec_program_too_large);
    }  /* if */
    /* Use the next entry number for this entry. */
    copy_to_bitfield((*count_ptr + 1), epp->entry_number,
                     BITS_IN_ENTRY_NUMBER);
    /* For string entries, overwrite epp->file_scope even when that
       changes the value set at allocation time.  During IL file output
       the prefix bit records the memory region kind (file scope vs.
       function scope) for which the entry number was assigned in this
       write, not where the string is stored in memory.  That value must
       remain set until il_read (or a later write pass) reassigns the
       entry, because subsequent pointer remapping, FUNC_ENTRY_NUMBER_BIT
       encoding, and the out-of-date entry-number logic above all consult
       epp->file_scope. */
    epp->file_scope = is_file_scope_entry;
    /* Increment the table entry by the right number of logical entries. */
    *count_ptr += num_entries;
  }  /* if */
  entry_number = epp->entry_number;
  /* Return the encoded form of the entry number in *encoded_number. */
  *encoded_number = entry_number;
  if (!epp->file_scope) *encoded_number |= FUNC_ENTRY_NUMBER_BIT;
#if CHECKING && DEBUG
  /* Stop if the entry being examined is the one we're looking for. */
  if (entry_kind == trace_entry_kind &&
      entry_number == trace_entry_number &&
      ((trace_memory_region_number == FILE_SCOPE_REGION_NUMBER) ?
        epp->file_scope :
        (trace_region_being_written == trace_memory_region_number))) {
    trace_entry_assignment();
  }  /* if */
#endif /* CHECKING && DEBUG */
  return epp;
}  /* assign_entry_number */


static char *remap_ptr_to_entry_number(char             *entry_ptr,
                                       an_il_entry_kind entry_kind)
/*
Convert entry_ptr, a pointer to an IL entry of type entry_kind, to the
corresponding encoded entry number, and return that number cast to "char *".
*/
{
  an_encoded_entry_number encoded_number;
  an_il_entry_prefix      *epp;

  if (entry_ptr == NULL) {
    /* A NULL pointer is represented by a zero encoded entry number. */
    encoded_number = 0;
  } else {
    /* Find the entry prefix preceding the entry. */
    epp = &il_entry_prefix_of(entry_ptr);
    check_assertion_str(!epp->secondary_trans_unit,
                 "remap_ptr_to_entry_number: pointer in secondary trans unit");
    /* Test for entry number already assigned.  This test is mostly for
       speed, since most entries will have numbers assigned by the
       time we get here. */
    if (epp->entry_number == 0) {
      /* The entry number has not been assigned yet. */
      a_boolean assign_number = TRUE;
#if CHECKING
      if (is_string_entry_kind(entry_kind)) {
        /* All string entries should have entry numbers already.  See
           write_entry.  We can't handle them here because we don't have the
           length. */
#if DEBUG
/* GCOVR_EXCL_START */
        display_il_entry_kind_and_ptr(entry_ptr, entry_kind);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
        internal_error("remap_ptr_to_entry_number: string entry");
      }  /* if */
#endif /* CHECKING */
      if (entry_kind == iek_type &&
          ((a_type_ptr)entry_ptr)->kind == (a_type_kind)tk_template_param) {
        /* Except for certain special cases, template parameter types should
           not leak out of the front end. */
        a_type_ptr tp = (a_type_ptr)entry_ptr;
        if (prototype_instantiations_in_il) {
          /* If prototype instantiations are recorded in the IL, it's okay
             for template parameters to be written. */
        } else if (tp->variant.template_param.kind ==
                                      (a_template_param_type_kind)tptk_param &&
                   tp->variant.template_param.extra_info->coordinates.depth ==
                                                     AUTO_TYPE_NESTING_DEPTH) {
          /* The "auto" specifier is represented by a special template
             parameter type; it's okay for that type to be written. */
        } else {
          /* Not one of the special cases. */
          assign_number = FALSE;
        }  /* if */
      }  /* if */
      if (!assign_number) {
        /* Just write a null pointer and don't assign an entry number. */
        encoded_number = 0;
      } else {
        /* Assign an entry number. */
        (void)assign_entry_number(entry_ptr, entry_kind,
                                  /*is_string_entry=*/FALSE, (sizeof_t)0,
                                  &encoded_number);
      }  /* if */
    } else {
      /* The entry already has an entry number. */
      /* Construct the encoded form of the entry number. */
      encoded_number = epp->entry_number;
      if (!epp->file_scope) encoded_number |= FUNC_ENTRY_NUMBER_BIT;
    }  /* if */
  }  /* if */
  /* Return the encoded entry number converted to "char *". */
  /* For CodeCenter -- suppress warning about bad pointer.  Version 3.0
     warning number. */
  /*SUPPRESS 80*/
  return ((char *)(ptrdiff_t)encoded_number);
}  /* remap_ptr_to_entry_number */
#endif /* ALTERNATE_IL_FILE_FORMAT */


void start_il_file(void)
/*
Write the initial information to the IL file, if there is one.
*/
{
  a_memory_region_number zero_region_number = 0;
  a_function_def_number  zero_function_def_number = 0;
  a_file_position        zero_file_position = 0;

  /* Note that the file was opened already by open_il_file.  f_il_output
     remains NULL if no IL file is being written, as when preprocessing
     only is being done. */
  if (f_il_output != NULL) {
    /* Write a string that identifies the file as an IL file.  The front
       end version number is inserted into the string. */
    an_il_file_magic_string magic_string = current_il_magic_string();

    print(magic_string, f_il_output, /*end=*/"");
    /* Leave space for the number of regions, the number of function definition
       entries, the offset to the file index, the offset to the file-scope
       region, and the il_header struct.  These will be filled in when the
       information is known at the end of file (see finish_il_file and
       write_memory_region). */
    (void)fwrite((char *)&zero_region_number, sizeof(zero_region_number), 1,
                 f_il_output);
    (void)fwrite((char *)&zero_function_def_number,
                 sizeof(zero_function_def_number), 1,
                 f_il_output);
    (void)fwrite((char *)&zero_file_position, sizeof(zero_file_position), 1,
                 f_il_output);
    (void)fwrite((char *)&zero_file_position, sizeof(zero_file_position), 1,
                 f_il_output);
    il_header_pos = get_file_position(f_il_output);
    (void)fwrite((char *)&il_header, sizeof(il_header), 1, f_il_output);
    /* Leave space for the orphaned_file_scope_il_entries array. */
    (void)fwrite((char *)orphaned_file_scope_il_entries,
                 sizeof(orphaned_file_scope_il_entries), 1, f_il_output);
  }  /* if */
#if ALTERNATE_IL_FILE_FORMAT
  /* Clear the array giving the count of entries of each kind for the
     file scope. */
  { int int_entry_kind;
    for (int_entry_kind = (int)iek_none;
         int_entry_kind < (int)iek_last;
         int_entry_kind++) {
      fs_entry_numbers_array[int_entry_kind] = 0;
    }  /* if */
  }
  /* We need a constant that is at least as big as the largest size of
     a (non-string) IL entry.  If RECORD_MACRO_INVOCATIONS is TRUE, that's
     probably the size of a macro invocation record block.  Otherwise, we
     take a guess by adding the sizes of two of the largest entries, and
     check here that we're okay. */
#if RECORD_MACRO_INVOCATIONS
#define MAX_SIZEOF_IL_ENTRY sizeof(a_macro_invocation_record_block)
#else /* !RECORD_MACRO_INVOCATIONS */
#ifdef UNION_AS_STRUCT
/* The a_constant structure is larger than a_scope in this configuration. */
#define MAX_SIZEOF_IL_ENTRY (sizeof(a_constant)+sizeof(a_routine))
#else /* !ifdef UNION_AS_STRUCT */
#define MAX_SIZEOF_IL_ENTRY (sizeof(a_scope)+sizeof(a_routine))
#endif /* ifdef UNION_AS_STRUCT */
#endif /* RECORD_MACRO_INVOCATIONS */
#if CHECKING
  { int int_entry_kind;
    for (int_entry_kind = (int)iek_none+1;
         int_entry_kind < (int)iek_last;
         int_entry_kind++) {
      if (sizeof_il_entry[int_entry_kind] > MAX_SIZEOF_IL_ENTRY) {
        internal_error("start_il_file: MAX_SIZEOF_IL_ENTRY is defined wrong");
      }  /* if */
    }  /* if */
  }
#endif /* CHECKING */
#if ENTRY_NUMBER_SHARES_BITS_IN_PREFIX
  /* Verify that BITS_IN_ENTRY_NUMBER is set correctly. */
  { int num_bits = BITS_IN_ENTRY_NUMBER;
#if CHECKING
    if (num_bits > (int)sizeof(an_il_entry_number)*CHAR_BIT ||
        num_bits <= 0) { /*lint !e774 !e845*/
      internal_error("start_il_file: BITS_IN_ENTRY_NUMBER is set wrong");
    }  /* if */
#endif /* CHECKING */
    /* Compute the maximum valid entry number. */
    if (num_bits == sizeof(an_il_entry_number)*CHAR_BIT) { /*lint !e774*/
      /* The entry number field is the same size as an_il_entry_number. */
      max_entry_number = ~(an_il_entry_number)0;  /* All "1" bits. */
    } else {
      /* The entry number field is smaller than an_il_entry_number. */
      /* Make a bit mask of length BITS_IN_ENTRY_NUMBER. */
      max_entry_number = ((an_il_entry_number)1 << BITS_IN_ENTRY_NUMBER) - 1;
    }  /* if */
  }
#else /* !ENTRY_NUMBER_SHARES_BITS_IN_PREFIX */
  max_entry_number = ~(an_il_entry_number)0;  /* All "1" bits. */
#endif /* ENTRY_NUMBER_SHARES_BITS_IN_PREFIX */
#if CHECKING
  /* Make sure the entry_number field in the prefix can contain the maximum
     value computed. */
  { an_il_entry_prefix dummy_prefix;

    copy_to_bitfield(max_entry_number, dummy_prefix.entry_number,
                     BITS_IN_ENTRY_NUMBER);
    if (dummy_prefix.entry_number != max_entry_number) {
      internal_error("start_il_file: prefix entry_number is defined wrong");
    }  /* if */
  }
  check_assertion(sizeof(an_encoded_entry_number) >=
                  sizeof(an_il_entry_number));
  check_assertion(sizeof(an_encoded_entry_number) <= sizeof(char*));
#endif /* CHECKING */
#endif /* ALTERNATE_IL_FILE_FORMAT */
}  /* start_il_file */


void finish_il_file(void)
/*
Finish writing the IL file, if there is one.
*/
{
  a_memory_region_number end_flag = NULL_region_number;
  a_file_position        index_pos;
  sizeof_t               index_size;

  if (f_il_output != NULL) {
    /* If the intermediate language is being written to a file, write the
       end of the file (no-more-regions flag plus the file index), then
       go back and fill in the information at the beginning of the file.
       Recall that the beginning of the file looks like:
         magic string that identifies an IL file (already written properly)
         number of regions
         file offset to the file index table
         file offset to the start of the file scope region
         il_header
         orphaned_file_scope_il_entries array
       and that zeroes were written in all but the first item
       when the file was begun (see start_il_file).  il_header and
       the orphaned_file_scope_il_entries array were written again by
       write_memory_region when the file-scope memory region was written.
    */
    /* Write a zero region number that indicates the end of the list
       of regions. */
    (void)fwrite((char *)&end_flag, sizeof(end_flag), 1, f_il_output);
    /* Write the file index table at the end of the file. */
    index_pos = get_file_position(f_il_output);
    index_size = (sizeof_t)(unsigned)highest_used_region_number *
                 sizeof(a_file_position);
    (void)fwrite((char *)&index_for_il_file[FILE_SCOPE_REGION_NUMBER],
                 size_t_arg(index_size), 1, f_il_output);
    /* Seek back to just after the "magic" string at the beginning of the
       file. */
    if (set_file_position(f_il_output, (long)LEN_IL_FILE_MAGIC_STRING,
                          SEEK_SET) != 0) {
      file_write_error(ec_intermediate_language_1, errno);
    }  /* if */
    /* Write the number of regions. */
    (void)fwrite((char *)&highest_used_region_number,
                 sizeof(highest_used_region_number), 1, f_il_output);
    /* Write the number of function definition entries. */
    (void)fwrite((char *)&highest_used_function_def_number,
                 sizeof(highest_used_function_def_number), 1, f_il_output);
    /* Write the file offset for the file index table. */
    (void)fwrite((char *)&index_pos, sizeof(index_pos), 1, f_il_output);
    /* Write the file offset for the file-scope memory region. */
    (void)fwrite((char *)&index_for_il_file[FILE_SCOPE_REGION_NUMBER],
		 sizeof(a_file_position), 1, f_il_output);
    /* Flush the IL file and check for errors on it. */
    if (fflush(f_il_output) || ferror(f_il_output)) {
      file_write_error(ec_intermediate_language_2, errno);
    }  /* if */
  }  /* if */
}  /* finish_il_file */


void close_il_output_file(void)
/*
Close the open IL output file.  If il_file_name is NULL, the IL file is
a temporary_file.
*/
{
  if (f_il_output != NULL) {
#if BACK_END_SHOULD_BE_CALLED
    /* If the intermediate language file is a temp file, delete it. */
    if (il_file_name == NULL) {
      close_temp_file(f_il_output);
    } else {
#endif /* BACK_END_SHOULD_BE_CALLED */
      /* The  intermediate language file is being written to an external
         file; close it. */
      if (fclose(f_il_output)) {
        file_write_error(ec_intermediate_language_3, errno);
      }  /* if */
#if BACK_END_SHOULD_BE_CALLED
    }  /* if */
#endif /* BACK_END_SHOULD_BE_CALLED */
    f_il_output = NULL;
  }  /* if */
}  /* close_il_output_file */


void cancel_il_file(void)
/*
Called on detection of any errors.  Stops further writing of the IL file.
*/
{
  if (f_il_output != NULL) {
    /* Close and delete the IL file. */
#if BACK_END_SHOULD_BE_CALLED
    if (il_file_name == NULL) {
      close_temp_file(f_il_output);
    } else {
#endif /* BACK_END_SHOULD_BE_CALLED */
      (void)fclose(f_il_output);
      delete_file(il_file_name);
#if BACK_END_SHOULD_BE_CALLED
    }  /* if */
#endif /* BACK_END_SHOULD_BE_CALLED */
    /* Prevent further writing to the IL file. */
    f_il_output = NULL;
    il_file_name = NULL;
  }  /* if */
}  /* cancel_il_file */


#if ALTERNATE_IL_FILE_FORMAT
static void write_entry(char             *entry_ptr,
                        an_il_entry_kind entry_kind,
                        sizeof_t         entry_length)
/*
Called during IL tree traversal to write an entry to the IL file.  Called
directly for string entries, and from write_nonstring_entry for all
other entries.  entry_kind indicates the kind of entry, and entry_length
its length.
*/
{
  a_byte             byte_entry_kind;
  an_encoded_entry_number
                     encoded_number;
  an_il_entry_prefix *epp;
  a_boolean          is_string_entry = is_string_entry_kind(entry_kind);
  char               entry_copy[MAX_SIZEOF_IL_ENTRY];

  if (entry_length == 0) {
    /* Zero-length arrays initialized with string literals can lead to
       zero-length entries.  E.g., in GNU C mode:
         char x[0] = { "" };
       Deal with them as one-byte entries to avoid problems. */
    check_assertion(is_string_entry);
    entry_length = 1;
  }  /* if */
  /* Give this entry an entry number if it does not have one yet.  The entry
     number is stored just ahead of the entry. */
  epp = assign_entry_number(entry_ptr, entry_kind, is_string_entry,
                            entry_length, &encoded_number);

  /* Check the "already written" flag in the entry number.  For strings,
     that's okay, since the same string can be pointed to from different
     places.  For non-string entries, it indicates an internal error. */
  if (epp->entry_written) {
#if CHECKING
    if (!is_string_entry) {
#if DEBUG
/* GCOVR_EXCL_START */
      display_il_entry_kind_and_ptr(entry_ptr, entry_kind);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
      internal_error("write_entry: non-string entry already written");
    }  /* if */
#endif /* CHECKING */
    goto end_of_routine;
  }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 5) {
    fprintf(f_debug,
           "Writing IL entry to file: kind = %d, number = %lu, length = %lu\n",
           (int)entry_kind, (unsigned long)encoded_number,
           (unsigned long)entry_length);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  /* For non-string entries, the pointers must be remapped.  Make a copy
     so the original pointers can be restored after the write. */
  /* Note that entry_copy may not be suitably aligned for direct processing,
     so we have to copy, change the pointers in place, then copy back.
     We can't just change the copy. */
  if (!is_string_entry) {
    (void)memcpy(entry_copy, entry_ptr, size_t_arg(entry_length));
    remap_pointers_in_il_entry(entry_ptr, entry_kind,
                               remap_ptr_to_entry_number,
                               remap_ptr_to_entry_number,
                               /*clear_fe_pointers=*/TRUE);
  }  /* if */
  /* Write the entry kind. */
  byte_entry_kind = (a_byte)entry_kind;
  (void)fwrite((char *)&byte_entry_kind, sizeof(byte_entry_kind), 1,
               f_il_output);
  /* Write the entry number. */
  (void)fwrite((char *)&encoded_number, sizeof(encoded_number), 1,
               f_il_output);
  /* For strings, write the length. */
  if (is_string_entry) {
    (void)fwrite((char *)&entry_length, sizeof(entry_length), 1, f_il_output);
  } else {
    /* For non-string entries in the file scope memory region, write the
       orphaned file scope IL entry chain pointer. */
    if (writing_file_scope_il) {
      /* Must remap the orphaned file scope IL entry chain pointer.  Use
         a local copy of the pointer. */
      char *orphan_ptr = remap_ptr_to_entry_number(
                                  fs_orphan_pointer_of(entry_ptr), entry_kind);

      (void)fwrite((char *)&orphan_ptr, sizeof(orphan_ptr), 1, f_il_output);
    }  /* if */
  }  /* if */
  /* Write the entry itself.  Note this can trigger harmless errors in memory
     checkers (e.g., valgrind) as the entry may contain bytes that are
     uninitialized (because the entry never initialized them/doesn't use
     them). */
  if (fwrite(entry_ptr, size_t_arg(entry_length), 1, f_il_output) != 1) {
    /* Error on write.  This check supplements the check done when the
       file is closed. */
    file_write_error(ec_intermediate_language_4, errno);
  }  /* if */
  if (!is_string_entry) {
    /* Restore the original pointers. */
    (void)memcpy(entry_ptr, entry_copy,
                 size_t_arg(entry_length)); /*lint !e645*/
  }  /* if */
  /* Set the "entry written" flag. */
  epp->entry_written = TRUE;
end_of_routine:;
}  /* write_entry */


static void write_nonstring_entry(char             *entry_ptr,
                                  an_il_entry_kind entry_kind)
/*
Called during IL tree traversal to write a non-string entry to the IL file.
*/
{
  write_entry(entry_ptr, entry_kind, sizeof_il_entry[(int)entry_kind]);
}  /* write_nonstring_entry */
#endif /* ALTERNATE_IL_FILE_FORMAT */


void write_memory_region(a_memory_region_number region_number)
/*
Write the indicated memory region to the file f_il_output.
*/
{
  char            il_header_copy[sizeof(il_header)];
  a_file_position end_pos;
#if ALTERNATE_IL_FILE_FORMAT
  char		  orphaned_file_scope_il_entries_copy
                             [sizeof(orphaned_file_scope_il_entries)];
#endif /* ALTERNATE_IL_FILE_FORMAT */

  db_enter(2, "write_memory_region");
  /* Check that the file should in fact be created, which is indicated
     by its having been opened.  If the front end is just doing
     preprocessing, for example, no intermediate language file is 
     written. */
  if (f_il_output != NULL) {
#if DEBUG
/* GCOVR_EXCL_START */
    if (debug_level >= 2) {
      fprintf(f_debug, "Writing out memory region %lu\n",
                       (unsigned long)(unsigned)region_number);
    }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
#if ALTERNATE_IL_FILE_FORMAT
    /* Using alternate IL file format. */
    /* The information written for a region is:
         region number
         array giving, for each entry type, the number of entries of
           that type
         for each entry ----|entry type
                            |entry number
                            |entry length (only for string entries)
                            |orphaned IL entry link (file scope only)
                            |the entry itself
         zero byte indicating the end of the list.
    */
#else /* !ALTERNATE_IL_FILE_FORMAT */
    /* Using standard IL file format. */
    /* The information written for a region is:
         region number
         memory address that the first block header came from
         memory address of the primary scope entry
         total size in bytes of the information following (all blocks)
         for each block ----|header
                            |block itself
       The header/block sequence is written in such a way that it can be
       read back in with a single read of the indicated total size, and
       so that the headers and blocks will end up properly aligned if
       read in to a properly aligned area.  The header, the start of
       the data in the block, and the next available location are
       already guaranteed to be correctly aligned (see alloc_in_region). */
#endif /* ALTERNATE_IL_FILE_FORMAT */
    check_assertion_str(index_for_il_file[region_number] == 0,
                        "write_memory_region: region already written");
    /* Remember the current file position as the position of the region
       by saving it in the file index. */
    index_for_il_file[region_number] = get_file_position(f_il_output);
    /* Write the region number. */
    (void)fwrite((char *)&region_number,
                 sizeof(region_number), 1, f_il_output);
    writing_file_scope_il = (region_number == FILE_SCOPE_REGION_NUMBER);
#if ALTERNATE_IL_FILE_FORMAT
    /* Alternate file format. */
#if CHECKING && DEBUG
    trace_region_being_written = region_number;
#endif /* CHECKING && DEBUG */
    { a_file_position  count_array_pos;
      int              int_entry_kind;
      char             zero = 0;
      an_encoded_entry_number
                       encoded_number;

      /* For a function scope, clear the array of entry counts. */
      if (!writing_file_scope_il) {
        for (int_entry_kind = (int)iek_none;
             int_entry_kind < (int)iek_last;
             int_entry_kind++) {
          entry_numbers_array[int_entry_kind] = 0;
        }  /* if */
      }  /* if */
      /* Write the array of entry counts.  At this point, the write is only
         to reserve the proper amount of space.  The correct values will
         only be known when the region has been fully processed.  Since
         we are writing to fill space, the distinction between the file-scope
         array and the function array is unimportant. */
      count_array_pos = get_file_position(f_il_output);
      /* The first entry of the array is skipped. */
      (void)fwrite((char *)(entry_numbers_array+1),
                   sizeof(entry_numbers_array)-sizeof(an_il_entry_number), 1,
                   f_il_output);
      /* Give the primary scope entry of the region the number 1. */
      (void)assign_entry_number(
                           (char *)il_header.region_scope_entry[region_number],
                           iek_scope, /*is_string_entry=*/FALSE,
                           (sizeof_t)0, &encoded_number);
      /* Walk the IL tree for the region, and write the entries. */
      if (writing_file_scope_il) {
        /* The memory region is the file scope region. */
        walk_file_scope_il(write_nonstring_entry, write_entry,
                           (a_remap_function_ptr)NULL,
                           (a_remap_function_ptr)NULL,
                           (a_walk_termination_test_function_ptr)NULL,
                           /*clear_fe_pointers=*/FALSE);
      } else {
        /* The memory region is a function scope. */
        walk_routine_scope_il(region_number,
                              write_nonstring_entry, write_entry,
                              (a_remap_function_ptr)NULL,
                              (a_remap_function_ptr)NULL,
                              (a_walk_termination_test_function_ptr)NULL,
                              /*clear_fe_pointers=*/FALSE);
      }  /* if */
      /* Write a zero entry kind, to indicate the end of the list of
         entries. */
      (void)fwrite(&zero, 1, 1, f_il_output);
      /* Save the end-of-file position. */
      end_pos = get_file_position(f_il_output);
      /* Go back and write the array of entry counts.  This time it matters
         which one we write. */
      if (set_file_position(f_il_output, count_array_pos, SEEK_SET) != 0) {
        file_write_error(ec_intermediate_language_5, errno);
      }  /* if */
      /* The first entry of the array is skipped. */
      (void)fwrite((char *)&(writing_file_scope_il ?
                              fs_entry_numbers_array : entry_numbers_array)[1],
                   sizeof(entry_numbers_array)-sizeof(an_il_entry_number), 1,
                   f_il_output);
      /* Reposition the file at the end to leave it properly positioned
         for future writes.  SEEK_END is not used because ANSI doesn't 
         guarantee it for binary files. */
      if (set_file_position(f_il_output, end_pos, SEEK_SET) != 0) {
        file_write_error(ec_intermediate_language_6, errno);
      }  /* if */
    }
#else /* !ALTERNATE_IL_FILE_FORMAT */
    /* Standard IL file format. */
    { sizeof_t               total_bytes;
      a_mem_block_header_ptr hdr;

      if (!writing_file_scope_il) {
        /* The memory region is a function scope.  Walk the IL tree to
           catch all references to file scope orphans and build the
           right orphan list. */
        walk_routine_scope_il(region_number,
                              (an_entry_process_function_ptr)NULL,
                              (a_string_entry_process_function_ptr)NULL,
                              (a_remap_function_ptr)NULL,
                              (a_remap_function_ptr)NULL,
                              (a_walk_termination_test_function_ptr)NULL,
                              /*clear_fe_pointers=*/FALSE);
      }  /* if */
      /* Determine the total size of all the blocks.  This includes the 
         headers as well as the block contents.  Note that we write out only
         to next_avail_in_block, not to after_end_of_block, since that's
         the only part with data in it, and it's correctly aligned. */
      total_bytes = 0;
      for (hdr = mem_region_table[region_number];
           hdr != NULL;
           hdr = hdr->next) {
        sizeof_t block_used = hdr->next_avail_in_block - hdr->start_of_block;
        total_bytes += sizeof(a_mem_block_header) + block_used;
      }  /* for */
#if DEBUG
/* GCOVR_EXCL_START */
      if (debug_level >= 3) {
        fprintf(f_debug, "total_bytes = %lu\n", (unsigned long)total_bytes);
      }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
      hdr = mem_region_table[region_number];
      /* Write the original address of the first block, the original address
         of the primary scope entry, and the total size of the blocks
         following. */
      (void)fwrite((char *)&hdr, sizeof(hdr), 1, f_il_output);
      (void)fwrite((char *)&il_header.region_scope_entry[region_number],
                   sizeof(a_scope_ptr), 1, f_il_output);
      (void)fwrite((char *)&total_bytes, sizeof(total_bytes), 1, f_il_output);
      /* Write the blocks. */
      for (; hdr != NULL; hdr = hdr->next) {
        sizeof_t block_used = hdr->next_avail_in_block - hdr->start_of_block;
#if DEBUG
/* GCOVR_EXCL_START */
        if (debug_level >= 3) {
          fprintf(f_debug, "writing %lu + %lu\n",
                           (unsigned long)sizeof(a_mem_block_header),
                           (unsigned long)block_used);
        }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
        if ((fwrite((char *)hdr, sizeof(a_mem_block_header),
                    1, f_il_output) != 1) ||
            (block_used != 0 &&
             fwrite(hdr->start_of_block, size_t_arg(block_used),
                    1, f_il_output) != 1)) {
          /* Error on write.  This check supplements the check done when the
             file is closed. */
          file_write_error(ec_intermediate_language_7, errno);
        }  /* if */
      }  /* for */
    }
#endif /* ALTERNATE_IL_FILE_FORMAT */
    if (region_number == FILE_SCOPE_REGION_NUMBER) {
      /* If we have just written the file-scope memory region, go
         back and rewrite il_header at the beginning of the file.  This
         must be done now rather than in finish_il_file because the
         file-scope storage may get freed and we may need to be
         able to check addresses in il_header to see if they're valid
         file-scope addresses.  The orphaned_file_scope_il_entries
         array must be written now for the same reason. */
     /*  Recall that the beginning of the file looks like:
           magic string that identifies an IL file (already written properly)
           number of regions (written as 0)
           number of function definition entries (written as 0)
           file offset to the file index table (written as 0)
           file offset to the start of the file scope region (written as 0)
           il_header (written as 0)
           orphaned_file_scope_il_entries array (written as 0)
      */
      /* Save the current (end of file) position. */
      end_pos = get_file_position(f_il_output);
      /* Seek to where the il_header was written. */
      if (set_file_position(f_il_output, (long)il_header_pos, SEEK_SET) != 0) {
        file_write_error(ec_intermediate_language_8, errno);
      }  /* if */
      /* Save il_header; it gets modified, written, then restored. */
      (void)memcpy(il_header_copy, (char *)&il_header, sizeof(il_header));
#if ALTERNATE_IL_FILE_FORMAT
      /* In the alternate form, the pointers in the header must be remapped to
         entry numbers. */
      remap_il_header_pointers(remap_ptr_to_entry_number,
                               remap_ptr_to_entry_number);
#endif /* ALTERNATE_IL_FILE_FORMAT */
      (void)fwrite((char *)&il_header, sizeof(il_header), 1, f_il_output);
      /* Restore il_header. */
      (void)memcpy((char *)&il_header, il_header_copy, sizeof(il_header));
#if ALTERNATE_IL_FILE_FORMAT
      /* Save a copy of the orphaned IL entry array; it gets modified,
         written, then restored. */
      (void)memcpy(orphaned_file_scope_il_entries_copy,
                   (char *)orphaned_file_scope_il_entries,
                   sizeof(orphaned_file_scope_il_entries));
      /* The pointers in the orphaned IL entry table must be remapped to
         entry numbers. */
      remap_first_ptr_of_orphaned_file_scope_entry_array(
                                                    remap_ptr_to_entry_number);
      remap_last_ptr_of_orphaned_file_scope_entry_array(
                                                    remap_ptr_to_entry_number);
#endif /* ALTERNATE_IL_FILE_FORMAT */
      /* Copy the orphaned_file_scope_il_entries array to the file. */
      (void)fwrite((char *)orphaned_file_scope_il_entries,
                   sizeof(orphaned_file_scope_il_entries), 1, f_il_output);
#if ALTERNATE_IL_FILE_FORMAT
      /* Restore the orphaned IL entry table. */
      (void)memcpy((char *)orphaned_file_scope_il_entries,
                   orphaned_file_scope_il_entries_copy,
                   sizeof(orphaned_file_scope_il_entries));
#endif /* ALTERNATE_IL_FILE_FORMAT */
      /* Restore the position at the end of the file.  SEEK_END is not
         used because ANSI doesn't guarantee it for binary files. */
      if (set_file_position(f_il_output, end_pos, SEEK_SET) != 0) {
        file_write_error(ec_intermediate_language_9, errno);
      }  /* if */
    }  /* if */
  }  /* if */

  db_exit();
}  /* write_memory_region */

void il_write_early_init(void)
/*
One time initialization that must take place early on in the front end.
This is done before command line processing.
*/
{
  f_il_output = NULL;
  il_file_name = NULL;
  il_header_pos = 0;
}  /* il_write_early_init */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */


