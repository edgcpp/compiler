/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

scope_stk.c - Management of the scope stack and related routines.

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
#if DO_IL_LOWERING
#include "lower_il.h"
#include "lower_c99.h"
#endif /* DO_IL_LOWERING */
#if NEED_NAME_MANGLING
#include "lower_name.h"
#endif /* NEED_NAME_MANGLING */
/* exprutil.h is needed to get an_expr_stack_entry for the scope stack. */
#include "exprutil.h"
/* statement.h is needed because of wrapup_control_flow_processing call. */
#include "statements.h"
#if MAINTAIN_NEEDED_FLAGS
#include "il_walk.h"
#endif /* MAINTAIN_NEEDED_FLAGS */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
Variables and constants related to the scope_stack:
*/
#define SCOPE_STACK_INCREMENTAL_ALLOCATION 30
			/* The number of elements added to scope_stack each
			   time it is reallocated; also the initial
			   allocation. */

STATIC_THREAD a_function_shareable_constants_table_ptr
		avail_function_shareable_constants_tables;
			/* A list of the function shareable constant tables
			   that have been freed and are available for
			   reuse. */

STATIC_THREAD a_pack_reference_ptr
		avail_pack_references;
			/* A list of pack reference entries that have
			   been freed and are available for reuse. */

STATIC_THREAD a_pack_expansion_stack_entry_ptr
		avail_pack_expansion_stack_entries;
			/* A list of pack expansion stack entries that have
			   been freed and are available for reuse. */

STATIC_THREAD a_pack_expansion_descr_ptr
		avail_pack_expansion_descrs;
			/* A list of pack expansion descriptors that have
			   been freed and are available for reuse. */

STATIC_THREAD a_pack_instantiation_descr_ptr
		avail_pack_instantiation_descrs;
			/* A list of pack instantiation descriptors that have
			   been freed and are available for reuse. */

#if GENERATE_SOURCE_SEQUENCE_LISTS
#if DO_IL_LOWERING && !PRESERVE_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING
STATIC_THREAD a_boolean
		any_lowering_being_done;
			/* TRUE if C99 lowering or C++ lowering is being
			   done. */
#endif /* DO_IL_LOWERING && !PRESERVE_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

#if DEBUG
/*
Counts of tables allocated, to track total use of memory.
*/
STATIC_THREAD unsigned long
		num_pack_expansion_stack_entries_allocated,
		num_pack_references_allocated,
		num_pack_expansion_descrs_allocated,
		num_pack_instantiation_descrs_allocated,
		num_function_shareable_constants_tables_allocated;

#if DO_IL_LOWERING && ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS
STATIC_THREAD unsigned long
		num_string_literal_table_entries_allocated,
		num_string_literal_tables_allocated;
#endif /* DO_IL_LOWERING && ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS */

int db_scope_kind(a_scope_kind sck)
/*
Put out a scope kind name (for debugging).
*/
{
  a_const_char *s = "";

  switch (sck) {
    case sck_block:                  s = "block";                    break;
    case sck_class_reactivation:     s = "class reactivation";       break;
    case sck_class_struct_union:     s = "class/struct/union";       break;
    case sck_condition:              s = "condition";                break;
    case sck_enum:                   s = "enum";                     break;
    case sck_file:                   s = "file";                     break;
    case sck_func_prototype:         s = "function prototype";       break;
    case sck_function:               s = "function";                 break;
    case sck_function_access:        s = "function access";          break;
    case sck_instantiation_context:  s = "instantiation context";    break;
    case sck_module_decl_import:     s = "module decl import";       break;
    case sck_module_isolated:        s = "module isolation";         break;
    case sck_namespace:              s = "namespace";                break;
    case sck_namespace_extension:    s = "namespace extension";      break;
    case sck_namespace_reactivation: s = "namespace reactivation";   break;
    case sck_none:                   s = "none";                     break;
    case sck_pragma:                 s = "pragma";                   break;
    case sck_template_declaration:   s = "template declaration";     break;
    case sck_template_instantiation: s = "template instantiation";   break;
    default_is_unexpected();
  }  /* switch */
  fputs(s, f_debug);
  return (int)strlen(s);
}  /* db_scope_kind */


void db_scope_stack_entry_at_depth(a_scope_depth  depth)
/*
Display identifying information for the scope stack entry at the indicated
depth in the scope stack, for debugging purposes.
*/
{
  a_scope_stack_entry_ptr  scope_stack_ptr;

  if (depth > depth_scope_stack || depth <= NO_SCOPE_DEPTH) {
    fputs("***BAD SCOPE DEPTH***", f_debug);
  } else {
    scope_stack_ptr = &scope_stack[depth];
    if (scope_stack_ptr->il_scope == NULL) {
      (void)db_scope_kind(scope_stack_ptr->kind);
      fprintf(f_debug, " scope %d", (int)scope_stack_ptr->number);
    } else {
      db_scope(scope_stack_ptr->il_scope);
    }  /* if */
  }  /* if */
}  /* db_scope_stack_entry_at_depth */


void db_scope_stack_entry(a_scope_stack_entry_ptr ssep)
/*
Display one scope stack entry.
*/
{
  int                      len;

  fprintf(f_debug, "%s%3ld %3d ",
          (ssep == &scope_stack[decl_scope_level]) ? "**" : "  ",
          (long)ssep->number, (int)scope_depth_of(ssep));
  len = db_scope_kind(ssep->kind);
  fprintf(f_debug, "%-*s", 25-len, "");
  fprintf(f_debug, "prev=%3d ", ssep->previous_scope);
  switch (ssep->kind) {
    case sck_condition:
    case sck_file:
    case sck_func_prototype:
    case sck_function_access:
    case sck_instantiation_context:
    case sck_module_decl_import:
    case sck_module_isolated:
    case sck_none:
    case sck_pragma:
    case sck_template_declaration:
      break;
    case sck_function:
      if (ssep->il_scope == NULL) {
        fprintf(f_debug, "null IL scope");
      } else {
        db_name_full(&ssep->il_scope->variant.routine.ptr->source_corresp,
                     iek_routine);
      }  /* if */
      break;
    case sck_block:
      if (ssep->il_scope == NULL) {
        fprintf(f_debug, "null IL scope");
      }  /* if */
      break;
    case sck_namespace:
    case sck_namespace_extension:
    case sck_namespace_reactivation:
      if (ssep->il_scope == NULL) {
        fprintf(f_debug, "null IL scope");
      } else {
        a_namespace_ptr  nsp = ssep->il_scope->variant.assoc_namespace;
        if (nsp == NULL) {
          fprintf(f_debug, "null assoc_namespace");
        } else {
          db_name_full(&nsp->source_corresp, iek_namespace);
        }  /* if */
      }  /* if */
      break;
    case sck_class_struct_union:
    case sck_class_reactivation:
    case sck_enum:
      db_abbreviated_type(ssep->assoc_type);
      break;
    case sck_template_instantiation:
      if (ssep->template_sym == NULL) {
        fputs("<null template symbol>", f_debug);
      } else {
        fprintf(f_debug, "<%s> ",
                symbol_kind_names[(int)ssep->template_sym->kind]);
        if (ssep->assoc_type != NULL) {
          db_type_name(ssep->assoc_type);
        } else {
          db_name(source_corresp_entry_for_symbol(ssep->template_sym));
        }  /* if */
      }  /* if */
      break;
    default_is_unexpected();
  }  /* switch */
  fputs("\n", f_debug);
}  /* db_scope_stack_entry */


void db_scope_stack(void)
/*
Dump the entire scope stack (for debugging).
*/
{
  if (depth_scope_stack == NO_SCOPE_DEPTH) {
    fputs("Scope stack is empty.\n", f_debug);
  } else {
    a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];

    for (; ssep != NULL;
         ssep = ssep->kind == (a_scope_kind)sck_file ? NULL : ssep - 1) {
      db_scope_stack_entry(ssep);
    }  /* for */
  }  /* if */
}  /* db_scope_stack */


void db_top_of_scope_stack(int entries)
/*
Dump the top "entries" of the scope stack (for debugging).
*/
{
  if (depth_scope_stack == NO_SCOPE_DEPTH) {
    fputs("Scope stack is empty.\n", f_debug);
  } else {
    a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];

    for (; ssep != NULL;
         ssep = ssep->kind == (a_scope_kind)sck_file ? NULL : ssep - 1) {
      db_scope_stack_entry(ssep);
      if (--entries == 0) break;
    }  /* for */
  }  /* if */
}  /* db_top_of_scope_stack */

#if EXTRA_SOURCE_POSITIONS_IN_IL

void db_source_range(a_source_range  *range)
/*
Dump the specified range.
*/
{
  fprintf(f_debug, "%4lu/%-3lu -- %4lu/%-3lu",
          (unsigned long)range->start.seq,
          (unsigned long)range->start.column,
          (unsigned long)range->end.seq,
          (unsigned long)range->end.column);
}  /* db_source_range */


void db_decl_pos_info(a_symbol_ptr  sym)
/*
Dump decl-pos information for the specified symbol (for debugging).
*/
{
  a_decl_position_supplement_ptr  dpsp;
  a_source_correspondence         *scp;
  a_boolean                       is_enumerator;

  if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
    sym = sym->variant.overloaded_function.symbols;
    for (; sym != NULL; sym = sym->next) db_decl_pos_info(sym);
  } else if (!sym->is_error && sym->decl_position.seq != 0) {
    scp = source_corresp_entry_for_symbol(sym);
    if (scp != NULL) {
      fprintf(f_debug, " ");
      db_symbol_name(sym);
      fprintf(f_debug, " <%s>, decl_position: %lu/%lu",
                       symbol_kind_names[(int)sym->kind],
                       (unsigned long)scp->decl_position.seq,
                       (unsigned long)scp->decl_position.column);
      dpsp = scp->decl_pos_info;
      if (dpsp == NULL) {
        fputs(", no decl-pos info\n", f_debug);
      } else {
        is_enumerator = (sym->kind == (a_symbol_kind)sk_constant &&
                         is_enum_constant(sym->variant.constant));
        fputc('\n', f_debug);
        if (!is_enumerator) {
          if (dpsp->specifiers_range.start.seq != 0 ||
              dpsp->specifiers_range.end.seq != 0) {
            fprintf(f_debug, "    specifiers range:  ");
            db_source_range(&dpsp->specifiers_range);
            fputc('\n', f_debug);
          }  /* if */
          if (dpsp->variant.declarator_range.start.seq != 0 ||
              dpsp->variant.declarator_range.end.seq != 0) {
            fprintf(f_debug, "    declarator range:  ");
            db_source_range(&dpsp->variant.declarator_range);
            fputc('\n', f_debug);
          }  /* if */
        }  /* if */
        if (dpsp->identifier_range.start.seq != 0 ||
            dpsp->identifier_range.end.seq != 0) {
          fprintf(f_debug, "    identifier range:  ");
          db_source_range(&dpsp->identifier_range);
          fputc('\n', f_debug);
        }  /* if */
        if (is_enumerator &&
            (dpsp->variant.enum_value_range.start.seq != 0 ||
             dpsp->variant.enum_value_range.end.seq != 0)) {
          fprintf(f_debug, "    enum value range:  ");
          db_source_range(&dpsp->variant.enum_value_range);
          fputc('\n', f_debug);
        } else if (sym->kind == (a_symbol_kind)sk_variable ||
                   sym->kind == (a_symbol_kind)sk_static_data_member) {
          a_variable_ptr  vp = sym->variant.variable.ptr;
          if (vp->initializer_range.start.seq != 0 ||
              vp->initializer_range.end.seq != 0) {
            fprintf(f_debug, "    initializer range: ");
            db_source_range(&vp->initializer_range);
            fputc('\n', f_debug);
          }  /* if */
        } else if (!C_mode() && is_class_struct_union_symbol(sym)) {
          a_base_class_ptr  bcp = base_classes_of(type_symbol_type(sym));
          for (; bcp != NULL; bcp = bcp->next) {
            if (bcp->base_specifier_range.start.seq != 0 ||
                bcp->base_specifier_range.end.seq != 0) {
              fprintf(f_debug, "    base class \"");
              db_type_name(bcp->type);
              fprintf(f_debug, "\", decl_position: %lu/%lu\n",
                      (unsigned long)bcp->decl_position.seq,
                      (unsigned long)bcp->decl_position.column);
              fprintf(f_debug, "      specifier range: ");
              db_source_range(&bcp->base_specifier_range);
              fputc('\n', f_debug);
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    if (sym->kind == (a_symbol_kind)sk_function_template) {
      /* Display decl-pos info for each explicit specialization of the
         function template. */
      a_template_instance_ptr  tip;
      a_symbol_ptr             instance_sym;

      tip = sym->variant.template_info->variant.function.instantiations;
      for (; tip != NULL; tip = tip->next) {
        instance_sym = tip->instance_sym;
        if (instance_sym->kind == (a_symbol_kind)sk_routine ||
            instance_sym->kind == (a_symbol_kind)sk_member_function) {
          if (instance_sym->variant.routine.ptr->is_specialized) {
            db_decl_pos_info(instance_sym);
          }  /* if */
        }  /* if */
      }  /* for */
    } else if (sym->kind == (a_symbol_kind)sk_class_template) {
      /* Display decl-pos info for each explicit specialization of the
         class template. */
      a_symbol_list_entry_ptr	    slep;
      for (slep = sym->variant.template_info->
                                  variant.class_template.instantiations;
           slep != NULL; slep = slep->next) {
        a_symbol_ptr  instance_sym = slep->symbol;
        if (is_class_struct_union_symbol(instance_sym) &&
            instance_sym->variant.class_struct_union.type->
                             variant.class_struct_union.is_specialized) {
          db_decl_pos_info(instance_sym);
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
}  /* db_decl_pos_info */


static void db_decl_pos_info_for_scope(a_scope_ptr            scope_ptr,
                                       a_scope_pointers_block *pointers_block)
/*
Dump decl-pos information for each symbol in the specified scope (for
debugging).
*/
{
  a_symbol_ptr  sym = pointers_block->symbols;

  if (sym != NULL) {
    fprintf(f_debug, "decl-pos info for ");
    db_scope(scope_ptr);
    fprintf(f_debug, "\n");
    for (; sym != NULL; sym = sym->next_in_scope) db_decl_pos_info(sym);
  }  /* if */
}  /* db_decl_pos_info_for_scope */

#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

STATIC_THREAD a_scope_depth
		max_depth_scope_stack = 0;
			/* A variable tracking the maximum depth of the
			   scope stack. */

STATIC_THREAD unsigned long
		scope_kind_stats[1+(int)sck_none] = { 0 };

void db_scope_stack_stats(void)
/*
Display some statistics about the scope stack.
*/
{
  int           k;
  unsigned long total = 0;

  fprintf(f_debug, "\nScope stack statistics");
  fprintf(f_debug, "\n======================\n");
  fprintf(f_debug, "Stack entry size: %d\n", (int)sizeof(a_scope_stack_entry));
  fprintf(f_debug, "Max. stack depth: %d\n", (int)max_depth_scope_stack);
  for (k = 0; k <= (int)sck_none; ++k) {
    int p = db_scope_kind((a_scope_kind)k);
    for (; p<24; ++p) fputs(" ", f_debug);
    fprintf(f_debug, ": %8lu\n", scope_kind_stats[k]);
    total += scope_kind_stats[k];
  }  /* for */
  fprintf(f_debug, "%24s: %8lu\n", "TOTAL", total);
}  /* db_scope_stack_stats */

#endif /* DEBUG */

#if NEED_NAME_MANGLING

#define LOCAL_NAME_COLLISION_TABLE_SIZE 16

/*
A hash table type to detect name collisions between declarations in function
scope.
*/
typedef union a_collision_table {
  a_symbol_list_entry_ptr
		buckets[LOCAL_NAME_COLLISION_TABLE_SIZE];
			/* The actual table of lists. */
  a_collision_table_ptr
		next_avail;
			/* Pointer to the next available free list. */
} a_collision_table;


/*
Pointer to a list of available collision tables.
*/
STATIC_THREAD a_collision_table_ptr
                avail_collision_tables;

/*
Last discriminator values assigned to unnamed types / closure types in the
file scope.  Saved here (rather than on a_scope_pointers_block) because there
is only one file scope per translation unit, and these must survive
file-scope reactivation (e.g., namespace_inject into ::).
*/
STATIC_THREAD a_discriminator
		last_file_scope_unnamed_type_number;
STATIC_THREAD a_discriminator
		last_file_scope_closure_type_number;


static void initialize_local_name_collision_table(a_scope_stack_entry_ptr ssep)
/*
Allocate and initialize a local name collision table for the given scope stack
entry.
*/
{
  check_assertion(ssep->kind == (a_scope_kind)sck_function);
  if (avail_collision_tables == NULL) {
    ssep->name_discr.local_name_collision_table =
                    (a_collision_table_ptr)alloc_fe(sizeof(a_collision_table));
  } else {
    ssep->name_discr.local_name_collision_table = avail_collision_tables;
    avail_collision_tables = avail_collision_tables->next_avail;
  }  /* if */
  memzero((char*)ssep->name_discr.local_name_collision_table->buckets,
          size_t_arg(
            sizeof(a_symbol_list_entry_ptr[LOCAL_NAME_COLLISION_TABLE_SIZE])));
}  /* initialize_local_name_collision_table */


static void free_local_name_collision_table(a_scope_stack_entry_ptr ssep)
/*
Release the storage allocated for the name collision table associated with
the given scope stack entry.
*/
{
  if (ssep->kind != (a_scope_kind)sck_function) {
    /* No local name collision table should be allocated for anything but a
       function scope. */
    check_assertion((ssep->kind != (a_scope_kind)sck_block &&
                     ssep->kind != (a_scope_kind)sck_condition) ||
                    ssep->name_discr.local_name_collision_table == NULL);
  } else if (ssep->name_discr.local_name_collision_table != NULL) {
    int  k;
    a_symbol_list_entry_ptr
         *sleps = ssep->name_discr.local_name_collision_table->buckets;
    /* First free up the lists. */
    for (k = 0; k<LOCAL_NAME_COLLISION_TABLE_SIZE; ++k) {
      if (sleps[k] != NULL) {
        free_list_of_symbol_list_entries(sleps[k]);
      }  /* if */
    }  /* for */
    /* Return the table entry to the available list. */
    ssep->name_discr.local_name_collision_table->next_avail =
                                                       avail_collision_tables;
    avail_collision_tables = ssep->name_discr.local_name_collision_table;
    ssep->name_discr.local_name_collision_table = NULL;
  }  /* if */
}  /* free_local_name_collision_table */


static a_const_char* name_for_linkage_purposes(a_symbol_ptr  sym)
/*
Return the "name for linkage purposes" of the given entity.  If the entity is
unnamed, return the symbol header identifier (usually something like
"<unnamed>").  The returned value is always non-NULL.
*/
{
  a_const_char *result = NULL;

  if (is_unnamed_tag_symbol(sym)) {
    /* Unnamed tag types may have acquired a name for linkage purposes through
       a typedef.  This name will be the unmangled name in the IL entry. */
    a_type_ptr  type = type_symbol_type(sym);
    result = unmangled_name_of(&type->source_corresp);
    if (result == NULL) {
      /* No name for linkage purposes was given: Fall back on the header
         identifier. */
      result = sym->header->identifier;
    }  /* if */
  } else {
    result = sym->header->identifier;
  }  /* if */
  check_assertion(result != NULL);
  return result;
}  /* name_for_linkage_purposes */


static a_boolean same_name_for_linkage_purposes(a_symbol_ptr  sym1,
                                                a_symbol_ptr  sym2)
/*
Return TRUE if the two given entities have the same "name for linkage
purposes".
*/
{
  a_const_char *name1, *name2;

  name1 = name_for_linkage_purposes(sym1);
  name2 = name_for_linkage_purposes(sym2);
  return name1 == name2 || strcmp(name1, name2) == 0;
}  /* same_name_for_linkage_purposes */


static a_symbol_list_entry_ptr* get_name_collision_list(a_symbol_ptr   sym,
                                                        a_scope_depth  depth)
/*
depth indicates a scope depth that is local to a function.  Return the address
of the pointer to the bucket list associated with the given symbol in the
name collision table for that function.  (If no table exists yet for this
function, one is created.)
*/
{
  unsigned                 hash_index;
  a_scope_stack_entry_ptr  ssep;
  a_collision_table_ptr    table;

  depth = scope_stack[depth].depth_innermost_function_scope;
  check_assertion(depth != NO_SCOPE_DEPTH);
  ssep = &scope_stack[depth];
  if (ssep->name_discr.local_name_collision_table == NULL) {
    initialize_local_name_collision_table(ssep);
  }  /* if */
  table = ssep->name_discr.local_name_collision_table;
  hash_index = (unsigned)(hash_source_string(
                                 (a_void_ptr)name_for_linkage_purposes(sym)) %
                                              LOCAL_NAME_COLLISION_TABLE_SIZE);
  return &table->buckets[hash_index];
}  /* get_name_collision_list */


static a_boolean distinct_lambda_signatures(a_type_ptr  ctp1,
                                            a_type_ptr  ctp2)
/*
The two given types must be closure types.  Return FALSE if their respective
function call operators have the same parameter types; TRUE otherwise.
*/
{
  a_boolean      result;
  a_routine_ptr  rp1 = lambda_body_for_closure(ctp1);
  a_routine_ptr  rp2 = lambda_body_for_closure(ctp2);

  if (rp1 == NULL || rp2 == NULL) {
    expect_error();
    result = TRUE;
  } else {
    result = !param_types_are_compatible(rp1->type, rp2->type, TCF_NO_FLAGS);
  }  /* if */
  return result;
}  /* distinct_lambda_signatures */


static a_discriminator discriminator_of(a_symbol_ptr  sym)
/*
The given symbol must represent a local static variable, a local class or
enumeration type, or a local typedef.  Return the associated discriminator.
*/
{
  a_discriminator  result = 0;

  switch (sym->kind) {
    case sk_variable:
      result = sym->variant.variable.discriminator;
      break;
    case sk_class_or_struct_tag:
    case sk_union_tag:
      result = sym->variant.class_struct_union.extra_info->discriminator;
      break;
    case sk_enum_tag:
      result = sym->variant.enumeration.extra_info->discriminator;
      break;
    case sk_type:
      result = sym->variant.type.discriminator;
      break;
    default:
      unexpected_condition();
  }  /* switch */
  return result;
}  /* discriminator_of */


static void compute_local_name_collision_discriminator(a_symbol_ptr   sym,
                                                       a_scope_depth  depth)
/*
Look in the name collision table associated with current function scope for a
symbol that has the same name for linkage purposes as the given symbol sym
(declared at the given scope depth).  If there is one, the current symbol is
assigned a discriminator value one higher than that of the symbol found.
Otherwise the discriminator value of sym is initialized to one.  Either way,
the symbol is added to the table, thereby becoming the symbol that would be
found if the name would be searched for again.  (Closure types are handled
specially: They are considered to be "colliding" only if the associated lambda
routines have the same type.  Other unnamed entities always collide with each
other.)  This information is used to generate distinct mangled names of
function-local entities.
*/
{
  a_symbol_list_entry_ptr  *p_sep, sep, new_entry;
  a_discriminator          value = 1;
  a_boolean                sym_is_for_lambda = is_closure_class_symbol(sym);

  check_assertion(discriminator_of(sym) == 0);
  p_sep = get_name_collision_list(sym, depth);
  /* Search for a "collision". */
  for (sep = *p_sep; sep != NULL; sep = sep->next) {
    if (same_name_for_linkage_purposes(sym, sep->symbol)) {
      /* Closure classes and other entities have distinct discriminator
         sequences. */
      if (sym_is_for_lambda) {
        if (is_closure_class_symbol(sep->symbol)) {
          /* Two closure types. */
          a_type_ptr  type, new_type;
          new_type = sym->variant.class_struct_union.type;
          type = sep->symbol->variant.class_struct_union.type;
          if (
#if IA64_ABI
              !emulate_gnu_abi_bugs &&
#endif /* IA64_ABI */
              distinct_lambda_signatures(type, new_type)) {
            /* Two closure types whose call operators have distinct types are
               considered to be non-colliding.  (Except when emulating GCC ABI
               bugs, since g++ appears to ignore the lambda signature.) */
          } else {
            /* A collision between closure types. */
            value = discriminator_of(sep->symbol)+1;
            break;
          }  /* if */
        }  /* if */
      } else if (!is_closure_class_symbol(sep->symbol)) {
        /* A collision between two entities that aren't closure classes. */
        value = discriminator_of(sep->symbol)+1;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  /* Record the new symbol in the collision table. */
  new_entry = alloc_symbol_list_entry();
  new_entry->next = *p_sep;
  *p_sep = new_entry;
  new_entry->symbol = sym;
  switch (sym->kind) {
    case sk_variable:
      sym->variant.variable.discriminator = value;
      break;
    case sk_class_or_struct_tag:
    case sk_union_tag:
      sym->variant.class_struct_union.extra_info->discriminator = value;
      break;
    case sk_enum_tag:
      sym->variant.enumeration.extra_info->discriminator = value;
      break;
    case sk_type:
      sym->variant.type.discriminator = value;
      break;
    default:
      unexpected_condition();
  }  /* switch */
}  /* compute_local_name_collision_discriminator */


void compute_name_collision_discriminator(a_symbol_ptr   sym,
                                          a_scope_depth  scope_depth)
/*
Assign a distinguishing "discriminator" value to the given entity declared in
the indicated scope (which must be a local scope, a class scope, a namespace
scope, or the file scope).  In non-local scopes, this only applies to unnamed
class and enumeration types (which includes closure types).  In local scopes,
additional possibilities exist: See compute_local_name_collision_discriminator.
Name collision discriminators may be "taken back" in two cases: (1) when an
unnamed type acquires a typedef name for linkage purposes, and (2) when an
unnamed type ends up being an "anonymous union".
*/
{
  a_scope_stack_entry_ptr
             ssep = &scope_stack[scope_depth];
  a_boolean  local_scope = is_local_scope_kind(ssep->kind);

  if (local_scope) {
    /* Local scope entities use a "collision table". */
    compute_local_name_collision_discriminator(sym, scope_depth);
  } else if (sym->kind == (a_symbol_kind)sk_class_or_struct_tag &&
             class_type_supp(sym->variant.class_struct_union.type)
                                                  ->is_lambda_closure_class) {
    /* Closure types have their own numbering convention. */
    a_class_symbol_supplement_ptr
                            cssp = sym->variant.class_struct_union.extra_info;
    if (entities_are_recorded_for_current_expression()) {
      /* The discriminator is determined elsewhere (e.g., in
         compute_default_arg_name_collision_discriminators). */
    } else {
      cssp->discriminator = ++ssep->last_closure_type_number;
    }  /* if */
  } else if (is_unnamed_tag_symbol(sym)) {
    /* An unnamed enum/class type in file, namespace, or class scope. */
    if (is_real_class_symbol(sym)) {
      a_type_ptr  class_type = sym->variant.class_struct_union.type;
      if (!has_name(class_type)  &&
          sym->variant.class_struct_union.extra_info->discriminator == 0) {
        sym->variant.class_struct_union.extra_info->discriminator =
                                  ++ssep->name_discr.last_unnamed_type_number;
      }  /* if */
    } else if (sym->kind == (a_symbol_kind)sk_enum_tag) {
      if (!has_name(sym->variant.enumeration.type) &&
          sym->variant.enumeration.extra_info->discriminator == 0) {
        sym->variant.enumeration.extra_info->discriminator =
                                  ++ssep->name_discr.last_unnamed_type_number;
      }  /* if */
    }  /* if */
  }  /* if */
}  /* compute_name_collision_discriminator */


void cancel_name_collision_discriminator(a_symbol_ptr   sym,
                                         a_scope_depth  scope_depth)
/*
The given symbol represents an unnamed type to which a discriminator was
assigned, but which now turns out not to require a discriminator (because it
either (1) is acquiring a name for linkage purposes through a typedef, or
(2) it is an anonymous union type).
Make the discriminator field available for another type, and zero out the
discriminator field in the symbol supplement.
*/
{
  a_scope_stack_entry_ptr
              ssep = &scope_stack[scope_depth];
  a_boolean   local_scope = is_local_scope_kind(ssep->kind);
  a_type_ptr  type = type_symbol_type(sym);

  check_assertion(is_unnamed_tag_symbol(sym) &&
                  !type->source_corresp.name_has_been_mangled);
  if (local_scope) {
    a_symbol_list_entry_ptr  *p_sep, sep;
    p_sep = get_name_collision_list(sym, scope_depth);
    /* The given symbol must be on the list, but new (named) symbols may
       already have been added. */
    check_assertion(p_sep != NULL);
    while ((*p_sep)->symbol != sym) {
      check_assertion(!is_unnamed_tag_symbol((*p_sep)->symbol));
      p_sep = &(*p_sep)->next;
      check_assertion(p_sep != NULL);
    }  /* while */
    /* Remove and dispose of the list entry. */
    sep = *p_sep;
    *p_sep = sep->next;
    sep->next = NULL;
    free_list_of_symbol_list_entries(sep);
    /* Clear the discriminator value in the symbol supplement. */
    switch (sym->kind) {
      case sk_class_or_struct_tag:
      case sk_union_tag:
        if (class_type_supp(type)->anonymous_union_kind !=
                                          (an_anonymous_union_kind)auk_none) {
          /* An anonymous union: Set the discriminator to "1" to indicate the
             lack of collision. */
          sym->variant.class_struct_union.extra_info->discriminator = 1;
        } else {
          /* Another anonymous class type: Its discriminator will be
             recomputed shortly.  Clear it for now. */
          sym->variant.class_struct_union.extra_info->discriminator = 0;
        }  /* if */
        break;
      case sk_enum_tag:
        sym->variant.enumeration.extra_info->discriminator = 0;
        break;
      default:
        unexpected_condition();
    }  /* switch */
  } else {
#if CHECKING
    a_discriminator  prev_value = ssep->name_discr.last_unnamed_type_number;
#endif  /* CHECKING */
    ssep->name_discr.last_unnamed_type_number -= 1;
    if (is_real_class_symbol(sym)) {
      check_assertion(sym->variant.class_struct_union.extra_info->discriminator
                        == prev_value);
      if (class_type_supp(type)->anonymous_union_kind !=
                                          (an_anonymous_union_kind)auk_none) {
        /* An anonymous union: Set the discriminator to "1" to indicate the
           lack of collision. */
        sym->variant.class_struct_union.extra_info->discriminator = 1;
      } else {
        /* Another anonymous class type: Its discriminator will be
           recomputed shortly.  Clear it for now. */
        sym->variant.class_struct_union.extra_info->discriminator = 0;
      }  /* if */
    } else if (sym->kind == (a_symbol_kind)sk_enum_tag) {
      check_assertion(sym->variant.enumeration.extra_info->discriminator
                        == prev_value);
      sym->variant.enumeration.extra_info->discriminator = 0;
    }  /* if */
  }  /* if */
}  /* cancel_name_collision_discriminator */


static void assign_discriminators_to_entities_list(
                                         an_il_entity_list_entry_ptr  elp,
                                         a_symbol_ptr                 sym)
/*
Assign consecutive discriminator values (starting with one) to the entities
in the given list when appropriate (currently, this is only for closure types
in the list).  If the entities list is one associated with a data member
initializer, sym represents that data member (otherwise, it is NULL).
*/
{
  a_discriminator  last_n = 0;

  for (; elp != NULL; elp = elp->next) {
    if (elp->entity.kind == iek_type) {
      a_type_ptr  tp = (a_type_ptr)elp->entity.ptr;
      a_class_type_supplement_ptr
                  ctsp;
      check_assertion(is_immediate_class_type(tp));
      ctsp = class_type_supp(tp);
      if (ctsp->is_lambda_closure_class) {
        symbol_supplement_for_class(tp)->discriminator = ++last_n;
        if (sym == NULL) {
          /* Nothing more to be done. */
        } else if (symbol_is(sym, sk_static_data_member)) {
          ctsp->defined_in_variable_initializer = TRUE;
        }  /* if */
      }  /* if */
    } else {
      unexpected_condition();
    }  /* if */
  }  /* for */
}  /* assign_discriminators_to_entities_list */


void compute_default_arg_name_collision_discriminators(a_param_type_ptr  ptp)
/*
If the given parameter description has a default argument that defines entities
that require discriminators for name mangling purposes, assign those
discriminators now.  (Currently, this only applies to closure types.)
*/
{
  assign_discriminators_to_entities_list(ptp->entities_defined_in_default_arg,
                                         (a_symbol_ptr)NULL);
}  /* compute_default_arg_name_collision_discriminators */

#endif /* NEED_NAME_MANGLING */


void set_parent_entity_for_closure_types(
                   an_il_entity_list_entry_ptr  elp,
                   a_symbol_ptr                 parent_sym,
                   a_boolean                    subject_to_trans_unit_corresp)
/*
Record the given symbol as the parent entity in each of the non-nested closure
types in the given list of entities.  If subject_to_trans_unit_corresp is TRUE,
the lambda expressions defining the closure types may appear in multiple
translation units and each such lambda expression then defines the same closure
type (this routine records a flag in the symbol supplement for the closure type
to indicate this).  (parent_sym may be NULL in error cases.)
*/
{
  for (; elp != NULL; elp = elp->next) {
    if (elp->entity.kind == iek_type) {
      a_type_ptr  tp = (a_type_ptr)elp->entity.ptr;
      a_class_type_supplement_ptr
                  ctsp;
      check_assertion(is_immediate_class_type(tp));
      ctsp = class_type_supp(tp);
      if (ctsp->is_lambda_closure_class) {
        ctsp->defined_in_variable_initializer = FALSE;
        ctsp->defined_in_field_initializer = FALSE;
        if (parent_sym == NULL) {
          /* An error case (e.g., a lambda in a default argument of what turned
             out not to be a function declaration). */
          expect_error();
        } else if (symbol_is(parent_sym, sk_static_data_member)) {
          ctsp->defined_in_variable_initializer = TRUE;
          ctsp->lambda_parent.variable =
                              parent_sym->variant.static_data_member.variable;
        } else {
          check_assertion(is_simple_function_symbol(parent_sym));
          ctsp->lambda_parent.routine = parent_sym->variant.routine.ptr;
        }  /* if */
        if (subject_to_trans_unit_corresp) {
          symbol_supplement_for_class(tp)
                                ->lambda_subject_to_trans_unit_corresp = TRUE;
        }  /* if */
      } else {
        unexpected_condition();
      }  /* if */
    } else {
      unexpected_condition();
    }  /* if */
  }  /* for */
}  /* set_parent_entity_for_closure_types */


void set_parent_routine_for_closure_types_in_default_args(
                                                       a_type_ptr    rtp,
                                                       a_symbol_ptr  rout_sym)
/*
The given routine was declared with the given type.  Make sure that any closure
types defined in default arguments have the routine recorded as a parent.
*/
{
  check_assertion(is_simple_function_symbol(rout_sym));
  if (rtp->kind != (a_type_kind)tk_routine) {
    /* If the routine was declared using a typedef type, there are no default
       arguments and hence no closure types to process. */
  } else {
    a_param_type_ptr  ptp = function_type_params(rtp);
    for (; ptp != NULL; ptp = ptp->next) {
      an_il_entity_list_entry_ptr  ep = ptp->entities_defined_in_default_arg;
      if (ep != NULL) {
        set_parent_entity_for_closure_types(
                              ep, rout_sym,
                              ptp->default_arg_appeared_in_class_definition);
      }  /* if */
    }  /* for */
  }  /* if */
}  /* set_parent_routine_for_closure_types_in_default_args */

#if !STANDALONE_UTILITY_PROGRAM

a_scope_depth get_curr_lambda_depth(void)
/*
Return the depth in the scope stack of the current lambda body or
NO_SCOPE_DEPTH if there is none.  Note that while parsing the header of a
nested lambda, the "current lambda body" is that of the enclosing lambda
expression.
*/
{
  a_scope_depth  depth_lambda = depth_scope_stack;

  for (;;) {
    a_scope_stack_entry  *ssep = &scope_stack[depth_lambda];
    if (scope_is(ssep, sck_function_access) && ssep->is_rescan) {
      depth_lambda = ssep->orig_access_depth;
    } else if (ssep->depth_innermost_function_scope != NO_SCOPE_DEPTH) {
      depth_lambda = ssep->depth_innermost_function_scope;
      break;
    } else if (scope_is(ssep, sck_class_struct_union) &&
               class_type_supp(ssep->assoc_type)->is_lambda_closure_class) {
      /* Presumably a reference to an enclosing class from an init-capture
         list.  For example:
              void f(auto fn) {
                [=]{ return [f = fn]() {}; }
              }
         Here, innermost_function_scope will be NO_SCOPE_DEPTH while parsing
         the initializer in "f = fn". */
      depth_lambda = ssep->previous_scope;
    } else if (scope_is(ssep, sck_func_prototype) ||
               scope_is(ssep, sck_template_declaration)) {
      depth_lambda = ssep->previous_scope;
    } else {
      depth_lambda = NO_SCOPE_DEPTH;
      break;
    }  /* if */
  }  /* for */
  return depth_lambda;
}  /* get_curr_lambda_depth */

#endif /* !STANDALONE_UTILITY_PROGRAM */

#if DO_IL_LOWERING && ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS

#define STRING_LITERAL_TABLE_SIZE 31

/*
Entry used to construct a hash table of string literal constants.
*/
typedef struct a_string_literal_table_entry *a_string_literal_table_entry_ptr;
typedef struct a_string_literal_table_entry {
  a_string_literal_table_entry_ptr
		next;
			/* Pointer to the next entry in the bucket, or on
			   the available list. */
  a_constant_ptr
		constant;
			/* Pointer to the string literal constant represented
			   by this entry. */
  unsigned long	sequence_number;
			/* The sequence number assigned to this constant. */
} a_string_literal_table_entry;

/*
A hash table used to assign sequence numbers to each unique string literal
used within a function.
*/
typedef struct a_string_literal_table {
  a_string_literal_table_ptr
		next;	/* Pointer to the next available free list. */
  a_string_literal_table_entry_ptr
		buckets[STRING_LITERAL_TABLE_SIZE];
			/* A list of string literals used in the
			   function scope. */
} a_string_literal_table;


STATIC_THREAD a_string_literal_table_ptr
		avail_string_literal_tables;
			/* A list of string literal tables that have been
			   freed and are available for reuse. */

STATIC_THREAD a_string_literal_table_entry_ptr
		avail_string_literal_table_entries;
			/* A list of string literal table entries that have
			   been freed and are available for reuse. */


static void free_list_of_string_literal_table_entries(
				a_string_literal_table_entry_ptr	sltep)
/*
Free the list of string literal table entries pointed to by "sltep" by
placing them on the available list.   sltep may be NULL, in which case
nothing is done.
*/
{
  a_string_literal_table_entry_ptr	sltep_tail;
  if (sltep != NULL) {
    /* Find the last entry on the list. */
    sltep_tail = sltep;
    while (sltep_tail->next != NULL) sltep_tail = sltep_tail->next;
    /* Add the current available list to the end of the list passed by the
       caller. */
    sltep_tail->next = avail_string_literal_table_entries;
    avail_string_literal_table_entries = sltep;
  }  /* if */
}  /* free_list_of_string_literal_table_entries */


static void initialize_string_literal_table(a_scope_stack_entry_ptr ssep)
/*
Allocate and initialize a string literal table for scope stack entry "ssep".
*/
{
  a_string_literal_table_ptr	sltp;

  if (avail_string_literal_tables == NULL) {
    sltp = alloc_fe_of_type(a_string_literal_table);
#if DEBUG
    num_string_literal_tables_allocated++;
#endif /* DEBUG */
  } else {
    sltp = avail_string_literal_tables;
    avail_string_literal_tables = avail_string_literal_tables->next;
  }  /* if */
  memzero((char *)sltp->buckets, size_t_arg(sizeof(sltp->buckets)));
  sltp->next = NULL;
  ssep->string_literal_table = sltp;
}  /* initialize_string_literal_table */


static void free_string_literal_table(a_scope_stack_entry_ptr ssep)
/*
Free the string literal table for the scope stack entry "ssep".
*/
{
  int					i;
  a_string_literal_table_ptr		sltp = ssep->string_literal_table;
  a_string_literal_table_entry_ptr	*buckets = sltp->buckets;

  /* Go through each bucket and free its entries. */
  for (i = 0;  i < STRING_LITERAL_TABLE_SIZE; ++i) {
    if (buckets[i] != NULL) {
      free_list_of_string_literal_table_entries(buckets[i]);
    }  /* if */
  }  /* for */
  /* Return the table to the available list. */
  sltp->next = avail_string_literal_tables;
  avail_string_literal_tables = sltp;
  ssep->string_literal_table = NULL;
}  /* free_string_literal_table */


void f_assign_string_literal_sequence_number(void)
/*
We have just completed scanning the token for a string literal, or have
retrieved such a token from a token cache.  The macro that calls this routine
has determined that we are inside of a function scope for which string
literal sequence numbers are required.  If the constant associated with
the string literal does not yet have a string literal sequence number,
assign one now.
*/
{
  a_constant_ptr		cp = &const_for_curr_token;
  a_scope_stack_entry_ptr	ssep;

  check_assertion(!fetch_pp_tokens);
  check_assertion(cp->kind == (a_constant_repr_kind)ck_string ||
                  cp->kind == (a_constant_repr_kind)ck_error);
  ssep = &scope_stack[depth_innermost_function_scope];
  /* If this is the first string literal in the function, create the hash
     table to be used to ensure that equal string literals (which will share
     their representation) are assigned the same sequence number. */
  if (ssep->string_literal_table == NULL) {
    initialize_string_literal_table(ssep);
  }  /* if */
  if (cp->kind == (a_constant_repr_kind)ck_error) {
    /* An error constant.  No action is needed. */
  } else if (cp->variant.string.sequence_number != 0) {
    /* A sequence number has already been assigned.  No action is needed. */
  } else {
    /* Determine whether we have already assigned a sequence number for
       this string literal by looking up the string in a hash table. */
    int					bucket_number;
    a_hash_value			hash;
    a_string_literal_table_entry_ptr	sltep;
    a_string_literal_table_entry_ptr	*bucket;
    hash = hash_constant(cp);
    bucket_number = (int)(hash % STRING_LITERAL_TABLE_SIZE);
    bucket = &ssep->string_literal_table->buckets[bucket_number];
    for (sltep = *bucket; sltep != NULL; sltep = sltep->next) {
      if (eq_constants(cp, sltep->constant)) break;
    }  /* for */
    if (sltep == NULL) {
      /* No entry was found.  Create one now. */
      sltep = alloc_fe_of_type(a_string_literal_table_entry);
      sltep->constant = alloc_constant((a_constant_repr_kind)ck_string);
      copy_constant(cp, sltep->constant);
      sltep->sequence_number = ++(ssep->string_literal_sequence_number);
      sltep->next = *bucket;
      *bucket = sltep;
#if DEBUG
      num_string_literal_table_entries_allocated++;
#endif /* DEBUG */
    }  /* if */
    cp->variant.string.sequence_number = sltep->sequence_number;
  }  /* if */
}  /* f_assign_string_literal_sequence_number */

#endif /* DO_IL_LOWERING && ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS */

a_function_shareable_constants_table_ptr
alloc_function_shareable_constants_table(void)
/*
Allocate a shareable constants hash table for the innermost function
scope.
*/
{
  a_function_shareable_constants_table_ptr	fsctp;

  if (avail_function_shareable_constants_tables != NULL) {
    fsctp = avail_function_shareable_constants_tables;
    avail_function_shareable_constants_tables = fsctp->next;
    /* The hash table is cleared before it is returned to the available
       list so it need not be initialized here. */
  } else {
    fsctp = alloc_fe_of_type(a_function_shareable_constants_table);
    memzero((char *)fsctp->table, sizeof(fsctp->table));
#if DEBUG
    num_function_shareable_constants_tables_allocated++;
#endif /* DEBUG */
  }  /* if */
  fsctp->next = NULL;
  return fsctp;
}  /* alloc_function_shareable_constants_table */


void free_function_shareable_constants_table(
				a_function_shareable_constants_table_ptr fsctp)
/*
Return a function shareable constant table to the available list.
*/
{
  fsctp->next = avail_function_shareable_constants_tables;
  avail_function_shareable_constants_tables = fsctp;
}  /* free_function_shareable_constants_table */


/*
Certain constructs in C99 inline definitions require a check that cannot be
fully accomplished until the complete translation unit has been seen (because
it isn't known until then whether a function definition is in fact an "inline
definition").  We therefore record suspect constructs in a list of
a_c99_inline_definition_locator nodes for later verification.
*/

typedef struct a_c99_inline_definition_locator
		*a_c99_inline_definition_locator_ptr;

typedef struct a_c99_inline_definition_locator {
  a_c99_inline_definition_locator_ptr
		next;
			/* Pointer to the next record to check. */
  a_routine_ptr
		routine;
			/* The routine in which the static entity was defined
			   or referenced. */
  a_source_position
		position;
			/* The source position of the suspect construct. */
  a_boolean
		static_variable_decl;
			/* TRUE if the suspect construct is a local static
			   variable declaration.  Otherwise, the issue is that
			   a reference to the static entity may be invalid. */
} a_c99_inline_definition_locator;


STATIC_THREAD a_c99_inline_definition_locator_ptr
		c99_inline_definition_locators_to_check;
			/* A pointer to the list of locators for suspect
			   constructs in C99 inline definitions. */

STATIC_THREAD a_c99_inline_definition_locator_ptr
		avail_c99_inline_definition_locators;
			/* A pointer to a list of locators that are no longer
			   in use. */

#if DEBUG
STATIC_THREAD unsigned long
		num_c99_inline_definition_locators_allocated;
#endif /* DEBUG */

void check_c99_inline_definition(a_variable_ptr     var,
                                 a_source_position  *pos)
/*
In C99 it is an error for a modifiable local static variable to be defined in
an "inline definition" (indicated by definition_for_inlining_only).  It is also
an error to reference a file-scope static entity from such a function.  We
cannot tell whether the current routine definition is an "inline definition"
until the end of the translation unit.  So at this time we just record the
position of the suspect construct if an error is still a possibility.  If var
is non-NULL, it represents a local static variable being declared; otherwise,
the suspect construct is a reference to a file-scope static entity.
*/
{
  a_boolean  suspect_construct = TRUE;

  check_assertion(var == NULL || var->storage_class == sc_static);
  if (var != NULL) {
    /* Declarations of local static variables in inline definitions are not a
       problem if the variable is not modifiable. */
    a_type_ptr type = var->type;
    if (is_array_type(type)) {
      /* In C, an array of const element type is not considered const,
         so drop down to the element type to do the test. */
      type = underlying_array_element_type(type);
    }  /* if */
    if (is_const_qualified_type(type)) {
      /* A nonmodifiable local static variable. */
      suspect_construct = FALSE;
    }  /* if */
  }  /* if */
  if (suspect_construct) {
    /* Only record an entry if the current function might be a C99 inline
       definition. */
    a_routine_ptr  rp = innermost_function_scope->variant.routine.ptr;
    check_assertion(rp != NULL);
    if (rp->is_inline && rp->definition_for_inlining_only &&
#if GNU_EXTENSIONS_ALLOWED
        !rp->gnu_c89_inline &&
#endif /* GNU_EXTENSIONS_ALLOWED */
        rp->storage_class == (a_storage_class)sc_unspecified) {
      a_c99_inline_definition_locator_ptr  to_check;
      if (avail_c99_inline_definition_locators == NULL) {
        to_check = alloc_fe_of_type(a_c99_inline_definition_locator);
#if DEBUG
        ++num_c99_inline_definition_locators_allocated;
#endif /* DEBUG */
      } else {
        to_check = avail_c99_inline_definition_locators;
        avail_c99_inline_definition_locators =
                                 avail_c99_inline_definition_locators->next;
      }  /* if */
      to_check->next = c99_inline_definition_locators_to_check;
      c99_inline_definition_locators_to_check = to_check;
      to_check->routine = rp;
      to_check->position = *pos;
      to_check->static_variable_decl = (var != NULL);
    }  /* if */
  }  /* if */
}  /* check_c99_inline_definition */


static void verify_c99_inline_definitions(void)
/*
Traverse the list of suspect constructs in C99 function definitions and issue
a diagnostic if the construct is indeed invalid.  (These constructs are not
allowed in C99 "inline definitions" with external linkage.  Only when the
translation unit has been fully parsed can we establish with certainty that a
function definition is in fact an "inline definition".  This function is
therefore called when the file scope is popped for the first time.)
*/
{
  a_c99_inline_definition_locator_ptr  to_verify, entry =
                                      c99_inline_definition_locators_to_check;

  while (entry != NULL) {
    to_verify = entry;
    if (to_verify->routine->definition_for_inlining_only) {
      an_error_severity  severity;
      an_error_code      code;
      if (strict_ansi_mode) {
        severity = strict_ansi_discretionary_severity;
      } else if (microsoft_mode) {
        /* MSVC does not diagnose these at all.  Issue a remark since it
           is a potential portability issue. */
        severity = es_remark;
      } else if (gcc_mode || clang_mode) {
        severity = es_warning;
      } else {
        severity = es_discretionary_error;
      }  /* if */
      if (to_verify->static_variable_decl) {
        code = ec_static_variable_in_inline_function;
      } else {
        code = ec_bad_linkage_of_ref_within_inline_function;
      }  /* if */
      pos_diagnostic(severity, code, &to_verify->position);
    }  /* if */
    entry = entry->next;
    to_verify->next = avail_c99_inline_definition_locators;
    avail_c99_inline_definition_locators = to_verify;
  }  /* while */
}  /* verify_c99_inline_definitions */


a_scope_pointers_block *get_pointers_block_for_scope(a_scope_ptr scope)
/*
Return the pointers block for the indicated scope, if there is one,
or NULL otherwise.
*/
{
  a_scope_pointers_block *pointers_block = NULL;

  if (scope->kind == (a_scope_kind)sck_file) {
    if (scope == il_header.primary_scope) {
      /* The file scope of the current translation unit. */
      pointers_block = &curr_translation_unit->file_scope_pointers_block;
    } else {
      /* Presumably a file scope of another translation unit.  Look for it. */
      a_translation_unit_ptr tup;
      for (tup = translation_units; ; tup = tup->next) {
        check_assertion_str(tup != NULL,
                         "get_pointers_block_for_scope: file scope not found");
        if (tup->primary_scope == scope) {
          pointers_block = &tup->file_scope_pointers_block;
          break;
        }  /* if */
      }  /* for */
    }  /* if */
  } else if (scope->kind == (a_scope_kind)sck_namespace) {
    /* A namespace scope. */
    a_namespace_ptr nsp = scope->variant.assoc_namespace;
    pointers_block = &symbol_supplement_for_namespace(nsp)->pointers_block;
  } else {
    /* For scopes other than file and namespace, we can get the pointers
       block only if the scope is on the scope stack. */
    a_scope_depth depth = scope->depth_in_scope_stack;

    if (depth != NO_SCOPE_DEPTH) {
      check_assertion_str(trans_unit_for_scope[scope->number] ==
                                                         curr_translation_unit,
                          "get_pointers_block_for_scope: wrong trans unit");
      pointers_block = assoc_pointers_block_of(&scope_stack[depth]);
    }  /* if */
  }  /* if */
  return pointers_block;
}  /* get_pointers_block_for_scope */


a_scope_depth scope_depth_of_symbol(a_symbol_ptr  sym,
                                    a_boolean     *is_local_to_function)
/*
Given a symbol with a decl_scope (which is a scope number), search the
scope stack for the scope stack entry that corresponds to it, and return
the depth.  Also return TRUE in *is_local_to_function if the declaration
is within a function body.
*/
{
  a_scope_depth  scope_depth;

  if (sym->decl_scope == file_scope_number) {
    /* Leave the is_local_to_function flag FALSE. */
    scope_depth = DEPTH_OF_FILE_SCOPE;
  } else if (sym->decl_scope == NO_SCOPE_NUMBER) {
    /* Leave the is_local_to_function flag FALSE. */
    /* Some entities (e.g., macros) have no decl_scope number. */
    scope_depth = NO_SCOPE_DEPTH;
  } else if (is_template_instance_class_symbol(sym) ||
             is_template_variable_symbol(sym) ||
             is_template_alias_instance_symbol(sym)) {
    /* Template classes and aliases can be created at arbitrary times and so
       the scope stack cannot be used to determine the scope depth. */
    scope_depth = NO_SCOPE_DEPTH;
  } else if (sym->decl_scope == scope_stack[decl_scope_level].number) {
    /* The normal case is when the current decl_scope_level corresponds to
       what's in the symbol.  Use the global variables. */
    if (depth_innermost_function_scope != NO_SCOPE_DEPTH ||
        inside_local_class) {
      *is_local_to_function = TRUE;
    }  /* if */
    scope_depth = decl_scope_level;
  } else {
    /* In certain unusual cases (e.g., when an entity is first seen in a
       friend declaration) it is necessary to compute the scope depth by
       running through the scope stack. */
    /*lint --e{446} scope_depth modified in loop (LINTBUG) */
    for (scope_depth = depth_scope_stack; ; --scope_depth) {
      a_scope_kind	kind;
      if (scope_depth < DEPTH_OF_FILE_SCOPE) {
        scope_depth = NO_SCOPE_DEPTH;
        break;
      }  /* if */
      kind = scope_stack[scope_depth].kind;
      if (kind == (a_scope_kind)sck_class_reactivation ||
          kind == (a_scope_kind)sck_namespace_reactivation) {
        /* Ignore class and namespace reactivations. */
        continue;
      } else if (scope_stack[scope_depth].number == sym->decl_scope) {
        /* This is the scope stack entry corresponding to the declaration
           scope number, where relevant characteristics of the scope are
           recorded. */
        if (scope_stack[scope_depth].depth_innermost_function_scope !=
                                                           NO_SCOPE_DEPTH ||
            scope_stack[scope_depth].inside_local_class) {
          *is_local_to_function = TRUE;
        }  /* if */
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return scope_depth;
}  /* scope_depth_of_symbol */


a_boolean namespace_is_enclosed_by_scope(a_symbol_ptr            sym,
                                         a_scope_stack_entry_ptr ssep)
/*
Determine whether the namespace in which sym is defined is enclosed
within the scope specified by ssep.  Return TRUE if it is, FALSE otherwise.
*/
{
  a_boolean	result = FALSE;

  if (ssep->kind == (a_scope_kind)sck_file) {
    /* Everything is enclosed within the file scope. */
    result = TRUE;
  } else if (ssep->kind != (a_scope_kind)sck_namespace &&
             ssep->kind != (a_scope_kind)sck_namespace_extension) {
    /* This is not a namespace scope.  A namespace cannot be enclosed
       within. */
  } else {
    a_namespace_ptr	nsp = parent_namespace_for_symbol(sym);
    if (nsp == NULL) {
      /* The symbol has no associated namespace, and so, is not enclosed
         within the current namespace. */
    } else {
      /* The symbol has a namespace.  See if its namespace, or one of
         its parent namespaces, matches the current namespace. */
      a_namespace_ptr     curr_nsp;
      curr_nsp = ssep->il_scope->variant.assoc_namespace;
      while (curr_nsp != nsp && nsp != NULL) {
        nsp = parent_namespace_or_null(nsp);
      }  /* while */
      if (nsp != NULL) result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* namespace_is_enclosed_by_scope */


static an_active_using_directive_ptr alloc_active_using_directive(void)
/*
Allocate a new active using directive entry, initialize its fields, and
return a pointer to the new entry. Reuse a freed entry if possible.
*/
{
  an_active_using_directive_ptr  audp;

  if (avail_active_using_directives != NULL) {
    /* Reuse a freed entry. */
    audp = avail_active_using_directives;
    avail_active_using_directives = avail_active_using_directives->next;
  } else {
    /* Allocate a new entry. */
    audp = (an_active_using_directive_ptr)
                                  alloc_fe(sizeof(an_active_using_directive));
#if DEBUG
    num_active_using_directives_allocated++;
#endif /* DEBUG */
  }  /* if */
  audp->entry = NULL;
  audp->next  = NULL;
  audp->next_that_applies_at_depth = NULL;
  audp->scope_depth_at_which_using_directive_applies = NO_SCOPE_DEPTH;
  audp->namespace_supplement = NULL;
  audp->effective_decl_seq = 0;
  return audp;
}  /* alloc_active_using_directive */


static a_scope_depth
determine_scope_at_which_using_directive_applies(a_symbol_ptr            sym,
                                                 a_scope_stack_entry_ptr ssep)
/*
Given a symbol that points to a namespace (sym), find the scope at which
names in that namespace should be visible as a consequence of a using
directive.  This is done by going back through the scope stack, starting
with ssep, looking for namespace scopes.
*/
{
  a_scope_depth	depth = NO_SCOPE_DEPTH;
  /* Compute the scope depth associated with the scope stack entry pointer
     passed by the caller. */
  for (; ssep != NULL; ssep = previous_scope_of(ssep)) {
    depth = scope_depth_of(ssep);
    if (ssep->kind == (a_scope_kind)sck_file ||
        ssep->kind == (a_scope_kind)sck_namespace ||
        ssep->kind == (a_scope_kind)sck_namespace_extension ||
        ssep->kind == (a_scope_kind)sck_namespace_reactivation) {
      /* This is a namespace scope (the file scope is treated as a
         namespace scope).  See if it encloses the namespace from
         the using directive. */
      if (namespace_is_enclosed_by_scope(sym, ssep)) break;
    }  /* if */
  }  /* for */
  return depth;
}  /* determine_scope_at_which_using_directive_applies */


/* Forward declarations. */
static void free_list_of_pack_references(a_pack_reference_ptr prp);

static void issue_pack_not_expanded_diagnostics(a_pack_reference_ptr	prp);

static void update_parameter_pack_symbol_values(
			a_pack_expansion_stack_entry_ptr	pesep);

static void add_active_using_directive_to_scope(
				a_using_decl_ptr	udp,
				a_scope_stack_entry_ptr	ssep,
				a_decl_sequence_number	effective_decl_seq);

static void add_active_using_directives_for_scope(
				a_scope_ptr		scope,
				a_scope_stack_entry_ptr	ssep,
				a_decl_sequence_number	parent_decl_seq)
/*
Create active using directive entries for any using directives present
in the specified scope (which is either a namespace scope or the file scope).
If scope is a namespace scope, and was made visible by a using-directive,
parent_decl_seq is the declaration sequence number of the using-directive
that made it visible.
*/
{
  a_using_decl_ptr  udp = scope->using_directives;

  while (udp != NULL) {
    a_decl_sequence_number	effective_decl_seq;
    check_assertion(udp->is_using_directive);
    /* A using-directive.  Use the higher of the parent's declaration
       sequence number or the one associated with the using directive
       now being processed.  If the namespace that nominated this namespace
       should not be visible then neither should this one. */
    effective_decl_seq = (a_decl_sequence_number)
                         (parent_decl_seq > udp->decl_sequence_number ?
                                 parent_decl_seq : udp->decl_sequence_number);
    add_active_using_directive_to_scope(udp, ssep, effective_decl_seq);
    udp = udp->next;
  }  /* while */
}  /* add_active_using_directives_for_scope */


static void add_active_using_directive_to_scope(
				a_using_decl_ptr	udp,
				a_scope_stack_entry_ptr	ssep,
				a_decl_sequence_number	effective_decl_seq)
/*
Allocate a new active using directive entry, initialize its fields, and
link it into a list of active using directives for the current scope.
Reuse a freed entry if possible.  If the specified namespace is already
on the list for the scope, a new entry is not added.  effective_decl_seq
is the declaration sequence point at which the using-directive is
effective.  This is used during template instantiations to ignore
using-directives specified after the point of definition of the template.
*/
{
  an_active_using_directive_ptr  	audp;
  a_namespace_ptr		 	nsp;
  a_symbol_ptr			 	ns_sym;
  a_scope_depth			 	new_depth;
  a_namespace_symbol_supplement_ptr	nssp;

  check_assertion(udp->entity.kind == iek_namespace);
  /* Get a pointer to the namespace to be used. */
  nsp = skip_namespace_aliases((a_namespace_ptr)udp->entity.ptr);
  ns_sym = (a_symbol_ptr)nsp->source_corresp.assoc_info;
  nssp = ns_sym->variant.namespace_info.extra_info;
  /* Determine the depth at which this using directive applies. */
  new_depth = determine_scope_at_which_using_directive_applies(ns_sym, ssep);
  /* Determine whether this namespace is already on the active using list
     for this scope. */
  audp = ssep->active_using_directives;
  for (; audp != NULL; audp = audp->next) {
    a_namespace_ptr  nsp2 = (a_namespace_ptr)audp->entry->entity.ptr;
    if (skip_namespace_aliases(nsp2) == nsp) break;
  }  /* for */
  if (audp == NULL) {
    /* Add the using directive to the active list for this scope. */
    audp = alloc_active_using_directive();
    audp->entry = udp;
    audp->namespace_supplement = nssp;
    audp->next = ssep->active_using_directives;
    audp->scope_depth_at_which_using_directive_applies = new_depth;
    audp->effective_decl_seq = effective_decl_seq;
    ssep->active_using_directives = audp;
#if DEBUG
    if (db_flag_is_set("using_dir")) {
      fprintf(f_debug,
              "adding using-dir at depth %d for namespace %s applies at %d",
              (int)scope_depth_of(ssep),
              nssp->symbol->variant.namespace_info.ptr->source_corresp.name,
              (int)new_depth);
      fprintf(f_debug, ", decl_seq %lu\n",
              (unsigned long)effective_decl_seq);
    }  /* if */
#endif /* DEBUG */
    /* Add the using directive to the list for scope at which this using
       directive applies. */
    audp->next_that_applies_at_depth =
                       scope_stack[new_depth].using_directives_that_apply_here;
    scope_stack[new_depth].using_directives_that_apply_here = audp;
    /* Add active using directives for the namespaces that should be
       visible because of the transitivity of using directives. */
    add_active_using_directives_for_scope(nsp->variant.assoc_scope, ssep,
                                          effective_decl_seq);
    /* Now that a using directive is active, inactive symbols may be
       visible. */
    scope_stack[depth_scope_stack].inactive_symbols_may_be_visible = TRUE;
  } else {
    /* An active using-directive entry already exists for this namespace.
       If the effective declaration sequence number of this entry is less
       than the existing entry, update the existing entry. */
    if (audp->effective_decl_seq > effective_decl_seq) {
      audp->effective_decl_seq = effective_decl_seq;
      /* Once again go through the using-directives that should be visible
         transitively and update their effective declaration sequence. */
      add_active_using_directives_for_scope(nsp->variant.assoc_scope, ssep,
                                            effective_decl_seq);
    }  /* if */
  }  /* if */
  /* Record the lowest declaration sequence number associated with
     this using-directive.  This is used to emulate the instantiation
     lookup used by g++. */
  if (nssp->using_dir_decl_seq == NO_DECL_SEQUENCE_NUMBER ||
      audp->effective_decl_seq < nssp->using_dir_decl_seq) {
    nssp->using_dir_decl_seq = audp->effective_decl_seq;
  }  /* if */
}  /* add_active_using_directive_to_scope */


void add_active_using_directive(a_using_decl_ptr udp,
				a_scope_depth    depth)
/*
Add a new active using directive entry to the scope specified by depth.
*/
{
  a_scope_stack_entry_ptr	ssep = &scope_stack[depth];

  add_active_using_directive_to_scope(udp, ssep,
                            (a_decl_sequence_number)udp->decl_sequence_number);
  if (ssep->kind == (a_scope_kind)sck_namespace ||
      ssep->kind == (a_scope_kind)sck_namespace_extension) {
    a_namespace_ptr	namespace_added_to;
    /* When a using directive is added to a namespace scope, we need to
       go through any previous scope stack entries to see if they reference
       the enclosing namespace.  If so, the new using directive, and any
       new namespaces transitively referenced by the new using directive,
       must be added to the previous scope stack entries. */
    /* Note: This code does not need to deal with scopes being skipped
       as a consequence of instantiation scopes because there is no way that
       a using directive can be added to a namespace scope as a consequence
       of a template instantiation. */
    namespace_added_to = ssep->il_scope->variant.assoc_namespace;
    namespace_added_to = skip_namespace_aliases(namespace_added_to);
    for (;; ssep--) {
      an_active_using_directive_ptr	audp;
      a_namespace_ptr                   audp_nsp;

      /* Look for namespace_added_to on the list of active using directives for
         this scope. */
      audp = ssep->active_using_directives;
      for (; audp != NULL; audp = audp->next) {
        audp_nsp = (a_namespace_ptr)audp->entry->entity.ptr;
        if (skip_namespace_aliases(audp_nsp) == namespace_added_to) {
          /* The enclosing namespace is on the list.  Add the using directive
             to this scope. */
          add_active_using_directive_to_scope(udp, ssep, decl_seq_counter);
          break;
        }  /* if */
      }  /* for */
      if (ssep->kind == (a_scope_kind)sck_file) break;
    }  /* for */
  }  /* if */
}  /* add_active_using_directive */


static
void free_active_using_directive_list(an_active_using_directive_ptr audp)
/*
Free the specified list of active using directives to the available list.
*/
{
  an_active_using_directive_ptr	last_audp = audp;

  /* Find the end of the list. */
  while (last_audp->next != NULL) last_audp = last_audp->next;
  /* Add the current available list to the end of this one.  Set the
     pointer to the start of the available list to the start of the
     list passed by the caller. */
  last_audp->next = avail_active_using_directives;
  avail_active_using_directives = audp;
}  /* free_active_using_directive_list */


/*
The name-linkage stack is used to save and restore the name-linkage state
for the currently active scopes.  a_name_linkage_stack_entry is the type of
each entry on the name-linkage stack.
*/
typedef struct a_name_linkage_stack_entry *a_name_linkage_stack_entry_ptr;
typedef struct a_name_linkage_stack_entry {
  a_name_linkage_stack_entry_ptr
		next;
			/* Next in name-linkage stack or available list;
			   NULL for last entry. */
  a_name_linkage_kind
		saved_name_linkage;
			/* Saved value of default_name_linkage for the
			   current scope. */
  a_byte_boolean
		saved_name_linkage_is_explicit;
			/* Saved value of name_linkage_is_explicit for the
			   current scope. */
} a_name_linkage_stack_entry;


STATIC_THREAD a_name_linkage_stack_entry_ptr
		name_linkage_stack;
			/* Linked list of entries recording saved name-linkage
			   states from the currently active scopes. */

STATIC_THREAD a_name_linkage_stack_entry_ptr
		avail_name_linkage_stack_entries;
			/* Freed name-linkage-stack entries that are available
			   for reuse. */

void push_name_linkage(a_name_linkage_kind  kind)
/*
Set the name-linkage field in the scope stack to "kind" and save its current
value.  The value will be restored when pop_name_linkage is called.
*/
{
  a_name_linkage_stack_entry_ptr  nlsep;
  a_scope_stack_entry_ptr         ssep = &scope_stack[depth_scope_stack];

  /* Allocate a new name-linkage-stack entry if necessary. */
  if (avail_name_linkage_stack_entries == NULL) {
    nlsep = (a_name_linkage_stack_entry_ptr)alloc_fe(
                                   sizeof(a_name_linkage_stack_entry));
  } else {
    nlsep = avail_name_linkage_stack_entries;
    avail_name_linkage_stack_entries = nlsep->next;
  }  /* if */
  /* Save the values for the current scope. */
  nlsep->saved_name_linkage =
                    enum_cast<a_name_linkage_kind>(ssep->default_name_linkage);
  nlsep->saved_name_linkage_is_explicit = ssep->name_linkage_is_explicit;
  /* Add the new entry to the name linkage stack. */
  nlsep->next = name_linkage_stack;
  name_linkage_stack = nlsep;
  /* Reset the values in the current scope. */
  ssep->default_name_linkage = kind;
  ssep->name_linkage_is_explicit = TRUE;
}  /* push_name_linkage */


void pop_name_linkage(void)
/*
Restore to the scope stack the name linkage state at the point of the
corresponding call to push_name_linkage.
*/
{
  a_name_linkage_stack_entry_ptr  nlsep = name_linkage_stack;
  a_scope_stack_entry_ptr         ssep = &scope_stack[depth_scope_stack];

  check_assertion(nlsep != NULL);
  /* Restore to the current scope the values saved in the name-linkage
     stack. */
  ssep->default_name_linkage = nlsep->saved_name_linkage;
  ssep->name_linkage_is_explicit = nlsep->saved_name_linkage_is_explicit;
  /* Pop the entry from the name linkage stack and return it to the
     available list. */
  name_linkage_stack = nlsep->next;
  nlsep->next = avail_name_linkage_stack_entries;
  avail_name_linkage_stack_entries = nlsep;
}  /* pop_name_linkage */


a_boolean current_class_symbol_if_class_template(a_symbol_ptr *sym)
/*
If the symbol is a class template that is currently being instantiated,
or if a specific definition of the class is being defined, the symbol of
the instantiation (or the specific definition) is returned in *sym,
otherwise the original symbol is left unchanged.  If the symbol returned
is not a class template symbol (either because the symbol passed by the
caller was not a class template or because we succeeded in finding an
instantiation or definition) we return TRUE.  If the symbol is a class
template with no current instantiation or definition, we return FALSE.
*/
{
  a_boolean      found = TRUE;
  a_boolean      is_instantiation_scope;
  a_symbol_ptr   instance_sym = NULL;

  if (is_injected_template_symbol(*sym)) {
    /* The symbol is the injected name of a class template.  Substitute
       the symbol of the associated class template. */
    *sym = class_template_for_injected_template_symbol(*sym);
  }  /* if */
  if ((*sym)->kind == (a_symbol_kind)sk_class_template) {
    found = FALSE;
    /* We can skip the lookup if there are no class scopes (including
       reactivation scopes) or instantiation scopes on the stack. */
    if ((num_classes_on_scope_stack > 0) ||
        (depth_innermost_instantiation_scope != NO_SCOPE_DEPTH)) {
      /* Loop through the scope stack looking at the instantiation scopes
         and the class declaration and reactivation scopes.  Stop after
         finding the first instantiation scope.  Check each of these
         scopes to see if the associated type is a template class associated
         with the class template symbol. */
      a_scope_stack_entry_ptr ssep;
      for (ssep = &scope_stack[depth_scope_stack];
           ssep != NULL; ssep = previous_scope_of(ssep)) {
        is_instantiation_scope =
                        ssep->kind == (a_scope_kind)sck_template_instantiation;
        if (is_instantiation_scope ||
            ssep->kind == (a_scope_kind)sck_class_struct_union ||
            ssep->kind == (a_scope_kind)sck_class_reactivation) {
          a_template_symbol_supplement_ptr	tssp;
          tssp = (*sym)->variant.template_info;
          /* Get the instance symbol pointed to by the type from the scope
             stack entry. */
          if (!is_instantiation_scope ||
              ((tssp->is_generic ||
                (microsoft_bugs && microsoft_version < 1400)) &&
               ssep->assoc_type != NULL)) {
            /* Normally, we look for a class or class reactivation scope for
               a class that is an instance of the template specified by "sym".
               The Microsoft compiler (when microsoft_version < 1400) also
               accepts a class name as being the "current instantiation" when
               referenced in the base class list of the class.  This is also
               the case for base classes of C++/CLI generics.  When the base
               classes are permitted to reference the current instantiation,
               we also check the class associated with a template
               instantiation scope. */
            a_symbol_ptr	template_sym;
            check_assertion_str(ssep->assoc_type != NULL,
				"ccsict: assoc_type is NULL");
            instance_sym = (a_symbol_ptr)(ssep->assoc_type->
                                                    source_corresp.assoc_info);
            /* A class/struct/union scope or reactivation scope.  Get the
               template from which this instantiation was generated.  If it
               is a partial specialization, get the primary template. */
            template_sym = instance_sym->variant.class_struct_union.
                                                    extra_info->class_template;
            if (template_sym != NULL) {
              template_sym = primary_template_of(template_sym);
            }  /* if */
            if (template_sym == *sym) {
              found = TRUE;
              break;
            }  /* if */
          }  /* if */
          /* Don't look beyond the innermost instantiation scope that
             is not a nested instantiation. */
          if (is_instantiation_scope && !ssep->nested_instantiation) break;
        }  /* if */
      }  /* for */
      if (found) *sym = instance_sym;
    }  /* if */
  }  /* if */
  return found;
}  /* current_class_symbol_if_class_template */


a_boolean scope_of_class_is_active(a_type_ptr  tp)
/*
Return TRUE if the class scope for the given class type is active on the scope
stack.
*/
{
  a_scope_stack_entry_ptr  ssep = &scope_stack_top();

  for (; ssep != NULL; ssep = previous_scope_of(ssep)) {
    if (scope_is(ssep, sck_class_struct_union) ||
        scope_is(ssep, sck_class_reactivation)) {
      if (ssep->assoc_type == tp) break;
    }  /* if */
  }  /* for */
  return ssep != NULL;
}  /* scope_of_class_is_active */


static void update_template_param_symbol(a_symbol_ptr		param_symbol,
                                         a_template_arg_ptr	tap)
/*
Update param_symbol to reflect the value specified by tap.
*/
{
  switch (tap->kind) {
    case tak_type:
      check_assertion(param_symbol->kind == (a_symbol_kind)sk_type);
      param_symbol->variant.type.ptr = tap->variant.type;
      break;
    case tak_template:
      /* A template template argument. */
      { a_template_symbol_supplement_ptr  param_tssp;

        check_assertion(param_symbol->kind == sk_class_template);
        /* Unlike the type and nontype cases, the symbol for a template
         template parameter is not updated directly.  Instead, the
         argument_template field of the template supplement is updated
         to point to the template that is to be used as the actual
         argument. */
        param_tssp = param_symbol->variant.template_info;

        a_template_ptr tap_templ = tap->variant.templ.ptr;
        a_symbol_ptr   tap_templ_sym = symbol_for(tap_templ);
        check_assertion(tap_templ_sym != NULL);
        param_tssp->variant.class_template.argument_template = tap_templ_sym;
        param_tssp->variant.class_template.substituted_param_template =
                                 tap->variant.templ.substituted_param_template;
      }
      break;
    case tak_nontype:
      check_assertion(param_symbol->kind == (a_symbol_kind)sk_constant);
      param_symbol->variant.constant = tap->variant.constant;
      break;
    case tak_start_of_pack_expansion:
      break;
    default:
      unexpected_condition();
      break;
  }  /* switch */
}  /* update_template_param_symbol */


static a_boolean template_arg_can_update_param_symbol(a_template_arg_ptr  tap)
/*
Return TRUE if tap has a value that can be used to update a template parameter
symbol.  Missing entries can occur in deduction and partial-ordering argument
lists that are not complete instantiation argument lists.
*/
{
  a_boolean  result = TRUE;

  if (is_type_templ_arg(tap)) {
    result = tap->variant.type != NULL;
  } else if (is_nontype_templ_arg(tap)) {
    result = tap->is_array_bound_of_unknown_type ||
             tap->variant.constant != NULL;
  } else if (is_template_templ_arg(tap)) {
    result = tap->variant.templ.ptr != NULL;
  } else {
    check_assertion(is_start_of_pack_expansion_templ_arg(tap));
  }  /* if */
  return result;
}  /* template_arg_can_update_param_symbol */


static void set_template_param_symbol_to_error(a_symbol_ptr	param_symbol)
/*
Set param_symbol to refer to an error value.
*/
{
  switch (param_symbol->kind) {
    case sk_type:
      param_symbol->variant.type.ptr = error_type();
      break;
    case sk_class_template:
      /* A template template argument. */
      { a_template_symbol_supplement_ptr	param_tssp;
        a_symbol_ptr				error_ct_sym;
        error_ct_sym = error_class_template();
        param_tssp = param_symbol->variant.template_info;
        param_tssp->variant.class_template.argument_template = error_ct_sym;
        param_tssp->variant.class_template.substituted_param_template =
                       error_ct_sym->variant.template_info->il_template_entry;
      }
      break;
    case sk_constant:
      { a_type_ptr	orig_type = param_symbol->variant.constant->type;
        param_symbol->variant.constant =
                                   fs_constant((a_constant_repr_kind)ck_error);
        param_symbol->variant.constant->type = orig_type;
      }
      break;
    default:
      unexpected_condition();
      break;
  }  /* switch */
}  /* set_template_param_symbol_to_error */


void update_template_param_symbols(a_template_param_ptr param_list,
                                   a_template_arg_ptr   arg_list,
                /* Defaulted: */   a_boolean            partial_argument_list)
/*
Bind template parameter symbols to arg_list.  When partial_argument_list is
FALSE (the usual case, including for template instantiation scopes on
push_scope and pop_scope), mark every parameter not visible, apply arguments,
coerce missing slots to errors, and clear not_visible per parameter.  When
partial_argument_list is TRUE, the list is incomplete for this parameter list:
skip the initial not_visible pass and error coercion.
*/
{
  a_template_arg_ptr    tap = arg_list;
  a_template_param_ptr	tpp;

  db_enter(4, "update_template_param_symbols");

  if (!partial_argument_list) {
    /* Initially mark all of the parameters as not visible. */
    for (tpp = param_list; tpp != NULL; tpp = tpp->next) {
      tpp->param_symbol->template_param_not_visible = TRUE;
    }  /* for */
  }  /* if */
  /* Loop through the parameters and arguments.  There may be fewer
     template arguments than parameters when push_scope is done while
     scanning the template argument list of a template class reference.
     The "special" versions of the traversal routines are used so that we
     access all parameters, even variadic ones for which there are no
     elements. */
  begin_special_variadic_template_arg_list_traversal(param_list,
                                                     arg_list, &tpp, &tap);
  for (; tap != NULL;
         special_variadic_advance_to_next_template_arg(&tpp, &tap)) {
    a_template_arg_ptr	tap_to_update = tap;
    /* If a start of pack expansion entry is returned, only update the
       symbol if there are associated pack elements. */
    if (tap_to_update != NULL &&
        is_start_of_pack_expansion_templ_arg(tap_to_update)) {
      tap_to_update = tap_to_update->next;
      if (tap_to_update == NULL ||
          (!tap_to_update->is_pack_element && tpp->is_pack)) {
        /* There is no argument for this parameter.  Set the parameter
           symbol to point to an error value. */
        tap_to_update = NULL;
        if (tpp == NULL) break;
        if (!partial_argument_list) {
          set_template_param_symbol_to_error(tpp->param_symbol);
        }  /* if */
      } else {
        /* The argument is a placeholder but the parameter is not a pack.
           Unless this is a placeholder, advance "tap" so that we will skip
           to the next parameter above. */
        if (is_start_of_pack_expansion_templ_arg(tap_to_update)) {
          tap_to_update = NULL;
        } else {
          tap = tap_to_update;
        }  /* if */
      }  /* if */
    }  /* if */
    if (tpp == NULL) {
      expect_error();
      break;
    }  /* if */
    if (tap_to_update != NULL && partial_argument_list &&
        !template_arg_can_update_param_symbol(tap_to_update)) {
      tap_to_update = NULL;
    }  /* if */
    if (tap_to_update != NULL) {
      /* A template argument exists for this parameter. */
      update_template_param_symbol(tpp->param_symbol, tap_to_update);
      tpp->param_symbol->template_param_not_visible = FALSE;
    } else if (!partial_argument_list) {
      tpp->param_symbol->template_param_not_visible = FALSE;
    }  /* if */
  }  /* for */
  db_exit();
}  /* update_template_param_symbols */


void update_template_param_symbols_for_param_list(a_template_param_ptr	tpp)
/*
Search through the scope stack looking for a previous instantiation scope that
uses the same template parameter list as the one specified by tpp, and update
the associated template parameter symbols to refer to the arguments from that
prior instantiation.
*/
{
  a_scope_depth	scope_depth;

  for (scope_depth = depth_scope_stack; scope_depth >= 0; scope_depth--) {
    if (scope_is(&scope_stack[scope_depth], sck_template_instantiation) &&
        scope_stack[scope_depth].template_decl_info->parameters == tpp) {
      break;
    }  /* if */
  }  /* for */
  if (scope_depth != NO_SCOPE_DEPTH) {
    /* Restore the parameter values from the previous instantiation. */
    update_template_param_symbols(tpp,
                                  scope_stack[scope_depth].template_arg_list);
  }  /* if */
}  /* update_template_param_symbols_for_param_list */


static void restore_default_template_param(a_template_param_ptr tpp)
/*
Update the symbol entry for the template parameter specified by tpp to its
"resting value".  This is the initial value supplied when the template
declaration was scanned and is used as a placeholder between instantiations
and also during rescans for template parameter packs that should be
preserved in the substituted type.
*/
{
  a_symbol_ptr	param_symbol = tpp->param_symbol;
  
  if (param_symbol->kind == (a_symbol_kind)sk_type) {
    param_symbol->variant.type.ptr = tpp->variant.type;
  } else if (param_symbol->kind == (a_symbol_kind)sk_constant) {
      param_symbol->variant.constant = tpp->variant.constant.ptr;
  } else {
    a_template_symbol_supplement_ptr	param_tssp;
    check_assertion(param_symbol->kind == (a_symbol_kind)sk_class_template);
    param_tssp = param_symbol->variant.template_info;
    param_tssp->variant.class_template.argument_template = param_symbol;
    param_tssp->variant.class_template.substituted_param_template = NULL;
  }  /* if */
  param_symbol->template_param_not_visible = FALSE;
}  /* restore_default_template_param */


void restore_default_template_params(a_template_param_ptr  tpp,
                                     a_boolean             packs_only)
/*
Update the symbol entries for the template parameter list specified by tpp
to their "resting values".  If packs_only is TRUE, only template parameters
for parameter packs are reset.
*/
{
  db_enter(4, "restore_default_template_params");
  /* Loop through the parameters and set them to the original dependent
     type, constant, or template value. */
  for (; tpp != NULL; tpp = tpp->next) {
    if (!packs_only || tpp->is_pack) {
      restore_default_template_param(tpp);
    }  /* if */
  }  /* for */
  db_exit();
}  /* restore_default_template_params */


static void reset_enclosing_pack_values(void)
/*
Go through the scope stack and reset the template parameters associated
with any enclosing variadic classes to their original (dependent)
values.  See the call in begin_potential_pack_expansion_context_full
for more information about when this is done.
*/
{
  a_scope_stack_entry_ptr	ssep;

  /* Go through any visible template instantiation scopes on the scope
     stack and reset the values of their template parameters to their
     original value. */
  for (ssep = scope_stack_entry_for(depth_innermost_instantiation_scope);
       ssep != NULL; ssep = previous_scope_of(ssep)) {
    if (scope_is(ssep, sck_template_instantiation) &&
        ssep->in_variadic_template) {
      restore_default_template_params(ssep->template_decl_info->parameters,
                                      /*packs_only=*/TRUE);
    }  /* if */
  }  /* for */
}  /* reset_enclosing_pack_values */


a_boolean reset_enclosing_packs_for_pack_index(
		a_pack_expansion_stack_entry_ptr	pesep)
/*
For a pack-index construct (C++26 "pack...[index]") scanned during the rescan
of a template declaration nested inside a real variadic instantiation, reset
the enclosing pack parameter symbols back to their dependent values.  Return
TRUE if the reset was done.
*/
{
  a_boolean  did_reset = FALSE;

  if (pesep != NULL && pesep->instantiation_descr != NULL &&
      !pesep->enclosing_packs_reset &&
      is_uninstantiated_pack_expansion_context()) {
    reset_enclosing_pack_values();
    pesep->enclosing_packs_reset = TRUE;
    did_reset = TRUE;
  }  /* if */
  return did_reset;
}  /* reset_enclosing_packs_for_pack_index */


static void restore_enclosing_pack_values(a_boolean process_enclosing_scopes)
/*
Restore the pack values of the current pack expansion (after another
pack expansion has completed).  If process_enclosing_scopes is TRUE
go through the scope stack and restore the template parameters associated
with any enclosing packs to their appropriate values (after having been
changed by a call of reset_enclosing_pack_values).
*/
{
  a_scope_stack_entry_ptr	ssep;
  a_boolean			first = TRUE;

  /* Go through any visible template instantiation scopes on the scope
     stack and restore the template parameters to refer to the actual
     template arguments. */
  for (ssep = scope_stack_entry_for(depth_innermost_instantiation_scope);
       ssep != NULL; ssep = previous_scope_of(ssep)) {
    if (scope_is(ssep, sck_template_instantiation) &&
        ssep->in_variadic_template) {
      a_pack_expansion_stack_entry_ptr	pesep;
      update_template_param_symbols(ssep->template_decl_info->parameters,
                                    ssep->template_arg_list);
      /* For the top instantiation scope, use the current value of the
         pack_expansion_stack variable.  For subsequent entries, use
         the value from the scope stack. */
      if (first) {
        pesep = pack_expansion_stack;
        first = FALSE;
      } else {
        pesep = ssep->pack_expansion_stack;
      }  /* if */
      if (pesep != NULL && pesep->instantiation_descr != NULL) {
        update_parameter_pack_symbol_values(pesep);
      }  /* if */
      if (!process_enclosing_scopes) break;
    }  /* if */
  }  /* for */
}  /* restore_enclosing_pack_values */


void set_active_using_list_scope_depths(
				a_scope_depth		starting_depth,
                                a_boolean		set_value,
				a_decl_sequence_number	effective_decl_seq)
/*
This routine goes through the active using list for the scopes that
are now active and updates the scope at which the using directive
applies for each of the namespaces referenced.  If set_value is TRUE,
the flag is set to the value specified in the active using directive
entry.  If set_value is FALSE the flag is set to NO_SCOPE_DEPTH.
starting_depth is the innermost scope to be processed.

If effective_decl_seq is not NO_DECL_SEQUENCE_NUMBER, it specifies a
point after which any symbols that are declared should not be visible. 
This is used during template instantiations to ignore using-directives
specified after the point of definition of the template.
*/
{
  a_scope_stack_entry_ptr	ssep;

  if (set_value) {
    /* Clear the list of using-directives that apply for each scope.  This
       is done to permit this routine to be called more than once. */
    for (ssep = &scope_stack[starting_depth]; ssep != NULL;
         ssep = previous_scope_of(ssep)) {
      ssep->using_directives_that_apply_here = NULL;
    }  /* for */
  }  /* if */
  /* Go through the list of scopes.  When setting the flags, use the
     list of scopes linked by the previous scope pointer.  When clearing
     the flags, consider all scopes. */
  for (ssep = &scope_stack[starting_depth]; ssep != NULL;
       ssep = previous_scope_of(ssep) ) {
    an_active_using_directive_ptr	audp = ssep->active_using_directives;
    /* Set the flag for any active using directives for this scope. */
    for (; audp != NULL; audp = audp->next) {
      a_namespace_symbol_supplement_ptr	nssp;
      a_scope_depth			new_depth;
      /* Note that this is done in all modes, not just when doing dependent
         name processing, so that using-directives from after the definition
         of a template are not considered. */
      if (set_value &&
          ssep->kind != (a_scope_kind)sck_block &&
          ssep->kind != (a_scope_kind)sck_function &&
          (!gpp_using_directive_lookup &&
           (effective_decl_seq != NO_DECL_SEQUENCE_NUMBER &&
           (audp->effective_decl_seq > effective_decl_seq)))) {
        /* This using-directive became effective after the point that the
           using-directive appeared.  Ignore this using-directive.  This
           test is ignored for block scope using directives, because any
           ones that are on the list are visible, and the declaration sequence
           number test will fail for these declarations.  In
           gpp_using_directive_lookup mode, the using-directive is made active,
           but certain symbols are ignored later. */
        continue;
      }  /* if */
      nssp = audp->namespace_supplement;
      if (set_value) {
        new_depth = audp->scope_depth_at_which_using_directive_applies;
      } else {
        new_depth = NO_SCOPE_DEPTH;
      }  /* if */
#if DEBUG
      if (db_flag_is_set("using_dir")) {
        fprintf(f_debug,
                "%s using-dir at depth %d for namespace %s applies at %d",
                set_value ? "setting" : "clearing",
                (int)scope_depth_of(ssep),
                nssp->symbol->variant.namespace_info.ptr->source_corresp.name,
                (int)new_depth);
        fprintf(f_debug, ", decl_seq %lu\n",
                (unsigned long)effective_decl_seq);
      }  /* if */
#endif /* DEBUG */
      /* Record the scope depth of the innermost active using directive for
         this namespace.  If we are clearing the flags, reset this depth
         to NO_SCOPE_DEPTH. */
      if (set_value) {
        /* Record the lowest declaration sequence number associated with
           this using-directive.  This is used to emulate the instantiation
           lookup used by g++. */
        if (nssp->using_dir_decl_seq == NO_DECL_SEQUENCE_NUMBER ||
            audp->effective_decl_seq < nssp->using_dir_decl_seq) {
          nssp->using_dir_decl_seq = audp->effective_decl_seq;
        }  /* if */
        /* Add the using directive to the list for scope at which this using
           directive applies. */
        audp->next_that_applies_at_depth =
                       scope_stack[new_depth].using_directives_that_apply_here;
        scope_stack[new_depth].using_directives_that_apply_here = audp;
      } else {
        nssp->using_dir_decl_seq = NO_DECL_SEQUENCE_NUMBER;
      }  /* if */
    }  /* for */
    /* If we are clearing the flags, clear the list for this scope that
       indicates the using directives that must be processed when this
       scope is reached. */
    if (!set_value) ssep->using_directives_that_apply_here = NULL;
  }  /* for */
}  /* set_active_using_list_scope_depths */

#if GENERATE_SOURCE_SEQUENCE_LISTS

static a_boolean prototype_inst_is_for_class_in_real_instance(
                                                        a_type_ptr  proto_type)
/*
This is a helper function for push_scope_full.  A template instantiation scope
is being pushed, and it is in the context of a prototype instantiation.  If the
instantiation is that of a class template or a class nested in a template,
proto_type will point to the associated prototype instantiation; otherwise,
proto_type will be NULL.  Return TRUE if proto_type is non-NULL and the
prototype instantiation is the result of the real instantiation of a class
template.  Otherwise, return FALSE.  For example, if X<T>::N describes a
template, X<int>::N<U> is a prototype instantiation of the nested template
X<T>::N inside the real instantiation X<int>.  For purposes of this test,
Microsoft instantiated nonreal classes are considered to be real classes.
*/
{
  a_boolean  result = FALSE;

  check_assertion(scope_stack_top().in_prototype_instantiation ||
                  scope_stack_top().in_generic_definition);
  if (proto_type != NULL &&
      is_prototype_instantiation_or_cli_generic_type(proto_type)) {
    /* The prototype instantiation is for a class. */
    a_scope_ptr  parent_scope = get_parent_scope_of(proto_type);
    a_template_symbol_supplement_ptr
                 tssp = template_supplement_for_symbol(symbol_for(proto_type));
    if (tssp != NULL && tssp->is_specific_definition) {
      /* proto_type is the prototype instantiation for a member template that
         is being specialized.  E.g.:
           template<class> struct A { template<class> struct B; };
           template<> template<class U> struct A <int>::B { };
         This is not treated as a prototype instantiation resulting from a
         real instance. */
    } else {
       /* Check if a parent class is a real template instance. */
      while (parent_scope != NULL &&
             parent_scope->kind == (a_scope_kind)sck_class_struct_union) {
        a_type_ptr  class_type = parent_scope->variant.assoc_type;
        if (class_type->variant.class_struct_union.is_template_class &&
            (!class_type->variant.class_struct_union.is_nonreal_class ||
             class_type->
                variant.class_struct_union.is_ms_instantiated_nonreal_class) &&
#if MICROSOFT_EXTENSIONS_ALLOWED
            !class_type->variant.class_struct_union.is_generic_definition &&
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
            !class_type->variant.class_struct_union.is_specialized) {
          /* A parent scope corresponding to a real class template
             instance. */
          result = TRUE;
          break;
        }  /* if */
        parent_scope = class_type->source_corresp.parent_scope;
      }  /* while */
    }  /* if */
  }  /* if */
  return result;
}  /* prototype_inst_is_for_class_in_real_instance */

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

void clear_scope_pointers_block(a_scope_pointers_block_ptr  spbp)
/*
Initialize the fields in a scope-pointers-block substructure.
*/
{
  spbp->symbols                      = NULL;
  spbp->synth_namespace_projection_symbols
                                     = NULL;
  spbp->last_symbol                  = NULL;
  spbp->last_constant                = NULL;
  spbp->last_type                    = NULL;
  spbp->last_variable                = NULL;
  spbp->last_routine                 = NULL;
  spbp->last_asm_entry               = NULL;
  spbp->last_dynamic_init            = NULL;
  spbp->last_namespace               = NULL;
  spbp->last_using_declaration       = NULL;
  spbp->last_using_directive         = NULL;
  spbp->last_pragma                  = NULL;
#if RECORD_HIDDEN_NAMES_IN_IL
  spbp->last_hidden_name             = NULL;
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
  spbp->last_template                = NULL;
  spbp->unnamed_namespace_sym        = NULL;
  spbp->inline_namespaces            = NULL;
  spbp->add_symbols_to_inactive_list = FALSE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  spbp->last_source_sequence_entry   = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if MICROSOFT_EXTENSIONS_ALLOWED
  spbp->last_ms_attribute             = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  spbp->lookup_table                  = NULL;
  spbp->module_lookup_table_map       = NULL;
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
  spbp->last_ms_if_exists             = NULL;
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
}  /* clear_scope_pointers_block */


static void set_parent_scope_on_push(a_scope_stack_entry_ptr  ssep)
/*
The given entry is being pushed onto the scope stack, and an IL scope has just
been allocated for it.  Set the parent pointer for the IL scope entry.
*/
{
  a_scope_ptr  sp = ssep->il_scope;

  if (C_mode()) {
    sp->parent = scope_stack[decl_scope_level].il_scope;
    check_assertion(sp->parent != NULL);
    if (in_file_scope(sp) && !in_file_scope(sp->parent)) {
      /* A memory region constraint violation.  Use the make_local_scope_ref
         mechanism as a work-around. */
      check_assertion(innermost_function_scope != NULL);
      make_local_scope_ref(sp->parent, (char*)sp, iek_scope,
                           innermost_function_scope);
      sp->parent = NULL;
    }  /* if */
    goto done;
  }  /* if */
  switch (sp->kind) {
    case sck_function:
      { a_source_correspondence_ptr  scp = &ssep->assoc_routine
                                                ->source_corresp;
        if (scp->is_class_member) {
          sp->parent = class_type_supp(scp_parent_class(scp))->assoc_scope;
        } else if (scp_is_namespace_member(scp)) {
          sp->parent = scp_parent_namespace(scp)->variant.assoc_scope;
        } else {
          sp->parent = scope_stack[DEPTH_OF_FILE_SCOPE].il_scope;
        }  /* if */
      }
      break;
    case sck_namespace:
      { a_source_correspondence_ptr  scp = &ssep->assoc_namespace
                                                ->source_corresp;
        if (scp_is_namespace_member(scp)) {
          sp->parent = scp_parent_namespace(scp)->variant.assoc_scope;
        } else {
          sp->parent = scope_stack[DEPTH_OF_FILE_SCOPE].il_scope;
        }  /* if */
      }
      break;
    case sck_class_struct_union:
    case sck_enum:
      { a_source_correspondence_ptr  scp = &ssep->assoc_type->source_corresp;
        if (scp->is_class_member) {
          sp->parent = class_type_supp(scp_parent_class(scp))->assoc_scope;
        } else if (scp_is_namespace_member(scp)) {
          sp->parent = scp_parent_namespace(scp)->variant.assoc_scope;
        } else if (!scp->is_local_to_function) {
          sp->parent = scope_stack[DEPTH_OF_FILE_SCOPE].il_scope;
        } else if (scope_stack[decl_scope_level].kind ==
                                            (a_scope_kind)sck_func_prototype) {
          /* A local class or enum defined in a prototype scope.  E.g.:
               void f() {
                 void g(struct S {} *p);
               }
             This is always an error, but the parent is set anyway for error
             recovery purposes. */
          sp->parent = ensure_il_scope_exists(&scope_stack[decl_scope_level]);
          expect_error();
        } else {
          /* A local type not nested in another local type.  The parent cannot
             be set because of memory region constraints.  Create an implicit
             reference instead. */
          a_scope_depth            func_depth;
          a_scope_stack_entry_ptr  declssep = &scope_stack[decl_scope_level];
          a_scope_ptr              parent_scope, func_scope;
          if (scp->parent_via_local_scope_ref) {
            /* A scope has been recorded already. */
            parent_scope = f_get_parent_scope_of(scp);
            func_scope = scope_for_routine(scp->enclosing_routine);
          } else {
            func_depth = declssep->depth_innermost_function_scope;
            if (is_local_scope_kind(declssep->kind)) {
              /* The normal cases. */
            } else {
              /* An unexpected parent scope for a local class/enum.  Use the
                 enclosing function scope for error recovery purposes. */
              expect_error_str(
                 "set_parent_scope_on_push: unexpected scope for class/enum");
              /* In some cases (these are error cases), we may have to search
                 through the scope stack to find the enclosing function
                 scope. */
              while (declssep->kind != sck_function) {
                if (declssep->depth_innermost_function_scope !=
                                                             NO_SCOPE_DEPTH) {
                  func_depth = declssep->depth_innermost_function_scope;
                } else {
                  func_depth = declssep->previous_scope;
                }  /* if */
                check_assertion(func_depth != NO_SCOPE_DEPTH);
                declssep = &scope_stack[func_depth];
              }  /* while */
            }  /* if */
            parent_scope = ensure_il_scope_exists(declssep);
            func_scope = scope_stack[func_depth].il_scope;
          }  /* if */
          make_local_scope_ref(parent_scope, (char*)sp, iek_scope, func_scope);
        }  /* if */
      }
      break;
    default:
      unexpected_condition();
  }  /* switch */
done:;
}  /* set_parent_scope_on_push */

#if DO_IL_LOWERING

a_boolean parent_is_lambda_closure(a_routine_ptr	routine,
				   a_type_ptr		*closure_class)
/*
Return TRUE if routine is a member of a closure class.  If it is,
and *closure_class is not NULL, a pointer to the closure class is
returned in that pointer (otherwise the pointer is returned as NULL).
*/
{
  a_type_ptr	result_class = NULL;

  if (routine->source_corresp.is_class_member) {
    a_type_ptr	parent_class = parent_class_of(routine);
    if (class_type_supp(parent_class)->is_lambda_closure_class) {
      result_class = parent_class;
    }  /* if */
  }  /* if */
  if (closure_class != NULL) *closure_class = result_class;
  return result_class != NULL;
}  /* parent_is_lambda_closure */

#endif /* DO_IL_LOWERING */

using a_saved_stmt_stack_stack = Dyn_array<a_struct_stmt_stack_state>;
			/* The type used for a stack of saved stmt stack
			   states. */

STATIC_THREAD a_saved_stmt_stack_stack
		*saved_stmt_stack_stack;
			/* The stack of saved stmt stack states. */


static void push_saved_stmt_scope_stack()
/*
Push a new entry onto the saved statement scope stack, stack.
*/
{
  saved_stmt_stack_stack->emplace_back();

  a_struct_stmt_stack_state *state = &saved_stmt_stack_stack->back_elem();
  new_struct_stmt_stack(state);
}  /* push_saved_stmt_scope_stack */


static void pop_saved_stmt_scope_stack()
/*
Pop the current top entry from the saved statement scope stack, stack.
*/
{
  a_struct_stmt_stack_state *state = &saved_stmt_stack_stack->back_elem();

  restore_struct_stmt_stack(state);
  saved_stmt_stack_stack->pop_back();
}  /* pop_saved_stmt_scope_stack */


static a_memory_region_number get_enclosing_memory_region(
						a_routine_ptr	assoc_routine)
/*
Determine if assoc_routine is a "top-level" routine.   If it is not,
find the enclosing routine and return its memory region number.
If it is top-level, return NULL_region_number.  A top-level function is any
function that is not a member function of a local class.
*/
{
  a_memory_region_number	result = NULL_region_number;
  a_type_ptr			parent_class;

  parent_class = parent_class_or_null(assoc_routine);
  if (parent_class != NULL) {
    /* Get the memory region of the function enclosing the parent class, if
       any.  If there is no enclosing function, then assoc_routine is a
       top-level routine. */
    a_scope_ptr	scope = get_parent_scope_of(parent_class);
    /* Enclosing class scopes are traversed as well as block scopes, because
       the parent class can itself be a member of a class that is local to a
       function, as in
         void f() { struct L { struct M { void m() {} }; }; }
       and, less obviously, for the closure class of a lambda that appears in
       the declarator of another lambda, as in
         short f() {
           const int k = 0;
           return []() -> B<[k]{ return k; }()>::C { return 29; }();
         }
       where the closure class of the inner lambda is a member of the closure
       class of the outer one.  The IL of a routine in such a class can refer
       to the entities of the enclosing function, and therefore must be
       allocated in the memory region of that function.  The parent pointer of
       a class scope is not set, so the parent scope of such a scope must be
       obtained through get_parent_scope_of. */
    while (scope != NULL && !scope_is(scope, sck_function)) {
      if (scope_is(scope, sck_class_struct_union)) {
        scope = get_parent_scope_of(scope->variant.assoc_type);
      } else if (is_local_scope_kind(scope->kind)) {
        scope = scope->parent;
      } else {
        break;
      }  /* if */
    }  /* while */
    if (scope != NULL && scope_is(scope, sck_function)) {
      a_function_def_number	number;
      number = scope->variant.routine.ptr->function_def_number;
      check_assertion(number != NULL_function_def_number);
      result = il_header.function_def_table[number].memory_region;
    }  /* if */
  }  /* if */
  check_assertion(result != file_scope_region_number);
  return result;
}  /* get_enclosing_memory_region */


/*
Return TRUE if the scope stack entry kind given by kind, and with the
specified push_scope options is for something that has an effect on
access control (a class, class reactivation, or function).  Access control
only exists in C++.
*/
#define is_scope_kind_that_affects_access_control(kind, options)       \
   ((kind) == (a_scope_kind)sck_class_struct_union ||                 \
    (kind) == (a_scope_kind)sck_class_reactivation ||                 \
    ((kind) == (a_scope_kind)sck_instantiation_context &&             \
     ((options) & PS_NEW_INSTANTIATION_CONTEXT) != 0) ||	      \
    ((kind) == (a_scope_kind)sck_template_instantiation &&	      \
     ((options) & PS_IS_RESCAN) == 0) ||			      \
    (kind) == (a_scope_kind)sck_function ||			      \
    (kind) == (a_scope_kind)sck_function_access)


/*
Return TRUE if the scope stack entry kind is for something that should
affect the current declarative level.  In C, the current declarative
level is the same as depth_scope_stack except when struct/union field
scopes are active; when they are, it indicates the first non-struct-or-union
scope.  In C++, struct/union/class scopes are real scopes; however,
class and namespace reactivations and template instantiations are not real
scopes.  In neither C or C++ is a pragma scope is treated as a real scope.
*/
#define is_scope_kind_that_affects_declarative_level(kind)              \
   ((kind) != (a_scope_kind)sck_pragma &&                               \
   ((C_dialect != C_dialect_cplusplus) ?                                \
       /* C -- struct/union classes are not real scopes. */             \
        ((kind) != (a_scope_kind)sck_class_struct_union) :              \
        /* C++ -- class reactivations are not real scopes. */           \
        ((kind) != (a_scope_kind)sck_class_reactivation &&              \
         (kind) != (a_scope_kind)sck_namespace_reactivation &&          \
         (kind) != (a_scope_kind)sck_instantiation_context &&		\
         (kind) != (a_scope_kind)sck_function_access &&			\
         (kind) != (a_scope_kind)sck_template_instantiation &&          \
         (kind) != (a_scope_kind)sck_module_decl_import)))


static inline void expand_scope_stack_if_needed(void)
/*
Ensure the scope stack is large enough to accommodate one more entry.
*/
{
  if (depth_scope_stack+1 == (int)size_scope_stack) {
    /* The stack is full; expand it by reallocating. */
    sizeof_t new_size = size_scope_stack + SCOPE_STACK_INCREMENTAL_ALLOCATION;
    scope_stack = (a_scope_stack_entry_ptr)realloc_buffer(
                      (char *)scope_stack,
                      (sizeof_t)(size_scope_stack*sizeof(a_scope_stack_entry)),
                      (sizeof_t)(new_size*sizeof(a_scope_stack_entry)));
    size_scope_stack = new_size;
  }  /* if */
}  /* expand_scope_stack_if_needed */


static INLINE void sync_pointers_block_to_il(
                                    a_scope_pointers_block_ptr scope_ptr_block,
                                    a_scope_ptr                il_scope)
/*
Update the pointers block to ensure that it matches the IL list state (as one
or more lists may have been manipulated while the scope -- and thus its
pointers block -- was not active on the scope stack).
*/
{
#define SYNC_PB_LIST(pb_list_name, il_list_name)                              \
  {                                                                           \
    auto end_of_list = il_scope->il_list_name;                                \
    if (scope_ptr_block->pb_list_name != NULL) {                              \
      /* Update the tail pointer from the previously stored tail. */          \
      end_of_list = scope_ptr_block->pb_list_name;                            \
      while (end_of_list->next != NULL) {                                     \
        end_of_list = end_of_list->next;                                      \
      }  /* while */                                                          \
    } else if (il_scope->il_list_name != NULL) {                              \
      /* This is a "cold start"; find the list from the IL list. */           \
      while (end_of_list->next != NULL) {                                     \
        end_of_list = end_of_list->next;                                      \
      }  /* while */                                                          \
      scope_ptr_block->pb_list_name = end_of_list;                            \
    }  /* if */                                                               \
    scope_ptr_block->pb_list_name = end_of_list;                              \
  }
  SYNC_PB_LIST(last_constant, constants);
  SYNC_PB_LIST(last_type, types);
  SYNC_PB_LIST(last_variable, variables);
  SYNC_PB_LIST(last_routine, routines);
  SYNC_PB_LIST(last_asm_entry, asm_entries);
  SYNC_PB_LIST(last_dynamic_init, dynamic_inits);
  SYNC_PB_LIST(last_namespace, namespaces);
  SYNC_PB_LIST(last_using_declaration, using_declarations);
  SYNC_PB_LIST(last_using_directive, using_directives);
  SYNC_PB_LIST(last_pragma, pragmas);
#if RECORD_HIDDEN_NAMES_IN_IL
  SYNC_PB_LIST(last_hidden_name, hidden_names);
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
  SYNC_PB_LIST(last_template, templates);
#if MICROSOFT_EXTENSIONS_ALLOWED
  SYNC_PB_LIST(last_ms_attribute, ms_attributes);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
  SYNC_PB_LIST(last_ms_if_exists, ms_if_exists);
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
#undef SYNC_PB_LIST
}  /* sync_pointers_block_to_il */


static a_scope_ptr push_scope_full(
			a_scope_kind			kind,
			a_scope_number			scope_number_to_reuse,
			a_type_ptr			assoc_type,
			a_routine_ptr			assoc_routine,
			a_namespace_ptr			assoc_namespace,
			a_symbol_ptr			instance_sym,
			a_symbol_ptr			template_sym,
			a_template_arg_ptr		template_arg_list,
			a_template_decl_info_ptr	template_decl_info,
			an_object_lifetime_ptr		lifetime,
			a_scope_ptr			scope_to_reactivate,
			a_scope_pointers_block_ptr	pointers_block,
			a_push_scope_options_set	options)
/*
Begin a new name scope by pushing an entry on the scope stack.  kind indicates
the kind of scope (file, function, block, function prototype, etc.).  Returns
a pointer to the IL scope allocated (or NULL if no IL scope is allocated,
as happens, for example, with function prototype scopes).  For function
scopes, scope_number_to_reuse is the scope number to be used (it was chosen
when the function prototype was scanned, or is NO_SCOPE_NUMBER if it hasn't
been chosen yet); for class reactivation scopes, scope_number_to_reuse is
the class scope number; for the file scope, scope_number_to_reuse is
file_scope_number assigned earlier; for the other cases, a new scope number
is generated. assoc_type points to an associated type for the cases where
that's meaningful (function prototype, class, class reactivation, and template
instantiation (for class templates only) scopes); it must be NULL in other
cases.  assoc_routine points to a routine for the function scope case and
for function access scopes; it must be NULL in other cases.  instance_symbol,
template_symbol, and template_arg_list are non-NULL only when a template
instantiation scope is being pushed; they represent, respectively, the symbol
for the class or function being instantiated or the static data member being
defined; the symbol identifying the template on which the instantiation or
definition is based; and the template argument list the produces the
specific version of the template.  template_decl_info is used for template
instantiation scopes, and describes the context being created by the
instantiation scope (the template parameters to be used, etc.).
template_decl_info is also used for template declaration scopes and points
to the declaration information for the template declaration scope being pushed
(it can be NULL if this scope is being inserted for an abbreviated function
template declaration).  lifetime, if non-NULL, is a previously allocated object
lifetime to be used for the scope.

scope_to_reactivate is non-NULL for block scopes that are being reactivated.
For other kinds of reactivations, the scope pointer is obtained elsewhere.
If pointers_block is non-NULL, the pointers block pointed to is used for
the scope being pushed.

options is a bit set of option flags that specify additional information about
the scope being pushed.
*/
{
  a_scope_stack_entry_ptr ssep;
  a_scope_ptr             sp = NULL;
  a_boolean               reactivate_template_params = FALSE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_boolean               already_in_nonspecialized_instantiation_context =
                                    is_nonspecialized_instantiation_context();
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  a_boolean               new_il_scope = FALSE;
  a_boolean               is_alias_template_instantiation = FALSE;

  db_enter(3, "push_scope_full");
  expand_scope_stack_if_needed();
  /* Push the stack, initialize the new scope entry. */
  ssep = &scope_stack[++depth_scope_stack];
  memzero((char*)ssep, sizeof(*ssep));
  /* Determine the scope number. */
  if ((scope_number_to_reuse != NO_SCOPE_NUMBER &&
       (kind == (a_scope_kind)sck_function ||
        kind == (a_scope_kind)sck_block ||
        kind == (a_scope_kind)sck_condition ||
        kind == (a_scope_kind)sck_template_declaration ||
        kind == (a_scope_kind)sck_func_prototype)) ||
       kind == (a_scope_kind)sck_file ||
       kind == (a_scope_kind)sck_namespace_extension ||
       kind == (a_scope_kind)sck_namespace_reactivation ||
       kind == (a_scope_kind)sck_class_reactivation ||
       kind == (a_scope_kind)sck_template_instantiation) {
    /* For function scopes, reuse the scope used for the parameters
       in the function declarator.  For block reactivations, use the scope
       number from the original push.  For template declaration reactivations,
       the scope number for the template parameters is used.  The number
       is only reused if an IL scope was allocated (otherwise
       scope_number_to_reuse will be NO_SCOPE_NUMBER). */
    /* For class reactivations, re-establish the class scope and for template
       instantiations re-establish the template declaration scope.
       For the file scope, use the specified scope number. */
    ssep->number       = scope_number_to_reuse;
  } else {
    /* Assign a new scope number for other kinds of scopes. */
    ssep->number       = take_next_scope_number();
  }  /* if */
  /* Save the current IL memory region for restoration by pop_scope.  That's
     important if we have temporarily switched into the file scope memory
     region. */
  ssep->prev_il_memory_region = curr_il_region_number;
  /* Allocate the IL scope entry if one is needed. */
  switch (kind) {
    case sck_file:
      /* Activate or reactivate the file scope. */
      sp = curr_translation_unit->primary_scope;
      switch_il_region(file_scope_region_number);
      ssep->il_memory_region = curr_il_region_number;
      break;
    case sck_function:
      if ((options & PS_IS_REACTIVATION) == 0) {
        /* If this is not a reactivation of the function scope, either
           start a new memory region or, if there is an enclosing function,
           use the region of that function.  This also allocates the
           top-level scope entry for the function. */
        a_memory_region_number	enclosing_region;
        enclosing_region = get_enclosing_memory_region(assoc_routine);
        new_il_scope = TRUE;
        sp = new_function_scope(ssep->number, assoc_routine, enclosing_region);
      } else {
        a_function_def_number		number;
        a_function_def_descr_ptr	fddp;
        check_assertion(assoc_routine != NULL);
        number = assoc_routine->function_def_number;
        fddp = &il_header.function_def_table[number];
        curr_il_region_number = fddp->memory_region;
        sp = fddp->scope;
      }  /* if */
      ssep->il_memory_region = curr_il_region_number;
      break;
    case sck_instantiation_context:
    case sck_template_instantiation:
      /* Template instantiations should always take place in the file scope
         memory region. */
    case sck_func_prototype:
      /* Use the file scope memory region for a function prototype scope,
         since param types and types declared within it are pointed to from
         the function type, which is also in file scope memory.  (Parameter
         variables are not created until the function scope is pushed.)
         However, the IL scope is not allocated until it is needed -- it
         usually isn't. */
      sp = NULL;
      if (curr_il_region_number != file_scope_region_number) {
        switch_il_region(file_scope_region_number);
      }  /* if */
      ssep->il_memory_region = file_scope_region_number;
      break;
    case sck_namespace:
    case sck_namespace_extension:
      if (curr_il_region_number != file_scope_region_number) {
        /* In a legal program we should already be in the file-scope memory
           region -- there must have been an error. */
        switch_il_region(file_scope_region_number);
      }  /* if */
      ssep->il_memory_region = file_scope_region_number;
      check_assertion(assoc_namespace != NULL);
      if (kind == (a_scope_kind)sck_namespace) {
        new_il_scope = TRUE;
        sp = alloc_scope(kind, ssep->number, (a_routine_ptr)NULL);
        sp->variant.assoc_namespace = assoc_namespace;
        assoc_namespace->variant.assoc_scope = sp; /*lint !e413*/
      } else {
        sp = assoc_namespace->variant.assoc_scope; /*lint !e413*/
      }  /* if */
      ssep->il_memory_region = file_scope_region_number;
      break;
    case sck_class_struct_union:
      /* Class/struct/union definitions require the file-scope memory region,
         since the entities created to represent the members are pointed to
         from the type entry. */
      check_assertion(assoc_type != NULL);
      if (curr_il_region_number != file_scope_region_number) {
        switch_il_region(file_scope_region_number);
      }  /* if */
      ssep->il_memory_region = file_scope_region_number;
      /* Save a copy of the scope number in the class symbol supplement. */
      symbol_supplement_for_class(assoc_type)->member_decl_scope =
                                                              ssep->number;
      new_il_scope = TRUE;
      /* In some contexts (e.g., modules), the associated scope of the class
         was needed before it was created.  In those contexts, a placeholder
         scope was created - use that scope if it exists. */
      sp = class_type_supp(assoc_type)->assoc_scope; /*lint !e413*/
      if (sp == NULL) {
        sp = alloc_scope(kind, ssep->number, (a_routine_ptr)NULL);
      } else {
        check_assertion(sp->is_placeholder_scope && scope_is(sp, kind));
        sp->is_placeholder_scope = FALSE;
        sp->number = ssep->number;
      }  /* if */
      break;
    case sck_condition:
      /* Use the enclosing memory region. */
      ssep->il_memory_region = (ssep-1)->il_memory_region;
      if (scope_to_reactivate != NULL) {
        sp = scope_to_reactivate;
      } else {
        /* A scope is allocated for the condition later because the parent
           scope must also be set. */
      }  /* if */
      break;
    case sck_enum:
      /* Create an IL scope in file scope memory.  (This case is very similar
         to the sck_class_struct_union case.) */
      if (curr_il_region_number != file_scope_region_number) {
        switch_il_region(file_scope_region_number);
      }  /* if */
      ssep->il_memory_region = file_scope_region_number;
      new_il_scope = TRUE;
      sp = alloc_scope(kind, ssep->number, (a_routine_ptr)NULL);
      break;
    case sck_template_declaration:
      /* When prototype instantiations are recorded in the IL, IL scopes
         exist for template declarations.  Only template declaration scopes
         from the original source are included.  Scopes pushed for the
         rescan of dependent template template parameters do not have IL
         scopes. */
      if (prototype_instantiations_in_il &&
          (options & PS_IS_TEMPLATE_PARAM_RESCAN) == 0) {
        /* Template declaration scopes always have parameter declarations, so
           we know a scope is required.  The scope will always be in the file
           scope memory region except for error cases. */
        sp = alloc_scope((a_scope_kind)sck_template_declaration, ssep->number,
                         (a_routine_ptr)NULL);
      }  /* if */
      /* Use the enclosing memory region. */
      ssep->il_memory_region = (ssep-1)->il_memory_region;
      break;
    case sck_block:
      /* Use the enclosing memory region. */
      ssep->il_memory_region = (ssep-1)->il_memory_region;
      sp = scope_to_reactivate;
      break;
    case sck_module_decl_import:
      ssep->il_memory_region = file_scope_region_number;
      sp = NULL;
      push_saved_stmt_scope_stack();
      break;
    default:
      /* For scopes for which a new memory region is not begun, the associated
         memory region is the same as for the enclosing scope (there must be an
         enclosing scope, because the scope we're opening here is not the file
         scope). */
      ssep->il_memory_region = (ssep-1)->il_memory_region;
      /* For block scopes the IL scope is not allocated until it is needed,
         because usually it will not be needed. For class reactivations in
         C++, no scope is ever allocated. */
      sp = NULL;
  }  /* switch */
  /* Fill in the fields of the scope entry. */
  ssep->kind                     = kind;
  ssep->inside_local_class       = inside_local_class;
  ssep->alias_in_template_decl   = (options & PS_ALIAS_IN_TEMPLATE_DECL) != 0;
  ssep->record_form_of_name_reference = kind == (a_scope_kind)sck_file &&
                                        record_form_of_name_reference;
  ssep->microsoft_specialization_instantiation_scope =
                                  (options & PS_MICROSOFT_SPECIALIZATION) != 0;
  ssep->function_partial_instantiation =
                            (options & PS_FUNCTION_PARTIAL_INSTANTIATION) != 0;
  ssep->is_generic_lambda = (options & PS_IS_GENERIC_LAMBDA) != 0;
  ssep->force_decl_seq_check = (options & PS_FORCE_DECL_SEQ_CHECK) != 0;
  /* The in_template_arg_list flag indicates whether we're currently scanning
     tokens inside angle brackets.  If we push a scope that implies a new
     source of tokens (e.g., a template instantiation), clear the flag.
     The implicit_typename flag is set to the value of the global variable
     for each new context.  It may then be reset by the caller, if needed. */
  if (kind == (a_scope_kind)sck_file ||
      kind == (a_scope_kind)sck_template_instantiation ||
      kind == (a_scope_kind)sck_instantiation_context ||
      kind == (a_scope_kind)sck_pragma) {
    ssep->implicit_typename = implicit_typename_enabled;
    ssep->in_tentative_decl = FALSE;
  } else {
    ssep->in_template_arg_list = (ssep-1)->in_template_arg_list;
    ssep->implicit_typename = (ssep-1)->implicit_typename;
    ssep->in_disambiguation = (ssep-1)->in_disambiguation;
    ssep->in_tentative_decl = (ssep-1)->in_tentative_decl;
    ssep->in_field_initializer = (ssep-1)->in_field_initializer;
#if GNU_EXTENSIONS_ALLOWED
    ssep->in_gnu_abi_tag_namespace =
                                 (ssep-1)->in_gnu_abi_tag_namespace ||
                                  (assoc_namespace != NULL &&
                                   assoc_namespace->has_gnu_abi_tag_attribute);
#endif /* GNU_EXTENSIONS_ALLOWED */
    if (kind == (a_scope_kind)sck_function ||
        kind == (a_scope_kind)sck_class_struct_union ||
        kind == (a_scope_kind)sck_class_reactivation) {
      /* If we are entering a class context or a lambda, don't inherit the
         discarded statement context, nor the "if consteval" context. */
    } else {
      ssep->in_discarded_statement = (ssep-1)->in_discarded_statement;
      ssep->in_consteval_context = (ssep-1)->in_consteval_context;
    }  /* if */
  }  /* if */
  ssep->is_rescan = (options & PS_IS_RESCAN) != 0;
  ssep->is_template_param_rescan =(options & PS_IS_TEMPLATE_PARAM_RESCAN) != 0;
  ssep->in_concept_rescan = FALSE;
  ssep->error_detected = FALSE;
  if (depth_scope_stack > 0 && (ssep-1)->module_load_context_count > 0) {
    ssep->module_load_context_count = 1;
  }  /* if */
  ssep->is_reactivation          = (options & PS_IS_REACTIVATION) != 0;
  ssep->il_scope                 = sp;
  ssep->assoc_type               = assoc_type;
  ssep->assoc_routine            = assoc_routine;
  ssep->assoc_namespace          = assoc_namespace;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  ssep->source_sequence_entries_disallowed =
                                       source_sequence_entries_disallowed;
  if (kind == (a_scope_kind)sck_file && ssep->is_reactivation) {
    /* For a reactivation of the file scope, restore the source sequence list
       that was built up on the previous push/pop. */
    ssep->source_sequence_list = il_header.primary_scope->source_sequence_list;
    ssep->end_of_source_sequence_list = curr_translation_unit->
                          file_scope_pointers_block.last_source_sequence_entry;
    curr_translation_unit->
                   file_scope_pointers_block.last_source_sequence_entry = NULL;
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  ssep->decl_scope_level         = decl_scope_level;
  ssep->depth_template_declaration_scope = depth_template_declaration_scope;
  ssep->depth_innermost_instantiation_scope =
                                       depth_innermost_instantiation_scope;
  ssep->instance_sym             = instance_sym;
  ssep->template_sym             = template_sym;
  ssep->template_arg_list        = template_arg_list;
  ssep->deduced_template_args    = NULL;
  ssep->source_position          = pos_curr_token;
  ssep->depth_innermost_function_scope = depth_innermost_function_scope;
  ssep->template_decl_info       = template_decl_info;
  ssep->next_scope_that_affects_access_control =
                          depth_of_innermost_scope_that_affects_access_control;
  ssep->orig_access_depth        = NO_SCOPE_DEPTH;
  ssep->saved_curr_deferred_access_scope
				 = curr_deferred_access_scope;
  ssep->saved_expr_stack         = expr_stack;  /* See also the setting of
                                                   expr_stack to NULL below. */
  ssep->saved_curr_object_lifetime = curr_object_lifetime;
  ssep->depth_innermost_namespace_scope = depth_innermost_namespace_scope;
  ssep->previous_scope            = NO_SCOPE_DEPTH;
  ssep->instantiation_context_depth = NO_SCOPE_DEPTH;
  ssep->instantiation_common_depth = NO_SCOPE_DEPTH;
  ssep->saved_depth_of_initial_lookup_scope = depth_of_initial_lookup_scope;
  ssep->orig_depth               = NO_SCOPE_DEPTH;
  ssep->saved_innermost_scope_that_affects_access = NO_SCOPE_DEPTH;
  ssep->fp_contract_state        = curr_fp_contract_state;
  ssep->fenv_access_state        = curr_fenv_access_state;
  ssep->cx_limited_range_state   = curr_cx_limited_range_state;
#if FIXED_POINT_ALLOWED
  ssep->fx_full_precision_state = curr_fx_full_precision_state;
  ssep->fx_fract_overflow_state = curr_fx_fract_overflow_state;
  ssep->fx_accum_overflow_state = curr_fx_accum_overflow_state;
#endif /* FIXED_POINT_ALLOWED */
  if (template_sym != NULL && template_sym->from_module_code) {
    ssep->module_load_context_count += 1;
  }  /* if */
  if (sp != NULL) {
    if (new_il_scope) {
      /* Set the parent scope. */
      set_parent_scope_on_push(ssep);
    }  /* if */
    if (sp->depth_in_scope_stack == NO_SCOPE_DEPTH) {
      /* Update the depth at which this scope is on the stack.  Only do this
         if the scope is not already on the stack somewhere else. */
      sp->depth_in_scope_stack = depth_scope_stack;
    }  /* if */
  }  /* if */
  if (kind == (a_scope_kind)sck_instantiation_context) {
    /* When an instantiation context is pushed, the previous scope is the
       file scope. */
    ssep->previous_scope = DEPTH_OF_FILE_SCOPE;
    if (!(options & PS_IS_RESCAN)) {
      /* Store the previous state, and initialize a new ovl_res_stack.
         (Instantiation contexts pushed for rescanning are associated with the
         current overload resolution and therefore should not trigger the
         initialization of a new stack.) */
      push_new_ovl_res_stack();
    }  /* if */
    if ((options & PS_NEW_INSTANTIATION_CONTEXT) != 0) {
      depth_innermost_instantiation_scope = NO_SCOPE_DEPTH;
      depth_template_declaration_scope = NO_SCOPE_DEPTH;
      curr_deferred_access_scope = NO_SCOPE_DEPTH;
      depth_of_innermost_scope_that_affects_access_control = NO_SCOPE_DEPTH;
    }  /* if */
  } else if (depth_scope_stack != DEPTH_OF_FILE_SCOPE) {
    /* By default, the previous scope is the one that precedes this one
       on the scope stack.  This may be adjusted for instantiation scopes. */
    ssep->previous_scope = depth_of_initial_lookup_scope;
  }  /* if */
  /* Set the point at which name lookups should start to the newly
     created scope. */
  depth_of_initial_lookup_scope = depth_scope_stack;
  /* Put the associated type (if any) into the IL scope (if any). */
  /* Note that the corresponding routine case was handled by the
     new_function_scope call. */
  if (assoc_type != NULL && sp != NULL) sp->variant.assoc_type = assoc_type;
  /* Determine whether this is an instantiation scope for an alias template. */
  if (kind == (a_scope_kind)sck_template_instantiation &&
      (instance_sym != NULL && instance_sym->kind == (a_symbol_kind)sk_type)) {
    is_alias_template_instantiation = TRUE;
  }  /* if */
  /* Maintain the current declarative level.  It is the same as 
     depth_scope_stack except when struct/union field scopes are
     active; when they are, it indicates the first non-struct-or-union
     scope.  In C++, struct/union/class scopes are real scopes; however,
     class reactivations are not real scopes. */
  if (is_scope_kind_that_affects_declarative_level(kind) ||
      (is_alias_template_instantiation &&
       (options & (PS_PROTOTYPE_INSTANTIATION |
                   PS_NONREAL_INSTANTIATION)) != 0)) {
    if (decl_scope_level < depth_innermost_instantiation_scope) {
      a_symbol_ptr	inst_template_sym;
      inst_template_sym =
                 scope_stack[depth_innermost_instantiation_scope].template_sym;
      if (inst_template_sym != NULL &&
          inst_template_sym->kind != (a_symbol_kind)sk_static_data_member &&
          !is_alias_template_instantiation) {
        /* Template parameters are considered part of the next scope that
           affects the declarative level -- except for static data member
           and alias template instantiations for which no such scope exists. */
        reactivate_template_params = TRUE;
      }  /* if */
    }  /* if */
    decl_scope_level = depth_scope_stack;
  }  /* if */
  if (kind == (a_scope_kind)sck_file && ssep->is_reactivation) {
    /* When a file scope is reactivated, its symbols will be on the inactive
       list. */
    ssep->inactive_symbols_may_be_visible = TRUE;
  }  /* if */
  /* Check for class reactivations, classes with base classes,
     namespace extensions, and template instantiations.  When these
     are found name lookup is more involved.  If the new scope is neither,
     we can just use the state from the previous scope. */
  if (!C_mode() &&
      (kind == (a_scope_kind)sck_class_reactivation ||
       kind == (a_scope_kind)sck_template_instantiation ||
       kind == (a_scope_kind)sck_namespace_extension ||
       kind == (a_scope_kind)sck_namespace_reactivation ||
       (kind == (a_scope_kind)sck_class_struct_union &&
        /*lint --e(413)*/
        base_classes_of(assoc_type) != NULL))) {
    ssep->inactive_symbols_may_be_visible = TRUE;
  } else if (kind != (a_scope_kind)sck_file) {
    ssep->inactive_symbols_may_be_visible =
                                   (ssep-1)->inactive_symbols_may_be_visible;
  }  /* if */
  if (kind != (a_scope_kind)sck_file) {
    ssep->record_form_of_name_reference = 
                                       (ssep-1)->record_form_of_name_reference;
  }  /* if */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
  /* Determine whether Microsoft __if_exist entries and associated source
     sequence entries should be created for this scope.  Such entries are
     created in class scopes.  The flag is also set for function prototype
     scopes within class scopes, so that a diagnostic may be issued for
     such cases. */
  if (create_microsoft_if_exists_entries) {
    a_boolean	create_ms_if_exists_entries = FALSE;
    if (kind == (a_scope_kind)sck_class_struct_union) {
      create_ms_if_exists_entries = TRUE;
    } else if (kind == (a_scope_kind)sck_func_prototype) {
      create_ms_if_exists_entries = (ssep-1)->create_ms_if_exists_entries;
    }  /* if */
    ssep->create_ms_if_exists_entries = create_ms_if_exists_entries;
  }  /* if */
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
  if (!C_mode()) {
    if (kind == (a_scope_kind)sck_class_reactivation) {
      /* Determine whether the class being reactivated is still in the process
         of being defined.  This can occur when a class nested within a
         class template is instantiated while the enclosing class is still
         in the process of being instantiated. */
      ssep->reactivated_class_being_defined = is_incomplete_type(assoc_type);
      ssep->treat_as_specialization = (options & PS_IS_SPECIALIZATION) != 0;
    }  /* if */
    /* Pragma, template declaration and template instantiation scopes
       require that the slow lookup algorithm be used because they require
       that certain symbols on the active list not be considered.  Lambda
       class scopes require special treatment to ignore names in the lambda
       scope. */
    if (kind == (a_scope_kind)sck_pragma ||
        kind == (a_scope_kind)sck_template_declaration ||
        kind == (a_scope_kind)sck_template_instantiation ||
        kind == (a_scope_kind)sck_instantiation_context ||
        (kind == (a_scope_kind)sck_class_struct_union &&
         /*lint --e(413)*/
         class_type_supp(assoc_type)->is_lambda_closure_class)) {
      ssep->slow_lookup_required = TRUE;
    } else if (kind != (a_scope_kind)sck_file) {
      ssep->slow_lookup_required = (ssep-1)->slow_lookup_required;
    }  /* if */
    if (kind == (a_scope_kind)sck_class_struct_union ||
        kind == (a_scope_kind)sck_class_reactivation) {
      /* Keep track of the number of classes and class reactivations. */
      num_classes_on_scope_stack++;
      /* If we're entering a class and we're already inside a function,
         the class is a local class. */
      /* Note that this is done before depth_innermost_function_scope is
         cleared below. */
      if (depth_innermost_function_scope != NO_SCOPE_DEPTH) {
        ssep->inside_local_class = inside_local_class = TRUE;
      }  /* if */
      /* Determine whether this scope represents the specialization of a
         class. */
      check_assertion(assoc_type != NULL);
      ssep->in_class_specialization =
                       assoc_type->variant.class_struct_union.is_specialized;
 
    }  /* if */
    /* The class specialization flag is also set if the parent scope is
       a specialization scope (or nested within one). */
    if (kind != (a_scope_kind)sck_file) {
      ssep->in_class_specialization |=
                              previous_scope_of(ssep)->in_class_specialization;
    }  /* if */
    if (kind == (a_scope_kind)sck_template_instantiation) {
      /* is_instantiation_context is TRUE for all instantiation scopes except
         for Microsoft specialization scopes that are not enclosed by other
         instantiation scopes. */
      ssep->is_instantiation_context =
                       depth_innermost_instantiation_scope != NO_SCOPE_DEPTH ||
                       !ssep->microsoft_specialization_instantiation_scope;
      /* Save the depth of the innermost instantiation scope. */
      depth_innermost_instantiation_scope = depth_scope_stack;
      /* Update the symbols of the template parameters to represent the
         values of the actual arguments by simply changing each to point to
         the type or constant specified by the corresponding template argument.
         The old values do not need to be saved because they can be easily
         recreated by pop_scope. */
      check_assertion(template_decl_info != NULL);
      update_template_param_symbols(template_decl_info->parameters,
                                    template_arg_list);
      /* The current stack state is suspended when an template instantiation
         is done.  It will be restored in pop_scope.  inside_local_class
         is reset by push_template_instantiation_scope. */
      depth_innermost_function_scope =
              ssep->depth_innermost_function_scope = NO_SCOPE_DEPTH;
      innermost_function_scope = NULL;
      ssep->in_prototype_instantiation =
                                   (options & PS_PROTOTYPE_INSTANTIATION) != 0;
      ssep->in_nonreal_instantiation =
                                   (options & PS_NONREAL_INSTANTIATION) != 0;
      ssep->in_generic_definition = (options & PS_GENERIC_DEFINITION) != 0;
      ssep->exception_specification = (options & PS_EXCEPTION_SPEC) != 0;
      ssep->is_default_template_arg =
                                   (options & PS_IS_DEFAULT_TEMPLATE_ARG) != 0;
      if (template_sym != NULL && !ssep->is_rescan) {
        /* Determine whether this is an instantiation of a variadic
           template. */
        a_template_symbol_supplement_ptr	tssp;
        tssp = template_supplement_for_symbol(template_sym);
        ssep->in_variadic_template = tssp->is_variadic ||
                                     (ssep-1)->in_variadic_template;
#if MICROSOFT_EXTENSIONS_ALLOWED
        ssep->instantiation_from_metadata =
                  tssp->from_metadata || (ssep-1)->instantiation_from_metadata;
        ssep->in_generic_instantiation = tssp->is_generic &&
                                         !ssep->in_generic_definition;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      }  /* if */
      if ((template_sym != NULL &&
           template_sym->kind == (a_symbol_kind)sk_static_data_member) ||
          is_alias_template_instantiation) {
        /* Static data members and template aliases don't have their own
           scope so the template parameters are added at the instantiation
           scope. */
        reactivate_template_params = TRUE;
      }  /* if */
      /* Initialize the pointer to the dependent call list for this
         template. */
      ssep->next_nondependent_call = template_decl_info->nondependent_calls;
      ssep->last_pack_expansion_used = template_decl_info->pack_expansions;
    } else if (kind != (a_scope_kind)sck_file &&
               kind != (a_scope_kind)sck_instantiation_context) {
      if (kind == (a_scope_kind)sck_function &&
          /*lint --e(413)*/
          assoc_routine->compiler_generated &&
          !assoc_routine->is_prototype_instantiation) {
        /* If the definition of a compiler-generated routine is kicked off
           within a prototype instantiation, the compiler generated routine
           should not be considered to be within a prototype instantiation. */
        ssep->in_prototype_instantiation = FALSE;
        ssep->in_nonreal_instantiation = FALSE;
        ssep->in_variadic_template = FALSE;
        ssep->in_generic_definition = FALSE;
        ssep->exception_specification = FALSE;
        ssep->is_default_template_arg = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
        ssep->instantiation_from_metadata = FALSE;
        ssep->in_generic_instantiation = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      } else {
        ssep->in_prototype_instantiation =
                                          (ssep-1)->in_prototype_instantiation;
        ssep->in_nonreal_instantiation =
                                          (ssep-1)->in_nonreal_instantiation;
        ssep->in_generic_definition =
                                          (ssep-1)->in_generic_definition;
        ssep->exception_specification =
                                          (ssep-1)->exception_specification;
        ssep->is_default_template_arg =
                                          (ssep-1)->is_default_template_arg;
        ssep->in_variadic_template =
                                          (ssep-1)->in_variadic_template;
        ssep->is_rescan =
                                          (ssep-1)->is_rescan;

#if MICROSOFT_EXTENSIONS_ALLOWED
        ssep->instantiation_from_metadata =
                                         (ssep-1)->instantiation_from_metadata;
        ssep->in_generic_instantiation = 
                                            (ssep-1)->in_generic_instantiation;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      }  /* if */
    }  /* if */
    if (reactivate_template_params) {
      /* We want to ensure that the first declarative scope following
         an instantiation scope does not allow the redeclaration of a
         template parameter name.  This is done by saving a pointer to
         the template parameter list in the scope stack entry of the
         template instantiation scope and setting the templ_param_decl_scope
         flag in the scope for which this test must be done.  For template
         classes and template functions the scope is the next scope
         that affects the declarative level.  For static data members
         there is no such scope, but we set this flag in the instantiation
         scope for consistency. */
      ssep->template_param_decl_scope = TRUE;
    }  /* if */
  }  /* if */
  /* Maintain the depth of the innermost function scope. */
  if (kind == (a_scope_kind)sck_function) {
    depth_innermost_function_scope =
            ssep->depth_innermost_function_scope = depth_scope_stack;
    innermost_function_scope = sp;
#if DO_IL_LOWERING && ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS
    if (!C_mode() && !is_template_dependent_context()) {
      /* Determine whether this is a function for which we need to
         compute string literal sequence numbers.  These are computed for
         routines for which there is the potential of having multiple copies
         in a program. */
      check_assertion(assoc_routine != NULL);
      ssep->assign_string_literal_sequence_numbers =
                         routine_might_exist_in_multiple_copies(assoc_routine);
    }  /* if */
#endif /* DO_IL_LOWERING && ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS */
  } else if (kind == (a_scope_kind)sck_file) {
    /* Note (1) depth_innermost_namespace_scope is set to the file scope's
       depth to give it the sense of "depth_innermost_global_scope", and (2)
       it's intentionally set even in C mode. */
    depth_innermost_namespace_scope =
            ssep->depth_innermost_namespace_scope = depth_scope_stack;
  } else if (C_dialect == C_dialect_cplusplus &&
             (kind == (a_scope_kind)sck_class_struct_union ||
              kind == (a_scope_kind)sck_class_reactivation)) {
    /* When we enter a class scope, the containing function scope (if any)
       becomes invisible in some respects.  (In particular, some expression
       processing routines need to know whether a function scope is the
       immediate context for processing.)  So clear out the variable and
       restore it in pop_scope. */
    depth_innermost_function_scope =
            ssep->depth_innermost_function_scope = NO_SCOPE_DEPTH;
    innermost_function_scope = NULL;
  } else if (kind == (a_scope_kind)sck_namespace ||
             kind == (a_scope_kind)sck_namespace_extension) {
    /* The following is required only to handle illegal programs gracefully.
       Ordinarily these will already be set correctly. */
    depth_innermost_function_scope =
            ssep->depth_innermost_function_scope = NO_SCOPE_DEPTH;
    innermost_function_scope = NULL;
  }  /* if */
  /* Set the default language linkage for the current scope. */
  if (C_dialect == C_dialect_cplusplus) {
    if (kind == (a_scope_kind)sck_file ||
        kind == (a_scope_kind)sck_module_decl_import) {
      /* File scope has extern "C++" linkage by default. */
      ssep->default_name_linkage = (a_name_linkage_kind)nlk_cplusplus_external;
      ssep->name_linkage_is_explicit = FALSE;
    } else if (kind == (a_scope_kind)sck_template_instantiation) {
      /* For template instantiations use the linkage of the template
         declaration. */
      check_assertion(template_decl_info != NULL);
      ssep->default_name_linkage = template_decl_info->name_linkage;
      ssep->name_linkage_is_explicit = FALSE;
    } else {
      ssep->default_name_linkage = (ssep-1)->default_name_linkage;
      ssep->name_linkage_is_explicit = (ssep-1)->name_linkage_is_explicit;
    }  /* if */
  } else {
    ssep->default_name_linkage = (a_name_linkage_kind)nlk_external;
    ssep->name_linkage_is_explicit = FALSE;
  }  /* if */
  if (C_dialect == C_dialect_cplusplus) {
    /* Maintain the depth of the innermost stack entry that affects access
       control.  Special handing for template instantiation scopes is done
       in fixup_instantiation_scopes. */
    if (is_scope_kind_that_affects_access_control(kind, options)) {
      depth_of_innermost_scope_that_affects_access_control = depth_scope_stack;
    }  /* if */
    /* Determine whether this scope affects whether access checks can
       be deferred. */
    if (kind == (a_scope_kind)sck_file ||
        kind == (a_scope_kind)sck_namespace ||
        kind == (a_scope_kind)sck_namespace_extension ||
        kind == (a_scope_kind)sck_pragma ||
        kind == (a_scope_kind)sck_instantiation_context ||
        (kind == (a_scope_kind)sck_class_reactivation &&
         (options & PS_NEW_ACCESS_CONTEXT) != 0) ||
        (kind == (a_scope_kind)sck_function &&
         !assoc_routine->is_lambda_body) ||
        kind == (a_scope_kind)sck_block ||
        kind == (a_scope_kind)sck_module_decl_import ||
        (kind == (a_scope_kind)sck_template_instantiation &&
         (options & PS_MICROSOFT_SPECIALIZATION) == 0) ||
        (kind == (a_scope_kind)sck_class_struct_union &&
         /*lint --e(413)*/
         !class_type_supp(assoc_type)->is_lambda_closure_class)) {
      /* A scope that introduces a new level at which deferred access
         checks may be recorded. */
      curr_deferred_access_scope = depth_scope_stack;
    } else if (kind == (a_scope_kind)sck_template_declaration ||
               kind == (a_scope_kind)sck_func_prototype ||
               kind == (a_scope_kind)sck_function_access ||
               kind == (a_scope_kind)sck_namespace_reactivation ||
               (kind == (a_scope_kind)sck_template_instantiation &&
                (options & PS_MICROSOFT_SPECIALIZATION) != 0) ||
               kind == (a_scope_kind)sck_class_reactivation ||
               kind == (a_scope_kind)sck_enum ||
               kind == (a_scope_kind)sck_condition ||
               (kind == (a_scope_kind)sck_function &&
                /*lint --e(413)*/
                assoc_routine->is_lambda_body) ||
               (kind == (a_scope_kind)sck_class_struct_union &&
                /*lint --e(413)*/
                class_type_supp(assoc_type)->is_lambda_closure_class)) {
      /* The current deferred access scope is left unchanged. */
    } else {
      /* For all other scopes, access checks cannot be deferred. */
      curr_deferred_access_scope = NO_SCOPE_DEPTH;
    }  /* if */
    /* Maintain the depth of a template declaration scope, if any. */
    if (kind == (a_scope_kind)sck_template_declaration) {
      ssep->depth_template_declaration_scope =
        depth_template_declaration_scope = depth_scope_stack;
      if (gnu_bases_operators_enabled) {
        /* Because the __bases and __direct_bases use the variadic mechanism,
           all template declaration scopes must be considered variadic. */
        ssep->in_variadic_template = TRUE;
      }  /* if */
    } else if (kind == (a_scope_kind)sck_template_instantiation ||
               kind == (a_scope_kind)sck_instantiation_context) {
      /* A template instantiation.  The things outside the instantiation
         become invisible.  Those scopes are visible if this is a Microsoft
         specialization instantiation scope, however. */
      if ((options & PS_MICROSOFT_SPECIALIZATION) == 0) {
        ssep->depth_template_declaration_scope =
          depth_template_declaration_scope = NO_SCOPE_DEPTH;
      }  /* if */
    }  /* if */
    if ((kind == (a_scope_kind)sck_template_declaration &&
         (options & PS_IS_TEMPLATE_TEMPLATE_PARAM) == 0) ||
        kind == (a_scope_kind)sck_template_instantiation) {
      /* Start a new pack expansion stack for a template declaration or
         instantiation scope.  Don't start a new stack if the template
         declaration scope is for the template parameters of a template
         template parameter. */
      ssep->pack_expansion_stack = pack_expansion_stack;
      pack_expansion_stack = NULL;
    }  /* if */
    /* The in_template_deduction_context field should be TRUE for
       template declaration scopes and function prototype scopes directly
       within a template declaration scope.  It should also be TRUE for a
       scope pushed with the PS_DEDUCTION_CONTEXT flag and function prototype
       scopes directly within that scope, as well as any class and namespace
       scopes reactivated for the function prototype scopes. */
    if (kind == (a_scope_kind)sck_template_declaration ||
        (options & PS_DEDUCTION_CONTEXT) != 0) {
      ssep->in_template_deduction_context = TRUE;
      /* Record name references in deduction contexts. */
      ssep->record_form_of_name_reference = TRUE;
    } else if (kind == (a_scope_kind)sck_func_prototype ||
               kind == (a_scope_kind)sck_namespace_reactivation ||
               kind == (a_scope_kind)sck_class_reactivation) {
      ssep->in_template_deduction_context =
                                       (ssep-1)->in_template_deduction_context;
    }  /* if */
    if (kind == (a_scope_kind)sck_namespace ||
        kind == (a_scope_kind)sck_namespace_extension ||
        kind == (a_scope_kind)sck_namespace_reactivation) {
      /* Set the scope-pointers-block pointer to refer to the namespace
         symbol supplement. */
      a_symbol_ptr  sym = symbol_for(assoc_namespace) /*lint !e413*/;
      a_namespace_symbol_supplement_ptr
                    nssp = sym->variant.namespace_info.extra_info;
      ssep->assoc_pointers_block = &nssp->pointers_block;
      if (kind != (a_scope_kind)sck_namespace_reactivation) {
        /* If this is a namespace scope that affects the declarative level
           (i.e., not just a reactivation) update the information about
           the current namespace. */
        ssep->within_unnamed_namespace = nssp->within_unnamed_namespace;
        /* Maintain the depth of the innermost namespace scope. */
        depth_innermost_namespace_scope =
              ssep->depth_innermost_namespace_scope = depth_scope_stack;
      }  /* if */
#if NEED_NAME_MANGLING
      if (kind == (a_scope_kind)sck_namespace_extension
#if ABI_COMPATIBILITY_VERSION >= 408
          || kind == (a_scope_kind)sck_namespace_reactivation
#endif /* ABI_COMPATIBILITY_VERSION >= 408 */
                                                             ) {
        /* Restore the discriminator counters. */
        ssep->name_discr.last_unnamed_type_number =
                                               nssp->last_unnamed_type_number;
        ssep->last_closure_type_number = nssp->last_closure_type_number;
      }  /* if */
#endif /* NEED_NAME_MANGLING */
    }  /* if */
    if (kind == (a_scope_kind)sck_function ||
        kind == (a_scope_kind)sck_template_instantiation ||
        kind == (a_scope_kind)sck_module_decl_import ||
        kind == (a_scope_kind)sck_module_isolated ||
        kind == (a_scope_kind)sck_pragma) {
      /* When beginning a nested context, clear the expression stack. */
      expr_stack = NULL;
    }  /* if */
  }  /* if */
  if (kind == (a_scope_kind)sck_class_struct_union) {
    a_class_symbol_supplement_ptr cssp;
    check_assertion(assoc_type != NULL);
    cssp = symbol_supplement_for_class(assoc_type);
    ssep->assoc_pointers_block = &cssp->pointers_block;
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (cssp->default_indexed_properties != NULL) {
      /* Default indexed properties were projected from base classes: Enter
         them in the class scope now. */
      enter_projected_default_indexed_properties(cssp);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else if (kind == (a_scope_kind)sck_class_reactivation) {
    /* For class reactivation scopes, use the lookup table created when the
       class was scanned. */
    a_class_symbol_supplement_ptr cssp;
    check_assertion(assoc_type != NULL);
    cssp = symbol_supplement_for_class(assoc_type);
    ssep->pointers_block.lookup_table = cssp->pointers_block.lookup_table;
  } else if (kind == (a_scope_kind)sck_file) {
    /* For the file scope, use the pointers block allocated in the
       translation unit entry. */
    ssep->assoc_pointers_block = &curr_translation_unit->
                                                    file_scope_pointers_block;
#if NEED_NAME_MANGLING
    if (ssep->is_reactivation) {
      /* Restore discriminator counters preserved across file-scope
         reactivation (parallel to namespace extension/reactivation).
         Prefer the live primary file-scope stack entry when nested,
         otherwise the per-translation-unit saved values. */
      a_discriminator  unnamed = last_file_scope_unnamed_type_number,
                       closure = last_file_scope_closure_type_number;
      if (depth_scope_stack > DEPTH_OF_FILE_SCOPE) {
        a_scope_stack_entry_ptr
                      fs_ssep = &scope_stack[DEPTH_OF_FILE_SCOPE];
        if (fs_ssep->name_discr.last_unnamed_type_number > unnamed) {
          unnamed = fs_ssep->name_discr.last_unnamed_type_number;
        }  /* if */
        if (fs_ssep->last_closure_type_number > closure) {
          closure = fs_ssep->last_closure_type_number;
        }  /* if */
      }  /* if */
      ssep->name_discr.last_unnamed_type_number = unnamed;
      ssep->last_closure_type_number = closure;
    }  /* if */
#endif /* NEED_NAME_MANGLING */
  } else if (pointers_block != NULL) {
    /* Some other kind of scope for which a pointers block was provided. */
    ssep->assoc_pointers_block = pointers_block;
  }  /* if */
  /* Make sure a non-NULL pointers block was not provided for one of the
     cases in which it would not be used. */
  check_assertion(pointers_block == NULL ||
                  ssep->assoc_pointers_block == pointers_block);
  /* Correct the pointer blocks associated with the IL scope for any elements
     added to them while this scope was not on the scope stack. */
  if (ssep->il_scope != NULL) {
    if (ssep->assoc_pointers_block != NULL) {
      sync_pointers_block_to_il(ssep->assoc_pointers_block, ssep->il_scope);
    } else {
      sync_pointers_block_to_il(&ssep->pointers_block, ssep->il_scope);
    }  /* if */
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* The creation of source sequence entries is suppressed in certain
     contexts. */
  if (!is_primary_translation_unit) {
    /* No source sequence entries are created for secondary translation
       units. */
  } else if (ssep->module_load_context_count > 0) {
    /* When loading entities from a module, source sequence entries are
       invalid. */
    ssep->source_sequence_entries_disallowed =
                                     source_sequence_entries_disallowed = TRUE;
  } else if (kind == (a_scope_kind)sck_template_declaration) {
    if (!prototype_instantiations_in_il) {
      /* Source sequence entries are generated in template declaration scopes
         when prototype instantiations are passed in the IL (prototype
         instantiations for default template arguments are done in
         template declaration scopes). */
      ssep->source_sequence_entries_disallowed =
                                     source_sequence_entries_disallowed = TRUE;
    }  /* if */
  } else if (kind == (a_scope_kind)sck_pragma) {
    ssep->source_sequence_entries_disallowed =
    source_sequence_entries_disallowed = TRUE;
  } else if (kind == (a_scope_kind)sck_template_instantiation) {
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (processing_vccorlib_header) {
      /* Don't update source_sequence_entries_disallowed when scanning
         vccorlib.h. */
    } else if (scanning_generated_code) {
      /* Don't update source_sequence_entries_disallowed when scanning
         generated code. */
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
    } else if (ssep->in_generic_instantiation) {
      /* Don't generate source sequence entries for instantiations of C++/CLI
         generics. */
      ssep->source_sequence_entries_disallowed = TRUE;
      source_sequence_entries_disallowed = TRUE;
    } else if (cli_symbols[csk_cli_namespace] != NULL && assoc_type != NULL &&
               is_member_of_namespace_cli(assoc_type)) {
      /* Do not generate source sequence entries for members of namespace
         "cli".  Only generated declarations can live in that namespace;
         in particular, explicit specializations of templates declared in
         namespace cli are not permitted.  We should therefore not create
         source sequence entries for instantiations in that namespace since
         those would be treated as explicit specializations (which is
         invalid). */
      ssep->source_sequence_entries_disallowed = TRUE;
      source_sequence_entries_disallowed = TRUE;
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
    } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* Do not insert code here. */
    if (instance_sym == NULL) {
      /* If instance_sym is NULL we are pushing the scope for the declaration
         (but not the body) of a template function -- no source sequence
         entries would be involved. */
      source_sequence_entries_disallowed = TRUE;
    } else if (ssep->in_prototype_instantiation ||
               ssep->in_generic_definition) {
      if (!prototype_instantiations_in_il) {
        /* If prototype instantiations are not recorded in the IL, source
           sequence entries are normally not generated during a prototype
           instantiation.  (When they are, they are placed on a list that
           is not part of the IL proper.) */
        source_sequence_entries_disallowed = TRUE;
      } else if (prototype_inst_is_for_class_in_real_instance(assoc_type)) {
        /* Prototype instantiations inside real instantiations should not
           generate source sequence entries. */
        source_sequence_entries_disallowed = TRUE;
#if DO_IL_LOWERING && !PRESERVE_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING
      } else if (any_lowering_being_done) {
        /* Suppress source sequence entries when IL lowering is done. */
        source_sequence_entries_disallowed = TRUE;
#endif /* DO_IL_LOWERING && !PRESERVE_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING */
      } else if (prototype_template_of(template_sym) != template_sym) {
        /* This is not the original prototype instantiation, but a
           prototype instantiation within a real instantiation.  Do not
           record source sequence entries this time around; they were
           already recorded during the prototype instantiation of the
           enclosing template. */
        source_sequence_entries_disallowed = TRUE;
      } else {
        /* In all other cases, produce source sequence entries for the
           prototype instantiation. */
        source_sequence_entries_disallowed = FALSE;
      }  /* if */
    } else if (scope_stack[DEPTH_OF_FILE_SCOPE].
                                    source_sequence_entries_disallowed) {
      /* In the absence of intervening instantiations, if source sequence
         entries are disallowed at the file scope the global variable
         should also indicate that. */
      check_assertion(source_sequence_entries_disallowed == TRUE ||
                      depth_innermost_instantiation_scope != NO_SCOPE_DEPTH);
    } else {
      /* If they are allowed at file scope, they may be permitted for a
         template instantiation. */
      if (assoc_type != NULL) {
        /* We are pushing the scope for a class template instantiation.
           Source sequence entries are normally disallowed for instantiations,
           but should not be disallowed for the instantiation scope pushed
           around a template class specialization in Microsoft mode (unless,
           of course, this specialization occurs during a normal instantiation,
           which is only possible with Microsoft in-class specializations).
           The "!is_incomplete_type" test is done to detect the reactivation of
           a class scope.  Source sequence entries should also not be
           disallowed for the instantiation scope pushed for the reactivation
           of a template class.  In Microsoft mode, certain nonreal classes
           have actual instantiations generated for them so that lookup
           can occur when they are used as base classes.  Source sequence
           entries should not be generated for those classes. */
        source_sequence_entries_disallowed =
        (!(use_microsoft_specialization_scope &&
           ((assoc_type->variant.class_struct_union.is_specialized &&
             !already_in_nonspecialized_instantiation_context) ||
            !is_incomplete_type(assoc_type))) &&
          /*lint --e(506)*/
          !CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS)
        || assoc_type->
                 variant.class_struct_union.is_ms_instantiated_nonreal_class;
      } else {
        /* We are pushing the scope for a function template instantiation
           or for the definition of a template static data member. */
        source_sequence_entries_disallowed =
     !NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS; /*lint !e506*/
      }  /* if */
    }  /* if */
    ssep->source_sequence_entries_disallowed =
                                     source_sequence_entries_disallowed;
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if (!C_mode()) {
    /* Do management related to the object lifetime stack. */
    if (kind == (a_scope_kind)sck_function ||
        kind == (a_scope_kind)sck_template_instantiation ||
        kind == (a_scope_kind)sck_pragma ||
        kind == (a_scope_kind)sck_func_prototype ||
        kind == (a_scope_kind)sck_class_struct_union ||
        kind == (a_scope_kind)sck_class_reactivation ||
        kind == (a_scope_kind)sck_module_decl_import) {
      /* These scopes do not nest properly from the point of view of object
         lifetimes, so break the object lifetime stack and then restore it
         in pop_scope. */
      curr_object_lifetime =
                   scope_stack[DEPTH_OF_FILE_SCOPE].curr_scope_object_lifetime;
    }  /* if */
    if (lifetime != NULL) {
      /* A lifetime was previously allocated and should be reused. */
      curr_object_lifetime = lifetime;
      ssep->curr_scope_object_lifetime = curr_object_lifetime;
    } else if (kind == (a_scope_kind)sck_file) {
      /* Push an object lifetime for the file scope, or reuse one if this
         is a reactivation. */
      curr_object_lifetime = sp->lifetime;
      if (curr_object_lifetime == NULL) {
        push_object_lifetime((an_il_entry_kind)iek_scope, (char *)sp,
                             (an_object_lifetime_kind)(olk_global_static));
      }  /* if */
      ssep->curr_scope_object_lifetime = curr_object_lifetime;
    } else if (is_local_scope_kind(kind) && !ssep->is_reactivation) {
      /* This is the sort of scope for which a new block object lifetime is
         pushed.  Note that no object lifetime is pushed for local scope
         reactivations (since such reactivations only restore context for
         generic lambda instantiations, not object lifetimes). */
      an_object_lifetime_ptr  prev_olp = NULL;
      if (sp != NULL) prev_olp = sp->lifetime;
      push_or_repush_object_lifetime((an_il_entry_kind)iek_scope, (char *)sp,
                                     prev_olp,
                                     (an_object_lifetime_kind)(olk_block),
                                     ssep->is_reactivation);
      ssep->curr_scope_object_lifetime = curr_object_lifetime;
    }  /* if */
    /* Propagate the flag indicating that this scope is inside the compound
       statement of a try block. */
    if (kind == (a_scope_kind)sck_block && (ssep-1)->within_try_block) {
      ssep->within_try_block = TRUE;
    }  /* if */
    if (kind == (a_scope_kind)sck_condition) {
        /* A C++ condition scope is only created when there is a declaration,
           so we know an IL scope will be required (unlike a block scope). */
      check_assertion_str(curr_il_region_number != file_scope_region_number,
                          "push_scope_full: bad region number for condition");
      sp = ensure_il_scope_exists(ssep);
    }  /* if */
  }  /* if */
#if DEBUG
  ++scope_kind_stats[ssep->kind];
  if (depth_scope_stack > max_depth_scope_stack) {
    max_depth_scope_stack = depth_scope_stack;
  }  /* if */
  if (debug_level >= 3) {
    db_scope_stack();
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return sp;
}  /* push_scope_full */


a_scope_ptr push_scope(a_scope_kind         kind,
		       a_scope_number       scope_number_to_reuse,
		       a_type_ptr           assoc_type,
		       a_routine_ptr        assoc_routine)
/*
Interface to push_scope_full that is used for scopes other than template
instantiation scopes.
*/
{
  a_scope_ptr scope;
  scope = push_scope_full(kind, scope_number_to_reuse, assoc_type,
                          assoc_routine, (a_namespace_ptr)NULL,
                          (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                          (a_template_arg_ptr)NULL,
                          (a_template_decl_info_ptr)NULL,
                          (an_object_lifetime_ptr)NULL,
                          (a_scope_ptr)NULL, (a_scope_pointers_block_ptr)NULL,
                          PS_NO_OPTIONS);
  return scope;
}  /* push_scope */


void push_file_scope(a_boolean	is_reactivation)
/*
Activate or reactivate the file scope of the current translation unit.
is_reactivation is TRUE if this is not the first push of the file
scope.
*/
{
  a_push_scope_options_set	ps_options = PS_NO_OPTIONS;

  if (is_reactivation) ps_options |= PS_IS_REACTIVATION;
  (void)push_scope_full((a_scope_kind)sck_file, file_scope_number,
                        (a_type_ptr)NULL, (a_routine_ptr)NULL,
                        (a_namespace_ptr)NULL, (a_symbol_ptr)NULL,
                        (a_symbol_ptr)NULL, (a_template_arg_ptr)NULL,
                        (a_template_decl_info_ptr)NULL,
                        (an_object_lifetime_ptr)NULL,
                        (a_scope_ptr)NULL, (a_scope_pointers_block_ptr)NULL,
                        ps_options);
  /* Add active using directives for the namespaces that should be
     visible because of the transitivity of using directives. */
  add_active_using_directives_for_scope(curr_translation_unit->primary_scope,
                                        &scope_stack[depth_scope_stack],
					NO_DECL_SEQUENCE_NUMBER);
}  /* push_file_scope */


void push_block_scope_with_lifetime(an_object_lifetime_ptr olp)
/*
Push a block scope for which the indicated object lifetime was
previously allocated.  olp can be NULL, in which case the normal
processing is done.
*/
{
  (void)push_scope_full((a_scope_kind)sck_block, NO_SCOPE_NUMBER,
                        (a_type_ptr)NULL, (a_routine_ptr)NULL,
                        (a_namespace_ptr)NULL,
                        (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                        (a_template_arg_ptr)NULL,
                        (a_template_decl_info_ptr)NULL,
                        olp,
                        (a_scope_ptr)NULL, (a_scope_pointers_block_ptr)NULL,
                        PS_NO_OPTIONS);
}  /* push_block_scope_with_lifetime */


void push_block_reactivation_scope(
				a_scope_ptr			scope,
				a_scope_pointers_block_ptr	pointers_block)
/*
Reactivate a block scope.  scope is the IL scope for the block.  pointers_block
is the pointers_block to be used for the scope stack entry.
*/
{
  a_symbol_ptr	sym;

  /* Note that PS_IS_REACTIVATION is not used here because the symbols
     are reentered so that it looks like a normal (non-reactivated)
     block scope. */
  check_assertion(scope != NULL);
  (void)push_scope_full((a_scope_kind)sck_block,
                        scope->number,
                        (a_type_ptr)NULL, (a_routine_ptr)NULL,
                        (a_namespace_ptr)NULL,
                        (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                        (a_template_arg_ptr)NULL,
                        (a_template_decl_info_ptr)NULL,
                        scope->lifetime,
                        scope, pointers_block,
                        PS_NO_OPTIONS);
  /* Reenter any symbols into the symbol table. */
  for (sym = pointers_block->symbols; sym != NULL; sym = sym->next_in_scope) {
    reenter_block_scope_symbol(sym);
  }  /* for */
  if (scope->lifetime != NULL) {
    /* Place the reactivated object lifetime in the right place in the
       destructions of the parent lifetime. */
    check_assertion(curr_object_lifetime == scope->lifetime);
    curr_object_lifetime->parent_destruction_sublist =
                           curr_object_lifetime->parent_lifetime->destructions;
  }  /* if */
}  /* push_block_reactivation_scope */


void pop_block_scope(a_boolean	is_final_pop)
/*
Pop a block scope from the scope stack.  If is_final_pop is FALSE certain
processing that is normally done when the scope is popped (e.g., the
end-of-scope symbol check) is suppressed.  This routine only needs to be
used when is_final_pop is FALSE, otherwise pop_scope can be used to pop
block scopes.
*/
{
  pop_scope_full(is_final_pop ? PS_NO_OPTIONS : PS_NOT_FINAL_POP);
}  /* pop_block_scope */


void push_block_scope(a_scope_pointers_block_ptr	pointers_block)
/*
Push a block scope and use pointers_block as the pointers block of the
scope stack entry so that the scope can be reactivated later.
This routine only needs to be used when a pointers_block is being
supplied, but it can also be used with a NULL pointers_block.
If a pointers block is provided, it is cleared by this routine.
*/
{
  if (pointers_block != NULL) {
    /* Clear the pointers block provided by the caller. */
    clear_scope_pointers_block(pointers_block);
  }  /* if */
  (void)push_scope_full((a_scope_kind)sck_block, NO_SCOPE_NUMBER,
                        (a_type_ptr)NULL, (a_routine_ptr)NULL,
                        (a_namespace_ptr)NULL,
                        (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                        (a_template_arg_ptr)NULL,
                        (a_template_decl_info_ptr)NULL,
                        (an_object_lifetime_ptr)NULL,
                        (a_scope_ptr)NULL, pointers_block,
                        PS_NO_OPTIONS);
}  /* push_block_scope */


a_scope_ptr push_for_init_scope(a_scope_pointers_block_ptr pointers_block)
/*
Push a block scope for a for-init declaration and return a pointer to the IL
scope.  A pointers block to be used for the scope may be specified (or NULL).
*/
{
  a_scope_stack_entry_ptr  ssep;
  a_scope_ptr              sp;

  push_block_scope(pointers_block);
  ssep = &scope_stack[depth_scope_stack];
  ssep->is_for_init_block = TRUE;
  sp = ensure_il_scope_exists(ssep);
  return sp;
}  /* push_for_init_scope */

namespace {

/*
A structure containing any state that is saved when reusing a scope for a
module (via reenter_scope_for_module) that should be restored when done reusing
the scope (via exit_scope_for_module).
*/
struct a_module_scope_reuse_state {
  a_pending_pragma_list
                *curr_construct_pragmas;
                        /* This is what was originally in
                           curr_construct_pragmas before the module load
                           occurred. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_boolean     source_sequence_entries_disallowed;
                        /* This what was originally in
                           source_sequence_entries_disallowed before the module
                           load occurred. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
};  /* a_module_scope_reuse_state */

}  /* namespace */
namespace detail {

template<>
struct Is_trivially_copyable_edg_impl<a_module_scope_reuse_state> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_copyable_edg_impl */

}  /* namespace detail */

using a_module_scope_reuse_state_array =
                                Small_dyn_array<a_module_scope_reuse_state, 3>;
                        /* The type of an array of module scope reuse states
                           used for the module reuse state stack. */

STATIC_THREAD a_module_scope_reuse_state_array
                *module_reuse_state_stack;
                        /* The previous module states. */


static void reenter_scope_for_module(a_scope_stack_entry_ptr ssep)
/*
Reenter the current scope for use by a module.  This function saves the
previous scope stack entry values relevant to modules for later restoration,
then resets them to the desired state for use by modules.
*/
{
  a_module_scope_reuse_state state;

  /* Save the state in the state stack. */
  state.curr_construct_pragmas = ssep->curr_construct_pragmas;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  state.source_sequence_entries_disallowed =
                                      ssep->source_sequence_entries_disallowed;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  module_reuse_state_stack->push_back(state);
  /* Clear the state in the current scope stack entry. */
  ssep->curr_construct_pragmas = NULL;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  ssep->source_sequence_entries_disallowed = TRUE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
}  /* reenter_scope_for_module */


static void exit_scope_for_module(a_scope_stack_entry_ptr ssep)
/*
Exit the current scope that has been reused for use by a module.  This
function restores the previously saved scope stack entry values relevant to
modules.
*/
{
  /* Restore the saved state from the state stack. */
  a_module_scope_reuse_state &state = module_reuse_state_stack->back_elem();

  delete_fe(&ssep->curr_construct_pragmas);
  ssep->curr_construct_pragmas = state.curr_construct_pragmas;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  ssep->source_sequence_entries_disallowed =
                                      state.source_sequence_entries_disallowed;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  /* Drop the state entry. */
  module_reuse_state_stack->pop_back();
}  /* exit_scope_for_module */


/* FIXME: These routines are probably overly simplistic and probably have other
   cases to account for. */
void push_module_declaration_context(
                                   a_scope_ptr              scope,
                                   a_module_scope_push_kind *scope_push_status)
/*
Module entities have an associated scope but may be loaded from any scope.
This function is used to ensure the front end's current scope is the given
scope (primarily for the declaration or definition of a module entity).
scope_push_status should be a pointer to a result variable initialized to
mspk_unattempted (the default initialization is important to verify the result
variable is not being unintentionally recycled).  If the current scope doesn't
match the given scope, this function will push new scope(s) and set
*scope_push_status to mspk_new; otherwise, *scope_push_status will be set to
mspk_unneccessary.
*/
{
  a_scope_stack_entry_ptr  ssep = &scope_stack_top();

  /* This should always be a fresh attempt, detect unintended reuse. */
  check_assertion(*scope_push_status == mspk_unattempted);
  check_assertion(scope != NULL);
  if (ssep->il_scope != scope || curr_object_lifetime != scope->lifetime) {
    /* FIXME: This pushes an instantiation context, that's not an accurate
       description in the scope stack of what's really happening.  In other
       words, no instantiation is occurring here. */
    /* FIXME: This repushes the scope if the object lifetime doesn't match the
       scope's object lifetime.  We should be able to just temporarily restore
       the object lifetime. */
    push_new_top_level_declaration();
    ssep = &scope_stack_top();
    ssep->inside_local_class = inside_local_class = FALSE;
    ssep->module_load_context_count = 1;
#if GENERATE_SOURCE_SEQUENCE_LISTS
    ssep->source_sequence_entries_disallowed = TRUE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    /* FIXME: There may be something further that needs done to fully handle
       namespaces.  See push_instantiation_context "Push the namespace(s)
       containing the definition of the template.". */
    if (scope_is(scope, sck_namespace)) {
      push_namespace_extension_scope(scope->variant.assoc_namespace);
      decl_scope_level = depth_innermost_namespace_scope;
    } else {
      /* Update the decl_scope_level. */
      decl_scope_level = DEPTH_OF_FILE_SCOPE;
    }  /* if */
    /* FIXME: Do we need to reactivate parents? */
    {
      /* We're doing several things with this new scope.

         - We need to update object lifetime stack while storing the old.
         - We need to carry the parent assoc_pointers_block forward.
         - We set the il_scope as an optimization to prevent scope buildup.
      */
      ssep = &scope_stack_top();
      (void)push_scope_full((a_scope_kind)sck_module_decl_import,
                            NO_SCOPE_NUMBER, NULL, NULL, (a_namespace_ptr)NULL,
                            (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                            (a_template_arg_ptr)NULL,
                            (a_template_decl_info_ptr)NULL,
                            (an_object_lifetime_ptr)NULL, (a_scope_ptr)NULL,
                            ssep->assoc_pointers_block, PS_NO_OPTIONS);
      /* Update the il_scope. */
      ssep = &scope_stack_top();
      ssep->il_scope = scope;
    }
    *scope_push_status = mspk_new;
  } else {
    ssep->module_load_context_count++;
    reenter_scope_for_module(ssep);
    *scope_push_status = mspk_unneccessary;
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  source_sequence_entries_disallowed = TRUE;
  check_assertion(scope_stack_top().source_sequence_entries_disallowed);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  check_assertion(ssep->module_load_context_count > 0);
  check_assertion(ssep->curr_construct_pragmas == NULL);
}  /* push_module_declaration_context */


void pop_module_declaration_context(a_module_scope_push_kind scope_push_status)
/*
If scope_push_status is mspk_new, pop the scope(s) that have been pushed by
push_module_declaration_context.  If scope_push_status is mspk_unneccessary
update the associated bookkeeping information.  This function should not be
called if scope_push_status is mspk_unattempted.
*/
{
  check_assertion(scope_push_status != mspk_unattempted);
  process_deferred_class_fixups_and_instantiations(/*for_instantiation=*/TRUE);
  if (scope_push_status == mspk_new) {
    /* "Unwind" the scope stack removing the corresponding pushed scopes. */
    /* Remove the sck_module_decl_import scope. */
    pop_scope();
    /* Remove the namespace extension scope if one exists. */
    if (scope_stack_top().kind == (a_scope_kind)sck_namespace_extension) {
      check_assertion(scope_stack_top().module_load_context_count == 1);
      pop_namespace_extension_scope();
    }  /* if */
    check_assertion(scope_stack_top().module_load_context_count == 1);
    /* Finally remove the instantiation scope. */
    pop_scope();
  } else {
    a_scope_stack_entry_ptr ssep = &scope_stack_top();
    check_assertion(ssep->module_load_context_count > 0 &&
                    (ssep->curr_construct_pragmas == NULL ||
                     ssep->curr_construct_pragmas->is_empty()));
    delete_fe(&ssep->curr_construct_pragmas);
    exit_scope_for_module(ssep);
    ssep->module_load_context_count--;
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Restore the previous global state. */
  {
    a_scope_stack_entry_ptr ssep = &scope_stack_top();

    source_sequence_entries_disallowed =
                                      ssep->source_sequence_entries_disallowed;
  }
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  /* A template declaration may have cleared this - reset it now. */
  /* FIXME: This seems expensive to do it here - is there a better way to
     determine if it's needed? */
  set_active_using_list_scope_depths(depth_scope_stack, /*set_value=*/TRUE,
                                     NO_DECL_SEQUENCE_NUMBER);
}  /* pop_module_declaration_context */


static void microsoft_using_directive_bug_processing(
					a_namespace_ptr		nsp,
					a_boolean		end_of_scope)
/*
The Microsoft compiler (as of Visual C++ 6.0) has a bug that causes a
namespace nominated by a using-directive to be visible in the file scope.

  namespace M  { 
    class C {};
  }
  namespace N {
    using namespace M;
  }
  namespace N {}
  void f(C*); // C is visible in the global namespace

This bug only occurs when the using-directive is in a namespace that
has been extended (i.e., not one for which there has only been a
primary namespace definition).

This routine implements this bug by taking the using-directives from the
namespace being pushed (when end_of_scope is FALSE) and applying them to
the file scope.

Another aspect of this bug involves a using-directive that refers to the
current namespace:

  namespace N {}
  namespace N {
  class C{};
    using namespace N;
  }
  void f(C*); // C is visible in the global namespace

If such a using-directive appears in a reactivated namespace, using-directives
from that namespace act as if they appeared in the file scope.  This routine
implements this bug by taking the using-directives from the namespace being
popped (when end_of_scope is TRUE) and applying the using-directive to the
file scope if it refers to the namespace being popped.
*/
{
  a_using_decl_ptr	udp = nsp->variant.assoc_scope->using_directives;
  a_scope_depth		depth;
  a_boolean		any_using_dirs_added = FALSE;

  /* Create using-directives for each of the namespaces nominated in a
     using-directive of the namespace scope specified by nsp. */
  while (udp != NULL) {
    a_namespace_ptr	udp_nsp;
    check_assertion(udp->is_using_directive);
    check_assertion(udp->entity.kind == iek_namespace);
    /* Get a pointer to the namespace to be used. */
    udp_nsp = skip_namespace_aliases((a_namespace_ptr)udp->entity.ptr);
    if (!end_of_scope || udp_nsp == nsp) {
      a_memory_region_number region_to_switch_back_to;
      switch_to_file_scope_region(&region_to_switch_back_to);
      make_using_directive(udp_nsp, DEPTH_OF_FILE_SCOPE,
                           &null_source_position,
                           /*compiler_generated=*/TRUE,
                           /*inline_namespace=*/FALSE,
                          (an_attribute_ptr)NULL);
      any_using_dirs_added = TRUE;
      switch_back_to_original_region(region_to_switch_back_to);
    }  /* if */
    udp = udp->next;
  }  /* while */
  /* Update all of the scopes on the scope stack to indicate that symbols
     visible as a result of a using-directive may be visible. */
  if (any_using_dirs_added) {
    for (depth = depth_scope_stack; depth >= DEPTH_OF_FILE_SCOPE; depth--) {
      scope_stack[depth].inactive_symbols_may_be_visible = TRUE;
    }  /* for */
  }  /* if */
}  /* microsoft_using_directive_bug_processing */


static a_scope_ptr push_namespace_scope_full(a_scope_kind    kind,
                                             a_namespace_ptr assoc_namespace,
                                             a_scope_depth   prev_scope)
/*
Interface to push_scope_full that is used for sck_namespace scopes
("original" namespace definitions).  It is also used to reopen an
sck_namespace IL scope by pushing an sck_namespace_extension scope stack
entry (for "extension-namespace-definitions").  This is used both when
a namespace is defined and when an instantiation scope is pushed for a
template defined in a namespace.  If prev_scope is not NO_SCOPE_DEPTH, the
previous scope for the namespace is set to prev_scope.
*/
{
  a_scope_ptr     scope;
  a_scope_number  scope_number_to_reuse = NO_SCOPE_NUMBER;

  check_assertion_str(assoc_namespace != NULL &&
                      !assoc_namespace->is_namespace_alias &&
                      ((assoc_namespace->variant.assoc_scope == NULL) ==
                                       (kind == (a_scope_kind)sck_namespace)),
                      "push_namespace_scope_full: bad assoc_namespace ptr");
  if (microsoft_bugs && microsoft_version <= 1200 &&
      kind == (a_scope_kind)sck_namespace_extension) {
    /* Make any using-directives in this namespace visible in the file
       scope (to emulate a Microsoft bug). */
    microsoft_using_directive_bug_processing(assoc_namespace,
                                             /*end_of_scope=*/FALSE);
  }  /* if */
  if (kind == (a_scope_kind)sck_namespace_extension ||
      kind == (a_scope_kind)sck_namespace_reactivation) {
    scope_number_to_reuse = assoc_namespace->variant.assoc_scope->number;
  }  /* if */
  scope = push_scope_full(kind, scope_number_to_reuse, (a_type_ptr)NULL,
                          (a_routine_ptr)NULL, assoc_namespace,
                          (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                          (a_template_arg_ptr)NULL,
                          (a_template_decl_info_ptr)NULL,
                          (an_object_lifetime_ptr)NULL,
                          (a_scope_ptr)NULL, (a_scope_pointers_block_ptr)NULL,
                          PS_NO_OPTIONS);
  if (prev_scope != NO_SCOPE_DEPTH) {
    scope_stack_top().previous_scope = prev_scope;
  }  /* if */
  /* Add active using directives for the namespaces that should be
     visible because of the transitivity of using directives. */
  add_active_using_directives_for_scope(assoc_namespace->variant.assoc_scope,
                                        &scope_stack[depth_scope_stack],
					NO_DECL_SEQUENCE_NUMBER);
  return scope;
}  /* push_namespace_scope_full */


a_scope_ptr push_namespace_scope(a_scope_kind    kind,
                                 a_namespace_ptr assoc_namespace)
/*
Interface to push_namespace_scope_full that supplies a default value for
prev_scope.
*/
{
  return push_namespace_scope_full(kind, assoc_namespace, NO_SCOPE_DEPTH);
}  /* push_namespace_scope */


void refresh_scope_stack()
/*
Refresh the scope stack state for any changes to information with caches
managed by the scope stack.  This is typically called by multi-translation unit
code to bring the scope stack back into sync with the current translation unit
state.
*/
{
  a_scope_stack_entry_ptr  ssep;

  for (ssep = &scope_stack[depth_scope_stack]; ssep != NULL;
       ssep = ssep->kind == (a_scope_kind)sck_file ? NULL : ssep - 1) {
    if (ssep->il_scope != NULL) {
      /* Refresh the pointer blocks in case anything has changed. */
      if (ssep->assoc_pointers_block != NULL) {
        sync_pointers_block_to_il(ssep->assoc_pointers_block, ssep->il_scope);
      } else {
        sync_pointers_block_to_il(&ssep->pointers_block, ssep->il_scope);
      }  /* if */
      /* Note that if a scope is on the stack more than once, this will have
         the effect of setting it to the outermost scope depth. */
      ssep->il_scope->depth_in_scope_stack = scope_depth_of(ssep);
    }  /* if */
  }  /* for */
}  /* refresh_scope_stack */


void pop_namespace_scope(void)
/*
Pop a namespace or namespace extension scope.  Unlike push_namespace_scope,
this routine is used only for namespace scopes that appear in the source
program, not used when popping the namespace scope pushed as part of the
template instantiation process.
*/
{
  a_scope_stack_entry_ptr	ssep = &scope_stack[depth_scope_stack];
  a_scope_kind			kind;
  a_boolean			initial_decl_of_namespace_std;

  kind = ssep->kind;
  /* The first use of namespace std is actually pushed as a namespace
     extension.  This must be ignored for purposes of checking this
     Microsoft bug. */
  initial_decl_of_namespace_std = ssep->initial_decl_of_namespace_std;
  check_assertion(kind == (a_scope_kind)sck_namespace ||
                  kind == (a_scope_kind)sck_namespace_extension);
  pop_scope();
  if (microsoft_bugs && microsoft_version <= 1200 &&
      kind == (a_scope_kind)sck_namespace_extension &&
      !initial_decl_of_namespace_std) {
    /* Make certain using-directives in this namespace visible in the file
       scope (to emulate a Microsoft bug). */
    microsoft_using_directive_bug_processing(ssep->assoc_namespace,
                                             /*end_of_scope=*/TRUE);
  }  /* if */
}  /* pop_namespace_scope */


static a_scope_depth find_depth_of_common_scope(a_namespace_ptr nsp)
/*
Find the innermost scope on the scope stack that is also either the
namespace pointed to by nsp or is a parent of nsp.  Return the
depth of the scope stack entry associated with the common scope or
DEPTH_OF_FILE_SCOPE if there is none.
*/
{
  a_scope_stack_entry_ptr	ssep = NULL;
  a_scope_depth			common_depth;

  for (; nsp != NULL; nsp = parent_namespace_or_null(nsp)) {
    a_scope_ptr	ns_scope = nsp->variant.assoc_scope;
    if (ns_scope->depth_in_scope_stack == NO_SCOPE_DEPTH) {
      /* The scope associated with this namespace is not on the scope stack.
         Keep looking. */
      continue;
    }  /* if */
    /* Even if the scope is on the stack, it could be invisible because of
       an intervening instantiation scope.  Look for the namespace in the
       currently visible scopes. */
    for (ssep = &scope_stack[depth_innermost_namespace_scope];
         ssep != NULL;
         ssep = previous_scope_of(ssep)) {
      if (ssep->il_scope == ns_scope) break;
    }  /* for */
    /* If we found a match, exit the loop. */
    if (ssep != NULL) break;
  }  /* for */
  /* Return DEPTH_OF_FILE_SCOPE if there is no common namespace scope. */
  common_depth = ssep != NULL ? scope_depth_of(ssep) : DEPTH_OF_FILE_SCOPE;
  return common_depth;
}  /* find_depth_of_common_scope */


static
void push_namespace_extension_for_instantiation(a_namespace_ptr nsp,
                                                a_namespace_ptr common_nsp,
                                                a_scope_depth   prev_scope)
/*
Push namespace extension scopes needed to instantiate an entity defined
in the namespace pointed to by nsp.  The scopes to be pushed are nsp
and its parent scopes up to, but not including, the scope pointed to
by common_nsp.  Link the previous scope entries so that prev_scope is
the previous scope of the first scope pushed by this routine.
*/
{
  a_namespace_ptr		parent_nsp;

  /* The entry isn't on the stack.  Push any parent namespaces, then push
     the specified namespace. */
  parent_nsp = parent_namespace_or_null(nsp);
  if (parent_nsp != NULL && parent_nsp != common_nsp) {
    /* A namespace nested in another namespace.  Push the parent
       namespace. */
    push_namespace_extension_for_instantiation(parent_nsp, common_nsp,
                                               prev_scope);
    prev_scope = NO_SCOPE_DEPTH;
  }  /* if */
  /* For the first scope pushed by this routine (the one whose parent is
     common_nsp), set the previous scope to the one passed from the
     caller.  For subsequent scopes, prev_scope will have been changed to
     NO_SCOPE_DEPTH above, so this assignment won't be done. */
  (void)push_namespace_scope_full((a_scope_kind)sck_namespace_extension, nsp,
                                  prev_scope);
}  /* push_namespace_extension_for_instantiation */


static
a_namespace_ptr referencing_namespace_for_instance(a_symbol_ptr	instance_sym,
                                                   a_boolean	is_lambda_body)
/*
Given a symbol that points to a particular instance of a template, return
the namespace pointer of the namespace in which an instantiation of
the template was first required.

instance_sym points to a symbol for an instance of a template.  It may also
be NULL if we don't yet know which instance we are dealing with.
is_lambda_body is TRUE if the instance is a generic lambda instantiation.
*/
{
  a_namespace_ptr	nsp = NULL;

  if (instance_sym == NULL || is_lambda_body) {
    /* We don't know which instance is being used yet.  Determine the
       referencing namespace from the scope stack.  There is not always
       a referencing namespace for lambdas, so they are also handled here. */
    nsp = determine_referencing_namespace();
  } else if (instance_sym->kind == (a_symbol_kind)sk_class_or_struct_tag ||
             instance_sym->kind == (a_symbol_kind)sk_union_tag) {
    /* The instance points to a class symbol.  Return the referencing
       namespace from the class symbol supplement. */
    a_class_symbol_supplement_ptr	cssp;
    cssp = instance_sym->variant.class_struct_union.extra_info;
    nsp = cssp->referencing_namespace;
  } else if (symbol_is(instance_sym, sk_type) || 
             symbol_is(instance_sym, sk_enum_tag)) {
    /* The instance points to a type for a template alias or enum.  Use the
       current innermost namespace. */
    nsp = scope_stack[depth_innermost_namespace_scope].assoc_namespace;
  } else {
    /* The instance points to a routine or static data member.  Return
       the referencing namespace from the template instance record. */
    a_template_instance_ptr	tip;
    if (instance_sym->kind == (a_symbol_kind)sk_static_data_member) {
      tip = instance_sym->variant.static_data_member.instance_ptr;
    } else if (symbol_is(instance_sym, sk_variable)) {
      tip = instance_sym->variant.variable.instance_ptr;
    } else {
      check_assertion(instance_sym->kind == (a_symbol_kind)sk_routine ||
                      instance_sym->kind == (a_symbol_kind)sk_member_function);
      tip = instance_sym->variant.routine.instance_ptr;
    }  /* if */
    nsp = tip->referencing_namespace;
  }  /* if */
  return nsp;
}  /* referencing_namespace_for_instance */


static
void get_parent_information_for_template(
					a_scope_ptr	sp,
					a_boolean	is_lambda_body,
					a_symbol_ptr	template_sym,
					a_symbol_ptr	instance_sym,
					a_namespace_ptr	*p_nsp,
					a_type_ptr	*p_tp)
/*
Determine the namespace and class scopes that must be reactivated in
order for a given instantiation to be done.  sp points to the enclosing
scope of the template declaration, template_sym is the symbol of the
template, instance_sym is the symbol for the instance being created
and may be NULL.  is_lambda_body is TRUE if the instance is a generic
lambda instantiation.  *nsp and *tp are returned by this routine, and point
to the namespace and class that must be reactivated.
*/
{
  a_type_ptr		parent_type = NULL;
  a_namespace_ptr	parent_namespace = NULL;

  /* Determine the parent type and namespace based on the enclosing scope
     of the template. */
  if (sp == NULL) {
    /* This will be the case when the template is a template template
       parameter and we are rescanning a dependent template parameter. */
  } else {
    /* Go through the parent scopes to find the nearest enclosing
       class scope (if any) and namespace scope (if any). */
    while (sp != NULL && !scope_is(sp, sck_file)) {
      if (scope_is(sp, sck_class_struct_union)) {
        a_type_ptr	tp = sp->variant.assoc_type;
        /* Record the first enclosing class type found. */
        if (parent_type == NULL) {
          parent_type = tp;
        }  /* if */
        sp = get_parent_scope_of(tp);
      } else if (scope_is(sp, sck_namespace)) {
        a_namespace_ptr	nsp = sp->variant.assoc_namespace;
        if (parent_namespace == NULL) {
          parent_namespace =  nsp;
        }  /* if */
        sp = get_parent_scope_of(nsp);
      } else {
        sp = sp->parent;
      }  /* if */
    }  /* while */
  }  /* if */
  if ((instance_sym == NULL || is_lambda_body) &&
      (template_sym == NULL ||
       template_sym->kind == (a_symbol_kind)sk_function_template ||
       template_sym->kind == (a_symbol_kind)sk_variable_template)) {
    /* Use the parent information determined above.

       When there is no specific instance being instantiated, that indicates
       that we are instantiating something like a template parameter type
       that depends on another template parameter, or a default template
       argument whose type depends on a template parameter.  This is also
       the case when scanning the function declaration of a function template
       to do the partial instantiation of the function.  Note that this
       is not the case when scanning the default function arguments
       though.  For the function template case, the parent information is
       determined by the scope that contained the template declaration
       based on which the current instantiation is being done. */
    *p_tp = parent_type;
    *p_nsp = parent_namespace;
  } else if (!template_sym->is_class_member && 
             template_sym->kind == (a_symbol_kind)sk_function_template) {
    /* When generating an instance of a function template (that is not a
       member template) that was defined in some other class scope, we need
       to create the context of that class scope in order to do the 
       instantiation.  The namespace to be used is the namespace
       of which the template is a member.  Functions cannot be defined
       using qualified names in friend declarations, so the namespace
       must be the same as the namespace containing the class. */
    *p_tp = parent_type;
    *p_nsp = parent_namespace_for_symbol(instance_sym);
  } else {
    /* If we are instantiating a particular instance of a template, get
       the parent information from the template being instantiated. */
    a_symbol_ptr	sym_to_use;
    sym_to_use = instance_sym != NULL ? instance_sym : template_sym;
    *p_nsp = parent_namespace_for_symbol(sym_to_use);
    if (sym_to_use->is_class_member) {
      *p_tp = sym_parent_class(sym_to_use);
    } else {
      *p_tp = NULL;
    }  /* if */
  }  /* if */
}  /* get_parent_information_for_template */


static void push_single_class_reactivation_scope(
				a_type_ptr			class_type,
				a_push_scope_options_set	options) 
/*
Reactivate the class indicated by class type.  Do not push the scopes for
any enclosing class types or namespaces.  "options" is the set of option
flags passed to the push scope routines.
*/
{
  a_scope_ptr	il_scope;

  /* Find the IL scope to get the scope number. */
  il_scope = class_type->variant.class_struct_union.extra_info->assoc_scope;
  check_assertion_str2(!scope_is_null_or_placeholder(il_scope),
                       "push_single_class_reactivation_scope:",
                       "NULL assoc_scope");
  (void)push_scope_full((a_scope_kind)sck_class_reactivation,
                        il_scope->number, class_type,
                        (a_routine_ptr)NULL, (a_namespace_ptr)NULL,
                        (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                        (a_template_arg_ptr)NULL,
                        (a_template_decl_info_ptr)NULL,
                        (an_object_lifetime_ptr)NULL,
                        (a_scope_ptr)NULL, (a_scope_pointers_block_ptr)NULL,
                        options);
}  /* push_single_class_reactivation_scope */


/* Forward declaration. */
static void push_simple_instantiation_scope(
                            a_template_decl_info_ptr	decl_info,
                            a_type_ptr			assoc_type,
                            a_routine_ptr		assoc_routine,
                            a_symbol_ptr		instance_sym,
                            a_symbol_ptr		template_sym,
                            a_template_arg_ptr		template_arg_list,
                            a_push_scope_options_set	options);

static void reactivate_variable_context(a_template_decl_info_ptr  decl_info,
                                        a_symbol_ptr              var_sym,
                                        a_push_scope_options_set  options)
/*
Unlike functions and classes, a variable template instance does not have its
own scope, but its template parameters need to be reactivated.  This routine is
called when that needs to be done.  decl_info is the declaration information
for the variable template for which var_sym is an instance.  options are the
push_scope options being used.
*/
{
  a_template_instance_ptr  tip;
  a_variable_ptr           var_ptr;
  a_template_arg_ptr       template_arg_list;

  var_ptr = variable_for_symbol(var_sym);
  tip = template_instance_for_symbol(var_sym);
  template_arg_list = templ_arg_list_for_variable(var_ptr);
  push_simple_instantiation_scope(decl_info, (a_type_ptr)NULL,
                                  (a_routine_ptr)NULL, var_sym,
                                  tip->template_sym, template_arg_list,
                                  options);
}  /* reactivate_variable_context */


static a_symbol_ptr variable_instance_for_reactivation(
                                        a_symbol_ptr              var_sym,
                                        a_routine_ptr             encl_routine,
                                        a_push_scope_options_set  options)
/*
Return the variable template instance that should be used when reactivating the
instantiation context for var_sym.  encl_routine is the enclosing function
being reactivated, if any; options are the push_scope options for the
reactivation.  If var_sym is a prototype instance but the reactivation is for a
real instantiation, a non-prototype instance of the same template is preferred
so that template parameters are bound to real arguments.
*/
{
  a_symbol_ptr    result = var_sym;
  a_variable_ptr  var_ptr = variable_for_symbol(var_sym);

  if (var_ptr->is_prototype_instantiation &&
      (options & PS_PROTOTYPE_INSTANTIATION) == 0) {
    a_template_instance_ptr  tip = template_instance_for_symbol(var_sym);
    a_symbol_ptr             template_sym = tip->template_sym;

    /* Prefer the variable recorded as the parent of the enclosing lambda
       closure, when that is a real instance of the same template. */
    if (encl_routine != NULL &&
        encl_routine->source_corresp.is_class_member) {
      a_type_ptr  parent_class = parent_class_of(encl_routine);
      a_class_type_supplement_ptr
                  ctsp = class_type_supp(parent_class);
      if (ctsp->is_lambda_closure_class &&
          ctsp->defined_in_variable_initializer &&
          ctsp->lambda_parent.variable != NULL) {
        a_symbol_ptr parent_var_sym = symbol_for(ctsp->lambda_parent.variable);
        if (is_template_variable_symbol(parent_var_sym)) {
          a_template_instance_ptr parent_tip =
                                  template_instance_for_symbol(parent_var_sym);
          if (parent_tip->template_sym == template_sym &&
              !ctsp->lambda_parent.variable->is_prototype_instantiation) {
            result = parent_var_sym;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* variable_instance_for_reactivation */


static void reactivate_parent_context(
			a_template_decl_info_ptr	decl_info,
			a_scope_ptr			scope,
			a_type_ptr			definition_class,
			a_symbol_ptr			instance_sym,
			a_type_ptr			assoc_type,
			a_routine_ptr			assoc_routine,
			a_push_scope_options_set	options);


static
void reactivate_class_context(
                      a_template_decl_info_ptr	decl_info,
                      a_type_ptr		parent_class,
                      a_symbol_ptr		instance_sym,
		      a_type_ptr		assoc_type,
		      a_routine_ptr		assoc_routine,
		      a_push_scope_options_set	options) 
/*
Reactivate the class specified by parent_class.  If the class is a template,
also push the instantiation scope.    See push_instantiation_context for
information about the parameters.
*/
{
  a_type_ptr                        class_type;
  a_symbol_ptr                      class_sym;
  a_symbol_ptr                      template_sym = NULL;
  a_template_symbol_supplement_ptr  tssp = NULL;
  a_template_arg_ptr                template_arg_list;
  a_template_decl_info_ptr	    enclosing_tdip;
  a_boolean			    is_template;
  a_symbol_ptr			    enclosing_instance_sym = NULL;
  a_type_ptr			    enclosing_assoc_type = NULL;
  a_routine_ptr			    enclosing_assoc_routine = NULL;

  class_type = skip_typerefs(parent_class);
  class_sym = (a_symbol_ptr)class_type->source_corresp.assoc_info;
  check_assertion_str2(class_sym != NULL,
                       "reactivate_class_context:",
                       "class type has NULL assoc_info");
  is_template = (options & PS_NEW_INSTANTIATION_CONTEXT) == 0 &&
                      is_template_class_and_not_specific_def_symbol(class_sym);
  if (is_template) {
    a_class_symbol_supplement_ptr     cssp;
    cssp = class_sym->variant.class_struct_union.extra_info;
    template_sym = cssp->class_template;
    tssp = template_sym->variant.template_info;
    /* Determine which template declaration information is to be used
       when pushing the instantiation scope for the enclosing class.
       For a member template defined outside of the class you need
       to use the template parameter list from the definition of
       the member template (as supplied by the decl_info pointer), not that
       of the enclosing class.  If decl_info is NULL, the information from
       the class is used instead. */
    if (decl_info == NULL) decl_info = cache_for_template(tssp)->decl_info;
    enclosing_tdip = decl_info->enclosing_template_decl;
    /* If no instance symbol is passed from the caller, use the symbol
       and type from the class that is being reactivated.  Otherwise,
       use the symbol, type and/or routine passed by the caller. */
    if (instance_sym == NULL) {
      instance_sym = class_sym;
      assoc_type = class_type;
      assoc_routine = NULL;
    }  /* if */
    enclosing_instance_sym = NULL;
  } else {
    /* If this is not a template, use the decl_info value passed in as
       the value passed to the recursive call below.  If no template
       declaration information was supplied, use the information associated
       with this class. */
    if (decl_info == NULL) {
      /* Note that the "is_any_template.." version is used below so that
         it will be TRUE for nested classes of prototype instantiations. */
      if (is_any_template_instance_class_symbol(class_sym) &&
          !is_template_instance_specific_def_symbol(class_sym)) {
        template_sym = template_symbol_for_class_symbol(class_sym);
        /* Get the template declaration information associated with
           the class. */
        tssp = template_supplement_for_symbol(template_sym);
        check_assertion(tssp != NULL);
        decl_info = cache_for_template(tssp)->decl_info;
      }  /* if */
    }  /* if */
    enclosing_tdip = decl_info;
    enclosing_instance_sym = instance_sym;
    enclosing_assoc_type = assoc_type;
    enclosing_assoc_routine = assoc_routine;
  }  /* if */
  if (class_sym->is_class_member) {
    /* If this is not the outermost class, reactivate any enclosing
       classes. */
    reactivate_class_context(enclosing_tdip,
                             sym_parent_class(class_sym),
                             enclosing_instance_sym,
                             enclosing_assoc_type,
                             enclosing_assoc_routine,
                             options);
  } else {
    a_scope_ptr	parent_scope;
    parent_scope = get_parent_scope_of(class_type);
    if (parent_scope != NULL &&
        is_local_scope_kind(parent_scope->kind)) {
      reactivate_parent_context(enclosing_tdip, parent_scope,
                                (a_type_ptr)NULL,
                                instance_sym, assoc_type, assoc_routine,
                                options);
    }  /* if */
  }  /* if */
  if (is_template) {
    /* Push a template instantiation scope associated with the
       class in which this template was defined. */
    a_push_scope_options_set	ps_options = PS_NO_OPTIONS;
    /* If the class is a prototype instantiation, pass the appropriate flag
       to push_scope. */
    if (is_prototype_instantiation_type(class_type)) {
      ps_options = PS_PROTOTYPE_INSTANTIATION;
    } else if (is_cli_generic_definition_type(class_type)) {
      ps_options |= PS_GENERIC_DEFINITION;
    } else if (class_type->variant.class_struct_union.
                                            is_ms_instantiated_nonreal_class) {
      ps_options |= PS_NONREAL_INSTANTIATION;
    }  /* if */
    if ((options & PS_EXCEPTION_SPEC) != 0) ps_options |= PS_EXCEPTION_SPEC;
    template_arg_list = templ_arg_list_for_class(class_type);
    push_simple_instantiation_scope(decl_info, assoc_type, assoc_routine,
                                    instance_sym, template_sym,
                                    template_arg_list, ps_options);
  }  /* if */
  /* Reactivate the enclosing class scope. */
  push_single_class_reactivation_scope(class_type, options);
}  /* reactivate_class_context */


void reactivate_local_context(
			a_template_decl_info_ptr	decl_info,
			a_scope_ptr			scope,
			a_symbol_ptr			instance_sym,
			a_type_ptr			assoc_type,
			a_routine_ptr			assoc_routine,
			a_push_scope_options_set	options) 
/*
Reactivate the local scope specified by scope.  See push_instantiation_context
for information about the parameters.
*/
{
  a_routine_ptr			rp;
  a_scope_ptr			parent = NULL;
  a_boolean			is_template = FALSE;
  a_template_decl_info_ptr	parent_tdip = decl_info;
  a_symbol_ptr			rout_template_sym;

  /* If the parent scope is not a file or namespace scope, reactivate it
     first. */
  rp = scope_is(scope, sck_function) ? assoc_routine : NULL;
  if (rp != NULL) {
    parent = get_parent_scope_of(rp);
    is_template = rp->is_template_function && !rp->is_specialized &&
                  rp->template_arg_list != NULL;
    if (is_template) {
      /* When no decl_info was supplied (e.g., from the lambda token rescan
         path, which only needs to make local variables from the enclosing
         function visible during name lookup), suppress the template
         instantiation scope push below.  The template parameter symbols are
         already visible from the surrounding instantiation context. */
      if (decl_info == NULL) {
        is_template = FALSE;
      } else if (decl_info->variable_instance_sym != NULL) {
        /* A generic lambda inside a variable template initializer: do not pass
           the variable template's decl_info to the enclosing class
           reactivation.  The variable template context is restored after the
           parent is reactivated. */
        parent_tdip = NULL;
      } else {
        parent_tdip = decl_info->enclosing_template_decl;
      }  /* if */
    }  /* if */
  } else {
    parent = scope->parent;
  }  /* if */
  if (rp != NULL && rp->defined_in_friend_decl &&
      rout_befriending_classes(rp) != NULL) {
    /* A routine defined in a friend declaration is lexically in the scope of
       the befriending class, but that class does not appear in the routine's
       parent scope chain.  Reactivate the defining class (kept at the head of
       befriending_classes) so that unqualified references to class members can
       be resolved. */
    reactivate_class_context(parent_tdip,
                             rout_befriending_classes(rp)->class_type,
                             instance_sym, assoc_type, assoc_routine,
                             options);
  } else if (scope_is(parent, sck_file) && !scope_is(parent, sck_namespace)) {
    /* Nothing to do. */
  } else if (scope_is(parent, sck_class_struct_union)) {
    /* For a class scope, get the class type to be reactivated. */
    reactivate_parent_context(parent_tdip, parent, parent->variant.assoc_type,
                              instance_sym, assoc_type, assoc_routine,
                              options);
  } else {
    /* Otherwise, reactivate the parent scope. */
    reactivate_parent_context(parent_tdip, parent, (a_type_ptr)NULL,
                              instance_sym, assoc_type, assoc_routine,
                              options);
  }  /* if */
  if (decl_info != NULL && decl_info->variable_instance_sym != NULL) {
    /* Restore the enclosing variable template instantiation after the
       class/namespace context has been reactivated. */
    reactivate_variable_context(decl_info->enclosing_template_decl,
                                variable_instance_for_reactivation(
                                              decl_info->variable_instance_sym,
                                              rp, options),
                                options);
  }  /* if */
  if (is_template) {
    /* For a function template, push the instantiation scope for the
       template arguments. */
    a_symbol_ptr	rout_sym;
    rout_sym = symbol_for(rp);
    check_assertion(rout_sym != NULL &&
                    (symbol_is(rout_sym, sk_routine) ||
                     symbol_is(rout_sym, sk_member_function)));
    rout_template_sym = rout_sym->variant.routine.instance_ptr->template_sym;
    push_simple_instantiation_scope(decl_info, (a_type_ptr)NULL,
                                    rp, rout_sym, rout_template_sym,
                                    rp->template_arg_list, options);
  }  /* if */
  (void)push_scope_full(scope->kind, scope->number, (a_type_ptr)NULL,
                        rp, (a_namespace_ptr)NULL,
                        (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                        (a_template_arg_ptr)NULL,
                        (a_template_decl_info_ptr)NULL,
                        (an_object_lifetime_ptr)NULL,
                        scope, (a_scope_pointers_block_ptr)NULL,
                        PS_IS_REACTIVATION);
}  /* reactivate_local_context */


static void reactivate_parent_context(
			a_template_decl_info_ptr	decl_info,
			a_scope_ptr			scope,
			a_type_ptr			definition_class,
			a_symbol_ptr			instance_sym,
			a_type_ptr			assoc_type,
			a_routine_ptr			assoc_routine,
			a_push_scope_options_set	options)
/*
Push the parent scope of "scope".  See push_instantiation_context for
information about the parameters.
*/
{
  a_scope_kind	kind = scope == NULL ? (a_scope_kind)sck_none : scope->kind;
  a_boolean	is_generic_lambda = (options & PS_IS_GENERIC_LAMBDA) != 0;

  /* If a class was specified and this is not a generic lambda, the scope
     will be the file scope or a namespace scope.  Ignore the specified
     scope and reactivate the class scope below. */
  if (!is_generic_lambda && definition_class != NULL) {
    kind = (a_scope_kind)sck_class_struct_union;
  } else if (is_local_scope_kind(kind) && !is_generic_lambda) {
    /* We are pushing a local scope for something that is not a generic
       lambda.  Don't actually push the scope.  This can happen in cases
       such as an inheriting constructor in a local class. */
    kind = (a_scope_kind)sck_none;
  }  /* if */
  switch (kind) {
    case sck_block:
    case sck_condition:
      reactivate_local_context(decl_info, scope,
                               instance_sym, (a_type_ptr)NULL,
                               (a_routine_ptr)NULL,
                               options);
      break;
    case sck_function:
      reactivate_local_context(decl_info, scope,
                               instance_sym, (a_type_ptr)NULL,
                               scope->variant.routine.ptr,
                               options);
      break;
    case sck_class_struct_union:
      if (definition_class != NULL) {
        /* If a definition class was provided, reactivate its context. */
        reactivate_class_context(decl_info, definition_class,
                                 instance_sym, assoc_type, assoc_routine,
                                 options);
      }  /* if */
      break;
    default:
      break;
  }  /* switch */
  if (scope != NULL && (options & PS_IS_GENERIC_LAMBDA) != 0) {
    /* Apply any using directives from the scope that was reactivated. */
    add_active_using_directives_for_scope(scope,
                                          &scope_stack[depth_scope_stack],
                                          NO_DECL_SEQUENCE_NUMBER);
  }  /* if */
}  /* reactivate_parent_context */


static void reactivate_instantiation_context(
			a_template_decl_info_ptr	decl_info,
			a_scope_ptr			scope,
			a_type_ptr			definition_class,
			a_symbol_ptr			instance_sym,
			a_type_ptr			assoc_type,
			a_routine_ptr			assoc_routine,
			a_push_scope_options_set	options) 
/*
Reactivate the set of scopes starting at "scope" and working outward
until a namespace scope is found.  See push_instantiation_context for
information about the parameters.
*/
{
  reactivate_parent_context(decl_info, scope, definition_class,
                           instance_sym, assoc_type, assoc_routine, options);
}  /* reactivate_instantiation_context */


void make_class_definition_context_visible(void)
/*
When a static data member is instantiated, most of the declaration is
scanned without certain scopes being visible.  Once we reach the point at
which the class scope should be reactivated, we need to update the scope
stack so that the class reactivation and other enclosing scopes will be
considered.
*/
{
  a_scope_stack_entry_ptr	ssep;

  for (ssep = &scope_stack[depth_of_initial_lookup_scope]; ssep != NULL;
       ssep = previous_scope_of(ssep)) {
    ssep->ignore_during_normal_lookup = FALSE;
  }  /* for */
}  /* make_class_definition_context_visible */


static void push_instantiation_context(
		a_template_decl_info_ptr	enclosing_decl_info,
		a_namespace_ptr			definition_nsp,
		a_type_ptr			definition_class,
		a_namespace_ptr			reference_nsp,
		a_scope_depth			*p_common_depth,
		a_scope_depth			*p_definition_depth,
		a_scope_depth			*p_context_depth,
		a_scope_depth			*p_after_definition_depth,
		a_scope_ptr			context_scope,
		a_symbol_ptr			instance_sym,
		a_type_ptr			assoc_type,
		a_routine_ptr			assoc_routine,
		a_push_scope_options_set	options)
/*
Pushes the scopes necessary to create the appropriate context for a
particular instantiation.  This process includes

- pushing the namespace containing the point of instantiation for the template

- pushing the namespaces necessary to recreate the context in which the
  template was defined, up to the "common" scope (the first scope that is
  part of both the definition and referencing context)

- pushing any class reactivation scopes that may be necessary if the template
  definition context is inside a class

- pushing instantiation scopes associated with the class reactivations if the
  class being reactivated is a class template

- pushing any block scopes needed to restore the context for a generic
  lambda

enclosing_decl_info is the template declaration information
for the enclosing template, if any, or NULL if there is none.
definition_nsp is the template definition namespace to be extended.
definition_class is the template definition class to be reactivated.
reference_nsp is the referencing namespace to be reactivated.
p_common_depth is set to the scope depth of the first scope that is common
to both the defining and referencing contexts.  p_definition_depth is
set to the innermost scope that is part of the template definition
context.  p_context_depth is set to the scope depth of the referencing
namespace.  When referencing_nsp is NULL, this is the current scope depth,
otherwise it is the depth of the scope pushed for referencing_nsp.
p_after_definition_depth is the scope depth of the scope whose previous
scope needs to be updated to point to definition_depth when the scope
stack is fixed up later.  instance_sym is the symbol to be used for
the outermost class instantiation scope and is non-NULL for nontemplate
member instantiations when the outermost instantiation scope is actually
the instantiation scope for the member, not the class that is being
reactivated.  Likewise, assoc_type and assoc_routine are non-NULL when
they should be used for the outermost instantiation scope.  "options" is
the set of option flags passed into the push scope routines.  context_scope
is used for generic lambdas and is the scope containing the lambda.
*/
{
  a_scope_depth		common_depth;
  a_scope_depth		context_depth;
  a_scope_depth		definition_depth;
  a_namespace_ptr	common_nsp;
  a_scope_depth		depth_of_first_context_scope = NO_SCOPE_DEPTH;

  /* Push a scope that marks the start of the instantiation context on the
     scope stack. */
  (void)push_scope_full((a_scope_kind)sck_instantiation_context,
                        NO_SCOPE_NUMBER, NULL, NULL, (a_namespace_ptr)NULL,
                        (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                        (a_template_arg_ptr)NULL,
                        (a_template_decl_info_ptr)NULL,
                        (an_object_lifetime_ptr)NULL,
                        (a_scope_ptr)NULL, (a_scope_pointers_block_ptr)NULL,
                        options);
  /* Push the namespace containing the point of instantiation.  This is not
     needed for classes being loaded by get_definition_of_class. */
  if ((options & PS_CLASS_DEFINITION_CONTEXT) == 0 &&
      (reference_nsp !=
              scope_stack[depth_innermost_namespace_scope].assoc_namespace ||
       depth_innermost_instantiation_scope != NO_SCOPE_DEPTH)) {
    /* The namespace that contains the first reference of this template
       that requires its instantiation is different than the current
       namespace.  If we are already in an instantiation we force the
       referencing namespace to be repushed because the earlier context
       may include instantiation scopes that should not be considered.
       Reactivate the namespace associated with that reference.  We must
       force a new entry to be pushed here to make sure that the depth of
       the first context scope is accurate. */
    if (reference_nsp != NULL) {
      depth_of_first_context_scope = depth_scope_stack + 1;
      f_push_namespace_extension_scope(reference_nsp,
                                       /*force_new_entry=*/TRUE);
      check_assertion(depth_scope_stack >= depth_of_first_context_scope);
    }  /* if */
  }  /* if */
  /* The context scope is the innermost namespace scope at this point,
     except when the referencing namespace is the file scope (because
     the file scope cannot be reactivated). */
  if (reference_nsp != NULL) {
    context_depth = depth_innermost_namespace_scope;
  } else {
    context_depth = DEPTH_OF_FILE_SCOPE;
  }  /* if */
  /* Push the namespace(s) containing the definition of the template. */
  if (definition_nsp != NULL) {
    /* The common depth is the innermost scope that is both part of the
       context and part of the definition scope.  The normal processing
       for this assumes that the referencing context was pushed above,
       which is not the case for the file scope.  When the referencing
       context is the file scope, the common scope is also the file scope.
       Don't try to use common scopes that may have been pushed by a
       previous instantiation scope. */
    if (reference_nsp == NULL) {
      common_depth = DEPTH_OF_FILE_SCOPE;
    } else if (scope_stack[depth_innermost_namespace_scope].assoc_namespace
                                                          == definition_nsp) {
      common_depth = depth_innermost_namespace_scope;
    } else {
      common_depth = find_depth_of_common_scope(definition_nsp);
    }  /* if */
    common_nsp = scope_stack[common_depth].assoc_namespace;
    if (common_nsp == definition_nsp) {
      definition_depth = common_depth;
    } else {
      /* Reactivate the scope from the common namespace scope through the
         parent namespace of the template. */
      push_namespace_extension_for_instantiation(definition_nsp, common_nsp,
                                                 common_depth);
      definition_depth = depth_scope_stack;
    }  /* if */
  } else {
    common_depth = DEPTH_OF_FILE_SCOPE;
    common_nsp = NULL;
    definition_depth = DEPTH_OF_FILE_SCOPE;
  }  /* if */
  /* Set the decl_scope_level to namespace of the template.  In most cases
     this will be further modified by the entity being instantiated, but
     in some cases (e.g., static data member instantiations) there
     is no other scope pushed. */
  decl_scope_level = definition_depth;
  /* If we pushed some context scopes, reset the previous scope of the first
     context scope so that its previous scope is the file scope.
     Strictly speaking, this shouldn't be necessary, but is done for safety.
     When a context scope is pushed, the instantiation_context_lookup is used
     to correctly inspect the definition, context, and common scopes.  In
     other words, when the previous scope is not already the file scope,
     the previous scope pointer of this scope shouldn't be used. */
  if (depth_of_first_context_scope != NO_SCOPE_DEPTH) {
    scope_stack[depth_of_first_context_scope].previous_scope =
                                                           DEPTH_OF_FILE_SCOPE;
  }  /* if */
  /* The next scope pushed needs to have its previous scope set to the
     definition depth.  Save the depth of the scope that will need to
     be fixed up later. */
  *p_after_definition_depth = depth_scope_stack + 1;
  reactivate_instantiation_context(enclosing_decl_info, context_scope,
                                   definition_class,
                                   instance_sym, assoc_type,
                                   assoc_routine, options);
  /* Return the calculated scope depths to the caller. */
  *p_common_depth = common_depth;
  *p_definition_depth = definition_depth;
  *p_context_depth = context_depth;
}  /* push_instantiation_context */


void inject_tokens_in_namespace(a_token_cache  *tokens,
                                a_scope        *namespace_scope)
/*
Scan the given tokens as a declaration appearing in the given namespace scope
(which might be the file scope).  Note that tokens must be a persistent
reusable token cache.
*/
{
  a_scope_depth       orig_depth = depth_scope_stack;
  an_object_lifetime  *saved_curr_olp = curr_object_lifetime;
  a_struct_stmt_stack_state
                      saved_sss_state;

  push_new_top_level_declaration();
  if (scope_is(namespace_scope, sck_namespace)) {
    push_namespace_extension_scope(namespace_scope->variant.assoc_namespace);
    decl_scope_level = depth_innermost_namespace_scope;
  } else {
    push_file_scope(/*is_reactivation=*/TRUE);
  }  /* if */
  curr_object_lifetime =
                  scope_stack[DEPTH_OF_FILE_SCOPE].curr_scope_object_lifetime;
  new_struct_stmt_stack(&saved_sss_state);
  rescan_persistent_reusable_cache(tokens);
  declaration(/*function_definition_allowed=*/TRUE,
              /*is_old_style_param_decl=*/FALSE,
              /*is_top_level_declaration=*/TRUE,
              /*marked_as_gnu_extension=*/FALSE,
              (a_param_id*)NULL,
              (a_source_range*)NULL);
  if (curr_token != tok_end_of_source) {
    pos_error(ec_extraneous_injected_declaration_tokens, &pos_curr_token);
  }  /* if */
  flush_past_token_cache_terminator();
  restore_struct_stmt_stack(&saved_sss_state);
  while (depth_scope_stack > orig_depth) pop_scope();
  curr_object_lifetime = saved_curr_olp;
}  /* inject_tokens_in_namespace */


static void fixup_instantiation_scopes(
			a_template_decl_info_ptr	decl_info,
			a_scope_depth			orig_depth,
			a_scope_depth			common_depth,
			a_scope_depth			definition_depth,
			a_scope_depth			context_depth,
			a_scope_depth			after_definition_depth,
			a_scope_ptr			lambda_scope,
			a_push_scope_options_set	options)
/*
Fix up the entries on the scope stack.  At this point the scope stack
looks like:

	7 instantiation scope for this entity
	6 reactivation for class containing template (optional)
	5 instantiation for class containing template (optional)
	  (any number of classes may be reactivated)
	4 namespace containing template (optional)
	  (any number of namespaces may be pushed)
	3 namespace containing point of instantiation (optional)
	2 scope containing reference that caused the instantiation
	1 some scope
	0 file scope

The following fixups need to be performed:

- all instantiation scopes pushed, except the first one, must be
  flagged as nested instantiations
- the context and common scopes must be recorded in the nonnested
  instantiation scope
- the previous_scope of the outermost definition context scope and the
  referencing context scope must be set to point to the common scope

If the template is a generic lambda, lambda_scope is the scope containing
the lambda.  NULL otherwise.
*/
{
  a_scope_depth			primary_instantiation_depth = NO_SCOPE_DEPTH;
  a_scope_depth			depth;
  a_boolean			exclude_from_context_output = FALSE;
  a_boolean			decl_scope_reached = FALSE;

  /* Mark all instantiation scopes that have been pushed as nested
     instantiations.  Save the depth of the outermost instantiation
     scope that was pushed.  It will be changed back to nonnested later.
     When this loop completes, primary_instantiation_depth will
     mark the first instantiation scope pushed.  All instantiation
     scopes pushed as part of this set, except the innermost one,
     should have the "exclude from context output" flag set so that
     only the context information associated with the innermost scope
     will be included in error output. */
  for (depth = depth_scope_stack; depth > orig_depth; depth--) {
    a_scope_stack_entry_ptr	ssep = scope_stack_entry_for(depth);
    if (ssep->kind == (a_scope_kind)sck_template_instantiation) {
      primary_instantiation_depth = depth;
      ssep->nested_instantiation = TRUE;
      ssep->exclude_from_context_output = ssep->template_sym == NULL ||
                                          exclude_from_context_output;
      exclude_from_context_output = TRUE;
    }  /* if */
    if (is_scope_kind_that_affects_access_control(ssep->kind, options)) {
      /* If the next_scope_that_affects_access_control field points to
         a scope that is not part of the instantiation context, set it
         to NO_SCOPE_DEPTH because it should not be considered for purposes
         of this instantiation. */
      if (ssep->next_scope_that_affects_access_control <= orig_depth) {
        ssep->next_scope_that_affects_access_control = NO_SCOPE_DEPTH;
      }  /* if */
    }  /* if */
    if ((options & PS_IGNORE_CLASS_CONTEXT) != 0 &&
         !decl_scope_reached) {
      /* During normal lookups, ignore certain scopes.  This is used
         during the instantiation of static data members.  They are unusual in
         that the declaration that is rescanned is the one that appeared
         outside of the class, so declarations from the class context should
         not be visible until the declarator is reached. */
      if (decl_info->enclosing_scope->number == ssep->number) {
        decl_scope_reached = TRUE;
      } else {
        if (ssep->kind != (a_scope_kind)sck_template_instantiation) {
          ssep->ignore_during_normal_lookup = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  if (primary_instantiation_depth != NO_SCOPE_DEPTH) {
    a_scope_stack_entry_ptr	primary_ssep;
    primary_ssep = &scope_stack[primary_instantiation_depth];
    if (lambda_scope == NULL ||
        lambda_scope->depth_in_scope_stack == NO_SCOPE_DEPTH) {
      /* Mark the initial instantiation scope as nonnested.  This is not done
         for generic lambdas where the enclosing scope is on the scope
         stack. */
      primary_ssep->nested_instantiation = FALSE;
      primary_ssep->instantiation_context_depth = context_depth;
      primary_ssep->instantiation_common_depth = common_depth;
    }  /* if */
  }  /* if */
  /* The previous_scope of the initial definition context scope will have
     already been set properly. */
  /* The scope pushed after the definition namespace context has been
     restored, if any, needs to have its previous scope field updated to
     point to the definition context. */
  if (after_definition_depth > depth_scope_stack) {
    /* When this routine is called (indirectly) by get_definition_of_class
       there is not always a scope pushed after the definition namespace
       context has been set up (when called from other contexts there is
       at least a template instantiation scope).  Adjust the after
       definition depth in such cases. */
    after_definition_depth = depth_scope_stack;
    check_assertion_or_expect_error(
                                (options & PS_CLASS_DEFINITION_CONTEXT) != 0);
  }  /* if */
  /* after_definition_depth will only be the same as definition_depth if
     it was adjusted in the code above. */
  if (after_definition_depth != definition_depth) {
    scope_stack[after_definition_depth].previous_scope = definition_depth;
  }  /* if */
}  /* fixup_instantiation_scopes */


/*
Instantiating a template or rescanning its template parameters (or default
template arguments) creates a new context and if we were previously inside a
local class that is now probably no longer the case.  This macro updates the
global inside_local_class flag when instantiating/rescanning a specific
template.
*/
#define set_inside_local_class_flag_for_template(templ_sym)                  \
  { inside_local_class =                                                     \
          templ_sym != NULL && templ_sym->is_class_member &&                 \
          sym_parent_class(templ_sym)->source_corresp.is_local_to_function;  \
  }


static void push_simple_instantiation_scope(
                            a_template_decl_info_ptr	decl_info,
                            a_type_ptr			assoc_type,
                            a_routine_ptr		assoc_routine,
                            a_symbol_ptr		instance_sym,
                            a_symbol_ptr		template_sym,
                            a_template_arg_ptr		template_arg_list,
                            a_push_scope_options_set	options)
/*
Push a template instantiation scope, but not all of the surrounding context
scopes.
*/
{
  a_scope_stack_entry_ptr	ssep;
  a_scope_depth			saved_innermost_scope_that_affects_access;

  set_inside_local_class_flag_for_template(template_sym);
  saved_innermost_scope_that_affects_access =
                         depth_of_innermost_scope_that_affects_access_control;
  (void)push_scope_full((a_scope_kind)sck_template_instantiation,
                        decl_info->declaration_scope, assoc_type,
                        assoc_routine, (a_namespace_ptr)NULL, instance_sym,
                        template_sym, template_arg_list, decl_info,
                        (an_object_lifetime_ptr)NULL,
                        (a_scope_ptr)NULL, (a_scope_pointers_block_ptr)NULL,
                        options);
  ssep = scope_stack_entry_for(depth_scope_stack);
  /* Template instantiation scopes need to record the scope depth before
     the set of scopes that represent the instantiation is pushed.
     The previous saved innermost access scope must also be recorded. */
  ssep->orig_depth = depth_scope_stack-1;
  ssep->saved_innermost_scope_that_affects_access =
                                     saved_innermost_scope_that_affects_access;
  /* Set the instantiation context depth to the enclosing scope.  Other parts
     of the front end require this to be set. */
  ssep->instantiation_context_depth = depth_scope_stack-1;
}  /* push_simple_instantiation_scope */


static a_boolean is_nested_in_prototype_instantiation(
                            a_symbol_ptr		template_sym)
/*
See if this is an instantiation scope nested within a prototype
instantiation.  This could be the prototype instantiation of a member
template that is defined inside of the enclosing template.  It could
also be a partial instantiation of a member function template in a
prototype instantiation (this can occur when a Microsoft mode in-class
specialization of a member class template is declared).  In such cases
the prototype instantiation of the enclosing template is still in
progress.  When processing an instantiation nested within a prototype
instantiation we simply push a new template instantiation scope onto
the existing context and flag it as a nested instantiation.

The caller is responsible for checking that the current scope has
its "in_prototype_intantiation" flag set.

template_sym is the template that is being instantiated.
*/
{
  a_scope_stack_entry_ptr	ssep;
  a_scope_stack_entry_ptr	instantiation_ssep;
  a_type_ptr			assoc_type = NULL;
  a_boolean			result = FALSE;

  /* Find the innermost instantiation scope, but stop searching
     if we're inside a function. */
  instantiation_ssep = &scope_stack[depth_innermost_instantiation_scope];
  /*lint --e{446,850} ssep modified in loop (LINTBUG) */
  for (ssep = &scope_stack[depth_scope_stack];
       ssep != NULL && ssep != instantiation_ssep;
       ssep = previous_scope_of(ssep)) {
    if (ssep->kind == (a_scope_kind)sck_function) {
      ssep = NULL;
      break;
    } else if (assoc_type == NULL) {
      /* Save the type of the first class scope that we encounter. */
      if (ssep->kind == (a_scope_kind)sck_class_struct_union ||
          ssep->kind == (a_scope_kind)sck_class_reactivation) {
        assoc_type = ssep->assoc_type;
      }  /* if */
    }  /* if */
  }  /* for */
  if (assoc_type != NULL) {
    /* If the class being instantiated is the same as the parent of the
       template to be instantiated, this is nested in the enclosing
       prototype instantiation. */
    if (template_sym->is_class_member &&
        same_entities(sym_parent_class(template_sym), assoc_type)) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* is_nested_in_prototype_instantiation */


void push_instantiation_scope_for_templ_param_rescan(
                            a_template_decl_info_ptr	decl_info,
                            a_type_ptr			assoc_type,
                            a_routine_ptr		assoc_routine,
                            a_symbol_ptr		instance_sym,
                            a_symbol_ptr		template_sym,
                            a_template_arg_ptr		template_arg_list,
			    a_push_scope_options_set	ps_options)
/*
Push an instantiation scope for rescanning a portion of a template
parameter list.  For most templates, a normal instantiation scope is
pushed.  But for a template template parameter, the previous lookup
scope of the instantiation scope is set to the scope in which the
template template parameter was declared.  This will be either a
template declaration scope or a template instantiation scope.
*/
{
  a_template_symbol_supplement_ptr tssp;

  tssp = template_sym->variant.template_info;
  if (template_sym->kind == (a_symbol_kind)sk_class_template &&
      tssp->variant.class_template.template_template_param) {
    a_boolean			is_local_to_function = FALSE;
    a_scope_depth		depth;
    depth = scope_depth_of_symbol(template_sym, &is_local_to_function);
    check_assertion(depth != NO_SCOPE_DEPTH);
    push_simple_instantiation_scope(decl_info,
				    assoc_type,
				    assoc_routine,
				    instance_sym,
				    template_sym,
				    template_arg_list,
				    ps_options);
    scope_stack[depth_scope_stack].previous_scope = depth;
  } else {
    (void)push_template_instantiation_scope(decl_info,
				            assoc_type,
				            assoc_routine,
				            instance_sym,
				            template_sym,
				            template_arg_list,
				            /*push_lex_state=*/TRUE,
				            ps_options);
  }  /* if */
}  /* push_instantiation_scope_for_templ_param_rescan */


a_boolean push_template_instantiation_scope(
                            a_template_decl_info_ptr	decl_info,
                            a_type_ptr			assoc_type,
                            a_routine_ptr		assoc_routine,
                            a_symbol_ptr		instance_sym,
                            a_symbol_ptr		template_sym,
                            a_template_arg_ptr		template_arg_list,
			    a_boolean			push_lex_state,
			    a_push_scope_options_set	options)
/*
Interface to push_scope_full that is used for template instantiation
scopes.  If push_lex_state is TRUE, a new lexical state stack entry
is pushed here, and popped when the instantiation scope is popped.
In some cases involving prototype instantiations, no scope is actually
pushed.  Return TRUE if a scope is pushed, FALSE otherwise.

When GET_DEFINITION_OF_CLASS_NEEDED is TRUE, this routine is also called
to reestablish the context for a normal (non-template) class to be defined.
In such cases, template_sym and instance_sym point to the symbol of the
class to be defined.
*/
{
  a_namespace_ptr		parent_nsp = NULL;
  a_type_ptr			parent_class;
  a_scope_depth			common_depth = NO_SCOPE_DEPTH;
  a_scope_depth			new_innermost_namespace_scope = NO_SCOPE_DEPTH;
  a_scope_depth			context_depth = NO_SCOPE_DEPTH;
  a_scope_depth			definition_depth = NO_SCOPE_DEPTH;
  a_scope_depth			after_definition_depth = NO_SCOPE_DEPTH;
  a_scope_depth			orig_depth = depth_scope_stack;
  a_scope_depth			saved_innermost_scope_that_affects_access;
  a_namespace_ptr		reference_nsp;
  a_boolean			is_template = FALSE;
  a_template_decl_info_ptr	enclosing_tdip;
  a_symbol_ptr			enclosing_instance_sym;
  a_type_ptr			enclosing_assoc_type;
  a_routine_ptr			enclosing_assoc_routine, inh_ctor_orig = NULL;
  a_boolean			use_existing_context = FALSE;
  a_boolean			is_lambda_body = FALSE;
  a_boolean			is_real_lambda_instantiation = FALSE;
  a_boolean			is_defined_in_friend_decl = FALSE;
  a_scope_ptr			lambda_scope = NULL;
  a_scope_ptr			context_scope = NULL;
  a_type_ptr			lambda_class = NULL;

  /* Clear the flag that indicates that we are in a local class so that any
     scopes pushed by this routine will not be indicated as being within a
     local class, unless we're instantiating a member template of a local
     class.  This will be restored to the correct state when the last scope
     pushed by this routine is popped.  The same is done for the flag that
     indicates whether we are within a function scope. */
  set_inside_local_class_flag_for_template(template_sym);
  depth_innermost_function_scope = NO_SCOPE_DEPTH;
  innermost_function_scope = NULL;
  saved_innermost_scope_that_affects_access =
                         depth_of_innermost_scope_that_affects_access_control;
  /* Determine whether the bottom-level entity is a template.  In some
     cases the only instantiation scopes that are pushed are ones
     that make up the context for the definition of an entity.  This
     is the case for any nontemplate entity (member function, class, etc.)
     defined within a template.  If this routine will push an instantiation
     scope, get a pointer to the enclosing template declaration information
     to be passed to the routine that pushes the context scopes. */
  is_template = template_sym == NULL || is_template_symbol(template_sym);
  if (is_template) {
    /* Get a pointer to the enclosing template declaration information.  If
       this pointer is NULL, the template declaration information from the
       template symbol supplement will be used instead. */
    enclosing_tdip = decl_info->enclosing_template_decl;
    enclosing_instance_sym = NULL;
    enclosing_assoc_type = NULL;
    enclosing_assoc_routine = NULL;
  } else {
    /* This entity is not a template, use the template declaration information
       that was passed in. */
    enclosing_tdip = decl_info;
    /* For nontemplate members of template classes use the symbol for the
       nontemplate member that is being instantiated as the instance
       symbol for the outermost instantiation scope. */
    enclosing_instance_sym = instance_sym;
    enclosing_assoc_type = assoc_type;
    enclosing_assoc_routine = assoc_routine;
  }  /* if */
  if (template_sym != NULL && symbol_is(template_sym, sk_function_template)) {
    /* Determine if this is an instantiation of a generic lambda, an inheriting
       constructor, or a friend function definition. */
    a_template_symbol_supplement_ptr	tssp;
    a_routine_ptr			rout;
    tssp = template_sym->variant.template_info;
    rout = tssp->variant.function.routine;
    is_lambda_body = rout->is_lambda_body;
    is_defined_in_friend_decl = rout->defined_in_friend_decl;
    if (rout->is_inheriting_ctor) {
      inh_ctor_orig = get_inh_ctor_originator(rout);
    }  /* if */
  }  /* if */
  if (is_lambda_body) {
    /* For a generic lambda, get the scope in which the lambda was declared.
       If that scope is still on the stack, we can use that for the
       instantiation context. */
    lambda_class = sym_parent_class(template_sym);
    lambda_scope = get_parent_scope_of(lambda_class);
    context_scope = lambda_scope;
  } else if (inh_ctor_orig != NULL) {
    a_symbol_ptr                     base_template_sym;
    a_template_symbol_supplement_ptr base_tssp;
    base_template_sym = symbol_for(inh_ctor_orig)->
                                    variant.routine.instance_ptr->template_sym;
    base_tssp = template_supplement_for_symbol(base_template_sym);
    context_scope = base_tssp->variant.function.decl_cache->decl_info->
                                                               enclosing_scope;
  } else if (is_defined_in_friend_decl) {
    a_template_symbol_supplement_ptr tssp;
    tssp = template_supplement_for_symbol(template_sym);
    context_scope = cache_for_template(tssp)->decl_info->enclosing_scope;
  } else {
    context_scope = decl_info->enclosing_scope;
  }  /* if */
  /* Determine whether this instantiation is a prototype instantiation of
     something within another prototype instantiation.  This affects the
     way that the scope stack is manipulated.  A prototype instantiation
     is not allowed in a generic definition, but that case is included here
     for error recovery purposes. */
  if (template_sym != NULL &&
      ((options & PS_PROTOTYPE_INSTANTIATION) != 0 ||
       (options & PS_GENERIC_DEFINITION) != 0 ||
       ((options & PS_NONREAL_INSTANTIATION) != 0 &&
        (options & PS_IS_RESCAN) == 0))) {
    if (scope_stack[depth_scope_stack].in_prototype_instantiation) {
      use_existing_context =
                            is_nested_in_prototype_instantiation(template_sym);
    } else if (is_lambda_body) {
      /* Treat a generic lambda prototype instantiation as nested in a
         prototype instantiation because we want to use the existing
         surrounding context. */
      use_existing_context = TRUE;
    }  /* if */
  } else if (is_lambda_body) {
    /* A real instantiation of a generic lambda.  If we are still in the
       function in which the lambda was defined, use the existing context. */
    if (lambda_scope->depth_in_scope_stack == orig_depth &&
        orig_depth != DEPTH_OF_FILE_SCOPE) {
      use_existing_context = TRUE;
    }  /* if */
    is_real_lambda_instantiation = TRUE;
    options |= PS_IS_GENERIC_LAMBDA;
  }  /* if */
  if (!use_existing_context) {
    a_template_decl_info_ptr  enclosing_variable_tdip = NULL;
    if (decl_info->variable_instance_sym != NULL) {
      /* If the context includes a variable template instantiation, the
         enclosing template declaration information is that of the variable
         template instantiation. */
      check_assertion(context_scope->kind != sck_block &&
                      context_scope->kind != sck_condition &&
                      context_scope->kind != sck_function);
      enclosing_variable_tdip = enclosing_tdip;
      enclosing_tdip = NULL;
    }  /* if */
    /* If the template was defined in a namespace, reactivate the namespace
       scope before pushing the instantiation scope. */
    /*coverity[var_deref_model]*/
    get_parent_information_for_template(context_scope, is_lambda_body,
                                        template_sym, instance_sym,
                                        &parent_nsp, &parent_class);
    if (parent_class != NULL) {
      check_assertion(!parent_class->
                       variant.class_struct_union.is_prototype_instantiation ||
                       (options & PS_PROTOTYPE_INSTANTIATION) != 0 ||
                       (options & PS_NONREAL_INSTANTIATION) != 0);
    }  /* if */
    reference_nsp = referencing_namespace_for_instance(instance_sym,
                                                       is_lambda_body);
    push_instantiation_context(enclosing_tdip,
                               parent_nsp, parent_class,
                               reference_nsp, &common_depth, &definition_depth,
                               &context_depth, &after_definition_depth,
                               context_scope,
                               enclosing_instance_sym, enclosing_assoc_type,
                               enclosing_assoc_routine, options);
    /* At this point, definition_depth points to the parent scope
       of the template being instantiated.  Save this value before it
       is potentially modified below. */
    new_innermost_namespace_scope = definition_depth;
    if (decl_info->variable_instance_sym != NULL) {
      a_template_symbol_supplement_ptr  tssp =
                                  template_supplement_for_symbol(template_sym);
      /* The context includes a variable template instantiation.  Reactivate
         the instantiation scope for the variable template. */
      reactivate_variable_context(
                                 enclosing_variable_tdip,
                                 tssp->cache->decl_info->variable_instance_sym,
                                 options);
    }  /* if */
  }  /* if */
  if (is_template) {
    a_scope_stack_entry_ptr	ssep = &scope_stack_top();
    if (is_real_lambda_instantiation &&
        (!scope_is(ssep, sck_class_reactivation) ||
         ssep->assoc_type != lambda_class)) {
      /* For a generic lambda, push the closure class.  Don't push it if it
         is already the current scope, which can happen sometimes as part of
         the pushing of the instantiation context above. */
      push_class_reactivation_scope(lambda_class,
                                    /*extend_namespace=*/FALSE);
    }  /* if */
    if (is_lambda_body && assoc_routine == NULL) {
      /* Real generic-lambda instantiations are pushed with a NULL
         assoc_routine; record the call operator for decl_seq and
         enclosing-routine queries. */
      a_template_symbol_supplement_ptr  tssp =
                                  template_supplement_for_symbol(template_sym);
      assoc_routine = tssp->variant.function.routine;
    }  /* if */
    (void)push_scope_full((a_scope_kind)sck_template_instantiation,
                          decl_info->declaration_scope, assoc_type,
                          assoc_routine, (a_namespace_ptr)NULL, instance_sym,
                          template_sym, template_arg_list, decl_info,
                          (an_object_lifetime_ptr)NULL,
                          (a_scope_ptr)NULL, (a_scope_pointers_block_ptr)NULL,
                          options);
    if (is_lambda_body) {
      /* Limit the lookup to symbols visible at the point of declaration of
         the lambda. */
      a_template_symbol_supplement_ptr  tssp =
                                  template_supplement_for_symbol(template_sym);
      scope_stack[depth_scope_stack].decl_seq_for_lookup =
                                              tssp->cache->decl_info->decl_seq;
    }  /* if */
  }  /* if */
  if (!use_existing_context) {
    a_scope_stack_entry_ptr	ssep;
    /* Update the scope stack entries that have been pushed so that the
       special instantiation context lookups can be done correctly. */
    fixup_instantiation_scopes(decl_info, orig_depth, common_depth,
                               definition_depth, context_depth,
                               after_definition_depth, lambda_scope, options);
    /* Update the depth of the innermost instantiation scope so that it points
       to the namespace that is the parent of the template being
       instantiated. */
    ssep = &scope_stack[depth_scope_stack];
    depth_innermost_namespace_scope =
         ssep->depth_innermost_namespace_scope = new_innermost_namespace_scope;
    /* Because a template instantiation introduces a new context for
       name lookup purposes, we need to clear the active using list
       flags for any namespaces for which it is currently set. */
    set_active_using_list_scope_depths(depth_scope_stack,
                                       /*set_value=*/FALSE,
                                       NO_DECL_SEQUENCE_NUMBER);
    { a_decl_sequence_number	decl_seq;
      /* Set the active using flags for the newly created context. */
      decl_seq = decl_info->decl_seq;
      if (!do_dependent_name_processing && !gpp_using_directive_lookup &&
          (assoc_routine != NULL || assoc_type != NULL)) {
        /* When not doing dependent name lookup, all using-directives are
           considered (not just the ones that should be visible), except
           during the partial instantiation of a function (when assoc_routine
           and assoc_type will be NULL). */
        decl_seq = NO_DECL_SEQUENCE_NUMBER;
      }  /* if */
      set_active_using_list_scope_depths(depth_scope_stack, /*set_value=*/TRUE,
                                         decl_seq);
    }
    check_assertion(scope_stack[new_innermost_namespace_scope].assoc_namespace
                                                                == parent_nsp);
  } else {
    /* A nested prototype instantiation must be flagged as a nested
       instantiation. */
    scope_stack[depth_scope_stack].nested_instantiation = TRUE;
  }  /* if */
  { a_scope_stack_entry_ptr ssep = &scope_stack[depth_scope_stack];
    /* Except in certain prototype instantiation cases, one or more scopes
       should always have been pushed by this process. */
    check_assertion(orig_depth != depth_scope_stack ||
                    (options & PS_PROTOTYPE_INSTANTIATION) != 0);
    if (orig_depth != depth_scope_stack) {
      /* Save the original scope depth in the last scope pushed by this
         routine.  This will be used later when popping the stack. */
      ssep->orig_depth = orig_depth;
      /* Save the original value of the depth of the innermost scope that
         affects access control.  This is necessary because the scope fixup
         routine may adjust some of the next scope that affects access
         control links in the scope stack resulting in an incorrect value
         for the global variable after the instantiation scopes have been
          popped. */
      ssep->saved_innermost_scope_that_affects_access =
                                    saved_innermost_scope_that_affects_access;
      if (push_lex_state) {
        /* Start a new lexical context for the instantiation. */
        push_lexical_state_stack();
        ssep->lexical_state_stack_pushed = TRUE;
      }  /* if */
    } else {
      /* No scopes were pushed.  Keep a count of the number of empty
         contexts in which the pop scope should not actually pop
         any entries. */
      ssep->empty_contexts_pushed++;
    }  /* if */
  }
#if DEBUG
  if (debug_level >= 4 || db_flag_is_set("instantiation_scope")) {
    fprintf(f_debug, "Pushed instantiation scope for: ");
    db_symbol(instance_sym, "", 0);
    if (!use_existing_context) {
      fprintf(f_debug, "context_depth=%d, common_depth=%d\n", context_depth,
              common_depth);
    }  /* if */
    fprintf(f_debug, "scope stack after instantiation scope:\n");
    db_scope_stack();
  }  /* if */
#endif /* DEBUG */

  /* Return TRUE if a scope was pushed. */
  a_boolean scope_pushed = depth_scope_stack != orig_depth;
  if (scope_pushed) {
    a_scope_stack_entry_ptr ssep = &scope_stack_top();

    /* If owns_module_push is already TRUE, the pushed scope cannot take
       ownership of the module entity state.  If this occurs, the code will
       need to be adjusted to either pop the previously-owned module entity
       state or to ensure this condition is not reached. */
    check_assertion(!ssep->owns_module_push);
    /* In most cases, set the module ownership of any symbols that follow from
       the template to its associated module entity (if any).  In some cases
       (e.g., push_instantiation_scope_for_boxed_enum_type) the template symbol
       is NULL and it's assumed the appropriate module is the global module. */
    if (template_sym != NULL) {
      push_module_entity_state(template_sym->module_entity);
    } else {
      push_module_entity_state(NULL);
    } /* if */
    /* Set the pushed scope to own the pushed module entity state. */
    ssep->owns_module_push = TRUE;
  }  /* if */
  return scope_pushed;
}  /* push_template_instantiation_scope */


void pop_template_instantiation_scope(void)
/*
Interface to pop_scope that is used for template instantiation scopes.
Pops all of the scopes that were pushed by a given call to
push_template_instantiation_scope.
*/
{
  a_scope_depth			orig_depth;
  a_scope_depth			saved_innermost_scope_that_affects_access;
  a_scope_stack_entry_ptr	ssep =  &scope_stack_top();

  if (ssep->empty_contexts_pushed != 0) {
    /* If there were any empty contexts pushed, simply decrement the count. */
    ssep->empty_contexts_pushed--;
  } else {
    orig_depth = scope_stack[depth_scope_stack].orig_depth;
    saved_innermost_scope_that_affects_access =
      scope_stack[depth_scope_stack].saved_innermost_scope_that_affects_access;
    check_assertion(saved_innermost_scope_that_affects_access <= orig_depth);
    check_assertion_str2(orig_depth != NO_SCOPE_DEPTH,
                         "pop_template_instantiation_scope:",
                         "invalid orig_depth");
    if (scope_stack[depth_scope_stack].lexical_state_stack_pushed) {
      /* Restore the original lexical state context. */
      pop_lexical_state_stack();
    }  /* if */
    /* Pop scopes until the depth of the scope stack is equal to orig_depth,
       which is the depth before any of the instantiation context scopes were
       pushed. */
    while (orig_depth < depth_scope_stack) pop_scope();
    /* Restore the original value of the depth of the innermost scope that
       affects access control.  This is necessary because the scope fixup
       routine used when an instantiation scope is pushed may adjust some
       of the next scope that affects access control links in the scope stack
       resulting in an incorrect value for the global variable after the
       instantiation scopes have been popped. */
    depth_of_innermost_scope_that_affects_access_control =
                                    saved_innermost_scope_that_affects_access;
    /* Reset the active using list flags to the values specified by
       the previous scope stack entries. */
    set_active_using_list_scope_depths(depth_scope_stack,
                                       /*set_value=*/TRUE,
                                       get_effective_decl_seq());
  }  /* if */
}  /* pop_template_instantiation_scope */


void push_instantiation_scope_for_rescan(a_symbol_ptr	template_sym)
/*
Push an instantiation scope for expression rescan.  template_sym is
the template that is being rescanned and can be NULL.
*/
{
  a_template_decl_info_ptr	tdip = NULL;
  a_routine_ptr			rp = NULL;
  a_scope_stack_entry_ptr	ssep;
  a_scope_depth			orig_access_depth;

  orig_access_depth = depth_of_innermost_scope_that_affects_access_control;
  if (cpp11_sfinae_enabled && template_sym != NULL) {
    a_template_symbol_supplement_ptr	tssp;
    tssp = template_supplement_for_symbol(template_sym);
    check_assertion(tssp != NULL);
    /* Get the template decl. info. to get the context to be used for
       access checking.  Use the decl_cache, if present, for functions. */
    if (template_sym->kind == (a_symbol_kind)sk_function_template ||
        template_sym->kind == (a_symbol_kind)sk_member_function) {
      rp = tssp->variant.function.routine;
      tdip = tssp->variant.function.decl_cache->decl_info;
    }  /* if */
    if (tdip == NULL) {
      tdip = tssp->cache->decl_info;
    }  /* if */
  } else {
    if (template_sym != NULL &&
        template_sym->kind != (a_symbol_kind)sk_class_template &&
        template_sym->kind != (a_symbol_kind)sk_function_template) {
      /* Don't include the template information when pushing the context
         for an entity that is not itself a template as we are not
         providing actual template_decl_info. */
      template_sym = NULL;
    }  /* if */
    tdip = alloc_template_decl_info();
  }  /* if */
  (void)push_template_instantiation_scope(
                              tdip, (a_type_ptr)NULL, rp,
                              (a_symbol_ptr)NULL, template_sym,
                              (a_template_arg_ptr)NULL,
                              /*push_lex_state=*/FALSE,
                              PS_NONREAL_INSTANTIATION | PS_IS_RESCAN);
  /* Don't include this scope in any diagnostic output that may be produced. */
  ssep = &scope_stack_top();
  ssep->exclude_from_context_output = TRUE;
  if (cpp11_sfinae_enabled && !cpp11_sfinae_ignore_access &&
      template_sym != NULL) {
    /* A function access scope is pushed even for the class case. */
    (void)push_scope((a_scope_kind)sck_function_access, NO_SCOPE_NUMBER,
                     (a_type_ptr)NULL, rp);
    scope_stack_top().template_sym = template_sym;
    if (template_sym != NULL && template_sym->from_module_code) {
      scope_stack_top().module_load_context_count += 1;
    }  /* if */
  }  /* if */
  /* Save the innermost scope that affects access control and defer
     access checks during the rescan.  This is needed even when
     SFINAE access checking is not being done because some access
     checks that are outside of the SFINAE process get re-done and
     must succeed. */
  if (orig_access_depth != NO_SCOPE_DEPTH) {
    ssep = &scope_stack_top();
    ssep->orig_access_depth = orig_access_depth;
    begin_deferral_of_access_checks();
  }  /* if */
}  /* push_instantiation_scope_for_rescan */


void pop_instantiation_scope_for_rescan(void)
/*
Pop the template instantiation scope pushed by
push_instantiation_scope_for_rescan.
*/
{
  a_template_decl_info_ptr	tdip = NULL;
  a_scope_depth			orig_access_depth;
  a_scope_stack_entry_ptr	ssep;

  ssep = &scope_stack_top();
  orig_access_depth = ssep->orig_access_depth;
  if (orig_access_depth != NO_SCOPE_DEPTH) {
    /* If an original access checking depth was saved, repeat any
       access checks that were deferred.  These are done both in the
       current context (when doing SFINAE access checking) and in the 
       original access context. */
    if (scope_is(ssep, sck_function_access)) {
      perform_deferred_access_checks_at_depth(depth_scope_stack);
    }  /* if */
    depth_of_innermost_scope_that_affects_access_control = orig_access_depth;
    end_deferral_of_access_checks();
  }  /* if */
  if (scope_is(ssep, sck_function_access)) {
    /* The presence of a function access scope means that the template
       decl. info. from a class or function is being used, so it
       must not be freed below. */
    pop_scope();
  } else {
    ssep = &scope_stack[depth_innermost_instantiation_scope];
    tdip = ssep->template_decl_info;
  }  /* if */
  pop_template_instantiation_scope();
  /* If the parameters field is NULL, that means this was a dummy entry
     allocated when the rescan scope was pushed, and the entry should be
     freed. */
  if (tdip != NULL && tdip->parameters == NULL) free_template_decl_info(tdip);
}  /* pop_instantiation_scope_for_rescan */


void push_enclosing_class_scope_for_rescan(a_type_ptr     enclosing_class,
                                           a_routine_ptr  assoc_routine)
/*
Push a class reactivation context for the specified enclosing class for
expression rescan.  assoc_routine is the associated routine and can be NULL.
*/
{
  a_scope_depth  orig_depth = depth_scope_stack;
  a_scope_depth  orig_access_depth =
                          depth_of_innermost_scope_that_affects_access_control;

  (void)push_scope(sck_instantiation_context, NO_SCOPE_NUMBER, NULL, NULL);
  reactivate_class_context(NULL, enclosing_class, NULL, NULL, assoc_routine,
                           PS_NONREAL_INSTANTIATION | PS_IS_RESCAN);
  scope_stack_top().in_nonreal_instantiation = TRUE;
  scope_stack_top().orig_depth = orig_depth;
  scope_stack_top().saved_innermost_scope_that_affects_access =
                                                             orig_access_depth;
  (void)push_scope(sck_function_access, NO_SCOPE_NUMBER, (a_type_ptr)NULL,
                   assoc_routine);
  /* Save the innermost scope that affects access control and defer
     access checks during the rescan. */
  if (orig_access_depth != NO_SCOPE_DEPTH) {
    scope_stack_top().orig_access_depth = orig_access_depth;
    begin_deferral_of_access_checks();
  }  /* if */
}  /* push_enclosing_class_scope_for_rescan */


void pop_enclosing_class_scope_for_rescan(void)
/*
Pop the enclosing class scope pushed by push_enclosing_class_scope_for_rescan.
*/
{
  a_scope_stack_entry_ptr  ssep = &scope_stack_top();
  a_scope_depth            orig_access_depth = ssep->orig_access_depth;
  if (orig_access_depth != NO_SCOPE_DEPTH) {
    /* If an original access-checking depth was saved, repeat any access checks
       that were deferred. */
    perform_deferred_access_checks_at_depth(depth_scope_stack);
    depth_of_innermost_scope_that_affects_access_control = orig_access_depth;
    end_deferral_of_access_checks();
  }  /* if */
  check_assertion(scope_is(ssep, sck_function_access));
  pop_scope();
  pop_class_reactivation_scope();
}  /* pop_enclosing_class_scope_for_rescan */


void set_template_decl_info_for_class_definition(
				a_template_decl_info_ptr	tdip,
				a_type_ptr			class_type)
/*
Set the fields of tdip to values suitable to process the definition of
class_type by get_definition_of_class.
*/
{
  if (class_type != NULL) {
    tdip->enclosing_scope = class_type->source_corresp.parent_scope;
    tdip->name_linkage =
       enum_cast<a_name_linkage_kind>(class_type->source_corresp.name_linkage);
  }  /* if */
}  /* set_template_decl_info_for_class_definition */


static void push_definition_context_for_class(
				a_type_ptr			class_type,
				a_push_scope_options_set	options)
/*
This routine is used when we need to reactivate a class, but we are no
longer in the context of the class (i.e., we are not in the class scope
or a class reactivation scope).  This routine pushes a dummy instantiation
context.  This routine is not used for template classes, which will have
their own actual instantiation scopes pushed.  options is a set of option
flags that is passed down to the other scope pushing routines.
*/
{
  a_symbol_ptr			class_sym;
  a_template_decl_info_ptr	tdip;

  tdip = alloc_template_decl_info();
  set_template_decl_info_for_class_definition(tdip, class_type);
  class_sym = symbol_for(class_type);
  (void)push_template_instantiation_scope(
                              tdip, class_type, (a_routine_ptr)NULL,
                              class_sym, class_sym,
                              (a_template_arg_ptr)NULL,
                              /*push_lex_state=*/FALSE,
                              options | PS_CLASS_DEFINITION_CONTEXT |
                                        PS_NEW_INSTANTIATION_CONTEXT);
}  /* push_definition_context_for_class */


void push_instantiation_scope_for_constraint_type(void)
/*
Push an instantiation scope surrounding the definition of a C++/CLI constraint
type.  Also used to set up other "dummy" instantiation scopes for error
recovery purposes.
*/
{
  a_template_decl_info_ptr  tdip;

  tdip = alloc_template_decl_info();
  (void)push_template_instantiation_scope(
                              tdip, (a_type_ptr)NULL, (a_routine_ptr)NULL,
                              (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                              (a_template_arg_ptr)NULL,
                              /*push_lex_state=*/FALSE, PS_NO_OPTIONS);
  /* Don't include this scope in any diagnostic output that may be produced. */
  scope_stack[depth_innermost_instantiation_scope].
                                            exclude_from_context_output = TRUE;
}  /* push_instantiation_scope_for_constraint_type */


void pop_instantiation_scope_for_constraint_type(void)
/*
Pop the instantiation scope pushed by
push_instantiation_scope_for_constraint_type.
*/
{
  a_template_decl_info_ptr	tdip;

  tdip = scope_stack[depth_innermost_instantiation_scope].template_decl_info;
  pop_template_instantiation_scope();
  free_template_decl_info(tdip);
}  /* pop_instantiation_scope_for_constraint_type */

#if MICROSOFT_EXTENSIONS_ALLOWED

void push_instantiation_scope_for_boxed_enum_type(void)
/*
Push an instantiation scope surrounding the definition of a C++/CLI boxed enum
type.
*/
{
  a_template_decl_info_ptr  tdip;

  tdip = alloc_template_decl_info();
  (void)push_template_instantiation_scope(
                              tdip, (a_type_ptr)NULL, (a_routine_ptr)NULL,
                              (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                              (a_template_arg_ptr)NULL,
                              /*push_lex_state=*/FALSE, PS_NO_OPTIONS);
  /* Don't include this scope in any diagnostic output that may be produced. */
  scope_stack[depth_innermost_instantiation_scope].
                                            exclude_from_context_output = TRUE;
}  /* push_instantiation_scope_for_boxed_enum_type */


void pop_instantiation_scope_for_boxed_enum_type(void)
/*
Pop the instantiation scope pushed by
push_instantiation_scope_for_boxed_enum_type.
*/
{
  a_template_decl_info_ptr	tdip;

  tdip = scope_stack[depth_innermost_instantiation_scope].template_decl_info;
  pop_template_instantiation_scope();
  free_template_decl_info(tdip);
}  /* pop_instantiation_scope_for_boxed_enum_type */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

void push_new_top_level_declaration(void)
/*
Push an instantiation scope surrounding the definition of a top level
declaration (e.g., a builtin declaration).  Can be called in C or C++ mode.
Use pop_scope to pop the scope.
*/
{
  depth_innermost_namespace_scope = DEPTH_OF_FILE_SCOPE;
  (void)push_scope_full((a_scope_kind)sck_instantiation_context,
                        NO_SCOPE_NUMBER, NULL, NULL, (a_namespace_ptr)NULL,
                        (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                        (a_template_arg_ptr)NULL,
                        (a_template_decl_info_ptr)NULL,
                        (an_object_lifetime_ptr)NULL,
                        (a_scope_ptr)NULL, (a_scope_pointers_block_ptr)NULL,
                        PS_NEW_INSTANTIATION_CONTEXT);
  innermost_function_scope = NULL;
  depth_innermost_function_scope = NO_SCOPE_DEPTH;
}  /* push_new_top_level_declaration */


void push_template_declaration_scope_full(
		a_template_decl_info_ptr	decl_info,
		a_scope_number			scope_number,
		a_boolean			is_template_param,
		a_boolean			is_template_param_rescan)
/*
Push a template declaration scope.  scope_number is the scope number to
reuse or NO_SCOPE_NUMBER.  is_template_param is TRUE if this is the
template declaration scope for the template parameter list of a template
template parameter.  is_template_param_rescan is TRUE if this scope is
for the rescan of a dependent template template parameter.
*/
{
  a_push_scope_options_set	ps_options = PS_NO_OPTIONS;

  if (is_template_param) ps_options |= PS_IS_TEMPLATE_TEMPLATE_PARAM;
  if (is_template_param_rescan) ps_options |= PS_IS_TEMPLATE_PARAM_RESCAN;
  (void)push_scope_full((a_scope_kind)sck_template_declaration,
                        scope_number,
                        (a_type_ptr)NULL, (a_routine_ptr)NULL,
                        (a_namespace_ptr)NULL, (a_symbol_ptr)NULL,
                        (a_symbol_ptr)NULL, (a_template_arg_ptr)NULL,
                        decl_info,
                        (an_object_lifetime_ptr)NULL,
                        (a_scope_ptr)NULL, (a_scope_pointers_block_ptr)NULL,
                        ps_options);
}  /* push_template_declaration_scope_full */


void push_template_declaration_scope(
		a_template_decl_info_ptr	decl_info,
		a_boolean			is_template_param_rescan)
/*
Interface to push_template_declaration_scope_full that provides a
default value for scope_number.
*/
{
  push_template_declaration_scope_full(decl_info, NO_SCOPE_NUMBER,
                                       /*is_template_template_param=*/FALSE,
                                       is_template_param_rescan);
}  /* push_template_declaration_scope */


#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
static
a_boolean check_for_file_scope_type_with_same_name(a_symbol_ptr sym_to_find)
/*
This routine is used to implement the "transitional model" for nested types.
Given a symbol this routine looks for a file scope type (class, struct, union,
typedef, or enum) with the same name.  Returns TRUE if one is found, FALSE
otherwise.
*/
{
  a_symbol_ptr		sym;
  a_boolean		found = FALSE;
  a_symbol_header_ptr	sym_header;

  sym_header = sym_to_find->header;
  /* Get the symbol list on which file scope symbols can be found. */
  sym = symbol_list_for_file_scope_symbols(sym_header);
  while (sym != NULL) {
    if (sym->decl_scope == file_scope_number) {
      /* Look for class, struct, union, enum, or typedef. */
      if (is_tag_symbol(sym) || sym->kind == (a_symbol_kind)sk_type) {
        found = TRUE;
        break;
      }  /* if */
    }  /* if */
    sym = sym->next;
  }  /* while */
  return found;
}  /* check_for_file_scope_type_with_same_name */


static
a_symbol_ptr find_cfront_transitional_nested_type_symbol
                                             (a_symbol_ptr sym_to_find)
/*
Given a symbol looks through the inactive list for a type symbol
of the same name whose type has the transitional name mangling flag set.
This is used for error generation of the transitional model for nested
type support.
*/
{
  a_symbol_ptr  sym;

  sym = sym_to_find->header->inactive_symbols;
  while (sym != NULL) {
    /* Look for class, struct, union, enum, or typedef. */
    if (is_tag_symbol(sym) || sym->kind == (a_symbol_kind)sk_type) {
      a_type_ptr  sym_type = type_symbol_type(sym);
      if (sym_type->use_cfront_transitional_nested_type_name_mangling) {
        break;
      }  /* if */
    }  /* if */
    sym = sym->next;
  }  /* while */
  return sym;
}  /* find_cfront_transitional_nested_type_symbol */
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */


a_boolean routine_defined(a_routine_ptr	rp)
/*
Return TRUE if a definition has been supplied for the routine pointed to
by "rp".  If "rp" is a template instance that has not been explicitly
specialized, return TRUE if a definition has been supplied for the
associated template.
*/
{
  a_boolean	result = FALSE;

  if (rp->is_defaulted || rp->is_deleted) {
    result = TRUE;
  } else if (rp->is_template_function &&
             !rp->is_specialized && !rp->compiler_generated) {
    a_template_symbol_supplement_ptr	tssp;
    a_symbol_ptr			sym;
    a_symbol_ptr			template_sym;
    sym = (a_symbol_ptr)rp->source_corresp.assoc_info;
    check_assertion(sym != NULL);
    template_sym = sym->variant.routine.instance_ptr->template_sym;
    tssp = template_supplement_for_symbol(template_sym);
    result = !cache_for_template(tssp)->tokens.is_empty();
#if GNU_EXTENSIONS_ALLOWED
  } else if (has_gnu_routine_supp(rp) &&
             gnu_routine_supp(rp)->aliased_routine != NULL) {
    /* Consider an alias defined if the entity it aliases is defined. */
    result = routine_defined(gnu_routine_supp(rp)->aliased_routine);
#endif /* GNU_EXTENSIONS_ALLOWED */
  } else if (routine_has_been_defined(rp)) {
    result = TRUE;
  }  /* if */
  return result;
}  /* routine_defined */


static void check_referenced_member_functions(a_scope_ptr scope,
                                              a_boolean   is_function_local,
                                              a_boolean   within_unnamed_class)
/*
If scope is a class scope, issue an error for any of its member functions
that have been referenced but are undefined and lack external linkage (i.e.,
inline member functions and member functions of local classes); if the class
has nested classes, check their member functions, too.  If it is not a class
scope, apply that check to the member functions of each class declared in
the scope.  is_function_local is TRUE if this scope belongs to a function
body.  Only called in C++ mode.
*/
{
  a_routine_ptr            rp;
  a_type_ptr               tp;
  a_scope_ptr              class_scope;
  a_symbol_ptr             sym;
  a_template_instance_ptr  tip;
  a_boolean                is_inline_virtual;

  /* Examine each of the class types on the types list of the scope (except
     prototype instantiations, since their member functions are never truly
     used).  If this is a class scope, it picks up the nested classes. */
  for (tp = scope->types; tp != NULL; tp = tp->next) {
    if (is_immediate_class_type(tp) &&
        !tp->variant.class_struct_union.is_nonreal_class) {
      class_scope = class_type_supp(tp)->assoc_scope;
      if (!scope_is_null_or_placeholder(class_scope)) {
        /* Check the member functions of the nested class. */
        check_referenced_member_functions(class_scope, is_function_local,
                                          (within_unnamed_class ||
                                           tp->variant.class_struct_union.
                                                         originally_unnamed));
      }  /* if */
    }  /* if */
  }  /* for */
  /* If this is a file, function, or block scope, do nothing more.  If it's
     a class scope, check its member functions. */
  if (scope->kind == (a_scope_kind)sck_class_struct_union &&
      /* Don't process classes promoted out of functions by IL lowering a
         second time when the file scope list is scanned. */
      scope->variant.assoc_type->source_corresp.is_local_to_function ==
                                                           is_function_local) {
    /* Now go though each routine entry for the current class. */
    for (rp = scope->routines; rp != NULL; rp = rp->next) {
      sym = (a_symbol_ptr)rp->source_corresp.assoc_info;
      if (!(routine_defined(rp) || (sym != NULL && sym->defined)) &&
          !rp->compiler_generated) {
        /* An undefined member function. */
        is_inline_virtual =
                    (rp->is_inline && rp->is_virtual && !rp->pure_virtual);
        if (rp->source_corresp.referenced || is_inline_virtual) {
          /* Either the function was actually referenced or could be
             referenced using the virtual function call mechanism. */
          if (sym != NULL) {
            tip = sym->variant.routine.instance_ptr;
            if (tip != NULL &&
                (tip->explicit_instantiation ||
                 tip->suppress_instantiation)) {
              /* A template function that has either been explicitly
                 instantiated (in which case an error would have been issued
                 on the explicit instantiation attempt) or for which some
                 error that should prevent the instantiation has already been
                 issued. */
            } else if (within_unnamed_class && rp->is_virtual) {
              /* Diagnostic has already been issued. */
            } else if (is_function_local) {
              /* An undefined member function of a local class. */
              if (rp->is_virtual && !rp->pure_virtual) {
                /* Diagnostic has already been put out in
                   class_member_declaration (virtual functions are considered
                   referenced immediately). */
              } else if (strict_ansi_mode) {
                pos_sy_error(ec_local_class_function_def_missing,
                             &sym->decl_position, sym);
              } else {
                /* Issue a warning in non-strict mode (other compilers
                   allow an unused local class to go un-diagnosed). */
                pos_sy_warning(ec_local_class_function_def_missing,
                               &sym->decl_position, sym);
              }  /* if */
            } else if (rp->source_corresp.referenced &&
                       !is_member_of_unnamed_namespace(&rp->source_corresp) &&
#if MICROSOFT_EXTENSIONS_ALLOWED
                       !(ms_extensions &&
                         (rp->decl_modifiers & DM_DLLIMPORT)) &&
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_FUNCTION_MULTIVERSIONING
                       !is_multiversion_representative(rp) &&
#endif /* GNU_FUNCTION_MULTIVERSIONING */
                       (rp->is_inline ||
                        rp->storage_class != (a_storage_class)sc_extern)) {
              /* A referenced but undefined member function that is either
                 extern-inline or has internal linkage.  If the class is a
                 member of an anonymous namespace this diagnostic has already
                 been issued. */
              an_error_severity  severity = es_discretionary_error;
              if (microsoft_mode ||
                  (!rp->is_inline && gnu_mode) ||
                  (gpp_mode && rp->is_inline &&
                   rp->storage_class == (a_storage_class)sc_extern)) {
                /* Microsoft compilers do not diagnose these sorts of
                   situations.  Similarly, GNU C++ accepts undefined extern
                   inline functions even though they are used, and accepts
                   undefined functions with internal linkage. */
                severity = (an_error_severity)es_warning;
              }  /* if */
              pos_sy_diagnostic(severity, ec_never_defined,
                                &sym->decl_position, sym);
            } else if (is_inline_virtual) {
              /* A non-local inline virtual function that is undefined and
                 unreferenced. */
              if (strict_ansi_mode) {
                /* In strict mode this is an error, because simply being
                   declared virtual counts as a use, and inline functions
                   that are used must be defined. */
                pos_sy_diagnostic(strict_ansi_discretionary_severity,
                                  ec_virtual_inline_never_defined,
                                  &sym->decl_position, sym);
#if DO_IL_LOWERING
              } else if (is_primary_translation_unit &&
                         inline_virtual_function_definitions_needed(
                                                 scope->variant.assoc_type)) {
                /* The virtual function table will be generated for the
                   class in this translation unit, so the function's
                   address will be taken.  Since there's no definition,
                   this will usually result in a linker error, so the user
                   may benefit from an earlier diagnostic. */
                pos_sy_warning(ec_virtual_inline_never_defined,
                               &sym->decl_position, sym);
#endif /* DO_IL_LOWERING */
              } else {
                /* By default we issue no diagnostic. */
              }  /* if */
              /* Modify the routine entry so that the IL will be valid.
                 (Otherwise, for example, if a virtual function table is put
                 out for the class of which this function is a member, there
                 may appear to be a reference to an undefined function with
                 static storage class.  This way, a linker error will be
                 forced.) */
              rp->storage_class = (a_storage_class)sc_extern;
              rp->source_corresp.name_linkage =
                               (a_name_linkage_kind)nlk_cplusplus_external;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
}  /* check_referenced_member_functions */


static void report_unreferenced(a_symbol_ptr  	  sym,
                                an_error_code	  error_code,
			        an_error_severity normal_severity)
/*
Issue a warning for an unreferenced entity.  However, demote the warning to
a remark if the entity is a file-scope entity declared in an include file.
*/
{
  if (normal_severity == es_none) {
    /* Call the diagnostic routine anyway to allow the user to choose to
       see the warning. */
    pos_sy_diagnostic(es_none, error_code, &sym->decl_position, sym);
  } else if (normal_severity == es_remark ||
      (depth_scope_stack == DEPTH_OF_FILE_SCOPE &&
       seq_is_in_include_file(sym->decl_position.seq))) {
    pos_sy_remark(error_code, &sym->decl_position, sym);
  } else {
    pos_sy_warning(error_code, &sym->decl_position, sym);
  }  /* if */
}  /* report_unreferenced */


/*
Return TRUE if a type is a completable incomplete type, i.e., it's incomplete
but it's not void.
*/
#define is_completable_type(tp)                                       \
  (is_incomplete_type(tp) && !is_void_type(tp))


/*
Return TRUE if a type is an array type whose elements have a complete
type.  This rules out arrays of incomplete struct/union types (an extension).
*/
#define is_array_with_complete_element_type(tp)                       \
  (is_array_type(tp) && !is_incomplete_type(array_element_type(tp)))


static void end_of_scope_member_function_check(a_symbol_ptr  rout_sym,
					       a_routine_ptr rp,
					       a_type_ptr    class_type,
					       a_boolean     unnamed_ns_member)
/*
Do end-of-scope checking for the member function specified by rout_sym
and rp, which is a member of class_type.  unnamed_ns_member is TRUE if
the outermost class was defined in an unnamed namespace.
*/
{
  if (unnamed_ns_member) {
    /* A member of an unnamed namespace must be defined if used.  We also
       give a diagnostic if a member is declared but not used. */
    if ((rp->source_corresp.referenced
#if IA64_ABI && DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
         && rp->overridden_function_for_wrapper == NULL
#endif /* IA64_ABI && DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL... */
                                                       ) ||
        (rp->is_virtual && !rp->pure_virtual && !rp->compiler_generated)) {
      /* A referenced function or a virtual function.  Virtual
         functions are in some way always "referenced" by the virtual
         function table, but pure virtual functions and compiler
         generated virtual functions (destructors) do not always need
         to have a definition.  Similarly, no diagnostic should be
         issued for IA-64 virtual call thunks. */
      if (!routine_defined(rp)) {
        an_error_severity  sev = es_discretionary_error;
        a_symbol_ptr       parent_sym = symbol_for(class_type);
        if (!strict_ansi_mode &&
            parent_sym != NULL && !parent_sym->referenced) {
          /* If the enclosing class is unreferenced, the lack of a definition
             for a virtual function is rarely a serious problem. */
          sev = es_remark;
        } else if (gnu_mode || microsoft_mode) {
          /* GCC, Clang and MSVC either don't diagnose this or only issue
             warnings. */
          sev = es_warning;
        }  /* if */
        pos_sy_diagnostic(sev,
                          rp->is_virtual ? ec_virtual_function_never_defined
                                         : ec_never_defined,
                          &rp->source_corresp.decl_position, rout_sym);
      }  /* if */
    } else if (!rout_sym->referenced &&
               !rp->compiler_generated &&
               !rp->is_virtual &&
               !rp->is_defaulted &&
               !rp->is_prototype_instantiation &&
               /* Don't warn about members that might be declared
                  to avoid compiler generated declarations. */
               !((special_kind_is(rp, sfk_constructor) ||
                  special_kind_is(rp, sfk_destructor) ||
                  (special_kind_is(rp, sfk_operator) &&
                   rp->variant.opname_kind == (an_opname_kind)onk_assign)) &&
                 !routine_defined(rp))) {
      a_symbol_ptr parent_sym = symbol_for(class_type);
      a_type_ptr   parent_type = type_symbol_type(parent_sym);
      /* For member functions of a template class, only warn if the parent
         class is not referenced. */
      if (!parent_type->variant.class_struct_union.is_template_class ||
          !parent_sym->referenced) {
        report_unreferenced(rout_sym, ec_declared_but_not_referenced,
                          es_warning);
      }  /* if */
    }  /* if */
  }  /* if */
  /* Check if this routine was declared using a type with no linkage.  The
     check is done for routines that are referenced but not defined.
     The not_needed_or_will_be_instantiated check is used so that a template
     that could be instantiated is considered defined. */
  if (decls_using_types_without_linkage_allowed &&
      rout_sym->referenced &&
      (rp->storage_class == (a_storage_class)sc_extern &&
       !rp->is_prototype_instantiation &&
       /*lint -e(506)*/!rout_is_generic_instance(rp) &&
       (!rp->is_template_function ||
        !not_needed_or_will_be_instantiated(rout_sym)))) {
    check_constituent_types_have_linkage(rout_sym,
                                         &rout_sym->decl_position,
                                         /*is_declaration=*/FALSE);
  }  /* if */
}  /* end_of_scope_member_function_check */


static void end_of_scope_static_data_member_check(
					a_symbol_ptr   var_sym,
					a_variable_ptr vp,
					a_boolean      unnamed_ns_member)
/*
Do end-of-scope checking for the static data member specified by var_sym
and vp.  unnamed_ns_member is TRUE if the outermost class was defined in
an unnamed namespace.
*/
{
  if (unnamed_ns_member) {
    /* A member of an unnamed namespace must be defined if used.  We also
       give a diagnostic if a member is declared but not used. */
    if (!var_sym->referenced) {
      report_unreferenced(var_sym, ec_declared_but_not_referenced, es_warning);
    } else if ((vp->used || var_sym->value_has_been_set) &&
               !vp->is_member_constant && !var_sym->defined) {
      an_error_severity severity = es_discretionary_error;
      if (microsoft_mode) {
        severity = es_warning;
      } else if (gnu_mode) {
        if (sdm_supp(var_sym) != NULL &&
            sdm_supp(var_sym)->token_cache != a_shared_token_cache()) {
          /* In g++ and clang modes, the is_member_constant flag of a
             static data member of a class template instance is only set
             when its initializer is instantiated.  The presence of a token
             cache for this static data member indicates that that did not
             occur, so avoid issuing a diagnostic that might prove to be
             erroneous if the initializer were instantiated. */
          severity = es_none;
        } else {
          severity = es_warning;
        }  /* if */
      }  /* if */
      if (severity != es_none) {
        pos_sy_diagnostic(severity, ec_never_defined,
                          &vp->source_corresp.decl_position, var_sym);
      }  /* if */
    }  /* if */
  }  /* if */
  /* Check if this variable was declared using a type with no linkage.  The
     check is done for routines that are referenced but not defined.
     The not_needed_or_will_be_instantiated check is used so that a template
     that could be instantiated is considered defined. */
  if (decls_using_types_without_linkage_allowed &&
      (vp->used || var_sym->value_has_been_set) &&
      (vp->storage_class == (a_storage_class)sc_extern &&
       (!vp->is_template_variable ||
        vp->is_nonreal ||
        vp->is_prototype_instantiation ||
        !not_needed_or_will_be_instantiated(var_sym)))) {
    check_constituent_types_have_linkage(var_sym,
                                         &var_sym->decl_position,
                                         /*is_declaration=*/FALSE);
  }  /* if */
}  /* end_of_scope_static_data_member_check */


static void end_of_scope_symbol_check_for_class(a_symbol_ptr  sym,
						a_scope_kind  scope_kind)
/*
This routine is called by end_of_scope_symbol_check to process classes
defined at namespace scope.  It does special end-of-scope processing for
class members, and calls itself recursively for nested classes.  sym is
the class symbol.  scope_kind identifies the kind of scope enclosing the
outermost class.
*/
{
  a_type_ptr			type = type_symbol_type(sym);
  a_boolean			unnamed_ns_member;
  a_class_type_supplement_ptr	ctsp;

  ctsp = type->variant.class_struct_union.extra_info;
  unnamed_ns_member = is_member_of_unnamed_namespace(&type->source_corresp);
  if (!scope_is_null_or_placeholder(ctsp->assoc_scope)) {
    /* A class definition was provided. */
    a_routine_ptr   rp = ctsp->assoc_scope->routines;
    a_variable_ptr  vp = ctsp->assoc_scope->variables;
    a_type_ptr      tp = ctsp->assoc_scope->types;
    /* Check each of the member function of the class.  Don't check compiler-
       generated members, or members defined as "= delete" (which implies they
       are not meant to be referenced). */
    for (; rp != NULL; rp = rp->next) {
      a_symbol_ptr rout_sym = symbol_for(rp);
      if (!rp->compiler_generated && !rp->is_deleted) {
        end_of_scope_member_function_check(rout_sym, rp, type,
                                           unnamed_ns_member);
      }  /* if */
    }  /* for */
    /* Check each of the static data members of the class. */
    for (; vp != NULL; vp = vp->next) {
      a_symbol_ptr var_sym = symbol_for(vp);
      end_of_scope_static_data_member_check(var_sym, vp, unnamed_ns_member);
    }  /* for */
    /* Check each of the nested classes. */
    for (; tp != NULL; tp = tp->next) {
      a_symbol_ptr tp_sym = symbol_for(tp);
      /* Only process class types.  Some compiler-generated types are
         created without associated symbols. */
      if (tp_sym != NULL && is_immediate_class_type(tp)) {
        end_of_scope_symbol_check_for_class(tp_sym, scope_kind);
      }  /* if */
    }  /* for */
  }  /* if */
}  /* end_of_scope_symbol_check_for_class */


static a_boolean diagnose_unreferenced_binding(a_variable_ptr  binding)
/*
Return TRUE if a diagnostic should be issued about the given binding being
unreferenced.  Currently, a diagnostic is only issued if none of the bindings
in the associated container is referenced, and this is the first of those
bindings.
*/
{
  a_boolean       result = FALSE;
  a_variable_ptr  container = binding->variant.container;
  an_il_entity_list_entry_ptr
                  ielep = container->variant.bindings;

  if (ielep != NULL && (a_variable_ptr)ielep->entity.ptr == binding) {
    /* This is the first binding on the list.  Return TRUE if all the other
       bindings are also unreferenced. */
    result = TRUE;
    for (ielep = ielep->next; ielep != NULL; ielep = ielep->next) {
      if (symbol_for((a_variable_ptr)ielep->entity.ptr)->referenced) {
        result = FALSE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return result;
}  /* diagnose_unreferenced_binding */


static void check_if_thread_local_needed(a_symbol_ptr sym)
/*
*/
{
  check_assertion(symbol_is(sym, sk_variable));
  a_variable_ptr var = sym->variant.variable.ptr;
  if (var->storage_class == (a_storage_class)sc_unspecified ||
      var->storage_class == (a_storage_class)sc_static) {
    if (!var->compiler_generated &&
        !var->is_thread_local && !var->has_explicit_initializer) {
#if 0
      a_const_char *file_name;
      a_const_char *full_name;
      a_line_number line_number;
      a_boolean     at_eos;
      a_const_char  *old_string;
      a_const_char  *new_string;
      if (var->storage_class == (a_storage_class)sc_static) {
        old_string = "static";
        new_string = "STATIC_THREAD";
      } else {
        old_string = "EXTERN";
        new_string = "EXTERN_THREAD";
      }  /* if */
#endif /* 0 */
#if 0
      conv_seq_to_file_and_line(
                  var->source_corresp.decl_pos_info->
                                                    specifiers_range.start.seq,
                  &file_name,
                  &full_name,
                  &line_number,
                  &at_eos);
      if (!at_eos) {
        fprintf(f_debug, "%s: %d s/%s/%s/\n", file_name, line_number,
                old_string, new_string);
      }  /* if */
#endif /* 0 */
#if 0
      pos_warning(ec_debug_show_location,
                  &var->source_corresp.decl_pos_info->specifiers_range.start);
#endif /* 0 */
    }  /* if */
  }  /* if */
}  /* check_if_thread_local_needed */


static void end_of_scope_symbol_check(a_symbol_ptr  sym,
				      a_scope_kind  scope_kind,
                                      a_routine_ptr curr_routine)
/*
The symbol sym is about to be removed from the symbol table at the end of
a scope.  Do any checking or processing required (e.g., issue a warning
message if the symbol is unreferenced).  scope_kind identifies the kind of
scope containing the symbol.  If the scope that is ending is for a function,
curr_routine points to the routine entry; otherwise, it is NULL.
*/
{
  a_storage_class          storage_class;
  a_type_ptr               var_type;
  a_variable_ptr           var_ptr;
  a_routine_ptr            rout_ptr;
  a_boolean                anon_ns_mem;
#if CHECKING
  a_source_correspondence  *scp = NULL;
#endif /* CHECKING */
  a_boolean                suppress_warning;
  an_init_kind             init_kind;
  an_initializer_ptr       ip;

  switch (sym->kind) {
    case sk_variable:
      /* Variable or parameter. */
      var_ptr = sym->variant.variable.ptr;
      storage_class = var_ptr->storage_class;
      anon_ns_mem = is_member_of_unnamed_namespace(&var_ptr->source_corresp) &&
                    !sym->is_class_member && sym->parent.namespace_ptr != NULL;
      check_if_thread_local_needed(sym);
      if (scope_kind == (a_scope_kind)sck_file &&
          var_ptr->used && var_ptr->is_inline &&
          (storage_class == (a_storage_class)sc_unspecified ||
           storage_class == (a_storage_class)sc_extern) &&
          !sym->defined) {
        /* An extern inline variable used but not defined in this
           translation unit. */
        pos_sy_error(ec_extern_inline_never_defined, &sym->decl_position,
                     sym);
      } else if ((storage_class == (a_storage_class)sc_unspecified ||
                  storage_class == (a_storage_class)sc_extern) &&
                 (!anon_ns_mem || var_ptr->source_corresp.name_linkage ==
                                         (a_name_linkage_kind)nlk_external)) {
        /* Note that if this test succeeds (e.g., the variable has
           storage class sc_unspecified), we do not do the test for
           referenced.  That's because an external variable can be assumed
           to be referenced from another compilation unit.  The referenced
           flag in the IL entry is set slightly later, in the
           sk_extern_variable processing.  Usually, we issue no warning for
           unused "extern" variables; this is a long-standing C convention. */
      } else if (anon_ns_mem) {
        /* In unnamed namespaces a warning is issued for unannotated unused
           declarations, and an error for missing definitions that were
           referenced. */
        if (!sym->referenced && !var_ptr->source_corresp.maybe_unused) {
          an_error_severity severity = es_warning;
          get_variable_initializer(var_ptr,
                                   scope_stack[depth_scope_stack].il_scope,
                                   &init_kind, &ip);
          if (init_kind == (an_init_kind)initk_dynamic &&
              (dynamic_init_has_side_effects(ip->dynamic,
                                             /*for_unused_var=*/TRUE,
                                             &suppress_warning) ||
               suppress_warning)) {
            severity = es_remark;
          }  /* if */
          report_unreferenced(sym, ec_declared_but_not_referenced, severity);
        } else if ((var_ptr->used || sym->value_has_been_set) &&
                   !sym->defined) {
          an_error_severity severity = es_discretionary_error;
          if (gnu_mode || microsoft_mode) {
            severity = es_warning;
          }  /* if */
          pos_sy_diagnostic(severity, ec_never_defined, &sym->decl_position,
                            sym);
        }  /* if */
      } else if (is_const_qualified_type(var_ptr->type) &&
                 (sym->referenced ||
                  (!C_mode() && depth_scope_stack == DEPTH_OF_FILE_SCOPE &&
                   seq_is_in_include_file(sym->decl_position.seq)))) {
        /* Since a const variable defined in a header is the C++ idiom
           corresponding to #define, issue no diagnostic on not using it.
           Also, a const variable declaration without an initializer is
           frequently invalid.  So don't issue other warnings (like
           "set but not used") if the variable is referenced at all. */
      } else if (var_ptr->is_parameter) {
        if (!sym->referenced) {
          /* An unreferenced parameter.  Warn unless a lint-style "argsused"
             comment appeared.  Also do not warn for parameters of "main". */
#if CHECKING
          if (curr_routine == NULL) {
            internal_error(
               "end_of_scope_symbol_check: parameter with no assoc routine");
          }  /* if */
#endif /* CHECKING */
          /* In C++ a routine type can have type qualifiers above it. */
          if (skip_typerefs(curr_routine->type)->variant.routine.
                                              extra_info->lint_argsused_flag) {
            /* The "argsused" flag was specified, so no warning is issued. */
          } else if (curr_routine == il_header.main_routine) {
            /* No warning for arguments of the main program, since
               they're dictated by the environment. */
#if ASM_FUNCTION_ALLOWED
          } else if (curr_routine->storage_class == (a_storage_class)sc_asm) {
            /* Parameters of "asm" functions are not referenced in the 
               usual way, so do not issue warnings. */
#endif /* ASM_FUNCTION_ALLOWED */
          } else if (var_ptr->source_corresp.maybe_unused
#if GNU_EXTENSIONS_ALLOWED
                     || skip_typerefs(var_ptr->type)
                                      ->variables_are_implicitly_referenced
#endif /* GNU_EXTENSIONS_ALLOWED */
                                                                           ) {
            /* Do not issue a remark about an unused parameter if the
               source explicitly annotated the parameter as being unused
               through the "maybe_unused" or GNU "unused" attribute. */
          } else {
            a_param_type_ptr	ptp = var_ptr->variant.assoc_param_type;
            if (ptp != NULL && ptp->is_pack_element && sym->is_invisible) {
              /* Non-initial pack element parameter variables are marked as
                 invisible.  Such symbols are never marked as referenced. */
            } else if (is_nonspecialized_instantiation_context() &&
                       is_real_instantiation_context()) {
              /* Don't issue unreferenced warnings in template instantiations.
                 These will have been issued in the prototype instantiation
                 if they would occur in all instantiations.  In addition, there
                 is no way for a user to fix such a diagnostic that occurs
                 only in some instantiations.  A parameter might be considered
                 unreferenced in an instantiation if it was used in a pack
                 expansion that was empty in that instantiation. */
            } else {
              /* Unreferenced parameter. */
              report_unreferenced(sym, ec_unreferenced_function_param,
                                  es_remark);
            }  /* if */
          }  /* if */
        } else if (var_ptr->param_value_has_been_changed && !var_ptr->used) {
          report_unreferenced(sym, ec_set_but_not_used, es_warning);
        }  /* if */
      } else if (var_ptr->is_handler_param) {
        /* A handler parameter. */
        if (!sym->referenced) {
          /* Unreferenced handler parameter. */
          report_unreferenced(sym, ec_declared_but_not_referenced,
                              es_remark);
        } else if (var_ptr->param_value_has_been_changed && !var_ptr->used) {
          report_unreferenced(sym, ec_set_but_not_used, es_warning);
        }  /* if */
      } else if ((!sym->referenced ||
                  (sym->value_has_been_set && !var_ptr->used))
                 && !var_ptr->source_corresp.maybe_unused
                 && (!var_ptr->is_struct_binding ||
                     diagnose_unreferenced_binding(var_ptr))
#if GNU_EXTENSIONS_ALLOWED
                 && !var_ptr->has_gnu_used_attribute
                 && !var_ptr->is_weakref
#endif /* GNU_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
                 && var_ptr->section == NULL
#endif /* GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
                                            ) {
        /* An unreferenced or unused variable or an unused parameter.  Nonreal
           class types and template parameter types may yet have unestablished
           side effects and no diagnostic should be issued (see below).  In
           GNU C, variables with internal linkage are concatenated within their
           section, which is sometimes used to create link-time chains. */
        a_type_ptr  type = skip_array_types(var_ptr->type);
        type = skip_typerefs(type);
        if (type->kind != (a_type_kind)tk_template_param &&
            !(is_immediate_class_type(type) &&
              type->variant.class_struct_union.is_nonreal_class) &&
#if GNU_EXTENSIONS_ALLOWED
            !type->variables_are_implicitly_referenced &&
#endif /* GNU_EXTENSIONS_ALLOWED */
            !is_error_type(type)) {
          an_error_code     error_code;
          an_error_severity severity = es_warning;
          a_boolean         suppress_diagnostic = FALSE;

          /* Check for a dynamic initialization that has side effects (such as
             a constructor call).  If such an initialization exists, suppress
             the warning. */
          get_variable_initializer(var_ptr,
                                   scope_stack[depth_scope_stack].il_scope,
                                   &init_kind, &ip);
          if (nonclass_prototype_instantiations &&
              is_nonspecialized_instantiation_context() &&
              is_real_instantiation_context()) {
            /* Don't issue unreferenced warnings in template instantiations.
               These will have been issued in the prototype instantiation
               if they would occur in all instantiations.  In addition, there
               is no way for a user to fix such a diagnostic that occurs
               only in some instantiations. */
            suppress_diagnostic = TRUE;
          } else if (is_prototype_instantiation_context() &&
                     is_local_symbol(sym) && sym->referenced) {
            /* In prototype instantiation contexts, it is not always possible
               to determine the nature of a local variable reference.  Do not
               warn about them. */
            suppress_diagnostic = TRUE;
          } else if (var_ptr->is_enhanced_for_iterator) {
            /* Iterator variables of C++/CLI "for each" statements or
               range-based-for statements shouldn't get warnings if
               unreferenced.  The loop itself might be the side effect. */
            severity = es_remark;
          } else if (init_kind == (an_init_kind)initk_dynamic) {
            a_dynamic_init_ptr dip = ip->dynamic;
            if (is_dynamic_init_for_vla(dip)) {
              /* Always issue a warning for an unreferenced VLA, even if
                 the declaration involves construction or destruction. */
            } else if (dip->kind == (a_dynamic_init_kind)dik_constructor ||
                       dip->destructor != NULL) {
              /* Issue no diagnostic when a variable is initialized by a
                 constructor, or when its destructor will be called. This
                 avoids spurious diagnostics when the user defines a variable
                 simply to assure that the constructor or destructor is
                 called. */
              severity = es_none;
            } else if (dynamic_init_has_side_effects(dip,
                                                     /*for_unused_var=*/TRUE,
                                                     &suppress_warning) ||
                       suppress_warning) {
              /* Initialization has side-effects -- issue a remark. */
              severity = es_remark;
            }  /* if */
          } else if (il_header.any_templates_seen &&
                     (!nonclass_prototype_instantiations ||
                      defer_function_prototype_instantiations) &&
                     instantiation_mode == tim_none &&
                     (scope_kind == (a_scope_kind)sck_file ||
                      scope_kind == (a_scope_kind)sck_namespace ||
                      scope_kind == (a_scope_kind)sck_class_struct_union)) {
            /* If we are not parsing all template definitions nor instantiating
               all template uses, it is possible that we missed a reference/use
               from a template to an enclosing scope (which can only be a file
               scope, namespace scope, or class scope): Don't issue a warning
               or remark in such cases since it would be unreliable. */
            severity = es_none;
          }  /* if */
          /* Issue different diagnostics depending on whether the variable
             was completely unreferenced or was set but not used. */
          if (!sym->referenced) {
            error_code = ec_declared_but_not_referenced;
          } else {
            check_assertion(sym->value_has_been_set);
            error_code = ec_set_but_not_used;
          }  /* if */
          if (!suppress_diagnostic) {
            /* An es_none diagnostic could still be issued if the severity
               is overridden.  suppress_diagnostic is used to make sure the
               diagnostic is never issued. */
            report_unreferenced(sym, error_code, severity);
          }  /* if */
        }  /* if */
      }  /* if */
      /* Check if this variable was declared using a type with no
         linkage. */
      if (decls_using_types_without_linkage_allowed &&
          sym->referenced && storage_class == (a_storage_class)sc_extern) {
        check_constituent_types_have_linkage(sym, &sym->decl_position,
                                             /*is_declaration=*/FALSE);
      }  /* if */
      if (symbol_for(var_ptr) != sym && !sym->is_pack_element) {
        /* If the symbol was referenced, ensure that the "primary symbol" for
           the variable is similarly marked as referenced.  This matters for
           code like the following:
             static int i;  // Referenced through a block-extern declaration.
             int f() { extern int i; return i; }
           A symbol for a parameter pack variable may point to a variable
           that does not point back to the symbol (it may point to a different
           variable of the pack).  Such cases should not mark the primary
           symbol as referenced. */
        a_symbol_ptr	primary_sym = symbol_for(var_ptr);
        if (primary_sym != NULL) {
          if (sym->referenced) primary_sym->referenced = TRUE;
          if (sym->value_has_been_set) {
            primary_sym->value_has_been_set = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
#if CHECKING
      scp = &var_ptr->source_corresp;
#endif /* CHECKING */
#if GENERATE_SOURCE_SEQUENCE_LISTS
      /* Do fixup on the source sequence entry for a tentative definition. */
      if (C_mode() && depth_scope_stack == DEPTH_OF_FILE_SCOPE &&
          sym->defined && is_primary_translation_unit) {
        a_source_sequence_entry_ptr   ssep;
        a_src_seq_secondary_decl_ptr  sssdp;
        ssep = var_ptr->source_corresp.source_sequence_entry;
        if (ssep != NULL &&
            ss_entry_kind(ssep) == iek_src_seq_secondary_decl) {
          /* This source sequence entry must represent a tentative definition
             of the variable (the first, if there were more than one) -- turn
             it into a primary declaration. */
          sssdp = ss_entry_ptr(ssep, a_src_seq_secondary_decl_ptr);
#if CHECKING
          check_assertion(sssdp->decl_position.seq == scp->decl_position.seq &&
                          sssdp->decl_position.column ==
                                                  scp->decl_position.column);
#endif /* CHECKING */
          ssep->entity.kind = iek_variable;
          ssep->entity.ptr = (char *)var_ptr;
          check_assertion(var_ptr->declared_type == NULL);
          var_ptr->declared_type = sssdp->declared_type;
          var_ptr->declared_storage_class = sssdp->declared_storage_class;
          var_ptr->embedded_source_sequence_entries =
                                       sssdp->embedded_source_sequence_entries;
          var_ptr->source_corresp.is_decl_after_first_in_comma_list =
                                      sssdp->is_decl_after_first_in_comma_list;
        }  /* if */
      }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      break;
    case sk_overloaded_function:
      /* For each function or function template symbol on the overload list
         do the check. */
      for (sym = sym->variant.overloaded_function.symbols;
           sym != NULL;
           sym = sym->next) {
        end_of_scope_symbol_check(sym, scope_kind, curr_routine);
      }  /* for */
      break;
#if CHECKING
    case sk_member_function:
      rout_ptr = sym->variant.routine.ptr;
      scp = &rout_ptr->source_corresp;
      break;
#endif /* CHECKING */
    case sk_routine:
      /* Function. */
      rout_ptr = sym->variant.routine.ptr;
      storage_class = rout_ptr->storage_class;
      if (c99_mode && strict_ansi_mode &&
          storage_class == (a_storage_class)sc_extern &&
          rout_ptr->is_inline) {
        /* In C99, an inline function with external linkage must be
           defined in the current translation unit.  We require this only
           in strict mode.  Make sure we issue an error, not a warning,
           if the function is actually referenced. */
        if (scope_kind == (a_scope_kind)sck_namespace ||
            scope_kind == (a_scope_kind)sck_file) {
          /* The diagnostic is not issued for block extern declarations. */
          pos_sy_diagnostic(rout_ptr->source_corresp.referenced ?
                                            es_discretionary_error :
                                            strict_ansi_discretionary_severity,
                            ec_inline_never_defined,
                            &sym->decl_position, sym);
        }  /* if */
      } else if (storage_class == (a_storage_class)sc_unspecified &&
                 (!is_member_of_unnamed_namespace(&rout_ptr->source_corresp) ||
                  rout_ptr->source_corresp.name_linkage ==
                                         (a_name_linkage_kind)nlk_external)) {
        /* Regard functions with "unspecified" storage class to be referenced
           somewhere, even if not in the current translation unit. extern
           inline functions are an exception, since their callability is
           "as if" they had static storage, but no warning is issued if they
           are unreferenced.  Note also that unnamed namespace members with
           extern "C" linkage can be referenced in other translation units. */
        if (!rout_ptr->is_inline) {
          rout_ptr->source_corresp.referenced = TRUE;
          sym->referenced = TRUE;
#if GNU_FUNCTION_MULTIVERSIONING
          if (is_multiversion_representative(rout_ptr)) {
            /* Propagate the referenced flag setting to the target-specific
               versions. */
            a_routine_list_entry_ptr rlep;
            for (rlep = gnu_routine_supp(rout_ptr)->
                                      mv_info.representative.targeted_versions;
                 rlep != NULL;
                 rlep = rlep->next) {
              rlep->routine->source_corresp.referenced = TRUE;
              symbol_for(rlep->routine)->referenced = TRUE;
            }  /* for */
          }  /* if */
#endif /* GNU_FUNCTION_MULTIVERSIONING */
        }  /* if */
      } else if (rout_ptr->source_corresp.referenced) {
        /* Referenced function.  We check the IL referenced flag because
           a reference in, say, a sizeof operation doesn't count. */
        if ((storage_class == (a_storage_class)sc_static ||
             is_member_of_unnamed_namespace(&rout_ptr->source_corresp)) &&
            !routine_defined(rout_ptr)) {
          /* A routine with internal linkage or in an unnamed namespace was
             never given a definition.  For ordinary nontemplate functions we
             check the corresponding sk_extern_routine symbol; for template
             instances there is no such symbol and hence we check it here. */
          a_template_instance_ptr  tip = sym->variant.routine.instance_ptr;
          /* If an attempt was made to explicitly instantiate the function
             template, an error will have been issued already. */
          if (tip != NULL && !tip->explicit_instantiation) {
            an_error_severity severity = es_discretionary_error;
            if (gnu_mode || microsoft_mode) {
              severity = es_warning;
            }  /* if */
            pos_sy_diagnostic(severity, ec_never_defined, &sym->decl_position,
                              sym);
          }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
        } else if (ms_extensions &&
                   (rout_ptr->decl_modifiers & DM_DLLIMPORT)) {
          /* No diagnostic. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        } else if (!C_mode() && rout_ptr->is_inline &&
                   !routine_defined(rout_ptr)) {
          /* An extern-inline function that was referenced but not defined.
             Note that the Microsoft and GNU compilers accept such code
             (though linker errors may result from this). */
          if (scope_kind == (a_scope_kind)sck_namespace ||
              scope_kind == (a_scope_kind)sck_file) {
            /* The diagnostic is not issued for block extern declarations. */
            pos_sy_diagnostic((microsoft_mode || gnu_mode || sun_mode)
                                                       ? es_warning : es_error,
                              ec_extern_inline_never_defined,
                              &sym->decl_position, sym);
          }  /* if */
        }  /* if */
      } else if (!sym->referenced) {
        /* Unreferenced function. */
        if (storage_class == (a_storage_class)sc_extern &&
            !is_member_of_unnamed_namespace(&rout_ptr->source_corresp)) {
          /* No warning on unused "extern" routines; this is a long-standing
             C tradition.  An exception are routines in unnamed namespaces. */
        } else if (rout_ptr->is_inline && sym->defined &&
                   seq_is_in_include_file(sym->decl_position.seq)) {
          /* No diagnostic on inline non-member functions defined in a header
             file. */
        } else if (rout_ptr->source_corresp.maybe_unused
#if GNU_EXTENSIONS_ALLOWED
                   || rout_ptr->has_gnu_used_attribute
                   || rout_ptr->is_weakref
#endif /* GNU_EXTENSIONS_ALLOWED */
                                          ) {
          /* Do not diagnose an unused function that carries the "maybe_unused"
             standard attribute, GNU "unused", or "used" attributes.
             Similarly, do not diagnose weakref functions. */
#if ASM_FUNCTION_ALLOWED
        } else if (storage_class == (a_storage_class)sc_asm) {
          /* "asm" functions don't generate any code unless referenced,
             and may appear in header files, so no warning is generated. */
#endif /* ASM_FUNCTION_ALLOWED */
        } else if (il_header.any_templates_seen &&
                   (!nonclass_prototype_instantiations ||
                    defer_function_prototype_instantiations ||
                    sym->header->any_function_referenced_in_dependent_call) &&
                   instantiation_mode == tim_none &&
                   (scope_kind == (a_scope_kind)sck_file ||
                    scope_kind == (a_scope_kind)sck_namespace ||
                    scope_kind == (a_scope_kind)sck_class_struct_union)) {
          /* If we are not parsing all template definitions nor instantiating
             all template uses, it is possible that we missed a reference/use
             from a template to an enclosing scope (which can only be a file
             scope, namespace scope, or class scope): Don't issue a warning or
             remark in such cases since it would be unreliable.  Even if we
             are parsing template definitions, don't warn on the use of a
             name that was referenced in a dependent call if we are not
             instantiating all uses. */
        } else if (scope_stack[depth_scope_stack].in_prototype_instantiation &&
                   (scope_kind == (a_scope_kind)sck_function ||
                    scope_kind == (a_scope_kind)sck_block)) {
          /* A local function declaration in a prototype instantiation.  If it
             is a nondependent declaration, it could potentially have internal
             linkage, but the "referenced" flag in prototype instantiation
             contexts is unreliable since overload resolution cannot be done
             until a real instantiation.  So don't issue a diagnostic here. */
        } else if (rout_ptr->is_deleted) {
          /* Functions defined with "= delete" are never meant to be
             referenced.  Don't warn about them being unreferenced. */
        } else {
          /* An unreferenced routine. */
          report_unreferenced(sym, ec_declared_but_not_referenced,
			      es_warning);
        }  /* if */
      }  /* if */
      /* Check if this routine was declared using a type with no linkage.  The
         check is done for routines that are referenced but not defined.
         The not_needed_or_will_be_instantiated check is used so that a
         template that could be instantiated is considered defined. 
         (Microsoft compilers do not impose this restriction.) */
      if (decls_using_types_without_linkage_allowed &&
          sym->referenced &&
          (storage_class == (a_storage_class)sc_extern &&
           (!rout_ptr->is_template_function ||
            ((scope_kind == (a_scope_kind)sck_file ||
             (scope_kind == (a_scope_kind)sck_namespace)) &&
            !not_needed_or_will_be_instantiated(sym))))) {
        /* Check if this routine was declared using a type with no
           linkage. */
        check_constituent_types_have_linkage(sym, &sym->decl_position,
                                             /*is_declaration=*/FALSE);
      }  /* if */
#if CHECKING
      scp = &rout_ptr->source_corresp;
#endif /* CHECKING */
      break;
    case sk_label:
      /* Label. */
      if (sym->variant.label.ptr->exec_stmt == NULL) {
        if (!sym->defined) {
          /* A label that was used but never defined. */
          pos_sy_error(ec_never_defined, &sym->decl_position, sym);
        }  /* if */
      } else if (!sym->referenced) {
        /* An unreferenced label. */
        if (sym->variant.label.ptr->source_corresp.maybe_unused) {
          /* This label was explicitly marked as not being used (by the
             GNU "unused" attribute). */
        } else {
          report_unreferenced(sym, ec_declared_but_not_referenced,
                              es_warning);
        }  /* if */
      }  /* if */
      break;
    case sk_extern_variable:
      /* Symbol for a variable with linkage. */
      var_ptr = sym->variant.extern_symbol_descr->variant.variable;
      storage_class = var_ptr->storage_class;
      var_type = skip_typerefs(var_ptr->type);
      /* Look for variables that have retained an incomplete type
         that isn't just plain "void". */
      if (is_completable_type(var_type)) {
        if ((storage_class == (a_storage_class)sc_unspecified ||
             (C_dialect != C_dialect_cplusplus &&
              storage_class == (a_storage_class)sc_static)) &&
            is_array_with_complete_element_type(var_type)) {
          /* A file-scope incomplete array with no storage class, for example
             "int a[];" at file scope; or else (in C mode) a file-scope
             incomplete array with static storage class.  Such an array is
             defined by the standard (3.7.2 semantics) to be equivalent
             to "int a[] = {0};"; change the size to 1 here.  However, if
             the extern_variable entry has more complete type information
             (i.e., an exact dimension), use that.  Note that no explicit
             check for scope depth is required since sk_extern_variable
             symbols are generated for file-scope variables only. */
          /* The test for complete element type disallows arrays of
             incomplete struct/unions (which are an extension). */
          if (!is_incomplete_type(sym->variant.extern_symbol_descr->type)){
            /* The external symbol entry has a dimension for the array.
               Use it.  This would happen for
                 int a[];
                 main () {extern int a[5];}
            */
            var_ptr->type = sym->variant.extern_symbol_descr->type;
          } else {
            /* There is no additional information; the array has an
               unknown size. */
            if (C_dialect != C_dialect_pcc ||
                storage_class == (a_storage_class)sc_static) {
              /* Change the array size to 1. */
              a_type_ptr array_type = alloc_type((a_type_kind)tk_array);
              copy_type(var_type, array_type);
              check_assertion(!has_unknown_specified_bound(array_type));
              array_type->variant.array.variant.number_of_elements = 1;
              set_type_size(array_type);
              var_ptr->type = array_type;
              pos_sy_remark(ec_array_size_one_assumed, &sym->decl_position,
                            sym);
              /* No need to call check_linked_entity_type here.  We
                 know elem[] and elem[1] are compatible. */
            } else {
              /* pcc mode.  Leave the size as zero but change the
                 storage class to extern. */
              var_ptr->storage_class = (a_storage_class)sc_extern;
            }  /* if */
          }  /* if */
        } else if (storage_class != (a_storage_class)sc_extern) {
          /* A namespace-scope variable that defines storage and has
             an incomplete type, as in "struct incomplete v;".  Issue an
             error.  Note that arrays like this, except for static arrays,
             were handled above. */
          an_error_severity sev = es_discretionary_error;
          if ((clangcpp_version_is(any_version) ||
               gpp_version_is(any_version)) &&
              is_array_with_complete_element_type(var_type) &&
              is_member_of_unnamed_namespace(&var_ptr->source_corresp)) {
            /* Clang and GCC accept:
                 namespace {
                   extern int x[];
                   int f() { return x[0]; }
                 }
               even if x is never defined. */
            sev = es_warning;
          }  /* if */
          pos_st_diagnostic(sev, ec_var_retained_incomp_type,
                            &sym->decl_position, sym->header->identifier);
        }  /* if */
      }  /* if */
      /* If the storage class remains sc_unspecified, set the
         referenced flag now to indicate possible references from other
         compilation units. */
      if (var_ptr->storage_class == (a_storage_class)sc_unspecified) {
        var_ptr->source_corresp.referenced = TRUE;
      }  /* if */
      break;
    case sk_extern_routine:
       /* Check for a routine with internal linkage (or from an unnamed
          namespace) that is referenced but not defined (3.7, constraints).
          This is checked only at the file scope because there can be symbols
          with linkage defined in inner scopes, but only the file scope
          declaration can have a body.
          The template instance case is handled under sk_routine because no
          sk_extern_routine symbol is created for those.  For nontemplates the
          sk_routine symbol might have been removed too early in the case of a
          block-scope declaration in an unnamed namespace. */
      rout_ptr = sym->variant.extern_symbol_descr->variant.routine.ptr;
      if (rout_ptr->source_corresp.referenced) {
        /* Referenced function.  We check the IL referenced flag because
           a reference in, say, a sizeof operation doesn't count. */
        if ((rout_ptr->storage_class == (a_storage_class)sc_static ||
             (is_member_of_unnamed_namespace(&rout_ptr->source_corresp) &&
              rout_ptr->source_corresp.name_linkage !=
                                         (a_name_linkage_kind)nlk_external)) &&
             !routine_defined(rout_ptr)) {
#if GNU_EXTENSIONS_ALLOWED
          if (rout_ptr->is_weakref ||
              (has_gnu_routine_supp(rout_ptr) &&
               gnu_routine_supp(rout_ptr)->aliased_routine != NULL)) {
            /* GNU weakref entities have no definition.  Static routine
               aliases may alias extern routines, which are presumably
               defined in other translation units.  (If they alias another
               static routine, any needed diagnostic will be issued on the
               aliased routine.) */
          } else
#endif /* GNU_EXTENSIONS_ALLOWED */
          /* Do not insert code here. */
          if (C_dialect == C_dialect_pcc || gcc_mode) {
            /* In pcc mode, just change the routine to extern.  Do the same
               in GNU C mode, but issue a warning too in that case. */
            rout_ptr->storage_class = (a_storage_class)sc_extern;
            rout_ptr->source_corresp.name_linkage =
                                         (a_name_linkage_kind)nlk_external;
            if (gcc_mode) {
              pos_sy_warning(ec_undefined_static_function_treated_as_extern,
                             &sym->decl_position, sym);
            }  /* if */
          } else {
            /* g++ 4.4 and later only warn about this. */
            an_error_severity  sev = (gpp_mode && gnu_version >= 40400) ?
                                          es_warning : es_discretionary_error;
            pos_sy_diagnostic(sev, ec_never_defined, &sym->decl_position, sym);
          }  /* if */
        }  /* if */
      }  /* if */
      break;
#if CHECKING
    case sk_static_data_member:
      scp = &sym->variant.static_data_member.variable->source_corresp;
      break;
    case sk_constant:
      scp = &sym->variant.constant->source_corresp;
      break;
    case sk_field:
      if (sym->variant.field.anonymous_parent_object == NULL) {
        scp = &sym->variant.field.ptr->source_corresp;
      } else {
        a_symbol_ptr  apo_sym;

        for (apo_sym = sym->variant.field.anonymous_parent_object;
             apo_sym->kind == (a_symbol_kind)sk_field;
             apo_sym = apo_sym->variant.field.anonymous_parent_object) {
          if (apo_sym->variant.field.anonymous_parent_object == NULL) {
            scp = &apo_sym->variant.field.ptr->source_corresp;
            break;
          }  /* if */
        }  /* for */
      }  /* if */
      break;
    case sk_type:
      scp = &sym->variant.type.ptr->source_corresp;
      break;
    case sk_enum_tag:
      /* Struct, union, or enum tag. */
      scp = &type_symbol_type(sym)->source_corresp;
      break;
#endif /* CHECKING */
    case sk_class_or_struct_tag:
    case sk_union_tag:
      /* Class members require special end-of-scope processing.  This is
         only done for classes at namespace scope. */
      if (scope_kind == (a_scope_kind)sck_namespace ||
          scope_kind == (a_scope_kind)sck_file) {
         end_of_scope_symbol_check_for_class(sym, scope_kind);
      }  /* if */
#if CHECKING
      scp = &type_symbol_type(sym)->source_corresp;
#endif /* CHECKING */
      break;
    case sk_class_template:
      {
      a_template_symbol_supplement_ptr  tssp;
      a_symbol_ptr                      template_class_sym;
      a_symbol_list_entry_ptr           slep;
      tssp = sym->variant.template_info;
      /* Only check instances of class templates, not alias templates. */
      if (!tssp->variant.class_template.is_alias_template) {
        for (slep = tssp->variant.class_template.instantiations;
             slep != NULL; slep = slep->next) {
          template_class_sym = slep->symbol;
          if (template_class_sym->variant.class_struct_union.type->
                                variant.class_struct_union.is_nonreal_class) {
            /* Skip the recursive check for prototype instantiation of a class
               template. */
          } else {
            end_of_scope_symbol_check(template_class_sym, scope_kind,
                                      curr_routine);
          }  /* if */
        }  /* for */
      }  /* if */
#if DEBUG
      if (db_flag_is_set("hash_stats")) {
        a_hash_table_ptr	htp;
        /* Display statistics about any templates with a large number of
           instances. */
        htp = tssp->instantiation_hash_table;
        if (htp != NULL && htp->num_buckets > 100) {
          fprintf(f_debug, "Hash statistics for: ");
          db_symbol_name(sym);
          fprintf(f_debug, "\n");
          db_hash_statistics(htp);
        }  /* if */
      }  /* if */
#endif /* DEBUG */
      }
      break;
    case sk_function_template:
      {
      a_template_instance_ptr  tip;

      tip = sym->variant.template_info->variant.function.instantiations;
      for (; tip != NULL; tip = tip->next) {
        if (tip->is_guiding_decl) {
          /* A user declaration was provided, so the associated symbol should
             be on the overload list -- ignore it here. */
        } else {
          end_of_scope_symbol_check(tip->instance_sym, scope_kind,
                                    curr_routine);
        }  /* if */
      }  /* for */
      }
      break;
    default:
      /* No processing for other kinds. */
      break;
  }  /* switch */
#if CHECKING
  if (scp != NULL) {
    if (sym->is_class_member == scp->is_class_member &&
        (!sym->is_class_member ||
         sym_parent_class(sym) == scp_parent_class(scp))) {
      /* Okay */
    } else if (scp->is_class_member &&
               !has_name_before_mangling(scp_parent_class(scp))) {
      /* Okay: probably a member of a possibly nonstandard anonymous union. */
    } else if (sym->kind == (a_symbol_kind)sk_type &&
               sym->variant.type.is_injected_class_name) {
      /* The symbol for the injected class name points to the class of which
         it's a member, which is also its parent. */
    } else {
      unexpected_condition_str2("end_of_scope_symbol_check:",
                                "sym/il-entry parent-class mismatch");
    }  /* if */
  }  /* if */
#endif /* if */
}  /* end_of_scope_symbol_check */


/*
Available list of entries of type a_name_hidden_by_old_for_init.
*/
STATIC_THREAD a_name_hidden_by_old_for_init_ptr
		avail_names_hidden_by_old_for_init;


static 
a_name_hidden_by_old_for_init_ptr alloc_name_hidden_by_old_for_init(void)
/*
Return an entry of type a_name_hidden_by_old_for_init, with its fields
cleared, either allocating a brand new one or taking one from the available
list.
*/
{
  a_name_hidden_by_old_for_init_ptr  nhp;

  if (avail_names_hidden_by_old_for_init == NULL) {
    nhp = (a_name_hidden_by_old_for_init_ptr)
                            alloc_fe(sizeof(a_name_hidden_by_old_for_init));
  } else {
    nhp = avail_names_hidden_by_old_for_init;
    avail_names_hidden_by_old_for_init = nhp->next;
  }  /* if */
  nhp->next = NULL;
  nhp->symbol = NULL;
  nhp->for_init_decl_sym = NULL;
  nhp->already_hidden = FALSE;
  return nhp;
}  /* alloc_name_hidden_by_old_for_init */


static void free_names_hidden_by_old_for_init(
                                      a_name_hidden_by_old_for_init_ptr  nhp)
/*
Return each of the entries of type a_name_hidden_by_old_for_init on the list
headed by nhp to the available list.  Update the hidden_by_old_for_init
flag in the associated symbol.
*/
{
  a_name_hidden_by_old_for_init_ptr  next_nhp;

  while (nhp != NULL) {
    next_nhp = nhp->next;
    /* Update the hidden_by_old_for_init flag in the associated symbol. */
    nhp->symbol->hidden_by_old_for_init = nhp->already_hidden;
    nhp->symbol = NULL;
    nhp->for_init_decl_sym = NULL;
    /* Return the entry to the available list. */
    nhp->next = avail_names_hidden_by_old_for_init;
    avail_names_hidden_by_old_for_init = nhp;
    nhp = next_nhp;
  }  /* while */
}  /* free_names_hidden_by_old_for_init */


static void record_names_hidden_by_old_for_init(a_symbol_ptr for_init_decl)
/*
for_init_decl is head of a list of symbols declared in a for-init block scope
that has just been popped of the scope stack -- the scoping rules governing
these symbols are the standard rules.  build a list of names that would have
been hidden in the current scope under the old rules but are visible under
the new rules.  This list can be used to issue diagnostics -- to avoid
silently giving programs different behavior than they had under the old
(cfront-style) rules.
*/
{
  a_scope_stack_entry_ptr            ssep = &scope_stack[depth_scope_stack];
  a_symbol_ptr                       previously_hidden_sym;
  a_symbol_locator                   locator;
  a_name_hidden_by_old_for_init_ptr  nhp;

  check_assertion(ssep->kind == (a_scope_kind)sck_block ||
                  ssep->kind == (a_scope_kind)sck_function);
  while (for_init_decl != NULL) {
    /* Manufacture a locator to do a lookup. */
    make_locator_for_symbol(for_init_decl, &locator);
    clear_specific_symbol(locator);
    locator.specific_symbol = NULL;
    previously_hidden_sym = normal_id_lookup(&locator, IDL_NO_OPTIONS);
    if (previously_hidden_sym != NULL) {
      /* Lookup was successful. */
      if (previously_hidden_sym->decl_scope == ssep->number) {
        /* Ignore symbols found in the current scope. */
      } else {
        /* A symbol from a surrounding scope.  Be sure it hasn't already been
           entered. */
        nhp = ssep->names_hidden_by_old_for_init;
        for (; nhp != NULL; nhp = nhp->next) {
          if (nhp->symbol == previously_hidden_sym) break;
        }  /* for */
        if (nhp == NULL) {
          /* Create a name-[would-have-been-]hidden-by-old-for-init entry. */
          nhp = alloc_name_hidden_by_old_for_init();
          nhp->symbol = previously_hidden_sym;
          nhp->for_init_decl_sym = for_init_decl;
          /* Save the flag in the symbol so it can be restored later. */
          nhp->already_hidden = previously_hidden_sym->hidden_by_old_for_init;
          previously_hidden_sym->hidden_by_old_for_init = TRUE;
          /* Add the entry to the list for the current scope. */
          nhp->next = ssep->names_hidden_by_old_for_init;
          ssep->names_hidden_by_old_for_init = nhp;
        }  /* if */
      }  /* if */
    }  /* if */
    for_init_decl = for_init_decl->next_in_scope;
  }  /* while */
}  /* record_names_hidden_by_old_for_init */


void report_for_init_difference(a_symbol_ptr       sym,
                                a_source_position  *pos)
/*
The new for-init declaration scoping rules are in effect, and sym is a symbol
that (possibly) would not have been found with the old (cfront-compatible)
scoping rules.  If that's the case, issue a warning (so that users will not
be bitten by silent changes in how their programs behave).  This routine is
called only if global variable warning_on_for_init_difference is TRUE.
*/
{
  a_scope_depth                      depth = depth_scope_stack;
  a_scope_stack_entry_ptr            ssep;
  a_name_hidden_by_old_for_init_ptr  nhp;

  /* The outer loop marches up the scope stack until a match is found in
     a names-hidden-by-old-for-init list. */
  for (;;) {
    ssep = &scope_stack[depth];
    /* The inner loop goes through a list for a given scope. */
    nhp = ssep->names_hidden_by_old_for_init;
    for (; nhp != NULL; nhp = nhp->next) {
      if (nhp->symbol == sym) {
        /* A match is found. */
        break;
      }  /* if */
    }  /* for */
    /* If either a match was found or we've reached the end of the stack,
       stop looping. */
    if (nhp != NULL || depth == DEPTH_OF_FILE_SCOPE) break;
    /* Otherwise, continue though the scope stack. */
    depth = ssep->previous_scope;
  }  /* for */
  if (nhp != NULL) {
    /* Issue the diagnostic. */
    pos_sy2_warning(ec_hidden_by_old_for_init, pos, sym,
                    nhp->for_init_decl_sym);
#if CHECKING
  } else {
    /* No entry was found.  This should only happen inside a template
       instantiation, since the hidden_by_old_for_init flag in sym is not
       necessarily reliable.  Here's an example:
         int i = 13;
         template <class T> int g(T t) {
           return i;
         }
         main() {
           for (int i = 0; i < 10; ++i) { ... }
           (void)g(0);
         }
       After the for-loop, ::i has hidden_by_old_for_init set to TRUE, but
       that has no effect within the instantiation of g. */
    check_assertion_str(depth_innermost_instantiation_scope != NO_SCOPE_DEPTH,
                        "report_for_init_difference: entry not found");
#endif /* CHECKING */
  }  /* if */
}  /* report_for_init_difference */


void report_excessive_rescan_depth(void)
/*
A template has been instantiated to many times in the process of deduction.
Issue an error, and record the excess in the scope stack to allow earlier
levels of deduction to be cut short (not doing so opens the door to the
possibility of reaching this point again repeatedly for the same root-level
deduction, potentially entering an exponential-time process).
*/
{
  a_scope_depth  sd = depth_innermost_instantiation_scope;

  if (sd != NO_SCOPE_DEPTH && scope_stack[sd].rescan_depth_exceeded) {
    /* This routine has already been called for the current instantiation
       scope.  No need to issue multiple errors. */
  } else {
    /* Scan the scope stack for consecutive rescan instantiation entries,
       marking each one with a flag that indicates the rescan depth limit was
       exceeded.  Also mark those instantiation scope entries so that they
       will appear in the diagnostic we are about to emit (ordinarily, rescan
       entries are not reported when listing the instantiation context, but
       this is the exception to that rule). */
    while (sd != NO_SCOPE_DEPTH && scope_stack[sd].is_rescan) {
      scope_stack[sd].rescan_depth_exceeded = TRUE;
      scope_stack[sd].exclude_from_context_output = FALSE;
      sd = scope_stack[sd-1].depth_innermost_instantiation_scope;
    }  /* while */
    pos_error(ec_excessive_rescan_depth, &error_position);
  }  /* if */
}  /* report_excessive_rescan_depth */


static void nested_class_anachronism_processing(a_symbol_ptr symbol_list,
                                                a_boolean    do_tags,
                                                a_boolean    do_typedefs)
/*
This routine is called to process the symbols of a class scope to
determine whether any nested types should be visible for
nested class anachronism processing.  It is also used in
cfront 2.1 object compatibility mode to determine whether any of
the symbols should receive special treatment when generating
the mangled named for the nested type.  do_tags is TRUE if tag symbols
should be processed, otherwise they should be ignored.  do_typedefs
is TRUE if typedef symbols should be processed.  Both may be TRUE.
When cfront 2.1 object compatibility and cfront 2.1 mode are being used,
this routine is called twice.  Tag symbols are processed the first time,
and typedefs the second time.
*/
{
  a_symbol_ptr	sym;

  for (sym = symbol_list; sym != NULL; sym = sym->next_in_scope) {
    /* Check for nested class/struct/unions on the inactive list.  If
       there are any, set the flag in the symbol header.  This is
       used to support the nonnested class anachronism.  We do not
       apply the anachronism to template classes. */
    if (((do_tags && is_tag_symbol(sym)) ||
         (do_typedefs && sym->kind == (a_symbol_kind)sk_type)) &&
         !is_injected_class_symbol(sym)) {
      sym->header->any_nested_types_on_inactive_list = TRUE;
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
      /* Cfront 2.1 implements a special "transitional model" for nested
         types.  Under this model cfront promotes nested types to the file
         scope unless a file scope type of the same name is already
         defined.  Subsequent definition of additional nested types with
         the same name is an error.  This code, which simulates the
         cfront behavior, sets a flag for the first nested type with
         a given name and issues errors on subsequent definitions. */
      if (cfront_2_1_mode) {
        /* Only do this if the name is not a type name at file
           scope. */
        if (!check_for_file_scope_type_with_same_name(sym)) {
          if (!sym->header->has_cfront_transitional_nested_type_mangled_name) {
            a_type_ptr   sym_type;
            sym_type = type_symbol_type(sym);
            sym->header->
                       has_cfront_transitional_nested_type_mangled_name = TRUE;
            sym_type->use_cfront_transitional_nested_type_name_mangling = TRUE;
          } else {
            a_symbol_ptr other_sym;
            other_sym = find_cfront_transitional_nested_type_symbol(sym);
            if (other_sym != NULL) {
              /* other_sym can be NULL in certain cases where the transitional
                 nested type flag has already been set for another symbol
                 in the same class.  This can occur for cases like
                   typedef class {} A;
                 which results in two symbols being created in cfront mode. */
              pos_sy2_error(ec_cfront_multiple_nested_types,
                            &sym->decl_position, sym, other_sym);
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
    }  /* if */
  }  /* for */
}  /* nested_class_anachronism_processing */


static void do_nested_class_anachronism_processing(a_symbol_ptr symbol_list)
/*
This is an interface routine to nested_class_anachronism_processing.
When generating cfront 2.1 compatible object code, and in cfront 2.1 mode,
the processing is done in two passes: first any tag symbols are processed,
then any typedef symbols.  This ensures that if both a tag and a typedef
have the same name, the tag will receive the special transitional
nested type name mangling.

In other modes, nested_class_anachronism_processing is only called once,
and all symbols are processed in that one pass.
*/
{
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
  a_boolean	separate_typedef_pass = cfront_2_1_mode;
  nested_class_anachronism_processing(symbol_list,
                                      /*do_tags=*/TRUE,
                                      !separate_typedef_pass);
  if (separate_typedef_pass) {
    nested_class_anachronism_processing(symbol_list,
                                        /*do_tags=*/FALSE,
                                        /*do_typedefs=*/TRUE);
  }  /* if */
#else /* !CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
  nested_class_anachronism_processing(symbol_list,
                                      /*do_tags=*/TRUE,
                                      /*do_typedefs=*/TRUE);
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
}  /* do_nested_class_anachronism_processing */


#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
static
void file_scope_transitional_nested_type_processing(a_symbol_ptr symbol_list)
/*
Look for file scope symbols that have the "cfront transitional" nested
type flag set.  This indicates that a file scope symbol with the same
name was defined after the nested class was seen.  This is an error
in cfront compatibility mode.
*/
{
  a_symbol_ptr	sym;
  for (sym = symbol_list; sym != NULL; sym = sym->next_in_scope) {
    if (sym->header->has_cfront_transitional_nested_type_mangled_name) {
      if (is_type_symbol(sym)) {
        a_type_ptr	sym_type = type_symbol_type(sym);
        if (sym_type->use_cfront_transitional_nested_type_name_mangling) {
          /* The symbol being popped is already designated as the
             transitional nested type, don't issue an error.  This can
             occur when the type is promoted out of an anonymous union and
             reentered on the active list at file scope. */
        } else {
          a_symbol_ptr other_sym;
          other_sym = find_cfront_transitional_nested_type_symbol(sym);
          pos_sy2_error(ec_cfront_global_defined_after_nested_type,
                        &sym->decl_position, sym, other_sym);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
}  /* file_scope_transitional_nested_type_processing */
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */


void wrapup_scope(a_scope_ptr			scope_ptr,
                  a_scope_kind			kind,
                  a_scope_pointers_block_ptr	pointers_block,
                  a_boolean 	                is_namespace_wrapup,
                  a_boolean 	                is_local_reactivation,
                  a_push_scope_options_set	options)

/*
Do the processing required when a scope is closed.  This includes
doing any necessary end-of-scope processing on the symbols from
the scope.  This routine does processing that is done for scopes as they
are popped off of the stack and also for namespace scopes and the end
of the translation unit.

scope_ptr points to the IL scope entry associated with this scope,
and may be NULL.  is_namespace_wrapup is TRUE if this call is
used to do the final namespace processing at the end of the translation
unit.  is_local_reactivation is TRUE for a reactivated local scope.
options is a set of option flags that specify additional information
about the scope being popped.
*/
{
  a_symbol_ptr	sym;
  a_symbol_ptr	symbol_list;
  a_symbol_ptr	synth_namespace_projection_symbols;

  db_enter(3, "wrapup_scope");
#if DEBUG
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (debug_level >= 3 || db_flag_is_set("dump_decl_pos_info")) {
    switch(kind) {
      case sck_func_prototype:
      case sck_namespace_extension:
      case sck_namespace_reactivation:
        break;
      case sck_namespace:
        if (!is_namespace_wrapup) break;
        FALLTHROUGH
      default:
        db_decl_pos_info_for_scope(scope_ptr, pointers_block);
    }  /* switch */
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#endif /* DEBUG */
  /* Save values from the pointers block that will be used later.  This is
     done to guard against the scope stack being reallocated (e.g., by
     hidden name processing) while this routine is processing, which could
     invalidate the pointers_block pointer.  Clear pointers_block to make
     sure it is not used later. */
  symbol_list = pointers_block->symbols;
  if (scope_ptr != NULL && is_local_scope_kind(kind) &&
      !is_local_reactivation) {
    /* When a local scope is popped, save the list of symbols from the
       scope. */
    scope_ptr->symbols = symbol_list;
  }  /* if */
  synth_namespace_projection_symbols =
                            pointers_block->synth_namespace_projection_symbols;
  pointers_block = NULL;
  /* Determine whether types defined in this scope should be handled
     as semivisible types.  Template classes and classes nested within
     template classes do not have this processing done. */
  if (allow_anachronisms &&
      depth_innermost_namespace_scope == DEPTH_OF_FILE_SCOPE) {
    a_boolean do_semivisible_type_processing = TRUE;
    /* Loop back through the scope stack until we find a scope that is
       not a class_struct_union scope or until we find a class_struct_union
       scope that is a template class_struct_union. */
    a_scope_depth sd = depth_scope_stack;
    for (; sd > DEPTH_OF_FILE_SCOPE; sd--) {
      a_scope_kind skind = scope_stack[sd].kind;
      if (skind != (a_scope_kind)sck_class_struct_union) break;
      if (is_template_class_type(scope_stack[sd].assoc_type)) {
        do_semivisible_type_processing = FALSE;
        break;
      }  /* if */
    }  /* for */
    if (kind == (a_scope_kind)sck_class_struct_union &&
        do_semivisible_type_processing) {
      /* Determine whether any of the symbols from this class scope should
         be treated as semivisible types. */
      do_nested_class_anachronism_processing(symbol_list);
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
    } else if (kind == (a_scope_kind)sck_file && cfront_2_1_mode &&
               is_namespace_wrapup) {
      /* See if any of the file scope symbols conflict with semivisible
         nested types. */
      file_scope_transitional_nested_type_processing(symbol_list);
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
    }  /* if */
  }  /* if */
  if (is_namespace_wrapup) {
#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
    if (kind == (a_scope_kind)sck_file) {
      /* The GNU alias or ifunc attribute can refer to names of entities before
         those entities are declared.  The actual IL connection is therefore
         set up when all the entities in a translation unit have been seen.
         This must occur before unneeded entities are determined. */
      process_alias_fixup_list(/*early_attr_resolution=*/FALSE);
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */
    if (secondary_translation_unit_seen()) {
      set_correspondence_of_unvisited_entries(scope_ptr);
    }  /* if */
  }  /* if */
#if RECORD_HIDDEN_NAMES_IN_IL
  if (!C_mode() && !is_at_least_one_error() && is_primary_translation_unit) {
    if (kind == (a_scope_kind)sck_function ||
        kind == (a_scope_kind)sck_block ||
        (kind == (a_scope_kind)sck_file && is_namespace_wrapup)) {
      /* Now that all declarations in the scope have been seen, check for
         name hiding.  The hidden name table assists the C++-generating back
         end to determine when to put out qualified names and elaborated
         type specifiers.  Note that namespace and class scopes are handled
         when the scopes in which they are directly nested are processed. */
      check_name_hiding_for_scope(scope_ptr);
    }  /* if */
  }  /* if */
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
  if (kind == (a_scope_kind)sck_namespace_extension ||
      kind == (a_scope_kind)sck_namespace_reactivation ||
      kind == (a_scope_kind)sck_module_decl_import) {
    /* Symbol processing is not done for namespace extension and
       reactivation scopes. */
  } else {
    a_boolean                is_prototype_instantiation = FALSE;
    a_routine_ptr            curr_routine = NULL;

    /* Check for prototype instantiation of a class template. */
    if (kind == (a_scope_kind)sck_function) {
      /* If the scope is for a routine, get a pointer to the routine. */
      curr_routine = scope_ptr->variant.routine.ptr;
    }  /* if */
    if (kind == (a_scope_kind)sck_class_struct_union && !C_mode() &&
        scope_ptr->variant.assoc_type->
                                 variant.class_struct_union.is_nonreal_class) {
      is_prototype_instantiation = TRUE;
    }  /* if */
    /* Remove the symbols declared in this scope from the symbol table.
       Check for unreferenced symbols, and issue warnings for those. */
    for (sym = symbol_list; sym != NULL; sym = sym->next_in_scope) {
#if DEBUG
      if (db_active && 
          (debug_level >= 3 ||
           db_flag_is_set("dump_symbols"))) {
        if ((kind != (a_scope_kind)sck_namespace &&
             kind != (a_scope_kind)sck_file) || is_namespace_wrapup) {
          if (sym == symbol_list) {
            fputs("Wrapping up ", f_debug);
            if (scope_ptr != NULL) {
              db_scope(scope_ptr);
            } else {
              (void)db_scope_kind(kind);
              (void)fprintf(f_debug, " scope");
            }  /* if */
            fputs(":\n", f_debug);
          }  /* if */
          db_symbol(sym, "", 2);
        }  /* if */
      }  /* if */
#endif /* DEBUG */
      if (kind == (a_scope_kind)sck_func_prototype && !is_tag_symbol(sym)) {
        /* Don't check on symbols entered in the scope of a function prototype.
           They will be reentered in the scope of the function and should be
           checked when the function scope is popped.  Tag symbols are
           checked because tags associated with incomplete types need to
           be put on the types list of the prototype scope. */
      } else if (is_prototype_instantiation) {
        /* Don't check on symbols entered in the scope of a class template
           prototype instantiation -- the information may not be complete. */
      } else if (kind == (a_scope_kind)sck_template_instantiation) {
        /* Don't check on symbols entered in instantiation scopes.  (These
           should only be injected friends of prototype instantiations.) */
      } else if (kind == (a_scope_kind)sck_namespace && !is_namespace_wrapup) {
        /* Don't check symbols in namespaces and namespace extensions because
           we don't have complete information yet.  This will be done at the
           end of the file scope. */
      } else if (!is_namespace_wrapup && is_class_struct_union_symbol(sym) &&
                 is_member_of_unnamed_namespace(
                                    &type_symbol_type(sym)->source_corresp)) {
        /* Same for unnamed namespace class members. */
      } else if (kind == (a_scope_kind)sck_file && !is_namespace_wrapup) {
        /* File scope symbols are not checked until the file scope is popped
           again after processing all translation units. */
      } else if (kind == (a_scope_kind)sck_block &&
                 (options & PS_NOT_FINAL_POP) != 0) {
        /* If a block scope is going to be reactivated, its symbols are not
           checked until the final pop of the block scope. */
      } else {
        end_of_scope_symbol_check(sym, kind, curr_routine);
#if RECORD_HIDDEN_NAMES_IN_IL
#if CHECKING
        if (!C_mode() && !is_at_least_one_error()) {
          if (!sym->is_error &&
              sym->kind != (a_symbol_kind)sk_undefined &&
              sym->kind != (a_symbol_kind)sk_macro) {
            if (is_tag_symbol(sym)) {
              check_assertion(sym->header->any_tag_decl);
            }  /* if */
            if (kind == (a_scope_kind)sck_file ||
                kind == (a_scope_kind)sck_namespace) {
              check_assertion(sym->header->
                                    any_decl_in_file_or_namespace_scope);
            }  /* if */
          }  /* if */
        }  /* if */
#endif /* CHECKING */
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
      }  /* if */
      if (sym->kind == (a_symbol_kind)sk_extern_variable ||
          sym->kind == (a_symbol_kind)sk_extern_routine) {
        /* Extern variable and routine symbols were not really entered into
           the symbol table proper, so don't try to remove them or add them to
           the inactive list. */
        continue;
      }  /* if */
      if ((kind == (a_scope_kind)sck_namespace ||
           kind == (a_scope_kind)sck_file) && is_namespace_wrapup) {
        /* File scope and namespace scope symbols were removed from the
           symbol table when the scope was first closed.  Don't do it again
           now. */
      } else if (symbol_is(sym, sk_field) &&
                 sym->variant.field.ptr->is_captured_this) {
        /* A field representing a captured "this" is on the scope list, but not
           in the symbol table. */
      } else {
        /* Remove the symbol from the symbol table.  This is not done for
           namespace extension scopes because symbols from namespace extension
           scopes are put directly on the inactive list.  The symbol list from
           the scope entry includes all symbols in the namespace, not only
           those added as a result of this extension.  (Note that symbols are
           not removed from the scope list.  This is because they must
           sometimes remain accessible and the scope list, saved away in some
           other data structure, is a convenient way to get at them again.) */
        unlink_symbol_from_symbol_table(sym);
      }  /* if */
      /* Put struct/union/class members, namespace members, and template
         parameters on the inactive list of the proper symbol header.  For
         namespace members, this only needs to be done for the initial
         definition.  When a namespace extension is done, the symbols are
         added directly to the inactive list. */
      if (kind == (a_scope_kind)sck_class_struct_union ||
          kind == (a_scope_kind)sck_enum ||
          ((kind == (a_scope_kind)sck_namespace ||
            kind == (a_scope_kind)sck_file) && !is_namespace_wrapup) ||
          kind == (a_scope_kind)sck_template_declaration) {
        add_symbol_to_inactive_list(sym);
      }  /* if */
    }  /* for */
    if (kind == (a_scope_kind)sck_namespace ||
        kind == (a_scope_kind)sck_file) {
      /* Synthesized namespace projections are not removed from file
         and namespace scopes because they may be reused if an extension
         scope is opened. */
    } else {
      /* Remove any synthesized namespace projection symbols from the
         others_symbols list of the symbol header. */
      for (sym = synth_namespace_projection_symbols;
           sym != NULL;
           sym = sym->next_in_scope) {
        a_symbol_ptr	prev_sym = NULL;
        a_symbol_ptr	other_sym;
        /* Symbols that are not reusable will not be on the other symbols
           list. */
        if (sym->do_not_reuse) continue;
        for (other_sym = sym->header->other_symbols;
             other_sym != NULL; other_sym = other_sym->next) {
          if (other_sym == sym) break;
          prev_sym = other_sym;
        }  /* for */
        check_assertion_str2(other_sym != NULL, "wrapup_scope:",
                             "synth sym not on other_symbols list");
        if (prev_sym == NULL) {
          /* The symbol is the first one on the list.  Remove it by setting
             the list to its next pointer. */
          sym->header->other_symbols = sym->next;
        } else {
          /* Remove the symbol from the linked list. */
          prev_sym->next = sym->next;
        }  /* if */
      }  /* for */
    }  /* if */
    if (C_dialect == C_dialect_cplusplus && scope_ptr != NULL) {
      if (kind == (a_scope_kind)sck_function ||
          kind == (a_scope_kind)sck_block ||
          (kind == (a_scope_kind)sck_file && is_namespace_wrapup) ||
          (kind == (a_scope_kind)sck_namespace && is_namespace_wrapup)) {
        /* Issue a diagnostic on non-extern member functions that have been
           referenced but not defined. */
        a_boolean  is_function_local = (kind == (a_scope_kind)sck_function ||
                                        kind == (a_scope_kind)sck_block);
        check_referenced_member_functions(scope_ptr, is_function_local,
                                          /*within_unnamed_class=*/FALSE);
      }  /* if */
    }  /* if */
  }  /* if */
  if (kind == (a_scope_kind)sck_namespace ||
      kind == (a_scope_kind)sck_namespace_extension ||
      kind == (a_scope_kind)sck_file ||
      kind == (a_scope_kind)sck_module_decl_import) {
    /* Move routine entries as needed (and remove placeholder entries). */
    perform_scheduled_routine_moves();
  }  /* if */
  db_exit();
}  /* wrapup_scope */


void wrapup_namespace_scopes(a_scope_ptr scope_ptr)
/*
Call wrapup_scope for any namespace scopes defined within the scope
pointed to by scope_ptr.
*/
{
  a_namespace_ptr	nsp = scope_ptr->namespaces;

  while (nsp != NULL) {
    if (!nsp->is_namespace_alias) {
      a_scope_pointers_block_ptr  pointers_block;
      a_scope_ptr                 assoc_scope = nsp->variant.assoc_scope;
      pointers_block = &symbol_supplement_for_namespace(nsp)->pointers_block;
      /* The value of the is_reactivation parameter is not significant
         for namespace scopes. */
      wrapup_scope(assoc_scope, assoc_scope->kind,
                   pointers_block, /*is_namespace_wrapup=*/TRUE,
                   /*is_reactivation=*/FALSE,
                   PS_NO_OPTIONS);
      /* Process any namespaces defined within this one. */
      wrapup_namespace_scopes(assoc_scope);
    }  /* if */
    nsp = nsp->next;
  }  /* while */
}  /* wrapup_namespace_scopes */

#if MAINTAIN_NEEDED_FLAGS

static a_boolean variable_needed_even_if_unreferenced(a_variable_ptr	var)
/*
Return TRUE if "var" is needed even if it is unreferenced (e.g., because
it is an external definition).
*/
{
  a_boolean		is_needed = FALSE;

  if (!var->is_template_variable) {
    /* A non-template variable. */
    if ((var->storage_class == (a_storage_class)sc_unspecified
#if DO_IL_LOWERING
         && !var->promoted_local_static
         && !var->is_optional_vtable
#endif /* DO_IL_LOWERING */
                                       ) ||
         var->init_kind == (an_init_kind)initk_dynamic) {
#if !GENERATE_EH_TABLES
      if (var->eff_class_typeinfo_var != NULL &&
          var->eff_class_typeinfo_var->storage_class ==
                                                  (a_storage_class)sc_extern) {
        /* This is a typeinfo variable for a pointer to a class type.  Such
           variables are always created with sc_unspecified storage while the
           typeinfo variables for the class are created with sc_extern
           storage and are only changed to sc_unspecified if it has been
           determined that the typeinfo for the class should be defined.
           In this case, the typeinfo for the class is not defined, so suppress
           the typeinfo for the pointer to the class (otherwise there would be
           undefined references at link time). */
      } else
#endif /* !GENERATE_EH_TABLES */
      /* Do not insert code here. */
      {
        if (var->is_inline && var->is_constexpr) {
          /* The definition of an inline constexpr variable is only needed
             if it is odr-used.  If only its value is used in this
             translation unit, it is not needed here. */
        } else {
          /* This is an externally-linked non-inline variable that has been
             defined in this translation unit, or it is a variable local to
             this translation unit but with dynamic initialization, in
             which case it is treated as "needed" because the
             initialization may have side effects.  Mark it as needed
             now. */
          is_needed = TRUE;
        }  /* if */
      }  /* if */
#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
    } else if (var->section != NULL) {
      /* GNU C concatenates all variables in the same named section.
         This creates tables that can be accessed from any translation unit
         even though the individual entries may have had internal linkage. */
      is_needed = TRUE;
#endif /* GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
    } else if (var->storage_class == (a_storage_class)sc_static &&
               (var->has_gnu_used_attribute || var->is_weak)) {
      /* GNU C doesn't eliminate unreferenced static variables.  This front end
         may do so, but some attributes are taken as an indication that the
         entry should be kept. */
      is_needed = TRUE;
    } else if (var->aliased_variable != NULL &&
               var->storage_class == (a_storage_class)sc_extern) {
      /* The current variable is an alias for another variable.  Aliases
         usually have external linkage and are needed in those cases
         (elsewhere the entities they alias are also marked as needed). */
      is_needed = TRUE;
    } else if (var_is_gnu_named_register(var)) {
      /* The declaration of a namespace-scope variable mapped on a specific
         register using the GNU "asm(...)" construct must be preserved even
         if the variable is unused, because such a declaration reserves the
         associated register throughout the program. */
      is_needed = TRUE;
#endif /* GNU_EXTENSIONS_ALLOWED */
    }  /* if */
  } else {
    /* A template static data member. */
    if (!is_primary_translation_unit) {
      /* Assume that all static data members from secondary translation units
         are needed.  This will be reconsidered after the routine has
         been copied to the primary translation unit. */
      is_needed = TRUE;
    } else if (var->is_specialized) {
      /* A specialized static data member is always needed if it is defined. */
      is_needed = var->storage_class != (a_storage_class)sc_extern;
    } else {
      /* An instantiation of a static data member must be considered
         to be needed if it was automatically instantiated (i.e, it is
         in the instantiation request file for this compilation), or if
         it was explicitly instantiated.  For other cases, which include
         things instantiated in -tused mode and things adopted by this
         compilation, use the needed flag mechanism to determine whether
         the instantiation is really needed. */
      a_symbol_ptr		sym;
      a_template_instance_ptr	tip;
      a_master_instance_ptr	mip;

      sym = (a_symbol_ptr)var->source_corresp.assoc_info;
      check_assertion(sym != NULL);
      tip = template_instance_for_symbol(sym);
      check_assertion(tip != NULL);
      mip = tip->master_instance;
      if (tip->explicit_instantiation ||
	  (mip != NULL &&
	   (mip->automatically_instantiated && !mip->add_to_request_file))) {
        /* The instance exists as a result of an explicit instantiation
	   directive, or as a result of being assigned to this file by
	   the automatic instantiation mechanism.  The automatically
           instantiated flag will be set for adopted entities.  The test
           of add_to_request_file is used so that adopted entities will
           not necessarily be considered to be needed. */
        is_needed = TRUE;
      } else {
	is_needed = FALSE;
      }  /* if */
    }  /* if */
  }  /* if */
  return is_needed;
}  /* variable_needed_even_if_unreferenced */


static void set_needed_flags_for_typedefs(a_scope_ptr scope)
/*
Make a pass through the IL tree looking for typeref types that should be
marked as "needed" because they are involved in declarations of unnamed
classes -- e.g.,
  typedef struct { ... } S;
If struct S is needed, the typeref type that points to it and from which
it acquired its name should be marked as needed, too.  The reason this is
done separately from set_needed_flags_at_end_of_file_scope, and after it
is done, is that all the classes have to have been marked first.
*/
{
  a_type_ptr                   tp, under_type;
  a_namespace_ptr              nsp;

  for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
    if (!nsp->is_namespace_alias) {
      /* Nested namespace scope.  Apply the check to each of its types. */
      set_needed_flags_for_typedefs(nsp->variant.assoc_scope);
    }  /* if */
  }  /* for */
  for (tp = scope->types; tp != NULL; tp = tp->next) {
    if (tp->kind == (a_type_kind)tk_typeref) {
      /* If a typeref type points to a class that is needed, has a name, and
         was originally unnamed, the typeref type is needed, too. */
      under_type = tp->variant.typeref.type;
      if (is_immediate_class_type(under_type) &&
          under_type->variant.class_struct_union.originally_unnamed) {
        mark_as_needed_like((char *)tp, (an_il_entry_kind)iek_type,
                            &under_type->source_corresp,
                            /*set_class_defn_needed=*/FALSE);
      }  /* if */
    } else if (is_immediate_class_type(tp)) {
      a_class_type_supplement_ptr  ctsp = class_type_supp(tp);
      if (!scope_is_null_or_placeholder(ctsp->assoc_scope)) {
        /* Apply the check to each of the types defined in the class. */
        set_needed_flags_for_typedefs(ctsp->assoc_scope);
      }  /* if */
    }  /* if */
  }  /* for */
}  /* set_needed_flags_for_typedefs */


void set_needed_flags_at_end_of_file_scope(a_scope_ptr scope)
/*
scope is a pointer to the file scope, a namespace scope, or a class scope.
Set the "needed" flags on classes, variables, and static data members now
that processing for the file scope (including IL lowering, if applicable) has
been completed.
*/
{
  a_type_ptr                   tp;
  a_namespace_ptr              nsp;
  a_variable_ptr               vp;
  a_routine_ptr                rp;

  if (scope->kind == (a_scope_kind)sck_file) {
    /* Top-level call. */
#if DEBUG
    if (db_flag_is_set("needed_flags")) {
      fprintf(f_debug, "Start of set_needed_flags_at_end_of_file_scope\n");
    }  /* if */
#endif /* DEBUG */
    end_of_file_scope_needed_flags_phase = TRUE;
  } else {
    check_assertion_str2(scope->kind == (a_scope_kind)sck_namespace ||
                         scope->kind == (a_scope_kind)sck_class_struct_union,
                       "set_needed_flags_at_end_of_file_scope:",
                       "bad scope kind");
  }  /* if */
  /* Apply this check to namespaces defined in the current scope, if there
     are any. */
  for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
    if (!nsp->is_namespace_alias) {
      /* Nested namespace scope. */
      set_needed_flags_at_end_of_file_scope(nsp->variant.assoc_scope);
    }  /* if */
  }  /* for */
  /* Check for classes defined in the current scope. */
  for (tp = scope->types; tp != NULL; tp = tp->next) {
    if (is_immediate_class_type(tp)) {
      /* If the class has been marked to indicate that it is needed,
         then we need to walk the subtree of the class; if not, we can ignore
         it. */
      a_class_type_supplement_ptr  ctsp = class_type_supp(tp);
      remark_as_needed((char *)tp, (an_il_entry_kind)iek_type);
      if (!scope_is_null_or_placeholder(ctsp->assoc_scope)) {
        /* Check nested classes and static data members, too.  Note that this
           may be done even if the class itself is not needed. */
        set_needed_flags_at_end_of_file_scope(ctsp->assoc_scope);
      }  /* if */
    }  /* if */
  }  /* for */
  /* Do processing on variables (or, if this a class scope, static data
     members). */
  for (vp = scope->variables; vp != NULL; vp = vp->next) {
    a_boolean is_needed;
    /* Determine whether this variable should be considered "needed".  This
       is true for variables that are referenced and for most external
       definitions. */
    is_needed = (vp->source_corresp.needed ||
                 variable_needed_even_if_unreferenced(vp));
    if (is_needed) {
      mark_as_needed((char *)vp, (an_il_entry_kind)iek_variable);
    }  /* if */
    /* If the variable is marked as needed, remark it to visit its
       subtree.  The subtree is not visited until this phase, because it
       can change. */
    remark_as_needed((char *)vp, (an_il_entry_kind)iek_variable);
  }  /* for */
  for (rp = scope->routines; rp != NULL; rp = rp->next) {
    a_boolean saved_defined = rp->defined;
#if GNU_EXTENSIONS_ALLOWED
    if ((has_gnu_routine_supp(rp) &&
         gnu_routine_supp(rp)->aliased_routine != NULL) &&
        !rp->implicit_alias &&
        rp->storage_class != (a_storage_class)sc_static) {
      /* Routine aliases are needed because they may be accessed from other
         translation units. */
      mark_as_needed((char *)rp, (an_il_entry_kind)iek_routine);
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    /* If the routine is marked as needed, remark it to visit its
       subtree.  The subtree is not visited until this phase, because it
       can change.  Note that the subtree here is the function type,
       not the body, which is handled elsewhere. */
    /* If the "defined" flag is TRUE, the body will already have been
       walked to mark its constituents as needed; we clear the flag to
       keep it from being walked again. */
    rp->defined = FALSE;
    remark_as_needed((char *)rp, (an_il_entry_kind)iek_routine);
    /* Restore the "defined" flag. */
    rp->defined = saved_defined;
  }  /* for */
  if (scope->kind == (a_scope_kind)sck_file) {
    /* End of top-level call. */
    if (!C_mode()) {
      /* Check for cases like this:
           typedef struct { ... } T;
         where the struct has been marked as needed but the typedef has not.
         The typedef really is needed in some cases -- e.g., in producing the
         proper definition by the C++-generating back end.  Mark the typedef,
         too. */
      set_needed_flags_for_typedefs(scope);
    }  /* if */
#if DEBUG
    if (db_flag_is_set("needed_flags")) {
      fprintf(f_debug, "End of set_needed_flags_at_end_of_file_scope\n");
    }  /* if */
#endif /* DEBUG */
    end_of_file_scope_needed_flags_phase = FALSE;
  }  /* if */
}  /* set_needed_flags_at_end_of_file_scope */

#endif /* MAINTAIN_NEEDED_FLAGS */
#if MAINTAIN_NEEDED_FLAGS

static a_boolean routine_needed_even_if_unreferenced(a_routine_ptr rout)
/*
Return TRUE if the indicated routine is needed even if it is unreferenced,
e.g., because it's externally defined.
*/
{
  a_boolean is_needed = FALSE;

#if GNU_EXTENSIONS_ALLOWED
  if (rout->is_initialization_routine || rout->is_finalization_routine) {
    /* An initialization or finalization routine is always needed,
       even if not otherwise referenced, because it will be called
       at program startup. */
    is_needed = TRUE;
  } else if (rout->storage_class == (a_storage_class)sc_static &&
             (rout->has_gnu_used_attribute || rout->is_weak)) {
    /* GNU C doesn't eliminate unreferenced static functions.  This front end
       may do so, but some attributes are taken as an indication that the
       entry should be kept. */
    is_needed = TRUE;
  } else 
#endif /* GNU_EXTENSIONS_ALLOWED */
  /* Do not insert code here. */
  {
    /* Generally, externally-defined routines are needed, because they might
       be referenced from some other compilation unit. */
    if (rout->storage_class == (a_storage_class)sc_unspecified) {
      a_boolean unspecialized_template = (rout->is_template_function &&
                                          !rout->is_specialized);
      is_needed = TRUE;
      if (rout->is_trivial_default_constructor) {
	/* Trivial constructors have no bodies so are never needed. */
	is_needed = FALSE;
      } else if (rout->is_inline &&
                 !(unspecialized_template && !treat_as_static_inline(rout)) &&
#if MICROSOFT_EXTENSIONS_ALLOWED
                 (rout->decl_modifiers & DM_DLLEXPORT) == 0 &&
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
                 !(gcc_mode && !rout->definition_for_inlining_only) &&
#endif /* GNU_EXTENSIONS_ALLOWED */
		 !(c99_mode && !rout->definition_for_inlining_only)) {
        /* An exception is "extern inline" functions, which are not regarded
           as referenced from elsewhere.  Each compilation unit has its own
           copy, and this copy is needed only if it is referenced in this
           compilation unit.  In C99 mode, however, an out-of-line copy that
           can be referenced from somewhere else may have been generated (if
           there was also a non-inline declaration of the function).  In GCC
           mode, an inline function can be referenced from other compilation
           units unless it is explicitly declared "extern inline".  In
           Microsoft mode, dllexport routines should always be retained.  In
           addition, in C++ inline template functions should be emitted if
           they are explicitly instantiated except when inline functions are
           implemented using static functions.  (Note: The flag
           need_out_of_line_copy is sometimes set for routines that are not
           actually needed; it only indicates that if the routine is really
           needed, an out-of-line copy is required.) */
	is_needed = FALSE;
      } else if (!is_primary_translation_unit) {
        /* Assume that all external routines from secondary translation units
           are needed.  This will be reconsidered after the routine has
           been copied to the primary translation unit. */
      } else if (unspecialized_template) {
        /* An instantiation of an external template function must be considered
           to be needed if it was automatically instantiated (i.e, it is
           in the instantiation request file for this compilation), or if
           it was explicitly instantiated.  For other cases, which include
           things instantiated in -tused mode and things adopted by this
           compilation, use the needed flag mechanism to determine whether
           the instantiation is really needed. */
	a_symbol_ptr             rout_sym;
	a_template_instance_ptr  tip;
	a_master_instance_ptr	 mip;

	rout_sym = (a_symbol_ptr)rout->source_corresp.assoc_info;
	check_assertion(rout_sym != NULL);
	tip = rout_sym->variant.routine.instance_ptr;
	check_assertion(tip != NULL);
        mip = tip->master_instance;
	if (tip->explicit_instantiation ||
            instantiation_mode == tim_all ||
	    (mip != NULL &&
	     (mip->automatically_instantiated && !mip->add_to_request_file))) {
	  /* The instance exists as a result of an explicit instantiation
	     directive, or as a result of being assigned to this file by
	     the automatic instantiation mechanism.  The automatically
             instantiated flag will be set for adopted entities.  The test
             of add_to_request_file is used so that adopted entities will
             not necessarily be considered to be needed.  In -tall mode,
             instantiations should always be considered needed. */
	} else {
	  is_needed = FALSE;
	}  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return is_needed;
}  /* routine_needed_even_if_unreferenced */

#endif /* MAINTAIN_NEEDED_FLAGS */

a_boolean keep_function_body_for_possible_inlining(a_routine_ptr routine)
/*
Return TRUE if it's desirable to keep the body of the indicated
function around for possible use in inlining calls to it.  Also
keeps constexpr function bodies.
*/
{
  a_boolean keep = FALSE;

  if (routine->is_constexpr) {
    /* Unconditionally keep the bodies of constexpr functions. */
    keep = TRUE;
  } else if (routine->is_inline) {
#if MINIMAL_INLINING
    if (inlining_enabled) {
      keep = TRUE;
    }  /* if */
#endif /* MINIMAL_INLINING */
#if ONE_INSTANTIATION_PER_OBJECT
    if (one_instantiation_per_object) {
      /* In one-instantiation-per-object mode, keep an inline function
         around so that its body can be swept for each instantiation that
         needs it. */
      keep = TRUE;
    }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  }  /* if */
  return keep;
}  /* keep_function_body_for_possible_inlining */

  
static a_boolean function_body_should_be_discarded(a_routine_ptr routine)
/*
Return TRUE if the body of the indicated function should be discarded.
For example, the bodies of generated trivial default constructors are
discarded right after they have been generated.
*/
{
  a_boolean discard = FALSE;

  if (routine->is_trivial_default_constructor && !routine->is_defaulted) {
    /* Discard implicit trivial default constructors. */
    discard = TRUE;
  } else if (routine->is_prototype_instantiation) {
    /* Do not discard prototype instantiations because they might contain
       expressions (like requires-expressions) that must be substituted in
       corresponding real instantiations. */
    discard = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (ms_extensions && (routine->decl_modifiers & DM_DLLIMPORT) &&
             !routine->is_inline) {
    /* In Microsoft mode, routines marked __declspec(dllimport) can
       have bodies, which are discarded.  Inline function definitions are
       not discarded but their bodies should only be used for inlining. */
    discard = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else if (routine->contains_generic_lambda ||
             (routine->is_lambda_body && routine->is_template_function)) {
    /* Routines that are or contain generic lambdas may need to have their
       scopes reactivated for instantiations. */
    discard = FALSE;
  } else if (is_nontemplate_routine_from_exported_trans_unit(routine)) {
    /* This is a non-template or a specialization in a secondary translation
       unit that is being compiled only for its exported templates.
       Discard it. */
    discard = TRUE;
    if (keep_function_body_for_possible_inlining(routine)) {
      /* ... but keep inline functions so we can inline from them. */
      discard = FALSE;
    }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  } else if (routine->is_ifunc) {
    /* Routines that use the "ifunc" dispatch mechanism should not have a
       definition (they may have unwittingly obtained an empty one if they are
       instantiated). */
    discard = TRUE;
#endif /* GNU_EXTENSIONS_ALLOWED */
  }  /* if */
  if (!discard && routine->source_corresp.is_local_to_function) {
    /* Member functions of local classes must be discarded if the surrounding
       function is discarded. */
    a_scope_depth depth;
    /* Find the entry for "routine" in the scope stack.  The loop is
       necessary for recursive calls to this routine. */
    for (depth = depth_scope_stack;
         ;
         depth = scope_stack[depth].previous_scope) {
      check_assertion(depth != NO_SCOPE_DEPTH);
      if (scope_stack[depth].kind == (a_scope_kind)sck_function &&
          scope_stack[depth].assoc_routine == routine) break;
    }  /* for */
    check_assertion(depth != NO_SCOPE_DEPTH);
    /* Found the routine.  The next entry on the stack tells us the
       innermost function scope depth for the surrounding function, if any. */
    depth = scope_stack[depth].previous_scope;
    depth = scope_stack[depth].depth_innermost_function_scope;
    if (depth != NO_SCOPE_DEPTH) {
      /* This routine is inside another routine.  Do a recursive call to
         find out whether that routine is to be discarded. */
      a_routine_ptr encl_routine = scope_stack[depth].assoc_routine;
      if (function_body_should_be_discarded(encl_routine)) {
        discard = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return discard;
}  /* function_body_should_be_discarded */


static void finish_local_function_body_processing(a_scope_ptr sp)
/*
If the routine that has associated scope sp has any local functions
(e.g., members of local classes), do final processing on their bodies
as necessary.  This is to ensure they are processed before the surrounding
function.  (To give one example why this is needed, we want to lower the
constructor body for a local class before we lower the surrounding
function, so that we know the virtual function table is used
by the time we process it in the surrounding function.)  This routine
calls itself recursively, and on those calls sp will be a block scope.
*/
{
  a_type_ptr    type;
  a_routine_ptr routine;
  a_scope_ptr   block_scope;

  /* Visit all types, looking for local classes. */
  for (type = sp->types; type != NULL; type = type->next) {
    if (is_immediate_class_type(type)) {
      /* Visit the class scope if it has one. */
      a_scope_ptr class_scope = class_type_supp(type)->assoc_scope;
      if (!scope_is_null_or_placeholder(class_scope)) {
        /* Visit the member functions of the class. */
        for (routine = class_scope->routines;
             routine != NULL;
             routine = routine->next) {
          a_function_def_number fdn = routine->function_def_number;
          if (fdn != NULL_function_def_number) {
            finish_function_processing_for_function_def(fdn,
                                                        /*only_inline=*/FALSE);
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
  }  /* for */
  /* Visit all block scopes to look for local classes. */
  for (block_scope = sp->scopes;
       block_scope != NULL;
       block_scope = block_scope->next) {
    finish_local_function_body_processing(block_scope);
  }  /* for */
}  /* finish_local_function_body_processing */


/* Forward declaration. */
static void finish_function_body_processing(
                                a_scope_ptr scope,
                                a_boolean   will_discard_function_body,
                                a_boolean   delayed);


void finish_function_processing_for_function_def(
                                            a_function_def_number n,
                                            a_boolean             only_inline)
/*
If function definition n is a function for which body processing has
not been finished, finish it now.  If only_inline is TRUE, finish the
processing only if the function is inline.
*/
{
  if (il_header.function_def_table[n].scope == NULL) {
    /* The body of this function has been cleared. */
  } else if (mem_region_table[mem_region_for_function_def(n)] == NULL) {
    /* This memory has already been freed. */
  } else {
    a_scope_ptr sp = scope_for_function_def(n);
    if (sp->kind == (a_scope_kind)sck_function &&
        (!only_inline || sp->variant.routine.ptr->is_inline) &&
        !in_secondary_trans_unit(sp) &&
        !sp->function_body_processing_finished) {
      if (!C_mode() && sp->variant.routine.ptr->contains_local_class_type) {
        /* Ensure that the definitions of local functions are
           processed before the enclosing function. */
        finish_local_function_body_processing(sp);
      }  /* if */
      finish_function_body_processing(sp, /*discard_function_body=*/FALSE,
                                      /*delayed=*/TRUE);
    }  /* if */
  }  /* if */
}  /* finish_function_processing_for_function_def */


static void finish_function_body_processing(
                                        a_scope_ptr scope,
                                        a_boolean   will_discard_function_body,
                                        a_boolean   delayed)
/*
Do final processing on the body of the function with the indicated scope.
This includes IL lowering if appropriate.  This routine is called
immediately after the body is scanned, and also after the body is copied
over to the primary IL for functions in a secondary translation unit.
That is, for functions in a secondary translation unit it is called twice.
If will_discard_function_body is TRUE, the function body will be
thrown away by the caller.  If delayed is TRUE, this processing is
being done later than at pop_scope time for the function, and therefore
the scope stack is no longer available.
*/
{
  LOCAL_UNUSED a_routine_ptr routine = scope->variant.routine.ptr;
#if MAINTAIN_NEEDED_FLAGS
  a_boolean                  lowering_done = FALSE;
#endif /* MAINTAIN_NEEDED_FLAGS */

  db_enter(1, "finish_function_body_processing");
#if DEBUG
  if (db_flag_is_set("ffbp") || debug_level >= 1 ||
      db_has_traced_name(routine, iek_routine)) {
    fprintf(f_debug, "Finishing function body processing for ");
    db_name_full(&routine->source_corresp, iek_routine);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  check_assertion(!scope->function_body_processing_finished);
  /* Do not do lowering and related processing of functions in secondary
     translation units until they are copied to the primary IL.
     When this routine is called after copying for functions from
     secondary translation units, is_primary_translation_unit is TRUE. */
  if (is_primary_translation_unit) {
#if ONE_INSTANTIATION_PER_OBJECT
    set_routine_instantiation_needed_bit_number(routine);
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if DO_IL_LOWERING
    if (!will_discard_function_body &&
        !routine->is_prototype_instantiation &&
        depth_template_declaration_scope == NO_SCOPE_DEPTH &&
        (delayed ||
         !scope_stack[depth_scope_stack].in_prototype_instantiation)) {
      /* Do IL lowering (change the C++ IL into C IL).  Note that consteval
         functions are lowered to avoid problems with embedded non-consteval
         functions, such as non-consteval lambdas. */
      lower_function_scope(routine, scope);
#if MAINTAIN_NEEDED_FLAGS
      lowering_done = TRUE;
#endif /* MAINTAIN_NEEDED_FLAGS */
    }  /* if */
    if (il_lowering_needed()) {
      /* If we're not supposed to pass object lifetime information to the
         back end, unlink all object lifetimes from the IL tree. */
      clean_up_all_object_lifetimes(scope);
    }  /* if */
#endif /* DO_IL_LOWERING */
  }  /* if */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  if (!will_discard_function_body) {
    /* If a function or block scope has local types or static variables,
       make a special entry to record those orphan lists on the il_header
       scope_orphaned_list_headers list so they can be found when
       processing the file scope memory region.  Note that processing
       for block scopes is done at the end of the function scope to give
       IL lowering a chance to add variables and types in block scopes.
       Also note that for functions in secondary translation units
       the lists are generated anew after the function body is (lowered
       and) moved over, rather than copying the lists. */
    add_scope_orphaned_il_lists(scope);
  } else {
    eliminate_pragmas_for_file_scope_entities(scope);
  }  /* if */
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
  if (!delayed) {
    /* Clear out the shareable constants table for the function scope.
       In the delayed case, the clearing was done at pop_scope time and
       no sharing of function-scope constants was allowed during lowering. */
    empty_func_shareable_constants_table();
  }  /* if */
  scope->function_body_processing_finished = TRUE;
#if MAINTAIN_NEEDED_FLAGS
  { a_boolean is_needed;
    /* Walk subtrees of local types and variables that have already been
       marked as needed.  If they are not marked as needed yet, this
       allows the subtrees to be swept in the future because they
       can no longer change.*/
    walk_subtrees_of_local_entities(scope);
    if (routine->defined && lowering_done) { /*lint !e774*/
      /* If the definition_needed flag is set already, sweep the body
         now that lowering has been done. */
      remark_routine_definition_needed(routine);
    }  /* if */
    /* If the function is globally visible and presumably needed by code
       in another translation unit, set the "needed" flag on the function. */
    is_needed = (delayed ||
                 !scope_stack[depth_scope_stack].in_prototype_instantiation) &&
                 !routine->is_prototype_instantiation &&
                (routine->source_corresp.needed ||
                 routine_needed_even_if_unreferenced(routine));
    if (is_needed) {
      mark_as_needed((char *)routine, (an_il_entry_kind)iek_routine);
#if DEBUG
    } else if (debug_level >= 3) {
      fprintf(f_debug, "Not calling mark_as_needed for \"");
      db_name_full(&routine->source_corresp, iek_routine);
      fprintf(f_debug, "\", storage class is %s\n",
              db_storage_class_names[(int)routine->storage_class]);
#endif /* DEBUG */
    }  /* if */
  }
#endif /* MAINTAIN_NEEDED_FLAGS */
  db_exit();
}  /* finish_function_body_processing */

#if DO_IL_LOWERING
#if MODULE_ID_NEEDED && !STANDALONE_UTILITY_PROGRAM

/*
A list of functions that are waiting for a module id to be generated before
they can be lowered.  The list is kept in the order in which the functions
are originally processed (which is the order in which the functions are
subsequently lowered).
*/
typedef struct a_delayed_lowering_list_entry
                                            *a_delayed_lowering_list_entry_ptr;
typedef struct a_delayed_lowering_list_entry {
  a_delayed_lowering_list_entry_ptr
                next;   
                        /* Pointer to the next entry on the list (or NULL
                           if this is the last entry). */
  a_routine_ptr	routine;
                        /* The function whose lowering has been delayed. */
} a_delayed_lowering_list_entry;

STATIC_THREAD a_delayed_lowering_list_entry_ptr
                waiting_for_module_id_list_head;
                        /* The head of the list of functions whose lowering
                           has been delayed because no module id was
                           available. */

STATIC_THREAD a_delayed_lowering_list_entry_ptr
                waiting_for_module_id_list_tail;
                        /* The tail of the list of functions whose lowering
                           has been delayed because no module id was
                           available. */

#if DEBUG
STATIC_THREAD unsigned long
                num_delayed_lowering_list_entries_allocated;
#endif /* DEBUG */

void lower_functions_waiting_for_module_id(void)
/*
Finish function processing (which includes lowering) for all memory regions
whose lowering was delayed due to lack of a module id.  Process the functions
in the same order in which they were originally encountered.
*/
{
  check_assertion(!il_lowering_underway);
  for (; waiting_for_module_id_list_head != NULL;
         waiting_for_module_id_list_head =
                                       waiting_for_module_id_list_head->next) {
    a_routine_ptr		routine;
    routine = waiting_for_module_id_list_head->routine;
    check_assertion(scope_for_routine(routine)->kind ==
                    (a_scope_kind)sck_function);
    finish_function_processing_for_function_def(routine->function_def_number,
                                                /*only_inline=*/FALSE);
  }  /* for */
  waiting_for_module_id_list_tail = NULL;
}  /* lower_functions_waiting_for_module_id */

#endif /* MODULE_ID_NEEDED && !STANDALONE_UTILITY_PROGRAM */
#if NEED_NAME_MANGLING

static a_boolean ttt_has_parentless_lambda_in_default_argument(
                                                     a_type_ptr type,
                                                     a_boolean  *end_traversal)
/*
Return TRUE (and stop the type traversal) if the specified type (type) is
a lambda type that is defined in a default argument but its parent pointer
has not yet been set.
*/
{
  a_boolean result = FALSE;

  if (is_immediate_class_type(type) &&
      class_type_supp(type)
                    ->does_not_contain_parentless_lambda_in_default_argument) {
    /* Stop traversal as the class type has already been checked. */
    *end_traversal = TRUE;
  } else if (type_is_lambda_closure(type) &&
      symbol_supplement_for_class(type)->
                            lambda_immediately_inside_default_arg_expression &&
      class_type_supp(type)->lambda_parent.routine == NULL) {
    *end_traversal = TRUE;
    result = TRUE;
  }  /* if */
  return result;
}  /* ttt_has_parentless_lambda_in_default_argument */


static void ttt_post_has_parentless_lambda_in_default_argument(
                                                             a_type_ptr type,
                                                             a_boolean  result)
/*
This is a service function designed to be called from traverse_type_tree_full
as a post-order traversal function (whence the ttt_post_ prefix).  If type_ptr
is a class type it caches a negative result in its class type supplement.
*/
{
  if (is_immediate_class_type(type) && !result) {
    a_class_type_supplement_ptr  ctsp = class_type_supp(type);
    ctsp->does_not_contain_parentless_lambda_in_default_argument = TRUE;
  }  /* if */
}  /* ttt_post_has_parentless_lambda_in_default_argument */


static inline a_boolean has_parentless_lambda_in_default_argument(
                                                               a_type_ptr type)
/*
Traverse the type to see if any template arguments contain lambdas in
default arguments where the parent of the lambda has not yet been identified.
*/
{
  return traverse_type_tree_full(
                            type,
                            ttt_has_parentless_lambda_in_default_argument,
                            ttt_post_has_parentless_lambda_in_default_argument,
                            TTT_TEMPLATE_ARGS | TTT_SKIP_TYPEREFS);
}  /* has_parentless_lambda_in_default_argument */


static a_boolean must_wait_for_mangling_reasons(a_routine_ptr routine)
/*
Returns TRUE if the specified routine cannot be lowered at the present time
because a mangled name might be generated by this routine that depends on
information that is not known yet.
*/
{
  a_symbol_ptr  sym;
  a_type_ptr    type;
  a_scope_ptr   sp = scope_for_routine(routine);
  a_boolean     result = FALSE;
  a_boolean     requires_early_mangling = FALSE;
  a_boolean     has_base_classes = FALSE;

  if (sp->types != NULL || sp->scopes != NULL || sp->variables != NULL) {
    /* Entities promoted from within scopes may require mangling. */
    requires_early_mangling = TRUE;
#if IA64_ABI
  } else if (routine->special_kind ==
                                    (a_special_function_kind)sfk_constructor ||
             routine->special_kind ==
                                     (a_special_function_kind)sfk_destructor) {
    /* Creating alternate entry points generates mangled names. */
    requires_early_mangling = TRUE;
#endif /* IA64_ABI */
  }  /* if */
  for (sp = get_parent_scope_of(routine); sp != NULL; ) {
    if (sp->kind == (a_scope_kind)sck_class_struct_union ||
        sp->kind == (a_scope_kind)sck_class_reactivation) {
      type = sp->variant.assoc_type;
      sym = symbol_for(type);
      check_assertion(is_immediate_class_type(type) && sym != NULL);
      if (!has_base_classes &&
          type->variant.class_struct_union.extra_info->base_classes != NULL) {
        /* Set a flag if an intermediate class (between the original
           routine and some outer unnamed class) contains base classes.  Such
           classes may result in the need to pre-lower the class type which can
           result in requests to mangle the class (thereby requiring that a
           mangled name be available for the unnamed class).  */
        has_base_classes = TRUE;
      }  /* if */
      if ((requires_early_mangling ||
           has_base_classes) &&
          type->variant.class_struct_union.originally_unnamed &&
          sym->variant.class_struct_union.extra_info->discriminator == 0) {
        /* A promoted entity from this scope or pre-lowering of a class
           would need the discriminator information during mangling, so delay
           lowering. */
        result = TRUE;
        break;
      }  /* if */
      if (type_is_lambda_closure(type) &&
          class_type_supp(type)->defined_in_variable_initializer &&
          class_type_supp(type)->lambda_parent.variable->
                                                 is_struct_binding_container) {
        /* The mangled name for this lambda will depend on the names of
           binding variables and they aren't known yet. */
        result = TRUE;
        break;
      }  /* if */
      if (has_parentless_lambda_in_default_argument(type)) {
        /* The mangling for a lambda in a default argument needs a parent
           pointer, but that pointer may not be set yet. */
        result = TRUE;
        break;
      }  /* if */
      /* Get parent scope of the (potentially local) type. */
      sp = get_parent_scope_of(type);
    } else {
      sp = sp->parent;
    }  /* if */
  }  /* for */
  return result;
}  /* must_wait_for_mangling_reasons */

#endif /* NEED_NAME_MANGLING */
#if MODULE_ID_NEEDED && !STANDALONE_UTILITY_PROGRAM

static a_boolean scope_contains_local_types_or_static_variables(
                                                             a_scope_ptr scope)
/*
Recursively inspect the scope to determine if it contains any local types or
static variables.
*/
{
  a_boolean   result = FALSE;
  a_scope_ptr sp;
  
  if (scope->variables != NULL || scope->types != NULL) {
    result = TRUE;
  } else {
    for (sp = scope->scopes; sp != NULL; sp = sp->next) {
      if (scope_contains_local_types_or_static_variables(sp)) {
        result = TRUE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return result;
}  /* scope_contains_local_types_or_static_variables */


static a_boolean must_wait_for_module_id(a_routine_ptr routine)
/*
Returns TRUE if lowering of the routine should be delayed because a module
id isn't available and the routine may need one.  A module id is needed
during mangling of certain entities (static functions that are
externalized because they might be referenced by a template (and static
variables therein), individuated entities, unnamed namespaces).  TRUE is
a safe return (but may cause excess memory usage).
*/
{
  a_boolean result = FALSE;

  if (!C_mode() && get_module_id() == NULL) {
    a_scope_ptr scope = scope_for_routine(routine);
    if (export_template_allowed &&
        routine->storage_class == (a_storage_class)sc_static &&
        (scope->variables != NULL || scope->types != NULL ||
         scope->scopes != NULL)) {
      /* When exported templates are allowed, a static function might be
         externalized because it might be referenced by a template.
         If it has local static variables, they might have to be
         externalized too, and we can't generate the externalized name
         now because we don't have the module id yet. */
      result = TRUE;
    } else if (local_types_as_template_args_enabled &&
               scope_contains_local_types_or_static_variables(scope)) {
      /* If a routine has local types and they could potentially be used
         as a template argument (thus needing individuation and a module id),
         delay lowering of the routine.  Static variables promoted out of
         functions also need individuation, so delay lowering of scopes
         with static variables as well. */
      result = TRUE;
#if DO_FULL_PORTABLE_EH_LOWERING
    } else if (exceptions_enabled &&
               skip_typerefs(routine->type)->variant.routine.extra_info->
                                             exception_specification != NULL &&
               exception_specification_contains_an_individuated_entity(
                                                              routine->type)) {
      /* Lowering of a routine whose type contains an exception specification
         where an unnamed type is used in the exception specification causes
         a reference to the typeinfo for the type, resulting in a module id
         reference. */
      result = TRUE;
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
    } else {
      /* If any parent of the routine is an unnamed namespace, or the
         routine type refers to an unnamed namespace, wait for a module id. */
      if (is_member_of_unnamed_namespace(&routine->source_corresp) ||
          is_or_contains_unnamed_namespace_type(routine->type)) {
        result = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* must_wait_for_module_id */

#endif /* MODULE_ID_NEEDED && !STANDALONE_UTILITY_PROGRAM */

static a_boolean ctor_needs_unprocessed_field_initializer(a_routine_ptr  ctor)
/*
Return TRUE if the given constructor depends on a field initializer that
hasn't been fully processed yet.
*/
{
  a_constructor_init_ptr  ctor_init;
  a_boolean               result = FALSE;

  check_assertion(special_kind_is(ctor, sfk_constructor));
  ctor_init = scope_for_routine(ctor)->variant.routine.constructor_inits;
  for (; ctor_init != NULL; ctor_init = ctor_init->next) {
    if (ctor_init->use_field_initializer && ctor_init->initializer == NULL) {
      result = TRUE;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* ctor_needs_unprocessed_field_initializer */


static a_boolean in_closure_with_pending_parent(void)
/*
Return TRUE if we're inside a lambda closure class whose parent isn't fully
known yet and therefore may not yet have a mangled name.
*/
{
  a_boolean      result = FALSE;
  a_scope_depth  d = depth_scope_stack;

  for (;;) {
    switch (scope_stack[d].kind) {
      case sck_class_struct_union:
        { a_type_ptr  class_type = scope_stack[d].assoc_type;
          if (class_type->incomplete) {
            /* A lambda call operator definition is sometimes processed before
               the closure itself is completed (because we must collect the
               implicit captures).  However, that in turn may cause the symbol
               list of the class to be pending, which could cause mangling to
               fail.  Instead, we delay lowering on such operators: The closure
               will be complete when it is handled later on. */
            result = TRUE;
          } else if (class_symbol_supp(symbol_for(class_type))
                         ->lambda_immediately_inside_default_arg_expression &&
              class_type_supp(class_type)->lambda_parent.routine == NULL) {
            /* A routine defined inside a lambda appearing in a default
               argument of a function has a mangled name that depends on that
               function.  However, for namespace-scope functions, the function
               with the default argument is declared after the default argument
               is parsed (i.e., when the lambda is parsed).  So we must delay
               lowering of the lambda routine until the namespace-scope
               function has been declared.  For example:
                   void g(int x = [] {
                                       struct A { A() {} void f() {} };
                                       return 1;
                                     }()) {}
               Here the member function A::f will have its definition popped
               from the scope stack before function g(int) is declared.  A::f
               should therefore not be lowered right away. */
            result = TRUE;
            goto done;
          } else if (!class_type->source_corresp.is_local_to_function) {
            goto done;
          }  /* if */
        }
        break;
      case sck_file:
      case sck_namespace:
      case sck_namespace_extension:
        goto done;
      default:
        break;
    }  /* switch */
    d = scope_stack[d].previous_scope;
  }  /* for */
done:
  return result;
}  /* in_closure_with_pending_parent */


a_boolean should_delay_lowering_on_function(a_routine_ptr routine,
                                            a_boolean     at_initial_scope_pop)
/*
The scope for the body of the indicated routine is either being popped
(when at_initial_scope_pop is TRUE) or has already been popped (otherwise).
Return TRUE if there is a reason why the lowering of the function should
be delayed.  In some cases below, lowering is delayed until the end of
compilation, but in the case of lowering being delayed solely due to the
lack of a module id, the function is added to a list of functions that will
be lowered as soon as a module id becomes available (and TRUE is returned).
*/
{
  a_boolean   delay_lowering = FALSE;

  check_assertion(!in_secondary_trans_unit(routine));
  if (secondary_translation_unit_seen()) {
    /* Don't lower template instantiations in the primary translation unit
       if there are exported templates, because we want to eliminate
       references to entities in the secondary translation unit IL first.
       The lowering will be done later -- see
       finish_processing_for_function_bodies. */
    delay_lowering = TRUE;
  } else if (routine->source_corresp.is_class_member &&
             (rout_type_supp(skip_typerefs(routine->type))->this_class ==
              NULL) && is_incomplete_type(parent_class_of(routine))) {
    /* A member function of a class that is currently being defined.  This
       should only occur for constexpr functions or functions with deduced
       return types. */
    check_assertion_or_expect_error(routine->is_constexpr ||
                                    routine->is_declared_constexpr ||
                                    routine->has_deducible_return_type);
    delay_lowering = TRUE;
  } else if (special_kind_is(routine, sfk_constructor) &&
             class_symbol_supp(symbol_for(parent_class_of(routine)))
                                               ->scanning_field_initializer &&
             ctor_needs_unprocessed_field_initializer(routine)) {
    /* Sometimes a constructor instantiation is triggered while scanning a
       field initializer but the constructor might depend on that field
       initializer (or another field initializer that hasn't been parsed yet).
       Lowering cannot proceed until all needed field initializers have been
       parsed. */
    delay_lowering = TRUE;
  } else if (routine->contains_generic_lambda) {
    /* Routines that contain a generic lambda need to have lowering delayed
       so that the parent scope information is still available if
       instantiations of the generic lambda are done after the enclosing
       function scope has been popped. */
    delay_lowering = TRUE;
  } else if (at_initial_scope_pop && routine->source_corresp.is_class_member &&
             type_is_lambda_closure(parent_class_of(routine))) {
    /* Delay the lowering of closure members because the closure may still be
       in the process of being defined, or some associated information may not
       be recorded yet (such as the parent entity for mangling purposes). */
    delay_lowering = TRUE;
  } else if (routine->source_corresp.is_local_to_function &&
             in_closure_with_pending_parent()) {
    /* Delay lowering of lambda operators (or other closure members) whose
       mangling may depend on a context that isn't yet sufficiently known to
       fully mangle. */
    delay_lowering = TRUE;
  } else if (routine->is_constexpr) {
    /* constexpr functions are interpreted from the unlowered IL.  So lowering
       should be delayed until all interpreted evaluations are known to have
       occurred. */
    delay_lowering = TRUE;
#if NEED_NAME_MANGLING
  } else if (must_wait_for_mangling_reasons(routine)) {
    /* Lowering of the routine may result in requests for mangled encodings
       that depend on things that haven't happened yet (e.g., discriminators
       being set or structured bindings being identified) so delay lowering. */
    delay_lowering = TRUE;
#endif /* NEED_NAME_MANGLING */
#if GNU_EXTENSIONS_ALLOWED && COMPILE_MULTIPLE_TRANSLATION_UNITS
  } else if (routine->is_weak) {
    /* Correspondence checking can choose a secondary translation unit
       function with a definition as canonical instead of a weak primary
       translation unit function with a definition, so don't allow a weak
       definition to be closed out before it might get deleted. */
    delay_lowering = TRUE;
#endif /* GNU_EXTENSIONS_ALLOWED && COMPILE_MULTIPLE_TRANSLATION_UNITS */
  } else if (routine->lowering_delayed_on_nested_function) {
    /* In cases where the lowering of constructors/destructors of a local class
       has been delayed, the determination of whether or not vtables should be
       emitted cannot be done.  By delaying the lowering of functions that
       contain lowering-delayed functions, we ensure the functions will be
       lowered in the proper order (from innermost to outermost) and that
       vtables will be handled properly. */
    delay_lowering = TRUE;
  }  /* if */
#if MODULE_ID_NEEDED && !STANDALONE_UTILITY_PROGRAM
  if (!delay_lowering &&
      !routine->is_prototype_instantiation &&
      !scope_stack[depth_scope_stack].in_prototype_instantiation &&
      must_wait_for_module_id(routine)) {
    /* Delay lowering if a module id is not yet available and this routine
       may need access to it during lowering.  Prototype instantiations
       aren't currently lowered, so there's no need to delay. */
    /* Queue functions waiting for a module id on a separate list (which
       will be drained soon after a module id becomes available). */
    a_delayed_lowering_list_entry *entry =
                               alloc_fe_of_type(a_delayed_lowering_list_entry);
#if DEBUG
    num_delayed_lowering_list_entries_allocated++;
#endif /* DEBUG */
    entry->routine = routine;
    entry->next = NULL;
    if (waiting_for_module_id_list_head == NULL) {
      waiting_for_module_id_list_head = entry;
    } else {
      waiting_for_module_id_list_tail->next = entry;
    }  /* if */
    waiting_for_module_id_list_tail = entry;
    delay_lowering = TRUE;
  }  /* if */
#endif /* MODULE_ID_NEEDED && !STANDALONE_UTILITY_PROGRAM */
  if (delay_lowering) {
    /* Set a flag to delay lowering on all enclosing function scopes
       (if any). */
    a_scope_ptr sp;
    for (sp = get_parent_scope_of(routine);
         sp != NULL;
         sp = sp->parent) {
      if (sp->kind == (a_scope_kind)sck_function) {
        /* Directly nested functions. */
        sp->variant.routine.ptr->lowering_delayed_on_nested_function = TRUE;
      } else if (sp->kind == (a_scope_kind)sck_class_struct_union) {
        a_type_ptr class_type = sp->variant.assoc_type;
        if (class_type->source_corresp.is_local_to_function) {
          /* Functions containing local class with delayed routine. */
          a_routine_ptr  enclosing_rp = 
                         enclosing_routine_for_local_type_or_null(class_type);
          if (enclosing_rp != NULL) {
            enclosing_rp->lowering_delayed_on_nested_function = TRUE;
          } else {
            /* This can occur with certain severe lambda errors. */
            expect_error();
            break;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
    /* Record that lowering has been delayed on at least one function in the
       primary IL. */
    function_body_processing_delayed_on_some_func_in_primary_il = TRUE;
  }  /* if */
  return delay_lowering;
}  /* should_delay_lowering_on_function */

#endif /* DO_IL_LOWERING */

a_boolean should_delay_finishing_of_function_body(a_routine_ptr  routine)
/*
Some routines must be kept in memory for later use.  Return TRUE if
routine should be kept.
*/
{
  a_boolean	result = FALSE;

  if (routine != NULL) {
    if (routine->contains_generic_lambda) {
      /* Functions containing generic lambdas need to be kept because generic
         lambda instantiations will refer to their internals. */
      result = TRUE;
    } else if (routine->source_corresp.is_class_member) {
      a_type_ptr  parent_class = parent_class_of(routine);
      if (parent_class->incomplete) {
        /* Lowering the member function requires prelowering its parent class,
           and that cannot be done if the parent class is incomplete. */
        result = TRUE;
      } else if (class_type_supp(parent_class)->is_lambda_closure_class) {
        if (class_type_supp(parent_class)->is_generic_lambda_closure_class) {
          /* Generic lambda closure instances should also be retained. */
          result = TRUE;
#if NEED_NAME_MANGLING
        } else if (routine->is_lambda_body) {
          /* Lambdas appearing in structured binding declarations have to wait
             until the structured binding declaration is fully processed. */
          a_class_type_supplement_ptr
                    ctsp = class_type_supp(parent_class);
          if (ctsp->defined_in_variable_initializer) {
            a_variable_ptr  vp = ctsp->lambda_parent.variable;
            if (vp->is_struct_binding_container) {
              result = TRUE;
            }  /* if */
          }  /* if */
#endif /* NEED_NAME_MANGLING */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (result) {
    /* Record that lowering has been delayed on at least one function in the
       primary IL. */
    function_body_processing_delayed_on_some_func_in_primary_il = TRUE;
  }  /* if */
  return result;
}  /* should_delay_finishing_of_function_body */


static void wrap_up_symbols_with_no_scope(void)
/*
Certain symbols (such as keywords and predefined macros) are not entered
on the scope list of any scope.  When the file scope is popped for the
first time, such symbols must be moved to the inactive list.
*/
{
  a_symbol_ptr	sym;

  for (sym = symbols_with_no_scope; sym != NULL; sym = sym->next_in_scope) {
    unlink_symbol_from_symbol_table(sym);
    add_symbol_to_inactive_list(sym);
  }  /* for */
  /* Indicate that the file scope symbols have been moved to the inactive
     list. */
  file_scope_symbols_are_on_inactive_list = TRUE;
}  /* wrap_up_symbols_with_no_scope */


static void pop_scope_stack_entry(void)
/*
A helper function for pop_scope: The currently active scope has been fully
processed and can be popped.  Reduce the scope stack depth, and update the
new top-of-stack entry with information from the entry that has been popped.
*/
{
  if (--depth_scope_stack >= 0) {
    /* The stack is not empty, so do anything necessary to activate the
       new top entry. */
    a_scope_stack_entry_ptr  new_ssep = &scope_stack[depth_scope_stack];
    a_scope_stack_entry_ptr  ssep = new_ssep+1;
    a_memory_region_number   new_memory_region_number =
                                                  ssep->prev_il_memory_region;

    /* If the new memory region is not the same as the old, activate it. */
    if (new_memory_region_number != ssep->il_memory_region) {
      switch_il_region(new_memory_region_number);
    }  /* if */
    /* Restore state variables. */
    inside_local_class = new_ssep->inside_local_class;
    depth_innermost_function_scope = new_ssep->depth_innermost_function_scope;
    depth_innermost_namespace_scope =
                                    new_ssep->depth_innermost_namespace_scope;
    innermost_function_scope =
                   (depth_innermost_function_scope != NO_SCOPE_DEPTH) ?
                         scope_stack[depth_innermost_function_scope].il_scope :
                         NULL;
    decl_scope_level = ssep->decl_scope_level;
    depth_template_declaration_scope =
                                   new_ssep->depth_template_declaration_scope;
#if GENERATE_SOURCE_SEQUENCE_LISTS
    source_sequence_entries_disallowed =
                                  new_ssep->source_sequence_entries_disallowed;
    if (ssep->source_sequence_list != NULL) {
      /* Merge the source sequence list from the previous top stack entry into
         the new one. */
      if (new_ssep->end_of_source_sequence_list == NULL) {
        new_ssep->source_sequence_list = ssep->source_sequence_list;
      } else {
        new_ssep->end_of_source_sequence_list->next =
                                      ssep->source_sequence_list;
        ssep->source_sequence_list->prev =
                                      new_ssep->end_of_source_sequence_list;
      }  /* if */
      new_ssep->end_of_source_sequence_list =
                                         ssep->end_of_source_sequence_list;
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
      /* Update the ss_list_depth field of classes recorded in the previous
         top-level classes_in_ss_list, and move the latter list to the new
         top-level scope stack entry. */
      update_classes_in_ss_list(ssep, new_ssep);
      /* Source sequence entries for real instantiations must be inserted
         at a location determined by ss_list_instantiation_insert_point.
         The entries produced for a prototype instantiation, however, should
         just be inserted at the end of the current list.  To distinguish
         entries coming from a prototype instantiation, we must mark the scope
         stack entry appropriately.  Note that the "in_prototype_instantiation"
         flag by itself is insufficient because the prototype instantiation
         of a nested template may have caused another scope to be pushed also.
         For example:
           template<class T> struct A { template<class T> struct B; };
           template<> template<class U> struct A<int>::B {};
         Here a reactivation scope for A<int> is pushed between the
         instantiation context scope and the actual prototype instantiation
         scope. */
      if (ssep->kind != (a_scope_kind)sck_instantiation_context &&
          (ssep->src_seq_entries_from_prototype_instantiation ||
           ssep->in_prototype_instantiation ||
           ssep->in_generic_definition)) {
        new_ssep->src_seq_entries_from_prototype_instantiation = TRUE;
      }  /* if */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    if (ssep->kind == sck_instantiation_context && !ssep->is_rescan) {
      /* Restore previous overload resolution stack.  */
      pop_cur_ovl_res_stack();
    }  /* if */
  }  /* if */
}  /* pop_scope_stack_entry */


static void set_block_scope_parents(a_scope_ptr  parent_scope)
/*
The given scope is a block, condition, or function scope.  Recursively
traverse its block and condition scopes and set their parent pointers.  
*/
{
  a_scope_ptr  scope = parent_scope->scopes;

  while (scope != NULL) {
    if (scope->kind == (a_scope_kind)sck_block ||
        scope->kind == (a_scope_kind)sck_condition) {
      check_assertion(scope->parent == NULL || scope->parent == parent_scope);
      scope->parent = parent_scope;
      /* Recursively set the parents for all block and condition scopes nested
         in this one. */
      set_block_scope_parents(scope);
    }  /* if */
    scope = scope->next;
  }  /* while */
}  /* set_block_scope_parents */


static void keep_enclosing_function_memory_region(void)
/*
If the current scope is in a function memory region, set a flag in the
associated scope entry indicating the memory should not be freed.
*/
{
  a_memory_region_number	region_number;

  region_number = scope_stack_top().il_memory_region;
  if (region_number != file_scope_region_number) {
    a_scope_ptr	scope;
    scope = il_header.region_scope_entry[region_number];
    check_assertion(scope != NULL);
    check_assertion(scope->kind == (a_scope_kind)sck_function);
    scope->do_not_free_memory_region = TRUE;
  }  /* if */
}  /* keep_enclosing_function_memory_region */


void function_contains_generic_lambda(void)
/*
Mark the routines associated with any enclosing function scopes as
containing a generic lambda.
*/
{
  a_scope_stack_entry_ptr	ssep;

  if (depth_innermost_function_scope != NO_SCOPE_DEPTH ||
      inside_local_class) {
    /* Indicate that the enclosing memory region cannot be freed
       because a generic lambda could be instantiated after the
       function is no longer on the scope stack. */
    keep_enclosing_function_memory_region();
    for (ssep = &scope_stack_top(); ssep != NULL;
         ssep = previous_scope_of(ssep)) {
      if (scope_is(ssep, sck_function)) {
        check_assertion(ssep->assoc_routine != NULL);
        /* Skip further processing if the contains_generic_lambda flag
           has already been set. */
        if (ssep->assoc_routine->contains_generic_lambda) break;
        ssep->assoc_routine->contains_generic_lambda = TRUE;
      }  /* if */
    }  /* for */
  }  /* if */
}  /* function_contains_generic_lambda */


#if EXPENSIVE_CHECKING

static void check_parent_scope_of_member_entities(a_scope_ptr  sp)
/*
Verify that the members of the given scope have their parent_scope pointers
set correctly.
*/
{
  a_namespace_ptr  nsp = sp->namespaces;
  a_constant_ptr   cp = sp->constants;
  a_type_ptr       tp = sp->types;
  a_variable_ptr   vp = sp->variables;
  a_variable_ptr   np = sp->nonstatic_variables;
  a_label_ptr      lp = sp->labels;
  a_routine_ptr    rp = sp->routines;

  /* Namespaces: */
  for (; nsp != NULL; nsp = nsp->next) {
    check_assertion(parent_scope_of(nsp) == sp ||
                    (nsp->is_namespace_alias && parent_scope_of(nsp) == NULL));
  }  /* for */
  /* Constants: */
  for (; cp != NULL; cp = cp->next) {
    if (parent_scope_of(cp) == sp || parent_scope_of(cp) == NULL) {
      /* Normal cases. */
    } else {
      /* The file scope may contain lowered entities whose parent scope still
         points to the original enclosing scope. */
      check_assertion(
                  sp->kind == (a_scope_kind)sck_file &&
                  (parent_scope_of(cp)->kind == (a_scope_kind)sck_namespace ||
                   cp->source_corresp.is_class_member));
    }  /* if */
  }  /* for */
  /* Types: */
  for (; tp != NULL; tp = tp->next) {
    if (sp == parent_scope_of(tp) ||
        (parent_scope_of(tp) == NULL && in_file_scope(tp) &&
                                        !in_file_scope(sp)) ||
         (parent_scope_of(tp) == NULL && is_at_least_one_error())) {
      /* Normal cases. */
    } else {
      /* The file scope may contain lowered entities whose parent scope still
         points to the original enclosing scope. */
      check_assertion(sp->kind == (a_scope_kind)sck_file &&
                      (tp->source_corresp.is_local_to_function ||
                       is_class_or_namespace_member(tp)));
    }  /* if */
  }  /* for */
  /* Static storage variables: */
  for (; vp != NULL; vp = vp->next) {
    if (sp == parent_scope_of(vp) ||
        (parent_scope_of(vp) == NULL && in_file_scope(vp) &&
                                        !in_file_scope(sp))) {
      /* Normal cases. */
    } else {
      /* The file scope may contain lowered entities whose parent scope still
         points to the original enclosing scope. */
      check_assertion(sp->kind == (a_scope_kind)sck_file &&
                      is_class_or_namespace_member(vp));
    }  /* if */
  }  /* for */
  /* Automatic variables: */
  for (; np != NULL; np = np->next) {
    check_assertion(parent_scope_of(np) == sp ||
                    (sp->kind == (a_scope_kind)sck_func_prototype &&
                     is_at_least_one_error()));
  }  /* for */
  /* Labels: */
  for (; lp != NULL; lp = lp->next) {
    check_assertion(parent_scope_of(lp) == sp);
  }  /* for */
  /* Routines: */
  for (; rp != NULL; rp = rp->next) {
    check_assertion(parent_scope_of(rp) != NULL);
    if (sp->kind == (a_scope_kind)sck_file &&
        (parent_scope_of(rp)->kind == (a_scope_kind)sck_namespace ||
         rp->source_corresp.is_class_member)) {
      /* The file scope may contain lowered entities whose parent scope still
         points to the original enclosing scope. */
    } else if (prototype_instantiations_in_il &&
               rp->is_prototype_instantiation &&
               (sp->kind == (a_scope_kind)sck_file ||
                sp->kind == (a_scope_kind)sck_namespace)) {
      /* When prototype instantiations are recorded in the IL, dependent
         friend function declarations may be recorded in file or namespace
         scope even when the functions are members of a class or of an
         unrelated namespace. */
    } else {
      check_assertion(parent_scope_of(rp) == sp);
    }  /* if */
  }  /* for */
}  /* check_parent_scope_of_member_entities */

#endif /* EXPENSIVE_CHECKING */

static void clear_pack_expansion_variables(a_scope_stack_entry_ptr	ssep)
/*
Go through the pack expansion entries pointed to by ssep and clear the
variable pointers of any variable symbols to prevent references to freed
memory regions.
*/
{
  a_template_decl_info_ptr	tdip = ssep->template_decl_info;
  a_pack_expansion_descr_ptr	pedp = tdip->pack_expansions;
  a_pack_reference_ptr		prp;

  for (prp = pedp->packs_referenced; prp != NULL; prp = prp->next) {
    /* Only clear the pointer when the scope containing the variable is
       being popped. */
    if (prp->kind == prk_variable && prp->symbol->decl_scope == ssep->number) {
      prp->symbol->variant.variable.ptr = NULL;
    }  /* if */
  }  /* for */
}  /* clear_pack_expansion_variables */


void pop_scope_full(a_push_scope_options_set	options)
/*
End a name scope by popping an entry off the scope stack.  options is a
set of option flags that specify additional information about the scope
being popped.
*/
{
  a_scope_stack_entry_ptr  ssep, parent_ssep = NULL;
  a_scope_pointers_block_ptr pointers_block;
  a_memory_region_number   old_memory_region_number;
  a_scope_kind             kind;
  an_extern_type_fixup_ptr etfp;
  a_scope_depth            scope_depth;
  a_boolean                old_region_still_needed;
  a_scope_ptr              il_scope;
  a_routine_ptr            curr_routine = NULL;
  a_boolean                discard_function_body = FALSE;

  db_enter(3, "pop_scope");
  ssep = &scope_stack[depth_scope_stack];
  pointers_block = assoc_pointers_block_of(ssep);
  kind = ssep->kind;
  if (ssep->owns_module_push) {
    pop_module_entity_state();
  }  /* if */
  if (kind == (a_scope_kind)sck_function) {
    /* If the scope is for a routine, get a pointer to the routine. */
    curr_routine = ssep->il_scope->variant.routine.ptr;
  }  /* if */
#if DEBUG
  if (debug_level >= 3) {
    if (pointers_block->symbols != NULL || debug_level >= 4) {
      fprintf(f_debug, "pop_scope: number = %ld, depth = %d",
              (long)ssep->number, depth_scope_stack);
      if (curr_routine != NULL) {
        (void)fputs(", curr_routine = \"", f_debug);
        db_name_full(&curr_routine->source_corresp, iek_routine);
        (void)fputc('"', f_debug);
      } else if ((kind == (a_scope_kind)sck_class_struct_union ||
                  kind == (a_scope_kind)sck_class_reactivation ||
                  kind == (a_scope_kind)sck_enum) &&
                 ssep->assoc_type != NULL) {
        if (kind == (a_scope_kind)sck_enum) {
          (void)fputs(", enum class = \"", f_debug);
        } else {
          (void)fputs(", class = \"", f_debug);
        }  /* if */
        db_name_full(&ssep->assoc_type->source_corresp, iek_type);
        (void)fputc('"', f_debug);
      } else if ((kind == (a_scope_kind)sck_namespace ||
                  kind == (a_scope_kind)sck_namespace_reactivation ||
                  kind == (a_scope_kind)sck_namespace_extension) &&
                 ssep->il_scope != NULL &&
                 ssep->il_scope->variant.assoc_namespace != NULL) {
        (void)fprintf(f_debug, ", namespace%s = \"",
                      kind == (a_scope_kind)sck_namespace ? "" : "-ext");
        db_name_full(&ssep->il_scope->variant.assoc_namespace->source_corresp,
                     iek_namespace);
        (void)fputc('"', f_debug);
      } else {
        fputs(", kind = ", f_debug);
        if (ssep->is_for_init_block) (void)fputs("for-init ", f_debug);
        (void)db_scope_kind(kind);
      }  /* if */
      (void)fputc('\n', f_debug);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  check_assertion(ssep->class_fixup_header.fixup_list == NULL);
  if (!(ssep->kind == (a_scope_kind)sck_file && ssep->is_reactivation)) {
    if (ssep->kind == (a_scope_kind)sck_file) {
      /* When the file scope is popped the first time, transfer any symbols
         that are not associated with a scope list from the active list to
         the inactive list.  This is done before the wrapup_scope call
         below so that these symbols will be on the inactive list before
         the ones added below. */
      wrap_up_symbols_with_no_scope();
    }  /* if */
    /* Remove symbols from the symbol table, and reenter them on the
       inactive list if necessary.  For the file scope, this is only done
       the first time that it is popped. */
    wrapup_scope(ssep->il_scope, kind, pointers_block,
                 /*is_namespace_wrapup=*/FALSE,
                 is_local_scope_kind(ssep->kind) && ssep->is_reactivation,
                 options);
    /* wrapup_scope may have temporarily reactivated some scopes, which in
       turn may have triggered a reallocation of the scope stack. */
    ssep = &scope_stack[depth_scope_stack];
    if (ssep->kind == (a_scope_kind)sck_file) {
      /* In C99 mode, issue diagnostics for any local static variables that
         were defined in "inline definitions". */
      verify_c99_inline_definitions();
    }  /* if */
  }  /* if */
  il_scope = ssep->il_scope;
  if (il_scope != NULL) {
    if (!C_mode() && (kind == (a_scope_kind)sck_function ||
                      kind == (a_scope_kind)sck_block)) {
      /* If there are any local classes, check for compiler-generated
         virtual destructors for which bodies should be put out. */
      check_assertion(il_scope != NULL); /* For Coverity. */
      generate_required_virtual_destructor_bodies(il_scope);
    }  /* if */
#if EXPENSIVE_CHECKING
    if (!no_very_expensive_checking) {
      check_parent_scope_of_member_entities(il_scope);
    }  /* if */
#endif /* EXPENSIVE_CHECKING */
  }  /* if */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
  if (pointers_block->last_ms_if_exists != NULL) {
    /* The block contains entries for Microsoft __if_exists blocks.  Make
       sure that any opened blocks have been closed. */
    check_for_unclosed_if_exists_blocks();
  }  /* if */
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
  /* There should be no entries left on the curr_construct_pragmas list when
     the scope stack is popped. */
  check_assertion_or_expect_error_str2(
                               (ssep->curr_construct_pragmas == NULL ||
                                ssep->curr_construct_pragmas->is_empty()),
                               "pop_scope:", "curr_construct_pragmas != NULL");
  delete_fe(&ssep->curr_construct_pragmas);
  if (ssep->pending_pragmas != NULL) {
    /* Issue diagnostics on any pragmas that are still on the pending list. */
    end_of_scope_pragma_processing(*ssep->pending_pragmas);
    delete_fe(&ssep->pending_pragmas);
  }  /* if */
  if (!C_mode()) {
    /* If the scope specified additional using directives, clear all of the
       active using list flags, and reset them to the values specified
       by the previous scope stack entries. */
    if (ssep->active_using_directives != NULL) {
      set_active_using_list_scope_depths(depth_scope_stack,
                                         /*set_value=*/FALSE,
                                         NO_DECL_SEQUENCE_NUMBER);
      if (depth_scope_stack != DEPTH_OF_FILE_SCOPE) {
        set_active_using_list_scope_depths(ssep->previous_scope,
                                           /*set_value=*/TRUE,
                                           NO_DECL_SEQUENCE_NUMBER);
      }  /* if */
    }  /* if */
    /* Free any active using directive entries. */
    if (ssep->active_using_directives != NULL) {
      free_active_using_directive_list(ssep->active_using_directives);
      ssep->active_using_directives = NULL;
    }  /* if */
    /* Unlink scope-specific constants that might have been generated. */
    if (ssep->generated_entities != NULL) {
      ssep->generated_entities->function_name = NULL;
      ssep->generated_entities->pretty_function_name = NULL;
      ssep->generated_entities->decorated_function_name = NULL;
    }  /* if */
    /* Do management related to the object lifetime stack.  Don't pop the
       file scope object lifetime yet, though, because we need it in IL
       lowering; see below */
    if (is_local_scope_kind(kind)) {
      /* For a function, block, or condition scope, pop the current object
         lifetime, which ought to be the one created when this scope was
         pushed, except perhaps for an olk_try_block or olk_block_after_label
         (these are not established through push_scope_full).  For scopes that
         were pushed with PS_IS_REACTIVATION this is not done (since such
         reactivations only restore context for generic lambdas instantiations,
         not object lifetimes), except that when popping a reactivated
         sck_function scope, the saved object lifetime is restored. */
      if (!ssep->is_reactivation) {
        if (curr_object_lifetime->kind ==
                                     (an_object_lifetime_kind)olk_try_block ||
            curr_object_lifetime->kind ==
                            (an_object_lifetime_kind)olk_block_after_label) {
          curr_object_lifetime = curr_object_lifetime->parent_lifetime;
        }  /* if */
        check_assertion_str2(curr_object_lifetime ==
                                             ssep->curr_scope_object_lifetime,
                             "pop_scope: unexpected curr_object_lifetime",
                             "for function or block scope");
        if (kind == (a_scope_kind)sck_block &&
            (options & PS_NOT_FINAL_POP) != 0) {
          /* This is not the "final pop" of the local scope: Don't perform the
             usual cleanup operations associated with popping the lifetime. */
          curr_object_lifetime = curr_object_lifetime->parent_lifetime;
        } else {
          (void)pop_object_lifetime();
        }  /* if */
      }  /* if */
      if (kind == (a_scope_kind)sck_function) {
        check_assertion(il_scope != NULL); /* For Coverity. */
        if (!il_scope->variant.routine.ptr->compiler_generated &&
            !il_scope->variant.routine.ptr->is_defaulted &&
            !ssep->is_reactivation) {
          /* Flow control wrapup for statement processing is done here because
             part of what needs to be done is dependent on popping the object
             lifetime of the function scope.  (We don't do this for generated
             function bodies, nor for reactivated function scopes.) */
          wrapup_control_flow_processing(il_scope);
        }  /* if */
        /* Functions are always processed in the context of the file scope
           lifetime; restore the lifetime stack as it was when the function
           scope was pushed. */
        curr_object_lifetime = ssep->saved_curr_object_lifetime;
      }  /* if */
    } else if (kind == (a_scope_kind)sck_pragma ||
               kind == (a_scope_kind)sck_func_prototype ||
               kind == (a_scope_kind)sck_template_instantiation ||
               kind == (a_scope_kind)sck_class_struct_union ||
               kind == (a_scope_kind)sck_class_reactivation ||
               kind == (a_scope_kind)sck_module_decl_import) {
      /* These scopes usually are associated with the global object lifetime,
         but an exception is the function prototype scope for a requires
         expression. */
      check_assertion_str2(curr_object_lifetime ==
                              scope_stack[DEPTH_OF_FILE_SCOPE]
                                                .curr_scope_object_lifetime ||
                           scope_stack_top().outside_parameter_list,
                           "pop_scope: curr_object_lifetime is not that of",
                           "file scope");
      curr_object_lifetime = ssep->saved_curr_object_lifetime;
#if NEED_NAME_MANGLING
    } else if (kind == (a_scope_kind)sck_namespace ||
#if ABI_COMPATIBILITY_VERSION >= 408
               kind == (a_scope_kind)sck_namespace_reactivation ||
#endif /* ABI_COMPATIBILITY_VERSION >= 408 */
               kind == (a_scope_kind)sck_namespace_extension) {
      /* Save the discriminator counters. */
      a_symbol_ptr  ns_sym = symbol_for(ssep->assoc_namespace);
      a_namespace_symbol_supplement_ptr
                    nssp = ns_sym->variant.namespace_info.extra_info;
      if (ssep->name_discr.last_unnamed_type_number >
                                             nssp->last_unnamed_type_number) {
        nssp->last_unnamed_type_number =
                                    ssep->name_discr.last_unnamed_type_number;
      }  /* if */
      if (ssep->last_closure_type_number > nssp->last_closure_type_number) {
        nssp->last_closure_type_number = ssep->last_closure_type_number;
      }  /* if */
    } else if (kind == sck_file) {
      /* Save file-scope discriminator counters so they survive
         reactivation.  When popping a nested reactivation, also update
         the primary file-scope stack entry. */
      if (ssep->name_discr.last_unnamed_type_number >
                                        last_file_scope_unnamed_type_number) {
        last_file_scope_unnamed_type_number =
                                    ssep->name_discr.last_unnamed_type_number;
      }  /* if */
      if (ssep->last_closure_type_number >
                                        last_file_scope_closure_type_number) {
        last_file_scope_closure_type_number = ssep->last_closure_type_number;
      }  /* if */
      if (ssep->is_reactivation &&
          depth_scope_stack > DEPTH_OF_FILE_SCOPE) {
        a_scope_stack_entry_ptr  fs_ssep = &scope_stack[DEPTH_OF_FILE_SCOPE];
        if (ssep->name_discr.last_unnamed_type_number >
                               fs_ssep->name_discr.last_unnamed_type_number) {
          fs_ssep->name_discr.last_unnamed_type_number =
                                    ssep->name_discr.last_unnamed_type_number;
        }  /* if */
        if (ssep->last_closure_type_number >
                                          fs_ssep->last_closure_type_number) {
          fs_ssep->last_closure_type_number = ssep->last_closure_type_number;
        }  /* if */
      }  /* if */
#endif /* NEED_NAME_MANGLING */
    }  /* if */
    /* Dispose of the list of entries of type a_name_hidden_by_old_for_init.
       They are no longer needed once the scope has been completed. */
    if (ssep->names_hidden_by_old_for_init != NULL) {
      free_names_hidden_by_old_for_init(ssep->names_hidden_by_old_for_init);
    }  /* if */
    /* Check to see if the scope being popped off the scope stack is an invalid
       instantiation that's suppressing further (recursive) instantiations of
       its associated template. */
    if (depth_innermost_instantiation_scope == depth_scope_stack) {
      a_symbol_ptr templ_sym = ssep->template_sym;

      if (templ_sym != NULL) {
        a_template_symbol_supplement_ptr sup =
                                              templ_sym->variant.template_info;

        if (sup->invalid_active_instantiation == ssep->instance_sym) {
          /* As this instantiation will no longer be in scope (and thus
             subsequent instantiations of this template are not recursive),
             resume allowing instantiations of this template. */
          sup->invalid_active_instantiation = NULL;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  check_assertion_str2(ssep->defer_access_checks == FALSE &&
                       ssep->deferred_access_checks == NULL,
                       "pop_scope:", "deferred access checks still on list");
  /* If a primary definition of a namespace is being popped (i.e., the
     initial definition of the namespace is complete), set the flag
     in the namespace's scope_pointers_block to indicate that any
     symbols that are subsequently added to the scope should be added
     directly to the inactive list.  This is also done with then file
     scope is popped, so that if the file scope is re-pushed, symbols
     will be added directly to the inactive list. */
  if (kind == (a_scope_kind)sck_namespace || kind == (a_scope_kind)sck_file) {
    ssep->assoc_pointers_block->add_symbols_to_inactive_list = TRUE;
  }  /* if */
  if (ssep->kind != (a_scope_kind)sck_file) {
    /* Restore the C99 and C++11 STDC pragma state. */
    curr_fp_contract_state      =
                       enum_cast<a_stdc_pragma_value>(ssep->fp_contract_state);
    curr_fenv_access_state      =
                       enum_cast<a_stdc_pragma_value>(ssep->fenv_access_state);
    curr_cx_limited_range_state =
                  enum_cast<a_stdc_pragma_value>(ssep->cx_limited_range_state);
#if FIXED_POINT_ALLOWED
    /* Restore the fixed-point STDC pragma state. */
    curr_fx_full_precision_state =
                 enum_cast<a_stdc_pragma_value>(ssep->fx_full_precision_state);
    curr_fx_fract_overflow_state =
                 enum_cast<a_stdc_pragma_value>(ssep->fx_fract_overflow_state);
    curr_fx_accum_overflow_state =
                 enum_cast<a_stdc_pragma_value>(ssep->fx_accum_overflow_state);
#endif /* FIXED_POINT_ALLOWED */
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Prune the list ahead of being moved onto the IL here or merging below
     during pop_scope_stack_entry. */
  prune_src_seq_list(&ssep->source_sequence_list,
                     &ssep->end_of_source_sequence_list);
  if (ssep->kind == (a_scope_kind)sck_file ||
      ssep->kind == (a_scope_kind)sck_function) {
    if (il_scope != NULL && ssep->source_sequence_list != NULL) {
      il_scope->source_sequence_list = ssep->source_sequence_list;
      if (ssep->kind == (a_scope_kind)sck_file) {
        /* Save the "last" pointer for the file scope for use on a
           later file scope reactivation. */
        curr_translation_unit->
                         file_scope_pointers_block.last_source_sequence_entry =
                                             ssep->end_of_source_sequence_list;
      }  /* if */
      ssep->source_sequence_list = NULL;
      ssep->end_of_source_sequence_list = NULL;
      if (ssep->kind == (a_scope_kind)sck_function) {
        fixup_function_scope_source_sequence_list(il_scope);
      }  /* if */
#if DEBUG
      if (debug_level >= 3 ||
          db_flag_is_set("dump_ss") ||
          db_flag_is_set("dump_ss_full")) {
        /* Display source sequence lists for debug purposes. */
        db_ss_list_for_scope(il_scope);
      }  /* if */
#endif /* DEBUG */
    }  /* if */
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if (ssep->first_scope != NULL) {
    /* Transfer the list of scopes nested within the current scope
       to the IL scope entry if there is one, or otherwise add it to
       the local scopes list for the parent scope.  We are doing this
       to avoid allocating IL scopes for empty block scopes and empty
       non-top-level function prototype scopes. */
    if (il_scope == NULL) {
      parent_ssep = ssep-1;
      /* Generate the scope entry if ssep is a top-level function prototype
         scope, i.e., one that is not nested within another function prototype
         scope. */
      if (kind == (a_scope_kind)sck_func_prototype &&
          parent_ssep->kind != (a_scope_kind)sck_func_prototype) {
        il_scope = ensure_il_scope_exists(ssep);
      }  /* if */
    }  /* if */
    if (il_scope != NULL) {
      /* There is an allocated IL scope entry. */
      il_scope->scopes = ssep->first_scope;
    } else {
      /* Add the list of scopes to the list for the parent scope. */
      if (parent_ssep->first_scope == NULL) {
        parent_ssep->first_scope = ssep->first_scope;
      } else {
        ssep->first_scope->prev = parent_ssep->last_scope->prev;
        parent_ssep->last_scope->next = ssep->first_scope;
      }  /* if */
      parent_ssep->last_scope = ssep->last_scope;
    }  /* if */
  }  /* if */
#if DEBUG
  if (kind == (a_scope_kind)sck_file ||
      kind == (a_scope_kind)sck_function) {
    /* Dump type list and object lifetime information. */
    if (il_scope != NULL) {
      if (db_flag_is_set("dump_type_lists")) {
        db_type_lists(il_scope, 0);
      }  /* if */
      if (db_flag_is_set("dump_lifetimes")) {
        fprintf(f_debug, "Object lifetime for ");
        db_scope(il_scope);
        fprintf(f_debug, ":\n");
        db_object_lifetime_tree(il_scope->lifetime);
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  /* Determine and remember the current (old) memory region, to see
     if it changes when returning to the outer scope. */
  old_memory_region_number = ssep->il_memory_region;
  /* Don't finish the file scope at this point.  That is deferred until
     the compilation unit (not translation unit) is completed. */
  if (old_memory_region_number == file_scope_region_number) {
    old_region_still_needed = TRUE;
  } else {
    /* If the old memory region number does not appear anywhere in the
       remaining stack, the region is no longer needed by the front end. */
    old_region_still_needed = FALSE;
    for (scope_depth = depth_scope_stack-1; scope_depth >= 0; scope_depth--) {
      if (scope_stack[scope_depth].il_memory_region ==
                                                    old_memory_region_number) {
        old_region_still_needed = TRUE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  /* For any entities on the extern_type_fixup_list, restore the type of the
     variable or routine to what it was earlier.  This is used for cases like
       int a[];
       main () {
         extern int a[5];
         ... Type of "a" is now "int [5]".
       }
       ... Type of "a" must be restored to "int []" at the end of "main".
     Note that the entries are just thrown away.  There are expected to be
     very few of them.
  */
  for (etfp = ssep->extern_type_fixup_list; etfp != NULL; etfp = etfp->next) {
    if (etfp->is_routine) {
      a_routine_ptr  rp = etfp->variant.routine;
      if (rp->has_deduced_return_type &&
          (!rp->defined ||
           (innermost_function_scope != NULL &&
            rp == current_routine_entry()) ||
           is_auto_type(etfp->type->variant.routine.return_type))) {
        /* Consider:
             auto f() { auto f(); return 1; }
           The local declaration triggered the creation of a fixup entry, but
           meanwhile the return type has been deduced.  Ensure that that
           deduced type is not lost. */
        etfp->type->variant.routine.return_type =
                                        rp->type->variant.routine.return_type;
      }  /* if */
      etfp->variant.routine->type  = etfp->type;
    } else {
      etfp->variant.variable->type = etfp->type;
    }  /* if */
  }  /* for */
  if (kind == sck_function && curr_routine != NULL && !ssep->is_reactivation) {
    /* End-of-function-definition processing: This is skipped for a reactivated
       function scope.  A reactivated function definition was either already
       processed earlier, or it will be processed when its still-active
       original scope stack entry is processed. */
    /* See whether this is a function whose body should be discarded. */
    discard_function_body = function_body_should_be_discarded(curr_routine) ||
                            scope_stack[depth_scope_stack].discard_when_popped;
#if DO_IL_LOWERING
    if (is_primary_translation_unit && !discard_function_body &&
        should_delay_lowering_on_function(curr_routine,
                                          /*at_initial_scope_pop=*/TRUE)) {
      /* Delay lowering of functions in some cases. */
      /* Note that il_lowering_needed() is not tested on purpose, to get
         proper error recovery behavior.  Also, it doesn't cover lowering
         needed in C99 mode. */
      /* Clear the function-scope shareable constants table, because
         that has to be done before the scope stack entry disappears. */
      empty_func_shareable_constants_table();
    } else
#endif /* DO_IL_LOWERING */
    if (!should_delay_finishing_of_function_body(curr_routine)) {
      /* Do final processing on the function body.  That includes
         IL lowering if appropriate. */
      check_assertion(il_scope != NULL); /* For Coverity. */
      finish_function_body_processing(il_scope, discard_function_body,
                                      /*delayed=*/FALSE);
    }  /* if */
    if (!discard_function_body) {
      /* The definition of the function is complete, so set the defined flag.
         Note: this allows sweeping the routine definition, and (except for
         inline functions) writing out of the body of the function, so it's
         done late.  Note that the body of a function in a secondary
         translation unit is swept twice -- first when unlowered, as
         part of the secondary translation unit, and again after
         lowering as part of the primary translation unit.  The first
         sweep is required to allow retention of referenced file-scope
         entities (e.g., types) during the elimination of unneeded
         entities in the secondary translation unit, before copying of
         IL from the secondary to the primary.  The second sweep is
         required to get the needed flags right in the final IL in
         the primary translation unit.  It is done by the call of
         remark_routine_definition_needed in
         finish_function_body_processing. */
      set_routine_defined(curr_routine);
    }  /* if */
  }  /* if */

  /* The IL scope, if any, is no longer on the stack.  This must occur
     after IL lowering and before check_for_done_with_memory_region. */
  if (il_scope != NULL &&
      il_scope->depth_in_scope_stack == depth_scope_stack) {
    il_scope->depth_in_scope_stack = NO_SCOPE_DEPTH;
  }  /* if */
  if (!old_region_still_needed) {
    if (discard_function_body) {
      /* This is a function whose body should be discarded (e.g., a
         trivial default constructor).  Discard it now.  Other functions
         that should be discarded are done later after source sequence
         processing has been done. */
      check_assertion(il_scope != NULL); /* For Coverity. */
      if (curr_routine->is_trivial_default_constructor) {
        clear_function_body(il_scope);
        /* Put the "defined" flag back on. */
        curr_routine->defined = TRUE;
        discard_function_body = FALSE;
      }  /* if */
    } else {
      if (il_scope->kind == (a_scope_kind)sck_function) {
        /* Set the parent scopes for any block scopes represented in the IL. */
        set_block_scope_parents(il_scope);
      }  /* if */
      /* Write the memory region and free it as appropriate. */
      check_for_done_with_memory_region(old_memory_region_number);
    }  /* if */
  }  /* if */
  /* For template instantiation scopes, restore the template parameters
     to their previous state.  Normally this just involves setting the
     parameters to point to the "resting" values assigned when the
     template declaration is scanned.  In the event of a recursive
     instantiation, however, this requires restoring the values from the
     previous instantiation. */
  if (kind == (a_scope_kind)sck_template_instantiation) {
    a_scope_depth                     prev_depth;
    a_template_decl_info_ptr	      template_decl_info =
                                                     ssep->template_decl_info;

    check_assertion(template_decl_info != NULL);
    prev_depth = NO_SCOPE_DEPTH;
    /* Loop through the scope stack looking for a previous instantiation
       scope that uses the same template parameter list as the one being
       popped.  This is necessary because template parameter lists are
       shared between a class and the member functions defined inside the
       class. */
    for (scope_depth = depth_scope_stack - 1;
         scope_depth >= 0;
         scope_depth--) {
      if (scope_stack[scope_depth].kind ==
          (a_scope_kind)sck_template_instantiation &&
          scope_stack[scope_depth].template_decl_info->parameters ==
                                              template_decl_info->parameters) {
        prev_depth = scope_depth;
        break;
      }  /* if */
    }  /* for */
    if (prev_depth == NO_SCOPE_DEPTH) {
      /* Restore the default values of the parameters. */
      restore_default_template_params(template_decl_info->parameters,
                                      /*packs_only=*/FALSE);
    } else {
      /* Restore the parameter values from the previous instantiation. */
      update_template_param_symbols(template_decl_info->parameters,
                                    scope_stack[prev_depth].template_arg_list);
    }  /* if */
  }  /* if */
  if (ssep->packs_referenced != NULL) {
    /* If there were any variadic parameter packs that were referenced but
       not expanded, issue a diagnostic. */
    issue_pack_not_expanded_diagnostics(ssep->packs_referenced);
    free_list_of_pack_references(ssep->packs_referenced);
    ssep->packs_referenced = NULL;
  }  /* if */
  if ((kind == (a_scope_kind)sck_template_declaration &&
       (options & PS_IS_TEMPLATE_TEMPLATE_PARAM) == 0) ||
      kind == (a_scope_kind)sck_template_instantiation) {
    /* Restore the original pack expansion stack before a template declaration
       or instantiation scope was pushed. */
    pack_expansion_stack = ssep->pack_expansion_stack;
    if (pack_expansion_stack != NULL &&
        pack_expansion_stack->instantiation_descr != NULL) {
      /* Restore the state of any parameter pack parameters. */
      update_parameter_pack_symbol_values(pack_expansion_stack);
    }  /* if */
  }  /* if */
  if (kind == sck_module_decl_import) {
    pop_saved_stmt_scope_stack();
  }  /* if */
  if (ssep->template_decl_info != NULL &&
      ssep->template_decl_info->pack_expansions != NULL) {
    /* The variable entries in pack expansions are cleared to prevent
       references to freed memory regions. */
    clear_pack_expansion_variables(ssep);
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (ssep->source_sequence_list != NULL) {
#if DEBUG
    if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
      fputs("popping ", f_debug);
      db_scope_stack_entry_at_depth(depth_scope_stack);
      fputs("\n", f_debug);
    }  /* if */
#endif /* DEBUG */
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
    if (ssep->kind == (a_scope_kind)sck_template_instantiation &&
        /* The prototype instantiation should not be moved away from the
           associated template. */
        !ssep->in_prototype_instantiation &&
        !ssep->in_nonreal_instantiation &&
        !ssep->in_generic_definition &&
        !ssep->src_seq_entries_from_prototype_instantiation &&
        !ssep->microsoft_specialization_instantiation_scope) {
      insert_instantiation_src_seq_list(ssep);
    } else
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
    /* Do not add code here. */
    {
#if DEBUG
      if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
        fputs("moving source sequences to ", f_debug);
        db_scope_stack_entry_at_depth(depth_scope_stack-1);
        fputs(":\n", f_debug);
        db_ss_list_for_scope_depth(depth_scope_stack);
      }  /* if */
#endif /* DEBUG */
    }
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  /* Set the initial name lookup scope to the previous scope on the
     stack.  Note that this could be different than the previous scope
     value in the scope stack entry. */
  depth_of_initial_lookup_scope = ssep->saved_depth_of_initial_lookup_scope;
#if NEED_NAME_MANGLING
  free_local_name_collision_table(ssep);
#endif /* NEED_NAME_MANGLING */
#if DO_IL_LOWERING && ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS
  /* If a string literal table was allocated for the scope, free it now. */
  if (ssep->string_literal_table != NULL) {
    free_string_literal_table(ssep);
  }  /* if */
#endif /* DO_IL_LOWERING && ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS */
  if (discard_function_body) {
    /* If the function body should be discarded (and was not done above),
       do orphan processing to promote file scope IL entities that may be
       on the scope list of the routine, and then get rid of the function
       body.  This must be done after the source sequence list processing
       above has been completed. */
    check_assertion(il_scope != NULL); /* For Coverity. */
    add_scope_orphaned_il_lists(il_scope);
    clear_function_body(il_scope);
    /* Put the "defined" flag back on. */
    curr_routine->defined = TRUE;
  }  /* if */
  /* Pop the stack. */
  pop_scope_stack_entry();
  if (C_dialect == C_dialect_cplusplus) {
    /* Keep track of the number of current classes and class reactivations.
       (If either count is non-zero name lookup is more involved.) */
    if (kind == (a_scope_kind)sck_class_struct_union ||
        kind == (a_scope_kind)sck_class_reactivation) {
      num_classes_on_scope_stack--;
    }  /* if */
    /* Maintain the depth of the innermost template instantiation scope. */
    depth_innermost_instantiation_scope =
                                 ssep->depth_innermost_instantiation_scope;
    /* Maintain the depth of the innermost stack entry that affects access
       control. */
    depth_of_innermost_scope_that_affects_access_control =
                                  ssep->next_scope_that_affects_access_control;
    curr_deferred_access_scope = ssep->saved_curr_deferred_access_scope;
    expr_stack = ssep->saved_expr_stack;
  }  /* if */
  if (ssep->is_for_init_block) {
    /* A for-init block is being popped.  Its declarations are going out of
       scope.  But in older versions of C++ they would have remained in scope
       till the end of the containing block.  Track declarations that were
       formerly hidden (say, with cfront) but are now visible (under the new
       for-init scoping rules). */
    record_names_hidden_by_old_for_init(
                                   assoc_pointers_block_of(ssep)->symbols);
  }  /* if */
#if !STANDALONE_UTILITY_PROGRAM && DO_IL_LOWERING
  if (innermost_function_scope == NULL &&
      waiting_for_module_id_list_head != NULL &&
      ssep->kind != (a_scope_kind)sck_file &&
      !is_template_dependent_context() &&
      get_module_id() != NULL) {
    /* There may be functions whose lowering has previously been delayed
       because a suitable module id had not yet been created until now.  If so,
       lower those functions now. */
    lower_functions_waiting_for_module_id();
  }  /* if */
#endif /* !STANDALONE_UTILITY_PROGRAM && DO_IL_LOWERING */
  db_exit();
}  /* pop_scope_full */


void pop_scope(void)
/*
Interface to pop_scope_full that supplies a default value for the options
parameter.
*/
{
  pop_scope_full(PS_NO_OPTIONS);
}  /* pop_scope */


void f_push_namespace_extension_scope(a_namespace_ptr nsp,
				      a_boolean	      force_new_entry)
/*
Push one or more scopes that will be used to extend the indicated namespace.
This is used, for example, when scanning functions defined in the namespace.
A namespace extension scope is pushed in contexts where members can be
added to a namespace either directly or as a result of a name injection.
This routine is called only in C++.

If force_new_entry is TRUE, a new extension is pushed even if the
current scope is already an extension of the requested scope.
*/
{
  a_namespace_ptr		parent_nsp;
  a_namespace_ptr		curr_nsp = NULL;
  a_scope_stack_entry_ptr	ssep = &scope_stack[depth_scope_stack];

  /* If the current scope is a namespace (or namespace extension) scope,
     see if it matches the one that we are pushing.  If so, don't actually
     push the scope, just increment the count of the number of excess
     pushes done on this scope. */
  if (ssep->kind == (a_scope_kind)sck_namespace ||
      ssep->kind == (a_scope_kind)sck_namespace_extension) {
    curr_nsp = ssep->il_scope->variant.assoc_namespace;
  }  /* if */
  if (curr_nsp == nsp && !force_new_entry) {
    /* The scope is already on the stack. */
    ssep->num_of_extra_times_pushed++;
  } else {
    /* The entry isn't on the stack.  Push any parent namespaces, then push
       the specified namespace. */
    parent_nsp = parent_namespace_or_null(nsp);
    if (parent_nsp != NULL) {
      /* A namespace nested in another namespace.  Push the parent
         namespace. */
      f_push_namespace_extension_scope(parent_nsp, force_new_entry);
    }  /* if */
    /* Push an entry for the scope. */
    (void)push_namespace_scope((a_scope_kind)sck_namespace_extension, nsp);
  }  /* if */
}  /* f_push_namespace_extension_scope */


void pop_namespace_extension_scope(void)
/*
Pop one or more scopes pushed by push_namespace_extension_scope.
This routine is called only in C++.
*/
{
  a_scope_stack_entry_ptr	ssep;
  a_namespace_ptr		parent_nsp;

  ssep = &scope_stack[depth_scope_stack];
  check_assertion_str2(ssep->kind == (a_scope_kind)sck_namespace_extension ||
                       ssep->kind == (a_scope_kind)sck_namespace,
                       "pop_namespace_extension_scope:",
                       "entry not namespace extension");
  if (ssep->num_of_extra_times_pushed > 0) {
    /* This namespace had already been pushed when the call to
       push_namespace_extension_scope was done.  So, we don't want to
       actually pop the scope at this point.  Just decrement the count
       of excess pushes. */
    ssep->num_of_extra_times_pushed--;
  } else {
    /* Pop the reactivation scope. */
    parent_nsp = parent_namespace_or_null(
                                     ssep->il_scope->variant.assoc_namespace);
    pop_scope();
    if (parent_nsp != NULL) {
      /* A nested namespace.  Pop the enclosing namespaces too. */
      pop_namespace_extension_scope();
    }  /* if */
  }  /* if */
}  /* pop_namespace_extension_scope */


static
void set_template_decl_lookup_sequence(a_scope_depth initial_depth)
/*
If a namespace reactivation scope is pushed on top of a template
declaration scope some special processing needs to be done so that
name lookup works properly.  The namespace reactivation scopes must be
considered after the template declaration scope (which contains the
template parameters).  This is done by altering the previous_scope
links to reflect the desired name lookup sequence.  The following
table illustrates how the scope stack is updated as each scope is
pushed for this example:

  namespace A { namespace B { template <class T> void f(); } }
  template <class T> void A::B::f(){}
 
Each entry contains "N xxx (P)", where N is the scope depth, xxx is
the scope kind, and P is the previous scope for name lookup purposes.

  Initial                 After namespace A      After namespace B
  state                   is reactivated         is reactivated
  -----------------       -----------------      -----------------
                                                 3 ns react B (2)
                          2 ns react A (0)       2 ns react A (0)
  1 templ. decl. (0)      1 templ. decl. (2)     1 templ. decl. (3)
  0 file (none)           0 file (none)          0 file (none)

initial_depth is the depth of the template declaration scope.

This routine is also used when a template instantiation scope is
pushed for a Microsoft template specialization scope.  This is done
to make template parameters from a template declaration scope visible
inside of the instantiation scope pushed for the specialization.
*/
{
  a_scope_stack_entry_ptr	initial_ssep = &scope_stack[initial_depth];
  a_scope_stack_entry_ptr	curr_ssep = &scope_stack[depth_scope_stack];
  a_scope_stack_entry_ptr	prev_ssep = curr_ssep-1;
  a_scope_depth			outer_templ_decl_depth = initial_depth;
  a_scope_stack_entry_ptr	outer_templ_decl_ssep;

  /* Look for the outermost template declaration scope. */
  while (scope_stack[outer_templ_decl_depth-1].kind ==
                                      (a_scope_kind)sck_template_declaration) {
    --outer_templ_decl_depth;
  }  /* while */
  outer_templ_decl_ssep = &scope_stack[outer_templ_decl_depth];
  if (initial_ssep == prev_ssep) {
    /* Make the previous scope for the first namespace reactivation
       point to the previous scope of the outermost template declaration
       scope. */
    curr_ssep->previous_scope = outer_templ_decl_ssep->previous_scope;
  } else {
    /* Subsequent reactivation scopes will have their previous scope entry
       set to point to the template declaration scope.  Reset the previous
       pointer to point to the previous namespace reactivation. */
    curr_ssep->previous_scope = depth_scope_stack-1;
  }  /* if */
  /* Make the previous scope of the template declaration scope the innermost
     namespace reactivation scope. */
  outer_templ_decl_ssep->previous_scope = depth_scope_stack;
  /* Name lookups should begin at the template declaration scope. */
  depth_of_initial_lookup_scope = scope_depth_of(initial_ssep);
}  /* set_template_decl_lookup_sequence */


static void reset_template_decl_lookup_sequence(void)
/*
Undo the lookup sequence updates done by set_template_decl_lookup_sequence.
This is called when the template declaration scope is at the top of
the stack.
*/
{
  a_scope_stack_entry_ptr	ssep;
  a_scope_depth			outer_templ_decl_depth = depth_scope_stack;

  /* Look for the outermost template declaration scope. */
  while (scope_stack[outer_templ_decl_depth-1].kind ==
                                      (a_scope_kind)sck_template_declaration) {
    --outer_templ_decl_depth;
  }  /* while */
  ssep = &scope_stack[outer_templ_decl_depth];
  ssep->previous_scope = outer_templ_decl_depth-1;
  depth_of_initial_lookup_scope = depth_scope_stack;
}  /* reset_template_decl_lookup_sequence */


void f_push_namespace_reactivation_scope(
				a_namespace_ptr		nsp,
				a_boolean		force_new_entry)
/*
Push one or more scopes that will reactivate the indicated namespace.
This is used in contexts where the names from a namespace need to be
visible, but new members cannot be added to the namespace.
This routine is called only in C++.

If force_new_entry is TRUE, a new reactivation is pushed even if the
current scope is already a reactivation of the requested scope.
*/
{
  a_namespace_ptr		parent_nsp;
  a_namespace_ptr		curr_nsp = NULL;
  a_scope_depth			initial_depth = depth_scope_stack;
  a_scope_stack_entry_ptr	ssep = &scope_stack[depth_scope_stack];
  a_boolean			initial_scope_is_template_decl;

  initial_scope_is_template_decl = ssep->kind ==
                                        (a_scope_kind)sck_template_declaration;
  /* If the current scope is a namespace (or namespace extension) scope,
     see if it matches the one that we are pushing.  If so, don't actually
     push the scope, just increment the count of the number of excess
     pushes done on this scope. */
  if (ssep->kind == (a_scope_kind)sck_namespace ||
      ssep->kind == (a_scope_kind)sck_namespace_extension) {
    curr_nsp = ssep->il_scope->variant.assoc_namespace;
  }  /* if */
  if (curr_nsp == nsp && !force_new_entry) {
    /* The scope is already on the stack. */
    ssep->num_of_extra_times_pushed++;
  } else {
    /* The entry isn't on the stack.  Push any parent namespaces, then push
       the specified namespace. */
    parent_nsp = parent_namespace_or_null(nsp);
    if (parent_nsp != NULL) {
      /* A namespace nested in another namespace.  Push the parent
         namespace. */
      push_namespace_reactivation_scope(parent_nsp);
    }  /* if */
    /* Push an entry for the scope. */
    (void)push_namespace_scope((a_scope_kind)sck_namespace_reactivation, nsp);
    if (initial_scope_is_template_decl) {
      set_template_decl_lookup_sequence(initial_depth);
    }  /* if */
#if DEBUG
    if (db_flag_is_set("ns_react_on_templ_decl")) {
      fprintf(f_debug, "Scope stack after namespace reactivation:\n");
      db_scope_stack();
    }  /* if */
#endif /* DEBUG */
  }  /* if */
}  /* f_push_namespace_reactivation_scope */


void pop_namespace_reactivation_scope(void)
/*
Pop one or more scopes pushed by push_namespace_reactivation_scope.
This routine is called only in C++.
*/
{
  a_scope_stack_entry_ptr	ssep;
  a_namespace_ptr		parent_nsp;

  ssep = &scope_stack[depth_scope_stack];
  check_assertion_str2(ssep->kind == (a_scope_kind)sck_namespace_extension ||
                       ssep->kind == (a_scope_kind)sck_namespace ||
                       ssep->kind == (a_scope_kind)sck_namespace_reactivation,
                       "pop_namespace_reactiveation_scope:",
                       "entry not reactivation extension");
  if (ssep->num_of_extra_times_pushed > 0) {
    /* This namespace had already been pushed when the call to
       push_namespace_reactivation_scope was done.  So, we don't want to
       actually pop the scope at this point.  Just decrement the count
       of excess pushes. */
    ssep->num_of_extra_times_pushed--;
  } else {
    /* Pop the reactivation scope. */
    parent_nsp = parent_namespace_or_null(ssep->assoc_namespace);
    pop_scope();
    if (parent_nsp != NULL) {
      /* A nested namespace.  Pop the enclosing namespaces too. */
      pop_namespace_reactivation_scope();
    }  /* if */
  }  /* if */
  ssep = &scope_stack[depth_scope_stack];
  if (ssep->kind == (a_scope_kind)sck_template_declaration) {
    /* When a namespace reactivation is placed on top of a template
       declaration scope, the template declaration scope will have had
       its previous pointer updated.  Restore it to the original value. */
    reset_template_decl_lookup_sequence();
  }  /* if */
}  /* pop_namespace_reactivation_scope */


static a_scope_depth reactivate_class_scope(
                                      a_type_ptr class_type,
                                      a_boolean  is_specialization,
                                      a_boolean  extend_namespace,
                                      a_boolean  force_new_context,
                                      a_boolean  suppress_namespace)
/*
Push one or more scopes that will reactivate the indicated class type.
Return the scope depth prior to the reactivation of the first class
reactivation scope that is pushed.  is_specialization is TRUE if the
class is being reactivated as part of the declaration of a specialization
of a class member.  If the class is a member of a namespace, a namespace
reactivation or extension scope is pushed (depending on the value of
extend_namespace); however, if force_new_context is FALSE the enclosing
namespace will be not be pushed if it is the current scope.
suppress_namespace is TRUE if the caller pushed the namespace context (if
needed), so that should not be done here (even if force_new_context is
TRUE).
*/
{
  a_symbol_ptr			class_sym;
  a_boolean			namespace_pushed = FALSE;
  a_scope_depth			orig_depth = NO_SCOPE_DEPTH;
  a_scope_depth			starting_depth = depth_scope_stack;
  a_push_scope_options_set	options;
  a_scope_stack_entry_ptr	ssep;

  /* Get the symbol associated with the class. */
  class_sym = (a_symbol_ptr)(class_type->source_corresp.assoc_info);
  if (class_sym->is_class_member) {
    /* Nested class.  Push the containing class(es) first. */
    orig_depth = reactivate_class_scope(sym_parent_class(class_sym),
                                        is_specialization,
                                        extend_namespace,
                                        force_new_context,
                                        suppress_namespace);
    /* Propagate the namespace pushed flag up to the innermost class
       reactivation scope. */
    namespace_pushed = scope_stack[depth_scope_stack].namespace_pushed;
  } else if (!suppress_namespace && sym_is_namespace_member(class_sym)) {
    /* The class is nested in a namespace -- push enclosing namespace(s). */
    a_namespace_ptr		parent_nsp;
    parent_nsp = sym_parent_namespace(class_sym);
    /* The namespace is either extended or reactivated depending on the
       value of extend_namespace. */
    if (extend_namespace) {
      f_push_namespace_extension_scope(parent_nsp, force_new_context);
    } else {
      f_push_namespace_reactivation_scope(parent_nsp, force_new_context);
    }  /* if */
    namespace_pushed = TRUE;
  }  /* if */
  if (orig_depth == NO_SCOPE_DEPTH) orig_depth = depth_scope_stack;
  /* For the outermost class reactivation, indicate that this should
     begin a new access checking context.  This is only done for
     cases that are considered new contexts as indicated by the
     extend_namespace flag. */
  options = starting_depth == depth_scope_stack && extend_namespace 
                                       ? PS_NEW_ACCESS_CONTEXT : PS_NO_OPTIONS;
  if (is_specialization) options |= PS_IS_SPECIALIZATION;
  push_single_class_reactivation_scope(class_type, options);
  ssep = &scope_stack_top();
  ssep->namespace_pushed = namespace_pushed;
  return orig_depth;
}  /* reactivate_class_scope */


void push_instantiation_scope_for_class(
			a_type_ptr	class_type,
			a_boolean	is_microsoft_specialization_scope)
/*
Push a template instantiation scope for "class_type".  This is used
to reactivate a template instantiation scope after the class has been
instantiated, and in Microsoft mode to push an instantiation scope used
when a class specialization is defined (is_microsoft_specialization_scope
is TRUE in this case).  As a result, partial specializations need not be
taken into account (if the class has been instantiated, the class_template of
the class symbol supplement points to the partial specialization).
*/
{
  a_symbol_ptr				template_sym;
  a_template_arg_ptr			template_arg_list;
  a_template_decl_info_ptr		decl_info;
  a_template_symbol_supplement_ptr	tssp;
  a_symbol_ptr				class_sym;

  if (class_type->variant.class_struct_union.is_in_class_specialization) {
    /* This is only true for Microsoft in-class specializations.
       Such a specialization may be a declared in a class template, or
       a nested class of a class template.  Reactivate the parent class,
       then push a normal reactivation scope for the specialized class. */
    /* Reactivate the parent class. */
    a_type_ptr	parent_class;
    parent_class = parent_class_of(class_type);
    push_class_and_template_reactivation_scope(
                                            parent_class,
                                            /*reactivate_template_param=*/TRUE,
                                            /*entend_namespace=*/FALSE);
    /* Template parameters are never reactivated for in-class specializations
       (a Microsoft extension applicable to other specializations). */
    check_assertion(!is_microsoft_specialization_scope);
  } else {
    /* Get the symbol associated with the class. */
    class_sym = (a_symbol_ptr)(class_type->source_corresp.assoc_info);
    /* Get a pointer to the symbol associated with the template from
       which this class was generated. */
    template_sym = template_symbol_for_class_symbol(class_sym);
    template_arg_list = templ_arg_list_for_class(class_type);
    /* Get the template declaration information associated with the class. */
    tssp = template_supplement_for_symbol(template_sym);
    decl_info = cache_for_template(tssp)->decl_info;
    if (is_microsoft_specialization_scope) {
      /* When pushing a Microsoft specialization scope, don't do the full
         instantiation scope processing.  This is done because the
         enclosing scopes should be visible for these cases (Microsoft
         specialization scopes are pushed for class scopes for
         explicitly specialized classes, and for class reactivation
         scopes for all template classes).  This has the effect of
         making the class's template parameters visible while possibly
         hiding a set of template parameters that really should have
         been used (as in the case of a definition of a member of a
         class template).  The Microsoft compiler actually has two sets
         of parameters visible (the incorrect ones and then the correct
         ones).  The Sun compiler has only the wrong ones visible, but
         we don't emulate that exactly (we do the same as in Microsoft
         mode). */
      a_scope_depth		orig_depth = depth_scope_stack;
      a_scope_stack_entry_ptr	ssep;
      a_scope_depth		saved_innermost_scope_that_affects_access;
      saved_innermost_scope_that_affects_access =
                         depth_of_innermost_scope_that_affects_access_control;
      if (class_type->source_corresp.is_class_member) {
        /* Reactivate the parent class. */
        a_type_ptr	parent_class = parent_class_of(class_type);
        push_class_reactivation_scope(parent_class,
                                     /*entend_namespace=*/FALSE);
      } else if (is_namespace_member(class_type)) {
        /* Reactivate the parent namespace.  A new entry is forced because we
           later must be able to pop back to the previous scope state based
           only on the scope depth. */
        f_push_namespace_reactivation_scope(parent_namespace_of(class_type),
                                            /*force_new_entry=*/TRUE);
      }  /* if */
      if (decl_info != NULL) {
        /* The decl_info will not be present for nested classes that were
           declared but not defined.  Suppress this processing in such
           cases. */
        push_simple_instantiation_scope(decl_info, class_type,
                                        (a_routine_ptr)NULL, class_sym,
                                        template_sym, template_arg_list,
                                        PS_MICROSOFT_SPECIALIZATION);
      }  /* if */
      ssep = scope_stack_entry_for(depth_scope_stack);
      ssep->nested_instantiation = TRUE;
      ssep->orig_depth = orig_depth;
      ssep->saved_innermost_scope_that_affects_access =
                                     saved_innermost_scope_that_affects_access;
    } else {
      a_push_scope_options_set		options;
      options = PS_NO_OPTIONS;
      if (is_prototype_instantiation_symbol(class_sym)) {
        options |= PS_PROTOTYPE_INSTANTIATION;
      } else if (is_cli_generic_class_definition_symbol(class_sym)) {
        options |= PS_GENERIC_DEFINITION;
      }  /* if */
      (void)push_template_instantiation_scope(decl_info, class_type,
                                              (a_routine_ptr)NULL, class_sym,
                                              template_sym, template_arg_list,
  				              /*push_lex_state=*/FALSE,
  				              options);
    }  /* if */
  }  /* if */
}  /* push_instantiation_scope_for_class */


void push_class_and_template_reactivation_scope_full(
		a_type_ptr			class_type,
		a_boolean			reactivate_template_params,
		a_boolean			is_specialization,
		a_boolean			extend_namespace,
		a_boolean			force_new_context,
		a_push_scope_options_set	options)
/*
Push the scopes needed to reactivate the context of the specified class.
If the class is a template class, or a class defined within a template class,
this involves pushing the necessary template instantiation scopes as well.
If reactivate_template_params is FALSE, the template instantiation scopes
are not reactivated.  This is FALSE when called for normal class
reactivations.  is_specialization is TRUE if the class is being reactivated
as part of the declaration of a specialization of a class member.  If the
class is a member of a namespace, a namespace reactivation or extension
scope is pushed (depending on the value of extend_namespace); however, if
force_new_context is FALSE the enclosing namespace will be not be pushed if
it is the current scope. When force_new_context is TRUE, an instantiation
context will be pushed even for non-template classes to guarantee that the
enclosing context will not influence subsequent processing.  options is a
set of option flags that is passed down to the other scope pushing routines.
*/
{
  a_boolean	is_template = FALSE;
  a_symbol_ptr	class_sym;
  a_scope_depth	orig_depth = depth_scope_stack;
  a_scope_depth	initial_depth = depth_scope_stack;
  a_boolean	is_microsoft_specialization_scope = FALSE;
  a_boolean	initial_scope_is_template_decl = FALSE;
  a_scope_depth	saved_innermost_scope_that_affects_access;

  saved_innermost_scope_that_affects_access =
                         depth_of_innermost_scope_that_affects_access_control;
  if (class_type->source_corresp.is_local_to_function) {
    /* Don't force namespace scopes (and instantiation contexts) to be
       pushed for local classes. */
    force_new_context = FALSE;
  }  /* if */
  /* Get the symbol associated with the class. */
  class_sym = (a_symbol_ptr)(class_type->source_corresp.assoc_info);
  check_assertion_str2(class_sym != NULL,
                       "push_class_and_template_reactivation_scope_full:",
                       "class type has NULL assoc_info");
  /* Template instantiation scopes must be pushed for template
     instances.  Normally, instantiation scopes are not pushed for
     specialized classes.  But in Microsoft mode, an instantiation scope
     is pushed because the template parameters are visible, even in
     specializations.  Specializations are not treated as templates,
     except for prototype instantiations of Microsoft in-class
     specializations. */
  if (is_any_template_instance_class_symbol(class_sym)) {
    is_template = reactivate_template_params &&
                  (!is_template_instance_specific_def_symbol(class_sym) ||
                   is_prototype_instantiation_symbol(class_sym));
    if (use_microsoft_specialization_scope &&
        !class_type->variant.class_struct_union.is_in_class_specialization &&
        is_real_class_symbol(class_sym)) {
      /* Determine whether the instantiation scope is being pushed only
         because we are in Microsoft mode. */
      is_microsoft_specialization_scope = !is_template;
      is_template = TRUE;
    } else if (class_type->
                       variant.class_struct_union.is_in_class_specialization) {
      /* For in class specializations, is_template is set to make sure that
         template parameters of enclosing templates will be visible. */
      is_template = TRUE;
    }  /* if */
    if (is_microsoft_specialization_scope) {
      a_scope_stack_entry_ptr	ssep = &scope_stack[depth_scope_stack];
      initial_scope_is_template_decl = ssep->kind ==
                                        (a_scope_kind)sck_template_declaration;
    }  /* if */
  }  /* if */
  if (is_template) {
    /* The push of the template instantiation scope will not reactivate the
       class type (that it thinks is being instantiated).  Reactivate it
       now. */
    a_scope_stack_entry_ptr	ssep;
    push_instantiation_scope_for_class(class_type,
                                       is_microsoft_specialization_scope);
    ssep = scope_stack_entry_for(depth_scope_stack);
    if (initial_scope_is_template_decl &&
        !class_type->source_corresp.is_class_member) {
      /* In Microsoft mode, if a specialization instantiation scope is
         pushed inside a template declaration scope, the previous pointers
         need to be updated in order for name lookup to consider the
         template declaration scope at the appropriate time.  This is not
         done for member classes, because it will have already been done for
         the parent class. */
      set_template_decl_lookup_sequence(initial_depth);
    }  /* if */
    push_single_class_reactivation_scope(class_type, PS_NO_OPTIONS);
    /* Indicate that a template instantiation scope was pushed so that,
       when popping the class and template reactivation, we know how the
       scopes should be popped. */
    ssep = scope_stack_entry_for(depth_scope_stack);
    ssep->instantiation_scope_pushed = TRUE;
    ssep->microsoft_specialization_scope_pushed = TRUE;
  } else {
    a_scope_depth	new_orig_depth = NO_SCOPE_DEPTH;
    a_boolean		use_new_orig_depth = TRUE;
    if (depth_innermost_instantiation_scope != NO_SCOPE_DEPTH &&
        force_new_context) {
      /* A normal class but in a context where some other instantiation is
         underway.  Push the definition context for desired class. */
      push_definition_context_for_class(class_type, options);
      use_new_orig_depth = FALSE;
    }  /* if */
    /* Push the reactivation for the class.  Unless a definition context
       was pushed, the original depth returned is used instead of the one
       saved above because the original depth should not include any namespace
       reactivation scopes that may have been pushed. */
    new_orig_depth = reactivate_class_scope(
                                 class_type, is_specialization,
                                 extend_namespace, force_new_context,
                                 /*suppress_namespace=*/!use_new_orig_depth);
    if (use_new_orig_depth) orig_depth = new_orig_depth;
  }  /* if */
  {
    /* Record the scope depth prior to the reactivation so that the scopes
     reactivated can be popped later. */
    a_scope_stack_entry_ptr	ssep;
    ssep = scope_stack_entry_for(depth_scope_stack);
    ssep->orig_depth = orig_depth;
    ssep->saved_innermost_scope_that_affects_access =
                                     saved_innermost_scope_that_affects_access;
  }
}  /* push_class_and_template_reactivation_scope_full */


void push_class_and_template_reactivation_scope(
                                 a_type_ptr	class_type,
                                 a_boolean      reactivate_template_params,
				 a_boolean	extend_namespace)
/*
Push the scopes needed to reactivate the context of the specified class.
This is an interface to push_class_and_template_reactivation_scope_full
that allows reuse of the current scope if it is the namespace of the class.
If the class is a template class, or a class defined within a template class,
this involves pushing the necessary template instantiation scopes as well.
If reactivate_template_params is FALSE, the template instantiation scopes
are not reactivated.  This is FALSE when called for normal class
reactivations.  If the class is a member of a namespace, a namespace
reactivation or extension scope is pushed (depending on the value of
extend_namespace) unless that namespace is already the current scope.
*/
{
  push_class_and_template_reactivation_scope_full(
                      class_type, reactivate_template_params,
                      /*is_specialization=*/FALSE, extend_namespace,
                      /*force_new_context=*/FALSE, PS_NO_OPTIONS);
}  /* push_class_and_template_reactivation_scope */

void push_class_reactivation_scope(a_type_ptr class_type,
				   a_boolean  extend_namespace)
/*
Push the scopes needed to reactivate the context of the specified class.
This is an interface to push_class_and_template_reactivation_scope_full
that causes only the class reactivation (and not any template instantiation
scopes) to be pushed.  If the class is a member of a namespace, a
namespace reactivation or extension scope is pushed (depending on
the value of extend_namespace) unless that namespace is already the
current scope.
*/
{
  push_class_and_template_reactivation_scope_full
                           (class_type, /*reactivate_template_params=*/FALSE,
                            /*is_specialization=*/FALSE, extend_namespace,
                            /*force_new_context=*/FALSE, PS_NO_OPTIONS);
}  /* push_class_reactivation_scope */


void pop_class_reactivation_scope(void)
/*
Pop one or more scopes pushed by push_class_reactivation_scope.  This routine
is called only in C++.
*/
{
  a_scope_stack_entry_ptr	ssep;
  a_scope_depth			orig_depth;
  a_boolean			namespace_pushed = FALSE;
  a_boolean			microsoft_specialization_scope_pushed;
  a_boolean			instantiation_scope_pushed;
  a_scope_depth			saved_innermost_scope_that_affects_access;

  ssep = &scope_stack[depth_scope_stack];
  microsoft_specialization_scope_pushed =
                                   ssep->microsoft_specialization_scope_pushed;
  namespace_pushed = ssep->namespace_pushed;
  instantiation_scope_pushed = ssep->instantiation_scope_pushed;
  orig_depth = scope_stack[depth_scope_stack].orig_depth;
  saved_innermost_scope_that_affects_access =
      scope_stack[depth_scope_stack].saved_innermost_scope_that_affects_access;
  /* Pop scopes until the depth of the scope stack is equal to orig_depth,
     which is the depth before any of the class reactivation scopes were
     pushed. */
  check_assertion_str2(orig_depth != NO_SCOPE_DEPTH,
                       "pop_class_reactivation_scope:",
                       "invalid orig_depth");
  /* Clear the information about any using-directives currently in effect. */
  set_active_using_list_scope_depths(depth_scope_stack,
                                     /*set_value=*/FALSE,
                                     NO_DECL_SEQUENCE_NUMBER);
  /* Do the actual popping of the scopes. */
  while (orig_depth < depth_scope_stack) pop_scope();
  /* Restore the using-directive information to the appropriate state. */
  set_active_using_list_scope_depths(depth_scope_stack,
                                     /*set_value=*/TRUE,
                                     get_effective_decl_seq());
  if (instantiation_scope_pushed) {
    if (microsoft_specialization_scope_pushed &&
        scope_stack[depth_scope_stack].kind ==
                                     (a_scope_kind)sck_template_declaration) {
      /* When a Microsoft specialization scope is placed on top of a template
         declaration scope, the template declaration scope will have had
         its previous pointer updated.  Restore it to the original value. */
      reset_template_decl_lookup_sequence();
    }  /* if */
  } else {
    /* Pop the class and namespace reactivation scopes. */
    if (namespace_pushed) {
      /* The class is nested in a namespace -- pop enclosing namespace(s).
         The enclosing scopes could have been pushed as either extension
         or reactivation scopes.  Pop the appropriate kind of scope. */
      if (scope_stack[depth_scope_stack].kind ==
                                      (a_scope_kind)sck_namespace_extension) {
        pop_namespace_extension_scope();
      } else {
        pop_namespace_reactivation_scope();
      }  /* if */
    }  /* if */
  }  /* if */
  /* Restore the innermost scope that affects access.  The current value
     could be incorrect if the reactivation involved pushing instantiation
     scopes. */
  depth_of_innermost_scope_that_affects_access_control =
                                    saved_innermost_scope_that_affects_access;
}  /* pop_class_reactivation_scope */


static a_scope_stack_entry_ptr get_innermost_template_dependent_context(void)
/*
Return the scope stack entry for the innermost template declaration
scope or template instantiation scope for a prototype instantiation.
*/
{
  a_scope_stack_entry_ptr	ssep;
  a_scope_depth			depth_to_use = NO_SCOPE_DEPTH;

  /* Find the innermost template declaration or template instantiation
     scope.  Ignore any instantiation scopes for template rescans. */
  if (depth_innermost_instantiation_scope != NO_SCOPE_DEPTH) {
    ssep = scope_stack_entry_for(depth_innermost_instantiation_scope);
    while (ssep->kind == (a_scope_kind)sck_template_instantiation &&
           ssep->is_rescan) {
      /* If we are in a rescan context, skip to the next enclosing template
         instantiation scope, if any. */
      for (ssep--;
           ssep->kind != (a_scope_kind)sck_template_instantiation &&
             ssep->kind != (a_scope_kind)sck_file;
           ssep--) {}
    }  /* while */
    if (ssep->kind == (a_scope_kind)sck_template_instantiation) {
      depth_to_use = scope_depth_of(ssep);
    }  /* if */
  }  /* if */
  if (depth_to_use < depth_template_declaration_scope) {
    depth_to_use = depth_template_declaration_scope;
  } else {
    /* A template instantiation scope must be for a prototype instantiation. */
    check_assertion(depth_to_use != NO_SCOPE_DEPTH);
    check_assertion(scope_stack[depth_to_use].in_prototype_instantiation);
  }  /* if */
  check_assertion(depth_to_use != NO_SCOPE_DEPTH);
  ssep = &scope_stack[depth_to_use];
  return ssep;
}  /* get_innermost_template_dependent_context */


static a_scope_stack_entry_ptr get_outermost_template_dependent_context(void)
/*
Return the scope stack entry for the outermost template declaration
scope or template instantiation scope for a prototype instantiation.
*/
{
  a_scope_stack_entry_ptr	ssep;
  a_scope_depth			depth_to_use;

  /* Start with the innermost template declaration or template instantiation
     scope. */
  depth_to_use = depth_innermost_instantiation_scope;
  if (depth_to_use < depth_template_declaration_scope) {
    depth_to_use = depth_template_declaration_scope;
  }  /* if */
  /* Loop outward looking for the outermost template declaration or
     prototype instantiation scope. */
  for (ssep = scope_stack_entry_for(depth_to_use); ssep != NULL;
       ssep = previous_scope_of(ssep)) {
    if (ssep->kind == (a_scope_kind)sck_template_declaration ||
        (ssep->kind == (a_scope_kind)sck_template_instantiation &&
         ssep->in_prototype_instantiation)) {
      depth_to_use = scope_depth_of(ssep);
    }  /* if */
  }  /* for */
  ssep = &scope_stack[depth_to_use];
  /* A template instantiation scope must be for a prototype instantiation. */
  check_assertion(depth_to_use != NO_SCOPE_DEPTH);
  check_assertion(ssep->kind == (a_scope_kind)sck_template_declaration ||
                  ssep->in_prototype_instantiation ||
                  in_ms_nonreal_class_instantiation());
  return ssep;
}  /* get_outermost_template_dependent_context */


static a_scope_stack_entry_ptr get_scope_for_pack_references(void)
/*
Return the scope stack entry on which pack references in the current context
should be recorded.
*/
{
  a_scope_stack_entry_ptr  ssep;

  if (scope_stack_top().in_nonreal_instantiation &&
      scope_stack_top().is_default_template_arg &&
      enclosing_scope_is_prototype_instantiation_context()) {
    /* Keep references introduced while rescanning a default template argument
       local to that rescan.  They may be expanded by an enclosing context, but
       their token sequence numbers come from the default argument declaration
       and should not leak into the enclosing prototype instantiation. */
    ssep = &scope_stack_top();
  } else {
    ssep = get_outermost_template_dependent_context();
  }  /* if */
  return ssep;
}  /* get_scope_for_pack_references */


a_template_decl_info_ptr get_specified_template_decl_info(
					a_boolean	innermost)
/*
Return a pointer to the template declaration information entry associated
with the either the innermost (when innermost is TRUE) or outermost (when
innermost is FALSE) template instantiation or template declaration scope.
Note that such a scope is required to exist when this routine is called.
*/
{
  a_scope_stack_entry_ptr	ssep;
  a_template_decl_info_ptr	tdip;

  /* Get the template declaration or template instantiation scope for a
     prototype instantiation. */
  if (innermost) {
    ssep = get_innermost_template_dependent_context();
  } else {
    ssep = get_outermost_template_dependent_context();
  }  /* if */
  tdip = ssep->template_decl_info;
  check_assertion(tdip != NULL);
  return tdip;
}  /* get_specified_template_decl_info */


a_pack_reference_ptr alloc_pack_reference(a_pack_reference_kind	kind)
/*
Allocate a new pack reference entry, initialize it, and return a pointer
to it.  kind indicates the kind of entity to which this pack reference
refers.  This is really only meaningful for entries created for actual
instantiations because it controls the initialization of fields used only
in such cases.
*/
{
  a_pack_reference_ptr	prp;

  if (avail_pack_references != NULL) {
    /* Reuse an existing entry. */
    prp = avail_pack_references;
    avail_pack_references = avail_pack_references->next;
  } else {
    /* Allocate a new entry. */
    prp = alloc_fe_of_type(a_pack_reference);
#if DEBUG
    num_pack_references_allocated++;
#endif /* DEBUG */
  }  /* if */
  prp->next = NULL;
  prp->symbol = NULL;
  prp->kind = kind;
  prp->param_or_binding_num = 0;
  prp->position = null_source_position;
  prp->token_sequence_number = NO_TOKEN_SEQUENCE_NUMBER;
  prp->primary_pack_symbol = NULL;
  prp->function_or_block_scopes_to_skip = 0;
  prp->param_info = NULL;
  prp->coordinates = NULL;
  prp->template_param = NULL;
  switch (kind) {
    case prk_variable:
    case prk_binding:
      prp->curr_argument.variable = NULL;
      break;
    case prk_template_param:
    case prk_bases:
      prp->curr_argument.template_arg = NULL;
      break;
    case prk_parameter:
      /* Both entries are initialized to handle union-as-struct testing. */
      prp->curr_argument.param_id = NULL;
      prp->curr_argument.param_type = NULL;
      break;
    case prk_init_capture:
      prp->curr_argument.field = NULL;
      break;
    default:
      unexpected_condition();
  }  /* switch */
  prp->prev_template_arg = NULL;
  prp->uses_enclosing_pack = FALSE;
  prp->direct_bases = FALSE;
  return prp;
}  /* alloc_pack_reference */


static void free_list_of_pack_references(a_pack_reference_ptr prp)
/*
Return a list of pack reference entries to the available list.  prp may
be NULL, in which case nothing is done.
*/
{
  a_pack_reference_ptr	prp_tail;
  if (prp != NULL) {
    /* Find the last entry on the list. */
    prp_tail = prp;
    while (prp_tail->next != NULL) prp_tail = prp_tail->next;
    /* Add the current available list to the end of the list passed by the
       caller. */
    prp_tail->next = avail_pack_references;
    avail_pack_references = prp;
  }  /* if */
}  /* free_list_of_pack_references */


/*
Variable that records the number of initial scans of, e.g., a lambda
parameter list that have begun but not yet completed.  If "auto" parameters
are detected, the scan must be repeated as a generic lambda.  In that case,
pack expansions recorded during that initial scan must be discarded.  Pack
expansion descriptors are marked with the value of this variable to
facilitate identification of the descriptors to be discarded.  If a second
parse is not required, the value in each such descriptor is reset to 0
after the initial parse.
*/
STATIC_THREAD int
                depth_tentative_pack_expansions;


void begin_tentative_pack_expansion_context(void)
/*
We are about to begin the initial scan of a function declarator that might
add pack expansions that must be reverted.  If we are in a template
dependent context, begin marking new pack expansion descriptors as
associated with this scan, to allow them to be identified for removal if
necessary, and save the current value of last_pack_expansion_used in each
instantiation scope for possible restoration.
*/
{
  if (depth_innermost_instantiation_scope != NO_SCOPE_DEPTH) {
    a_scope_stack_entry_ptr ssep;
    for (ssep = &scope_stack[depth_innermost_instantiation_scope];
         ssep != NULL; ssep = previous_scope_of(ssep)) {
      if (ssep->kind == sck_template_instantiation) {
        ssep->saved_last_pack_expansion_used = ssep->last_pack_expansion_used;
      }  /* if */
    }  /* for */
    ++depth_tentative_pack_expansions;
  }  /* if */
}  /* begin_tentative_pack_expansion_context */


a_pack_expansion_descr_ptr alloc_pack_expansion_descr(void)
/*
Allocate a new pack expansion descriptor, initialize it, and return a pointer
to it.
*/
{
  a_pack_expansion_descr_ptr	pedp;

  if (avail_pack_expansion_descrs != NULL) {
    /* Reuse an existing entry. */
    pedp = avail_pack_expansion_descrs;
    avail_pack_expansion_descrs = avail_pack_expansion_descrs->next;
  } else {
    /* Allocate a new entry. */
    pedp = alloc_fe_of_type(a_pack_expansion_descr);
#if DEBUG
   num_pack_expansion_descrs_allocated++;
#endif /* DEBUG */
  }  /* if */
  pedp->next = NULL;
  pedp->previous = NULL;
  pedp->first_token = NO_TOKEN_SEQUENCE_NUMBER;
  pedp->last_token = NO_TOKEN_SEQUENCE_NUMBER;
  pedp->packs_referenced = NULL;
  pedp->ellipsis_position = null_source_position;
  pedp->param_symbol_header = NULL;
  pedp->param_symbol_type = NULL;
  pedp->param_symbol_is_template_template = FALSE;
  pedp->ellipsis_seen = FALSE;
  pedp->is_function_declarator = FALSE;
  pedp->uses_only_enclosing_packs = FALSE;
  pedp->uses_any_enclosing_packs = FALSE;
  pedp->is_pack_index = FALSE;
  pedp->tentative_pack_expansion_depth = (scope_stack_top().in_tentative_decl ?
                                          depth_tentative_pack_expansions : 0);
  return pedp;
}  /* alloc_pack_expansion_descr */


static void free_pack_expansion_descr(a_pack_expansion_descr_ptr	pedp)
/*
Return the pack expansion descriptor pedp to the available list.
*/
{
  pedp->next = avail_pack_expansion_descrs;
  /* The previous pointer is not maintained on the available list. */
  pedp->previous = NULL;
  avail_pack_expansion_descrs = pedp;
}  /* free_pack_expansion_descr */


void end_tentative_pack_expansion_context(a_boolean remove_descriptors)
/*
We have just finished the initial scan of a function declarator that might
have added pack expansions that must be reverted.  If we are in a template
dependent context, scan the list of pack expansion descriptors in each
instantiation scope on the scope stack.  For each descriptor that matches
the current tentative pack expansion depth, either remove the descriptor
from the list (if remove_descriptors is TRUE) or reset its depth to 0.
Also, if remove_descriptors is TRUE, restore the value of
last_pack_expansion_used in each affected instantiation scope stack entry.
*/
{
  if (depth_innermost_instantiation_scope != NO_SCOPE_DEPTH) {
    a_scope_stack_entry_ptr    ssep;
    a_pack_expansion_descr_ptr pedp;
    for (ssep = &scope_stack[depth_innermost_instantiation_scope];
         ssep != NULL; ssep = previous_scope_of(ssep)) {
      if (ssep->kind == sck_template_instantiation) {
        a_boolean change_made = FALSE;
        pedp = ssep->template_decl_info->pack_expansions;
        while (pedp != NULL) {
          if (pedp->tentative_pack_expansion_depth >=
                                             depth_tentative_pack_expansions) {
            check_assertion(pedp->tentative_pack_expansion_depth ==
                                              depth_tentative_pack_expansions);
            change_made = TRUE;
            if (remove_descriptors) {
              /* Remove the descriptor from the list. */
              a_pack_expansion_descr_ptr old_pedp;
              if (pedp->previous == NULL) {
                ssep->template_decl_info->pack_expansions = pedp->next;
              } else {
                pedp->previous->next = pedp->next;
              }  /* if */
              if (pedp->next == NULL) {
                ssep->template_decl_info->last_pack_expansion = pedp->previous;
              } else {
                pedp->next->previous = pedp->previous;
              }  /* if */
              old_pedp = pedp;
              pedp = pedp->next;
              free_pack_expansion_descr(old_pedp);
            } else {
              /* Just mark the descriptor as no longer tentative. */
              pedp->tentative_pack_expansion_depth = 0;
              pedp = pedp->next;
            }  /* if */
          } else {
            pedp = pedp->next;
          }  /* if */
        }  /* while */
        if (remove_descriptors && change_made) {
          /* Restore the value of last_pack_expansion_used to its value
             before the initial parse. */
          ssep->last_pack_expansion_used =
                                          ssep->saved_last_pack_expansion_used;
        }  /* if */
      }  /* if */
    }  /* for */
    --depth_tentative_pack_expansions;
  }  /* if */
}  /* end_tentative_pack_expansion_context */


static a_pack_instantiation_descr_ptr alloc_pack_instantiation_descr(void)
/*
Allocate a new pack instantiation descriptor, initialize it, and return a
pointer to it.
*/
{
  a_pack_instantiation_descr_ptr	pidp;

  if (avail_pack_instantiation_descrs != NULL) {
    /* Reuse an existing entry. */
    pidp = avail_pack_instantiation_descrs;
    avail_pack_instantiation_descrs = avail_pack_instantiation_descrs->next;
  } else {
    /* Allocate a new entry. */
    pidp = alloc_fe_of_type(a_pack_instantiation_descr);
#if DEBUG
   num_pack_instantiation_descrs_allocated++;
#endif /* DEBUG */
  }  /* if */
  pidp->next = NULL;
  pidp->pack_status = NULL;
  pidp->after_first_element = FALSE;
  pidp->is_empty = FALSE;
  pidp->has_hybrid_pack_expansion = FALSE;
  return pidp;
}  /* alloc_pack_instantiation_descr */


static
void free_pack_instantiation_descr(a_pack_instantiation_descr_ptr	pidp)
/*
Return the pack instantiation descriptor pidp to the available list.
*/
{
  /* If the entry points to any pack references, free those. */
  free_list_of_pack_references(pidp->pack_status);
  pidp->next = avail_pack_instantiation_descrs;
  avail_pack_instantiation_descrs = pidp;
}  /* free_pack_instantiation_descr */


static a_pack_expansion_stack_entry_ptr alloc_pack_expansion_stack_entry(void)
/*
Allocate a new pack expansion stack entry, initialize it, and return a pointer
to it.
*/
{
  a_pack_expansion_stack_entry_ptr	pesep;

  if (avail_pack_expansion_stack_entries != NULL) {
    /* Reuse an existing entry. */
    pesep = avail_pack_expansion_stack_entries;
    avail_pack_expansion_stack_entries =
                                      avail_pack_expansion_stack_entries->next;
  } else {
    /* Allocate a new entry. */
    pesep = alloc_fe_of_type(a_pack_expansion_stack_entry);
#if DEBUG
   num_pack_expansion_stack_entries_allocated++;
#endif /* DEBUG */
  }  /* if */
  pesep->next = NULL;
  pesep->expansion_descr = NULL;
  pesep->instantiation_descr = NULL;
  new (&pesep->first_token_cache) a_reusable_token_cache();
  pesep->first_token_tsn = NO_TOKEN_SEQUENCE_NUMBER;
  pesep->template_arg_list = NULL;
  pesep->is_rescan = FALSE;
  pesep->is_deduction = FALSE;
  pesep->is_suppression = FALSE;
  pesep->expansion_with_no_packs_diagnostic_issued = FALSE;
  pesep->is_lookahead = FALSE;
  pesep->enclosing_packs_reset = FALSE;
  pesep->preserve_deduced_packs = FALSE;
  pesep->contains_pack_reference = FALSE;
  return pesep;
}  /* alloc_pack_expansion_stack_entry */


static a_pack_expansion_stack_entry_ptr push_pack_expansion_stack(void)
/*
Push a new entry on the pack expansion stack.  Return a pointer to the
new stack entry.
*/
{
  a_pack_expansion_stack_entry_ptr	pesep;

  pesep = alloc_pack_expansion_stack_entry();
  pesep->next = pack_expansion_stack;
  pack_expansion_stack = pesep;
  return pesep;
}  /* push_pack_expansion_stack */


static void pop_pack_expansion_stack(void)
/*
Pop the current entry off of the pack expansion stack.
*/
{
  a_pack_expansion_stack_entry_ptr	pesep;

  pesep = pack_expansion_stack;
  /* Unlink this entry from the stack. */
  pack_expansion_stack = pesep->next;
  /* Destroy the first_token_cache object. */
  pesep->first_token_cache.~a_reusable_token_cache();
  /* If there is an instantiation entry, free it now. */
  if (pesep->instantiation_descr != NULL) {
    free_pack_instantiation_descr(pesep->instantiation_descr);
  }  /* if */
  if (pesep->template_arg_list != NULL) {
    free_template_arg_list(pesep->template_arg_list);
  }  /* if */
  if (pesep->expansion_descr != NULL) {
    /* Restore the previous values of the parameter packs used by this
       expansion. */
    restore_enclosing_pack_values(pesep->enclosing_packs_reset);
  }  /* if */
  /* Add the old entry to the list of available stack entries. */
  pesep->next = avail_pack_expansion_stack_entries;
  avail_pack_expansion_stack_entries = pesep;
  pesep = pack_expansion_stack;
  if (pesep != NULL && !pesep->is_rescan && !pesep->is_deduction) {
    if (pesep->enclosing_packs_reset) {
      reset_enclosing_pack_values();
    }  /* if */
  }  /* if */
}  /* pop_pack_expansion_stack */

#if DEBUG

static inline a_boolean is_pack_within_cache(a_token_cache_ptr          cache,
                                             a_pack_expansion_descr_ptr pedp)
/*
Return TRUE if the given pack expansion descriptor's token range is contained
within the given token cache; otherwise, return FALSE.
*/
{
  a_boolean result = TRUE;

  if (cache->is_empty()) {
    result = FALSE;
  } else {
    const a_shared_token &first_tok = cache->get_first_token();
    const a_shared_token &last_tok = cache->get_last_token();

    if (first_tok->get_starting_seq_number() > pedp->first_token) {
      result = FALSE;
    } else if (last_tok->get_starting_seq_number() < pedp->last_token) {
      result = FALSE;
    }  /* if */
  }  /* if */
  return result;
}  /* is_pack_within_cache */


void db_pack_tokens(a_pack_expansion_descr_ptr	pedp)
/*
Display the tokens that make up a pack expansion, for debugging purposes.
*/
{
  a_scope_stack_entry_ptr	ssep;
  a_symbol_ptr			template_sym;
  a_token_cache_ptr		result_cache = NULL;

  if (is_prototype_instantiation_context()) {
    ssep = get_innermost_template_dependent_context();
  } else {
    ssep = scope_stack_entry_for(depth_innermost_instantiation_scope);
    check_assertion(ssep != NULL);
  }  /* if */
  template_sym = ssep->template_sym;
  if (template_sym != NULL) {
    a_template_symbol_supplement_ptr	tssp;
    a_token_cache_ptr			cache;
    tssp = template_supplement_for_symbol(template_sym);
    /* The tokens could come from the body cache or the declaration cache.
       They could presumably also come from something like a default
       argument cache, but that is not supported by this routine. */
    cache = tssp->cache->tokens.ptr();
    if (is_pack_within_cache(cache, pedp)) {
      /* Use this cache. */
      result_cache = cache;
    } else if (is_function_or_template_symbol(template_sym)) {
      /* Try the declaration cache for a function template. */
      cache = tssp->variant.function.decl_cache->tokens.ptr();
      if (is_pack_within_cache(cache, pedp)) {
        /* Use this cache. */
        result_cache = cache;
      }  /* if */
    }  /* if */
    if (result_cache != NULL) {
      init_token_string(result_cache->get_first_token()->get_source_position(),
                        /*keep_spacing=*/FALSE,
                        /*suppress_identifier_wrapping=*/FALSE);
      add_token_cache_segment_to_string(result_cache, pedp->first_token,
                                        pedp->last_token);
      fprintf(f_debug, "%s\n", token_string());
    }  /* if */
  }  /* if */
}  /* db_pack_tokens */

#endif /* DEBUG */


static a_boolean pack_expansion_descr_is_on_stack(
                                              a_pack_expansion_descr_ptr  pedp)
/*
Return TRUE if the given pack expansion descriptor appears on the current pack
expansion stack.
*/
{
  a_pack_expansion_stack_entry_ptr  pesep;
  a_boolean                         on_stack = FALSE;

  for (pesep = pack_expansion_stack; pesep != NULL; pesep = pesep->next) {
    if (!pesep->is_suppression && pesep->expansion_descr == pedp) {
      on_stack = TRUE;
      break;
    }  /* if */
  }  /* for */
  return on_stack;
}  /* pack_expansion_descr_is_on_stack */


static a_pack_expansion_descr_ptr get_pack_expansion_for_curr_context(void)
/*
Look for a pack expansion descriptor for the current token location.
This is used during the actual instantiation of a variadic template to
determine whether we are entering a pack expansion context.
*/
{
  a_pack_expansion_descr_ptr	result_pedp = NULL;
  a_pack_expansion_descr_ptr	pedp;
  a_scope_stack_entry_ptr	ssep;

#if DEBUG
  if (db_flag_is_set("packs")) {
    fprintf(f_debug, "Looking for pack expansion at TSN %ld\n",
            (long)curr_token_sequence_number);
  }  /* if */
#endif /* DEBUG */
  for (ssep = scope_stack_entry_for(depth_innermost_instantiation_scope);
       ssep != NULL; ssep = previous_scope_of(ssep)) {
    if (ssep->kind == (a_scope_kind)sck_template_instantiation) {
      pedp = ssep->last_pack_expansion_used;
      /* Move forward or backward in the list of pack expansions to attempt to
         find one that matches the current token sequence number. */
      for (; pedp != NULL && pedp->first_token < curr_token_sequence_number;
           pedp = pedp->next) {}
      for (; pedp != NULL && pedp->first_token > curr_token_sequence_number;
           pedp = pedp->previous) {}
      if (pedp != NULL && pedp->first_token == curr_token_sequence_number) {
        /* Multiple expansions can share a first token (e.g., nested contexts
           that begin at the same source location).  Pick one based on range
           nesting and the active expansion stack, so that the outermost
           expansion is selected first and inner expansions are selected when
           entered recursively. */
        a_pack_expansion_descr_ptr first_same = pedp;
        a_pack_expansion_descr_ptr cand, best = NULL;
        a_boolean                  have_bound = FALSE;
        a_token_sequence_number    bound_last = NO_TOKEN_SEQUENCE_NUMBER;
        a_pack_expansion_stack_entry_ptr
                                   pesep;

        while (first_same->previous != NULL &&
               first_same->previous->first_token ==
                                                  curr_token_sequence_number) {
          first_same = first_same->previous;
        }  /* while */
        /* If we are already inside a same-token expansion, constrain the
           candidate to be nested within the innermost active one. */
        for (pesep = pack_expansion_stack;
             pesep != NULL;
             pesep = pesep->next) {
          if (!pesep->is_suppression &&
              pesep->expansion_descr != NULL &&
              pesep->expansion_descr->first_token ==
                                                  curr_token_sequence_number) {
            if (!have_bound ||
                pesep->expansion_descr->last_token < bound_last) {
              bound_last = pesep->expansion_descr->last_token;
              have_bound = TRUE;
            }  /* if */
          }  /* if */
        }  /* for */
        for (cand = first_same;
             cand != NULL && cand->first_token == curr_token_sequence_number;
             cand = cand->next) {
          if (cand->packs_referenced != NULL &&
              !pack_expansion_descr_is_on_stack(cand) &&
              (!have_bound || cand->last_token < bound_last) &&
              (best == NULL || best->last_token < cand->last_token)) {
            /* Prefer the outermost descriptor among those available. */
            best = cand;
          }  /* if */
        }  /* for */
        if (best != NULL) {
          pedp = best;
        } else if (pedp->packs_referenced == NULL) {
          /* The matching same-token entries are non-pack contexts. */
          pedp = NULL;
        }  /* if */
        if (pedp != NULL) {
          /* Record the most recent pack used. */
          ssep->last_pack_expansion_used = pedp;
          result_pedp = pedp;
          break;
        }
      }  /* if */
    }  /* if */
  }  /* for */
#if DEBUG
  if (db_flag_is_set("packs")) {
    if (result_pedp != NULL) {
      fprintf(f_debug, "Found pack expansion from %ld to %ld\n",
            (long)result_pedp->first_token, (long)result_pedp->last_token);
      db_pack_tokens(result_pedp);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  return result_pedp;
}  /* get_pack_expansion_for_curr_context */


a_boolean is_non_initial_variadic_element(void)
/*
Return TRUE if we are currently in the 2nd through Nth element of
the current pack.
*/
{
  a_boolean				result = FALSE;
  a_pack_expansion_stack_entry_ptr	pesep;

  pesep = pack_expansion_stack;
  if (pesep != NULL) {
    if (pesep->instantiation_descr != NULL) {
      result = pesep->instantiation_descr->after_first_element;
    }  /* if */
  }  /* if */
  return result;
}  /* is_non_initial_variadic_element */


static void get_curr_template_params_and_args(
				a_template_param_ptr	*templ_param_list,
				a_template_arg_ptr	*templ_arg_list)
/*
This routine can be called within a template instantiation context to
return the template parameter list and template argument list of the
template that is being instantiated.
*/
{
  a_template_decl_info_ptr	tdip;
  a_scope_stack_entry_ptr	ssep;

  check_assertion(depth_innermost_instantiation_scope != NO_SCOPE_DEPTH);
  ssep = &scope_stack[depth_innermost_instantiation_scope];
  tdip = ssep->template_decl_info;
  *templ_param_list = tdip->parameters;
  check_assertion(*templ_param_list != NULL);
  *templ_arg_list = ssep->template_arg_list;
}  /* get_curr_template_params_and_args */


static void get_enclosing_template_params_and_args(
				a_template_param_ptr	*templ_param_list,
				a_template_arg_ptr	*templ_arg_list)
/*
This routine can be called within a template instantiation context to
return the template parameter list and template argument list of a
template that is being instantiated.  The template parameter list and
argument list are expected to be from one of the instantiations that
encloses the current context.  If no such instantiation exists,
NULL template parameter list and argument lists are returned.
*/
{
  a_template_decl_info_ptr	tdip;
  a_scope_stack_entry_ptr	ssep;
  a_boolean			curr_scope_found = FALSE;
  a_template_param_ptr		curr_param_list = *templ_param_list;

  *templ_param_list = NULL;
  *templ_arg_list = NULL;
  for (ssep = scope_stack_entry_for(depth_innermost_instantiation_scope);
       ssep != NULL;
       ssep = previous_scope_of(ssep)) {
    if (ssep->kind != (a_scope_kind)sck_template_instantiation) {
      /* We only need to inspect instantiation scopes. */
      continue;
    } else if (curr_scope_found) {
      /* We should return the values for this scope below. */
    } else if (ssep->template_decl_info->parameters == curr_param_list) {
      /* We found the scope that was currently being inspected.  We want
         to return the next one found. */
      curr_scope_found = TRUE;
      continue;
    } else if (!curr_scope_found) {
      /* This is a scope we have already inspected, and not the most recent
         one -- ignore it. */
      continue;
    }  /* if */
    tdip = ssep->template_decl_info;
    *templ_param_list = tdip->parameters;
    check_assertion(*templ_param_list != NULL);
    if (ssep->template_arg_list != NULL) {
      *templ_arg_list = ssep->template_arg_list;
    } else {
      /* If we are in the midst of substituting into the type of a
         just-deduced template instance, the argument list will be found
         in deduced_template_args. */
      *templ_arg_list = ssep->deduced_template_args;
    }  /* if */
    check_assertion(*templ_arg_list != NULL);
    break;
  }  /* for */
}  /* get_enclosing_template_params_and_args */


static a_template_arg_ptr find_template_arg_for_pack(
				a_template_param_ptr	templ_param_list,
				a_template_arg_ptr	templ_arg_list,
				a_symbol_ptr		sym,
				uint32_t		*elements,
				a_template_param_ptr	*template_param,
				a_boolean		*found,
				a_boolean		is_rescan,
				a_boolean		is_deduction)
/*
Find the initial template argument (from templ_arg_list) associated
with the pack specified by sym, which is a template parameter symbol
from templ_param_list.  If there are no actual arguments for the pack,
return NULL.  Return the associated template parameter in *template_param.
This is returned even when the function returns NULL.  Return the number of
actual arguments in *elements.  Set *found to TRUE if the pack specified
was found in templ_param_list and to FALSE otherwise.

is_deduction is TRUE if the pack instantiation is being created as
part of the deduction of the pack argument values.  is_rescan is TRUE
if the pack instantiation is being created as part of an expression
rescan.
*/
{
  a_template_arg_ptr	result_tap = NULL;
  a_template_arg_ptr	tap;
  a_template_param_ptr	tpp;

  *found = FALSE;
  *elements = 0;
  *template_param = NULL;
  begin_special_variadic_template_arg_list_traversal(
                                 templ_param_list, templ_arg_list, &tpp, &tap);
  for (; tap != NULL && tpp != NULL;
       special_variadic_advance_to_next_template_arg(&tpp, &tap)) {
    if (tpp->param_symbol->token_sequence_number ==
                                                  sym->token_sequence_number) {
      check_assertion(tpp->param_symbol->header == sym->header);
      /* If this is a start of pack placeholder, advance to the next
         argument. */
      if (is_start_of_pack_expansion_templ_arg(tap)) {
        tap = tap->next;
        /* If the next argument is also a start of pack, this argument is
           empty. */
        if (tap != NULL && is_start_of_pack_expansion_templ_arg(tap)) {
          tap = NULL;
        }  /* if */
      }  /* if */
      result_tap = tap;
      *template_param = tpp;
      *found = TRUE;
      /* Compute the number of pack elements. */
      for (; tap != NULL && tap->is_pack_element; tap = tap->next) {
        (*elements)++;
      }  /* for */
      break;
    }  /* if */
  }  /* for */
  if (!*found && !is_deduction) {
    /* The immediate instantiation context does not have the specified
       template parameter.  Look in an enclosing context. */
    get_enclosing_template_params_and_args(&templ_param_list,
                                           &templ_arg_list);
    if (templ_arg_list != NULL) {
      result_tap = find_template_arg_for_pack(templ_param_list, templ_arg_list,
                                              sym, elements, template_param,
                                              found, is_rescan, is_deduction);
    }  /* if */
  }  /* if */
  return result_tap;
}  /* find_template_arg_for_pack */


a_boolean pack_expansion_maps_to_unexpanded_pack(
                                  a_pack_expansion_descr_ptr  pedp,
                                  a_template_param_ptr        templ_param_list,
                                  a_template_arg_ptr          templ_arg_list)
/*
Return TRUE if pedp refers to a template parameter pack whose corresponding
argument in templ_arg_list is itself still a parameter pack rather than a
list of expanded elements.
*/
{
  a_boolean             result = FALSE;
  a_pack_reference_ptr  prp;

  for (prp = pedp->packs_referenced; prp != NULL; prp = prp->next) {
    if (prp->kind == prk_template_param && !prp->uses_enclosing_pack) {
      uint32_t              elements;
      a_template_arg_ptr    tap;
      a_template_param_ptr  tpp;
      a_boolean             found = FALSE;
      tap = find_template_arg_for_pack(templ_param_list, templ_arg_list,
                                       prp->symbol, &elements, &tpp, &found,
                                       /*is_rescan=*/TRUE,
                                       /*is_deduction=*/FALSE);
      if (found && tap != NULL && tap->is_pack) {
        result = TRUE;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return result;
}  /* pack_expansion_maps_to_unexpanded_pack */


static a_param_id_ptr find_parameter_for_pack(
				a_pack_reference_ptr	prp,
				uint32_t		*elements)
/*
Return the initial function parameter associated with the variadic
function template currently being instantiated.  prp describes
the parameter from the prototype instantiation.  If there are no 
actual arguments for the pack, return NULL.  Return the number of actual
arguments in *elements.
*/
{
  a_param_id_ptr		result_param_id = NULL;
  a_scope_stack_entry_ptr	ssep;
  uint32_t			param_num = prp->param_or_binding_num;
  a_param_id_ptr		param_id = NULL;

  /* Look through any enclosing function prototype scopes for the
     parameter. */
  ssep = &scope_stack_top();
  check_assertion(ssep->kind == (a_scope_kind)sck_func_prototype);
  for (; ssep != NULL && ssep->kind == (a_scope_kind)sck_func_prototype;
       ssep = previous_scope_of(ssep)) {
    for (param_id = ssep->param_id_list;
         param_id != NULL; param_id = param_id->next) {
      if (param_id->param_num == param_num) {
        result_param_id = param_id;
        break;
      }  /* if */
    }  /* for */
    if (result_param_id != NULL) break;
  }  /* for */
  *elements = 0;
  /* Count the number of pack elements. */
  for (; param_id != NULL && param_id->is_pack_element;
       param_id = param_id->next) {
    (*elements)++;
  }  /* for */
  return result_param_id;
}  /* find_parameter_for_pack */


static a_scope_stack_entry_ptr skip_function_or_block_scopes(
                                       a_scope_stack_entry_ptr  ssep,
                                       uint32_t                 scopes_to_skip)
/*
Starting from ssep, skip over scopes_to_skip function or block scopes and
return the resulting scope stack entry.  This is used when looking up the scope
containing a function parameter pack or structured binding pack.
*/
{
  if (scope_is(ssep, sck_template_instantiation) && ssep->is_rescan) {
    /* We might run into a pack expansion during SFINAE checking.  In that
       case, skip the sck_template_instantiation and sck_instantiation_context
       scopes that are on the top of the stack.  For example:

         template<typename... T> bool g(T... t) {
           return requires { f(t...); };
         }
         int r = g();

       While (re)scanning the requires clause the two scopes on top of the
       stack are sck_template_instantiation and sck_instantiation_context
       (with their associated "previous scope" being the file scope). */
    ssep -= 1;
    if (scope_is(ssep, sck_instantiation_context) && ssep->is_rescan) {
      ssep -= 1;
    }  /* if */
  }  /* if */
  /* Bypass the number of function or block scopes indicated by scopes_to_skip.
     We can't start with depth_innermost_function_scope because that is cleared
     if we are in a local class. */
  for (; ssep != NULL; ssep = previous_scope_of(ssep)) {
    /* Only consider function and block scopes. */
    if (scope_is(ssep, sck_function) || scope_is(ssep, sck_block)) {
      if (scopes_to_skip == 0) break;
      scopes_to_skip--;
    }  /* if */
  }  /* for */
  return ssep;
}  /* skip_function_or_block_scopes */


static a_variable_ptr find_variable_for_pack(
				a_pack_reference_ptr	prp,
				uint32_t		*elements)
/*
Return the initial function parameter associated with the variadic function
template currently being instantiated.  prp describes the parameter from the
prototype instantiation.  If there are no actual arguments for the pack, return
NULL.  Return the number of actual arguments in *elements.

This routine is only expected to be called in the context of a function
template instantiation of a variadic template or some other context considered
to be part of the function template such as a lambda nested therein.
*/
{
  a_scope_stack_entry_ptr	ssep = &scope_stack_top();
  a_variable_ptr		vp;
  a_variable_ptr		result_vp = NULL;
  uint32_t			param_num = prp->param_or_binding_num;

  ssep = skip_function_or_block_scopes(
                                  ssep, prp->function_or_block_scopes_to_skip);
  if (ssep == NULL) {
    expect_error();
    goto done;
  }  /* if */
  check_assertion(ssep->kind == sck_function);
  for (vp = ssep->il_scope->variant.routine.parameters;
       vp != NULL; vp = vp->next) {
    if (vp->variant.assoc_param_type->param_num == param_num) {
      result_vp = vp;
      break;
    }  /* if */
  }  /* for */
  *elements = 0;
  /* Count the number of pack elements. */
  for (; vp != NULL; vp = vp->next) {
    if (vp->variant.assoc_param_type->param_num != param_num) break;
    (*elements)++;
  }  /* for */
done:
  return result_vp;
}  /* find_variable_for_pack */


static a_variable_ptr find_binding_for_pack(a_pack_reference_ptr  prp,
                                            uint32_t              *elements)
/*
Return the initial binding pack element associated with the structured binding
pack currently being instantiated.  prp describes the structured binding pack
from the prototype instantiation.  If there are no actual bindings for the
pack, return NULL.  Return the number of actual bindings in *elements.
*/
{
  a_scope_stack_entry_ptr	ssep = &scope_stack_top();
  a_variable_ptr		vp;
  a_variable_ptr		result_vp = NULL;
  uint32_t			binding_num = prp->param_or_binding_num;

  ssep = skip_function_or_block_scopes(
                                  ssep, prp->function_or_block_scopes_to_skip);
  if (ssep == NULL) {
    expect_error();
    goto done;
  }  /* if */
  if (prp->symbol->variant.variable.ptr->storage_class == sc_static) {
    vp = ssep->il_scope->variables;
  } else {
    vp = ssep->il_scope->nonstatic_variables;
  }  /* if */
  while (vp != NULL) {
    if (vp->is_struct_binding_container) {
      --binding_num;
      if (binding_num == 0) break;
    }  /* if */
    vp = vp->next;
  }  /* while */
  *elements = 0;
  if (vp != NULL) {
    /* Count the number of pack elements. */
    an_il_entity_list_entry_ptr  ielep = vp->variant.bindings;
    for (; ielep != NULL; ielep = ielep->next) {
      vp = (a_variable_ptr)ielep->entity.ptr;
      if (vp->is_pack_element) {
        if (result_vp == NULL) result_vp = vp;
        (*elements)++;
      }  /* if */
    }  /* for */
  }  /* if */
done:
  return result_vp;
}  /* find_binding_for_pack */


static a_field_ptr find_init_capture_for_pack(
				a_pack_reference_ptr	prp,
				uint32_t		*elements)
/*
Return the initial field associated with the variadic init-capture for
the lambda body being evaluated.  prp describes the symbol that was
referenced from the prototype instantiation.  If there are no actual
fields for the pack, return NULL.  Return the number of actual arguments
in *elements.
*/
{
  a_scope_stack_entry_ptr	ssep;
  a_field_ptr			fp = NULL;
  a_field_ptr			result_fp = NULL;
  a_const_char			*capture_name;

  capture_name = prp->symbol->header->identifier;
  /* For any scope stack entries for closure classes, look for a field with
     the name of the pack being referenced. */
  for (ssep = &scope_stack_top(); ssep != NULL;
       ssep = previous_scope_of(ssep)) {
    /* Only consider class scopes for lambdas. */
    a_type_ptr	tp = ssep->assoc_type;
    if ((scope_is(ssep, sck_class_struct_union) ||
         scope_is(ssep, sck_class_reactivation)) &&
        tp != NULL &&
        class_type_supp(tp)->is_lambda_closure_class) {
      for (fp = tp->variant.class_struct_union.field_list; fp != NULL;
           fp = fp->next) {
        a_const_char *field_name = fp->source_corresp.name;
        if (field_name != NULL && strcmp(capture_name, field_name) == 0) {
          result_fp = fp;
          break;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* for */
  *elements = 0;
  if (result_fp != NULL) {
    /* Count the number of pack elements. */
    a_const_char *field_name = result_fp->source_corresp.name;
    for (fp = result_fp; fp != NULL; fp = fp->next) {
      if (fp->source_corresp.name != field_name) break;
      (*elements)++;
    }  /* for */
  }  /* if */
  return result_fp;
}  /* find_init_capture_for_pack */


static a_template_arg_ptr find_placeholder_arg_for_pack(
				a_template_param_ptr	templ_param_list,
				a_template_arg_ptr	templ_arg_list,
				a_symbol_ptr		sym)
/*
Find the placeholder template argument (from templ_arg_list) associated
with the pack specified by sym, which is a template parameter symbol
from templ_param_list.
*/
{
  a_template_arg_ptr	result_tap = NULL;
  a_template_arg_ptr	tap;
  a_template_param_ptr	tpp;

  begin_special_variadic_template_arg_list_traversal(
                                 templ_param_list, templ_arg_list, &tpp, &tap);
  for (; tap != NULL;
         special_variadic_advance_to_next_template_arg(&tpp, &tap)) {
    if (tpp->param_symbol == sym) {
      result_tap = tap;
      break;
    }  /* if */
  }  /* for */
  check_assertion_str2(tap != NULL, "find_placeholder_arg_for_pack:",
                       "symbol not found");
  return result_tap;
}  /* find_placeholder_arg_for_pack */


static a_variadic_param_info_ptr find_variadic_param_info_for_pack(
				a_pack_reference_ptr	prp,
				a_ctws_state_ptr	ctws_state,
				uint32_t		*elements)
/*
Go through the variadic parameter info in ctws_state and return the
entry that corresponds to the function parameter represented by prp.
ctws_state is a substitution state block pointer, and can be NULL.
Return the number of actual arguments in *elements.
*/
{
  a_variadic_param_info_ptr	result_vpip = NULL;
  a_param_type_ptr		result_ptp = NULL;
  a_param_type_ptr		ptp;
  uint32_t			param_num;

  *elements = 0;
  /* ctws_state should only be NULL in contexts in which it is not possible
     to reference the parameter, so NULL is returned in that case. */
  if (ctws_state != NULL) {
    a_variadic_param_info_ptr	vpip;
    param_num = prp->symbol->variant.param_id->param_num;
    for (vpip = ctws_state->variadic_param_info; vpip != NULL;
         vpip = vpip->next) {
      if (vpip->orig_param_type->param_num == param_num &&
          vpip->orig_param_type->name != NULL &&
          strcmp(vpip->orig_param_type->name,
                 prp->symbol->header->identifier) == 0) {
        result_vpip = vpip;
        result_ptp = vpip->param_type;
        /* Continue searching for a closer (more recently added) pack. */
      }  /* if */
    }  /* for */
    if (result_ptp != NULL) {
      /* Count the number of pack elements. */
      for (ptp = result_ptp;
           ptp != NULL && ptp->is_pack_element && ptp->param_num == param_num;
           ptp = ptp->next) {
        (*elements)++;
      }  /* for */
    }  /* if */
  }  /* if */
  return result_vpip;
}  /* find_variadic_param_info_for_pack */


a_pack_reference_ptr copy_pack_reference(a_pack_reference_ptr	prp)
/*
Make a copy of prp, which is a pack reference from a prototype instantiation.
Return a pointer to the copy.
*/
{
  a_pack_reference_ptr	new_prp;

   new_prp = alloc_pack_reference(prp->kind);
   /* Copy the entire entry then clear the fields that should not be
      inherited. */
   *new_prp = *prp;
   new_prp->next = NULL;
   return new_prp;
}  /* copy_pack_reference */


a_pack_reference_ptr copy_pack_references_in_token_range(
                                          a_token_sequence_number  first_token,
                                          a_token_sequence_number  last_token)
/*
Return a list of copies of the pack references recorded in the current
context whose token sequence numbers fall in the range [first_token,
last_token].  This is used to save the pack references of a construct
(like a requires-expression) whose tokens will be skipped rather than
rescanned during instantiations; see rerecord_pack_references.
*/
{
  a_scope_stack_entry_ptr  ssep = get_scope_for_pack_references();
  a_pack_reference_ptr     prp, result = NULL, *p_tail = &result;

  /* The list is in token sequence number order. */
  for (prp = ssep->packs_referenced; prp != NULL; prp = prp->next) {
    if (prp->token_sequence_number > last_token) break;
    if (prp->token_sequence_number >= first_token) {
      *p_tail = copy_pack_reference(prp);
      p_tail = &(*p_tail)->next;
    }  /* if */
  }  /* for */
  return result;
}  /* copy_pack_references_in_token_range */


void rerecord_pack_references(a_pack_reference_ptr  prp)
/*
Re-record the pack references on the list pointed to by prp, which was
created by copy_pack_references_in_token_range when a construct was
originally parsed.  This is done when the tokens of the construct are
skipped during an instantiation so that an enclosing pack expansion
context sees the references even though the tokens that produced them
are not rescanned.
*/
{
  for (; prp != NULL; prp = prp->next) {
    record_potential_pack_reference(prp->symbol, &prp->position);
  }  /* for */
}  /* rerecord_pack_references */


static a_template_arg_ptr make_base_class_arg_list(
						a_type_ptr	class_type,
						a_boolean	direct_bases,
						uint32_t	*elements)
/*
Create a list of template arguments that represent the base classes of the
bases of class_type.  direct_bases is TRUE for the __direct_bases operator,
FALSE for the __bases operator.  Return the number of actual arguments in
*elements.
*/
{
  a_template_arg_ptr	arg_list = NULL;
  a_template_arg_ptr	last_arg = NULL;

  *elements = 0;
  class_type = skip_typerefs(class_type);
  /* The type might not be a class type in error cases. */
  if (is_immediate_class_type(class_type)) {
    a_base_class_ptr	bcp = base_classes_of(class_type);
    for (; bcp != NULL; bcp = bcp->next) {
      /* The __bases operator includes all bases, the __direct_bases only
         direct bases. */
      if (bcp->direct || !direct_bases) {
        a_template_arg_ptr	tap;
        tap = alloc_template_arg((a_templ_arg_kind)tak_type);
        tap->variant.type = bcp->type;
        tap->is_pack_element = TRUE;
        if (arg_list == NULL) arg_list = tap;
        if (last_arg != NULL) last_arg->next = tap;
        last_arg = tap;
        (*elements)++;
      }  /* if */
    }  /* for */
  }  /* if */
  return arg_list;
}  /* make_base_class_arg_list */


void add_pack_expansion_descr_to_prototype_arg(
					a_template_param_ptr	templ_param,
					a_template_arg_ptr	templ_arg)
/*
Create a pack expansion descriptor for the prototype instantiation template
parameter and argument specified by templ_param and templ_arg.  This is a
template argument that does not appear in the source code, but that should
have a pack expansion so that it can be expanded in a rescan context.
*/
{
  a_pack_expansion_descr_ptr	pedp;
  a_pack_reference_ptr		prp;

  pedp = alloc_pack_expansion_descr();
  prp = alloc_pack_reference(prk_template_param);
  prp->symbol = templ_param->param_symbol;
  prp->coordinates = coordinates_of_template_param(templ_param);
  pedp->packs_referenced = prp;
  templ_arg->pack_expansion_descr = pedp;
}  /* add_pack_expansion_descr_to_prototype_arg */


static a_boolean in_generic_lambda_in_real_instantiation(void)
/*
Return TRUE if we are in the definition of a generic lambda that is nested
inside a real instantiation.  In such contexts, a kind of "hybrid expansion"
is performed (i.e., an expansion based on real template arguments where some
template parameters may still be present in the result).
*/
{
  a_boolean			result = FALSE;
  a_boolean			lambda_found = FALSE;
  a_scope_stack_entry_ptr	ssep = &scope_stack_top();

  for (; ssep != NULL; ssep = previous_scope_of(ssep)) {
    /* Look for a generic lambda prototype instantiation and, if found, look
       for an enclosing instantiation scope. */
    if (scope_is(ssep, sck_template_instantiation)) {
      if (lambda_found) {
        result = !ssep->in_prototype_instantiation;
        break;
      } else if (ssep->is_generic_lambda && ssep->in_prototype_instantiation) {
        lambda_found = TRUE;
      }  /* if */
    }  /* if */
  }  /* for */
  return result;
}  /* in_generic_lambda_in_real_instantiation */


static a_pack_instantiation_descr_ptr create_pack_instantiation_descr(
		a_pack_expansion_descr_ptr		pedp,
		a_template_param_ptr			templ_param_list,
		a_template_arg_ptr			templ_arg_list,
		a_boolean				is_rescan,
		a_boolean				is_deduction,
		a_boolean				allow_empty_list,
		a_ctws_state_ptr			ctws_state,
		a_boolean				*err)
/*
We are beginning a real instantiation of the pack expansion specified
by "pedp".  Determine whether this is a non-empty expansion context and
whether all of the packs being expanded have the same number of elements.

templ_param_list and templ_arg_list are the template parameters and
arguments for the instantiation.  is_deduction is TRUE if the pack
instantiation is being created as part of the deduction of the pack
argument values.  is_rescan is TRUE if the pack instantiation is
being created as part of an expression rescan.  allow_empty_list is TRUE
if a pack instantiation entry should be created (for non-deduction cases)
even if there are no elements of the expansion.  ctws_state is a
substitution state block pointer, and can be NULL.

For non-deduction contexts, if this is a valid non-empty expansion,
establish the initial values of the parameter pack symbols and return
a pack expansion instantiation descriptor that can be used later to
advance to the next pack element for each symbol.

If the pack expansion is invalid (e.g., because there are packs of different
lengths) *err is set to TRUE, FALSE otherwise.
*/
{
  a_pack_reference_ptr			prp;
  a_pack_reference_ptr			new_pack_list = NULL;
  a_pack_reference_ptr			new_pack_tail = NULL;
  uint32_t				elements = 0;
  a_boolean				is_first_pack = TRUE;
  a_boolean				any_errors = FALSE;
  a_boolean				has_hybrid_pack_expansion = FALSE;
  a_pack_instantiation_descr_ptr	result_pidp = NULL;

  /* Go through the pack expansion references and determine the number
     of arguments for each.  Create a copy of the list to record information
     about the instantiation. */
  for (prp = pedp->packs_referenced; prp != NULL; prp = prp->next) {
    uint32_t			elements_for_pack = 0;
    a_pack_reference_ptr	new_prp;
    /* Create a copy of the pack reference entry and add it to the list of
       entries for this instantiation. */
    new_prp = copy_pack_reference(prp);
    if (new_pack_list == NULL) {
      new_pack_list = new_prp;
    } else {
      check_assertion(new_pack_tail != NULL);
      new_pack_tail->next = new_prp;
    }  /* if */
    new_pack_tail = new_prp;
    if (is_deduction) {
      a_template_arg_ptr	tap;
      a_template_arg_ptr	prev_tap;
      /* In a deduction context, we will deduce zero or more template
         argument values.  The curr_argument field of the pack element will
         be NULL until a value is deduced.   It is then cleared when
         the deduction of a given function argument has been completed.
         If an explicit argument list has been specified, deduction can
         be used to deduce additional elements of the pack.  If the
         reference is for an enclosing pack, ignore it here as it will
         have been substituted and can't be deduced. */
      if (prp->kind == prk_template_param && !prp->uses_enclosing_pack) {
        tap = find_placeholder_arg_for_pack(templ_param_list, templ_arg_list,
                                            prp->symbol);
        prev_tap = tap;
        tap = tap->next;
        if (tap != NULL && !tap->is_pack_element) {
          /* The next argument is not from the pack (i.e., the pack is
             empty) but there is some other argument that follows. */
          tap = NULL;
        } else {
          /* Skip over any explicitly specified pack elements. */
          while (tap != NULL && tap->is_pack_element &&
                 tap->explicitly_specified) {
            prev_tap = tap;
            tap = tap->next;
          }  /* while */
        }  /* if */
        /* If we reached an argument that is not part of the pack, it
           should be ignored.  Note that prev_tap is still set because we
           can deduce arguments that follow. */
        if (tap != NULL && !tap->is_pack_element) {
          tap = NULL;
        }  /* if */
        new_prp->prev_template_arg = prev_tap;
        new_prp->curr_argument.template_arg = tap;
      }  /* if */
    } else {
      /* In non-deduction contexts, find the current pack element to
         be used. */
      a_boolean  not_found = FALSE;
      if (prp->kind == prk_variable || prp->kind == prk_binding) {
        a_variable_ptr	vp;
        a_symbol_ptr	sym;
        if (prp->kind == prk_variable) {
          vp = find_variable_for_pack(prp, &elements_for_pack);
        } else {
          vp = find_binding_for_pack(prp, &elements_for_pack);
        }  /* if */
        if (vp != NULL) {
          sym = symbol_for(vp);
        } else {
          sym = NULL;
          not_found = TRUE;
        }  /* if */
        /* In some error cases the variable might not have a symbol.  Treat
           this as an empty pack (except that elements_for_pack is not
           changed). */
        check_assertion((vp == NULL) == (sym == NULL) ||
                        is_at_least_one_error());
        if (sym != NULL) {
          new_prp->curr_argument.variable = vp;
          new_prp->primary_pack_symbol = symbol_for(vp);
          new_prp->primary_pack_symbol->variant.variable.ptr = vp;
        } else {
          new_prp->primary_pack_symbol = NULL;
        }  /* if */
      } else if (prp->kind == prk_template_param) {
        a_template_arg_ptr	tap;
        a_template_param_ptr	tpp;
        a_boolean		found = FALSE;
        tap = find_template_arg_for_pack(templ_param_list, templ_arg_list,
                                         prp->symbol, &elements_for_pack,
                                         &tpp, &found, is_rescan,
                                         is_deduction);
        if (tap == NULL) not_found = TRUE;
        if (!found && ctws_state != NULL) ctws_state->unexpanded_pack = TRUE;
        new_prp->curr_argument.template_arg = tap;
        new_prp->template_param = tpp;
      } else if (prp->kind == prk_init_capture) {
        a_field_ptr	fp;
        a_symbol_ptr	sym;
        fp = find_init_capture_for_pack(prp, &elements_for_pack);
        if (fp != NULL) {
          sym = symbol_for(fp);
        } else {
          sym = NULL;
          not_found = TRUE;
        }  /* if */
        check_assertion((fp == NULL) == (sym == NULL) ||
                        is_at_least_one_error());
        if (sym != NULL) {
          new_prp->curr_argument.field = fp;
          new_prp->primary_pack_symbol = sym;
          new_prp->primary_pack_symbol->variant.field.ptr = fp;
        } else {
          new_prp->primary_pack_symbol = NULL;
        }  /* if */
      } else if (prp->kind == prk_bases) {
        /* A g++ __bases or __direct_bases operator. */
        a_template_arg_ptr	tap;
        a_template_param_ptr	tpp;
        a_boolean		found = FALSE;
        tap = find_template_arg_for_pack(templ_param_list, templ_arg_list,
                                         prp->symbol, &elements_for_pack,
                                         &tpp, &found, is_rescan,
                                         is_deduction);
        new_prp->curr_argument.template_arg =
                make_base_class_arg_list(tap->variant.type, prp->direct_bases,
                                         &elements_for_pack);
        new_prp->template_param = tpp;
      } else {
        check_assertion(prp->kind == prk_parameter);
        /* A parameter from a function prototype scope. */
        if (is_rescan) {
          /* In rescan contexts we look for the corresponding actual
             parameter in information saved by copy_type_with_substitution. */
          if (ctws_state != NULL) {
            a_variadic_param_info_ptr	vpip;
            vpip = find_variadic_param_info_for_pack(prp, ctws_state,
                                                     &elements_for_pack);
            check_assertion(is_rescan || vpip != NULL);
            if (vpip != NULL) {
              if (vpip->param_type != NULL &&
                  vpip->param_type->is_parameter_pack) {
                /* If we are checking a trailing requires clause, we might
                   still see an unexpanded pack here, which we now need to
                   expand. */
                a_boolean            copy_error = FALSE;
                a_ctws_state         local_ctws_state;
                a_param_type_ptr     new_param_type, subst_param_type;
                a_subst_pairs_array  subst_pairs = get_current_subst_pairs();

                subst_pairs.push_back(a_subst_pairs_descr{ templ_param_list,
                                                           templ_arg_list,
                                                           FALSE, FALSE,
                                                           FALSE, FALSE });
                local_ctws_state = *ctws_state;
                local_ctws_state.variadic_param_info = NULL;
                local_ctws_state.variadic_param_info_tail = NULL;
                local_ctws_state.unexpanded_pack = FALSE;
                /* vpip->param_type points into the function parameter list,
                   but as we only want to substitute a single parameter, make a
                   copy and clear its next pointer. */
                new_param_type = alloc_param_type(vpip->param_type->type);
                *new_param_type = *vpip->param_type;
                new_param_type->next = NULL;
                subst_param_type = param_types_after_substitutions(
                                                            new_param_type,
                                                            subst_pairs,
                                                            &prp->position,
                                                            CTWS_NO_OPTIONS,
                                                            &copy_error,
                                                            &local_ctws_state);
                free_param_type_list(new_param_type);
                free_list_of_variadic_param_info(
                                         local_ctws_state.variadic_param_info);
                elements_for_pack = 0;
                if (!local_ctws_state.unexpanded_pack) {
                  vpip->param_type = subst_param_type;
                  if (!copy_error) {
                    /* Count the number of pack elements. */
                    for (a_param_type_ptr ptp = vpip->param_type;
                         ptp != NULL && ptp->is_pack_element &&
                                   ptp->param_num == prp->param_or_binding_num;
                         ptp = ptp->next) {
                      ++elements_for_pack;
                    }  /* for */
                  }  /* if */
                } else {
                  free_param_type_list(subst_param_type);
                  ctws_state->unexpanded_pack = TRUE;
                }  /* if */
              }  /* if */
              new_prp->curr_argument.param_type = vpip->param_type;
            } else {
              new_prp->curr_argument.param_type = NULL;
              not_found = TRUE;
            }  /* if */
            new_prp->param_info = vpip;
          }  /* if */
        } else {
          /* When scanning from tokens, we look in the enclosing function
             prototype scopes. */
          a_param_id_ptr	param_id;
          param_id = find_parameter_for_pack(prp, &elements_for_pack);
          if (param_id != NULL) {
            new_prp->curr_argument.param_id = param_id;
            new_prp->primary_pack_symbol = param_id->symbol;
          } else {
            new_prp->primary_pack_symbol = NULL;
          }  /* if */
        }  /* if */
      }  /* if */
      /* Make sure the number of pack elements is consistent. */
      if (is_first_pack) {
        elements = elements_for_pack;
        is_first_pack = FALSE;
      } else if (elements != elements_for_pack) {
        if (in_generic_lambda_in_real_instantiation()) {
          /* Got an expansion from an enclosing real instantiation and an
             unexpanded pack from a nested generic lambda. */
          has_hybrid_pack_expansion = TRUE;
        } else if (!is_rescan) {
          pos_st2_error(ec_pack_length_mismatch, &prp->position,
                        prp->symbol->header->identifier,
                        pedp->packs_referenced->symbol->header->identifier);
          any_errors = TRUE;
        } else {
          /* A pack length mismatch is not an error when preserve_deduced_packs
             is TRUE (i.e., in rescan contexts for the first pass of
             substitution when deduction is still to be done) or when
             in_parent_substitution is TRUE and we found a pack reference for
             an "inner" pack that isn't found at this level. */
          any_errors = !(ctws_state->preserve_deduced_packs ||
                         (not_found && ctws_state->in_parent_substitution));
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  if (is_deduction || (!any_errors && (elements > 0 || allow_empty_list))) {
    /* There were no errors and there are pack elements to be expanded,
       this is a deduction context, or an empty list is allowed in this
       context.  Create an instantiation entry to be returned. */
    result_pidp = alloc_pack_instantiation_descr();
    result_pidp->pack_status = new_pack_list;
    result_pidp->is_empty = allow_empty_list && elements == 0;
    result_pidp->has_hybrid_pack_expansion = has_hybrid_pack_expansion;
  } else {
    /* There is no expansion to be done.  Free any pack references that may
       have been allocated. */
    free_list_of_pack_references(new_pack_list);
  }  /* if */
  *err = any_errors;
  return result_pidp;
}  /* create_pack_instantiation_descr */


static void skip_pack_expansion_tokens(a_pack_expansion_descr_ptr	pedp)
/*
We have reached a pack expansion for which there are no elements to be
expanded.  Advance to the token after the end of the pack expansion.
*/
{
  a_token_sequence_number	last_token = pedp->last_token;

  /* Advance to the last token of the expansion. */
  while (curr_token_sequence_number != last_token &&
         curr_token_sequence_number != NO_TOKEN_SEQUENCE_NUMBER &&
         curr_token != tok_end_of_source) {
    (void)get_token();
  }  /* while */
  /* Now go to the token after the expansion. */
  if (curr_token_sequence_number != last_token &&
      curr_token != tok_end_of_source) {
    (void)get_token();
  }  /* if */
}  /* skip_pack_expansion_tokens */


static void update_parameter_pack_symbol_values(
			a_pack_expansion_stack_entry_ptr	pesep)
/*
Update the symbols of any packs referenced to refer to the current
values specified by the instantiation arguments.  pesep points to the
pack expansion stack entry for which the symbols are to be updated.
*/
{
  a_pack_reference_ptr	param_prp;
  a_pack_reference_ptr	arg_prp;

  for (param_prp = pesep->expansion_descr->packs_referenced,
         arg_prp = pesep->instantiation_descr->pack_status;
       param_prp != NULL;
       param_prp = param_prp->next, arg_prp = arg_prp->next) {
    a_symbol_ptr	sym = param_prp->symbol;
    if (param_prp->kind == prk_template_param) {
      if (arg_prp->curr_argument.template_arg != NULL) {
        /* A template argument. */
        update_template_param_symbol(sym, arg_prp->curr_argument.template_arg);
      } else if (pesep->is_rescan && pesep->preserve_deduced_packs &&
                 !arg_prp->uses_enclosing_pack) {
        /* In a rescan context, if deduced packs should be preserved,
           set the template parameter symbol to point back to the template
           parameter value. */
        restore_default_template_param(arg_prp->template_param);
      } else {
        /* There is no argument -- set the symbol to an error value. */
        set_template_param_symbol_to_error(sym);
      }  /* if */
    } else if (param_prp->kind == prk_variable ||
               param_prp->kind == prk_binding) {
      if (arg_prp->primary_pack_symbol != NULL) {
        arg_prp->primary_pack_symbol->variant.variable.ptr =
                                               arg_prp->curr_argument.variable;
      }  /* if */
    } else if (param_prp->kind == prk_init_capture) {
      if (arg_prp->primary_pack_symbol != NULL) {
        arg_prp->primary_pack_symbol->variant.field.ptr =
                                               arg_prp->curr_argument.field;
      }  /* if */
    } else if (param_prp->kind == prk_bases) {
      /* There is nothing to be done for this case. */
    } else {
      check_assertion(param_prp->kind == prk_parameter);
      if (arg_prp->primary_pack_symbol != NULL) {
        arg_prp->primary_pack_symbol->variant.param_id =
                                               arg_prp->curr_argument.param_id;
      }  /* if */
    }  /* if */
  }  /* for */
}  /* update_parameter_pack_symbol_values */


a_type_ptr get_curr_variadic_param_type(an_expr_node_ptr	expr)
/*
This routine is called during rescan contexts to find the current type
for the enk_param_ref specified by expr.  If no matching parameter
is found, NULL is returned.
*/
{
  a_pack_reference_ptr			arg_prp = NULL;
  a_pack_expansion_stack_entry_ptr	pesep;
  a_type_ptr				result_type = NULL;

  check_assertion(expr->kind == (an_expr_node_kind)enk_param_ref);
  /* A reference to "this" can't match. */
  if (expr->variant.param_ref.param_num != 0) {
    /* Look up through the pack expansion stack to find a matching
       parameter. */
    for (pesep = pack_expansion_stack;
         pesep != NULL && !pesep->is_suppression &&
           pesep->instantiation_descr != NULL; pesep = pesep->next) {
      /* Only rescan contexts are considered. */
      if (!pesep->is_rescan) continue;
      for (arg_prp = pesep->instantiation_descr->pack_status;
           arg_prp != NULL; arg_prp = arg_prp->next) {
        if (arg_prp->kind == prk_parameter) {
          a_variadic_param_info_ptr	vpip;
          vpip = arg_prp->param_info;
          if (vpip != NULL && vpip->param_type != NULL &&
              vpip->orig_param_type->param_num ==
                                 (uint32_t)expr->variant.param_ref.param_num &&
              (unsigned int)vpip->level == expr->variant.param_ref.levels_up) {
            result_type = arg_prp->curr_argument.param_type->type;
            break;
          }  /* if */
        }  /* if */
      }  /* for */
      if (result_type != NULL) break;
    }  /* for */
  }  /* if */
  return result_type;
}  /* get_curr_variadic_param_type */


a_template_arg_ptr get_curr_variadic_arg_for_param(
			a_template_param_coordinate_ptr	coordinates,
			a_boolean			is_rescan,
			a_template_param_ptr		templ_param,
			a_boolean			create_if_not_found)
/*
This routine is called during rescan and deduction contexts.  We need the
current template argument value for the pack specified by coordinates,
which identifies the template parameter.  Go through the pack references
for the current expansions and look for one that matches coordinates.  Return
the current template argument value for that parameter.  is_rescan is TRUE if
this is called from a rescan/substitution context.  If there is not a matching
parameter, or if there is not a current argument, an argument is created
if create_if_not_found is TRUE.  Otherwise, NULL is returned.  templ_param
is the template parameter associated with the argument to be found,
and can be NULL only if create_if_not_found is FALSE.
*/
{
  a_pack_reference_ptr			param_prp = NULL;
  a_pack_reference_ptr			arg_prp = NULL;
  a_pack_expansion_stack_entry_ptr	pesep = pack_expansion_stack;
  a_template_arg_ptr			result_tap = NULL;

  /* The pack expansion stack could be NULL in certain error cases, or we
     could be in a suppression context. */
  if (pesep != NULL && !pesep->is_suppression &&
      pesep->instantiation_descr != NULL) {
    for (; pesep != NULL; pesep = pesep->next) {
      param_prp = pesep->expansion_descr->packs_referenced;
      arg_prp = pesep->instantiation_descr->pack_status;
      for (; param_prp != NULL;
           param_prp = param_prp->next, arg_prp = arg_prp->next) {
        /* Only process pack references for template parameters. */
        if (param_prp->kind != prk_template_param) continue;
        /* See if the coordinates of the pack reference match the ones
           specified by the caller. */
        if (param_prp->coordinates->depth != coordinates->depth ||
            param_prp->coordinates->position != coordinates->position) {
          continue;
        }  /* if */
        result_tap = arg_prp->curr_argument.template_arg;
        if (pesep->is_deduction) {
          if (result_tap != NULL &&
              !is_start_of_pack_expansion_templ_arg(result_tap)) {
            /* We are deducing an existing element. */
          } else {
            /* We are deducing the value for a new pack element.  Create the
               argument now and link it into the argument list. */
            result_tap = alloc_template_arg(
                      templ_arg_kind_for_symbol_kind(param_prp->symbol->kind));
            result_tap->is_pack_element = TRUE;
            check_assertion(arg_prp->prev_template_arg != NULL);
            result_tap->next = arg_prp->prev_template_arg->next;
            arg_prp->prev_template_arg->next = result_tap;
            arg_prp->prev_template_arg = result_tap;
            arg_prp->curr_argument.template_arg = result_tap;
          }  /* if */
        }  /* if */
        break;
      }  /* for */
      if (param_prp != NULL) break;
    }  /* for */
  } else {
    /* In cases where we can't find an argument, create one if we have a
       template parameter on which to base it. */
    create_if_not_found = templ_param != NULL;
  }  /* if */
  if (result_tap == NULL && create_if_not_found) {
    /* If not found above, just return an empty template argument. */
    result_tap = alloc_template_arg(
              templ_arg_kind_for_symbol_kind(templ_param->param_symbol->kind));
    if (is_rescan) {
      /* In error cases, including rescan errors, which may not result
         in actual compilation errors, we may not find an argument.
         Create an error argument. */
      set_template_arg_to_error(result_tap);
    }  /* if */
  }  /* if */
  return result_tap;
}  /* get_curr_variadic_arg_for_param */


static a_pack_expansion_descr_ptr get_curr_pack_expansion_descr_for_param(
				a_pack_expansion_stack_entry_ptr	pesep)
/*
If we are in the instantiation of an alias template based on a pack expansion
from a template declaration context, go through the pack references in the
instantiation descriptor for pesep (if any).  If there is exactly one
template parameter pack, and if the template parameter pack points to a
template parameter value, return the pack expansion descriptor of its
associated template argument.  Otherwise, return NULL.
*/
{
  a_pack_expansion_descr_ptr	result_pedp = NULL;
  a_pack_reference_ptr		arg_prp = NULL;
  a_scope_stack_entry_ptr	instantiation_ssep;

  instantiation_ssep =
                    scope_stack_entry_for(depth_innermost_instantiation_scope);
  if (pesep != NULL && !pesep->is_suppression &&
      pesep->instantiation_descr != NULL && instantiation_ssep != NULL &&
      instantiation_ssep->alias_in_template_decl) {
    arg_prp = pesep->instantiation_descr->pack_status;
  }  /* if */
  for (; arg_prp != NULL; arg_prp = arg_prp->next) {
    a_template_arg_ptr	tap;
    /* Only process pack references for template parameters. */
    if (arg_prp->kind != prk_template_param) continue;
    tap = arg_prp->curr_argument.template_arg;
    if (tap != NULL && tap->pack_expansion_descr != NULL) {
      if (result_pedp == NULL) {
        result_pedp = tap->pack_expansion_descr;
      } else {
        /* If there is more than one pack expansion descriptor, this should be
           a context in which deduction can't be done.  Return NULL. */
        result_pedp = NULL;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return result_pedp;
}  /* get_curr_pack_expansion_descr_for_param */


static a_pack_expansion_stack_entry_ptr push_pack_instantiation(
		a_pack_expansion_descr_ptr		pedp,
		a_template_param_ptr			templ_param_list,
		a_template_arg_ptr			templ_arg_list,
		a_boolean				is_rescan,
		a_boolean				is_deduction,
		a_boolean				allow_empty_list,
		a_ctws_state_ptr			ctws_state,
		a_boolean				*err)
/*
Create a pack instantiation description entry based on the expansion described
by pedp and push it on the pack expansion stack.  templ_param_list and
templ_arg_list are the template parameters and arguments for the
instantiation. Return a pointer to the pack expansion stack entry.  If
this is an invalid expansion or there are no arguments to be expanded,
return NULL.  is_rescan is TRUE if the pack instantiation is being pushed
as part of processing a rescan context.   is_deduction is TRUE if the pack
instantiation is being pushed as part of template argument deduction.
allow_empty_list is TRUE if a pack instantiation entry should be created
(for non-deduction cases) even if there are no elements of the expansion.
ctws_state is a substitution state block pointer, and can be NULL.

If the pack expansion is invalid (e.g., because there are packs of different
lengths) *err is set to TRUE, FALSE otherwise.
*/
{
  a_pack_expansion_stack_entry_ptr	pesep = NULL;
  a_pack_instantiation_descr_ptr	pidp;

  *err = FALSE;
  /* Construct the pack instantiation information based on the pack
     expansion information and the current context.  If the instantiation
     is invalid, or if there are no pack elements, a NULL instantiation
     entry will be returned. */
  pidp = create_pack_instantiation_descr(pedp, templ_param_list,
                                         templ_arg_list, is_rescan,
                                         is_deduction, allow_empty_list,
                                         ctws_state, err);
  if (pidp != NULL) {
    pesep = push_pack_expansion_stack();
    pesep->is_rescan = is_rescan;
    pesep->is_deduction = is_deduction;
    pesep->expansion_descr = pedp;
    pesep->instantiation_descr = pidp;
    pesep->preserve_deduced_packs = ctws_state != NULL &&
                                    ctws_state->preserve_deduced_packs;
    if (!is_rescan && !is_deduction) {
      /* Set the parameter pack symbols to the first element of each
         pack. */
      update_parameter_pack_symbol_values(pesep);
    }  /* if */
  }  /* if */
  return pesep;
}  /* push_pack_instantiation */


void suppress_expansion_with_no_packs_diagnostic(
			a_pack_expansion_stack_entry_ptr	pesep)
/*
Record that the caller has already issued a diagnostic to the effect the pack
expansion specified by pesep did not reference any packs (so that another
diagnostic will not be issued).
*/
{
  pesep->expansion_with_no_packs_diagnostic_issued = TRUE;
}  /* suppress_expansion_with_no_packs_diagnostic */


void push_expansion_suppression(
			a_pack_expansion_stack_entry_ptr	*p_pesep)
/*
Push an entry on the pack expansion stack that will suppress the recording
of pack expansion contexts and pack references in template dependent
contexts.  This routine has no effect outside of such contexts.  It
is used in prescan contexts to avoid creating redundant pack expansion
entries.
*/
{
  a_pack_expansion_stack_entry_ptr	pesep = NULL;

  if (is_prototype_instantiation_context() && is_variadic_template_context()) {
    pesep = push_pack_expansion_stack();
    pesep->is_suppression = TRUE;
  }  /* if */
  *p_pesep = pesep;
}  /* push_expansion_suppression */


void pop_expansion_suppression(
			a_pack_expansion_stack_entry_ptr	pesep)
/*
Pop the pack suppression entry pesep from the pack expansion stack.  pesep
will be NULL outside of template dependent contexts.
*/
{
  if (pesep != NULL) {
    check_assertion(pesep == pack_expansion_stack &&
                    pesep->is_suppression);
    pop_pack_expansion_stack();
  }  /* if */
}  /* pop_expansion_suppression */


void begin_prescan_context(
    a_boolean                        suppress_packs,
    a_boolean                        *packs_suppressed,
    a_pack_expansion_stack_entry_ptr *pack_expansion_stack_entry,
    a_boolean                        *saved_in_disambiguation,
    ARG_UNUSED a_boolean             *saved_source_sequence_entries_disallowed)
/*
Update the scope stack to indicate that we are in a prescan or
disambiguation context.  suppress_packs is TRUE if a pack suppression
context should be pushed if we are in a variadic prototype instantiation.
*packs_suppressed is set to TRUE if a variadic pack suppression was pushed.
*saved_in_disambiguation and *saved_source_sequence_entries_disallowed are
used to record the previous value of the corresponding scope stack flags.
pack_expansion_stack_entry points to the location used for the pack expansion
stack entry pointer returned if a pack suppression needs to pushed, and can be
NULL if suppress_packs is FALSE.
*/
{
  a_scope_stack_entry_ptr	ssep;

  *packs_suppressed = suppress_packs &&
       is_prototype_instantiation_context() && is_variadic_template_context();
  if (*packs_suppressed) {
    /* Disable variadic processing during the prescan. */
    push_expansion_suppression(pack_expansion_stack_entry);
  } else {
    *pack_expansion_stack_entry = NULL;
  }  /* if */
  ssep = &scope_stack_top();
  *saved_in_disambiguation = ssep->in_disambiguation;
  ssep->in_disambiguation = TRUE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  *saved_source_sequence_entries_disallowed =
                                            source_sequence_entries_disallowed;
  check_assertion(source_sequence_entries_disallowed ==
                  ssep->source_sequence_entries_disallowed);
  ssep->source_sequence_entries_disallowed = TRUE;
  source_sequence_entries_disallowed = TRUE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
}  /* begin_prescan_context */


void end_prescan_context(
     a_boolean                        packs_suppressed,
     a_pack_expansion_stack_entry_ptr pack_expansion_stack_entry,
     a_boolean                        saved_in_disambiguation,
     ARG_UNUSED a_boolean             saved_source_sequence_entries_disallowed)
/*
Update the scope stack to indicate that we are no longer in a prescan or
disambiguation context.  packs_suppressed, pack_expansion_stack_entry,
saved_in_disambiguation and saved_source_sequence_entried_disallowed are the
values returned by the begin_prescan_context call.
*/
{
  if (packs_suppressed) {
    /* Restore the variadic processing state. */
    pop_expansion_suppression(pack_expansion_stack_entry);
  }  /* if */
  scope_stack_top().in_disambiguation = saved_in_disambiguation;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  scope_stack_top().source_sequence_entries_disallowed =
                                      saved_source_sequence_entries_disallowed;
  source_sequence_entries_disallowed =
                                      saved_source_sequence_entries_disallowed;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
}  /* end_prescan_context */


a_boolean in_generic_lambda_in_prototype_instantiation(void)
/*
Return TRUE if we are within the prototype instantiation of a generic lambda.
*/
{
  a_boolean			result = FALSE;

  if (!is_prototype_instantiation_context()) {
    /* We are not in a prototype instantiation context of any kind. */
  } else {
    a_scope_stack_entry_ptr	ssep = &scope_stack_top();
    for (; ssep != NULL; ssep = previous_scope_of(ssep)) {
      /* Look for a generic lambda prototype instantiation. */
      if (scope_is(ssep, sck_template_instantiation) &&
          ssep->is_generic_lambda && ssep->in_prototype_instantiation) {
        result = TRUE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return result;
}  /* in_generic_lambda_in_prototype_instantiation */


a_routine_ptr enclosing_nonlambda_routine_for_lambda_class(
                                                   a_type_ptr  *p_lambda_class)
/*
Skip outward from the lambda closure class indicated by *p_lambda_class through
enclosing lambda call operators and update *p_lambda_class to the outermost
lambda closure class reached.  Return the first enclosing non-lambda routine,
or NULL if there is no such routine or if reaching it would cross a non-lambda
class.
*/
{
  a_type_ptr     lambda_class = *p_lambda_class;
  a_routine_ptr  encl_rout;

  check_assertion(lambda_class != NULL &&
                  type_is_lambda_closure(lambda_class));
  encl_rout = lambda_class->source_corresp.enclosing_routine;
  while (encl_rout != NULL && encl_rout->is_lambda_body) {
    if (lambda_class->source_corresp.is_class_member &&
        !type_is_lambda_closure(parent_class_of(lambda_class))) {
      break;
    }  /* if */
    lambda_class = parent_class_of(encl_rout);
    encl_rout = lambda_class->source_corresp.enclosing_routine;
  }  /* while */
  if (encl_rout != NULL && lambda_class->source_corresp.is_class_member &&
      !type_is_lambda_closure(parent_class_of(lambda_class))) {
    encl_rout = NULL;
  }  /* if */
  *p_lambda_class = lambda_class;
  return encl_rout;
}  /* enclosing_nonlambda_routine_for_lambda_class */


a_boolean generic_lambda_is_in_specialized_routine(a_routine_ptr  call_op_rp)
/*
Return TRUE if the generic lambda whose call operator is call_op_rp is enclosed
by an explicitly-specialized function definition; otherwise, return FALSE.
*/
{
  a_type_ptr     lambda_class = parent_class_of(call_op_rp);
  a_routine_ptr  encl_rout =
                   enclosing_nonlambda_routine_for_lambda_class(&lambda_class);
  return encl_rout != NULL && encl_rout->is_specialized;
}  /* generic_lambda_is_in_specialized_routine */


static a_boolean template_instantiation_for_class_is_on_stack(
                                           a_type_ptr               class_type,
                                           a_scope_stack_entry_ptr  ssep)
/*
Return TRUE if a real instantiation scope for class_type appears at or before
ssep in the current scope stack.
*/
{
  a_boolean  result = FALSE;

  for (; ssep != NULL; ssep = previous_scope_of(ssep)) {
    if (scope_is(ssep, sck_template_instantiation) &&
        !ssep->in_prototype_instantiation &&
        !ssep->in_nonreal_instantiation && ssep->assoc_type != NULL &&
        same_entities(ssep->assoc_type, class_type)) {
      result = TRUE;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* template_instantiation_for_class_is_on_stack */


static a_boolean class_template_friend_instantiation_is_on_stack(
                                            a_routine_ptr            friend_rp,
                                            a_scope_stack_entry_ptr  ssep)
/*
Return TRUE if friend_rp is on the current scope stack as a function defined
in a class template whose corresponding real instantiation scope is also on
the stack.
*/
{
  a_boolean  found_friend_scope = FALSE, result = FALSE;

  for (; ssep != NULL; ssep = previous_scope_of(ssep)) {
    if (!found_friend_scope) {
      if (scope_is(ssep, sck_function) && ssep->assoc_routine == friend_rp) {
        found_friend_scope = TRUE;
      }  /* if */
    } else if (scope_is(ssep, sck_class_struct_union) ||
               scope_is(ssep, sck_class_reactivation)) {
      if (is_template_class_type(ssep->assoc_type)) {
        result = template_instantiation_for_class_is_on_stack(
                                                      ssep->assoc_type,
                                                      previous_scope_of(ssep));
      }  /* if */
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* class_template_friend_instantiation_is_on_stack */


a_boolean in_generic_lambda_in_class_template_friend(void)
/*
Return TRUE if the innermost instantiation scope is a generic lambda call
operator whose lambda is lexically inside a function defined in a friend
declaration of a class template that is being instantiated.  In that situation
the friend definition, and therefore the lambda, is instantiated as part of the
class template instantiation, and ADL inside the lambda body must continue to
see friend declarations introduced by sibling class template instantiations
performed during the same instantiation chain.
*/
{
  a_boolean                result = FALSE;
  a_scope_stack_entry_ptr  ssep;

  ssep = scope_stack_entry_for(depth_innermost_instantiation_scope);
  if (ssep != NULL && ssep->is_generic_lambda &&
      ssep->assoc_routine != NULL && ssep->assoc_routine->is_lambda_body) {
    a_type_ptr     lambda_class = parent_class_of(ssep->assoc_routine);
    a_routine_ptr  enclosing_rp =
                   enclosing_nonlambda_routine_for_lambda_class(&lambda_class);
    if (enclosing_rp != NULL && enclosing_rp->defined_in_friend_decl) {
      result = class_template_friend_instantiation_is_on_stack(
                                                      enclosing_rp,
                                                      previous_scope_of(ssep));
    }  /* if */
  }  /* if */
  return result;
}  /* in_generic_lambda_in_class_template_friend */


a_boolean is_nested_in_real_instantiation(void)
/*
Return TRUE if we are nested within a real instantiation.
*/
{
  a_boolean  result = FALSE;

  a_scope_stack_entry_ptr  ssep = &scope_stack_top();
  for (; ssep != NULL; ssep = previous_scope_of(ssep)) {
    /* Look for a real instantiation. */
    if (scope_is(ssep, sck_template_instantiation) &&
        !ssep->in_prototype_instantiation &&
        !ssep->in_nonreal_instantiation) {
      result = TRUE;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* is_nested_in_real_instantiation */


static a_boolean is_alias_prototype_in_real_instantiation(void)
/*
Return TRUE if we are within the prototype instantiation of an alias
template within a real instantiation of a class.
*/
{
  a_boolean			result = FALSE;

  if (!is_prototype_instantiation_context()) {
    /* We are not in a prototype instantiation context of any kind. */
  } else {
    a_scope_stack_entry_ptr ssep;
    a_symbol_ptr            sym;
    ssep = scope_stack_entry_for(depth_innermost_instantiation_scope);
    sym = ssep->template_sym;
    if (is_alias_template_symbol(sym)) {
      /* The innermost instantiation is of an alias template.  See if
         we are in a real instantiation. */
      for (ssep = previous_scope_of(ssep); ssep != NULL;
           ssep = previous_scope_of(ssep)) {
        /* Look for an enclosing real instantiation scope. */
        if (scope_is(ssep, sck_template_instantiation)) {
          result = !ssep->in_prototype_instantiation;
          break;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  return result;
}  /* is_alias_prototype_in_real_instantiation */


static a_boolean is_pack_instantiation_context(
				a_pack_expansion_descr_ptr	*p_pedp)
/*
Determine if we are in a context where a pack instantiation could be done.
Also determine whether there is pack expansion at the current position.

Something is treated as an instantiation context if:
  - We are in a real instantiation context.
  - We are in a prototype instantiation context for an expansion that
    only uses enclosing packs (e.g., the declaration of a member template
    in the instantiation of an enclosing class template).
  - We are in a generic lambda prototype instantiation inside some real
    enclosing instantiation.
  - We have real template arguments for all referenced packs.

In such contexts, return TRUE.  If we found a pack expansion, set *p_pedp
to the pack expansion descriptor for the pack being instantiated.  Note
that *p_pedp is set even when FALSE is returned.
*/
{
  a_pack_expansion_descr_ptr	pedp;
  a_boolean			result = FALSE;

  pedp = get_pack_expansion_for_curr_context();
  *p_pedp = pedp;
  if (pedp != NULL) {
    if (!is_uninstantiated_pack_expansion_context() &&
        !in_ms_nonreal_class_instantiation()) {
      result = TRUE;
    } else if (pedp->uses_only_enclosing_packs &&
               is_real_instantiation_context()) {
      result = TRUE;
    } else if (pedp->uses_only_enclosing_packs &&
               is_alias_prototype_in_real_instantiation()) {
      result = TRUE;
    } else if (pedp->uses_any_enclosing_packs &&
               in_generic_lambda_in_real_instantiation()) {
      result = TRUE;
    } else if (pedp->uses_only_enclosing_packs) {
      a_pack_reference_ptr      prp;
      a_template_nesting_depth  max_pack_depth = NO_NESTING_DEPTH;
      uint32_t                  min_scopes_to_skip = 0;
      a_boolean                 has_variable_pack_reference = FALSE,
                                has_unhandled_pack_reference = FALSE;

      /* Check if we have real template arguments for each referenced pack.  We
         can do this by checking the coordinates of template parameters and the
         number of function scopes to skip for variables. */
      for (prp = pedp->packs_referenced; prp != NULL; prp = prp->next) {
        if (prp->kind == prk_template_param) {
          if (prp->coordinates->depth > max_pack_depth) {
            max_pack_depth = prp->coordinates->depth;
          }  /* if */
        } else if (prp->kind == prk_variable || prp->kind == prk_binding) {
          has_variable_pack_reference = TRUE;
          if (min_scopes_to_skip == 0 ||
              prp->function_or_block_scopes_to_skip < min_scopes_to_skip) {
            min_scopes_to_skip = prp->function_or_block_scopes_to_skip;
          }  /* if */
        } else {
          has_unhandled_pack_reference = TRUE;
          break;
        }  /* if */
      }  /* for */
      if (!has_unhandled_pack_reference) {
        a_template_nesting_depth  real_instantiation_depth = NO_NESTING_DEPTH,
                                  max_variable_pack_depth = NO_NESTING_DEPTH;
        a_scope_stack_entry_ptr   ssep = &scope_stack_top();
        a_boolean                 skip_next_inst_scope = FALSE;

        /* Walk the scope stack to count the number of template instantiation
           scopes with real template arguments and determine the depth of the
           innermost variable pack reference. */
        skip_next_inst_scope = FALSE;
        for (; ssep != NULL; ssep = previous_scope_of(ssep)) {
          if (scope_is(ssep, sck_function) || scope_is(ssep, sck_block)) {
            if (min_scopes_to_skip > 0) {
              skip_next_inst_scope = TRUE;
              --min_scopes_to_skip;
            }  /* if */
          } else if (scope_is(ssep, sck_template_instantiation)) {
            if (has_variable_pack_reference && !skip_next_inst_scope) {
              ++max_variable_pack_depth;
            } else {
              skip_next_inst_scope = FALSE;
            }  /* if */
            if (!ssep->in_prototype_instantiation &&
                !ssep->in_nonreal_instantiation) {
              ++real_instantiation_depth;
            }  /* if */
          } else {
            skip_next_inst_scope = FALSE;
          }  /* if */
        }  /* for */
        if (max_variable_pack_depth > max_pack_depth) {
          max_pack_depth = max_variable_pack_depth;
        }  /* if */
        /* We are in a pack instantiation context if we have real template
           arguments for all referenced packs. */
        result = max_pack_depth <= real_instantiation_depth;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* is_pack_instantiation_context */


a_boolean begin_potential_pack_expansion_context_full(
		a_pack_expansion_stack_entry_ptr	*p_pesep,
		a_pack_expansion_descr_ptr		*p_pedp,
		a_boolean				is_lookahead,
		a_boolean				allow_empty_list,
		a_boolean				ignore_suppression,
		a_boolean				claim_pack_index)
/*
This is called at the start of a construct that could be a variadic template
pack expansion.  Such pack expansions occur only within the declarations
and bodies of templates.  This routine is called when entering a potential
pack expansion context.  In most cases it will turn out that the context
does not actually contain a pack expansion.  In the example below, this
routine is called at the points marked with "^".   On each of the lines, the
first "^" is not actually a pack expansion but the second one is.

  template <class ... T> void f(int i, T ... args){
                                ^      ^
    h(1, (args+1)...);
      ^  ^
  }

A variadic template must undergo a prototype instantiation before a real
instantiation can be done.  During the prototype instantiation the token
range of each actual pack expansion is recorded.  A list of the parameter packs
actually referenced from within the expansion is also recorded.  When we
reach the corresponding potential pack expansion context during a real
instantiation we know which argument values, if any, need to be expanded.

This routine returns TRUE if there are any argument values to be expanded or
if this is not an actual pack expansion.  In other words, it returns FALSE
for an actual pack expansion for which the associated parameter packs have
zero elements.  It will also issue an error and return FALSE for a pack
expansion that uses multiple parameter packs but for which the parameter
packs do not all have the same number of elements.  When FALSE is returned,
the token stream will be advanced so that the current token is the one
following the end of the pack expansion context (i.e., usually after
the "...").

When this routine is called in a prototype instantiation, it always returns
TRUE -- we have either a non-pack context or a prototype instantiation of
a pack expansion, which is always treated as having a single element.

When this routine is called in a real instantiation context (and the
parameter packs are non-empty), the symbols for any parameter packs
referenced in the expansion are updated to refer to the first element
of the associated pack.

*p_pesep is a pointer to an entry that describes the current pack expansion
context.  A stack of such entries is maintained and the p_pesep pointer passed
by the caller is updated to point to the stack entry created for the
new context.  In a non-variadic context in an actual instantiation, *p_pesep
will be set to NULL.

*p_pedp is a pointer to the pack expansion descriptor for this pack
expansion.  If p_pedp is non-NULL a pointer to the entry is returned in
*p_pedp;

is_lookahead is TRUE if this context is being pushed to distinguish between
two contexts.  When this is TRUE, it is assumed that another begin...
call will be done for the same starting position, and that context
will be responsible for the end... and advance... calls.

allow_empty_list is TRUE if a pack instantiation entry should be created
(for non-deduction cases) even if there are no elements of the expansion.

If ignore_suppression is TRUE, a new entry will be pushed even if a
suppression is on the stack.

claim_pack_index is TRUE when the caller is scanning the operand of a C++26
pack-index construct.  Descriptors for those constructs are reserved for that
scan.  Other callers that encounter such a descriptor should treat the
construct as a single element and leave the descriptor available for the
pack-index scanner.
*/
{
  a_boolean				any_args = FALSE;
  a_pack_expansion_stack_entry_ptr	pesep = NULL;
  a_pack_expansion_descr_ptr		pedp = NULL;

  if (!variadic_templates_enabled) {
    /* Variadic template processing is not enabled.  Return TRUE as this will
       always be treated as a single argument value. */
    any_args = TRUE;
  } else if (!is_variadic_template_context()) {
    /* We are not in the definition or instantiation of a variadic template. */
    any_args = TRUE;
  } else if (!ignore_suppression && pack_expansion_stack != NULL &&
             pack_expansion_stack->is_suppression) {
    /* We are suppressing the recording of pack expansions.  Return the pack
       expansion stack entry for the suppression entry. */
    pesep = pack_expansion_stack;
    any_args = TRUE;
  } else if (pack_expansion_stack != NULL &&
             pack_expansion_stack->is_lookahead &&
             pack_expansion_stack->expansion_descr != NULL &&
             pack_expansion_stack->expansion_descr->first_token ==
                                                  curr_token_sequence_number) {
    /* This is a redundant push of the same starting location.  Just return
       the previously created entry. */
    pesep = pack_expansion_stack;
    pedp = pesep->expansion_descr;
    any_args = TRUE;
  } else if (is_pack_instantiation_context(&pedp)) {
    if (pedp != NULL && pedp->is_pack_index && !claim_pack_index) {
      /* This descriptor belongs to a pack-index operand, not the current
         list-level context.  Leave it for the pack-index scanner and handle
         the current construct as a single element. */
      any_args = TRUE;
      pedp = NULL;
    } else {
      /* This is a real instantiation.  See if there is a corresponding
         parameter pack from the template definition.  Get the template
         parameter list and template argument list associated with the current
         instantiation. */
      a_template_param_ptr  templ_param_list;
      a_template_arg_ptr    templ_arg_list;
      a_boolean             err;
      get_curr_template_params_and_args(&templ_param_list, &templ_arg_list);
      pesep = push_pack_instantiation(pedp, templ_param_list, templ_arg_list,
                                      /*is_rescan=*/FALSE,
                                      /*is_deduction=*/FALSE,
                                      allow_empty_list,
                                      (a_ctws_state_ptr)NULL, &err);
      increment_dependent_scans_for_reusable_cache();
      any_args = pesep != NULL;
      if (pesep != NULL && pesep->instantiation_descr != NULL &&
          !pesep->instantiation_descr->is_empty) {
        check_assertion(curr_token_sequence_number == pedp->first_token);
        pesep->first_token_cache = get_token_cache_being_scanned();
        pesep->first_token_tsn = curr_token_sequence_number;
        /* Mark that the current reusable cache is being used for rescan
           purposes. */
        if (is_lookahead) pesep->is_lookahead = TRUE;
      } else {
        /* There are no arguments to be expanded.  Advance to the token
           after the end of the expansion.  Don't advance when is_lookahead
           is TRUE because we want to return the same result (i.e., FALSE)
           when this routine is called again. */
        decrement_dependent_scans_for_reusable_cache();
        if (!is_lookahead) skip_pack_expansion_tokens(pedp);
        any_args = FALSE;
      }  /* if */
    }  /* if */
  } else if (is_uninstantiated_pack_expansion_context() ||
             in_ms_nonreal_class_instantiation()) {
    any_args = TRUE;
    pesep = push_pack_expansion_stack();
    if (pedp != NULL) {
      /* If pedp was set above, reset any enclosing packs to their dependent
         values.  This can come up in examples like
           template <typename... T> struct C {
             template <typename... U> static A<B<T, U>...> f(U&& ... args);
           };
         where it is necessary to record B<T,U> using the template parameter
         values for both T and U. */
      reset_enclosing_pack_values();
      pesep->enclosing_packs_reset = TRUE;
    }  /* if */
    /* Allocate an expansion descriptor for this stack entry. */
    pesep->expansion_descr = alloc_pack_expansion_descr();
    /* Save the start of the token range for the pack. */
    pesep->expansion_descr->first_token = curr_token_sequence_number;
    if (is_lookahead) pesep->is_lookahead = TRUE;
    pedp = pesep->expansion_descr;
  } else {
    /* Some other context -- assumed to have a single nonvariadic argument. */
    any_args = TRUE;
  }  /* if */
  /* Return the pack expansion descriptor, if any, to the caller. */
  *p_pesep = pesep;
  if (p_pedp != NULL) *p_pedp = pedp;
  return any_args;
}  /* begin_potential_pack_expansion_context_full */


a_boolean begin_potential_pack_expansion_context(
			a_pack_expansion_stack_entry_ptr	*p_pesep)
/*
Interface to begin_potential_pack_expansion_context_full that supplies
default values for certain arguments.  See that routine for more information.
*/
{
  a_boolean				any_args;

  any_args = begin_potential_pack_expansion_context_full(
                                   p_pesep, (a_pack_expansion_descr_ptr*)NULL,
                                   /*is_lookahead=*/FALSE,
                                   /*allow_empty_list=*/FALSE,
                                   /*ignore_suppression=*/FALSE,
                                   /*claim_pack_index=*/FALSE);
  return any_args;
}  /* begin_potential_pack_expansion_context */


a_boolean begin_rescan_pack_expansion_context(
		a_pack_expansion_descr_ptr		pedp,
		a_template_param_ptr			templ_param_list,
		a_template_arg_ptr			templ_arg_list,
		a_pack_expansion_stack_entry_ptr	*p_pesep,
		a_ctws_options_set              	options,
		a_ctws_state_ptr			ctws_state,
		a_boolean				*err)

/*
This routine is similar to begin_potential_pack_expansion_context (see
that routine for more details about how pack expansions are handled
in general), but is called in expression rescan contexts to do pack
expansions during actual instantiations rescans.  This is known to
be an actual pack expansion (unlike begin_potential_pack_expansion_context).
pedp is the pack expansion descriptor created when the pack expansion
was initially scanned, and can be NULL in which case this routine simply
returns a NULL value in *p_pesep.  templ_param_list and templ_arg_list are the
template parameters and arguments for the instantiation.  ctws_state
is a substitution state block pointer, and can be NULL.  options is a set
of flags controlling substitution.

See begin_potential_pack_expansion_context for a description of the
return value and the setting of *p_pesep (note that this routine is
never called in prototype instantiation contexts).  This routine
returns TRUE if pedp was passed in as NULL.  If the pack expansion is
invalid (e.g., because there are packs of different lengths) *err is
set to TRUE, FALSE otherwise.
*/
{
  a_pack_expansion_stack_entry_ptr	pesep = NULL;

  *err = FALSE;
  if ((options & CTWS_ADJUST_COORDINATES) != 0) {
    /* Don't rescan packs when adjusting template parameter coordinates. */
    pedp = NULL;
  } else if (pedp != NULL) {
    /* Make a copy of the template argument list.  In some cases the caller
       can free the list before the rescan is complete. */
    templ_arg_list = copy_template_arg_list(templ_arg_list);
    pesep = push_pack_instantiation(pedp, templ_param_list, templ_arg_list,
                                    /*is_rescan=*/TRUE,
                                    /*is_deduction=*/FALSE,
                                    /*allow_empty_list=*/FALSE,
                                    ctws_state, err);
    if (pesep != NULL) {
      pesep->template_arg_list = templ_arg_list;
    } else {
      free_template_arg_list(templ_arg_list);
    }  /* if */
  }  /* if */
  *p_pesep = pesep;
  return pesep != NULL || pedp == NULL;
}  /* begin_rescan_pack_expansion_context */


void begin_pack_deduction_context(
		a_pack_expansion_descr_ptr		pedp,
		a_template_param_ptr			templ_param_list,
		a_template_arg_ptr			*templ_arg_list,
		a_pack_expansion_stack_entry_ptr	*p_pesep)
/*
This routine is called in template argument deduction contexts to do
deduction of the elements of a template argument pack.  templ_param_list
is the template parameter list for the template whose arguments are
being deduced.  *templ_arg_list is the list of arguments that have
been deduced so far.  If it is NULL, a new argument list will be
created here.  For variadic parameters, it will have the start of pack
expansion placeholder.  This routine pushes a pack expansion stack
entry and returns a pointer to that entry in *p_pesep.
*/
{
  a_pack_expansion_stack_entry_ptr	pesep;
  a_boolean				err;

  check_assertion(pedp != NULL);
  if (*templ_arg_list == NULL) {
    /* The template argument list does not exist yet.  Create an argument
       list without any values filled in. */
    *templ_arg_list = create_initial_template_arg_list(
				templ_param_list, (a_template_arg_ptr)NULL,
                                /*is_templ_templ_param_check=*/FALSE,
                                (a_source_position*)NULL);
  }  /* if */
  pesep = push_pack_instantiation(pedp, templ_param_list, *templ_arg_list,
                                  /*is_rescan=*/FALSE, /*is_deduction=*/TRUE,
                                  /*allow_empty_list=*/FALSE,
                                  (a_ctws_state_ptr)NULL, &err);
  *p_pesep = pesep;
}  /* begin_pack_deduction_context */


void advance_to_next_deduced_element(
				a_pack_expansion_stack_entry_ptr	pesep)
/*
Unlike other pack contexts, this "advance" routine is called before
the end-of-context routine (end_pack_deduction_context in this case).
It advances the current template argument information.  Typically the
pack elements are being deduced for the first time, so that any
subsequent deductions will result in the creation of a new deduced
argument.  But if this is a subsequent deduction of the same pack, this
routine will advance through the existing list of elements, and then
additional elements can be deduced at the end.   pesep describes the
current pack deduction context.
*/
{
   a_pack_reference_ptr	param_prp;
   a_pack_reference_ptr	arg_prp;

  /* The pack expansion descriptor passed in should be on top of the
     stack. */
  check_assertion(pesep == pack_expansion_stack);
  for (param_prp = pesep->expansion_descr->packs_referenced,
         arg_prp = pesep->instantiation_descr->pack_status;
       param_prp != NULL;
       param_prp = param_prp->next, arg_prp = arg_prp->next) {
    if (param_prp->kind == prk_template_param) {
      a_template_arg_ptr	tap = arg_prp->curr_argument.template_arg;
      a_template_arg_ptr	next_tap = NULL;
      
      if (tap != NULL && tap->next != NULL && tap->next->is_pack_element) {
        next_tap = tap->next;
      }  /* if */
      arg_prp->curr_argument.template_arg = next_tap;
    }  /* if */
  }  /* for */
}  /* advance_to_next_deduced_element */


void end_pack_deduction_context(
			a_pack_expansion_stack_entry_ptr	pesep)
/*
This is called at the end of a variadic deduction context and pops the
pack expansion stack.
*/
{
  if (pesep != NULL) pop_pack_expansion_stack();
}  /* end_pack_deduction_context */


static void record_pack_expansion(a_pack_expansion_descr_ptr	pedp)
/*
We have reached the end of a potential pack expansion context and
there were parameter packs referenced.  Save the information needed
to expand the pack in a real instantiation.
*/
{
  a_template_decl_info_ptr	tdip;
  a_scope_stack_entry_ptr	ssep;

#if DEBUG
  if (db_flag_is_set("packs")) {
    fprintf(f_debug, "Recording pack expansion from %ld to %ld\n",
            (long)pedp->first_token, (long)pedp->last_token);
    db_pack_tokens(pedp);
  }  /* if */
#endif /* DEBUG */
  /* Get the template declaration information entry associated with the
     current context. */
  ssep = get_outermost_template_dependent_context();
  tdip = ssep->template_decl_info;
  /* Set the last_pack_expansion_used field so that this pack expansion can
     be found by get_pack_expansion_for_curr_context. */
  ssep->last_pack_expansion_used = pedp;
  /* Insert the new entry to preserve a list sorted by starting token
     sequence number. */
  {
    a_pack_expansion_descr_ptr	last_pedp = tdip->last_pack_expansion; 
    a_pack_expansion_descr_ptr	insert_pedp = last_pedp;
    /* Normally entries will just be added to the end of the list, but
       for nested expansions we may need to back up in the list. */
    for (; insert_pedp != NULL &&
           insert_pedp->first_token > pedp->first_token;
         insert_pedp = insert_pedp->previous) {}
    if (insert_pedp == NULL) {
      pedp->next = tdip->pack_expansions;
      if (tdip->pack_expansions != NULL) {
        tdip->pack_expansions->previous = pedp;
      }  /* if */
      tdip->pack_expansions = pedp;
    } else {
      if (insert_pedp->next != NULL) insert_pedp->next->previous = pedp;
      pedp->next = insert_pedp->next;
      pedp->previous = insert_pedp;
      insert_pedp->next = pedp;
    }  /* if */
    /* If this is the last entry on the list, update the last pointer. */
    if (pedp->next == NULL) tdip->last_pack_expansion = pedp;
  }
}  /* record_pack_expansion */


static void extract_pack_references_for_context(
					a_pack_expansion_descr_ptr	pedp)
/*
Go through the list of pack references in the current scope stack entry,
extract any entries for the range of tokens comprised by pedp, and attach
that list to pedp.
*/
{
  a_scope_stack_entry_ptr	ssep;
  a_pack_reference_ptr		*p_first_prp;
  a_pack_reference_ptr		last_prp;
  a_pack_reference_ptr		prp;

  /* Set this to TRUE now -- it will be cleared later if we extract a
     reference to a non-enclosing pack. */
  pedp->uses_only_enclosing_packs = TRUE;
  ssep = get_scope_for_pack_references();
  p_first_prp = &ssep->packs_referenced;
  /* Find the first pack reference, if any, of the expansion range. */
  for (prp = ssep->packs_referenced; prp != NULL; prp = prp->next) {
    if (prp->token_sequence_number >= pedp->first_token) break;
    p_first_prp = &prp->next;
  }  /* for */
  if (prp != NULL) {
    /* We found a pack reference after the start of the expansion.  Find any
       that are also before the end of the expansion. */
    last_prp = NULL;
    for (; prp != NULL; prp = prp->next) {
      if (prp->token_sequence_number > pedp->last_token) break;
      last_prp = prp;
    }  /* for */
    if (last_prp != NULL) {
      /* There are pack references for this expansion.  Extract them from
         the scope stack list. */
      pedp->packs_referenced = *p_first_prp;
      *p_first_prp = last_prp->next;
      last_prp->next = NULL;
#if DEBUG
      if (db_flag_is_set("packs")) {
        fprintf(f_debug, "Extracting references for:\n");
        for (prp = pedp->packs_referenced; prp != NULL; prp = prp->next) {
          fprintf(f_debug, "  ");
          db_symbol_name(prp->symbol);
          fprintf(f_debug, " at tsn %lu\n",
                  (unsigned long)prp->token_sequence_number);
        }  /* for */
      }  /* if */
#endif /* DEBUG */
      {
        /* Make sure the same symbol is not on the extracted list more than
           once. */
        a_pack_reference_ptr	prp1;
        for (prp1 = pedp->packs_referenced; prp1 != NULL; prp1 = prp1->next) {
          a_pack_reference_ptr	prp2;
          a_pack_reference_ptr	prev_prp2 = prp1;
          if (!prp1->uses_enclosing_pack) {
            /* Record that this expansion uses only a non-enclosing pack. */
            pedp->uses_only_enclosing_packs = FALSE;
          } else {
            /* This expansion uses at least one enclosing pack. */
            pedp->uses_any_enclosing_packs = TRUE;
          }  /* if */
          for (prp2 = prp1->next; prp2 != NULL; prp2 = prp2->next) {
            if (prp1->symbol == prp2->symbol) {
              /* Remove the redundant symbol. */
              prev_prp2->next = prp2->next;
            } else {
              prev_prp2 = prp2;
            }  /* if */
          }  /* for */
          if (prp1->kind == prk_variable || prp1->kind == prk_binding) {
            uint32_t  scopes_to_skip = 0;
            /* Find out how many function scopes need to be skipped to find
               the parameter or binding variable. */
            for (ssep = &scope_stack_top(); ssep != NULL;
                 ssep = previous_scope_of(ssep)) {
              /* Only consider function and block scopes. */
              if (!scope_is(ssep, sck_function) &&
                  !scope_is(ssep, sck_block)) {
                continue;
              }  /* if */
              if (ssep->number == prp1->symbol->decl_scope) {
                break;
              }  /* if */
              scopes_to_skip++;
            }  /* for */
            if (ssep == NULL) {
              expect_error();
              break;
            } else if (prp1->kind == prk_binding) {
              /* For a binding pack, determine the binding declaration
                 number. */
              a_scope_ptr     sp = ssep->il_scope;
              uint32_t        binding_num = 0;
              a_variable_ptr  binding_var = prp1->symbol->variant.variable.ptr;
              a_variable_ptr  vp = binding_var->storage_class == sc_static ?
                                       sp->variables : sp->nonstatic_variables;
              for (; vp != NULL; vp = vp->next) {
                if (vp == binding_var) break;
                if (vp->is_struct_binding_container) ++binding_num;
              }  /* for */
              if (vp != NULL) {
                prp1->param_or_binding_num = binding_num;
              } else {
                unexpected_condition();
              }  /* if */
            }  /* if */
            prp1->function_or_block_scopes_to_skip = scopes_to_skip;
          }  /* if */
        }  /* for */
      }
    }  /* if */
  }  /* if */
}  /* extract_pack_references_for_context */


static void issue_pack_not_expanded_diagnostics(a_pack_reference_ptr	prp)
/*
Issue diagnostics for each element of the pack reference list pointed to
by prp.
*/
{
  if (in_generic_lambda_in_real_instantiation()) {
    /* When a generic lambda appears in an instantiation, a new prototype
       instantiation is done.  The pack processing in such cases is a
       hybrid of a normal prototype instantiation and an actual instantiation.
       Any actual missing expansions will have been diagnosed in the original
       prototype instantiations, so avoid issuing any potentially incorrect
       diagnostics here. */
  } else if (scope_stack_top().in_nonreal_instantiation &&
             scope_stack_top().is_default_template_arg &&
             enclosing_scope_is_prototype_instantiation_context()) {
    /* Similarly, a nonreal instantiation of a default template argument that
       is enclosed by a prototype instantiation context might result in a pack
       that is only expanded in an outer context. */
  } else if (scope_stack_top().in_disambiguation) {
    /* We are in disambiguation, so pack references are not being recorded. */
  } else {
    for (; prp != NULL; prp = prp->next) {
      pos_st_error(ec_pack_not_expanded, &prp->position,
                   prp->symbol->header->identifier);
    }  /* for */
  }  /* if */
}  /* issue_pack_not_expanded_diagnostics */


a_pack_expansion_descr_ptr end_potential_pack_expansion_context(
			a_pack_expansion_stack_entry_ptr	pesep,
			a_boolean				is_declarator)
/*
This is called at the end of a construct that could be a variadic template
pack expansion.  This is called in circumstances similar to those for
begin_potential_pack_expansion_context (see also for more information).
pesep describes the current pack expansion, and can be NULL in an actual
instantiation of a context that did not turn out to be variadic.  Note that
this routine will not be called during an actual instantiation in which the
parameter packs are empty.

is_declarator is TRUE if this is being called while scanning a parameter
declaration.  Normally, this routine expects to be called when the "..."
is the current token, and that token is bypassed.  This is not done when
is_declarator is TRUE (in which case the caller is expected to call
record_pack_expansion_ellipsis when the "..." is encountered in the middle
of the declaration).

In a prototype instantiation context for something that is an actual
pack expansion, this routine returns a pointer to the pack expansion
descriptor.  In most contexts this can be ignored, but in expression
rescan contexts it must be saved so that the pack expansion can be
rescanned.

When this routine is called in an expression rescan context, it has no
effect and returns NULL.
*/
{
  a_pack_expansion_descr_ptr	result_pedp = NULL;

  /* This routine should do nothing when called in an expression rescan
     context. */
  if (pesep != NULL && pesep->is_rescan) pesep = NULL;
  if (pesep == NULL) {
    /* A non-variadic context.  There is nothing to be done. */
  } else if (pesep->is_suppression) {
    /* We are suppressing pack expansion processing. */
    pesep = NULL;
    if (!is_declarator && curr_token == tok_ellipsis) {
      /* Skip over the ellipsis. */
      (void)get_token();
    }  /* if */ 
  } else {
    a_pack_expansion_descr_ptr	pedp = pesep->expansion_descr;
    check_assertion(pesep == pack_expansion_stack);
    if (!is_declarator && curr_token == tok_ellipsis) {
      /* Record the fact that the "..." has been seen. */
      record_pack_expansion_ellipsis();
    }  /* if */ 
    if (pesep->instantiation_descr == NULL) {
      /* Save the end of the token range for the pack. */
      pedp->last_token = curr_token_sequence_number;
      /* Get the pack references for this context from the scope stack.  This
         is only done if we have seen an ellipsis or if we know there is no
         enclosing expansion. */
      if (pedp->ellipsis_seen || pesep->next == NULL) {
        extract_pack_references_for_context(pedp);
      } else if (pesep->next != NULL) {
        /* Propagate the contains_pack_reference flag to the enclosing
           context. */
        if (pesep->contains_pack_reference) {
          pesep->next->contains_pack_reference = TRUE;
        }  /* if */
      }  /* if */
      if (pedp->packs_referenced != NULL) {
        if (pedp->ellipsis_seen) {
          /* There were expanded packs referenced and expanded.  This is a
             pack expansion. */
          record_pack_expansion(pedp);
          result_pedp = pedp;
        }  /* if */
      } else {
        /* There were no packs referenced.  This is not a pack expansion.
           If a pack expansion ("...") has been seen, issue an error that
           no packs were encountered. */
        if (pedp->ellipsis_seen &&
            !pesep->expansion_with_no_packs_diagnostic_issued) {
          pos_error(ec_expansion_contains_no_packs,
                    &pedp->ellipsis_position);
        }  /* if */
        /* Free the expansion descriptor entry. */
        free_pack_expansion_descr(pedp);
        pesep->expansion_descr = NULL;
        pesep = NULL;
      }  /* if */
    } else {
      /* Look for a pack expansion descriptor for this argument that should
         be inherited from an enclosing template declaration context. */
      result_pedp = get_curr_pack_expansion_descr_for_param(pesep);
    }  /* if */
  }  /* if */
  if (pesep != NULL) {
    /* If a "..." was not encountered, issue an error. */
    a_pack_expansion_descr_ptr	pedp = pesep->expansion_descr;
    if (!pedp->ellipsis_seen) {
      issue_pack_not_expanded_diagnostics(pedp->packs_referenced);
    }  /* if */
  }  /* if */
  return result_pedp;
}  /* end_potential_pack_expansion_context */


void discard_pack_references_for_context(
			a_pack_expansion_stack_entry_ptr	pesep)
/*
This is called just before end_potential_pack_expansion_context for a
potential pack expansion context (described by pesep) that is known not to be
a pack expansion, but that may contain pack references whose expansion cannot
be determined.  For example, in
  template <class... M>
  void f(M&... m) __attribute__((some_attr(m...)));
the arguments of the unrecognized attribute are recorded as raw tokens, so it
is not known whether m is expanded within them.  The pack references recorded
so far for the context are discarded without any diagnostic, so that
end_potential_pack_expansion_context neither records a pack expansion, nor
issues diagnostics for unexpanded packs, nor passes the references on to an
enclosing context.  pesep can be NULL, in which case nothing is done; nothing
is done either for suppression, rescan, or real instantiation contexts, in
which pack references are not recorded.
*/
{
  if (pesep != NULL && !pesep->is_rescan && !pesep->is_suppression &&
      pesep->instantiation_descr == NULL) {
    a_pack_expansion_descr_ptr	pedp = pesep->expansion_descr;
    check_assertion(pesep == pack_expansion_stack);
    /* The context ends here: Extract its pack references and free them. */
    pedp->last_token = curr_token_sequence_number;
    extract_pack_references_for_context(pedp);
#if DEBUG
    if (db_flag_is_set("packs") && pedp->packs_referenced != NULL) {
      fprintf(f_debug, "Discarding the references extracted above\n");
    }  /* if */
#endif /* DEBUG */
    free_list_of_pack_references(pedp->packs_referenced);
    pedp->packs_referenced = NULL;
    pesep->contains_pack_reference = FALSE;
  }  /* if */
}  /* discard_pack_references_for_context */


a_boolean advance_to_next_pack_element(a_pack_expansion_stack_entry_ptr	pesep)
/*
This routine is called after end_potential_pack_expansion_context has been
called, to update the symbols that refer to pack expansions so that they
refer to the next element of the pack, if any.  pesep describes the current
pack expansion, and can be NULL in an actual instantiation of a context that
did not turn out to be variadic.

TRUE is returned if there are any more elements in the pack.  FALSE otherwise.
*/
{
  a_boolean			done = FALSE;

  if (pesep == NULL) {
    /* We are not in a pack expansion.  Indicate that there are no further
       elements. */
    done = TRUE;
  } else if (pesep->is_suppression) {
    /* We are suppressing pack expansion processing. */
    done = TRUE;
    pesep = NULL;
  } else if (pesep->instantiation_descr == NULL) {
    /* We are in a prototype instantiation.  Indicate that there are no
       further elements. */
    done = TRUE;
  } else {
     a_pack_reference_ptr	param_prp;
     a_pack_reference_ptr	arg_prp;

    /* The pack expansion descriptor passed in should be on top of the
       stack. */
    check_assertion(pesep == pack_expansion_stack);
    for (param_prp = pesep->expansion_descr->packs_referenced,
           arg_prp = pesep->instantiation_descr->pack_status;
         param_prp != NULL;
         param_prp = param_prp->next, arg_prp = arg_prp->next) {
      a_symbol_ptr	sym = param_prp->symbol;
      check_assertion(arg_prp != NULL);
      if (param_prp->kind == prk_variable || param_prp->kind == prk_binding) {
        /* The symbol for the first pack element is found by lookup.  Update
           that symbol (pointed to by primary_pack_symbol) to point
           to the current variable to be used. */
        a_variable_ptr	vp = arg_prp->curr_argument.variable;
        a_variable_ptr	next_vp = vp == NULL ? NULL : vp->next;
        if (vp != NULL && vp->is_pack &&
            pesep->instantiation_descr->has_hybrid_pack_expansion) {
          /* Don't advance past a pack in a hybrid expansion. */
        } else if ((param_prp->kind == prk_variable &&
                    (next_vp == NULL ||
                     next_vp->variant.assoc_param_type == NULL ||
                     vp->variant.assoc_param_type->param_num !=
                              next_vp->variant.assoc_param_type->param_num)) ||
                   (param_prp->kind == prk_binding &&
                    (next_vp == NULL || !next_vp->is_pack_element))) {
          arg_prp->curr_argument.variable = NULL;
          done = TRUE;
        } else {
          arg_prp->curr_argument.variable = next_vp;
          arg_prp->primary_pack_symbol->variant.variable.ptr = next_vp;
        }  /* if */
      } else if (param_prp->kind == prk_template_param) {
        /* A template argument. */
        a_template_arg_ptr	tap = arg_prp->curr_argument.template_arg;
        a_template_arg_ptr	orig_tap = tap;
        /* Advance to the next argument, if any.  If the argument is not
           part of the pack, we are done with this expansion. */
        if (tap != NULL) {
          /* Don't advance past a pack in a hybrid expansion. */
          if (!tap->is_pack ||
              !pesep->instantiation_descr->has_hybrid_pack_expansion) {
            tap = tap->next;
          }  /* if */
          arg_prp->curr_argument.template_arg = tap;
        }  /* if */
        if (tap == NULL || !tap->is_pack_element) {
          /* Normally, we are done now.  But if we are preserving
             deduced packs, we are only done if this is the only
             pack being expanded. */
          a_boolean	is_preserved_pack_context;
          is_preserved_pack_context = pesep->is_rescan &&
                                      pesep->preserve_deduced_packs &&
                                      !arg_prp->uses_enclosing_pack;
          if (tap != NULL || !is_preserved_pack_context ||
              (arg_prp == pesep->instantiation_descr->pack_status &&
               arg_prp->next == NULL)) {
            done = TRUE;
          }  /* if */
          if (tap == NULL && orig_tap != NULL && is_preserved_pack_context) {
            /* We got to the end of substituted arguments.  Update the
               template parameter to point back to its original value. */
            restore_default_template_param(arg_prp->template_param);
          }  /* if */
        } else {
          if (!pesep->is_rescan && !pesep->is_deduction) {
            /* The symbols only need to be updated in actual instantiation
               contexts. */
            update_template_param_symbol(sym, tap);
          }  /* if */
        }  /* if */
      } else if (param_prp->kind == prk_init_capture) {
        /* A lambda init-capture. */
        a_field_ptr	fp = arg_prp->curr_argument.field;
        a_field_ptr	next_fp = fp == NULL ? NULL : fp->next;
        if (fp != NULL && fp->is_captured_pack_element &&
            pesep->instantiation_descr->has_hybrid_pack_expansion) {
          /* Don't advance past a pack in a hybrid expansion. */
        } else if (next_fp == NULL ||
            fp->source_corresp.name != next_fp->source_corresp.name) {
          arg_prp->curr_argument.field = NULL;
          done = TRUE;
          if (fp != NULL) {
            /* Reset the symbol for the first field. */
            a_field  *first_fp = fields_of(parent_class_of(fp));
            while (symbol_for(first_fp) != arg_prp->primary_pack_symbol) {
              first_fp = first_fp->next;
            }  /* while */
            arg_prp->primary_pack_symbol->variant.field.ptr = first_fp;
          }  /* if */
        } else {
          arg_prp->curr_argument.field = next_fp;
          arg_prp->primary_pack_symbol->variant.field.ptr = next_fp;
        }  /* if */
      } else if (param_prp->kind == prk_bases) {
        /* A template argument. */
        a_template_arg_ptr	tap = arg_prp->curr_argument.template_arg;
        /* Unless it's a pack in a hybrid expansion, advance to the next
           argument, if any. */
        if (!tap->is_pack ||
            !pesep->instantiation_descr->has_hybrid_pack_expansion) {
          tap = tap->next;
        }  /* if */
        arg_prp->curr_argument.template_arg = tap;
        if (tap == NULL) done = TRUE;
      } else {
        /* A parameter from a function prototype scope. */
        check_assertion(param_prp->kind == prk_parameter);
        if (pesep->is_rescan) {
          a_param_type_ptr	ptp;
          a_param_type_ptr	next_ptp;
          /* Advance to the next element on the list if the parameter number
             and level match the current value.  In some cases, no parameter
             is found because it belongs to an "inner" pack while substituting
             an "outer" pack (i.e., a pack from a parent template). */
          check_assertion(arg_prp != NULL);
          ptp = arg_prp->curr_argument.param_type;
          next_ptp = ptp != NULL ? ptp->next : NULL;
          if (ptp != NULL && ptp->is_parameter_pack &&
              pesep->instantiation_descr->has_hybrid_pack_expansion) {
            /* Don't advance past a pack in a hybrid expansion. */
          } else if (next_ptp == NULL || !next_ptp->is_pack_element ||
              ptp->param_num != next_ptp->param_num) {
            next_ptp = NULL;
            done = TRUE;
          }  /* if */
          arg_prp->curr_argument.param_type = next_ptp;
        } else {
          /* The symbol for the first pack element is found by lookup.  Update
             that symbol (pointed to by primary_pack_symbol) to point to the
             current param_id to be used. */
          a_param_id_ptr	param_id = arg_prp->curr_argument.param_id;
          a_param_id_ptr	next_param_id = param_id->next;
          arg_prp->curr_argument.param_id = next_param_id;
          if (param_id->is_parameter_pack &&
              pesep->instantiation_descr->has_hybrid_pack_expansion) {
            /* Don't advance past a pack in a hybrid expansion. */
          } else if (next_param_id == NULL ||
              param_id->param_num != next_param_id->param_num) {
            done = TRUE;
          } else {
            arg_prp->primary_pack_symbol->variant.param_id = next_param_id;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
    if (done && !pesep->is_rescan) {
      decrement_dependent_scans_for_reusable_cache();
    }  /* if */
  }  /* if */
  if (!done) {
    if (!pesep->is_rescan) {
      /* Reset the token position to the start of the pack expansion. */
      if (get_token_cache_being_scanned().ptr() ==
                                              pesep->first_token_cache.ptr()) {
        update_reusable_cache_rescan_location(pesep->first_token_cache.ptr(),
                                              pesep->first_token_tsn);
      } else {
        expect_error_str("active reusable cache changed unexpectedly");
      }  /* if */
    }  /* if */
    pesep->instantiation_descr->after_first_element = TRUE;
  } else if (pesep != NULL) {
    /* If we have advanced past the last element, pop the pack expansion
       stack. */
    pop_pack_expansion_stack();
  }  /* if */
  return !done;
}  /* advance_to_next_pack_element */


a_boolean skip_pack_index_iteration(
                                 a_pack_expansion_stack_entry_ptr  *p_pesep,
                                 a_pack_expansion_descr_ptr        pedep,
                                 a_boolean                         *p_any_more)
/*
Short-circuit pack iteration for a standalone pack-index construct.  A
pack-index that is not followed by "..." contributes a single element,
so there is nothing to iterate.  If the pending pack-expansion context
describes such a pack-index, abandon it, clear *p_pesep, set *p_any_more
to FALSE, and return TRUE to tell the caller to stop.  Otherwise return
FALSE so the caller can proceed with normal advance_to_next_pack_element
processing.
*/
{
  a_pack_expansion_stack_entry_ptr  pesep = *p_pesep;
  a_boolean                         is_pack_index;

  is_pack_index = pesep != NULL && pesep->instantiation_descr != NULL &&
                  pesep->expansion_descr != NULL &&
                  pesep->expansion_descr->is_pack_index &&
                  (pedep == NULL || pedep->is_pack_index);
  if (is_pack_index) {
    abandon_potential_pack_expansion_context(pesep);
    *p_pesep = NULL;
    *p_any_more = FALSE;
  }  /* if */
  return is_pack_index;
}  /* skip_pack_index_iteration */


void abandon_potential_pack_expansion_context(
				a_pack_expansion_stack_entry_ptr	pesep)
/*
A call has been made to begin_potential_pack_expansion_context, but we
have encountered something that caused us to discard this context, so the
pack expansion stack must be popped.
*/
{
  if (pesep != NULL && !pesep->is_suppression) {
    /* The pack expansion descriptor passed in should be on top of the
       stack. */
    check_assertion(pesep == pack_expansion_stack);
    if (pesep->instantiation_descr != NULL && !pesep->is_rescan) {
      /* begin_potential_pack_expansion_context increments the dependent-scan
         count for real instantiations.  If this context is abandoned (instead
         of being finished via advance_to_next_pack_element), balance that
         increment here. */
      decrement_dependent_scans_for_reusable_cache();
    }  /* if */
    /* Propagate the contains_pack_reference flag to the enclosing context. */
    if (pesep->next != NULL && pesep->contains_pack_reference) {
      pesep->next->contains_pack_reference = TRUE;
    }  /* if */
    pop_pack_expansion_stack();
  }  /* if */
}  /* abandon_potential_pack_expansion_context */


a_boolean any_packs_referenced(void)
/*
Return TRUE if the current pack expansion context contains any pack
references (including those from an enclosing pack context) and we are in
a template definition context.  See any_packs_referenced_in_curr_context
for more information.
*/
{
  a_boolean	result = FALSE;

  if (pack_expansion_stack != NULL &&
      !pack_expansion_stack->is_suppression) {
    a_pack_expansion_stack_entry_ptr	pesep = pack_expansion_stack;
    result = pesep->expansion_descr->packs_referenced != NULL;
    if (!result && is_prototype_instantiation_context()) {
      a_scope_stack_entry_ptr	ssep;
      ssep = get_scope_for_pack_references();
      result = ssep->packs_referenced != NULL;
    }  /* if */
  }  /* if */
  return result;
}  /* any_packs_referenced */


a_boolean any_packs_referenced_in_curr_context(void)
/*
Return TRUE if the current pack expansion context contains any pack
references and we are in a template definition context.  This differs
from any_packs_referenced in that this will only be TRUE if the
context on the top of the pack expansion stack contains a pack reference
while any_packs_referenced will be TRUE if the reference was in an
enclosing context.
*/
{
  a_boolean	result = FALSE;

  /* In a prototype instantiation context expansion_descr->packs_referenced
     will be NULL but contains_pack_reference could be TRUE.  In a real
     instantiation expansion_descr->packs_referenced might be TRUE but
     contains_pack_reference will be FALSE. */
  if (pack_expansion_stack != NULL &&
      !pack_expansion_stack->is_suppression) {
    a_pack_expansion_stack_entry_ptr	pesep = pack_expansion_stack;
    result = pesep->expansion_descr->packs_referenced != NULL ||
             pesep->contains_pack_reference;
  }  /* if */
  return result;
}  /* any_packs_referenced_in_curr_context */


void record_potential_pack_reference_full(
				a_symbol_ptr		pack_symbol,
				a_source_position_ptr	position,
				a_type_ptr		bases_type,
				a_boolean		direct_bases)
/*
This routine is called to determine whether pack_symbol is a reference
to a parameter pack, and if so, make a record that the particular
parameter pack has been referenced in the current variadic context.
The symbol passed in can be of any kind (but, an actual pack can only
be a template parameter symbol for a template parameter pack, or a
parameter or variable symbol for the parameter of a function parameter pack).
The source position of the use of the symbol is indicated by position.
It is also called to record the use of an implicit pack reference created
for the g++ __bases or __direct_bases type operators, in which case bases_type
is the type specified, and direct_bases is TRUE for the __direct_bases
form.
*/
{
  /* It is only possible to reference a pack expansion in a template definition
     context.  Don't record pack references during rescans or while expansion
     processing is suppressed -- just use the pack references from the
     definition. */
  if (is_uninstantiated_pack_expansion_context() &&
      !(pack_expansion_stack != NULL &&
       pack_expansion_stack->instantiation_descr != NULL) &&
      (pack_expansion_stack == NULL ||
       (!pack_expansion_stack->is_rescan &&
        !pack_expansion_stack->is_suppression))) {
    if (bases_type != NULL || symbol_is_pack(pack_symbol)) {
      /* Add this pack symbol to the list of packs in the scope stack
         entry. */
      a_pack_reference_ptr		prp;
      a_scope_stack_entry_ptr		ssep;
      a_pack_reference_ptr		*p_prp;
      if (pack_symbol != NULL && !pack_symbol->is_template_param &&
          pack_symbol->kind == (a_symbol_kind)sk_type) {
        /* For type symbols, strip off any typerefs.  This is not done for
           template parameter symbols as you want to use the actual parameter
           symbol. */
        a_type_ptr	tp = pack_symbol->variant.type.ptr;
        tp = skip_typerefs(tp);
        pack_symbol = symbol_for(tp);
        check_assertion(pack_symbol != NULL);
      }  /* if */
      ssep = get_scope_for_pack_references();
      p_prp = &ssep->packs_referenced;
      /* Look for an existing expansion of this symbol or type at this
         location. */
      for (prp = ssep->packs_referenced; prp != NULL;
           p_prp = &prp->next, prp = prp->next) {
        if (prp->symbol == pack_symbol &&
            prp->token_sequence_number == curr_token_sequence_number) {
          break;
        } else if (prp->token_sequence_number > curr_token_sequence_number) {
          /* The list is in token sequence number order.  Stop if we have
             found an entry beyond the one we are recording. */
          prp = NULL;
          break;
        }  /* if */
      }  /* for */
      if (prp == NULL) {
        /* An existing entry was not found.  Create a new one. */
        a_pack_reference_kind	kind;
        /* Determine the kind of entity being represented. */
        if (bases_type != NULL) {
          kind = prk_bases;
        } else if (symbol_is(pack_symbol, sk_variable)) {
          if (pack_symbol->variant.variable.ptr->is_struct_binding) {
            kind = prk_binding;
          } else {
            kind = prk_variable;
          }  /* if */
        } else if (symbol_is(pack_symbol, sk_parameter)) {
          kind = prk_parameter;
        } else if (symbol_is(pack_symbol, sk_field)) {
          kind = prk_init_capture;
        } else {
          kind = prk_template_param;
        }  /* if */
        prp = alloc_pack_reference(kind);
        prp->symbol = pack_symbol;
        if (kind == prk_variable || kind == prk_binding) {
          if (kind == prk_variable) {
            prp->param_or_binding_num = pack_symbol->variant.variable.ptr
                                        ->variant.assoc_param_type->param_num;
          }  /* if */
          if (depth_innermost_function_scope == NO_SCOPE_DEPTH ||
              pack_symbol->is_pack_expansion ||
              pack_symbol->decl_scope !=
                          scope_stack[depth_innermost_function_scope].number) {
            prp->uses_enclosing_pack = TRUE;
          }  /* if */
        } else if (kind == prk_parameter) {
          a_param_id_ptr  pip = pack_symbol->variant.param_id;
          prp->param_or_binding_num = pip->param_num;
          if (pip->uses_only_enclosing_pack) {
            prp->uses_enclosing_pack = TRUE;
          }  /* if */
        } else if (kind == prk_init_capture) {
          /* No additional information is needed for an init-capture. */
        } else if (kind == prk_bases) {
          prp->direct_bases = direct_bases;
        } else {
          /* Determine whether the template parameter referenced is from
             an enclosing pack. */
          a_scope_depth	depth = depth_innermost_instantiation_scope;
          if (depth < depth_template_declaration_scope) {
            depth = depth_template_declaration_scope;
          }  /* if */
          check_assertion(depth != NO_SCOPE_DEPTH);
          if ((pack_symbol != NULL &&
               pack_symbol->decl_scope != scope_stack[depth].number)) {
            prp->uses_enclosing_pack = TRUE;
          } else {
            if (scope_is(&scope_stack_top(), sck_func_prototype)) {
              /* This is a reference to a local pack.  Clear the flag that
                 indicates all references were to enclosing packs. */
              a_decl_parse_state_ptr  dps = scope_stack_top().decl_parse_state;
              if (dps != NULL && dps->param_with_only_enclosing_pack_refs) {
                dps->param_with_only_enclosing_pack_refs = FALSE;
              }  /* if */
            }  /* if */
          }  /* if */
          prp->coordinates = coordinates_of_template_param_symbol(pack_symbol);
        }  /* if */
        prp->position = *position;
        prp->token_sequence_number = curr_token_sequence_number;
        /* Insert this in the list of entries on the scope stack. */
        if (*p_prp != NULL) prp->next = *p_prp;
        *p_prp = prp;
#if DEBUG
        if (db_flag_is_set("packs")) {
          fprintf(f_debug, "Recording pack reference for ");
          if (pack_symbol != NULL) {
            db_symbol_name(pack_symbol);
          } else {
            db_type_name(bases_type);
          }  /* if */
          fprintf(f_debug, " at tsn %lu\n",
                  (unsigned long)curr_token_sequence_number);
        }  /* if */
#endif /* DEBUG */
      }  /* if */
      /* We don't associate pack references with contexts yet, but record
         the fact that the current context did contain a pack reference. */
      if (pack_expansion_stack != NULL) {
        pack_expansion_stack->contains_pack_reference = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
}  /* record_potential_pack_reference_full */


#if GNU_EXTENSIONS_ALLOWED

a_type_ptr get_type_for_bases_operator(
				a_type_ptr		bases_type,
				a_source_position_ptr	position,
				a_boolean		direct_bases)
/*
This routine is called to record an implicit pack reference created for the
g++ __bases or __direct_bases type operators.  It is called both during
prototype and real instantiations.  During a prototype instantiation,
it creates a pack reference to the bases_type.   During a real instantiation
it updates the pack reference for the instantiation to refer to bases_type.
position is the source position of the operator.  direct_bases is TRUE for
the __direct_bases form, FALSE otherwise.  During the prototype instantiation,
bases_type is returned.  During a real instantiation, an element of the base
class list is returned.
*/
{
  a_symbol_ptr	sym;
  a_type_ptr	result = NULL;

  sym = symbol_for(bases_type);
  if (is_prototype_instantiation_context()) {
    record_potential_pack_reference_full(sym, position,
                                         bases_type, direct_bases);
    result = bases_type;
  } else if (is_real_instantiation_context() &&
             pack_expansion_stack != NULL) {
    a_pack_reference_ptr		prp;
    a_pack_expansion_stack_entry_ptr	pesep;
    a_pack_instantiation_descr_ptr	pidp;
    pesep = pack_expansion_stack;
    pidp = pesep->instantiation_descr;
    prp = pidp == NULL ? NULL : pidp->pack_status;
    /* Look for an existing expansion of this symbol or type at this
       location. */
    for (; prp != NULL; prp = prp->next) {
      if (prp->token_sequence_number == curr_token_sequence_number) {
        result = prp->curr_argument.template_arg->variant.type;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  check_assertion_or_expect_error(result != NULL);
  if (result == NULL) result = error_type();
  return result;
}  /* get_type_for_bases_operator */

#endif /* GNU_EXTENSIONS_ALLOWED */


void record_pack_expansion_ellipsis_position(a_source_position  *ellipsis_pos)
/*
Called during the processing of a variadic template to record that an ellipsis
indicating a pack expansion was encountered, and at what position it appeared.
This marks an expansion of the pack on the top of the pack expansion stack.
Usually, this is called when the current token is the ellipsis, via
record_pack_expansion_ellipsis.  With C++17 fold expressions, however, one
ellipsis describes the expansion of a potential pack before and after the
ellipsis, and calling this routine directly is more appropriate.
*/
{
  if (is_uninstantiated_pack_expansion_context()) {
    if (pack_expansion_stack == NULL) {
      pos_error(ec_expansion_contains_no_packs, ellipsis_pos);
    } else {
      /* The instantiation_descr will be non-NULL for nondependent pack
         expansions in prototype instantiation contexts. */
      if (!pack_expansion_stack->is_suppression &&
          pack_expansion_stack->instantiation_descr == NULL) {
        a_pack_expansion_descr_ptr	pedp;
        pedp = pack_expansion_stack->expansion_descr;
        pedp->ellipsis_seen = TRUE;
        pedp->ellipsis_position = *ellipsis_pos;
      }  /* if */
    }  /* if */
  }  /* if */
}  /* record_pack_expansion_ellipsis_position */


void record_pack_expansion_ellipsis(void)
/*
Called during the processing of a variadic template when the "..."
indicating the pack expansion is detected.  This marks an expansion of
the pack on the top of the pack expansion stack.  The current token must
be "...", and that token is bypassed by this routine.

This routine is normally called by end_potential_pack_expansion_context.
It is only called elsewhere for pack expansions in declarators (see
end_potential_pack_expansion_context).
*/
{
  check_assertion(curr_token == tok_ellipsis);
  record_pack_expansion_ellipsis_position(&pos_curr_token);
  /* Bypass the "...". */
  (void)get_token();
}  /* record_pack_expansion_ellipsis */


a_boolean in_deprecated_or_unavailable_definition(void)
/*
Return TRUE if and only if we're inside the definition of an entity marked as
deprecated or unavailable.
*/
{
  a_boolean                result = FALSE;
  a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];

  for (; ssep != NULL; ssep = previous_scope_of(ssep)) {
    switch (ssep->kind) {
      case sck_file:
      case sck_namespace:
      case sck_namespace_extension:
        /* Namespace scopes cannot be marked as deprecated. */
        goto done;
      case sck_class_struct_union:
      case sck_class_reactivation:
      case sck_enum:
        if (ssep->assoc_type->source_corresp.is_deprecated_or_unavailable) {
          result = TRUE;
          goto done;
        }  /* if */
        break;
      case sck_function:
        if (ssep->assoc_routine->source_corresp.is_deprecated_or_unavailable) {
          result = TRUE;
          goto done;
        }  /* if */
        break;
      default:
        /* Nothing to do. */
        break;
    }  /* switch */
  }  /* for */
done:
  return result;
}  /* in_deprecated_or_unavailable_definition */


a_boolean in_ms_nonreal_class_instantiation(void)
/*
Return TRUE if we are in a Microsoft nonreal class instantiation (a kind of
instantiation performed to permit lookups in dependent base classes).
*/
{
  a_boolean  result = FALSE;

  if (scope_stack_top().in_nonreal_instantiation) {
    a_type_ptr  tp =
                  scope_stack[depth_innermost_instantiation_scope].assoc_type;
    if (tp != NULL && is_immediate_class_type(tp) &&
        tp->variant.class_struct_union.is_ms_instantiated_nonreal_class) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* in_ms_nonreal_class_instantiation */


a_scope_ptr get_innermost_function_scope(void)
/*
Returns the innermost function scope (or NULL if there is none).  Typically
the same as innermost_function_scope, but not in all cases (say, when scanning
a local class or substituting an alias template).
*/
{
  a_scope_ptr              result = innermost_function_scope;
  a_scope_stack_entry_ptr  ssep = &scope_stack_top();

  if (result == NULL) {
    a_boolean  scopes_skipped = FALSE;
    /* Skip alias template instantiations. */
    while (scope_is(ssep, sck_template_instantiation) &&
           ssep->instance_sym != NULL &&
           symbol_is(ssep->instance_sym, sk_type)) {
      /* Skip enclosing class and namespace reactivations. */
      while (scope_is(ssep-1, sck_class_reactivation) ||
             scope_is(ssep-1, sck_namespace_reactivation) ||
             scope_is(ssep-1, sck_namespace_extension)) {
        ssep -= 1;
        /* This may be an instantiated class reactivation: */
        if (scope_is(ssep-1, sck_template_instantiation)) {
          ssep -= 1;
        }  /* if */
      }  /* while */
      check_assertion(scope_is(ssep-1, sck_instantiation_context));
      ssep -= 2;
      scopes_skipped = TRUE;
    }  /* while */
    if (scopes_skipped || inside_local_class) {
      for (; ssep != &scope_stack[DEPTH_OF_FILE_SCOPE];
                                             ssep = previous_scope_of(ssep)) {
        if (ssep->il_scope != NULL && scope_is(ssep->il_scope, sck_function)) {
          result = ssep->il_scope;
          break;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  return result;
}  /* get_innermost_function_scope */


a_scope_depth get_depth_innermost_function_scope(void)
/*
Return the depth of the innermost function scope (or NO_SCOPE_DEPTH if
there is none).  Typically the same as depth_innermost_function_scope, but
can be different (e.g., when the current scope is a local class).
*/
{
  a_scope_depth depth = depth_innermost_function_scope;

  if (depth == NO_SCOPE_DEPTH && inside_local_class) {
    /* Scan back through the scope stack for the nearest function scope. */
    for (depth = depth_scope_stack;
         depth != NO_SCOPE_DEPTH &&
                         scope_stack[depth].kind != (a_scope_kind)sck_function;
         depth = scope_stack[depth].previous_scope) {}
  }  /* if */
  return depth;
}  /* get_depth_innermost_function_scope */


a_boolean src_seq_entries_permitted_in_il(void)
/*
Return TRUE if source sequence entries might appear in the IL tree.
*/
{
  a_boolean  result = GENERATE_SOURCE_SEQUENCE_LISTS;

#if GENERATE_SOURCE_SEQUENCE_LISTS
#if DO_IL_LOWERING && !PRESERVE_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING
  if (result) {
    result = !any_lowering_being_done;
  }  /* if */
#endif /* DO_IL_LOWERING && !PRESERVE_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  return result;
}  /* src_seq_entries_permitted_in_il */

#if DEBUG

unsigned long db_show_scope_stack_space_used(unsigned long grand_total)
/*
Show space used by the scope_stack routines.  This is called by
the symbol table space used routine.  The space used by the scope_stack
routines is reported as part of the symbol table memory used.
*/
{
  unsigned long	num;
  unsigned long	size;
  unsigned long	total;

#if DO_IL_LOWERING && ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS
  db_space_used_lost("string literal tables", avail_string_literal_tables,
                     num_string_literal_tables_allocated,
                     a_string_literal_table);
  db_space_used_lost("string literal entries",
                     avail_string_literal_table_entries,
                     num_string_literal_table_entries_allocated,
                     a_string_literal_table_entry);
#endif /* DO_IL_LOWERING && ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS */
  db_space_used_lost("func. shareable constants",
                     avail_function_shareable_constants_tables,
                     num_function_shareable_constants_tables_allocated,
                     a_function_shareable_constants_table);
  db_space_used_lost("loc. static var. locators",
                     avail_c99_inline_definition_locators,
                     num_c99_inline_definition_locators_allocated,
                     a_c99_inline_definition_locator);
#if DO_IL_LOWERING && MODULE_ID_NEEDED && !STANDALONE_UTILITY_PROGRAM
  db_space_used("wait for mod. id entries",
                num_delayed_lowering_list_entries_allocated,
                a_delayed_lowering_list_entry);
#endif /* DO_IL_LOWERING && MODULE_ID_NEEDED && !STANDALONE_UTILITY_PROGRAM */
  db_space_used_lost("pack exp. stack entries",
                     avail_pack_expansion_stack_entries,
                     num_pack_expansion_stack_entries_allocated,
                     a_pack_expansion_stack_entry);
  db_space_used_lost("pack references",
                     avail_pack_references,
                     num_pack_references_allocated,
                     a_pack_reference);
  db_space_used_lost("pack expansion descrs",
                     avail_pack_expansion_descrs,
                     num_pack_expansion_descrs_allocated,
                     a_pack_expansion_descr);
  db_space_used_lost("pack instantiation descrs",
                     avail_pack_instantiation_descrs,
                     num_pack_instantiation_descrs_allocated,
                     a_pack_instantiation_descr);
  return grand_total;
}  /* db_show_scope_stack_space_used */

#endif /* DEBUG */

void scope_stk_one_time_init(void)
/*
Do one-time initialization of variables related to scope stack management.
(Variables that need to be reinitialized with each new translation unit
are handled in scope_stk_init.)
*/
{
  /* Save variables from scope_stk.h and scope_stk.c that are needed for
     precompiled headers */
  if (precompiled_header_processing_required) {
    STATIC_THREAD a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(num_classes_on_scope_stack),
      pch_saved_var_array_elem(avail_names_hidden_by_old_for_init),
      pch_saved_var_array_elem(name_linkage_stack),
      pch_saved_var_array_elem(pack_expansion_stack),
      pch_saved_var_array_elem(avail_pack_expansion_stack_entries),
      pch_saved_var_array_elem(avail_pack_references),
      pch_saved_var_array_elem(avail_pack_expansion_descrs),
      pch_saved_var_array_elem(avail_pack_instantiation_descrs),
      pch_saved_var_array_elem(avail_name_linkage_stack_entries),
      pch_saved_var_array_elem(avail_function_shareable_constants_tables),
      pch_saved_var_array_elem(
                  function_body_processing_delayed_on_some_func_in_primary_il),
      pch_saved_var_array_elem(saved_stmt_stack_stack),
#if NEED_NAME_MANGLING
      pch_saved_var_array_elem(avail_collision_tables),
      pch_saved_var_array_elem(last_file_scope_unnamed_type_number),
      pch_saved_var_array_elem(last_file_scope_closure_type_number),
#endif /* NEED_NAME_MANGLING */
#if DO_IL_LOWERING && ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS
      pch_saved_var_array_elem(avail_string_literal_tables),
      pch_saved_var_array_elem(avail_string_literal_table_entries),
#if DEBUG
      pch_saved_var_array_elem(num_string_literal_tables_allocated),
      pch_saved_var_array_elem(num_string_literal_table_entries_allocated),
#endif /* DEBUG */
#endif /* DO_IL_LOWERING && ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS */
#if DO_IL_LOWERING && MODULE_ID_NEEDED && !STANDALONE_UTILITY_PROGRAM
      pch_saved_var_array_elem(waiting_for_module_id_list_head),
      pch_saved_var_array_elem(waiting_for_module_id_list_tail),
#if DEBUG
      pch_saved_var_array_elem(num_delayed_lowering_list_entries_allocated),
#endif /* DEBUG */
#endif /* DO_IL_LOWERING && MODULE_ID_NEEDED && !STANDALONE_UTILITY_PROGRAM */
      pch_saved_var_array_elem(c99_inline_definition_locators_to_check),
      pch_saved_var_array_elem(avail_c99_inline_definition_locators),
#if DEBUG
      pch_saved_var_array_elem(
                            num_c99_inline_definition_locators_allocated),
#endif /* DEBUG */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  register_trans_unit_variable(scope_stack);
  register_trans_unit_variable(size_scope_stack);
  register_trans_unit_variable(depth_scope_stack);
  register_trans_unit_variable(depth_of_initial_lookup_scope);
  register_trans_unit_variable(decl_scope_level);
  register_trans_unit_variable(depth_innermost_function_scope);
  register_trans_unit_variable(innermost_function_scope);
  register_trans_unit_variable(depth_innermost_instantiation_scope);
  register_trans_unit_variable(depth_template_declaration_scope);
  register_trans_unit_variable(curr_deferred_access_scope);
  register_trans_unit_variable(inside_local_class);
  register_trans_unit_variable(non_local_class_fixup_depth);
  register_trans_unit_variable(depth_innermost_namespace_scope);
  register_trans_unit_variable(
                         depth_of_innermost_scope_that_affects_access_control);
  register_trans_unit_variable(num_classes_on_scope_stack);
  register_trans_unit_variable(pack_expansion_stack);
  register_trans_unit_variable(saved_stmt_stack_stack);
  register_trans_unit_variable(closure_class_to_lambda_map);
#if NEED_NAME_MANGLING
  register_trans_unit_variable(last_file_scope_unnamed_type_number);
  register_trans_unit_variable(last_file_scope_closure_type_number);
#endif /* NEED_NAME_MANGLING */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  register_trans_unit_variable(source_sequence_entries_disallowed);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
}  /* scope_stk_one_time_init */


void scope_stk_trans_unit_init(void)
/*
Initialize variables related to scope stack processing that are specific to a
given translation unit.
*/
{
  scope_stack = NULL;
  size_scope_stack = 0;
  depth_scope_stack = NO_SCOPE_DEPTH;
  depth_of_initial_lookup_scope = NO_SCOPE_DEPTH;
  decl_scope_level = NO_SCOPE_DEPTH;
  depth_innermost_function_scope = NO_SCOPE_DEPTH;
  innermost_function_scope = NULL;
  depth_innermost_instantiation_scope = NO_SCOPE_DEPTH;
  depth_template_declaration_scope = NO_SCOPE_DEPTH;
  curr_deferred_access_scope = NO_SCOPE_DEPTH;
  inside_local_class = FALSE;
  depth_innermost_namespace_scope = NO_SCOPE_DEPTH;
  depth_of_innermost_scope_that_affects_access_control = NO_SCOPE_DEPTH;
  num_classes_on_scope_stack = 0;
  pack_expansion_stack = NULL;
  c99_inline_definition_locators_to_check = NULL;
  saved_stmt_stack_stack = new_fe<a_saved_stmt_stack_stack>();
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Source sequence entries are always suppressed when compiling secondary
     translation units. */
  source_sequence_entries_disallowed = !is_primary_translation_unit;
#if DO_IL_LOWERING
  /* Suppress source sequence entries when IL lowering is done. */
#if !PRESERVE_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING
  any_lowering_being_done = (C_mode() ? c99_il_lowering_needed() :
                                        il_lowering_needed());
  source_sequence_entries_disallowed |= any_lowering_being_done;
#endif /* !PRESERVE_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING */
#endif /* DO_IL_LOWERING */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  closure_class_to_lambda_map =
                              alloc_fe_of_type(a_closure_class_to_lambda_map);
  construct(closure_class_to_lambda_map, /*mask_width=*/10u);
#if NEED_NAME_MANGLING
  last_file_scope_unnamed_type_number = 0;
  last_file_scope_closure_type_number = 0;
#endif /* NEED_NAME_MANGLING */
}  /* scope_stk_trans_unit_init */


void scope_stk_init(void)
/*
Initialize static variables related to scope stack management.  This is
done as a subroutine (rather than relying on static initialization) so that it
can be redone to compile more than one source file in a single invocation
of the front end.
*/
{
  depth_of_innermost_scope_that_affects_access_control = NO_SCOPE_DEPTH;
  depth_of_initial_lookup_scope = NO_SCOPE_DEPTH;
  num_classes_on_scope_stack = 0;
  non_local_class_fixup_depth = DEPTH_OF_FILE_SCOPE;
  avail_names_hidden_by_old_for_init = NULL;
  name_linkage_stack = NULL;
  avail_name_linkage_stack_entries = NULL;
  module_reuse_state_stack = alloc_fe_of_type(
                                             a_module_scope_reuse_state_array);
  construct(module_reuse_state_stack);
  avail_function_shareable_constants_tables = NULL;
  avail_pack_expansion_stack_entries = NULL;
  avail_pack_expansion_descrs = NULL;
  avail_pack_references = NULL;
  avail_pack_instantiation_descrs = NULL;
#if NEED_NAME_MANGLING
  avail_collision_tables = NULL;
#endif /* NEED_NAME_MANGLING */
#if DO_IL_LOWERING && ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS
  avail_string_literal_tables = NULL;
  avail_string_literal_table_entries = NULL;
#if DEBUG
  num_string_literal_table_entries_allocated = 0;
  num_string_literal_tables_allocated = 0;
#endif /* DEBUG */
#endif /* DO_IL_LOWERING && ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS */
  avail_c99_inline_definition_locators = NULL;
#if DEBUG
  num_c99_inline_definition_locators_allocated = 0;
  num_function_shareable_constants_tables_allocated = 0;
  num_pack_expansion_stack_entries_allocated = 0;
  num_pack_references_allocated = 0;
  num_pack_expansion_descrs_allocated = 0;
  num_pack_instantiation_descrs_allocated = 0;
#endif /* DEBUG */
#if DO_IL_LOWERING && MODULE_ID_NEEDED && !STANDALONE_UTILITY_PROGRAM
  waiting_for_module_id_list_head = NULL;
  waiting_for_module_id_list_tail = NULL;
#if DEBUG
  num_delayed_lowering_list_entries_allocated = 0;
#endif /* DEBUG */
#endif /* DO_IL_LOWERING && MODULE_ID_NEEDED && !STANDALONE_UTILITY_PROGRAM */
  function_body_processing_delayed_on_some_func_in_primary_il = FALSE;
  depth_tentative_pack_expansions = 0;
}  /* scope_stk_init */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

