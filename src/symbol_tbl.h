/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

symbol_tbl.h - Declarations related to symbol table processing.

*/

/* Avoid including these declarations more than once. */
#ifndef SYMBOL_TBL_H
#define SYMBOL_TBL_H 1

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/* Type used for the options set for the name lookup routines.  This
   is declared here to prevent recursion problems. */
typedef unsigned int an_id_lookup_options_set;

/* Declare pointer types up front to minimize mutual recursion problems. */
typedef struct a_symbol        *a_symbol_ptr;
typedef struct a_symbol_header *a_symbol_header_ptr;
typedef struct a_macro_param   *a_macro_param_ptr;
typedef struct a_macro_def     *a_macro_def_ptr;
typedef struct a_vla_fixup     *a_vla_fixup_ptr;
typedef struct an_extern_type_fixup *an_extern_type_fixup_ptr;
typedef struct a_template_param *a_template_param_ptr;
typedef struct an_access_error_descr *an_access_error_descr_ptr;
typedef struct a_template_cache_segment *a_template_cache_segment_ptr;
typedef struct a_template_decl_info *a_template_decl_info_ptr;
typedef struct a_template_instance *a_template_instance_ptr;
typedef struct a_nondependent_call_info *a_nondependent_call_info_ptr;
typedef struct a_template_cache *a_template_cache_ptr;
typedef struct a_control_flow_descr a_control_flow_descr_dummy_typedef;
typedef struct a_tmpl_decl_state a_tmpl_decl_state_dummy_typedef;
typedef struct an_exception_spec_error_descr
                                          *an_exception_spec_error_descr_ptr;
typedef struct a_gnu_attribute  a_gnu_attribute_dummy_typedef;
typedef struct a_symbol_list_entry *a_symbol_list_entry_ptr;
typedef struct a_hash_table *a_hash_table_ptr;
typedef struct a_param_id *a_param_id_ptr;
typedef struct a_pack_expansion_stack_entry *a_pack_expansion_stack_entry_ptr;
typedef struct a_type_list_entry *a_type_list_entry_ptr;
typedef struct a_namespace_list_entry *a_namespace_list_entry_ptr;

/* The pointer types to a_routine_fixup and an_initializer_fixup are declared
   here even though the struct themselves are defined in class_decl.c.  This
   allows the pointer to be made available to symbol_tbl.h without creating
   recursive reference problems.  Similarly, a_deferred_constraint_check
   is declared in templates.h. */
typedef struct a_routine_fixup *a_routine_fixup_ptr;
typedef struct an_initializer_fixup *an_initializer_fixup_ptr;
#if MICROSOFT_EXTENSIONS_ALLOWED
typedef struct a_deferred_constraint_check *a_deferred_constraint_check_ptr;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/* The pointer to a_def_arg_expr_fixup is declared here even though the struct
   itself is defined in def_arg.h.  This allows the pointer to be made
   available to symbol_tbl.h without creating recursive reference problems. */
typedef struct a_def_arg_expr_fixup *a_def_arg_expr_fixup_ptr;

/* The pointer to a_pending_pragma is declared here even though the struct
   itself is defined in pragma.h.  This allows the pointer to be made
   available to symbol_tbl.h without creating recursive reference problems. */
typedef struct a_pending_pragma *a_pending_pragma_ptr;

using a_shared_pending_pragma = Shared_obj<a_pending_pragma>;
			/* The type used for a pending pragma potentially
			   shared between multiple pending pragma lists. */

using a_pending_pragma_list = Dyn_array<a_shared_pending_pragma>;
			/* The type used for a list of pending pragmas. */


/* The pointer to a_translation_unit is declared here even though the struct
   itself is defined in trans_unit.h.  This allows the pointer to be made
   available to symbol_tbl.h without creating recursive reference problems. */
typedef struct a_translation_unit *a_translation_unit_ptr;

/* Some other things declared up front to avoid mutual recursion problems. */
/*
A symbol-reference kind is a bit vector whose values are defined in
symbol_ref.h.  The typedef declaration is here to avoid mutual inclusion
problems.
*/
typedef unsigned long a_symbol_reference_kind;

/* Unique sequence number identifying a declaration in a given scope. */
typedef uint32_t a_decl_sequence_number;
  
EXTERN_THREAD a_decl_sequence_number
		decl_seq_counter;
			/* Counter, initialized to 0 with each compilation
			   unit, for maintaining the declaration sequence
			   numbers for symbols. */

/*
Set the declaration sequence number of the symbol pointed to by sym.
*/
#define set_decl_sequence_number(sym)					\
  ((sym)->decl_seq = ++decl_seq_counter)

/*
The special value used to represent an unset declaration sequence number.
*/
#define NO_DECL_SEQUENCE_NUMBER ((a_decl_sequence_number)(0))

/*
A special value used to indicate a declaration sequence number that is always
visible.  Also used as the starting value of decl_seq_counter.
*/
#define FIRST_DECL_SEQUENCE_NUMBER ((a_decl_sequence_number)(1))

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

/*
The definition of a_symbol_locator refers to declarations from il_def.h,
but lexical.h requires a_symbol_locator to be defined.  So the former is
included here, and the latter is included after a_symbol_locator is
declared.
*/
#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */
#ifndef IL_TO_STR_H
#include "il_to_str.h"
#endif /* ifndef IL_TO_STR_H */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
The scope number for the file scope.  Equal to FILE_SCOPE_NUMBER (zero)
except in secondary translation units (i.e., when export template is used).
*/
EXTERN_THREAD a_scope_number
		file_scope_number;

EXTERN_THREAD a_translation_unit_ptr
		*trans_unit_for_scope;
			/* A dynamically allocated array of translation
			   unit pointers indexed by scope number. */


typedef struct a_symbol_locator {
  /* Data structure used to store information about an identifier token.
     Can be used to look up the identifier or enter it into the symbol
     table.  This is also used for tok_decltype_construct to store the
     type of a decltype operator that has been scanned. */
  /* If you change this structure, be sure to also change the initialization
     of global variable cleared_locator in symbol_tbl_one_time_init. */
  a_symbol_header_ptr
		symbol_header;
			/* The symbol header for the list of symbols with the
			   identifier.  When this is NULL, this locator is
			   for an error symbol. */
  a_source_position
		source_position;
			/* The source position to be used when this symbol
			   is entered.  When a qualified name is scanned,
			   this source position points to the final component
			   of the name, while pos_curr_token points to the
			   beginning of the entire qualified name. */
  a_bit_field	is_qualified_name:1;
			/* TRUE if the "identifier" is a C++ qualified-name
			   (e.g., "A::x" or "::y").  specific_symbol points
			   to the proper symbol. */
  a_bit_field	is_global_qualified_name:1;
			/* TRUE if the "identifier" is a C++ qualified-name
			   that begins with a unary "::" (e.g., "::y" or
			   ::A::x). */
  a_bit_field	is_file_scope_qualified_name:1;
			/* TRUE if the "identifier" is a C++ qualified-name
			   that refers to a file scope entity (e.g., ::y
			   but not ::A::x). */
  a_bit_field	is_operator_name:1;
			/* TRUE if the "identifier" is a C++ overloaded
			   operator name, of the form "operator<token>",
			   e.g., "operator+".  Cannot be TRUE when
			   is_conversion_name is TRUE. */
  a_bit_field	is_conversion_name:1;
			/* TRUE if the "identifier" is a C++ user-defined
			   conversion name, of the form "operator <type-name>",
			   e.g., "operator int".  Cannot be TRUE when
			   is_operator_name is TRUE. */
  a_bit_field	is_destructor_name:1;
			/* TRUE if the "identifier" is a C++ destructor
			   name, of the form "~<name>". */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	is_finalizer_name:1;
			/* TRUE if the "identifier" is a C++/CLI finalizer
			   name, of the form "!<name>". */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_bit_field	is_udl_operator_name:1;
			/* TRUE if the "identifier" is a literal-operator-id.
			   I.e., a constructor like
			     operator "" X
			   or
			     operator ""X
			   where X is an identifier. */
  a_bit_field	is_semivisible_nested_type:1;
			/* TRUE if specific_symbol points to a nested type
			   that is not actually visible, except as a C++
			   anachronism (ARM 18.3.5). */
  a_bit_field	access_control_error_reported:1;
			/* TRUE if an accessibility error has already been
			   issued on the associated symbol. */
  a_bit_field	has_been_coalesced:1;
			/* TRUE if the identifier has already been processed
			   by is_generalized_identifier_start -- even if
			   no coalescing was actually performed.  This
			   indicates that no processing is needed should
			   is_generalized_identifier_start be called again. */
  a_bit_field	is_vacuous_destructor_reference:1;
			/* TRUE if the identifier is a destructor name of
			   a type that has no destructor.  Used for
			   explicit destructor invocations of the form
			   p->int::~int.  The type can be a nonclass type
			   or a class type with no destructor.  (Also used for
			   C++/CLI finalizers.) */
  a_bit_field	is_nonclass_destructor:1;
			/* TRUE for vacuous destructor references for 
			   nonclass types such as int::~int or i::~i
			   where "i" is a typedef name.  (Also used for
			   C++/CLI finalizers.) */
  a_bit_field	is_inheriting_ctor:1;
			/* TRUE when a qualified name following a "using" token
			   denotes an inheriting constructor. */
  a_bit_field	is_error:1;
			/* TRUE if an error has been diagnosed on the use
			   of the associated identifier and no symbol should
			   be entered into the symbol table. */
  a_bit_field	do_not_clear_specific_symbol:1;
			/* TRUE if the specific symbol field of the locator
			   should not be cleared when clear_specific_symbol is
			   called.  This is set when clearing the specific
			   symbol field would result in the loss of information
			   that cannot be recovered by repeating the lookup
			   process.  This is TRUE for template references that
			   have been coalesced, locators resolved from pseudo
			   tokens, and for specific symbol error locators. */
  a_bit_field	is_implicitly_qualified:1;
			/* TRUE if the specific symbol is implicitly qualified
			   (such as a resolved pseudo token). */
  a_bit_field	is_template_id:1;
			/* TRUE if the coalesced identifier is a template-id
			   (i.e., template-name < template-arg-list >). */
  a_bit_field	is_class_member:1;
			/* TRUE if is_qualified_name is TRUE and the entity
			   pointed to by the parent field is a class or enum
			   type (not a namespace). */
  a_bit_field	is_unknown_template_reference:1;
			/* TRUE if this is a reference to a nonreal template
			   that was uncoalesced by ensure_correct_nonreal-
			   instance_kind. */
  a_bit_field	qualifier_is_super:1;
			/* TRUE if the "identifier" is a C++ qualified-name
			   in which the qualifier is the Microsoft __super
			   keyword.  This is TRUE only if __super is the
			   only qualifier present. */
  a_bit_field	is_super_qualified:1;
			/* TRUE if a qualified name began with the Microsoft
			   __super keyword.  This is TRUE even if there
			   are other qualifiers after __super. */
  a_bit_field	is_decltype_qualified:1;
			/* TRUE if a qualified name began with a decltype
			   specifier. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	is_property_or_event_accessor:1;
			/* TRUE if the identifier is a Microsoft property
			   or event accessor function. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_bit_field	is_template_param:1;
			  /* TRUE if normal_id_lookup found a template
			     parameter name. */
  a_bit_field	is_splicer:1;
			  /* TRUE if this locator is the result of a splicer
			     construct. */
  a_symbol_ptr	specific_symbol;
			/* If is_qualified_name is TRUE, this points to the
			   specific symbol for the qualified name.  Otherwise,
			   if this pointer is non-NULL, it is the result of
			   the most recent lookup of this identifier (e.g.,
			   by normal_id_lookup). */
  a_parent_class_or_namespace
		parent;
			/* If is_qualified_name is TRUE, this points to either
			   the type or the namespace specified by the
			   qualifier (depending on the value of the
			   is_class_member flag).  If is_vacuous_destructor
			   is TRUE this points to the type of the qualifier,
			   which may not actually be a class type (e.g.,
			   for int::~int this will point to the type "int").
			   This may point to an enumeration type if the
			   qualifier is a C++11-mode or Microsoft-mode enum
			   qualifier.  For tok_ptr_to_member tokens (e.g.,
			   "A::*"), this points to the class type before the
			   "::" and is NULL if the type before the "::" is not
			   a class type.  In C++/CLI mode for constructs like
			   X::typeid, a tok_cli_typeid token is created and
			   this points to the type before the ::typeid,
			   which can be a non-class type. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_symbol_ptr
		property_or_event_parent;
			/* If is_property_or_event_accessor is TRUE, this
			   points to the property set or the event data member
			   (sk_field or sk_static_data_member) associated with
			   the accessor. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_template_arg_ptr
		template_arg_list;
			/* When a function template symbol, or an overloaded
			   function symbol, is followed by a template argument
			   list, this points to the argument list that was
			   specified.  Typically, the reference cannot be
			   coalesced to a pointer to a template instance until
			   the function type is known.  This also points to the
			   template argument list for a variable template
			   symbol.  For a class template symbol it only points
			   to the template argument list if it is lexically
			   different from the one associated with the type and
			   record_form_of_name_reference is TRUE. */
  a_name_qualifier_ptr
		name_qualifier;
			/* When recording the form of name references, this
			   provides detailed information about how the
			   qualifier portion of the name (if any) was
			   specified.  The pointer is NULL if there was no
			   qualifier, if there was an error processing the
			   qualifier, or if we are in a prototype instantiation
			   and we are not recording prototype instantiations
			   in the IL. */
  union {
    /* When is_operator_name, is_conversion_name, is_destructor_name, and
       is_finalizer_name are all FALSE, the variants are undefined. */
    /* When is_operator_name is TRUE: */
    an_opname_kind
		opname;
			/* For an operator name, the kind of operator. */
    /* When is_conversion_name is TRUE: */
    a_type_ptr  conversion_result_type;
			/* The return type when a user-defined conversion
			   name is scanned. */
    /* When is_destructor_name or is_finalizer_name is TRUE: */
    a_type_ptr	destructor_type;
			/* This is the type of the name that follows the "~"
			   in a destructor name or the "!" in a finalizer name.
			   This field is only guaranteed to be non-NULL for
			   destructor and finalizer names from field selection
			   operations. */
    /* When curr_token is tok_decltype_construct: */
    a_type_ptr	decltype_type;
			/* The type of a decltype operator that has been
			   scanned. */
  } variant;
} a_symbol_locator;

/*
If a locator refers to a class member, return a pointer to the parent class
type, otherwise return NULL.
*/
#define qualifier_class_type(locator)					\
  ((locator).is_class_member ? (locator).parent.class_type : (a_type_ptr)NULL)

/*
If a locator refers to a namespace member, return a pointer to the parent
namespace, otherwise return NULL.
*/
#define qualifier_namespace_ptr(locator)				\
  ((locator).is_class_member ? (a_namespace_ptr)NULL			\
                             : (locator).parent.namespace_ptr)


EXTERN_THREAD a_symbol_locator
		cleared_locator;
			/* An empty locator used to initialize locators in
                           macro clear_locator. */

/*
Clear a symbol locator.
*/
#define clear_locator(locator, position)                              \
{  *(locator) = cleared_locator;                                      \
   (locator)->source_position = *position;                            \
}  /* clear_locator */

/* Mark a symbol locator to indicate an error and prevent its entry
   into the symbol table. */
#define set_to_error_locator(loc)                                     \
{  clear_locator(&(loc), &error_position); (loc).is_error = TRUE; }

/* Like set_to_error_locator, but preserving information about the
   identifier with which the locator is associated. */
#define set_to_named_error_locator(loc)                               \
{  (loc).is_error = TRUE; (loc).specific_symbol = NULL; }

/* Test a locator to see if it is an error locator. */
#define is_error_locator(loc) ((loc).is_error)

/* Test a locator to see if it is a destructor or finalizer locator. */
#if MICROSOFT_EXTENSIONS_ALLOWED
#define is_dtor_like_locator(loc)                                     \
  ((loc).is_destructor_name || (loc).is_finalizer_name)
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define is_dtor_like_locator(loc)                                     \
  ((loc).is_destructor_name)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/* Retrieve a pointer to the symbol list from a locator. */
#define symbol_list_from_locator(loc) ((loc).symbol_header->symbol)

/* Retrieve a pointer to the inactive symbol list from a locator. */
#define inactive_symbol_list_from_locator(loc)                        \
  ((loc).symbol_header->inactive_symbols)

/* Retrieve a pointer to the symbol list on which file scope symbols are
   located.  This is usually the active list, but after the file scope
   has been popped, it is the inactive list. */
#define symbol_list_for_file_scope_symbols(symhdr)			\
  (file_scope_symbols_are_on_inactive_list ?				\
                       (symhdr)->inactive_symbols : (symhdr)->symbol)

/* Clear the specific symbol field of the locator unless instructed not
   to by the do_not_clear_specific_symbol field of the locator. */
#define clear_specific_symbol(loc)					\
{  if (!((loc).do_not_clear_specific_symbol)) {				\
     (loc).specific_symbol = NULL;					\
     (loc).is_semivisible_nested_type = FALSE;				\
   }									\
}

/* Returns TRUE if locator_for_curr_id refers to a qualified class member of a
   nonreal, non-prototype-instantiation template class. */
#define locator_for_curr_id_is_member_of_nonreal_template_class()       \
  (locator_for_curr_id.is_class_member &&                               \
   is_immediate_class_type(locator_for_curr_id.parent.class_type) &&    \
   locator_for_curr_id.parent.class_type->                              \
                         variant.class_struct_union.is_nonreal_class && \
   locator_for_curr_id.parent.class_type->                              \
                        variant.class_struct_union.is_template_class && \
   !locator_for_curr_id.parent.class_type->                             \
                   variant.class_struct_union.is_prototype_instantiation)

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#ifndef LEXICAL_H
#include "lexical.h"
#endif /* ifndef LEXICAL_H */
#ifndef MEM_TABLES_H
#include "mem_tables.h"
#endif /* ifndef MEM_TABLES_H */
#ifndef SCOPE_STK_H
#include "scope_stk.h"
#endif /* ifndef SCOPE_STK_H */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
Kinds of symbols in the symbol table.
If this is changed, the definitions for name_space_for_symbol_kind
and symbol_kind_names should also be changed.
*/
enum a_symbol_kind : a_byte {
  sk_keyword,    	/* Language keyword. */
  sk_macro,       	/* Preprocessor macro. */
  sk_constant,		/* Constant (enumerator). */
  sk_type,		/* Typedef'd type. */
  sk_class_or_struct_tag,
			/* Tag of a struct, or C++ class type. */
  sk_union_tag,		/* Tag of a union, or C++ union type. */
  sk_enum_tag,		/* Tag of an enumeration, or C++ enum type. */
  sk_variable,		/* Variable or parameter. */
  sk_field,		/* Field (member) of a struct or union.  In C++,
			   a non-static data member. */
  sk_static_data_member,/* Static data member of a class. */
  sk_member_function,   /* Member function of a class. */
  sk_routine,		/* Function. */
  sk_label,		/* Label in a function. */
  sk_undefined,		/* Undefined identifier. */
  sk_extern_variable,	/* Definition of a variable with external or internal
			   linkage, used to check that all definitions of a
			   given external/internal name are equivalent. */
  sk_extern_routine,	/* Definition of a routine with external or internal
			   linkage, ditto. */
  sk_projection,	/* Projection of a member symbol from a base class
			   into a derived class. */
  sk_overloaded_function,
			/* C++ overloaded function (member or non-member). */
  sk_parameter,         /* Parameter name in a function prototype. */
  sk_class_template,    /* Definition of a C++ class template. */
  sk_function_template, /* Definition of a C++ function template. */
  sk_variable_template, /* Definition of a C++ variable template. */
  sk_concept_template,  /* Definition of a C++ concept template. */
  sk_namespace,         /* Definition of a C++ namespace. */
  sk_namespace_projection,
		        /* Projection of a member of a namespace into another
			   scope (either through a using-declaration or as a
			   by-product of a lookup). */
  sk_named_module,      /* A C++ named module. */
#if NAMED_ADDRESS_SPACES_ALLOWED
  sk_named_address_space,
                        /* Embedded C (TR 18037) named address space. */
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
#if NAMED_REGISTERS_ALLOWED
  sk_named_register,
			/* Embedded C (TR 18037) named-register storage
			   class. */
#endif /* NAMED_REGISTERS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  sk_property_set,
			/* C++/CLI property. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if EXPENSIVE_CHECKING
  sk_freed,             /* A symbol that has gone out of scope.  */
#endif /* EXPENSIVE_CHECKING */
  sk_last
};


/*
Table of names corresponding to symbol kinds.
*/
EXTERN_CONSTINIT_ARRAY(a_const_char*, symbol_kind_names, sk_last + 1)
#if VAR_INITIALIZERS
= {
   "keyword", "macro", "constant", "type", "class or struct", "union",
   "enum", "variable", "field", "static data member", "member function",
   "routine", "label", "undefined", "extern variable", "extern routine",
   "projection", "overloaded function", "parameter", "class template",
   "function template", "variable template", "concept",
   "namespace", "namespace projection", "module",
#if NAMED_ADDRESS_SPACES_ALLOWED
   "named address space",
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
#if NAMED_REGISTERS_ALLOWED
   "named register",
#endif /* NAMED_REGISTERS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
   "property set",
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if EXPENSIVE_CHECKING
   "freed",
#endif /* EXPENSIVE_CHECKING */
   "last"
}
#endif /* VAR_INITIALIZERS */
EXTERN_CONSTINIT_ARRAY_END(symbol_kind_names)

/* Macro to return the name of a symbol kind. */
#define name_of_symbol_kind(kind) symbol_kind_names[(int)(kind)]


/*
There are different "name spaces" in each scope (see standard, 3.1.2.3).
The following define the name spaces and a mapping from symbol kind to
the associated name space.
*/
enum a_name_space_kind {
  nsk_label,		/* Code labels. */
  nsk_tag,		/* Struct, union, and enum tags, in C.  (In C++,
			   those have kind nsk_other, and this kind is
			   not used.) */
  nsk_other,		/* The primary case: types, constants, variables,
			   functions. */
  nsk_macro,		/* Macros. */
  nsk_keyword,		/* Keywords. */
  nsk_extern,		/* External names of variables and routines, perhaps
			   truncated.  Used to check that all uses of
			   a given external name are equivalent. */
  nsk_member		/* Members (fields) of structs and unions.  Used in
			   C mode only. */
};

EXTERN_THREAD a_name_space_kind
		name_space_for_symbol_kind[(int)sk_last+1];
			/* For each symbol kind, this array maps the kind to
			   the associated name space.  See
			   symbol_tbl_one_time_init for initialization. */

typedef struct a_macro_param {
  /* A parameter of a function-like preprocessor macro.  The names of
     parameters need to be kept around for checking of benign redefinitions
     of macros. */
  char		*name;
			/* Parameter name, null-terminated. */
  a_macro_param_ptr
		next;
			/* Pointer to the next parameter for the same macro,
			   or NULL if this is the last parameter. */
  a_byte_boolean
		need_expanded_form;
			/* TRUE if the parameter is used somewhere in the
			   body of the macro in a context that calls for the
			   macro-expanded form of the argument. */
  a_byte_boolean
		is_operand_of_paste;
			/* TRUE if the parameter is used somewhere in the
			   body of the macro as an operand of the paste
			   (##) operator. */
} a_macro_param;

typedef struct a_macro_def {
  /* For macros defined to the preprocessor: */
  a_bit_field	object_like:1;
			/* TRUE if this macro is object-like (i.e., has no
			   parameters). */
  a_bit_field	cannot_be_redefined:1;
			/* TRUE if this is a predefined macro that cannot
			   be redefined later.  This is TRUE for ANSI
			   predefined macros. */
  a_bit_field	ref_suppresses_pch_file:1;
			/* TRUE if referencing this macro within a header is
			   incompatible with creating a precompiled header
			   file; TRUE, e.g., for predefined macros __DATE__
			   and __TIME__. */
  a_bit_field	variadic:1;
			/* TRUE if the formal parameter list of this macro
			   ended with the ellipsis token (an extension). */
  a_bit_field	is_predefined:1;
			/* TRUE for predefined macros. */
  a_macro_param_ptr
		param_list;
			/* Pointer to a list of entries describing the
			   formal parameters of this macro.  NULL if
			   the macro is object-like or has no parameters. */
  char		*repl_text;
			/* Replacement body for the macro.  Contains cues
			   on where to insert argument values, do pasting,
			   etc.  See below.  NULL for special macros that
			   require code expansion (e.g., __LINE__). */
#if RECORD_MACROS_IN_IL
  a_macro_ptr	macro;	/* The IL macro entry.  NULL for predefined macros
			   and those defined on the command line. */
#endif /* RECORD_MACROS_IN_IL */
#if FULLY_RESOLVED_MACRO_POSITIONS
  a_macro_text_map
		text_map;
			/* Map from offsets into repl_text to the original
			   source locations from which the text came. */
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
} a_macro_def;

/*
Entry used to save and restore macro definition information for use
by the push_macro and pop_macro pragmas.  A stack of such entries is
pointed to by the symbol header.
*/
typedef struct a_saved_macro_state *a_saved_macro_state_ptr;
typedef struct a_saved_macro_state {
  a_saved_macro_state_ptr
		next;	/* Pointer to the next macro state entry for a given
			   symbol header, or NULL if this is the last entry
			   on the stack. */
  a_symbol_ptr	symbol;
			/* The macro symbol to which the macro name refers at
			   the point at which the push_macro appeared.  This
			   will be NULL if the macro was undefined at that
			   point. */
  a_macro_def
		macro_def;
			/* The macro definition of the symbol at the point at
			   which the push_macro appeared.  This is not set
			   (i.e., all of the fields will be cleared) if the
			   macro was undefined at that point.  This is
			   needed if the macro is redefined without first
			   being undefined. */
} a_saved_macro_state;

extern void push_macro_pragma(a_pending_pragma_ptr	ppp);

extern void pop_macro_pragma(a_pending_pragma_ptr	ppp);

/*
The repl_text string for a macro definition is a sequence of sections,
each of which defines some part of the replacement text of the macro.
A null follows the last sequence and ends the repl_text string.  The
sequences are:
*/
enum a_repl_text_seq_kind {
  rt_unused,
			/* Equivalent to '\0' (null); unused to avoid
			   confusion with LE_ESCAPE in a replacement
			   text. */
  rt_end,
			/* Marks the end of the replacement text. */
  rt_text,
			/* Raw text.  Followed by 3 bytes containing a
			   character count, and then that many characters
			   of raw text. */
  rt_paste,
			/* "##" token.  This is just a placeholder and not
			   actual replacement text. */
  rt_raw_argument,
			/* Raw string for argument.  Followed by 3 bytes
			   containing the argument number (first argument is
			   numbered 1).  "raw arguments" are used for arguments
			   adjacent to "##" and all arguments in pcc
			   mode. */
  rt_stringized_raw_argument,
			/* Same as rt_raw_argument, but the argument raw
			   string is turned into a string literal (see
			   [cpp.stringize]).  If the "argument number" is
			   MAX_REPL_TEXT_NUMBER, the operand of # is not a
			   parameter but a __VA_OPT__ operator whose
			   operand is to be stringized, and in
			   configurations in which
			   FULLY_RESOLVED_MACRO_POSITIONS is set to TRUE,
			   this section will be followed by
			   2*sizeof(a_source_position) bytes containing the
			   source positions of the __VA_OPT__ operator and
			   its closing right parenthesis. */
  rt_charized_raw_argument,
			/* Same as rt_stringized_raw_argument, except that
			   the argument raw string is turned into a char
			   literal instead (Microsoft extension); the
			   special arrangements supporting a __VA_OPT__
			   operator do not apply. */
  rt_argument,		/* Macro-expanded string for argument.  Followed by 3
			   bytes containing the argument number, as for 
			   rt_raw_argument. */
  rt_microsoft_magic_arg_marker,
			/* Like rt_paste in an extended variadic macro in
			   that it consumes a comma preceding an empty
			   __VA_ARGS__ substitution, but without actually
			   pasting the following token to the preceding
			   text.  Used to support the Microsoft variety of
			   variadic macros. */
  rt_microsoft_maybe_raw_argument,
			/* An argument string that might be raw or
			   expanded, depending on its subsequent use.
			   Normally the Microsoft preprocessor expands
			   macro arguments, even when they are the operand
			   of a paste, but if the argument is concatenated
			   with a preceding "(" or "," and the result is
			   passed in the expanded text to a macro that uses
			   it as an operand of a paste, the raw argument is
			   used.  Followed by 3 byes containing the
			   argument number, as for rt_raw_argument. */
  rt_optional_text
			/* Begins the text corresponding to the argument of
			   a __VA_OPT__ operator, which will be included in
			   the replacement text only if the replacement for
			   __VA_ARGS__ is non-empty.  Followed by 3 bytes
			   containing the number of characters to skip
			   forward in the repl_text string (beginning with
			   the byte following this section header to the
			   start of the section following the sections
			   representing the argument) if __VA_ARGS__ is
			   empty. */
};


/*
Fetch a multi-byte number from a macro-definition string.  Return it in
num.  rtp points to the first byte of the number; it is advanced
past the number on return.
*/
#define NUM_BYTES_IN_MULTI_BYTE_REPL_TEXT_NUMBER 3

#define MAX_REPL_TEXT_NUMBER                                                  \
(NUM_BYTES_IN_MULTI_BYTE_REPL_TEXT_NUMBER >= sizeof(sizeof_t)                 \
 ? (sizeof_t)(-1)                                                             \
 : ~((sizeof_t)(-1) << (CHAR_BIT * NUM_BYTES_IN_MULTI_BYTE_REPL_TEXT_NUMBER)))

#define get_macro_repl_text_number(num, rtp)            \
{ sizeof_t temp = 0;                                    \
  temp  = (sizeof_t)*(a_byte *)rtp++;                   \
  temp |= (sizeof_t)(*(a_byte *)rtp++) << CHAR_BIT;     \
  temp |= (sizeof_t)(*(a_byte *)rtp++) << (CHAR_BIT*2); \
  num = temp;                                           \
}  /* get_macro_repl_text_number */


/*
Put a multi-byte number (num) into a macro-definition string. rtp points
to the first byte of the number; it is advanced past the number on return.
*/
#define PN_BYTE_MASK ((1 << CHAR_BIT) - 1)
#define put_macro_repl_text_number(num, rtp)                          \
{ sizeof_t temp = num;                                                \
  check_assertion(temp <= MAX_REPL_TEXT_NUMBER);                      \
  *(a_byte *)rtp++ = (a_byte)(temp                   & PN_BYTE_MASK); \
  *(a_byte *)rtp++ = (a_byte)((temp >> CHAR_BIT)     & PN_BYTE_MASK); \
  *(a_byte *)rtp++ = (a_byte)((temp >> (CHAR_BIT*2)) & PN_BYTE_MASK); \
}  /* put_macro_repl_text_number */


/*
Kinds of fixup to be performed on entries in the dependent type fixup list.
*/
enum a_dependent_type_fixup_kind : a_byte {
  dtfk_arg_transfer_method,	
			/* Set the arg transfer method flag in a param type. */
  dtfk_routine_calling_method,
			/* Set the routine calling method flag in a routine
			   type. */
  dtfk_array_type_size,	/* Set the size of an array type. */
  dtfk_array_of_abstract_class_check
			/* Check whether an array of an incomplete type became
			   an array of an abstract class type when that class
			   type was completed. */
};


/*
Entries identifying array types, routine types, and parameters that are
dependent on an incomplete class type and that must be fixed up when the
class is completed.  For example, the array types must have their sizes
computed (note: arrays of incomplete struct are an extension).
*/
typedef struct a_dependent_type_fixup *a_dependent_type_fixup_ptr;
typedef struct a_dependent_type_fixup {
  a_dependent_type_fixup_ptr
		next;
			/* Next fixup on the list, or NULL if this is the
			   last. */
  a_source_position
		decl_position;
			/* Source position at which a diagnostic is to be
			   issued, if required. */
  a_dependent_type_fixup_kind
		fixup_kind;
			/* The kind of fixup to be applied to the entity. */
  a_tagged_pointer
		entity;
			/* A pointer to a type or to a param type.  When the
			   entity is a type, it may be an array whose
			   underlying element type is an incomplete class type
			   or it may be a routine type whose return type is an
			   incomplete class type; when the entity is a param
			   type, the type is an incomplete class type.  The
			   fixup takes place when the incomplete class type
			   is completed. */
} a_dependent_type_fixup;


typedef struct a_symbol_list_entry {
  /* Entry created to produce a list of symbols for some special purpose.
     (For example, such a list is created to track the user-defined conversion
     functions for a given class type.  The symbol pointed to will be an
     sk_member_function or sk_projection symbol identifying a function to
     convert a class object to another type.) */
  a_symbol_list_entry_ptr
		next;
			/* Next in a linked list of symbol list entries; NULL
			   for the last on the list. */
  a_symbol_ptr  symbol;
			/* Pointer to a symbol entry. */
} a_symbol_list_entry;


typedef struct a_type_list_entry {
  /* Entry created to produce a list of types for some special purpose.
     For example, such a list is used when building a list of associated
     types for namespace/class directed lookup. */
  a_type_list_entry_ptr
		next;
			/* Next in a linked list of type list entries; NULL
			   for the last on the list. */
  a_type_ptr	type;
			/* Pointer to a type entry. */
} a_type_list_entry;

#if MICROSOFT_EXTENSIONS_ALLOWED

/*
An entry used to represent a list of symbols that should be considered by
a function call using a C++/CLI hide-by-sig ref class or interface.

Given a hierarchy like this:

A   B   D   E
 \ /     \ /
  C       F
    \   /
      G

A flattened version of the tree is represented as a list.  So if all of
the nodes had symbols, the list would look like (where the number for
each entry is its level):

G-0 -> C-1 -> A-2 -> B-2 -> F-1 -> D-2 -> E-2

A node is only on the list if it or one of its child nodes has a non-NULL
symbol pointer.
*/
typedef struct a_hide_by_sig_list_entry *a_hide_by_sig_list_entry_ptr;
typedef struct a_hide_by_sig_list_entry {
  a_hide_by_sig_list_entry_ptr
		next;
			/* Next in a list of entries, or NULL for the
			   last on the list. */
  a_symbol_ptr  symbol;
			/* Pointer to the symbol entry for this level, or
			   NULL if this level has no symbol. */
  a_base_class_ptr
		base_class;
			/* The bass class in which this symbol was found, or
			   NULL if the symbol was found in the derived
			   class. */
  uint32_t	level;
			/* The level associated with this entry. */
} a_hide_by_sig_list_entry;

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

typedef struct a_substituted_type_list_entry
			*a_substituted_type_list_entry_ptr;
typedef struct a_substituted_type_list_entry {
  /* Entry that points to a template argument list and a routine type
     that resulted from substituting the template parameters of the
     associated template with the template arguments. */
  a_substituted_type_list_entry_ptr
		next;
			/* Next in a linked list of type list entries; NULL
			   for the last on the list. */
  a_template_arg_ptr
		templ_arg_list;
			/* Pointer to a template argument list that was used
			   to create type.  This argument list may contain
			   unspecified template arguments (i.e., template
			   arguments with NULL type or constant pointers). */
  a_ctws_options_set
		options;
			/* The CTWS options used to create the type. */
  a_type_ptr	type;
			/* Pointer to a type entry. */
} a_substituted_type_list_entry;


typedef struct a_namespace_list_entry {
  /* Entry created to produce a list of namespaces for some special purpose.
     For example, the list of namespaces in which operators may be found
     for an argument of a given class type. */
  a_namespace_list_entry_ptr
		next;
			/* Next in a linked list of namespace list
			   entries; NULL for the last on the list. */
  a_namespace_ptr  ptr;
			/* Pointer to a namespace entry. */
} a_namespace_list_entry;

/* Forward definition. */
typedef struct a_template_symbol_supplement *a_template_symbol_supplement_ptr;


typedef struct a_class_symbol_supplement *a_class_symbol_supplement_ptr;
typedef struct a_class_symbol_supplement {
  /* Additional information about a C++ class, struct, or union, supplementing
     the information residing in the class's symbol entry. */
  a_symbol_ptr	symbols;
			/* Symbol entries for members of the class. */
  a_symbol_ptr	constructor;
			/* Pointer to either an sk_member_function symbol (when
			   there is only one constructor defined for the class)
			   or an sk_overloaded_function symbol (when there are
			   more than one); NULL if there is none.  The set
			   may comprise user-declared constructors, an
			   implicitly-declared (trivial or nontrivial) copy
			   constructor, or an implicitly-declared nontrivial
			   default constructor.  The set is empty if the
			   class has only an implicitly-declared trivial
			   default constructor and an implicitly-declared
			   trivial copy constructor. */
  a_symbol_ptr	trivial_default_constructor;
			/* When constructor is NULL and is_cpp03_POD is FALSE,
			   pointer to an sk_member_function symbol for the
			   trivial default constructor; it is never actually
			   called (that's why it's not in the constructor set
			   for this class), and the associated routine entry
			   is not added to the IL, but its definition may
			   nevertheless require diagnostics:
			     class X { const int i; };
			     X x;
			   X is not a POD (private member) and the implicit
			   definition of X::X() is ill-formed.  This may also
			   point to a defaulted (and hence user-declared)
			   default constructor that is trivial (in that case,
			   the constructor also appears on the list pointed to
			   by constructor).  NULL in all other cases.  Note:
			   when an implicitly declared default constructor is
			   nontrivial, it appears on the constructor list for
			   the class. */
  a_symbol_ptr	destructor;
			/* Pointer to an sk_member_function symbol that
			   identifies the destructor for this class; NULL if
			   there is none. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_symbol_ptr  static_constructor;
                        /* Pointer to an sk_member_function symbol when there
                           is a C++/CLI static constructor defined for the
                           class; NULL if there is none. */
  a_symbol_ptr	finalizer;
			/* Pointer to an sk_member_function symbol that
			   identifies the C++/CLI finalizer for this class;
			   NULL if there is none. */
  a_symbol_ptr	idisposable_dispose;
			/* Pointer to an sk_member_function symbol that
			   identifies the member function that implements
			   IDisposable::Dispose() and which further derived
			   classes should invoke in their implementation of
			   Dispose(bool).  It will be a member of this class
			   or one of its bases, or NULL if no chaining should
			   occur.  This function is only part of the dispose
			   pattern if has_dispose_pattern_idisposable_dispose
			   is TRUE. */
  a_symbol_ptr	dispose_bool;
			/* Pointer to an sk_member_function symbol that
			   identifies the virtual Dispose(bool) member
			   function that should be overridden or hidden by
			   derived class implementations of the dispose
			   pattern.  It will be a member of this class or one
			   of its bases, or NULL if no such function exists.
			   This function is only part of the dispose pattern
			   if has_dispose_pattern_idisposable_dispose and/or
			   has_dispose_pattern_object_finalize is TRUE. */
  a_symbol_ptr	object_finalize;
			/* Pointer to an sk_member_function symbol that
			   identifies the member function that overrides
			   Object::Finalize(); NULL if no such function
			   exists.  This function is only part of the dispose
			   pattern if has_dispose_pattern_object_finalize is
			   TRUE. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_symbol_ptr  assignment_operator;
			/* Pointer to a symbol (sk_member_function or
			   sk_overloaded_function) symbol that identifies
			   the assignment operator for this class; NULL if
			   there is none. */
  a_symbol_list_entry_ptr
		conversion_list;
			/* Pointer to a linked list of entries providing
			   quick access to user-defined conversion functions
			   declared for this class.  Once the class has been
			   defined this list is exhaustive, including one
			   entry for each target type for which a conversion
			   is defined.  Inherited conversion functions are
			   represented by projection symbols. */
  a_symbol_list_entry_ptr
		conversion_template_list;
			/* Pointer to a linked list of entries identifying
			   conversion operator templates declared for this
			   class. Symbols pointed to are (or are projection
			   symbols referring to) sk_function_template symbols.
			   Instances of the conversion operator templates
			   are not added to conversion_list but are listed
			   under the template on which they are based. */
  a_routine_fixup_ptr
		routine_fixup_list;
			/* Pointer to a list of entities used in the token
			   caching and delayed scanning scheme required for
			   C++ member functions (routine bodies and default
			   arguments). */
  an_initializer_fixup_ptr
		initializer_fixup_list;
			/* Pointer to a list of entities used in the token
			   caching and delayed scanning scheme required for
			   C++ in-class initializers. */
  a_symbol_ptr  class_template;
                        /* Pointer to a class template symbol.  Present
                           only when this class is an instantiation of
                           a class template, NULL otherwise.  Note that
			   this is NULL for a class nested within a
			   class template (except for member templates)
			   even if the nested class was defined outside
			   of the class template. */
  a_template_symbol_supplement_ptr
		template_info;
			/* Pointer to associated template information when
			   the associated class is a prototype instantiation
			   of a class template or a nested class of a class
			   template.  NULL for other classes including 
			   other instantiations of the template. */
  a_source_position
		instantiation_position;
			/* For a nonspecialized template class that has been
			   fully instantiated, this is the position of the
			   reference that caused the instantiation. */
  a_scope_number
		member_decl_scope;
			/* Scope number of members of the class.  For
			   normal classes this is set by push_scope.  For
			   proxy and nonreal classes this is assigned when
			   a lookup is done. */
  a_decl_sequence_number
		num_unparsed_field_initializers;
			/* The number of field initializers in this class
			   that haven't yet been parsed.  (When this number
			   drops to zero, certain properties of the class can
			   be established.) */
  a_symbol_ptr	corresp_prototype_sym;
			/* If the class is a template class instance, or a
			   class nested within a template class, this points
			   to the corresponding prototype instantiation
			   class.  Otherwise, it is NULL. */
  a_token_sequence_number
		prototype_token_sequence_number;
			/* The token sequence number of a token that
			   represents this class.  Present for the prototype
			   instantiation of a class template and the
			   prototype instantiation of any nested classes
			   within the class template. */
  a_namespace_ptr
		referencing_namespace;
			/* For template classes this contains a pointer
			   to the namespace in which the use that
			   first required the instantiation of the template
			   was encountered.  NULL if the first reference
			   was in the global namespace.  This field is
			   set when the instantiation_required flag is
			   set. */
  a_dependent_type_fixup_ptr
		dependent_type_fixup_list;
			/* If the current class is not yet defined, a pointer
			   to a list of entries identifying arrays, function
			   types, and parameters that are dependent on it and
			   require fixup when it is completed.  Once the class
			   is defined, the pointer is cleared. */
  a_namespace_list_entry_ptr
		operator_lookup_namespaces;
			/* Pointer to a list of namespaces that are the
			   parent namespaces of this class or one of its
			   base classes.  This is the list of namespaces
			   that must be searched for an operand of this
			   class type.  The entire list or some portion
			   of the end of the list may be shared between
			   classes in a given namespace. */
  a_symbol_ptr	friend_functions;
			/* Pointer to a list of non-class-member functions
			   declared as friends of the current class.  The
			   symbols on the list will be sk_namespace_projection
			   symbols that point to an sk_routine symbol that
			   is a friend (or sk_overloaded_function symbols that
			   point to sk_namespace_projection symbols).  This
			   list is used to assist with namespace and class
			   directed lookup. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_symbol_ptr	super_lookup_symbols;
			/* A list of symbols created when doing a Microsoft
			   __super lookup.  This list is consulted for
			   subsequent lookups so that the symbols may be
			   reused. */
  a_symbol_ptr	default_indexed_properties;
			/* A pointer to an sk_property_set symbol representing
			   the default-indexed properties of this class, or
			   NULL if there are no such properties. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_name_qualifier_ptr
		name_qualifiers;
			/* Points to a list of the various forms of name
			   qualifiers used to name this class.  This is used
			   to find a previously allocated entry so that it
			   can be reused. */
  a_type_ptr	prev_entry_on_types_list;
			/* When non-NULL, this points to an entry believed
			   to have the associated class type as its "next"
			   pointer.  This is used to optimize the performance
			   of move_to_end_of_types_list. */
#if NEED_NAME_MANGLING
  a_discriminator
		discriminator;
			/* An identifying number used to distinguish multiple
			   entities with the same name in the same function
			   in the name mangling for the IA-64 ABI.  In file,
			   namespace, and class scopes, a sequence number used
			   to distinguish unnamed class types (in both ABIs).
			   Zero if not needed. */
#endif /* NEED_NAME_MANGLING */
  a_bit_field	has_nontrivial_default_constructor:1;
			/* TRUE if a default constructor has been explicitly
			   declared or a nontrivial default constructor has
			   implicitly declared for this class. */
  a_bit_field	has_user_declared_default_constructor:1;
			/* TRUE if a default constructor has been explicitly
			   declared for this class. */
  a_bit_field	has_user_provided_default_constructor:1;
			/* TRUE if a default constructor has been explicitly
			   declared for this class, and the first declaration
			   was not defaulted. */
  a_bit_field	has_copy_constructor:1;
			/* TRUE if a copy constructor (possibly a move
			   constructor) has either been declared or generated
			   for the class. */
  a_bit_field	has_copy_constructor_for_const_object:1;
			/* TRUE if there is a copy constructor (possibly a move
			   constructor) for the class and it can be used to
			   copy a const object. */
  a_bit_field	has_user_provided_copy_constructor:1;
			/* TRUE if a copy constructor (possibly a move
			   constructor) has been user-provided (i.e.,
			   explicitly declared, and the first declaration was
			   not defaulted). */
  a_bit_field	has_user_declared_move_constructor:1;
			/* TRUE if a move constructor has been explicitly
			   declared for this class. */
  a_bit_field	has_user_provided_move_constructor:1;
			/* TRUE if a move constructor has been user-provided
			   (i.e., explicitly declared, and the first
			   declaration was not defaulted). */
  a_bit_field	has_deleted_copy_or_move_constructor:1;
			/* TRUE if a copy/move constructor is deleted. */
  a_bit_field	has_trivial_destructor:1;
			/* TRUE if the destructor is trivial.  This could be
			   an implicitly-declared destructor (destructor will
			   be NULL), or a defaulted destructor (pointed to by
			   destructor).  FALSE otherwise.  (Note that for
			   nonreal classes, this field can be FALSE even if
			   destructor is NULL).  For testing the presence of
			   a nontrivial destructor, the macro
			   has_nontrivial_destructor is often preferable. */
  a_bit_field	has_user_declared_move_assign_operator:1;
			/* TRUE if a move assignment operator has been
                           explicitly declared for this class. */
  a_bit_field	has_user_provided_move_assign_operator:1;
			/* TRUE if a move assignment operator has been user-
			   provided (i.e., explicitly declared, and the first
			   declaration was not defaulted). */
  a_bit_field	has_deleted_copy_or_move_assign_operator:1;
			/* TRUE if a copy/move assignment operator is
			   deleted. */
  a_bit_field	assignment_by_bitwise_copy_allowed:1;
			/* TRUE if assignment can be performed by a bitwise
			   copy rather than by calling an assignment operator
			   function (i.e., when the assignment operator is
			   not user-defined and when the current class has no
			   virtual base classes and no subobjects for which
			   bitwise copy is not allowed). */
  a_bit_field	construction_by_bitwise_copy_allowed:1;
			/* TRUE if copy construction can be performed by a
			   bitwise copy rather than by calling a copy
			   constructor function. */
  a_bit_field	makes_copy_construction_nontrivial:1;
  a_bit_field	makes_move_construction_nontrivial:1;
  a_bit_field	makes_copy_assignment_nontrivial:1;
  a_bit_field	makes_move_assignment_nontrivial:1;
			/* TRUE if having this class as a subobject (field or
			   base) makes the generated copy/move constructor or
			   generated copy/move assignment operator nontrivial.
			   This can be FALSE even when the corresponding
			   ...by_bitwise_copy flag is FALSE because in C++03 a
			   volatile class field cannot be bitwise-copied
			   (because the corresponding constructor signature
			   has a "const&" type, and that doesn't admit a
			   volatile argument), but it doesn't make the copy
			   function nontrivial either. */
  a_bit_field	contains_vtable:1;
			/* TRUE if the class has a virtual function or a
			   virtual base class, or if any of its subobjects
			   has a virtual function or a virtual base class. */
  a_bit_field	has_auto_conversion_function:1;
			/* TRUE if this class has at least one conversion
			   function member whose type involves the "auto" or
			   "decltype(auto)" type specifiers. */
  a_bit_field	target_of_conversion_function:1;
			/* TRUE if this class is the target of a user-defined
			   conversion function (for conversion from another
			   class to this class). */
  a_bit_field	any_ref_member:1;
			/* TRUE if this class has any fields of reference
			   type. */
  a_bit_field	is_class_aggregate:1;
			/* TRUE if the class has no constructors, no base
			   classes (prior to C++17), no private or protected
			   members, and no virtual functions. */
  a_bit_field	is_cpp03_POD:1;
			/* TRUE if the class is a "POD" (in the C++03 sense: an
			   aggregate with further restrictions that make it
			   look like a C struct or union).  Always FALSE in
			   C mode.  Use the function is_pod_class to test
			   whether a given class type is a POD in the current
			   language mode. */
  a_bit_field	is_pod_class:1;
			/* TRUE if the class satisfies the POD requirements for
			   the current language mode (the is_pod_class function
			   has been called).  FALSE if the class does not
			   satisfy the POD requirements *or* these requirements
			   have not yet been checked.  See pod_checked
			   below. */
  a_bit_field	pod_checked:1;
			/* TRUE if the class has been checked to see if it
			   satisfies the POD requirements for the current
			   language mode (the is_pod_class function has been
			   called).  See is_pod_class above. */
  a_bit_field	has_operator_new:1;
			/* TRUE if a member operator new() has been declared
			   for this class or a class from which it derived. */
  a_bit_field	has_operator_array_new:1;
			/* TRUE if a member operator new[]() has been
			   declared for this class or a class from which it
			   is derived. */
  a_bit_field	has_operator_delete:1;
			/* TRUE if a member operator delete() has been
			   declared for this class or a class from which it
			   is derived. */
  a_bit_field	has_operator_array_delete:1;
			/* TRUE if a member operator delete[]() has been
			   declared for this class or a class from which it
			   is derived. */
  a_bit_field	has_two_argument_operator_array_delete:1;
			/* TRUE if a member operator delete[]() having two
			   arguments has been declared for this class or a
			   class from which it is derived. */
  a_bit_field	any_nonstatic_data_members:1;
			/* TRUE if the class or any of its base classes has
			   one or more nonstatic data members. */
  a_bit_field	any_nonreal_base_classes:1;
			/* For a prototype instantiation this is TRUE
			   if any of its base classes are nonreal classes.
			   This flag is only set for nonreal bases with unknown
			   members, not for nonreal base classes that are
			   prototype instantiations or local classes. */
  a_bit_field	any_dependent_base_classes:1;
			/* TRUE if any of the base classes should be ignored
			   during dependent lookup. */
  a_bit_field	any_template_dependent_fields:1;
			/*  For a prototype instantiation this is TRUE if any
			    field is dependent on a template parameter. */
  a_bit_field	instantiation_in_progress:1;
			/* For an real instantiation, this is TRUE if the
			   full instantiation is in the process of being
			   generated. */
  a_bit_field	default_arg_fixup_pass_1_started:1;
			/* TRUE if pass 1 of the fixup of default arguments
			   of this class has begun.  This is used to prevent
			   the fixup process from being called recursively. */
  a_bit_field	default_arg_fixup_pass_2_started:1;
			/* TRUE if pass 2 of the fixup of default arguments
			   of this class has begun.  This is used to prevent
			   the fixup process from being called recursively. */
#if IA64_ABI
  a_bit_field	has_empty_class_subobject:1;
			/* TRUE if a (field or base) subobject has an empty
			   class type.  The subobject could be an indirect
			   base or field.  This is also TRUE for a class that
			   is itself empty.  Computed during layout. */
#endif /* IA64_ABI */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_bit_field	definition_is_first_decl:1;
			/* TRUE when the first declaration of this class in
			   the translation unit is the definition. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  a_bit_field	lambda_inside_default_arg_expression:1;
			/* TRUE if this class is the closure class for a lambda
			   that occurs inside a default argument expression. */
  a_bit_field	lambda_immediately_inside_default_arg_expression:1;
			/* TRUE if this class is the closure class for a lambda
			   that occurs immediately inside a default argument
			   expression, i.e., this would be FALSE for a lambda
			   that is nested inside a lambda that is immediately
			   inside a default argument expression. */
  a_bit_field	lambda_in_invalid_scope:1;
			/* TRUE for the closure class of a lambda expression
			   that appeared in an invalid scope (e.g., a
			   template declaration scope).  An error will have
			   been issued that the lambda cannot appear in a
			   constant expression, but this flag is used to
			   improve the error recovery for such cases. */
  a_bit_field	lambda_subject_to_trans_unit_corresp:1;
			/* TRUE for the closure class of a lambda expression
			   that is subject to ODR ("one-definition rule")
			   constraints across translation units.  Specifically,
			   these are lambdas appearing in a class definition,
			   in the initializer for a static data member of a
			   class template, in an inline function body, or in
			   a function template body. */
  a_bit_field	base_check:1;
			/* TRUE if this class was defined with the C++11
			   "base_check" attribute, which in turn requires
			   diagnosing "accidental" hiding and overriding. */
  a_bit_field	check_hiding_attr:1;
			/* TRUE if this class includes a member declared with
			   the C++11 "hiding" attribute. */
  a_bit_field	has_field_with_attr_to_merge:1;
			/* TRUE if this class has a field with an attribute
			   whose must_be_preserved_in_trans_unit_copy flag is
			   TRUE. */
  a_bit_field	standard_layout:1;
			/* TRUE if this is a "standard layout" class as
			   defined by C++11. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	disable_dispose_pattern_implementation:1;
			/* TRUE if check_for_reserved_dispose_pattern_member
			   found a Dispose(), Dispose(bool), or Finalize()
			   function declaration in the class.  This is used to
			   prevent the implementation of the dispose pattern to
			   avoid spurious errors caused by redeclarations of
			   those functions. */
  a_bit_field	checked_for_dispose_pattern:1;
			/* TRUE if a check for an existing dispose pattern
			   implementation has already been performed. */
  a_bit_field	is_disposable:1;
			/* TRUE if the class implements the IDisposable
			   interface. */
  a_bit_field	any_disposable_data_members:1;
			/* TRUE if the class itself has one or more ref class
			   data members that implement IDisposable::Dispose. */
  a_bit_field	has_dispose_pattern_idisposable_dispose:1;
			/* TRUE if this class implements the dispose pattern
			   and has an implementation of IDisposable::Dispose();
			   idisposable_dispose is that function's symbol. */
  a_bit_field	has_dispose_pattern_object_finalize:1;
			/* TRUE if this class implements the dispose pattern
			   and has an implementation of Object::Finalize();
			   object_finalize is that function's symbol. */
  a_bit_field	needs_new_idisposable_dispose:1;
			/* TRUE if this class or one of its base classes
			   implements a virtual Dispose() that either did not
			   implement IDisposable::Dispose() or was sealed.
			   Derived classes that need to implement
			   IDisposable::Dispose() will therefore need to mark
			   their declaration with the "new" modifier. */
  a_bit_field	from_vccorlib:1;
			/* TRUE if this class was defined while processing the
			   vccorlib.h header. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_bit_field	being_defined:1;
			/* TRUE during the call of scan_class_definition for
			   this class.  This is used to detect certain problems
			   in code generated from metadata. */
  a_bit_field	may_need_fixups:1;
			/* Similar to being_defined, but remains TRUE until
			   fixup processing is complete.  This also indicates
			   that the class is tracked on a list of class that
			   may need fixups. */
  a_bit_field	union_member_with_initializer:1;
			/* TRUE if this class is a union and has a member with
			   a default member initializer. */
  a_bit_field	variant_member_with_nontrivial_default_ctor:1;
			/* TRUE if this class has a variant member with a
			   nontrivial default constructor.  FALSE if the
			   anonymous union containing this member also has a
			   (possibly the same) member with a default member
			   initializer that overrides this case
			   (see union_member_with_initializer).  (If this field
			   is TRUE, possible with unrestricted unions only, a
			   generated default constructor is implicitly
			   deleted.) */
  a_bit_field	variant_member_with_nontrivial_copy_ctor:1;
			/* TRUE if this class has a variant member with a
			   nontrivial copy constructor. */
  a_bit_field	variant_member_with_nontrivial_move_ctor:1;
			/* TRUE if this class has a variant member with a
			   nontrivial move constructor. */
  a_bit_field	variant_member_with_nontrivial_dtor:1;
			/* TRUE if this class has a variant member with a
			   nontrivial destructor. */
  a_bit_field	variant_member_with_nontrivial_copy_assign:1;
			/* TRUE if this class has a variant member with a
			   nontrivial copy assignment operator. */
  a_bit_field	variant_member_with_nontrivial_move_assign:1;
			/* TRUE if this class has a variant member with a
			   nontrivial move assignment operator. */
  a_bit_field	known_to_be_a_literal_type:1;
			/* TRUE if this class is known to be a literal type. */
  a_bit_field	known_not_to_be_a_literal_type:1;
			/* TRUE if this class is known not to be a literal
			   type. */
  a_bit_field	has_constexpr_nonstatic_member_function:1;
			/* TRUE if the class has a nonstatic member function
			   that is constexpr. */
  a_bit_field	scanning_field_initializer:1;
			/* TRUE while scanning a field initializer of this
			   class. */
  a_bit_field	has_instantiatable_field_initializers:1;
			/* TRUE if this class has field initializers that are
			   instantiated "on-demand" (normally this is the case
			   for field initializers of template instances). */
  a_bit_field	has_initializer_fixups:1;
			/* TRUE if this class as associated initializer fixups
			   (which may be recorded on the initializer_fixup_list
			   of an enclosing class). */
  a_bit_field	default_ctor_body_delayed:1;
			/* TRUE if the generation of the default constructor
			   body has been delayed. */
  a_bit_field	base_classes_fixed:1;
			/* TRUE if the base classes (if any) have been
			   determined (except perhaps for some C++/CLI bases
			   that are added implicitly late in the definition
			   process). */
  a_scope_pointers_block
		pointers_block;
			/* A block of pointers that are logically part of the
			   scope stack entry for the associated class
			   -- including a pointer to a linked list of all
			   symbols declared in the class and pointers to
			   the last entries in linked lists of IL entries
			   entered in the associated IL scope. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  a_scope_depth	ss_list_depth;
			/* The scope stack depth that contains the source
			   sequence list on which the source sequence entry
			   for this class is recorded. */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
} a_class_symbol_supplement;


typedef struct an_enum_symbol_supplement *an_enum_symbol_supplement_ptr;
typedef struct an_enum_symbol_supplement {
  /* Additional information about an enum type, supplementing the information
     residing in the type's symbol entry. */
  a_dependent_type_fixup_ptr
		dependent_type_fixup_list;
			/* If the enum type is not yet defined (possible as
			   an extension in both C and C++ modes), a pointer
			   to a list of entries identifying arrays that are
			   dependent on it and require fixup when it is
			   completed.  Once the enum is defined, the pointer
			   is cleared. */
  a_name_qualifier_ptr
		name_qualifiers;
			/* Points to a list of the various forms of name
			   qualifiers used to name this enum.  This is used
			   to find a previously allocated entry so that it
			   can be reused. */
#if NEED_NAME_MANGLING
  a_discriminator
		discriminator;
			/* An identifying number used to distinguish multiple
			   entities with the same name in the same function
			   in the name mangling for the IA-64 ABI.  In file,
			   namespace, and class scopes, a sequence number used
			   to distinguish unnamed enum types (in both ABIs).
			   Zero if not needed. */
#endif /* NEED_NAME_MANGLING */
  a_symbol_ptr	template_sym;
			/* Pointer to the symbol for the prototype
			   instantiation of the enumeration.  This is set
			   regardless of whether the actual list of enumeration
			   elements has been seen yet (i.e., for an enum
			   where the enumerators are provided in an
			   out-of-class declaration. */
  a_template_symbol_supplement_ptr
		template_info;
			/* Pointer to associated template information when
			   the associated enum is a prototype instantiation
			   of a scoped enumeration declared in a class
			   template or a nested class of a class template.
			   NULL for other enum types. */
  a_source_position
		instantiation_position;
			/* For a nonspecialized template enum that has been
			   instantiated, this is the position of the
			   reference that caused the instantiation. */
  a_bit_field	instantiated:1;
			/* TRUE for an enumeration that is a member of a
			   class template (or nested class thereof) for which
			   an instantiation of the enumerators has been
			   done. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	replaced_enum_symbol:1;
			/* TRUE for an enumeration type in Microsoft mode that
			   was declared but not defined, and later defined
			   with an explicit underlying type other than "int".
			   E.g.:     enum E ee;
			             enum E: char { e };  // New type E.
			   The definition introduces a new type and hence a
			   new symbol; the symbol resulting from the original
			   declaration is marked using this flag. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
} an_enum_symbol_supplement;


/* Used to track the number of pending instantiations of a given class. */
typedef uint32_t a_pending_instantiation_count;

/* Used to track the number of instantiations performed in tim_all mode that
   were not actually required. */
typedef short an_unused_instantiation_count;


typedef struct a_field_symbol_supplement *a_field_symbol_supplement_ptr;
typedef struct a_field_symbol_supplement {
  /* Additional information about a field, supplementing the information
     residing in the field's symbol entry. */
  a_token_sequence_number
		token_sequence_number;
			/* This is used to match find the initializer from
			   a prototype instantiation for a field in a
			   real instantiation. */
  a_shared_token_cache
		token_cache;
			/* For a field that is a member of a template class
			   (including prototype and real instantiations)
			   this points to the cache containing the
			   initializer, if any.  For real instantiations,
			   this is copied from the entry from the prototype
			   instantiation to the entry for the real
			   instantiation when the real instantiation of the
			   enclosing class is done.  NULL if there is no
			   initializer, for fields of non-template classes,
			   and for fields of template classes if an
			   instantiation has been done. */
  a_field_symbol_supplement_ptr
		prototype_field;
			/* For a field that is a member of an instance of
			   a class template or nested class of a class
			   template, this points to the field symbol
			   supplement of the corresponding field from the
			   prototype instantiation. */
  a_pending_instantiation_count
		pending_instantiations;
			/* The number of instantiations of this template
			   that are in the process of being instantiated.
			   Used to detect runaway recursive instantiations.
			   This is only used for the entry of the field
			   from the prototype instantiation. */
  a_bit_field	being_instantiated:1;
			/* TRUE for a field of a template class that is in
			   the process of being instantiated. */
  a_bit_field	is_variant_member:1;
			/* TRUE if this symbol represents an anonymous union
			   member introduced in a surrounding non-union class.
			   (Nonstandard anonymous unions that aren't actually
			   unions don't count in this context.) */
  a_bit_field	is_first_variant_member:1;
			/* TRUE if this is the first variant member introduced
			   by an anonymous union in the surrounding class. */
  a_bit_field	is_last_variant_member:1;
			/* TRUE if this is the last variant member introduced
			   by an anonymous union in the surrounding class. */
} a_field_symbol_supplement;


typedef struct a_static_data_member_supplement
                                         *a_static_data_member_supplement_ptr;
typedef struct a_static_data_member_supplement {
  /* Additional information about a static data member, supplementing the
     information residing in the member's symbol entry. */
  a_token_sequence_number
		token_sequence_number;
			/* This is used to find the initializer from a
			   prototype instantiation for a member in a real
			   instantiation. */
  a_shared_token_cache
		token_cache;
			/* For a member of a template class (including
			   prototype and real instantiations) this is the cache
			   containing the initializer, if any.  For real
			   instantiations, this is copied from the entry from
			   the prototype instantiation to the entry for the
			   real instantiation when the real instantiation of
			   the enclosing class is done.  This is the default
			   (empty) Shared_obj state if there is no initializer,
			   for members of non-template classes, and for members
			   of template classes if an instantiation has been
			   done. */
  a_symbol_ptr
		prototype_member;
			/* For a member of an instance of a class template or
			   nested class of a class template, this points to
			   the static data member symbol of the corresponding
			   member from the prototype instantiation. */
} a_static_data_member_supplement;


/*
Data structure used to pass information about function declarations back
from the scanning of the function declarator.
*/
typedef struct a_param_id {
  /* Entry giving the name of one parameter in a function declarator.
     The type of the parameter does not appear here; it is in an
     entry of type a_param_type attached to the type entry for the
     function.  The present structure is used both for old-style
     identifier lists and for the names of parameters in prototypes. */
  a_param_id_ptr
		next;
			/* Next parameter id on the list, or NULL if this
			   is the last parameter id. */
  a_symbol_ptr	symbol;
			/* Points to an sk_parameter symbol to represent a
			   parameter name.  It is NULL when a name is omitted
			   in a function prototype.  The symbol pointed to,
			   when present, is transformed into an sk_variable
			   symbol as part of function definition processing. */
  a_type_ptr	type;
			/* For a new- or old-style function parameter, this
			   is its type.  This is usually the same as the
			   information in the function type parameter list,
			   but is kept here also so we can be sure of
			   associating the proper identifier and type
			   in error cases. */
  a_type_ptr	declared_type;
			/* The type as actually declared by the program --
			   before array-to-pointer adjustment and before
			   type-qualifiers are stripped off. */
  a_source_position
		type_pos;
			/* Source position of the start of the type
			   specification of the parameter declaration. */
  a_storage_class
		storage_class;
			/* For a new- or old-style function parameter, this is
			   the storage class to be associated with it when it
			   is declared. */
  a_type_qualifier_set
		eff_top_level_cv_quals:NUM_BITS_FOR_TYPE_QUALIFIER_SET;
			/* Top-level const/volatile qualifiers that have a
			   potential effect on the type.  For example, for a
			   declaration "T const x" where T is "int const",
			   this is TQ_NONE since the top-level qualifier has
			   no effect due to T already being const. */
  a_bit_field	implicitly_declared:1;
			/* TRUE for an old-style parameter for which
			   an explicit declaration is omitted. */
  a_bit_field	is_parameter_pack:1;
			/* TRUE for the parameter of a template definition of
			   a variadic template for the function parameter
			   pack.  (See the similar field in a_param_type for a
			   situation where this flag can be TRUE at the same
			   type as is_pack_element.) */
  a_bit_field	is_pack_element:1;
			/* TRUE for parameters of an actual instantiation of
			   a variadic template for those parameters that are
			   associated with a parameter pack of the original
			   variadic template. */
  a_bit_field	uses_only_enclosing_pack:1;
			/* TRUE for a parameter whose declaration refers only
			   to an enclosing parameter pack (and not a local
			   one).  For example:
			     template<class ... Ts> struct S {
			       template<class F> auto m(F f, Ts... p)
			                                 ->decltype(f(p...));
			     };
			   Parameter p will have this flag set. */
  a_bit_field	is_empty_pack_parameter:1;
			/* TRUE for "dummy" parameters created as placeholders
			   for empty pack expansions.  For example:
			     template<typename... Ts> inline bool g(Ts ...ps) {
			       return [&](auto &... xs) {
			                return ((xs < ps) && ...);
			              }();
			     }
			     bool r = g();
			   During the instantiation of g<>(), the identifier ps
			   in "xs < ps" must still be resolved in the prototype
			   instantiation of the nested lambda.  So we create a
			   dummy dependent argument for it. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_bit_field	is_decl_after_first_in_comma_list:1;
			/* TRUE for an old-style parameter defined in a
			   comma-separated list, but not the first in that
			   list.  For example, for y in:
				void f(x, y) int x, y; {} */
  a_source_sequence_entry_ptr
		source_sequence_entry;
			/* Source-sequence information saved during declarator
			   processing. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  a_variable_ptr
		dummy_vla_variable;
			/* A dummy variable created for scanning a VLA
			   expression that refers to the parameter before
			   its "real" variable entry is allocated. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_range
		specifiers_range,
		declarator_range,
		identifier_range;
			/* Source position information recorded at the point
			   of declaration, to be transferred to the associated
			   parameter variable entry if one is created. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_source_position
		old_style_id_pos;
			/* For an old-style parameter, the source position of
			   the initial reference (i.e., of the declaration
			   within the parenthesized comma-list of parameter
			   names).  null_source_position for a new-style
			   parameter. */
  unsigned int	param_num;
			/* The parameter number (the first parameter is
			   number one). */
} a_param_id;


typedef struct a_func_info_block *a_func_info_block_ptr;
typedef struct a_func_info_block {
  /* Information about a function declarator. */
  a_symbol_ptr	prototype_scope_symbols;
			/* List of symbols in the prototype scope, linked
			   on the next_in_scope field.  NULL if none.
			   Usually NULL.  Only named types (structs/unions/
			   enums) declared within the prototype scope
			   appear on this list. */
  a_param_id_ptr
		param_id_list;
			/* List of entries giving parameter names, NULL if
			   there were none.  Used for both old-style and
			   new-style parameter names. */
  an_exception_specification_ptr
		exception_specification;
			/* An entry (or list of entries) representing an
			   exception specification (C++ only). */
  a_source_position
		throw_position;
			/* Source position of the exception specification (C++
			   only). */
  an_exception_spec_error_descr_ptr
		exception_spec_errors;
			/* Pointer to a linked list of diagnostics that were
			   detected during scanning of exception
			   specifications but that are to be issued later;
			   may be NULL.  C++ only. */
  a_scope_number
		scope_number;
			/* The scope number used for the function prototype
			   scope for the parameters, to be reused for the
			   function scope if a body is found. */
  a_vla_fixup_ptr
                vla_fixup_list;
			/* A list of entries representing fixups that are
			   required resulting from a VLA declaration in a
			   function prototype scope.  Originally the list
			   appears in the sck_func_prototype scope stack
			   entry; it is moved when the scope stack is popped,
			   and the fixups are done if the function prototype
			   is associated with a function definition. */
  a_lambda_ptr	lambda;
			/* If this entry is for a lambda construct, this points
			   to the IL description of the lambda. */
  a_bit_field	any_prototype_names_omitted:1;
			/* TRUE if the parameter list is a prototype list,
			   and it includes at least one parameter with
			   just a type and no name. */
  a_bit_field	is_inline:1;
			/* TRUE if inline was specified (C++ only). */
  a_bit_field	is_definition:1;
			/* TRUE if the current declaration is a definition. */
  a_bit_field	is_defaulted:1;
			/* TRUE if the current declaration is followed by
			   "= default" (is_definition is also TRUE in such
			   cases). */
  a_bit_field	is_deleted:1;
			/* TRUE if the current declaration is followed by
			   "= delete". */
  a_bit_field	is_main_function:1;
			/* TRUE if the function "main". */
  a_bit_field	is_implicit_declaration:1;
			/* TRUE if this is an implicit declaration. */
  a_bit_field	function_type_from_typedef:1;
			/* TRUE if the function type came from a typedef
			   rather than from the declarator.  When it is TRUE,
			   an error will be issued on a function definition
			   and param_id_list and prototype_scope_symbols will
			   be NULL. */
  a_bit_field	any_default_args:1;
			/* TRUE if the function type declaration included
			   the declarations of default arguments. */
  a_bit_field	final:1;
			/* TRUE if the function was declared with the "final"
			   modifier (a context-sensitive keyword). */
  a_bit_field	override:1;
			/* TRUE if the function was declared with the 
			   "override" modifier (a context-sensitive
			   keyword). */
  a_bit_field	keep_param_id_list:1;
			/* TRUE if the param_id_list should not be freed
			   when the func_info_block is no longer needed. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	abstract:1;
			/* TRUE if the function was declared with the C++/CLI
			   "abstract" modifier (a context-sensitive keyword).
			   Only set in some Microsoft C++ modes. */
  a_bit_field	sealed:1;
			/* TRUE if the function was declared with the C++/CLI
			   "sealed" modifier (a context-sensitive keyword).
			   Only set in some Microsoft C++ modes. */
  a_bit_field	new_member:1;
			/* TRUE if the function was declared with the C++/CLI
			   "new" modifier, indicating that a member function
			   does not override a virtual function from a base
			   class. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if ASM_FUNCTION_ALLOWED
  a_bit_field	is_asm_function:1;
			/* TRUE if the function type declaration included the
			   asm specifier. */
#endif /* ASM_FUNCTION_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS
  a_bit_field	is_movable_member_or_friend_def:1;
			/* TRUE if the function is defined inside a class
			   definition but the source-sequence entry for its
			   definition should make it appear to have been
			   defined outside the class.  Only set when
			   NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_
			   SEQUENCE_LISTS is configured to TRUE. */
#endif /* FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS */
  a_source_sequence_entry_ptr
		declarator_ssep;
			/* Source sequence entry for the function
			   declarator. */
  a_type_ptr	declared_type;
			/* The routine type as it actually appears in the
			   current declaration. */
  a_source_sequence_entry_ptr
		prototype_scope_ss_list;
			/* Pointer to the list of source sequence entries
			   generated for the parameter declarations of a
			   function declarator (if any). */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      a_targ_alignment
		max_member_alignment;
			/* If nonzero, the default maximum alignment of any
			   nonstatic data member of any class, struct, or
			   union defined in the body of the function.  The
			   value may be overridden by #pragma pack directives
			   within the function body. */
} a_func_info_block;


/*
Structure that contains the information about a template declaration that
is needed to recreate the context in which tokens from the declaration
should be rescanned when creating an instantiation.

When GET_DEFINITION_OF_CLASS_NEEDED is TRUE, these entries are also used
to reestablish the context for a normal (non-template) class to be defined.
In such cases, the parameters and declaration_scope fields will not be
set (i.e., they will have their default values).
*/
typedef struct a_template_decl_info {
  a_template_param_ptr
		parameters;
			/* The formal template parameters that must be
			   visible when then tokens are rescanned. */
  a_scope_number
		declaration_scope;
			/* The scope number assigned when the template
			   declaration containing these tokens was scanned.
			   This scope needs to be used when the tokens are
			   scanned for the parameter symbols to be visible. */
  a_scope_ptr	enclosing_scope;
			/* The scope containing the template declaration of
			   which these tokens are a part. */
  a_template_decl_info_ptr
		enclosing_template_decl;
			/* If the template declaration appeared as part of a
			   nested template declaration (i.e., a single
			   declaration that includes more than one
			   "template <...>" clause), this points to the
			   template declaration information of the enclosing
			   template declaration information structure (i.e.,
			   the "template <...>" to the left of the current one
			   in the declaration).  Contains NULL for the leftmost
			   "template <...>" clause in a declaration.  When
			   an entry is returned to the available list, this
			   field is used as the pointer to the next entry on
			   the list. */
  a_template_decl_ptr
		template_decl;
			/* Points to the IL template declaration entry for
			   this template declaration. */
  a_name_linkage_kind
		name_linkage;
			/* The default name linkage at the point of the
			   declaration.  This is "reactivated" as the default
			   when a template is instantiated. */
  unsigned short
		n_params;
			/* The number of entries in the parameters list. */
  a_decl_sequence_number
		decl_seq;
			/* The declaration sequence number at the point of
			   the template declaration.  Used during lookup
			   to exclude names not visible at the point of
			   template definition. */
  a_decl_sequence_number
		starting_decl_seq;
			/* For class templates, the declaration sequence
			   number at the point of the template declaration. */
  a_nondependent_call_info_ptr
		nondependent_calls;
			/* A list of entries that describe the nondependent
			   calls within this template.  NULL if no such list
			   exists.  The list is maintained in token sequence
			   number order.  For class templates, this includes
			   the nondependent calls for default argument
			   expressions and bodies of nontemplate member
			   functions. */
  a_nondependent_call_info_ptr
		last_entry_added;
			/* Pointer to the entry most recently added to the
			   list. */
  a_pack_expansion_descr_ptr
		pack_expansions;
			/* For a variadic template, a list of entries that
			   describe the contexts in which any pack expansions
			   occur.  NULL for non-variadic templates or if
			   no pack expansions are used. */
  a_pack_expansion_descr_ptr
		last_pack_expansion;
			/* Pointer to the end of the pack expansion list. */
  a_hash_table_ptr
		constexpr_if_hash_table;
			/* A hash table used at instantiation time to find
			   the cache information for constexpr if that
			   was saved if a prototype instantiation of
			   the function was done. */
  a_symbol_ptr	variable_instance_sym;
			/* For the template declaration entry for a generic
			   lambda instantiation inside a variable template
			   initializer, this is the variable template instance
			   on which the instantiation is based. */
} a_template_decl_info;


/*
Entry used to map a token sequence number to a pointer to some other
entry.  This is used to create a hash table to look up an entry that
corresponds to a token sequence number.
*/
typedef struct a_token_sequence_xref *a_token_sequence_xref_ptr;
typedef struct a_token_sequence_xref {
  a_token_sequence_number
		token_sequence_number;
			/* The token sequence number value being mapped. */
  a_void_ptr	entry;
			/* Pointer to the entry to which the mapped entry
			   refers. */
} a_token_sequence_xref;

/*
Entry used to record front end information for dependent "if constexpr"
statements to permit the discarded parts of a template cache to be
quickly skipped.
*/
typedef struct a_constexpr_if_cache_info *a_constexpr_if_cache_info_ptr;
struct a_constexpr_if_cache_info {
  a_reusable_token_cache
		token_cache;
			/* Pointer to the token cache containing the
			   tokens of the function containing the
			   constexpr if. */
  a_token_sequence_number
		else_start_tsn = NO_TOKEN_SEQUENCE_NUMBER;
			/* The starting token sequence number of the first
			   token of the "else" of the constexpr if. */
  a_token_sequence_number
		end_start_tsn = NO_TOKEN_SEQUENCE_NUMBER;
			/* The starting token sequence number of the closing
			   brace of the constexpr if. */
};  /* a_constexpr_if_cache_info */


/*
Structure used to record information about nondependent calls.
An entry is created during the prototype instantiation of a call.
The list is traversed during a real instantiation, and a matching
entry is returned if a given call is nondependent.  Entries are
matched using a token sequence number.  The list is maintained in
token sequence number order.

For class templates, the list includes nondependent call entries for
default arguments and bodies of nontemplate member functions.
*/
typedef unsigned long a_nondependent_call_depth;
typedef struct a_nondependent_call_info {
  a_nondependent_call_info_ptr
		previous;
			/* The previous entry in the list.  NULL for the first
			   entry. */
  a_nondependent_call_info_ptr
		next;
			/* The next entry in the list.  NULL for the last
			   entry. */
  a_token_sequence_number
		token_sequence_number;
			/* Token sequence number that identifies the location
			   of the call.  For normal calls, this is the
			   number associated with the "(" of the argument
			   list.  For calls made via operators, this is the
			   position of the operator. */
  a_nondependent_call_depth
		depth;
			/* Disambiguator on token_sequence_number.  Usually
			   zero, but for operator-> functions indicates the
			   depth, since chained operator-> replacement
			   calls will have the same token_sequence_number. */
  a_symbol_ptr	symbol;
			/* Pointer to the symbol of the function to be
			   called.  NULL for nondependent calls for which
			   overload resolution must be deferred to the
			   real instantiation. */
  a_bit_field	supplemental:1;
			/* TRUE if the call was to a "supplemental" C++20
			   comparison candidate (which requires a rewrite of
			   the comparison). */
  a_bit_field	reversed_opnds:1;
			/* TRUE if the operands of the call should be reversed
			   (because this is a C++20 implicitly-reversed
			   comparison). */
} a_nondependent_call_info;


/*
Structure that contains supplementary declarative information (much of it
nonstandard, e.g., as used in Microsoft-compatibility mode).  This block is
passed around during declaration processing; its contents may be copied into
IL entries after the appropriate checking is done.
*/
typedef struct a_decl_modifiers_block *a_decl_modifiers_block_ptr;
typedef struct a_decl_modifiers_block {
  a_decl_modifier_set
		flags;
			/* A bit-vector of flags representing additional
			   declarative information (e.g.,  via the __declspec
			   mechanism in Microsoft compatibility mode).  This
			   may eventually be copied into the IL. */
  a_bit_field	direct_linkage_specifier:1;
			/* TRUE if an only if a linkage specifier was added
			   directly to the declaration (e.g., extern "C" A x;
			   but not extern "C" { A x; }).  Not copied into
			   the IL. */
  a_bit_field	marked_as_gnu_extension:1;
			/* TRUE if the declaration was preceded by the GNU
			   keyword __extension__. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	is_deprecated:1;
			/* TRUE if the declaration was marked with
			   __declspec(deprecated). */
  a_bit_field	is_microsoft_intrinsic:1;
			/* TRUE if the declaration was marked with
			   __declspec(intrin_type). */
  a_const_char	*uuid_string;
			/* Pointer to a string representing the argument of
			   a "uuid" decl-modifier (in Microsoft mode). */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
} a_decl_modifiers_block;


/*
Structure that contains a token cache that represents a template or
part of a template, and the information needed to recreate the context
in which the tokens should be rescanned.
*/
typedef struct a_template_cache {
  a_reusable_token_cache
		tokens; /* A reusable token cache containing the tokens. */
  a_template_decl_info_ptr
		decl_info;
			/* Pointer to the template declaration information
			   associated with the template declaration that
			   contained the tokens in the token cache above. */
} a_template_cache;


typedef struct a_template_param {
  /* Information describing a template formal parameter.  Pointed to by the
     a_template_decl_info, which in turn is pointed to by the a_template_cache
     entry.  A separate template parameter entry is needed for each
     declaration of a template because the template parameters can be named
     differently in each declaration. */
  a_template_param_ptr
                next;
                        /* Pointer to the next template parameter. */
  a_symbol_ptr	param_symbol;
			/* Symbol entry for a formal parameter of the
			   template.  During instantiations, this resolves
			   to the corresponding template argument. */
  a_template_cache
		cache;
			/* Contains the cached tokens that comprise the
			   template parameter declaration.  Used to
			   create the parameter types for instances of
			   the class template when the parameter type
			   depends on other template parameters. */
  a_bit_field	has_default_arg:1;
			/* TRUE if a default argument has been declared for
			   this parameter. */
  a_bit_field	def_arg_involves_template_param:1;
			/* TRUE if the default argument involves a template
			   parameter.  For nontype parameters, this means
			   that the constant involves a template parameter.
			   It will also be set TRUE if the type of the
			   constant involves a template parameter. */
  a_bit_field	def_arg_has_not_been_scanned:1;
			/* TRUE if the tokens that make up the default argument
			   have not yet been scanned. */
  a_bit_field	def_arg_from_other_decl:1;
			/* TRUE if the default argument was specified in
			   the parameter list of a different declaration of
			   the template. */
  a_bit_field	is_pack:1;
			/* TRUE if this is a template parameter pack.
			   For a pack that is a pack expansion, this is
			   TRUE for the first element of the expansion. */
  a_bit_field	is_pack_expansion:1;
			/* TRUE if this is a pack expansion of an enclosing
			   template parameter pack.  This differs from
			   is_pack_element, which is only TRUE for
			   instantiations of packs, while this is also
			   TRUE for declarations. */
  a_bit_field	is_pack_element:1;
			/* TRUE if this is a template parameter expanded
			   from an enclosing template parameter pack. */
  a_bit_field	is_empty_pack:1;
			/* TRUE if this is placeholder for an enclosing
			   template parameter pack with an empty expansion. */
  a_bit_field	do_prototype_instantiation:1;
			/* TRUE if a prototype instantiation should be done
			   for this parameter.  FALSE if it should not be
			   done or has already been done. */
  a_bit_field	is_dependent:1;
			/* TRUE if the declaration of the template parameter
			   or its default argument is dependent.  For
			   a template template parameter, this is TRUE if
			   any of its template parameters are dependent. */
  a_bit_field	used_in_alias:1;
			/* TRUE if the parameter is used in the resulting
			   alias type. */
  a_bit_field	uses_auto:1;
			/* TRUE if this is a nontype parameter whose type
			   involves "auto" or "decltype(auto)". */
  uint32_t	param_num;
			/* The ordinal position of the parameter (1, 2, ...).
			   In the instantiation of a variadic template, this
			   is the position of the corresponding parameter from
			   the original template.  In other words, there can
			   be missing or repeated values in the parameter list
			   of the instantiation of a variadic template. */
  union {
    /* When param_symbol->kind = sk_type. */
    a_type_ptr
		type;
                        /* Type entry for a formal parameter.  A unique type
                           entry is created for each template type
                           parameter. */
    /* When param_symbol->kind = sk_constant. */
    struct {
      a_constant_ptr
		ptr;
			/* Constant entry for a formal parameter.  A unique
			   constant entry is created for each template constant
			   parameter. */
      a_bit_field
		type_involves_template_param:1;
			/* TRUE if the type entry associated with the
			   parameter constant involves (anywhere in its
			   type tree) a tk_template_param type entry. */
    } constant;
    /* When param_symbol->kind = sk_class_template. */
    a_template_symbol_supplement_ptr
		templ;
			/* Template entry for a formal parameter.  A unique
			   template entry is created for each template
			   template parameter. */
  } variant;
  union {
    /* Note that when def_arg_involves_template_param is FALSE, these
       fields contain the actual default argument to be used.  When
       def_arg_involves_template_param is TRUE, they contain the
       "prototype" default argument (i.e., the template-dependent one
       that was scanned when the template declaration is scanned).
       In some modes, the default is not scanned when its type is
       dependent.  In such cases, the prototype value is NULL. */
    /* When param_symbol->kind = sk_constant. */
    a_constant_ptr
		constant;
			/* Constant containing the default value
			   to be used as the actual argument of an
		           instantiation when the actual argument
			   corresponding to this parameter is omitted. */
    /* When param_symbol->kind = sk_type. */
    a_type_ptr
		type;
			/* Type containing the default value to be used
			   as the actual argument of an instantiation when
			   the actual argument corresponding to this parameter
			   is omitted. */
    /* When param_symbol->kind = sk_class_template. */
    a_template_ptr
		templ;
			/* Template that is the default value to be used
			   as the actual argument of an instantiation when
			   the actual argument corresponding to this parameter
			   is omitted. */
  } default_arg;
  a_template_parameter_ptr
		il_template_parameter;
			/* Points to the IL template parameter entry if one
			   has been created.  NULL otherwise. */
  /* When def_arg_involves_template_param is TRUE. */
  a_template_cache
		default_arg_cache;
			/* The template cache that contains the
			   tokens of the default argument expression.
			   Only used when def_arg_involves_template_param is
			   TRUE. */
} a_template_param;


typedef struct a_def_undef_string *a_def_undef_string_ptr;
typedef struct a_def_undef_string {
  /* Used to save -D (define symbol) and -U (undefined symbol) command-line
     arguments. */
  a_def_undef_string_ptr
		next;
			/* Next entry on this list, or NULL if this is the
			   last entry. */
  a_const_char	*text;
			/* The text of the argument (i.e., "x=1" for the
			   option "-Dx=1", "x" for "-Ux"). */
  a_boolean
		is_undef;
			/* TRUE if this is an entry for a -U argument. */
} a_def_undef_string;


/*
Entry used to record information about a file containing exported
template definitions.
*/
typedef struct an_exported_template_file *an_exported_template_file_ptr;
typedef struct an_exported_template_file {
  a_const_char	*directory_name;
			/* Directory containing the file. */
  a_const_char	*source_file_name;
			/* Name of the source file. */
  a_translation_unit_ptr
		translation_unit;
			/* If the translation unit for this file has been
			   loaded, this point to the translation unit entry.
			   NULL if the translation unit has not been loaded. */
  a_const_char	*module_id;
			/* The module ID read from the exported template
			   file.  When instantiating exported templates, the
			   original module ID must be used when referring to
			   things like static entities that were promoted to
			   be external so that they could be referenced from
			   instantiations. */
  a_directory_name_entry_ptr
		incl_search_path;
			/* The include search path to be used when loading this
			   file. */
  a_directory_name_entry_ptr
		end_incl_search_path;
			/* The end of the include search path. */
  a_directory_name_entry_ptr
		sys_incl_search_path;
			/* The system include search path to be used when
			   loading this file. */
  a_def_undef_string_ptr
		define_list;
			/* A list of command-line macro definitions to be used
			   when loading this file. */
  a_def_undef_string_ptr
		undefine_list;
			/* A list of command-line macro undefines to be used
			   when loading this file. */
} an_exported_template_file;


typedef int an_instance_required_count;
			/* Type used to record the number of translation units
			   for which an instance of a given template is
			   required. */

/*
Entry that describes the use of a template instance across the entire
set of translation units that are being processed.  A template instance
for an external entity may be referenced by any number of translation
units, but only one instantiation of the instance is required.  This
entry is used to track the instance-related information that is shared
among translation units.
*/
typedef struct a_master_instance *a_master_instance_ptr;
typedef struct a_master_instance {
  a_master_instance_ptr
		next;	/* Pointer to the next entry in the list of master
			   instance entries, or NULL for the last entry. */
  a_template_instance_ptr
		instance;
			/* Pointer to one of the instance entries associated
			   with one of the translation units.  This points to
			   an arbitrary instance, and not necessarily the
			   canonical one. */
  char		*name;
			/* The mangled name of the entity. */
  an_instance_required_count
		instance_required_count;
			/* The number of translation units for which the
			   instantiation_required flag is set for this
			   instance. */
  a_bit_field	already_instantiated:1;
			/* TRUE if instantiation has already been performed
			   (for instance, for inline functions, which are
			   instantiated at the point of first reference). */
  a_bit_field	automatically_instantiated:1;
			/* TRUE if the instance was listed in the instantiation
			   request file as an instantiation assigned to
			   this compilation.  This field is only used when
			   automatic template instantiation is configured. */
  a_bit_field	add_to_request_file:1;
			/* TRUE if the instance was "adopted" by this
			   translation unit because it was known not to be
			   defined elsewhere.  A list of these entities is
			   returned to the prelinker to be appended to
			   the instantiation request file. */
  a_bit_field	is_static_or_inline:1;
			/* TRUE if the routine has previously been determined
			   to be static or inline (or should be treated as
			   such for instantiation purposes).  This is the saved
			   result of is_static_or_inline_template_entity.
			   That routine should always be used instead of
			   checking this flag directly (because it may not
			   have been set yet). */
} a_master_instance;


typedef struct a_template_instance {
  /* Information describing an instance of a function template or an
     instance of a member function or static data member of a template class.
     The kind of instance may be derived from instance_sym->kind. */
  a_template_instance_ptr
                next;
                        /* Pointer to the next instance of a given template. */
  a_template_instance_ptr
                next_in_instantiation_list;
                        /* Pointer to the next instance in a list of
			   entries for which full instantiation is required. */
  a_master_instance_ptr
		master_instance;
			/* Pointer to the master instance that contains
			   information about this instance that is shared
			   among translation units. */
  a_symbol_ptr  instance_sym;
                        /* Pointer to the sk_routine, sk_member_function, or
			   sk_static_data_member symbol entry that describes
			   this template instance.  Note that the instance
			   may not be a "real" instance when it is a member
			   of a prototype instantiation of a class template. */
  a_symbol_ptr  template_sym;
			/* For nonmember functions, a pointer to the
			   sk_function_template of which it is an instance.
			   For member functions, a pointer to the
			   sk_member_function entry of the class template's
			   prototype instantiation.  For static data members,
			   a pointer to the sk_static_data_member symbol of
			   the prototype instantiation.  (Note that
                           template_sym == instance_sym when instance_sym is
			   a member of prototype instantiation; when this is
			   the case template_info is non-NULL.) */
  a_symbol_ptr	template_used_for_instantiation;
			/* For a variable template instance this points to
			   the template symbol used for the instantiation.
			   This is the same as template_sym unless a
			   partial specialization was used.  A variable
			   template can be instantiated more than once if
			   it is initially declared extern, instantiated,
			   and then later has a definition supplied.  This
			   is used to make sure the same partial specialization
			   is used at both points. */
  a_namespace_ptr
		referencing_namespace;
			/* Pointer to the namespace in which the use that
			   first required the instantiation of the template
			   was encountered.  NULL if the first reference
			   was in the global namespace.  This field is
			   set when the instantiation_required flag is
			   set. */
  a_template_symbol_supplement_ptr
		template_info;
			/* Pointer to associated template information when
			   instance_sym points to a member of a prototype
			   instantiation of a class template (in which case
			   instance_sym == template_sym and the instance is
			   not a "real" instance but a kind of template for a
			   member function or a static data member).  Otherwise
			   (i.e., usually) NULL. */
  a_symbol_ptr	prototype_scope_symbols;
			/* For member and nonmember functions, a list of
			   symbols in the prototype scope, linked on the
			   next_in_scope field.  NULL if none. */
  an_exported_template_file_ptr
		exported_template_file;
			/* If this is an instance of an exported template,
			   this points to an entry that describes the file in
			   which the template definition was found.  This is
			   set when determining whether it is possible to
			   generate the instance. */
  a_bit_field	instantiation_required:1;
			/* TRUE if a routine body or static data member
			   definition needs to be generated for this instance.
			   This flag is FALSE if an explicit definition has
			   been provided by the user (i.e., if specific_def
			   is set). */
  a_bit_field	suppress_instantiation:1;
			/* TRUE if the instantiation of this entity should be
			   suppressed because of previous errors that occurred
			   during the partial instantiation of the entity,
			   because the instance is ineligible (e.g., because
			   C++20 constraints were not satisfied), or because
			   the instantiation appeared as an artifact of an
			   instantiation (of the same underlying template) that
			   had one or more errors during instantiation. */
  a_bit_field	is_guiding_decl:1;
			/* For instances of nonmember function templates,
			   TRUE if this instance is a guiding declaration
			   (i.e., if it has been explicitly declared as
			   though it were a normal function -- in which case
			   instance_sym has been added to the overload list
			   for this name).  Undefined for member functions
			   and static data members of template classes. */
  a_bit_field	explicit_instantiation:1;
			/* TRUE if an instantiation has been explicitly
			   requested using an explicit instantiation directive
			   or an instantiation pragma. */
  a_bit_field	class_explicitly_instantiated:1;
			/* TRUE if the instantiation request specified the
			   class (meaning that all its members should be
			   instantiated).  When the class is specified for
			   instantiation, no error is issued if template
			   definitions are not available for some of the
			   members. */
  a_bit_field	explicit_do_not_instantiate:1;
			/* TRUE if instantiation has been explicitly 
			   suppressed by an "extern template" directive or
			   a do_not_instantiate pragma. */
  a_bit_field	explicit_can_instantiate:1;
			/* TRUE if instantiation has been explicitly declared
                           as being possible by a can_instantiate pragma. */
  a_bit_field	can_be_instantiated:1;
			/* TRUE if this entity can be instantiated.  This
			   means that a template definition is available
			   if one is needed.  Note that if this flag is FALSE
			   it does not necessarily mean that the entity cannot
			   be instantiated, because a definition may have
			   been supplied since the last time the check was
			   done.  The can_be_instantiated routine should
		           be used instead of this field. */
  a_bit_field	on_instantiations_list:1;
			/* TRUE if this entry is already on the instantiations
			   required list. */
  a_bit_field	error_issued:1;
			/* TRUE if an error has already been issued for this
			   instance.  This is used to suppress duplicate
			   messages. */
  a_bit_field	suppress_default_arg_instantiations:1;
			/* TRUE if a default argument recursion was detected
			   and subsequent default argument instantiations
			   should be suppressed. */
  a_bit_field	instantiation_requested_for_constant_value:1;
			/* TRUE for a variable template that is being
			   instantiated to establish its constant value. */
  a_source_position
		explicit_instantiation_pos;
			/* The position of the explicit instantiation directive
			   or pragma when explicit_instantiation is TRUE. */
  a_source_position
		pos_of_first_reference;
			/* The position of the first reference to the entity.
			   This is used in diagnostic output to indicate the
			   position of the first reference that caused an
			   instantiation of the entity. */
  a_param_id_ptr
		param_id_list;
			/* List of entries describing parameter symbols.
			   NULL if there were none. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_type_ptr	declared_type;
			/* When instance_sym points to an sk_routine or
			   sk_member_function, pointer to the routine's type
			   as it actually appears in the source program (i.e.,
			   before parameter type adjustments). */
  a_type_ptr	declared_type_for_default_arg_fixup;
			/* Same as declared_type, but only when the declared
			   type is a candidate for default argument fixup.
			   (It will not be, for instance, when no source
			   sequence entry is generated to record the declared
			   type.)  NULL when default arg fixup is not
			   appropriate. */
#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  a_source_sequence_entry_ptr
		partial_instantiation;
			/* An iek_src_seq_secondary_decl source sequence
			   entry representing the partial instantiation of
			   a function template that is dependent on a class
			   that is currently being defined.  As long as this
			   pointer is non-NULL, the source sequence list has
			   not yet been updated. */
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
} a_template_instance;


/*
Structure used to keep track of the segments of a template token cache
that are used to record the definition of member classes and member
functions of a class template definition.  This information is used
to extract the member bodies from the enclosing token cache.
*/
typedef struct a_template_cache_segment {
  ~a_template_cache_segment();
  a_symbol_ptr	symbol;
			/* Pointer to the symbol entry for the member
			   associated with this entry. */
  a_template_symbol_supplement_ptr
		template_info;
			/* Pointer to the template supplement for
			   template_sym. */
  a_token_sequence_number
		first_token_number;
			/* Token sequence number of the first token in
			   the definition of the template. */
  a_token_sequence_number
		last_token_number;
			/* Token sequence number of the last token in
			   the definition of the template. */
  a_token_cache_ptr
		source_cache;
			/* The cache associated with the iterators
			   before_first_token and last_token. */
  a_byte_boolean
		is_friend;
			/* TRUE if this entry represents a friend function. */
  a_byte_boolean
		is_default_arg;
			/* TRUE if this entry represents a default argument
			   expression. */
  a_byte_boolean
		expression_missing;
			/* TRUE if default argument, exception specification,
			   or initializer expression is missing (e.g.,
			   "void f(int=)"). */
  a_byte_boolean
		is_exception_specification_arg;
			/* TRUE if this entry represents the argument (or
			   arguments) of an exception specification. */
  a_byte_boolean
		exception_spec_on_templ_friend;
			/* TRUE when is_exception_specification_arg is TRUE
			   and the declaration is a template friend. */
} a_template_cache_segment;


/*
Structure that contains information associated with a friend template
declared in a class template.
*/
typedef struct a_templ_friend_info *a_templ_friend_info_ptr;
typedef struct a_templ_friend_info {
  a_templ_friend_info_ptr
		next;
			/* Pointer to the next entry on the list, or NULL
			   for the last entry. */
  a_symbol_ptr	symbol;
			/* Pointer to the symbol entry for the friend
			   declaration. */
  a_token_sequence_number
		token_number;
			/* Then token sequence number of end of the friend
			   function declaration.  This is used to associate
			   a declaration in a real instantiation with the
			   corresponding declaration in the prototype
			   instantiation. */
} a_templ_friend_info;


/*
Entry used to record information about partial specializations of members
of class templates that are declared outside of the parent class template.
When an instance of the enclosing class template is instantiated, each
of the entries on this list is processed to create a declaration of the
partial specialization for that instance of the enclosing class.
*/
typedef struct an_out_of_class_partial_spec *an_out_of_class_partial_spec_ptr;
typedef struct an_out_of_class_partial_spec {
  an_out_of_class_partial_spec_ptr
		next;	/* Pointer to the next entry on the list or NULL for
			   the last entry. */
  a_symbol_ptr	symbol;
			/* The class template symbol of the partial
			   specialization. */
  a_template_cache
		cache;	/* The cache containing the declaration of the
			   partial specialization. */
  struct a_tmpl_decl_state
		*tmpl_decl_state;
			/* Pointer to a copy of the template declaration
			   state entry used when the partial specialization
			   was first scanned. */
} an_out_of_class_partial_spec;


typedef struct a_template_symbol_supplement {
  /* Additional information about a C++ class or function template
     supplementing the information residing in the class's symbol entry. */
  a_template_cache_ptr
		cache;	/* The tokens comprising the template are cached
			   in order to be rescanned later during
			   instantiation.  Typically begins with the left
			   brace that begins the class or function body	and
			   extends to the right	brace; for constructors	it
			   may begin at a colon.  For templates for static
			   data members it embraces the initializer
			   expression, if any. */
  a_pending_instantiation_count
		pending_instantiations;
			/* The number of instantiations of this template
			   that are in the process of being instantiated.
			   Used to detect runaway recursive instantiations. */
  a_symbol_ptr	invalid_active_instantiation;
			/* When the template should have new (recursive)
			   instantiations suppressed due to an invalid
			   instantiation of the same template being
			   instantiated in the current scope stack, this is the
			   symbol for the template instantiation that triggered
			   the suppression; otherwise, NULL. */
  a_pending_pragma_list
		*pragmas_bound_to_template;
			/* A list of pbk_next_construct pragmas to be bound
			   to each instance generated from this template. */
  a_token_sequence_number
		token_sequence_number;
			/* This is used for class members to match the
			   declarations of the prototype instantiation
			   (to which the template symbol supplement is
			   attached) to declarations found inside real
			   instantiations.  This field contains the token
			   sequence number of a certain token within the
			   declaration. */
  a_class_list_entry_ptr
                befriending_classes;
                        /* A linked list of entries identifying classes
			   that have declared the current class a friend
			   (i.e., classes that have befriended the this
			   template).  If the template friend declaration
			   appears as part of a class template definition,
			   a new entry will be added to this list for
			   each class instantiated from the class
			   template. */
  a_template_cache_segment_ptr
		cache_segment;
			/* Pointer to a structure that describes the
			   range of tokens from the template cache
			   of the enclosing template that contain the
			   definition of this template.  This field  is
			   used to extract the definitions of member
			   functions and nested classes from the bodies
			   of class template definitions. */
  a_symbol_ptr	prototype_template;
			/* If this is a member template of a class template
			   instance, this points to the template symbol for
			   the original member template declaration in the
			   prototype instantiation.  This pointer will be
			   set even if the member template is specialized in
			   one of the instances of the enclosing class
			   template.  In other words, the
			   is_specific_definition flag must be used to
			   determine whether the cache information from
			   the prototype template or the cache information
			   from this template should be used. */
  a_symbol_list_entry_ptr
		subordinate_templates;
			/* If this is a member template of a prototype
			   instantiation, this points to a list of template
			   symbols for the templates generated from this
			   template. */
  a_template_ptr
		il_template_entry;
			/* When  the symbol kind is sk_class_template,
			   sk_function_template, or sk_variable_template, the
			   IL entry created to represent this template.  Points
			   to the entry associated with the first
			   declaration. */
  a_symbol_list_entry_ptr
		all_instantiations;
			/* When secondary translation units are processed,
			   this points to a list of all the instantiations of
			   this template (across all translation units).
                           Only set for the canonical entry. */
  char		*name;
			/* The mangled name of the entity.  Used for exported
			   templates. */
  an_attribute_ptr
		attributes;
			/* Attributes specified on this template that need to
			   be applied to every instantiation. */
  a_hash_table_ptr
		instantiation_hash_table;
			/* A hash table used to locate previously-created
			   instantiations of this template.  NULL if no
			   instances have been created or if a hash table is
			   not used for this kind of template. */
  a_symbol_ptr
		partial_specializations;
			/* A list of symbols for partial specializations of
			   a class template or variable template.
			   This is present only for templates that are
			   "primary" templates (i.e., those that are not
			   already partial specializations).  NULL for
			   templates with no partial specializations, for
			   templates that are already partial
			   specializations, and for templates that cannot
			   be partially specialized. */
  a_symbol_ptr
		primary_template_sym;
			/* For partial specialization, points back to the
			   primary template of which this is a partial
			   specialization. */
  a_bit_field
		is_specific_definition:1;
			/* TRUE if the template is a specific definition of
			   a member template. */
  a_bit_field	is_nonreal_member:1;
			/* TRUE if the template was created as a member of
			   a proxy or nonreal class and does not represent
			   an actual template declaration. */
  a_bit_field	is_error:1;
			/* TRUE if this is an error class template created
			   for error recovery purposes. */
  a_bit_field	is_variadic:1;
			/* TRUE if this is a variadic template, or should be
			   treated as variadic in GNU mode because it might
			   contain GNU variadic operators such as __bases. */
  a_bit_field	has_variadic_template_params:1;
			/* TRUE if this is an actual variadic template and
			   not simply treated as variadic in GNU mode (see
			   is_variadic above. */
  a_bit_field	has_template_param_constraint:1;
			/* TRUE if one of the template parameters has a type
			   constraint. */
  a_bit_field	has_partial_spec_with_requires_clause:1;
			/* TRUE if one of the partial specializations has a
			   requires clause. */
  a_bit_field
		is_generic:1;
			/* TRUE for C++/CLI generics. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	is_delegate:1;
			/* TRUE for C++/CLI generic delegates. */
  a_bit_field	from_metadata:1;
			/* TRUE if this is a C++/CLI generic that was
			   imported from metadata. */
  a_bit_field	generic_constraints_pending:1;
			/* TRUE if this is a C++/CLI generic class imported
			   from metadata without constraints but with an
			   indication that constraints will be specified on a
			   forthcoming redeclaration.  (Reset to FALSE when
			   the redeclaration with the constraints is
			   processed.) */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  union {
    /* For concept templates (sk_concept_template): No variant field. */
    /* For class templates, nested classes of class templates, member
       enumerations of those, and for alias templates. */
    struct {
      a_symbol_list_entry_ptr
                instantiations;
                        /* Pointer to a list of symbols describing types
                           that have been instantiated from this class
			   template or alias template.  Nonreal types are
			   included in this list, but prototype instantiations
			   are not.  For class templates, the symbols on the
			   list are classes.  For alias templates, the
			   symbols are types.  This is not used for
			   enumerations. */
      a_symbol_ptr
		prototype_instantiation;
			/* Points to the symbol representing the prototype
			   instantiation.  For class templates, this is a
			   class.  For alias templates, this is a type.  For
			   an enumeration, this is an enum type. */
      an_out_of_class_partial_spec_ptr
		out_of_class_partial_specs;
			/* When a partial specialization of a class template
			   that is a member of another class template is
			   declared outside of the enclosing class, the
			   partial specialization must be evaluated for
			   each instantiation of the enclosing class.  This
			   happens automatically for partial specializations
			   that appear inside the enclosing class (because
			   those tokens are rescanned during the instantiation
			   of the enclosing class).  For partial
			   specializations that appear outside of the class
			   this is done by rescanning the declarations
			   associated with the entries on this list. */
      a_templ_friend_info_ptr
		friend_info;
			/* Information about default arguments of friend
			   templates declared in this class template. */
      a_symbol_ptr
		argument_template;
			/* For a class template associated with a template
			   template parameter, points to the symbol of the
			   actual template template argument for the
			   current instantiation. */
      a_template_ptr
		substituted_param_template;
			/* For a class template associated with a template
			   template parameter, if the template template
			   parameter for which this is an argument has a
			   template parameter with a dependent type, this
			   points to the rescanned template parameter.
			   This comes up in cases like
			   "template <class T, template <T t> struct X> ...".
			   The rescanned version of the parameter list must
			   be used when scanning template argument lists of
			   the template template parameter. */
      a_symbol_ptr
		deduction_guides;
			/* The C++17 deduction guides for a given primary class
			   template.  Multiple guides are grouped in an
			   overload set. */
      a_template_cache_ptr
		initial_decl_cache;
			/* For class templates, this represents the initial
			   declaration of the class template.  If the
			   initial declaration is also the definition,
			   this will point to the same information as
			   "cache" above. */
#if MICROSOFT_EXTENSIONS_ALLOWED
      a_symbol_list_entry_ptr
		generic_arity_list;
			/* There can be more than one generic of a given name
			   in a given scope provided that they differ in the
			   number of generic parameters (also known as
			   "arity").  If there is more than one arity for
			   a generic, this points to a list of all of the
			   different arity versions of the generic.  It is
			   NULL if there is only one arity.  The list
			   is attached to the initial generic entered into
			   the symbol table and that generic is included on
			   the list.  The other entries on the list are not
			   entered into the symbol table.  The list is
			   maintained in the order in which the generics were
			   declared. */
      a_symbol_ptr
		non_generic_class;
			/* In addition to generics of varying arity described
			   above, a non-generic class can exist in the same
			   scope as one or more generic of the same name.
			   In such cases, the generic is found by name
			   lookup and this pointer can be used if it is
			   determined that the reference is to the non-generic
			   version.  It is NULL if there is no non-generic
			   version. */
      uint32_t	arity;
			/* For C++/CLI generic classes, the number of generic
			   parameters for this generic. */
      uint32_t	min_arity;
			/* For C++/CLI generics, this field is set in the
			   generic found by lookup and is the minimum number
			   of generic parameters of the various versions of
			   the generic.  It is also set if there is only
			   one arity. */
      uint32_t	max_arity;
			/* For C++/CLI generics, this field is set in the
			   generic found by lookup and is the maximum number
			   of generic parameters of the various versions of
			   the generic.  It is also set if there is only
			   one arity. */
      a_pending_instantiation_count
		pending_nonreal_instantiations;
			/* The number of Microsoft nonreal instantiations
			   of this template that are in the process of being
			   instantiated.  Used to prevent recursive nonreal
			   instantiations that have a way of being terminated
			   for real instantiations, but do not for nonreal
			   instantiations. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      a_type_kind
		type_kind;
			/* The kind (tk_class, tk_struct, or tk_union) that
			   the instantiated types will have.  Not used for
			   alias templates. */
      a_bit_field
		is_alias_template:1;
			/* TRUE if this is an alias template. */
      a_bit_field
		prototype_instantiation_complete:1;
			/* TRUE when the prototype instantiation of the
			   class template or alias template has been completed.
			   Used for class templates to prevent a real
			   instantiation from occurring while the prototype
			   instantiation is in progress.  Used for alias
			   templates to detect uses of the alias name within
			   its definition. */
      ENUM_TYPE_FOR_BIT_FIELD(a_name_linkage_kind)
		name_linkage:NUM_BITS_FOR_NAME_LINKAGE;
			/* The name linkage associated with this class
			   template -- typically C++ linkage, but internal
			   linkage if the template is declared inside an
			   unnamed namespace. */
#if MICROSOFT_EXTENSIONS_ALLOWED
      a_bit_field
		is_interface:1;
			/* TRUE for Microsoft __interface class templates. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      a_bit_field
		not_standalone_nested_class:1;
			/* TRUE for nested classes of class templates in
			   which the definition of the nested class cannot
			   be extracted from the token cache for the
			   enclosing template because it is part of the
			   declaration of some other entity in the enclosing
			   class.  For example, "struct { ... } a;". */
      ENUM_TYPE_FOR_BIT_FIELD(an_access_specifier)
		access:2;
			/* If the template is a member of a class, this
                           specifies the access for the member. */
      a_bit_field
		template_template_param:1;
			/* TRUE if this is a class template symbol associated
			   with a template template parameter. */
      a_bit_field
		def_templ_templ_arg_check_delayed:1;
			/* TRUE when template_template_param is TRUE and a
			   default template argument was scanned, but its
			   template parameter list was not yet checked against
			   the parameter list of the template template
			   parameter.  (Used to emulate g++ behavior.) */
      a_bit_field
		involves_template_param:1;
			/* TRUE for template template parameters for which
			   one or more template parameters depends on another
			   template parameter. */
      a_bit_field
		any_full_instantiations:1;
			/* TRUE if any full instantiations have been done of
			   this class template or any of its partial
			   specializations. */
      a_bit_field
		alias_uses_own_type:1;
			/* TRUE if an alias template uses its own type in the
			   type-id referred to by the alias.  This is used to
			   suppress instantiations of the alias. */
      a_bit_field
		cannot_be_specialized:1;
			/* TRUE if this template cannot be explicitly 
			   specialized. */
#if MICROSOFT_EXTENSIONS_ALLOWED
      a_bit_field
		any_ms_instantiated_nonreal_classes:1;
			/* TRUE if this template has any instantiations that
			   are Microsoft mode instantiated nonreal classes. */
      a_bit_field
		has_ms_undeclared_base_class:1;
			/* TRUE if this template has a base class that has not
			   been declared yet.  Such a base class is only
			   accepted in permissive Microsoft mode. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      a_bit_field
		invented_template:1;
			/* TRUE if this is a template "invented" for the
			   checking of template template argument
			   compatibility. */
      a_bit_field
		explicit_deduction_guides_added:1;
			/* TRUE if at least one explicit deduction guide is
			   recorded in the deduction_guides symbol. */
      a_bit_field
		implicit_deduction_guides_added:1;
			/* TRUE if deduction_guides includes generated
			   deduction guides. */
      a_bit_field
		interim_implicit_deduction_guides:1;
			/* TRUE if implicit_deduction_guides_added is TRUE but
			   the generated guides were generated when the class
			   template was not defined. */
      a_bit_field
		has_alias_params_not_in_type:1;
			/* TRUE for an alias template if it has template
			   parameters that are not used in the aliased type. */
#if CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
      a_source_sequence_entry_ptr
		source_sequence_list;
			/* List of source-sequence entries collected during
			   prototype instantiation of the class template;
			   May be NULL. */
#endif /* CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
    } class_template;
    /* When symbol kind = sk_function_template or sk_member_function: */
    struct {
      a_template_instance_ptr
                instantiations;
                        /* Pointer to a list of entries describing template
                           functions that have been instantiated from this
                           function template. */
      a_routine_ptr
                routine;
                        /* Points to a routine entry for the function
                           template.  This is needed for function
                           matching. */
#if GNU_FUNCTION_MULTIVERSIONING
                        /* Note that when routine->is_representative
                           is TRUE, this symbol represents the entire set of
                           multiversioned functions; if a specific-
                           target versioned routine is desired (or all of
                           them), the representative.targeted_versions list
                           must be consulted. */
#endif /* GNU_FUNCTION_MULTIVERSIONING */
      a_func_info_block
		func_info;
			/* Information about the prototype parameters
			   in a function template declaration (the function
			   parameters not the template parameters). */
      a_def_arg_expr_fixup_ptr
		def_arg_expr_list;
			/* List of entries describing default argument
			   expressions associated with parameters for
			   this template declaration.  For a subordinate
			   template this points to the default argument
			   list of the prototype template. */
      a_template_cache_ptr
		decl_cache;
			/* A cache of the tokens that comprise the function
			   declaration.  These are rescanned later to create
			   routine types for instances of the function
			   template.  The cache begins with the first token
			   of the function declaration (the token after the
			   closing ">" of the template parameter list) and
			   ends with the last token of the function
			   declarator. */
      a_template_cache_ptr
		exception_spec_arg_cache;
			/* Cache for the exception specification argument, to
			   be instantiated when needed. */
      a_hash_table_ptr
		substituted_types_table;
			/* A hash table for template argument lists and the
			   type that results from substituting the template
			   parameters in the template routine types with
			   specified template arguments.  This is used by
			   substitute_template_arguments to determine whether
			   a type has already been produced for a given
			   template argument list. */
      an_unused_instantiation_count
		unused_instantiations;
			/* When a function is added to the instantiations
			   required list in tim_all mode but is not actually
			   required, it is not instantiated until instantiation
			   wrapup is done, even if it is an inline function.
			   This is done because these functions may be put
			   on the list before they can actually be
			   instantiated.  Consequently, the runaway recursive
			   instantiation check will not detect a loop in which
			   new "unused" entries get added while instantiating
			   earlier "unused" entries.  To prevent such loops
			   we set an arbitrary limit to the number of unused
			   instantiations that can be generated for a given
			   function.  This field records the number of unused
			   instantiations that have been performed so far. */
      a_pending_instantiation_count
		pending_partial_instantiations;
			/* The number of partial instantiations of this
                           template that are in the process of being
			   instantiated.  Used to detect runaway recursive
			   instantiations. */
      a_pending_instantiation_count
		pending_deductions;
			/* The number of deductions/substitutions of this
                           template that are in the process of being
			   performed.  Used to detect runaway recursion
			   during deduction. */
      a_symbol_ptr
		prototype_friend_symbol;
			/* If this template was declared as a friend of a
			   class template this field is used for friend
			   declarations of real instantiations of the
			   class template and points to the corresponding
			   friend symbol from the prototype instantiation
			   of the class template. */
      a_param_type_ptr
		invented_partial_ordering_param;
			/* If the template is a non-static member function
			   template and an invented parameter type was
			   created for purposes of comparison with a
			   static or non-member function during partial
			   ordering, this points to the invented parameter.
			   NULL otherwise. */
      a_symbol_ptr
		constructor_symbol_for_guide;
			/* If this is an implicit deduction guide, this
			   points to the symbol for the original constructor.
			   For a guide generated for a hypothetical
			   constructor, this will be NULL. */
      a_bit_field
		template_param_not_in_function_type:1;
			/* TRUE if the function template has template
			   parameters that are not used in the function
			   type. */
      a_bit_field
		has_prototype_instantiation:1;
			/* TRUE if a prototype instantiation has been
			   performed on this function.  This flag is set
			   at the beginning of the prototype instantiation
			   processing. */
      a_bit_field
		exception_spec_prototype_instantiation_done:1;
			/* TRUE if the prototype instantiation of the
			   exception specification (if any) has had its
			   prototype instantiation done.  Also TRUE if the
			   the routine has no exception specification, once
			   the check to see if a prototype instantiation is
			   needed or not has been done. */
      a_bit_field
		must_have_only_one_decl:1;
			/* TRUE if there can be only one declaration of
			   this function template.  This is the case for
			   a friend template with a default argument. */
      a_bit_field
		implicit_deduction_guide:1;
			/* TRUE if this is a function template generated to
			   serve as an implicit deduction guide. */
    } function;
    /* When symbol kind = sk_variable_template or sk_static_data_member: */
    struct {
      a_bit_field
		has_out_of_class_definition:1;
			/* TRUE if the variable template or static data
			   member has a definition that was not inside
			   the parent class (if any).  Note, this is always
			   TRUE for variable templates declared outside of
			   class scope. */
      a_template_instance_ptr
		definitions;
			/* Pointer to a list of entries specifying definitions
			   for static data members of instantiated template
			   classes.  NULL for variable templates. */
      a_symbol_list_entry_ptr
		instantiations;
			/* For variable templates, a pointer to a list of
			   symbols for the variables instantiated from this
			   template. */
      a_variable_ptr
		prototype_variable;
			/* Pointer to the variable for the prototype
			   instantiation of a variable template or template
			   static data member. */
      a_template_cache_ptr
		decl_cache;
			/* A cache of the tokens that comprise the out-of-class
			   definition of the static data member.  This cache
			   contains only the declaration portion of the
			   definition.  The initializer, if any, is represented
			   by the "cache" entry.  These tokens are rescanned
			   later to create the variable type instances of the
			   static data member.  The type could be different
			   than the one declared in the containing class if the
			   static data member is an array with no size
			   specified in the class.  The cache begins with the
			   first token of the declaration (the token after the
			   closing ">" of the template parameter list) and
			   ends with the last token of the declarator. */
      a_token_sequence_number
		declarator_name_tsn;
			/* The token sequence number of the identifier
			   in the declarator. */
    } variable;
  } variant;
} a_template_symbol_supplement;


typedef struct a_namespace_symbol_supplement
                                        *a_namespace_symbol_supplement_ptr;
typedef struct a_namespace_symbol_supplement {
  a_scope_pointers_block
		pointers_block;
			/* A block of pointers that are logically part of the
			   scope stack entry for the associated namespace
			   -- including a pointer to a linked list of all
			   symbols declared in the namespace and pointers to
			   the last entries in linked lists of IL entries
			   entered in the associated IL scope. */
  a_namespace_list_entry_ptr
		namespace_list_entry;
			/* A namespace list entry that points to the associated
			   namespace.  This is used so that the
			   operator_lookup_namespaces pointer in the
			   class symbol supplement can point to a common
			   entry for all of the leaf classes (i.e., most
			   base classes) in a given namespace. */
  a_symbol_ptr	symbol;
			/* Pointer back to the namespace symbol.  This lets
			   you get a namespace pointer when you just have
			   a pointer to the namespace supplement. */
  a_decl_sequence_number
		using_dir_decl_seq;
			/* The lowest declaration sequence number of any active
			   using-directives that name this namespace.  This is
			   used by g++ instantiation lookup emulation. */
  a_name_qualifier_ptr
		name_qualifiers;
			/* Points to a list of the various forms of name
			   qualifiers used to name this namespace.  This is
			   used to find a previously allocated entry so that it
			   can be reused. */
#if NEED_NAME_MANGLING
  a_discriminator
		last_unnamed_type_number;
			/* The last number ("discriminator") assigned to an
			   unnamed enum or class type in this namespace
			   (closure types have a separate counter).  This
			   value is saved when the namespace scope is popped,
			   and retrieved into the scope stack when a namespace
			   extension is encountered. */
  a_discriminator
		last_closure_type_number;
			/* Same as last_unnamed_type_number, but for closure
			   types. */
#endif /* NEED_NAME_MANGLING */
  a_bit_field	visited_by_qualified_lookup:1;
			/* Used by the qualified lookup routines to indicate
			   that this namespace has already been visited. */
  a_bit_field	within_unnamed_namespace:1;
			/* TRUE when the namespace is itself an unnamed
			   namespace or is enclosed by an unnamed namespace. */
} a_namespace_symbol_supplement;


/*
An entry corresponding to an IL entry of kind a_using_directive and containing
front-end-only information.  (Note: a using-directive is a declaration of the
form "using namespace N"; it should not be confused with "using N::x" or
"using ::x", which are referred to as "using-declarations".)
*/
typedef struct an_active_using_directive {
  an_active_using_directive_ptr
		next;
			/* Next in the linked list of active using-directives
			   associated with the current scope or namespace. */
  an_active_using_directive_ptr
		next_that_applies_at_depth;
			/* Next in the linked list of entries that apply at
			   a given scope depth. */
  a_using_decl_ptr
		entry;
			/* The IL entry to which this front-end only entry
			   corresponds; there is a one-to-one correspondence
			   between the two sorts of entries, though a pointer
			   is required in one direction only. */
  a_namespace_symbol_supplement_ptr
		namespace_supplement;
			/* The namespace symbol supplement associated with
			   the namespace referenced in the using directive.
			   If the using directive refers to a namespace
			   alias, this field points to the namespace
			   supplement associated with the underlying
			   namespace. */
  a_scope_depth
		scope_depth_at_which_using_directive_applies;
			/* Contains the scope depth of the scope at which
                           symbols from this namespace should be visible. */
  a_decl_sequence_number
		effective_decl_seq;
			/* The declaration sequence number of the point at
			   which this using-directive comes into effect.
			   This is usually the declaration sequence number
			   of the using-directive, but for a namespace made
			   visible as a result of the transitivity of
			   using-directives, this will be the declaration
			   sequence number of the outermost using-directive. */
} an_active_using_directive;


typedef struct an_extern_symbol_descr *an_extern_symbol_descr_ptr;
typedef struct an_extern_symbol_descr {
  /* Information on an sk_extern_variable or sk_extern_routine entry, i.e.,
     an external symbol.  Such an entry is created for each name with
     linkage (external or internal) as a place to hold the pointer to the
     unique IL entry for that variable or function. */
  a_type_ptr	type;
			/* The full type for the entry.  May differ from the
			   type in the variable or routine for the symbol
			   in that the type here is the full composite type
			   of all declarations seen so far, while the type
			   in the variable or routine is compatible with
			   the full composite type but may have only a subset
			   of the information.  For example, the type here
			   could be "int [5]" while the type in the variable
			   entry is "int []". */
  union {
    /* When symbol kind == sk_extern_variable: */
    a_variable_ptr
		variable;
    struct {
      /* When symbol kind == sk_extern_routine: */
      a_routine_ptr
		ptr;
      a_byte_boolean
                is_implicit_declaration;
                        /* TRUE if this external routine has only been
			   declared implicitly. */
    } routine;
  } variant;
} an_extern_symbol_descr;


typedef struct a_projection_descr *a_projection_descr_ptr;
typedef struct a_projection_descr {
  /* Description of the projection of a base class member symbol into
     a derived class.  Pointed to by an sk_projection symbol. */
  a_symbol_ptr  fundamental_symbol;
			/* The fundamental base class member to which this
			   projection symbol refers, i.e., the symbol for
			   the definition of the entity rather than any
			   inherited instance of it. */
  a_base_class_ptr
		fundamental_base_class;
			/* This field is a pointer to the base class entry for
			   the entity represented by fundamental_symbol.  It
			   will be a base class entry on the current class's
			   base classes list, and its derivation
			   specifies the path between the current class object
			   and the member specified by fundamental_symbol. */
  a_type_ptr	naming_type;
			/* For a using-declaration, the type of the qualifier.
			   For an implicit projection, if the type was named
			   using a qualified name, a typeref that identifies
			   the type used in the qualifier (that case is only
			   recorded when name references are recorded). */
} a_projection_descr;

#if MICROSOFT_EXTENSIONS_ALLOWED

typedef struct a_property_set_symbol_supplement
                                        *a_property_set_symbol_supplement_ptr;
typedef struct a_property_set_symbol_supplement {
  a_symbol_ptr
		properties;
			/* One or more symbols representing properties.  Each
			   entry may represent a nonstatic property (sk_field)
			   or a static property (sk_static_data_member).
			   Overloading of properties is possible when multiple
			   properties of the same name have different index
			   types. */
  a_symbol_ptr
		get_accessors;
			/* A symbol representing the "get" accessors of the
			   properties in this set.  If only one "get" accessor
			   is present among the properties, this points to a
			   sk_member_function symbol; otherwise to a
			   sk_overloaded_function symbol. */
  a_symbol_ptr
		set_accessors;
			/* Same as get_accessors, but for the "set"
			   accessors. */
} a_property_set_symbol_supplement;

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

typedef struct a_symbol {
  /* A symbol as used by the front end. */
  /* If you change this structure, be sure to also change the initialization
     of global variable cleared_symbol in symbol_tbl_one_time_init. */
  a_symbol_header_ptr
		header;
			/* Pointer back to the header which contains a list of
			   symbols of which this is one.  This helps
			   when this symbol needs to be removed from the symbol
			   table.  The header also gives the symbol identifier
			   string. */
  a_symbol_ptr  next;
    			/* The next symbol in this list with the same
			   identifier.  This field is NULL if this is the
			   last symbol with this identifier. */
  a_symbol_ptr	next_in_scope;
			/* When the symbol is in the symbol table, this
			   points to the next symbol in the same scope. */
  a_symbol_ptr	prev_in_scope;
			/* When the symbol is in the symbol table, this
			   points to the previous symbol in the same scope. */
  a_symbol_ptr	next_in_lookup_table;
			/* When the symbol is in a lookup table (a hash table),
			   this points to the next symbol in the same scope
			   with the same symbol header. */
  a_scope_number
		decl_scope;
			/* Scope number of the scope in which this symbol
			   was declared. */
  a_decl_sequence_number
		decl_seq;
			/* A number (> 0) that, within a translation unit,
			   uniquely identifies the declaration associated with
			   this symbol.  The numbers are assigned sequentially,
			   so that a symbol with a higher decl_seq value was
			   declared after one with a lower number. */
  a_source_position
		decl_position;
			/* Source position of the declaration of this
			   symbol. */
  a_token_sequence_number
		token_sequence_number;
			/* For template parameter symbols, the token
			   sequence number associated with the
			   declaration.  NO_TOKEN_SEQUENCE_NUMBER
			   otherwise. */
  a_parent_class_or_namespace
		parent;
			/* When is_class_member is TRUE, parent.class_type
			   points to the class of which the current symbol is
			   a member; it may be assumed to be non-NULL.  When
			   is_class_member is FALSE and the current entity
			   was declared to be a namespace member (C++ only),
			   parent.namespace_ptr points to the namespace;
			   otherwise it is NULL. */
  a_module_entity_ptr
		module_entity;
			/* If the entity associated with this symbol was
			   imported from a module, this points to the
			   corresponding module entity.  (Note: multiple source
			   correspondences can point to the same module entity.
			   The module entity is determined via the current
			   module entity on the module entity stack when this
			   source correspondence was created). */
  a_symbol_ptr	corresp_nonreal_or_nested_type;
			/* For types that are nested within prototype
			   instantiation types, this points to a nonreal
			   type that is used in place of the original type
			   in contexts where the name of the type is to
			   be treated as a dependent type.  For the symbol
			   associated with the nonreal type, this points back
			   to the original symbol.  NULL for other symbols.
			   The is_nonreal_nested_type flag can be used to
			   determine whether a given symbol is the original
			   one or the nonreal version. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_hide_by_sig_list_entry_ptr
		hide_by_sig_lookup_result;
			/* In C++/CLI mode this points to the list of lookup
			   symbols to be used.  This is NULL until the first
			   hide-by-sig lookup is done.  After the first lookup
			   is done, hide_by_sig_lookup_done will be TRUE and
			   this field will contain the saved result, which
			   can be NULL. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_symbol_kind kind;
			/* The kind of symbol. */
  a_bit_field	referenced:1;
			/* TRUE if the symbol is actually referenced, not just
			   declared. */
  a_bit_field	defined:1;
			/* TRUE if the symbol is actually defined, not just
			   declared. */
  a_bit_field	explicit_linkage_specifier:1;
			/* TRUE for variables and routines for which an
			   explicit external linkage was specified (e.g.,
			   ``extern "C"'' -- C++ only). */
  a_bit_field	reentered_from_prototype_scope:1;
			/* TRUE if symbol was originally declared in a
			   function prototype scope and was subsequently
			   reentered in the function scope. */
  a_bit_field	is_class_member:1;
			/* TRUE if symbol represents a C++ class member; also
			   TRUE for fields in C.  (Note: it is not set for
			   anonymous union members whose names are promoted to
			   a non-class scope.)  */
  a_bit_field	is_error:1;
			/* TRUE if the symbol represents an identifier for
			   which an error has been diagnosed and which should
			   not be entered into the symbol table. */
  a_bit_field	is_template_param:1;
			/* TRUE if the symbol represent a template
			   parameter. */
  a_bit_field	is_nonreal_nested_type:1;
			/* TRUE if the symbol represents the nonreal version
			   of a nested type of a class template.  This is the
			   symbol pointed to by the
			   corresp_nonreal_or_nested_type field of the
			   original nested type symbol. */
  a_bit_field	template_param_not_visible:1;
			/* TRUE if this is a template parameter that should
			   not be visible for name lookup purposes at this
			   point in time. */
  a_bit_field	force_external_linkage:1;
			/* TRUE if this is a class or enum type that has been
			   used in a way that would force external linkage (if
			   it has linkage at all).  Maintained only in
			   cfront mode (in other modes, the linkage of a class
			   or enum is not affected by the ways it is used). */
  a_bit_field	ambiguous:1;
			/* TRUE if the symbol name is ambiguous in the current
			   scope, i.e., another symbol with the same name is
			   visible, and there is no reason to prefer one over
			   the other.  This is used for sk_projection,
			   sk_namespace_projection, and sk_overloaded_function
			   symbols that are synthesized namespace projection
			   symbols.  Also TRUE for a symbol representing a
			   parameter with a duplicate name in GNU modes
			   (e.g., "int f(int i, int i);"). */
  a_bit_field	synthesized_namespace_projection:1;
			/* TRUE for sk_namespace_projection and
			   sk_overloaded_function symbols that were created
			   as a result of a lookup that found one or more
			   symbols that are visible as a result of
			   using directives.  Also TRUE for
                           sk_overloaded_function symbols created by
			   template instantiation lookups. */
  a_bit_field	qualified_lookup:1;
			/* TRUE for synthesized namespace projection symbols
			   that were generated as a result of a namespace
			   qualified lookup.  Also TRUE for class member
			   symbols that are invisible (is_invisible is TRUE),
			   but should be found by qualified lookup. */
  a_bit_field	must_be_class_or_namespace_lookup:1;
			/* TRUE for synthesized namespace projection symbols
			   that were generated as a result of an
			   IDL_MUST_BE_CLASS_OR_NAMESPACE lookup. */
  a_bit_field	must_be_tag_lookup:1;
			/* TRUE for synthesized namespace projection symbols
			   that were generated as a result of an
			   IDL_MUST_BE_TAG lookup. */
  a_bit_field	tentative_type_lookup:1;
			/* TRUE for synthesized namespace projection symbols
			   that were generated as a result of an
			   IDL_TENTATIVE_TYPE_LOOKUP lookup. */
  a_bit_field	do_not_reuse:1;
			/* TRUE for synthesized namespace symbols generated
			   as a result of a special lookup that cannot be
			   reused by a subsequent lookup. */
  a_bit_field	instantiation_context_lookup:1;
			/* TRUE for synthesized namespace projection symbols
			   that are generated as a result of an instantiation
			   context lookup. */
  a_bit_field	must_be_class_lookup:1;
			/* TRUE for synthesized namespace projection symbols
			   that were generated as a result of an
			   IDL_MUST_BE_CLASS lookup. */
  a_bit_field	must_be_namespace_lookup:1;
			/* TRUE for synthesized namespace projection symbols
			   that were generated as a result of an
			   IDL_MUST_BE_NAMESPACE lookup. */
  a_bit_field	hidden_by_old_for_init:1;
			/* TRUE for a symbol that is visible with the new
			   for-init scoping rules but would be hidden if old
			   (cfront-compatible) scoping were used.  For example,
			       int i;
			       void f() {
			         for (int i = 0; i < 10; i++) { ... }
			         return i;
			       }
			   Under the old scoping rules the local i is returned
			   and ::i is hidden, but by the new rules the local
			   i goes out of scope and ::i is returned.  Unless
			   global flag use_nonstandard_for_init_scope is TRUE,
			   hidden_by_for_init will be set for ::i in the
			   function scope (following the termination of the
			   for statement) and will be cleared again once the
			   function scope is terminated.  A symbol for which
			   this flag is set is pointed to by an entry of type
			   a_name_hidden_by_old_for_init entry, accessed from
			   the scope stack.  (Used in C++ only.) */
  a_bit_field	overload_set_member:1;
			/* TRUE for a symbol that is on the symbols list of
			   an sk_overloaded_function symbol. */
  a_bit_field	is_invisible:1;
			/* TRUE for a symbol that is "invisible" (i.e., to be
			   ignored during normal lookup).  This occurs when
			   the initial declaration of a function or class is
			   a friend declaration; the entity becomes visible
			   only when it is subsequently declared in the
			   scope to which it belongs.  This is also used for
			   projection symbols to names found in base classes
			   that are ignored during normal lookup (when doing
			   dependent name processing).  It is also used to
			   disable keywords using pragma directives and for
			   types that are predeclared but not usable until an
			   explicit declaration is seen, such as
			   std::align_val_t. */
  a_bit_field	ignore_in_decl_scope:1;
			/* This flag can be set for alias template symbols.
			   If it is set, ignore this symbol if it is the
			   same as the template associated with the innermost
			   instantiation scope.  This is used to enforce the
			   point of declaration rules for alias templates
			   during their instantiation. */
  a_bit_field	is_unknown_function:1;
			/* TRUE if this symbol was created to represent an
			   unknown function. */
  a_bit_field	is_nonreal_member:1;
			/* TRUE if this symbol represents a member of a
			   nonreal class. */
  a_bit_field	potentially_overloaded:1;
			/* TRUE for a function, overloaded function, or
			   template function symbol in a prototype
			   instantiation if the scope also contains a
			   using-declaration that could be an additional
			   member of the overload set. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field
		is_super_reference:1;
			/* TRUE for projection and overloaded function
			   symbols used to represent a Microsoft __super
			   lookup. */
  a_bit_field	is_microsoft_invisible_operator:1;
			/* Used in Microsoft mode to indicate that an operator
			   function should be treated as invisible when
			   referenced using operator notation.  Functions
			   defined in friend declarations (and not declared
			   elsewhere) have this flag set. */
  a_bit_field	hide_by_sig_lookup_done:1;
			/* TRUE if the hide-by-sig processing has already
			   been done for this symbol, in which case the
			   hide_by_sig_lookup_field contains the lookup
			   result to be used. */
  a_bit_field	suppress_hide_by_sig_lookup:1;
			/* TRUE if hide_by_sig_lookup_done is TRUE and
			   the hide-by-sig processing determined that the
			   original normal lookup result should be used
			   instead of the hide-by-sig lookup result. */
  a_bit_field
		declared_in_for_init:1;
			/* TRUE if this symbol represents a variable that was
			   declared in a for-init block in Microsoft mode. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  a_bit_field	is_alias:1;
			/* Used in GNU mode for routines and variables declared
			   with attribute "alias" or attribute "weakref". */
#endif /* GNU_EXTENSIONS_ALLOWED */
  a_bit_field	is_pack_element:1;
			/* TRUE for variable symbols created as elements of
			   a function parameter pack and also TRUE for
			   template parameter symbols for template parameters
			   that were declared as packs. */
  a_bit_field	is_pack_expansion:1;
			/* TRUE if this is the dummy symbol created for
			   an empty template parameter pack expansion. */
  a_bit_field	is_nondeducible_pack:1;
			/* TRUE if this is a template parameter pack that can
			   never be deduced as it is being used in a
			   non-deduced context. */
  a_bit_field
		value_has_been_set:1;
			/* TRUE for a variable or static data member that was
			   initialized (explicitly or implicitly), that has
			   been assigned to, or that has had its address taken.
			   Also TRUE if it is of aggregate type and at least
			   one of its fields or elements has been assigned to
			   or has had its address taken.
			   Also TRUE for a variable if its storage class is
			   extern, since its value will be set elsewhere in the
			   definition. */
  a_bit_field
		from_module_code:1;
			/* TRUE if this is a symbol created from code generated
			   from a compiled module file. */
  union {
    /* When kind == sk_undefined, no variant fields. */
    /* When kind == sk_keyword: */
    struct {
      a_token_kind
		token;
			/* For keywords, the token identifying the keyword. */
      a_bit_field
		is_preprocessing_op_or_punc:1;
			/* TRUE for symbols corresponding to keywords that
			   are also preprocessing tokens (e.g., "and"). */
      an_error_code
		diagnostic_issued_if_used;
			/* The error code of a diagnostic to be issued
			   the first time that this keyword is used.
			   The error code is replaced with ec_no_error
			   after the diagnostic has been issued. */
    } keyword;
    /* When kind == sk_macro: */
    a_macro_def_ptr
		macro_def;
			/* A structure defining the macro. */
    /* When kind == sk_constant: */
    a_constant_ptr
		constant;
			/* The value of the constant. */
    /* When kind == sk_type: */
    struct {
      a_type_ptr
		ptr;
			/* The type. */
#if NEED_NAME_MANGLING
      a_discriminator
		discriminator;
			/* An identifying number used to distinguish multiple
			   entities with the same name in the same function.
			   Zero if not needed. */
#endif /* NEED_NAME_MANGLING */
      a_byte_boolean
		is_injected_class_name;
			/* TRUE if the symbol represents an injected class
			   name generated by the compiler (C++ only). */
    } type;
    /* When kind == sk_enum_tag: */
    struct {
      a_type_ptr
		type;
			/* The type that represents the enumeration. */
      an_enum_symbol_supplement_ptr
		extra_info;
			/* Pointer to an entry providing additional info about
			   an enum type. */
    } enumeration;
    /* When kind == sk_class_or_struct_tag or sk_union_tag: */
    struct {
      a_type_ptr
		type;
			/* The type. */
      a_class_symbol_supplement_ptr
		extra_info;
			/* Pointer to an entry providing additional info about
			   a C++ class. */
    } class_struct_union;
    /* When kind == sk_variable: */
    struct {
      a_variable_ptr
		ptr;
			/* Pointer to the variable entry. */
      a_template_instance_ptr
                instance_ptr;
			/* For a symbol that represents a variable template
			   instance (real or prototype), a pointer to an
			   entry providing additional information about
			   whether and how to define the variable.  NULL
			   otherwise. */
#if NEED_NAME_MANGLING
      a_discriminator
		discriminator;
			/* An identifying number used to distinguish multiple
			   entities with the same name in the same function.
			   Zero if not needed. */
#endif /* NEED_NAME_MANGLING */
    } variable;
    /* When kind == sk_static_data_member: */
    struct {
      a_variable_ptr
		variable;
			/* Pointer to the variable entry. */
      a_template_instance_ptr
                instance_ptr;
			/* For a symbol that represents a static data member
			   of a (real or prototype) instantiation of a class
			   template, a pointer to an entry providing
			   additional information about whether and how to
			   define the static data member.  NULL otherwise. */
      a_static_data_member_supplement_ptr
		extra_info;
			/* Pointer to an entry providing additional info about
			   a static data member. */
    } static_data_member;
    /* When kind == sk_field: */
    struct {
      a_field_ptr
		ptr;
			/* The field. */
      a_symbol_ptr
		anonymous_parent_object;
			/* If this field is a member of an anonymous union,
			   a pointer to the symbol for the (unnamed) variable
			   or field it is associated with; otherwise NULL.
			   Note that the symbol for an anonymous union member
			   is "promoted" into the scope of its parent entity,
			   so that this is a way to get at the intervening
			   anonymous structure(s) it belongs to.  The type of
			   the parent object is normally a union type (or a
			   class type for certain nonstandard anonymous
			   unions); in some modes, that type may be
			   cv-qualified. */
      a_field_symbol_supplement_ptr
		extra_info;
			/* Pointer to an entry providing additional info about
			   a field. */
    } field;
    /* When kind == sk_routine or sk_member_function: */
    struct {
      a_routine_ptr 
                ptr;
			/* The routine. */
      a_template_instance_ptr
                instance_ptr;
                        /* Present for template functions and member functions
                           of template classes.  Points to information about
                           the particular instance of the function. */
      a_bit_field
		pending_trailing_requires_clause:1;
			/* TRUE for constrained ordinary member functions of
			   class templates when the constraint has not been
			   substituted yet. */
      a_bit_field
		pending_mapped_exc_spec:1;
			/* TRUE if this function has a pending exception
			   specification (cached but not parsed) that has
			   associated information stored in a hash table
			   (noexcept_args in declarator.c). */
    } routine;
    /* When kind == sk_label: */
    struct {
      a_label_ptr
		ptr;
			/* The label. */
      struct a_control_flow_descr
		*assoc_control_flow_descr;
			/* When the label has been referenced in one or more
			   goto statements but has not yet been defined,
			   pointer to a list of entries identifying the
			   references; and when the label has been defined,
			   a pointer to an entry representing the label
			   statement itself. */
    } label;
    /* When kind == sk_extern_variable or sk_extern_routine: */
    an_extern_symbol_descr_ptr
		extern_symbol_descr;
			/* Information on the external symbol. */
    /* When kind == sk_projection: */
    struct {
      a_projection_descr_ptr
		extra_info;
			/* Additional information about the projection. */
      ENUM_TYPE_FOR_BIT_FIELD(an_access_specifier)
		access:2;
			/* Access to the base class member in the scope of the
			   derived class.  This may differ from the access
			   with which it was originally declared in its own
			   class: when the projection symbol represents a
			   using declaration or access adjustment (i.e., when
			   is_using_decl is TRUE), the access is the declared
			   access in the derived class; otherwise, the access
			   is computed based on both the access of the member
			   within the base class and the access of the base
			   class itself (along the "preferred derivation" --
			   the path affording greatest access -- when there
			   are multiple paths) within the derived class. */
      a_bit_field
		is_using_decl:1;
			/* If TRUE this projection symbol represents a
			   using-declaration. */
      a_bit_field
		any_intervening_using_decl:1;
			/* TRUE if a using-declaration appeared anywhere on the
			   derivation path between the fundamental symbol and
			   the current projection. */
      a_bit_field
		fund_sym_is_nonreal_member:1;
			/* TRUE if the fundamental symbol is a member of
			   a nonreal or proxy class.  Such members are
			   created as a result of a class-qualified
			   lookup of a member in the prototype instantiation
			   of a derived class, when the lookup fails to find
			   a member in the derived class or any of the real
		 	   base classes. */
      a_bit_field
		injected_class_template_name_is_unambiguous:1;
			/* If ambiguous is TRUE, this flag is TRUE if the
			   fundamental symbols represent injected class names
			   for instances of the same class template; if
			   ambiguous is FALSE, this flag is undefined.
			   The flag is TRUE in cases like this:
			     class B : public A<int>, public A<float> { ... };
			   where within B the projections of the injected
			   class names of the base classes are ambiguous
			   insofar as one is interested in the base class type
			   and unambiguous insofar as one is interested in
			   the template. */
    } projection;
    /* When kind = sk_overloaded_function: */
    struct {
      a_symbol_ptr
		symbols;
			/* Linked list of two or more symbols comprising a
			   function overload set, where each symbol in the
			   list has the same name as the current symbol.  When
			   the latter is a class member, each symbol in the
			   list is an sk_member_function or
			   sk_function_template symbol or an sk_projection
			   symbol that points to an sk_member_function or
			   sk_function_template fundamental symbol, or, when
			   fund_sym_is_nonreal_member is set on it, to a
			   symbol standing for a member of a nonreal class.
			   When the current symbol is not a class member and
			   synthesized_namespace_projection is FALSE, each
			   symbol is an sk_routine or sk_function_template
			   symbol or an sk_namespace_projection symbol that
			   points to an sk_routine or sk_function_template
			   symbol.  When synthesized_namespace_projection is
			   TRUE, the symbols in the list can be a combination
			   of sk_routine, sk_member_function, sk_projection,
			   sk_namespace_projection, and sk_function_template
			   symbols (and the sk_function_template symbols can
			   be members and/or nonmembers). */
      a_byte_boolean
		mixed_static_nonstatic;
			/* TRUE when the current symbol is a class member and
			   some but not all the members of the overload set
			   are static member functions. */
    } overloaded_function;
    /* When kind == sk_parameter: */
    a_param_id_ptr
		param_id;
			/* Pointer to the param_id entry with which this
			   symbol is associated. */
    /* When kind = sk_class_template, sk_function_template, or
       sk_variable_template: */
    a_template_symbol_supplement_ptr
                template_info;
			/* Pointer to an entry providing additional info about
			   a C++ class template, function template, or variable
			   template. */
    /* When kind == sk_namespace: */
    struct {
      a_namespace_ptr
		ptr;
			/* The IL entry for the namespace. */
      a_namespace_symbol_supplement_ptr
		extra_info;
			/* Pointer to an entry providing additional info
			   about a C++ namespace definition; NULL when the
			   symbol represents a namespace alias. */
    } namespace_info;
    /* When kind == sk_namespace_projection: */
    struct {
      a_symbol_ptr
		fundamental_symbol;
			/* Pointer to the symbol representing the fundamental
			   namespace member to which this projection refers.
			   For instance:
			     namespace A { int i; }
			     namespace B { using A::i; }
			     namespace C { using B::i; }
			   The sk_namespace_projection symbols in the scopes
			   of B and C both point to A::i as fundamental
			   symbol: the fundamental_symbol is never itself an
			   an sk_namespace_projection symbol.  Nor will the
			   fundamental symbol be an sk_overloaded_function
			   symbol; rather, separate projection symbols will be
			   created for members of the fundamental namespace's
			   overload set. */
      ENUM_TYPE_FOR_BIT_FIELD(an_access_specifier)
		access:2;
			/* When the projection symbol represents a using
			   declaration in class scope (a using declaration for
			   an enumerator in C++20), the declared access in the
			   class; otherwise as_public. */
      a_bit_field
		is_using_decl:1;
			/* If TRUE this projection symbol represents a
			   using-declaration. */
    } namespace_projection;
    /* When kind == sk_named_module: */
    struct {
      a_symbol_header_ptr
		primary_name;
			/* The symbol header representing the named module's
			   name. */
      a_symbol_header_ptr
		partition_name;
			/* If this symbol is representing a named module
			   partition, this is the symbol header representing
			   the name of the partition; otherwise, NULL. */
      a_bit_field
		is_interface_unit:1;
			/* TRUE if this is a module interface unit. */
      a_bit_field
		is_header_unit:1;
			/* TRUE if this is a module header unit. */
    } module_info;
#if NAMED_ADDRESS_SPACES_ALLOWED
    /* When kind == sk_named_address_space: */
    struct {
      a_named_address_space_id
		id;
			/* A small integer identifying the named address
			   space. */
    } named_address_space;
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
#if NAMED_REGISTERS_ALLOWED
    /* When kind == sk_named_register: */
    struct {
      a_named_register_id
		id;
			/* A small integer identifying the named-register
			   storage class. */
    } named_register;
#endif /* NAMED_REGISTERS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* When kind == sk_property_set: */
    a_property_set_symbol_supplement_ptr
		property_info;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } variant;
} a_symbol;


/*
Macros to retrieve the parent class or namespace associated with a symbol.
*/
#define sym_is_class_or_namespace_member(sym)                                \
  ((sym)->is_class_member || (sym)->parent.namespace_ptr != NULL)

#define sym_is_namespace_member(sym)                                         \
  (!(sym)->is_class_member && (sym)->parent.namespace_ptr != NULL)


#if defined(_lint)
/* When linting, duplicate the macro argument to catch side-effects that would
   be duplicated in the EXPENSIVE_CHECKING version, but don't call
   check_assertion since that results in spurious lint errors when the macro
   is used in a macro that itself duplicates its argument. */
/*lint -emacro(505 664,sym_parent_namespace)*/
#define sym_parent_namespace(sym)                                            \
  ((void)sym_is_namespace_member(sym),                                       \
   (sym)->parent.namespace_ptr)
#else /* !defined(_lint) */
#if EXPENSIVE_CHECKING
#define sym_parent_namespace(sym)                                            \
  (check_assertion(sym_is_namespace_member(sym)),                            \
   (sym)->parent.namespace_ptr)
#else /* !EXPENSIVE_CHECKING */
#define sym_parent_namespace(sym)                                            \
  ((sym)->parent.namespace_ptr)
#endif /* EXPENSIVE_CHECKING */
#endif /* defined(_lint) */


#if defined(_lint)
/* When linting, duplicate the macro argument to catch side-effects that would
   be duplicated in the EXPENSIVE_CHECKING version, but don't call
   check_assertion since that results in spurious lint errors when the macro
   is used in a macro that itself duplicates its argument. */
/*lint -emacro(505 664,sym_parent_namespace_or_null)*/
#define sym_parent_namespace_or_null(sym)                                    \
  ((void)(sym)->is_class_member, (sym)->parent.namespace_ptr)
#else /* !defined(_lint) */
#if EXPENSIVE_CHECKING
#define sym_parent_namespace_or_null(sym)                                    \
  (check_assertion(!(sym)->is_class_member), (sym)->parent.namespace_ptr)
#else /* !EXPENSIVE_CHECKING */
#define sym_parent_namespace_or_null(sym)                                    \
  ((sym)->parent.namespace_ptr)
#endif /* EXPENSIVE_CHECKING */
#endif /* defined(_lint) */


#if defined(_lint)
/* When linting, duplicate the macro argument to catch side-effects that would
   be duplicated in the EXPENSIVE_CHECKING version, but don't call
   check_assertion since that results in spurious lint errors when the macro
   is used in a macro that itself duplicates its argument. */
/*lint -emacro(505 664,sym_parent_class)*/
#define sym_parent_class(sym)                                                \
  ((void)(sym)->is_class_member, (sym)->parent.class_type)
#else /* !defined(_lint) */
#if EXPENSIVE_CHECKING
#define sym_parent_class(sym)                                                \
  (check_assertion((sym)->is_class_member), (sym)->parent.class_type)
#else /* !EXPENSIVE_CHECKING */
#define sym_parent_class(sym)                                                \
  ((sym)->parent.class_type)
#endif /* EXPENSIVE_CHECKING */
#endif /* defined(_lint) */


#if MICROSOFT_EXTENSIONS_ALLOWED
/*
Data structures related to CLI operators.
*/

/*
An enumeration of CLI operators that may appear in metadata.  This enumeration
corresponds to the list of operators in ECMA-372.  Note that not all of these
operators have C++/CLI counterparts.
*/
enum a_cli_operator_kind : a_byte {
  cok_none,                            /* No operator. */
  cok_first,
  cok_addition = cok_first,            /* "op_Addition" */
  cok_addition_assignment,             /* "op_AdditionAssignment" */
  cok_address_of,                      /* "op_AddressOf" */
  cok_assign,                          /* "op_Assign" */
  cok_bitwise_and,                     /* "op_BitwiseAnd" */
  cok_bitwise_and_assignment,          /* "op_BitwiseAndAssignment" */
  cok_bitwise_or,                      /* "op_BitwiseOr" */
  cok_bitwise_or_assignment,           /* "op_BitwiseOrAssignment" */
  cok_comma,                           /* "op_Comma" */
  cok_decrement,                       /* "op_Decrement" */
  cok_division,                        /* "op_Division" */
  cok_division_assignment,             /* "op_DivisionAssignment" */
  cok_equality,                        /* "op_Equality" */
  cok_exclusive_or,                    /* "op_ExclusiveOr" */
  cok_exclusive_or_assignment,         /* "op_ExclusiveOrAssignment" */
  cok_explicit,                        /* "op_Explicit" */
  cok_false,                           /* "op_False" */
  cok_function_call,                   /* "op_FunctionCall" */
  cok_greater_than,                    /* "op_GreaterThan" */
  cok_greater_than_or_equal,           /* "op_GreaterThanOrEqual" */
  cok_implicit,                        /* "op_Implicit" */
  cok_increment,                       /* "op_Increment" */
  cok_inequality,                      /* "op_Inequality" */
  cok_left_shift,                      /* "op_LeftShift" */
  cok_left_shift_assignment,           /* "op_LeftShiftAssignment" */
  cok_less_than,                       /* "op_LessThan" */
  cok_less_than_or_equal,              /* "op_LessThanOrEqual" */
  cok_logical_and,                     /* "op_LogicalAnd" */
  cok_logical_not,                     /* "op_LogicalNot" */
  cok_logical_or,                      /* "op_LogicalOr" */
  cok_member_selection,                /* "op_MemberSelection" */
  cok_modulus,                         /* "op_Modulus" */
  cok_modulus_assignment,              /* "op_ModulusAssignment" */
  cok_multiply,                        /* "op_Multiply" */
  cok_multiplication_assignment,       /* "op_MultiplicationAssignment" */
  cok_ones_complement,                 /* "op_OnesComplement" */
  cok_pointer_dereference,             /* "op_PointerDereference" */
  cok_pointer_to_member_selection,     /* "op_PointerToMemberSelection" */
  cok_right_shift,                     /* "op_RightShift" */
  cok_right_shift_assignment,          /* "op_RightShiftAssignment" */
  cok_signed_right_shift,              /* "op_SignedRightShift" */
  cok_subscript,                       /* "op_Subscript" */
  cok_subtraction,                     /* "op_Subtraction" */
  cok_subtraction_assignment,          /* "op_SubtractionAssignment" */
  cok_true,                            /* "op_True" */
  cok_unary_negation,                  /* "op_UnaryNegation" */
  cok_unary_plus,                      /* "op_UnaryPlus" */
  cok_unsigned_right_shift,            /* "op_UnsignedRightShift" */
  cok_unsigned_right_shift_assignment, /* "op_UnsignedRightShiftAssignment" */
  cok_last
};


typedef struct a_cli_operator_info *a_cli_operator_info_ptr;
typedef struct a_cli_operator_info {
  /* Structure representing information associated with a CLI operator. */
  a_const_char	*cli_name;
			/* Name of the operator. */
  a_const_char	*cpp_name;
			/* Name of the operator in C++/CLI; NULL if no such
			   mapping exists. */
  a_boolean	is_assignment_operator;
			/* TRUE if the operator is an assignment operator. */
} a_cli_operator_info;

/*
Table of a_cli_operator_info structures corresponding to each entry in 
a_cli_operator_kind.
*/
EXTERN_CONSTINIT_ARRAY(a_cli_operator_info, cli_operator_info, cok_last + 1)
#if VAR_INITIALIZERS
= {
  { "<none>", NULL, FALSE },
  { "op_Addition", "operator+", FALSE },
  { "op_AdditionAssignment", "operator+=", TRUE  },
  { "op_AddressOf", "operator&", FALSE },
  { "op_Assign", "operator=", TRUE  },
  { "op_BitwiseAnd", "operator&", FALSE },
  { "op_BitwiseAndAssignment", "operator&=", TRUE  },
  { "op_BitwiseOr", "operator|", FALSE },
  { "op_BitwiseOrAssignment", "operator|=", TRUE  },
  { "op_Comma", "operator,", FALSE },
  { "op_Decrement", "operator--", FALSE },
  { "op_Division", "operator/", FALSE },
  { "op_DivisionAssignment", "operator/=", TRUE  },
  { "op_Equality", "operator==", FALSE },
  { "op_ExclusiveOr", "operator^", FALSE },
  { "op_ExclusiveOrAssignment", "operator^=", TRUE  },
  { "op_Explicit", NULL, FALSE },
  { "op_False", NULL, FALSE },
  { "op_FunctionCall", "operator()", FALSE },
  { "op_GreaterThan", "operator>", FALSE },
  { "op_GreaterThanOrEqual", "operator>=", FALSE },
  { "op_Implicit", NULL, FALSE },
  { "op_Increment", "operator++", FALSE },
  { "op_Inequality", "operator!=", FALSE },
  { "op_LeftShift", "operator<<", FALSE },
  { "op_LeftShiftAssignment", "operator<<=", TRUE  },
  { "op_LessThan", "operator<", FALSE },
  { "op_LessThanOrEqual", "operator<=", FALSE },
  { "op_LogicalAnd", "operator&&", FALSE },
  { "op_LogicalNot", "operator!", FALSE },
  { "op_LogicalOr", "operator||", FALSE },
  { "op_MemberSelection", "operator->", FALSE },
  { "op_Modulus", "operator%", FALSE },
  { "op_ModulusAssignment", "operator%=", TRUE  },
  { "op_Multiply", "operator*", FALSE },
  { "op_MultiplicationAssignment", "operator*=", TRUE  },
  { "op_OnesComplement", "operator~", FALSE },
  { "op_PointerDereference", "operator*", FALSE },
  { "op_PointerToMemberSelection", NULL, FALSE },
  { "op_RightShift", "operator>>", FALSE },
  { "op_RightShiftAssignment", "operator>>=", TRUE  },
  { "op_SignedRightShift", NULL, FALSE },
  { "op_Subscript", "operator[]", FALSE },
  { "op_Subtraction", "operator-", FALSE },
  { "op_SubtractionAssignment", "operator-=", TRUE  },
  { "op_True", NULL, FALSE },
  { "op_UnaryNegation", "operator-", FALSE },
  { "op_UnaryPlus", "operator+", FALSE },
  { "op_UnsignedRightShift", NULL, FALSE  },
  { "op_UnsignedRightShiftAssignment", NULL, TRUE },
  { "last", NULL, FALSE } /* cok_last */
}
#endif /* VAR_INITIALIZERS */
EXTERN_CONSTINIT_ARRAY_END(cli_operator_info)

#define cli_operator_info_from_kind(cok) (&cli_operator_info[(int)(cok)])


extern void init_cli_operator_headers(void);

#if CPPCLI_ENABLING_POSSIBLE && EDG_WIN32
extern a_cli_operator_kind find_cli_operator_kind(a_const_char *identifier);
#endif /* CPPCLI_ENABLING_POSSIBLE && EDG_WIN32 */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
The kind of module entry locator.
*/
enum a_module_entry_locator_kind {
  melk_none,
  melk_ifc
};

/*
This is a forward declaration of an_ifc_module_file for use by the symbol
table.
*/
struct an_ifc_module_file;

/*
This is a forward declaration of an_ifc_partition_kind for use by the symbol
table.
*/
enum an_ifc_partition_kind : uint32_t;

/*
A lightweight representation of a module entry that can be resolved by the
corresponding module interface to a concrete entry corresponding to an IL
entity itself (or some components thereof).
*/
struct a_module_entry_locator {
  a_module_entry_locator_kind
		kind;	/* The kind of module entry locator. */
  union {
    /* When kind == melk_none: no variant fields. */
    /* When kind == melk_ifc: */
    struct {
      an_ifc_partition_kind
		partition;
			/* The IFC partition kind. */
      sizeof_t	offset; /* The file offset into the IFC where this entity is
			   defined. */
      an_ifc_module_file
		*file;	/* An opaque pointer to the IFC module file
			   (an_ifc_module_file) containing this declaration. */
    } ifc;
  } variant;
};  /* a_module_entry_locator */


inline a_boolean operator==(a_module_entry_locator a,
                            a_module_entry_locator b)
/*
Return TRUE if the given module entry locators are equal; otherwise, return
FALSE.
*/
{
  a_boolean result = TRUE;

  if (a.kind != b.kind) {
    result = FALSE;
  } else {
    switch (a.kind) {
      case melk_none:
        break;
      case melk_ifc:
        { auto &av = a.variant.ifc;
          auto &bv = b.variant.ifc;

          if (av.partition != bv.partition) {
            result = FALSE;
          } else if (av.offset != bv.offset) {
            result = FALSE;
          } else if (av.file != bv.file) {
            result = FALSE;
          }  /* if */
        }  /* if */
        break;
      default_is_unexpected();
    }  /* switch */
  }  /* if */
  return result;
}  /* operator== */


inline a_boolean operator!=(a_module_entry_locator a,
                            a_module_entry_locator b)
/*
Return FALSE if the given module entry locators are equal; otherwise, return
TRUE.
*/
{
  return !(a == b);
}  /* operator!= */

namespace detail {

/*
The following specializations provide Is_trivially_copyable and
Is_trivially_destructible support for a_module_entry_locator.
*/

template<>
struct Is_trivially_copyable_edg_impl<a_module_entry_locator> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_copyable_edg_impl */

template<>
struct Is_trivially_destructible_edg_impl<a_module_entry_locator> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_destructible_edg_impl */

}  /* namespace detail */

/*
A pairing of a scope and a module entry locator.
*/
struct a_deferred_module_entry {
  a_scope_ptr	scope;	/* The scope the module entry lives in. */
  a_module_entry_locator
		locator;
			/* The module entry locator (used to resolve the entry
			   itself). */
};  /* a_deferred_module_entry */

namespace detail {

/*
The following specializations provide Is_trivially_copyable and
Is_trivially_destructible support for a_deferred_module_entry.
*/

template<>
struct Is_trivially_copyable_edg_impl<a_deferred_module_entry> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_copyable_edg_impl */


template<>
struct Is_trivially_destructible_edg_impl<a_deferred_module_entry> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_destructible_edg_impl */

}  /* namespace detail */

/*
A type used to encapsulate the list of module entries that should be considered
for lazy loading of a given symbol header.
*/
struct a_deferred_module_entry_array {
  Small_dyn_array<a_deferred_module_entry, 5>
		entries = {};
			/* The module entries to consider. */
  size_t	num_active_scopes = 0;
			/* The number of times the associated entries are being
			   considered in the current call stack.  This is used
			   to determine if it's safe to clean up processed
			   entries (by virtue of knowing if there are other
			   calls currently reading from the entries
			   Dyn_array). */
  size_t	num_processed = 0;
			/* The number of entries that have been processed.
			   This is used to determine if there's any need to
			   perform cleanup of the entries array. */
};  /* a_deferred_module_entry_array */

typedef struct a_symbol_header {
  /* This is the container for information that the symbol table
     management routines use in manipulating a list of symbols that have
     the same name. */
  a_symbol_header_ptr
		next;
			/* This is the pointer to the next symbol header in the
			   same bucket of the symbol table.  This field is NULL
			   if this is the last symbol header in this bucket. */
  a_const_char	*identifier;
			/* A pointer to a null-terminated string containing the
			   name of the symbol. */
  sizeof_t	identifier_length;
			/* The length of the identifier, not counting the
			   final null. */
  a_symbol_ptr	symbol;
			/* This is the pointer to a symbol table entry.  This
			   is actually a list of all symbols with the same
			   identifier. */
  a_symbol_ptr	inactive_symbols;
			/* A list of symbols that are currently inactive
			   but can be reached with some sort of qualification,
			   i.e., members of structs/unions/classes. */
  a_symbol_ptr	other_symbols;
			/* sk_extern_variable, sk_extern_routine and
                           synthesized namespace projection symbols
			   associated with this name. */
  a_saved_macro_state_ptr
		saved_macro_stack;
			/* A stack of entries used to save and restore macro
			   state information.  Used by the push_macro and
			   pop_macro pragmas. */
  a_hash_value	hash_value;
			/* The hash value for the identifier.  This is saved
			   to avoid the need to recompute it if the header
			   is entered into a scope's lookup table. */
  a_deferred_module_entry_array
		*deferred_module_entries;
			/* A list of module file entries that match this symbol
			   header that have been deferred because no lookup has
			   been performed on this symbol header yet. */
  union {
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* When is_cli_operator is TRUE: */
    a_cli_operator_kind
		cli_operator;
			/* The CLI operator kind that corresponds to this
			   header. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* When is_cli_operator is FALSE: */
    an_opname_kind
		opname;
			/* If the symbol header is for an operator name, this
			   identifies the particular operator kind.  For
			   other kinds of symbols, this is onk_none. */
  } variant;
  a_bit_field	is_unnamed:1;
			/* TRUE if this symbol header reflects an unnamed
			   entity. */
  a_bit_field	has_intrinsic_name:1;
			/* TRUE if the identifier is used as the name of an
			   intrinsic construct (e.g., C++20's
			   "is_constant_evaluated"). */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	microsoft_identifier_used:1;
			/* TRUE if the identifier was named using a Microsoft
			   __identifier operator.  This flag is set if any
			   reference to the identifier used __identifier. */
  a_bit_field	is_cli_operator:1;
			/* TRUE in C++/CLI mode if the symbol header is for a
			   name that matches the metadata name of a CLI
			   operator.  (The symbols under this header may not
			   actually represent CLI operators, but this flag
			   permits a more efficient check in contexts where
			   CLI operator names are reserved.) */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_bit_field	any_nested_types_on_inactive_list:1;
			/* TRUE if a symbol for a nested type has been
                           transferred to the inactive list.  This field is
                           used to speed up processing to support the
                           nested class anachronism (ARM 18.3.5) and is
                           only set when anachronisms are allowed. */
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
  a_bit_field	has_cfront_transitional_nested_type_mangled_name:1;
                        /* TRUE if a nested type has been flagged for
                           special handling during name mangling.  The first
                           nested type with a given name will have this flag
                           set indicating that its name should be mangled as
                           if it were not a nested type. */
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
#if RECORD_HIDDEN_NAMES_IN_IL
  a_bit_field	any_tag_decl:1;
			/* TRUE if any symbol represents a tag declaration. */
  a_bit_field	any_decl_in_file_or_namespace_scope:1;
			/* TRUE if any symbol represents a declaration in
			   the file scope or in a namespace scope (i.e., a
			   non-class-member declaration that can be referred
			   to with a qualified name). */
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
  a_bit_field	any_function_referenced_in_dependent_call:1;
			/* TRUE if a dependent function call referenced a
			   non-member function with the name given by
			   this header.  This is used to suppress warnings
			   about unused static functions in some cases. */
#if UNICODE_VULNERABILITY_DETECTION_SUPPORTED
  a_bit_field	id_added_to_map:1;
			/* Initially FALSE; set to TRUE when the identifier
			   has been added to the id_representation_map (see
			   lexical.c for details) in order to avoid the
			   overhead of multiple hash table lookups when the
			   identifier is encountered multiple times. */
#endif /* UNICODE_VULNERABILITY_DETECTION_SUPPORTED */
#if PRAGMA_WEAK_ALLOWED
  a_bit_field	named_in_weak_pragma:1;
			/* TRUE if we saw a #pragma weak <id> that named this
			   identifier. */
#endif /* PRAGMA_WEAK_ALLOWED */
#if BUILTIN_FUNCTIONS_ENABLED
  a_bit_field	is_builtin_function:1;
                        /* TRUE if this symbol header is for a builtin
                           function (which may or may not have been loaded). */
  a_bit_field	is_builtin_overloadable:1;
                        /* TRUE if this is a builtin that may be overloaded. */
  a_bit_field	is_builtin_overload_set:1;
                        /* TRUE if this is a builtin whose name denotes a set
                           of overloaded builtin functions; the routines of the
                           set are entered lazily when the name is first
                           referenced. */
  a_bit_field	is_builtin_deferred:1;
                        /* TRUE if this is an overloadable builtin whose
                           routine has not yet been entered and whose builtin
                           table entry is recorded in the
                           builtin_function_category and builtin_function_index
                           fields, so that the routine can be entered lazily
                           when the name is first referenced. */
  a_bit_field	builtin_has_been_loaded:1;
                        /* TRUE if this is a builtin (i.e., is_builtin_function
                           is TRUE) and the builtin has been loaded.  Relevant
                           only for the primary translation unit. */
  a_builtin_function_category
                builtin_function_category;
                        /* Category of the builtin function, describing where
                           information about the builtin is to be found (either
                           in the user-defined builtin table
                           (builtin_user_table) or one of the system builtin
                           tables). */
  a_builtin_function_index
                builtin_function_index;
                        /* When is_builtin_function is TRUE and
                           is_builtin_overload_set is FALSE, the value is an
                           index into either a system builtin table or the
                           builtin_user_table depending on the value of
                           builtin_function_category (the members of an
                           overload set are recorded separately). */
#endif /* BUILTIN_FUNCTIONS_ENABLED */
} a_symbol_header;


#if BUILTIN_FUNCTIONS_ENABLED

/* Macro that is TRUE if the symbol header refers to a builtin function that
   has not been loaded yet. */
#define builtin_needs_to_be_loaded(sym_hdr) \
  ((sym_hdr)->is_builtin_function && \
   (is_primary_translation_unit ? !(sym_hdr)->builtin_has_been_loaded : \
            builtin_needs_to_be_loaded_in_secondary_translation_unit(sym_hdr)))

void mark_builtin_loaded(a_symbol_header *sym_hdr);

#endif /* BUILTIN_FUNCTIONS_ENABLED */

#define SYMBOL_TABLE_SIZE 262133
	  		/* The number of buckets in the symbol table.  This
			   number should be prime.  (262133 is a prime close
			   to 2^18 and the optimized code to compute a
			   remainder modulo-262133 is slightly more efficient
			   than for other nearby primes.) */

/*
Top level structure for the hash-table portion of the symbol table.  Each
bucket of the array contains a pointer to a list of symbol headers whose
identifiers hash to that bucket.  (Other portions of the symbol table,
defined for C++ only, are the opname_symbol_table, for accessing operator
functions by operator, and the conversion_header_list, for accessing
conversion functions by destination type.)  
*/
EXTERN_THREAD a_symbol_header_ptr
		symbol_table[SYMBOL_TABLE_SIZE];

/*
Table of pointers to symbol headers for C++ operator name symbols, for
names like "operator+".  Indexed by opname kind.
*/
EXTERN_THREAD a_symbol_header_ptr
		opname_symbol_table[(int)onk_last];

typedef struct a_conversion_header *a_conversion_header_ptr;
typedef struct a_conversion_header {
  /* Top level lookup mechanism for symbols that identify user defined
     conversion functions.  Since such symbols are looked up by return
     type, they do not appear in the symbol proper.  Each conversion header
     entry points to a symbol header that points to symbols for all the
     user-defined conversion functions that return objects of a given type. */
  a_conversion_header_ptr
		next;
			/* Next in a linked list of conversion header
			   entries; NULL for the last entry on the list. */
  a_symbol_header_ptr
		symbol_header;
			/* Pointer to the symbol header pointing to conversion
			   functions whose destination type is "type". */
  a_type_ptr	type;
			/* Pointer to the type entry by which the symbol
			   header is looked up. */
} a_conversion_header;

/*
List of conversion header entries that serve as a lookup list for conversion
function symbols.
*/
EXTERN_THREAD a_conversion_header_ptr
		conversion_header_list;

typedef struct a_literal_operator_header *a_literal_operator_header_ptr;
typedef struct a_literal_operator_header {
  /* Top level lookup mechanism for symbols that identify literal operators
     and literal operator templates (for C++11 user-defined literals).  Each
     literal operator header entry points to a symbol header that points to
     symbols for all the literal operators and literal operator templates
     that have the same ud-suffix identifier. */
  a_literal_operator_header_ptr
		next;	/* Next in a linked list of literal operator header
			   entries, NULL for the last one. */
  a_symbol_header_ptr
		symbol_header;
			/* Pointer to the symbol header pointing to literal
			   operators and literal operator templates whose
			   ud-suffix identifier is of length suffix_len and
			   spelling suffix. */
  a_const_char	*suffix;
			/* The spelling of the ud-suffix. */
  sizeof_t	suffix_len;
			/* The length of the ud-suffix. */
} a_literal_operator_header;

/*
List of literal operator header entries that serve as a lookup list for
literal operators and literal operator templates.
*/
EXTERN_THREAD a_literal_operator_header_ptr
		literal_operator_header_list;

/*
Symbol information related to the current token:
*/
EXTERN_THREAD a_symbol_locator
		locator_for_curr_id;
			/* If curr_token == tok_identifier, this is information
			   fully specifying the identifier.  If curr_token ==
			   tok_ud_literal, this is information specifying the
			   canonical name of the literal operator or literal
			   operator template, i.e., operator ""identifier. */

EXTERN_THREAD an_active_using_directive_ptr
		avail_active_using_directives;
			/* List of active using directive entries freed and
			   available for reuse. */

EXTERN_THREAD sizeof_t
		size_scope_stack;
			/* Allocated size of scope_stack in elements.
			   Not per-file. */

#if MICROSOFT_EXTENSIONS_ALLOWED
/* Header for the contextual keyword "safe_cast" used in C++/CLI. */
EXTERN_THREAD a_symbol_header_ptr
		safe_cast_symbol_header;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Entry describing a fixup that is required for a VLA that appears in a
function prototype parameter declaration.  There are two sorts of fixup that
happen once the function scope and its associated memory region are created:
(1) Parameter variable fixup -- If the dimension expression refers to a
    parameter name, the expression will have been scanned before the param
    variable was actually created (since it cannot be created until the
    function's IL scope exists).  Instead, the expression will refer to a
    dummy variable, and the fixup involves replacing the dummy variable with
    the "real" param variable.
(2) Dimension expression fixup -- Every VLA dimension expression is
    eventually represented by a VLA-dimension entry, which points to the
    expression node.  Both the VLA-dimension entry and the expression node
    have to be in the scope of the function, but when the expression is
    originally scanned, the function's IL scope does not yet exist, so the
    fixup involves copying the expression node into the function scope memory
    region and allocating the VLA-dimension entry to point to it.
When no function definition is associated with the function prototype
declaration, the fixup entries are discarded.
*/
typedef struct a_vla_fixup {
  a_vla_fixup_ptr
		next;
			/* Pointer to the next fixup entry on the list. */
  a_type_ptr	array_type;
			/* Pointer to a type entry for a variable length
			   array.  If it is non-NULL, dimension expression
			   fixup is required; otherwise, parameter variable
			   fixup is required. */
  an_expr_node_ptr
                expr;   /* If array_type is NULL, a pointer to an enk_variable
			   expression node which needs to be patched with the
			   correct variable for the function parameter.
			   If array_type is non-NULL, a pointer to an
			   expression representing a variable dimension, and
			   dimension expression fixup will be done. */
  a_symbol_ptr	param_sym;
			/* If array_type is NULL, a pointer to the parameter
			   symbol associated with the param variable fixup.
			   NULL if array_type is non-NULL. */
  a_source_position
		position;
			/* Source position of the VLA expression. */
} a_vla_fixup;


extern void add_vla_fixup_entry(a_type_ptr        array_type,
                                an_expr_node_ptr  expr_node,
                                a_symbol_ptr      param_sym,
                                a_source_position *position);

extern void free_vla_fixup_list(a_vla_fixup_ptr vfp);


typedef struct an_extern_type_fixup {
  /* Entry on a list indicating variables and routines whose types must
     be reset to an earlier state at the end of a scope.  This is
     needed because there is only one IL entry for a variable or routine
     even though there may be several symbols at different scope levels
     with varying visibility of the overall type of the IL entry.
     For example,
       int a[];
       main () {
         extern int a[5];
         ... Type of "a" is now "int [5]".
       }
       ... Type of "a" must be restored to "int []" at the end of "main".
  */
  an_extern_type_fixup_ptr
		next;	/* Pointer to the next fixup entry on the list for
			   the same scope. */
  a_type_ptr	type;	/* The type to be restored. */
  a_boolean	is_routine;
			/* TRUE for routine, FALSE for variable. */
  union {
    /* When is_routine == FALSE: */
    a_variable_ptr
		variable;
			/* The variable whose type is to be changed. */
    /* When is_routine == TRUE: */
    a_routine_ptr
		routine;
			/* The routine whose type is to be changed. */
  } variant;
} an_extern_type_fixup;


/* Contains a description of an access error that has been detected
   for which an error may need to be issued later. */
typedef struct an_access_error_descr {
  an_access_error_descr_ptr
		next;	/* Pointer to the next error description record. */
  struct a_symbol	
		*sym;
			/* Symbol that the program was trying to access
			   that should be included in the error message. */
  struct a_symbol
		*overload_sym;
			/* If "sym" is a member of an overload set, this
			   points to the set. */
  a_type_ptr	protected_access_class;
			/* When non-NULL, the access check was for the special
			   protected member rule (11.5 in the C++98 and C++03
			   standards, 11.4 in the C++11 standard) and this is
			   the class of the object used to access the
			   member. */
  a_source_position
		position;
			/* Position to be used when the error is issued. */
  a_token_sequence_number
		token_sequence_number;
			/* Token sequence number of the current token when the
			   access error was first detected. */
  an_error_severity
		severity;
			/* The severity at which the error should be issued
			   or es_none to use a default value.  The severity
			   is only used if error_code is not ec_no_error. */
  an_error_code
		error_code;
			/* The error code for the error to be issued
			   or ec_no_error to use a default value. */
  a_byte_boolean
		in_template_arg_list;
			/* TRUE if the access occurred in the context of
			   a template argument list. */
  a_byte_boolean
		in_decltype_context;
			/* TRUE if the access occurred in the context of
			   a decltype(...) construct.  (Currently only
			   relevant in Microsoft mode.) */
} an_access_error_descr;


/* Contains a description of an exception specification error that has been
   detected and for which an error may need to be issued later. */
typedef struct an_exception_spec_error_descr {
  an_exception_spec_error_descr_ptr
		next;
			/* Pointer to the next error description record. */
  a_source_position
		position;
			/* Position to be used when the diagnostic is
			   issued. */
  an_error_code
		error_code;
			/* The code indicating the diagnostic message to be
			   issued. */
  a_type_ptr    assoc_type;
			/* When error_code is ec_incomplete_type_not_allowed,
			   this is a pointer to the associated incomplete
			   type. */
} an_exception_spec_error_descr;

#if CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
EXTERN_THREAD a_symbol_ptr
		last_ctor_or_dtor_sym;
			/* The last constructor or destructor
			   with a definition outside of a class
			   declaration.  Used only in cfront
			   compatibility mode to emulate a cfront
			   name lookup bug. */
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */


extern a_symbol_ptr find_symbol(a_const_char     *identifier,
			        sizeof_t         identifier_length,
				a_symbol_locator *location);

namespace detail {

extern a_hash_table_ptr create_name_lookup_table(a_scope_kind kind);

extern a_symbol_ptr find_symbol_list_in_non_null_table(
                                                a_hash_table_ptr    hash_table,
                                                a_symbol_header_ptr header);

}  /* namespace detail */

/* Forward declaration of skip_module_partitions (defined in modules.h). */
inline a_module_ptr skip_module_partitions(a_module_ptr mod);


inline a_hash_table_ptr curr_lookup_table(
                              a_scope_pointers_block_ptr pointers_block,
                              a_module_ptr               module_context,
                              a_scope_kind               scope_kind,
                              a_boolean                  create = FALSE)
/*
Return a pointer to the lookup table for the given module context and the scope
associated with pointers_block.  Note that this address could point into a
Ptr_map, so it is potentially not stable across things like instantiations,
etc.  When module_context is NULL the lookup_table pointer is used, otherwise a
map from the module context to a particular lookup table is used.  If create is
TRUE, a map entry is added for the lookup table for the module_context.
*/
{
  a_hash_table_ptr result = NULL;

  if (module_context == NULL || !is_file_or_namespace_scope_kind(scope_kind)) {
    result = pointers_block->lookup_table;
    if (result == NULL && create) {
      result = detail::create_name_lookup_table(scope_kind);
      pointers_block->lookup_table = result;
    }  /* if */
  } else {
    a_module_lookup_table_map_ptr
                &mltmp = pointers_block->module_lookup_table_map;

    module_context = skip_module_partitions(module_context);
    if (create) {
      if (mltmp == NULL) {
        mltmp = new_fe<a_module_lookup_table_map>(/*mask_width=*/10u);
      }  /* if */
      result = mltmp->get(module_context);
      if (result == NULL) {
        result = detail::create_name_lookup_table(scope_kind);
        mltmp->map(module_context, result);
      }  /* if */
    } else if (mltmp != NULL) {
      result = mltmp->get(module_context);
    }  /* if */
  }  /* if */
  return result;
}  /* curr_lookup_table */


inline a_hash_table_ptr curr_lookup_table(
                                        a_scope_stack_entry_ptr ssep,
                                        a_module_ptr            module_context)
/*
Return a pointer to the lookup table for the given module context and the
specified scope stack entry.  Note that this address could point into a
Ptr_map, so it is potentially not stable across things like instantiations,
etc.  The choice of the lookup_table pointer vs. the module map is based on the
scope kind.
*/
{
  a_scope_pointers_block_ptr pointers_block = assoc_pointers_block_of(ssep);

  return curr_lookup_table(pointers_block, module_context, ssep->kind);
}  /* curr_lookup_table */


inline a_symbol_ptr find_symbol_list_in_table(a_hash_table_ptr    hash_table,
                                              a_symbol_header_ptr header)
/*
Look up header in hash_table.  Return a pointer to the symbol list from the
hash table or NULL if no entry was found.
*/
{
  a_symbol_ptr result_sym = NULL;

  if (hash_table != NULL) {
    result_sym = detail::find_symbol_list_in_non_null_table(hash_table,
                                                            header);
  }  /* if */
  return result_sym;
}  /* find_symbol_list_in_table */


inline a_symbol_ptr find_symbol_list_in_table(
                                     a_scope_pointers_block_ptr pointers_block,
                                     a_symbol_header_ptr        header)
/*
Look up header in the lookup table of pointers_block.  Return a pointer to
the symbol list from the hash table or NULL if no entry was found.
*/
{
  a_symbol_ptr      result_sym = NULL;
  a_hash_table_ptr  hash_table = pointers_block->lookup_table;

  if (hash_table != NULL) {
    result_sym = find_symbol_list_in_table(hash_table, header);
  }  /* if */
  return result_sym;
}  /* find_symbol_list_in_table */


inline a_symbol_ptr find_symbol_list_in_table(
                                        a_scope_stack_entry_ptr ssep,
                                        a_module_ptr            module_context,
                                        a_symbol_header_ptr     header)
/*
Look up header in the lookup table specified by ssep and module_context.
Return a pointer to the symbol list from the hash table or NULL if no
entry was found.
*/
{
  a_symbol_ptr      result_sym = NULL;
  a_hash_table_ptr  hash_table;

  hash_table = curr_lookup_table(ssep, module_context);
  if (hash_table != NULL) {
    result_sym = find_symbol_list_in_table(hash_table, header);
  }  /* if */
  return result_sym;
}  /* find_symbol_list_in_table */


extern void add_symbol_to_scope_list(a_symbol_ptr  sym_ptr,
                                     a_scope_depth scope_depth,
                                     a_boolean     *err);

extern
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
                        a_boolean		 can_create_nonreal);

extern a_boolean looks_like_ctor_or_dtor(a_symbol_locator  *loc);

extern void make_locator_for_symbol(a_symbol_ptr     sym_ptr,
                                    a_symbol_locator *location);

extern void make_resolved_id_pseudo_token_locator(a_symbol_ptr     sym_ptr,
                                                  a_symbol_locator *location);

extern void make_specific_symbol_error_locator(a_symbol_locator *locator);

extern void make_error_locator(a_symbol_locator *loc);

extern void clear_qualifier_from_locator(a_symbol_locator  *locator);

extern a_symbol_ptr corresp_prototype_for_class_symbol(a_symbol_ptr sym);

extern a_symbol_ptr template_symbol_for_class_symbol(a_symbol_ptr class_sym);

#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
extern a_boolean entity_cannot_be_specialized(a_symbol_ptr  sym);
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

extern
a_template_cache_segment_ptr alloc_template_cache_segment(
                                a_symbol_ptr				sym,
                                a_template_symbol_supplement_ptr	tssp);

extern a_template_cache_segment_ptr get_template_cache_segment(
                                a_symbol_ptr                      sym,
                                a_template_symbol_supplement_ptr  tssp,
                                a_token_sequence_number           first_tsn,
                                a_token_sequence_number           last_tsn);

extern an_out_of_class_partial_spec_ptr alloc_out_of_class_partial_spec(void);

extern a_template_decl_info_ptr alloc_template_decl_info(void);

extern void free_template_decl_info(a_template_decl_info_ptr tdip);

extern a_nondependent_call_info_ptr get_nondependent_call_info(
                                a_token_sequence_number         tsn,
                                a_nondependent_call_depth       depth);

extern void check_for_nested_type_of_prototype_instantiation(a_symbol_ptr sym);

extern void record_nondependent_call(
                             a_symbol_ptr              symbol,
                             a_token_sequence_number   tsn,
                             a_nondependent_call_depth depth,
                             a_boolean                 supplemental = FALSE,
                             a_boolean                 reversed_opnds = FALSE);

extern a_templ_friend_info_ptr alloc_templ_friend_info(void);

extern void clear_template_cache(a_template_cache_ptr tcp);

extern void set_template_cache_info(a_template_cache_ptr     tcp,
                                    a_reusable_token_cache   tokens,
                                    a_template_decl_info_ptr tdip);

extern a_template_symbol_supplement_ptr alloc_template_symbol_supplement(
                                                         a_symbol_kind  kind);

extern a_symbol_ptr make_symbol(a_symbol_kind    sym_kind,
                                a_symbol_locator *location);

extern void add_symbol_to_symbol_table(a_symbol_ptr     sym_ptr,
                                       a_scope_depth    scope_depth,
                                       a_boolean        suppress_error);

extern a_symbol_ptr enter_symbol(a_symbol_kind    sym_kind,
				 a_symbol_locator *location,
                                 a_scope_depth    scope_depth,
                                 a_boolean        suppress_error);

extern void reenter_symbol(a_symbol_ptr     symbol_to_reenter,
                           a_scope_depth    scope_depth,
                           a_boolean        suppress_error);

extern void enter_symbol_into_completed_class(a_symbol_ptr  sym);

extern a_symbol_ptr enter_enumerator_into_completed_class(
                                                 a_symbol_locator  *loc,
                                                 a_type_ptr        class_type,
                                                 a_scope_number    scope_num);

extern a_symbol_ptr enter_copy_of_symbol(a_symbol_ptr     orig_sym,
                                         a_scope_depth    scope_depth,
                                         a_boolean        suppress_error);

extern a_symbol_ptr enter_extern_symbol(a_symbol_kind    sym_kind,
                                        a_symbol_locator *locator);

extern void reactivate_prototype_scope_symbols(
                                        a_symbol_ptr  prototype_scope_symbols);

extern void relink_unnamed_tag_symbol(a_symbol_ptr      sym,
                                      a_symbol_locator  *locator);

extern void enter_undefined_symbol(a_symbol_ptr sym);

extern a_symbol_ptr enter_undefined_member_symbol(a_symbol_locator *locator);

extern a_symbol_ptr make_namespace_projection_symbol(
                                              a_symbol_ptr       fund_sym,
                                              a_source_position  *pos,
                                              a_scope_depth      scope_depth);

extern void set_namespace_projection_symbol(a_symbol_ptr     proj_sym,
                                            a_symbol_ptr     fund_sym,
                                            a_scope_depth    scope_depth);

extern a_symbol_ptr enter_namespace_projection_symbol(
                                            a_symbol_ptr    fund_sym,
                                            a_boolean        is_using_decl,
                                            a_symbol_locator *location,
                                            a_scope_depth   scope_depth,
                                            a_boolean       suppress_error);

extern void add_friend_function_to_lookup_list_for_class(
                                                  a_symbol_ptr  rout_sym,
                                                  a_type_ptr    class_type);

extern
a_boolean is_symbol_from_inline_namespace_of_scope(a_symbol_ptr	sym,
						   a_scope_ptr	scope);

extern a_boolean is_symbol_from_inline_namespace_of_parent(
					a_symbol_ptr	ns_sym,
					a_symbol_ptr	sym);

extern a_boolean is_symbol_from_inline_namespace(a_symbol_ptr	sym);

extern a_symbol_ptr enter_synthesized_projection_symbol(
                               a_symbol_ptr		fund_sym,
                               a_symbol_locator		*location,
                               a_boolean		qualified_lookup,
                               a_namespace_ptr		qualifier_namespace,
                               an_id_lookup_options_set	options);

extern a_symbol_ptr add_symbol_to_overload_list(a_symbol_ptr    new_sym,
                                                a_symbol_ptr    other_sym,
                                                a_boolean	use_namespace,
                                                a_namespace_ptr ns_ptr);

extern a_symbol_ptr enter_overloaded_symbol(a_symbol_kind    sym_kind,
                                            a_symbol_locator *location,
                                            a_boolean        is_constructor,
                                            a_symbol_ptr     old_sym_ptr,
                                            a_symbol_ptr     *overload_sym);

extern void add_deduction_guide(a_symbol_ptr  new_guide,
                                a_symbol_ptr  *p_guide_set);

extern void remove_deduction_guide(a_symbol_ptr  new_guide,
                                   a_symbol_ptr  *p_guide_set);

extern a_type_ptr function_or_template_symbol_type(a_symbol_ptr sym);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_symbol_ptr enter_property_set_member(
                                     a_symbol_locator               *loc,
                                     a_scope_depth                  depth,
                                     a_property_or_event_descr_ptr  pedp,
                                     a_symbol_ptr                   *set_sym);

extern a_symbol_ptr enter_cli_accessor(a_symbol_locator               *locator,
                                       a_scope_depth                  depth,
                                       a_property_or_event_descr_ptr  pedp);

extern void enter_projected_default_indexed_properties(
                                         a_class_symbol_supplement_ptr  cssp);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void make_make_integer_seq_internal_template(void);

extern void make_type_pack_element_internal_template(void);

extern void make_builtin_common_type_internal_templates(void);

extern void make_builtin_dedup_pack_internal_template(void);

EXTERN_THREAD a_symbol_ptr
                symbol_for_make_integer_seq;
                        /* Symbol for "__make_integer_seq", which is a
                           builtin class template (used for cases where
                           template arguments to __make_integer_seq are
                           dependent). */

EXTERN_THREAD a_symbol_ptr
                symbol_for_make_integer_seq_alias;
                        /* Symbol for "__make_integer_seq_alias", which is a
                           builtin alias template (used for cases where
                           template arguments to __make_integer_seq are
                           non-dependent). */

EXTERN_THREAD a_symbol_ptr
                symbol_for_type_pack_element;
                        /* Symbol for "__type_pack_element", which is a
                           builtin class template (used for cases where
                           template arguments to __type_pack_element are
                           dependent). */

EXTERN_THREAD a_symbol_ptr
                symbol_for_type_pack_element_alias;
                        /* Symbol for "__type_pack_element_alias", which is a
                           builtin alias template (used for cases where
                           template arguments to __type_pack_element are
                           non-dependent). */

EXTERN_THREAD a_symbol_ptr
                symbol_for_builtin_common_type;
                        /* Symbol for "__builtin_common_type", which is a
                           builtin class template (used for cases where
                           template arguments to __builtin_common_type are
                           dependent). */

EXTERN_THREAD a_symbol_ptr
                symbol_for_builtin_common_type_alias;
                        /* Symbol for "__builtin_common_type_alias", which is
                           a builtin alias template (used for cases where
                           template arguments to __builtin_common_type are
                           non-dependent). */

EXTERN_THREAD a_symbol_ptr
                symbol_for_builtin_dedup_pack;
                        /* Symbol for "__builtin_dedup_pack", which is a
                           builtin class template, used to deduplicate a
                           template type argument list. */

extern void reenter_block_scope_symbol(a_symbol_ptr  sym);

extern a_base_class_ptr find_base_with_type(a_type_ptr        base_type,
                                            a_type_ptr        class_type,
                                            a_base_class_ptr  ref_bcp);

extern a_symbol_ptr make_projection_symbol(a_symbol_ptr      progenitor_sym,
                                           a_type_ptr        class_ptr,
                                           a_base_class_ptr  fundamental_bcp,
                                           a_derivation_step *path,
                                           a_boolean         ambiguous);

extern a_symbol_ptr make_function_template_prototype_symbol(
				a_symbol_ptr		template_sym,
				a_routine_ptr		rout_ptr,
				a_template_param_ptr	templ_param_list);

extern a_symbol_ptr make_template_variable_symbol(a_symbol_ptr  templ_sym);

extern a_symbol_ptr make_template_class_symbol(a_symbol_ptr  ct_symbol);

extern
a_symbol_ptr make_template_function_symbol(a_symbol_ptr       templ_sym,
                                           a_source_position  *pos,
                                           a_type_ptr         rout_type);

extern a_symbol_ptr error_class_template(void);

extern a_template_symbol_supplement_ptr template_supplement_for_template(
						a_template_ptr	templ_ptr);

extern a_symbol_ptr get_member_function_template_symbol(a_symbol_ptr rout_sym);

extern a_symbol_ptr make_template_param_object_sym(a_source_position  *pos);

extern a_symbol_ptr make_unentered_symbol(a_symbol_kind        sym_kind,
                                          a_symbol_header_ptr  header,
                                          a_source_position    *pos);

extern a_symbol_ptr make_unnamed_tag_symbol(a_symbol_kind      sym_kind,
                                            a_source_position  *pos);

extern a_boolean is_unnamed_tag_symbol(a_symbol_ptr  sym);

extern a_boolean is_unnamed_namespace_symbol(a_symbol_ptr  sym);

extern a_symbol_ptr make_unnamed_namespace_symbol(a_source_position  *pos);

extern
a_symbol_ptr make_unnamed_symbol(a_symbol_kind		kind,
				 a_source_position	*pos);

extern a_symbol_ptr make_module_symbol(const a_string    &primary_name,
                                       const a_string    &partition_name,
                                       a_boolean         is_interface,
                                       a_source_position *pos);

extern a_symbol_ptr get_unnamed_field_symbol(void);

extern a_symbol_ptr make_anonymous_parent_object_symbol(
                                                a_symbol_kind      kind,
                                                a_source_position  *pos,
                                                a_scope_number     decl_scope);

extern a_symbol_ptr full_enter_symbol(a_const_char  *identifier,
				      sizeof_t      identifier_length,
				      a_symbol_kind sym_kind,
				      a_scope_depth scope_depth);

extern void enter_keyword(a_token_kind token,
                          a_const_char *keyword);

extern void enter_builtin_keyword(a_token_kind token,
                                  a_const_char *keyword);

#if NAMED_ADDRESS_SPACES_ALLOWED
extern a_symbol_ptr enter_named_address_space(a_const_char  *name);
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */

#if NAMED_REGISTERS_ALLOWED
extern a_symbol_ptr enter_named_register(a_const_char  *name);
#endif /* NAMED_REGISTERS_ALLOWED */

extern void make_symbol_for_predeclared_type(a_type_ptr    predeclared_type,
                                             a_const_char  *name);

extern void enter_injected_class_name_symbol(a_symbol_ptr  tag_sym);

extern a_symbol_ptr enter_typedef_symbol(a_type_ptr       type_ptr,
                                         a_symbol_locator *locator,
                                         a_scope_depth    scope_level,
                                         a_boolean        suppress_error);

EXTERN_THREAD a_symbol_ptr
		symbol_for_namespace_std;
			/* Symbol for namespace "std", which is predeclared
			   by the front end (but not entered into the symbol
			   table until a source declaration is encountered).
			   C++ only. */
EXTERN_THREAD a_symbol_ptr
		symbol_for_namespace_std_meta;
			/* Symbol for namespace "std::meta", which is
			   predeclared by the front end in some modes (but
			   not entered into the symbol table until a source
			   declaration is encountered). */

extern void make_symbol_for_namespace_std(void);

extern void enter_symbol_for_namespace_std(a_symbol_locator  *locator);

extern void enter_symbol_for_namespace_std_meta(a_symbol_locator  *locator);

extern void init_alias_templ_intrinsic_descriptions(void);

extern void init_var_templ_intrinsic_descriptions(void);

extern void init_templ_type_member_intrinsic_descriptions(void);

#if IA64_ABI
EXTERN_THREAD a_symbol_ptr
		symbol_for_namespace_abi;
			/* Analogous, but for the namespace used in
			   the IA-64 ABI for the derived classes of
			   type_info. */

extern void make_symbol_for_namespace_abi(void);

extern void enter_symbol_for_namespace_abi(a_symbol_locator  *locator);
#endif /* IA64_ABI */

extern a_symbol_ptr look_up_name_string_in_std(a_const_char  *name);

extern a_boolean is_member_of_namespace(a_symbol_ptr  sym,
                                        a_symbol_ptr  ns_sym);

EXTERN_THREAD a_namespace_ptr
		namespace_for_coroutine_types;
			/* Namespace containing the various class templates
			   used with coroutines, such as "coroutine_traits".
			   NULL if not yet determined, otherwise points to
			   either the std or std::experimental namespace. */

extern void init_coroutine_descr(a_routine_ptr          rp,
                                 a_coroutine_descr_ptr  cdp);

EXTERN_THREAD a_symbol_ptr
		symbol_for_std_initializer_list;
			/* Symbol for "std::initializer_list", a class
			   template that should be defined in the standard
			   header <initializer_list>.  NULL until such a
			   template is encountered. */

#if MICROSOFT_EXTENSIONS_ALLOWED
/*
Enumerates the cli_symbols array.  The array is apportioned as follows:

1) The first section is indexable by the an_integer_kind enumerators from
   ik_char through ik_unsigned_long_long and contains the corresponding
   C++/CLI type for each of those kinds.  Later integer kinds (__int128,
   _BitInt) have no corresponding CLI type.
2) The second section is indexable by the a_float_kind enumeration and
   should contain the corresponding C++/CLI type for each float kind.
3) The remainder of the array is for other well-known C++/CLI types
   (e.g. System::Object, etc.)

Note that for each addition to this enumeration, a corresponding entry must
be added to the cli_symbol_names array.  The cli_symbol_names entry will be
used to look up and initialize the corresponding cli_symbol entry.  If the
symbol requires special initialization, leave the corresponding
cli_symbol_names entry set to NULL.
*/
enum a_cli_symbol_kind : a_byte {
  csk_none,
  csk_first,
  csk_first_namespace = csk_first,
  csk_cli_namespace = csk_first_namespace,
  csk_system_namespace,
  csk_system_collections_namespace,
  csk_system_collections_generic_namespace,
  csk_platform_details_namespace,
  csk_platform_metadata_namespace,
  csk_windows_namespace,
  csk_windows_foundation_namespace,
  csk_windows_foundation_metadata_namespace,
  csk_windows_foundation_collections_namespace,
  csk_first_type,
  csk_last_namespace = (int)csk_first_type - 1, /*lint !e488*/
  csk_first_integer = (int)csk_first_type, /*lint !e488*/
  csk_system_byte_sign_unspecified = (int)csk_first_integer, /*lint !e488*/
                                                /* ik_char */
  csk_system_sbyte,				/* ik_signed_char */
  csk_system_byte,				/* ik_unsigned_char */
  csk_system_int16,				/* ik_short */
  csk_system_uint16,				/* ik_unsigned_short */
  csk_system_int32,				/* ik_int */
  csk_system_uint32,				/* ik_unsigned_int */
  csk_system_int32_is_long,			/* ik_long */
  csk_system_uint32_is_long,			/* ik_unsigned_long */
  csk_system_int64,				/* ik_long_long */
  csk_system_uint64,				/* ik_unsigned_long_long */
  csk_last_integer = csk_system_uint64,
  csk_first_float,
  csk_system_single = csk_first_float,		/* fk_float */
  csk_system_double,				/* fk_double */
  csk_system_double_is_long,			/* fk_long_double */
  csk_last_float = csk_system_double_is_long,
  csk_system_boolean,				/* bool_type() */
  csk_system_char,				/* wchar_t_type() */
  csk_system_void,				/* void_type() */
  csk_system_object,
  csk_system_value_type,
  csk_system_enum,
  csk_system_type,
  csk_system_string,
  csk_system_delegate,
  csk_system_multicast_delegate,
  csk_system_idisposable,
  csk_system_array,
  csk_system_nullable,
  csk_system_runtime_argument_handle,
  csk_system_async_callback,
  csk_system_iasync_result,
  csk_system_attribute,
  csk_system_attribute_targets,
  csk_system_attribute_usage_attribute,
  csk_system_flags_attribute,
  csk_system_param_array_attribute,
  csk_system_obsolete_attribute,
  csk_system_collections_ienumerable,
  csk_system_collections_generic_ienumerable,
  csk_cli_array,
  csk_interior_ptr,
  csk_pin_ptr,
  csk_size_t,					/* targ_size_t_int_kind */
  csk_platform_details_guid,			/* const type_of_guid& */
  csk_platform_write_only_array,
  csk_platform_callback_context,
  csk_cppcx_box,
  csk_abi_hstring,
  csk_windows_foundation_event_registration_token,
  csk_windows_foundation_metadata_allow_multiple_attribute,
  csk_windows_foundation_metadata_deprecated_attribute,
  csk_last,
  csk_last_type = (int)csk_last - 1 /*lint !e488*/
};


EXTERN_THREAD a_symbol_ptr
		cli_symbols[(int)csk_last];
			/* Contains pointers to various well-known C++/CLI
			   symbols (e.g. System::Object, System::Int) and is
			   indexed by the a_cli_symbol_kind enumeration. See
			   the a_cli_symbol_kind enumeration for more
			   information. */

constexpr a_cli_symbol_kind cli_fallback_symbols[]
/*
The csk_none-terminated set of symbols to perform "dual-lookup" on
in C++/CLI mode.
*/
= {
  csk_cli_array,
  csk_interior_ptr,
  csk_pin_ptr,
  csk_none
};


constexpr a_cli_symbol_kind cppcx_fallback_symbols[]
/*
The csk_none-terminated set of symbols to perform "dual-lookup" on in
C++/CX mode.
*/
= {
  csk_system_sbyte,
  csk_system_byte,
  csk_system_int16,
  csk_system_uint16,
  csk_system_int32,
  csk_system_uint32,
  csk_system_int32_is_long,
  csk_system_uint32_is_long,
  csk_system_int64,
  csk_system_uint64,
  csk_system_single,
  csk_system_double,
  csk_system_double_is_long,
  csk_system_char,
  csk_cli_array,
  csk_platform_write_only_array,
  csk_cppcx_box,
  csk_none
};


/*
A bit set type to describe the initialization of predeclared C++/CLI and
C++/CX symbols.
*/
typedef a_byte a_cli_symbol_init_flag_set;

#define CISF_DEFAULT       ((a_cli_symbol_init_flag_set)0x00)
			/* The symbol is loaded on startup.  This implies
			   CISF_CLI_METADATA | CISF_PLATFORM_METADATA. */
#define CISF_OPTIONAL      ((a_cli_symbol_init_flag_set)0x01)
			/* The symbol is not required to be found. */
#define CISF_CLI_METADATA  ((a_cli_symbol_init_flag_set)0x02)
			/* For C++/CLI, the symbol is loaded on startup after
			   mscorlib.dll is loaded. */
#define CISF_PLATFORM_METADATA \
                           ((a_cli_symbol_init_flag_set)0x04)
			/* For C++/CX, the symbol is loaded on startup after
			   Platform.winmd is loaded. */
#define CISF_WINDOWS_METADATA \
                           ((a_cli_symbol_init_flag_set)0x08)
			/* For C++/CX, the symbol is loaded on startup after
			   Windows.winmd is loaded. */

/*
Structure representing a managed symbol that will be pre-created and stored in
cli_symbols.
*/
typedef struct {
  a_const_char	*name;	/* Unqualified name of the symbol. */
  a_cli_symbol_kind
                namespace_kind;
                        /* Enum value for the parent namespace for the symbol
                           or csk_none. */
  a_const_char  *cppcx_name;
                        /* Unqualified name of the symbol in C++/CX mode.
                           NULL if the name does not differ from the C++/CLI
                           name.*/
  a_cli_symbol_kind
                cppcx_namespace_kind;
                        /* Enum value for the parent namespace for the symbol
                           in C++/CX mode.  csk_none if the parent
                           namespace does not differ from that of C++/CLI. */
  a_cli_symbol_init_flag_set
                init_flags;
                        /* Init flags required by the symbol. */
} a_cli_symbol_name;

EXTERN_CONSTINIT_ARRAY(a_cli_symbol_name, cli_symbol_names, csk_last + 1)
			/* Array of symbol names corresponding to each entry
			   in a_cli_symbol_kind, respectively.  See
			   a_cli_symbol_kind for more information. */
#if VAR_INITIALIZERS
= {
  { NULL, csk_none, NULL, csk_none, CISF_DEFAULT },
			/* csk_none */
  { NULL, csk_none, "default", csk_none, CISF_DEFAULT },
			/* csk_cli_namespace */
  { "System", csk_none, "Platform", csk_none, CISF_DEFAULT },
			/* csk_system_namespace */
  { "Collections", csk_system_namespace,
    NULL, csk_none, CISF_CLI_METADATA },
			/* csk_system_collections_namespace */
  { "Generic", csk_system_collections_namespace,
    NULL, csk_none, CISF_CLI_METADATA },
			/* csk_system_collections_generic_namespace */
  { NULL, csk_none, "Details", csk_system_namespace, CISF_DEFAULT },
			/* csk_platform_details_namespace */
  { NULL, csk_none, "Metadata", csk_system_namespace, CISF_DEFAULT },
			/* csk_platform_metadata_namespace */
  { NULL, csk_none, "Windows", csk_none, CISF_DEFAULT },
			/* csk_windows_namespace */
  { NULL, csk_none, "Foundation", csk_windows_namespace, CISF_DEFAULT },
			/* csk_windows_foundation_namespace */
  { NULL, csk_none,
    "Metadata", csk_windows_foundation_namespace, CISF_DEFAULT },
			/* csk_windows_foundation_metadata_namespace */
  { NULL, csk_none, "Collections", csk_windows_foundation_namespace,
   (CISF_OPTIONAL | CISF_WINDOWS_METADATA) },
			/* csk_windows_foundation_collections_namespace */
  { NULL, csk_none, NULL, csk_none, CISF_DEFAULT },
			/* csk_system_byte_sign_unspecified */
  { "SByte", csk_system_namespace, "int8", csk_cli_namespace, CISF_DEFAULT },
			/* csk_system_sbyte */
  { "Byte", csk_system_namespace, "uint8", csk_cli_namespace, CISF_DEFAULT },
			/* csk_system_byte */
  { "Int16", csk_system_namespace, "int16", csk_cli_namespace, CISF_DEFAULT  },
			/* csk_system_int16 */
  { "UInt16", csk_system_namespace,
    "uint16", csk_cli_namespace, CISF_DEFAULT },
			/* csk_system_uint16 */
  { "Int32", csk_system_namespace, "int32", csk_cli_namespace, CISF_DEFAULT },
			/* csk_system_int32 */
  { "UInt32", csk_system_namespace,
    "uint32", csk_cli_namespace, CISF_DEFAULT },
			/* csk_system_uint32 */
  { "Int32", csk_system_namespace, "int32", csk_cli_namespace, CISF_DEFAULT },
			/* csk_system_int32_is_long */
  { "UInt32", csk_system_namespace,
    "uint32", csk_cli_namespace, CISF_DEFAULT },
			/* csk_system_uint32_is_long */
  { "Int64", csk_system_namespace, "int64", csk_cli_namespace, CISF_DEFAULT },
			/* csk_system_int64 */
  { "UInt64", csk_system_namespace,
    "uint64", csk_cli_namespace, CISF_DEFAULT },
			/* csk_system_uint64 */
  { "Single", csk_system_namespace,
    "float32", csk_cli_namespace, CISF_DEFAULT },
			/* csk_system_single */
  { "Double", csk_system_namespace,
    "float64", csk_cli_namespace, CISF_DEFAULT },
			/* csk_system_double */
  { "Double", csk_system_namespace,
    "float64", csk_cli_namespace, CISF_DEFAULT },
			/* csk_system_double_is_long */
  { "Boolean", csk_system_namespace, NULL, csk_none, CISF_DEFAULT },
			/* csk_system_boolean */
  { "Char", csk_system_namespace, "char16", csk_cli_namespace, CISF_DEFAULT },
			/* csk_system_char */
  { "Void", csk_system_namespace, NULL, csk_none, CISF_CLI_METADATA },
			/* csk_system_void */
  { "Object", csk_system_namespace, NULL, csk_none, CISF_DEFAULT },
			/* csk_system_object */
  { "ValueType", csk_system_namespace, NULL, csk_none, CISF_DEFAULT },
			/* csk_system_value_type */
  { "Enum", csk_system_namespace, NULL, csk_none, CISF_DEFAULT },
			/* csk_system_enum */
  { "Type", csk_system_namespace, NULL, csk_none, CISF_DEFAULT },
			/* csk_system_type */
  { "String", csk_system_namespace, NULL, csk_none, CISF_DEFAULT },
			/* csk_system_string */
  { "Delegate", csk_system_namespace, NULL, csk_none, CISF_DEFAULT },
			/* csk_system_delegate */
  { "MulticastDelegate", csk_system_namespace,
    NULL, csk_none, CISF_CLI_METADATA },
			/* csk_system_multicast_delegate */
  { "IDisposable", csk_system_namespace, NULL, csk_none, CISF_DEFAULT },
			/* csk_system_idisposable */
  { "Array", csk_system_namespace, NULL, csk_none, CISF_CLI_METADATA },
			/* csk_system_array */
  { "Nullable", csk_system_namespace, NULL, csk_none, CISF_CLI_METADATA },
			/* csk_system_nullable */
  { "RuntimeArgumentHandle", csk_system_namespace,
    NULL, csk_none, CISF_CLI_METADATA },
			/* csk_system_runtime_argument_handle */
  { "AsyncCallback", csk_system_namespace,
    NULL, csk_none, CISF_CLI_METADATA },
			/* csk_system_async_callback */
  { "IAsyncResult", csk_system_namespace,
    NULL, csk_none, CISF_CLI_METADATA },
			/* csk_system_iasync_result */
  { "Attribute", csk_system_namespace,
    NULL, csk_platform_metadata_namespace, CISF_DEFAULT },
			/* csk_system_attribute */
  { "AttributeTargets", csk_system_namespace,
    NULL, csk_windows_foundation_metadata_namespace,
    (CISF_CLI_METADATA | CISF_WINDOWS_METADATA) },
			/* csk_system_attribute_targets */
  { "AttributeUsageAttribute", csk_system_namespace,
    NULL, csk_windows_foundation_metadata_namespace,
    (CISF_CLI_METADATA | CISF_WINDOWS_METADATA) },
			/* csk_system_attribute_usage_attribute */
  { "FlagsAttribute", csk_system_namespace,
    NULL, csk_platform_metadata_namespace, CISF_DEFAULT },
			/* csk_system_flags_attribute */
  { "ParamArrayAttribute", csk_system_namespace,
    NULL, csk_none, CISF_CLI_METADATA },
			/* csk_system_param_array_attribute */
  { "ObsoleteAttribute", csk_system_namespace,
    NULL, csk_none, CISF_CLI_METADATA },
			/* csk_system_obsolete_attribute */
  { "IEnumerable", csk_system_collections_namespace,
    NULL, csk_none, CISF_CLI_METADATA },
			/* csk_system_collections_ienumerable */
  { "IEnumerable", csk_system_collections_generic_namespace,
    NULL, csk_none, CISF_CLI_METADATA },
			/* csk_system_collections_generic_ienumerable */
  { NULL, csk_none, NULL, csk_none, CISF_DEFAULT },
			/* csk_cli_array */
  { NULL, csk_none, NULL, csk_none, CISF_DEFAULT },
			/* csk_interior_ptr */
  { NULL, csk_none, NULL, csk_none, CISF_DEFAULT },
			/* csk_pin_ptr */
  { NULL, csk_none, "SizeT", csk_system_namespace, CISF_DEFAULT },
			/* csk_size_t */
  { NULL, csk_none, "_GUID", csk_platform_details_namespace, CISF_DEFAULT },
			/* csk_platform_details_guid */
  { NULL, csk_none, NULL, csk_none, CISF_DEFAULT },
			/* csk_platform_write_only_array */
  { NULL, csk_none, "CallbackContext", csk_system_namespace, CISF_DEFAULT },
			/* csk_platform_callback_context */
  { NULL, csk_none, NULL, csk_none, CISF_DEFAULT },
			/* csk_cppcx_box */
  { NULL, csk_none, "HSTRING__", csk_none, CISF_WINDOWS_METADATA },
			/* csk_abi_hstring */
  { NULL, csk_none, "EventRegistrationToken", csk_windows_foundation_namespace,
    CISF_WINDOWS_METADATA },
			/* csk_windows_foundation_event_registration_token */
  { NULL, csk_none,
    "AllowMultipleAttribute", csk_windows_foundation_metadata_namespace,
    CISF_WINDOWS_METADATA },
		/* csk_windows_foundation_metadata_allow_multiple_attribute */
  { NULL, csk_none,
    "DeprecatedAttribute", csk_windows_foundation_metadata_namespace,
    CISF_WINDOWS_METADATA },
		/* csk_windows_foundation_metadata_deprecated_attribute */
  { "last", csk_none, NULL, csk_none, CISF_DEFAULT },
			/* csk_last */
}
#endif /* VAR_INITIALIZERS */
EXTERN_CONSTINIT_ARRAY_END(cli_symbol_names)

#define cli_symbol_is_required(csk)                                          \
  ((cli_symbol_names[(int)(csk)].init_flags & CISF_OPTIONAL) == 0)

inline a_cli_symbol_kind integer_kind_to_cli_symbol_kind(an_integer_kind  ik)
/*
Return the CLI symbol kind corresponding to the integer kind ik, or csk_none
if there is no corresponding C++/CLI type.  Only the kinds from ik_char
through ik_unsigned_long_long have System counterparts.
*/
{
  a_cli_symbol_kind csk = csk_none;

  if ((int)ik <= (int)ik_unsigned_long_long) {
    csk = (a_cli_symbol_kind)((int)csk_first_integer + (int)ik);
  }  /* if */
  return csk;
}  /* integer_kind_to_cli_symbol_kind */


inline a_cli_symbol_kind float_kind_to_cli_symbol_kind(a_float_kind  fk)
/*
Return the CLI symbol kind corresponding to the float kind fk.
*/
{
  a_cli_symbol_kind csk = csk_system_double_is_long;

  switch (fk) {
    case fk_float:
      csk = csk_system_single;
      break;
    case fk_float32x:
    case fk_double:
      csk = csk_system_double;
      break;
    case fk_float64x:
    case fk_long_double:
      csk = csk_system_double_is_long;
      break;
    default:
      unexpected_condition();
  }  /* switch */
  return csk;
}  /* float_kind_to_cli_symbol_kind */

/*
Macros to return a cli_symbols entry given one of
a_cli_symbol_kind/an_integer_kind/a_float_kind respectively.
*/
#define cli_symbol_from_kind(csk) (cli_symbols[(int)(csk)])
#define cli_symbol_from_integer_kind(ik)                              \
  (cli_symbol_from_kind(integer_kind_to_cli_symbol_kind((ik))))
#define cli_symbol_from_float_kind(fk)                                \
  (cli_symbol_from_kind(float_kind_to_cli_symbol_kind((fk))))

extern a_type_ptr f_cli_class_type_for(a_cli_symbol_kind kind);
extern a_symbol_ptr f_cli_symbol_from_kind_or_null(a_cli_symbol_kind kind);
extern a_boolean is_cli_cx_pseudo_template(a_symbol_ptr	template_sym);

#define cli_class_type_for(csk)                                              \
  (f_cli_class_type_for((a_cli_symbol_kind)(csk)))

#define cli_symbol_from_kind_or_null(csk)                                    \
  (f_cli_symbol_from_kind_or_null((a_cli_symbol_kind)(csk)))

/*
Macros to retrieve special C++/CLI types.
*/
#define cli_system_object_type()                                             \
  (cli_class_type_for((a_cli_symbol_kind)csk_system_object))
#define cli_system_value_type()                                              \
  (cli_class_type_for((a_cli_symbol_kind)csk_system_value_type))

/*
Return the arity of a symbol that points to an sk_class_template symbol
for a C++/CLI generic.
*/
#define arity_for_generic(sym)						\
  ((sym)->variant.template_info->variant.class_template.arity)


extern void make_symbol_for_namespace_cli(void);
extern void init_cli_symbols(void);
extern a_boolean is_generic_cli_ienumerable_type(a_type_ptr type,
                                                 a_type_ptr elem_type);

extern void make_symbol_for_cppcx_box(void);
extern void make_symbol_for_abi_hstring(void);
extern void init_windows_metadata_symbols(void);

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

EXTERN_THREAD a_type_ptr
		builtin_va_list_type;
			/* When the <stdarg.h> header is handled as a builtin,
			   this points to the va_list type once it has been
			   defined.  NULL until then. */

EXTERN_THREAD a_type_ptr
		type_underlying_va_list;
			/* This is a configurable type (NULL by default; it
			   can conveniently be set in sys_predef.c).  When the
			   <stdarg.h> header is handled as a builtin, this is
			   the type underlying the generated va_list type.
			   Otherwise, if GNU builtin <stdarg.h> operators are
			   supported (see GCC_BUILTIN_VARARGS), it is the
			   underlying type of __builtin_va_list.  If this type
			   is NULL, a default type (usual "void*") is used. */

EXTERN_THREAD a_symbol_ptr
		symbols_with_no_scope;
			/* A list of symbols that were entered into the
			   symbol table, but are not associated with any
			   scope (and so, are not on a scope list).  This
			   includes things such as predefined macros and
			   keywords.  The next_in_scope field is used to
			   link these symbols together. */

EXTERN_THREAD a_boolean
		file_scope_symbols_are_on_inactive_list;
			/* TRUE once the file scope has been popped for the
			   first time, and any file scope symbols have been
			   moved to the inactive list. */

void declare_builtin_va_list_type(a_boolean	is_cstdarg);

extern void set_symbol_kind(a_symbol_ptr  sym_ptr,
                            a_symbol_kind sym_kind);

extern void unlink_symbol_from_symbol_table(a_symbol_ptr sym_ptr);

extern a_symbol_ptr alloc_symbol(a_symbol_kind       kind,
                                 a_symbol_header_ptr hdr_ptr,
                                 a_source_position   *position);

extern a_symbol_ptr make_dummy_undefined_symbol(a_symbol_header_ptr hdr_ptr,
                                                a_source_position   *position);

extern void remove_symbol_from_overload_set(a_symbol_ptr  sym,
                                            a_symbol_ptr  ovl_set);

extern void remove_symbol(a_symbol_ptr sym_ptr);

extern void remove_symbol_from_lookup_table(
				a_symbol_ptr        symbol,
				a_hash_table_ptr    lookup_table);

extern void remove_anonymous_union_member_from_inactive_symbols_list
                                                       (a_symbol_ptr sym_ptr);

extern void add_symbol_to_inactive_list(a_symbol_ptr sym_ptr);

extern a_symbol_ptr f_find_external_symbol(a_symbol_locator     *location,
                                           a_name_linkage_kind  linkage,
                                           a_type_ptr           type,
                                           a_requires_clause    *trcp,
                                           a_boolean            c_overload,
                                           a_symbol_locator     *ext_location);

#define find_external_symbol(loc, linkage, type, trcp, ext_loc)               \
  (f_find_external_symbol(loc, linkage, type, trcp, /*c_overload=*/FALSE,     \
                          ext_loc))

extern void change_to_destructor_or_finalizer_locator(
                                                 a_symbol_locator  *locator,
                                                 a_boolean         finalizer);

#define tildize_locator(loc)                                                 \
  (change_to_destructor_or_finalizer_locator((loc), /*finalizer=*/FALSE))

extern a_boolean destructor_name_matches_class_name(a_symbol_ptr class_sym);

extern void change_class_locator_into_constructor_locator(
                                            a_symbol_locator   *locator,
                                            a_source_position  *pos,
                                            a_boolean          is_static_ctor);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern void change_ms_attr_locator_into_alt_name_locator(
                                                  a_symbol_locator  *locator);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void make_opname_locator(an_opname_kind    opname,
                                a_symbol_locator  *locator,
                                a_source_position *pos);

extern void make_literal_opname_locator(a_const_char      *ud_suffix,
                                        sizeof_t          ud_suffix_len,
                                        a_symbol_locator  *locator,
                                        a_source_position *pos);

extern void make_struct_binding_container_locator(a_symbol_locator  *locator,
                                                  a_source_position *pos);

extern void make_type_conversion_locator(a_type_ptr         type,
                                         a_symbol_locator   *locator,
                                         a_source_position  *pos);

extern a_symbol_ptr find_default_operator_new_sym(a_symbol_ptr sym,
                                                  a_boolean    *ambiguous);

extern a_boolean is_default_operator_delete(
                                          a_routine_ptr routine,
                                          a_boolean     *is_sized_ver,
                                          a_boolean     *is_aligned_delete,
                                          a_boolean     *is_destroying_delete);

extern a_symbol_ptr find_default_operator_delete_sym(a_symbol_ptr sym,
                                                     a_type_ptr   delete_type,
                                                     a_boolean    *ambiguous);

extern a_symbol_ptr find_corresponding_operator_delete_sym(
                                                  a_symbol_ptr op_new_sym,
                                                  a_type_ptr   class_type,
                                                  a_type_ptr   delete_type,
                                                  a_boolean    placement_new,
                                                  a_boolean    template_okay,
                                                  a_boolean    *ambiguous,
                                                  a_symbol_ptr *overload_sym);

extern a_symbol_ptr make_predeclared_function_symbol(
                                              a_symbol_locator  *locator,
                                              a_type_ptr        rout_type);

extern void make_global_operator_new_or_delete_symbol(
                                              an_opname_kind  opname,
                                              a_boolean       sized_version,
                                              a_boolean       aligned_version);

#if MICROSOFT_EXTENSIONS_ALLOWED

extern void make_predeclared_alloca_symbol(void);

EXTERN_THREAD a_symbol_ptr
		predeclared_size_t_symbol;
			/* Symbol for predeclared "size_t", in microsoft
			   mode.*/

extern void make_predeclared_size_t_symbol(void);

extern void make_predeclared_bool_symbol(void);

extern void make_predeclared_nullptr_t_symbol(void);

extern a_symbol_ptr make_cppcli_unresolved_type_symbol(
                                                    a_constant_ptr  name_con);

extern a_boolean treat_as_cli_class_for_lookup(a_type_ptr	type);

extern
a_boolean use_hide_by_sig_lookup(
			a_symbol_ptr			sym,
			a_hide_by_sig_list_entry_ptr	*p_hide_by_sig_list);

#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
/*lint -emacro(506,treat_as_cli_class_for_lookup)*/
#define treat_as_cli_class_for_lookup(tp) FALSE
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void add_on_diag_for_skipped_inaccessible_function(
                                                   a_symbol_ptr sym,
                                                   a_diagnostic_ptr dp);

extern a_routine_ptr select_default_constructor_full(
                                         a_type_ptr        class_type,
                                         a_source_position *err_pos,
                                         a_type_ptr        object_class_type,
                                         a_boolean         declarative_context,
                                         a_boolean         evaluated,
                                         a_boolean         check_access,
                                         a_boolean         no_explicit,
                                         a_boolean         *error_detected,
                                         a_boolean         *err);

extern a_routine_ptr select_default_constructor
					(a_type_ptr        class_type,
                                         a_source_position *err_pos,
					 a_type_ptr	   object_class_type,
                                         a_boolean         *err);

extern a_routine_ptr select_destructor_full(
                                     a_type_ptr        class_type,
                                     a_type_ptr        object_class_type,
                                     a_source_position *position,
                                     a_boolean         honor_virtual,
                                     a_boolean         evaluated,
                                     a_boolean         instantiate,
                                     a_boolean         check_access,
                                     a_boolean         *error_detected);

extern a_routine_ptr select_destructor(a_type_ptr        class_type,
				       a_type_ptr        object_class_type,
                                       a_source_position *position);

extern a_routine_ptr select_copy_constructor_full(
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
                                  a_boolean             *error_detected);

extern a_routine_ptr select_copy_constructor(
                                  a_type_ptr            class_type,
                                  a_type_qualifier_set  required_qualifiers,
                                  a_boolean             source_is_rvalue,
                                  a_source_position     *err_pos,
                                  a_type_ptr            object_class_type,
                                  a_boolean             *class_bitwise_copy,
                                  a_boolean             allow_suppressed_ctor);

extern char *il_entry_for_symbol_null_okay(a_symbol_ptr      sym,
                                           an_il_entry_kind  *kind);

extern char *il_entry_for_symbol(a_symbol_ptr      sym,
                                 an_il_entry_kind  *kind);

template<typename an_Il_type>
inline an_Il_type *il_entry_for_symbol(a_symbol_ptr sym) DELETED_FN_DEF

#if RECORD_MACROS_IN_IL

template<>
inline a_macro *il_entry_for_symbol(a_symbol_ptr sym)
/*
Given a symbol known to reference a macro def, return the macro def.
*/
{
  check_assertion(sym->kind == sk_macro);
  return sym->variant.macro_def->macro;
}  /* il_entry_for_symbol */

#endif /* RECORD_MACROS_IN_IL */

template<>
inline a_constant *il_entry_for_symbol(a_symbol_ptr sym)
/*
Given a symbol known to reference a constant, return the constant.
*/
{
  check_assertion(sym->kind == sk_constant);
  return sym->variant.constant;
}  /* il_entry_for_symbol */


template<>
inline a_type *il_entry_for_symbol(a_symbol_ptr sym)
/*
Given a symbol known to reference a type, return the type.
*/
{
  a_type_ptr result = NULL;

  switch (sym->kind) {
    case sk_type:
      result = sym->variant.type.ptr;
      break;
    case sk_enum_tag:
      result = sym->variant.enumeration.type;
      break;
    case sk_class_or_struct_tag:
    case sk_union_tag:
      result = sym->variant.class_struct_union.type;
      break;
    default:
      unexpected_condition();
  }  /* switch */
  return result;
}  /* il_entry_for_symbol */


template<>
inline a_variable *il_entry_for_symbol(a_symbol_ptr sym)
/*
Given a symbol known to reference a variable, return the variable.
*/
{
  a_variable_ptr result = NULL;

  switch (sym->kind) {
    case sk_variable:
      result = sym->variant.variable.ptr;
      break;
    case sk_static_data_member:
      result = sym->variant.static_data_member.variable;
      break;
    default:
      unexpected_condition();
  }  /* switch */
  return result;
}  /* il_entry_for_symbol */


template<>
inline a_field *il_entry_for_symbol(a_symbol_ptr sym)
/*
Given a symbol known to reference a field, return the field.
*/
{
  check_assertion(sym->kind == sk_field);
  return sym->variant.field.ptr;
}  /* il_entry_for_symbol */


template<>
inline a_routine *il_entry_for_symbol(a_symbol_ptr sym)
/*
Given a symbol known to reference a routine, return the routine.
*/
{
  a_routine_ptr result = NULL;

  switch (sym->kind) {
    case sk_routine:
    case sk_member_function:
      result = sym->variant.routine.ptr;
      break;
    default:
      unexpected_condition();
  }  /* switch */
  return result;
}  /* il_entry_for_symbol */


template<>
inline a_label *il_entry_for_symbol(a_symbol_ptr sym)
/*
Given a symbol known to reference a label, return the label.
*/
{
  check_assertion(sym->kind == sk_label);
  return sym->variant.label.ptr;
}  /* il_entry_for_symbol */


template<>
inline a_namespace *il_entry_for_symbol(a_symbol_ptr sym)
/*
Given a symbol known to reference a namespace, return the namespace.
*/
{
  check_assertion(sym->kind == sk_namespace);
  return sym->variant.namespace_info.ptr;
}  /* il_entry_for_symbol */


template<>
inline a_template *il_entry_for_symbol(a_symbol_ptr sym)
/*
Given a symbol known to reference a template, return the template.
*/
{
  a_template_ptr result = NULL;

  switch (sym->kind) {
    case sk_function_template:
    case sk_class_template:
    case sk_variable_template:
    case sk_concept_template:
      result = sym->variant.template_info->il_template_entry;
      break;
    default:
      unexpected_condition();
  }  /* switch */
  return result;
}  /* il_entry_for_symbol */


extern a_source_correspondence *source_corresp_entry_for_symbol(
                                                         a_symbol_ptr sym_ptr);

extern a_module *module_for_symbol(a_symbol_ptr sym_ptr);

extern a_module *lookup_module_for_symbol(a_symbol_ptr sym_ptr);

extern a_boolean is_symbol_globally_visible(a_symbol_ptr sym_ptr);

extern a_boolean is_symbol_currently_lookup_visible(a_symbol_ptr sym_ptr);

extern an_access_specifier compute_access(an_access_specifier access,
                                          an_access_specifier class_access);

extern an_access_specifier access_to_end_of_path
                                      (an_access_specifier         sym_access,
                                       a_derivation_step_ptr       path,
                                       a_base_class_derivation_ptr bcdp);

extern an_access_specifier access_for_symbol(a_symbol_ptr sym_ptr);

extern an_access_specifier effective_access_of_member_in_class(
                                                a_symbol_ptr member_sym,
                                                a_type_ptr   naming_class);

extern a_boolean have_member_access_privilege(a_type_ptr class_type);

extern a_boolean have_protected_member_access_privilege(a_type_ptr class_type);

extern a_boolean have_protected_access_from_derived_class(
                                                 a_type_ptr class_type,
                                                 a_type_ptr derived_class);

extern a_boolean have_access_to_symbol_full(a_symbol_ptr symbol,
                                            a_boolean    ignore_func_templ);

#define have_access_to_symbol(sym)                                            \
  (have_access_to_symbol_full(sym, /*ignore_func_templ=*/TRUE))

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_boolean have_hide_by_sig_access_to_symbol(a_symbol_ptr symbol);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void f_check_ambiguity_and_verify_access
                                (a_symbol_locator *loc,
                                 a_boolean        is_templ_context,
                                 a_boolean        is_qualifier,
                                 a_boolean        *error_detected);

extern void perform_deferred_access_checks_at_depth(a_scope_depth	depth);

extern void perform_deferred_access_checks_for_function(a_routine_ptr rp);

extern void f_discard_deferred_access_checks(a_scope_depth	depth);

extern void discard_declarator_access_errors(void);

extern void overload_check_ambiguity_and_verify_access(
                                           a_symbol_locator *locator,
                                           a_symbol_ptr     overloaded_symbol,
                                           a_boolean        *error_detected);

/*
Check to see if a symbol found is ambiguous or inaccessible.  There
are two kinds of ambiguity: ambiguity caused by inheritance and
ambiguity caused by using directives.  Inheritance ambiguity
checking precedes access control (ARM, 10.1.1).  Call a subroutine
to do further checking if the ambiguous flag is set, or for class
members in C++ (so that access checking can be done).  This macro
does nothing when called in C mode.
*/
#define check_ambiguity_and_verify_access(locator)                    \
{ if (C_dialect == C_dialect_cplusplus &&                             \
      (locator)->specific_symbol != NULL &&                           \
      ((locator)->specific_symbol->is_class_member ||                 \
       (locator)->specific_symbol->ambiguous)) {                      \
    f_check_ambiguity_and_verify_access(locator,		      \
                                        /*is_template_context=*/FALSE, \
                                        /*is_qualifier=*/FALSE,       \
                                        (a_boolean *)NULL);           \
  }  /* if */                                                         \
}  /* check_ambiguity_and_verify_access */


/*
Similar to check_ambiguity_and_verify_access, except with full parameters.
*/
#define check_ambiguity_and_access_full(locator, templ_context, is_qualifier,\
                                        error_detected) \
{ if (C_dialect == C_dialect_cplusplus &&                             \
      (locator)->specific_symbol != NULL &&                           \
      ((locator)->specific_symbol->is_class_member ||                 \
       (locator)->specific_symbol->ambiguous)) {                      \
    f_check_ambiguity_and_verify_access(locator, templ_context,	      \
                                        is_qualifier, error_detected); \
  }  /* if */                                                         \
}  /* check_ambiguity_and_access_full */

a_boolean f_check_for_ambiguity(a_symbol_locator *locator,
                                a_boolean        is_templ_context,
                                a_boolean        is_qualifier,
                                a_boolean        diagnostic_should_be_issued);

/*
Check to see if a symbol found is ambiguous.  If so, call a routine to
report the error.  The locator is set to an error locator if there is
an error.
*/
#define check_for_ambiguity(locator)					\
{ if ((locator)->specific_symbol != NULL &&				\
      (locator)->specific_symbol->ambiguous) {				\
    (void)f_check_for_ambiguity(locator, /*is_template_id=*/FALSE,      \
                                /*is_qualifier=*/FALSE,                 \
                                /*diagnostic_should_be_issued=*/TRUE);  \
  }  /* if */                                                         	\
}  /* check_for_ambiguity */

/*
Macro that returns TRUE if there are any deferred access checks to be
processed.
*/
#define any_deferred_access_checks()					\
  (curr_deferred_access_scope != NO_SCOPE_DEPTH &&			\
   scope_stack[curr_deferred_access_scope].deferred_access_checks != NULL)

/*
Set the flag that specifies that access errors should be deferred and
rechecked later.
*/
#define begin_deferral_of_access_checks()				\
{									\
  if (C_dialect == C_dialect_cplusplus) {				\
    check_assertion(curr_deferred_access_scope != NO_SCOPE_DEPTH);	\
    scope_stack[curr_deferred_access_scope].defer_access_checks = TRUE; \
  }  /* if */								\
}

/*
Perform deferred access checks for the current deferred access scope.
*/
#define perform_deferred_access_checks()				\
  (perform_deferred_access_checks_at_depth(curr_deferred_access_scope))

/*
Clear the flag that specifies that access errors should be deferred.
*/
#define end_deferral_of_access_checks()					\
{									\
  if (C_dialect == C_dialect_cplusplus) {				\
    check_assertion(curr_deferred_access_scope != NO_SCOPE_DEPTH);	\
    scope_stack[curr_deferred_access_scope].defer_access_checks = FALSE;  \
    if (scope_stack[curr_deferred_access_scope].			\
                                           deferred_access_checks != NULL) { \
      /* Only make this call if there are entries on the list. */	\
      perform_deferred_access_checks();					\
    }  /* if */								\
  }  /* if */								\
}

/*
Throw away any deferred access entries.
*/
#define discard_deferred_access_checks()				\
{									\
  check_assertion(curr_deferred_access_scope != NO_SCOPE_DEPTH);	\
  if (scope_stack[curr_deferred_access_scope].			\
                                           deferred_access_checks != NULL) { \
    /* Only make this call if there are entries on the list. */	       	\
    f_discard_deferred_access_checks(curr_deferred_access_scope);	\
  }  /* if */								\
}

extern void record_access_error(a_symbol_ptr            sym,
                                a_symbol_ptr            overload_sym,
                                a_type_ptr              protected_access_class,
                                a_source_position       *source_position,
                                a_symbol_locator        *locator,
				an_error_severity	severity,
				an_error_code		error_code,
                                a_boolean               *error_detected);


extern a_boolean check_protected_member_access(
                                            a_symbol_ptr      sym,
                                            a_symbol_ptr      proj_sym,
                                            a_source_position *err_pos,
                                            a_type_ptr        access_class,
                                            a_boolean         *error_detected);

/*
If a symbol has a corresponding nonreal type, return the symbol for that type,
otherwise return the original symbol.
*/
#define nonreal_type_if_nested_prototype_type(sym)			\
  ((sym)->corresp_nonreal_or_nested_type != NULL &&			\
   !(sym)->is_nonreal_nested_type					\
               ? f_nonreal_type_if_nested_prototype_type(sym)		\
               : sym)

/*
If a symbol is a nonreal type with a corresponding nested type, return the
nested type, otherwise return the original symbol.
*/
#define nested_prototype_type_for_nonreal_type(sym)			\
  (((sym)->corresp_nonreal_or_nested_type != NULL &&			\
    (sym)->is_nonreal_nested_type)					\
               ? (sym)->corresp_nonreal_or_nested_type			\
               : sym)

/*
If symbol is a projection symbol, change it to the fundamental symbol pointed
to by the projection.
*/
#define reduce_projection_symbol_to_fundamental_symbol(symbol)        \
{ if ((symbol)->kind == sk_projection) {                              \
    (symbol) = (symbol)->variant.projection.extra_info->fundamental_symbol;\
  }  /* if */                                                         \
  if ((symbol)->kind == sk_namespace_projection) {                    \
    (symbol) = (symbol)->variant.namespace_projection.fundamental_symbol;\
  }  /* if */                                                         \
}  /* reduce_projection_symbol_to_fundamental_symbol */


inline a_symbol_ptr fundamental_symbol_of(a_symbol_ptr  symbol)
/*
Return the fundamental symbol for a given symbol.
*/
{
  if (symbol->kind == sk_projection) {
    symbol = symbol->variant.projection.extra_info->fundamental_symbol;
  }  /* if */
  if (symbol->kind == sk_namespace_projection) {
    symbol = symbol->variant.namespace_projection.fundamental_symbol;
  }  /* if */
  return symbol;
}  /* fundamental_symbol_of */


inline a_symbol_ptr fundamental_symbol_of_projection(a_symbol_ptr  symbol)
/*
Return the fundamental symbol of a projection (but not a namespace projection)
for a given symbol.
*/
{
  if (symbol->kind == sk_projection) {
    symbol = symbol->variant.projection.extra_info->fundamental_symbol;
  }  /* if */
  return symbol;
}  /* fundamental_symbol_of_projection */


/*
Given a namespace projection symbol, return the fundamental symbol.
*/
#define namespace_projection_fundamental_symbol(sym)			\
  ((sym)->variant.namespace_projection.fundamental_symbol)

/*
Return TRUE if this_step (a_base_class_ptr) represents a traversal of a
protected base class in a conversion involving target_base (another
a_base_class_ptr) that would be considered accessible in the current
context by versions of g++ prior to 4.4.
*/
#if GNU_EXTENSIONS_ALLOWED
/* Do some easy tests here for efficiency before calling a function to do
   the heavy lifting. */
#define is_gnu_accessible_protected_base(this_step, target_base)           \
  ((this_step)->derivation->access == (an_access_specifier)as_protected && \
   gpp_mode && gnu_version < 40400 &&                                      \
   f_is_gnu_accessible_protected_base(this_step, target_base))

extern a_boolean f_is_gnu_accessible_protected_base(
                                                 a_base_class_ptr this_step,
                                                 a_base_class_ptr target_base);
#else /* !GNU_EXTENSIONS_ALLOWED */
/* Just return FALSE. */
/*lint -emacro(506,is_gnu_accessible_protected_base)*/
#define is_gnu_accessible_protected_base(this_step, target_base) FALSE
#endif /* GNU_EXTENSIONS_ALLOWED */

/*
Return TRUE if the base class indicated by the base class entry bcp
is an accessible base class of viewpoint_class.  bcp must be a direct or
virtual base class of viewpoint_class, but bcp->derived_class might
be something other than viewpoint_class.  bcp represents a step in the
derivation; target_base is the ultimate base class in the conversion.  A
base class is accessible if its public members are accessible, which means
  (a) if the derivation is public, the base class is accessible;
  (b) if the derivation is private, the base class is accessible if we
      have member access to the derived class;
  (c) if the derivation is protected, the base class is accessible if we
      have member access to the derived class or to one of its derived
      classes.
In addition, when emulating g++ versions prior to 4.4, a protected step in
the derivation that is not otherwise acceptable is permitted if targ_base
is accessible in the current context (this check is implemented by
is_gnu_accessible_protected_base above).  is_accessible_imm_base_class can
be used for direct or virtual base classes; if the derivation step is a
non-simple virtual step, it calls is_accessible_virtual_base_class.
is_accessible_direct_base_class_derivation can be used for specific
derivations of direct or simple virtual base classes.
The function is_accessible_base_class should be used when it is
not known that the base class is an immediate base class.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
/* The Microsoft version of this macro is different in that it considers
   a base class accessible if we have member access to the base class.
   This is probably based on the wording in 11.2 that says "A base class
   is said to be accessible if an invented public member of the class
   is accessible."  This is fixed in the 7.1 compiler. */
#define is_accessible_direct_base_class_derivation(bcp, bcdp, viewpoint_class)\
  ((bcdp)->access == (an_access_specifier)as_public ||                \
   have_member_access_privilege(viewpoint_class) ||                   \
   ((bcdp)->access == (an_access_specifier)as_protected &&            \
    have_protected_member_access_privilege(viewpoint_class)) ||       \
   (microsoft_mode && microsoft_version <= 1300 &&		      \
    have_member_access_privilege(bcp->type)))
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define is_accessible_direct_base_class_derivation(bcp, bcdp, viewpoint_class)\
  ((bcdp)->access == (an_access_specifier)as_public ||                \
   have_member_access_privilege(viewpoint_class) ||                   \
   ((bcdp)->access == (an_access_specifier)as_protected &&            \
    have_protected_member_access_privilege(viewpoint_class)))
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#define is_virtual_but_not_simple_direct_base_class(bcp)              \
  ((bcp)->is_virtual && (!(bcp)->direct || (bcp)->derivation->next != NULL))
#define is_accessible_imm_base_class(bcp, viewpoint_class, target_base) \
  ((is_virtual_but_not_simple_direct_base_class(bcp) ?                  \
    is_accessible_virtual_base_class(bcp, viewpoint_class) :            \
    is_accessible_direct_base_class_derivation(bcp, bcp->derivation,    \
                                               viewpoint_class)) ||     \
   is_gnu_accessible_protected_base(bcp, target_base))

extern a_boolean is_accessible_base_class(a_base_class_ptr bcp);

extern a_boolean is_accessible_virtual_base_class(
                                             a_base_class_ptr bcp,
                                             a_type_ptr       viewpoint_class);

extern a_symbol_ptr find_progenitor_symbol(
                      a_type_ptr               class_ptr,
                      a_symbol_locator         *locator,
                      an_id_lookup_options_set options,
		      a_boolean		       look_in_dependent_bases,
		      a_boolean		       look_in_interfaces,
                      a_derivation_step_ptr    *path,
                      an_access_specifier      *access,
                      a_boolean                *ambiguous,
                      a_boolean                *any_using_decl,
                      a_boolean                *unambiguous_injected_template);

extern void set_source_corresp_name(a_source_correspondence	*sc,
				    a_symbol_header_ptr		sym_header);

extern void clear_source_corresp_name(a_source_correspondence	*sc);

extern void set_source_corresp(a_source_correspondence *sc,
                               a_symbol_ptr            sp);

extern
void set_source_corresp_with_scope_depth(a_source_correspondence *sc,
                                         a_symbol_ptr            sp,
			                 a_scope_depth		depth);

extern void set_class_membership(a_symbol_ptr             sym,
                                 a_source_correspondence  *scp,
                                 a_type_ptr               class_type);

extern void set_namespace_membership(a_symbol_ptr             sym,
                                     a_source_correspondence  *scp,
                                     a_namespace_ptr          nsp);

extern void set_membership_in_source_corresp(a_source_correspondence  *scp,
                                             a_symbol_ptr             sym);

/* Allocation */
extern an_extern_type_fixup_ptr alloc_etype_fixup(void);
extern
a_substituted_type_list_entry_ptr alloc_substituted_type_list_entry(void);
extern a_symbol_list_entry_ptr alloc_symbol_list_entry(void);
extern void free_list_of_symbol_list_entries(a_symbol_list_entry_ptr slep);
extern a_type_list_entry_ptr alloc_type_list_entry(void);
extern void free_list_of_type_list_entries(a_type_list_entry_ptr slep);
extern a_namespace_list_entry_ptr alloc_namespace_list_entry(void);
extern
void free_list_of_namespace_list_entries(a_namespace_list_entry_ptr nlep);
extern a_template_param_ptr alloc_template_param(a_symbol_ptr sym);
extern a_template_param_ptr make_copy_of_template_param_based_on_new_symbol(
					a_template_param_ptr	orig_tpp,
					a_symbol_ptr		new_sym);
extern a_template_instance_ptr alloc_template_instance(void);
extern a_master_instance_ptr alloc_master_instance(void);
extern void free_param_id_list(a_param_id_ptr *pidlist);
extern void clear_func_info(a_func_info_block *func_info);

/*
Free any param_id entries that may have been allocated for the given
func_info block.  This is suppressed for variadic template definition
contexts because those param_ids might be referenced by pack expansion
descriptions.
*/
#define done_with_func_info(func_info)					\
  if (!is_variadic_definition_context() &&				\
      !(func_info).keep_param_id_list) {				\
    free_param_id_list(&((func_info).param_id_list));			\
  }  /* if */

extern void clear_decl_modifiers_block(a_decl_modifiers_block *decl_modifiers);

extern void add_to_param_id_list(a_symbol_locator            *locator,
                                 a_type_ptr                  type_ptr,
                                 a_source_position           *type_pos,
                                 a_storage_class             storage_class,
                                 a_func_info_block_ptr       func_info,
                                 a_source_sequence_entry_ptr param_ssep,
                                 a_param_id_ptr              *last_param_id,
                                 a_boolean                   is_pack_element);

extern a_param_id_ptr param_id_on_list(a_symbol_locator *locator,
                                       a_param_id_ptr    param_id_list);

extern void add_to_dependent_type_fixup_list(
                                      a_type_ptr                   type_ptr,
                                      a_dependent_type_fixup_kind  fixup_kind,
                                      char                         *entity_ptr,
                                      an_il_entry_kind             entity_kind,
                                      a_source_position            *pos);

extern void defer_exception_spec_error(a_func_info_block  *func_info,
                                       an_error_code      error_code,
                                       a_source_position  *pos,
                                       a_type             *assoc_type);

extern void report_exception_spec_errors(a_func_info_block  *func_info);

extern void check_dependent_type_fixup_list(a_symbol_ptr  sym);

extern a_namespace_ptr parent_namespace_for_symbol(a_symbol_ptr sym);

extern a_boolean is_local_symbol(a_symbol_ptr sym);

extern a_boolean is_block_extern_symbol(a_symbol_ptr sym);

extern void determine_operator_lookup_namespaces(a_type_ptr class_type);

extern a_symbol_ptr find_label_symbol(a_symbol_header_ptr sym_hdr);

extern a_symbol_ptr find_macro_symbol(a_symbol_header_ptr sym_hdr);

extern a_symbol_ptr find_macro_symbol_by_name(a_const_char     *identifier,
                                              sizeof_t         length,
                                              a_symbol_locator *locator);

extern a_symbol_header_ptr find_symbol_header(a_const_char     *identifier,
                                              sizeof_t         length,
                                              a_symbol_locator *locator);


extern a_symbol_header_ptr find_il_symbol_header(a_const_char *identifier,
                                                 sizeof_t     length);


#define symbol_for(entry)  ((a_symbol_ptr)(entry)->source_corresp.assoc_info)

template<typename a_Type>
EXPAND a_symbol_ptr symbol_for_or_null(const a_Type *entry)
/*
Return the symbol associated with an IL entry, or NULL if the IL entry is NULL.
*/
{
  return (entry == NULL) ? NULL : symbol_for(entry);
}  /* symbol_for_or_null */


/*
Return whether a given symbol is of a given kind.
*/
#define symbol_is(sym, sym_kind)                                             \
  ((sym)->kind == (a_symbol_kind)(sym_kind))

/*
Return the master instance pointer of a template instance.
*/
/*lint -emacro(664,master_instance_of)*/
#define master_instance_of(tip)						\
  ((check_assertion((tip)->master_instance != NULL), (tip)->master_instance))

/* Return TRUE if a symbol is a class symbol.   A class symbol is
   one defined as a class, struct, or union, or a typedef of one of
   those.  This macro should only be used in C++ mode. */
#define is_class_symbol(sym)                                          \
  ((sym)->kind == (a_symbol_kind)sk_class_or_struct_tag ||            \
   (sym)->kind == (a_symbol_kind)sk_union_tag ||                      \
   ((sym)->kind == (a_symbol_kind)sk_type &&                          \
                   is_class_struct_union_type((sym)->variant.type.ptr)))

/* Return TRUE if a symbol is of kind sk_class_or_struct_tag or
   sk_union_tag. */
#define is_class_struct_union_symbol(sym)                             \
  ((sym)->kind == (a_symbol_kind)sk_class_or_struct_tag ||            \
   (sym)->kind == (a_symbol_kind)sk_union_tag)

extern a_boolean class_sym_is_for_closure_class(a_symbol_ptr  sym);

#define is_closure_class_symbol(sym)                                  \
  ((sym)->kind == (a_symbol_kind)sk_class_or_struct_tag &&            \
   class_sym_is_for_closure_class(sym))

/* Return TRUE if a symbol is a namespace symbol. */
#define is_namespace_symbol(sym)                                          \
  ((sym)->kind == (a_symbol_kind)sk_namespace)

/* Return TRUE if a symbol is an enum symbol or a typedef to an enum. */
#define is_enum_symbol(sym)					      \
  ((sym)->kind == (a_symbol_kind)sk_enum_tag ||			      \
   ((sym)->kind == (a_symbol_kind)sk_type &&			      \
    is_enum_type((sym)->variant.type.ptr)))

extern a_boolean overload_set_contains_template(a_symbol_ptr sym);

extern a_boolean sym_may_include_nonstatic_member_function(a_symbol_ptr sym);

/* Return TRUE if a symbol is an sk_type symbol that points to a
   tk_template_param type, or a typeref to such a type.
*/
#define is_template_param_type_symbol(sym)				\
  ((sym)->kind == (a_symbol_kind)sk_type &&				\
   skip_typerefs((sym)->variant.type.ptr)->kind ==			\
                                              (a_type_kind)tk_template_param)

/*
Return TRUE if sym refers to a C++/CLI property or event, which can
be used as a qualifier in a qualified name.  The symbol may also refer to
an overloaded set of properties.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#define is_cppcli_property_or_event(sym)                                \
  (cli_or_cx_enabled &&                                                 \
   (symbol_is(sym, sk_property_set) ||                                  \
    (symbol_is((sym), sk_field) &&                                      \
     (sym)->variant.field.ptr->property_or_event_descr != NULL &&       \
     (sym)->variant.field.ptr->property_or_event_descr->kind !=         \
                    (a_property_or_event_kind)pek_declspec_property) || \
    (symbol_is(sym, sk_static_data_member) &&                           \
     (sym)->variant.static_data_member.variable->                       \
                                property_or_event_descr != NULL &&      \
     (sym)->variant.static_data_member.variable->                       \
                                property_or_event_descr->kind !=        \
                        (a_property_or_event_kind)pek_declspec_property)))
#else  /* !MICROSOFT_EXTENSIONS_ALLOWED */
/*lint -emacro(506,is_cppcli_property_or_event)*/
#define is_cppcli_property_or_event(sym) FALSE
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Return TRUE if we're in C++/CLI mode and sym (which must be an sk_type symbol)
refers to a fundamental type with a corresponding C++/CLI System value type
(e.g., System::Char for wchar_t).
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#define is_cppcli_fundamental_system_type(sym)                          \
  (cli_or_cx_enabled &&                                                 \
   (system_type_from_fundamental_type(                                  \
                    skip_typerefs((sym)->variant.type.ptr)) != NULL))
#else  /* !MICROSOFT_EXTENSIONS_ALLOWED */
/*lint -emacro(506,is_cppcli_fundamental_system_type)*/
#define is_cppcli_fundamental_system_type(sym) FALSE
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */


/*
Return TRUE if a symbol is one that should be found in a lookup of a name used
as part of the qualifier in a qualified name.  The C++ standard requires that
any type name be found by the lookup even though some kinds of types will
later result in an error.  Certain invalid types (enums and/or typedefs to
non-class types) are ignored in Microsoft and g++ mode.  This macro should
only be used in C++ mode.
*/
#define symbol_may_precede_qualifier(sym)                             \
  ((sym)->kind == (a_symbol_kind)sk_class_template ||		      \
   is_class_symbol(sym) ||                                            \
   (sym)->kind == (a_symbol_kind)sk_namespace ||		      \
   is_cppcli_property_or_event(sym) ||				      \
   ((sym)->kind == (a_symbol_kind)sk_type &&                          \
    (is_template_param_type((sym)->variant.type.ptr) ||               \
     is_cppcli_fundamental_system_type(sym) ||			      \
     (!microsoft_mode && (!gpp_mode || gnu_version < 30400)))) ||     \
   ((!gpp_mode || gnu_version < 30400 || enum_qualifiers_enabled) &&  \
    is_enum_symbol(sym)))

/*
Return TRUE if sym represents an enum name that can be a valid name qualifier.
Ordinarily, this is the case for all enum types when enum_qualifiers_enabled
is TRUE.  However, when emulating earlier Microsoft compilers, enum types that
are not class members are excluded.
*/
#define is_valid_enum_qualifier_symbol(sym)                           \
  (enum_qualifiers_enabled && is_enum_symbol(sym) &&                  \
   !(microsoft_mode && microsoft_version < 1400 && !cpp11_mode &&     \
     !skip_typerefs(type_symbol_type(sym))->source_corresp.is_class_member))

/*
Return TRUE if sym represents an entity that can be used as the qualifier
in a qualified name.  symbol_may_precede_qualifier is TRUE if the name
should be found by lookup; this macro is then used to determine if the
symbol found by the lookup is semantically valid.
*/
#define is_valid_qualifier_symbol(sym)				      \
  ((sym)->kind == (a_symbol_kind)sk_class_template ||		      \
   is_class_symbol(sym) ||                                            \
   (sym)->kind == (a_symbol_kind)sk_namespace ||		      \
   ((sym)->kind == (a_symbol_kind)sk_type &&                          \
    (is_template_param_type((sym)->variant.type.ptr) ||		      \
     is_cppcli_fundamental_system_type(sym))) ||		      \
   is_valid_enum_qualifier_symbol(sym))
  
/* Return TRUE if a symbol is a class symbol, a class template symbol,
   a template parameter symbol, or a typedef to a template parameter.
   Note that a template parameter symbol is considered even if the type
   referred to is not a class type. */
#define is_class_or_class_proxy_symbol(sym)                               \
  (is_class_symbol(sym) ||					      \
   (sym)->kind == (a_symbol_kind)sk_class_template ||		      \
   ((sym)->kind == (a_symbol_kind)sk_type &&			      \
    (is_template_param_type((sym)->variant.type.ptr) ||		      \
    (sym)->is_template_param)))

/* Return TRUE if the symbol is a template class symbol. */
#define is_template_class_symbol(sym)				      \
  (is_class_struct_union_symbol(sym) &&				      \
   class_symbol_supp(sym)->class_template != NULL)

/* Return TRUE if the symbol is a template class symbol for a class
   generated from the template (i.e., not a specific definition). */
#define is_template_class_and_not_specific_def_symbol(sym)		\
  (is_template_class_symbol((sym)) &&					\
   !(sym)->variant.class_struct_union.type->                            \
                        variant.class_struct_union.is_specialized)

/* Return TRUE if the symbol is a class template symbol. */
#define is_class_template_symbol(sym)					\
  ((sym)->kind == (a_symbol_kind)sk_class_template)

/* Return TRUE if the symbol is a class template symbol, but not one that
   represents an alias template. */
#define is_class_template_but_not_alias_symbol(sym)			\
  (((sym)->kind == (a_symbol_kind)sk_class_template) &&			\
   !(sym)->variant.template_info->variant.class_template.is_alias_template)

/* Return TRUE if the symbol is an alias template symbol. */
#define is_alias_template_symbol(sym)			\
  (((sym)->kind == (a_symbol_kind)sk_class_template) &&			\
   (sym)->variant.template_info->variant.class_template.is_alias_template)

/*
Return TRUE if the symbol is a class template, but not a C++/CLI generic.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#define is_class_template_but_not_cli_generic(sym)			\
  (is_class_template_symbol(sym) &&					\
   !(sym)->variant.template_info->is_generic)
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define is_class_template_but_not_cli_generic(sym)			\
  (is_class_template_symbol(sym))
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if MICROSOFT_EXTENSIONS_ALLOWED

/*
Return the non-generic class symbol pointer (which may be NULL)
for a C++/CLI generic class.
*/
#define non_generic_class_for_cli_generic(sym)				\
   ((sym)->variant.template_info->variant.class_template.non_generic_class)

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if MICROSOFT_EXTENSIONS_ALLOWED

/*
Return TRUE if the symbol is a C++/CLI generic.
*/
#define is_cli_generic_class_symbol(sym)				\
  (is_class_template_symbol(sym) &&					\
   (sym)->variant.template_info->is_generic)

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Return TRUE if the symbol is the class symbol representing the definition
of a C++/CLI generic class.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#define is_cli_generic_class_definition_symbol(sym)			\
  (is_class_struct_union_symbol(sym) &&					\
   (sym)->variant.class_struct_union.type->				\
                   variant.class_struct_union.is_generic_definition)

#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
/*lint -emacro(506,is_cli_generic_class_definition_symbol)*/
#define is_cli_generic_class_definition_symbol(sym) (FALSE)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Return TRUE if the symbol is a prototype instantiation or C++/CLI generic
class definition.
*/
#define is_prototype_instantiation_or_cli_generic(symbol)	\
  (is_prototype_instantiation_symbol(symbol) ||				\
   is_cli_generic_class_definition_symbol(symbol))

/* Return TRUE if the symbol is an sk_type symbol that represents an
   injected class name in a template class.  In a template class, the
   template name can be used either to refer to the template or to refer
   to the current instance. */
#define is_injected_template_symbol(sym)				\
  ((sym)->kind == (a_symbol_kind)sk_type &&				\
   (sym)->variant.type.is_injected_class_name &&			\
   (sym)->variant.type.ptr->variant.class_struct_union.is_template_class && \
   (sym)->variant.type.ptr->						\
	    variant.class_struct_union.extra_info->template_arg_list != NULL)

/* Return TRUE if the symbol is an sk_type symbol that represents an
   injected class name. */
#define is_injected_class_symbol(sym)					\
  ((sym)->kind == (a_symbol_kind)sk_type &&				\
   (sym)->variant.type.is_injected_class_name)

/* Return TRUE if the symbol is a class template symbol or an sk_type
   symbol that represents an injected class name in a template class. */
#define is_class_template_or_injected_template_symbol(sym)		\
  (is_class_template_symbol(sym) ||					\
   is_injected_template_symbol(sym))

/* Return TRUE if the symbol is a class symbol for either a normal
   (non-template) or a "real" instantiation of a template class.  */
#define is_real_class_symbol(sym)				      \
  (((sym)->kind == (a_symbol_kind)sk_class_or_struct_tag ||           \
    (sym)->kind == (a_symbol_kind)sk_union_tag) &&		      \
    (sym)->variant.class_struct_union.type != NULL &&		      \
    !((sym)->variant.class_struct_union.type->			      \
                   variant.class_struct_union.is_nonreal_class))

/* Return TRUE if the symbol is a template class symbol for a class template
   instance or a class nested within a class template. */
#define is_template_instance_class_symbol(sym)				\
  (is_real_class_symbol(sym) &&						\
   (sym)->variant.class_struct_union.type->				\
                   variant.class_struct_union.is_template_class)

/* Return TRUE if the symbol is a template alias symbol for an alias
   template. */
#define is_template_alias_instance_symbol(sym)				\
  (symbol_is((sym), sk_type) &&						\
   (sym)->variant.type.ptr != NULL &&					\
   (sym)->variant.type.ptr->kind == (a_type_kind)tk_typeref &&		\
   is_typeref_kind((sym)->variant.type.ptr, trk_is_template_alias))

/* Return TRUE if the symbol is a template class symbol for a real or
   nonreal class template instance or a class nested within a class
   template. */
#define is_any_template_instance_class_symbol(sym)		\
  (((sym)->kind == (a_symbol_kind)sk_class_or_struct_tag ||           \
    (sym)->kind == (a_symbol_kind)sk_union_tag) &&		      \
   (sym)->variant.class_struct_union.type->			      \
                   variant.class_struct_union.is_template_class)

/* Return TRUE if the symbol is a template class symbol for a nonreal
   class template instance or a class nested within a nonreal class
   template.  This will include prototype instantiations.  Note that this
   is not TRUE for other nonreal types such as proxy classes for template
   parameters. */
#define is_nonreal_instance_class_symbol(sym)                         \
  (((sym)->kind == (a_symbol_kind)sk_class_or_struct_tag ||           \
    (sym)->kind == (a_symbol_kind)sk_union_tag) &&		      \
   (sym)->variant.class_struct_union.type->			      \
                   variant.class_struct_union.is_template_class &&    \
   (sym)->variant.class_struct_union.type->			      \
                   variant.class_struct_union.is_nonreal_class)

/*
If type is a proxy class, return the associated template parameter,
otherwise return NULL.
*/
#define template_param_if_proxy_class(type)				\
  ((type->kind == (a_type_kind)tk_class) &&				\
   (type)->variant.class_struct_union.proxy_class			\
   ? class_type_supp(type)->proxy_of_type                               \
   : NULL)

#if MICROSOFT_EXTENSIONS_ALLOWED
/* Return TRUE if the symbol is a template class symbol for a Microsoft mode
   instantiated nonreal class. */
#define is_ms_instantiated_nonreal_class_symbol(sym)                  \
  (((sym)->kind == (a_symbol_kind)sk_class_or_struct_tag ||           \
    (sym)->kind == (a_symbol_kind)sk_union_tag) &&		      \
   (sym)->variant.class_struct_union.type->			      \
      variant.class_struct_union.is_ms_instantiated_nonreal_class &&  \
   (sym)->variant.class_struct_union.type->			      \
                   variant.class_struct_union.is_nonreal_class)

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_boolean is_proxy_member_symbol(a_symbol_ptr  sym);

extern a_boolean f_symbol_is_pack(a_symbol_ptr	sym);

/*
Macro wrapper for f_symbol_is_pack to avoid calls in most contexts.
*/
#define symbol_is_pack(sym)						\
  (is_variadic_template_context() && is_template_dependent_context() ?	\
   f_symbol_is_pack(sym) : FALSE)

/* Return TRUE if the symbol is a specific definition of a class template
   instance or a class nested within a class template. */
#define is_template_instance_specific_def_symbol(sym)			\
  ((sym)->variant.class_struct_union.type->                             \
                        variant.class_struct_union.is_specialized)

/* Return TRUE if the symbol is a specific definition of a class template
   instance or a class nested within a class template, but not a
   prototype instantiation. 
   This differs from is_template_instance_specific_def_symbol when
   a Microsoft in-class specialization is defined in a prototype
   instantiation. */
#define is_real_template_instance_specific_def_symbol(sym)		\
  (is_real_class_symbol(sym) &&						\
   (sym)->variant.class_struct_union.type->                             \
                        variant.class_struct_union.is_specialized)

/* Return TRUE if the symbol is a template enum symbol. */
#define is_any_template_enum_symbol(sym)		\
  ((sym)->kind == (a_symbol_kind)sk_enum_tag &&			\
   (sym)->variant.enumeration.type->variant.integer.is_template_enum)

/* Return the template argument list associated with a given instance of a
   class template, alias template, function template, or variable template. */
#define template_arg_list_for_symbol(sym)				\
  ((sym)->kind == (a_symbol_kind)sk_type				\
    ? typeref_supp((sym)->variant.type.ptr)->template_arg_list          \
    : is_class_struct_union_symbol(sym)                                 \
      ? (sym)->variant.class_struct_union.type->			\
                     variant.class_struct_union.extra_info->template_arg_list \
      : symbol_is((sym), sk_variable)					\
        ? (sym)->variant.variable.ptr->template_info->template_arg_list	\
        : (sym)->variant.routine.ptr->template_arg_list)


/* Return the template argument list associated with a given template class
   or template alias symbol.  For an alias, the original argument list
   is returned. */
#define orig_template_arg_list_for_symbol(sym)				\
  ((sym)->kind == (a_symbol_kind)sk_type				\
    ? typeref_supp((sym)->variant.type.ptr)->orig_template_arg_list     \
    : is_class_struct_union_symbol(sym)                                 \
      ? (sym)->variant.class_struct_union.type->			\
                     variant.class_struct_union.extra_info->template_arg_list \
      : symbol_is((sym), sk_variable)					\
        ? (sym)->variant.variable.ptr->template_info->template_arg_list	\
        : (sym)->variant.routine.ptr->template_arg_list)


/*
Return the address of the pointer to the partial specialization argument
list for the given symbol.
*/
#define partial_spec_template_arg_list_addr_for_symbol(sym)		\
  (is_class_struct_union_symbol(sym)					\
    ? &(sym)->variant.class_struct_union.type->variant.			\
        class_struct_union.extra_info->partial_spec_template_arg_list	\
    : (symbol_is(sym, sk_variable)				\
      ? &(sym)->variant.variable.ptr->template_info->			\
                                   partial_spec_template_arg_list	\
      : NULL))


/* Return TRUE if the given symbol kind corresponds to a tag. */
#define is_tag_symbol_kind(kind)                                 \
  ((kind) == (a_symbol_kind)sk_class_or_struct_tag ||            \
   (kind) == (a_symbol_kind)sk_union_tag ||                      \
   (kind) == (a_symbol_kind)sk_enum_tag)

/* Return TRUE if a symbol is a tag symbol.   A tag symbol is
   one defined as a class, struct, union, or enum (but not as a typedef
   of one of those).  An injected class name, although represented as
   an sk_type symbol, is considered a tag for lookup purposes. */
#define is_tag_symbol(sym)                                            \
  (is_tag_symbol_kind((sym)->kind) ||                                 \
   is_injected_class_symbol(sym))

/* Return TRUE if a symbol is a tag symbol, or a type symbol in C++.
   A tag symbol is one defined as a class, struct, union, or enum (but
   not as a typedef of one of those).  In C++ a tag lookup does find type
   symbols even though they are not tags (and will usually result in an
   error when found).  is_friend is TRUE if the test is being done in the
   context of a friend declaration.  g++ ignores certain typedefs in
   friend declarations (but does find an injected class symbol). */
#define is_tag_or_cplusplus_type_symbol(sym, is_friend)               \
  (is_tag_symbol_kind((sym)->kind) ||                                 \
   (gpp_mode && gnu_version >= 40500 && is_injected_class_symbol(sym)) || \
   (!((is_friend) && gpp_mode && gnu_version >= 40500) &&		\
    (elab_type_lookup_finds_typedefs &&					\
     (sym)->kind == (a_symbol_kind)sk_type)))

/* Return TRUE if a symbol is a tag symbol, a class template symbol,
   or a type template parameter.  is_friend is TRUE if the test is being
   done in the context of a friend declaration.  */
#define is_tag_or_tag_proxy_symbol(sym, is_friend)                    \
  (is_tag_or_cplusplus_type_symbol((sym), (is_friend)) ||	      \
   (sym)->kind == (a_symbol_kind)sk_class_template ||		      \
   ((sym)->kind == (a_symbol_kind)sk_type && (sym)->is_template_param))

/* Return TRUE if a symbol is a type symbol.   A type symbol is
   one defined as a typedef, or, in C++, as a class, struct, union,
   or enum. */
#define is_type_symbol(sym)                                           \
  ((sym)->kind == (a_symbol_kind)sk_type ||                           \
   (C_dialect == C_dialect_cplusplus && is_tag_symbol(sym)))

/* Return TRUE if a symbol represents a class, function, or variable
   template. */
#define is_template_symbol(sym)                                           \
  ((sym)->kind == (a_symbol_kind)sk_class_template ||                     \
   (sym)->kind == (a_symbol_kind)sk_variable_template ||                  \
   (sym)->kind == (a_symbol_kind)sk_concept_template ||                  \
   (sym)->kind == (a_symbol_kind)sk_function_template)

/*
Return the variable associated with a variable or static data member
symbol.  For a variable template, return the prototype variable.
For any other kind of symbol, return NULL.
*/
#define variable_for_symbol(sym)					\
  (symbol_is((sym), sk_static_data_member)				\
    ? (sym)->variant.static_data_member.variable			\
    : symbol_is((sym), sk_variable)					\
      ? (sym)->variant.variable.ptr					\
      : symbol_is((sym), sk_variable_template)				\
        ? (sym)->variant.template_info->variant.variable.prototype_variable \
        : (a_variable_ptr)NULL)

/*
Return TRUE if a symbol is a class template or an injected template symbol.
*/
#define is_class_or_injected_template_symbol(sym)		       \
  ((sym)->kind == (a_symbol_kind)sk_class_template ||                  \
   is_injected_template_symbol(sym))

/*
Return TRUE if a symbol that, when followed by a "<", should not be
coalesced as a template for error recovery purposes.  In general, a
constant cannot be followed by a template argument list, but an exception
is made for tpck_member constants that can be found in some modes as a result
of the ability to name members assumed to exist in dependent base classes
using unqualified names.  Types are considered to be possible templates
here so that a "<" that follows one will not result in a "template argument
list not allowed" diagnostic.  An overloaded set of functions might include
one or more function templates, so it is also considered to be a possible
template.  Undefined symbols are possible templates, which generally
results in better error recovery.
*/
#define symbol_cannot_be_template(sym)					\
  ((!symbol_is(sym, sk_class_or_struct_tag) &&				\
    !symbol_is(sym, sk_union_tag) &&					\
    !symbol_is(sym, sk_overloaded_function) &&				\
    !symbol_is(sym, sk_class_template) &&				\
    !symbol_is(sym, sk_function_template) &&				\
    !symbol_is(sym, sk_variable_template) &&				\
    !symbol_is(sym, sk_type) &&						\
    !symbol_is(sym, sk_undefined)) &&					\
   (!symbol_is(sym, sk_constant) ||					\
    ((sym)->variant.constant->kind !=					\
                          (a_constant_repr_kind)ck_template_param ||	\
     (sym)->variant.constant->variant.template_param.kind !=		\
                    (a_template_param_constant_kind)tpck_member)))

/* Return TRUE if a symbol is a class or function template symbol or an
   overload set containing a function template symbol. */
#define symbol_is_or_contains_template(sym)				\
  (is_class_template_or_injected_template_symbol(sym) ||		\
   (sym)->kind == (a_symbol_kind)sk_function_template ||		\
   (sym)->kind == (a_symbol_kind)sk_variable_template ||		\
   is_template_variable_symbol(sym) ||					\
   ((sym)->kind == (a_symbol_kind)sk_overloaded_function &&		\
    overload_set_contains_template(sym)))

/* Return TRUE if a symbol is a function template symbol or an overload set
   containing a function template symbol. */
#define symbol_is_or_contains_function_template(sym)                    \
  (symbol_is(sym, sk_function_template) ||                              \
   (symbol_is(sym, sk_overloaded_function) &&                           \
    overload_set_contains_template(sym)))

/* Return TRUE if a symbol is an instance of a variable template. */
#define is_template_variable_symbol(sym)				\
  ((symbol_is((sym), sk_variable) ||					\
    symbol_is((sym), sk_static_data_member)) &&				\
    variable_for_symbol((sym)) != NULL &&				\
    variable_for_symbol((sym))->is_template_variable &&			\
    variable_for_symbol((sym))->template_info->template_arg_list != NULL)

/* Return TRUE if a symbol is a function symbol. */
#define is_function_symbol(sym)                                       \
  ((sym)->kind == (a_symbol_kind)sk_routine ||                        \
   (sym)->kind == (a_symbol_kind)sk_member_function ||                \
   (sym)->kind == (a_symbol_kind)sk_overloaded_function)

/* Return TRUE if a symbol represents a single function or member function. */
#define is_simple_function_symbol(sym)                                \
  ((sym)->kind == (a_symbol_kind)sk_routine ||                        \
   (sym)->kind == (a_symbol_kind)sk_member_function)

/* Return TRUE if a symbol is a function or function template symbol. */
#define is_function_or_template_symbol(sym)				\
  (is_function_symbol((sym)) ||						\
   (sym)->kind == (a_symbol_kind)sk_function_template)

/* Return TRUE if a symbol is a single function, member function, or function
   template symbol. */
#define is_simple_function_or_template_symbol(sym)                            \
  (is_simple_function_symbol((sym)) || symbol_is(sym, sk_function_template))

/* Return TRUE if a symbol is a member function symbol. */
#define is_member_function_symbol(sym)                                \
  ((sym)->is_class_member &&                                          \
   ((sym)->kind == (a_symbol_kind)sk_member_function ||               \
    (sym)->kind == (a_symbol_kind)sk_overloaded_function ||           \
    (sym)->kind == (a_symbol_kind)sk_function_template))

extern
a_special_function_kind special_function_kind_for_symbol(a_symbol_ptr	sym);

/*
If sym is a routine symbol of some sort, return TRUE if the special function
kind recorded in its routine entry is "kind" and FALSE if it is not.  If sym
is not a routine symbol, return FALSE.
*/
#define is_special_function_symbol(sym, kind)				\
  (special_function_kind_for_symbol(sym) == (a_special_function_kind)(kind))

extern a_type_ptr underlying_function_type(a_symbol_ptr  sym);

/* Return TRUE if a symbol is a constructor symbol. */
#define is_constructor_symbol(sym)                                    \
  is_special_function_symbol(sym,                                     \
                             (a_special_function_kind)sfk_constructor)


inline a_boolean is_deduction_guide_symbol(a_symbol_ptr sym)
/*
Return TRUE if the given symbol is a deduction guide; otherwise, return FALSE.
*/
{
  return is_special_function_symbol(sym, sfk_deduction_guide);
}  /* is_deduction_guide_symbol */


#define is_ctor_or_deduction_guide(sym)                                      \
  (is_special_function_symbol(sym,                                           \
                              (a_special_function_kind)sfk_constructor) ||   \
   is_deduction_guide_symbol(sym))

/* Return TRUE if the class has a trivial default constructor (implicitly
   declared or defaulted), and no nontrivial default constructor.  (Note
   that a class could have a defaulted trivial default constructor, and one
   or more nontrivial default constructors that have parameters with default
   arguments.  This macro returns FALSE for such classes; default
   initialization may be ambiguous in such cases.) */
#define has_trivial_default_constructor(cssp)                        \
  (!(cssp)->has_nontrivial_default_constructor &&                    \
   ((cssp)->trivial_default_constructor != NULL ||                   \
    (cssp)->constructor == NULL))

/* Like has_trivial_default_constructor above, but if at least one of the
   default constructors is trivial but another one is not, this still returns
   TRUE. */
#define has_any_trivial_default_constructor(cssp)                    \
  ((cssp)->trivial_default_constructor != NULL ||                    \
   (cssp)->constructor == NULL)

/* TRUE if the class has any (trivial or nontrivial) default constructor. */
#define has_any_default_constructor(cssp)                            \
    ((cssp)->has_nontrivial_default_constructor ||                   \
     (cssp)->trivial_default_constructor != NULL ||                  \
     (cssp)->constructor == NULL)

inline a_boolean has_explicit_trivial_default_ctor(
                                             a_class_symbol_supplement  *cssp)
/*
Return TRUE if the trivial default constructor is "explicit" (that can happen
if it is defined with "= default;").
*/
{
   return cssp->trivial_default_constructor != NULL &&
          cssp->trivial_default_constructor->variant.routine.ptr
                                           ->is_explicit_constructor;
}  /* has_explicit_trivial_default_ctor */



/* Return TRUE if a symbol is a destructor symbol. */
#define is_destructor_symbol(sym)                                     \
  is_special_function_symbol(sym,                                     \
                             (a_special_function_kind)sfk_destructor)

#if MICROSOFT_EXTENSIONS_ALLOWED
/* Return TRUE if a symbol is a C++/CLI static constructor symbol. */
#define is_static_constructor_symbol(sym)                             \
  is_special_function_symbol(sym,                                     \
                             (a_special_function_kind)sfk_static_constructor)

/* Return TRUE if a symbol is a C++/CLI finalizer symbol. */
#define is_finalizer_symbol(sym)                                      \
  is_special_function_symbol(sym,                                     \
                             (a_special_function_kind)sfk_finalizer)

extern a_boolean is_cli_param_array_routine_symbol(a_symbol_ptr sp);

#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
/*lint -emacro(506,is_static_constructor_symbol)*/
#define is_static_constructor_symbol(sym) FALSE
/*lint -emacro(506,is_finalizer_symbol)*/
#define is_finalizer_symbol(sym) FALSE
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_boolean f_has_nontrivial_ctor(a_class_symbol_supplement_ptr  cssp);

/* Return TRUE if a class symbol supplement is for a class with a nontrivial
   constructor. */
#define has_nontrivial_ctor(cssp)                                         \
  ((cssp)->constructor != NULL && f_has_nontrivial_ctor(cssp))

/* Return TRUE if a class symbol supplement is for a class with a nontrivial
   destructor.  (The cssp->destructor != NULL test ensures that the macro
   returns FALSE for nonreal class templates.) */
#define has_nontrivial_destructor(cssp)                               \
  ((cssp)->destructor != NULL && !(cssp)->has_trivial_destructor)

#define has_deleted_or_nontrivial_destructor(cssp)                      \
  ((cssp)->destructor != NULL &&                                        \
   (!(cssp)->destructor->variant.routine.ptr->is_trivial_destructor ||  \
    (cssp)->destructor->variant.routine.ptr->is_deleted))

/*
If ctor is an inheriting constructor, return the inherited routine entry.
Otherwise, return NULL.
*/
#define inh_ctor_inherited_ctor(ctor)                                         \
  ((ctor)->is_inheriting_ctor ?                                               \
   (ctor)->friends_or_originator.inherited_routine : NULL)

#define inh_ctor_inherits_virtually(ctor)                                     \
  ((ctor)->inherits_virtually)                                                \

#define rout_befriending_classes(rout)                                        \
  (!(rout)->is_inheriting_ctor ?                                              \
   (rout)->friends_or_originator.befriending_classes : NULL)


inline a_routine_ptr get_inh_ctor_originator(
					   a_routine_ptr ctor,
					   a_boolean     ignore_virtual = TRUE)
/*
ctor is a generated inheriting constructor.  Find and return the original class
type from where the constructor came.  If ignore_virtual is FALSE and ctor
inherits virtually, return the virtual base constructor it inherited, whether
or not that constructor is also inheriting (this is typically what's desired
due to the way virtual bases are initialized).  Otherwise return the true
originator of the inheriting constructor.
*/
{
  a_boolean inheriting_virtually = !ignore_virtual &&
                                   inh_ctor_inherits_virtually(ctor);

  while (ctor->is_inheriting_ctor) {
    /* If the inheriting constructor is inheriting the constructor from a
       virtual base, and the virtual base's inheriting constructor does *not*
       inherit from a virtual base, consider the virtual base's constructor to
       be the originator.  This may not be wholly true; however, initialization
       of the virtual base behaves as if the constructor originated from the
       virtual base. */
    ctor = inh_ctor_inherited_ctor(ctor);
    if (inheriting_virtually && !inh_ctor_inherits_virtually(ctor)) {
      break;
    }  /* if */
  }  /* while */
  return ctor;
}  /* get_inh_ctor_originator */


inline a_symbol_ptr originator_symbol_of(a_symbol_ptr  sym)
/*
Similar to "fundamental_symbol_of", but also "look through" inheriting
constructor symbols.
*/
{
  sym = fundamental_symbol_of(sym);
  if (is_simple_function_symbol(sym)) {
    a_routine_ptr  rp = sym->variant.routine.ptr;
    if (rp->is_inheriting_ctor) {
      sym = symbol_for(get_inh_ctor_originator(rp));
    }  /* if */
  } else if (symbol_is(sym, sk_function_template)) {
    a_routine_ptr  rp = sym->variant.template_info->variant.function.routine;
    if (rp->is_inheriting_ctor) {
      sym = symbol_for(get_inh_ctor_originator(rp)->assoc_template);
    }  /* if */
  }  /* if */
  return sym;
}  /* originator_symbol_of */


/* Return TRUE if a symbol is a conversion operator symbol. */
#define is_conversion_function_symbol(sym)                            \
  is_special_function_symbol(sym,                                     \
                             (a_special_function_kind)sfk_conversion)

/* Return TRUE if a symbol is a projection symbol created for a class
   member using declaration. */
#define is_class_member_using_decl_symbol(sym)                        \
  ((sym)->kind == (a_symbol_kind)sk_projection &&                     \
   (sym)->variant.projection.is_using_decl)

/* Return TRUE if a symbol is a class template symbol that represents
   a nonreal template (such as X in T::X<int>). */
#define is_nonreal_template_symbol(sym)					\
  ((sym)->kind == (a_symbol_kind)sk_class_template &&			\
   (sym)->variant.template_info->is_nonreal_member)

/* Return TRUE if a symbol is a class template symbol that represents
   a template template parameter. */
#define is_template_template_param_symbol(sym)				\
  ((sym)->kind == (a_symbol_kind)sk_class_template &&			\
   (sym)->variant.template_info->					\
                          variant.class_template.template_template_param)

/* Return TRUE if a symbol represents a type template param. */
#define is_type_template_param_symbol(sym)                              \
  ((sym)->kind == (a_symbol_kind)sk_type &&                             \
   (sym)->variant.type.ptr->kind == (a_type_kind)tk_template_param)

/* Return TRUE if a symbol represents a non-type template param. */
#define is_nontype_template_param_symbol(sym)                           \
  ((sym)->kind == (a_symbol_kind)sk_constant &&                         \
   (sym)->variant.constant != NULL &&					\
   (sym)->variant.constant->kind == (a_constant_repr_kind)ck_template_param)


inline a_type_ptr type_symbol_type(a_symbol_ptr sym)
/*
Extract and return the type from a type symbol (one for which is_type_symbol is
TRUE).
*/
{
  a_type_ptr result = NULL;

  switch (sym->kind) {
    case sk_type:
      result = sym->variant.type.ptr;
      break;
    case sk_enum_tag:
      result = sym->variant.enumeration.type;
      break;
    case sk_class_or_struct_tag:
    case sk_union_tag:
      result = sym->variant.class_struct_union.type;
      break;
    default:
      /* If this is reached, a case is presumably missing or is_type_symbol
         returned TRUE when it should not have. */
      unexpected_condition();
  }  /* switch */
  return result;
}  /* type_symbol_type */


/*
Extract the routine type from the routine associated with an sk_routine
or sk_member_function symbol.
*/
#define routine_symbol_type(sym)                                      \
  (skip_typerefs((sym)->variant.routine.ptr->type))

/*
Get the routine entry associated with a function or function template.
*/
#define func_sym_routine(sym)                                         \
  (symbol_is(sym, sk_function_template) ?                             \
       (sym)->variant.template_info->variant.function.routine :       \
       (sym)->variant.routine.ptr)

/*
Extract a pointer to the symbol supplement for a static data member.  The
result may be NULL.
*/
#define sdm_supp(sdm_sym)                                             \
  ((sdm_sym)->variant.static_data_member.extra_info)

/*
Get a pointer to the symbol supplement for a static data member.  If there
is no such supplement yet, allocate one.
*/
#define get_sdm_supp(sdm_sym)                                                \
  (sdm_supp(sdm_sym) != NULL ? sdm_supp(sdm_sym)                             \
                             : alloc_static_data_member_supplement(sdm_sym))

extern a_static_data_member_supplement_ptr
                   alloc_static_data_member_supplement(a_symbol_ptr  sdm_sym);

EXPAND a_class_symbol_supplement_ptr& class_symbol_supp(a_symbol_ptr sym)
/*
Extract a pointer to the class symbol supplement for a given class type_symbol.
*/
{
#if EXPENSIVE_CHECKING
  check_assertion(sym->kind == sk_class_or_struct_tag ||
                  sym->kind == sk_union_tag);
#endif /* EXPENSIVE_CHECKING */
  return sym->variant.class_struct_union.extra_info;
}  /* class_symbol_supp */

/*
Extract a pointer to the class symbol supplement for a given type for
which is_class_struct_union_type is TRUE.
*/
#define symbol_supplement_for_class(tp)                              \
  class_symbol_supp(symbol_for(skip_typerefs(tp)))

/*
Extract a pointer to the enum symbol supplement for a given type for
which is_enum_type is TRUE.
*/
#define symbol_supplement_for_enum(tp)                              \
  (((a_symbol_ptr)(skip_typerefs(tp))->source_corresp.assoc_info)->  \
                            variant.enumeration.extra_info)

/*
Given a namespace pointer, return a pointer to the namespace symbol
supplement.
*/
#define symbol_supplement_for_namespace(nsp)	                     \
  (((a_symbol_ptr)(skip_namespace_aliases(nsp))->source_corresp.assoc_info)-> \
                                          variant.namespace_info.extra_info)


/* Return a pointer to the current routine entry (only usable when within
   a routine definition). */
#define current_routine_entry() (innermost_function_scope->variant.routine.ptr)

/* Return a pointer to the current routine entry or NULL if there is no
   current routine. */
#define curr_routine_or_null()                                               \
  (innermost_function_scope != NULL ?                                        \
          innermost_function_scope->variant.routine.ptr : NULL)


inline a_template_symbol_supplement_ptr
template_supplement_for_symbol(a_symbol_ptr sym)
/*
Return a pointer to the template symbol supplement for a given symbol.  Return
NULL for symbols of the wrong kind.
*/
{
  a_template_symbol_supplement_ptr result = NULL;

  switch (sym->kind) {
    case sk_class_template:
    case sk_variable_template:
    case sk_concept_template:
    case sk_function_template:
      result = sym->variant.template_info;
      break;
    case sk_member_function:
      result = sym->variant.routine.instance_ptr->template_info;
      break;
    case sk_class_or_struct_tag:
    case sk_union_tag:
      result = sym->variant.class_struct_union.extra_info->template_info;
      break;
    case sk_static_data_member:
      result = sym->variant.static_data_member.instance_ptr->template_info;
      break;
    case sk_enum_tag:
      result = sym->variant.enumeration.extra_info->template_info;
      break;
    default:
      break;
  }  /* switch */
  return result;
}  /* template_supplement_for_symbol */


/* If sym is a template template parameter, return the symbol for the template
   argument, otherwise return the original symbol. */
#define template_argument_if_template_template_param(sym)		\
  (((sym)->kind == (a_symbol_kind)sk_class_template &&			\
    (sym)->variant.template_info->					\
                     variant.class_template.template_template_param)	\
     ? (sym)->variant.template_info->variant.class_template.argument_template \
     : sym)

/* Return TRUE if the symbol represents the prototype instantiation of a
   class template. */
#define is_prototype_instantiation_symbol(sym)				\
  (is_class_struct_union_symbol(sym) &&					\
   (sym)->variant.class_struct_union.type->				\
                   variant.class_struct_union.is_prototype_instantiation)

/* If a symbol represents a subordinate template, return a pointer to the
   prototype template; otherwise return the symbol provided. */
#define prototype_template_of(sym)					\
  ((sym)->variant.template_info->prototype_template != NULL &&		\
   !(sym)->variant.template_info->is_specific_definition ?		\
      (sym)->variant.template_info->prototype_template : (sym))

/*
If "sym" is a template symbol (class or function) return the prototype template
symbol; otherwise return the original symbol.
*/
#define prototype_template_if_template_symbol(sym)			\
  (is_template_symbol(sym) ? prototype_template_of(sym) : (sym))

/*
If "sym" is a class template symbol return the primary template
symbol; otherwise return the original symbol.
*/
#define primary_template_if_template_symbol(sym)			\
  (is_class_template_symbol(sym) ? primary_template_of(sym) : (sym))

/* Return a pointer to the namespace associated with a namespace symbol.
   Remove any namespace aliases that may be present.  The symbol provided
   must be a namespace symbol. */
#define namespace_symbol_namespace(sym)					\
  (skip_namespace_aliases((sym)->variant.namespace_info.ptr))

/*
Given a symbol kind (associated with a template parameter) return the
template argument kind to be used.
*/
/*lint -emacro(641,templ_arg_kind_for_symbol_kind)*/
#define templ_arg_kind_for_symbol_kind(sym_kind)			\
  ((a_templ_arg_kind)((sym_kind) == (a_symbol_kind)sk_type ? tak_type :	\
   ((sym_kind) == (a_symbol_kind)sk_constant ? tak_nontype : tak_template)))

/*
Return TRUE if tp is not a C++03 POD (for pre-C++11 modes) or has nontrivial
constructors or destructor (for C++11 and later modes).
*/
#define is_nonPOD_or_has_nontrivial_copy_semantics(tp)                \
    (cpp11_mode ? !is_trivially_copyable_type(tp)                     \
                : !is_pod_class(skip_typerefs(tp)))

/*
Returns TRUE if tp is not a C++03 POD (for pre-C++11 modes) or is not a
standard-layout class (for C++11 and later modes).
*/
#define is_nonPOD_or_has_non_standard_layout(tp)           \
    ((cpp11_mode)                                          \
       ? !symbol_supplement_for_class(tp)->standard_layout \
       : !symbol_supplement_for_class(tp)->is_cpp03_POD)

void form_optionally_qualified_symbol_name(
		a_symbol_ptr				sym,
		an_il_to_str_output_control_block_ptr	octl,
		a_boolean				suppress_qualifier);

extern void form_symbol_name(a_symbol_ptr                          sym,
                             an_il_to_str_output_control_block_ptr octl);


EXTERN_THREAD a_boolean
		collect_top_templates;
			/* When TRUE (set by the --top_templates option),
			   templated entities are recorded as they are
			   created so that show_top_templates can later
			   report the most-substituted templates. */
EXTERN_THREAD unsigned
		top_templates_count;
			/* The number of templates to report (the "N" of
			   --top_templates=N).  When zero, every template with
			   a nonzero number of substitutions is reported. */

/* Report the most-substituted templates to the error output file.  See
   show_top_templates in symbol_tbl.c for details. */
extern void show_top_templates(unsigned  n);

#if DEBUG
/* GCOVR_EXCL_START */
/* Show and return the amount of memory used by symbol table entries. */
extern unsigned long show_symbol_space_used(void);

/* Display a symbol table entry. */
extern void db_symbol(a_symbol_ptr sym,
                      a_const_char *string,
                      size_t       indentation);

/* Short-hand version of db_symbol. */
extern void db_sym(a_symbol_ptr  sym);

extern void db_symbol_name(a_symbol_ptr  sym);

extern a_const_char *db_symbol_trans_unit(a_symbol_ptr sym);

extern void db_symbol_name_trans_unit(a_symbol_ptr sym);

extern char *db_canonical_ptr_for_symbol(a_symbol_ptr	sym);

extern void db_template_param_list(a_template_param_ptr	tpp);

extern void db_template_parameter(a_template_param_ptr	tpp);

extern void db_tpp(a_template_param_ptr  tpp);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern void db_hide_by_sig_list(a_hide_by_sig_list_entry_ptr	hbslep);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Information used to gather performance statistics related to symbol
table processing that needs to be externally visible.
*/
EXTERN_THREAD unsigned long
		num_fast_id_lookups,
		num_slow_id_lookups,
		num_active_using_directives_allocated,
		num_generated_entity_blocks_allocated;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

extern a_symbol_ptr f_class_template_for_type(a_type_ptr	type);

/*
Interface to f_class_template_for_type that handles all of the
cases that are not template classes.
*/
#define class_template_for_type(type)					\
  ((is_immediate_class_type(type) &&					\
   (type)->variant.class_struct_union.is_template_class) ?		\
   f_class_template_for_type(type) : (a_symbol_ptr)NULL)

extern a_symbol_ptr class_template_for_injected_template_symbol(
							a_symbol_ptr sym);

extern a_symbol_ptr find_literal_operator(a_const_char      *name,
                                          sizeof_t          name_len,
                                          a_source_position *pos,
                                          a_type_ptr        literal_type,
                                          a_boolean         from_cache,
                                          a_diagnostic_ptr  dp);

/*
Return TRUE if "tp" is a proxy class.
*/
#define is_proxy_class(tp)						\
  (type_is(tp, tk_class) && class_type_supp(tp)->proxy_of_type != NULL)

extern a_scope_number take_next_scope_number(void);

extern a_boolean symbol_is_from_trans_unit(a_symbol_ptr			sym,
					   a_translation_unit_ptr	tup);

extern
a_translation_unit_ptr get_trans_unit_for_scope(a_scope_number	scope_number);


inline a_boolean symbol_has_trans_unit_ptr(a_symbol_ptr sym)
/*
Return TRUE if a translation unit pointer can be retrieved from this symbol;
otherwise, return FALSE.
*/
{
#if EXPENSIVE_CHECKING
  /* If this assertion fails, the caller likely is using a symbol that it
     shouldn't be.  The symbol's associated IL entry belongs to a freed memory
     region and requesting the symbol's translation unit is dubious. */
  check_assertion_str(sym == NULL || sym->kind != sk_freed,
                      "attempted to check translation unit of a freed symbol");
#endif /* EXPENSIVE_CHECKING */
  return sym != NULL && !sym->is_error && sym->decl_scope != NO_SCOPE_NUMBER;
}  /* symbol_has_trans_unit_ptr */


extern a_translation_unit_ptr trans_unit_for_symbol(a_symbol_ptr	sym);

extern void set_keyword_visibility(a_const_char     *keyword,
                                   a_boolean        is_visible,
                                   a_symbol_locator *loc);

#if SUN_EXTENSIONS_ALLOWED
extern void ldscope_pragma(a_pending_pragma_ptr ppp);
#endif /* SUN_EXTENSIONS_ALLOWED */

/*
Type used to represent the size of a hash table.  This must not be larger
than the size of a_hash_value.
*/
typedef uint32_t
		a_hash_table_size;

/*
Entry used to represent an entry in a hash table.
*/
typedef struct a_hash_table_entry *a_hash_table_entry_ptr;
typedef struct a_hash_table_entry {
  a_hash_table_entry_ptr
		next;	/* The next entry in the bucket, or NULL for the last
			   entry in the bucket. */
  a_void_ptr	data;	/* Opaque pointer to the entity represented by this
			   entry. */
  a_hash_value	hash_value;
			/* The hash value of this entry. */
} a_hash_table_entry;

/*
The type of a function used to produce a hash value for a given key.
*/
typedef a_hash_value a_hash_function(a_void_ptr	key);
typedef a_hash_function
		*a_hash_function_ptr;


/*
The type of a function used to compare a key with a value from the hash
table.  Return TRUE if they match.
*/
typedef a_boolean a_hash_compare_function(a_void_ptr	entry,
					  a_void_ptr	key);
typedef a_hash_compare_function
		*a_hash_compare_function_ptr;

/*
The type returned by hash_find.  This points to the data field of
a_hash_table_entry.
*/
typedef a_void_ptr
		a_hash_data_ptr;


/*
A general-purpose hash table.
*/
typedef struct a_hash_table {
  a_function_number
		hash_function_index;
			/* An index to a function pointer (of type
			   a_hash_function_ptr) used to produce a hash
			   value for a key. */
  a_function_number
		compare_function_index;
			/* An index to a function pointer (of type
			   a_hash_compare_function_ptr) used to compare a key
			   with an entry in the hash table. */
  a_memory_region_number
		memory_region;
			/* The memory region in which hash table entries are
			   to be allocated.  If the value is
			   NO_MEMORY_REGION_NUMBER, the entries are to be
			   allocated in general memory. */
  a_hash_table_size
		num_buckets;
			/* The number of buckets in the hash table. */
  int32_t	entry_count;
			/* The number of entries in the hash table. */
  a_hash_table_entry_ptr
		*table;
			/* Pointer to the hash table array.  The size is
			   specified by num_buckets. */
} a_hash_table;

extern a_hash_table_ptr alloc_hash_table(
		a_memory_region_number		memory_region,
		a_hash_table_size		num_elements,
		a_function_number		hash_function_index,
		a_function_number		compare_function_index);

extern a_hash_data_ptr *hash_find(a_hash_table_ptr	table,
			    a_void_ptr		key,
			    a_boolean		create);

#if DEBUG
/* GCOVR_EXCL_START */
void db_hash_statistics(a_hash_table_ptr	table);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

extern a_hash_value hash_source_string(a_void_ptr  key);

void namespace_has_no_actual_member_error(a_symbol_locator	*locator);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_hash_value hash_prop_or_event_accessor_header_lookup(
						       a_void_ptr	key);

extern a_boolean compare_prop_or_event_accessor_header_lookup(
						       a_void_ptr	entry,
						       a_void_ptr	key);

extern a_boolean is_cppcx_externally_visible_symbol(a_symbol_ptr sym);

extern a_boolean is_unnamed_virtual_function_symbol(a_symbol_ptr sym);

extern void make_unnamed_virtual_function_locator(a_symbol_locator *loc);

#define is_cppcx_externally_visible_assembly_access(assembly_access)  \
  ((assembly_access) == (an_access_specifier)as_protected ||          \
   (assembly_access) == (an_access_specifier)as_public)

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_hash_value hash_token_sequence_xref(a_void_ptr	key);

extern a_boolean compare_token_sequence_xref(a_void_ptr	entry,
                                      a_void_ptr	key);

extern a_constexpr_if_cache_info_ptr check_constexpr_if_cache_hash_table(
					a_token_sequence_number	start_tsn);
					
extern void add_to_constexpr_if_cache_hash_table(
				a_constexpr_if_cache_info_ptr	cicip,
				a_token_sequence_number		start_tsn);
					
extern a_hash_value hash_symbol_header_lookup_entry(a_void_ptr	key);

extern a_boolean compare_symbol_header_lookup_entry(a_void_ptr	entry,
                                                    a_void_ptr	key);

extern a_symbol_ptr look_up_name_string_in_class(
                                        a_const_char             *symbol_name,
                                        a_type_ptr               class_type,
                                        an_id_lookup_options_set options);

extern a_symbol_ptr look_up_class_template_in_std(a_const_char  *ctname);

extern a_symbol_ptr look_up_name_string_in_namespace(
                                        a_const_char             *symbol_name,
                                        a_namespace_ptr          ns_ptr,
                                        an_id_lookup_options_set options);

extern a_boolean resolve_pending_trailing_requires_clause(a_symbol_ptr  sym);

extern a_requires_clause_ptr
        function_template_head_requires_clause(a_symbol_ptr sym);

inline a_boolean is_ineligible(a_symbol_ptr  sym)
/*
Return whether the function or member function associated with the given symbol
does not satisfy its trailing requires-clause.  If necessary, this substitutes
and evaluates the associated constraint.
*/
{
  a_boolean  result;

  if (sym->variant.routine.pending_trailing_requires_clause) {
    result = resolve_pending_trailing_requires_clause(sym);
  } else {
    result = func_sym_routine(sym)->is_ineligible;
  }  /* if */
  return result;
}  /* is_ineligible */


extern void check_for_constexpr_intrinsic(a_routine_ptr     rp,
                                          a_symbol_header  *sym_hdr);

extern int get_intrinsic_alias_templ_idx(a_symbol  *t_sym);

extern int get_intrinsic_var_templ_idx(a_symbol  *t_sym);

extern int get_intrinsic_templ_type_member_idx(a_symbol  *t_sym);

extern a_boolean intrinsic_templ_type_member_matches(
                                       int              idx,
                                       a_symbol_header  *member_hdr);

extern a_boolean intrinsic_templ_type_member_lookup(
                                       a_type_ptr       qualifier_type,
                                       a_symbol_header  *member_hdr,
                                       a_type_ptr       *result_tp,
                                       a_boolean        *no_such_member);

extern a_boolean is_intrinsic_type_transform_name(a_symbol_header  *hdr);

inline a_boolean is_intrinsic_type_transform_token(void)
/*
Return TRUE if the current token is an identifier matching a type transform
name.
*/
{
  a_boolean  result;

  if (curr_token == tok_identifier &&
      locator_for_curr_id.symbol_header->has_intrinsic_name) {
    result = is_intrinsic_type_transform_name(
                                           locator_for_curr_id.symbol_header);
  } else {
    result = FALSE;
  }  /* if */
  return result;
}  /* is_intrinsic_type_transform_token */


extern a_token_kind check_type_transform_name(void);

extern void symbol_tbl_one_time_init(void);

extern void symbol_tbl_trans_unit_init(void);

extern void symbol_tbl_init(void);

#if EXPENSIVE_CHECKING

extern void symbol_table_memory_region_wrap_up(a_memory_region_number region);

extern void symbol_table_trans_unit_validate();

#endif /* EXPENSIVE_CHECKING */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef SYMBOL_TBL_H */

