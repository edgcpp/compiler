/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

scope_stk.h - Declarations related to management of the scope stack and
              related routines.

*/

/* Avoid including these declarations more than once. */
#ifndef SCOPE_STK_H
#define SCOPE_STK_H 1

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
Forward declarations needed:
*/
typedef struct an_active_using_directive *an_active_using_directive_ptr;
typedef struct an_expr_stack_entry an_expr_stack_entry_dummy_typedef;
typedef struct a_class_def_state a_class_def_state_dummy_typedef;
typedef struct a_class_fixup *a_class_fixup_ptr;
typedef struct a_pack_expansion_descr *a_pack_expansion_descr_ptr;
typedef struct a_pack_instantiation_descr *a_pack_instantiation_descr_ptr;
struct an_ovl_res_stack;

/*
Option flags passed to the push_scope routines.
*/
typedef int a_push_scope_options_set;
#define PS_NO_OPTIONS			0x00
#define PS_MICROSOFT_SPECIALIZATION	0x01
			/* The scope being pushed is a template instantiation
			   scope that is pushed around a class or class
			   reactivation scope in Microsoft mode to make the
			   template parameters visible.  This is also used
			   in Sun mode. */
#define PS_PROTOTYPE_INSTANTIATION	0x02
			/* The scope being pushed is the template instantiation
			   scope for a prototype instantiation. */
#define PS_NONREAL_INSTANTIATION	0x04
			/* The scope being pushed in a template instantiation
			   in which the template arguments are template
			   dependent.  Only used for certain default template
			   argument cases and, in Microsoft mode, instantiation
			   of certain nonreal classes. */
#define PS_IS_REACTIVATION		0x08
			/* TRUE to indicate that a file scope is being
			   reactivated. */
#define PS_IGNORE_CLASS_CONTEXT		0x100
			/* TRUE if when pushing a template instantiation scope,
			   the class definition context should be ignored
			   during normal lookups. */
#define PS_IS_TEMPLATE_PARAM_RESCAN	0x200
			/* TRUE for a template declaration scope pushed for the
			   rescan of a dependent template template
			   parameter. */
#define PS_FORCE_DECL_SEQ_CHECK		0x400
			/* TRUE if, for a template instantiation scope,
			   declaration sequence numbers should be checked
			   during the instantiation context lookup even in
			   modes where such checks would not normally be
			   done. */
#define PS_DEDUCTION_CONTEXT		0x800
			/* TRUE if the in_template_deduction_context flag
			   should be set for the instantiation scope being
			   pushed. */
#define PS_IS_RESCAN			0x1000
			/* TRUE if the scope being pushed is an instantiation
			   scope for template rescan purposes. */
#define PS_GENERIC_DEFINITION		0x2000
			/* TRUE if the scope being pushed is the template
			   instantiation scope for a C++/CLI generic
			   definition. */
#define PS_CLASS_DEFINITION_CONTEXT	0x4000
			/* TRUE when push_template_instantiation_scope is
			   being used to push the context for loading
			   a class definition from get_definition_of_class.
			   This indicates that there may not actually be
			   an instantiation scope pushed. */
#define PS_NOT_FINAL_POP		0x8000
			/* This is used when popping a block scope to
			   indicate that certain operations, such as the
			   end-of-scope symbol check, should be suppressed
			   because the scope will be reactivated. */
#define PS_FUNCTION_PARTIAL_INSTANTIATION \
					0x10000
			/* TRUE when rescanning a function template declaration
			   from tokens to create the partial instantiation of
			   the function. */
#define PS_EXCEPTION_SPEC		0x20000
			/* TRUE when instantiating an exception
			   specification. */
#define PS_NEW_ACCESS_CONTEXT		0x40000
			/* TRUE for a class reactivation scope if the scope
			   should be considered a new access context. */
#define PS_NEW_INSTANTIATION_CONTEXT	0x80000
			/* TRUE if any enclosing template instantiation
			   contexts should be ignored (i.e., as if the
			   enclosing scope was the file scope).  This means
			   that depth_innermost_instantiation_scope will be
			   NO_SCOPE_DEPTH, for example. */
#define PS_ALIAS_IN_TEMPLATE_DECL	0x100000
			/* TRUE when an alias template is instantiated in
			   a template declaration scope with dependent template
			   arguments. */
#define PS_IS_GENERIC_LAMBDA		0x200000
			/* TRUE for an instantiation of a generic lambda or
			   a context scope pushed for a generic lambda. */
#define PS_IS_SPECIALIZATION		0x400000
			/* TRUE if any class scopes that are pushed should be
			   treated as specializations for name lookup
			   purposes. */
#define PS_IS_TEMPLATE_TEMPLATE_PARAM	0x800000
			/* TRUE for a template declaration scope pushed for the
			   template parameters of a template template
			   parameter. */
#define PS_IS_DEFAULT_TEMPLATE_ARG	0x1000000
			/* TRUE for an instantiation of a default template
			   argument. */
#define SIZE_FUNCTION_SHAREABLE_CONSTANTS_TABLE 31
			/* Size of the shareable constants hash table for
			   a function. */

/*
A structure used to represent a shareable constant table for the constants
associated with a given function.
*/
typedef struct a_function_shareable_constants_table
		*a_function_shareable_constants_table_ptr;
typedef struct a_function_shareable_constants_table {
  a_function_shareable_constants_table_ptr
		next;	/* Pointer to the next table on the available list. */
  a_constant_ptr
		table[SIZE_FUNCTION_SHAREABLE_CONSTANTS_TABLE];
			/* The hash table for a given function. */
} a_function_shareable_constants_table;


typedef unsigned long a_pending_class_definition_count;

/*
Entry used to keep track of the inline functions, default arguments, and
in-class initializers that must be fixed up.  A list is kept for each function
scope, and a global scope list for non-local classes.
*/
typedef struct a_class_fixup_header *a_class_fixup_header_ptr;
typedef struct a_class_fixup_header {
  unsigned int	defer_inline_function_fixups;
			/* Nonzero if the fixup of inline function bodies and
                           in-class initializers should be deferred. */
  a_pending_class_definition_count
		pending_class_definitions;
			/* The number of class definitions currently in
			   process.  This includes normal class definitions
			   and template class instantiations. */
  a_class_fixup_ptr
		fixup_list;
			/* Pointer to a list of class fixup entries for class
			   definitions for which default argument fixup,
			   in-class inline function fixup, and/or in-class
			   initializer fixup must be done. */
  a_class_fixup_ptr
		fixup_list_tail;
			/* Pointer to the last entry on the list pointed to by
			   fixup_list, or NULL if that list is empty. */
  a_type_list_entry_ptr
		classes_that_may_need_fixups;
			/* A list of classes that potentially have associated
			   fixups (i.e., classes whose definition has just
			   appeared in the source code). */
} a_class_fixup_header;


/*
Type for a map from a module pointer to a lookup table.
*/
using a_module_lookup_table_map = Ptr_map<a_module_ptr, a_hash_table_ptr>;
using a_module_lookup_table_map_ptr = a_module_lookup_table_map*;


/*
Structure that is logically (and historically) part of a_scope_stack_entry,
but which must persist longer than a scope stack entry for namespace scopes
(since "extension-definitions" are allowed for them).  Therefore,
a_scope_pointers_block is also part of a_namespace_symbol_supplement.  When
an sck_namespace or sck_namespace_extension scope is pushed onto the stack, a
pointer in the scope stack entry is set to refer to the persistent scope
pointers block (the one in the symbol supplement) -- and the one in the scope
stack entry itself is unused.
*/
typedef struct a_scope_pointers_block *a_scope_pointers_block_ptr;
typedef struct a_scope_pointers_block {
  a_symbol_ptr	symbols;
			/* Pointer to the head of a linked list of all symbols
			   declared in this scope (linked by the field
			   next_in_scope); NULL if there are no such
			   declarations. */
  a_symbol_ptr	synth_namespace_projection_symbols;
			/* Pointer to the head of a list of synthesized
			   projection symbols created in this scope.
			   These are linked by the next_in_scope field in
                           the symbol. */
  a_symbol_ptr	last_symbol;
			/* End of the symbol list pointed to by symbols. */
  a_constant_ptr
		last_constant;
			/* End of list of named constants of this scope,
			   NULL if none. */
  a_type_ptr	last_type;
			/* End of list of local types of this scope, NULL if
			   none. */
  a_variable_ptr
		last_variable;
			/* End of list of local variables of this scope, NULL
			   if none. */
  a_routine_ptr	last_routine;
			/* End of list of local routines of this scope, NULL
			   if none.  Includes both routines with definitions
			   and those that are just declarations of interfaces
			   to external routines. */
  an_asm_entry_ptr
		last_asm_entry;
			/* End of list of asm entries of this scope, NULL if
			   none. */
  a_dynamic_init_ptr
		last_dynamic_init;
			/* End of list of dynamic initializations for this
			   scope, NULL if none.  Only the file scope has
			   a dynamic initializations list. */
  a_namespace_ptr
		last_namespace;
			/* End of list of namespace entries in this scope,
			   NULL if there are none. */
  a_using_decl_ptr
		last_using_declaration,
		last_using_directive;
			/* End of lists of using-declaration and
                           using-directive entries in this scope; NULL if
			   there are none. */
  a_pragma_ptr	last_pragma;
			/* End of list of IL pragma entries entered on the
			   pragma_list of il_scope, NULL if none. */
#if RECORD_HIDDEN_NAMES_IN_IL
  a_hidden_name_ptr
		last_hidden_name;
			/* End of the list of hidden-name entries entered on
			   the corresponding IL scope entry; NULL if none. */
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
  a_template_ptr
		last_template;
			/* End of the list of template entries entered on
			   the corresponding IL scope entry; NULL if none. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  an_ms_attribute_ptr
		last_ms_attribute;
			/* End of the list of Microsoft attribute entries
			   entered on the corresponding IL scope entry; NULL
			   if none. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
  an_ms_if_exists_ptr
		last_ms_if_exists;
			/* End of the list of Microsoft __if_exists entries
			   entered on the corresponding IL scope entry; NULL
			   if none. */
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_source_sequence_entry_ptr
		last_source_sequence_entry;
			/* End of the list of source sequence entries entered
			   on the corresponding IL scope entry; NULL if none.
			   Used only for the file scope, and updated only as
			   the file scope is popped from the scope stack;
			   the scope stack end_of_source_sequence_list field
			   should be used at other times. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  a_symbol_ptr	unnamed_namespace_sym;
			/* For sck_file and sck_namespace scopes only, pointer
			   to the symbol representing the unnamed namespace
			   for the current scope; NULL if there is none. */
  a_namespace_list_entry_ptr
		inline_namespaces;
			/* A list of the inline namespaces in this namespace.
			   This is also used for the list of namespaces that
			   used this namespace via a GNU strong
			   using-directive.  Such namespaces are considered
			   associated namespaces for lookups for which this
			   namespace is an associated namespace. */
  a_hash_table_ptr
		lookup_table;
			/* Some scopes have an associated hash table to aid
			   in name lookup.  This is non-NULL if such a table
			   has been created for the associated scope.  The
			   table is only created when the first entry has been
			   added to the table.  Some scopes can have multiple
			   tables for different namespaces.  For more
			   information see module_lookup_table_map below. */
  a_module_lookup_table_map_ptr
		module_lookup_table_map;
			/* For namespace scopes (including the file scope),
			   there is a separate lookup table for each module
			   that uses that scope.  This is a map from a module
			   to a lookup table. */
  a_bit_field	add_symbols_to_inactive_list:1;
			/* TRUE for sck_namespace_reactivation scopes if
			   symbols added to the scope should be added
			   directly to the inactive list, instead of being
			   added to the active list as is usually done. */
} a_scope_pointers_block;


/*
Value that identifies the kind of entity represented by a pack reference.
*/
enum a_pack_reference_kind {
  prk_template_param,	/* A template parameter pack */
  prk_variable,		/* An argument pack represented by a parameter
			   variable. */
  prk_parameter,	/* An argument pack represented by a parameter
			   symbol. */
  prk_binding,		/* An argument pack represented by a structured
			   binding. */
  prk_init_capture,	/* An init-capture that is a pack. */
  prk_bases		/* A generated pack that represents a g++ __bases
			   or __direct_bases trait. */
};


/*
Entry used to construct a list of pack symbols that have been referenced
in a variadic pack expansion context.  This entry is used both during
the prototype instantiation and during a real instantiation.  During
a real instantiation, a copy of the entry from the prototype instantiation
is made and additional information about the current values of the parameter
packs is maintained.
*/
typedef struct a_pack_reference *a_pack_reference_ptr;
typedef struct a_pack_reference {
  a_pack_reference_ptr
		next;
			/* The next entry on the list or NULL for the last
			   entry. */
  a_symbol_ptr	symbol;
			/* The symbol of the pack that was referenced.  For
			   references to function parameter packs (i.e., when
			   the symbol points to a variable), this variable
			   pointer is cleared when the end of the prototype
			   instantiation of the function is reached
			   because the variable pointed to will be in the
			   function memory region.  NULL for prk_bases
			   entries. */
  uint32_t	param_or_binding_num;
			/* This is used for function parameter or structured
			   binding packs to record the parameter or structured
			   binding number of the pack, respectively. */
  a_source_position
		position;
			/* The source position of the pack reference. */
  a_token_sequence_number
		token_sequence_number;
			/* The token sequence number associated with the
			   pack reference. */
  a_pack_reference_kind
		kind;
			/* Specifies the kind of entity to which this
			   entry refers. */
  a_symbol_ptr	primary_pack_symbol;
			/* When kind == prk_variable, prk_binding,
			   prk_parameter, or prk_init_capture in an actual
			   instantiation, this points to the variable,
			   parameter, or field symbol that is found by name
			   lookup. */
  uint32_t	function_or_block_scopes_to_skip;
			/* When kind == prk_variable or prk_binding, this
			   indicates the number of function or block scopes to
			   be bypassed to look for the matching parameter or
			   structured binding variable. */
  a_variadic_param_info_ptr
		param_info;
			/* When kind == prk_parameter and this is a rescan
			   context, this points to the variadic parameter
			   information entry to be used for the current
			   expansion.  This can be NULL in contexts in which
			   it is not possible to refer to the parameter
			   (e.g., deduction contexts). */
  a_template_param_coordinate_ptr
		coordinates;
			/* When kind == prk_template_param, this points to
			   the coordinates of the parameter. */
  a_template_param_ptr
		template_param;
			/* When kind == prk_template_param, this points to
			   the template parameter.  This is only set for
			   pack references created for instantiations. */
  union {
    a_variable_ptr
		variable;
			/* When kind == prk_variable or prk_binding, this
			   points to the variable to be used for the current
			   expansion. */
    a_param_type_ptr
		param_type;
			/* When kind == prk_parameter and this is a rescan
			   context, this points to the param type
			   to be used for the current expansion.  This can
			   be NULL in contexts in which it is not possible
			   to refer to the parameter (e.g., deduction
			   contexts). */
    a_param_id_ptr
		param_id;
			/* When kind == prk_parameter and this is not a rescan
			   context, this points to the parameter to
			   be used for the current expansion. */
    a_field_ptr
		field;
			/* When kind == prk_init_capture this points to the
			   field entry of the closure class that represents
			   the init-capture to be used for the current
			   expansion. */
    a_template_arg_ptr
		template_arg;
			/* When kind == prk_template_param or prk_bases, this
			   points to the template argument entry to be used
			   for the current expansion. */
  } curr_argument;
  a_template_arg_ptr
		prev_template_arg;
			/* This is used during deduction to point to the
			   previous template argument of a template
			   parameter pack.  Initially, this points to the
			   placeholder.  Later it points to the last of the
			   arguments deduced so far. */
  a_byte_boolean
		uses_enclosing_pack;
			/* TRUE if this is a reference to a pack from an
			   enclosing template context.  This is the case
			   when, for example, a template parameter from
			   an outer template is used in a nested template. */
  a_byte_boolean
		direct_bases;
			/* For a prk_bases entry, this is TRUE if the
			   entry is for a __direct_bases, FALSE otherwise. */
} a_pack_reference;


/*
Structure used for variadic templates to record information about potential
pack expansion contexts.
*/
typedef struct a_pack_expansion_descr {
  a_pack_expansion_descr_ptr
		next;
			/* The next entry on a list of pack expansion entries,
			   or NULL for the last entry on the list. */
  a_pack_expansion_descr_ptr
		previous;
			/* The previous entry on the list, or NULL for the
			   first entry. */
  a_token_sequence_number
		first_token;
			/* Identifies the first token of the range of tokens
			   to be rescanned for an expansion of the pack. */
  a_token_sequence_number
		last_token;
			/* Identifies the last token of the range of tokens
			   to be rescanned for an expansion of the pack. */
  a_pack_reference_ptr
		packs_referenced;
			/* A list of the parameter packs used within the pack
			   expansion.  This will include template parameter
			   symbols for template parameter packs as well as
			   variable symbols for function parameter packs. */
  a_source_position
		ellipsis_position;
			/* If ellipsis_seen is TRUE, this is the position of
			   the ellipsis token; null_source_position
			   otherwise. */
  a_symbol_header_ptr
		param_symbol_header;
			/* For a template parameter declaration that is a pack
			   expansion, this is the symbol header of the
			   parameter.  Also recorded for function parameter
			   packs. */
  a_type_ptr	param_symbol_type;
			/* For a template type parameter declaration that is a
			   pack expansion, this is the type of the
			   parameter; NULL otherwise. */
  int		tentative_pack_expansion_depth;
			/* The number of nested tentative scans of function
			   declarators in process at the time this pack
			   expansion descriptor was recorded.  Used to
			   determine if this descriptor should be removed
			   in case the scan is to be repeated.  If a
			   repeated scan is not necessary, this value will
			   be reset to 0.  See
			   begin/end_tentative_pack_expansion_context. */
  a_bit_field	ellipsis_seen:1;
			/* TRUE if the ellipsis marking a pack expansion
			   has been encountered.  This is primarily used for
			   the declarator case where the "..." is not
			   necessarily at the end. */
  a_bit_field	is_function_declarator:1;
			/* This field is used by declarator processing to
			   save the disambiguation result between a function
			   declarator and a parenthesized initializer so that
			   during an actual instantiation the zero-trip case
			   can be handled properly. */
  a_bit_field	uses_only_enclosing_packs:1;
			/* TRUE if all of the pack references are to packs
			   from enclosing templates.  These must be expanded
			   during the declaration of a nested template. */
  a_bit_field	uses_any_enclosing_packs:1;
			/* TRUE if any of the pack references are to packs
			   from enclosing templates. */
  a_bit_field	is_pack_index:1;
			/* TRUE if this pack expansion describes a C++26
			   pack-index construct (T...[N] or id...[N]). */
  a_bit_field	param_symbol_is_template_template:1;
			/* For a template template parameter declaration
			   that is a pack expansion, this is TRUE (in which
			   case param_symbol_type is NULL). */
} a_pack_expansion_descr;


/*
Structure used for variadic templates to track information about actual
pack instantiations.
*/
typedef struct a_pack_instantiation_descr {
  a_pack_instantiation_descr_ptr
		next;
			/* The next entry on a list of pack instantiation
			   entries, or NULL for the last entry on the list. */
  a_pack_reference_ptr
		pack_status;
			/* A list of the parameter packs used within the pack
			   expansion including information about the current
			   element to which each pack refers. */
  a_byte_boolean
		after_first_element;
			/* TRUE if the current element is the 2nd through
			   Nth element of the instantiation. */
  a_byte_boolean
		is_empty;
			/* TRUE if there were no elements in the pack(s) to
			   be expanded.  Always FALSE for deduction
			   contexts. */
  a_byte_boolean
		has_hybrid_pack_expansion;
			/* TRUE if this is a "hybrid pack expansion" that
			   contains both an expansion from an enclosing real
			   instantiation and an unexpanded pack from a nested
			   generic lambda. */
} a_pack_instantiation_descr;


/*
Entry used to maintain a stack of variadic template pack expansions.
*/
typedef struct a_pack_expansion_stack_entry {
  a_pack_expansion_stack_entry_ptr
		next;
			/* The next entry on the pack expansion stack, or
			   NULL if this is the bottom of the stack. */
  a_pack_expansion_descr_ptr
		expansion_descr;
			/* A pointer to the entry that describes this pack
			   expansion.  During prototype instantiations this
			   points to an entry that is being constructed to
			   describe a potential pack expansion context.  The
			   entry will be freed later if this is not actually
			   a pack expansion.  During a real instantiation,
			   this points to a pack expansion descriptor created
			   during the prototype instantiation. */
  a_pack_instantiation_descr_ptr
		instantiation_descr;
			/* During a real instantiation, this points to
			   information about the actual parameter packs being
			   used for the instantiation.  NULL during prototype
			   instantiations. */
  a_reusable_token_cache
		first_token_cache;
			/* During a real instantiation, this is the token cache
			   for the token that starts pack expansion.  This is
			   used to reset the token position to scan the
			   non-initial pack elements.  This is not used (is a
			   default-constructed token cache iterator) when
			   is_rescan is TRUE. */
  a_token_sequence_number
		first_token_tsn;
			/* During a real instantiation, this is the starting
			   token sequence number for the token at the start of
			   the pack expansion.  This is used to reset the token
			   position to scan the non-initial pack elements.
			   This is not used (is a default-constructed token
			   cache iterator) when is_rescan is TRUE. */
  a_template_arg_ptr
		template_arg_list;
			/* In rescan contexts, a copy of the supplied
			   template argument list is made.  This points to
			   that copy of the list.  NULL otherwise. */
  a_byte_boolean
		is_rescan;
			/* TRUE when the expansion is being done in an
			   expression rescan context.  In such contexts the
			   current values of the packs are maintained, but
			   no token manipulation or checking is done. */
  a_byte_boolean
		is_deduction;
			/* TRUE when the expansion is being done in an
			   template argument deduction context.  In such
			   contexts a new template argument is created for
			   each pack element produced by the deduction
			   process, but no token manipulation or checking
			   is done. */
  a_byte_boolean
		is_suppression;
			/* TRUE when the processing of expansion contexts
			   is being suppressed.  This only suppresses
			   expansions from tokens, not rescans. */
  a_byte_boolean
		expansion_with_no_packs_diagnostic_issued;
			/* TRUE if suppress_expansion_with_no_packs_diagnostic
			   was called to indicate that the caller already
			   issued a diagnostic for an expansion with no
			   packs. */
  a_byte_boolean
		is_lookahead;
			/* TRUE if this context is being pushed to
			   distinguish between two contexts.  When this
			   is TRUE, it is assumed that another begin...
			   call will be done for the same starting position,
			   and that context will be responsible for the
			   end... and advance... calls. */
  a_byte_boolean
		enclosing_packs_reset;
			/* TRUE for a pack expansion of a template declaration
			   in a real instantiation (see call of
			   reset_enclosing_pack_values in
			   begin_potential_pack_expansion_context_full for
			   more information). */
  a_boolean
		preserve_deduced_packs;
			/* TRUE if is_rescan is TRUE and deduced parameter
			   packs should be retained in the substituted type. */
  a_byte_boolean
		contains_pack_reference;
			/* TRUE during prototype instantiations if any
			   pack references were recorded while this entry
			   was at the top of the pack expansion stack. */
} a_pack_expansion_stack_entry;


/*
Entry identifying a symbol that is visible in a given scope but would not
be if old (cfront-compatible) for-init declaration scoping rules were used.
*/
typedef struct a_name_hidden_by_old_for_init
                                     *a_name_hidden_by_old_for_init_ptr;
typedef struct a_name_hidden_by_old_for_init {
  a_name_hidden_by_old_for_init_ptr
		next;
			/* Next in a list of name_hidden_by_old_for_init
			   entries for a given scope; NULL for the last in
			   the list. */
  a_symbol_ptr	symbol;
			/* Pointer to a symbol (from an enclosing scope) for
			   which hidden_by_old_for_init is TRUE. */
  a_symbol_ptr	for_init_decl_sym;
			/* Pointer to a symbol declared a for-init declaration.
			   It has the same name as the other symbol, and under
			   the old rules would have hidden it for the rest of
			   the current scope. */
  a_byte_boolean
		already_hidden;
			/* Value to which hidden_by_old_for_init in symbol
			   should be restored when the current scope is
			   popped. */
} a_name_hidden_by_old_for_init;


/* A set of pointers to constants that might be generated in a scope. */
typedef struct a_generated_entity_block *a_generated_entity_block_ptr;
typedef struct a_generated_entity_block {
  a_variable_ptr
		function_name;
			/* Pointer to a constant string variable holding the
			   name of the function currently being defined.
			   Set only when the appropriate reserved identifier
			   (e.g., __func__) is used.  NULL until then. */
  a_variable_ptr
		pretty_function_name;
			/* Like function_name, but for __PRETTY_FUNCTION__. */
  a_variable_ptr
		decorated_function_name;
			/* Like function_name, but for __FUNCDNAME__
			   (Microsoft mode only). */
} a_generated_entity_block;


#if NEED_NAME_MANGLING
/* Type of a discriminator, which is an identifying number used to
   distinguish multiple entities with the same name in the same function
   in the name mangling for the IA-64 ABI. */
typedef unsigned long
                a_discriminator;

/*
A hash table type to detect name collisions between declarations in function
scope.  The type is defined in scope_stk.c.
*/
typedef union a_collision_table *a_collision_table_ptr;
#endif /* NEED_NAME_MANGLING */

#if DO_IL_LOWERING && ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS
/*
A hash table used to assign sequence numbers to each unique string literal
used within a function.  The type is defined in scope_stk.c.
*/
typedef struct a_string_literal_table *a_string_literal_table_ptr;

void f_assign_string_literal_sequence_number(void);
#endif /* DO_IL_LOWERING && ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS */

using a_template_cache_segment_list = Dyn_array<a_template_cache_segment_ptr>;
			/* The type used for a list of template cache
			   segments. */

/* Scope stack, containing an entry for each currently-active scope. */
typedef struct a_scope_stack_entry *a_scope_stack_entry_ptr;
typedef struct a_scope_stack_entry {
  a_scope_number
		number;
			/* Scope number (unique identifier) for this scope. */
  a_scope_kind	kind;
			/* Kind of scope (file, function, block, function
			   prototype, etc.).  See the definition of
			   a_scope_kind in il_def.h. */
  ENUM_TYPE_FOR_BIT_FIELD(an_access_specifier)
		current_access:2;
			/* The access control specification that currently
			   prevails for declarations in the current scope;
			   as_public by default, but may be otherwise for
			   C++ class definitions.  (For instance, if an
                           enumeration is defined as a member type of a class,
			   the access to be applied to the enumeration
			   constants may be derived from the setting of this
			   field.) */
#if MICROSOFT_EXTENSIONS_ALLOWED
  ENUM_TYPE_FOR_BIT_FIELD(an_access_specifier)
		current_assembly_access:2;
			/* The assembly access that currently prevails for
			   declarations in the current scope: as_protected for
			   assembly family access, as_private for assembly
			   access, and as_public for universal access.
			   (C++/CLI only.) */
  a_bit_field	defer_constraint_checks:1;
			/* TRUE if checking of generic constraints should be
			   deferred and performed later.  This is used to
			   defer checking of constraints of base-specifiers
			   and generic "where" clauses. */
  a_bit_field	scanning_cli_delegate_definition:1;
			/* TRUE if we are currently scanning the definition of
			   a C++/CLI delegate type. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_bit_field	inactive_symbols_may_be_visible:1;
			/* TRUE if the scope stack to this depth contains any
			   class reactivation entries or class entries for
			   classes with base classes.  In either case,
			   symbols on a symbol header's inactive list may be
			   visible from the current scope. */
  a_bit_field	inside_local_class:1;
			/* TRUE if the current scope level is that of a local
			   class or is (logically) within the scope of a local
			   class.  Once this flag is set it is usually
			   propagated each time a new scope is pushed onto
			   the stack; the exception is when a template
			   instantiation scope is pushed, in which case the
			   flag is cleared. */
  a_bit_field	template_param_decl_scope:1;
			/* TRUE if this is the first scope that
			   affects the declarative level after a template
			   instantiation scope. */
  a_bit_field	is_loop_scope:1;
			/* TRUE if this scope is associated with the compound
			   statement of a for, do, while, or "for each"
			   loop. */
  a_bit_field	is_dissociated_from_loop_scope:1;
			/* TRUE for for-init scopes and loop condition scopes
			   that are dissociated from the loop scopes within
			   them because of a nested for-statement.  Also set
			   in "for each" loops.  Used in Microsoft mode
			   only. */
  a_bit_field	slow_lookup_required:1;
			/* TRUE if this is a scope for which a slow lookup
			   is required because the scope stack contains a
			   scope in which certain symbols on the active list
			   must not be visible. */
  a_bit_field	return_value_optimization_possible:1;
			/* TRUE if this scope is a function scope and named
			   return value optimization (NRVO) is possible for the
			   routine.  That is, the routine returns a class value
			   via a copy constructor, and all return statements
			   return a single local variable. */
  a_bit_field	in_prototype_instantiation:1;
			/* TRUE if kind is sck_template_instantiation and
			   what is being instantiated is the prototype for a
			   class template.  Also true for scopes nested within
			   a prototype instantiation. */
  a_bit_field	in_nonreal_instantiation:1;
			/* TRUE for instantiations based on template-dependent
			   template arguments and for rescan operations (to
			   implement C++11 SFINAE rules) where template-
			   dependent constructs may arise. */
  a_bit_field	in_generic_definition:1;
			/* TRUE if kind is sck_template_instantiation and
			   what is being instantiated is the definition of a
			   C++/CLI generic.  Also TRUE for scopes nested within
			   a generic definition. */
  a_bit_field	alias_in_template_decl:1;
			/* TRUE if kind is sck_template_instantiation and this
			   is an alias template being instantiated with
			   dependent template arguments in a template
			   declaration scope. */
  a_bit_field	exception_specification:1;
			/* TRUE if this is a scope within the instantiation
			   of an exception specification. */
  a_bit_field	is_default_template_arg:1;
			/* TRUE if this is a scope within the instantiation
			   of a default template argument. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	instantiation_from_metadata:1;
			/* TRUE for instantiation scopes for C++/CLI generic
			   entities that were imported from metadata. */
  a_bit_field	in_generic_instantiation:1;
			/* TRUE if kind is sck_template_instantiation and
			   a C++/CLI generic is being instantiated (but not
			   TRUE when in_generic_definition is TRUE).  Also
			   TRUE for scopes nested within a generic
			   instantiation. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_bit_field	in_class_specialization:1;
			/* TRUE for scopes that are template class explicit
			   specialization scopes or scopes nested within such
			   scopes.  This is typically used to identify
			   contexts within prototype instantiations that
			   represent explicit instantiations (which must be
			   handled specially in certain contexts). */
  a_bit_field	in_template_deduction_context:1;
			/* TRUE if we are in a context in which an expression
			   is being scanned that could later potentially
			   participate in template argument deduction
			   and/or template argument substitution into an
			   expression. */
  a_bit_field	in_variadic_template:1;
			/* TRUE if we are in the context of a variadic
			   template.  This is TRUE both in the context of the
			   original definition of the template and in actual
			   instantiations. */
  a_bit_field	record_form_of_name_reference:1;
			/* TRUE if the form of name references should be
			   recorded in this scope. */
  a_bit_field	record_dependent_name_references:1;
			/* Dependent name references are not usually recorded
			   unless prototype_instantiations_in_il is TRUE.
			   This forces name references to be recorded in
			   dependent contexts. */
  a_bit_field	defer_access_checks:1;
			/* TRUE while scanning the decl-specifiers and
			   declarator of a global or namespace-level
                           declaration.  Access checks for names
			   scanned while this is TRUE cannot be done
			   until the declarator has been scanned. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_bit_field	source_sequence_entries_disallowed:1;
			/* TRUE if the current scope establishes or belongs to
			   a context in which source sequence entries should
			   not be issued -- e.g. a template declaration, a
			   a template instantiation, or a pragma. */
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  a_bit_field	src_seq_entries_from_prototype_instantiation:1;
			/* TRUE if source sequence entries from a prototype
			   instantiations have been recorded in this scope
			   stack entry.  This affects where these entries
			   will be inserted in the enclosing scope's list. */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
  a_bit_field	create_ms_if_exists_entries:1;
			/* TRUE if Microsoft __if_exist entries should be
			   created for this scope.  Such entries are created
			   for __if_exist directives in class scopes.  This
			   flag is also set for function prototype scopes
			   with class scopes so that the use of an __if_exists
			   in that context may be diagnosed. */
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
  a_bit_field	nested_instantiation:1;
                        /* TRUE for a template instantiation scope that
			   is expected to be nested inside of another
			   instantiation scope.  This occurs when a friend
			   template declaration from a class template is
			   being instantiated.  This flag lets name lookups
			   continue on past the nested instantiation scope so
			   that names from the outer instantiation scope can
			   be visible. */
  a_bit_field	is_compound_statement_block:1;
			/* TRUE if the scope is that of a compound
			   statement. */
  a_bit_field	is_try_block:1;
			/* TRUE if the scope is that of the compound statement
			   of a try block (sck_block only).  Note: not set
			   for the scope pushed for a catch clause. */
  a_bit_field	within_try_block:1;
			/* TRUE if is_try_block is TRUE or if this scope is
			   an sck_block scope nested within a scope for which
			   is_try_block is set. */
  a_bit_field	is_catch_in_function_try:1;
			/* TRUE if this is the block scope pushed for a catch
			   clause in a function try block.  Some special error
			   tests are required for variables declared in such
			   blocks. */
  a_bit_field	within_unnamed_namespace:1;
			/* TRUE if the current entry on the scope stack is
			   itself an unnamed namespace or is a named
			   namespace contained within an unnamed namespace. */
  a_bit_field	reactivated_class_being_defined:1;
			/* TRUE for class reactivation scopes if the class
			   being reactivated is in the process of being
			   defined.  This causes the lookup to look on the
			   active list instead of the inactive list for
			   the class members. */
  a_bit_field	is_for_init_block:1;
			/* TRUE if the scope is pushed for a C++ for-init
			   declaration (sck_block only). */
  a_bit_field	namespace_pushed:1;
    		        /* TRUE for class reactivation scopes if the
                           parent namespace was pushed. */
  a_bit_field	exclude_from_context_output:1;
			/* TRUE for scopes that would normally result in
			   the creation of error context information
			   (such as template instantiation scopes),
			   but for which the context information should
			   be suppressed. */
  a_bit_field	instantiation_scope_pushed:1;
			/* TRUE if, when pushing a class and template
			   reactivation scope, a template instantiation
			   scope was pushed. */
  a_bit_field	microsoft_specialization_scope_pushed:1;
			/* TRUE if, when pushing a class and template
			   reactivation scope, a template instantiation
			   scope was pushed for a Microsoft specialization
			   scope.  This is also used in Sun mode. */
  a_bit_field	lexical_state_stack_pushed:1;
			/* TRUE if, when pushing a template instantiation
			   scope, a new lexical state stack entry was pushed.
			   This flag is set in the last scope pushed by
			   push_template_instantiation_scope, which is
			   not necessarily a template instantiation scope. */
  ENUM_TYPE_FOR_BIT_FIELD(a_name_linkage_kind)
		default_name_linkage:NUM_BITS_FOR_NAME_LINKAGE;
			/* The default language linkage (e.g., extern "C++" or
			   extern "C") for declarations in the current scope
			   (used in C++ mode only).  In general, when a scope
			   is pushed, the setting is copied from the enclosing
			   scope; it may then be modified and later restored
			   when a linkage specification is seen.  However,
			   template instantiation scopes take the setting for
			   the template declaration. */
  a_bit_field	name_linkage_is_explicit:1;
			/* TRUE if the default name linkage was explicitly
			   specified in the source; FALSE for the default
			   setting for the translation unit as a whole. */
  a_bit_field	explicitly_declared_namespace_extension:1;
			/* TRUE for sck_namespace_extension scopes that
			   correspond to explicit declarations. */
  a_bit_field	microsoft_specialization_instantiation_scope:1;
			/* TRUE for an sck_template_instantiation scope pushed
			   for compatibility with the Microsoft compiler,
			   which permits the body of a class specialization to
			   reference template parameters of the template.  This
			   is also used in Sun mode. */
  a_bit_field	is_instantiation_context:1;
			/* TRUE for an sck_template_instantiation scope that
			   should be considered to be an instantiation context.
			   This is true for most instantiation scopes, but not
			   for Microsoft specialization scopes that are not
			   enclosed by an instantiation scope. */
  a_bit_field	pragma_pack_is_local:1;
			/* TRUE for an sck_function scope of a routine in
			   which a "#pragma pack" directive is local in
			   effect -- i.e., does not affect the packing of
			   structs declared outside the function body. */
  a_bit_field	is_reactivation:1;
			/* File scopes can be pushed, popped, and then pushed
			   again later.  When generic lambdas are used this is
			   also true of function and other local scopes.  This
			   is TRUE when a scope has been re-pushed. */
#if DO_IL_LOWERING && ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS
  a_bit_field	assign_string_literal_sequence_numbers:1;
			/* TRUE if this is a function scope for which
			   string literal sequence numbers should be
			   assigned. */
#endif /* DO_IL_LOWERING && ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS */
  a_bit_field	discard_when_popped:1;
			/* TRUE if this is a scope that should be discarded 
			   when popped.  Specifically, this is used for
			   function scopes of duplicate definitions of explicit
			   specializations in some Microsoft modes. */
  a_bit_field	in_base_specifier_list:1;
			/* TRUE while scanning a base specifier list. */
  ENUM_TYPE_FOR_BIT_FIELD(a_stdc_pragma_value)
		fp_contract_state:NUM_BITS_FOR_STDC_PRAGMA_VALUE;
  ENUM_TYPE_FOR_BIT_FIELD(a_stdc_pragma_value)
		fenv_access_state:NUM_BITS_FOR_STDC_PRAGMA_VALUE;
  ENUM_TYPE_FOR_BIT_FIELD(a_stdc_pragma_value)
		cx_limited_range_state:NUM_BITS_FOR_STDC_PRAGMA_VALUE;
			/* Saved values of the current state of the C99
			   STDC pragma values.  These are saved when a scope
			   is entered and restored when the scope is left. */
#if FIXED_POINT_ALLOWED
  ENUM_TYPE_FOR_BIT_FIELD(a_stdc_pragma_value)
		fx_full_precision_state:NUM_BITS_FOR_STDC_PRAGMA_VALUE;
  ENUM_TYPE_FOR_BIT_FIELD(a_stdc_pragma_value)
		fx_fract_overflow_state:NUM_BITS_FOR_STDC_PRAGMA_VALUE;
  ENUM_TYPE_FOR_BIT_FIELD(a_stdc_pragma_value)
		fx_accum_overflow_state:NUM_BITS_FOR_STDC_PRAGMA_VALUE;
			/* Saved values of the current state of the fixed-
			   point STDC pragma values.  These are saved when a
			   scope is entered and restored when the scope is
			   left. */
#endif /* FIXED_POINT_ALLOWED */
  a_bit_field	qualified_conversion_operator:1;
			/* TRUE when conversion_parent_type is set and the
			   conversion type was specified using the form
			   "A::operator B". */
  a_bit_field	initial_decl_of_namespace_std:1;
			/* TRUE if this is the first explicit declaration of
			   namespace std.  This flag is needed because the
			   std namespace is predeclared and as a result the
			   first use in the program results in a namespace
			   extension scope stack entry instead of the
			   expected namespace scope stack entry. */
  a_bit_field	ignore_during_normal_lookup:1;
			/* TRUE if this scope should be skipped during normal
			   lookups. */
  a_bit_field	force_decl_seq_check:1;
			/* TRUE if, for a template instantiation scope,
			   declaration sequence numbers should be checked
			   during the instantiation context lookup even in
			   modes where such checks would not normally be
			   done. */
  a_bit_field	outside_parameter_list:1;
			/* TRUE if, for a function prototype scope, the closing
			   parenthesis of the associated function declarator
			   has been seen.  (Additional elements may follow in
			   C++, including trailing return types and exception
			   specifications.) */
  a_bit_field	in_field_initializer:1;
			/* TRUE while scanning a field initializer.  This flag
			   is set to TRUE in the class (reactivation) scope for
			   the field initializer, and is "sticky" for scopes
			   that appear in the field initializer (e.g., scopes
			   created for lambda expressions, but not template
			   instantiation scopes kicked of by the field
			   initializer expression). */
  a_bit_field	in_template_arg_list:1;
			/* TRUE while scanning a template argument list.  This
			   flag is inherited by many scopes pushed on the
			   stack, but not template instantiation and
			   instantiation context scopes, nor class definition
			   scopes resulting from lambda expressions. */
  a_bit_field	implicit_typename:1;
			/* TRUE if, in this scope, implicit typename processing
			   should be done. */
  a_bit_field	in_disambiguation:1;
			/* TRUE if we are currently doing disambiguation
			   processing. */
  a_bit_field	in_tentative_decl:1;
			/* TRUE if we are currently attempting a tentative
			   declaration scan where "auto" parameters may appear
			   (which will require to be re-parsed in a template
			   declaration context). */
  a_bit_field	is_rescan:1;
			/* TRUE if the scope being pushed is an instantiation
			   scope for template rescan purposes. */
  a_bit_field	is_template_param_rescan:1;
			/* TRUE if the scope is a template declaration scope
			   pushed for the rescan of a dependent template
			   template parameter. */
  a_bit_field	in_concept_rescan:1;
			/* TRUE if we are currently rescanning the constraint
			   expression of a concept. */
  a_bit_field	error_detected:1;
			/* TRUE in some cases where is_rescan is TRUE and an
			   error was suppressed. */
  a_bit_field	rescan_depth_exceeded:1;
			/* TRUE for a chain of instantiation scopes for which
			   is_rescan is TRUE and for which excessive recursion
			   has been detected.  (Used to short-circuit overload
			   resolution.) */
  a_bit_field	in_decltype_context:1;
			/* TRUE when scanning the expression in a decltype
			   operator. */
  a_bit_field	in_noexcept_spec:1;
			/* TRUE when scanning the expression in a noexcept
			   specifier. */
  a_bit_field	function_partial_instantiation:1;
			/* TRUE for template instantiation scopes when the
			   tokens of a function template are being rescanned
			   to create a partial instantiation of the
			   function. */
  a_bit_field	has_at_least_one_return:1;
			/* TRUE if the function has at least one return
			   statement (constexpr functions are required to
			   have exactly one return statement). */
  a_bit_field	constexpr_ruled_out:1;
			/* TRUE if the constexpr constructor or constexpr
			   function has an invalid body (which precludes
			   it from being considered constexpr). */
  a_bit_field	in_consteval_context:1;
			/* TRUE if this is a local scope in the "then" branch
			   of an "if consteval" statement, the "else" branch
			   of an "if not consteval" statement, or in a
			   consteval function.  FALSE within local class scopes
			   appearing in such contexts. */
  a_bit_field	make_access_errors_warnings:1;
			/* Turn access errors into warnings while this flag
			   is set. */
#if GNU_EXTENSIONS_ALLOWED
  a_bit_field	in_gnu_abi_tag_namespace:1;
			/* TRUE if this scope is an inline namespace with
			   the abi_tag attribute, or the scope has some parent
			   that is an inline namespace with the abi_tag
			   attribute (this has an effect on the mangled
			   name). */
#endif /* GNU_EXTENSIONS_ALLOWED */
  a_bit_field	treat_as_specialization:1;
			/* This can be TRUE for class reactivation scopes.
			   When it is TRUE the class should be treated as
			   having been specialized for name lookup purposes
			   (i.e., dependent base classes should be included
			   in the lookup). */
  a_bit_field	in_discarded_statement:1;
			/* TRUE if we are in the discarded branch of a
			   constexpr if that is not in a template context. */
  a_bit_field	is_generic_lambda:1;
			/* TRUE for the sck_template_instantiation scope
			   pushed for the instantiation (including the
			   prototype instantiation) of a generic lambda,
			   and for the template declaration scope for the
			   template parameters of a generic lambda. */
  a_bit_field	in_ctor_initializer:1;
			/* TRUE while scanning the arguments of a constructor
			   initializer. */
  a_bit_field	exporting_decl:1;
			/* TRUE if the current declaration should be
			   exported. */
  a_bit_field	in_export_block:1;
			/* TRUE while scanning a block export declaration.
			   exporting_decl is also TRUE while this is TRUE. */
  a_bit_field
		owns_module_push:1;
			/* TRUE if the module stack should be popped when this
			   scope is popped. */
  a_source_position
		export_pos;
			/* When in_export_block is TRUE this is the position of
			   the "export" keyword. */
  a_scope_pointers_block_ptr
		assoc_pointers_block;
			/* Pointer to a scope pointer block that should be
			   used (in place of the one that is embedded in
			   this scope stack entry); NULL when the embedded
			   scope-pointer-block should be used.  This pointer
			   will be non-NULL when kind is sck_namespace,
			   sck_namespace_extension, or sck_class_struct_union;
			   otherwise it is NULL. */
  a_scope_pointers_block
		pointers_block;
			/* A block of pointers associated with this scope,
			   including a pointer to the linked list of all
			   symbols declared in this scope and pointers to
			   the last entry in linked lists of IL entries
			   entered in the associated IL scope. */
  a_scope_ptr	il_scope;
			/* Pointer to the intermediate language scope
			   entry for this scope.  This can be a real
			   pointer rather than a memory region number because
			   the entry must always be in memory when the scope
			   is active.  NULL if the scope entry has not yet
			   been allocated, which happens in function 
			   declarators and blocks (almost always, a scope
			   entry is not needed, so we wait until something is
			   declared to allocate it).  The entry pointed
			   to can be the one attached to a_routine (usually)
			   or the one attached to a routine type entry
			   (rarely). */
  a_memory_region_number
		il_memory_region;
			/* The number of the IL memory region for this scope.
			   Set even if il_scope == NULL.  Note that this is
			   the "base" memory region; the "current" memory
			   region might switch between this "base" region and
			   the file scope region many times during the
			   processing of the scope. */
  a_memory_region_number
		prev_il_memory_region;
			/* The number of the IL memory region that was the
			   current region at the time this scope was entered.
			   This is restored by pop_scope. */
  int		module_load_context_count;
			/* Non-zero if a module entity is being loaded.  The
			   value indicates how many module declaration contexts
			   have been pushed, which may be greater than 1 when
			   recursive declarations are being processed.  Note
			   that a module declaration context may not include
			   a scope push, and as such this value may fluctuate
			   within the same scope. */
  a_type_ptr	assoc_type;
			/* When kind == sck_func_prototype, this points to the
			   function type whose prototype scope this is.  When
			   kind == sck_class_struct_union or
			   kind == sck_class_reactivation, this points to the
			   class type.  When kind == sck_enum, this points to
			   the enum type.  When kind == sck_function_access for
			   an implicit deduction guide, this points to the
			   class type.  This may also be set for
			   sck_template_instantiation scopes when a class type
			   is instantiated. */
  a_routine_ptr	assoc_routine;
			/* When kind == sck_function, kind ==
			   sck_function_access, or when kind ==
			   sck_template_instantiation for a function
			   instantiation, this points to the routine
			   whose scope this is. */
  a_namespace_ptr
		assoc_namespace;
			/* When kind == sck_namespace, sck_namespace_extension,
			   or sck_namespace_reactivation, this points to the
			   namespace. */
  a_vla_fixup_ptr
		vla_fixup_list;
			/* When kind == sck_func_prototype.  C mode only.
			   Temporary holding place for the vla_fixup_list.
			   When the scope_stack is popped the vla_fixup_list
			   is moved to a_func_info_block for the function. */
  an_extern_type_fixup_ptr
		extern_type_fixup_list;
			/* List of types of variables and routines to be
			   reset at the end of the scope.  Used when
			   inner- and outer-scope declarations of entities
			   with linkage have compatible but not identical
			   types, and the outer-scope type must be restored
			   at the end of the inner scope. */
  a_generated_entity_block_ptr
		generated_entities;
			/* Set of constants generated in the current scope.
			   (NULL if no such constants were generated.) */
  a_function_shareable_constants_table_ptr
		shareable_constants_table;
			/* Hash table of shared constants for the current
			   scope.  Only used if the scope is a function scope.
			   These are constants that refer to something local
			   to the scope, and therefore cannot be shared at 
			   the file scope. */
  a_routine_fixup_ptr
		last_routine_fixup;
			/* Defined for sck_class_struct_union scopes only:
			   the tail of a list of entities used in the token
			   caching and delayed scanning scheme required for
			   C++ member functions (routine bodies and default
			   arguments). */
  an_initializer_fixup_ptr
		last_initializer_fixup;
			/* Defined for sck_class_struct_union scopes only:
			   the tail of a list of entities used in the token
			   caching and delayed scanning scheme required for
			   C++ in-class data member initializers. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_deferred_constraint_check_ptr
		deferred_constraint_checks;
			/* When defer_constraint_checks is TRUE, this contains
			   a list of constraint checks to be performed at
			   a later point in time. */
  a_type_list_entry_ptr
		types_using_pending_constraints;
			/* If a C++/CLI generic declaration being processed
			   from metadata makes uses of pending generic
			   constraints, this is a list of the types based
			   on the generic constraints.  These types will be
			   rechecked at the end of the declaration to make
			   sure the constraints are no longer pending at
			   that point. */
  uint32_t	pending_dependent_if_exists;
			/* The number of enclosing dependent __if_exists
			   or __if_not_exists in the current scope.  This
			   does not count any that may be active in
			   enclosing scopes.  Because these directives are
			   not required to nest properly within scopes,
			   it is possible for a start to have no matching
			   end in the same scope, or an end to have no matching
			   start.  In the former case, the count can be
			   non-zero when the scope is popped.  In the latter
			   case, an attempt to have the value drop below zero
			   will be ignored. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* The following pointers are the end pointers for the lists begun
     in the current IL scope entry.  They are needed only while the scope
     is active (to add entries to the ends of lists), and are therefore
     here instead of in the a_scope entry to save space. */
  a_variable_ptr
		last_parameter;
			/* End of list of parameters of the associated routine,
			   if assoc_routine != NULL.  In declaration order.
			   NULL if no parameters. */
  a_variable_ptr
		last_nonstatic_variable;
			/* End of list of nonstatic local variables of this
			   scope, NULL if none. */
  a_label_ptr	last_label;
			/* End of list of local labels of this scope, NULL
			   if none. */
  a_scope_ptr	first_scope,
		last_scope;
			/* Start and end of list of local scopes (those
			   associated with blocks containing declarations,
			   not with functions or prototypes), NULL if none.
			   A first_scope pointer is needed for those cases
			   where il_scope is NULL.  For ease of implementation,
			   the scopes list is always built using first_scope/
			   last_scope, then transferred to the il_scope entry
			   or into the parent scope when the current scope
			   is popped. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_source_sequence_entry_ptr
		source_sequence_avail_list;
			/* List of freed source sequence entries that are
			   available for reuse; NULL if none. */
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  a_source_sequence_entry_ptr
		ss_list_instantiation_insert_point;
			/* If kind == sck_file, pointer to a source sequence
			   entry before which source sequence entries for a
			   template instantiation should be inserted, or NULL
			   if they should be added to the end of the list.
			   If kind == sck_template_instantiation, the current
			   pointer in the file scope entry when push_scope is
			   called and to which that pointer is restored by
			   pop_scope.  Not used for any other scope kinds. */
  a_type_list_entry_ptr
		classes_in_ss_list;
			/* List of classes whose source sequence entries are
			   recorded in the list headed by source_sequence_list.
			   This list is not maintained for the file scope
			   (because it's not needed there), and is merged along
			   with source_sequence_list when the scope is popped.
			   The purpose of this list is to update the field
			   ss_list_depth in the class symbol supplements of
			   classes recorded in source_sequence_list when the
			   scope stack entries are popped. */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
  a_source_sequence_entry_ptr
		source_sequence_list,
		end_of_source_sequence_list;
			/* Head and tail of a list of source sequence entries
			   generated while the current scope is active.  When
			   the scope is popped, the list is merged with a
			   list on a containing scope -- except when the scope
			   kind is sck_function and sck_file, in which case
			   the list is moved onto the associated IL scope. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  a_scope_depth decl_scope_level;
			/* Saved value of decl_scope_level when this scope
			   was pushed. */
  a_scope_depth depth_template_declaration_scope;
			/* Depth of the sck_template_declaration scope entry,
			   if any, that the current scope is enclosed by;
			   otherwise, NO_SCOPE_DEPTH. */
  a_scope_depth depth_innermost_instantiation_scope;
                        /* Depth of the nearest enclosing instantiation scope
			   of any kind.  This is a copy of the global
			   variable of the same name. */
  a_symbol_ptr  instance_sym;
                        /* When kind == sck_template_instantiation, contains
                           a pointer to the symbol for the class or function
			   being instantiated or the static data member being
			   defined. */
  a_symbol_ptr  template_sym;
                        /* When kind == sck_template_instantiation, contains
                           a pointer to the symbol for a symbol providing
			   information about the template on which the
			   instantiation is based.  When a template class is
			   being instantiated it points to an sk_class_template
			   symbol; for a nonmember function it points to an
			   sk_function_template symbol; for member functions
			   and static data members of an instance of a class
			   template, it points to an sk_member_function or
			   sk_static_data_member symbol that is a member of
			   a prototype instantiation of the template class. */
  a_template_arg_ptr
                template_arg_list;
                        /* When kind == sck_template_instantiation, contains
                           a pointer to a template argument list. */
  a_template_arg_ptr
		deduced_template_args;
			/* When kind == scl_template_instantiation, set to
			   point to the deduced template arguments when
			   function template argument deduction is
			   complete. */
  a_source_position
		source_position;
			/* The source position when the scope was pushed
			   onto the stack. */
  a_scope_depth depth_innermost_function_scope;
			/* The scope depth of the containing function scope,
			   or NO_SCOPE_DEPTH if there is no containing
			   function scope or if the scope of a local class or
			   template instantiation intervenes between the
			   current scope and the containing function scope. */
  a_template_decl_info_ptr
		template_decl_info;
                        /* When kind == sck_template_instantiation, contains
			   a pointer to the information about the template
			   declaration from which the instantiation is
			   being generated.
                           When kind == sck_template_declaration, contains
			   a pointer to the template declaration information
			   for the current template declaration nested
                           depth. */
  a_decl_sequence_number
		last_label_decl_seq;
			/* When kind == sck_function, the declaration sequence
			   number of the last label defined (so far) in the
			   current scope; 0 if this is not a function scope
			   or if there are no label definitions.  The value
			   is updated each time a label definition is seen. */
  a_decl_sequence_number
		exception_spec_decl_seq;
			/* When the PS_EXCEPTION_SPEC option is used, this
			   is the declaration sequence number to be used
			   in g++ mode during the lookup of names in an
			   exception specification. */
  a_decl_sequence_number
		decl_seq_for_lookup;
			/* If this is not NO_DECL_SEQUENCE_NUMBER, this value
			   is used for normal lookups. */
  a_pending_pragma_list
		*pending_pragmas;
			/* A list of pragmas that have been cached by
			   the lexical routines but have not yet been
			   fully processed.  This list contains only
			   pbk_other pragmas. */
  a_pending_pragma_list
		*curr_construct_pragmas;
			/* Points to the list of pbk_next_construct
			   pragmas for the construct that is currently
			   being scanned.  This is in the scope stack entry
			   so that it will automatically nest when
			   template instantiations are performed. */
  a_scope_depth	next_scope_that_affects_access_control;
			/* Depth of the first scope stack entry below this
			   one that has an effect on access control.
			   Indicates the next entry on a list headed by
			   depth_of_innermost_scope_that_affects_access_control
			   (a global variable).  Also set in scope stack
			   entries that are not part of the list because they
			   do not affect access control. */
  a_scope_depth	orig_access_depth;
			/* When instantiation scopes are pushed for rescan
			   purposes, the innermost scope that affects
			   access control is saved in the last context
			   scope that is pushed.  This is used later
			   to recheck failed access checks in the
			   referencing context. */
  an_access_error_descr_ptr
		deferred_access_checks;
			/* When defer_access_checks is TRUE, this contains
			   a list of access checks that were done (and failed)
			   and must be repeated once the declarator has been
			   scanned. */
  an_access_error_descr_ptr
		last_deferred_access_check;
			/* When defer_access_checks is TRUE, this points
			   to the last element in a list of access checks. */
  a_scope_depth	saved_curr_deferred_access_scope;
			/* The value of curr_deferred_access_scope when
			   this scope was pushed.  Used to restore the value
			   when the scope is popped. */
  struct an_expr_stack_entry /* struct form used to avoid having to include
			        exprutil.h all over. */
		*saved_expr_stack;
			/* The value of expr_stack when this scope was pushed,
			   used to restore the value when the scope is
			   popped. */
  an_object_lifetime_ptr
		curr_scope_object_lifetime;
			/* A pointer to the object lifetime created for this
			   scope. */
  an_object_lifetime_ptr
		saved_curr_object_lifetime;
			/* The value of curr_object_lifetime when the scope
			   is pushed onto the stack, and the value to which
			   it will be restored when the scope is popped. */
  an_object_lifetime_ptr
		object_lifetime_avail_list;
			/* List of freed object lifetime entries that are
			   available for reuse.  Only used for file and
			   function scopes; the entries on the list belong to
			   the memory region associated with the scope. */
  a_symbol_ptr	templ_member_class_sym;
			/* For sck_template_declaration scopes, this points
			   to the symbol of the class of which the entity
			   currently being defined is a member (e.g., if
			   A<T>::f is being defined, this points to the
			   class type of A<T>.  Contains NULL if the
			   template is not a member. */
  a_scope_depth depth_innermost_namespace_scope;
                        /* Depth of the nearest enclosing namespace scope or,
			   by default, the depth of the file scope. This is
			   a copy of the global variable of the same name. */
  long		num_of_extra_times_pushed;
			/* Namespace scopes may be pushed more than once
			   under some circumstances (such as defining a
			   member of a nested namespace in the enclosing
			   namespace).  When this occurs, the scope is
			   not duplicated on the scope stack.  Instead,
			   this counter is incremented so that when the
			   scope is popped, it is possible to know when
			   the scope should actually be removed from
			   the stack. */
  an_active_using_directive_ptr
		active_using_directives;
			/* Linked list of entries representing the
			   using-directives currently active in the current
			   scope; NULL if none. */
  an_active_using_directive_ptr
		using_directives_that_apply_here;
			/* Linked list of entries representing using-directives
			   that were declared in other scopes but apply at
			   this scope.  This list is linked using the
			   next_that_applies_at_depth field. */
  a_scope_depth	previous_scope;
			/* Scope depth of the scope that logically precedes
			   the current one.  This allows scopes on the stack
			   to be skipped over for name lookup and other
			   purposes.  This is primarily used to hide certain
			   scopes during template instantiation. */
  a_scope_depth	instantiation_context_depth;
			/* Present only for template instantiation scopes.
			   Contains the scope depth of the innermost
			   namespace scope at the point that the instantiation
			   was initiated. */
  a_scope_depth	instantiation_common_depth;
			/* Present only for template instantiation scopes.
			   Contains the scope depth of the scope that is
			   part of both the template definition context and
			   the context at the point of instantiation. */
  a_scope_depth	saved_depth_of_initial_lookup_scope;
			/* The previous value of the global variable
			   depth_of_initial_lookup_scope when a new scope
			   is pushed.  This value is restored when the
			   scope is popped. */
  uint32_t	empty_contexts_pushed;
			/* In some cases, no scope will be pushed for an
			   instantiation scope in a prototype instantiation.
			   This is a count of such scopes that have been
			   pushed. */
  a_scope_depth	orig_depth;
			/* For nonnested template instantiation scopes,
			   specifies the scope depth before the process
			   of pushing the instantiation context began.
			   This is used to determine how many scopes should
			   be popped when the instantiation scope is popped. */
  a_scope_depth	saved_innermost_scope_that_affects_access;
			/* This field is used in the last scope pushed when
			   a template instantiation scope is pushed.  It is
			   used to store the depth of the innermost scope
			   that affects access control when the template
			   instantiation scope was pushed.  This is needed
			   because the values in the scope stack that are
			   normally used to restore this value are altered
			   by the routines that create the instantiation
			   context. */
  a_template_cache_segment_list
		*template_cache_segment_list;
			/* A list of template cache segment entries for a
			   member class or function of the current prototype
			   instantiation.  Present only for template
			   instantiation scopes associated with prototype
			   instantiations. */
  struct a_class_def_state
		*class_def_state;
			/* For sck_class_struct_union scopes, pointer to an
			   entry that tracks general information about the
			   class/struct/union definition as it accumulates;
			   NULL otherwise. */
  a_name_hidden_by_old_for_init_ptr
		names_hidden_by_old_for_init;
			/* For sck_function and sck_block scopes, pointer to
			   a (possibly NULL) linked list of entries that
			   identify symbols from an enclosing scope for which
			   hidden_by_old_for_init is set to TRUE (because of
			   for-init declarations of for-statements in the
			   current scope).  Always NULL in C-mode or when
			   use_nonstandard_for_init_scope is TRUE. */
  struct a_tmpl_decl_state
		*tmpl_decl_state;
			/* For template declaration scopes, points to the
			   entry used to record information about the
			   current template declaration. */
  struct a_decl_parse_state
		*decl_parse_state;
			/* For function prototype scopes, points to the
			   a_decl_parse_state entry passed to the call to
			   function_declarator that pushed the scope.  In
			   file/namespace scope, this may point to the entry
			   created for a linkage specification while parsing
			   the embedded declaration (which has its own state).
			   While parsing an initializer, this points to the
			   state block for the initialized entity.  Otherwise,
			   NULL. */
  unsigned long
		pending_templ_arg_lists;
			/* The number of opening "<" delimiters that have been
			   seen without matching closing ">" delimiters.  (This
			   is slightly more general than "pending template
			   argument lists", because "<" delimiters also appear
			   in new-style cast constructs.) */
  a_nondependent_call_info_ptr
		next_nondependent_call;
			/* When doing dependent name processing, this
			   field is present for template instantiation scopes
			   and points to the next nondependent call entry
			   for the current instantiation.  During a real
			   instantiation this list is used to determine
			   whether a given call is dependent. */
  a_pack_expansion_descr_ptr
		last_pack_expansion_used;
			/* In real instantiation scopes for variadic
			   templates, this points to the last entry on the
			   list of pack expansions that was used.  This is
			   used to find the pack expansion entry for a given
			   point within the actual instantiation.  It is
			   initialized with the first pack expansion for the
			   template. */
  a_pack_expansion_descr_ptr
		saved_last_pack_expansion_used;
			/* Used to save and potentially restore the value
			   of last_pack_expansion_used across a tentative
			   pack expansion context.  See
			   begin_tentative_pack_expansion_context and
			   end_tentative_pack_expansion_context for
			   details. */
  a_pack_reference_ptr
		packs_referenced;
			/* When a variadic parameter pack is referenced, it
			   is placed on this list.  When we reach the end of
			   the expansion context, any packs referenced within
			   the range of the pack are extracted from this list.
			   The list is expected to be NULL when the end of the
			   scope is reached, otherwise a diagnostic is
			   issued. */
  a_pack_expansion_stack_entry_ptr
		pack_expansion_stack;
			/* The saved value of the pack expansion stack when
			   a template declaration or template instantiation
			   scope is pushed. */
  a_type_ptr	conversion_parent_type;
			/* When scanning a conversion operator, this provides
			   the left hand side of the field selection associated
			   with the call of the conversion operator.  This is
			   used to do the required dual-lookup of the name
			   following the operator keyword. */
#if NEED_NAME_MANGLING
  union {
    /* When kind == sck_function, sck_block, or sck_condition: */
    a_collision_table_ptr
		local_name_collision_table;
			/* A hash table of local symbols to detect local
			   entities with a same name.  Such entities must have
			   a discriminator appended to their mangled name.
			   (Non-NULL only for function scopes.) The union type
			   a_collision_table is defined in scope_stk.c. */
    /* When kind == sck_file, sck_namespace, sck_namespace_extension, or
       sck_class_struct_union: */
    a_discriminator
		last_unnamed_type_number;
			/* In non-local scopes, the last "discriminator" value
			   assigned to an unnamed enum or class type. */
  } name_discr;
  a_discriminator
		last_closure_type_number;
			/* The last "discriminator" value assigned to a closure
			   type in this scope.  (This value may be "swapped
			   out" in certain initializer contexts.) */
#endif /* NEED_NAME_MANGLING */
#if DO_IL_LOWERING && ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS
  a_string_literal_table_ptr
		string_literal_table;
			/* A hash table of string literals used within a
			   given function.  Used only for function scopes.
			   This is used to assign sequence numbers to
			   string literals and to detect multiple uses of
			   the same string literal value. */
  unsigned long	string_literal_sequence_number;
			/* For function scopes, the highest sequence number
			   that has already been used as a string literal
			   sequence number. */
#endif /* DO_IL_LOWERING && ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  an_ELF_visibility_kind
		ELF_visibility;
			/* The default ELF visibility for entities declared
			   in this scope (only applies to sck_namespace,
			   sck_namespace_extension, and sck_class_struct_union
			   scopes). */
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
  a_class_fixup_header
		class_fixup_header;
			/* Class fixup information for the scope.  Only used
			   for the global scope and function scope. */
  a_param_id_ptr
		param_id_list;
			/* In function prototype scopes, this points to the
			   param_id_list for the function, if any. */
  a_type_ptr
		orig_return_type;
			/* The original return type of the function (when kind
			   is sck_function).  The final return type can change
			   (e.g., because of "auto" deduction). */
  a_token_sequence_number
		var_templ_decl_name_tsn;
			/* If we are in the instantiation of a variable
			   template this is the token sequence number of
			   the declarator name.  NO_TOKEN_SEQUENCE_NUMBER
			   in other cases. */
  an_il_entity_list_entry
		*injections;
			/* Token sequence entries scheduled for injection
			   in this scope. */
} a_scope_stack_entry;

EXTERN_THREAD a_scope_stack_entry_ptr
		scope_stack;
			/* Stack of entries describing active scopes.
			   scope_stack[0] is the entry for the file scope,
			   scope_stack[1] is an entry for a function scope,
			   etc.  Dynamically allocated; can be expanded
			   if necessary.  size_scope_stack gives the
			   number of elements currently allocated.
			   Allocation is not per-file. */

EXTERN_THREAD a_scope_depth
		depth_scope_stack;
			/* Current depth of the scope stack.  NO_SCOPE_DEPTH
			   (i.e., -1) indicates that the stack is empty. */

/*
Convenience macro to access the top of the scope stack.
*/
#define scope_stack_top()  (scope_stack[depth_scope_stack])


/*
Convenience macro to test the scope kind for a scope stack entry or for an IL
scope entry.
*/
#define scope_is(scope, sck)                                                \
  ((scope)->kind == (a_scope_kind)(sck))


/*
Given a scope depth, return a pointer to the scope stack entry or
a NULL pointer if the scope depth is NO_SCOPE_DEPTH.
*/
#define scope_stack_entry_for(depth)					\
  ((depth) == NO_SCOPE_DEPTH ? NULL : &scope_stack[(depth)])


inline a_boolean scope_number_is_active(a_scope_number n)
/*
Return TRUE if the given scope number is on the scope stack.
*/
{
  a_boolean            result = FALSE;
  a_scope_stack_entry  *ssep = &scope_stack_top();

  do {
    if (ssep->number == n) {
      result = TRUE;
      break;
    }  /* if */
  } while (!scope_is(ssep--, sck_file));
  return result;
}  /* scope_number_is_active */


/*
Return TRUE if we are in the partial instantiation of a function template.
*/
#define is_function_template_partial_instantiation_context()		\
  (depth_innermost_instantiation_scope != NO_SCOPE_DEPTH &&		\
   scope_stack[depth_innermost_instantiation_scope].			\
                                             function_partial_instantiation)



/*
Given a pointer to a scope stack entry, return the address of the associated
scope-pointers-block -- it may either be part of the entry itself or part of
another data structure elsewhere (as indicated by the value of the
assoc_pointers_block field in the scope stack entry).
*/
#define assoc_pointers_block_of(ssep)                                    \
  ((ssep)->assoc_pointers_block == NULL ?                                \
     &((ssep)->pointers_block) : (ssep)->assoc_pointers_block)

/*
Given a pointer to a scope stack entry, return the address of the previous
scope stack entry according to the previous_scope field.  Return NULL if there
is no previous scope.
*/
#define previous_scope_of(ssep)						\
  ((ssep)->previous_scope == NO_SCOPE_DEPTH				\
                                ? NULL : &scope_stack[(ssep)->previous_scope])


/*
Given a pointer to a scope stack entry, return the scope depth.  If the
pointer is NULL, return NO_SCOPE_DEPTH.
*/
#define scope_depth_of(ssep)						\
  ((a_scope_depth)((ssep) == NULL ? NO_SCOPE_DEPTH : (ssep - &scope_stack[0])))

/*
Make sure the specified scope depth is a valid depth on the scope stack.
*/
#define assert_is_valid_scope_depth(depth)				\
  { check_assertion(depth == NO_SCOPE_DEPTH || depth <= depth_scope_stack); }

/*
Determine whether a given scope kind is one that is "local" (i.e., one in
which variables have automatic storage duration by default).
*/
#define is_local_scope_kind(kind)                                        \
  ((kind) == (a_scope_kind)sck_function ||                               \
   (kind) == (a_scope_kind)sck_block ||                                  \
   (kind) == (a_scope_kind)sck_condition)


inline a_boolean is_file_or_namespace_scope_kind(a_scope_kind kind)
/*
Given a pointer to a scope kind, return TRUE if the kind is a file
scope or namespace scope.
*/
{
  return (kind == sck_file ||
          kind == sck_namespace ||
          kind == sck_namespace_extension);
}  /* is_file_or_namespace_scope_kind */


/*
Given a pointer to a scope stack entry, return TRUE if and only if the
associated scope is a file or namespace scope.
*/
#define is_file_or_namespace_scope(ssep)                     \
  is_file_or_namespace_scope_kind((ssep)->kind)


inline a_boolean is_lambda_body_scope(a_scope_stack_entry_ptr ssep)
/*
Return TRUE if the given scope stack entry represents a lambda body scope;
otherwise, return FALSE.
*/
{
  return ssep->kind == sck_function && ssep->assoc_routine->is_lambda_body;
}  /* is_lambda_body_scope */


inline a_boolean scope_is_null_or_placeholder(a_scope_ptr sp)
/*
Return TRUE if sp is either NULL or a placeholder scope, FALSE otherwise.
*/
{
  return sp == NULL || sp->is_placeholder_scope;
}  /* scope_is_null_or_placeholder */


/*
TRUE if we are in a context in which template dependent types need to
be handled in contexts such as expressions.  Typically, this is in
a prototype instantiation, but can also occur in template declaration
scopes.  It is also TRUE when in_nonreal_instantiation is TRUE.
*/
#define is_template_dependent_context()                                 \
  (depth_template_declaration_scope != NO_SCOPE_DEPTH ||                \
   scope_stack[depth_scope_stack].in_prototype_instantiation ||         \
   scope_stack[depth_scope_stack].in_nonreal_instantiation ||           \
   scope_stack[depth_scope_stack].kind == sck_module_isolated)

/*
TRUE if we are within a template declaration scope.
*/
#define is_template_declaration_context()				\
  (depth_template_declaration_scope != NO_SCOPE_DEPTH)

/*
TRUE if we are in the context of a variadic template.  This is TRUE both
when the original template is scanned and during a real instantiation.
*/
#define is_variadic_template_context()					\
  (depth_scope_stack != NO_SCOPE_DEPTH ?				\
   scope_stack[depth_scope_stack].in_variadic_template : FALSE)

/*
TRUE if we are in the context of the definition of a variadic template.
*/
#define is_variadic_definition_context()				\
  (is_variadic_template_context() && is_prototype_instantiation_context())

/*
TRUE if implicit typename processing should be done.
*/
#define use_implicit_typename()						\
  (depth_scope_stack != NO_SCOPE_DEPTH ?				\
   scope_stack[depth_scope_stack].implicit_typename : FALSE)

/*
TRUE if we are processing code that was imported from metadata or the
instantiation of a generic imported from metadata.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#define in_code_generated_from_metadata()				\
  (scanning_generated_code_from_metadata ||				\
   (depth_innermost_instantiation_scope != NO_SCOPE_DEPTH &&		\
    scope_stack[depth_scope_stack].instantiation_from_metadata))
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
TRUE if we are processing code that was not present in an input file, i.e., it
was scanned as part of metadata or from a builtin template.  Note that this
is TRUE only when scanning generated code; it is FALSE during an instantiation
of generated code.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#define in_generated_code() (scanning_generated_code)
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
/*lint -emacro(506,in_generated_code)*/
#define in_generated_code() FALSE 
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
TRUE if we are in a C++/CLI generic definition context.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#define is_cli_generic_definition_context()				\
  (depth_scope_stack != NO_SCOPE_DEPTH &&		\
   scope_stack[depth_scope_stack].in_generic_definition)
#else  /* !MICROSOFT_EXTENSIONS_ALLOWED */
/*lint -emacro(506,is_cli_generic_definition_context)*/
#define is_cli_generic_definition_context() FALSE
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Return TRUE if the symbol is a prototype instantiation or C++/CLI generic
class definition.
*/
#define is_prototype_instantiation_or_cli_generic_context()	\
  (is_prototype_instantiation_context() ||				\
   is_cli_generic_definition_context())


/*
Return TRUE if the current context is one in which a C++/CLI generic
declaration is allowed.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#define is_valid_cli_generic_declaration_context()	\
  (depth_scope_stack != NO_SCOPE_DEPTH &&			\
   (!scope_stack[depth_scope_stack].in_prototype_instantiation || \
    scope_stack[depth_scope_stack].in_generic_definition))
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Safe version of is_template_dependent_context that can be used in
back ends (returns TRUE there and FALSE in IL lowering).  Basically guards
is_template_dependent_context to avoid calling it when the scope
stack does not exist.
*/
#if DO_IL_LOWERING
#define context_may_have_dependent_types() \
 (!il_lowering_underway && (!in_front_end || is_template_dependent_context()))
#else /* !DO_IL_LOWERING */
#define context_may_have_dependent_types() \
 (!in_front_end || is_template_dependent_context())
#endif /* DO_IL_LOWERING */


inline a_boolean is_module_isolation_context()
/*
TRUE if we are in a module isolation context.
*/
{
  a_boolean result = FALSE;

  if (depth_scope_stack != NO_SCOPE_DEPTH) {
    a_scope_kind deepest_scope_kind = scope_stack[depth_scope_stack].kind;
    if (deepest_scope_kind == sck_module_isolated) {
      result = TRUE;
    } else if (deepest_scope_kind == sck_template_declaration) {
      if (depth_scope_stack - 1 != NO_SCOPE_DEPTH &&
          scope_stack[depth_scope_stack - 1].kind == sck_module_isolated) {
        result = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* is_module_isolation_context */


/*
TRUE if we are in a context in which an expression is being scanned
that could later potentially participate in template argument deduction
and/or template argument substitution into an expression.
*/
#define is_template_deduction_context()					\
  (depth_scope_stack != NO_SCOPE_DEPTH ?			        \
   scope_stack[depth_scope_stack].in_template_deduction_context : FALSE)


/*
TRUE if we are in a template prototype instantiation context, which
includes template declaration scopes.  This is similar to
is_template_dependent_context, but excludes nonreal instantiations.
*/
#define is_prototype_instantiation_context()				\
  (depth_template_declaration_scope != NO_SCOPE_DEPTH ||		\
   scope_stack[depth_scope_stack].in_prototype_instantiation)

/*
TRUE if we are in an uninstantiated pack expansion context.  This is either a
prototype instantiation context or a nonreal instantiation of a default
template argument that is enclosed by a prototype instantiation context.
*/
#define is_uninstantiated_pack_expansion_context()			\
  (is_prototype_instantiation_context() ||				\
   (scope_stack[depth_scope_stack].in_nonreal_instantiation &&		\
    scope_stack[depth_scope_stack].is_default_template_arg &&		\
    enclosing_scope_is_prototype_instantiation_context()))

/*
TRUE if we are in a template prototype instantiation context but not
in the context of a class specialization.  This excludes in-class
specializations within prototype instantiations.  Note that this is
FALSE for template declaration contexts.
*/
#define is_nonspecialized_prototype_instantiation_context()		\
   (scope_stack[depth_scope_stack].in_prototype_instantiation &&	\
   !scope_stack[depth_scope_stack].in_class_specialization)

/*
TRUE if we are in an in-class specialization.
*/
#define is_in_class_specialization_context()				\
  (scope_stack[depth_scope_stack].in_class_specialization)

/*
TRUE if we are in a template instantiation context, but not a special
instantiation scope pushed for specializations in Microsoft and Sun modes.
*/
#define is_nonspecialized_instantiation_context()			\
  (depth_innermost_instantiation_scope != NO_SCOPE_DEPTH &&		\
   scope_stack[depth_innermost_instantiation_scope].is_instantiation_context)

/*
TRUE if we are in a template declaration scope or any kind of instantiation
scope.
*/
#define is_template_context()						\
  (is_nonspecialized_instantiation_context() ||		\
   depth_template_declaration_scope != NO_SCOPE_DEPTH)

/*
TRUE if we are in an instantiation that is not a prototype or nonreal
instantiation.
*/
#define is_real_instantiation_context()					\
  (depth_innermost_instantiation_scope != NO_SCOPE_DEPTH &&		\
   !scope_stack[depth_scope_stack].in_prototype_instantiation &&	\
   !scope_stack[depth_scope_stack].in_nonreal_instantiation)

/*
TRUE if we are in an instantiation that is a prototype or nonreal
instantiation.
*/
#define is_nonreal_instantiation_context()				\
  (depth_innermost_instantiation_scope != NO_SCOPE_DEPTH &&		\
   (scope_stack[depth_scope_stack].in_prototype_instantiation ||	\
    scope_stack[depth_scope_stack].in_nonreal_instantiation))

/*
TRUE if the innermost instantiation scope is an alias template being
instantiated with dependent template arguments in a template declaration
scope.
*/
#define is_alias_in_template_decl_context()				\
  (depth_innermost_instantiation_scope != NO_SCOPE_DEPTH &&		\
   scope_stack[depth_innermost_instantiation_scope].alias_in_template_decl)

/*
TRUE if the enclosing scope is a prototype instantiation context.
*/
#define enclosing_scope_is_prototype_instantiation_context()		\
  ((depth_scope_stack != NO_SCOPE_DEPTH && depth_scope_stack > 0) &&	\
   scope_stack[depth_scope_stack-1].in_prototype_instantiation)

/*
TRUE if we are in the instantiation of a template in a translation unit
loaded for the purpose of instantiating exported templates.  Note that
this will be FALSE for an instantiation performed during the initial scan
of a translation unit (which should only occur for prototype instantiations).
The check of is_prototype_instantiation_context() is done to make sure that
this returns FALSE for deferred prototype instantiations.
*/
#define in_exported_template_instantiation()				\
  (curr_export_translation_unit_stack_entry != NULL &&			\
   curr_export_translation_unit_stack_entry->next != NULL &&		\
   depth_innermost_instantiation_scope != NO_SCOPE_DEPTH &&		\
   scope_stack[DEPTH_OF_FILE_SCOPE].is_reactivation &&			\
   !is_prototype_instantiation_context())

/*
TRUE if we are in a context in which the form of name references should
be recorded.
*/
#define record_name_references_in_context()				\
  (depth_scope_stack != NO_SCOPE_DEPTH ?			        \
   scope_stack[depth_scope_stack].record_form_of_name_reference : FALSE)

/*
TRUE if we are in the discarded branch of a constexpr if in a non-template
context.
*/
#define in_constexpr_if_discarded_statement()				\
  (depth_scope_stack != NO_SCOPE_DEPTH ?			        \
   scope_stack[depth_scope_stack].in_discarded_statement : FALSE)


/*
Return a pointer to the class fixup header entry to be used for the
current context.  This is either the one for the current function scope
or the scope depth used for non-local fixups (usually the file scope
depth).  The non-local list is also used if the fixup is being done after
an instantiation.  A different scope for non-local fixups is used when
get_definition_of_class is used.  See the description of
non_local_class_fixup_depth for more information.
*/
/*lint -emacro(506,curr_class_fixup_header)*/
#define curr_class_fixup_header(for_instantiation)			\
  (&scope_stack[((for_instantiation) ||		                        \
                 depth_innermost_function_scope == NO_SCOPE_DEPTH)	\
                         ? non_local_class_fixup_depth			\
                         : depth_innermost_function_scope].class_fixup_header)


/* Note that the following variables, which give positions in the scope stack,
   are defined as indexes into the array, not as pointers.  Pointers into
   the scope stack are dangerous because the scope stack can be reallocated
   and moved on a push_scope. */
EXTERN_THREAD a_scope_depth
		depth_of_initial_lookup_scope;
			/* Scope depth of the scope at which name lookup
			   operations should begin.  This is usually the
			   same as depth_scope_stack but is different
			   under certain conditions (for example, when
			   a namespace reactivation is pushed on top of
			   a template declaration scope). */
EXTERN_THREAD a_scope_depth
		decl_scope_level;
			/* Level in the scope stack that contains the
			   current declaration level.  In C, differs from
			   depth_scope_stack when the innermost "scopes"
			   are for struct/union fields; decl_scope_level
			   then contains the real scope level rather than
			   the struct/union pseudo-scope level.  In C++,
			   differs from depth_scope_stack when the innermost
			   "scope" is a class reactivation. */
EXTERN_THREAD a_scope_depth
		depth_innermost_function_scope;
			/* Level in the scope stack that contains the innermost
			   function scope, or NO_SCOPE_DEPTH if there isn't
			   one.

			   Note that this is reset when entering local classes;
			   code that needs to observe the true innermost
			   function scope should instead use
			   get_depth_innermost_function_scope(). */
EXTERN_THREAD a_scope_ptr
		innermost_function_scope;
			/* The innermost function scope, or NULL if there isn't
			   one.

			   Typically, this matches
			   depth_innermost_function_scope, but can be different
			   in situations where a function is being processed
			   where no scope stack entry exists (e.g., in IL
			   lowering, when routines are generated).

			   Like depth_innermost_function_scope, this is reset
			   when entering local classes; code that needs to
			   observe the true innermost function scope must use
			   get_innermost_function_scope() and access the scope
			   stack directly.  */
EXTERN_THREAD a_scope_depth
		depth_innermost_instantiation_scope;
			/* If there are template instantiation scopes on the
                           scope stack, this is the depth of the innermost
                           one.  Otherwise, NO_SCOPE_DEPTH. */
EXTERN_THREAD a_scope_depth
		depth_template_declaration_scope;
			/* Depth of the sck_template_declaration scope entry,
			   if any, that the current scope is enclosed by;
			   otherwise, NO_SCOPE_DEPTH. */
EXTERN_THREAD a_scope_depth
		curr_deferred_access_scope;
			/* Depth of the scope entry to be used to determine
			   whether access checking should be deferred, and if
			   so, the entry to which the deferred access checks
			   should be attached.  Set to NO_SCOPE_DEPTH if
			   access checking cannot be deferred in this scope. */

#if GENERATE_SOURCE_SEQUENCE_LISTS
EXTERN_THREAD a_boolean
		source_sequence_entries_disallowed;
			/* TRUE if the current scope establishes or belongs to
			   a context in which source sequence entries should
			   not be issued -- e.g. a template declaration, a
			   a template instantiation, or a pragma. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

EXTERN_THREAD a_boolean
		inside_local_class;
			/* TRUE if we are currently inside a local class,
			   i.e., a class defined within a function. */
EXTERN_THREAD a_scope_depth
		depth_innermost_namespace_scope;
			/* If there are any namespace scopes on the scope
			   stack, this is the depth of the innermost one.
			   Otherwise, it is the depth of the file scope.
			   It is defined in both C and C++. */
EXTERN_THREAD a_scope_number
		next_scope_number;
			/* Next scope number to be assigned.  These are
			   unique identifiers for each scope, not just
			   the scope nesting depth.  Also used for the
			   pseudo-scopes associated with the members of
			   structs and unions in C (not C++). */

EXTERN_THREAD a_scope_depth
		depth_of_innermost_scope_that_affects_access_control;
			/* If there are scopes on the scope stack that
			   affect C++ access control, this is the depth of
			   the innermost one.  Otherwise, NO_SCOPE_DEPTH.
			   Heads a list linked by
			   next_scope_that_affects_access_control. */

EXTERN_THREAD a_scope_depth
		num_classes_on_scope_stack;
			/* Current count of sck_class_struct_union and
			   sck_class_reactivation entries in scope_stack.
			   When non-zero, we are inside a class or
			   reactivation of the scope of a class, and name
			   lookup is more complicated. */

EXTERN_THREAD a_scope_depth
		non_local_class_fixup_depth;
			/* The fixup of classes is delayed until any pending
			   class definitions have completed, except that
			   local classes are fixed up when any pending
			   local class definitions are complete, even if
			   some namespace scope classes (including possibly
			   class template instantiations) are still pending.
			   This variable is the scope depth at which non-local
			   class fixups should be recorded.  This is the file
			   scope depth unless get_definition_of_class is
			   being used, in which case it is the depth of the
			   special context scope that get_definition_of_class
			   pushes. */

EXTERN_THREAD a_boolean
		function_body_processing_delayed_on_some_func_in_primary_il;
			/* TRUE if function body processing (e.g., lowering)
			   was delayed for some function in the primary IL. */


EXTERN_THREAD a_pack_expansion_stack_entry_ptr
		pack_expansion_stack;
			/* Pointer to the top of the pack expansion stack. */

#if NEED_NAME_MANGLING
extern void compute_name_collision_discriminator(a_symbol_ptr   sym,
                                                 a_scope_depth  scope_depth);

extern void cancel_name_collision_discriminator(a_symbol_ptr   sym,
                                                a_scope_depth  scope_depth);

extern
void compute_default_arg_name_collision_discriminators(a_param_type_ptr  ptp);

#endif /* NEED_NAME_MANGLING */

extern void set_parent_entity_for_closure_types(
                   an_il_entity_list_entry_ptr  elp,
                   a_symbol_ptr                 parent_sym,
                   a_boolean                    subject_to_trans_unit_corresp);

extern void set_parent_routine_for_closure_types_in_default_args(
                                                       a_type_ptr    rtp,
                                                       a_symbol_ptr  rout_sym);

#if DO_IL_LOWERING
extern a_boolean parent_is_lambda_closure(a_routine_ptr	routine,
					  a_type_ptr	*closure_class);
#endif /* DO_IL_LOWERING */

extern void check_c99_inline_definition(a_variable_ptr     var,
                                        a_source_position  *pos);

/* Begin a name scope. */
extern a_scope_ptr push_scope(a_scope_kind       kind,
       	                      a_scope_number     scope_number_to_reuse,
                              a_type_ptr         assoc_type,
                              a_routine_ptr      assoc_routine);

extern void push_file_scope(a_boolean	is_reactivation);

extern void push_block_scope_with_lifetime(an_object_lifetime_ptr olp);

extern void push_block_reactivation_scope(
			a_scope_ptr			scope,
			a_scope_pointers_block_ptr	pointers_block);

extern void pop_block_scope(a_boolean	is_final_pop);

extern void push_block_scope(a_scope_pointers_block_ptr	pointers_block);

extern void push_template_declaration_scope_full(
		a_template_decl_info_ptr	decl_info,
		a_scope_number			scope_number,
		a_boolean			is_template_param,
		a_boolean			is_template_param_rescan);

extern void push_template_declaration_scope(
		a_template_decl_info_ptr	decl_info,
		a_boolean			is_template_param_rescan);

extern a_scope_ptr push_for_init_scope(
                                    a_scope_pointers_block_ptr pointers_block);

extern a_scope_ptr push_namespace_scope(a_scope_kind    kind,
                                        a_namespace_ptr assoc_namespace);

extern void make_class_definition_context_visible(void);

extern void reactivate_local_context(
			a_template_decl_info_ptr	decl_info,
			a_scope_ptr			scope,
			a_symbol_ptr			instance_sym,
			a_type_ptr			assoc_type,
			a_routine_ptr			assoc_routine,
			a_push_scope_options_set	options);

extern void refresh_scope_stack();

extern void pop_namespace_scope(void);

extern void inject_tokens_in_namespace(a_token_cache  *tokens,
                                       a_scope        *namespace_scope);

enum a_module_scope_push_kind {
  mspk_unattempted,     /* No push was performed. */
  mspk_unneccessary,    /* The required scope was already in use. */
  mspk_new              /* A new scope was pushed. */
};  /* a_module_scope_push_kind */

extern void push_module_declaration_context(
                                  a_scope_ptr              scope,
                                  a_module_scope_push_kind *scope_push_status);

extern void pop_module_declaration_context(
                                   a_module_scope_push_kind scope_push_status);

extern void set_template_decl_info_for_class_definition(
				a_template_decl_info_ptr	tdip,
				a_type_ptr			class_type);

extern void function_contains_generic_lambda(void);

extern a_boolean push_template_instantiation_scope(
                            a_template_decl_info_ptr	decl_info,
                            a_type_ptr			assoc_type,
                            a_routine_ptr		assoc_routine,
                            a_symbol_ptr		instance_sym,
                            a_symbol_ptr		template_sym,
                            a_template_arg_ptr		template_arg_list,
			    a_boolean			push_stop_tokens,
			    a_push_scope_options_set	options);

extern void push_instantiation_scope_for_rescan(a_symbol_ptr	template_sym);

extern void pop_instantiation_scope_for_rescan(void);

extern void push_enclosing_class_scope_for_rescan(
                                                a_type_ptr     enclosing_class,
                                                a_routine_ptr  assoc_routine);

extern void pop_enclosing_class_scope_for_rescan(void);

extern void push_instantiation_scope_for_constraint_type(void);

extern void pop_instantiation_scope_for_constraint_type(void);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern void push_instantiation_scope_for_boxed_enum_type(void);

extern void pop_instantiation_scope_for_boxed_enum_type(void);

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void push_new_top_level_declaration(void);

extern void push_instantiation_scope_for_templ_param_rescan(
                            a_template_decl_info_ptr	decl_info,
                            a_type_ptr			assoc_type,
                            a_routine_ptr		assoc_routine,
                            a_symbol_ptr		instance_sym,
                            a_symbol_ptr		template_sym,
                            a_template_arg_ptr		template_arg_list,
			    a_push_scope_options_set	ps_options);

extern void pop_template_instantiation_scope(void);

extern void update_template_param_symbols(
                          a_template_param_ptr param_list,
                          a_template_arg_ptr   arg_list,
                          a_boolean            partial_argument_list = FALSE);

extern void finish_function_processing_for_function_def(
                                            a_function_def_number n,
                                            a_boolean             only_inline);

/* End a name scope. */
extern void pop_scope(void);
extern void pop_scope_full(a_push_scope_options_set	options);
extern void f_push_namespace_extension_scope(a_namespace_ptr nsp,
					     a_boolean	     force_new_entry);
extern void pop_namespace_extension_scope(void);
extern void f_push_namespace_reactivation_scope(
				a_namespace_ptr		nsp,
				a_boolean		force_new_entry);

/* Macro that calls f_push_namespace_reactivation_scope and supplies a default
   value for the force_new_entry parameter. */
#define push_namespace_reactivation_scope(nsp)				\
  f_push_namespace_reactivation_scope(nsp, /*force_new_entry=*/FALSE)

/* Macro that calls f_push_namespace_extension_scope and supplies a default
   value for the force_new_entry parameter. */
#define push_namespace_extension_scope(nsp)				\
  f_push_namespace_extension_scope(nsp, /*force_new_entry=*/FALSE)

extern void pop_namespace_reactivation_scope(void);
extern void push_class_reactivation_scope(a_type_ptr   class_type,
                                          a_boolean    extend_namespace);
extern void pop_class_reactivation_scope(void);
extern void push_instantiation_scope_for_class(
			a_type_ptr	class_type,
			a_boolean	is_microsoft_specialization_scope);
extern void push_class_and_template_reactivation_scope(
                                 a_type_ptr	class_type,
                                 a_boolean      reactivate_template_params,
                                 a_boolean	extend_namespace);
extern void push_class_and_template_reactivation_scope_full(
		a_type_ptr			class_type,
		a_boolean			reactivate_template_params,
		a_boolean			is_specialization,
		a_boolean			extend_namespace,
		a_boolean			force_new_context,
		a_push_scope_options_set	options);

extern a_scope_pointers_block *get_pointers_block_for_scope(a_scope_ptr scope);

extern
a_scope_depth scope_depth_of_symbol(a_symbol_ptr  sym,
                                    a_boolean     *is_local_to_function);
extern a_boolean namespace_is_enclosed_by_scope(a_symbol_ptr             sym,
                                                a_scope_stack_entry_ptr  ssep);
/*
Call namespace_is_enclosed_by_scope for the current scope.
*/
#define namespace_is_enclosed_by_curr_scope(sym)                     \
  (namespace_is_enclosed_by_scope((sym), &scope_stack[decl_scope_level]))

extern a_boolean current_class_symbol_if_class_template(a_symbol_ptr *sym);

extern a_boolean scope_of_class_is_active(a_type_ptr  tp);

extern a_function_shareable_constants_table_ptr
alloc_function_shareable_constants_table(void);

extern void free_function_shareable_constants_table(
			a_function_shareable_constants_table_ptr fsctp);

extern void add_active_using_directive(a_using_decl_ptr udp,
				       a_scope_depth    depth);

extern a_boolean routine_defined(a_routine_ptr  rp);

extern void report_for_init_difference(a_symbol_ptr       sym,
                                       a_source_position  *pos);

extern void report_excessive_rescan_depth(void);

extern void push_name_linkage(a_name_linkage_kind  kind);

extern void pop_name_linkage(void);

extern void set_needed_flags_at_end_of_file_scope(a_scope_ptr scope);

a_boolean keep_function_body_for_possible_inlining(a_routine_ptr routine);

extern void set_active_using_list_scope_depths(
				a_scope_depth		starting_depth,
                                a_boolean		set_value,
				a_decl_sequence_number	effective_decl_seq);

extern void clear_scope_pointers_block(a_scope_pointers_block_ptr  spbp);

extern void wrapup_namespace_scopes(a_scope_ptr scope_ptr);

extern
void wrapup_scope(a_scope_ptr			scope_ptr,
                  a_scope_kind			kind,
                  a_scope_pointers_block_ptr	pointers_block,
                  a_boolean 	                is_namespace_wrapup,
                  a_boolean 	                is_local_reactivation,
		  a_push_scope_options_set	options);

extern a_type_ptr get_curr_variadic_param_type(an_expr_node_ptr	expr);

extern
a_template_decl_info_ptr get_specified_template_decl_info(
					a_boolean	innermost);

#define get_curr_template_decl_info()					\
  (check_assertion(depth_innermost_instantiation_scope !=		\
                   NO_SCOPE_DEPTH &&					\
   scope_stack[depth_innermost_instantiation_scope].			\
                                        template_decl_info != NULL),	\
   scope_stack[depth_innermost_instantiation_scope].template_decl_info)

a_template_arg_ptr get_curr_variadic_arg_for_param(
			a_template_param_coordinate_ptr	coordinates,
			a_boolean			is_rescan,
			a_template_param_ptr		templ_param,
			a_boolean			create_if_not_found);

extern a_pack_reference_ptr alloc_pack_reference(a_pack_reference_kind	kind);

extern a_pack_reference_ptr copy_pack_reference(a_pack_reference_ptr	prp);

extern a_pack_reference_ptr copy_pack_references_in_token_range(
				a_token_sequence_number		first_token,
				a_token_sequence_number		last_token);

extern void rerecord_pack_references(a_pack_reference_ptr		prp);

extern a_pack_expansion_descr_ptr alloc_pack_expansion_descr(void);

extern void add_pack_expansion_descr_to_prototype_arg(
					a_template_param_ptr	templ_param,
					a_template_arg_ptr	templ_arg);

extern a_boolean any_packs_referenced(void);

extern a_boolean any_packs_referenced_in_curr_context(void);

extern a_boolean begin_rescan_pack_expansion_context(
		a_pack_expansion_descr_ptr		pedp,
		a_template_param_ptr			templ_param_list,
		a_template_arg_ptr			templ_arg_list,
		a_pack_expansion_stack_entry_ptr	*p_pesep,
		a_ctws_options_set              	options,
		a_ctws_state_ptr			ctws_state,
		a_boolean				*err);

extern a_boolean pack_expansion_maps_to_unexpanded_pack(
		a_pack_expansion_descr_ptr		pedp,
		a_template_param_ptr			templ_param_list,
		a_template_arg_ptr			templ_arg_list);

extern void begin_pack_deduction_context(
		a_pack_expansion_descr_ptr		pedp,
		a_template_param_ptr			templ_param_list,
		a_template_arg_ptr			*templ_arg_list,
		a_pack_expansion_stack_entry_ptr	*p_pesep);

extern void push_expansion_suppression(
			a_pack_expansion_stack_entry_ptr	*p_pesep);

extern void pop_expansion_suppression(
			a_pack_expansion_stack_entry_ptr	pesep);

extern void begin_prescan_context(
	a_boolean		suppress_packs,
	a_boolean		*packs_suppressed,
	a_pack_expansion_stack_entry_ptr
				*pack_expansion_stack_entry,
	a_boolean		*saved_in_disambiguation,
	a_boolean		*saved_source_sequence_entries_disallowed);

extern void end_prescan_context(
	a_boolean		packs_suppressed,
	a_pack_expansion_stack_entry_ptr
				pack_expansion_stack_entry,
	a_boolean		saved_in_disambiguation,
	a_boolean		saved_source_sequence_entries_disallowed);

extern a_boolean in_generic_lambda_in_prototype_instantiation(void);

extern a_routine_ptr enclosing_nonlambda_routine_for_lambda_class(
                                                  a_type_ptr  *p_lambda_class);

extern a_boolean generic_lambda_is_in_specialized_routine(
                                                       a_routine_ptr  call_op);

extern a_boolean in_generic_lambda_in_class_template_friend(void);

extern a_boolean is_nested_in_real_instantiation(void);

extern a_boolean begin_potential_pack_expansion_context_full(
		a_pack_expansion_stack_entry_ptr	*p_pesep,
		a_pack_expansion_descr_ptr		*p_pedp,
		a_boolean				is_lookahead,
		a_boolean				allow_empty_list,
		a_boolean				ignore_suppression,
		a_boolean				claim_pack_index);

extern a_boolean begin_potential_pack_expansion_context(
			a_pack_expansion_stack_entry_ptr	*p_pesep);

extern void advance_to_next_deduced_element(
				a_pack_expansion_stack_entry_ptr	pesep);

extern void end_pack_deduction_context(
			a_pack_expansion_stack_entry_ptr	pesep);

extern a_pack_expansion_descr_ptr end_potential_pack_expansion_context(
			a_pack_expansion_stack_entry_ptr	pesep,
			a_boolean				is_declarator);

extern void discard_pack_references_for_context(
			a_pack_expansion_stack_entry_ptr	pesep);

extern void begin_tentative_pack_expansion_context(void);

extern void end_tentative_pack_expansion_context(a_boolean);

extern void suppress_expansion_with_no_packs_diagnostic(
			a_pack_expansion_stack_entry_ptr	pesep);

extern a_boolean is_non_initial_variadic_element(void);

extern
a_boolean advance_to_next_pack_element(a_pack_expansion_stack_entry_ptr	pesep);

extern
void abandon_potential_pack_expansion_context(
				a_pack_expansion_stack_entry_ptr	pesep);

extern
a_boolean reset_enclosing_packs_for_pack_index(
				a_pack_expansion_stack_entry_ptr	pesep);

extern
a_boolean skip_pack_index_iteration(
			a_pack_expansion_stack_entry_ptr	*p_pesep,
			a_pack_expansion_descr_ptr		pedep,
			a_boolean				*p_any_more);

extern void record_potential_pack_reference_full(
				a_symbol_ptr		pack_symbol,
				a_source_position_ptr	position,
				a_type_ptr		bases_type,
				a_boolean		direct_bases);

inline void record_potential_pack_reference(a_symbol_ptr          pack_symbol,
                                            a_source_position_ptr position)
/*
Interface to record_potential_pack_reference_full for the most common
case where only a pack_symbol and position are provided.  This also
does some tests to avoid the call in most cases.
*/
{
  if (is_variadic_template_context() && is_template_dependent_context()) {
    record_potential_pack_reference_full(pack_symbol, position,
                                         (a_type_ptr)NULL,
                                         /*direct_bases=*/FALSE);
  }  /* if */
}  /* record_potential_pack_reference */


extern void restore_default_template_params(a_template_param_ptr  tpp,
                                            a_boolean             packs_only);

extern
void update_template_param_symbols_for_param_list(a_template_param_ptr	tpp);

#if GNU_EXTENSIONS_ALLOWED
extern a_type_ptr get_type_for_bases_operator(
				a_type_ptr		bases_type,
				a_source_position_ptr	position,
				a_boolean		direct_bases);
#endif /* GNU_EXTENSIONS_ALLOWED */

extern
void record_pack_expansion_ellipsis_position(a_source_position  *ellipsis_pos);

extern void record_pack_expansion_ellipsis(void);

extern a_boolean in_deprecated_or_unavailable_definition(void);

extern a_boolean in_ms_nonreal_class_instantiation(void);

extern void scope_stk_one_time_init(void);

extern void scope_stk_trans_unit_init(void);

extern void scope_stk_init(void);

#if DEBUG
extern void db_pack_tokens(a_pack_expansion_descr_ptr	pedp);

extern int db_scope_kind(a_scope_kind sck);

extern void db_scope_stack_entry_at_depth(a_scope_depth  depth);

extern void db_scope_stack_entry(a_scope_stack_entry_ptr ssep);

extern void db_scope_stack(void);

extern void db_top_of_scope_stack(int entries);

extern void db_scope_stack_stats(void);

#if EXTRA_SOURCE_POSITIONS_IN_IL
extern void db_source_range(a_source_range *range);

extern void db_decl_pos_info(a_symbol_ptr sym);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

extern unsigned long db_show_scope_stack_space_used(unsigned long grand_total);
#endif /* DEBUG */

#if DO_IL_LOWERING
#if MODULE_ID_NEEDED && !STANDALONE_UTILITY_PROGRAM
extern void lower_functions_waiting_for_module_id(void);
#endif /* MODULE_ID_NEEDED && !STANDALONE_UTILITY_PROGRAM */

extern a_boolean should_delay_lowering_on_function(
                                           a_routine_ptr routine,
                                           a_boolean     at_initial_scope_pop);

#endif /* DO_IL_LOWERING */

extern a_boolean should_delay_finishing_of_function_body(
						a_routine_ptr	routine);

extern a_scope_ptr get_innermost_function_scope(void);

extern a_scope_depth get_depth_innermost_function_scope(void);

/*
Return TRUE if we are in a pack expansion context that is not a suppression.
*/
#define in_pack_expansion()					\
  (pack_expansion_stack != NULL && !pack_expansion_stack->is_suppression)

/*
Return TRUE if we are in a context where Microsoft compilers do not appear to
instantiate a class template to ensure that candidate functions (friend
functions or member operators) are seen.  (This is a conservative
approximation; the actual behavior of Microsoft compilers is unclear.  Newer
Microsoft compilers fixed this.)
*/
#define ms_does_not_complete_class_for_candidate_decl()                      \
  (scope_stack_top().in_decltype_context &&                                  \
   microsoft_version < 1900 &&                                               \
   (scope_stack_top().is_rescan ||                                           \
    scope_stack_top().function_partial_instantiation))


extern a_boolean src_seq_entries_permitted_in_il(void);

typedef Ptr_map<a_type_ptr, a_lambda_ptr>
		a_closure_class_to_lambda_map;

EXTERN_THREAD a_closure_class_to_lambda_map
		*closure_class_to_lambda_map;
			/* A map from lambda closure class types to the
			   corresponding a_lambda entries.  Populated when
			   a lambda header is started. */


inline a_lambda_ptr get_lambda_for_closure_class(a_type_ptr  closure_class)
/*
If closure_class is a lambda closure class that has been recorded in
closure_class_to_lambda_map, return the corresponding a_lambda entry.
Otherwise return NULL.
*/
{
  return (closure_class != NULL) ?
           closure_class_to_lambda_map->get(closure_class) :
           (a_lambda_ptr)NULL;
}  /* get_lambda_for_closure_class */


inline a_lambda_ptr get_lambda_for_scope_depth(a_scope_depth  sd)
/*
The given scope depth is for an sck_function scope.  If the associated function
is a lambda call operator, return the corresponding a_lambda entry.  Otherwise,
return NULL.
*/
{
  check_assertion(sd != NO_SCOPE_DEPTH);
  return get_lambda_for_closure_class(
                    parent_class_or_null(scope_stack[sd].assoc_routine));
}  /* get_lambda_for_scope_depth */


extern a_scope_depth get_curr_lambda_depth(void);


inline a_lambda_ptr get_current_lambda(void)
/*
Return the lambda associated with the innermost currently-active lambda body,
or NULL if there is no such lambda.  Note that while parsing the header of a
nested lambda, this produces the enclosing lambda.
*/
{
  a_scope_depth  depth = get_curr_lambda_depth();

  return depth != NO_SCOPE_DEPTH ? get_lambda_for_scope_depth(depth)
                                 : (a_lambda_ptr)NULL;
}  /* get_current_lambda */


inline a_boolean in_code_from_module(void)
/*
Return TRUE if the current code is generated from a compiled module file (e.g.,
a Microsoft IFC file).
*/
{
  return scope_stack_top().module_load_context_count > 0;
}  /* in_code_from_module */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef SCOPE_STK_H */

