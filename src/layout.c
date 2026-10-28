/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

layout.c -- Managing the layout of class objects, based on user
            declarations and implementation conventions.

General comments on class layout

I.  Unions

  The offset of each nonstatic data member of a union is zero bytes from
  the starting address of the union object.  Its size is large enough to
  contain the largest member, and its alignment is that of the member with
  the greatest alignment requirement.  Unions cannot have base classes so
  no space is reserved for them (nor for pointers to data sections of
  virtual base classes).  Unions cannot have virtual functions, so no
  space is reserved for a "virtual function info" field.

II.  Classes and structs

A.  C vs. C++

  C structs are a special case of C++ structs.  They are classes without
  base classes and virtual functions, so space is required for fields
  only.  Fields are put out in strict declaration order and aligned based
  on data type.  The alignment requirement for the struct as a whole is that
  of the member with the greatest alignment requirement.

  C++ classes (in the generic sense) that are not unions have more
  complicated layouts.  The language definition provides few requirements
  on how a class is laid out, so there are potential incompatibilities
  between different language processors in (1) the order and locations of
  nonstatic data members, base class data sections, and pointers to
  virtual base classes, and (2) the representation of virtual function
  information.

B.  Layout options

  The EDG C++ front end provides several options in laying out a class,
  including compatibility with the layout of AT&T's cfront and a "normal"
  option that uses memory more efficiently than cfront's layout.  There is
  also an option that controls whether fields are put out in strict
  declaration order or grouped by accessibility.  The front end can be
  configured as required by setting CFRONT_OBJECT_CODE_COMPATIBILITY and
  TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE, both of which are
  defined in targ_def.h.

  The normal layout is as follows:

    1. Nonvirtual direct base class data sections, in declaration order.
       (Such base class data sections are referred to as "incomplete
       subobjects" because the space reserved for them does not include
       space for their own virtual base classes.  All virtual base classes,
       direct and indirect, appear at the end of layout.)  If the empty base
       class optimization is enabled (targ_optimize_empty_base_class_layout),
       then empty bases and fields with empty class type and the
       [[no_unique_address]] attribute are allocated in a separate pass after
       the nonempty bases (see set_offsets_for_empty_nonvirtual_base_classes).

    2. Nonstatic data members, in declaration order.

    3. The virtual function information "block"; in a typical
       implementation this will be space reserved for a pointer to a
       virtual function table. (An optimization allows the sharing of
       virtual function tables between a derived class and a nonvirtual
       base class, in which case this pointer will be omitted.)

    4. Pointers to direct and some indirect virtual base class subobjects.
       (A pointer-sharing optimization results in the omission of pointers
       to indirect virtual base classes in most cases.)

    5. The data sections for all direct and indirect virtual base classes.
       These appear as incomplete subobjects, in an order based on
       declaration order (namely, the order of their appearance on the base
       classes list, which is the same as the order of their construction:
       depth-first left-to-right traversal of the directed acyclic graph of
       the base classes).

  When targ_field_alloc_sequence_equals_decl_sequence is FALSE, only item
  2 of the normal layout is modified.  The order of nonstatic data
  members becomes:  all public members, in declaration order; then, all
  protected members, in declaration order; and lastly, all private members,
  in declaration order.  (This option is provided based on a suggestion
  in the commentary section of ARM 11.1.)

  When CFRONT_OBJECT_CODE_COMPATIBILITY is TRUE, items 1, 4, and 5 of the
  normal layout are affected.

    1. Nonvirtual direct base classes, in declaration order.  The first is
       an incomplete subobject; any others are complete -- i.e., space is
       reserved for their virtual base classes.  (This may result in wasted
       space, since a virtual base class may appear in association with
       more than one nonvirtual direct base class.)

    2. Nonstatic data members, in declaration order.

    3. Virtual function table pointer, unless omitted because the table is
       shared with the first nonvirtual direct base class.

    4. Pointers to direct and some indirect virtual base class subobjects.
       (The pointer-sharing optimization is the same as that for the
       normal layout.)  The order in which the pointers are put out cannot
       be summarized easily, since it appears to be an accidental artifact
       of the algorithm and internal data structures employed by cfront.

    5. All virtual base classes (generally direct but sometimes indirect)
       that are not embedded in some other subobject, in an order that
       again is an artifact of cfront's implementation.  These virtual
       base class data sections are put out as complete subobjects.

  In general, there are two advantages of the normal layout over the
  cfront-compatible layout.  First, and most importantly, it may result in
  smaller objects, since virtual base classes data sections are guaranteed
  to be put out only once.  Second, the order in which virtual base class
  pointers and virtual base class data sections appear in the layout is
  predictable, not an accident of the implementation.

  For detailed information about cfront layout compatibility issues please
  refer to the functions involved, including set_data_section_base_class
  in class_decl.c.
*/


/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Additional header files. */
#include "layout.h"
#if DEBUG || IA64_ABI
#include "class_decl.h"
#endif /* DEBUG || IA64_ABI */
#include "pch.h"
#include "pragma.h"

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
Macro that returns TRUE if the specified field should be treated as an
empty class for the purposes of layout.  That's the case when the
[[no_unique_address]] attribute is applied to an field whose type is an empty
class.
*/
#define is_empty_field_for_layout_purposes(field)                         \
 ((field)->has_no_unique_address_attribute &&                             \
  is_empty_class_type((field)->type))

/* Data structure to track some information about the layout of a class
   as it is being constructed. */
typedef struct a_layout_block *a_layout_block_ptr;
typedef struct a_layout_block {
  a_type_ptr	class_type;
			/* Pointer to the class type whose layout is being
			   defined. */
  a_targ_size_t	byte_offset;
			/* Byte offset (relative to the start of the class
			   object) for the *next* field to be entered. */
  an_unnormalized_bit_offset
		bit_offset;
			/* Bit offset relative to byte_offset for the *next*
			   bit field to be entered. */
  a_targ_alignment
		alignment;
			/* The alignment for the class object as a whole,
			   never less than the alignment required for any
			   field or subobject of that class. */
  a_byte_boolean
		any_overflow;
			/* Set to TRUE when the layout exceeds the maximum
			   size allowed for a class object. */
  a_type_ptr
		curr_container_type;
			/* Used only when targ_microsoft_bit_field_allocation
			   is TRUE, the type of the current bit field
			   container. */
  an_unnormalized_bit_offset
		curr_container_avail_bits;
			/* Used only when targ_microsoft_bit_field_allocation
			   is TRUE and when curr_container_type is non-NULL,
			   the number of bits in the current container that
			   remain unused. */
#if IA64_ABI
  a_targ_size_t
		curr_base_extent;
			/* The number of leading bytes occupied by base class
			   subobjects.  This is used to optimize the layout
			   process of fields in the IA-64 ABI. */
  a_targ_size_t
		curr_extent;
			/* The number of leading bytes occupied by subobjects
			   (base classes and fields).  This is used to
			   optimize the layout of fields in the IA-64 ABI. */
  a_targ_size_t
		min_final_class_size;
			/* The minimum size for the class.  This is used
			   in the "finalization" step to ensure the class
			   encompasses any potentially-overlapping data
			   members. */
  a_base_class_ptr
		trailing_nonempty_base;
			/* The nonempty direct or virtual base class that has
			   been given the highest offset so far. */
#endif /* IA64_ABI */
} a_layout_block;


static void clear_layout_block(a_layout_block_ptr  lob,
                               a_type_ptr          class_type)
/*
Clear the block used to contain information while working out class layout.
*/
{
  lob->class_type = class_type;
  lob->byte_offset = 0;
  lob->bit_offset = 0;
  lob->alignment = targ_minimum_struct_alignment;
  lob->any_overflow = FALSE;
  lob->curr_container_type = NULL;
  lob->curr_container_avail_bits = 0;
#if IA64_ABI
  lob->curr_base_extent = 0;
  lob->curr_extent = 0;
  lob->min_final_class_size = 0;
  lob->trailing_nonempty_base = NULL;
#endif /* IA64_ABI */
}  /* clear_layout_block */



/* An entry on the pack alignment stack.  a_pack_alignment_stack_entry_ptr
   is already defined in layout.h. */
typedef struct a_pack_alignment_stack_entry {
  a_pack_alignment_stack_entry_ptr
		next;
			/* Next entry on the stack.  NULL for the entry at
			   the bottom of the stack. */
  a_const_char	*name;
			/* The identifying name of this stack entry -- used
			   for "targeted" popping.  May be NULL. */
  a_targ_alignment
		alignment;
			/* The alignment associated with this stack entry.
			   May be zero, to indicate that the global default
			   should be used. */
} a_pack_alignment_stack_entry;


STATIC_THREAD a_pack_alignment_stack_entry_ptr
		pack_alignment_stack;
			/* Pointer to the top of the stack of pack alignment
			   values produced by #pragma pack directives. */

STATIC_THREAD a_pack_alignment_stack_entry_ptr
		avail_pack_alignment_stack_entries;
			/* List of pack alignment stack entries freed and
			   available for reuse. */


void reset_pack_alignment_state(a_targ_alignment            alignment,
                                a_pack_alignment_state_ptr  state)
/*
Called in C++ when entering a (non-prototype) class template instantiation
or a function definition.  The default pack alignment is reset to the
indicated alignment and the pack alignment stack is temporarily suspended
(i.e., its pointer is cleared).
*/
{
  a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];

  state->saved_max_member_alignment = curr_max_member_alignment;
  state->saved_pack_alignment_stack = pack_alignment_stack;
  if (ssep->kind == (a_scope_kind)sck_class_struct_union &&
      ssep->in_prototype_instantiation) {
  } else {
    curr_max_member_alignment = alignment;
    pack_alignment_stack = NULL;
  }  /* if */
}  /* reset_pack_alignment_state */


void restore_pack_alignment_state(a_pack_alignment_state_ptr  state)
/*
Called to C++ to restore the default pack alignment and pack alignment
stack when exiting a class template instantiation or function definition.
*/
{
  curr_max_member_alignment = state->saved_max_member_alignment;
  if (pack_alignment_stack != NULL) {
    /* Issue a diagnostic? */
  }  /* if */
  pack_alignment_stack = state->saved_pack_alignment_stack;
}  /* restore_pack_alignment_state */


static void push_pack_alignment(a_const_char      *name,
                                a_targ_alignment  alignment)
/*
Push an entry onto the top of the pack alignment stack, initializing it with
the specified name (which may be a NULL pointer) and alignment (which may be
zero).
*/
{
  a_pack_alignment_stack_entry_ptr  pasep;

  if (avail_pack_alignment_stack_entries != NULL) {
    pasep = avail_pack_alignment_stack_entries;
    avail_pack_alignment_stack_entries = pasep->next;
  } else {
    pasep = (a_pack_alignment_stack_entry_ptr)alloc_fe(
                                   sizeof(a_pack_alignment_stack_entry));
  }  /* if */
  pasep->name = name;
  pasep->alignment = alignment;
  pasep->next = pack_alignment_stack;
  pack_alignment_stack = pasep;
}  /* push_pack_alignment */


static void pop_pack_alignment(void)
/*
Pop the top entry from the pack alignment stack and return it to the available
list.
*/
{
  a_pack_alignment_stack_entry_ptr  pasep = pack_alignment_stack;

  pack_alignment_stack = pasep->next;
  pasep->next = avail_pack_alignment_stack_entries;
  avail_pack_alignment_stack_entries = pasep;
}  /* pop_pack_alignment */


static a_pack_alignment_stack_entry_ptr find_pack_alignment_stack_entry(
                                                           a_const_char  *name)
/*
Search the pack alignment stack for an entry that matches "name" and if it's
found return a pointer to it.  Return NULL if it's not found.
*/
{
  a_pack_alignment_stack_entry_ptr  pasep = pack_alignment_stack;

  if (name != NULL) {
    for (; pasep != NULL; pasep = pasep->next) {
      if (pasep != NULL && pasep->name != NULL &&
          strcmp(name, pasep->name) == 0) break;
    }  /* for */
  }  /* if */
  return pasep;
}  /* find_pack_alignment_stack_entry */


a_boolean check_pack_alignment_value(a_host_large_integer value,
                                     a_targ_alignment     *alignment)
/*
Check to be sure value is a valid "pack alignment" -- that it is a power of
2 within the range of the minimum and maximum allowed.
*/
{
  a_boolean  err = FALSE;

  if (value >= (a_host_large_integer)targ_minimum_pack_alignment &&
      value <= (a_host_large_integer)targ_maximum_pack_alignment &&
      (value & (value-1)) == 0) {
    *alignment = (a_targ_alignment)value;
  } else {
    err = TRUE;
  }  /* if */
  return !err;
}  /* check_pack_alignment_value */


void pack_pragma(a_pending_pragma_ptr ppp)
/*
Scan and process a cached #pragma pack directive, which is used to control
the alignment of nonstatic data members (and thereby the layout of structs).
The syntax is:

   #pragma pack(n)
   #pragma pack()

where n is a "pack alignment", the maximum alignment that may be assigned to
any nonstatic data member (even if it is less than the normal alignment for
the member's type).  If n is omitted, the pack alignment effectively reverts
to the default value, if any, that was specified on the command line.  The
current value is stored in curr_max_member_alignment and is copied into the
type entry for a class, struct, or union when layout processing commences.

The "enhanced syntax" is also supported:

   #pragma pack(push {, name} {, n})
   #pragma pack(pop  {, name} {, n})
   #pragma pack(show)

"push" mean to push curr_max_member_alignment onto the pack alignment stack.
A name may be provided to identify the entry for a targeted pop.  When n is
specified, that becomes the new curr_max_member_alignment; otherwise the old
one is retained.  "pop" with a name means to remove all entries on the pack
alignment stack down to and including the named entry; without a name, only
the top entry is removed.  curr_max_member_alignment is then set either to n
if n is supplied or to the value associated with the last entry popped.
The only effect of "show" is to issue a warning displaying the value of
curr_max_member_alignment.
*/
{
  a_boolean            err = FALSE;
  a_boolean            is_push = FALSE, is_pop = FALSE, is_show = FALSE;
  a_host_large_integer val;
  an_error_severity    severity;
  a_boolean            updated = FALSE, pragma_ignored = FALSE;

  db_enter(3, "pack_pragma");
  /* Save the stop token state, push a pragma scope, etc. */
  begin_rescan_of_pragma_tokens(ppp);
  add_stop_token(tok_rparen);
  add_stop_token(tok_identifier);
  add_stop_token(tok_int_constant);
  /* Check for a left parenthesis. */
  if ((microsoft_mode || gnu_mode) && curr_token != tok_lparen) {
    /* Microsoft and GNU issue a warning. */
    pos_warning(ec_exp_lparen, &error_position);
  } else {
    (void)required_token(tok_lparen, ec_exp_lparen);
  }  /* if */
  if (curr_token == tok_identifier) {
    /* Issue warnings in Microsoft and GNU modes for incorrect "push" and "pop"
       uses, errors otherwise. */
    severity = (microsoft_mode || gnu_mode) ? es_warning : es_error;
    /* Scan the tokens of the "enhanced syntax", except for the integer
       constant if any.  First check for "push" and "pop". */
    if (locator_for_curr_id.symbol_header->identifier_length == 4 &&
        strncmp(locator_for_curr_id.symbol_header->identifier,
                "push", size_t_arg(4)) == 0) {
      is_push = TRUE;
      /* Advance past "push". */
      (void)get_token();
    } else if (locator_for_curr_id.symbol_header->identifier_length == 3 &&
               strncmp(locator_for_curr_id.symbol_header->identifier,
                       "pop", size_t_arg(3)) == 0) {
      is_pop = TRUE;
      if (pack_alignment_stack == NULL) {
        /* In Microsoft and GNU modes we issue a warning on specifying "pop"
           when the stack is empty.  In default mode it's an error. */
        diagnostic(severity, ec_empty_pack_alignment_stack);
      }  /* if */
      /* Advance past "pop". */
      (void)get_token();
    } else if (locator_for_curr_id.symbol_header->identifier_length == 4 &&
               strncmp(locator_for_curr_id.symbol_header->identifier,
                       "show", size_t_arg(4)) == 0) {
      a_number_buffer val_str;

      is_show = TRUE;
      if (curr_max_member_alignment != 0) {
        val_str.reset_to((unsigned long long)curr_max_member_alignment);
      } else {
        val_str.reset_to("not set");
      }  /* if */
      pos_warning(ec_value_of_pragma_pack_show, &error_position,
                  val_str.as_temp_characters());
      /* Advance past "show". */
      (void)get_token();
    }  /* if */
    if (is_push || is_pop) {
      /* Do special processing for managing the pack alignment stack. */
      a_const_char                      *name = NULL;
      a_pack_alignment_stack_entry_ptr  pasep = NULL;

      /* Next should be either the closing parenthesis or a comma. */
      if (curr_token != tok_rparen) {
        /* Advance past the comma.  It should be followed by either an
           identifier or an integer constant. */
        (void)required_token(tok_comma, ec_exp_comma);
        if (curr_token == tok_identifier) {
          name = locator_for_curr_id.symbol_header->identifier;
          if (is_pop && pack_alignment_stack != NULL) {
            /* Be sure the name specified is on the stack.  If it's not,
               issue a diagnostic and leave the stack intact. */
            pasep = find_pack_alignment_stack_entry(name);
            if (pasep == NULL) {
              /* In Microsoft mode we issue a warning on specifying "pop" for
                 a name that's not on the stack.  In default mode it's an
                 error. */
              pos_st_diagnostic(severity, ec_not_found_on_pack_alignment_stack,
                                &pos_curr_token, name);
            } else {
              /* If the entry was found, pop off all the intervening entries
                 now.  The last one is popped off later. */
              while (pasep != pack_alignment_stack) pop_pack_alignment();
            }  /* if */
          }  /* if */
          /* Advance past the name. */
          (void)get_token();
          /* Next should be either the closing parenthesis or a comma. */
          if (curr_token != tok_rparen) {
            /* Advance past the comma -- the next token should be the integer
               constant. */
            (void)required_token(tok_comma, ec_exp_comma);
          }  /* if */
        }  /* if */
      }  /* if */
      /* Final processing for "push" and "pop". */
      if (is_push) {
        /* Push the entry onto the stack, saving the current pack alignment
           before it's overwritten by a new value. */
        push_pack_alignment(name, curr_max_member_alignment);
        updated = TRUE;
      } else {
        if (pack_alignment_stack != NULL) {
          if (name != NULL && pasep == NULL && severity == es_error) {
            /* An error was issued.  Leave the stack intact.  Note -- this
               is not what Microsoft does after the warning; they pop one
               entry off the stack, as though "#pragma pack(pop, xxx)" were
               identical to "#pragma pack(pop)" when "xxx" is not found.
               This is not done by default because it creates error
               recovery problems. */
          } else {
            /* Pull out the saved pack alignment value before popping the
               entry off the stack. */
            curr_max_member_alignment = pack_alignment_stack->alignment;
            pop_pack_alignment();
            updated = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
    } else if (is_show && microsoft_mode) {
      /* Microsoft compilers accept optional ", <identifier>" and/or
         ", <integer-constant>" after the "show", but they have no effect. */
      if (curr_token != tok_rparen && curr_token != tok_end_of_source) {
        pos_warning(ec_pragma_pack_show_args_ignored, &error_position);
        (void)required_token(tok_comma, ec_exp_comma);
        if (curr_token == tok_identifier) {
          (void)get_token();
          if (curr_token != tok_rparen && curr_token != tok_end_of_source) {
            (void)required_token(tok_comma, ec_exp_comma);
            /* Next should be an integer constant. */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (curr_token == tok_int_constant) {
    /* Get the integer constant variable and check it against allowable
       values for a pack alignment. */
    val = value_of_integer_constant(&const_for_curr_token, &err);
    if (is_show && microsoft_mode) {
      /* Ignore the constant. */
    } else if (gnu_mode && next_token() != tok_rparen) {
      /* In GNU mode, a malformed #pragma pack is ignored with a warning.
         We do the same (the warning will be issued below). */
      pragma_ignored = TRUE;
    } else if ((gnu_mode || sun_mode) && !err && val == 0) {
      /* In GNU and Sun modes, "#pragma pack(0)" is equivalent to
         "#pragma pack()". */
      curr_max_member_alignment = 0;
      updated = TRUE;
    } else if (err ||
               !check_pack_alignment_value(val, &curr_max_member_alignment)) {
      diagnostic(microsoft_mode ? es_warning : es_error,
                 ec_bad_pack_alignment);
      /* Reset the current pack alignment value to zero, which means: use the
         default pack alignment that was specified on the command line. */
      if (!microsoft_mode) curr_max_member_alignment = 0;
    } else {
      updated = TRUE;
    }  /* if */
    /* Advance to the right parenthesis. */
    (void)get_token();
  } else if (curr_token == tok_rparen) {
    if (is_push || is_pop || is_show) {
      /* push/pop version, optionally with a name, but with no constant value
         (i.e., pack(push), pack(push, xxx), pack(pop), or pack(pop, xxx) --
         already dealt with. */
    } else {
      /* Empty argument list: "#pragma pack()", which means revert to the
         command-line default. */
      curr_max_member_alignment = 0;
      updated = TRUE;
    }  /* if */
  } else if (!is_show) {
    /* Expected an integer constant. */
    syntax_error(ec_exp_int_constant);
  }  /* if */
  remove_stop_token(tok_rparen);
  remove_stop_token(tok_identifier);
  remove_stop_token(tok_int_constant);
  /* Check for the closing parenthesis. */
  if ((microsoft_mode || gnu_mode) && curr_token != tok_rparen) {
    /* Microsoft and GNU issue a warning.  (In some cases, GCC ignores the
       pragma.) */
    if (pragma_ignored) {
      pos_warning(ec_exp_rparen_and_pragma_ignored, &pos_curr_token);
    } else {
      pos_warning(ec_exp_rparen, &pos_curr_token);
    }  /* if */
  } else {
    (void)required_token(tok_rparen, ec_exp_rparen);
  }  /* if */
  /* Restore the stop token array, pop the pragma scope, etc. */
  wrapup_rescan_of_pragma_tokens(pragma_ignored);
  if (updated) {
    /* Issue a diagnostic on a #pragma pack that appears within an
       instantiation or inline-defined member function definition. */
    a_scope_stack_entry_ptr  ssep;
    a_symbol_ptr             sym = NULL;

    if (depth_innermost_function_scope != NO_SCOPE_DEPTH) {
      ssep = &scope_stack[depth_innermost_function_scope];
      if (ssep->pragma_pack_is_local) {
        sym = (a_symbol_ptr)(ssep->il_scope->
                              variant.routine.ptr->source_corresp.assoc_info);
      }  /* if */
    } else if (is_nonspecialized_instantiation_context()) {
      ssep = &scope_stack[depth_innermost_instantiation_scope] + 1;
      if (ssep->kind == (a_scope_kind)sck_class_struct_union &&
          !ssep->in_prototype_instantiation) {
        sym = (a_symbol_ptr)(ssep->il_scope->variant.assoc_type
                                           ->source_corresp.assoc_info);
      }  /* if */
    }  /* if */
    if (sym != NULL) {
      pos_sy_remark(ec_local_pragma_pack, &ppp->pragma_position, sym);
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 3) {
    fprintf(f_debug, "curr_max_member_alignment = %d, stack = ",
            (int)curr_max_member_alignment);
    if (pack_alignment_stack == NULL) {
      fputs("NULL\n", f_debug);
    } else {
      a_const_char *name = pack_alignment_stack->name;
      fprintf(f_debug, "\"%s\" : %d\n", name == NULL ? "" : name,
              pack_alignment_stack->alignment);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
#if BACK_END_IS_CP_GEN_BE || BACK_END_IS_GCC_GEN_BE
  if (ppp->il_pragma_entry != NULL) {
    /* The C++-generating back end needs to track the current alignment
       in order to generate and revert #pragma pack directives.  (There
       will be no il_pragma_entry when the directive is encountered during
       the prototype instantiation of a template.) */
    ppp->il_pragma_entry->variant.alignment = curr_max_member_alignment;
  }  /* if */
#endif /* BACK_END_IS_CP_GEN_BE || BACK_END_IS_GCC_GEN_BE */
  db_exit();
}  /* pack_pragma */


a_targ_alignment current_max_alignment_for_class_members(void)
/*
Return the current pack alignment -- using curr_max_member_alignment if it
is non-zero and default_max_member_alignment otherwise.
*/
{
  return (curr_max_member_alignment > 0) ? curr_max_member_alignment :
                                           default_max_member_alignment;
}  /* current_max_alignment_for_class_members */


a_targ_alignment current_pack_pragma_value(void)
/*
Return curr_max_member_alignment.
*/
{
  return curr_max_member_alignment;
}  /* current_pack_pragma_value */


static void adjust_alignment_for_packing(a_targ_alignment *alignment,
                                         a_type_ptr       class_type)
/*
The "pack alignment" (if any) that is specified for class_type is the
maximum alignment for any of its nonstatic data members. Adjust *alignment
if necessary.
*/
{
  a_targ_alignment  pack_alignment;

#if IA64_ABI
  if (gnu_mode && gnu_abi_version < 30300 &&
      targ_bit_field_container_size < 0 && is_union_type(class_type)) {
    /* Some versions of GNU C that follow a Microsoft-like bit field allocation
       strategy (negative targ_bit_field_container_size) do not honor the
       pack alignment for unions. */
  } else
#endif /* IA64_ABI */
  /* Do not insert code here. */
  {
    pack_alignment =
                   class_type->variant.class_struct_union.max_member_alignment;
    if (pack_alignment > 0 && pack_alignment < *alignment) {
      *alignment = pack_alignment;
    }  /* if */
  }  /* if */
}  /* adjust_alignment_for_packing */


static a_boolean apply_explicit_field_alignment_directive(
                                                 a_field_ptr       field,
                                                 a_targ_alignment  *alignment)
/*
The given field has a "natural alignment" value of *alignment.  If the field
was declared with an explicit alignment directive, return TRUE.  If that
explicit alignment value is valid, update *alignment as needed; if it is
invalid, the field entry may be updated to reflect a valid value.  If no
explicit alignment value was specified, return FALSE.
*/
{
  a_boolean   result = FALSE;

  /* If the alignment of this field was explicitly specified, honor that. */
  if (field->alignment != 0) {
#if GNU_EXTENSIONS_ALLOWED
    a_type_ptr  class_type = parent_class_of(field);
    class_type = skip_typerefs(class_type);
    if (gnu_mode && !ms_extensions && field->alignment < *alignment &&
        !(field->is_packed ||
          class_type->variant.class_struct_union.is_packed)) {
      /* GNU C compilers ignore alignment directives that reduce the
         alignment, unless the packed attribute was also specified. */
      pos_warning(ec_alignment_reduction_ignored,
                  &field->source_corresp.decl_position);
      field->alignment = *alignment;
    } else
#endif /* GNU_EXTENSIONS_ALLOWED */
    {
      *alignment = field->alignment;
    }  /* if */
    result = TRUE;
#if GNU_EXTENSIONS_ALLOWED
  } else if (field->is_packed) {
    /* By default, the "packed" attribute implies an alignment of "1". */
    *alignment = 1;
    result = TRUE;
#endif /* GNU_EXTENSIONS_ALLOWED */
  }  /* if */
  return result;
}  /* apply_explicit_field_alignment_directive */


a_targ_alignment alignment_of_field_full(a_field_ptr  field,
                                         a_boolean    for_alignof)
/*
Return the alignment of the given field, taking into account any Microsoft or
GNU attributes specified on that field.  If for_alignof is TRUE, the alignment
of the field to return is for an __alignof operator (possible in GNU modes
only).
*/
{
  a_type_ptr        class_type = parent_class_of(field);
  a_targ_alignment  field_alignment;
#if GNU_EXTENSIONS_ALLOWED
  a_boolean         ignore_packing = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */

  if (for_alignof && gnu_version_is(< 30400)) {
    /* In recent GNU C and C++ compilers, __alignof__ applied to a field
       selection operation (. or ->) results in the "field alignment" rather
       than the intrinsic alignment.  For example (assuming recent GNU rules
       on the IA-32 architecture where long long is intrinsically aligned to
       8-byte boundaries, but aligned to 4-byte boundaries when laying out
       fields):
         struct S { long long x; } s;
         int a1 = __alignof__(s.x);      // a1 == 4
         int a2 = __alignof__((&s)->x);  // a2 == 4
         int a3 = __alignof__(*&s.x);    // a3 == 8
       Earlier versions of GCC, however, ignored the dual alignment rules. */
    field_alignment = alignment_of_type(field->type);
  } else {
    field_alignment = field_alignment_for(field->type);
  }  /* if */
  class_type = skip_typerefs(class_type);
#if GNU_EXTENSIONS_ALLOWED
  if ((gpp_version_is(>= 30400) || clangcpp_version_is(any_version)) && 
      !field->is_packed &&
      class_type->variant.class_struct_union.max_member_alignment > 0 &&
      class_type->variant.class_struct_union.max_member_alignment
                                                          < field_alignment &&
      find_attribute(ak_packed, class_type->source_corresp.attributes)
                                                                    != NULL) {
    /* Check whether the given field has a non-packed, non-POD type.  In that
       case, class-level packing coming from an attribute should be ignored
       (but packing coming from a pragma remains in effect). */
    a_type_ptr  ftp = field->type;
    if (is_array_type(ftp)) ftp = underlying_array_element_type(ftp);
    ftp = skip_typerefs(ftp);
    if (is_immediate_class_type(ftp) &&
        !ftp->variant.class_struct_union.is_packed &&
        symbol_for(ftp) != NULL &&
        (clang_mode ?
          !symbol_supplement_for_class(ftp)->is_cpp03_POD :
          (!class_symbol_supp(symbol_for(ftp))->standard_layout ||
           !class_symbol_supp(symbol_for(ftp))->is_class_aggregate))) {
      a_targ_alignment  pragma_alignment = 0;
      an_attribute_ptr  psap = find_attribute(
                                       ak_pragma_pack_state,
                                       class_type->source_corresp.attributes);
      ignore_packing = TRUE;
      if (psap != NULL) {
        /* The presence of a ak_pragma_pack_state internal attribute indicates
           that a "#pragma pack..." directive (or an equivalent command-line
           option) was in effect at the time of the class definition.  Since
           we are ignoring the "packed" attribute, we must fall back on that
           directive. */
        a_boolean  ovflo;
        pragma_alignment = (a_targ_alignment)
                             unsigned_value_of_integer_constant(
                                   psap->arguments->variant.constant, &ovflo);
        check_assertion(pragma_alignment != 0 && !ovflo);
        if (pragma_alignment < field_alignment) {
          field_alignment = pragma_alignment;
        }  /* if */
      }  /* if */
      if (!for_alignof) {
        pos_ty_warning(ec_no_packing_of_non_POD_field,
                       &field->source_corresp.decl_position, field->type);
      }  /* if */
    }  /* if */
  }  /* if */
  if (field->is_packed && for_alignof && gnu_version < 30400) {
    /* In early versions of GCC, __alignof applied to a field selection
       operation for a field declared with the "packed" attribute produced
       "1" even if the field has a stronger alignment (because of an
       alignment attribute). */
    field_alignment = 1;
    goto done;
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (apply_explicit_field_alignment_directive(field, &field_alignment)) {
    /* An explicit field alignment was specified.  field_alignment will have
       been updated as needed.  Nothing more to be done. */
#if IA64_ABI
  } else if (emulate_gnu_abi_bugs && field->is_bit_field &&
             field->bit_size == 0 && !type_is(class_type, tk_union)) {
    /* Note that GNU compilers do not consider the alternative field
       alignment for zero-width bit fields in structs and classes (but
       they do in unions).  We therefore do not use "field_alignment_for"
       here, and instead we access the type's intrinsic alignment
       directly. */
    field_alignment = alignment_of_type(field->type);
#endif /* IA64_ABI */
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (microsoft_mode && type_contains_explicit_alignment(field->type)) {
    /* Microsoft does not apply packing/alignment directives to fields of
       types with explicit alignment requirements. */
    class_type_supp(class_type)->has_explicitly_aligned_subobject = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  } else if (ignore_packing) {
    /* In some cases, packing directives do not apply. */
#endif /* GNU_EXTENSIONS_ALLOWED */
  } else {
    /* Adjust the field's alignment for packing, if required. */
    adjust_alignment_for_packing(&field_alignment, class_type);
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
done:
#endif /* GNU_EXTENSIONS_ALLOWED */
  return field_alignment;
}  /* alignment_of_field_full */

#define alignment_of_field(field)                                            \
  alignment_of_field_full((field), /*for_alignof=*/FALSE)


static a_boolean increment_field_offsets(
                                     a_targ_size_t               *byte_offset,
                                     an_unnormalized_bit_offset  *bit_offset,
                                     a_targ_size_t               byte_incr,
                                     an_unnormalized_bit_offset  bit_incr)
/*
Increment the byte and bit offsets by the indicated amount, checking for
overflow.  Return TRUE if the update is successful, FALSE if there was an
overflow error.
*/
{
  a_targ_size_t extra_byte_offset;
  a_boolean     overflow = FALSE;

  db_enter(4, "increment_field_offsets");
  if (byte_incr > targ_max_class_object_size ||
      *byte_offset > (targ_max_class_object_size - byte_incr)) {
    overflow = TRUE;
  } else {
    *byte_offset += byte_incr;
  }  /* if */
  if (bit_incr != 0) {
    if (*bit_offset > MAX_HOST_LARGE_UNSIGNED-bit_incr) {
      overflow = TRUE;
    } else {
      *bit_offset += bit_incr;
    }  /* if */
    /* If the bit offset has gone into the next byte, transfer some of the
       bit offset over to the byte offset. */
    if (*bit_offset >= targ_char_bit) {
      extra_byte_offset = *bit_offset / targ_char_bit;
      if (*byte_offset > (targ_max_class_object_size - extra_byte_offset)) {
        overflow = TRUE;
      } else {
        *byte_offset += extra_byte_offset;
      }  /* if */
      *bit_offset = *bit_offset % targ_char_bit;
    }  /* if */
  }  /* if */
  db_exit();
  return !overflow;
}  /* increment_field_offsets */


static a_boolean do_alignment(a_targ_size_t               *byte_offset,
                              an_unnormalized_bit_offset  *bit_offset,
                              a_targ_alignment            alignment)
/*
Increment the byte and bit offsets to align them with the indicated 
byte-multiple boundary.  Return TRUE if the update is successful, FALSE if
there was an overflow error.
*/
{
  a_targ_size_t byte_mod;
  a_boolean	overflow = FALSE;

  if (*bit_offset != 0) {
    /* If the bit offset indicates a partial storage byte, round the offsets
       to the next byte. */
    overflow = !increment_field_offsets(byte_offset, bit_offset,
				       (a_targ_size_t)0,
                                       targ_char_bit - *bit_offset);
  }  /* if */
  if (!overflow) {
    check_assertion(alignment != 0);
    byte_mod = *byte_offset % alignment;
    if (byte_mod != 0) {
      /* Increment the byte offset to make it a multiple of the required
         alignment. */
      overflow = !increment_field_offsets(byte_offset, bit_offset,
                                         (a_targ_size_t)(alignment - byte_mod),
					 (an_unnormalized_bit_offset)0);
    }  /* if */
  }  /* if */
  return !overflow;
}  /* do_alignment */


/*
Return the error code to be used when a class (or in C a struct/union)
is too large.
*/
#define struct_too_large_error()					\
  (C_mode() ? ec_struct_too_large : ec_class_too_large)


static void pad_bit_field(a_layout_block_ptr lob)
/*
If the last data field was a bit field that did not completely fill its
containing byte, pad out the remaining bits in that byte.
*/
{
  if (lob->bit_offset > 0) {
    /* If the last data field was a bit field, bump the byte count by one. */
    if (!increment_field_offsets(&lob->byte_offset, &lob->bit_offset,
                                 (a_targ_size_t)1,
                                 (an_unnormalized_bit_offset)0)) {
      if (!lob->any_overflow) {
        pos_error(struct_too_large_error(), &error_position);
        lob->any_overflow = TRUE;
      }  /* if */
    }  /* if */
    lob->bit_offset = 0;
  }  /* if */
}  /* pad_bit_field */


static void pad_ms_bit_field_container(a_layout_block_ptr  lob)
/*
Pad the remaining bits in the current bit field container, as represented
by the state of the layout block pointed to by lob -- that is, reset the
offset values as though the extra bits actually were being used.
*/
{
  check_assertion(lob->curr_container_type != NULL);
  (void)increment_field_offsets(&lob->byte_offset, &lob->bit_offset,
                                (a_targ_size_t)0, 
                                lob->curr_container_avail_bits);
  /* Padding to the end of the container means there's no room left
     for additional bit fields. */
  lob->curr_container_type = NULL;
  lob->curr_container_avail_bits = 0;
}  /* pad_ms_bit_field_container */

#if IA64_ABI

static a_type_ptr longest_integer_type_fitting_in_bit_field(a_field_ptr  field)
/*
Return the longest signed integer type that is not longer than the bit field.
*/
{
  unsigned long    bit_size = field->declared_bit_size;
  an_integer_kind  int_kind = (an_integer_kind)ik_none;

  check_assertion(targ_char_bit <= bit_size);
  int_kind = (an_integer_kind)ik_char;
  if (targ_char_bit * targ_sizeof_short <= bit_size) {
    int_kind = (an_integer_kind)ik_short;
  }  /* if */
  if (targ_char_bit * targ_sizeof_int <= bit_size) {
    int_kind = (an_integer_kind)ik_int;
  }  /* if */
  if (targ_char_bit * targ_sizeof_long <= bit_size) {
    int_kind = (an_integer_kind)ik_long;
  }  /* if */
#if LONG_LONG_ALLOWED
  if (targ_char_bit * targ_sizeof_long_long <= bit_size) {
    int_kind = (an_integer_kind)ik_long_long;
  }  /* if */
#endif /* LONG_LONG_ALLOWED */
#if INT128_EXTENSIONS_ALLOWED
  if (int128_extensions_enabled &&
      targ_sizeof_int128 > targ_sizeof_long_long &&
      field->type->size >= targ_sizeof_int128 &&
      targ_char_bit * targ_sizeof_int128 <= bit_size) {
    /* Strictly speaking, the ABI requires us to also consider int128 if it
       fits in the declared bit size.  However, that would create a binary
       incompatibility when a compiler transitions to enabling int128 for a
       field like "long long x:130;".  So we only consider int128 if the field
       type is at least 128 bits wide (in practice, that means the signed or
       unsigned int128 type). */
    int_kind = (an_integer_kind)ik_int128;
  }  /* if */
#endif /* INT128_EXTENSIONS_ALLOWED */
  return integer_type(int_kind);
}  /* longest_integer_type_fitting_in_bit_field */

#endif /* IA64_ABI */

static void update_class_alignment_for_bit_field(a_field_ptr         field,
                                                 a_targ_alignment    alignment,
                                                 a_layout_block_ptr  lob)
/*
field is a bit field allocated in a container with the given alignment.  Adjust
the alignment of the class being laid out (as recorded in lob) as needed.
*/
{
  a_boolean  do_update = TRUE, union_case = is_union_type(lob->class_type);

#if IA64_ABI
  if (emulate_gnu_abi_bugs && union_case) {
    /* In the GNU implementation of the IA-64 ABI, bit fields seem to affect
       the alignment of unions, but usually not that of classes and structs. */
#if GNU_EXTENSIONS_ALLOWED
  } else if (field->bit_size != 0 && field->alignment != 0) {
    /* Nonzero-length bit fields with an explicitly specified alignment
       always affect the alignment of the enclosing type in GNU compilers
       (even if the bit field is unnamed). */
#endif /* GNU_EXTENSIONS_ALLOWED */
  } else
#endif /* IA64_ABI */
  /* Do not insert code here. */
  if ((field->bit_size == 0 &&
       !targ_zero_width_bit_field_affects_struct_alignment) ||
      (union_case && !targ_bit_field_affects_union_alignment) ||
      (!targ_unnamed_bit_field_affects_struct_alignment &&
       field->source_corresp.assoc_info ==
                                       (char *)get_unnamed_field_symbol())) {
    /* Various cases where the bit field does not affect the alignment of the
       parent type:
         - zero-length bit fields when
           targ_zero_width_bit_field_affects_struct_alignment is FALSE;
         - bit fields in unions when targ_bit_field_affects_union_alignment
           is FALSE;
         - unnamed bit fields when
           targ_unnamed_bit_field_affects_struct_alignment is FALSE. */
    do_update = FALSE;
  }  /* if */
  if (do_update) {
    /* Remember the most stringent alignment requirement as the alignment
       requirement for the overall struct. */
#if IA64_ABI
      /* In the GNU implementation of the IA-64 ABI, zero-length bit fields
         seem  to affect the alignment of unions.  The resulting alignment is
         at least the alignment of an int. */
      if (emulate_gnu_abi_bugs && union_case && field->bit_size == 0 &&
          alignment < targ_alignof_int) {
        alignment = targ_alignof_int;
      }  /* if */
#endif /* IA64_ABI */
    /* The alignment was not adjusted earlier on because the environment does
       not apply packing directives to the relative layout of bit fields that
       straddle their base type's alignment boundary.  The class as a whole
       still obeys the packing directive however. */
    if (!targ_user_control_of_struct_packing_affects_bit_fields) {
      adjust_alignment_for_packing(&alignment, lob->class_type);
    }  /* if */
    if (alignment > lob->alignment) {
      lob->alignment = alignment;
    }  /* if */
  }  /* if */
}  /* update_class_alignment_for_bit_field */


static a_boolean align_offsets_for_bit_field(a_field_ptr         field,
                                             a_layout_block_ptr  lob)
/*
As part of maintaining field offsets while processing fields of a struct
definition, update the byte_offset and bit_offset fields of the layout block
pointed to by lob to indicate the position (after alignment if necessary) of
the bit field designated by *field.  If bit_size == 0, this forces some kind
of bit-field alignment.  See 3.5.2.1.  Moreover, if the alignment for the
container is greater than any field alignment seen so far, the layout
block's alignment field is updated.  If any overflow was detected in
computing the alignment, FALSE is returned; if there's no overflow TRUE is
returned.
*/
{
  a_targ_size_t     container_size;
  a_targ_alignment  container_alignment = 1;
  a_boolean         overflow = FALSE;
  unsigned int      bit_size = field->bit_size;
  a_type_ptr        base_type = skip_typerefs(field->type);

  db_enter(4, "align_offsets_for_bit_field");

/*
Useful macro that determines whether a field of size bit_size at the
current offset will fit into a container of the indicated size (in bytes)
that is aligned according to the indicated alignment.  (For use only when
targ_microsoft_bit_field_allocation is FALSE.)

This also checks that the current offset is inside the actual size of the
container when that size is smaller than the alignment.
*/
#define fits_in_container(size, alignment)                                   \
 (bit_size <= targ_char_bit * ((size) - (lob->byte_offset % (alignment)))    \
              - lob->bit_offset &&                                           \
  (lob->byte_offset % (alignment)) < (size))

  if (bit_size == 0) {
    /* A zero-width bit field is declared for alignment only.  The container
       size is not significant. */
    container_size = 1;
    /* Do the necessary alignment for a zero width bit field. */
    /* targ_zero_width_bit_field_alignment is
         >  0 to indicate a particular alignment
         == 0 to indicate minimal alignment
         <  0 to indicate "use the alignment of the base type from the
              declaration as the container alignment".
    */
    if (targ_zero_width_bit_field_alignment > 0) {
      container_alignment =
                       (a_targ_alignment)targ_zero_width_bit_field_alignment;
    } else if (targ_zero_width_bit_field_alignment == 0) {
      container_alignment = (a_targ_alignment)1;
    } else {
      /* targ_zero_width_bit_field_alignment < 0 */
      /* Use the normal field alignment. */
      container_alignment = alignment_of_field(field);
    }  /* if */
    if (targ_microsoft_bit_field_allocation) {
      /* Special handling of zero-width bit fields in Microsoft mode,
         depending on whether the previous field was a bit field or not. */
      if (lob->curr_container_type != NULL) {
        /* The previous member was a bit-field, so pad out the container
           before doing the alignment adjustment that was specified. */
        pad_ms_bit_field_container(lob);
      } else {
        /* In Microsoft mode a zero-sized bit field is only effective after
           a bit field, so reset container_alignment to a value that will
           cause it have no effect. */
        container_alignment = (a_targ_alignment)1;
      }  /* if */
    }  /* if */
  } else {
    /* targ_bit_field_container_size is
         >  0 to indicate a particular size for the bit-field container.
         == 0 to indicate "use the smallest integral type into which the
              bit-field will fit".
         < 0  to indicate "use the base type from the declaration as
              the container type".
    */
#if IA64_ABI
   if (field->declared_bit_size != field->bit_size &&
       /* In some (error) cases, named zero length bit fields are given
          a bit size of one (error recovery). */
       field->declared_bit_size != 0) {
      /* Handle alignment for bit fields that are too long for their
         underlying types, for the IA-64 ABI.  Find the longest integral
         type that is not longer than the bit field.  The bit field is
         aligned the same as this integral type.  The signedness of the
         integral type doesn't matter, because it's used for its alignment
         only; the bit field does not get that type. */
      a_type_ptr  int_type = longest_integer_type_fitting_in_bit_field(field);
      container_size = int_type->size;
      container_alignment = field_alignment_for(int_type);
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_GCC_GEN_BE
      field->bit_field_alignment_type = int_type;
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_GCC_GEN_BE */
      /* Force alignment. */
      overflow = !do_alignment(&lob->byte_offset, &lob->bit_offset,
                               container_alignment);
    } else
#endif /* IA64_ABI */
    /* Do not insert code here. */
    if (targ_bit_field_container_size > 0) {
      /* Use a fixed size container.  targ_bit_field_container_size indicates
         the size in bytes. */
      container_size = (a_targ_size_t)targ_bit_field_container_size;
      if (container_size == 1) {
        container_alignment = 1;
      } else if (container_size == targ_sizeof_short) {
        container_alignment = targ_alignof_short;
      } else if (container_size == targ_sizeof_int) {
        container_alignment = targ_alignof_int;
      } else if (container_size == targ_sizeof_long) {
        container_alignment = targ_alignof_long;
#if LONG_LONG_ALLOWED
      } else if (container_size == targ_sizeof_long_long) {
       container_alignment = targ_alignof_long_long;
#endif /* LONG_LONG_ALLOWED */
#if CHECKING
      } else {
        internal_error(
             "align_offsets_for_bit_field: bad targ_bit_field_container_size");
#endif /* CHECKING */
      }  /* if */
      (void)apply_explicit_field_alignment_directive(field,
                                                     &container_alignment);
    } else if (targ_bit_field_container_size == 0) {
      /* Use the smallest integral type into which the field will fit as
         the container.  Try first to find such a type for the current
         position (where the field may start off a byte boundary, and
         may therefore require a larger container than it would if optimally
         aligned). */
      container_size = 0;  /* Meaning not set yet. */
      if (bit_size > 0) {
        unsigned int one = 1;  /* To appease Lint. */
        if (fits_in_container(1, one)) {
          /* Char. */
          container_size      = 1;
          container_alignment = 1;
        } else if (fits_in_container(targ_sizeof_short, targ_alignof_short)) {
          /* Short. */
          container_size      = targ_sizeof_short;
          container_alignment = targ_alignof_short;
        } else if (fits_in_container(targ_sizeof_int, targ_alignof_int)) {
          /* Int. */
          container_size      = targ_sizeof_int;
          container_alignment = targ_alignof_int;
        } else if (fits_in_container(targ_sizeof_long, targ_alignof_long)) {
          /* Long. */
          container_size      = targ_sizeof_long;
          container_alignment = targ_alignof_long;
        }  /* if */
      }  /* if */
      if (container_size == 0) {
        /* The field can't be made to fit at the current position, so alignment
           will have to be done.  A smaller container size might now apply,
           since the field will be optimally aligned. */
        container_size = ((a_targ_size_t)bit_size +
                                        (targ_char_bit-1)) / targ_char_bit;
        if (container_size <= 1) {
          /* Char. */
          container_size      = 1;
          container_alignment = 1;
        } else if (container_size <= targ_sizeof_short) {
          /* Short. */
          container_size      = targ_sizeof_short;
          container_alignment = targ_alignof_short;
        } else if (container_size <= targ_sizeof_int) {
          /* Int. */
          container_size      = targ_sizeof_int;
          container_alignment = targ_alignof_int;
        } else if (container_size <= targ_sizeof_long) {
          /* Long. */
          container_size      = targ_sizeof_long;
          container_alignment = targ_alignof_long;
#if LONG_LONG_ALLOWED
        } else if (container_size <= targ_sizeof_long_long) {
          /* Long long. */
          container_size      = targ_sizeof_long_long;
          container_alignment = targ_alignof_long_long;
#endif /* LONG_LONG_ALLOWED */
#if CHECKING
        } else {
          internal_error("align_offsets_for_bit_field: size is too big");
#endif /* CHECKING */
        }  /* if */
      }  /* if */
      (void)apply_explicit_field_alignment_directive(field,
                                                     &container_alignment);
    } else {
      /* targ_bit_field_container_size < 0 */
      /* Always use the base type size.  For the alignment use the base type
         before skip_typerefs in case a GNU typedef attribute must be picked
         up. */
      container_size      = base_type->size;
      container_alignment = alignment_of_field(field);
#if IA64_ABI
      if (emulate_gnu_abi_bugs && container_alignment > container_size) {
        container_size = container_alignment;
      }  /* if */
#endif /* IA64_ABI */
    }  /* if */
  }  /* if */

  /* Adjust the container alignment for packing, if required.  Some
     environments do not let packing directives influence the layout of bit
     fields that cross their base type's alignment boundary, but the class
     type containing the bit field is still aligned according to the
     directive.  (I.e., the bit field is aligned independently from the
     directive wrt. the origin of the containing object, but in absolute
     terms the field may end up being unaligned.)  For such environments, the
     adjustment is made later on. */
#if GNU_EXTENSIONS_ALLOWED && IA64_ABI
  if (gnu_mode && field->is_bit_field &&
      (field->bit_size == 0 || field->alignment != 0)) {
    /* The GNU IA-64 ABI does not apply packing directives to zero-length
       bit fields or bit fields with an explicit alignment directive. */
  } else
#endif /* GNU_EXTENSIONS_ALLOWED && IA64_ABI */
  /* Do not insert code here. */
  if (targ_user_control_of_struct_packing_affects_bit_fields) {
    adjust_alignment_for_packing(&container_alignment, lob->class_type);
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED && IA64_ABI
  /* In GNU compilers, when #pragma pack(n) is in effect (with a nonzero n) a
     bit field is not aligned, but the overall alignment of the enclosing class
     is updated if needed. */ 
  if (gnu_mode && !targ_microsoft_bit_field_allocation &&
      field->alignment == 0 &&
      curr_max_member_alignment > 0 && field->bit_size != 0 &&
      !(gnu_abi_version < 30300 && is_union_type(lob->class_type))) {
    a_targ_alignment declared_alignment = field_alignment_for(field->type);
    if (container_alignment <= declared_alignment) {
      /* Determine the effect of the container alignment on the alignment of
         the enclosing class. */
      if (curr_max_member_alignment < declared_alignment) {
        container_alignment = curr_max_member_alignment;
      }  else {
        container_alignment = declared_alignment;
      }  /* if */
      if (container_alignment > lob->alignment) {
        lob->alignment = container_alignment;
      }  /* if */
      /* Do not align the bit field itself. */
      goto done;
    }  /* if */
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED && IA64_ABI */
  /* We want to make sure that the bit field can be grabbed using one
     load of the size of the container aligned the way the container
     must be. */
  if (targ_microsoft_bit_field_allocation && bit_size != 0) {
    /* A Microsoft-style two-stage bit-field allocation strategy is being
       used.  This means it's as though the container were preallocated as a
       field in its own right.  If the container type changes or hasn't
       enough room for the current bit field, simulate its having been
       allocated by padding it out. */
    if ((lob->curr_container_type == NULL) ||
        !compatible_ms_bit_field_container_types(lob->curr_container_type,
                                                 base_type) ||
        lob->curr_container_avail_bits < bit_size) {
      /* Either this is a bit field that does not follow another bit field of
         the same type or else it won't fit in the current container.  In
         either case, create a new container for it. */
      /* First, pad out the rest of the current container. */
      if (lob->curr_container_type != NULL) pad_ms_bit_field_container(lob);
      /* Make sure the new container is properly aligned. */
      overflow = !do_alignment(&lob->byte_offset, &lob->bit_offset,
                               container_alignment);
      /* Establish the new container. */
      lob->curr_container_type = base_type;
      lob->curr_container_avail_bits = (container_size * targ_char_bit);
    }  /* if */
  } else if (bit_size == 0 ||
             field->alignment != 0 ||
             !fits_in_container(container_size, container_alignment)) {
    /* Force alignment. */
    overflow = !do_alignment(&lob->byte_offset, &lob->bit_offset,
                             container_alignment);
  }  /* if */
  update_class_alignment_for_bit_field(field, container_alignment, lob);
#if GNU_EXTENSIONS_ALLOWED && IA64_ABI
done:
#endif /* GNU_EXTENSIONS_ALLOWED && IA64_ABI */
  db_exit();
  return !overflow;
}  /* align_offsets_for_bit_field */
                              

static a_boolean empty_base_conflict(
                            a_type_ptr                  etype, 
                            a_type_ptr                  atype,
                            ARG_UNUSED a_base_class_ptr atype_bcp,
                            a_targ_size_t               offset,
                            ARG_UNUSED a_boolean        consider_virtual_bases,
                            a_boolean                   consider_fields)
/*
Determine whether a subobject of type etype (an empty class type) can be
allocated at offset bytes from the start of another (not necessarily empty)
class type atype.  Return TRUE if this is not the case (i.e., there is
a type conflict that would cause two empty subobjects of the same type
to end up at the same address); FALSE otherwise.  If atype corresponds
to a base of the complete class in which etype is being allocated,
atype_bcp gives that base type; otherwise, atype_bcp is NULL.  If
atype_bcp is non-NULL, offset is the offset from the start of the
complete object containing atype_bcp, rather than from atype_bcp
itself.  If consider_virtual_bases is TRUE, virtual bases of atype are
considered; otherwise, they are ignored.  Similarly, field subobjects are
ignored when consider_fields is FALSE (used to emulate some GNU IA-64 ABI
bugs).
*/
{
  a_boolean     result = FALSE;
  a_targ_size_t atype_offset;

  etype = skip_typerefs(etype);
  atype = skip_typerefs(atype);
  check_assertion(is_empty_class_type(etype));
#if IA64_ABI
  /* If atype contains no empty class subobject, the result will remain
     FALSE. */
  if (!symbol_supplement_for_class(atype)->has_empty_class_subobject) {
    goto done;
  }  /* if */
#else /* !IA64_ABI */
  /* The offset is always zero in the Cfront-like ABI. */
  check_assertion(offset == 0);
#endif /* !IA64_ABI */
  if (atype_bcp != NULL) {
    atype_offset = atype_bcp->offset;
  } else {
    atype_offset = 0;
  }  /* if */
  if (offset == atype_offset && same_entities(etype, atype)) {
    /* Is there a direct type conflict? */
    result = TRUE;
#if !IA64_ABI
  } else {
    /* Is there a type conflict with any of the bases of the empty base (they
       are by definition also empty)? */
    a_base_class_ptr bcp = base_classes_of(etype);
    for (; bcp != NULL; bcp = bcp->next) {
      if (same_entities(bcp->type, atype)) {
        result = TRUE;
        break;
      }  /* if */
    }  /* for */
#endif /* !IA64_ABI */
  }  /* if */
  /* Apply these tests recursively to any base and field of atype. */
  if (!result) {
    a_base_class_ptr bcp;
#if IA64_ABI
    a_base_class_ptr a_base_class::*next_ptr;
    if (consider_virtual_bases &&
        atype->variant.class_struct_union.any_virtual_base_classes) {
      bcp = base_classes_of(atype);
      next_ptr = &a_base_class::next;
    } else {
      bcp = direct_base_classes_of(atype);
      next_ptr = &a_base_class::next_direct;
    }  /* if */
#else /* !IA64_ABI */
    bcp = direct_base_classes_of(atype);
#endif /* IA64_ABI */
    for (; bcp != NULL;
#if IA64_ABI
         bcp = bcp->*next_ptr
#else /* !IA64_ABI */
         bcp = bcp->next_direct
#endif /* IA64_ABI */
                               ) {
      a_base_class_ptr  eff_bcp;
      a_boolean         consider_fields_in_bcp = consider_fields;
      /* Skip indirect bases -- unless they are virtual and virtual bases are
         under consideration. */
      if (!bcp->direct
#if IA64_ABI
          && !(bcp->is_virtual && consider_virtual_bases)
#endif /* IA64_ABI */
                                                         ) {
        continue;
      }  /* if */
#if IA64_ABI
      if (emulate_gnu_abi_bugs && bcp->is_virtual) {
        /* Early GNU implementations of the IA-64 ABI ignore fields of
           virtual base classes when looking for conflicts. */
        consider_fields_in_bcp = FALSE;
      }  /* if */
      if (atype_bcp != NULL) {
        eff_bcp = corresponding_base_class(bcp, atype_bcp->derived_class,
                                           atype_bcp);
      } else
#endif /* IA64_ABI */
      /* Do not add code here. */
      {
        eff_bcp = bcp;
      }  /* if */
      if (
#if IA64_ABI
          eff_bcp->offset_is_set && eff_bcp->offset <= offset &&
#else /* !IA64_ABI */
          eff_bcp->offset == atype_offset && 
#endif /* !IA64_ABI */
          empty_base_conflict(etype, eff_bcp->type, eff_bcp, offset,
                              /*consider_virtual_bases=*/FALSE,
                              consider_fields_in_bcp)) {
        result = TRUE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  if (!result && consider_fields) {
    a_field_ptr field = atype->variant.class_struct_union.field_list;
    /* If the offset was relative to a complete class containing atype, we can
       now normalize it to be relative to atype_bcp. */
    offset -= atype_offset;
    /* Check fields of atype. */
    for (; 
         !result &&
         (field != NULL 
#if IA64_ABI
                        && field->offset_is_set
#endif /* IA64_ABI */
                                               );
         field = field->next) {
      a_type_ptr    field_type;
      a_targ_size_t elt, num_array_elts = 1, field_offset;
      /* Skip compiler generated fields. */
      if (field->compiler_generated) continue;
      /* Skip Microsoft-mode property and event fields (except C++/CLI
         "trivial" properties and events, which do have associated storage
         also represented by the field). */
      if (microsoft_mode && field_is_nontrivial_property_or_event(field)) {
        continue;
      }  /* if */
      field_type = skip_typerefs(field->type);
#if IA64_ABI || ABI_COMPATIBILITY_VERSION >= 300
      if (is_array_type(field_type)) {
        /* If the field has an array type, we're interested in the element
           type of that array. */
#if IA64_ABI
        /* In the IA-64 ABI, in each element of the array. */
        if (!has_any_unknown_specified_bound(field_type)) {
          num_array_elts = num_array_elements(field_type);
        }  /* if */
#endif /* IA64_ABI */
        field_type =f_skip_typerefs(underlying_array_element_type(field_type));
      }  /* if */
#endif /* IA64_ABI || ABI_COMPATIBILITY_VERSION >= 300 */
      if (is_class_struct_union_type(field_type)) {
        for (elt = 0; elt < num_array_elts; ++elt) {
          field_offset = field->offset + elt * field_type->size;
#if IA64_ABI
          /* Check if we can discard the field (or an element thereof) from
             consideration based on positions alone. */
          if (field_offset > offset) break;
          if (offset >= field_offset + field_type->size) {
            /* There might be a conflict with a subsequent element.  Therefore,
               use "continue" rather than "break." */
            continue;
          }  /* if */
#else /* !IA64_ABI */
          if (field_offset != 0) break;
#endif /* !IA64_ABI */
          /* The lint comment indicates that field_offset is known to be zero
             in some configurations. */
          if (empty_base_conflict(etype, field_type, (a_base_class_ptr)NULL,
                                  offset - field_offset, /*lint !e845*/
                                  /*consider_virtual_bases=*/TRUE,
                                  consider_fields)) {
            result = TRUE;
            break;
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* for */
  }  /* if */
#if IA64_ABI
done:
#endif /* IA64_ABI */
  return result;
}  /* empty_base_conflict */

#if IA64_ABI

static a_targ_size_t offset_after_base(a_base_class_ptr  bcp)
/*
Return the offset after the last byte covered by the given base class
(excluding virtual bases, but including the optimized bytes of an empty
base class).
*/
{
  a_type_ptr     bctp = skip_typerefs(bcp->type);
  a_targ_size_t  next_byte;

  if (
#if ABI_COMPATIBILITY_VERSION >= 304
      !is_empty_class_type(bctp)
#else /* ABI_COMPATIBILITY_VERSION < 304 */
      bctp->variant.class_struct_union.any_virtual_base_classes
#endif /* ABI_COMPATIBILITY_VERSION >= 304 */
                                                               ) {
    /* In IA-64 ABI configurations, size_without_virtual_base_classes
       does not include tail padding.  Even when there are no virtual
       bases, size_without_virtual_base_classes may be different from
       size since the latter does include tail padding.  For empty
       class types size_without_virtual_base_classes is zero, but
       that is not the right value to compute the base extent since
       the difference between zero and the actual empty class size is
       not considered to be "tail padding" by the IA-64 ABI. */
    next_byte = bcp->offset + bctp->variant.class_struct_union.extra_info
                                  ->size_without_virtual_base_classes;
  } else {
    next_byte = bcp->offset + bctp->size;
  }  /* if */
  return next_byte;
}  /* offset_after_base */


static void update_curr_base_extent(a_layout_block_ptr  lob,
                                    a_base_class_ptr    bcp)
/*
If the given base class extends beyond any previous base class, record the
new extent.  This is used to accelerate the layout process.  Also check that
the given base class contains no flexible array.
*/
{
  a_targ_size_t  next_byte = offset_after_base(bcp);

  lob->curr_base_extent = max_val(lob->curr_base_extent, next_byte-1);
  lob->curr_extent = max_val(lob->curr_extent, next_byte-1);
#if IA64_ABI
  if (!bcp->type->variant.class_struct_union.any_virtual_base_classes) {
    /* If there are no virtual bases, the base class should fall within the
       derived class as one contiguous block of size bcp->type->size. */
    lob->min_final_class_size = max_val(bcp->offset + bcp->type->size,
                                        lob->min_final_class_size);
  }  /* if */
#endif /* IA64_ABI */
}  /* update_curr_base_extent */


static a_type_ptr type_for_gnu_conflicts(a_type_ptr  type)
/*
Early GNU implementations of the IA-64 ABI treat arrays in a special way
when determine conflicts.  If a subobject has type "X[N1]...[Nm]" then
the underlying type X is used (as usual) unless one of the dimensions
N1, ..., Nm equals one.  If the given type is of the form "X[N1]...[Nm]"
with no dimension equal to one and X is a complete type, return type X.
Otherwise, return the given type unmodified.
*/
{
  a_type_ptr  result = skip_typerefs(type);

  while (is_array_type(result)) {
    if (has_unknown_specified_bound(result) ||
        is_incomplete_type(result) ||
        result->variant.array.variant.number_of_elements == 1) {
      result = type;
      break;
    } else {
      result = skip_typerefs(result->variant.array.element_type);
    }  /* if */
  }  /* while */
  return result;
}  /* type_for_gnu_conflicts */


static a_boolean has_dimension_of_length_one(a_type_ptr  type)
/*
Return TRUE if and only if the given array type has a dimension of length one
(e.g., "int [3][1][7]").
*/
{
  check_assertion(is_array_type(type));
  return is_array_type(type_for_gnu_conflicts(type));
}  /* has_dimension_of_length_one */


static a_boolean is_base_of_virtual_base(a_base_class_ptr  bcp)
/*
Return TRUE if the given base class is part of a virtual base class on every
derivation path.
*/
{
  a_boolean                    result = TRUE;
  a_base_class_derivation_ptr  dp = bcp->derivation;

  for (; dp != NULL; dp = dp->next) {
    if (!dp->path->base_class->is_virtual) {
      result = FALSE;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* is_base_of_virtual_base */


static a_boolean subobject_conflict(a_layout_block  *lob,
                                    a_type_ptr      subobject_type,
                                    a_targ_size_t   offset,
                                    a_boolean       consider_bases,
                                    a_boolean       consider_virtual_bases,
                                    a_boolean       consider_fields)
/*
Return TRUE if placing a subobject (whose type is subobject_type) at the
indicated offset would result in a conflict with some other subobject of
lob->class_type.  If consider_bases is FALSE, no base classes of the subobject
type are examined.  If consider_virtual_bases is TRUE, virtual bases of
subobject_type are considered in addition to direct bases.  If consider_fields
is FALSE, field subobjects are ignored while searching for a conflict.
*/
{
  a_targ_size_t               size, elt, num_array_elts;
  a_targ_size_t               field_elt, num_field_array_elts;
  a_base_class_ptr            bcp;
  a_field_ptr                 field;
  a_type_ptr                  field_type, class_type = lob->class_type;
  a_boolean                   result = FALSE;
  a_boolean                   array_subobject = is_array_type(subobject_type);

  /* If the subobject is an array, get the (ultimate) element type. */
  if (array_subobject && emulate_gnu_abi_bugs &&
      has_dimension_of_length_one(subobject_type)) {
    /* Early GNU implementations of the IA-64 class layout algorithm ignore
       conflicts with array subobjects that have a dimension equal to 1. */
    goto done;
  }  /* if */
  if (array_subobject &&
      !has_any_unknown_specified_bound(subobject_type) &&
      !is_incomplete_type(subobject_type)) {
    num_array_elts = num_array_elements(subobject_type);
    subobject_type = underlying_array_element_type(subobject_type);
  } else {
    num_array_elts = 1;
  }  /* if */
  if (is_class_struct_union_type(subobject_type)) {
    /* The subobject is a class, struct, or union type, so there may be empty
       subobjects which conflict with other, already-allocated, empty
       subobjects. */
    subobject_type = skip_typerefs(subobject_type);
    /* If we're not considering virtual bases, it doesn't make sense for the
       subobject to be an array type. */
    check_assertion(consider_virtual_bases || num_array_elts == 1);
    size = subobject_type->size;
    /* Loop over all the elements of the array (treating the non-array case as
       a degenerate case of the array case) looking for conflicts. */
    /* For example:
           struct E1 {};           // Empty.
           struct E2 {};           // Ditto.
           struct CE: E1, E2 {};   // Ditto
           struct S: E2, CE {
             E1 v[20];
           };
       The direct base E2-in-S is at offset 0, and the direct base CE-in-S is
       at offset 1 since at offset zero it would conflict with the direct base
       E2-in-S.  When placing v, it is not sufficient to examine only the first
       element, because that would suggest that offset 0 is okay; instead, v[1]
       conflicts with the direct base CE-in-S (due to its indirect base of type
       E1). */
    for (elt = 0; !result && elt < num_array_elts; ++elt, offset += size) {
      if (offset > lob->curr_extent) {
        /* No conflict is possible from this point on. */
        break;
      }  /* if */
      /* Try the subobject type itself. */
      if (is_empty_class_type(subobject_type) && 
          empty_base_conflict(subobject_type, class_type, 
                              (a_base_class_ptr)NULL, offset,
                              /*consider_virtual_bases=*/TRUE,
                              /*consider_fields=*/TRUE)) {
        result = TRUE;
      }  /* if */
      /* Go through the base classes of the subobject type. */
      if (!result && consider_bases) {
        for (bcp = base_classes_of(subobject_type);
             bcp != NULL;
             bcp = bcp->next) {
          if ((bcp->direct || (bcp->is_virtual && consider_virtual_bases)) &&
              subobject_conflict(lob, bcp->type, 
                                 offset + bcp->offset,
                                 /*consider_bases=*/TRUE,
                                 /*consider_virtual_bases=*/FALSE,
                                 /*consider_fields=*/TRUE) &&
              !(emulate_gnu_abi_bugs && !bcp->direct &&
                is_base_of_virtual_base(bcp))) {
            result = TRUE;
            break;
          }  /* if */
        }  /* for */
      }  /* if */
      /* Go through the fields of the subobject type. */
      if (!result && consider_fields) {
        for (field = subobject_type->variant.class_struct_union.field_list;
             field != NULL;
             field = field->next) {
          /* Skip compiler generated fields. */
          if (field->compiler_generated) continue;
#if MICROSOFT_EXTENSIONS_ALLOWED
          if (microsoft_mode && field_is_nontrivial_property_or_event(field)) {
            /* Nontrivial properties and events do not participate in object
               layout. */
            continue;
          }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          /* If the field type is an array get the (ultimate) element type. */
          num_field_array_elts = 1;
          if (is_array_type(field->type)) {
            /* Watch out for prototype instantiations. */
            if (!has_any_unknown_specified_bound(field->type)) {
              if (skip_typerefs(field->type)->size == 0 ||
                  (emulate_gnu_abi_bugs &&
                   has_dimension_of_length_one(field->type))) {
                /* Do not consider conflicts with flexible array members.
                   Also, early GNU compilers do not consider conflicts with
                   arrays of length one. */
                continue;
              }  /* if */
              num_field_array_elts = num_array_elements(field->type);
            }  /* if */
            field_type = underlying_array_element_type(field->type);
          } else {
            field_type = field->type;
          }  /* if */
          field_type = skip_typerefs(field_type);
          if (is_immediate_class_type(field_type) &&
              field_type->source_corresp.assoc_info != NULL &&
              symbol_supplement_for_class(field_type)
                                                 ->has_empty_class_subobject) {
            /* Loop through the elements of the array. */
            for (field_elt = 0; 
                 field_elt < num_field_array_elts; 
                 ++field_elt) {
              if (subobject_conflict(lob, field_type,
                                     offset + field->offset + field_elt *
                                                              field_type->size,
                                     /*consider_bases=*/TRUE,
                                     /*consider_virtual_bases=*/TRUE,
                                     /*consider_fields=*/TRUE)) {
                result = TRUE;
                break;
              }  /* if */
            }  /* for */
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* for */
  }  /* if */
done:
  return result;
}  /* subobject_conflict */


static a_boolean is_primary_virtual_base(a_base_class_ptr  bcp)
/*
Return TRUE if and only if bcp has a derivation path on which it is a primary
virtual base of a class directly derived from it.
*/
{
  a_boolean                    result = FALSE;
  a_base_class_derivation_ptr  dp = bcp->derivation;

  check_assertion(bcp->is_virtual);
  do {
    if (dp->path != dp->path_tail) {
      if (dp->path_tail->prev->base_class->primary_base_class == bcp) {
        result = TRUE;
      }  /* if */
    }  /* if */
    dp = dp->next;
  } while (!result && dp != NULL);
  return result;
}  /* is_primary_virtual_base */


static a_boolean base_subobject_conflict(a_layout_block   *lob,
                                         a_base_class_ptr bcp,
                                         a_targ_size_t    offset)
/*
Return TRUE if placing bcp at offset would result in a subobject conflict.
*/
{
  a_boolean        result = FALSE;
  a_type_ptr       base_type;
  a_base_class_ptr base_bcp, eff_bcp;

  base_type = bcp->type;
  /* See if there is a conflict with base_type itself.  Some GNU compilers
     ignore the fields of virtual base subobjects. */
  if (subobject_conflict(lob, base_type, offset,
                         /*consider_bases=*/FALSE,
                         /*consider_virtual_bases=*/FALSE,
                         !(emulate_gnu_abi_bugs && bcp->is_virtual))) {
    result = TRUE;
  } else {
    /* There is no direct conflict.  There might, however, be
       a conflict with bases that are going to be allocated as part of
       this base.  (This code is structured to reduce the number of calls
       to corresp_base_class, because those can be expensive.) */
    if (emulate_gnu_abi_bugs && offset != 0 && bcp->is_virtual &&
        !is_empty_class_type(bcp->type) && !is_primary_virtual_base(bcp)) {
      /* Some GNU C++ compilers do not consider bases of nonprimary virtual
         bases. */
    } else if (bcp->primary_base_class != NULL &&
               base_subobject_conflict(lob, bcp->primary_base_class, offset)) {
      /* The primary base is always at offset zero. */
      result = TRUE;
    } else {
      for (base_bcp = direct_base_classes_of(base_type);
           base_bcp != NULL;
           base_bcp = base_bcp->next_direct) {
        if (!base_bcp->is_virtual) {
          /* A direct base is at a fixed offset. */
          eff_bcp = corresp_base_class(base_bcp, bcp);
          if (bcp->primary_base_class != eff_bcp &&
              base_subobject_conflict(lob, eff_bcp,
                                      offset + base_bcp->offset)) {
            result = TRUE;
            break;
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  return result;
}  /* base_subobject_conflict */


static a_boolean gnu_conflict_found(a_type_ptr        subobject_type,
                                    a_base_class_ptr  ebcp,
                                    a_boolean         in_field,
                                    a_boolean         consider_virtual_bases)
/*
This is a helper routine to identify certain spurious GNU empty base
conflicts (see e.g.  gnu_first_field_conflict, gnu_base_conflict, and
gnu_leading_empty_base_conflict below).  It may call itself recursively
if needed.  ebcp is the empty base class (of the complete type being
laid out) for which conflicts are considered.  subobject_type is the
type of an already allocated subobject in which a conflict is looked for.

See gnu_first_field_conflict for a description of this GNU C++ layout bug.
See also gnu_base_conflict for a similar problem with preceding empty
bases.  Yet another similar issue occurs for nearly empty virtual bases
that should normally be allocated at offset zero; in that case, in_field is
TRUE indicating that conflicts must involve a field subobject (not just a
base class of the complete object).

GNU compilers seem to consider virtual bases only during certain stages of
layout.  The flag consider_virtual_bases must be TRUE to enable consideration
of virtual bases (but the flag is always set to TRUE when considering field
types).
*/
{
  a_boolean  result = FALSE;
  a_field_ptr  field;
  a_type_ptr   eb_type;

  if (!is_immediate_class_type(subobject_type) ||
      (subobject_type->source_corresp.assoc_info != NULL &&
       !symbol_supplement_for_class(subobject_type)
                                               ->has_empty_class_subobject)) {
    goto done;
  }  /* if */
  eb_type = skip_typerefs(ebcp->type);
  field = subobject_type->variant.class_struct_union.field_list;
  for (; field != NULL; field = field->next) {
    a_type_ptr  field_type = skip_typerefs(field->type);
    /* Get the underlying element type except in some special cases. */
    field_type = type_for_gnu_conflicts(field_type);
    if (field->compiler_generated || is_array_type(field_type)) {
      continue;
    }  /* if */
    /* Spurious conflicts occur with fields that start in the first N bytes
       of their enclosing class, where N depends on the platform (but not
       necessarily within the first N bytes of the complete object they
       belong to).  Within those N bytes, the fields are treated as if
       they appeared at offset zero. */
#if defined(__sun) && defined(__sparc)
/* N is 8 on SPARC Solaris. */
#define offset_limit 8
#else /* !(defined(__sun) ... ) */
/* N is 16 on various other platforms. */
#define offset_limit 16
#endif /* defined(__sun) && defined(__sparc) */
    if (field->offset < offset_limit && is_immediate_class_type(field_type)) {
      if (identical_types(field_type, eb_type) ||
          gnu_conflict_found(field_type, ebcp,
                             /*in_field*/FALSE,
                             /*consider_virtual_bases=*/TRUE)) {
        result = TRUE;
        break;
      }  /* if */
    }  /* if */
#undef offset_limit
  }  /* for */
  if (!result) {
    a_base_class_ptr  bcp = base_classes_of(subobject_type);
    for (; bcp != NULL; bcp = bcp->next) {
      if (bcp->offset == 0 && bcp->offset_is_set &&
          (consider_virtual_bases ||
           !(bcp->is_virtual || is_base_of_virtual_base(bcp)))) {
        /* Unlike field subobjects, only base class subobjects at offset
           zero are considered for this kind of conflicts.  Virtual bases and
           bases of virtual bases aren't always considered. */
        if ((!in_field && identical_types(bcp->type, eb_type)) ||
            (bcp->direct && gnu_conflict_found(bcp->type, ebcp, in_field,
                                               consider_virtual_bases))) {
          result = TRUE;
          break;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
done:
  return result;
}  /* gnu_conflict_found */


static a_field_ptr get_gnu_first_field(a_type_ptr  class_type)
/*
Return the first field of class_type that the GNU layout algorithm finds
significant.  This excludes zero-length bit fields.  Return NULL is there
is no such field.
*/
{
  a_field_ptr  first_field = class_type->variant.class_struct_union.field_list;

  /* Leading zero-length bit fields are not considered. */
  while (first_field != NULL && first_field->is_bit_field &&
         first_field->bit_size == 0) {
    first_field = first_field->next;
  }  /* while */
  return first_field;
}  /* get_gnu_first_field */


static a_boolean gnu_first_field_conflict(a_type_ptr     class_type,
                                          a_field_ptr    field,
                                          a_targ_size_t  offset)
/*
This routine identifies a strange (but not entirely unusual) situation where
certain GNU C++ compilers incorrectly assume that the first field (given by
field) of a class (given by class_type) conflicts with an empty base class.

These situations are described as follows.  Assume the given class type has
an empty base of type E at the given offset, and let F be the type of the 
first field.  If F has a field e of type E such that the offset of e within
F is less than 16, the situation is encountered and this routine returns
TRUE.  If this is not the case, this rule is applied to every field of F
whose offset is less than 16, and to every base class subobject of F whose
offset is zero.
*/
{
  a_boolean  result = FALSE;

  if (get_gnu_first_field(class_type) == field) {
    a_base_class_ptr  bcp = base_classes_of(class_type);
    for (; bcp != NULL; bcp = bcp->next) {
      /* Examine every empty base class subobject that has no empty base class
         subobjects of its own (if it has a base class subobject of its own,
         any conflict would also occur with the latter base subobject). */
      if (bcp->offset == offset &&
          bcp->type->variant.class_struct_union.is_empty_class &&
          base_classes_of(bcp->type) == NULL &&
          gnu_conflict_found(type_for_gnu_conflicts(field->type), bcp,
                             /*in_field=*/FALSE,
                             /*consider_virtual_bases=*/FALSE)) {
        result = TRUE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return result;
}  /* gnu_first_field_conflict */


static a_boolean gnu_base_conflict(a_type_ptr        class_type,
                                   a_base_class_ptr  bcp,
                                   a_targ_size_t     offset)
/*
This routine identifies a situation similar to the GNU first field conflict
(see above), but this time the conflict is with a base class instead of a
field.  We are attempting to place base class bcp at the given offset in the
layout of the given class type.  If this is the offset of a previously
allocated nonvirtual empty base, GNU compilers will not optimize the bcp
base if it has a subobject of the same type as the previous base.
*/
{
  a_boolean  result = FALSE;

  /* A potential for this type of conflict only exists if a previous base has
     already been allocated (so bcp should not be the first direct base). */
  if (bcp->direct_base_number != 1) {
    a_base_class_ptr  ebcp = base_classes_of(class_type);
    for (; ebcp != NULL; ebcp = ebcp->next) {
      a_base_class_ptr  sub_ebcp;
      if (!ebcp->direct || ebcp->is_virtual || !ebcp->offset_is_set ||
          ebcp->offset != offset) {
        continue;
      }  /* if */
      sub_ebcp = base_classes_of(ebcp->type);
      /* Only examine conflicts with bottom-most base classes and ignore
         virtual bases if bcp is not virtual (i.e., if we are not yet in the
         stage of laying out virtual bases). */
      if (sub_ebcp == NULL) {
        if (gnu_conflict_found(skip_typerefs(bcp->type), ebcp,
                               /*in_field=*/FALSE, bcp->is_virtual)) {
          result = TRUE;
          goto done;
        }  /* if */
      } else {
        for (; sub_ebcp != NULL; sub_ebcp = sub_ebcp->next) {
          if (base_classes_of(sub_ebcp->type) == NULL &&
              gnu_conflict_found(skip_typerefs(bcp->type), sub_ebcp,
                                 /*in_field=*/FALSE, bcp->is_virtual)) {
            result = TRUE;
            goto done;
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* for */
  }  /* if */
done:
  return result;
}  /* gnu_base_conflict */


static a_boolean gnu_leading_empty_base_conflict(a_type_ptr        class_type,
                                                 a_base_class_ptr  ebcp)
/*
ebcp is an empty base class of class_type that we want to allocate at
offset zero.  If a direct base allocated at offset zero already contains a
subobject of type ebcp->type (or one of its base types) in its first few
bytes, a GNU compiler will mistakenly assume that the empty base cannot be
allocated at that offset.  This function returns TRUE in that case.
*/
{
  a_boolean         result = FALSE;
  a_base_class_ptr  bcp = base_classes_of(class_type);

  for (; bcp != NULL; bcp = bcp->next) {
    if (bcp->offset_is_set && bcp->direct && bcp->offset == 0) {
      a_base_class_ptr  sub_ebcp = base_classes_of(ebcp->type);
      /* Only examine conflicts with bottom-most base classes and ignore
         virtual bases if bcp is not virtual (i.e., if we are not yet in the
         stage of laying out virtual bases). */
      if (sub_ebcp == NULL &&
          gnu_conflict_found(skip_typerefs(bcp->type), ebcp,
                             /*in_field=*/TRUE, ebcp->is_virtual)) {
        result = TRUE;
        break;
      } else {
        for (; sub_ebcp != NULL; sub_ebcp = sub_ebcp->next) {
          if (base_classes_of(sub_ebcp->type) == NULL &&
              gnu_conflict_found(skip_typerefs(bcp->type), sub_ebcp,
                                 /*in_field=*/TRUE, ebcp->is_virtual)) {
            result = TRUE;
            break;
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
  }  /* for */
  return result;
}  /* gnu_leading_empty_base_conflict */


static a_targ_size_t virtual_base_offset_computed_for_direct_base_type(
                                                       a_base_class_ptr  ebcp)
/*
The given base class should be an empty virtual base.  If this base had
already appeared as a virtual base of a direct base, return the offset
within the last direct base in which it appeared (excluding cases where
it appeared at offset zero).  Otherwise return zero.  This is used to
emulate a strange GNU IA-64 layout bug.
*/
{
  a_targ_size_t     result = (a_targ_size_t)0;
  a_base_class_ptr  bcp = preorder_base_classes_of(ebcp->derived_class);

  check_assertion(ebcp->is_virtual);

  for (; bcp != NULL; bcp = bcp->next_preorder) {
    if (bcp == ebcp) {
      /* Do not consider bases appearing after the given base in a preorder
         traversal. */
      goto done;
    } else if (bcp->direct && bcp->offset_is_set) {
      a_base_class_ptr  sub_bcp = base_classes_of(bcp->type);
      for (; sub_bcp != NULL; sub_bcp = sub_bcp->next) {
        if (sub_bcp->is_virtual && same_entities(sub_bcp->type, ebcp->type)) {
          result = sub_bcp->offset;
          goto done;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* for */
done:
  return result;
}  /* virtual_base_offset_computed_for_direct_base_type */


static void reposition_gnu_disconnected_virtual_bases(a_layout_block_ptr  lob)
/*
A GNU IA-64 layout bug can cause virtual bases to end up outside the space
occupied by the complete object.  Since this can lead to strange memory
corruption bugs, we do not normally emulate this bug in those cases.
Instead, such virtual bases are repositioned to the end of the object and
a warning is issued.  This routine performs this adjustment (when needed).
*/
{
  a_base_class_ptr  bcp = base_classes_of(lob->class_type);

  check_assertion(emulate_gnu_abi_bugs);
  for (; bcp != NULL; bcp = bcp->next) {
    if (bcp->is_virtual && bcp->offset > lob->byte_offset) {
      /* This should never happen for virtual bases that are also direct
         bases (see: allocate_empty_base). */
      check_assertion(!bcp->direct);
      if (emulate_unsafe_gnu_abi_bugs) {
        /* Emulate the bug after all, but issue a warning. */
        pos_sy2_warning(
                    ec_gnu_virtual_base_gap, &bcp->decl_position,
                    (a_symbol_ptr)bcp->type->source_corresp.assoc_info,
                    (a_symbol_ptr)lob->class_type->source_corresp.assoc_info);
      } else {
        pos_sy2_warning(
                    ec_no_gnu_virtual_base_gap, &bcp->decl_position,
                    (a_symbol_ptr)bcp->type->source_corresp.assoc_info,
                    (a_symbol_ptr)lob->class_type->source_corresp.assoc_info);
        bcp->offset = lob->byte_offset;
      }  /* if */
    }  /* if */
  }  /* for */
}  /* reposition_gnu_disconnected_virtual_bases */


static a_field_ptr trailing_nonclass_field(a_type_ptr     class_type,
                                           a_targ_size_t  *offset)
/*
Return the last field of the given class type.  If the last field has a class
type, return its last field, etc.  If there are no fields, consider the fields
of any trailing base class.  *offset is incremented by the offset of the field
(if any).
*/
{
  a_field_ptr  result = class_type->variant.class_struct_union.field_list;

  if (result != NULL) {
    a_targ_size_t  field_offset;
    while (result->next != NULL) result = result->next;
    field_offset = *offset + result->offset;
    if (is_class_struct_union_type(result->type)) {
      result = trailing_nonclass_field(skip_typerefs(result->type),
                                       &field_offset);
    }  /* if */
    if (result != NULL) {
      *offset = field_offset;
    }  /* if */
  } else if (class_type->variant.class_struct_union.extra_info != NULL) {
    /* No fields: Check for base classes. */
    a_base_class_ptr  bcp = base_classes_of(class_type), last_bcp = bcp;
    if (bcp != NULL) {
      /* Find the trailing base. */
      a_targ_size_t  base_offset = bcp->offset;
      bcp = bcp->next;
      while (bcp != NULL) {
        if (bcp->offset >= base_offset) {
          last_bcp = bcp;
          base_offset = bcp->offset;
        }  /* if */
        bcp = bcp->next;
      }  /* while */
      if (!last_bcp->is_virtual) {
        /* Look for a trailing field in this base. */
        result = trailing_nonclass_field(skip_typerefs(last_bcp->type),
                                         &base_offset);
      }  /* if */
      if (result != NULL) {
        *offset += base_offset;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* trailing_nonclass_field */


static a_boolean gnu_may_use_bit_padding(a_layout_block_ptr  lob,
                                         a_field_ptr         fp)
/*
fp is the first field of the class whose layout state is described by lob.
fp is also a bit field.  Return whether a GNU compiler might allocate this
bit field in a trailing bit field container of one of its bases.
*/
{
  a_boolean   result = FALSE;
  a_type_ptr  class_type = lob->class_type;

  if (!C_mode() &&
      !(class_type->source_corresp.assoc_info != NULL &&
        symbol_supplement_for_class(class_type)->is_cpp03_POD)) {
    /* This is not a POD: The reuse of bit field containers does not apply
       to PODs. */
    a_base_class_ptr  bcp = base_classes_of(class_type);
    a_field_ptr       trailing_field = NULL;
    for (; bcp != NULL; bcp = bcp->next) {
      if (bcp->offset_is_set &&
          offset_after_base(bcp) == lob->curr_base_extent+1) {
        /* A trailing base class: See if it has a trailing bit field. */
        a_type_ptr  bctp = skip_typerefs(bcp->type);
        a_targ_size_t  offset = bcp->offset;
        a_field_ptr    candidate = trailing_nonclass_field(bctp, &offset);
        if (candidate != NULL &&
            (trailing_field == NULL ||
             candidate->offset > trailing_field->offset)) {
          trailing_field = candidate;
        }  /* if */
      }  /* if */
    }  /* for */
    if (trailing_field != NULL && trailing_field->is_bit_field &&
        (lob->curr_base_extent - trailing_field->offset + 1) * targ_char_bit
                     > (a_targ_size_t)(trailing_field->offset_bit_remainder +
                                       fp->bit_size)) {
      /* There is room to stuff fp in the bit padding of a preceding bit
         field. */
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* gnu_may_use_bit_padding */


static void warn_if_offset_in_tail_padding(a_field_ptr         field,
                                           a_base_class_ptr    base,
                                           a_layout_block_ptr  lob)
/*
Issue a warning if the given field or base was allocated in the tail padding 
of a base class.  Either field or base (but not both) must be NULL.
*/
{
  a_type_ptr        class_type = lob->class_type;
  a_base_class_ptr  bcp = base_classes_of(class_type);
  a_targ_size_t     offset;

  check_assertion((field != NULL && base == NULL) ||
                  (field == NULL && base != NULL));
  offset = (field != NULL) ? field->offset : base->offset; /*lint !e413*/
  for (; bcp != NULL; bcp = bcp->next) {
    if ((bcp->direct || bcp->is_virtual) && bcp->offset_is_set &&
        !is_empty_class_type(bcp->type)) {
      /* Note that we don't need to warn about empty base classes because
         they are handled correctly through the empty base class optimization
         code. */
      an_unnormalized_bit_offset
                      dummy = 0;
      a_class_type_supplement_ptr cts = class_type_supp(bcp->type);
      a_targ_alignment
                      alignment = cts->alignment_without_virtual_base_classes;
      a_targ_size_t   size = cts->size_without_virtual_base_classes;
      (void)do_alignment(&size, &dummy, alignment);
      if (offset < bcp->offset + size && offset > bcp->offset) {
        if (field != NULL) {
          pos_warning(ec_field_uses_tail_padding,
                      &field->source_corresp.decl_position);
        } else {
          /*lint -e{413}*/
          pos_sy2_warning(ec_base_uses_tail_padding, &base->decl_position,
                          symbol_for(base->type), symbol_for(bcp->type));
        }  /* if */
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  /* If this is a bit field, GNU compilers may allocate it in the container
     of an inherited bit field.  Warn about the layout difference if this is
     such a situation. */
  if (emulate_gnu_abi_bugs && field != NULL && field->is_bit_field &&
      gnu_may_use_bit_padding(lob, field)) {
    pos_warning(ec_gnu_may_use_bit_padding,
                &field->source_corresp.decl_position);
  }  /* if */
}  /* warn_if_offset_in_tail_padding */


static void gnu_trim_trailing_base_bits(a_targ_size_t       *end_of_object,
                                        a_layout_block_ptr  lob)
/*
Version 3.3 of the GNU C++ compiler will allocate an empty virtual base
in the bit field padding of a trailing bit field of another virtual base
(if the latter virtual base class is a non-POD class that immediately
precedes the empty virtual base in the overall object layout).  To emulate
this behavior, this routine adjusts *end_of_object if the currently trailing
base class ends with a bit field.
*/
{
  a_base_class_ptr  bcp = lob->trailing_nonempty_base;

  if (bcp != NULL && bcp->is_virtual && bcp->offset_is_set &&
      bcp->offset + bcp->type->size >= *end_of_object &&
      !(bcp->type->source_corresp.assoc_info != NULL &&
        symbol_supplement_for_class(bcp->type)->is_cpp03_POD)) {
    a_targ_size_t  offset = bcp->offset;
    a_type_ptr     btp = bcp->type;
    /* The call to trailing_nonclass_field sets offset to the offset of the
       last field. */
    a_field_ptr    last_field = trailing_nonclass_field(btp, &offset);
    if (last_field != NULL && last_field->is_bit_field &&
        (unsigned)(last_field->offset_bit_remainder + last_field->bit_size) %
                                                         targ_char_bit != 0) {
      /* The trailing base ends with a bit field that leaves some unused
         bits in its last byte. */
      a_targ_size_t  last_field_byte = offset + 1;
      last_field_byte +=
           (unsigned)(last_field->offset_bit_remainder+last_field->bit_size) /
                                                                 targ_char_bit;
      if (last_field_byte + btp->alignment <= *end_of_object) {
        /* The trailing bit field actually triggered the GNU bit field
           overpadding bug.  In that case, more bytes are trimmed, but one
           byte of overpadding remains nonetheless. */
        *end_of_object -= (btp->alignment - 1);
      } else {
        /* No overpadding: Just trim the last byte. */
        --*end_of_object;
      }  /* if */
    }  /* if */
  }  /* if */
}  /* gnu_trim_trailing_base_bits */


static a_field_ptr last_user_field_of(a_type_ptr  type)
/*
Return the last field declared by the user in the given class type.
*/
{
  a_field_ptr  result = NULL, fp;

  check_assertion(is_immediate_class_type(type));
  fp = type->variant.class_struct_union.field_list;
  for (; fp != NULL; fp = fp->next) {
    if (!fp->compiler_generated) {
      result = fp;
    }  /* if */
  }  /* for */
  return result;
}  /* last_user_field_of */


static void emulate_gnu_bit_field_overpadding(a_layout_block_ptr  lob,
                                              a_boolean           virtual_base)
/*
GNU C++ 3.3.x has a bug that causes it to insert extra padding ("overpadding")
after the last nonvirtual base and/or the last virtual base if that base has
a non-POD type and its last direct field is a bit field whose last bit is
located in the last byte (after alignment and excluding virtual bases) of the
base type.  The current layout state is described by lob (and may be modified
by this routine to account for the overpadding).  The flag virtual_base is set
when this routine is to calculate the overpadding for the last virtual base;
otherwise, the last nonvirtual base should be considered.
*/
{
  a_base_class_ptr  bcp = base_classes_of(lob->class_type);

  if (!virtual_base &&
      lob->class_type->variant.class_struct_union.field_list != NULL) {
    /* Nonvirtual bases are never overpadded if a field is to follow (even
       if that field is a zero-length bit field). */
    goto done;
  }  /* if */
  for (; bcp != NULL; bcp = bcp->next) {
    a_type_ptr  btp = bcp->type;
    if (bcp->direct && bcp->is_virtual == virtual_base &&
        bcp->offset_is_set &&
        !(btp->source_corresp.assoc_info != NULL &&
          symbol_supplement_for_class(btp)->is_cpp03_POD)) {
      /* This is the right kind of base.  Check if it is a base that covers
         the last byte allocated so far (assuming alignment). */
      an_unnormalized_bit_offset
                      dummy = 0;
      a_targ_size_t  bsize = btp->variant.class_struct_union.extra_info
                                ->size_without_virtual_base_classes;
      a_targ_size_t  next_byte = lob->byte_offset;
      (void)do_alignment(&next_byte, &dummy, lob->alignment);
      if (bcp->offset + bsize == next_byte) {
        a_field_ptr  fp = last_user_field_of(btp);
        if (fp != NULL && fp->is_bit_field && fp->bit_size != 0 &&
            (bsize - fp->offset)*targ_char_bit
                   - fp->offset_bit_remainder - fp->bit_size < targ_char_bit) {
          /* The last base is a bit field, and the last bit of that field is
             in the last byte of the base (excluding virtual bases). */
          lob->byte_offset += lob->alignment;
          break;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
done:;
}  /* emulate_gnu_bit_field_overpadding */

#endif /* IA64_ABI */

static void check_base_for_flex_array(a_layout_block  *lob,
                                      a_base_class    *bcp)

/*
Check whether the given base class has a flexible array member and, if so,
issue a diagnostic if needed.
*/
{
  if (bcp->direct &&
      bcp->type->variant.class_struct_union.contains_flexible_array_member &&
      (!gpp_version_is(any_version) ||
       (gpp_version_is(>=60000) && fields_of(lob->class_type) != NULL))) {
    /* Most compilers diagnose inheriting from a base class with a flexible
       array member.  GCC accepts it prior to version 6.x, after that it
       appears to accept it if there are no subsequent fields. */
    pos_error(ec_base_with_flexible_array, &bcp->decl_position);
  }  /* if */
}  /* check_base_for_flex_array */


static a_boolean set_field_size_and_offset(a_field_ptr         field,
                                           a_layout_block_ptr  lob)
/*

field points to a field that has not yet been allocated.  So far in the
class to which it belongs, the byte/bit offsets are as given by the
byte_offset and bit_offset fields of *lob.  Set the field's type size and
alignment, and update the byte/bit offset values.  The alignment field in
*lob contains the maximum alignment required so far in the structure, and is
updated if the new field requires a larger alignment value.  If any overflow
was detected in computing the byte or bit offset, FALSE is returned; if
there's no overflow TRUE is returned.

*/
{
  a_type_ptr                  field_type;
  a_targ_alignment            field_alignment = 0;
  a_boolean                   overflow = FALSE;
  a_targ_size_t               save_byte_offset;
  an_unnormalized_bit_offset  save_bit_offset;
  a_type_ptr                  class_type;
#if IA64_ABI
  a_boolean                   is_potentially_overlapping_data_member;
  a_targ_size_t               orig_byte_offset = lob->byte_offset;
  an_unnormalized_bit_offset  orig_bit_offset = lob->bit_offset;
#endif /* IA64_ABI */

  db_enter(4, "set_field_size_and_offset");
  /* Set the size and alignment for the field's type, if necessary. */
  field_type = skip_typerefs(field->type);
  class_type = parent_class_of(field);
#if IA64_ABI
  is_potentially_overlapping_data_member =
                                    (field->has_no_unique_address_attribute &&
                                     is_class_or_struct(field_type));
#endif /* IA64_ABI */
  if (is_error_type(field_type)) {
    /* Do nothing if the field has an error type. */
  } else if (microsoft_mode && field_is_nontrivial_property_or_event(field)) {
    /* Nontrivial property or event fields do not take any space. */
    field->offset = field->offset_bit_remainder = 0;
#if !IA64_ABI
  } else if (targ_optimize_empty_base_class_layout &&
             is_empty_field_for_layout_purposes(field)) {
    /* Empty classes with the [[no_unique_address]] attribute are treated
       as empty base classes in the Cfront ABI (and are allocated in
       set_offsets_for_empty_nonvirtual_base_classes and not here). */
#endif /* !IA64_ABI */
  } else {
    /* Ensure that the size of the field's type has been computed. */
    set_type_size(field_type);
    /* Check for a bit-field. */
    if (field->is_bit_field) {
      /* Do any necessary alignment for a bit-field. */
#if GNU_EXTENSIONS_ALLOWED
      if (curr_max_member_alignment == 0 &&
          ((field->is_packed && field->alignment == 0)
#if ABI_COMPATIBILITY_VERSION >= 307
           || (class_type->variant.class_struct_union.is_packed &&
#if IA64_ABI
               !(emulate_gnu_abi_bugs &&
                 gnu_abi_version < 40400 && gnu_abi_version >= 40100) &&
#endif /* IA64_ABI */
               field->alignment == 0)
#endif /* ABI_COMPATIBILITY_VERSION >= 307 */
                                     )) {
        /* No alignment to perform: Either the field is marked as "packed",
           or the whole class is marked as "packed", and no explicit alignment
           attribute was specified.  GNU versions 4.1.x through 4.3.x appear
           to ignore the "packed" attribute applied to a class type for the
           purpose of laying out bit fields.  If a bit field is both marked as
           "packed" and explicitly aligned, the alignment is performed. */
      } else
#endif /* GNU_EXTENSIONS_ALLOWED */
      {
        overflow = !align_offsets_for_bit_field(field, lob);
      }  /* if */
    } else {
      /* Do any necessary alignment for a normal field. */
      if (targ_microsoft_bit_field_allocation &&
          lob->curr_container_type != NULL) {
        /* This is a normal field immediately following a bit field.  When
           emulating Microsoft bit-field allocation, treat the container as
           having been independently allocated: pad out the rest of it before
           proceeding. */
        pad_ms_bit_field_container(lob);
      }  /* if */
      field_alignment = alignment_of_field(field);
      overflow = !do_alignment(&lob->byte_offset, &lob->bit_offset,
                               field_alignment);
      /* Remember the most stringent alignment requirement as the alignment
         requirement for the overall struct. */
      if (field_alignment > lob->alignment) lob->alignment = field_alignment;
    }  /* if */
    if (!overflow) {
      /* Save the current byte_offset and bit_offset values.  The bit_offset
         value for the field is not updated until after increment_field_offsets
         is called because the latter performs overflow checking. */
      save_byte_offset = lob->byte_offset;
      save_bit_offset = lob->bit_offset;
      /* Increment the current offsets to account for the field. */
      if (field->is_bit_field) {
        /* For a bit-field. */
        an_unnormalized_bit_offset bit_size =
                                   (an_unnormalized_bit_offset)field->bit_size;
        overflow = !increment_field_offsets(
                        &lob->byte_offset, &lob->bit_offset,
                        (a_targ_size_t)0,
                        targ_pad_bit_fields_larger_than_base_type ?
                                         field->declared_bit_size : bit_size);
        if (targ_microsoft_bit_field_allocation &&
            lob->curr_container_type != NULL) {
          /* Update the number of bits that are available in the container
             after the bit field is allocated by subtracting from the number
             of bits available in the container the number that is now being
             allocated.  It ought not to be a negative value. */
          check_assertion_str2(lob->curr_container_avail_bits >= bit_size,
                               "set_field_size_and_alignment:",
                               "bad curr_container_avail_bits adjustment");
          lob->curr_container_avail_bits -= bit_size;
          if (class_type->kind == (a_type_kind)tk_union) {
            /* Pad out the rest of the current container. */
            pad_ms_bit_field_container(lob);
#if RECORD_BIT_FIELD_CONTAINER_OFFSETS_IN_IL
          } else {
            /* Record the offset (in bytes) within the container.  This
               simplifies code generation for certain back ends. */
            an_unnormalized_bit_offset
                     container_bit_size, container_bit_offset;
            container_bit_size = skip_typerefs(lob->curr_container_type)->size
                               * targ_char_bit;
            container_bit_offset = container_bit_size - field->bit_size
                                 - lob->curr_container_avail_bits;
            field->offset_in_container = (a_bit_field_container_offset)
                                       (container_bit_offset / targ_char_bit);
#endif /* RECORD_BIT_FIELD_CONTAINER_OFFSETS_IN_IL */
          }  /* if */
        }  /* if */
      } else {
#if IA64_ABI
        /* Find an offset at which this field can be placed without creating a
           conflict between empty subobjects.  In unions, only one field
           exists at any time (and unions have no base classes); so there are
           never conflicts in those cases.  No test for conflict is needed if
           the tentative offset of the field is already beyond the extent of
           any allocated base class. */
        if (!C_mode() && class_type->kind != (a_type_kind)tk_union) {
          a_boolean offset_determined = FALSE;
          if (is_empty_field_for_layout_purposes(field)) {
            field->is_optimized_empty_class = TRUE;
            /* Fields that have the C++20 [[no_unique_address]] attribute
               and have empty class type can share an address with other
               non-static data members and/or a base class. */
            if (!subobject_conflict(lob, field_type, (a_targ_size_t)0,
                                    /*consider_bases=*/TRUE,
                                    /*consider_virtual_bases=*/TRUE,
                                    /*consider_fields=*/TRUE) ||
                    (emulate_gnu_abi_bugs &&
                     gnu_first_field_conflict(lob->class_type, field,
                                              (a_targ_size_t)0))) {
              save_byte_offset = 0;
              offset_determined = TRUE;
            }  /* if */
          }  /* if */
          if (!offset_determined &&
              (save_byte_offset <= lob->curr_extent ||
               is_empty_field_for_layout_purposes(field))) {
            /* When laying out base subobjects or fields with
               [[no_unique_address]], make sure a unique address is given. */
            while (subobject_conflict(lob, field_type, save_byte_offset,
                                      /*consider_bases=*/TRUE,
                                      /*consider_virtual_bases=*/TRUE,
                                      /*consider_fields=*/TRUE) ||
                   (emulate_gnu_abi_bugs &&
                    gnu_first_field_conflict(lob->class_type, field,
                                             save_byte_offset))) {
              /* The field can't go at this offset.  Advance by the field
                 alignment. */
              if (!increment_field_offsets(&lob->byte_offset,
                                           &lob->bit_offset,
                                           (a_targ_size_t)field_alignment,
                                           (an_unnormalized_bit_offset)0)) {
                overflow = TRUE;
                break;
              } else {
                save_byte_offset = lob->byte_offset;
              }  /* if */
            }  /* while */
          }  /* if */
        }  /* if */
        if (!overflow && !field->is_optimized_empty_class) {
#endif /* IA64_ABI */
          /* For a normal field. */
          a_targ_size_t size_to_allocate = size_of_type(field->type);
#if IA64_ABI
          if (is_potentially_overlapping_data_member) {
            /* A potentially-overlapping data member has special layout
               requirements in the IA-64 ABI; rather than the typical
               sizeof(field), use max(nvsize(field),dsize(field)) here and
               record a minimum class size later (to be used during
               "finalization") if the class ends up being too small. */
            a_targ_size_t dsize = compute_dsize(field_type);
            a_targ_size_t nvsize = class_type_supp(field_type)->
                                             size_without_virtual_base_classes;
            size_to_allocate = dsize;
            if (nvsize > size_to_allocate) {
              size_to_allocate = nvsize;
            }  /* if */
          }  /* if */
#endif /* IA64_ABI */
          overflow = !increment_field_offsets(&lob->byte_offset,
                                              &lob->bit_offset,
                                              size_to_allocate,
                                              (an_unnormalized_bit_offset)0);
#if IA64_ABI
        }  /* if */
#endif /* IA64_ABI */
      }  /* if */
      if (!overflow) {
        /* Now compute the field's bit offset within the struct.  We know the
           sum will fit in the bit_offset field because increment_field_offsets
           did not report overflow. */
        field->offset = save_byte_offset;
        check_assertion(save_bit_offset < targ_char_bit);
        field->offset_bit_remainder = (an_offset_bit_remainder)save_bit_offset;
#if IA64_ABI
        if (is_potentially_overlapping_data_member) {
          /* Record a minimum size for this class (this is used during the
             "finalization" step). */
          lob->min_final_class_size = max_val(field->offset + field_type->size,
                                              lob->min_final_class_size);
        }  /* if */
#endif /* IA64_ABI */
      }  /* if */
    }  /* if */
    if (overflow && !lob->any_overflow) {
      pos_error(struct_too_large_error(), &error_position);
      lob->any_overflow = TRUE;
    }  /* if */
  }  /* if */
#if IA64_ABI
  field->offset_is_set = TRUE;
  lob->curr_extent = max_val(lob->curr_extent, field->offset+field_type->size);
  if (warn_about_tail_padding_use &&
      class_type->variant.class_struct_union.field_list == field) {
    /* First field.  See if it reuses tail padding. */
    warn_if_offset_in_tail_padding(field, (a_base_class_ptr)NULL, lob);
  }  /* if */
  if (field->is_optimized_empty_class) {
    lob->byte_offset = orig_byte_offset;
    lob->bit_offset = orig_bit_offset;
  }  /* if */
#endif /* IA64_ABI */
  db_exit();
  return !overflow;
}  /* set_field_size_and_offset */


static a_targ_size_t set_offset_and_alignment(
                                    a_layout_block_ptr          lob,
                                    a_targ_size_t               size,
                                    a_targ_alignment            alignment,
                                    ARG_UNUSED a_base_class_ptr bcp)
/*
Given a subobject of the specified size in bytes and requiring the specified
alignment, allocate space for it in the class whose current status is
given in the layout block pointed to by lob.  Return the byte offset at
which it is allocated.  If bcp is non-NULL, it is the base class that is being
allocated.
*/
{
  a_targ_size_t  offset;
  a_boolean      do_not_update_overall_alignment = FALSE;

  /* Be sure the current byte_offset is consistent with the alignment required
     for the subobject. */
  if (!do_alignment(&lob->byte_offset, &lob->bit_offset, alignment)) {
    /* Issue an error only if one has not yet been put out. */
    if (!lob->any_overflow) {
      pos_error(struct_too_large_error(), &error_position);
      lob->any_overflow = TRUE;
    }  /* if */
  }  /* if */
#if IA64_ABI
  /* If placing the subobject at this location, skip forward until we find
     a location that works. */
  if (bcp != NULL) {
    while (base_subobject_conflict(lob, bcp, lob->byte_offset) ||
           (emulate_gnu_abi_bugs &&
            gnu_base_conflict(lob->class_type, bcp, lob->byte_offset))) {
      if (emulate_gnu_abi_bugs && bcp != NULL && !bcp->is_virtual) {
        /* Some GNU compilers do not update the alignment of the derived
           class if a base class could not be allocated at the first
           candidate offset. */
        do_not_update_overall_alignment = TRUE;
      }  /* if */
      if (!increment_field_offsets(&lob->byte_offset, &lob->bit_offset,
                                   (a_targ_size_t)alignment, 
                                   (an_unnormalized_bit_offset)0)) {
        /* Not enough space remains available in this class for this
           subobject. */
        if (!lob->any_overflow) {
          /* Issue an error only if one has not yet been put out. */
          pos_error(struct_too_large_error(), &error_position);
          lob->any_overflow = TRUE;
          break;
        }  /* if */
      }  /* if */
    }  /* while */
    if (bcp->direct || bcp->is_virtual) {
      lob->trailing_nonempty_base = bcp;
    }  /* if */
  }  /* if */
#endif /* IA64_ABI */
  /* Save the offset at which space for the subobject is being reserved. */
  offset = lob->byte_offset;
  /* Adjust the overall alignment requirement for the current class, if
     necessary. */
  if (lob->alignment < alignment && !do_not_update_overall_alignment) {
    lob->alignment = alignment;
  }  /* if */
  /* Advance the layout block's byte_offset value -- it will be class's
     size if no new subobjects are added or else the offset for the *next*
     subobject. */
  if (!increment_field_offsets(&lob->byte_offset, &lob->bit_offset, size,
                               (an_unnormalized_bit_offset)0)) {
    /* Not enough space remains available in the class for this subobject. */
    if (!lob->any_overflow) {
      /* Issue an error only if one has not yet been put out. */
      pos_error(struct_too_large_error(), &error_position);
      lob->any_overflow = TRUE;
    }  /* if */
  }  /* if */
  /* Return the offset for the current subobject.  This value will typically
     be stored in the data struct representing the subobject. */
  return offset;
}  /* set_offset_and_alignment */

#if IA64_ABI

static void allocate_empty_base(a_layout_block_ptr lob,
                                a_base_class_ptr   bcp)
/*
Allocate bcp (an empty base class).
*/
{
  a_targ_size_t               offset = (a_targ_size_t)0, size;
  a_targ_alignment            alignment;
  an_unnormalized_bit_offset  dummy = 0;

  /* Attempt to allocate the base at offset zero.  Some GNU compilers do not
     always use offset zero for the initial attempt at placing a direct empty
     virtual base: Instead they may use an offset computed for the virtual
     base in one of the direct base types. */
  if (emulate_gnu_abi_bugs && bcp->is_virtual) {
    offset = virtual_base_offset_computed_for_direct_base_type(bcp);
    if (offset != 0 && !base_subobject_conflict(lob, bcp, offset)) {
      /* Carry over the offset computed for a direct base. */
      bcp->offset = offset;
      goto done;
    }  /* if */
  }  /* if */
  if (offset == 0 &&
      !(base_subobject_conflict(lob, bcp, (a_targ_size_t)0) ||
        (emulate_gnu_abi_bugs &&
         gnu_leading_empty_base_conflict(lob->class_type, bcp)))) {
    bcp->offset = 0;
  } else {
    /* It didn't work at offset zero; try putting it at the end of the object 
       as created so far. */
    a_targ_size_t  end_of_object = lob->byte_offset;
    if (emulate_gnu_abi_bugs && bcp->is_virtual &&
        gnu_abi_version >= 30300 && gnu_abi_version < 30400) {
      /* Emulate a strange GNU 3.3 ABI bug that causes some empty virtual
         bases to be allocated in bit-field padding. */
      gnu_trim_trailing_base_bits(&end_of_object, lob);
    }  /* if */
    offset += end_of_object;
    if (bcp->direct) {
      /* If a GNU compiler initially tried a nonzero offset it effectively
         adds that offset to the current end of the object (thereby creating
         a "gap" in the layout).  However, if the virtual base is not also a
         direct base, GNU C++ will not update the object size.  The latter
         bug can cause a virtual base to end up at an offset larger than the
         size of the class.  In such cases, offset will be adjusted in
         reposition_gnu_disconnected_virtual_bases. */
      lob->byte_offset = offset;
    }  /* if */
    size = class_type_supp(bcp->type)->alignment_without_virtual_base_classes;
    while (base_subobject_conflict(lob, bcp, offset)) {
      if (!increment_field_offsets(&offset, &dummy, size, 
                                   (an_unnormalized_bit_offset)0)) {
        /* Not enough space remains available in this class for this
           subobject. */
        if (!lob->any_overflow) {
          /* Issue an error only if one has not yet been put out. */
          pos_error(struct_too_large_error(), &error_position);
          lob->any_overflow = TRUE;
          break;
        }  /* if */
      }  /* if */
    }  /* while */
    bcp->offset = offset;
  }  /* if */
done:
  bcp->is_optimized_empty_base = TRUE;
  /* Ensure the alignment of the class as a whole is at least as strict as
     that of the empty base.  (Early versions of g++ do not do this.) */
  alignment = alignment_of_type(bcp->type);
  if (packing_applies_to_base_classes) {
    adjust_alignment_for_packing(&alignment, bcp->derived_class);
  }  /* if */
  if (alignment > lob->alignment &&
      !(emulate_gnu_abi_bugs && gnu_abi_version < 40300)) {
    lob->alignment = alignment;
  }  /* if */
}  /* allocate_empty_base */

#endif /* IA64_ABI */

static void set_base_class_offsets(a_layout_block    *lob,
                                   a_base_class_ptr  proximate_derivation)
/*
The offset of base class proximate_derivation has been computed, but the
offsets of its own base classes have not.  The subtle aspect of this
processing is that two different base class entries are involved.  First,
the "most derived class" contains a list of all its base classes, direct
and indirect.  But each class from which it is derived has its own base
class list, too.  For example:

          A    A       class B points to base class A (path ==>A)
          |    |
          B    C       class C points to base class A (path ==>A)
           \  /
             D         class D points to base classes B (path ==>B)
                                                      A (path ==>B==>A)
                                                      C (path ==>C)
                                                      A (path ==>C==>A)

Thus, while the base classes for D, the "most derived class", are
represented by only 3 type entries (A, B, and C), there are 4 base class
entries involved, since 2 are associated with class A.  As for offsets, B's
"A" base class entry indicates the offset of the "A" data section within
the block occupied by class "B" entities, whereas the two "A" base classes
associated with class D should have offsets representing their locations
within a "D" object.  Computing the offset of an indirect base class (e.g.,
A) within a most derived class (e.g., D) requires adding the offset of the
base class immediately derived from it (e.g., B) to its own offset within
that class (e.g., A's offset within B); in other words, the D::A offset
equals the D::B offset plus the B::A offset.

The algorithm involves going through direct base classes of
proximate_derivation (in this example "B in D" is the proximate_derivation,
and it has only one direct base class of its own, namely, "A in B"),
finding the corresponding base class entry in the most derived class (e.g.,
finding the appropriate "A in D" -- the one whose path is ==>B==A), and
setting the offset field in the latter.

lob points to a block of information that describes some current state about
the layout of lob->class_type (which is also proximate_derivation->type).
*/
{
  a_base_class_ptr  ref_bcp;
#if IA64_ABI
  a_boolean         consider_indirect_bases = FALSE;
#endif /* IA64_ABI */

  db_enter(4, "set_base_class_offsets");
  /* Remember that the offset for this base has been set. */
#if IA64_ABI
  proximate_derivation->offset_is_set = TRUE;
  if (proximate_derivation->primary_base_class != NULL &&
      proximate_derivation->primary_base_class->is_virtual) {
    /* An indirect nearly-empty virtual base might have been chosen as a
       primary base.  We will therefore have to consider indirect bases. */
    consider_indirect_bases = TRUE;
    ref_bcp = base_classes_of(proximate_derivation->type);
  } else
#endif /* IA64_ABI */
  /* Do not insert code here. */
  {
    /* Get the first "reference" base class of the root class, which is itself
       a base class of the most derived class.  It is called a reference base
       class because it contains an offset relative to the root base class.
       It is the offset value relative to the most derived class that we need
       to determine and record. */
    ref_bcp = direct_base_classes_of(proximate_derivation->type);
  }  /* if */
#if DEBUG
  if (debug_level >= 4) {
    if (ref_bcp != NULL) {
      fputs("setting offsets for base classes of:\n  ", f_debug);
      db_base_class(proximate_derivation, /*show_offset=*/TRUE);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  /* Loop through the reference base classes, the direct base classes of
     the proximate_derivation base class. */
  for (; ref_bcp != NULL;
#if IA64_ABI
       ref_bcp = consider_indirect_bases ? ref_bcp->next : ref_bcp->next_direct
#else /* !IA64_ABI */
       ref_bcp = ref_bcp->next
#endif /* IA64_ABI */
                              ) {
    a_base_class_ptr  bcp = NULL;
    if (ref_bcp->direct) {
      /* The most common case of interest: ref_bcp is a direct base class
         of proximate_derivation->type.  We can therefore find the
         corresponding base by using proximate_derivation itself as a
         disambiguator. */
      bcp = corresponding_base_class(ref_bcp,
                                     proximate_derivation->derived_class,
                                     proximate_derivation);
#if IA64_ABI
    } else if (consider_indirect_bases && ref_bcp->is_virtual) {
      /* In the IA64 ABI we must consider the possibility that an indirect
         virtual (nearly empty) base is the primary base. */
      bcp = corresp_base_class(ref_bcp, proximate_derivation);
      if (proximate_derivation->primary_base_class != bcp) {
        bcp = NULL;
      }  /* if */
#endif /* IA64_ABI */
    }  /* if */
    if (bcp != NULL) {
      if (!bcp->is_virtual) {
        /* Nonvirtual base class. */
        bcp->offset = proximate_derivation->offset + ref_bcp->offset;
#if IA64_ABI
        bcp->offset_is_set = TRUE;
#endif /* IA64_ABI */
#if DEBUG
        if (debug_level >= 4) {
          fputs("reference base class ", f_debug);
          db_base_class(ref_bcp, /*show_offset=*/TRUE);
          fputs("new offset for ", f_debug);
          db_base_class(bcp, /*show_offset=*/TRUE);
        }  /* if */
#endif /* DEBUG */
#if IA64_ABI
      } else if (proximate_derivation->primary_base_class == bcp) {
        /* This virtual base class is the primary base class of the
           proximate_derivation. */
        bcp->offset = proximate_derivation->offset;
        bcp->offset_is_set = TRUE;
#endif /* IA64_ABI */
#if CFRONT_OBJECT_CODE_COMPATIBILITY
      } else {
        /* Virtual base class. */
        if (bcp->data_section_base_class == proximate_derivation) {
          /* bcp is a virtual base class whose data section is embedded in
             the data section of another base class data section.  Update
             the offset. */
          bcp->offset = proximate_derivation->offset + ref_bcp->offset;
#if DEBUG
          if (debug_level >= 4) {
            fputs("reference base class ", f_debug);
            db_base_class(ref_bcp, /*show_offset=*/TRUE);
            fputs("new offset for ", f_debug);
            db_base_class(bcp, /*show_offset=*/TRUE);
          }  /* if */
#endif /* DEBUG */
        }  /* if */
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
      }  /* if */
#if IA64_ABI
      if (!bcp->offset_is_set) {
        /* Nothing to do. */
      } else
#endif /* IA64_ABI */
      /* Do not insert code here. */
      {
#if IA64_ABI
        a_targ_size_t  end = bcp->offset
                           + class_type_supp(bcp->type)
                                          ->size_without_virtual_base_classes;
        lob->curr_extent = max_val(lob->curr_extent, end-1);
#endif /* IA64_ABI */
        /* Make a recursive call to apply this processing to the next level of
           base classes. */
        set_base_class_offsets(lob, bcp);
      }  /* if */
#if CFRONT_OBJECT_CODE_COMPATIBILITY
    } else if (ref_bcp->is_virtual) {
      /* Virtual indirect base class. */
      bcp = corresponding_base_class(ref_bcp,
                                     proximate_derivation->derived_class,
                                     (a_base_class_ptr)NULL);
      if (bcp->data_section_base_class == proximate_derivation) {
        /* bcp is a virtual base class whose data section is embedded in
           the data section of another base class data section.  Update
           the offset. */
        bcp->offset = proximate_derivation->offset + ref_bcp->offset;
#if DEBUG
        if (debug_level >= 4) {
          fputs("reference base class ", f_debug);
          db_base_class(ref_bcp, /*show_offset=*/TRUE);
          fputs("new offset for ", f_debug);
          db_base_class(bcp, /*show_offset=*/TRUE);
        }  /* if */
#endif /* DEBUG */
      }  /* if */
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
    }  /* if */
  }  /* for */
  db_exit();
}  /* set_base_class_offsets */


static void set_offset_for_nonvirtual_base_class(a_layout_block_ptr lob,
                                                 a_base_class_ptr   bcp)
/*
Lay out the nonvirtual direct base class bcp.
*/
{
  a_targ_size_t     size;
  a_targ_alignment  alignment;

  check_assertion(!bcp->is_virtual && bcp->direct);
  if (targ_optimize_empty_base_class_layout &&
      is_empty_class_type(bcp->type)) {
#if !IA64_ABI
    /* Empty bases will be allocated later. */
#else /* IA64_ABI */
    allocate_empty_base(lob, bcp);
#endif /* !IA64_ABI */
  } else 
  /* Do not add code here. */
  {
#if CFRONT_OBJECT_CODE_COMPATIBILITY
    /* When cfront compatibility is required, space for a complete
       subobject (i.e., including space for it virtual base classes)
       is sometimes reserved, depending on how the complete_subobject
       flag has been set during prior processing. */
    if (bcp->complete_subobject) {
      alignment = bcp->type->alignment;
      size = bcp->type->size;
    } else
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
    /* Do not insert code here. */
    {
      /* For a nonvirtual base classes reserve space for all the base
         class except what is required for its own virtual base classes.
         The latter will be added at the end of the storage. */
      a_class_type_supplement_ptr  base_ctsp = class_type_supp(bcp->type);
      size = base_ctsp->size_without_virtual_base_classes;
      alignment = base_ctsp->alignment_without_virtual_base_classes;
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (microsoft_mode && type_contains_explicit_alignment(bcp->type)) {
        /* Microsoft does not apply packing/alignment directives to bases of
           types with explicit alignment requirements. */
      } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* Do not insert code here. */
      if (packing_applies_to_base_classes) {
        adjust_alignment_for_packing(&alignment, bcp->derived_class);
      }  /* if */
    }  /* if */
    bcp->offset = set_offset_and_alignment(lob, size, alignment, bcp);
#if DEBUG
    if (debug_level >= 4) {
      fputs("updated offset for ", f_debug);
      db_base_class(bcp, /*show_offset=*/TRUE);
    }  /* if */
#endif /* DEBUG */
  }  /* if */
#if IA64_ABI
  if (warn_about_tail_padding_use) {
    /* Examine if this base class was allocated in the tail padding of
       another base. */
    warn_if_offset_in_tail_padding((a_field_ptr)NULL, bcp, lob);
  }  /* if */
  /* Set the offsets for all of the non-virtual bases of this base. */
  set_base_class_offsets(lob, bcp);
#endif /* IA64_ABI */
}  /* set_offset_for_nonvirtual_base_class */


static void set_offsets_for_nonvirtual_base_classes(a_layout_block_ptr  lob)
/*
Lay out the class_type object to store the nonvirtual direct base classes.
(They will precede the fields of the current class.)  Do this by going
through the base classes list for the current class and reserving enough
space for each base class that is directly and nonvirtually inherited;
storage is reserved in the order in which the base classes appear.  (Virtual
base classes, both direct and indirect, are dealt with after the fields of
the current class are allocated; indirect nonvirtual base classes are just
subobjects of the direct nonvirtual base classes.)  Lob points to the
layout block used to track the layout of the current class.
*/
{
  a_base_class_ptr            bcp;
#if IA64_ABI || MICROSOFT_EXTENSIONS_ALLOWED
  a_class_type_supplement_ptr ctsp;
#endif /* IA64_ABI || MICROSOFT_EXTENSIONS_ALLOWED */

  db_enter(4, "set_offsets_for_nonvirtual_base_classes");
#if IA64_ABI || MICROSOFT_EXTENSIONS_ALLOWED
  ctsp = lob->class_type->variant.class_struct_union.extra_info;
#endif /* IA64_ABI || MICROSOFT_EXTENSIONS_ALLOWED */
  /* Traverse the list of base classes. */
  for (bcp = base_classes_of(lob->class_type); bcp != NULL; bcp = bcp->next) {
    if (bcp->direct && !bcp->is_virtual 
#if IA64_ABI
        && bcp != ctsp->primary_base_class
#endif /* IA64_ABI */
                                          ) {
      set_offset_for_nonvirtual_base_class(lob, bcp);
#if IA64_ABI
      update_curr_base_extent(lob, bcp);
#endif /* IA64_ABI */
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (microsoft_mode && type_contains_explicit_alignment(bcp->type)) {
        /* Microsoft does not apply packing/alignment directives to subobjects
           of types with explicit alignment requirements. */
        ctsp->has_explicitly_aligned_subobject = TRUE;
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      check_base_for_flex_array(lob, bcp);
    }  /* if */
  }  /* for */
#if IA64_ABI
  if (emulate_gnu_abi_bugs) {
    if (gnu_abi_version >= 30300 && gnu_abi_version < 30400) {
      /* GNU C++ version 3.3 sometimes "overpads" a class whose last base
         ends with a bit field. */
      emulate_gnu_bit_field_overpadding(lob, /*virtual_base=*/FALSE);
    }  /* if */
  }  /* if */
#endif /* IA64_ABI */
  db_exit();
}  /* set_offsets_for_nonvirtual_base_classes */

#if !IA64_ABI

static a_base_class_ptr next_empty_nonvirtual_direct_base(a_base_class_ptr
                                                                         ebcp)
/*
Return the given base class if it is empty; otherwise the next empty
nonvirtual direct base or NULL if there is none such.
*/
{
  for (; ebcp != NULL; ebcp = ebcp->next) {
    if (ebcp->direct && !ebcp->is_virtual &&
        is_empty_class_type(ebcp->type)) {
      break;
    }  /* if */
  }  /* for */
  return ebcp;
}  /* next_empty_nonvirtual_direct_base */


static a_base_class_ptr next_nonempty_nonvirtual_direct_base(a_base_class_ptr
                                                                         nbcp)
/*
Return the given base class if it is nonempty; otherwise the next nonempty
nonvirtual direct base or NULL if there is none such.
*/
{
  for (; nbcp != NULL; nbcp = nbcp->next) {
    if (nbcp->direct && !nbcp->is_virtual &&
        !is_empty_class_type(nbcp->type)) {
      break;
    }  /* if */
  }  /* for */
  return nbcp;
}  /* next_nonempty_nonvirtual_direct_base */


static a_field_ptr first_allocated_field(a_type_ptr class_type)
/*
Return the first field of a given class to be allocated.  By default this is
the first declared field; if targ_field_alloc_sequence_equals_decl_sequence is
defined to be FALSE however, it is the first field with the most access (i.e.,
public is preferred over protected, which is preferred over private).
Also, in Microsoft mode we must skip over property fields.
*/
{
  a_field_ptr  result;

  if (targ_field_alloc_sequence_equals_decl_sequence) {
    result = class_type->variant.class_struct_union.field_list;
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode) {
      while (result && field_is_nontrivial_property_or_event(result)) {
        result = result->next;
      }  /* while */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else {
    /* Public fields are allocated first, then the protected ones and finally
       the private ones; so fetch the first allocated one. */
    an_access_specifier access = (an_access_specifier)as_inaccessible;
    a_field_ptr         field =
                            class_type->variant.class_struct_union.field_list;
    result = field;
    while (field) {
      if (microsoft_mode && field_is_nontrivial_property_or_event(result)) {
        /* Nontrivial property fields do not take any space. */
      } else if (field->source_corresp.access ==
                                             (an_access_specifier)as_public) {
        break;
      } else if (field->source_corresp.access < access) {
        access = enum_cast<an_access_specifier>(field->source_corresp.access);
        result = field;
      }  /* if */
      field = field->next;
    }  /* while */
  }  /* if */
  return result;
}  /* first_allocated_field */


static a_field_ptr next_empty_class_field(a_type_ptr class_type)
/*
Returns the first field in the specified class that is considered an
empty class field for the purposes of layout, or NULL if there are none.
*/
{
  a_field_ptr      efp = NULL;

  /* Don't bother to look if [[no_unique_address]] has not been seen. */
  if (no_unique_address_attribute_seen) {
    for (efp = first_allocated_field(class_type);
         efp != NULL;
         efp = efp->next) {
      if (is_empty_field_for_layout_purposes(efp)) {
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return efp;
}  /* next_empty_class_field */


static void set_offsets_for_empty_nonvirtual_base_classes(
                                                      a_layout_block_ptr  lob)
/*
This routine is called if the empty base optimization is enabled.  If so,
empty base subobjects and certain empty class fields (i.e., those with the
[[no_unique_address]] attribute) were not yet allocated in the class layout and
this runs an extra pass to allocate them at the same location as other bases
or (when that is not possible) just after the last already allocated base.
Most of the work consists in avoiding situations where two empty classes would
end up at the same address.  The layout state lob is updated if necessary. 
*/
{
  a_type_ptr       class_type = lob->class_type;
  /* nbcp points to the next nonempty base class; ebcp to the next empty base
     class. */
  a_base_class_ptr nbcp = next_nonempty_nonvirtual_direct_base(
                                                 base_classes_of(class_type));
  a_base_class_ptr ebcp = next_empty_nonvirtual_direct_base(
                                                 base_classes_of(class_type));
  a_base_class_ptr first_empty_base = ebcp, last_optimized_base = NULL;
  a_boolean        conflict;
  a_field_ptr      efp = next_empty_class_field(class_type);
  a_field_ptr      first_empty_class_field = efp;
  a_type_ptr       empty_class_type;

  /* This loop first covers empty base classes, then empty class fields. */
  while (ebcp != NULL || efp != NULL) {
    conflict = FALSE;
    if (ebcp != NULL) {
      ebcp->is_optimized_empty_base = TRUE; /* Assume we can overlap it. */
      /* First, tentatively allocate the empty base ignoring conflicts. */
      if (nbcp != NULL) {
        ebcp->offset = nbcp->offset;
      } else {
        ebcp->offset = lob->byte_offset;
      }
      empty_class_type = ebcp->type;
    } else {
      efp->is_optimized_empty_class = TRUE;
      efp->offset = lob->byte_offset;
      empty_class_type = efp->type;
    }  /* if */
    /* Next, verify if this offset causes a conflict with another empty
       subobject that has a common empty type at that location. */
    if (nbcp != NULL && empty_base_conflict(empty_class_type, nbcp->type, 
                                            (a_base_class_ptr)NULL,
                                            (a_targ_size_t)0,
                                            /*consider_virtual_bases=*/TRUE,
                                            /*consider_fields=*/TRUE)) {
      /* A conflict with a nonempty base subobject. */
      conflict = TRUE;
    } else {
      /* Check for conflicts with previously allocated empty bases. */
      a_base_class_ptr prior_ebcp = first_empty_base;
      a_field_ptr prior_efp = first_empty_class_field;
      while (ebcp != NULL && prior_ebcp != NULL && prior_ebcp != ebcp) {
        if (prior_ebcp->offset == ebcp->offset &&
            empty_base_conflict(empty_class_type, prior_ebcp->type,
                                (a_base_class_ptr)NULL,
                                (a_targ_size_t)0,
                                /*consider_virtual_bases=*/TRUE,
                                /*consider_fields=*/TRUE)) {
          conflict = TRUE;
          break;
        }  /* if */
        prior_ebcp = next_empty_nonvirtual_direct_base(prior_ebcp->next);
      }  /* while */
      /* Also check for conflicts with previously allocated empty class
         fields. */
      while (prior_efp != NULL && prior_efp != efp) {
        if (prior_efp->offset == efp->offset &&
            empty_base_conflict(empty_class_type, prior_efp->type,
                                (a_base_class_ptr)NULL,
                                (a_targ_size_t)0,
                                /*consider_virtual_bases=*/TRUE,
                                /*consider_fields=*/TRUE)) {
          conflict = TRUE;
          break;
        }  /* if */
        for (prior_efp = prior_efp->next;
             prior_efp != NULL;
             prior_efp = prior_efp->next) {
          if (is_empty_field_for_layout_purposes(prior_efp)) {
            break;
          }  /* if */
        }  /* for */
      }  /* while */
    }  /* if */
    /* If there was no conflict, move to the next empty class; otherwise,
       try to find another slot where the empty class could be allocated. */
    if (!conflict) {
      if (ebcp != NULL) {
        /* Move to the next empty base class. */
        last_optimized_base = ebcp;
        ebcp = next_empty_nonvirtual_direct_base(ebcp->next);
      } else {
        /* The empty class field has been successfully placed. */
        if (last_optimized_base != NULL &&
            efp->offset != last_optimized_base->offset) {
          /* An empty class field now follows the last optimized base. */
          last_optimized_base = NULL;
        }  /* if */
        /* Skip to the next empty field class (with the [[no_unique_address]]
           attribute), if any. */
        for (efp = efp->next; efp != NULL; efp = efp->next) {
          if (is_empty_field_for_layout_purposes(efp)) {
            break;
          }  /* if */
        }  /* for */
      }  /* if */
    } else {
      if (nbcp != NULL) {
        nbcp = next_nonempty_nonvirtual_direct_base(nbcp->next);
      } else {
        /* We were already at the end of the list of nonempty bases.  So the
           conflict was with a previously allocated empty base. Move to the
           next available byte. */
        lob->byte_offset += targ_minimum_struct_alignment;
        /* The previously allocated empty base takes up its own space after
           all. */
        if (last_optimized_base != NULL) {
          last_optimized_base->is_optimized_empty_base = FALSE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* while */
  /* Finally, check if we created a conflict with the first field. */
  if (nbcp != NULL || first_empty_base == NULL) {
    /* There are nonempty bases left after the last empty base, so there
       cannot be a conflict with the fields (since they are allocated after
       the nonempty base). */
  } else {
    a_field_ptr field = first_allocated_field(class_type);
    /* Skip past any initial empty class fields. */
    while (field != NULL && is_empty_field_for_layout_purposes(field)) {
      field = field->next;
    }  /* for */
    if (field) {
      /* There is a field. */
      a_type_ptr field_type = skip_typerefs(field->type);
#if ABI_COMPATIBILITY_VERSION >= 300
      if (is_array_type(field_type)) {
        /* If the field has an array type, we are really interested in the
           type of its first element. */
        field_type = underlying_array_element_type(field_type);
        field_type = skip_typerefs(field_type);
      }  /* if */
#endif /* ABI_COMPATIBILITY_VERSION >= 300 */
      if (is_class_struct_union_type(field_type)) {
        ebcp = next_empty_nonvirtual_direct_base(base_classes_of(class_type));
        /* First check empty bases. */
        while (ebcp) {
          if (ebcp->offset == lob->byte_offset &&
              empty_base_conflict(ebcp->type, field_type, 
                                  (a_base_class_ptr)NULL,
                                  (a_targ_size_t)0,
                                  /*consider_virtual_bases=*/TRUE,
                                  /*consider_fields=*/TRUE)) {
            lob->byte_offset += targ_minimum_struct_alignment;
            break;
          }  /* if */
          ebcp = next_empty_nonvirtual_direct_base(ebcp->next);
        }  /* while */
        /* Now check empty class fields. */
        for (efp = next_empty_class_field(class_type);
             efp != NULL;
             efp = efp->next) {
          if (is_empty_field_for_layout_purposes(efp) &&
              efp->offset == lob->byte_offset &&
              empty_base_conflict(efp->type, field_type, 
                                  (a_base_class_ptr)NULL,
                                  (a_targ_size_t)0,
                                  /*consider_virtual_bases=*/TRUE,
                                  /*consider_fields=*/TRUE)) {
            lob->byte_offset += targ_minimum_struct_alignment;
            break;
          }  /* if */
        }  /* if */
      }  /* if */
    } else {
      /* We cannot end the layout with a zero-sized empty base because
         otherwise we might end up conflicting with an adjacent object. */
      a_class_type_supplement_ptr
           ctsp = class_type->variant.class_struct_union.extra_info;
      if (class_type->variant.class_struct_union.any_virtual_functions &&
          ctsp->virtual_function_info_base_class == NULL) {
        /* This class has its own (as opposed to inherited) virtual function
           info block pointer, whose offset can be shared by the last empty
           class. */
      } else {
        /* Check if there is a direct virtual base: if so, there will be a
           virtual base pointer whose offset can be shared by the last empty
           class. */
        a_base_class_ptr  bcp = base_classes_of(class_type);
        for (; bcp != NULL; bcp = bcp->next) {
          if (bcp->direct && bcp->is_virtual) { break; }
        }  /* for */
        if (bcp == NULL) {
          /* We did not find anything to share an offset with, so allocate
             space for the last empty class. */
          lob->byte_offset += targ_minimum_struct_alignment;
        }  /* if */
      }  /* if */
    }  /* if */
    if (last_optimized_base &&
        last_optimized_base->offset != lob->byte_offset) {
      /* The last empty base takes up its own place after all. */
      last_optimized_base->is_optimized_empty_base = FALSE;
    }  /* if */
  }  /* if */
}  /* set_offsets_for_empty_nonvirtual_base_classes */


static void check_if_last_empty_base_is_optimized(a_layout_block_ptr  lob)
/*
An empty base is said to be "optimized" if it is allocated at the same offset
as another subobject.  That other subobject can be: another empty base, a
field, a virtual function info block (typically: a pointer to a virtual
function table) or a pointer to a virtual base.  The latter three cases only
occur for the last declared empty base and can only reliably be determined
after the corresponding subobjects have been allocated.  Hence this separate
function to confirm the "is_optimized_empty_base" bit.
*/
{
  a_type_ptr        type = lob->class_type;
  a_base_class_ptr  last = 0, ebcp = base_classes_of(type);
  a_targ_size_t     max_base_offset = 0;
  a_boolean         max_base_offset_valid = FALSE;

  /* Look for the direct nonvirtual base with the largest offset. */
  for (; ebcp != NULL; ebcp = ebcp->next) {
    if (ebcp->direct && !ebcp->is_virtual) {
      if (!max_base_offset_valid || ebcp->offset>max_base_offset) {
        max_base_offset_valid = TRUE;
        max_base_offset = ebcp->offset;
        last = ebcp;
      } else if (last != NULL && ebcp->offset == max_base_offset) {
        /* The are two base at the same offset: at least one was optimized and
           another base at that offset must be nonempty or not optimized. */
        last = NULL;
      }  /* if */
    }  /* if */
  }  /* for */
  /* If there was a trailing optimized base, check if it really shares the
     offset of another subobject. */
  if (last && last->is_optimized_empty_base) {
    a_field_ptr  first_field = first_allocated_field(type);

    last->is_optimized_empty_base = FALSE;
    if (first_field && first_field->offset == last->offset) {
      /* The last base is allocated at the address of the first field, so
         it is indeed optimized. */
      last->is_optimized_empty_base = TRUE;
    } else {
      a_class_type_supplement_ptr ctsp =
                                  type->variant.class_struct_union.extra_info;
      if (ctsp->virtual_function_info_offset == last->offset) {
        last->is_optimized_empty_base = TRUE;
      } else if (type->variant.class_struct_union.any_virtual_base_classes) {
        a_base_class_ptr  bcp = base_classes_of(type);

        for (; bcp != NULL; bcp = bcp->next) {
          if (bcp->is_virtual && (bcp->pointer_offset == last->offset ||
                                  bcp->offset == last->offset)) {
            last->is_optimized_empty_base = TRUE;
            break;
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
    if (!last->is_optimized_empty_base && last->offset == lob->byte_offset) {
      /* This empty base class is at the end of the current layout.
         Adjust the total class size to reflect the fact that this base
         class occupies some space after all. */
      lob->byte_offset += last->type->size;
    }  /* if */
  }  /* if */
}  /* check_if_last_empty_base_is_optimized */

#endif /* !IA64_ABI */

static void set_offsets_for_fields(a_layout_block_ptr  lob)
/*
Set the sizes and offsets of the fields of lob->class_type. By default (i.e.,
when targ_field_alloc_sequence_equals_decl_sequence is TRUE) the fields are
allocated in exactly the same order in which they were declared. Optionally
(i.e., when targ_field_alloc_sequence_equals_decl_sequence is FALSE), fields
are grouped by access before being allocated (though within each group they
are allocated in declaration order).
*/
{
  a_type_ptr                  class_type = lob->class_type;
  a_field_ptr                 fp;
  a_targ_size_t               initial_byte_offset = lob->byte_offset;
  a_targ_size_t               max_byte_offset = initial_byte_offset;
  an_unnormalized_bit_offset  max_bit_offset = 0;
  an_access_specifier         access = (an_access_specifier)as_public;

  /* When targ_field_alloc_sequence_equals_decl_sequence is FALSE, fields are
     allocated in groups based on access -- first all the public fields, then
     all the protected fields, and finally all the private fields.  Therefore,
     there is an outer loop so that the field list is traversed three times,
     once for each access category. */
  for (;;) {
    /* Traverse the field list. */
    for (fp = class_type->variant.class_struct_union.field_list;
         fp != NULL;
         fp = fp->next) {
      if (targ_field_alloc_sequence_equals_decl_sequence ||
          fp->source_corresp.access == access) {
        if (class_type->kind == (a_type_kind)tk_union) {
          /* All fields in a union have offset zero. */
          lob->byte_offset = initial_byte_offset;
          lob->bit_offset = 0;
        }  /* if */
        if (set_field_size_and_offset(fp, lob)) {
          /* Offset values were modified.  For unions save the highest offset
             in order to establish the size of the overall aggregate. */
          if (class_type->kind == (a_type_kind)tk_union &&
              (lob->byte_offset > max_byte_offset ||
               (lob->byte_offset == max_byte_offset &&
                lob->bit_offset > max_bit_offset))) {
            max_byte_offset = lob->byte_offset;
            max_bit_offset = lob->bit_offset;
          }  /* if */
        }  /* if */
      }  /* if */
      /* Continue the field list traversal. */
    }  /* for */
    if (targ_field_alloc_sequence_equals_decl_sequence) {
      break;
    }  /* if */
    /* Advance to the next access specifier and resume the outer loop. */
    if (access == (an_access_specifier)as_public) {
      access = (an_access_specifier)as_protected;
    } else if (access == (an_access_specifier)as_protected) {
      access = (an_access_specifier)as_private;
    } else {
      /* This must be the third iteration of the outer loop -- we're done. */
      break;
    }  /* if */
  }  /* for */
  if (class_type->kind == (a_type_kind)tk_union) {
    /* Now reset the offset fields in the layout block to reflect the minimum
       size this union has to be. */
    lob->byte_offset = max_byte_offset;
    lob->bit_offset = max_bit_offset;
  } else {
    if (targ_microsoft_bit_field_allocation &&
        lob->curr_container_type != NULL) {
      /* The last field in the struct was a bit field.  When emulating
         Microsoft bit-field allocation, treat the container as having been
         independently allocated: pad out the rest of it before proceeding. */
      pad_ms_bit_field_container(lob);
    }  /* if */
  }  /* if */
}  /* set_offsets_for_fields */


static void set_offset_for_virtual_function_info(a_layout_block_ptr  lob)
/*
In C++ classes with virtual functions provide a special mechanism for
dynamic function binding.  Typically, this is a virtual function table,
and each object of the class contains a pointer to the table.  For each
class with virtual functions this routine allocates a field to contain
such a pointer -- or other data as required by a given implementation.
The size and alignment of such a field are defined by global configuration
variables that can be redefined for various implementation strategies.  lob
points to the layout block used to track the layout of the current class.
*/
{
  a_targ_size_t                size;
  a_targ_alignment             alignment;
  a_base_class_ptr             bcp;

  db_enter(4, "set_offset_for_virtual_function_info");
  if (needs_virtual_function_table(lob->class_type)) {
    a_class_type_supplement_ptr  ctsp, bcp_ctsp;
    ctsp = lob->class_type->variant.class_struct_union.extra_info;
    if (ctsp->virtual_function_info_base_class == NULL) {
      size = (a_targ_size_t)targ_sizeof_virtual_function_info;
      alignment = (a_targ_alignment)targ_alignof_virtual_function_info;
      /* Adjust the vtbl pointer's alignment for packing, if required. */
      adjust_alignment_for_packing(&alignment, lob->class_type);
      ctsp->virtual_function_info_offset =
                            set_offset_and_alignment (lob, size, alignment,
                                                      (a_base_class_ptr)NULL);
    } else {
      bcp = ctsp->virtual_function_info_base_class;
#if CHECKING
      if (bcp->offset != 0) {
        internal_error(
                  "set_offset_for_virtual_function_info: non-zero bcp offset");
      }  /* if */
#endif /* CHECKING */
      bcp_ctsp = bcp->type->variant.class_struct_union.extra_info;
      ctsp->virtual_function_info_offset = 
                          bcp->offset + bcp_ctsp->virtual_function_info_offset;
    }  /* if */
  }  /* if */
  db_exit();
}  /* set_offset_for_virtual_function_info */

#if !IA64_ABI

static void pointer_offset_for_virtual_base_class(a_layout_block_ptr  lob,
                                                  a_base_class_ptr    bcp)
/*
Allocate space in the current class for a pointer to virtual base class
bcp.
*/
{
  a_targ_size_t      size;
  a_targ_alignment   alignment;

  db_enter(4, "pointer_offset_for_virtual_base_class");
#if CFRONT_OBJECT_CODE_COMPATIBILITY
#if CHECKING
  if (bcp->pointer_offset_is_set) {
    internal_error("pointer_offset_for_virtual_base_class: already set");
  }  /* if */
#endif /* CHECKING */
  bcp->pointer_offset_is_set = TRUE;
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
  size = (a_targ_size_t)targ_sizeof_ptr_to_virtual_base_class;
  alignment = (a_targ_alignment)targ_alignof_ptr_to_virtual_base_class;
  /* Adjust the virtual base class pointer's alignment for packing, if
     required. */
  adjust_alignment_for_packing(&alignment, lob->class_type);
  bcp->pointer_offset = set_offset_and_alignment(lob, size, alignment,
                                                 (a_base_class_ptr)NULL);
#if DEBUG
  if (debug_level >= 4) {
    fputs("updated pointer offset for ", f_debug);
    db_base_class(bcp, /*show_offset=*/TRUE);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* pointer_offset_for_virtual_base_class */

#endif /* !IA64_ABI */

#if CFRONT_OBJECT_CODE_COMPATIBILITY

/* This is a set of routines that allocate space for pointers to virtual
   base class data sections in cfront compatibility mode.  It is much more
   complicated that what is provided for normal mode because we have had
   to reverse-engineer cfront's algorithm for ordering the pointers. */

static a_boolean is_best_derivation(a_base_class_ptr  bcp,
                                    a_base_class_ptr  derived_bcp,
                                    a_type_ptr        class_type)
/*
Return TRUE if bcp is a direct base class (meaning derived_bcp is NULL) or if
the base class in class_type that corresponds to derived_bcp is on a
derivation path of the base class in class_type that corresponds to bcp.
*/
{
  a_boolean                    is_best_path;
  a_base_class_derivation_ptr  bcdp;
  a_derivation_step_ptr        step, tail;

  if (derived_bcp == NULL) {
#if CHECKING
    if (!bcp->direct) {
      internal_error(
                 "is_best_derivation: no derived_bcp for indirect base class");
    }  /* if */
#endif /* CHECKING */
    is_best_path = TRUE;
  } else {
    bcp = corresponding_base_class(bcp, class_type, (a_base_class_ptr)NULL);
    if (first_derivation_is_direct(bcp)) {
      is_best_path = FALSE;
    } else {
      derived_bcp = corresponding_base_class(derived_bcp, class_type,
                                             (a_base_class_ptr)NULL);
      /* Return TRUE if a step pointing to derived_bcp is on the derivation
         for bcp. */
      bcdp = bcp->derivation;
      tail = bcdp->path_tail;
      is_best_path = FALSE;
      while (!bcdp->direct) {
        for (step = bcdp->path; step != tail->next; step = step->next) {
          if (step->base_class == derived_bcp) {
            is_best_path = TRUE;
            goto done;
          }  /* if */
        }  /* for */
        bcdp = bcdp->path->base_class->derivation;
      }  /* for */
    }  /* if */
  }  /* if */
done:
  return is_best_path;
}  /* is_best_derivation */


static void set_pointer_offsets_for_corresponding_virtual_base_classes(
                                             a_layout_block_ptr lob,
                                             a_base_class_ptr   base_class,
                                             a_boolean          use_decl_order,
                                             a_base_class_ptr   derived_bcp)
/*
base_class is a base class on the base classes list of derived_bcp.  Look
at it and its successors on the list.  For each that is a direct virtual
base class of derived_bcp and that has a NULL pointer base class allocate a
pointer to its data section.  For each base class on the list that is a
direct nonvirtual base class, call this routine recursively to pick up its
own virtual base classes, if any.  If use_decl_order is TRUE, we have a
simple loop, allocating pointers for base classes in the order they appear
on the base classes list; if it is FALSE, a recursive call assures that the
successors on the list, if any, are processed before the predecessor.
*/
{
  a_base_class_ptr  bcp;
  a_type_ptr        tp;

  db_enter(4, "set_pointer_offsets_for_corresponding_virtual_base_classes");
  for (; base_class != NULL; base_class = base_class->next) {
    if (first_derivation_is_direct(base_class)) {
      /* We are only interested in direct base classes. */
      if (!use_decl_order) {
        /* We should use reverse declaration order, so do the successors
           first, then return to the current base class. */
        set_pointer_offsets_for_corresponding_virtual_base_classes(
                                 lob, base_class->next, use_decl_order,
                                 derived_bcp);
      }  /* if */
      /* Virtual and nonvirtual direct base classes are handled differently. */
      if (base_class->is_virtual) {
        /* If the virtual base class needs and does not yet have a pointer
           to its data section, allocate it now and then allocate a pointer
           for any of its own virtual base classes that may need it. */
        bcp = corresponding_base_class(base_class, lob->class_type,
                                       (a_base_class_ptr)NULL);
        if (bcp->pointer_base_class == NULL && !bcp->pointer_offset_is_set &&
            is_best_derivation(bcp, derived_bcp, lob->class_type)) {
          /* Allocate the pointer. */
          pointer_offset_for_virtual_base_class(lob, bcp);
          /* Check its own virtual base classes, reversing the setting of
             use_decl_order as we go down another step in the derivation. */
          tp = bcp->type;
          if (tp->variant.class_struct_union.any_virtual_base_classes) {
            set_pointer_offsets_for_corresponding_virtual_base_classes(
                               lob, base_classes_of(tp), !use_decl_order, bcp);
          }  /* if */
        }  /* if */
      } else {
        /* If the base class has any virtual base classes of its own, allocate
           pointers to their data sections as needed.  Reverse the setting of
           use_decl_order as we go down another step in the derivation. */
        tp = base_class->type;
        if (tp->variant.class_struct_union.any_virtual_base_classes) {
          set_pointer_offsets_for_corresponding_virtual_base_classes(
                        lob, base_classes_of(tp), !use_decl_order, base_class);
        }  /* if */
      }  /* if */
      /* If we've already done the successors, break out of the loop now. */
      if (!use_decl_order) break;
    }  /* if */
  }  /* for */
  db_exit();
}  /* set_pointer_offsets_for_corresponding_virtual_base_classes */


static a_boolean has_virtual_base_class_with_null_pointer_base_class(
                                                  a_base_class_ptr  base_class,
                                                  a_type_ptr        class_type)
/*
Return TRUE if any of the base classes of base_class have a NULL pointer-
base-class field.  (It is the base class entry in the list associated with
class_type that we are interesting in examining.)
*/
{
  a_base_class_ptr  bcp, corresp_bcp;
  a_boolean         has_one = FALSE;

  /* Traverse the list of base classes of base_class. */
  for (bcp = base_classes_of(base_class->type); bcp != NULL; bcp = bcp->next) {
    /* If bcp is a virtual base class, look for the corresponding base class
       among the base classes of class_type. */
    if (bcp->is_virtual) {
      corresp_bcp = corresponding_base_class(bcp, class_type,
                                             (a_base_class_ptr)NULL);
      /* If it has a NULL pointer base class (meaning its pointer is not
         embedded within the body of some other base class) return TRUE.
         Otherwise keep looking. */
      if (corresp_bcp->pointer_base_class == NULL) {
        has_one = TRUE;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return has_one;
}  /* has_virtual_base_class_with_null_pointer_base_class */


/* Forward declaration for set_pointer_offset_for_direct_virtual_base_class --
   mutual recursion with check_direct_virtual_base_classes_for_special_case */
static void set_pointer_offset_for_direct_virtual_base_class(
                                            a_layout_block_ptr lob,
                                            a_base_class_ptr   base_class,
                                            a_boolean          use_decl_order);

static void check_direct_virtual_base_classes_for_special_case(
                                             a_layout_block_ptr lob,
                                             a_base_class_ptr   bcp,
                                             a_boolean          use_decl_order,
                                             a_base_class_ptr   derived_bcp)
/*
bcp is a base class on the base classes list of derived_bcp.  Look at bcp
and its successors on the list.  For each that is a direct virtual base class
of derived_bcp that has a NULL pointer base class *and* has at least one of
its own virtual base classes with a NULL pointer base class (that's the
special case) allocate a pointer to its data section.  If use_decl_order is
TRUE, we have a simple loop, allocating pointers for base classes in the
order they appear on the base classes list; if it is FALSE, a recursive call
assures that the successors on the list, if any, are processed before the
predecessor.
*/
{
  for (; bcp != NULL; bcp = bcp->next) {
    if (bcp->direct && bcp->is_virtual && bcp->pointer_base_class == NULL &&
        has_virtual_base_class_with_null_pointer_base_class(bcp,
                                                            lob->class_type)) {
      if (!use_decl_order) {
        check_direct_virtual_base_classes_for_special_case(lob, bcp->next,
                                                           use_decl_order,
                                                           derived_bcp);
      }  /* if */
      /* Is there another derivation for this virtual base class via a more
         directly reachable intermediate base class?  If so, skip over it now
         -- it will be put out later. */
      if (is_best_derivation(bcp, derived_bcp, lob->class_type)) {
        set_pointer_offset_for_direct_virtual_base_class(lob, bcp,
                                                         use_decl_order);
      }  /* if */
      if (!use_decl_order) break;
    }  /* if */
  }  /* for */
}  /* check_direct_virtual_base_classes_for_special_case */


static void set_pointer_offset_for_direct_virtual_base_class(
                                             a_layout_block_ptr lob,
                                             a_base_class_ptr   base_class,
                                             a_boolean          use_decl_order)
/*
base_class is a direct virtual base class of a base class of the current
class (identified by lob->class_type).  Allocate a pointer to its data
section in the current class, and then check its own direct virtual base
classes, for which pointers may also be needed.  Once all its own direct
virtual base classes have been accounted for, go through all its virtual
base classes, direct and indirect, and allocate pointers as needed for them.
*/
{
  a_base_class_ptr  bcp;
  a_type_ptr        tp;

  /* Find the base class entry on the base classes list of the current class
     that corresponds to base_class. */
  bcp = corresponding_base_class(base_class, lob->class_type,
                                 (a_base_class_ptr)NULL);
  if (!bcp->pointer_offset_is_set) {
    /* Allocate a pointer to its data section.  The offset of the pointer will
       be recorded in *bcp. */
    pointer_offset_for_virtual_base_class(lob, bcp);
    tp = base_class->type; 
    if (tp->variant.class_struct_union.any_virtual_base_classes) {
      /* Next go through the base class's own direct virtual base classes,
         looking for any that require special handling. */
      bcp = base_classes_of(tp);
      check_direct_virtual_base_classes_for_special_case(lob, bcp,
                                                         !use_decl_order,
                                                         base_class);
      /* Finally, make another pass over the base classes list.  All base
         classes for which pointers must be allocated should be taken care
         of now. */
      set_pointer_offsets_for_corresponding_virtual_base_classes(
                                         lob, bcp, use_decl_order, base_class);
    }  /* if */
  }  /* if */
}  /* set_pointer_offset_for_direct_virtual_base_class */

#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */

#if !IA64_ABI

static void set_virtual_base_class_pointer_offsets(a_layout_block_ptr lob)
/*
Set the pointer_offset fields in direct virtual base classes where the pointer
is not shared (i.e., where the pointer from a base class is not used).
*/
{
  a_base_class_ptr   bcp;
  
  db_enter(4, "set_virtual_base_class_pointer_offsets");

  if (lob->class_type->variant.class_struct_union.any_virtual_base_classes) {
    /* Since we are going to allocate pointers, any active empty base can be
       deactivated (i.e., there is no danger that a subsequent subobject will
       be allocated at the same location. */
    bcp = base_classes_of(lob->class_type);
#if CFRONT_OBJECT_CODE_COMPATIBILITY
    /* In cfront compatibility mode we go through the base classes list
       twice.  First we look at direct virtual base classes with a NULL pointer
       base class field and for which at least one of its own base classes
       has a NULL pointer base class field.  These get special treatment and
       are put out in declaration order. */

    for (; bcp != NULL; bcp = bcp->next) {
      if (bcp->is_virtual && first_derivation_is_direct(bcp) &&
          bcp->pointer_base_class == NULL &&
          has_virtual_base_class_with_null_pointer_base_class(
                                                    bcp, lob->class_type)) {
        set_pointer_offset_for_direct_virtual_base_class(
                                          lob, bcp, /*use_decl_order=*/TRUE);
      }  /* if */
    }  /* for */
    /* Next we traverse the base classes list again to look for pointers that
       are still unaccounted for (i.e., have a NULL pointer base class and
       and still have a zero pointer offset).  Direct virtual base classes are
       put out in reverse declaration order, direct virtual base classes of
       direct virtual bases are put out in declaration order, etc.; in other
       words, if the number of levels of derivation is odd, they are put out in
       reverse order; if it is even, they are put out in the same order as they
       were declared.  Thus we use a recursive loop, and each time we go down
       a level we reverse the value of use_decl_order. */
    set_pointer_offsets_for_corresponding_virtual_base_classes(
                          lob, base_classes_of(lob->class_type),
                          /*use_decl_order=*/FALSE, (a_base_class_ptr)NULL);

#else /* i.e., #if !CFRONT_OBJECT_CODE_COMPATIBILITY */
    /* In normal layout mode we traverse the base classes list only once.
       The pointers are put out (when needed -- i.e., when the pointer base
       class is NULL) in base class order. */

    for (; bcp != NULL; bcp = bcp->next) {
      /* For virtual base classes we only reserve enough space for a pointer
         to the actual data section.  The latter is added at the end of the
         storage. */
      /* Only pointers to direct virtual base classes need space reserved --
         and only when the pointer is not shared, i.e., not already present in
         the data section of another base class, as indicated by the
         pointer_base_class field. */
      if (bcp->is_virtual && bcp->pointer_base_class == NULL) {
        pointer_offset_for_virtual_base_class(lob, bcp);
      }  /* if */
    }  /* for */
#endif /* !CFRONT_OBJECT_CODE_COMPATIBILITY */
  }  /* if */
  db_exit();
}  /* set_virtual_base_class_pointer_offsets */

#endif /* !IA64_ABI */

#if CFRONT_OBJECT_CODE_COMPATIBILITY

static void fixup_embedded_virtual_base_classes(a_base_class_ptr base_class,
                                                a_type_ptr       class_type)
/*
base_class is a virtual base class whose data section is being allocated;
base_class will be represented as a "complete subobject" of class_type, which
means space will be reserved for all its own virtual base classes. Therefore,
if it has any virtual base classes with data sections that have not already
been associated with some other base class, record the "official" location
of the latter as its position within base_class.  This is a recursive
algorithm.
*/
{
  a_base_class_ptr  bcp, embedded_base_class;

  db_enter(4, "fixup_embedded_virtual_base_classes");
  if (base_class->type->variant.class_struct_union.any_virtual_base_classes) {
    /* base_class has one or more virtual base classes of its own. */
    for (bcp = base_classes_of(base_class->type);
         bcp != NULL;
         bcp = bcp->next) {
      if (bcp->is_virtual) {
        /* bcp is one of the virtual base class of base_class.  Find the
           base class entry that corresponds to it in the base classes list
           for class_type. */
        embedded_base_class = corresponding_base_class(bcp, class_type,
                                                       (a_base_class_ptr)NULL);
        if (embedded_base_class->data_section_base_class != NULL) {
          /* The data section for this virtual base class has already been
             assigned a location. */
        } else {
          /* Proceed to specify how it should be embedded. */
          if (bcp->data_section_base_class == NULL) {
            /* In the context of base_class, it was not embedded (for instance,
               it may have been a direct virtual base class or a virtual
               base class that was inherited through a direct base class
               represented as an "incomplete subobject").  Therefore it will
               be embedded in the data section reserved for base_class in
               the layout of class_type. */
            embedded_base_class->data_section_base_class = base_class;
          } else {
            /* In the context of base class it was embedded (for instance,
               it may have been inherited through another virtual base class
               or through a "complete subobject" base class).  Use the same
               location in the layout of class_type. */
            embedded_base_class->data_section_base_class =
                  corresponding_base_class(bcp->data_section_base_class,
                                           class_type, (a_base_class_ptr)NULL);
          }  /* if */
          /* Apply the algorithm recursively. */
          fixup_embedded_virtual_base_classes(embedded_base_class, class_type);
        }  /* if */
      }  /* for */
    }  /* for */
  }  /* for */
  db_exit();
}  /* fixup_embedded_virtual_base_classes */


static void set_offsets_for_corresponding_virtual_base_classes(
                                             a_layout_block_ptr lob,
                                             a_base_class_ptr   base_class,
                                             a_boolean          use_decl_order)
/*
Check each direct virtual base class in the list (or partial list) headed by
base_class.  If use_decl_order is true, the qualifying entries are processed
in list order; otherwise, they are processed from back to front -- a recursive
call processes the successors first, then returns to the qualifying entry.
The processing that is done is to allocate space for a virtual base class
that has turned out not be embedded anywhere; once this is done, any virtual
base class it is derived from is marked as embedded in it (if not already so
designated).
*/
{
  a_base_class_ptr             bcp;
  a_base_class_derivation_ptr  bcdp;

  db_enter(4, "set_offsets_for_corresponding_virtual_base_classes");
  for (; base_class != NULL; base_class = base_class->next) {
    if (base_class->is_virtual && base_class->direct &&
        base_class->data_section_base_class == NULL) {
      if (!use_decl_order) {
        set_offsets_for_corresponding_virtual_base_classes(
                                        lob, base_class->next, use_decl_order);
      }  /* if */
      /* base_class may be a base class of another type, not a base class of
         lob->class_type.  Find the corresponding base class of the latter. */
      bcp = corresponding_base_class(base_class, lob->class_type,
                                     (a_base_class_ptr)NULL);
      if (bcp->data_section_base_class == NULL && bcp->offset == 0 &&
          !lob->any_overflow) {
#if CHECKING
        /* All virtual base classes should be marked as "complete
           subobjects". */
        if (!bcp->complete_subobject) {
          internal_error("set_offsets_for_corresp...: not complete subobj");
        }  /* if */
        if (lob->byte_offset == 0 && lob->bit_offset == 0) {
          internal_error("set_offsets_for_corresp...: zero offset");
        }  /* if */
#endif /* CHECKING */
        /* Does this virtual base class have a virtual base class on any its
           own derivation paths?  If so, it is still a candidate for being
           embedded, so don't allocate space for it yet. */
        for (bcdp = bcp->derivation; bcdp != NULL; bcdp = bcdp->next) {
          if (!bcdp->direct && bcdp->path->base_class->is_virtual) {
            break;
          }  /* if */
        }  /* for */
        if (bcdp == NULL) {
          /* No virtual base classes on any of its derivations. */
          bcp->offset = set_offset_and_alignment(lob, bcp->type->size,
                                                 bcp->type->alignment,
                                                 (a_base_class_ptr)NULL);
#if DEBUG
          if (debug_level >= 4) {
            fputs("updated offset for ", f_debug);
            db_base_class(bcp, /*show_offset=*/TRUE);
          }  /* if */
#endif /* DEBUG */
          fixup_embedded_virtual_base_classes(bcp, lob->class_type);
        }  /* if */
      }  /* if */
      if (!use_decl_order) break;
    }  /* if */
  }  /* for */
  db_exit();
}  /* set_offsets_for_corresponding_virtual_base_classes */


static void cfc_set_virtual_base_class_offsets(a_layout_block_ptr lob,
                                               a_base_class_ptr   base_class,
                                               a_boolean        use_decl_order)
/*
lob->class_type is the most-derived-type whose offsets are currently being
specified.  base_class may be NULL, in which case the direct base classes of
lob->class_type are processed; if base_class is non-NULL, the direct base
classes of base_class->type are processed.  A non-NULL base_class is a direct
or indirect nonvirtual base class of lob->class_type with a complete_subobject
flag set to FALSE.  The processing involves going through the appropriate set
of direct base classes (of either lob->class_type or of base_class->type) and
locating virtual base classes that are not embedded anywhere else.  Space in
lob->class_type must be reserved for them.

base_class is a direct or indirect base class of lob->class_type for which
the complete_subobject flag is FALSE and whose virtual base classes, therefore,
may space reserved in the compete derived class.  Examine the direct
virtual base classes of base_class, find the corresponding indirect virtual
base class of class_type, and allocate space for the latter.
*/
{
  a_base_class_ptr  base_class_list, bcp;

  db_enter(4, "cfc_set_virtual_base_class_offsets");
  if (base_class == NULL) {
    /* Go through the base classes of lob->class_type, which is the most
       derived class. */
    base_class_list = base_classes_of(lob->class_type);
    /* Look for virtual bass classes which have already been determined to
       be embedded in another data section.  If such class have virtual base
       classes of their own, record their virtual base classes as embedded
       within them. */
    /* Why is this being done at precisely this point?  Virtual base classes
       that could be embedded in a nonvirtual base class will already have
       been dealt with (in set_data_section_base_class in class_decl.c).  That
       leaves only those whose embedding has been deferred and will be
       embedded, if anywhere, only in another virtual base class.  But if it
       is a base class of two virtual base classes, one of which is already
       embedded and the other of which is not (because it is a direct base
       class, say), should the former have precedence?  For example:
               V1        V1, V2, and V3 are virtual base classes,
              /  \       V2 is embedded in Y (a complete subobject), and
             V2   V3     V3, being a direct base class, is not embedded.
              |   /      Should V1 be embedded in V2 or V3?  The current
           X  Y  /       processing assures that it is embedded in V2-in-Y.
            \ | /        (What does cfront do?  CC3 aborts on this example.)
              D
     One reason for doing it at this point is to assure that virtual base
     classes that are both direct and indirect get embedded if they can be. */
    for (bcp = base_class_list; bcp != NULL; bcp = bcp->next) {
      if (bcp->data_section_base_class != NULL) {
        /* bcp is an embedded virtual base class. */
        fixup_embedded_virtual_base_classes(bcp, lob->class_type);
      }  /* if */
    }  /* for */
  } else {
    /* Go though the base classes of a (direct or indirect) base class of
       lob->class_type. */
    base_class_list = base_classes_of(base_class->type);
  }  /* if */
  if (use_decl_order) {
    /* Examine all the direct virtual base classes in list order. */
    set_offsets_for_corresponding_virtual_base_classes(lob, base_class_list,
                                                       use_decl_order);
  }  /* if */
  for (bcp = base_class_list; bcp != NULL; bcp = bcp->next) {
    if (bcp->direct) {
      /* bcp is a direct base class either of lob->class_type or of
         base_class->type. */
      if (base_class == NULL) {
        /* This is a "top-level" base class list -- i.e., a direct base
           of lob->class_type. */
        if (!bcp->complete_subobject) {
          /* bcp is not a complete subobject.  That means it must be the
             first non-virtual direct base class in the list. There will not
             be any others, so we break after processing it. */
          cfc_set_virtual_base_class_offsets(lob, bcp, !use_decl_order);
          break;
        }  /* if */
      } else {
        /* This is a direct base class of base_class->type but an indirect
           base class of lob->class_type.  More important, it is a base
           class of an incomplete subobject, which means virtual base
           classes from which it is derived may not have been embedded,
           whether bcp is itself incomplete or not. */
        cfc_set_virtual_base_class_offsets(lob, bcp, !use_decl_order);
      }  /* if */
    }  /* if */
  }  /* for */
  if (!use_decl_order) {
    /* Examine all the direct virtual base classes in the opposite of
       list order. */
    set_offsets_for_corresponding_virtual_base_classes(lob, base_class_list,
                                                       use_decl_order);
  }  /* if */
  db_exit();
}  /* cfc_set_virtual_base_class_offsets */

#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */

#if IA64_ABI

static a_boolean trailing_base_does_not_affect_gnu_size(a_base_class_ptr  bcp)
/*
Normally, an empty base class appearing "off the end" of a class forces the
size of that class to be enlarged to avoid conflicts between adjacent objects
of that class type.  Some GNU C++ compilers ignore that requirement for
certain bases.  Return TRUE if the given empty base class is a base class
that would trigger this GNU bug.
*/
{
  a_boolean  result = FALSE;

  check_assertion(emulate_gnu_abi_bugs && is_empty_class_type(bcp->type));
  if (any_virtual_steps_in_derivation(bcp)) {
    /* If the class is a base class of a virtual base on some derivation
       path, it will not affect the GNU size.  This is certainly the case
       for indirect base classes with virtual steps in their derivation. */
    if (!bcp->direct) {
      result = TRUE;
    } else if (bcp->derivation != NULL && bcp->derivation->next != NULL &&
               get_gnu_first_field(bcp->derived_class) != NULL) {
      /* A virtual base that appears multiple times in the inheritance graph
         (once as a direct base).  Look for a derivation on which it is also
         a virtual base class of another virtual base. */
      a_base_class_derivation_ptr  dp = bcp->derivation;
      check_assertion(bcp->is_virtual);
      for (; dp != NULL; dp = dp->next) {
        if (!dp->direct && dp->path->base_class->is_virtual) {
          result = TRUE;
          break;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  return result;
}  /* trailing_base_does_not_affect_gnu_size */


static void adjust_size_for_empty_bases(a_layout_block_ptr lob)
/*
There may be empty base classes that are located "off the end" of the
class. Update lob->byte_offset to reflect the real end of the class.
Note that GNU compilers do not perform this adjustment if the trailing
empty base class is virtual and indirect.
*/
{
  a_base_class_ptr bcp;

  for (bcp = base_classes_of(lob->class_type); bcp != NULL; bcp = bcp->next) {
    if (is_empty_class_type(bcp->type) && 
        bcp->offset + bcp->type->size > lob->byte_offset &&
        /* Under some fairly strange circumstances, some GNU C++ compilers
           will not perform this adjustment.  The conditions for this bug
           include the requirements that the base class be on a virtual
           derivation path and that the class be indirect.  If the base
           class is both direct and indirect the derived class must have no
           significant fields for the bug to manifest itself. */
        !(emulate_gnu_abi_bugs &&
          trailing_base_does_not_affect_gnu_size(bcp))) {
      lob->byte_offset = bcp->offset + bcp->type->size;
      lob->bit_offset = 0;
    }  /* if */
  }  /* for */
}  /* adjust_size_for_empty_bases */

#endif /* IA64_ABI */
#if !CFRONT_OBJECT_CODE_COMPATIBILITY

static void set_virtual_base_class_offset(a_layout_block_ptr lob,
                                          a_base_class_ptr   bcp)
/*
Set bcp->offset.  The base class bcp must be a virtual base.
*/
{
  a_targ_size_t      size;
  a_targ_alignment   alignment;

  check_assertion(bcp->is_virtual);
  /* Record the current offset in the data_section_offset of the
     virtual base class entry.  This allows for direct access of
     its fields (rather than through a pointer) as an optimization
     under certain circumstances. */
#if IA64_ABI
  if (targ_optimize_empty_base_class_layout && 
      is_empty_class_type(bcp->type)) {
    allocate_empty_base(lob, bcp);
  } else 
#endif /* IA64_ABI */
  /* Do not add code here. */
  {
    a_class_type_supplement_ptr  base_ctsp = class_type_supp(bcp->type);
    size = base_ctsp->size_without_virtual_base_classes;
    alignment = base_ctsp->alignment_without_virtual_base_classes;
    if (packing_applies_to_base_classes) {
      adjust_alignment_for_packing(&alignment, bcp->derived_class);
    }  /* if */
    bcp->offset = set_offset_and_alignment(lob, size, alignment, bcp);
  }  /* if */
#if IA64_ABI
  { a_targ_size_t  end = bcp->offset
                       + class_type_supp(bcp->type)
                                          ->size_without_virtual_base_classes;
    lob->curr_extent = max_val(lob->curr_extent, end-1);
  }
  if (warn_about_tail_padding_use) {
    /* Examine if this base class was allocated in the tail padding of
       another base. */
    warn_if_offset_in_tail_padding((a_field_ptr)NULL, bcp, lob);
  }  /* if */
  /* Set the offsets for all of the non-virtual bases of this base. */
  set_base_class_offsets(lob, bcp);
#endif /* IA64_ABI */
#if DEBUG
  if (debug_level >= 4) {
    fputs("updated offset for ", f_debug);
    db_base_class(bcp, /*show_offset=*/TRUE);
  }  /* if */
#endif /* DEBUG */
}  /* set_virtual_base_class_offset */

#endif /* !CFRONT_OBJECT_CODE_COMPATIBILITY */

static void set_virtual_base_class_offsets(a_layout_block_ptr  lob)
/*
Reserve space at the end of the class object for virtual base classes.
*/
{
  a_class_type_supplement_ptr	ctsp;
  an_unnormalized_bit_offset	zero = 0;
  
  db_enter(4, "set_virtual_base_class_offsets");

  if (lob->class_type->variant.class_struct_union.any_virtual_base_classes) {
    ctsp = lob->class_type->variant.class_struct_union.extra_info;
    /* Record the size and alignment of the class before space is added for
       virtual base classes. */
    pad_bit_field(lob);
#if IA64_ABI
    if (lob->curr_base_extent >= lob->byte_offset) {
      /* The "size without virtual bases" may include some padding (e.g., when
         the last nonvirtual base is empty and the class has no fields of its
         own).  This padding is normally never reused when the class is used
         as a subobject type (except when emulating GNU ABI bugs).
         Note that the quantity "dsize(C)" used in the IA-64 ABI specification
         is equivalent to lob->byte_offset.  Similarly, the current value of
         "sizeof(C)" as used in the ABI specification equals
         lob->curr_base_extent+1 when the latter expression is larger than
         lob->byte_offset. */
      if (emulate_gnu_abi_bugs && targ_reuse_tail_padding) {
        /* GNU reuses the tail padding of nonvirtual base classes. */
        ctsp->size_without_virtual_base_classes = lob->byte_offset;
        lob->byte_offset = lob->curr_base_extent + 1;
      } else {
        ctsp->size_without_virtual_base_classes = lob->curr_base_extent + 1;
      }  /* if */
    } else
#endif /* IA64_ABI */
    /* Do not insert code here. */
    {
      ctsp->size_without_virtual_base_classes = lob->byte_offset;
    }  /* if */
    ctsp->alignment_without_virtual_base_classes = lob->alignment;
#if IA64_ABI
    if (emulate_gnu_abi_bugs && gnu_abi_version < 30400) {
      /* Early GNU implementations for the IA-64 ABI force an alignment
         boundary before allocating trailing virtual bases. */
      if (!do_alignment(&lob->byte_offset, &lob->bit_offset, lob->alignment) &&
          !lob->any_overflow) {
        pos_error(struct_too_large_error(), &error_position);
        lob->any_overflow = TRUE;
      }  /* if */
    }  /* if */
    if (targ_reuse_tail_padding) {
      /* This is an IA-64 ABI configuration that allowed tail padding of base
         classes to be reused for other subobjects. */
      if (warn_about_tail_padding_use) {
        a_targ_size_t size = ctsp->size_without_virtual_base_classes;
        if (do_alignment(&size, &zero,
                         ctsp->alignment_without_virtual_base_classes)) {
          if (size != ctsp->size_without_virtual_base_classes) {
            pos_warning(ec_size_affected_by_tail_padding,
                        &lob->class_type->source_corresp.decl_position);
          }  /* if */
        }  /* if */
      }  /* if */
    } else
#endif /* IA64_ABI */
    /* Do not insert code here. */
    {
      /* Do not reuse tail padding. */
      /* Note that the current size may not be consistent (according to the
         rules for C structs) with the current alignment.  Modify
         size-without-virtual-base-classes in such a case, but without changing
         lob->byte_offset (i.e., without introducing unwanted padding in the
         current class before the data sections for the virtual base classes
         are put out.  This assures that size-without-virtual-base-classes will
         correspond to the actual size of an incomplete subobject. */
      if (!do_alignment(&ctsp->size_without_virtual_base_classes, &zero,
                        ctsp->alignment_without_virtual_base_classes)) {
        if (!lob->any_overflow) {
          pos_error(struct_too_large_error(), &error_position);
          lob->any_overflow = TRUE;
        }  /* if */
      } else {
        /* Propagate the rounded size without virtual base classes to the
           layout state block (since that is what will be used to track the
           layout of virtual bases). */
        lob->byte_offset = ctsp->size_without_virtual_base_classes;
      }  /* if */
    }  /* if */
    /* Now see if there are any virtual base class data sections that need to
       be added to the layout for the current class. */
#if CFRONT_OBJECT_CODE_COMPATIBILITY
    /* In cfront compatibility layout mode all virtual base classes that
       are not embedded in another class have space reserved for them.  The
       order in which cfront puts them out is emulated. */
    cfc_set_virtual_base_class_offsets(lob, (a_base_class_ptr)NULL,
                                       /*use_decl_order=*/FALSE);
#else /* !CFRONT_OBJECT_CODE_COMPATIBILITY */
    /* In normal layout mode all virtual base classes have space reserved at
       this point in the layout.  The order in which they are put out is
       the order of their appearance in the base classes list, which
       corresponds to a depth-first left-to-right traversal of the base
       classes represented in a directed acyclic graph (see ARM 12.6.2). */
    if (ctsp->base_classes != NULL) {
      /* Now add the virtual base classes to the storage.  This is done
         almost exactly as for nonvirtual base classes. */
      a_base_class_ptr   bcp;

      for (bcp = 
#if !IA64_ABI
                 ctsp->base_classes; 
#else /* IA64_ABI */
                 ctsp->preorder_base_classes;
#endif /* IA64_ABI */
           bcp != NULL; 
           bcp = 
#if !IA64_ABI
                 bcp->next
#else /* IA64_ABI */
                 bcp->next_preorder
#endif /* IA64_ABI */
                                   ) {
        if (bcp->is_virtual
#if IA64_ABI
            /* If the virtual base is primary, it will be allocated as part of
               some other base. */
            && !bcp->shares_virtual_function_info &&
            /* Skip virtual bases that have already been processed. */
            !bcp->offset_is_set
#endif /* IA64_ABI */
                                                    ) {
          set_virtual_base_class_offset(lob, bcp);
        }  /* if */
      }  /* for */
    }  /* if */
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
#if IA64_ABI
    if (emulate_gnu_abi_bugs) {
      if (gnu_abi_version >= 30300 && gnu_abi_version < 30400) {
        /* GNU C++ version 3.3 sometimes "overpads" a class whose last base
           ends with a bit field. */
        emulate_gnu_bit_field_overpadding(lob, /*virtual_base=*/TRUE);
      }  /* if */
    }  /* if */
#endif /* IA64_ABI */
  }  /* if */
  db_exit();
}  /* set_virtual_base_class_offsets */

#if !IA64_ABI

static void set_offsets_for_indirect_base_classes(a_layout_block  *lob)
/*
Compute the offsets from the start of the object described by lob->class_type
of each of its indirect base classes.  The direct base classes and the data
sections of all virtual base classes, direct or indirect, have already been
handled.  The processing of this routine and its subroutines is addressed to
indirect base classes.  lob points to a block of information that describes
some current state about the layout of lob->class_type.
*/
{
  a_type_ptr        class_type = lob->class_type;
  a_base_class_ptr  bcp = base_classes_of(class_type);

  db_enter(4, "set_offsets_for_indirect_base_classes");
#if DEBUG
  if (debug_level >= 4) {
    if (bcp != NULL) {
      fputs("before setting offsets: ", f_debug);
      db_base_class_list(class_type);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  /* Loop through the list of base classes, direct and indirect, that are
     defined for the class, but ignore all but the direct base classes.  The
     rest are handled by recursively scanning the base class tree. */
  for (; bcp != NULL; bcp = bcp->next) {
    if (first_derivation_is_direct(bcp)) {
      set_base_class_offsets(lob, bcp);
    }  /* if */
  }  /* for */
  db_exit();
}  /* set_offsets_for_indirect_base_classes */

#endif /* !IA64_ABI */

#if CFRONT_OBJECT_CODE_COMPATIBILITY

static void set_embedded_virtual_base_class_offset(a_layout_block   *lob,
                                                   a_base_class_ptr base_class)
/*
base_class is a direct or indirect virtual base class of lob->class_type (which
is also base_class->derived_class).  If it is allocated inside another base
class, compute its offset within the layout of its class.  Then do the same
check for its own direct virtual base classes.  lob points to a block of
information that describes some current state about the layout of
lob->class_type.
*/
{
  a_base_class_ptr  data_section_bcp, bcp, curr_class_bcp;

  db_enter(4, "set_embedded_virtual_base_class_offset");
  if (base_class->offset == 0) {
    /* Offset has not yet been set. */
#if DEBUG
    if (debug_level >= 4) {
      db_base_class(base_class, /*show_offset=*/TRUE);
    }  /* if */
#endif /* DEBUG */
    data_section_bcp = base_class->data_section_base_class;
    if (data_section_bcp != NULL) {
      if (data_section_bcp->is_virtual &&
          data_section_bcp->data_section_base_class != NULL &&
          data_section_bcp->offset == 0) {
        set_embedded_virtual_base_class_offset(lob, data_section_bcp);
      }  /* if */
      /* Look for the corresponding virtual base class. */
      bcp = corresponding_base_class(base_class, data_section_bcp->type,
                                     (a_base_class_ptr)NULL);
      /* The pointer_offset value in the context of the derived class
         is the offset of the pointer base class plus the offset of the
         virtual base class pointer within the latter. */
      base_class->offset = bcp->offset + data_section_bcp->offset;
      /* Update the offsets of nonvirtual base classes from which base_class is
         derived. */
      set_base_class_offsets(lob, base_class);
      /* Apply the check recursively to see if there are any indirect virtual
         base classes of class_type that have not been properly assigned an
         offset yet. */
      bcp = base_classes_of(base_class->type);
      for (; bcp != NULL; bcp = bcp->next) {
        if (bcp->is_virtual && bcp->direct) {
          curr_class_bcp = corresponding_base_class(bcp,
                                                    base_class->derived_class,
                                                    (a_base_class_ptr)NULL);
          if (curr_class_bcp->data_section_base_class == NULL) {
            /* curr_class_bcp is an indirect virtual base class of class type
               that is not yet marked as embedded. */
            curr_class_bcp->data_section_base_class = base_class;
          }  /* if */
          set_embedded_virtual_base_class_offset(lob, curr_class_bcp);
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  db_exit();
}  /* set_embedded_virtual_base_class_offset */

#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */

#if !IA64_ABI

static void fixup_shared_virtual_base_class_offsets(a_layout_block  *lob)
/*
Set the pointer_offset fields in direct virtual base classes of lob->class_type
where the virtual base class pointer is shared with some other base class.
lob points to a block of information that describes some current state about
the layout of lob->class_type.
*/
{
  a_type_ptr        class_type = lob->class_type;
  a_base_class_ptr  virtual_base_class;
  a_base_class_ptr  pointer_base_class;
  a_base_class_ptr  bcp;

  db_enter(4, "fixup_shared_virtual_base_class_offsets");
  /* Make a pass over all the base classes for the current derived class and
     check each virtual base class. */
  for (virtual_base_class = base_classes_of(class_type);
       virtual_base_class != NULL;
       virtual_base_class = virtual_base_class->next) {
    if (virtual_base_class->is_virtual) {
      /* If the pointer_base_class field is non-NULL, the virtual base class
         pointer for the derived class is the same as the pointer to the
         corresponding virtual base class for pointer_base_class. */
      pointer_base_class = virtual_base_class->pointer_base_class;
      if (pointer_base_class != NULL) {
        /* Look for the corresponding virtual base class. */
        bcp = corresponding_base_class(virtual_base_class,
                                       pointer_base_class->type,
                                       (a_base_class_ptr)NULL);
        /* The pointer_offset value in the context of the derived class
           is the offset of the pointer base class plus the offset of the
           virtual base class pointer within the latter. */
        virtual_base_class->pointer_offset = bcp->pointer_offset +
                                                  pointer_base_class->offset;
      }  /* if */
#if CFRONT_OBJECT_CODE_COMPATIBILITY
      set_embedded_virtual_base_class_offset(lob, virtual_base_class);
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
    }  /* if */
  }  /* for */
  db_exit();
}  /* fixup_shared_virtual_base_class_offsets */

#endif /* !IA64_ABI */

static void check_base_class_offsets(a_layout_block *lob)
/*
Issue an error if any base class offset exceeds the maximum that is allowed.
*/
{
  a_base_class_ptr  bcp;

  if (lob->byte_offset <= targ_max_base_class_offset) {
    /* The entire class is within the limit, so no base class offset can
       exceed it. */
  } else if (lob->any_overflow) {
    /* An error has already been issued for the class.  Don't bother with
       another (possibly redundant) error. */
  } else {
    /* Check the base classes. */
    bcp = base_classes_of(lob->class_type);
    for (; bcp != NULL; bcp = bcp->next) {
      if (bcp->offset > targ_max_base_class_offset) {
        /* Issue an error only on the first base class that exceeds the
           limit. */
        pos_sy2_error(ec_base_class_offset_too_large, &bcp->decl_position,
                     (a_symbol_ptr)bcp->type->source_corresp.assoc_info,
                     (a_symbol_ptr)lob->class_type->source_corresp.assoc_info);
        break;
      }  /* if */
    }  /* for */
  }  /* if */
}  /* check_base_class_offsets */


static void compute_empty_class_bit(a_type_ptr  type)
/*
Determine whether the given class type is empty---i.e., has no nonstatic
data members, virtual functions, virtual base classes or base classes with
such things---and record the outcome in the type.  In C mode, this normally
reduces to structs with no fields.  Note that GNU C mode also has an empty
class concept: struct or unions with no fields or with no fields of nonzero
size (such classes actually have size zero).
*/
{
  a_boolean        result = TRUE;
  a_base_class_ptr bcp;
  a_field_ptr     field = type->variant.class_struct_union.field_list;

  if (C_mode()) {
    if (!gcc_mode) {
      /* Simply return whether the struct has any fields. */
      result = (field == NULL);
#if GNU_EXTENSIONS_ALLOWED
    } else {
      /* In GNU C mode, structs and unions can be "empty" either because they
         have no fields, or because all their fields have size zero.  Such
         class types have size zero (without being incomplete). */
      for (; field != NULL; field = field->next) {
        if (skip_typerefs(field->type)->size != 0 &&
            (!field->is_bit_field || field->bit_size != 0)) {
          result = FALSE;
          break;
        }  /* if */
      }  /* for */
#endif /* GNU_EXTENSIONS_ALLOWED */
    }  /* if */
  } else {
    /* C++ mode. */
#if IA64_ABI
    /* In the IA64 ABI, a zero-width bit field does not make a class 
       non-empty. */
    for (; field != NULL; field = field->next) {
      if (is_empty_field_for_layout_purposes(field)) {
        /* Fields that are empty for layout purposes do not disqualify a type
           from being considered "empty".  Note that is_optimized_empty_class
           is not set yet. */
        continue;
      }  /* if */
#if GNU_EXTENSIONS_ALLOWED
      if (gpp_mode) {
        /* Zero-length array fields do not make a GNU C++ class non-empty. */
        a_type_ptr  field_type = skip_typerefs(field->type);
        if (field_type->kind == (a_type_kind)tk_array &&
            field_type->variant.array.bound_is_zero) {
          continue;
        }  /* if */
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
      if (!field->is_bit_field || field->bit_size != 0) {
        result = FALSE;
        break;
      }  /* if */
    }  /* for */
#else /* !IA64_ABI */
    /* In the Cfront-like ABI, any field makes the class non-empty. */
    if (field != NULL) {
      result = FALSE;
    }  /* if */
#endif /* IA64_ABI */
    if (!result) {
      /* The result is already fully determined. */
    } else if (type->variant.class_struct_union.any_virtual_base_classes ||
               type->variant.class_struct_union.any_virtual_functions) {
      result = FALSE;
    } else {
      /* So far, the class appears to be empty (result is still TRUE).
         Also check that every base class is similarly empty: */
      for (bcp = base_classes_of(type); bcp != NULL; bcp = bcp->next) {
        if (!bcp->type->variant.class_struct_union.is_empty_class) {
          result = FALSE;
          break;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  type->variant.class_struct_union.is_empty_class = result;
#if IA64_ABI
  if (type->source_corresp.assoc_info == NULL) {
    /* Lowering creates class types without associated symbol, but they
       should not have an empty subobject. */
    check_assertion(type->variant.class_struct_union.field_list != NULL);
  } else {
  /* Compute the "has_empty_class_subobject" flag. */
    a_class_symbol_supplement_ptr  cssp = symbol_supplement_for_class(type);
    check_assertion(cssp != NULL);
    if (result) {
      /* Record that this type has a (nonproper) empty subobject. */
      cssp->has_empty_class_subobject = TRUE;
    } else {
      /* Examine base classes and fields. */
      if (!C_mode()) {
        for (bcp = base_classes_of(type); bcp != NULL; bcp = bcp->next) {
          if (bcp->direct && symbol_supplement_for_class(bcp->type)
                                                ->has_empty_class_subobject) {
            cssp->has_empty_class_subobject = TRUE;
            break;
          }  /* if */
        }  /* for */
      }  /* if */
      if (!cssp->has_empty_class_subobject) {
        /* No empty class subobject was found among the base classes.
           Look among the fields. */
        field = type->variant.class_struct_union.field_list;
        for (; field != NULL; field = field->next) {
          a_type_ptr  field_type = skip_typerefs(field->type);
          if (is_array_type(field_type)) {
            field_type 
                 = f_skip_typerefs(underlying_array_element_type(field_type));
          }  /* if */
          if (is_class_struct_union_type(field_type) &&
              symbol_supplement_for_class(field_type)
                                                ->has_empty_class_subobject) {
            cssp->has_empty_class_subobject = TRUE;
            break;
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* IA64_ABI */
}  /* compute_empty_class_bit */

#if IA64_ABI

static void compute_primary_base_classes(a_layout_block  *lob)
/*
Set the primary_base_class for the bases of lob->class_type.  The primary base
class of class_type was already determined during class scanning (see
set_virtual_function_info_base_class); this code propagates that decision 
into the base classes of class_type.  Update *lob for the extent of the primary
base class.
*/
{
  a_type_ptr                   class_type = lob->class_type;
  a_class_type_supplement_ptr  ctsp = class_type_supp(class_type);
  a_base_class_ptr             bcp, primary = ctsp->primary_base_class;

  if (primary != NULL) {
    /* The offset is already set to zero.  Indicate that that is not the
       default value but the actual offset. */
    primary->offset_is_set = TRUE;
    lob->curr_extent += class_type_supp(primary->type)
                                          ->size_without_virtual_base_classes;
  }  /* if */
  for (bcp = preorder_base_classes_of(class_type);
       bcp != NULL;
       bcp = bcp->next_preorder) {
    primary = nominal_primary_base(bcp);
    if (primary != NULL && !primary->offset_is_set) {
      bcp->primary_base_class = primary;
      primary->offset_is_set = TRUE;
    }  /* if */
  }  /* for */
  /* Reset the offset_is_set flag in the base classes. */
  for (bcp = base_classes_of(class_type); bcp != NULL; bcp = bcp->next) {
    bcp->offset_is_set = FALSE;
  }  /* for */
}  /* compute_primary_base_classes */

#endif /* IA64_ABI */

static a_boolean gnu_zero_sized_class_type(a_type_ptr  type)
/*
The given class must be a class type whose size as determined by the layout
algorithm so far is zero.  Return TRUE if the given class type has no base
classes and if all its fields are either zero-length arrays or zero-sized
class types; return FALSE otherwise.  (If TRUE is returned, the class type's
size will remain zero; otherwise it will require padding.)
*/
{
  a_boolean    result = TRUE;
  a_field_ptr  fp;

  check_assertion(is_immediate_class_type(type) && type->size == 0);
  fp = type->variant.class_struct_union.field_list;
  if (fp == NULL) {
    /* At least one zero-sized field must be present. */
    result = FALSE;
  } else {
    for (; fp != NULL; fp = fp->next) {
      if (skip_typerefs(fp->type)->size == 0 &&
          (is_array_type(fp->type) || is_class_struct_union_type(fp->type))) {
        /* A zero-length array member or a zero-sized class type member allow
           for the parent class to have size zero. */
      } else {
        /* Any other field type: The parent class cannot have size zero. */
        result = FALSE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  if (result && type->variant.class_struct_union.extra_info != NULL &&
      base_classes_of(type) != NULL) {
    /* If the class has base classes, return FALSE. */
    result = FALSE;
  }  /* if */
  return result;
}  /* gnu_zero_sized_class_type */


static void check_explicit_alignment(a_type_ptr          class_type,
                                     a_targ_alignment    alignment,
                                     a_layout_block_ptr  lob)
/*
The given class_type has the given explicitly specified alignment.  If this is
a reduction of alignment compared to the "natural" alignment recorded in *lob,
issue a diagnostic if such a reduction is invalid or ignored.
*/
{
  if (class_type->alignment_set_explicitly) {
    /* GNU allows the alignment to be increased.  If the class has the
       "packed" attribute its alignment can also be decreased; otherwise,
       a reduction in alignment is ignored.  Microsoft ignores a decrease in
       alignment for standard class types, but not for managed class types.
       The standard attribute does not allow a decrease in alignment. */
    if (alignment < lob->alignment) {
      /* Find the attribute that results in the indicated alignment. */
      an_attribute_ptr  ap = class_type->source_corresp.attributes;
      for (; ap != NULL; ap = ap->next) {
        if (ap->kind == ak_align) {
          an_attribute_arg_ptr  aap = ap->arguments;
          if (aap->kind == (an_attribute_arg_kind)aak_constant) {
            a_boolean  ovflo;
            if (value_of_integer_constant(aap->variant.constant, &ovflo) ==
                                                                  alignment) {
              break;
            }  /* if */
            check_assertion(!ovflo);
          } else {
            check_assertion(aap->kind == (an_attribute_arg_kind)aak_type);
            if (alignment_of_type(aap->variant.type) == alignment) break;
          }  /* if */
        }  /* if */
      }  /* for */
      /* If ap is NULL, the reduction might be the result of a pragma. */
      if (ap != NULL && is_std_attribute(ap) && !(gnu_mode && !clang_mode)) {
        pos_error(ec_invalid_alignment_reducing_attr, &ap->position);
        alignment = lob->alignment;
      } else if (gnu_mode || sun_mode ||
                 (ms_extensions && ap != NULL &&
                  /*lint -e(506)*/!is_immediate_managed_class_type(
                                                                class_type))) {
        a_boolean  is_packed = FALSE;
#if GNU_EXTENSIONS_ALLOWED
        is_packed = class_type->variant.class_struct_union.is_packed;
#endif /* GNU_EXTENSIONS_ALLOWED */
        if (!is_packed) {
          pos_warning(gnu_mode && !ms_extensions ?
                               ec_alignment_reduction_ignored
                             : ec_alignment_reduction_unconditionally_ignored,
                      ap != NULL ? &ap->position
                                 : &class_type->source_corresp.decl_position);
          alignment = lob->alignment;
        }  /* if */
      }  /* if */
    }  /* if */
    lob->alignment = alignment;
  }  /* if */
}  /* check_explicit_alignment */


void do_class_layout(a_type_ptr  class_type)
/*
Allocate the subobjects defined for class_type -- its nonvirtual and
virtual base classes, its nonstatic data members, and various pointers
for handling virtual bases and functions.
*/
{
  a_layout_block    lob;
  a_targ_alignment  alignment = 0;
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_targ_size_t     end_of_fields_byte_offset;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if IA64_ABI
  a_boolean         is_POD = FALSE;
  a_boolean         zero_size_adjusted = FALSE;
#endif /* IA64_ABI */

  db_enter(3, "do_class_layout");
  if (class_type->variant.class_struct_union.is_prototype_instantiation ||
      class_type->variant.class_struct_union.
                                            is_ms_instantiated_nonreal_class) {
    /* In general, no meaningful layout can be computed for prototype
       instantiations or Microsoft mode instantiated nonreal classes.  We
       just make sure that it has a nonzero size. */
    goto set_size_for_complete_object;
  }  /* if */
#if DEBUG
  if (db_trace("dump_layout", class_type, iek_type)) {
    fprintf(f_debug, "Computing layout for ");
    db_abbreviated_type(class_type);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  if (class_type->alignment_set_explicitly) {
    /* Save the desired alignment and compute the alignment normally.
       Later, we will adjust the computed alignment, if necessary. */
    alignment = class_type->alignment;
    class_type->alignment = 1;
  }  /* if */
  clear_layout_block(&lob, class_type);
  compute_empty_class_bit(class_type);
  if (C_dialect == C_dialect_cplusplus) {
#if IA64_ABI
    a_base_class_ptr            bcp;
    a_class_type_supplement_ptr ctsp;
    /* Identify all of the primary base classes. */
    compute_primary_base_classes(&lob);
    ctsp = class_type->variant.class_struct_union.extra_info;
    bcp = ctsp->primary_base_class;
    if (bcp != NULL) {
      /* If class_type shares virtual function table information with a base 
         class, that base class comes first. */
      if (bcp->is_virtual) {
        set_virtual_base_class_offset(&lob, bcp);
      } else {
        set_offset_for_nonvirtual_base_class(&lob, bcp);
      }  /* if */
      update_curr_base_extent(&lob, bcp);
      check_base_for_flex_array(&lob, bcp);
    } else {
      /* If there is virtual function info, it comes first. */
      set_offset_for_virtual_function_info(&lob);
    }  /* if */
#endif /* IA64_ABI */
    /* Reserve space in the current class for its nonvirtual base classes,
       which are located at the start of the object.  (Virtual base classes
       appear at the end.) */
    set_offsets_for_nonvirtual_base_classes(&lob);
#if !IA64_ABI
    if (targ_optimize_empty_base_class_layout) {
      set_offsets_for_empty_nonvirtual_base_classes(&lob);
    }  /* if */
#endif /* !IA64_ABI */
  }  /* if */
  /* Set offsets for nonstatic data members (fields) declared for the current
     class. */
  set_offsets_for_fields(&lob);
  if (C_dialect == C_dialect_cplusplus) {
#if !IA64_ABI
    /* After the nonstatic data members allocate space for the virtual
       function info block (typically a pointer to the virtual function
       table. */
    set_offset_for_virtual_function_info(&lob);
    /* Next allocate space for pointers to the virtual base class data
       sections. */
    set_virtual_base_class_pointer_offsets(&lob);
#else /* IA64_ABI */
    if (emulate_gnu_abi_bugs) {
      /* After laying out fields, but before laying out virtual bases,
         GNU compilers may force extra padding to avoid ending with
         an empty base. */
      adjust_size_for_empty_bases(&lob);
    }  /* if */
#endif /* !IA64_ABI */
    /* Finally, allocate space for the virtual base class data sections
       themselves. */
    set_virtual_base_class_offsets(&lob);
#if !IA64_ABI
    /* Now verify if the last empty base really does overlap with a field,
       a virtual function info block or a pointer to a virtual base. */
    if (targ_optimize_empty_base_class_layout) {
      check_if_last_empty_base_is_optimized(&lob);
    }  /* if */
#endif /* !IA64_ABI */
  }  /* if */
  check_explicit_alignment(class_type, alignment, &lob);
#if IA64_ABI
  if (C_dialect == C_dialect_cplusplus) {
    /* If there are empty bases "off the end" of the class, update the class
       size now. */
    adjust_size_for_empty_bases(&lob);
  }  /* if */
  if (targ_reuse_tail_padding) {
    pad_bit_field(&lob);
    /* If this class is a POD, tail-padding cannot be reused. */
    is_POD = (class_type->source_corresp.assoc_info != NULL &&
              symbol_supplement_for_class(class_type)->is_cpp03_POD);
    /* If the class has virtual base classes, the size and alignment without
       virtual base classes will already have been recorded; otherwise, record
       it now. */
    if (!C_mode() &&
        !class_type->variant.class_struct_union.any_virtual_base_classes &&
        !is_POD) {
      a_class_type_supplement_ptr  ctsp = class_type_supp(class_type);
      ctsp->size_without_virtual_base_classes = lob.byte_offset;
      ctsp->alignment_without_virtual_base_classes = lob.alignment;
    }  /* if */
  }  /* if */
  /* "Finalize" the size of the class (if it contained any potentially-
     overlapping data members). */
  if (lob.min_final_class_size != 0 &&
      lob.byte_offset < lob.min_final_class_size) {
    lob.byte_offset = lob.min_final_class_size;
    lob.bit_offset = 0;
  }  /* if */
#endif /* IA64_ABI */
#if MICROSOFT_EXTENSIONS_ALLOWED
  end_of_fields_byte_offset = lob.byte_offset;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Adjust the total size of the class to be consistent with the
     overall alignment required for the class. */
  if (!do_alignment(&lob.byte_offset, &lob.bit_offset, lob.alignment)) {
    if (!lob.any_overflow) {
      pos_error(struct_too_large_error(), &error_position);
      lob.any_overflow = TRUE;
    }  /* if */
  }  /* if */
  if (C_dialect == C_dialect_cplusplus) {
#if !IA64_ABI
    /* Go through all the indirect base classes and compute their
       offsets within the current derived class. */
    set_offsets_for_indirect_base_classes(&lob);
    /* Similarly go through all the virtual base classes and do any required
       fixup on their pointer offsets and (in cfront compatibility mode)
       their data section offsets. */
    fixup_shared_virtual_base_class_offsets(&lob);
#endif /* !IA64_ABI */
    /* Issue a diagnostic if the offset assigned to any base class is too
       large. */
    check_base_class_offsets(&lob);
#if IA64_ABI
    /* Limit the emulation of a strange GNU ABI bug to relatively safe
       cases. */
    if (emulate_gnu_abi_bugs) {
      reposition_gnu_disconnected_virtual_bases(&lob);
    }  /* if */
#endif /* IA64_ABI */
  }  /* if */
  /* Record the overall size and alignment in the class's type entry. */
  class_type->size = lob.byte_offset;
  class_type->alignment = lob.alignment;
  /* Avoid a zero-sized structure (as in "struct {int : 0;}" for C and in
     "class {}" for C++).  In GNU C mode, zero-sized structures are
     possible. */
  if (class_type->size == 0) {
#if MICROSOFT_EXTENSIONS_ALLOWED
    a_field_ptr  fp;
    if (microsoft_mode && C_mode() &&
        (fp = class_type->variant.class_struct_union.field_list) != NULL &&
        !fp->is_bit_field && !is_error_type(fp->type)) {
#if CHECKING
      check_assertion_str2((fp->next == NULL || is_union_type(class_type)) &&
                           is_array_type(fp->type) &&
                           (is_incomplete_type(fp->type) ||
                            has_any_zero_bound(fp->type)),
                           "do_class_layout: unexpected field in zero-size",
                           "struct (Microsoft C mode)");
#endif /* CHECKING */
      /* Something like this:
           struct S { T t[]; };           // sizeof(S) == sizeof(int)
         where T is any type -- sizeof(S) in Microsoft C mode is 2 on x86 and
         4 on PowerPCs.  However, we don't really understand what's going on
         here, and we may not have reverse engineered it quite accurately.
         In any case, it's different in C++ mode, and even in C mode this sort
         of case is handled differently:
           struct S2 { char c; T t[]; };  // sizeof (S2) == 1
      */
      class_type->size = targ_sizeof_int;
    } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* Do not insert code here. */
    if (!(gcc_mode || (gpp_mode && gnu_zero_sized_class_type(class_type)))) {
      class_type->size = 1;
#if IA64_ABI
      /* Because we found the need to adjust the class size here, we may need
         to also adjust ctsp->size_without_virtual_base_classes below. */
      zero_size_adjusted = TRUE;
#endif /* IA64_ABI */
    }  /* if */
  }  /* if */
  /* If the class has virtual base classes, the size and alignment without
     virtual base classes will already have been recorded; otherwise, record
     it now. */
  if (!C_mode() &&
      !class_type->variant.class_struct_union.any_virtual_base_classes
#if IA64_ABI
      && (!targ_reuse_tail_padding || is_POD || zero_size_adjusted)
#endif /* IA64_ABI */
                                                                   ) {
    a_class_type_supplement_ptr	ctsp = class_type_supp(class_type);
    ctsp->size_without_virtual_base_classes = class_type->size;
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode) {
      a_targ_alignment  max_alignment_for_base_class;
      /* MSVC ignores explicitly-specified alignment requirements that are more
         strict than platform-specific defaults when calculating the size of a
         a base class. */
      max_alignment_for_base_class = current_max_alignment_for_class_members();
      if (max_alignment_for_base_class == 0) {
        if (target_is_arm_based() || !target_is_64_bits()) {
          max_alignment_for_base_class = 8;
        } else {
          max_alignment_for_base_class = 16;
        }  /* if */
      }  /* if */
      if (max_alignment_for_base_class < (target_is_64_bits() ? 16u : 8u) &&
          max_alignment_for_base_class < class_type->alignment) {
        an_unnormalized_bit_offset   bit_offset = 0;
        if (do_alignment(&end_of_fields_byte_offset, &bit_offset,
                         max_alignment_for_base_class)) {
          ctsp->size_without_virtual_base_classes = end_of_fields_byte_offset;
        }  /* if */
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    ctsp->alignment_without_virtual_base_classes = class_type->alignment;
  }  /* if */
set_size_for_complete_object:
  /* The alignment for the class is (by definition) no less than
     targ_minimum_struct_alignment, but its size may have been computed to be
     smaller (e.g., for an empty class).  Adjust the size if appropriate. */
  if (class_type->size == 0 && gnu_mode) {
    /* This rule does not apply to empty GNU C structs and unions (even if the
       alignment was set explicitly), nor to certain GNU C++ class types. */
  } else if (class_type->size < class_type->alignment) {
    class_type->size = class_type->alignment;
#if TARG_PAD_ALLOCATED_EMPTY_BASE
    /* The size as a base can remain small, so that subsequent fields or bases
       can be allocated in the area that would otherwise be padding.  However,
       sometimes that is not desirable.  (In particular with a C generating
       back end where the base will be emitted as a structure field: the C
       compiler will perform the padding and we must ensure that this front
       end agrees with the C compiler on the size and offsets of the type. */
    if (!C_mode()) {
      a_class_type_supplement_ptr	ctsp = class_type->
                                        variant.class_struct_union.extra_info;
      ctsp->size_without_virtual_base_classes =
                                 ctsp->alignment_without_virtual_base_classes;
    }  /* if */
#endif /* TARG_PAD_ALLOCATED_EMPTY_BASE */
  }  /* if */
  class_type->incomplete = FALSE;
#if GNU_EXTENSIONS_ALLOWED
  /* If the class is a transparent union, verify that transparency
     is legal. */
  if (class_type->kind == (a_type_kind)tk_union &&
      class_type->variant.class_struct_union.is_transparent &&
      !check_transparent_union(class_type, &error_position)) {
    class_type->variant.class_struct_union.is_transparent = FALSE;
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if DEBUG
  if (debug_level >= 3) {
    if (C_dialect == C_dialect_cplusplus) db_base_class_list(class_type);
  }  /* if */
  if (db_flag_is_set("dump_layout")) {
    db_type(class_type);
    fputs("\n", f_debug);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* do_class_layout */


void layout_one_time_init(void)
/*
Do one-time initialization of variables related to class layout.  (Variables
that need to be reinitialized with each new translation unit are handled in
layout_init.)
*/
{
  /* Save variable needed for precompiled headers */
  if (precompiled_header_processing_required) {
    STATIC_THREAD a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(curr_max_member_alignment),
      pch_saved_var_array_elem(pack_alignment_stack),
      pch_saved_var_array_elem(avail_pack_alignment_stack_entries),
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  register_trans_unit_variable(curr_max_member_alignment);
  register_trans_unit_variable(pack_alignment_stack);
}  /* layout_one_time_init */


void layout_trans_unit_init(void)
/*
Initialize static variables related to class layout.  This function is
responsible for those variables that need to be initialized for the
processing of each (primary or secondary) translation unit.
*/
{
  curr_max_member_alignment = 0;
  pack_alignment_stack = NULL;
}  /* layout_trans_unit_init */


void layout_init(void)
/*
Initialize static variables related to class layout.  This is done as a
subroutine (rather than relying on static initialization) so that it
can be redone to compile more than one source file in a single invocation
of the front end.
*/
{
  avail_pack_alignment_stack_entries = NULL;
  check_assertion_str2(!targ_microsoft_bit_field_allocation ||
                              (targ_bit_field_container_size < 0),
                       "layout_init: inconsistent configuration",
                       "for bit field allocation");
}  /* layout_init */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

