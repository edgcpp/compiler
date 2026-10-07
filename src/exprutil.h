/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

exprutil.h -- Declarations related to expression parsing.

*/

/* Avoid including these declarations more than once: */
#ifndef EXPRUTIL_H
#define EXPRUTIL_H 1

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */
#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */
#ifndef DECLS_H
#include "decls.h"
#endif /* ifndef DECLS_H */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/* Include of overload.h comes later. */

/*
Interfaces to add_stop_token and remove_stop_token to be used for matching
closing tokens, like ")" for "(".  These mark the beginning and end of
a nested construct.
*/
#define add_matching_stop_token(token)                                \
{ add_stop_token(token);                                              \
  expr_stack->nested_construct_depth++;                               \
}  /* add_matching_stop_token */
#define remove_matching_stop_token(token)                             \
{ remove_stop_token(token);                                           \
  expr_stack->nested_construct_depth--;                               \
}  /* remove_matching_stop_token */


/* Define the categories of expressions that are allowed. */
enum an_expression_kind : a_byte {
  /* Note that the constant expression kinds must appear at the beginning
     of the list so that one can determine if an expression is constant
     by a "<=" comparison instead of several "==" comparisons.  See
     curr_expr_kind_is_const below.  ek_init_constant must be the last constant
     expression kind. */
  ek_pp,		/* Preprocessing expression (see 3.8.1). */
  ek_integral_constant,	/* Integral constant expression (see 3.4). */
  ek_template_arg,	/* Nontype template argument (C++). */
  ek_init_constant,	/* Constant expression allowed in initializers (see
			   3.4).  Limited use in C++, except that in C++11
			   this implements "core constant expression". */
  /* Non-constant expression kinds: */
  ek_normal,		/* Normal expression, no restrictions. */
  ek_sizeof		/* The operand of sizeof.  This is almost the same
			   as a normal expression. */
};


/*
Kinds of expressions again, this time as bits in a set used to
record occurrence of constructs that rule out certain expression
kinds.  These are used for C and pre-C++11 C++; in C++11 different
rules apply, and a different mechanism is used (see
constant_expr_ruled_out in the expression stack).
*/
typedef a_byte a_ruled_out_expr_kind_set;
#define ROEK_NONE              ((a_ruled_out_expr_kind_set)0)
#define ROEK_INTEGRAL_CONSTANT ((a_ruled_out_expr_kind_set)0x1)
			/* An integral constant expression is ruled out. */
#define ROEK_CONSTANT          ((a_ruled_out_expr_kind_set)0x2)
			/* A constant expression is ruled out. */

/*
Kinds of source forms for casts.
*/
enum a_cast_source_form {
  csf_none,
  csf_old_style,	/* Old-style cast, (T)x */
  csf_functional,	/* Functional-notation cast, T(x) */
  csf_static_cast,	/* static_cast<T>(x) */
  csf_const_cast,	/* const_cast<T>(x) */
  csf_reinterpret_cast,	/* reinterpret_cast<T>(x) */
  csf_safe_cast,	/* safe_cast<T>(x) -- C++/CLI mode. */
  csf_dynamic_cast	/* dynamic_cast<T>(x) */
};


/*
The context kinds in which overload resolution functions like
select_overloaded_function and select_and_prepare_to_call_overloaded_function
are called.  This determines what kind of error messages might be issued when
overload resolution fails.  In some cases it can also decide other aspects of
the call resolution (such as lookup).
*/
enum an_overload_context {
  oc_default,                 /* Ordinary function or member function calls
                                 (e.g., scan_function_call). */
  oc_deferred,                /* A syntactically-ordinary call in a template
                                 context whose resolution should be deferred
                                 until instantiation.  This is used to
                                 emulate a Microsoft bug. */
  oc_constructor,             /* Calls to constructors (see
                                 scan_ctor_arguments). */
  oc_new_expression,          /* Calls to allocation functions generated for
                                 new-expressions. */
  oc_range_based_for_bounds,  /* Calls generated to determine range-based
                                 "for" loop bounds. */
  oc_for_each_bounds,         /* Calls generated to determine Microsoft C++
                                 "for each" loop bounds. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  oc_cppcx_for_each_bounds,   /* Calls generated to determine Microsoft C++/CX
                                 "for each" loop bounds. */
  oc_property_access,          /* See rewrite_property_reference. */
  oc_event_access,             /* See rewrite_event_operator. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  oc_synthesized_member_call,  /* See call_named_member_function. */
  oc_ctad,                     /* See deduce_class_template_args. */
  oc_tuple_like_binding,       /* "get<N>" for tuple-like binding. */
  oc_reversed_cmp_candidate,   /* Set when checking the viability of a
                                  "reversed" comparison operator candidate. */
  oc_conv_to_class_check,      /* Set when doing constructor overload
                                  resolution in a call to function
                                  conversion_to_class_possible. */
  oc_multi_subscript,          /* A C++23 multi-subscript call. */
  oc_call_through_address_of_overload_set,
                               /* A call of the form "(&func)(args...)".  In
                                  this context, no implied object argument is
                                  is added, static member functions do not get
                                  an implicit object parameter, and if the
                                  selected function is an ordinary nonstatic
                                  member, the program is ill-formed. */
  oc_last
};


#if BUILTIN_FUNCTIONS_ENABLED

/*
These values are returned by __builtin_classify_type.  Order matters;
the values of these enumeration constants must match those used by
GCC.
*/
enum a_type_class_kind {
  tck_none = -1,
  tck_void,             /* void */
  tck_integer,          /* short, int, long, long long */
  tck_char,             /* char (currently unused) */
  tck_enum,             /* enumeration types */
  tck_bool,             /* bool */
  tck_pointer,          /* pointers */
  tck_reference,        /* references */
  tck_offset,           /* pointer to data member */
  tck_float,            /* float, double, long double */
  tck_complex,		/* float complex, double complex, long double
			   complex */
  tck_routine,          /* functions */
  tck_method,           /* Unused in C or C++. */
  tck_struct,           /* structs or classes */
  tck_union,            /* unions */
  tck_array,            /* arrays */
  tck_string            /* strings (currently unused) */
/* Unused type classes:
  tck_set
  tck_file
  tck_lang
*/
};

#endif /* BUILTIN_FUNCTIONS_ENABLED */

/*
Information on a single reference to a symbol.  Used in cases where the kind
of reference can be affected by context and therefore cannot be recorded
immediately.  This entry holds the information that must be retained to be
able to record the reference once the kind of reference is known.
*/
typedef struct a_ref_entry *a_ref_entry_ptr;
typedef struct a_ref_entry {
  /* Describes one reference to a symbol. */
  a_symbol_reference_kind
		kind;
			/* Kind of reference (modification, address taken,
			   etc.). */
  a_byte_boolean
		already_recorded;
			/* TRUE if the reference has already been recorded.
			   Used when modifications are recorded at potential
			   sequence points.  The modification is recorded, but
			   the symbol pointer has to be kept around in case
			   the address of the entity is taken for a reference
			   parameter to an overloaded operator function. */
#if CHECKING
  a_byte_boolean
		freed;
			/* TRUE if the entry has been freed and is on the
			   available list. */
#endif /* CHECKING */
  a_symbol_ptr	symbol;	/* Pointer to the referenced symbol. */
  a_source_correspondence
		*specific_il_entry;
			/* If non-NULL, a pointer to a specific IL entry
			   to be used in place of the one pointed to by the
			   symbol.  Used for variadic template pack elements,
			   where the symbol may be changed to point to a
			   different IL entry between the time the ref entry
			   is created and when the symbol reference is
			   recorded. */
  a_source_position
		position;
			/* Source location of the reference. */
  a_ref_entry_ptr
		next;
			/* Next entry on the list of reference entries
			   for the current expression, NULL if last. */
  a_ref_entry_ptr
		next_operand_ref;
			/* Next entry on the list of reference entries
			   that apply to one operand, NULL if last. */
} a_ref_entry;

/* Kinds of operands: */
enum an_operand_kind : a_byte {
  ok_error,		/* Error. */
  ok_expression,	/* An expression tree. */
  ok_constant,		/* A constant value. */
  ok_indefinite_function,
			/* A function name that's not fully discriminated yet,
			   i.e., a C++ overloaded function.  With state ==
			   os_function_designator, represents the function
			   itself and has very limited lifetime.  With state ==
			   os_prvalue, represents the address of an overloaded
			   function, and survives until it meets a destination
			   type (which selects a specific function) or until
			   used in some other way (which is an error).  Not
			   used in C. */
  ok_sym_for_member,	/* A symbol for a nonstatic data member or nonstatic
			   member function.  Represented in this way because
			   it's not clear yet how the member is being used --
			   its address might be taken (e.g., "&A::f"), it
			   might get bound to an object and called (e.g.,
			   "p->f(1)"), etc.  Not used for overloaded
			   functions (ok_indefinite_function is used
			   instead).  Used only in C++. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  ok_property_ref,	/* A reference to a Microsoft property member
			   of a class (traditional or managed). */
  ok_event_ref,		/* A reference to a Microsoft event member of a
			   managed class. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  ok_braced_init_list,	/* A C++11 brace-enclosed list.  Note that this is
			   used only in somewhat unusual situations, as most
			   braced-init-lists are scanned as part of an
			   expression list and are represented by
			   an_init_component (aka an_arg_list_elem). */
  ok_undefined_symbol	/* An undefined symbol encountered while scanning an
			   expression.  Could be an implicit function
			   declaration or a genuine undefined symbol.
			   Has a very limited lifetime. */
};


/*
Operand states (lvalue versus rvalue, etc.).  In C++, glvalues encompass
lvalues and xvalues, and traditional C rvalues are called prvalues.
In C mode, interpret "glvalue" as "lvalue" and "prvalue" as "rvalue".
A "function designator" is an expression of function type; in C++, those
are lvalues, but is_an_lvalue does not indicate that (see function
is_a_cplusplus_lvalue instead).
*/
enum an_operand_state : a_byte {
  os_none,
  os_glvalue,
  os_prvalue,
  os_function_designator
};


/* Data structure used to represent an expression within the expression
   routines: */
typedef struct an_operand {
  /*lint -e{1401}*/
  inline an_operand() {}
  inline an_operand(const an_operand  &src) { this->copy_from(&src); }
  /*lint -e{1529}*/
  inline an_operand& operator=(const an_operand  &src)
    { this->copy_from(&src); return *this; }
  inline void copy_from(an_operand const  *src);

  a_type_ptr    type;
			/* Type of this operand.  A tk_unknown type if not
			   applicable (ok_indefinite_function,
			   ok_undefined_symbol). */
#if OPTIMIZE_VIRTUAL_FUNCTION_CALLS
  a_type_ptr	orig_routine_type;
			/* If this operand is an ok_constant designating a
			   routine that is an overrider of the
			   statically-chosen routine (because the type of
			   the complete object is known and the virtual
			   call has been optimized to a direct call of the
			   overrider), this is the type of the overridden
			   function; NULL in all other cases. */
#endif /* OPTIMIZE_VIRTUAL_FUNCTION_CALLS */
  an_operand_kind
		kind;
			/* The kind of operand. */
  an_operand_state
		state;
			/* Whether the operand is an lvalue, rvalue, or a
			   function designator. */
  a_bit_field	bound_function:1;
			/* TRUE if the operand is a bound function, i.e.,
			   another operand is required to give the object
			   relative to which this function is selected. */
  a_bit_field	selector_is_object_pointer:1;
			/* On the operand bound to one with bound_function
			   TRUE, this flag indicates that the source form
			   that created the bound function was "->" or "->*"
			   rather than "." or ".*".  In other words, it
			   indicates that the selector is an object pointer
			   rather than a class lvalue or rvalue.  Also used
			   on operands that are intended to be used as
			   selectors before they get bound to anything. */
  a_bit_field	virtual_function:1;
			/* TRUE if the operand represents a function that
			   should be called as a virtual function (e.g.,
			   FALSE for a virtual function referenced using
			   a qualified name). */
  a_bit_field   is_id_expression:1;
			/* TRUE if this operand was generated from an
			   id-expression (a qualified or unqualified name).
			   TRUE even if the id-expression is parenthesized;
			   use the id_expression_was_parenthesized flag to
			   exclude that case. */
  a_bit_field   is_address_of_id_expression:1;
			/* TRUE if this operand was generated from an
			   id-expression prefixed with an ampersand ("&").
			   TRUE even if the id-expression is parenthesized;
			   use the id_expression_was_parenthesized flag to
			   exclude that case. */
  a_bit_field   id_expression_was_parenthesized:1;
			/* TRUE if this operand was generated from a
			   parenthesized id-expression. */
  a_bit_field	is_qualified_name:1;
			/* TRUE if the operand was generated from a qualified
			   name. */
  a_bit_field	is_microsoft_deferred_name:1;
			/* TRUE if the operand represents a name that would
			   normally not be considered template-dependent, but
			   whose resolution the Microsoft compiler defers until
			   template instantiation.*/
  a_bit_field	access_control_error_reported:1;
			/* TRUE if an access control error was reported
			   on the base identifier for this operand.  This
			   remains meaningful only for operands that are
			   essentially still just a representation for
			   an identifier, e.g., ok_indefinite_function and
			   ok_sym_for_member. */
  a_bit_field	designated_by_splice:1;
			/* TRUE if this operand was generated from a
			   splice-expression designating a class member.
			   Such a member is accessible from any point, and
			   the additional protected member access check
			   ([class.protected]) does not apply to it.  Like
			   access_control_error_reported, this remains
			   meaningful only for ok_sym_for_member
			   operands. */
  a_bit_field	is_operand_of_address_of:1;
			/* TRUE if this operand is the immediate operand
			   of an "&" address-of operator.  This is
			   used on ok_indefinite_function operands to remember
			   that a "&" appeared that can't yet be represented
			   in the IL, because we don't know yet what the
			   underlying entity will be.  Also used on some
			   constant operands that represent indefinite
			   functions; see operand_allows_is_operand_address_of.
			   When this is TRUE, the ampersand_position field
			   gives the position of the "&" (the normal position
			   field gives the position of the underlying
			   identifier).  Note that "immediate operand" does
			   not preclude parentheses in the source form,
			   as in &(A::f); see also
			   has_required_ptr_to_member_form. */
  a_bit_field	has_required_ptr_to_member_form:1;
			/* Similar to is_operand_of_address_of, but indicates
			   that the operand has the correct form for a
			   pointer-to-member constant, e.g., no parentheses.
			   TRUE only when is_operand_of_address_of is TRUE.
			   Doesn't include whether the operand is a qualified
			   name; see is_qualified_name. */
  a_bit_field	is_template_id:1;
			/* TRUE if an explicit template argument list
			   applies to variant.symbol.  template_arg_list
			   gives the argument list. */
  a_bit_field	is_simple_string_literal:1;
			/* TRUE if this operand is a simple string literal
			   or wide string literal, or the result of the
			   decay of such a literal to a pointer.  FALSE
			   for a string literal that has been subjected to
			   a cast or other operation. */
  a_bit_field	conditional_simple_string_literal:1;
			/* TRUE if this operand represents a conditional
			   operand of the form "x ? y : z" where either y or z
			   is a simple string literal.  This is used to emulate
			   a Microsoft bug. */
  a_bit_field	is_cfront_null_pointer_constant:1;
			/* TRUE if this operand is a simple 0 which is
			   suitable as a null pointer constant in
			   overload resolution in cfront mode. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	is_microsoft_noop:1;
			/* TRUE if the operand came from a Microsoft __noop.
			   This is used to suppress the warning that the
			   code has no effect. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_bit_field	is_name_followed_by_left_paren:1;
			/* TRUE if this is a name followed by a left
			   parenthesis.  Needed to know if argument-dependent
			   lookup applies, and also for C++/CLI hide-by-sig
			   lookup. */
  a_bit_field	is_dummy_lvalue:1;
			/* TRUE if this operand was created by
			   make_dummy_lvalue_operand.  It could be a dummy
			   xvalue, in spite of the name. */
  a_bit_field	is_parenthesized:1;
			/* TRUE if the operand is enclosed in one or more sets 
			   of parentheses. */
  a_bit_field	name_reference_set:1;
			/* TRUE if name_reference has been set. */
  a_bit_field	caused_template_instantiation:1;
			/* TRUE if parsing this operand caused a template
			   to be instantiated. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	allow_addr_of_managed_member:1;
			/* TRUE if this operand is for a function scanned in
			   a C++/CLI context where it's okay to take the
			   address of a member of a managed class. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_bit_field	pending_capture:1;
			/* TRUE for an enk_variable expression that should
			   potentially be rewritten as a field selection
			   because the variable being referred to must be
			   captured.  This cannot be decided when the node is
			   created because constant-valued variables used as
			   prvalues only don't need to be captured, and we
			   don't know that it is used as a prvalue until
			   later. */
  a_name_reference
		name_reference;
			/* Records the form of reference to a name, for
			   some operands that represent named entities
			   (e.g., a function name).  Valid when
			   name_reference_set is TRUE. */
  a_ruled_out_expr_kind_set
		ruled_out_expr_kinds;
			/* Bits indicating kinds of expressions ruled out.
			   For example, if the expression contains something
			   not allowed in an integral constant expression,
			   the roek_integral_constant bit will be set.
			   Of use in checking after a scan whether an
			   expression scanned in default mode meets the
			   requirements of a certain expression kind.
			   Not used for C++11 constant expressions. */
  a_source_position
		position;
			/* The source position for the operand. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position
		end_position;
			/* The source position of the end of the operand. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_ref_entry_ptr
		ref_entries_list;
			/* A list of reference entries that are related
			   to the operand.  This list includes only entries
			   for functions and for entities involved in lvalue
			   address computations, whose reference kinds might
			   be changed once the full context surrounding the
			   operand is known.  Note that this list does
			   sometimes have meaning in an operand that is an
			   rvalue, e.g., for an array name that has decayed
			   to a pointer. */
  a_ref_entry_ptr
		saved_ref_entries_list;
			/* Saved list of reference entries used for cases
			   where an rvalue can be turned back into an
			   lvalue, e.g., in gcc mode. */
  a_template_arg_ptr
		template_arg_list;
			/* When is_template_id is TRUE, a template argument
			   list to be applied to variant.symbol. */
  a_source_position
  		id_position;
			/* Extra source position for an identifier, used
			   for ok_indefinite_function, ok_undefined_symbol,
			   and ok_property_ref.  If the name is "X::f",
			   this gives the position of "f", where the field
			   position above gives the position of the "X". */
  a_source_position
		ampersand_position;
			/* When is_operand_of_address_of is TRUE, this gives
			   the position of the "&" operator. */
  a_pack_expansion_descr_ptr
		pack_expansion_descr;
			/* If non-NULL, this operand is a pack expansion
			   (it is followed by "...", as in "T()..."), and this
			   points to the expansion description. */
  /* When kind == ok_indefinite_function, ok_sym_for_member, ok_property_ref,
     ok_event_ref, or ok_undefined_symbol, and also ok_expression for
     the enk_field case: */
  a_symbol_ptr
		symbol;
			/* Pointer to the symbol.  May be a projection
			   symbol for ok_indefinite_function,
			   ok_sym_for_member, or ok_expression/enk_field. */
  /* The following "variant" union must be the last member of an_operand.
     The member function an_operand::copy_from relies on it. */
  union {
    /* When kind == ok_error, ok_indefinite_function, ok_sym_for_member, or
       ok_undefined_symbol: No variant fields. */
    /* When kind == ok_expression: */
    an_expr_node_ptr
		expression;
    /* When kind == ok_constant: */
    a_constant	constant;
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* When kind == ok_property_ref: */
    /* This is used for a reference to a class member declared with the
       Microsoft C++ extension __declspec(property(...)) or with the C++/CLI
       property syntax. */
    struct {
      an_expr_node_ptr
		object;	/* Expression for the class object pointer.  For a
			   reference to a C++/CLI static property, can be NULL
			   because there's no object, and if it is non-NULL
			   indicates an object that is evaluated and discarded
			   (it won't take part in the overload resolution
			   used to select the accessor). */
      an_arg_list_elem_ptr
		subscripts;
			/* Optional list of subscript expressions, for cases
			   like p->x[y][z]. */
    } property_ref;
    /* When kind == ok_event_ref: */
    struct {
      an_expr_node_ptr
		object;	/* Expression for the class object pointer.  For a
			   reference to a C++/CLI static event, can be NULL
			   because there's no object, and if it is non-NULL
			   indicates an object that is evaluated and discarded
			   (it won't take part in the overload resolution
			   used to select the accessor). */
    } event_ref;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* When kind == ok_braced_init_list: */
    an_arg_list_elem_ptr
		braced_init_list;
			/* Pointer to a brace-enclosed list.  The entry pointed
			   to will always be a single entry (not a list)
			   of type ick_braced, representing a brace-enclosed
			   list.  Note that this kind of operand is used only
			   in some unusual situations, as most
			   braced-init-lists are scanned as part of an
			   expression list and are represented directly
			   by an_init_component (aka an_arg_list_elem). */
  } variant;
} an_operand;


inline void an_operand::copy_from(an_operand const  *src)
/*
Shallowly copy the given operand to *this.  (For a "deep copy", see
clone_operand.)  This function is used by the copy constructor and the
copy assignment operator.
*/
{
  /* Copy everything but the variant. */
  memcpy((char*)this, (char*)src, offsetof(an_operand, variant));
  /* For the variant, only copy the active field. */
  switch (src->kind) {
    case ok_expression:
      this->variant.expression = src->variant.expression;
      break;
    case ok_constant:
      this->variant.constant = src->variant.constant;
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case ok_property_ref:
      this->variant.property_ref = src->variant.property_ref;
      break;
    case ok_event_ref:
      this->variant.event_ref = src->variant.event_ref;
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case ok_braced_init_list:
      this->variant.braced_init_list = src->variant.braced_init_list;
      break;
    default:
      /* Nothing to do. */
      break;
  }  /* switch */
}  /* copy_from */


EXTERN_THREAD an_operand_ptr
		*internal_opnd_array;
			/* Pointer to an array of operands available when
			   scanning internal expressions. */

EXTERN_THREAD int
		n_internal_opnds;
			/* Length of the array pointed to by
			   internal_opnd_array. */

/*
Entry describing an expression, in a form that can be dynamically allocated
and linked onto a list.
*/
/* The typedef an_arg_operand_ptr is defined in il_def.h. */
typedef struct an_arg_operand {
  an_arg_operand_ptr
		next;	/* Pointer to the next argument, or NULL if this is the
			   last argument. */
  an_operand	operand;
			/* The argument value. */
} an_arg_operand;


/*
Entry used to attach front-end-only information to an_expr_node (or a_constant
or a_dynamic_init) for use when the expression is rescanned to do semantic
analysis as part of template deduction.
*/
/* The typedef an_expr_rescan_info_entry_ptr is defined in il_def.h. */
typedef struct an_expr_rescan_info_entry {
  an_operand	saved_operand;
			/* A copy of the operand, which contains the position
			   information and all the extra flags.  Not used for
			   the basic type/expression/constant values.  This
			   must be the first field (relied upon by function
			   record_cast_position_in_expr_rescan_info). */
  an_expression_kind
		expression_kind;
			/* Kind of expression we are in, e.g., template
			   argument. */
  a_source_position
		operator_position;
			/* Position of the expression operator, if there is
			   one; null_source_position otherwise. */
  a_token_sequence_number
		operator_token_sequence_number;
			/* Token sequence number for the operator, if there
			   is one.  NO_TOKEN_SEQUENCE_NUMBER otherwise.
			   Not set for operators that can't be overloaded. */
  a_source_position
		secondary_operator_position;
			/* Position of a second operator, e.g., the closing
			   "]" of a subscript or the ":" of a "?" operator.
			   Also used for the type position for a cast. */
  a_type_ptr	type;
			/* For a cast, the type cast to.  For a "new", the
			   type being allocated.  Otherwise, NULL. */
} an_expr_rescan_info_entry;

/*
Copy the source position from an expression operand into an expression node.
*/
#if EXTRA_SOURCE_POSITIONS_IN_IL
#define copy_operand_position_to_expr(operand, node) \
  { (node)->expr_range.start = (operand)->position; \
    (node)->expr_range.end = (operand)->end_position; \
    (node)->position = (operand)->position; }
#else /* !EXTRA_SOURCE_POSITIONS_IN_IL */
#define copy_operand_position_to_expr(operand, node) \
  ((node)->position = (operand)->position)
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
/*
Copy the source position from one operand into another.
*/
#if EXTRA_SOURCE_POSITIONS_IN_IL
#define copy_operand_position(source_operand, dest_operand) \
  {(dest_operand)->position = (source_operand)->position; \
   (dest_operand)->end_position = (source_operand)->end_position;}
#else /* !EXTRA_SOURCE_POSITIONS_IN_IL */
#define copy_operand_position(source_operand, dest_operand) \
  {(dest_operand)->position = (source_operand)->position;}
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

/*
Used to pass addresses of end positions as call arguments.  Substitutes
the address of null_source_position in configurations that don't have extra
source positions.
*/
#if EXTRA_SOURCE_POSITIONS_IN_IL
#define end_position_or_null(end_position) (end_position)
#else /* !EXTRA_SOURCE_POSITIONS_IN_IL */
#define end_position_or_null(end_position) (&null_source_position)
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

/*
Get the end position address from an operand if there is one, otherwise return
the address of null_source_position.
*/
#define end_position_of_operand(operand) \
  end_position_or_null(&(operand)->end_position)


/*
Get the expression node, if any, associated with an operand.  A direct
backing expression in a constant operand is given preference over a
template parameter expression if the constant is a ck_template_param
constant.  This just fetches an existing expression node, or returns NULL;
it never creates one.
*/
#define expr_node_from_operand(operand)                                       \
  (is_expression_operand(operand) ?                                           \
     (operand)->variant.expression :                                          \
   is_constant_operand(operand) && (operand)->variant.constant.expr != NULL ? \
     (operand)->variant.constant.expr :                                       \
   is_template_param_expression_constant_operand(operand) ?                   \
     expr_node_from_tpck_expression(&(operand)->variant.constant) :           \
     NULL)

/*
Return TRUE if we are in a context in which operations, such as
access checking, are required to produce immediate results (and so are
allowed even in deferred access contexts).
*/
#define in_expr_testing_context() \
	  (scope_stack_top().is_rescan || \
	   (expr_stack != NULL && expr_stack->suppress_diagnostics)) 

/*
Bit flags used in calling do_operand_transformations, to suppress
some of the transformations.
*/
#define TOPT_SUPPRESS_FUNCTION_TO_POINTER_CONVERSION 0x1
			/* Functions should not be converted implicitly to
			   pointer-to-function. */
#define TOPT_SUPPRESS_ARRAY_TO_POINTER_CONVERSION 0x2
			/* Arrays should not be converted implicitly to
			   pointer-to-first-element-of-the-array. */
#define TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION 0x4
			/* Suppress conversion of an lvalue to an rvalue.
			   (Really, a glvalue to a prvalue, but like the
			   C++11 standard we keep the traditional
			   terminology.) */
#define TOPT_SUPPRESS_CHECK_FOR_INDEFINITE_FUNCTION 0x8
			/* Suppress the check for indefinite functions. */
#define TOPT_WILL_CALL 0x10
			/* The operand is the function being called in a
			   call. */
#define TOPT_SUPPRESS_MEMBER_FUNC_TO_PM_CONVERSION 0x20
			/* Member functions should not be converted implicitly
			   to pointer-to-member. */
#define TOPT_SUPPRESS_RVALUE_PROPERTY_REWRITE 0x40
			/* References to class members that are Microsoft
			   properties should not be rewritten as calls of
			   the appropriate "get" function (yet). */
#define TOPT_COPY_CLASS_ON_CONV_TO_RVALUE 0x80
			/* When converting a C++ class to an rvalue, make
			   a copy.  This is required by the C++ standard, but
			   it actually applies only in certain unusual
			   situations, so we leave the default as no copy. */
#define TOPT_NO_OPTIONS 0
typedef int a_transformation_options_set;

extern a_boolean expr_error_should_be_issued(void);

/* Include overload.h after an_operand has been defined to avoid circular
   reference problems. */
#ifndef OVERLOAD_H
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
#include "overload.h"
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */
#endif /* ifndef OVERLOAD_H */

/*
Entry used to record information about a dynamic init entry whose
destructor processing could not be completed when the dynamic init entry
was created because the initialization occurs within the return expression
in a routine that returns its value via a copy constructor.  The destructor
call on the topmost initialization is optimized away, but there's no way to
know that when it is generated, so all such initializations are placed on
a list and checked at the end of the expression.
*/
typedef struct a_dynamic_init_dtor_fixup *a_dynamic_init_dtor_fixup_ptr;
typedef struct a_dynamic_init_dtor_fixup {
  a_dynamic_init_dtor_fixup_ptr
		next;	/* Next entry on the list. */
  a_dynamic_init_ptr
		dynamic_init;
			/* The dynamic initialization entry to be checked. */
  a_source_position
		position;
			/* The source position for errors. */
} a_dynamic_init_dtor_fixup;


/* an_initializer_cache is in decls.h because of header ordering reasons. */
typedef struct an_initializer_cache an_initializer_cache_dummy_typedef;


EXTERN_THREAD a_routine_ptr
		*p_called_nonconstexpr_routine;
			/* Pointer to the location where a non-constexpr
			   routine should be recorded if it is called (when
			   the flag record_first_nonceonstexpr_call is TRUE
			   in the expression stack). */

/*
Entry in a stack used during expression processing to record transitions
into substantially disjunct pieces of the expression (e.g., inside a
sizeof is quite different from a normal expression).  These entries are
allocated in the stack.
*/
typedef struct an_expr_stack_entry *an_expr_stack_entry_ptr;
typedef struct an_expr_stack_entry {
  an_expr_stack_entry_ptr
		prev;	/* Previous entry on the stack */
  a_ref_entry_ptr
		old_ref_entries_list;
			/* Saved copy of the global reference entries list
			   at the time of the push of this entry. */
  an_expression_kind
		expression_kind;
			/* The kind of expression. */
  a_bit_field
		evaluated:1;
			/* Expression is evaluated, e.g., FALSE if it's the
			   operand of a sizeof or in a "dead" part of a
			   short-circuiting operation. */
  a_bit_field
		potentially_evaluated:1;
			/* Expression is potentially evaluated, e.g., FALSE
			   if it's the operand of a sizeof.  Always TRUE in
			   a constant expression, even one inside a not-
			   evaluated expression. */
  a_bit_field
		potentially_unevaluated:1;
			/* Expression may be an unevaluated operand, i.e.,
			   TRUE for operands of sizeof and similar
			   operators and for the operand of typeid, which
			   is evaluated for polymorphic lvalues and
			   unevaluated for all other operands.  Determines
			   whether an objectless reference to a nonstatic
			   data member is allowed. */
  a_bit_field
		objectless_nonstatic_data_ref_seen:1;
			/* Initially FALSE, set to TRUE if an objectless
			   reference to a nonstatic data member occurs in a
			   context that permits such constructs. */
  a_bit_field
		potentially_unevaluated_lambda_seen:1;
			/* Initially FALSE, set to TRUE if a lambda definition
			   occurs in a potentially unevaluated context
			   (e.g., the operand of a typeid). */
  a_bit_field
		is_type_operator_arg_expression:1;
			/* TRUE if the expression is the argument for a C++11
			   decltype construct, a GNU typeof construct, or an
			   __underlying_type construct. */
  a_bit_field
		is_default_arg_expression:1;
			/* TRUE if the expression is or is inside of a
			   C++ default argument expression in a parameter
			   list. */
  a_bit_field
		is_initial_default_arg_scan:1;
			/* TRUE the first time a default argument expression is
			   encountered, FALSE on subsequent occurrences. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field
		is_cli_attr_arg_expression:1;
			/* TRUE if the expression is or is inside of a
			   C++/CLI attribute argument expression. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_bit_field
		is_template_arg_expression:1;
			/* TRUE if the expression is an argument of a C++
			   template reference.  This is TRUE only for the
			   top level major expression for a template
			   argument, e.g., it's not TRUE inside a sizeof
			   inside a template argument.  This flag is mainly
			   about recognizing ">" as a template closing
			   bracket. */
  a_bit_field
		is_vla_dimension_expression:1;
			/* TRUE if the expression is the dimension of a
			   VLA (variable-length array).  This is TRUE only
			   for the top level major expression for a VLA
			   dimension, e.g., it's not TRUE inside a sizeof
			   inside a VLA dimension. */
  a_bit_field
		in_cctor_elision_initializer:1;
			/* TRUE if the expression is or is inside of an
			   initializer expression that is subject to
			   the copy constructor elision optimization,
			   for example in a return expression in a
			   routine that returns its value to the caller by
			   calling a copy constructor.  This has an effect
			   on destructor calls noted in dynamic initialization
			   entries for temporaries.  See
			   fix_up_dynamic_init_dtors. */
  a_bit_field
		favor_constant_result:1;
			/* TRUE if a constant result should be produced
			   if possible.  Always TRUE if the expression
			   is a constant expression; sometimes TRUE for
			   nonconstant expressions (e.g., initializer
			   expressions, where it helps in discerning static
			   initialization cases from others).  Among other
			   things, this controls whether constant addressing
			   expressions should be folded to constants. */
  a_bit_field
		inside_conditional_expression:1;
			/* TRUE if inside a conditional operand of an
			   operator like "?". */
  a_bit_field
		unevaluated_expr_will_be_kept_in_il:1;
			/* TRUE if the expression will be kept in the IL even
			   though it is a not-evaluated expression
			   (specifically, one with potentially_evaluated
			   set to FALSE).  Normally, such expressions are
			   discarded. */
  a_bit_field
		template_deduction_context:1;
			/* TRUE if we're currently redoing semantic analysis
			   on an expression as part of template deduction. */
  a_bit_field
		suppress_diagnostics:1;
			/* TRUE if diagnostics should be suppressed in the
			   current context.  This is set when an expression
			   is rescanned in a template deduction (SFINAE)
			   context. */
  a_bit_field
		any_suppressed_error:1;
			/* TRUE if any error was detected and suppressed
			   because suppress_diagnostics is TRUE.  Access
			   errors may or may not set this flag depending on
			   cpp11_sfinae_ignore_access. */
  a_bit_field
		possible_rescan_context:1;
			/* TRUE if the expression being scanned is in a context
			   where it might have to be rescanned later, e.g.,
			   for template deduction.  Extra information is saved
			   that will be needed to redo the semantic analysis
			   on the expression.  Only set when
			   cpp11_sfinae_enabled is TRUE. */
  a_bit_field
		in_static_initializer:1;
			/* TRUE if we're inside the initializer of an entity
			   with static storage duration. */
  a_bit_field
		next_stack_push_considered_same_expression:1;
			/* Signal to transfer_expr_context_if_applicable that
			   the next expression stack push should consider that
			   we are still in the same expression even if we went
			   into declaration processing and came back. */
  a_bit_field
		record_first_nonconstexpr_call:1;
			/* TRUE if the first encountered call to a non-
			   constexpr function should be recorded (in
			   *p_called_nonconstexpr_routine). */
  a_bit_field
		constant_expr_ruled_out:1;
			/* TRUE if the expression contains something that
			   rules it out as a constant expression, even
			   if the expression is not being scanned as a
			   constant expression.  In C++11, this is the
			   only thing used to track adherence to the
			   restrictions of constant expressions.  In
			   other modes, the ruled_out_expr_kinds set in
			   an operand is also maintained. */
  a_bit_field
		traditional_const_expr_required:1;
			/* TRUE if we're in a constant expression to be
			   processed with the pre-C++11 rules for constant
			   expressions.  Those are (1) disallowed operators,
			   types, and constructs are flagged as invalid
			   immediately when they appear, even in unevaluated
			   subexpressions; (2) each operation/conversion must
			   immediately fold to a constant result, except when
			   unevaluated (e.g., 1 || 1/0 is okay). */
  a_bit_field
		in_noexcept_operand_expression:1;
			/* TRUE if we're in the operand of a noexcept
			   operator. */
  a_bit_field
		suppress_constexpr_call_folding:1;
			/* TRUE to suppress folding of constexpr calls and
			   constructions.  Used for
			   __is_trivially_constructible. */
  a_bit_field
		consteval_call_need_not_fold:1;
			/* When TRUE, a call to a consteval function is not
			   required to fold to a constant.  Used for consteval
			   calls in default arguments in an immediate
			   ("consteval") context. */
  a_bit_field
		consteval_function_designator_seen:1;
			/* When TRUE, a function designator for a consteval
			   function was seen. */
  a_bit_field
		allow_call_with_incomplete_return_type:1;
			/* TRUE if a call with an incomplete return type should
			   be permitted at the top level of an expression.
			   (This is only the case for the operand of a
			   decltype construct.) */
  a_bit_field
		allow_array_decay_in_constant_expr:1;
			/* TRUE if an array-to-pointer conversion involving
			   a variable with automatic storage duration
			   should be permitted in a constant expression.
			   This is to emulate gcc, which allows expressions
			   like a == a, even when the address of a is not
			   constant. */
  a_bit_field
		uses_this_operand:1;
			/* Set to TRUE when make_this_variable_operand is
			   called.  Used to emulate a Clang/GCC bug. */
  a_bit_field
		nodiscard_expr_seen:1;
			/* Set to TRUE when a function call with the nodiscard
			   attribute or a call that returns a type with the
			   nodiscard attribute has been seen. */
  a_bit_field
		marked_as_gnu_extension:1;
			/* Set to TRUE when the expression is prefixed with
			   the GNU __extension__ keyword.  In that case, some
			   diagnostics are inhibited. */
  a_bit_field
		fold_prvalue_if_possible:1;
			/* Set to TRUE when conv_glvalue_to_prvalue should
			   attempt to constant-fold the prvalue (which might
			   affect whether variables involved are "odr-used"
			   or not). */
  a_bit_field
		in_call_argument:1;
			/* TRUE when parsing and processing an explicit call
			   argument (not when parsing a default argument in a
			   function declaration). */
  a_bit_field
		in_coroutine_desc_init:1;
			/* TRUE if we're in the process of building the
			   coroutine descriptor. */
  a_bit_field
		paren_as_aggregate_init:1;
			/* Set to true when a braced initializer component has
			   been created because a parenthesized expression-list
			   is being treated as aggregate initialization. */
  a_bit_field
		expr_will_be_discarded:1;
			/* TRUE if scanning an expression whose result will be
			   discarded (e.g., a void expression). */
  a_bit_field
		statement_expression_seen:1;
			/* TRUE if a statement expression was seen in the
			   current expression. */
  a_bit_field	likely_not_evaluated:1;
			/* TRUE while scanning an expression that is likely
			   not evaluated.  In particular, this is TRUE when
			   scanning X in "0 && X" or "1 || X" even when an
			   overloaded operator && or || exists. */
  a_bit_field	trace_unevaluated_lambdas:1;
			/* TRUE if an unevaluated lambda should be accepted 
			   but its presence recorded for a potential later
			   diagnostic. */
  a_bit_field	range_based_for_range:1;
			/* TRUE if this is the expression or braced initializer
			   that determines the range of a range-based-for loop
			   to iterate over. */
  a_bit_field	in_constant_array_dimension:1;
			/* TRUE if this is an array dimension expression. */
  a_bit_field	is_enumerator_value:1;
			/* TRUE if this is the expression for an explicit
			   enumerator value.  E.g., "enum E { e = x+1 };". */
  a_const_eval_reattempt_state
		const_eval_reattempt_state;
			/* The current constant evaluation reattempt state
			   describing exceptions to folding requirements.  This
			   state is set by high level functions in the
			   interpreter and consumed by interested IL
			   construction code.  This allows for proper setup of
			   const_eval_deferred expressions.  In the future, a
			   more detailed interpreter evaluation result could
			   simplify this flow. */
  a_dynamic_init_dtor_fixup_ptr
		dynamic_init_dtor_fixup_list;
			/* List of dynamic init entries for which destructor
			   processing was delayed.  Only non-NULL when
			   in_cctor_elision_initializer is TRUE. */
  unsigned long	nested_construct_depth;
			/* Number of nested constructs like parentheses
			   begun within this major expression level. */
  an_object_lifetime_ptr
		lifetime;
			/* If non-NULL, points to an object lifetime that
			   exactly covers this expression, i.e., the lifetime
			   for temporaries created in the expression. */
  a_scope_number
		scope_number;
			/* The scope number associated with the top of the
			   scope_stack when this entry was pushed, used to
			   decide whether a nested expression is in the same
			   context or a new one when processing goes from
			   the expression routines out to declaration
			   processing and back in again. */
  a_dynamic_init_ptr
		destructions_preceding_expr;
			/* Points to the dynamic initialization that was the
			   first on the list of the current object lifetime
			   (i.e., the most recently constructed) when this
			   expression was begun.  Needed when no lifetime
			   is pushed for the current expression, to find
			   the sequence of destructions associated with the
			   expression.  NULL if the "lifetime" field is
			   non-NULL. */
  a_scope_ptr	last_subscope_preceding_expr;
			/* The last local subscope (i.e., the scope stack
			   last_scope pointer) when this expression was begun.
			   Used to discard any scopes created within the
			   expression (e.g., in a GNU statement expression)
			   if the overall expression is discarded. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_source_sequence_entry_ptr
		last_source_seq_entry_preceding_expr;
			/* The end of the source sequence list (i.e., the
			   scope stack end_of_source_sequence_list pointer)
			   when this expression was begun.  Used to discard
			   any source sequence entries created within the
			   expression (e.g., in a GNU statement expression)
			   if the overall expression is discarded. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  a_source_position
		objectless_nonstatic_data_ref_pos;
			/* The source position of the most recent objectless
			   reference to a nonstatic data member, for use in
			   diagnostic messages if the reference is later
			   deemed invalid. */
  a_source_position
		potentially_unevaluated_lambda_pos;
			/* The source position of the most recent potentially
			   unevaluated lambda definition, for use in
			   diagnostic messages if the definition is later
			   deemed invalid. */
  a_source_position
		call_in_right_comma_operand_pos;
			/* The source position of the most recent function call
			   seen in the right hand side of a comma operation.
			   Only set when allow_call_with_incomplete_return_type
			   is TRUE; it is used to provide a position for a
			   diagnostic potentially occurring later on. */
  a_lambda_ptr
		current_lambda_in_header;
			/* If non-NULL, we're inside the header (not the body)
			   of the indicated lambda. */
  an_il_entity_list_entry_ptr
		*p_end_of_entities_defined_in_expression;
			/* If non-NULL, entries are recorded to keep track of
			   entities defined in the expression.  This field
			   then points to the pointer to update with the next
			   such entry.  (Currently, only closure types appear
			   on this list.)  The field is copied to newly pushed
			   entries, and then copied back when the entry is
			   popped. */
  an_expr_rescan_info_entry_ptr
		default_rescan_info;
			/* When template_deduction_context is TRUE, this
			   points to the rescan information on the nearest
			   enclosing expression that has one, and is NULL
			   otherwise.  This is used to provide default rescan
			   information (e.g., source positions) for expressions
			   that don't have any. */
  struct an_initializer_cache
		*initializer_cache;
			/* If non-NULL, points to an initializer cache that
			   may contain queued up expressions that should be
			   taken before new ones are scanned from source or
			   rescanned. */
  a_rescan_control_block
		*rcblock;
			/* If non-NULL, points to a rescan control block
			   controlling the current redoing of semantic analysis
			   on a previously-scanned expression. */
} an_expr_stack_entry;

EXTERN_THREAD an_expr_stack_entry_ptr
		expr_stack;
			/* Pointer to the top of the expression stack. */

EXTERN_THREAD a_ref_entry_ptr
		curr_expr_ref_entries;
			/* List of all the reference entries for the current
			   expression.  They are recorded at the end of the
			   expression.  Before that, the kind of reference
			   each indicates might be adjusted. */

EXTERN_THREAD an_expr_node_ptr
		decltype_rescan_operand;
			/* While rescanning a decltype operand, this points
			   to the operand of the innermost type operator, or
			   NULL if that innermost type operator is not
			   "decltype". */

#if SEQUENCING_DIAGNOSTICS_ENABLED

/*
When checking full expressions for side-effects that are unsequenced
with respect to other side-effects and uses in the expression, the
following structures are used to maintain a list of variables used in the
expression, and for each of these variables, lists of side-effects
(modifications) and uses (loads) of the variable.
*/

typedef struct a_seq_pt_info_entry *a_seq_pt_info_entry_ptr;
typedef struct a_seq_pt_info_entry {
  a_seq_pt_info_entry_ptr
                next;   /* Pointer to the next entry on this list. */
  an_expr_node_ptr
                expr;   /* The expression (always an enk_variable node)
                           that generated the side-effect/use. */
  a_byte_boolean
                independent_of_value_computation;
                        /* Most side-effects are required to complete before
                           a value computation for the operation; however
                           certain side-effects are independent of value
                           computation (and result in this flag being set
                           to TRUE).  Used only for side-effects, not uses. */
  a_byte_boolean
                diagnostic_issued;
                        /* Set to TRUE if a sequencing diagnostic has been
                           issued for this side-effect/use. */
} a_seq_pt_info_entry;

typedef struct a_seq_pt_var_entry *a_seq_pt_var_entry_ptr;
typedef struct a_seq_pt_var_entry {
  a_seq_pt_var_entry_ptr
                next;   /* Pointer to the next entry on this list.  Entries
                           are sorted using the variable name as a key. */
  a_variable_ptr
                variable;
                        /* The variable for which side-effects and uses
                           appertain. */
  a_seq_pt_info_entry_ptr
                side_effects;
                        /* A list of side-effects (modifications) of
                           variable. */
  a_seq_pt_info_entry_ptr
                uses;   /* A list of uses (loads) of variable. */
} a_seq_pt_var_entry;

#endif /* SEQUENCING_DIAGNOSTICS_ENABLED */

/*
Entry representing a failure to fold a call to a consteval function.
*/
typedef struct a_pending_consteval_failure {
  a_routine_ptr
		routine;
			/* The consteval function targeted by the failed
			   call.  NULL if there is no pending failure. */ 
  a_source_position
		diag_pos;
			/* The position at which to diagnose the failure.
			   Undefined if there is no pending failure. */
  a_diag_list
		diag_list;
			/* A description of the cause of the failure.
			   Undefined if there is no pending failure. */
} a_pending_consteval_failure;

EXTERN_THREAD a_pending_consteval_failure
		pending_consteval_failure;
			/* While scanning call arguments, this may hold a
			   description of a failure to fold a nested consteval
			   call.  The failure cannot be diagnosed until it is
			   determined that the enclosing call is not itself to
			   a consteval function. */

extern a_boolean consteval_failure(a_routine_ptr      rp,
                                   a_constant_ptr     result_con,
                                   a_source_position  *pos,
                                   a_diag_list        *diag_list);

extern void diag_invalid_consteval_func_in_expr(an_expr_node_ptr  expr);

extern void diag_invalid_consteval_func_in_dyn_init(a_dynamic_init_ptr  dip);

/*
Variable that controls whether an attempt should be made to fold all
initializers to constant expressions or only initializers for variables
with static duration.  Also indicates a preference for other expressions,
like arguments or the source of assignments.  In general, folding to a
constant is good for code generation and less good for source analysis.
*/
EXTERN_THREAD a_boolean
		favor_constant_result_for_nonstatic_init
#if VAR_INITIALIZERS
                                    = FAVOR_CONSTANT_RESULT_FOR_NONSTATIC_INIT
#endif /* VAR_INITIALIZERS */
                                                                              ;


/*
Variable that controls whether backing expressions for constants should be
inhibited in some cases (particularly, when implicitly converting a constant
value to an arithmetic type).  This can be useful when dealing with very large
aggregate initializers.
*/
EXTERN_THREAD a_boolean
		reduce_backing_expression_use;

#if DEBUG
/*
Counts of entries allocated, for debugging purposes.
*/
EXTERN_THREAD unsigned long
		num_arg_match_summaries_allocated;
#endif /* DEBUG */

/*
Macro that returns the current expression kind.
*/
#define curr_expr_kind() (expr_stack->expression_kind)

/*
Macro that tests the current expression kind.
*/
#define curr_expr_kind_is(kind)                                       \
  (curr_expr_kind() == (an_expression_kind)(kind))

/*
Macro that returns TRUE if the current expression kind is some variety of
constant expression.  Note that the "<=" is possible because the constant
kinds are at the beginning of the list.
*/
#define curr_expr_kind_is_const()                                     \
  ((int)(curr_expr_kind()) <= (int)ek_init_constant)

/*
Macro that returns TRUE if the current expression is or is inside of a C++/CLI
attribute argument expression.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#define curr_expr_is_cli_attribute_argument()                         \
  (cli_or_cx_enabled && expr_stack != NULL &&                         \
   (a_boolean)expr_stack->is_cli_attr_arg_expression)
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
/*lint -emacro(506,curr_expr_is_cli_attribute_argument)*/
#define curr_expr_is_cli_attribute_argument() (FALSE)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */


/*
Macro that returns TRUE if the current expression kind is one in which
backing expressions are recorded for constants.  They are never recorded
for preprocessing expressions (because the constants are never saved).  The
decision for other kinds of expressions depends on the configuration; see
below.
*/
#if DO_IL_LOWERING && !RECORD_BACKING_EXPRS_WITH_IL_LOWERING
/* In IL-lowering configurations, unless otherwise requested, the only
   backing expressions kept are those appearing in template declarations,
   where they may be needed for mangling (at least in the IA-64 ABI), and
   those that might be referred to by a_local_expr_node_ref entries (since
   otherwise we may end up with IL write-read errors and the like). */
#define curr_expr_kind_is_one_in_which_const_exprs_are_recorded() \
  (!curr_expr_kind_is(ek_pp) &&                                   \
   ((innermost_function_scope != NULL &&                          \
     innermost_function_scope->expr_node_refs != NULL) ||         \
    depth_template_declaration_scope != NO_SCOPE_DEPTH))
#else /* !(DO_IL_LOWERING && !RECORD_BACKING_EXPRS_WITH_IL_LOWERING) */
#if !BACKING_EXPR_FOR_NONTYPE_TEMPL_ARG || \
    TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
/* In non-IL-lowering configurations that do not incorporate the
   C++-generating back end, backing expressions are normally kept for all
   non-preprocessing constants except for non-type template arguments,
   which are only kept within template definitions. */
#define curr_expr_kind_is_one_in_which_const_exprs_are_recorded() \
  (!curr_expr_kind_is(ek_pp) &&                                   \
   (depth_template_declaration_scope != NO_SCOPE_DEPTH ||         \
    !curr_expr_kind_is(ek_template_arg)))
#else /* !(!BACKING_EXPR_FOR_NONTYPE_TEMPL_ARG || ...) */
/* When BACKING_EXPR_FOR_NONTYPE_TEMPL_ARG is TRUE (which is required for
   the C++-generating back end), all non-preprocessing contexts allow
   backing expressions to be recorded. */
#define curr_expr_kind_is_one_in_which_const_exprs_are_recorded() \
  (!curr_expr_kind_is(ek_pp))
#endif /* !BACKING_EXPR_FOR_NONTYPE_TEMPL_ARG || ... */
#endif /* DO_IL_LOWERING && !RECORD_BACKING_EXPRS_WITH_IL_LOWERING */

/*
Macro that returns TRUE if the current expression is evaluated, i.e.,
it's not inside a sizeof or alignof, and it's not in a "dead" piece of
a short-circuiting operation like "?".
*/
#define curr_expr_is_evaluated() ((a_boolean)expr_stack->evaluated)

/*
Macro that returns TRUE if the current expression is potentially evaluated,
i.e., it's not inside a sizeof or alignof.
*/
#define curr_expr_is_potentially_evaluated()                          \
  ((a_boolean)expr_stack->potentially_evaluated)

#define in_unevaluated_expr_context()                                 \
  (expr_stack != NULL && !curr_expr_is_potentially_evaluated())

/*
Macro that returns TRUE if the current expression is potentially unevaluated,
i.e., it's an unevaluated operand or the operand of typeid.
*/
#define curr_expr_is_potentially_unevaluated() \
  ((a_boolean)expr_stack->potentially_unevaluated)

/*
Macro that returns TRUE if the current expression is const and the current
subexpression is evaluated.  This is the right test for the requirement that
folding of an operation on constants produces a constant result, e.g., 1/0
does not have to fold when in an unevaluated context.  Those rules are the
same in C++11 constant expressions and traditional constant expressions.
*/
#define curr_expr_kind_is_evaluated_const()                           \
  (curr_expr_kind_is_const() && curr_expr_is_evaluated())

/*
Macro that returns TRUE if the current expression is some kind of
C or pre-C++11 constant expression.  In such expressions,
  (1) disallowed operators, types, and constructs are flagged as
      invalid immediately when they appear, even in unevaluated
      subexpressions;
  (2) each operation/conversion must immediately fold to a constant
      result, except when unevaluated (e.g., 1 || 1/0 is okay).
In C++11 constant expressions, most operators/types/constructs are
allowed as long as the final result is a constant of the right kind.

This is the right test for a validity test of a source construct,
i.e., a particular operator (for example, "*" for indirection is not
allowed in pre-C++11 constant expressions).  Often, a use of this
macro will be followed by an "else" that calls
operator_not_allowed_in_cpp11_constant_expr.
*/
#define curr_expr_kind_is_traditional_const()                         \
  (expr_stack->traditional_const_expr_required)

/*
Macro that returns TRUE if there is at least one initializer cached for the
current context (as indicated by the expression stack).  Any such expression(s)
or braced-init-list(s) should be consumed before taking more from source or
a rescan.
*/
#define cached_initializer_present() \
  (expr_stack != NULL && \
   expr_stack->initializer_cache != NULL && \
   anything_cached(expr_stack->initializer_cache))

/*
Macro that returns TRUE if the expressions being processed are being done so in
the context of preparing a coroutine's descriptor block.
*/
#define initializing_coroutine_descriptor() \
  (expr_stack != NULL && expr_stack->in_coroutine_desc_init)

#define in_constant_array_dimension() \
  (expr_stack != NULL && expr_stack->in_constant_array_dimension)

/*
TRUE if the current mode allows binding an rvalue reference to an lvalue
bit-field expression.  MSVC10 allows that.
*/
#define binding_rvalue_ref_to_bit_field_allowed() \
  (microsoft_bugs && microsoft_version >= 1600)

/* Copy an operand.  Note that this does not copy the subtree of the
   operand.  This macro is used to move an operand from one place to
   another, with the idea that the old copy will no longer be used.
   If you want to make a full copy, see clone_operand. */
#define copy_operand(from, to) (*(to) = *(from))

/*
Macro that is TRUE if the operand is an error operand.
*/
#define is_error_operand(operand)					\
	((operand)->kind == (an_operand_kind)ok_error ||		\
	 is_error_type((operand)->type))

/*
Macro that is TRUE if the operand is an expression operand.
*/
#define is_expression_operand(operand)					\
	((operand)->kind == (an_operand_kind)ok_expression)

/*
Macro that is TRUE if the operand is a constant operand.
*/
#define is_constant_operand(operand)					\
	((operand)->kind == (an_operand_kind)ok_constant)

/*
Macro that is TRUE if the operand is a ck_template_param constant operand.
*/
#define is_template_param_constant_operand(operand)			\
	(is_constant_operand(operand) &&                                \
	 (operand)->variant.constant.kind ==                            \
                                (a_constant_repr_kind)ck_template_param)

/*
Macro that is TRUE if the operand is a tpck_expression ck_template_param
constant operand.
*/
#define is_template_param_expression_constant_operand(operand)          \
        (is_template_param_constant_operand(operand) &&                 \
         (operand)->variant.constant.variant.template_param.kind ==     \
                        (a_template_param_constant_kind)tpck_expression)

/*
Macro that is TRUE if the operand is an indefinite function operand.
*/
#define is_indefinite_function_operand(operand)				\
	((operand)->kind == (an_operand_kind)ok_indefinite_function)

/*
Macro that is TRUE if the operand is a pointer-to-member symbol operand.
*/
#define is_sym_for_member_operand(operand)			        \
	((operand)->kind == (an_operand_kind)ok_sym_for_member)

/*
Macro that is TRUE if the operand is an undefined symbol operand.
*/
#define is_undefined_symbol_operand(operand)				\
	((operand)->kind == (an_operand_kind)ok_undefined_symbol)

#if MICROSOFT_EXTENSIONS_ALLOWED
/*
Macro that is TRUE if the operand is one that references a member declared
with the Microsoft C++ extension __declspec(property(...)) or the C++/CLI
property syntax.
*/
#define is_property_ref_operand(operand)				\
	((operand)->kind == (an_operand_kind)ok_property_ref)
/*
Macro that is TRUE if the operand is one that references a member declared
with the Microsoft C++/CLI event syntax.
*/
#define is_event_ref_operand(operand)				\
	((operand)->kind == (an_operand_kind)ok_event_ref)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Macro that is TRUE if the operand represents a brace-enclosed list used
as a single expression.
*/
#define is_braced_init_list_operand(operand) \
	((operand)->kind == (an_operand_kind)ok_braced_init_list)

/*
Macro that is TRUE if the operand is an lvalue.  Note that this isn't
precisely the standard definition of an lvalue, as an lvalue is not
allowed to have type void, and this macro returns TRUE for that case.
The difference is compensated for in the cases where it matters.
*/
#define is_an_lvalue(operand)						\
	(is_a_glvalue(operand) && !is_an_xvalue(operand))
/*
Macro that is TRUE if the operand is a prvalue.
*/
#define is_a_prvalue(operand)						\
	((operand)->state == (an_operand_state)os_prvalue)

/*
Macro that is TRUE if the operand is an rvalue.
*/
#define is_an_rvalue(operand)						\
	(is_a_prvalue(operand) || is_an_xvalue(operand))

/*
Macro that is TRUE if the operand is a function designator.
*/
#define is_a_function_designator(operand)				\
	((operand)->state == (an_operand_state)os_function_designator)

/*
Macro that is TRUE if the operand is a glvalue (lvalue or xvalue).
*/
#define is_a_glvalue(operand)						\
	((operand)->state == (an_operand_state)os_glvalue)


extern a_boolean is_potentially_constant_valued_variable(a_variable_ptr var);

extern a_ref_entry_ptr copy_ref_entry_list(a_ref_entry_ptr ref_list);

extern void record_operand_ref_entries(an_operand  *operand);

extern void flush_ref_entries_except(a_ref_entry_ptr keep_list1,
                                     a_ref_entry_ptr keep_list2);

extern a_ref_entry_ptr ref_entry(a_symbol_ptr      sym_ptr,
                                 a_source_position *source_position);

extern void change_ref_kinds(a_ref_entry_ptr         ref_list,
                             a_symbol_reference_kind new_kind);

extern void change_refs_to_error(a_ref_entry_ptr ref_list);

extern void change_operand_refs_to_error(an_operand *operand);

extern void change_arg_list_refs_to_error(an_arg_list_elem_ptr arg_list);

extern void change_some_ref_kinds(a_ref_entry_ptr         ref_list,
                                  a_symbol_reference_kind old_kind,
                                  a_symbol_reference_kind new_kind);

extern void record_operand_modification_refs(an_operand *operand);

extern void detach_ref_entries_from_curr_expr(an_operand *operand);

extern void reattach_ref_entries_to_curr_expr(an_operand *operand);

extern void free_arg_operand_list(an_arg_operand_ptr aop);

extern an_arg_operand_ptr alloc_arg_operand(void);

extern an_arg_operand_ptr arg_operand_for_constant(a_constant_ptr  cp);

extern void free_attachments_to_operand(an_operand *operand);

extern an_init_component_ptr alloc_init_component(an_init_component_kind kind);

extern
an_arg_list_elem_ptr alloc_arg_list_elem_for_operand(an_operand *operand);

extern void free_init_component(an_init_component_ptr icp);
extern void free_init_component_list(an_init_component_ptr icp);
#define free_arg_list(icp) free_init_component_list(icp)
#define free_arg_list_elem(icp) free_init_component(icp)

extern a_source_position* init_component_pos(an_init_component_ptr icp);

#if EXTRA_SOURCE_POSITIONS_IN_IL
a_source_position *init_component_end_pos(an_init_component_ptr icp);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

extern a_boolean is_error_component(an_init_component_ptr  icp);

extern a_boolean is_string_literal_component(an_init_component_ptr  icp,
                                             a_constant_ptr         *p_con);

extern a_boolean is_parenthesized_component(an_init_component_ptr  icp);

extern a_boolean is_pack_expansion_component(an_init_component_ptr  icp);

extern
a_boolean any_pack_expansion_components_in_list(an_init_component_ptr icp);

#if DEBUG
extern void db_init_component(an_init_component_ptr icp);
#endif /* DEBUG */

extern
void conv_braced_init_component_to_error_expression(an_arg_list_elem_ptr alep);

extern void check_arg_list_elem_is_expression(an_arg_list_elem_ptr alep);

extern void clear_initializer_cache(struct an_initializer_cache *cache);

extern void flush_initializer_cache(struct an_initializer_cache *cache);

extern
void add_init_component_to_initializer_cache(
                                          an_init_component_ptr       icp,
                                          a_boolean                   to_front,
                                          struct an_initializer_cache *cache);

extern void add_operand_to_initializer_cache(
                                 an_operand                  *operand,
                                 a_boolean                   to_front,
                                 a_boolean                   bundle,
                                 struct an_initializer_cache *cache);

extern an_init_component_ptr fetch_init_component_from_initializer_cache(
                                           struct an_initializer_cache *cache);

extern
void extract_operand_from_expression_component(an_init_component_ptr icp,
                                               an_operand            *operand,
                                               a_boolean             free_icp);
extern
a_boolean fetch_operand_from_initializer_cache(
                                          an_operand                  *operand,
                                          struct an_initializer_cache *cache);

extern void free_dynamic_init_dtor_fixup(a_dynamic_init_dtor_fixup_ptr didfp);

extern void if_evaluating_mark_routine_referenced(a_routine_ptr  routine);

extern void expr_reference_to_implicitly_invoked_function_full(
                                             a_symbol_ptr      sym,
                                             a_source_position *pos,
                                             a_type_ptr        class_of_object,
                                             a_boolean         honor_virtual,
                                             a_boolean         evaluated,
                                             a_boolean         instantiate);

extern void expr_reference_to_implicitly_invoked_function
                                            (a_symbol_ptr      sym,
                                             a_source_position *pos,
                                             a_type_ptr        class_of_object,
                                             a_boolean         honor_virtual);

extern a_boolean expr_reference_to_trivial_default_constructor(
                                              a_type_ptr         class_type,
                                              a_source_position  *pos);

extern void expr_reference_to_trivial_copy_constructor(
                                           a_type_ptr        class_type,
                                           a_source_position *pos,
                                           a_boolean         elided_reference);

extern an_expr_node_ptr expr_copy_default_arg_expr_list(a_routine_ptr    rout,
                                                        a_param_type_ptr ptp);

extern void save_expr_stack(an_expr_stack_entry_ptr *saved_expr_stack);

extern void restore_expr_stack(an_expr_stack_entry_ptr saved_expr_stack);

extern void push_expr_stack(an_expression_kind      expression_kind,
                            an_expr_stack_entry_ptr new_entry,
                            a_boolean               force_object_lifetime,
                            a_boolean               suppress_object_lifetime);

extern void push_expr_stack_with_rcblock(
                              an_expression_kind      expression_kind,
                              an_expr_stack_entry_ptr new_entry,
                              a_boolean               force_object_lifetime,
                              a_boolean               suppress_object_lifetime,
                              a_rescan_control_block  *rcblock);

extern void push_expr_stack_for_initializer(
                                       an_expr_stack_entry *expr_stack_entry,
                                       an_expr_stack_entry **saved_expr_stack,
                                       an_expression_kind  expr_kind,
                                       a_boolean           is_full_expr,
                                       a_decl_parse_state  *dps,
                                       an_init_state       *is);

extern void transfer_expr_context_if_applicable(
                                            an_expr_stack_entry *saved_stack);

extern void set_up_initializer_rescan(a_decl_parse_state *dps);

extern void undo_side_effects_for_discarded_unevaluated_expression(void);

extern void pop_expr_stack(void);

extern void pop_expr_stack_for_initializer(
                                        an_expr_stack_entry *saved_expr_stack,
                                        a_boolean           is_full_expr,
                                        a_decl_parse_state  *dps,
                                        an_init_state       *is);

extern void temporarily_set_non_constant_expression_kind(
                                        an_expression_kind *saved_kind,
                                        a_boolean          *saved_traditional);
extern
void restore_constant_expression_kind(an_expression_kind saved_kind,
                                      a_boolean          saved_traditional);

extern void record_entity_defined_in_expression(
                                             char              *entity,
                                             an_il_entry_kind  kind,
                                             a_boolean         in_file_scope);

extern a_boolean entities_are_recorded_for_current_expression(void);

extern void rule_out_expr_kinds(a_ruled_out_expr_kind_set ruled_out_set,
                                an_operand                *operand);

extern void instantiate_template_var_if_applicable(a_variable  *vp);

extern an_expr_node_ptr wrap_up_full_expression(an_expr_node_ptr expr);

extern void discard_curr_expr_object_lifetime(void);

extern void discard_constant_expr_object_lifetime(void);

extern void wrap_up_dynamic_init_full_expression(a_dynamic_init_ptr dip);

extern void wrap_up_constant_full_expression(a_constant  *constant);

extern
a_constant_ptr var_constant_value_full(a_variable_ptr var,
                                       a_boolean      copy_for_reuse,
                                       a_boolean      clear_backing_expr,
                                       a_boolean      allow_C_mode_const_var);

extern a_constant_ptr var_constant_value(a_variable_ptr var);

extern void using_lvalue(an_operand *operand);

extern void modifying_lvalue(an_operand *operand,
                             a_boolean  value_used);

extern a_boolean is_bit_field_operand(an_operand *operand);

extern a_boolean is_bit_field_whose_address_can_be_taken(a_field_ptr field);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_boolean is_any_initonly_field_operand(an_operand *operand);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_boolean is_dllimport_variable_glvalue(an_expr_node_ptr expr,
                                               a_constant       *conaddr);

extern void take_address_of_or_reference_to_lvalue(
                                    an_operand        *operand,
                                    a_boolean         reference_case,
                                    a_boolean         rvalue_reference_case,
                                    a_boolean         use_handle_for_ref_class,
                                    a_boolean         is_builtin_addressof,
                                    a_source_position *operator_position);

extern void take_address_of_lvalue(an_operand *operand,
                                   a_source_position *operator_position);

extern void take_reference_to_operand(an_operand *operand,
                                      a_boolean  rvalue_reference_case);

extern void conv_object_pointer_to_lvalue(an_operand *operand);

extern void conv_prvalue_expr_to_object_pointer(an_expr_node_ptr *p_node,
                                                a_boolean        *converted);

extern
an_expr_node_ptr strip_rvalue_base_class_casts(an_expr_node_ptr expr,
                                               an_expr_node_ptr *top_cast,
                                               an_expr_node_ptr *bottom_cast);

extern void conv_xvalue_to_lvalue(an_operand *operand);

extern void conv_class_prvalue_operand_to_glvalue(an_operand  *operand,
                                                  a_boolean   xvalue);

extern void conv_class_prvalue_operand_to_lvalue(an_operand *operand);

extern void conv_class_operand_to_object_pointer(an_operand *operand);

extern a_boolean is_an_xvalue(an_operand *operand);

extern a_boolean rvalue_ref_can_be_bound_to(an_operand *operand);

extern a_constant_ptr value_of_constant_var_lvalue_operand(
                                                          an_operand *operand);

extern
a_boolean current_mode_allows_dot_static_folding(an_expr_node_ptr lhs_expr);

extern
a_constant_ptr fold_constant_base_class_cast(an_expr_node_ptr expr,
                                             a_constant_ptr   alloc_con);

extern
an_expr_node_ptr conv_glvalue_expr_to_prvalue(an_expr_node_ptr node,
                                              a_boolean        *constant_case,
                                              a_constant_ptr   *con_value,
                                              a_source_position *err_pos);

extern an_expr_node_ptr conv_glvalue_expr_to_prvalue_external(
                                            an_expr_node_ptr   node,
                                            a_boolean          *constant_case,
                                            a_constant_ptr     *con_value,
                                            a_source_position  *err_pos,
                                            a_ctws_options_set options);

extern void conv_glvalue_to_prvalue(an_operand *operand);

extern a_type_ptr determine_arithmetic_conversions(an_operand *operand_1,
					           an_operand *operand_2);

extern a_type_ptr usual_arithmetic_conversions(a_type_ptr operand_1_type,
                                               a_type_ptr operand_2_type);

extern void check_binary_scoped_enum_operation(an_operand  *operand_1,
                                               an_operand  *operand_2,
                                               a_type_ptr  *p_operation_type);

#if TARG_HAS_IEEE_FLOATING_POINT
extern void make_nan_operand(an_operand  *result);

extern void make_infinity_operand(an_operand  *result);
#endif /* TARG_HAS_IEEE_FLOATING_POINT */

#if C99_IL_EXTENSIONS_SUPPORTED
extern void make_imaginary_unit_operand(an_operand  *result);

extern a_boolean determine_imaginary_operation_type
                                        (a_token_kind          op_token,
                                         an_operand            *operand_1,
                                         an_operand            *operand_2,
                                         a_source_position     *err_pos,
                                         a_type_ptr            *result_type,
                                         an_expr_operator_kind *op);
#endif /* C99_IL_EXTENSIONS_SUPPORTED */

#if GNU_VECTOR_TYPES_ALLOWED
extern a_type_ptr make_integer_vector_result_type(a_type_ptr operand_type);

extern a_boolean vector_and_scalar_types_are_compatible(
                                              a_type_ptr      vec_type,
                                              a_type_ptr      scalar_type,
                                              a_constant_ptr  scalar_con,
                                              a_boolean       narrowing_okay);

extern void make_vector_fill_operand(an_operand *operand,
                                     a_type_ptr vec_type);

extern a_boolean determine_vector_operation_type(
                                        a_token_kind           op_token,
                                        an_operand             *operand_1,
                                        an_operand             *operand_2,
                                        a_source_position      *err_pos,
                                        a_type_ptr             *operation_type,
                                        an_expr_operator_kind  *op);
#endif /* GNU_VECTOR_TYPES_ALLOWED */

#if MICROSOFT_EXTENSIONS_ALLOWED
extern void adjust_constant_operand_info_for_microsoft_null_pointer_test(
                                         an_operand       *operand,
                                         a_boolean        *operand_is_constant,
                                         a_constant       **operand_constant,
                                         an_expr_node_ptr *con_expr);

extern void convert_operand_to_handle_to_cli_string(an_operand_ptr op);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void add_cast_to_node(an_expr_node_ptr  *p_node,
                             a_type_ptr        new_type,
                             a_boolean         check_cast_access,
                             a_boolean         check_ambiguity,
                             a_boolean         is_implicit_cast,
                             a_boolean         is_reinterpret_cast,
                             a_boolean         reinterpret_semantics,
                             a_source_position *err_pos);

extern a_boolean operand_is_function(an_operand *operand);

extern a_boolean check_compatibility_of_pointer_operands(
                   an_operand        *operand_1,
                   an_operand        *operand_2,
                   a_source_position *operator_position,
                   an_opname_kind    opkind,
                   a_boolean         pointer_normalization_standard_in_C,
                   a_boolean         pointers_to_functions_standard_in_C,
                   a_boolean         pointers_to_incomplete_standard_in_C,
                   a_boolean         mixed_object_and_incomplete_standard_in_C,
                   a_type_ptr        *operation_type);

#if MICROSOFT_EXTENSIONS_ALLOWED
a_boolean check_compatibility_of_handle_operands(
                                          an_operand        *operand_1,
                                          an_operand        *operand_2,
                                          a_source_position *operator_position,
                                          a_type_ptr        *operation_type);
extern
a_boolean is_literal_convertible_to_cli_string(an_operand *operand,
                                               a_boolean  allow_complex);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_boolean check_compatibility_of_nullptr_operands(
                                          an_operand        *operand_1,
                                          an_operand        *operand_2,
                                          a_source_position *operator_position,
                                          a_type_ptr        *operation_type);

extern a_boolean check_ptr_to_member_operands_for_compatibility(
                                          an_operand        *operand_1,
                                          an_operand        *operand_2,
                                          a_source_position *operator_position,
                                          a_type_ptr        *operation_type);

#if FIXED_POINT_ALLOWED
extern void check_mixed_integer_fixed_point_arithmetic(
                                            an_operand             *operand_1,
                                            an_operand             *operand_2,
                                            an_expr_operator_kind  op);
#endif /* FIXED_POINT_ALLOWED */

extern void change_binary_operand_types(a_type_ptr             type,
				        an_operand             *operand_1,
				        an_operand             *operand_2,
                                        an_expr_operator_kind  op);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern
a_symbol_ptr get_property_accessor_symbol(a_symbol_ptr       property_sym,
                                          a_boolean          put,
                                          a_boolean          must_be_present,
                                          a_source_position  *pos);
extern void rewrite_property_reference(
                             an_operand                          *operand,
                             an_operand                          *put_operand,
                             a_rewritten_property_reference_kind kind,
                             a_routine_ptr                       *get_routine);

extern void rewrite_event_operator(an_operand         *lhs,
                                   an_operand         *rhs,
                                   an_operand         *result,
                                   a_token_kind       operator_token,
                                   a_source_position  *operator_pos,
                                   a_boolean          *p_err);

extern void rewrite_event_ref_for_call(
                                   an_operand        *operand,
                                   an_operand        *bound_function_selector);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
void call_named_member_function(an_operand           *selector_operand,
                                a_const_char         *member_name,
                                a_template_arg_ptr   templ_arg_list,
                                an_arg_list_elem_ptr alep,
                                an_operand           *orig_operand,
                                an_operand           *result);

extern
void call_adl_named_function(a_const_char            *func_name,
                             a_template_arg_ptr      templ_arg_list,
                             an_arg_list_elem_ptr    alep,
                             a_source_position       *pos,
                             a_token_sequence_number tok_seq_number,
                             an_overload_context     ovl_context,
                             an_operand              *result,
                             an_expr_node_ptr        *call_node);

extern void convert_function_template_to_single_function_full(
                                                an_operand    *operand,
                                                a_boolean     will_call,
                                                a_symbol_ptr  *single_func_sym,
                                                a_boolean     *dependent);

extern void convert_function_template_to_single_function_if_possible(
                                                        an_operand *operand,
                                                        a_boolean   will_call);

extern a_boolean conv_bound_function_to_static_selection(
                                          an_operand *operand,
                                          an_operand *bound_function_selector);

extern void error_if_indefinite_function(an_operand *operand);

extern void do_operand_transformations(an_operand                   *operand,
                                       a_transformation_options_set options);

extern void do_void_operand_transformations(an_operand *operand,
                                            a_boolean  force_lvalue_to_rvalue);

extern void eliminate_unusual_operand_kinds(an_operand *operand);

extern a_type_ptr boolean_result_type(void);

extern a_boolean op_is_zero_constant(an_operand *operand);

extern a_boolean op_is_false_constant(an_operand *operand);

#define op_is_null_pointer_constant(op)                                      \
  (is_nullptr_type((op)->type) ||                                            \
   (is_constant_operand(op) &&                                               \
    is_null_pointer_constant(&(op)->variant.constant)))

extern a_boolean op_is_null_pointer_value(an_operand *operand);

extern a_boolean op_is_null_address_lvalue(an_operand *operand);

extern a_boolean pointer_operand_cannot_be_null(an_operand *operand);

extern void add_reference_indirection(an_operand *result);

#if GNU_EXTENSIONS_ALLOWED
extern a_boolean operand_is_address_of_label(an_operand  *op);
#endif /* GNU_EXTENSIONS_ALLOWED */

extern a_boolean variable_has_constant_address(a_variable_ptr variable);

extern a_boolean operand_is_lvalue_for_variable(an_operand      *operand,
                                                a_variable_ptr  *var);

extern a_boolean operand_is_lvalue_for_rref_variable(an_operand      *operand,
                                                     a_variable_ptr  *var);

extern void make_lvalue_variable_operand(a_variable_ptr    variable,
                                         a_source_position *position,
                                         a_source_position *end_position,
                                         an_operand        *result,
                                         a_ref_entry_ptr   rep);

extern void make_lvalue_operand_from_compound_constant(
                                                     a_constant_ptr  constant,
                                                     an_operand      *operand);

extern void make_ptr_to_member_constant_operand(
                                    a_symbol_ptr      member_sym,
                                    a_symbol_ptr      member_proj_sym,
                                    a_source_position *position,
                                    a_source_position *end_position,
                                    a_boolean         check_protected_access,
                                    a_boolean         is_qualified_name,
                                    a_boolean         has_required_ampersand,
                                    a_boolean         allow_on_managed,
                                    an_operand        *result);

extern a_boolean check_object_pointer_operand(an_operand    *operand,
                                              an_error_code err_code);

#if PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED
extern a_boolean check_object_or_incomp_array_pointer_operand(
                                               an_operand    *operand,
                                               an_error_code err_code,
                                               an_operand    *otherop);
#endif /* PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED */

extern an_error_code expr_not_integral_or_any_enum_code(void);

extern an_error_code expr_not_integral_code(void);

extern an_error_code expr_not_arithmetic_code(void);

extern an_error_code type_not_arithmetic_or_pointer_code(void);

extern a_boolean check_arithmetic_or_enum_operand(an_operand *operand);

extern void make_integer_constant_operand(an_operand		*operand,
				          a_host_large_integer	value);

extern void promote_operand(an_operand *operand);

extern void arg_default_promote_operand(an_operand *argument_operand,
                                        a_boolean  is_ellipsis);

#if !STANDALONE_UTILITY_PROGRAM
extern void expr_clear_init_state(an_init_state *init_state);
#endif /* !STANDALONE_UTILITY_PROGRAM */

extern void set_glvalue_operand_state(an_operand *operand);

extern void make_constant_operand(a_constant *constant,
			          an_operand *operand);

extern void make_sym_constant_operand(a_symbol_ptr sym,
                                      an_operand   *operand);

extern void make_string_constant_operand(a_constant *constant,
                                         an_operand *operand);

#if DEBUG
extern void db_operand(an_operand *operand);

extern void db_ref_entries(a_ref_entry_ptr rep);

extern void db_operand_ref_entries(an_operand *operand);
#endif /* DEBUG */

extern void clear_operand(an_operand_kind kind,
		          an_operand      *operand);

extern an_expr_node_ptr alloc_empty_parens_func_cast(
                                          a_type_ptr          type_cast_to,
                                          a_dynamic_init_kind init_kind,
                                          a_source_position   *start_position);

extern a_dynamic_init_ptr add_array_nonconstant_aggregate_init(
                                        a_dynamic_init_ptr element_dip,
                                        a_type_ptr         array_type,
                                        a_type_ptr         elem_type,
                                        a_routine_ptr      dtor_routine,
                                        a_targ_size_t      number_of_elements);

extern void accumulate_array_size(a_type_ptr    array_type,
                                  a_targ_size_t *num_elements);

extern a_dynamic_init_ptr add_array_nonconstant_aggregate_init_computing_size(
                                         a_dynamic_init_ptr element_dip,
                                         a_type_ptr         array_type,
                                         a_routine_ptr      dtor_routine);

extern an_expr_node_ptr make_expr_reusable_copy(
                                  an_expr_node_ptr expr,
                                  a_boolean        vars_can_change,
                                  a_boolean        *temp_init_used,
                                  a_boolean        treat_as_potential_prvalue);

/*
Macro to set the "position" field in an expression node, given the start
position of that expression and, if applicable, the position of the top-level
operator in the expression.
*/
#define set_expr_base_position(expr, start_pos, operator_pos)                 \
{                                                                             \
  /* Use a variable in case operator_pos expands to NULL. */                  \
  /* ("*(NULL)" elicits compiler errors.) */                                  \
  a_source_position  *pos_var = (operator_pos);                               \
  if (pos_var != NULL && pos_var->seq != 0) {                                 \
    (expr)->position = *pos_var;                                              \
  } else {                                                                    \
    (expr)->position = *(start_pos);                                          \
  }  /* if */                                                                 \
}

#if EXTRA_SOURCE_POSITIONS_IN_IL
#define set_expr_position(expr, start_position, end_position,                 \
                          operator_position)                                  \
{                                                                             \
  set_expr_base_position(expr, start_position, operator_position);            \
  (expr)->expr_range.start = *start_position;                                 \
  (expr)->expr_range.end = *end_position;                                     \
}
#else /* !EXTRA_SOURCE_POSITIONS_IN_IL */
#define set_expr_position(expr, start_position, end_position,                 \
                          operator_position)                                  \
{                                                                             \
  set_expr_base_position(expr, start_position, operator_position);            \
}
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

extern void set_operand_expr_position_if_expr(an_operand        *operand,
                                              a_source_position *operator_pos);

extern void set_operand_kind(an_operand      *operand,
                             an_operand_kind kind);

/*
Function type used for the call-back routine of
make_glvalue_expr_reusable_copy.
*/
typedef an_expr_node_ptr a_reusable_copy_function(
                                  an_expr_node_ptr expr,
                                  a_boolean        vars_can_change,
                                  a_boolean        *temp_init_used,
                                  a_boolean        treat_as_potential_prvalue);
typedef a_reusable_copy_function *a_reusable_copy_function_ptr;

extern an_expr_node_ptr glvalue_expr_reusable_copy(
                      an_expr_node_ptr             expr,
                      a_boolean                    vars_can_change,
                      a_reusable_copy_function_ptr copy_func,
                      a_boolean                    *temp_init_used,
                      a_boolean                    treat_as_potential_prvalue);

extern void clone_operand(an_operand *operand,
                          an_operand *operand_clone,
                          a_boolean  vars_can_change,
                          a_boolean  *temp_init_used,
                          a_boolean  treat_as_potential_prvalue);

extern void error_in_operand(an_error_code error_code,
		             an_operand    *operand);

extern void type_error_in_operand(an_error_code error_code,
                                  an_operand    *operand,
                                  a_type_ptr    type);

extern void type2_error_in_operand(an_error_code error_code,
                                   an_operand    *operand,
                                   a_type_ptr    type1,
                                   a_type_ptr    type2);

extern
void change_template_param_constant_operand_to_lvalue(
                                           an_operand *operand,
                                           a_boolean  const_qualified = FALSE);

extern
void change_nonreal_member_constant_operand_to_lvalue(an_operand *operand);

extern
void revert_class_prvalue_to_glvalue_if_possible(an_operand *operand,
                                                 a_boolean  xvalue = FALSE);

extern void revert_gcc_rvalue_to_lvalue_if_possible_full(
                                              an_operand *operand,
                                              a_boolean  ignore_casts,
                                              a_boolean  drop_same_size_casts);

extern void revert_gcc_rvalue_to_lvalue_if_possible(an_operand *operand,
                                                    a_boolean  ignore_casts);

extern void revert_microsoft_rvalue_to_lvalue_if_possible(an_operand *operand);

extern a_boolean check_modifiable_lvalue_operand(an_operand *operand);

extern
a_boolean construct_not_allowed_in_cpp11_constant_expr(
                                                    an_error_code     err_code,
                                                    a_source_position *pos);

extern
a_boolean operator_not_allowed_in_cpp11_constant_expr(a_source_position *pos);

extern a_boolean check_scalar_operand(an_operand *operand);

extern void make_field_operand(a_symbol_locator  *locator,
                               a_source_position *source_position,
                               a_source_position *end_position,
                               an_operand        *result);

extern a_dynamic_init_ptr alloc_expr_dynamic_init(a_dynamic_init_kind kind);

extern a_dynamic_init_ptr alloc_expr_ctor_dynamic_init(
                                            a_routine_ptr     ctor_routine,
                                            an_expr_node_ptr  args,
                                            a_type_ptr        dest_type,
                                            a_boolean         static_temp,
                                            a_boolean         add_default_args,
                                            a_boolean         implied_source,
                                            a_boolean         value_init,
                                            a_boolean         sequenced_args,
                                            a_boolean         fold_constexpr,
                                            a_boolean         check_constexpr,
                                            a_source_position *pos);

extern a_routine_ptr expr_select_default_constructor(
                                         a_type_ptr        class_type,
                                         a_source_position *err_pos,
                                         a_boolean         *err);

extern a_routine_ptr expr_select_copy_constructor(
                                  a_type_ptr            class_type,
                                  a_type_qualifier_set  required_qualifiers,
                                  a_boolean             source_is_rvalue,
                                  a_source_position     *err_pos,
                                  a_boolean             *class_bitwise_copy,
                                  a_boolean             record_ref);

extern
a_routine_ptr expr_select_destructor_b(a_type_ptr        class_type,
                                       a_type_ptr        object_class_type,
                                       a_source_position *position,
                                       a_boolean         honor_virtual,
                                       a_boolean         *error_detected);

extern a_routine_ptr expr_select_destructor(
                                     a_type_ptr        class_type,
                                     a_type_ptr        object_class_type,
                                     a_source_position *position,
                                     a_boolean         honor_virtual);

extern void add_dtor_to_dynamic_init(a_dynamic_init_ptr dip,
                                     a_type_ptr         class_type,
                                     a_type_ptr         object_class_type,
                                     a_source_position  *position);

/*lint -emacro(506,record_dtor_in_dynamic_init)*/
#define record_dtor_in_dynamic_init(dtor, dip, evaluated)                    \
  if ((dtor)!= NULL) {                                                       \
    (dip)->destructor = (dtor);                                              \
    if ((evaluated)) (dtor)->called = TRUE;                                  \
  }  /* if */

extern a_dynamic_init_ptr alloc_dtor_dynamic_init(
                                           a_dynamic_init_kind kind,
                                           a_type_ptr          type,
                                           a_source_position   *position);

extern void set_temp_dynamic_init_lifetime(a_dynamic_init_ptr dip);

extern void set_temp_init_dynamic_init_lifetime(
                                              an_expr_node_ptr temp_init_node);

extern an_expr_node_ptr alloc_temp_init_node(
                                      a_type_ptr         temp_type,
                                      a_dynamic_init_ptr dip,
                                      a_boolean          is_lvalue,
                                      a_boolean          is_explicit_cast);

extern a_boolean error_on_abstract_class_object(a_type_ptr        object_type,
                                                a_source_position *position);

extern an_expr_node_ptr create_expr_temporary(
                                    a_type_ptr          temp_type,
                                    a_boolean           result_is_addr,
                                    a_boolean           is_explicit_cast,
                                    a_boolean           suppress_abstract_test,
                                    a_dynamic_init_kind init_kind,
                                    a_source_position   *position,
                                    a_dynamic_init_ptr  *dip);

extern a_routine_ptr routine_from_function_operand(an_operand *operand);

#if OPTIMIZE_VIRTUAL_FUNCTION_CALLS
extern an_expr_node_ptr retrace_base_casts(an_expr_node_ptr base_cast_node,
                                           a_type_ptr       target_class,
                                           a_type_ptr       qualifiers_model,
                                           an_expr_node_ptr *new_top_of_tree);
#endif /* OPTIMIZE_VIRTUAL_FUNCTION_CALLS */

extern a_routine_ptr final_overrider(a_routine_ptr    base_class_function,
                                     an_expr_node_ptr implicit_this_arg,
                                     a_type_ptr       complete_object_type);

extern void make_function_call(an_expr_node_ptr  function_node,
                               a_type_ptr        function_type,
                               a_boolean         is_virtual,
                               a_boolean         virtual_suppressed,
                               a_boolean         selector_is_object_pointer,
                               a_boolean         compiler_generated,
                               a_boolean         is_conversion,
                               a_boolean         arg_dep_lookup_suppressed,
                               a_boolean         qualified_function_name,
                               a_boolean         found_through_adl,
                               a_boolean         uses_operator_syntax,
                               a_source_position *start_position,
                               a_source_position *operator_position,
                               a_source_position *end_position,
                               an_operand        *result,
                               a_boolean         *p_folded,
                               an_expr_node_ptr  *function_call_node);

extern void assemble_function_call(an_operand        *function_operand,
                                   an_operand        *bound_function_selector,
                                   an_expr_node_ptr  argument_list,
                                   a_boolean         compiler_generated,
                                   a_boolean         arg_dep_lookup_suppressed,
                                   a_boolean         qualified_function_name,
                                   a_boolean         found_through_adl,
                                   a_boolean         uses_operator_syntax,
                                   a_source_position *start_position,
                                   a_source_position *operator_position,
                                   a_source_position *end_position,
                                   an_operand        *result,
                                   a_boolean         *p_folded,
                                   an_expr_node_ptr  *function_call_node);

extern a_statement_ptr make_call_assignment_statement(
                                            a_routine_ptr     rout,
                                            a_boolean         suppress_virtual,
                                            an_expr_node_ptr  dest,
                                            an_expr_node_ptr  source,
                                            a_source_position *err_pos);

extern a_boolean is_uuidof_expr(an_expr_node_ptr expr,
                                a_boolean        *p_is_type = NULL,
                                an_expr_node_ptr *p_op_expr = NULL,
                                a_type_ptr       *p_type = NULL);

extern void make_selection_rescan_operands(
                             a_rescan_control_block  *rcblock,
                             an_operand              *operand_1,
                             a_boolean               call_rescan_case,
                             a_boolean               offsetof_case,
                             a_source_position       *operator_position,
                             a_token_sequence_number *operator_tok_seq_number);

extern a_type_ptr do_type_substitution_for_rescan(
                                        a_type_ptr                    type,
                                        a_rescan_control_block        *rcblock,
                                        an_expr_rescan_info_entry_ptr eriep);

extern
void make_sizeof_et_al_rescan_operands(
                              a_rescan_control_block  *rcblock,
                              a_boolean               *p_is_type,
                              an_operand              *operand,
                              a_type_ptr              *p_type,
                              a_source_position       *operator_position,
                              a_token_sequence_number *operator_tok_seq_number,
                              a_source_position       *type_position);

extern an_expr_node_ptr arg_list_from_dyn_init(a_dynamic_init_ptr dip);

extern void make_cast_rescan_operands(
                              a_rescan_control_block *rcblock,
                              a_dynamic_init_ptr     dip,
                              a_source_position      *start_position,
                              a_type_ptr             *cast_type, 
                              a_source_position      *type_position,
                              an_init_component_ptr  *braced_init_list,
                              a_dynamic_init_ptr     *actual_dip,
                              an_operand             *operand,
                              an_operand             *bound_function_selector);

extern void make_new_delete_rescan_operands(
                                   a_rescan_control_block      *rcblock,
                                   a_new_delete_supplement_ptr *ndsp,
                                   a_source_position           *start_position,
                                   a_type_ptr                  *type, 
                                   a_source_position           *type_position);

#if MICROSOFT_EXTENSIONS_ALLOWED

extern void make_gcnew_rescan_operands(
                                  a_rescan_control_block      *rcblock,
                                  a_gcnew_supplement_ptr      *gsp,
                                  a_source_position           *start_position,
                                  a_type_ptr                  *type,
                                  a_source_position           *type_position);

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void make_throw_rescan_operands(a_rescan_control_block *rcblock,
                                       a_source_position      *start_position,
                                       an_operand             *operand,
                                       a_boolean              *expr_present);

extern void make_type_operand_rescan_type(
                                   a_rescan_control_block *rcblock,
                                   a_type_ptr             *type,
                                   a_source_position      *type_position);

extern void make_template_name_rescan_template(
                                   a_rescan_control_block *rcblock,
                                   a_template_ptr         *templ,
                                   a_source_position      *templ_position);

extern an_arg_list_elem_ptr rescan_expr_as_arg_list_elem(
                                              an_expr_node_ptr       expr,
                                              a_rescan_control_block *rcblock);

extern
an_expr_node_ptr find_primary_cast_node(an_expr_node_ptr   orig_operand_expr,
                                        a_cast_source_form source_form,
                                        an_operand         *operand);

extern a_boolean check_pointer_operand(an_operand    *operand,
				       an_error_code err_code);

#if MICROSOFT_EXTENSIONS_ALLOWED

extern a_boolean check_pointer_or_handle_operand(an_operand     *operand,
                                                 an_error_code  err_code);

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void make_expression_operand(an_expr_node_ptr node,
			            an_operand       *operand);

extern void make_glvalue_expression_operand(an_expr_node_ptr node,
                                            an_operand       *operand);

extern
void make_lvalue_or_rvalue_expression_operand(an_expr_node_ptr node,
                                              an_operand       *operand);

extern void make_indefinite_function_operand(a_symbol_ptr     routine_sym,
                                             a_symbol_locator *locator,
                                             an_operand       *operand);

extern void make_undefined_symbol_operand(a_symbol_ptr      sym,
                                          a_ref_entry_ptr   ref_list,
                                          a_symbol_locator  *loc,
                                          an_operand        *operand);

extern void make_sym_for_member_operand(a_symbol_ptr    member_sym,
                                        a_boolean       is_qualified_name,
                                        a_ref_entry_ptr rep,
                                        an_operand      *operand);

extern void make_template_param_expr_constant_operand(an_operand *operand);

extern an_expr_node_ptr strip_ref_indirect(an_expr_node_ptr expr,
                                           a_boolean        parens_also);

extern an_expr_rescan_info_entry_ptr save_operand_info_in_rescan_info_entry(
                                        an_operand                    *operand,
                                        an_expr_rescan_info_entry_ptr eriep);

extern void save_rescan_info_for_braced_init_list(a_dynamic_init_ptr    dip,
                                                  an_init_component_ptr icp);

extern void record_operator_position_in_expr_rescan_info(
                               an_expr_node_ptr        node,
                               a_source_position       *operator_position,
                               a_token_sequence_number operator_tok_seq_number,
                               a_source_position       *operator_position_2);

extern void record_operator_position_in_rescan_info(
                               an_operand              *operand,
                               a_source_position       *operator_position,
                               a_token_sequence_number operator_tok_seq_number,
                               a_source_position       *operator_position_2);

extern void record_typed_operator_position_in_expr_rescan_info(
                                             an_expr_node_ptr  expr,
                                             a_source_position *start_position,
                                             a_source_position *type_position,
                                             a_type_ptr        cast_type);

extern void record_cast_position_in_expr_rescan_info(
                                             an_expr_node_ptr  expr,
                                             a_source_position *start_position,
                                             a_source_position *type_position,
                                             a_type_ptr        cast_type);

extern
void record_cast_position_in_rescan_info(an_operand         *operand,
                                         an_expr_node_ptr   orig_operand_expr,
                                         a_cast_source_form source_form,
                                         a_source_position  *start_position,
                                         a_source_position  *type_position,
                                         a_type_ptr         cast_type);

extern
void restore_operand_info_from_expr_rescan_info_entry(
                                        an_operand                    *operand,
                                        an_expr_rescan_info_entry_ptr eriep);

extern void clear_rescan_control_block(a_rescan_control_block *rcblock);

extern an_expr_node_ptr strip_implicit_operations_for_rescan(
                                        an_expr_node_ptr        expr,
                                        an_expr_rescan_info_entry_ptr *periep);

inline an_expr_node_ptr strip_implicit_operations(an_expr_node_ptr  expr)
/*
Return the top-most node in the given expression tree that corresponds to an
explicit source construct (as opposed, for example, to an implied conversion).
*/
{
  return strip_implicit_operations_for_rescan(
                                   expr, (an_expr_rescan_info_entry_ptr*)NULL);
}  /* strip_implicit_operations */

extern an_expr_rescan_info_entry_ptr get_expr_rescan_info(
                                       an_expr_node_ptr          expr,
                                       an_expr_rescan_info_entry *rescan_info);
extern void make_rescan_operand_full(
                            an_expr_node_ptr         expr,
                            a_rescan_control_block   *rcblock,
                            a_local_expr_options_set local_options,
                            an_operand               *operand,
                            an_operand               *bound_function_selector);

extern void make_rescan_operand(an_expr_node_ptr       expr,
                                a_rescan_control_block *rcblock,
                                an_operand             *operand);

extern
void make_rescan_operands(a_rescan_control_block  *rcblock,
                          an_operand              *operand_1,
                          an_operand              *operand_2,
                          an_operand              *operand_3,
                          a_source_position       *operator_position,
                          a_token_sequence_number *operator_tok_seq_number,
                          a_source_position       *operator_position_2);

extern void make_call_rescan_operands(
                             a_rescan_control_block  *rcblock,
                             an_operand              *operand,
                             an_operand              *bound_function_selector,
                             a_source_position       *operator_position,
                             a_token_sequence_number *operator_tok_seq_number,
                             a_source_position       *closing_paren_position);

extern an_init_component_ptr rescan_init_component(
                                            an_init_component_ptr  icp,
                                            a_rescan_control_block *rcblock);

extern an_expr_node_ptr alloc_node_for_constant_operand(an_operand *operand);

extern void rewrite_captured_variable_access(an_operand *opnd);

extern an_expr_node_ptr make_node_from_operand(an_operand *operand,
                                               a_boolean  no_rewrite = FALSE);

extern void record_rescan_info_for_node(an_expr_node  *node);

extern
an_expr_node_ptr make_node_from_operand_for_expr_list(an_operand *operand);

extern an_expr_node_ptr full_expr_from_operand(an_operand  *opnd);

extern
void mark_expr_of_operand_as_pack_expansion_if_necessary(an_operand *operand);

extern
void set_operand_name_reference_from_locator(an_operand       *operand,
                                             a_symbol_locator *locator);

extern void set_operand_id_details_from_locator(an_operand       *operand,
                                                a_symbol_locator *locator);

extern
void force_operand_to_constant_if_possible_full(
                                             an_operand *operand,
                                             a_boolean  is_constant_evaluated);

#define force_operand_to_constant_if_possible(opnd)                          \
  (force_operand_to_constant_if_possible_full(                               \
                                     opnd, /*is_constant_evaluated=*/FALSE))

extern
a_boolean expr_interpret_expression_operand(an_operand  *operand,
                                            a_boolean   must_be_constant,
                                            a_boolean   is_constant_evaluated,
                                            a_boolean   force_prvalue = FALSE);

extern a_boolean constant_conv_function_result(a_routine_ptr   conv_func,
                                               an_operand      *source_operand,
                                               a_type_ptr      result_type,
                                               a_constant_ptr  result_con);

extern a_boolean error_on_nonconstant_constant(a_constant        *constant,
                                               a_source_position *pos);

extern void make_template_param_constant_from_operand(
                                                    an_operand      *operand,
                                                    a_constant_ptr  result_con,
                                                    a_type_ptr      type);

extern void extract_constant_from_operand(an_operand     *operand,
                                          a_constant_ptr constant);

extern void discard_operand(an_operand *operand);

extern a_boolean in_potential_constant_constexpr_context(void);

extern a_boolean call_did_not_fold_to_constant(a_routine_ptr     routine,
                                               an_operand        *operand,
                                               a_boolean         no_diagnostic,
                                               a_diag_list_ptr   diag_list,
                                               a_source_position *pos);

extern void prep_generic_nontype_template_argument(an_operand *operand);

extern void prep_generic_template_argument_list(
                                         a_template_arg_ptr template_arg_list);

extern void make_unknown_dependent_function_operand(
                                          a_symbol_ptr       sym,
                                          a_boolean          is_template_id,
                                          a_template_arg_ptr template_arg_list,
                                          a_boolean          is_qualified_name,
                                          an_operand         *operand);

extern void set_has_address_of_flag_if_needed(a_constant_ptr  con,
                                              a_boolean       flag_value);

extern void conv_indefinite_function_to_unknown_dependent_function(
                                                   an_operand *operand,
                                                   a_boolean  force_to_rvalue);

extern void cast_overloaded_function(a_type_ptr type_cast_to,
                                     an_operand *operand,
                                     a_boolean  is_cast,
                                     a_boolean  is_static_cast,
                                     a_boolean  skip_final_adjustment);

extern an_expr_node_ptr make_node_from_void_expression_operand(
                                          an_operand_ptr  operand,
                                          a_boolean       result_of_stmt_expr);

extern void cast_operand_to_void(an_operand *operand,
                                 a_type_ptr type_cast_to);

extern
void cast_operand_full(a_type_ptr        new_type,
                       an_operand        *operand,
                       a_source_position *err_pos,
                       a_boolean         check_cast_access,
                       a_boolean         check_ambiguity,
                       a_boolean         is_implicit_cast,
                       a_boolean         is_reinterpret_cast,
                       a_boolean         reinterpret_semantics);

extern void cast_operand(a_type_ptr new_type,
                         an_operand *operand,
                         a_boolean  is_implicit_cast);

extern
void cast_operand_special(a_type_ptr        new_type,
                          an_operand        *operand,
                          a_source_position *err_pos,
                          a_boolean         check_cast_access,
                          a_boolean         is_implicit_cast,
                          a_boolean         is_reinterpret_cast,
                          a_boolean         reinterpret_semantics);

extern void conv_selector_to_object_pointer(an_operand *operand,
                                            a_boolean  *is_arrow_operator);

extern void base_class_cast_operand(an_operand       *operand_1,
                                    a_base_class_ptr bcp,
                                    a_type_ptr       qualifiers_model,
                                    a_boolean        check_cast_access,
                                    a_boolean        allow_ambiguity,
                                    a_boolean        is_implicit_cast,
                                    a_boolean        implicit_in_naming,
                                    a_boolean        is_object_pointer);

extern void adjust_glvalue_type(an_operand *operand,
                                a_type_ptr dest_type);


extern void conv_rvalue_reference_result_to_xvalue(an_operand *operand);

extern
void conv_reference_cast_operand_to_lvalue_if_necessary(an_operand *operand,
                                                        a_type_ptr  dest_type);

extern void mark_as_reference_cast(an_expr_node_ptr expr,
                                   a_type_ptr       type_cast_to);

extern
void cast_operand_for_reference_cast(an_operand        *operand,
                                     a_type_ptr        dest_type,
                                     a_boolean         check_cast_access,
                                     a_boolean         is_implicit_cast,
                                     a_boolean         reinterpret_semantics);

extern void adjust_class_prvalue_type(an_operand *operand,
                                      a_type_ptr dest_type);

extern a_boolean is_a_cplusplus_lvalue(an_operand *operand);

extern void record_suppressed_error(void);

extern a_boolean expr_diagnostic_should_be_issued(an_error_severity sev,
                                                  an_error_code     err_code,
                                                  a_source_position *pos);

extern a_boolean is_consteval_diag_deferred();


template<typename... a_Fill_in_type>
inline void expr_pos_error(an_error_code     error_code,
                           a_source_position *error_pos,
                           a_Fill_in_type... fill_ins)
/*
Report the indicated error at the indicated position with the given fill-ins.
Suppress the error if we're in a context where diagnostics should be
suppressed, e.g., a template deduction context.
*/
{
  if (expr_error_should_be_issued()) {
    pos_error(error_code, error_pos, fill_ins...);
  }  /* if */
}  /* expr_pos_error */


template<typename... a_Fill_in_type>
inline void expr_pos_warning(an_error_code     error_code,
                             a_source_position *error_pos,
                             a_Fill_in_type... fill_ins)
/*
Report the indicated warning at the indicated position with the given fill-ins.
Suppress the warning if we're in a context where diagnostics should be
suppressed, e.g., a template deduction context.
*/
{
  if (expr_diagnostic_should_be_issued(es_warning, error_code, error_pos)) {
    pos_warning(error_code, error_pos, fill_ins...);
  }  /* if */
}  /* expr_pos_warning */


extern void expr_pos_st_warning(an_error_code     error_code,
                                a_source_position *error_pos,
                                a_const_char      *str);

extern void expr_pos_diagnostic(an_error_severity sev,
                                an_error_code     error_code,
                                a_source_position *error_pos);

extern void expr_pos_sy_diagnostic(an_error_severity sev,
                                   an_error_code     error_code,
                                   a_source_position *error_pos,
                                   a_symbol_ptr      sym);

extern void expr_pos_ty_diagnostic(an_error_severity sev,
                                   an_error_code     error_code,
                                   a_source_position *error_pos,
                                   a_type_ptr        tp);

extern void expr_pos_ty2_diagnostic(an_error_severity sev,
                                    an_error_code     error_code,
                                    a_source_position *error_pos,
                                    a_type_ptr        tp1,
                                    a_type_ptr        tp2);

extern void expr_pos_st_diagnostic(an_error_severity sev,
                                   an_error_code     error_code,
                                   a_source_position *error_pos,
                                   a_const_char      *str);

extern void expr_pos_st_error(an_error_code     error_code,
                              a_source_position *error_pos,
                              a_const_char      *str);

extern void expr_pos_ty_error(an_error_code     error_code,
                              a_source_position *error_pos,
                              a_type_ptr        tp);

extern void expr_pos_ty2_error(an_error_code     error_code,
                               a_source_position *error_pos,
                               a_type_ptr        tp1,
                               a_type_ptr        tp2);

extern void expr_syntax_error(an_error_code error_code);

extern void expr_issue_incomplete_type_diag(
                                        a_source_position *pos,
                                        a_type            *type,
                                        an_error_severity severity = es_error);

extern void expr_issue_sizeless_type_error(a_source_position *pos,
                                           a_type            *type);

extern void expr_expect_error(void);

extern a_boolean expr_access_checking_should_be_done(void);

extern void reclaim_fs_nodes_of_operand(an_operand *opnd);

extern void make_error_operand(an_operand *operand);

extern void expr_check_ambiguity_and_verify_access(a_symbol_locator *locator);

extern void expr_overload_check_ambiguity_and_verify_access(
                                           a_symbol_locator *locator,
                                           a_symbol_ptr     overloaded_symbol);

extern void operand_will_not_be_used_because_of_error(an_operand *operand);

extern void arg_list_will_not_be_used_because_of_error(
                                            an_arg_list_elem_ptr operand_list);

extern void conv_to_error_operand(an_operand *operand);

extern void normalize_error_operand(an_operand *operand);

extern void restore_operand_details(an_operand *operand,
                                    an_operand *orig_operand);

extern void restore_operand_details_incl_ref(an_operand *operand,
                                             an_operand *orig_operand);

extern void restore_operand_details_for_cast(an_operand *operand,
                                             an_operand *orig_operand,
                                             a_boolean  is_implicit_cast,
                                             a_boolean  incl_ref);

extern void restore_operand_id_details(an_operand *operand,
                                       an_operand *orig_operand);

extern void restore_operand_form_of_name_reference(an_operand *operand,
                                                   an_operand *orig_operand);

extern a_boolean check_call_function_pointer_operand(an_operand *operand);

extern void make_function_designator_operand(
                                      a_symbol_ptr      routine_sym,
                                      a_boolean         is_qualified_name,
                                      a_boolean         compiler_generated,
                                      a_source_position *position,
                                      a_source_position *end_position,
                                      a_ref_entry_ptr   rep,
                                      an_operand        *result);

extern void build_unary_result_operand(an_operand            *operand,
                                       an_expr_operator_kind kind,
                                       a_type_ptr            type,
                                       an_operand            *result);

extern
void build_binary_result_operand_full(an_operand            *operand_1,
                                      an_operand            *operand_2,
                                      an_expr_operator_kind kind,
                                      a_type_ptr            type,
                                      a_boolean             result_is_lvalue,
                                      an_operand            *result);

extern void build_binary_result_operand(an_operand            *operand_1,
	       		 	        an_operand            *operand_2,
				        an_expr_operator_kind kind,
				        a_type_ptr            type,
	       			        an_operand            *result);

extern void build_question_result_operand(an_operand  *operand_1,
                                          an_operand  *operand_2,
                                          an_operand  *operand_3,
                                          a_type_ptr  result_type,
                                          a_boolean   result_is_an_lvalue,
                                          a_boolean   is_gnu_two_operand_form,
                                          an_operand  *result);

extern a_boolean check_integral_or_enum_operand(an_operand *operand);

extern a_boolean check_integral_or_enum_or_fixed_point_operand(
                                                        an_operand  *operand);

extern an_expr_node_ptr conv_array_expr_to_pointer(an_expr_node_ptr node);

extern void do_array_to_pointer_conversion(an_operand *operand);

extern void conv_array_operand_to_pointer_operand(an_operand *operand);

extern a_type_ptr type_after_function_to_pointer_transformation(
                                                      a_type_ptr arg_type,
                                                      an_operand *arg_operand);

extern a_type_ptr type_of_call(an_expr_node_ptr  expr);

extern void conv_sym_for_member_operand_to_ptr_to_member(
                                        an_operand        *operand,
                                        a_source_position *ampersand_position);

extern
void conv_expr_function_designator_to_ptr_to_function(
                                        an_operand        *operand,
                                        a_boolean         will_call,
                                        a_source_position *ampersand_position);

extern void conv_function_designator_to_ptr_to_function(
                                         an_operand        *operand,
                                         a_source_position *ampersand_position,
                                         a_boolean         allow_ctor,
                                         a_boolean         will_call);

extern a_type_ptr do_implicit_type_transformations(a_type_ptr type,
                                                   an_operand *operand);

extern void error_and_make_error_operand(an_error_code error_code,
				         an_operand    *operand);

extern
void do_binary_operation_full(an_expr_operator_kind   op,
                              an_operand              *operand_1,
                              an_operand              *operand_2,
                              a_type_ptr              result_type,
                              a_boolean               result_is_lvalue,
                              an_operand              *result,
                              a_source_position       *operator_position,
                              a_token_sequence_number operator_tok_seq_number,
                              a_source_position       *operator_position_2);

extern
void do_binary_operation(an_expr_operator_kind   op,
                         an_operand              *operand_1,
                         an_operand              *operand_2,
                         a_type_ptr              result_type,
                         an_operand              *result,
                         a_source_position       *operator_position,
                         a_token_sequence_number operator_tok_seq_number);

extern a_boolean operand_has_uncertain_value_category(an_operand *operand);

extern void prep_generic_operand_full(an_operand *operand,
                                      a_boolean  lvalue_expected,
                                      a_boolean  rvalue_expected);

extern void prep_generic_operand(an_operand *operand);

extern void generic_cast_operand(an_operand         *operand,
                                 a_type_ptr         dest_type,
                                 a_cast_source_form source_form,
                                 a_boolean          is_implicit_cast);

extern void make_generic_designator_constant(an_arg_list_elem_ptr icp,
                                             a_constant_ptr       con);

extern void prep_generic_argument(an_arg_list_elem_ptr arg);

extern void prep_generic_argument_list(an_arg_list_elem_ptr arg_list);

extern void mark_init_component_list_as_permanently_allocated(
                                                  an_init_component *list_icp);

extern void make_braced_init_list_operand(an_arg_list_elem_ptr alep,
                                          an_operand           *result);

extern an_expr_node_ptr make_expr_from_argument(an_arg_list_elem_ptr arg);

extern an_expr_node_ptr make_expr_list_from_argument_list(
                                    an_arg_list_elem_ptr arg_list,
                                    a_boolean            dependent_expression);

extern void make_dummy_lvalue_operand(a_type_ptr type,
                                      an_operand *operand);

extern
void template_binary_operation(an_expr_operator_kind   op,
                               an_operand              *operand_1,
                               an_operand              *operand_2,
                               an_operand              *result,
                               a_source_position       *operator_position,
                               a_token_sequence_number operator_tok_seq_number,
                               a_source_position       *operator_position_2);

extern
void template_multi_subscript_operation(
                               an_expr_operator_kind   op_kind,
                               an_operand              *operand_1,
                               an_arg_list_elem_ptr    subscripts,
                               an_operand              *result,
                               a_source_position       *operator_position,
                               a_token_sequence_number operator_tok_seq_number,
                               a_source_position       *operator_position_2);

extern
void do_unary_operation(an_expr_operator_kind   op,
                        an_operand              *operand,
                        a_type_ptr              result_type,
                        an_operand              *result,
                        a_source_position       *start_position,
                        a_token_sequence_number operator_tok_seq_number);

extern
void template_unary_operation(an_expr_operator_kind   op,
                              an_operand              *operand,
                              an_operand              *result,
                              a_source_position       *start_position,
                              a_token_sequence_number operator_tok_seq_number);

extern
void do_question_operation(an_operand        *operand_1,
                           an_operand        *operand_2,
                           an_operand        *operand_3,
                           a_type_ptr        result_type,
                           a_boolean         result_is_an_lvalue,
                           a_boolean         suppress_class_rvalue_temp,
                           a_boolean         suppress_class_temp_optimization,
                           a_boolean         template_case,
                           a_boolean         is_gnu_two_operand_form,
                           a_source_position *question_position,
                           a_source_position *colon_position,
                           an_operand        *result);

/*
Versions of MSVC prior to 10.00 had an odd bug when dealing with certain
rvalues in conditional operators.  The following macro decides when the
emulation of that bug is enabled.  Since the bug interacts dangerously with
xvalues, we disable its emulation if rvalue references have been enabled.
*/
#define ms_rvalue_temp_in_cond_operator_bug_enabled()                        \
  (microsoft_bugs && microsoft_version < 1600 &&                             \
   !rvalue_references_enabled && !ms_strict_ternary)

extern
void template_question_operation(an_operand        *operand_1,
                                 an_operand        *operand_2,
                                 an_operand        *operand_3,
                                 a_boolean         is_gnu_two_operand_form,
                                 a_source_position *question_position,
                                 a_source_position *colon_position,
                                 an_operand        *result);

extern a_boolean check_boolean_controlling_expr(an_operand *operand);

extern a_boolean still_an_lvalue(a_type_ptr type_before_cast,
			         a_type_ptr type_cast_to);

extern a_boolean is_glvalue_for_auto_object(an_expr_node_ptr expr,
                                             a_boolean        *is_temp);

extern a_boolean is_prvalue_for_auto_object(an_expr_node_ptr expr,
                                            a_boolean        *is_temp);

extern a_boolean is_address_of_auto_object(an_expr_node_ptr  expr,
                                           a_boolean         *is_temp);

extern an_expr_operator_kind which_binary_operator(a_token_kind token,
						   a_type_ptr   type);

extern an_expr_operator_kind operator_for_opname_kind(
                                                an_opname_kind kind,
                                                a_boolean      unary_operator);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern
an_opname_kind simple_opname_kind_for_compound_assignment(an_opname_kind kind);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void add_base_class_casts(a_base_class_ptr  bcp,
                                 a_type_ptr        qualifiers_model,
                                 a_boolean         check_cast_access,
                                 a_boolean         check_ambiguity,
                                 a_boolean         allow_ambiguity,
                                 a_boolean         is_implicit_cast,
                                 a_boolean         implicit_in_naming,
                                 an_expr_node_ptr  *p_node,
                                 a_source_position *err_pos,
                                 a_boolean         *error_detected);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern
an_expr_node_ptr add_box_to_expression(an_expr_node_ptr expr,
                                       a_boolean        is_implicit,
                                       a_boolean        handle_to_form);
extern an_expr_node_ptr add_unbox_to_expression(an_expr_node_ptr expr,
                                                a_type_ptr       unboxed_type,
                                                a_boolean        make_lvalue);
extern
an_expr_node_ptr unbox_after_indirection_if_required(an_expr_node_ptr expr);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern
void add_derived_class_casts(a_type_ptr        new_type_pointed_to,
                             a_base_class_ptr  bcp,
                             a_boolean         check_ambiguity,
                             a_boolean         requires_runtime_check,
                             an_expr_node_ptr  *p_node,
                             a_source_position *err_pos,
                             a_boolean         *error_detected);

extern void cast_node(an_expr_node_ptr  *p_node,
		      a_type_ptr        type,
                      a_boolean         check_cast_access,
                      a_boolean         check_ambiguity,
		      a_boolean         is_implicit_cast,
                      a_boolean         is_reinterpret_cast,
                      a_boolean         reinterpret_semantics,
                      a_boolean         within_expr_processing,
                      a_source_position *err_pos);

#if DO_IL_LOWERING
extern a_type_ptr node_type_after_integral_promotion(an_expr_node_ptr node);
#endif /* DO_IL_LOWERING */

extern a_type_ptr operand_type_after_integral_promotion(an_operand  *operand);

#if UPC_EXTENSIONS_ALLOWED
extern void make_upc_thread_operand(an_operand            *operand,
                                    a_constant_repr_kind  kind);
#endif /* UPC_EXTENSIONS_ALLOWED */

extern a_boolean type_has_nodiscard_attribute(a_type_ptr   type,
                                              a_const_char **reason);

extern int compare_constraints(a_symbol_ptr  sym1,
                               a_symbol_ptr  sym2,
                               a_boolean     *p_equiv = NULL);

extern
a_boolean is_concept_satisfied(an_expr_node_ptr    constraint,
                               a_template_arg_ptr  args,
                               a_diag_list_ptr     diag_list,
                               a_ctws_options_set  options,
                               a_ctws_state_ptr    ctws_state,
                               a_boolean           *p_fatal);

extern
a_boolean is_substituted_concept_satisfied(
                                       an_expr_node_ptr           constraint,
                                       a_subst_pairs_array const  &subst_pairs,
                                       a_diag_list_ptr            diag_list,
                                       a_ctws_options_set         options,
                                       a_ctws_state_ptr           ctws_state,
                                       a_boolean                  *p_fatal);

extern 
a_boolean constraint_satisfied(an_expr_node_ptr      constraint,
                               a_template_arg_ptr    template_arg_list,
                               a_template_param_ptr  template_param_list,
                               a_diag_list_ptr       diag_list,
                               a_ctws_options_set    options = CTWS_NO_OPTIONS,
                               a_ctws_state_ptr      ctws_state = NULL,
                               a_boolean             *p_fatal = NULL,
                               a_boolean             *p_copy_error = NULL);

extern
a_boolean constraint_satisfied_full(
                          an_expr_node_ptr           constraint,
                          a_subst_pairs_array const  &subst_pairs,
                          a_diag_list_ptr            diag_list,
                          a_ctws_options_set         options = CTWS_NO_OPTIONS,
                          a_ctws_state_ptr           ctws_state = NULL,
                          a_boolean                  *p_fatal = NULL,
                          a_boolean                  *p_copy_error = NULL);

an_expr_node_ptr  substitute_expr(an_expr_node_ptr           expr,
                                  a_subst_pairs_array const  &subst_pairs,
                                  a_ctws_state               *ctws_state,
                                  a_ctws_options_set         options,
                                  a_constant_ptr             cp,
                                  a_constant_ptr             *p_allocated_cp,
                                  a_boolean                  *p_err);

extern
a_boolean check_type_constraint(a_type_ptr                 type,
                                an_expr_node_ptr           constraint,
                                a_subst_pairs_array const  &subst_pairs,
                                a_ctws_state               *ctws_state,
                                a_diag_list                *diag_list = NULL);

extern
a_boolean requires_expr_satisfied_full(an_expr_node_ptr          requires_expr,
                                       a_subst_pairs_array const &subst_pairs,
                                       a_ctws_state_ptr          cwts_state);

extern
a_boolean requires_expr_satisfied(an_expr_node_ptr           requires_expr,
                                  a_subst_pairs_array const  &subst_pairs);


/*
Simple structure to track some information associated with "requires" tokens
in C++20.  Specifically:
    (a) where the associated construct (requires-clause or requires-expression)
        ends, and
    (b) the representation of the unsubstituted construct.
*/
struct a_requires_range_descr {
  a_token_sequence_number
		next_tsn;
			/* The token sequence number seen after originally
			   parsing the construct. */
  a_boolean	is_friend_template;
			/* TRUE if this is a requires-clause for a friend
			   template. */
  a_pack_reference_ptr
		packs_referenced;
			/* For a requires-expression, a list of copies of the
			   pack references that were recorded when the
			   expression was originally parsed. */
  union {
    a_requires_clause_ptr
		requires_clause;
			/* The unsubstituted requires-clause. */
    an_expr_node_ptr
		requires_expr;
			/* The unsubstituted requires-expression. */
  };
};

typedef Ptr_map<a_token_sequence_number, a_requires_range_descr>
		a_requires_range_map;

EXTERN_THREAD a_requires_range_map
		*requires_ranges;
			/* A map from the token_sequence_number corresponding
			   to a tok_requires token for a requires-expression
			   or requires-clause, to the token sequence number
			   that follows the corresponding construct.  This is
			   used to skip over those constructs during
			   instantiations. */

typedef Ptr_map<an_expr_node_ptr, a_subst_pairs_array>
		a_requires_subst_map;

EXTERN_THREAD a_requires_subst_map
		*requires_expr_substs;
			/* A map from a requires-expression to template
			   argument substitutions.  This is used to delay these
			   substitutions until a fully non-dependent template
			   argument list is available. */

EXTERN_THREAD Ptr_map<a_variable_ptr, a_boolean>
		*vars_being_deduced;
			/* A map containing variables whose type is in the
			   process of being deduced. */

typedef Ptr_map<a_type_ptr, a_token_cache_ptr>
		a_cached_lambdas_map;

EXTERN_THREAD a_cached_lambdas_map
		*cached_lambdas;
			/* A map from the closure class of a lambda expression
			   scanned in a template deduction context to the
			   token cache that holds the tokens of the lambda.
			   This is used to re-instantiate the lambda when the
			   enclosing template parameters are substituted. */


/* Reflection-range iteration support.  A cached description, built once per
   reflection_range type, of how to iterate that range to collect its
   std::meta::info elements.  The constructs below mirror the
   begin/end/not-equal/increment/dereference of a range-based for loop, but the
   __range variable carries no initializer: the constexpr interpreter binds its
   storage to the actual argument object when replaying the plan (see
   reflection_range_plan_for_type and its use in interpret.c). */
typedef struct a_reflection_range_plan *a_reflection_range_plan_ptr;
typedef struct a_reflection_range_plan {
  a_variable_ptr    range;
                        /* The synthetic __range reference variable.  It has
                           no initializer: The interpreter binds it to the
                           reflection_range argument before replaying. */
  a_variable_ptr    begin;
                        /* __begin, dynamically initialized to __range.begin()
                           or begin(__range). */
  a_variable_ptr    end;
                        /* __end, dynamically initialized to __range.end() or
                           end(__range). */
  a_variable_ptr    iterator;
                        /* The element variable (of type std::meta::info),
                           dynamically initialized to *__begin.  The
                           interpreter re-initializes it on each iteration and
                           reads the std::meta::info value out of its
                           storage. */
  an_expr_node_ptr  ne_call_expr;
                        /* The "__begin != __end" loop test. */
  an_expr_node_ptr  incr_call_expr;
                        /* The "++__begin" increment. */
  a_byte_boolean    usable;
                        /* TRUE if the plan was built successfully; FALSE
                           caches a range type whose analysis failed. */
} a_reflection_range_plan;

typedef Ptr_map<a_type_ptr, a_reflection_range_plan_ptr>
		a_reflection_range_plan_map;

extern a_reflection_range_plan_ptr build_reflection_range_plan(
                                                a_type_ptr  range_type);

extern a_reflection_range_plan_ptr reflection_range_plan_for_type(
                                                a_type_ptr  range_type);

extern
a_dynamic_init_ptr unoptimize_conditional_return(a_dynamic_init_ptr  dip,
                                                 a_routine_ptr       rp,
                                                 a_source_position   *pos);

#if DEBUG
extern void count_rescan_fs_expr_nodes(unsigned long *p_count);

extern unsigned long show_expr_space_used(void);
#endif /* DEBUG */
#if CHECKING && DEBUG
extern void check_all_init_component_entries_freed(void);
#endif /* CHECKING && DEBUG */

extern void exprutil_one_time_init(void);

extern void exprutil_trans_unit_init(void);

extern void exprutil_init(void);

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef EXPRUTIL_H */


