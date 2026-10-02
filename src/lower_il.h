/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

lower_il.h -- Declarations related to lower_il.c (having to do with
              lowering C++ intermediate language to C intermediate language).

*/

/* Avoid including these declarations more than once: */
#ifndef LOWER_IL_H
#define LOWER_IL_H 1

/* Only include this code if it is needed.  A few routines are needed
   if name mangling is needed, even if IL lowering is not. */
/* NEED_NAME_MANGLING is always TRUE if DO_IL_LOWERING is TRUE. */
#if NEED_NAME_MANGLING

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_DEF_H */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

#if IA64_ABI || DO_IL_LOWERING
extern a_type_ptr make_vtbl_entry_type(void);
#endif /* IA64_ABI || DO_IL_LOWERING */

#if IA64_ABI
extern a_targ_size_t vtbl_entry_size(void);
#endif /* IA64_ABI */

extern void repr_for_ptr_to_data_member_constant(a_constant_ptr   constant, 
                                                 a_targ_ptrdiff_t *delta);

extern void repr_for_ptr_to_member_function_constant(a_constant_ptr   constant,
                                                     a_targ_ptrdiff_t *delta,
                                                     a_targ_ptrdiff_t *index,
                                                     a_routine_ptr    *func,
                                                     a_targ_ptrdiff_t *offset);

extern char *alloc_lowered_name_string(sizeof_t size);

extern char *alloc_lowered_name(const char *str);

extern a_boolean is_or_was_nullptr_type(a_type_ptr tp);

#if DO_IL_LOWERING

EXTERN_THREAD a_boolean
		il_lowering_underway;
			/* TRUE while IL lowering is actually being done. */
EXTERN_THREAD a_boolean
		lowering_file_scope;
			/* TRUE if lowering the file scope's IL, FALSE if
			   lowering a routine scope's IL. */
EXTERN_THREAD a_boolean
		keep_object_lifetime_info_in_lowered_il;
			/* TRUE if object lifetime information should be
			   preserved by the lowering process (so a back end
			   can use it, e.g., for exception handling). */
EXTERN_THREAD a_boolean
		typeinfo_uncoupled_when_vtable_is_optional;
			/* TRUE if in the current ABI a typeinfo variable
			   definition can go out independently of the vtable
			   variable definition if the vtable variable is
			   "optional". */

#if ENSURE_LOWERED_TYPE_LIST_ORDERING
EXTERN_THREAD a_boolean
		perform_type_list_ordering;
			/* TRUE if types may appear out-of-order in lowered
			   C code and need to be sorted to produce code that
			   will compile properly. */
#endif /* ENSURE_LOWERED_TYPE_LIST_ORDERING */

/*
Access the il_lowering_flag in an IL entry.
*/
#define il_lowering_flag_of(entry_ptr)                                \
  (il_entry_prefix_of(entry_ptr).il_lowering_flag)

/*
Macro that tests whether or not a given entry has been visited yet.
*/
#define visited_yet(entry_ptr) (il_lowering_flag_of(entry_ptr))

/*
Set the flag to indicate that an entry has been visited.
*/
#define mark_as_visited(entry_ptr) (il_lowering_flag_of(entry_ptr) = TRUE)

/*
Set the flag to indicate that an entry has not been visited.  Used
when a just-allocated entry requires lowering.
*/
#define mark_as_not_visited(entry_ptr) (il_lowering_flag_of(entry_ptr) = FALSE)

/*
Macro to test for a zero-length field.  This includes zero-length bit fields,
incomplete array fields (where allowed), and nontrivial properties and events
(in Microsoft mode).
*/
#define field_has_zero_length(field)                                        \
  ((field)->is_bit_field ? (field)->bit_size == 0 :                         \
                           (skip_typerefs((field)->type)->size == 0 ||      \
                            field_is_nontrivial_property_or_event(field)))

EXTERN_THREAD an_integer_kind
		targ_ptr_to_data_member_int_kind;
			/* The integer kind to use for pointers to data
			   members. */

#if GENERATE_EH_TABLES
typedef unsigned long a_handle_number;
			/* Number in the region table that identifies an
			   entry in the object address table or in the array
			   table. */
#else /* !GENERATE_EH_TABLES */
typedef int a_handle_number;
			/* Unused in this configuration, but passed as an
			   (unused) argument in some cases. */
#endif /* GENERATE_EH_TABLES */

/*
Types used to describe a position within an initialization:
*/
typedef struct an_init_pos_modifier *an_init_pos_modifier_ptr;
typedef struct an_init_pos_modifier {
  /* Modifier for an_init_pos_descr.  Usually allocated on the stack, but
     allocated on the heap when saved as part of a cleanup action entry. */
  an_init_pos_modifier_ptr
		next;
			/* Pointer to the similar entry at the next
			   level out. */
  a_type_ptr	type;
			/* Type of entity being initialized at this level.
			   This is the type after the modification at this
			   level. */
  a_targ_size_t	curr_elem;
			/* If the entity is an array, this is the number of
			   the element currently being initialized.  Ignored
			   unless curr_field == NULL and curr_base == NULL. */
  a_field_ptr	curr_field;
			/* If the entity is a struct or union, this points
			   to the field currently being initialized.  NULL
			   otherwise. */
  a_base_class_ptr
		curr_base;
			/* If the entity is a base class, this points to the
			   base class entry.  NULL otherwise. */
#if GNU_VECTOR_TYPES_ALLOWED
  a_byte_boolean
		is_vector_element;
			/* TRUE if the entity is an element of a vector.  Such
			   elements are mostly treated like array elements
			   except that they cannot be individually addressed,
			   so any initialization cannot be rewritten as
			   an assignment. */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
#if C99_IL_EXTENSIONS_SUPPORTED && !LOWER_COMPLEX
  a_byte_boolean
                is_complex;
                        /* TRUE if the entity is the real or imaginary part of
                           an un-lowered complex constant.  When TRUE,
                           curr_elem == 0 represents the "real" part of the
                           complex number and curr_elem == 1 represents the
                           "imaginary" part of the complex number. */
#endif /* C99_IL_EXTENSIONS_SUPPORTED && !LOWER_COMPLEX */
} an_init_pos_modifier;

EXTERN_THREAD an_init_pos_modifier_ptr
		avail_init_pos_modifiers;
			/* List of initialization position modifier entries
			   that have been freed and are available for reuse. */

typedef struct an_init_pos_descr *an_init_pos_descr_ptr;
typedef struct an_init_pos_descr {
  /* An initialization position description.  Starts with a variable (the
     variable itself or what it points to).  That base entity may be
     modified by a modifiers list. */
  an_init_pos_descr_ptr
                next;   /* When non-NULL, the next initialization position
                           description on a list.  Used to create a stack of
                           descriptions that describe the initial positions of
                           each entity in a nested aggregate (see
                           aggregate_this_stack). */
  a_variable_ptr
		variable;
			/* The base variable. */
#if !DO_FULL_PORTABLE_EH_LOWERING
  a_byte_boolean
		thrown_object_address;
			/* TRUE if the entity is the runtime location to which
			   a thrown object should be copied.  Note that
			   variable will be NULL in that case. */
#endif /* !DO_FULL_PORTABLE_EH_LOWERING */
  a_byte_boolean
		indirect_through_variable;
			/* If TRUE, variable is a pointer and its value gives
			   the base entity address. */
  a_byte_boolean
		array_element_sequence;
			/* TRUE if the entity is a sequence of array elements
			   initialized as one unit (i.e., under a
			   ck_init_repeat). */
  a_byte_boolean
		base_class_subobject;
			/* TRUE if the entity is a base class of an object,
			   and therefore not a complete object. */
  a_byte_boolean
		base_of_complete_object;
			/* When base_class_subobject is TRUE, this is TRUE
			   to indicate that the derived class is a complete
			   object, which allows more efficient addressing of
			   virtual base classes. */
  a_type_ptr	base_type;
			/* Base entity type. */
  an_init_pos_modifier_ptr
		modifiers;
			/* Optional list of modifiers of the base variable,
			   NULL if none.  In order from innermost to outermost
			   modifier. */
  a_targ_ptrdiff_t
		array_element_count;
			/* If array_element_sequence is TRUE, the count of
			   elements in the array, or -1 for an unknown-length
			   array (new/delete only).  Zero otherwise. */
  a_type_ptr	array_element_type;
			/* If array_element_sequence is TRUE, the type of
			   the elements in the array.  NULL otherwise.
			   Useful in distinguishing cases where the
			   element sequence is a flattened multi-dimensional
			   array. */
  an_expr_node_ptr
		num_elem_node;
			/* For variably-sized arrays, an expression that
			   gives (at runtime) the number of elements
			   in the array.  NULL otherwise. */
  a_host_large_integer
		partial_initialization_starting_element;
			/* When not -1, indicates the element to begin
			   default initializing when a variably-sized array
			   is partially-initialized. */
} an_init_pos_descr;

typedef struct a_destructible_entity_descr *a_destructible_entity_descr_ptr;
typedef struct a_destructible_entity_descr {
  /* Description of an entity that requires destructor.  Dynamically
     allocated and pointed to from a_dynamic_init.  Contains the information
     IL lowering needs in addition to the dynamic init entry to destroy
     an entity. */
  a_destructible_entity_descr_ptr
		next;	/* Pointer to next entry when on an available list. */
  an_init_pos_descr
		init_pos_descr;
			/* Location of the entity. */
  a_dynamic_init_ptr
		cleanup_state_to_set_when_starting_destruction;
			/* When destroying this entity when exceptions are
			   enabled, this is the cleanup state to establish
			   as current when beginning the destruction.  It's
			   the next destruction to process after this
			   entity is destroyed. */
#if DO_UNORDERED_EH_PROCESSING
			/* In the presence of unordered initializations in the
			   IL, this indicates the first entry in a set of
			   unordered destructions, and in that way differs from
			   next_in_region_table. */
#endif /* DO_UNORDERED_EH_PROCESSING */
  a_variable_ptr
		conditional_flag_var;
			/* If non-NULL, points to a variable that is the
			   conditional flag variable that is set to non-zero
			   to indicate that the initialization has been
			   done. */
#if DO_FULL_PORTABLE_EH_LOWERING
  a_handle_number
		conditional_flag_handle;
			/* If conditional_flag_var is non-NULL, this is
			   the object table index number for the conditional
			   flag variable. */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
#if GENERATE_EH_TABLES
  a_cleanup_region_number
		region_number;
			/* When exceptions are enabled, this is the
			   destructible object region number, i.e., the index
			   into the region table.  This is the region number
			   to establish as the current region number once the
			   construction of the entity has been finished.
			   Usually, this is set when the region table entry
			   is created, but for constructor-inits in a
			   destructor it is preassigned. */
#if DO_UNORDERED_EH_PROCESSING
			/* Note that if the initialization/destruction is part
			   of an unordered set, this number will be the number
			   of the first member of the set. */
#endif /* DO_UNORDERED_EH_PROCESSING */
  a_constant_ptr
		region_table_entry;
			/* When exceptions are enabled, this points to the
			   aggregate constant that defines the region table
			   entry for the destruction of this entity. */
  a_dynamic_init_ptr
		next_in_region_table;
			/* When exceptions are enabled, this points to the
			   initialization that follows this one in destruction
			   order.  Usually, this is the same as the
			   next_in_destruction_list pointer in the dynamic
			   initialization itself, but different when region
			   table entries are cloned (e.g., because of long
			   lifetime temporaries).  Also, this is not always
			   the same as the value in the "next" field in the
			   constant pointed to by region_table_entry.
			   For one thing, this does not leave a lifetime,
			   whereas the previous entry in the region table
			   might be from a previous lifetime.  Another way
			   of describing this field: it links an
			   initialization to the one processed most recently
			   before it.  In the presence of unordered
			   initializations in the IL, lowering traversal
			   order (reflected by this pointer) might be
			   slightly different than the front end order
			   (reflected by the dynamic init
			   next_in_destruction_list pointer). */
#endif /* GENERATE_EH_TABLES */
  a_byte_boolean
		initialization_done;
			/* Set to TRUE once the initialization of this entity
			   has been completed. */
  a_byte_boolean
		needs_subobject_construction_vtbl;
			/* Set to TRUE if the initialization is of a base
			   class subobject and a special construction vtable
			   needs to be passed to the subobject constructor
			   and destructor. */
  a_byte_boolean
		construction_vtbls_var_is_array;
			/* Set to TRUE if the variable in
			   construction_vtbls_var is itself the array, FALSE
			   if the variable is a pointer to the array. */
  a_byte_boolean
		use_delegation_dtor;
			/* Set to TRUE if a special "delegation" destructor
			   should be used in the region table entry for this
			   destruction (because the destruction may be for
			   either a complete or subobject destruction and
			   that isn't known until run-time). */
  a_byte_boolean
                is_destruction_for_partial_static_aggregate;
                        /* TRUE if this destruction is for a partial aggregate
                           of a static variable.  Such destructions are left
                           on the destruction list, but should be ignored
                           once the static variable has been completely
                           constructed (because static variables use a
                           different destruction mechanism).  This flag is set
                           only after the static variable has been (fully)
                           initialized. */
  a_variable_ptr
		delegation_dtor_arg;
			/* When use_delegation_dtor is TRUE, this variable
			   represents the argument that will be passed to the
			   "delegation" destructor.  In the IA-64 ABI, this
			   is the VTT parameter from the enclosing routine;
			   in the Cfront ABI, it is a base class pointer. */
  a_variable_ptr
		construction_vtbls_var;
			/* When needs_subobject_construction_vtbl is TRUE,
			   points to a variable for the appropriate
			   construction vtables array, possibly a pointer
			   as indicated by construction_vtbl_is_array.
			   When needs_subobject_construction_vtbl is TRUE and
			   this field is NULL, a NULL pointer should be
			   passed to the constructor or destructor (in
			   the IA-64 ABI). */
  a_base_class_ptr
		subobject_construction_base_class;
			/* When needs_subobject_construction_vtbl is TRUE,
			   the subobject base class. */
} a_destructible_entity_descr;

EXTERN_THREAD a_destructible_entity_descr_ptr
		avail_destructible_entity_descrs;
			/* List of destructible entity descriptions that
			   have been freed and are available for reuse. */


#if DEBUG
/*
Count of entries allocated, for debugging purposes.
*/
EXTERN_THREAD unsigned long
		num_init_pos_modifiers_allocated,
		num_destructible_entity_descrs_allocated;
#endif /* DEBUG */


/*
Structure put on a list to remember the locations of all return statements
in the current routine, so they can be rewritten to execute epilogue code.
*/
typedef struct a_return_memo *a_return_memo_ptr;
typedef struct a_return_memo {
  a_return_memo_ptr
		next;	/* Next entry on the list, or NULL if this is the
			   last. */
  a_statement_ptr
		stmt;
			/* Pointer to a return statement. */
} a_return_memo;


/*
Entry used to describe an insert location within a statement or expression
tree.
*/
enum an_insert_location_kind {
  /* Kind of insert location: */
  ilk_after_statement,	/* Insert after a statement. */
  ilk_block_start,	/* Insert at the start of a block. */
  ilk_statement_creation,
			/* Create a new statement (first insert provides the
			   statement). */
  ilk_before_expr,	/* Insert before an expression. */
  ilk_after_expr,	/* Insert after an expression. */
  ilk_expr_creation	/* Create a new expression (first insert provides the
			   expression). */
};

/* Test for the insertion kinds for insertions within expressions. */
#define is_expr_insert_location_kind(kind)                            \
 ((kind) == ilk_before_expr || (kind) == ilk_after_expr ||            \
  (kind) == ilk_expr_creation)
#define is_expr_insert_location(insert_location)                      \
  is_expr_insert_location_kind((insert_location)->kind)
/* Macros for testing whether an insert location refers to an "empty"
   statement insert location. */
#define is_empty_statement_insert_location_kind(kind)                 \
 ((kind) == ilk_block_start || (kind) == ilk_statement_creation)
#define is_empty_statement_insert_location(insert_location)           \
  is_empty_statement_insert_location_kind((insert_location)->kind)

typedef struct an_insert_location *an_insert_location_ptr;
typedef struct an_insert_location {
  an_insert_location_kind
		kind;	/* Kind of insert location: after expression,
			   after statements, etc. */
  union {
    /* When kind == ilk_after_statement
       or   kind == ilk_block_start
       or   kind == ilk_statement_creation: */
    struct {
      a_statement_ptr
		stmt;	/* The statement to insert after, or the block to
			   insert at the start of.  In the ilk_after_statement
			   case, the statement must be part of a statement
			   sequence, not, for example, the dependent statement
			   of an "if". */
      a_statement_ptr
		marker;	/* Points to a "marked" statement within the
			   insert location if non-NULL. */
      a_byte_boolean
		is_marked;
			/* TRUE if this insert location has been "marked". */
    } statement;
    /* When kind == ilk_before_expr
       or   kind == ilk_after_expr
       or   kind == ilk_expr_creation: */
    an_expr_node_ptr
		expr;	/* The expression to insert before or after. */
  } variant;
} an_insert_location;

/*
Entry used to keep track of a list of local temporary variables that are
reusable.
*/
typedef struct a_temporary_list_entry *a_temporary_list_entry_ptr;
typedef struct a_temporary_list_entry {
  a_temporary_list_entry_ptr
		next;
			/* The next entry on the list, or NULL if this is the
			   last entry. */
  a_variable_ptr
		var;	/* A temporary variable. */
  a_byte_boolean
		in_use;	/* TRUE if the temporary is currently in use and is
			   not reusable at the moment. */
} a_temporary_list_entry;


/*
Entry used to make a list of the compound statements with no associated
scope nested within the current context on the context stack.
*/
typedef struct a_scopeless_compound_stmt *a_scopeless_compound_stmt_ptr;
typedef struct a_scopeless_compound_stmt {
  a_scopeless_compound_stmt_ptr
		next;	/* Next entry on the list, i.e., next scope out,
			   or NULL if this is the last entry. */
  a_statement_ptr
		stmt;	/* The compound statement. */
  a_temporary_list_entry_ptr
		saved_local_temporaries;
			/* List of local temporary variables that are
			   potentially reusable, saved from the current
			   context stack value when this entry was pushed,
			   for later restoration. */
} a_scopeless_compound_stmt;


/*
Entry used to keep track of the context during the lowering operation.
A linked list of these runs from the current point back through the stack
to the outermost invocations, giving a history of the IL parents of
the IL object currently being considered.
*/
typedef struct a_context *a_context_ptr;
typedef struct a_context {
  a_context_ptr parent;	/* Parent context. */
  a_scope_ptr	scope;	/* Scope associated with this context. */
  an_object_lifetime_ptr
		lifetime;
			/* Object lifetime associated with this context.
			   If the context doesn't define a lifetime, the
			   lifetime is inherited from the parent.  NULL
			   only if there are no lifetimes at all, all
			   the way up. */
  a_byte_boolean
		new_lifetime;
			/* TRUE if this context entry defines a new object
			   lifetime (i.e., it has a lifetime and the lifetime
			   is not inherited from the parent context). */
  a_byte_boolean
		is_function_try_block;
			/* TRUE if this context is associated with a
			   function-try-block. */
  a_byte_boolean
		is_generated_routine_context;
			/* TRUE if the context is for a routine that has been
			   created by lowering. */
  an_object_lifetime_ptr
		successor_lifetime_at_statement;
			/* If the object lifetime has a successor that begins
			   at a label or switch case statement (when
			   long_lifetime_temps is TRUE), this is the lifetime.
			   This helps us watch for the appearance of the
			   associated statement, since there is no explicit
			   indication in the statement that it begins another
			   lifetime. */
  a_dynamic_init_ptr
		latest_initialization;
			/* The current position in the destructions list
			   of the lifetime, i.e., the latest encountered
			   dynamic initialization requiring destruction.
			   Note that this is not updated when destructions
			   are generated (e.g., at the end of a block or
			   on a goto or return), so it stays indicating
			   the "most-constructed" state.  That's different
			   than curr_cleanup_state, which is updated on
			   destructions. */
  a_dynamic_init_ptr
		curr_cleanup_state;
			/* Current cleanup position.  That is, a pointer to
			   the dynamic initialization entry for the first
			   destruction to be done if one wishes to exit from
			   the current location in the program.  Further
			   cleanups are attached to the first entry.  Differs
			   from latest_initialization in that this field gets
			   updated as destructions are generated when cleaning
			   up on exit from a lifetime.  Also spans object
			   lifetimes. */
  an_object_lifetime_ptr
		saved_curr_object_lifetime;
			/* Used to save/restore the global variable
			   curr_object_lifetime over push_context/
			   pop_context. */
#if GENERATE_EH_TABLES
#if DO_FULL_PORTABLE_EH_LOWERING
  a_variable_ptr
		try_frame;
			/* For a context associated with a "try" block, this
			   points to the variable for the stack frame for the
			   try. */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
#endif /* GENERATE_EH_TABLES */
  a_temporary_list_entry_ptr
		local_temporaries;
			/* List of local temporary variables that are
			   potentially reusable. */
  a_scopeless_compound_stmt_ptr
		scopeless_compound_stmts;
			/* List of compound statements without scopes that
			   we are currently inside of, in order from
			   innermost to outermost.  Stops at the scope
			   or lifetime associated with this context stack
			   entry. */
#if CHECKING
  a_boolean     in_full_expression;
                        /* TRUE if we're lowering a full-expression in this
                           context.  Used to guard against lowering nested
                           full-expressions in the same context. */
#endif /* CHECKING */
} a_context;

EXTERN_THREAD a_context_ptr
		curr_context;
			/* Current (bottom) end of the context chain. */

EXTERN_THREAD a_return_memo_ptr
		return_memo_list;
			/* List of return statements found in the current
			   routine, maintained so that epilogue code can be
			   added. */

EXTERN_THREAD a_variable_ptr
		return_value_pointer_variable;
			/* While processing a routine that returns its
			   value via a copy constructor, this points to
			   the parameter variable for the implicit parameter
			   through which the caller sends the address
			   at which the result will be stored; NULL
			   otherwise. */

EXTERN_THREAD a_variable_ptr
                gse_return_value_pointer_variable;
                        /* Somewhat similar to return_value_pointer_variable
                           above, this is set to point to the variable that is
                           being initialized via copy construction from the
                           result of a GNU statement expression. For example:
                               A a = ({ f(); A(); });
                           NULL when not lowering a GNU statement expression
                           and in cases where the initialization doesn't
                           explicitly specify a variable (see below). */
EXTERN_THREAD an_init_pos_descr_ptr
                gse_init_position;
                        /* Specifies the entity that is being initialized
                           (when it isn't a variable -- see above) by a
                           GNU statement expression.  For example:
                              S *p = new S(({ S(); }));
                           NULL otherwise. */

#if DO_RETURN_VALUE_OPTIMIZATION_IN_LOWERING
/*
Return TRUE if the indicated variable is the return value optimization
variable for the current function.
*/
#define var_is_return_value_variable(var)                             \
  (innermost_function_scope != NULL &&                                \
   innermost_function_scope->variant.routine.return_value_variable == (var))
#endif /* DO_RETURN_VALUE_OPTIMIZATION_IN_LOWERING */

EXTERN_THREAD a_local_static_variable_init_ptr
                promoted_local_static_variable_inits;
			/* List of initialization entries for local static
			   variables promoted out of the current routine. */

EXTERN_THREAD a_source_position
		code_pos_for_lowering;
			/* The source position associated with executable code
			   currently being lowered. */

/*
Put the current code_pos_for_lowering into a statement, if the statement
pointer is non-NULL.  Set ending position as well if
EXTRA_SOURCE_POSITIONS_IN_IL is TRUE.
*/
#if EXTRA_SOURCE_POSITIONS_IN_IL
#define set_stmt_pos_to_code_pos_for_lowering(stmt)                       \
{ if ((stmt) != NULL) {                                                   \
    check_assertion((stmt)->compiler_generated &&                         \
                    (stmt)->lowering_generated);                          \
    (stmt)->position = code_pos_for_lowering;                             \
    (stmt)->end_position = code_pos_for_lowering;                         \
  }  /* if */                                                             \
}  /* set_stmt_pos_to_code_pos_for_lowering */
#else /* !EXTRA_SOURCE_POSITIONS_IN_IL */
#define set_stmt_pos_to_code_pos_for_lowering(stmt)                       \
{ if ((stmt) != NULL) {                                                   \
    check_assertion((stmt)->compiler_generated &&                         \
                    (stmt)->lowering_generated);                          \
    (stmt)->position = code_pos_for_lowering;                             \
  }  /* if */                                                             \
}  /* set_stmt_pos_to_code_pos_for_lowering */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS

/*
Description of a virtual function table instance to be used during
construction or destruction of a class that has overridden virtual
functions in virtual base classes.  In such cases, a virtual function
table instance is needed for a base class (call it A) for use while
executing the constructor/destructor for a derived class (call it B)
where the complete object type is actually some further-derived class
(call it C).  We call this the virtual function table instance for
"A in B in C".  The set of overriding functions is determined by B
(i.e., no overriding functions in derived classes of B are considered),
but the layout -- and therefore the delta value in the virtual function
table entry -- is determined by C.

A list of these represents an array of virtual function table instance
addresses.
*/
/* a_construction_vtbl_ptr is declared in il_def.h */
typedef struct a_construction_vtbl {
  a_construction_vtbl_ptr
		next;	/* Next entry on the list in array element order,
			   or NULL for the last element. */
  union {
    /* When is_subobject is TRUE: */
    a_base_class_ptr
		base_class;
			/* The base class used to determine overriding, e.g.,
			   base class A in B in the above description. */
#if IA64_ABI
    /* When is_subobject is FALSE: */
    a_type_ptr	derived_class;
			/* The class used to determine overriding. */
#endif /* IA64_ABI */
  } variant;
  a_base_class_ptr
		ctor_base_class;
			/* The base class describing the subobject considered
			   to be the complete object for purposes of overriding
			   relative to the class that really is the complete
			   object, e.g., base class B in C in the above
			   description.  NULL if the two are the same (in
			   which case the virtual function table instance is
			   a standard one where overriding and layout are
			   determined relative to the same class). */
  a_variable_ptr
		virtual_function_table_var;
			/* The variable for this instance of the virtual
			   function table. */
#if IA64_ABI
  a_virtual_table_index
		virtual_function_table_index;
			/* The index in the virtual_function_table_var where
			   this construction virtual function table begins.
			   This value indicates the location to which the vptr
			   should be set, not the location of the start of the
			   virtual function table. */
  a_byte_boolean
		is_subobject;
			/* TRUE if A and B are not the same class. */
#endif /* IA64_ABI */
} a_construction_vtbl;

#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */

/*
TRUE if the base class given by bcp has its own virtual function table.
*/
#if !IA64_ABI
#define base_class_has_vtbl(bcp) \
  (bcp->virtual_function_table_var != NULL)
#else /* IA64_ABI */
#define base_class_has_vtbl(bcp) \
  (bcp->virtual_function_table_offset != -1)
#endif /* IA64_ABI */

/*
Data structure used to pass information between lower_destructor_code,
gen_dtor_member_and_base_destructions, and
insert_dtor_member_and_base_destructions.
*/
typedef struct a_destructor_wrapper_info_block
		*a_destructor_wrapper_info_block_ptr;
typedef struct a_destructor_wrapper_info_block {
  a_statement_ptr
		epilogue_block;
			/* Block containing destructions, and initially
			   unattached to the IL tree.  Set by
			   gen_dtor_member_and_base_destructions.  NULL
			   if there are no destructions. */
  a_dynamic_init_ptr
		first_epilogue_destruction;
			/* First destruction to be done in the epilogue.
			   NULL if there are no destructions in the epilogue.
			   Set by gen_dtor_member_and_base_destructions. */
  a_variable_ptr
		destruction_vtbls_var;
			/* Pointer to a variable that is an array of virtual
			   function table addresses to be used during
			   destruction, if needed.  NULL otherwise. */
  a_variable_ptr
		complete_obj_var;
			/* For a destructor for a class with virtual base
			   classes, pointer to a variable with a non-zero
			   value if the destructor is destroying a complete
			   object.  For the Cfront-like ABI, this is simply
			   the parameter passed in.  For the IA-64 ABI, this
			   is a temporary set to the proper value. */
} a_destructor_wrapper_info_block;


/*
Return TRUE if the indicated constructor routine needs added implied arguments.
This must match make_ctor_implied_arg_list.  ctor_needs_vtt_argument
is TRUE (only in the IA-64 ABI) if the constructor takes a VTT parameter.
*/
#if !IA64_ABI
#define ctor_needs_implied_arg_list(ctor_routine)                     \
  (parent_class_of(ctor_routine)->                                    \
                 variant.class_struct_union.any_virtual_base_classes)
#else /* IA64_ABI */
#define ctor_needs_vtt_argument(ctor_routine)                         \
  (((ctor_routine)->ctor_dtor_kind == (a_ctor_or_dtor_kind)cdk_subobject || \
    (ctor_routine)->ctor_dtor_kind == (a_ctor_or_dtor_kind)cdk_delegation) && \
   parent_class_of(ctor_routine)->                                    \
                 variant.class_struct_union.any_virtual_base_classes)
#define ctor_needs_implied_arg_list(ctor_routine)                     \
  ctor_needs_vtt_argument(ctor_routine)
#endif /* IA64_ABI */


/*
Return TRUE if the indicated destructor routine needs added implied arguments.
This must match make_dtor_implied_arg_list.  dtor_needs_vtt_argument
is TRUE (only in the IA-64 ABI) if the destructor takes a VTT parameter.
*/
#if !IA64_ABI
#define dtor_needs_implied_arg_list(dtor_routine) TRUE
#else /* IA64_ABI */
#define dtor_needs_vtt_argument(dtor_routine)                              \
  (((dtor_routine)->ctor_dtor_kind == (a_ctor_or_dtor_kind)cdk_subobject || \
    (dtor_routine)->ctor_dtor_kind == (a_ctor_or_dtor_kind)cdk_delegation) && \
   parent_class_of(dtor_routine)->                                         \
                 variant.class_struct_union.any_virtual_base_classes)
#define dtor_needs_implied_arg_list(dtor_routine) \
   dtor_needs_vtt_argument(dtor_routine)
#endif /* IA64_ABI */


/*
Utility that returns TRUE if the class type has been pre-lowered.
*/
/*lint -emacro(664,class_has_been_prelowered)*/
#define class_has_been_prelowered(class)                                      \
  (check_assertion(is_immediate_class_type((class))),                         \
   class_type_supp((class))->subobject_partner != NULL)

/*
Utility to return a pointer to the subobject type for a class type that has
already been pre-lowered.  Note that in cases where a subobject type isn't
needed the class type itself is returned.
*/
/*lint -emacro(664,subobject_for_class)*/
#define subobject_for_class(class)                                            \
  (check_assertion(class_has_been_prelowered((class))),                       \
   class_type_supp((class))->has_subobject_type ?                             \
     class_type_supp((class))->subobject_partner :                            \
     (check_assertion((class) == class_type_supp((class))->subobject_partner),\
      (class)))

/*
Utility to return a pointer to the original class type given a subobject class
type that was generated during pre-lowering.
*/
#define orig_class_for_potential_subobject_type(class)                        \
  (check_assertion(class_has_been_prelowered(class)),                         \
   class_type_supp(class)->has_subobject_type ?                               \
    (check_assertion(class_type_supp((class))->subobject_partner == (class)), \
     (class)) :                                                               \
     (class_type_supp((class))->subobject_partner == (class) ?                \
      (class) :                                                               \
      (check_assertion(class_type_supp(class_type_supp((class))               \
                                   ->subobject_partner)->has_subobject_type), \
       class_type_supp((class))->subobject_partner)))

/*
Returns TRUE if the specified class is a subobject type (and is not a "trivial"
subobject type -- i.e., it differs from the complete class type).
*/
#define is_a_unique_subobject_type(class)                                     \
  (!class_type_supp(class)->has_subobject_type &&                             \
   class_type_supp(class)->subobject_partner != NULL &&                       \
   class_type_supp(class)->subobject_partner != (class))

extern a_field_ptr corresponding_subobject_au_field(
                                                a_type_ptr  subobject_type,
                                                a_field_ptr complete_au_field);

extern a_boolean il_lowering_needed(void);

extern void pop_context(void);

extern void push_context(a_context              *context,
                         a_scope_ptr            scope,
                         an_object_lifetime_ptr lifetime);

extern void save_and_push_context(a_context              *context,
                                  a_scope_ptr            scope,
                                  an_object_lifetime_ptr lifetime,
                                  a_context              **saved_curr_context);

extern void restore_saved_context(a_context *context);

extern void set_insert_location(a_statement_ptr    stmt,
                                an_insert_location *insert_location);

extern void set_block_start_insert_location(
                                          a_statement_ptr    stmt,
                                          an_insert_location *insert_location);
extern void set_statement_creation_insert_location(
                                          an_insert_location *insert_location);

extern void set_expr_insert_location(an_expr_node_ptr   node,
                                     an_insert_location *insert_location);

extern void set_expr_creation_insert_location(
                                          an_insert_location *insert_location);

extern void finish_class_type(a_type_ptr class_type);

extern void add_to_front_of_file_scope_types_list(a_type_ptr type);

extern a_type_ptr make_mptr_type(void);

extern a_type_ptr pointer_to_vtbl_type(void);

extern a_type_ptr make_virtual_table_table_pointer_type(void);

extern an_expr_node_ptr get_virtual_function_address(
                                             an_expr_node_ptr func_node,
                                             an_expr_node_ptr *object_node,
                                             a_boolean        vars_can_change,
                                             a_variable_ptr   *vtbl_temp_var,
                                             an_expr_node_ptr *assign_node);

#if IA64_ABI

extern a_boolean contains_ptr_to_data_member(a_type_ptr type);

extern void lower_initializer(a_variable_ptr     variable,
                              an_init_kind       *init_kind,
                              an_initializer_ptr initializer);
#endif /* IA64_ABI */

extern a_type_ptr get_underlying_type(a_type_ptr type);

extern a_type_ptr pm_member_type_possibly_lowered(a_type_ptr type);

extern a_type_ptr pm_class_type_possibly_lowered(a_type_ptr type);

extern a_boolean is_or_was_ptr_to_data_member_type(a_type_ptr type);

extern a_boolean is_or_was_ptr_to_member_function_type(a_type_ptr type);

extern an_expr_node_ptr au_field_lvalue_selection_expr(an_expr_node_ptr node,
                                                       a_field_ptr      field);

extern an_expr_node_ptr make_base_class_lvalue(
                                             an_expr_node_ptr node,
                                             a_base_class_ptr bcp,
                                             a_boolean        complete_object);

extern an_expr_node_ptr make_base_class_lvalue_from_var(
                                             a_variable_ptr   var,
                                             a_base_class_ptr bcp,
                                             a_boolean        complete_object);

extern void change_to_cast(an_expr_node_ptr node,
                           an_expr_node_ptr operand_node,
                           a_type_ptr       new_type);

extern an_expr_node_ptr add_lowered_cast_if_necessary(
                                                    an_expr_node_ptr node,
                                                    a_type_ptr       new_type);

extern an_expr_node_ptr add_cast_to_char_star(an_expr_node_ptr node);

extern an_expr_node_ptr array_first_element_addr_expr(a_variable_ptr var);

extern an_expr_node_ptr make_node_for_il_constant(a_constant_ptr constant);

#if !IA64_ABI
extern an_expr_node_ptr make_vbptr_field_lvalue(an_expr_node_ptr node,
                                                a_base_class_ptr bcp);

extern an_expr_node_ptr make_vbptr_field_lvalue_from_var(a_variable_ptr   var,
                                                         a_base_class_ptr bcp);
#else /* IA64_ABI */
extern void put_routine_into_comdat_group(a_routine_ptr routine);

extern a_virtual_table_index num_negative_vtable_entries(
                                                   a_type_ptr       class_type,
                                                   a_base_class_ptr bcp);
#endif /* !IA64_ABI */

extern void put_variable_into_comdat_group(a_variable_ptr variable);

extern an_expr_node_ptr make_vptr_field_lvalue(an_expr_node_ptr node);

#if ABI_CHANGES_FOR_RTTI
extern an_expr_node_ptr make_any_vptr_rvalue(an_expr_node_ptr expr,
                                             an_expr_node_ptr *other_expr);
#endif /* ABI_CHANGES_FOR_RTTI */

#if !IA64_ABI
extern an_expr_node_ptr make_vbase_class_lvalue_from_var(
                                             a_variable_ptr   var,
                                             a_base_class_ptr bcp,
                                             a_boolean        complete_object);
#endif /* !IA64_ABI */

extern a_variable_ptr assign_expr_to_temp(an_expr_node_ptr expr);

extern an_expr_node_ptr assign_expr_to_temp_and_make_expr_for_reuse(
                                                        an_expr_node_ptr expr);

extern an_expr_node_ptr make_reusable_copy(an_expr_node_ptr expr,
                                           a_boolean        vars_can_change);

extern an_expr_node_ptr make_lvalue_reusable_copy_full(
                                             an_expr_node_ptr expr,
                                             a_boolean        vars_can_change,
                                             a_boolean        *temp_init_used);

extern an_expr_node_ptr make_lvalue_reusable_copy(
                                             an_expr_node_ptr expr,
                                             a_boolean        vars_can_change);

extern void insert_expr(an_expr_node_ptr       inserted_expr,
                        an_insert_location_ptr insert_location);

extern void mark_stmk_inits_as_following_exec_statement(
                                                    a_statement_ptr statement);

/*
Macro for typical call to insert_statement_full where a lowering post pass
is necessary.
*/
#define insert_statement(statement, insert_location) \
  insert_statement_full(statement, insert_location, /*perform_post_pass=*/TRUE)

extern void insert_statement_full(a_statement_ptr        statement,
                                  an_insert_location_ptr insert_location,
                                  a_boolean              perform_post_pass);

extern void set_insert_location_mark(an_insert_location_ptr insert_location);

extern void reset_insert_location_mark(an_insert_location_ptr insert_location);

extern a_statement_ptr insert_expr_statement(
                                       an_expr_node_ptr       node,
                                       an_insert_location_ptr insert_location);

extern a_statement_ptr insert_expr_statement_set_pos(
                                       an_expr_node_ptr       node,
                                       an_insert_location_ptr insert_location);

extern an_expr_node_ptr make_var_assignment_expr(a_variable_ptr   lvalue_var,
                                                 an_expr_node_ptr rvalue_expr);

extern a_statement_ptr insert_assignment_statement(
                                       an_expr_node_ptr       lvalue_expr,
                                       an_expr_operator_kind  op,
                                       an_expr_node_ptr       rvalue_expr,
                                       an_insert_location_ptr insert_location);

extern a_statement_ptr insert_var_assignment_statement(
                                       a_variable_ptr         lvalue_var,
                                       an_expr_node_ptr       rvalue_expr,
                                       an_insert_location_ptr insert_location);

#if REWRITE_UCN_ESCAPE_CHAR_IN_LOWERING
extern void rewrite_ucns_in_name(a_source_correspondence *source_corresp);
#endif /* REWRITE_UCN_ESCAPE_CHAR_IN_LOWERING */

extern a_statement_ptr last_statement_in_block(
                                              a_statement_ptr block_statement);

extern a_variable_ptr make_unnamed_local_static_variable(
                                                 a_type_ptr type,
                                                 a_boolean  in_function_scope);

extern a_variable_ptr make_lowered_variable(a_const_char    *var_name,
                                            a_boolean       already_il_name,
                                            a_type_ptr      var_type,
                                            a_storage_class var_storage_class);

extern a_variable_ptr make_lowered_param_variable(a_type_ptr type);

extern an_expr_node_ptr make_array_to_pointer_node(an_expr_node_ptr operand);

extern a_variable_ptr make_global_var_with_prefixed_name(
                                      a_const_char            *prefix,
                                      an_integer_kind         ikind,
                                      a_source_correspondence *source_corresp,
                                      an_il_entry_kind        kind);

#if AUTOMATIC_TEMPLATE_INSTANTIATION
extern void make_instantiation_info_var(
                                    a_const_char            *prefix,
                                    a_source_correspondence *source_corresp,
                                    an_il_entry_kind        kind);
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */

extern void add_temporary_to_scope(a_variable_ptr temp,
                                   a_scope_ptr    scope,
                                   a_boolean      promote_if_necessary);

extern a_variable_ptr make_temporary(a_type_ptr  temp_type,
                                     a_boolean   force_static);

extern a_variable_ptr make_temporary_in_scope(
                                             a_type_ptr  temp_type,
                                             a_scope_ptr scope,
                                             a_boolean   force_static,
                                             a_boolean   promote_if_necessary);

extern a_variable_ptr make_lowered_temporary(a_type_ptr temp_type);

extern a_variable_ptr make_file_scope_temporary(a_type_ptr temp_type);

extern a_variable_ptr find_reusable_temporary(
                                          a_type_ptr                 temp_type,
                                          a_temporary_list_entry_ptr *ptlep);

extern void add_to_reusable_temporaries_list(a_variable_ptr temp_var);

extern a_variable_ptr make_local_temporary(a_type_ptr temp_type);

extern a_variable_ptr make_temporary_for_dynamic_init(
                                         a_type_ptr         temp_type,
                                         a_dynamic_init_ptr dip,
                                         a_boolean          *is_reusable_temp);

extern a_type_ptr make_lowered_class_type(a_type_kind  kind);

extern void make_lowered_field(a_const_char  *field_name,
                               a_type_ptr    field_type,
                               a_type_ptr    struct_type,
                               a_field_ptr   *last_field);

extern a_type_ptr void_star_type(void);

extern a_type_ptr char_star_type(void);

extern a_type_ptr make_vptp_type(void);

extern void set_integer_constant_with_overflow_check(
                                    a_constant_ptr       con,
                                    a_host_large_integer con_val,
                                    an_integer_kind      ikind,
                                    a_type_ptr           class_type,
                                    a_boolean            preserve_needed_flag);

extern void set_unsigned_integer_constant_with_overflow_check(
                                   a_constant_ptr        con,
                                   a_host_large_unsigned con_val,
                                   an_integer_kind       ikind,
                                   a_type_ptr            class_type,
                                   a_boolean             preserve_needed_flag);

extern void set_virtual_function_table_name(a_variable_ptr   vtbl_var,
                                            a_type_ptr       class_type,
                                            a_base_class_ptr bcp,
                                            a_base_class_ptr ctor_bcp);

extern a_variable_ptr make_var_for_virtual_function_table(
                                                   a_type_ptr       class_type,
                                                   a_base_class_ptr bcp,
                                                   a_base_class_ptr ctor_bcp);

extern a_variable_ptr primary_vtbl_var_for_class_if_any(a_type_ptr class_type);

extern a_variable_ptr primary_vtbl_var_for_class(a_type_ptr class_type);

extern a_boolean inline_virtual_function_definitions_needed(
                                                        a_type_ptr class_type);

extern a_boolean typeinfo_goes_out_where_vtable_goes_out(a_type_ptr class_type,
                                                         a_boolean  *unknown);

#if ABI_COMPATIBILITY_VERSION < 238
extern a_boolean external_typeinfo_will_be_defined_for_class(
                                                        a_type_ptr class_type);
#endif /* ABI_COMPATIBILITY_VERSION < 238 */

extern void add_to_return_memo_list(a_statement_ptr return_stmt);

extern void free_return_memo_list(a_return_memo_ptr rmp);

extern a_dynamic_init_ptr normalize_cleanup_state_for_outer_lifetimes(
                                             a_dynamic_init_ptr cleanup_state);

extern void set_curr_cleanup_state_to_latest_initialization(void);

extern void turn_statement_into_block(a_statement_ptr        statement,
                                      an_insert_location_ptr insert_location,
                                      a_statement_ptr        *orig_statement);

extern void turn_statement_into_block_transferring_pragma(
                                        a_statement_ptr        statement,
                                        an_insert_location_ptr insert_location,
                                        a_statement_ptr        *orig_statement,
                                        a_scope_ptr            scope);

extern void put_block_around_try_block(a_statement_ptr        statement,
                                       an_insert_location_ptr insert_location,
                                       a_statement_ptr        *orig_statement);

extern void turn_branch_into_block(a_statement_ptr        statement,
                                   an_insert_location_ptr insert_location,
                                   a_statement_ptr        *orig_statement);

extern a_context_ptr context_for_lifetime(an_object_lifetime_ptr lifetime);

extern void gen_cleanup_actions(an_object_lifetime_ptr outer_lifetime,
                                an_insert_location_ptr insert_location);

extern void prelower_class_type(a_type_ptr class_type);

extern void lower_ptr_to_member_constant(a_constant_ptr constant);

#if ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS
extern void rewrite_address_of_string_as_address_of_variable(
                                                      a_constant_ptr constant);
#endif /* ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS */

extern void lower_constant(a_constant_ptr constant);

extern a_type_ptr type_of_cctor_param_after_adding_indirection(
                                                         a_param_type_ptr ptp);

extern void add_indirection_to_cctor_param_type(a_param_type_ptr ptp);

extern a_boolean should_drop_const_on_this_param_variable(
                                                   a_routine_ptr routine,
                                                   a_type_ptr    routine_type);

extern void lower_os_type(a_type_ptr type);

extern void lower_os_constant(a_constant_ptr constant);

extern void lower_expr_list(an_expr_node_ptr expr_list,
                            unsigned int     is_bool_controlling_expr_mask,
                            unsigned int     assume_expr_is_non_null_mask,
                            a_boolean        eval_right_to_left);

extern unsigned int expr_boolean_controlling_expr_mask(an_expr_node_ptr expr);

extern void lower_reuse_value_expr(an_expr_node_ptr expr);

extern void lower_builtin_operation(an_expr_node_ptr expr);

extern a_boolean is_ptr_to_member_function_constant_expr(
                                                        an_expr_node_ptr expr);

extern a_boolean is_constant_valued_expression(
                                          an_expr_node_ptr expr,
                                          a_boolean        local_vars_change,
                                          a_boolean        other_vars_change,
                                          a_boolean        this_cannot_be_null,
                                          a_boolean        *is_non_null);

extern a_boolean bool_value_is_known_at_compile_time(
                                          an_expr_node_ptr expr,
                                          a_boolean        this_cannot_be_null,
                                          a_boolean        *value);

extern void lower_logical_operator(an_expr_node_ptr expr);

extern void lower_question_operator(an_expr_node_ptr expr,
                                    a_boolean        assume_expr_is_non_null);

extern void lower_comma(an_expr_node_ptr expr);

extern void lower_expr_full(an_expr_node_ptr expr,
                            a_boolean        assume_expr_is_non_null);

/*
Define a macro for the typical invocation of lower_expr_full.
*/
#define lower_expr(expr) lower_expr_full((expr), FALSE)

extern void perform_post_pass_on_lowered_expression(an_expr_node_ptr expr);

extern void lower_full_expr(an_expr_node_ptr expr,
                            a_statement_ptr  statement);

extern void end_of_full_expr_processing(an_expr_node_ptr expr);

extern void normalize_boolean_controlling_expr_if_needed(
                                                       an_expr_node_ptr expr);

extern an_expr_node_ptr boolean_controlling_expr(an_expr_node_ptr expr);

extern a_param_type_ptr unlowered_param_type_list_for_routine(
                                                  a_routine_ptr routine);

extern a_param_type_ptr param_type_for_this(a_type_ptr routine_type);

extern void lower_arg_expr_list(an_expr_node_ptr   expr_list,
                                a_type_ptr         called_rout_type,
                                a_routine_ptr      called_rout,
                                a_param_type_ptr   param,
                                a_boolean          maintain_sequencing,
                                a_boolean          eval_right_to_left,
                                an_expr_node_ptr   conflict_node,
                                an_insert_location *insert_location);

extern void lower_dynamic_cast(an_expr_node_ptr expr);

extern void lower_bool_cast(an_expr_node_ptr expr);

extern void lower_bool_incr_decr(an_expr_node_ptr expr);

extern void rewrite_compound_assignment(an_expr_node_ptr expr);

extern void lower_virtual_function_call(an_expr_node_ptr expr);

extern void lower_call(an_expr_node_ptr      expr,
                       an_init_pos_descr_ptr ipdp,
                       a_statement_ptr       statement,
                       a_boolean             *expr_has_been_detached);

extern void initial_processing_on_destructible_initialization(
                                          a_dynamic_init_ptr dip,
                                          an_insert_location *insert_location);

extern void begin_object_lifetime(
                              an_object_lifetime_ptr lifetime,
                              an_insert_location     *insert_location);

extern void begin_block_object_lifetime(
                                       an_object_lifetime_ptr lifetime,
                                       an_insert_location_ptr insert_location);

extern void reinsert_for_loop_initialization(
                                      a_statement_ptr    init_stmt,
                                      an_insert_location *insert_location);

extern void lower_asm_statement(a_statement_ptr statement);

extern void lower_statement_list(a_statement_ptr statement_list,
                                 a_statement_ptr *last_statement);

extern void lower_block_statement(
                      a_statement_ptr                 statement,
                      a_boolean                       is_block_of_function_try,
                      a_boolean                       is_block_of_stmt_expr,
                      a_destructor_wrapper_info_block *dtor_info,
                      a_statement_ptr                 *last_statement);

extern void lower_statement(a_statement_ptr statement);

extern void externalize_source_correspondence(
                                       a_source_correspondence *scp,
                                       a_boolean               is_variable);

#if ONE_INSTANTIATION_PER_OBJECT
extern void make_statics_referenced_from_instantiations_external(void);
#endif /* ONE_INSTANTIATION_PER_OBJECT */

extern void lower_file_scope(void);

extern void lower_function_scope(a_routine_ptr	routine,
                                 a_scope_ptr	scope);

extern void eliminate_expr_object_lifetime(an_expr_node_ptr expr);

extern void clean_up_all_object_lifetimes(a_scope_ptr scope);

#if DEBUG
extern unsigned long show_lowering_space_used(void);

extern unsigned long compute_checksum_for_expr(an_expr_node_ptr expr);

extern unsigned long compute_checksum_for_statement(a_statement_ptr statement);

extern void db_context(a_context_ptr context);

extern void db_context_stack(void);
#endif /* DEBUG */

extern void function_lower_init(void);

extern void il_lower_one_time_init(void);

extern void il_lower_trans_unit_init(void);

extern void il_lower_init(void);

extern void clear_parent_information(void);

extern an_expr_node_ptr rvalue_pointer_for_class_rvalue(an_expr_node_ptr expr);

extern an_expr_node_ptr rvalue_pointer_for_class_expression(
                                                        an_expr_node_ptr expr);

extern a_boolean type_has_param_passed_via_cctor(a_type_ptr tp);

extern a_type_ptr cast_type_for_param_passed_via_cctor(a_type_ptr source,
                                                       a_type_ptr dest);

extern a_boolean constant_must_remain_in_function_scope(
                                                     a_constant_ptr  constant);

extern a_variable_ptr assoc_var_for_constant(a_constant_ptr constant,
                                             a_boolean      const_okay);

extern void prelower_aggregate_constant(a_constant_ptr constant);

extern a_boolean check_for_troublesome_aggregate_constant(
                                                   a_constant_ptr constant,
                                                   a_variable_ptr *temp_var);
#if LOWER_IFUNC
extern a_variable_ptr make_ifunc_resolver_var(a_routine_ptr rp);

extern void lower_ifunc_expr(an_expr_node_ptr expr);
#endif /* LOWER_IFUNC */

extern void lower_gnu_statement_expression(an_expr_node_ptr expr);

extern an_expr_node_ptr make_class_lvalue_from_var(a_variable_ptr var);

/*
Macro that returns TRUE if the type specified by tp contains a function type
with a parameter type that is passed via a copy constructor.  Such parameter
types are rewritten during lowering to use a pointer (see
add_indirection_to_cctor_param_type).  Cases where the copy constructed
parameter is cv-qualified result in lowered function types that are
incompatible with cv-qualified destinations in initializations, assignments and
function calls.  Note that this test is sub-optimal in that it only checks for
parameters being passed via copy constructor, not for ones that are also
cv-qualified.  This is because the cv-qualifiers may already have been removed
during lowering.  The result is that we may add a cast where none is necessary.
Casts are not needed for pointer to member function types.
*/
#define needs_cast_because_type_has_param_passed_via_cctor(tp)  \
  (!C_mode() && !make_all_functions_unprototyped &&             \
   !is_or_was_ptr_to_member_function_type(tp) &&                \
   type_has_param_passed_via_cctor(tp))                         \

extern a_boolean expr1_could_affect_expr2(an_expr_node_ptr expr1,
                                          an_expr_node_ptr expr2);

extern void overwrite_type_with_new_type(a_type_ptr type,
                                         a_type_ptr new_type);

#endif /* DO_IL_LOWERING */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* NEED_NAME_MANGLING */

#endif /* ifndef LOWER_IL_H */

