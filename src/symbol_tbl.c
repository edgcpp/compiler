/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

symbol_tbl.c - Symbol table management routines.

*/

/* Header files common to all files. */
#include "fe_common.h"
/* Although symbol_tbl.c is not really a "declaration processing file",
   it turns out that most of the header files it needs are in decl_hdrs.h. */
#include "decl_hdrs.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Additional header files. */
#include "literals.h"
/* macro.h is needed for enter_predef_macro and clear_macro_def. */
#include "macro.h"
#if MICROSOFT_EXTENSIONS_ALLOWED
#include "ms_attrib.h"
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#include "overload.h"
#include "folding.h"
#include "sys_predef.h"
#include "interpret.h"

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/* The multiplier used in the hash algorithm that generates an index
   in the hash table from an identifier name string.  Do not change
   without investigating the hash table performance that results.
   Prime values are likely to work better than non-prime values. */
#define HASH_FACTOR ((unsigned int)73)

/*
Dummy symbol headers used for compiler-generated error symbols and for
unnamed class symbols.
*/
STATIC_THREAD a_symbol_header_ptr
		error_symbol_header,
		template_param_object_symbol_header,
		unnamed_tag_symbol_header,
		anonymous_parent_object_symbol_header,
		unnamed_field_symbol_header,
		unnamed_virtual_function_symbol_header,
		unnamed_namespace_symbol_header;

STATIC_THREAD a_symbol_ptr
		symbols_with_no_scope_tail;
			/* End of the symbols_with_no_scope list. */

STATIC_THREAD a_boolean
		va_list_global_alias_has_been_created;
			/* TRUE if the va_list type is in namespace std and
			   a global using-declaration has already been
			   created as a result of an include of stdarg.h. */

#if NAMED_ADDRESS_SPACES_ALLOWED
STATIC_THREAD a_named_address_space_id
		next_named_address_space_id;
			/* The next unused id for named address spaces.  The
			   first id is one. */
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */

#if NAMED_REGISTERS_ALLOWED
STATIC_THREAD a_named_register_id
		next_named_register_id;
			/* The next unused id for named registers.  The
			   first id is one. */
#endif /* NAMED_REGISTERS_ALLOWED */

/*
An empty symbol used to initialize newly allocated symbols.
*/
STATIC_THREAD a_symbol
                cleared_symbol;

static inline void clear_symbol(a_symbol_ptr   sym,
                                a_symbol_kind  kind)
/*
Set the shared fields of a symbol to default values, set the kind, and
initialize its variant fields.
*/
{
  *sym = cleared_symbol;
  if (scope_stack != NULL && in_code_from_module()) {
    sym->from_module_code = TRUE;
  }  /* if */
  set_symbol_kind(sym, kind);
}  /* clear_symbol */

#if DEBUG
/* GCOVR_EXCL_START */
/*
Information used to gather performance statistics on the symbol table:
*/
STATIC_THREAD unsigned long
		num_symbols_allocated,
		num_symbol_headers_allocated,
		num_symbol_headers_in_hash_table,
		num_conversion_headers_allocated,
		num_literal_operator_headers_allocated,
		symbol_name_string_space,
		num_symbol_header_lookup_entries_allocated,
		num_field_symbol_supplements_allocated,
		num_static_data_member_supplements_allocated,
		num_enum_symbol_supplements_allocated,
		num_class_symbol_supplements_allocated,
		num_template_symbol_supplements_allocated,
		num_namespace_symbol_supplements_allocated,
		num_template_params_allocated,
		num_param_ids_allocated,
		num_dependent_type_fixups_allocated,
		num_template_instances_allocated,
		num_master_instances_allocated,
		num_symbol_list_entries_allocated,
		num_type_list_entries_allocated,
		num_substituted_type_list_entries_allocated,
                num_out_of_class_partial_specs_allocated,
		num_template_decl_info_allocated,
		num_nondependent_call_info_allocated,
		num_templ_friend_info_allocated,
		num_namespace_list_entries_allocated,
		num_extern_symbol_descrs_allocated,
		num_vla_fixups_allocated,
		num_extern_type_fixups_allocated,
		num_projection_descrs_allocated,
		num_used_symbol_buckets,
		num_searches_for_symbols,
		num_compares_for_symbols,
		num_access_error_descrs_allocated,
		num_progenitors_allocated,
		num_hash_tables_allocated,
		num_hash_table_entries_allocated,
		total_hash_table_size,
		num_saved_macro_states_allocated,
#if MICROSOFT_EXTENSIONS_ALLOWED
		num_hide_by_sig_list_entries_allocated,
		num_property_set_symbol_supplements_allocated,
		num_prop_or_event_accessor_header_lookups_allocated,
		num_ms_attr_alt_name_entries_allocated,
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
		num_token_sequence_xrefs_allocated,
		num_constexpr_if_cache_info_allocated,
		num_exception_spec_error_descrs_allocated;

/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

/*
A structure recording instance counts for templates.
*/
struct an_inst_count {
  ~an_inst_count() {}	/* To make an_inst_count non-trivially-copyable. */
  a_symbol_kind  kind;
			/* The kind of symbol associated with tssp. */
  a_template_symbol_supplement_ptr
                 tssp;
			/* The "template symbol supplement" for the templated
			   entity being tracked. */
  unsigned long  count, defined;
			/* The number of instances recorded for this templated
			   entity, and the number of those instances that are
			   considered "defined". */
};

STATIC_THREAD Dyn_array<an_inst_count>
		 *inst_counters;
			/* A dynamic array used to collect all templated
			   entities when the --top_templates option is in
			   effect, later used to determine the most-used
			   templates. */

STATIC_THREAD a_namespace_list_entry_ptr
		global_namespace_list_entry;
			/* Pointer to a namespace list entry for the
			   global scope.  This contains a NULL namespace
			   pointer. */
/*
Array used to hold an identifier for external name or destructor name
generation.
*/
STATIC_THREAD char
                *ident_buffer;
			/* Buffer itself.  Dynamic allocated; current size is
			   given by size_ident_buffer.  Allocated in general
			   storage.  Not per-file. */
STATIC_THREAD sizeof_t
                size_ident_buffer;
			/* Current allocated size of ident_buffer. */
#define IDENT_BUFFER_INCREMENTAL_ALLOCATION 300
			/* Incremental allocation for ident_buffer.  Should
			   be bigger than most identifiers. */

STATIC_THREAD a_param_id_ptr
		avail_param_ids;
			/* List of parameter id entries freed and available
			   for reuse. */

STATIC_THREAD a_dependent_type_fixup_ptr
		avail_dependent_type_fixups;
			/* List of dependent type fixup entries freed and
			   available for reuse. */

STATIC_THREAD an_access_error_descr_ptr
		avail_access_error_descrs;
			/* List of access error description  entries (allocated
                           in front end storage) freed and available for
                           reuse. */

STATIC_THREAD a_symbol_list_entry_ptr
		avail_symbol_list_entries;
			/* List of symbol list entries freed and available for
			   reuse. */

STATIC_THREAD a_type_list_entry_ptr
		avail_type_list_entries;
			/* List of type list entries freed and available for
			   reuse. */

STATIC_THREAD a_namespace_list_entry_ptr
		avail_namespace_list_entries;
			/* List of namespace list entries freed and available
			   for reuse. */

STATIC_THREAD a_vla_fixup_ptr
		avail_vla_fixups;
			/* List of vla fixup entries freed and available for
			   reuse. */

STATIC_THREAD a_template_decl_info_ptr
		avail_template_decl_infos;
			/* List of template declaration info entries freed and
			   available for reuse. */

STATIC_THREAD a_saved_macro_state_ptr
		avail_saved_macro_states;
			/* List of saved macro state entries freed and
			   available for reuse. */

#if MICROSOFT_EXTENSIONS_ALLOWED

STATIC_THREAD a_hide_by_sig_list_entry_ptr
		avail_hide_by_sig_list_entries;
			/* List of hide-by-sig list entries freed and
			   available for reuse. */

STATIC_THREAD a_hash_table_ptr
		prop_or_event_accessor_header_hash_table;
			/* A hash table used to find previously created
			   symbol header entries that represent a combination
			   of a given property name and accessor function
			   name (as specified by their symbol header
			   pointers). */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

STATIC_THREAD a_symbol_ptr
		error_class_template_symbol;
			/* Pointer to a shared error class template entry. */

STATIC_THREAD a_symbol_ptr
		unnamed_field_symbol;
			/* Pointer to "the" unnamed field symbol, which exists
			   only for the sake of identifying a given field entry
			   as representing an unnamed field. */

STATIC_THREAD sizeof_t
		size_of_trans_unit_for_scope;
			/* Allocated size of the trans_unit_for_scope table. */

STATIC_THREAD a_boolean
		symbol_for_namespace_std_entered;
			/* TRUE when the symbol for the "std" namespace has
			   been entered into the symbol table. */

STATIC_THREAD a_boolean
		symbol_for_namespace_std_meta_entered;
			/* TRUE when the symbol for the "std::meta" namespace
			   has been entered into the symbol table. */

#define TRANS_UNIT_FOR_SCOPE_INCREMENTAL_ALLOCATION 16384
			/* Incremental allocation for the trans_unit_for_scope
			   table. */

STATIC_THREAD a_type_ptr
		size_t_type;
			/* Type for std::size_t, used for parameter type
			   checking by find_literal_operator. */

STATIC_THREAD a_type_ptr
		ptr_to_const_char_type;
			/* Type for const char*, used for parameter type
			   checking by find_literal_operator. */

void form_optionally_qualified_symbol_name(
		a_symbol_ptr				sym,
		an_il_to_str_output_control_block_ptr	octl,
		a_boolean				suppress_qualifier)
/*
Output the (possibly qualified) name of the indicated symbol.  The output
is done according to the output control block octl.  If suppress_qualifier
is TRUE, output only the final portion of the name, not any qualifier
that might normally precede it.
*/
{
  char                    *entry;
  an_il_entry_kind        kind;
  a_source_correspondence *scp;

  /* See whether the symbol has an associated IL entry and whether the IL
     entry has a source correspondence field -- but use it only if its
     parent matches that of the symbol (the parent class can differ for
     symbols promoted from anonymous unions, in which case preference is
     given to the symbol; the parent namespace can differ for extern-C
     declarations). */
  entry = il_entry_for_symbol_null_okay(sym, &kind);
  if (entry != NULL &&
      (scp = source_corresp_for_il_entry(entry, kind)) != NULL &&
      sym->is_class_member == scp->is_class_member &&
      (sym->is_class_member ?
         sym_parent_class(sym) == scp_parent_class(scp) :
         sym_parent_namespace_or_null(sym) ==
                                         scp_parent_namespace_or_null(scp))) {
    /* Use the IL entry to generate the name. */
    if (kind == iek_variable && ((a_variable*)entry)->is_this_parameter) {
      check_assertion(!octl->gen_compilable_code);
      octl->output_str("<this-param>", octl);
    } else if (suppress_qualifier) {
      form_unqualified_name(scp, kind, octl);
    } else {
      form_name(scp, kind, octl);
    }  /* if */
  } else {
    /* No source correspondence entry, or else it has a different class
       parent; use the symbol name directly. */
    if (il_header.source_language == sl_Cplusplus && !suppress_qualifier) {
      /* Put out the class or namespace qualifier on a member. */
      form_class_or_namespace_qualifier((a_boolean)sym->is_class_member,
                                        sym->parent, octl);
    }  /* if */
    octl->output_str(sym->header->identifier, octl);
  }  /* if */
}  /* form_optionally_qualified_symbol_name */


void form_symbol_name(a_symbol_ptr                          sym,
                      an_il_to_str_output_control_block_ptr octl)
/*
Output the (possibly qualified) name of the indicated symbol.  The output
is done according to the output control block octl.
*/
{
  form_optionally_qualified_symbol_name(sym, octl,
                                        /*suppress_qualifier=*/FALSE);
}  /* form_symbol_name */


#if DEBUG
/* GCOVR_EXCL_START */
#define DEBUG_LINE_LENGTH 79
/* Macros used within db_symbol, referencing local variables defined
   in that routine. */
/* put_separator appends the separator to the current line, along with a
   blank if the line still has room for sting_len additional characters;
   otherwise, it puts a new-line character and indents the next line.
   Variable col is updated in both cases. */
#define put_separator(separator, string_len)                         \
{ col += (uint32_t)(strlen(separator) + 1);                          \
  if (col + (string_len) > DEBUG_LINE_LENGTH) {                      \
    fprintf(f_debug, "%s\n%*s", (separator), (int)indentation, "");  \
    col = indentation;                                               \
  } else {                                                           \
    fprintf(f_debug, "%s ", (separator));                            \
  }  /* if */                                                        \
}  /* put_separator */


/* put_string puts out a comma separator and then writes out str.  col is
   updated. */
#define put_string(str)                                         \
{ a_const_char *local_str = (str);                              \
  put_separator(",", strlen(local_str));                        \
  fputs((local_str), f_debug);                                  \
  col += (uint32_t)strlen((local_str));                         \
}  /* put_string */

#define put_buffer_string(buf_str)                              \
{ put_separator(",", (buf_str).length());                       \
  print((buf_str), f_debug, /*end=*/"");                        \
  col += (buf_str).length();                                    \
}  /* put_buffer_string */


/* Determines whether the current line has a certain amount of room left. */
#define space_left(size)  (DEBUG_LINE_LENGTH - (size) + 1 >= col)

using a_symbol_buffer = a_string;
                        /* The type of the db_symbol buffer. */

static an_il_to_str_output_control_block
set_up_il_to_str_octl(a_symbol_buffer *buffer)
/*
Set octl so that it can be passed into the il_to_str routines to tell them
to output to the indicated buffer.
*/
{
  an_il_to_str_output_control_block octl;

  clear_il_to_str_output_control_block(&octl);
  auto output_str_func = [](a_const_char                          *str,
                            an_il_to_str_output_control_block_ptr octl_ptr) {
    ((a_symbol_buffer*)octl_ptr->text_buffer)->append(str);
  };
  octl.output_str = output_str_func;
  octl.text_buffer = (void*)buffer;
  octl.gen_pcc_code = (C_dialect == C_dialect_pcc);
  octl.debug_output = TRUE;
  return octl;
}  /* set_up_il_to_str_octl */


static a_const_char *str_access(an_access_specifier access)
/*
Return the string corresponding to the given access specifier.
*/
{
  a_const_char *s;

  switch (access) {
    case as_public:       s = "public";       break;
    case as_protected:    s = "protected";    break;
    case as_private:      s = "private";      break;
    case as_inaccessible: s = "inaccessible"; break;
    default:              s = "<bad access>"; break;
  }  /* switch */
  return s;
}  /* str_access */


/* Display an access specifier. */
#define put_access(access)                               \
{                                                        \
  put_string(str_access((an_access_specifier)(access))); \
}


static void str_type(a_symbol_buffer *buffer,
                     a_type_ptr      tp)
/*
Construct a string in buffer that represents a type.
*/
{
  an_il_to_str_output_control_block octl = set_up_il_to_str_octl(buffer);

  form_type(tp, &octl);
}  /* str_type */


static void str_qualified_name(a_symbol_buffer *buffer,
                               a_symbol_ptr    sym)
/*
Construct a string in buffer that represents a qualified name -- called
from db_symbol.
*/
{
  an_il_to_str_output_control_block octl = set_up_il_to_str_octl(buffer);

  form_symbol_name(sym, &octl);
}  /* str_qualified_name */


static void str_function_name_and_param_list(a_symbol_buffer *buffer,
                                             a_symbol_ptr    sym)
/*
Construct a string the buffer that represents a qualified name plus
function param list -- called from db_symbol.
*/
{
  an_il_to_str_output_control_block octl = set_up_il_to_str_octl(buffer);

  form_symbol_name(sym, &octl);
  if (sym->kind == (a_symbol_kind)sk_routine ||
      sym->kind == (a_symbol_kind)sk_member_function) {
    a_type_ptr tp = sym->variant.routine.ptr->type;

    tp = skip_typerefs(tp);
    form_function_declarator(tp, &octl);
  }  /* if */
}  /* str_function_name_and_param_list */


static void str_path(a_symbol_buffer    *buffer,
                     a_derivation_path  path,
                     a_const_char       *initial_string,
                     a_const_char       *separator)
/*
Construct a string in buffer that represents a derivation path -- called
from db_symbol.
*/
{
  a_derivation_step  *dsp;
  a_const_char       *sep = initial_string;

  for (dsp = path.head; dsp != path.tail->next; dsp = dsp->next) {
    a_base_class *bcp = dsp->base_class;
    a_const_char *path_part_name;

    if (bcp == NULL) {
      path_part_name = "<null bcp>";
    } else {
      a_type_ptr tp = bcp->type;

      if (tp == NULL) {
        path_part_name = "<null tp>";
      } else if (tp->source_corresp.name == NULL) {
        path_part_name = "<unnamed type>";
      } else {
        path_part_name = tp->source_corresp.name;
      }  /* if */
    }  /* if */
    buffer->append(sep, path_part_name);
    sep = separator;
  }  /* for */
}  /* str_path */


static void str_name_linkage(a_symbol_buffer         *buffer,
                             a_source_correspondence *source_corresp)
/*
Construct a string in buffer that represents a name linkage kind -- called
from db_symbol.
*/
{
  a_const_char *str =
                    name_linkage_kind_names[(int)source_corresp->name_linkage];

  buffer->reset_to(str, " linkage");
}  /* str_name_linkage */


static void db_property_or_event_suffix(a_symbol_ptr  sym)
/*
If the given symbol is for a C++/CLI accessor function, output a phrase
identifying which property or event it is for.
*/
{
  if (symbol_is(sym, sk_member_function)) {
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (cli_or_cx_enabled) {
      a_routine_ptr  rp = sym->variant.routine.ptr;
      if (rout_is_cli_accessor(rp)) {
        /* A property or event accessor: Display the property or event name. */
        a_source_correspondence    *scp;
        a_property_or_event_descr  *pedp = rp->variant.property_or_event_descr;
        if (pedp->is_static) {
          scp = &pedp->variant.variable->source_corresp;
        } else {
          scp = &pedp->variant.field->source_corresp;
        }  /* if */
        if (pedp->kind == (a_property_or_event_kind)pek_cli_event) {
          fprintf(f_debug, " for event %s", unmangled_name_of(scp));
        } else {
          fprintf(f_debug, " for property %s", unmangled_name_of(scp));
        }  /* if */
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
}  /* db_property_or_event_suffix */


void db_symbol_name(a_symbol_ptr  sym)
/*
Write out the name (including function parameters if there are any) of the
specified symbol.
*/
{
  fputs("\"", f_debug);

  a_symbol_buffer buffer;
  str_qualified_name(&buffer, sym);
  print(buffer, f_debug, /*end=*/"");
  if (sym->kind == (a_symbol_kind)sk_routine ||
      sym->kind == (a_symbol_kind)sk_member_function) {
    a_type_ptr  tp = routine_symbol_type(sym);
    if (tp != NULL) {
      a_type_qualifier_set	qualifiers;
      tp = skip_typerefs(tp);
      db_function_param_list(tp);
      qualifiers = tp->variant.routine.extra_info->qualifiers |
                   tp->variant.routine.extra_info->this_qualifiers;
      if (qualifiers != TQ_NONE) {
        fprintf(f_debug, " %s", db_qualifiers_str(qualifiers));
      }  /* if */
    }  /* if */
    db_property_or_event_suffix(sym);
  }  /* if */
  fputs("\"", f_debug);
}  /* db_symbol_name */


a_const_char *db_symbol_trans_unit(a_symbol_ptr sym)
/*
Return the name of the file for the translation unit of the indicated
symbol, if it has one and if it is not the primary translation unit.
Return NULL otherwise.  Also return NULL if sym is NULL.
*/
{
  a_const_char *name = NULL;

  if (sym != NULL && sym->decl_scope != NO_SCOPE_NUMBER) {
    a_translation_unit_ptr	tup;
    tup = trans_unit_for_scope[sym->decl_scope];
    if (tup != NULL && tup != translation_units &&
        /* The source_file pointer is not set yet early in initialization. */
        tup->source_file != NULL) {
      name = tup->source_file->name_as_written;
    }  /* if */
  }  /* if */
  return name;
}  /* db_symbol_trans_unit */


void db_symbol_name_trans_unit(a_symbol_ptr sym)
/*
Write out the symbol name (including function parameters, if any).  Include
the translation unit, if not the primary translation unit.
*/
{
  a_const_char *name;

  db_symbol_name(sym);
  name = db_symbol_trans_unit(sym);
  if (name != NULL) fprintf(f_debug, " (trans unit %s)", name); 
}  /* db_symbol_name_trans_unit */


void db_symbol(a_symbol_ptr sym,
               a_const_char *string,
               size_t       indentation)
/*
Write out information on a symbol, for debugging purposes.  sym points to
the symbol; string is an optional identifying string ("" or NULL if omitted);
and indentation is the indentation desired.
*/
{
  a_const_char    *str;
  a_symbol_buffer buffer;
  size_t          col = indentation;
  a_type_ptr      type = NULL;
  a_variable_ptr  var = NULL;
  a_boolean       suppress_newline = FALSE;
  a_symbol_ptr    apo_sym = NULL;

  if (string != NULL && strlen(string) > 0) {
    fputs(string, f_debug);
    col += strlen(string);
  }  /* if */

  if (sym == NULL) {
    /* The symbol passed by the caller is NULL. */
    fprintf(f_debug, "<NULL>");
    goto done;
  }  /* if */
  str = symbol_kind_names[(int)sym->kind];
  if (col + strlen(str) + 2 > DEBUG_LINE_LENGTH) {
    fprintf(f_debug, "\n%*s", (int)indentation, "");
    col = indentation;
  }  /* if */
  fprintf(f_debug, "<%s>", str);
  col += strlen(str) + 2;

  str_qualified_name(&buffer, sym);
  put_separator("", buffer.length() + 2);
  fprintf(f_debug, "\"%s\"", buffer.as_temp_characters());
  col += buffer.length() + 2;

  db_property_or_event_suffix(sym);
  if (sym->kind == sk_projection) {
    a_symbol_ptr fsym = sym->variant.projection.extra_info->fundamental_symbol;
    if (fsym != NULL) {
      buffer.reset_to();
      str_qualified_name(&buffer, fsym);
    }  /* if */
    put_separator("", buffer.length() + 6);
    fprintf(f_debug, "(= \"%s\")", buffer.as_temp_characters());
    col += buffer.length() + 6;
  } else if (sym->kind == sk_namespace_projection) {
    a_symbol_ptr fsym = sym->variant.namespace_projection.fundamental_symbol;
    if (fsym != NULL) {
      buffer.reset_to();
      str_qualified_name(&buffer, fsym);
      put_separator("", buffer.length() + 6);
      fprintf(f_debug, "(= \"%s\")", buffer.as_temp_characters());
      col += buffer.length() + 6;
    }  /* if */
  }  /* if */

  if (sym->decl_seq > 0) {
    buffer.reset_to("#", sym->decl_seq);
    put_separator("", buffer.length());
    print(buffer, f_debug, /*end=*/"");
    col += buffer.length();
  }  /* if */

  buffer.reset_to("(", sym->decl_position.seq, "/",
                  sym->decl_position.column, ")");
  put_separator("", buffer.length());
  print(buffer, f_debug, /*end=*/"");
  col += buffer.length();

  /* If this symbol is for a secondary translation unit, display the
     translation unit. */
  str = db_symbol_trans_unit(sym);
  if (str != NULL) {
    buffer.reset_to("trans unit ", str);
    put_buffer_string(buffer);
  }  /* if */

  /* Display information about the module entity (if relevant). */
  {
    a_source_correspondence *src_corresp =
                                          source_corresp_entry_for_symbol(sym);

    if (src_corresp != NULL && src_corresp->module_entity != NULL) {
      a_module_entity_ptr m_entity = src_corresp->module_entity;
      a_string            mep_debug = s_basic_db_mep(m_entity);

      put_buffer_string(mep_debug);
    }  /* if */
  }

  /* Display the file name (if not the primary source file) and the line
     number of the symbol declaration. */
  {
    a_const_char  *file_name;
    a_const_char  *full_name;
    a_line_number line_number;
    a_boolean     at_end_of_source;
    if (sym->decl_position.seq > 0) {
      (void)conv_seq_to_file_and_line(sym->decl_position.seq, &file_name,
                                      &full_name, &line_number,
                                      &at_end_of_source);
      if (seq_is_in_include_file(sym->decl_position.seq)) {
        buffer.reset_to("file ", file_name);
        put_buffer_string(buffer);
      }  /* if */
      if (at_end_of_source) {
        buffer.reset_to("line <end of source>");
      } else {
        buffer.reset_to("line ", line_number);
      }  /* if */
      put_buffer_string(buffer);
    }  /* if */
  }

  buffer.reset_to("scope ", sym->decl_scope);
  put_buffer_string(buffer);

  if (sym->referenced) put_string("ref'd");
  if (sym->defined) put_string("def'd");
  if (sym->ambiguous) put_string("ambig");
  if (sym->synthesized_namespace_projection) {
    put_string("synth_namespace_proj");
  }  /* if */
  if (sym->is_invisible) put_string("invisible");
  switch (sym->kind) {
    case sk_undefined:
    case sk_extern_variable:
    case sk_extern_routine:
    case sk_parameter:
      break;
    case sk_keyword:
      fprintf(f_debug, "\"%s\"",
                       token_names[(int)sym->variant.keyword.token]);
      break;
    case sk_macro:

      break;
    case sk_constant:
      fprintf(f_debug, ",\n%*s", (int)indentation, "");
      db_constant(sym->variant.constant);
      break;
    case sk_type:
      if (sym->variant.type.is_injected_class_name) {
        put_string("injected class name");
      }  /* if */
      type = sym->variant.type.ptr;
      break;
    case sk_enum_tag:
      type = sym->variant.enumeration.type;
      break;
    case sk_class_or_struct_tag:
    case sk_union_tag:
      { type = sym->variant.class_struct_union.type;
        if (type == NULL) break;

        /* The result of skip_typerefs() is copied to a temporary variable to
           work around a problem with Borland C++. */
        a_type_ptr temp_type = skip_typerefs(type);
#if MAINTAIN_NEEDED_FLAGS
        if (temp_type->variant.class_struct_union.definition_needed) {
          put_string("def needed");
        } else if (temp_type->source_corresp.needed) {
          put_string("needed");
        }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
        str_name_linkage(&buffer, &(temp_type->source_corresp));
        put_buffer_string(buffer);

        a_class_symbol_supplement_ptr  cssp =
                                    sym->variant.class_struct_union.extra_info;
        if (cssp->is_cpp03_POD) {
          put_string("C++03 POD");
        } else if (cssp->is_class_aggregate) {
          put_string("aggregate");
        }  /* if */
        if (cssp->constructor != NULL) put_string("has ctor");
        if (cssp->trivial_default_constructor != NULL) {
          put_string("has trivial default-ctor");
        }  /* if */
        if (cssp->has_nontrivial_default_constructor) {
          put_string("has default-ctor");
        }  /* if */
        if (cssp->has_copy_constructor_for_const_object) {
          put_string("has const-copy-ctor");
        } else if (cssp->has_copy_constructor) {
          put_string("has copy-ctor");
        }  /* if */
        if (cssp->destructor != NULL) put_string("has dtor");
        if (cssp->construction_by_bitwise_copy_allowed) {
          put_string("ctor bitwise copy okay");
        }  /* if */
        if (cssp->assignment_by_bitwise_copy_allowed) {
          put_string("op= bitwise copy okay");
        }  /* if */
        if (cssp->contains_vtable) {
          put_string("contains vtable");
        }  /* if */
        if (cssp->target_of_conversion_function) {
          put_string("conv target");
        }  /* if */
        if (cssp->class_template != NULL) {
          if (debug_level >= 4) put_string("has class template ptr");
        }  /* if */
        if (temp_type->variant.class_struct_union.is_template_class) {
          put_string("is instance");
        }  /* if */
        if (temp_type->variant.class_struct_union.is_nonreal_class) {
          put_string("nonreal");
        }  /* if */
        if (temp_type->variant.class_struct_union.is_prototype_instantiation) {
          put_string("prototype instantiation");
        }  /* if */
        if (cssp->any_nonreal_base_classes) {
          put_string("has nonreal base class");
        }  /* if */
        if (cssp->any_dependent_base_classes) {
          put_string("has dependent base class");
        }  /* if */
        if (cssp->any_template_dependent_fields) {
          put_string("has dependent field");
        }  /* if */
        if (class_type_supp(type)->proxy_of_type != NULL) {
          if (debug_level >= 4) put_string("has ptr for proxy");
        }  /* if */
        if (temp_type->variant.class_struct_union.
                                          contains_flexible_array_member) {
          put_string("contains flexible array member");
        }  /* if */
        if (temp_type->variant.class_struct_union.any_const_member) {
          put_string("has const member");
        }  /* if */
        if (temp_type->variant.class_struct_union.any_volatile_member) {
          put_string("has volatile member");
        }  /* if */
        if (temp_type->variant.class_struct_union.any_mutable_member) {
          put_string("has mutable member");
        }  /* if */
        if (temp_type->variant.class_struct_union.has_operator_ampersand) {
          put_string("has operator&");
        }  /* if */
        if (temp_type->declared_in_function_prototype) {
          put_string("in func prototype");
        }  /* if */
        if (temp_type->variant.class_struct_union.is_specialized) {
          buffer.reset_to(temp_type->variant.class_struct_union.
                             specialized_with_old_syntax ? "old-style " : "",
                          "specialization");
          put_buffer_string(buffer);
        }  /* if */
        if (cssp->friend_functions != NULL) {
          a_symbol_ptr  friend_sym, overload_sym, fund_sym;
          a_const_char  *sep;

          friend_sym = cssp->friend_functions;
          put_string("invisible friends =");
          buffer.reset_to("[ ");
          sep = "";
          friend_sym = cssp->friend_functions;
          overload_sym = NULL;
          do {
            if (friend_sym->kind == (a_symbol_kind)sk_overloaded_function) {
              overload_sym = friend_sym;
              friend_sym = overload_sym->variant.overloaded_function.symbols;
            }  /* if */
            fund_sym = fundamental_symbol_of(friend_sym);
            if (fund_sym->overload_set_member || overload_sym != NULL) {
              str_function_name_and_param_list(&buffer, fund_sym);
            } else {
              str_qualified_name(&buffer, fund_sym);
            }  /* if */
            friend_sym = friend_sym->next;
            if (overload_sym != NULL && friend_sym == NULL) {
              friend_sym = overload_sym->next;
              overload_sym = NULL;
            }  /* if */
            if (friend_sym == NULL) {
              buffer.append(" ]");
            }  /* if */
            put_separator(sep, buffer.length());
            print(buffer, f_debug);
            col += buffer.length();
            sep = ",";
            buffer.reset_to();
          } while (friend_sym != NULL);
        }  /* if */
      }
      break;
    case sk_field:
      if (sym->variant.field.ptr == NULL) {
        put_string("<null>");
      } else {
        a_field_ptr  fp = sym->variant.field.ptr;
        if (C_dialect == C_dialect_cplusplus) {
          put_access(fp->source_corresp.access);
        }  /* if */
        if (fp->is_mutable) {
          put_string("mutable");
        }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (fp->is_initonly) {
          put_string("initonly");
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        buffer.reset_to("offset");
        apo_sym = sym->variant.field.anonymous_parent_object;
        if (apo_sym != NULL) {
          buffer.append(" (relative to anon parent obj)");
        }  /* if */
        buffer.append(" = ", fp->offset);
        if (fp->is_bit_field) {
          buffer.append("+", (unsigned)fp->offset_bit_remainder);
          put_buffer_string(buffer);
          buffer.reset_to("size = ", (unsigned)fp->bit_size, " bit",
                          fp->bit_size == 1 ? "" : "s");
        }  /* if */
        put_buffer_string(buffer);
        if (fp->is_anonymous_parent_object) {
          put_string("is anon parent object");
        }  /* if */
        type = sym->variant.field.ptr->type;
      }  /* if */
      break;
    case sk_label:
      break;
    case sk_static_data_member:
      var = sym->variant.static_data_member.variable;
      goto do_variable;
    case sk_variable:
      var = sym->variant.variable.ptr;
do_variable:
      if (var == NULL) {
        put_string("<null>");
      } else {
        if (var->template_info != NULL &&
            var->template_info->template_arg_list != NULL) {
          db_template_arg_list(var->template_info->template_arg_list);
        }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (var->is_initonly) {
          put_string("initonly ");
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        if (sym->kind == (a_symbol_kind)sk_static_data_member) {
          put_access(var->source_corresp.access);
        }  /* if */
        buffer.reset_to("sc_",
                        db_storage_class_names[(int)var->storage_class]);
        put_buffer_string(buffer);
        str_name_linkage(&buffer, &(var->source_corresp));
        put_buffer_string(buffer);
        if (sym->value_has_been_set) put_string("set");
        if (sym->kind == (a_symbol_kind)sk_static_data_member) {
          if (var->is_template_variable) put_string("is instance");
          if (var->is_specialized) {
            buffer.reset_to(var->specialized_with_old_syntax ?
                                                "old-style " : "",
                            "specialization");
            put_buffer_string(buffer);
          }  /* if */
        } else {
#if MAINTAIN_NEEDED_FLAGS
          if (var->source_corresp.needed) put_string("needed");
#endif /* MAINTAIN_NEEDED_FLAGS */
          if (var->is_parameter || var->is_handler_param) {
            put_string((char *)(var->is_parameter ? "is param"
                                                  : "is handler param"));
            if (var->param_value_has_been_changed) put_string("changed");
            if (var->param_used_more_than_once) put_string("multiply used");
          }  /* if */
        }  /* if */
        if (var->used) put_string("used");
        if (var->is_anonymous_parent_object) {
          put_string("is anon parent object");
        }  /* if */
        type = var->type;
      }  /* if */
      break;
    case sk_member_function:
    case sk_routine:
      { a_routine_ptr rp = sym->variant.routine.ptr;

        if (rp == NULL) {
          put_string("<null>");
        } else {
          if (sym->kind == (a_symbol_kind)sk_member_function) {
            put_access(rp->source_corresp.access);
            if (rp->is_virtual) {
              buffer.reset_to("virtual (", rp->number.virtual_function, ")");
              put_buffer_string(buffer);
            }  /* if */
            if (rp->special_kind != (a_special_function_kind)sfk_none) {
              put_string(db_special_function_kinds[rp->special_kind]);
            }  /* if */
          }  /* if */
          if (rp->is_consteval) {
            put_string("consteval");
          } else if (rp->is_constexpr) {
            put_string("constexpr");
          }  /* if */
          if (rp->is_inline) put_string("inline");
          if (rp->is_deleted) put_string("=delete");
          if (rp->is_inheriting_ctor) put_string("inheriting");
          if (rp->definition_for_inlining_only) {
            put_string("def. for inlining only");
          } else if (rp->suppress_inline_body) {
            put_string("suppress inline body");
          }  /* if */
          if (rp->compiler_generated) put_string("compiler generated");
          if (rp->is_trivial_default_constructor) {
            put_string("trivial default-ctor");
          }  /* if */
          if (rp->is_trivial_copy_function) {
            put_string("trivial copy function");
          }  /* if */
          buffer.reset_to("sc_",
                          db_storage_class_names[(int)rp->storage_class]);
          put_buffer_string(buffer);
          str_name_linkage(&buffer, &(rp->source_corresp));
          put_buffer_string(buffer);
          if (rp->is_template_function) put_string("is instance");
          if (rp->is_specialized) {
            buffer.reset_to(rp->specialized_with_old_syntax ?
                                               "old-style " : "",
                            "specialization");
            put_buffer_string(buffer);
          }  /* if */
#if MAINTAIN_NEEDED_FLAGS
          if (rp->source_corresp.needed) put_string("needed");
#endif /* MAINTAIN_NEEDED_FLAGS */
          type = rp->type;
          if (C_dialect == C_dialect_cplusplus) {
            an_exception_specification_ptr       esp;
            an_exception_specification_type_ptr  estp;

            esp = (skip_typerefs(type))->variant.routine.extra_info->
                                                       exception_specification;
            if (esp == NULL || esp->throw_any) {
              if (exceptions_enabled) put_string("throws any");
            } else if (esp->indeterminate) {
              put_string("<indeterminate exn spec>");
            } else if (esp->is_noexcept) {
              put_string("noexcept");
            } else if (esp->variant.exception_specification_type_list ==
                       NULL) {
              put_string("throws none");
            } else {
              estp = esp->variant.exception_specification_type_list;
              buffer.reset_to("throws (");
              str_type(&buffer, estp->type);
              for (estp = estp->next; estp != NULL; estp = estp->next) {
                put_buffer_string(buffer);
                buffer.reset_to();
                str_type(&buffer, estp->type);
              }  /* for */
              buffer.append(")");
              put_buffer_string(buffer);
            }  /* if */
          }  /* if */
        }  /* if */
      }
      break;
    case sk_projection:
      put_access(sym->variant.projection.access);
      if (sym->variant.projection.is_using_decl) {
        put_string("using decl");
      }  /* if */
      if (sym->variant.projection.any_intervening_using_decl) {
        put_string("intervening using decl");
      }  /* if */
      { a_projection_descr_ptr pdp = sym->variant.projection.extra_info;
        a_base_class_ptr       bcp = pdp->fundamental_base_class;
        if (bcp != NULL && bcp->derivation != NULL) {
          buffer.reset_to();
          str_path(&buffer,
                   { cast_derivation_path_of(bcp),
                     bcp->derivation->path_tail },
                   "path = ==>", "==>");
          put_buffer_string(buffer);
        }  /* if */
      }
      break;
    case sk_overloaded_function:
      if (sym->variant.overloaded_function.mixed_static_nonstatic) {
        put_string ("mixed static/nonstatic");
      }  /* if */
      if (sym->variant.overloaded_function.symbols == NULL) {
        put_string("func symbols = <null>");
      } else {
        a_symbol_ptr  rtn_sym = sym->variant.overloaded_function.symbols;
        put_string("func symbols =\n");
        for (; rtn_sym != NULL; rtn_sym = rtn_sym->next) {
          fprintf(f_debug, "%*s", (int)indentation, "");
          db_symbol(rtn_sym, "", indentation + 2);
        }  /* for */
        col = 0;
        suppress_newline = TRUE;
      }  /* if */
      break;
    case sk_class_template:
    case sk_function_template:
    case sk_variable_template:
    case sk_concept_template:
      {
        a_template_symbol_supplement_ptr  tssp;
        a_template_param_ptr              tplep;
        a_symbol_ptr                      inst_sym;
        a_template_param_ptr		  templ_param_list;
        a_template_decl_info_ptr	  template_decl_info;

        tssp = sym->variant.template_info;
        if (!tssp->cache->tokens.is_empty()) {
          put_string("template body cached");
        }  /* if */
        if (sym->kind == (a_symbol_kind)sk_class_template) {
          a_symbol_ptr	prototype_sym;
          switch (tssp->variant.class_template.type_kind) {
            case tk_class:  put_string("class");           break;
            case tk_struct: put_string("struct");          break;
            case tk_union:  put_string("union");           break;
            case tk_error:  put_string("no type kind");    break;
            default:        put_string("<BAD TYPE KIND>"); break;
          }  /* switch */
          prototype_sym = tssp->variant.class_template.prototype_instantiation;
          if (prototype_sym != NULL &&
              !tssp->variant.class_template.is_alias_template) {
            /* If the prototype instantiation has a partial specialization
               template argument list (i.e., it is for a partial
               specialization), display the primary template argument list
               to identify the partial specialization. */
            a_type_ptr			prototype_type;
            a_class_type_supplement_ptr	ctsp;
            prototype_type = type_symbol_type(prototype_sym);
            ctsp = prototype_type->variant.class_struct_union.extra_info;
            if (ctsp->partial_spec_template_arg_list != NULL) {
              db_template_arg_list(ctsp->template_arg_list);
            }  /* if */
          }  /* if */
        }  /* if */
        /* Output information from the template symbol supplement. */
        if (sym->kind == (a_symbol_kind)sk_function_template) {
          template_decl_info = tssp->variant.function.decl_cache->decl_info;
        } else {
          template_decl_info = tssp->cache->decl_info;
        }  /* if */
        templ_param_list = template_decl_info != NULL ?
                                       template_decl_info->parameters : NULL;
        put_string("template parameters =\n");
        for (tplep = templ_param_list; tplep != NULL; tplep = tplep->next) {
          fprintf(f_debug, "%*s", (int)(indentation + 2), "");
          db_symbol(tplep->param_symbol, "", indentation + 4);
          switch (tplep->param_symbol->kind) {
            case sk_type:
              fprintf(f_debug, "%*sparameter type: ",
                      (int)(indentation + 4), "");
              /* Display the proxy class type if one exists. */
              if (tplep->variant.type != NULL) {
                a_type_ptr ptype = tplep->variant.type;
                db_type(ptype);
                if (ptype->variant.template_param.extra_info != NULL) {
                  a_type_ptr  class_type;
                  class_type =
                          ptype->variant.template_param.extra_info->class_type;
                  if (class_type != NULL) {
                    fprintf(f_debug, "\n%*sproxy class: ",
                            (int)(indentation + 6), "");
                    db_type(class_type);
                  }  /* if */
                }  /* if */
              } else {
                fprintf(f_debug, "NULL");
              }  /* if */
              if (tplep->has_default_arg) {
		if (!tplep->def_arg_involves_template_param) {
		  put_string("= ");
		  db_type(tplep->default_arg.type);
		} else {
		  put_string("= <token cache>");
		}  /* if */
	      }  /* if */
              break;
            case sk_constant:
              fprintf(f_debug, "%*sparameter constant: ",
                      (int)(indentation + 4), "");
              if (tplep->variant.constant.ptr != NULL) {
                db_constant(tplep->variant.constant.ptr);
              } else {
                fprintf(f_debug, "NULL");
              }  /* if */
              if (tplep->has_default_arg) {
		if (!tplep->def_arg_involves_template_param) {
		  put_string("= ");
		  db_constant(tplep->default_arg.constant);
		} else {
		  put_string("= <token cache>");
		}  /* if */
	      }  /* if */
              break;
            case sk_class_template:
              fprintf(f_debug, "%*sparameter template: ",
                      (int)(indentation + 4), "");
              if (tplep->variant.templ != NULL) {
                a_template_ptr	templ_ptr;
                templ_ptr = tplep->variant.templ->il_template_entry;
                db_symbol((a_symbol_ptr)templ_ptr->source_corresp.assoc_info,
                          "", indentation + 4);
              } else {
                fprintf(f_debug, "NULL");
              }  /* if */
              break;
            default:
              fprintf(f_debug, "<BAD TEMPLATE PARAM SYMBOL KIND>");
          }  /* if */
          fprintf(f_debug, "\n");
          col = 0;
        }  /* for */
        if (sym->kind == (a_symbol_kind)sk_class_template) {
          a_symbol_list_entry_ptr	slep;
          /* Display the prototype instantiation. */
          inst_sym = tssp->variant.class_template.prototype_instantiation;
          if (inst_sym != NULL) {
            fprintf(f_debug, "%*sprototype instantiation:\n", (int)indentation,
                    "");
            fprintf(f_debug, "%*s", (int)(indentation + 2), "");
            db_symbol(inst_sym, "", indentation + 4);
          }  /* if */
          /* Display any partial specializations. */
          inst_sym = tssp->partial_specializations;
          while (inst_sym != NULL) {
            fprintf(f_debug, "%*spartial specialization:\n",
                    (int)indentation, "");
            fprintf(f_debug, "%*s", (int)(indentation + 2), "");
            db_symbol(inst_sym, "", indentation + 4);
            inst_sym = inst_sym->next;
          }  /* while */
          /* Display instantiations based on this template. */
          slep = tssp->variant.class_template.instantiations;
          while (slep != NULL) {
            fprintf(f_debug, "%*sinstantiation:\n", (int)indentation, "");
            fprintf(f_debug, "%*s", (int)(indentation + 2), "");
            db_symbol(slep->symbol, "", indentation + 4);
            slep = slep->next;
          }  /* while */
        } else if (sym->kind == (a_symbol_kind)sk_function_template) {
          a_routine_ptr            routine = tssp->variant.function.routine;
          a_template_instance_ptr  tip;

          fprintf(f_debug, "%*sroutine type: ", (int)indentation, "");
          if (routine != NULL) {
            db_type(tssp->variant.function.routine->type);
          } else {
            fprintf(f_debug, "(routine ptr is NULL)");
          }  /* if */
          fprintf(f_debug, "\n");
          tip = tssp->variant.function.instantiations;
          while (tip != NULL) {
            fprintf(f_debug, "%*sinstance", (int)indentation, "");
            if (tip->instance_sym == NULL) {
              fputs(": NULL instance sym\n", f_debug);
            } else {
              a_routine_ptr inst_rp = tip->instance_sym->variant.routine.ptr;
              if (tip->instantiation_required || tip->is_guiding_decl ||
                  inst_rp->is_specialized) {
                a_const_char* comma = "";
                fputs(" (", f_debug);
                if (tip->instantiation_required) {
                  fputs("instantiation req'd", f_debug);
                  comma = ", ";
                }  /* if */
                if (tip->is_guiding_decl) {
                  fprintf(f_debug, "%sguiding decl", comma);
                  comma = ", ";
                }  /* if */
                if (inst_rp->is_specialized) {
                  fprintf(f_debug, "%s%sspecialization", comma,
                          inst_rp->specialized_with_old_syntax ?
                                                            "old-style " : "");
                }  /* if */
                fputc(')', f_debug);
              }  /* if */
              fputs(":\n", f_debug);
              fprintf(f_debug, "%*s", (int)(indentation + 2), "");
              db_symbol(tip->instance_sym, "", indentation + 4);
            }  /* if */
            tip = tip->next;
          }  /* while */
        } else if (sym->kind == (a_symbol_kind)sk_variable_template) {
          /* Display the prototype instantiation. */
          var = tssp->variant.variable.prototype_variable;
          inst_sym = symbol_for(var);
          if (inst_sym != NULL) {
            fprintf(f_debug, "%*sprototype instantiation:\n", (int)indentation,
                    "");
            fprintf(f_debug, "%*s", (int)(indentation + 2), "");
            db_symbol(inst_sym, "", indentation + 4);
          }  /* if */
        } else if (sym->kind == (a_symbol_kind)sk_concept_template) {
        }  /* if */
        col = 0;
        suppress_newline = TRUE;
      }
      break;
    case sk_namespace:
      break;
    case sk_namespace_projection:
      break;
    case sk_named_module:
      break;
#if NAMED_ADDRESS_SPACES_ALLOWED
    case sk_named_address_space:
      fprintf(f_debug, " (id = %d)", (int)sym->variant.named_address_space.id);
      break;
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
#if NAMED_REGISTERS_ALLOWED
    case sk_named_register:
      fprintf(f_debug, " (id = %d)", (int)sym->variant.named_register.id);
      break;
#endif /* NAMED_REGISTERS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case sk_property_set:
      if (sym->variant.property_info->properties == NULL) {
        put_string("properties = <null>");
      } else {
        a_symbol_ptr  property_sym = sym->variant.property_info->properties;
        put_string("properties =\n");
        for (; property_sym != NULL; property_sym = property_sym->next) {
          fprintf(f_debug, "%*s", (int)indentation, "");
          db_symbol(property_sym, "", indentation + 2);
        }  /* for */
        col = 0;
        suppress_newline = TRUE;
      }  /* if */
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    default:
      put_string("UNEXPECTED SYMBOL KIND");
      break;
  }  /* switch */
  if (type != NULL) {
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
    if (type->use_cfront_transitional_nested_type_name_mangling) {
      put_string("semivisible");
    }  /* if */
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
    if (type->kind == (a_type_kind)tk_typeref &&
        type->variant.typeref.type == NULL) {
      /* The type may still be under construction (e.g., if this is the symbol
         for an alias template).  Don't try to test the type with functions,
         like is_function_type, that will perform a skip_typerefs. */
      fprintf(f_debug, ",\n%*stype = ", (int)indentation, "");
    } else if (!space_left(20) ||
               (!space_left(30) && is_array_type(type)) ||
               is_function_type(type) ||
               (is_any_ptr_or_ref_type(type) &&
                ((is_array_type(type_pointed_to(type)) && !space_left(35)) ||
                 is_function_type(type_pointed_to(type)))) ||
               (!space_left(55) && is_template_class_type(type))) {
      fprintf(f_debug, ",\n%*stype = ", (int)indentation, "");
    } else {
      fputs(", type = ", f_debug);
    }  /* if */
    if (sym->kind == (a_symbol_kind)sk_class_or_struct_tag ||
        sym->kind == (a_symbol_kind)sk_union_tag) {
      db_type(type);
    } else {
      db_abbreviated_type(type);
    }  /* if */
    suppress_newline = FALSE;
  }  /* if */
  if (apo_sym != NULL) {
    if (!suppress_newline) (void)fputc('\n', f_debug);
    fprintf(f_debug, "%*s", (int)indentation, "");
    buffer.reset_to("- anon parent object [", (void *)apo_sym, "]: ");
    db_symbol(apo_sym, buffer.as_temp_characters(), indentation + 2);
    suppress_newline = TRUE;
  }  /* if */
done:
  /* Recursive calls to db_symbol can create unwanted newlines in the
     output.  Don't output a newline if the last thing we did was
     a call to db_symbol. */
  if (!suppress_newline) (void)fputc('\n', f_debug);
  if (var != NULL) {
    db_initializer(var, indentation);
  }  /* if */
}  /* db_symbol */


void db_sym(a_symbol_ptr sym)
/*
A short-hand version of db_symbol for convenient access from a debugger.
*/
{
  db_symbol(sym, "", 2);
}  /* db_sym */


void db_template_parameter(a_template_param_ptr	tpp)
/*
Display a template parameter, for debugging purposes.
*/
{
  size_t indentation = 2;
  db_symbol(tpp->param_symbol, "", indentation + 4);
  switch (tpp->param_symbol->kind) {
    case sk_type:
      fprintf(f_debug, "%*sparameter type: ", (int)(indentation + 4), "");
      /* Display the proxy class type if one exists. */
      if (tpp->variant.type != NULL) {
        a_type_ptr ptype = tpp->variant.type;
        db_type(ptype);
        if (ptype->variant.template_param.extra_info != NULL) {
          a_type_ptr  class_type;
          class_type =
                  ptype->variant.template_param.extra_info->class_type;
          if (class_type != NULL) {
            fprintf(f_debug, "\n%*sproxy class: ",
                    (int)(indentation + 6), "");
            db_type(class_type);
          }  /* if */
        }  /* if */
      } else {
        fprintf(f_debug, "NULL");
      }  /* if */
      if (tpp->has_default_arg) {
	if (!tpp->def_arg_involves_template_param) {
	  fprintf(f_debug, "%s", "= ");
	  db_type(tpp->default_arg.type);
	} else {
	  fprintf(f_debug, "%s", "= <token cache>");
	}  /* if */
      }  /* if */
      break;
    case sk_constant:
      fprintf(f_debug, "%*sparameter constant: ", (int)(indentation + 4), "");
      if (tpp->variant.constant.ptr != NULL) {
        db_constant(tpp->variant.constant.ptr);
      } else {
        fprintf(f_debug, "NULL");
      }  /* if */
      if (tpp->has_default_arg) {
	if (!tpp->def_arg_involves_template_param) {
	  fprintf(f_debug, "%s", "= ");
	  db_constant(tpp->default_arg.constant);
	} else {
	  fprintf(f_debug, "%s", "= <token cache>");
	}  /* if */
      }  /* if */
      break;
    case sk_class_template:
      fprintf(f_debug, "%*sparameter template: ", (int)(indentation + 4), "");
      if (tpp->variant.templ != NULL) {
        a_template_ptr	templ_ptr;
        templ_ptr = tpp->variant.templ->il_template_entry;
        db_symbol((a_symbol_ptr)templ_ptr->source_corresp.assoc_info, "",
                  indentation + 4);
      } else {
        fprintf(f_debug, "NULL");
      }  /* if */
      break;
    default:
      fprintf(f_debug, "<BAD TEMPLATE PARAM SYMBOL KIND>");
  }  /* if */
  fprintf(f_debug, "\n");
}  /* db_template_parameter */


void db_template_param_list(a_template_param_ptr	tpp)
/*
Display a template parameter list, for debugging purposes.
*/
{
  int count = 1;
  for (; tpp != NULL; tpp = tpp->next, count++) {
    fprintf(f_debug, "Template parameter %d:\n", count);
    db_template_parameter(tpp);
  }  /* for */
}  /* db_template_param_list */


void db_tpp(a_template_param_ptr  tpp)
/*
Shorthand for db_template_param_list.
*/
{
  db_template_param_list(tpp);
}  /* db_tpp */

/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

void set_source_corresp_name(a_source_correspondence	*sc,
			     a_symbol_header_ptr	sym_header)
/*
Set the name information in the source correspondence entry based on the
information in sym_header.
*/
{
#if NEED_NAME_MANGLING
  if (sc->name_has_been_mangled) {
    /* If using a PCH file, it's possible that this name has already been
       mangled; if so, no action is necessary. */
    check_assertion(sym_header->identifier ==
                                      sc->unmangled_name_or_mangled_encoding ||
                    strcmp(sym_header->identifier,
                           sc->unmangled_name_or_mangled_encoding) == 0);
  } else
#endif /* NEED_NAME_MANGLING */
  /* Do not insert code here. */
  if (!sym_header->is_unnamed) {
    /* Note that the identifier name was allocated in the intermediate language
       memory area (see find_symbol); it can therefore be used without
       copying. */
    sc->name = sym_header->identifier;
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  sc->microsoft_identifier_used = sym_header->microsoft_identifier_used;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* set_source_corresp_name */


void clear_source_corresp_name(a_source_correspondence	*sc)
/*
Clear the name information in the source correspondence entry.
*/
{
  sc->name = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  sc->microsoft_identifier_used = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* clear_source_corresp_name */


void set_source_corresp(a_source_correspondence *sc,
                        a_symbol_ptr            sp)
/*
Set the given source correspondence to point to the given symbol.  The
scope for the symbol must still be active.
*/
{
  a_boolean  is_local_to_function = FALSE;

  sc->assoc_info = (char *)sp;
  if (sp->header == unnamed_tag_symbol_header) {
    /* Let the name pointer in the IL entry remain NULL. */
  } else {
    /* Set the name based on the information in the symbol header. */
    set_source_corresp_name(sc, sp->header);
  }  /* if */
  if (sc->decl_position.seq != 0) {
    /* The decl-position is already set, so this must be a resetting of
       the source correspondence.  Let the caller decide whether and how the
       current value should be overwritten. */
  } else {
    /* Set the source position. */
    sc->decl_position = sp->decl_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    if (sc->decl_pos_info != NULL) {
      /* If decl_position is being reset, assume that the related source
         position information (which should be tied to the same declaration)
         has been invalidated. */
      clear_decl_position_supplement(sc->decl_pos_info);
    } else if (sp->decl_position.seq != 0) {
      /* Create a decl-position-supplement for this entry. */
      sc->decl_pos_info = alloc_decl_position_supplement(in_file_scope(sc));
    }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  }  /* if */
  /* Clear the referenced flag.  It was set to TRUE in
     set_default_source_corresp, so that unassociated entities will
     all have the referenced flag set.  Here it's cleared, now that we
     know this is an entity with some corresponding entity in the
     source program.  The flag will later be set to TRUE again if
     there is an actual reference. */
  sc->referenced = FALSE;
#if RECORD_SCOPE_DEPTH_IN_IL
  /* Record the scope depth of the declaration of this entity in the source
     correspondence. */
  sc->scope_depth = scope_depth_of_symbol(sp, &is_local_to_function);
#else /* RECORD_SCOPE_DEPTH_IN_IL */
  (void)scope_depth_of_symbol(sp, &is_local_to_function);
#endif /* RECORD_SCOPE_DEPTH_IN_IL */
  /* Set the is_local_to_function flag. */
  sc->is_local_to_function = is_local_to_function;
}  /* set_source_corresp */


void set_class_membership(a_symbol_ptr             sym,
                          a_source_correspondence  *scp,
                          a_type_ptr               class_type)
/*
Set the is_class_member and parent.class_type fields of the indicated
symbol and source-correspondence entries.  class_type may be NULL to
indicate that the entity is not actually a member.  If sym or scp is
NULL (which can happen,. e.g., with anonymous unions, unnamed fields, or
fields generated by IL lowering) ignore the entity.
*/
{
  if (class_type != NULL) {
    if (class_type->kind == (a_type_kind)tk_template_param) {
      /* If the parent is a template parameter, use the associated proxy class
         as the parent type. */
      class_type = proxy_class_for_template_param(class_type);
    }  /* if */
    check_assertion(is_immediate_class_type(class_type));

    a_scope_ptr  parent_scope = class_type_supp(class_type)->assoc_scope;
    if (sym != NULL) {
      sym->is_class_member = TRUE;
      sym->parent.class_type = class_type;
      if (parent_scope != NULL) {
        sym->decl_scope = parent_scope->number;
      } else {
        expect_error();
      }  /* if */
    }  /* if */
    if (scp != NULL) {
      if (parent_scope != NULL) {
        scp->is_class_member = TRUE;
        scp->parent_scope = parent_scope;
      } else {
        expect_error();
      }  /* if */
    }  /* if */
  }  /* if */
}  /* set_class_membership */


void set_namespace_membership(a_symbol_ptr             sym,
                              a_source_correspondence  *scp,
                              a_namespace_ptr          nsp)
/*
Clear is_class_member and set parent.namespace_ptr for the indicated symbol
and source-correspondence entries.  If nsp is NULL use the current scope
depth to determine the namespace.  If sym or scp is NULL, ignore the entity.
*/
{
  a_scope_stack_entry_ptr  ssep;

  if (nsp == NULL) {
    if (decl_scope_level > DEPTH_OF_FILE_SCOPE &&
        decl_scope_level <= depth_innermost_namespace_scope) {
      ssep = &scope_stack[decl_scope_level];
      check_assertion_str(ssep->il_scope != NULL &&
                          ssep->il_scope->kind == (a_scope_kind)sck_namespace,
                          "set_namespace_membership: unexpected scope kind");
      nsp = ssep->il_scope->variant.assoc_namespace;
    }  /* if */
  } else {
    /* If the namespace supplied is an alias, get the "base" namespace. */
    while (nsp->is_namespace_alias) nsp = nsp->variant.assoc_namespace;
  }  /* if */
  if (nsp != NULL) {
    a_scope_ptr scope = nsp->variant.assoc_scope;

    if (sym != NULL) {
      sym->is_class_member = FALSE;
      sym->parent.namespace_ptr = nsp;
      sym->decl_scope = scope->number;
    }  /* if */
    if (scp != NULL) {
      scp->is_class_member = FALSE;
      scp->parent_scope = scope;
    }  /* if */
  }  /* if */
}  /* set_namespace_membership */


void set_membership_in_source_corresp(a_source_correspondence  *scp,
                                      a_symbol_ptr             sym)
/*
Set the class/namespace membership information in the source correspondence
information pointed to by scp based on the membership information in the
symbol entry pointed to by sym.
*/
{
  if (sym->is_class_member) {
    set_class_membership((a_symbol_ptr)NULL, scp, sym_parent_class(sym));
  } else if (sym_is_namespace_member(sym)) {
    set_namespace_membership((a_symbol_ptr)NULL, scp,
                             sym_parent_namespace(sym));
  }  /* if */
}  /* set_membership_in_source_corresp */


void set_source_corresp_with_scope_depth(a_source_correspondence  *sc,
                                         a_symbol_ptr             sp,
                                         ARG_UNUSED a_scope_depth depth)
/*
Set the source correspondence to point to a given symbol for which
the scope is not still active.  This routine works by temporarily
changing the scope of the symbol to NO_SCOPE_NUMBER and calling
set_source_corresp.  The scope number is set to its original value
and the scope depth is set to the value passed by the caller.
*/
{
  a_scope_depth		saved_scope_number;

  saved_scope_number = sp->decl_scope;
  sp->decl_scope = NO_SCOPE_NUMBER;
  set_source_corresp(sc, sp);
  sp->decl_scope = saved_scope_number;
#if RECORD_SCOPE_DEPTH_IN_IL
  sc->scope_depth = depth;
#endif /* RECORD_SCOPE_DEPTH_IN_IL */
}  /* set_source_corresp_with_scope_depth */


a_special_function_kind special_function_kind_for_symbol(a_symbol_ptr	sym)
/*
Return the special function kind of the routine associated with sym.
If sym is not a routine, or is not a special function, return sfk_none.
*/
{
  a_special_function_kind	kind;

  switch (sym->kind) {
    case sk_routine:
    case sk_member_function:
      kind = sym->variant.routine.ptr->special_kind;
      break;
    case sk_overloaded_function:
      /* All entries on a list of overloaded functions should have the same
         special function kind, so looking at the first on the list is
         sufficient. */
      sym = sym->variant.overloaded_function.symbols;
      kind = special_function_kind_for_symbol(sym);
      break;
    case sk_function_template:
      kind = sym->variant.template_info->
                                        variant.function.routine->special_kind;
      break;
    default:
      kind = (a_special_function_kind)sfk_none;
  }  /* switch */
  return kind;
}  /* special_function_kind_for_symbol */


a_type_ptr underlying_function_type(a_symbol_ptr  sym)
/*
If the given symbol refers to a function, a typedef, a variable or a data
member, extract its type and see if it is a function type or a type composed
from a function type.  If so, return the underlying function type; otherwise,
return NULL.
*/
{
  a_type_ptr  result;

  /* First extract a type pointer: */
  switch (sym->kind) {
    case sk_routine:
    case sk_member_function:
      result = sym->variant.routine.ptr->type;
      break;
    case sk_function_template:
      result = sym->variant.template_info->variant.function.routine->type;
      break;
    case sk_variable:
      result = sym->variant.variable.ptr->type;
      break;
    case sk_type:
      result = sym->variant.type.ptr;
      break;
    case sk_static_data_member:
      result = sym->variant.static_data_member.variable->type;
      break;
    case sk_field:
      result = sym->variant.field.ptr->type;
      break;
    default:
      result = NULL;
  }  /* switch */
  /* Now peel off pointer and reference operators until we find a routine
     type (if at all): */
  while (result != NULL && !is_function_type(result)) {
    result = skip_typerefs(result);
    if (is_any_ptr_or_ref_type(result)) {
      result = type_pointed_to(result);
    } else if (is_ptr_to_member_type(result)) {
      result = pm_member_type(result);
    } else if (is_array_type(result)) {
      result = array_element_type(result);
    } else if (!is_function_type(result)) {
      result = NULL;
    }  /* if */
  }  /* while */
  return result;
}  /* underlying_function_type */


a_boolean overload_set_contains_template(a_symbol_ptr sym)
/*
Return TRUE if sym points to an overload set containing a function
template symbol.
*/
{
  a_boolean	result = FALSE;

  check_assertion(sym->kind == (a_symbol_kind)sk_overloaded_function);
  for (sym = sym->variant.overloaded_function.symbols;
       sym != NULL && !result; sym = sym->next) {
    a_symbol_ptr	fund_sym;
    fund_sym = fundamental_symbol_of(sym);
    if (fund_sym->kind == (a_symbol_kind)sk_function_template) result = TRUE;
  }  /* for */
  return result;
}  /* overload_set_contains_template */


a_boolean sym_may_include_nonstatic_member_function(a_symbol_ptr  sym)
/*
If sym potentially represents at least one nonstatic member function, return
TRUE.  Otherwise, return FALSE.  sym may represent an overload set or a single
entity.
*/
{
  a_boolean  result = FALSE;

  sym = fundamental_symbol_of(sym);
  if (symbol_is(sym, sk_member_function)) {
    a_routine_ptr  rp = sym->variant.routine.ptr;
    if (routine_type_is_nonstatic_member_function(rp->type)) {
      result = TRUE;
    }  /* if */
  } else if (symbol_is(sym, sk_function_template)) {
    a_routine_ptr  rp = sym->variant.template_info->variant.function.routine;
    if (routine_type_is_nonstatic_member_function(rp->type)) {
      result = TRUE;
    }  /* if */
  } else if (symbol_is(sym, sk_overloaded_function)) {
    sym = sym->variant.overloaded_function.symbols;
    for (; sym != NULL; sym = sym->next) {
      a_symbol_ptr  fund_sym = fundamental_symbol_of(sym);
      if (sym_may_include_nonstatic_member_function(fund_sym)) {
        result = TRUE;
        break;
      }  /* if */
    }  /* for */
  } else if (symbol_is(sym, sk_constant)) {
    a_constant_ptr  cp = sym->variant.constant;
    if (constant_is(cp, ck_template_param)) {
      if (tpck_is(cp, tpck_address)) {
        cp = cp->variant.template_param.variant.constant;
      }  /* if */
      if (tpck_is(cp, tpck_member) ||
          tpck_is(cp, tpck_unknown_function) ||
          tpck_is(cp, tpck_template_ref) ||
          tpck_is(cp, tpck_destructor)) {
        result = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* sym_may_include_nonstatic_member_function */


a_boolean is_proxy_member_symbol(a_symbol_ptr  sym)
/*
Return TRUE if the symbol sym refers to a hypothetical member of a proxy
class.  (E.g., the symbol returned for T::f, where T is a template parameter.
*/
{
  a_boolean  result = FALSE;
  if (sym->kind == (a_symbol_kind)sk_constant) {
    a_constant_ptr  constant = sym->variant.constant;
    if (constant != NULL &&
        constant->kind == (a_constant_repr_kind)ck_template_param &&
        (constant->variant.template_param.kind ==
                                (a_template_param_constant_kind)tpck_member ||
         constant->variant.template_param.kind ==
                           (a_template_param_constant_kind)tpck_destructor)) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* is_proxy_member_symbol */


a_boolean f_symbol_is_pack(a_symbol_ptr	sym)
/*
Return TRUE if sym is a template parameter pack or function parameter pack.
*/
{
  a_boolean	result = FALSE;

  switch (sym->kind) {
    case sk_type:
      /* For a type, check for a template parameter type that is marked as
         a pack, but don't look through typedefs. */
      { a_type_ptr  tp = sym->variant.type.ptr;
        check_assertion(tp != NULL);
        if (!type_is(tp, tk_typeref) || !typeref_is_typedef(tp)) {
          tp = skip_typerefs(tp);
          result = type_is_pack(tp);
        }  /* if */
      }
      break;
    case sk_constant:
      /* For a constant, check for a template parameter constant that is
         marked as a pack. */
      { a_constant_ptr	cp = sym->variant.constant;
        result = constant_is_pack(cp);
      }
      break;
    case sk_class_template:
      /* For a class template, check for a template template parameter
         marked as a pack. */
      { a_template_symbol_supplement_ptr	tssp;
        a_template_ptr				templ;
        /* If this is a template template parameter, replace the template
           symbol with the one referred to by the parameter. */
        sym = template_argument_if_template_template_param(sym);
        tssp = sym->variant.template_info;
        templ = tssp->il_template_entry;
        result = templ->is_pack;
      }
      break;
    case sk_variable:
      /* For variables, check for a parameter variable marked as a parameter
         pack. */
      result = sym->variant.variable.ptr->is_pack;
      break;
    case sk_parameter:
      result = sym->variant.param_id->is_parameter_pack;
      break;
    case sk_field:
      /* For fields, an init-capture that is a pack expansion is a pack. */
      { a_field_ptr	fp = sym->variant.field.ptr;
        result = fp->is_init_capture && fp->is_captured_pack_element;
      }
      break;
    default:
      break;
  }  /* switch */
  return result;
}  /* f_symbol_is_pack */


static a_symbol_header_ptr alloc_symbol_header(void)
/*
Allocate a new symbol header, and return a pointer to it.
*/
{
  a_symbol_header_ptr ptr;

  db_enter(5, "alloc_symbol_header");

  ptr = (a_symbol_header_ptr)alloc_fe(sizeof(a_symbol_header));
#if DEBUG
/* GCOVR_EXCL_START */
  num_symbol_headers_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  ptr->next              = NULL;
  ptr->identifier        = NULL;
  ptr->identifier_length = 0;
  ptr->symbol            = NULL;
  ptr->inactive_symbols  = NULL;
  ptr->other_symbols     = NULL;
  ptr->saved_macro_stack = NULL;
  ptr->hash_value        = 0;
  ptr->deferred_module_entries = NULL;
  ptr->variant.opname    = (an_opname_kind)onk_none;
  ptr->is_unnamed        = FALSE;
  ptr->has_intrinsic_name = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  ptr->microsoft_identifier_used = FALSE;
  ptr->is_cli_operator = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  ptr->any_nested_types_on_inactive_list = FALSE;
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
  ptr->has_cfront_transitional_nested_type_mangled_name = FALSE;
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
#if RECORD_HIDDEN_NAMES_IN_IL
  ptr->any_tag_decl = FALSE;
  ptr->any_decl_in_file_or_namespace_scope = FALSE;
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
  ptr->any_function_referenced_in_dependent_call = FALSE;
#if UNICODE_VULNERABILITY_DETECTION_SUPPORTED
  ptr->id_added_to_map = FALSE;
#endif /* UNICODE_VULNERABILITY_DETECTION_SUPPORTED */
#if PRAGMA_WEAK_ALLOWED
  ptr->named_in_weak_pragma = FALSE;
#endif /* PRAGMA_WEAK_ALLOWED */
#if BUILTIN_FUNCTIONS_ENABLED
  ptr->is_builtin_function = FALSE;
  ptr->is_builtin_overloadable = FALSE;
  ptr->is_builtin_overload_set = FALSE;
  ptr->is_builtin_deferred = FALSE;
  ptr->builtin_has_been_loaded = FALSE;
  ptr->builtin_function_category = bfc_none;
  ptr->builtin_function_index = 0;
#endif /* BUILTIN_FUNCTIONS_ENABLED */
  db_exit();

  return ptr;
}  /* alloc_symbol_header */


static a_conversion_header_ptr alloc_conversion_header(void)
/*
Allocate a new conversion header and return a pointer to it.
*/
{
  a_conversion_header_ptr ptr;

  db_enter(5, "alloc_conversion_header");
  ptr = (a_conversion_header_ptr)alloc_fe(sizeof(a_conversion_header));
#if DEBUG
/* GCOVR_EXCL_START */
  num_conversion_headers_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  ptr->next          = NULL;
  ptr->symbol_header = NULL;
  ptr->type          = NULL;
  
  db_exit();
  return ptr;
}  /* alloc_conversion_header */


static a_literal_operator_header_ptr alloc_literal_operator_header(
                                                       a_const_char *suffix,
                                                       sizeof_t     suffix_len)
/*
Allocate a new literal operator header, initialize its fields with the
values provided, link it to the list of literal operator headers, and
return a pointer to it.  suffix is not assumed to be null-terminated, and
suffix_len does not include a null terminator; however, for convenience, a
null terminator is added to the header's copy of the string, even though it
is not needed by make_literal_opname_locator.
*/
{
  a_literal_operator_header_ptr ptr;

  ptr = (a_literal_operator_header_ptr)
                                   alloc_fe(sizeof(a_literal_operator_header));
#if DEBUG
/* GCOVR_EXCL_START */
  ++num_literal_operator_headers_allocated;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  ptr->next = literal_operator_header_list;
  literal_operator_header_list = ptr;
  ptr->symbol_header = NULL;
  ptr->suffix = (a_const_char *)alloc_fe(suffix_len + 1);
  memcpy((char *)ptr->suffix, suffix, suffix_len);
  ((char *)ptr->suffix)[suffix_len] = '\0';
  ptr->suffix_len = suffix_len;
  return ptr;
}  /* alloc_literal_operator_header */


a_substituted_type_list_entry_ptr alloc_substituted_type_list_entry(void)
/*
Allocate a new type list entry and return a pointer to it.
*/
{
  a_substituted_type_list_entry_ptr ptr;

  ptr = (a_substituted_type_list_entry_ptr)
                               alloc_fe(sizeof(a_substituted_type_list_entry));
#if DEBUG
/* GCOVR_EXCL_START */
  num_substituted_type_list_entries_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  ptr->next = NULL;
  ptr->templ_arg_list = NULL;
  ptr->options = CTWS_NO_OPTIONS;
  ptr->type = NULL;
  return ptr;
}  /* alloc_substituted_type_list_entry */


a_symbol_list_entry_ptr alloc_symbol_list_entry(void)
/*
Allocate a new symbol list entry and return a pointer to it.
*/
{
  a_symbol_list_entry_ptr ptr;

  db_enter(5, "alloc_symbol_list_entry");
  if (avail_symbol_list_entries != NULL) {
    /* Reuse an existing entry. */
    ptr = avail_symbol_list_entries;
    avail_symbol_list_entries = avail_symbol_list_entries->next;
  } else {
    /* Allocate a new entry. */
    ptr = (a_symbol_list_entry_ptr)alloc_fe(sizeof(a_symbol_list_entry));
#if DEBUG
/* GCOVR_EXCL_START */
   num_symbol_list_entries_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  }  /* if */
  ptr->next    = NULL;
  ptr->symbol  = NULL;
  db_exit();
  return ptr;
}  /* alloc_symbol_list_entry */


void free_list_of_symbol_list_entries(a_symbol_list_entry_ptr slep)
/*
Add a list of symbol list entries to the available list.  slep may
be NULL, in which case nothing is done.
*/
{
  a_symbol_list_entry_ptr	slep_tail;
  if (slep != NULL) {
    /* Find the last entry on the list. */
    slep_tail = slep;
    while (slep_tail->next != NULL) slep_tail = slep_tail->next;
    /* Add the current available list to the end of the list passed by the
       caller. */
    slep_tail->next = avail_symbol_list_entries;
    avail_symbol_list_entries = slep;
  }  /* if */
}  /* free_list_of_symbol_list_entries */


static void set_identifier_for_symbol_header(
					a_symbol_header_ptr	hdr_ptr,
					a_const_char		*string,
					sizeof_t		length,
					a_boolean		is_unnamed)
/*
Make a copy of the specified string, whose length is specified by "length", in
the primary file scope memory region.  Set the symbol header specified by
hdr_ptr to use the resulting string and length.  is_unnamed is TRUE if the
header is for an unnamed entity (in which case the string is a placeholder
value such as <unnamed>).
*/
{
  char		*new_string;

  new_string = alloc_primary_file_scope_il((sizeof_t)(length + 1));
  memcpy(new_string, string, size_t_arg(length));
  /* Terminate the string. */
  new_string[length] = '\0';
  hdr_ptr->identifier = new_string;
  hdr_ptr->identifier_length = length;
  hdr_ptr->is_unnamed = is_unnamed;
#if DEBUG
/* GCOVR_EXCL_START */
  symbol_name_string_space += (unsigned long)(length + 1);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
}  /* set_identifier_for_symbol_header */


#if MICROSOFT_EXTENSIONS_ALLOWED

/*
Entry used to build a hash table for mapping a property or event and
one of its accessors to a given symbol header.
*/
typedef struct a_prop_or_event_accessor_header_lookup
			*a_prop_or_event_accessor_header_lookup_ptr;
typedef struct a_prop_or_event_accessor_header_lookup {
  a_symbol_header_ptr	property_or_event_header;
				/* The symbol header associated with the
				   property or event. */
  a_symbol_header_ptr	accessor_header;
				/* The symbol header associated with the
				   accessor function. */
  a_symbol_header_ptr	combined_header;
				/* The symbol header that represents the
				   combination of the two headers above. */
} a_prop_or_event_accessor_header_lookup;


static void clear_prop_or_event_accessor_header_lookup(
			a_prop_or_event_accessor_header_lookup_ptr peahlp)
/*
Initialize the fields of a property or event accessor header lookup entry.
*/
{
  peahlp->property_or_event_header = NULL;
  peahlp->accessor_header = NULL;
  peahlp->combined_header = NULL;
}  /* clear_prop_or_event_accessor_header_lookup */


static a_prop_or_event_accessor_header_lookup_ptr
                               alloc_prop_or_event_accessor_header_lookup(void)
/*
Allocate a property or event accessor header lookup entry, initialize
its fields and return a pointer to it.
*/
{
  a_prop_or_event_accessor_header_lookup_ptr	peahlp;

  peahlp = alloc_fe_of_type(a_prop_or_event_accessor_header_lookup);
#if DEBUG
/* GCOVR_EXCL_START */
  num_prop_or_event_accessor_header_lookups_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  clear_prop_or_event_accessor_header_lookup(peahlp);
  return peahlp;
}  /* alloc_prop_or_event_accessor_header_lookup */


a_hash_value hash_prop_or_event_accessor_header_lookup(a_void_ptr	key)
/*
Produce a hash value for a property or event accessor header lookup
entry.  The key is a pointer to a property or event accessor header
lookup entry.
*/
{
  a_hash_value					value;
  a_prop_or_event_accessor_header_lookup_ptr	peahlp;

  peahlp = (a_prop_or_event_accessor_header_lookup_ptr)key;
  /* Add the hash values of the two component symbol headers to produce the
     hash value for the combined entry. */
  value = peahlp->property_or_event_header->hash_value +
          peahlp->accessor_header->hash_value;
  return value;
}  /* hash_prop_or_event_accessor_header_lookup */


a_boolean compare_prop_or_event_accessor_header_lookup(a_void_ptr	entry,
						       a_void_ptr	key)
/*
Compare an entry in the property or event accessor header lookup table
with an entry to be found.  "entry" and "key" are of type
a_prop_or_event_accessor_header_lookup_ptr.  Return TRUE if the key matches
the entry. */
{
  a_prop_or_event_accessor_header_lookup_ptr	entry_peahlp;
  a_prop_or_event_accessor_header_lookup_ptr	key_peahlp;
  a_boolean					result;

  entry_peahlp = (a_prop_or_event_accessor_header_lookup_ptr)entry;
  key_peahlp = (a_prop_or_event_accessor_header_lookup_ptr)key;
  result = entry_peahlp->property_or_event_header ==
                                        key_peahlp->property_or_event_header &&
           entry_peahlp->accessor_header == key_peahlp->accessor_header;
  return result;
}  /* compare_prop_or_event_accessor_header_lookup */


static a_symbol_header_ptr get_property_or_event_accessor_symbol_header(
			a_symbol_header_ptr	property_or_event_header,
			a_symbol_header_ptr	accessor_header)
/*
Given a symbol header for a property or event (property_or_event_header)
and a symbol header for an accessor function (accessor_header) return
a symbol header that represents that combination.  A previously created
header is returned if one exists.  Otherwise, a new header is created.
*/
{
  a_prop_or_event_accessor_header_lookup_ptr	peahlp = NULL;
  a_prop_or_event_accessor_header_lookup_ptr	*peahlp_in_table = NULL;
  a_prop_or_event_accessor_header_lookup	peahlp_key;

  /* If the hash table has not been allocated yet, allocate it now. */
  if (prop_or_event_accessor_header_hash_table == NULL) {
    prop_or_event_accessor_header_hash_table =
               alloc_hash_table(FRONT_END_REGION_NUMBER,
                (a_hash_table_size)100,
                fn_for_function(hash_prop_or_event_accessor_header_lookup),
                fn_for_function(compare_prop_or_event_accessor_header_lookup));
  }  /* if */
  /* Create an entry to be used as the lookup key. */
  clear_prop_or_event_accessor_header_lookup(&peahlp_key);
  peahlp_key.property_or_event_header = property_or_event_header;
  peahlp_key.accessor_header = accessor_header;
  peahlp_in_table = (a_prop_or_event_accessor_header_lookup_ptr*)
                          hash_find(prop_or_event_accessor_header_hash_table,
                                    (a_void_ptr)&peahlp_key, /*create=*/TRUE);
  peahlp = *peahlp_in_table;
  /* If no entry was found, create one. */
  if (peahlp == NULL) {
    a_symbol_header_ptr	new_header;
    peahlp = alloc_prop_or_event_accessor_header_lookup();
    peahlp->property_or_event_header = property_or_event_header;
    peahlp->accessor_header = accessor_header;
    peahlp->combined_header = new_header = alloc_symbol_header();
    /* The identifier of the combined header is the same as the accessor. */
    new_header->identifier = accessor_header->identifier;
    new_header->identifier_length = accessor_header->identifier_length;
    /* Assign a hash value to this symbol header based on the hash values
       of the component headers. */
    new_header->hash_value = property_or_event_header->hash_value +
                             accessor_header->hash_value;
    *peahlp_in_table = peahlp;
  }  /* if */
  return peahlp->combined_header;
}  /* get_property_or_event_accessor_symbol_header */

#if DEBUG
/* GCOVR_EXCL_START */

void db_hide_by_sig_list(a_hide_by_sig_list_entry_ptr	hbslep)
/*
Display a hide-by-sig list, for debugging purposes.
*/
{
  fprintf(f_debug, "hide-by-sig list:\n");
  if (hbslep == NULL) {
    fprintf(f_debug, "<NULL LIST>\n");
  } else {
    for (; hbslep != NULL; hbslep = hbslep->next) {
      fprintf(f_debug, "%*s", (int)(hbslep->level*2), "");
      if (hbslep->symbol == NULL) {
        fprintf(f_debug, "<NULL> (%d)\n", (int)hbslep->level);
      } else {
        db_symbol_name(hbslep->symbol);
        fprintf(f_debug, " (%d)", (int)hbslep->level);
        if (hbslep->base_class != NULL) {
          fprintf(f_debug, " base_class: ");
          db_abbreviated_base_class(hbslep->base_class);
        }  /* if */
        fprintf(f_debug, "\n");
      }  /* if */
    }  /* for */
  }  /* if */
}  /* db_hide_by_sig_list */

/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

static void free_list_of_hide_by_sig_list_entries(
					a_hide_by_sig_list_entry_ptr hbslep)
/*
Add a list of hide-by-sig list entries to the available list.  hbslep may
be NULL, in which case nothing is done.
*/
{
  a_hide_by_sig_list_entry_ptr	hbslep_tail;
  if (hbslep != NULL) {
    /* Find the last entry on the list. */
    hbslep_tail = hbslep;
    while (hbslep_tail->next != NULL) hbslep_tail = hbslep_tail->next;
    /* Add the current available list to the end of the list passed by the
       caller. */
    hbslep_tail->next = avail_hide_by_sig_list_entries;
    avail_hide_by_sig_list_entries = hbslep;
  }  /* if */
}  /* free_list_of_hide_by_sig_list_entries */


static a_hide_by_sig_list_entry_ptr alloc_hide_by_sig_list_entry(void)
/*
Allocate a new hide-by-sig list entry, initialize it, and return a pointer
to it.
*/
{
  a_hide_by_sig_list_entry_ptr	hbslep;

  if (avail_hide_by_sig_list_entries != NULL) {
    /* Reuse an existing entry. */
    hbslep = avail_hide_by_sig_list_entries;
    avail_hide_by_sig_list_entries = avail_hide_by_sig_list_entries->next;
  } else {
    /* Allocate a new entry. */
    hbslep = alloc_fe_of_type(a_hide_by_sig_list_entry);
#if DEBUG
/* GCOVR_EXCL_START */
    num_hide_by_sig_list_entries_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  }  /* if */
  hbslep->next = NULL;
  hbslep->symbol = NULL;
  hbslep->base_class = NULL;
  hbslep->level = 0;
  return hbslep;
}  /* alloc_hide_by_sig_list_entry */


/*
Structure used to pass information between the hide-by-sig processing
routines.
*/
typedef struct a_hide_by_sig_state *a_hide_by_sig_state_ptr;
typedef struct a_hide_by_sig_state {
  a_symbol_ptr	orig_sym;
			/* The symbol passed from overload resolution. */
  a_boolean	is_class;
			/* TRUE if orig_sym came from a ref class (as
			   opposed to a ref interface). */
  a_boolean	suppress_hide_by_sig;
			/* TRUE if a condition was detected that renders
			   this lookup ineligible for hide-by-sig
			   processing. */
} a_hide_by_sig_state;


static void init_hide_by_sig_state(a_hide_by_sig_state_ptr	hbssp)
/*
Initialize a hide-by-sig state block.
*/
{
  hbssp->orig_sym = NULL;
  hbssp->is_class = FALSE;
  hbssp->suppress_hide_by_sig = FALSE;
}  /* init_hide_by_sig_state */


static void add_symbol_to_hide_by_sig_list(
				a_hide_by_sig_list_entry_ptr	*result_list,
				a_hide_by_sig_list_entry_ptr	*list_tail,
				a_symbol_ptr			sym,
				uint32_t			level,
				a_base_class_ptr		base_class)
/*
Add an entry to the hide-by-sig list specified by *result_list and
*list_tail.   sym is the symbol for the entry, and can be NULL.  level is
the level associated with the symbol.  base_class is the base class in
which the symbol was found, or NULL if it was found in the most derived
class.
*/
{
  a_hide_by_sig_list_entry_ptr	hbslep;

  hbslep = alloc_hide_by_sig_list_entry();
  hbslep->symbol = sym;
  hbslep->level = level;
  hbslep->base_class = base_class;
  if (*result_list == NULL) {
    *result_list = hbslep;
  } else {
    (*list_tail)->next = hbslep;
  }  /* if */
  *list_tail = hbslep;
}  /* add_symbol_to_hide_by_sig_list */


static void add_base_classes_to_hide_by_sig_list(
		a_hide_by_sig_state_ptr		hbssp,
		a_hide_by_sig_list_entry_ptr	*p_result_list,
		a_hide_by_sig_list_entry_ptr	*p_list_tail,
		a_type_ptr			type,
		uint32_t			level,
		a_boolean			*p_any_entries_at_level,
		a_base_class_ptr		base_class)
/*
Go through the base classes of type and call this routine recursively to
see if the base class contains a function or overload set that should be
returned in the hide-by-sig list specified by *p_result_list and
*p_result_tail.  Note that a new list is created by this routine at each level.
level is the level value to be recorded for any entry created.
*p_any_entries_at_level is set to TRUE if any entries are added to the
list for the level passed in.  hbssp points to a state block used to pass
information between the hide-by-sig routines.  base_class is the base
class entry associated with type relative to the most derived type.
It will be NULL for the most derived class.
*/
{
  a_hide_by_sig_list_entry_ptr	list = NULL;
  a_hide_by_sig_list_entry_ptr	list_tail = NULL;
  a_base_class_ptr		bcp;
  a_symbol_ptr			result_sym = NULL;
  a_symbol_ptr			other_sym = NULL;
  a_class_symbol_supplement_ptr	cssp;
  a_symbol_ptr			sym;
  a_boolean			any_entries_at_next_level = FALSE;
  a_class_type_supplement_ptr	ctsp;

  /* Look for a symbol in the specified type.  Note that the orig_sym is
     never directly entered on the list (although it could be found by the
     lookup below).  This is done because the orig_sym may not be
     appropriate (e.g., it could be an ambiguous symbol of some kind). */
  cssp = symbol_supplement_for_class(type);
  sym = find_symbol_list_in_table(&cssp->pointers_block,
                                  hbssp->orig_sym->header);
  for (; sym != NULL; sym = sym->next_in_lookup_table) {
    if (sym->is_invisible) {
      /* Ignore invisible symbols. */
    } else if (is_function_or_template_symbol(sym)) {
      /* If the same property or event is declared more than once (an error)
         there can be more than one symbol.  Use the first one found. */
      check_assertion_or_expect_error(result_sym == NULL);
      if (result_sym == NULL) result_sym = sym;
    } else if (sym->kind != (a_symbol_kind)sk_projection ||
               sym->variant.projection.is_using_decl) {
      /* Record the fact that we found a non-function.  Ignore projection
         symbols unless they represent using-declarations. */
      other_sym = sym;
    }  /* if */
  }  /* for */
  if (result_sym == NULL && other_sym != NULL) {
    /* This type contains a member with the given name that is not
       a function.  This symbol is added to the list and if it is
       accessible during overload resolution will cause the lookup
       to not go any further on this branch.  Note that *p_any_entries_at_level
       is not set for this entry because we want to skip any base
       class entries if this entry is not accessible. */
    add_symbol_to_hide_by_sig_list(&list, &list_tail, other_sym, level,
                                   base_class);
  } else if (result_sym != NULL) {
    a_type_ptr				parent_type;
    a_class_type_supplement_ptr		parent_ctsp;
    a_boolean				is_static_in_interface = FALSE;
    a_boolean				is_class;
    a_symbol_ptr			fund_result_sym;
    parent_type = sym_parent_class(result_sym);
    parent_ctsp = class_type_supp(parent_type);
    is_class = parent_ctsp->cli_class_type_kind ==
                                               (a_cli_class_type_kind)cctk_ref;
    fund_result_sym = fundamental_symbol_of(result_sym);
    /* If we encounter a static method in an interface, the original symbol
       should be used and hide-by-sig processing suppressed. */
    if (!is_class) {
      a_symbol_ptr	rout_sym = NULL;
      a_type_ptr	rout_type;
      if (fund_result_sym->kind == (a_symbol_kind)sk_overloaded_function) {
        if (fund_result_sym->
                          variant.overloaded_function.mixed_static_nonstatic) {
          /* When this flag is set, we know there is at least one static
             method. */
          is_static_in_interface = TRUE;
        } else {
          rout_sym = fund_result_sym->variant.overloaded_function.symbols;
          rout_sym = fundamental_symbol_of(rout_sym);
        }  /* if */
      } else {
        rout_sym = fund_result_sym;
      }  /* if */
      if (!is_static_in_interface) {
        if (rout_sym->kind == (a_symbol_kind)sk_function_template) {
          rout_type = rout_sym->variant.template_info->
                                                variant.function.routine->type;
        } else {
          rout_type = rout_sym->variant.routine.ptr->type;
        }  /* if */
        if (!routine_type_is_nonstatic_member_function(rout_type)) {
          /* We either have a non-mixed list (so all the list entries are
             either static or nonstatic) or a single symbol.  So we can just
             look at the first entry (or the single symbol). */
          is_static_in_interface = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
    if (is_static_in_interface) {
      hbssp->suppress_hide_by_sig = TRUE;
    } else {
      add_symbol_to_hide_by_sig_list(&list, &list_tail, result_sym, level,
                                     base_class);
      *p_any_entries_at_level = TRUE;
    }  /* if */
  }  /* if */
  /* Look through the base classes (and possibly interfaces) of this type. */
  ctsp = class_type_supp(type);
  if (!ctsp->is_hide_by_sig) {
    /* We have encountered a class that is hide-by-name.  Do not inspect
       its base classes. */
    bcp = NULL;
  } else {
    bcp = base_classes_of(type);
  }  /* if */
  for (; bcp != NULL && !hbssp->suppress_hide_by_sig; bcp = bcp->next) {
    a_type_ptr			base_type = bcp->type;
    a_class_type_supplement_ptr	base_ctsp;
    /* Only process direct bases. */
    if (!bcp->direct) continue;
    base_ctsp = class_type_supp(base_type);
    if (!hbssp->is_class ||
        base_ctsp->cli_class_type_kind == (a_cli_class_type_kind)cctk_ref) {
      a_hide_by_sig_list_entry_ptr	sublist = NULL;
      a_hide_by_sig_list_entry_ptr	sublist_tail = NULL;
      a_base_class_ptr			adjusted_bcp = bcp;
      if (base_class != NULL) {
        /* Get the version of the base class that is relative to the most
           derived class. */
        adjusted_bcp = corresp_base_class(bcp, base_class);
      }  /* if */
      add_base_classes_to_hide_by_sig_list(hbssp, &sublist, &sublist_tail,
                                           bcp->type, level+1,
                                           &any_entries_at_next_level,
                                           adjusted_bcp);
      if (sublist != NULL) {
        if (result_sym == NULL && *p_any_entries_at_level) {
          /* There was a list returned for the base class.  Add an entry for
             the current class if one does not already exist.  This is only
             done if there are already entries at this level (so the
             caller needs to be able to find then one when skipping
             forward). */
          add_symbol_to_hide_by_sig_list(&list, &list_tail, (a_symbol_ptr)NULL,
                                         level, base_class);
        }  /* if */
        /* Append the sublist to the list being built. */
        if (list == NULL) {
          list = sublist;
        } else {
          list_tail->next = sublist;
        }  /* if */
        list_tail = sublist_tail;
      }  /* if */
    }  /* if */
  }  /* for */
  *p_result_list = list;
  *p_list_tail = list_tail;
}  /* add_base_classes_to_hide_by_sig_list */


a_boolean treat_as_cli_class_for_lookup(a_type_ptr	type)
/*
Return TRUE if type should be considered a class for C++/CLI lookup purposes.
When lookup begins in something considered to be a ref class, base interfaces
are not considered; otherwise they are.
*/
{
  a_class_type_supplement_ptr	ctsp;
  a_boolean			result = FALSE;

  /* Certain generic constraint types are ref classes but are not treated
     as such for lookup purposes. */
  ctsp = class_type_supp(type);
  result = ctsp->cli_class_type_kind == (a_cli_class_type_kind)cctk_ref &&
                  !type->variant.class_struct_union.is_hybrid_constraint &&
                  !type->variant.class_struct_union.any_interface_constraints;
  return result;
}  /* treat_as_cli_class_for_lookup */


a_boolean use_hide_by_sig_lookup(
			a_symbol_ptr			sym,
			a_hide_by_sig_list_entry_ptr	*p_hide_by_sig_list)
/*
Determine the set of symbols that should be considered for a call of the
derived class or interface routine specified by sym.

Return TRUE if sym is a symbol for which hide-by-sig lookup should be done,
or FALSE if a normal lookup should be done (in which case "sym" is the
symbol that will be used).  Note that FALSE is returned for native
(i.e., non-C++/CLI) classes, and also for symbols that are not functions
or not class members.

If hide-by-sig lookup should be done, (and p_hide_by_sig_list is not NULL)
return a list that identifies the symbols of the functions to be considered,
and additional information to determine where the symbol fits in the
derivation hierarchy.  Note that the list can be NULL if no such symbols are
found.  NULL is also returned if the lookup encounters an interface with a
static method.  The list is returned in *p_hide_by_sig_list.
*/
{
  a_hide_by_sig_list_entry_ptr		result_list = NULL;
  a_type_ptr				parent_type = NULL;
  a_class_type_supplement_ptr		parent_ctsp = NULL;
  a_boolean				is_class = FALSE;
  a_boolean				result = FALSE;
  a_symbol				*fund_sym;

  if (sym->is_class_member) {
    if (sym->kind == (a_symbol_kind)sk_projection &&
        sym->is_super_reference) {
      /* For a name reference __super::x the hide-by-sig lookup is done on the
         underlying symbol. */
      sym = fundamental_symbol_of(sym);
    }  /* if */
    parent_type = sym_parent_class(sym);
    parent_ctsp = class_type_supp(parent_type);
    /* Certain generic constraint types are ref classes but are not treated
       as such for lookup purposes. */
    is_class = treat_as_cli_class_for_lookup(parent_type);
  }  /* if */
  if (sym->hide_by_sig_lookup_done) {
    /* We have already done the hide-by-sig processing.   Return the results
       from the original lookup. */
    result_list = sym->hide_by_sig_lookup_result;
    result = !sym->suppress_hide_by_sig_lookup;
  } else if (!sym->is_class_member) {
    /* This lookup only applies to class members -- return FALSE. */
  } else if (!parent_ctsp->is_hide_by_sig) {
    /* The parent class is hide-by-name -- return FALSE. */
  } else if ((fund_sym = fundamental_symbol_of(sym)),
             !is_function_or_template_symbol(fund_sym)) {
    /* This lookup only applies to functions -- return FALSE. */
  } else if (is_constructor_symbol(fund_sym) ||
             is_destructor_symbol(fund_sym) ||
             is_finalizer_symbol(fund_sym) ||
             is_conversion_function_symbol(fund_sym)) {
    /* This lookup does not apply to constructors, destructors, finalizers,
       and conversion functions. */
  } else if (sym->is_invisible ||
             (sym->kind == (a_symbol_kind)sk_overloaded_function &&
              sym->variant.overloaded_function.symbols->is_invisible)) {
    /* An invisible symbol.  Don't attempt hide-by-sig lookup and return
       FALSE. */
  } else if (is_class ||
             parent_ctsp->cli_class_type_kind ==
                                      (a_cli_class_type_kind)cctk_interface) {
    a_hide_by_sig_state			hbss;
    a_hide_by_sig_list_entry_ptr	sublist;
    a_hide_by_sig_list_entry_ptr	sublist_tail;
    a_boolean				any_entries_at_level = FALSE;
    init_hide_by_sig_state(&hbss);
    hbss.orig_sym =  sym;
    hbss.is_class = is_class;
    add_base_classes_to_hide_by_sig_list(&hbss, &sublist, &sublist_tail,
                                         parent_type, /*level=*/0,
                                         &any_entries_at_level,
                                         (a_base_class_ptr)NULL);
    result_list = sublist;
    if (hbss.suppress_hide_by_sig) {
      /* Hide-by-sig lookup should be suppressed (e.g., a base interface
         contains a static method).  Return NULL. */
      /* Free all of the allocated entries. */
      free_list_of_hide_by_sig_list_entries(result_list);
      result_list = NULL;
      sym->suppress_hide_by_sig_lookup = TRUE;
    } else {
      result = TRUE;
    }  /* if */
    sym->hide_by_sig_lookup_result = result_list;
    sym->hide_by_sig_lookup_done = TRUE;
  }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
  if (db_flag_is_set("hbs")) {
    db_hide_by_sig_list(result_list);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  /* Return the list if a pointer to the list pointer was provided by
     the caller. */
  if (p_hide_by_sig_list != NULL) *p_hide_by_sig_list = result_list;
  return result;
}  /* use_hide_by_sig_lookup */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

a_type_list_entry_ptr alloc_type_list_entry(void)
/*
Allocate a new type list entry and return a pointer to it.
*/
{
  a_type_list_entry_ptr ptr;

  db_enter(5, "alloc_type_list_entry");
  if (avail_type_list_entries != NULL) {
    /* Reuse an existing entry. */
    ptr = avail_type_list_entries;
    avail_type_list_entries = avail_type_list_entries->next;
  } else {
    /* Allocate a new entry. */
    ptr = (a_type_list_entry_ptr)alloc_fe(sizeof(a_type_list_entry));
#if DEBUG
/* GCOVR_EXCL_START */
   num_type_list_entries_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  }  /* if */
  ptr->next    = NULL;
  ptr->type  = NULL;
  
  db_exit();
  return ptr;
}  /* alloc_type_list_entry */


void free_list_of_type_list_entries(a_type_list_entry_ptr tlep)
/*
Add a list of type list entries to the available list.  tlep may
be NULL, in which case nothing is done.
*/
{
  a_type_list_entry_ptr	tlep_tail;
  if (tlep != NULL) {
    /* Find the last entry on the list. */
    tlep_tail = tlep;
    while (tlep_tail->next != NULL) tlep_tail = tlep_tail->next;
    /* Add the current available list to the end of the list passed by the
       caller. */
    tlep_tail->next = avail_type_list_entries;
    avail_type_list_entries = tlep;
  }  /* if */
}  /* free_list_of_type_list_entries */

#if DEBUG
/* GCOVR_EXCL_START */

void db_type_entries(a_type_list_entry_ptr  tlep,
                     int                    indent = 0)
/*
Display the contents of the given list.  (This function is not called
db_type_list because that name was in use already.)
*/
{
  int  k = 0;

  for (; tlep != NULL; tlep = tlep->next, ++k) {
    for (int n = 0; n < indent; n++) fputc(' ', f_debug);
    fprintf(f_debug, "[%3d] ", k);
    db_abbreviated_type(tlep->type);
    (void)fprintf(f_debug, "\n");
  }  /* for */
}  /* db_type_entries */

/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

a_symbol_ptr find_symbol(a_const_char     *identifier,
			 sizeof_t         length,
			 a_symbol_locator *location)
/*
Look up a symbol in the symbol table.  Return a pointer to the first symbol
under the symbol header for that name.  If the symbol header is not there,
create one and set the symbol locator to point to the header.  Note that
the source position in the locator is not changed; usually, it will have
been set by get_token when an identifier is scanned, but sometimes the
caller may have to set it directly.
*/
{
  a_hash_value        hash_value = 0;
  a_const_char        *ptr;
  sizeof_t            a;
  a_symbol_header_ptr hdr_ptr;
  a_symbol_header_ptr prev_hdr_ptr;
  a_symbol_ptr        sym_ptr    = NULL;
  unsigned            bucket_number;

  db_enter(4, "find_symbol");
#if DEBUG
/* GCOVR_EXCL_START */
  num_searches_for_symbols++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

  /* Hash the symbol's identifier.  This is not a particularly strong hash,
     but it does well enough on most inputs. */
  ptr = identifier;
  for (a = 0; a < length; a++) {
    hash_value = (hash_value * HASH_FACTOR) + (unsigned char)*ptr++;
  }  /* for */

  /* Look in the symbol bucket saving the position in case this symbol needs
     to be added. */
  bucket_number = hash_value % SYMBOL_TABLE_SIZE;
  if ((hdr_ptr = symbol_table[bucket_number]) != NULL) {
    prev_hdr_ptr = NULL;
    do {
#if DEBUG
/* GCOVR_EXCL_START */
      num_compares_for_symbols++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
      if (length == hdr_ptr->identifier_length) {
        /*lint -e{670}*/
        if (memcmp(identifier, hdr_ptr->identifier, size_t_arg(length)) == 0) {
	  /* Have a match. */
	  sym_ptr = hdr_ptr->symbol;
          /* Relink the symbol header at the front of the list of headers,
             so that frequently-used headers will be found quickly. */
          if (prev_hdr_ptr != NULL) {
            prev_hdr_ptr->next = hdr_ptr->next;
            hdr_ptr->next = symbol_table[bucket_number];
            symbol_table[bucket_number] = hdr_ptr;
          }  /* if */
	  goto symbol_found;
        }  /* if */
      }  /* if */
      prev_hdr_ptr = hdr_ptr;
    } while ((hdr_ptr = hdr_ptr->next) != NULL);
  }  /* if */

  /* Exiting this loop indicates that the symbol does not exist in the table;
     allocate a symbol header for it. */
  hdr_ptr = alloc_symbol_header();
#if DEBUG
/* GCOVR_EXCL_START */
  num_symbol_headers_in_hash_table++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

  /* Link the new header onto the front of the appropriate bucket of the symbol
     table. */
#if DEBUG
/* GCOVR_EXCL_START */
  if (symbol_table[bucket_number] == NULL) num_used_symbol_buckets++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  hdr_ptr->next = symbol_table[bucket_number];
  symbol_table[bucket_number] = hdr_ptr;

  /* Copy the string to memory in the appropriate memory region.  It is
     allocated in the intermediate language memory region because it must
     be passed to the back end. */
  set_identifier_for_symbol_header(hdr_ptr, identifier, length,
                                   /*is_unnamed=*/FALSE);
  hdr_ptr->hash_value = hash_value;

  /* There is no symbol. */
  sym_ptr = NULL;

symbol_found:
  location->symbol_header = hdr_ptr;

  db_exit();

  return sym_ptr;
}  /* find_symbol */


a_boolean looks_like_ctor_or_dtor(a_symbol_locator  *loc)
/*
Return TRUE if the given symbol locator looks like that for a constructor,
destructor, or C++/CLI finalizer.  The answer can be TRUE even when an error
symbol is given (in which case the symbol is never actually marked as being a
special function).  This is useful to inhibit some diagnostics that are not
meaningful on these kinds of member functions (e.g., missing return statements
and implicit return types).
*/
{
  a_boolean  answer = FALSE;

  if (loc->is_class_member && loc->symbol_header != NULL) {
    a_symbol_ptr  parent = symbol_for(qualifier_class_type(*loc));
    if (loc->symbol_header->identifier != NULL &&
        parent->header->identifier != NULL &&
        strcmp(loc->symbol_header->identifier,
               parent->header->identifier) == 0) {
      answer = TRUE;
    }  /* if */
  }  /* if */
  if (!answer && loc->symbol_header != NULL) {
    /* Misdeclared destructors may not be marked as class members: */
    a_const_char *name = loc->symbol_header->identifier;
    if (name != NULL &&
        (name[0] == '~' || (cli_or_cx_enabled && name[0] == '!'))) {
      answer = TRUE;
    }  /* if */
  }  /* if */
  return answer;
}  /* looks_like_ctor_or_dtor */


void make_locator_for_symbol(a_symbol_ptr     sym_ptr,
                             a_symbol_locator *location)
/*
Create in *location a locator for the symbol pointed to by sym_ptr.
*/
{
  clear_locator(location, &sym_ptr->decl_position);
  location->symbol_header = sym_ptr->header;
  location->specific_symbol = sym_ptr;
  location->is_class_member = sym_ptr->is_class_member;
  location->parent = sym_ptr->parent;
  location->is_error = sym_ptr->is_error;
}  /* make_locator_for_symbol */


void make_resolved_id_pseudo_token_locator(a_symbol_ptr     sym_ptr,
                                           a_symbol_locator *location)
/*
Create in *location a locator for the symbol resolved from a resolved
identifier pseudo token.
*/
{
  make_locator_for_symbol(sym_ptr, location);
  location->do_not_clear_specific_symbol = TRUE;
  location->is_implicitly_qualified = TRUE;
}  /* make_resolved_id_pseudo_token_locator */


void make_specific_symbol_error_locator(a_symbol_locator *locator)
/*
Make a specific symbol error locator in *locator.  This identifies a
specific symbol which is an error symbol.
*/
{
  a_symbol_header_ptr  hdr_ptr = locator->symbol_header;

  clear_locator(locator, &error_position);
  locator->symbol_header = hdr_ptr;
  locator->is_error = TRUE;
  locator->do_not_clear_specific_symbol = TRUE;
  locator->specific_symbol = enter_symbol((a_symbol_kind)sk_undefined,
                                          locator,
                                          DEPTH_OF_FILE_SCOPE,
                                          /*suppress_error=*/TRUE);
}  /* make_specific_symbol_error_locator */


static a_symbol_header_ptr get_error_symbol_header(void)
/*
Return a pointer to the error symbol header.  Create it if it has not
already been created.
*/
{
  if (error_symbol_header == NULL) {
    error_symbol_header = alloc_symbol_header();
    set_identifier_for_symbol_header(error_symbol_header, "<error>", 7,
                                   /*is_unnamed=*/FALSE);
  }  /* if */
  return error_symbol_header;
}  /* get_error_symbol_header */


extern void make_error_locator(a_symbol_locator *locator)
/*
Make a locator not specifically associated with any symbol.
*/
{
  set_to_error_locator(*locator);
  locator->symbol_header = get_error_symbol_header();
}  /* make_error_locator */


void clear_qualifier_from_locator(a_symbol_locator  *locator)
/*
Reset the fields in the specified locator to remove traces of a class,
global, or namespace qualifier.
*/
{
  locator->is_qualified_name = FALSE;
  locator->is_file_scope_qualified_name = FALSE;
  locator->is_global_qualified_name = FALSE;
  if (locator->is_class_member) {
    locator->is_class_member = FALSE;
    locator->parent.class_type = NULL;
  } else {
    locator->parent.namespace_ptr = NULL;
  }  /* if */
}  /* clear_qualifier_from_locator */


a_namespace_list_entry_ptr alloc_namespace_list_entry(void)
/*
Allocate a namespace list entry and return a pointer to it.
*/
{
  a_namespace_list_entry_ptr ptr;

  if (avail_namespace_list_entries != NULL) {
    /* Reuse an existing entry. */
    ptr = avail_namespace_list_entries;
    avail_namespace_list_entries = avail_namespace_list_entries->next;
  } else {
    /* Allocate a new entry. */
    ptr = (a_namespace_list_entry_ptr)alloc_fe(sizeof(a_namespace_list_entry));
#if DEBUG
/* GCOVR_EXCL_START */
   num_namespace_list_entries_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  }  /* if */
  ptr->next = NULL;
  ptr->ptr = NULL;
  return ptr;
}  /* alloc_namespace_list_entry */


void free_list_of_namespace_list_entries(a_namespace_list_entry_ptr nlep)
/*
Add a list of namespace list entries to the available list.  nlep may
be NULL, in which case nothing is done.
*/
{
  a_namespace_list_entry_ptr	nlep_tail;
  if (nlep != NULL) {
    /* Find the last entry on the list. */
    nlep_tail = nlep;
    while (nlep_tail->next != NULL) nlep_tail = nlep_tail->next;
    /* Add the current available list to the end of the list passed by the
       caller. */
    nlep_tail->next = avail_namespace_list_entries;
    avail_namespace_list_entries = nlep;
  }  /* if */
}  /* free_list_of_namespace_list_entries */


a_symbol_ptr corresp_prototype_for_class_symbol(a_symbol_ptr sym)
/*
sym points to a symbol entry for a class.  If the class is an instance
of a template, but not a specialized template or a nonreal class, return
the corresponding prototype symbol from the class symbol supplement.
Otherwise, return NULL.
*/
{
  a_class_symbol_supplement_ptr	cssp;
  a_symbol_ptr			result_sym = NULL;
  a_type_ptr                    class_type;

  check_assertion(is_class_struct_union_symbol(sym));
  cssp = sym->variant.class_struct_union.extra_info;
  class_type = sym->variant.class_struct_union.type;
  if (class_type->variant.class_struct_union.is_template_class &&
      /*lint -e(506)*/!is_cli_generic_definition_type(class_type) &&
      !class_type->variant.class_struct_union.is_nonreal_class) {
    if (!class_type->variant.class_struct_union.is_specialized) {
      result_sym = cssp->corresp_prototype_sym;
      check_assertion_str2(result_sym != NULL,
                           "corresp_prototype_for_class_symbol:",
                           "no corresponding prototype symbol for instance");
    }  /* if */
  }  /* if */
  return result_sym;
}  /* corresp_prototype_for_class_symbol */


a_symbol_ptr template_symbol_for_class_symbol(a_symbol_ptr class_sym)
/*
Return the template symbol for the template from which the class
associated with class_sym was generated.  If class_sym points to an
instance of a class template, the template symbol returned points to
the class template symbol associated with the template definition.  If
class_sym points to a class nested within a class template, the template
symbol returned points to the prototype instantiation of the nested
class.
*/
{
  a_symbol_ptr			template_sym;
  a_class_symbol_supplement_ptr	cssp;

  cssp = class_sym->variant.class_struct_union.extra_info;
  if (cssp->class_template == NULL) {
    /* If the class_template pointer is NULL, this is expected to be a class
       nested within a class template. */
    template_sym = cssp->corresp_prototype_sym;
  } else {
    template_sym = cssp->class_template;
  }  /* if */
  return template_sym;
}  /* template_symbol_for_class_symbol */

#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS

a_boolean entity_cannot_be_specialized(a_symbol_ptr  sym)
/*
Return TRUE if the given entity is an instance of a template that cannot be
explicitly specialized.
*/
{
  a_boolean     result = FALSE;
  a_symbol_ptr  template_sym = NULL;

  if (sym->is_class_member) {
    a_type_ptr  parent_class = sym_parent_class(sym);
    if (class_type_supp(parent_class)->is_lambda_closure_class) {
      /* The member templates in closure types of generic lambdas cannot be
         specialized. */
      result = TRUE;
    } else {
      while (parent_class->source_corresp.is_class_member) {
        parent_class = parent_class_of(parent_class);
      }  /* while */
      template_sym = template_symbol_for_class_symbol(
                                                    symbol_for(parent_class));
    }  /* if */
  } else if (is_class_struct_union_symbol(sym)) {
    template_sym = template_symbol_for_class_symbol(sym);
  }  /* if */
  if (template_sym != NULL &&
      template_sym->variant.template_info
                  ->variant.class_template.cannot_be_specialized) {
    result = TRUE;
  }  /* if */
  return result;
}  /* entity_cannot_be_specialized */

#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */


struct a_token_range {
  a_token_sequence_number
		first, last;
			/* First and last token sequence numbers delimiting
			   a range. */
};


static inline a_boolean operator==(a_token_range  x,
                                   a_token_range  y)
/*
Return TRUE if the given token ranges are equivalent.
*/
{
  return x.first == y.first && x.last == y.last;
}  /* operator== */


static inline a_boolean operator!=(a_token_range  x,
                                   a_token_range  y)
/*
Return TRUE if the given token ranges are not equivalent.
*/
{
  return !(x == y);
}  /* operator!= */


static inline uintptr_t hash_ptr(a_token_range  tr)
/*
Return a hash value for the given token range.
*/
{
  uintptr_t  result = 17*31 + (uintptr_t)tr.first;

  result = result*31 + (uintptr_t)tr.last;
  return result;
}  /* hash_ptr */


using a_template_cache_segment_table = Ptr_map<a_token_range,
                                               a_template_cache_segment_ptr>;
			/* The type of a table that maps token ranges to
			   template cache segments. */


STATIC_THREAD a_template_cache_segment_table
		*template_cache_segment_table;
			/* A map from token ranges to corresponding template
			   cache segment structures. */


a_template_cache_segment_ptr alloc_template_cache_segment(
                                a_symbol_ptr				sym,
                                a_template_symbol_supplement_ptr	tssp)
/*
Allocate a new template segment descriptor entry, initialize its fields, and
return a pointer to it.  sym points to the symbol for the member class or
function for which the cache segment entry is being created.  tssp points
to the symbol supplement associated with sym.
*/
{
  a_template_cache_segment_ptr tcsp = new_fe<a_template_cache_segment>();
  a_scope_stack_entry_ptr      ssep;
  a_scope_depth                depth_to_use = NO_SCOPE_DEPTH;
  a_boolean                    is_valid_context;

  tcsp->symbol = sym;
  tcsp->template_info = tssp;
  tcsp->first_token_number = NO_TOKEN_SEQUENCE_NUMBER;
  tcsp->last_token_number = NO_TOKEN_SEQUENCE_NUMBER;
  tcsp->source_cache = NULL;
  tcsp->is_friend = FALSE;
  tcsp->is_default_arg = FALSE;
  tcsp->expression_missing = FALSE;
  tcsp->is_exception_specification_arg = FALSE;
  tcsp->exception_spec_on_templ_friend = FALSE;
  /* Add the new entry to the list of template cache segments associated
     with the current instantiation.  If there is no current instantiation,
     use the current template declaration scope. */
  for (ssep = &scope_stack_top(); ssep != NULL;
       ssep = previous_scope_of(ssep)) {
    if (ssep->kind == (a_scope_kind)sck_template_instantiation) {
      depth_to_use = scope_depth_of(ssep);
      break;
    }  /* if */
  }  /* for */
  if (depth_to_use == NO_SCOPE_DEPTH) {
    depth_to_use = depth_template_declaration_scope;
    check_assertion(depth_to_use != NO_SCOPE_DEPTH);
  }  /* if */
  ssep = &scope_stack[depth_to_use];
  is_valid_context = ssep->in_prototype_instantiation ||
                     ssep->in_generic_definition;
  if (!is_valid_context && is_template_declaration_context()) {
    ssep = &scope_stack[depth_template_declaration_scope];
    is_valid_context = TRUE;
  }  /* if */
  check_assertion_or_expect_error_str2(is_valid_context,
                                       "alloc_template_cache_segment:",
                                       "not in prototype instantiation");
  if (is_valid_context) {
    /* Don't add the entry to the scope stack list if it the construct
       appeared in an invalid location as a result of an error. */
    if (ssep->template_cache_segment_list == NULL) {
      ssep->template_cache_segment_list =
                                       new_fe<a_template_cache_segment_list>();
    }  /* if */
    ssep->template_cache_segment_list->push_back(tcsp);
  }  /* if */
  return tcsp;
}  /* alloc_template_cache_segment */


a_template_cache_segment::~a_template_cache_segment()
/*
Destroy the current template cache segment entry.
*/
{
  a_token_range                 key{this->first_token_number,
                                    this->last_token_number};
  a_template_cache_segment_ptr  match = template_cache_segment_table->get(key);

  if (match == this) {
    /* Remove this template cache segment from the template cache segment table
       if it's being destroyed and it was mapped into the template cache
       segment table. */
    template_cache_segment_table->unmap(key);
  }  /* if */
}  /* a_template_cache_segment::~a_template_cache_segment */


a_template_cache_segment_ptr get_template_cache_segment(
                                a_symbol_ptr                      sym,
                                a_template_symbol_supplement_ptr  tssp,
                                a_token_sequence_number           first_tsn,
                                a_token_sequence_number           last_tsn)
/*
Return a template cache segment entry with the given parameters.  If an entry
with the given token range already exists, return it if it matches sym and
tssp.
*/
{
  a_template_cache_segment_ptr  result;

  result = template_cache_segment_table->get(
                                        a_token_range{ first_tsn, last_tsn });
  if (result == NULL ||
      result->symbol != sym || result->template_info != tssp) {
    a_boolean  map_result = result == NULL;
    result = alloc_template_cache_segment(sym, tssp);
    result->first_token_number = first_tsn;
    result->last_token_number = last_tsn;
    if (map_result) {
      template_cache_segment_table->map(a_token_range{ first_tsn, last_tsn },
                                        result);
    }  /* if */
  }  /* if */
  return result;
}  /* get_template_cache_segment */


an_out_of_class_partial_spec_ptr alloc_out_of_class_partial_spec(void)
/*
Allocate a new template declaration information entry, initialize its
fields, and return a pointer to it.
*/
{
  an_out_of_class_partial_spec_ptr  oocpsp;

  /* Allocate a template declaration information entry. */
  oocpsp = alloc_fe_of_type(an_out_of_class_partial_spec);
  oocpsp->next = NULL;
  oocpsp->symbol = NULL;
  new (&oocpsp->cache) a_template_cache();
  clear_template_cache(&oocpsp->cache);
#if DEBUG
/* GCOVR_EXCL_START */
  num_out_of_class_partial_specs_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

  return oocpsp;
}  /* alloc_out_of_class_partial_spec */


a_template_decl_info_ptr alloc_template_decl_info(void)
/*
Allocate a new NULL template decl information entry, initialize its
fields, and return a pointer to it.  Reuse a freed entry if possible.
*/
{
  a_template_decl_info_ptr  tdip;

  if (avail_template_decl_infos != NULL) {
    /* Reuse a freed entry.  The enclosing_template_decl field is used as
       a pointer to the next entry on the available list. */
    tdip = avail_template_decl_infos;
    avail_template_decl_infos = tdip->enclosing_template_decl;
  } else {
    /* Allocate a template declaration information entry. */
    tdip = (a_template_decl_info_ptr)alloc_fe(sizeof(a_template_decl_info));
#if DEBUG
/* GCOVR_EXCL_START */
    num_template_decl_info_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  }  /* if */
  tdip->parameters = NULL;
  tdip->declaration_scope = NO_SCOPE_NUMBER;
  tdip->enclosing_scope = NULL;
  tdip->enclosing_template_decl = NULL;
  tdip->template_decl = NULL;
  tdip->name_linkage = (a_name_linkage_kind)nlk_none;
  tdip->n_params = 0;
  tdip->decl_seq = NO_DECL_SEQUENCE_NUMBER;
  tdip->nondependent_calls = NULL;
  tdip->last_entry_added = NULL;
  tdip->pack_expansions = NULL;
  tdip->last_pack_expansion = NULL;
  tdip->constexpr_if_hash_table = NULL;
  tdip->variable_instance_sym = NULL;
  return tdip;
}  /* alloc_template_decl_info */


void free_template_decl_info(a_template_decl_info_ptr tdip)
/*
Free the template declaration information entry pointed to by tdip.
Put the freed entry on the available list to be reused.
*/
{
  /* The enclosing_template_decl field is used as a pointer to the next
     entry on the available list. */
  tdip->enclosing_template_decl = avail_template_decl_infos;
  avail_template_decl_infos = tdip;
}  /* free_template_decl_info */


static a_token_sequence_xref_ptr alloc_token_sequence_xref(void)
/*
Allocate a new token sequence number mapping entry, initialize its
fields, and return a pointer to it.
*/
{
  a_token_sequence_xref_ptr  tsxp;

  tsxp = alloc_fe_of_type(a_token_sequence_xref);
  tsxp->token_sequence_number = NO_TOKEN_SEQUENCE_NUMBER;
  tsxp->entry = NULL;
#if DEBUG
/* GCOVR_EXCL_START */
  num_token_sequence_xrefs_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  return tsxp;
}  /* alloc_token_sequence_xref */


static a_constexpr_if_cache_info_ptr alloc_constexpr_if_cache_info(void)
/*
Allocate a new constexpr if cache information entry, initialize its
fields, and return a pointer to it.
*/
{
  a_constexpr_if_cache_info_ptr  cicip = new_fe<a_constexpr_if_cache_info>();
#if DEBUG
/* GCOVR_EXCL_START */
  num_constexpr_if_cache_info_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  return cicip;
}  /* alloc_constexpr_if_cache_info */


static a_nondependent_call_info_ptr alloc_nondependent_call_info(void)
/*
Allocate a new nondependent call information entry, initialize its
fields, and return a pointer to it.
*/
{
  a_nondependent_call_info_ptr  ndcip;

  /* Allocate the entry. */
  ndcip = (a_nondependent_call_info_ptr)
                                   alloc_fe(sizeof(a_nondependent_call_info));
  ndcip->next = NULL;
  ndcip->previous = NULL;
  ndcip->token_sequence_number = NO_TOKEN_SEQUENCE_NUMBER;
  ndcip->depth = 0;
  ndcip->symbol = NULL;
  ndcip->reversed_opnds = FALSE;
#if DEBUG
/* GCOVR_EXCL_START */
  num_nondependent_call_info_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  return ndcip;
}  /* alloc_nondependent_call_info */


a_nondependent_call_info_ptr get_nondependent_call_info(
                                a_token_sequence_number         tsn,
                                a_nondependent_call_depth       depth)
/*
If "tsn" is the token sequence number of a nondependent call in the
nondependent call list of the current template, return a pointer to
the associated information block.  Otherwise (i.e., if the call is
dependent), return NULL.  The list is maintained in token sequence
number order, and is pointed to from the template decl info block for
the template.  The current position on the list is maintained in the
next_nondependent_call field of the scope stack entry for the innermost
instantiation scope.  depth is usually zero, but serves as an additional
position disambiguator on tsn if non-zero.
*/
{
  a_scope_stack_entry_ptr	ssep;
  a_nondependent_call_info_ptr	list_ptr, result = NULL;

  check_assertion(depth_innermost_instantiation_scope != NO_SCOPE_DEPTH);
  ssep = &scope_stack[depth_innermost_instantiation_scope];
  list_ptr = ssep->next_nondependent_call;
  if (list_ptr == NULL) {
    /* There is no list. */
  } else if (tsn > list_ptr->token_sequence_number) {
    /* The current position in the list refers to a lower token sequence
       number than the one we are looking for.  Look forward in the list
       to see if an entry exists for this token sequence number. */
    while (list_ptr != NULL && tsn > list_ptr->token_sequence_number) {
      list_ptr = list_ptr->next;
    }  /* while */
  } else if (tsn < list_ptr->token_sequence_number) {
    /* The current position in the list refers to a higher token sequence
       number than the one we are looking for.  Look backward in the list
       to see if an entry exists for this token sequence number. */
    while (list_ptr != NULL && tsn < list_ptr->token_sequence_number) {
      list_ptr = list_ptr->previous;
    }  /* while */
  }  /* if */
  if (list_ptr != NULL) {
    if (tsn == list_ptr->token_sequence_number) {
      /* The token sequence number matches the next entry on the list.
         Return the entry. */
      if (depth != list_ptr->depth) {
        /* Unusual case -- we have to find a nearby entry to match the
           depth as well. */
        if (depth > list_ptr->depth) {
          while (list_ptr != NULL &&
                 depth > list_ptr->depth &&
                 tsn == list_ptr->token_sequence_number) {
            list_ptr = list_ptr->next;
          }  /* while */
        } else {
          while (list_ptr != NULL &&
                 depth < list_ptr->depth &&
                 tsn == list_ptr->token_sequence_number) {
            list_ptr = list_ptr->previous;
          }  /* while */
        }  /* if */
        if (list_ptr != NULL &&
            (tsn != list_ptr->token_sequence_number ||
             depth != list_ptr->depth)) {
          /* No entry with the right depth value found. */
          list_ptr = NULL;
        }  /* if */
      }  /* if */
      result = list_ptr;
    }  /* if */
  }  /* if */
  if (list_ptr != NULL) {
    /* Save the updated list pointer back into the scope stack entry. */
    ssep->next_nondependent_call = list_ptr;
  }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
  if (db_flag_is_set("nondep_call")) {
    fprintf(f_debug, "Searching for nondependent call at %ld", (long)tsn);
    if (depth != 0) {
      fprintf(f_debug, " (depth %lu)", (unsigned long)depth);
    }  /* if */
    fprintf(f_debug, "\n");
    if (result != NULL) {
      fprintf(f_debug, "  Found ");
      db_symbol_name(result->symbol);
      fprintf(f_debug, "\n");
    }  /* if */
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  return result;
}  /* get_nondependent_call_info */


void record_nondependent_call(a_symbol_ptr              symbol,
                              a_token_sequence_number   tsn,
                              a_nondependent_call_depth depth,
            /* Defaulted: */  a_boolean                 supplemental,
                              a_boolean                 reversed_opnds)
/*
This routine is called within the scope of a template (either a template
declaration scope or a prototype instantiation) to record the result of
overload resolution for a nondependent call.  "symbol" is the function symbol
for the function to be called; it can be NULL for an error case.  "tsn" is a
token sequence number used to represent this call so that the entry can be
found during a real instantiation.  depth is usually zero, but serves as an
additional position disambiguator on tsn if non-zero.  supplemental is TRUE
if the call was for a C++20 comparison that that required a rewrite in terms
of a supplemental candidate (e.g., "<" rewritten via a "<=>" candidate).
reversed_opnds is TRUE if the call is to a C++20 comparison that requires an
implicit reversal of operands.
*/
{
  a_template_decl_info_ptr	tdip;
  a_nondependent_call_info_ptr	ndcip;

  /* Get the template declaration information entry associated with the
     current context. */
  tdip = get_specified_template_decl_info(/*innermost=*/TRUE);
#if DEBUG
/* GCOVR_EXCL_START */
  if (db_flag_is_set("nondep_call")) {
    fprintf(f_debug, "Recording nondependent call at %ld ", (long)tsn);
    if (depth != 0) {
      fprintf(f_debug, "(depth %lu) ", (unsigned long)depth);
    }  /* if */
    fprintf(f_debug, "to ");
    if (symbol != NULL) db_symbol_name(symbol);
    fprintf(f_debug, "\n");
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  /* Create a nondependent call entry. */
  ndcip = alloc_nondependent_call_info();
  ndcip->symbol = symbol;
  ndcip->token_sequence_number = tsn;
  ndcip->depth = depth;
  ndcip->supplemental = supplemental;
  ndcip->reversed_opnds = reversed_opnds;
  /* Add the entry to the appropriate point in the list.  This is usually
     immediately after the last entry added, but in certain cases we need
     to locate the appropriate insertion point. */
  if (tdip->nondependent_calls == NULL ||
      tdip->nondependent_calls->token_sequence_number > tsn ||
      (tdip->nondependent_calls->token_sequence_number == tsn &&
       tdip->nondependent_calls->depth > depth)) {
    /* Either the list is entry, or the token sequence number of this entry
       precedes the previous start of the list. */
    ndcip->next = tdip->nondependent_calls;
    if (tdip->nondependent_calls != NULL) {
      tdip->nondependent_calls->previous = ndcip;
    }  /* if */
    tdip->nondependent_calls = ndcip;
  } else {
    /* The new entry does not go at the start of the list.  See if the
       last_entry_added points to the right insert location. */
    a_nondependent_call_info_ptr	insert_loc;
    insert_loc = tdip->last_entry_added;
    /* If the token sequence number of the insert location is after the
       desired location, restart the search from the beginning of the list. */
    if (insert_loc->token_sequence_number > tsn ||
        (insert_loc->token_sequence_number == tsn &&
         insert_loc->depth > depth)) {
      insert_loc = tdip->nondependent_calls;
    }  /* if */
    /* Find an entry with a token sequence number greater than the one we
       are inserting, or the end of the list.  We are usually at the
       right place (i.e., nothing needs to be done). */
    while (insert_loc->next != NULL &&
           insert_loc->next->token_sequence_number < tsn) {
      insert_loc = insert_loc->next;
    }  /* while */
    while (insert_loc->next != NULL &&
           insert_loc->next->token_sequence_number == tsn &&
           insert_loc->next->depth < depth) {
      insert_loc = insert_loc->next;
    }  /* while */
    ndcip->next = insert_loc->next;
    ndcip->previous = insert_loc;
    if (insert_loc->next != NULL) {
      insert_loc->next->previous = ndcip;
    }  /* if */
    insert_loc->next = ndcip;
  }  /* if */
  tdip->last_entry_added = ndcip;
}  /* record_nondependent_call */


void check_for_nested_type_of_prototype_instantiation(a_symbol_ptr sym)
/*
If "sym" is a nested class of a prototype instantiation, create its nonreal
version.  See create_nonreal_version_of_nested_type for more information.
*/
{
  if (sym->is_class_member && is_prototype_instantiation_context()) {
    a_type_ptr	parent_class = sym_parent_class(sym);
    if (parent_class->variant.class_struct_union.is_prototype_instantiation) {
      create_nonreal_version_of_nested_type(sym);
    }  /* if */
  }  /* if */
}  /* check_for_nested_type_of_prototype_instantiation */

				
a_templ_friend_info_ptr alloc_templ_friend_info(void)
/*
Allocate a new template friend information entry, initialize its fields,
and return a pointer to it.
*/
{
  a_templ_friend_info_ptr  tfip;

  /* Allocate a template friend default argument entry. */
  tfip = (a_templ_friend_info_ptr)alloc_fe(sizeof(a_templ_friend_info));
  tfip->next = NULL;
  tfip->symbol = NULL;
  tfip->token_number = NO_TOKEN_SEQUENCE_NUMBER;
#if DEBUG
/* GCOVR_EXCL_START */
  num_templ_friend_info_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  return tfip;
}  /* alloc_templ_friend_info */


static a_namespace_symbol_supplement_ptr
                                  alloc_namespace_symbol_supplement(void)
/*
Allocate a new template symbol supplement entry, initialize its fields, and
return a pointer to it.
*/
{
  a_namespace_symbol_supplement_ptr  nssp;

  /* Allocate a namespace symbol supplement. */
  nssp = (a_namespace_symbol_supplement_ptr)
                   alloc_fe(sizeof(a_namespace_symbol_supplement));
  nssp->namespace_list_entry = NULL;
  nssp->symbol = NULL;
  nssp->using_dir_decl_seq = NO_DECL_SEQUENCE_NUMBER;
  nssp->name_qualifiers = NULL;
#if NEED_NAME_MANGLING
  nssp->last_unnamed_type_number = 0;
  nssp->last_closure_type_number = 0;
#endif /* NEED_NAME_MANGLING */
  nssp->visited_by_qualified_lookup = FALSE;
  nssp->within_unnamed_namespace = FALSE;
#if DEBUG
/* GCOVR_EXCL_START */
  num_namespace_symbol_supplements_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  clear_scope_pointers_block(&nssp->pointers_block);

  return nssp;
}  /* alloc_namespace_symbol_supplement */


void clear_template_cache(a_template_cache_ptr tcp)
/*
Initialize a template cache.
*/
{
  tcp->tokens = a_reusable_token_cache();
  tcp->decl_info = NULL;
}  /* clear_template_cache */


void set_template_cache_info(a_template_cache_ptr      tcp,
                             a_reusable_token_cache    tokens,
                             a_template_decl_info_ptr  tdip)
/*
Set the fields of a template cache entry.  Only set the tokens if a non-empty
shared object is passed.  Only set the decl_info if a non-NULL value is passed.
*/
{
  if (tokens.ptr() != NULL) {
    tcp->tokens = tokens;
  }  /* if */
  if (tdip != NULL) tcp->decl_info = tdip;
}  /* set_template_cache_info */


a_template_symbol_supplement_ptr alloc_template_symbol_supplement(
                                                          a_symbol_kind  kind)
/*
Allocate a new template symbol supplement entry, initialize its fields
appropriately (based on the kind of symbol with which it will be associated),
and return a pointer to it.
*/
{
  a_template_symbol_supplement_ptr  tssp;

  db_enter(5, "alloc_template_symbol_supplement");
  /* Allocate a template symbol supplement. */
  tssp = (a_template_symbol_supplement_ptr)
                   alloc_fe(sizeof(a_template_symbol_supplement));
#if DEBUG
/* GCOVR_EXCL_START */
  num_template_symbol_supplements_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  /* Initialize its fields. */
  tssp->pending_instantiations = 0;
  tssp->invalid_active_instantiation = NULL;
  tssp->pragmas_bound_to_template = NULL;
  tssp->token_sequence_number = NO_TOKEN_SEQUENCE_NUMBER;
  tssp->cache = new_fe<a_template_cache>();
  clear_template_cache(tssp->cache);
  tssp->befriending_classes = NULL;
  tssp->cache_segment = NULL;
  tssp->prototype_template = NULL;
  tssp->subordinate_templates = NULL;
  tssp->il_template_entry = NULL;
  tssp->all_instantiations = NULL;
  tssp->name = NULL;
  tssp->attributes = NULL;
  tssp->instantiation_hash_table = NULL;
  tssp->partial_specializations = NULL;
  tssp->primary_template_sym = NULL;
  tssp->is_specific_definition = FALSE;
  tssp->is_nonreal_member = FALSE;
  tssp->is_error = FALSE;
  tssp->is_variadic = FALSE;
  tssp->has_variadic_template_params = FALSE;
  tssp->has_template_param_constraint = FALSE;
  tssp->has_partial_spec_with_requires_clause = FALSE;
  tssp->is_generic = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  tssp->is_delegate = FALSE;
  tssp->from_metadata = FALSE;
  tssp->generic_constraints_pending = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  switch (kind) {
    case sk_class_template:
    case sk_class_or_struct_tag:
    case sk_union_tag:
    case sk_enum_tag:
      tssp->variant.class_template.instantiations = NULL;
      tssp->variant.class_template.type_kind = (a_type_kind)tk_error;
      tssp->variant.class_template.prototype_instantiation = NULL;
      tssp->variant.class_template.out_of_class_partial_specs = NULL;
      tssp->variant.class_template.friend_info = NULL;
      tssp->variant.class_template.is_alias_template = FALSE;
      tssp->variant.class_template.prototype_instantiation_complete = FALSE;
      tssp->variant.class_template.access = (an_access_specifier)as_public;
      tssp->variant.class_template.name_linkage =
                                            (a_name_linkage_kind)nlk_none;
#if MICROSOFT_EXTENSIONS_ALLOWED
      tssp->variant.class_template.is_interface = FALSE;
      tssp->variant.class_template.generic_arity_list = NULL;
      tssp->variant.class_template.non_generic_class = NULL;
      tssp->variant.class_template.arity = 0;
      tssp->variant.class_template.min_arity = 0;
      tssp->variant.class_template.max_arity = 0;
      tssp->variant.class_template.pending_nonreal_instantiations = 0;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      tssp->variant.class_template.not_standalone_nested_class = FALSE;
      tssp->variant.class_template.template_template_param = FALSE;
      tssp->variant.class_template.def_templ_templ_arg_check_delayed = FALSE;
      tssp->variant.class_template.involves_template_param = FALSE;
      tssp->variant.class_template.any_full_instantiations = FALSE;
      tssp->variant.class_template.alias_uses_own_type = FALSE;
      tssp->variant.class_template.cannot_be_specialized = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
      tssp->variant.class_template.any_ms_instantiated_nonreal_classes = FALSE;
      tssp->variant.class_template.has_ms_undeclared_base_class = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      tssp->variant.class_template.explicit_deduction_guides_added = FALSE;
      tssp->variant.class_template.implicit_deduction_guides_added = FALSE;
      tssp->variant.class_template.interim_implicit_deduction_guides = FALSE;
      tssp->variant.class_template.has_alias_params_not_in_type = FALSE;
      tssp->variant.class_template.invented_template = FALSE;
      tssp->variant.class_template.argument_template = NULL;
      tssp->variant.class_template.substituted_param_template = NULL;
      tssp->variant.class_template.deduction_guides = NULL;
      tssp->variant.class_template.initial_decl_cache =
                                                    new_fe<a_template_cache>();
      clear_template_cache(tssp->variant.class_template.initial_decl_cache);
#if CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
      tssp->variant.class_template.source_sequence_list = NULL;
#endif /* CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
      break;
    case sk_function_template:
    case sk_member_function:
      tssp->variant.function.instantiations = NULL;
      tssp->variant.function.routine = NULL;
      clear_func_info(&tssp->variant.function.func_info);
      tssp->variant.function.def_arg_expr_list = NULL;
      tssp->variant.function.decl_cache = new_fe<a_template_cache>();
      clear_template_cache(tssp->variant.function.decl_cache);
      tssp->variant.function.exception_spec_arg_cache =
                                                    new_fe<a_template_cache>();
      clear_template_cache(tssp->variant.function.exception_spec_arg_cache);
      tssp->variant.function.substituted_types_table = NULL;
      tssp->variant.function.unused_instantiations = 0;
      tssp->variant.function.pending_partial_instantiations = 0;
      tssp->variant.function.pending_deductions = 0;
      tssp->variant.function.prototype_friend_symbol = NULL;
      tssp->variant.function.invented_partial_ordering_param = NULL;
      tssp->variant.function.template_param_not_in_function_type = FALSE;
      tssp->variant.function.constructor_symbol_for_guide = NULL;
      tssp->variant.function.has_prototype_instantiation = FALSE;
      tssp->
          variant.function.exception_spec_prototype_instantiation_done = FALSE;
      tssp->variant.function.must_have_only_one_decl = FALSE;
      tssp->variant.function.implicit_deduction_guide = FALSE;
      break;
    case sk_static_data_member:
    case sk_variable_template:
      tssp->variant.variable.definitions = NULL;
      tssp->variant.variable.has_out_of_class_definition = FALSE;
      tssp->variant.variable.instantiations = NULL;
      tssp->variant.variable.prototype_variable = NULL;
      tssp->variant.variable.decl_cache = new_fe<a_template_cache>();
      clear_template_cache(tssp->variant.variable.decl_cache);
      tssp->variant.variable.declarator_name_tsn = NO_TOKEN_SEQUENCE_NUMBER;
     break;
    case sk_concept_template:
     /* No variant fields. */
     break;
    default:
      unexpected_condition_str(
                          "alloc_template_symbol_supplement: bad symbol kind");
  }  /* switch */
  if (collect_top_templates) {
    /* Record the supplement for the --top_templates report.  This is done
       only when the option is in effect to avoid growing inst_counters
       needlessly. */
    inst_counters->push_back(an_inst_count{ kind, tssp, 0, 0 });
  }  /* if */
  db_exit();
  return tssp;
}  /* alloc_template_symbol_supplement */

#if MICROSOFT_EXTENSIONS_ALLOWED

a_boolean is_cppcx_externally_visible_symbol(a_symbol_ptr sym)
/*
Returns TRUE if this symbol is considered externally visible (i.e., it will be
emitted into metadata) in C++/CX mode.
*/
{
  a_boolean             result = FALSE;  /* Assume. */
  
  check_assertion(cppcx_enabled);
  if (sym->is_class_member && is_managed_class_type(sym->parent.class_type)) {
    /* Any member inside a C++/CX type with a declared assembly access of
      "public" or "protected" is considered externally visible. */
    an_access_specifier  assembly_access = enum_cast<an_access_specifier>(
                        source_corresp_entry_for_symbol(sym)->assembly_access);
    result = is_cppcx_externally_visible_assembly_access(assembly_access);
  }  /* if */
  return result;
}  /* is_cppcx_externally_visible_symbol */


static
a_property_set_symbol_supplement_ptr alloc_property_set_symbol_supplement(void)
/*
Allocate a new property set symbol supplement and initialize its fields to
null.  (Used for C++/CLI properties.)
*/
{
  a_property_set_symbol_supplement_ptr  psssp;

  /* Allocate a property set symbol supplement. */
  psssp = (a_property_set_symbol_supplement_ptr)
                           alloc_fe(sizeof(a_property_set_symbol_supplement));
#if DEBUG
/* GCOVR_EXCL_START */
  num_property_set_symbol_supplements_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  /* Initialize its fields. */
  psssp->properties = NULL;
  psssp->get_accessors = NULL;
  psssp->set_accessors = NULL;
  return psssp;
}  /* alloc_property_set_symbol_supplement */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */


a_static_data_member_supplement_ptr alloc_static_data_member_supplement(
                                                        a_symbol_ptr  sdm_sym)
/*
Allocate a static data member supplement entry and return a pointer to it.
This function should normally only be called through the macro get_sdm_supp.
*/
{
  a_static_data_member_supplement_ptr
                    sdmsp = alloc_fe_of_type(a_static_data_member_supplement);
  sdmsp->token_sequence_number = NO_TOKEN_SEQUENCE_NUMBER;
  new (&sdmsp->token_cache) a_shared_token_cache();
  sdmsp->prototype_member = NULL;
#if DEBUG
/* GCOVR_EXCL_START */
  num_static_data_member_supplements_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  sdm_sym->variant.static_data_member.extra_info = sdmsp;
  return sdmsp;
}  /* alloc_static_data_member_supplement */


void set_symbol_kind(a_symbol_ptr   sym_ptr,
		     a_symbol_kind  sym_kind)
/*
Set the symbol's kind and initialize the associated variant fields to a safe
state.
*/
{
  db_enter(5, "set_symbol_kind");

  sym_ptr->kind = sym_kind;
  switch (sym_kind) {
    case sk_undefined:
      /* No variant fields to set. */
      break;
    case sk_keyword:
      sym_ptr->variant.keyword.token = tok_error;
      sym_ptr->variant.keyword.is_preprocessing_op_or_punc = FALSE;
      sym_ptr->variant.keyword.diagnostic_issued_if_used = ec_no_error;
      break;
    case sk_macro:
      sym_ptr->variant.macro_def = NULL;
      break;
    case sk_constant:
      sym_ptr->variant.constant = NULL;
      break;
    case sk_type:
      sym_ptr->variant.type.ptr = NULL;
#if IA64_ABI && NEED_NAME_MANGLING
      sym_ptr->variant.type.discriminator = 0;
#endif /* IA64_ABI && NEED_NAME_MANGLING */
      sym_ptr->variant.type.is_injected_class_name = FALSE;
      break;
    case sk_enum_tag:
      sym_ptr->variant.enumeration.type = NULL;
      { an_enum_symbol_supplement  *essp;
        essp = (an_enum_symbol_supplement_ptr)alloc_fe(
                                            sizeof(an_enum_symbol_supplement));
#if DEBUG
/* GCOVR_EXCL_START */
        num_enum_symbol_supplements_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
        sym_ptr->variant.enumeration.extra_info = essp;
        essp->dependent_type_fixup_list = NULL;
        essp->name_qualifiers = NULL;
#if NEED_NAME_MANGLING
        essp->discriminator = 0;
#endif /* NEED_NAME_MANGLING */
        essp->template_sym = NULL;
        essp->template_info = NULL;
        essp->instantiation_position = null_source_position;
        essp->instantiated = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
        essp->replaced_enum_symbol = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      }
      break;
    case sk_class_or_struct_tag:
    case sk_union_tag:
      sym_ptr->variant.class_struct_union.type = NULL;
      { a_class_symbol_supplement  *cssp;
        cssp = (a_class_symbol_supplement_ptr)alloc_fe(
                                            sizeof(a_class_symbol_supplement));
#if DEBUG
/* GCOVR_EXCL_START */
        num_class_symbol_supplements_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
        sym_ptr->variant.class_struct_union.extra_info = cssp;
        /* Note: Some of the fields cleared below must also be cleared in
           check_anonymous_union_symbols (class_decl.c). */
        cssp->symbols = NULL;
        cssp->constructor = NULL;
        cssp->trivial_default_constructor = NULL;
        cssp->destructor = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
        cssp->static_constructor = NULL;
        cssp->finalizer = NULL;
        cssp->idisposable_dispose = NULL;
        cssp->dispose_bool = NULL;
        cssp->object_finalize = NULL;
        cssp->disable_dispose_pattern_implementation = FALSE;
        cssp->checked_for_dispose_pattern = FALSE;
        cssp->is_disposable = FALSE;
        cssp->any_disposable_data_members = FALSE;
        cssp->has_dispose_pattern_idisposable_dispose = FALSE;
        cssp->has_dispose_pattern_object_finalize = FALSE;
        cssp->needs_new_idisposable_dispose = FALSE;
        cssp->from_vccorlib = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        cssp->being_defined = FALSE;
        cssp->may_need_fixups = FALSE;
        cssp->union_member_with_initializer = FALSE;
        cssp->variant_member_with_nontrivial_default_ctor = FALSE;
        cssp->variant_member_with_nontrivial_copy_ctor = FALSE;
        cssp->variant_member_with_nontrivial_move_ctor = FALSE;
        cssp->variant_member_with_nontrivial_dtor = FALSE;
        cssp->variant_member_with_nontrivial_copy_assign = FALSE;
        cssp->variant_member_with_nontrivial_move_assign = FALSE;
        cssp->known_to_be_a_literal_type = FALSE;
        cssp->known_not_to_be_a_literal_type = FALSE;
        cssp->has_constexpr_nonstatic_member_function = FALSE;
        cssp->scanning_field_initializer = FALSE;
        cssp->has_instantiatable_field_initializers = FALSE;
        cssp->has_initializer_fixups = FALSE;
        cssp->default_ctor_body_delayed = FALSE;
        cssp->base_classes_fixed = FALSE;
        cssp->assignment_operator = NULL;
        cssp->conversion_list = NULL;
        cssp->conversion_template_list = NULL;
        cssp->routine_fixup_list = NULL;
        cssp->initializer_fixup_list = NULL;
        cssp->class_template = NULL;
        cssp->template_info = NULL;
        cssp->member_decl_scope = NO_SCOPE_NUMBER;
        cssp->num_unparsed_field_initializers = 0;
        cssp->instantiation_position = null_source_position;
        cssp->corresp_prototype_sym = NULL;
        cssp->prototype_token_sequence_number = NO_TOKEN_SEQUENCE_NUMBER;
        cssp->referencing_namespace = NULL;
        cssp->dependent_type_fixup_list = NULL;
        cssp->operator_lookup_namespaces = NULL;
        cssp->friend_functions = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
        cssp->super_lookup_symbols = NULL;
        cssp->default_indexed_properties = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        cssp->name_qualifiers = NULL;
        cssp->prev_entry_on_types_list = NULL;
#if NEED_NAME_MANGLING
        cssp->discriminator = 0;
#endif /* NEED_NAME_MANGLING */
        cssp->has_nontrivial_default_constructor = FALSE;
        cssp->has_user_declared_default_constructor = FALSE;
        cssp->has_user_provided_default_constructor = FALSE;
        cssp->has_copy_constructor = FALSE;
        cssp->has_copy_constructor_for_const_object = FALSE;
        cssp->has_user_provided_copy_constructor = FALSE;
        cssp->has_user_declared_move_constructor = FALSE;
        cssp->has_user_provided_move_constructor = FALSE;
        cssp->has_deleted_copy_or_move_constructor = FALSE;
        cssp->has_trivial_destructor = FALSE;
        cssp->has_user_declared_move_assign_operator = FALSE;
        cssp->has_user_provided_move_assign_operator = FALSE;
        cssp->has_deleted_copy_or_move_assign_operator = FALSE;
        cssp->assignment_by_bitwise_copy_allowed = FALSE;
        cssp->construction_by_bitwise_copy_allowed = FALSE;
        cssp->makes_copy_construction_nontrivial = FALSE;
        cssp->makes_move_construction_nontrivial = FALSE;
        cssp->makes_copy_assignment_nontrivial = FALSE;
        cssp->makes_move_assignment_nontrivial = FALSE;
        cssp->contains_vtable = FALSE;
        cssp->has_auto_conversion_function = FALSE;
        cssp->target_of_conversion_function = FALSE;
        cssp->any_ref_member = FALSE;
        /* The is_class_aggregate flag is initialized to TRUE when we are not
           in C++ mode. */
        cssp->is_class_aggregate = (C_dialect != C_dialect_cplusplus);
        cssp->is_cpp03_POD = FALSE;
        cssp->is_pod_class = FALSE;
        cssp->pod_checked = FALSE;
        cssp->any_template_dependent_fields = FALSE;
        cssp->has_operator_new = FALSE;
        cssp->has_operator_array_new = FALSE;
        cssp->has_operator_delete = FALSE;
        cssp->has_operator_array_delete = FALSE;
        cssp->has_two_argument_operator_array_delete = FALSE;
        cssp->any_nonstatic_data_members = FALSE;
        cssp->any_nonreal_base_classes = FALSE;
        cssp->any_dependent_base_classes = FALSE;
        cssp->instantiation_in_progress = FALSE;
        cssp->default_arg_fixup_pass_1_started = FALSE;
        cssp->default_arg_fixup_pass_2_started = FALSE;
#if IA64_ABI
        cssp->has_empty_class_subobject = FALSE;
#endif /* IA64_ABI */
#if GENERATE_SOURCE_SEQUENCE_LISTS
        cssp->definition_is_first_decl = FALSE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        cssp->lambda_inside_default_arg_expression = FALSE;
        cssp->lambda_immediately_inside_default_arg_expression = FALSE;
        cssp->lambda_in_invalid_scope = FALSE;
        cssp->lambda_subject_to_trans_unit_corresp = FALSE;
        cssp->base_check = FALSE;
        cssp->check_hiding_attr = FALSE;
        cssp->has_field_with_attr_to_merge = FALSE;
        cssp->standard_layout = TRUE;
        clear_scope_pointers_block(&cssp->pointers_block);
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
        cssp->ss_list_depth = NO_SCOPE_DEPTH;
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      }
      break;
    case sk_variable:
      sym_ptr->variant.variable.ptr = NULL;
      sym_ptr->variant.variable.instance_ptr = NULL;
#if IA64_ABI && NEED_NAME_MANGLING
      sym_ptr->variant.variable.discriminator = 0;
#endif /* IA64_ABI && NEED_NAME_MANGLING */
      break;
    case sk_static_data_member:
      sym_ptr->variant.static_data_member.variable = NULL;
      sym_ptr->variant.static_data_member.instance_ptr = NULL;
      sym_ptr->variant.static_data_member.extra_info = NULL;
      break;
    case sk_field:
      sym_ptr->variant.field.ptr = NULL;
      sym_ptr->variant.field.anonymous_parent_object = NULL;
      { a_field_symbol_supplement  *fssp;
        fssp = alloc_fe_of_type(a_field_symbol_supplement);
        fssp->token_sequence_number = NO_TOKEN_SEQUENCE_NUMBER;
        new (&fssp->token_cache) a_shared_token_cache();
        fssp->prototype_field = NULL;
        fssp->pending_instantiations = 0;
        fssp->being_instantiated = FALSE;
        fssp->is_variant_member = FALSE;
        fssp->is_first_variant_member = FALSE;
        fssp->is_last_variant_member = FALSE;
#if DEBUG
/* GCOVR_EXCL_START */
        num_field_symbol_supplements_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
        sym_ptr->variant.field.extra_info = fssp;
      }
      break;
    case sk_routine:
    case sk_member_function:
      sym_ptr->variant.routine.ptr = NULL;
      sym_ptr->variant.routine.instance_ptr = NULL;
      sym_ptr->variant.routine.pending_trailing_requires_clause = FALSE;
      break;
    case sk_label:
      sym_ptr->variant.label.ptr = NULL;
      sym_ptr->variant.label.assoc_control_flow_descr = NULL;
      break;
    case sk_extern_variable:
    case sk_extern_routine:
      { an_extern_symbol_descr_ptr esdp;
        esdp = (an_extern_symbol_descr_ptr)alloc_fe(
                                               sizeof(an_extern_symbol_descr));
#if DEBUG
/* GCOVR_EXCL_START */
        num_extern_symbol_descrs_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
        sym_ptr->variant.extern_symbol_descr = esdp;
        esdp->type = NULL;
        /* Both the variable and routine pointer are cleared even though
           they share the same location.  This is done to support a
           testing mode in which they do not actually share the same
           location. */
        esdp->variant.variable = NULL;
        esdp->variant.routine.ptr = NULL;
	esdp->variant.routine.is_implicit_declaration = FALSE;
      }
      break;
    case sk_projection:
      { a_projection_descr *pdp;
        pdp = (a_projection_descr_ptr)alloc_fe(sizeof(a_projection_descr));
#if DEBUG
/* GCOVR_EXCL_START */
        num_projection_descrs_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
        pdp->fundamental_symbol     = NULL;
        pdp->fundamental_base_class = NULL;
        pdp->naming_type            = NULL;
        sym_ptr->variant.projection.extra_info= pdp;
        sym_ptr->variant.projection.access    = (an_access_specifier)as_public;
        sym_ptr->variant.projection.is_using_decl = FALSE;
        sym_ptr->variant.projection.any_intervening_using_decl = FALSE;
        sym_ptr->variant.projection.fund_sym_is_nonreal_member = FALSE;
        sym_ptr->variant.projection.
                     injected_class_template_name_is_unambiguous = FALSE;
      }
      break;
    case sk_overloaded_function:
      sym_ptr->variant.overloaded_function.symbols = NULL;
      sym_ptr->variant.overloaded_function.mixed_static_nonstatic = FALSE;
      break;
    case sk_parameter:
      sym_ptr->variant.param_id = NULL;
      break;
    case sk_class_template:
    case sk_function_template:
    case sk_variable_template:
    case sk_concept_template:
      sym_ptr->variant.template_info =
                             alloc_template_symbol_supplement(sym_ptr->kind);
      break;
    case sk_namespace:
      sym_ptr->variant.namespace_info.ptr = NULL;
      sym_ptr->variant.namespace_info.extra_info =
                                  alloc_namespace_symbol_supplement();
      sym_ptr->variant.namespace_info.extra_info->symbol = sym_ptr;
      break;
    case sk_namespace_projection:
      sym_ptr->variant.namespace_projection.fundamental_symbol = NULL;
      sym_ptr->variant.namespace_projection.access = as_public;
      sym_ptr->variant.namespace_projection.is_using_decl = FALSE;
      break;
    case sk_named_module:
      sym_ptr->variant.module_info.primary_name = NULL;
      sym_ptr->variant.module_info.partition_name = NULL;
      sym_ptr->variant.module_info.is_interface_unit = FALSE;
      sym_ptr->variant.module_info.is_header_unit = FALSE;
      break;
#if NAMED_ADDRESS_SPACES_ALLOWED
    case sk_named_address_space:
      sym_ptr->variant.named_address_space.id = 0;
      break;
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
#if NAMED_REGISTERS_ALLOWED
    case sk_named_register:
      sym_ptr->variant.named_register.id = 0;
      break;
#endif /* NAMED_REGISTERS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case sk_property_set:
      sym_ptr->variant.property_info = alloc_property_set_symbol_supplement();
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    default:
      unexpected_condition_str("set_symbol_kind: bad symbol kind");
  }  /* switch */

  db_exit();
}  /* set_symbol_kind */


static void init_symbol(a_symbol_ptr        sym_ptr,
                        a_symbol_kind       kind,
                        a_symbol_header_ptr hdr_ptr,
                        a_source_position   *position)
/*
Initialize the fields of sym_ptr to a safe state.  Set the
kind to kind, the header to hdr_ptr, and the decl_position to *position.
hdr_ptr == NULL indicates that an error symbol should be constructed.
*/
{
  /* Set the shared fields to default values, set the kind, and initialize
     its variant fields. */
  clear_symbol(sym_ptr, kind);
  /* Set the header. */
  if (hdr_ptr == NULL) {
    /* Use the error symbol header. */
    hdr_ptr = get_error_symbol_header();
  }  /* if */
  sym_ptr->header = hdr_ptr;
  /* Set the declaration source position. */
  sym_ptr->decl_position = *position;
  /* Mark this symbol as associated with the current module entity (if any).

     If this is a module isolation context, the module entity is a temporary
     value.  Additionally, the IL entity is being created in a special context
     from which it shouldn't escape for purposes of entity identification; its
     symbol does not need to be marked with the associated module entity. */
  if (!is_module_isolation_context()) {
    if (module_entity_stack != NULL && !module_entity_stack->is_empty()) {
      a_module_entity_stack_entry &mese = module_entity_stack->back_elem();

      sym_ptr->module_entity = mese.mep;
    }  /* if */
  }  /* if */
}  /* init_symbol */

#if EXPENSIVE_CHECKING

using an_allocated_symbols_list = Dyn_array<a_symbol_ptr, General_allocator>;
                        /* The type for a list of symbols allocated in the
                           current translation unit. */

STATIC_THREAD an_allocated_symbols_list
                *allocated_symbols;
                        /* The symbols allocated in the current translation
                           unit.  This is used for post-memory region and
                           post-compilation sanity checks.  Note that this is
                           only populated when no_very_expensive_checking is
                           FALSE. */

#endif /* EXPENSIVE_CHECKING */

a_symbol_ptr alloc_symbol(a_symbol_kind       kind,
                          a_symbol_header_ptr hdr_ptr,
                          a_source_position   *position)
/*
Allocate a symbol and initialize the fields to a safe state.  Set the
kind to kind, the header to hdr_ptr, and the decl_position to *position.
hdr_ptr == NULL indicates that an error symbol should be constructed.
*/
{
  a_symbol_ptr sym_ptr;

  db_enter(5, "alloc_symbol");

  sym_ptr = (a_symbol_ptr)alloc_fe(sizeof(a_symbol));
#if DEBUG
/* GCOVR_EXCL_START */
  num_symbols_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
#if EXPENSIVE_CHECKING
  /* Add the symbol to the list of allocated symbols to check. */
  if (!no_very_expensive_checking) {
    allocated_symbols->push_back(sym_ptr);
  }  /* if */
#endif /* EXPENSIVE_CHECKING */
  /* Set the shared fields to default values, set the kind, and initialize
     its variant fields. */
  init_symbol(sym_ptr, kind, hdr_ptr, position);
  db_exit();
  return sym_ptr;
}  /* alloc_symbol */


/*
A pointer to an sk_undefined symbol.  This symbol is never entered into
the symbol table.
*/
STATIC_THREAD a_symbol_ptr
                dummy_undefined_symbol;


a_symbol_ptr make_dummy_undefined_symbol(a_symbol_header_ptr hdr_ptr,
                                         a_source_position   *position)
/*
Create if necessary and return an sk_undefined symbol for temporary use.
The same symbol is returned each time, so the caller should not store its
address.  The symbol is re-initialized each time and the header is set
to hdr_ptr and decl_position to *position.
*/
{
  if (dummy_undefined_symbol == NULL) {
    dummy_undefined_symbol = alloc_symbol((a_symbol_kind)sk_undefined, hdr_ptr,
                                    position);
  } else {
    init_symbol(dummy_undefined_symbol, (a_symbol_kind)sk_undefined, hdr_ptr,
                position);
  }  /* if */
  return dummy_undefined_symbol;
}  /* make_dummy_undefined_symbol */


void unlink_symbol_from_symbol_table(a_symbol_ptr sym_ptr)
/*
Remove a symbol from the symbol table, i.e., unlink it from either the main
(active) symbol list or the inactive list of its symbol header.
*/
{
  a_symbol_ptr        ptr, prev_ptr;
  a_symbol_header_ptr hdr_ptr;

  db_enter(4, "unlink_symbol_from_symbol_table");
  if (sym_ptr->is_error) {
    /* Error symbols are never added to a symbol list and cannot be removed. */
  } else if (sym_ptr->kind == (a_symbol_kind)sk_extern_variable ||
             sym_ptr->kind == (a_symbol_kind)sk_extern_routine) {
    /* These symbols are not in the symbol table proper. */
  } else {
    hdr_ptr = sym_ptr->header;
    if (sym_ptr == hdr_ptr->symbol) {
      /* The symbol is the first on the header list.  Link around it. */
      hdr_ptr->symbol = sym_ptr->next;
    } else if (sym_ptr == hdr_ptr->inactive_symbols) {
      /* The symbol is the first on the inactive list.  Link around it. */
      hdr_ptr->inactive_symbols = sym_ptr->next;
    } else {
      /* The symbol is not the first on either of the lists.  Find it on one
         of the lists, remembering the preceding symbol. */
      ptr = prev_ptr = NULL;
      if (hdr_ptr->symbol != NULL) {
        /* Check the active list. */
        prev_ptr = hdr_ptr->symbol;
        ptr = prev_ptr->next;
        while (ptr != NULL && ptr != sym_ptr) {
          prev_ptr = ptr;
          ptr = ptr->next;
        }  /* while */
      }  /* if */
      if (ptr == NULL) {
        /* It wasn't found on the active list.  Check the inactive list. */
        if (hdr_ptr->inactive_symbols != NULL) {
          prev_ptr = hdr_ptr->inactive_symbols;
          ptr = prev_ptr->next;
          while (ptr != NULL && ptr != sym_ptr) {
            prev_ptr = ptr;
            ptr = ptr->next;
          }  /* while */
        }  /* if */
      }  /* if */
#if CHECKING
      if (ptr == NULL) {
#if DEBUG
/* GCOVR_EXCL_START */
        if (debug_level > 0) {
          fprintf(f_debug, "Symbol name = %s\n", hdr_ptr->identifier);
        }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
        internal_error(
                  "unlink_symbol_from_symbol_table: cannot find symbol entry");
      }  /* if */
#endif /* CHECKING */
      /* Found the entry on one of the lists.  Link around it. */
      prev_ptr->next = sym_ptr->next;
    }  /* if */
  }  /* if */
  sym_ptr->next = NULL;
  db_exit();
}  /* unlink_symbol_from_symbol_table */


void remove_symbol_from_overload_set(a_symbol_ptr  sym,
                                     a_symbol_ptr  ovl_set)
/*
Remove sym from the list headed by ovl_set (a symbol of kind
sk_overloaded_function).
*/
{
  a_symbol_ptr  *p = &ovl_set->variant.overloaded_function.symbols;

  while (*p != sym) {
    p = &(*p)->next;
    check_assertion(*p != NULL);
  }  /* while */
  *p = (*p)->next;
}  /* remove_symbol_from_overload_set */


static void remove_symbol_from_no_scope_list(a_symbol_ptr sym_ptr)
/*
Remove a symbol from the symbols_with_no_scope list.
*/
{
  if (sym_ptr == symbols_with_no_scope) {
    symbols_with_no_scope = sym_ptr->next_in_scope;
  } else {
    sym_ptr->prev_in_scope->next_in_scope = sym_ptr->next_in_scope;
  }  /* if */
  if (sym_ptr->next_in_scope != NULL) {
    sym_ptr->next_in_scope->prev_in_scope = sym_ptr->prev_in_scope;
  }  /* if */
  /* If the removed entry is the last entry on the list, update the
     last-pointer. */
  if (sym_ptr == symbols_with_no_scope_tail) {
    symbols_with_no_scope_tail = sym_ptr->prev_in_scope;
  }  /* if */
}  /* remove_symbol_from_no_scope_list */


/*
Entry used to build a hash table for looking up symbols with a given
symbol header in a scope.
*/
typedef struct a_symbol_header_lookup_entry *a_symbol_header_lookup_entry_ptr;
typedef struct a_symbol_header_lookup_entry {
  a_symbol_header_ptr	header;
				/* The symbol header associated with this
				   entry. */
  a_symbol_ptr
			symbols;
				/* A list of symbol entries that have the
				   given header.  Can be NULL. */
} a_symbol_header_lookup_entry;


static void clear_symbol_header_lookup_entry(
				a_symbol_header_lookup_entry_ptr shlep)
/*
Initialize the fields of a symbol header lookup entry.
*/
{
  shlep->header = NULL;
  shlep->symbols = NULL;
}  /* clear_symbol_header_lookup_entry */


static a_symbol_header_lookup_entry_ptr alloc_symbol_header_lookup_entry(void)
/*
Allocate a symbol header lookup entry, initialize its fields, and
return a pointer to it.
*/
{
  a_symbol_header_lookup_entry_ptr	shlep;

  shlep = alloc_fe_of_type(a_symbol_header_lookup_entry);
#if DEBUG
/* GCOVR_EXCL_START */
  num_symbol_header_lookup_entries_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  clear_symbol_header_lookup_entry(shlep);
  return shlep;
}  /* alloc_symbol_header_lookup_entry */


void remove_symbol_from_lookup_table(
				a_symbol_ptr        symbol,
				a_hash_table_ptr    lookup_table)
/*
Remove symbol from lookup_table.
*/
{
  a_symbol_header_lookup_entry_ptr	shlep;
  a_symbol_header_lookup_entry_ptr	*shlep_in_table;
  a_symbol_header_lookup_entry		shle_key;
  a_symbol_ptr				*p_sym;
  a_symbol_ptr				table_sym;

  /* Some scopes do not have lookup tables.  Do nothing in that case. */
  if (lookup_table != NULL) {
    /* Create an entry to be used as the lookup key. */
    clear_symbol_header_lookup_entry(&shle_key);
    shle_key.header = symbol->header;
    shlep_in_table = (a_symbol_header_lookup_entry_ptr*)
                              hash_find(lookup_table,
					(a_void_ptr)&shle_key,
					/*create=*/FALSE);
    shlep = *shlep_in_table;
    check_assertion(shlep != NULL);
    /* Find the entry on the symbol list and remove it. */
    p_sym = &shlep->symbols;
    for (table_sym = *p_sym; table_sym != NULL;
         p_sym = &table_sym->next_in_lookup_table,
           table_sym = table_sym->next_in_lookup_table) {
      if (table_sym == symbol) break;
    }  /* for */
    check_assertion(table_sym != NULL);
    /* Unlink the entry from the list. */
    *p_sym = symbol->next_in_lookup_table;
    symbol->next_in_lookup_table = NULL;
  }  /* if */
}  /* remove_symbol_from_lookup_table */


static a_boolean is_scope_kind_with_lookup_table(a_scope_kind	kind)
/*
Return TRUE if the scope kind is one for which a lookup table will be
created if needed.
*/
{
  a_boolean	result = FALSE;

  switch (kind) {
    case sck_file:
    case sck_namespace:
    case sck_namespace_extension:
    case sck_class_struct_union:
    case sck_class_reactivation:
      result = TRUE;
      break;
    case sck_func_prototype:
    case sck_block:
    case sck_template_declaration:
    case sck_template_instantiation:
    case sck_module_decl_import:
    case sck_pragma:
    case sck_condition:
    case sck_enum:
    case sck_function:
      break;
    default:
#if DEBUG
/* GCOVR_EXCL_START */
      fprintf(f_debug, "Bad scope kind:\n");
      (void)db_scope_kind(kind);
/* GCOVR_EXCL_STOP */
#endif  /* DEBUG */
      unexpected_condition_str("is_scope_kind_with_lookup_table");
      break;
  }  /* switch */
  return result;
}  /* is_scope_kind_with_lookup_table */


static void remove_symbol_from_scope_list(a_symbol_ptr sym_ptr)
/*
Remove the given symbol from the list of symbols for its scope.
*/
{
  a_scope_stack_entry_ptr     ssep;
  a_scope_pointers_block_ptr  pointers_block;
  a_scope_kind	              scope_kind;

  if (sym_ptr->is_error) {
    /* Error symbols are not on the scope list and cannot be removed. */
  } else if (sym_ptr->decl_scope == NO_SCOPE_NUMBER) {
    /* Symbols removed by a command-line -U option can be outside of any
       scope. */
    remove_symbol_from_no_scope_list(sym_ptr);
  } else {
    /* Find the proper entry in the scope stack (it will almost always be
       the topmost entry). */
#if CHECKING
    if (depth_scope_stack < 0) {
      internal_error("remove_symbol_from_scope_list: empty scope stack");
    }  /* if */
#endif /* CHECKING */
    for (ssep = &scope_stack[depth_scope_stack];
         sym_ptr->decl_scope != ssep->number;
         ssep--) {
#if CHECKING
      if (ssep == &scope_stack[0]) {
#if DEBUG
/* GCOVR_EXCL_START */
        if (debug_level > 0) {
          fprintf(f_debug, "Symbol name = %s\n", sym_ptr->header->identifier);
        }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
        internal_error("remove_symbol_from_scope_list: bad scope");
      }  /* if */
#endif /* CHECKING */
    }  /* for */
    /* Usually (i.e., when this routine is called from pop_scope), the
       symbol will be the first on the list.  If not, we have to find the
       previous symbol. */
    pointers_block = assoc_pointers_block_of(ssep);
    scope_kind = ssep->kind;
    if (sym_ptr == pointers_block->symbols) {
      pointers_block->symbols = sym_ptr->next_in_scope;
    } else {
      sym_ptr->prev_in_scope->next_in_scope = sym_ptr->next_in_scope;
    }  /* if */
    if (sym_ptr->next_in_scope != NULL) {
      sym_ptr->next_in_scope->prev_in_scope = sym_ptr->prev_in_scope;
    }  /* if */
    /* If the removed entry is the last entry on the list, update the
       last-pointer. */
    if (sym_ptr == pointers_block->last_symbol) {
      pointers_block->last_symbol = sym_ptr->prev_in_scope;
    }  /* if */
    if (is_scope_kind_with_lookup_table(scope_kind)) {
      a_module_ptr     module_ptr = lookup_module_for_symbol(sym_ptr);
      a_hash_table_ptr lookup_table = curr_lookup_table(pointers_block,
                                                        module_ptr,
                                                        scope_kind);
      remove_symbol_from_lookup_table(sym_ptr, lookup_table);
    }  /* if */
  }  /* if */
  sym_ptr->next_in_scope = NULL;
  sym_ptr->prev_in_scope = NULL;
}  /* remove_symbol_from_scope_list */


void remove_symbol(a_symbol_ptr sym_ptr)
/*
Unlink a symbol from the symbol table and remove it from the list of symbols
for the scope it's in.
*/
{
  db_enter(4, "remove_symbol");
  /* Remove the symbol from the symbol table. */
  unlink_symbol_from_symbol_table(sym_ptr);
  /* Remove the symbol from the list of symbols declared in its scope. */
  remove_symbol_from_scope_list(sym_ptr);
  db_exit();
}  /* remove_symbol */


void remove_anonymous_union_member_from_inactive_symbols_list
                                                       (a_symbol_ptr sym_ptr)
/*
Remove the indicated symbol from its header's inactive symbol list.
This is used in removing symbols inside unnamed unions before re-entering
them up one level.
*/
{
  a_symbol_header_ptr hdr_ptr = sym_ptr->header;
  a_symbol_ptr        prev_sym;

  db_enter(4, "remove_anonymous_union_member_from_inactive_symbol_list");
  
  if (sym_ptr == hdr_ptr->inactive_symbols) {
    /* The symbol is the first one on the list. */
    hdr_ptr->inactive_symbols = sym_ptr->next;
  } else {
    /* Find the previous entry on the list. */
    for (prev_sym = hdr_ptr->inactive_symbols;
         prev_sym->next != sym_ptr;
         prev_sym = prev_sym->next) {
      check_assertion_str(prev_sym->next != NULL,
                          "remove_anonymous_union...: symbol_not_found");
    }  /* for */
    prev_sym->next = sym_ptr->next;
  }  /* if */
  sym_ptr->next = NULL;
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
  if (cfront_2_1_mode) {
    /* If this is a type symbol that has previously been designated as the
       symbol receiving special transitional nested type name mangling, then
       reset that flag now that it has been promoted to another scope.  The
       flag will be restored, if appropriate, when this symbol is popped
       from the scope in which it will be reentered. */
    if (sym_ptr->header->has_cfront_transitional_nested_type_mangled_name) {
      if (is_type_symbol(sym_ptr)) {
        a_type_ptr	tp = type_symbol_type(sym_ptr);
        if (tp->use_cfront_transitional_nested_type_name_mangling) {
          tp->use_cfront_transitional_nested_type_name_mangling = FALSE;
          sym_ptr->header->has_cfront_transitional_nested_type_mangled_name
                                                                      = FALSE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
  db_exit();
}  /* remove_anonymous_union_member_from_inactive_symbols_list */


static a_boolean is_using_decl_to_same_type(a_symbol_ptr	sym1,
					    a_symbol_ptr	sym2)
/*
Returns TRUE if sym1 and sym2 refer to the same type after removing any
namespace projection symbols.
*/
{
  a_boolean	result = FALSE;

  if (sym1->kind == (a_symbol_kind)sk_namespace_projection ||
      sym2->kind == (a_symbol_kind)sk_namespace_projection) {
    sym1 = fundamental_symbol_of(sym1);
    sym2 = fundamental_symbol_of(sym2);
    if (is_type_symbol(sym1) && is_type_symbol(sym2)) {
      /* Two type symbols.  Note that if one of these symbols is in the process
         of being declared, its associated IL entry may still be NULL. */
      a_type_ptr  tp1 = type_symbol_type(sym1), tp2 = type_symbol_type(sym2);
      if (tp1 != NULL && tp2 != NULL && identical_types(tp1, tp2)) {
        result = TRUE;
      }  /* if */
    } else if (is_class_template_symbol(sym1) &&
               is_class_template_symbol(sym2)) {
      result = same_entities(sym1->variant.template_info->il_template_entry,
                             sym2->variant.template_info->il_template_entry);
    }  /* if */
  }  /* if */
  return result;
}  /* is_using_decl_to_same_type */

#if MICROSOFT_EXTENSIONS_ALLOWED

static a_boolean symbol_is_for_cli_accessor(a_symbol_ptr  sym)
/*
Return TRUE if the given symbol represents a C++/CLI accessor, or an overload
set of such accessors.
*/
{
  if (symbol_is(sym, sk_overloaded_function)) {
    sym = sym->variant.overloaded_function.symbols;
  }  /* if */
  return symbol_is(sym, sk_member_function) &&
         rout_is_cli_accessor(sym->variant.routine.ptr);
}  /* symbol_is_for_cli_accessor */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static
a_boolean symbols_may_coexist_in_curr_scope(a_symbol_ptr  old_sym,
                                            a_symbol_ptr  new_sym,
                                            a_symbol_ptr  *insert_sym,
                                            a_scope_depth scope_depth,
					    a_boolean	  suppress_error)
/*
old_sym is a symbol that is already in the symbol table.  new_sym is
a newly created symbol that is about to be added or a symbol for which
a projection symbol will be created and added.  Return TRUE if the
old and new symbols can coexist in the same scope.  For example, in C++
a tag symbol and a nontype symbol may coexist on the symbol list for a
scope.  The tag symbol should follow the other in the list, so if the
new symbol is a tag symbol, it must be inserted after the old.
insert_sym points provides the location at which the new symbol
should be entered.  This is used to make sure that a nontype symbol
will be found instead of a type symbol when both exist.  *insert_sym
is set only if insert_sym is not NULL.

In pcc mode and in cfront compatibility mode local variables of a
function are allowed to hide function parameters.  A warning is
issued for this case, except if suppress_error is TRUE.  In modes where
this is not allowed, an error will be issued by the caller.
*/
{
  a_boolean  err = TRUE;
  a_boolean  ignore_error = FALSE;

#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode && is_template_dependent_context()) {
    /* A redeclaration error is suppressed if the code is inside a dependent
       __if_exists.  We still go through the processing below because
      the insert_sym may be set. */
    if (scope_stack[decl_scope_level].pending_dependent_if_exists > 0) {
      suppress_error = TRUE;
      ignore_error = TRUE;
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (old_sym->kind == (a_symbol_kind)sk_undefined) {
    /* The old symbol was created for an undefined symbol that
       was referenced.  A new symbol can always coexist with
       an undefined one. */
    err = FALSE;
  } else if (lookup_module_for_symbol(old_sym) !=
             lookup_module_for_symbol(new_sym)) {
    /* The symbols are in different modules for the purposes of lookup. */
    err = FALSE;
  } else if ((cfront_2_1_mode || C_dialect == C_dialect_pcc ||
              (gcc_mode && gnu_version < 30400)) &&
             old_sym->kind == (a_symbol_kind)sk_variable &&
             old_sym->variant.variable.ptr->is_parameter &&
             (new_sym->kind != (a_symbol_kind)sk_variable ||
              (new_sym->variant.variable.ptr == NULL ||
               !new_sym->variant.variable.ptr->is_parameter))) {
    /* The old symbol is a parameter and the new symbol not a parameter --
       allowed in cfront, pcc, and some GNU C modes.  Note that we test the
       variable pointer for being NULL before dereferencing it above and we
       also pass the identifier string to the warning routine rather than
       using the standard symbol name fill-in.  This is done because the
       variable pointer may not have been filled in at the time the symbol
       is entered. */
    err = FALSE;
    if (!suppress_error) {
      pos_st_warning(ec_decl_hides_function_parameter, &new_sym->decl_position,
		     new_sym->header->identifier);
    }  /* if */
  } else if (symbol_is(old_sym, sk_variable) &&
             symbol_is(new_sym, sk_variable) &&
             old_sym->variant.variable.ptr->is_pack &&
             old_sym->variant.variable.ptr->compiler_generated &&
             old_sym->variant.variable.ptr->is_struct_binding) {
    /* An element of a structured binding pack is allowed for its corresponding
       pack. */
    err = FALSE;
  } else if ((symbol_is(old_sym, sk_variable) &&
              symbol_is(new_sym, sk_variable) &&
              (old_sym->variant.variable.ptr->is_parameter ||
               old_sym->variant.variable.ptr->is_struct_binding) &&
              old_sym->variant.variable.ptr->is_pack_element) ||
             (symbol_is(old_sym, sk_field) && symbol_is(new_sym, sk_field) &&
              old_sym->variant.field.ptr->is_captured_pack_element)) {
    /* Multiple elements of a pack (a function parameter pack, a structured
       binding pack, or a capture pack) are allowed. */
    err = FALSE;
    /* Keep the first element of the pack at the head of the list and set the
       is_invisible flag on the new symbol. */
    if (insert_sym != NULL) *insert_sym = old_sym;
    new_sym->is_invisible = TRUE;
#if GNU_EXTENSIONS_ALLOWED
  } else if (symbol_is(old_sym, sk_field) && symbol_is(new_sym, sk_field)) {
    /* Some modes ignore conflicts between fields if one of those fields comes
       from an anonymous union.  The first declaration prevails in such cases.
       We cannot use the insert_sym mechanism for this because these symbols
       are normally found on the inactive list (where ordering is ignored).
       Therefore, we set the is_invisible flag on the new symbol. */
    /* (Closure fields with duplicate names that result from capturing pack
       elements have already been handled above.) */
    if (gcc_mode || (microsoft_bugs && !C_mode())) {
      if (old_sym->variant.field.anonymous_parent_object != NULL ||
          suppress_error) {
        /* If the new symbol is an anonymous union field, the parent object
           will not have been recorded yet, but suppress_error will be TRUE. */
        if (!new_sym->is_invisible) {
          pos_sy_warning(ec_hidden_anonymous_union_field,
                         &new_sym->decl_position,
                         old_sym);
        }  /* if */
        err = FALSE;
        new_sym->is_invisible = TRUE;
        /* Keeping the first field at the head of the list results in nicer
           diagnostics if there are multiple conflicts. */
        if (insert_sym != NULL) *insert_sym = old_sym;
      }  /* if */
    }  /* if */
  } else if (gnu_mode && gnu_version < 40600 &&
             is_type_symbol(old_sym) && is_type_symbol(new_sym) &&
             is_pointer_type(type_symbol_type(old_sym)) &&
             is_pointer_type(type_symbol_type(new_sym)) &&
             is_function_type(type_pointed_to(type_symbol_type(old_sym))) &&
             is_function_type(type_pointed_to(type_symbol_type(new_sym))) &&
             seq_is_in_system_header(new_sym->decl_position.seq)) {
    /* In earlier versions of GNU, it appears that some redeclarations
       involving pointers to functions with different parameter types
       were silently allowed (but only in system headers).  This code
       is a little lenient in that it will issue a warning rather than
       an error in more cases than older versions of GNU (GNU issues a
       redeclaration error for some mismatched parameter types, but not
       all). */
    err = FALSE;
    if (!suppress_error) {
      pos_st_warning(ec_bad_type_name_redeclaration,
                     &new_sym->decl_position,
                     new_sym->header->identifier);
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  } else if (!C_mode()) {
    /* Some checks specific to C++ mode. */
    a_boolean	new_is_namespace = is_namespace_symbol(new_sym);
    a_boolean	old_is_namespace = is_namespace_symbol(old_sym);
    if ((new_is_namespace || old_is_namespace) &&
        !gnu_namespace_and_class_in_same_scope) {
      /* A namespace name must be unique in its scope. */
      /* err = TRUE; */
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (ms_version_is(>= 1300) &&
               (use_nonstandard_for_init_scope ||
                microsoft_type_dependent_for_init_scope) &&
               old_sym->declared_in_for_init &&
               symbol_is(new_sym, sk_variable)) {
      /* Microsoft Visual C++ 7.0 (and later) supports a nonstandard for-init
         declaration mode that makes the declared variable visible outside the
         for-statement, but it does not conflict with the declaration of other
         variables in that scope.  This handles the case where the for-init
         declaration comes first; decl_variable handles the other case. */
      if (!suppress_error && use_nonstandard_for_init_scope) {
        /* The nonstandard scope of a for-init variable has been preempted
           by a newer declaration: Issue a warning only in modes with pre-
           standard for-init scopes.  By default, the warning will not be
           issued when microsoft_version >= 1310. */
        a_diagnostic_ptr dp;
        dp = pos_start_diagnostic(es_warning, ec_declaration_hides_for_init,
                                  &new_sym->decl_position);
        add_diag_info_with_pos_insert(dp, ec_for_init_hidden_declaration,
                                      &old_sym->decl_position);
        end_diagnostic(dp);
      }  /* if */
      err = FALSE;
    } else if (microsoft_bugs &&
               scope_stack[depth_scope_stack].kind ==
                                        (a_scope_kind)sck_class_struct_union &&
               scope_stack[scope_depth].kind !=
                                        (a_scope_kind)sck_class_struct_union &&
               is_tag_symbol_kind(new_sym->kind) &&
               old_sym->kind == (a_symbol_kind)sk_type) {
      /* MSVC++ allows an elaborated-type-specifier used in a class scope to
         inject a type in the surrounding non-class scope and hide an
         existing typedef-name in that scope. */
#if CHECKING
      a_type_ptr  orig_tp = type_symbol_type(old_sym);
      a_type_ptr  tp = orig_tp;
      tp  = skip_typerefs(tp);
      check_assertion((is_immediate_class_type(tp) ||
                       is_immediate_enum_type(tp) ||
                       type_is_typedef(orig_tp)) &&
                      symbol_for(orig_tp)->header == old_sym->header);
#endif /* CHECKING */
      err = FALSE;
    } else if (microsoft_bugs && old_sym->is_invisible &&
               old_sym->kind == (a_symbol_kind)sk_type &&
               !is_tag_symbol_kind(new_sym->kind)) {
      /* MSVC++ effectively ignores a typedef of a named class to its own name
         (e.g., "typedef class C {} C;").  decl_typedef will have marked the
         associated symbol as "invisible". */
      err = FALSE;
    } else if (microsoft_mode && symbol_is(old_sym, sk_enum_tag) &&
               old_sym->variant.enumeration.extra_info->replaced_enum_symbol &&
               symbol_is(new_sym, sk_enum_tag)) {
      /* Microsoft compilers allow:
            enum E ee; // E considered complete with underlying type int.
            enum E: char { e };  // New type E (not compatible with previous E.
         The old symbol will be marked with the replaced_enum_symbol flag. */
      err = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else if (scope_stack[scope_depth].in_prototype_instantiation &&
               scope_stack[scope_depth].kind ==
                                 (a_scope_kind)sck_template_instantiation) {
      /* This must be a template friend declaration during prototype
         instantiation.  The symbol is injected into the template instantiation
         scope, and overloading is not performed at this point.  Let the two
         symbols coexist. */
      err = FALSE;
    } else if (is_injected_class_symbol(old_sym)) {
      /* The old symbol is an injected class-name.  It is hidden by the
         current declaration. */
      err = FALSE;
    } else if (is_using_decl_to_same_type(new_sym, old_sym)) {
      /* At least one of the symbols is a namespace projection that points
         to the same type (or class template) as the other symbol.  Enter the
         new symbol.  The insert point is not changed, so the newly entered
         symbol will be used. */
      err = FALSE;
    } else if (gpp_mode &&
               old_sym->kind == (a_symbol_kind)sk_variable &&
               old_sym->variant.variable.ptr->is_handler_param) {
      /* A declaration can hide a handler parameter in g++ mode.  Note that
         the kind of the new symbol is not important. */
      err = FALSE;
      if (!suppress_error) {
        pos_st_warning(ec_decl_hides_catch_parameter,
                       &new_sym->decl_position,
                       new_sym->header->identifier);
      }  /* if */
    } else {
      a_symbol_ptr fund_new_sym = fundamental_symbol_of(new_sym);
      a_symbol_ptr fund_old_sym = fundamental_symbol_of(old_sym);
      a_boolean    new_is_using_decl;

      new_is_using_decl = symbol_is(new_sym, sk_namespace_projection) &&
                          new_sym->variant.namespace_projection.is_using_decl;
      if (strict_ansi_mode &&
          (is_template_symbol(fund_new_sym) ||
           is_template_symbol(fund_old_sym) ||
           (fund_old_sym->kind == (a_symbol_kind)sk_overloaded_function &&
            overload_set_contains_template(fund_old_sym)))) {
        /* In strict mode a template name must be unique in its scope.  It
           can be part of an overload set, but otherwise there can be no
           declaration of the same name. */
        /* err = TRUE; */
      } else if ((symbol_is(fund_new_sym, sk_concept_template) ||
                  symbol_is(fund_old_sym, sk_concept_template)) &&
                 !(gpp_mode && !clang_mode)) {
        /* Concept templates must be unique in their scope, but GCC allows
           them to coexist with tag types. */
        /* err = TRUE; */
      } else if (is_tag_symbol(fund_new_sym) &&
                 !is_type_symbol(fund_old_sym) &&
                 !is_class_template_symbol(fund_old_sym)) {
        /* The new symbol is a tag symbol and the old symbol is a non-type
           name.  Be sure the new symbol inserted into the list after the
           old one.  old_is_namespace can only be TRUE in g++ mode, where
           a namespace and class can be declared with the same name.  In
           that case, the class should go on the front of the list because
           it should be found instead of the namespace by normal lookups. */
        err = FALSE;
        if (!old_is_namespace) {
          if (insert_sym != NULL) *insert_sym = old_sym;
        }  /* if */
      } else if (is_tag_symbol(fund_old_sym) &&
                 !is_type_symbol(fund_new_sym) &&
                 !is_class_template_symbol(fund_new_sym)) {
        /* The old symbol is a tag symbol and the new one is not a type
           symbol or a class template name.  It will be placed at the front
           of the list automatically.  new_is_namespace can only be TRUE in
           g++ mode, where a namespace and class can be declared with the
           same name.  In that case, the namespace should go on the list after
           the class because the class should be found instead of the
           namespace by normal lookups. */
        err = FALSE;
        if (new_is_namespace) {
          if (insert_sym != NULL) *insert_sym = old_sym;
        }  /* if */
      } else if ((gpp_mode || microsoft_mode) && new_is_using_decl &&
                 is_tag_symbol(fund_new_sym) &&
                 symbol_is(fund_old_sym, sk_type)) {
        /* The new symbol is a using-declaration to a tag symbol and the
           old symbol is a typedef.  This is allowed in g++, clang, and
           Microsoft modes.  g++ and clang find the typedef, while
           Microsoft finds the using-declaration. */
        err = FALSE;
        if (gpp_mode) {
          if (insert_sym != NULL) *insert_sym = old_sym;
        }  /* if */
      } else if (!strict_ansi_mode &&
                 old_sym->kind == (a_symbol_kind)sk_projection &&
                 !old_sym->variant.projection.is_using_decl &&
                 new_sym->kind != (a_symbol_kind)sk_projection) {
        /* A previously created projection symbol can be hidden by a new
           declaration.  In strict mode this is an error because it
           violates the rule that a name must mean the same thing when
           considered in the complete class definition.  Remove the old
           symbol so that it does not need to be handled by the lookup
           routines. */
        remove_symbol(old_sym);
        err = FALSE;
      } else if (scope_stack[scope_depth].in_prototype_instantiation &&
                 scope_stack[scope_depth].kind ==
                                 (a_scope_kind)sck_class_struct_union) {
        if (is_class_member_using_decl_symbol(old_sym)) {
          if (is_nontype_template_param_symbol(fund_old_sym) &&
              (is_function_or_template_symbol(fund_new_sym) ||
               is_nontype_template_param_symbol(fund_new_sym))) {
            /* During a prototype instantiation a nontype using-declaration
               (which *could* represent a function) is followed by function
               declaration or another nontype using-declaration.  Assume
               these do not conflict.  No insert point needs to be set since
               the default places the function declaration (new_sym) in front
               of the using-decl (old_sym) in the active list. */
            err = FALSE;
            new_sym->potentially_overloaded = TRUE;
          }  /* if */
        } else if (is_class_member_using_decl_symbol(new_sym)) {
          if (is_nontype_template_param_symbol(fund_new_sym) &&
              is_function_or_template_symbol(old_sym)) {
            /* The opposite case: during a prototype instantiation a function
               declaration (or several -- old_sym could be an overload set)
               is followed by a nontype using-declaration (which *could*
               represent another function).  Assume these do not conflict.
               Set the insert_sym so that the using-decl (new_sym) will not
               hide the function declaration (old-sym); this is especially
               important for building overload sets. */
            err = FALSE;
            old_sym->potentially_overloaded = TRUE;
            if (insert_sym != NULL) *insert_sym = old_sym;
          }  /* if */
        }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (cli_or_cx_enabled &&
                 (is_cli_generic_class_symbol(fund_new_sym) &&
                  is_class_struct_union_symbol(fund_old_sym)) &&
                 (generic_arity_overload_allowed ||
                  in_code_generated_from_metadata())) {
        /* The old symbol is a class type and the new symbol is a C++/CLI
           generic.  These can exist in the same scope.  The generic should
           hide the non-generic.  This is only allowed when importing code
           from metadata or when the generic_arity_overload_allowed flag
           is TRUE. */
        err = FALSE;
        /* Record the non-generic symbol in the information about the
           generic. */
        non_generic_class_for_cli_generic(fund_new_sym) = fund_old_sym;
      } else if (cli_or_cx_enabled &&
                 (is_cli_generic_class_symbol(fund_old_sym) &&
                  is_class_struct_union_symbol(fund_new_sym)) &&
                 (generic_arity_overload_allowed ||
                  in_code_generated_from_metadata())) {
        /* The new symbol is a class type and the old symbol is a C++/CLI
           generic.  These can exist in the same scope.  The generic should
           hide the non-generic.  This is only allowed when importing code
           from metadata or when the generic_arity_overload_allowed flag
           is TRUE. */
        err = FALSE;
        if (insert_sym != NULL) *insert_sym = old_sym;
        /* Record the non-generic symbol in the information about the
           generic. */
        non_generic_class_for_cli_generic(fund_old_sym) = fund_new_sym;
      } else if (cli_or_cx_enabled && old_sym->is_invisible &&
                 symbol_is_for_cli_accessor(old_sym)) {
        /* Property and event accessors don't conflict with members that
           happen to have the same name. */
        err = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      }  /* if */
    }  /* if */
  }  /* if */
  return !err || ignore_error;
}  /* symbols_may_coexist_in_curr_scope */


static a_boolean is_redeclared_in_handler(a_symbol_header_ptr	sym_hdr)
/*
Return TRUE if a variable of the name specified by sym_hdr is a parameter
of the nearest enclosing function.
*/
{
  a_boolean	result = FALSE;

  /* Look on the symbols list of the enclosing function, if any. */
  if (depth_innermost_function_scope != NO_SCOPE_DEPTH) {
    a_scope_stack_entry_ptr	ssep;
    a_symbol_ptr		sym;
    ssep = &scope_stack[depth_innermost_function_scope];
    for (sym = assoc_pointers_block_of(ssep)->symbols;
         sym != NULL; sym = sym->next_in_scope) {
      if (sym->header == sym_hdr && sym->kind == (a_symbol_kind)sk_variable) {
        /* If this is a variable, see if it is a parameter.  If so, we have
           redeclared a parameter. */
        a_variable_ptr	vp = sym->variant.variable.ptr;
        if (vp->is_parameter) {
          result = TRUE;
          break;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  return result;
}  /* is_redeclared_in_handler */


static a_boolean is_redeclared_template_param(a_symbol_ptr      sym,
                                              an_error_severity *severity)
/*
Look through the template parameters associated with the innermost
instantiation scope for a symbol whose header matches the header of sym.
Return TRUE if a match is found.  Return in *severity the error
severity to be used for the diagnostic when TRUE is returned.
*/
{
  a_template_param_ptr		tpp;
  a_boolean			result = FALSE;
  a_scope_stack_entry_ptr	ssep;
  a_scope_depth			starting_depth;

  /* If the current scope is the one in which the template parameters are
     considered to be declared, then a redeclaration is an error.  Otherwise,
     the severity depends on the mode. */
  *severity = scope_stack[depth_scope_stack].template_param_decl_scope
                                      ? es_error : strict_ansi_error_severity;
  /* Start searching at the innermost instantiation scope or the
     innermost template declaration scope, which ever is at a greater
     depth. */
  starting_depth = depth_innermost_instantiation_scope;
  if (starting_depth < depth_template_declaration_scope) {
    /* The innermost template declaration scope is at a greater depth
       than the innermost instantiation scope. Start at the innermost template
       declaration scope.  For redeclarations in template declaration
       scopes, always issue an error at the innermost template declaration
       scope. */
    starting_depth = depth_template_declaration_scope;
    *severity = es_error;
  }  /* if */
  if (clang_mode) {
    if (!scope_stack[depth_scope_stack].template_param_decl_scope) {
      /* Clang appears to silently accept something like: 
            template<typename T> T T(T) {}
         We emulate that but issue a warning. */
      *severity = es_warning;
    }  /* if */
  } else if (gpp_mode && symbol_is(sym, sk_parameter)) {
    /* In g++ mode (but not Clang mode), reduce the severity to a warning for
       a redeclaration that is a parameter name. */
    *severity = es_warning;
  }  /* if */
  for (ssep = scope_stack_entry_for(starting_depth);
       ssep != NULL; ssep = previous_scope_of(ssep)) {
    /* Only look at template instantiation and declaration scopes. */
    if (ssep->kind != (a_scope_kind)sck_template_instantiation &&
        ssep->kind != (a_scope_kind)sck_template_declaration) continue;
    tpp = ssep->template_decl_info->parameters;
    while (tpp != NULL && !result) {
      a_symbol_ptr  param_symbol = tpp->param_symbol;
      if (param_symbol->header == sym->header) {
        result = TRUE;
      }  /* if */
      tpp = tpp->next;
    }  /* while */
    /* The parameter was declared in this scope.  Exit the loop. */
    if (result) break;
    /* Only redeclarations of the innermost instantiation scope are
       always errors. */
    *severity = strict_ansi_error_severity;
  }  /* for */
  return result;
}  /* is_redeclared_template_param */


static a_boolean is_redeclared_for_init_decl_name(a_symbol_header_ptr  hdr,
                                                  a_scope_depth  scope_depth)
/*
Return TRUE if scope_depth specifies a scope that, for purposes of name
lookup, is the same scope as an enclosing for-init scope and if hdr matches
the symbol header of a symbol declared in the for-init scope.
*/
{
  a_scope_stack_entry_ptr  curr_ssep, ssep;
  a_boolean                match = FALSE;
  a_symbol_ptr             sym;

  if (use_nonstandard_for_init_scope) {
    /* No need to check, since a for-init scope will not have been created. */
  } else if (gpp_version_is(<40700)) {
    /* In early GNU C++ modes, the for-init scope is considered to be distinct
       from the scope containing the condition and the body of the loop. */
  } else {
    /* Three scopes are treated as "the same" for lookup purposes (based on
       language in WP 6.5.3 [stmt.for] and in 6.4 [stmt.select] para 2-3). */
    curr_ssep = &scope_stack[scope_depth];
    ssep = NULL;
    if (curr_ssep->kind == (a_scope_kind)sck_block) {
      if (curr_ssep->is_loop_scope) {
        /* The current scope is a block scope for a loop (possibly a
           for-loop); check whether the immediately surrounding scope is a
           for-init scope, a case like this:
             for (int i = 0; ; ) { int i; }              // Error
           or else a condition scope that is surrounded by a for-init scope,
           a case like this:
             for (int i = 0; int j = 1; ) { int i; }     // Error
        */
        ssep = curr_ssep - 1;
        if ((ssep-1)->kind == (a_scope_kind)sck_condition) {
          ssep = ssep - 1;
        }  /* if */
      }  /* if */
    } else if (curr_ssep->kind == (a_scope_kind)sck_condition) {
      /* The current scope is a condition scope; see if the immediately
         surrounding scope is a for-init scope -- a case like this:
           for (int i = 0; int i = 1; )                 // Error
      */
      ssep = curr_ssep - 1;
    }  /* if */
    if (ssep != NULL && ssep->is_for_init_block &&
        !ssep->is_dissociated_from_loop_scope) {
      /* ssep is a for-init scope -- check for a name mismatch. */
      for (sym = (assoc_pointers_block_of(ssep))->symbols;
           sym != NULL;
           sym = sym->next_in_scope) {
        if (sym->header == hdr) {
          match = TRUE;
          break;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  return match;
}  /* is_redeclared_for_init_decl_name */


static a_boolean is_redeclaration_of_enhanced_for_iterator(
                                             a_symbol_header_ptr  hdr,
                                             a_scope_depth        scope_depth,
                                             a_boolean            is_tag_sym)
/*
Return TRUE if scope_depth specifies the outer "body" of a range-based "for"
loop or a Microsoft "for each" loop that declares an iterator variable
matching hdr.  In non-strict modes, tag/non-tag symbols are ignored depending
on whether is_tag_sym is FALSE.
*/
{
  a_boolean  match = FALSE;

  if (scope_stack[scope_depth].is_loop_scope) {
    a_symbol_ptr  sym = hdr->symbol;
    if (sym != NULL && symbol_is(sym, sk_variable) &&
        sym->variant.variable.ptr->is_enhanced_for_iterator &&
        sym->decl_scope ==
                       previous_scope_of(&scope_stack[scope_depth])->number &&
        (is_tag_symbol_kind(sym->kind) == is_tag_sym || strict_ansi_mode)) {
      match = TRUE;
    }  /* if */
  }  /* if */
  return match;
}  /* is_redeclaration_of_enhanced_for_iterator */


static a_boolean is_redeclared_condition_decl_name(
                                             a_symbol_header_ptr  hdr,
                                             a_scope_depth        scope_depth,
                                             a_boolean            is_tag_sym)
/*
Return TRUE if scope_depth specifies a scope immediately enclosed by a
condition scope and hdr matches the symbol header kind of a symbol declared
in the condition scope.  In non-strict modes, tag/non-tag symbols are ignored
depending on whether is_tag_sym is FALSE.
*/
{
  a_scope_stack_entry_ptr  ssep = &scope_stack[scope_depth];
  a_boolean                match = FALSE;
  a_symbol_ptr             sym;

  if (ssep->kind == (a_scope_kind)sck_block &&
      (ssep-1)->kind == (a_scope_kind)sck_condition &&
      !(ssep-1)->is_dissociated_from_loop_scope) {
    for (sym = (assoc_pointers_block_of(ssep-1))->symbols;
         sym != NULL;
         sym = sym->next_in_scope) {
      if (sym->header == hdr &&
          (is_tag_symbol_kind(sym->kind) == is_tag_sym ||
           strict_ansi_mode)) {
        match = TRUE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return match;
}  /* is_redeclared_condition_decl_name */


void add_symbol_to_inactive_list(a_symbol_ptr sym_ptr)
/*
Add the given symbol to its symbol header's inactive list.
*/
{
  a_symbol_header_ptr sym_hdr = sym_ptr->header;

#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 4) {
    db_symbol(sym_ptr, "add_symbol_to_inactive_list: ", 2);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  check_assertion_str(sym_ptr->kind != (a_symbol_kind)sk_extern_variable &&
                      sym_ptr->kind != (a_symbol_kind)sk_extern_routine,
                      "add_symbol_to_inactive_list: bad symbol kind");
  sym_ptr->next = sym_hdr->inactive_symbols;
  sym_hdr->inactive_symbols = sym_ptr;
}  /* add_symbol_to_inactive_list */


static a_symbol_ptr check_for_hidden_declaration(a_symbol_ptr	new_sym)
/*
We are entering a symbol for a local variable or parameter (new_sym).
Look up the symbol ignoring the current scope.  If the lookup finds a field,
variable, or parameter of the same name, return the hidden symbol.
*/
{
  a_symbol_ptr		sym;
  a_symbol_ptr		hidden_sym = NULL;
  a_symbol_locator	locator;

  make_locator_for_symbol(new_sym, &locator);
  clear_specific_symbol(locator);
  sym = normal_id_lookup(&locator, IDL_SKIP_CURR_SCOPE |
                                   IDL_IS_LOOKUP_TO_CHECK_FOR_NAME_HIDING);
  /* Check for the hiding of a field, variable, or static data member.
     We don't considering hiding to have occurred if both symbols are from
     a namespace scope, which can occur with block extern declarations. */
  if (sym != NULL &&
      (sym->kind == (a_symbol_kind)sk_field ||
       sym->kind == (a_symbol_kind)sk_variable ||
       sym->kind == (a_symbol_kind)sk_static_data_member)) {
    a_boolean		new_is_from_namespace = FALSE;
    a_boolean		hidden_is_from_namespace = FALSE;
    if (new_sym->variant.variable.ptr != NULL) {
      a_scope_ptr	sp;
      sp = new_sym->variant.variable.ptr->source_corresp.parent_scope;
      if (sp != NULL &&
          (sp->kind == (a_scope_kind)sck_file ||
           sp->kind == (a_scope_kind)sck_namespace)) {
        new_is_from_namespace = TRUE;
      }  /* if */
    }  /* if */
    if (sym->kind == (a_symbol_kind)sk_variable &&
        sym->variant.variable.ptr != NULL) {
      a_scope_ptr	sp;
      sp = sym->variant.variable.ptr->source_corresp.parent_scope;
      if (sp != NULL &&
          (sp->kind == (a_scope_kind)sck_file ||
           sp->kind == (a_scope_kind)sck_namespace)) {
        hidden_is_from_namespace = TRUE;
      }  /* if */
    }  /* if */
    if (!new_is_from_namespace || !hidden_is_from_namespace) {
      hidden_sym = locator.specific_symbol;
      /* Only use a projection symbol if it refers to a using-declaration.
         Otherwise, use the fundamental symbol.  Synthesized namespace
         projection symbols result from using-directive lookups, so those
         should not be used in diagnostics to name the hidden symbol. */
      if (hidden_sym->synthesized_namespace_projection ||
          (hidden_sym->kind == (a_symbol_kind)sk_projection &&
           !hidden_sym->variant.projection.is_using_decl)) {
        hidden_sym = sym;
      }  /* if */
    }  /* if */
  }  /* if */
  return hidden_sym;
}  /* check_for_hidden_declaration */


static void link_symbol_into_symbol_table(a_symbol_ptr  sym_ptr,
                                          a_scope_depth scope_depth,
                                          a_boolean     suppress_error)
/*
Add the given symbol to the symbol table, i.e., link it into its header's
list.  Also check to see if there is a previous definition of the symbol
in the same scope, and issue an error in that case.  The error is suppressed
if suppress_error is TRUE.  scope_depth indicates the level in the scope
stack at which the symbol is being entered, which is needed to determine
the proper insert location.

Symbols are usually added to the symbol header's active list.  When
a symbol is added to a sck_namespace_extension scope, however, the
symbol must be added to the inactive list.
*/
{
  a_symbol_ptr        old_sym_ptr, hidden_sym = NULL;
  a_scope_number      scope_number;
  a_name_space_kind   sym_name_space_kind;
  a_symbol_header_ptr hdr_ptr = sym_ptr->header;
  a_symbol_ptr        insert_after;
  a_scope_depth       curr_depth;
  a_boolean           redecl_err = FALSE;

  if (sym_ptr->is_error) {
    /* Error symbols are never added to the symbol table. */
  } else {
    a_boolean	add_sym_to_inactive_list = FALSE;
#if CHECKING
    if (hdr_ptr == NULL || hdr_ptr == error_symbol_header) {
      internal_error("link_symbol_into_symbol_table: NULL or error header");
    }  /* if */
    check_assertion_str(sym_ptr->kind != (a_symbol_kind)sk_extern_variable &&
                        sym_ptr->kind != (a_symbol_kind)sk_extern_routine,
                        "link_symbol_into_symbol_table: bad symbol kind");
#endif /* CHECKING */
    insert_after = NULL;
    if (scope_depth == NO_SCOPE_DEPTH) {
      if (sym_ptr->is_class_member) {
        /* A symbol is being added to a completed class. */
        add_sym_to_inactive_list = TRUE;
      } else {
        /* The symbol is being entered outside of any scope; this happens for
           keywords and command-line -D options, for example.  No error check
           is done. */
        add_sym_to_inactive_list = file_scope_symbols_are_on_inactive_list;
      }  /* if */
    } else {
      check_assertion_str2(scope_stack[scope_depth].kind !=
                                                   (a_scope_kind)sck_pragma,
                           "link_symbol_into_symbol_table:",
                           "attempting to add symbol to pragma scope");
      if (scope_stack[scope_depth].kind ==
                                      (a_scope_kind)sck_namespace_extension ||
          scope_stack[scope_depth].kind == (a_scope_kind)sck_file) {
        /* Once the initial namespace definition has been closed, additional
           symbols for the namespace are added to the inactive list.  The
           flag in the assoc_pointers_block needs to be tested because it
           is possible for namespace extension scopes to be pushed while the
           initial namespace definition is still in progress.  This is also
           true of the file scope.  File scope symbols are on the inactive list
           once the file scope has been popped for the first time. */
        a_scope_pointers_block_ptr	spbp;
        spbp = assoc_pointers_block_of(&scope_stack[scope_depth]);
        add_sym_to_inactive_list = spbp->add_symbols_to_inactive_list;
      }  /* if */
      if (add_sym_to_inactive_list) {
        /* Symbols entered into namespace extension scopes are added
           directly to the inactive list.  The sequence of symbols on the
           inactive list is not significant. */
        old_sym_ptr = hdr_ptr->inactive_symbols;
        scope_number = scope_stack[scope_depth].number;
      } else {
        old_sym_ptr = hdr_ptr->symbol;
        /* If the symbol is not being entered in the innermost scope, skip
           past any symbols on the active list from the scopes inside the
           entry scope.  That's necessary so that the new symbol can be added
           at the right place in the active list, which is ordered from
           innermost to outermost scope. */
        for (curr_depth = depth_scope_stack; ; curr_depth--) {
          scope_number = scope_stack[curr_depth].number;
          if (curr_depth == scope_depth) break;
          /* Ignore any symbols from the scope skipped over. */
          while (old_sym_ptr != NULL &&
                 old_sym_ptr->decl_scope == scope_number) {
            insert_after = old_sym_ptr;
            old_sym_ptr = old_sym_ptr->next;
          }  /* while */
        }  /* for */
      }  /* if */
      /* Check for a local variable hiding another variable, field, or
         static data member in an enclosing scope (a remark will be issued
         later, unless a more serious declaration error is encountered).
         This check is suppressed if we are not entering the symbol in the
         current scope.  If the symbol has already been set to point to
         a variable, don't issue a diagnostic if the variable is from the
         file scope because it is actually a linked declaration. */
      if (scope_depth == depth_scope_stack &&
          sym_ptr->kind == (a_symbol_kind)sk_variable &&
          depth_innermost_function_scope != NO_SCOPE_DEPTH) {
        hidden_sym = check_for_hidden_declaration(sym_ptr);
      }  /* if */
      sym_name_space_kind = name_space_for_symbol_kind[(int)sym_ptr->kind];
      if (!C_mode()) {
        /* See if this is a redeclaration of a for-init or condition variable
           name. */
        if (depth_innermost_function_scope != NO_SCOPE_DEPTH) {
          a_boolean  is_tag = is_tag_symbol_kind(sym_ptr->kind);
          if (is_redeclared_condition_decl_name(
                                              hdr_ptr, scope_depth, is_tag)) {
            /* The name of the variable declared in a condition may not be
               redeclared in the topmost scope of if, switch, while, or for
               statement. */
            if (!suppress_error) {
              pos_st_error(ec_redeclaration_of_condition_decl_name,
                           &(sym_ptr->decl_position),
                           sym_ptr->header->identifier);
            }  /* if */
            redecl_err = TRUE;
          } else if (is_redeclared_for_init_decl_name(hdr_ptr, scope_depth)) {
            /* The name of the variable declared in a for-init may not be
               redeclared in the condition or the topmost scope of the for
               statement. */
            if (!suppress_error) {
              pos_st_error(ec_redeclaration_of_for_init_decl_name,
                           &(sym_ptr->decl_position),
                           sym_ptr->header->identifier);
            }  /* if */
            redecl_err = TRUE;
          } else if (is_redeclaration_of_enhanced_for_iterator(
                                              hdr_ptr, scope_depth, is_tag)) {
            /* The name of the iterator variable in a range-based for statement
               cannot be declared in the outermost loop scope.  E.g.:
                 for (int N: vec) { double N = 1.0; }  // Error
            */
            if (!suppress_error) {
              pos_st_diagnostic(microsoft_mode ?
                                          es_warning : es_discretionary_error,
                                ec_redeclaration_of_range_iterator,
                                &(sym_ptr->decl_position),
                                sym_ptr->header->identifier);
            }  /* if */
          }  /* if */
          if (!suppress_error &&
              scope_stack[depth_scope_stack].is_catch_in_function_try &&
              is_redeclared_in_handler(sym_ptr->header)) {
            /* A variable declared in a catch parameter or the top block
               of a catch clause is also the name of a function parameter. */
            pos_st_diagnostic(strict_ansi_discretionary_severity,
                              ec_handler_redeclares_parameter,
                              &(sym_ptr->decl_position),
                              sym_ptr->header->identifier);
          }  /* if */
        }  /* if */
        /* See if this name is a redeclaration of a template parameter name.
           Redeclarations are permitted in Microsoft mode.  Injected class
           names are ignored because the test will have already been done
           on the declaration of the class in the enclosing scope. */
        if (!redecl_err &&
            (is_nonspecialized_instantiation_context() ||
             depth_template_declaration_scope != NO_SCOPE_DEPTH) &&
            sym_name_space_kind == nsk_other &&
            sym_ptr->kind != (a_symbol_kind)sk_undefined) {
          an_error_severity severity;
          if (!suppress_error &&
              !ms_extensions &&
              !is_injected_class_symbol(sym_ptr) &&
              /* Don't diagnose a redeclaration or hiding of a template
                 parameter when rescanning a dependent template template
                 parameter: enclosing parameters declared after the
                 template template parameter are already visible during
                 the rescan, so the diagnostic would be an artifact of
                 the rescan. */
              !scope_stack[depth_scope_stack].is_template_param_rescan &&
              is_redeclared_template_param(sym_ptr, &severity)) {
            if (severity == es_error) {
              /* A template parameter name has been reused in the first scope
                 associated with the instantiation that affects the
                 declarative level (or in a scope nested within that scope,
                 in strict mode). Note that we pass the identifier string to
                 the error routine rather than using the standard symbol name
                 fill-in.  This is done because the variable pointer may not
                 have been filled in at the time the symbol is entered. */
              pos_st_error(ec_redeclaration_of_template_param_name,
                           &(sym_ptr->decl_position),
                           sym_ptr->header->identifier);
              redecl_err = TRUE;
            } else {
              /* A template parameter name has been reused in an inner scope
                 of a template class or function.  Issue a warning that the
                 template parameter will be hidden. */
              pos_st_warning(ec_decl_hides_template_parameter,
                             &(sym_ptr->decl_position),
                             sym_ptr->header->identifier);
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
      if (!redecl_err) {
        a_boolean	set_insert_after = TRUE;
        /* See if there is already a definition of this identifier in the same
           scope and name space.  (For name spaces, see C standard, 3.1.2.3.)
           Because of the code above and because the active list is ordered,
           if there are any symbols in the same scope they will be at the
           front of the list.  Note that this test must be done even if
           suppress_error is TRUE, because tag names must still be entered
           behind existing non-tag names in C++.  suppress_error is only TRUE
           when there has already been an error issued, so in practical terms
           the extra check costs nothing.  If the symbol is being entered
           into a namespace extension scope, then the list being scanned is
           the inactive list (and not the active list).  The inactive list
           is not ordered, so we have to scan the entire list looking for
           symbols from the appropriate scope. */
        for (; old_sym_ptr != NULL &&
               (old_sym_ptr->decl_scope == scope_number ||
                add_sym_to_inactive_list);
             old_sym_ptr = old_sym_ptr->next) {
          /* If this is a symbol from another scope (which can only occur
             when adding to a namespace extension scope) skip this symbol. */
          if (old_sym_ptr->decl_scope != scope_number) continue;
          if (name_space_for_symbol_kind[(int)old_sym_ptr->kind] ==
                                                         sym_name_space_kind) {
            /* Two declarations in the same name space in the same scope:
               in most cases, this is an error, but in C++, one is allowed to
               define a tag name and a non-type name in the same scope (see ARM
               3.2, 3.1c, and 7.1.3).  In cfront and pcc modes a variable is
               allowed to hide a function parameter.  MSVC++ allows an
               elaborated-type-specifier (if declared in a class scope) to
               hide a typedef in the containing scope. */
            if (!symbols_may_coexist_in_curr_scope
                            (old_sym_ptr, sym_ptr,
                             set_insert_after ? &insert_after :
                                                (a_symbol_ptr*)NULL,
                             scope_depth, suppress_error)) {
              /* Error, this identifier has already been declared. */
              if (!suppress_error) {
                if (is_type_symbol(sym_ptr) && is_type_symbol(old_sym_ptr)) {
                  /* Typedef names can sometimes be redeclared, as long as the
                     underlying type is the same.  That's not the case here. */
                  a_type *tp = il_entry_for_symbol<a_type>(old_sym_ptr);

                  if (!is_incomplete_type(tp) &&
                      is_class_struct_union_type(tp)) {
                    issue_redef_diag(&(sym_ptr->decl_position), old_sym_ptr);
                  } else {
                    pos_sy_error(ec_bad_type_name_redeclaration,
                                 &(sym_ptr->decl_position),
                                 old_sym_ptr);
                  }  /* if */
                } else {
                  /* Note that we pass the identifier string to the error
                     routine rather than using the standard symbol name
                     fill-in. This is done because the variable pointer
                     may not have been filled in at the time the symbol is
                     entered. */
                  pos_st_error(ec_id_already_declared,
                               &(sym_ptr->decl_position),
                               sym_ptr->header->identifier);
                }  /* if */
                redecl_err = TRUE;
              }  /* if */
              /* Only break out of the loop if an error occurred.  Otherwise
                 check with other symbols to make sure that this symbol
                 can coexist with an already hidden symbol. */
              break;
            }  /* if */
            /* Go ahead and enter the symbol anyway.  Both symbols will be
               in the symbol table.  The insert position should only be set
               by the first symbols_may_coexist_in_curr_scope call.
               Subsequent calls are only for error detection purposes and
               should not affect the position of the new symbol in the symbol
               table. */
            set_insert_after = FALSE;
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
    if (hidden_sym != NULL && !redecl_err) {
      /* hidden_sym represents a variable, field, or static data member
         hidden by a local variable or parameter declaration.  Issue a remark.
         The remark is delayed until we know that the declaration was not
         the cause of an error (in which case the remark would be moot).
         This diagnostic was originally only issued for the case where
         one local variable hides another.  Continue to use the original
         error code for such diagnostics. */
      an_error_code	error_code;
      error_code = hidden_sym->kind == (a_symbol_kind)sk_variable &&
                   hidden_sym->variant.variable.ptr
                                       ->source_corresp.is_local_to_function
                          ? ec_local_variable_hidden
                          : ec_variable_hides_entity;
      pos_sy_remark(error_code, &sym_ptr->decl_position, hidden_sym);
    }  /* if */
    if (add_sym_to_inactive_list) {
      /* In namespace extension scopes, just add the symbol to the
         inactive list.  The sequence is not significant. */
      add_symbol_to_inactive_list(sym_ptr);
    } else if (insert_after == NULL) {
      /* Link the symbol onto the front of the list in the symbol header.
         This is the normal case. */
      sym_ptr->next = hdr_ptr->symbol;
      hdr_ptr->symbol = sym_ptr;
    } else {
      /* Link the symbol behind insert_after. */
      sym_ptr->next = insert_after->next;
      insert_after->next = sym_ptr;
    }  /* if */
  }  /* if */
}  /* link_symbol_into_symbol_table */


static a_boolean member_name_conflicts_with_class_name(a_type_ptr   class_type,
                                                       a_symbol_ptr member_sym)
/*
If the member specified by member_sym has the same name as the class of
which it is a member (class_type), issue an error.  An exception is made
for nonstatic data members in a class with no constructors (ARM 9.2).
Constructors of named classes are another special case, but since they
are not actually entered into the symbol table, this routine is not called
for them; however, implicitly declared constructors of unnamed classes are
checked for and ignored. Finally, injected class names are also allowed.
*/
{
  a_symbol_ptr class_sym = (a_symbol_ptr)class_type->source_corresp.assoc_info;
  a_boolean    err = FALSE;
  a_field_ptr  fp;

  if (class_sym->header == member_sym->header) {
    /* Member has the same name as the class to which it belongs. */
    /* If no constructor already exists, a field is allowed to have the
       same name as its class, as long as it's not an anonymous union field
       being promoted to a containing class with the same name. */
    if (member_sym->kind == (a_symbol_kind)sk_field &&
        class_sym->variant.
                    class_struct_union.extra_info->constructor == NULL &&
        ((fp = member_sym->variant.field.ptr) == NULL ||
         (member_sym->is_class_member &&
          same_entities(sym_parent_class(member_sym), parent_class_of(fp))))) {
        /* Note: the last checks serve to exclude anonymous union promotions.
           It is never the case that the field is not yet bound to the symbol
           when an anonymous union member is being promoted, nor will the
           parent classes correspond (during promotion, the symbol's
           is_class_member flag may temporarily be FALSE, however). */
    } else if (class_sym->header == unnamed_tag_symbol_header) {
      /* This must be a constructor for an unnamed class. */
    } else if (is_injected_class_symbol(member_sym)) {
      /* Okay. */
    } else if (symbol_is(member_sym, sk_projection) &&
               !member_sym->variant.projection.is_using_decl) {
      /* A generated projection symbol -- okay. */
    } else {
      /* Error: an identifier that is not a constructor and that has the
         same name as a class is being declared within the class. */
      pos_error(is_function_symbol(member_sym) ?
                     ec_class_and_member_function_name_conflict :
                     ec_class_and_member_name_conflict,
                &member_sym->decl_position);
      err = TRUE;
      member_sym->is_error = TRUE;
    }  /* if */
  }  /* if */
  return err;
}  /* member_name_conflicts_with_class_name */


a_hash_value hash_token_sequence_xref(a_void_ptr	key)
/*
Produce a hash value for a token sequence number.  The key is a pointer to a
token sequence xref entry.
*/
{
  a_hash_value			value;
  a_token_sequence_xref_ptr	tsxp;

  tsxp = (a_token_sequence_xref_ptr)key;
  /* Just use the token sequence number as the hash value. */
  value = (a_hash_value)tsxp->token_sequence_number;
  return value;
}  /* hash_token_sequence_xref */


a_boolean compare_token_sequence_xref(a_void_ptr	entry,
                                      a_void_ptr	key)
/*
Compare an entry in the symbol header lookup hash table with an entry to be
found.  "entry" and "key" are of type a_token_sequence_xref_ptr.
Return TRUE if the key matches the entry.
*/
{
  a_token_sequence_xref_ptr	entry_tsxp;
  a_token_sequence_xref_ptr	key_tsxp;
  a_boolean			result;

  entry_tsxp = (a_token_sequence_xref_ptr)entry;
  key_tsxp = (a_token_sequence_xref_ptr)key;
  result = entry_tsxp->token_sequence_number ==
                                               key_tsxp->token_sequence_number;
  return result;
}  /* compare_token_sequence_xref */


void add_to_constexpr_if_cache_hash_table(
				a_constexpr_if_cache_info_ptr	cicip,
				a_token_sequence_number		start_tsn)
					
/*
Add a copy of the entry specified by cicip to the constexpr if cache hash
table for the current template declaration.  start_tsn is the token sequence
number of the "if" of the "if constexpr" and is used as the key to the hash
table.
*/
{
  a_token_sequence_xref     tsx_key;
  a_template_decl_info_ptr  tdip;

  /* The hash table is stored in the a_template_decl entry for the
     current template. */
  if (depth_template_declaration_scope != NO_SCOPE_DEPTH) {
    tdip = scope_stack[depth_template_declaration_scope].template_decl_info;
  } else {
    tdip = get_curr_template_decl_info();
  }  /* if */
  if (tdip->constexpr_if_hash_table == NULL) {
    tdip->constexpr_if_hash_table =
                alloc_hash_table(FRONT_END_REGION_NUMBER, 1,
                                 fn_for_function(hash_token_sequence_xref),
                                 fn_for_function(compare_token_sequence_xref));
  }  /* if */
  tsx_key.token_sequence_number = start_tsn;

  a_token_sequence_xref_ptr *p_tsxp = (a_token_sequence_xref_ptr*)
                                       hash_find(tdip->constexpr_if_hash_table,
                                                 (a_void_ptr)&tsx_key,
                                                 /*create=*/TRUE);
  a_token_sequence_xref_ptr tsxp = *p_tsxp;
  if (tsxp != NULL) {
    /* An existing entry should never be found. */
    unexpected_condition();
  } else {
    tsxp = alloc_token_sequence_xref();
    tsxp->token_sequence_number = start_tsn;

    a_constexpr_if_cache_info_ptr new_cicip = alloc_constexpr_if_cache_info();
    *new_cicip = *cicip;
    tsxp->entry = (void*)new_cicip;
    *p_tsxp = tsxp;
  }  /* if */
}  /* add_to_constexpr_if_cache_hash_table */


a_constexpr_if_cache_info_ptr check_constexpr_if_cache_hash_table(
					a_token_sequence_number	start_tsn)
					
/*
Look up start_tsn in a constexpr if cache hash table.  Return the entry
found, or NULL if no entry is found.
*/
{
  a_token_sequence_xref_ptr	tsxp;
  a_token_sequence_xref_ptr	*p_tsxp;
  a_token_sequence_xref		tsx_key;
  a_constexpr_if_cache_info_ptr	result = NULL;
  a_template_decl_info_ptr	tdip = NULL;

  if (!inside_local_class) {
    /* The mechanism to find saved constexpr if information can't be used
       for members of local classes because of interactions with a potential
       enclosing cache (when enclosed in a template). */
    tdip = get_curr_template_decl_info();
  }  /* if */
  if (tdip != NULL && tdip->constexpr_if_hash_table != NULL) {
    tsx_key.token_sequence_number = start_tsn;
    p_tsxp = (a_token_sequence_xref_ptr*)
                             hash_find(tdip->constexpr_if_hash_table,
                                       (a_void_ptr)&tsx_key, /*create=*/FALSE);
    if (p_tsxp != NULL) {
      tsxp = *p_tsxp;
      result = (a_constexpr_if_cache_info_ptr)tsxp->entry;
#if DEBUG
/* GCOVR_EXCL_START */
      if (db_flag_is_set("ccicht")) {
        fprintf(f_debug,
                "Found constexpr_if cache tsn=%lu, else=%lu, ending=%lu\n",
                (unsigned long)start_tsn,
                (unsigned long)result->else_start_tsn,
                (unsigned long)result->end_start_tsn);
      }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
    }  /* if */
  }  /* if */
  return result;
}  /* check_constexpr_if_cache_hash_table */


a_hash_value hash_symbol_header_lookup_entry(a_void_ptr	key)
/*
Produce a hash value for a symbol header.  The key is a pointer to a
symbol header lookup entry.
*/
{
  a_hash_value				value;
  a_symbol_header_lookup_entry_ptr	shlep;

  shlep = (a_symbol_header_lookup_entry_ptr)key;
  value = shlep->header->hash_value;
  return value;
}  /* hash_symbol_header_lookup_entry */


a_boolean compare_symbol_header_lookup_entry(a_void_ptr	entry,
                                             a_void_ptr	key)
/*
Compare an entry in the symbol header lookup hash table with an entry to be
found.  "entry" and "key" are of type a_symbol_header_lookup_entry_ptr.
Return TRUE if the key matches the entry.
*/
{
  a_symbol_header_lookup_entry_ptr	entry_shlep;
  a_symbol_header_lookup_entry_ptr	key_shlep;
  a_boolean				result;

  entry_shlep = (a_symbol_header_lookup_entry_ptr)entry;
  key_shlep = (a_symbol_header_lookup_entry_ptr)key;
  result = entry_shlep->header == key_shlep->header;
  return result;
}  /* compare_symbol_header_lookup_entry */

namespace detail {

a_hash_table_ptr create_name_lookup_table(a_scope_kind	kind)
/*
Create a name lookup table for the specified scope kind.  Return a pointer to
it.
*/
{
  a_hash_table_ptr	hash_table;

  /* Select an initial size for the hash table.  This value is not critical
     as the table is enlarged if needed, and that is an inexpensive
     operation.  Note this is a minimum size.  The actual allocated size
     will be determined by select_hash_table_size. */
  a_hash_table_size	size = 0;
  switch (kind) {
    case sck_file: size = 100; break;
    case sck_func_prototype: size = 10; break;
    case sck_block: size = 10; break;
    case sck_namespace: size = 100; break;
    case sck_namespace_extension: size = 100; break;
    case sck_class_struct_union: size = 30; break;
    case sck_template_declaration: size = 10; break;
    case sck_template_instantiation: size = 10; break;
    case sck_pragma: size = 5; break;
    case sck_condition: size = 2; break;
    case sck_enum: size = 5; break;
    case sck_function: size = 20; break;
    case sck_class_reactivation: size=5; break;
    default:
#if DEBUG
/* GCOVR_EXCL_START */
      fprintf(f_debug, "Bad scope kind:\n");
      (void)db_scope_kind(kind);
/* GCOVR_EXCL_STOP */
#endif  /* DEBUG */
      unexpected_condition_str("create_name_lookup_table");
      break;
  }  /* switch */
  hash_table = alloc_hash_table(FRONT_END_REGION_NUMBER, size,
                          fn_for_function(hash_symbol_header_lookup_entry),
                          fn_for_function(compare_symbol_header_lookup_entry));
  return hash_table;
}  /* create_name_lookup_table */


a_symbol_ptr find_symbol_list_in_non_null_table(a_hash_table_ptr    hash_table,
                                                a_symbol_header_ptr header)
/*
Look up header in hash_table (which must be non-NULL).  Return a pointer to the
symbol list from the hash table or NULL if no entry was found.
*/
{
  a_symbol_ptr                      result_sym = NULL;
  a_symbol_header_lookup_entry      shle_key;
  a_symbol_header_lookup_entry_ptr  *shlep_in_table;

  /* Create an entry to be used as the lookup key. */
  clear_symbol_header_lookup_entry(&shle_key);
  shle_key.header = header;
  shlep_in_table = (a_symbol_header_lookup_entry_ptr*)
                            hash_find(hash_table,
                                      (a_void_ptr)&shle_key,
                                      /*create=*/FALSE);
  if (shlep_in_table != NULL) {
    result_sym = (*shlep_in_table)->symbols;
  }  /* if */
  return result_sym;
}  /* find_symbol_list_in_non_null_table */

}  /* namespace detail */

static void add_symbol_to_lookup_table(a_symbol_ptr     symbol,
                                       a_hash_table_ptr lookup_table)
/*
Add symbol to the specified lookup table.
*/
{
  a_symbol_header_lookup_entry_ptr shlep;
  a_symbol_header_lookup_entry_ptr *shlep_in_table;
  a_symbol_header_lookup_entry     shle_key;

  /* Some scopes do not have lookup tables.  Do nothing in that case. */
  if (lookup_table != NULL) {
    /* Create an entry to be used as the lookup key. */
    clear_symbol_header_lookup_entry(&shle_key);
    shle_key.header = symbol->header;
    shlep_in_table = (a_symbol_header_lookup_entry_ptr*)
                              hash_find(lookup_table,
                                        (a_void_ptr)&shle_key,
                                        /*create=*/TRUE);
    shlep = *shlep_in_table;
    if (shlep == NULL) {
      /* No entry was found -- create one now. */
      shlep = alloc_symbol_header_lookup_entry();
      shlep->header = symbol->header;
      /* Update the entry in the hash table. */
      *shlep_in_table = shlep;
    }  /* if */
    /* Link the symbol into the table. */
    symbol->next_in_lookup_table = shlep->symbols;
    shlep->symbols = symbol;
  }  /* if */
}  /* add_symbol_to_lookup_table */



void add_symbol_to_scope_list(a_symbol_ptr  sym_ptr,
                              a_scope_depth scope_depth,
                              a_boolean     *err)
/*
Add the given symbol to the list of symbols for the scope at scope_depth
in the scope stack.  *err is set to TRUE if there is an error; it is not
changed if there is no error.
*/
{
  a_scope_stack_entry_ptr     ssep;
  a_scope_pointers_block_ptr  pointers_block;
  a_namespace_ptr             nsp;
  a_scope_kind	              scope_kind = (a_scope_kind)sck_none;

  if (scope_depth == NO_SCOPE_DEPTH) {
    /* The scope to which this symbol belongs is not on the scope stack. */
    ssep = NULL;
    if (sym_ptr->is_class_member) {
      a_type_ptr			tp = sym_parent_class(sym_ptr);
      a_class_symbol_supplement_ptr	cssp;
      tp = skip_typerefs(tp);
      cssp = symbol_supplement_for_class(tp);
      pointers_block = &cssp->pointers_block;
      scope_kind = (a_scope_kind)sck_class_struct_union;
    } else {
      nsp = sym_parent_namespace_or_null(sym_ptr);
      if (nsp == NULL) {
        /* The symbol is being entered outside of any scope (e.g., a macro
           defined by a command-line -D option). */
        sym_ptr->decl_scope = NO_SCOPE_NUMBER;
        pointers_block = NULL;
      } else {
        /* The symbol belongs to a namespace scope that may not actually be on
           the stack.  This can happen with a friend declaration that causes
           instantiation of a function template that is a namespace member:
             namespace N { template <class T> void f(T); }
             class A { friend void N::f(int); };
        */
        nsp = skip_namespace_aliases(nsp);
        sym_ptr->decl_scope = nsp->variant.assoc_scope->number;
        pointers_block = &((a_symbol_ptr)nsp->source_corresp.assoc_info)->
                            variant.namespace_info.extra_info->pointers_block;
        scope_kind = (a_scope_kind)sck_namespace;
      }  /* if */
    }  /* if */
  } else {
#if CHECKING
    if (scope_depth < 0 || scope_depth > depth_scope_stack) {
      internal_error("add_symbol_to_scope_list: bad scope depth");
    }  /* if */
#endif /* CHECKING */
    ssep = &scope_stack[scope_depth];
    pointers_block = assoc_pointers_block_of(ssep);
    scope_kind = ssep->kind;
    /* Put the proper scope number into the symbol entry. */
    sym_ptr->decl_scope = ssep->number;
    if (C_dialect == C_dialect_cplusplus && !sym_ptr->is_error) {
      /* In C++, it's an error for something with the same name as a class to
         be defined within the class unless it's a constructor (the symbol
         for which is not added to the scope list) or a nonstatic data
         member that is not an anonymous union member (ARM 9.2). */
      if (ssep->kind == (a_scope_kind)sck_class_struct_union &&
          member_name_conflicts_with_class_name(ssep->assoc_type, sym_ptr)) {
        *err = TRUE;
      }  /* if */
    }  /* if */
#if RECORD_HIDDEN_NAMES_IN_IL
  /* If appropriate, set a flag in the symbol header to indicate that at
     least one declaration with this name was a tag name and/or appeared in
     the file scope or a namespace scope.  The information is used in
     building the hidden name table. */
    if (!sym_ptr->is_error &&
        sym_ptr->kind != (a_symbol_kind)sk_undefined &&
        sym_ptr->kind != (a_symbol_kind)sk_macro) {
      if (is_file_or_namespace_scope_kind(scope_kind)) {
        sym_ptr->header->any_decl_in_file_or_namespace_scope = TRUE;
      }  /* if */
    }  /* if */
    if (is_tag_symbol(sym_ptr)) sym_ptr->header->any_tag_decl = TRUE;
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
  }  /* if */
  sym_ptr->next_in_scope = NULL;
  sym_ptr->prev_in_scope = NULL;
  if (sym_ptr->is_error) {
    /* Error symbols are not added to the scope list. */
  } else if (pointers_block == NULL) {
    /* A symbol that is not associated with a scope (e.g., a keyword
       or predefined macro).  Keep a special list of such symbols. */
    if (symbols_with_no_scope == NULL) {
      symbols_with_no_scope = sym_ptr;
    } else {
      symbols_with_no_scope_tail->next_in_scope = sym_ptr;
      sym_ptr->prev_in_scope = symbols_with_no_scope_tail;
    }  /* if */
    symbols_with_no_scope_tail = sym_ptr;
  } else {
    /* Add the symbol to the end of the symbols list for the scope. */
    if (pointers_block->symbols == NULL) {
      pointers_block->symbols = sym_ptr;
    } else {
      pointers_block->last_symbol->next_in_scope = sym_ptr;
      sym_ptr->prev_in_scope = pointers_block->last_symbol;
    }  /* if */
    pointers_block->last_symbol = sym_ptr;
    if (is_scope_kind_with_lookup_table(scope_kind)) {
      a_module_ptr     module_ptr = lookup_module_for_symbol(sym_ptr);
      a_hash_table_ptr lookup_table = curr_lookup_table(pointers_block,
                                                        module_ptr,
                                                        scope_kind,
                                                        /*create=*/TRUE);
      add_symbol_to_lookup_table(sym_ptr, lookup_table);
    }  /* if */
  }  /* if */
}  /* add_symbol_to_scope_list */


void set_namespace_projection_symbol(a_symbol_ptr     proj_sym,
                                     a_symbol_ptr     fund_sym,
                                     a_scope_depth    scope_depth)
/*
Initialize the fields of the symbol proj_sym to point to be a namespace
projection symbol that points to fund_sym.  proj_sym must already point to
an sk_namespace_projection symbol. scope_depth is the depth in the scope
stack of the projection symbol, or NO_SCOPE_DEPTH if the decl_scope of
proj_sym should remain unchanged.
*/
{
  /* Make sure fund_sym is really a fundamental symbol and not another
     projection. */
  fund_sym = fundamental_symbol_of(fund_sym);
  proj_sym->variant.namespace_projection.fundamental_symbol = fund_sym;
  if (scope_depth != NO_SCOPE_DEPTH) {
    proj_sym->decl_scope = scope_stack[scope_depth].number;
  }  /* if */
}  /* set_namespace_projection_symbol */


a_symbol_ptr make_namespace_projection_symbol(a_symbol_ptr       fund_sym,
                                              a_source_position  *pos,
                                              a_scope_depth      scope_depth)
/*
Create a namespace projection symbol and set it to point to fund_sym.  pos
is the source position to be associated with the projection symbol, and
scope_depth is its depth in the scope stack.
*/
{
  a_symbol_ptr	sym;

  sym = alloc_symbol((a_symbol_kind)sk_namespace_projection,
                     fund_sym->header, pos);
  set_namespace_projection_symbol(sym, fund_sym, scope_depth);
  set_decl_sequence_number(sym);
  return sym;
}  /* make_namespace_projection_symbol */


a_symbol_ptr enter_namespace_projection_symbol(a_symbol_ptr     fund_sym,
                                               a_boolean        is_using_decl,
                                               a_symbol_locator *location,
                                               a_scope_depth    scope_depth,
                                               a_boolean        suppress_error)
/*
Create a namespace projection symbol, set it to point to fund_sym,
and enter it in the symbol table.  is_using_decl is TRUE if this represents
a using-declaration.  This routine is like enter_symbol, but it is only
used to create namespace projection symbols.  enter_symbol should not be
used to create namespace projection symbols because the fundamental symbol
pointer must be set before link_symbol_into_symbol_table is called.
*/
{
  a_symbol_ptr	sym_ptr;

  sym_ptr = make_namespace_projection_symbol(fund_sym,
                                             &location->source_position,
                                             scope_depth);
  sym_ptr->is_error = location->is_error;
  sym_ptr->variant.namespace_projection.is_using_decl = is_using_decl;
  /* Set the locator to point to the symbol entered. */
  location->specific_symbol = sym_ptr;
  location->is_qualified_name = FALSE;
  /* Enter the symbol into the symbol table. */
  add_symbol_to_symbol_table(sym_ptr, scope_depth, suppress_error);
  return sym_ptr;
}  /* enter_namespace_projection_symbol */


void add_friend_function_to_lookup_list_for_class(a_symbol_ptr  rout_sym,
                                                  a_type_ptr    class_type)
/*
rout_sym represents a non-class-member function that has been declared a
friend of the specified class.  Enter it on a list that is used by argument-
dependent lookup.  
*/
{
  a_class_symbol_supplement_ptr  cssp;
  a_symbol_ptr                   sym, other_sym, overload_sym = NULL;
  a_boolean                      duplicate = FALSE, is_list;

  /* Check for other functions with the same name that have been declared
     friends of the current class. */
  cssp = symbol_supplement_for_class(class_type);
  for (other_sym = cssp->friend_functions;
       other_sym != NULL;
       other_sym = other_sym->next) {
    if (other_sym->header == rout_sym->header) break;
  }  /* for */
  if (other_sym != NULL) {
    /* Ignore this symbol if it's a duplicate. */
    sym = other_sym;
    is_list = (sym->kind == (a_symbol_kind)sk_overloaded_function);
    if (is_list) {
      /* An overload set already exists. */
      overload_sym = sym;
      sym = overload_sym->variant.overloaded_function.symbols;
    }  /* if */
    for (; sym != NULL; sym = (is_list ? sym->next : NULL)) {
      if (fundamental_symbol_of(sym) == rout_sym) {
        duplicate = TRUE;
        break;
      }  /* if */
    }  /* if */
  }  /* if */
  if (!duplicate) {
    /* Create a namespace projection symbol (one that won't actually be
       entered in the symbol table) to point to the friend function symbol. */
    sym = make_namespace_projection_symbol(rout_sym,
                                           &rout_sym->decl_position,
                                           depth_innermost_namespace_scope);
    /* Now add the projection symbol to the class symbol supplement. */
    if (other_sym == NULL) {
      /* No overloading -- add it directly. */
      sym->next = cssp->friend_functions;
      cssp->friend_functions = sym;
    } else if (overload_sym != NULL) {
      /* An overload set already exists.  Add the new symbol to the overload
         set. */
      sym->next = overload_sym->variant.overloaded_function.symbols;
      overload_sym->variant.overloaded_function.symbols = sym;
      sym->overload_set_member = TRUE;
    } else {
      /* An overload set will have to be created.  First remove the other
         symbol from the main list; it will be added to the overload set
         later. */
      if (cssp->friend_functions == other_sym) {
        cssp->friend_functions = other_sym->next;
      } else {
        a_symbol_ptr  prev = cssp->friend_functions;
        while (prev->next != other_sym) prev = prev->next;
        prev->next = other_sym->next;
      }  /* if */
      other_sym->next = NULL;
      /* Create a symbol for the overload set. */
      overload_sym = alloc_symbol((a_symbol_kind)sk_overloaded_function,
                                   sym->header, &other_sym->decl_position);
      overload_sym->decl_scope = sym->decl_scope;
      /* Add the two symbols to the overload set. */
      overload_sym->variant.overloaded_function.symbols = sym;
      sym->overload_set_member = TRUE;
      sym->next = other_sym;
      other_sym->overload_set_member = TRUE;
      /* Add the overload set to the list. */
      overload_sym->next = cssp->friend_functions;
      cssp->friend_functions = overload_sym;
    }  /* if */
  }  /* if */
}  /* add_friend_function_to_lookup_list_for_class */


a_boolean is_symbol_from_inline_namespace_of_scope(a_symbol_ptr	sym,
						   a_scope_ptr	scope)
/*
Return TRUE if the parent namespace of sym is an inline namespace of scope.
*/
{
  a_using_decl_ptr	udp;
  a_namespace_ptr	parent_nsp;
  a_boolean		result = FALSE;

  parent_nsp = parent_namespace_for_symbol(sym);
  /* Go through the using-directives of the scope.  Look for an
     inline namespace using-directive that names the parent namespace of
     the symbol. */
  for (udp = scope->using_directives; udp != NULL; udp = udp->next) {
    if (udp->inline_namespace) {
      a_namespace_ptr	udp_nsp = (a_namespace_ptr)udp->entity.ptr;
      if (same_entities(parent_nsp, udp_nsp)) {
        result = TRUE;
        break;
      } else if (is_symbol_from_inline_namespace_of_scope(
                                          sym, udp_nsp->variant.assoc_scope)) {
        result = TRUE;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return result;
}  /* is_symbol_from_inline_namespace_of_scope */


a_boolean is_symbol_from_inline_namespace_of_parent(a_symbol_ptr	ns_sym,
						    a_symbol_ptr	sym)
/*
Return TRUE if sym is a member of an inline namespace of the parent
namespace of ns_sym or is an immediate member of that namespace.  If sym
is not a namespace projection symbol, also return TRUE.
*/
{
  a_boolean		result = TRUE;
  
  if (symbol_is(sym, sk_namespace_projection)) {
    a_symbol_ptr	fund_sym = fundamental_symbol_of(sym);
    a_namespace_ptr	parent_nsp = ns_sym->parent.namespace_ptr;
    a_scope_ptr		parent_scope;
    result = FALSE;
    if (parent_nsp != NULL) {
      parent_scope = parent_nsp->variant.assoc_scope;
    } else {
      parent_scope = scope_stack[DEPTH_OF_FILE_SCOPE].il_scope;
    }  /* if */
    if (fund_sym->decl_scope == ns_sym->decl_scope) {
      result = TRUE;
    } else if (is_symbol_from_inline_namespace_of_scope(fund_sym,
                                                        parent_scope)) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* is_symbol_from_inline_namespace_of_parent */


a_boolean is_symbol_from_inline_namespace(a_symbol_ptr	sym)
/*
Determine whether sym is from a namespace that has been made visible by
an inline namespace or a GNU strong using-directive in the current
namespace.
*/
{
  a_boolean			result = FALSE;
  a_scope_stack_entry_ptr	ssep;

  /* Suppress the processing if inline namespaces are not enabled.  The
     g++ strong using directive feature is implemented using the inline
     namespace mechanism, so this processing is also required in g++ mode. */
  if (inline_namespaces_enabled || gpp_mode) {
    for (ssep = scope_stack_entry_for(depth_scope_stack);
         !result && ssep != NULL;
         ssep = previous_scope_of(ssep)) {
      /* Inline namespace using-directives only appear at namespace scope. */
      if (ssep->kind == (a_scope_kind)sck_namespace ||
          ssep->kind == (a_scope_kind)sck_namespace_extension ||
          ssep->kind == (a_scope_kind)sck_file) {
        if (is_symbol_from_inline_namespace_of_scope(sym, ssep->il_scope)) {
          result = TRUE;
          break;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  return result;
}  /* is_symbol_from_inline_namespace */


static
a_scope_pointers_block_ptr pointers_block_for_namespace(a_namespace_ptr nsp)
/*
Return a pointer to the scope pointers block for nsp.  If nsp is NULL, return
the scope pointers block for the global scope.
*/
{
  a_scope_pointers_block_ptr	result;

  if (nsp == NULL) {
    result = assoc_pointers_block_of(&scope_stack[DEPTH_OF_FILE_SCOPE]);
  } else {
    result = &symbol_supplement_for_namespace(nsp)->pointers_block;
  }  /* if */
  return result;
}  /* pointers_block_for_namespace */


a_symbol_ptr enter_synthesized_projection_symbol(
                               a_symbol_ptr		fund_sym,
                               a_symbol_locator		*location,
                               a_boolean		qualified_lookup,
                               a_namespace_ptr		qualifier_namespace,
                               an_id_lookup_options_set	options)
/*
Create a synthesized namespace projection symbol, set it to point to fund_sym,
add it to the "other" symbols list of the symbol header, and the
synthesized namespace projections symbols list of the scope it is being
added to.

qualified_lookup is TRUE if the symbol being created is the result of
a namespace or file scope qualified lookup.  For qualified lookups
qualifier_namespace points to the namespace in which the lookup is
being done, or is NULL for a file scope lookup.  options specifies
the options being used for the lookup.
*/
{
  a_symbol_ptr			sym_ptr;
  a_symbol_header_ptr		sym_hdr;
  a_scope_pointers_block_ptr	pointers_block;
  a_boolean			can_be_reused;
  a_scope_depth			scope_depth;
  a_scope_stack_entry_ptr	ssep;

  scope_depth = scope_depth_for_synth_namespace_symbol();
  ssep = scope_stack_entry_for(scope_depth);
  sym_ptr = make_namespace_projection_symbol(fund_sym,
                                             &location->source_position,
                                             scope_depth);
  sym_ptr->is_error = location->is_error;
  /* Set the locator to point to the symbol entered. */
  location->specific_symbol = sym_ptr;
  location->is_qualified_name = qualified_lookup;
  /* Synthesized projection symbols are not entered into the symbol table.
     They are put on the "other" symbols list and are added to a separate
     list in the scope stack. */
  sym_hdr = sym_ptr->header;
  /* For a qualified lookup, add the symbol to the scope in which the
     name is being looked up.  For unqualified lookups, add it to the
     current scope. */
  if (qualified_lookup) {
    pointers_block = pointers_block_for_namespace(qualifier_namespace);
  } else {
    pointers_block = assoc_pointers_block_of(ssep);
  }  /* if */
  sym_ptr->next_in_scope = pointers_block->synth_namespace_projection_symbols;
  if (pointers_block->synth_namespace_projection_symbols != NULL) {
    pointers_block->synth_namespace_projection_symbols->prev_in_scope =
                                                                       sym_ptr;
  }  /* if */
  pointers_block->synth_namespace_projection_symbols = sym_ptr;
  /* See if the specified lookup options represent a reusable lookup. */
  can_be_reused = is_reusable_using_directive_lookup(options);
  if (can_be_reused) {
    /* Only reusable symbols are entered on the "other" symbols list. */
    sym_ptr->next = sym_hdr->other_symbols;
    sym_hdr->other_symbols = sym_ptr;
  }  /* if */
  /* Set the flags that describe the kind of that created this symbol. */
  sym_ptr->synthesized_namespace_projection = TRUE;
  sym_ptr->do_not_reuse = !can_be_reused;
  sym_ptr->qualified_lookup = qualified_lookup;
  if (qualified_lookup && qualifier_namespace != NULL) {
    set_namespace_membership(sym_ptr, (a_source_correspondence*)NULL,
                             qualifier_namespace);
  } else if (scope_is(ssep, sck_namespace) ||
             scope_is(ssep, sck_namespace_extension)) {
    /* For an unqualified lookup, if the symbol is being created in a namespace
       scope, set the namespace parent information. */
    set_namespace_membership(sym_ptr, (a_source_correspondence*)NULL,
                             ssep->assoc_namespace);
  }  /* if */
  if (qualified_lookup) {
    /* Set the decl_scope of the symbol to the scope number associated
       with the qualifiers namespace. */
    if (qualifier_namespace == NULL) {
      sym_ptr->decl_scope = scope_stack[DEPTH_OF_FILE_SCOPE].number;
    } else {
      sym_ptr->decl_scope = qualifier_namespace->variant.assoc_scope->number;
    }  /* if */
  }  /* if */
  sym_ptr->must_be_class_or_namespace_lookup =
                              (options & IDL_MUST_BE_CLASS_OR_NAMESPACE) != 0;
  sym_ptr->must_be_tag_lookup = (options & IDL_MUST_BE_TAG) != 0;
  sym_ptr->tentative_type_lookup = (options & IDL_TENTATIVE_TYPE_LOOKUP) != 0;
  sym_ptr->must_be_class_lookup = (options & IDL_MUST_BE_CLASS) != 0;
  sym_ptr->must_be_namespace_lookup = (options & IDL_MUST_BE_NAMESPACE) != 0;
  sym_ptr->instantiation_context_lookup =
                                    (options & IDL_INSTANTIATION_CONTEXT) != 0;
  return sym_ptr;
}  /* enter_synthesized_projection_symbol */


static void copy_symbol_lookup_flags(a_symbol_ptr from,
                                     a_symbol_ptr to)
/*
Copy the lookup flags for synthesized namespace projection symbols from
the symbol pointed to by "from" to the symbol pointed to by "to".
*/
{
  to->synthesized_namespace_projection
                                     = from->synthesized_namespace_projection;
  to->do_not_reuse                   = from->do_not_reuse;
  to->qualified_lookup               = from->qualified_lookup;
  to->must_be_class_or_namespace_lookup
                                     = from->must_be_class_or_namespace_lookup;
  to->must_be_tag_lookup             = from->must_be_tag_lookup;
  to->tentative_type_lookup          = from->tentative_type_lookup;
  to->must_be_class_lookup           = from->must_be_class_lookup;
  to->must_be_namespace_lookup       = from->must_be_namespace_lookup;
  to->instantiation_context_lookup   = from->instantiation_context_lookup;
}  /* copy_symbol_lookup_flags */


static void copy_locator_parent_to_sym(a_symbol_locator  *loc,
                                       a_symbol_ptr      sym)
/*
Copy the parent entity description from the locator to the symbol (if
applicable).
*/
{
  if (loc->is_class_member) {
    /* Make sure the parent is indeed a class type. */
    a_type_ptr  class_type = loc->parent.class_type;
    if (class_type->kind == (a_type_kind)tk_template_param) {
      class_type = proxy_class_for_template_param(class_type);
    }  /* if */
    if (class_type != NULL && is_immediate_class_type(class_type)) {
      sym->is_class_member = TRUE;
      sym->parent.class_type = class_type;
    } else {
      expect_error();
    }  /* if */
  } else {
    sym->parent = loc->parent;
  }  /* if */
}  /* copy_locator_parent_to_sym */


a_symbol_ptr make_symbol(a_symbol_kind    sym_kind,
			 a_symbol_locator *location)
/*
Create a new symbol entry, but don't add it to the symbol table.
*location must be refer to the desired symbol header.  If *location is
an error locator, an error symbol is created.  Note that any specific symbol
indicated in the locator is supposed to be ignored.
*/
{
  a_symbol_ptr sym_ptr;

  /* Allocate and initialize the symbol. */
  sym_ptr = alloc_symbol(sym_kind, location->symbol_header,
                         &location->source_position);
  sym_ptr->is_error = location->is_error;
  if (sym_ptr->is_error) {
    /* In the error case it can be useful to remember the membership (e.g.,
       to recognize constructor-like symbols). */
    copy_locator_parent_to_sym(location, sym_ptr);
  }  /* if */
  /* Set the locator to point to the symbol entered. */
  location->specific_symbol = sym_ptr;
  location->is_qualified_name = FALSE;
  return sym_ptr;
}  /* make_symbol */


void add_symbol_to_symbol_table(a_symbol_ptr     sym_ptr,
				a_scope_depth    scope_depth,
	                        a_boolean        suppress_error)
/*
Add sym_ptr to the symbol table and the scope list for its scope.
scope_depth indicates the level of the scope stack at which the symbol
should be entered.  Generate an error if the symbol is already defined
in that scope and name space unless suppress_error is TRUE.
*/
{
  /* Add the symbol to the proper scope's symbol list. */
  add_symbol_to_scope_list(sym_ptr, scope_depth, &suppress_error);
  /* Add the symbol to the symbol table.  This must be done after the symbol
     is added to the scope list, because that sets the scope number, which
     is needed to check for redeclaration. */
  link_symbol_into_symbol_table(sym_ptr, scope_depth, suppress_error);
}  /* add_symbol_to_symbol_table */


a_symbol_ptr enter_symbol(a_symbol_kind    sym_kind,
			  a_symbol_locator *location,
			  a_scope_depth    scope_depth,
                          a_boolean        suppress_error)
/*
Enter a new symbol table entry into the symbol table.  This routine assumes
that "find_symbol" has already been called to correctly set the header that
the symbol will be linked from (this is summarized in *location).  scope_depth
indicates the level of the scope stack at which the symbol should be entered.
Generate an error if the symbol is already defined in that scope and name
space unless suppress_error is TRUE.  If *location is an error locator,
an error symbol is created and entered.  Note that any specific symbol
indicated in the locator is supposed to be ignored.

If this routine is changed, enter_namespace_projection_symbol may need to
be changed too.
*/
{
  a_symbol_ptr sym_ptr;

  db_enter(4, "enter_symbol");

  /* Allocate and initialize the symbol. */
  sym_ptr = make_symbol(sym_kind, location);
  /* Enter the symbol into the symbol table. */
  add_symbol_to_symbol_table(sym_ptr, scope_depth, suppress_error);
  db_exit();
  return sym_ptr;
}  /* enter_symbol */


static a_symbol_ptr create_symbol_for_non_initial_variadic_param(
						a_symbol_locator *location)
/*
Create a symbol to represent a non-initial parameter of a variadic
function.  Such symbols are not entered into the symbol table.
*/
{
  a_symbol_ptr	sym_ptr;

  /* Allocate and initialize the symbol. */
  sym_ptr = alloc_symbol((a_symbol_kind)sk_parameter, location->symbol_header,
                         &location->source_position);
  sym_ptr->is_error = location->is_error;
  /* Set the locator to point to the symbol entered. */
  location->specific_symbol = sym_ptr;
  return sym_ptr;
}  /* create_symbol_for_non_initial_variadic_param */


a_symbol_ptr enter_extern_symbol(a_symbol_kind    sym_kind,
                                 a_symbol_locator *locator)
/*
Enter an sk_extern_variable or sk_extern_routine symbol into the symbol
table.  Note that these symbols are put on the "other" symbols list -- they
are not actually put onto the active list, and they are not found during
normal symbol lookup.
*/
{
  a_symbol_header_ptr  header = locator->symbol_header;
  a_symbol_ptr         sym;
  a_boolean            err;
  a_namespace_ptr      nsp;

  db_enter(4, "enter_extern_symbol");
  sym = alloc_symbol(sym_kind, header, &locator->source_position);
  /* Extern symbol entries are associated with the file scope of the current
     translation units.  This allows find_external_symbol to discard extern
     symbols created in other file scopes. */
  sym->decl_scope = file_scope_number;
  if (is_error_locator(*locator)) {
    sym->is_error = TRUE;
  } else {
    /* Just add the entry to the front of the list. */
    sym->next = header->other_symbols;
    header->other_symbols = sym;
  }  /* if */
  if (!C_mode()) {
    /* See if this symbol is directly or indirectly a namespace member. */
    nsp = qualifier_namespace_ptr(*locator);
    if (nsp == NULL &&
        depth_innermost_namespace_scope != DEPTH_OF_FILE_SCOPE) {
      nsp = scope_stack[depth_innermost_namespace_scope].
                                  il_scope->variant.assoc_namespace;
    }  /* if */
    /* Set namespace membership, if required. */
    if (nsp != NULL) {
      set_namespace_membership(sym, (a_source_correspondence *)NULL, nsp);
    }  /* if */
  }  /* if */
  /* Add the symbol to the file scope's symbol list, for checking when the
     file scope is popped.  (Note: we do not add sk_extern_variable and
     sk_extern_routine symbols to the symbol table proper.) */
  add_symbol_to_scope_list(sym, DEPTH_OF_FILE_SCOPE, &err);
  db_exit();
  return sym;
}  /* enter_extern_symbol */


void reenter_symbol(a_symbol_ptr     symbol_to_reenter,
		    a_scope_depth    scope_depth,
                    a_boolean        suppress_error)
/*
Re-enter a symbol table entry into the symbol table.  symbol_to_reenter
points to a symbol that was previously in the symbol table and was
removed.  scope_depth indicates the level of the scope stack at which the
symbol should be re-entered.  Generate an error if the symbol is already
defined in that scope and name space unless suppress_error is TRUE.
*/
{
  add_symbol_to_symbol_table(symbol_to_reenter, scope_depth, suppress_error);
}  /* reenter_symbol */


void enter_symbol_into_completed_class(a_symbol_ptr  sym)
/*
Enter the given symbol, which represents a member of a complete class, in the
symbol table.  (That implies that it will be added to the "inactive list" for
the corresponding symbol header.)
*/
{
  check_assertion(sym->is_class_member);
  add_symbol_to_symbol_table(sym, NO_SCOPE_DEPTH, /*suppress_error=*/FALSE);
}  /* enter_symbol_into_completed_class */


a_symbol_ptr enter_enumerator_into_completed_class(
                                                 a_symbol_locator  *loc,
                                                 a_type_ptr        class_type,
                                                 a_scope_number    scope_num)
/*
An enumerator constant is being added to the out-of-class definition of an
enum type that is a member of class_type (scope_num is the number of the class
definition scope).  The enumerator's name and position is given through loc.  
Issue an error if the enumerator name conflicts with a prior declaration, and
enter a symbol to represent it.  A pointer to that symbol is returned.
*/
{
  an_id_lookup_options_set  idl_options = IDL_DIRECT_CLASS_MEMBERS_ONLY;
  a_symbol_ptr              prev_sym, sym;

  clear_specific_symbol(*loc);
  prev_sym = class_qualified_id_lookup(loc, class_type, idl_options);
  if (prev_sym != NULL && !is_tag_symbol(prev_sym)) {
    pos_sy_error(ec_enumerator_already_declared, &loc->source_position,
                 prev_sym);
  }  /* if */
  sym = make_symbol((a_symbol_kind)sk_constant, loc);
  sym->is_class_member = TRUE;
  sym->decl_scope = scope_num;
  sym->parent.class_type = class_type;
  enter_symbol_into_completed_class(sym);
  return sym;
}  /* enter_enumerator_into_completed_class */


void reenter_block_scope_symbol(a_symbol_ptr  sym)
/*
sym is a symbol from a block scope that is being reactivated.  Add the
symbol back to the active list.
*/
{
  link_symbol_into_symbol_table(sym, depth_scope_stack,
                                /*suppress_error=*/FALSE);
}  /* reenter_block_scope_symbol */


a_symbol_ptr enter_copy_of_symbol(a_symbol_ptr     orig_sym,
                                  a_scope_depth    scope_depth,
                                  a_boolean        suppress_error)
/*
Enter a copy of a symbol table entry into the symbol table.  orig_sym
points to a symbol that is in the symbol table.  scope_depth indicates the
level of the scope stack at which the symbol should be entered.  Generate
an error if the symbol is already defined in that scope and name space
unless suppress_error is TRUE.
*/
{
  a_symbol_ptr	new_sym;

  db_enter(4, "enter_copy_of_symbol");
  /* Allocate a new symbol into which the symbol will be copied. */
  new_sym = alloc_symbol(orig_sym->kind, orig_sym->header,
                         &orig_sym->decl_position);
  *new_sym = *orig_sym;
  /* Clear the next pointers. */
  new_sym->next = NULL;
  new_sym->next_in_scope = NULL;
  new_sym->prev_in_scope = NULL;
  /* Enter the symbol into the symbol table. */
  add_symbol_to_symbol_table(new_sym, scope_depth, suppress_error);
  db_exit();
  return new_sym;
}  /* enter_copy_of_symbol */


void reactivate_prototype_scope_symbols(a_symbol_ptr  prototype_scope_symbols)
/*
Some symbols for types were created in a function prototype scope.
Reactivate those symbols (which were removed at the end of the function
prototype scope) now that we are in the body of the function.
*/
{
  a_symbol_ptr curr_symbol, next_symbol;

  /* Re-enter each symbol in the symbol table. */
  for (curr_symbol = prototype_scope_symbols;
       curr_symbol != NULL;
       curr_symbol = next_symbol) {
    next_symbol = curr_symbol->next_in_scope;
    reenter_symbol(curr_symbol, depth_scope_stack, /*suppress_error=*/TRUE);
    curr_symbol->reentered_from_prototype_scope = TRUE;
  }  /* for */
}  /* reactivate_prototype_scope_symbols */


void relink_unnamed_tag_symbol(a_symbol_ptr      sym,
                               a_symbol_locator  *locator)
/*
A name is belatedly specified for a class, and so the tag symbol originally
created for it must be modified to bear the new name.  Give the symbol
the new name and relink it into the symbol table under the new header.
*/
{
  db_enter(4, "relink_unnamed_tag_symbol");
#if CHECKING
  /* The symbol should not have been linked onto the symbol list for its
     header. */
  if (sym->header != unnamed_tag_symbol_header) {
    internal_error("relink_unnamed_tag_symbol: unexpected symbol header");
  }  /* if */
  /* The declaration scope should not be changed. */
  if (scope_stack[decl_scope_level].number != sym->decl_scope) {
    internal_error("relink_unnamed_tag_symbol: bad scope");
  }  /* if */
#endif /* CHECKING */
  /* Replace the special symbol header for unnamed class symbols with the
     header associated with its new name. */
  sym->header = locator->symbol_header;
  /* Add the symbol to the symbol table. */
  reenter_symbol(sym, decl_scope_level, /*suppress_error=*/FALSE);
  db_exit();
}  /* relink_unnamed_tag_symbol */


void enter_undefined_symbol(a_symbol_ptr sym)
/*
The indicated symbol is an sk_undefined symbol created because of an
undefined identifier.  It is now known that this is an error.  Enter
the symbol into the symbol table so it can be found on subsequent
uses of the name.
*/
{
  /* Note that the is_error flag is not set on this symbol.  Error
     symbols are not entered into the symbol table, but undefined
     symbols need to be (for error recovery purposes). */
  reenter_symbol(sym, decl_scope_level, /*suppress_error=*/TRUE);
}  /* enter_undefined_symbol */


a_symbol_ptr enter_undefined_member_symbol(a_symbol_locator *locator)
/*
Enter a symbol for an undefined class member, or find an existing one, and
return a pointer to the symbol.  This is used for invalid member references
so that there is a symbol against which a reference can be recorded.
*locator gives the necessary information about the member name and position.
*/
{
  a_symbol_ptr sym_ptr;

  db_enter(4, "enter_undefined_member_symbol");
  /* Look for an existing sk_undefined symbol on the inactive list. */
  for (sym_ptr = inactive_symbol_list_from_locator(*locator);
       sym_ptr != NULL;
       sym_ptr = sym_ptr->next) {
    if (sym_ptr->kind == (a_symbol_kind)sk_undefined &&
        sym_ptr->decl_scope == NO_SCOPE_NUMBER) {
      /* Found one. */
      break;
    }  /* if */
  }  /* for */
  if (sym_ptr == NULL ) {
    /* Allocate and initialize the symbol. */
    a_symbol_header_ptr sym_hdr = locator->symbol_header;
    sym_ptr = alloc_symbol((a_symbol_kind)sk_undefined, sym_hdr,
                           &locator->source_position);
    sym_ptr->is_error = TRUE;
    /* Add the symbol to the front of the inactive list. */
    add_symbol_to_inactive_list(sym_ptr);
  }  /* if */
  db_exit();
  return sym_ptr;
}  /* enter_undefined_member_symbol */


a_symbol_ptr add_symbol_to_overload_list(a_symbol_ptr    new_sym,
                                         a_symbol_ptr    other_sym,
                                         a_boolean       use_namespace,
                                         a_namespace_ptr ns_ptr)
/*
new_sym is a newly created function (or function template) symbol that
shares a name with other_sym, which is either an overloaded function symbol
or another function or function template symbol.  If necessary, create an
overloaded function symbol and add other_sym to its list.  Add new_sym to
the new or existing list of overloaded functions, and return a pointer to
the overloaded function symbol.

If other_sym is not already an sk_overloaded_function symbol, it
may need to be removed from the symbol header and scope stack lists
and replaced with the newly created overloaded function symbol.
Normally, the scope list is found by looking through the scope stack
for the decl_scope of other_sym.  However, if use_namespace
is TRUE, the pointers block associated with the namespace pointed to
by ns_ptr is used instead.  If ns_ptr is NULL, the pointers block for
the file scope is used.
*/
{
  a_symbol_ptr        overload_sym, prev_sym_ptr;
  a_symbol_header_ptr hdr_ptr;

  if (other_sym->kind == (a_symbol_kind)sk_overloaded_function) {
    overload_sym = other_sym;
    other_sym = overload_sym->variant.overloaded_function.symbols;
  } else {
    a_scope_pointers_block_ptr pointers_block = NULL;
    a_scope_kind               pointers_block_scope_kind;

    /* The existing symbol is not an sk_overloaded_function symbol
       (i.e., it's a simple function symbol of some kind). */
    if (!use_namespace) {
      a_scope_stack_entry *ssep;

      if (other_sym->synthesized_namespace_projection) {
        /* When looking for a synthesized namespace symbol, look in the
           scope in which the symbol was entered. */
        a_scope_depth	scope_depth;
        scope_depth = scope_depth_for_synth_namespace_symbol();
        ssep = scope_stack_entry_for(scope_depth);
      } else {
        /* Find the scope stack entry associated with this declaration. */
        ssep = &scope_stack[decl_scope_level];
        /* If the scope stack entry for the overloaded function is not that of
           the current scope (e.g., when a friend declaration refers to a 
           function at file scope), find the correct one. */
        while (ssep->number != other_sym->decl_scope) {
          check_assertion(ssep != &scope_stack[DEPTH_OF_FILE_SCOPE]);
          --ssep;
        }  /* if */
      }  /* if */
      pointers_block = assoc_pointers_block_of(ssep);
      pointers_block_scope_kind = ssep->kind;
    } else {
      /* A namespace pointer was passed by the caller.  Use the pointers
         block associated with this namespace. */
      pointers_block = pointers_block_for_namespace(ns_ptr);
      pointers_block_scope_kind = ns_ptr == NULL ? sck_file : sck_namespace;
    }  /* if */
    /* Create an sk_overloaded_function symbol and attach the old
       function symbol to it. */
    hdr_ptr = other_sym->header;
    overload_sym = alloc_symbol((a_symbol_kind)sk_overloaded_function,
                                   hdr_ptr, &(other_sym->decl_position));
    overload_sym->decl_scope = other_sym->decl_scope;
    overload_sym->decl_seq = other_sym->decl_seq;
    overload_sym->potentially_overloaded = other_sym->potentially_overloaded;
    /* If the symbol is a member of a class or namespace, set the membership
       of the new symbol. */
    if (other_sym->is_class_member) {
      set_class_membership(overload_sym, (a_source_correspondence *)NULL,
                           sym_parent_class(other_sym));
    } else if (sym_is_namespace_member(other_sym)) {
      set_namespace_membership(overload_sym,  (a_source_correspondence *)NULL,
                               sym_parent_namespace(other_sym));
    }  /* if */
    /* Synthesized projection symbols marked "do not reuse" are not on
       any of the symbol header lists, so don't try to find them. */
    if (!other_sym->do_not_reuse) {
      /* Put overload_sym into the symbol list in place of other_sym.
         This will normally use the active symbol list, but may use the
         inactive list for namespace extensions.  If other_sym is a
         synthesized namespace projection symbol, it will be on the
         "other" symbols list. */
      /* Find the symbol preceding other_sym on its list. */
      if (other_sym->synthesized_namespace_projection) {
        prev_sym_ptr = hdr_ptr->other_symbols;
        if (prev_sym_ptr == other_sym) hdr_ptr->other_symbols = overload_sym;
      } else if (pointers_block->add_symbols_to_inactive_list) {
        prev_sym_ptr = hdr_ptr->inactive_symbols;
        if (prev_sym_ptr == other_sym) {
          hdr_ptr->inactive_symbols = overload_sym;
        }  /* if */
      } else {
        prev_sym_ptr = hdr_ptr->symbol;
        if (prev_sym_ptr == other_sym) hdr_ptr->symbol = overload_sym;
      }  /* if */
      if (prev_sym_ptr == other_sym) {
        /* The entry is the first on the header list.  This case is handled
           above. */
      } else {
        while (prev_sym_ptr != NULL && prev_sym_ptr->next != other_sym) {
          prev_sym_ptr = prev_sym_ptr->next;
        }  /* while */
        check_assertion_str2(prev_sym_ptr != NULL,
                             "add_symbol_to_overload_list:",
                             "symbol not in symbol header list");
        prev_sym_ptr->next = overload_sym;
      }  /* if */
      overload_sym->next = other_sym->next;
      other_sym->next = NULL;
    }  /* if */
    /* Also put overload_sym into the scope list in place of other_sym. */
    if (other_sym->synthesized_namespace_projection) {
      prev_sym_ptr = pointers_block->synth_namespace_projection_symbols;
      if (prev_sym_ptr == other_sym) {
        /* The entry is the first on the scope's symbol list. */
        pointers_block->synth_namespace_projection_symbols = overload_sym;
      }  /* if */
      /* Transfer any symbol lookup flags that are set to the newly
         created overloaded function symbol. */
      copy_symbol_lookup_flags(other_sym, overload_sym);
    } else {
      prev_sym_ptr = pointers_block->symbols;
      if (prev_sym_ptr == other_sym) {
        /* The entry is the first on the scope's symbol list. */
        pointers_block->symbols = overload_sym;
      }  /* if */

      /* Remove the original symbol from the hash table and add the new one. */
      a_module_ptr     module_ptr = lookup_module_for_symbol(other_sym);
      a_hash_table_ptr original_lookup_table = curr_lookup_table(
                                                    pointers_block,
                                                    module_ptr,
                                                    pointers_block_scope_kind);
      remove_symbol_from_lookup_table(other_sym, original_lookup_table);

      /* The overload symbol is always entered into the non-module-specific
         lookup table; overload resolution is then responsible for filtering
         the overload set to only those overload candidates that are currently
         visible. */
      a_hash_table_ptr new_lookup_table = original_lookup_table;
      if (module_ptr != NULL) {
        /* Only find the lookup table again if the original symbol was actually
           in a module-specific lookup table. */
        new_lookup_table = curr_lookup_table(pointers_block,
                                             /*module=*/NULL,
                                             pointers_block_scope_kind);
      }  /* if */
      add_symbol_to_lookup_table(overload_sym, new_lookup_table);
    }  /* if */
    overload_sym->next_in_scope = other_sym->next_in_scope;
    overload_sym->prev_in_scope = other_sym->prev_in_scope;
    if (prev_sym_ptr == other_sym) {
        /* The entry is the first on the list.  This case is handled above. */
    } else {
       other_sym->prev_in_scope->next_in_scope = overload_sym;
    }  /* if */
    if (other_sym->next_in_scope != NULL) {
      other_sym->next_in_scope->prev_in_scope = overload_sym;
    }  /* if */
    other_sym->next_in_scope = NULL;
    other_sym->prev_in_scope = NULL;
    if (pointers_block->last_symbol == other_sym) {
      pointers_block->last_symbol = overload_sym;
    }  /* if */
    /* Attach the old symbol under the overloaded symbol. */
    overload_sym->variant.overloaded_function.symbols = other_sym;
    other_sym->overload_set_member = TRUE;
  }  /* if */
  /* Attach the new symbol to the front of the list under the overloaded
     symbol. */
  new_sym->next = overload_sym->variant.overloaded_function.symbols;
  overload_sym->variant.overloaded_function.symbols = new_sym;
  new_sym->overload_set_member = TRUE;
  /* Return a pointer to the sk_overloaded_function symbol. */
  return overload_sym;
}  /* add_symbol_to_overload_list */


a_symbol_ptr enter_overloaded_symbol(a_symbol_kind    sym_kind,
                                     a_symbol_locator *location,
                                     a_boolean        is_constructor,
                                     a_symbol_ptr     other_sym,
                                     a_symbol_ptr     *overload_sym)
/*
Enter a new symbol that is an overloading of the existing symbol other_sym.
other_sym may be either a simple function or an sk_overloaded_function.
The kind of symbol is sym_kind (some function kind).  *location gives
a locator for the new symbol.  Return a pointer to the new symbol.
*/
{
  a_symbol_ptr        sym_ptr;
  a_boolean	      use_namespace;
  a_namespace_ptr     ns_ptr = NULL;

  sym_ptr = alloc_symbol(sym_kind, location->symbol_header,
                         &location->source_position);
  sym_ptr->decl_scope = other_sym->decl_scope;
  /* Set the locator to point to the symbol entered. */
  location->specific_symbol = sym_ptr;
  location->is_qualified_name = FALSE;
  /* Check for the obscure case in which a non-constructor is being added
     to an overload set of constructors. */
  if (other_sym->is_class_member && !is_constructor &&
      is_constructor_symbol(other_sym)) {
    pos_error(ec_class_and_member_function_name_conflict,
              &location->source_position);
    set_to_error_locator(*location);
    sym_ptr->is_error = TRUE;
    *overload_sym = NULL;
  } else {
    use_namespace = sym_is_namespace_member(other_sym);
    if (use_namespace) ns_ptr = sym_parent_namespace(other_sym);
    /* Add the symbol to the overloaded function list. */
    *overload_sym = 
            add_symbol_to_overload_list(sym_ptr, other_sym, use_namespace,
                                        ns_ptr);
  }  /* if */
  /* Return a pointer to the newly created symbol as well. */
  return sym_ptr;
}  /* enter_overloaded_symbol */


static a_boolean check_for_deduction_guide_redeclaration(
                                                   a_symbol_ptr  new_guide,
                                                   a_symbol_ptr  guide_set);

static a_type_ptr deduction_guide_routine_type(a_symbol_ptr sym);


void add_deduction_guide(a_symbol_ptr  new_guide,
                         a_symbol_ptr  *p_guide_set)
/*
*p_guide_set represents a set of deduction guides (possibly NULL for an empty
set).  Add new_guide to this set.
*/
{
  a_symbol_ptr  guide_set = *p_guide_set;

  check_assertion(new_guide->decl_seq != NO_DECL_SEQUENCE_NUMBER);
  /* Diagnose a redeclaration when new_guide has the same type and equivalent
     requires-clauses as an existing user-declared guide in the set.  Do not
     add new_guide to the set if check_for_deduction_guide_redeclaration
     returns FALSE. */
  if (guide_set != NULL) {
    if (!check_for_deduction_guide_redeclaration(new_guide, guide_set)) {
      return;
    }  /* if */
  }  /* if */
  if (guide_set == NULL) {
    *p_guide_set = new_guide;
  } else if (symbol_is(guide_set, sk_overloaded_function)) {
    /* Add new_guide to the existing set. */
    new_guide->next = guide_set->variant.overloaded_function.symbols;
    guide_set->variant.overloaded_function.symbols = new_guide;
    new_guide->overload_set_member = TRUE;
  } else {
    /* Create an overload set. */
    a_symbol_ptr  old_guide = guide_set;
    guide_set = alloc_symbol((a_symbol_kind)sk_overloaded_function,
                             old_guide->header, &old_guide->decl_position);
    guide_set->decl_scope = old_guide->decl_scope;
    guide_set->decl_seq = old_guide->decl_seq;
    guide_set->potentially_overloaded = old_guide->potentially_overloaded;
    /* If the symbol is a member of a class or namespace, set the membership
       of the new symbol. */
    if (old_guide->is_class_member) {
      set_class_membership(guide_set, (a_source_correspondence *)NULL,
                           sym_parent_class(old_guide));
    } else if (sym_is_namespace_member(old_guide)) {
      set_namespace_membership(guide_set,  (a_source_correspondence *)NULL,
                               sym_parent_namespace(old_guide));
    }  /* if */
    new_guide->next = old_guide;
    guide_set->variant.overloaded_function.symbols = new_guide;
    *p_guide_set = guide_set;
  }  /* if */
}  /* add_deduction_guide */


void remove_deduction_guide(a_symbol_ptr  guide,
                            a_symbol_ptr  *p_guide_set)
/*
*p_guide_set represents a set of deduction guides, or possibly a pointer to
just one symbol (which then must to point to guide).  Remove guide from
the set.
*/
{
  a_symbol_ptr  guide_set = *p_guide_set;

  check_assertion(guide_set != NULL);
  if (guide_set == guide) {
    *p_guide_set = NULL;
  } else {
    check_assertion(symbol_is(guide_set, sk_overloaded_function));
    remove_symbol_from_overload_set(guide, guide_set);
  }  /* if */
}  /* remove_deduction_guide */


a_requires_clause_ptr function_template_head_requires_clause(a_symbol_ptr sym)
/*
Return the template-head requires-clause for sym if it is a function
template; otherwise return NULL.
*/
{
  a_requires_clause_ptr             rcp = NULL;
  a_template_symbol_supplement_ptr  tssp;
  a_template_decl_ptr               tdp = NULL;

  reduce_projection_symbol_to_fundamental_symbol(sym);
  if (!symbol_is(sym, sk_function_template)) {
    goto done;
  }  /* if */
  tssp = sym->variant.template_info;
  if (tssp->il_template_entry != NULL) {
    tdp = tssp->il_template_entry->template_decl;
  }  /* if */
  if (tdp == NULL &&
      tssp->variant.function.decl_cache != NULL &&
      tssp->variant.function.decl_cache->decl_info != NULL) {
    tdp = tssp->variant.function.decl_cache->decl_info->template_decl;
  }  /* if */
  if (tdp != NULL) {
    rcp = if_microsoft_extensions(tdp->is_generic ? NULL : )
          tdp->constraint.requires_clause;
  }  /* if */
done:
  return rcp;
}  /* function_template_head_requires_clause */


static a_boolean deduction_guides_are_redeclarations(a_symbol_ptr  sym1,
                                                     a_symbol_ptr  sym2)
/*
Return TRUE if sym1 and sym2, two user-declared deduction guides, have
the same type and equivalent requires-clauses (template-head and trailing).
*/
{
  a_type_ptr             type1, type2;
  a_routine_ptr          rp1, rp2;
  a_requires_clause_ptr  head_rcp1, head_rcp2;
  a_boolean              result = FALSE;

  type1 = function_or_template_symbol_type(sym1);
  type2 = function_or_template_symbol_type(sym2);
  if (identical_types(type1, type2)) {
    rp1 = func_sym_routine(sym1);
    rp2 = func_sym_routine(sym2);
    if (equiv_requires_clauses(trailing_requires_clause(rp1),
                               trailing_requires_clause(rp2))) {
      head_rcp1 = function_template_head_requires_clause(sym1);
      head_rcp2 = function_template_head_requires_clause(sym2);
      if (equiv_requires_clauses(head_rcp1, head_rcp2)) {
        result = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* deduction_guides_are_redeclarations */


static a_boolean deduction_guide_redeclaration_is_cross_context(
                                                   a_symbol_ptr  sym1,
                                                   a_symbol_ptr  sym2)
/*
Return TRUE if sym1 and sym2 are duplicate deduction guides introduced
in different contexts (i.e., in different modules, or one in a module
and one not), in which case the redeclaration should be permitted.

Use module_for_symbol (not lookup_module_for_symbol) to identify the module
that introduced a guide: Exported module entities are globally visible, so
lookup_module_for_symbol returns NULL for them, which would not distinguish
an exported module guide from a source guide.
*/
{
  a_symbol_ptr   fund_sym1 = sym1, fund_sym2 = sym2;
  a_boolean      result = FALSE;

  reduce_projection_symbol_to_fundamental_symbol(fund_sym1);
  reduce_projection_symbol_to_fundamental_symbol(fund_sym2);
  if (fund_sym1->from_module_code != fund_sym2->from_module_code) {
    result = TRUE;
  } else if (fund_sym1->from_module_code) {
    a_module_ptr mod1 = skip_module_partitions(module_for_symbol(fund_sym1));
    a_module_ptr mod2 = skip_module_partitions(module_for_symbol(fund_sym2));

    /* Treat an unknown module identity as a distinct context. */
    if (mod1 != mod2 || mod1 == NULL) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* deduction_guide_redeclaration_is_cross_context */


static a_type_ptr deduction_guide_routine_type(a_symbol_ptr sym)
/*
Return the routine type of sym, a deduction guide represented as either a
simple function or a function template.  Unlike
function_or_template_symbol_type, typerefs are not stripped from the type.
*/
{
  a_type_ptr rout_type = NULL;

  reduce_projection_symbol_to_fundamental_symbol(sym);
  if (is_simple_function_symbol(sym)) {
    rout_type = routine_symbol_type(sym);
  } else if (sym->kind == (a_symbol_kind)sk_function_template) {
    rout_type = sym->variant.template_info->variant.function.routine->type;
  } else {
    unexpected_condition();
  }  /* if */
  return rout_type;
}  /* deduction_guide_routine_type */


a_type_ptr function_or_template_symbol_type(a_symbol_ptr sym)
/*
Return the type of a function, whether a simple function or a function
template.
*/
{
  a_type_ptr rout_type = NULL;

  reduce_projection_symbol_to_fundamental_symbol(sym);
  if (is_simple_function_symbol(sym)) {
    rout_type = routine_symbol_type(sym);
  } else if (sym->kind == (a_symbol_kind)sk_function_template) {
    rout_type = sym->variant.template_info->variant.function.routine->type;
  } else {
    unexpected_condition();
  }  /* if */
  rout_type = skip_typerefs(rout_type);
  return rout_type;
}  /* function_or_template_symbol_type */


static a_boolean check_for_deduction_guide_redeclaration(
                                                   a_symbol_ptr  new_guide,
                                                   a_symbol_ptr  guide_set)
/*
If new_guide has the same type and equivalent requires-clauses as an
existing user-declared deduction guide in guide_set, issue an error unless
the redeclaration is permitted because the guides were introduced in
different contexts.  Return FALSE if new_guide should not be added to
guide_set (because an error was issued or because a permitted cross-context
redeclaration was detected).  Return TRUE otherwise.  Ill-formed guides
(with invalid return types) are ignored.
*/
{
  a_routine_ptr  new_rp = func_sym_routine(new_guide);
  a_symbol_ptr   ct_sym = symbol_for(new_rp->variant.class_template);
  a_type_ptr     new_type, rout_type;
  a_boolean      result = TRUE;

  if (new_rp->compiler_generated || deduction_guide_redeclaration_allowed) {
    goto done;
  }  /* if */
  rout_type = deduction_guide_routine_type(new_guide);
  if (!deduction_guide_return_type_is_strictly_valid(rout_type, ct_sym)) {
    goto done;
  }  /* if */
  new_type = function_or_template_symbol_type(new_guide);
  if (symbol_is(guide_set, sk_overloaded_function)) {
    a_symbol_ptr  sym;

    for (sym = guide_set->variant.overloaded_function.symbols;
         sym != NULL; sym = sym->next) {
      a_routine_ptr  rp = func_sym_routine(sym);
      a_type_ptr     existing_rout_type = deduction_guide_routine_type(sym);

      if (!rp->compiler_generated &&
          deduction_guide_return_type_is_strictly_valid(existing_rout_type,
                                                        ct_sym) &&
          deduction_guides_are_redeclarations(new_guide, sym)) {
        if (deduction_guide_redeclaration_is_cross_context(new_guide, sym)) {
          result = FALSE;
          goto done;
        }  /* if */
        pos_error(ec_deduction_guide_redeclaration, &new_guide->decl_position,
                  new_type, &sym->decl_position);
        result = FALSE;
        goto done;
      }  /* if */
    }  /* for */
  } else {
    a_routine_ptr  rp = func_sym_routine(guide_set);
    a_type_ptr     existing_rout_type =
                                   deduction_guide_routine_type(guide_set);

    if (!rp->compiler_generated &&
        deduction_guide_return_type_is_strictly_valid(existing_rout_type,
                                                      ct_sym) &&
        deduction_guides_are_redeclarations(new_guide, guide_set)) {
      if (deduction_guide_redeclaration_is_cross_context(new_guide,
                                                       guide_set)) {
        result = FALSE;
      } else {
        pos_error(ec_deduction_guide_redeclaration, &new_guide->decl_position,
                  new_type, &guide_set->decl_position);
        result = FALSE;
      }  /* if */
    }  /* if */
  }  /* if */
done:
  return result;
}  /* check_for_deduction_guide_redeclaration */

#if MICROSOFT_EXTENSIONS_ALLOWED

a_symbol_ptr enter_property_set_member(a_symbol_locator               *loc,
                                       a_scope_depth                  depth,
                                       a_property_or_event_descr_ptr  pedp,
                                       a_symbol_ptr                   *set_sym)
/*
pedp describes a C++/CLI property that is being declared at the given scope
depth (which must correspond to a sck_class_struct_union scope).  loc is the
locator for the declaration.  Create and return an sk_field or
sk_static_data_member symbol (the latter for static properties) for this
declaration.  That symbol is placed under an sk_property_set symbol (which
might be pre-existing or might be created for this particular call) entered
in the class scope described by depth; the sk_property_set symbol is returned
in *set_sym.  Issue errors if a conflict with a previous non-property
declaration arises.
*/
{
  a_type_ptr     class_type;
  a_symbol_kind  member_kind;
  a_symbol_ptr   member_sym, *p_member_sym;

  /* First retrieve or create a sk_property_set symbol for the property. */
  check_assertion(
             scope_stack[depth].kind == (a_scope_kind)sck_class_struct_union);
  class_type = scope_stack[depth].assoc_type;
  *set_sym = class_qualified_id_lookup(loc, class_type,
                                       IDL_DIRECT_CLASS_MEMBERS_ONLY);
  if (*set_sym == NULL || !symbol_is(*set_sym, sk_property_set)) {
    /* No property with this name has been declared yet in this class. */
    *set_sym = enter_symbol((a_symbol_kind)sk_property_set, loc, depth,
                            /*suppress_error=*/FALSE);
    set_class_membership(*set_sym, (a_source_correspondence*)NULL, class_type);
  }  /* if */
  /* Next, create the field or static data member symbol for the given
     property. */
  if (pedp->is_static) {
    member_kind = (a_symbol_kind)sk_static_data_member;
  } else {
    member_kind = (a_symbol_kind)sk_field;
  }  /* if */
  member_sym = alloc_symbol(member_kind, loc->symbol_header,
                            &loc->source_position);
  member_sym->decl_scope = scope_stack[depth].number;
  /* Finally, link the property symbol under the property set symbol. */
  p_member_sym = &(*set_sym)->variant.property_info->properties;
  while (*p_member_sym != NULL) p_member_sym = &(*p_member_sym)->next;
  *p_member_sym = member_sym;
  return member_sym;
}  /* enter_property_set_member */


static a_symbol_ptr enter_cli_property_accessor(
                                a_symbol_locator               *locator,
                                a_scope_depth                  depth,
                                a_symbol_ptr                   set_sym)
/*
Enter and return a symbol for an accessor member function of a C++/CLI
property.  The given locator describes the name of the accessor, set_sym
points to the associated property set, and depth is the scope stack depth
corresponding to the enclosing class definition.
*/
{
  a_symbol_ptr  result, *p_accessor_sym;

  if (strcmp(locator->symbol_header->identifier, "get") == 0) {
    p_accessor_sym = &set_sym->variant.property_info->get_accessors;
  } else {
    p_accessor_sym = &set_sym->variant.property_info->set_accessors;
  }  /* if */
  if (*p_accessor_sym == NULL) {
    /* This is the first accessor of this kind in the set. */
    *p_accessor_sym = result =
                enter_local_symbol((a_symbol_kind)sk_member_function, locator,
                                   depth, /*suppress_redecl_error=*/TRUE);
  } else {
    /* At least one accessor of this kind is already present in the set.
       Add another one and ensure the overload set symbol is marked invisible.
       The accessor symbol itself will be marked invisible by the caller. */
    result = enter_overloaded_symbol((a_symbol_kind)sk_member_function,
                                     locator, /*is_constructor=*/FALSE,
                                     *p_accessor_sym, p_accessor_sym);
    (*p_accessor_sym)->is_invisible = TRUE;
  }  /* if */
  return result;
}  /* enter_cli_property_accessor */


a_symbol_ptr enter_cli_accessor(a_symbol_locator               *locator,
                                a_scope_depth                  depth,
                                a_property_or_event_descr_ptr  pedp)
/*
Enter and return a symbol for an accessor member function for a C++/CLI
property or event.  The given locator describes the name of the accessor, pedp
describes the property or event, and depth is the scope stack depth
corresponding to the enclosing class definition.
*/
{
  a_symbol_ptr    sym = NULL;
  a_symbol_ptr    prop_sym = NULL;

  check_assertion(
             scope_stack[depth].kind == (a_scope_kind)sck_class_struct_union);
  if (locator->is_error) {
    /* Don't try to get a property or event header for an error locator as
       there may not be a symbol header pointer. */
    set_to_named_error_locator(*locator);
    sym = enter_local_symbol((a_symbol_kind)sk_member_function, locator,
                             depth, /*suppress_redecl_error=*/TRUE);
  } else {
    /* Get the symbol for the property or event. */
    prop_sym = pedp->is_static ? symbol_for(pedp->variant.variable)
                               : symbol_for(pedp->variant.field);
    /* Translate the symbol header from one that represents just the
       accessor kind (e.g., "get") into one that represents both the
       property name and the accessor kind.  The new header has the
       same identifier string as the original accessor, but because
       they have distinct symbol headers that are not in the symbol
       header lookup table, they are not found by normal lookup of an
       identifier such as "get". */
    locator->symbol_header = get_property_or_event_accessor_symbol_header(
                                     prop_sym->header,
                                     locator->symbol_header);
  }  /* if */
  if (locator->is_error) {
    /* This case was handled above. */
  } else if (pedp->kind == (a_property_or_event_kind)pek_cli_event) {
    /* C++/CLI events cannot be overloaded.  Their accessors are entered as
       member functions. */
    sym = enter_local_symbol((a_symbol_kind)sk_member_function, locator,
                             depth, /*suppress_redecl_error=*/TRUE);
  } else if (pedp->kind == (a_property_or_event_kind)pek_cli_property) {
    /* A C++/CLI property accessor.  Since properties can be overloaded, we
       keep track of accessor overload sets for each property set. */
    /* Find the property set for this accessor. */
    a_symbol_locator  set_loc;
    a_symbol_ptr      set_sym;
    a_type_ptr        class_type;
    class_type = scope_stack[depth].assoc_type;
    make_locator_for_symbol(prop_sym, &set_loc);
    clear_specific_symbol(set_loc);
    set_sym = class_qualified_id_lookup(&set_loc, class_type,
                                        IDL_DIRECT_CLASS_MEMBERS_ONLY);
    if (set_sym == NULL || !symbol_is(set_sym, sk_property_set)) {
      /* No property set was found.  An error must have occurred earlier. */
      expect_error();
      set_to_named_error_locator(*locator);
      sym = enter_local_symbol((a_symbol_kind)sk_member_function, locator,
                               depth, /*suppress_redecl_error=*/TRUE);
    } else {
      sym = enter_cli_property_accessor(locator, depth, set_sym);
    }  /* if */
  } else {
    unexpected_condition();
  }  /* if */
  return sym;
}  /* enter_cli_accessor */


void enter_projected_default_indexed_properties(
                                          a_class_symbol_supplement_ptr  cssp)
/*
cssp is for a class being defined whose scope was just pushed.  If any
default indexed properties were projected in the class (by a call to
inherit_default_indexed_properties), enter them in the symbol table now.
*/
{
  if (cssp->default_indexed_properties != NULL) {
    a_property_set_symbol_supplement_ptr
       property_set = cssp->default_indexed_properties->variant.property_info;
    add_symbol_to_symbol_table(cssp->default_indexed_properties,
                               depth_scope_stack, /*suppress_error=*/TRUE);
    if (property_set->get_accessors != NULL) {
      add_symbol_to_symbol_table(property_set->get_accessors,
                                 depth_scope_stack, /*suppress_error=*/TRUE);
    }  /* if */
    if (property_set->set_accessors != NULL) {
      add_symbol_to_symbol_table(property_set->set_accessors,
                                 depth_scope_stack, /*suppress_error=*/TRUE);
    }  /* if */
  }  /* if */
}  /* enter_projected_default_indexed_properties */


a_boolean is_cli_param_array_routine_symbol(a_symbol_ptr  sym)
/*
Return TRUE if sym is a symbol for a routine with a C++/CLI parameter array.
The symbol must be a function or function template symbol.
*/
{
  a_type_ptr rout_type = function_or_template_symbol_type(sym);

  return rout_type != NULL && is_cli_param_array_routine_type(rout_type);
}  /* is_cli_param_array_routine_symbol */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#define ctor_is_trivial(sym)                                          \
  (symbol_is(sym, sk_member_function) &&                              \
   ((sym)->variant.routine.ptr->is_trivial_default_constructor ||     \
    (sym)->variant.routine.ptr->is_trivial_copy_function))

a_boolean f_has_nontrivial_ctor(a_class_symbol_supplement_ptr  cssp)
/*
Return TRUE if any of the constructors associated with cssp is nontrivial.
*/
{
  a_boolean     result = FALSE;
  a_symbol_ptr  sym = cssp->constructor;

  if (sym) {
    if (symbol_is(sym, sk_overloaded_function)) {
      /* An overloaded set of constructors: Check each one in turn. */
      sym = cssp->constructor->variant.overloaded_function.symbols;
      for (; sym != NULL; sym = sym->next) {
        if (!ctor_is_trivial(sym)) {
          result = TRUE;
          break;
        }  /* if */
      }  /* for */
    } else if (!ctor_is_trivial(sym)) {
      /* A single constructor and it is nontrivial. */
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* f_has_nontrivial_ctor */


a_base_class_ptr find_base_with_type(a_type_ptr        base_type,
                                     a_type_ptr        class_type,
                                     a_base_class_ptr  ref_bcp)
/*
Determine the base class of class_type whose type it base_type and whose
derivation path to class_type goes through ref_bcp.
*/
{
  a_base_class_ptr  result = NULL, bcp = base_classes_of(class_type);

  check_assertion(ref_bcp->direct || ref_bcp->is_virtual);
  for (; bcp != NULL; bcp = bcp->next) {
    if (same_entities(bcp->type, base_type)) {
      /* ref_bcp is the root of a path to the base class where the inherited
         name was found.  Be sure ref_bcp is also on the path to bcp before
         deciding that bcp is the base class containing sym.  Note that if the
         inheritance is ambiguous there may be several base classes that match
         the type in question, and there may be more than one for which
         ref_bcp is on the path. */
      if (!bcp->ambiguous || ref_bcp == bcp ||
          is_on_any_derivation_of(bcp, ref_bcp)) {
        result = bcp;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return result;
}


a_symbol_ptr make_projection_symbol(a_symbol_ptr      progenitor_sym,
                                    a_type_ptr        class_ptr,
                                    a_base_class_ptr  fundamental_bcp,
                                    a_derivation_step *path,
                                    a_boolean         ambiguous)
/*
Create a new projection symbol entry and return a pointer to it.  The symbol
is a projection of progenitor_sym into the current scope.  The symbol is not
added to the scope symbols list and is not linked into the symbol table.
fundamental_bcp is a pointer to the base class of class_ptr in which the
fundamental symbol resides; if it is NULL, it must be computed, using *path
(which describes the derivation of *class_ptr from the class of which
progenitor_sym is a member) if ambiguous is TRUE.
*/
{
  a_symbol_ptr            sym;
  a_projection_descr_ptr  pdp, progenitor_pdp = NULL;
  a_base_class_ptr        bcp;
  a_scope_ptr             scope = class_type_supp(class_ptr)->assoc_scope;

  db_enter(4, "make_projection_symbol");

  /* Allocate and initialize the symbol. */
  sym = alloc_symbol((a_symbol_kind)sk_projection, progenitor_sym->header,
                     &progenitor_sym->decl_position);
  set_class_membership(sym, (a_source_correspondence *)NULL, class_ptr);
  if (!scope_is_null_or_placeholder(scope)) sym->decl_scope = scope->number;
  sym->ambiguous = ambiguous;
  pdp = sym->variant.projection.extra_info;
  if (progenitor_sym->kind == (a_symbol_kind)sk_projection) {
    /* The "progenitor" of this new projection symbol is itself a projection
       symbol. */
    progenitor_pdp = progenitor_sym->variant.projection.extra_info;
    pdp->fundamental_symbol = progenitor_pdp->fundamental_symbol;
    /* Set the flag indicating whether there are any intervening access
       declarations in the inheritance path. */
    if (progenitor_sym->variant.projection.is_using_decl ||
        progenitor_sym->variant.projection.any_intervening_using_decl) {
      sym->variant.projection.any_intervening_using_decl = TRUE;
    }  /* if */
  } else {
    pdp->fundamental_symbol = progenitor_sym;
  }  /* if */
  if (fundamental_bcp != NULL) {
    /* The caller has supplied the fundamental base class. */
    pdp->fundamental_base_class = fundamental_bcp;
  } else {
    /* To set the fundamental_base_class pointer in the projection descriptor
       for sym, we have to look through the base symbols for the current class.
       The base class with which the fundamental symbol is associated is the
       one we want. */
    a_type_ptr  tp = sym_parent_class(pdp->fundamental_symbol);
    if (!ambiguous) {
      /* There is no ambiguity in the use of this name, so a simple type match
         may be enough to identify the base class of the fundamental symbol.
         (In some cases -- like static members -- the progenitor symbol itself
         may not be considered ambiguous, but the base class associated with
         it may be.  In such cases, the base class must still be selected based
         on the derivation path, because one path may grant more access than
         the other.) */
      bcp = class_ptr->variant.class_struct_union.extra_info->base_classes;
      for (; bcp != NULL; bcp = bcp->next) {
        if (same_entities(bcp->type, tp)) {
          pdp->fundamental_base_class = bcp;
          ambiguous = bcp->ambiguous;
          break;
        }  /* if */
      }  /* for */
    }  /* if */
    if (!ambiguous) {
      /* We're done finding the fundamental base class. */
#if CHECKING
    } else if (path == NULL) {
      unexpected_condition();
#endif /* CHECKING */
    } else {
      /* When there is an ambiguity, we must check the paths as well as the
         type. */
      a_base_class_ptr  ref_bcp = path->base_class;

      if (!same_entities(ref_bcp->derived_class, class_ptr)) {
        ref_bcp = corresponding_base_class(ref_bcp, class_ptr,
                                           (a_base_class_ptr)NULL);
      }  /* if */
      check_assertion(ref_bcp->direct || ref_bcp->is_virtual);
      pdp->fundamental_base_class = find_base_with_type(tp, class_ptr,
                                                        ref_bcp);
    }  /* if */
#if CHECKING
    if (pdp->fundamental_base_class == NULL) {
      internal_error("make_projection_symbol: no fundamental base class");
    }  /* if */
#endif /* CHECKING */
  }  /* if */
  db_exit();
  return sym;
}  /* make_projection_symbol */

using a_type_name_string = Small_string<50>;
                        /* The type of a buffer storing a type name represented
                           as a string. */

static a_type_name_string operator_type_name_str(a_type_ptr tp)
/*
Return a string containing the type pointed to by tp.  The formatted type is
stripped of some type alias information (see
an_il_to_str_output_control_block::keep_template_typedefs for more
information).
*/
{
  a_type_name_string
                result;
  an_il_to_str_output_control_block
                local_octl;

  /* Set up for use of the il_to_str routines. */
  clear_il_to_str_output_control_block(&local_octl);

  auto output_str_func = [](a_const_char                          *str,
                            an_il_to_str_output_control_block_ptr octl_ptr) {
    ((a_type_name_string*)octl_ptr->text_buffer)->append(str);
  };
  local_octl.output_str = output_str_func;
  local_octl.text_buffer = (void*)&result;
  local_octl.keep_template_typedefs = FALSE;
  form_type(tp, &local_octl);
  return result;
}  /* operator_type_name_str */


static
a_symbol_header_ptr symbol_header_for_conversion_function(a_type_ptr type)
/*
Look up the symbol header for a given conversion function.  If there
is none, create a new one.
*/
{
  a_conversion_header_ptr  conv_hdr;
  a_conversion_header_ptr  prev_conv_hdr;
  a_symbol_header_ptr      sym_hdr;

  /* Search the conversion header list for an entry of the required type.
     If one is found, it is moved to the front of the list. */
  prev_conv_hdr = NULL;
  conv_hdr = conversion_header_list;
  for (; conv_hdr != NULL; conv_hdr = conv_hdr->next) {
    if (types_are_strictly_compatible(type, conv_hdr->type,
                                      TCF_CHECK_ENABLE_IF_ATTRIBUTES)) {
      /* Found it.  Move it to the front of the list. */
      if (prev_conv_hdr != NULL) {
        prev_conv_hdr->next = conv_hdr->next;
        conv_hdr->next = conversion_header_list;
        conversion_header_list = conv_hdr;
      }  /* if */
      break;
    }  /* if */
    prev_conv_hdr = conv_hdr;
  }  /* if */
  /* conv_hdr is NULL if no entry already exists on the list for the
     specified type. */
  if (conv_hdr == NULL) {
    /* Create a new conversion header entry and add it to the front of
       the list. */
    conv_hdr = alloc_conversion_header();
    conv_hdr->next = conversion_header_list;
    conversion_header_list = conv_hdr;
    /* Set the type and symbol header. */
    conv_hdr->type = type;
    conv_hdr->symbol_header = sym_hdr = alloc_symbol_header();

    /* Conversion symbols have the name "operator <type-name>". */
    a_type_name_string name = operator_type_name_str(type);
    sym_hdr->identifier_length =
                    LENGTH_CANONICAL_CONVERSION_FUNCTION_INTRO + name.length();
    sym_hdr->identifier =
                  alloc_primary_file_scope_il(sym_hdr->identifier_length + 1);
    (void)memcpy((char *)sym_hdr->identifier,
                 CANONICAL_CONVERSION_FUNCTION_INTRO,
                 LENGTH_CANONICAL_CONVERSION_FUNCTION_INTRO);
    (void)memcpy((char *)sym_hdr->identifier +
                                   LENGTH_CANONICAL_CONVERSION_FUNCTION_INTRO,
                 name.as_temp_characters(),
                 name.length());
    ((char*)sym_hdr->identifier)[sym_hdr->identifier_length] = '\0';
#if DEBUG
/* GCOVR_EXCL_START */
    symbol_name_string_space += (unsigned long)(sym_hdr->identifier_length);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  }  /* if */
  return conv_hdr->symbol_header;
}  /* symbol_header_for_conversion_function */


static a_symbol_ptr make_parameter_symbol(a_symbol_locator  *locator)
/*
Create but do not yet enter an sk_parameter symbol.  This routine is called
for old style parameter declarations.
*/
{
  a_symbol_ptr  sym;

  sym = alloc_symbol((a_symbol_kind)sk_parameter, locator->symbol_header,
                     &locator->source_position);
  /* Set the locator to point to the symbol entered. */
  locator->specific_symbol = sym;
  locator->is_qualified_name = FALSE;

  return sym;
}  /* make_parameter_symbol */


a_symbol_ptr make_template_variable_symbol(a_symbol_ptr  templ_sym)
/*
Create a symbol for an instance of a variable template.  Link the symbol to
the variable template symbol but do not enter it into the symbol table.
templ_sym is the symbol of the variable template.
*/
{
  a_symbol_ptr 				sym;

  /* Create the symbol.  Use the position of the template declaration as its
     declaration position. */
  sym = alloc_symbol((a_symbol_kind)sk_variable, templ_sym->header,
                     &templ_sym->decl_position);
  /* Make the declaration scope the same as the class template's. */
  sym->decl_scope = templ_sym->decl_scope;
  /* Set the new symbol to have the same class or namespace membership as
     the template from which it was created. */
  if (templ_sym->is_class_member) {
    set_class_membership(sym, (a_source_correspondence *)NULL,
                         sym_parent_class(templ_sym));
  } else if (sym_is_namespace_member(templ_sym)) {
    set_namespace_membership(sym, (a_source_correspondence *)NULL,
                             sym_parent_namespace(templ_sym));
  }  /* if */
  return sym;
}  /* make_template_variable_symbol */


a_symbol_ptr make_template_class_symbol(a_symbol_ptr  ct_symbol)
/*
Create a symbol for an instance of a class template.  Link the symbol to
the class template symbol but do not enter it into the symbol table.
ct_symbol is the symbol of the class template.  This is also used to
create the instance symbols for template aliases.
*/
{
  a_symbol_ptr 				sym;
  a_symbol_kind 			kind = (a_symbol_kind)sk_last;
  a_class_symbol_supplement_ptr		cssp;
  a_boolean				is_alias_template;
  a_template_symbol_supplement_ptr	tssp;

  tssp = ct_symbol->variant.template_info;
  is_alias_template = tssp->variant.class_template.is_alias_template;
  if (is_alias_template) {
    kind = (a_symbol_kind)sk_type;
  } else {
    /* Determine kind of symbol to be entered.  It can be either a
       class_or_struct or a union depending on the type of the class
       template. */
    switch (tssp->variant.class_template.type_kind) {
      case tk_class:
      case tk_struct:  kind = (a_symbol_kind)sk_class_or_struct_tag;  break;
      case tk_union:   kind = (a_symbol_kind)sk_union_tag;            break;
      default:
        unexpected_condition_str("make_template_class_symbol: bad type kind");
    }  /* switch */
  }  /* if */
  /* Create the symbol.  Use the position of the template declaration as its
     declaration position. */
  sym = alloc_symbol(kind, ct_symbol->header, &ct_symbol->decl_position);
  if (!is_alias_template) {
    /* Set the pointer that points back to the original class template
       symbol. */
    cssp = sym->variant.class_struct_union.extra_info;
    cssp->class_template = ct_symbol;
  }  /* if */
  /* If the class template symbol is an error symbol, the instance
     is also implicitly in an error state. */
  if (ct_symbol->is_error) {
    sym->is_error = TRUE;
  }  /* if */
  /* Make the declaration scope the same as the class template's. */
  sym->decl_scope = ct_symbol->decl_scope;
  /* Set the new symbol to have the same class or namespace membership as
     the template from which it was created. */
  if (ct_symbol->is_class_member) {
    set_class_membership(sym, (a_source_correspondence *)NULL,
                         sym_parent_class(ct_symbol));
  } else if (sym_is_namespace_member(ct_symbol)) {
    set_namespace_membership(sym, (a_source_correspondence *)NULL,
                             sym_parent_namespace(ct_symbol));
  }  /* if */
  return sym;
}  /* make_template_class_symbol */


a_symbol_ptr make_function_template_prototype_symbol(
				a_symbol_ptr		template_sym,
				a_routine_ptr		rout_ptr,
				a_template_param_ptr	templ_param_list)
/*
Create the symbol for the prototype instantiation of a function template.
Return the newly created symbol.  template_sym points to the function
template symbol.  rout_ptr points to the routine entry for the prototype
instantiation.
*/
{
  a_symbol_kind			kind;
  a_symbol_ptr			sym;
  a_template_instance_ptr	tip;
  a_template_symbol_supplement_ptr
				tssp;
  a_boolean			is_generic;

  tssp = template_supplement_for_symbol(template_sym);
  is_generic = tssp->is_generic;
  /* If the template is a class member make the prototype instantiation
     a member function, otherwise make it a normal routine. */
  kind = template_sym->is_class_member ? (a_symbol_kind)sk_member_function
                                       : (a_symbol_kind)sk_routine;
  sym = alloc_symbol(kind, template_sym->header, &template_sym->decl_position);
  tip = alloc_template_instance();
  tip->template_sym = template_sym;
  tip->instance_sym = sym;
  tip->template_info = tssp;
  /* If the function template prototype symbol is an error symbol, the function
     prototype symbol is also implicitly in an error state. */
  if (template_sym->is_error) {
    sym->is_error = TRUE;
  }  /* if */
  sym->decl_scope = template_sym->decl_scope;
  sym->variant.routine.instance_ptr = tip;
  sym->variant.routine.ptr = rout_ptr;
  sym->is_class_member = template_sym->is_class_member;
  sym->parent = template_sym->parent;
  /* Create the template argument list for the prototype routine. */
  rout_ptr->template_arg_list = create_prototype_arg_list(
                                                template_sym, templ_param_list,
                                                /*add_pack_descr=*/FALSE);
  rout_ptr->is_prototype_instantiation = !is_generic;
  rout_ptr->is_template_function = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  rout_ptr->is_generic_definition = is_generic;
  rout_ptr->is_generic_instance = is_generic;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  return sym;
}  /* make_function_template_prototype_symbol */


a_symbol_ptr make_template_function_symbol(a_symbol_ptr       templ_sym,
                                           a_source_position  *pos,
                                           a_type_ptr         conv_type)
/*
Create a symbol for a template function.  Do not enter it into the symbol
table, since it is accessed from the associated function instantiation entry.
conv_type is the return type of the function being created.  When the
function is a conversion function, conv_type is used to generate the name
of the instance symbol.  For example, the template may be called "operator T",
but the instance needs to be called "operator int".
*/
{
  a_symbol_ptr  	sym;
  a_symbol_header_ptr	sym_hdr;

  /* Determine which symbol header should be used for this symbol.  This
     is usually the same symbol header as the template.  But for conversion
     operators, a new name must be generated based on the type. */
  if (is_conversion_function_symbol(templ_sym)) {
    sym_hdr = symbol_header_for_conversion_function(conv_type);
  } else {
    sym_hdr = templ_sym->header;
  }  /* if */
  sym = alloc_symbol((a_symbol_kind) (templ_sym->is_class_member
                                          ? (a_symbol_kind)sk_member_function
                                          : (a_symbol_kind)sk_routine),
                     sym_hdr, pos);
  /* Template functions will be in the same scope as the template (which
     should always be the file scope. */
  sym->decl_scope = templ_sym->decl_scope;;
  /* Set the new symbol to have the same class or namespace membership as
     the template from which it was created. */
  if (templ_sym->is_class_member) {
    set_class_membership(sym, (a_source_correspondence *)NULL,
                         sym_parent_class(templ_sym));
  } else if (sym_is_namespace_member(templ_sym)) {
    set_namespace_membership(sym, (a_source_correspondence *)NULL,
                             sym_parent_namespace(templ_sym));
  }  /* if */
  return sym;
}  /* make_template_function_symbol */


a_symbol_ptr error_class_template(void)
/*
Return a pointer to an error class template.
*/
{
  if (error_class_template_symbol == NULL) {
    /* Create a class template symbol for this template template parameter. */
    a_symbol_ptr			sym;
    a_template_symbol_supplement_ptr	tssp;
    a_template_ptr			templ_ptr;
    sym = alloc_symbol((a_symbol_kind)sk_class_template,
                       (a_symbol_header_ptr)NULL, &null_source_position);
    sym->is_error = TRUE;
    sym->is_template_param = TRUE;
    tssp = sym->variant.template_info;
    templ_ptr = alloc_template();
    templ_ptr->template_info = tssp;
    set_source_corresp(&templ_ptr->source_corresp, sym);
    templ_ptr->kind = (a_template_kind)templk_template_template_param;
    /* Note that this is not marked as a nonreal member.  In this way,
       instances based on an error class template will not necessarily
       be nonreal. */
    tssp->variant.class_template.type_kind = (a_type_kind)tk_class;
    tssp->il_template_entry = templ_ptr;
    tssp->is_error = TRUE;
    {
      /* Create a template_decl_info entry for the error template for
         error recovery purposes. */
      a_template_decl_info_ptr template_decl_info = alloc_template_decl_info();

      tssp->cache->decl_info = template_decl_info;
    }
    error_class_template_symbol = sym;
  }  /* if */
  return error_class_template_symbol;
}  /* error_class_template */


a_template_symbol_supplement_ptr template_supplement_for_template(
						a_template_ptr	templ_ptr)
/*
Given an IL template entry, return the template symbols supplement.  If
the template entry has a template_info field, use that.  Otherwise, get
the template supplement information from the associated symbol.  Nonreal
templates and template template parameter templates have a template_info
field.
*/
{
  a_symbol_ptr				sym;
  a_template_symbol_supplement_ptr	tssp;

  tssp = templ_ptr->template_info;
  if (tssp == NULL) {
    sym = (a_symbol_ptr)templ_ptr->source_corresp.assoc_info;
    tssp = template_supplement_for_symbol(sym);
  }  /* if */
  return tssp;
}  /* template_supplement_for_template */


a_symbol_ptr get_member_function_template_symbol(a_symbol_ptr  rout_sym)
/*
rout_sym is a symbol representing a member function of a prototype
instantiation of a template class.  If there is already a function template
symbol associated with rout_sym, return it.  Otherwise, allocate and
initialize a function instantiation entry and a function template symbol
and attach them to rout_sym, and return the function template symbol.
*/
{
  a_symbol_ptr             template_sym;
  a_template_instance_ptr  tip;

#if CHECKING
  { a_type_ptr	parent_type = sym_parent_class(rout_sym);
    if (!parent_type->variant.class_struct_union.is_nonreal_class &&
        /*lint -e(506)*/!is_cli_generic_definition_type(parent_type)) {
      internal_error("make_member_function_template_symbol: bad class member");
    }  /* if */
  }
#endif /* CHECKING */
  tip = rout_sym->variant.routine.instance_ptr;
  if (tip != NULL) {
    template_sym = tip->template_sym;
  } else {
    /* Note that the function template symbol is not entered in the symbol
       table, since it need only be accessed through the corresponding
       member function symbol rout_sym. */
    template_sym = alloc_symbol((a_symbol_kind)sk_function_template,
                                rout_sym->header, &rout_sym->decl_position);
    template_sym->is_class_member = TRUE;
    template_sym->parent.class_type = sym_parent_class(rout_sym);
    template_sym->variant.template_info->variant.function.routine =
                                            rout_sym->variant.routine.ptr;
    /* Create the associated function instantiation entry, but do not link it
       onto the instantiation list for the template. */
    tip = alloc_template_instance();
    tip->template_sym = template_sym;
    /* Make the function instantiation entry and its associated symbol
       point at each other. */
    tip->instance_sym = rout_sym;
    rout_sym->variant.routine.instance_ptr = tip;
  }  /* if */
  return template_sym;
}  /* get_member_function_template_symbol */


a_symbol_ptr make_template_param_object_sym(a_source_position  *pos)
/*
Create a symbol for a template parameter object symbol.  Do not enter it into
the symbol table.
*/
{
  a_symbol_ptr  sym;

  /* Use the unnamed tag symbol header.  Allocate it if necessary. */
  if (template_param_object_symbol_header == NULL) {
    template_param_object_symbol_header = alloc_symbol_header();
    set_identifier_for_symbol_header(template_param_object_symbol_header,
                                     "<templ-param-object>", 20,
                                     /*is_unnamed=*/TRUE);
  }  /* if */
  sym = alloc_symbol((a_symbol_kind)sk_variable,
                     template_param_object_symbol_header, pos);
  sym->decl_scope = scope_stack[DEPTH_OF_FILE_SCOPE].number;
  return sym;
}  /* make_template_param_object_sym */


a_symbol_ptr make_unentered_symbol(a_symbol_kind        sym_kind,
                                   a_symbol_header_ptr  header,
                                   a_source_position    *pos)
/*
Create a symbol of kind sym_kind listed under the given symbol header, at
source position pos, declared in the current declaration scope.  Do not enter
it into the symbol table.
*/
{
  a_symbol_ptr  sym;

  sym = alloc_symbol(sym_kind, header, pos);
  sym->decl_scope = scope_stack[decl_scope_level].number;
  return sym;
}  /* make_unentered_symbol */


a_symbol_ptr make_unnamed_tag_symbol(a_symbol_kind      sym_kind,
                                     a_source_position  *pos)
/*
Create a symbol for a tagless class, struct, or union symbol.  Do not enter
it into the symbol table.
*/
{
  a_symbol_ptr  sym;

  db_enter(4, "make_unnamed_tag_symbol");
  /* Use the unnamed tag symbol header.  Allocate it if necessary. */
  if (unnamed_tag_symbol_header == NULL) {
    unnamed_tag_symbol_header = alloc_symbol_header();
    set_identifier_for_symbol_header(unnamed_tag_symbol_header,
                                     "<unnamed>", 9,
                                     /*is_unnamed=*/TRUE);
  }  /* if */
  sym = make_unentered_symbol(sym_kind, unnamed_tag_symbol_header, pos);
  db_exit();
  return sym;
}  /* make_unnamed_tag_symbol */


a_boolean is_unnamed_tag_symbol(a_symbol_ptr  sym)
/*
Return TRUE if sym represents an unnamed class type.
*/
{
  return (sym->header == unnamed_tag_symbol_header);
}  /* is_unnamed_tag_symbol */


a_boolean is_unnamed_namespace_symbol(a_symbol_ptr  sym)
/*
Return TRUE if sym represents an unnamed namespace.
*/
{
  return (sym->header == unnamed_namespace_symbol_header);
}  /* is_unnamed_namespace_symbol */


a_symbol_ptr get_unnamed_field_symbol(void)
/*
Return a pointer to "the" unnamed field symbol, which exists only for the
sake of identifying a given field entry as representing an unnamed field.
*/
{
  if (unnamed_field_symbol_header == NULL) {
    unnamed_field_symbol_header = alloc_symbol_header();
    set_identifier_for_symbol_header(unnamed_field_symbol_header,
                                     "<unnamed>", 9,
                                     /*is_unnamed=*/TRUE);
    unnamed_field_symbol = alloc_symbol((a_symbol_kind)sk_field,
                                        unnamed_field_symbol_header,
                                        &null_source_position);
  }  /* if */
  return unnamed_field_symbol;
}  /* get_unnamed_field_symbol */


a_symbol_ptr make_unnamed_namespace_symbol(a_source_position  *pos)
/*
Create a symbol for an unnamed namespace.  Do not enter it into the symbol
table.
*/
{
  a_symbol_ptr  sym;

  /* Use the unnamed namespace symbol header.  Allocate it if necessary. */
  if (unnamed_namespace_symbol_header == NULL) {
    unnamed_namespace_symbol_header = alloc_symbol_header();
    set_identifier_for_symbol_header(unnamed_namespace_symbol_header,
                                     "<unnamed>", 9,
                                     /*is_unnamed=*/TRUE);
  }  /* if */
  sym = alloc_symbol((a_symbol_kind)sk_namespace,
                     unnamed_namespace_symbol_header, pos);
  sym->decl_scope = scope_stack[depth_scope_stack].number;
  return sym;
}  /* make_unnamed_namespace_symbol */


static a_symbol_header_ptr make_unnamed_symbol_header(void)
/*
Return a unique unnamed symbol header.
*/
{
  a_symbol_header_ptr	sym_hdr;

  sym_hdr = alloc_symbol_header();
  set_identifier_for_symbol_header(sym_hdr, "<unnamed>", 9,
                                   /*is_unnamed=*/TRUE);
  return sym_hdr;
}  /* make_unnamed_symbol_header */


a_symbol_ptr make_unnamed_symbol(a_symbol_kind		kind,
				 a_source_position	*pos)
/*
Create a symbol for an unnamed entity.  Such symbols are not entered
into the symbol table.  Each unnamed symbol is given a unique symbol header.
*/
{
  a_symbol_ptr		sym;

  sym = alloc_symbol(kind, make_unnamed_symbol_header(), pos);
  sym->decl_scope = scope_stack[decl_scope_level].number;
  return sym;
}  /* make_unnamed_symbol */


a_symbol_ptr make_module_symbol(const a_string    &primary_name,
                                const a_string    &partition_name,
                                a_boolean         is_interface,
                                a_source_position *pos)
/*
Create a symbol to represent a module.  primary_name is the primary name of the
module and partition_name is the partition name of the module.  is_interface is
TRUE if the module is an interface unit.  pos is the position where the module
name started.
*/
{
  /* A module symbol must have a non-empty primary name. */
  check_assertion(!primary_name.is_empty());
  a_string combined_name;

  if (partition_name.is_empty()) {
    combined_name = primary_name;
  } else {
    combined_name.reset_to(primary_name, ":", partition_name);
  }  /* if */

  a_symbol_header_ptr sym_hdr = find_il_symbol_header(
                                            combined_name.as_temp_characters(),
                                            combined_name.length());
  a_symbol_ptr        sym = alloc_symbol(sk_named_module, sym_hdr, pos);
  sym->variant.module_info.primary_name =
                       find_il_symbol_header(primary_name.as_temp_characters(),
                                             primary_name.length());
  if (!partition_name.is_empty()) {
    sym->variant.module_info.partition_name =
                     find_il_symbol_header(partition_name.as_temp_characters(),
                                           partition_name.length());
  }  /* if */
  sym->variant.module_info.is_interface_unit = is_interface;
  return sym;
}  /* make_module_symbol */

#if MICROSOFT_EXTENSIONS_ALLOWED

void make_unnamed_virtual_function_locator(a_symbol_locator *loc)
/*
Initialize a locator, *loc, for an unnamed virtual function.
*/
{
  check_assertion(cppcx_enabled);
  clear_locator(loc, &null_source_position);
  if (unnamed_virtual_function_symbol_header == NULL) {
    unnamed_virtual_function_symbol_header = alloc_symbol_header();
    set_identifier_for_symbol_header(unnamed_virtual_function_symbol_header,
                                     "<unnamed>", 9,
                                     /*is_unnamed=*/TRUE);
  }  /* if */
  loc->symbol_header = unnamed_virtual_function_symbol_header;
}  /* make_unnamed_virtual_function_locator */


a_boolean is_unnamed_virtual_function_symbol(a_symbol_ptr sym)
/*
Return TRUE if sym represents an unnamed virtual function symbol.
*/
{
  check_assertion(sym->header != NULL);
  return sym->header == unnamed_virtual_function_symbol_header;
}  /* is_unnamed_virtual_function_symbol */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

a_symbol_ptr make_anonymous_parent_object_symbol(a_symbol_kind      kind,
                                                 a_source_position  *pos,
                                                 a_scope_number     decl_scope)
/*
Return a symbol for a field or variable that serves as the "anonymous
parent object" for an anonymous union.  Do not enter it in the symbol table.
*/
{
  a_symbol_ptr  sym;

  db_enter(4, "make_anonymous_parent_object_symbol");
  /* Use the unnamed class symbol header.  Allocate it if necessary. */
  if (anonymous_parent_object_symbol_header == NULL) {
    anonymous_parent_object_symbol_header = alloc_symbol_header();
    set_identifier_for_symbol_header(anonymous_parent_object_symbol_header,
                                     "<unnamed>", 9,
                                     /*is_unnamed=*/TRUE);
  }  /* if */
  sym = alloc_symbol(kind, anonymous_parent_object_symbol_header, pos);
  sym->decl_scope = decl_scope;
  db_exit();
  return sym;
}  /* make_anonymous_parent_object_symbol */


a_symbol_ptr full_enter_symbol(a_const_char  *identifier,
		               sizeof_t      length,
			       a_symbol_kind sym_kind,
			       a_scope_depth scope_depth)
/*
Enter a new symbol into the symbol table.  This is like enter_symbol, but
for those cases where a symbol locator is not available (because the symbol
is being entered regardless of any previous definition), like for keywords
entered during initialization.
*/
{
  a_symbol_locator  location;
  a_symbol_ptr      sym_ptr;

  db_enter(4, "full_enter_symbol");
  clear_locator(&location, &null_source_position);
  (void)find_symbol(identifier, length, &location);
  sym_ptr = enter_symbol(sym_kind, &location, scope_depth,
                         /*suppress_error=*/FALSE);
  db_exit();
  return sym_ptr;
}  /* full_enter_symbol */


void enter_keyword(a_token_kind token,
                   a_const_char *keyword)
/*
Enter a keyword.  keyword is the keyword string, token is the lexical
token that corresponds to it.
*/
{
  a_symbol_ptr sym_ptr;

  sym_ptr = full_enter_symbol(keyword, (sizeof_t)(strlen(keyword)),
			      (a_symbol_kind)sk_keyword, NO_SCOPE_DEPTH);
  sym_ptr->variant.keyword.token = token;
}  /* enter_keyword */


void enter_builtin_keyword(a_token_kind token,
                           a_const_char *keyword)
/*
Enter the name of a builtin operator as a keyword.  This differs from
enter_keyword in that it sets the is_builtin_function flag, thereby enabling
__has_builtin to return TRUE when queried for the builtin.
*/
{
  a_symbol_ptr sym_ptr;

  sym_ptr = full_enter_symbol(keyword, (sizeof_t)(strlen(keyword)),
			      (a_symbol_kind)sk_keyword, NO_SCOPE_DEPTH);
  sym_ptr->variant.keyword.token = token;
#if BUILTIN_FUNCTIONS_ENABLED
  sym_ptr->header->is_builtin_function = TRUE;
  sym_ptr->header->builtin_function_category = bfc_keyword;
#endif /* BUILTIN_FUNCTIONS_ENABLED */
}  /* enter_builtin_keyword */

#if NAMED_ADDRESS_SPACES_ALLOWED

a_symbol_ptr enter_named_address_space(a_const_char *name)
/*
Enter a new symbol for an Embedded C (TR 18037) named address space with the
given name.
*/
{
  a_symbol_ptr  sym;

  /* Verify that NUM_BITS_FOR_NAMED_ADDRESS_SPACE is configured with a
     sufficiently high value. */
  check_assertion_str(
        next_named_address_space_id < (1 << NUM_BITS_FOR_NAMED_ADDRESS_SPACE),
        "Too many named address spaces");
  sym = full_enter_symbol(name, (sizeof_t)(strlen(name)),
                          (a_symbol_kind)sk_named_address_space,
                          DEPTH_OF_FILE_SCOPE);
  sym->variant.named_address_space.id = next_named_address_space_id++;
  return sym;
}  /* enter_named_address_space */

#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
#if NAMED_REGISTERS_ALLOWED

a_symbol_ptr enter_named_register(a_const_char *name)
/*
Enter a new symbol for an Embedded C (TR 18037) named register with the given
name.
*/
{
  a_symbol_ptr  sym;

  sym = full_enter_symbol(name, (sizeof_t)(strlen(name)),
                          (a_symbol_kind)sk_named_register,
                          DEPTH_OF_FILE_SCOPE);
  sym->variant.named_register.id = next_named_register_id++;
  return sym;
}  /* enter_named_register */

#endif /* NAMED_REGISTERS_ALLOWED */

void enter_injected_class_name_symbol(a_symbol_ptr  tag_sym)
/*
Allocate a symbol to represent an injected class-name for the class
specified by tag_sym, and enter it into the symbol table.
*/
{
  a_symbol_ptr      sym;
  a_type_ptr        class_type = tag_sym->variant.class_struct_union.type;
  a_boolean         suppress_error = FALSE;

  if (!is_unnamed_tag_symbol(tag_sym) && !tag_sym->is_error) {
    sym = alloc_symbol((a_symbol_kind)sk_type, tag_sym->header,
                       &tag_sym->decl_position);
    sym->variant.type.ptr = class_type;
    sym->variant.type.is_injected_class_name = TRUE;
    sym->is_class_member = TRUE;
    sym->parent.class_type = class_type;
    add_symbol_to_scope_list(sym, depth_scope_stack, &suppress_error);
    link_symbol_into_symbol_table(sym, depth_scope_stack, suppress_error);
  }  /* if */
}  /* enter_injected_class_name_symbol */


a_symbol_ptr enter_typedef_symbol(a_type_ptr       type_ptr,
                                  a_symbol_locator *locator,
                                  a_scope_depth    scope_depth,
                                  a_boolean        suppress_error)
/*
Allocate an sk_type symbol to represent a typedef declaration for the type
specified by type_ptr and enter it into the symbol table.  This routine
is called for typedef declarations (in place of enter_local_symbol, which
calls enter_symbol) because some error checks require the type to be added
to the symbol entry before the symbol is added to the symbol table.
*/
{
  a_symbol_ptr  sym;

  db_enter(3, "enter_typedef_symbol");
  if (scope_stack[scope_depth].kind == (a_scope_kind)sck_func_prototype &&
      !is_error_locator(*locator) && !is_error_type(type_ptr)) {
    pos_warning(ec_decl_in_prototype_scope, &locator->source_position);
  }  /* if */
  sym = alloc_symbol((a_symbol_kind)sk_type, locator->symbol_header,
                         &locator->source_position);
  sym->is_error = locator->is_error;
  /* Set the locator to point to the symbol entered. */
  locator->specific_symbol = sym;
  locator->is_qualified_name = FALSE;
  /* Bind the type in the symbol.  This has to be done before adding the
     symbol to the symbol table. */
  sym->variant.type.ptr = type_ptr;
  /* Add the symbol to the proper scope's symbol list. */
  add_symbol_to_scope_list(sym, scope_depth, &suppress_error);
  /* Add the symbol to the symbol table.  This must be done after the symbol
     is added to the scope list, because that sets the scope number, which
     is needed to check for redeclaration. */
  link_symbol_into_symbol_table(sym, scope_depth, suppress_error);
  db_exit();
  return sym;
}  /* enter_typedef_symbol */


void make_symbol_for_predeclared_type(a_type_ptr   predeclared_type,
                                      a_const_char *name)
/*
Create a symbol of the specified name for the type entry pointed to by
predeclared_type, binding them to one another.  (predeclared_type should
point to an undefined class or struct type generated by the compiler.)
*/
{
  a_symbol_locator  loc;
  a_symbol_ptr      sym;

  check_assertion(predeclared_type != NULL &&
                  predeclared_type->source_corresp.assoc_info == NULL);
  clear_locator(&loc, &null_source_position);
  (void)find_symbol(name, (sizeof_t)strlen(name), &loc);
  check_assertion(predeclared_type->kind == (a_type_kind)tk_struct ||
                  predeclared_type->kind == (a_type_kind)tk_class);
  sym = alloc_symbol((a_symbol_kind)sk_class_or_struct_tag,
                     loc.symbol_header, &null_source_position);
  set_source_corresp(&(predeclared_type->source_corresp), sym);
  sym->variant.class_struct_union.type = predeclared_type;
}  /* make_symbol_for_predeclared_type */

static void enter_symbol_for_namespace(a_symbol_ptr      sym,
                                       a_symbol_locator  *locator);

static void make_symbol_for_predeclared_namespace(a_const_char     *name,
                                                  a_namespace_ptr  parent,
                                                  a_symbol_ptr     *sym)
/*
Predeclare the namespace with the indicated name -- that is, create the
namespace entry and add it to the namespaces list in the indicated parent
namespace (or the file scope if parent is NULL), and create the symbol entry
for the namespace and return it in *sym.  However, don't actually add the
symbol to the symbol table (since the user is actually free to use the name
for other entities as long as the namespace is never declared).
*/
{
  a_namespace_ptr  nsp;
  a_symbol_locator loc;

  /* Be sure the symbol hasn't been created yet. */
  check_assertion(*sym == NULL);
  check_assertion(depth_scope_stack == DEPTH_OF_FILE_SCOPE);
  if (parent != NULL) {
    (void)push_namespace_scope((a_scope_kind)sck_namespace_extension, parent);
  }  /* if */
  /* Create the symbol header. */
  clear_locator(&loc, &null_source_position);
  (void)find_symbol(name, (sizeof_t)strlen(name), &loc);
  /* Allocate the symbol. */
  *sym = alloc_symbol((a_symbol_kind)sk_namespace, loc.symbol_header,
                      &null_source_position);
  (*sym)->decl_scope = file_scope_number;
  if (parent != NULL) {
    (*sym)->parent.namespace_ptr = parent;
  }  /* if */
  /* Create the namespace entry and bind it to the symbol. */
  nsp = alloc_namespace(/*is_alias=*/FALSE);
  set_source_corresp(&nsp->source_corresp, *sym);
  nsp->source_corresp.name_linkage =
                                  (a_name_linkage_kind)nlk_cplusplus_external;
  (*sym)->variant.namespace_info.ptr = nsp;
  /* Add the namespace to the namespaces list for the file scope. */
  add_to_namespaces_list(nsp);
  /* Push a scope to be sure a scope entry is recorded in the new
     namespace; then pop it off the stack again. */
  (void)push_namespace_scope((a_scope_kind)sck_namespace, nsp);
  pop_scope();
  if (parent != NULL) {
    pop_scope();
  }  /* if */
}  /* make_symbol_for_predeclared_namespace */


void make_symbol_for_namespace_std(void)
/*
Predeclare namespace "std".  Don't put its symbol into the symbol table yet.
If reflection is enabled, also predeclare "std::meta".
*/
{
  a_namespace_ptr  std_nsp;

  make_symbol_for_predeclared_namespace("std", (a_namespace_ptr)NULL,
                                        &symbol_for_namespace_std);
  std_nsp = symbol_for_namespace_std->variant.namespace_info.ptr;
  std_nsp->is_std = TRUE;
  if (reflection_enabled) {
    make_symbol_for_predeclared_namespace("meta", std_nsp,
                                          &symbol_for_namespace_std_meta);
  }  /* if */
}  /* make_symbol_for_namespace_std */

#if IA64_ABI

void make_symbol_for_namespace_abi(void)
/*
Predeclare namespace "abi".  This is the namespace defined in the IA-64
ABI, in which things like the derived classes of type_info are defined.
Don't put its symbol into the symbol table yet.
*/
{
  /* The namespace "abi" is actually an alias; here we want to declare the
     underlying namespace, not the alias. */
  make_symbol_for_predeclared_namespace("__cxxabiv1", (a_namespace_ptr)NULL,
                                        &symbol_for_namespace_abi);
}  /* make_symbol_for_namespace_abi */

#endif /* IA64_ABI */

a_symbol_ptr look_up_name_string_in_namespace(
                                        a_const_char             *symbol_name,
                                        a_namespace_ptr          ns_ptr,
                                        an_id_lookup_options_set options)
/*
Look up symbol_name in the specified namespace (or file scope if ns_ptr is
NULL).  Return the symbol found, if any.  options is passed to the underlying
call of file_scope_id_lookup or namespace_qualified_id_lookup.
*/
{
  a_symbol_locator loc;
  a_symbol_ptr     sym;

  clear_locator(&loc, &null_source_position);
  (void)find_symbol(symbol_name, (sizeof_t)strlen(symbol_name), &loc);
  if (ns_ptr == NULL) {
    sym = file_scope_id_lookup(il_header.primary_scope, &loc, options);
  } else {
    sym = namespace_qualified_id_lookup(&loc, ns_ptr, options);
  }  /* if */
  return sym;
}  /* look_up_name_string_in_namespace */


a_boolean resolve_pending_trailing_requires_clause(a_symbol_ptr  sym)
/*
sym represents a member function or a friend function of a class template
instance with a requires-clause whose satisfaction has not been resolved yet.
Resolve it now (by substituting the requires-clause constraint).  Return TRUE
if the constraints fails, or FALSE otherwise.
*/
{
  a_routine_ptr          rp = sym->variant.routine.ptr;
  a_requires_clause_ptr  rcp = rp->trailing_requires_clause;;
  a_subst_pairs_array    subst_pairs(1);
  an_expr_node_ptr       constraint;
  a_boolean              err = FALSE;
  a_type_ptr             enclosing_class;
  a_decl_parse_state     dps;
  a_diag_list            diag_list;

  sym->variant.routine.pending_trailing_requires_clause = FALSE;
  if (symbol_is(sym, sk_member_function)) {
    a_template_ptr  rout_templ = rp->assoc_template;
    if (rout_templ == NULL) {
      /* This can happen in severe error cases. */
      expect_error();
      err = TRUE;
      goto done;
    }  /* if */
    enclosing_class = sym_parent_class(sym);
    push_enclosing_class_scope_for_rescan(enclosing_class, rp);
    init_decl_parse_state(&dps);
    dps.sym = sym;
    dps.type = rp->type;
    dps.is_inclass_member_function_decl = TRUE;
    (void)push_scope(sck_func_prototype, NO_SCOPE_NUMBER, rp->type,
                     (a_routine_ptr)NULL);
    scope_stack_top().outside_parameter_list = TRUE;
    scope_stack_top().decl_parse_state = &dps;
  } else {
    /* A friend function defined in a class template instance. */
    check_assertion(rp->routine_fixup != NULL);
    enclosing_class = class_from_routine_fixup(rp->routine_fixup);
    push_enclosing_class_scope_for_rescan(enclosing_class, NULL);
  }  /* if */
  /* Identify all the substitutions applicable to the constraint. */
  get_all_class_subst_pairs(enclosing_class, &subst_pairs);
  clear_diag_list(&diag_list);
  constraint = rcp->constraint;
  /* Check the constraint. */
  if (!constraint_satisfied_full(constraint, subst_pairs, &diag_list,
                                 CTWS_NO_OPTIONS,
                                 (a_ctws_state_ptr)NULL, &err)) {
    err = TRUE;
  }  /* if */
  if (err) {
    rp->is_ineligible = TRUE;
  }  /* if */
  if (symbol_is(sym, sk_member_function)) {
    check_assertion(scope_is(&scope_stack_top(), sck_func_prototype));
    pop_scope();
  }  /* if */
  pop_enclosing_class_scope_for_rescan();
done:
  return err;
}  /* resolve_pending_trailing_requires_clause */


static a_symbol_ptr make_internal_template(a_const_char    *symbol_name,
                                           a_const_char    *definition_string,
                                           a_namespace_ptr ns_ptr,
                                           a_boolean       is_metadata)
/*
Declare and define the template specified by symbol_name and return the symbol
associated with it.  The definition is provided by definition_string.
These templates are generated internally to implement C++/CLI and C++/CX
features like cli::interior_ptr or Platform::WriteOnlyArray as well as
builtin alias templates (e.g., __make_integer_seq).  is_metadata is TRUE if
the template is for a C++/CLI type and FALSE otherwise.
*/
{
  a_template_symbol_supplement_ptr tssp;
  a_symbol_ptr                     result_sym;

  check_assertion(internal_templates_enabled);
  scan_top_level_generated_code(definition_string, (an_assembly_index)0,
                                is_metadata);
  result_sym = look_up_name_string_in_namespace(
                                           symbol_name, ns_ptr,
                                           IDL_DIRECT_NAMESPACE_MEMBERS_ONLY);
  check_assertion(result_sym != NULL &&
                  symbol_is(result_sym, sk_class_template));
  tssp = result_sym->variant.template_info;
  tssp->variant.class_template.cannot_be_specialized = TRUE;
  return result_sym;
}  /* make_internal_template */


void make_make_integer_seq_internal_template(void)
/*
Creates a builtin class template for "__make_integer_seq" and also a builtin
alias template for "__make_integer_seq_alias" at the file scope.  In cases
where the arguments to __make_integer_seq are dependent, the class template is
used (so that it survives rescanning).  For the non-dependent case, the
internal alias is used.
*/
{
  check_assertion(variadic_templates_enabled);
  /* Create a class template for __make_integer_seq. */
  symbol_for_make_integer_seq = make_internal_template(
      "__make_integer_seq",
      "template<template<typename U, U... K> class S, typename T, T N>"
      "  struct __make_integer_seq;",
      (a_namespace_ptr)NULL,
      /*is_metadata=*/FALSE);
  /* Note that the target type of the alias template (i.e., "T") is arbitrary
     here as the template will be instantiated programatically (by
     instantiate_make_integer_seq). */
  symbol_for_make_integer_seq_alias = make_internal_template(
      "__make_integer_seq_alias",
      "template<template<typename U, U... K> class S, typename T, T N>"
      "  __internal_alias_decl __make_integer_seq_alias = T;",
      (a_namespace_ptr)NULL,
      /*is_metadata=*/FALSE);
}  /* make_make_integer_seq_internal_template */


void make_type_pack_element_internal_template(void)
/*
Creates a builtin alias template for "__type_pack_element" at the file scope.
*/
{
  /* Note that the target type of the alias template (i.e., "int") is
     arbitrary here as the template will be instantiated programatically (by
     instantiate_type_pack_element). */
  check_assertion(variadic_templates_enabled);
  symbol_for_type_pack_element = make_internal_template(
      "__type_pack_element",
      "template<__edg_size_type__ N, typename ...T>"
      "  struct __type_pack_element;",
      (a_namespace_ptr)NULL,
      /*is_metadata=*/FALSE);
  symbol_for_type_pack_element_alias = make_internal_template(
      "__type_pack_element_alias",
      "template<__edg_size_type__ N, typename ...T>"
      "  __internal_alias_decl __type_pack_element_alias = int;",
      (a_namespace_ptr)NULL,
      /*is_metadata=*/FALSE);
}  /* make_type_pack_element_internal_template */


void make_builtin_common_type_internal_templates(void)
/*
Create a builtin class template "__builtin_common_type" and a corresponding
alias template "__builtin_common_type_alias" (both at file scope).  This is a
Clang 20.0 feature, where __builtin_common_type is an alias template.  We use
both a class template and an alias template because our default handling of an
alias template would erase the alias too quickly.  Instead, we switch the class
template to the alias template (in templates.c) once we have a nondependent
template argument list.
*/
{
  /* Note that the target type of the alias template (i.e., "void") is
     arbitrary here as the template will be instantiated programatically (by
     instantiate_builtin_common_type). */
  check_assertion(variadic_templates_enabled);
  symbol_for_builtin_common_type = make_internal_template(
      "__builtin_common_type",
      "template<template<class ... _Args> class _BaseTemplate,"
      "         template<class _TypeMember> class _HasTypeMember,"
      "         class _HasNoTypeMember, class ..._Ts>"
      "  struct __builtin_common_type;",
      (a_namespace_ptr)NULL,
      /*is_metadata=*/FALSE);
  symbol_for_builtin_common_type_alias = make_internal_template(
      "__builtin_common_type_alias",
      "template<template<class ... _Args> class _BaseTemplate,"
      "         template<class _TypeMember> class _HasTypeMember,"
      "         class _HasNoTypeMember, class ..._Ts>"
      "  __internal_alias_decl __builtin_common_type_alias = void;",
      (a_namespace_ptr)NULL,
      /*is_metadata=*/FALSE);
}  /* make_builtin_common_type_internal_templates */


void make_builtin_dedup_pack_internal_template(void)
/*
Creates a builtin class template for "__builtin_dedup_pack" at the file scope.
*/
{
  check_assertion(variadic_templates_enabled);
  symbol_for_builtin_dedup_pack = make_internal_template(
      "__builtin_dedup_pack",
      "template<typename ...T>"
      "  struct __builtin_dedup_pack;",
      (a_namespace_ptr)NULL,
      /*is_metadata=*/FALSE);
}  /* make_builtin_dedup_pack_internal_template */


#if MICROSOFT_EXTENSIONS_ALLOWED

static void init_cli_symbol(a_cli_symbol_kind  csk);

static a_namespace_ptr f_cli_namespace_ptr_for(a_cli_symbol_kind kind)
/*
The given C++/CLI symbol kind must designate a one of the C++/CLI namespaces
known to the front end (like "System" or "cli").  Return the IL entry for that
namespace.
*/
{
  a_symbol_ptr     sym;
  a_namespace_ptr  result;

  check_assertion((int)kind >= (int)csk_first_namespace &&
                  (int)kind <= (int)csk_last_namespace);
  sym = cli_symbol_from_kind(kind);
  if (sym == NULL) {
    init_cli_symbol(kind);
    sym = cli_symbol_from_kind(kind);
  }  /* if */
  if (!cli_symbol_is_required(kind) && sym == NULL) {
    result = NULL;
  } else {
    check_assertion(sym != NULL && is_namespace_symbol(sym));
    result = sym->variant.namespace_info.ptr;
  }  /* if */
  return result;
}  /* f_cli_namespace_ptr_for */

#define cli_namespace_ptr_for(csk)                                           \
  (f_cli_namespace_ptr_for((a_cli_symbol_kind)(csk)))


static void init_cli_symbol(a_cli_symbol_kind  csk)
/*
Look up the C++/CLI namespace or type specified by csk and cache it in the
cli_symbols array.  This function assumes that mscorlib.dll has been imported.
*/
{
  a_const_char      *name;
  a_cli_symbol_kind ns_kind;

  check_assertion((int)csk >= (int)csk_first && (int)csk < (int)csk_last);
  name = cli_symbol_names[csk].name;
  ns_kind = (a_cli_symbol_kind)cli_symbol_names[csk].namespace_kind;
  if (cppcx_enabled) {
    if (cli_symbol_names[csk].cppcx_name != NULL) {
      name = cli_symbol_names[csk].cppcx_name;
    }  /* if */
    if (cli_symbol_names[csk].cppcx_namespace_kind != csk_none) {
      ns_kind = (a_cli_symbol_kind)cli_symbol_names[csk].cppcx_namespace_kind;
    }  /* if */
  }  /* if */
  if (name != NULL) {
    a_namespace_ptr          ns_ptr = NULL;
    an_id_lookup_options_set options = IDL_DIRECT_NAMESPACE_MEMBERS_ONLY;
    a_boolean                required = cli_symbol_is_required(csk);
    check_assertion(*name != '\0');
    if ((int)csk >= (int)csk_first_namespace &&
        (int)csk <= (int)csk_last_namespace) {
      options |= IDL_MUST_BE_NAMESPACE;
    }  /* if */
    if (ns_kind != (a_cli_symbol_kind)csk_none) {
      ns_ptr = cli_namespace_ptr_for(ns_kind);
      if (ns_ptr == NULL) {
        if (!required) goto done;
        /* The parent namespace wasn't found. */
        str_catastrophe(ec_cli_entity_not_loaded, name);
      }  /* if */
    }  /* if */
    cli_symbols[csk] = look_up_name_string_in_namespace(name, ns_ptr, options);
    if (required && cli_symbols[csk] == NULL) {
      /* The symbol wasn't found in the parent namespace. */
      str_catastrophe(ec_cli_entity_not_loaded, name);
    }  /* if */
  }  /* if */
done:;
}  /* init_cli_symbol */


a_type_ptr f_cli_class_type_for(a_cli_symbol_kind kind)
/*
The given C++/CLI symbol kind must designate one of the C++/CLI class types
known to the front end (e.g., "System::Float", but not e.g. "cli::array",
which is a template).  Return the IL entry for that class type.
*/
{
  a_type_ptr              type;
  a_symbol_ptr            sym;

  check_assertion((int)kind >= (int)csk_first_type &&
                  (int)kind <= (int)csk_last_type);
  sym = cli_symbol_from_kind(kind);
  if (sym == NULL) {
    init_cli_symbol(kind);
    sym = cli_symbol_from_kind(kind);
  }  /* if */
  type = (sym != NULL && is_type_symbol(sym)) ? type_symbol_type(sym) : NULL;
  check_assertion(!cli_symbol_is_required(kind) || type != NULL);
  return type;
}  /* f_cli_class_type_for */


void make_symbol_for_namespace_cli(void)
/*
Predeclare namespace "cli".  This namespace is used in C++/CLI mode.
*/
{
  a_symbol_locator locator;
  a_symbol_ptr     symbol = NULL;

  clear_locator(&locator, &null_source_position);
  make_symbol_for_predeclared_namespace("cli", (a_namespace_ptr)NULL, &symbol);
  enter_symbol_for_namespace(symbol, &locator);
  cli_symbols[(int)csk_cli_namespace] = symbol;
}  /* make_symbol_for_namespace_cli */


static void make_symbol_for_cli_array(void)
/*
In C++/CLI mode, declare and define the C++/CLI type "cli::array".  (The
definition is lifted from ECMA-372, subsection 8.2.3.)
*/
{
 /* Create cli::array in two parts.  First, declare the template without
    defining it to ensure cli_symbols[csk_cli_array] is set before the
    prototype instantiation of cli::array is done.  Then complete the
    definition. */
  cli_symbols[(int)csk_cli_array] = make_internal_template("array",
      "namespace cli {"
      "  template <typename T, int rank = 1>"
      "  ref class array;"
      "}",
      cli_namespace_ptr_for(csk_cli_namespace),
      /*is_metadata=*/TRUE);
  cli_symbols[(int)csk_cli_array]
      ->variant.template_info
      ->variant.class_template.prototype_instantiation
      ->variant.class_struct_union.type
      ->variant.class_struct_union.extra_info->is_cli_array = TRUE;
  scan_top_level_generated_code(
      "namespace cli {"
      "  template <typename T, int rank>"
      "  ref class array sealed : System::Array {};"
      "}",
      (an_assembly_index)0,
      /*is_metadata=*/TRUE);
}  /* make_symbol_for_cli_array */


static void make_symbol_for_cli_interior_ptr(void)
/*
Declare and define the C++/CLI type "cli::interior_ptr".
*/
{
  cli_symbols[(int)csk_interior_ptr] =
    make_internal_template("interior_ptr",
      "namespace cli {"
      "  template <typename Type>"
      "  __internal_alias_decl interior_ptr ="
      "              __declspec(__edg_interior_ptr_alias) Type;"
      "}",
      cli_namespace_ptr_for(csk_cli_namespace),
      /*is_metadata=*/TRUE);
}  /* make_symbol_for_cli_interior_ptr */


static void make_symbol_for_cli_pin_ptr(void)
/*
Declare and define the C++/CLI type "cli::pin_ptr".
*/
{
  cli_symbols[(int)csk_pin_ptr] = make_internal_template("pin_ptr",
      "namespace cli {"
      "  template <typename Type>"
      "  __internal_alias_decl pin_ptr ="
      "              __declspec(__edg_pin_ptr_alias) Type;"
      "}",
      cli_namespace_ptr_for(csk_cli_namespace),
      /*is_metadata=*/TRUE);
}  /* make_symbol_for_cli_pin_ptr */


static void make_symbols_for_cppcx_arrays(void)
/*
Declare (but do not define) the C++/CX templates Platform::WriteOnlyArray
and Platform::Array (the definitions will come from the vccorlib.h header).
This routine should only be called in C++/CX mode.

Predeclaring these template is convenient for at least two reasons:
    1) We need to set is_cli_array to TRUE.
    2) If in the future changes are made such that we need the array type
       while importing windows.foundation.winmd (or platform.winmd for that
       matter), we need to have this type pre-declared.
Don't put default template arguments since those will be specified in the
vccorlib.h header.
*/
{
  check_assertion(cppcx_enabled);
  cli_symbols[(int)csk_cli_array] = make_internal_template(
        "Array",
          "namespace Platform {"
          "  template <typename T, unsigned int dimension>"
          "  ref class Array;"
          "}",
        cli_namespace_ptr_for(csk_system_namespace),
        /*is_metadata=*/TRUE);
  cli_symbols[(int)csk_cli_array]
      ->variant.template_info
      ->variant.class_template.prototype_instantiation
      ->variant.class_struct_union.type
      ->variant.class_struct_union.extra_info->is_cli_array = TRUE;
  cli_symbols[(int)csk_platform_write_only_array] = make_internal_template(
        "WriteOnlyArray",
          "namespace Platform {"
          "  template <typename T, unsigned int dimension>"
          "  ref class WriteOnlyArray;"
          "}",
        cli_namespace_ptr_for(csk_system_namespace),
        /*is_metadata=*/TRUE);
  cli_symbols[(int)csk_platform_write_only_array]
        ->variant.template_info
        ->variant.class_template.prototype_instantiation
        ->variant.class_struct_union.type
        ->variant.class_struct_union.extra_info->is_cli_array = TRUE;
  cli_symbols[(int)csk_platform_write_only_array]
        ->variant.template_info
        ->variant.class_template.prototype_instantiation
        ->variant.class_struct_union.type
        ->variant.class_struct_union.extra_info
        ->is_cppcx_write_only_array = TRUE;
  /* make_internal_template disables specialization of the class template.
     However, C++/CX defines specializations for single-dimension
     WriteOnlyArray and Array instantiations.  Enable specializations for
     those templates here (they will be disabled again once we have scanned
     them). */
  cli_symbols[(int)csk_platform_write_only_array]->variant.template_info
                       ->variant.class_template.cannot_be_specialized = FALSE;
  cli_symbols[(int)csk_cli_array]->variant.template_info->variant
                                .class_template.cannot_be_specialized = FALSE;
}  /* make_symbols_for_cppcx_arrays */


void make_symbol_for_cppcx_box(void)
/*
Declare and define the C++/CX type "Platform::Box".
*/
{
  /* Declare Platform::Box<T> without defining it to ensure that
     cli_symbols[csk_cppcx_box] is set before the prototype instantiation of
     Platform::Box is done when we encounter the definition in vccorlib.h. */
  cli_symbols[(int)csk_cppcx_box] = make_internal_template(
    "Box",
      "namespace Platform {"
      "  template <typename T>"
      "  ref class Box;"
      "}",
    cli_namespace_ptr_for(csk_system_namespace),
    /*is_metadata=*/TRUE);
  cli_symbols[(int)csk_cppcx_box]
                 ->variant.template_info
                 ->variant.class_template.prototype_instantiation
                 ->variant.class_struct_union.type
                 ->variant.class_struct_union.extra_info->is_cppcx_box = TRUE;
}  /* make_symbol_for_cppcx_box */


void make_symbol_for_abi_hstring(void)
/*
C++/CX symbol for the __abi_HSTRING type.  This will be used to inject the
special Platform::String constructor that accepts an argument of the
__abi_HSTRING type in C++/CX mode.
*/
{
  check_assertion (cppcx_enabled);
  scan_top_level_generated_code("struct HSTRING__;", (an_assembly_index)0,
                                /*is_metadata=*/TRUE);
  init_cli_symbol((a_cli_symbol_kind)csk_abi_hstring);
}  /* make_symbol_for_abi_hstring */


static void init_cli_symbols_corresponding_to_fundamental_types(void)
/*
For each fundamental CLI type, initialize its corresponding_basic_type member.
This will be used later by the fundamental_type_from_system_type function.
Without taking modopts into account, there is a one-to-many relationship
when mapping some CLI types to basic types.  For example, System::Int32
could be mapped to either int or long.  The cli_integer_kinds and
cli_float_kinds arrays therefore list only the preferred basic types.
*/
{
  /* Select only the integer kinds that a CLI fundamental type maps to. */
  an_integer_kind cli_integer_kinds[] = {
    (an_integer_kind)ik_signed_char,
    (an_integer_kind)ik_unsigned_char,
    (an_integer_kind)ik_short,
    (an_integer_kind)ik_unsigned_short,
    (an_integer_kind)ik_int,
    (an_integer_kind)ik_unsigned_int,
    (an_integer_kind)ik_long_long,
    (an_integer_kind)ik_unsigned_long_long
  };
  /* Select only the float kinds that a CLI fundamental type maps to. */
  a_float_kind    cli_float_kinds[] = {
    (a_float_kind)fk_float,
    (a_float_kind)fk_double
  };
  int             i;
  a_symbol_ptr    cli_symbol;

  /* Map the CLI fundamental integer types. */
  for (i = 0;
       i < (int)(sizeof(cli_integer_kinds)/sizeof(cli_integer_kinds[0]));
       i++) {
    an_integer_kind kind = cli_integer_kinds[i];
    cli_symbol = cli_symbol_from_integer_kind(kind);
    check_assertion(cli_symbol != NULL);
    check_assertion(class_type_supp(type_symbol_type(cli_symbol))->
                                             corresponding_basic_type == NULL);
    class_type_supp(type_symbol_type(cli_symbol))->
                                 corresponding_basic_type = integer_type(kind);
  }  /* for */
  /* Map the CLI fundamental float types. */
  for (i = 0;
       i < (int)(sizeof(cli_float_kinds)/sizeof(cli_float_kinds[0]));
       i++) {
    a_float_kind kind = cli_float_kinds[i];
    cli_symbol = cli_symbol_from_float_kind(kind);
    check_assertion(cli_symbol != NULL);
    check_assertion(class_type_supp(type_symbol_type(cli_symbol))->
                                             corresponding_basic_type == NULL);
    class_type_supp(type_symbol_type(cli_symbol))->
                                   corresponding_basic_type = float_type(kind);
  }  /* for */
  /* Map System::Boolean to bool. */
  cli_symbol = cli_symbol_from_kind(csk_system_boolean);
  check_assertion(cli_symbol != NULL);
  check_assertion(class_type_supp(type_symbol_type(cli_symbol))->
                                             corresponding_basic_type == NULL);
  class_type_supp(type_symbol_type(cli_symbol))->
                                        corresponding_basic_type = bool_type();
  /* Map System::Char to wchar_t. */
  cli_symbol = cli_symbol_from_kind(csk_system_char);
  check_assertion(cli_symbol != NULL);
  check_assertion(class_type_supp(type_symbol_type(cli_symbol))->
                                             corresponding_basic_type == NULL);
  class_type_supp(type_symbol_type(cli_symbol))->
                                     corresponding_basic_type = wchar_t_type();
  if (cppcx_enabled) {
    /* Map Platform::SizeT to size_t. */
    cli_symbol = cli_symbol_from_kind(csk_size_t);
    check_assertion(cli_symbol != NULL);
    check_assertion(class_type_supp(type_symbol_type(cli_symbol))->
                                             corresponding_basic_type == NULL);
    class_type_supp(type_symbol_type(cli_symbol))->corresponding_basic_type =
                                   type_symbol_type(predeclared_size_t_symbol);
    /* Map Platform::Details::_GUID to const _GUID&. */
    cli_symbol = cli_symbol_from_kind(csk_platform_details_guid);
    check_assertion(cli_symbol != NULL);
    check_assertion(class_type_supp(type_symbol_type(cli_symbol))->
                                             corresponding_basic_type == NULL);
    class_type_supp(type_symbol_type(cli_symbol))->corresponding_basic_type =
             make_reference_type(make_qualified_type(type_of_guid, TQ_CONST));
  } else {
    /* Map System::Void to void.  (There is no C++/CX counterpart for this
       one.) */
    cli_symbol = cli_symbol_from_kind(csk_system_void);
    check_assertion(cli_symbol != NULL);
    check_assertion(class_type_supp(type_symbol_type(cli_symbol))->
                                             corresponding_basic_type == NULL);
    class_type_supp(type_symbol_type(cli_symbol))->
                                        corresponding_basic_type = void_type();
  }  /* if */
}  /* init_cli_symbols_corresponding_to_fundamental_types */


a_boolean is_cli_cx_pseudo_template(a_symbol_ptr	template_sym)
/*
Return true if template_sym represents a C++/CLI or C++/CX facility (such as
arrays, pin_ptrs, etc.) that is implemented as a template but is not
really a template in the language.
*/
{
  a_boolean	result = FALSE;

  if (cppcx_enabled) {
    if (template_sym == cli_symbol_from_kind(csk_platform_write_only_array) ||
        template_sym == cli_symbol_from_kind(csk_cli_array) ||
        template_sym == cli_symbol_from_kind(csk_cppcx_box)) {
      result = TRUE;
    }  /* if */
  } else if (cli_or_cx_enabled) {
    if (template_sym == cli_symbol_from_kind(csk_cli_array) ||
        template_sym == cli_symbol_from_kind(csk_pin_ptr) ||
        template_sym == cli_symbol_from_kind(csk_interior_ptr)) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* is_cli_cx_pseudo_template */


a_symbol_ptr f_cli_symbol_from_kind_or_null(a_cli_symbol_kind kind)
/*
Return the symbol associated with kind, and if it is not initialized, attempt
to initialize the symbol.  If the symbol cannot be initialized, return NULL.
This function is often more conveniently called through the corresponding
macro cli_symbol_from_kind_or_null.
*/
{
  a_symbol_ptr sym = cli_symbol_from_kind(kind);

  if (sym == NULL) {
    init_cli_symbol(kind);
    sym = cli_symbol_from_kind(kind);
  }  /* if */
  return sym;
}  /* f_cli_symbol_from_kind_or_null */


static void make_symbols_for_system_string_operators(void)
/*
Create symbols for the builtin System::String operators.
*/
{
  a_symbol_locator  locator;
  a_type_ptr        hstring_type, hobject_type;
  
  hstring_type = make_handle_to_system_string();
  hobject_type = make_handle_type(
                    type_symbol_type(cli_symbol_from_kind(csk_system_object)));
  make_opname_locator((an_opname_kind)onk_plus, &locator,
                      &null_source_position);
  /* Create compiler-generated
     String^ operator+(String^ left, String^ right); */
  (void)make_predeclared_function_symbol(&locator,
                                         make_routine_type(hstring_type,
                                                           hstring_type,
                                                           hstring_type,
                                                           (a_type_ptr)NULL,
                                                           (a_type_ptr)NULL));
  make_opname_locator((an_opname_kind)onk_plus, &locator,
                      &null_source_position);
  /* Create compiler-generated
     String^ operator+(String^ left, Object^ right); */
  (void)make_predeclared_function_symbol(&locator,
                                         make_routine_type(hstring_type,
                                                           hstring_type,
                                                           hobject_type,
                                                           (a_type_ptr)NULL,
                                                           (a_type_ptr)NULL));
  make_opname_locator((an_opname_kind)onk_plus, &locator,
                      &null_source_position);
  /* Create compiler-generated
     String^ operator+(Object^ left, String^ right); */
  (void)make_predeclared_function_symbol(&locator,
                                         make_routine_type(hstring_type,
                                                           hobject_type,
                                                           hstring_type,
                                                           (a_type_ptr)NULL,
                                                           (a_type_ptr)NULL));
}  /* make_symbols_for_system_string_operators */


void init_cli_symbols()
/*
Initialize symbols for various C++/CLI core library entities that the front
end knows about (this function assumes that mscorlib.dll has been imported).
Many of these symbols will be accessible through the cli_symbols array.
*/
{
  int  csk;

#if CHECKING
  /* The CLI integer kinds must span ik_char through ik_unsigned_long_long.
     Later integer kinds (__int128, _BitInt) have no CLI counterpart. */
  /*lint -e{506,1564}*/
  if ((int)csk_last_integer - (int)csk_first_integer !=
                                                 (int)ik_unsigned_long_long) {
    internal_error("init_cli_symbols: incorrect a_cli_symbol_kind");
  }  /* if */
#endif /* CHECKING */
  /* Initialize the symbols in the cli_symbols array. */
  for (csk = (int)csk_first_type; csk <= (int)csk_last_type; csk++) {
    a_cli_symbol_init_flag_set  init_mask;
    init_mask = cppcx_enabled ?  CISF_PLATFORM_METADATA : CISF_CLI_METADATA;
    if (cli_symbols[csk] == NULL &&
        (cli_symbol_names[csk].init_flags == CISF_DEFAULT ||
         (cli_symbol_names[csk].init_flags & init_mask) != 0)) {
      init_cli_symbol((a_cli_symbol_kind)csk);
    }  /* if */
  }  /* for */
  /* Initialize csk_system_byte_sign_unspecified based on the signedness
     of plain char. */
  cli_symbols[(int)csk_system_byte_sign_unspecified] =
          il_header.plain_chars_are_signed ? cli_symbols[(int)csk_system_sbyte]
                                           : cli_symbols[(int)csk_system_byte];

  init_cli_symbols_corresponding_to_fundamental_types();
  make_symbols_for_system_string_operators();
  /* Make the symbol associated with some C++/CLI internal templates. */
  if (cppcx_enabled) {
    make_symbols_for_cppcx_arrays();
  } else {
    make_symbol_for_cli_array();
    make_symbol_for_cli_interior_ptr();
    make_symbol_for_cli_pin_ptr();
  }  /* if */
}  /* init_cli_symbols */


void init_windows_metadata_symbols(void)
/*
Look up various C++/CX types (and the namespaces in which they are located)
and cache them in the cli_symbols array.  This function assumes that
Windows.winmd has been imported.
*/
{
  int csk;

  /* Initialize the symbols in the cli_symbols array. */
  for (csk = (int)csk_first_type; csk <= (int)csk_last_type; csk++) {
    if (cli_symbols[csk] == NULL &&
        (cli_symbol_names[csk].init_flags & CISF_WINDOWS_METADATA) != 0) {
      init_cli_symbol((a_cli_symbol_kind)csk);
    }  /* if */
  }  /* for */
}  /* init_windows_metadata_symbols */


a_boolean is_generic_cli_ienumerable_type(a_type_ptr type,
                                          a_type_ptr elem_type)
/*
Return TRUE if "type" is an instance of the C++/CLI
System::Collections::Generic::IEnumerable<T> generic class.
If elem_type is non-NULL, it is a specific type T (although some
conversions are allowed).  If it's NULL, the test is for any instance
of the generic class.
*/
{
  a_boolean is_instance = FALSE;

  type = skip_typerefs(type);
  if (is_class_struct_union_type(type) &&
      type->variant.class_struct_union.is_generic_instance &&
      is_namespace_member(type)) {
    a_class_symbol_supplement_ptr cssp = symbol_supplement_for_class(type);
    a_symbol_ptr                  ienumerable_sym;
    /* The type is a generic instance that is a member of a namespace.
       See if this type is an instance of the generic IEnumerable. */
    ienumerable_sym = cli_symbol_from_kind(
                                  csk_system_collections_generic_ienumerable);
    check_assertion(ienumerable_sym != NULL &&
                   ienumerable_sym->kind == (a_symbol_kind)sk_class_template);
    if (cssp->class_template == ienumerable_sym) {
      /* Yes, this is an instance of the generic IEnumerable. */
      if (elem_type == NULL) {
        is_instance = TRUE;
      } else {
        /* Check for a specific instance of the generic. */
        a_template_arg_ptr tap = class_type_supp(type)->template_arg_list;
        if (tap != NULL && tap->next == NULL && is_type_templ_arg(tap)) {
          /* The generic has a single type argument. */
          a_type_ptr arg_type = tap->variant.type;
          if (identical_types(elem_type, arg_type) ||
              (is_handle_type(arg_type) &&
               impl_handle_conversion(elem_type, arg_type,
                                      /*allow_qualifier_of_eh_mismatch=*/FALSE,
                                      (a_std_conv_descr *)NULL))) {
            is_instance = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return is_instance;
}  /* is_generic_cli_ienumerable_type */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void enter_symbol_for_namespace(a_symbol_ptr      sym,
                                       a_symbol_locator  *locator)
/*
The namespace given by sym was predeclared: its symbol was created but wasn't
added to the symbol table.  Do that now.  *locator indicates where a
declaration of the namespace was encountered in the source.
*/
{
  a_boolean  suppress_error = FALSE;

  /* Be sure the symbol hasn't already been entered. */
  check_assertion(sym->decl_position.seq == 0);
  /* Update the source position in the existing symbol. */
  sym->decl_position = locator->source_position;
  locator->specific_symbol = sym;
  /* Add the symbol to the proper scope's symbol list. */
  add_symbol_to_scope_list(sym, decl_scope_level, &suppress_error);
  /* Add the symbol to the symbol table. */
  link_symbol_into_symbol_table(sym, depth_scope_stack,
                                suppress_error);
}  /* enter_symbol_for_namespace */


void enter_symbol_for_namespace_std(a_symbol_locator  *locator)
/*
Namespace std was predeclared: Its symbol was created but wasn't added to
the symbol table.  Enter it now, if it has not already been entered.  *locator
indicates where a declaration of namespace std was encountered in the source.
*/
{
  if (!symbol_for_namespace_std_entered) {
    Value_saver<a_scope_depth>  saver(&decl_scope_level, DEPTH_OF_FILE_SCOPE);
    enter_symbol_for_namespace(symbol_for_namespace_std, locator);
    symbol_for_namespace_std_entered = TRUE;
  }  /* if */
}  /* enter_symbol_for_namespace_std */


void enter_symbol_for_namespace_std_meta(a_symbol_locator  *locator)
/*
Namespace std::meta was predeclared: Its symbol was created but wasn't added
to the symbol table.  Enter it now, if it has not already been entered.
*locator indicates where a declaration of namespace std::meta was encountered
in the source.
*/
{
  if (!symbol_for_namespace_std_meta_entered) {
    check_assertion(depth_scope_stack != DEPTH_OF_FILE_SCOPE);
    enter_symbol_for_namespace(symbol_for_namespace_std_meta, locator);
    symbol_for_namespace_std_meta_entered = TRUE;
  }  /* if */
}  /* enter_symbol_for_namespace_std_meta */

#if IA64_ABI

void enter_symbol_for_namespace_abi(a_symbol_locator  *locator)
/*
Namespace abi was predeclared: its symbol was created but wasn't added to
the symbol table.  Do that now.  *locator indicates where a declaration of
namespace abi was encountered in the source.
*/
{
  check_assertion(depth_scope_stack == DEPTH_OF_FILE_SCOPE);
  enter_symbol_for_namespace(symbol_for_namespace_abi, locator);
}  /* enter_symbol_for_namespace_abi */

#endif /* IA64_ABI */

a_symbol_ptr look_up_name_string_in_std(a_const_char  *name)
/*
Look up name in namespace std and return the symbol found, if any.
*/
{
  a_namespace_ptr  std_nsp;
  a_symbol_ptr     result_sym = NULL;

  if (symbol_for_namespace_std != NULL) {
    if (!symbol_for_namespace_std_entered) {
      /* If "std" hasn't been entered into the symbol table, make sure that
         it is entered now. */
      a_symbol_locator  loc;
      clear_locator(&loc, &null_source_position);
      (void)find_symbol("std", (sizeof_t)strlen("std"), &loc);
      enter_symbol_for_namespace_std(&loc);
      /* Force a lookup of "std" to make sure that any lazily-loaded symbols
         (from a module file) are primed. */
      clear_specific_symbol(loc);
      (void)file_scope_id_lookup(scope_stack[DEPTH_OF_FILE_SCOPE].il_scope,
                                 &loc, IDL_MUST_BE_NAMESPACE);
    } else if (symbol_for_namespace_std->header
                                           ->deferred_module_entries != NULL &&
               lazy_symbols_may_be_visible) {
      /* Namespace std was already entered, but module files contain some
         pending "std" declarations.  Make sure those are visible to lookup. */
      define_names_from_scope(il_header.primary_scope,
                              symbol_for_namespace_std->header);
    }  /* if */
    std_nsp = symbol_for_namespace_std->variant.namespace_info.ptr;
    if (std_nsp != NULL) {
      result_sym = look_up_name_string_in_namespace(
                                               name, std_nsp, IDL_NO_OPTIONS);
    }  /* if */
  }  /* if */
  return result_sym;
}  /* look_up_name_string_in_std */


a_boolean is_member_of_namespace(a_symbol_ptr  sym,
                                 a_symbol_ptr  ns_sym)
/*
Return TRUE if the sym represents a member of the namespace indicated by
ns_sym, ignoring any directly-enclosing inline namespaces.
*/
{
  a_boolean  result = FALSE;

  if (sym_is_namespace_member(sym)) {
    a_namespace_ptr  nsp = sym_parent_namespace(sym);
    while (nsp->is_inline) {
      nsp = parent_namespace_or_null(nsp);
      if (nsp == NULL) goto done;
    }  /* if */
    if (nsp == ns_sym->variant.namespace_info.ptr) {
      result = TRUE;
    }  /* if */
  }  /* if */
done:
  return result;
}  /* is_member_of_namespace */


a_symbol_ptr look_up_name_string_in_class(
                                        a_const_char             *symbol_name,
                                        a_type_ptr               class_type,
                                        an_id_lookup_options_set options)
/*
Look up symbol_name in the specified class type.  Return the symbol found, if
any.
*/
{
  a_symbol_locator loc;
  a_symbol_ptr     sym;

  clear_locator(&loc, &null_source_position);
  (void)find_symbol(symbol_name, (sizeof_t)strlen(symbol_name), &loc);
  complete_type_is_needed(class_type);
  sym = class_qualified_id_lookup(&loc, class_type, options);
  return sym;
}  /* look_up_name_string_in_class */


a_symbol_ptr look_up_class_template_in_std(a_const_char  *ctname)
/*
Look up a class template of the given name in namespace std and return its
associated symbol, or NULL if it is not found.
*/
{
  a_symbol_ptr     result_sym = look_up_name_string_in_std(ctname);

  if (result_sym != NULL && !symbol_is(result_sym, sk_class_template)) {
    result_sym = NULL;
  }  /* if */
  return result_sym;
}  /* look_up_class_template_in_std */


static a_symbol_ptr look_up_coroutine_class_template(a_const_char *ctname)
/*
Look up the coroutine class template of the given name in the namespace pointed
to by namespace_for_coroutine_types.  If this is NULL, look up the class
template in namespace std or std::experimental and store the associated
namespace in namespace_for_coroutine_types to ensure all future lookups for
coroutine types occur in the same namespace.  Return the symbol associated with
the coroutine class template, or NULL if it is not found.
*/
{
  a_symbol_ptr result_sym = NULL;

  if (namespace_for_coroutine_types == NULL &&
      symbol_for_namespace_std != NULL) {
    a_namespace_ptr ns = symbol_for_namespace_std->variant.namespace_info.ptr;
    if (ns != NULL) {
      a_symbol_ptr sym = look_up_name_string_in_namespace(ctname, ns,
                                                          IDL_NO_OPTIONS);
      if (sym != NULL && symbol_is(sym, sk_class_template)) {
        namespace_for_coroutine_types = ns;
      } else {
        sym = look_up_name_string_in_namespace("experimental", ns,
                                               IDL_NO_OPTIONS);
        if (sym != NULL && symbol_is(sym, sk_namespace)) {
          ns = sym->variant.namespace_info.ptr;
          sym = look_up_name_string_in_namespace(ctname, ns, IDL_NO_OPTIONS);
          if (sym != NULL && symbol_is(sym, sk_class_template)) {
            namespace_for_coroutine_types = ns;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (namespace_for_coroutine_types != NULL) {
    result_sym =
        look_up_name_string_in_namespace(ctname, namespace_for_coroutine_types,
                                         IDL_NO_OPTIONS);
  }  /* if */
  return result_sym;
}  /* look_up_coroutine_class_template */


static a_type_ptr instantiate_coroutine_class_template_with_one_type(
                                                         a_const_char  *ctname,
                                                         a_type_ptr    type,
                                                         an_error_code *err)
/*
Return the coroutine type corresponding to
	std[::experimental]::xyz<T>
where xyz is a class template described by ctname, and T is the given type.
Return an error type if there is no such class template or if its instantiation
is unsuccessful.  If err is non-NULL, set it to an appropriate error code
describing the failure, or ec_no_error if there is no failure.
*/
{
  a_symbol_ptr  class_template;
  a_type_ptr    result;

  if (err != NULL) {
    *err = ec_no_error;
  }  /* if */
  class_template = look_up_coroutine_class_template(ctname);
  if (class_template != NULL) {
    a_template_arg_ptr  tap = alloc_template_arg((a_templ_arg_kind)tak_type);
    a_symbol_ptr        instance;
    tap->variant.type = type;
    tap->explicitly_specified = TRUE;
    instance = find_class_template_instance(class_template, &tap);
    free_template_arg_list(tap);
    if (instance == NULL || !is_type_symbol(instance)) {
      result = error_type();
    } else {
      result = type_symbol_type(instance);
    }  /* if */
  } else {
    if (err != NULL) {
      *err = ec_special_class_template_not_found;
    }  /* if */
    result = error_type();
  }  /* if */
  return result;
}  /* instantiate_coroutine_class_template_with_one_type */


static void get_coroutine_parameter_variables(a_routine_ptr        coroutine,
                                              an_arg_list_elem_ptr *alep)
/*
Create an argument list containing the parameters (including the implicit
"this" parameter if appropriate) of the given coroutine, returning them
in alep.  The caller is responsible for freeing the created argument list.
*/
{
  an_arg_list_elem_ptr *next_alep = alep;
  a_variable_ptr       rout_param_var;
  an_expr_node_ptr     var_expr;
  an_operand           arg_operand;
  a_scope_ptr          sp = scope_for_routine(coroutine);

  *alep = NULL;
  if (sp->variant.routine.this_param_variable != NULL) {
    var_expr = var_rvalue_expr(sp->variant.routine.this_param_variable);
    make_expression_operand(var_expr, &arg_operand);
    *alep = alloc_arg_list_elem_for_operand(&arg_operand);
    next_alep = &(*alep)->next;
  }  /* if */
  for (rout_param_var = sp->variant.routine.parameters;
       rout_param_var != NULL; rout_param_var = rout_param_var->next) {
    var_expr = var_rvalue_expr(rout_param_var);
    if (is_reference_type(var_expr->type)) {
      var_expr = add_ref_indirection_to_node(var_expr);
    }  /* if */
    make_lvalue_or_rvalue_expression_operand(var_expr, &arg_operand);
    *next_alep = alloc_arg_list_elem_for_operand(&arg_operand);
    next_alep = &(*next_alep)->next;
  }  /* for */
}  /* get_coroutine_parameter_variables */


static void initialize_coroutine_promise_variable(a_variable_ptr promise,
                                                  a_routine_ptr  coroutine)
/*
Generate the initializer for a coroutine promise variable in promise.  The
initializer for a promise variable is either the constructor that takes all the
function parameters of coroutine (including the implicit "this" for non-static
member functions), or the default constructor.
*/
{
  an_arg_list_elem_ptr alep = NULL;
  a_symbol_ptr         ctor_sym = NULL;
  a_source_position    *pos = &promise->source_corresp.decl_position;
  a_dynamic_init_ptr   dip = NULL;
  an_expr_stack_entry  expr_stack_entry, *saved_expr_stack = expr_stack;
  a_boolean            saved_suppress_diagnostics;

  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  expr_stack->in_coroutine_desc_init = TRUE;
  ctor_sym = symbol_supplement_for_class(promise->type)->constructor;
  if (ctor_sym != NULL) {
    saved_suppress_diagnostics = expr_stack->suppress_diagnostics;
    expr_stack->suppress_diagnostics = TRUE;
    get_coroutine_parameter_variables(coroutine, &alep);
    scan_ctor_arguments(ctor_sym, pos,
                        /*object_class_type=*/NULL,
                        /*dest_type=*/NULL,
                        /*fill_in_dtor=*/FALSE,
                        /*elision_allowed=*/FALSE,
                        /*is_custom_ms_attr_arg_list=*/FALSE,
                        CCO_INITIALIZING_VARIABLE,
                        (a_rescan_control_block*)NULL,
                        /*arg_list_supplied=*/TRUE,
                        alep,
                        /*init_list_ctor_arg_list=*/NULL,
                        /*trivial_ctor=*/NULL,
                        /*explicit_ctor=*/NULL,
                        /*elision_done=*/NULL,
                        /*unboxing_conv=*/NULL,
                        /*string_ctor_skip=*/NULL,
                        /*simple_result=*/NULL,
                        &dip,
                        (an_expr_node_ptr*)NULL,
                        /*closing_paren_position=*/NULL);
    expr_stack->suppress_diagnostics = saved_suppress_diagnostics;
    if (expr_stack->any_suppressed_error) {
      dip = NULL;
    } else if (dip != NULL && dip->kind == dik_constructor) {
      if (dip->variant.constructor.ptr != NULL) {
        /* Mark the constructor as referenced as scan_ctor_arguments does not
           do that when suppressing diagnostics. */
        mark_routine_referenced(dip->variant.constructor.ptr);
      }  /* if */
    }  /* if */
  }  /* if */
  if (dip == NULL) {
    a_boolean     def_ctor_err;
    a_routine_ptr ctor_routine;
    /* We failed to find a matching constructor for all arguments, try the
       default constructor. */
    ctor_routine = expr_select_default_constructor(promise->type,
                                                   pos,
                                                   &def_ctor_err);
    if (!def_ctor_err && ctor_routine != NULL) {
      /* A non-trivial constructor. */
      dip = alloc_expr_ctor_dynamic_init(ctor_routine,
                                         (an_expr_node_ptr)NULL,
                                         /*dest_type=*/NULL,
                                         /*static_temp=*/FALSE,
                                         /*add_default_args=*/TRUE,
                                         /*implied_source=*/FALSE,
                                         /*value_init=*/FALSE,
                                         /*sequenced_args=*/FALSE,
                                         /*fold_constexpr=*/TRUE,
                                         /*check_constexpr=*/FALSE,
                                         pos);
    } else {
      dip = alloc_expr_dynamic_init((a_dynamic_init_kind)dik_none);
    }  /* if */
  }  /* if */
  wrap_up_dynamic_init_full_expression(dip);
  add_dtor_to_dynamic_init(dip, promise->type, promise->type, pos);
  dip->variable = promise;
  promise->init_kind = (an_init_kind)initk_dynamic;
  promise->initializer.dynamic = dip;
  free_arg_list(alep);
  pop_expr_stack();
  expr_stack = saved_expr_stack;
}  /* initialize_coroutine_promise_variable */


static void make_coroutine_promise_call_operand(an_operand        *result,
                                                a_const_char      *func_name,
                                                a_variable_ptr    promise_var,
                                                a_boolean         add_await,
                                                a_boolean         init_suspend)
/*
Create a call to promise_var's given member function, returning the resulting
operand in result.  If add_await is TRUE, treat it as if the call was preceded
by "co_await".  If init_suspend is TRUE, this is the call to "initial_suspend".
*/
{
  an_operand          promise_operand;
  a_source_position   *pos = &promise_var->source_corresp.decl_position;
  an_expr_stack_entry expr_stack_entry, *saved_expr_stack = expr_stack;

  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  make_lvalue_variable_operand(promise_var, pos, pos, &promise_operand,
                               (a_ref_entry *)NULL);
  call_named_member_function(&promise_operand, func_name,
                             (a_template_arg_ptr)NULL,
                             (an_arg_list_elem_ptr)NULL,
                             &promise_operand, result);
  if (add_await && !is_error_operand(result)) {
    add_await_to_operand(result, pos, NO_TOKEN_SEQUENCE_NUMBER,
                         /*for_yield=*/FALSE, /*generated_suspend_point=*/TRUE,
                         /*initial_suspend_point=*/init_suspend);
  }
  pop_expr_stack();
  expr_stack = saved_expr_stack;
}  /* make_coroutine_promise_call_operand */


static a_symbol_ptr select_coroutine_new(a_symbol_ptr       new_sym,
                                         a_routine_ptr      coroutine,
                                         a_source_position* pos,
                                         a_boolean          use_nothrow_new)
/*
Select the appropriate "new" operator for the coroutine.  new_sym is the
(possibly overloaded) symbol for operator "new" in the scope of the promise
type.  If new_sym is non-NULL, attempt to find an overload that takes all the
parameters of the provided coroutine as arguments.  If none is found, use the
operator "new" that only takes a size_t.  If new_sym is NULL, use the global
operator "new" that takes a size_t (or the nothrow_t variant if use_nothrow_new
is TRUE).  pos is the position at which this call is ostensibly taking place
and is where diagnostics will be issued.  Return the selected operator "new"
or NULL if no appropriate symbol could be found.
*/
{
  an_expr_node_ptr     size_t_expr;
  an_operand           size_t_operand;
  an_arg_list_elem_ptr size_t_arg_alep;

  size_t_expr = node_for_host_large_integer((a_host_large_integer)0,
                                            targ_size_t_int_kind);
  make_expression_operand(size_t_expr, &size_t_operand);
  size_t_arg_alep = alloc_arg_list_elem_for_operand(&size_t_operand);
  if (new_sym != NULL) {
    an_arg_list_elem_ptr     alep;
    an_arg_match_summary_ptr arg_match_list;
    new_sym = fundamental_symbol_of(new_sym);
    get_coroutine_parameter_variables(coroutine, &alep);
    size_t_arg_alep->next = alep;
    if (!overloaded_function_match_possible(new_sym,
                                            oc_new_expression,
                                            /*is_template_id=*/FALSE,
                                            /*template_arg_list=*/NULL,
                                            size_t_arg_alep,
                                            /*have_selector=*/FALSE,
                                            /*bound_function_selector*/NULL)) {
      /* Free the arguments after size_t and try to resolve that one. */
      free_arg_list(alep);
      size_t_arg_alep->next = NULL;
    }  /* if */
    new_sym = select_overloaded_function(new_sym,
                                         /*is_template_id=*/FALSE,
                                         /*template_arg_list=*/NULL,
                                         /*have_selector=*/FALSE,
                                         /*bound_function_selector=*/NULL,
                                         size_t_arg_alep,
                                         /*init_list_ctor_arg_list=*/NULL,
                                         CCO_DEFAULT,
                                         /*do_arg_dep_lookup=*/FALSE,
                                         /*use_pure_arg_dep_lookup=*/FALSE,
                                         /*use_std_for_arg_dep_lookup=*/FALSE,
                                         oc_new_expression,
                                         pos,
                                         NO_TOKEN_SEQUENCE_NUMBER,
                                         /*single_function=*/NULL,
                                         /*init_list_ctor_case=*/NULL,
                                         /*unknown_dependent_function=*/NULL,
                                         /*found_through_adl=*/NULL,
                                         /*surrogate_function_conv_sym=*/NULL,
                                         &arg_match_list);
  } else {
    /* Using global operator new. */
    new_sym = opname_function_symbol((an_opname_kind)onk_new);
    new_sym = fundamental_symbol_of(new_sym);
    if (new_sym->kind == (a_symbol_kind)sk_overloaded_function) {
      new_sym = new_sym->variant.overloaded_function.symbols;
    }  /* if */
    for (; new_sym != NULL; new_sym = new_sym->next) {
      a_type_ptr       rout_type = func_sym_routine(new_sym)->type;
      a_param_type_ptr params = rout_type->variant.routine.extra_info
                                                             ->param_type_list;
      if (params == NULL) continue;
      if (use_nothrow_new) {
        if (params->next != NULL && params->next->next == NULL &&
            is_new_nothrow_param(params->next)) {
          break;
        }  /* if */
      } else {
        if (params->next == NULL) break;
      }  /* if */
    }  /* for */
    if (new_sym == NULL) {
      pos_error(ec_no_nothrow_global_new_for_coroutine, pos);
    }  /* if */
  }  /* if */
  if (new_sym != NULL) {
    record_symbol_reference(SRK_REFERENCE, new_sym, pos,
                            /*update_il_entry=*/FALSE);
  }  /* if */
  free_arg_list(size_t_arg_alep);
  return new_sym;
}  /* select_coroutine_new */


static a_symbol_ptr select_coroutine_delete(a_symbol_ptr      del_sym,
                                            a_source_position *pos)
/*
Select the deallocation function for the coroutine.  del_sym is the (possibly
overloaded) symbol for operator "delete" within the scope of the promise type.
If del_sym is NULL, use the global operator "delete".  Of these symbols,
select the usual deallocation function that takes both a pointer parameter and
a size parameter.  If this is not found, select the usual deallocation function
that takes just a pointer parameter.  pos is the position at which this call is
ostensibly taking place and is where diagnostics will be issued.  Return the
selected operator "delete" or NULL if no appropriate symbol could be found.
*/
{
  a_boolean is_sized_ver, is_aligned_ver, is_destroying_delete;

  if (del_sym == NULL) {
    del_sym = opname_function_symbol((an_opname_kind)onk_delete);
  }  /* if */
  del_sym = fundamental_symbol_of(del_sym);
  if (del_sym->kind == (a_symbol_kind)sk_overloaded_function) {
    a_symbol_ptr single_arg_delete = NULL;
    for (del_sym = del_sym->variant.overloaded_function.symbols;
         del_sym != NULL; del_sym = del_sym->next) {
      if (is_default_operator_delete(func_sym_routine(del_sym),
                                     &is_sized_ver, &is_aligned_ver,
                                     &is_destroying_delete) &&
          !is_aligned_ver) {
        /* If this is the version with a size_t argument, we have the symbol
           we want.  Otherwise, this is the single-arg version - save it and
           continue looking. */
        if (is_sized_ver) break;
        single_arg_delete = del_sym;
      }  /* if */
    }  /* for */
    if (del_sym == NULL) {
      del_sym = single_arg_delete;
    }  /* if */
  } else if (is_function_or_template_symbol(del_sym)) {
    if (!is_default_operator_delete(func_sym_routine(del_sym),
                                    &is_sized_ver, &is_aligned_ver,
                                    &is_destroying_delete) ||
        is_aligned_ver) {
      /* Either this isn't a usual deallocation function or it's the aligned
         version.  Neither case is acceptable. */
      del_sym = NULL;
    }  /* if */
  } else {
    /* Not a function symbol. */
    del_sym = NULL;
  }  /* if */
  if (del_sym == NULL) {
    pos_error(ec_no_viable_delete_for_coroutine, pos);
  }  /* if */
  if (del_sym != NULL) {
    record_symbol_reference(SRK_REFERENCE, del_sym, pos,
                            /*update_il_entry=*/FALSE);
  }  /* if */
  return del_sym;
}  /* select_coroutine_delete */


static a_label_ptr make_coroutine_final_suspend_label(void)
/*
Create the label that will be used as the final suspend label.  Later, when the
coroutine is being wrapped up, a statement for the label will be generated.
*/
{
  a_label_ptr            label;
  a_memory_region_number region_to_switch_back_to;

  switch_to_scope_region(depth_innermost_function_scope,
                         &region_to_switch_back_to);
  label = alloc_label();
  switch_back_to_original_region(region_to_switch_back_to);
  add_to_labels_list(label);
  return label;
}  /* make_coroutine_final_suspend_label */


static void select_coroutine_new_delete(a_coroutine_descr_ptr cr_desc,
                                        a_routine_ptr         coroutine)
/*
A coroutine implementation may require run-time allocation of storage.  Resolve
the appropriate calls to the allocation/deallocation routines.  As part of
this, determine whether "get_return_object_on_allocation_failure" is defined
and if so, resolve and record the appropriate call.
*/
{
  a_variable_ptr       promise_var = cr_desc->promise;
  a_type_ptr           promise_type = promise_var->type;
  a_source_position    *pos = &cr_desc->position;
  a_symbol_ptr         new_sym, del_sym, alloc_fail_sym;
  an_expr_stack_entry  expr_stack_entry, *saved_expr_stack = expr_stack;

  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  new_sym = opname_member_function_symbol((an_opname_kind)onk_new,
                                          promise_type);
  del_sym = opname_member_function_symbol((an_opname_kind)onk_delete,
                                          promise_type);
  alloc_fail_sym =
        look_up_name_string_in_class("get_return_object_on_allocation_failure",
                                     promise_type,
                                     IDL_DO_NOT_ADD_TO_NONREAL_CLASS);
  new_sym = select_coroutine_new(new_sym, coroutine, pos,
                                 /*use_nothrow_new=*/alloc_fail_sym != NULL);
  if (new_sym != NULL) {
    cr_desc->new_routine = func_sym_routine(new_sym);
  }  /* if */
  del_sym = select_coroutine_delete(del_sym, pos);
  if (del_sym != NULL) {
    cr_desc->delete_routine = func_sym_routine(del_sym);
  }  /* if */
  if (alloc_fail_sym != NULL) {
    an_operand         operand;
    a_dynamic_init_ptr dip;
    an_expr_node_ptr   rout_node = NULL;
    a_type_ptr         rout_type = NULL;
    a_symbol_ptr       sym;
    a_boolean          ambiguous = FALSE;
    if (symbol_is(alloc_fail_sym, sk_overloaded_function)) {
      alloc_fail_sym = alloc_fail_sym->variant.overloaded_function.symbols;
    }  /* if */
    for (sym = alloc_fail_sym; sym != NULL; sym = sym->next) {
      if (symbol_is(sym, sk_member_function)) {
        rout_type = routine_symbol_type(sym);
        if (!rout_type_supp(rout_type)->has_this_param &&
            (rout_type_supp(rout_type)->param_type_list == NULL ||
             rout_type_supp(rout_type)->param_type_list->has_default_arg)) {
          /* This routine is a static member function that can be called with
             no arguments. */
          if (rout_node != NULL) {
            ambiguous = TRUE;
          } else {
            rout_node = function_rvalue_expr(func_sym_routine(sym));
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
    if (rout_node == NULL || ambiguous) {
      type_error(ec_bad_gro_on_alloc_fail, promise_type);
    } else {
      make_function_call(rout_node, rout_type, /*is_virtual=*/FALSE,
                         /*virtual_suppressed=*/FALSE,
                         /*selector_is_object_pointer=*/FALSE,
                         /*compiler_generated=*/TRUE, /*is_conversion=*/FALSE,
                         /*arg_dep_lookup_suppressed=*/FALSE,
                         /*qualified_function_name=*/TRUE,
                         /*found_through_adl=*/FALSE,
                         /*uses_operator_syntax=*/FALSE,
                         /*start_position=*/&null_source_position,
                         /*operator_position=*/pos,
                         /*end_position=*/&null_source_position,
                         &operand, /*p_folded=*/NULL,
                         /*p_function_call_node=*/NULL);
      if (is_class_struct_union_type(coroutine->type->
                                                variant.routine.return_type)) {
        prep_elision_initializer_operand(&operand, coroutine->type->
                                                   variant.routine.return_type,
                                         /*fill_in_dtor=*/FALSE,
                                         CCO_INITIALIZING_RETURN_VALUE,
                                         ec_bad_return_value_type,
                                         /*elision_done=*/NULL,
                                         &dip);
      }  /* if */
      cr_desc->alloc_failure_gro_call = expr_node_from_operand(&operand);
      set_possibly_null_expr_result_not_used(cr_desc->alloc_failure_gro_call);
    }  /* if */
  }  /* if */
  pop_expr_stack();
  expr_stack = saved_expr_stack;
}  /* select_coroutine_new_delete */


static void prepare_coroutine_calls(a_coroutine_descr_ptr cr_desc,
                                    a_routine_ptr         coroutine)
/*
Prepare the calls that may be required for the set-up/tear-down phases of the
coroutine as described in N4810 (or N4775+P0912R5).
*/
{
  a_variable_ptr      promise_var = cr_desc->promise;
  an_operand          operand;
  a_dynamic_init_ptr  dip;
  an_expr_stack_entry expr_stack_entry, *saved_expr_stack = expr_stack;
  a_type_ptr          return_type;
  an_object_lifetime  *saved_curr_object_lifetime = curr_object_lifetime,
                      *func_olp = curr_object_lifetime;
  a_dynamic_init      *saved_destructions, **p_link;

  /* We're about to construct some expressions to set up a coroutine and those
     might cause exceptions to be thrown.  However, we may not be at the start 
     of the coroutine (calling this function may, e.g., be triggered when
     encountering a co_return statement).  Temporarily "reset" the object
     lifetime state to that at the start of the coroutine by (a) making the
     function's root lifetime object be the current object lifetime, and (b)
     unlinking the destructions that have been scheduled in that lifetime. */
  check_assertion(func_olp != NULL);
  for (;; func_olp = func_olp->parent_lifetime) {
    if (func_olp->kind == olk_block &&
        func_olp->entity.kind == iek_scope) {
      if (scope_is((a_scope*)func_olp->entity.ptr, sck_function)) {
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  saved_destructions = func_olp->destructions;
  func_olp->destructions = NULL;
  curr_object_lifetime = func_olp;
  initialize_coroutine_promise_variable(promise_var, coroutine);

  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  expr_stack->in_coroutine_desc_init = TRUE;
  /* Resolve the needed calls that use the promise variable. */
  make_coroutine_promise_call_operand(&operand, "initial_suspend",
                                      promise_var, /*add_await=*/TRUE,
                                      /*init_suspend=*/TRUE);
  if (is_error_operand(&operand)) goto done;
  cr_desc->initial_suspend_call = full_expr_from_operand(&operand);
  set_possibly_null_expr_result_not_used(cr_desc->initial_suspend_call);
  pop_expr_stack();

  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  expr_stack->in_coroutine_desc_init = TRUE;
  make_coroutine_promise_call_operand(&operand, "final_suspend",
                                      promise_var, /*add_await=*/TRUE,
                                      /*init_suspend=*/FALSE);
  if (is_error_operand(&operand)) goto done;
  cr_desc->final_suspend_call = full_expr_from_operand(&operand);
  set_possibly_null_expr_result_not_used(cr_desc->final_suspend_call);
  if (cr_desc->final_suspend_call != NULL &&
      !is_error_node(cr_desc->final_suspend_call) &&
      expr_might_throw(cr_desc->final_suspend_call)) {
    /* Calling final_suspend() on the promise cannot throw (see N4810
       [dcl.fct.def.coroutine]/15). */
    a_symbol_locator loc;
    a_symbol_ptr     sym = look_up_named_member_function(promise_var->type,
                                                         "final_suspend",
                                                         &loc);
    check_assertion(sym != NULL);
    pos_sy_error(ec_final_suspend_cannot_throw, &sym->decl_position, sym);
  }  /* if */
  pop_expr_stack();

  if (exceptions_enabled) {
    push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry,
                    /*force_object_lifetime=*/FALSE,
                    /*suppress_object_lifetime=*/FALSE);
    expr_stack->in_coroutine_desc_init = TRUE;
    make_coroutine_promise_call_operand(&operand, "unhandled_exception",
                                        promise_var, /*add_await=*/FALSE,
                                        /*init_suspend=*/FALSE);
    if (is_error_operand(&operand)) goto done;
    cr_desc->unhandled_exception_call = full_expr_from_operand(&operand);
    set_possibly_null_expr_result_not_used(cr_desc->unhandled_exception_call);
    pop_expr_stack();
  }  /* if */
  /* Resolve the call to p.get_return_object and convert it to the return type
     of the coroutine. */
  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  expr_stack->in_coroutine_desc_init = TRUE;
  make_coroutine_promise_call_operand(&operand, "get_return_object",
                                      promise_var, /*add_await=*/FALSE,
                                      /*init_suspend=*/FALSE);
  return_type = coroutine->type->variant.routine.return_type;
  if (is_class_struct_union_type(return_type)) {
    prep_elision_initializer_operand(&operand, return_type,
                                     /*fill_in_dtor=*/FALSE,
                                     CCO_INITIALIZING_RETURN_VALUE,
                                     ec_bad_return_value_type,
                                     /*elision_done=*/NULL,
                                     &dip);
  } else if (!is_void_type(return_type)) {
    prep_initializer_operand(&operand, return_type, /*is_transparent=*/NULL,
                             /*conversion=*/NULL,
                             /*is_copy_initialization=*/TRUE,
                             CCO_INITIALIZING_RETURN_VALUE,
                             ec_bad_return_value_type);
  }  /* if */
  cr_desc->get_return_object_call = full_expr_from_operand(&operand);
  set_possibly_null_expr_result_not_used(cr_desc->get_return_object_call);
  pop_expr_stack();

  select_coroutine_new_delete(cr_desc, coroutine);
done:
  if (expr_stack == &expr_stack_entry) {
    /* An error situation caused an early exit, which bypassed popping the
       expression stack. */
    pop_expr_stack();
  }  /* if */
  /* Restore the object lifetime state to what it was on entry to this
     function.  The destructions that were previously unlinked are re-linked
     at the end of the "destructions" list that they were on previously. */
  p_link = &func_olp->destructions;
  while (*p_link != NULL) p_link = &(*p_link)->next_in_destruction_list;
  *p_link = saved_destructions;
  curr_object_lifetime = saved_curr_object_lifetime;
  expr_stack = saved_expr_stack;
}  /* prepare_coroutine_calls */


void init_coroutine_descr(a_routine_ptr          rp,
                          a_coroutine_descr_ptr  cdp)
/*
Initialize some basic fields of the given coroutine description (associated
with the given coroutine).

Specifically, record in cdp->traits the traits type instance
	std[::experimental]::coroutine_traits<R, P1, P2, ...>
and in cdp->promise record a new variable of type traits::promise_type.
(R is the return type of rp and P1, P2, ... are the parameter types of rp; for
a nonstatic member function, P1 is the type of this.)
*/
{
  a_symbol_ptr           traits_sym = NULL, traits_inst_sym = NULL,
                         promise_sym;
  a_type_ptr             traits = NULL, promise_type = NULL, handle_type,
                         rtp = skip_typerefs(rp->type);
  a_template_arg_ptr     tap_list, *p_tap;
  a_param_type_ptr       ptp;

  check_assertion(rp->is_coroutine && cdp != NULL);
  /* First look up std[::experimental]::coroutine_traits. */
  traits_sym = look_up_coroutine_class_template("coroutine_traits");
  if (rp->has_deducible_return_type) {
    rp->type->variant.routine.return_type = error_type();
    rp->has_deduced_return_type = TRUE;
    cdp->error_descr = TRUE;
    pos_error(ec_coroutine_with_deduced_return_type, &cdp->position);
  }  /* if */
  if (cdp->error_descr) {
    expect_error();
  } else if (traits_sym == NULL) {
    pos_st_error(ec_special_class_template_not_found, &cdp->position,
                 "std::coroutine_traits");
    cdp->error_descr = TRUE;
  } else {
    /* Now instantiate coroutine_traits<R, P1, P2, ...> where R is the return
       type of rp, and P1, P2, ... its parameters types. */
    tap_list = alloc_template_arg((a_templ_arg_kind)tak_type);
    tap_list->variant.type = rtp->variant.routine.return_type;
    tap_list->explicitly_specified = TRUE;
    p_tap = &tap_list->next;
    if (routine_type_is_nonstatic_member_function(rtp)) {
      *p_tap = alloc_template_arg((a_templ_arg_kind)tak_type);
      (*p_tap)->variant.type = f_implicit_this_param_type_of(rtp);
      (*p_tap)->explicitly_specified = TRUE;
      p_tap = &(*p_tap)->next;
    }  /* if */
    for (ptp = function_type_params(rtp); ptp != NULL; ptp = ptp->next) {
      *p_tap = alloc_template_arg((a_templ_arg_kind)tak_type);
      (*p_tap)->variant.type = ptp->type;
      (*p_tap)->explicitly_specified = TRUE;
      p_tap = &(*p_tap)->next;
    }  /* for */ 
    traits_inst_sym = find_class_template_instance(traits_sym, &tap_list);
    free_template_arg_list(tap_list);
    if (traits_inst_sym == NULL || !is_type_symbol(traits_inst_sym)) {
      expect_error();
      traits = NULL;
    } else {
      traits = type_symbol_type(traits_inst_sym);
    }  /* if */
  }  /* if */
  if (traits != NULL) {
    /* Retrieve the promise type from the traits instantiation. */
    promise_sym = look_up_name_string_in_class("promise_type", traits,
                                               IDL_TYPENAME_LOOKUP);
    if (promise_sym == NULL || !is_type_symbol(promise_sym)) {
      pos_stsy_error(ec_not_a_member, &cdp->position, "promise_type",
                     traits_inst_sym);
      promise_type = error_type();
    } else {
      promise_type = type_symbol_type(promise_sym);
    }  /* if */
  } else {
    expect_error();
    promise_type = error_type();
  }  /* if */
  cdp->traits = (traits == NULL) ? error_type() : traits;
  cdp->promise = make_variable(promise_type, (a_storage_class)sc_auto,
                               NO_SCOPE_DEPTH);
  cdp->promise->source_corresp.decl_position=rp->source_corresp.decl_position;
  cdp->promise->source_corresp.is_local_to_function = TRUE;
  { an_error_code err_code;
    /* Create a placeholder variable for the coroutine "handle". */
    handle_type = instantiate_coroutine_class_template_with_one_type(
                                                            "coroutine_handle",
                                                            promise_type,
                                                            &err_code);
    if (err_code != ec_no_error) {
      pos_st_error(err_code, &cdp->position, "std::coroutine_handle");
    }  /* if */
    cdp->handle = make_variable(handle_type, (a_storage_class)sc_auto,
                                NO_SCOPE_DEPTH);
    cdp->handle->source_corresp.decl_position=rp->source_corresp.decl_position;
    cdp->handle->source_corresp.is_local_to_function = TRUE;
  }
  /* Create a placeholder variable for "init-await-resume-called". */
  cdp->init_await_resume = make_variable(bool_type(), (a_storage_class)sc_auto,
                                         NO_SCOPE_DEPTH);
  cdp->init_await_resume->source_corresp.decl_position =
                                              rp->source_corresp.decl_position;
  cdp->init_await_resume->source_corresp.is_local_to_function = TRUE;
  cdp->init_await_resume->init_kind = (an_init_kind)initk_zero;
  if (is_error_type(promise_type) || is_error_type(handle_type)) {
    expect_error();
    cdp->error_descr = TRUE;
  }  /* if */
  /* Determine what type of return this coroutine uses. */
  if (!is_error_type(promise_type)) {
    a_symbol_ptr  rv_sym, rvoid_sym;
    rv_sym = look_up_name_string_in_class("return_value", promise_type,
                                          IDL_DO_NOT_ADD_TO_NONREAL_CLASS);
    rvoid_sym = look_up_name_string_in_class("return_void", promise_type,
                                             IDL_DO_NOT_ADD_TO_NONREAL_CLASS);
    if (rv_sym != NULL && rvoid_sym != NULL) {
      a_diagnostic_ptr dp;
      dp = pos_ty_start_error(ec_no_return_value_and_return_void,
                              &cdp->position, promise_type);
      add_diag_info_with_pos_insert(dp, ec_return_value_at,
                                    &rv_sym->decl_position);
      add_diag_info_with_pos_insert(dp, ec_return_void_at,
                                    &rvoid_sym->decl_position);
      end_diagnostic(dp);
      rv_sym = NULL;
    } else if (rv_sym == NULL) {
      rv_sym = rvoid_sym;
    }  /* if */
    if (rv_sym != NULL && is_member_function_symbol(rv_sym)) {
      cdp->has_return_void = rvoid_sym != NULL;
    }  /* if */
  }  /* if */
  if (!cdp->error_descr) {
    cdp->final_suspend_label = make_coroutine_final_suspend_label();
    if (!is_template_param_type(promise_type)) {
      /* We have a real promise type, so we can prepare the various calls that
         a coroutine requires. */
      prepare_coroutine_calls(cdp, rp);
    }  /* if */
  }  /* if */
}  /* init_coroutine_descr */

#if defined(GUARD_MACRO_FOR_VA_LIST) || defined(GUARD_MACRO2_FOR_VA_LIST)

static a_boolean define_guard_macro(a_const_char *macro_name)
/*
If the guard macro with the indicated name is not defined already, define
it and return FALSE.  If it is defined already, do nothing and return TRUE.
*/
{
  a_boolean        already_defined = FALSE;
  a_symbol_locator locator;
  a_symbol_ptr     macro_sym;

  macro_sym = find_macro_symbol_by_name(macro_name,
                                        (sizeof_t)(strlen(macro_name)),
                                        &locator);
  if (macro_sym != NULL) {
    /* The macro is defined already. */
    already_defined = TRUE;
  } else {
    /* The macro is not defined.  Define it. */
    (void)enter_predef_macro("1", macro_name,
                             /*cannot_be_redefined=*/FALSE,
                             /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
  return already_defined;
}  /* define_guard_macro */

#endif /* defined(GUARD_MACRO_FOR_VA_LIST) || ... */

void declare_builtin_va_list_type(a_boolean	is_cstdarg)
/*
Declare the type va_list when <stdarg.h> is treated as a builtin.  This is
called at the point where the #include <stdarg.h> appears.

is_cstdarg is TRUE in C++ mode if the header name was specified as
cstdarg, and FALSE if the header name was specified as stdarg.h.  In
some modes, use of stdarg.h causes va_list to be put into both the std
and global namespaces.
*/
{
  a_type_ptr       va_list_type, va_list_typedef;
  a_symbol_locator locator;
  a_namespace_ptr  std_namespace = NULL;
  a_boolean	   new_symbol_created = FALSE;

  if (builtin_va_list_type == NULL) {
    a_symbol_ptr     sym;
    if (va_list_in_std_namespace) {
      /* When the type is put in namespace std, the symbol for std
         should already exist. */
      check_assertion(symbol_for_namespace_std != NULL);
      std_namespace = symbol_for_namespace_std->variant.namespace_info.ptr;
    }  /* if */
    /* Look for an existing va_list symbol.  Such a symbol would exist
       if declared in other headers, e.g., stdio.h.  That would be
       nonstandard, but we accommodate it. */
    clear_locator(&locator, &null_source_position);
#define VA_LIST_NAME "va_list"
    (void)find_symbol(VA_LIST_NAME, (sizeof_t)(sizeof(VA_LIST_NAME)-1),
                      &locator);
    /* Look in either "std" or the global namespace for an existing
       va_list. */
    /* A linkage lookup is done to prevent using-directives from affecting
       the lookup. */
    if (va_list_in_std_namespace) {
      sym = namespace_qualified_id_lookup(&locator, std_namespace,
                                          IDL_LINKAGE_LOOKUP);
    } else {
      sym = file_scope_id_lookup(il_header.primary_scope, &locator,
                                 IDL_LINKAGE_LOOKUP);
    }  /* if */
    if (sym != NULL && is_type_symbol(sym)) {
      /* Yes, there is an existing type called va_list.  Use it rather than
         declaring a new symbol. */
      va_list_type = type_symbol_type(sym);
    } else {
      /* There is no existing va_list.  Create one. */
      a_scope_depth	scope_depth;
      /* Look for a special predefined name (e.g., __edg_va_list).  If it's
         declared as a file-scope type, use that type as the type for the
         built-in va_list. */
      clear_locator(&locator, &null_source_position);
      (void)find_symbol(
                      BUILTIN_VA_LIST_OVERRIDE_TYPE_NAME,
                      (sizeof_t)(sizeof(BUILTIN_VA_LIST_OVERRIDE_TYPE_NAME)-1),
                      &locator);
      sym = file_scope_id_lookup(il_header.primary_scope, &locator,
                                 IDL_NO_OPTIONS);
      if (sym != NULL && is_type_symbol(sym)) {
        va_list_type = type_symbol_type(sym);
      } else {
        /* The special symbol does not exist.  Use a generated or configured
           default (often char* or void*). */
        va_list_type = get_default_va_list_type();
      }  /* if */
      /* If the new va_list symbol is to be created in the std namespace,
         push the namespace now. */
      if (va_list_in_std_namespace) {
        enter_symbol_for_namespace_std(&locator);
        (void)push_namespace_scope((a_scope_kind)sck_namespace_extension,
                                   std_namespace);
        scope_depth = depth_scope_stack;
        /* Make sure the symbol for the "std" namespace is actually in the
           symbol table. */
      } else {
        scope_depth = DEPTH_OF_FILE_SCOPE;
      }  /* if */
      /* Enter a file-scope symbol "va_list" that is a typedef to the
         proper type. */
      sym = full_enter_symbol(VA_LIST_NAME, (sizeof_t)(sizeof(VA_LIST_NAME)-1),
                              (a_symbol_kind)sk_type, scope_depth);
      new_symbol_created = TRUE;
      if (va_list_in_std_namespace) {
        /* Pop the namespace scope pushed above. */
        pop_namespace_scope();
        /* When included via "stdarg.h", a using-declaration in the global
           namespace must be created.  This can't be done until the type
           is created below. */
      }  /* if */
    }  /* if */
    if (va_list_using_using_decl_in_std_namespace) {
      /* va_list is not in namespace std.  Create a using-declaration for
         the global va_list in the std namespace. */
      a_symbol_ptr	using_decl_sym;
      check_assertion(symbol_for_namespace_std != NULL);
      check_assertion(!va_list_in_std_namespace);
      std_namespace = symbol_for_namespace_std->variant.namespace_info.ptr;
      (void)push_namespace_scope((a_scope_kind)sck_namespace_extension,
                                 std_namespace);
      /* Make sure the symbol for the "std" namespace is actually in the
         symbol table. */
      enter_symbol_for_namespace_std(&locator);
      using_decl_sym = enter_namespace_projection_symbol(
                                              sym, /*is_using_decl=*/TRUE,
                                              &locator, depth_scope_stack,
                                              /*suppress_redecl_error=*/FALSE);
      set_namespace_membership(using_decl_sym, (a_source_correspondence*)NULL,
                               std_namespace);
      /* Pop the namespace scope pushed above. */
      pop_namespace_scope();
    }  /* if */
    /* Build a typedef for va_list.  This is done even when there is
       an existing symbol, because we need a declaration at the right
       place to tell the C- or C++-generating back end where to put the
       include of <stdarg.h>. */
    va_list_typedef = alloc_type((a_type_kind)tk_typeref);
    va_list_typedef->variant.typeref.type = va_list_type;
    va_list_typedef->is_builtin_va_list = TRUE;
    va_list_typedef->is_builtin_va_list_from_cstdarg = is_cstdarg;
    add_to_types_list(va_list_typedef, DEPTH_OF_FILE_SCOPE);
    /* Note that we update a pre-existing symbol to point to the typedef.
       this is necessary so that the needed and referenced flags will be
       set appropriately on uses of va_list after this point. */
    sym->variant.type.ptr = va_list_typedef;
    set_source_corresp(&va_list_typedef->source_corresp, sym);
    va_list_typedef->source_corresp.decl_position = null_source_position;
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Put out a source sequence entry for the type. */
    add_to_source_sequence_list((char *)va_list_typedef,
                                (an_il_entry_kind)iek_type);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    builtin_va_list_type = type_symbol_type(sym);
#undef VA_LIST_NAME
#ifdef GUARD_MACRO_FOR_VA_LIST
    /* Define a macro that tells the headers that va_list has been
       defined. */
    va_list_typedef->va_list_guard_macro_was_defined =
                                   define_guard_macro(GUARD_MACRO_FOR_VA_LIST);
#endif /* ifdef GUARD_MACRO_FOR_VA_LIST */
#ifdef GUARD_MACRO2_FOR_VA_LIST
    /* Define a macro that tells the headers that va_list has been
       defined. */
    va_list_typedef->va_list_guard_macro2_was_defined =
                                  define_guard_macro(GUARD_MACRO2_FOR_VA_LIST);
#endif /* ifdef GUARD_MACRO2_FOR_VA_LIST */
    if (new_symbol_created) {
      if (va_list_in_std_namespace) {
        /* Set the namespace information for the symbol.  Note that the
           type itself must remain in the global scope. */
        set_namespace_membership(sym, &va_list_typedef->source_corresp,
                                 std_namespace);
      }  /* if */
    }  /* if */
  }  /* if */
  if (!is_cstdarg && va_list_in_std_namespace &&
      !va_list_global_alias_has_been_created) {
    /* Create a file-scope using-declaration for the namespace scope type. */
    a_symbol_ptr     sym = symbol_for(builtin_va_list_type);
    (void)make_using_decl(sym, &null_source_position, DEPTH_OF_FILE_SCOPE);
    clear_locator(&locator, &null_source_position);
    (void)enter_namespace_projection_symbol(sym,
                                            /*is_using_decl=*/TRUE,
                                            &locator,
                                            DEPTH_OF_FILE_SCOPE,
                                            /*suppress_error=*/TRUE);
    va_list_global_alias_has_been_created = TRUE;
  }  /* if */
}  /* declare_builtin_va_list_type */


static void expand_ident_buffer(sizeof_t size_needed)
/*
Expand the ident_buffer by reallocating it, so that its total size is at
least size_needed.  Called by ensure_ident_buffer_space.
*/
{
  sizeof_t new_size;

  db_enter(4, "expand_ident_buffer");
  new_size = size_ident_buffer + IDENT_BUFFER_INCREMENTAL_ALLOCATION;
  if (new_size < size_needed) new_size  = size_needed;
  ident_buffer = realloc_buffer(ident_buffer, size_ident_buffer, new_size);
  size_ident_buffer = new_size;
  db_exit();
}  /* expand_ident_buffer */


/*
Ensure that ident_buffer has at least size_needed bytes in it.
If not, expand ident_buffer by reallocating it.
*/
#define ensure_ident_buffer_space(size_needed)                        \
{ if (size_ident_buffer < (size_needed)) {                            \
    expand_ident_buffer((sizeof_t)((size_needed)));                    \
  }  /* if */                                                         \
}  /* ensure_ident_buffer_space */


a_symbol_ptr f_find_external_symbol(a_symbol_locator     *location,
                                    a_name_linkage_kind  linkage,
                                    a_type_ptr           rout_type,
                                    a_requires_clause    *trcp,
                                    a_boolean            c_overload,
                                    a_symbol_locator     *ext_location)
/*
Determine the external name that should be associated with the identifier
specified by *location, and return a locator for it in *ext_location.  Also
return a pointer to any existing sk_extern_variable or sk_extern_routine
found, or NULL if there is no such entry.  The handling for C and C++ names
is different in two respects.  First, in C++ we have to deal with the
possible overloading of function names.  Second, in C++ we assume the
linker can handle whatever names the compiler will generate on the basis of
user name and type, and that those names will be generated in such a way as
to be unique (with some exceptions for global variable and entities with C
name linkage), whereas in C we allow for differences in external names
due to truncation.  If c_overload is TRUE, this is a case where the Clang
"overloadable" attribute was specified: Even in C-mode this requires a type
check.  For function cases, rout_type is the type of the function and trcp
is the trailing-requires-clause (NULL if none).  linkage is the name-linkage
of the entity being declared by the identifier.
*/
{
  a_symbol_header_ptr hdr_ptr;
  a_symbol_ptr        sym, second_best_match;
  a_namespace_ptr     nsp = NULL;
  a_boolean           extern_C_linkage_specified = FALSE;
  a_boolean           extern_C_overload = FALSE;

  db_enter(4, "f_find_external_symbol");
  /* Start with the external locator the same as the normal locator.  This
     is usually correct. */
  *ext_location = *location;
  if (is_error_locator(*ext_location)) {
    /* This is a compiler-generated error symbol (probably generated because
       an identifier was missing). */
    sym = NULL;
  } else {
    hdr_ptr = ext_location->symbol_header;
    if (linkage != (a_name_linkage_kind)nlk_external) {
      /* Either static or C++ external name linkage.  We can use the name and
         locator as passed in. */
    } else {
      /* The identifier is external, with "ordinary C" linkage. */
#if !TARG_CASE_SENSITIVE_EXTERNAL_NAMES
      /* External names are case-insensitive, so make a case-neutral copy
         of the identifier, upcasing each letter. */
      { a_const_char *src = hdr_ptr->identifier;
        char         *dest;
        sizeof_t     count;
        char         ch;
#if TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME > 0
        /* There is an upper limit on significance in external names. */
        char     new_ident[TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME];

        /* Copy the name, upcasing each letter.  Stop at the end of the name
           (without copying the final null) or when enough characters have been
           copied to meet the maximum number of significant characters. */
        dest = new_ident;
        for (count = 0;
             *src != '\0' && count < TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME;
             count++) {
          ch = *src++;
          if (islower((unsigned char)ch)) ch = toupper(ch);
          *dest++ = ch;
        }  /* for */
#else /* TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME == 0 */
        /* Use a dynamically-allocated array, since there is no limit to
           the significance of external names. */
        char *new_ident;
        ensure_ident_buffer_space(hdr_ptr->identifier_length);
        dest = new_ident = ident_buffer;
   
        /* Copy the name, upcasing each letter.  Stop at the end of the name
           (without copying the final null). */
        for (count = 0; *src != '\0'; count++) {
          ch = *src++;
          if (islower((unsigned char)ch)) ch = toupper(ch);
          *dest++ = ch;
        }  /* for */
#endif /* TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME > 0 */
        /* Create a header/locator for the upcased/truncated name. */
        (void)find_symbol(new_ident, count, ext_location);
        hdr_ptr = ext_location->symbol_header;
      }
#else /* TARG_CASE_SENSITIVE_EXTERNAL_NAMES */
#if TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME > 0
      /* See if the symbol's name is longer than the maximum number of
         significant characters in an external name. */
      if (hdr_ptr->identifier_length > TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME) {
        /* The name is overlong; the external version must be truncated.
           Create the header/locator by looking up the truncated name. */
        (void)find_symbol(hdr_ptr->identifier,
                          TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME, ext_location);
        hdr_ptr = ext_location->symbol_header;
      } else {
        /* The name is not overlong; the external name will be the same as the 
           source name, and therefore the locator for the new symbol is the
           same as that for the current symbol. */
      }  /* if */
#endif /* TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME > 0 */
#endif /* !TARG_CASE_SENSITIVE_EXTERNAL_NAMES */
    }  /* if */
    if (!C_mode() && linkage == (a_name_linkage_kind)nlk_external) {
      extern_C_linkage_specified = TRUE;
    }  /* if */
    /* See if there is already an external symbol with this name and belonging
       to the appropriate namespace. */
    if (!C_mode() && !location->is_file_scope_qualified_name) {
      /* See if the current declaration is a namespace-qualified name. */
      nsp = qualifier_namespace_ptr(*location);
      /* If not, use the innermost enclosing namespace by default. */
      if (nsp == NULL &&
          depth_innermost_namespace_scope != DEPTH_OF_FILE_SCOPE) {
        nsp = scope_stack[depth_innermost_namespace_scope].
                                           il_scope->variant.assoc_namespace;
      }  /* if */
    }  /* if */
    second_best_match = NULL;
    for (sym = hdr_ptr->other_symbols; sym != NULL; sym = sym->next) {
      a_source_correspondence  *scp;
      a_namespace_ptr          sym_nsp;
      a_boolean                sym_has_C_linkage;
      a_boolean                sym_is_variable;
      a_type_ptr               other_type;
      an_extern_symbol_descr_ptr
                               esdp;
      /* Ignore symbols not associated with the current file scope.  These
         could be extern entities associated with other translation units. */
      if (sym->decl_scope != file_scope_number) {
        continue;
      }  /* if */
      /* Ignore symbols not visible to the current module. */
      if (!is_symbol_currently_lookup_visible(sym)) {
        continue;
      }  /* if */
      if (sym->kind == (a_symbol_kind)sk_extern_variable) {
        sym_is_variable = TRUE;
        scp = &sym->variant.extern_symbol_descr->
                                  variant.variable->source_corresp;
      } else if (sym->kind == (a_symbol_kind)sk_extern_routine) {
        sym_is_variable = FALSE;
        scp = &sym->variant.extern_symbol_descr->variant.routine.ptr
                                               ->source_corresp;
      } else {
        /* Ignore other symbols on the list.  These include synthesized
           namespace projection symbols, and unknown function symbols. */
        continue;
      }  /* if */
      esdp = sym->variant.extern_symbol_descr;
      other_type = esdp->type;
      other_type = skip_typerefs(other_type);
      sym_nsp = sym_parent_namespace_or_null(sym);
      sym_has_C_linkage =
                     (scp->name_linkage == (a_name_linkage_kind)nlk_external);
      if (extern_C_linkage_specified) {
        if (sym_has_C_linkage) {
          if (rout_type != NULL && !sym_is_variable && sym_nsp == nsp &&
              !is_error_type(sym->variant.extern_symbol_descr->type)) {
            /* A special case -- two extern "C" routine declarations in the
               same namespace.  Consider them a match only if their
               signatures match; it they don't match, this will be treated
               as an overloading error. */
            extern_C_overload = TRUE;
          } else {
            /* Except for the special case noted above, two extern "C"
               entities with the same name are always a match, even if they
               belong to different namespaces or have different signatures,
               and even if one is a routine and the other is a variable:
               the names are identical to the linker. */
            if (second_best_match == NULL) second_best_match = sym;
            continue;
          }  /* if */
        } else if (sym_nsp == NULL && sym_is_variable) {
          /* sym represents a variable in global namespace scope and the new
             declaration is an extern "C" declaration in another namespace
             scope.  This is an error: Return the symbol for error reporting
             purposes. */
          break;
        }  /* if */
      } else if (sym_has_C_linkage && nsp == NULL && rout_type == NULL) {
        /* The new declaration is a variable in global scope, and sym
           represents an extern "C" declaration with the same name.  This
           is an error: Return the symbol for error reporting purposes. */
        break;
      }  /* if */
      if (sym_nsp != nsp) {
        /* Namespaces do not match -- keep looking.  Note that an extern "C"
           entity declared in a namespace does match up with a variable
           declared in the global namespace (because the latter typically do
           not have mangled names).  In such cases an error will be issued. */
      } else if (sym->kind == (a_symbol_kind)sk_extern_variable) {
        if (rout_type == NULL) break;
        if (second_best_match == NULL) second_best_match = sym;
      } else if (sym->kind == (a_symbol_kind)sk_extern_routine) {
        if (rout_type != NULL) rout_type = skip_typerefs(rout_type);
        /* A type compatibility check may also be required for routines. */
        if (rout_type == NULL ||
            (C_mode() && !c_overload &&
             esdp->variant.routine.ptr->source_corresp.name_linkage !=
                               (a_name_linkage_kind)nlk_cplusplus_external) ||
            esdp->variant.routine.ptr == il_header.main_routine) {
          /* A name match is enough in C (and for C++, if the routine is
             "::main"). */
          break;
        } else if (is_error_type(esdp->type)) {
          /* Assume this is not a match.  Keep looking. */
        } else {
          /* In C++ the function's type signature is effectively part of the
             name.  Therefore we check for parameter type compatibility (the
             return type is not decisive, since functions with the same
             param types and different return types are not allowed). */
          /* Note that error types are not considered compatible with
             anything here, and that's deliberate to avoid a false clash
             on something like
               int f(int)   { return 0; }
               int f(undef) { return 0; }
          */
          if (param_types_are_compatible(rout_type, other_type,
                                         TCF_NO_FLAGS)) {
            /* Param types are compatible, so we have a match, unless a
               special situation applies.  The first such special situation
               is if we have incompatible requires clauses (e.g., on friend
               function declarations).  The other such special situation
               occurs with mismatched enable_if attributes. */
            a_requires_clause_ptr  old_trcp = esdp->variant.routine.ptr
                                                  ->trailing_requires_clause;
            if ((old_trcp == trcp ||
                 equiv_requires_clauses(old_trcp, trcp)) &&
                (!(rout_type->variant.routine.extra_info
                                                 ->has_enable_if_attribute ||
                   other_type->variant.routine.extra_info
                                                 ->has_enable_if_attribute) ||
                 compatible_enable_if_attributes(rout_type, other_type))) {
              break;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
      /* No match found yet, so keep looking.  If none is found, a NULL
         sym is returned to the caller. */
    }  /* for */
    if (sym == NULL && !extern_C_overload) {
      sym = second_best_match;
    }  /* if */
  }  /* if */
  /* Make the ext_location source position the same as the original source
     position. */
  ext_location->source_position = location->source_position;
  db_exit();
  return sym;
}  /* f_find_external_symbol */


a_symbol_header_ptr find_symbol_header(a_const_char     *identifier,
                                       sizeof_t         length,
                                       a_symbol_locator *locator)
/*
Return the symbol header for the specified identifier.  The given locator
will have its symbol_header set to the resulting symbol header.
*/
{
  a_symbol_header_ptr	sym_hdr;

  (void)find_symbol(identifier, length, locator);
  sym_hdr = locator->symbol_header;
  return sym_hdr;
}  /* find_symbol_header */


a_symbol_header_ptr find_il_symbol_header(a_const_char *identifier,
                                          sizeof_t     length)
/*
Return the symbol header for the specified identifier.  This function should be
preferred in contexts where there is no reasonable locator to populate;
otherwise, prefer find_symbol_header.
*/
{
  a_symbol_locator locator;

  clear_locator(&locator, &null_source_position);
  return find_symbol_header(identifier, length, &locator);
}  /* find_il_symbol_header */

#if MICROSOFT_EXTENSIONS_ALLOWED

void init_cli_operator_headers(void)
/*
Create symbol headers for the CLI operator names.  This is used to map the
operator name to an a_cli_operator_kind entry and the associated
a_cli_operator_info structure.  Furthermore, it provides a mechanism to
readily detect when the operator name is used in a context in which the name
is reserved.
*/
{
  for (int cok = (int)cok_first; cok < (int)cok_last; ++cok) {
    a_symbol_header_ptr header;
    a_symbol_locator    locator;
    a_const_char        *name = cli_operator_info[cok].cli_name;
    check_assertion(name != NULL && *name != '\0');
    clear_locator(&locator, &null_source_position);
    header = find_symbol_header(name, (sizeof_t)strlen(name), &locator);
    header->is_cli_operator = TRUE;
    header->variant.cli_operator = (a_cli_operator_kind)cok;
  }  /* for */
}  /* init_cli_operator_headers */

#if CPPCLI_ENABLING_POSSIBLE && EDG_WIN32

static a_symbol_header_ptr find_cli_operator_header(a_const_char *identifier)
/*
Return the symbol header for the specified identifier if it has the same name
as a CLI operator.
*/
{
  a_symbol_header_ptr header;
  a_symbol_locator    locator;

  check_assertion(identifier != NULL && *identifier != '\0');
  clear_locator(&locator, &null_source_position);
  header = find_symbol_header(identifier, (sizeof_t)strlen(identifier),
                              &locator);
  if (!header->is_cli_operator) header = NULL;
  return header;
}  /* find_cli_operator_header */


a_cli_operator_kind find_cli_operator_kind(a_const_char *identifier)
/*
Return the CLI operator kind corresponding to the specified identifier if it
has the same name as a CLI operator, or cok_none if it doesn't.
*/
{
  a_symbol_header_ptr header;

  header = find_cli_operator_header(identifier);
  return header != NULL ? header->variant.cli_operator
                        : (a_cli_operator_kind)cok_none;
}  /* find_cli_operator_kind */

#endif /* CPPCLI_ENABLING_POSSIBLE && EDG_WIN32 */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

a_symbol_ptr find_label_symbol(a_symbol_header_ptr	sym_hdr)
/*
Look through the active list of sym_hdr for a label symbol declared in
the current function.  Return the symbol if one is found, or NULL if not.
*/
{
  a_symbol_ptr   sym;
  a_scope_number func_scope_number;

  /* The important thing here is that we do not want to find labels
     declared in functions surrounding this one, e.g., if we're in
     a member function of a local class. */
  check_assertion(depth_innermost_function_scope != NO_SCOPE_DEPTH);
  func_scope_number = scope_stack[depth_innermost_function_scope].number;
  for (sym = sym_hdr->symbol; sym != NULL; sym = sym->next) {
    if (sym->kind == (a_symbol_kind)sk_label) {
      if (sym->decl_scope == func_scope_number) break;
      /* In GNU mode there can be local label declarations in statement
         expressions.  They have block scope. */
      if (gnu_mode) {
        a_scope_stack_entry_ptr ssep;
        /* Look at the scopes from the current top of the scope stack to
           the innermost enclosing function scope.  If the label is
           declared in one of those scopes it is in the current function. */
        for (ssep = &scope_stack[decl_scope_level];
             ssep->number != func_scope_number;
             ssep = previous_scope_of(ssep)) {
          if (ssep->number == sym->decl_scope) goto have_sym;
        }  /* for */
      }  /* if */
    }  /* if */
  }  /* for */
have_sym:
  return sym;
}  /* find_label_symbol */


a_symbol_ptr find_macro_symbol(a_symbol_header_ptr	sym_hdr)
/*
Look for a macro symbol on the symbol list of sym_hdr.  Return the macro
symbol or NULL if none is found.
*/
{
  a_symbol_ptr	sym;

  for (sym = symbol_list_for_file_scope_symbols(sym_hdr);
       sym != NULL; sym = sym->next) {
    if (sym->kind == (a_symbol_kind)sk_macro) break;
  }  /* for */
  return sym;
}  /* find_macro_symbol */


a_symbol_ptr find_macro_symbol_by_name(a_const_char     *identifier,
				       sizeof_t         length,
				       a_symbol_locator	*locator)
/*
Look for a macro symbol with the specified name.  Return the macro symbol
if found, or NULL.
*/
{
  a_symbol_ptr	sym;

  sym = find_macro_symbol(find_symbol_header(identifier, length, locator));
  return sym;
}  /* find_macro_symbol_by_name */


void change_to_destructor_or_finalizer_locator(a_symbol_locator  *locator,
                                               a_boolean         finalizer)
/*
Change the name indicated by the given locator so that it has a "~" or "!" on
the front.  This is used for destructor names and C++/CLI finalizer names.
*/
{
  sizeof_t ident_length = locator->symbol_header->identifier_length;
  a_source_position position;

  /* Copy the identifier name into a dynamically-allocated buffer and put a
     tilde or exclamation point at the front.  The final null is not copied. */
  ensure_ident_buffer_space(ident_length+1);
  (void)memcpy(ident_buffer+1, locator->symbol_header->identifier,
               size_t_arg(ident_length));
  ident_buffer[0] = finalizer ? '!' : '~';
  ident_length++;
  position = locator->source_position;
  clear_locator(locator, &position);
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (finalizer) {
    check_assertion(cli_or_cx_enabled);
    locator->is_finalizer_name = TRUE;
  } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Do not insert code here. */
  {
    locator->is_destructor_name = TRUE;
  }  /* if */
  (void)find_symbol(ident_buffer, ident_length, locator);
}  /* change_to_destructor_or_finalizer_locator */


a_boolean destructor_name_matches_class_name(a_symbol_ptr class_sym)
/*
The locator points to a symbol header associated with a destructor or
C++/CLI finalizer (e.g., "~A" or "!A") and class_sym points to a symbol
associated with a class (e.g., "A").  This routine compares the class name
portion of the destructor or finalizer name with the name of the class symbol.
Returns TRUE if the names match and FALSE if they do not match.
*/
{
  a_const_char	*destructor_name;
  a_const_char	*class_name;
  a_boolean	result = FALSE;

  check_assertion(!is_error_locator(locator_for_curr_id));
  check_assertion(class_sym != NULL);
  check_assertion(is_dtor_like_locator(locator_for_curr_id));
  /* Get a pointer to the second character of the identifier associated
     with the destructor.  This skips over the tilde or exclamation point. */
  destructor_name = &locator_for_curr_id.symbol_header->identifier[1];
  /* Get a pointer to the first character of the class name. */
  class_name = class_sym->header->identifier;
  /* Compare the strings -- return TRUE if they are the same. */
  result = (strcmp(destructor_name, class_name) == 0);
  return result;
}  /* destructor_name_matches_class_name */


a_boolean class_sym_is_for_closure_class(a_symbol_ptr  sym)
/*
The given symbol is for a class type.  Return TRUE if the class is a closure
class.
*/
{
  a_type_ptr  type;

  check_assertion(is_class_struct_union_symbol(sym));
  type = sym->variant.class_struct_union.type;
  return class_type_supp(type)->is_lambda_closure_class;
}  /* class_sym_is_for_closure_class */


void change_class_locator_into_constructor_locator(
                                              a_symbol_locator  *locator,
                                              a_source_position *pos,
                                              a_boolean         is_static_ctor)
/*
Change a locator for a class name into the locator for the constructor for
the class.  The original locator must be for a specific symbol.  pos_curr_token
is used as the source position in the new locator.  If is_static_ctor is
TRUE, the locator is modified to have the symbol header of the C++/CLI
static constructor.  This routine is only used in C++ mode.
*/
{
  a_symbol_ptr                  class_symbol = locator->specific_symbol;
  a_class_symbol_supplement_ptr extra_info;
  a_symbol_header_ptr           hdr_ptr;

  /* If the symbol is for a class template, get the associated prototype
     instantiation. */
  if (is_class_template_symbol(class_symbol)) {
    class_symbol = class_symbol->variant.template_info->
                                variant.class_template.prototype_instantiation;
  }  /* if */
#if CHECKING
  if (class_symbol == NULL) {
    internal_error(
        "change_class_locator_into_constructor_locator: NULL specific symbol");
  }  /* if */
  if (class_symbol->kind != (a_symbol_kind)sk_class_or_struct_tag &&
      class_symbol->kind != (a_symbol_kind)sk_union_tag) {
    internal_error(
       "change_class_locator_into_constructor_locator: locator not for class");
  }  /* if */
  check_assertion(!is_static_ctor || cli_or_cx_enabled);
#endif /* CHECKING */
  if (locator->symbol_header == unnamed_tag_symbol_header
#if MICROSOFT_EXTENSIONS_ALLOWED
      && !is_static_ctor
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                        ) {
    /* Let the symbols for an unnamed class and its constructor share the
       same symbol header (except for C++/CLI static constructors: They can
       be unnamed when they are for the boxed type corresponding to an
       unnamed enumeration type). */
    hdr_ptr = locator->symbol_header;
  } else {
    extra_info = class_symbol->variant.class_struct_union.extra_info;
    if (!is_static_ctor && extra_info->constructor != NULL) {
      /* A constructor exists already, so get the header pointer from it. */
      hdr_ptr = extra_info->constructor->header;
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (is_static_ctor && extra_info->static_constructor != NULL) {
      /* A C++/CLI static constructor exists already, so get the header pointer
         from it. */
      hdr_ptr = extra_info->static_constructor->header;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else {
      /* The class has no constructor yet, so create a new header. */
      hdr_ptr = alloc_symbol_header();
      /* The name string can be shared with the class header. */
      hdr_ptr->identifier = locator->symbol_header->identifier;
      hdr_ptr->identifier_length = locator->symbol_header->identifier_length;
    }  /* if */
  }  /* if */
  clear_locator(locator, pos);
  locator->symbol_header = hdr_ptr;
}  /* change_class_locator_into_constructor_locator */

#if MICROSOFT_EXTENSIONS_ALLOWED

typedef struct an_ms_attr_alt_name_entry *an_ms_attr_alt_name_entry_ptr;
typedef struct an_ms_attr_alt_name_entry {
  /*  */
  an_ms_attr_alt_name_entry_ptr
		next;
			/* Next in a linked list of Microsoft attribute
			   alternate name header entries; NULL for the last
			   entry on the list. */
  a_symbol_header_ptr
		original_header;
			/* Pointer to the symbol header without the
			   "Attribute" suffix. */
  a_symbol_header_ptr
		alt_name_header;
			/* Pointer to the symbol header with the "Attribute"
			   suffix. */
} an_ms_attr_alt_name_entry;


STATIC_THREAD an_ms_attr_alt_name_entry_ptr
		ms_attr_alt_name_entry_list;
			/* List of symbol header entries that are used to look
			   up Microsoft attributes with an alternate name that
			   includes an "Attribute" suffix. */


static an_ms_attr_alt_name_entry_ptr alloc_ms_attr_alt_name_entry(void)
/*
Allocate a new alternate name entry and return a pointer to it.
*/
{
  an_ms_attr_alt_name_entry_ptr ptr;

  db_enter(5, "alloc_ms_attr_alt_name_entry");
  ptr = (an_ms_attr_alt_name_entry_ptr)alloc_fe(
                                           sizeof(an_ms_attr_alt_name_entry));
#if DEBUG
/* GCOVR_EXCL_START */
  num_ms_attr_alt_name_entries_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  ptr->next            = NULL;
  ptr->original_header = NULL;
  ptr->alt_name_header = NULL;

  db_exit();
  return ptr;
}  /* alloc_ms_attr_alt_name_entry */


static a_symbol_header_ptr find_ms_attr_alt_name_header(
                                                   a_symbol_header_ptr header)
/*
Look up the alternate name symbol header for a given symbol header.  If there
is none, create a new one.
*/
{
  an_ms_attr_alt_name_entry_ptr alt_name_entry;
  an_ms_attr_alt_name_entry_ptr prev_alt_name_entry;

  /* Search the alternate name entry list for an entry matching the given
     symbol header.  If one is found, it is moved to the front of the list. */
  prev_alt_name_entry = NULL;
  alt_name_entry = ms_attr_alt_name_entry_list;
  for (; alt_name_entry != NULL; alt_name_entry = alt_name_entry->next) {
    if (alt_name_entry->original_header == header) {
      /* Found it.  Move it to the front of the list. */
      if (prev_alt_name_entry != NULL) {
        prev_alt_name_entry->next = alt_name_entry->next;
        alt_name_entry->next = ms_attr_alt_name_entry_list;
        ms_attr_alt_name_entry_list = alt_name_entry;
      }  /* if */
      break;
    }  /* if */
    prev_alt_name_entry = alt_name_entry;
  }  /* if */
  /* alt_name_entry is NULL if no entry already exists on the list for the
     specified symbol header. */
  if (alt_name_entry == NULL) {
    a_symbol_header_ptr alt_name_header;
    char                attribute_suffix[] = "Attribute";
    char*               alternate_name;
    sizeof_t            length;
    a_symbol_locator    locator;
    /* Create a symbol header for the alternate name of a Microsoft attribute,
       which includes an "Attribute" suffix. */
    length = header->identifier_length + sizeof(attribute_suffix) - 1;
    alternate_name = alloc_primary_file_scope_il((sizeof_t)(length + 1));
    /* Copy the identifier from the original header. */
    (void)strncpy(alternate_name, header->identifier,
                  header->identifier_length);
    /* Append "Attribute" to the identifier. */
    (void)strcpy(alternate_name + header->identifier_length,
                 attribute_suffix);
    /* Terminate the string. */
    alternate_name[length] = '\0';
#if DEBUG
/* GCOVR_EXCL_START */
    symbol_name_string_space += (unsigned long)(length + 1);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
    /* Find the symbol header for the alternate name. */
    clear_locator(&locator, &null_source_position);
    alt_name_header = find_symbol_header(alternate_name, length,
                                         &locator);
    /* Create a new alternate name entry and add it to the front of the
       list. */
    alt_name_entry = alloc_ms_attr_alt_name_entry();
    alt_name_entry->next = ms_attr_alt_name_entry_list;
    alt_name_entry->original_header = header;
    alt_name_entry->alt_name_header = alt_name_header;
    ms_attr_alt_name_entry_list = alt_name_entry;
  }  /* if */
  return alt_name_entry->alt_name_header;
}  /* find_ms_attr_alt_name_header */


void change_ms_attr_locator_into_alt_name_locator(a_symbol_locator *locator)
/*
Change the symbol header in locator to reference the alternate name symbol
header.
*/
{
  a_symbol_header_ptr hdr_ptr;

  check_assertion(!is_error_locator(*locator) &&
                  !locator->do_not_clear_specific_symbol);
  clear_specific_symbol(*locator);
  hdr_ptr = find_ms_attr_alt_name_header(locator->symbol_header);
  locator->symbol_header = hdr_ptr;
}  /* change_ms_attr_locator_into_alt_name_locator */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

void make_opname_locator(an_opname_kind     opname,
                         a_symbol_locator   *locator,
                         a_source_position  *pos)
/*
Make a locator for the operator name associated with opname.  This is
used for C++ constructs like "operator+".  Use pos as the source position.
*/
{
  a_symbol_header_ptr *table_entry = &opname_symbol_table[opname];
  a_symbol_header_ptr hdr_ptr;
  a_const_char        *opstr;
  char                *str;
  a_boolean           blank_needed;
  sizeof_t            opname_length;

  clear_locator(locator, pos);
  if (opname == (an_opname_kind)onk_none) {
    set_to_error_locator(*locator);
    goto done;
  }  /* if */
  hdr_ptr = *table_entry;
  if (hdr_ptr == NULL) {
    /* First use of this opname.  Allocate the header. */
    *table_entry = hdr_ptr = alloc_symbol_header();
    /* Give the header the name "operatorX" where "X" is the string for the
       operator. */
    opstr = opname_names[(int)opname];
    /* For "new" and "delete", a blank is needed between the "operator"
       and the opname. */
    blank_needed = (is_id_char[opstr[1]-CHAR_MIN] != FALSE);
    opname_length = LENGTH_CANONICAL_OPERATOR_FUNCTION_INTRO + strlen(opstr)
                    + blank_needed;
    hdr_ptr->identifier_length = opname_length;
    hdr_ptr->identifier = str =
                    alloc_primary_file_scope_il((sizeof_t)(opname_length + 1));
    (void)memcpy(str, CANONICAL_OPERATOR_FUNCTION_INTRO,
                 LENGTH_CANONICAL_OPERATOR_FUNCTION_INTRO);
    if (blank_needed) str[LENGTH_CANONICAL_OPERATOR_FUNCTION_INTRO] = ' ';
    (void)strcpy(str+LENGTH_CANONICAL_OPERATOR_FUNCTION_INTRO+blank_needed,
                 opstr);
    hdr_ptr->variant.opname = opname;
#if DEBUG
/* GCOVR_EXCL_START */
    symbol_name_string_space += (unsigned long)(opname_length + 1);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  }  /* if */
  locator->symbol_header = hdr_ptr;
  locator->is_operator_name = TRUE;
  locator->variant.opname = opname;
done:;
}  /* make_opname_locator */


void make_literal_opname_locator(a_const_char      *ud_suffix,
                                 sizeof_t          ud_suffix_len,
                                 a_symbol_locator  *locator,
                                 a_source_position *pos)
/*
Make a locator in *locator for the C++11 literal operator or literal
operator template whose literal-operator-id contains the identifier
ud_suffix (of length ud_suffix_len).  Use pos as the source position
(NULL to suppress diagnostics).
*/
{
  a_literal_operator_header_ptr lo_hdr_ptr;
  a_symbol_header_ptr           sym_hdr_ptr;

  clear_locator(locator,
                ((pos == NULL) ? &null_source_position : pos));
  for (lo_hdr_ptr = literal_operator_header_list;
       lo_hdr_ptr != NULL && (lo_hdr_ptr->suffix_len != ud_suffix_len ||
                              memcmp(lo_hdr_ptr->suffix, ud_suffix,
                                     ud_suffix_len) != 0);
       lo_hdr_ptr = lo_hdr_ptr->next) {}
  if (lo_hdr_ptr != NULL) {
    sym_hdr_ptr = lo_hdr_ptr->symbol_header;
  } else {
    /* First use of this literal operator name.  Allocate the header. */
    sizeof_t len = LENGTH_CANONICAL_LITERAL_OPERATOR_INTRO + ud_suffix_len;
    char     *str = alloc_primary_file_scope_il((sizeof_t)(len + 1));
    lo_hdr_ptr = alloc_literal_operator_header(ud_suffix, ud_suffix_len);
    sym_hdr_ptr = alloc_symbol_header();
    /* Give the header the name 'operator ""X' where "X" is ud_suffix. */
    strcpy(str, CANONICAL_LITERAL_OPERATOR_INTRO);
    memcpy(str + LENGTH_CANONICAL_LITERAL_OPERATOR_INTRO, ud_suffix,
           ud_suffix_len);
    str[len] = '\0';
    sym_hdr_ptr->identifier_length = len;
    sym_hdr_ptr->identifier = str;
    lo_hdr_ptr->symbol_header = sym_hdr_ptr;
#if DEBUG
/* GCOVR_EXCL_START */
    symbol_name_string_space += (unsigned long)(len + 1);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
    if (*ud_suffix != '_') {
      /* ud-suffixes that do not begin with "_" are reserved. */
      if (curr_ise == NULL || curr_ise->from_system_include_dir) {
        /* Accept the ud-suffix silently on the assumption that it
           represents a standard suffix, which are exempt from the naming
           restriction. */
      } else if (pos == NULL) {
        /* This is a tentative lookup for which no diagnostics should
           be issued. */
      } else {
        /* Issue a diagnostic of a severity that depends on the context. */
        an_error_severity severity;
        severity = (strict_ansi_mode || gpp_mode) ? es_warning
                                                  : es_remark;
        pos_diagnostic(severity, ec_lit_suffix_no_underscore, pos);
      }  /* if */
    }  /* if */
  }  /* if */
  locator->symbol_header = sym_hdr_ptr;
}  /* make_literal_opname_locator */


STATIC_THREAD unsigned long
		sb_counter = 0;
			/* Counter used to give a unique name to structured
			   binding containers (the name is only used while
			   the container is being declared; eventually, the
			   container variable is unnamed). */


void make_struct_binding_container_locator(a_symbol_locator  *locator,
                                           a_source_position *pos)
/*
Make a unique locator in *locator for a structured binding container variable.
*/
{
  a_symbol_header_ptr  sym_hdr = alloc_symbol_header();
  Small_string<50>     buffer("<struct binding ", ++sb_counter, ">");

  sym_hdr->identifier = buffer.to_allocated_storage(IL_allocator<char>());
  sym_hdr->identifier_length = buffer.length();
  clear_locator(locator, pos);
  locator->symbol_header = sym_hdr;
}  /* make_struct_binding_container_locator */


void make_type_conversion_locator(a_type_ptr         type,
                                  a_symbol_locator   *locator,
                                  a_source_position  *pos)
/*
Create a locator to represent a type conversion function.  The destination
type "type" is recorded in the locator.
*/
{
  if (is_error_type(type)) {
    set_to_error_locator(*locator);
    locator->symbol_header = get_error_symbol_header();
  } else {
    clear_locator(locator, pos);
    locator->symbol_header = symbol_header_for_conversion_function(type);
  }  /* if */
  locator->is_conversion_name = TRUE;
  locator->variant.conversion_result_type = type;
}  /* make_type_conversion_locator */


a_symbol_ptr find_default_operator_new_sym(a_symbol_ptr sym,
                                           a_boolean    *ambiguous)
/*
Given the symbol for an operator new() (which may be overloaded), find the
default (i.e., single-argument) version and return a pointer to its symbol,
or NULL if it is not found or there is an ambiguity (e.g., resulting from
default arguments).  The symbol might be for a class-specific operator new(),
and therefore might be a projection symbol.  If there is an ambiguity return
*ambiguous set to TRUE.
*/
{
  an_overload_set_traversal_block ostblock;
  a_param_type_ptr                ptp;
  a_symbol_ptr                    default_sym = NULL;

  *ambiguous = FALSE;
  /*lint --e{850} sym modified in loop */
  for (sym = set_up_overload_set_traversal_simple(sym, &ostblock);
       sym != NULL;
       sym = next_symbol_in_overload_set(&ostblock)) {
    if (sym->kind == (a_symbol_kind)sk_projection) {
      /* An overload set can contain a projection symbol as the result of a
         using declaration. */
      if (sym->ambiguous) {
        /* All bets are off if the symbol is ambiguous. */
        *ambiguous = TRUE;
        default_sym = NULL;
        break;
      }  /* if */
      reduce_projection_symbol_to_fundamental_symbol(sym);
    }  /* if */
    /* Ignore function templates. */
    if (is_function_symbol(sym)) {
      /* Look for a symbol for a function with just one parameter.  A default
         argument is not allowed on the first argument and need not be checked
         for; however, one may appear on the second argument. */
      ptp = skip_typerefs(sym->variant.routine.ptr->type)->
                                  variant.routine.extra_info->param_type_list;
      check_assertion(ptp != NULL);
      if (!is_error_type(ptp->type)) {
        if (ptp->next == NULL || ptp->next->has_default_arg) {
          if (default_sym == NULL) {
            /* A match.  But keep looking in case there's an ambiguity. */
            default_sym = sym;
          } else {
            /* It's ambiguous, so just return NULL.  No error is issued at
               this point. */
            default_sym = NULL;
            *ambiguous = TRUE;
            break;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  return default_sym;
}  /* find_default_operator_new_sym */


static a_boolean is_default_operator_new(a_routine_ptr routine,
                                         a_boolean     *is_aligned_new)
/*
Return TRUE if the indicated routine (an operator new function) is a default
operator new function (including the variant with a std::align_val_t
parameter).  *is_aligned_new is set to TRUE if the routine has a second
parameter of type align_val_t and to FALSE otherwise.
*/
{
  a_routine_type_supplement_ptr rtsp;
  a_boolean                     is_default = FALSE;

  *is_aligned_new = FALSE;
  rtsp = skip_typerefs(routine->type)->variant.routine.extra_info;
  if (rtsp->has_ellipsis) {
    /* An operator new declared with ellipsis can't be a default operator
       new. */
  } else {
    a_param_type_ptr ptp = rtsp->param_type_list;
    check_assertion(ptp != NULL);
    if (ptp->next == NULL) {
      /* operator new(std::size_t) is a default operator new. */
      is_default = TRUE;
    } else if (ptp->next != NULL && overaligned_allocation_enabled &&
        identical_types(ptp->next->type, type_of_align_val_t) &&
        ptp->next->next == NULL) {
      /* operator new(std::size_t, std::align_val_t) for an overaligned
         type is a default operator new. */
      is_default = TRUE;
      *is_aligned_new = TRUE;
    }  /* if */
  }  /* if */
  return is_default;
}  /* is_default_operator_new */


a_boolean is_default_operator_delete(a_routine_ptr routine,
                                     a_boolean     *is_sized_ver,
                                     a_boolean     *is_aligned_delete,
                                     a_boolean     *is_destroying_delete)
/*
Return TRUE if the indicated routine (an operator delete function) is a
default operator delete function (including the variant with parameters of type
std::size_t and/or std::align_val_t or a destroying operator delete).
*is_sized_ver is set to TRUE if the routine has a second parameter of type
size_t and is set to FALSE otherwise.  *is_aligned_delete is set to TRUE if
the routine is one of the variants that has a parameter of type
std::align_val_t and to FALSE otherwise.  *is_destroying_delete is set to TRUE
if the routine is a destroying delete and to FALSE otherwise.  Note that this
routine does not report whether the routine is a "usual deallocation function"
-- only that it is a candidate to be one.  In particular, this routine will
return TRUE for a two-parameter class member operator delete, but the presence
of a one-parameter class member operator delete would disqualify the
two-parameter version from being a "usual deallocation function" (see
[basic.stc.dynamic.deallocation]).
*/
{
  a_boolean                      is_default = TRUE;
  a_routine_type_supplement_ptr  rtsp;
  a_param_type_ptr               ptp;
  a_type_ptr                     param_type;

  *is_sized_ver = FALSE;
  *is_aligned_delete = FALSE;
  *is_destroying_delete = FALSE;
  rtsp = skip_typerefs(routine->type)->variant.routine.extra_info;
  if (rtsp->has_ellipsis) {
    /* An operator delete declared with ellipsis can't be a default operator
       delete. */
    is_default = FALSE;
  } else {
    ptp = rtsp->param_type_list;
    check_assertion(ptp != NULL);
    if (destroying_operator_delete_enabled) {
      if (routine->source_corresp.is_class_member &&
          f_identical_types(make_pointer_type(parent_class_of(routine)),
                            ptp->type, ITF_NO_FLAGS) &&
          is_std_destroying_delete_t(ptp->next->type)) {
        /* A destroying delete operator. */
        *is_destroying_delete = TRUE;
        ptp = ptp->next;
      }  /* if */
    }  /* if */
    ptp = ptp->next;
    /* We've skipped past the initial void* parameter, or in some cases
       the A* and std::destroying_delete_t parameters. */
    if (ptp == NULL) {
      goto no_more_parameters;
    }  /* if */
    param_type = skip_typerefs(ptp->type);
    /* Look for an optional std::size_t parameter. */
    if ((routine->source_corresp.is_class_member ||
         (sized_deallocation_enabled &&
          !is_class_or_namespace_member(routine))) &&
        is_integral_type(param_type) &&
        param_type->variant.integer.int_kind == targ_size_t_int_kind) {
      *is_sized_ver = TRUE;
      ptp = ptp->next;
      if (ptp == NULL) {
        goto no_more_parameters;
      }  /* if */
    }  /* if */
    /* Look for an optional std::align_val_t parameter. */
    if (overaligned_allocation_enabled &&
        identical_types(ptp->type, type_of_align_val_t)) {
      *is_aligned_delete = TRUE;
      ptp = ptp->next;
    }  /* if */
no_more_parameters:
    /* If there are any other parameters, this is not a default operator
       delete. */
    if (ptp != NULL) {
      is_default = FALSE;
    }  /* if */
  }  /* if */
  return is_default;
}  /* is_default_operator_delete */


a_symbol_ptr find_default_operator_delete_sym(a_symbol_ptr sym,
                                              a_type_ptr   delete_type,
                                              a_boolean    *ambiguous)
/*
Given the symbol for an operator delete() (which may be overloaded and/or
be a projection symbol), find the default version (which may be the sized
and/or aligned version) and return a pointer to its symbol (which may be a
projection symbol), or NULL if it is not found or there is an ambiguity.
delete_type is the type of the object being deleted (not the delete
expression -- which is a pointer).  If there is an ambiguity return
*ambiguous set to TRUE.
*/
{
  an_overload_set_traversal_block
                 ostblock;
  a_boolean      ambiguous_alternate = FALSE, is_class_member;
  a_boolean      overaligned_type;
  a_symbol_ptr   fund_sym, default_sym = NULL, alternate_default_sym = NULL;
  a_routine_ptr  rp;
  a_boolean      is_sized_ver, use_alternate = FALSE;
  a_boolean      destroying_delete_exists = FALSE;
  a_symbol_ptr   syms[2][2];
  a_boolean      ambig[2][2];

  is_class_member = sym->is_class_member;
  overaligned_type = type_is_overaligned_for_new(delete_type);
start_over:
  *ambiguous = FALSE;
  syms[0][0] = NULL;
  syms[0][1] = NULL;
  syms[1][0] = NULL;
  syms[1][1] = NULL;
  ambig[0][0] = FALSE;
  ambig[0][1] = FALSE;
  ambig[1][0] = FALSE;
  ambig[1][1] = FALSE;
  for (sym = set_up_overload_set_traversal_simple(sym, &ostblock);
       sym != NULL;
       sym = next_symbol_in_overload_set(&ostblock)) {
    fund_sym = sym;
    if (sym->kind == (a_symbol_kind)sk_projection) {
      /* An overload set can contain a projection symbol as the result of a
         using declaration. */
      if (sym->ambiguous) {
        /* All bets are off if the symbol is ambiguous. */
        *ambiguous = TRUE;
        break;
      }  /* if */
      fund_sym = fundamental_symbol_of(sym);
    }  /* if */
    /* Ignore function templates. */
    if (is_function_symbol(fund_sym)) {
      a_boolean is_aligned_delete, is_destroying_delete;
      /* See if this is a default operator delete. */
      rp = fund_sym->variant.routine.ptr;
      if (is_default_operator_delete(rp, &is_sized_ver, &is_aligned_delete,
                                     &is_destroying_delete)) {
        if (destroying_operator_delete_enabled) {
          if (is_destroying_delete && !destroying_delete_exists) {
            /* First time through the loop and we found that the overload
               set contains a destroying operator delete; re-start the
               process, this time looking only at destroying operator delete
               routines. */
            destroying_delete_exists = TRUE;
            goto start_over;
          } else if (destroying_delete_exists && !is_destroying_delete) {
            /* Only destroying deletes are considered. */
            continue;
          }  /* if */
        }  /* if */
        /* Check for ambiguity and record the symbol. */
        if (syms[is_sized_ver][is_aligned_delete] != NULL) {
          /* Already saw a symbol for this version, so it is ambiguous. */
          ambig[is_sized_ver][is_aligned_delete] = TRUE;
        } else {
          syms[is_sized_ver][is_aligned_delete] = sym;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  if (!*ambiguous) {
    int preferred_index = overaligned_type ? 1 : 0;
    int non_preferred_index = 1 - preferred_index;
    /* If the type being deleted is overaligned, then the aligned delete
       versions are preferred.  Otherwise the non-aligned versions are
       preferred.  If overaligned allocation is not enabled, there are no
       non-preferred delete routines.*/
    if (syms[0][preferred_index] != NULL ||
        syms[1][preferred_index] != NULL ||
        !overaligned_allocation_enabled) {
      /* A preferred delete was seen; ignore any non-preferred delete
         functions. */
      default_sym = syms[0][preferred_index];
      *ambiguous = ambig[0][preferred_index];
      alternate_default_sym = syms[1][preferred_index];
      ambiguous_alternate = ambig[1][preferred_index];
    } else {
      /* No preferred delete was seen; use the non-preferred delete
         functions. */
      default_sym = syms[0][non_preferred_index];
      *ambiguous = ambig[0][non_preferred_index];
      alternate_default_sym = syms[1][non_preferred_index];
      ambiguous_alternate = ambig[1][non_preferred_index];
    }  /* if */
  }  /* if */
  if (*ambiguous) {
    default_sym = NULL;
  } else if (alternate_default_sym != NULL && default_sym != NULL &&
             sized_deallocation_enabled && !is_class_member &&
             !is_incomplete_type(delete_type)) {
    /* If a sized usual deallocation function is found as well as an
       unsized usual deallocation function, use the sized version (but do
       this only for the global versions -- when both default versions are
       present in a class the sized version is not considered a "usual
       deallocation function"). */
#if NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE
    check_assertion(alternate_default_sym->kind == (a_symbol_kind)sk_routine &&
                    alternate_default_sym->variant.routine.ptr->special_kind ==
                                        (a_special_function_kind)sfk_operator);
    if (alternate_default_sym->variant.routine.ptr->variant.opname_kind ==
                                           (an_opname_kind)onk_array_delete &&
        !new_or_delete_type_requires_array_handling(delete_type,
#if IA64_ABI
                                                    /*check_constructor=*/FALSE
#else /* !IA64_ABI */
                                                    /*check_constructor=*/TRUE
#endif /* IA64_ABI */
                                                                           )) {
        /* In cases where the deallocation is for an array and the size of the
           array is not known (because there's no cookie stored at the
           beginning of the array), don't use the sized allocation routine.
           For example: "delete [] new int[5]". */
      } else
#endif /* NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE */
      /* Do not insert code here. */
      {
        use_alternate = TRUE;
      }  /* if */
  } else if (is_class_member && default_sym == NULL) {
    /* No ordinary default operator delete has been found, but maybe an
       alternative version was declared.  If so, return it. */
    use_alternate = TRUE;
  }  /* if */
  if (use_alternate) {
    if (ambiguous_alternate) {
      *ambiguous = TRUE;
    } else {
      default_sym = alternate_default_sym;
    }  /* if */
  }  /* if */
  return default_sym;
}  /* find_default_operator_delete_sym */


a_symbol_ptr find_corresponding_operator_delete_sym(a_symbol_ptr op_new_sym,
                                                    a_type_ptr   class_type,
                                                    a_type_ptr   delete_type,
                                                    a_boolean    placement_new,
                                                    a_boolean    template_okay,
                                                    a_boolean    *ambiguous,
                                                    a_symbol_ptr *overload_sym)
/*
op_new_sym is a symbol for an operator new function; it cannot be a projection
symbol or an overload set.  Looking in the scope of class_type, or in the
global scope if class_type is NULL, find and return the corresponding
operator delete function (i.e., the operator delete function with identical
parameter types as the operator new function, excluding the first parameter
in each).  delete_type is the type of the object to be deleted.  Return NULL if
no match is found or if there is an ambiguity; in the latter case, return
*ambiguous set to TRUE.  If placement_new is TRUE, don't attempt to find a
matching default operator delete function.  Otherwise, attempt to determine
from the signature of the operator new whether it's a placement "new" or not.
If template_okay is TRUE, simply return the symbol for a matching function
template, if appropriate; otherwise, return the symbol for the instance.  Also
return in *overload_sym the result of looking up the delete operator; it may
be the same as the symbol that is returned as the corresponding operator
delete symbol, but it may be an overload symbol instead.

Note that this routine handles both new and array new cases (returning
delete and array delete operator functions as appropriate).
*/
{
  a_symbol_ptr   sym = NULL;
  a_symbol_ptr   corresp_op_delete_sym = NULL;
  a_routine_ptr  rp;
  an_opname_kind delete_opname_kind;

  db_enter(4, "find_corresponding_operator_delete_sym");
  check_assertion(op_new_sym->kind == (a_symbol_kind)sk_routine ||
                  op_new_sym->kind == (a_symbol_kind)sk_member_function);
  *ambiguous = FALSE;
  rp = op_new_sym->variant.routine.ptr;
  delete_opname_kind = (rp->variant.opname_kind == (an_opname_kind)onk_new) ?
                         (an_opname_kind)onk_delete :
                         (an_opname_kind)onk_array_delete;
  if (class_type != NULL)  {
    /* Class member. */
    sym = opname_member_function_symbol(delete_opname_kind, class_type);
    if (sym == NULL) {
      /* There is no delete/array-delete operator declared in the given
         class, so we need to find a corresponding operator in the global
         scope instead (as required by WP 5.3.4 [expr.new] and 12.5
         [class.free]). */
      sym = opname_function_symbol(delete_opname_kind);
    }  /* if */
  } else {
    /* Global operator new. */
    sym = opname_function_symbol(delete_opname_kind);
  }  /* if */
  *overload_sym = sym;
  if (sym != NULL) {
    a_boolean aligned_new = FALSE;
    if (!placement_new &&
        is_default_operator_new(rp, &aligned_new)) {
      /* This is a default (sized or sized + aligned) operator new, so find
         the best matching default operator delete. */
      corresp_op_delete_sym = find_default_operator_delete_sym(sym,
                                                               delete_type,
                                                               ambiguous);
    } else {
      /* Placement new.  We need to examine all the delete operators and look
         for a type match. */
      an_overload_set_traversal_block
                       ostblock;
      a_routine_type_supplement_ptr
                       rtsp;
      a_param_type_ptr op_new_param_type_list, op_new_ptp, ptp;
      a_symbol_ptr     fund_sym;
      a_boolean        op_new_has_ellipsis;
      a_boolean        any_template_seen = FALSE;

      rtsp = skip_typerefs(rp->type)->variant.routine.extra_info;
      op_new_has_ellipsis = rtsp->has_ellipsis;
      op_new_param_type_list = rtsp->param_type_list;
      /*lint --e{446,850} sym modified in loop (LINTBUG) */
      for (sym = set_up_overload_set_traversal_simple(sym, &ostblock);
           sym != NULL;
           sym = next_symbol_in_overload_set(&ostblock)) {
        if (sym->kind == (a_symbol_kind)sk_projection) {
          /* An overload set can contain a projection symbol as the result of
             a using-declaration. */
          if (sym->ambiguous) {
            /* All bets are off if the symbol is ambiguous. */
            sym = NULL;
            *ambiguous = TRUE;
            break;
          }  /* if */
          fund_sym = fundamental_symbol_of(sym);
        } else {
          fund_sym = sym;
        }  /* if */
        if (fund_sym->kind == (a_symbol_kind)sk_function_template) {
          /* Look for a matching template only if there's no match among the
             non-template functions. */
          any_template_seen = TRUE;
        } else {
          check_assertion(is_function_symbol(fund_sym));
          rtsp = skip_typerefs(fund_sym->variant.routine.ptr->type)->
                                               variant.routine.extra_info;
          if ((a_boolean)rtsp->has_ellipsis != op_new_has_ellipsis) {
            /* There can't be a match unless both were declared with ellipsis
               or neither was. */
            continue;
          }  /* if */
          ptp = rtsp->param_type_list;
          check_assertion(ptp != NULL);
          for (ptp = ptp->next, op_new_ptp = op_new_param_type_list->next;
               ptp != NULL && op_new_ptp != NULL;
               ptp = ptp->next, op_new_ptp = op_new_ptp->next) {
            if (!identical_types(ptp->type, op_new_ptp->type)) {
              /* No match. */
              goto next_delete_symbol;
            }  /* if */
            /* Keep looping as long as the types are identical and as long as
               there are still entries to compare on both lists. */
          }  /* for */
          if (ptp == NULL && op_new_ptp == NULL) {
            /* Both lists were the same length: a match was found. */
            if (corresp_op_delete_sym == NULL) {
              corresp_op_delete_sym = sym;
              /* Keep looping, in case there is an ambiguity. */
            } else {
              /* An ambiguity, presumably introduced into the overload set by
                 a using-declaration, has been encountered. */
              *ambiguous = TRUE;
              corresp_op_delete_sym = NULL;
              break;
            }  /* if */
          }  /* if */
        }  /* if */
next_delete_symbol:;
      }  /* for */
      if (any_template_seen) {
        a_partial_order_candidate_ptr	candidate_list = NULL;
        a_symbol_ptr			template_sym;
        a_template_arg_ptr		templ_arg_list;
        /* The overload set included at least one function template. */
        if (corresp_op_delete_sym == NULL && !(*ambiguous)) {
          /* There was no match among the ordinary functions, so see if the
             template function(s) satisfy the need. */
          a_type_ptr  tp, saved_return_type, saved_first_param_type;

          tp = skip_typerefs(rp->type);
          /* Change the return type from void * to void. */
          saved_return_type = tp->variant.routine.return_type;
          tp->variant.routine.return_type = void_type();
          /* Change the first parameter type from size_t to void *. */
          saved_first_param_type = op_new_param_type_list->type;
          op_new_param_type_list->type = make_pointer_type(void_type());
          for (sym = set_up_overload_set_traversal_simple(*overload_sym,
                                                          &ostblock);
               sym != NULL;
               sym = next_symbol_in_overload_set(&ostblock)) {
            fund_sym = fundamental_symbol_of(sym);
            if (fund_sym->kind == (a_symbol_kind)sk_function_template) {
              if (has_matching_template_function(
                                       fund_sym, tp, (a_template_arg_ptr)NULL,
                                       /*is_decl_context=*/TRUE,
                                       /*ignore_noexcept=*/FALSE)) {
                /* We have a match.  Add the matching template to a list of
                   matching candidates.  Any poorer matches will be removed
                   by this process. */
                add_to_partial_order_candidates_list(&candidate_list, sym,
                                                     (a_template_arg_ptr)NULL);
              }  /* if */
            }  /* if */
          }  /* for */
          if (candidate_list != NULL) {
            /* If any of the templates matched, select the best one using
               the partial ordering rules. */
            select_best_partial_order_candidate(
                           candidate_list, (a_symbol_ptr)NULL, &template_sym,
                           &templ_arg_list, ambiguous);
            if (!*ambiguous) {
              if (template_okay) {
                /* A template can be returned to the caller. */
                corresp_op_delete_sym = template_sym;
              } else {
                /* Do a partial instantiation if a match is found so that the
                   template instance can be returned. */
                a_boolean  is_new_template_instance;
                corresp_op_delete_sym = matching_template_function(
 					   template_sym, tp,
                                           (a_template_arg_ptr)NULL,
				           /*explicit_arg_list_present=*/FALSE,
                                           /*is_decl_context=*/TRUE,
                                           /*ignore_noexcept=*/FALSE,
                                           /*in_class_specialization=*/FALSE,
                                           &is_new_template_instance);
              }  /* if */
            }  /* if */
          }  /* if */
          /* Restore the function type for the operator new. */
          tp->variant.routine.return_type = saved_return_type;
          op_new_param_type_list->type = saved_first_param_type;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 4) {
    db_symbol(op_new_sym, "operator new is: ", 2);
    if (class_type != NULL) {
      fputs("lookup class is: ", f_debug);
      db_type_name(class_type);
      fputc('\n', f_debug);
    }  /* if */
    if (corresp_op_delete_sym == NULL) {
      fputs("no corresponding operator delete was found\n", f_debug);
    } else {
      db_symbol(corresp_op_delete_sym,
                "corresponding operator delete is: ", 2);
    }  /* if */
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  db_exit();
  return corresp_op_delete_sym;
}  /* find_corresponding_operator_delete_sym */


a_symbol_ptr make_predeclared_function_symbol(a_symbol_locator  *locator,
                                              a_type_ptr        rout_type)
/*
Create a symbol and routine entry for a predeclared function.  locator points
to a symbol locator created to represent the entity's name.  rout_type is the
associated function type and must be a tk_routine entry (i.e., not a typeref).
*/
{
  a_symbol_ptr                   ext_sym;
  a_type_ptr                     old_type;
  an_id_linkage_kind             linkage;
  a_func_info_block              func_info;
  a_decl_parse_state             dps;

  check_assertion(rout_type->kind == (a_type_kind)tk_routine);
  clear_func_info(&func_info);
  init_decl_parse_state(&dps);
  /* Create the symbol and routine entry.  Note that the routine entry
     is given a storage class of sc_extern since there is no definition
     in the current translation unit. */
  dps.storage_class = dps.declared_storage_class = (a_storage_class)sc_extern;
  dps.type = rout_type;
  decl_routine(locator, &dps, &func_info, SRK_DECLARATION, &linkage, &old_type,
               &ext_sym, (a_decl_pos_block_ptr)NULL);
  dps.sym->variant.routine.ptr->compiler_generated = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode) {
    /* Predeclared functions should use __cdecl calling convention.  If that's
       not the default for the compilation, set it now. */
    if (default_calling_convention != (a_calling_convention)cc_cdecl) {
      rout_type->variant.routine.extra_info->calling_convention =
                                               (a_calling_convention)cc_cdecl;
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  return dps.sym;
}  /* make_predeclared_function_symbol */


void make_global_operator_new_or_delete_symbol(an_opname_kind  opname,
                                               a_boolean       sized_version,
                                               a_boolean       aligned_version)
/*
Create a symbol and routine entry for ::operator new, ::operator new[],
::operator delete, or ::operator delete[].  When sized_version is TRUE (for
operator delete only), a second argument, of type size_t, is added.  When
aligned_version is TRUE, a version that takes a std::align_val_t argument
is added.  These are entered into the symbol table as part of
initialization, so the locator has a default value (as used with keywords).
The routine entry is marked as compiler generated; if a user declaration
appears later, the compiler-generated flag should be cleared.
*/
{
  a_symbol_locator               locator;
  a_type_ptr                     return_type, param1_type, param2_type = NULL;
  a_type_ptr                     param3_type = NULL;
  a_symbol_ptr                   sym;
  a_routine_type_supplement_ptr  rtsp;

  db_enter(5, "make_global_operator_new_or_delete_symbol");
  check_assertion_str(is_new_operator(opname) || is_delete_operator(opname),
                      "global_operator_new_or_delete_symbol: bad opname kind");
  /* Create a locator for the symbol that is to be created. This will also
     create the symbol header. */
  make_opname_locator(opname, &locator, &null_source_position);
  if (is_new_operator(opname)) {
    check_assertion(!sized_version);
    /* Return type for operator new is void *. */
    return_type = make_pointer_type(void_type());
    /* Type of the one parameter for operator new is size_t. */
    param1_type = integer_type(targ_size_t_int_kind);
    if (aligned_version) {
      param2_type = type_of_align_val_t;
    }  /* if */
  } else {
    /* Return type of operator delete is void. */
    return_type = void_type();
    /* Type of the one parameter for operator delete is void *. */
    param1_type = make_pointer_type(void_type());
    if (sized_version) {
      /* Global sized deallocation routines take a second parameter of type
         size_t. */
      param2_type = integer_type(targ_size_t_int_kind);
      if (aligned_version) {
        param3_type = type_of_align_val_t;
      }  /* if */
    } else if (aligned_version) {
      param2_type = type_of_align_val_t;
    }  /* if */
  }  /* if */
  /* In Microsoft mode, array versions of the operators are never directly
     predeclared.  Instead, alias symbols for the array versions are sometimes
     implicitly created (see below) when the non-array versions are
     predeclared. */
  check_assertion(!(ms_extensions &&
                    (opname == (an_opname_kind)onk_array_new ||
                     opname == (an_opname_kind)onk_array_delete)));
  sym = make_predeclared_function_symbol(
                 &locator,
                 make_routine_type(return_type, param1_type, param2_type,
                                   param3_type, (a_type_ptr)NULL));
  if (microsoft_mode) {
    if (microsoft_version >= 1400 && !sized_version && !aligned_version) {
      /* More recent Microsoft compilers treat the implicit declaration of
         array new and array delete as synonyms for the corresponding non-
         array versions.  A user declaration of these functions does
         declare a distinct routine, however.  Sized and aligned versions
         don't get an alias. */
      a_symbol_ptr  av_sym;
      if (opname == (an_opname_kind)onk_new) {
        make_opname_locator((an_opname_kind)onk_array_new, &locator,
                            &null_source_position);
      } else {
        check_assertion(opname == (an_opname_kind)onk_delete);
        make_opname_locator((an_opname_kind)onk_array_delete, &locator,
                            &null_source_position);
      }  /* if */
      av_sym = enter_local_symbol((a_symbol_kind)sk_routine, &locator,
                                  (a_scope_depth)DEPTH_OF_FILE_SCOPE,
                                  /*suppress_redecl_error=*/FALSE);
      av_sym->variant.routine.ptr = sym->variant.routine.ptr;
    }  /* if */
  } else if (exceptions_enabled) {
    if (is_delete_operator(opname)) {
      /* In C++11, deallocation functions changed from "throw()" to
         "noexcept". */
      if (!cpp11_mode) {
        rtsp = sym->variant.routine.ptr->type->variant.routine.extra_info;
        rtsp->exception_specification = alloc_exception_specification();
      }  /* if */
    } else {
      /* Putting out "throw(std::bad_alloc)" for the predeclared operator new
         is not yet implemented; it would entail predeclaring namespace std
         and class std::bad_alloc (and probably class std::exception). */
    }  /* if */
  }  /* if */
  db_exit();
}  /* make_global_operator_new_or_delete_symbol */

#if MICROSOFT_EXTENSIONS_ALLOWED

void make_predeclared_alloca_symbol(void)
/*
Create a symbol and routine entry for predeclared _alloca (only in Microsoft
C compatibility mode).
*/
{
  a_symbol_locator               locator;
  a_type_ptr                     return_type, param1_type;

  db_enter(5, "make_predeclared_alloca_symbol");
  check_assertion(ms_extensions && C_mode());
  /* Create a locator for the symbol that is to be created.  This will also
     create the symbol header. */
  clear_locator(&locator, &null_source_position);
  (void)find_symbol("_alloca", (sizeof_t)7, &locator);
  /* Return type for _alloca is void *. */
  return_type = make_pointer_type(void_type());
  /* One parameter -- the size. */
  param1_type = integer_type(targ_size_t_int_kind);
  (void)make_predeclared_function_symbol(
            &locator,
            make_routine_type(return_type, param1_type, (a_type_ptr)NULL,
                              (a_type_ptr)NULL, (a_type_ptr)NULL));
  db_exit();
}  /* make_predeclared_alloca_symbol */


static a_symbol_ptr make_predeclared_typedef(a_type_ptr    tp,
                                             a_const_char  *name)
/*
Create a typedef of the given name for the given type and return the
associated symbol.
*/
{
  a_symbol_locator    locator;
  a_decl_parse_state  state;

  init_decl_parse_state(&state);
  state.type = tp;
  clear_locator(&locator, &null_source_position);
  (void)find_symbol(name, strlen(name), &locator);
  decl_typedef(&locator, &state, (a_type_ptr)NULL, (a_decl_pos_block_ptr)NULL);
  return state.sym;
}  /* make_predeclared_typedef */


void make_predeclared_size_t_symbol(void)
/*
Create a symbol and type entry for size_t (only in Microsoft mode).
C++ note: for Microsoft compatibility, the entries are recorded in the file
scope, not in namespace std.
*/
{
  db_enter(5, "make_predeclared_size_t_symbol");
  check_assertion(ms_extensions);
  predeclared_size_t_symbol = make_predeclared_typedef(
                                integer_type(targ_size_t_int_kind), "size_t");
  /* Setting the defined flag to FALSE indicates there is (as yet) no explicit
     definition in the source program. */
  predeclared_size_t_symbol->defined = FALSE;
  db_exit();
}  /* make_predeclared_size_t_symbol */


void make_predeclared_bool_symbol(void)
/*
Create a symbol and type entry for bool (only in Microsoft mode); the entries
are recorded in the file scope.
*/
{
  db_enter(5, "make_predeclared_bool_symbol");
  check_assertion(microsoft_mode);
  (void)make_predeclared_typedef(bool_type(), "bool");
  db_exit();
}  /* make_predeclared_bool_symbol */


void make_predeclared_nullptr_t_symbol(void)
/*
Create a symbol and type entry for std::nullptr_t (only in Microsoft mode).
*/
{
  a_namespace_ptr std_namespace;
  a_symbol_ptr    nullptr_t_sym;

  db_enter(5, "make_predeclared_nullptr_t_symbol");
  check_assertion(ms_extensions && symbol_for_namespace_std != NULL);
  std_namespace = symbol_for_namespace_std->variant.namespace_info.ptr;
  (void)push_namespace_scope((a_scope_kind)sck_namespace_extension,
                             std_namespace);
  nullptr_t_sym = make_predeclared_typedef(standard_nullptr_type(),
                                           "nullptr_t");
  nullptr_t_sym->defined = FALSE;
  pop_namespace_scope();
}  /* make_predeclared_nullptr_t_symbol */


a_symbol_ptr make_cppcli_unresolved_type_symbol(a_constant_ptr  name_con)
/*
Create a class symbol with the name represented by the given string literal
and return it.  The symbol is not entered in the symbol table.
This is used for C++/CLI "unresolved types" (types referenced in assemblies
from other assemblies that haven't been loaded yet).
*/
{
  a_symbol_header_ptr  sym_hdr = alloc_symbol_header();
  a_symbol_ptr         sym;

  check_assertion(name_con->kind == (a_constant_repr_kind)ck_string);
  set_identifier_for_symbol_header(sym_hdr, name_con->variant.string.value,
                                   (sizeof_t)name_con->variant.string.length,
                                   /*is_unnamed=*/FALSE);
  sym = alloc_symbol((a_symbol_kind)sk_class_or_struct_tag, sym_hdr,
                     &null_source_position);
  sym->decl_scope = FILE_SCOPE_NUMBER;
  return sym;
}  /* make_cppcli_unresolved_type_symbol */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

void add_on_diag_for_skipped_inaccessible_function(a_symbol_ptr     sym,
                                                   a_diagnostic_ptr dp)
/*
If sym is non-NULL, it is the symbol for function that was skipped in
overload resolution because it is inaccessible (with C++/CLI
hide-by-sig lookup), but which is viable when no accessible function
turned out to be viable.  Issue an add-on diagnostic that identifies
the function.  dp is the diagnostic to which the message should be
added.
*/
{
  if (sym != NULL) {
    check_assertion(cli_or_cx_enabled);
    sym_add_diag_info(dp, ec_skipped_inaccessible_function, sym);
  }  /* if */
}  /* add_on_diag_for_skipped_inaccessible_function */


a_routine_ptr select_default_constructor_full(
                                         a_type_ptr        class_type,
                                         a_source_position *err_pos,
                                         a_type_ptr        object_class_type,
                                         a_boolean         declarative_context,
                                         a_boolean         evaluated,
                                         a_boolean         check_access,
                                         a_boolean         no_explicit,
                                         a_boolean         *error_detected,
                                         a_boolean         *err)
/*
Find and return a pointer to a routine representing a default constructor for
the class indicated by class_type.  (A default constructor is a constructor
that requires no arguments.)  If the class has no default constructor,
or a trivial default constructor, return NULL.  If no acceptable constructor
is found, issue a diagnostic and return NULL.  If more than one acceptable
constructor is found, issue a (different) diagnostic and return NULL.
On the error cases, if err is non-NULL return *err set to TRUE.
Check access to the constructor (if check_access is TRUE) and issue an
error if the constructor is not accessible.  object_class_type points
to the type of the object being created; class_type may be a base
class of object_class_type.  This is needed for protected member
access checking.  If declarative_context is TRUE, the function is being
called in a declarative context (where default arguments are not parsed);
otherwise, this is a call in an expression context (where default arguments
must have been parsed).  If evaluated is FALSE, the reference is within an
unevaluated expression.  If error_detected is non-NULL, return
*error_detected set to TRUE if there was an error, and do not issue
any diagnostics (including warnings).  (That return value does not
quite duplicate the one in *err, because it includes access errors
and *err does not.)  This routine does consider template constructors
that can be called with zero arguments.
*/
{
  a_routine_ptr ctor_routine = NULL;
  a_symbol_ptr  ctor_sym;
  a_symbol_ptr  inaccessible_match = NULL;
  a_boolean     local_err = FALSE, ambiguous, trivial;

  /* This routine is similar to select_overloaded_function. */
  if (error_detected != NULL) *error_detected = FALSE;
  class_type = skip_typerefs(class_type);
  ctor_sym = find_default_constructor(class_type, /*include_templates=*/TRUE,
                                      declarative_context, no_explicit,
                                      err_pos, &ambiguous,
                                      (error_detected == NULL ?
                                         &inaccessible_match :
                                         (a_symbol **)NULL),
                                      &trivial);
  if (ambiguous) {
    /* More than one default constructor. */
    if (error_detected != NULL) {
      *error_detected = TRUE;
    } else {
      pos_ty_error(ec_ambiguous_default_constructor, err_pos, class_type);
    }  /* if */
    local_err = TRUE;
  } else if (ctor_sym == NULL || is_ineligible(ctor_sym)) {
    if (trivial && ctor_sym == NULL) {
      /* The class has an implicit (not user-declared) trivial default
         constructor.  Note that we get here for the combination of
         ctor_sym == NULL and trivial TRUE.  ctor_sym != NULL and
         trivial TRUE indicates a user-declared trivial default constructor
         and is handled below. */
      ctor_routine = NULL;
    } else {
      /* No default constructor at all. */
      if (error_detected != NULL) {
        *error_detected = TRUE;
      } else {
        a_diagnostic_ptr dp;
        an_error_code    err_code = ec_no_default_constructor;
        if (ctor_sym != NULL && is_ineligible(ctor_sym)) {
          err_code = ec_ineligible_default_constructor;
        } else if (ctor_sym == NULL && no_explicit) {
          /* Check for the case of an explicit defaulted trivial
             constructor. */
          a_class_symbol_supplement  *cssp;
          cssp = class_symbol_supp(symbol_for(class_type));
          if (has_explicit_trivial_default_ctor(cssp)) {
            err_code = ec_default_constructor_is_explicit;
          }  /* if */
        }  /* if */
        dp = pos_ty_start_error(err_code, err_pos, class_type);
        add_on_diag_for_skipped_inaccessible_function(inaccessible_match, dp);
        end_diagnostic(dp);
      }  /* if */
      local_err = TRUE;
    }  /* if */
  } else {
    /* Exactly one default constructor. */
    ctor_routine = ctor_sym->variant.routine.ptr;
    if (ctor_routine->is_deleted) {
      /* An error is going to be reported by the call to
         reference_to_implicitly_invoked_function. */
      check_assertion_or_expect_error(error_detected != NULL);
      local_err = TRUE;
    } else if (ctor_routine->is_trivial_default_constructor) {
      /* The constructor is a trivial default constructor.  Check access
         but do not mark it as referenced (because there will be no call). */
      evaluated = FALSE;
      /* Return NULL from this routine. */
      ctor_routine = NULL;
    }  /* if */
    /* Check that the constructor is accessible and mark it referenced. */
    reference_to_implicitly_invoked_function(ctor_sym, err_pos,
                                             object_class_type,
                                             /*honor_virtual=*/FALSE,
                                             evaluated,
                                             /*instantiate=*/TRUE,
                                             check_access,
                                             /*elided_reference=*/FALSE,
                                             error_detected);
  }  /* if */
  if (err != NULL) *err = local_err;
  return ctor_routine;
}  /* select_default_constructor_full */


a_routine_ptr select_default_constructor(a_type_ptr        class_type,
                                         a_source_position *err_pos,
                                         a_type_ptr        object_class_type,
                                         a_boolean         *err)
/*
Interface to select_default_constructor for the simple case.
*/
{
  a_routine_ptr ctor_routine;

  ctor_routine = select_default_constructor_full(class_type,
                                                 err_pos,
                                                 object_class_type,
                                                 /*declarative_context=*/FALSE,
                                                 /*evaluated=*/TRUE,
                                                 /*check_access=*/TRUE,
                                                 /*no_explicit=*/FALSE,
                                                 /*error_detected=*/
                                                             (a_boolean *)NULL,
                                                 err);
  return ctor_routine;
}  /* select_default_constructor */


a_routine_ptr select_destructor_full(a_type_ptr        class_type,
                                     a_type_ptr        object_class_type,
                                     a_source_position *position,
                                     a_boolean         honor_virtual,
                                     a_boolean         evaluated,
                                     a_boolean         instantiate,
                                     a_boolean         check_access,
                                     a_boolean         *error_detected)
/*
If the indicated class has a destructor, check that it is accessible (if
check_access is TRUE), mark it as referenced, and return a pointer to
the routine entry.  Otherwise, return NULL.  object_class_type points
to the type of the object being destroyed; class_type may be a base
class of object_class_type.  This is needed for protected member
access checking.  object_class_type can be NULL if that checking is
not needed.  If honor_virtual is TRUE, and if the destructor is
virtual, consider this reference a virtual function call.  If
evaluated is FALSE, the reference is within an unevaluated expression.
If instantiate is TRUE, the destructor is instantiated if necessary.
*position is the source position of the reference.  If error_detected
is non-NULL, return *error_detected set to TRUE if there was an error,
and do not issue any diagnostics (including warnings).
*/
{
  a_symbol_ptr  dtor_sym;
  a_routine_ptr dtor_routine = NULL;
  a_class_symbol_supplement_ptr
                cssp;

  if (error_detected != NULL) *error_detected = FALSE;
  class_type = skip_typerefs(class_type);
  cssp = class_symbol_supp(symbol_for(class_type));
  if (cssp != NULL) {
    dtor_sym = cssp->destructor;
    if (dtor_sym != NULL) {
      if (object_class_type != NULL) {
        object_class_type = skip_typerefs(object_class_type);
      }  /* if */
      if (microsoft_mode && microsoft_version < 1400 &&
          object_class_type != NULL &&
          object_class_type->variant.class_struct_union.dtor_decl_suppressed &&
          !have_access_to_symbol(dtor_sym)) {
        /* MSVC++ versions before 8.0 simply do not invoke an inaccessible
           subobject destructor if the declaration of the complete object's
           destructor was suppressed. */
        if (error_detected != NULL) {
          if (is_effective_error(ec_inaccessible_dtor_not_invoked,
                                 es_warning, position)) {
            *error_detected = TRUE;
          }  /* if */
        } else {
          pos_ty2_diagnostic(es_warning, ec_inaccessible_dtor_not_invoked,
                             position, class_type, object_class_type);
        }  /* if */
      } else {
        /* Check that the destructor is accessible and mark it referenced. */
        reference_to_implicitly_invoked_function(
               dtor_sym, position, object_class_type, honor_virtual,
               evaluated && has_deleted_or_nontrivial_destructor(cssp),
               instantiate, check_access, /*elided_reference=*/FALSE,
               error_detected);
        dtor_routine = dtor_sym->variant.routine.ptr;
      }  /* if */
      if (!has_deleted_or_nontrivial_destructor(cssp) && evaluated) {
        /* A trivial destructor (e.g., a defaulted destructor).  Treat it as
           an implicitly-declared destructor (i.e., return NULL) if this is
           an evaluated context.  (In an unevaluated context, we may be
           checking whether the destructor throws, which is possible with
           something like:
             struct S { ~S() noexcept(false) = default; };
           In that case, the caller needs the actual destructor entry. */
        dtor_routine = NULL;
      }  /* if */
    } else if (class_type->variant.class_struct_union.dtor_decl_suppressed &&
               microsoft_version >= 1400) {
      /* MSVC++ 8.0 issues an error if a suppressed destructor would have
         been called. */
      if (error_detected != NULL) {
        if (is_effective_sfinae_error(ec_suppressed_dtor_needed,
                                      es_discretionary_error, position)) {
          *error_detected = TRUE;
        }  /* if */
      } else {
        pos_ty_diagnostic(es_discretionary_error, ec_suppressed_dtor_needed,
                          position, class_type);
      }  /* if */
    }  /* if */
  }  /* if */
  return dtor_routine;
}  /* select_destructor_full */


a_routine_ptr select_destructor(a_type_ptr        class_type,
				a_type_ptr        object_class_type,
                                a_source_position *position)
/*
Interface to select_destructor_full for the simple case.
*/
{
  a_routine_ptr dtor_routine;

  dtor_routine = select_destructor_full(class_type,
                                        object_class_type,
                                        position,
                                        /*honor_virtual=*/FALSE,
                                        /*evaluated=*/TRUE,
                                        /*instantiate=*/TRUE,
                                        /*check_access=*/TRUE,
                                        /*error_detected=*/(a_boolean *)NULL);
  return dtor_routine;
}  /* select_destructor */


a_routine_ptr select_copy_constructor_full(
                                  a_type_ptr            class_type,
                                  a_type_qualifier_set  required_qualifiers,
                                  a_boolean             source_is_rvalue,
                                  a_source_position     *err_pos,
                                  a_type_ptr            object_class_type,
                                  a_boolean             *class_bitwise_copy,
                                  a_boolean             record_ref,
                                  a_boolean             evaluated,
                                  a_boolean             allow_suppressed_ctor,
                                  a_boolean             check_access,
                                  a_boolean             *error_detected)
/*
Find and return a pointer to a routine representing a copy constructor for
the class indicated by class_type and accepting a first parameter whose type
is qualified as specified by required_qualifiers, and an rvalue
(including xvalue) if source_is_rvalue is TRUE.  If no acceptable copy
constructor is found, issue a diagnostic and return NULL.  If more than
one acceptable copy constructor is found, issue a (different) diagnostic and
return NULL.  object_class_type points to the type of the object being
copied; class_type may be a base class of object_class_type.  This is needed
for protected member access checking.  If the copy constructor selected
is implicit (not user-declared, i.e., there's no associated symbol)
and performs a bitwise copy, return NULL and *class_bitwise_copy TRUE.
If record_ref is TRUE, a reference is recorded against the copy
constructor selected; as a side effect, access to the copy constructor
is checked (if check_access is TRUE), and an error issued if the copy
constructor is inaccessible.  If evaluated is FALSE, the reference is
within an unevaluated expression.  If class_type has no copy
constructor because its declaration was suppressed, no diagnostic will
be emitted if allow_suppressed_ctor is TRUE.  If error_detected is
non-NULL, return *error_detected set to TRUE if there was an error,
and do not issue any diagnostics (including warnings).
*/
{
  a_symbol_ptr  cctor_sym;
  a_routine_ptr cctor_routine = NULL;
  a_symbol_ptr  inaccessible_match = NULL;
  a_boolean     ambiguous;

  if (error_detected != NULL) *error_detected = FALSE;
  class_type = skip_typerefs(class_type);
  if (object_class_type != NULL) {
    object_class_type = skip_typerefs(object_class_type);
  }  /* if */
  cctor_sym = find_copy_constructor(class_type, required_qualifiers,
                                    source_is_rvalue,
                                    err_pos, &ambiguous,
                                    (error_detected == NULL ?
                                       &inaccessible_match :
                                       (a_symbol **)NULL),
                                    class_bitwise_copy);
  if (*class_bitwise_copy) {
    /* A bitwise copy is allowed. */
    reference_to_trivial_copy_constructor(class_type, object_class_type,
                                          err_pos, check_access,
                                          /*elided_reference=*/FALSE,
                                          error_detected);
  } else if (ambiguous) {
    /* More than one applicable copy constructor. */
    if (error_detected != NULL) {
      *error_detected = TRUE;
    } else {
      pos_ty_error(ec_ambiguous_copy_constructor, err_pos, class_type);
    }  /* if */
  } else if (cctor_sym == NULL || is_ineligible(cctor_sym)) {
    /* No applicable copy constructor. */
    if (class_type->variant.class_struct_union.copy_ctor_decl_suppressed &&
        allow_suppressed_ctor) {
      /* The declaration of the copy constructor was suppressed (for
         Microsoft compatibility).  Do not report an error at this point. */
    } else if (required_qualifiers == TQ_CONST && inaccessible_match == NULL) {
      /* The common case:  missing const copy constructor. */
      if (error_detected != NULL) {
        *error_detected = TRUE;
      } else {
        pos_ty_error(ec_missing_const_copy_constructor, err_pos, class_type);
      }  /* if */
    } else {
      /* Unusual case: volatile or const-volatile expected, or skipped
         because inaccessible. */
      if (error_detected != NULL) {
        *error_detected = TRUE;
      } else {
        a_diagnostic_ptr dp;
        dp = pos_ty_start_error(ec_no_suitable_copy_constructor,
                                err_pos, class_type);
        add_on_diag_for_skipped_inaccessible_function(inaccessible_match, dp);
        end_diagnostic(dp);
      }  /* if */
    }  /* if */
  } else {
    /* Exactly one copy constructor is best. */
    if (record_ref) {
      /* Check that the constructor is accessible and mark it referenced. */
      reference_to_implicitly_invoked_function(cctor_sym, err_pos,
                                               object_class_type,
                                               /*honor_virtual=*/FALSE,
                                               evaluated,
                                               /*instantiate=*/TRUE,
                                               check_access,
                                               /*elided_reference=*/FALSE,
                                               error_detected);
    }  /* if */
    cctor_routine = cctor_sym->variant.routine.ptr;
  }  /* if */
  return cctor_routine;
}  /* select_copy_constructor_full */


a_routine_ptr select_copy_constructor(
                                  a_type_ptr            class_type,
                                  a_type_qualifier_set  required_qualifiers,
                                  a_boolean             source_is_rvalue,
                                  a_source_position     *err_pos,
                                  a_type_ptr            object_class_type,
                                  a_boolean             *class_bitwise_copy,
                                  a_boolean             allow_suppressed_ctor)
/*
Interface to select_copy_constructor_full for the simple case.
*/
{
  a_routine_ptr cctor_routine;

  cctor_routine = select_copy_constructor_full(class_type,
                                               required_qualifiers,
                                               source_is_rvalue,
                                               err_pos,
                                               object_class_type,
                                               class_bitwise_copy,
                                               /*record_ref=*/TRUE,
                                               /*evaluated=*/TRUE,
                                               allow_suppressed_ctor,
                                               /*check_access=*/TRUE,
                                               /*error_detected=*/
                                                            (a_boolean *)NULL);
  return cctor_routine;
}  /* select_copy_constructor */


char *il_entry_for_symbol_null_okay(a_symbol_ptr      sym,
                                    an_il_entry_kind  *kind)
/*
Return a pointer to the IL entry to which the specified symbol refers.  Also
return the kind of IL entry that is found (if kind is not NULL).  If the
symbol is not associated with an IL entry, return NULL, and return kind
set to iek_none.
*/
{
  char             *entry_ptr = NULL;
  an_il_entry_kind lkind = iek_none;

#if EXPENSIVE_CHECKING
  /* If this assertion fails, the caller likely is using a symbol that it
     shouldn't be.  The symbol's associated IL entry belongs to a freed
     memory region (and thus the IL entry no longer exists). */
  check_assertion_str(sym->kind != sk_freed,
                      "attempted to retrieve IL entry for a freed symbol");
#endif /* EXPENSIVE_CHECKING */
  switch (sym->kind) {
    case sk_macro:
#if RECORD_MACROS_IN_IL
      entry_ptr = (char *)il_entry_for_symbol<a_macro>(sym);
      lkind = iek_macro;
#endif /* RECORD_MACROS_IN_IL */
      break;
    case sk_constant:
      entry_ptr = (char *)il_entry_for_symbol<a_constant>(sym);
      lkind = iek_constant;
      break;
    case sk_type:
    case sk_enum_tag:
    case sk_class_or_struct_tag:
    case sk_union_tag:
      entry_ptr = (char *)il_entry_for_symbol<a_type>(sym);
      lkind = iek_type;
      break;
    case sk_variable:
    case sk_static_data_member:
      entry_ptr = (char *)il_entry_for_symbol<a_variable>(sym);
      lkind = iek_variable;
      break;
    case sk_field:
      entry_ptr = (char *)il_entry_for_symbol<a_field>(sym);
      lkind = iek_field;
      break;
    case sk_routine:
    case sk_member_function:
      entry_ptr = (char *)il_entry_for_symbol<a_routine>(sym);
      lkind = iek_routine;
      break;
    case sk_label:
      entry_ptr = (char *)il_entry_for_symbol<a_label>(sym);
      lkind = iek_label;
      break;
    case sk_namespace:
      entry_ptr = (char *)il_entry_for_symbol<a_namespace>(sym);
      lkind = iek_namespace;
      break;
    case sk_function_template:
    case sk_class_template:
    case sk_variable_template:
    case sk_concept_template:
      entry_ptr = (char *)il_entry_for_symbol<a_template>(sym);
      lkind = iek_template;
      break;
    default:;
      /* Other cases ignored. */
  }  /* switch */
  if (entry_ptr == NULL) lkind = iek_none;
  if (kind != NULL) *kind = lkind;
  return entry_ptr;
}  /* il_entry_for_symbol_null_okay */


char *il_entry_for_symbol(a_symbol_ptr      sym,
                          an_il_entry_kind  *kind)
/*
Return a pointer to the IL entry to which the specified symbol refers.  Also
return the kind of IL entry that is found.  Always returns a non-NULL value.
*/
{
  char *entry_ptr = il_entry_for_symbol_null_okay(sym, kind);

  check_assertion_str(entry_ptr != NULL,
                      "il_entry_for_symbol: NULL assoc IL entry ptr");
  return entry_ptr;
}  /* il_entry_for_symbol */


a_source_correspondence *source_corresp_entry_for_symbol(a_symbol_ptr sym_ptr)
/*
Return a pointer to the source correspondence entry in the IL entry
for the given symbol.  Return NULL if there isn't one.
*/
{
  char              *entity_ptr;
  an_il_entry_kind  entity_kind;

  entity_ptr = il_entry_for_symbol_null_okay(sym_ptr, &entity_kind);
  return (entity_ptr == NULL ?
           NULL : source_corresp_for_il_entry(entity_ptr, entity_kind));
}  /* source_corresp_entry_for_symbol */


a_module *module_for_symbol(a_symbol_ptr sym_ptr)
/*
Return a pointer to the module where the given symbol was declared.  Return
NULL if there isn't one.
*/
{
  a_module_ptr  result = NULL;
  a_module_entity_ptr
                introducing_entity = sym_ptr->module_entity;

  if (introducing_entity != NULL) {
    result = introducing_entity->module_info;
  }  /* if */
  return result;
}  /* module_for_symbol */


a_module *lookup_module_for_symbol(a_symbol_ptr sym_ptr)
/*
Return a pointer to the module used for lookup of the given symbol.  Return
NULL if there isn't one.
*/
{
  a_module_ptr result;

  if (is_symbol_globally_visible(sym_ptr)) {
    result = NULL;
  } else {
    result = skip_module_partitions(module_for_symbol(sym_ptr));
  }  /* if */
  return result;
}  /* lookup_module_for_symbol */


a_boolean is_symbol_globally_visible(a_symbol_ptr sym_ptr)
/*
Return TRUE if the given symbol is always (i.e., globally) visible to lookup.
For the symbol to be globally visible to lookup, it must either be part of the
global module or it must have been exported from a module (that has been
imported).  Otherwise, return FALSE.
*/
{
  a_boolean result = FALSE;

  if (sym_ptr->is_class_member) {
    /* [module.interface] specifies that:

         Class and enumeration member names can be found by name lookup in any
         context in which a definition of the type is reachable.

       Thus, all class members are treated as globally visible with the
       visibility check being performed only when the class symbol is involved
       in the name. */
    result = TRUE;
  } else {
    a_module_entity_ptr mep = sym_ptr->module_entity;

    if (is_module_entity_globally_visible(mep)) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* is_symbol_globally_visible */


a_boolean is_symbol_currently_lookup_visible(a_symbol_ptr sym_ptr)
/*
Return TRUE if the given symbol is currently visible to lookup.  For the symbol
to be currently visible to lookup, it must either be globally visible (see the
above function, is_symbol_globally_visible, for more information) or part of
the module that the front end is currently processing.  Otherwise, return
FALSE.
*/
{
  a_boolean result = FALSE;

  if (is_symbol_globally_visible(sym_ptr)) {
    /* This declaration belongs to the global module, so it's definitely
       lookup visible. */
    result = TRUE;
  } else {
    a_module_entity_ptr mep = sym_ptr->module_entity;
    a_module_ptr        mod = skip_module_partitions(mep->module_info);

    if (mod == curr_lookup_module()) {
      /* This is a non-exported module entity, but we're doing lookup from
         within the same module, so lookup succeeds. */
      check_assertion(mep->non_exported);
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* is_symbol_currently_lookup_visible */

#if DEBUG
/* GCOVR_EXCL_START */

char *db_canonical_ptr_for_symbol(a_symbol_ptr	sym_ptr)
/*
If "sym_ptr" has an associated IL entry, return the canonical entry associated
with the IL entry; otherwise return NULL.
*/
{
  char              *entity_ptr;
  an_il_entry_kind  entity_kind;

  entity_ptr = il_entry_for_symbol_null_okay(sym_ptr, &entity_kind);
  if (entity_ptr != NULL) {
    entity_ptr = canonical_il_entry_of(entity_ptr);
  }  /* if */
  return entity_ptr;
}  /* db_canonical_ptr_for_symbol */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */


an_access_specifier access_for_symbol(a_symbol_ptr sym_ptr)
/*
Return the access specified in the symbol pointed to by sym_ptr.  This is
a low-level routine that just gets the access from the symbol, with
a special case for projection symbols and overloaded function symbols.
It cannot be used for checking access (see have_access_to_symbol).
*/
{
  an_access_specifier access;

  if (fundamental_symbol_of(sym_ptr)->kind == sk_overloaded_function) {
    /* Overloaded function.  Cannot tell what the access is; leave it
       to be checked later.  This may not be necessary because overloaded
       functions shouldn't get into the main portion of the access checking
       code. */
    access = as_public;
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (symbol_is(fundamental_symbol_of(sym_ptr), sk_property_set)) {
    /* In general, we can only tell the access once we know which property in
       the set is selected. */
    access = as_public;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else if (sym_ptr->kind == sk_projection) {
    /* Projection symbol. */
    access =enum_cast<an_access_specifier>(sym_ptr->variant.projection.access);
  } else if (sym_ptr->kind == sk_namespace_projection) {
    /* Namespace projection symbol. */
    access = enum_cast<an_access_specifier>(sym_ptr->
                                          variant.namespace_projection.access);
  } else if (sym_ptr->kind == sk_class_template) {
    /* Access for class templates is stored in the template symbol
       supplement. */
    access = enum_cast<an_access_specifier>(sym_ptr->
                         variant.template_info->variant.class_template.access);
  } else if (sym_ptr->kind == sk_function_template) {
    /* Access for function templates is stored in routine entry pointed to
       by the template symbol supplement. */
    access = enum_cast<an_access_specifier>(sym_ptr->variant.template_info->
                              variant.function.routine->source_corresp.access);
  } else if (sym_ptr->kind == sk_type &&
             sym_ptr->variant.type.is_injected_class_name) {
    /* Symbols for injected class names are always public. */
    access = as_public;
  } else if (sym_ptr->kind == sk_undefined) {
    /* Error case; assume public. */
    access = as_public;
  } else {
    /* Normal symbol (not projection or overloaded function). */
    a_source_correspondence *scp = source_corresp_entry_for_symbol(sym_ptr);
    check_assertion_str2(sym_ptr->kind != sk_namespace_projection,
                         "access_for_symbol:", "invalid symbol kind");
    check_assertion(scp != NULL);
    access = enum_cast<an_access_specifier>(scp->access);
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (cli_or_cx_enabled && sym_ptr->is_class_member &&
        sym_ptr->parent.class_type != NULL &&
        assembly_index_from_assembly_scope_index(
         class_type_supp(sym_ptr->parent.class_type)->assembly_scope_index) !=
                                                         curr_assembly_index) {
      /* The symbol comes from an assembly different from the active assembly;
         set access to the (more limited) assembly access value. */
      access = enum_cast<an_access_specifier>(scp->assembly_access);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  return access;
}  /* access_for_symbol */


an_access_specifier compute_access(an_access_specifier sym_access,
                                   an_access_specifier deriv_access)
/*
Compute the access for a symbol projected from a base class, where sym_access
is its accessibility in the base class and deriv_access describes the class
derivation from the base class.

If the derivation access for the class is "public", the access of public
and protected members stays as it is; if it is "protected", public
members become protected and protected members are unaffected; if it is
"private", public and protected symbols become private.  Members private
to the base class become inaccessible to the derived class in every case.
(ARM 11.2).  The following table summarizes the transformations:

                 derivation:
                   inaccessible  private       protected     public
  symbol:        ------------------------------------------------------
    public       | inaccessible  private       protected     public
                 |
    protected    | inaccessible  private       protected     protected
                 |
    private      | inaccessible  inaccessible  inaccessible  inaccessible
                 |
    inaccessible | inaccessible  inaccessible  inaccessible  inaccessible

An "inaccessible" derivation is not a single derivation step; it is
a combination of several steps whose net effect is complete loss of
accessibility.
*/
{
  if (deriv_access == (an_access_specifier)as_inaccessible ||
      !is_more_accessible(sym_access, as_private)) {
    sym_access = (an_access_specifier)as_inaccessible;
  } else if (deriv_access == (an_access_specifier)as_private) {
    sym_access = (an_access_specifier)as_private;
  } else if (sym_access != deriv_access) {
    sym_access = (an_access_specifier)as_protected;
  }  /* if */
  /* Return the (possibly altered) symbol access specifier. */
  return sym_access;
}  /* compute_access */


an_access_specifier access_to_end_of_path(
                                        an_access_specifier         sym_access,
                                        a_derivation_step_ptr       path,
                                        a_base_class_derivation_ptr bcdp)
/*
Compute the accessibility (public, protected, private, inaccessible) to an
entity with access sym_access from the end of the derivation path pointed
to by "path".  This path is part of the path for the base class derivation
bcdp.
*/
{
  if (path != NULL) {
    a_derivation_step_ptr  dsp = bcdp->path_tail;

    /* This is the last step in the derivation for a base class.
       Add the effect of this step into the accumulated access.
       Use the derivation access specified in the header for the base
       class derivation (important for virtual base classes). */
    sym_access = compute_access(sym_access, bcdp->access);
    dsp = dsp->prev;
    for (; dsp != path->prev; dsp = dsp->prev) {
      a_base_class_ptr  bcp = dsp->base_class;

      /* Virtual base classes get special handling. */
      if (is_virtual_but_not_simple_direct_base_class(bcp)) {
        /* This is a virtual step which is more than a direct base class with a
           single derivation.  Compute the access over the preferred derivation
           (which has the best access). */
        a_base_class_derivation_ptr  pref_bcdp;
        pref_bcdp = preferred_virtual_derivation_of(bcp);
        sym_access = access_to_end_of_path(sym_access, pref_bcdp->path,
                                           pref_bcdp);
      } else {
        /* This is a nonvirtual step, or it's a simple virtual step.  Add the
           effect of this step into the accumulated access. */
        sym_access = compute_access(sym_access, bcp->derivation->access);
      }  /* if */
    }  /* for */
  }  /* if */
  return sym_access;
}  /* access_to_end_of_path */


/*
Function pointer types for routines to be passed into
have_particular_member_access_privilege.
*/
typedef a_boolean a_befriending_list_test_function(
                                     a_class_list_entry_ptr befriending_list,
                                     a_type_ptr             class_type);
typedef a_befriending_list_test_function *a_befriending_list_test_function_ptr;
typedef a_boolean a_member_access_from_class_scope_test_function(
                                            a_type_ptr              class_type,
                                            a_scope_stack_entry_ptr ssep);
typedef a_member_access_from_class_scope_test_function
                           *a_member_access_from_class_scope_test_function_ptr;


static a_boolean have_particular_member_access_privilege(
      a_type_ptr                                         class_type,
      a_befriending_list_test_function_ptr               befriending_list_test,
      a_member_access_from_class_scope_test_function_ptr class_scope_test)
/*
Return TRUE if we currently have member access privilege to the class
indicated by class_type in the particular way tested for by the
functions befriending_list_test and class_scope_test.
*/
{
  a_boolean               have_member_privilege = FALSE;
  a_scope_stack_entry_ptr ssep;
  a_routine_ptr           scope_routine = NULL;
  a_scope_depth           scope_depth;
  a_type_ptr              skip_to_class = NULL;

  if (scope_stack[depth_scope_stack].in_prototype_instantiation) {
    /* Suppress access checking during prototype instantiations.  Access
       checking cannot be done for a template, only for instances. */
    have_member_privilege = TRUE;
    goto done;
  }  /* if */
  /* Consider each scope on the scope stack that affects access control.
     They are linked together on a list. */
  for (scope_depth = depth_of_innermost_scope_that_affects_access_control;
       scope_depth != NO_SCOPE_DEPTH;
       scope_depth = ssep->next_scope_that_affects_access_control) {
    a_scope_kind		kind;
    a_class_list_entry_ptr	befriending_classes = NULL;
    ssep = &scope_stack[scope_depth];
    kind = ssep->kind;
    if (kind == (a_scope_kind)sck_function ||
        kind == (a_scope_kind)sck_function_access) {
      /* A function or function access scope.  See if class_type is on
         its befriending list. */
      if (kind == (a_scope_kind)sck_function_access) {
        scope_routine = ssep->assoc_routine;
        if (scope_routine != NULL &&
            scope_routine->special_kind == sfk_deduction_guide &&
            ssep->assoc_type != NULL) {
          /* If the function access scope is for an implicit deduction guide,
             check if the associated class type passes the test. */
          if (class_scope_test(class_type, ssep)) {
            have_member_privilege = TRUE;
            break;
          }  /* if */
        }  /* if */
        /* If the function access scope is for a template, get the
           befriending information associated with the template. */
        if (ssep->template_sym != NULL) {
          a_template_symbol_supplement_ptr	tssp;
          tssp = template_supplement_for_symbol(ssep->template_sym);
          befriending_classes = tssp->befriending_classes;
        }  /* if */
      } else {
        scope_routine = ssep->il_scope->variant.routine.ptr;
      }  /* if */
      /* If the befriending information was not set above, get it from the
         routine entry. */
      if (befriending_classes == NULL && scope_routine != NULL) {
        befriending_classes = rout_befriending_classes(scope_routine);
      }  /* if */
      if (befriending_list_test(befriending_classes, class_type)) {
        /* We are inside a function that is a friend of class_type. */
        have_member_privilege = TRUE;
        break;
      }  /* if */
      if (scope_routine != NULL && scope_routine->is_inheriting_ctor) {
        /* See if the inherited constructor has a befriending class that passes
           the test. */
        a_routine_ptr inh_ctor = get_inh_ctor_originator(scope_routine);
        a_class_list_entry_ptr inh_ctor_friends = NULL;
        if (inh_ctor->is_template_function) {
          a_template_symbol_supplement_ptr tssp;
          a_symbol_ptr                     template_sym;
          template_sym = symbol_for(inh_ctor)->variant.routine.instance_ptr->
                                                                  template_sym;
          tssp = template_supplement_for_symbol(template_sym);
          inh_ctor_friends = tssp->befriending_classes;
        }  /* if */
        if (inh_ctor_friends == NULL) {
          inh_ctor_friends = rout_befriending_classes(inh_ctor);
        }  /* if */
        if (befriending_list_test(inh_ctor_friends, class_type)) {
          have_member_privilege = TRUE;
          break;
        }  /* if */
      }  /* if */
      /* scope_routine will be NULL for a function access scope that was
         pushed as part of a class template rescan context.  Skip the
         processing that requires a scope_routine. */
      if (scope_routine != NULL) {
        if (!scope_routine->source_corresp.is_class_member) {
          /* For a non-member function, in particular a friend function
             defined inside a class, we're done.  Being a friend of a class
             doesn't make one a friend of any enclosing classes. */
          break;
         }  /* if */
        /* Ignore class scopes until we get to the class of which this
           function is a member. */
        skip_to_class = parent_class_of(scope_routine);
      }  /* if */
    } else if (kind == (a_scope_kind)sck_template_instantiation ||
               kind == (a_scope_kind)sck_instantiation_context) {
      /* Nothing required for template instantiation and instantiation
         context scopes. */
    } else {
      a_type_ptr scope_class_type;
      check_assertion_str(kind == (a_scope_kind)sck_class_struct_union ||
                          kind == (a_scope_kind)sck_class_reactivation,
                   "have_particular_member_access_privilege: bad stack entry");
      /* A class or class reactivation. */
      scope_class_type = ssep->assoc_type;
      if (skip_to_class != NULL &&
          !(scope_routine != NULL && scope_routine->is_inheriting_ctor) &&
          !same_entities(scope_class_type, skip_to_class)) {
        /* We're skipping to the class skip_to_class, so ignore this entry.
           Note that in the normal standard language this won't happen,
           because the set of surrounding scopes will match the class
           parents.  However, this code may be useful if someone puts
           in an extension like allowing definition of a member function
           in a friend declaration inside another class, where the lookup
           nesting does not match the access nesting.  Don't skip to a specific
           class with inheriting constructors as we could be in either the
           originator's context or the inheriting constructor's context. */
      } else {
        /* Check for access granted by being a member of the class. */
        if (class_scope_test(class_type, ssep)) {
          /* We are inside a class that gives us member access. */
          have_member_privilege = TRUE;
          break;
        }  /* if */
        if (scope_routine != NULL && scope_routine->is_inheriting_ctor) {
          a_routine_ptr inh_ctor = get_inh_ctor_originator(scope_routine);
          a_type_ptr    saved_scope_class = ssep->assoc_type;
          /* See if the inherited constructor passes the class scope test.
             Note that this test assumes that the scope stack is encoding the
             appropriate type - we'll need to save it off so that we can
             restore it later. */
          ssep->assoc_type = parent_class_of(inh_ctor);
          if (class_scope_test(class_type, ssep)) {
            have_member_privilege = TRUE;
          }  /* if */
          ssep->assoc_type = saved_scope_class;
          if (have_member_privilege) {
            break;
          }  /* if */
        }  /* if */
        if (scope_class_type->source_corresp.is_class_member) {
          /* Ignore class scopes until we get to the class of which this
             class is a member. */
          skip_to_class = parent_class_of(scope_class_type);
        } else {
          /* For a non-nested class, keep going to check any enclosing
             function. */
          skip_to_class = NULL;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
done:
  return have_member_privilege;
}  /* have_particular_member_access_privilege */
  

static a_boolean on_befriending_list(a_class_list_entry_ptr befriending_list,
                                     a_type_ptr             class_type)
/*
Return TRUE if the class type indicated by class_type in on the list
of befriending classes given by befriending_list.
*/
{
  a_boolean on_list = FALSE;

  for (; befriending_list != NULL; befriending_list = befriending_list->next) {
    if (befriending_list->class_type == class_type) {
      on_list = TRUE;
      break;
    }  /* if */
  }  /* for */
  return on_list;
}  /* on_befriending_list */


static a_boolean have_member_access_from_class_scope(
                                            a_type_ptr              class_type,
                                            a_scope_stack_entry_ptr ssep)
/*
We have member access privilege to the class indicated by the scope stack
entry pointed to by ssep.  Return TRUE if that fact means that we have
member access privilege to class_type.
*/
{
  a_boolean  have_member_privilege = FALSE;
  a_type_ptr scope_class = ssep->assoc_type;

  if (same_entities(scope_class, class_type)) {
    /* We are inside class_type. */
    have_member_privilege = TRUE;
  } else if (on_befriending_list(scope_class->variant.
                                            class_struct_union.extra_info->
                                                           befriending_classes,
                                 class_type)) {
    /* We are inside a class that is a friend of class_type. */
    have_member_privilege = TRUE;
  }  /* if */
  return have_member_privilege;
}  /* have_member_access_from_class_scope */


a_boolean have_member_access_privilege(a_type_ptr class_type)
/*
Return TRUE if we currently have member access privilege to the class
indicated by class_type.
*/
{
  a_boolean have_member_privilege = have_particular_member_access_privilege(
                                          class_type,
                                          on_befriending_list,
                                          have_member_access_from_class_scope);
  return have_member_privilege;
}  /* have_member_access_privilege */


a_boolean have_protected_access_from_derived_class(a_type_ptr class_type,
                                                   a_type_ptr derived_class)
/*
Return TRUE if a protected member of class_type can be accessed from
derived_class.  We know that we have member access to derived_class but we do
not know if class_type is a base class of derived_class or if the derivation
between the two will allow access to a protected member.  This routine is used
for determining access to protected members and in the presence of protected
derivations.
*/
{
  a_boolean                   accessible = FALSE;
  a_base_class_ptr            bcp;
  a_base_class_derivation_ptr preferred_derivation;

  /* See if class_type is a base class of derived_class. */
  bcp = find_base_class_of(derived_class, class_type);
  if (bcp != NULL) {
    /* Yes.  See if the derivation steps are such that a protected member
       of the base class can be accessed in the derived class. */
    preferred_derivation = preferred_derivation_of(bcp);
    if (access_to_end_of_path((an_access_specifier)as_protected,
                              preferred_derivation->path,
                              preferred_derivation) !=
                                        (an_access_specifier)as_inaccessible) {
      accessible = TRUE;
    }  /* if */
  }  /* if */
  return accessible;
}  /* have_protected_access_from_derived_class */
  

static a_boolean have_protected_access_from_befriending_list(
                                       a_class_list_entry_ptr befriending_list,
                                       a_type_ptr             class_type)
/*
Return TRUE if protected members of class_type are accessible from
any class on the list of befriending classes given by befriending_list.
We know that we have member access to the classes on befriending_list but we
do not know if class_type is a base class of any of those classes or if the
derivations between the class_type and one of those classes will allow access
to a protected member.  This routine is used in determining access to
protected members and in the presence of protected derivations.
*/
{
  a_boolean accessible = FALSE;

  for (; befriending_list != NULL; befriending_list = befriending_list->next) {
    if (have_protected_access_from_derived_class(class_type,
                                               befriending_list->class_type)) {
      accessible = TRUE;
      break;
    }  /* if */
  }  /* for */
  return accessible;
}  /* have_protected_access_from_befriending_list */


static a_boolean have_protected_access_from_class_scope(
                                            a_type_ptr              class_type,
                                            a_scope_stack_entry_ptr ssep)
/*
We have member access privilege to the class indicated by the scope stack
entry pointed to by ssep.  Return TRUE if that fact means that we have
access to protected members of class_type by virtue of having member
access privilege to a derived class of class_type.  This is needed in
determining accessibility to protected members and in the presence of
protected derivations.
*/
{
  a_boolean  have_protected_access = FALSE;
  a_type_ptr scope_class = ssep->assoc_type;

  if (have_protected_access_from_derived_class(class_type, scope_class)) {
    /* We are in a class that is an appropriate derived class of class_type,
       so we have access to protected members of class_type. */
    have_protected_access = TRUE;
  } else if (have_protected_access_from_befriending_list(scope_class->
                    variant.class_struct_union.extra_info->befriending_classes,
                                                                 class_type)) {
    /* We are in a class that is a friend of an appropriate derived class
       of class_type, so we have access to protected members of class_type. */
    have_protected_access = TRUE;
  }  /* if */
  return have_protected_access;
}  /* have_protected_access_from_class_scope */


a_boolean have_protected_member_access_privilege(a_type_ptr class_type)
/*
Return TRUE if we have access to protected members of class_type by
virtue of having member access privilege to a derived class of class_type.
This is needed in determining accessibility to protected members and
in the presence of protected derivations.  See p. 214 of "The C++
Programming Language", 2nd Edition.
*/
{
  a_boolean have_member_privilege =
                 have_particular_member_access_privilege(
                                   class_type,
                                   have_protected_access_from_befriending_list,
                                   have_protected_access_from_class_scope);
  return have_member_privilege;
}  /* have_protected_member_access_privilege */


/*
Entry used to keep track of stacking in processing virtual steps on
a derivation path in have_access_across_path.  These are allocated as
auto variables and chained together.
*/
typedef struct a_virtual_step_stack_entry *a_virtual_step_stack_entry_ptr;
typedef struct a_virtual_step_stack_entry {
  a_virtual_step_stack_entry_ptr
		next;	/* Next entry on the list. */
  a_derivation_step_ptr
		virtual_step;
			/* Derivation step for a virtual base class, being
			   expanded. */
  a_base_class_derivation_ptr
		derivation;
			/* The base class derivation of whose path virtual_step
			   is a step. */
} a_virtual_step_stack_entry;


static an_access_specifier access_to_end_of_virtual_step_stack(
                             an_access_specifier            access,
                             a_virtual_step_stack_entry_ptr virtual_step_stack)
/*
Determine and return the amount of access available to an entity with
access "access" across the concatenation of the derivation paths indicated
by the stack of entries in virtual_step_stack.
*/
{
  if (virtual_step_stack != NULL) {
    /* Do a recursive call to do all the path segments for the stack entries
       following the first one. */
    access = access_to_end_of_virtual_step_stack(access,
                                                 virtual_step_stack->next);
    /* Add in the access for the segment represented by the first stack
       entry. */
    access = access_to_end_of_path(access,
                                   virtual_step_stack->virtual_step->next,
                                   virtual_step_stack->derivation);
  }  /* if */
  return access;
}  /* access_to_end_of_virtual_step_stack */


static an_access_specifier access_across_path(
                             a_symbol_ptr                   sym,
                             a_type_ptr                     viewpoint_class,
                             a_derivation_step_ptr          path,
                             a_base_class_derivation_ptr    bcdp,
                             a_symbol_ptr                   proj_sym,
                             a_virtual_step_stack_entry_ptr virtual_step_stack)
/*
Return the statically-determined best access to the symbol sym when
viewed from the class viewpoint_class.  path is the derivation path
from viewpoint_class to sym; it is NULL if sym is in viewpoint_class.
If non-NULL, it is part of the path of the base class derivation bcdp.
proj_sym is the projection symbol from which we started this access
check, or an updated one picked up during the recursive descent
through the derivation; it is ignored if path == NULL, but otherwise
it must be a projection symbol (although its fundamental symbol might
not be sym, i.e., in the overloaded function case; in that case sym
might be a projection symbol as well).  See have_access_across_path
for a description of virtual_step_stack.
*/
{
  an_access_specifier access = (an_access_specifier)as_public;
  a_symbol_ptr        fund_proj_sym, step_proj_sym;
  a_boolean           need_to_compute_access;

  if (path == NULL) {
    /* No derivation path, so the access is the access for the symbol. */
    access = access_for_symbol(sym);
  } else {
    /* A derivation path, so determine the effective access across the
       path. */
#if CHECKING
    if (proj_sym == NULL) {
      internal_error("access_across_path: proj_sym is NULL");
    }  /* if */
    if (proj_sym->kind != (a_symbol_kind)sk_projection) {
      internal_error("access_across_path: proj_sym not projection");
    }  /* if */
#endif /* CHECKING */
    /* Note that in the overloaded function case proj_sym is a projection
       of an sk_overloaded_function symbol, not of sym. */
    fund_proj_sym= proj_sym->variant.projection.extra_info->fundamental_symbol;
    need_to_compute_access = TRUE;
    if (same_entities(sym_parent_class(proj_sym), viewpoint_class)) {
      /* The step we are looking at is the first one, so the effective
         access is available from the projection symbol. */
      access = enum_cast<an_access_specifier>(
                                          proj_sym->variant.projection.access);
      need_to_compute_access = FALSE;
    } else if (proj_sym->variant.projection.any_intervening_using_decl) {
      /* There is a using declaration somewhere on some derivation path, so
         we must look for a projection symbol that applies at this step of
         the path in case it is a using declaration.  It's okay not to find
         such a projection symbol since the using declaration might not be
         at the current level. */
      step_proj_sym = find_symbol_list_in_table(
                           &symbol_supplement_for_class(viewpoint_class)
                                                              ->pointers_block,
                           proj_sym->header);
      for (; step_proj_sym != NULL; step_proj_sym = step_proj_sym->next) {
        if (same_entities(sym_parent_class(step_proj_sym),
                          viewpoint_class) &&
            step_proj_sym->kind == (a_symbol_kind)sk_projection &&
            step_proj_sym->variant.projection.extra_info->
                                         fundamental_symbol == fund_proj_sym) {
          /* Replace the projection symbol we have by the new one.  Note
             that it will get passed down in the recursive call below,
             which is good, because once we get past the using declarations
             we can use the faster technique. */
          proj_sym = step_proj_sym;
          access = enum_cast<an_access_specifier>(
                                          proj_sym->variant.projection.access);
          need_to_compute_access = FALSE;
          break;
        }  /* if */
      }  /* for */
    }  /* if */
    if (!need_to_compute_access) {
      /* If the symbol is for an overloaded function, we can use the access
         computed only if the projection symbol is a using declaration.
         Otherwise, the individual functions in the overload sets can have
         distinct access settings, and the projection symbol cannot
         indicate all of them. */
      if (fund_proj_sym->kind == (a_symbol_kind)sk_overloaded_function) {
        if (!proj_sym->variant.projection.is_using_decl) {
          need_to_compute_access = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
    if (need_to_compute_access) {
      /* The access must be determined by looking at the derivation steps.
         This is probably a little faster than looking for the projection
         symbol (and the projection symbol might not exist and would have
         to be created if that approach were used). */
      access = access_for_symbol(sym);
      /* Adjust the access for any path sequences indicated in the virtual
         step stack. */
      if (virtual_step_stack != NULL) {
        access = access_to_end_of_virtual_step_stack(access,
                                                     virtual_step_stack);
      }  /* if */
      /* Adjust the access for the path remaining after the steps indicated in
         the stack. */
      access = access_to_end_of_path(access, path, bcdp);
    }  /* if */
  }  /* if */
  return access;
}  /* access_across_path */


/* Declaration needed because of forward reference: */
static a_boolean have_access_across_derivations(a_symbol_ptr symbol,
                                                a_symbol_ptr view_sym);


static a_boolean have_access_across_path(
                             a_symbol_ptr                   sym,
                             a_type_ptr                     viewpoint_class,
                             a_derivation_step_ptr          path,
                             a_base_class_derivation_ptr    bcdp,
                             a_symbol_ptr                   proj_sym,
                             a_virtual_step_stack_entry_ptr virtual_step_stack)
/*
Return TRUE if the symbol sym is accessible at the current location
in the source program when viewed from the class viewpoint_class.
path is the derivation path from viewpoint_class to sym;
it is NULL if sym is in viewpoint_class.  If non-NULL, it is part of
the path of the base class derivation bcdp.  proj_sym is the projection
symbol from which we started this access check, or an updated one picked
up during the recursive descent through the derivation; if path == NULL
it might not be a projection symbol, but otherwise it must be one
(although its fundamental symbol might not be sym, i.e., in the overloaded
function case; in that case sym might be a projection symbol as well).
virtual_step_stack is a pointer to a linked list that describes a stack
of virtual steps being expanded by invocations of this routine above
this one.
*/
{
  a_boolean             have_access = FALSE, base_class_accessible;
  an_access_specifier   access, base_class_deriv;
  a_boolean             have_member_access = FALSE, determined_member_access;
  a_boolean             have_protected_member_access = FALSE;
  a_boolean             determined_protected_member_access;
  a_base_class_ptr      bcp;
  a_virtual_step_stack_entry
                        vsse;
  a_boolean             virtual_step;
  a_derivation_step_ptr path_next;

  /* Determine the effective access to the fundamental symbol from the
     viewpoint class. */
  access = access_across_path(sym, viewpoint_class, path, bcdp, proj_sym,
                              virtual_step_stack);
  /* We now have the effective access to the member in the viewpoint class,
     statically determined.  See if we have access. */
  /* The expensive determinations are only done if needed. */
  determined_member_access = determined_protected_member_access = FALSE;
  if (access == (an_access_specifier)as_public) {
    /* The member is public, so it is accessible. */
    have_access = TRUE;
  } else if (access != (an_access_specifier)as_inaccessible &&
             (determined_member_access = TRUE,
              have_member_access =
                              have_member_access_privilege(viewpoint_class),
              have_member_access)) {
    /* The member is not inaccessible (i.e., there is some access to it),
       and we have member access privilege to the class, so we have access
       to the member. */
    have_access = TRUE;
  } else if (access == (an_access_specifier)as_protected &&
             (determined_protected_member_access = TRUE,
              have_protected_member_access =
                    have_protected_member_access_privilege(viewpoint_class),
              have_protected_member_access)) {
    /* The member is protected, and we have member access to a derived
       class of the viewpoint class, so we have access to the member.
       Note that this is more generous than the access allowed by ARM 11.5;
       additional checking in the expression routines is needed to enforce
       that restriction. */
    have_access = TRUE;
  } else if (proj_sym->kind == (a_symbol_kind)sk_projection &&
             ((proj_sym->variant.projection.is_using_decl &&
	       !(strict_ansi_mode || (gpp_mode && gnu_version < 30400))) ||
	      (symbol_is(sym, sk_projection) && sym == proj_sym))) {
    /* We do not have access to the member in this class, and the symbol
       here is a using-declaration.  Core Issue 360 suggests that we should
       not look for access in a base class.  Since that issue is still
       open (January 2012), the standard still requires the base class
       access check, so we do it in strict mode.  If the projection symbol
       is the same as the fundamental symbol, we got here from a recursive
       call using the adjusted symbol from the any_intevening_using_decl
       test below. */
    /* have_access = FALSE;  -- already set. */
  } else {
    /* We do not have access to the member in this class, but perhaps we
       have access to it in a base class.  This would be because of some
       member access to a base class that does not figure into the
       general-case access determined above.  We walk down the path
       to the fundamental base class, continuing as long as the base
       class at each step is accessible from the original class, and we
       check for special access at each step. */
    if (path == NULL) {
      /* If sym is the specific symbol chosen from an overload set
         designated by proj_sym, it might be a projection symbol itself.
         Look down from it to its fundamental symbol, looking for access. */
      if (sym->kind != (a_symbol_kind)sk_projection) {
        /* Normal case. */
        /* We're already in the class of the fundamental symbol, so we do
           not have access. */
        /* have_access = FALSE;  -- already set. */
      } else {
        /* This code really is needed, for obscure cases involving member
           access due to protected derivations. */
        proj_sym = sym;
        sym = fundamental_symbol_of(sym);
        have_access = have_access_across_derivations(sym, proj_sym);
      }  /* if */
    } else {
      /* Find the base class that's first on the path.  It most cases, that's
         trivial, but for virtual base classes we have to go to the virtual
         base class itself and run down its derivation (or derivations,
         as there may be several).  The final step on the derivation for
         a virtual base class is not treated specially -- it's just a simple
         step to that base class. */
      bcp = path->base_class;
      virtual_step = FALSE;
      if (bcp->is_virtual && path != bcdp->path_tail) {
        /* Virtual step.  We have to examine the various derivations for
           the virtual base class.  Add an entry to the stack of virtual step
           entries being processed.  This stack is used when the other end of
           the virtual base class derivation is reached, to know where to
           continue on the derivation path following this virtual step. */
        vsse.next = virtual_step_stack;
        vsse.virtual_step = path;
        vsse.derivation = bcdp;
        virtual_step_stack = &vsse;
        virtual_step = TRUE;
        /* The loop will go through all the derivations of the virtual
           base class.  Start with the first.  It doesn't seem necessary to
           start with the preferred derivation, since we've already failed
           to obtain access in the usual way over the preferred derivation.
           Any access we get now is going to be unusual in some way. */
        bcdp = bcp->derivation;
        path = bcdp->path;
        bcp = path->base_class;
      }  /* if */
      /* Loop through the derivation paths to be considered.  There is more
         than one path only in the virtual step case. */
      for (;;) {
        /* Determine whether or not the base class is accessible.  A base class
           is accessible if its public members are accessible from the derived
           class.  This is like the macro is_accessible_imm_base_class, but
           optimized to use whatever we've already determined about member
           access to the viewpoint class. */
        /* Get the derivation access for this derivation step.  If this step
           is the last on a derivation, get the access from the base class
           derivation entry (important for virtual base classes). */
        if (path == bcdp->path_tail) {
          base_class_deriv = bcdp->access;
          path_next = NULL;
        } else {
          base_class_deriv = path->base_class->derivation->access;
          path_next = path->next;
        }  /* if */
        base_class_accessible = FALSE;
        if (base_class_deriv == (an_access_specifier)as_public) {
          /* The base class is public, so it is accessible. */
          base_class_accessible = TRUE;
        } else {
          /* See if we have member access privilege to the viewpoint class. */
          if (!determined_member_access) {
            determined_member_access = TRUE;
            have_member_access = have_member_access_privilege(viewpoint_class);
          }  /* if */
          if (have_member_access) {
            /* We have member access to the viewpoint class, so the base class
               is accessible regardless of the type of derivation. */
            base_class_accessible = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
          } else if (microsoft_mode &&
                     have_member_access_privilege(bcp->type)) {
            /* Microsoft considers a base class accessible if we have member
               access to it.  This is presumably because of the WP wording
               that says "A base class is said to be accessible if an invented
               public member of the base class is accessible." */
            base_class_accessible = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          } else {
            /* See if special protected member access privilege applies.  This
               is only meaningful when the base class derivation is
               protected. */
            if (base_class_deriv == (an_access_specifier)as_protected) {
              if (!determined_protected_member_access) {
                determined_protected_member_access = TRUE;
                have_protected_member_access =
                       have_protected_member_access_privilege(viewpoint_class);
              }  /* if */
              if (have_protected_member_access) {
                base_class_accessible = TRUE;
              }  /* if */
            }  /* if */
          }  /* if */
        }  /* if */
        if (base_class_accessible) {
          /* The base class is accessible, so we want to do a recursive call
             to check accessibility at the next step.  Determine the path
             for the next step.  Usually, it's just path->next, already in
             path_next. */
          a_virtual_step_stack_entry_ptr local_virtual_step_stack =
                                                            virtual_step_stack;
          a_base_class_derivation_ptr    local_bcdp = bcdp;
          while (path_next == NULL && virtual_step_stack != NULL) {
            /* At the end of the derivation path for a virtual step, continue
               with the step following the virtual step (up one level in
               the stack). */
            path_next = virtual_step_stack->virtual_step->next;
            local_bcdp = virtual_step_stack->derivation;
            local_virtual_step_stack = virtual_step_stack->next;
          }  /* while */
          /* Do a recursive call to see if the member is accessible in the
             base class. */
          if (proj_sym->kind == sk_projection &&
              proj_sym->variant.projection.any_intervening_using_decl) {
            /* If the class scope we are about to check contains an
               intervening using-declaration for sym, change sym to refer
               to that instead of the fundamental symbol so we check the
               effective access of the using-declaration. */
            a_class_symbol_supplement_ptr base_cssp =
                                      class_symbol_supp(symbol_for(bcp->type));
            if (base_cssp->pointers_block.last_using_declaration != NULL) {
              for (a_symbol_ptr mbr_sym = base_cssp->symbols; mbr_sym != NULL;
                   mbr_sym = mbr_sym->next_in_scope) {
                if (symbol_is(mbr_sym, sk_projection) &&
                    mbr_sym->variant.projection.is_using_decl &&
                    fundamental_symbol_of_projection(mbr_sym) == sym) {
                  sym = mbr_sym;
                  break;
                }  /* if */
              }  /* for */
            }  /* if */
          }  /* if */
          if (have_access_across_path(sym, bcp->type, path_next,
                                      local_bcdp, proj_sym,
                                      local_virtual_step_stack)) {
            /* Yes, it is. */
            have_access = TRUE;
          }  /* if */
        }  /* if */
        /* Loop only for the virtual step case. */
        if (!virtual_step) break;
        bcdp = bcdp->next;
        /* Stop after the last derivation for the virtual step case. */
        /* Note that we do not have to take the stack entry off the stack
           or restore bcdp et al., since we have not affected the caller's
           variables.  If there were more processing to be done in this
           routine, that might be a good thing to do. */
        if (bcdp == NULL) break;
        /* Loop for another derivation. */
        path = bcdp->path;
        bcp = path->base_class;
      }  /* for */
    }  /* if */
  }  /* if */
  return have_access;
}  /* have_access_across_path */


static a_boolean have_access_across_derivations_helper(
                                                 a_symbol_ptr        symbol,
                                                 a_symbol_ptr        view_sym,
                                                 an_access_specifier *p_access)
/*
Return TRUE if the symbol "symbol" is accessible at the current location
in the source program when viewed from the class of which view_sym is a
member.  If view_sym is an overloaded function symbol or a projection
thereof, symbol is the specific symbol chosen from that overload set
(and possibly a projection symbol); otherwise symbol is not a projection
symbol, and view_sym is either the same as symbol or a projection thereof.
If p_access is non-NULL, *p_access is set to the statically-determined
best access for symbol, and the dynamic access determination is not
done (and therefore the return value is meaningless).
*/
{
  a_boolean                   have_access = FALSE;
  an_access_specifier         access;
  a_base_class_ptr            bcp;
  a_base_class_derivation_ptr derivations, preferred_derivation, bcdp;
  a_derivation_step_ptr       preferred_path;
  a_type_ptr                  viewpoint_class;
  a_symbol_ptr                fund_view_sym;

#if MICROSOFT_EXTENSIONS_ALLOWED
  if (view_sym->is_super_reference) {
    /* When a symbol is accessed via a Microsoft super, ignore the projection
       symbol and treat it as a reference to the underlying symbol. */
    symbol = fundamental_symbol_of(symbol);
    view_sym = symbol;
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  fund_view_sym = fundamental_symbol_of(view_sym);
  if (fund_view_sym->kind == (a_symbol_kind)sk_overloaded_function &&
      symbol->kind == (a_symbol_kind)sk_member_function &&
      symbol->variant.routine.ptr->template_arg_list != NULL &&
      sym_parent_class(symbol) != sym_parent_class(fund_view_sym)) {
    a_symbol_ptr sym;
    /* When symbol is an instance of a member function template, and
       the overload set is in a different class (i.e., it comes from a
       using-declaration), find the symbol in the overload set that
       corresponds to symbol, because we need a projection symbol to
       compute the access.  Template cases are special because the
       instance symbol is generated rather than looked up (the template
       was looked up, not the instance). */
    /* Go back to the template symbol. */
    a_symbol_ptr templ_sym= symbol->variant.routine.instance_ptr->template_sym;
    for (sym = fund_view_sym->variant.overloaded_function.symbols;
         ;
         sym = sym->next) {
      check_assertion_str2(sym != NULL,
                           "have_access_across_derivations_helper:",
                           "sym not found in overload set");
      if (fundamental_symbol_of(sym) == templ_sym) {
        symbol = sym;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  if (view_sym->kind == (a_symbol_kind)sk_projection) {
    /* The view symbol is a projection symbol. */
    bcp = view_sym->variant.projection.extra_info->fundamental_base_class;
    derivations = bcp->derivation;
    /* Base classes generated for references to members of nonreal classes
       have a null derivation. */
    if (derivations == NULL) {
      preferred_derivation = NULL;
      preferred_path = NULL;
    } else {
      preferred_derivation = preferred_derivation_of(bcp);
      preferred_path = preferred_derivation->path;
    }  /* if */
  } else {
    /* The view symbol is not a projection symbol, so the view class is the
       same as the class of the viewed symbol. */
    derivations = preferred_derivation = NULL;
    preferred_path = NULL;
  }  /* if */
  viewpoint_class = sym_parent_class(view_sym);
  if (p_access == NULL) {
    /* Called from have_access_across_derivations to determine whether we
       have dynamic access. */
    /* Check the preferred derivation (the one that gives the most access
       statically).  preferred_derivation and preferred_path are NULL if the
       view class is the same class as the class of the viewed symbol. */
    if (have_access_across_path(symbol, viewpoint_class,
                                preferred_path, preferred_derivation,
                                view_sym,
                                (a_virtual_step_stack_entry_ptr)NULL)) {
      /* The preferred derivation gives access. */
      have_access = TRUE;
    } else {
      /* The preferred derivation does not give access.  Check all the other
         derivations, if any.  Only virtual base classes can have more than
         one derivation. */
      for (bcdp = derivations; bcdp != NULL; bcdp = bcdp->next) {
        if (!bcdp->preferred) {
          if (have_access_across_path(symbol, viewpoint_class,
                                      bcdp->path, bcdp, view_sym,
                                      (a_virtual_step_stack_entry_ptr)NULL)) {
            /* This derivation gives access. */
            have_access = TRUE;
            break;
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
  } else {
    /* Called from access_across_derivations to determine the best static
       access to the symbol. */
    have_access = FALSE;  /* Arbitrary. */
    access = access_across_path(symbol, viewpoint_class,
                                preferred_path, preferred_derivation,
                                view_sym,
                                (a_virtual_step_stack_entry_ptr)NULL);
    /* Check all the other derivations, if any.  Only virtual base classes
       can have more than one derivation.  Remember the best access. */
    for (bcdp = derivations;
         bcdp != NULL && access != (an_access_specifier)as_public;
         bcdp = bcdp->next) {
      if (!bcdp->preferred) {
        an_access_specifier other_access =
                     access_across_path(symbol, viewpoint_class,
                                        bcdp->path, bcdp,
                                        view_sym,
                                        (a_virtual_step_stack_entry_ptr)NULL);
        if (is_more_accessible(other_access, access)) {
          access = other_access;
        }  /* if */
      }  /* if */
    }  /* for */
    *p_access = access;
  }  /* if */
  return have_access;
}  /* have_access_across_derivations_helper */


static a_boolean have_access_across_derivations(a_symbol_ptr symbol,
                                                a_symbol_ptr view_sym)
/*
Return TRUE if the symbol "symbol" is accessible at the current location
in the source program when viewed from the class of which view_sym is a
member.  If view_sym is an overloaded function symbol or a projection
thereof, symbol is the specific symbol chosen from that overload set
(and possibly a projection symbol); otherwise symbol is not a projection
symbol, and view_sym is either the same as symbol or a projection thereof.
*/
{
  a_boolean have_access;

  have_access = have_access_across_derivations_helper(symbol, view_sym,
                                                  (an_access_specifier *)NULL);
  return have_access;
}  /* have_access_across_derivations */


static an_access_specifier access_across_derivations(a_symbol_ptr symbol,
                                                     a_symbol_ptr view_sym)
/*
Return the statically-determined best access for the symbol "symbol"
when viewed from the class of which view_sym is a member.  If view_sym is
an overloaded function symbol or a projection thereof, symbol is the
specific symbol chosen from that overload set (and possibly a projection
symbol); otherwise symbol is not a projection symbol, and view_sym is
either the same as symbol or a projection thereof.  This is similar
to access_for_symbol, but deals with the overloaded function case.
*/
{
  an_access_specifier access;

  (void)have_access_across_derivations_helper(symbol, view_sym, &access);
  return access;
}  /* access_across_derivations */


an_access_specifier effective_access_of_member_in_class(
                                                   a_symbol_ptr  member_sym,
                                                   a_type_ptr    naming_class)
/*
Return the access that member_sym has when named as a member of naming_class
(the "naming class" of N5046 [class.access.base]) rather than the access with
which member_sym was declared in its own class.  When naming_class is derived
from member_sym's class, this accounts both for the derivation path's access
and for any using-declaration along the way that changes the member's access
(for example, a public using-declaration in a derived class that re-exports an
inherited protected member as public).  member_sym must be a class member and
naming_class an immediate class type.  When naming_class is the member's own
class, or when the member cannot be reached (or resolves to a different entity)
through naming_class, member_sym's own declared access is returned.
*/
{
  an_access_specifier access;
  a_type_ptr          member_class =
                              skip_typerefs(sym_parent_class(member_sym));

  naming_class = skip_typerefs(naming_class);
  if (same_entities(naming_class, member_class)) {
    /* Named through its own class; the declared access is the effective
       one. */
    access = access_for_symbol(member_sym);
  } else {
    a_symbol_locator  locator;
    a_symbol_ptr      found;
    /* Look up the member's name in naming_class, exactly as a qualified name
       "naming_class::member" would.  class_qualified_id_lookup returns the
       fundamental symbol and records the naming-class-rooted projection in the
       locator; that projection is what access_across_derivations views the
       member through, and it carries both the derivation path's access and any
       using-declaration that changes the member's access along the way. */
    make_locator_for_symbol(member_sym, &locator);
    clear_specific_symbol(locator);
    found = class_qualified_id_lookup(&locator, naming_class, IDL_NO_OPTIONS);
    if (found != NULL && locator.specific_symbol != NULL &&
        fundamental_symbol_of(found) == fundamental_symbol_of(member_sym)) {
      access = access_across_derivations(member_sym, locator.specific_symbol);
    } else {
      /* The name resolves to a different entity through naming_class (for
         instance a member that hides member_sym), or is not found; fall back
         to the member's own declared access. */
      access = access_for_symbol(member_sym);
    }  /* if */
  }  /* if */
  return access;
}  /* effective_access_of_member_in_class */


static a_boolean is_member_of_prototype_instantiation(a_symbol_ptr	sym)
/*
Return TRUE if sym is for a member of a class template prototype instantiation.
*/
{
  a_boolean	result = FALSE;

  if (sym->is_class_member) {
    a_type_ptr	parent_type = sym->parent.class_type;
    if (parent_type->variant.class_struct_union.is_prototype_instantiation) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* is_member_of_prototype_instantiation */


static a_boolean have_access_to_inherited_ctor(a_symbol_ptr symbol)
/*
Return TRUE if generating the body of an inheriting constructor and the
indicated symbol is the constructor that was inherited, or if calling an
inheriting constructor where there is access to the inherited constructor.
*/
{
  a_boolean               have_access = FALSE;
  a_scope_stack_entry_ptr ssep = &scope_stack_top();
  a_routine_ptr           sym_ctor = NULL, scope_rout = NULL;

  if (is_constructor_symbol(symbol)) {
    sym_ctor = func_sym_routine(symbol);
  }  /* if */
  if (scope_is(ssep, sck_function)) {
    scope_rout = ssep->il_scope->variant.routine.ptr;
  } else if (scope_is(ssep, sck_function_access)) {
    scope_rout = ssep->assoc_routine;
  }  /* if */
  if (sym_ctor != NULL && scope_rout != NULL &&
      scope_rout->is_inheriting_ctor) {
    a_routine_ptr ctor_orig = get_inh_ctor_originator(ssep->assoc_routine);
    a_routine_ptr sym_orig = get_inh_ctor_originator(sym_ctor);
    if (ctor_orig == sym_orig) {
      have_access = TRUE;
    }  /* if */
  } else if (sym_ctor != NULL && sym_ctor->is_inheriting_ctor) {
    sym_ctor = get_inh_ctor_originator(sym_ctor);
    symbol = symbol_for(sym_ctor);
    if (have_access_across_derivations(symbol, symbol)) {
      have_access = TRUE;
    }  /* if */
  }  /* if */
  return have_access;
}  /* have_access_to_inherited_ctor */


a_boolean have_access_to_symbol_full(a_symbol_ptr symbol,
                                     a_boolean    ignore_func_templ)
/*
Return TRUE if the indicated symbol is accessible from the current location
in the source program.  If ignore_func_templ is TRUE, function template symbols
are not checked (i.e., they're considered unconditionally accessible); this is
used because a later check for a specific instance of the function template
will be performed (after overload resolution).
*/
{
  a_symbol_ptr	fund_sym = fundamental_symbol_of_projection(symbol);
  a_boolean	have_access = TRUE;

  if (scope_stack_top().in_prototype_instantiation) {
    /* Suppress access checking during prototype instantiations.  Access
       checking cannot be done for a template, only for instances. */
  }  else if (scope_stack_top().is_rescan &&
              symbol->is_class_member &&
              is_member_of_prototype_instantiation(symbol)) {
    /* In a rescan context don't recheck the access of a member of a prototype
       instantiation. */
  } else if (microsoft_mode &&
             depth_innermost_instantiation_scope != NO_SCOPE_DEPTH &&
             scope_stack[depth_innermost_instantiation_scope].
                                              function_partial_instantiation &&
             !scope_stack_top().in_decltype_context) {
    /* The Microsoft compiler ignores certain access errors during the rescan
       of function template declarations when creating the partial
       instantiation of the function. */
  } else if (fund_sym->kind == (a_symbol_kind)sk_overloaded_function) {
    /* For overloaded functions, do not check access now.  The check will
       be done after the specific function is determined. */
  } else if (fund_sym->kind == (a_symbol_kind)sk_function_template &&
             ignore_func_templ) {
    /* Likewise treat templates as sets of overloaded functions. */
  } else if (!strict_ansi_mode && is_injected_template_symbol(fund_sym)) {
    /* Microsoft, g++, clang, and Sun all treat the injected class name
       of a class template as accessible in all cases. */
  } else if (have_access_across_derivations(fund_sym, symbol)) {
    /* have_access = TRUE */
  } else if (have_access_to_inherited_ctor(fund_sym)) {
    /* An inheriting constructor is allowed to access its inherited
       constructor, and inheriting constructors inherit friends for the
       purposes of accessing the inheriting constructor. */
  } else {
    have_access = FALSE;
  }  /* if */
  return have_access;
}  /* have_access_to_symbol_full */

#if MICROSOFT_EXTENSIONS_ALLOWED

a_boolean have_hide_by_sig_access_to_symbol(a_symbol_ptr symbol)
/*
Return TRUE if the indicated symbol is accessible from the current location
in the source program, in the sense required to keep it in a C++/CLI
hide-by-sig overload set.
*/
{
  a_symbol_ptr fund_sym = fundamental_symbol_of(symbol);
  a_boolean    have_access;

  /* We don't suppress access checking in prototype instantiations because
     access checking for managed classes is simpler (no friendship) and
     we need to get the right access-based answer for nondependent calls. */
  check_assertion(fund_sym->kind != (a_symbol_kind)sk_overloaded_function);
  have_access = have_access_across_derivations(fund_sym, symbol);
  return have_access;
}  /* have_hide_by_sig_access_to_symbol */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void issue_access_error(a_symbol_ptr       sym,
                               a_type_ptr         protected_access_class,
                               a_source_position  *err_pos,
                               an_error_severity  severity,
                               an_error_code      error_code,
                               a_boolean          in_template_arg_list,
                               a_boolean          *error_detected)
/*
Issue the appropriate error on the inaccessibility of sym at *err_pos.
If protected_access_class is non-NULL, the checking is the special
protected member checking of 11.5 of the C++ standard, and
protected_access_class indicates the type of the object used
to access the member.  in_template_arg_list is TRUE if the access
occurred in the context of a template argument list.

Normally this routine determines the error code and severity to be
used, but they can also be specified by the caller using severity and
error_code.  If the default values are to be used, severity should be
es_none, and error_code should be ec_no_error.  The severity is only
used if error_code is not ec_no_error.

If error_detected is non-NULL, do not issue any diagnostics.  Just figure
out the error and severity, and return *error_detected TRUE if the selected
diagnostic would actually be an error.
*/
{
  a_boolean issue_diagnostics = (error_detected == NULL);

  sym = originator_symbol_of(sym);
  if (error_code != ec_no_error) {
    /* The error code and severity were provided by the caller. */
    if (issue_diagnostics) {
      pos_sy_diagnostic(severity, error_code, err_pos, sym);
    }  /* if */
  } else if (protected_access_class != NULL) {
    error_code = ec_protected_access_problem;
    severity = es_discretionary_error;
    if (issue_diagnostics) {
      pos_syty_diagnostic(severity, error_code,
                          err_pos, sym, protected_access_class);
    }  /* if */
  } else {
    a_routine_ptr     rp;
    error_code = ec_no_access_to_name;
    severity = es_discretionary_error;
    if (is_function_symbol(sym)) {
      if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
        rp = sym->variant.overloaded_function.symbols->variant.routine.ptr;
      } else {
        rp = sym->variant.routine.ptr;
      }  /* if */
      if (rp->special_kind == (a_special_function_kind)sfk_constructor ||
          rp->special_kind == (a_special_function_kind)sfk_destructor ||
          rp->special_kind == (a_special_function_kind)sfk_conversion ||
          (rp->special_kind == (a_special_function_kind)sfk_operator &&
           rp->variant.opname_kind == (an_opname_kind)onk_assign)) {
        error_code = ec_inaccessible_special_function;
      }  /* if */
    } else if (is_type_symbol(sym)) {
      if (any_cfront_mode()) {
        /* In cfront mode access errors on types are only warnings.  cfront
           doesn't check access to types at all. */
        severity = es_warning;
        error_code = ec_no_access_to_type_cfront_mode;
      }  /* if */
    }  /* if */
    if (gpp_version_is(<40900) && gnu_version >= 30400 &&
        in_template_arg_list) {
      /* Some versions of g++ have a bug where some access errors are ignored
         in template argument list.  The g++ bug only occurs if the argument
         list is followed by "::".  Our emulation issues a diagnostic in all
         cases, but reduces the severity to a warning when it appears in a
         template argument list. */
      severity = es_warning;
    }  /* if */
    if (issue_diagnostics) {
      pos_sy_diagnostic(severity, error_code, err_pos, sym);
    }  /* if */
  }  /* if */
  if (!issue_diagnostics) {
    /* In some modes, is_effective_sfinae_error will ignore diagnostic
       overrides in making the error determination. */
    check_assertion(error_detected != NULL);
    *error_detected = is_effective_sfinae_error(error_code, severity, err_pos);
  }  /* if */
}  /* issue_access_error */


static an_access_error_descr_ptr alloc_access_error_descr(void)
/*
Allocate an access error description entry.  Reuse a freed entry if possible.
*/
{
  an_access_error_descr_ptr aedp;

  if (avail_access_error_descrs != NULL) {
    /* Reuse a freed entry. */
    aedp = avail_access_error_descrs;
    avail_access_error_descrs = avail_access_error_descrs->next;
  } else {
    /* Allocate a new entry. */
    aedp = (an_access_error_descr_ptr)alloc_fe(sizeof(an_access_error_descr));
#if DEBUG
/* GCOVR_EXCL_START */
    num_access_error_descrs_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  }  /* if */
  aedp->next = NULL;
  aedp->sym = NULL;
  aedp->overload_sym = NULL;
  aedp->position = pos_curr_token;
  aedp->token_sequence_number = NO_TOKEN_SEQUENCE_NUMBER;
  aedp->severity = es_none;
  aedp->error_code = ec_no_error;
  aedp->in_template_arg_list = FALSE;
  return aedp;
}  /* alloc_access_error_descr */
	

static void free_access_error_descr(an_access_error_descr_ptr aedp)
/*
Free the access error description entry pointed to by aedp.
Put the freed entry on the available list to be reused.
*/
{
  aedp->next = avail_access_error_descrs;
  avail_access_error_descrs = aedp;
}  /* free_access_error_descr */


void record_access_error(a_symbol_ptr            sym,
                         a_symbol_ptr            overload_sym,
                         a_type_ptr              protected_access_class,
                         a_source_position       *source_position,
                         a_symbol_locator        *locator,
                         an_error_severity       severity,
                         an_error_code           error_code,
                         a_boolean               *error_detected)
/*
An access error on "sym" has been detected.  If "sym" is a member of
an overload set, "overload_sym" is the symbol for the set.
source_position is the source position of the reference.
If protected_access_class is non-NULL, the checking is the special
protected member checking of 11.5 of the C++ standard, and
protected_access_class indicates the type of the object used
to access the member.  "locator" is the symbol locator for "sym",
or NULL one if not available (it's used only to suppress redundant
error messages, by setting the access_control_error_reported field).
If access checking is not being deferred, issue the error now.
Otherwise, create an access error entry so that the access error can
be rechecked or discarded later.

Normally issue_access_error determines the error code and severity to be
used, but they can also be specified by the caller using severity and
error_code.  If the default values are to be used, severity should be
es_none, and error_code should be ec_no_error.  The severity is only
used if error_code is not ec_no_error.

If error_detected is non-NULL, do not issue any diagnostics.  Just figure
out the error and severity, and return *error_detected TRUE if the selected
diagnostic would actually be an error.  That feature cannot be used in
a context where deferral of errors applies.
*/
{
  a_boolean			defer_access_checks = FALSE;
  a_scope_stack_entry_ptr	ssep = NULL;
  a_boolean			in_template_arg_list, in_decltype_context;
  a_boolean			in_expr_testing = in_expr_testing_context();

  if (scope_stack_top().make_access_errors_warnings) {
    /* We're in a context where we are supposed to reduce access errors
       to warnings. */
    severity = es_warning;
  } else if (in_expr_testing) {
    /* In SFINAE contexts, access errors should trigger a deduction
       failure. */
    if (!cpp11_sfinae_ignore_access) {
      severity = es_error;
    }  /* if */
  }  /* if */
  in_template_arg_list = scope_stack_top().in_template_arg_list;
  in_decltype_context = scope_stack_top().in_decltype_context;
  if (curr_deferred_access_scope != NO_SCOPE_DEPTH) {
    ssep = &scope_stack[curr_deferred_access_scope];
    defer_access_checks = ssep->defer_access_checks;
  }  /* if */
  if (!defer_access_checks || in_expr_testing) {
    a_boolean suppressed_error = FALSE;
    a_boolean *p_error_detected = error_detected;
    /* In rescan and similar contexts, do the check immediately so that the
       result can be returned. */
    if (error_detected == NULL && in_expr_testing) {
      p_error_detected = &suppressed_error;
    }  /* if */
    if (locator == NULL || !locator->access_control_error_reported) {
      issue_access_error(sym,
                         protected_access_class,
                         source_position, severity, error_code,
                         in_template_arg_list,
                         p_error_detected);
      if (locator != NULL) locator->access_control_error_reported = TRUE;
    }  /* if */
    if (suppressed_error) {
      /* This can only be true if error_detected == NULL and
         in_expr_testing_context(), meaning expr_stack != NULL and
         expr_stack->suppress_diagnostics == TRUE. */
      expr_stack->any_suppressed_error = TRUE;
    }  /* if */
  } else {
    /* Access checks are deferred, so put an entry on a list for later
       checking. */
    an_access_error_descr_ptr	aedp;
    /* If things go right, we should not be asking for an error return in
       contexts where deferral is active.  In overload resolution contexts
       we should not be checking access, and in SFINAE contexts we should
       already know the access context (because we're rescanning) and
       therefore deferral should not be active. */
    check_assertion_str(error_detected == NULL,
 "access check result needed immediately but access check deferral in effect");
    /* Look for an existing entry for this check.  Only create a new entry
       if none is found. */
    for (aedp = ssep->deferred_access_checks; aedp != NULL;
         aedp = aedp->next) {
      if (aedp->sym == sym &&
          aedp->overload_sym == overload_sym &&
          aedp->protected_access_class == protected_access_class &&
          aedp->token_sequence_number == curr_token_sequence_number &&
          aedp->severity == severity &&
          aedp->error_code == error_code &&
          aedp->in_template_arg_list == in_template_arg_list &&
          aedp->in_decltype_context == in_decltype_context &&
          cmp_source_positions(aedp->position, *source_position) == 0) {
        break;
      }  /* if */
    }  /* if */
    if (aedp == NULL) {
      /* No entry was found above. */
      aedp = alloc_access_error_descr();
      aedp->sym = sym;
      aedp->overload_sym = overload_sym;
      aedp->position = *source_position;
      aedp->protected_access_class = protected_access_class;
      aedp->token_sequence_number = curr_token_sequence_number;
      aedp->severity = severity;
      aedp->error_code = error_code;
      aedp->in_template_arg_list = in_template_arg_list;
      aedp->in_decltype_context = in_decltype_context;
      if (ssep->deferred_access_checks == NULL) {
        ssep->deferred_access_checks = aedp;
      }  /* if */
      if (ssep->last_deferred_access_check != NULL) {
        ssep->last_deferred_access_check->next = aedp;
      }  /* if */
      ssep->last_deferred_access_check = aedp;
    }  /* if */
  }  /* if */
}  /* record_access_error */


a_boolean f_check_for_ambiguity(a_symbol_locator *locator,
                                a_boolean        is_templ_context,
                                a_boolean        is_qualifier,
                                a_boolean        diagnostic_should_be_issued)
/*
Check whether the symbol indicated by the locator is ambiguous, and if
so issue an error at the position indicated in the locator (if
diagnostic_should_be_issued is TRUE), set the locator to an error
locator, and return TRUE.  If the symbol is not ambiguous, return FALSE.
*/
{
  a_boolean    err = FALSE;
  a_symbol_ptr sym = locator->specific_symbol;

  if (sym->ambiguous &&
      !(is_templ_context && sym->kind == (a_symbol_kind)sk_projection &&
        sym->variant.projection.injected_class_template_name_is_unambiguous)) {
    if (microsoft_bugs && microsoft_version >= 1400 && is_qualifier &&
        sym->kind == (a_symbol_kind)sk_projection &&
        sym->variant.projection.injected_class_template_name_is_unambiguous) {
      /* If a class has two base classes that are instances of the same class
         template, the Microsoft compiler (starting with version 8) allows a
         reference to the ambiguous injected class as the qualifier in a
         qualified name.  This should be ambiguous, but the Microsoft
         compiler selects the injected class name from the first base class.
         The name following the injected class name that was used as a
         qualifier can be any kind of member except a nonstatic data member.
         Our emulation, however, accepts any kind of reference.  For that
         reason, and because this is a particularly dangerous feature, we
         give a discretionary error and let users downgrade the diagnostic
         if they really need the feature.  It is dangerous because even in
         the case where the name after the qualifier refers to a static
         entity, the definition of the entity can be different in the two
         instances of the class template being used. */
      if (diagnostic_should_be_issued) {
        pos_sy2_diagnostic(es_discretionary_error,
                           ec_ambiguous_injected_template_name,
                           &locator->source_position, sym,
                           fundamental_symbol_of(sym));
      }  /* if */
      if (is_effective_error(ec_ambiguous_injected_template_name,
                             es_discretionary_error,
                             &locator->source_position)) {
        err = TRUE;
      }  /* if */
    } else {
      if (diagnostic_should_be_issued) {
        pos_sy_error(ec_ambiguous_name, &locator->source_position, sym);
      }  /* if */
      err = TRUE;
    }  /* if */
  }  /* if */
  if (err) set_to_error_locator(*locator);
  return err;
}  /* f_check_for_ambiguity */


void f_check_ambiguity_and_verify_access(a_symbol_locator *locator,
                                         a_boolean        is_templ_context,
                                         a_boolean        is_qualifier,
                                         a_boolean        *error_detected)
/*
Verify that the indicated symbol is not ambiguous and that we have
access to it.  In case of an ambiguity, the locator is set to an error
locator.  Note that no access checking is done on overloaded function symbols.

This routine will only be called for member symbols or symbols that
are ambiguous.  In other words, if the symbol is not ambiguous, it
must be a member symbol.

If an error is detected, the scope stack is consulted to see if access
errors should be deferred and rechecked later.  When access errors are
to be deferred, and an access error is detected, instead of issuing the
error immediately an access error descriptor is created that provides
information about the error that was detected.  Later
perform_deferred_access_checks will be called to repeat the access
checks that had failed earlier.  If the access checks still fail,
the errors may be issued or retained for yet another check.

The access deferral mechanism is used when processing definitions
of member functions and friend functions.  For member functions,
access to the return type cannot be checked until we know the parent
class of the member being defined.  For friend functions, access to the
return type and parameter types of the function cannot be checked until
we have scanned the entire function declarator.

is_templ_context is TRUE if the token following the identifier is a
"<" token and an unambiguous injected class template symbol should be
accepted even though the injected class symbol is ambiguous.  is_qualifier
is TRUE if the name is followed by the "::" in a qualified name.

If error_detected is non-NULL, return *error_detected set to TRUE if
there was an error, and do not issue any diagnostics (including warnings).
*/
{
  a_symbol_ptr   sym = locator->specific_symbol;
  a_symbol_ptr   fund_sym = fundamental_symbol_of(sym);
  a_boolean      issue_diagnostics = (error_detected == NULL);

  /* This routine looks like overload_check_ambiguity_and_verify_access. */
  if (!issue_diagnostics) *error_detected = FALSE;
  /* Issue an error if the symbol is ambiguous.  Symbols can be ambiguous
     either as a result of using directives or as a result of inheritance.
     Ambiguity checking must precede access control (ARM, 10.1.1). */
  if (f_check_for_ambiguity(locator, is_templ_context, is_qualifier,
                            issue_diagnostics)) {
    /* The symbol is ambiguous. */
    if (!issue_diagnostics) *error_detected = TRUE;
  } else if (locator->is_template_id) {
    /* The access of the template is checked when the template name
       is looked up.  For functions, access is checked after overload
       resolution has been done. */
  } else if (microsoft_mode &&
             (microsoft_version <= 1200 ||
              (is_qualifier && fund_sym->kind == (a_symbol_kind)sk_type)) &&
             sym != fund_sym &&
             !locator->is_qualified_name &&
             is_type_symbol(fund_sym)) {
    /* The Microsoft compiler (up to version 6) allows access to private types
       in base classes as long as they are named by the inherited name.
       The Microsoft compiler (all versions as of 9.0) also allows a private
       typedef to be used as a qualifier in a qualified name. */
  } else if (locator->is_qualified_name && locator->is_class_member &&
             is_enum_type(locator->parent.class_type)) {
    /* Committee paper P1787R6 clarified that access checking is not performed
       for enumeration-qualified enumerators as they are considered to be
       members of the enumeration. */
  } else if (!have_access_to_symbol(sym)) {
    /* The symbol is not accessible.  Issue the error or record it
       for later checking if access checking is deferred.  Suppress it
       if error_detected is non-NULL. */
    record_access_error(sym, (a_symbol_ptr)NULL, (a_type_ptr)NULL,
                        &locator->source_position, locator,
                        es_none, ec_no_error, error_detected);
  }  /* if */
}  /* f_check_ambiguity_and_verify_access */


void perform_deferred_access_checks_at_depth(a_scope_depth	depth)
/*
Go through the list of deferred access checks for the scope depth
specified by depth and repeat the test.  If the symbol is still not
accessible, the entry may either stay on the list (if the
defer_access_checks flag is still set) or an error may be issued.  The
ability to retain failed checks on the list is needed because the
deferred access checks need to be done in two phases.  First, member
access is checked during declarator processing when the class
reactivation scope has been pushed.  Later, after the entire function
declaration has been processed, we need to check for friend access.
In addition, the access of base-specifiers is checked while the class
scope is still active, but there could be other access errors that will
be reported when access deferral is ended by the enclosing context.
*/
{
  a_scope_stack_entry_ptr	ssep;

  check_assertion(depth != NO_SCOPE_DEPTH);
  ssep = &scope_stack[depth];
  if (ssep->deferred_access_checks != NULL) {
    an_access_error_descr_ptr	aedp = ssep->deferred_access_checks;
    an_access_error_descr_ptr	new_head = NULL;
    an_access_error_descr_ptr	new_tail = NULL;
    an_access_error_descr_ptr	next_aedp;
    a_boolean			remove_from_list = TRUE;
    a_source_position		prev_error_position;
    a_symbol_ptr		prev_error_symbol = NULL;
    prev_error_position = null_source_position;  /* Keep lint happy. */
    if (aedp != NULL) {
      a_boolean  saved_in_decltype_context =
                                        scope_stack_top().in_decltype_context;
      for (; aedp != NULL; aedp = next_aedp) {
        a_boolean	accessible;
        next_aedp = aedp->next;
        aedp->next = NULL;
        /* Temporarily set the "in_decltype_context" flag to match the original
           context.  This matters in Microsoft mode, where access errors are
           treated differently inside "decltype(...)" constructs. */
        scope_stack_top().in_decltype_context = aedp->in_decltype_context;
        if (aedp->protected_access_class != NULL) {
          /* Protected member check of 11.5 in the C++ standard. */
          if (prev_error_symbol == aedp->sym &&
              cmp_source_positions(prev_error_position, aedp->position) == 0) {
            /* We already issued an access error on this symbol at this
               position, so skip this one. */
            accessible = TRUE;
          } else {
            accessible = check_protected_member_access(
                                                 aedp->sym,
                                                 aedp->overload_sym,
                                                 (a_source_position *)NULL,
                                                 aedp->protected_access_class,
                                                 (a_boolean *)NULL);
          }  /* if */
        } else {
          /* Errors originally checked by overload_check_ambiguity... must
             be rechecked here using have_access_across_derivations. */
          if (aedp->overload_sym != NULL) {
            accessible = have_access_across_derivations(aedp->sym,
                                                        aedp->overload_sym);
          } else {
            accessible = have_access_to_symbol(aedp->sym);
          }  /* if */
        }  /* if */
        if (!accessible) {
          /* The access check still failed. */
          if (ssep->defer_access_checks) {
            /* Keep the entry on the list. */
            remove_from_list = FALSE;
          } else {
            issue_access_error(aedp->sym,
                               aedp->protected_access_class,
                               &aedp->position,
                               aedp->severity, aedp->error_code,
                               aedp->in_template_arg_list,
                               (a_boolean *)NULL);
            /* Record the symbol and position of the previous access error. */
            prev_error_symbol = aedp->sym;
            prev_error_position = aedp->position;
          }  /* if */
        }  /* if */
        if (remove_from_list) {
          free_access_error_descr(aedp);
        } else {
          /* If we are keeping the entry, add it to the new list. */
          if (new_head == NULL) new_head = aedp;
          if (new_tail != NULL) new_tail->next = aedp;
          new_tail = aedp;
        }  /* if */
      }  /* for */
      ssep->deferred_access_checks = new_head;
      ssep->last_deferred_access_check = new_tail;
      scope_stack_top().in_decltype_context = saved_in_decltype_context;
    }  /* if */
  }  /* if */
}  /* perform_deferred_access_checks_at_depth */


void perform_deferred_access_checks_for_function(a_routine_ptr rp)
/*
Push a function access scope and retry any failed access checks
that were encountered while the function declaration was being 
scanned.  This causes the accessibility to be reevaluated taking
into account possible friendship relationships.  rp points to the
routine entry of the function that was declared.
*/
{
  a_scope_stack_entry_ptr  ssep;

  check_assertion(curr_deferred_access_scope != NO_SCOPE_DEPTH);
  ssep = &scope_stack[curr_deferred_access_scope];
  /* This routine is always called last, so we can reset this flag now. */
  ssep->defer_access_checks = FALSE;
  if (ssep->deferred_access_checks != NULL) {
    if (rp->source_corresp.is_class_member) {
      push_class_reactivation_scope(parent_class_of(rp),
                                    /*extend_namespace=*/FALSE);
    }  /* if */
    (void)push_scope((a_scope_kind)sck_function_access, NO_SCOPE_NUMBER,
                     (a_type_ptr)NULL, rp);
    perform_deferred_access_checks();
    pop_scope();
    if (rp->source_corresp.is_class_member) pop_class_reactivation_scope();
  }  /* if */
}  /* perform_deferred_access_checks_for_function */


void f_discard_deferred_access_checks(a_scope_depth	depth)
/*
Free any deferred access checks that may have been created and clear
the list pointers.  depth is the scope depth at which the deferred
access check list should be discarded.
*/
{
  a_scope_stack_entry_ptr	ssep;

  check_assertion(depth != NO_SCOPE_DEPTH);
  ssep = &scope_stack[depth];
  if (ssep->deferred_access_checks != NULL) {
    an_access_error_descr_ptr	aedp = ssep->deferred_access_checks;
    an_access_error_descr_ptr	next_aedp;
    for (; aedp != NULL; aedp = next_aedp) {
      next_aedp = aedp->next;
      free_access_error_descr(aedp);
    }  /* for */
    ssep->deferred_access_checks = NULL;
    ssep->last_deferred_access_check = NULL;
  }  /* if */
}  /* f_discard_deferred_access_checks */


void discard_declarator_access_errors(void)
/*
Discard any deferred access checks that were recorded while scanning the
declarator name.  The current token must be the coalesced declarator
identifier at which point curr_token_sequence_number is the number of the first
token that is part of the generalized identifier.  All tokens after
the start of the declarator and before the next token are assumed to
be part of the declarator name.
*/
{
  a_scope_stack_entry_ptr	ssep;
  a_token_sequence_number	next_tok_seq_number;

  check_assertion(curr_deferred_access_scope != NO_SCOPE_DEPTH);
  ssep = &scope_stack[curr_deferred_access_scope];
  if (ssep->deferred_access_checks != NULL) {
    an_access_error_descr_ptr	aedp = ssep->deferred_access_checks;
    an_access_error_descr_ptr	new_head = NULL;
    an_access_error_descr_ptr	new_tail = NULL;
    an_access_error_descr_ptr	next_aedp;
    /* Get the sequence number associated with the next token. */
    (void)next_token_with_seq_number(&next_tok_seq_number);
    for (; aedp != NULL; aedp = next_aedp) {
      next_aedp = aedp->next;
      aedp->next = NULL;
      if (aedp->token_sequence_number >= curr_token_sequence_number &&
          aedp->token_sequence_number < next_tok_seq_number) {
        free_access_error_descr(aedp);
      } else {
        /* If we are keeping the entry, add it to the new list. */
        if (new_head == NULL) new_head = aedp;
        if (new_tail != NULL) new_tail->next = aedp;
        new_tail = aedp;
      }  /* if */
    }  /* for */
    ssep->deferred_access_checks = new_head;
    ssep->last_deferred_access_check = new_tail;
  }  /* if */
}  /* discard_declarator_access_errors */


void overload_check_ambiguity_and_verify_access(
                                            a_symbol_locator *locator,
                                            a_symbol_ptr     overloaded_symbol,
                                            a_boolean        *error_detected)
/*
Verify that the function symbol indicated in the locator (a specific
function from an overload set) is not ambiguous and that we have
access to it when it is viewed from the vantage point of overloaded_symbol;
issue an error if appropriate.  In case of an ambiguity,
the locator is set to an error locator.  overloaded_symbol is either
the sk_overloaded_function symbol containing the locator symbol, or
an sk_function_template symbol from which the locator symbol was
instantiated, or a projection symbol pointing to one of those two
kinds of symbols.  If error_detected is non-NULL, return *error_detected
set to TRUE if there was an error, and do not issue any diagnostics
(including warnings).
*/
{
  a_boolean issue_diagnostics = (error_detected == NULL);

  /* This routine looks like f_check_ambiguity_and_verify_access. */
  if (!issue_diagnostics) *error_detected = FALSE;
  /* Issue an error if the symbol is ambiguous.  Symbols can be ambiguous
     either as a result of using directives or as a result of inheritance.
     Ambiguity checking must precede access control (ARM, 10.1.1). */
  if (overloaded_symbol->ambiguous) {
    if (!issue_diagnostics) {
      *error_detected = TRUE;
    } else {
      pos_sy_error(ec_ambiguous_name, &locator->source_position,
                   overloaded_symbol);
    }  /* if */
    set_to_error_locator(*locator);
  } else if (scope_stack[depth_scope_stack].in_prototype_instantiation) {
    /* Suppress access checking during prototype instantiations.  Access
       checking cannot be done for a template, only for instances. */
  } else if (!overloaded_symbol->is_class_member) {
    /* Non-class-members are always accessible. */
  } else {
    a_symbol_ptr symbol = locator->specific_symbol;
    /* See if we have access to the symbol.  Note that we do not strip
       projection symbols from the specific symbol. */
    if (!have_access_across_derivations(symbol, overloaded_symbol)) {
      /* The symbol is not accessible.  Issue the error or record it
         for later checking if access checking is deferred.  Suppress it
         if error_detected is non-NULL. */
      record_access_error(symbol, overloaded_symbol, (a_type_ptr)NULL,
                          &locator->source_position, locator,
                          es_none, ec_no_error, error_detected);
    }  /* if */
  }  /* if */
}  /* overload_check_ambiguity_and_verify_access */


static an_access_specifier max_access_of_overloaded_function(a_symbol_ptr  sym)
/*
Given overloaded function symbol sym, return in *max_access the access control
value of the most accessible of the functions.
*/
{
  an_access_specifier  access, max_access;

#if CHECKING
  if (sym->kind != (a_symbol_kind)sk_overloaded_function) {
    internal_error("max_access_of_overloaded_functions: bad symbol kind");
  }  /* if */
#endif /* CHECKING */
  sym = sym->variant.overloaded_function.symbols;
  max_access = access_for_symbol(sym);
  while ((sym = sym->next) != NULL) {
    access = access_for_symbol(sym);
    if (is_more_accessible(access, max_access)) max_access = access;
  }  /* while */
  return max_access;
}  /* max_access_of_overloaded_function */


static a_boolean have_member_access_to_some_class_on_derivation(
                                                          a_base_class_ptr bcp)
/*
Return TRUE if we have member access to some class on the derivation of
the base class bcp.
*/
{
  a_boolean                   have_access = FALSE;
  a_base_class_derivation_ptr bcdp;
  a_derivation_step_ptr       dsp, tail;

  /* For each derivation (virtual base classes can have more than one): */
  for (bcdp = bcp->derivation; bcdp != NULL; bcdp = bcdp->next) {
    /* For each step on the derivation path, check to see if we have
       member access to the class. */
    tail = bcdp->path_tail;
    for (dsp = bcdp->path; dsp != tail->next; dsp = dsp->next) {
      a_base_class_ptr base_class = dsp->base_class;
      if (dsp != tail && base_class->is_virtual) {
        /* Virtual base class.  Do a recursive call to process the
           derivations of the virtual base class. */
        if (have_member_access_to_some_class_on_derivation(base_class)) {
          have_access = TRUE;
          goto have_accessibility;
        }  /* if */
      } else {
        /* Simple base class case. */
        if (have_member_access_privilege(base_class->type)) {
          /* Found a class to which we have member access. */
          have_access = TRUE;
          goto have_accessibility;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* for */
have_accessibility:
  return have_access;
}  /* have_member_access_to_some_class_on_derivation */


a_boolean check_protected_member_access(a_symbol_ptr      sym,
                                        a_symbol_ptr      proj_sym,
                                        a_source_position *err_pos,
                                        a_type_ptr        access_class,
                                        a_boolean         *error_detected)
/*
This routine implements the access control check mandated by 11.5 of
the C++ standard, which requires that a protected nonstatic member
accessed from a friend or member function of a derived class be
accessed through an object of the derived class or a class further
derived from that.  sym is the member (not overloaded, possibly a
projection symbol).  proj_sym is the same as sym, or is the overloaded
function symbol that contains sym, or it can be a projection symbol
for either of those.  The class of proj_sym is the "naming class" of
the reference, as defined by the standard.  access_class is the class
of the pointer or object through which the member is being accessed.
access_class is NULL if we don't know the object type (which will
cause an error).  access_class may also be an error type (which will
cause no error).  *err_pos is the source position for an error; if
it is NULL, no error is put out.  In all cases, the result value is
TRUE if the access is okay, and FALSE if there is an error.  This
routine is called only for nonstatic members, but it has not yet been
established that the member is protected in the naming class.
If error_detected is non-NULL, return *error_detected set to TRUE if
there was an error, and do not issue any diagnostics (including warnings).
*/
{
  a_boolean        have_access;
  a_type_ptr       base_class;
  a_base_class_ptr bcp;

  if (scope_stack[depth_scope_stack].in_prototype_instantiation) {
    /* Suppress access checking in prototype instantiations.
       One can check access only in instances of templates, not in
       the templates themselves. */
    have_access = TRUE;
  } else if ((any_cfront_mode() ?
                        access_for_symbol(fundamental_symbol_of(sym)) :
                        access_across_derivations(sym, proj_sym)) !=
                                           (an_access_specifier)as_protected) {
    /* The member is not protected, so the check does not apply. */
    /* The old test here (still done in some modes) is that the member is
       declared protected.  The new test (see core issue 385) is that the
       member is protected in the naming class.  That in particular allows
       using-declarations to make this check no longer apply by making the
       inherited member public. */
    have_access = TRUE;
  } else if (access_class == NULL) {
    /* Class is unknown; error. */
    have_access = FALSE;
  } else if (is_error_type(access_class)) {
    /* Class is an error type; no error. */
    have_access = TRUE;
  } else {
    sym = fundamental_symbol_of(sym);
    base_class = sym_parent_class(sym);
    access_class = skip_typerefs(access_class);
    /* Try to find a class class_type such that
         (1)  class_type is on the derivation list between base_class
              and access_class.  That is,
                base_class is a base class of
                    ^
                    |
                class_type, which is a base class of
                    ^
                    |
                access_class.
              class_type can be the same as either of the other
              classes.  In fact, all three can be the same.
              It is already known that base_class is a base class of
              access_class or is the same class.
         (2)  We have member access to class_type.
    */
    if (have_member_access_privilege(access_class)) {
      /* We have member access to the access_class, so we've found our
         class_type. */
      have_access = TRUE;
    } else if (same_entities(access_class, base_class)) {
      /* The endpoints are the same class, so there is no class that meets
         the requirement (we tested the only possible class above). */
      have_access = FALSE;
    } else {
      /* The endpoints are not the same, so examine the classes on the
         derivation(s) between them. */
      bcp = find_base_class_of(access_class, base_class);
#if CHECKING
      if (bcp == NULL) {
        internal_error("check_protected_member_access: base class not found");
      }  /* if */
#endif /* CHECKING */
      have_access = have_member_access_to_some_class_on_derivation(bcp);
    }  /* if */
  }  /* if */
  if (!have_access && err_pos != NULL) {
    record_access_error(sym, proj_sym, access_class,
                        err_pos, (a_symbol_locator *)NULL,
                        es_none, ec_no_error, error_detected);
  }  /* if */
  return have_access;
}  /* check_protected_member_access */


a_boolean is_accessible_base_class(a_base_class_ptr bcp)
/*
Return TRUE if the base class indicated by bcp is accessible from the
current point in the program, relative to the class of which it is a
base class.  bcp need not be an immediate base class of its derived
class.
*/
{
  a_boolean                   accessible = TRUE;
  a_derivation_step_ptr       dsp, tail;
  a_base_class_ptr            base_class;
  a_type_ptr                  curr_type;

  if (bcp->has_public_derivation) goto done;
  curr_type = bcp->derived_class;
  if (bcp->is_virtual) {
    /* Use a special subroutine for a virtual base class. */
    accessible = is_accessible_virtual_base_class(bcp, curr_type);
  } else {
    /* Non-virtual base class. */
    tail = bcp->derivation->path_tail;
    for (dsp = bcp->derivation->path; dsp != tail->next; dsp = dsp->next) {
      base_class = dsp->base_class;
      if (!is_accessible_imm_base_class(base_class, curr_type, bcp)) {
        accessible = FALSE;
        break;
      }  /* if */
      curr_type = base_class->type;
    }  /* for */
  }  /* if */
done:
  return accessible;
}  /* is_accessible_base_class */

#if GNU_EXTENSIONS_ALLOWED

a_boolean f_is_gnu_accessible_protected_base(a_base_class_ptr this_step,
                                             a_base_class_ptr target_base)
/*
Return TRUE if the class target_base->type is an accessible base of some
class in the scope stack.  This is used to emulate a bug in pre-4.4
versions of g++ that considered a protected base to be accessible in a cast
if the base class named in the conversion is accessible in the current
context.  For example, given

    struct B { };
    struct D1: protected B { };
    struct D2: B { ... };

g++ versions prior to 4.4 considered a cast from D1* to B* to be valid if
it occurred in the context of D2.

This function is intended to be called only via the macro
is_gnu_accessible_protected_base.
*/
{
  a_boolean               is_accessible = FALSE;
  a_scope_stack_entry_ptr ssep;
  a_scope_depth           scope_depth;

  /* Make sure that the checks in is_gnu_accessible_protected_base have
     been satisfied. */
  check_assertion(this_step->derivation->access ==
                                           (an_access_specifier)as_protected &&
                  gpp_mode && gnu_version < 40400);
  /* Scan through all class scopes on the scope stack to see if any of them
     is accessibly derived from the class represented by target_base. */
  for (scope_depth = depth_of_innermost_scope_that_affects_access_control;
       !is_accessible && scope_depth != NO_SCOPE_DEPTH;
       scope_depth = ssep->next_scope_that_affects_access_control) {
    ssep = &scope_stack[scope_depth];
    if (ssep->kind == (a_scope_kind)sck_class_struct_union ||
        ssep->kind == (a_scope_kind)sck_class_reactivation) {
      a_base_class_ptr local_base = find_base_class_of(ssep->assoc_type,
                                                       target_base->type);
      if (local_base != NULL && is_accessible_base_class(local_base)) {
        /* The conversion represented by this_step is valid under the old
           g++ rules. */
        is_accessible = TRUE;
      }  /* if */
    }  /* if */
  }  /* for */
  return is_accessible;
}  /* f_is_gnu_accessible_protected_base */

#endif /* GNU_EXTENSIONS_ALLOWED */

a_boolean is_accessible_virtual_base_class(a_base_class_ptr bcp,
                                           a_type_ptr       viewpoint_class)
/*
Return TRUE if the base class bcp (a virtual base class) is accessible from
the current point in the program, relative to viewpoint_class.
*/
{
  a_boolean                   accessible = FALSE, last_step;
  a_base_class_derivation_ptr bcdp, step_bcdp;
  a_derivation_step_ptr       dsp;
  a_base_class_ptr            base_class;
  a_type_ptr                  curr_type;

  check_assertion(bcp->is_virtual);
  /* A virtual base class can have multiple derivations.  Loop through
     each derivation in turn. */
  for (bcdp = bcp->derivation; bcdp != NULL; bcdp = bcdp->next) {
    a_derivation_step_ptr  tail = bcdp->path_tail;
    curr_type = viewpoint_class;
    /* Look through the path of the derivation. */
    for (dsp = bcdp->path; dsp != tail->next; dsp = dsp->next) {
      base_class = dsp->base_class;
      /* See if the base class at this step is accessible. */
      /* Virtual steps cause recursive calls, but treat the last step
         as a direct base class and as the specific derivation of the base
         class even if it is virtual. */
      last_step = (dsp == tail);
      step_bcdp = last_step ? bcdp : base_class->derivation;
      if ((!last_step &&
           is_virtual_but_not_simple_direct_base_class(base_class)) ?
          /* Non-simple virtual base class, not last step. */
          is_accessible_virtual_base_class(base_class, curr_type) :
          /* Simple direct base class, or last step on derivation. */
          is_accessible_direct_base_class_derivation(base_class, step_bcdp,
                                                     curr_type)) {
        /* Base class is accessible, so keep going on the path for this
           derivation. */
      } else {
        /* Base class is not accessible, so go on to the next derivation. */
        goto next_derivation;
      }  /* if */
      curr_type = base_class->type;
    }  /* for */
    /* We've found a derivation that gives access, so we can stop now. */
    accessible = TRUE;
    break;
next_derivation:;
  }  /* for */
  return accessible;
}  /* is_accessible_virtual_base_class */


/*
Representation of a base class declaration that is a candidate for
inheritance; that is, it represents the base class entity that is projected
into a derived class as an sk_projection symbol.
*/
typedef struct a_progenitor *a_progenitor_ptr;
typedef struct a_progenitor {
  a_progenitor_ptr
		next;
			/* Next in a linked list of progenitors; NULL for the
			   last entry on the list. */
  a_symbol_ptr	sym;
			/* Pointer to the progenitor symbol -- the symbol that
			   is projected into the derived class when the name
			   is inherited. */
  a_derivation_step_ptr
		path;
			/* Derivation path from the most derived class to the
			   base class in which the progenitor declaration
			   appears. */
  an_access_specifier
		access;
			/* The access of the progenitor symbol within the
			   derived class. */
} a_progenitor;

STATIC_THREAD a_progenitor_ptr
		avail_progenitors;
			/* Linked list of progenitor entries that are
			   available for reuse; may be NULL. */

static a_progenitor_ptr alloc_progenitor(void)
/*
Allocate a progenitor entry, initialize its fields, and return a pointer to it.
*/
{
  a_progenitor_ptr  pp;

  if (avail_progenitors == NULL) {
    /* Nothing on the available list to use. */
    pp = (a_progenitor_ptr)alloc_fe(sizeof(a_progenitor));
#if DEBUG
/* GCOVR_EXCL_START */
    num_progenitors_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  } else {
    /* Use the entry that heads the available list. */
    pp = avail_progenitors;
    avail_progenitors = pp->next;
  }  /* if */
  pp->next = NULL;
  pp->sym = NULL;
  pp->path = NULL;
  pp->access = (an_access_specifier)as_public;
  return pp;
}  /* alloc_progenitor */


static void free_progenitor(a_progenitor_ptr  pp)
/*
Return a progenitor entry to the available list.
*/
{
  if (pp->path != NULL) free_derivation_step(pp->path);
  pp->next = avail_progenitors;
  avail_progenitors = pp;
}  /* free_progenitor */


static void free_progenitor_list(a_progenitor_ptr  pp)
/*
Return a linked list of progenitor entries to the available list.
*/
{
  a_progenitor_ptr  next;

  for (; pp != NULL; pp = next) {
    next = pp->next;
    free_progenitor(pp);
  }  /* while */
}  /* free_progenitor_list */


/* Forward declaration. */
static a_progenitor_ptr find_progenitor(
			a_type_ptr                class_ptr,
                        a_symbol_locator          *locator,
                        an_id_lookup_options_set  options,
		        a_boolean		  look_in_dependent_bases,
		        a_boolean		  look_in_interfaces);

static a_progenitor_ptr find_progenitor_in_base_class(
                        a_base_class_ptr          base_class,
                        a_symbol_locator          *locator,
                        an_id_lookup_options_set  options,
		        a_boolean		  look_in_dependent_bases,
		        a_boolean		  look_in_interfaces)
/*
Given a pointer to a base class and a locator, determine whether the name
specified in the locator is declared either in the base class itself or in
a class from which the base class is derived.  Such a declaration is
referred to as the "progenitor" of a projection symbol, which may or may
not be created later.  If such a progenitor is found, return a pointer to
a progenitor entry (which, in the case of ambiguity, may be the head of a
linked list of progenitor entries); otherwise, return NULL.
look_in_dependent_bases is TRUE if the lookup should consider dependent
bases classes of generated template classes.  look_in_interfaces is TRUE if
the lookup should consider C++/CLI interface classes.
*/
{
  a_symbol_ptr      sym, tag_sym, using_decl_sym = NULL;
  a_scope_ptr       scope;
  a_boolean	    must_be_tag = (options & IDL_MUST_BE_TAG) != 0;
  a_progenitor_ptr  progenitor, pp;
  a_symbol_ptr      class_symbol;
  a_type_ptr        base_type = base_class->type;
  a_boolean         is_closure = type_is_lambda_closure(base_type);
  a_class_symbol_supplement_ptr
                    cssp;

  db_enter(4, "find_progenitor_in_base_class");
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 4) {
    fprintf(f_debug, "looking for \"%s\" in base class \"%s\"\n",
                     locator->symbol_header->identifier,
                     base_type->source_corresp.name);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  /* First look in the scope of the base class itself. */
  scope = base_type->variant.class_struct_union.extra_info->assoc_scope;
  if (scope_is_null_or_placeholder(scope)) {
    /* This is probably a nonreal class encountered during a prototype
       instantiation.  Ignore it. */
    sym = NULL;
  } else {
    /* We need search only the inactive symbols list, since a class cannot be
       declared as a base class unless it has been fully defined (at which
       point each of its member symbols is moved off the active list and onto
       the inactive list).  class_qualified_id_lookup is not called for two
       reasons:  to avoid unnecessary overhead and to prevent extra projection
       symbols from being created. */
    class_symbol = symbol_for(base_type);
    cssp = class_symbol->variant.class_struct_union.extra_info;
    sym = find_symbol_list_in_table(&cssp->pointers_block,
                                    locator->symbol_header);
    tag_sym = NULL;
    for (; sym != NULL; sym = sym->next_in_lookup_table) {
      if (sym->decl_scope == scope->number) {
        /* Ignore sk_undefined symbols. */
        if (sym->kind == (a_symbol_kind)sk_undefined) continue;
        /* Ignore this symbol if it doesn't match the lookup options
           specified by the caller. */
        if (!sym_matches_lookup_options(sym, options)) continue;
        /* Ignore fields of lambda closure classes -- they should not be found
           in base class lookups. */
        if (is_closure && symbol_is(sym, sk_field)) continue;
        if (is_tag_symbol(fundamental_symbol_of(sym))) {
          if (must_be_tag) {
            /* Tag symbol is required and that's what we have. */
            break;
          } else {
            /* Tag and nontag symbols can coexist in the same scope, and the
               latter are preferred, so keep looking -- but remember the tag
               symbol in case no other is found. */
            tag_sym = sym;
          }  /* if */
        } else {
          if (must_be_tag) {
            /* A tag symbol is required but this isn't one.  Keep looking. */
          } else {
            /* Found a match. */
            /* In C++ mode members are always in the nsk_other name space. */
            check_assertion_str2(
                     name_space_for_symbol_kind[(int)sym->kind] == nsk_other,
                     "find_progenitor_in_base_class:",
                     "unexpected name space kind");
            break;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
    if (sym == NULL) {
      sym = tag_sym;
    } else if (is_destructor_symbol(sym)) {
      /* A destructor cannot be inherited (ARM 12.4) so don't make a projection
         symbol for it. */
      sym = NULL;
    }  /* if */
    if (sym != NULL && sym->ambiguous) {
      /* Don't treat an ambiguous symbol as a progenitor; add each of its
         own progenitor symbols to the progenitor set that's returned to
         the caller. */
      if (is_class_member_using_decl_symbol(sym)) {
        /* Remember this symbol, however, so that the access recorded in the
           progenitor entry can be corrected. */
        using_decl_sym = sym;
      }  /* if */
      /* Set sym to NULL to force a call to find_progenitor. */
      sym = NULL;
    }  /* if */
  }  /* if */
  if (sym != NULL) {
    /* Found in the base class itself. */
#if DEBUG
/* GCOVR_EXCL_START */
    if (debug_level >= 4) db_symbol(sym, "found: ", 2);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
    progenitor = alloc_progenitor();
    progenitor->sym = sym;
    if (sym->kind == sk_overloaded_function) {
      progenitor->access = max_access_of_overloaded_function(sym);
    } else if (sym->kind == sk_projection) {
      progenitor->access =
                enum_cast<an_access_specifier>(sym->variant.projection.access);
    } else {
      progenitor->access = access_for_symbol(sym);
    }  /* if */
  } else {
    /* Not found in the current base class, so examine its own base classes,
       if any.  Note that a linked list of progenitor entries may be returned
       -- this usually represents an ambiguity. */
    progenitor = find_progenitor(base_type, locator, options,
				 look_in_dependent_bases, look_in_interfaces);
  }  /* if */
  /* Update the path and access fields of each entry in the set of
     progenitors.  (There will usually be only one.) */
  for (pp = progenitor; pp != NULL; pp = pp->next) {
    /* The path is not augmented if it starts with a virtual base class,
       unless it is only a single step.  This is consistent with the way
       derivations are constructed for base classes: the steps between the
       most derived class and an intermediate virtual base class are elided. */
    a_derivation_step_ptr  dsp = pp->path;
    if (dsp == NULL || dsp->next == NULL || !dsp->base_class->is_virtual) {
      pp->path = make_derivation_step(base_class, dsp);
      if (dsp != NULL) dsp->prev = pp->path;
    }  /* if */
    if (using_decl_sym != NULL) {
      pp->access = enum_cast<an_access_specifier>(
                                    using_decl_sym->variant.projection.access);
    }  /* if */
    pp->access = compute_access(pp->access,
                                preferred_derivation_of(base_class)->access);
  }  /* for */
  db_exit();
  return progenitor;
}  /* find_progenitor_in_base_class */


static a_derivation_path path_to_fundamental_symbol_base_class
                                              (a_symbol_ptr      sym,
                                               a_base_class_ptr  disambiguator)
/*
sym is a projection symbol.  Disambiguator is a base class of the current
most derived class that is intermediate between the base class we are looking
for and the derived class.  What we're looking for is the base class in the
derived class that corresponds to the base class associated with sym's
fundamental symbol.  Return the preferred derivation of that base class.
*/
{
  a_type_ptr             tp;
  a_base_class_ptr       bcp;
  a_derivation_path      path = { NULL, NULL };

  db_enter(4, "path_to_fundamental_symbol_base_class");
  /* Note that corresponding_base_class is not called, since it is hard to
     compute a disambiguator that is immediately derived from the base
     class we're looking for. */
  tp = sym->variant.projection.extra_info->fundamental_base_class->type;
  bcp = base_classes_of(disambiguator->derived_class);
  for (; bcp != NULL; bcp = bcp->next) {
    if (same_entities(bcp->type, tp)) {
      /* A base class with the right type. */
      if (!bcp->ambiguous || is_on_any_derivation_of(bcp, disambiguator)) {
        /* Either unambiguous or disambiguated. */
        a_base_class_derivation_ptr  preferred_derivation;
        preferred_derivation = preferred_derivation_of(bcp);
        path = { preferred_derivation->path,
                 preferred_derivation->path_tail };
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  check_assertion_str(path.head != NULL && path.tail != NULL,
                      "path_to_fundamental_symbol_base_class: not found");
  db_exit();
  return path;
}  /* path_to_fundamental_symbol_base_class */


static a_boolean injected_and_equiv_noninjected_symbol(a_symbol_ptr	sym1,
						       a_symbol_ptr	sym2)
/*
Return TRUE if either sym1 or sym2 is an injected class symbol, and the
other is the non-injected version of the same class.
*/
{
  a_boolean	result = FALSE;

  if (is_injected_class_symbol(sym1) != is_injected_class_symbol(sym2) &&
      is_type_symbol(sym1) && is_type_symbol(sym2)) {
    /* Both symbols are types, and one is an injected class name.  See
       if they refer to the same type. */
    a_type_ptr	type1;
    a_type_ptr	type2;
    type1 = type_symbol_type(sym1);
    type2 = type_symbol_type(sym2);
    result = identical_types(type1, type2);
  }  /* if */
  return result;
}  /* injected_and_equiv_noninjected_symbol */


static a_boolean progenitors_are_equivalent(a_progenitor_ptr  progenitor1,
                                            a_progenitor_ptr  progenitor2)
/*
Given two progenitors (referring to symbols projected into the same class
from two different base classes), return TRUE if their respective fundamental
symbols are the same (not only the same members of the same class but with
equivalent derivations).
*/
{
  a_symbol_ptr           sym1 = progenitor1->sym, sym2 = progenitor2->sym;
  a_derivation_step_ptr  dsp1 = progenitor1->path, dsp2 = progenitor2->path;
  a_boolean              equiv = FALSE;
  a_symbol_ptr           fundamental_sym1;
  a_symbol_ptr           fundamental_sym2;
  a_type_ptr             rout_type;
  a_derivation_step_ptr  tail1, tail2;

  db_enter(4, "progenitors_are_equivalent");
  fundamental_sym1 = fundamental_symbol_of(sym1);
  fundamental_sym2 = fundamental_symbol_of(sym2);
  if (fundamental_sym1 == fundamental_sym2) {
    /* Fundamental symbols are the same.  Set equiv to TRUE if they
       represent the same function, object, type, or enumerator (ARM 10.1.1).
       In other words, if they are independent of a class object, they are
       equivalent (any path to a static data member, for example, gets to
       the same object) or if they are dependent on the same class object
       (e.g., if a field belongs to a virtual base class). */
    switch (fundamental_sym1->kind) {
      case sk_field:
        /* Nonstatic data member.  Equivalence must be determined by comparing
           the paths to the subclass object. */
        break;
      case sk_overloaded_function:
        /* Overloaded function.  If there are any nonstatic member functions,
           we must compare the paths. */
        if (fundamental_sym1->
                      variant.overloaded_function.mixed_static_nonstatic) {
          /* One or more is a nonstatic member function. */
          break;
        } else {
          /* Either all are static or all are nonstatic.  Check the first in
             the list. */
          a_symbol_ptr  sym = fundamental_sym1->
                                    variant.overloaded_function.symbols;
          sym = fundamental_symbol_of(sym);
          if (sym->kind != (a_symbol_kind)sk_function_template) {
            rout_type = routine_symbol_type(sym);
          } else {
            rout_type = sym->variant.template_info->
                                         variant.function.routine->type;
          }  /* if */
          goto check_rout_type;
        }
      case sk_member_function:
        /* See if the member function is static.  Otherwise the paths must be
           compared. */
        rout_type = fundamental_sym1->variant.routine.ptr->type;
check_rout_type:
        if (!routine_type_is_nonstatic_member_function(rout_type)) {
          /* There is only one instance of a static member function. */
          equiv = TRUE;
        }  /* if */
        break;
#if MICROSOFT_EXTENSIONS_ALLOWED
      case sk_property_set:
        /* Property sets only appear in C++/CLI managed classes, which don't
           permit multiple base subobjects of the same type. */
        check_assertion(cli_or_cx_enabled);
        equiv = TRUE;
        break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      default:
        /* Static data member, member constant, or member type. */
        equiv = TRUE;
    }  /* switch */
    if (!equiv) {
      /* The fundamental symbols are the same but may not represent the same
         object (nonstatic data member) or routine (nonstatic member function).
         They will be considered the same only if the belong to the same
         base class subobject.  For example,
                   A{i}
                   |
                   V
                 /   \
                B     C
                 \   /
                   D
         If "i" were declared in A, it is inherited by D along two paths which
         are equivalent insofar as the lead to one and the same base class
         subobject.  However, in this case,
                A{i}  A{i}
                |     |
                B     C
                 \   /
                   D
         A::i is inherited by D along paths that lead to different base class
         subobjects named "A".  Thus the determination of equivalence requires
         determining whether the paths lead to the same of different base
         class subobjects. */
      /* If sym1 or sym2 is a projection symbol, use a path that goes all the
         way to the corresponding fundamental symbol instead of a path to the
         projection. */
      a_derivation_path  path1, path2;
      if (sym1->kind == (a_symbol_kind)sk_projection) {
        /* Find the path to the base class to which sym1 belongs. */
        path1 = path_to_fundamental_symbol_base_class(sym1, dsp1->base_class);
        tail1 = path1.tail;
      } else {
        /* Find the end of the path. */
        for (tail1 = dsp1; tail1->next != NULL; tail1 = tail1->next) {}
        path1 = { dsp1, tail1 };
      }  /* if */
      if (sym2->kind == (a_symbol_kind)sk_projection) {
        /* Find the path to the base class to which sym1 belongs. */
        path2 = path_to_fundamental_symbol_base_class(sym2, dsp2->base_class);
        tail2 = path2.tail;
      } else {
        /* Find the end of the path. */
        for (tail2 = dsp2; tail2->next != NULL; tail2 = tail2->next) {}
        path2 = { dsp2, tail2 };
      }  /* if */
      /* Find the end of each path. */
      if (same_entities(tail1->base_class->type, tail2->base_class->type)) {
        if (tail1->base_class->is_virtual) {
          /* If the ends of the paths refer to the same virtual base class,
             then the members belong to the same subobject. */
          if (tail2->base_class->is_virtual) equiv = TRUE;
        } else if (!tail2->base_class->is_virtual) {
          /* If they refer to the same nonvirtual base class, the paths must
             coincide for it to be the same subobject. */
          if (congruent_paths(path1, path2)) equiv = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  } else if (injected_and_equiv_noninjected_symbol(fundamental_sym1,
                                                   fundamental_sym2)) {
    /* One symbol is an injected class name and the other is the
       primary symbol for the same class. */
    equiv = TRUE;
  } else if (microsoft_mode) {
    /* If the two progenitors are from Microsoft nonreal instantiations,
       consider them equivalent if they are based on the same template. */
    a_type_ptr				parent_class1;
    a_type_ptr				parent_class2;
    parent_class1 = sym_parent_class(fundamental_sym1);
    parent_class2 = sym_parent_class(fundamental_sym2);
    check_assertion(is_immediate_class_type(parent_class1));
    check_assertion(is_immediate_class_type(parent_class2));
    if (parent_class1->variant.class_struct_union.
                                            is_ms_instantiated_nonreal_class &&
        parent_class2->variant.class_struct_union.
                                            is_ms_instantiated_nonreal_class) {
      a_class_symbol_supplement_ptr	cssp1;
      a_class_symbol_supplement_ptr	cssp2;
      cssp1 = symbol_supplement_for_class(parent_class1);
      cssp2 = symbol_supplement_for_class(parent_class2);
      equiv = cssp1->template_info == cssp2->template_info;
    }  /* if */
  }  /* if */
  db_exit();
  return equiv;
}  /* progenitors_are_equivalent */


static a_boolean check_for_dominance(a_symbol_ptr          sym1,
                                     a_symbol_ptr          sym2,
                                     a_derivation_step_ptr path_to_sym2,
                                     a_type_ptr            class_type)
/*
This routine returns TRUE if sym2 is on a path dominated by sym1.

Dominance is discussed (rather imprecisely) in ARM 10.1.1.  Briefly, if the
declaration of a name in a virtual base class is hidden/overridden by a
redeclaration along one of the paths from the virtual base class, an ambiguity
between the initial declaration and the redeclaration is resolved in favor of
the latter.  Consider, for example,
    class A {public: int i; };
    class B : virtual public A {public: int i; };
    class C : virtual public A {};
    class D : public B, public C {};
which graphically looks like this:
          A{i}
         /   \
        B{i}  C
         \   /
           D
Within the scope of D, where one derivation path for i leads to B::i and the
other leads to A::i, there is in fact no ambiguity, since the declaration of
i in B dominates all other paths from A.  Thus an unqualified reference to i
within the scope of D unambiguously refers to B::i (though of course a
qualified reference either to A::i or to C::i will pick up A::i).
*/
{
  a_boolean              dominated = FALSE;
  a_derivation_step_ptr  step;
  a_base_class_ptr       bcp, next_bcp, dominated_bcp = NULL;

  /* If sym1, the candidate dominating symbol, is a projection symbol, find
     its fundamental symbol. */
  reduce_projection_symbol_to_fundamental_symbol(sym1);
  /* Loop through the base classes of the class of which sym1 is a member. */
  /*lint --e{850} bcp modified in loop */
  for (bcp = base_classes_of(sym_parent_class(sym1));
       bcp != NULL;
       bcp = next_bcp) {
    next_bcp = bcp->next;
    /* We are interested only in virtual base classes. */
    if (bcp->is_virtual) {
      /* Translate the virtual base class into a base class of the common
         derived type. */
      bcp = corresponding_base_class(bcp, class_type, (a_base_class_ptr)NULL);
      if (dominated_bcp == NULL) {
        /* Do the same for the base class associated with the candidate for
           dominated declaration.  dominated_bcp is the base class in which
           sym2 was declared. */
        for (step = path_to_sym2; step->next != NULL; step = step->next) {}
        dominated_bcp = corresponding_base_class(step->base_class, class_type,
                                                 (a_base_class_ptr)NULL);
        if (sym2->kind == (a_symbol_kind)sk_projection) {
          /* Follow out to the fundamental symbol if sym2 is a projection.
             dominated_bcp will be the base class in which the fundamental
             symbol for sym2 was declared. */
          a_base_class_ptr  temp_bcp = sym2->variant.projection.extra_info->
                                                       fundamental_base_class;
          dominated_bcp = corresp_base_class(temp_bcp, dominated_bcp);
        }  /* if */
      }  /* if */
      /* If they are the same base class or if bcp is on any possible
         derivation of dominated_bcp, return TRUE.  Here's an example:
                       X
                      /|\
                     A B C
                      \|/
                       Y
                      /|\
                     D E F
                      \|/
                       Z
          A declaration of D::i dominates declarations of X::i, A::i, B::i,
          C::i, and Y::i, because (1) D is on one of the derivations of each
          of X, A, B, C, and Y, and (2) there is another derivation of each.
          The second may be assumed, since the search up the derivation graph
          stops when a name match is found; if the path through D were the
          only path to X, A, etc., X::i, A::i, etc., would not be found. */
      if (dominated_bcp == bcp ||
          is_on_any_derivation_of(dominated_bcp, bcp)) {
        dominated = TRUE;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return dominated;
}  /* check_for_dominance */       


static a_progenitor_ptr find_progenitor(
			a_type_ptr               class_ptr,
                        a_symbol_locator         *locator,
                        an_id_lookup_options_set options,
		        a_boolean		 look_in_dependent_bases,
		        a_boolean		 look_in_interfaces)
/*
Given a pointer to a class (or struct or union) type and a locator, find
in the classes from which the current class is derived symbols that would
serve as "progenitors" if the name specified in the locator is inherited in
a derived class.  Return a pointer to one or more progenitor entries (or NULL
if no such base-class symbol is found).  look_in_dependent_bases is TRUE if
the lookup should consider dependent bases classes of generated template
classes.  look_in_interfaces is TRUE if the lookup should consider C++/CLI
interface classes.
*/
{
  a_base_class_ptr       bcp;
  a_progenitor_ptr       progenitor_set = NULL, pp, prev, next;
  a_progenitor_ptr       new_set, new_pp, prev_in_new_set, next_in_new_set;
  a_boolean              retain_pp, retain_new_pp;  

  db_enter(4, "find_progenitor");
  bcp = class_ptr->variant.class_struct_union.extra_info->base_classes;
  /* Loop through the base classes. */
  for (; bcp != NULL; bcp = bcp->next) {
    /* When doing dependent name lookup certain base classes should be
       ignored for unqualified lookups.  The test of microsoft_mode and
       use_implicit_typename() causes the ignore_during_dependent_lookup
       flag to be used for function bodies when parsing nonclass templates.
       There is no "right" way to parse function bodies in permissive
       Microsoft mode, so this is a heuristic to improve certain cases. */
    if ((do_dependent_name_processing ||
         gpp_dependent_name_lookup ||
         (microsoft_mode && !use_implicit_typename())) &&
        !look_in_dependent_bases &&
        bcp->ignore_during_dependent_lookup) continue;
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* In C++/CLI mode, look_in_interfaces will be FALSE for lookups that
       begin in a non-interface class. */
    if (!look_in_interfaces && is_cli_interface_type(bcp->type)) continue;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* For the most part, we are only interested in the direct base classes
       (either virtual or nonvirtual).  However, it may happen that a virtual
       base class is marked as "direct" yet the path of greatest access is
       that of an indirect derivation; such cases are treated as indirect
       base classes. */
    if (preferred_derivation_is_direct(bcp)) {
      new_set = find_progenitor_in_base_class(bcp, locator, options,
  				             /*look_in_dependent_bases=*/TRUE,
                                             look_in_interfaces);
      if (new_set != NULL) {
        if (progenitor_set == NULL) {
          progenitor_set = new_set;
        } else {
          /* There's at least one item in each set.  Look for equivalence
             and dominance (which will allow eliminating some items) and
             then merge the sets. */
          prev_in_new_set = NULL;
          for (new_pp = new_set; new_pp != NULL; new_pp = next_in_new_set) {
            next_in_new_set = new_pp->next;
            prev = NULL;
            retain_new_pp = TRUE;
            for (pp = progenitor_set; pp != NULL; pp = next) {
              next = pp->next;
              retain_pp = TRUE;
              if (pp->sym->ambiguous || new_pp->sym->ambiguous) {
                /* Don't do any of the comparison tests (equivalence,
                   dominance) -- they depend on the fundamental symbol, but
                   that's not really reliable with an ambiguous name. */
              } else if (progenitors_are_equivalent(pp, new_pp)) {
                /* No ambiguity (presumably because sym and other_sym are the
                   same member of a virtually derived class); choose between
                   the two projections based on access. */
                if (is_more_accessible(new_pp->access, pp->access)) {
                  /* Remove pp from the progenitor set. */
                  retain_pp = FALSE;
                } else {
                  /* Remove new_pp from the new progenitor set. */
                  retain_new_pp = FALSE;
                }  /* if */
              } else if (check_for_dominance(pp->sym, new_pp->sym,
                                             new_pp->path, class_ptr)) {
                /* pp->sym dominates new_pp->sym, resolving a potential
                   ambiguity.  Remove new_pp from the new progenitor set. */
                retain_new_pp = FALSE;
              } else if (check_for_dominance(new_pp->sym, pp->sym, pp->path,
                                             class_ptr)) {
                /* new_pp->sym dominates pp->sym, resolving a potential
                   ambiguity.  Remove pp from the progenitor set. */
                retain_pp = FALSE;
              } else {
                /* An unresolved ambiguity.  Both entries will be retained. */
              }  /* if */
              if (!retain_pp) {
                /* The current entry in progenitor_set is to be eliminated.
                   Branch around pp and return it to the available list. */
                if (prev == NULL) {
                  progenitor_set = next;
                } else {
                  prev->next = next;
                }  /* if */
                free_progenitor(pp);
                /* Continue the inner loop.  Note that prev is not changed. */
              } else if (!retain_new_pp) {
                /* The current entry in new_set is to be eliminated.  Branch
                   around new_pp and return it to the available list. */
                if (prev_in_new_set == NULL) {
                  new_set = next_in_new_set;
                } else {
                  prev_in_new_set->next = next_in_new_set;
                }  /* if */
                free_progenitor(new_pp);
                /* Break out of the inner loop and advance to the next
                   member of the new progenitor set.  prev_new_pp should not
                   be adjusted before continuing the outer loop. */
                break;
              } else {
                /* Nothing was eliminated from either list.  Reset the
                   pointer that tracks the previous item on the list, for
                   use the next time through the inner loop. */
                prev = pp;
              }  /* if */
            }  /* for */
            if (retain_new_pp) {
              /* Reset the pointer that tracks the previous item on the list,
                 for use the next time through the outer loop.  (This is not
                 done when new_pp is eliminated -- in that case, the current
                 pointer is still valid.) */
              prev_in_new_set = new_pp;
            }  /* if */
          }  /* for */
          /* Now merge what's left of the two lists. */
          if (new_set != NULL) {
            if (progenitor_set == NULL) {
              /* Coverity bug: tool progenitor_set cannot be NULL. */
              /* coverity[dead_error_line] */
              progenitor_set = new_set;
            } else {
              for (pp = progenitor_set; pp->next != NULL; pp = pp->next) { }
              pp->next = new_set;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (options & IDL_EXCLUDE_BASE_INTERFACE_MEMBERS) {
    /* Traverse the set of progenitors that was collected, and remove any
       that come from an interface base. */
    a_progenitor_ptr  *p_progenitor = &progenitor_set;
    check_assertion(cli_or_cx_enabled);
    while (*p_progenitor != NULL) {
      a_symbol_ptr  sym = fundamental_symbol_of((*p_progenitor)->sym);
      a_type_ptr    sym_parent = sym_parent_class(sym);
      if (cli_class_type_kind_is(sym_parent, cctk_interface)) {
        /* Discard this entry. */
        pp = *p_progenitor;
        *p_progenitor = pp->next;
        free_progenitor(pp);
      } else {
        /* Move on to the next entry (if any). */
        p_progenitor = &(*p_progenitor)->next;
      }  /* if */
    }  /* while */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  db_exit();
  return progenitor_set;
}  /* find_progenitor */


a_symbol_ptr find_progenitor_symbol(
                      a_type_ptr               class_ptr,
                      a_symbol_locator         *locator,
                      an_id_lookup_options_set options,
		      a_boolean		       look_in_dependent_bases,
		      a_boolean		       look_in_interfaces,
                      a_derivation_step_ptr    *path,
                      an_access_specifier      *access,
                      a_boolean                *ambiguous,
                      a_boolean                *any_using_decl,
                      a_boolean                *unambiguous_injected_template)
/*
Given a pointer to a class (or struct or union) type and a locator, find
in the classes from which the current class is derived a symbol that
would serve as progenitor of the name specified in the locator.  Return the
progenitor symbol or NULL is none is found.  Set *ambiguous to TRUE if
there is more than one progenitor.  *path and *access (and *ambiguous as well
under certain circumstances) may be set by subroutines and are just passed
through back to the caller.  *any_using_decl is set if any progenitor candidate
represents a using declaration or is or the projection of symbol that does.
*unambiguous_injected_template is set when class_name_injection_enabled is
TRUE, when *ambiguous is also set, and when all members of the progenitor
set are instances of the same template.  look_in_dependent_bases is TRUE if
the lookup should consider dependent bases classes of generated template
classes.  look_in_interfaces is TRUE if the lookup should consider C++/CLI
interface classes.

*/
{
  a_symbol_ptr      progenitor_sym;
  a_progenitor_ptr  progenitor_set, progenitor, pp;

  db_enter(4, "find_progenitor_symbol");
  /* Get what may be a linked list of progenitor entries. */
  progenitor_set = find_progenitor(class_ptr, locator, options,
                                   look_in_dependent_bases,
                                   look_in_interfaces);
  if (progenitor_set == NULL) {
    /* Empty list.  Return NULL. */
    progenitor_sym = NULL;
  } else {
    progenitor = progenitor_set;
    progenitor_sym = progenitor->sym;
    if (progenitor->next == NULL) {
      /* Only one entry on the list. */
      *ambiguous = progenitor_sym->ambiguous;
    } else {
      /* A list of entries.  Select one to return as the progenitor symbol.
         If one of the symbols represents a type name, return that symbol.
         (This makes a difference in declaration processing, whereas in
         executable expression processing only the ambiguity is of interest.)
         Otherwise just return the first symbol seen. */
      an_access_specifier	prog_access;
      a_boolean			prog_is_type;
      prog_access = progenitor->access;
      if (!is_type_symbol(fundamental_symbol_of(progenitor_sym))) {
        for (pp = progenitor->next; pp != NULL; pp = pp->next) {
          if (is_type_symbol(fundamental_symbol_of(pp->sym))) {
            progenitor = pp;
            progenitor_sym = progenitor->sym;
            break;
          }  /* if */
        }  /* for */
      }  /* if */
      prog_is_type = is_type_symbol(fundamental_symbol_of(progenitor_sym));
      /* Use the access of the most accessible of the symbols. */
      for (pp = progenitor->next; pp != NULL; pp = pp->next) {
        if (is_more_accessible(pp->access, prog_access) &&
            prog_is_type == is_type_symbol(fundamental_symbol_of(pp->sym))) {
          progenitor = pp;
          progenitor_sym = progenitor->sym;
          prog_access = enum_cast<an_access_specifier>(
                                    progenitor_sym->variant.projection.access);
        }  /* if */
      }  /* for */
      *ambiguous = TRUE;
    }  /* if */
    /* If this projection is ambiguous, it may be appropriate to set a
       flag indicating that it is ambiguous for instances of a class template
       but not for the template itself.  E.g.,
         struct B : A<int>, A<double> { ... };
       A reference to "A" within B is ambiguous if one is interested in a
       class but unambiguous if one is interested in a template.  (This is
       an issue only when class-name-injection is enabled.) */
    if (*ambiguous && class_name_injection_enabled) {
      a_symbol_ptr  sym, templ_sym = NULL;

      /* Set the flag to TRUE and look to change it back to FALSE. */
      *unambiguous_injected_template = TRUE;
      /* Traverse the list of progenitor symbols. */
      for (pp = progenitor_set; pp != NULL; pp = pp->next) {
        sym = pp->sym;
        if (sym->kind == (a_symbol_kind)sk_projection) {
          /* Special handling when the progenitor is itself a projection. */
          if (sym->ambiguous &&
              !sym->variant.projection.
                              injected_class_template_name_is_unambiguous) {
            *unambiguous_injected_template = FALSE;
            break;
          }  /* if */
          sym = fundamental_symbol_of(sym);
        }  /* if */
        if (is_injected_template_symbol(sym)) {
          /* The name is a projection of an injected class template name. */
          sym = class_template_for_injected_template_symbol(sym);
          if (templ_sym == NULL) {
            /* Must be the first time through the loop. */
            templ_sym = sym;
          } else if (templ_sym != sym) {
            /* The templates don't match. */
            *unambiguous_injected_template = FALSE;
            break;
          }  /* if */
        } else {
          *unambiguous_injected_template = FALSE;
          break;
        }  /* if */
      }  /* for */
    }  /* if */
    *access = progenitor->access;
    /* Move the derivation path from the progenitor entry and return it to
       the caller.  Clear the pointer in the progenitor; otherwise the path
       would be freed. */
    *path = progenitor->path;
    progenitor->path = NULL;
    *any_using_decl =
          (progenitor_sym->kind == (a_symbol_kind)sk_projection &&
           (progenitor_sym->variant.projection.is_using_decl ||
            progenitor_sym->variant.projection.any_intervening_using_decl));
    /* Return the progenitor entries to the available list. */
    free_progenitor_list(progenitor_set);
  }   /* if */
  db_exit();
  return progenitor_sym;
}  /* find_progenitor_symbol */

    
static a_symbol_ptr create_nonreal_progenitor_symbol(
					 a_type_ptr	          class_type,
					 an_id_lookup_options_set options,
					 a_symbol_locator         *locator,
                                         a_derivation_step_ptr	  *path)
/*
Find a nonreal base class of class_type, create a member of that nonreal
base class, and return the symbol to the caller.  Create a derivation step
entry that points to the class in which the nonreal member is created.
*/
{
  a_symbol_ptr		sym;
  a_base_class_ptr	bcp = base_classes_of(class_type);
  a_base_class_ptr	nonreal_bcp = NULL;

  /* Loop through the base classes to find a direct nonreal base.  Any
     class with nonreal bases must have at least one direct nonreal base. */
  for (; bcp != NULL; bcp = bcp->next) {
    a_type_ptr		base_type = bcp->type;
    if (base_type->variant.class_struct_union.is_nonreal_class &&
        /*lint -e(506)*/!is_cli_generic_instance_type(base_type)) {
      if (bcp->direct) {
        nonreal_bcp = bcp;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  check_assertion_str2(nonreal_bcp != NULL,
                       "create_nonreal_progenitor_symbol:", "no nonreal base");
  sym = class_qualified_id_lookup(locator, nonreal_bcp->type,
                                  options | IDL_MEMBER_OF_UNKNOWN_BASE);
  check_assertion(sym != NULL);
  *path = make_derivation_step(nonreal_bcp, (a_derivation_step_ptr)NULL);
  return sym;
}  /* create_nonreal_progenitor_symbol */


static a_boolean check_for_microsoft_template_lookup_bug(a_symbol_ptr sym)
/*
The Microsoft compiler (as of version 4.2) includes a bug in the lookup
of template names that, in the following example, will find the global
template x instead of the base class member.

  template <class T> struct x {};
  class A {
    int x;
  };
  class B : public A {
    typedef x<int> xi;  // Microsoft compiler finds template ::x
  };

For this to occur the base class member must be a nonstatic member, or an
overload set containing nonstatic members.  Version 7.1 fixes a portion of
this problem.  7.1 only finds the enclosing template when it is a nonstatic
data member or an injected class name.

sym is the progenitor symbol that was found.  Return TRUE if it represents
a symbol that should be ignored in favor of a template to be found later.
*/
{
  a_boolean	result = FALSE;

  if (sym->kind == (a_symbol_kind)sk_field) {
    /* The name found is a nonstatic data member -- discard it. */
    result = TRUE;
  } else if (microsoft_version <= 1300 &&
             is_function_or_template_symbol(sym)) {
    a_boolean		mixed_static_nonstatic = FALSE;
    if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
      mixed_static_nonstatic =
                      sym->variant.overloaded_function.mixed_static_nonstatic;
      sym = sym->variant.overloaded_function.symbols;
    }  /* if */
    if (mixed_static_nonstatic) {
      /* There is at least one static member function.  This symbol must
         not be ignored. */
    } else {
      a_routine_ptr			rp;
      a_routine_type_supplement_ptr	rtsp;
      /* All of the symbols are either static or all are nonstatic.
         Check the first symbol on the list to see which. */
      if (sym->kind == (a_symbol_kind)sk_function_template) {
        rp = sym->variant.template_info->variant.function.routine;
      } else {
        rp = sym->variant.routine.ptr;
      }  /* if */
      rtsp = rp->type->variant.routine.extra_info;
      /* If the name found is a nonstatic member function, discard it. */
      if (rtsp->this_class != NULL) result = TRUE;
    }  /* if */
  } else if (is_injected_class_symbol(sym) &&
             depth_innermost_instantiation_scope != NO_SCOPE_DEPTH &&
             scope_stack[depth_innermost_instantiation_scope].assoc_type
                                                                     != NULL) {
    /* The name found is an injected class name referenced from within a
       class template -- discard it. */
    result = TRUE;
  }  /* if */
  return result;
}  /* check_for_microsoft_template_lookup_bug */


static a_boolean check_for_microsoft_type_lookup_bug(a_type_ptr   class_ptr,
						     a_symbol_ptr sym)
/*
The Microsoft compiler (as of version 6.0) includes a bug in the lookup
of type names in class definitions.  The caller is responsible for verifying
that the current scope is a class definition.  The bug does not occur
in class reactivations.

In Microsoft mode, the injected class name is not normally found, so
a reference to "Y" from within struct Y normally finds the base class
member and not the injected class name.

The Microsoft compiler seems to do a special lookup of the type name
in a declaration in a class definition.  This lookup considers only type
names from base classes (i.e., it ignores nontypes):

  struct A {
    int Y;
  };
  template <class T> struct Y : public T {
    Y* p;  // ::Y not A::Y
  };

sym is the symbol found from a base class.  class_ptr is the class in which
the lookup is being done.  Return TRUE if this is a symbol that should
be ignored as a result of the Microsoft bug.
*/
{
  a_boolean	result = FALSE;
  a_symbol_ptr	fund_sym = fundamental_symbol_of(sym);

  if (!is_type_symbol(fund_sym)) {
    /* Not a type.  For Microsoft 7.0 and above, see if the symbol has the
       same name as the current class. */
    a_symbol_ptr	class_sym;
    class_sym = (a_symbol_ptr)class_ptr->source_corresp.assoc_info;
    if (microsoft_version < 1300 ||
        (class_sym != NULL && class_sym->header == sym->header)) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* check_for_microsoft_type_lookup_bug */


a_boolean find_projected_symbol(
			a_type_ptr               class_ptr,
                        a_symbol_locator         *locator,
                        an_id_lookup_options_set options,
			a_boolean		 look_in_dependent_bases,
			a_boolean		 look_in_interfaces,
                        a_boolean                tentative_type_lookup,
                        a_boolean                tentative_template_lookup,
			a_boolean		 do_not_create_proj_sym,
                        a_boolean                add_to_active_list,
                        a_symbol_ptr             insert_sym,
                        a_symbol_ptr             *projected_symbol,
                        a_boolean		 can_create_nonreal)
/*
Given class_ptr, which identifies a class (or struct or union) type, search
its base classes for a symbol that projects the name specified in *locator
into the class.  If such a symbol is found, create a projection symbol for it
(marked "ambiguous" if there is more than one possible progenitor) and return
it to the caller through *projected_symbol; otherwise, set *projected_symbol
to NULL.  The new symbol is added to the symbol table in one of two ways,
depending on how add_to_active_list is set: if the flag is FALSE, the
new symbol is added to the beginning of the locator's inactive list;
if it is TRUE, it is inserted in the locator's active list (which is
order dependent) immediately following insert_sym (or, if insert_sym
is NULL, at the beginning of the list), and in addition it is added to
the end of the scope entry symbol list for the class.  The symbol
found must meet the criteria indicated by "options".  

look_in_dependent_bases is TRUE if the lookup should consider dependent
base classes of generated template classes.  look_in_interfaces is TRUE if
the lookup should consider C++/CLI interface classes.  If
tentative_type_lookup is TRUE, a projection symbol is only created if the
symbol returned by find_progenitor_symbol is a type.  Likewise, if
tentative_template_lookup is TRUE, a projection symbol is only created if
the symbol returned by find_progenitor_symbol is a template.  If
do_not_create_proj_sym is TRUE the creation of a projection symbol is
unconditionally suppressed.  Note that "options" and tentative_type_lookup
are handled differently: a symbol that fails the lookup options test does
not hide symbols from deeper base classes, while a symbol that is not a
type does hide symbols from deeper base classes that may be types.

can_create_nonreal is TRUE if, when looking for a projected symbol in a
class with a nonreal base, a member of the nonreal base should be
created if a projected symbol cannot be found in any of the real bases.
*/
{
  a_derivation_step_ptr		path = NULL;
  a_symbol_ptr			progenitor_sym;
  a_symbol_ptr			new_sym = NULL;
  an_access_specifier		access;
  a_boolean			ambiguous = FALSE;
  a_boolean			found;
  a_scope_stack_entry_ptr	ssep;
  a_symbol_ptr			class_sym;
  a_class_symbol_supplement_ptr	cssp;
  a_boolean			any_using_decl = FALSE;
  a_boolean		        fund_sym_is_nonreal_member = FALSE;
  a_boolean                     unambiguous_injected_template = FALSE;

  db_enter(4, "find_projected_symbol");
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 4) {
    fprintf(f_debug, "looking for projection of \"%s\" into class \"%s\"\n",
                     locator->symbol_header->identifier,
                     class_ptr->source_corresp.name != NULL ?
                         class_ptr->source_corresp.name : "<unnamed>");
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  class_sym = (a_symbol_ptr)class_ptr->source_corresp.assoc_info;
  cssp = class_sym->variant.class_struct_union.extra_info;
  if (locator->symbol_header == class_sym->header &&
      class_sym->
               variant.class_struct_union.extra_info->class_template == NULL &&
      (options & IDL_HIDDEN_NAME_LOOKUP) == 0) {
    /* A name X cannot be inherited into class X, since the "name slot" for
       is already taken (sort of) by the constructor. */
    progenitor_sym = NULL;
  } else if (locator->is_operator_name &&
             locator->variant.opname == (an_opname_kind)onk_assign) {
    /* Assignment operators are not inherited (13.4.3). */
    progenitor_sym = NULL;
  } else {
    progenitor_sym = find_progenitor_symbol(class_ptr, locator, options,
                                            look_in_dependent_bases,
                                            look_in_interfaces,
                                            &path, &access, &ambiguous,
                                            &any_using_decl,
                                            &unambiguous_injected_template);
    if (microsoft_bugs && progenitor_sym != NULL) {
      a_boolean	is_class_scope =
          scope_stack[depth_scope_stack].kind ==
                                        (a_scope_kind)sck_class_struct_union;
      /* Check for cases in which Microsoft ignores certain names. */
      if (tentative_template_lookup && is_class_scope) {
        /* In Microsoft bugs mode, if the progenitor symbol is for a nonstatic
           member (data or function), and we are doing a tentative template
           lookup in a class scope, ignore this symbol. */
        if (check_for_microsoft_template_lookup_bug(progenitor_sym)) {
          progenitor_sym = NULL;
        }  /* if */
      } else if (tentative_type_lookup &&
                 (microsoft_version >= 1310 || is_class_scope)) {
        /* In Microsoft bugs mode, ignore non-types found by a tentative
           type lookup and continue looking for the symbol in enclosing
           scopes.  This test is done in class scopes for Microsoft versions
           prior to 1310 and in all scopes thereafter. */
        if (check_for_microsoft_type_lookup_bug(class_ptr, progenitor_sym)) {
          progenitor_sym = NULL;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (progenitor_sym == NULL && can_create_nonreal &&
      cssp->any_nonreal_base_classes) {
    /* The symbol was not found in the class or in any of its "real"
       base classes.  This class has nonreal base classes, so we will
       assume that the name being looked up is a member of one of the
       nonreal base classes. */
    progenitor_sym = create_nonreal_progenitor_symbol(class_ptr, options,
                                                      locator, &path);
    /* Assume that the member is publicly accessible. */
    access = (an_access_specifier)as_public;
    fund_sym_is_nonreal_member = TRUE;
  }  /* if */
  if (progenitor_sym == NULL) {
    /* Indicate that no symbol was found and return a NULL pointer. */
    found = FALSE;
  } else {
    /* A symbol was found. */
    a_symbol_ptr	fund_progenitor_sym =
                                fundamental_symbol_of(progenitor_sym);
    found = TRUE;
    if (do_not_create_proj_sym) {
      /* Don't create a projection symbol and return the progenitor symbols
         as the result of the lookup (for hidden name lookups and other special
         lookups). */
      new_sym = progenitor_sym;
    } else if (tentative_type_lookup &&
               !(is_type_symbol(fund_progenitor_sym) ||
                 is_class_template_symbol(fund_progenitor_sym))) {
      /* The symbol found is not a type name symbol, so do not create a
         projection for it.  A class template symbol is returned on a
         tentative type lookup for improved error recovery. */
    } else if (tentative_template_lookup &&
               !symbol_is_or_contains_template(fund_progenitor_sym)) {
      /* The symbol found is not a template name symbol, so do not create a
         projection for it. */
    } else {
      /* Create a new symbol based on the symbol returned. */
      new_sym = make_projection_symbol(progenitor_sym, class_ptr,
                                       (a_base_class_ptr)NULL, path,
                                       ambiguous);
      new_sym->variant.projection.access = access;
      /* Set the flag indicating whether there are any intervening access
         declarations on any inheritance path linking the current class to
         the fundamental symbol. */
      new_sym->variant.projection.any_intervening_using_decl = any_using_decl;
      new_sym->variant.projection.fund_sym_is_nonreal_member =
                                                   fund_sym_is_nonreal_member;
      /* Mark projection symbols for names in dependent base classes as
         invisible.  Such projection symbols should not be found by normal
         lookup (because the underlying symbol would not be found). */
      if ((do_dependent_name_processing || gpp_dependent_name_lookup) &&
          is_unspecialized_template_class(class_ptr)) {
        new_sym->is_invisible = path != NULL &&
                              path->base_class->ignore_during_dependent_lookup;
      }  /* if */
      if (new_sym->ambiguous) {
        new_sym->
          variant.projection.injected_class_template_name_is_unambiguous =
                                                unambiguous_injected_template;
      }  /* if */
      /* Add the symbol to the symbol table. */
      if (add_to_active_list) {
        a_boolean	err;
        /* Insert the symbol into the active list. */
        if (insert_sym == NULL) {
          /* Insert at head of list. */
          new_sym->next = locator->symbol_header->symbol;
          locator->symbol_header->symbol = new_sym;
        } else {
          /* Insert following insert_sym. */
          new_sym->next = insert_sym->next;
          insert_sym->next = new_sym;
        }  /* if */
        /* Add the symbol to the scope symbol list, so that it will be moved
           to the inactive list when the scope is popped. */
        for (ssep = &scope_stack[depth_scope_stack];
             new_sym->decl_scope != ssep->number;
             ssep--) {
#if CHECKING
          if (ssep == &scope_stack[0]) {
#if DEBUG
/* GCOVR_EXCL_START */
            if (debug_level > 0) {
              fprintf(f_debug, "symbol name = %s\n",
                                new_sym->header->identifier);
            }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
            internal_error("find_projected_symbol: bad scope");
          }  /* if */
#endif /* CHECKING */
        }  /* for */
        add_symbol_to_scope_list(new_sym, scope_depth_of(ssep), &err);
      } else {
        /* Add it to the inactive list.  It can go at the beginning. */
        add_symbol_to_inactive_list(new_sym);
        add_symbol_to_lookup_table(new_sym, cssp->pointers_block.lookup_table);
      }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
      if (debug_level >= 4) db_symbol(new_sym, "symbol created: ", 2);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
    }  /* if */
    check_assertion(path != NULL);
    free_derivation_step(path);
  }  /* if */
  *projected_symbol = new_sym;

  db_exit();
  return found;
}  /* find_projected_symbol */


void add_vla_fixup_entry(a_type_ptr        array_type,
                         an_expr_node_ptr  expr_node,
                         a_symbol_ptr      param_sym,
                         a_source_position *position)
/*
Allocate and initialize a VLA fixup entry.  expr_node is an expression node
and will never be NULL.  Either array_type or param_sym will be non-NULL (but
not both).  (See comments on the definition of a_vla_fixup for further
details.)  The current scope will be a function prototype scope.  Add the
fixup entry to the end of the vla_fixup_list of the current scope stack entry.
*/
{
  a_vla_fixup_ptr          vfp;
  a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];

  db_enter(5, "add_vla_fixup_entry");
  if (avail_vla_fixups != NULL) {
    vfp = avail_vla_fixups;
    avail_vla_fixups = vfp->next;
  } else {
    vfp = (a_vla_fixup_ptr)alloc_fe(sizeof(a_vla_fixup));
#if DEBUG
/* GCOVR_EXCL_START */
    num_vla_fixups_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  }  /* if */
#if CHECKING
  check_assertion_str(ssep->kind == (a_scope_kind)sck_func_prototype,
                      "add_vla_fixup_entry: not func-prototype scope");
  check_assertion_str(expr_node != NULL,
                      "add_vla_fixup_entry: NULL expr node");
  if (array_type == NULL) {
    /* param_sym must be non-NULL and must be an sk_parameter symbol. */
    check_assertion_str(param_sym != NULL &&
                          param_sym->kind == (a_symbol_kind)sk_parameter,
                        "add_vla_fixup_entry: bad parameter symbol");
    /* The expression node should be the result of scanning the dummy
       parameter variable. */
    check_assertion_str(expr_node->kind == (an_expr_node_kind)enk_variable,
                        "add_vla_fixup_entry: bad expression node");
  } else {
    /* param_sym must be NULL and array_type must refer to a tk_array. */
    check_assertion_str(param_sym == NULL &&
                          array_type->kind == (a_type_kind)tk_array,
                        "add_vla_fixup_entry: bad array type");
  }  /* if */
#endif /* CHECKING */
  vfp->next = NULL;
  vfp->array_type = array_type;
  vfp->expr = expr_node;
  vfp->param_sym = param_sym;
  vfp->position = *position;
  if (ssep->vla_fixup_list == NULL) {
    ssep->vla_fixup_list = vfp;
  } else {
    a_vla_fixup_ptr  end_of_list = ssep->vla_fixup_list;
    while (end_of_list->next != NULL) end_of_list = end_of_list->next;
    end_of_list->next = vfp;
  }  /* if */
  db_exit();
}  /* add_vla_fixup_entry */


void free_vla_fixup_list(a_vla_fixup_ptr vfp)
/*
Add the indicated list of vla fixup entries to the available list.
*/
{
  if (avail_vla_fixups == NULL) {
    avail_vla_fixups = vfp;
  } else if (vfp != NULL) {
    /* Find the last entry on the list. */
    a_vla_fixup_ptr  end_of_list = vfp;
    while (end_of_list->next != NULL) end_of_list = end_of_list->next;
    /* Add the current available list to the end of the list passed by the
       caller. */
    end_of_list->next = avail_vla_fixups;
    avail_vla_fixups = vfp;
  }  /* if */
}  /* free_vla_fixup_list */


an_extern_type_fixup_ptr alloc_etype_fixup(void)
/*
Allocate an_extern_type_fixup entry and return a pointer to it.  A list
of such entries is used to record variables and routines whose types must
be restored to their outer-scope values at the end of a scope.  The entry
allocated is added to the front of the list of fixup entries for the
current scope (the front of the list is good, because the fixup list
should act like a stack if the same entity has several fixups).
*/
{
  an_extern_type_fixup_ptr ptr;

  db_enter(5, "alloc_etype_fixup");

  ptr = (an_extern_type_fixup_ptr)alloc_fe(sizeof(an_extern_type_fixup));
#if DEBUG
/* GCOVR_EXCL_START */
  num_extern_type_fixups_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  ptr->next            = scope_stack[depth_scope_stack].extern_type_fixup_list;
  ptr->type            = NULL;
  ptr->is_routine      = FALSE;
  ptr->variant.variable= NULL;
  scope_stack[depth_scope_stack].extern_type_fixup_list = ptr;

  db_exit();
  return ptr;
}  /* alloc_etype_fixup */


static a_param_id_ptr alloc_param_id(void)
/*
Allocate a parameter id block, set its fields to default values, and
return a pointer to it.  The locator field of the entry is set to
locator_for_curr_id.
*/
{
  a_param_id_ptr pip;

  db_enter(5, "alloc_param_id");
  if (avail_param_ids != NULL) {
    /* Reuse a previously freed entry. */
    pip = avail_param_ids;
    avail_param_ids = avail_param_ids->next;
  } else {
    /* Allocate a new entry. */
    pip = (a_param_id_ptr)alloc_fe(sizeof(a_param_id));
#if DEBUG
/* GCOVR_EXCL_START */
    num_param_ids_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  }  /* if */
  /* Set the entry's fields to default values. */
  pip->next = NULL;
  pip->symbol = NULL;
  pip->type = NULL;
  pip->declared_type = NULL;
  pip->type_pos = null_source_position;
  pip->storage_class = (a_storage_class)sc_unspecified;
  pip->eff_top_level_cv_quals = TQ_NONE;
  pip->implicitly_declared = FALSE;
  pip->is_parameter_pack = FALSE;
  pip->is_pack_element = FALSE;
  pip->uses_only_enclosing_pack = FALSE;
  pip->is_empty_pack_parameter = FALSE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  pip->is_decl_after_first_in_comma_list = FALSE;
  pip->source_sequence_entry = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  pip->dummy_vla_variable = NULL;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  pip->specifiers_range.start = null_source_position;
  pip->specifiers_range.end = null_source_position;
  pip->declarator_range.start = null_source_position;
  pip->declarator_range.end = null_source_position;
  pip->identifier_range.start = null_source_position;
  pip->identifier_range.end = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  pip->old_style_id_pos = null_source_position;
  pip->param_num = 0;
  db_exit();
  return(pip);
}  /* alloc_param_id */


static void free_param_id(a_param_id_ptr *ppip)
/*
Free the parameter id block pointed to by *ppip, set *ppip to NULL.
*/
{
  db_enter(5, "free_param_id");
  (*ppip)->next = avail_param_ids;
  avail_param_ids = *ppip;
  *ppip = NULL;
  db_exit();
}  /* free_param_id */


void free_param_id_list(a_param_id_ptr *pidlist)
/*
Free the list of parameter id blocks pointed to by *pidlist, and set
*pidlist to NULL.
*/
{
  a_param_id_ptr pip;

  db_enter(5, "free_param_id_list");
  while (*pidlist != NULL) {
    pip = *pidlist;
    *pidlist = pip->next;
    free_param_id(&pip);
  }  /* while */
  db_exit();
}  /* free_param_id_list */


a_param_id_ptr param_id_on_list(a_symbol_locator *locator,
                                a_param_id_ptr    param_id_list)
/*
Search the parameter id list given by param_id_list to see if the identifier
given by *locator is on it.  If so, return a pointer to the entry; if not,
return NULL.
*/
{
  a_param_id_ptr param_id = param_id_list;

  while (param_id != NULL) {
    if (param_id->symbol != NULL &&
        param_id->symbol->header == locator->symbol_header) {
      /* Found a match. */
      break;
    }  /* if */
    /* Keep searching the param id list for this name. */
    param_id = param_id->next;
  }  /* while */
  return(param_id);
}  /* param_id_on_list */


void add_to_param_id_list(
                        a_symbol_locator                       *locator,
                        a_type_ptr                             type_ptr,
                        a_source_position                      *type_pos,
                        a_storage_class                        storage_class,
                        a_func_info_block_ptr                  func_info,
                        ARG_UNUSED a_source_sequence_entry_ptr param_ssep,
                        a_param_id_ptr                         *last_param_id,
                        a_boolean                              is_pack_element)
/*
Create a new param_id entry and an sk_parameter symbol to go with it,
and add the former to the parameter id list pointed to by func_info;
*last_param_id points to the last entry on it.  type_ptr and
storage_class are the type and storage class for the parameter.
is_pack_element is TRUE if the parameter is a pack element.
*/
{
  a_param_id_ptr  new_param_id;
  a_symbol_ptr    sym;
  a_boolean       unnamed_param = FALSE, duplicate = FALSE;
  a_boolean       is_prototype_param_decl = (type_ptr != NULL);
  a_boolean       non_initial_variadic_param = FALSE;

  /* See if this identifier name already appears on the list.  If so, issue
     an error (except in some GNU modes, where duplicate parameter names are
     only diagnosed in function definitions).  Create a param_id entry if this
     is a prototype parameter list, but not otherwise. */
  if (!is_error_locator(*locator)) {
    if (is_pack_element && is_non_initial_variadic_element()) {
      /* Only the first copy of a element of a variadic parameter is entered
         into the symbol table. */
      non_initial_variadic_param = TRUE;
    } else if (param_id_on_list(locator, func_info->param_id_list) != NULL) {
      if (((gpp_mode && gnu_version < 40300) ||
           (gcc_mode && !is_prototype_param_decl)) &&
          !clang_mode) {
        duplicate = TRUE;
      } else {
        pos_error(ec_dupl_param_name, &error_position);
        set_to_error_locator(*locator);
      }  /* if */
    }  /* if */
  } else if (is_prototype_param_decl) {
    /* Assume that if an error locator is passed in and this is a prototype
       parameter declaration that we have an unnamed parameter.  We'll need
       a param_id entry to keep track of the type. */
    unnamed_param = TRUE;
  }  /* if */
  /* Create a param_id entry and enter it onto the param_id list.  Skip
     this if we have an old-style param id list in which a duplicate was
     encountered. */
  if (is_prototype_param_decl || !is_error_locator(*locator)) {
    new_param_id = alloc_param_id();
    /* Save the type and storage class for the later declaration. */
    if (is_prototype_param_decl) {
      new_param_id->type = type_ptr;
      check_assertion(type_pos != NULL);
      copy_source_position(*type_pos, new_param_id->type_pos);
      new_param_id->storage_class = storage_class;
    }  /* if */
    /* Create a parameter symbol.  It is used during parameter processing
       only.  The corresponding symbol in the function scope itself is a
       variable symbol for which the variable's is_parameter flag is set to
       TRUE. */
    if (unnamed_param) {
      /* Create no symbol for an unnamed parameter. */
      sym = NULL;
    } else {
      if (type_ptr != NULL) {
        /* Prototyped parameter list.  The symbol is entered in the function
           prototype scope.  It will later be copied to the function scope
           when it is changed to sk_variable. */
        if (non_initial_variadic_param) {
          /* Non-initial variadic parameters are not entered into the symbol
             table. */
          sym = create_symbol_for_non_initial_variadic_param(locator);
        } else {
          sym = enter_symbol((a_symbol_kind)sk_parameter, locator,
                             depth_scope_stack, duplicate);
        }  /* if */
        sym->is_pack_element = is_pack_element;
        if (parameters_visible_late) {
          /* In some GNU C++ modes, the parameters are invisible within the
             prototype scope.  This allows code like:
               struct X; void f(int X, X *p);
             The is_invisible flag will be cleared when parsing the function
             definition. */
          sym->is_invisible = TRUE;
        }  /* if */
      } else {
        /* Must be an old-style parameter declaration.  The type and storage
           class will be supplied later.  We won't actually enter this symbol
           until the function scope is pushed. */
        sym = make_parameter_symbol(locator);
      }  /* if */
      new_param_id->symbol = sym;
      sym->variant.param_id = new_param_id;
      if (duplicate) {
        sym->ambiguous = TRUE;
        sym->is_invisible = TRUE;
      }  /* if */
      set_decl_sequence_number(sym);
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    new_param_id->source_sequence_entry = param_ssep;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    /* Put this entry on the end of the list of param ids. */
    if (func_info->param_id_list == NULL) {
      func_info->param_id_list = new_param_id;
      scope_stack_top().param_id_list = new_param_id;
    } else {
      (*last_param_id)->next = new_param_id;
    }  /* if */
    (*last_param_id) = new_param_id;
  }  /* if */
}  /* add_to_param_id_list */


void defer_exception_spec_error(a_func_info_block  *func_info,
                                an_error_code      error_code,
                                a_source_position  *pos,
                                a_type             *assoc_type)
/*
Create an entry to record the need for an incomplete-type diagnostic on an
exception specification.  At this point in processing (while the exception
specification is being scanned), it is not known whether it belongs to a
function definition or to a non-defining declaration; since this may affect the
severity of the diagnostic, issuing the message is deferred.  func_info points
to the block in which the deferral is recorded.  error_code and *pos indicate
the particular diagnostic required and the error position.  assoc_type is the
incomplete type the diagnostic is associated with.
*/
{
  an_exception_spec_error_descr_ptr  esedp, end_of_list;

  db_enter(5, "defer_exception_spec_error");
  esedp = (an_exception_spec_error_descr_ptr)alloc_fe(
                                      sizeof(an_exception_spec_error_descr));
#if DEBUG
/* GCOVR_EXCL_START */
  num_exception_spec_error_descrs_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  esedp->next       = NULL;
  esedp->position   = *pos;
  esedp->error_code = error_code;
  esedp->assoc_type = assoc_type;
  if (func_info->exception_spec_errors == NULL) {
    func_info->exception_spec_errors = esedp;
  } else {
    end_of_list = func_info->exception_spec_errors;
    while (end_of_list->next != NULL) end_of_list = end_of_list->next;
    end_of_list->next = esedp;
  }  /* if */
  db_exit();
}  /* defer_exception_spec_error */


void report_exception_spec_errors(a_func_info_block  *func_info)
/*
Report one or more deferred exception-specification errors.  Suppress the
diagnostic if this is not a definition and not in strict conformance mode.
*/
{
  an_exception_spec_error_descr_ptr  esedp;
  an_error_severity                  severity;

  esedp = func_info->exception_spec_errors;
  if (esedp != NULL) {
    if (func_info->is_definition) {
      /* Always an error on a definition. */
      severity = es_error;
    } else if (strict_ansi_mode) {
      /* Always some diagnostic in strict mode. */
      severity = strict_ansi_discretionary_severity;
    } else {
      /* Except in strict mode, suppress the diagnostic on a declaration that
         does not define a function. */
      severity = es_none;
    }  /* if */
    if (severity != es_none) {
      /* Note that a diagnostic may have been deferred for more than one
         type. */
      for (; esedp != NULL; esedp = esedp->next) {
        issue_incomplete_type_diag(esedp->error_code, &esedp->position,
                                   esedp->assoc_type, severity);
      }  /* for */
    }  /* if */
  }  /* if */
}  /* report_exception_spec_errors */


void clear_func_info(a_func_info_block *func_info)
/*
Clear the fields of a function information block to default values.
*/
{
  func_info->prototype_scope_symbols     = NULL;
  func_info->param_id_list               = NULL;
  func_info->exception_specification     = NULL;
  func_info->throw_position              = null_source_position;
  func_info->exception_spec_errors       = NULL;
  func_info->scope_number                = NO_SCOPE_NUMBER;
  func_info->vla_fixup_list              = NULL;
  func_info->lambda                      = NULL;
  func_info->any_prototype_names_omitted = FALSE;
  func_info->is_inline                   = FALSE;
  func_info->is_definition               = FALSE;
  func_info->is_defaulted                = FALSE;
  func_info->is_deleted                  = FALSE;
  func_info->is_main_function            = FALSE;
  func_info->is_implicit_declaration     = FALSE;
  func_info->function_type_from_typedef  = FALSE;
  func_info->any_default_args            = FALSE;
  func_info->final                       = FALSE;
  func_info->override                    = FALSE;
  func_info->keep_param_id_list          = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  func_info->abstract                    = FALSE;
  func_info->sealed                      = FALSE;
  func_info->new_member                  = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if ASM_FUNCTION_ALLOWED
  func_info->is_asm_function             = FALSE;
#endif /* ASM_FUNCTION_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS
  func_info->is_movable_member_or_friend_def = FALSE;
#endif /* FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS */
  func_info->declarator_ssep                = NULL;
  func_info->declared_type                  = NULL;
  func_info->prototype_scope_ss_list        = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  func_info->max_member_alignment           = 0;
}  /* clear_func_info */


void clear_decl_modifiers_block(a_decl_modifiers_block_ptr  decl_modifiers)
/*
Clear the block passed around the declaration routines to represent
declaration modifiers.
*/
{
  decl_modifiers->flags = DM_NONE;
  decl_modifiers->direct_linkage_specifier = FALSE;
  decl_modifiers->marked_as_gnu_extension = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  decl_modifiers->is_deprecated = FALSE;
  decl_modifiers->is_microsoft_intrinsic = FALSE;
  decl_modifiers->uuid_string = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* clear_decl_modifiers_block */


void add_to_dependent_type_fixup_list(a_type_ptr                   type_ptr,
                                      a_dependent_type_fixup_kind  fixup_kind,
                                      char                         *entity_ptr,
                                      an_il_entry_kind             entity_kind,
                                      a_source_position            *pos)
/*
type_ptr is a pointer to an incomplete class/struct/union or enum type, and
entity_ptr and entity_kind together identify a type or param_type dependent
on it.  An fixup entry is created and put on a list so that the dependent
entity can be modified appropriately when the class or enum type is finally
defined.
*/
{
  a_dependent_type_fixup_ptr     dtfp, *start_of_list;
  a_symbol_ptr                   sym;

  db_enter(5, "add_to_dependent_type_fixup_list");
  /* A dependent type fixup entry is required for the type or param type. */
  if (avail_dependent_type_fixups != NULL) {
    /* Reuse a previously freed entry. */
    dtfp = avail_dependent_type_fixups;
    avail_dependent_type_fixups = avail_dependent_type_fixups->next;
  } else {
    /* Allocate a new entry. */
    dtfp = (a_dependent_type_fixup_ptr)alloc_fe(
                                            sizeof(a_dependent_type_fixup));
#if DEBUG
/* GCOVR_EXCL_START */
    num_dependent_type_fixups_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  }  /* if */
  dtfp->fixup_kind = fixup_kind;
  dtfp->entity.ptr = entity_ptr;
  dtfp->entity.kind = entity_kind;
  dtfp->decl_position = *pos;
  dtfp->next = NULL;
  /* Add the entry to the end of the appropriate list. */
  sym = symbol_for(type_ptr);
  if (is_class_symbol(sym)) {
    /* Use the list associated with the class. */
    start_of_list = &class_symbol_supp(sym)->dependent_type_fixup_list;
  } else {
    /* Use the list associated with the enum type. */
    check_assertion(sym->kind == (a_symbol_kind)sk_enum_tag);
    start_of_list = &sym->variant.enumeration.extra_info
                                              ->dependent_type_fixup_list;
  }  /* if */
  dtfp->next = *start_of_list;
  *start_of_list = dtfp;
  db_exit();
}  /* add_to_dependent_type_fixup_list */


void check_dependent_type_fixup_list(a_symbol_ptr  sym)
/*
Go through the entries on the dependent-type-fixup list for sym, which
identifies either a class type or an enumeration type.  The list is a
registry of entities that depend on the type but were declared before it
was defined. Now that the definition is there, the rest of the declaration
can be completed for the dependent types, too.
*/
{
  a_dependent_type_fixup_ptr     dtfp, next_dtfp, prev_dtfp;
  a_dependent_type_fixup_ptr     *start_of_list, list;
  a_type_ptr                     tp;

  if (is_class_symbol(sym)) {
    /* Check the list associated with the class. */
    start_of_list = &sym->variant.class_struct_union.extra_info->
                                              dependent_type_fixup_list;
  } else {
    /* Check the list associated with the enum type. */
    check_assertion(sym->kind == (a_symbol_kind)sk_enum_tag);
    start_of_list = &sym->variant.enumeration.extra_info
                                              ->dependent_type_fixup_list;
  }  /* if */
  list = *start_of_list;
  if (list != NULL) {
#if CHECKING
    a_boolean  any_entries_removed = FALSE;
#endif /* CHECKING */
    /* Keep going through the list until all its entries have been removed.
       Usually only one traversal is required, but multidimensional arrays
       may require extra trips. */
    do {
      /* Traverse the list. */
      prev_dtfp = NULL;
      for (dtfp = list; dtfp != NULL; dtfp = next_dtfp) {
        next_dtfp = dtfp->next;
        switch (dtfp->fixup_kind) {
          case dtfk_arg_transfer_method:
            check_assertion(dtfp->entity.kind == iek_param_type);
            /* A parameter of class type.  Set the flag indicating whether
               passing it requires a copy constructor call. */
            set_arg_transfer_method_flag((a_param_type_ptr)dtfp->entity.ptr,
                                           &dtfp->decl_position);
            break;
          case dtfk_array_type_size:
            check_assertion(dtfp->entity.kind == iek_type);
            tp = (a_type_ptr)dtfp->entity.ptr;
            if (!is_error_type(tp)) {
              check_assertion(is_array_type(tp));
              if (is_incomplete_type(tp->variant.array.element_type)) {
                /* The array is still incomplete.  This can happen if it is
                   dependent on another array that is still to be checked (the
                   case of "array of array of T").  Leave dtfp on the list and
                   continue. */
                prev_dtfp = dtfp;
                goto next_list_entry;
              } else {
                /* An array of elements of the (now complete) class or enum
                   type.  The array's size can be computed. */
                error_position = dtfp->decl_position;
                set_type_size(tp);
              }  /* if */
            }  /* if */
            break;
          case dtfk_array_of_abstract_class_check:
            check_assertion(dtfp->entity.kind == iek_type);
            tp = (a_type_ptr)dtfp->entity.ptr;
            if (!is_error_type(tp)) {
              /* Check if this was an array of an incomplete class type and
                 that class type has turned out to be an abstract class. */
              check_assertion(is_array_type(tp));
              if (is_abstract_class_type(tp->variant.array.element_type)) {
                abstract_class_diagnostic(es_error, ec_array_of_abstract_class,
                                          tp->variant.array.element_type,
                                          &dtfp->decl_position);
              }  /* if */
            }  /* if */
            break;
          case dtfk_routine_calling_method:
            check_assertion(dtfp->entity.kind == iek_type);
            tp = (a_type_ptr)dtfp->entity.ptr;
            if (!is_error_type(tp)) {
              check_assertion(is_function_type(tp));
              if (dtfp->fixup_kind ==
                   (a_dependent_type_fixup_kind)dtfk_routine_calling_method) {
                /* A function returning a class type.  Set the flag indicating
                   whether the return involves a copy constructor. */
                set_routine_calling_method_flag(tp, &dtfp->decl_position);
              }  /* if */
            }  /* if */
            break;
          default:
            unexpected_condition_str(
                            "check_dependent_type_fixup_list: bad fixup kind");
        }  /* switch */
        /* If the head of the list is being removed (the common case) reset the
           list pointer. */
        check_assertion((list == dtfp) == (prev_dtfp == NULL));
        if (list == dtfp) {
          list = next_dtfp;
        } else {
          prev_dtfp->next = next_dtfp;
        }  /* if */
        /* Remove dtfp from its list and add it to the available list. */
        dtfp->next = avail_dependent_type_fixups;
        avail_dependent_type_fixups = dtfp;
#if CHECKING
        any_entries_removed = TRUE;
#endif /* CHECKING */
next_list_entry:;
      }  /* for */
      /* If the inner loop is completed without eliminating all the entries on
         the list, go though it again. */
#if CHECKING
      if (!any_entries_removed) {
        /* No entries were removed on the last traversal of the list. */
        internal_error("check_dependent_type_fixup_list: looping error");
      }  /* if */
      /* Reset the flag for the next trip through the list. */
      any_entries_removed = FALSE;
#endif /* CHECKING */
    } while (list != NULL);
    /* Null out the list pointer before returning. */
    *start_of_list = NULL;
  }  /* if */
}  /* check_dependent_type_fixup_list */


static
a_namespace_list_entry_ptr list_entry_for_namespace(a_namespace_ptr nsp,
                                                    a_boolean       shared)
/*
Return a pointer to a namespace list entry that points to the namespace
nsp.  The namespace symbol supplement contains a pointer to a shared
namespace list entry.  This routine allocates and initializes that entry
if it has not yet been generated.  shared is TRUE if the caller can
use an entry that will be shared between multiple lists (i.e., it is
known to be the last entry on the list).  If a shared entry is
acceptable, the entry pointed to by the namespace symbol supplement
is returned.  If a nonshared entry is required, a new entry is
allocated and returned.

Note that nsp can be NULL, in which case the variable
global_namespace_list_entry is used to save a pointer to the shared
list entry (because the global namespace has no namespace symbol
supplement).
*/
{
  a_symbol_ptr				ns_sym;
  a_namespace_list_entry_ptr		nlep;
  a_namespace_symbol_supplement_ptr	nssp;

  if (nsp == NULL) {
    /* When nsp is NULL, the namespace is the global namespace. */
    nlep = global_namespace_list_entry;
    /* If the sharable entry has not yet been allocated, allocate one now. */
    if (nlep == NULL) {
      nlep = alloc_namespace_list_entry();
      nlep->ptr = nsp;
      global_namespace_list_entry = nlep;
    }  /* if */
  } else {
    /* A "real" namespace. */
    nsp = skip_namespace_aliases(nsp);
    ns_sym = (a_symbol_ptr)nsp->source_corresp.assoc_info;
    nssp = ns_sym->variant.namespace_info.extra_info;
    /* If the sharable entry has not yet been allocated, allocate one now. */
    if (nssp->namespace_list_entry == NULL) {
      nlep = alloc_namespace_list_entry();
      nlep->ptr = nsp;
      nssp->namespace_list_entry = nlep;
    }  /* if */
    nlep = nssp->namespace_list_entry;
  }  /* if */
  /* If the caller wants a shared entry, return the shared pointer.
     Otherwise allocate a new entry. */
  if (shared) {
    /* Use the value of nlep set above that points to the shared entry. */
  } else {
    /* Allocate a new entry and point it to the namespace. */
    nlep = alloc_namespace_list_entry();
    nlep->ptr = nsp;
  }  /* if */
  return nlep;
}  /* list_entry_for_namespace */


static
void add_to_operator_lookup_namespaces(a_class_symbol_supplement_ptr cssp,
                                       a_namespace_ptr		 nsp_to_add)
/*
See if the namespace pointed to by nsp_to_add is already on the
operator_lookup_namespaces list of cssp.  If it is not on
the list, add it to the front of the list.
*/
{
  a_namespace_list_entry_ptr	nlep;
  for (nlep = cssp->operator_lookup_namespaces;
       nlep != NULL; nlep = nlep->next) {
    /* If the namespaces match, exit the loop. */
    if (nlep->ptr == nsp_to_add) break;
  }  /* for */
  if (nlep == NULL) {
    nlep = list_entry_for_namespace(nsp_to_add, /*shared=*/FALSE);
    nlep->next = cssp->operator_lookup_namespaces;
    cssp->operator_lookup_namespaces = nlep;
  }  /* if */
}  /* add_to_operator_lookup_namespaces */
           


void determine_operator_lookup_namespaces(a_type_ptr	class_type)
/*
Build a list of the namespace of which the class or one of its base classes
is a member.  This list is used to determine which operator functions
should be considered for operands of a given class type.  The list that
is constructed may be partially or completely shared with a base class
of the class.
*/
{
  a_class_type_supplement_ptr	ctsp;
  a_class_symbol_supplement_ptr	cssp;
  a_namespace_ptr		class_nsp;
  a_base_class_ptr		bcp;

  check_assertion_str2(class_type->kind == (a_type_kind)tk_class ||
                       class_type->kind == (a_type_kind)tk_struct ||
                       class_type->kind == (a_type_kind)tk_union,
                       "deterine_operator_lookup_namespace:",
                       "type is not class type");
  ctsp = class_type->variant.class_struct_union.extra_info;
  cssp = symbol_supplement_for_class(class_type);
  check_assertion(cssp != NULL);
  /* Determine the namespace of this class. */
  class_nsp = namespace_enclosing_class(class_type);
  if (ctsp->base_classes == NULL) {
    /* No base classes.  The only namespace is the namespace of this class. */
    a_namespace_list_entry_ptr	nlep;
    nlep = list_entry_for_namespace(class_nsp, /*shared=*/TRUE);
    cssp->operator_lookup_namespaces = nlep;
  } else {
    /* This class has one or more direct base.  Share the list with the
       first base class, then add the namespace for this class, and the
       namespaces from any other direct bases to the list. */
    a_boolean				first_base_found = FALSE;
    /* Find any other direct bases in the base class list. */
    for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
      if (bcp->direct) {
        a_type_ptr			base_type = bcp->type;
        a_class_symbol_supplement_ptr	base_cssp;
        base_cssp = symbol_supplement_for_class(base_type);
        if (!first_base_found) {
          /* This is the direct base class with which we are sharing a list.
             Copy the namespace list pointer to the current class and add
             the namespace for this class to the front of the list. */
          cssp->operator_lookup_namespaces =
                                         base_cssp->operator_lookup_namespaces;
          add_to_operator_lookup_namespaces(cssp, class_nsp);
          first_base_found = TRUE;
        } else {
          /* Add the namespace from this base class to the front of the
             list for this class. */
          a_namespace_list_entry_ptr	nlep;
          for (nlep = base_cssp->operator_lookup_namespaces;
               nlep != NULL; nlep = nlep->next) {
            add_to_operator_lookup_namespaces(cssp, nlep->ptr);
          }  /* for */
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 5 || db_flag_is_set("operator_namespaces")) {
    a_namespace_list_entry_ptr	nlep = cssp->operator_lookup_namespaces;
    fprintf(f_debug, "operator namespaces for class: ");
    db_type_name(class_type);
    fprintf(f_debug, "\n");
    for (; nlep != NULL; nlep = nlep->next) {
      fprintf(f_debug, "  ");
      if (nlep->ptr == NULL) {
        fprintf(f_debug, "<global>");
      } else {
        db_name(&nlep->ptr->source_corresp);
      }  /* if */
      fprintf(f_debug, "\n");
    }  /* for */
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
}  /* determine_operator_lookup_namespaces */


a_namespace_ptr parent_namespace_for_symbol(a_symbol_ptr sym)
/*
Return the parent namespace of sym.  If sym is a class member, return
the parent of the outermost enclosing class.
*/
{
  a_namespace_ptr	nsp;

  /* If this is a class member, skip out to the outermost class type. */
  if (sym->is_class_member) {
    a_type_ptr	tp = sym_parent_class(sym);
    while (tp->source_corresp.is_class_member) {
      tp = parent_class_of(tp);
    }  /* while */
    /* Get the namespace pointer from the outermost class. */
    nsp = parent_namespace_or_null(tp);
  } else {
    nsp = sym_parent_namespace_or_null(sym);
  }  /* if */
  return nsp;
}  /* parent_namespace_for_symbol */


a_boolean is_local_symbol(a_symbol_ptr sym)
/*
Return TRUE if the indicated symbol is a function-local symbol.
*/
{
  a_boolean               is_local = FALSE;
  a_scope_stack_entry_ptr ssep;

  /* Reject the easy cases, i.e., class and namespace members. */
  if (sym_is_class_or_namespace_member(sym) ||
      sym->decl_scope == file_scope_number ||
      sym->synthesized_namespace_projection) {
    /* is_local = FALSE;  -- already set. */
  } else {
    /* Look through the scope stack for the scope of the symbol, to see
       whether the scope is a block scope. */
    for (ssep = &scope_stack[depth_scope_stack];
         ssep != NULL;
         ssep = previous_scope_of(ssep)) {
      if (ssep->number == sym->decl_scope) {
        /* Found the scope. */
        if (ssep->kind == (a_scope_kind)sck_block ||
            ssep->kind == (a_scope_kind)sck_function) {
          is_local = TRUE;
        }  /* if */
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return is_local;
}  /* is_local_symbol */


a_boolean is_block_extern_symbol(a_symbol_ptr sym)
/*
Return TRUE if the indicated symbol is a function-local block extern symbol.
This includes symbols for overload sets of block extern declarations.
*/
{
  a_boolean is_block_extern = FALSE;

  if (is_local_symbol(sym)) {
    /* Rule out using-declarations. */
    if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
      /* For an overload set, return TRUE if the set contains at least
         one block extern declaration. */
      a_symbol_ptr sym2;
      for (sym2 = sym->variant.overloaded_function.symbols;
           sym2 != NULL;
           sym2 = sym2->next) {
        if (sym2->kind != (a_symbol_kind)sk_namespace_projection) {
          is_block_extern = TRUE;
          break;
        }  /* if */
      }  /* for */
    } else if (sym->kind != (a_symbol_kind)sk_namespace_projection) {
      is_block_extern = TRUE;
    }  /* if */
  }  /* if */
  return is_block_extern;
}  /* is_block_extern_symbol */


static void clear_template_param_default_arg_info(a_template_param_ptr	ptr)
/*
Clear the default argument fields of a template parameter entry based
on kind of default argument it has.
*/
{
  a_symbol_kind	kind = ptr->param_symbol->kind;

  if (kind == (a_symbol_kind)sk_type) {
    ptr->default_arg.type = NULL;
  } else if (kind == (a_symbol_kind)sk_constant) {
    ptr->default_arg.constant = NULL;
  } else {
    ptr->default_arg.templ = NULL;
  }  /* if */
  clear_template_cache(&ptr->default_arg_cache);
}  /* clear_template_param_default_arg_info */


a_template_param_ptr make_copy_of_template_param_based_on_new_symbol(
					a_template_param_ptr	orig_tpp,
					a_symbol_ptr		new_sym)
/*
Allocate a new template parameter and copy the fields orig_tpp to the
new parameter.  Update the type/nontype/template referenced by the
parameter to refer to the entity represented by new_sym.  Return a
pointer to the new template parameter.
*/
{
  a_template_param_ptr	new_tpp;

  new_tpp = alloc_template_param(new_sym);
  /* Copy the old parameter to the new parameter. */
  *new_tpp = *orig_tpp;
  new_tpp->next = NULL;
  new_tpp->il_template_parameter = NULL;
  new_tpp->do_prototype_instantiation = FALSE;
  /* Update the fields that are based on the symbol to refer to the
     proper information. */
  new_tpp->param_symbol = new_sym;
  if (new_sym->kind == (a_symbol_kind)sk_type) {
    new_tpp->variant.type     = new_sym->variant.type.ptr;
  } else if (new_sym->kind == (a_symbol_kind)sk_constant) {
    new_tpp->variant.constant.ptr = new_sym->variant.constant;
  } else {
    /* A template template parameter. */
    check_assertion(new_sym->kind == (a_symbol_kind)sk_class_template);
    new_tpp->variant.templ = new_sym->variant.template_info;
  }  /* if */
  return new_tpp;
}  /* make_copy_of_template_param_based_on_new_symbol */


a_template_param_ptr alloc_template_param(a_symbol_ptr sym)
/*
Allocate a new template parameter list entry, initialize it,
and return a pointer to it.
*/
{
  a_template_param_ptr ptr;

  db_enter(5, "alloc_template_param");
  ptr = (a_template_param_ptr)alloc_fe(sizeof(a_template_param));
#if DEBUG
/* GCOVR_EXCL_START */
  num_template_params_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  check_assertion(sym != NULL);
  ptr->next           = NULL;
  ptr->param_symbol   = sym;
  new (&ptr->cache) a_template_cache();
  clear_template_cache(&ptr->cache);
  ptr->has_default_arg = FALSE;
  ptr->def_arg_involves_template_param = FALSE;
  ptr->def_arg_has_not_been_scanned = FALSE;
  ptr->def_arg_from_other_decl = FALSE;
  ptr->is_pack = FALSE;
  ptr->is_pack_expansion = FALSE;
  ptr->is_pack_element = FALSE;
  ptr->is_empty_pack = FALSE;
  ptr->do_prototype_instantiation = FALSE;
  ptr->is_dependent = FALSE;
  ptr->used_in_alias = FALSE;
  ptr->uses_auto = FALSE;
  if (sym->kind == (a_symbol_kind)sk_type) {
    ptr->variant.type     = sym->variant.type.ptr;
  } else if (sym->kind == (a_symbol_kind)sk_constant) {
    ptr->variant.constant.ptr = sym->variant.constant;
    ptr->variant.constant.type_involves_template_param = FALSE;
  } else {
    /* A template template parameter. */
    check_assertion(sym->kind == (a_symbol_kind)sk_class_template);
    ptr->variant.templ = sym->variant.template_info;
  }  /* if */
  ptr->il_template_parameter = NULL;
  new (&ptr->default_arg_cache) a_template_cache();
  clear_template_param_default_arg_info(ptr);
  ptr->param_num = 0;
  db_exit();
  return ptr;
}  /* alloc_template_param */


a_master_instance_ptr alloc_master_instance(void)
/*
Allocate a master instance entry, initialize its fields, and return a
pointer to it.
*/
{
  a_master_instance_ptr  mip;

  mip = (a_master_instance_ptr)alloc_fe(sizeof(a_master_instance));
#if DEBUG
/* GCOVR_EXCL_START */
  num_master_instances_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  mip->next                        = NULL;
  mip->instance                    = NULL;
  mip->name                        = NULL;
  mip->instance_required_count     = 0;
  mip->already_instantiated        = FALSE;
  mip->automatically_instantiated  = FALSE;
  mip->add_to_request_file	   = FALSE;
  mip->is_static_or_inline	   = FALSE;
  return mip;
}  /* alloc_master_instance */


a_template_instance_ptr alloc_template_instance(void)
/*
Allocate a new function instantiation entry and return a pointer to it.
*/
{
  a_template_instance_ptr  tip;

  db_enter(5, "alloc_template_instance");
  tip = (a_template_instance_ptr)alloc_fe(sizeof(a_template_instance));
#if DEBUG
/* GCOVR_EXCL_START */
  num_template_instances_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  tip->next                        = NULL;
  tip->next_in_instantiation_list  = NULL;
  tip->master_instance             = NULL;
  tip->instance_sym                = NULL;
  tip->template_sym                = NULL;
  tip->template_used_for_instantiation
                                   = NULL;
  tip->referencing_namespace       = NULL;
  tip->template_info               = NULL;
  tip->prototype_scope_symbols     = NULL;
  tip->exported_template_file      = NULL;
  tip->instantiation_required      = FALSE;
  tip->suppress_instantiation      = FALSE;
  tip->is_guiding_decl             = FALSE;
  tip->explicit_instantiation      = FALSE;
  tip->class_explicitly_instantiated
                                   = FALSE;
  tip->explicit_do_not_instantiate = FALSE;
  tip->explicit_can_instantiate    = FALSE;
  tip->can_be_instantiated	   = FALSE;
  tip->on_instantiations_list	   = FALSE;
  tip->error_issued                = FALSE;
  tip->suppress_default_arg_instantiations
                                   = FALSE;
  tip->instantiation_requested_for_constant_value
				   = FALSE;
  tip->explicit_instantiation_pos  = null_source_position;
  tip->pos_of_first_reference      = null_source_position;
  tip->param_id_list               = NULL;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  tip->declared_type               = NULL;
  tip->declared_type_for_default_arg_fixup
                                   = NULL;
#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  tip->partial_instantiation       = NULL;
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  db_exit();
  return tip;
}  /* alloc_template_instance */


a_scope_number take_next_scope_number(void)
/*
Assign the next scope number in sequence, and return it.
*/
{
  a_scope_number	result;

  if (next_scope_number == MAX_SCOPE_NUMBER) {
    /* The number of scopes exceeds the size of the scope number field. */
    catastrophe(ec_program_too_large);
  }  /* if */
  result = next_scope_number++;
  if (result >= (long)size_of_trans_unit_for_scope) {
    /* The table used to map scope numbers to translation unit pointers
       is full.  Expand it by reallocating it. */
    sizeof_t new_size = size_of_trans_unit_for_scope +
                        TRANS_UNIT_FOR_SCOPE_INCREMENTAL_ALLOCATION;
    trans_unit_for_scope = (a_translation_unit_ptr*)realloc_buffer(
                      (char *)trans_unit_for_scope,
                      (sizeof_t)(size_of_trans_unit_for_scope *
                                 sizeof(a_translation_unit_ptr)),
                      (sizeof_t)(new_size * sizeof(a_translation_unit_ptr)));
    size_of_trans_unit_for_scope = new_size;
  }  /* if */
  /* Record the translation unit with which this scope is associated. */
  trans_unit_for_scope[result] = curr_translation_unit;
  return result;
}  /* take_next_scope_number */


a_boolean symbol_is_from_trans_unit(a_symbol_ptr		sym,
				    a_translation_unit_ptr	tup)
/*
Return TRUE if the declaration scope of sym is from a scope associated with the
translation unit specified by tup.  If the symbol has no associated translation
unit information, return TRUE.
*/
{
  a_boolean result = TRUE;

  if (symbol_has_trans_unit_ptr(sym)) {
    result = trans_unit_for_symbol(sym) == tup;
  }  /* if */
  return result;
}  /* symbol_is_from_trans_unit */


a_translation_unit_ptr get_trans_unit_for_scope(a_scope_number	scope_number)
/*
Return the translation unit pointer for the translation unit that contains
scope_number.
*/
{
  a_translation_unit_ptr tup;

  check_assertion(scope_number != NO_SCOPE_NUMBER);
  tup = trans_unit_for_scope[scope_number];
  check_assertion(tup != NULL);
  return tup;
}  /* get_trans_unit_for_scope */


a_translation_unit_ptr trans_unit_for_symbol(a_symbol_ptr	sym)
/*
Return the translation unit pointer for the translation unit in which
"sym" was declared.
*/
{
  /* If this assertion fails either no symbol or a symbol that has no
     translation unit information was passed.  The symbol likely should have
     translation unit information or the caller needs to be corrected to filter
     out symbols that don't have translation unit information. */
  check_assertion(symbol_has_trans_unit_ptr(sym));
  a_scope_number         scope_number = sym->decl_scope;
  a_translation_unit_ptr tup = trans_unit_for_scope[scope_number];
  /* If this assertion fails, the scope number is likely incorrect 
     and thus is accessing a bad translation unit pointer.  Otherwise,
     the translation unit was never properly set for
     trans_unit_for_scope. */
  check_assertion(tup != NULL);
  return tup;
}  /* trans_unit_for_symbol */


a_symbol_ptr f_class_template_for_type(a_type_ptr	type)
/*
If "type" is based on a class template, return the class template on which
it is based; otherwise, return NULL.  This routine is only called for
types that are known to be template classes.
*/
{
  a_symbol_ptr	result = NULL;
  a_class_symbol_supplement_ptr	cssp;

  if (in_front_end) {
    /* The symbol table is not available after the front end terminates. */
    cssp = symbol_supplement_for_class(type);
    result = cssp->class_template;
  }  /* if */
  return result;
}  /* f_class_template_for_type */


a_symbol_ptr class_template_for_injected_template_symbol(a_symbol_ptr sym)
/*
sym is an injected template name symbol.  Return the class template symbol
for the class template of which this class is an instance.
*/
{
  a_type_ptr			templ_class_type;
  a_class_symbol_supplement_ptr	cssp;
  a_symbol_ptr			template_sym;

  templ_class_type = sym->variant.type.ptr;
  cssp = symbol_supplement_for_class(templ_class_type);
  template_sym = cssp->class_template;
  /* If this class is from a partial specialization, get the symbol for the
     primary template. */
  template_sym = primary_template_of(template_sym);
  return template_sym;
}  /* class_template_for_injected_template_symbol */


static void create_constant_from_token_spelling(a_constant_ptr cp)
/*
Replace *cp with a ck_string constant containing the spelling of the
current token.
*/
{
  a_targ_size_t token_len =
              (a_targ_size_t)(end_of_curr_token -
                              start_of_curr_token + 2) /*lint --e(571)*/
                                                       /*lint --e(776)*/;
  char          *str;
  a_const_char  *tok_str = start_of_curr_token;

  if (tok_str == NULL) {
    /* This function can be called with a NULL start_of_curr_token in some
       error situations.  Use an empty string as the spelling. */
    expect_error();
    token_len = 1;
    tok_str = "";
  }  /* if */
  str = copy_string_of_length_to_region(file_scope_region_number,
                                        tok_str, (sizeof_t)token_len);
  /* Overwrite the character following the token spelling with the
     terminating zero byte. */
  str[token_len - 1] = '\0';
  clear_constant(cp, (a_constant_repr_kind)ck_string);
  cp->variant.string.length = token_len;
  cp->variant.string.value = str;
  cp->type = string_literal_type((a_character_kind)chk_char,
                                                  token_len);
}  /* create_constant_from_token_spelling */


a_symbol_ptr find_literal_operator(a_const_char      *name,
                                   sizeof_t          name_len,
                                   a_source_position *pos,
                                   a_type_ptr        literal_type,
                                   a_boolean         from_cache,
                                   a_diagnostic_ptr  dp)
/*
name and name_len specify the ud-suffix of a user-defined literal (C++11
Standard 2.14.8 [lex.ext]) and pos is the start of the literal or of the
literal-operator-id in which name appears (NULL to suppress diagnostics for
tentative lookups); literal_type is the type of the literal and determines
the type of the first parameter of the literal operator (the list of
potential types is found in 13.5.8 [over.literal] of the C++11 Standard).
The name of the literal operator or literal operator template is looked up.
If the lookup finds a single matching function, return the corresponding
symbol; otherwise, return the overloaded function symbol or NULL, if no
literal operator or literal operator template with the designated name has
yet been declared.  If ambiguous symbols are found and dp is non-NULL, put
out an "additional info" diagnostic for each one.  dp is a pointer to the
primary diagnostic entry with which any new messages should be attached.
As a side effect, locator_for_curr_id is set to refer to the corresponding
literal-operator-id.  If from_cache is FALSE and a raw literal operator or
literal operator template is selected, a string literal containing the
portion of the current token preceding the ud-suffix is created and copied
to const_with_curr_tok_spelling.  If from_cache is TRUE, the lookup is
being performed while fetching a token from a token cache, and
const_for_curr_token and const_with_curr_tok_spelling are set up from the
information stored in the cache; if a raw literal operator or literal
operator template is selected, const_with_curr_tok_spelling is copied to
const_for_curr_token.
*/
{
  a_type_ptr              req_param1_type = NULL;
  a_boolean               is_string;
  a_boolean               allow_raw_and_template;
  a_symbol_ptr            orig_sym;
  a_symbol_ptr            raw_operator = NULL;
  a_boolean               ambiguous_raw_operator = FALSE;
  a_symbol_ptr            operator_template = NULL;
  a_boolean               ambiguous_operator_template = FALSE;
  a_symbol_ptr            matching_sym = NULL;
  a_boolean               ambiguous_matching_sym = FALSE;
  a_symbol_ptr            sym;
  a_symbol_ptr            list_sym;
  a_symbol_list_entry_ptr slep;
  a_symbol_list_entry_ptr operators = NULL;
  a_symbol_list_entry_ptr raw_and_template_operators = NULL;

  if (literal_type->kind == tk_array &&
      literal_type->variant.array.element_type->kind == tk_integer) {
    /* When string_literals_are_const is FALSE, the type of a string
       literal is "array of X" instead of "array of const X".  However, the
       first parameter of a user-defined string literal operator must be
       "pointer to const X", so the types will not match.  Adjust the
       type of the literal to the Standard-conforming version before
       looking for a matching literal operator. */
    a_type_ptr new_type = alloc_type(tk_array);
    check_assertion(!string_literals_are_const);
    new_type->variant.array.element_type =
       make_qualified_type(literal_type->variant.array.element_type, TQ_CONST);
    new_type->variant.array.variant.number_of_elements =
                        literal_type->variant.array.variant.number_of_elements;
    set_type_size(new_type);
    literal_type = new_type;
  }  /* if */
  if (!from_cache) {
    clear_constant(&const_with_curr_tok_spelling,
                   (a_constant_repr_kind)ck_error);
  }  /* if */
  /* Find the required first parameter type and other literal operator
     characteristics based on the type of the literal. */
  if (literal_type->kind == (a_type_kind)tk_integer) {
    /* An integral type, including character types. */
    is_string = FALSE;
    if (literal_type->variant.integer.int_kind == (an_integer_kind)ik_char ||
        literal_type->variant.integer.wchar_t_type ||
        literal_type->variant.integer.char8_t_type ||
        literal_type->variant.integer.char16_t_type ||
        literal_type->variant.integer.char32_t_type) {
      /* This is a character literal.  Raw literal operators and literal
         operator templates are not allowed, and the matching operator's
         parameter type is the same as that of the literal. */
      allow_raw_and_template = FALSE;
      req_param1_type = literal_type;
    } else {
      /* This is an integer literal.  Raw literal operators and literal
         operator templates are allowed, and the matching operator's
         parameter type is unsigned long long. */
      allow_raw_and_template = TRUE;
      req_param1_type = integer_type((an_integer_kind)ik_unsigned_long_long);
    }  /* if */
  } else if (literal_type->kind == (a_type_kind)tk_float) {
    /* This is a floating-point literal.  Raw literal operators and literal
       operator templates are allowed, and the matching operator's
       parameter type is long double. */
    is_string = FALSE;
    allow_raw_and_template = TRUE;
    req_param1_type = float_type((a_float_kind)fk_long_double);
  } else {
    /* This is a string literal.  Raw literal operators and literal
       operator templates are not allowed, and the matching operator's
       parameter type is a pointer to the literal's array element type. */
    check_assertion(literal_type->kind == (a_type_kind)tk_array);
    is_string = TRUE;
    allow_raw_and_template = FALSE;
    req_param1_type = make_pointer_type(array_element_type(literal_type));
  }  /* if */
  make_literal_opname_locator(name, name_len, &locator_for_curr_id, pos);
  if (caching_tokens &&
      (allow_raw_and_template || string_literal_operator_template_allowed ||
       (cpp20_mode && is_string)) &&
      !from_cache) {
    /* We may need the token spelling when we do the lookup of the cached
       token; if this token isn't already in a cache, save the token
       spelling in const_with_curr_tok_spelling so the value can be
       cached. */
    if (is_string) {
      /* const_for_curr_token contains the string value, but the
         current token pointer is not yet set (because concatenation
         of adjacent string literals is not yet complete).  Set the
         spelling from const_for_curr_token. */
      copy_constant(&const_for_curr_token, &const_with_curr_tok_spelling);
    } else {
      create_constant_from_token_spelling(&const_with_curr_tok_spelling);
    }  /* if */
  }  /* if */

  /* Look up the symbol(s) for the specified literal operator. */
  a_symbol_locator id_locator = locator_for_curr_id;
  orig_sym = normal_id_lookup(&id_locator, IDL_NO_OPTIONS);
  if (orig_sym != NULL) {
    if (size_t_type == NULL) {
      /* Initialize the special types used for parameter checking. */
      size_t_type = integer_type(targ_size_t_int_kind);
      ptr_to_const_char_type = make_pointer_type(
                  make_qualified_type(integer_type((an_integer_kind)ik_char),
                                      TQ_CONST));
    }  /* if */
    list_sym = symbol_is(orig_sym, sk_overloaded_function)
                              ? orig_sym->variant.overloaded_function.symbols
                              : orig_sym;
    do {
      /* Check the symbol for a match against the permitted operators. */
      sym = fundamental_symbol_of(list_sym);
      if (symbol_is(sym, sk_function_template)) {
        if (!allow_raw_and_template &&
            !string_literal_operator_template_allowed &&
            !(cpp20_mode && is_string)) {
          /* Ignore the symbol. */
        } else {
          /* This is a literal operator template, and the current literal
             is of a kind for which a literal operator template is a
             possible match.  Make a note of it and continue the scan. */
          a_template_symbol_supplement_ptr tssp = sym->variant.template_info;
          a_template_param_ptr             tpp = tssp->variant.function.
                                             decl_cache->decl_info->parameters;
          a_boolean                        is_string_lit_op_template = FALSE;

          check_assertion(tpp != NULL);
          if (cpp20_mode && tpp->next == NULL && !tpp->is_pack &&
              symbol_is(tpp->param_symbol, sk_constant) &&
              (is_class_struct_union_type(tpp->variant.constant.ptr->type) ||
               is_class_template_placeholder_type(
                                          tpp->variant.constant.ptr->type))) {
            /* C++20 permits string literal operator templates of the form:
                    template<String strval> operator""str();
               where String is a class type.
            */
            is_string_lit_op_template = TRUE;
          } else if (symbol_is(tpp->param_symbol, sk_type) &&
                     tpp->next != NULL &&
                     string_literal_operator_template_allowed) {
            /* Clang and GCC allow string literal operator templates of the
               form:
                    template<typename charT, charT ...chars> operator""str();
            */
            is_string_lit_op_template = TRUE;
          }  /* if */
          if (is_string != is_string_lit_op_template) {
            /* The template does not match the literal; ignore the symbol. */
          } else {
            if (operator_template != NULL) {
              /* We already saw a literal operator template.  Remember that
                 for possible later handling. */
              ambiguous_operator_template = TRUE;
            }  /* if */
            operator_template = sym;
            if (dp != NULL) {
              /* Record the symbol for later display, if needed. */
              slep = alloc_symbol_list_entry();
              slep->symbol = sym;
              slep->next = raw_and_template_operators;
              raw_and_template_operators = slep;
            }  /* if */
          }  /* if */
        }  /* if */
      } else if (symbol_is(sym, sk_routine)) {
        /* This is a function.  Get its parameter list and check it against
           the required parameter type(s). */
        a_routine_type_supplement_ptr rtsp;
        a_type_ptr                    param1_type;
        a_type_ptr                    param2_type;
        rtsp = sym->variant.routine.ptr->type->variant.routine.extra_info;
        if (rtsp->param_type_list == NULL) {
          /* A literal operator must have at least one parameter.  An error
             should already have been issued. */
          expect_error();
          continue;
        }  /* if */
        param1_type = skip_typerefs(rtsp->param_type_list->type);
        if (rtsp->param_type_list->next != NULL) {
          param2_type = skip_typerefs(rtsp->param_type_list->next->type);
          if (rtsp->param_type_list->next->next != NULL) {
            /* A literal operator cannot have more than two parameters.
               An error should already have been issued. */
            expect_error();
            continue;
          }  /* if */
        } else {
          param2_type = NULL;
        }  /* if */
        if (identical_types(param1_type, ptr_to_const_char_type) &&
            param2_type == NULL && allow_raw_and_template) {
          /* This is a raw literal operator, and the current literal is of
             a kind for which a raw literal operator is a possible match.
             Make a note of it and continue the scan. */
          if (raw_operator != NULL) {
            /* We already saw a raw literal operator.  Remember that for
               possible later handling. */
            ambiguous_raw_operator = TRUE;
          }  /* if */
          raw_operator = sym;
          if (dp != NULL) {
            /* Record the symbol for later display, if needed. */
            slep = alloc_symbol_list_entry();
            slep->symbol = sym;
            slep->next = raw_and_template_operators;
            raw_and_template_operators = slep;
          }  /* if */
        } else if (identical_types(req_param1_type, param1_type)) {
          /* The first parameter has the required type. */
          if (is_string && param2_type == NULL) {
            /* This is a raw literal operator, which can't be used for a
               string literal -- ignore it. */
          } else if ((is_string &&
                      !types_are_compatible(param2_type, size_t_type)) ||
                     (!is_string && param2_type != NULL)) {
            /* In the case of a string literal operator there should be a
               second parameter of type size_t; otherwise, there should not
               be another parameter.  If this is not the case, this
               candidate was declared erroneously and should be ignored.
               (Note the use of types_are_compatible instead of
               identical_types; this produces slightly better error
               recovery in the presence of error types.) */
            expect_error();
          } else {
            if (matching_sym != NULL) {
              /* We already saw a matching symbol. */
              ambiguous_matching_sym = TRUE;
              if (dp == NULL) {
                /* This is an error (and we are not producing diagnostics);
                   no need to keep scanning. */
                break;
              }  /* if */
            }  /* if */
            matching_sym = sym;
            if (dp != NULL) {
              /* Record the symbol for later display. */
              slep = alloc_symbol_list_entry();
              slep->symbol = sym;
              slep->next = operators;
              operators = slep;
            }  /* if */
          }  /* if */
        }  /* if */
      } else {
        /* Not a template and not a function.  This can result from
           erroneous declarations using literal operator ids.  Ignore the
           symbol. */
        expect_error();
      }  /* if */
    } while (symbol_is(orig_sym, sk_overloaded_function) &&
             (list_sym = list_sym->next) != NULL);
    if (ambiguous_matching_sym) {
      /* Return the original overloaded function symbol to indicate the
         ambiguity. */
      matching_sym = orig_sym;
    } else if (matching_sym == NULL) {
      /* See if there is exactly one literal operator template or raw
         literal operator in the set; if so, select that and convert
         const_for_curr_token to a string containing the spelling of the
         token. */
      a_boolean token_string_needed = FALSE;
      a_boolean ambiguous = FALSE;
      if (operator_template != NULL) {
        if (raw_operator != NULL || ambiguous_operator_template) {
          /* Return the original overloaded function symbol to indicate the
             ambiguity. */
          matching_sym = orig_sym;
          ambiguous = TRUE;
        } else {
          matching_sym = operator_template;
        }  /* if */
        token_string_needed = TRUE;
      } else if (raw_operator != NULL) {
        if (ambiguous_raw_operator) {
          /* Return the original overloaded function symbol to indicate the
             ambiguity. */
          matching_sym = orig_sym;
          ambiguous = TRUE;
        } else {
          matching_sym = raw_operator;
        }  /* if */
        token_string_needed = TRUE;
      }  /* if */
      if (token_string_needed) {
        if (from_cache) {
          if (!ambiguous) {
            if (const_with_curr_tok_spelling.kind ==
                                              (a_constant_repr_kind)ck_error) {
              /* The constant with the current token spelling is not set,
                 which occurs when we are instantiating a string literal
                 operator template.  Leave const_for_curr_token
                 untouched. */
            } else {
              /* The current token is being extracted from a cache, so both
                 const_for_curr_token and const_with_curr_tok_spelling are
                 valid.  We need the "raw" version for a raw literal operator
                 or literal operator template, so copy the token spelling
                 into const_for_curr_token. */
              copy_constant(&const_with_curr_tok_spelling,
                            &const_for_curr_token);
            }  /* if */
          } else {
            /* Leave const_for_curr_token unchanged in case of ambiguity. */
          }  /* if */
        } else if (!caching_tokens) {
          /* A regular token, neither being added to nor extracted from a
             cache.  Record the spelling of the token. */
          if (is_string) {
            /* const_for_curr_token contains the string value, but the
               current token pointer is not yet set (because concatenation
               of adjacent string literals is not yet complete).  Set the
               spelling from const_for_curr_token. */
            copy_constant(&const_for_curr_token,
                          &const_with_curr_tok_spelling);
          } else {
            create_constant_from_token_spelling(&const_with_curr_tok_spelling);
          }  /* if */
          if (!ambiguous) {
            /* We need to put the spelling of the current token into
               const_for_curr_token for a raw literal operator or literal
               operator template. */
            copy_constant(&const_with_curr_tok_spelling,
                          &const_for_curr_token);
          } else {
            /* There was an ambiguity detected.  In some cases, that can be
               resolved by SFINAE, so we have put the token spelling into
               const_with_curr_tok_spelling in case it is needed but leave
               const_for_curr_token unchanged. */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (dp != NULL) {
    /* There should have been an ambiguity of some kind.  Display
       "additional info" diagnostics for each symbol that contributed to
       the ambiguity. */
    check_assertion(orig_sym != NULL);
    if (ambiguous_matching_sym) {
      /* More than one literal operator matched the requirements. */
      for (slep = operators; slep != NULL; slep = slep->next) {
        sym_add_diag_info(dp, ec_ambiguous_function_add_on, slep->symbol);
      }  /* for */
    } else {
      check_assertion(raw_and_template_operators != NULL &&
                      raw_and_template_operators->next != NULL);
      for (slep = raw_and_template_operators; slep != NULL;
           slep = slep->next) {
        sym_add_diag_info(dp, ec_ambiguous_function_add_on, slep->symbol);
      }  /* for */
    }  /* if */
    free_list_of_symbol_list_entries(operators);
    free_list_of_symbol_list_entries(raw_and_template_operators);
  }  /* if */
  return matching_sym;
}  /* find_literal_operator */


void set_keyword_visibility(a_const_char     *keyword,
                            a_boolean        is_visible,
                            a_symbol_locator *loc)
/*
Retrieve the symbol for the given keyword (it must exist) and set its
visibility as indicated.  Also set *loc to point to the associated symbol
header.
*/
{
  a_symbol_header_ptr  sym_hdr = find_symbol_header(keyword,
                                                    (sizeof_t)strlen(keyword),
                                                    loc);
  a_symbol_ptr         sym = symbol_list_for_file_scope_symbols(sym_hdr);

  for (; sym != NULL; sym = sym->next) {
    if (symbol_is(sym, sk_keyword)) break;
  }  /* for */
  check_assertion(sym != NULL);
  sym->is_invisible = !is_visible;
}  /* set_keyword_visibility */

#if SUN_EXTENSIONS_ALLOWED

void ldscope_pragma(a_pending_pragma_ptr	ppp)
/*
This routine is called when a Sun CC pragma with one of the following forms
	#pragma enable_ldscope
	#pragma disable_ldscope
is called.  kind indicates which particular form was encountered.  It controls
whether __global, __symbolic, and __hidden are treated as keywords.  Note that
since these pragmas are automatically recorded in the IL, the tokens
"enable_ldscope" or "disable_ldscope" will already have been consumed.
*/
{
  a_boolean         keywords_visible = FALSE;
  a_pragma_kind     kind = ppp->descr_ptr->kind;
  a_symbol_locator  loc;

  switch (kind) {
    case pk_enable_ldscope:  keywords_visible = TRUE;  break;
    case pk_disable_ldscope: keywords_visible = FALSE; break;
    default:                 unexpected_condition();
  }  /* switch */
  set_keyword_visibility("__global", keywords_visible, &loc);
  set_keyword_visibility("__symbolic", keywords_visible, &loc);
  set_keyword_visibility("__hidden", keywords_visible, &loc);
}  /* ldscope_pragma */

#endif /* SUN_EXTENSIONS_ALLOWED */

static a_saved_macro_state_ptr alloc_saved_macro_state(void)
/*
Allocate a new saved macro state entry and return a pointer to it.
*/
{
  a_saved_macro_state_ptr smsp;

  if (avail_saved_macro_states != NULL) {
    /* Reuse an existing entry. */
    smsp = avail_saved_macro_states;
    avail_saved_macro_states = avail_saved_macro_states->next;
  } else {
    /* Allocate a new entry. */
    smsp = alloc_fe_of_type(a_saved_macro_state);
#if DEBUG
/* GCOVR_EXCL_START */
    num_saved_macro_states_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  }  /* if */
  smsp->next = NULL;
  smsp->symbol = NULL;
  clear_macro_def(&smsp->macro_def);
  return smsp;
}  /* alloc_saved_macro_state */


static void free_saved_macro_state(a_saved_macro_state_ptr smsp)
/*
Add "smsp" to the list of saved macro state entries available for reuse.
*/
{
  smsp->next = avail_saved_macro_states;
  avail_saved_macro_states = smsp;
}  /* free_saved_macro_state */


static a_symbol_header_ptr symbol_header_for_macro_push_or_pop(
					a_pending_pragma_ptr	ppp,
					a_source_position	*name_pos)
/*
This routine scans the tokens of a push_macro or pop_macro pragma,
looks up the symbol header for the identifier and returns it.
If an error occurs while scanning the pragma, NULL is returned.

The form of such a pragma is:

        #pragma {push,pop}_macro("identifier")

If a symbol header is returned, the position of the identifier string
is returned in name_pos.

Note that "ppp" can be NULL when this routine is called when doing
preprocessing only.  Also note that the tokens are scanned in fetch_pp_tokens
mode.
*/
{
  a_boolean		err = FALSE;
  a_symbol_header_ptr	sym_hdr = NULL;

  /* Record the balance of the current source line in the pragma text field
     of the pragma. */
  if (ppp != NULL) {
    ppp->pragma_text = copy_string_to_region(file_scope_region_number,
                                             start_of_curr_token);
  }  /* if */
  /* Bypass the pragma identifier. */
  (void)get_token();
  /* Scan the "(". */
  if (curr_token == tok_lparen) {
    (void)get_token();
  } else {
    pos_warning(ec_exp_lparen, &error_position);
    err = TRUE;
  }  /* if */
  add_stop_token(tok_rparen);
  /* Scan the string literal that specifies the identifier. */
  if (curr_token != tok_string_literal) {
    if (!err) {
      pos_warning(ec_exp_string_literal, &error_position);
      err = TRUE;
    }  /* if */
  } else if (*start_of_curr_token != '"') {
    /* A wide string is not allowed. */
    pos_warning(ec_wide_string_not_allowed, &error_position);
    err = TRUE;
  } else {
    unsigned long num_chars = 0;
    *name_pos = pos_curr_token;
    /* Rescan the characters of the string literal. */
    curr_char_loc = start_of_curr_token;
    /* Advance past the opening quote. */
    curr_char_loc++;
    /* Scan the characters that make up the string literal. */
    if (!accum_quoted_string(&num_chars, /*is_header_name=*/FALSE,
                            SCLK_ORDINARY_STRING_LITERAL, '"', NULL, -1,
                            start_of_curr_token)) {
      a_const_char	*err_char_pos;
      an_error_code	err_code;
      /* Convert the string literal into a string constant. */
      conv_string_literal(start_of_curr_token + 1, end_of_curr_token,
                          SCLK_ORDINARY_STRING_LITERAL, num_chars, &err_code,
                          &err_char_pos);
      /* Advance past the opening quote. */
      curr_char_loc++;
      if (err_code != ec_no_error) {
        /* An error occurred while converting the string.  Issue a warning. */
        a_source_position	err_source_pos;
        conv_line_loc_to_source_pos(err_char_pos, &err_source_pos);
        pos_warning(err_code, &err_source_pos);
      } else {
        /* Find the symbol header for the named identifier.  Note that
           there is no check to determine if the identifier name is valid. */
        a_symbol_locator	locator;
        clear_locator(&locator, &null_source_position);
        sym_hdr = find_symbol_header(
                      const_for_curr_token.variant.string.value,
                      (sizeof_t)const_for_curr_token.variant.string.length - 1,
                      &locator);
      }  /* if */
    }  /* if */
    (void)get_token();
  }  /* if */
  /* Scan the ")". */
  if (curr_token == tok_rparen) {
    (void)get_token();
  } else if (!err) {
    pos_warning(ec_exp_rparen, &error_position);
    err = TRUE;
  }  /* if */
  remove_stop_token(tok_rparen);
  /* If an error occurred, flush any tokens until we reach the end of line. */
  if (curr_token != tok_newline && err) flush_to_newline();
  return err ? (a_symbol_header_ptr)NULL : sym_hdr;
}  /* symbol_header_for_macro_push_or_pop */


void push_macro_pragma(a_pending_pragma_ptr	ppp)
/*
The pragma processing function called when a push_macro pragma is encountered.

The form of such a pragma is:

        #pragma push_macro("identifier")

When this pragma is used, the state of any macro named "identifier" is
recorded on a stack so that the state can be restored when a pop_macro
pragma is encountered.  Note that the macro is not actually undefined or
changed in any way by the push_macro.

Note that "ppp" can be NULL when this routine is called when doing
preprocessing only.
*/
{
  a_symbol_header_ptr	sym_hdr;
  a_source_position	name_pos;

  /* Scan the pragma and get the symbol header for the named identifier. */
  sym_hdr = symbol_header_for_macro_push_or_pop(ppp, &name_pos);
  if (sym_hdr != NULL) {
    a_saved_macro_state_ptr	smsp;
    a_symbol_ptr		symbol;
    smsp = alloc_saved_macro_state();
    /* Find the symbol for a currently defined macro of the specified name.
       If there is no such macro, NULL will be returned. */
    symbol = find_macro_symbol(sym_hdr);
    smsp->symbol = symbol;
    if (symbol != NULL) {
      /* If there is a currently defined macro, also save the macro definition
         entry.  This is needed if the symbol is redefined without being
         undefined. */
      smsp->macro_def = *symbol->variant.macro_def;
    }  /* if */
    /* Put this entry on the front of the stack of saved macros. */
    smsp->next = sym_hdr->saved_macro_stack;
    sym_hdr->saved_macro_stack = smsp;
  }  /* if */
}  /* push_macro_pragma */


void pop_macro_pragma(a_pending_pragma_ptr	ppp)
/*
The pragma processing function called when a pop_macro pragma is encountered.

The form of such a pragma is:

        #pragma pop_macro("identifier")

When this pragma is used, the state of any macro named "identifier" is
restored from the stack entry created by a push_macro pragma.

Note that "ppp" can be NULL when this routine is called when doing
preprocessing only.
*/
{
  a_symbol_header_ptr	sym_hdr;
  a_source_position	name_pos;

  /* Scan the pragma and get the symbol header for the named identifier. */
  sym_hdr = symbol_header_for_macro_push_or_pop(ppp, &name_pos);
  if (sym_hdr != NULL) {
    a_saved_macro_state_ptr	smsp = sym_hdr->saved_macro_stack;
    a_symbol_ptr		curr_macro_sym;
    if (smsp == NULL) {
      /* The was no prior push_macro for this name. */
      pos_st_warning(ec_no_prior_push_macro, &name_pos, sym_hdr->identifier);
    } else {
      /* Unlink the entry on the top of the stack. */
      sym_hdr->saved_macro_stack = smsp->next;
      /* Find the symbol for a currently defined macro of the specified name.
         If there is no such macro, NULL will be returned. */
      curr_macro_sym = find_macro_symbol(sym_hdr);
      if (curr_macro_sym != NULL && curr_macro_sym == smsp->symbol) {
        /* The correct macro symbol is present.  Reset its macro definition
           to the original value. */
        *curr_macro_sym->variant.macro_def = smsp->macro_def;
      } else {
        /* The macro symbol currently entered is not the desired one.
           Remove it. */
        if (curr_macro_sym != NULL) remove_symbol(curr_macro_sym);
        if (smsp->symbol != NULL) {
          /* There was a previous symbol.  Re-enter it. */
          reenter_symbol(smsp->symbol, (a_scope_depth)DEPTH_OF_FILE_SCOPE,
                         /*suppress_error=*/TRUE);
        }  /* if */
      }  /* if */
      free_saved_macro_state(smsp);
    }  /* if */
  }  /* if */
}  /* pop_macro_pragma */



static
a_hash_table_size select_hash_table_size(a_hash_table_size	num_of_entries)
/*
Select the hash table size to be used to hold "num_of_entries" hash
table entries.  Return the value to be used.
*/
{
  static constexpr a_hash_table_size sizes[] = {
	1,	5,
	11,	17,	29,	41,	59,	83,	127,	179,
	251,	353,	499,	701,	983,	1381,	1949,	2729,
	3821,	5351,	7499,	10499,	14699,	20593,	28837,	40387,
	56543,	79181,	110863,	155209,	217307,	304253,	425959,	596363,
	834913,		1168879,	1636457,	2291041,
	3207461,	4490459,	6286661,	8801327,
	12321863,	17250641,	24150901,	33811277,
	47335793,	66270121,	92778187,	129889477,
	181845299,	254583437,	356416861,	498983623,
	698577083,	978007931,	1369211111,	1916895569,
	0 /* Must be last entry */
  };
  unsigned int	i;

  /* Select a size that is greater than or equal to the number of elements. */
  for (i = 0; i < ((sizeof(sizes) / sizeof(a_hash_table_size)) - 1); i++) {
    if (num_of_entries <= sizes[i]) break;
  }  /* for */
  check_assertion(sizes[i] != 0);
  return sizes[i];
}  /* select_hash_table_size */


static a_hash_table_entry_ptr alloc_hash_table_entry(
					a_memory_region_number	memory_region)
/*
Allocate a new hash table entry, initialize its fields, and return a pointer
to it.  "memory_region" is the memory region in which the entry should be
allocated or NO_MEMORY_REGION_NUMBER if general memory should be used.
*/
{
  a_hash_table_entry_ptr	htep;

  htep = alloc_general_or_in_region_of_type(memory_region, a_hash_table_entry);
  htep->next = NULL;
  htep->data = NULL;
#if DEBUG
/* GCOVR_EXCL_START */
  num_hash_table_entries_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  return htep;
}  /* alloc_hash_table_entry */


a_hash_table_ptr alloc_hash_table(
			a_memory_region_number		memory_region,
			a_hash_table_size		num_elements,
			a_function_number		hash_function_index,
			a_function_number		compare_function_index)
/*
Allocate a hash table, initialize its fields, and return a pointer to
the table.  "memory_region" is the memory region in which the table and its
entries should be allocated or NO_MEMORY_REGION_NUMBER if general memory
should be used.  "num_elements" is the number of elements expected (i.e., the
number of elements on which the hash table size should be based).  This value
is rounded up to one of a set of prime values.  "hash_function_index" is the
index of a function to be used to produce a hash value from a key.
"compare_function_index" is the index of a function to be used to compare a key
value with an element of the table.
*/
{
  a_hash_table_ptr	htp;
  sizeof_t		table_size_in_bytes;
  a_hash_table_size	buckets;

  htp = alloc_general_or_in_region_of_type(memory_region, a_hash_table);
  htp->hash_function_index = hash_function_index;
  htp->compare_function_index = compare_function_index;
  htp->memory_region = memory_region;
  /* Select a table size based on the number of elements. */
  buckets = select_hash_table_size(num_elements);
  htp->num_buckets = buckets;
  htp->entry_count = 0;
  table_size_in_bytes = sizeof(a_hash_table_entry_ptr) * buckets;
  htp->table = (a_hash_table_entry_ptr*)
                alloc_general_or_in_region(memory_region, table_size_in_bytes);
  memzero((a_void_ptr)htp->table, size_t_arg(table_size_in_bytes));
#if DEBUG
/* GCOVR_EXCL_START */
  num_hash_tables_allocated++;
  total_hash_table_size += (unsigned long)table_size_in_bytes;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  return htp;
}  /* alloc_hash_table */


static void resize_hash_table(a_hash_table_ptr	htp)
/*
The hash table "htp" is not large enough for the number of entries it
holds.  Allocate a larger table and rehash the existing entries.
*/
{
  sizeof_t			table_size_in_bytes;
  sizeof_t			old_table_size_in_bytes;
  a_hash_table_size		new_buckets;
  a_hash_table_entry_ptr	*new_table;
  a_hash_table_size		bucket;

  /* Allocate a table large enough to handle four times as many entries as
     we currently have. */
  new_buckets = select_hash_table_size(htp->num_buckets * 4);
  table_size_in_bytes = sizeof(a_hash_table_entry_ptr) * new_buckets;
  new_table = (a_hash_table_entry_ptr*)
                               alloc_general_or_in_region(htp->memory_region,
                                                          table_size_in_bytes);
#if DEBUG
/* GCOVR_EXCL_START */
  if (db_flag_is_set("hash")) {
    fprintf(f_debug, "Resizing hash table at %p, old_size=%lu, new_size=%lu\n",
            (void*)htp, (unsigned long)htp->num_buckets,
            (unsigned long)new_buckets);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  memzero((a_void_ptr)new_table, size_t_arg(table_size_in_bytes));
  old_table_size_in_bytes = sizeof(a_hash_table_entry_ptr) *
                            htp->num_buckets;
  /* Go through the old table and rehash them into the new one. */
  for (bucket = 0; bucket < htp->num_buckets; bucket++) {
    a_hash_table_entry_ptr	htep;
    a_hash_table_entry_ptr	next_htep = NULL;
    a_hash_table_size		new_bucket;
    for (htep = htp->table[bucket]; htep != NULL;  htep = next_htep) {
      next_htep = htep->next;
      new_bucket = htep->hash_value % (a_hash_value)new_buckets;
      /* Link it at the start of the bucket. */
      htep->next = new_table[new_bucket];
      new_table[new_bucket] = htep;
    }  /* for */
  }  /* for */
  htp->num_buckets = new_buckets;
  if (htp->memory_region == NO_MEMORY_REGION_NUMBER)  {
    /* Tables in IL memory cannot be freed, so are just discarded. */
    free_general(htp->table, old_table_size_in_bytes);
  }  /* if */
  htp->table = new_table;
#if DEBUG
/* GCOVR_EXCL_START */
  total_hash_table_size += (unsigned long)(table_size_in_bytes -
                                           old_table_size_in_bytes);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
}  /* resize_hash_table */


#define HASH_LOAD_FACTOR	1.0
			/* The number of entries per bucket at which we should
			   reallocate the hash table. */


a_hash_data_ptr *hash_find(a_hash_table_ptr	table,
			   a_void_ptr		key,
			   a_boolean		create)
/*
Look for an entry that matches "key" in "table".  If the entry does not exist,
and "create" is TRUE, create an entry.  Return the address of the "data"
field of a_hash_table_entry, or NULL if no entry was found.  When a new entry
has been created by this routine (i.e., *return_value == NULL) the caller
must use the pointer returned to set the new hash table entry to refer to the
appropriate user-defined entry.
*/
{
  a_hash_table_size		bucket;
  a_hash_table_entry_ptr	htep;
  a_void_ptr			result;
  a_hash_value			hash_value;
  a_hash_function_ptr		hash_function;
  a_hash_compare_function_ptr	compare_function;

  /* Convert function indices into pointers. */
  hash_function = (a_hash_function_ptr)
                         index_to_function_pointer(table->hash_function_index);
  compare_function = (a_hash_compare_function_ptr)
                      index_to_function_pointer(table->compare_function_index);
  hash_value = hash_function(key);
  bucket = hash_value % (a_hash_value)table->num_buckets;
  /* Look for a matching entry in this bucket. */
  for (htep = table->table[bucket]; htep != NULL; htep = htep->next) {
    check_assertion(htep->data != NULL);
    if (htep->hash_value == hash_value && compare_function(htep->data, key)) {
      break;
    }  /* if */
  }  /* for */
  if (htep == NULL && create) {
    /* Increment the number of entries in the table. */
    table->entry_count++;
    if ((double)table->entry_count / (double)table->num_buckets >
                                                            HASH_LOAD_FACTOR) {
      /* The hash table is too small.  Reallocate it. */
      resize_hash_table(table);
      bucket = hash_value % (a_hash_value)table->num_buckets;
    }  /* if */
    /* No entry was found.  Create one now. */
    htep = alloc_hash_table_entry(table->memory_region);
    /* Link it at the start of the bucket. */
    htep->next = table->table[bucket];
    table->table[bucket] = htep;
    htep->hash_value = hash_value;
  }  /* if */
  /* If an entry was found or created, return the address of the pointer.
     This allows the caller to fill in the data pointer for a new entry. */
  result = htep == NULL ? (a_void_ptr)NULL : (a_void_ptr)&htep->data;
  return (a_hash_data_ptr*)result;
}  /* hash_find */

#if DEBUG
/* GCOVR_EXCL_START */

void db_hash_statistics(a_hash_table_ptr	table)
/*
Display statistics about a hash table.
*/
{
  int32_t		counts[32];
  a_hash_table_size	i;
  int			j;
  int			max_count_entry = 0;
  uint32_t		entries;

  fprintf(f_debug, "Total entries=%lu, buckets=%lu\n",
          (unsigned long)(long)table->entry_count,
          (unsigned long)table->num_buckets);
  /* Clear the table of entry counts. */
  for (j = 0; j < 32; j++) counts[j] = 0;
  /* Loop through the buckets and count the number of entries in each one. */
  for (i = 0; i < table->num_buckets; i++) {
    /* Count the entries in this bucket. */
    a_hash_table_entry_ptr	htep = table->table[i];
    entries = 0;
    for (; htep != NULL; htep = htep->next) entries++;
    /* The count array records the number of entries with at least
       (2**N)-1 entries. */
    for (j = 0; j < 31; j++, entries >>= 1) {
      if (entries == 0) break;
    }  /* for */
    counts[j]++;
    if (j > max_count_entry) max_count_entry = j;
  }  /* for */
  entries = 0;
  for (j = 0; j <= max_count_entry; j++) {
    fprintf(f_debug, "%5u: %lu\n", entries, (unsigned long)(long)counts[j]);
    entries = entries == 0 ? 1 : entries*2;
  }  /* for */
}  /* db_hash_statistics */

/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

a_hash_value hash_source_string(a_void_ptr  key)
/*
Produce a hash value for the given pointer (it points to a string of source
characters).
*/
{
  return (a_hash_value)hash_ptr(a_string_view((char*)key));
}  /* hash_source_string */


void namespace_has_no_actual_member_error(a_symbol_locator	*locator)
/*
Issue an error that the parent namespace from locator has no actual member
of the name specified by locator.  If the parent namespace is NULL, issue
a special version of the message that refers to the global namespace.
*/
{
  a_namespace_ptr	parent_namespace = qualifier_namespace_ptr(*locator);

  if (parent_namespace != NULL) {
    pos_stsy_error(ec_not_an_actual_member, &locator->source_position,
                   locator->symbol_header->identifier,
                   symbol_for(parent_namespace));
  } else {
    pos_st_error(ec_global_ns_has_no_actual_member,
                 &locator->source_position,
                 locator->symbol_header->identifier);
  }  /* if */
}  /* namespace_has_no_actual_member_error */

#if DEBUG
/* GCOVR_EXCL_START */

unsigned long show_symbol_space_used(void)
/*
Display and return the amount of memory used for symbol-related entries,
for space tracking purposes.
*/
{
  unsigned long num, size, total, grand_total = 0;

  db_space_used_header("Symbol table use:");

  db_space_used("symbol", num_symbols_allocated, a_symbol);
  db_space_used("symbol header", num_symbol_headers_allocated,
                a_symbol_header);
  db_space_used_general("scope stack", (unsigned long)size_scope_stack,
                        a_scope_stack_entry);
  db_space_used("conversion header", num_conversion_headers_allocated,
                a_conversion_header);
  db_space_used("literal operator header",
                num_literal_operator_headers_allocated,
                a_literal_operator_header);
  db_space_used("Name strings", symbol_name_string_space, char);
  db_space_used("symbol header lookup ents",
                num_symbol_header_lookup_entries_allocated,
                a_symbol_header_lookup_entry);
  db_space_used("extern symbol descr", num_extern_symbol_descrs_allocated,
                an_extern_symbol_descr);
  db_space_used("extern type fixup", num_extern_type_fixups_allocated,
                an_extern_type_fixup);
  db_space_used("field symbol supplement",
                num_field_symbol_supplements_allocated,
                a_field_symbol_supplement);
  db_space_used("static data member supplement",
                num_static_data_member_supplements_allocated,
                a_static_data_member_supplement);
  db_space_used("enum symbol supplement",
                num_enum_symbol_supplements_allocated,
                an_enum_symbol_supplement);
  db_space_used("class symbol supplement",
                num_class_symbol_supplements_allocated,
                a_class_symbol_supplement);
  db_space_used("namespace symbol suppl.",
                num_namespace_symbol_supplements_allocated,
                a_namespace_symbol_supplement);
  db_space_used("template symbol suppl.",
                num_template_symbol_supplements_allocated,
                a_template_symbol_supplement);
  db_space_used("template param", num_template_params_allocated,
                a_template_param);
  db_space_used_lost("param ids", avail_param_ids, num_param_ids_allocated,
                     a_param_id);
  db_space_used_lost("dependent type fixups", avail_dependent_type_fixups,
                     num_dependent_type_fixups_allocated,
                     a_dependent_type_fixup);
  db_space_used_lost("vla fixup", avail_vla_fixups, num_vla_fixups_allocated,
                     a_vla_fixup);
  db_space_used("template instance", num_template_instances_allocated,
                a_template_instance);
  db_space_used("master instance", num_master_instances_allocated,
                a_master_instance);
  db_space_used("symbol list entry", num_symbol_list_entries_allocated,
                a_symbol_list_entry);
  db_space_used("type list entry", num_type_list_entries_allocated,
                a_type_list_entry);
  db_space_used("subst. type list entry",
                num_substituted_type_list_entries_allocated,
                a_substituted_type_list_entry);
  db_space_used("template decl info", num_template_decl_info_allocated,
                a_template_decl_info);
  db_space_used("out of class partial spec",
                num_out_of_class_partial_specs_allocated,
                an_out_of_class_partial_spec);
  db_space_used("nondependent call info", num_nondependent_call_info_allocated,
                a_nondependent_call_info);
  db_space_used("token sequence xref",
                num_token_sequence_xrefs_allocated,
                a_token_sequence_xref);
  db_space_used("constexpr if cache info",
                num_constexpr_if_cache_info_allocated,
                a_constexpr_if_cache_info);
  db_space_used("templ friend def arg", num_templ_friend_info_allocated,
                a_templ_friend_info);
  db_space_used("namespace list entry", num_namespace_list_entries_allocated,
                a_namespace_list_entry);
  db_space_used("projection symbol descr", num_projection_descrs_allocated,
                a_projection_descr);
  db_space_used_lost("access error descr", avail_access_error_descrs,
                     num_access_error_descrs_allocated, an_access_error_descr);
  db_space_used_lost("active using directives", avail_active_using_directives,
                     num_active_using_directives_allocated,
                     an_active_using_directive);
  db_space_used("exception spec err descr",
                num_exception_spec_error_descrs_allocated,
                an_exception_spec_error_descr);
  db_space_used_general("generated entity blocks",
                        num_generated_entity_blocks_allocated,
                        a_generated_entity_block);
  db_space_used("hash table", num_hash_tables_allocated, a_hash_table);
  db_space_used("hash table entries", num_hash_table_entries_allocated,
                a_hash_table_entry);
  db_space_used_other("hash table size", total_hash_table_size, "");
  grand_total += total_hash_table_size;
  db_space_used("saved macro state", num_saved_macro_states_allocated,
                a_saved_macro_state);
#if MICROSOFT_EXTENSIONS_ALLOWED
  db_space_used("hide-by-sig list entries",
                num_hide_by_sig_list_entries_allocated,
                a_hide_by_sig_list_entry);
  db_space_used("property set sym. suppl.",
                num_property_set_symbol_supplements_allocated,
                a_property_set_symbol_supplement);
  db_space_used("C++/CLI accessor lookup",
                num_prop_or_event_accessor_header_lookups_allocated,
                a_prop_or_event_accessor_header_lookup);
  grand_total = db_show_ms_attrib_space_used(grand_total);
  db_space_used("ms attribute alternate name entries",
                 num_ms_attr_alt_name_entries_allocated,
                 an_ms_attr_alt_name_entry);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  grand_total = db_show_pch_space_used(grand_total);
  grand_total = db_show_scope_stack_space_used(grand_total);
  grand_total = db_show_template_space_used(grand_total);
  grand_total = db_show_routine_fixups_used(grand_total);
  grand_total = db_show_initializer_fixups_used(grand_total);
  grand_total = db_show_pending_exception_check_entries_used(grand_total);
#if IA64_ABI
  grand_total = db_show_covariant_overrides_used(grand_total);
#endif /* IA64_ABI */
  grand_total = db_show_class_fixups_used(grand_total);
  grand_total = db_show_override_registry_entries_used(grand_total);
#if MICROSOFT_EXTENSIONS_ALLOWED
  grand_total = db_show_quasi_override_descrs_used(grand_total);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  grand_total = db_show_def_arg_expr_fixups_used(grand_total);
  grand_total = db_show_il_c_fe_space_used(grand_total);
  grand_total = db_show_folding_fe_space_used(grand_total);
  grand_total = db_show_interpret_fe_space_used(grand_total);
  grand_total = db_show_trans_unit_space_used(grand_total);

  db_space_used_total();

  /* Print some symbol table performance statistics. */
  (void)fputc('\n', f_debug);
  db_space_used_other
                   ("Percent of buckets used",
                    (100 * num_used_symbol_buckets) / SYMBOL_TABLE_SIZE, "");

  if (num_used_symbol_buckets != 0) {
    db_space_used_float_other("Avg non-empty bucket len",
                             (double)num_symbol_headers_in_hash_table /
                             (double)num_used_symbol_buckets, "");
  }  /* if */
  db_space_used_other("Number of searches", num_searches_for_symbols, "");
  if (num_searches_for_symbols != 0) {
    db_space_used_float_other("Avg compares/search",
                             (double)num_compares_for_symbols /
                             (double)num_searches_for_symbols, "");
  }  /* if */
  db_space_used_other("Number of fast id lookups", num_fast_id_lookups, "");
  db_space_used_other("Number of slow id lookups", num_slow_id_lookups, "");

  return grand_total;
}  /* show_symbol_space_used */


/* GCOVR_EXCL_STOP */
#endif /* DEBUG */


void show_top_templates(unsigned  n)
/*
For every template recorded in inst_counters (which is populated only when the
--top_templates option is in effect), count the number of instances that were
created for it, and the number of those instances that are definitions.  The
templates are sorted by descending instance count and the top n are written,
with their counts, to the error output file.  When n is zero, every template
that has a nonzero instance count is reported.
*/
{
  unsigned  total = (unsigned)inst_counters->length();
  unsigned  limit = (n == 0) ? total : min_val(total, n);

  for (unsigned k = 0; k<total; ++k) {
    a_symbol_list_entry_ptr  slep = NULL;
    a_template_instance_ptr  tip = NULL;
    an_inst_count            &inst = (*inst_counters)[k];
    switch (inst.kind) {
      case sk_function_template:
      case sk_member_function:
        tip = inst.tssp->variant.function.instantiations;
        for (; tip != NULL; tip = tip->next) {
          ++inst.count;
          if (tip->instance_sym->defined) ++inst.defined;
        }  /* for */
        break;
      case sk_variable_template:
      case sk_static_data_member:
        slep = inst.tssp->variant.variable.instantiations;
        for (; slep != NULL; slep = slep->next) {
          ++inst.count;
          if (slep->symbol->defined) ++inst.defined;
        }  /* for */
        break;
      case sk_concept_template:
        break;
      default:
        slep = inst.tssp->variant.class_template.instantiations;
        for (; slep != NULL; slep = slep->next) {
          ++inst.count;
          if (slep->symbol->defined) ++inst.defined;
        }  /* for */
        break;
    }  /* switch */
  }  /* for */
  sort(inst_counters->begin(), inst_counters->end(),
       [](an_inst_count const &x, an_inst_count const &y) {
         return  x.count > y.count;
       });
  for (unsigned k = 0; k<limit; ++k) {
    an_inst_count  &inst = (*inst_counters)[k];
    a_template     *templ = inst.tssp->il_template_entry;
    if (inst.count == 0) break;
    if (templ == NULL) continue;
    /* The localized fragments supply the words surrounding the counts; the
       template name itself is source text and is not localized. */
    fprintf(f_error, "%6lu%s%6lu%s",
            inst.count, error_text(ec_top_templates_instances),
            inst.defined, error_text(ec_top_templates_defs_of));
    if (symbol_for(templ) != NULL) {
      an_il_to_str_output_control_block octl;
      clear_il_to_str_output_control_block(&octl);
      octl.output_str = put_str_to_temp_text_buffer_octl;
      pos_in_temp_text_buffer = 0;
      form_symbol_name(symbol_for(templ), &octl);
      put_ch_to_temp_text_buffer('\0');
      fprintf(f_error, "%s", temp_text_buffer);
    } else if (unmangled_name_of(&templ->source_corresp) != NULL) {
      fprintf(f_error, "%s",
              unmangled_name_of(&templ->source_corresp));
    } else {
      fprintf(f_error, "%s", error_text(ec_top_templates_unknown));
    }  /* if */
    if (templ->source_corresp.decl_position.seq > 0) {
      /* Append the declaration location so that templates with the same
         name (for instance overloaded function templates) can be told
         apart. */
      a_const_char  *file_name;
      a_const_char  *full_name;
      a_line_number line_number;
      a_boolean     at_end_of_source;
      (void)conv_seq_to_file_and_line(templ->source_corresp.decl_position.seq,
                                      &file_name, &full_name, &line_number,
                                      &at_end_of_source);
      if (!at_end_of_source) {
        /* The localized fragments supply the words; the file name is source
           text and is not localized. */
        fprintf(f_error, " (%s%lu%s%s)",
                error_text(ec_at_line), (unsigned long)line_number,
                error_text(ec_of), file_name);
      }  /* if */
    }  /* if */
    fprintf(f_error, "\n");
  }  /* for */
}  /* show_top_templates */


/*
A table of identifiers that are of interest to front end processing.  The
associated symbol table entries are flagged so other identifiers can avoid
special checks.  (Additional identifiers are flagged via other tables, such
as constexpr_intrinsic_descriptions and type_transform_names below).
*/
static constexpr a_const_char* intrinsic_names[] = {
  "main",
  "__is_pointer",   /* Type trait or identifier in clang or later GNU modes. */
  "__is_signed",    /* Type trait or identifier in clang mode. */
  "__is_invocable", /* Type trait or identifier in some GNU modes. */
  "__is_nothrow_invocable",
                    /* Type trait or identifier in some GNU modes. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  "safe_cast",
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  "allocator",
  "allocate",
  "deallocate",
  "val",
  "str",
  "nullptr"
};

#define N_INTRINSIC_NAMES \
   ((int)(sizeof(intrinsic_names)/sizeof(intrinsic_names[0])))


/*
A structure recording some characteristics of a constexpr function that the
front end recognizes for potential evaluation by the interpreter.  This is
the value type of a Ptr_map described below.
*/
struct a_constexpr_intrinsic_descr {
  a_constexpr_intrinsic
		kind;	/* The enumerator value identifying this function
			   for efficient dispatch in the interpreter. */
  a_symbol_ptr	*p_namespace_sym;
			/* A pointer to a (global) variable pointing to the
			   symbol representing the parent namespace of this
			   function. */
  a_const_char	*signature;
			/* A string describing some constraint on the type and
			   template arguments of this function (see
			   interpret.h for details of the string's format). */
};


/*
A table of entries describing characteristics of a constexpr function that the
front end recognizes for potential evaluation by the interpreter.  The table
is produced by expanding the macro NS_scope_constexpr_intrinsics (see
interpret.h).  The entry at index zero is a dummy entry that is never used
(it exists because index zero is used as an indication that a name is not
associated with any intrinsic).
*/
STATIC_THREAD struct {
  a_const_char	*name;	/* The name of a function known to the interpreter. */
  a_constexpr_intrinsic_descr
		descr;	/* Information characterizing the function beyond its
			   name. */
} constexpr_intrinsic_descriptions[] = {
  { NULL, {} },
#define CIT_descr(ns, name, signature) \
  { #name, { cit_##ns##_##name, &symbol_for_namespace_##ns, signature } },
  NS_scope_constexpr_intrinsics(CIT_descr)
#undef CIT_descr
};

#define N_CONSTEXPR_INTRINSIC_DESCRIPTIONS \
   ((int)(sizeof(constexpr_intrinsic_descriptions) \
                              /sizeof(constexpr_intrinsic_descriptions[0])))

using a_constexpr_intrinsic_descr_table =
		Ptr_map<a_symbol_header*, int>;
			/* The type of a table that maps intrinsic identifiers
			   to the index (in constexpr_intrinsic_descriptions)
			   of descriptions of functions that the interpreter
			   knows how to evaluate.  (Functions with names in the
			   table but which don't match the description will
			   not be handled specially by the interpreter.) */

STATIC_THREAD a_constexpr_intrinsic_descr_table
		*constexpr_intrinsic_descr_table;
			/* A map from symbol headers for names of functions
			   that the interpreter might know to descriptions
			   that decide whether an appropriately named function
			   should be handled intrinsically. */

static void init_constexpr_intrinsic_descriptions(void)
/*
Pre-enter symbol headers for some function names so they can efficiently be
recognized during parsing.  Also, record associated information in a Ptr_map
to efficiently dispatch evaluations of functions that can be handled
intrinsically.
*/
{
  int  n;

  constexpr_intrinsic_descr_table =
                          alloc_fe_of_type(a_constexpr_intrinsic_descr_table);
  construct(constexpr_intrinsic_descr_table, /*mask_width=*/8u);
  /* Note that we start at index 1 since index 0 is used as an indication
     that there is no corresponding intrinsic. */
  for (n = 1; n<N_CONSTEXPR_INTRINSIC_DESCRIPTIONS; ++n) {
    a_symbol_locator  loc;
    a_const_char      *name = constexpr_intrinsic_descriptions[n].name;
    (void)find_symbol(name, strlen(name), &loc);
    loc.symbol_header->has_intrinsic_name = TRUE;
    constexpr_intrinsic_descr_table->map(loc.symbol_header, n);
  }  /* for */
}  /* init_constexpr_intrinsic_descriptions */


static
a_const_char* check_constexpr_intrinsic_template_args(a_routine     *rp,
                                                      a_const_char  *sig)
/*
Match the template arguments (if any) of the given function to the string *sig
(see the description of NS_scope_constexpr_intrinsics in interpret.h for
details).  If successful, return a pointer to the '>' character that should be
present in the string.  If unsuccessful, the return value will point to a
character other than '>'.
*/
{
  a_template_arg  *tap = rp->template_arg_list;

  while (*sig != '>' && tap != NULL) {
    if (*sig == 'T') {
      /* A type argument is required next. */
      if (tap == NULL || tap->kind != tak_type) break;
      tap = tap->next;
      ++sig;
    }  /* if */
    if (*sig == ',') {
      /* More arguments are expected. */
      ++sig;
      if (*sig == '>') {
        /* A trailing comma means additional arguments are okay (but
           ignored). */
        tap = NULL;
      }  /* if */
    } else if (*sig != '>') {
      /* No more arguments are expected, but we didn't reach a '>'.  Something
         went wrong (i.e., the function doesn't match). */
      break;
    }  /* if */
  }  /* while */
  if (tap != NULL && *sig == '>') {
    /* There are more arguments (that are not ignored), but we reached a '>'.
       This is not a match: Back up one position so the caller knows it is
       not a match. */
    --sig;
  }  /*if */
  return sig;
}  /* check_constexpr_intrinsic_template_args */


static a_boolean is_std_meta_class_named(a_type_ptr    tp,
                                         a_const_char  *class_name)
/*
Return TRUE if the given type is the class type std::meta::<class_name>.
*/
{
  a_boolean  result = FALSE;

  tp = skip_typerefs(tp);
  if (is_immediate_class_type(tp) &&
      symbol_for_namespace_std_meta != NULL &&
      is_namespace_member(tp) &&
      parent_namespace_of(tp) ==
                  symbol_for_namespace_std_meta->variant.namespace_info.ptr) {
    a_const_char  *name = unmangled_name_of(&tp->source_corresp);
    if (strcmp(name, class_name) == 0) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* is_std_meta_class_named */


static a_boolean is_std_meta_access_context_type(a_type_ptr  tp)
/*
Return TRUE if the given type is the class type std::meta::access_context.
*/
{
  return is_std_meta_class_named(tp, "access_context");
}  /* is_std_meta_access_context_type */


static a_boolean is_std_meta_member_offset_type(a_type_ptr  tp)
/*
Return TRUE if the given type is the class type std::meta::member_offset.
*/
{
  return is_std_meta_class_named(tp, "member_offset");
}  /* is_std_meta_member_offset_type */


static a_type_ptr std_specialization_element_type(a_type_ptr    tp,
                                                  a_const_char  *name)
/*
Return the first template argument of tp when tp is a specialization of the
class template std::<name> whose first argument is a type, and NULL otherwise.
This identifies the specializations, such as std::vector<std::meta::info>, that
some of the constexpr intrinsics are declared to return.
*/
{
  a_type_ptr  elem_type = NULL;

  tp = skip_typerefs(tp);
  if (is_std_class(tp, name)) {
    a_template_arg_ptr  tap = class_type_supp(tp)->template_arg_list;
    if (tap != NULL && tap->kind == tak_type) {
      elem_type = skip_typerefs(tap->variant.type);
    }  /* if */
  }  /* if */
  return elem_type;
}  /* std_specialization_element_type */


static
a_boolean check_constexpr_intrinsic_type(a_type        *tp,
                                         a_const_char  **p_sig)
/*
Check whether type tp matches the "type code" that comes next in the string
pointed to by *p_sig (see the description of NS_scope_constexpr_intrinsics in
interpret.h for details about the type codes).  If successful, return TRUE and
set *p_sig one position past the end of the type code.  If unsuccessful,
return FALSE and leave *p_sig unchanged.
*/
{
  a_boolean     okay = TRUE;
  a_const_char  *sig = *p_sig;
  
more_components:
  switch (*sig) {
    case '.':
      ++sig;
      break;
    case 'b':
      if (!is_bool_type(tp)) okay = FALSE;
      ++sig;
      break;
    case 'r':
      if (!is_reflection_type(tp)) okay = FALSE;
      ++sig;
      break;
    case 'v':
      if (!is_void_type(tp)) okay = FALSE;
      ++sig;
      break;
    case 'C':
      if (!is_character_type(tp)) okay = FALSE;
      ++sig;
      break;
    case 'G':
      if (!is_general_character_type(tp)) okay = FALSE;
      ++sig;
      break;
    case 'I':
      if (!is_integral_type(tp)) okay = FALSE;
      ++sig;
      break;
    case 'S':
      if (sig[1] == 'v') {
        if (!check_consistent_string_view_type(tp)) okay = FALSE;
        sig += 2;
      } else if (sig[1] == 'z') {
        if (!is_size_t_type(tp)) okay = FALSE;
        sig += 2;
      } else if (sig[1] == 'u') {
        if (std_specialization_element_type(tp, "basic_string_view") !=
                                                         eff_char8_t_type()) {
          okay = FALSE;
        }  /* if */
        sig += 2;
      } else if (sig[1] == 'o') {
        if (!is_std_class(tp, "strong_ordering")) okay = FALSE;
        sig += 2;
      } else {
        unexpected_condition();
      }  /* if */
      break;
    case 'V':
      if (sig[1] == 'r') {
        a_type_ptr  elem_type = std_specialization_element_type(tp, "vector");
        if (elem_type == NULL || !is_reflection_type(elem_type)) okay = FALSE;
        sig += 2;
      } else {
        unexpected_condition();
      }  /* if */
      break;
    case 'L':
      if (!is_std_class(tp, "source_location")) okay = FALSE;
      ++sig;
      break;
    case 'A':
      if (!is_std_meta_access_context_type(tp)) okay = FALSE;
      ++sig;
      break;
    case 'M':
      if (sig[1] == 'o') {
        if (!is_std_meta_member_offset_type(tp)) okay = FALSE;
        sig += 2;
      } else {
        unexpected_condition();
      }  /* if */
      break;
    case '*':
      if (is_pointer_type(tp)) {
        ++sig;
        tp = type_pointed_to(tp);
        goto more_components;
      } else {
        okay = FALSE;
      }  /* if */
      break;
    case '&':
      if (is_reference_type(tp)) {
        ++sig;
        tp = type_pointed_to(tp);
        goto more_components;
      } else {
        okay = FALSE;
      }  /* if */
      break;
    default:
      unexpected_condition();
  }  /* switch */
  if (okay) *p_sig = sig;
  return okay;
}  /* check_constexpr_intrinsic_type */


static
a_const_char* check_constexpr_intrinsic_params(a_type        *rtp,
                                               a_const_char  *sig)
/*
Match the parameter (if any) of the given function type to the string *sig
(see the description of NS_scope_constexpr_intrinsics in interpret.h for
details).  If successful, return a pointer to the ')' character that should be
present in the string.  If unsuccessful, the return value will point to a
character other than ')'.
*/
{
  a_param_type_ptr  ptp = function_type_params(rtp);

  while (*sig != ')' && ptp != NULL) {
    if (!check_constexpr_intrinsic_type(ptp->type, &sig)) {
      break;
    }  /* if */
    ptp = ptp->next;
    if (*sig == ',') {
      /* More parameters are expected. */
      ++sig;
      if (*sig == ')') {
        /* A trailing comma means additional parameters are okay (but
           ignored). */
        ptp = NULL;
      }  /* if */
    } else if (*sig != ')') {
      /* No more arguments are expected, but we didn't reach a ')'.  Something
         went wrong (i.e., the function type doesn't match). */
      break;
    }  /* if */
  }  /* while */
  if (ptp != NULL && *sig == ')') {
    /* There are more parameters (that are not ignored), but we reached a ')'.
       This is not a match: Back up one position so the caller knows it is not
       a match. */
    --sig;
  }  /* if */
  return sig;
}  /* check_constexpr_intrinsic_params */


static bool matches_constexpr_intrinsic_sig(a_routine     *rp,
                                            a_const_char  *sig)
/*
Return TRUE if rp matches the characteristics described by the given string.
(See the description of NS_scope_constexpr_intrinsics in interpret.h for
details about the format of this string.)  Otherwise, return FALSE.
*/
{
  a_boolean  result = TRUE;

  for (;;) {
    a_type_ptr  rtp;
    if (*sig == '<') {
      /* Check template argument constraints. */
      sig = check_constexpr_intrinsic_template_args(rp, sig+1);
      if (*sig != '>') goto failed;
      ++sig;
    }  /* if */
    rtp = skip_typerefs(rp->type);
    if (*sig == '(') {
      /* Check function parameter constraints. */
      sig = check_constexpr_intrinsic_params(rtp, sig+1);
      if (*sig != ')') goto failed;
      ++sig;
    }  /* if */
    /* Check the return type. */
    if (!check_constexpr_intrinsic_type(rtp->variant.routine.return_type,
                                        &sig)) {
      goto failed;
    }  /* if */
    if (*sig == '\0' || *sig == '|') {
      /* Success. */
      break;
    }  /* if */
failed:
    /* Skip any remaining characters looking for '|' (meaning another signature
       is next) or '\0' (meaning we are done). */
    while (*sig != '\0' && *sig != '|') ++sig;
    if (*sig == '\0') {
      result = FALSE;
      break;
    }  /* if */
    /* Skip over the '|': */
    ++sig;
  }  /* for */
  return result;
}  /* matches_constexpr_intrinsic_sig */


static a_boolean is_member_of_class_in_namespace(a_routine_ptr  rp,
                                                 a_symbol_ptr   ns_sym)
/*
Return TRUE if rp is a member of a class that is itself a member of the
namespace indicated by ns_sym.
*/
{
  a_boolean  result = FALSE;

  if (ns_sym != NULL && rp->source_corresp.is_class_member) {
    a_type_ptr  c = parent_class_of(rp);
    if (is_namespace_member(c) &&
        parent_namespace_of(c) == ns_sym->variant.namespace_info.ptr) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* is_member_of_class_in_namespace */


void check_for_constexpr_intrinsic(a_routine_ptr    rp,
                                   a_symbol_header  *sym_hdr)
/*
The given routine is being declared with the given symbol header and that
header is associated with an intrinsic.  Check whether the routine is actually
a "constexpr intrinsic" (i.e., a function handled specially by the constexpr
interpreter) and if so mark it as such.
*/
{
  int  n = constexpr_intrinsic_descr_table->get(sym_hdr);

  if (n != 0) {
    a_constexpr_intrinsic_descr  *descr;
    descr = &constexpr_intrinsic_descriptions[n].descr;
    check_assertion(descr->kind != cit_error);
    if (*descr->p_namespace_sym != NULL &&
        ((is_namespace_member(rp) &&
          is_member_of_namespace(symbol_for(rp), *descr->p_namespace_sym)) ||
         is_member_of_class_in_namespace(rp, *descr->p_namespace_sym))) {
      if (matches_constexpr_intrinsic_sig(rp, descr->signature)) {
        register_constexpr_intrinsic(descr->kind, rp);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* check_for_constexpr_intrinsic */


/*
A structure identifying a particular alias or variable template that the front
end might encounter and handle intrinsically.
*/
struct a_templ_intrinsic_descr {
  a_const_char	*name;	/* The name of a template known to the front end. */
  a_symbol_ptr	*p_namespace_sym;
			/* A pointer to a (global) variable pointing to the
			   symbol representing the parent namespace of this
			   template.  This uses an additional level of
			   indirection because we cannot statically initialize
			   namespace pointers (because namespace symbols are
			   created after front end startup). */
};

STATIC_THREAD a_templ_intrinsic_descr
		alias_templ_intrinsic_descriptions[] = {
  { NULL, NULL },
#define ATI_descr(ns, name) \
  { #name, &symbol_for_namespace_##ns },
  NS_alias_templ_intrinsics(ATI_descr)
#undef ATI_DESCR
};

#define N_ALIAS_TEMPL_INTRINSIC_DESCRIPTIONS \
   ((int)(sizeof(alias_templ_intrinsic_descriptions) \
                              /sizeof(alias_templ_intrinsic_descriptions[0])))

STATIC_THREAD a_templ_intrinsic_descr
		var_templ_intrinsic_descriptions[] = {
  { NULL, NULL },
#define VTI_descr(ns, name) \
  { #name, &symbol_for_namespace_##ns },
  NS_var_templ_intrinsics(ATI_descr)
#undef VTI_DESCR
};

#define N_VAR_TEMPL_INTRINSIC_DESCRIPTIONS \
   ((int)(sizeof(var_templ_intrinsic_descriptions) \
                              /sizeof(var_templ_intrinsic_descriptions[0])))

/*
A structure describing a particular identifier in a specific class or namespace
scope.
*/
struct a_scoped_identifier {
  a_symbol_header
		*name;
			/* The symbol header corresponding to this
			   identifier. */
  a_symbol_ptr
		scope;
			/* The entity (class or namespace) that directly owns
			   the scope in which the name belongs. */
};


static inline a_boolean operator==(a_scoped_identifier  x,
                                   a_scoped_identifier  y)
/*
Return TRUE if the given scoped identifiers are identical.
*/
{
  return x.name == y.name && x.scope == y.scope;
}  /* operator== */


static inline a_boolean operator!=(a_scoped_identifier  x,
                                   a_scoped_identifier  y)
/*
Return TRUE if the given scoped identifiers are different.
*/
{
  return !(x == y);
}  /* operator!= */


static inline uintptr_t hash_ptr(a_scoped_identifier  sn)
/*
Return a hash value for the given scoped identifier.
*/
{
  uintptr_t  result = 17*31 + hash_ptr((void*)sn.name);
  result = result*31 + hash_ptr((void*)sn.scope);
  return result;
}  /* hash_ptr */


using a_templ_intrinsic_descr_table =
		Ptr_map<a_scoped_identifier, int>;
			/* The type of a table that maps intrinsic identifiers
			   to the descriptions of templates that the front end
			   knows how to instantiate. */

STATIC_THREAD a_templ_intrinsic_descr_table
		*alias_templ_intrinsic_descr_table;
			/* A map from scoped identifiers denoting alias
			   templates to descriptions identifying those
			   templates and their intrinsic treatment in the
			   front end. */

void init_alias_templ_intrinsic_descriptions(void)
/*
Pre-enter headers for some template names so they can efficiently be
recognized during parsing.  Also, record associated information in a Ptr_map
to efficiently dispatch substitutions that can be handled intrinsically.
*/
{
  int  n;

  alias_templ_intrinsic_descr_table =
                              alloc_fe_of_type(a_templ_intrinsic_descr_table);
  if (alias_templ_intrinsics_enabled) {
    construct(alias_templ_intrinsic_descr_table, /*mask_width=*/8u);
    /* Note that we start at index 1 since index 0 is used as an indication
       that there is no corresponding intrinsic. */
    for (n = 1; n<N_ALIAS_TEMPL_INTRINSIC_DESCRIPTIONS; ++n) {
      a_symbol_locator  loc;
      a_templ_intrinsic_descr
                        &descr = alias_templ_intrinsic_descriptions[n];
      (void)find_symbol(descr.name, strlen(descr.name), &loc);
      loc.symbol_header->has_intrinsic_name = TRUE;
      alias_templ_intrinsic_descr_table->map(
         a_scoped_identifier{ loc.symbol_header, *descr.p_namespace_sym }, n);
    }  /* for */
  } else {
    construct(alias_templ_intrinsic_descr_table, /*mask_width=*/1u);
  }  /* if */
}  /* init_alias_templ_intrinsic_descriptions */


int get_intrinsic_alias_templ_idx(a_symbol  *t_sym)
/*
If the given template is an alias template that the front end should treat
intrinsically, return an index identifying that alias template (the index
corresponds to an enumerator of type an_alias_templ_intrinsic).
*/
{
  int  idx = 0;

  if (!t_sym->is_class_member && t_sym->parent.namespace_ptr != NULL) {
    a_symbol_ptr  ns_sym = symbol_for(t_sym->parent.namespace_ptr);
    idx = alias_templ_intrinsic_descr_table->get(
                                a_scoped_identifier{ t_sym->header, ns_sym });
  }  /* if */
  return idx;
}  /* get_intrinsic_alias_templ_idx */


STATIC_THREAD a_templ_intrinsic_descr_table
		*var_templ_intrinsic_descr_table;
			/* A map from scoped identifiers denoting variable
			   templates to descriptions identifying those
			   templates and their intrinsic treatment in the
			   front end. */

void init_var_templ_intrinsic_descriptions(void)
/*
Pre-enter headers for some template names so they can efficiently be
recognized during parsing.  Also, record associated information in a Ptr_map
to efficiently dispatch substitutions that can be handled intrinsically.
*/
{
  int  n;

  var_templ_intrinsic_descr_table =
                              alloc_fe_of_type(a_templ_intrinsic_descr_table);
  if (var_templ_intrinsics_enabled) {
    construct(var_templ_intrinsic_descr_table, /*mask_width=*/8u);
    /* Note that we start at index 1 since index 0 is used as an indication
       that there is no corresponding intrinsic. */
    for (n = 1; n<N_VAR_TEMPL_INTRINSIC_DESCRIPTIONS; ++n) {
      a_symbol_locator  loc;
      a_templ_intrinsic_descr
                        &descr = var_templ_intrinsic_descriptions[n];
      (void)find_symbol(descr.name, strlen(descr.name), &loc);
      loc.symbol_header->has_intrinsic_name = TRUE;
      var_templ_intrinsic_descr_table->map(
         a_scoped_identifier{ loc.symbol_header, *descr.p_namespace_sym }, n);
    }  /* for */
  } else {
    construct(var_templ_intrinsic_descr_table, /*mask_width=*/1u);
  }  /* if */
}  /* init_var_templ_intrinsic_descriptions */


int get_intrinsic_var_templ_idx(a_symbol  *t_sym)
/*
If the given template is a variable template that the front end should treat
intrinsically, return an index identifying that variable template (the index
corresponds to an enumerator of type a_var_templ_intrinsic).
*/
{
  int  idx = 0;

  if (!t_sym->is_class_member && t_sym->parent.namespace_ptr != NULL) {
    a_symbol_ptr  ns_sym = symbol_for(t_sym->parent.namespace_ptr);
    idx = var_templ_intrinsic_descr_table->get(
                                a_scoped_identifier{ t_sym->header, ns_sym });
  }  /* if */
  return idx;
}  /* get_intrinsic_var_templ_idx */


/*
A structure describing a class template (e.g., std::remove_cv) whose use in
the form xyz<A...>::member the front end resolves intrinsically (without
completing xyz<A...>).
*/
struct a_templ_type_member_intrinsic_descr {
  a_const_char	*name;	/* The class template name. */
  a_symbol_ptr	*p_namespace_sym;
			/* See a_templ_intrinsic_descr. */
  a_const_char	*member_name;
			/* The member accessed via "::" (e.g., "type"). */
};

STATIC_THREAD a_templ_type_member_intrinsic_descr
		templ_type_member_intrinsic_descriptions[] = {
  { NULL, NULL, NULL },
#define TTMI_descr(ns, name, member) \
  { #name, &symbol_for_namespace_##ns, #member },
  NS_templ_type_member_intrinsics(TTMI_descr)
#undef TTMI_descr
};

#define N_TEMPL_TYPE_MEMBER_INTRINSIC_DESCRIPTIONS \
   ((int)(sizeof(templ_type_member_intrinsic_descriptions) \
                        /sizeof(templ_type_member_intrinsic_descriptions[0])))

STATIC_THREAD a_templ_intrinsic_descr_table
		*templ_type_member_intrinsic_descr_table;
			/* A map from scoped identifiers denoting class
			   templates to the index of their type-template-
			   member intrinsic treatment. */

STATIC_THREAD a_symbol_header
		*templ_type_member_intrinsic_member_hdrs[
                                  N_TEMPL_TYPE_MEMBER_INTRINSIC_DESCRIPTIONS];
			/* For each intrinsic index, the symbol header of the
			   member name (e.g., "type") whose access triggers the
			   intrinsic. */

void init_templ_type_member_intrinsic_descriptions(void)
/*
Pre-enter headers for the class template names so they can efficiently be
recognized during parsing.  Record, in a Ptr_map, the information needed to
dispatch xyz<A...>::member resolutions handled intrinsically, and cache the
header of each member name for matching.
*/
{
  int  n;

  templ_type_member_intrinsic_descr_table =
                              alloc_fe_of_type(a_templ_intrinsic_descr_table);
  if (templ_type_member_intrinsics_enabled) {
    construct(templ_type_member_intrinsic_descr_table, /*mask_width=*/8u);
    /* Note that we start at index 1 since index 0 is used as an indication
       that there is no corresponding intrinsic. */
    for (n = 1; n<N_TEMPL_TYPE_MEMBER_INTRINSIC_DESCRIPTIONS; ++n) {
      a_symbol_locator  loc, member_loc;
      a_templ_type_member_intrinsic_descr
                        &descr = templ_type_member_intrinsic_descriptions[n];
      (void)find_symbol(descr.name, strlen(descr.name), &loc);
      loc.symbol_header->has_intrinsic_name = TRUE;
      templ_type_member_intrinsic_descr_table->map(
         a_scoped_identifier{ loc.symbol_header, *descr.p_namespace_sym }, n);
      (void)find_symbol(descr.member_name, strlen(descr.member_name),
                        &member_loc);
      templ_type_member_intrinsic_member_hdrs[n] = member_loc.symbol_header;
    }  /* for */
  } else {
    construct(templ_type_member_intrinsic_descr_table, /*mask_width=*/1u);
  }  /* if */
}  /* init_templ_type_member_intrinsic_descriptions */


int get_intrinsic_templ_type_member_idx(a_symbol  *t_sym)
/*
If t_sym is a class template that the front end resolves intrinsically when
used in the form t_sym<A...>::member, return an index identifying that template
(corresponding to an enumerator of a_templ_type_member_intrinsic).  Otherwise,
return 0.
*/
{
  int  idx = 0;

  if (!t_sym->is_class_member && t_sym->parent.namespace_ptr != NULL) {
    a_symbol_ptr  ns_sym = symbol_for(t_sym->parent.namespace_ptr);
    idx = templ_type_member_intrinsic_descr_table->get(
                                a_scoped_identifier{ t_sym->header, ns_sym });
  }  /* if */
  return idx;
}  /* get_intrinsic_templ_type_member_idx */


a_boolean intrinsic_templ_type_member_matches(int              idx,
                                              a_symbol_header  *member_hdr)
/*
Return TRUE if member_hdr is the member name associated with the type-template-
member intrinsic identified by idx (e.g., "type" for std::remove_cv).
*/
{
  return idx > 0 && idx < N_TEMPL_TYPE_MEMBER_INTRINSIC_DESCRIPTIONS &&
         templ_type_member_intrinsic_member_hdrs[idx] == member_hdr;
}  /* intrinsic_templ_type_member_matches */


a_boolean intrinsic_templ_type_member_lookup(
                                       a_type_ptr       qualifier_type,
                                       a_symbol_header  *member_hdr,
                                       a_type_ptr       *result_tp,
                                       a_boolean        *no_such_member)
/*
Determine whether qualifier_type is an instance of a class template whose use
in the form qualifier_type::member_hdr is to be resolved intrinsically (see
init_templ_type_member_intrinsic_descriptions).  If so, return TRUE; set
*no_such_member to TRUE if the member provably does not exist (e.g.,
std::enable_if<false,T>::type), and otherwise set *no_such_member to FALSE and
*result_tp to the resolved member type.  If qualifier_type is not such an
instance, or the member cannot be decided intrinsically, return FALSE so the
caller falls back to ordinary processing.  The class qualifier_type is never
completed.
*/
{
  a_boolean  applicable = FALSE;

  *no_such_member = FALSE;
  if (templ_type_member_intrinsics_enabled &&
      is_immediate_class_type(qualifier_type) &&
      !is_template_dependent_type(qualifier_type)) {
    a_symbol_ptr  template_sym = class_template_for_type(qualifier_type);
    if (template_sym != NULL) {
      template_sym = primary_template_if_template_symbol(template_sym);
      /* The has_intrinsic_name flag, set on the headers of the class template
         names of interest by the init routine for this table, is a cheap way
         to skip the descriptor-table lookup for the common case of a template
         that is not handled intrinsically. */
      if (template_sym->header->has_intrinsic_name) {
        int  idx = get_intrinsic_templ_type_member_idx(template_sym);
        if (idx != 0 &&
            intrinsic_templ_type_member_matches(idx, member_hdr)) {
          a_template_arg_ptr  t_args =
                    template_arg_list_for_symbol(symbol_for(qualifier_type));
          a_templ_type_member_result  r =
                  eval_intrinsic_templ_type_member(idx, t_args, result_tp);
          switch (r) {
            case ttmr_resolved:
              applicable = TRUE;
              break;
            case ttmr_no_such_member:
              applicable = TRUE;
              *no_such_member = TRUE;
              break;
            case ttmr_not_applicable:
              break;
            default_is_unexpected();
          }  /* switch */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return applicable;
}  /* intrinsic_templ_type_member_lookup */


/*
Like intrinsic_names, these are names of interest to the front end, but they
should also be associated with token kinds (for context-sensitive promotion to
keywords).
*/
static constexpr struct {
  a_token_kind	token_kind;
			/* Token representation of the name. */ 
  a_const_char	*str;
			/* String representation of the name. */
} type_transform_names[] = {
  { tok_add_lvalue_reference , "__add_lvalue_reference" }, 
  { tok_add_pointer , "__add_pointer" },
  { tok_add_rvalue_reference , "__add_rvalue_reference" },
  { tok_decay , "__decay" },
  { tok_make_signed , "__make_signed" },
  { tok_make_unsigned , "__make_unsigned" },
  { tok_remove_all_extents , "__remove_all_extents" },
  { tok_remove_const , "__remove_const" },
  { tok_remove_cv , "__remove_cv" },
  { tok_remove_cvref , "__remove_cvref" },
  { tok_remove_extent , "__remove_extent" },
  { tok_remove_pointer , "__remove_pointer" },
  { tok_remove_reference , "__remove_reference" },
  { tok_remove_reference_t , "__remove_reference_t" },
  { tok_remove_restrict , "__remove_restrict" },
  { tok_remove_volatile , "__remove_volatile" }
};

#define N_TYPE_TRANSFORM_NAMES \
   ((int)(sizeof(type_transform_names)/sizeof(type_transform_names[0])))

using a_type_transform_name_table = Ptr_map<a_symbol_header*, a_token_kind>;
			/* The type of a table that maps intrinsic identifiers
			   to corresponding token kinds. */

STATIC_THREAD a_type_transform_name_table
		*type_transform_name_table;
			/* A map from symbol headers for type transform
			   operators to the keyword the associated identifier
			   promotes to in some contexts. */


static void init_type_transform_names(void)
/*
Pre-enter symbol headers for type transform names so they can efficiently be
recognized during parsing.  Also, record the associated keyword kinds.
*/
{
  int  n;

  type_transform_name_table = alloc_fe_of_type(a_type_transform_name_table);
  construct(type_transform_name_table, /*mask_width=*/6u);
  for (n = 0; n<N_TYPE_TRANSFORM_NAMES; ++n) {
    a_symbol_locator  loc;
    a_const_char      *name = type_transform_names[n].str;
    a_token_kind      kind = type_transform_names[n].token_kind;
    (void)find_symbol(name, strlen(name), &loc);
    loc.symbol_header->has_intrinsic_name = TRUE;
    type_transform_name_table->map(loc.symbol_header, kind);
  }  /* for */
}  /* init_type_transform_names */


a_boolean is_intrinsic_type_transform_name(a_symbol_header  *hdr)
/*
Return TRUE if the given symbol header corresponds to the name of a type
transform intrinsic (like __add_pointer).
*/
{
  return type_transform_name_table->get(hdr) != tok_error;
}  /* is_intrinsic_type_transform_name */


a_token_kind check_type_transform_name(void)
/*
If the current token is an identifier matching a type transform name, promote
the identifier to a keyword and return the associated token kind.  Otherwise,
return tok_error.
*/
{
  a_token_kind  result;

  if (curr_token == tok_identifier) {
    a_symbol_header  *hdr = locator_for_curr_id.symbol_header;
    result = type_transform_name_table->get(hdr);
    if (result != tok_error) {
      if (!check_context_sensitive_keyword(result, hdr->identifier)) {
        /* By construction, the check should have succeeded. */
        unexpected_condition();
      }  /* if */
    }  /* if */
  } else {
    result = tok_error;
  }  /* if */
  return result;
}  /* check_type_transform_name */


static void init_intrinsic_symbol_headers(void)
/*
Pre-enter symbol headers for intrinsic names so they can efficiently be
recognized during parsing.
*/
{
  int  n;

  for (n = 0; n<N_INTRINSIC_NAMES; ++n) {
    a_symbol_locator  loc;
    a_const_char      *name = intrinsic_names[n];
    (void)find_symbol(name, strlen(name), &loc);
    loc.symbol_header->has_intrinsic_name = TRUE;
  }  /* for */
  init_constexpr_intrinsic_descriptions();
  init_type_transform_names();
}  /* init_intrinsic_symbol_headers */


void symbol_tbl_one_time_init(void)
/*
Do one-time initialization of variables related to the symbol table.
(Variables that need to be reinitialized with each new translation unit
are handled in symbol_tbl_init.)
*/
{
  a_name_space_kind tag_name_space;
  a_name_space_kind member_name_space;

  /* Variables in symbol_tbl.h: */
  /* Build the table that maps symbol kinds to the corresponding name
     space.  See standard, 3.1.2.3. */
  name_space_for_symbol_kind[(int)sk_keyword]             = nsk_keyword;
  name_space_for_symbol_kind[(int)sk_macro]               = nsk_macro;
  name_space_for_symbol_kind[(int)sk_constant]            = nsk_other;
  name_space_for_symbol_kind[(int)sk_type]                = nsk_other;
  /* In C++, tags (class/struct/union) are in (almost) the same name space as
     normal symbols.  In C, they are in a separate name space. */
  tag_name_space = (C_dialect == C_dialect_cplusplus) ? nsk_other : nsk_tag;
  name_space_for_symbol_kind[(int)sk_class_or_struct_tag] = tag_name_space;
  name_space_for_symbol_kind[(int)sk_union_tag]           = tag_name_space;
  name_space_for_symbol_kind[(int)sk_enum_tag]            = tag_name_space;
  name_space_for_symbol_kind[(int)sk_variable]            = nsk_other;
  /* Note that in C++ mode the class members are nsk_other rather than some
     other kind, which works because the members are on the inactive list
     once the class definition is ended.  Therefore, they won't be
     found inadvertently.  In C mode they are in a separate nsk_member
     name space to prevent them from being found while they are still on
     the active list. */
  member_name_space = C_mode() ? nsk_member : nsk_other;
  name_space_for_symbol_kind[(int)sk_field]               = member_name_space;
  name_space_for_symbol_kind[(int)sk_static_data_member]  = member_name_space;
  name_space_for_symbol_kind[(int)sk_member_function]     = member_name_space;
  name_space_for_symbol_kind[(int)sk_routine]             = nsk_other;
  name_space_for_symbol_kind[(int)sk_label]               = nsk_label;
  name_space_for_symbol_kind[(int)sk_undefined]           = nsk_other;
  name_space_for_symbol_kind[(int)sk_parameter]           = nsk_other;
  name_space_for_symbol_kind[(int)sk_extern_variable]     = nsk_extern;
  name_space_for_symbol_kind[(int)sk_extern_routine]      = nsk_extern;
  name_space_for_symbol_kind[(int)sk_projection]          = nsk_other;
  name_space_for_symbol_kind[(int)sk_overloaded_function] = nsk_other;
  name_space_for_symbol_kind[(int)sk_class_template]      = nsk_other;
  name_space_for_symbol_kind[(int)sk_function_template]   = nsk_other;
  name_space_for_symbol_kind[(int)sk_variable_template]   = nsk_other;
  name_space_for_symbol_kind[(int)sk_concept_template]    = nsk_other;
  name_space_for_symbol_kind[(int)sk_namespace]           = nsk_other;
  name_space_for_symbol_kind[(int)sk_namespace_projection] = nsk_other;
  name_space_for_symbol_kind[(int)sk_named_module]        = nsk_other;
#if NAMED_ADDRESS_SPACES_ALLOWED
  name_space_for_symbol_kind[(int)sk_named_address_space] = nsk_other;
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
#if NAMED_REGISTERS_ALLOWED
  name_space_for_symbol_kind[(int)sk_named_register] = nsk_other;
#endif /* NAMED_REGISTERS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  name_space_for_symbol_kind[(int)sk_property_set] = nsk_other;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if CHECKING
  /* "undefined" and "routine" must be in the same name space.  See
      decl_default_function. */
  if (name_space_for_symbol_kind[(int)sk_undefined] !=
      name_space_for_symbol_kind[(int)sk_routine]) {
    internal_error(
  "symbol_table_one_time_init: different name spaces for undefined & routine");
  }  /* if */
#endif /* CHECKING */
  /* Clear a locator that can be used to make initialization more efficient. */
  cleared_locator.symbol_header                   = NULL;
  cleared_locator.source_position                 = null_source_position;
  cleared_locator.is_qualified_name               = FALSE;
  cleared_locator.is_global_qualified_name        = FALSE;
  cleared_locator.is_file_scope_qualified_name    = FALSE;
  cleared_locator.is_operator_name                = FALSE;
  cleared_locator.is_conversion_name              = FALSE;
  cleared_locator.is_destructor_name              = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  cleared_locator.is_finalizer_name               = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  cleared_locator.is_udl_operator_name            = FALSE;
  cleared_locator.is_semivisible_nested_type      = FALSE;
  cleared_locator.access_control_error_reported   = FALSE;
  cleared_locator.has_been_coalesced              = FALSE;
  cleared_locator.is_vacuous_destructor_reference = FALSE;
  cleared_locator.is_nonclass_destructor          = FALSE;
  cleared_locator.is_inheriting_ctor              = FALSE;
  cleared_locator.is_error                        = FALSE;
  cleared_locator.do_not_clear_specific_symbol    = FALSE;
  cleared_locator.is_implicitly_qualified         = FALSE;
  cleared_locator.is_template_id                  = FALSE;
  cleared_locator.is_class_member                 = FALSE;
  cleared_locator.is_unknown_template_reference   = FALSE;
  cleared_locator.qualifier_is_super              = FALSE;
  cleared_locator.is_super_qualified              = FALSE;
  cleared_locator.is_decltype_qualified           = FALSE;
  cleared_locator.specific_symbol                 = NULL;
  cleared_locator.parent.class_type               = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  cleared_locator.is_property_or_event_accessor   = FALSE;
  cleared_locator.property_or_event_parent        = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  cleared_locator.is_template_param               = FALSE;
  cleared_locator.is_splicer                      = FALSE;
  cleared_locator.template_arg_list               = NULL;
  cleared_locator.name_qualifier                  = NULL;
  /* The following initializations are typically redundant, but are helpful
     in some testing modes (e.g., union-as-struct). */
  cleared_locator.variant.opname                  = (an_opname_kind)onk_none;
  cleared_locator.variant.conversion_result_type  = NULL;
  cleared_locator.variant.destructor_type         = NULL;
  cleared_locator.variant.decltype_type           = NULL;

  /* Static variables in symbol_tbl.c: */
  ident_buffer = NULL;
  size_ident_buffer = 0;
  size_scope_stack = 0;
  /* Clear a symbol that can be used to make initialization more efficient. */
  cleared_symbol.header                            = NULL;
  cleared_symbol.next                              = NULL;
  cleared_symbol.next_in_scope                     = NULL;
  cleared_symbol.prev_in_scope                     = NULL;
  cleared_symbol.next_in_lookup_table              = NULL;
  cleared_symbol.decl_scope                        = NO_SCOPE_NUMBER;
  cleared_symbol.decl_seq                          = 0;
  cleared_symbol.decl_position                     = null_source_position;
  cleared_symbol.token_sequence_number             = NO_TOKEN_SEQUENCE_NUMBER;
  /* Clear both fields for union-as-struct testing. */
  cleared_symbol.parent.class_type                 = NULL;
  cleared_symbol.parent.namespace_ptr              = NULL;
  cleared_symbol.module_entity                     = NULL;
  cleared_symbol.corresp_nonreal_or_nested_type    = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  cleared_symbol.hide_by_sig_lookup_result         = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  cleared_symbol.referenced                        = FALSE;
  cleared_symbol.defined                           = FALSE;
  cleared_symbol.explicit_linkage_specifier        = FALSE;
  cleared_symbol.reentered_from_prototype_scope    = FALSE;
  cleared_symbol.is_class_member                   = FALSE;
  cleared_symbol.is_error                          = FALSE;
  cleared_symbol.is_template_param                 = FALSE;
  cleared_symbol.is_nonreal_nested_type            = FALSE;
  cleared_symbol.template_param_not_visible        = FALSE;
  cleared_symbol.force_external_linkage            = FALSE;
  cleared_symbol.ambiguous                         = FALSE;
  cleared_symbol.synthesized_namespace_projection  = FALSE;
  cleared_symbol.qualified_lookup                  = FALSE;
  cleared_symbol.must_be_class_or_namespace_lookup = FALSE;
  cleared_symbol.must_be_tag_lookup                = FALSE;
  cleared_symbol.tentative_type_lookup             = FALSE;
  cleared_symbol.do_not_reuse                      = FALSE;
  cleared_symbol.instantiation_context_lookup      = FALSE;
  cleared_symbol.must_be_class_lookup              = FALSE;
  cleared_symbol.must_be_namespace_lookup          = FALSE;
  cleared_symbol.hidden_by_old_for_init            = FALSE;
  cleared_symbol.overload_set_member               = FALSE;
  cleared_symbol.is_invisible                      = FALSE;
  cleared_symbol.ignore_in_decl_scope              = FALSE;
  cleared_symbol.is_unknown_function               = FALSE;
  cleared_symbol.is_nonreal_member                 = FALSE;
  cleared_symbol.potentially_overloaded            = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  cleared_symbol.is_super_reference                = FALSE;
  cleared_symbol.is_microsoft_invisible_operator   = FALSE;
  cleared_symbol.hide_by_sig_lookup_done           = FALSE;
  cleared_symbol.suppress_hide_by_sig_lookup       = FALSE;
  cleared_symbol.declared_in_for_init              = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  cleared_symbol.is_alias                          = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
  cleared_symbol.is_pack_element                   = FALSE;
  cleared_symbol.is_pack_expansion                 = FALSE;
  cleared_symbol.is_nondeducible_pack              = FALSE;
  cleared_symbol.value_has_been_set                = FALSE;
  dummy_undefined_symbol = NULL;
  size_of_trans_unit_for_scope = 0;
  trans_unit_for_scope = NULL;
  /* Save variables from symbol_tbl.h and symbol_tbl.c that are needed for
     precompiled headers */
  if (precompiled_header_processing_required) {
    STATIC_THREAD a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(conversion_header_list),
      pch_saved_var_array_elem(literal_operator_header_list),
#if MICROSOFT_EXTENSIONS_ALLOWED
      pch_saved_var_array_elem(ms_attr_alt_name_entry_list),
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      pch_saved_var_array_elem(decl_seq_counter),
      pch_array_saved_var_array_elem(opname_symbol_table),
      /* In effect, only the first element of the scope stack entry is
         copied. */
      pch_indirect_saved_var_array_elem(scope_stack,
                                        sizeof(a_scope_stack_entry)),
      pch_saved_var_array_elem(next_scope_number),
      pch_array_saved_var_array_elem(symbol_table),
      pch_saved_var_array_elem(anonymous_parent_object_symbol_header),
      pch_saved_var_array_elem(avail_access_error_descrs),
      pch_saved_var_array_elem(avail_active_using_directives),
      pch_saved_var_array_elem(avail_symbol_list_entries),
      pch_saved_var_array_elem(avail_type_list_entries),
      pch_saved_var_array_elem(avail_namespace_list_entries),
      pch_saved_var_array_elem(template_cache_segment_table),
      pch_saved_var_array_elem(avail_dependent_type_fixups),
      pch_saved_var_array_elem(avail_template_decl_infos),
      pch_saved_var_array_elem(avail_param_ids),
      pch_saved_var_array_elem(avail_vla_fixups),
      pch_saved_var_array_elem(avail_progenitors),
      pch_saved_var_array_elem(avail_saved_macro_states),
#if MICROSOFT_EXTENSIONS_ALLOWED
      pch_saved_var_array_elem(avail_hide_by_sig_list_entries),
      pch_saved_var_array_elem(prop_or_event_accessor_header_hash_table),
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      pch_saved_var_array_elem(error_symbol_header),
      pch_saved_var_array_elem(template_param_object_symbol_header),
      pch_saved_var_array_elem(unnamed_tag_symbol_header),
      pch_saved_var_array_elem(unnamed_virtual_function_symbol_header),
      pch_saved_var_array_elem(unnamed_namespace_symbol_header),
      pch_saved_var_array_elem(anonymous_parent_object_symbol_header),
      pch_saved_var_array_elem(unnamed_field_symbol_header),
      pch_saved_var_array_elem(unnamed_field_symbol),
      pch_saved_var_array_elem(global_namespace_list_entry),
      pch_saved_var_array_elem(symbol_for_namespace_std),
      pch_saved_var_array_elem(symbol_for_namespace_std_entered),
      pch_saved_var_array_elem(symbol_for_namespace_std_meta),
      pch_saved_var_array_elem(symbol_for_namespace_std_meta_entered),
      pch_saved_var_array_elem(constexpr_intrinsic_descr_table),
#if MICROSOFT_EXTENSIONS_ALLOWED
      pch_array_saved_var_array_elem(cli_symbols),
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      pch_saved_var_array_elem(symbol_for_make_integer_seq),
      pch_saved_var_array_elem(symbol_for_make_integer_seq_alias),
      pch_saved_var_array_elem(symbol_for_type_pack_element),
      pch_saved_var_array_elem(symbol_for_type_pack_element_alias),
      pch_saved_var_array_elem(symbol_for_builtin_common_type),
      pch_saved_var_array_elem(symbol_for_builtin_common_type_alias),
      pch_saved_var_array_elem(symbol_for_builtin_dedup_pack),
      pch_saved_var_array_elem(va_list_global_alias_has_been_created),
      pch_saved_var_array_elem(file_scope_symbols_are_on_inactive_list),
      pch_saved_var_array_elem(symbols_with_no_scope),
      pch_saved_var_array_elem(symbols_with_no_scope_tail),
#if IA64_ABI
      pch_saved_var_array_elem(symbol_for_namespace_abi),
#endif /* IA64_ABI */
      pch_saved_var_array_elem(symbol_for_std_initializer_list),
      pch_saved_var_array_elem(builtin_va_list_type),
      pch_saved_var_array_elem(type_underlying_va_list),
      pch_saved_var_array_elem(error_class_template_symbol),
      pch_saved_var_array_elem(file_scope_number),
#if CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
      pch_saved_var_array_elem(last_ctor_or_dtor_sym),
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */
#if NAMED_ADDRESS_SPACES_ALLOWED
      pch_saved_var_array_elem(next_named_address_space_id),
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
#if NAMED_REGISTERS_ALLOWED
      pch_saved_var_array_elem(next_named_register_id),
#endif /* NAMED_REGISTERS_ALLOWED */
#if DEBUG
/* GCOVR_EXCL_START */
      pch_saved_var_array_elem(num_access_error_descrs_allocated),
      pch_saved_var_array_elem(num_active_using_directives_allocated),
      pch_saved_var_array_elem(num_generated_entity_blocks_allocated),
      pch_saved_var_array_elem(num_field_symbol_supplements_allocated),
      pch_saved_var_array_elem(num_static_data_member_supplements_allocated),
      pch_saved_var_array_elem(num_enum_symbol_supplements_allocated),
      pch_saved_var_array_elem(num_class_symbol_supplements_allocated),
      pch_saved_var_array_elem(num_compares_for_symbols),
      pch_saved_var_array_elem(num_conversion_headers_allocated),
      pch_saved_var_array_elem(num_literal_operator_headers_allocated),
      pch_saved_var_array_elem(num_dependent_type_fixups_allocated),
      pch_saved_var_array_elem(num_extern_symbol_descrs_allocated),
      pch_saved_var_array_elem(num_vla_fixups_allocated),
      pch_saved_var_array_elem(num_extern_type_fixups_allocated),
      pch_saved_var_array_elem(num_fast_id_lookups),
      pch_saved_var_array_elem(num_namespace_list_entries_allocated),
      pch_saved_var_array_elem(num_param_ids_allocated),
      pch_saved_var_array_elem(num_projection_descrs_allocated),
      pch_saved_var_array_elem(num_searches_for_symbols),
      pch_saved_var_array_elem(num_slow_id_lookups),
      pch_saved_var_array_elem(num_symbol_headers_allocated),
      pch_saved_var_array_elem(num_symbol_headers_in_hash_table),
      pch_saved_var_array_elem(num_symbol_list_entries_allocated),
      pch_saved_var_array_elem(num_symbol_header_lookup_entries_allocated),
      pch_saved_var_array_elem(num_type_list_entries_allocated),
      pch_saved_var_array_elem(num_substituted_type_list_entries_allocated),
      pch_saved_var_array_elem(num_symbols_allocated),
      pch_saved_var_array_elem(num_template_instances_allocated),
      pch_saved_var_array_elem(num_master_instances_allocated),
      pch_saved_var_array_elem(num_template_params_allocated),
      pch_saved_var_array_elem(num_template_symbol_supplements_allocated),
      pch_saved_var_array_elem(num_namespace_symbol_supplements_allocated),
      pch_saved_var_array_elem(num_progenitors_allocated),
      pch_saved_var_array_elem(num_hash_tables_allocated),
      pch_saved_var_array_elem(num_hash_table_entries_allocated),
      pch_saved_var_array_elem(total_hash_table_size),
      pch_saved_var_array_elem(num_token_sequence_xrefs_allocated),
      pch_saved_var_array_elem(num_constexpr_if_cache_info_allocated),
      pch_saved_var_array_elem(num_exception_spec_error_descrs_allocated),
      pch_saved_var_array_elem(num_used_symbol_buckets),
      pch_saved_var_array_elem(symbol_name_string_space),
      pch_saved_var_array_elem(num_saved_macro_states_allocated),
#if MICROSOFT_EXTENSIONS_ALLOWED
      pch_saved_var_array_elem(num_hide_by_sig_list_entries_allocated),
      pch_saved_var_array_elem(num_property_set_symbol_supplements_allocated),
      pch_saved_var_array_elem(
                         num_prop_or_event_accessor_header_lookups_allocated),
      pch_saved_var_array_elem(num_ms_attr_alt_name_entries_allocated),
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
/* GCOVR_EXCL_STOP */
#endif /* if DEBUG */
      pch_saved_var_array_elem(inst_counters),
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  register_trans_unit_variable(global_namespace_list_entry);
  register_trans_unit_variable(symbol_for_namespace_std);
  register_trans_unit_variable(symbol_for_namespace_std_entered);
  register_trans_unit_variable(symbol_for_namespace_std_meta);
  register_trans_unit_variable(symbol_for_namespace_std_meta_entered);
#if MICROSOFT_EXTENSIONS_ALLOWED
  register_trans_unit_array(cli_symbols),
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  register_trans_unit_variable(symbol_for_make_integer_seq);
  register_trans_unit_variable(symbol_for_make_integer_seq_alias);
  register_trans_unit_variable(symbol_for_type_pack_element);
  register_trans_unit_variable(symbol_for_type_pack_element_alias);
  register_trans_unit_variable(symbol_for_builtin_common_type);
  register_trans_unit_variable(symbol_for_builtin_common_type_alias);
  register_trans_unit_variable(symbol_for_builtin_dedup_pack);
  register_trans_unit_variable(va_list_global_alias_has_been_created);
#if IA64_ABI
  register_trans_unit_variable(symbol_for_namespace_abi);
#endif /* IA64_ABI */
  register_trans_unit_variable(symbol_for_std_initializer_list);
  register_trans_unit_variable(builtin_va_list_type);
  register_trans_unit_variable(type_underlying_va_list);
  register_trans_unit_variable(symbols_with_no_scope);
  register_trans_unit_variable(symbols_with_no_scope_tail);
  register_trans_unit_variable(file_scope_symbols_are_on_inactive_list);
#if MICROSOFT_EXTENSIONS_ALLOWED
  register_trans_unit_variable(predeclared_size_t_symbol);
  register_trans_unit_variable(ms_attr_alt_name_entry_list);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  register_trans_unit_variable(conversion_header_list);
  register_trans_unit_variable(decl_seq_counter);
#if CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
  register_trans_unit_variable(last_ctor_or_dtor_sym);
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */
  register_trans_unit_variable(error_class_template_symbol);
  register_trans_unit_variable(file_scope_number);
  register_trans_unit_variable(locator_for_curr_id);
#if NAMED_ADDRESS_SPACES_ALLOWED
  register_trans_unit_variable(next_named_address_space_id);
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
#if NAMED_REGISTERS_ALLOWED
  register_trans_unit_variable(next_named_register_id);
#endif /* NAMED_REGISTERS_ALLOWED */
  register_trans_unit_variable(template_cache_segment_table);
#if EXPENSIVE_CHECKING
  register_trans_unit_variable(allocated_symbols);
#endif /* EXPENSIVE_CHECKING */
}  /* symbol_tbl_one_time_init */


void symbol_tbl_trans_unit_init(void)
/*
Initialize variables related to the symbol table that are specific to a
given translation unit.
*/
{
  global_namespace_list_entry = NULL;
  /* Initialize the predeclared symbol for namespace "std". */
  symbol_for_namespace_std = NULL;
  symbol_for_namespace_std_entered = FALSE;
  symbol_for_namespace_std_meta = NULL;
  symbol_for_namespace_std_meta_entered = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  memzero((char *)cli_symbols, sizeof(cli_symbols));
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  symbol_for_make_integer_seq = NULL;
  symbol_for_make_integer_seq_alias = NULL;
  symbol_for_type_pack_element = NULL;
  symbol_for_type_pack_element_alias = NULL;
  symbol_for_builtin_common_type = NULL;
  symbol_for_builtin_common_type_alias = NULL;
  symbol_for_builtin_dedup_pack = NULL;
  va_list_global_alias_has_been_created = FALSE;
#if IA64_ABI
  symbol_for_namespace_abi = NULL;
#endif /* IA64_ABI */
  namespace_for_coroutine_types = NULL;
  symbol_for_std_initializer_list = NULL;
  builtin_va_list_type = NULL;
  type_underlying_va_list = NULL;
  symbols_with_no_scope = NULL;
  symbols_with_no_scope_tail = NULL;
  file_scope_symbols_are_on_inactive_list = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  predeclared_size_t_symbol = NULL;
  ms_attr_alt_name_entry_list = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Initialize the conversion header list. */
  conversion_header_list = NULL;
  literal_operator_header_list = NULL;
  decl_seq_counter = FIRST_DECL_SEQUENCE_NUMBER;
#if CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
  last_ctor_or_dtor_sym = NULL;
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */
  error_class_template_symbol = NULL;
  if (!is_primary_translation_unit) {
    /* In secondary translation units, the scope numbering continues where it
       left off, and the file scope number is the next available number.
       For primary translation units, file_scope_number is already set to 0. */
    file_scope_number = take_next_scope_number();
  } else {
    /* The primary translation unit file scope number is assigned during
       compilation initialization.  Record the translation unit for that
       scope now. */
    trans_unit_for_scope[file_scope_number] = curr_translation_unit;
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (!is_primary_translation_unit) {
    /* Disable source sequence entries for secondary translation units. */
    source_sequence_entries_disallowed = TRUE;
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if NAMED_ADDRESS_SPACES_ALLOWED
  next_named_address_space_id = 1;
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
#if NAMED_REGISTERS_ALLOWED
  next_named_register_id = 1;
#endif /* NAMED_REGISTERS_ALLOWED */
  template_cache_segment_table =
                    new_fe<a_template_cache_segment_table>(/*mask_width=*/10u);
#if EXPENSIVE_CHECKING
  allocated_symbols = new_general<an_allocated_symbols_list>();
#endif /* EXPENSIVE_CHECKING */
}  /* symbol_tbl_trans_unit_init */


void symbol_tbl_init(void)
/*
Initialize static variables related to the symbol table.  This is done as a
subroutine (rather than relying on static initialization) so that it
can be redone to compile more than one source file in a single invocation
of the front end.
*/
{
  /* Variables in symbol_tbl.h: */
  /* Clear the symbol table.  Note that this assumes that NULL is a zero
     bit pattern. */
  memzero((char *)symbol_table, sizeof(symbol_table));
  /* Clear the operator name symbol table.  Note that this assumes that
     NULL is a zero bit pattern. */
  memzero((char *)opname_symbol_table, sizeof(opname_symbol_table));
  next_scope_number = FILE_SCOPE_NUMBER;
  file_scope_number = take_next_scope_number();
  avail_param_ids = NULL;
  avail_dependent_type_fixups = NULL;
  avail_access_error_descrs = NULL;
  avail_active_using_directives = NULL;
  avail_symbol_list_entries = NULL;
  avail_type_list_entries = NULL;
  avail_namespace_list_entries = NULL;
  avail_template_decl_infos = NULL;
  avail_vla_fixups = NULL;
  avail_progenitors = NULL;
  avail_saved_macro_states = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  avail_hide_by_sig_list_entries = NULL;
  prop_or_event_accessor_header_hash_table = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  error_symbol_header = NULL;
  template_param_object_symbol_header = NULL;
  unnamed_tag_symbol_header = NULL;
  unnamed_virtual_function_symbol_header = NULL;
  unnamed_namespace_symbol_header = NULL;
  anonymous_parent_object_symbol_header = NULL;
  unnamed_field_symbol_header = NULL;
  unnamed_field_symbol = NULL;
  size_t_type = NULL;
  ptr_to_const_char_type = NULL;
  dummy_undefined_symbol = NULL;
  sb_counter = 0;
#if DEBUG
/* GCOVR_EXCL_START */
  num_symbols_allocated                         = 0;
  num_symbol_headers_allocated                  = 0;
  num_symbol_headers_in_hash_table              = 0;
  num_conversion_headers_allocated              = 0;
  num_literal_operator_headers_allocated        = 0;
  symbol_name_string_space                      = 0;
  num_symbol_header_lookup_entries_allocated    = 0;
  num_field_symbol_supplements_allocated        = 0;
  num_static_data_member_supplements_allocated  = 0;
  num_enum_symbol_supplements_allocated         = 0;
  num_class_symbol_supplements_allocated        = 0;
  num_template_symbol_supplements_allocated     = 0;
  num_namespace_symbol_supplements_allocated    = 0;
  num_template_params_allocated                 = 0;
  num_param_ids_allocated                       = 0;
  num_dependent_type_fixups_allocated           = 0;
  num_template_instances_allocated              = 0;
  num_master_instances_allocated                = 0;
  num_symbol_list_entries_allocated             = 0;
  num_type_list_entries_allocated               = 0;
  num_substituted_type_list_entries_allocated   = 0;
  num_template_decl_info_allocated              = 0;
  num_out_of_class_partial_specs_allocated      = 0;
  num_nondependent_call_info_allocated          = 0;
  num_templ_friend_info_allocated               = 0;
  num_namespace_list_entries_allocated          = 0;
  num_extern_symbol_descrs_allocated            = 0;
  num_vla_fixups_allocated                      = 0;
  num_extern_type_fixups_allocated              = 0;
  num_projection_descrs_allocated               = 0;
  num_used_symbol_buckets                       = 0;
  num_searches_for_symbols                      = 0;
  num_compares_for_symbols                      = 0;
  num_access_error_descrs_allocated             = 0;
  num_fast_id_lookups                           = 0;
  num_slow_id_lookups                           = 0;
  num_active_using_directives_allocated         = 0;
  num_generated_entity_blocks_allocated         = 0;
  num_progenitors_allocated                     = 0;
  num_hash_tables_allocated                     = 0;
  num_hash_table_entries_allocated              = 0;
  total_hash_table_size                         = 0;
  num_token_sequence_xrefs_allocated            = 0;
  num_constexpr_if_cache_info_allocated         = 0;
  num_exception_spec_error_descrs_allocated     = 0;
  num_saved_macro_states_allocated              = 0;
#if MICROSOFT_EXTENSIONS_ALLOWED
  num_hide_by_sig_list_entries_allocated        = 0;
  num_property_set_symbol_supplements_allocated = 0;
  num_prop_or_event_accessor_header_lookups_allocated
                                                = 0;
  num_ms_attr_alt_name_entries_allocated        = 0;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  inst_counters = alloc_fe_of_type(Dyn_array<an_inst_count>);
  construct(inst_counters, /*cap=*/256u);
  init_intrinsic_symbol_headers();
}  /* symbol_tbl_init */

#if EXPENSIVE_CHECKING

void symbol_table_memory_region_wrap_up(a_memory_region_number region)
/*
Sanitize the symbol table for the given memory region being freed.
*/
{
  if (!no_very_expensive_checking && allocated_symbols != NULL) {
    for (a_symbol_ptr sym : *allocated_symbols) {
      if (sym->kind == sk_freed) {
        /* This symbol was already freed. */
        continue;
      }  /* if */

      an_il_entry_kind kind;
      char             *ptr = il_entry_for_symbol_null_okay(sym, &kind);
      if (ptr == NULL) {
        continue;
      }  /* if */

      an_il_entry_prefix_ptr prefix = &il_entry_prefix_of(ptr);
      a_memory_region_number prefix_region = prefix->region_number;
      if (prefix_region != region) {
        continue;
      }  /* if */
      /* This symbol's IL entry is being freed. */
      sym->kind = sk_freed;
    }  /* for */
    /* Note: It's conceptually tempting to remove symbols from the list here
       after they've been marked freed (so they're not checked again).
       However, in practice normally only a small number of symbols are removed
       so this actually increases the cost of the checking. */
  }  /* if */
}  /* symbol_table_memory_region_wrap_up */


void symbol_table_trans_unit_validate()
/*
Validate the current state of the symbol table for the current translation
unit.
*/
{
  if (!no_very_expensive_checking) {
    a_boolean any_errors = FALSE;

    for (a_symbol_ptr sym : *allocated_symbols) {
      if (sym->kind == sk_freed) {
        /* Don't check freed symbols, they're no longer relevant. */
        continue;
      }  /* if */
      if (!symbol_has_trans_unit_ptr(sym)) {
        /* Don't check symbols that are acknowledged as not having an
           associated translation unit.  */
        continue;
      }  /* if */

      an_il_entry_kind kind;
      char             *ptr = il_entry_for_symbol_null_okay(sym, &kind);
      if (ptr == NULL) {
        continue;
      }  /* if */

      /* There should always be a reachable translation unit for the symbol,
         unless the symbol has already been freed (covered by the sk_freed case
         above). */
      a_translation_unit_ptr sym_tu = trans_unit_for_symbol(sym);
      a_memory_region_number sym_tu_region = sym_tu->file_scope_region_number;
      an_il_entry_prefix_ptr prefix = &il_entry_prefix_of(ptr);
      a_memory_region_number prefix_region = prefix->file_scope_region_number;
      if (sym_tu_region != prefix_region) {
#if DEBUG
/* GCOVR_EXCL_START */
        fputs("the following symbol:\n    ", f_debug);
        db_symbol(sym, "", 6);

        a_string err_msg("  claims the IL entity is in the TU correspoding "
                         "to memory region ", sym_tu_region, " but it was "
                         "actually allocated in the TU correspoding to ",
                         prefix_region);
        print(err_msg, f_debug);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
        any_errors = TRUE;
      }  /* if */
    }  /* for */
    check_assertion_str(!any_errors,
                        "at least one symbol has the incorrect "
                        "translation unit information");
    delete_general(&allocated_symbols);
  }  /* if */
}  /* symbol_table_trans_unit_validate */

#endif /* EXPENSIVE_CHECKING */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

