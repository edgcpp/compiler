/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

il_display.c -- Display the intermediate language in human-readable form.

Compile with STANDALONE_IL_DISPLAY defined to get an IL display
utility main program.  Otherwise, a version to be called in the same
program as the front end is produced.

*/

#ifdef PCH_PRAGMA_GUARD
/* Suppress generation of a precompiled header file -- il_display.c cannot
   share its precompiled header with any other file.  (The only utility from
   generating a precompiled header file would be for recompilation; for
   that, the no_pch pragma should be removed and a hdrstop pragma added
   after the last #include, outside all #ifs.)  */
#pragma no_pch
#endif /* PCH_PRAGMA_GUARD */

#include "basic_hdrs.h"

/*
This code is only needed if the IL is to be displayed, either in the
standalone il_display program or as part of the front end.  For a standalone
il_display program, the makefile should define STANDALONE_IL_DISPLAY.  To
include il_display in a front end, that makefile should define
NEED_IL_DISPLAY and a call of il_display should be added in the front end.
*/
#if NEED_IL_DISPLAY

/* Header files common to all files. */
#include "fe_common.h"

/* Additional header files. */
#include "il_display.h"
#include "il_walk.h"
#if STANDALONE_IL_DISPLAY
#include "fe_init.h"
#endif /* STANDALONE_IL_DISPLAY */
#if IL_SHOULD_BE_WRITTEN_TO_FILE
#include "il_file.h"
#include "il_read.h"
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

STATIC_THREAD a_boolean
		displaying_file_scope_il;
			/* TRUE if displaying the file-scope memory region,
			   FALSE if displaying a function scope memory
			   region. */

STATIC_THREAD FILE
		*f_display;
			/* The file to which IL display output is written. */

STATIC_THREAD an_il_to_str_output_control_block
		octl;	/* Output control block for interface to il_to_str
			   routines. */


/* Declaration required because of mutual recursion. */
static void disp_ptr(a_const_char     *ptr_name,
                     char             *entry_ptr,
                     an_il_entry_kind entry_kind);
static void disp_opname_kind_name(an_opname_kind kind);
static void disp_template_arg_list(a_const_char        *name,
                                   a_template_arg_ptr  ptr);
static void disp_special_function_kind_name(a_special_function_kind kind);


static void disp_string(a_const_char *string_ptr,
                        sizeof_t     string_length)
/*
Print the string at string_ptr, whose length is string_length.
*/
{
  if (string_ptr == NULL) {
    (void)fprintf(f_display, "NULL");
  } else {
    /* Strings can have unprintable characters, so print them carefully. */
    (void)putc('"', f_display);
    for (sizeof_t i = 0; i < string_length; i++) {
      char ch = string_ptr[i];

      if (isprint((unsigned char)ch)) {
        if (ch == '"' || ch == '\\') {
          (void)putc('\\', f_display);
        }  /* if */
        (void)putc(ch, f_display);
      } else {
        (void)fprintf(f_display, "\\%03o",
                      (unsigned int)(ch&((1<<targ_host_string_char_bit)-1)));
      }  /* if */
    }  /* for */
    (void)putc('"', f_display);
  }  /* if */
}  /* disp_string */


static void disp_null_term_string(a_const_char *string_ptr)
/*
Display the NULL-terminated string at string_ptr.
*/
{
  if (string_ptr == NULL) {
    (void)fprintf(f_display, "NULL");
  } else {
    disp_string(string_ptr, (sizeof_t)strlen(string_ptr));
  }  /* if */
}  /* disp_null_term_string */


static void put_str_to_display(
                       a_const_char                                     *str,
                       ARG_UNUSED an_il_to_str_output_control_block_ptr dummy)
/*
Output the indicated string to f_display.  This is used as an output routine
when using the il_to_str routines.
*/
{
  fputs(str, f_display);
}  /* put_str_to_display */


static void summarize_type(a_type *tp)
/*
Print a short version of the type at *tp.
*/
{
  form_type(tp, &octl);
}  /* summarize_type */


static void summarize_constant(a_constant *cp)
/*
Print a short version of the constant at *cp.
*/
{
  form_constant(cp, /*need_parens=*/FALSE, &octl);
}  /* summarize_constant */


static void disp_ptr_value(char             *entry_ptr,
                           an_il_entry_kind entry_kind)
/*
Display the value of the indicated pointer, which points to an entry of
kind entry_kind.
*/
{
  a_boolean is_file_scope_entry;

  /* Print the pointer value. */
  if (entry_ptr == NULL) {
    (void)fprintf(f_display, "NULL");
  } else {
    is_file_scope_entry = in_file_scope(entry_ptr);
    if (displaying_file_scope_il && !is_file_scope_entry) {
      /* Reference from file scope to a pointer whose prefix indicates
         function-scope memory region kind.  For string entries this can
         happen even when the string is allocated in file-scope memory,
         because strings referenced from a function-scope memory region
         are numbered as honorary members of that region during IL output.
         See assign_entry_number for details. */
      if (is_string_entry_kind(entry_kind)) {
        /* This is usually benign, so omit the pointer value, which would
           only make the display vary from run to run. */
        (void)fprintf(f_display, "**possible non-file-scope ptr**");
      } else {
        (void)fprintf(f_display, "**NON FILE SCOPE PTR** (%p)",
                      (a_void_ptr)entry_ptr);
      }  /* if */
    } else {
      (void)fprintf(f_display, is_file_scope_entry ? "file-scope"
                                                   : "func-scope");
      /* Print the entry kind. */
      (void)fprintf(f_display, " %s", il_entry_kind_names[(int)entry_kind]);
#if ALTERNATE_IL_FILE_FORMAT && STANDALONE_IL_DISPLAY
      /* Use entry_number.  After entries are read in, they
         are allocated in an array of entries, so one can determine the
         entry number from the offset relative to the base of the array
         of entries of that kind. */
      { char               **entry_array_base_array_ptr;
        an_il_entry_number entry_number;
        sizeof_t           gross_entry_size, prefix_size;

        /* Determine the size of the entry including the prefix, the length
           of the prefix, and the base array to use. */
        if (is_file_scope_entry) {
          gross_entry_size = fs_entry_length_with_prefix[(int)entry_kind];
          prefix_size = fs_length_of_entry_prefix[(int)entry_kind];
          entry_array_base_array_ptr = fs_entry_array_base_array;
        } else {
          gross_entry_size = entry_length_with_prefix[(int)entry_kind];
          prefix_size = length_of_entry_prefix[(int)entry_kind];
          entry_array_base_array_ptr = entry_array_base_array;
        }  /* if */

        /* Determine the entry number by dividing the offset into the
           area by the size of each entry. */
        /* The first entry in the array is entry 1, therefore "1 +". */
        a_ptrdiff offset = (entry_ptr - prefix_size -
                            entry_array_base_array_ptr[(int)entry_kind]);
        check_assertion(offset >= 0);
        entry_number = (an_il_entry_number)(1 + ((sizeof_t)offset /
                                                 gross_entry_size));
        (void)fprintf(f_display, "#%ld", (unsigned long)entry_number);
      }
#else /* !(ALTERNATE_IL_FILE_FORMAT && STANDALONE_IL_DISPLAY) */
      /* Use pointer address. */
#if defined(_WIN64)
      /* Avoid data loss in casting to unsigned long. */
      (void)fprintf(f_display, "@%p", entry_ptr);
#else /* !defined(_WIN64) */
      (void)fprintf(f_display, "@%lx", (unsigned long)entry_ptr);
#endif /* defined(_WIN64) */
#endif /* !(ALTERNATE_IL_FILE_FORMAT && STANDALONE_IL_DISPLAY) */
    }  /* if */
  }  /* if */
}  /* disp_ptr_value */


static void disp_name(a_const_char *name)
/*
Display a name that labels the display of an item.  name == NULL to display
no name.
*/
{
  int name_len;

  if (name != NULL) {
    (void)fprintf(f_display, "%s:", name);
    /* Get the text following indented the same amount regardless of the
       length of the name. */
#define Label_indent 25
    name_len = (int)(strlen(name) + 1);  /* 1 for the ":". */
    if (name_len >= Label_indent) {
      /* Name is already too long.  Start a new line and indent. */
      (void)fprintf(f_display, "\n");
      name_len = 0;
    }  /* if */
    /* Print spaces to get the following data in column Label_indent+1. */
    (void)fprintf(f_display, "%*c", Label_indent - name_len, ' ');
  }  /* if */
}  /* disp_name */


static void disp_int32(a_const_char *name,
                       int32_t      value)
/*
Display an int32_t value along with a name.
*/
{
  disp_name(name);
  (void)fprintf(f_display, "%ld\n", (long)value);
}  /* disp_int32 */


static void disp_uint32(a_const_char *name,
                        uint32_t     value)
/*
Display an uint32_t value along with a name.
*/
{
  disp_name(name);
  (void)fprintf(f_display, "%lu\n", (unsigned long)value);
}  /* disp_uint32 */


static void disp_long(a_const_char *name,
                      long         value)
/*
Display a long value along with a name.
*/
{
  disp_name(name);
  (void)fprintf(f_display, "%ld\n", value);
}  /* disp_long */


static void disp_unsigned_long(a_const_char  *name,
                               unsigned long value)
/*
Display an unsigned long value along with a name.
*/
{
  disp_name(name);
  (void)fprintf(f_display, "%lu\n", value);
}  /* disp_unsigned_long */


static void disp_host_large_integer(a_const_char		*name,
                                    a_host_large_integer	value)
/*
Display a host large unsigned value along with a name.
*/
{
  disp_name(name);

  a_number_buffer tmp_str(value);
  (void)fprintf(f_display, "%s\n", tmp_str.as_temp_characters());
}  /* disp_host_large_integer */


static void disp_host_large_unsigned(a_const_char		*name,
                                     a_host_large_unsigned	value)
/*
Display a host large unsigned value along with a name.
*/
{
  disp_name(name);

  a_number_buffer tmp_str(value);
  (void)fprintf(f_display, "%s\n", tmp_str.as_temp_characters());
}  /* disp_host_large_unsigned */


static void disp_boolean(a_const_char *name,
                         a_boolean    value)
/*
Display a boolean value along with a name.
*/
{
  disp_name(name);
  if (value) {
    (void)fprintf(f_display, "TRUE\n");
  } else {
    (void)fprintf(f_display, "FALSE\n");
  }  /* if */
}  /* disp_boolean */


static void disp_ptr(a_const_char     *ptr_name,
                     char             *entry_ptr,
                     an_il_entry_kind entry_kind)
/*
Display a pointer along with a summary of what it points to.  entry_ptr
is the pointer, and it points to an entry of kind entry_kind.  ptr_name
gives the name to be used for the display, or is NULL if no name should
be written.
*/
{
  a_const_char *name = NULL;
  a_type_ptr   type_name_type = NULL;

  disp_name(ptr_name);
  disp_ptr_value(entry_ptr, entry_kind);
  if (entry_ptr != NULL) {
    /* If the entry is named, print the name. */
    switch (entry_kind) {
      case iek_constant:
      case iek_variable:
      case iek_field:
      case iek_namespace:
      case iek_routine:
      case iek_label:
#if RECORD_MACROS_IN_IL
      case iek_macro:
#endif /* RECORD_MACROS_IN_IL */
      case iek_template_parameter:
        /* Entry has a source correspondence field. */
        name = ((a_constant_ptr)entry_ptr)->source_corresp.name;
        break;
      case iek_type:
        if (((a_type_ptr)entry_ptr)->source_corresp.name != NULL) {
          type_name_type = (a_type_ptr)entry_ptr;
        }  /* if */
        break;
      case iek_base_class:
        type_name_type = ((a_base_class_ptr)entry_ptr)->type;
        break;
      default:;
    }  /* switch */
    if (name != NULL || type_name_type != NULL) {
      /* Entry has a name.  If this is a tag, put "tag" in front of the
         name.  If a label, put "label". */
      (void)fprintf(f_display, ": ");
      if (type_name_type != NULL) {
        summarize_type(type_name_type);
        if (entry_kind == iek_base_class &&
            ((a_base_class_ptr)entry_ptr)->derived_class != NULL) {
          (void)fprintf(f_display, " (in ");
          summarize_type(((a_base_class_ptr)entry_ptr)->derived_class);
          (void)fprintf(f_display, ")");
        }  /* if */
      } else {
        if (entry_kind == iek_label) {
          (void)fprintf(f_display, "label ");
        }  /* if */
        (void)fprintf(f_display, "%s", name);
      }  /* if */
    } else {
      /* Entry is unnamed.  Give short description for some entries. */
      if (entry_kind == iek_constant) {
        if (!constant_is_recursive((a_constant_ptr)entry_ptr)) {
           (void)fprintf(f_display, ": ");
           summarize_constant((a_constant_ptr)entry_ptr);
        }  /* if */
      } else if (entry_kind == iek_type) {
        a_type_ptr type = (a_type_ptr)entry_ptr;
        (void)fprintf(f_display, ": ");
        summarize_type(type);
      } else if (entry_kind == iek_source_file) {
        (void)fprintf(f_display, ": ");
        disp_null_term_string(((a_source_file_ptr)entry_ptr)->file_name);
      }  /* if */
    }  /* if */
  }  /* if */
  (void)fprintf(f_display, "\n");
}  /* disp_ptr */


static void disp_string_ptr(a_const_char     *ptr_name,
                            a_const_char     *entry_ptr,
                            an_il_entry_kind entry_kind,
                            sizeof_t         entry_length)
/*
Display a pointer along with a summary of what it points to.  entry_ptr
is the pointer, and it points to a string entry of kind entry_kind, whose
length is given by entry_length if it is of kind iek_string_text.  ptr_name
gives the name to be used for the display, or is NULL if no name should
be written.
*/
{
  disp_name(ptr_name);
  disp_ptr_value((char *)entry_ptr, entry_kind);
  if (entry_ptr != NULL) {
    (void)fprintf(f_display, ": ");
    if (entry_kind == iek_string_text) {
      disp_string(entry_ptr, entry_length);
    } else {
      /* Others are null-terminated. */
      disp_null_term_string(entry_ptr);
    }  /* if */
  }  /* if */
  (void)fprintf(f_display, "\n");
}  /* disp_string_ptr */


static void disp_entity_list(a_const_char                *name,
                             an_il_entity_list_entry_ptr ptr)
/*
Display the indicated entity list and name.
*/
{
  if (ptr == NULL) {
    disp_ptr(name, (char *)ptr, iek_il_entity_list_entry);
  } else {
    for (; ptr != NULL; ptr = ptr->next) {
      if (name != NULL) {
        disp_name(name);
      } else {
        (void)fprintf(f_display, "%*c", Label_indent, ' ');
      }  /* if */
      disp_ptr_value(ptr->entity.ptr, (an_il_entry_kind)ptr->entity.kind);
      (void)fprintf(f_display, "\n");
      /* Only display the list name for the first entry. */
      name = NULL;
    }  /* for */
  }  /* if */
}  /* disp_entity_list */


static void disp_variable_list(a_const_char   *name,
                               a_variable_ptr ptr)
/*
Display the indicated variable list and name.
*/
{
  if (ptr == NULL) {
    disp_ptr(name, (char *)ptr, iek_variable);
  } else {
    for (; ptr != NULL; ptr = ptr->next) {
      if (name != NULL) {
        disp_name(name);
      } else {
        (void)fprintf(f_display, "%*c", Label_indent, ' ');
      }  /* if */
      disp_ptr_value((char*)ptr, iek_variable);
      (void)fprintf(f_display, "\n");
      /* Only display the list name for the first entry. */
      name = NULL;
    }  /* for */
  }  /* if */
}  /* disp_variable_list */


static void disp_access(a_const_char        *name,
                        an_access_specifier access)
/*
Display the indicated access specifier with a name.
*/
{
  a_const_char *s;

  disp_name(name);
  switch (access) {
    case as_public:         s = "as_public\n";         break;
    case as_protected:      s = "as_protected\n";      break;
    case as_private:        s = "as_private\n";        break;
    case as_inaccessible:   s = "as_inaccessible\n";   break;
    default:                s = "**BAD ACCESS SPECIFIER**\n";
  }  /* switch */  
  (void)fprintf(f_display, "%s", s);
}  /* disp_access */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void disp_assembly_visibility(a_const_char            *name,
                                     an_assembly_visibility  vis)
/*
Display the indicated assembly visibility with a name.
*/
{

  if (vis != (an_assembly_visibility)av_none) {
    a_const_char  *s;
    disp_name(name);
    switch (vis) {
      case av_public:         s = "av_public\n";         break;
      case av_private:        s = "av_private\n";        break;
      default:                s = "**BAD ASSEMBLY VISIBILITY**\n";
    }  /* switch */  
    (void)fprintf(f_display, "%s", s);
  }  /* if */
}  /* disp_assembly_visibility */


static void disp_cli_class_type_kind(a_const_char           *name,
                                     a_cli_class_type_kind  cctk)
/*
Display the indicated C++/CLI class kind with a name.
*/
{

  if (il_header.cppcli_enabled || il_header.cppcx_enabled) {
    a_const_char  *s;
    disp_name(name);
    switch (cctk) {
      case cctk_standard:     s = "cctk_standard\n";         break;
      case cctk_value:        s = "cctk_value\n";            break;
      case cctk_ref:          s = "cctk_ref\n";              break;
      case cctk_interface:    s = "cctk_interface\n";        break;
      case cctk_unresolved:   s = "cctk_unresolved\n";       break;
      default:                s = "**BAD C++/CLI CLASS TYPE KIND**\n";
    }  /* switch */  
    (void)fprintf(f_display, "%s", s);
  }  /* if */
}  /* disp_cli_class_type_kind */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void disp_name_linkage(a_const_char         *name,
                              a_name_linkage_kind  nlk)
/*
Display the indicated field name and name linkage kind.
*/
{
  disp_name(name);
  (void)fprintf(f_display, "%s\n", name_linkage_kind_names[(int)nlk]);
}  /* disp_name_linkage */


static void disp_source_position(a_const_char       *str,
                                 a_source_position  *pos)
/*
Display the indicated source position, preceding it with the specified
string.  Note that nothing is printed out when *pos is null_source_position.
*/
{
  check_assertion(str != NULL);
  if (pos->seq != 0 || pos->column != 0) {
    Small_string<40> buff(str, ".seq");

    disp_unsigned_long(buff.as_temp_characters(), (unsigned long)pos->seq);
    buff.reset_to(str, ".column");
    disp_unsigned_long(buff.as_temp_characters(), (unsigned long)pos->column);
#if FULLY_RESOLVED_MACRO_POSITIONS
    if (pos->orig_seq != pos->seq || pos->orig_column != pos->column) {
      /* If the orig_seq/orig_column are different from seq/column, they
         represent the location from which the text was copied into a macro
         expansion (from a macro definition or macro argument) and should be
         printed. */
      buff.reset_to(str, ".orig_seq");
      disp_unsigned_long(buff.as_temp_characters(),
                         (unsigned long)pos->orig_seq);
      buff.reset_to(str, ".orig_column");
      disp_unsigned_long(buff.as_temp_characters(),
                         (unsigned long)pos->orig_column);
    }  /* if */
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
#if RECORD_MACRO_INVOCATIONS
    if (pos->macro_context != NO_PARENT_MACRO_INVOCATION) {
      /* Values other than NO_PARENT_MACRO_INVOCATION indicate that the
         position is in the expansion of the macro invocation whose record is
         indexed by macro_context: print it. */
      buff.reset_to(str, ".macro_context");
      disp_long(buff.as_temp_characters(), (long)pos->macro_context);
    }  /* if */
#endif /* RECORD_MACRO_INVOCATIONS */
  }  /* if */
}  /* disp_source_position */

#if EXTRA_SOURCE_POSITIONS_IN_IL

static void disp_source_range(a_const_char    *str,
                              a_source_range  *range)
/*
Display the indicated source position range, preceding it with the specified
string.
*/
{
  check_assertion(str != NULL);
  /* Don't put out "null" source-range information. */
  if (range->start.seq != 0 || range->end.seq != 0) {
    (void)fprintf(f_display, "%s\n", str);

    /* Default indentation is 2, but add any indentation implied by the
       string that is passed in. */
    Small_string<12> buff("  ");
    for (; *str == ' '; ++str) {
      buff.append(" ");
    }  /* for */

    /* Put out start and end positions separately. */
    size_t orig_len = buff.length();
    buff.append("start");
    disp_source_position(buff.as_temp_characters(), &range->start);
    buff.truncate_to(orig_len);
    buff.append("end");
    disp_source_position(buff.as_temp_characters(), &range->end);
  }  /* if */
}  /* disp_source_range */

#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

static void disp_name_reference(a_name_reference_ptr ptr)
/*
Display a_name_reference entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_name_reference);
  disp_ptr("qualifier", (char *)ptr->qualifier, iek_name_qualifier);
  if (special_kind_is(ptr, sfk_none)) {
    if (ptr->variant.destructor_type != NULL) {
      disp_ptr("destructor_type", (char *)ptr->variant.destructor_type,
               iek_type);
    }  /* if */
#if (MICROSOFT_EXTENSIONS_ALLOWED && !DO_IL_LOWERING) || \
    BUILTIN_FUNCTIONS_ENABLED
  } else {
    disp_name("special_kind");
    disp_special_function_kind_name(ptr->special_kind);
    (void)fprintf(f_display, "\n");
#if MICROSOFT_EXTENSIONS_ALLOWED && !DO_IL_LOWERING
    if (ptr->variant.property_or_event_descr != NULL) {
      disp_ptr("property_or_event_descr",
               (char *)ptr->variant.property_or_event_descr,
               iek_property_or_event_descr);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED && !DO_IL_LOWERING */
#endif /* (MICROSOFT_EXTENSIONS_ALLOWED && !DO_IL_LOWERING) || ... */
  }  /* if */
  disp_long("num_template_arguments", ptr->num_template_arguments);
  if (ptr->orig_template_arg_list != NULL) {
    disp_template_arg_list("orig_template_arg_list",
                           ptr->orig_template_arg_list);
  }  /* if */
  disp_boolean("is_global_qualified_name",
               (a_boolean)ptr->is_global_qualified_name);
  disp_boolean("is_template_id", (a_boolean)ptr->is_template_id);
  disp_boolean("is_super_qualified", (a_boolean)ptr->is_super_qualified);
  disp_boolean("is_decltype_qualified", (a_boolean)ptr->is_decltype_qualified);
  disp_boolean("from_prototype_instantiation",
               (a_boolean)ptr->from_prototype_instantiation);
#if GENERATE_SOURCE_SEQUENCE_LISTS
  disp_boolean("used_in_primary_declarator",
               (a_boolean)ptr->used_in_primary_declarator);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
}  /* disp_name_reference */


static void disp_name_qualifier(a_name_qualifier_ptr ptr)
/*
Display a_name_qualifier entry.
*/
{
  /* The "next" field is front-end-only. */
  disp_boolean("is_class", (a_boolean)ptr->is_class);
  if (ptr->is_class) {
    disp_ptr("qualifier.class_type", (char *)ptr->qualifier.class_type,
             iek_type);
  } else {
    disp_ptr("qualifier.namespace_ptr", (char *)ptr->qualifier.namespace_ptr,
             iek_namespace);
  }  /* if */
  if (ptr->name != NULL) {
    disp_string_ptr("name", ptr->name, iek_id_name, (sizeof_t)0);
  }  /* if */
  disp_ptr("previous_qualifier", (char *)ptr->previous_qualifier,
           iek_name_qualifier);
}  /* disp_name_qualifier */

#if EXTRA_SOURCE_POSITIONS_IN_IL

static void disp_element_position(an_element_position_ptr  epp)
/*
Display the indicated element position entry.
*/
{
  a_const_char  *kind_str;

  switch (epp->kind) {
    case epk_error:
      kind_str = "**ERROR ENTRY**";
      break;
    case epk_specialization_header:
      kind_str = "specialization_header";
      break;
    case epk_noreturn:
      kind_str = "noreturn";
      break;
    default:
      kind_str = "**BAD ELEMENT POSITION KIND**";
      break;
  }  /* switch */
  disp_name("kind");
  (void)fprintf(f_display, "%s\n", kind_str);
  disp_source_position("position", &epp->position);
  disp_ptr("next", (char *)epp->next, iek_element_position);
}  /* disp_element_position */

#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

static void disp_source_corresp(a_source_correspondence     *scp,
                                ARG_UNUSED an_il_entry_kind kind)
/*
Display the indicated source correspondence entry.
*/
{
  (void)fprintf(f_display, "source_corresp:\n");
  if (scp->name != NULL) {
    disp_string_ptr("  name", scp->name, iek_id_name, (sizeof_t)0);
  }  /* if */
#if NEED_NAME_MANGLING
  if (scp->unmangled_name_or_mangled_encoding != NULL) {
    disp_string_ptr("  unmangled_name_or_mangled_encoding",
                    scp->unmangled_name_or_mangled_encoding, iek_id_name,
                    (sizeof_t)0);
  }  /* if */
#endif /* NEED_NAME_MANGLING */
  disp_source_position("  decl_position", &scp->decl_position);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (scp->decl_pos_info != NULL) {
    disp_source_range("  identifier_range",
                      &scp->decl_pos_info->identifier_range);
    disp_source_range("  specifiers_range",
                      &scp->decl_pos_info->specifiers_range);
    if (kind == iek_constant && scp->name != NULL &&
        is_enum_constant((a_constant_ptr)scp)) {
      disp_source_range("  enum_value_range",
                        &scp->decl_pos_info->variant.enum_value_range);
    } else if (kind == iek_namespace) {
      disp_source_range(
                     "  namespace_definition_range",
                     &scp->decl_pos_info->variant.namespace_definition_range);
    } else {
      disp_source_range("  declarator_range",
                        &scp->decl_pos_info->variant.declarator_range);
    }  /* if */
    disp_ptr("  extra_positions", (char*)scp->decl_pos_info->extra_positions,
             iek_element_position);
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  if (scp->name_references != NULL) {
    disp_ptr("  name_references", (char *)scp->name_references,
             iek_name_reference);
  }  /* if */
  if (scp->is_class_member) {
    disp_boolean("  is_class_member", TRUE);
    disp_access("  access", (an_access_specifier)scp->access);
  }  /* if */
  disp_ptr("  parent_scope", (char *)scp->parent_scope, iek_scope);
  disp_ptr("  enclosing_routine", (char *)scp->enclosing_routine, iek_routine);
  disp_boolean("  referenced", (a_boolean)scp->referenced);
#if MAINTAIN_NEEDED_FLAGS
  disp_boolean("  needed", (a_boolean)scp->needed);
#endif /* MAINTAIN_NEEDED_FLAGS */
  if (scp->is_local_to_function) {
    disp_boolean("  is_local_to_function", TRUE);
  }  /* if */
  if (scp->parent_via_local_scope_ref) {
    disp_boolean("  parent_via_local_scope_ref", TRUE);
  }  /* if */
  if (scp->name != NULL) {
    disp_name_linkage("  name_linkage",
                      (a_name_linkage_kind)scp->name_linkage);
  }  /* if */
  if (scp->has_associated_pragma) {
    disp_boolean("  has_associated_pragma", TRUE);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (scp->has_associated_attribute) {
    disp_boolean("  has_associated_attribute", TRUE);
  }  /* if */
  if (scp->microsoft_identifier_used) {
    disp_boolean("  microsoft_identifier_used", TRUE);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NEED_NAME_MANGLING
  /* Do not print out name_has_been_mangled,
     mangled_name_cannot_be_included_in_other_name,
     final_name_mangling_pending, and unnamed_entity_given_fabricated_name,
     which are used only in the front end. */
#endif /* NEED_NAME_MANGLING */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (scp->is_decl_after_first_in_comma_list) {
    disp_boolean("  is_decl_after_first_in_comma_list", TRUE);
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if ONE_INSTANTIATION_PER_OBJECT
  if (scp->static_used_by_instantiation) {
    disp_boolean("  static_used_by_instantiation", TRUE);
  }  /* if */
#if DUPLICATE_SPECIAL_STATICS_IN_INSTANTIATION_SLICES
  if (scp->duplicate_static_in_instantiation_slices) {
    disp_boolean("  duplicate_static_in_instantiation_slices", TRUE);
  }  /* if */
#endif /* DUPLICATE_SPECIAL_STATICS_IN_INSTANTIATION_SLICES */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  if (scp->copied_from_secondary_trans_unit) {
    disp_boolean("  copied_from_secondary_trans_unit", TRUE);
  }  /* if */
  if (scp->same_name_as_external_entity_in_secondary_trans_unit) {
    disp_boolean("  same_name_as_external_entity_in_secondary_trans_unit",
                 TRUE);
  }  /* if */
  if (scp->member_of_unknown_base) {
    disp_boolean("  member_of_unknown_base", TRUE);
  }  /* if */
  if (scp->qualified_unknown_base_member) {
    disp_boolean("  qualified_unknown_base_member", TRUE);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (scp->member_of_unknown_super) {
    disp_boolean("  member_of_unknown_super", TRUE);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (scp->marked_as_gnu_extension) {
    disp_boolean("marked_as_gnu_extension", TRUE);
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if (scp->is_deprecated_or_unavailable) { 
    disp_boolean("is_deprecated_or_unavailable", TRUE);
  }  /* if */
  if (scp->externalized) {
    disp_boolean("externalized", TRUE);
  }  /* if */
  if (scp->maybe_unused) {
    disp_boolean("maybe_unused", TRUE);
  }  /* if */
#if RECORD_SCOPE_DEPTH_IN_IL
  disp_long("  scope_depth", (long)scp->scope_depth);
#endif /* RECORD_SCOPE_DEPTH_IN_IL */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (scp->source_sequence_entry != NULL) {
    disp_ptr("  source_sequence_entry", (char *)scp->source_sequence_entry,
             iek_source_sequence_entry);
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if ONE_INSTANTIATION_PER_OBJECT
  if (scp->per_instantiation_needed_flags != NULL) {
    a_per_instantiation_needed_flags_entry_ptr pinfep;

    disp_name("  per_instantiation_needed_flags");
    for (pinfep = scp->per_instantiation_needed_flags;
         pinfep != NULL;
         pinfep = pinfep->next) {
      int nbyte;
      for (nbyte = 0;
           nbyte < BYTES_PER_INSTANTIATION_NEEDED_FLAG_ENTRY;
           nbyte++) {
        a_byte curr_byte = pinfep->bytes[nbyte];
        int    nbit;
        for (nbit = 0; nbit < CHAR_BIT; nbit++) {
          (void)fprintf(f_display, "%c", ((curr_byte >> nbit)&1) ? '1' : '0');
        }  /* for */
      }  /* for */
      (void)fprintf(f_display, "\n");
    }  /* for */
  }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  disp_ptr("  attributes", (char *)scp->attributes, iek_attribute);
}  /* disp_source_corresp */


static void disp_source_file(a_source_file_ptr ptr)
/*
Display a_source_file entry.
*/
{
  disp_string_ptr("file_name", ptr->file_name, iek_other_text, (sizeof_t)0);
  disp_string_ptr("full_name", ptr->full_name, iek_other_text, (sizeof_t)0);
  disp_string_ptr("name_as_written", ptr->name_as_written, iek_other_text,
                  (sizeof_t)0);
  disp_unsigned_long("first_seq_number", (unsigned long)ptr->first_seq_number);
  disp_unsigned_long("last_seq_number", (unsigned long)ptr->last_seq_number);
  disp_unsigned_long("first_line_number",
                     (unsigned long)ptr->first_line_number);
  disp_ptr("first_child_file", (char *)ptr->first_child_file, iek_source_file);
  disp_ptr("last_child_file", (char *)ptr->last_child_file, iek_source_file);
  disp_ptr("next", (char *)ptr->next, iek_source_file);
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
  if (ptr->related_file_implicit_include_done) {
    disp_boolean("related_file_implicit_include_done", TRUE);
  }  /* if */
  if (ptr->is_implicit_include) {
    disp_boolean("is_implicit_include", TRUE);
  }  /* if */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
  if (ptr->is_include_file) {
    disp_boolean("is_include_file", TRUE);
  }  /* if */
  if (ptr->included_by_system_include) {
    disp_boolean("included_by_system_include", TRUE);
  }  /* if */
  if (ptr->included_by_preinclude) {
    disp_boolean("included_by_preinclude", TRUE);
  }  /* if */
  if (ptr->preinclude_macros_only) {
    disp_boolean("preinclude_macros_only", TRUE);
  }  /* if */
  if (ptr->from_system_include_dir) {
    disp_boolean("from_system_include_dir", TRUE);
  }  /* if */
  if (ptr->top_level_file) {
    disp_boolean("top_level_file", TRUE);
  }  /* if */
  if (ptr->top_level_file_from_pch) {
    disp_boolean("top_level_file_from_pch", TRUE);
  }  /* if */
}  /* disp_source_file */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void disp_cli_metadata_file(a_cli_metadata_file_ptr ptr)
/*
Display a CLI metadata file made available by an explicit or implicit
#using directive.
*/
{
  disp_string_ptr("full_name", ptr->full_name, iek_other_text, (sizeof_t)0);
  disp_string_ptr("name_as_written", ptr->name_as_written, iek_other_text,
                  (sizeof_t)0);
  disp_ptr("next", (char *)ptr->next, iek_cli_metadata_file);
  disp_source_position("position", &ptr->position);
  if (ptr->as_friend) {
    disp_boolean("as_friend", TRUE);
  }  /* if */
  if (ptr->referenced_by_preusing) {
    disp_boolean("referenced_by_preusing", TRUE);
  }  /* if */
  if (ptr->referenced_by_system_using) {
    disp_boolean("referenced_by_system_using", TRUE);
  }  /* if */
}  /* disp_cli_metadata_file */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void disp_subobject_path(a_subobject_path_ptr ptr)
/*
Display the indicated subobject path.
*/
{
  (void)fprintf(f_display, "\n");
  disp_ptr("next", (char *)ptr->next, iek_subobject_path);
  if (ptr->is_offset) {
    disp_boolean("is_offset", TRUE);
    if (ptr->is_converted) {
      disp_boolean("is_converted", TRUE);
    }  /* if */
    disp_host_large_integer("ptr_offset",
                            (a_host_large_integer)ptr->variant.ptr_offset);
  } else if (ptr->is_base_class) {
    disp_boolean("is_base_class", TRUE);
    disp_ptr("variant.base_class", (char*)ptr->variant.base_class,
             iek_base_class);
  } else {
    disp_ptr("variant.field", (char*)ptr->variant.field, iek_field);
  }  /* if */
}  /* disp_subobject_path */


static void disp_module(a_module_ptr ptr)
/*
Display information about the indicated module.
*/
{
  // FIXME: Make sure all fields are printed
  if (ptr->resolved_file != NULL) {
    disp_string_ptr("resolved_file", ptr->resolved_file, iek_other_text,
                    (sizeof_t)0);
  }  /* if */
  disp_name("kind");
  switch (ptr->kind) {
    case mk_none:
      (void)fprintf(f_display, "mk_none\n");
      break;
    case mk_header_unit:
      (void)fprintf(f_display, "mk_header_unit\n");
      disp_boolean("variant.header_unit.is_sys_include",
                   ptr->variant.header_unit.is_sys_include);
      disp_boolean("variant.header_unit.suppress_macro_export",
                   ptr->variant.header_unit.suppress_macro_export);
      disp_string_ptr("variant.header_unit.name",
                      ptr->variant.header_unit.name,
                      iek_other_text, (sizeof_t)0);
      disp_string_ptr("variant.header_unit.resolved_header",
                      ptr->variant.header_unit.resolved_header,
                      iek_other_text, (sizeof_t)0);
      break;
    case mk_unit:
      (void)fprintf(f_display, "mk_unit\n");
      disp_string_ptr("variant.unit.name", ptr->variant.unit.name,
                      iek_other_text, (sizeof_t)0);
      break;
    case mk_unit_partition:
      (void)fprintf(f_display, "mk_unit_partition\n");
      disp_string_ptr("variant.unit_partition.name",
                      ptr->variant.unit_partition.name,
                      iek_other_text, (sizeof_t)0);
      disp_ptr("variant.unit_partition.unit",
               (char*)ptr->variant.unit_partition.unit, iek_module);
      break;
    default_is_unexpected();
  }  /* switch */
}  /* disp_module */


static void disp_module_import_decl(a_module_import_decl_ptr ptr)
/*
Display the indicated module-import-declaration.
*/
{
  // FIXME: Make sure all fields are printed
  disp_source_position("position", &ptr->position);
  disp_source_position("module_name_position", &ptr->module_name_position);
  if (ptr->attributes != NULL) {
    disp_ptr("attributes", (char *)ptr->attributes, iek_attribute);
  }  /* if */
  if (ptr->module_info != NULL) {
    disp_ptr("module_info", (char *)ptr->module_info, iek_module);
  }  /* if */
}  /* disp_module_import_decl */


static void disp_scoped_expression(a_scoped_expression *ptr)
/*
Display the indicated scoped expression.
*/
{
  disp_source_corresp(&ptr->source_corresp, iek_scoped_expression);
  disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
}  /* disp_scoped_expression */


static void disp_template_param_coordinate(a_template_param_coordinate *ptr)
/*
Display the indicated template parameter coordinate.
*/
{
  disp_uint32("coordinates.position", ptr->position);
  disp_int32("coordinates.depth", ptr->depth);
}  /* disp_template_param_coordinate */


static void disp_template_param_constant(a_constant *ptr)
/*
Display a ck_template_param constant.
*/
{
  (void)fprintf(f_display, "ck_template_param\n");
  disp_name("kind");
  switch (ptr->variant.template_param.kind) {
    case tpck_param:
      (void)fprintf(f_display, "tpck_param\n");
      disp_template_param_coordinate(
                             &ptr->variant.template_param.variant.coordinates);
      break;
    case tpck_expression:
      (void)fprintf(f_display, "tpck_expression\n");
#if PROTOTYPE_INSTANTIATIONS_IN_IL
      if (ptr->variant.template_param.local_expr_ref) {
        disp_boolean("local_expr_ref", TRUE);
      }  /* if */
#endif /* PROTOTYPE_INSTANTIATIONS_IN_IL */
      disp_ptr("expr", (char *)ptr->variant.template_param.variant.expr,
               iek_expr_node);
      break;
    case tpck_member:
      (void)fprintf(f_display, "tpck_member\n");
      break;
    case tpck_unknown_function:
      (void)fprintf(f_display, "tpck_unknown_function\n");
      if (ptr->variant.template_param.is_qualified_name) {
        disp_boolean("is_qualified_name", TRUE);
      }  /* if */
      if (ptr->variant.template_param.has_address_of) {
        disp_boolean("has_address_of", TRUE);
      }  /* if */
      { a_type_ptr conversion_type =
          ptr->variant.template_param.variant.unknown_function.conversion_type;
        if (conversion_type != NULL) {
          disp_ptr("conversion_type", (char *)conversion_type, iek_type);
        }  /* if */
      }
#if MICROSOFT_EXTENSIONS_ALLOWED
      { a_property_or_event_descr_ptr property_or_event_descr =
                             ptr->variant.template_param
                             .variant.unknown_function.property_or_event_descr;
        if (property_or_event_descr != NULL) {
          disp_ptr("property_or_event_descr", (char *)property_or_event_descr,
                   iek_property_or_event_descr);
        }  /* if */
      }
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      { an_opname_kind opname_kind = 
              ptr->variant.template_param.variant.unknown_function.opname_kind;
        if (opname_kind != (an_opname_kind)onk_none) {
          disp_name("opname_kind");
          disp_opname_kind_name(opname_kind);
          (void)fprintf(f_display, "\n");
        }  /* if */
      }
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (ptr->variant.template_param.variant.unknown_function.special_kind !=
                                           (a_special_function_kind)sfk_none) {
        disp_name("special_kind");
        disp_special_function_kind_name(ptr->variant.template_param.variant.
                                                unknown_function.special_kind);
        (void)fprintf(f_display, "\n");
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      /* unknown_function.symbol is front-end-only and is not printed. */
      break;
    case tpck_dependent_constant:
      (void)fprintf(f_display, "tpck_dependent_constant\n");
      disp_ptr("constant",
               (char *)ptr->variant.template_param.variant.constant,
               iek_constant);
      break;
    case tpck_concat_string_literals:
      (void)fprintf(f_display, "tpck_concat_string_literals\n");
      { a_constant_ptr list_con =
              ptr->variant.template_param.variant.string_literal_list;
        for (; list_con != NULL; list_con = list_con->next) {
          disp_ptr("string_literal", (char *)list_con, iek_constant);
        }  /* for */
      }
      break;
    case tpck_address:
      (void)fprintf(f_display, "tpck_address\n");
      disp_ptr("constant",
               (char *)ptr->variant.template_param.variant.constant,
               iek_constant);
      break;
    case tpck_sizeof:
      (void)fprintf(f_display, "tpck_sizeof\n");
      goto do_sizeof_cases;
    case tpck_datasizeof:
      (void)fprintf(f_display, "tpck_datasizeof\n");
      goto do_sizeof_cases;
    case tpck_alignof:
      (void)fprintf(f_display, "tpck_alignof\n");
      goto do_sizeof_cases;
    case tpck_uuidof:
      (void)fprintf(f_display, "tpck_uuidof\n");
      goto do_sizeof_cases;
    case tpck_typeid:
      (void)fprintf(f_display, "tpck_typeid\n");
      goto do_sizeof_cases;
    case tpck_noexcept:
      (void)fprintf(f_display, "tpck_noexcept\n");
do_sizeof_cases:
#if PROTOTYPE_INSTANTIATIONS_IN_IL
      if (ptr->variant.template_param.local_expr_ref) {
        disp_boolean("local_expr_ref", TRUE);
      }  /* if */
#endif /* PROTOTYPE_INSTANTIATIONS_IN_IL */
      disp_ptr("type",
               (char *)ptr->variant.template_param.variant.templ_sizeof.type,
               iek_type);
      disp_ptr("expr",
               (char *)ptr->variant.template_param.variant.templ_sizeof.expr,
               iek_expr_node);
      if (ptr->variant.template_param.variant.templ_sizeof.is_std_alignof) {
        disp_boolean("is_std_alignof", TRUE);
      }  /* if */
      break;
    case tpck_template_ref:
      (void)fprintf(f_display, "tpck_template_ref\n");
      if (ptr->variant.template_param.is_qualified_name) {
        disp_boolean("is_qualified_name", TRUE);
      }  /* if */
      if (ptr->variant.template_param.has_address_of) {
        disp_boolean("has_address_of", TRUE);
      }  /* if */
      disp_ptr("con",
               (char *)ptr->variant.template_param.variant.template_ref.con,
               iek_constant);
      disp_template_arg_list("arg_list",
                             ptr->variant.template_param.variant.
                                                        template_ref.arg_list);
      break;
    case tpck_integer_pack:
      (void)fprintf(f_display, "tpck_integer_pack\n");
      disp_ptr("bound", (char *)ptr->variant.template_param.variant.bound,
               iek_constant);
      break;
    case tpck_destructor:
      (void)fprintf(f_display, "tpck_destructor\n");
      disp_ptr("destructor",
               (char*)ptr->variant.template_param.variant.destructor.type,
               iek_type);
      disp_boolean("unqualified",
                   ptr->variant.template_param.variant.destructor.unqualified);
      break;
    default:
      (void)fprintf(f_display, "**BAD TEMPLATE PARAM CONSTANT KIND**\n");
      break;
  }  /* switch */    
  if (ptr->variant.template_param.is_pack) {
    disp_boolean("is_pack", TRUE);
  }  /* if */
}  /* disp_template_param_constant */


static void disp_constant(a_constant_ptr ptr)
/*
Display the indicated constant entry.
*/
{
  disp_source_corresp(&ptr->source_corresp, iek_constant);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_source_position("end_position", &ptr->end_position);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  disp_ptr("next", (char *)ptr->next, iek_constant);
  disp_ptr("type", (char *)ptr->type, iek_type);
  if (ptr->orig_type != NULL) {
    disp_ptr("orig_type", (char *)ptr->orig_type, iek_type);
  }  /* if */
  if (ptr->expr != NULL) {
    disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
  }  /* if */
#if DO_IL_LOWERING
  if (ptr->assoc_var != NULL) {
    disp_ptr("assoc_var", (char *)ptr->assoc_var, iek_variable);
  }  /* if */
#endif /* DO_IL_LOWERING */
  if (ptr->implicit_cast) {
    disp_boolean("implicit_cast", TRUE);
  }  /* if */
  if (ptr->explicit_cast_applied) {
    disp_boolean("explicit_cast_applied", TRUE);
  }  /* if */
  if (ptr->is_reinterpret_cast) {
    disp_boolean("is_reinterpret_cast", TRUE);
  }  /* if */
  if (ptr->is_reinterpret_like_cast) {
    disp_boolean("is_reinterpret_like_cast", TRUE);
  }  /* if */
  if (ptr->non_arithmetic) {
    disp_boolean("non_arithmetic", TRUE);
  }  /* if */
  if (ptr->is_simple_zero) {
    disp_boolean("is_simple_zero", TRUE);
  }  /* if */
  if (ptr->null_pointer_constant_ruled_out) {
    disp_boolean("null_pointer_constant_ruled_out", TRUE);
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  if (ptr->null_keyword) {
    disp_boolean("null_keyword", TRUE);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (ptr->nullptr_keyword) {
    disp_boolean("nullptr_keyword", TRUE);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ptr->native_nullptr_keyword) {
    disp_boolean("native_nullptr_keyword", TRUE);
  }  /* if */
  if (ptr->ptr_to_mem_constant_construct) {
    disp_boolean("ptr_to_mem_constant_construct", TRUE);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (ptr->explicit_braces_on_aggregate) {
    disp_boolean("explicit_braces_on_aggregate", TRUE);
  }  /* if */
  if (ptr->explicit_parentheses_on_aggregate) {
    disp_boolean("explicit_parentheses_on_aggregate", TRUE);
  }  /* if */
  if (ptr->from_undefined_preproc_id) {
    disp_boolean("from_undefined_preproc_id", TRUE);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED
  if (ptr->flexible_array_initializer) {
    disp_boolean("flexible_array_initializer", TRUE);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED */
  if (ptr->uses_designated_initializers) {
    disp_boolean("uses_designated_initializers", TRUE);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ptr->is_literal_field) {
    disp_boolean("is_literal_field", TRUE);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (ptr->is_pack_expansion) {
    disp_boolean("is_pack_expansion", TRUE);
  }  /* if */
  if (ptr->is_named_constant_definition) {
    disp_boolean("is_named_constant_definition", TRUE);
  }  /* if */
  if (ptr->partial_aggr_value) {
    disp_boolean("partial_aggr_value", TRUE);
  }  /* if */
  if (ptr->is_partially_initialized) {
    disp_boolean("is_partially_initialized", TRUE);
  }  /* if */
  if (ptr->implicit_aggr_element) {
    disp_boolean("implicit_aggr_element", TRUE);
  }  /* if */
  if (ptr->is_compound_literal) {
    disp_boolean("is_compound_literal", TRUE);
  }  /* if */
  if (ptr->is_result_of_constexpr_call) {
    disp_boolean("is_result_of_constexpr_call", TRUE);
  }  /* if */
  if (ptr->is_generic_initializer) {
    disp_boolean("is_generic_initializer", TRUE);
  }  /* if */
  if (ptr->constant_for_base_class_from_constexpr_folding) {
    disp_boolean("constant_for_base_class_from_constexpr_folding", TRUE);
  }  /* if */
  if (ptr->local_expr_ref) {
    disp_boolean("local_expr_ref", TRUE);
  }  /* if */
  if (ptr->folded_statement_expression) {
    disp_boolean("folded_statement_expression", TRUE);
  }  /* if */
  disp_name("kind");
  switch (ptr->kind) {
    case ck_error:
      (void)fprintf(f_display, "ck_error\n");
      break;
#if UPC_EXTENSIONS_ALLOWED
    case ck_upc_threads:
      (void)fprintf(f_display, "ck_upc_threads\n");
      goto display_constant_value;
    case ck_upc_mythread:
      (void)fprintf(f_display, "ck_upc_mythread\n");
      break;
#endif /* UPC_EXTENSIONS_ALLOWED */
    case ck_integer:
      (void)fprintf(f_display, "ck_integer\n");
      disp_name("integer_value");
      /* Use form_integer_constant directly instead of summarize_constant so
         enumeration constants will show their numeric value and not the
         enumerator name. */
      form_integer_constant(ptr, /*suppress_cast=*/TRUE, /*need_parens=*/FALSE,
                            &octl);
      (void)fprintf(f_display, "\n");
      break;
#if FIXED_POINT_ALLOWED
    case ck_fixed_point:
      (void)fprintf(f_display, "ck_fixed_point\n");
      disp_name("fixed_point_value");
      goto display_constant_value;
#endif /* FIXED_POINT_ALLOWED */
    case ck_string:
      (void)fprintf(f_display, "ck_string\n");
      disp_name("character_kind");
      switch(ptr->character_kind) {
        case chk_char:
          (void)fprintf(f_display, "char\n");
          break;
        case chk_wchar_t:
          (void)fprintf(f_display, "wchar_t\n");
          break;
        case chk_char8_t:
          (void)fprintf(f_display, "char8_t\n");
          break;
        case chk_char16_t:
          (void)fprintf(f_display, "char16_t\n");
          break;
        case chk_char32_t:
          (void)fprintf(f_display, "char32_t\n");
          break;
        default:
          (void)fprintf(f_display, "**BAD CHARACTER KIND**");
          break;
      }  /* switch */
      disp_host_large_unsigned(
                  "length", (a_host_large_unsigned)ptr->variant.string.length);
#if DO_IL_LOWERING && ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS
      if (ptr->variant.string.sequence_number) {
        disp_host_large_unsigned("sequence_number",
                   (a_host_large_unsigned)ptr->variant.string.sequence_number);
      }  /* if */
#endif /* DO_IL_LOWERING && ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS */
      disp_name("literal_kind");
      (void)fprintf(f_display, "%s\n",
                   readable_literal_kind(ptr->variant.string.literal_kind));
      if (ptr->variant.string.func_name_tok) {
        disp_boolean("func_name_tok", TRUE);
      }  /* if */
      if (ptr->variant.string.embed_expansion) {
        disp_boolean("embed_expansion", TRUE);
      }  /* if */
#if PRESERVE_EMBED_DIRECTIVE_WHEN_OPTIMIZED
      if (ptr->variant.string.embed_directive != NULL) {
        disp_string_ptr("embed_directive", ptr->variant.string.embed_directive,
                        iek_other_text, (sizeof_t)0);
      }  /* if */
#endif /* PRESERVE_EMBED_DIRECTIVE_WHEN_OPTIMIZED */
      disp_name("value");
display_constant_value:
      summarize_constant(ptr);
      (void)fprintf(f_display, "\n");
      break;
    case ck_float:
      (void)fprintf(f_display, "ck_float\n");
      disp_name("float_value");
      goto display_constant_value;
#if C99_IL_EXTENSIONS_SUPPORTED
    case ck_complex:
      (void)fprintf(f_display, "ck_complex\n");
      disp_name("complex_value");
      goto display_constant_value;
    case ck_imaginary:
      (void)fprintf(f_display, "ck_imaginary\n");
      disp_name("float_value");
      goto display_constant_value;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case ck_address:
      (void)fprintf(f_display, "ck_address\n");
      disp_name("address.kind");
      switch (ptr->variant.address.kind) {
        case abk_routine:
          (void)fprintf(f_display, "abk_routine\n");
          disp_ptr("routine", (char *)ptr->variant.address.variant.routine,
                   iek_routine);
          break;
        case abk_variable:
          (void)fprintf(f_display, "abk_variable\n");
          disp_ptr("variable", (char *)ptr->variant.address.variant.variable,
                   iek_variable);
          break;
        case abk_constant:
          (void)fprintf(f_display, "abk_constant\n");
          disp_ptr("constant", (char *)ptr->variant.address.variant.constant,
                   iek_constant);
          break;
        case abk_temporary:
          (void)fprintf(f_display, "abk_temporary\n");
          disp_ptr("temporary", (char *)ptr->variant.address.variant.constant,
                   iek_constant);
          break;
        case abk_uuidof:
          (void)fprintf(f_display, "abk_uuidof\n");
          disp_ptr("type", (char *)ptr->variant.address.variant.type,
                   iek_type);
          break;
        case abk_typeid:
          (void)fprintf(f_display, "abk_typeid\n");
          disp_ptr("type", (char *)ptr->variant.address.variant.type,
                   iek_type);
          break;
#if MICROSOFT_EXTENSIONS_ALLOWED
        case abk_cli_typeid:
          (void)fprintf(f_display, "abk_cli_typeid\n");
          disp_ptr("type", (char *)ptr->variant.address.variant.type,
                   iek_type);
          break;
        case abk_cli_array:
          (void)fprintf(f_display, "abk_cli_array\n");
          /* No variant fields. */
          break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        case abk_label:
          (void)fprintf(f_display, "abk_label\n");
          disp_ptr("label", (char *)ptr->variant.address.variant.label,
                   iek_label);
          break;
        case abk_param_ref:
          (void)fprintf(f_display, "abk_param_ref\n");
          disp_unsigned_long(
             "param_num",
             (unsigned long)ptr->variant.address.variant.param_ref.param_num);
          break;
        default:
          (void)fprintf(f_display, "**BAD ADDRESS CONSTANT KIND**\n");
      }  /* switch */
      disp_ptr("subobject_path", (char *)ptr->variant.address.subobject_path,
               iek_subobject_path);
      disp_host_large_integer(
          "address.offset", (a_host_large_integer)ptr->variant.address.offset);
      if (ptr->variant.address.one_past_the_end) {
        disp_boolean("address.one_past_the_end", TRUE);
      }  /* if */
      break;
    case ck_ptr_to_member:
      (void)fprintf(f_display, "ck_ptr_to_member\n");
      disp_ptr("casting_base_class",
               (char *)ptr->variant.ptr_to_member.casting_base_class,
               iek_base_class);
      disp_ptr("name_reference",
               (char *)ptr->variant.ptr_to_member.name_reference,
               iek_name_reference);
      disp_boolean("cast_to_base",
                   (a_boolean)ptr->variant.ptr_to_member.cast_to_base);
      disp_boolean("is_function_ptr",
                   (a_boolean)ptr->variant.ptr_to_member.is_function_ptr);
      if (ptr->variant.ptr_to_member.is_function_ptr) {
        disp_ptr("routine", (char *)ptr->variant.ptr_to_member.variant.routine,
                 iek_routine);
      } else {
        disp_ptr("field", (char *)ptr->variant.ptr_to_member.variant.field,
                 iek_field);
      }  /* if */
      break;
#if GNU_EXTENSIONS_ALLOWED
    case ck_label_difference:
      (void)fprintf(f_display, "ck_label_difference\n");
      disp_ptr("from_address",
               (char*)ptr->variant.label_difference.from_address,
               iek_constant);
      disp_ptr("to_address", (char*)ptr->variant.label_difference.to_address,
               iek_constant);
      break;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING && GENERATE_EH_TABLES && !DO_FULL_PORTABLE_EH_LOWERING
    case ck_stack_offset:
      (void)fprintf(f_display, "ck_stack_offset\n");
      disp_ptr("variable",
               (char *)ptr->variant.stack_offset.variable,
               iek_variable);
      disp_unsigned_long("offset",
                         (unsigned long)ptr->variant.stack_offset.offset);
      break;
#endif /* DO_IL_LOWERING && ... */
    case ck_dynamic_init:
      (void)fprintf(f_display, "ck_dynamic_init\n");
      disp_ptr("dynamic_init", (char *)ptr->variant.dynamic_init.ptr,
               iek_dynamic_init);
      break;
    case ck_aggregate:
      (void)fprintf(f_display, "ck_aggregate\n");
      disp_ptr("first_constant", (char *)ptr->variant.aggregate.first_constant,
               iek_constant);
      disp_ptr("last_constant", (char *)ptr->variant.aggregate.last_constant,
               iek_constant);
      if (ptr->variant.aggregate.has_dynamic_init_component) {
        disp_boolean("has_dynamic_init_component", TRUE);
      }  /* if */
      if (ptr->variant.aggregate.added_const_for_template_param) {
        disp_boolean("added_const_for_template_param", TRUE);
      }  /* if */
      break;
    case ck_init_repeat:
      (void)fprintf(f_display, "ck_init_repeat\n");
      disp_ptr("constant", (char *)ptr->variant.init_repeat.constant,
               iek_constant);
      disp_host_large_unsigned(
               "count", (a_host_large_unsigned)ptr->variant.init_repeat.count);
      if (ptr->variant.init_repeat.multidimensional_aggr_tail_not_repeated) {
        disp_boolean("multidimensional_aggr_tail_not_repeated", TRUE);
      }  /* if */
      break;
    case ck_designator:
      (void)fprintf(f_display, "ck_designator\n");
      if (ptr->variant.designator.is_field_designator) {
        disp_boolean("is_field_designator", TRUE);
      }  /* if */
      if (ptr->variant.designator.is_generic) {
        disp_boolean("is_generic", TRUE);
      }  /* if */
      if (ptr->variant.designator.uses_direct_init_syntax) {
        disp_boolean("uses_direct_init_syntax", TRUE);
      }  /* if */
      if (ptr->variant.designator.is_field_designator) {
        /* A field designator: */
        if (ptr->variant.designator.is_generic) {
          disp_string_ptr("field_name",
                          ptr->variant.designator.variant.field_name,
                          iek_id_name,
                          (sizeof_t)0);
        } else {
          disp_string_ptr("field",
                          ptr->variant.designator.variant.field
                             ->source_corresp.name,
                          iek_id_name,
                          (sizeof_t)0);
        }  /* if */
      } else {
        /* An array element designator: */
        if (ptr->variant.designator.is_generic) {
          disp_ptr("subscript",
                   (char *)ptr->variant.designator.variant.subscript,
                   iek_constant);
        } else {
          disp_host_large_unsigned(
                 "array_element",
                 (a_host_large_unsigned)
                               ptr->variant.designator.variant.array_element);
        }  /* if */
      }  /* if */
      break;
    case ck_reflection:
      (void)fprintf(f_display, "ck_reflection\n");
      disp_ptr("entity", (char *)ptr->variant.reflection.entity.ptr,
               (an_il_entry_kind)ptr->variant.reflection.entity.kind);
      disp_int32("local_scope_number",
                 ptr->variant.reflection.local_scope_number);
      break;
    case ck_template_param:
      disp_template_param_constant(ptr);
      break;
    case ck_void:
      (void)fprintf(f_display, "ck_void\n");
      break;
    default:
      fprintf(f_display, "**BAD CONSTANT KIND**\n");
  }  /* switch */
}  /* disp_constant */


static void disp_param_type(a_param_type_ptr ptr)
/*
Display a_param_type entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_param_type);
  disp_ptr("type", (char *)ptr->type, iek_type);
  disp_ptr("declared_type", (char *)ptr->declared_type, iek_type);
  if (ptr->name != NULL) {
    disp_string_ptr("name", ptr->name, iek_id_name, (sizeof_t)0);
  }  /* if */
  if (ptr->has_name_conflict) {
    disp_boolean("has_name_conflict", TRUE);
  }  /* if */
  if (ptr->passed_via_copy_constructor) {
    disp_boolean("passed_via_copy_constructor", TRUE);
  }  /* if */
  if (ptr->has_default_arg) {
    disp_boolean("has_default_arg", TRUE);
  }  /* if */
  if (ptr->default_arg_appeared_in_class_definition) {
    disp_boolean("default_arg_appeared_in_class_definition", TRUE);
  }  /* if */
  if (ptr->has_unevaluated_template_default) {
    disp_boolean("has_unevaluated_template_default", TRUE);
  }  /* if */
  if (ptr->default_being_instantiated) {
    disp_boolean("default_being_instantiated", TRUE);
  }  /* if */
  if (ptr->type_involves_deduced_template_param) {
    disp_boolean("type_involves_deduced_template_param", TRUE);
  }  /* if */
  if (ptr->type_involves_template_param) {
    disp_boolean("type_involves_template_param", TRUE);
  }  /* if */
  if (ptr->is_parameter_pack) {
    disp_boolean("is_parameter_pack", TRUE);
  }  /* if */
  if (ptr->is_pack_element) {
    disp_boolean("is_pack_element", TRUE);
  }  /* if */
  if (ptr->was_nontrailing_pack) {
    disp_boolean("was_nontrailing_pack", TRUE);
  }  /* if */
  if (ptr->is_auto_param) {
    disp_boolean("is_auto_param", TRUE);
  }  /* if */
  disp_uint32("param_num", ptr->param_num);
  if (ptr->default_arg_expr != NULL) {
    disp_ptr("default_arg_expr", (char *)ptr->default_arg_expr, iek_expr_node);
  }  /* if */
  if (ptr->entities_defined_in_default_arg != NULL) {
    disp_entity_list("entities_defined_in_default_arg",
                     ptr->entities_defined_in_default_arg);
  }  /* if */
  if (ptr->qualifiers != TQ_NONE) {
    disp_name("qualifiers");
    form_type_qualifier((a_type_qualifier_set)ptr->qualifiers,
                        UPC_BLOCK_SIZE_NONE,
                        /*need_trailing_space=*/FALSE, &octl);
    (void)fprintf(f_display, "\n");
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  if (ptr->is_transparent) {
    disp_boolean("is_transparent", TRUE);
  }  /* if */
  if (ptr->nonnull) {
    disp_boolean("nonnull", TRUE);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (ptr->duplicate_name) {
    disp_boolean("duplicate_name", TRUE);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ptr->is_cli_param_array) {
    disp_boolean("is_cli_param_array", TRUE);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (ptr->move_ctor_or_assign_parameter) {
    disp_boolean("move_ctor_or_assign_parameter", TRUE);
  }  /* if */
  if (ptr->copy_or_move_ctor_parameter) {
    disp_boolean("copy_or_move_ctor_parameter", TRUE);
  }  /* if */
  if (ptr->attributes != NULL) {
    disp_ptr("attributes", (char *)ptr->attributes, iek_attribute);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ptr->ms_attributes != NULL) {
    disp_ptr("ms_attributes", (char *)ptr->ms_attributes, iek_ms_attribute);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (ptr->decl_pos_info != NULL) {
    disp_source_range("identifier_range",
                      &ptr->decl_pos_info->identifier_range);
    disp_source_range("specifiers_range",
                      &ptr->decl_pos_info->specifiers_range);
    disp_source_range("declarator_range",
                      &ptr->decl_pos_info->variant.declarator_range);
    disp_ptr("extra_positions", (char*)ptr->decl_pos_info->extra_positions,
             iek_element_position);
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* pack_expansion_descr is not displayed because it is front end only. */
}  /* disp_param_type */


static void disp_pragma_kind_name(a_pragma_kind  kind)
/*
Print the name of a pragma kind.  Actually, the pragma ID (the name
used in the #pragma directive) is displayed.
*/
{
  a_const_char *s;

  s = pragma_ids[(int)kind];
  (void)fprintf(f_display, "%s\n", s);
}  /* disp_pragma_kind_name */


static void disp_type_qualifiers(a_type_qualifier_set qualifiers)
/*
Display a set of type qualifiers (e.g., const, volatile).
*/
{
  form_type_qualifier(qualifiers, UPC_BLOCK_SIZE_NONE,
                      /*need_trailing_space=*/FALSE, &octl);
}  /* disp_type_qualifiers */


static void disp_routine_type_supplement(a_routine_type_supplement_ptr ptr)
/*
Display a_routine_type_supplement.
*/
{
  disp_ptr("param_type_list", (char *)ptr->param_type_list, iek_param_type);
  disp_ptr("assoc_routine", (char *)ptr->assoc_routine, iek_routine);
  if (ptr->has_ellipsis) {
    disp_boolean("has_ellipsis", (a_boolean)ptr->has_ellipsis);
  }  /* if */
  disp_boolean("prototyped", (a_boolean)ptr->prototyped);
  if (ptr->trailing_return_type) {
    disp_boolean("trailing_return_type", TRUE);
  }  /* if */
  if (ptr->lint_argsused_flag) {
    disp_boolean("lint_argsused_flag", TRUE);
  }  /* if */
  if (ptr->value_returned_by_cctor) {
    disp_boolean("value_returned_by_cctor", TRUE);
  }  /* if */
#if DO_IL_LOWERING
  if (ptr->value_returned_as_parameter) {
    disp_boolean("value_returned_as_parameter", TRUE);
  }  /* if */
  if (ptr->return_value_parameter_follows_this) {
    disp_boolean("return_value_parameter_follows_this", TRUE);
  }  /* if */
#endif /* DO_IL_LOWERING */
  if (ptr->assoc_routine_is_ctor) {
    disp_boolean("assoc_routine_is_ctor", TRUE);
  }  /* if */
  if (ptr->assoc_routine_is_dtor) {
    disp_boolean("assoc_routine_is_dtor", TRUE);
  }  /* if */
  if (ptr->assoc_routine_is_lambda_body) {
    disp_boolean("assoc_routine_is_lambda_body", TRUE);
  }  /* if */
  if (ptr->routine_name_linkage != (a_name_linkage_kind)nlk_none) {
    disp_name_linkage("routine_name_linkage",
                      (a_name_linkage_kind)ptr->routine_name_linkage);
    if (ptr->routine_name_linkage_is_explicit) {
      disp_boolean("routine_name_linkage_is_explicit", TRUE);
    }  /* if */
  }  /* if */
  if (ptr->does_not_return) {
    disp_boolean("does_not_return", TRUE);
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  if (ptr->result_should_be_used) {
    disp_boolean("result_should_be_used", TRUE);
  }  /* if */
  if (ptr->has_enable_if_attribute) {
    disp_boolean("has_enable_if_attribute", TRUE);
  }  /* if */
  if (ptr->is_const) {
    disp_boolean("is_const", TRUE);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (ptr->is_variadic_instance) {
    disp_boolean("is_variadic_instance", TRUE);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED
  if (ptr->explicit_calling_convention) {
    disp_boolean("explicit_calling_convention", TRUE);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED */
  if (ptr->had_been_implicitly_const) {
    disp_boolean("had_been_implicitly_const", TRUE);
  }  /* if */
  if (ptr->is_conditionally_explicit) {
    disp_boolean("is_conditionally_explicit", TRUE);
  }  /* if */
  if (ptr->has_this_param) {
    disp_boolean("has_this_param", TRUE);
  }  /* if */
  if (ptr->lint_varargs_count != NOT_LINT_VARARGS) {
    disp_long("lint_varargs_count", (long)ptr->lint_varargs_count);
  }  /* if */
  if (ptr->arg_pragma != (a_pragma_kind)pk_none) {
    disp_name("arg_pragma");
    disp_pragma_kind_name(ptr->arg_pragma);
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  disp_long("fmt_arg", (long)ptr->fmt_arg);
  disp_long("format_first_subst_arg", (long)ptr->format_first_subst_arg);
  if (ptr->sentinel_pos != 0) {
    disp_long("sentinel_pos", (long)ptr->sentinel_pos);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED
  disp_name("calling_convention");
  (void)fprintf(f_display, "%s\n",
                calling_convention_names[(int)ptr->calling_convention]);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED */
  if (ptr->this_class != NULL) {
    disp_ptr("this_class", (char *)ptr->this_class, iek_type);
  }  /* if */
  if (ptr->qualifiers != TQ_NONE) {
    disp_name("qualifiers");
    disp_type_qualifiers(ptr->qualifiers);
    (void)fprintf(f_display, "\n");
  }  /* if */
  if (ptr->this_qualifiers != TQ_NONE) {
    disp_name("this_qualifiers");
    disp_type_qualifiers(ptr->this_qualifiers);
    (void)fprintf(f_display, "\n");
  }  /* if */
  if (ptr->ref_qualifiers != (a_ref_qualifier_kind)rqk_default) {
    disp_name("ref_qualifiers");
    switch (ptr->ref_qualifiers) {
      case rqk_lvalue:
        fprintf(f_display, "&\n");
        break;
      case rqk_rvalue:
        fprintf(f_display, "&&\n");
        break;
      default:
        fprintf(f_display, "**BAD REF-QUALIFIER KIND**\n");
        break;
    }  /* switch */
  }  /* if */
  if (ptr->prototype_scope != NULL) {
    disp_ptr("prototype_scope", (char *)ptr->prototype_scope, iek_scope);
  }  /* if */
  if (ptr->exception_specification != NULL) {
    disp_ptr("exception_specification", (char *)ptr->exception_specification,
             iek_exception_specification);
  }  /* if */
}  /* disp_routine_type_supplement */


static void disp_based_type_list(a_based_type_list_member_ptr ptr)
/*
Display the indicated based type list.
*/
{
  a_const_char *kind_str;

  if (ptr == NULL) {
    disp_ptr("based_types", (char *)ptr, iek_based_type_list_member);
  } else {
    disp_name("based_types");
    (void)fprintf(f_display, "\n");
    for (; ptr != NULL; ptr = ptr->next) {
      switch (ptr->kind) {
        case btk_qualified:      kind_str = "  qualified";               break;
        case btk_rvalue_reference:
                                 kind_str = "  rvalue reference";        break;
        case btk_reference:      kind_str = "  reference";               break;
        case btk_ptr_to_member:  kind_str = "  ptr_to_member";           break;
        case btk_unqualified_array_type:
                                 kind_str = "  unqualified_array_type";  break;
#if MICROSOFT_EXTENSIONS_ALLOWED
        case btk_handle:         kind_str = "  handle";                  break;
        case btk_tracking_ref:   kind_str = "  tracking reference";      break;
        case btk_interior_ptr:   kind_str = "  interior_ptr";            break;
        case btk_pin_ptr:        kind_str = "  pin_ptr";                 break;
        case btk_cppcx_box:      kind_str = "  cppcx_box";               break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        case btk_pointer:        kind_str = "  pointer";                 break;
        default:                 kind_str = "  **BAD BASED TYPE KIND**"; break;
      }  /* switch */
      disp_ptr(kind_str, (char *)ptr->based_type, iek_type);
    }  /* for */
  }  /* if */
}  /* disp_based_type_list */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void disp_generic_constraint_clause(a_generic_constraint_clause_ptr ptr)
/*
Display the indicated generic constraint clause.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_generic_constraint_clause);
  disp_ptr("type", (char *)ptr->type, iek_type);
  disp_source_position("type_position", &ptr->type_position);
  disp_ptr("constraints", (char *)ptr->constraints, iek_generic_constraint);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_source_position("where_position", &ptr->where_position);
  disp_source_position("colon_position", &ptr->colon_position);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* disp_generic_constraint_clause */


static void disp_generic_constraint(a_generic_constraint_ptr ptr)
/*
Display the indicated generic constraint.
*/
{
  a_const_char  *kind_str;

  switch (ptr->kind) {
    case gck_none:             kind_str = "unknown/invalid";      break;
    case gck_type:             kind_str = "type";                 break;
    case gck_naked_type_param: kind_str = "naked type parameter"; break;
    case gck_ref_class:        kind_str = "ref class";            break;
    case gck_value_class:      kind_str = "value class";          break;
    case gck_gcnew:            kind_str = "gcnew";                break;
    case gck_fail:             kind_str = "fail";                 break;
    default:                   kind_str = "**BAD CONSTRAINT KIND**";
  }  /* switch */
  disp_name("kind");
  (void)fprintf(f_display, "%s\n", kind_str);
  if (ptr->implicit_constraint) {
    disp_boolean("implicit_constraint", TRUE);
  }  /* if */
  disp_ptr("next", (char *)ptr->next, iek_generic_constraint);
  if (ptr->type != NULL) {
    disp_ptr("type", (char *)ptr->type, iek_type);
  }  /* if */
  /* Do not display type_cache: Front end pointer only. */
  disp_source_position("position", &ptr->position);
}  /* disp_generic_constraint */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void disp_template_param_type_supplement(
                                      a_template_param_type_supplement_ptr ptr)
/*
Display the indicated template parameter type supplement.
*/
{
  disp_ptr("class_type", (char *)ptr->class_type, iek_type);
  if (ptr->orig_nested_type != NULL) {
    disp_ptr("orig_nested_type", (char *)ptr->orig_nested_type, iek_type);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ptr->generic_constraints != NULL) {
    disp_ptr("generic_constraints", (char *)ptr->generic_constraints,
             iek_generic_constraint);
  }  /* if */
  if (ptr->generic_param_seq_number > 0) {
    disp_int32("generic_param_seq_number", ptr->generic_param_seq_number);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  disp_template_param_coordinate(&ptr->coordinates);
  if (ptr->coordinates.depth == BIT_PRECISE_INT_NESTING_DEPTH) {
    disp_ptr("constraint.bit_width_constant",
             (char *)ptr->constraint.bit_width_constant, iek_constant);
  } else if (ptr->coordinates.depth !=
                         CLASS_TEMPLATE_PLACEHOLDER_NESTING_DEPTH &&
      ptr->constraint.type_constraint != NULL) {
    disp_ptr("constraint.type_constraint",
             (char *)ptr->constraint.type_constraint, iek_expr_node);
  }  /* if */
}  /* disp_template_param_type_supplement */


static void disp_typeref_type_supplement(a_typeref_type_supplement_ptr ptr)
/*
Display the indicated typeref type supplement.
*/
{
  if (ptr->template_arg_list != NULL) {
    disp_template_arg_list("template_arg_list", ptr->template_arg_list);
  }  /* if */
  if (ptr->orig_template_arg_list != NULL) {
    disp_template_arg_list("orig_template_arg_list",
                           ptr->orig_template_arg_list);
  }  /* if */
#if DEFAULT_RECORD_FORM_OF_NAME_REFERENCE
  if (ptr->name_qualifier != NULL) {
    disp_ptr("qualifier", (char *)ptr->name_qualifier, iek_name_qualifier);
  }  /* if */
#endif /* DEFAULT_RECORD_FORM_OF_NAME_REFERENCE */
  if (ptr->assoc_template != NULL) {
    disp_ptr("assoc_template", (char*)ptr->assoc_template, iek_template);
  }  /* if */
  if (ptr->expr != NULL) {
    disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
  }  /* if */
  if (ptr->min_template_arguments >= 0) {
    disp_int32("min_template_arguments", ptr->min_template_arguments);
  }  /* if */
  if (ptr->operator_type_arg != NULL) {
    disp_ptr("operator_type_arg", (char *)ptr->operator_type_arg, iek_type);
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_source_range("type_id_range", &ptr->type_id_range);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* disp_typeref_type_supplement */


static void disp_integer_type_supplement(an_integer_type_supplement_ptr  ptr)
/*
Display the indicated integer type supplement.
*/
{
  if (ptr->enumerator_list_seen) disp_boolean("enumerator_list_seen", TRUE);
  if (ptr->enumerator_list_complete) {
    disp_boolean("enumerator_list_complete", TRUE);
  }  /* if */
  if (ptr->has_nodiscard_attribute) {
    disp_boolean("has_nodiscard_attribute", TRUE);
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  if (ptr->underlying_type_should_use_unsigned) {
    disp_boolean("underlying_type_should_use_unsigned", TRUE);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  disp_assembly_visibility("declared_assembly_visibility",
                           enum_cast<an_assembly_visibility>(
                                           ptr->declared_assembly_visibility));
  disp_assembly_visibility("assembly_visibility",
                           enum_cast<an_assembly_visibility>(
                                                    ptr->assembly_visibility));
  if (ptr->uuid_string != NULL) {
    disp_string_ptr("uuid_string", ptr->uuid_string,
                    iek_other_text, (sizeof_t)0);
  }  /* if */
  if (ptr->boxed_type != NULL) {
    disp_ptr("boxed_type", (char *)ptr->boxed_type, iek_type);
  }  /* if */
  /* uuid_variable not displayed since it is used for IL lowering only. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (ptr->base_type != NULL) {
    disp_ptr("base_type", (char *)ptr->base_type, iek_type);
    disp_source_position("base_type_position", &ptr->base_type_position);
  }  /* if */
  if (ptr->assoc_template != NULL) {
    disp_ptr("assoc_template", (char*)ptr->assoc_template, iek_template);
  }  /* if */
}  /* disp_integer_type_supplement */


static a_const_char* type_kind_string(a_type_kind  type_kind)
/*
Return a string corresponding to the indicated type kind.
*/
{
  a_const_char  *str;

  switch (type_kind) {
    case tk_error:
      str = "tk_error";
      break;
    case tk_unknown:
      str = "tk_unknown";
      break;
    case tk_void:
      str = "tk_void";
      break;
    case tk_integer:
      str = "tk_integer";
      break;
#if FIXED_POINT_ALLOWED
    case tk_fixed_point:
      str = "tk_fixed_point";
      break;
#endif /* FIXED_POINT_ALLOWED */
    case tk_float:
      str = "tk_float";
      break;
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_imaginary:
      str = "tk_imaginary";
      break;
    case tk_complex:
      str = "tk_complex";
      break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case tk_pointer:
      str = "tk_pointer";
      break;
    case tk_routine:
      str = "tk_routine";
      break;
    case tk_array:
      str = "tk_array";
      break;
    case tk_class:
      str = "tk_class";
      break;
    case tk_struct:
      str = "tk_struct";
      break;
    case tk_union:
      str = "tk_union";
      break;
    case tk_typeref:
      str = "tk_typeref";
      break;
    case tk_ptr_to_member:
      str = "tk_ptr_to_member";
      break;
    case tk_template_param:
      str = "tk_template_param";
      break;
#if GNU_VECTOR_TYPES_ALLOWED
    case tk_vector:
      str = "tk_vector";
      break;
    case tk_scalable_vector:
      str = "tk_scalable_vector";
      break;
    case tk_scalable_vector_count:
      str = "tk_scalable_vector_count";
      break;
    case tk_riscv_vector:
      str = "tk_riscv_vector";
      break;
    case tk_mfp8:
      str = "tk_mfp8";
      break;
    case tk_float8e4m3:
      str = "tk_float8e4m3";
      break;
    case tk_float8e5m2:
      str = "tk_float8e5m2";
      break;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
    case tk_nullptr:
      str = "tk_nullptr";
      break;
    default:
      str = "**BAD TYPE KIND**";
  }  /* switch */
  return str;
}  /* type_kind_string */

#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED

static void disp_ELF_visibility_kind(an_ELF_visibility_kind  ELF_visibility)
/*
Display an ELF_visibility field.
*/
{
  a_const_char  *str;

  disp_name("ELF_visibility");
  switch (ELF_visibility) {
    case evk_unspecified: str = "evk_unspecified";             break;
    case evk_hidden:      str = "evk_hidden";                  break;
    case evk_protected:   str = "evk_protected";               break;
    case evk_internal:    str = "evk_internal";                break;
    case evk_default:     str = "evk_default";                 break;
    default:              str = "**BAD ELF VISIBILITY KIND**";
  }  /* switch */
  (void)fprintf(f_display, "%s\n", str);
}  /* disp_ELF_visibility_kind */

#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */

static void disp_typeref_kind(a_typeref_kind kind)
/*
Display a typeref kind.
*/
{
  a_const_char  *str;

  disp_name("kind");
  switch (kind) {
    case trk_none:                     str = "none";                     break;
    case trk_is_decltype:              str = "is_decltype";              break;
    case trk_is_deduced_decltype_auto: str = "is_deduced_decltype_auto"; break;
    case trk_is_deduced_auto:          str = "is_deduced_auto";          break;
    case trk_is_deduced_class:         str = "is_deduced_class";         break;
    case trk_is_underlying_type:       str = "is_underlying_type";       break;
    case trk_is_typeof_with_expression:str = "is_typeof_with_expression";break;
    case trk_is_typeof_with_type_operand:
                                    str = "is_typeof_with_type_operand"; break;
    case trk_for_type_attributes:      str = "for_type_attributes";      break;
    case trk_is_alias:                 str = "is_alias";                 break;
    case trk_is_template_alias:        str = "is_template_alias";        break;
    case trk_is_splice:                str = "is_splice";                break;
    case trk_bases:                    str = "bases";                    break;
    case trk_direct_bases:             str = "direct_bases";             break;
    case trk_add_lvalue_reference:     str = "add_lvalue_reference";     break;
    case trk_add_pointer:              str = "add_pointer";              break;
    case trk_add_rvalue_reference:     str = "add_rvalue_reference";     break;
    case trk_decay:                    str = "decay";                    break;
    case trk_make_signed:              str = "make_signed";              break;
    case trk_make_unsigned:            str = "make_unsigned";            break;
    case trk_remove_all_extents:       str = "remove_all_extents";       break;
    case trk_remove_const:             str = "remove_const";             break;
    case trk_remove_cv:                str = "remove_cv";                break;
    case trk_remove_cvref:             str = "remove_cvref";             break;
    case trk_remove_extent:            str = "remove_extent";            break;
    case trk_remove_pointer:           str = "remove_pointer";           break;
    case trk_remove_reference_t:       str = "remove_reference_t";       break;
    case trk_remove_restrict:          str = "remove_restrict";          break;
    case trk_remove_volatile:          str = "remove_volatile";          break;
    case trk_template_arg_list:        str = "template_arg_list";        break;
    case trk_name_qualifier:           str = "name_qualifier";           break;
    case trk_pack_index:               str = "pack_index";               break;
    default:                           str = "**BAD TYPEREF KIND**";     break;
  }  /* switch */
  (void)fprintf(f_display, "%s\n", str);
}  /* disp_typeref_kind */


static void disp_type(a_type_ptr ptr)
/*
Display the indicated type entry.
*/
{
  disp_source_corresp(&ptr->source_corresp, iek_type);
  disp_ptr("next", (char *)ptr->next, iek_type);
  disp_based_type_list(ptr->based_types);
  disp_host_large_unsigned("size", (a_host_large_unsigned)ptr->size);
  disp_unsigned_long("alignment", (unsigned long)ptr->alignment);
  if (ptr->incomplete) {
    disp_boolean("incomplete", TRUE);
  }  /* if */
  if (ptr->used_in_exception_or_rtti) {
    disp_boolean("used_in_exception_or_rtti", TRUE);
  }  /* if */
  if (ptr->declared_in_function_prototype) {
    disp_boolean("declared_in_function_prototype", TRUE);
  }  /* if */
  if (ptr->is_tag_redefinition) {
    disp_boolean("is_tag_redefinition", TRUE);
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
  if (ptr->alignment_set_explicitly) {
    disp_boolean("alignment_set_explicitly", TRUE);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
#if BACK_END_IS_CP_GEN_BE
  if (ptr->elab_type_spec_needed_in_some_scope) {
    disp_boolean("elab_type_spec_needed_in_some_scope", TRUE);
  }  /* if */
#endif /* BACK_END_IS_CP_GEN_BE */
#if GNU_EXTENSIONS_ALLOWED
  if (ptr->variables_are_implicitly_referenced) {
    disp_boolean("variables_are_implicitly_referenced", TRUE);
  }  /* if */
  if (ptr->may_alias) {
    disp_boolean("may_alias", TRUE);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ptr->has_microsoft_w64_specifier) {
    disp_boolean("has_microsoft_w64_specifier", TRUE);
  }  /* if */
  if (ptr->is_microsoft_intrinsic) {
    disp_boolean("is_microsoft_intrinsic", TRUE);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
  if (ptr->use_cfront_transitional_nested_type_name_mangling) {
    disp_boolean("use_cfront_transitional_nested_type_name_mangling", TRUE);
  }  /* if */
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (ptr->autonomous_primary_tag_decl) {
    disp_boolean("autonomous_primary_tag_decl", TRUE);
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if (ptr->is_builtin_va_list) {
    disp_boolean("is_builtin_va_list", TRUE);
  }  /* if */
  if (ptr->is_builtin_va_list_from_cstdarg) {
    disp_boolean("is_builtin_va_list_from_cstdarg", TRUE);
  }  /* if */
#ifdef GUARD_MACRO_FOR_VA_LIST
  if (ptr->va_list_guard_macro_was_defined) {
    disp_boolean("va_list_guard_macro_was_defined", TRUE);
  }  /* if */
#endif /* ifdef GUARD_MACRO_FOR_VA_LIST */
#ifdef GUARD_MACRO2_FOR_VA_LIST
  if (ptr->va_list_guard_macro2_was_defined) {
    disp_boolean("va_list_guard_macro2_was_defined", TRUE);
  }  /* if */
#endif /* ifdef GUARD_MACRO2_FOR_VA_LIST */
#if GNU_EXTENSIONS_ALLOWED
  if (ptr->has_gnu_abi_tag_attribute) {
    disp_boolean("has_gnu_abi_tag_attribute", TRUE);
  }  /* if */
  if (ptr->in_gnu_abi_tag_namespace) {
    disp_boolean("in_gnu_abi_tag_namespace", TRUE);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING
  if (ptr->typeinfo_var != NULL) {
    disp_ptr("typeinfo_var", (char *)ptr->typeinfo_var, iek_variable);
  }  /* if */
#endif /* DO_IL_LOWERING */
  disp_name("kind");
  (void)fprintf(f_display, "%s\n", type_kind_string(ptr->kind));
  /* Display variant fields (if applicable). */
  switch (ptr->kind) {
    case tk_integer:
      disp_name("int_kind");
      (void)fprintf(f_display, "%s\n", int_type_name(ptr));
      if (ptr->variant.integer.explicitly_signed) {
        disp_boolean("explicitly_signed", TRUE);
      }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (ptr->variant.integer.microsoft_sized_int_type) {
        disp_boolean("microsoft_sized_int_type", TRUE);
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      if (ptr->variant.integer.has_explicit_enum_base) {
        disp_boolean("has_explicit_enum_base", TRUE);
      }  /* if */
      if (ptr->variant.integer.wchar_t_type) {
        disp_boolean("wchar_t_type", TRUE);
      }  /* if */
      if (ptr->variant.integer.char8_t_type) {
        disp_boolean("char8_t_type", TRUE);
      }  /* if */
      if (ptr->variant.integer.char16_t_type) {
        disp_boolean("char16_t_type", TRUE);
      }  /* if */
      if (ptr->variant.integer.char32_t_type) {
        disp_boolean("char32_t_type", TRUE);
      }  /* if */
      if (ptr->variant.integer.bool_type) {
        disp_boolean("bool_type", TRUE);
      }  /* if */
      if (ptr->variant.integer.enum_type) {
        disp_boolean("enum_type", TRUE);
        if (integer_type_is_scoped_enum(ptr)) {
          disp_boolean("is_scoped_enum", TRUE);
        }  /* if */       
#if GNU_EXTENSIONS_ALLOWED
	if (ptr->variant.integer.packed) {
	  disp_boolean("packed", TRUE);
	}  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
        if (ptr->variant.integer.originally_unnamed) {
          disp_boolean("originally_unnamed", TRUE);
        }  /* if */
        if (ptr->variant.integer.is_template_enum) {
          disp_boolean("is_template_enum", TRUE);
        }  /* if */
        if (ptr->variant.integer.is_prototype_instantiation) {
          disp_boolean("is_prototype_instantiation", TRUE);
        }  /* if */
        if (ptr->variant.integer.is_nonreal) {
          disp_boolean("is_nonreal", TRUE);
        }  /* if */
        if (ptr->variant.integer.is_specialized) {
          disp_boolean("is_specialized", TRUE);
        }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (ptr->variant.integer.is_ms_instantiated_nonreal_enum) {
          disp_boolean("is_ms_instantiated_nonreal_enum", TRUE);
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
        if (ptr->variant.integer.ELF_visibility != evk_unspecified) {
          disp_ELF_visibility_kind(enum_cast<an_ELF_visibility_kind>(
                                         ptr->variant.integer.ELF_visibility));
        }  /* if */
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
        if (integer_type_is_scoped_enum(ptr)) {
          disp_ptr("enum_info.assoc_scope",
                   (char*)ptr->variant.integer.enum_info.assoc_scope,
                   iek_scope);
        } else {
          disp_ptr("enum_info.constant_list",
                   (char *)ptr->variant.integer.enum_info.constant_list,
                   iek_constant);
        }  /* if */
      } else if (ptr->variant.integer.enum_info.affiliated_type != NULL) {
        disp_ptr("enum_info.affiliated_type",
                 (char *)ptr->variant.integer.enum_info.affiliated_type,
                 iek_type);
      }  /* if */
      disp_integer_type_supplement(ptr->variant.integer.extra_info);
      break;
#if FIXED_POINT_ALLOWED
    case tk_fixed_point:
      {
        a_fixed_point_precision  prec = ptr->variant.fixed_point.precision;
        disp_name("precision");
        fprintf(f_display, "%s\n",
                (prec == (a_fixed_point_precision)fpp_short)   ? "short" :
                (prec == (a_fixed_point_precision)fpp_default) ? "default" :
                (prec == (a_fixed_point_precision)fpp_long)    ? "long" :
                                                                 "*ERROR*");
        disp_boolean("is_unsigned",
                     (a_boolean)ptr->variant.fixed_point.is_unsigned);
        disp_boolean("is_fract_type",
                     (a_boolean)ptr->variant.fixed_point.is_fract_type);
        disp_boolean("saturating",
                     (a_boolean)ptr->variant.fixed_point.saturating);
      }
      break;
#endif /* FIXED_POINT_ALLOWED */
    case tk_float:
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_imaginary:
    case tk_complex:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      disp_name("float_kind");
      (void)fprintf(f_display, "%s\n", float_kind_name(ptr->variant.float_kind,
                                           /*use_C_form=*/TRUE));
      break;
    case tk_pointer:
      disp_ptr("type_pointed_to", (char *)ptr->variant.pointer.type, iek_type);
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (ptr->variant.pointer.base_variable != NULL) {
        disp_ptr("base_variable", (char *)ptr->variant.pointer.base_variable,
                 iek_variable);
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      disp_boolean("is_reference",
                   (a_boolean)ptr->variant.pointer.is_reference);
      if (ptr->variant.pointer.is_rvalue_reference) {
        disp_boolean("is_rvalue_reference", TRUE);
      }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (ptr->variant.pointer.is_handle) {
        disp_boolean("is_handle", TRUE);
      }  /* if */
      if (ptr->variant.pointer.is_interior_ptr) {
        disp_boolean("is_interior_ptr", TRUE);
      }  /* if */
      if (ptr->variant.pointer.is_pin_ptr) {
        disp_boolean("is_pin_ptr", TRUE);
      }  /* if */
      if (ptr->variant.pointer.modifiers != PM_NONE) {
        disp_name("modifiers");
        form_pointer_modifiers(ptr->variant.pointer.modifiers, &octl);
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      break;
    case tk_routine:
      disp_ptr("return_type", (char *)ptr->variant.routine.return_type,
               iek_type);
      disp_routine_type_supplement(ptr->variant.routine.extra_info);
      break;
    case tk_array:
      disp_ptr("element_type", (char *)ptr->variant.array.element_type,
               iek_type);
      if (ptr->variant.array.qualifiers != TQ_NONE) {
        disp_name("qualifiers");
        disp_type_qualifiers(ptr->variant.array.qualifiers);
        (void)fprintf(f_display, "\n");
      }  /* if */
      if (ptr->variant.array.is_static) {
        disp_boolean("is_static", TRUE);
      }  /* if */
      if (ptr->variant.array.is_variable_size_array) {
        disp_boolean("is_variable_size_array", TRUE);
        if (ptr->variant.array.is_vla) {
          disp_boolean("is_vla", TRUE);
          disp_boolean("has_assoc_vla_dimension",
                       (a_boolean)ptr->variant.array.has_assoc_vla_dimension);
        } else {
          disp_ptr("element_count_expr",
                   (char *)ptr->variant.array.variant.element_count_expr,
                   iek_expr_node);
        }  /* if */
      } else if (ptr->variant.array.is_template_dependent_size_array) {
        disp_boolean("is_template_dependent_size_array", TRUE);
        disp_ptr("element_count_constant",
                 (char *)ptr->variant.array.variant.element_count_constant,
                 iek_constant);
      } else {
        disp_host_large_unsigned("number_of_elements",
                                 (a_host_large_unsigned)ptr->
                                    variant.array.variant.number_of_elements);
      }  /* if */
      if (ptr->variant.array.constant_bound_expr_in_local_expr_node_ref) {
        disp_boolean("constant_bound_expr_in_local_expr_node_ref", TRUE);
      }  /* if */
      if (ptr->variant.array.dep_constant_bound_expr_in_local_expr_node_ref) {
        disp_boolean("dep_constant_bound_expr_in_local_expr_node_ref", TRUE);
      }  /* if */
      if (ptr->variant.array.bound_constant != NULL) {
        disp_ptr("bound_constant",
                 (char *)ptr->variant.array.bound_constant, iek_constant);
      }  /* if */
      break;
    case tk_class:
    case tk_struct:
    case tk_union:
      disp_ptr("field_list",
               (char *)ptr->variant.class_struct_union.field_list, iek_field);
      if (ptr->variant.class_struct_union.extra_info != NULL) {
        disp_ptr("extra_info",
                 (char *)ptr->variant.class_struct_union.extra_info,
                 iek_class_type_supplement);
      } else {
        disp_name("extra_info");
        fprintf(f_display, "**BAD (MISSING) CLASS TYPE SUPPLEMENT**\n");
      }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (ptr->variant.class_struct_union.is_interface) {
        disp_boolean("is_interface", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.is_interface_like) {
        disp_boolean("is_interface_like", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.is_delegate_class) {
        disp_boolean("is_delegate_class", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.is_generic_definition) {
        disp_boolean("is_generic_definition", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.is_generic_instance) {
        disp_boolean("is_generic_instance", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.is_open_constructed_type) {
        disp_boolean("is_open_constructed_type", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.is_generic_constraint) {
        disp_boolean("is_generic_constraint", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.is_hybrid_constraint) {
        disp_boolean("is_hybrid_constraint", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.any_interface_constraints) {
        disp_boolean("any_interface_constraints", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.unconstrained) {
        disp_boolean("unconstrained", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.sealed) {
        disp_boolean("sealed", TRUE);
      }  /* if */
#if BACK_END_IS_CP_GEN_BE
      if (ptr->variant.class_struct_union
                                      .defined_with_abstract_class_modifier) {
        disp_boolean("defined_with_abstract_class_modifier", TRUE);
      }  /* if */
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      if (ptr->variant.class_struct_union.final) {
        disp_boolean("final", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.any_const_member) {
        disp_boolean("any_const_member", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.any_volatile_member) {
        disp_boolean("any_volatile_member", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.any_mutable_member) {
        disp_boolean("any_mutable_member", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.any_virtual_base_classes) {
        disp_boolean("any_virtual_base_classes", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.abstract) {
        disp_boolean("abstract", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.any_virtual_functions) {
        disp_boolean("any_virtual_functions", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.any_pure_virtual_functions) {
        disp_boolean("any_pure_virtual_functions", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.
                           any_virtual_functions_including_in_base_classes) {
        disp_boolean("any_virtual_functions_including_in_base_classes", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.
                                      nested_class_defined_outside_of_parent) {
        disp_boolean("nested_class_defined_outside_of_parent", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.originally_unnamed) {
        disp_boolean("originally_unnamed", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.is_nonstd_anonymous_union_type) {
        disp_boolean("is_nonstd_anonymous_union_type", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.is_template_class) {
        disp_boolean("is_template_class", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.is_prototype_instantiation) {
        disp_boolean("is_prototype_instantiation", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.is_nonreal_class) {
        disp_boolean("is_nonreal_class", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.is_specialized) {
        disp_boolean("is_specialized", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.specialized_with_old_syntax) {
        disp_boolean("specialized_with_old_syntax", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.explicitly_instantiated) {
        disp_boolean("explicitly_instantiated", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.do_not_instantiate) {
        disp_boolean("do_not_instantiate", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.proxy_class) {
        disp_boolean("proxy_class", TRUE);
      }  /* if */
#if MAINTAIN_NEEDED_FLAGS
      disp_boolean("definition_needed",
                 (a_boolean)ptr->variant.class_struct_union.definition_needed);
      /* Note: the keep_definition_in_il flag is not displayed, since it is
         for front-end use only. */
#endif /* MAINTAIN_NEEDED_FLAGS */
      if (ptr->variant.class_struct_union.is_empty_class) {
        disp_boolean("is_empty_class", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.no_proper_data) {
        disp_boolean("no_proper_data", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.has_zero_init_component) {
        disp_boolean("has_zero_init_component", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.has_pointer_component) {
        disp_boolean("has_pointer_component", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.has_operator_ampersand) {
        disp_boolean("has_operator_ampersand", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.copy_assignment_decl_suppressed) {
        disp_boolean("copy_assignment_decl_suppressed", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.copy_ctor_decl_suppressed) {
        disp_boolean("copy_ctor_decl_suppressed", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.default_ctor_decl_suppressed) {
        disp_boolean("default_ctor_decl_suppressed", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.dtor_decl_suppressed) {
        disp_boolean("dtor_decl_suppressed", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.inc_class_used_in_array_type) {
        disp_boolean("inc_class_used_in_array_type", TRUE);
      }  /* if */
#if GNU_EXTENSIONS_ALLOWED
      if (ptr->variant.class_struct_union.is_transparent) {
        disp_boolean("is_transparent", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.is_packed) {
        disp_boolean("is_packed", TRUE);
      }  /* if */
      if (ptr->variant.class_struct_union.has_internal_linkage_attribute) {
        disp_boolean("has_internal_linkage_attribute", TRUE);
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
      if (ptr->variant.class_struct_union.max_member_alignment != 0) {
        disp_unsigned_long("max_member_alignment",
                           (unsigned long)ptr->variant.class_struct_union.
                                                        max_member_alignment);
      }  /* if */
      break;
    case tk_typeref:
      disp_ptr("typeref_type", (char *)ptr->variant.typeref.type,
               iek_type);
      disp_typeref_type_supplement(ptr->variant.typeref.extra_info);
#if DO_IL_LOWERING
      /* Do not print out ptr->variant.typeref.orig_type, which is used only
         during IL lowering. */
#endif /* DO_IL_LOWERING */
      if (ptr->variant.typeref.qualifiers != TQ_NONE) {
        disp_name("qualifiers");
        disp_type_qualifiers(ptr->variant.typeref.qualifiers);
        (void)fprintf(f_display, "\n");
      }  /* if */
      disp_typeref_kind(ptr->variant.typeref.kind);
      if (ptr->variant.typeref.predeclared) disp_boolean("predeclared", TRUE);
#if NEAR_AND_FAR_ALLOWED
      if (ptr->variant.typeref.explicit_memory_attribute_made_implicit) {
        disp_boolean("explicit_memory_attribute_made_implicit", TRUE);
      }  /* if */
#endif /* NEAR_AND_FAR_ALLOWED */
      if (ptr->variant.typeref.has_variably_modified_type) {
        disp_boolean("has_variably_modified_type", TRUE);
      }  /* if */
#if BACK_END_IS_CP_GEN_BE
      if (ptr->variant.typeref.surrounding_name_linkage_state !=
                                              (a_name_linkage_kind)nlk_none) {
        disp_name_linkage("surrounding_name_linkage_state",
                          (a_name_linkage_kind)ptr->variant.typeref.
                                              surrounding_name_linkage_state);
      }  /* if */
      if (ptr->variant.typeref.is_renamed_builtin) {
        disp_boolean("is_renamed_builtin", TRUE);
      }  /* if */
#endif /* BACK_END_IS_CP_GEN_BE */
      if (ptr->variant.typeref.decltype_expr_not_parenthesized) {
        disp_boolean("decltype_expr_not_parenthesized", TRUE);
      }  /* if */
      if (ptr->variant.typeref.is_dependent_type_operator) {
        disp_boolean("is_dependent_type_operator", TRUE);
      }  /* if */
      if (ptr->variant.typeref.is_nonreal) {
        disp_boolean("is_nonreal", TRUE);
      }  /* if */
      if (ptr->variant.typeref.is_prototype_instantiation) {
        disp_boolean("is_prototype_instantiation", TRUE);
      }  /* if */
#if C99_IL_EXTENSIONS_SUPPORTED && LOWER_COMPLEX
      if (ptr->variant.typeref.is_lowered_complex_type) {
        disp_boolean("is_lowered_complex_type", TRUE);
      }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED && LOWER_COMPLEX */
      if (ptr->variant.typeref.embedded_source_sequence_entries) {
        disp_boolean("embedded_source_sequence_entries", TRUE);
      }  /* if */
      if (ptr->variant.typeref.added_to_record_name) {
        disp_boolean("added_to_record_name", TRUE);
      }  /* if */
      if (ptr->variant.typeref.has_typename_prefix) {
        disp_boolean("has_typename_prefix", TRUE);
      }  /* if */
      if (ptr->variant.typeref.is_global_qualified_name) {
        disp_boolean("is_global_qualified_name", TRUE);
      }  /* if */
      if (ptr->variant.typeref.is_intrinsic_member) {
        disp_boolean("is_intrinsic_member", TRUE);
      }  /* if */
      break;
    case tk_ptr_to_member:
      disp_ptr("class_of_which_a_member", (char *)pm_class_type(ptr),
               iek_type);
      if (pm_class_type(ptr) != pm_orig_class_type(ptr)) {
        disp_ptr("orig_class_of_which_a_member",
                 (char *)pm_orig_class_type(ptr), iek_type);
      }  /* if */
      disp_ptr("type", (char *)ptr->variant.ptr_to_member.type, iek_type);
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (ptr->variant.ptr_to_member.modifiers != PM_NONE) {
        disp_name("modifiers");
        form_pointer_modifiers(ptr->variant.ptr_to_member.modifiers, &octl);
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      break;
    case tk_template_param:
      disp_name("kind");
      switch (ptr->variant.template_param.kind) {
        case tptk_param:
          (void)fprintf(f_display, "tptk_param\n");
          break;
        case tptk_member:
          (void)fprintf(f_display, "tptk_member\n");
          break;
        case tptk_unknown:
          (void)fprintf(f_display, "tptk_unknown\n");
          break;
        default:
          (void)fprintf(f_display, "**BAD TEMPLATE PARAM TYPE KIND**\n");
          break;
      }  /* switch */
      if (ptr->variant.template_param.is_pack) {
        disp_boolean("is_pack", TRUE);
      }  /* if */
      if (ptr->variant.template_param.is_generic_param) {
        disp_boolean("is_generic_param", TRUE);
      }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (ptr->variant.template_param.is_generic_function_param) {
        disp_boolean("is_generic_function_param", TRUE);
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      if (ptr->variant.template_param.is_auto_param) {
        disp_boolean("is_auto_param", TRUE);
      }  /* if */
      disp_template_param_type_supplement(
                                       ptr->variant.template_param.extra_info);
      break;
#if GNU_VECTOR_TYPES_ALLOWED
    case tk_vector:
      disp_ptr("element_type", (char *)ptr->variant.vector.element_type,
               iek_type);
      disp_ptr("size_constant", (char *)ptr->variant.vector.size_constant,
               iek_constant);
      if (ptr->variant.vector.is_boolean_vector) {
        disp_boolean("is_boolean_vector", TRUE);
      }  /* if */
      disp_unsigned_long("vector_kind", ptr->variant.vector.kind);
      break;
    case tk_scalable_vector:
      disp_ptr("element_type",
               (char *)ptr->variant.scalable_vector.element_type,
               iek_type);
      disp_uint32("tuple_elements",
                  ptr->variant.scalable_vector.tuple_elements);
      break;
    case tk_riscv_vector:
      disp_ptr("element_type", (char *)ptr->variant.riscv_vector.element_type,
               iek_type);
      disp_int32("length_multiplier",
                 ptr->variant.riscv_vector.length_multiplier);
      disp_uint32("tuple_elements", ptr->variant.riscv_vector.tuple_elements);
      break;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
    default:
      /* Nothing more to be done. */
      break;
  }  /* switch */
}  /* disp_type */


static void disp_stdc_pragma_value(a_const_char		*name,
                                   a_stdc_pragma_value	value)
/*
Display a STDC pragma value along with a name.
*/
{
  a_const_char *s;

  disp_name(name);
  switch (value) {
    case stdc_pv_none:    s = "none";                      break;
    case stdc_pv_off:     s = "off";                       break;
    case stdc_pv_on:      s = "on";                        break;
#if FIXED_POINT_ALLOWED
    case stdc_pv_sat:     s = "sat";                       break;
#endif /* FIXED_POINT_ALLOWED */
    case stdc_pv_default: s = "default";                   break;
    default:              s = "**BAD STDC PRAGMA VALUE**"; break;
  }  /* switch */
  (void)fprintf(f_display, "%s\n", s);
}  /* disp_stdc_pragma_value */


static void disp_storage_class_name(a_storage_class sclass)
/*
Display the name for the indicated storage class.
*/
{
  a_const_char *s;

  switch (sclass) {
    case sc_extern:       s = "sc_extern";             break;
    case sc_static:       s = "sc_static";             break;
    case sc_auto:         s = "sc_auto";               break;
    case sc_unspecified:  s = "sc_unspecified";        break;
    case sc_register:     s = "sc_register";           break;
    case sc_typedef:      s = "sc_typedef";            break;
    /* sc_asm is only used in versions with ASM_FUNCTION_ALLOWED set TRUE. */
    case sc_asm:          s = "sc_asm";                break;
    default:              s = "**BAD STORAGE CLASS**"; break;
  }  /* switch */
  (void)fprintf(f_display, "%s\n", s);
}  /* disp_storage_class_name */

#if DECL_MODIFIERS_IN_USE

static void disp_decl_modifiers(a_decl_modifier_set  dm)
/*
Display the indicated decl modifiers.
*/
{
  if (dm != DM_NONE) {
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (dm & DM_DLLIMPORT) {
      disp_boolean("dllimport", TRUE);
    }  /* if */
    if (dm & DM_DLLEXPORT) {
      disp_boolean("dllexport", TRUE);
    }  /* if */
    if (dm & DM_THREAD) {
      disp_boolean("thread", TRUE);
    }  /* if */
    if (dm & DM_MICROSOFT_INLINE) {
      disp_boolean("microsoft_inline", TRUE);
    }  /* if */
    if (dm & DM_FORCEINLINE) {
      disp_boolean("forceinline", TRUE);
    }  /* if */
    if (dm & DM_SELECTANY) {
      disp_boolean("selectany", TRUE);
    }  /* if */
    if (dm & DM_NOVTABLE) {
      disp_boolean("novtable", TRUE);
    }  /* if */
    if (dm & DM_NOALIAS) {
      disp_boolean("noalias", TRUE);
    }  /* if */
    if (dm & DM_RESTRICT) {
      disp_boolean("restrict", TRUE);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
}  /* disp_decl_modifiers */

#endif /* DECL_MODIFIERS_IN_USE */

static void disp_initializer(an_init_kind         kind,
                             an_initializer_ptr   ptr,
                             ARG_UNUSED a_boolean is_member_constant)
/*
Display the indicated init kind and initializer.  If is_member_constant is
TRUE, a constant can be indicated even if kind is initk_none.  This only occurs
in configurations that perform lowering.
*/
{
  disp_name("init_kind");
  switch (kind) {
    case initk_none:
      (void)fprintf(f_display, "initk_none\n");
#if DO_IL_LOWERING
      if (is_member_constant) {
        disp_ptr("constant", (char *)ptr->constant, iek_constant);
      }  /* if */
#endif /* DO_IL_LOWERING */
      break;
    case initk_static:
      (void)fprintf(f_display, "initk_static\n");
      disp_ptr("constant", (char *)ptr->constant, iek_constant);
      break;
    case initk_dynamic:
      (void)fprintf(f_display, "initk_dynamic\n");
      disp_ptr("dynamic", (char *)ptr->dynamic, iek_dynamic_init);
      break;
    case initk_zero:
      (void)fprintf(f_display, "initk_zero\n");
      break;
    case initk_function_local:
      (void)fprintf(f_display, "initk_function_local\n");
      break;
    case initk_binding:
      (void)fprintf(f_display, "initk_binding\n");
      disp_ptr("binding", (char *)ptr->bound_expr, iek_expr_node);
      break;
    default:
      (void)fprintf(f_display, "**BAD INITIALIZATION KIND**\n");
  }  /* switch */
}  /* disp_initializer */

#if GNU_EXTENSIONS_ALLOWED

static void disp_named_register(a_const_char     *field_name,
                                a_named_register reg)
/*
Display a named register "reg".  The "name" is the name of the IL
field storing the register.
*/
{
  a_const_char *s;

  disp_name(field_name);
  (void)fprintf(f_display, ": ");
  switch (reg) {
    case anr_invalid: s ="anr_invalid";  break;
#if GNU_X86_ASM_EXTENSIONS_ALLOWED
    case anr_a:       s = "anr_a";       break;
    case anr_b:       s = "anr_b";       break;
    case anr_c:       s = "anr_c";       break;
    case anr_d:       s = "anr_d";       break;
    case anr_si:      s = "anr_si";      break;
    case anr_di:      s = "anr_di";      break;
    case anr_bp:      s = "anr_bp";      break;
    case anr_sp:      s = "anr_sp";      break;
    case anr_r8:      s = "anr_r8";      break;
    case anr_r9:      s = "anr_r9";      break;
    case anr_r10:     s = "anr_r10";     break;
    case anr_r11:     s = "anr_r11";     break;
    case anr_r12:     s = "anr_r12";     break;
    case anr_r13:     s = "anr_r13";     break;
    case anr_r14:     s = "anr_r14";     break;
    case anr_r15:     s = "anr_r15";     break;
    case anr_st:      s = "anr_st";      break;
    case anr_st1:     s = "anr_st1";     break;
    case anr_st2:     s = "anr_st2";     break;
    case anr_st3:     s = "anr_st3";     break;
    case anr_st4:     s = "anr_st4";     break;
    case anr_st5:     s = "anr_st5";     break;
    case anr_st6:     s = "anr_st6";     break;
    case anr_st7:     s = "anr_st7";     break;
    case anr_mm0:     s = "anr_mm0";     break;
    case anr_mm1:     s = "anr_mm1";     break;
    case anr_mm2:     s = "anr_mm2";     break;
    case anr_mm3:     s = "anr_mm3";     break;
    case anr_mm4:     s = "anr_mm4";     break;
    case anr_mm5:     s = "anr_mm5";     break;
    case anr_mm6:     s = "anr_mm6";     break;
    case anr_mm7:     s = "anr_mm7";     break;
    case anr_f0:      s = "anr_f0";      break;
    case anr_f1:      s = "anr_f1";      break;
    case anr_f2:      s = "anr_f2";      break;
    case anr_f3:      s = "anr_f3";      break;
    case anr_f4:      s = "anr_f4";      break;
    case anr_f5:      s = "anr_f5";      break;
    case anr_f6:      s = "anr_f6";      break;
    case anr_f7:      s = "anr_f7";      break;
    case anr_f8:      s = "anr_f8";      break;
    case anr_f9:      s = "anr_f9";      break;
    case anr_f10:     s = "anr_f10";     break;
    case anr_f11:     s = "anr_f11";     break;
    case anr_f12:     s = "anr_f12";     break;
    case anr_f13:     s = "anr_f13";     break;
    case anr_f14:     s = "anr_f14";     break;
    case anr_f15:     s = "anr_f15";     break;
    case anr_flags:   s = "anr_flags";   break;
    case anr_fpsr:    s = "anr_fpsr";    break;
    case anr_dirflag: s = "anr_dirflag"; break;
#endif /* GNU_X86_ASM_EXTENSIONS_ALLOWED */
    default: s = "**BAD REGISTER KIND**";
  }  /* switch */
  (void)fprintf(f_display, "%s\n", s);
}  /* disp_named_register */

#endif /* GNU_EXTENSIONS_ALLOWED */
                                
static void disp_variable_template_info(a_variable_template_info_ptr ptr)
/*
Display the indicated variable template information entry.
*/
{
  if (ptr->template_arg_list != NULL) {
    disp_template_arg_list("template_arg_list", ptr->template_arg_list);
  }  /* if */
  if (ptr->partial_spec_template_arg_list != NULL) {
    disp_template_arg_list("partial_spec_template_arg_list",
                           ptr->partial_spec_template_arg_list);
  }  /* if */
  if (ptr->assoc_template != NULL) {
    disp_ptr("assoc_template", (char*)ptr->assoc_template, iek_template);
  }  /* if */
}  /* disp_variable_template_info */


static void disp_variable(a_variable_ptr ptr)
/*
Display the indicated variable.
*/
{
  disp_source_corresp(&ptr->source_corresp, iek_variable);
  disp_ptr("next", (char *)ptr->next, iek_variable);
  disp_ptr("type", (char *)ptr->type, iek_type);
  if (ptr->is_struct_binding) {
    disp_ptr("container", (char *)ptr->variant.container, iek_variable);
  } else if (ptr->is_struct_binding_container) {
    disp_entity_list("bindings", ptr->variant.bindings);
  } else if (ptr->variant.assoc_param_type != NULL) {
    disp_ptr("assoc_param_type", (char *)ptr->variant.assoc_param_type,
             iek_param_type);
  }  /* if */
  disp_name("storage_class");
  disp_storage_class_name(ptr->storage_class);
#if GENERATE_SOURCE_SEQUENCE_LISTS
  disp_name("declared_storage_class");
  disp_storage_class_name(ptr->declared_storage_class);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if DECL_MODIFIERS_IN_USE
  disp_decl_modifiers(ptr->decl_modifiers);
#endif /* DECL_MODIFIERS_IN_USE */
  {
    a_boolean  asm_name_valid = FALSE, named_register_storage_class = FALSE;
#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
    asm_name_valid = ptr->asm_name_is_valid;
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */
#if NAMED_REGISTERS_ALLOWED
    named_register_storage_class = ptr->has_named_register_storage_class;
#endif /* NAMED_REGISTERS_ALLOWED */
    if (asm_name_valid && named_register_storage_class) {
      /* A GNU asm alias and an Embedded C named-register storage class are
         mutually exclusive. */
      (void)fprintf(f_display, "**BAD IL: %s %s**\n",
                   "asm_name_is_valid and has_named_register_storage_class",
                   "both TRUE");
#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
    } else if (asm_name_valid) {
      if (ptr->asm_name_or_reg.name != NULL) {
        disp_string_ptr("asm_name", ptr->asm_name_or_reg.name, iek_other_text,
                        (sizeof_t)0);
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */
#if NAMED_REGISTERS_ALLOWED
    } else if (named_register_storage_class) {
      a_const_char *name =
                  named_register_storage_classes[ptr->asm_name_or_reg.id].name;
      disp_name("register_id");
      (void)fprintf(f_display, "%s\n", name);
#endif /* NAMED_REGISTERS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
    } else {
      disp_named_register("reg", ptr->asm_name_or_reg.reg);
#endif /* GNU_EXTENSIONS_ALLOWED */
    }  /* if */
  }
  if (ptr->alignment != 0) {
    disp_unsigned_long("alignment", (unsigned long)ptr->alignment);
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
  if (ptr->init_priority != 0) {
    disp_unsigned_long("init_priority", (unsigned long)ptr->init_priority);
  }  /* if */
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
  if (ptr->cleanup_routine != NULL) {
    disp_ptr("cleanup_routine", (char *)ptr->cleanup_routine, iek_routine);
  }  /* if */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  if (ptr->ELF_visibility != evk_unspecified) {
    disp_ELF_visibility_kind(enum_cast<an_ELF_visibility_kind>(
                                                         ptr->ELF_visibility));
  }  /* if */
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
  if (ptr->is_weak) { 
    disp_boolean("is_weak", TRUE);
  }  /* if */
  if (ptr->is_weakref) { 
    disp_boolean("is_weakref", TRUE);
  }  /* if */
  if (ptr->is_gnu_alias) { 
    disp_boolean("is_gnu_alias", TRUE);
  }  /* if */
  if (ptr->has_gnu_used_attribute) { 
    disp_boolean("has_gnu_used_attribute", TRUE);
  }  /* if */
  if (ptr->has_gnu_abi_tag_attribute) {
    disp_boolean("has_gnu_abi_tag_attribute", TRUE);
  }  /* if */
  if (ptr->is_not_common) {
    disp_boolean("is_not_common", TRUE);
  }  /* if */
  if (ptr->is_common) {
    disp_boolean("is_common", TRUE);
  }  /* if */
  if (ptr->has_internal_linkage_attribute) {
    disp_boolean("has_internal_linkage_attribute", TRUE);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
  if (ptr->asm_name_is_valid) {
    disp_boolean("asm_name_is_valid", TRUE);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */
  if (ptr->used) {
    disp_boolean("used", (a_boolean)ptr->used);
  }  /* if */
  if (ptr->address_taken) {
    disp_boolean("address_taken", (a_boolean)ptr->address_taken);
  }  /* if */
  if (ptr->is_parameter) {
    disp_boolean("is_parameter", TRUE);
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (ptr->embedded_source_sequence_entries) {
    disp_boolean("embedded_source_sequence_entries", TRUE);
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if (ptr->declared_using_type_without_linkage) {
    disp_boolean("declared_using_type_without_linkage", TRUE);
  }  /* if */
  if (ptr->is_pack) {
    disp_boolean("is_pack", TRUE);
  }  /* if */
  if (ptr->is_pack_element) {
    disp_boolean("is_pack_element", TRUE);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ptr->is_initonly) {
    disp_boolean("is_initonly", TRUE);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (ptr->is_enhanced_for_iterator) {
    disp_boolean("is_enhanced_for_iterator", TRUE);
  }  /* if */
  if (ptr->initializer_in_class) {
    disp_boolean("initializer_in_class", TRUE);
  }  /* if */
  if (ptr->constant_valued) {
    disp_boolean("constant_valued", TRUE);
  }  /* if */
  if (ptr->is_thread_local) {
    disp_boolean("is_thread_local", TRUE);
  }  /* if */
  if (ptr->extends_lifetime) {
    disp_boolean("extends_lifetime", TRUE);
  }  /* if */
  if (ptr->is_template_param_object) {
    disp_boolean("is_template_param_object", TRUE);
  }  /* if */
  if (ptr->compiler_generated) {
    disp_boolean("compiler_generated", TRUE);
  }  /* if */
  if (ptr->is_in_class_specialization) {
    disp_boolean("is_in_class_specialization", TRUE);
  }  /* if */
  disp_initializer(ptr->init_kind, &ptr->initializer,
                   ptr->is_member_constant);
  if (ptr->entities_defined_in_initializer != NULL) {
    disp_entity_list("entities_defined_in_initializer",
                     ptr->entities_defined_in_initializer);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  disp_ptr("property_or_event_descr", (char*)ptr->property_or_event_descr,
           iek_property_or_event_descr);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (ptr->template_info != NULL) {
    disp_ptr("template_info", (char*)ptr->template_info,
             iek_variable_template_info);
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
  if (ptr->section != NULL) {
    disp_string_ptr("section", ptr->section, iek_other_text, (sizeof_t)0);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  if (ptr->aliased_variable != NULL) {
    disp_ptr("aliased_variable", (char*)ptr->aliased_variable, iek_variable);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING
  if (ptr->comdat_group != NULL) {
    disp_string_ptr("comdat_group", ptr->comdat_group, 
                    iek_other_text, (sizeof_t)0);
  }  /* if */
  if (ptr->vla_element_count_variable != NULL) {
    disp_ptr("vla_element_count_variable",
             (char*)ptr->vla_element_count_variable, iek_variable);
  }  /* if */
#endif /* DO_IL_LOWERING */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_source_range("initializer_range", &ptr->initializer_range);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  if (ptr->is_handler_param) {
    disp_boolean("is_handler_param", TRUE);
  }  /* if */
  if (ptr->is_this_parameter) {
    disp_boolean("is_this_parameter", TRUE);
  }  /* if */
#if DO_IL_LOWERING
  if (ptr->initialization_rewritten_as_assignment) {
    disp_boolean("initialization_rewritten_as_assignment", TRUE);
  }  /* if */
#endif /* DO_IL_LOWERING */
  if (ptr->referenced_non_locally) {
    disp_boolean("referenced_non_locally", TRUE);
  }  /* if */
  if (ptr->modified_within_try_block) {
    disp_boolean("modified_within_try_block", TRUE);
  }  /* if */
  if (ptr->is_template_variable) {
    disp_boolean("is_template_variable", TRUE);
  }  /* if */
  if (ptr->is_prototype_instantiation) {
    disp_boolean("is_prototype_instantiation", TRUE);
  }  /* if */
  if (ptr->is_nonreal) {
    disp_boolean("is_nonreal", TRUE);
  }  /* if */
  if (ptr->is_specialized) {
    disp_boolean("is_specialized", TRUE);
  }  /* if */
  if (ptr->specialized_with_old_syntax) {
    disp_boolean("specialized_with_old_syntax", TRUE);
  }  /* if */
  if (ptr->explicit_instantiation) {
    disp_boolean("explicit_instantiation", TRUE);
  }  /* if */
  if (ptr->class_explicitly_instantiated) {
    disp_boolean("class_explicitly_instantiated", TRUE);
  }  /* if */
  if (ptr->explicit_do_not_instantiate) {
    disp_boolean("explicit_do_not_instantiate", TRUE);
  }  /* if */
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  if (ptr->can_be_instantiated) {
    disp_boolean("can_be_instantiated", TRUE);
  }  /* if */
  if (ptr->do_not_instantiate) {
    disp_boolean("do_not_instantiate", TRUE);
  }  /* if */
  if (ptr->instance_required) {
    disp_boolean("instance_required", TRUE);
  }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
  if (ptr->is_parameter || ptr->is_handler_param) {
    disp_boolean("param_value_has_been_changed",
                 (a_boolean)ptr->param_value_has_been_changed);
#if MINIMAL_INLINING
    disp_boolean("param_used_as_lvalue",
                 (a_boolean)ptr->param_used_as_lvalue);
#endif /* MINIMAL_INLINING */
    disp_boolean("param_used_more_than_once",
                 (a_boolean)ptr->param_used_more_than_once);
  }  /* if */
  if (ptr->is_anonymous_parent_object) {
    disp_boolean("is_anonymous_parent_object", TRUE);
  }  /* if */
  if (ptr->is_member_constant) {
    disp_boolean("is_member_constant", TRUE);
  }  /* if */
  if (ptr->is_constexpr) {
    disp_boolean("is_constexpr", TRUE);
  }  /* if */
  if (ptr->declared_constinit) {
    disp_boolean("declared_constinit", TRUE);
  }  /* if */
  if (ptr->is_inline) {
    disp_boolean("is_inline", TRUE);
  }  /* if */
  if (ptr->suppress_inline_definition) {
    disp_boolean("suppress_inline_definition", TRUE);
  }  /* if */
  if (ptr->superseded_external) {
    disp_boolean("superseded_external", TRUE);
  }  /* if */
  if (ptr->has_variably_modified_type) {
    disp_boolean("has_variably_modified_type", TRUE);
    disp_boolean("is_vla", ptr->is_vla);
  }  /* if */
#if DO_IL_LOWERING
  if (ptr->lowering_generated) {
    disp_boolean("lowering_generated", TRUE);
  }  /* if */
#endif /* DO_IL_LOWERING */
  if (ptr->is_compound_literal) {
    disp_boolean("is_compound_literal", TRUE);
  }  /* if */
  if (ptr->has_explicit_initializer) {
    disp_boolean("has_explicit_initializer", TRUE);
  }  /* if */
  if (ptr->has_parenthesized_initializer) {
    disp_boolean("has_parenthesized_initializer", TRUE);
  }  /* if */
  if (ptr->has_direct_braced_initializer) {
    disp_boolean("has_direct_braced_initializer", TRUE);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED
  if (ptr->has_flexible_array_initializer) {
    disp_boolean("has_flexible_array_initializer", TRUE);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED */
  if (ptr->declared_with_auto_type_specifier) {
    disp_boolean("declared_with_auto_type_specifier", TRUE);
  }  /* if */
  if (ptr->declared_with_decltype_auto) {
    disp_boolean("declared_with_decltype_auto", TRUE);
  }  /* if */
  if (ptr->declared_with_class_template_placeholder) {
    disp_boolean("declared_with_class_template_placeholder", TRUE);
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  disp_ptr("declared_type", (char *)ptr->declared_type, iek_type);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if ONE_INSTANTIATION_PER_OBJECT
  if (ptr->instantiation_needed_bit_number != 0) {
    disp_unsigned_long("instantiation_needed_bit_number",
                       (unsigned long)ptr->instantiation_needed_bit_number);
  }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  /* remapping_for_inlining is a front-end-only field. */
#if SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
  if (!ptr->is_thread_local &&
      ptr->init_routine.dynamic_init_routine != NULL) {
    disp_ptr("dynamic_init_routine",
             (char *)ptr->init_routine.dynamic_init_routine,
             iek_routine);
  }  /* if */
#endif /* SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */
#if USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES
  if (ptr->is_thread_local &&
      ptr->init_routine.thread.init_routine != NULL) {
    disp_ptr("init_routine",
             (char *)ptr->init_routine.thread.init_routine,
             iek_routine);
  }  /* if */
  if (ptr->is_thread_local &&
      ptr->init_routine.thread.wrapper != NULL) {
    disp_ptr("wrapper",
             (char *)ptr->init_routine.thread.wrapper,
             iek_routine);
  }  /* if */
#endif /* USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES */
}  /* disp_variable */


static void disp_field(a_field_ptr ptr)
/*
Display the indicated field.
*/
{
  disp_source_corresp(&ptr->source_corresp, iek_field);
  disp_ptr("next", (char *)ptr->next, iek_field);
  disp_ptr("type", (char *)ptr->type, iek_type);
  disp_host_large_unsigned("offset", (a_host_large_unsigned)ptr->offset);
  if (ptr->is_bit_field) {
    disp_boolean("is_bit_field", TRUE);
    disp_unsigned_long("offset_bit_remainder",
                       (unsigned long)ptr->offset_bit_remainder);
    disp_unsigned_long("bit_size", (unsigned long)ptr->bit_size);
#if RECORD_BIT_FIELD_CONTAINER_OFFSETS_IN_IL
    if (ptr->offset_in_container != 0) {
      disp_unsigned_long("offset_in_container",
                         (unsigned long)ptr->offset_in_container);
    }  /* if */
#endif /* RECORD_BIT_FIELD_CONTAINER_OFFSETS_IN_IL */
    disp_ptr("bit_size_constant", (char *)ptr->bit_size_constant,
             iek_constant);
    if (ptr->declared_bit_size != ptr->bit_size) {
      disp_unsigned_long("declared_bit_size", ptr->declared_bit_size);
#if BACK_END_IS_C_GEN_BE
      if (ptr->bit_field_alignment_type != NULL) {
        disp_ptr("bit_field_alignment_type",
                 (char *)ptr->bit_field_alignment_type, iek_type);
      }  /* if */
#endif /* BACK_END_IS_C_GEN_BE */
    }  /* if */
    disp_boolean("bit_field_is_signed", (a_boolean)ptr->bit_field_is_signed);
  }  /* if */
  if (ptr->alignment) {
    disp_unsigned_long("alignment", (unsigned long)ptr->alignment);
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  if (ptr->is_packed) {
    disp_boolean("is_packed", TRUE);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (ptr->has_initializer) {
    disp_boolean("has_initializer", TRUE);
    if (ptr->has_direct_braced_initializer) {
      disp_boolean("has_direct_braced_initializer", TRUE);
    }  /* if */
    if (ptr->has_nonconstant_initializer) {
      disp_boolean("has_nonconstant_initializer", TRUE);
    }  /* if */
    if (ptr->bit_size_constant_expr_in_local_expr_node_ref) {
      disp_boolean("bit_size_constant_expr_in_local_expr_node_ref", TRUE);
    }  /* if */
    disp_ptr("initializer", (char *)ptr->initializer, iek_dynamic_init);
    if (ptr->entities_defined_in_initializer != NULL) {
      disp_entity_list("entities_defined_in_initializer",
                       ptr->entities_defined_in_initializer);
    }  /* if */
  }  /* if */
  if (ptr->has_no_unique_address_attribute) {
    disp_boolean("has_no_unique_address_attribute", TRUE);
  }  /* if */
  if (ptr->is_optimized_empty_class) {
    disp_boolean("is_optimized_empty_class", TRUE);
  }  /* if */
  if (ptr->is_anonymous_parent_object) {
    disp_boolean("is_anonymous_parent_object", TRUE);
  }  /* if */
  if (ptr->is_mutable) disp_boolean("is_mutable", TRUE);
  if (ptr->compiler_generated) disp_boolean("compiler_generated", TRUE);
  if (ptr->is_init_capture) disp_boolean("is_init_capture", TRUE);
  if (ptr->is_captured_this) disp_boolean("is_captured_this", TRUE);
  if (ptr->is_captured_pack_element) {
    disp_boolean("is_captured_pack_element", TRUE);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ptr->is_initonly) disp_boolean("is_initonly", TRUE);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  if (ptr->vla_treated_as_zero_length_array) {
    disp_boolean("vla_treated_as_zero_length_array", TRUE);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING
  if (ptr->is_lowered_base_class) {
    disp_boolean("is_lowered_base_class", TRUE);
  }  /* if */
  if (ptr->class_subobject_with_tail_padding) {
    disp_boolean("class_subobject_with_tail_padding", TRUE);
  }  /* if */
#endif /* DO_IL_LOWERING */
#if MICROSOFT_EXTENSIONS_ALLOWED
  disp_ptr("property_or_event_descr", (char*)ptr->property_or_event_descr,
           iek_property_or_event_descr);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (ptr->has_initializer) {
    disp_source_range("initializer_range", &ptr->initializer_range);
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* disp_field */


static void disp_special_function_kind_name(a_special_function_kind kind)
/*
Print the name of a special function kind.
*/
{
  a_const_char * s;

  switch (kind) {
    case sfk_none:               s = "sfk_none";               break;
    case sfk_constructor:        s = "sfk_constructor";        break;
    case sfk_destructor:         s = "sfk_destructor";         break;
    case sfk_conversion:         s = "sfk_conversion";         break;
    case sfk_udl_operator:       s = "sfk_udl_operator";       break;
    case sfk_operator:           s = "sfk_operator";           break;
    case sfk_lambda_entry_point: s = "sfk_lambda_entry_point"; break;
    case sfk_deduction_guide:    s = "sfk_deduction_guide";    break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case sfk_static_constructor: s = "sfk_static_constructor"; break;
    case sfk_property_get:       s = "sfk_property_get";       break;
    case sfk_property_set:       s = "sfk_property_set";       break;
    case sfk_event_add:          s = "sfk_event_add";          break;
    case sfk_event_remove:       s = "sfk_event_remove";       break;
    case sfk_event_raise:        s = "sfk_event_raise";        break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if BUILTIN_FUNCTIONS_ENABLED
    case sfk_gnu_sync_concrete_function:
                                s = "sfk_gnu_sync_concrete_function";
                                                               break;
    case sfk_gnu_atomic_nongeneric_function:
                                s = "sfk_gnu_atomic_nongeneric_function";
                                                               break;
    case sfk_gnu_atomic_generic_function:
                                s = "sfk_gnu_atomic_generic_function";
                                                               break;
    case sfk_builtin_operator_new:
                                s = "sfk_builtin_operator_new";
                                                               break;
    case sfk_builtin_operator_delete:
                                s = "sfk_builtin_operator_delete";
                                                               break;
#endif /* BUILTIN_FUNCTIONS_ENABLED */
    default:                     s = "**BAD SPECIAL FUNCTION KIND**";
  }  /* switch */
  (void)fprintf(f_display, "%s", s);
}  /* disp_special_function_kind_name */


static void disp_opname_kind_name(an_opname_kind kind)
/*
Print the name of the C++ operator kind.
*/
{
  a_const_char *s;

  switch (kind) {
    case onk_none:                s = "onk_none";                  break;
    case onk_new:                 s = "onk_new";                   break;
    case onk_delete:              s = "onk_delete";                break;
    case onk_array_new:           s = "onk_array_new";             break;
    case onk_array_delete:        s = "onk_array_delete";          break;
    case onk_plus:                s = "onk_plus";                  break;
    case onk_minus:               s = "onk_minus";                 break;
    case onk_star:                s = "onk_star";                  break;
    case onk_divide:              s = "onk_divide";                break;
    case onk_remainder:           s = "onk_remainder";             break;
    case onk_excl_or:             s = "onk_excl_or";               break;
    case onk_ampersand:           s = "onk_ampersand";             break;
    case onk_or:                  s = "onk_or";                    break;
    case onk_compl:               s = "onk_compl";                 break;
    case onk_not:                 s = "onk_not";                   break;
    case onk_assign:              s = "onk_assign";                break;
    case onk_lt:                  s = "onk_lt";                    break;
    case onk_gt:                  s = "onk_gt";                    break;
    case onk_plus_assign:         s = "onk_plus_assign";           break;
    case onk_minus_assign:        s = "onk_minus_assign";          break;
    case onk_times_assign:        s = "onk_times_assign";          break;
    case onk_divide_assign:       s = "onk_divide_assign";         break;
    case onk_remainder_assign:    s = "onk_remainder_assign";      break;
    case onk_excl_or_assign:      s = "onk_excl_or_assign";        break;
    case onk_and_assign:          s = "onk_and_assign";            break;
    case onk_or_assign:           s = "onk_or_assign";             break;
    case onk_shift_left:          s = "onk_shift_left";            break;
    case onk_shift_right:         s = "onk_shift_right";           break;
    case onk_shift_right_assign:  s = "onk_shift_right_assign";    break;
    case onk_shift_left_assign:   s = "onk_shift_left_assign";     break;
    case onk_eq:                  s = "onk_eq";                    break;
    case onk_ne:                  s = "onk_ne";                    break;
    case onk_le:                  s = "onk_le";                    break;
    case onk_ge:                  s = "onk_ge";                    break;
    case onk_spaceship:           s = "onk_spaceship";             break;
    case onk_and_and:             s = "onk_and_and";               break;
    case onk_or_or:               s = "onk_or_or";                 break;
    case onk_plus_plus:           s = "onk_plus_plus";             break;
    case onk_minus_minus:         s = "onk_minus_minus";           break;
    case onk_comma:               s = "onk_comma";                 break;
    case onk_arrow_star:          s = "onk_arrow_star";            break;
    case onk_arrow:               s = "onk_arrow";                 break;
    case onk_function_call:       s = "onk_function_call";         break;
    case onk_subscript:           s = "onk_subscript";             break;
    case onk_gnu_min:             s = "onk_gnu_min";               break;
    case onk_gnu_max:             s = "onk_gnu_max";               break;
    default:                      s = "**BAD OPERATOR NAME KIND**";
  }  /* switch */
  (void)fprintf(f_display, "%s", s);
}  /* disp_opname_kind_name */


static void disp_class_list(a_const_char           *name,
                            a_class_list_entry_ptr ptr)
/*
Display the indicated class list and name.
*/
{
  a_const_char *type_string;

  if (ptr == NULL) {
    disp_ptr(name, (char *)ptr, iek_class_list_entry);
  } else {
    disp_name(name);
    (void)fprintf(f_display, "\n");
    for (; ptr != NULL; ptr = ptr->next) {
      switch (ptr->class_type->kind) {
        case tk_class:     type_string = "  tk_class";     break;
        case tk_struct:    type_string = "  tk_struct";    break;
        case tk_union:     type_string = "  tk_union";     break;
        default:           type_string = "  **BAD TYPE KIND**";
      }  /* switch */
      disp_ptr(type_string, (char *)ptr->class_type, iek_type);
    }  /* for */
  }  /* if */
}  /* disp_class_list */

#if GNU_FUNCTION_MULTIVERSIONING || \
    SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS

static void disp_routine_list(a_const_char             *name,
                              a_routine_list_entry_ptr ptr)
/*
Display the indicated routine list and name.
*/
{
  if (ptr == NULL) {
    disp_ptr(name, (char *)ptr, iek_routine_list_entry);
  } else {
    disp_name(name);
    (void)fprintf(f_display, "\n");
    for (; ptr != NULL; ptr = ptr->next) {
      disp_ptr("  routine", (char *)ptr->routine, iek_routine);
    }  /* for */
  }  /* if */
}  /* disp_routine_list */

#endif /* GNU_FUNCTION_MULTIVERSIONING || ... */

static void disp_template_arg_list(a_const_char        *name,
                                   a_template_arg_ptr  ptr)
/*
Display the indicated name and template arg list.
*/
{
  if (ptr == NULL) {
    disp_ptr(name, (char *)ptr, iek_template_arg);
  } else {
    disp_name(name);
    (void)fprintf(f_display, "\n");
    for (; ptr != NULL; ptr = ptr->next) {
      if (is_type_templ_arg(ptr)) {
        disp_ptr("  type", (char *)ptr->variant.type, iek_type);
      } else if (is_nontype_templ_arg(ptr)) {
        if (ptr->is_array_bound_of_unknown_type) {
          fprintf(f_display, "**BAD is_array_bound_of_unknown_type**");
        } else {
          disp_ptr("  constant", (char *)ptr->variant.constant, iek_constant);
        }  /* if */
      } else if (is_template_templ_arg(ptr)) {
        /* A template template argument. */
        disp_ptr("  template", (char *)ptr->variant.templ.ptr, iek_template);
      } else if (is_start_of_pack_expansion_templ_arg(ptr)) {
        disp_name("  pack exp placeholder");
        (void)fprintf(f_display, "\n");
      } else {
        disp_name("  **BAD TMP ARG KIND**");
        (void)fprintf(f_display, "\n");
      }  /* if */
      if (ptr->is_array_bound_of_unknown_type) {
        disp_boolean("  is_array_bound_of_unknown_type", TRUE);
      }  /* if */
      if (ptr->explicitly_specified) {
        disp_boolean("  explicitly_specified", TRUE);
      }  /* if */
      if (ptr->is_pack_element) {
        disp_boolean("  is_pack_element", TRUE);
      }  /* if */
      if (ptr->is_pack) {
        disp_boolean("  is_pack", TRUE);
      }  /* if */
      if (ptr->has_pack_ellipsis) {
        disp_boolean("  has_pack_ellipsis", TRUE);
      }  /* if */
      if (ptr->is_integer_pack) {
        disp_boolean("  is_integer_pack", TRUE);
      }  /* if */
      if (ptr->type_is_injected_class_name) {
        disp_boolean("  type_is_injected_class_name", TRUE);
      }  /* if */
      if (ptr->is_provisional_value) {
        disp_boolean("  is_provisional_value", TRUE);
      }  /* if */
      if (ptr->is_error) {
        disp_boolean("  is_error", TRUE);
      }  /* if */
      if (ptr->param_is_auto) {
        disp_boolean("  param_is_auto", TRUE);
      }  /* if */
      if (ptr->param_is_decltype_auto) {
        disp_boolean("  param_is_decltype_auto", TRUE);
      }  /* if */
    }  /* for */
  }  /* if */
}  /* disp_template_arg_list */

#if DO_IL_LOWERING && IA64_ABI

static void disp_ctor_or_dtor_kind_name(a_ctor_or_dtor_kind kind)
/*
Display the name of the indicated constructor or destructor kind.
*/
{
  a_const_char *s;
  
  switch (kind) {
    case cdk_none:      s = "none";                      break;
    case cdk_complete:  s = "complete";                  break;
    case cdk_subobject: s = "subobject";                 break;
    case cdk_deleting:  s = "deleting";                  break;
    case cdk_delegation:s = "delegation";                break;
    default:            s = "**BAD CTOR OR DTOR KIND**"; break;
  }  /* switch */
  (void)fprintf(f_display, "%s", s);
}  /* disp_ctor_or_dtor_kind_name */

#endif /* DO_IL_LOWERING && IA64_ABI */


static void disp_routine(a_routine_ptr ptr)
/*
Display the indicated routine.
*/
{
  disp_source_corresp(&ptr->source_corresp, iek_routine);
  disp_ptr("next", (char *)ptr->next, iek_routine);
  disp_ptr("type", (char *)ptr->type, iek_type);
  disp_unsigned_long("function_def_number",
                     (unsigned long)(long)ptr->function_def_number);
  disp_unsigned_long("memory_region", (unsigned long)(long)ptr->memory_region);
  disp_name("storage_class");
  disp_storage_class_name(ptr->storage_class);
#if GENERATE_SOURCE_SEQUENCE_LISTS
  disp_name("declared_storage_class");
  disp_storage_class_name(ptr->declared_storage_class);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if (!special_kind_is(ptr, sfk_none)) {
    disp_name("special_kind");
    disp_special_function_kind_name(ptr->special_kind);
    (void)fprintf(f_display, "\n");
  }  /* if */
  if (special_kind_is(ptr, sfk_operator)) {
    disp_name("opname_kind");
    disp_opname_kind_name(ptr->variant.opname_kind);
    (void)fprintf(f_display, "\n");
  } else if (special_kind_is(ptr, sfk_lambda_entry_point)) {
    disp_ptr("lambda_call_operator",
             (char *)ptr->variant.lambda_call_operator, iek_routine);
#if IA64_ABI && DO_IL_LOWERING
  } else if (special_kind_is(ptr, sfk_constructor) ||
             special_kind_is(ptr, sfk_destructor)) {
    /* Do not print out alternate_entry_points, which is used only
       during IL lowering. */
    disp_unsigned_long("base_name_offset",
                       (unsigned long)ptr->variant.ctor_dtor.base_name_offset);
#endif /* IA64_ABI && DO_IL_LOWERING */
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (rout_is_cli_accessor(ptr)) {
    disp_ptr("property_or_event_descr",
             (char*)ptr->variant.property_or_event_descr,
             iek_property_or_event_descr);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if BUILTIN_FUNCTIONS_ENABLED
  } else if (special_kind_is(ptr, sfk_none) &&
             ptr->variant.builtin_function_kind != 
                                           (a_builtin_function_kind)bfk_none) {
    disp_name("builtin_function_kind");
    (void)fprintf(f_display, "%s\n",
                  unmangled_or_fabricated_name_of(&ptr->source_corresp));
#endif /* BUILTIN_FUNCTIONS_ENABLED */
  }  /* if */
  if (ptr->address_taken) {
    disp_boolean("address_taken", TRUE);
  }  /* if */
  if (ptr->is_virtual) {
    disp_boolean("is_virtual", TRUE);
  }  /* if */
  if (ptr->overrides_base_member) {
    disp_boolean("overrides_base_member", TRUE);
  }  /* if */
  if (ptr->pure_virtual) {
    disp_boolean("pure_virtual", TRUE);
  }  /* if */
  if (ptr->final) {
    disp_boolean("final", TRUE);
  }  /* if */
  if (ptr->override) {
    disp_boolean("override", TRUE);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ptr->abstract) {
    disp_boolean("abstract", TRUE);
  }  /* if */
  if (ptr->sealed) {
    disp_boolean("sealed", TRUE);
  }  /* if */
  if (ptr->new_member) {
    disp_boolean("new_member", TRUE);
  }  /* if */
  if (ptr->interface_slot) {
    disp_boolean("interface_slot", TRUE);
  }  /* if */
  if (ptr->definition_cannot_be_generated) {
    disp_boolean("definition_cannot_be_generated", TRUE);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (ptr->covariant_return_virtual_override) {
    disp_boolean("covariant_return_virtual_override", TRUE);
  }  /* if */
  if (ptr->is_inline) {
    disp_boolean("is_inline", TRUE);
  }  /* if */
  if (ptr->is_declared_constexpr) {
    disp_boolean("is_declared_constexpr", TRUE);
  }  /* if */
  if (ptr->is_constexpr) {
    disp_boolean("is_constexpr", TRUE);
  }  /* if */
  if (ptr->is_constexpr_intrinsic) {
    disp_boolean("is_constexpr_intrinsic", TRUE);
  }  /* if */
  if (ptr->compiler_generated) {
    disp_boolean("compiler_generated", TRUE);
  }  /* if */
  disp_boolean("defined", ptr->defined);
  disp_boolean("called", ptr->called);
  if (ptr->is_explicit_constructor) {
    disp_boolean("is_explicit_constructor", TRUE);
  }  /* if */
  if (ptr->is_explicit_conversion_function) {
    disp_boolean("is_explicit_conversion_function", TRUE);
  }  /* if */
  if (ptr->is_trivial_default_constructor) {
    disp_boolean("is_trivial_default_constructor", TRUE);
  }  /* if */
  if (ptr->is_trivial_copy_function) {
    disp_boolean("is_trivial_copy_function", TRUE);
  }  /* if */
  if (ptr->is_trivial_destructor) {
    disp_boolean("is_trivial_destructor", TRUE);
  }  /* if */
  if (ptr->is_initializer_list_ctor) {
    disp_boolean("is_initializer_list_ctor", TRUE);
  }  /* if */
  if (ptr->is_delegating_ctor) {
    disp_boolean("is_delegating_ctor", TRUE);
  }  /* if */
  if (ptr->is_inheriting_ctor) {
    disp_boolean("is_inheriting_ctor", TRUE);
  }  /* if */
  if (ptr->is_deduction_guide_from_inheriting_ctor) {
    disp_boolean("is_deduction_guide_from_inheriting_ctor", TRUE);
  }  /* if */
#if ASSIGNMENT_TO_THIS_ALLOWED
  if (ptr->assignment_to_this_done) {
    disp_boolean("assignment_to_this_done", TRUE);
  }  /* if */
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
  if (ptr->is_prototype_instantiation) {
    disp_boolean("is_prototype_instantiation", TRUE);
  }  /* if */
  if (ptr->is_template_function) {
    disp_boolean("is_template_function", TRUE);
  }  /* if */
  if (ptr->is_specialized) {
    disp_boolean("is_specialized", TRUE);
  }  /* if */
  if (ptr->specialized_with_old_syntax) {
    disp_boolean("specialized_with_old_syntax", TRUE);
  }  /* if */
  if (ptr->explicit_instantiation) {
    disp_boolean("explicit_instantiation", TRUE);
  }  /* if */
  if (ptr->class_explicitly_instantiated) {
    disp_boolean("class_explicitly_instantiated", TRUE);
  }  /* if */
  if (ptr->explicit_do_not_instantiate) {
    disp_boolean("explicit_do_not_instantiate", TRUE);
  }  /* if */
  if (ptr->has_nodiscard_attribute) {
    disp_boolean("has_nodiscard_attribute", TRUE);
  }  /* if */
  if (ptr->never_throws) {
    disp_boolean("never_throws", TRUE);
  }  /* if */
  if (ptr->is_in_class_specialization) {
    disp_boolean("is_in_class_specialization", TRUE);
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
  if (ptr->never_inline) {
    disp_boolean("never_inline", TRUE);
  }  /* if */
  if (ptr->is_pure) {
    disp_boolean("is_pure", TRUE);
  }  /* if */
#if GNU_NAKED_ATTRIBUTE_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
  if (ptr->is_naked) {
    disp_boolean("is_naked", TRUE);
  }  /* if */
#endif /* GNU_NAKED_ATTRIBUTE_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ptr->declared_only_as_friend) {
    disp_boolean("declared_only_as_friend", TRUE);
  }  /* if */
  if (ptr->explicit_extern_inline) {
    disp_boolean("explicit_extern_inline", TRUE);
  }  /* if */
  if (ptr->direct_linkage_specifier_on_nondef_decl) {
    disp_boolean("direct_linkage_specifier_on_nondef_decl", TRUE);
  }  /* if */
  if (ptr->is_reverse_conversion_function) {
    disp_boolean("is_reverse_conversion_function", TRUE);
  }  /* if */
  if (ptr->is_generic_definition) {
    disp_boolean("is_generic_definition", TRUE);
  }  /* if */
  if (ptr->is_generic_instance) {
    disp_boolean("is_generic_instance", TRUE);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  if (ptr->ELF_visibility != evk_unspecified) {
    disp_ELF_visibility_kind(enum_cast<an_ELF_visibility_kind>(
                                                         ptr->ELF_visibility));
  }  /* if */
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
  if (ptr->is_initialization_routine) {
    disp_boolean("is_initialization_routine", TRUE);
  }  /* if */
  if (ptr->is_finalization_routine) {
    disp_boolean("is_finalization_routine", TRUE);
  }  /* if */
  if (ptr->is_weak) {
    disp_boolean("is_weak", TRUE);
  }  /* if */
  if (ptr->is_weakref) {
    disp_boolean("is_weakref", TRUE);
  }  /* if */
  if (ptr->is_gnu_alias) { 
    disp_boolean("is_gnu_alias", TRUE);
  }  /* if */
  if (ptr->is_ifunc) {
    disp_boolean("is_ifunc", TRUE);
  }  /* if */
  if (ptr->has_gnu_used_attribute) { 
    disp_boolean("has_gnu_used_attribute", TRUE);
  }  /* if */
  if (ptr->has_gnu_abi_tag_attribute) {
    disp_boolean("has_gnu_abi_tag_attribute", TRUE);
  }  /* if */
  if (ptr->in_gnu_abi_tag_namespace) {
    disp_boolean("in_gnu_abi_tag_namespace", TRUE);
  }  /* if */
  if (ptr->allocates_memory) {
    disp_boolean("allocates_memory", TRUE);
  }  /* if */
  if (ptr->no_instrument_function) {
    disp_boolean("no_instrument_function", TRUE);
  }  /* if */
  if (ptr->no_check_memory_usage) {
    disp_boolean("no_check_memory_usage", TRUE);
  }  /* if */
  if (ptr->always_inline) {
    disp_boolean("always_inline", TRUE);
  }  /* if */
  if (ptr->gnu_c89_inline) {
    disp_boolean("gnu_c89_inline", TRUE);
  }  /* if */
  if (ptr->implicit_alias) {
    disp_boolean("implicit_alias", TRUE);
  }  /* if */
  if (ptr->has_internal_linkage_attribute) {
    disp_boolean("has_internal_linkage_attribute", TRUE);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  if (ptr->can_be_instantiated) {
    disp_boolean("can_be_instantiated", TRUE);
  }  /* if */
  if (ptr->do_not_instantiate) {
    disp_boolean("do_not_instantiate", TRUE);
  }  /* if */
  if (ptr->instance_required) {
    disp_boolean("instance_required", TRUE);
  }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
  if (ptr->contains_try_block) {
    disp_boolean("contains_try_block", TRUE);
  }  /* if */
  if (ptr->contains_local_class_type) {
    disp_boolean("contains_local_class_type", TRUE);
  }  /* if */
  if (ptr->superseded_external) {
    disp_boolean("superseded_external", TRUE);
  }  /* if */
  if (ptr->defined_in_friend_decl) {
    disp_boolean("defined_in_friend_decl", TRUE);
  }  /* if */
  if (ptr->expl_template_arg_list_used) {
    disp_boolean("expl_template_arg_list_used", TRUE);
  }  /* if */
#if BACK_END_IS_CP_GEN_BE
  if (ptr->surrounding_name_linkage_state != (a_name_linkage_kind)nlk_none) {
    disp_name_linkage("surrounding_name_linkage_state",
                      (a_name_linkage_kind)ptr->
                                              surrounding_name_linkage_state);
  }  /* if */
  if (ptr->definition_C_name_linkage_specified) {
    disp_boolean("definition_C_name_linkage_specified", TRUE);
  }  /* if */
  if (ptr->definition_has_direct_linkage_specifier) {
    disp_boolean("definition_has_direct_linkage_specifier", TRUE);
  }  /* if */
#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  if (ptr->evaluated_in_interpreter) {
    disp_boolean("evaluated_in_interpreter", TRUE);
  }  /* if */
  if (ptr->suppress_explicit_specialization) {
    disp_boolean("suppress_explicit_specialization", TRUE);
  }  /* if */
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* BACK_END_IS_CP_GEN_BE */
  if (ptr->definition_for_inlining_only) {
    disp_boolean("definition_for_inlining_only", TRUE);
  }  /* if */
  if (ptr->suppress_inline_body) {
    disp_boolean("suppress_inline_body", TRUE);
  }  /* if */
  if (ptr->need_out_of_line_copy) {
    disp_boolean("need_out_of_line_copy", TRUE);
  }  /* if */
  if (il_header.source_language == (a_source_language)sl_C &&
      il_header.std_version >= 199901) {
    if (ptr->fp_contract != stdc_pv_default) {
      disp_stdc_pragma_value("fp_contract", enum_cast<a_stdc_pragma_value>(
                                                            ptr->fp_contract));
    }  /* if */
    if (ptr->fenv_access != stdc_pv_default) {
      disp_stdc_pragma_value("fenv_access", enum_cast<a_stdc_pragma_value>(
                                                            ptr->fenv_access));
    }  /* if */
    if (ptr->cx_limited_range != stdc_pv_default) {
      disp_stdc_pragma_value("cx_limited_range",
                             enum_cast<a_stdc_pragma_value>(
                                                       ptr->cx_limited_range));
    }  /* if */
  }  /* if */
#if FIXED_POINT_ALLOWED
  if (ptr->fx_full_precision != stdc_pv_default) {
    disp_stdc_pragma_value("fx_full_precision",
                           enum_cast<a_stdc_pragma_value>(
                                                      ptr->fx_full_precision));
  }  /* if */
  if (ptr->fx_fract_overflow != stdc_pv_default) {
    disp_stdc_pragma_value("fx_fract_overflow",
                           enum_cast<a_stdc_pragma_value>(
                                                      ptr->fx_fract_overflow));
  }  /* if */
  if (ptr->fx_accum_overflow != stdc_pv_default) {
    disp_stdc_pragma_value("fx_accum_overflow",
                           enum_cast<a_stdc_pragma_value>(
                                                      ptr->fx_accum_overflow));
  }  /* if */
#endif /* FIXED_POINT_ALLOWED */
  if (ptr->contains_statement_expression) {
    disp_boolean("contains_statement_expression", TRUE);
  }  /* if */
#if IA64_ABI
  if (ptr->inline_in_class_definition) {
    disp_boolean("inline_in_class_definition", TRUE);
  }  /* if */
#endif /* IA64_ABI */
#if DO_IL_LOWERING && IA64_ABI
  if (ptr->use_comdat) {
    disp_boolean("use_comdat", TRUE);
  }  /* if */
  if (ptr->ctor_dtor_kind != cdk_none) {
    disp_name("ctor_dtor_kind");
    disp_ctor_or_dtor_kind_name(
                          enum_cast<a_ctor_or_dtor_kind>(ptr->ctor_dtor_kind));
    (void)fprintf(f_display, "\n");
  }  /* if */
  if (ptr->is_alias_entry) {
    disp_boolean("is_alias_entry", TRUE);
  }  /* if */
#endif /* DO_IL_LOWERING && IA64_ABI */
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
  if (ptr->statics_have_been_promoted) {
    disp_boolean("statics_have_been_promoted", TRUE);
  }  /* if */
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
  if (ptr->is_lambda_body) {
    disp_boolean("is_lambda_body", TRUE);
  }  /* if */
  if (ptr->declared_using_type_without_linkage) {
    disp_boolean("declared_using_type_without_linkage", TRUE);
  }  /* if */
  if (ptr->is_defaulted) {
    disp_boolean("is_defaulted", TRUE);
  }  /* if */
  if (ptr->is_deleted) {
    disp_boolean("is_deleted", TRUE);
  }  /* if */
  if (ptr->contains_local_static_variable) {
    disp_boolean("contains_local_static_variable", TRUE);
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (ptr->embedded_source_sequence_entries) {
    disp_boolean("embedded_source_sequence_entries", TRUE);
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if (ptr->is_raw_literal_operator) {
    disp_boolean("is_raw_literal_operator", TRUE);
  }  /* if */
#if USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES
  if (ptr->is_tls_init_alias) {
    disp_boolean("is_tls_init_alias", TRUE);
  }  /* if */
#endif /* USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES */
  if (ptr->is_tls_init_routine) {
    disp_boolean("is_tls_init_routine", TRUE);
  }  /* if */
  if (ptr->has_deducible_return_type) {
    disp_boolean("has_deducible_return_type", TRUE);
  }  /* if */
  if (ptr->has_deduced_return_type) {
    disp_boolean("has_deduced_return_type", TRUE);
  }  /* if */
  if (ptr->contains_generic_lambda) {
    disp_boolean("contains_generic_lambda", TRUE);
  }  /* if */
  if (ptr->is_coroutine) {
    disp_boolean("is_coroutine", TRUE);
  }  /* if */
  if (ptr->is_top_level_in_mem_region) {
    disp_boolean("is_top_level_in_mem_region", TRUE);
  }  /* if */
  if (ptr->friend_defined_in_instantiation) {
    disp_boolean("friend_defined_in_instantiation", TRUE);
  }  /* if */
  if (ptr->is_ineligible) {
    disp_boolean("is_ineligible", TRUE);
  }  /* if */
  if (ptr->has_pass_object_size_attr) {
    disp_boolean("has_pass_object_size_attr", TRUE);
  }  /* if */
  if (ptr->from_injected_tokens) {
    disp_boolean("from_injected_tokens", TRUE);
  }  /* if */
#if MAINTAIN_NEEDED_FLAGS
  disp_boolean("definition_needed", (a_boolean)ptr->definition_needed);
  /* Note: the keep_definition_in_il flag is not displayed, since it is
     for front-end use only. */
#endif /* MAINTAIN_NEEDED_FLAGS */
  if (ptr->defined_outside_of_parent) {
    disp_boolean("defined_outside_of_parent", TRUE);
  }  /* if */
#if DECL_MODIFIERS_IN_USE
  disp_decl_modifiers(ptr->decl_modifiers);
#endif /* DECL_MODIFIERS_IN_USE */
  if (ptr->trailing_requires_clause != NULL) {
    disp_ptr("trailing_requires_clause", (char *)ptr->trailing_requires_clause,
             iek_requires_clause);
  }  /* if */
  if (ptr->is_virtual) {
    disp_unsigned_long("number.virtual_function",
                       (unsigned long)ptr->number.virtual_function);
  } else if (ptr->is_constexpr_intrinsic) {
    disp_unsigned_long("number.constexpr_intrinsic",
                       (unsigned long)(long)ptr->number.constexpr_intrinsic);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ptr->overridden_functions != NULL) {
    disp_entity_list("overridden_functions", ptr->overridden_functions);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (rout_befriending_classes(ptr) != NULL) {
    disp_class_list("befriending_classes", rout_befriending_classes(ptr));
  }  /* if */
  if (ptr->assoc_template != NULL) {
    disp_ptr("assoc_template", (char*)ptr->assoc_template, iek_template);
  }  /* if */
  if (ptr->template_arg_list != NULL) {
    disp_template_arg_list("template_arg_list", ptr->template_arg_list);
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
  if (has_gnu_routine_supp(ptr)) {
    a_gnu_routine_supplement_ptr grsp = gnu_routine_supp(ptr);
    if (grsp->section != NULL) {
      disp_string_ptr("section", grsp->section, iek_other_text, (sizeof_t)0);
    }  /* if */
    if (grsp->aliased_routine != NULL) {
      disp_ptr("aliased_routine", (char*)grsp->aliased_routine, iek_routine);
    }  /* if */
#if LOWER_IFUNC
    if (grsp->resolver_var != NULL) {
      disp_ptr("resolver_var", (char *)grsp->resolver_var, iek_variable);
    }  /* if */
#endif /* LOWER_IFUNC */
    if (grsp->inline_partner != NULL) {
      disp_ptr("inline_partner", (char*)grsp->inline_partner, iek_routine);
    }  /* if */
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
    if (ptr->has_ctor_priority) {
      disp_unsigned_long("ctor_priority", (unsigned long)grsp->ctor_priority);
    }  /* if */
    if (ptr->has_dtor_priority) {
      disp_unsigned_long("dtor_priority", (unsigned long)grsp->dtor_priority);
    }  /* if */
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
    if (grsp->asm_name != NULL) {
      disp_string_ptr("asm_name", grsp->asm_name, iek_other_text, (sizeof_t)0);
    }  /* if */
#if GNU_FUNCTION_MULTIVERSIONING
    if (grsp->is_target_specific_version) {
      disp_boolean("is_target_specific_version", TRUE);
    }  /* if */
#if USE_X86_FUNCTION_MULTIVERSIONING
    if (grsp->mv_resolver_required) {
      disp_boolean("mv_resolver_required", TRUE);
    }  /* if */
#endif /* USE_X86_FUNCTION_MULTIVERSIONING */
    if (grsp->is_representative) {
      disp_boolean("is_representative", TRUE);
      disp_routine_list("targeted_versions",
                        grsp->mv_info.representative.targeted_versions);
    }  /* if */
#endif /* GNU_FUNCTION_MULTIVERSIONING */
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  disp_ptr("declared_type", (char *)ptr->declared_type, iek_type);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
  if (ptr->overriding_function_for_wrapper != NULL) {
    disp_ptr("overriding_function_for_wrapper",
             (char *)ptr->overriding_function_for_wrapper,
             iek_routine);
    disp_ptr("overridden_function_for_wrapper",
             (char *)ptr->overridden_function_for_wrapper,
             iek_routine);
  }  /* if */
#if IA64_ABI
  if (ptr->delta != 0) {
    disp_host_large_integer("delta", (a_host_large_integer)ptr->delta);
  }  /* if */
  if (ptr->vcall_index != 0) {
    disp_host_large_integer("vcall_index", 
                            (a_host_large_integer)ptr->vcall_index);
  }  /* if */
  if (ptr->return_delta != 0) {
    disp_host_large_integer("return_delta", 
                            (a_host_large_integer)ptr->return_delta);
  }  /* if */
  if (ptr->vbase_index != 0) {
    disp_host_large_integer("vbase_index", 
                            (a_host_large_integer)ptr->vbase_index);
  }  /* if */
#endif /* IA64_ABI */
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
#if DO_IL_LOWERING && IA64_ABI
  if (ptr->primary_ctor_or_dtor != NULL) {
    disp_ptr("primary_ctor_or_dtor",
             (char *)ptr->primary_ctor_or_dtor,
             iek_routine);
  }  /* if */
#endif /* DO_IL_LOWERING && IA64_ABI */
#if ONE_INSTANTIATION_PER_OBJECT
  if (ptr->instantiation_needed_bit_number != 0) {
    disp_unsigned_long("instantiation_needed_bit_number",
                       (unsigned long)ptr->instantiation_needed_bit_number);
  }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED && DO_IL_LOWERING
  if (ptr->init_priority != 0) {
    disp_unsigned_long("init_priority", (unsigned long)ptr->init_priority);
  }  /* if */
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED && DO_IL_LOWERING */
  if (ptr->generating_using_decl != NULL) {
    disp_ptr("generating_using_decl", (char *)ptr->generating_using_decl,
             iek_using_decl);
  }  /* if */
}  /* disp_routine */


static void disp_label(a_label_ptr ptr)
/*
Display the indicated label.
*/
{
  disp_source_corresp(&ptr->source_corresp, iek_label);
  disp_ptr("next", (char *)ptr->next, iek_label);
  if (ptr->reachable_by_fall_through) {
    disp_boolean("reachable_by_fall_through",
                 (a_boolean)ptr->reachable_by_fall_through);
  }  /* if */
  if (ptr->break_label) {
    disp_boolean("break_label", (a_boolean)ptr->break_label);
  }  /* if */
  if (ptr->switch_break_label) {
    disp_boolean("switch_break_label", (a_boolean)ptr->switch_break_label);
  }  /* if */
  if (ptr->continue_label) {
    disp_boolean("continue_label", (a_boolean)ptr->continue_label);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ptr->leave_label) {
    disp_boolean("leave_label", (a_boolean)ptr->leave_label);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  if (ptr->address_taken) {
    disp_boolean("address_taken", (a_boolean)ptr->address_taken);
  }  /* if */
  if (ptr->locally_declared) {
    disp_boolean("locally_declared", TRUE);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (ptr->is_likely) {
    disp_boolean("is_likely", TRUE);
  }  /* if */
  if (ptr->is_unlikely) {
    disp_boolean("is_unlikely", TRUE);
  }  /* if */
  disp_ptr("exec_stmt", (char *)ptr->exec_stmt, iek_statement);
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ptr->num_microsoft_trys_inside_of != 0) {
    disp_unsigned_long("num_microsoft_trys_inside_of",
                       (unsigned long)ptr->num_microsoft_trys_inside_of);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* disp_label */


static void disp_expr_operator_name(an_expr_operator_kind okind)
/*
Display the name of an expression operator.
*/
{
  a_const_char *s;

  switch (okind) {
    case eok_address_of:        s = "eok_address_of";             break;
    case eok_reference_to:      s = "eok_reference_to";           break;
    case eok_handle_to:         s = "eok_handle_to";              break;
    case eok_indirect:          s = "eok_indirect";               break;
    case eok_ref_indirect:      s = "eok_ref_indirect";           break;
    case eok_cast:              s = "eok_cast";                   break;
    case eok_lvalue_cast:       s = "eok_lvalue_cast";            break;
    case eok_ref_cast:          s = "eok_ref_cast";               break;
    case eok_lvalue_adjust:     s = "eok_lvalue_adjust";          break;
    case eok_class_rvalue_adjust:
                                s = "eok_class_rvalue_adjust";    break;
    case eok_box:               s = "eok_box";                    break;
    case eok_handle_to_box:     s = "eok_handle_to_box";          break;
    case eok_unbox:             s = "eok_unbox";                  break;
    case eok_unbox_lvalue:      s = "eok_unbox_lvalue";           break;
    case eok_base_class_cast:   s = "eok_base_class_cast";        break;
    case eok_derived_class_cast:
                                s = "eok_derived_class_cast";     break;
    case eok_pm_base_class_cast:
                                s = "eok_pm_base_class_cast";     break;
    case eok_pm_derived_class_cast:
                                s = "eok_pm_derived_class_cast";  break;
    case eok_dynamic_cast:      s = "eok_dynamic_cast";           break;
    case eok_ref_dynamic_cast:  s = "eok_ref_dynamic_cast";       break;
    case eok_bool_cast:         s = "eok_bool_cast";              break;
    case eok_array_to_pointer:  s = "eok_array_to_pointer";       break;
    case eok_dot_vacuous_destructor_call:
                                s = "eok_dot_vacuous_destructor_call";
                                                                  break;
    case eok_points_to_vacuous_destructor_call:
                                s = "eok_points_to_vacuous_destructor_call";
                                                                  break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case eok_assume:            s = "eok_assume";                 break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case eok_noexcept:          s = "eok_noexcept";               break;
    case eok_parens:            s = "eok_parens";                 break;
    case eok_negate:            s = "eok_negate";                 break;
    case eok_unary_plus:        s = "eok_unary_plus";             break;
    case eok_complement:        s = "eok_complement";             break;
    case eok_not:               s = "eok_not";                    break;
    case eok_vector_not:        s = "eok_vector_not";             break;
    case eok_vector_fill:       s = "eok_vector_fill";            break;
#if C99_IL_EXTENSIONS_SUPPORTED
    case eok_xconj:             s = "eok_xconj";                  break;
    case eok_real_part:         s = "eok_real_part";              break;
    case eok_imag_part:         s = "eok_imag_part";              break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case eok_post_incr:         s = "eok_post_incr";              break;
    case eok_post_decr:         s = "eok_post_decr";              break;
    case eok_pre_incr:          s = "eok_pre_incr";               break;
    case eok_pre_decr:          s = "eok_pre_decr";               break;
    case eok_add:               s = "eok_add";                    break;
    case eok_subtract:          s = "eok_subtract";               break;
    case eok_multiply:          s = "eok_multiply";               break;
    case eok_divide:            s = "eok_divide";                 break;
    case eok_remainder:         s = "eok_remainder";              break;
#if C99_IL_EXTENSIONS_SUPPORTED
    case eok_jmultiply:         s = "eok_jmultiply";              break;
    case eok_jdivide:           s = "eok_jdivide";                break;
    case eok_fjadd:             s = "eok_fjadd";                  break;
    case eok_jfadd:             s = "eok_jfadd";                  break;
    case eok_fjsubtract:        s = "eok_fjsubtract";             break;
    case eok_jfsubtract:        s = "eok_jfsubtract";             break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case eok_padd:              s = "eok_padd";                   break;
    case eok_psubtract:         s = "eok_psubtract";              break;
    case eok_pdiff:             s = "eok_pdiff";                  break;
    case eok_shiftl:            s = "eok_shiftl";                 break;
    case eok_shiftr:            s = "eok_shiftr";                 break;
    case eok_and:               s = "eok_and";                    break;
    case eok_or:                s = "eok_or";                     break;
    case eok_xor:               s = "eok_xor";                    break;
    case eok_eq:                s = "eok_eq";                     break;
    case eok_ne:                s = "eok_ne";                     break;
    case eok_gt:                s = "eok_gt";                     break;
    case eok_lt:                s = "eok_lt";                     break;
    case eok_ge:                s = "eok_ge";                     break;
    case eok_le:                s = "eok_le";                     break;
    case eok_spaceship:         s = "eok_spaceship";              break;
    case eok_vector_eq:         s = "eok_vector_eq";              break;
    case eok_vector_ne:         s = "eok_vector_ne";              break;
    case eok_vector_gt:         s = "eok_vector_gt";              break;
    case eok_vector_lt:         s = "eok_vector_lt";              break;
    case eok_vector_ge:         s = "eok_vector_ge";              break;
    case eok_vector_le:         s = "eok_vector_le";              break;
    case eok_gnu_min:           s = "eok_gnu_min";                break;
    case eok_gnu_max:           s = "eok_gnu_max";                break;
    case eok_assign:            s = "eok_assign";                 break;
    case eok_add_assign:        s = "eok_add_assign";             break;
    case eok_subtract_assign:   s = "eok_subtract_assign";        break;
    case eok_multiply_assign:   s = "eok_multiply_assign";        break;
    case eok_divide_assign:     s = "eok_divide_assign";          break;
    case eok_remainder_assign:  s = "eok_remainder_assign";       break;
    case eok_shiftl_assign:     s = "eok_shiftl_assign";          break;
    case eok_shiftr_assign:     s = "eok_shiftr_assign";          break;
    case eok_and_assign:        s = "eok_and_assign";             break;
    case eok_or_assign:         s = "eok_or_assign";              break;
    case eok_xor_assign:        s = "eok_xor_assign";             break;
    case eok_padd_assign:       s = "eok_padd_assign";            break;
    case eok_psubtract_assign:  s = "eok_psubtract_assign";       break;
    case eok_bassign:           s = "eok_bassign";                break;
    case eok_land:              s = "eok_land";                   break;
    case eok_lor:               s = "eok_lor";                    break;
    case eok_vector_land:       s = "eok_vector_land";            break;
    case eok_vector_lor:        s = "eok_vector_lor";             break;
    case eok_comma:             s = "eok_comma";                  break;
    case eok_subscript:         s = "eok_subscript";              break;
    case eok_vector_subscript:  s = "eok_vector_subscript";       break;
    case eok_dot_field:         s = "eok_dot_field";              break;
    case eok_points_to_field:   s = "eok_points_to_field";        break;
    case eok_pm_field:          s = "eok_pm_field";               break;
    case eok_pm_points_to_field:s = "eok_pm_points_to_field";     break;
    case eok_dot_pm_func_ptr:   s = "eok_dot_pm_func_ptr";        break;
    case eok_points_to_pm_func_ptr:
                                s = "eok_points_to_pm_func_ptr";  break;
    case eok_dot_static:        s = "eok_dot_static";             break;
    case eok_points_to_static:  s = "eok_points_to_static";       break;
    case eok_virtual_function_ptr:
                                s = "eok_virtual_function_ptr";   break;
    case eok_question:          s = "eok_question";               break;
    case eok_vector_question:   s = "eok_vector_question";        break;
    case eok_call:              s = "eok_call";                   break;
    case eok_dot_member_call:   s = "eok_dot_member_call";        break;
    case eok_points_to_member_call:
                                s = "eok_points_to_member_call";  break;
    case eok_dot_pm_call:       s = "eok_dot_pm_call";            break;
    case eok_points_to_pm_call: s = "eok_points_to_pm_call";      break;
    case eok_cli_subscript:     s = "eok_cli_subscript";          break;
    case eok_va_start:          s = "eok_va_start";               break;
    case eok_va_arg:            s = "eok_va_arg";                 break;
    case eok_va_end:            s = "eok_va_end";                 break;
    case eok_va_copy:           s = "eok_va_copy";                break;
    case eok_va_start_single_operand:
                                s = "eok_va_start_single_operand";
                                                                  break;
    case eok_lvalue:            s = "eok_lvalue";                 break;
    case eok_await:             s = "eok_await";                  break;
    case eok_yield:             s = "eok_yield";                  break;
    case eok_splice:            s = "eok_splice";                 break;
    case eok_error:             s = "eok_error";                  break;
    default:                    s = "**BAD EXPR OPERATOR KIND**"; break;
  }  /* switch */
  (void)fprintf(f_display, "%s", s);
}  /* disp_expr_operator_name */


static void disp_new_delete_supplement(a_new_delete_supplement_ptr ndsp)
/*
Display the indicated new/delete supplement to an expression node.
*/
{
  disp_boolean("is_new", (a_boolean)ndsp->is_new);
  disp_boolean("placement_new", (a_boolean)ndsp->placement_new);
  disp_boolean("aligned_version", (a_boolean)ndsp->aligned_version);
  disp_boolean("array_delete", (a_boolean)ndsp->array_delete);
  disp_boolean("global_new_or_delete", (a_boolean)ndsp->global_new_or_delete);
  disp_boolean("has_new_initializer", (a_boolean)ndsp->has_new_initializer);
  disp_boolean("new_initializer_is_brace_enclosed",
               (a_boolean)ndsp->new_initializer_is_brace_enclosed);
  disp_boolean("new_initializer_is_paren_aggr_init",
               (a_boolean)ndsp->new_initializer_is_paren_aggr_init);
  disp_boolean("deducible_type", (a_boolean)ndsp->deducible_type);
  if (ndsp->parenthesized_type_id) {
    disp_boolean("parenthesized_type_id", TRUE);
  }  /* if */
  disp_ptr("type", (char *)ndsp->type, iek_type);
  disp_ptr("routine", (char *)ndsp->routine, iek_routine);
  disp_ptr("arg", (char *)ndsp->arg, iek_expr_node);
  disp_ptr("dynamic_init", (char *)ndsp->dynamic_init, iek_dynamic_init);
  disp_ptr("freeing_of_storage_on_exception",
           (char *)ndsp->freeing_of_storage_on_exception,
           iek_dynamic_init);
  disp_ptr("number_of_elements", (char *)ndsp->number_of_elements,
           iek_expr_node);
}  /* disp_new_delete_supplement */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void disp_gcnew_supplement(a_gcnew_supplement_ptr gsp)
/*
Display the indicated gcnew supplement for an expression node.
*/
{
  disp_boolean("has_new_initializer", (a_boolean)gsp->has_new_initializer);
  disp_boolean("is_cli_array", (a_boolean)gsp->is_cli_array);
  disp_ptr("type", (char *)gsp->type, iek_type);
  disp_ptr("cli_array_dimension_lengths",
           (char *)gsp->cli_array_dimension_lengths, iek_expr_node);
  disp_ptr("dynamic_init", (char *)gsp->dynamic_init, iek_dynamic_init);
}  /* disp_gcnew_supplement */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if !ABI_CHANGES_FOR_RTTI

static void disp_accessible_base_classes(an_accessible_base_class_ptr abcp)
/*
Display the indicated accessible base class entry.
*/
{
  if (abcp == NULL) {
    disp_ptr("accessible_base_classes", (char *)abcp,
             iek_accessible_base_class);
  } else {
    disp_name("accessible_base_classes");
    (void)fprintf(f_display, "\n");
    for (; abcp != NULL; abcp = abcp->next) {
      disp_ptr("  base_class", (char *)abcp->base_class, iek_base_class);
    }  /* for */
  }  /* if */
}  /* disp_accessible_base_classes */

#endif /* !ABI_CHANGES_FOR_RTTI */

static void disp_throw_supplement(a_throw_supplement_ptr tsp)
/*
Display the indicated throw supplement to an expression node.
*/
{
  disp_ptr("type", (char *)tsp->type, iek_type);
  disp_ptr("dynamic_init", (char *)tsp->dynamic_init, iek_dynamic_init);
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
  if (tsp->expr != NULL) disp_ptr("expr", (char *)tsp->expr, iek_expr_node);
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#if !ABI_CHANGES_FOR_RTTI
  if (tsp->type->kind == (a_type_kind)tk_class ||
      tsp->type->kind == (a_type_kind)tk_struct ||
      tsp->type->kind == (a_type_kind)tk_union) {
    disp_accessible_base_classes(tsp->accessible_base_classes);
  }  /* if */
#endif /* !ABI_CHANGES_FOR_RTTI */
  disp_ptr("destructor", (char *)tsp->destructor, iek_routine);
}  /* disp_throw_supplement */


static void disp_condition_supplement(a_condition_supplement_ptr csp)
/*
Display the indicated condition supplement to an expression node.
*/
{
  disp_ptr("scope", (char *)csp->scope, iek_scope);
  disp_ptr("dynamic_init", (char *)csp->dynamic_init, iek_dynamic_init);
  disp_ptr("expr", (char *)csp->expr, iek_expr_node);
  disp_ptr("initialization", (char *)csp->initialization, iek_statement);
}  /* disp_condition_supplement */


#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING

static void disp_eh_prologue_supplement(an_eh_prologue_supplement_ptr psp)
/*
Display the indicated exception handling prologue supplement to an expression
node.
*/
{
  disp_ptr("routine", (char *)psp->routine, iek_routine);
#if GENERATE_EH_TABLES
  disp_ptr("region_table", (char *)psp->region_table, iek_variable);
  disp_ptr("array_table", (char *)psp->array_table, iek_variable);
#endif /* GENERATE_EH_TABLES */
}  /* disp_eh_prologue_supplement */

#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */

static void disp_local_expr_node_ref(a_local_expr_node_ref_ptr  ptr)
/*
Display the given reference to an expression node in a function-scope (i.e.,
local) memory region.
*/
{
  disp_ptr("expr", (char*)ptr->expr, iek_expr_node);
  switch (ptr->kind) {
#if PROTOTYPE_INSTANTIATIONS_IN_IL
    case lerk_generic_sizeof:
      (void)fprintf(f_display, "generic-sizeof");
      break;
    case lerk_tpl_param_expr:
      (void)fprintf(f_display, "tpl-param-expr");
      break;
#endif /* PROTOTYPE_INSTANTIATIONS_IN_IL */
    case lerk_array_bound:
      (void)fprintf(f_display, "array-bound");
      break;
    case lerk_dep_array_bound:
      (void)fprintf(f_display, "dep-array-bound");
      break;
    case lerk_decltype:
      (void)fprintf(f_display, "decltype");
      break;
    case lerk_bit_field_width:
      (void)fprintf(f_display, "bit-field-width");
      break;
    case lerk_constant_expr:
      (void)fprintf(f_display, "constant-expr");
      break;
    case lerk_scoped_expr:
      (void)fprintf(f_display, "scoped-expr");
      break;
    default:
      (void)fprintf(f_display, "**BAD LOCAL-EXPR-NODE-REF KIND**");
  }  /* switch */
  disp_ptr(" referrer", (char*)ptr->referrer.ptr,
           (an_il_entry_kind)ptr->referrer.kind);
}  /* disp_local_expr_node_ref */


static void disp_expr_node(an_expr_node_ptr ptr)
/*
Display the indicated expression node.
*/
{
  disp_ptr("type", (char *)ptr->type, iek_type);
  if (ptr->orig_lvalue_type != NULL) {
    disp_ptr("orig_lvalue_type", (char *)ptr->orig_lvalue_type, iek_type);
  }  /* if */
  disp_ptr("next", (char *)ptr->next, iek_expr_node);
  if (ptr->is_lvalue) {
    disp_boolean("is_lvalue", TRUE);
  }  /* if */
  if (ptr->is_xvalue) {
    disp_boolean("is_xvalue", TRUE);
  }  /* if */
  if (ptr->result_is_not_used) {
    disp_boolean("result_is_not_used", TRUE);
  }  /* if */
  if (ptr->is_initialization_guard) {
    disp_boolean("is_initialization_guard", TRUE);
  }  /* if */
  if (ptr->generated_default_arg) {
    disp_boolean("generated_default_arg", TRUE);
  }  /* if */
  if (ptr->marked_as_gnu_extension) {
    disp_boolean("marked_as_gnu_extension", TRUE);
  }  /* if */
  if (ptr->is_static_cast) {
    disp_boolean("is_static_cast", TRUE);
  }  /* if */
  if (ptr->is_functional_notation_cast) {
    disp_boolean("is_functional_notation_cast", TRUE);
  }  /* if */
  if (ptr->is_brace_notation_cast) {
    disp_boolean("is_brace_notation_cast", TRUE);
  }  /* if */
  if (ptr->is_objectless_nonstatic_data_mem_ref) {
    disp_boolean("is_objectless_nonstatic_data_mem_ref", TRUE);
  }  /* if */
  if (ptr->is_pack_expansion) {
    disp_boolean("is_pack_expansion", TRUE);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ptr->is_safe_cast) {
    disp_boolean("is_safe_cast", TRUE);
  }  /* if */
  if (ptr->element_of_cli_param_array_arg) {
    disp_boolean("element_of_cli_param_array_arg", TRUE);
  }  /* if */
  if (ptr->is_cli_typeid) {
    disp_boolean("is_cli_typeid", TRUE);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING
  if (ptr->is_non_normalized_boolean_controlling_expr) {
    disp_boolean("is_non_normalized_boolean_controlling_expr", TRUE);
  }  /* if */
#endif /* DO_IL_LOWERING */
#if BACK_END_IS_CP_GEN_BE
  if (ptr->keep_as_cast_for_cp_gen_be) {
    disp_boolean("keep_as_cast_for_cp_gen_be", TRUE);
  }  /* if */
  if (ptr->needed_in_cp_gen_be) {
    disp_boolean("needed_in_cp_gen_be", TRUE);
  }  /* if */
#endif /* BACK_END_IS_CP_GEN_BE */
  if (ptr->is_parenthesized) {
    disp_boolean("is_parenthesized", TRUE);
  }  /* if */
  if (ptr->type_definition_needed) {
    disp_boolean("type_definition_needed", TRUE);
  }  /* if */
  if (ptr->volatile_fetch) {
    disp_boolean("volatile_fetch", TRUE);
  }  /* if */
  if (ptr->do_not_interpret) {
    disp_boolean("do_not_interpret", TRUE);
  }  /* if */
  if (ptr->compiler_generated) {
    disp_boolean("compiler_generated", TRUE);
  }  /* if */
  if (ptr->is_type_constraint) {
    disp_boolean("is_type_constraint", TRUE);
  }  /* if */
  if (ptr->was_lvalue_temp_initializer) {
    disp_boolean("was_lvalue_temp_initializer", TRUE);
  }  /* if */
  disp_name("kind");
  (void)fprintf(f_display, "enk_%s\n", expr_node_kind_names[ptr->kind]);
  switch (ptr->kind) {
    case enk_error:
      break;
    case enk_operation:
      disp_name("operation.kind");
      disp_expr_operator_name(ptr->variant.operation.kind);
      (void)fprintf(f_display, "\n");
      disp_name("operation.type_kind");
      (void)fprintf(f_display, "%s\n",
                    type_kind_string(ptr->variant.operation.type_kind));
      if (ptr->variant.operation.returns_lvalue_instead_of_usual_rvalue) {
        disp_boolean("returns_lvalue_instead_of_usual_rvalue", TRUE);
      }  /* if */
      if (ptr->variant.operation.is_reinterpret_cast) {
        disp_boolean("is_reinterpret_cast", TRUE);
      }  /* if */
      if (ptr->variant.operation.is_reinterpret_like_cast) {
        disp_boolean("is_reinterpret_like_cast", TRUE);
      }  /* if */
      if (ptr->variant.operation.is_const_cast) {
        disp_boolean("is_const_cast", TRUE);
      }  /* if */
      if (ptr->variant.operation.is_reference_cast) {
        disp_boolean("is_reference_cast", TRUE);
      }  /* if */
      if (ptr->variant.operation.is_rvalue_reference_cast) {
        disp_boolean("is_rvalue_reference_cast", TRUE);
      }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (ptr->variant.operation.is_tracking_reference_cast) {
        disp_boolean("is_tracking_reference_cast", TRUE);
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      if (ptr->variant.operation.implicit_in_member_naming) {
        disp_boolean("implicit_in_member_naming", TRUE);
      }  /* if */
      if (ptr->variant.operation.implicit_step_of_explicit_cast) {
        disp_boolean("implicit_step_of_explicit_cast", TRUE);
      }  /* if */
      if (ptr->variant.operation.is_conversion_call) {
        disp_boolean("is_conversion_call", TRUE);
      }  /* if */
      if (ptr->variant.operation.arg_dependent_lookup_suppressed_on_call) {
        disp_boolean("arg_dependent_lookup_suppressed_on_call", TRUE);
      }  /* if */
      if (ptr->variant.operation.call_with_qualified_function_name) {
        disp_boolean("call_with_qualified_function_name", TRUE);
      }  /* if */
#if BACK_END_IS_CP_GEN_BE
      if (ptr->variant.operation.only_found_through_arg_dependent_lookup) {
        disp_boolean("only_found_through_arg_dependent_lookup", TRUE);
      }  /* if */
      if (ptr->variant.operation.called_through_address_of_overload_set) {
        disp_boolean("called_through_address_of_overload_set", TRUE);
      }  /* if */
#endif /* BACK_END_IS_CP_GEN_BE */
      if (ptr->variant.operation.call_uses_operator_syntax) {
        disp_boolean("call_uses_operator_syntax", TRUE);
      }  /* if */
#if GNU_EXTENSIONS_ALLOWED
      if (ptr->variant.operation.is_gnu_two_operand_question_mark) {
        disp_boolean("is_gnu_two_operand_question_mark", TRUE);
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
      if (ptr->variant.operation.pointer_operand_is_second) {
        disp_boolean("pointer_operand_is_second", TRUE);
      }  /* if */
      if (ptr->variant.operation.is_virtual_call) {
        disp_boolean("is_virtual_call", TRUE);
      }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (ptr->variant.operation.rewritten_property_reference_kind !=
                              (a_rewritten_property_reference_kind)rprk_none) {
        disp_name("rewritten_property_reference_kind");
        switch (ptr->variant.operation.rewritten_property_reference_kind) {
          case rprk_compound_assignment:
            (void)fprintf(f_display, "rprk_compound_assignment\n");
            break;
          case rprk_pre_incr_decr:
            (void)fprintf(f_display, "rprk_pre_incr_decr\n");
            break;
          case rprk_post_incr_decr:
            (void)fprintf(f_display, "rprk_post_incr_decr\n");
            break;
          default:
            (void)fprintf(f_display, "**BAD REWRITTEN PROP REF KIND**\n");
            break;
        }  /* switch */
      }  /* if */
      if (ptr->variant.operation.requires_runtime_cast_check) {
        disp_boolean("requires_runtime_cast_check", TRUE);
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if BACK_END_IS_C_GEN_BE
      if (ptr->variant.operation.has_deferred_ampersand) {
        disp_boolean("has_deferred_ampersand", TRUE);
      }  /* if */
#endif /* BACK_END_IS_C_GEN_BE */
      if (ptr->variant.operation.eval_left_to_right) {
        disp_boolean("eval_left_to_right", TRUE);
      }  /* if */
      if (ptr->variant.operation.eval_right_to_left) {
        disp_boolean("eval_right_to_left", TRUE);
      }  /* if */
      disp_ptr("operands", (char *)ptr->variant.operation.operands,
               iek_expr_node);
      break;
    case enk_constant:
      disp_ptr("constant", (char *)ptr->variant.constant.ptr, iek_constant);
      if (ptr->variant.constant.name_reference != NULL) {
        disp_name_reference(ptr->variant.constant.name_reference);
      }  /* if */
      break;
    case enk_variable:
      disp_ptr("variable", (char *)ptr->variant.variable.ptr, iek_variable);
      if (ptr->variant.variable.name_reference != NULL) {
        disp_name_reference(ptr->variant.variable.name_reference);
      }  /* if */
      break;
    case enk_routine:
      disp_ptr("routine", (char *)ptr->variant.routine.ptr, iek_routine);
      if (ptr->variant.routine.name_reference != NULL) {
        disp_name_reference(ptr->variant.routine.name_reference);
      }  /* if */
      break;
    case enk_field:
      disp_ptr("field", (char *)ptr->variant.field.ptr, iek_field);
      if (ptr->variant.field.name_reference != NULL) {
        disp_name_reference(ptr->variant.field.name_reference);
      }  /* if */
      break;
    case enk_temp_init:
      disp_ptr("dynamic_init", (char *)ptr->variant.init.dynamic_init,
               iek_dynamic_init);
      disp_ptr("source.type", (char *)ptr->variant.init.source.type, iek_type);
      break;
    case enk_lambda:
      disp_ptr("dynamic_init", (char *)ptr->variant.init.dynamic_init,
               iek_dynamic_init);
      disp_ptr("source.lambda", (char *)ptr->variant.init.source.lambda,
               iek_lambda);
      break;
    case enk_new_delete:
      disp_new_delete_supplement(ptr->variant.new_delete);
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case enk_gcnew:
      disp_gcnew_supplement(ptr->variant.gcnew_info);
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case enk_throw:
      if (ptr->variant.throw_info != NULL) {
        disp_throw_supplement(ptr->variant.throw_info);
      }  /* if */
      break;
    case enk_condition:
      if (ptr->variant.condition != NULL) {
        disp_condition_supplement(ptr->variant.condition);
      }  /* if */
      break;
    case enk_object_lifetime:
      disp_ptr("expr", (char *)ptr->variant.object_lifetime.expr,
               iek_expr_node);
      disp_ptr("ptr", (char *)ptr->variant.object_lifetime.ptr,
               iek_object_lifetime);
      break;
    case enk_typeid:
      disp_ptr("type_with_opt_expr",
               (char *)ptr->variant.typeid_info.type_with_opt_expr,
               iek_expr_node);
      if (ptr->variant.typeid_info.is_dynamic) {
        disp_boolean("is_dynamic", TRUE);
      }  /* if */
      break;
    case enk_alignof:
      goto sizeof_cases;
    case enk_datasizeof:
      goto sizeof_cases;
    case enk_sizeof:
sizeof_cases:
      disp_boolean("is_type",
                   (a_boolean)ptr->variant.sizeof_info.is_type);
      disp_boolean("is_std_alignof",
                   (a_boolean)ptr->variant.sizeof_info.is_std_alignof);
      if (ptr->variant.sizeof_info.is_type) {
        disp_ptr("type", (char *)ptr->variant.sizeof_info.variant.type,
                 iek_type);
      } else {
        disp_ptr("expr", (char *)ptr->variant.sizeof_info.variant.expr,
                 iek_expr_node);
      }  /* if */
      break;
    case enk_sizeof_pack:
      disp_boolean("is_type",
                   (a_boolean)ptr->variant.sizeof_pack.is_type);
      disp_boolean("is_template_template",
                   (a_boolean)ptr->variant.sizeof_pack.is_template_template);
      if (ptr->variant.sizeof_pack.is_template_template) {
        disp_ptr("templ", (char *)ptr->variant.sizeof_pack.variant.templ,
                 iek_template);
      } else if (ptr->variant.sizeof_pack.is_type) {
        disp_ptr("type", (char *)ptr->variant.sizeof_pack.variant.type,
                 iek_type);
      } else {
        disp_ptr("expr", (char *)ptr->variant.sizeof_pack.variant.expr,
                 iek_expr_node);
      }  /* if */
      break;
    case enk_address_of_ellipsis:
      break;
    case enk_statement:
      disp_ptr("statement", (char *)ptr->variant.statement, iek_statement);
      break;
    case enk_reuse_value:
      disp_ptr("reused_value_init", (char *)ptr->variant.reused_value_init,
               iek_dynamic_init);
      break;
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
    /* Nodes generated by IL lowering for partial lowering of exception
       handling features. */
    case enk_lowered_eh_construct:
      disp_name("lowered_eh.kind");
      switch (ptr->variant.lowered_eh.kind) {
        case leck_caught_object_address:
          (void)fprintf(f_display, "leck_caught_object_address\n");
          disp_ptr("caught_object_handler",
                   (char *)ptr->variant.lowered_eh.variant.
                                                         caught_object_handler,
                   iek_handler);
          break;
        case leck_thrown_object_address:
          (void)fprintf(f_display, "leck_thrown_object_address\n");
          break;
        case leck_cleanup_state:
          (void)fprintf(f_display, "leck_cleanup_state\n");
          goto cleanup_state_common;
        case leck_unreachable_cleanup_state:
          (void)fprintf(f_display, "leck_unreachable_cleanup_state\n");
cleanup_state_common:
#if GENERATE_EH_TABLES
          disp_long("cleanup_region_number",
                  (long)ptr->variant.lowered_eh.variant.cleanup_region_number);
#else /* !GENERATE_EH_TABLES */
          disp_ptr("cleanup_ptr",
                   (char *)ptr->variant.lowered_eh.variant.cleanup_ptr,
                   iek_dynamic_init);
#endif /* GENERATE_EH_TABLES */
          break;
        case leck_function_prologue:
          (void)fprintf(f_display, "leck_function_prologue\n");
          disp_eh_prologue_supplement(
                                ptr->variant.lowered_eh.variant.prologue_info);
          break;
        case leck_function_epilogue:
          (void)fprintf(f_display, "leck_function_epilogue\n");
          disp_ptr("epilogue_routine",
                   (char *)ptr->variant.lowered_eh.variant.epilogue_routine,
                   iek_routine);
          break;
        case leck_catch_epilogue:
          (void)fprintf(f_display, "leck_catch_epilogue\n");
          disp_ptr("epilogue_handler",
                   (char *)ptr->variant.lowered_eh.variant.epilogue_handler,
                   iek_handler);
          break;
        case leck_try_epilogue:
          (void)fprintf(f_display, "leck_try_epilogue\n");
          disp_ptr("epilogue_try_block",
                   (char *)ptr->variant.lowered_eh.variant.epilogue_try_block,
                   iek_try_supplement);
          break;
        case leck_exception_caught:
          (void)fprintf(f_display, "leck_exception_caught\n");
          break;
        case leck_exception_started:
          (void)fprintf(f_display, "leck_exception_started\n");
          break;
#if !GENERATE_EH_TABLES
        case leck_initialization_completed:
          (void)fprintf(f_display, "leck_initialization_completed\n");
          disp_ptr("dynamic_init",
                   (char *)ptr->variant.lowered_eh.variant.dynamic_init,
                   iek_dynamic_init);
          break;
#endif /* !GENERATE_EH_TABLES */
        case leck_internal_try:
          (void)fprintf(f_display, "leck_internal_try\n");
          disp_ptr("  try_and_catch_expr",
                   (char *)ptr->variant.lowered_eh.variant.try_and_catch_expr,
                   iek_expr_node);
          break;
        default:
          (void)fprintf(f_display, "**BAD LOWERED EH CONSTRUCT KIND**\n");
      }  /* switch */
      break;
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
    case enk_result_of_overriding_function:
      /* Node generated as part of the body of an entry function used
         as a wrapper for a call of an overriding virtual function
         with a covariant return type. */
      break;
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
#if VLA_DEALLOCATIONS_IN_IL
    case enk_vla_dealloc:
      disp_ptr("vla_variable", (char *)ptr->variant.vla_variable,
               iek_variable);
      break;
#endif /* VLA_DEALLOCATIONS_IN_IL */
    case enk_type_operand:
      disp_ptr("type", (char *)ptr->variant.type_operand.type, iek_type);
      if (ptr->variant.type_operand.name_reference != NULL) {
        disp_name_reference(ptr->variant.type_operand.name_reference);
      }  /* if */
      break;
    case enk_builtin_operation:
      disp_name("builtin_operation.kind");
      (void)fprintf(f_display, "%s\n", builtin_operation_names[
                                        ptr->variant.builtin_operation.kind]);
      disp_ptr("operands", (char *)ptr->variant.builtin_operation.operands,
               iek_expr_node);
      break;
    case enk_param_ref:
      disp_unsigned_long("param_ref.param_num",
                         (unsigned long)ptr->variant.param_ref.param_num);
      disp_unsigned_long("param_ref.levels_up",
                         (unsigned long)ptr->variant.param_ref.levels_up);
      break;
    case enk_braced_init_list:
      disp_ptr("braced_init_list", (char *)ptr->variant.braced_init_list,
               iek_expr_node);
      break;
    case enk_c11_generic:
      disp_ptr("c11_generic.operands",
               (char *)ptr->variant.c11_generic.operands,
               iek_expr_node);
      disp_ptr("c11_generic.result", (char *)ptr->variant.c11_generic.result,
               iek_expr_node);
      break;
#if BUILTIN_FUNCTIONS_ENABLED
    case enk_builtin_choose_expr:
      disp_ptr("builtin_choose_expr.operands",
               (char *)ptr->variant.builtin_choose_expr.operands,
               iek_expr_node);
      disp_boolean("builtin_choose_expr.choose_first",
                   (a_boolean)ptr->variant.builtin_choose_expr.choose_first);
      break;
#endif /* BUILTIN_FUNCTIONS_ENABLED */
    case enk_await:
    case enk_yield:
      (void)fprintf(f_display, ptr->kind == enk_await ? "enk_await\n"
                                                      : "enk_yield\n");
      disp_ptr("await_info.operand", (char *)ptr->variant.await_info.operand,
               iek_expr_node);
      disp_ptr("await_info.ready_resume_suspend",
               (char *)ptr->variant.await_info.ready_resume_suspend,
               iek_expr_node);
      break;
    case enk_fold:
      disp_name("fold.operator_token");
      (void)fprintf(f_display, "%s\n",
                    token_names[ptr->variant.fold.operator_token]);
      disp_boolean("fold.left_associative",
                   (a_boolean)ptr->variant.fold.left_associative);
      disp_ptr("fold.operands", (char *)ptr->variant.fold.operands,
               iek_expr_node);
      break;
    case enk_initializer:
      disp_ptr("initializer.dyn_init",
               (char *)ptr->variant.initializer.dyn_init, iek_dynamic_init);
      break;
    case enk_concept_id:
      disp_ptr("concept_id.concept_template",
               (char *)ptr->variant.concept_id.concept_template, iek_template);
      disp_template_arg_list("concept_id.args", ptr->variant.concept_id.args);
      break;
    case enk_requires:
      disp_ptr("requires_expr.requirements",
               (char *)ptr->variant.requires_expr.requirements, iek_expr_node);
      disp_ptr("requires_expr.parameters",
               (char *)ptr->variant.requires_expr.parameters, iek_param_type);
      break;
    case enk_compound_req:
      disp_ptr("compound_req.expr_and_constraint",
               (char *)ptr->variant.compound_req.expr_and_constraint,
               iek_expr_node);
      if (ptr->variant.compound_req.is_noexcept) {
        disp_boolean("compound_req.is_noexcept", TRUE);
      }  /* if */
      break;
    case enk_nested_req:
      disp_ptr("nested_req.constraint",
               (char *)ptr->variant.nested_req.constraint, iek_expr_node);
      break;
    case enk_const_eval_deferred:
      { auto &deferred_state = ptr->variant.const_eval_deferred;

        disp_ptr("const_eval_deferred.wrapped", (char *)deferred_state.wrapped,
                 iek_expr_node);
        if (deferred_state.reattempt_state.default_arg) {
          disp_boolean("const_eval_deferred.reattempt_state.default_arg",
                       TRUE);
        }  /* if */
        if (deferred_state.reattempt_state.default_mem_init) {
          disp_boolean("const_eval_deferred.reattempt_state.default_mem_init",
                       TRUE);
        }  /* if */
      }
      break;
    case enk_template_name:
      disp_ptr("template_name", (char *)ptr->variant.template_name,
               iek_template);
      break;
    case enk_pack_index:
      disp_ptr("expr", (char *)ptr->variant.pack_index.expr, iek_expr_node);
      disp_ptr("index_expr", (char *)ptr->variant.pack_index.index_expr,
               iek_expr_node);
      break;
    default:
      (void)fprintf(f_display, "**BAD EXPR NODE KIND**\n");
  }  /* switch */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_source_range("expr_range", &ptr->expr_range);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  disp_source_position("position", &ptr->position);
}  /* disp_expr_node */


static void disp_switch_case_entry(a_switch_case_entry_ptr ptr)
/*
Display the indicated switch case entry.
*/
{
  disp_ptr("stmt", (char *)ptr->stmt, iek_statement);
  disp_ptr("case_value", (char *)ptr->case_value, iek_constant);
#if GNU_EXTENSIONS_ALLOWED
  disp_ptr("range_end", (char *)ptr->range_end, iek_constant);
#endif /* GNU_EXTENSIONS_ALLOWED */
  disp_ptr("next", (char *)ptr->next, iek_switch_case_entry);
  disp_ptr("next_on_sorted_list", (char *)ptr->next_on_sorted_list,
           iek_switch_case_entry);
  disp_source_position("position", &ptr->position);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_source_position("end_position", &ptr->end_position);
  disp_source_position("colon_position", &ptr->colon_position);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  if (ptr->reachable_by_fall_through) {
    disp_boolean("reachable_by_fall_through", TRUE);
  }  /* if */
}  /* disp_switch_case_entry */


static void disp_switch_stmt_descr(a_switch_stmt_descr_ptr ptr)
/*
Display the indicated switch statement description.
*/
{
  disp_ptr("cases", (char *)ptr->cases, iek_switch_case_entry);
  disp_ptr("default_case", (char *)ptr->default_case, iek_switch_case_entry);
  disp_ptr("sorted_cases", (char *)ptr->sorted_cases, iek_switch_case_entry);
}  /* disp_switch_stmt_descr */


static void disp_exception_specification_type(
                                  an_exception_specification_type_ptr ptr)
/*
Display the indicated exception-specification-type entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_exception_specification_type);
  disp_ptr("type", (char *)ptr->type, iek_type);  
  disp_boolean("redundant", (a_boolean)ptr->redundant);
  if (ptr->is_pack_expansion) {
    disp_boolean("is_pack_expansion", TRUE);
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_source_position("source_position", &ptr->source_position);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* disp_exception_specification_type */


static void disp_exception_specification(an_exception_specification_ptr ptr)
/*
Display the indicated exception-specification entry.
*/
{
  if (ptr->is_noexcept) disp_boolean("is_noexcept", TRUE);
  if (ptr->indeterminate) disp_boolean("indeterminate", TRUE);
  if (ptr->throw_any) disp_boolean("throw_any", TRUE);
  if (ptr->compiler_generated) disp_boolean("compiler_generated", TRUE);
  if (ptr->from_attribute) disp_boolean("from_attribute", TRUE);
  if (ptr->arg_cached) {
    disp_boolean("arg_cached", TRUE);
  } else if (ptr->copy_from_prototype) {
    disp_boolean("copy_from_prototype", TRUE);
  } else if (ptr->is_noexcept) {
    disp_ptr("noexcept_arg", (char *)ptr->variant.noexcept_arg,
             iek_constant);
  } else {
    disp_ptr("exception_specification_type_list",
             (char *)ptr->variant.exception_specification_type_list,
             iek_exception_specification_type);
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_source_range("source_range", &ptr->source_range);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* disp_exception_specification */


static void disp_handler(a_handler_ptr ptr)
/*
Display the indicated handler.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_handler);
  disp_source_position("catch_position", &ptr->catch_position);
  disp_ptr("parameter", (char *)ptr->parameter, iek_variable);
  disp_ptr("statement", (char *)ptr->statement, iek_statement);
  disp_ptr("dynamic_init", (char *)ptr->dynamic_init, iek_dynamic_init);
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
  disp_ptr("typeinfo_var", (char *)ptr->typeinfo_var, iek_variable);
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
}  /* disp_handler */


static void disp_try_supplement(a_try_supplement_ptr ptr)
/*
Display the indicated exception-handling "try" supplement.
*/
{
  disp_boolean("is_function_try_block", (a_boolean)ptr->is_function_try_block);
  disp_ptr("statement", (char *)ptr->statement, iek_statement);
  disp_ptr("handlers", (char *)ptr->handlers, iek_handler);
#if MICROSOFT_EXTENSIONS_ALLOWED
  disp_ptr("finally", (char *)ptr->finally_statement, iek_statement);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  disp_ptr("lifetime", (char *)ptr->lifetime, iek_object_lifetime);
}  /* disp_try_supplement */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void disp_microsoft_try_supplement(a_microsoft_try_supplement_ptr ptr)
/*
Display the indicated Microsoft structured exception handling try-finally
or try-except statement supplement.
*/
{
  disp_ptr("guarded_statement", (char *)ptr->guarded_statement, iek_statement);
  disp_ptr("except_expr", (char *)ptr->except_expr, iek_expr_node);
  disp_ptr("cleanup_statement", (char *)ptr->cleanup_statement, iek_statement);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_source_position("except_or_finally_position",
                       &ptr->except_or_finally_position);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* disp_microsoft_try_supplement */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void disp_coroutine_descr(a_coroutine_descr_ptr  cdp)
/*
Display the indicated coroutine description.
*/
{
  if (cdp->error_descr) {
    disp_boolean("error_descr", TRUE);
    goto done;
  }  /* if */
  disp_ptr("traits", (char*)cdp->traits, iek_type);
  disp_ptr("handle", (char*)cdp->handle, iek_variable);
  disp_ptr("promise", (char*)cdp->promise, iek_variable);
  disp_ptr("init_await_resume", (char*)cdp->init_await_resume, iek_variable);
  disp_ptr("this_param_copy", (char*)cdp->this_param_copy, iek_variable);
  disp_variable_list("paramter_copies", cdp->parameter_copies);
  disp_ptr("final_suspend_label", (char*)cdp->final_suspend_label, iek_label);
  disp_ptr("initial_suspend_call", (char*)cdp->initial_suspend_call,
           iek_expr_node);
  disp_ptr("final_suspend_call", (char*)cdp->final_suspend_call,
           iek_expr_node);
  disp_ptr("unhandled_exception_call", (char*)cdp->unhandled_exception_call,
           iek_expr_node);
  disp_ptr("get_return_object_call", (char*)cdp->get_return_object_call,
           iek_expr_node);
  disp_ptr("alloc_failure_gro_call", (char*)cdp->alloc_failure_gro_call,
           iek_expr_node);
  disp_ptr("new_routine", (char*)cdp->new_routine, iek_routine);
  disp_ptr("delete_routine", (char*)cdp->delete_routine, iek_routine);
  disp_source_position("position", &cdp->position);
  if (cdp->has_return_void) {
    disp_boolean("has_return_void", TRUE);
  }  /* if */
  if (cdp->body_generated) {
    disp_boolean("body_generated", TRUE);
  }  /* if */
done:;
}  /* disp_coroutine_descr */


static void disp_block(a_block_ptr ptr)
/*
Display the indicated block.
*/
{
  disp_source_position("final_position", &ptr->final_position);
  disp_ptr("assoc_scope", (char *)ptr->assoc_scope, iek_scope);
  disp_ptr("lifetime", (char *)ptr->lifetime, iek_object_lifetime);
  disp_boolean("end_of_block_reachable",
               (a_boolean)ptr->end_of_block_reachable);
  if (ptr->is_statement_expression) {
    disp_boolean("is_statement_expression", TRUE);
  }  /* if */
  if (ptr->implicit_scope_not_allowed) {
    disp_boolean("implicit_scope_not_allowed", TRUE);
  }  /* if */
}  /* disp_block */


static void disp_range_based_for_statement(a_statement_ptr ptr)
/*
Display a range-based-for statement.
*/
{
  a_range_based_for_loop_ptr extra_info =
                                  ptr->variant.range_based_for_loop.extra_info;

  (void)fprintf(f_display, "stmk_range_based_for\n");
  disp_ptr("statement",
           (char *)ptr->variant.range_based_for_loop.statement,
           iek_statement);
  disp_ptr("initialization", (char *)extra_info->initialization,
           iek_statement);
  disp_ptr("iterator", (char *)extra_info->iterator,
           iek_variable);
  disp_ptr("range", (char *)extra_info->range,
           iek_variable);
  disp_ptr("range_based_for_scope", (char *)extra_info->range_based_for_scope,
           iek_scope);
  disp_ptr("iterator_scope", (char *)extra_info->iterator_scope,
           iek_scope);
  disp_ptr("begin", (char *)extra_info->begin,
           iek_variable);
  disp_ptr("end", (char *)extra_info->end,
           iek_variable);
  disp_ptr("ne_call_expr", (char *)extra_info->ne_call_expr,
           iek_expr_node);
  disp_ptr("incr_call_expr", (char *)extra_info->incr_call_expr,
           iek_expr_node);
  if (extra_info->use_await) {
    disp_boolean("use_await", TRUE);
  }  /* if */
}  /* disp_range_based_for_statement */


static void disp_constexpr_if_statement(a_statement_ptr ptr)
/*
Display a C++17 constexpr if statement.
*/
{
  a_constexpr_if_ptr cip = ptr->variant.constexpr_if;

  (void)fprintf(f_display, "stmk_constexpr_if\n");
  disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
  disp_ptr("then_statement", (char *)cip->then_statement, iek_statement);
  disp_ptr("else_statement", (char *)cip->else_statement, iek_statement);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (cip->else_statement != NULL) {
    disp_source_position("else_position", &cip->else_position);
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* disp_constexpr_if_statement */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void disp_for_each_statement(a_statement_ptr ptr)
/*
Display a for-each statement.
*/
{
  a_for_each_loop_ptr extra_info = ptr->variant.for_each_loop.extra_info;

  (void)fprintf(f_display, "stmk_for_each\n");
  disp_ptr("statement",
           (char *)ptr->variant.for_each_loop.statement,
           iek_statement);
  disp_boolean("uses_prev_decl_iterator",
               (a_boolean)extra_info->uses_prev_decl_iterator);
  if (!extra_info->uses_prev_decl_iterator) {
    disp_ptr("iterator.variable", (char *)extra_info->iterator.variable,
             iek_variable);
  } else {
    disp_ptr("iterator.prev_decl.variable",
             (char *)extra_info->iterator.prev_decl.variable,
             iek_variable);
    disp_ptr("iterator.prev_decl.field",
             (char *)extra_info->iterator.prev_decl.field,
             iek_field);
    disp_ptr("iterator.prev_decl.assign_expr",
             (char *)extra_info->iterator.prev_decl.assign_expr,
             iek_expr_node);
  }  /* if */
  disp_ptr("collection_expr_ref", (char *)extra_info->collection_expr_ref,
           iek_variable);
  disp_ptr("for_each_scope", (char *)extra_info->for_each_scope,
           iek_scope);
  disp_ptr("iterator_scope", (char *)extra_info->iterator_scope,
           iek_scope);
  disp_ptr("temporary_variable", (char *)extra_info->temporary_variable,
           iek_variable);
  disp_name("for-each pattern kind");
  switch (extra_info->kind) {
    case sfepk_none:
      (void)fprintf(f_display, "sfepk_none\n");
      break;
    case sfepk_stl_pattern:
      (void)fprintf(f_display, "sfepk_stl_pattern\n");
      goto stl_array_pattern;
    case sfepk_array_pattern:
      (void)fprintf(f_display, "sfepk_array_pattern\n");
stl_array_pattern:
      disp_ptr("end_variable",
               (char *)extra_info->variant.stl_array_pattern.end_variable,
               iek_variable);
      disp_ptr("ne_call_expr",
               (char *)extra_info->variant.stl_array_pattern.ne_call_expr,
               iek_expr_node);
      disp_ptr("incr_call_expr",
               (char *)extra_info->variant.stl_array_pattern.incr_call_expr,
               iek_expr_node);
      break;
    case sfepk_cli_pattern:
      (void)fprintf(f_display, "sfepk_cli_pattern\n");
      disp_ptr("movenext_call_expression",
               (char *)extra_info->
                               variant.cli_pattern.movenext_call_expression,
               iek_expr_node);
      break;
    case sfepk_cli_array_pattern:
      (void)fprintf(f_display, "sfepk_cli_array_pattern\n");
      disp_ptr("upper_bound_vars",
               (char *)extra_info->variant.cli_array_pattern.upper_bound_vars,
               iek_variable);
      disp_ptr("loop_vars",
               (char *)extra_info->variant.cli_array_pattern.loop_vars,
               iek_variable);
      break;
    default:
      (void)fprintf(f_display, "**BAD FOR EACH KIND**\n");
      break;
  }  /* switch */
}  /* disp_for_each_statement */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void disp_statement(a_statement_ptr ptr)
/*
Display the indicated statement.
*/
{
  disp_source_position("position", &ptr->position);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_source_position("end_position", &ptr->end_position);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  disp_ptr("next", (char *)ptr->next, iek_statement);
  disp_ptr("parent", (char *)ptr->parent, iek_statement);
  disp_ptr("attributes", (char *)ptr->attributes, iek_attribute);
  if (ptr->has_associated_pragma) {
    disp_boolean("has_associated_pragma", TRUE);
  }  /* if */
  if (ptr->is_initialization_guard) {
    disp_boolean("is_initialization_guard", TRUE);
  }  /* if */
  if (ptr->compiler_generated) {
    disp_boolean("compiler_generated", TRUE);
  }  /* if */
#if DO_IL_LOWERING
  if (ptr->lowering_generated) {
    disp_boolean("lowering_generated", TRUE);
  }  /* if */
  if (ptr->is_lowering_boilerplate) {
    disp_boolean("is_lowering_boilerplate", TRUE);
  }  /* if */
#endif /* DO_IL_LOWERING */
  if (ptr->is_fallthrough_statement) {
    disp_boolean("is_fallthrough_statement", TRUE);
  }  /* if */
  if (ptr->is_likely) {
    disp_boolean("is_likely", TRUE);
  }  /* if */
  if (ptr->is_unlikely) {
    disp_boolean("is_unlikely", TRUE);
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (ptr->source_sequence_entry != NULL) {
    disp_ptr("source_sequence_entry", (char *)ptr->source_sequence_entry,
             iek_source_sequence_entry);
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  disp_name("kind");
  switch (ptr->kind) {
    case stmk_empty:
      (void)fprintf(f_display, "stmk_empty\n");
      break;
    case stmk_expr:
      (void)fprintf(f_display, "stmk_expr\n");
      disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
      break;
    case stmk_return:
      (void)fprintf(f_display, "stmk_return\n");
      disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
      if (ptr->variant.return_dynamic_init != NULL) {
        disp_ptr("return_dynamic_init",
                 (char *)ptr->variant.return_dynamic_init, iek_dynamic_init);
      }  /* if */
      break;
    case stmk_coroutine_return:
      (void)fprintf(f_display, "stmk_coroutine_return\n");
      disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
      break;
    case stmk_coroutine:
      (void)fprintf(f_display, "stmk_coroutine\n");
      if (ptr->variant.coroutine.descr != NULL) {
        disp_coroutine_descr(ptr->variant.coroutine.descr);
      }  /* if */
      break;
    case stmk_if:
    case stmk_if_consteval:
    case stmk_if_not_consteval:
      (void)fprintf(f_display, 
          ptr->kind == (a_statement_kind)stmk_if ? "stmk_if\n" :
          ptr->kind == (a_statement_kind)stmk_if_consteval ?
                                                   "stmk_if_consteval\n"
                                                 : "stmk_if_not_consteval\n");
      disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
      disp_ptr("then_statement", (char *)ptr->variant.if_stmt.then_statement,
               iek_statement);
      disp_ptr("else_statement", (char *)ptr->variant.if_stmt.else_statement,
               iek_statement);
      if (ptr->variant.if_stmt.else_statement == NULL) {
#if EXTRA_SOURCE_POSITIONS_IN_IL
      } else {
        disp_source_position("else_position",
                             &ptr->variant.if_stmt.else_position);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      }  /* if */
      break;
    case stmk_constexpr_if:
      disp_constexpr_if_statement(ptr);
      break;
    case stmk_while:
      (void)fprintf(f_display, "stmk_while\n");
      goto do_loop;
    case stmk_end_test_while:
      (void)fprintf(f_display, "stmk_end_test_while\n");
do_loop:
      disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
      disp_ptr("loop_statement", (char *)ptr->variant.loop_statement,
               iek_statement);
      break;
    case stmk_goto:
      (void)fprintf(f_display, "stmk_goto\n");
      goto do_label;
    case stmk_label:
      (void)fprintf(f_display, "stmk_label\n");
do_label:
      disp_ptr("label", (char *)ptr->variant.label.ptr, iek_label);
      disp_ptr("lifetime", (char *)ptr->variant.label.lifetime,
               iek_object_lifetime);
      break;
    case stmk_block:
      (void)fprintf(f_display, "stmk_block\n");
      disp_ptr("statements", (char *)ptr->variant.block.statements,
               iek_statement);
      disp_block(ptr->variant.block.extra_info);
      break;
#if UPC_EXTENSIONS_ALLOWED
    case stmk_upc_notify:
      (void)fprintf(f_display, "stmk_upc_notify\n");
      disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
      break;
    case stmk_upc_wait:
      (void)fprintf(f_display, "stmk_upc_wait\n");
      disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
      break;
    case stmk_upc_barrier:
      (void)fprintf(f_display, "stmk_upc_barrier\n");
      disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
      break;
    case stmk_upc_fence:
      (void)fprintf(f_display, "stmk_upc_fence\n");
      break;
#endif /* UPC_EXTENSIONS_ALLOWED */
    case stmk_for:
#if UPC_EXTENSIONS_ALLOWED
    case stmk_upc_forall:
      if (ptr->kind == stmk_upc_forall) {
        (void)fprintf(f_display, "stmk_upc_forall\n");
      } else
#endif /* UPC_EXTENSIONS_ALLOWED */
      /* Do not insert code here. */
      {
        (void)fprintf(f_display, "stmk_for\n");
      }  /* if */
      disp_ptr("initialization",
               (char *)ptr->variant.for_loop.extra_info->initialization,
               iek_statement);
      disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
      disp_ptr("statement", (char *)ptr->variant.for_loop.statement,
               iek_statement);
      disp_ptr("increment",
               (char *)ptr->variant.for_loop.extra_info->increment,
               iek_expr_node);
#if UPC_EXTENSIONS_ALLOWED
      if (ptr->kind == stmk_upc_forall) {
        disp_ptr("affinity",
                 (char *)ptr->variant.for_loop.extra_info->affinity,
                 iek_expr_node);
      }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
      if (ptr->variant.for_loop.extra_info->for_init_scope != NULL) {
        disp_ptr("for_init_scope",
                 (char *)ptr->variant.for_loop.extra_info->for_init_scope,
                 iek_scope);
      }  /* if */
      break;
    case stmk_range_based_for:
      disp_range_based_for_statement(ptr);
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case stmk_for_each:
      disp_for_each_statement(ptr);
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case stmk_switch_case:
      (void)fprintf(f_display, "stmk_switch_case\n");
      disp_ptr("switch_statement",
               (char *)ptr->variant.switch_case.switch_statement,
               iek_statement);
      disp_ptr("extra_info", (char *)ptr->variant.switch_case.extra_info,
               iek_switch_case_entry);
      break;
    case stmk_switch:
      (void)fprintf(f_display, "stmk_switch\n");
      disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
      disp_ptr("body_statement",
               (char *)ptr->variant.switch_stmt.body_statement, iek_statement);
      disp_ptr("extra_info", (char *)ptr->variant.switch_stmt.extra_info,
               iek_switch_stmt_descr);
      break;
    case stmk_init:
      (void)fprintf(f_display, "stmk_init\n");
      disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
      disp_ptr("dynamic_init", (char *)ptr->variant.dynamic_init,
               iek_dynamic_init);
      break;
    case stmk_asm:
      /* Asm statement. */
      (void)fprintf(f_display, "stmk_asm\n");
      disp_ptr("asm_entry", (char *)ptr->variant.asm_entry, iek_asm_entry);
      break;
#if ASM_FUNCTION_ALLOWED
    case stmk_asm_func_body:
      /* Statement representing an function body. */
      (void)fprintf(f_display, "stmk_asm_func_body\n");
      disp_string_ptr("asm_func_body", ptr->variant.asm_func_body,
                      iek_other_text, (sizeof_t)0);
      break;
#endif /* ASM_FUNCTION_ALLOWED */
    case stmk_try_block:
      /* Try block. */
      (void)fprintf(f_display, "stmk_try_block\n");
      disp_try_supplement(ptr->variant.try_block);
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case stmk_microsoft_try:
      /* Try block. */
      (void)fprintf(f_display, "stmk_microsoft_try\n");
      disp_microsoft_try_supplement(ptr->variant.microsoft_try);
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case stmk_decl:
      /* "Decl" pseudo-statement. */
      (void)fprintf(f_display, "stmk_decl\n");
      disp_entity_list("entities", ptr->variant.decl.entities);
      if (ptr->variant.decl.has_static_or_thread_variable) {
        disp_boolean("decl.has_static_or_thread_variable", TRUE);
      }  /* if */
      break;
    case stmk_set_vla_size:
      (void)fprintf(f_display, "stmk_set_vla_size\n");
      disp_ptr("vla_dimension", (char *)ptr->variant.vla_dimension,
               iek_vla_dimension);
      break;
    case stmk_vla_decl:
      (void)fprintf(f_display, "stmk_vla_decl\n");
      if (ptr->variant.vla.is_typedef_decl) {
        disp_boolean("vla.is_typedef_decl", TRUE);
        disp_ptr("vla.typedef_type",
                 (char *)ptr->variant.vla.variant.typedef_type, iek_type);
      } else {
        disp_boolean("vla.is_typedef_decl", FALSE);
        disp_ptr("vla.variable", (char *)ptr->variant.vla.variant.variable,
                 iek_variable);
      }  /* if */
      break;
#if GNU_EXTENSIONS_ALLOWED
    case stmk_assigned_goto:
      (void)fprintf(f_display, "stmk_assigned_goto\n");
      disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
      break;
#endif /* GNU_EXTENSIONS_ALLOWED */
    case stmk_stmt_expr_result:
      (void)fprintf(f_display, "stmk_stmt_expr_result\n");
      if (ptr->variant.stmt_expr_result.dynamic_init != NULL) {
        disp_ptr("dynamic_init",
                 (char *)ptr->variant.stmt_expr_result.dynamic_init,
                 iek_dynamic_init);
      } else {
        disp_ptr("expr", (char *)ptr->expr, iek_expr_node);
      }  /* if */
      break;
    default:
      (void)fprintf(f_display, "**BAD STATEMENT KIND**\n");
  }  /* switch */
}  /* disp_statement */


static void disp_pragma(a_pragma_ptr ptr)
/*
Display the indicated pragma entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_pragma);
  disp_ptr("entity", (char *)ptr->entity.ptr,
           (an_il_entry_kind)ptr->entity.kind);
  disp_source_position("position", &ptr->position);
#if GENERATE_SOURCE_SEQUENCE_LISTS
  disp_ptr("source_sequence_entry", (char *)ptr->source_sequence_entry,
           iek_source_sequence_entry);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  disp_string_ptr("pragma_text", ptr->pragma_text, iek_other_text,
                  (sizeof_t)0);
  if (ptr->ignore_in_back_end) disp_boolean("ignore_in_back_end", TRUE);
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ptr->is_microsoft_pragma_operator) {
    disp_boolean("is_microsoft_pragma_operator", TRUE);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  disp_name("kind");
  disp_pragma_kind_name(ptr->kind);
#if IDENT_DIRECTIVE_AND_PRAGMA
  if (ptr->kind == (a_pragma_kind)pk_ident_directive) {
    disp_constant(ptr->variant.ident_string);
  }  /* if */
#endif /* IDENT_DIRECTIVE_AND_PRAGMA */
#if BACK_END_IS_CP_GEN_BE
  if (ptr->kind == (a_pragma_kind)pk_pack) {
    disp_unsigned_long("alignment", (unsigned long)ptr->variant.alignment);
  }  /* if */
#endif /* BACK_END_IS_CP_GEN_BE */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ptr->kind == (a_pragma_kind)pk_comment) {
    disp_name("comment.kind");
    (void)fprintf(f_display, "%s\n",
                 microsoft_pragma_comment_ids[(int)ptr->variant.comment.kind]);
    if (ptr->variant.comment.str != NULL) {
      disp_ptr("comment.str", (char *)ptr->variant.comment.str, iek_constant);
    }  /* if */
  } else if (ptr->kind == (a_pragma_kind)pk_conform) {
    disp_name("conform.kind");
    if (ptr->variant.conform.kind !=
                             (a_microsoft_pragma_conform_kind)mpck_forScope) {
      (void)fprintf(f_display, "**BAD KIND**\n");
    } else {
      (void)fprintf(f_display, "mpck_forScope\n");
      if (ptr->variant.conform.on) {
        disp_boolean("conform.on", TRUE);
      }  /* if */
      if (ptr->variant.conform.off) {
        disp_boolean("conform.off", TRUE);
      }  /* if */
      if (ptr->variant.conform.show) {
        disp_boolean("conform.show", TRUE);
      }  /* if */
      if (ptr->variant.conform.push) {
        disp_boolean("conform.push", TRUE);
      }  /* if */
      if (ptr->variant.conform.pop) {
        disp_boolean("conform.pop", TRUE);
      }  /* if */
      if (ptr->variant.conform.identifier != NULL) {
        disp_string_ptr("conform.identifier", ptr->variant.conform.identifier,
                        iek_other_text, (sizeof_t)0);
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* disp_pragma */

#if RECORD_HIDDEN_NAMES_IN_IL

static void disp_hidden_name(a_hidden_name_ptr  ptr)
/*
Display the indicated hidden-name entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_hidden_name);
  disp_ptr("entity", (char *)ptr->entity.ptr,
           (an_il_entry_kind)ptr->entity.kind);
  disp_boolean("qualification_needed",
               (a_boolean)ptr->qualification_needed);
  disp_boolean("elaborated_type_specifier_needed",
               (a_boolean)ptr->elaborated_type_specifier_needed);
  disp_boolean("partially_hidden_by_microsoft_injected_class_name",
               (a_boolean)ptr->
               partially_hidden_by_microsoft_injected_class_name);
  disp_boolean("is_class_member", (a_boolean)ptr->is_class_member);
  disp_boolean("hidden_by_simulated_injected_class_name",
               (a_boolean)ptr->hidden_by_simulated_injected_class_name);
  disp_boolean("hidden_by_class_name", (a_boolean)ptr->hidden_by_class_name);
  disp_boolean("hidden_by_template_parameter",
               (a_boolean)ptr->hidden_by_template_parameter);
}  /* disp_hidden_name */

#endif /* RECORD_HIDDEN_NAMES_IN_IL */


static void disp_template_parameter(a_template_parameter_ptr  ptr)
/*
Display the indicated template parameter.
*/
{
  disp_source_corresp(&ptr->source_corresp, iek_template_parameter);
  if (ptr->next != NULL) {
    disp_ptr("next", (char*)ptr->next, iek_template_parameter);
  }  /* if */
  if (ptr->is_pack) {
    disp_boolean("is_pack", TRUE);
  }  /* if */
  if (ptr->is_abbreviated) {
    disp_boolean("is_abbreviated", TRUE);
  }  /* if */
  disp_name("kind");
  switch (ptr->kind) {
    case tpk_error:
      (void)fprintf(f_display, "tpk_error\n");
      break;
    case tpk_type:
      (void)fprintf(f_display, "tpk_type\n");
      disp_ptr("ptr", (char*)ptr->variant.type.ptr, iek_type);
      if (ptr->variant.type.default_arg_type != NULL) {
        disp_ptr("default_arg_type",
                 (char*)ptr->variant.type.default_arg_type, iek_type);
      }  /* if */
      break;
    case tpk_nontype:
      (void)fprintf(f_display, "tpk_nontype\n");
      disp_ptr("constant", (char*)ptr->variant.nontype.constant, iek_constant);
      if (ptr->variant.nontype.default_arg_constant != NULL) {
        disp_ptr("default_arg_constant",
                 (char*)ptr->variant.nontype.default_arg_constant,
                 iek_constant);
      }  /* if */
      break;
    case tpk_template:
      (void)fprintf(f_display, "tpk_template\n");
      disp_ptr("class_template", (char*)ptr->variant.templ.class_template,
               iek_template);
      if (ptr->variant.templ.default_arg_template) {
        disp_ptr("default_arg_template",
                 (char*)ptr->variant.templ.default_arg_template, iek_template);
      }  /* if */
      break;
    default:
      (void)fprintf(f_display, "**BAD KIND**\n");
  }  /* switch */
}  /* disp_template_parameter */


static void disp_template_decl(a_template_decl_ptr  ptr)
/*
Display the indicated template declaration information.
*/
{
  if (ptr->parent != NULL) {
    disp_ptr("parent", (char*)ptr->parent, iek_template_decl);
  }  /* if */
  disp_ptr("param_list", (char*)ptr->param_list, iek_template_parameter);
  if (ptr->scope != NULL) {
     disp_ptr("scope", (char*)ptr->scope, iek_scope);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ptr->is_generic) {
    disp_boolean("is_generic", TRUE);
    disp_ptr("constraint.where_clauses", (char *)ptr->constraint.where_clauses,
             iek_generic_constraint_clause);
  } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Do not insert code here. */
  {
    if (ptr->constraint.requires_clause != NULL) {
      disp_ptr("constraint.requires_clause",
               (char *)ptr->constraint.requires_clause, iek_requires_clause);
    }  /* if */
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_source_position("template_pos", &ptr->template_pos);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* disp_template_decl */


static void disp_requires_clause(a_requires_clause_ptr  ptr)
/*
Display the indicated requires clause entry.
*/
{
  disp_ptr("constraint", (char*)ptr->constraint, iek_expr_node);
  disp_source_position("requires_pos", &ptr->requires_pos);
}  /* disp_requires_clause */


static void disp_template(a_template_ptr  ptr)
/*
Display the indicated template.
*/
{
  disp_source_corresp(&ptr->source_corresp, iek_template);
  disp_ptr("next", (char *)ptr->next, iek_template);
  disp_name("kind");
  switch (ptr->kind) {
    case templk_none:
      (void)fprintf(f_display, "templk_none\n");
      break;
    case templk_class:
      (void)fprintf(f_display, "templk_class\n");
      break;
    case templk_function:
      (void)fprintf(f_display, "templk_function\n");
      break;
    case templk_variable:
      (void)fprintf(f_display, "templk_variable\n");
      break;
    case templk_member_function:
      (void)fprintf(f_display, "templk_member_function\n");
      break;
    case templk_static_data_member:
      (void)fprintf(f_display, "templk_static_data_member\n");
      break;
    case templk_member_class:
      (void)fprintf(f_display, "templk_member_class\n");
      break;
    case templk_member_enum:
      (void)fprintf(f_display, "templk_member_enum\n");
      break;
    case templk_template_template_param:
      (void)fprintf(f_display, "templk_template_template_param\n");
      disp_template_param_coordinate(&ptr->coordinates);
      break;
    case templk_concept:
      (void)fprintf(f_display, "templk_concept\n");
      break;
    default:
      (void)fprintf(f_display, "**BAD TEMPLATE KIND**\n");
  }  /* switch */
  if (ptr->is_exported) {
    disp_boolean("is_exported", (a_boolean)ptr->is_exported);
  }  /* if */
  if (ptr->ignore_export) {
    disp_boolean("ignore_export", (a_boolean)ptr->ignore_export);
  }  /* if */
  if (ptr->is_pack) {
    disp_boolean("is_pack", TRUE);
  }  /* if */
  if (ptr->is_friend_template) {
    disp_boolean("is_friend_template", TRUE);
  }  /* if */
  if (ptr->template_decl != NULL) {
    disp_ptr("template_decl", (char *)ptr->template_decl, iek_template_decl);
  }  /* if */
  switch (ptr->kind) {
    case templk_class:
    case templk_member_class:
      disp_ptr("type", (char *)ptr->prototype_instantiation.type, iek_type);
      break;
    case templk_function:
    case templk_member_function:
      disp_ptr("routine", (char *)ptr->prototype_instantiation.routine,
               iek_routine);
      break;
    case templk_static_data_member:
    case templk_variable:
      disp_ptr("variable", (char *)ptr->prototype_instantiation.variable,
               iek_variable);
      break;
    case templk_concept:
      disp_ptr("constraint", (char *)ptr->prototype_instantiation.constraint,
               iek_expr_node);
      break;
    default:
      break;
  }  /* switch */
  if (ptr->canonical_template != NULL) {
     disp_ptr("canonical_template", (char*)ptr->canonical_template,
              iek_template);
  }  /* if */
  if (ptr->definition_template != NULL) {
     disp_ptr("definition_template", (char*)ptr->definition_template,
              iek_template);
  }  /* if */
  if (ptr->prototype_template != NULL) {
     disp_ptr("prototype_template", (char*)ptr->prototype_template,
              iek_template);
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_source_position("export_position", &ptr->export_position);
  disp_source_range("definition_range", &ptr->definition_range);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if RECORD_TEMPLATE_STRINGS
  disp_string_ptr("text", ptr->text, iek_other_text, (sizeof_t)0);
#endif /* RECORD_TEMPLATE_STRINGS */
#if BACK_END_IS_CP_GEN_BE
  disp_unsigned_long("final_alignment", (unsigned long)ptr->final_alignment);
  if (ptr->min_template_arguments >= 0) {
    disp_int32("min_template_arguments", ptr->min_template_arguments);
  }  /* if */
#endif /* BACK_END_IS_CP_GEN_BE */
}  /* disp_template */


static void disp_lambda(a_lambda_ptr ptr)
/*
Display the indicated lambda entry.
*/
{
  disp_ptr("capture_list", (char*)ptr->capture_list, iek_lambda_capture);
  disp_ptr("closure_class", (char*)ptr->closure_class, iek_type);
  disp_ptr("lambda_routine", (char*)ptr->lambda_routine, iek_routine);
  if (ptr->is_generic) {
    disp_boolean("is_generic", TRUE);
  }  /* if */
  if (ptr->is_mutable) {
    disp_boolean("is_mutable", TRUE);
  }  /* if */
  if (ptr->constexpr_specified) {
    disp_boolean("constexpr_specified", TRUE);
  }  /* if */
  if (ptr->has_capture_default) {
    disp_boolean("has_capture_default", TRUE);
  }  /* if */
  if (ptr->default_is_by_reference) {
    disp_boolean("default_is_by_reference", TRUE);
  }  /* if */
  if (ptr->explicit_return_type) {
    disp_boolean("explicit_return_type", TRUE);
  }  /* if */
  if (ptr->has_parameter_decl) {
    disp_boolean("has_parameter_decl", TRUE);
  }  /* if */
  if (ptr->has_template_param_list) {
    disp_boolean("has_template_param_list", TRUE);
  }  /* if */
  disp_source_position("start_position", &ptr->start_position);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_source_position("capture_end_position", &ptr->capture_end_position);
  disp_source_position("mutable_position", &ptr->mutable_position);
  disp_source_position("constexpr_position", &ptr->constexpr_position);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* disp_lambda */


static void disp_lambda_capture(a_lambda_capture_ptr ptr)
/*
Display the indicated lambda capture.
*/
{
  disp_ptr("next", (char*)ptr->next, iek_lambda_capture);
  if (!ptr->is_init_capture) {
    if (ptr->is_indirect_init_capture) {
      disp_boolean("is_indirect_init_capture", TRUE);
      disp_ptr("captured.init_capture_field",
               (char*)ptr->captured.init_capture_field,
               iek_field);
    } else {
      disp_ptr("captured.variable", (char*)ptr->captured.variable,
               iek_variable);
    }
    disp_ptr("source_closure_field",
             (char*)ptr->capture_info.source_closure_field, iek_field);
  } else {
    disp_boolean("is_init_capture", TRUE);
    disp_ptr("captured.initializer", (char*)ptr->captured.initializer,
             iek_dynamic_init);
  }  /* if */
  disp_ptr("closure_field", (char*)ptr->closure_field, iek_field);
  if (ptr->is_param_ref_capture) {
    disp_boolean("is_param_ref_capture", TRUE);
  }  /* if */
  if (ptr->capture_by_reference) {
    disp_boolean("capture_by_reference", TRUE);
  }  /* if */
  if (ptr->is_implicit) {
    disp_boolean("is_implicit", TRUE);
  }  /* if */
  if (ptr->is_pack_expansion) {
    disp_boolean("is_pack_expansion", TRUE);
  }  /* if */
  if (ptr->is_pack_element) {
    disp_boolean("is_pack_element", TRUE);
  }  /* if */
  if (ptr->direct_init) {
    disp_boolean("direct_init", TRUE);
  }  /* if */
  if (ptr->parenthesized_init) {
    disp_boolean("parenthesized_init", TRUE);
  }  /* if */
  disp_source_position("position", &ptr->position);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_source_position("end_position", &ptr->end_position);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* disp_lambda_capture */


static void disp_attribute(an_attribute_ptr  ap)
/*
Display the indicated attribute entry.
*/
{
  a_const_char *kind_name = "<unknown>",
               *family_name = "<unknown>",
               *loc_name = "<unknown>";

  switch (ap->kind) {
    case ak_unrecognized:        kind_name = "unrecognized";        break;
    case ak_empty_attr:          kind_name = "empty attribute";     break;
    case ak_attr_using_prefix:   kind_name = "\"using\" prefix";    break;
    /* Standard attributes: */
    case ak_align:               kind_name = "align";               break;
    case ak_assume:              kind_name = "assume";              break;
    case ak_base_check:          kind_name = "base_check";          break;
    case ak_carries_dependency:  kind_name = "carries_dependency";  break;
    case ak_deprecated:          kind_name = "deprecated";          break;
    case ak_final:               kind_name = "final";               break;
    case ak_hiding:              kind_name = "hiding";              break;
    case ak_known_semantics:     kind_name = "known_semantics";     break;
    case ak_noreturn:            kind_name = "noreturn";            break;
    case ak_override:            kind_name = "override";            break;
    case ak_nodiscard:           kind_name = "nodiscard";           break;
    case ak_maybe_unused:        kind_name = "maybe_unused";        break;
    case ak_fallthrough:         kind_name = "fallthrough";         break;
    case ak_likely:              kind_name = "likely";              break;
    case ak_unlikely:            kind_name = "unlikely";            break;
    case ak_no_unique_address:   kind_name = "no_unique_address";   break;
    case ak_indeterminate:       kind_name = "indeterminate";       break;
    case ak_enable_if:           kind_name = "enable_if";           break;
    case ak_overloadable:        kind_name = "overloadable";        break;
    case ak_pass_object_size:    kind_name = "pass_object_size";    break;
    case ak_diagnose_if:         kind_name = "diagnose_if";         break;
    case ak_unavailable:         kind_name = "unavailable";         break;
#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
    /* Nonstandard attributes available in both GNU and Microsoft
       configurations. */
#if GNU_NAKED_ATTRIBUTE_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
    case ak_naked:               kind_name = "naked";               break;
#endif /* GNU_NAKED_ATTRIBUTE_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
    case ak_noinline:            kind_name = "noinline";            break;
    case ak_nothrow:             kind_name = "nothrow";             break;
    case ak_section:             kind_name = "section";             break;
#endif /* GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
    /* GNU-only attributes. */
    case ak_alias:               kind_name = "alias";               break;
    case ak_alloc_size:          kind_name = "alloc_size";          break;
    case ak_always_inline:       kind_name = "always_inline";       break;
    case ak_artificial:          kind_name = "artificial";          break;
#if GNU_X86_ATTRIBUTES_ALLOWED
    case ak_cdecl:               kind_name = "cdecl";               break;
#endif /* GNU_X86_ATTRIBUTES_ALLOWED */
    case ak_cleanup:             kind_name = "cleanup";             break;
    case ak_cold:                kind_name = "cold";                break;
    case ak_common:              kind_name = "common";              break;
    case ak_const:               kind_name = "const";               break;
    case ak_constructor:         kind_name = "constructor";         break;
    case ak_destructor:          kind_name = "destructor";          break;
    case ak_error:               kind_name = "error";               break;
#if GNU_VECTOR_TYPES_ALLOWED
    case ak_ext_vector_type:     kind_name = "ext_vector_type";     break;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
    case ak_externally_visible:  kind_name = "externally_visible";  break;
#if GNU_X86_ATTRIBUTES_ALLOWED
    case ak_fastcall:            kind_name = "fastcall";            break;
#endif /* GNU_X86_ATTRIBUTES_ALLOWED */
    case ak_flatten:             kind_name = "flatten";             break;
    case ak_format:              kind_name = "format";              break;
    case ak_format_arg:          kind_name = "format_arg";          break;
    case ak_gnu_inline:          kind_name = "gnu_inline";          break;
    case ak_hot:                 kind_name = "hot";                 break;
    case ak_ifunc:               kind_name = "ifunc";               break;
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
    case ak_init_priority:       kind_name = "init_priority";       break;
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
    case ak_internal_linkage:    kind_name = "internal_linkage";    break;
    case ak_malloc:              kind_name = "malloc";              break;
    case ak_may_alias:           kind_name = "may_alias";           break;
    case ak_mode:                kind_name = "mode";                break;
    case ak_no_instrument_function:
                                 kind_name = "no_instrument_function";
                                                                    break;
    case ak_no_check_memory_usage:
                                 kind_name = "no_check_memory_usage";
                                                                    break;
    case ak_nocommon:            kind_name = "nocommon";            break;
    case ak_nonnull:             kind_name = "nonnull";             break;
    case ak_noop_dtor:           kind_name = "noop_dtor";           break;
    case ak_noplt:               kind_name = "noplt";               break;
    case ak_packed:              kind_name = "packed";              break;
    case ak_pure:                kind_name = "pure";                break;
    case ak_sentinel:            kind_name = "sentinel";            break;
#if GNU_X86_ATTRIBUTES_ALLOWED
    case ak_stdcall:             kind_name = "stdcall";             break;
#endif /* GNU_X86_ATTRIBUTES_ALLOWED */
    case ak_strong:              kind_name = "strong";              break;
    case ak_target:              kind_name = "target";              break;
#if THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
    case ak_tls_model:           kind_name = "tls_model";           break;
#endif /* THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED */
    case ak_transparent_union:   kind_name = "transparent_union";   break;
    case ak_unused:              kind_name = "unused";              break;
    case ak_used:                kind_name = "used";                break;
#if GNU_VECTOR_TYPES_ALLOWED
    case ak_vector_size:         kind_name = "vector_size";         break;
    case ak_neon_vector_type:    kind_name = "neon_vector_type";    break;
    case ak_neon_polyvector_type:kind_name = "neon_polyvector_type";break;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
    case ak_visibility:          kind_name = "visibility";          break;
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
    case ak_warn_unused_result:  kind_name = "warn_unused_result";  break;
    case ak_warning:             kind_name = "warning";             break;
    case ak_weak:                kind_name = "weak";                break;
    case ak_weakref:             kind_name = "weakref";             break;
    case ak_abi_tag:             kind_name = "abi_tag";             break;
    case ak_no_specializations:  kind_name = "no_specializations";  break;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* Microsoft-__declspec-only attributes. */
    case ak_appdomain:           kind_name = "appdomain";           break;
    case ak_assembly_info:       kind_name = "assembly_info";       break;
    case ak_dllexport:           kind_name = "dllexport";           break;
    case ak_dllimport:           kind_name = "dllimport";           break;
    case ak_edg_interior_ptr_alias:
                                 kind_name = "edg_interior_ptr_alias";
                                                                    break;
    case ak_edg_pin_ptr_alias:   kind_name = "edg_pin_ptr_alias";   break;
    case ak_empty_bases:         kind_name = "empty_bases";         break;
    case ak_guard:               kind_name = "guard";               break;
    case ak_hybrid_patchable:    kind_name = "hybrid_patchable";    break;
    case ak_implementation_key:  kind_name = "implementation_key";  break;
    case ak_intrin_type:         kind_name = "intrin_type";         break;
    case ak_jitintrinsic:        kind_name = "jitintrinsic";        break;
    case ak_no_init_all:         kind_name = "no_init_all";         break;
    case ak_noalias:             kind_name = "noalias";             break;
    case ak_non_user_code:       kind_name = "non_user_code";       break;
    case ak_novtable:            kind_name = "novtable";            break;
    case ak_process:             kind_name = "process";             break;
    case ak_property:            kind_name = "property";            break;
    case ak_restrict:            kind_name = "restrict";            break;
    case ak_safebuffers:         kind_name = "safebuffers";         break;
    case ak_selectany:           kind_name = "selectany";           break;
    case ak_spectre:             kind_name = "spectre";             break;
#if THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
    case ak_thread:              kind_name = "thread";              break;
#endif /* THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED */
    case ak_uuid:                kind_name = "uuid";                break;
    case ak_layout_as_external:  kind_name = "layout_as_external";  break;
    case ak_no_empty_identity_interface:
                                 kind_name = "no_empty_identity_interface";
                                                                    break;
    case ak_no_ftm:              kind_name = "no_ftm";              break;
    case ak_no_refcount:         kind_name = "no_refcount";         break;
    case ak_no_release_return:   kind_name = "no_release_return";   break;
    case ak_no_weakreferencesource:
                                 kind_name = "no_weakreferencesource";
                                                                    break;
    case ak_one_phase_constructed:
                                 kind_name = "one_phase_constructed";
                                                                    break;
    case ak_allocator:           kind_name = "allocator";           break;
    case ak_no_sanitize_address: kind_name = "no_sanitize_address"; break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if INCLUDE_EDG_TEST_ATTRIBUTES
  /* Attributes used for testing by EDG. */
    case ak_edg_e1:              kind_name = "edg_e1";              break;
    case ak_edg_n1:              kind_name = "edg_n1";              break;
#endif /* INCLUDE_EDG_TEST_ATTRIBUTES */
    case ak_availability:        kind_name = "availability";        break;
    case ak_using_if_exists:     kind_name = "using_if_exists";     break;
    case ak_exclude_from_explicit_instantiation:
                 kind_name = "exclude_from_explicit_instantiation"; break;
    case ak_annotation:          kind_name = "annotation";          break;
    case ak_conditional_explicit:kind_name = "conditional_explicit";break;
    case ak_pragma_pack_state:   kind_name = "pragma_pack_state";   break;
    case ak_last:                kind_name = "<last>";              break;
    default_is_unexpected();
  }  /* switch */
  disp_name("kind");
  (void)fprintf(f_display, "%s\n", kind_name);
  disp_ptr("next", (char *)ap->next, iek_attribute);
  switch (ap->family) {
    case af_internal:            family_name = "internal";          break;
    case af_std:                 family_name = "std";               break;
    case af_gnu:                 family_name = "gnu";               break;
    case af_ms_declspec:         family_name = "ms_declspec";       break;
    case af_alignas:             family_name = "alignas";           break;
    case af_has_attribute:       family_name = "has_attribute";     break;
    case af_last:                family_name = "<last>";            break;
    default_is_unexpected();
  }  /* switch */
  disp_name("family");
  (void)fprintf(f_display, "%s\n", family_name);
  switch (ap->syntactic_location) {
    case al_implicit:            loc_name = "implicit";             break;
    case al_prefix:              loc_name = "prefix";               break;
    case al_tag_name:            loc_name = "tag name";             break;
    case al_post_tag_definition: loc_name = "post tag definition";  break;
    case al_base_specifier:      loc_name = "base specifier";       break;
    case al_specifier:           loc_name = "specifier";            break;
    case al_declarator_id:       loc_name = "declarator-id";        break;
    case al_post_ptr_or_ref:     loc_name = "post ptr or ref";      break;
    case al_post_array:          loc_name = "post array";           break;
    case al_post_func:           loc_name = "post func";            break;
    case al_postfix:             loc_name = "postfix";              break;
    case al_predeclarator:       loc_name = "predeclarator";        break;
    case al_id_equivalent:       loc_name = "id_equivalent";        break;
    case al_trailing_return:     loc_name = "trailing return";      break;
    case al_post_initializer:    loc_name = "post initializer";     break;
    case al_namespace:           loc_name = "namespace";            break;
    case al_gnu_namespace:       loc_name = "gnu_namespace";        break;
    case al_label:               loc_name = "label";                break;
    case al_explicit:            loc_name = "explicit";             break;
    case al_enumerator:          loc_name = "enumerator";           break;
    case al_id_equivalent_as_postfix:
                                 loc_name = "id_equivalent_as_postfix"; break;
    case al_builtin_has_attribute:
                                 loc_name = "builtin_has_attribute"; break;
    case al_module:              loc_name = "module";                break;
    case al_post_using_declarator:
                                 loc_name = "post_using_declarator"; break;
    case al_lambda_expression:   loc_name = "lambda_expression";     break;
    case al_last:                loc_name = "<last>";                break;
    default_is_unexpected();
  }  /* switch */
  disp_name("syntactic_location");
  (void)fprintf(f_display, "%s\n", loc_name);
  if (ap->on_primary_declaration) {
    disp_boolean("on_primary_declaration", TRUE);
  }  /* if */
  if (ap->transforms_type_specifier) {
    disp_boolean("transforms_type_specifier", TRUE);
  }  /* if */
  if (ap->applied_to_declared_type) {
    disp_boolean("applied_to_declared_type", TRUE);
  }  /* if */
  if (ap->must_be_preserved_in_trans_unit_copy) {
    disp_boolean("must_be_preserved_in_trans_unit_copy", TRUE);
  }  /* if */
  if (ap->is_pack_expansion) {
    disp_boolean("is_pack_expansion", TRUE);
  }  /* if */
  if (ap->is_std_gcc_attribute) {
    disp_boolean("is_std_gcc_attribute", TRUE);
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  if (ap->is_implicit_abi_tag_attribute) {
    disp_boolean("is_implicit_abi_tag_attribute", TRUE);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (ap->namespace_from_using) {
    disp_boolean("namespace_from_using", TRUE);
  }  /* if */
  if (ap->is_invalid_namespace) {
    disp_boolean("is_invalid_namespace", TRUE);
  }  /* if */
  disp_string_ptr("name", ap->name, iek_other_text, (sizeof_t)0);
  if (ap->namespace_name != NULL) {
    disp_string_ptr("namespace_name", ap->namespace_name, iek_other_text,
                    (sizeof_t)0);
  }  /* if */
  if (ap->arguments != NULL) {
    disp_ptr("arguments", (char*)ap->arguments, iek_attribute_arg);
  }  /* if */
  if (ap->group != NULL) {
    disp_ptr("group", (char*)ap->group, iek_attribute_group);
  }  /* if */
  disp_source_position("position", &ap->position);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_source_position("end_position", &ap->end_position);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* pack_expansion_descr is not displayed because it is front end only. */
}  /* disp_attribute */


static void disp_attribute_arg(an_attribute_arg_ptr  aap)
/*
Display the indicated attribute argument entry.
*/
{
  a_const_char *kind_name = "** BAD KIND **";

  switch (aap->kind) {
    case aak_empty:              kind_name = "empty";               break;
    case aak_raw_token:          kind_name = "raw token";           break;
    case aak_token:              kind_name = "token";               break;
    case aak_constant:           kind_name = "constant";            break;
    case aak_type:               kind_name = "type";                break;
    case aak_expression:         kind_name = "expression";          break;
    case aak_last:                                                  break;
    default_is_unexpected();
  }  /* switch */
  disp_name("kind");
  (void)fprintf(f_display, "%s\n", kind_name);
  disp_ptr("next", (char *)aap->next, iek_attribute_arg);
  if (aap->is_pack_expansion) {
    disp_boolean("is_pack_expansion", TRUE);
  }  /* if */
  if (aap->local_expr_ref) {
    disp_boolean("local_expr_ref", TRUE);
  }  /* if */
  /* pack_expansion_descr is not displayed because it is front end only. */
  disp_source_position("position", &aap->position);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_source_position("end_position", &aap->end_position);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  if (aap->token_kind != tok_error) {
    disp_name("token_kind");
    (void)fprintf(f_display, "%s\n", token_names[aap->token_kind]);
  }  /* if */
  switch (aap->kind) {
    case aak_empty:
    case aak_last:
      /* No variant field. */
      break;
    case aak_raw_token:
    case aak_token:
      disp_string_ptr("token", aap->variant.token, iek_other_text,
                      (sizeof_t)0);
      break;
    case aak_constant:
      disp_ptr("constant", (char*)aap->variant.constant, iek_constant);
      break;
    case aak_type:
      disp_ptr("type", (char*)aap->variant.type, iek_type);
      break;
    case aak_expression:
      if (aap->local_expr_ref) {
        disp_ptr("sexpr", (char*)aap->variant.sexpr, iek_scoped_expression);
      } else {
        disp_ptr("expr", (char*)aap->variant.expr, iek_expr_node);
      }  /* if */
      break;
    default_is_unexpected();
  }  /* switch */
}  /* disp_attribute_arg */


static void disp_attribute_group(an_attribute_group_ptr  agp)
/*
Display the indicated attribute group entry.
*/
{
  disp_source_position("position", &agp->position);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_source_position("end_position", &agp->end_position);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* disp_attribute_group */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void disp_ms_attribute(an_ms_attribute_ptr ptr)
/*
Display the indicated Microsoft attribute entry.
*/
{
  int arg_number = 0;

  disp_name("kind");
  switch (ptr->kind) {
    case msak_none:         (void)fprintf(f_display, "none\n");         break;
    case msak_unrecognized: (void)fprintf(f_display, "unrecognized\n"); break;
    case msak_custom:       (void)fprintf(f_display, "custom\n");       break;
    default:                (void)fprintf(f_display, "other\n");        break;
  }  /* switch */
  disp_ptr("next", (char *)ptr->next, iek_ms_attribute);
  disp_ptr("next_in_block", (char *)ptr->next_in_block, iek_ms_attribute);
  disp_ptr("entity", (char *)ptr->entity.ptr,
           (an_il_entry_kind)ptr->entity.kind);
  if (ptr->is_attribute_attribute) {
    disp_boolean("is_attribute_attribute", TRUE);
  }  /* if */
  (void)fprintf(f_display, "target: ");
  switch (ptr->target) {
    case msat_none:
      (void)fprintf(f_display, "<none>\n");
      break;
    case msat_assembly:
      (void)fprintf(f_display, "assembly\n");
      break;
    case msat_module:
      (void)fprintf(f_display, "module\n");
      break;
    case msat_class:
      (void)fprintf(f_display, "class\n");
      break;
    case msat_struct:
      (void)fprintf(f_display, "struct\n");
      break;
    case msat_union:
      (void)fprintf(f_display, "union\n");
      break;
    case msat_enum:
      (void)fprintf(f_display, "enum\n");
      break;
    case msat_constructor:
      (void)fprintf(f_display, "constructor\n");
      break;
    case msat_method:
      (void)fprintf(f_display, "method\n");
      break;
    case msat_property:
      (void)fprintf(f_display, "property\n");
      break;
    case msat_field:
      (void)fprintf(f_display, "field\n");
      break;
    case msat_event:
      (void)fprintf(f_display, "event\n");
      break;
    case msat_interface:
      (void)fprintf(f_display, "interface\n");
      break;
    case msat_parameter:
      (void)fprintf(f_display, "parameter\n");
      break;
    case msat_delegate:
      (void)fprintf(f_display, "delegate\n");
      break;
    case msat_returnvalue:
      (void)fprintf(f_display, "returnvalue\n");
      break;
    case msat_genericparameter:
      (void)fprintf(f_display, "genericparameter\n");
      break;
    case msat_typedef:
      (void)fprintf(f_display, "typedef\n");
      break;
    case msat_variable:
      (void)fprintf(f_display, "variable\n");
      break;
    case msat_routine:
      (void)fprintf(f_display, "routine\n");
      break;
    case msat_interfaceimpl:
      (void)fprintf(f_display, "interfaceimpl\n");
      break;
    default:
      unexpected_condition();
      break;
  }  /* switch */
  if (ptr->kind == (an_ms_attribute_kind)msak_custom) {
    a_custom_ms_attribute_arg_ptr named_arg;
    disp_ptr("type", (char *)ptr->variant.custom_info.type,
             iek_type);
    disp_ptr("constructor", (char *)ptr->variant.custom_info.constructor,
             iek_routine);
    disp_ptr("args", (char *)ptr->variant.custom_info.args, iek_expr_node);
    for (named_arg = ptr->variant.custom_info.named_args;
         named_arg != NULL;
         named_arg = named_arg->next) {
      disp_long("named argument", arg_number++);
      disp_ptr("field", (char *)named_arg->field, iek_field);
      disp_ptr("expression", (char *)named_arg->expression, iek_expr_node);
    }  /* for */
  } else {
    an_ms_attribute_arg_ptr arg;
    Small_string<80>        buffer;

    disp_string_ptr("name", ptr->variant.info.name, iek_other_text,
                    (sizeof_t)0);
    disp_string_ptr("string", ptr->variant.info.string, iek_other_text,
                    (sizeof_t)0);
    for (arg = ptr->variant.info.arg_list; arg != NULL; arg = arg->next) {
      buffer.reset_to("  argument ", arg_number++, " (", arg->param_name, ")");
      switch (arg->kind) {
        case msaak_integer:
          disp_host_large_integer(
                            buffer.as_temp_characters(),
                            (a_host_large_integer)arg->variant.integer_value);
          break;
        case msaak_boolean:
          disp_boolean(buffer.as_temp_characters(),
                       (a_boolean)arg->variant.bool_value);
          break;
        case msaak_string:
          disp_ptr(buffer.as_temp_characters(),
                   (char *)arg->variant.string_constant,
                   iek_constant);
          break;
        case msaak_other:
          disp_string_ptr(buffer.as_temp_characters(),
                          arg->variant.other_string, iek_other_text,
                          (sizeof_t)0);
          break;
        case msaak_uuid:
          disp_string_ptr(buffer.as_temp_characters(),
                          arg->variant.uuid_string, iek_other_text,
                          (sizeof_t)0);
          break;
        case msaak_enumeration:
          disp_host_large_integer(
                               buffer.as_temp_characters(),
                               (a_host_large_integer)arg->variant.enum_value);
          break;
        default:
          break;
      }  /* switch */
    }  /* for */
  }  /* if */
  disp_source_position("position", &ptr->position);
}  /* disp_ms_attribute */


static void disp_property_index_type(a_property_index_type_ptr  ptr)
/*
Display the indicated property index type entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_property_index_type);
  disp_ptr("type", (char *)ptr->type, iek_type);  
  disp_source_position("position", &ptr->position);
}  /* disp_property_index_type */


static void disp_property_or_event_descr(a_property_or_event_descr_ptr  ptr)
/*
Display the indicated property/event description.
*/
{
  disp_name("kind");
  switch (ptr->kind) {
    case pek_declspec_property:
      (void)fprintf(f_display, "__declspec property\n");
      break;
    case pek_cli_property:
      (void)fprintf(f_display, "C++/CLI property\n");
      break;
    case pek_cli_event:
      (void)fprintf(f_display, "C++/CLI event\n");
      break;
    default:
      (void)fprintf(f_display, "** BAD KIND **\n");
      break;
  }  /* switch */
  if (ptr->is_trivial) {
    disp_boolean("is_trivial", TRUE);
  }  /* if */
  if (ptr->is_default_indexed) {
    disp_boolean("is_default_indexed", TRUE);
  }  /* if */
  if (ptr->is_virtual) {
    disp_boolean("is_virtual", TRUE);
  }  /* if */
  if (ptr->is_static) {
    disp_boolean("is_static", TRUE);
  }  /* if */
  disp_ptr("indices", (char*)ptr->indices, iek_property_index_type);
  if (ptr->is_static) {
    disp_ptr("variable", (char*)ptr->variant.variable, iek_variable);
  } else {
    disp_ptr("field", (char*)ptr->variant.field, iek_field);
  }  /* if */
  switch (ptr->kind) {
    case pek_declspec_property:
      disp_string_ptr("get_routine.name", ptr->get_routine.name,
                      iek_other_text, (sizeof_t)0);
      disp_string_ptr("set_routine.name", ptr->set_routine.name,
                      iek_other_text, (sizeof_t)0);
      break;
    case pek_cli_property:
      disp_ptr("get_routine.ptr", (char*)ptr->get_routine.ptr, iek_routine);
      disp_ptr("set_routine.ptr", (char*)ptr->set_routine.ptr, iek_routine);
      break;
    case pek_cli_event:
      disp_ptr("add_routine", (char*)ptr->add_routine, iek_routine);
      disp_ptr("remove_routine", (char*)ptr->remove_routine, iek_routine);
      disp_ptr("raise_routine", (char*)ptr->raise_routine, iek_routine);
      break;
    default:
      /* "** BAD KIND **" was already displayed -- no further output is
         is needed. */
      break;
  }  /* switch */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_source_position("property_or_event_position",
                       &ptr->property_or_event_position);
  disp_source_range("indices_range", &ptr->indices_range);
  disp_source_range("definition_range", &ptr->definition_range);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* disp_property_or_event_descr */


static void disp_event_interface(an_event_interface_ptr  eip)
/*
Display the indicated event interface.
*/
{
  disp_ptr("next", (char *)eip->next, iek_event_interface);
  disp_ptr("interface_type", (char *)eip->interface_type, iek_type);
  disp_source_position("pos", &eip->pos);
}  /* disp_event_interface */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES

static void disp_ms_if_exists(an_ms_if_exists_ptr ptr)
/*
Display the indicated Microsoft __if_exists entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_ms_if_exists);
  if (ptr->entity.ptr != NULL) {
    disp_ptr("entity", (char *)ptr->entity.ptr,
             (an_il_entry_kind)ptr->entity.kind);
    /* is_if_exists is only set for the start of a block. */
    disp_boolean("is_if_exists", (a_boolean)ptr->is_if_exists);
  }  /* if */
  disp_source_position("position", &ptr->position);
  if (ptr->name_reference != NULL) {
    disp_ptr("name_reference", (char *)ptr->name_reference,
             iek_name_reference);
  }  /* if */
  if (ptr->pending) disp_boolean("pending", (a_boolean)ptr->pending);
  if (ptr->is_this) disp_boolean("is_this", (a_boolean)ptr->is_this);
}  /* disp_ms_if_exists */

#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */

#if RECORD_MACROS_IN_IL

static void disp_macro(a_macro_ptr  ptr)
/*
Display the indicated macro entry.
*/
{
  disp_source_corresp(&ptr->source_corresp, iek_macro);
  disp_ptr("next", (char *)ptr->next, iek_macro);
  disp_boolean("is_undef", (a_boolean)ptr->is_undef);
  disp_boolean("is_command_line_definition",
               (a_boolean)ptr->is_command_line_definition);
  disp_boolean("is_predefined", (a_boolean)ptr->is_predefined);
  disp_boolean("object_like", (a_boolean)ptr->object_like);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_source_range("replacement_text_range", &ptr->replacement_text_range);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  disp_string_ptr("text", ptr->text, iek_other_text, (sizeof_t)0);
}  /* disp_macro */

#endif /* RECORD_MACROS_IN_IL */

#if MACRO_INVOCATION_TREE_IN_IL

static void disp_simple_source_position(a_const_char              *str,
                                        a_simple_source_position  *pos)
/*
Display the indicated source position, preceding it with the specified
string.  Note that nothing is printed out when *pos is (the
simple-source-position portion of) null_source_position.
*/
{

  check_assertion(str != NULL);
  if (pos->seq != 0 || pos->column != 0) {
    Small_string<50> buff(str, ".seq");

    disp_unsigned_long(buff.as_temp_characters(), (unsigned long)pos->seq);
    buff.reset_to(str, ".column");
    disp_unsigned_long(buff.as_temp_characters(), (unsigned long)pos->column);
  }  /* if */
}  /* disp_simple_source_position */


static void disp_macro_invocation_record(a_macro_invocation_record_ptr   mirp,
                                         a_macro_invocation_record_index idx)
/*
Display the fields of the specified macro invocation record, which is at
offset idx.
*/
{
  (void)fprintf(f_display, "\nmacro_invocation_record#%ld\n", (long)idx);
  disp_long("parent_macro_index", (long)mirp->parent_macro_index);
  disp_ptr("assoc_macro", (char*)mirp->assoc_macro, iek_macro);
  disp_simple_source_position("start", &mirp->start);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_simple_source_position("end", &mirp->end);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if RECORD_MACRO_ARGS
  if (mirp->arguments != NULL) {
    disp_name("arguments");
    disp_null_term_string(mirp->arguments);
  }  /* if */
#endif /* RECORD_MACRO_ARGS */
}  /* disp_macro_invocation_record */


static void disp_macro_invocation_record_block(
                                   a_macro_invocation_record_block_ptr mirbp,
                                   a_macro_invocation_record_index num_records)
/*
Display, in numerical order,  the tree of macro invocation records rooted in
mirbp, up through index num_records-1.
*/
{
  a_macro_invocation_record_index num_records_to_display;
  int                             i;

  if (mirbp->left_subtree != NULL) {
    disp_macro_invocation_record_block(mirbp->left_subtree, num_records);
  }  /* if */
  num_records_to_display = num_records - mirbp->first_record_in_block;
  if (num_records_to_display > MACRO_INVOCATION_RECORDS_PER_BLOCK) {
    num_records_to_display = MACRO_INVOCATION_RECORDS_PER_BLOCK;
  }  /* if */
  for (i = 0; i < num_records_to_display; ++i) {
    disp_macro_invocation_record(mirbp->records + i,
                                 mirbp->first_record_in_block + i);
  }  /* for */
  if (mirbp->right_subtree != NULL) {
    disp_macro_invocation_record_block(mirbp->right_subtree, num_records);
  }  /* if */
}  /* disp_macro_invocation_record_block */

#endif /* MACRO_INVOCATION_TREE_IN_IL */

static void disp_seq_number_lookup_entry(a_seq_number_lookup_entry_ptr ptr)
/*
Display the indicated sequence number lookup entry.
*/
{
  disp_ptr("next", (char*)ptr->next, iek_seq_number_lookup_entry);
  disp_unsigned_long("first", (unsigned long)ptr->first);
  disp_unsigned_long("last", (unsigned long)ptr->last);
  disp_unsigned_long("line_number", (unsigned long)ptr->line_number);
  disp_ptr("source_file", (char*)ptr->source_file, iek_source_file);
}  /* disp_seq_number_lookup_entry */


static void disp_object_lifetime(an_object_lifetime_ptr ptr)
/*
Display the indicated object lifetime.
*/
{
  disp_ptr("entity", (char *)ptr->entity.ptr,
           (an_il_entry_kind)ptr->entity.kind);
  disp_name("kind");
  switch (ptr->kind) {
    case olk_global_static:
      (void)fprintf(f_display, "olk_global_static\n");
      break;
    case olk_block:
      (void)fprintf(f_display, "olk_block\n");
      break;
    case olk_block_after_label:
      (void)fprintf(f_display, "olk_block_after_label\n");
      break;
    case olk_function_static:
      (void)fprintf(f_display, "olk_function_static\n");
      break;
    case olk_expr_temporary:
      (void)fprintf(f_display, "olk_expr_temporary\n");
      break;
    case olk_try_block:
      (void)fprintf(f_display, "olk_try_block\n");
      break;
    default:
      (void)fprintf(f_display, "**BAD OBJECT LIFETIME KIND**\n");
  }  /* switch */
  if (ptr->has_block_after_label_child_lifetime) {
    disp_boolean("has_block_after_label_child_lifetime", TRUE);
  }  /* if */
  if (ptr->has_implicit_child) {
    disp_boolean("has_implicit_child", TRUE);
  }  /* if */
  disp_ptr("destructions", (char *)ptr->destructions, iek_dynamic_init);
  disp_ptr("parent_lifetime", (char *)ptr->parent_lifetime,
           iek_object_lifetime);
  disp_ptr("parent_destruction_sublist",
           (char *)ptr->parent_destruction_sublist, iek_dynamic_init);
  disp_ptr("child_lifetime", (char *)ptr->child_lifetime, iek_object_lifetime);
  disp_ptr("next", (char *)ptr->next, iek_object_lifetime);
}  /* disp_object_lifetime */


static void disp_local_scope_ref(a_local_scope_ref_ptr  ptr)
/*
Display the given reference to a scope in a function-scope (i.e., local)
memory region.
*/
{
  disp_ptr("scope", (char*)ptr->scope, iek_scope);
  disp_ptr("referrer", (char*)ptr->referrer.ptr,
           (an_il_entry_kind)ptr->referrer.kind);
}  /* disp_local_scope_ref */


static void disp_scope(a_scope_ptr ptr)
/*
Display the indicated scope.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_scope);
  disp_ptr("parent", (char *)ptr->parent, iek_scope);
  disp_name("kind");
  switch (ptr->kind) {
    case sck_file:
      (void)fprintf(f_display, "sck_file\n");
      break;
    case sck_block:
      (void)fprintf(f_display, "sck_block\n");
      if (ptr->variant.assoc_handler != NULL) {
        disp_ptr("assoc_handler", (char *)ptr->variant.assoc_handler,
                 iek_handler);
      }  /* if */
      break;
    case sck_func_prototype:
      (void)fprintf(f_display, "sck_func_prototype\n");
      goto do_assoc_type;
    case sck_enum:
      (void)fprintf(f_display, "sck_enum\n");
      goto do_assoc_type;
    case sck_class_struct_union:
      (void)fprintf(f_display, "sck_class_struct_union\n");
do_assoc_type:
      disp_ptr("assoc_type", (char *)ptr->variant.assoc_type, iek_type);
      break;
    case sck_condition:
      (void)fprintf(f_display, "sck_condition\n");
      disp_ptr("assoc_statement", (char *)ptr->variant.assoc_statement,
               iek_statement);
      break;
    case sck_namespace:
      (void)fprintf(f_display, "sck_namespace\n");
      disp_ptr("assoc_namespace", (char *)ptr->variant.assoc_namespace,
               iek_namespace);
      break;
    case sck_function:
      (void)fprintf(f_display, "sck_function\n");
      disp_ptr("routine.ptr", (char *)ptr->variant.routine.ptr, iek_routine);
      disp_ptr("parameters", (char *)ptr->variant.routine.parameters,
               iek_variable);
      disp_ptr("constructor_inits",
               (char *)ptr->variant.routine.constructor_inits,
               iek_constructor_init);
      disp_ptr("lifetime_of_local_static_vars",
               (char *)ptr->variant.routine.lifetime_of_local_static_vars,
               iek_object_lifetime);
      if (ptr->variant.routine.this_param_variable != NULL) {
        disp_ptr("this_param_variable",
                 (char *)ptr->variant.routine.this_param_variable,
                 iek_variable);
      }  /* if */
      if (ptr->variant.routine.return_value_variable != NULL) {
        disp_ptr("return_value_variable",
                 (char *)ptr->variant.routine.return_value_variable,
                 iek_variable);
      }  /* if */
      break;
    case sck_template_declaration:
      /* Only present when prototype instantiations are included in the IL. */
      (void)fprintf(f_display, "sck_template_declaration\n");
      break;
    case sck_template_instantiation:
      /* Front end only. */
    default:
      (void)fprintf(f_display, "**BAD SCOPE KIND**\n");
  }  /* switch */
  if (ptr->do_not_free_memory_region) {
    disp_boolean("do_not_free_memory_region", TRUE);
  }  /* if */
  if (ptr->is_constexpr_routine) {
    disp_boolean("is_constexpr_routine", TRUE);
  }  /* if */
  if (ptr->is_stmt_expr_block) {
    disp_boolean("is_stmt_expr_block", TRUE);
  }  /* if */
  if (ptr->is_placeholder_scope) {
    disp_boolean("is_placeholder_scope", TRUE);
  }  /* if */
  if (ptr->needed_walk_done) {
    disp_boolean("needed_walk_done", TRUE);
  }  /* if */
  disp_ptr("assoc_block", (char *)ptr->assoc_block, iek_statement);
  disp_ptr("lifetime", (char *)ptr->lifetime, iek_object_lifetime);
  disp_ptr("constants", (char *)ptr->constants, iek_constant);
  disp_ptr("types", (char *)ptr->types, iek_type);
  disp_ptr("variables", (char *)ptr->variables, iek_variable);
  disp_ptr("nonstatic_variables", (char *)ptr->nonstatic_variables,
           iek_variable);
  disp_ptr("labels", (char *)ptr->labels, iek_label);
  disp_ptr("routines", (char *)ptr->routines, iek_routine);
  disp_ptr("asm_entries", (char *)ptr->asm_entries, iek_asm_entry);
  disp_ptr("scopes", (char *)ptr->scopes, iek_scope);
  switch (ptr->kind) {
    case sck_file:
    case sck_namespace:
      disp_ptr("namespaces", (char *)ptr->namespaces, iek_namespace);
      FALLTHROUGH
    case sck_function:
    case sck_block:
    case sck_class_struct_union:
      disp_ptr("using_declarations", (char *)ptr->using_declarations,
               iek_using_decl);
      disp_ptr("using_directives", (char *)ptr->using_directives,
               iek_using_decl);
      break;
    default:;
  }  /* if */
  disp_ptr("dynamic_inits", (char *)ptr->dynamic_inits, iek_dynamic_init);
  if (ptr->kind == (a_scope_kind)sck_function ||
      ptr->kind == (a_scope_kind)sck_block) {
    disp_ptr("local_static_variable_inits",
             (char *)ptr->local_static_variable_inits,
             iek_local_static_variable_init);
    if (ptr->expr_node_refs != NULL) {
      disp_ptr("expr_node_refs", (char *)ptr->expr_node_refs,
               iek_local_expr_node_ref);
    }  /* if */
    if (ptr->scope_refs != NULL) {
      disp_ptr("scope_refs", (char *)ptr->scope_refs, iek_local_scope_ref);
    }  /* if */
  }  /* if */
  if (ptr->kind == (a_scope_kind)sck_function &&
      il_header.source_language != (a_source_language)sl_Cplusplus) {
    disp_ptr("vla_dimensions", (char *)ptr->vla_dimensions, iek_vla_dimension);
  }  /* if */
  disp_ptr("pragmas", (char *)ptr->pragmas, iek_pragma);
#if RECORD_HIDDEN_NAMES_IN_IL
  disp_ptr("hidden_names", (char *)ptr->hidden_names, iek_hidden_name);
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
#if RECORD_TEMPLATE_STRINGS
  disp_ptr("templates", (char *)ptr->templates, iek_template);
#endif /* RECORD_TEMPLATE_STRINGS */
#if MICROSOFT_EXTENSIONS_ALLOWED
  disp_ptr("ms_attributes", (char *)ptr->ms_attributes, iek_ms_attribute);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
  disp_ptr("ms_if_exists", (char *)ptr->ms_if_exists, iek_ms_if_exists);
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (ptr->kind == (a_scope_kind)sck_file ||
      ptr->kind == (a_scope_kind)sck_function) {
    disp_ptr("source_sequence_list", (char *)ptr->source_sequence_list,
             iek_source_sequence_entry);
    if (ptr->kind == (a_scope_kind)sck_function) {
      disp_ptr("src_seq_sublist_list", (char *)ptr->src_seq_sublist_list,
               iek_src_seq_sublist);
    }  /* if */
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
}  /* disp_scope */


static void disp_namespace(a_namespace_ptr  ptr)
/*
Display the indicated namespace entry.
*/
{
  disp_source_corresp(&ptr->source_corresp, iek_namespace);
  disp_ptr("next", (char *)ptr->next, iek_namespace);
#if MICROSOFT_EXTENSIONS_ALLOWED
  disp_ptr("proxy_class", (char *)ptr->proxy_class, iek_type);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (ptr->is_inline) {
    disp_boolean("is_inline", TRUE);
  }  /* if */
  if (ptr->has_internal_linkage) {
    disp_boolean("internal_linkage", TRUE);
  }  /* if */
  if (ptr->named_in_strong_using) {
    disp_boolean("named_in_strong_using", TRUE);
  }  /* if */
  if (ptr->is_std) {
    disp_boolean("is_std", TRUE);
  }  /* if */
#if BACK_END_IS_CP_GEN_BE
  if (ptr->shadowed_by_class) {
    disp_boolean("shadowed_by_class", TRUE);
  }  /* if */
#endif /* BACK_END_IS_CP_GEN_ BE */
#if GNU_EXTENSIONS_ALLOWED
  if (ptr->has_gnu_abi_tag_attribute) {
    disp_boolean("has_gnu_abi_tag_attribute", TRUE);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (ptr->is_namespace_alias) {
    disp_boolean("is_namespace_alias", TRUE);
    disp_ptr("assoc_namespace", (char *)ptr->variant.assoc_namespace,
             iek_namespace);
  } else {
    disp_ptr("assoc_scope", (char *)ptr->variant.assoc_scope, iek_scope);
  }  /* if */
}  /* disp_namespace */


static void disp_using_decl(a_using_decl_ptr  ptr)
/*
Display the indicated using-declaration or using-directive entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_using_decl);
  disp_ptr("entity", (char *)ptr->entity.ptr,
           (an_il_entry_kind)ptr->entity.kind);
  disp_source_position("position", &ptr->position);
  disp_ptr("attributes", (char *)ptr->attributes, iek_attribute);
  disp_boolean("is_using_directive", ptr->is_using_directive);
  if (!ptr->is_using_directive) {
    /* Either a class member using-declaration or a nonmember
       using-declaration. */
    disp_boolean("is_class_member", ptr->is_class_member);
    disp_boolean("is_using_enum", ptr->is_using_enum);
    disp_boolean("is_enumerator", ptr->is_enumerator);
    if (ptr->is_class_member) {
      /* Class member using-declaration. */
      disp_access("access", ptr->access);
      if (ptr->is_inheriting_ctor) disp_boolean("is_inheriting_ctor", TRUE);
      if (ptr->hidden) disp_boolean("hidden", TRUE);
      disp_ptr("qualifier.class_type",
               (char *)ptr->qualifier.class_type, iek_type);
    } else {
      /* Nonmember using-declaration. */
      disp_ptr("qualifier.namespace_ptr",
               (char *)ptr->qualifier.namespace_ptr, iek_namespace);
    }  /* if */
  }  /* if */
  if (ptr->compiler_generated) {
    disp_boolean("compiler_generated", ptr->compiler_generated);
  }  /* if */
  if (ptr->inline_namespace) {
    disp_boolean("inline_namespace", ptr->inline_namespace);
  }  /* if */
  if (ptr->strong) {
    disp_boolean("strong", ptr->strong);
  }  /* if */
  if (ptr->is_pack_expansion) {
    disp_boolean("is_pack_expansion", ptr->is_pack_expansion);
  }  /* if */
  if (ptr->is_representative) {
    disp_boolean("is_representative", ptr->is_representative);
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  disp_ptr("source_sequence_entry", (char *)ptr->source_sequence_entry,
           iek_source_sequence_entry);
  if (ptr->next_in_set != NULL) {
    disp_ptr("next_in_set", (char *)ptr->next_in_set,
             iek_using_decl);
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
}  /* disp_using_decl */


static void disp_dynamic_init(a_dynamic_init_ptr ptr)
/*
Display the indicated dynamic_init structure.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_dynamic_init);
  if (ptr->variable != NULL) {
    disp_ptr("variable", (char *)ptr->variable, iek_variable);
  }  /* if */
  if (ptr->destructor != NULL) {
    disp_ptr("destructor", (char *)ptr->destructor, iek_routine);
    if (ptr->lifetime != NULL) {
      disp_ptr("lifetime", (char *)ptr->lifetime, iek_object_lifetime);
      disp_ptr("next_in_destruction_list",
               (char *)ptr->next_in_destruction_list, iek_dynamic_init);
      disp_boolean("unordered", (a_boolean)ptr->unordered);
    }  /* if */
  }  /* if */
  if (ptr->init_expr_lifetime != NULL) {
    disp_ptr("init_expr_lifetime", (char *)ptr->init_expr_lifetime,
             iek_object_lifetime);
  }  /* if */
  if (ptr->static_temp) {
    disp_boolean("static_temp", TRUE);
  }  /* if */
  if (ptr->follows_an_exec_statement) {
    disp_boolean("follows_an_exec_statement", TRUE);
  }  /* if */
  if (ptr->inside_conditional_expression) {
    disp_boolean("inside_conditional_expression", TRUE);
  }  /* if */
  if (ptr->has_temporary_lifetime) {
    disp_boolean("has_temporary_lifetime", TRUE);
  }  /* if */
  if (ptr->is_constructor_init) {
    disp_boolean("is_constructor_init", TRUE);
  }  /* if */
  if (ptr->is_freeing_of_storage_on_exception) {
    disp_boolean("is_freeing_of_storage_on_exception", TRUE);
  }  /* if */
  if (ptr->is_array_freeing) {
    disp_boolean("is_array_freeing", TRUE);
  }  /* if */
  if (ptr->destruction_is_for_partially_constructed_aggregate) {
    disp_boolean("destruction_is_for_partially_constructed_aggregate", TRUE);
  }  /* if */
  if (ptr->overlaps_temps_in_inner_lifetime) {
    disp_boolean("overlaps_temps_in_inner_lifetime", TRUE);
    disp_ptr("lifetime_of_overlapping_temps",
              (char *)ptr->lifetime_of_overlapping_temps,
              iek_object_lifetime);
  }  /* if */
  if (ptr->is_explicit_cast) {
    disp_boolean("is_explicit_cast", TRUE);
  }  /* if */
  if (ptr->is_compound_literal) {
    disp_boolean("is_compound_literal", TRUE);
  }  /* if */
  if (ptr->is_braced_initializer) {
    disp_boolean("is_braced_initializer", TRUE);
  }  /* if */
  if (ptr->is_partially_initialized) {
    disp_boolean("is_partially_initialized", TRUE);
  }  /* if */
  if (ptr->is_result_for_class_rvalue_question_mark) {
    disp_boolean("is_result_for_class_rvalue_question_mark", TRUE);
  }  /* if */
  if (ptr->class_rvalue_initialized_through_master_entry) {
    disp_boolean("class_rvalue_initialized_through_master_entry", TRUE);
  }  /* if */
  if (ptr->is_reused_value) {
    disp_boolean("is_reused_value", TRUE);
  }  /* if */
#if DO_IL_LOWERING
  if (ptr->is_vla_deallocation) {
    disp_boolean("is_vla_deallocation", TRUE);
  }  /* if */
#if GENERATE_EH_TABLES
  if (ptr->is_freeing_of_exception_object) {
    disp_boolean("is_freeing_of_exception_object", TRUE);
  }  /* if */
#endif /* GENERATE_EH_TABLES */
#endif /* DO_IL_LOWERING */
  if (ptr->is_creation_of_initializer_list_object) {
    disp_boolean("is_creation_of_initializer_list_object", TRUE);
  }  /* if */
  if (ptr->is_array_for_initializer_list_object) {
    disp_boolean("is_array_for_initializer_list_object", TRUE);
  }  /* if */
  if (ptr->is_top_temporary_for_constexpr_reference_param) {
    disp_boolean("is_top_temporary_for_constexpr_reference_param", TRUE);
  }  /* if */
#if BACK_END_IS_CP_GEN_BE
  if (ptr->suppress_template_arguments_for_cast) {
    disp_boolean("suppress_template_arguments_for_cast", TRUE);
  }  /* if */
#endif /* BACK_END_IS_CP_GEN_BE */
  if (ptr->master_entry != NULL) {
    disp_ptr("master_entry", (char *)ptr->master_entry, iek_dynamic_init);
  }  /* if */
  disp_name("kind");
  switch (ptr->kind) {
    case dik_none:
      (void)fprintf(f_display, "dik_none\n");
      break;
    case dik_zero:
      (void)fprintf(f_display, "dik_zero\n");
      break;
    case dik_constant:
      (void)fprintf(f_display, "dik_constant\n");
      goto do_constant;
    case dik_expression:
      (void)fprintf(f_display, "dik_expression\n");
      disp_ptr("expression", (char *)ptr->variant.expression, iek_expr_node);
      break;
    case dik_class_result_via_ctor:
      (void)fprintf(f_display, "dik_class_result_via_ctor\n");
      disp_ptr("expression", (char *)ptr->variant.expression, iek_expr_node);
      break;
    case dik_constructor:
      (void)fprintf(f_display, "dik_constructor\n");
      disp_ptr("routine", (char *)ptr->variant.constructor.ptr,
               iek_routine);
      disp_ptr("args", (char *)ptr->variant.constructor.args,
               iek_expr_node);
      disp_boolean("is_copy_constructor_with_implied_source",
                   (a_boolean)ptr->variant.constructor.
                                    is_copy_constructor_with_implied_source);
      disp_boolean("is_implicit_copy_for_copy_initialization",
                   (a_boolean)ptr->variant.constructor.
                                    is_implicit_copy_for_copy_initialization);
      disp_boolean("value_initialization",
                   (a_boolean)ptr->variant.constructor.value_initialization);
      break;
    case dik_lambda:
      (void)fprintf(f_display, "dik_lambda\n");
      disp_ptr("lambda", (char *)ptr->variant.constant.lambda, iek_lambda);
      goto do_constant;
    case dik_nonconstant_aggregate:
      (void)fprintf(f_display, "dik_nonconstant_aggregate\n");
do_constant:
      if (ptr->variant.constant.non_constant) {
        disp_boolean("non_constant", TRUE);
      }  /* if */
      disp_ptr("constant", (char *)ptr->variant.constant.ptr, iek_constant);
      break;
    case dik_bitwise_copy:
      (void)fprintf(f_display, "dik_bitwise_copy\n");
      if (ptr->variant.bitwise_copy.source != NULL) {
        disp_ptr("source", (char *)ptr->variant.bitwise_copy.source,
                 iek_expr_node);
      }  /* if */
      break;
    default:
      (void)fprintf(f_display, "**BAD DYNAMIC INIT KIND**\n");
  }  /* switch */
}  /* disp_dynamic_init */


static void disp_local_static_variable_init(
                                         a_local_static_variable_init_ptr ptr)
/*
Display the indicated local_static_variable_init entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_local_static_variable_init);
  disp_ptr("variable", (char *)ptr->variable, iek_variable);
  disp_initializer(ptr->init_kind, &ptr->initializer,
                   /*is_member_constant=*/FALSE);
  disp_ptr("lifetime", (char *)ptr->lifetime, iek_object_lifetime);
}  /* disp_local_static_variable_init */


static void disp_vla_dimension(a_vla_dimension_ptr ptr)
/*
Display the indicated vla_dimension entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_vla_dimension);
  disp_ptr("type", (char *)ptr->type, iek_type);
  if (ptr->dimension_expr != NULL) {
    disp_ptr("dimension_expr", (char *)ptr->dimension_expr, iek_expr_node);
  } else {
    disp_ptr("original_dimension", (char *)ptr->original_dimension,
             iek_vla_dimension);
  }  /* if */
  if (ptr->in_prototype_scope) {
    disp_boolean("in_prototype_scope", TRUE);
  }  /* if */
  disp_source_position("position", &ptr->position);
#if DO_IL_LOWERING && !LOWER_VARIABLE_LENGTH_ARRAYS
  if (ptr->dimension_variable != NULL) {
    disp_ptr("dimension_variable", (char *)ptr->dimension_variable,
             iek_variable);
  }  /* if */
#endif /* DO_IL_LOWERING && !LOWER_VARIABLE_LENGTH_ARRAYS */
}  /* disp_vla_dimension */

#if DO_IL_LOWERING && IA64_ABI

static void disp_vcall_offset_entry(a_vcall_offset_entry_ptr ptr)
/*
Display the indicated vcall offset entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_vcall_offset_entry);
  disp_ptr("routine", (char *)ptr->routine, iek_routine);
  if (ptr->base_class != NULL) {
    disp_ptr("base_class", (char *)ptr->base_class, iek_base_class);
  }  /* if */
  disp_host_large_integer("vcall_offset_index", 
                          (a_host_large_integer)ptr->vcall_offset_index);
  if (ptr->is_primary) {
    disp_boolean("is_primary", TRUE);
  }  /* if */
}  /* disp_vcall_offset_entry */

#endif /* DO_IL_LOWERING && IA64_ABI */

static void disp_overriding_virtual_function (
		an_overriding_virtual_function_ptr ptr)
/*
Display the indicated overriding virtual function entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_overriding_virtual_function);
  disp_ptr("overriding_function", (char *)ptr->overriding_function,
           iek_routine);
  disp_ptr("primary_function", (char *)ptr->primary_function, iek_routine);
  disp_ptr("base_class", (char *)ptr->base_class, iek_base_class);
  if (ptr->return_adjustment_base_class != NULL) {
    disp_ptr("return_adjustment_base_class",
             (char *)ptr->return_adjustment_base_class, iek_base_class);
  }  /* if */
}  /* disp_overriding_virtual_function */


static void disp_derivation_step_list(a_derivation_step_ptr ptr,
                                      a_derivation_step_ptr tail)
/*
Display the indicated derivation step list.
*/
{
  if (ptr == NULL) {
    disp_ptr("path", (char *)NULL, iek_derivation_step);
  } else {
    disp_name("path");
    (void)fprintf(f_display, "\n");
    for (; ptr != tail->next; ptr = ptr->next) {
      disp_ptr("  base_class", (char *)ptr->base_class, iek_base_class);
    }  /* for */
  }  /* if */
}  /* disp_derivation_step_list */


static void disp_base_class_derivation(a_base_class_derivation_ptr ptr)
/*
Display the indicated base class derivation entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_base_class_derivation);
  if (ptr->direct) disp_boolean("direct", TRUE);
  if (ptr->preferred) disp_boolean("preferred", TRUE);
  disp_derivation_step_list(ptr->path, ptr->path_tail);
  disp_access("access", (an_access_specifier)ptr->access);
}  /* disp_base_class_derivation */


static void disp_base_class(a_base_class_ptr ptr)
/*
Display the indicated base class entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_base_class);
  disp_ptr("next_direct", (char *)ptr->next_direct, iek_base_class);
#if IA64_ABI
  disp_ptr("next_preorder", (char *)ptr->next_preorder, iek_base_class);
  disp_ptr("primary_base_class", (char *)ptr->primary_base_class, 
           iek_base_class);
#endif /* IA64_ABI */
  disp_ptr("attributes", (char *)ptr->attributes, iek_attribute);
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ptr->ms_attributes != NULL) {
    disp_ptr("ms_attributes", (char *)ptr->ms_attributes, iek_ms_attribute);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  disp_ptr("type", (char *)ptr->type, iek_type);
  if (ptr->orig_type != ptr->type) {
    disp_ptr("orig_type", (char *)ptr->orig_type, iek_type);
  }  /* if */
  disp_ptr("derived_class", (char *)ptr->derived_class, iek_type);
  disp_source_position("decl_position", &ptr->decl_position);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_source_range("base_specifier_range", &ptr->base_specifier_range);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  disp_boolean("direct", (a_boolean)ptr->direct);
  if (ptr->direct) {
    disp_host_large_unsigned("direct_base_number",
                             (a_host_large_unsigned)ptr->direct_base_number);
  }  /* if */
  disp_boolean("is_virtual", (a_boolean)ptr->is_virtual);
  disp_boolean("ambiguous", (a_boolean)ptr->ambiguous);
  disp_boolean("shares_virtual_function_info",
               (a_boolean)ptr->shares_virtual_function_info);
  disp_boolean("ignore_during_dependent_lookup",
               (a_boolean)ptr->ignore_during_dependent_lookup);
  disp_boolean("has_public_derivation", (a_boolean)ptr->has_public_derivation);
  disp_host_large_unsigned("offset", (a_host_large_unsigned)ptr->offset);
  if (ptr->is_virtual) {
#if CFRONT_OBJECT_CODE_COMPATIBILITY
    disp_ptr("data_section_base_class", (char *)ptr->data_section_base_class,
             iek_base_class);
    disp_boolean("complete_subobject", (a_boolean)ptr->complete_subobject);
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
    disp_boolean("is_optimized_empty_base",
                 (a_boolean)ptr->is_optimized_empty_base);
    if (ptr->is_pack_expansion) {
      disp_boolean("is_pack_expansion", TRUE);
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (ptr->is_implicit_direct_base) {
      disp_boolean("is_implicit_direct_base", TRUE);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if !IA64_ABI
    disp_host_large_unsigned("pointer_offset",
                             (a_host_large_unsigned)ptr->pointer_offset);
    disp_ptr("pointer_base_class", (char *)ptr->pointer_base_class,
             iek_base_class);
#endif /* !IA64_ABI */
  }  /* if */
  disp_ptr("derivation", (char *)ptr->derivation, iek_base_class_derivation);
  if (!ptr->is_pack_expansion) {
    disp_ptr("variant.overriding_virtual_functions",
             (char *)ptr->variant.overriding_virtual_functions,
             iek_overriding_virtual_function );
  }  /* if */
#if DO_IL_LOWERING
#if IA64_ABI
  /* Do not print out ptr->virtual_function_table_var, which is used only
     during IL lowering. */
#else /* !IA64_ABI */
  /* Likewise for ptr->virtual_function_table_offset. */
#endif /* !IA64_ABI */
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
  /* Likewise for index_in_construction_vtbl_array,
     base_subarray_index_in_construction_vtbl_array, and
     base_construction_vtbls. */
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
#if IA64_ABI
  /* Likewise for vbase_offset_index. */
#endif /* IA64_ABI */
#endif /* DO_IL_LOWERING */
}  /* disp_base_class */


static void disp_class_type_supplement(a_class_type_supplement_ptr ptr)
/*
Display the indicated class type supplement entry.
*/
{
  disp_ptr("base_classes", (char *)ptr->base_classes, iek_base_class);
  disp_ptr("direct_base_classes", (char *)ptr->direct_base_classes,
           iek_base_class);
#if IA64_ABI
  disp_ptr("preorder_base_classes", (char *)ptr->preorder_base_classes,
           iek_base_class);
  disp_ptr("primary_base_class", (char *)ptr->primary_base_class, 
           iek_base_class);
#endif /* IA64_ABI */
  disp_host_large_unsigned("size_without_virtual_base_classes",
                (a_host_large_unsigned)ptr->size_without_virtual_base_classes);
  disp_unsigned_long("alignment_without_virtual_base_classes",
                   (unsigned long)ptr->alignment_without_virtual_base_classes);
  disp_host_large_unsigned("highest_virtual_function_number",
                  (a_host_large_unsigned)ptr->highest_virtual_function_number);
#if DO_IL_LOWERING && IA64_ABI
  disp_host_large_integer("next_negative_virtual_table_index",
                 (a_host_large_integer)ptr->next_negative_virtual_table_index);
  disp_host_large_integer("first_vcall_offset_index",
                          (a_host_large_integer)ptr->first_vcall_offset_index);
  disp_ptr("vcall_offsets", (char*)ptr->vcall_offsets, iek_vcall_offset_entry);
#endif /* DO_IL_LOWERING && IA64_ABI */
  /* virtual_function_info_offset and virtual_function_info_base_class are
     undefined if highest_virtual_function_number is zero. */
  if (ptr->highest_virtual_function_number > 0) {
    disp_host_large_unsigned("virtual_function_info_offset",
                     (a_host_large_unsigned)ptr->virtual_function_info_offset);
    if (ptr->virtual_function_info_base_class != NULL) {
      disp_ptr("virtual_function_info_base_class",
               (char *)ptr->virtual_function_info_base_class, iek_base_class);
    }  /* if */
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ptr->uuid_string != NULL) {
    disp_string_ptr("uuid_string", ptr->uuid_string, iek_other_text,
                    (sizeof_t)0);
  }  /* if */
  if (ptr->decl_modifiers != DM_NONE) {
    disp_decl_modifiers(ptr->decl_modifiers);
  }  /* if */
  /* Only display orig_type_kind it differs from the type kind specified on
     the definition. */
  if (!scope_is_null_or_placeholder(ptr->assoc_scope)) {
    /* The associated type does have a definition. */
    a_type_ptr  class_type = ptr->assoc_scope->variant.assoc_type;
    if (class_type != NULL && class_type->kind != ptr->orig_type_kind) {
      disp_name("orig_type_kind");
      (void)fprintf(f_display, "%s\n", type_kind_string(ptr->orig_type_kind));
    }  /* if */
  }  /* if */
  if (ptr->inheritance_kind != (an_inheritance_kind)ihk_none) {
    disp_name("inheritance_kind");
    switch (ptr->inheritance_kind) {
      case ihk_single:
        (void)fprintf(f_display, "ihk_single\n");
        break;
      case ihk_multiple:
        (void)fprintf(f_display, "ihk_multiple\n");
        break;
      case ihk_virtual:
        (void)fprintf(f_display, "ihk_virtual\n");
        break;
      default:
        (void)fprintf(f_display, "**BAD INHERITANCE KIND**\n");
        break;
    }  /* switch */
    disp_boolean("inheritance_kind_is_explicit",
                 (a_boolean)ptr->inheritance_kind_is_explicit);
  }  /* if */
  if (ptr->has_direct_property_or_event) {
    disp_boolean("has_direct_property_or_event", TRUE);
  }  /* if */
  disp_assembly_visibility("declared_assembly_visibility",
                           enum_cast<an_assembly_visibility>(
                                           ptr->declared_assembly_visibility));
  disp_assembly_visibility("assembly_visibility",
                           enum_cast<an_assembly_visibility>(
                                                    ptr->assembly_visibility));
  disp_cli_class_type_kind("cli_class_type_kind",
                           enum_cast<a_cli_class_type_kind>(
                                                    ptr->cli_class_type_kind));
  if (ptr->is_hide_by_sig) {
    disp_boolean("is_hide_by_sig", TRUE);
  }  /* if */
  if (ptr->is_cli_array) {
    disp_boolean("is_cli_array", TRUE);
    if (ptr->is_cppcx_write_only_array) {
      disp_boolean("is_cppcx_write_only_array", TRUE);
    }  /* if */
  } else if (ptr->is_cppcx_box) {
    disp_boolean("is_cppcx_box", TRUE);
  }  /* if */
  if (ptr->is_partial) {
    disp_boolean("is_partial", TRUE);
  }  /* if */
  if (ptr->has_coclass_attribute) {
    disp_boolean("has_coclass_attribute", TRUE);
  }  /* if */
  if (ptr->has_explicitly_aligned_subobject) {
    disp_boolean("has_explicitly_aligned_subobject", TRUE);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  if (ptr->ELF_visibility != evk_unspecified) {
    disp_ELF_visibility_kind(enum_cast<an_ELF_visibility_kind>(
                                                         ptr->ELF_visibility));
  }  /* if */
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if NEAR_AND_FAR_ALLOWED
  if (ptr->qualifiers != TQ_NONE) {
    disp_name("qualifiers");
    disp_type_qualifiers(ptr->qualifiers);
  }  /* if */
#endif /* NEAR_AND_FAR_ALLOWED */
#if BACK_END_IS_CP_GEN_BE
  if (ptr->surrounding_name_linkage_state != nlk_none) {
    disp_name_linkage("surrounding_name_linkage_state",
                      enum_cast<a_name_linkage_kind>(
                                         ptr->surrounding_name_linkage_state));
  }  /* if */
#endif /* BACK_END_IS_CP_GEN_BE */
#if DO_IL_LOWERING
  if (ptr->compiler_generated) {
    disp_boolean("compiler_generated", TRUE);
  }  /* if */
#endif /* DO_IL_LOWERING */
  if (ptr->is_lambda_closure_class) {
    disp_boolean("is_lambda_closure_class", TRUE);
  }  /* if */
  if (ptr->is_generic_lambda_closure_class) {
    disp_boolean("is_generic_lambda_closure_class", TRUE);
  }  /* if */
  if (ptr->has_lambda_conversion_function) {
    disp_boolean("has_lambda_conversion_function", TRUE);
  }  /* if */
  if (ptr->is_initializer_list) {
    disp_boolean("is_initializer_list", TRUE);
  }  /* if */
  if (ptr->has_initializer_list_ctor) {
    disp_boolean("has_initializer_list_ctor", TRUE);
  }  /* if */
  if (ptr->has_anonymous_union_member) {
    disp_boolean("has_anonymous_union_member", TRUE);
  }  /* if */
  if (ptr->defined_in_variable_initializer) {
    disp_boolean("defined_in_variable_initializer", TRUE);
  }  /* if */
  if (ptr->defined_in_field_initializer) {
    disp_boolean("defined_in_field_initializer", TRUE);
  }  /* if */
  if (ptr->anonymous_union_kind != (an_anonymous_union_kind)auk_none) {
    disp_name("anonymous_union_kind");
    switch (ptr->anonymous_union_kind) {
      case auk_none:
        (void)fprintf(f_display, "auk_none\n");
        break;
      case auk_variable:
        (void)fprintf(f_display, "auk_variable\n");
        break;
      case auk_field:
        (void)fprintf(f_display, "auk_field\n");
        disp_ptr("anonymous_union_field", (char *)ptr->anonymous_union_field,
                 iek_field);
        break;
      default:
        (void)fprintf(f_display, "**BAD ANONYMOUS UNION KIND**\n");
    }  /* switch */
  }  /* if */
  if (ptr->is_va_list_tag) {
    disp_boolean("is_va_list_tag", TRUE);
  }  /* if */
  if (ptr->defined_in_parent_class) {
    disp_boolean("defined_in_parent_class", TRUE);
  }  /* if */
  if (ptr->has_nodiscard_attribute) {
    disp_boolean("has_nodiscard_attribute", TRUE);
  }  /* if */
  if (ptr->has_field_initializer) {
    disp_boolean("has_field_initializer", TRUE);
  }  /* if */
  if (ptr->removed_from_il) {
    disp_boolean("removed_from_il", TRUE);
  }  /* if */
  if (ptr->contains_error_cached) {
    disp_boolean("contains_error", (a_boolean)ptr->contains_error);
  }  /* if */
  if (ptr->contains_local_type_cached) {
    disp_boolean("contains_local_type", (a_boolean)ptr->contains_local_type);
  }  /* if */
  if (ptr->contains_unnamed_namespace_type_cached) {
    disp_boolean("contains_unnamed_namespace_type",
                 (a_boolean)ptr->contains_unnamed_namespace_type);
  }  /* if */
  if (ptr->does_not_contain_parentless_lambda_in_default_argument) {
    disp_boolean("does_not_contain_parentless_lambda_in_default_argument",
                 TRUE);
  }  /* if */
  if (ptr->does_not_contain_deprecated_or_unavailable_type) {
    disp_boolean("does_not_contain_deprecated_or_unavailable_type", TRUE);
  }  /* if */
  if (ptr->befriending_classes != NULL) {
    disp_class_list("befriending_classes", ptr->befriending_classes);
  }  /* if */
  if (ptr->friends != NULL) {
    disp_entity_list("friends", ptr->friends);
  }  /* if */
#if MAINTAIN_CLASS_MEMBER_LIST
  if (ptr->member_declarations != NULL) {
    disp_entity_list("member_declarations", ptr->member_declarations);
  }  /* if */
#endif /* MAINTAIN_CLASS_MEMBER_LIST */
  disp_ptr("assoc_scope", (char * )ptr->assoc_scope, iek_scope);
  if (ptr->assoc_template != NULL) {
    disp_ptr("assoc_template", (char*)ptr->assoc_template, iek_template);
  }  /* if */
  if (ptr->template_arg_list != NULL) {
    disp_template_arg_list("template_arg_list", ptr->template_arg_list);
  }  /* if */
  if (ptr->partial_spec_template_arg_list != NULL) {
    disp_template_arg_list("partial_spec_template_arg_list",
                           ptr->partial_spec_template_arg_list);
  }  /* if */
#if NEW_CAN_BE_FOLDED_INTO_CTOR
  disp_ptr("assoc_operator_new_routine",
           (char *)ptr->assoc_operator_new_routine, iek_routine);
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
#if DELETE_CAN_BE_FOLDED_INTO_DTOR
  disp_ptr("assoc_operator_delete_routine",
           (char *)ptr->assoc_operator_delete_routine, iek_routine);
#endif /* DELETE_CAN_BE_FOLDED_INTO_DTOR */
#if DO_IL_LOWERING
  /* Do not print out ptr->virtual_function_table_var which is used only during
     IL lowering. */
  if (ptr->subobject_partner != NULL) {
    disp_ptr("subobject_partner", (char *)ptr->subobject_partner, iek_type);
  }  /* if */
#if IA64_ABI
  /* Likewise ptr->virtual_table_table_var. */
#endif /* IA64_ABI */
#if MICROSOFT_EXTENSIONS_ALLOWED
  /* Likewise ptr->uuid_variable. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
  /* Likewise ptr->promoted_local_types. */
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
  /* Likewise construction_vtbls. */
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
#endif /* DO_IL_LOWERING */
  disp_int32("min_template_arguments", ptr->min_template_arguments);
  if (ptr->defined_in_variable_initializer) {
    disp_ptr("lambda_parent.variable", (char*)ptr->lambda_parent.variable,
             iek_variable);
  } else if (ptr->defined_in_field_initializer) {
    disp_ptr("lambda_parent.field", (char*)ptr->lambda_parent.field,
             iek_field);
  } else if (ptr->lambda_parent.routine != NULL) {
    disp_ptr("lambda_parent.routine", (char*)ptr->lambda_parent.routine,
             iek_routine);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ptr->corresponding_basic_type != NULL) {
    disp_ptr("corresponding_basic_type",
             (char *)ptr->corresponding_basic_type, iek_type);
  }  /* if */
  if (ptr->base_dispose_bool_routine != NULL) {
    disp_ptr("base_dispose_bool_routine",
             (char*)ptr->base_dispose_bool_routine, iek_routine);
  }  /* if */
  if (ptr->base_idisposable_dispose_routine != NULL) {
    disp_ptr("base_idisposable_dispose_routine",
             (char*)ptr->base_idisposable_dispose_routine, iek_routine);
  }  /* if */
  if (ptr->base_object_finalize_routine != NULL) {
    disp_ptr("base_object_finalize_routine",
             (char*)ptr->base_object_finalize_routine, iek_routine);
  }  /* if */
  if (ptr->invocation_type != NULL) {
    disp_ptr("invocation_type", (char*)ptr->invocation_type, iek_type);
  }  /* if */
  if (ptr->event_interfaces != NULL) {
    disp_ptr("event_interfaces", (char*)ptr->event_interfaces,
             iek_event_interface);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (ptr->proxy_of_type != NULL) {
    disp_ptr("proxy_of_type", (char *)ptr->proxy_of_type, iek_type);
  }  /* if */
}  /* disp_class_type_supplement */


static void disp_constructor_init(a_constructor_init_ptr ptr)
/*
Display the indicated constructor init entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_constructor_init);
  disp_boolean("compiler_generated", (a_boolean)ptr->compiler_generated);
  if (ptr->is_pack_expansion) disp_boolean("is_pack_expansion", TRUE);
  if (ptr->is_braced) disp_boolean("is_braced", TRUE);
  disp_name("kind");
  switch (ptr->kind) {
    case cik_virtual_base_class:
      (void)fprintf(f_display, "cik_virtual_base_class\n");
      goto do_base_class;
    case cik_direct_base_class:
      (void)fprintf(f_display, "cik_direct_base_class\n");
do_base_class:
      disp_ptr("base_class", (char *)ptr->variant.base_class,
               iek_base_class);
      break;
    case cik_field:
      (void)fprintf(f_display, "cik_field\n");
      disp_ptr("field", (char *)ptr->variant.field, iek_field);
      break;
    case cik_delegation:
      (void)fprintf(f_display, "cik_delegation\n");
      break;
    default:
      (void)fprintf(f_display, "**BAD CONSTRUCTOR INIT KIND**\n");
  }  /* switch */
  if (ptr->use_field_initializer) {
    disp_boolean("use_field_initializer", TRUE);
  } else {
    disp_ptr("initializer", (char *)ptr->initializer, iek_dynamic_init);
  }  /* if */
  if (ptr->source_expr != NULL) {
    disp_ptr("source_expr", (char *)ptr->source_expr, iek_expr_node);
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  disp_source_range("ctor_init_range", &ptr->ctor_init_range);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  if (ptr->orig_type != NULL) {
    disp_ptr("orig_type", (char *)ptr->orig_type, iek_type);
  }  /* if */
}  /* disp_constructor_init */

#if GNU_EXTENSIONS_ALLOWED

static void disp_asm_operand(an_asm_operand_ptr ptr)
/*
Display the indicated asm operand.
*/
{
#if !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
  an_asm_operand_constraint_ptr c;
#endif /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */

  disp_ptr("next", (char *)ptr->next, iek_asm_operand);
  if (ptr->name != NULL) {
    disp_string_ptr("name", ptr->name, iek_other_text, (sizeof_t)0);
  }  /* if */
#if RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
  if (ptr->is_output_operand) {
    disp_boolean("is_output_operand", TRUE);
  }  /* if */
  disp_string_ptr("constraints_string", ptr->constraints_string,
                  iek_other_text, (sizeof_t)0);
#else /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
  if (ptr->modifiers & (an_asm_operand_modifier)aom_output) {
    disp_boolean("aom_output", TRUE);
  }  /* if */
  if (ptr->modifiers & (an_asm_operand_modifier)aom_input) {
    disp_boolean("aom_input", TRUE);
  }  /* if */
  for (c = ptr->constraints; c != NULL; c = c->next) {
    fprintf(f_display, "constraint: %c\n",
            asm_operand_constraint_letters[(int)c->kind]);
#if GNU_X86_ASM_EXTENSIONS_ALLOWED
    if (c->cond_code != NULL) {
      fprintf(f_display, "cond_code: %s\n", c->cond_code);
    }  /* if */
#endif /* GNU_X86_ASM_EXTENSIONS_ALLOWED */
  }  /* for */
#endif /* RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
  disp_ptr("expr", (char *)ptr->expression, iek_expr_node);
}  /* disp_asm_operand */


static void disp_named_register_list(a_named_register_list_ptr ptr)
/*
Display the indicated named register list.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_named_register_list);
  disp_name("reg");
  (void)fprintf(f_display, "%s\n", named_register_names[ptr->reg]);
}  /* disp_named_register_list */

#endif /* GNU_EXTENSIONS_ALLOWED */

static void disp_asm_entry(an_asm_entry_ptr ptr)
/*
Display the indicated asm entry.
*/
{
  disp_source_corresp(&ptr->source_corresp, iek_asm_entry);
  disp_ptr("next", (char *)ptr->next, iek_asm_entry);
  disp_ptr("asm_string", (char *)ptr->asm_string, iek_constant);
#if GNU_EXTENSIONS_ALLOWED
  if (ptr->gnu_asm_form) {
    disp_boolean("gnu_asm_form", TRUE);
  }  /* if */
  if (ptr->is_volatile) {
    disp_boolean("is_volatile", TRUE);
  }  /* if */
  if (ptr->has_volatile_keyword) {
    disp_boolean("has_volatile_keyword", TRUE);
  }  /* if */
  if (ptr->has_inline_keyword) {
    disp_boolean("has_inline_keyword", TRUE);
  }  /* if */
  if (ptr->is_asm_goto) {
    disp_boolean("is_asm_goto", TRUE);
  }  /* if */
  disp_ptr("operands", (char *)ptr->operands, iek_asm_operand);
  disp_ptr("clobbers", (char *)ptr->clobbers, iek_named_register_list);
  (void)putc('\n', f_display);
#endif /* GNU_EXTENSIONS_ALLOWED */
}  /* disp_asm_entry */

#if GENERATE_SOURCE_SEQUENCE_LISTS

static void disp_source_sequence_entry(a_source_sequence_entry_ptr ssep)
/*
Display the indicated source sequence entry.
*/
{
  disp_ptr("next", (char *)ssep->next, iek_source_sequence_entry);
  disp_ptr("prev", (char *)ssep->prev, iek_source_sequence_entry);
  disp_ptr("entity", (char *)ssep->entity.ptr,
           (an_il_entry_kind)ssep->entity.kind);
}  /* disp_source_sequence_entry */


static void disp_src_seq_secondary_decl(a_src_seq_secondary_decl_ptr sssdp)
/*
Display the indicated source sequence secondary declaration entry.
*/
{
  disp_source_position("decl_position", &sssdp->decl_position);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (sssdp->decl_pos_info != NULL) {
    disp_source_range("identifier_range",
                      &sssdp->decl_pos_info->identifier_range);
    disp_source_range("specifiers_range",
                      &sssdp->decl_pos_info->specifiers_range);
    if ((an_il_entry_kind)sssdp->entity.kind == iek_namespace) {
      disp_source_range(
                    "namespace_definition_range",
                    &sssdp->decl_pos_info->variant.namespace_definition_range);
    } else {
      disp_source_range("declarator_range",
                        &sssdp->decl_pos_info->variant.declarator_range);
    }  /* if */
    disp_ptr("extra_positions", (char*)sssdp->decl_pos_info->extra_positions,
             iek_element_position);
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  disp_ptr("entity", (char *)sssdp->entity.ptr,
           (an_il_entry_kind)sssdp->entity.kind);
  disp_ptr("declared_type", (char *)sssdp->declared_type, iek_type);
  if (sssdp->name_reference != NULL) {
    disp_ptr("name_reference", (char *)sssdp->name_reference,
             iek_name_reference);
  }  /* if */
  disp_ptr("attributes", (char *)sssdp->attributes, iek_attribute);
  disp_name("declared_storage_class");
  disp_storage_class_name(sssdp->declared_storage_class);
  if (sssdp->autonomous_tag_decl) disp_boolean("autonomous_tag_decl", TRUE);
  if (sssdp->embedded_source_sequence_entries) {
    disp_boolean("embedded_source_sequence_entries", TRUE);
  }  /* if */
  if (sssdp->friend_decl) disp_boolean("friend_decl", TRUE);
  if (sssdp->declared_in_func_prototype) {
    disp_boolean("declared_in_func_prototype", TRUE);
  }  /* if */
  if (sssdp->specialized_with_new_syntax) {
    disp_boolean("specialized_with_new_syntax", TRUE);
  }  /* if */
  if (sssdp->first_declaration) disp_boolean("first_declaration", TRUE);
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  if (sssdp->is_partial_instantiation) {
    disp_boolean("is_partial_instantiation", TRUE);
  }  /* if */
  if (sssdp->compiler_generated_forward_decl) {
    disp_boolean("compiler_generated_forward_decl", TRUE);
  }  /* if */
  if (sssdp->originally_nonautonomous_definition) {
    disp_boolean("originally_nonautonomous_definition", TRUE);
  }  /* if */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
  if (sssdp->marked_as_gnu_extension) {
    disp_boolean("marked_as_gnu_extension", TRUE);
  }  /* if */
  if (sssdp->is_decl_after_first_in_comma_list) {
    disp_boolean("is_decl_after_first_in_comma_list", TRUE);
  }  /* if */
  if (sssdp->explicit_storage_class) {
    disp_boolean("explicit_storage_class", TRUE);
  }  /* if */
  if (sssdp->is_alias) {
    disp_boolean("is_alias", TRUE);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (sssdp->is_event_interface) {
    disp_boolean("is_event_interface", TRUE);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* disp_src_seq_secondary_decl */


static void disp_src_seq_end_of_construct(a_src_seq_end_of_construct_ptr ptr)
/*
Display the indicated source sequence end-of-construct entry.
*/
{
  disp_source_position("position", &ptr->position);
  disp_ptr("entity", (char *)ptr->entity.ptr,
           (an_il_entry_kind)ptr->entity.kind);
}  /* disp_src_seq_end_of_construct */


static void disp_src_seq_sublist(a_src_seq_sublist_ptr sssp)
/*
Display the indicated source sequence sublist header.
*/
{
  disp_ptr("next", (char *)sssp->next, iek_src_seq_sublist);
  disp_ptr("source_sequence_list", (char *)sssp->source_sequence_list,
           iek_source_sequence_entry);
  disp_ptr("last_source_sequence_entry",
           (char *)sssp->last_source_sequence_entry,
           iek_source_sequence_entry);
}  /* disp_src_seq_sublist */


static void disp_instantiation_directive(an_instantiation_directive_ptr  idp)
/*
Display the indicated instantiation-directive entry.
*/
{
  disp_source_position("position", &idp->position);
  disp_ptr("entity", (char *)idp->entity.ptr,
           (an_il_entry_kind)idp->entity.kind);
  if (idp->do_not_instantiate) {
    disp_boolean("do_not_instantiate", idp->do_not_instantiate);
  }  /* if */
  disp_ptr("attributes", (char *)idp->attributes, iek_attribute);
#if GENERATE_SOURCE_SEQUENCE_LISTS
  disp_ptr("declared_type", (char *)idp->declared_type, iek_type);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (idp->decl_pos_info != NULL) {
    disp_source_range("identifier_range",
                      &idp->decl_pos_info->identifier_range);
    disp_source_range("specifiers_range",
                      &idp->decl_pos_info->specifiers_range);
    disp_source_range("declarator_range",
                      &idp->decl_pos_info->variant.declarator_range);
    disp_ptr("extra_positions", (char*)idp->decl_pos_info->extra_positions,
             iek_element_position);
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* disp_instantiation_directive */

#if GENERATE_LINKAGE_SPEC_BLOCKS

static void disp_linkage_spec_block(a_linkage_spec_block_ptr lsbp)
/*
Display the indicated entry.
*/
{
  disp_ptr("name_strict", (char*)lsbp->name_string, iek_constant);
  disp_name_linkage("name_linkage", (a_name_linkage_kind)lsbp->name_linkage);
  disp_source_position("position", &lsbp->position);
  disp_source_position("end_position", &lsbp->end_position);
}  /* disp_linkage_spec_block */

#endif /* GENERATE_LINKAGE_SPEC_BLOCKS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

static void disp_static_assertion(a_static_assertion_ptr sap)
/*
Display the indicated static assertion entry.
*/
{
  disp_ptr("condition", (char*)sap->condition, iek_constant);
  disp_ptr("string_literal", (char*)sap->string_literal, iek_constant);
  disp_source_position("position", &sap->position);
}  /* disp_static_assertion */

#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED

static void disp_scope_orphaned_list_header(
                                          a_scope_orphaned_list_header_ptr ptr)
/*
Display the indicated a_scope_orphaned_list_header entry.
*/
{
  disp_ptr("next", (char *)ptr->next, iek_scope_orphaned_list_header);
  disp_ptr("assoc_routine", (char *)ptr->assoc_routine, iek_routine);
  disp_ptr("orphaned_types", (char *)ptr->orphaned_types, iek_type);
  disp_ptr("orphaned_variables", (char *)ptr->orphaned_variables,
           iek_variable);
  disp_ptr("orphaned_namespaces", (char *)ptr->orphaned_namespaces,
           iek_namespace);
#if GENERATE_SOURCE_SEQUENCE_LISTS
  disp_ptr("orphaned_src_seq_sublists", (char *)ptr->orphaned_src_seq_sublists,
           iek_src_seq_sublist);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
}  /* disp_scope_orphaned_list_header */

#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */

static void disp_entry(char             *entry_ptr,
                       an_il_entry_kind entry_kind)
/*
Display the nonstring entry at *entry_ptr, which is of kind entry_kind.
This routine is called during IL walking.
*/
{
  /* Do not display entries that are displayed at the point of use. */
  switch (entry_kind) {
    case iek_template_param_type_supplement:
    case iek_typeref_type_supplement:
    case iek_routine_type_supplement:
    case iek_based_type_list_member:
    case iek_block:
#if C99_IL_EXTENSIONS_SUPPORTED
    case iek_internal_complex_value:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case iek_try_supplement:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case iek_microsoft_try_supplement:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case iek_for_loop:
    case iek_range_based_for_loop:
    case iek_constexpr_if:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case iek_for_each_loop:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case iek_derivation_step:
    case iek_class_list_entry:
    case iek_routine_list_entry:
    case iek_template_arg:
    case iek_new_delete_supplement:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case iek_gcnew_supplement:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case iek_throw_supplement:
    case iek_condition_supplement:
#if !ABI_CHANGES_FOR_RTTI
    case iek_accessible_base_class:
#endif /* !ABI_CHANGES_FOR_RTTI */
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
    case iek_eh_prologue_supplement:
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#if ONE_INSTANTIATION_PER_OBJECT
    case iek_per_instantiation_needed_flags_entry:
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    case iek_decl_position_supplement:
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case iek_ms_attribute_arg:
    case iek_custom_ms_attribute_arg:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if RECORD_MACRO_INVOCATIONS
    case iek_macro_invocation_record_block:
#endif /* RECORD_MACRO_INVOCATIONS */
    case iek_il_entity_list_entry:
    case iek_integer_type_supplement:
#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
    case iek_gnu_routine_supplement:
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */
    case iek_coroutine_descr:
      break;
    default:
      (void)fprintf(f_display, "\n");
      disp_ptr_value(entry_ptr, entry_kind);
      (void)fprintf(f_display, "\n");
      switch (entry_kind) {
#if EXTRA_SOURCE_POSITIONS_IN_IL
        case iek_element_position:
          disp_element_position((an_element_position_ptr)entry_ptr);
          break;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        case iek_source_file:
          disp_source_file((a_source_file_ptr)entry_ptr);
          break;
#if MICROSOFT_EXTENSIONS_ALLOWED
        case iek_cli_metadata_file:
          disp_cli_metadata_file((a_cli_metadata_file_ptr)entry_ptr);
          break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        case iek_constant:
          disp_constant((a_constant_ptr)entry_ptr);
          break;
        case iek_param_type:
          disp_param_type((a_param_type_ptr)entry_ptr);
          break;
        case iek_type:
          disp_type((a_type_ptr)entry_ptr);
          break;
        case iek_variable:
          disp_variable((a_variable_ptr)entry_ptr);
          break;
        case iek_variable_template_info:
          disp_variable_template_info((a_variable_template_info_ptr)entry_ptr);
          break;
        case iek_routine:
          disp_routine((a_routine_ptr)entry_ptr);
          break;
        case iek_label:
          disp_label((a_label_ptr)entry_ptr);
          break;
        case iek_expr_node:
          disp_expr_node((an_expr_node_ptr)entry_ptr);
          break;
        case iek_field:
          disp_field((a_field_ptr)entry_ptr);
          break;
        case iek_exception_specification:
          disp_exception_specification(
                             (an_exception_specification_ptr)entry_ptr);
          break;
        case iek_exception_specification_type:
          disp_exception_specification_type(
                             (an_exception_specification_type_ptr)entry_ptr);
          break;
        case iek_switch_case_entry:
          disp_switch_case_entry((a_switch_case_entry_ptr)entry_ptr);
          break;
        case iek_switch_stmt_descr:
          disp_switch_stmt_descr((a_switch_stmt_descr_ptr)entry_ptr);
          break;
        case iek_handler:
          disp_handler((a_handler_ptr)entry_ptr);
          break;
        case iek_statement:
          disp_statement((a_statement_ptr)entry_ptr);
          break;
        case iek_object_lifetime:
          disp_object_lifetime((an_object_lifetime_ptr)entry_ptr);
          break;
        case iek_scope:
          disp_scope((a_scope_ptr)entry_ptr);
          break;
        case iek_pragma:
          disp_pragma((a_pragma_ptr)entry_ptr);
          break;
#if RECORD_HIDDEN_NAMES_IN_IL
        case iek_hidden_name:
          disp_hidden_name((a_hidden_name_ptr)entry_ptr);
          break;
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
        case iek_template_parameter:
          disp_template_parameter((a_template_parameter_ptr)entry_ptr);
          break;
        case iek_template_decl:
          disp_template_decl((a_template_decl_ptr)entry_ptr);
          break;
        case iek_requires_clause:
          disp_requires_clause((a_requires_clause_ptr)entry_ptr);
          break;
        case iek_template:
          disp_template((a_template_ptr)entry_ptr);
          break;
#if MICROSOFT_EXTENSIONS_ALLOWED
        case iek_ms_attribute:
          disp_ms_attribute((an_ms_attribute_ptr)entry_ptr);
          break;
        case iek_property_index_type:
          disp_property_index_type((a_property_index_type_ptr)entry_ptr);
          break;
        case iek_property_or_event_descr:
          disp_property_or_event_descr(
                                    (a_property_or_event_descr_ptr)entry_ptr);
          break;
        case iek_generic_constraint_clause:
          disp_generic_constraint_clause(
                                  (a_generic_constraint_clause_ptr)entry_ptr);
          break;
        case iek_generic_constraint:
          disp_generic_constraint((a_generic_constraint_ptr)entry_ptr);
          break;
        case iek_event_interface:
          disp_event_interface((an_event_interface_ptr)entry_ptr);
          break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
        case iek_ms_if_exists:
          disp_ms_if_exists((an_ms_if_exists_ptr)entry_ptr);
          break;
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
#if RECORD_MACROS_IN_IL
        case iek_macro:
          disp_macro((a_macro_ptr)entry_ptr);
          break;
#endif /* RECORD_MACROS_IN_IL */
        case iek_namespace:
          disp_namespace((a_namespace_ptr)entry_ptr);
          break;
        case iek_using_decl:
          disp_using_decl((a_using_decl_ptr)entry_ptr);
          break;
        case iek_dynamic_init:
          disp_dynamic_init((a_dynamic_init_ptr)entry_ptr);
          break;
        case iek_local_static_variable_init:
          disp_local_static_variable_init(
                                 (a_local_static_variable_init_ptr)entry_ptr);
          break;
        case iek_vla_dimension:
          disp_vla_dimension((a_vla_dimension_ptr)entry_ptr);
          break;
#if DO_IL_LOWERING && IA64_ABI
        case iek_vcall_offset_entry:
          disp_vcall_offset_entry((a_vcall_offset_entry_ptr)entry_ptr);
          break;
#endif /* DO_IL_LOWERING && IA64_ABI */
        case iek_overriding_virtual_function:
          disp_overriding_virtual_function(
                      (an_overriding_virtual_function_ptr)entry_ptr);
          break;
        case iek_base_class_derivation:
          disp_base_class_derivation((a_base_class_derivation_ptr)entry_ptr);
          break;
        case iek_base_class:
          disp_base_class((a_base_class_ptr)entry_ptr);
          break;
        case iek_class_type_supplement:
          disp_class_type_supplement((a_class_type_supplement_ptr)entry_ptr);
          break;
        case iek_constructor_init:
          disp_constructor_init((a_constructor_init_ptr)entry_ptr);
          break;
        case iek_asm_entry:
          disp_asm_entry((an_asm_entry_ptr)entry_ptr);
          break;
#if GNU_EXTENSIONS_ALLOWED
        case iek_asm_operand:
          disp_asm_operand((an_asm_operand_ptr)entry_ptr);
          break;
        case iek_named_register_list:
          disp_named_register_list((a_named_register_list_ptr)entry_ptr);
          break;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
        case iek_source_sequence_entry:
          disp_source_sequence_entry((a_source_sequence_entry_ptr)entry_ptr);
          break;
        case iek_src_seq_secondary_decl:
          disp_src_seq_secondary_decl((a_src_seq_secondary_decl_ptr)entry_ptr);
          break;
        case iek_src_seq_end_of_construct:
          disp_src_seq_end_of_construct(
                                    (a_src_seq_end_of_construct_ptr)entry_ptr);
          break;
        case iek_src_seq_sublist:
          disp_src_seq_sublist((a_src_seq_sublist_ptr)entry_ptr);
          break;
        case iek_instantiation_directive:
          disp_instantiation_directive(
                                   (an_instantiation_directive_ptr)entry_ptr);
          break;
#if GENERATE_LINKAGE_SPEC_BLOCKS
        case iek_linkage_spec_block:
          disp_linkage_spec_block((a_linkage_spec_block_ptr)entry_ptr);
          break;
#endif /* GENERATE_LINKAGE_SPEC_BLOCKS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        case iek_static_assertion:
          disp_static_assertion((a_static_assertion_ptr)entry_ptr);
          break;
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
        case iek_scope_orphaned_list_header:
          disp_scope_orphaned_list_header(
                                  (a_scope_orphaned_list_header_ptr)entry_ptr);
          break;
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
        case iek_name_reference:
          disp_name_reference((a_name_reference_ptr)entry_ptr);
          break;
        case iek_name_qualifier:
          disp_name_qualifier((a_name_qualifier_ptr)entry_ptr);
          break;
        case iek_seq_number_lookup_entry:
          disp_seq_number_lookup_entry(
                                    (a_seq_number_lookup_entry_ptr)entry_ptr);
          break;
        case iek_local_expr_node_ref:
          disp_local_expr_node_ref((a_local_expr_node_ref_ptr)entry_ptr);
          break;
        case iek_local_scope_ref:
          disp_local_scope_ref((a_local_scope_ref_ptr)entry_ptr);
          break;
        case iek_lambda:
          disp_lambda((a_lambda_ptr)entry_ptr);
          break;
        case iek_lambda_capture:
          disp_lambda_capture((a_lambda_capture_ptr)entry_ptr);
          break;
        case iek_attribute:
          disp_attribute((an_attribute_ptr)entry_ptr);
          break;
        case iek_attribute_arg:
          disp_attribute_arg((an_attribute_arg_ptr)entry_ptr);
          break;
        case iek_attribute_group:
          disp_attribute_group((an_attribute_group_ptr)entry_ptr);
          break;
        case iek_subobject_path:
          disp_subobject_path((a_subobject_path_ptr)entry_ptr);
          break;
        case iek_module:
          disp_module((a_module_ptr)entry_ptr);
          break;
        case iek_module_import_decl:
          disp_module_import_decl((a_module_import_decl_ptr)entry_ptr);
          break;
        case iek_scoped_expression:
          disp_scoped_expression((a_scoped_expression_ptr)entry_ptr);
          break;
        default:
          (void)fprintf(f_display, "**BAD ENTRY KIND**\n");
      }  /* switch */
  }  /* switch */
}  /* disp_entry */


static void disp_source_language_name(a_source_language source_language)
/*
Display the name for the indicated source language name.
*/
{
  a_const_char *s;

  switch (source_language) {
    case sl_Cplusplus:    s = "sl_Cplusplus";            break;
    case sl_C:            s = "sl_C";                    break;
    default:              s = "**BAD SOURCE LANGUAGE**"; break;
  }  /* switch */
  (void)fprintf(f_display, "%s", s);
}  /* disp_source_language_name */


static void init_for_il_to_str_output(void)
/*
Set up for use of the il_to_str routines.
*/
{
  clear_il_to_str_output_control_block(&octl);
  octl.output_str = put_str_to_display;
  octl.gen_pcc_code = il_header.pcc_compatibility_mode;
#if DEBUG
  octl.debug_output = TRUE;
#endif /* DEBUG */
}  /* init_for_il_to_str_output */


void disp_file_scope_il(void)
/*
Display the IL for the file scope in human-readable form.
*/
{
  /* Set up for use of the il_to_str routines. */
  init_for_il_to_str_output();
  (void)fprintf(f_display, 
            "\n\nIntermediate language for memory region 1 (file scope):\n");

  displaying_file_scope_il = TRUE;
  (void)fprintf(f_display, "\nil_header:\n");
  disp_ptr("primary_source_file", (char *)il_header.primary_source_file,
           iek_source_file);
  disp_ptr("primary_scope", (char *)il_header.primary_scope, iek_scope);
  disp_ptr("file_scope_statements", (char *)il_header.file_scope_statements,
           iek_il_entity_list_entry);
  disp_ptr("main_routine", (char *)il_header.main_routine, iek_routine);
  disp_string_ptr("compiler_version", il_header.compiler_version,
                  iek_other_text, (sizeof_t)0);
  disp_string_ptr("time_of_compilation", il_header.time_of_compilation,
                  iek_other_text, (sizeof_t)0);
  disp_boolean("plain_chars_are_signed",
               (a_boolean)il_header.plain_chars_are_signed);
  /* region_scope_entry is not displayed. */
  disp_name("source_language");
  disp_source_language_name(il_header.source_language);
  (void)fprintf(f_display, "\n");
  disp_unsigned_long("std_version", il_header.std_version);
  disp_boolean("pcc_compatibility_mode",
               (a_boolean)il_header.pcc_compatibility_mode);
  disp_boolean("enum_type_is_integral",
               (a_boolean)il_header.enum_type_is_integral);
  if (il_header.default_max_member_alignment != 0) {
    disp_unsigned_long("default_max_member_alignment",
                       (unsigned long)il_header.default_max_member_alignment);
  }  /* if */
#if RECORD_MACROS_IN_IL
  disp_ptr("macros", (char *)il_header.macros, iek_macro);
#endif /* RECORD_MACROS_IN_IL */
#if MICROSOFT_EXTENSIONS_ALLOWED
  disp_boolean("microsoft_mode", (a_boolean)il_header.microsoft_mode);
  disp_boolean("cppcli_enabled", (a_boolean)il_header.cppcli_enabled);
  disp_boolean("cppcx_enabled", (a_boolean)il_header.cppcx_enabled);
  disp_unsigned_long("microsoft_version", il_header.microsoft_version);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  disp_boolean("gcc_mode", (a_boolean)il_header.gcc_mode);
  disp_boolean("gpp_mode", (a_boolean)il_header.gpp_mode);
  disp_unsigned_long("gnu_version", il_header.gnu_version);
  disp_boolean("short_enums", (a_boolean)il_header.short_enums);
  disp_boolean("default_nocommon", (a_boolean)il_header.default_nocommon);
#endif /* GNU_EXTENSIONS_ALLOWED */
#if NEAR_AND_FAR_ALLOWED
  disp_boolean("near_and_far_are_enabled",
               (a_boolean)il_header.near_and_far_are_enabled);
  disp_boolean("far_data_pointers",
               (a_boolean)il_header.far_data_pointers);
  disp_boolean("far_code_pointers",
               (a_boolean)il_header.far_code_pointers);
#endif /* NEAR_AND_FAR_ALLOWED */
  disp_boolean("UCN_identifiers_used",
               (a_boolean)il_header.UCN_identifiers_used);
  disp_boolean("vla_used", (a_boolean)il_header.vla_used);
  disp_boolean("any_templates_seen", (a_boolean)il_header.any_templates_seen);
  disp_boolean("prototype_instantiations_in_il",
               (a_boolean)il_header.prototype_instantiations_in_il);
  disp_boolean("il_has_all_prototype_instantiations",
               (a_boolean)il_header.il_has_all_prototype_instantiations);
  disp_boolean("il_has_C_semantics", (a_boolean)il_header.il_has_C_semantics);
#if ONE_INSTANTIATION_PER_OBJECT
  if (il_header.instantiation_dir_name != NULL) {
    disp_string_ptr("instantiation_dir_name",
                    il_header.instantiation_dir_name,
                    iek_other_text, (sizeof_t)0);
  }  /* if */
  if (il_header.number_of_external_nonclass_template_entities != 0) {
    disp_unsigned_long("number_of_external_nonclass_template_entities",
                      il_header.number_of_external_nonclass_template_entities);
  }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  disp_ptr("nontag_types_used_in_exception_or_rtti",
           (char *)il_header.nontag_types_used_in_exception_or_rtti,
           iek_type);
  disp_ptr("seq_number_lookup_entries",
           (char *)il_header.seq_number_lookup_entries,
           iek_seq_number_lookup_entry);
#if MACRO_INVOCATION_TREE_IN_IL
  disp_long("num_macro_invocation_records",
            (long)il_header.num_macro_invocation_records);
  disp_long("max_macro_invocation_depth",
            (long)il_header.max_macro_invocation_depth);
  if (il_header.root_macro_invocation_record_block != NULL) {
    disp_macro_invocation_record_block(
                                  il_header.root_macro_invocation_record_block,
                                  il_header.num_macro_invocation_records);
  }  /* if */
#endif /* MACRO_INVOCATION_TREE_IN_IL */
#if SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
  if (il_header.file_scope_dynamic_init_routines != NULL) {
    disp_routine_list("file_scope_dynamic_init_routines",
                      il_header.file_scope_dynamic_init_routines);
  }  /* if */
#if !USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES
  if (il_header.thread_local_dynamic_init_routines != NULL) {
    disp_routine_list("thread_local_dynamic_init_routines",
                      il_header.thread_local_dynamic_init_routines);
  }  /* if */
#endif /* !USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES */
#endif /* SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */
  if (il_header.target_configuration_index != NO_TARGET_CONFIG) {
    disp_long("target_configuration_index",
              (long)il_header.target_configuration_index);
  }  /* if */
  walk_file_scope_il(disp_entry, (a_string_entry_process_function_ptr)NULL,
                     (a_remap_function_ptr)NULL, (a_remap_function_ptr)NULL,
                     (a_walk_termination_test_function_ptr)NULL,
                     /*clear_fe_pointers=*/FALSE);
}  /* disp_file_scope_il */


void disp_routine_scope_il(a_memory_region_number region_number)
/*
Display the IL for the indicated region (a function scope) in human-readable
form.
*/
{
  a_scope_ptr   sp;
  a_routine_ptr rp;
  a_const_char  *fname = NULL;

  /* Set up for use of the il_to_str routines. */
  init_for_il_to_str_output();
  /* Extract the associated function name. */
  sp = il_header.region_scope_entry[region_number];
  if (sp != NULL) {
    if (sp->kind == (a_scope_kind)sck_function) {
      rp = sp->variant.routine.ptr;
      if (rp != NULL) {
        fname = rp->source_corresp.name;
        /* NULL pointer is used for blank COMMON and unnamed main programs. */
        if (fname == NULL) fname = "<unnamed>";
      }  /* if */
    }  /* if */
  }  /* if */
  if (fname == NULL) fname = "**NAME UNKNOWN**";
  (void)fprintf(f_display,
        "\n\nIntermediate language for memory region %ld (function \"%s\"):\n",
        (long)region_number, fname);
  displaying_file_scope_il = FALSE;
  walk_routine_scope_il(region_number,
                        disp_entry, (a_string_entry_process_function_ptr)NULL,
                        (a_remap_function_ptr)NULL, (a_remap_function_ptr)NULL,
                        (a_walk_termination_test_function_ptr)NULL,
                        /*clear_fe_pointers=*/FALSE);
}  /* disp_routine_scope_il */

#if !STANDALONE_IL_DISPLAY

static void init_front_end_f_display()
/*
Initialize f_display for processing outside of the standalone IL display
application (i.e., processing performed by the main front end binary itself).
*/
{
  f_display = default_il_display_output_file();
}  /* init_front_end_f_display */


static void clean_up_front_end_f_display()
/*
Clean up the f_display value that was initialized for processing outside of the
standalone IL display application (i.e., processing performed by the main front
end binary itself).
*/
{
  if (f_display != stderr && f_display != stdout) {
    (void)fclose(f_display);
    f_display = NULL;
  }  /* if */
}  /* clean_up_front_end_f_display */


void pragma_il_display(ARG_UNUSED a_pending_pragma_ptr ppp,
                       ARG_UNUSED a_symbol_ptr         sym_ptr,
                       ARG_UNUSED a_statement_ptr      stmt_ptr)
/*
A "#pragma il_display" was attached to a symbol or statement; display
the IL associated with the entity that was bound to the pragma.
*/
{
  char             *entry_ptr = NULL;
  an_il_entry_kind kind;

  init_front_end_f_display();
  if (sym_ptr != NULL) {
#if DEBUG
    db_sym(sym_ptr);
#endif /* DEBUG */
    entry_ptr = il_entry_for_symbol_null_okay(sym_ptr, &kind);
  }  /* if */
  if (stmt_ptr != NULL) {
#if DEBUG
    db_statement(stmt_ptr);
#endif /* DEBUG */
    entry_ptr = (char*)stmt_ptr;
    kind = (an_il_entry_kind)iek_statement;
  }  /* if */
  if (entry_ptr != NULL) {
    init_for_il_to_str_output();
    disp_entry(entry_ptr, kind);
  }  /* if */
  fflush(f_display);
  clean_up_front_end_f_display();
}  /* pragma_il_display */

#endif /* !STANDALONE_IL_DISPLAY */

void do_il_display(char *file_name)
/*
Display the entire IL tree that resides in memory.  If file_name is not NULL,
it is the name of the file from which the IL was read.
*/
{
  a_memory_region_number region_number;

#if !STANDALONE_UTILITY_PROGRAM
  init_front_end_f_display();
#endif /* !STANDALONE_UTILITY_PROGRAM */
  if (file_name == NULL) {
    (void)fprintf(f_display,
                  "Display of IL produced by the compilation of \"%s\"\n",
                  primary_source_file_name);
  } else {
    (void)fprintf(
          f_display,
          "Display of IL file \"%s\", produced by the compilation of \"%s\"\n",
          file_name, primary_source_file_name);
  }  /* if */
  /* Display the file scope IL. */
  disp_file_scope_il();
  /* Read and display the IL for each function scope. */
  for (region_number = FILE_SCOPE_REGION_NUMBER+1;
       region_number <= highest_used_region_number;
       region_number++) {
    if (
#if IL_SHOULD_BE_WRITTEN_TO_FILE
        index_for_il_file[region_number] != 0
#else /* !IL_SHOULD_BE_WRITTEN_TO_FILE */
        mem_region_table[region_number] != 0
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
                                             ) {
#if STANDALONE_IL_DISPLAY
      read_memory_region(region_number);
#endif /* STANDALONE_IL_DISPLAY */
      disp_routine_scope_il(region_number);
#if STANDALONE_IL_DISPLAY
      free_memory_region(region_number);
#endif /* STANDALONE_IL_DISPLAY */
    } else {
      /* Skip this memory region -- the associated routine was removed from
         the IL (e.g., because it is unneeded or was reserved for a trivial
         default constructor). */
    }  /* if */
  }  /* for */
  fflush(f_display);
#if !STANDALONE_UTILITY_PROGRAM
  clean_up_front_end_f_display();
#endif /* !STANDALONE_UTILITY_PROGRAM */
}  /* do_il_display */


/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#if STANDALONE_IL_DISPLAY
/*
The "main" routine must be outside of the EDG namespace, so do a
using-directive to make the EDG names visible.
*/
USING_NAMESPACE_EDG

int main(int argc, char *argv[])
/*
Main program for il_display as a program.  The program is invoked by

  il_display file.cil

where file.cil specifies the IL file.  Output is to stdout.
*/
{
  char                   *file_name;
  FILE                   *f_il_input;
  int                    optind = 1;

#if DEBUG
  /* Initialize the file variable used for debug output.  This should be
     done before anything else that could potentially produce debug output. */
  f_debug = stderr;
#endif /* DEBUG */
  /* Initialize the components of the front end needed by standalone
     utility programs; further initialization is done after the il_header
     has been read. */
  standalone_utility_early_init();
  /* Set the position for errors to "unknown". */
  set_position_to(error_position, 0, SP_COL_UNKNOWN);
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
      default:
        str_command_line_error(ec_cl_invalid_option, argv[optind]);
    }  /* switch */
    optind++;
  }  /* while */
  if (optind != argc - 1) {
    command_line_error(ec_cl_il_display_requires_il_file_name);
  }  /* if */
  file_name = argv[optind];
  f_il_input = fopen(file_name, "rb");
  if (f_il_input == NULL) {
    str_command_line_error(ec_cl_could_not_open_il_file, file_name);
  }  /* if */
  /* Read the file-scope IL. */
  il_read(f_il_input);
  /* Complete the initialization (based on il_header contents). */
  standalone_utility_late_init();
  primary_source_file_name = il_header.primary_source_file->file_name;
  f_display = stdout;
  do_il_display(file_name);
  (void)fclose(f_il_input);
  normal_termination();
  return 0;  /* Not reached; here to keep lint happy. */
}  /* main */
#endif /* STANDALONE_IL_DISPLAY */

#else /* !NEED_IL_DISPLAY */

#ifdef USING_QUANTIFY
/*
Quantify has a bug that causes an error when an empty object file is used.
When using quantify, generate a dummy variable.
*/
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */
char quantify_dummy_in_il_display;
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
#endif /* defined(USING_QUANTIFY) */


#endif /* NEED_IL_DISPLAY */

