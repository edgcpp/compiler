/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

debug.h -- Declarations related to debugging.

*/

/* Avoid including these declarations more than once: */
#ifndef DEBUG_H
#define DEBUG_H 1

#if DEBUG
/* GCOVR_EXCL_START */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
Externals for debugging.
*/
extern void debug_early_init(void);

extern a_boolean proc_debug_option(a_const_char *debug_option);

extern a_boolean proc_debug_name_option(a_const_char *debug_option);

#if MAINTAIN_ALLOCATION_SEQUENCE_NUMBER
extern a_boolean proc_debug_alloc_seq_option(a_const_char *debug_option);
#endif /* MAINTAIN_ALLOCATION_SEQUENCE_NUMBER */

/* See the macro db_flag_is_set for a good way to call debug_flag_is_set. */
extern a_boolean debug_flag_is_set(a_const_char *name);

extern a_boolean f_db_sym_has_traced_name(a_symbol_ptr	sym);

extern a_boolean f_db_has_traced_name(a_source_correspondence *scp,
                                      an_il_entry_kind        entry_kind);

/*
Macro interface to f_db_has_traced_name.
*/
#define db_has_traced_name(entity, kind) \
  (db_active && \
   f_db_has_traced_name((a_source_correspondence *)(entity), (kind)))

/*
Macro interface to f_db_sym_has_traced_name.
*/
#define db_sym_has_traced_name(sym) \
  (db_active && \
   f_db_sym_has_traced_name(sym))

extern a_boolean f_db_trace(a_const_char     *flag_name,
                            char             *entry,
                            an_il_entry_kind kind);

/* Macro interface to f_db_trace. */
#define db_trace(name, entry, kind) \
  (db_active && f_db_trace((name), (char *)(entry), (kind)))

extern a_boolean f_db_sym_trace(a_const_char	*flag_name,
				a_symbol_ptr	sym);

/* Macro interface to f_db_sym_trace. */
#define db_sym_trace(name, sym) \
  (db_active && f_db_sym_trace((name), sym))

/* Prints the headers for a given category of data structures (e.g.,
   "Lexical table use"). */
#define db_space_used_header(name)					 \
  fprintf(f_debug, "\n%s\n", name);					 \
  fprintf(f_debug, "%25s %8s %8s %8s\n", "Table", "Number", "Each", "Total")
  

/* Macros that display the space used by a given type of structure.
   db_space_used computes the total space used for a given type
   of structure given the number of items allocated and the type of the
   structure.  db_space_lost computes the number of allocated records
   that were never freed.  This is done by scanning the available list
   and counting the number of entries.  db_space_used_lost calls
   both of the other routines. */

#define db_space_used(name, counter, type)                            \
{ num = counter; size = sizeof(type); total = num*size;               \
  fprintf(f_debug, "%25s %8lu %8lu %8lu\n", name, num, size, total);  \
  grand_total += total;                                               \
}  /* db_space_used */


#define db_space_used_general(name, counter, type)                    \
{ num = counter; size = (unsigned long)sizeof(type); total = num*size; \
  fprintf(f_debug, "%25s %8lu %8lu %8lu (gen. storage)\n", name, num, \
          size, total);                                               \
  grand_total += total;                                               \
}  /* db_space_used_general */


#define db_space_used_general_buffer(name, size)                      \
{ fprintf(f_debug, "%25s %8s %8s %8lu (gen. storage)\n", name, "",    \
          "", size);                                                  \
  grand_total += size;                                                \
}  /* db_space_used_general_buffer */


#define db_space_used_nontype(name, counter, size_arg)                \
{ num = (unsigned long)counter;                                       \
  size = (unsigned long)size_arg;                                     \
  total = num*size;                                                   \
  fprintf(f_debug, "%25s %8lu %8lu %8lu\n", name, num, size, total);  \
  grand_total += total;                                               \
}  /* db_space_used_nontype */


#define db_space_lost(avail_list, counter, type)                \
{ type          *ptr;                                                 \
  unsigned long count = 0;                                            \
  for (ptr = avail_list; ptr != NULL; ptr = ptr->next) count++;       \
  if (count != counter) {                                             \
    fprintf(f_debug, "%25s %8lu %8s %8s lost\n", "", counter-count, "", ""); \
  }  /* if */                                                         \
}  /* db_space_lost */


#define db_space_used_lost(name, avail_list, counter, type)              \
{ db_space_used(name, counter, type);                             	 \
  db_space_lost(avail_list, counter, type);                        	 \
}  /* db_space_used_lost */


#define db_space_used_lost_general(name, avail_list, counter, type)      \
{ db_space_used_general(name, counter, type);                            \
  db_space_lost(avail_list, counter, type);                        	 \
}  /* db_space_used_lost_general */


/* Prints a "miscellaneous" line including a name, a number (printed under
   the "total" column, and a remark. */
#define db_space_used_other(name, number, remarks)			\
  fprintf(f_debug, "%25s %8s %8s %8lu %s\n", name,  "", "", number, remarks)

/* Prints a "miscellaneous" line including a name, a floating point number
   (printed under the "total" column, and a remark. */
#define db_space_used_float_other(name, number, remarks)		\
  fprintf(f_debug, "%25s %8s %8s %8.2f %s\n", name,  "", "", number, remarks)


/* Prints the grand total. */
#define db_space_used_total()						\
  db_space_used_other("Total", grand_total, "")


extern void db_print_indent(unsigned amount);


inline a_boolean db_color_flag_is_set()
/*
Return TRUE if a db flag is set for colorization.
*/
{
  return db_flag_is_set("brightcolor") || db_flag_is_set("darkcolor");
}  /* db_color_flag_is_set */


/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef DEBUG_H */

