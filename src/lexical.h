/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

lexical.h -- Declarations relating to lexical.c (having to do with source
             input and lexical scanning).

*/

/* Avoid including these declarations more than once: */
#ifndef LEXICAL_H
#define LEXICAL_H 1

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/* Declare pointer types up front to minimize mutual recursion problems. */
typedef struct an_orig_line_modif  *an_orig_line_modif_ptr;
typedef struct a_source_line_modif *a_source_line_modif_ptr;
typedef struct a_token_cache *a_token_cache_ptr;
typedef struct a_cached_token *a_cached_token_ptr;

typedef struct a_symbol_header a_symbol_header_dummy_typedef;

#ifndef HOST_ENVIR_H
#include "host_envir.h"
#endif /* ifndef HOST_ENVIR_H */
#ifndef ERROR_H
#include "error.h"
#endif /* ifndef ERROR_H */
#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */


/* There are more #includes later in this file. */

EXTERN_THREAD a_token_sequence_number
		curr_token_sequence_number;
			/* The sequence number associated with the
			   current token.  A token retains its sequence
			   number even when saved and restored from a
			   token cache.  For a coalesced token this is the
			   token sequence number of the initial token. */

EXTERN_THREAD a_token_sequence_number
		last_token_sequence_number_of_token;
			/* The sequence number associated with the end of
			   the current token.  This is normally the same as
			   curr_token_sequence_number, but for coalesced
			   identifiers refers to a later token. */

EXTERN_THREAD a_token_sequence_number
		last_token_sequence_number_used;
			/* The counter used to assign token sequence
			   numbers. */

#define NO_TOKEN_SEQUENCE_NUMBER ((a_token_sequence_number)0)
			/* The value used to indicate that no sequence number
			   is present. */

/* These declarations are placed here so that they will be defined before
   symbol_tbl.h is included. */


/* Flags used to specify how identifiers are to be scanned by the
   generalized identifier routines.  is_generalized_identifier_start
   recognizes only the GID_TEMPLATE_ARGS_OPTIONAL and
   GID_DTOR_RECOGNIZED flags. */
typedef unsigned an_identifier_options_set;
#define GID_NO_OPTIONS                0x00u
#define GID_TEMPLATE_ARGS_OPTIONAL    0x01u
			/* Normally if a class template name is seen it must
			   be followed by a template argument list, otherwise
			   an error is issued.  This flag allows the
			   template argument list to be omitted. */
#define GID_DTOR_RECOGNIZED           0x02u
			/* Enables recognition of destructor names
			   ("~" followed by an identifier).  The default
			   is to not recognize "~" as the start of an
			   identifier (no error is issued).  Destructor
			   names are always recognized following qualifiers. */
#define GID_DISALLOW_QUALIFIED_NAME   0x04u
			/* Causes an error to be issued if the identifier
			   is a qualified name (either A::B or ::i). */
#define GID_DISALLOW_GLOBAL_QUALIFIER 0x08u
			/* Causes an error to be issued if the identifier
			   is a global qualifier (::A::B or ::i). */
#define GID_DISALLOW_OPERATOR_NAME    0x10u
			/* Causes an error to be issued if the identifier
			   is an operator name of the form "operator =" or
			   "operator int". */
#define GID_VACUOUS_DTOR_RECOGNIZED   0x20u
			/* Enables recognition of destructor calls, as part
			   of a qualified name, for non-class types and class
			   types that have no destructors (e.g., A::~A or
			   int::~int). */
#define GID_DTOR_MUST_BE_NONCLASS     0x40u
			/* Enables recognition of destructor calls for
			   non-class types and class types that have no
			   destructors that are not part of a qualified name
			   (e.g., ~A or ~int). */
#define GID_IS_TEMPLATE_DECLARATION   0x80u
			/* If the identifier is a qualified name the
			   class component must refer to the prototype
			   instantiation, unless GID_IS_TEMPLATE_SPECIALIZATION
			   is also set. */
#define GID_IS_NEW_TYPE_NAME	      0x100u
			/* Specifies that the name being scanned is the type
			   name in a new expression.  This causes the check
			   for an unexpected template argument list to be
			   suppressed because a new type name may be followed
			   by a less than sign. */
#define GID_IS_FIELD_SELECTION_OPERAND \
				      0x200u
			/* Specifies that the name being scanned is the
			   operand following a "." or "->" operator. */
#define GID_USE_PROTOTYPE_NOT_NONREAL 0x400u
			/* Specifies that a template reference such as
			   A<T> should be considered to refer to the
			   prototype instantiation, not the nonreal
			   instantiation of the same name. */
#define GID_IS_TYPENAME	0x800u
			/* Specifies that the name being looked up follows the
			   typename keyword.  This affects the way that
			   a qualified name is handled. */
#define GID_CLASS_TEMPLATE_REQUIRED 0x1000u
			/* Specifies that a class template name that is not
			   followed by an argument list must be returned as
			   the class template and should not be converted to
			   the current instantiation of the template, if such
			   an instantiation is currently in scope. */
#define GID_IS_TEMPLATE_SPECIALIZATION 0x2000u
			/* If the identifier is a qualified name the
			   identifier named must be a member template. */
#define GID_IS_EXPR_CONTEXT 0x4000u
			/* TRUE if the name is being coalesced in a context
			   that is known to be an expression context.  This
			   causes a "<" to be treated as a less than sign
			   and not the start of a template argument list
			   when it follows a nonreal class member. */
#define GID_IS_CLASS_TEMPLATE_DECL 0x8000u
			/* TRUE if the name being coalesced is the class
			   template name in a class template declaration. */
#define GID_IS_TEMPLATE_PRESCAN 0x10000u
			/* TRUE if the name is being coalesced during the
      			   prescan of a template declaration.  This suppresses
			   certain diagnostics. */
#define GID_FOLLOWS_TEMPLATE 0x20000u
			/* TRUE if the name being coalesced follows the
      			   "template" keyword.  This is used is constructs
			   like "p->template f<x>(1)". */
#define GID_IMPLICIT_TYPE_CONTEXT 0x40000u
			/* TRUE if the name is known to be a type based on
			   context. */
#define GID_IN_IF_EXISTS 0x80000u
			/* TRUE when scanning the identifier of a Microsoft
			   __if_exists or __if_not_exists directive.  Also
			   when scanning the identifier of a using-declaration
			   subject to the using_if_exists attribute. */
#define GID_IS_FRIEND_DECL 0x100000u
			/* TRUE when scanning the declarator of a friend
			   function declaration. */
#define GID_IS_TAG_NAME    0x200000u
			/* TRUE when scanning a tag name. */
#define GID_SIMPLIFY_CURR_CLASS_QUALIFIED_NAME 0x400000u
			/* TRUE if a qualified name of the form Q::X (with no
			   leading "::") should be treated as just X if Q
			   denotes the class currently being defined.
			   Microsoft compilers in particular appear to behave
			   this way. */
#define GID_IS_UNKNOWN_TEMPLATE_ARG    0x800000u
			/* TRUE when scanning an identifier at the beginning
			   of a template argument in an unknown template
			   argument list context. */
#define GID_IS_BASE_CLASS 0x1000000u
			/* TRUE when scanning a base class specifier. */
#define GID_IS_CPPCLI_CONSTRAINT 0x2000000u
			/* TRUE when scanning C++/CLI generic constraint. */
#define GID_IMPLICIT_TYPENAME_CONTEXT 0x4000000u
			/* TRUE if this is a context in which a
			   dependent qualified name is implicitly
			   treated as a type (a C++20 feature). */
#define GID_IS_DTOR_NAME 0x8000000u
			/* TRUE when scanning a destructor or C++/CLI finalizer
			   name. */
#define GID_PERMIT_UNKNOWN_QUALIFIER 0x10000000u
			/* TRUE if a name qualifier that is not a type or
			   namespace (or a known entity at all) should be
			   permitted (e.g., X::name where X is not found). */
#define GID_SUPPRESS_PACK_INDEX_QUALIFIER 0x20000000u
			/* TRUE to suppress recognition of a
			   pack-index-specifier as a nested-name-specifier
			   during type name probing in curr_type_symbol.  This
			   prevents pack-index constructs like T...[N]::name
			   from being partially processed during tentative
			   parses. */

#define GID_ERROR_FLAGS (GID_DISALLOW_QUALIFIED_NAME |		\
			 GID_DISALLOW_GLOBAL_QUALIFIER |	\
			 GID_DISALLOW_OPERATOR_NAME)
			/* Contains all the flags used by the routine
			   check_for_generalized_identifier_errors.
			   Is used to mask the error flags so that errors
			   are only reported once. */
#define GID_CHECK_TAG_NAME_FLAGS (GID_TEMPLATE_ARGS_OPTIONAL |   \
                                  GID_IMPLICIT_TYPE_CONTEXT |    \
                                  GID_IS_TAG_NAME)
			/* Contains the flags used to check for a tag name. */

/* Lookup modes supported by coalesce_and_lookup_generalized_identifier.
   If this list is changed the corresponding array of ID lookup options
   in lexical.c must also be changed.  */
enum an_identifier_lookup_mode {
  ilm_normal,		/* Find any symbol. */
  ilm_tag,		/* Find only tag names. */
  ilm_tentative_type,	/* Uses IDL_TENTATIVE_TYPE_LOOKUP to do the lookup. */
  ilm_ctor_initializer_name,
			/* Used to look up identifiers in the initializer
			   list of a constructor declaration. */
  ilm_qualified_ctor_initializer_name,
			/* Used to look up qualified identifiers in the
                           initializer list of a constructor declaration. */
  ilm_namespace,	/* Find only namespace names. */
  ilm_typename,		/* Uses IDL_TYPENAME_LOOKUP to do the lookup. */
  ilm_class,		/* Find only class names. */
  ilm_template_linkage,	/* Uses IDL_LINKAGE_LOOKUP to do the lookup, create
			   nonreal members as templates. */
  ilm_using_declaration,
			/* Uses IDL_USING_DECLARATION to do the lookup. */
  ilm_using_typename,	/* Uses both IDL_USING_DECLARATION and
			   IDL_TYPENAME_LOOKUP to do the lookup. */
  ilm_expr,		/* Uses IDL_IS_EXPR_CONTEXT. */
  ilm_declarator,	/* Uses IDL_IS_DECLARATOR. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  ilm_static_declarator,
			/* Uses both IDL_IS_DECLARATOR and
			   IDL_IS_STATIC_DECL. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  ilm_template_friend,	/* Uses IDL_FRIEND_LOOKUP to do the lookup, create
			   nonreal members as templates. */
  ilm_template_tag,	/* Uses find tag names and find members of the
			   prototype instantiation, not nonreal members. */
  ilm_template_template_arg,
			/* Uses IDL_TREAT_AS_TEMPLATE_ID. */
  ilm_last
};

extern a_constant_ptr alloc_cached_constant();
extern void free_cached_token_constant(a_constant_ptr cp);

/* Forward declaration of copy_construct_pragma_list and
   destroy_pending_pragma_list (defined in pragma.c). */
namespace detail {
extern void copy_construct_pragma_list(a_pending_pragma_list       *dest,
                                       const a_pending_pragma_list &old_list);
extern void destroy_pending_pragma_list(a_pending_pragma_list *pplp);
}  /* namespace detail */

/*
Forward declaration of types of extra information associated with a token
(defined later in this file).
*/
struct a_pp_token_descr;
struct an_extracted_template_descr;
struct a_removed_expr_descr;
struct a_ud_literal_descr;
struct an_unresolved_ud_literal_descr;
struct a_lexical_ifc_index_reference;

/*
Data structure used to save information about a token so that the token
can be cached and then rescanned.  Note that this is never done with
pp-tokens.  See cache_curr_token et al.
*/
enum a_token_extra_info_kind : a_byte {
  /* Kind of additional information saved in a cached token entry. */
  teik_none,            /* No extra information, i.e., normal token. */
  teik_identifier,      /* Extra information for an identifier. */
  teik_constant,        /* Extra information for a literal constant. */
  teik_pragma,          /* Extra information for a pragma. */
  teik_pp_token,        /* Extra information for a pp token. */
  teik_extracted_body,  /* Extra information for an extracted template body. */
  teik_removed_expr,    /* Extra information for a removed expression. */
  teik_asm_string,      /* Extra information for a Microsoft asm block. */
  teik_insert_string,   /* Extra information for an inserted token string. */
  teik_ud_lit,          /* Extra information for a user-defined literal. */
  teik_unresolved_ud_lit,
                        /* Extra information for a unresolved user-defined
                           literal. */
  teik_ifc_index        /* Extra information for a pseudo-token referring to a
                           an IFC module node. */
};

/*
Structure used to record information about a pp token in a token cache.
*/
struct a_pp_token_descr {
  char		*token_start;
			/* Pointer to the first character of the token. */
  char		*token_end;
			/* Pointer to the last character of the token.
			   The last character will be followed by a null
			   terminator. */
};  /* a_pp_token_descr */
using a_pp_token_descr_ptr = a_pp_token_descr*;

/*
Structure used to record information about a template body that has been
extracted from the enclosing cache.  This is used to mark the body of a member
function or member class template that has been extracted from its enclosing
class template.  This is also used when creating template strings so that the
nested template body can be put out as part of the template string for the
enclosing template.
*/
struct an_extracted_template_descr {
  a_symbol_ptr	symbol;
			/* The symbol associated with the extracted body. */
  a_byte_boolean
		semicolon_inserted;
			/* TRUE if the token with which this body is associated
			   is a semicolon that was inserted after the body
			   was removed. */
};  /* an_extracted_template_descr */

/*
Structure used to record information about a tok_removed_expr token.
*/
struct a_removed_expr_descr {
  inline a_removed_expr_descr();
  inline ~a_removed_expr_descr();
  a_token_cache  *cache;
			/* A cache containing the removed tokens.  */
};  /* a_removed_expr_descr */

/*
Structure used to record information about a user-defined literal token.
*/
struct a_ud_literal_descr {
  INLINE a_ud_literal_descr() = default;
  INLINE a_ud_literal_descr(const a_ud_literal_descr& other);
  INLINE a_ud_literal_descr(a_ud_literal_descr&& other);
  INLINE ~a_ud_literal_descr();
  a_constant_ptr
		value_con;
			/* Pointer to a constant entry (in front end
			   storage) giving the value to be passed as the
			   first argument to the literal operator
			   designated by ud_lit_op_sym. */
  a_constant_ptr
		spelling_con;
			/* Pointer to a ck_string constant entry (in front
			   end storage) containing the characters of the
			   token spelling with which the raw literal
			   operator is to be invoked or the literal
			   operator template is to be instantiated. */
  a_symbol_ptr
		op_sym;	/* The literal operator or literal operator template
			   selected to produce the value of the literal, if
			   any; otherwise, NULL. */
  a_const_char
		*suffix;
			/* The identifier portion of the literal operator
			   or literal operator template name (this is needed
			   when a user-defined literal is used to declare
			   the first literal operator or literal operator
			   template with that name and thus there is no
			   existing symbol for ud_lit_op_sym). */
  a_type_ptr
		type;	/* The type of the literal, to be passed to
			   find_literal_operator when repeating the operator
			   lookup for a cached token. */
};  /* a_ud_literal_descr */


/*
Structure used to record information about an unresolved user-defined literal
token.
*/
struct an_unresolved_ud_literal_descr {
  INLINE an_unresolved_ud_literal_descr() = default;
  INLINE an_unresolved_ud_literal_descr(
                                  const an_unresolved_ud_literal_descr& other);
  INLINE an_unresolved_ud_literal_descr(
                                       an_unresolved_ud_literal_descr&& other);
  INLINE ~an_unresolved_ud_literal_descr();
  a_constant_ptr
		value_con;
			/* Pointer to a constant entry (in front end
			   storage) giving the value to be passed as the
			   first argument to the literal operator
			   designated by ud_lit_op_sym. */
  a_constant_ptr
		spelling_con;
			/* Pointer to a ck_string constant entry (in front
			   end storage) containing the characters of the
			   token spelling with which the raw literal
			   operator is to be invoked or the literal
			   operator template is to be instantiated. */
  a_symbol_locator
		curr_id_locator;
			/* The saved value of locator_for_curr_id. */
};  /* an_unresolved_ud_literal_descr */


/*
A kind enum representing the possible index types represented by an instance
of the a_lexical_ifc_index_reference.
*/
enum a_lexical_ifc_index_kind {
  liik_decl_index,      /* IFC DeclIndex. */
  liik_expr_index,      /* IFC ExprIndex. */
  liik_type_index       /* IFC TypeIndex. */
};

/*
Information on an IFC index to be used in conjunction with a_cached_token
below, saved for later rescanning.  Separated from a_cached_token to allow for
making this information available upon rescanning (see
ifc_index_for_curr_token).
*/
struct a_lexical_ifc_index_reference {
  uint32_t      sort;
			/* The index sort in the IFC file used to address the
			   node being referred to. */
  uint32_t      index;
			/* The index in the IFC file used to address the node
			   being referred to. */
  const void    *file;
			/* An opaque pointer to the IFC module file
			   (an_ifc_module_file) containing this declaration. */
#if DEBUG
/* GCOVR_EXCL_START */
  a_lexical_ifc_index_kind
		reference_kind;
			/* The kind of IFC index being referenced. */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
};  /* a_lexical_ifc_index_reference */

namespace detail {

/*
The base implementation for a_cached_token and an_immutable_cached_token.
*/
struct a_cached_token_base {
  INLINE a_cached_token_base()
    : a_cached_token_base(tok_error)
    {}
  INLINE a_cached_token_base(a_token_kind kind)
    : a_cached_token_base(kind, null_source_position, NO_TOKEN_SEQUENCE_NUMBER)
    {}
  INLINE a_cached_token_base(a_token_kind            kind,
                             const a_source_position &pos,
                             a_token_sequence_number seq)
    : a_cached_token_base(kind, pos, pos, seq, seq)
    {}
  INLINE a_cached_token_base(a_token_kind       kind,
                        const a_source_position &pos,
                        a_token_sequence_number start_seq,
                        a_token_sequence_number end_seq)
    : a_cached_token_base(kind, pos, pos, start_seq, end_seq)
    {}
  INLINE a_cached_token_base(a_token_kind                       kind,
                             const a_source_position            &start_pos,
                             ARG_UNUSED const a_source_position &end_pos,
                             a_token_sequence_number            start_seq,
                             a_token_sequence_number            end_seq)
    : token(kind), source_position(start_pos),
  #if EXTRA_SOURCE_POSITIONS_IN_IL
      end_source_position(end_pos),
  #endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      token_sequence_number(start_seq), ending_token_sequence_number(end_seq),
      extra_info_kind(teik_none)
    {}
  INLINE a_cached_token_base(const a_cached_token_base &other);
  INLINE a_cached_token_base(a_cached_token_base &&other);
  INLINE ~a_cached_token_base();

  INLINE a_token_kind get_kind() const
    { return this->token; }
  INLINE a_token_extra_info_kind get_extra_kind() const
    { return this->extra_info_kind; }

  INLINE a_boolean is(a_token_kind tok_kind) const
    { return this->token == tok_kind; }
  INLINE a_boolean is(a_token_extra_info_kind tok_kind) const
    { return this->extra_info_kind == tok_kind; }

  INLINE a_boolean is_basic() const
    { return this->is(teik_none); }
  INLINE a_boolean is_identifier() const
    { return this->is(teik_identifier); }
  INLINE a_boolean is_constant() const
    { return this->is(teik_constant); }
  INLINE a_boolean is_pp_token() const
    { return this->is(teik_pp_token); }
  INLINE a_boolean is_pragma() const
    { return this->is(teik_pragma); }
  INLINE a_boolean is_string_literal() const
    { return this->is(tok_string_literal) && this->is_constant(); }
  INLINE a_boolean is_extracted_template_body() const
    { return this->is(teik_extracted_body); }
  INLINE a_boolean is_ud_literal() const
    { return this->is(teik_ud_lit); }
  INLINE a_boolean is_unresolved_ud_literal() const
    { return this->is(teik_unresolved_ud_lit); }
  INLINE a_boolean is_asm_string() const
    { return this->is(teik_asm_string); }
  INLINE a_boolean is_insert_string() const
    { return this->is(teik_insert_string); }
  INLINE a_boolean is_ifc_reference() const
    { return this->is(teik_ifc_index); }

  INLINE const a_source_position* get_source_position() const
    { return &this->source_position; }
#if EXTRA_SOURCE_POSITIONS_IN_IL
  INLINE const a_source_position* get_starting_source_position() const
    { return &this->source_position; }
  INLINE const a_source_position* get_ending_source_position() const
    { return &this->end_source_position; }
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  INLINE a_token_sequence_number get_starting_seq_number() const
    { return this->token_sequence_number; }
  INLINE a_token_sequence_number get_ending_seq_number() const
    { return this->ending_token_sequence_number; }

  INLINE a_symbol_locator& get_locator()
    { return this->extra_info.locator; }
  INLINE const a_symbol_locator& get_locator() const
    { return this->extra_info.locator; }
  INLINE a_constant* get_constant()
    { return this->extra_info.constant; }
  INLINE const a_constant* get_constant() const
    { return this->extra_info.constant; }
  INLINE a_symbol_locator* get_symbol_locator()
    { return &this->extra_info.locator; }
  INLINE const a_symbol_locator* get_symbol_locator() const
    { return &this->extra_info.locator; }
  INLINE a_pending_pragma_list* get_pragma_list()
    { return &this->extra_info.pragmas; }
  INLINE const a_pending_pragma_list* get_pragma_list() const
    { return &this->extra_info.pragmas; }
  INLINE a_pp_token_descr* get_pp_token_descr()
    { return &this->extra_info.pp_token_descr; }
  INLINE const a_pp_token_descr* get_pp_token_descr() const
    { return &this->extra_info.pp_token_descr; }
  INLINE an_extracted_template_descr* get_extracted_template_descr()
    { return &this->extra_info.extracted_template; }
  INLINE const an_extracted_template_descr*
  get_extracted_template_descr() const
    { return &this->extra_info.extracted_template; }
  INLINE const a_token_cache* get_removed_expression() const
    { return this->extra_info.removed_expr.cache; }
  INLINE a_ud_literal_descr* get_ud_literal_descr()
    { return &this->extra_info.ud_lit; }
  INLINE const a_ud_literal_descr* get_ud_literal_descr() const
    { return &this->extra_info.ud_lit; }
  INLINE an_unresolved_ud_literal_descr* get_unresolved_ud_literal_descr()
    { return &this->extra_info.unresolved_ud_lit; }
  INLINE const an_unresolved_ud_literal_descr*
  get_unresolved_ud_literal_descr() const
    { return &this->extra_info.unresolved_ud_lit; }
  INLINE char* get_asm_string()
    { return this->extra_info.asm_string; }
  INLINE a_const_char* get_asm_string() const
    { return this->extra_info.asm_string; }
  INLINE a_lexical_ifc_index_reference get_ifc_index() const
    { return this->extra_info.ifc_index; }

  uintptr_t hash_code() const;

  INLINE a_cached_token_base& operator=(const a_cached_token_base &other);
  INLINE a_cached_token_base& operator=(a_cached_token_base &&other);
protected:
  a_token_kind  token;
			/* The token kind (e.g., tok_identifier).  Not valid
			   when extra_info is present and
			   extra_info->kind == teik_pragma.  */
  a_source_position
		source_position;
			/* Source position of the token. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position
		end_source_position;
			/* The end position of the current token -- that is,
			   the source position of the last character of the
			   token. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_token_sequence_number
		token_sequence_number;
			/* The sequence number associated with this token. */
  a_token_sequence_number
		ending_token_sequence_number;
			/* The ending sequence number of this token (different
			   from token_sequence_number for coalesced
			   identifiers). */
  a_token_extra_info_kind
		extra_info_kind;
			/* Indication of the type of extra information about
			   the token provided below. */
#ifdef UNION_AS_STRUCT
/* Workaround for union-as-struct build issue. */
#undef union
#endif /* ifdef UNION_AS_STRUCT */
  union a_variant {
    inline a_variant()
      {}
    inline ~a_variant()
      {}
    /* When extra_info_kind == teik_none, no variant fields. */
    /* When extra_info_kind == teik_identifier: */
    a_symbol_locator
		locator;
			/* Symbol locator for the identifier. */
    /* When extra_info_kind == teik_constant: */
    a_constant_ptr
		constant;
			/* Pointer to a constant entry (in front end storage)
			   giving the value for the literal constant. */
    /* When extra_info_kind == teik_pragma: */
    a_pending_pragma_list
		pragmas;
			/* A list of pragmas associated with the next token
			   in the cache. */
    /* When extra_info_kind == teik_pp_token: */
    a_pp_token_descr
		pp_token_descr;
			/* When a pp token is cached a copy of the string
			   that represents the token is saved as part of
			   the cache. */
    /* When extra_info_kind == teik_extracted_body: */
    an_extracted_template_descr
		extracted_template;
			/* When a template body is removed from a token
			   cache, the semicolon after the member declaration
			   is annotated with an extract template descriptor. */
    /* When extra_info_kind == teik_removed_expr: */
    a_removed_expr_descr
		removed_expr;
			/* When an expression is removed from a token cache, a
			   tok_removed_expr token is injected in its place with
			   the extracted expression tokens accessible via this
			   member. */
    /* When extra_info_kind == teik_asm_string: */
    char	*asm_string;
			/* The string representing a Microsoft asm block. */
    /* When extra_info_kind == teik_ud_lit: */
    a_ud_literal_descr
		ud_lit; /* The extra information required to reconstruct a
			   user-defined literal token. */
    /* When extra_info_kind == teik_unresolved_ud_lit: */
    an_unresolved_ud_literal_descr
		unresolved_ud_lit;
			/* The extra information required to reconstruct a
			   unresolved user-defined literal token. */
    /* When extra_info_kind == teik_ifc_index: */
    a_lexical_ifc_index_reference
		ifc_index;
			/* An index into the IFC module containing additional
			   information. */
  } extra_info;
#ifdef UNION_AS_STRUCT
#define union struct
#endif /* ifdef UNION_AS_STRUCT */
private:
  /* Friend to allow construction to be performed optimally without exposing
     implementation details. */
  friend struct a_token_factory;
};  /* a_cached_token_base */


a_cached_token_base::a_cached_token_base(const a_cached_token_base &other)
/*
Copy-construct a new cached token base object from the given cached token base.
*/
  : token(other.token), source_position(other.source_position),
#if EXTRA_SOURCE_POSITIONS_IN_IL
    end_source_position(other.end_source_position),
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    token_sequence_number(other.token_sequence_number),
    ending_token_sequence_number(other.ending_token_sequence_number),
    extra_info_kind(other.extra_info_kind)
{
  switch (this->extra_info_kind) {
    case teik_none:
    case teik_insert_string:
      break;
    case teik_identifier:
      new (&this->extra_info.locator) a_symbol_locator(
                                                     other.extra_info.locator);
      break;
    case teik_constant:
      { a_constant_ptr new_const = alloc_cached_constant();

        copy_constant(other.extra_info.constant, new_const);
        this->extra_info.constant = new_const;
      }
      break;
    case teik_pragma:
      detail::copy_construct_pragma_list(&this->extra_info.pragmas,
                                         move_from(&other.extra_info.pragmas));
      break;
    case teik_pp_token:
      new (&this->extra_info.pp_token_descr) a_pp_token_descr(
                                              other.extra_info.pp_token_descr);
      break;
    case teik_extracted_body:
      new (&this->extra_info.extracted_template) an_extracted_template_descr(
                                          other.extra_info.extracted_template);
      break;
    case teik_removed_expr:
      new (&this->extra_info.removed_expr) a_removed_expr_descr(
                                                other.extra_info.removed_expr);
      break;
    case teik_asm_string:
      this->extra_info.asm_string = other.extra_info.asm_string;
      break;
    case teik_ud_lit:
      new (&this->extra_info.ud_lit) a_ud_literal_descr(
                                                      other.extra_info.ud_lit);
      break;
    case teik_unresolved_ud_lit:
      new (&this->extra_info.unresolved_ud_lit) an_unresolved_ud_literal_descr(
                                           other.extra_info.unresolved_ud_lit);
      break;
    case teik_ifc_index:
      new (&this->extra_info.ifc_index) a_lexical_ifc_index_reference(
                                                   other.extra_info.ifc_index);
      break;
    default_is_unexpected();
  }  /* switch */
}  /* a_cached_token_base::a_cached_token_base */


a_cached_token_base::a_cached_token_base(a_cached_token_base &&other)
/*
Move-construct a new cached token base object from the given cached token base.
*/
  : token(other.token), source_position(other.source_position),
#if EXTRA_SOURCE_POSITIONS_IN_IL
    end_source_position(other.end_source_position),
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    token_sequence_number(other.token_sequence_number),
    ending_token_sequence_number(other.ending_token_sequence_number),
    extra_info_kind(other.extra_info_kind)
{
  switch (this->extra_info_kind) {
    case teik_none:
    case teik_insert_string:
      break;
    case teik_identifier:
      new (&this->extra_info.locator) a_symbol_locator(
                                         move_from(&other.extra_info.locator));
      break;
    case teik_constant:
      this->extra_info.constant = other.extra_info.constant;
      break;
    case teik_pragma:
      new (&this->extra_info.pragmas) a_pending_pragma_list(
                                         move_from(&other.extra_info.pragmas));
      break;
    case teik_pp_token:
      new (&this->extra_info.pp_token_descr) a_pp_token_descr(
                                  move_from(&other.extra_info.pp_token_descr));
      break;
    case teik_extracted_body:
      new (&this->extra_info.extracted_template) an_extracted_template_descr(
                              move_from(&other.extra_info.extracted_template));
      break;
    case teik_removed_expr:
      new (&this->extra_info.removed_expr) a_removed_expr_descr(
                                    move_from(&other.extra_info.removed_expr));
      break;
    case teik_asm_string:
      this->extra_info.asm_string = other.extra_info.asm_string;
      break;
    case teik_ud_lit:
      new (&this->extra_info.ud_lit) a_ud_literal_descr(
                                          move_from(&other.extra_info.ud_lit));
      break;
    case teik_unresolved_ud_lit:
      new (&this->extra_info.unresolved_ud_lit) an_unresolved_ud_literal_descr(
                               move_from(&other.extra_info.unresolved_ud_lit));
      break;
    case teik_ifc_index:
      new (&this->extra_info.ifc_index) a_lexical_ifc_index_reference(
                                       move_from(&other.extra_info.ifc_index));
      break;
    default_is_unexpected();
  }  /* switch */
  other.extra_info_kind = teik_none;
}  /* a_cached_token_base::a_cached_token_base */


a_cached_token_base::~a_cached_token_base()
/*
Destroy the current cached token base object.
*/
{
  switch (this->extra_info_kind) {
    case teik_none:
    case teik_insert_string:
      break;
    case teik_identifier:
      this->extra_info.locator.~a_symbol_locator();
      break;
    case teik_constant:
      free_cached_token_constant(this->extra_info.constant);
      break;
    case teik_pragma:
      detail::destroy_pending_pragma_list(&this->extra_info.pragmas);
      break;
    case teik_pp_token:
      this->extra_info.pp_token_descr.~a_pp_token_descr();
      break;
    case teik_extracted_body:
      this->extra_info.extracted_template.~an_extracted_template_descr();
      break;
    case teik_removed_expr:
      this->extra_info.removed_expr.~a_removed_expr_descr();
      break;
    case teik_asm_string:
      break;
    case teik_ud_lit:
      this->extra_info.ud_lit.~a_ud_literal_descr();
      break;
    case teik_unresolved_ud_lit:
      this->extra_info.unresolved_ud_lit.~an_unresolved_ud_literal_descr();
      break;
    case teik_ifc_index:
      this->extra_info.ifc_index.~a_lexical_ifc_index_reference();
      break;
    default_is_unexpected();
  }  /* switch */
}  /* a_cached_token_base::~a_cached_token_base */


a_cached_token_base& a_cached_token_base::operator=(
                                        const a_cached_token_base &other)
/*
Copy-assign the given cached token's state into this token.
*/
{
  if (this != &other) {
    destroy(this);
    construct(this, other);
  }  /* if */
  return *this;
}  /* a_cached_token_base::operator= */


a_cached_token_base& a_cached_token_base::operator=(
                                             a_cached_token_base &&other)
/*
Move-assign the given cached token's state into this token.
*/
{
  if (this != &other) {
    destroy(this);
    construct(this, move_from(&other));
  }  /* if */
  return *this;
}  /* a_cached_token_base::operator= */

}  /* namespace detail */

#if EXTRA_SOURCE_POSITIONS_IN_IL
#define extra_il_src_pos(pos) pos
#else /* !EXTRA_SOURCE_POSITIONS_IN_IL */
#define extra_il_src_pos(pos) a_source_position()
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

/*
Information on a single token, saved for later rescanning of the token.
*/
struct an_immutable_cached_token : public detail::a_cached_token_base {
  using detail::a_cached_token_base::a_cached_token_base;

  INLINE an_immutable_cached_token(const detail::a_cached_token_base &val)
    : detail::a_cached_token_base(val)
    {}
  INLINE an_immutable_cached_token(detail::a_cached_token_base &&val)
    : detail::a_cached_token_base(move_from(&val))
    {}
  INLINE ~an_immutable_cached_token() = default;
};  /* an_immutable_cached_token */

/*
Information on a single token, saved for later rescanning of the token.
*/
struct a_cached_token : public detail::a_cached_token_base {
  using detail::a_cached_token_base::a_cached_token_base;

  INLINE a_cached_token(const detail::a_cached_token_base &val)
    : detail::a_cached_token_base(val)
    {}
  INLINE a_cached_token(detail::a_cached_token_base &&val)
    : detail::a_cached_token_base(move_from(&val))
    {}
  INLINE ~a_cached_token() = default;

  INLINE void set_kind(a_token_kind kind)
    { this->token = kind; }
  INLINE void set_source_position(const a_source_position &pos)
    { this->source_position = pos; }
#if EXTRA_SOURCE_POSITIONS_IN_IL
  INLINE void set_starting_source_position(const a_source_position &pos)
    { this->source_position = pos; }
  INLINE void set_ending_source_position(const a_source_position &pos)
    { this->end_source_position = pos; }
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  INLINE void set_seq_number(a_token_sequence_number num)
    { this->set_seq_number(num, num); }
  INLINE void set_seq_number(a_token_sequence_number starting_num,
                             a_token_sequence_number ending_num);
  INLINE void set_constant(a_constant_ptr new_constant)
    { this->extra_info.constant = new_constant; }
};  /* a_cached_token */


using a_shared_token_ctrl_block =
                   detail::Shared_obj_control_block<an_immutable_cached_token>;

EXTERN_THREAD Dyn_array<a_shared_token_ctrl_block*>
		*avail_token_ctrl_blocks;
			/* A list of available memory allocations that can be
			   used to quickly construct a new shared token. */

#if DEBUG
/* GCOVR_EXCL_START */
EXTERN_THREAD unsigned long
		num_shared_token_states_allocated;
			/* Counts of the number of a_shared_token_ctrl_block
			   values allocated. */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

/*
Specialize the allocator for shared token control blocks to improve
performance.
*/
template<>
struct FE_allocator<a_shared_token_ctrl_block> {
  typedef a_shared_token_ctrl_block an_elem;
  typedef Allocation<an_elem> an_allocation;
  typedef FE_allocator<an_elem> an_allocator;
  typedef FE_allocator<an_elem> a_deallocator;
  static void reserve_block();
  INLINE static auto alloc(size_t n) -> an_allocation;
  INLINE static void dealloc(an_allocation allocation);
};  /* FE_allocator */


auto FE_allocator<a_shared_token_ctrl_block>::alloc(size_t n) -> an_allocation
/*
Allocate or reuse a shared token control block and return the resulting
allocation.
*/
{
  /* Only one shared token control block can be allocated at a time. */
  check_assertion(n == 1);
  if (avail_token_ctrl_blocks->is_empty()) {
    FE_allocator<a_shared_token_ctrl_block>::reserve_block();
  }  /* if */

  an_elem *elem = avail_token_ctrl_blocks->back_elem();
  avail_token_ctrl_blocks->pop_back();
  return an_allocation{ elem, 1 };
}  /* FE_allocator::alloc */


void FE_allocator<a_shared_token_ctrl_block>::dealloc(an_allocation a)
/*
Release the given allocation -- which was allocated by the same allocator.
The caller is responsible for ensuring the allocation contains no live
objects.
*/
{
  /* Note that FE_allocator will normally correctly handle a null or
     multi-element allocation.  This specialization diverges slightly given its
     limited scope of use. */
  check_assertion(a.start != NULL &&
                  a.n_bytes_allocated == sizeof(a_shared_token_ctrl_block));
  avail_token_ctrl_blocks->push_back(a.start);
}  /* FE_allocator::dealloc */


using a_shared_token = Shared_obj<an_immutable_cached_token>;
			/* The type of an immutable token shared in multiple
			   places. */

namespace detail {

/*
An iterator for traversing tokens in a token cache.
*/
template<typename a_Derived_type>
struct a_token_cache_iterator_base {
  INLINE a_token_cache_iterator_base(a_token_cache *cache_val,
                                     int           offset_val)
    : cache(cache_val), offset(offset_val)
    {}
  INLINE ~a_token_cache_iterator_base() = default;

  INLINE a_Derived_type operator++();
  INLINE a_Derived_type operator--();
  INLINE a_Derived_type operator+(int adjustment);
  INLINE a_Derived_type operator-(int adjustment);

  template<typename a_Type>
  INLINE sizeof_t operator-(a_token_cache_iterator_base<a_Type> other);

  template<typename a_Type>
  INLINE
  a_boolean operator==(const a_token_cache_iterator_base<a_Type>& other) const;
  template<typename a_Type>
  INLINE
  a_boolean operator!=(const a_token_cache_iterator_base<a_Type>& other) const
    { return !(*this == other); }
protected:
  a_token_cache *cache; /* The cache being read from. */
  int           offset; /* The offset from the start of the token cache for the
                           token represented by this iterator. */
  friend struct EDG_PREFIX::a_token_cache;
};  /* a_token_cache_iterator_base */


template<typename a_Derived_type>
template<typename a_Type>
sizeof_t a_token_cache_iterator_base<a_Derived_type>::operator-(
                                     a_token_cache_iterator_base<a_Type> other)
/*
Return the number of elements between this iterator and the given iterator.
This iterator's offset must be greater than or equal to the other iterator's
offset.  Additionally, both iterators must point to the same cache.
*/
{
  check_assertion(this->cache == other.cache && this->offset >= other.offset);
  return (sizeof_t)(this->offset - other.offset);
}  /* a_token_cache_iterator_base::operator- */

}  /* namespace detail */

/*
An iterator for traversing tokens in a token cache.
*/
struct a_token_cache_iterator :
                  detail::a_token_cache_iterator_base<a_token_cache_iterator> {
  INLINE a_token_cache_iterator()
    : a_token_cache_iterator_base(NULL, 0)
    {}
  INLINE a_token_cache_iterator(a_token_cache *cache_val,
                         int           offset_val)
    : a_token_cache_iterator_base(cache_val, offset_val)
    {}
  INLINE a_token_cache_iterator(const a_token_cache_iterator&) = default;
  INLINE ~a_token_cache_iterator() = default;

  INLINE a_shared_token& operator*() const;

  friend struct a_const_token_cache_iterator;
#if DEBUG
/* GCOVR_EXCL_START */
  friend void db_tokens(a_token_cache_iterator it);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
};  /* a_token_cache_iterator */


/*
An iterator for traversing const tokens in a const token cache.
*/
struct a_const_token_cache_iterator :
            detail::a_token_cache_iterator_base<a_const_token_cache_iterator> {
  INLINE a_const_token_cache_iterator()
    : a_token_cache_iterator_base(NULL, 0)
    {}
  INLINE a_const_token_cache_iterator(const a_token_cache *cache_val,
                                      int                 offset_val)
    : a_token_cache_iterator_base(const_cast<a_token_cache*>(cache_val),
                                  offset_val)
    {}
  INLINE a_const_token_cache_iterator(const a_token_cache_iterator &it)
    : a_token_cache_iterator_base(it.cache, it.offset)
    {}
  INLINE a_const_token_cache_iterator(
                                const a_const_token_cache_iterator&) = default;
  INLINE ~a_const_token_cache_iterator() = default;

  INLINE const a_shared_token& operator*() const;
#if DEBUG
/* GCOVR_EXCL_START */
  friend void db_tokens(a_const_token_cache_iterator it);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
};  /* a_const_token_cache_iterator */


using a_reverse_token_cache_iterator = Reverse_iter<a_token_cache_iterator>;
			/* The type for a reversed a_token_cache_iterator. */

using a_reverse_const_token_cache_iterator =
                                    Reverse_iter<a_const_token_cache_iterator>;
			/* The type for a reversed
			   a_const_token_cache_iterator. */


template<>
INLINE sizeof_t distance(a_token_cache_iterator begin,
                         a_token_cache_iterator end)
/*
Return the number of elements in the range [begin, end).
*/
{
  return end - begin;
}  /* distance */


template<>
INLINE sizeof_t distance(a_const_token_cache_iterator begin,
                         a_const_token_cache_iterator end)
/*
Return the number of elements in the range [begin, end).
*/
{
  return end - begin;
}  /* distance */


/*
Data structure used to hold a token cache, i.e., some number of tokens that are
being saved for later rescanning.
*/
struct a_token_cache {
  INLINE a_token_cache(a_boolean is_reusable_val = FALSE,
                       size_t    initial_capacity = 0)
    : is_reusable(is_reusable_val), tokens(initial_capacity)
    {}
  INLINE a_token_cache(const a_token_cache &other) = default;
  INLINE a_token_cache(a_token_cache &&other) = default;
  ~a_token_cache() = default;

  INLINE a_boolean is_empty() const
    { return this->tokens.is_empty(); }
  INLINE size_t length() const
    { return this->tokens.length(); }
  INLINE size_t capacity() const
    { return this->tokens.capacity(); }

  INLINE void append_token(const a_cached_token &token);
  INLINE void append_token(a_cached_token &&token);
  INLINE void append_token(const a_shared_token &token);
  INLINE void append_token(a_shared_token &&token);
  INLINE void insert_token(a_token_cache_iterator it,
                           const a_cached_token   &tok);
  INLINE void insert_token(a_token_cache_iterator it,
                           a_cached_token         &&tok);
  INLINE void insert_token(a_token_cache_iterator it,
                           const a_shared_token   &tok);
  INLINE void insert_token(a_token_cache_iterator it,
                           a_shared_token         &&tok);
  INLINE void move_copy_tokens(a_token_cache_iterator it,
                               a_token_cache_iterator end_it);
  INLINE void remove_token_range(a_token_cache_iterator it,
                                 a_token_cache_iterator end_it);

  INLINE const a_shared_token& get_first_token() const;
  INLINE const a_shared_token& get_last_token() const;

  INLINE void find_first_and_last(
                                a_token_sequence_number first_token_number,
                                a_token_sequence_number last_token_number,
                                a_token_cache_iterator  *before_first_token_it,
                                a_token_cache_iterator  *last_token_it);
  INLINE void find_first_and_last(
                           a_token_sequence_number      first_token_number,
                           a_token_sequence_number      last_token_number,
                           a_const_token_cache_iterator *before_first_token_it,
                           a_const_token_cache_iterator *last_token_it) const;

  INLINE a_token_cache_iterator get_last_token_iter()
    { return a_token_cache_iterator(this, (int)(this->length() - 1)); }
  INLINE a_const_token_cache_iterator get_last_token_iter() const
    { return a_const_token_cache_iterator(this, (int)(this->length() - 1)); }

  INLINE void remove_first_token();
  INLINE void remove_token(a_token_cache_iterator it);
  INLINE void remove_tokens(a_token_cache_iterator it);
  INLINE void remove_non_pragma_tokens_after(a_token_cache_iterator it);
  INLINE void clear()
    { this->tokens.clear(); }

  INLINE void reserve(size_t amt)
    { this->tokens.reserve(amt); }

  INLINE a_token_cache_iterator begin()
    { return a_token_cache_iterator(this, 0); }
  INLINE a_token_cache_iterator end()
    { return a_token_cache_iterator(this, (int)this->length()); }
  INLINE a_const_token_cache_iterator begin() const
    { return a_const_token_cache_iterator(this, 0); }
  INLINE a_const_token_cache_iterator end() const
    { return a_const_token_cache_iterator(this, (int)this->tokens.length()); }

  INLINE a_reverse_token_cache_iterator rbegin()
    { return {a_token_cache_iterator(this, (int)(this->length() - 1))}; }
  INLINE a_reverse_token_cache_iterator rend()
    { return {a_token_cache_iterator(this, -1)}; }
  INLINE a_reverse_const_token_cache_iterator rbegin() const
    { return {a_const_token_cache_iterator(this, (int)(this->length() - 1))}; }
  INLINE a_reverse_const_token_cache_iterator rend() const
    { return {a_const_token_cache_iterator(this, -1)}; }

  INLINE a_shared_token& operator[](size_t idx)
    { return this->tokens[idx]; }
  INLINE const a_shared_token& operator[](size_t idx) const
    { return this->tokens[idx]; }

  INLINE a_token_cache& operator=(const a_token_cache &other) = default;
  INLINE a_token_cache& operator=(a_token_cache &&other) = default;

  a_bit_field   is_reusable : 1;
			/* TRUE if this cache will be reused (e.g.,
			   for a template cache).  This should be TRUE if
			   there is any possibility that the cache may
			   be reused. */
private:
  Dyn_array<a_shared_token>
		tokens;
			/* The list of tokens. */
  inline void check_iterator(a_token_cache_iterator it);
  friend void rescan_cached_tokens(a_token_cache *cache,
                  /* Defaulted: */ a_boolean     discard_curr_token);
};  /* a_token_cache */

using a_shared_token_cache = Shared_obj<a_token_cache>;
			/* The type used for shared token caches. */

EXTERN_THREAD Dyn_array<a_token_cache*>
		*avail_scanning_token_caches;
			/* A list of available token caches to be used by
			   instances of a_scanning_token_cache (see
			   a_scanning_token_cache for more information). */

EXTERN_THREAD Dyn_array<a_token_cache*>
		*avail_tiny_scanning_token_caches;
			/* A list of available token caches to be used by
			   instances of a_tiny_scanning_token_cache (see
			   a_tiny_scanning_token_cache for more
			   information). */

/*
This is a specialized token cache used for temporary scanning of tokens.

If the token cache is guaranteed to be very small (practically always less than
10 tokens), consider using a_tiny_scanning_token_cache.
*/
struct a_scanning_token_cache {
  INLINE a_scanning_token_cache(a_boolean is_reusable = FALSE);
  a_scanning_token_cache(const a_scanning_token_cache&) = delete;
  INLINE a_scanning_token_cache(a_scanning_token_cache &&other);
  INLINE ~a_scanning_token_cache();

  INLINE a_token_cache& operator*() const
    { return *this->backing_cache; }
  INLINE a_token_cache* operator->() const
    { return this->backing_cache; }
  INLINE a_token_cache* ptr() const
    { return this->backing_cache; }

  INLINE a_scanning_token_cache& operator=(a_scanning_token_cache &&other);
private:
  a_token_cache *backing_cache;
			/* The token cache backing the scanning token cache. */
};  /* a_scanning_token_cache */


/*
This is a specialized token cache used for temporary scanning of a very small
numbers of tokens (ideally guaranteed to be less than 10 tokens).
*/
struct a_tiny_scanning_token_cache {
  INLINE a_tiny_scanning_token_cache(a_boolean is_reusable = FALSE);
  a_tiny_scanning_token_cache(const a_tiny_scanning_token_cache&) = delete;
  a_tiny_scanning_token_cache(a_tiny_scanning_token_cache&&) = delete;
  INLINE ~a_tiny_scanning_token_cache();

  INLINE a_token_cache& operator*() const
    { return *this->backing_cache; }
  INLINE a_token_cache* operator->() const
    { return this->backing_cache; }
  INLINE a_token_cache* ptr() const
    { return this->backing_cache; }
private:
  a_token_cache *backing_cache;
			/* The token cache backing the scanning token cache. */
};  /* a_tiny_scanning_token_cache */


a_scanning_token_cache::a_scanning_token_cache(a_boolean is_reusable)
/*
Construct a new scanning token cache.  If is_reusable is TRUE, the backing
cache will be marked as reusable.
*/
{
  if (avail_scanning_token_caches->is_empty()) {
    this->backing_cache = new_fe<a_token_cache>(is_reusable,
                                                /*capacity=*/50u);
  } else {
    this->backing_cache = avail_scanning_token_caches->back_elem();
    this->backing_cache->is_reusable = is_reusable;
    avail_scanning_token_caches->pop_back();
  }  /* if */
}  /* a_scanning_token_cache::a_scanning_token_cache */


a_scanning_token_cache::a_scanning_token_cache(a_scanning_token_cache &&other)
/*
Move-construct a new scanning token cache.
*/
  : backing_cache(other.backing_cache)
{
  other.backing_cache = NULL;
}  /* a_scanning_token_cache::a_scanning_token_cache */


a_scanning_token_cache::~a_scanning_token_cache()
/*
Destroy the current scanning token cache releasing the underlying cache
back to the list of available scanning token caches.
*/
{
  if (this->backing_cache != NULL) {
    size_t insertion_point = avail_scanning_token_caches->length();

    this->backing_cache->clear();
    for (; insertion_point != 0; --insertion_point) {
      if ((*avail_scanning_token_caches)[insertion_point - 1]->capacity() <=
          this->backing_cache->capacity()) {
        break;
      }  /* if */
    }  /* for */
    avail_scanning_token_caches->insert(insertion_point,
                                        this->backing_cache);
  }  /* if */
}  /* a_scanning_token_cache::~a_scanning_token_cache */


a_scanning_token_cache& a_scanning_token_cache::operator=(
                                                a_scanning_token_cache &&other)
/*
Move-assign the backing cache from the given scanning token cache to this token
cache.  The caches are swapped to ensure no scanning token cache may be
permanently lost.
*/
{
  swap_at(&this->backing_cache, &other.backing_cache);
  return *this;
}  /* a_scanning_token_cache::operator= */


a_tiny_scanning_token_cache::a_tiny_scanning_token_cache(a_boolean is_reusable)
/*
Construct a new tiny scanning token cache.  If is_reusable is TRUE, the backing
cache will be marked as reusable.
*/
{
  if (avail_tiny_scanning_token_caches->is_empty()) {
    this->backing_cache = new_fe<a_token_cache>(is_reusable,
                                                /*capacity=*/10u);
  } else {
    this->backing_cache = avail_tiny_scanning_token_caches->back_elem();
    this->backing_cache->is_reusable = is_reusable;
    avail_tiny_scanning_token_caches->pop_back();
  }  /* if */
}  /* a_tiny_scanning_token_cache::a_tiny_scanning_token_cache */


a_tiny_scanning_token_cache::~a_tiny_scanning_token_cache()
/*
Destroy the current tiny scanning token cache releasing the underlying cache
back to the list of available scanning token caches.
*/
{
  this->backing_cache->clear();
  avail_tiny_scanning_token_caches->push_back(this->backing_cache);
}  /* a_tiny_scanning_token_cache::~a_tiny_scanning_token_cache */


/* These includes are placed here so that a_token_cache will be defined
   for general use before including these files. */
#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */

/* Array of opname kinds indexed by token kind. */
EXTERN_CONSTINIT_ARRAY(an_opname_kind, opname_kind_for_token, tok_last + 1)
#if VAR_INITIALIZERS
= {
   onk_none,          /* tok_error */
   onk_none,          /* tok_identifier */
   onk_none,          /* tok_float_constant */
   onk_none,          /* tok_fixed_point_constant */
   onk_none,          /* tok_int_constant */
   onk_none,          /* tok_char_constant */
   onk_none,          /* tok_gen_constant */
   onk_none,          /* tok_string_literal */
   onk_none,          /* tok_ud_literal */
   onk_none,          /* tok_end_of_source */
   onk_none,          /* tok_newline */
   onk_none,          /* tok_header_name */
   onk_none,          /* tok_pp_number */
   onk_none,          /* tok_digit_sequence */
   onk_none,          /* tok_cpp_quote */
   onk_none,          /* tok_ptr_to_member */
   onk_none,          /* tok_removed_default_arg */
   onk_none,          /* tok_removed_template_body */
#if MICROSOFT_EXTENSIONS_ALLOWED
   onk_none,          /* tok_cli_typeid */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
   onk_none,          /* tok_decltype_construct */
   onk_none,          /* tok_pending_ifc_expr  */
   onk_none,          /* tok_ifc_entity_ref */
   onk_none,          /* tok_ifc_decl_ref */
   onk_none,          /* tok_ifc_type_ref */
   onk_none,          /* tok_ifc_param_ref */
   onk_none,          /* tok_ifc_decl */
   onk_none,          /* tok_unresolved_ud_literal*/
   onk_none,          /* tok_unimplemented */
   onk_subscript,     /* operator[] starts with tok_lbracket */
   onk_none,          /* tok_rbracket */
   onk_function_call, /* operator() starts with tok_lparen */
   onk_none,          /* tok_rparen */
   onk_none,          /* tok_period */
   onk_arrow,
   onk_plus_plus,
   onk_minus_minus,
   onk_ampersand,
   onk_star,
   onk_plus,
   onk_minus,
   onk_compl,
   onk_not,
   onk_divide,
   onk_remainder,
   onk_shift_left,
   onk_shift_right,
   onk_lt,
   onk_gt,
   onk_le,
   onk_ge,
   onk_eq,
   onk_ne,
   onk_spaceship,
   onk_none,          /* tok_caret_caret */
   onk_excl_or,
   onk_or,
   onk_and_and,
   onk_or_or,
   onk_question,      /* Used only in front end. */
   onk_none,          /* tok_colon */
   onk_assign,
   onk_times_assign,
   onk_divide_assign,
   onk_remainder_assign,
   onk_plus_assign,
   onk_minus_assign,
   onk_shift_left_assign,
   onk_shift_right_assign,
   onk_and_assign,
   onk_excl_or_assign,
   onk_or_assign,
   onk_comma,
   onk_none,          /* tok_sharp */
   onk_none,          /* tok_paste */
   onk_gnu_min,       /* tok_gnu_min */
   onk_gnu_max,       /* tok_gnu_max */
   onk_none,          /* tok_lbrace */
   onk_none,          /* tok_rbrace */
   onk_none,          /* tok_lsplice */
   onk_none,          /* tok_rsplice */
   onk_none,          /* tok_backslash */
   onk_none,          /* tok_semicolon */
   onk_none,          /* tok_ellipsis */
   onk_none,          /* tok_auto */
   onk_none,          /* tok_break */
   onk_none,          /* tok_case */
   onk_none,          /* tok_char */
   onk_none,          /* tok_const */
   onk_none,          /* tok_continue */
   onk_none,          /* tok_default */
   onk_none,          /* tok_do */
   onk_none,          /* tok_double */
   onk_none,          /* tok_else */
   onk_none,          /* tok_enum */
   onk_none,          /* tok_extern */
   onk_none,          /* tok_float */
   onk_none,          /* tok_for */
   onk_none,          /* tok_goto */
   onk_none,          /* tok_if */
   onk_none,          /* tok_int */
   onk_none,          /* tok_bit_precise_int */
   onk_none,          /* tok_long */
   onk_none,          /* tok_register */
   onk_none,          /* tok_return */
   onk_none,          /* tok_short */
   onk_none,          /* tok_signed */
   onk_none,          /* tok_sizeof */
   onk_none,          /* tok_static */
   onk_none,          /* tok_struct */
   onk_none,          /* tok_switch */
   onk_none,          /* tok_typedef */
   onk_none,          /* tok_union */
   onk_none,          /* tok_unsigned */
   onk_none,          /* tok_void */
   onk_none,          /* tok_volatile */
   onk_none,          /* tok_while */
   onk_none,          /* tok_c99_generic */
   onk_none,          /* tok_c99_genericfx */
   onk_none,          /* tok_ext_alignof */
   onk_none,          /* tok_intaddr */
   onk_none,          /* tok_va_start */
   onk_none,          /* tok_va_arg */
   onk_none,          /* tok_va_end */
   onk_none,          /* tok_va_copy */
   onk_none,          /* tok_builtin_offsetof */
   onk_none,          /* tok_restrict */
   onk_none,          /* tok_gnu_restrict */
   onk_none,          /* tok_c99_bool */
   onk_none,          /* tok_c99_complex */
   onk_none,          /* tok_c99_imaginary */
   onk_none,          /* tok_imaginary_unit */
   onk_none,          /* tok_nan */
   onk_none,          /* tok_infinity */
   onk_none,          /* tok_char16_t */
   onk_none,          /* tok_char32_t */
   onk_none,          /* tok_char8_t */
   onk_none,          /* tok_fract */
   onk_none,          /* tok_accum */
   onk_none,          /* tok_sat */
   onk_none,          /* tok_declspec */
#if MICROSOFT_EXTENSIONS_ALLOWED
   onk_none,          /* tok_abstract */
   onk_none,          /* tok_sealed */
   onk_none,          /* tok_cdecl */
   onk_none,          /* tok_fastcall */
   onk_none,          /* tok_stdcall */
   onk_none,          /* tok_thiscall */
   onk_none,          /* tok_vectorcall */
   onk_none,          /* tok_clrcall */
   onk_none,          /* tok_microsoft_inline */
   onk_none,          /* tok_forceinline */
   onk_none,          /* tok_unaligned */
   onk_none,          /* tok_microsoft_try */
   onk_none,          /* tok_finally */
   onk_none,          /* tok_leave */
   onk_none,          /* tok_except */
   onk_none,          /* tok_int8 */
   onk_none,          /* tok_int16 */
   onk_none,          /* tok_int32 */
   onk_none,          /* tok_int64 */
   onk_none,          /* tok_based */
   onk_none,          /* tok_uuidof */
   onk_none,          /* tok_assume */
   onk_none,          /* tok_charize */
   onk_none,          /* tok_if_exists */
   onk_none,          /* tok_if_not_exists */
   onk_none,          /* tok_end_of_if_exists */
   onk_none,          /* tok_super */
   onk_none,          /* tok_noop */
   onk_none,          /* tok_interface */
   onk_none,          /* tok_event */
   onk_none,          /* tok_microsoft_ptr32 */
   onk_none,          /* tok_microsoft_ptr64 */
   onk_none,          /* tok_microsoft_sptr */
   onk_none,          /* tok_microsoft_uptr */
   onk_none,          /* tok_microsoft_w64 */
   onk_none,          /* tok_microsoft_Lprefix */
   onk_none,          /* tok_microsoft_lprefix */
   onk_none,          /* tok_microsoft_Uprefix */
   onk_none,          /* tok_microsoft_uprefix */
   onk_none,          /* tok_microsoft_identifier */
   onk_none,          /* tok_uuid */
   onk_none,          /* tok_in */
   onk_none,          /* tok_gcnew */
   onk_none,          /* tok_safe_cast */
   onk_none,          /* tok_implements */
   onk_none,          /* tok_unresolved_type */
   onk_none,          /* tok_for_each */
   onk_none,          /* tok_ref_class */
   onk_none,          /* tok_ref_struct */
   onk_none,          /* tok_value_class */
   onk_none,          /* tok_value_struct */
   onk_none,          /* tok_enum_class */
   onk_none,          /* tok_enum_struct */
   onk_none,          /* tok_interface_class */
   onk_none,          /* tok_interface_struct */
   onk_none,          /* tok_ref_new */
   onk_none,          /* tok_partial_ref_class */
   onk_none,          /* tok_partial_ref_struct */
   onk_none,          /* tok_prefix_ref */
   onk_none,          /* tok_prefix_value */
   onk_none,          /* tok_prefix_interface */
   onk_none,          /* tok_prefix_for */
   onk_none,          /* tok_prefix_enum */
   onk_none,          /* tok_prefix_partial */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
   onk_none,          /* tok_microsoft_asm */
   onk_none,          /* tok_func_name */
   onk_none,          /* tok_function_name */
   onk_none,          /* tok_pretty_function_name */
   onk_none,          /* tok_decorated_function_name */
#if NEAR_AND_FAR_ALLOWED
   onk_none,          /* tok_near */
   onk_none,          /* tok_far */
#endif /* NEAR_AND_FAR_ALLOWED */
   onk_none,          /* tok_attribute */
#if GNU_EXTENSIONS_ALLOWED
   onk_none,          /* tok_builtin_types_compatible */
   onk_none,          /* tok_gnu_real */
   onk_none,          /* tok_gnu_imag */
#endif /* GNU_EXTENSIONS_ALLOWED */
   onk_none,          /* tok_colon_colon */
   onk_none,          /* tok_period_star */
   onk_arrow_star,
   onk_none,          /* tok_asm */
   onk_none,          /* tok_catch */
   onk_none,          /* tok_class */
   onk_delete,
   onk_none,          /* tok_friend */
   onk_none,          /* tok_inline */
   onk_new,
   onk_none,          /* tok_operator */
   onk_none,          /* tok_private */
   onk_none,          /* tok_protected */
   onk_none,          /* tok_public */
   onk_none,          /* tok_template */
   onk_none,          /* tok_this */
   onk_none,          /* tok_throw */
   onk_none,          /* tok_try */
   onk_none,          /* tok_virtual */
   onk_none,          /* tok_wchar_t */
   onk_none,          /* tok_const_cast */
   onk_none,          /* tok_dynamic_cast */
   onk_none,          /* tok_explicit */
   onk_none,          /* tok_cpp98_export */
   onk_none,          /* tok_export */
   onk_none,          /* tok_export_keyword */
   onk_none,          /* tok_import */
   onk_none,          /* tok_module */
   onk_none,          /* tok_mutable */
   onk_none,          /* tok_namespace */
   onk_none,          /* tok_reinterpret_cast */
   onk_none,          /* tok_static_cast */
   onk_none,          /* tok_typeid */
   onk_none,          /* tok_using */
   onk_none,          /* tok_bool */
   onk_none,          /* tok_false */
   onk_none,          /* tok_true */
   onk_none,          /* tok_typename */
   onk_none,          /* tok_static_assert */
   onk_none,          /* tok_decltype */
   onk_none,          /* tok_auto_type */
   onk_none,          /* tok_extension */
   onk_none,          /* tok_null */
   onk_none,          /* tok_typeof */
   onk_none,          /* tok_typeof_unqual */
   onk_none,          /* tok_overload */
#if SUN_EXTENSIONS_ALLOWED
   onk_none,          /* tok_global_link_scope */
   onk_none,          /* tok_symbolic_link_scope */
   onk_none,          /* tok_hidden_link_scope */
#endif /* SUN_EXTENSIONS_ALLOWED */
   onk_none,          /* tok_thread */
   onk_none,          /* tok_thread_local */
   onk_none,          /* tok_c11_thread_local */
#if UPC_EXTENSIONS_ALLOWED
   onk_none,          /* tok_upc_strict */
   onk_none,          /* tok_upc_relaxed */
   onk_none,          /* tok_upc_shared */
   onk_none,          /* tok_upc_forall */
   onk_none,          /* tok_upc_barrier */
   onk_none,          /* tok_upc_notify */
   onk_none,          /* tok_upc_wait */
   onk_none,          /* tok_upc_fence */
   onk_none,          /* tok_upc_threads */
   onk_none,          /* tok_upc_mythread */
   onk_none,          /* tok_upc_blocksizeof */
   onk_none,          /* tok_upc_localsizeof */
   onk_none,          /* tok_upc_elemsizeof */
#endif /* UPC_EXTENSIONS_ALLOWED */
   onk_none,          /* tok_has_assign */
   onk_none,          /* tok_has_copy */
   onk_none,          /* tok_has_nothrow_assign */
   onk_none,          /* tok_has_nothrow_constructor */
   onk_none,          /* tok_has_nothrow_copy */
   onk_none,          /* tok_has_trivial_assign */
   onk_none,          /* tok_has_trivial_constructor */
   onk_none,          /* tok_has_trivial_copy */
   onk_none,          /* tok_has_trivial_destructor */
   onk_none,          /* tok_has_user_destructor */
   onk_none,          /* tok_has_virtual_destructor */
   onk_none,          /* tok_is_abstract */
   onk_none,          /* tok_is_base_of */
   onk_none,          /* tok_is_class */
   onk_none,          /* tok_is_convertible_to */
   onk_none,          /* tok_is_convertible */
   onk_none,          /* tok_is_nothrow_convertible */
   onk_none,          /* tok_is_empty */
   onk_none,          /* tok_is_enum */
   onk_none,          /* tok_is_scoped_enum */
   onk_none,          /* tok_is_pod */
   onk_none,          /* tok_is_polymorphic */
   onk_none,          /* tok_is_union */
   onk_none,          /* tok_is_trivial */
   onk_none,          /* tok_is_standard_layout */
   onk_none,          /* tok_is_trivially_copyable */
   onk_none,          /* tok_is_literal_type */
   onk_none,          /* tok_has_trivial_move_constructor */
   onk_none,          /* tok_has_trivial_move_assign */
   onk_none,          /* tok_has_nothrow_move_assign */
   onk_none,          /* tok_is_invocable */
   onk_none,          /* tok_is_nothrow_invocable */
   onk_none,          /* tok_is_constructible */
   onk_none,          /* tok_is_nothrow_constructible */
   onk_none,          /* tok_is_trivially_constructible */
   onk_none,          /* tok_is_destructible */
   onk_none,          /* tok_is_nothrow_destructible */
   onk_none,          /* tok_is_trivially_destructible */
   onk_none,          /* tok_is_nothrow_assignable */
   onk_none,          /* tok_is_trivially_assignable */
   onk_none,          /* tok_is_valid_winrt_type */
   onk_none,          /* tok_underlying_type */
#if MICROSOFT_EXTENSIONS_ALLOWED
   onk_none,          /* tok_has_finalizer */
   onk_none,          /* tok_is_delegate */
   onk_none,          /* tok_is_interface_class */
   onk_none,          /* tok_is_ref_array */
   onk_none,          /* tok_is_ref_class */
   onk_none,          /* tok_is_sealed */
   onk_none,          /* tok_is_simple_value_class */
   onk_none,          /* tok_is_value_class */
   onk_none,          /* tok_is_win_class */
   onk_none,          /* tok_is_win_interface */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED*/
   onk_none,          /* tok_nullptr */
#if MICROSOFT_EXTENSIONS_ALLOWED
   onk_none,          /* tok_native_nullptr */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
   onk_none,          /* tok_internal_alias_decl */
#if INT128_EXTENSIONS_ALLOWED
   onk_none,          /* tok_int128 */
#endif /* INT128_EXTENSIONS_ALLOWED */
   onk_none,          /* tok_override */
   onk_none,          /* tok_final */
   onk_none,          /* tok_is_final */
   onk_none,          /* tok_noexcept */
   onk_none,          /* tok_constexpr */
   onk_none,          /* tok_consteval */
   onk_none,          /* tok_constinit */
   onk_none,          /* tok_alignof */
   onk_none,          /* tok_alignas */
#if GNU_EXTENSIONS_ALLOWED
   onk_none,          /* tok_bases */
   onk_none,          /* tok_direct_bases */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if GNU_VECTOR_TYPES_ALLOWED
   onk_none,           /* tok_builtin_shuffle */
   onk_none,           /* tok_builtin_shufflevector */
   onk_none,           /* tok_builtin_convertvector */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
   onk_none,           /* tok_noreturn */
   onk_none,           /* tok_builtin_complex */
   onk_none,           /* tok_c11_generic */
   onk_none,           /* tok_c11_atomic */
   onk_none,           /* tok_nullable */
   onk_none,           /* tok_nonnull */
   onk_none,           /* tok_null_unspecified */
   onk_none,           /* tok_coroutine_yield */
   onk_none,           /* tok_coroutine_return */
   onk_await,          /* tok_coroutine_await */
   onk_none,           /* tok_is_assignable */
#if MICROSOFT_EXTENSIONS_ALLOWED
   onk_none,           /* tok_is_trivially_copy_assignable */
   onk_none,           /* tok_is_assignable_no_precondition_check */
#endif  /* MICROSOFT_EXTENSIONS_ALLOWED */
   onk_none,           /* tok_builtin_addressof */
   onk_none,           /* tok_edg_internal_type */
   onk_none,           /* tok_edg_vector_type */
   onk_none,           /* tok_edg_neon_vector_type */
   onk_none,           /* tok_edg_neon_polyvector_type */
   onk_none,           /* tok_edg_scalable_vector_type */
   onk_none,           /* tok_edg_size_type */
   onk_none,           /* tok_edg_ptrdiff_type */
   onk_none,           /* tok_edg_bool_type */
   onk_none,           /* tok_edg_wchar_type */
   onk_none,           /* tok_edg_throw */
   onk_none,           /* tok_edg_internal_opnd */
   onk_none,           /* tok_clang_version */
   onk_none,           /* tok_datasizeof */
   onk_none,           /* tok_has_unique_object_representations */
   onk_none,           /* tok_is_aggregate */
   onk_none,           /* tok_integer_pack */
   onk_none,           /* tok_reference_binds_to_temporary */
   onk_none,           /* tok_reference_constructs_to_temporary */
   onk_none,           /* tok_reference_converts_to_temporary */
   onk_none,           /* tok_is_same */
   onk_none,           /* tok_is_same_as */
   onk_none,           /* tok_is_function */
   onk_none,           /* tok_requires */
   onk_none,           /* tok_concept */
   onk_none,           /* tok_builtin_has_attribute */
   onk_none,           /* tok_builtin_bit_cast */
   onk_none,           /* tok_is_layout_compatible */
   onk_none,           /* tok_is_pointer_interconvertible_base_of */
   onk_none,           /* tok_is_pointer_interconvertible_with_class */
   onk_none,           /* tok_builtin_is_pointer_interconvertible_with_class */
   onk_none,           /* tok_is_corresponding_member */
   onk_none,           /* tok_builtin_is_corresponding_member */
   onk_none,           /* tok_is_deducible */
   onk_none,           /* tok_is_array */
   onk_none,           /* tok_array_rank */
   onk_none,           /* tok_array_extent */
   onk_none,           /* tok_is_arithmetic */
   onk_none,           /* tok_is_complete_type */
   onk_none,           /* tok_is_compound */
   onk_none,           /* tok_is_const */
   onk_none,           /* tok_is_floating_point */
   onk_none,           /* tok_is_fundamental */
   onk_none,           /* tok_is_integral */
   onk_none,           /* tok_is_lvalue_reference */
   onk_none,           /* tok_is_member_function_pointer */
   onk_none,           /* tok_is_member_object_pointer */
   onk_none,           /* tok_is_member_pointer */
   onk_none,           /* tok_is_object */
   onk_none,           /* tok_is_pointer */
   onk_none,           /* tok_is_reference */
   onk_none,           /* tok_is_rvalue_reference */
   onk_none,           /* tok_is_scalar */
   onk_none,           /* tok_is_signed */
   onk_none,           /* tok_is_unsigned */
   onk_none,           /* tok_is_void */
   onk_none,           /* tok_is_volatile */
   onk_none,           /* tok_float32 */
   onk_none,           /* tok_float32x */
   onk_none,           /* tok_float64 */
   onk_none,           /* tok_float64x */
   onk_none,           /* tok_float128 */
   onk_none,           /* tok_is_bounded_array */
   onk_none,           /* tok_is_unbounded_array */
   onk_none,           /* tok_is_referenceable */
   onk_none,           /* tok_add_lvalue_reference */
   onk_none,           /* tok_add_pointer */
   onk_none,           /* tok_add_rvalue_reference */
   onk_none,           /* tok_decay */
   onk_none,           /* tok_make_signed */
   onk_none,           /* tok_make_unsigned */
   onk_none,           /* tok_remove_all_extents */
   onk_none,           /* tok_remove_const */
   onk_none,           /* tok_remove_cv */
   onk_none,           /* tok_remove_cvref */
   onk_none,           /* tok_remove_extent */
   onk_none,           /* tok_remove_pointer */
   onk_none,           /* tok_remove_reference */
   onk_none,           /* tok_remove_reference_t */
   onk_none,           /* tok_remove_restrict */
   onk_none,           /* tok_remove_volatile */
   onk_none,           /* tok_is_trivially_equality_comparable */
   onk_none,           /* tok_nullptr_t */
   onk_none,           /* tok_is_trivially_relocatable */
   onk_none,           /* tok_is_bitwise_cloneable */
   onk_none,           /* tok_builtin_is_virtual_base_of */
   onk_none,           /* tok_builtin_is_implicit_lifetime */
   onk_none,           /* tok_builtin_lt_synthesizes_from_spaceship */
   onk_none,           /* tok_builtin_gt_synthesizes_from_spaceship */
   onk_none,           /* tok_builtin_le_synthesizes_from_spaceship */
   onk_none,           /* tok_builtin_ge_synthesizes_from_spaceship */
   onk_none,           /* tok_builtin_is_structural */
   onk_last            /* tok_last */
}
#endif /* VAR_INITIALIZERS */
EXTERN_CONSTINIT_ARRAY_END(opname_kind_for_token)

/*
Array giving the token name for each opname kind.
*/
EXTERN_THREAD a_const_char
		*opname_names[(int)onk_last];


/*
Structure used to build a list of files to be pre-included.
*/
typedef struct a_preinclude_file *a_preinclude_file_ptr;
typedef struct a_preinclude_file {
  a_preinclude_file_ptr
		next;	/* Pointer to the next entry in a linked list of
			   preinclude files. */
  a_const_char	*file_name;
			/* Name of the file to be preincluded. */
} a_preinclude_file;

/*
Structure used to record information about files that have been included.
See the comment preceding find_include_history in lexical.c.
*/
typedef struct an_include_file_history *an_include_file_history_ptr;
typedef struct an_include_file_history {
  a_const_char  *full_name;
			/* Pointer to the full path name of the include
			   file. */
  a_bit_field	suppress_subsequent_include:1;
			/* TRUE if this file is potentially one that can
			   have subsequent includes suppressed. */
  a_bit_field	pragma_once:1;
			/* TRUE if this file contained a "#pragma once"
			   directive. */
  a_bit_field	ifdef_guard:1;
			/* TRUE if this file was guarded by a #ifdef. */
  a_bit_field	ifndef_guard:1;
			/* TRUE if this file was guarded by a #ifndef. */
  a_bit_field	use_canonical_name:1;
			/* This is used to pass a flag into the
			   compare_include_file_history routine to indicate
			   whether the file name comparison should use the
			   canonical form of the file name. */
  a_const_char  *controlling_macro_name;
			/* The name of the macro used to guard the include
			   file against multiple inclusions. */
#if UNIQUE_FILE_IDENTIFIER_AVAILABLE
  a_unique_file_id
		unique_id;
			/* Host-dependent information that uniquely
			   identifies a file.  This is used when two different
			   names can refer to the same file (i.e., as a
			   result of symbolic or hard links to the file or
			   directories containing the file). */
#endif /* UNIQUE_FILE_IDENTIFIER_AVAILABLE */
} an_include_file_history;


/*
The order of these states is important.
*/
#define IFG_STATE_START		0
			/* The state when a file is first opened and before
			   any tokens have been scanned. */
#define IFG_STATE_ACCEPT	1
			/* We have seen the closing #endif of a top level
			   #ifdef or #ifndef.  There must be no additional
			   tokens in this file. */
#define IFG_STATE_FAIL		2
			/* This file is not a candidate for suppression of
			   subsequent includes. */
#define IFG_STATE_INTERMED	3
			/* We are in the process of scanning the tokens
		 	   inside a top level #ifdef or #ifndef. */
#define IFG_STATE_ONCE		4
			/* A #pragma once has been encountered in the file. */


extern a_boolean suppress_subsequent_include_of_file(
				 a_const_char                 *full_name,
				 an_include_file_history_ptr *ifhp_ptr,
				 a_boolean		     create,
			         a_boolean		     use_canonical);

extern
a_boolean find_include_history(a_const_char                *full_name,
			       an_include_file_history_ptr *ifhp_ptr,
			       a_boolean		   create,
			       a_boolean		   use_canonical);

extern a_byte get_ifg_state(void);
extern void set_ifg_state(a_byte new_state);

EXTERN_THREAD a_boolean
		any_tokens_fetched_from_curr_input_file;
			/* TRUE if any "real" tokens have been fetched from
			   the current input file.  This is used to
			   determine whether the current file is a candidate
			   to have subsequent inclusions suppressed.  This
			   variable is cleared at various times during the
			   scanning of the include file so great care must
			   be used before attempting to use this value for
			   some more general purpose. */


/*
Variables pertaining to the input stack (for include files and the
primary source file) and the current input file (the top entry on the
stack).
*/
typedef struct an_input_stack_entry *an_input_stack_entry_ptr;
typedef struct an_input_stack_entry {
  FILE		*file;
			/* The file at this level.  NULL if the file has
			   been closed and must be reopened (see position). */
  a_const_char	*file_name;
			/* The form of the file name to be used in error
			   messages and the like, null-terminated.
			   May have been modified by a #line directive. */
  a_const_char	*full_name;
			/* The form of the file name to be used in opening the
			   file (may have directory).  Null-terminated.
			   Can be the same as file_name. */
  a_const_char	*dir_name;
			/* The directory name of full_name.  Can be "". */
  a_directory_name_entry_ptr
		dir_entry;
			/* The directory name entry on the search path that
			   was used to find this file, or NULL if the
			   search path was not used (e.g., for an absolute
			   path name). */
  a_line_number	line_number;
			/* Physical line number of the line most recently
			   read from this file.  May have been modified by
			   a #line directive. */
  long		position;
			/* If the file was closed to preserve file blocks
			   (i.e., file == NULL), this is the position for
			   an fseek to re-establish the proper position. */
  a_source_file_ptr
		assoc_il_file,
		assoc_actual_il_file;
			/* The associated source file description in the
			   intermediate language.  The two pointers usually
			   point to the same entry, but when a #line directive
			   is processed assoc_il_file points to a new entry
			   for the #line information and assoc_actual_il_file
			   stays as it was (pointing to the entry for the
			   file actually being read). */
  long		base_pp_if_stack_depth;
			/* The value of pp_if_stack_depth (see preproc.c)
			   at entry to this file.  Needed because ANSI C
			   requires that each #if be closed in the same
			   file in which it began. */
  a_line_number actual_line;
			/* The physical line number of the line currently
			   being read or last read from this file.  This
			   field will not be modified by a #line directive. */
  a_line_number next_index_point;
			/* The next physical line number whose file position
			   should be recorded in file position index table
			   maintained for diagnostic generation. */
  a_bit_field	is_include_file:1;
			/* TRUE if this file was added to the input stack
			   as the result of a #include directive or a
			   --preinclude command-line directive.  FALSE
			   for all other cases including implicitly included
			   source files. */
  a_bit_field	from_system_include_dir:1;
			/* TRUE if this source file was found in an include
			   directory marked as a "system" include directory.
			   Warnings are suppressed when processing system
			   include directories. */
  a_bit_field	nested_inclusion:1;
			/* TRUE if this is a nested inclusion of a file
			   already on the input stack. */
  a_bit_field	saved_any_tokens_fetched:1;
			/* Used to save and restore the value of the global
			   variable any_tokens_fetched_from_curr_input_file. */
  a_bit_field	is_preinclude:1;
			/* TRUE if this is a preincluded file. */
  a_bit_field	do_not_advance_past_end_of_file:1;
			/* TRUE if when we reach the end of this file we
			   should stay there and not advance beyond it. */
#if ACCEPT_GNU_CARRIAGE_RETURN_LINE_TERMINATOR
  a_bit_field	prev_line_terminator_was_carriage_return:1;
			/* TRUE if the most recent line read from this file
			   ended with a carriage return.  This allows
			   treating a carriage return followed by a newline
			   as a single line terminator instead of the
			   newline being treated as the end of an empty
			   line. */
#endif /* ACCEPT_GNU_CARRIAGE_RETURN_LINE_TERMINATOR */
  a_bit_field	cloned_for_line_directive:1;
			/* TRUE if this entry was created to track #line
			   directives by cloning the previous top of the
			   input stack. */
  a_byte        ifg_state;
			/* Include file guard state information used to
                           determine whether subsequent inclusions of this
                           file may be suppressed. */
  an_include_file_history_ptr
		include_history;
                        /* Pointer to the structure that preserves information
                           used for include file guard processing. */
  a_unicode_source_kind
		unicode_source_kind;
			/* Indication of the kind of Unicode encoding (e.g.,
			   UTF-8, UTF-16) of this source file.  usk_none
			   except in versions with UNICODE_SOURCE_SUPPORTED
			   set to TRUE. */
} an_input_stack_entry;

/* See lexical.c for the definitions of input_stack, depth_input_stack,
   and seq_number_last_read. */
EXTERN_THREAD an_input_stack_entry_ptr
		curr_ise;
			/* Pointer to input_stack[depth_input_stack].  NULL
			   if depth_input_stack == -1. */

EXTERN_THREAD unsigned int
		include_file_depth;
			/* Number of include files (input_stack entries for
			   which is_include_file is TRUE) currently on the
			   input stack.  Used to support the GNU
			   __INCLUDE_LEVEL__ built-in macro. */

#if UNICODE_SOURCE_SUPPORTED
EXTERN_THREAD a_unicode_source_kind
		curr_file_unicode_source_kind;
			/* If not usk_none, indicates the kind of Unicode
			   source characters being read in the current source
			   file. */
#endif /* UNICODE_SOURCE_SUPPORTED */


#if FULLY_RESOLVED_MACRO_POSITIONS
/*
Data structures allowing offsets in a given buffer -- a macro definition,
argument, or expansion -- to be translated into the source positions from
which they originally came.
*/
typedef struct a_macro_text_map_entry *a_macro_text_map_entry_ptr;
typedef struct a_macro_text_map_entry {
  /* One of an array of entries that enables a given position in a macro
     definition string, a macro argument, or a macro expansion (source line
     modification) to be mapped into the corresponding source location.  Each
     entry has an offset from the beginning of the text buffer of the macro
     definition, argument, or expansion, and the source location to which it
     corresponds. */
  sizeof_t	start_of_region;
  a_simple_source_position
		corresponding_source_pos;
#if RECORD_MACRO_INVOCATIONS
  a_macro_invocation_record_index
		macro_context;
			/* Identifies the macro invocation to which this
			   entry belongs.  (Logically, this information is
			   part of the source line modification with which
			   this entry is associated.  However, in pcc and
			   Microsoft modes, all the source line modifications
			   for a given top-level macro invocation are
			   coalesced into a single source line modification,
			   so having the macro context here allows the parent
			   macro chain to be preserved in spite of the loss of
			   the source line modifications.) */
#endif /* RECORD_MACRO_INVOCATIONS */
} a_macro_text_map_entry;

typedef struct a_macro_text_map *a_macro_text_map_ptr;
typedef struct a_macro_text_map {
  /* A data structure that enables each offset in a macro definition string,
     a macro argument, or a macro expansion (source line modification) to be
     mapped to the corresponding source location.  This is done via a sorted
     extensible array of a_macro_text_map_entry objects; each entry specifies
     the corresponding source position for that offset in the text buffer of
     the macro definition, argument, or expansion and, by extension, for all
     offsets up to that of the next entry. */
  sizeof_t	max_entries;
			/* The number of a_macro_text_map_entry objects that
			   can be stored in the current allocation of the
			   entries array. */
  sizeof_t	num_entries;
			/* The number of a_macro_text_map_entry objects that
			   are currently in the entries array. */
  a_boolean	resizable;
			/* If TRUE, the entries array is allocated using
			   alloc_resizable_buffer and can be extended to
			   accommodate more entries.  Otherwise, the size is
			   fixed, and either entries is a pointer to a
			   subrange of the entries in another text map or the
			   array is allocated in front-end memory and thus
			   will be saved in precompiled headers. */
  a_macro_text_map_entry_ptr
		entries;
			/* Pointer to an array of a_macro_text_map_entry
			   objects.  These are kept in order of increasing
			   offset into the buffer with which this map is
			   associated. */
} a_macro_text_map;
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */

/*
Data structures that enable tracking of the location of a concatenation in
a macro expansion when check_concatenations is TRUE.  This information is
used to detect cases in which a concatenation did not result in a valid
token, as required by the language standards: if a new token begins at the
location occupied by the right-hand operand of ##, or if the combined token
ends before the right-hand token previously did, the concatenation did not
form a valid token.

A source line modification that incorporates text created by concatenation
has a singly-linked list of concatenation records in the order of their
offsets within the inserted text.  As tokenization proceeds through the
inserted text, concatenation records are removed from the list and placed
on a list of available records for potential reuse.
*/
typedef struct a_concatenation_record *a_concatenation_record_ptr;
typedef struct a_concatenation_record {
  a_concatenation_record_ptr
		next;	/* Pointer to the next entry in the list of records
			   for a source line modification or the list of
			   available records; NULL if this is the last
			   record on the list. */
  char		*line_loc;
			/* Pointer to the first character in a source line
			   modification's inserted text that comes from the
			   right-hand operand of the ## operator. */
  a_symbol_ptr	macro_sym;
			/* The macro in whose expansion the concatenation
			   occurs. */
} a_concatenation_record;

EXTERN_THREAD a_concatenation_record_ptr
		avail_concatenation_records;
			/* List of entries that have been allocated and
			   freed and are available for reuse. */

/*
Information relating to the components of a #embed directive or __has_embed
preprocessor operator.

The following structure is populated by parse_embed.  Its values are valid
only for the duration of the processing of the current #embed/__has_embed
construct.
*/
struct an_embed_parse_data {
  a_const_char	*file_name;
			/* The specified file name, allocated in the
			   file-scope IL memory. */
  a_const_char	*prefix_start;
			/* Points to the first character of the operand of
			   the prefix parameter; NULL if the parameter is
			   omitted. */
  a_const_char	*after_prefix;
			/* Points to the closing ')' of the prefix
			   parameter; NULL if the parameter is omitted. */
  a_const_char	*suffix_start;
			/* Points to the first character of the operand of
			   the suffix parameter; NULL if the parameter is
			   omitted. */
  a_const_char	*after_suffix;
			/* Points to the closing ')' of the suffix
			   parameter; NULL if the parameter is omitted. */
  a_const_char	*if_empty_start;
			/* Points to the first character of the operand of
			   the if_empty parameter; NULL if the parameter is
			   omitted. */
  a_const_char	*after_if_empty;
			/* Points to the closing ')' of the if_empty
			   parameter; NULL if the parameter is omitted. */
  a_host_large_unsigned
		limit;	/* The value of the operand of the limit parameter;
			   (a_host_large_unsigned)-1 if the parameter is
			   omitted. */
  a_host_large_unsigned
		offset;	/* The value of the gnu::offset or clang::offset
			   parameter; 0 if the parameter is omitted. */
#if PRESERVE_EMBED_DIRECTIVE_WHEN_OPTIMIZED
  a_text_buffer_ptr
		directive;
			/* A textual representation of the tokens of the
			   #embed directive. */
#endif /* PRESERVE_EMBED_DIRECTIVE_WHEN_OPTIMIZED */
  unsigned short
		num_prefix_elems;
			/* If prefix_is_value_list is TRUE, the number of
			   elements contained in the operand of the prefix
			   parameter, or 0 if the prefix parameter was
			   omitted or empty.  If prefix_is_value_list is
			   FALSE, the value is unspecified. */
  unsigned short
		num_suffix_elems;
			/* If suffix_is_value_list is TRUE, the number of
			   elements contained in the operand of the suffix
			   parameter, or 0 if the suffix parameter was
			   omitted or empty.  If suffix_is_value_list is
			   FALSE, the value is unspecified. */
  a_boolean	prefix_is_value_list;
			/* TRUE if the operand of the prefix parameter
			   consists solely of a list of comma-separated and
			   terminated identifiers (to allow for object-like
			   macros), integer literals, or character literals
			   or if the prefix parameter was omitted or empty;
			   FALSE otherwise. */
  a_boolean	suffix_is_value_list;
			/* TRUE if the operand of the suffix parameter
			   consists solely of a list of comma-separated
			   identifiers (to allow for object-like macros),
			   integer literals, or character literals,
			   introduced by a comma, or if the suffix
			   parameter was omitted or empty; FALSE
			   otherwise. */
};  /* an_embed_parse_data */

EXTERN_THREAD an_embed_parse_data
		embed_parse_data;
			/* Information regarding the #embed directive or
			   __has_embed operator currently being processed;
			   the value is unspecified outside those
			   contexts. */

extern void clear_embed_parse_data(void);

EXTERN_THREAD a_boolean
		next_token_is_start_of_braced_initializer;
			/* Set by parse_braced_init_list_full to cause a
			   #embed directive to return a special
			   tok_string_literal token if possible instead of
			   a token-by-token expansion. */

extern void copy_embed_data_to_il(a_type_ptr elem_type);

extern void process_embed_data_as_bytes(void);

/*
Variables pertaining to the current logical source line.  A "logical"
source line is what results after trigraph characters (see standard,
2.2.1.1) have been replaced, and lines ending in newline-backslash
have been spliced with the lines immediately following (see standard,
2.1.1.2).

Note that the source line and positions within it are supposed to be
hidden within the lexical routines.  Outside of these routines, all
positions are in a_source_position form (sequence number, column).
One exception is the pointers start_of_curr_token and end_of_curr_token,
which point to characters in curr_source_line.

On reading (by read_logical_source_line), curr_source_line contains the
characters of the logical source line, and orig_line_modif_list
contains information on the trigraphs and line splices sufficient to
map locations in curr_source_line back to source positions and to
exactly reconstruct the original lines.  These transformations are
done in curr_source_line because they have significance at the intra-token
level.

If any preprocessor transformations are done (comments deleted, macros
expanded), source_line_modif_list will be non-NULL and will contain
information about the transformations done.  The line in curr_source_line
is only logically modified by this information.  Physically, it is only
changed in that the first character of text being deleted is changed to 
ATTENTION_MARKER as a cue to indicate that the source_line_modif_list
should be consulted.  The text that is the expansion of a macro appears
in macro_buffer, and is pointed to by the source_line_modif entry.
These transformations have significance at the inter-token level; they
cannot occur in the middle of tokens.

A fundamental principle of input line scanning is that it must be possible
to represent the current source position completely in terms of a
single pointer (curr_char_loc), and it must be possible to set that
pointer to an arbitrary location in the source input and scan forward
using that pointer only.  Therefore, if there is some point in the
sequence of characters following curr_char_loc where input must be
diverted elsewhere, there must be a marker character at that point to
force examination of related data structures.
*/
EXTERN_THREAD a_const_char
		*curr_source_line;
			/* Characters of the current logical source line,
			   ended by LE_NEWLINE and LE_END_OF_LINE lexical
			   escape sequences.  Space is dynamically allocated,
			   and its upper bound is given by
			   after_end_of_curr_source_line.  See lexical_init
			   for the initial allocation.  When
			   after_end_of_all_source is TRUE, this array
			   contains just the LE_END_OF_LINE lexical escape
			   sequence (no newline, nothing else). */
#define CURR_SOURCE_LINE_INITIAL_ALLOCATION 3000
			/* Initial allocation size for curr_source_line.  The
			   initial allocation should be such that almost all
			   cases can be accepted (so that the realloc is
			   hardly ever needed) -- that means big enough for
			   all the lines of a large macro definition.
			   Subsequent reallocations will double the amount
			   previously allocated. */
EXTERN_THREAD a_const_char
		*after_end_of_curr_source_line;
			/* Address past the last element of curr_source_line,
			   as an aid to checking for overflow, etc.  A variable
			   because curr_source_line line can reallocated larger
			   if needed. */
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
EXTERN_THREAD a_const_char
		**logical_char_info;
			/* A dynamically allocated array that is used to
			   convert a pointer into curr_source_line from a
			   raw byte offset to a logical column number.  Each
			   multibyte character in the source line represents
			   one logical character.  When a non-initial character
			   of a token is encountered, the pointer to that
			   non-initial character is stored in the next
			   available entry in this array (the first such
			   pointer is in element zero, the second in element
			   one, etc.).  The array is allocated with the same
			   number of elements as curr_source_line to guarantee
			   that it is not possible to overflow the array.
			   To convert a pointer into curr_source_line into
			   a logical column number, you need to find the entry
			   in this array that contains the largest pointer that
			   is <= the pointer you are looking for.  The array
			   index+1 is the adjustment needed to convert to
			   a logical column number. */
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
EXTERN_THREAD int
		logical_char_info_entries_used;
			/* The number of entries in the logical_char_info
			   array that are in use for the current source
			   line (zero if no entries are in use).  Only
			   referenced when MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
			   is TRUE. */
EXTERN_THREAD a_seq_number
		curr_seq_number;
			/* The sequence number of the first physical line
			   in curr_source_line.  Once the end of file on
			   the primary source file has been passed, this
			   indicates a sequence number one past the highest
			   sequence number actually read, as an indication
			   of a sort of end-of-file line. */
EXTERN_THREAD a_boolean
		at_end_of_source_file;
			/* If TRUE, there is no current logical source line;
			   the last attempt to read one ran into an end of
			   file instead.  This may, however, be only the end
			   of an include file, and not of the entire source
			   sequence (see pop_input_stack).  The current
			   position is at the end of the current input file,
			   but still within it -- the stack has not yet been
			   popped.  curr_source_line still contains the line
			   most recently read. */
EXTERN_THREAD sizeof_t
		end_of_line_escape_offset;
			/* The offset of the LE_END_OF_LINE escape that
			   ends curr_source_line.  This is set by
			   skip_white_space and is used only in the rare
			   case when a line that logically begins with a
			   line_start_source_line_modif is the last line of
			   a file.  As noted in the preceding comment for
			   at_end_of_source_file, the line contents at that
			   point are still the last line before the EOF was
			   encountered, so this variable allows the
			   line_start_source_line_modif to be treated as if
			   it occurred just before the terminating
			   LE_END_OF_LINE instead of the beginning of the
			   line to avoid processing the line's contents
			   twice. */

EXTERN_THREAD a_seq_number
		seq_number_last_read;
			/* The sequence number of the physical line last read
			   from curr_input_stream.  This is often not the
			   same as the line number in the file.  Once the
			   end of file on the primary source file has been
			   passed, this indicates a sequence number one past
			   the highest sequence number actually read, as an
			   indication of a sort of end-of-file line. */
/*
Variable giving the current character position in the current
logical source line.  Usually, this points within curr_source_line,
but when macros are being scanned, it can point elsewhere (in macro_buffer
or in an argument string).
*/
EXTERN_THREAD a_const_char
		*curr_char_loc;
			/* Pointer to the current character within
			   curr_source_line.  More precisely, this is the
			   character about to be processed.  Points at the
			   LE_END_OF_LINE lexical escape sequence at the end
			   of the line when the line has been completely
			   scanned. */

/*
Escape characters used within curr_source_line and macro_buffer.
*/
/*
ATTENTION_MARKER is a one-character escape indicating the point at
which a source line modification occurs.  The source_line_modif_list
must be consulted for details of the modification.

ATTENTION_MARKER must be something that will not otherwise occur in the
source line, including in multibyte characters.  The newline character
is used.  The real newline at the end of input lines is replaced by a
two-character escape to free up a character that can be used as
a single-character escape.
*/
#define ATTENTION_MARKER '\n'
/*
Two-character escapes.  The first character is always LE_ESCAPE (a zero,
which is guaranteed not to occur otherwise in source lines).  Lint thinks
that a test for LE_ESCAPE followed by a check of the next character is
looking beyond the end of a null-terminated string.
*/
#define LE_ESCAPE 0 /*lint --e(448)*/
#define LE_ESCAPE_LEN 2	/* Length of escape sequence. */
/*
Second character of two-character escape is one of the following.
Note that zero is not used so that one can find LE_ESCAPE characters
without worrying that they are the second character of a two-character
escape.
*/
#define LE_END_OF_LINE 1
			/* End of curr_source_line. */
#define LE_NEWLINE 2
			/* Newline character (replaced with an escape so
			   that the newline character itself can be used
			   as ATTENTION_MARKER). */
#define LE_END_OF_INSERTION 3
			/* End of an insertion, e.g., the replacement text
			   inserted for a macro expansion. */
#define LE_END_OF_TOKEN 4
			/* End of a token.  Used as a token divider in
			   macro definitions and macro expansions, to
			   guarantee that the text will be tokenized the
			   same way as on the original macro definition. */
#define LE_INERT_MACRO 5
			/* Precedes an identifier that is the name of
			   a macro that appears within its own expansion,
			   and should therefore not be expanded.  Not used
			   in pcc preprocessing mode. */
#define LE_NULL 6
			/* In modes that allow a null (zero) character in
			   an input line (e.g., gcc mode), indicates such
			   a character. */
#define LE_COMMA_FROM_ARGUMENT 7
			/* In Microsoft mode, indicates that the following
			   comma token originally appeared at the top level
			   (i.e., not within parentheses) in the expanded
			   text of a macro argument.  (Used to prevent such
			   commas from delimiting macro arguments when the
			   expansion is rescanned.) */
#define LE_TEMPORARILY_INERT_MACRO 8
			/* Like LE_INERT_MACRO, except that it is removed
			   by expand_top_level_pcc_macro.  It is used to
			   support Microsoft-mode macros in which the left
			   parenthesis of a macro invocation appears in a
			   macro expansion in a macro argument and the
			   corresponding right parenthesis appears in the
			   text following the outermost macro invocation
			   (which violates the Standard requirement that
			   macro arguments are expanded in isolation).  A
			   macro with a "missing" right parenthesis is
			   marked as temporarily inert so it will only be
			   expanded after the expansion of the top-level
			   macro invocation is complete. */
#define LE_END_OF_TOP_LEVEL_EXPANSION 9
			/* Indicates the end of the expansion of a macro
			   whose invocation occurs on a source line, i.e.,
			   not in the expansion of another macro.  Used
			   only when FULLY_RESOLVED_MACRO_POSITIONS is
			   FALSE as part of an optimization to speed up
			   calculation of source positions. */
#define LE_RAW_OR_EXPANDED_ARGUMENT 10
			/* Indicates that the immediately following text
			   consists of a deletion source line modification
			   where the deleted text is the raw form of a
			   macro argument immediately followed by a
			   deletion source line modification where the
			   deleted text is the expanded form of that
			   argument.  This structure allows choosing the
			   correct form when needed to support an obscure
			   characteristic of the Microsoft preprocessor.
			   See choose_raw_or_expanded_arg for details. */
#define LE_MICROSOFT_MAGIC_COMMA 11
			/* Indicates that the immediately following
			   character, which must be a comma, is a comma
			   immediately preceding an empty __VARARGS__
			   expansion in Microsoft emulation mode.  While
			   ordinarily such a comma is deleted (see
			   adjust_length_for_magic_arg), the Microsoft
			   preprocessor preserves such commas if they
			   appear in a macro argument list.  This escape
			   allows skip_white_space to skip over the comma
			   normally but stop the white space scan on the
			   comma when processing macro arguments. */
#define LE_EMPTY_VARIADIC_MACRO 12
			/* Indicates the presence of an empty variadic
			   macro expansion.  This is used to support the
			   Microsoft feature that deletes a comma in a
			   macro argument list that precedes an empty
			   variadic macro expansion. */
#define LE_LPAREN_FROM_ARGUMENT 13
			/* Indicates that the following character, a left
			   parenthesis, was the first character of a macro
			   argument.  This is used in the analysis to
			   determine whether a comma in a macro argument
			   delimits macro arguments during rescanning of
			   the expansion. */
#define LE_END_OF_EMBED_PREFIX 14
			/* Terminates the prefix text of a #embed
			   directive.  Processing this escape results in
			   beginning to process the contents of the embed
			   file. */
#define LE_END_OF_EMBED 15
			/* Terminates the suffix or if_empty text of a
			   #embed directive.  Processing this escape
			   results in ending the expansion of the #embed
			   (moving to the next line) and freeing the buffer
			   in the embed control block. */

/*
Modifications made to the current source line.  orig_line_modif holds
information needed to restore curr_source_line to its original form
when read; source_line_modif is the information needed to transform
curr_source_line into its fully preprocessed form (with macros expanded,
etc.).
*/

enum an_orig_line_modif_kind {
  olm_trigraph,
  olm_line_splice,
  olm_multiline_string_splice,
  olm_null,		/* Null (zero) character in source line. */
  olm_splice_whitespace	/* A whitespace character following the '\' of a
			   line splice. */
};

typedef struct an_orig_line_modif {
  /* A modification (trigraph or line splice) that has been made to the
     original source lines to turn them into the current contents of
     curr_source_line. */
  an_orig_line_modif_ptr
		next;
			/* A pointer to the next entry on the
			   orig_line_modif_list, or NULL if there is no
			   next entry.  Also used to link entries on
			   the avail_orig_line_modifs list. */
  a_const_char	*line_loc;
			/* The location in curr_source_line of the
			   modification.  Points to the character that
			   resulted from the trigraph, or the character
			   following the line splice. */
  an_orig_line_modif_kind
		kind;
			/* Kind of modification: trigraph or line splice. */
  a_byte_boolean
		in_raw_string_literal;
			/* TRUE if this modification occurs within a raw
			   string literal, FALSE otherwise. */
  union {
    /* When kind == olm_null, no variant fields. */
    /* When kind == olm_trigraph or olm_splice_whitespace: */
    unsigned char
		orig_char;
			/* For olm_trigraph, the original third character
			   of the trigraph, for example "=" in "? ? ="
			   (extra space added so that won't actually be a
			   trigraph).  For olm_splice_whitespace, a
			   whitespace character following the '\' of a line
			   splice.  (Multiple whitespace characters are
			   represented by multiple modifications, one per
			   character in the order that they appear, so they
			   can be correctly reconstructed in raw string
			   literals.) */
    /* When kind == olm_line_splice or olm_multiline_string_splice: */
    a_seq_number
		line_splice_seq_number;
			/* The source sequence number of the line following
			   the line splice. */
  } variant;
} an_orig_line_modif;

EXTERN_THREAD an_orig_line_modif_ptr
		orig_line_modif_list,
		end_orig_line_modif_list;
			/* List of modifications to the original source lines
			   to produce the current logical source line.  Entries
			   are in order by position in curr_source_line.
			   end_orig_line_modif_list points to the last entry
			   on the list.  Both pointers are NULL if there are
			   no changes (a very common case). */
EXTERN_THREAD an_orig_line_modif_ptr
		avail_orig_line_modifs;
			/* List of entries that have been allocated and freed
			   and are available for reuse. */

typedef struct a_source_line_modif {
  /* A modification (text deletion or replacement) that must be made to
     the current contents of curr_source_line or macro_buffer to turn them
     into the fully preprocessed version of the line. */
  a_source_line_modif_ptr
		next;
			/* A pointer to the next entry on the
			   source_line_modif_list, or NULL if there is
			   no next entry.  Also used to link entries on the 
			   avail_source_line_modifs list. */
  a_source_line_modif_ptr
		next_in_hash_table;
			/* A pointer to the next entry in the same bucket
			   of the hash table of source line modifications. */
  a_const_char	*line_loc;
			/* The location in curr_source_line or macro_buffer
			   of the modification.  Points to an ATTENTION_MARKER
			   character that replaced the first character of
			   the text to be deleted.  NULL to indicate that
			   this entry is be inserted preceding the first
			   character of the source line (there can be only
			   one of those).  Also NULL for entries saved
			   because they contain macro argument text, when
			   the text is from a previous source line. */
  a_source_line_modif_ptr
		parent_modif;
			/* If line_loc points into the text inserted by
			   another modification, this points to that parent
			   modification entry.  Otherwise (i.e., if line_loc
			   points into the primary source line), NULL.
			   Meaningful only if parent_modif_determined is
			   TRUE. */
  sizeof_t	num_chars_to_delete;
			/* The number of characters to be logically deleted
			   (beginning at line_loc).  Greater than zero
			   (except that when line_loc == NULL, this is
			   zero). */
  sizeof_t	num_deleted_chars;
			/* The number of characters in inserted_text that
			   have been replaced by a nested source line
			   modification (less one for the ATTENTION_MARKER)
			   and thus would be reclaimed if the macro buffer
			   were compacted. */
  a_bit_field	is_isolated_text:1;
			/* TRUE if this modification's text isn't connected
			   to the surrounding context.  When the end of the
			   insertion is reached, end-of-source is returned.
			   This is used during macro expansion of macro
			   arguments to prevent scanning more tokens from
			   the remainder of the source file, and also for
			   buffers that are temporary. */
  a_bit_field	is_for_comment:1;
			/* TRUE if this modification is due to a comment
			   in the source (as opposed to a macro expansion). */
  a_bit_field	parent_modif_determined:1;
			/* TRUE if a value has been determined for
			   parent_modif. */
  a_bit_field	being_rescanned_for_token_pasting:1;
			/* TRUE if this modification is for a macro expansion
			   that is currently being rescanned in order to
			   do old-style token pasting. */
  a_bit_field	locked:1;
			/* TRUE if this modification should not be moved (for
			   the purpose of optimizing searches).  Used when
			   this entry is used as a marker to delimit a prefix
			   part of the list of source line modifications. */
  a_bit_field	contains_saved_macro_argument_text:1;
			/* TRUE if this modification contains some text saved
			   for a macro argument, and therefore should not be
			   freed automatically when the next source line is
			   read. */
  a_bit_field	is_whitespace_kwd:1;
			/* TRUE if this modification represents the canonical
			   form of a whitespace keyword or is a line-start
			   modification that reinserts the first token of
			   a failed multi-line whitespace keyword, i.e., a
			   token that might have begun a whitespace keyword
			   but the token that followed it (on a succeeding
			   line) did not complete the keyword. */
  a_bit_field	is_raw_or_expanded_arg:1;
			/* TRUE if this modification represents either the
			   raw or expanded version of an argument where the
			   choice was deferred pending expansion of a
			   nested macro invocation.  See the comments on
			   choose_raw_or_expanded_arg in macro.c for
			   details. */
  a_bit_field	is_concat_with_macro_argument:1;
			/* TRUE if the inserted text of this modification
			   includes the result of the concatenation
			   operator ## applied to a macro argument.  This
			   is needed to emulate the behavior of the
			   traditional Microsoft preprocessor regarding
			   commas appearing in __VA_ARGS__ text.  See the
			   comments in macro_invocation describing the
			   handling of comma_is_from_argument for
			   details. */
  a_bit_field	is_concat_with_va_args:1;
			/* TRUE if the inserted text of this modification
			   includes the result of applying the
			   concatenation operator ## to __VA_ARGS__.  This
			   is needed to emulate the behavior of the
			   traditional Microsoft preprocessor regarding
			   commas appearing in __VA_ARGS__ text.  See the
			   comments in macro_invocation describing the
			   handling of comma_is_from_argument for
			   details. */
  a_bit_field	has_lparen_from_arg:1;
			/* TRUE if this modification resulted from a
			   function-like macro in which the left
			   parenthesis was the first token of a macro
			   argument.  This is used to assist in the
			   determination of whether a comma appearing in a
			   macro argument should delimit macro arguments
			   when the expansion is rescanned. */
  unsigned char	orig_char;
			/* The character that was in the source line at
			   position line_loc (provided so that the original
			   line can be reconstructed; not needed
			   otherwise).  Meaningless when line_loc ==
			   NULL. */
  char		inserted_chars[3];
			/* Place for an insert string of up to three characters
			   including the end-of-insertion lexical escape.
			   Used for the blank that replaces comments. */
  a_const_char	*inserted_text;
			/* Pointer to text to be inserted, in macro_buffer.
			   The text is terminated by an LE_END_OF_INSERTION
			   lexical escape.  Points to a zero-length string
			   if this is a deletion only. */
  a_const_char	*end_inserted_text;
			/* Pointer to the LE_END_OF_INSERTION lexical escape
			   at the end of inserted_text. */
  a_symbol_ptr	assoc_macro;
			/* The macro that generated this expansion.  This
			   is important in that the macro name is protected
			   from expansion within its own expansion.  If
			   this is NULL, the expansion did not come from
			   a macro (it might have come from deletion of
			   a comment or insertion of the text of a macro
			   argument so macro expansion can be done on it). */
  unsigned long	sequence_id;
			/* An integer indicating the sequence in which 
			   modifications were added to the source line
			   (higher values were added later).  Generated
			   from the value of sequence_id_for_source_line_modifs
			   when this entry was created. */
  a_source_position
		source_position;
			/* When source_position.seq != 0, this indicates
			   the source position associated with this
			   modification.  This is particularly useful when
			   the modification is for a multi-line macro call. */
  a_const_char	*text_from_primary_source_line;
			/* If non-NULL, text from the location pointed to
			   to the end-of-insertion marker is from the primary
			   source line, placed in this modification so that
			   it can be token-pasted with the end of a macro
			   expansion in pcc mode. */
#if FULLY_RESOLVED_MACRO_POSITIONS
  a_macro_text_map
		text_map;
			/* A map of offsets within inserted_text to the
			   original positions (macro definition and arguments)
			   from which they came.  (This field is unused for
			   modifications not resulting from macro expansions.)
			   Note that this text map is not "standalone" -- the
			   num_entries and entries fields denote a subset of
			   the macro_text_map defined in macro.c, so this
			   array is not extensible. */
  int		num_active_position_trackers;
			/* The number of a_text_map_position_trackers that are
			   currently referring to this source line modif (see
			   macro.c).  If the inserted text is compacted when
			   the macro_buffer is reallocated, any position
			   trackers referring to this source line modification
			   may need to be updated accordingly. */
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
#if RECORD_MACRO_INVOCATIONS
  a_macro_invocation_record_index
		invocation_record;
			/* The invocation record for the macro invocation
			   that resulted in this modification, or
			   NO_PARENT_MACRO_INVOCATION if this modification is
			   not the result of a macro expansion. */
  a_macro_invocation_record_index
		invocation_depth;
			/* The depth in the invocation stack of this
			   invocation (0 for modifications that are not the
			   result of a macro expansion). */
#endif /* RECORD_MACRO_INVOCATIONS */
  a_concatenation_record_ptr
		concatenations;
			/* Head of a singly-linked list of concatenation
			   records that apply to this modification's
			   inserted text; NULL if there are none (including
			   the case where check_concatenations is
			   FALSE). */
} a_source_line_modif;

EXTERN_THREAD a_source_line_modif_ptr
		source_line_modif_list;
			/* List of modifications to the current logical source
			   line to produce the fully preprocessed line.
			   Modifications are in no particular order, so
			   searching for something usually requires a linear
			   search from the start of the list.  However,
			   entries are moved to the front of the list when
			   they are referenced, so the searches are short.
			   NULL if there are no changes (a very common
			   case). */
EXTERN_THREAD a_source_line_modif_ptr
		line_start_source_line_modif;
			/* Ordinarily NULL.  When non-NULL, points to
			   a source modification entry with line_loc == NULL,
			   which is an entry to be inserted before the first
			   character of curr_source_line (there is at most
			   one of these, and that only in rare cases).
			   The entry is also on the source_line_modif_list;
			   this pointer exists only to allow rapid
			   determination of whether or not there is such
			   an entry.  Used for re-insertion of macro name
			   identifiers for function-like macros when the
			   identifier is not followed by a "(". */
EXTERN_THREAD a_source_line_modif_ptr
		avail_source_line_modifs;
			/* List of entries that have been allocated and freed
			   and are available for reuse. */
EXTERN_THREAD unsigned long
		sequence_id_for_source_line_modifs;
			/* Used to generate sequence_id values as 
			   modifications are added; incremented each
			   time used.  This is useful so that all entries
			   generated after a certain "time" can be
			   easily found. */

/*
This flag is ordinarily NULL.  It is set non-NULL when a section of a line
is to be deleted because of preprocessing.  Specifically, the part of the
line from delete_source_from_loc to just before curr_char_loc is to
be deleted.  This flag is useful when scanning may cause reading of a new
logical source line.  Having the flag ensures that the deletion will be
done as the newline is processed, after which delete_source_from_loc
will be set to the start of the new line.
*/
EXTERN_THREAD a_const_char
		*delete_source_from_loc;

/*
Variables pertaining to the current token:
(These are the main interface between the lexical routines and the
rest of the compiler.)
See also the related variables in symbol_tbl.h.
If new variables are added here, be sure to put them also into
cache_curr_token, get_token_from_cached_token_rescan_stack, and
get_token_from_reusable_cache_stack.
*/
EXTERN_THREAD a_token_kind
		curr_token;
			/* The current token of input.  More precisely,
			   the one about to be processed. */
EXTERN_THREAD a_const_char
		*start_of_curr_token,
		*end_of_curr_token;
			/* The characters of the current token, in
			   curr_source_line.  Only valid outside the
			   lexical routines when raw pp tokens are being
			   fetched. */
EXTERN_THREAD sizeof_t
		len_of_curr_token;
			/* The length of the current token.  Only valid 
			   outside the lexical routines when raw pp tokens
			   are being fetched. */
EXTERN_THREAD a_source_position
		pos_curr_token;
			/* The start position of the current token. */

#if EXTRA_SOURCE_POSITIONS_IN_IL
EXTERN_THREAD a_source_position
		end_pos_curr_token;
			/* The end position of the current token -- that is,
			   the source position of the last character of the
			   token. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

EXTERN_THREAD int
		kind_of_white_space_skipped;
			/* Kind of white space skipped by the most recent
			   call to skip_white_space (not necessarily
			   correct when get_token is called).  Used in
			   scanning macro definitions, where white space
			   can be significant.  0 = none, others as bits
			   as follows: */
#define WHITE_SPACE_COMMENTS 0x01
#define WHITE_SPACE_OTHER    0x02

EXTERN_THREAD a_boolean
		preserve_white_space_kind;
			/* When TRUE, skip_white_space will not reset
			   kind_of_white_space_skipped to 0, and the caller
			   of skip_white_space will be responsible for
			   resetting it to 0.  This allows
			   expand_top_level_pcc_macro to be aware of white
			   space skipped during a single call to get_token,
			   which can occur when the next preprocessing
			   token is a macro invocation whose expansion is
			   empty, followed by a non-empty macro
			   expansion. */

EXTERN_THREAD a_boolean
		comma_is_from_argument;
			/* Set to TRUE when an LE_COMMA_FROM_ARGUMENT is
			   seen by skip_white_space.  It is the responsibility
			   of the caller of skip_white_space to set it to
			   FALSE beforehand. */
EXTERN_THREAD a_boolean
		comma_is_magic;
			/* Set to TRUE when skip_white_space encounters an
			   LE_MICROSOFT_MAGIC_COMMA when in_macro_arg_list
			   is TRUE and the following comma is thus not
			   skipped.  It is the responsibility of the caller
			   of skip_white_space to set it to FALSE
			   beforehand. */
EXTERN_THREAD a_boolean
		empty_variadic_macro_seen;
			/* Set to TRUE when skip_white_space encounters an
			   LE_EMPTY_VARIADIC_MACRO.  It is the
			   responsibility of the caller of skip_white_space
			   to set it to FALSE beforehand. */
EXTERN_THREAD a_boolean
		lparen_is_from_argument;
			/* Set to TRUE when an LE_LPAREN_FROM_ARGUMENT is
			   seen by skip_white_space.  It is the
			   responsibility of the caller of skip_white_space
			   to set it to FALSE beforehand. */
EXTERN_THREAD a_source_line_modif_ptr
		last_source_line_modif_exited_while_skipping_white_space;
			/* Set by skip_white_space whenever a source line
			   modification is exited. */
EXTERN_THREAD a_constant
		const_for_curr_token;
			/* If the current token is a literal constant,
			   this is its value. */
EXTERN_THREAD a_constant
		const_with_curr_tok_spelling;
			/* If the current token is a numeric user-defined
			   literal and tokens are either being cached or
			   extracted from a cache, this is a ck_string
			   containing the spelling of the token, excluding
			   the literal suffix.  For example, if the current
			   token is 123_x, this constant would be the
			   string "123". */
EXTERN_THREAD a_symbol_ptr
		ud_lit_op_sym_for_curr_token;
			/* If the current token is a user-defined literal
			   (tok_ud_literal), this is the symbol for its
			   literal operator or literal operator template,
			   if known.  May be NULL or designate an
			   overloaded function in error cases or if the
			   user-defined literal is a string literal
			   appearing in the declaration of a literal
			   operator or literal operator template;
			   otherwise, it designates the specific function
			   or template to be used to produce the value of
			   this literal. */
EXTERN_THREAD a_type_ptr
		ud_lit_type_for_curr_token;
			/* If the current token is a user-defined literal
			   (tok_ud_literal), this is the type passed to
			   find_literal_operator to look up the associated
			   literal operator or literal operator
			   template. */
EXTERN_THREAD a_lexical_ifc_index_reference
		ifc_index_for_curr_token;
			/* If the current token is a pseudotoken indicating an
			   index into an IFC module (i.e., extra_info_kind is
			   teik_ifc_index), this is the associated index. */
EXTERN_THREAD an_error_code
		err_code_for_error_token;
			/* If the current token is tok_error, this is the
			   error code that applies to the error.  This is
			   useful if the token was fetched with fetch_pp_tokens
			   TRUE, since no diagnostic was put out in that
			   case. */
EXTERN_THREAD a_boolean
		curr_token_is_inert_macro;
			/* TRUE if the current token is an identifier that
			   is the name of a macro that was found within its
			   own expansion and therefore should not be expanded
			   again.  Not used in pcc preprocessing mode. */
EXTERN_THREAD a_boolean
		curr_token_is_temporarily_inert_macro;
			/* TRUE when curr_token_is_inert_macro is TRUE and
			   the lexical escape indicating inertness is
			   LE_TEMPORARILY_INERT_MACRO instead of
			   LE_INERT_MACRO. */

EXTERN_THREAD a_boolean
		any_initial_get_token_tests_needed;
			/* TRUE if a condition exists that requires some
			   special processing when get_token is called.
			   This is set when tokens are being rescanned from
			   a cache, when there are pragmas that are
			   associated with the current token, or when
			   tokens are being taken from a #embed file or
			   prefix. */

EXTERN_THREAD a_boolean
		treat_newline_as_token;
			/* TRUE if ends-of-lines should be returned as
			   tok_newline instead of skipped as white space. */

EXTERN_THREAD char
		*curr_token_asm_string;
			/* When curr_token == tok_microsoft_asm, this points
			   to the associated asm string. */

EXTERN_THREAD a_constant_ptr
		name_linkage_constants;
			/* A pointer to an array of constants representing
			   the name linkages that the front end accepts. */

EXTERN_THREAD a_boolean
		id_contains_ucn_or_multibyte_char;
			/* When curr_token is tok_identifier, TRUE if the
			   the identifier was written using a
			   universal-character-name or a multibyte
			   character, FALSE otherwise.  This is only set
			   upon the initial scan of an identifier from
			   source text, not, e.g., when the identifier
			   comes from a token cache, so it should only be
			   checked in contexts such as preprocessor
			   directives where it is known that source text is
			   being scanned. */

EXTERN_THREAD a_boolean
		number_contains_digit_separator;
			/* Set to TRUE by scan_number if the current token
			   has embedded digit separators (apostrophes) and
			   FALSE otherwise.  Valid only until the next call
			   to get_token. */

#if ASM_SUPPORT_NEEDED

EXTERN_THREAD a_boolean
		in_asm_function_body;
			/* TRUE if processing takes place during the scan of
			   an asm function body. */

EXTERN_THREAD sizeof_t
		pos_in_asm_func_body_buffer;
			/* The number of characters that have been added to
			   asm_func_body_buffer thus far in processing. */

EXTERN_THREAD char
		*asm_func_body_buffer;
			/* Pointer to a dynamically allocated buffer used to
			   construct the string representation of an asm
			   function or Microsoft asm block. */

EXTERN_THREAD sizeof_t
		size_asm_func_body_buffer;
			/* The size of the asm buffer. */

EXTERN_THREAD a_const_char
		*prev_asm_stop_char;
			/* The last character copied by the previous call to
			   copy_from_source_to_asm_func_buffer.  (Must be
			   EXTERN so it can be relocated if macro_buffer is
			   reallocated.) */


#if ASM_FUNCTION_ALLOWED
extern void copy_from_source_to_asm_func_buffer(
                                        a_const_char *stop_char,
                                        a_const_char *after_comment_stop_char);

extern void reset_asm_buffer(void);
#endif /* ASM_FUNCTION_ALLOWED */
#endif /* ASM_SUPPORT_NEEDED */

#if EXTRA_SOURCE_POSITIONS_IN_IL
EXTERN_THREAD a_source_position
		curr_construct_end_position;
			/* Global variable used to return an end position
			   from the scanning of some construct.  Should be
			   retrieved and saved quickly, since it is set
			   by many scanning routines. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

#if !FULLY_RESOLVED_MACRO_POSITIONS
EXTERN_THREAD a_source_position
		pos_of_macro_invocation;
			/* Global variable that contains the source
			   position of the outermost macro invocation
			   within whose expansion the current source
			   position occurs, if any, and
			   null_source_position otherwise.  Used to speed
			   up conversion of line locations to source
			   positions.  Not used with
			   FULLY_RESOLVED_MACRO_POSITIONS because original
			   positions must be calculated for each
			   position. */
#endif /* !FULLY_RESOLVED_MACRO_POSITIONS */

/*
The stop token array: If a syntactic error occurs, flush_tokens
will be called.  It will throw away tokens until it finds one for which
curr_stop_token_stack_entry->stop_tokens[curr_token] != 0 (i.e., a non-zero
entry means the corresponding token is in the set of stop tokens).
The entries are actually counts; each time something is added to the stop
set, an entry is incremented, and each time something is removed, the entry
is decremented.  The entries are unsigned so that overflow need not be
checked for; in boundary cases, like an error in an expression with
256 levels of parentheses, one might flush a little further than usual,
but this is not a meaningful problem.
*/
typedef unsigned char
		a_token_set_array_element;
typedef a_token_set_array_element
		a_token_set_array[(int)tok_last+1];
			/* Generic array-of-unsigned-char both for the global
			   stop token array and for local arrays used in
			   token caching. */

/*
A stack of stop token arrays is maintained.  The top of the stack is
pointed to by curr_stop_token_stack_entry.  A new stop token stack
entry is pushed when a new lexical context is entered (for example,
when a template instantiation is done) and popped when that context
is no longer needed and the previous context must be restored.
*/
typedef struct a_stop_token_stack_entry *a_stop_token_stack_entry_ptr;
typedef struct a_stop_token_stack_entry {
  a_stop_token_stack_entry_ptr
		next;
			/* Pointer to the previous stack entry (e.g., the
			   entry that should become the current entry when
			   this one is popped off of the stack). */
  a_token_set_array
		stop_tokens;
			/* The set of tokens that will terminate a flush on
			   syntactic error.  A given token is in the set if
			   stop_tokens[token] != 0; */
} a_stop_token_stack_entry;

EXTERN_THREAD a_stop_token_stack_entry_ptr
		curr_stop_token_stack_entry;
			/* Pointer to the current stop token stack entry. */

/*
A stack of lexical state arrays is maintained.  The top of the stack is
pointed to by curr_lexical_state_stack_entry.  A new lexical state stack
entry is pushed when a new lexical context is entered (for example,
when a template instantiation is done) and popped when that context
is no longer needed and the previous context must be restored.
*/
typedef struct a_lexical_state_stack_entry *a_lexical_state_stack_entry_ptr;
typedef struct a_lexical_state_stack_entry {
  a_lexical_state_stack_entry_ptr
		next;
			/* Pointer to the previous stack entry (e.g., the
			   entry that should become the current entry when
			   this one is popped off of the stack). */
  int		cache_tokens;
			/* Non-zero if tokens fetched by get_token should also
			   be cached.  This is incremented by each caller that
			   requests token caching. */
  a_token_sequence_number
		last_tsn_in_cache;
			/* The token sequence number of the last token added
			   to the cache, or NO_TOKEN_SEQUENCE_NUMBER if the
			   cache is empty. */
  a_source_position
		error_position;
			/* The saved value of error_position when the state
			   stack was pushed. */
  Opt<a_scanning_token_cache>
		cache;
			/* The cache used to save tokens when cache_tokens is
			   non-zero. */
  a_bit_field	next_token_is_top_level_decl_start:1;
			/* The saved value of
			   next_token_is_top_level_decl_start when the state
			   stack was pushed. */
  a_bit_field	caching_tokens:1;
			/* The saved value of caching_tokens when the state
			   stack was pushed. */
  a_bit_field	suspend_caching_tokens:1;
			/* TRUE if, when we are caching tokens, we should
			   temporarily suspend that caching. */
  a_bit_field	flushing_tokens:1;
			/* TRUE if we are flushing tokens that are being
			   ignored. */
} a_lexical_state_stack_entry;

EXTERN_THREAD a_lexical_state_stack_entry_ptr
		curr_lexical_state_stack_entry;
			/* Pointer to the current lexical state stack entry. */

inline a_token_cache_ptr curr_lexical_state_cache()
/*
Return a pointer to the token cache associated with the current lexical
state stack entry.
*/
{
  /* This function assumes that the caller knows a cache is present. */
  check_assertion(curr_lexical_state_stack_entry->cache.has_value());
  return curr_lexical_state_stack_entry->cache->ptr();
}  /* curr_lexical_state_cache */

/*
Other general variables:
*/
EXTERN_THREAD a_boolean
		is_id_char[CHAR_MAX-CHAR_MIN+1];
			/* For each character, whether or not it can be a
			   character after the first in an identifier.
			   Also used in scanning pp-numbers.  Note that
			   this covers only the identifier characters that
			   can be expressed in a single character; with
			   encodings like UTF-8, there may be other
			   identifier characters that take more than one
			   character.  If you need the full set of identifier
			   characters, see is_identifier_char. */
#if UNICODE_SOURCE_SUPPORTED
EXTERN_THREAD a_boolean
		is_id_char_no_mbc[UCHAR_MAX+1];
			/* For each character, whether or not it can be a
			   character after the first in an identifier.
			   This is used to check characters after
			   multibyte characters have been converted to a
			   single character value.  For example, in UTF-8
			   the accented European characters of Latin-1
			   can be checked in this table once they have been
			   converted to a single-byte Unicode code point. */
#endif /* UNICODE_SOURCE_SUPPORTED */
EXTERN_THREAD a_boolean
		char_ends_id[CHAR_MAX-CHAR_MIN+1];
			/* A table to quickly identify characters that end
			   an identifier (to avoid a relatively expensive
			   call to f_is_identifier_char). */
EXTERN_THREAD a_boolean
		char_ends_number[CHAR_MAX-CHAR_MIN+1];
			/* A table to quickly identify characters that end
			   a numeric literal (used to check for the
			   single-digit integer performance
			   optimization). */
EXTERN_THREAD a_boolean
		is_raw_string_delimiter_char[CHAR_MAX-CHAR_MIN+1];
			/* For each character, whether or not it can appear
			   in the d-char-sequence of a raw string
			   literal. */

using a_token_stack = Dyn_array<a_shared_token>;
			/* The type used for a stack of tokens. */

/*
Structure used to represent a reusable token cache that is either allocated
into persistent front end memory (i.e., will not be freed until the front end
wraps up) or shared (i.e., with a lifetime managed via an instance of
a_shared_token_cache).
*/
struct a_reusable_token_cache {
  INLINE a_reusable_token_cache()
    : a_reusable_token_cache(NULL)
    {}
  INLINE explicit a_reusable_token_cache(a_token_cache *persistent_cache)
    : shared_token_cache(FALSE)
    { this->variant.persistent = persistent_cache; }
  INLINE explicit a_reusable_token_cache(
                                      const a_shared_token_cache &shared_cache)
    : shared_token_cache(TRUE)
    { new (&this->variant.shared) a_shared_token_cache(shared_cache); }
  INLINE explicit a_reusable_token_cache(a_shared_token_cache &&shared_cache);
  INLINE a_reusable_token_cache(const a_reusable_token_cache &other);
  INLINE ~a_reusable_token_cache();

  INLINE a_token_cache& operator*() const
    { check_assertion(this->ptr() != NULL); return *this->ptr(); }
  INLINE a_token_cache* operator->() const
    { check_assertion(this->ptr() != NULL); return this->ptr(); }

  INLINE a_token_cache* ptr() const;

  INLINE a_boolean is_empty() const
    { return this->ptr() == NULL || (*this)->is_empty(); }

  INLINE a_reusable_token_cache &operator=(a_token_cache *persistent_cache)
    { return (*this) = a_reusable_token_cache(persistent_cache); }
  INLINE a_reusable_token_cache &operator=(
                                     const a_shared_token_cache &shared_cache)
    { return (*this) = a_reusable_token_cache(shared_cache); }
  INLINE a_reusable_token_cache &operator=(
                                          const a_reusable_token_cache &other);
  INLINE a_reusable_token_cache &operator=(a_reusable_token_cache &&other);
private:
  a_byte_boolean
		shared_token_cache;
			/* TRUE if variant should use the shared_token_cache;
			   otherwise, FALSE. */
#ifdef UNION_AS_STRUCT
/* Workaround for union-as-struct build issue. */
#undef union
#endif /* ifdef UNION_AS_STRUCT */
  union a_token_cache_union {
    inline a_token_cache_union()
      {}
    inline ~a_token_cache_union()
      {}
    /* When shared_token_cache is FALSE: */
    a_token_cache
		*persistent;
			/* The persistent backing token cache. */
    /* When shared_token_cache is TRUE: */
    a_shared_token_cache
		shared; /* The shared backing token cache. */
  } variant;
#ifdef UNION_AS_STRUCT
#define union struct
#endif /* ifdef UNION_AS_STRUCT */
};  /* a_reusable_token_cache */


a_reusable_token_cache::a_reusable_token_cache(
                                           a_shared_token_cache &&shared_cache)
/*
Move-construct a new reusable token cache object from the given shared token
cache.
*/
  : shared_token_cache(TRUE)
{
  new (&this->variant.shared) a_shared_token_cache(move_from(&shared_cache));
}  /* a_reusable_token_cache::a_reusable_token_cache */


a_reusable_token_cache::a_reusable_token_cache(
                                           const a_reusable_token_cache &other)
/*
Copy-construct the reusable token cache.
*/
  : shared_token_cache(other.shared_token_cache)
{
  if (this->shared_token_cache) {
    new (&this->variant.shared) a_shared_token_cache(other.variant.shared);
  } else {
    this->variant.persistent = other.variant.persistent;
  }  /* if */
}  /* a_reusable_token_cache::a_reusable_token_cache */


a_reusable_token_cache::~a_reusable_token_cache()
/*
Destroy the reusable token cache.
*/
{
  if (this->shared_token_cache) {
    this->variant.shared.~Shared_obj<a_token_cache>();
  }  /* if */
}  /* a_reusable_token_cache::~a_reusable_token_cache */


a_token_cache* a_reusable_token_cache::ptr() const
/*
Return a pointer to the underlying token cache object (if any).
*/
{
  a_token_cache *result;

  if (this->shared_token_cache) {
BEGIN_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED
    result = this->variant.shared.ptr();
END_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED
  } else {
    result = this->variant.persistent;
  }  /* if */
  return result;
}  /* a_reusable_token_cache::ptr */


a_reusable_token_cache &a_reusable_token_cache::operator=(
                                           const a_reusable_token_cache &other)
/*
Copy-assign from the given reusable token cache.
*/
{
  if (this != &other) {
    destroy(this);
    construct(this, other);
  }  /* if */
  return *this;
}  /* a_reusable_token_cache::operator= */


a_reusable_token_cache &a_reusable_token_cache::operator=(
                                                a_reusable_token_cache &&other)
/*
Move-assign from the given reusable token cache.
*/
{
  if (this != &other) {
    destroy(this);
    construct(this, move_from(&other));
  }  /* if */
  return *this;
}  /* a_reusable_token_cache::operator= */


/*
The token cache stack is used to manipulate persistent token caches
used for template instantiations.
*/
typedef struct a_reusable_cache_entry *a_reusable_cache_entry_ptr;
typedef struct a_reusable_cache_entry {
  a_boolean has_next_token()
    { return this->next_token != a_token_cache_iterator(); }
  a_reusable_cache_entry_ptr
		next;
			/* Pointer to the next entry on	the stack. */
  a_token_stack *previous_token_rescan_stack;
			/* Contains a pointer to the token rescan stack	at
			   the time the	new persistent cache was pushed
			   onto	the stack. */
  a_token_cache_iterator
		next_token;
			/* The current iterator position in the token cache. */
  a_reusable_token_cache
		token_cache;
			/* The token cache from which the token list was
			   obtained. */
  uint32_t
		dependent_scans;
			/* Non-zero if calls to get_token that would walk off
			   the end of this cache should instead return an end
			   of source token.  This is used in cases where the
			   cache may otherwise be inadvertently popped during
			   error recovery. */
} a_reusable_cache_entry;


/*
Bit vector used to pass flags into the cache token stream routines.
*/
typedef unsigned int a_cts_flag_set;
#define CTS_NO_OPTIONS            0x0
#define CTS_COALESCE_IDS          0x1
			/* TRUE if identifiers should be coalesced during
			   the caching process. */
#define CTS_STOP_ON_STATEMENT_END 0x2
			/* TRUE if the caching should stop if a semicolon
			   or mismatched right brace is encountered.  This
			   is used to avoid excessive caching in programs
			   with certain kinds of syntax errors. */
#define CTS_IS_EXPRESSION         0X4
			/* TRUE if the stream being cached represents an
			   expression.  Used to determine the setting of
			   GID_IS_EXPR_CONTEXT and GID_IS_TYPENAME while
			   coalescing identifiers. */


inline a_token_sequence_number assign_new_token_sequence_number()
/*
A new token is being created (either from the input stream, or from a token
cache request).  Assign a token sequence number to this token.  The value is
incremented by two to reserve a slot in case the token is a ">>" that needs to
be split into two tokens because it closes two template argument lists.  This
availability is also taken advantage of when associating token sequence numbers
with calls to "begin(...)" and "end(...)" in some cases of range-based for
loops. The new token sequence number is returned.
*/
{
  last_token_sequence_number_used += 2;
  return last_token_sequence_number_used;
}  /* assign_new_token_sequence_number */


inline void assign_curr_token_sequence_number()
/*
A new token is being created (e.g., being scanned from the input stream).
Assign a token sequence number to this token, and update the current token
sequence number information.
*/
{
  curr_token_sequence_number = assign_new_token_sequence_number();
  last_token_sequence_number_of_token = curr_token_sequence_number;
}  /* assign_curr_token_sequence_number */


/* Save an end-of-source token in the token cache. */
extern void terminate_token_cache(a_token_cache *cache);
/* Create a token cache entry for a given token kind. */
extern a_shared_token build_cached_token(
                                       a_token_kind            kind,
                                       a_token_sequence_number sequence_number,
                                       const a_source_position *position);


a_removed_expr_descr::a_removed_expr_descr()
  : cache(NULL)
/*
Construct a removed expression descriptor.
*/
{
}  /* a_removed_expr_descr::a_removed_expr_descr */


a_removed_expr_descr::~a_removed_expr_descr()
/*
Destroy the current removed expression descriptor.
*/
{
  delete_fe(&this->cache);
}  /* a_removed_expr_descr::~a_removed_expr_descr */


a_ud_literal_descr::a_ud_literal_descr(const a_ud_literal_descr &other)
/*
Copy-construct a new user-defined literal descriptor from the given
user-defined literal descriptor.
*/
  : value_con(alloc_cached_constant()), spelling_con(alloc_cached_constant()),
    op_sym(other.op_sym), suffix(other.suffix), type(other.type)
{
  copy_constant(other.value_con, this->value_con);
  copy_constant(other.spelling_con, this->spelling_con);
}  /* a_ud_literal_descr::a_ud_literal_descr */


a_ud_literal_descr::a_ud_literal_descr(a_ud_literal_descr &&other)
/*
Move-construct a new user-defined literal descriptor from the given
user-defined literal descriptor.
*/
  : value_con(other.value_con), spelling_con(other.spelling_con),
    op_sym(other.op_sym), suffix(other.suffix), type(other.type)
{
  other.value_con = NULL;
  other.spelling_con = NULL;
}  /* a_ud_literal_descr::a_ud_literal_descr */


a_ud_literal_descr::~a_ud_literal_descr()
/*
Destroy the current user-defined literal descriptor.
*/
{
  free_cached_token_constant(this->value_con);
  free_cached_token_constant(this->spelling_con);
}  /* a_ud_literal_descr::~a_ud_literal_descr */


an_unresolved_ud_literal_descr::an_unresolved_ud_literal_descr(
                                   const an_unresolved_ud_literal_descr &other)
/*
Copy-construct a new unresolved user-defined literal descriptor from the
given unresolved user-defined literal descriptor.
*/
  : value_con(alloc_cached_constant()), spelling_con(alloc_cached_constant()),
    curr_id_locator(other.curr_id_locator)
{
  copy_constant(other.value_con, this->value_con);
  copy_constant(other.spelling_con, this->spelling_con);
}  /* an_unresolved_ud_literal_descr::an_unresolved_ud_literal_descr */


an_unresolved_ud_literal_descr::an_unresolved_ud_literal_descr(
                                        an_unresolved_ud_literal_descr &&other)
/*
Move-construct a new unresolved user-defined literal descriptor from the
given unresolved user-defined literal descriptor.
*/
  : value_con(alloc_cached_constant()), spelling_con(alloc_cached_constant()),
    curr_id_locator(other.curr_id_locator)
{
  other.value_con = NULL;
  other.spelling_con = NULL;
}  /* an_unresolved_ud_literal_descr::an_unresolved_ud_literal_descr */


an_unresolved_ud_literal_descr::~an_unresolved_ud_literal_descr()
/*
Destroy the current unresolved user-defined literal descriptor.
*/
{
  free_cached_token_constant(this->value_con);
  free_cached_token_constant(this->spelling_con);
}  /* an_unresolved_ud_literal_descr::~an_unresolved_ud_literal_descr */

namespace detail {

template<typename a_Derived_type>
a_Derived_type a_token_cache_iterator_base<a_Derived_type>::operator++()
/*
Increment the current iterator and return the iterator state.
*/
{
  ++this->offset;
  return *static_cast<a_Derived_type*>(this);
}  /* a_token_cache_iterator_base::operator++ */


template<typename a_Derived_type>
a_Derived_type a_token_cache_iterator_base<a_Derived_type>::operator--()
/*
Decrement the current iterator and return the iterator state.
*/
{
  --this->offset;
  return *static_cast<a_Derived_type*>(this);
}  /* a_token_cache_iterator_base::operator-- */


template<typename a_Derived_type>
a_Derived_type a_token_cache_iterator_base<a_Derived_type>::operator+(
                                                                 int increment)
/*
Increment the current iterator by the given value and return a new iterator
state.
*/
{
  /* This function currently only supports moving the iterator forward. */
  check_assertion(increment > 0);

  a_Derived_type new_it(*static_cast<a_Derived_type*>(this));
  new_it.offset += increment;
  return new_it;
}  /* a_token_cache_iterator_base::operator+ */


template<typename a_Derived_type>
a_Derived_type a_token_cache_iterator_base<a_Derived_type>::operator-(
                                                                 int increment)
/*
Increment the current iterator by the given value and return a new iterator
state.
*/
{
  /* This function currently only supports moving the iterator backwards. */
  check_assertion(increment > 0);

  a_Derived_type new_it(*static_cast<a_Derived_type*>(this));
  new_it.offset -= increment;
  return new_it;
}  /* a_token_cache_iterator_base::operator- */


template<typename a_Derived_type>
template<typename a_Type>
a_boolean a_token_cache_iterator_base<a_Derived_type>::operator==(
                        const a_token_cache_iterator_base<a_Type> &other) const
/*
Return TRUE if the given token cache iterator represents the same position (in
the same cache) as this iterator; otherwise, return FALSE.
*/
{
  a_boolean result = TRUE;

  if (this->offset != other.offset) {
    result = FALSE;
  } else if (this->cache != other.cache) {
    result = FALSE;
  }  /* if */
  return result;
}  /* a_token_cache_iterator::operator== */

}  /* namespace detail */

a_shared_token& a_token_cache_iterator::operator*() const
/*
Return the current iterator value.
*/
{
  return (*this->cache)[(size_t)this->offset];
}  /* a_token_cache_iterator::operator* */


const a_shared_token& a_const_token_cache_iterator::operator*() const
/*
Return the current iterator value.
*/
{
  return (*this->cache)[(size_t)this->offset];
}  /* a_const_token_cache_iterator::operator* */


void a_token_cache::append_token(const a_cached_token &tok)
/*
Append the given token to this token cache.
*/
{
  this->tokens.emplace_back(tok);
}  /* a_token_cache::append_token */


void a_token_cache::append_token(a_cached_token &&tok)
/*
Append the given token to this token cache.
*/
{
  this->tokens.emplace_back(move_from(&tok));
}  /* a_token_cache::append_token */


void a_token_cache::append_token(const a_shared_token &tok)
/*
Append the given token to this token cache.
*/
{
  this->tokens.emplace_back(tok);
}  /* a_token_cache::append_token */


void a_token_cache::append_token(a_shared_token &&tok)
/*
Append the given token to this token cache.
*/
{
  this->tokens.emplace_back(move_from(&tok));
}  /* a_token_cache::append_token */


void a_token_cache::insert_token(a_token_cache_iterator it,
                                 const a_cached_token   &tok)
/*
Insert the given token to this token cache at the given iterator position.
*/
{
  this->check_iterator(it);
  this->tokens.insert((size_t)it.offset,
                      shared_obj<an_immutable_cached_token>(tok));
}  /* a_token_cache::insert_token */


void a_token_cache::insert_token(a_token_cache_iterator it,
                                 a_cached_token         &&tok)
/*
Insert the given token to this token cache at the given iterator position.
*/
{
  this->check_iterator(it);
  this->tokens.insert((size_t)it.offset,
                      shared_obj<an_immutable_cached_token>(move_from(&tok)));
}  /* a_token_cache::insert_token */


void a_token_cache::insert_token(a_token_cache_iterator it,
                                 const a_shared_token   &tok)
/*
Insert the given token to this token cache at the given iterator position.
*/
{
  this->check_iterator(it);
  this->tokens.insert((size_t)it.offset, tok);
}  /* a_token_cache::insert_token */


void a_token_cache::insert_token(a_token_cache_iterator it,
                                 a_shared_token         &&tok)
/*
Insert the given token to this token cache at the given iterator position.
*/
{
  this->check_iterator(it);
  this->tokens.insert((size_t)it.offset, move_from(&tok));
}  /* a_token_cache::insert_token */


void a_token_cache::move_copy_tokens(a_token_cache_iterator it,
                                     a_token_cache_iterator end_it)
/*
Perform a non-destructive move of the tokens in the range [it, end_it) into
this token cache.  The tokens moved into this token cache are appended to the
end of the token cache as they are moved.
*/
{
  sizeof_t num_elems = distance(it, end_it);

  /* Add the token range from src_cache to this token cache. */
  this->tokens.reserve(this->tokens.length() + num_elems);
  for (; it != end_it; ++it) {
    this->tokens.emplace_back(move_from(&(*it)));
  }  /* if */
}  /* a_token_cache::move_copy_tokens */


void a_token_cache::remove_token_range(a_token_cache_iterator it,
                                       a_token_cache_iterator end_it)
/*
Remove the tokens in the range [it, end_it).
*/
{
  this->check_iterator(it);
  this->check_iterator(end_it);

  sizeof_t num_elems = distance(it, end_it);
  this->tokens.remove_many((size_t)it.offset, num_elems);
}  /* a_token_cache::remove_token_range */


void a_token_cache::remove_first_token()
/*
Remove the first token in the token cache.
*/
{
  this->remove_token(this->begin());
}  /* a_token_cache::remove_first_token */


void a_token_cache::remove_token(a_token_cache_iterator it)
/*
Remove the token at the given iterator position.
*/
{
  this->tokens.remove((size_t)it.offset);
}  /* a_token_cache::remove_token */


void a_token_cache::remove_tokens(a_token_cache_iterator it)
/*
Remove all tokens in the range [it, this->end()).
*/
{
  this->remove_token_range(it, this->end());
}  /* a_token_cache::remove_tokens */


void a_token_cache::check_iterator(a_token_cache_iterator it)
/*
Validate that the given iterator is associated with this token cache and that
it is within the range [this->begin(), this->end()).
*/
{
#if EXPENSIVE_CHECKING
  a_token_cache_iterator it_end = this->end();
  if (it == it_end) {
    /* Okay */
  } else {
    a_token_cache_iterator curr_it = this->begin();

    for (; curr_it != it_end; ++curr_it) {
      if (curr_it == it) {
        break;
      }  /* if */
    }  /* for */
    /* If this assertion fails, the iterator was not part of the token
       cache. */
    check_assertion(curr_it != this->end());
  }  /* if */
#endif /* EXPENSIVE_CHECKING */
}  /* a_token_cache::check_iterator */


void a_token_cache::remove_non_pragma_tokens_after(a_token_cache_iterator it)
/*
Remove all non-pragma tokens in the range (it, this->end()).  The token at *it
will have its end position updated to include the end position of the last
token removed.
*/
{
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_token_cache_iterator last_non_pragma_it = it;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_token_cache_iterator end_it = this->end();

  for (a_token_cache_iterator lookahead_it = it + 1; lookahead_it != end_it;
       ++lookahead_it) {
    const a_shared_token& tok = *lookahead_it;

    if (!tok->is_pragma()) {
#if EXTRA_SOURCE_POSITIONS_IN_IL
      /* Update the end of token position of the resulting string with the
         end position of the subsequent segment that is about to be freed.
         This results in the ending position of the last string segment
         being used as the ending position of the concatenated string. */
      a_cached_token replacement_tok = **last_non_pragma_it;

      replacement_tok.set_ending_source_position(
                                           *tok->get_ending_source_position());
      *last_non_pragma_it = move_from(&replacement_tok);
#endif /*  EXTRA_SOURCE_POSITIONS_IN_IL */
    }  /* if */
  }  /* for */

  /* As of the time of writing, there is an existing issue when self-compiling
     the front end with the following combination of features:
       - multi-translation unit compilation mode
       - IL_SHOULD_BE_WRITTEN_TO_FILE set to TRUE
       - IA64_ABI set to TRUE

     If a lambda is used below under these conditions, the alternative entry
     points from the IA-64 ABI will not be properly merged and will result in
     an incorrect IL write -> read process.  Thus to avoid triggering this
     issue, a local class has been used in place of a lambda. */
  struct a_criterion {
    a_boolean operator()(const a_shared_token &tok) const {
      return !tok->is_pragma();
    }
  };
  this->tokens.remove_if((size_t)(it.offset + 1), a_criterion());
}  /* a_token_cache::remove_non_pragma_tokens_after */


const a_shared_token& a_token_cache::get_first_token() const
/*
Return a reference to the first token in the cache.
*/
{
  check_assertion(!this->is_empty());
  return this->tokens[0];
}  /* a_token_cache::get_first_token */


const a_shared_token& a_token_cache::get_last_token() const
/*
Return a reference to the last token in the cache.
*/
{
  check_assertion(!this->is_empty());
  return this->tokens[this->tokens.length() - 1];
}  /* a_token_cache::get_last_token */

namespace detail {

template<typename a_Cache_type, typename an_Iterator_type>
INLINE void find_first_and_last_impl(
                                a_Cache_type            *cache,
                                a_token_sequence_number first_token_number,
                                a_token_sequence_number last_token_number,
                                an_Iterator_type        *before_first_token_it,
                                an_Iterator_type        *last_token_it)
/*
This function sets *before_first_token_it to the token immediately preceding
the first token with a starting token sequence number greater than or equal to
first_token_number.  *last_token_number is set to the last token in the cache
with an ending token sequence number equal to or less than last_token_number.

If first_token_number is NO_TOKEN_SEQUENCE_NUMBER *before_first_token_it is set
to the position immediately before the first token in the cache.  If
last_token_it is NO_TOKEN_SEQUENCE_NUMBER *last_token_it is set to the last
token in the cache.
*/
{
  check_assertion(first_token_number <= last_token_number ||
                  first_token_number == NO_TOKEN_SEQUENCE_NUMBER ||
                  last_token_number == NO_TOKEN_SEQUENCE_NUMBER);
  *before_first_token_it = an_Iterator_type(cache, -1);
  *last_token_it = cache->get_last_token_iter();
  if (first_token_number != NO_TOKEN_SEQUENCE_NUMBER) {
    auto      get_tsn = [cache](size_t idx) -> a_token_sequence_number {
      return (*cache)[idx]->get_starting_seq_number();
    };
    ptrdiff_t first_idx = low_bound(cache->length(), first_token_number,
                                    get_tsn);

    if (first_idx == -1) {
      first_idx = (ptrdiff_t)(cache->length() - 1);
    }  /* if */

    a_token_sequence_number curr_tsn = first_token_number;
    for (; first_idx != -1; --first_idx) {
      const a_shared_token &tok = (*cache)[(size_t)first_idx];

      curr_tsn = tok->get_starting_seq_number();

      if (curr_tsn < first_token_number) {
        *before_first_token_it = an_Iterator_type(cache, (int)first_idx);
        break;
      }  /* if */
    }  /* for */
    check_assertion_or_expect_error_str(
                      first_idx != -1 || curr_tsn == first_token_number,
                      "a_token_cache::find_first_and_last: first_tsn missing");
  }  /* if */
  if (last_token_number != NO_TOKEN_SEQUENCE_NUMBER) {
    auto      get_tsn = [cache](size_t idx) -> a_token_sequence_number {
      return (*cache)[idx]->get_ending_seq_number();
    };
    ptrdiff_t last_idx = low_bound(cache->length(), last_token_number + 1,
                                   get_tsn);

    if (last_idx == -1) {
      last_idx = (ptrdiff_t)(cache->length() - 1);
    }  /* if */
    for (; last_idx != -1; --last_idx) {
      const a_shared_token &tok = (*cache)[(size_t)last_idx];

      a_token_sequence_number curr_tsn = tok->get_ending_seq_number();
      if (curr_tsn <= last_token_number) {
        *last_token_it = an_Iterator_type(cache, (int)last_idx);
        break;
      }  /* if */
    }  /* for */
    check_assertion_or_expect_error_str(
                       last_idx != -1,
                       "a_token_cache::find_first_and_last: last_tsn missing");
  }  /* if */
}  /* find_first_and_last_impl */

}  /* namespace detail */

void a_token_cache::find_first_and_last(
                                a_token_sequence_number first_token_number,
                                a_token_sequence_number last_token_number,
                                a_token_cache_iterator  *before_first_token_it,
                                a_token_cache_iterator  *last_token_it)
/*
This is an interface to find_first_and_last_impl that uses non-constant
iterators.  See find_first_and_last_impl for more information.
*/
{
  detail::find_first_and_last_impl(this, first_token_number, last_token_number,
                                   before_first_token_it, last_token_it);
}  /* a_token_cache::find_first_and_last */


void a_token_cache::find_first_and_last(
                           a_token_sequence_number      first_token_number,
                           a_token_sequence_number      last_token_number,
                           a_const_token_cache_iterator *before_first_token_it,
                           a_const_token_cache_iterator *last_token_it) const
/*
This is an interface to find_first_and_last_impl that uses constant iterators.
See find_first_and_last_impl for more information.
*/
{
  detail::find_first_and_last_impl(this, first_token_number, last_token_number,
                                   before_first_token_it, last_token_it);
}  /* a_token_cache::find_first_and_last */


void a_cached_token::set_seq_number(a_token_sequence_number starting_num,
                                    a_token_sequence_number ending_num)
/*
This function updates the sequence numbers of the current token with the
given starting and ending numbers.
*/
{
  this->token_sequence_number = starting_num;
  this->ending_token_sequence_number = ending_num;
}  /* a_cached_token::set_seq_number */


INLINE void cache_token(a_token_cache_ptr       cache,
                        a_token_kind            tok,
                        const a_source_position *pos)
/*
Add tok to cache.  pos is the position of the token.
*/
{
  a_token_sequence_number seq = NO_TOKEN_SEQUENCE_NUMBER;

  if (tok != tok_error) {
    seq = assign_new_token_sequence_number();
  }  /* if */
  cache->append_token(build_cached_token(tok, seq, pos));
}  /* cache_token */


extern a_shared_token build_tok_constant(
                              a_constant_ptr          cp,
                              const a_source_position *pos,
                              a_token_kind            kind = tok_gen_constant);

extern a_shared_token build_tok_body_replacement(
                                    a_token_kind            token,
                                    a_token_sequence_number start_seq,
                                    a_token_sequence_number end_seq,
                                    a_symbol_ptr            symbol,
                                    a_boolean               semicolon_inserted,
                                    const a_source_position *pos);


extern a_shared_token build_tok_removed_expr(a_token_sequence_number start_seq,
                                             const a_source_position *pos,
                                             a_token_cache_iterator  begin,
                                             a_token_cache_iterator  end);

extern a_shared_token build_tok_pragma(a_token_sequence_number seq,
                                       const a_source_position *pos);

extern a_shared_token build_tok_pp(char                    *text,
                                   a_targ_size_t           len,
                                   const a_source_position *pos);

extern a_shared_token build_tok_identifier(a_const_char            *str,
                                           a_targ_size_t           len,
                                           const a_source_position *pos);

extern a_shared_token build_tok_ifc_ref(a_token_kind                  kind,
                                        a_lexical_ifc_index_reference ref,
                                        const a_source_position       *pos);

extern a_shared_token build_tok_ud_literal(
                                          a_constant_ptr          str_constant,
                                          a_const_char            *suffix,
                                          const a_source_position *pos);

extern a_shared_token build_tok_ud_literal(
                                     a_constant_ptr          value_constant,
                                     a_constant_ptr          spelling_constant,
                                     a_const_char            *suffix,
                                     a_type_ptr              type,
                                     const a_source_position *pos);

extern a_shared_token build_tok_resolved_type(a_type_ptr              type,
                                              const a_source_position *pos);

/* Save the current token in a token cache. */
extern void cache_curr_token(a_token_cache *cache);
extern void cache_curr_token_fresh(a_token_cache *cache);
/* Save a token stream in a token cache. */
extern void cache_token_stream(a_token_cache      *cache,
                               a_token_set_array  stop_tokens);

extern
void cache_token_stream_coalesce_identifiers(a_token_cache_ptr  cache,
                                             a_token_set_array  stop_tokens);

extern void cache_token_stream_full(a_token_cache_ptr  cache,
                                    a_token_set_array  stop_tokens,
                                    a_cts_flag_set     options);

extern void cache_std_attribute(a_token_cache	*cache,
                                a_boolean	add_tokens_to_cache);

extern a_boolean cache_token_stream_until_matching_token(
                                a_token_cache           *cache,
                                a_cts_flag_set          options);

extern a_token_kind get_token_to_be_cached(void);
/* Put some cached tokens on the get_token rescan list. */
extern void rescan_cached_tokens(a_token_cache *cache,
                                 a_boolean     discard_curr_token = FALSE);
extern void rescan_copy_of_cache(a_token_cache *cache);
/* Push a reusable cache onto the reusable cache stack. */
extern void rescan_reusable_cache(const a_reusable_token_cache &cache);


INLINE void rescan_persistent_reusable_cache(a_token_cache *cache)
/*
This is a convenience wrapper function around rescan_reusable_cache for
persistent reusable caches (see a_reusable_token_cache for more information
about persistent reusable caches).
*/
{
  rescan_reusable_cache(a_reusable_token_cache(cache));
}  /* rescan_persistent_reusable_cache */


INLINE void rescan_shared_reusable_cache(a_shared_token_cache cache)
/*
This is a convenience wrapper function around rescan_reusable_cache for
shared reusable caches (see a_reusable_token_cache for more information
about shared reusable caches).
*/
{
  rescan_reusable_cache(a_reusable_token_cache(move_from(&cache)));
}  /* rescan_shared_reusable_cache */


extern void begin_caching_fetched_tokens(a_boolean include_curr_token);
extern void end_caching_fetched_tokens(void);

extern
void copy_tokens_from_cache(const a_token_cache     *src_cache,
                            a_token_sequence_number first_tsn,
                            a_token_sequence_number last_tsn,
                            a_boolean               include_last_token,
                            a_token_cache_ptr       dest_cache);

extern
a_boolean skip_to_token_sequence_number(a_token_cache_ptr       cache,
                                        a_token_sequence_number tsn);

extern a_boolean scanning_from_token_cache(void);

extern a_reusable_token_cache get_token_cache_being_scanned(void);

extern
void split_token_cache(a_token_cache	       *cache1,
                       a_token_cache	       *cache2,
                       a_token_sequence_number split_location,
                       a_boolean	       include_prev_token,
                       a_boolean	       okay_if_not_found);

extern
void update_reusable_cache_rescan_location(a_token_cache_ptr       cache,
                                           a_token_sequence_number tsn);

extern void increment_dependent_scans_for_reusable_cache(void);

extern void decrement_dependent_scans_for_reusable_cache(void);

extern a_boolean same_string_ignoring_underscores(a_const_char *s1, 
                                                  a_const_char *s2);

/*
Variables to flag whether extra token separators should be emitted in
preprocessed (or raw listing) output to ensure that the output is correctly
tokenizable.  The second variable only applies to the next line of output
(after which it is reset to the value of the first variable).
*/
EXTERN_THREAD a_boolean
		no_token_separators_in_pp_output;

EXTERN_THREAD a_boolean
		no_token_separators_in_this_line_of_pp_output;

EXTERN_THREAD a_preinclude_file_ptr
		next_preinclude_file;
			/* The next preinclude file to be processed in either
			   the normal or macro-only preinclude file list,
			   depending on the setting of
			   processing_macro_preincludes. */

EXTERN_THREAD a_boolean
		processing_macro_preincludes;
			 /* TRUE when processing the list of macro-only
			    preincludes. */

EXTERN_THREAD a_boolean
		next_token_is_top_level_decl_start;
			/* Flag toggled in translation_unit when advancing
			   past a token that marks the end of a "top-level"
			   declaration (i.e., a ";" or "}").  When this flag
			   is TRUE, the state of the compiler is in effect
			   between declarations -- or else just before the
			   first declaration or just after the last. */

EXTERN_THREAD a_boolean
		caching_tokens;
			/* TRUE when caching a token stream to be scanned
			   later. */

/*
Data structure used in deciding where to put extra blanks to separate
adjacent tokens in textual preprocessing output.
*/
EXTERN_THREAD a_byte
		pp_lexical_category[CHAR_MAX-CHAR_MIN+1];
			/* For each character, the lexical category to
			   be used in preprocessing output.  These categories
			   are used to decide when extra token-separating
			   blanks must be inserted between tokens resulting
			   from macro expansion. */
#define PLC_SINGLETON 1
			/* A character in the singleton category always
			   stands alone as a token, and thus no extra blank
			   is ever required next to it for token separation. */
#define PLC_ID_OR_NUMBER 2
			/* Characters appearing in identifiers or pp-numbers
			   (see is_id_char). */
#define PLC_OTHER 3
			/* All other characters. */


/* Read next logical source line. */
a_boolean read_logical_source_line(a_boolean do_pop_on_end_of_file,
                                   a_boolean extend_current_line);
/* Check character as identifier character. */
extern a_boolean f_is_identifier_char(a_const_char *ptr,
                                      int          *len,
                                      a_boolean    is_identifier_start);

#define is_identifier_char(ptr, len, is_start)                               \
  (!char_ends_id[*(ptr)-CHAR_MIN] && f_is_identifier_char(ptr, len, is_start))

/* Check a character sequence as the spelling of an identifier. */
extern a_boolean is_identifier_spelling(a_const_char  *str,
                                        sizeof_t      len);

#if CPPCLI_ENABLING_POSSIBLE && EDG_WIN32
extern an_error_code is_valid_UCN_identifier_char(
                                           unsigned long uchar,
                                           a_boolean     is_identifier_start);
#endif /* CPPCLI_ENABLING_POSSIBLE && EDG_WIN32 */
/* Check character as nonstandard. */
extern a_boolean is_nonstandard_character(char ch);
/* Skip white space. */
extern void skip_white_space(void);
/* Narrow character overflow handling. */
extern void register_char_overflow(an_error_severity severity,
                                   an_error_code     err_code,
                                   a_source_position *pos);
extern void clear_char_overflows(void);
/* Concatenate adjacent string literals in the current string constant. */
extern a_token_kind concat_adjacent_string_literals(
                                                 a_boolean function_name_case);
/* Read tokens from a #embed expansion. */
extern void insert_embed_contents(a_source_position *pos);
/* Retrieve a character replaced in a #embed directive by a lexical escape. */
extern char orig_char_from_embed_directive(a_const_char lex_escape);
/* Get next token. */
extern a_token_kind get_token(void);
/* Return whether a token is a keyword token. */
extern a_boolean is_keyword_token(a_token_kind	token);

/*
The kinds of interpolator that may appear in a token sequence, i.e., in the
operand of a "^^{ ... }" reflection operator.  Each interpolator is introduced
by a backslash and has a parenthesized, bracketed, or braced operand list.
*/
enum an_interpolator_kind : a_byte {
  ipk_none,		/* Not an interpolator introducer. */
  ipk_identifier,	/* "\[...]": an identifier formed from the operands. */
  ipk_splice,		/* "\[: ... :]": a splice of the operand value, i.e.,
			   the equivalent of "[: \val(...) :]". */
  ipk_tokens,		/* "\{...}": the tokens of the operand sequence. */
  ipk_value,		/* "\val(...)": a pseudo-token for the operand
			   value. */
  ipk_string		/* "\str(...)": a string literal whose contents are
			   those of the operand string view. */
};

/* Classify the interpolator, if any, introduced by the current token. */
extern an_interpolator_kind interpolator_kind_of_curr_token(
                                                a_token_kind  *closing_token);
/* Transform a tok_colon followed by a ':' into a tok_colon_colon. */
extern void add_colon_to_tok_colon_if_present(void);
/* Generate a line-identifying directive in preprocessing output. */
extern void gen_pp_line_info(char      kind,
		             a_boolean next_line);
/* Generate textual preprocessing output for the current line. */
extern void gen_pp_output_for_curr_line(a_const_char *start_loc);
/* Generate a line information record in the raw listing file. */
extern void gen_rlisting_line_info(char kind);
/* Generate raw listing output for the macro-expanded form of the current
   line. */
extern void gen_expanded_raw_listing_output_for_curr_line(
                                                   a_boolean do_inserted_text);
extern void finish_raw_listing_file(void);
extern void add_source_line_modif_to_hash_table(a_source_line_modif_ptr slmp);
extern void rem_source_line_modif_from_hash_table(
                                                a_source_line_modif_ptr slmp);
extern a_token_kind get_token_with_colon_separation(
                                              a_boolean *seen_tok_colon_colon);
#if FULLY_RESOLVED_MACRO_POSITIONS
extern a_macro_text_map_entry_ptr find_macro_text_map_entry_for_offset(
                                                a_macro_text_map_ptr mtmp,
                                                sizeof_t             offset);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
extern void add_concatenation_record(a_concatenation_record_ptr *headp,
                                     a_concatenation_record_ptr *tailp,
                                     char                       *line_loc,
                                     a_symbol_ptr               macro_sym);
/* Add an entry recording a logical modification to the source line. */
extern a_source_line_modif_ptr add_source_line_modif(
                          a_const_char              *line_loc,
                          sizeof_t                  num_chars_to_delete,
                          a_const_char              *inserted_text,
                          a_const_char              *end_inserted_text);

/*
Add a source line modification entry to indicate deletion of num_chars
characters starting at line_loc.  The inserted_chars area is used for the
zero-length replacement string.  for_comment is TRUE if the modification is
due to a comment.  raw_or_exp is TRUE if the modification is for the raw or
expanded version of a macro argument; see choose_raw_or_expanded_arg in
macro.c for details.  If no text is to be deleted, nothing is done (there
is no room for the ATTENTION_MARKER).
*/
#define add_deletion_source_line_modif(line_loc, num_chars, for_comment,     \
                                       raw_or_exp)                           \
{                                                                            \
  if ((num_chars) > 0) {                                                     \
    a_source_line_modif_ptr dslmp;                                           \
    dslmp = add_source_line_modif(line_loc, (sizeof_t)(num_chars),           \
                                  (char *)NULL, (char *)NULL);               \
    *dslmp->inserted_chars   = LE_ESCAPE;                                    \
    dslmp->inserted_chars[1] = LE_END_OF_INSERTION;                          \
    dslmp->inserted_text = dslmp->end_inserted_text = dslmp->inserted_chars; \
    dslmp->is_for_comment = for_comment;                                     \
    dslmp->is_raw_or_expanded_arg = raw_or_exp;                              \
  }  /* if */                                                                \
}  /* add_deletion_source_line_modif */

/* Free a source line modification entry. */
extern void free_source_line_modif(a_source_line_modif_ptr *slmp);
/* Remove a source line modification entry from the source_line_modif_list. */
extern void rem_source_line_modif(a_source_line_modif_ptr slmp);
/* Find the source line modification entry associated with a given source
   location. */
extern a_source_line_modif_ptr assoc_source_line_modif_full(
                                                 a_const_char *loc_in_line,
                                                 a_boolean    failure_allowed);
/* Interface for assoc_source_line_modif_full that assumes a matching
   source line modification will be found. */
#define assoc_source_line_modif(loc_in_line)                          \
  assoc_source_line_modif_full((loc_in_line), /*failure_allowed=*/FALSE)
/* Find the parent modification of a given source line modification. */
#define parent_source_line_modif(slmp)                                \
  ((slmp)->parent_modif_determined ? (slmp)->parent_modif :           \
                                     f_parent_source_line_modif(slmp))
extern a_source_line_modif_ptr f_parent_source_line_modif(
                                                 a_source_line_modif_ptr slmp);
/* Set the parent pointer of ins_slmp to point to slmp. */
#define set_parent_modif(ins_slmp, slmp)                              \
{ (ins_slmp)->parent_modif = (slmp);                                  \
  (ins_slmp)->parent_modif_determined = TRUE;                         \
}  /* set_parent_modif */
/* Find the source line modification that affects the attention marker
   at a given source location. */
extern a_source_line_modif_ptr nested_source_line_modif(
                                                    a_const_char *loc_in_line);
extern a_boolean has_nested_source_line_modif(a_const_char *loc_in_line);
/* Convert a character location in the source line to a source sequence
   number and column. */
extern void conv_line_loc_to_source_pos(a_const_char      *loc_in_line,
                                        a_source_position *position_var);
extern void report_missing_closing_delimiter(an_error_code     msg,
                                             an_error_code     matching_msg,
                                             a_source_position *matching_pos);
/* Check for a specific token. */
extern a_boolean required_token(a_token_kind      token,
                                an_error_code     error_code,
                                an_error_code     matching_code = ec_no_error,
                                a_source_position *matching_start_pos = NULL);
extern a_boolean required_token_no_advance(
                                 a_token_kind      token,
                                 an_error_code     error_code,
                                 an_error_code     matching_code = ec_no_error,
                                 a_source_position *matching_start_pos = NULL);
/* Check for a specific token, at the bottom of a loop for a repeated
   syntactic construct. */
extern a_boolean loop_token(a_token_kind token);
/* Look ahead at the token following the current one. */
extern a_token_kind next_token_full(a_token_sequence_number *seq,
                                    struct a_symbol_header  **sym_hdr);
/* Macro that calls next_token_full with default arguments. */
#define next_token()							\
  (next_token_full((a_token_sequence_number*)NULL,			\
                   (struct a_symbol_header **)NULL))
#define next_token_with_seq_number(seq) 				\
  (next_token_full((seq), (struct a_symbol_header **)NULL))

/* Look ahead at the next two tokens. */
extern
a_token_kind next_two_tokens(a_token_kind	first_token_must_be,
                             a_token_kind	*token_2);

/* Back up one token. */
extern void unget_token(void);

extern a_boolean pending_ifc_func_body_next(a_symbol_ptr  rout_sym);

extern a_symbol_ptr coalesce_template_class_reference
			(a_symbol_ptr		   template_symbol,
			 an_identifier_options_set options,
			 a_boolean		   *err);

extern void begin_rescan_of_pragma_tokens(struct a_pending_pragma *ppp);

extern void wrapup_rescan_of_pragma_tokens(a_boolean  error_in_pragma);

extern a_boolean spliced_name_qualifier_next(void);

extern a_boolean pack_index_next(void);

extern a_token_kind token_kind_following_pack_index_specifier(void);

extern a_boolean f_is_generalized_identifier_start
                     (an_identifier_options_set options,
                      a_type_ptr                field_sel_type);
extern a_boolean coalesce_and_lookup_qualified_name
                     (an_identifier_options_set        options,
		      an_identifier_lookup_mode	       ilm,
                      a_boolean			       *err);
extern a_symbol_ptr coalesce_and_lookup_generalized_identifier
                        (an_identifier_options_set        options,
                         an_identifier_lookup_mode        ilm,
                         a_boolean                        *err);

extern unsigned long scan_universal_character(
                                           a_const_char **start_pos,
                                           a_boolean    is_identifier,
                                           a_boolean    is_identifier_start,
                                           a_boolean    issue_diagnostics,
                                           a_boolean    *malformed_err = NULL);

extern unsigned long scan_named_unicode_char(
					a_const_char	**start_pos,
					a_boolean	is_identifier,
				        a_boolean	is_identifier_start,
					a_boolean	issue_diagnostics,
					a_boolean	update_pos_on_error);

/*
Unicode values occupy at most 21 bits.
*/
#define MAX_UNICODE_VAL 0x1ffffful

#if ABI_COMPATIBILITY_VERSION >= 302
extern char *make_canonical_identifier(a_const_char *identifier,
                                       sizeof_t     *length,
                                       a_boolean    force_ucn);
#else /* !(ABI_COMPATIBILITY_VERSION >= 302) */
/*
No translation of identifiers was done for older ABIs.  Simply
return the original identifier pointer.
*/
#define make_canonical_identifier(identifier, length, force_ucn) (identifier)
#endif /* ABI_COMPATIBILITY_VERSION >= 302 */

/*
Macro that returns TRUE when a given symbol header is for a given identifier
string.
*/
#define symbol_header_is_for_identifier_string(sym_hdr, tok_str)             \
  ((sym_hdr)->identifier[0] == (tok_str)[0] &&                               \
   strcmp((sym_hdr)->identifier, (tok_str)) == 0)

extern a_boolean curr_token_is_identifier_string(a_const_char *tok_str);

extern a_boolean check_context_sensitive_keyword(a_token_kind  tok_kind,
                                                 a_const_char  *tok_str);

/* Flags describing the encoding prefix, if any, of a string or character
   literal and which kind of literal is being processed.  The flags are
   defined as follows:

     kReee
     ||---
     || |
     || +---- The encoding prefix (U, u, u8, L, or none)
     |+------ Whether the literal is raw or not (string literals only)
     +------- The kind of literal (string or character) */

#define SCLK_NOT_A_LITERAL      -1
			/* The characters are not a literal prefix. */
#define SCLK_ORDINARY_LITERAL   0x01
			/* No encoding prefix */
#define SCLK_UTF8_LITERAL       0x02
			/* u8"..." or (C++17) u8'x' */
#define SCLK_CHAR16_T_LITERAL   0x03
			/* u"..." or u'x' */
#define SCLK_CHAR32_T_LITERAL   0x04
			/* U"..." or U'x' */
#define SCLK_WIDE_LITERAL       0x05
			/* L"..." or L'x' */
#define SCLK_RAW_STRING_LITERAL 0x08
			/* R"...", u8R"...", uR"xxx", or UR"..." */
#define SCLK_STRING_LITERAL     0x10
			/* TRUE for string literal, FALSE for char literal */
#define SCLK_FUNCTION_NAME      0x20
			/* TRUE if the literal is the result of __FUNCTION__
			   or similar constructs. */

/* A convenient name for a narrow non-raw string literal: */
#define SCLK_ORDINARY_STRING_LITERAL \
                                  (SCLK_ORDINARY_LITERAL | SCLK_STRING_LITERAL)

/* Extract the encoding prefix from a_string_or_char_literal_kind. */
#define literal_encoding_prefix(k) ((k) & 0x7)

/* Return the offset of the first character following the prefix (if any)
   and quote character of a string or character literal described by the
   specified a_string_or_char_literal_kind. */
#define offset_to_start_of_literal_value(k)                                  \
  ((int)(((k) & SCLK_RAW_STRING_LITERAL) != 0) /* 1 for "R" */               \
   + ((literal_encoding_prefix(k) > SCLK_UTF8_LITERAL) ? 1 /* 1 for u/U/L */ \
      : (literal_encoding_prefix(k) == SCLK_UTF8_LITERAL) ? 2 /* 2 for u8 */ \
      : 0) /* 0 for no prefix */                                             \
   + 1 /* 1 for quoting character */)


inline a_string_or_char_literal_kind char_kind_to_str_literal_kind(
                                              a_character_kind  character_kind)
/*
Given a character kind, return the appropriate string literal kind.
*/
{
  a_string_or_char_literal_kind result;

  switch (character_kind) {
    case chk_char:     result = SCLK_ORDINARY_LITERAL; break;
    case chk_wchar_t:  result = SCLK_WIDE_LITERAL;     break;
    case chk_char8_t:  result = SCLK_UTF8_LITERAL;     break;
    case chk_char16_t: result = SCLK_CHAR16_T_LITERAL; break;
    case chk_char32_t: result = SCLK_CHAR32_T_LITERAL; break;
    default:
      unexpected_condition();
  }  /* switch */
  result |= SCLK_STRING_LITERAL;
  return result;
}  /* char_kind_to_str_literal_kind */


inline a_const_char *readable_literal_kind(
                                        a_string_or_char_literal_kind lit_kind)
/*
Return a character string with a human-readable version of the specified
literal kind.  Uses a local buffer in order to be usable in standalone
utilities that do not have memory management code.
*/
{
#define add_str(s) { strcpy(buff + len, s); len += sizeof(s) - 1; }
  STATIC_THREAD char buff[64];
  size_t len = 0;

  if (lit_kind == SCLK_NOT_A_LITERAL) {
    add_str("not a literal");
  } else {
    if (lit_kind & SCLK_RAW_STRING_LITERAL) {
      add_str("raw ");
    }  /* if */
    switch (literal_encoding_prefix(lit_kind)) {
      case SCLK_ORDINARY_LITERAL:
        add_str("ordinary ");
        break;
      case SCLK_UTF8_LITERAL:
        add_str("UTF8 ");
        break;
      case SCLK_CHAR16_T_LITERAL:
        add_str("char16_t ");
        break;
      case SCLK_CHAR32_T_LITERAL:
        add_str("char32_t ");
        break;
      case SCLK_WIDE_LITERAL:
        add_str("wide ");
        break;
      default:
        add_str("** UNKNOWN KIND ** ");
        break;
    }  /* switch */
    if (lit_kind & SCLK_FUNCTION_NAME) {
      add_str("function name ");
    } else if (lit_kind & SCLK_STRING_LITERAL) {
      add_str("string ");
    } else {
      add_str("character ");
    }  /* if */
    add_str("literal");
  }  /* if */
  return buff;
#undef add_str
}  /* readable_literal_kind */

extern a_string_or_char_literal_kind scan_encoding_prefix(a_const_char *loc);

extern a_boolean accum_quoted_string(
                  unsigned long                 *num_chars,
                  a_boolean                     is_header_name,
                  a_string_or_char_literal_kind literal_kind,
                  char                          quoting_char,
                  a_const_char                  *start_of_raw_string_delimiter,
                  int                           raw_string_delimiter_len,
                  a_const_char                  *string_start_loc,
                  an_orig_line_modif_ptr        last_olmp = NULL);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_boolean is_valid_GUID_string(a_const_char  *str,
                                      a_targ_size_t length);

#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
extern void setlocale_pragma(a_pending_pragma_ptr	ppp);
#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
#endif /* !MICROSOFT_EXTENSIONS_ALLOWED */

/*
Macro that compares the current token (an identifier) against a constant
string, and returns true if the two match.
*/
#define string_is(str, len, cmp)                                      \
  ((len) == sizeof(cmp)-1 &&                                          \
   strncmp(cmp, str, size_t_arg(sizeof(cmp)-1)) == 0)

/* Macro that tests whether f_is_generalized_identifier_start needs
   to be called.  We don't need to call it in C mode, or if we have an
   identifier that has already been coalesced.  There are other cases that
   could be eliminated such as tok_ptr_to_member (which returns FALSE) and
   current tokens that are not things that could start an identifier.  These
   have smaller payoffs so are not currently included. */
#define is_generalized_identifier_start_full(options, type)		\
  /* if */ ((C_dialect != C_dialect_cplusplus) /* { */ ?		\
    curr_token == tok_identifier					\
  /* } else { */ :							\
    /* if */ (curr_token == tok_identifier &&				\
              locator_for_curr_id.has_been_coalesced) /* { */ ?		\
      TRUE								\
    /* } else { */ :							\
      f_is_generalized_identifier_start(options, (type)))		\
  /* } */								\

/*
Interface to is_generalized_identifier_start_full that provides a default
NULL type pointer.
*/
#define is_generalized_identifier_start(options)			\
  (is_generalized_identifier_start_full((options), (a_type_ptr)NULL))

/* Return TRUE if the current token is the start of a C++ qualified
   name (including a simple identifier).  This version of the macro is
   used in expression contexts in which a "<" should be considered a
   less than sign and not the start of a template argument list. */
#define is_expr_qualified_name_start()					  \
 (is_generalized_identifier_start(GID_IS_EXPR_CONTEXT))

/* Return TRUE if the current token is the start of a C++ qualified
   name (including a simple identifier).  This version of the macro is
   used in declaration contexts in which a "<" should be considered to
   be the start of a template argument list. */
#define is_decl_qualified_name_start()					  \
 (is_generalized_identifier_start(GID_NO_OPTIONS))

/*
Return TRUE if the indicated token is a type qualifier.  This is
straightforward for const, volatile, and restrict but __unaligned is
valid only with certain configurations.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
/*lint -save -e773*/
#define or_is_unaligned_token(tok) || (tok) == tok_unaligned
/*lint -restore*/
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define or_is_unaligned_token(tok)  /* Nothing */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
/*
Unified Parallel C adds several new type qualifiers.
*/
#if UPC_EXTENSIONS_ALLOWED
/*lint -save -e773*/
#define or_is_upc_qual_token(tok)                                             \
  || (tok) == tok_upc_shared                                                  \
  || (tok) == tok_upc_strict || (tok) == tok_upc_relaxed
/*lint -restore*/
#else /* !UPC_EXTENSIONS_ALLOWED */
#define or_is_upc_qual_token(tok) /* Nothing */
#endif /* UPC_EXTENSIONS_ALLOWED */

/*
Return TRUE if the current token is the standard "restrict" token or the
equivalent GNU/Microsoft "__restrict" token.  (Distinct token kinds are used
because uses of the nonstandard spelling are warned about in some modes.)
*/
#define is_restrict_token(tok)                                                \
  ((tok) == tok_restrict || (tok) == tok_gnu_restrict)

#define is_type_qualifier_token(tok)                                          \
  ((tok) == tok_const || (tok) == tok_volatile || is_restrict_token(tok) ||   \
   (tok) == tok_c11_atomic ||                                                 \
   (tok) == tok_nullable || (tok) == tok_nonnull ||                           \
   (tok) == tok_null_unspecified                                              \
   or_is_unaligned_token(tok)                                                 \
   or_is_upc_qual_token(tok))

#define is_literal_token(tok)                                                 \
 ((int)(tok) >= (int)tok_first_literal_token_kind &&                          \
  (int)(tok) <= (int)tok_last_literal_token_kind)


inline a_boolean is_resolved_id_pseudo_token(a_token_kind tok)
/*
Given a token, return TRUE if it's a resolved identifier pseudo token;
otherwise, return FALSE.
*/
{
  a_boolean result = FALSE;

  if (tok == tok_ifc_entity_ref || tok == tok_ifc_decl_ref) {
    result = TRUE;
  }  /* if */
  return result;
}  /* is_resolved_id_pseudo_token */


extern a_symbol_ptr resolve_id_pseudo_token_to_sym();

extern void push_next_preinclude_file(void);

/* Push a file onto the input stack. */
extern void open_file_and_push_input_stack(
                                         a_const_char *file_name,
                                         a_boolean    use_search_path,
                                         a_boolean    is_include_file,
                                         a_boolean    is_system_include,
                                         a_boolean    is_preinclude,
                                         a_boolean    preinclude_macros,
                                         a_boolean    is_implicit_include,
                                         a_boolean    is_include_next,
                                         a_boolean    continue_on_open_failure,
                                         a_boolean    *include_was_suppressed);

extern a_boolean open_file_for_input(
		a_const_char			*file_name,
		a_boolean			use_search_path,
		a_boolean			is_include_file,
		a_boolean			is_system_include,
		a_boolean			is_include_next,
		a_boolean			is_implicit_include,
		a_boolean			is_preinclude,
		a_boolean			is_embed,
		a_boolean			continue_on_open_failure,
		a_const_char			**full_file_name,
		a_const_char			**display_name,
		FILE				**new_input_file,
		a_boolean			*suppress_include,
		a_unicode_source_kind		*unicode_source_kind,
		a_directory_name_entry_ptr	*dir_entry);

extern a_const_char *resolve_header(a_const_char *filename,
                                    a_boolean    is_system_include,
                                    a_boolean    is_include_next,
                                    a_boolean    is_embed,
                                    a_boolean    suppress_diagnostics);

extern a_const_char *resolve_header_in_map(a_const_char *filename,
                                           a_const_char *resolved_header,
                                           a_boolean    is_system_include);

extern a_boolean header_can_be_found(a_const_char *filename,
                                     a_boolean    is_system_include,
                                     a_boolean    is_include_next);

extern void push_input_stack(
			FILE			    *new_input_file,
                        a_const_char		    *name_as_written,
                        a_const_char		    *display_name,
                        a_const_char		    *full_file_name,
			a_boolean                   is_include_file,
			a_boolean                   is_system_include,
                        a_boolean                   is_preinclude,
			a_boolean		    preinclude_macros,
                        a_boolean                   is_implicit_include,
                        a_unicode_source_kind       unicode_source_kind,
                        a_directory_name_entry_ptr  dir_entry,
			an_include_file_history_ptr ifhp);

extern void push_cloned_input_stack_entry(void);

extern a_source_file_ptr clone_current_input_file(a_seq_number  seq_number,
                                                  a_line_number line_number);

extern void pop_input_stack(void);

extern void pop_cloned_input_stack_entry(void);

extern void ensure_min_curr_source_line_length(sizeof_t  min_len);

extern a_boolean processing_primary_source_file(void);

extern void check_for_generation_of_pch_on_return_to_primary_file(void);

extern void skip_function_body(a_boolean is_constructor);

extern a_boolean cache_function_body(
				a_token_cache		*p_token_cache,
				a_boolean		is_constructor,
				a_boolean		*missing_end,
				a_token_sequence_number	*first_tsn,
				a_token_sequence_number	*last_tsn,
				a_source_position	*start_pos,
				a_source_position	*end_pos);

/* Set the error position to the current token position. */
#define set_err_pos_to_curr_token()                                   \
{ copy_source_position(pos_curr_token, error_position);}

/* Define primitive operations on a_token_set_array. */
#define clear_token_set_array(array) memzero((char *)(array), sizeof(array))
#define incr_token_set_array_element(array, tok) (array)[(int)(tok)]++
#define decr_token_set_array_element(array, tok) (array)[(int)(tok)]--

/* Clear the set of syntax error flush stop tokens. */
#define clear_stop_tokens() \
  clear_token_set_array(curr_stop_token_stack_entry->stop_tokens)
/* Add a token to the set of syntax error flush stop tokens. */
#define add_stop_token(stop_token)                                    \
  incr_token_set_array_element(curr_stop_token_stack_entry->stop_tokens, \
			       stop_token)
/* Remove a token from the set of syntax error flush stop tokens. */
#define remove_stop_token(stop_token)                                 \
  decr_token_set_array_element(curr_stop_token_stack_entry->stop_tokens, \
			       stop_token)
/* Flush to the token that matches an opening token (e.g., parenthesis). */
extern void flush_until_matching_token_full(a_boolean	limit_flush);
extern void flush_until_matching_token(void);
extern void flush_tokens_with_stop_tokens_and_warning_flag(
                                         a_token_set_array  stop_tokens,
                                         a_boolean          suppress_warning);
extern void flush_to_closing_paren(void);
extern void flush_to_end_of_source(a_boolean suppress_warning);
extern void flush_if_or_else_statement(void);
extern void flush_tokens(void);
extern void flush_tokens_without_warning(void);

/*
Flush to the newline at the end of the current preprocessing directive.
End-of-source is also checked for because it can come up in some error
cases.
*/
#define flush_to_newline()                                            \
{ while (curr_token != tok_newline &&                                 \
         curr_token != tok_end_of_source) (void)get_token();}


extern void scan_module_name(a_string *primary_name,
                             a_string *partition_name);

extern void push_stop_token_stack(void);
extern void pop_stop_token_stack(void);
extern void push_lexical_state_stack(void);
extern void pop_lexical_state_stack(void);

extern a_template_arg_ptr scan_unknown_template_arg_list(a_boolean is_nonreal,
                                                         a_boolean *p_err);

extern a_template_ptr scan_template_template_argument(
				a_template_ptr		param_template,
				a_source_position	*err_pos,
                                a_boolean		is_default,
                                a_boolean		dependent_default);

a_template_arg_ptr scan_template_argument_list(
                              a_symbol_ptr              template_sym,
                              a_boolean                 type_constraint,
                              a_boolean                 *any_errors,
                              an_identifier_options_set options,
                              long                      *first_defaulted_arg);

extern a_template_arg_ptr scan_concept_arg_list(a_symbol_ptr template_sym,
                                                a_boolean    type_constraint,
                                                a_boolean    *any_errors);

extern a_template_arg_ptr scan_integer_pack(a_boolean record_operands);

extern void insert_string_into_token_stream(
                                        a_const_char      *string,
                                        a_boolean         insert_after,
                                        a_boolean         p_expand_macros,
                                        a_boolean         suppress_caching,
                                        a_source_position position_for_tokens);

extern void cache_tokens_from_string(
                                 a_const_char            *string,
                                 a_token_cache_ptr       cache,
                                 const a_source_position *position_for_tokens);


/*
Macro producing TRUE if the standard bit-precise integer literal suffixes (like
"Uwb") are accepted in the current mode.
*/
#define standard_bit_precise_literal_suffix_allowed() \
  (bit_precise_int_enabled && !clangcpp_version_is(any_version))

/*
Macro producing TRUE if the Clang extension spelling of bit-precise integer
literal suffixes (like "__uWB") is accepted in the current mode.
*/
#define prefixed_bit_precise_literal_suffix_allowed() \
  (bit_precise_int_enabled && clang_version_is(>=190000))


#if CHECKING
void check_all_stop_token_entries_are_reset(a_token_set_array stop_tokens);
#endif /* CHECKING */

/* Initialize the name linkage constants. */
void init_name_linkage_constants(void);

/* Initialize the lexical routines. */
extern void lexical_pch_reset(void);
extern void lexical_reset(void);
extern void lexical_one_time_init(void);
extern void lexical_trans_unit_init(void);
extern void lexical_init(void);
extern void lexical_trans_unit_wrapup(void);
#if MAKE_FRONT_END_CALLABLE
extern void lexical_cleanup(void);
#endif /* MAKE_FRONT_END_CALLABLE */

/* Flush until the tok_end_of_source terminating a token cache is found, but
   do not go past it. */
#define flush_to_token_cache_terminator()			\
  {								\
    while (curr_token != tok_end_of_source) (void)get_token();	\
  }

/* Flush until the tok_end_of_source terminating a token cache is found and
   fetch the next token. */
#define flush_past_token_cache_terminator()			\
  {								\
    flush_to_token_cache_terminator()				\
    /* Advance past the end-of-source token. */			\
    (void)get_token();						\
  }

extern a_string token_to_string(
                             const a_shared_token &token,
                             a_boolean            expand_pseudo_tokens = TRUE);

extern
void add_cached_tokens_to_string(a_const_token_cache_iterator begin,
                                 a_const_token_cache_iterator end,
                                 a_token_sequence_number      start_tsn,
                                 a_token_sequence_number      end_tsn);

extern
void add_token_cache_segment_to_string(const a_token_cache      *cache,
                                       a_token_sequence_number  start_tsn,
                                       a_token_sequence_number  end_tsn);

extern void add_token_cache_to_string(const a_token_cache *cache);

extern void init_token_string(
                         const a_source_position *pos,
                         a_boolean               keep_spacing,
                         a_boolean               suppress_identifier_wrapping);

extern a_const_char *token_string(void);

extern char *make_copy_of_token_string(void);

extern a_const_char *il_string_for_curr_token(void);

extern a_preinclude_file_ptr alloc_preinclude_file(void);

#if GET_DEFINITION_OF_CLASS_NEEDED
extern void get_definition_of_class(a_type_ptr	class_type);
#endif /* GET_DEFINITION_OF_CLASS_NEEDED */

#if DEFAULT_RECORD_FORM_OF_NAME_REFERENCE
#if CREATE_LEXICAL_TYPEREFS
extern a_type_ptr make_typeref_with_lexical_information(
                                              a_type_ptr        tp,
                                              a_symbol_locator  *locator);
extern a_type_ptr make_typeref_with_name_qualifier(
                               a_type_ptr            tp,
                               a_name_qualifier_ptr  name_qualifier,
                               a_boolean             is_global_qualified_name);
#else /* !CREATE_LEXICAL_TYPEREFS */
#define make_typeref_with_lexical_information(tp, locator) (tp)
#define make_typeref_with_name_qualifier(tp, qual, global) (tp)
#endif /* CREATE_LEXICAL_TYPEREFS */
extern a_hash_value hash_type_and_name_qualifier(a_void_ptr     key);
extern a_boolean compare_type_and_name_qualifier(a_void_ptr     entry,
                                                 a_void_ptr     key);
extern a_hash_value hash_type_and_template_arg_list(a_void_ptr  key);
extern a_boolean compare_type_and_template_arg_list(a_void_ptr  entry,
                                                    a_void_ptr  key);
#endif /* DEFAULT_RECORD_FORM_OF_NAME_REFERENCE */

extern a_hash_value hash_name_reference(a_void_ptr	key);
extern a_boolean compare_name_reference(a_void_ptr	entry,
					a_void_ptr	key);
extern a_name_reference_ptr make_name_reference(
					a_symbol_locator	*locator,
					a_source_correspondence	*scp);
extern void make_name_reference_from_locator(
				      a_symbol_locator		*locator,
				      a_name_reference_ptr	nrp);
extern a_name_reference_ptr find_allocated_name_reference(
				a_source_correspondence		*scp,
				a_name_reference_ptr		entry_to_copy);
extern void db_name_qualifier(a_name_qualifier_ptr	nqp);
extern void db_name_reference(a_name_reference_ptr	nrp);

/*
Do not create name references for template parameters; the name cannot be
qualified, and such a name reference could cause problems if it were carried
over to the substituted value in a generated instance of the template.
*/
#define name_reference_needed_for_locator(locator)			\
  (!(locator)->is_template_param)

/*
Convenience macro to avoid calling make_name_reference for entities that are
known not to have a qualified name.  This includes all entities in C mode,
and all entities declared in functions.
*/
#define qualifiable_name_reference(loc, scp)                                  \
  ((C_mode() || !in_file_scope(scp) ||                                        \
    !name_reference_needed_for_locator((loc)))                                \
                                     ? (a_name_reference_ptr)NULL             \
                                     : make_name_reference((loc), (scp)))

#if DEBUG
/* GCOVR_EXCL_START */
/* Show space used in the lexical routines, for debugging purposes. */
extern unsigned long show_lexical_space_used(void);

extern a_shared_token get_cache_token(a_token_cache_ptr       cache,
                                      a_token_sequence_number seq_number);

extern void db_rescan_stack(void);

extern void db_token_cache(const a_token_cache *cache,
                           a_const_char        *cache_name);

extern void db_diff_token_caches(const a_token_cache *cache_a,
                                 const a_token_cache *cache_b);

extern void db_source_position(const a_source_position *pos);

extern void db_tokens(const a_token_cache *cache);
extern void db_tokens(const a_shared_token_cache &cache);
extern void db_tokens(const a_reusable_token_cache &cache);
extern void db_tokens(a_token_cache_iterator it);
extern void db_tokens(a_const_token_cache_iterator it);

extern void db_stop_tokens(void);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

/* Test whether or not a given character location falls within
   curr_source_line. */
/* Watch out -- the argument is evaluated more than once. */
#define within_curr_source_line(line_loc) \
  ptr_in_range((line_loc), curr_source_line, after_end_of_curr_source_line)

/*
loc_in_line points to an attention marker indicating the start of
an insertion.  Update loc_in_line to point to the first character in the
insertion.  If the insertion is really a deletion, update loc_in_line
to after the deleted character.  slmp is set to point to the source
line modification for the insertion.
If you change this, also change walk_into_insertion.
*/
#define go_into_insertion(slmp, loc_in_line)                          \
{ (slmp) = nested_source_line_modif(loc_in_line);                     \
  if ((slmp)->inserted_text != (slmp)->end_inserted_text) {           \
    /* Text replacement; continue with the text in the expansion. */  \
    (loc_in_line) = (slmp)->inserted_text;                            \
  } else {                                                            \
    /* Text deletion; skip over the deleted text. */                  \
    (loc_in_line) += (slmp)->num_chars_to_delete;                     \
  }  /* if */                                                         \
}  /* go_into_insertion */

/*
Similar to go_into_insertion, but for cases where one maintains a pointer
to the modification entry for the current position, as when one is
walking the entire line modification tree.  slmp on entry points to the
current modification entry, and it is updated on exit.  loc_in_line points
to an attention marker indicating the start of an insertion.  loc_in_line
is updated to point to the first character in the insertion.  If the
insertion is really a deletion, loc_in_line is updated to after the
deleted character.  ins_slmp is set to point to the source line
modification for the insertion.  The parent pointer in the insertion
entry is set, so exiting from the insertion will be quick.
If you change this, also change go_into_insertion.
*/
#define walk_into_insertion(slmp, ins_slmp, loc_in_line)              \
{ (ins_slmp) = nested_source_line_modif(loc_in_line);                 \
  set_parent_modif(ins_slmp, slmp);                                   \
  if ((ins_slmp)->inserted_text != (ins_slmp)->end_inserted_text) {   \
    /* Text replacement; continue with the text in the expansion. */  \
    (loc_in_line) = (ins_slmp)->inserted_text;                        \
    (slmp) = (ins_slmp);                                              \
  } else {                                                            \
    /* Text deletion; skip over the deleted text. */                  \
    (loc_in_line) += (ins_slmp)->num_chars_to_delete;                 \
  }  /* if */                                                         \
}  /* walk_into_insertion */

/*
slmp points to a source modification.  Set loc_in_line to the character
location that follows the characters replaced by that modification.
*/
#define leave_insertion(slmp, loc_in_line)                            \
{ (loc_in_line) = loc_of_insert(slmp) + (slmp)->num_chars_to_delete;  \
}  /* leave_insertion */

/*
slmp points to a source modification.  Set loc_in_line to the character
location that follows the characters replaced by that modification.  Update
slmp to point to the modification associated with loc_in_line (i.e.,
the parent of the original slmp).
*/
#define walk_out_of_insertion(slmp, loc_in_line)                      \
{ leave_insertion(slmp, loc_in_line)                                  \
  slmp = parent_source_line_modif(slmp);                              \
}  /* walk_out_of_insertion */

/*
Determine the insert location for a source line modification.  This is
tricky for an entry that is inserted in front of the first character
of curr_source_line, as indicated by line_loc == NULL.  Normally, the
location in that case will be the first character of curr_source_line.
However, when this occurs at the end of a file (at_end_of_source_file ==
TRUE), the previous contents of the line will not yet have been replaced
but have already been scanned.  In this case, we treat the insertion as
having appeared just before the terminating LE_END_OF_LINE to avoid
processing the now-defunct line a second time.
*/
#define loc_of_insert(slmp)                                                \
  ((slmp)->line_loc != NULL ? (slmp)->line_loc :                           \
   at_end_of_source_file ? curr_source_line + end_of_line_escape_offset    \
                         : curr_source_line)

/*
Set loc_in_line to point to the first character of the current source
line.  This is tricky when there is an insert at the start of the line.
Also set slmp for use in scanning the line with the "walk_.." macros.
*/
#define set_up_for_walk_of_source_line(loc_in_line, slmp)             \
{ if (line_start_source_line_modif != NULL) {                         \
    slmp = line_start_source_line_modif;                              \
    loc_in_line = slmp->inserted_text;                                \
  } else {                                                            \
    slmp = NULL;                                                      \
    loc_in_line = curr_source_line;                                   \
  }  /* if */                                                         \
}  /* set_up_for_walk_of_source_line */


inline unsigned char hexvalue(unsigned char ch)
/*
Convert a character hex digit to the associated hex digit value.
*/
{
  if (isdigit(ch)) {
    ch -= '0';
  } else if (islower(ch)) {
    ch -= 'a'-0xa;
  } else {
    ch -= 'A'-0xA;
  }  /* if */
  return ch;
}  /* hexvalue */


#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
/*
Macro used in locations where a source sequence entry can be created for
an __if_exists directive.
*/
#define check_for_if_exists_pragmas()					\
  if (!curr_token_pragmas->is_empty()) f_check_for_if_exists_pragmas()
/*
Macro used to determine whether a source sequence entry should be created
for a given __if_exists directive.
*/
#define generate_microsoft_if_exists_entries()				\
  (create_microsoft_if_exists_entries &&				\
   prototype_instantiations_in_il &&					\
   depth_scope_stack != NO_SCOPE_DEPTH &&				\
   scope_stack[depth_scope_stack].create_ms_if_exists_entries)
#else /* !GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
#define check_for_if_exists_pragmas() /* nothing */
#endif /* !GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */

#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
extern void if_exists_pragma(a_pending_pragma_ptr	ppp);
extern void f_check_for_if_exists_pragmas(void);
extern void check_for_unclosed_if_exists_blocks(void);
#endif /* !GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */

#if MICROSOFT_EXTENSIONS_ALLOWED
/*
Return TRUE if tok is one of the Microsoft __xPREFIX string operators.
*/
#define is_microsoft_string_prefix_operator(tok)                       \
  ((tok) == tok_microsoft_Lprefix || (tok) == tok_microsoft_lprefix || \
   (tok) == tok_microsoft_Uprefix || (tok) == tok_microsoft_uprefix)
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
/*lint -emacro(506,is_microsoft_string_prefix_operator)*/
#define is_microsoft_string_prefix_operator(tok) FALSE
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if MICROSOFT_EXTENSIONS_ALLOWED
extern void init_whitespace_keywords(void);

extern char *generate_top_level_metadata_code(an_assembly_index index);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_hash_value hash_include_file_history(a_void_ptr	key);
extern a_boolean compare_include_file_history(a_void_ptr	entry,
                                              a_void_ptr	key);
extern a_hash_value hash_include_search_result(a_void_ptr	key);
extern a_boolean compare_include_search_result(a_void_ptr	entry,
                                               a_void_ptr	key);

#if UNIQUE_FILE_IDENTIFIER_AVAILABLE
extern a_hash_value hash_unique_file_id_for_table(a_void_ptr	key);

extern a_boolean compare_unique_file_id(a_void_ptr	entry,
                                        a_void_ptr	key);
#endif /* UNIQUE_FILE_IDENTIFIER_AVAILABLE */

extern a_token_kind scan_string_literal(
                                       a_string_or_char_literal_kind lit_kind);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_partial_class_body_ptr cache_partial_class_body(
                                                       a_type_ptr class_type);

extern void replace_curr_token(a_token_kind  new_token);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if UNICODE_VULNERABILITY_DETECTION_SUPPORTED

/*
The following table is adapted from the confusables.txt data described in
unicode.org/reports/tr39, "Unicode Security Mechanisms", section 4.  It
associates various Unicode code points with their "prototypes".  It must be
sorted by src_char so that it can be searched by a binary search.  The current
contents reflect version 17.0.0 (2025-07-22) of the data.
*/

#define MAX_PROTOTYPE_LENGTH 18
			/* The maximum number of code points in a prototype. */

typedef const struct a_confusable_map_elem *a_confusable_map_elem_ptr;
struct a_confusable_map_elem {
  int	src_char;	/* The numeric value of a Unicode code point whose
			   glyph could be confused with that of another
			   code point. */
  int	prototype[MAX_PROTOTYPE_LENGTH];
			/* The numeric values of the Unicode code points of
			   the prototype character(s) for src_char, as
			   defined in unicode.org/reports/tr39, "Unicode
			   Security Mechanisms", section 4. */
};  /* a_confusable_map_elem */

extern const a_confusable_map_elem confusable_map[]
#if VAR_INITIALIZERS
  = {
      { 0x00022, { 0x00027, 0x00027 } },
      { 0x00025, { 0x000BA, 0x0002F, 0x02080 } },
      { 0x00030, { 0x0004F } },
      { 0x00031, { 0x0006C } },
      { 0x00049, { 0x0006C } },
      { 0x00060, { 0x00027 } },
      { 0x0006D, { 0x00072, 0x0006E } },
      { 0x0007C, { 0x0006C } },
      { 0x000A0, { 0x00020 } },
      { 0x000A2, { 0x00063, 0x00338 } },
      { 0x000A5, { 0x00059, 0x00335 } },
      { 0x000AF, { 0x002C9 } },
      { 0x000B4, { 0x00027 } },
      { 0x000B5, { 0x003BC } },
      { 0x000B8, { 0x0002C } },
      { 0x000C6, { 0x00041, 0x00045 } },
      { 0x000C7, { 0x00043, 0x00326 } },
      { 0x000D0, { 0x00044, 0x00335 } },
      { 0x000D7, { 0x00078 } },
      { 0x000D8, { 0x0004F, 0x00338 } },
      { 0x000E6, { 0x00061, 0x00065 } },
      { 0x000E7, { 0x00063, 0x00326 } },
      { 0x000F0, { 0x02202, 0x00335 } },
      { 0x000F6, { 0x00629 } },
      { 0x000F8, { 0x0006F, 0x00338 } },
      { 0x000FE, { 0x00070 } },
      { 0x00110, { 0x00044, 0x00335 } },
      { 0x00111, { 0x00064, 0x00335 } },
      { 0x0011A, { 0x00114 } },
      { 0x0011B, { 0x00115 } },
      { 0x00126, { 0x00048, 0x00335 } },
      { 0x00127, { 0x00068, 0x00335 } },
      { 0x00131, { 0x00069 } },
      { 0x00132, { 0x0006C, 0x0004A } },
      { 0x00133, { 0x00069, 0x0006A } },
      { 0x0013F, { 0x0006C, 0x000B7 } },
      { 0x00140, { 0x0006C, 0x000B7 } },
      { 0x00141, { 0x0004C, 0x00338 } },
      { 0x00142, { 0x0006C, 0x00338 } },
      { 0x00146, { 0x00272 } },
      { 0x00149, { 0x00027, 0x0006E } },
      { 0x0014B, { 0x0006E, 0x00329 } },
      { 0x00150, { 0x000D6 } },
      { 0x00152, { 0x0004F, 0x00045 } },
      { 0x00153, { 0x0006F, 0x00065 } },
      { 0x00163, { 0x001AB } },
      { 0x00166, { 0x00054, 0x00335 } },
      { 0x00167, { 0x00074, 0x00335 } },
      { 0x0017F, { 0x00066 } },
      { 0x00180, { 0x00062, 0x00335 } },
      { 0x00181, { 0x00027, 0x00042 } },
      { 0x00182, { 0x00062, 0x00304 } },
      { 0x00183, { 0x00062, 0x00304 } },
      { 0x00184, { 0x00062 } },
      { 0x00187, { 0x00043, 0x00027 } },
      { 0x00189, { 0x00044, 0x00335 } },
      { 0x0018A, { 0x00027, 0x00044 } },
      { 0x0018C, { 0x00064, 0x00304 } },
      { 0x0018D, { 0x00067 } },
      { 0x00191, { 0x00046, 0x00326 } },
      { 0x00192, { 0x00066 } },
      { 0x00193, { 0x00047, 0x00027 } },
      { 0x00196, { 0x0006C } },
      { 0x00197, { 0x0006C, 0x00335 } },
      { 0x00198, { 0x0004B, 0x00027 } },
      { 0x00199, { 0x0006B, 0x00314 } },
      { 0x0019A, { 0x0006C, 0x00335 } },
      { 0x0019B, { 0x003BB, 0x00338 } },
      { 0x0019D, { 0x0004E, 0x00326 } },
      { 0x0019E, { 0x0006E, 0x00329 } },
      { 0x0019F, { 0x0004F, 0x00335 } },
      { 0x001A0, { 0x0004F, 0x00027 } },
      { 0x001A1, { 0x0006F, 0x00027 } },
      { 0x001A4, { 0x00027, 0x00050 } },
      { 0x001A5, { 0x00070, 0x00314 } },
      { 0x001A6, { 0x00052 } },
      { 0x001A7, { 0x00032 } },
      { 0x001AC, { 0x00027, 0x00054 } },
      { 0x001AD, { 0x00074, 0x00314 } },
      { 0x001AE, { 0x00054, 0x00328 } },
      { 0x001B3, { 0x00027, 0x00059 } },
      { 0x001B4, { 0x00079, 0x00314 } },
      { 0x001B5, { 0x0005A, 0x00335 } },
      { 0x001B6, { 0x0007A, 0x00335 } },
      { 0x001B7, { 0x00033 } },
      { 0x001BB, { 0x00032, 0x00335 } },
      { 0x001BC, { 0x00035 } },
      { 0x001BD, { 0x00073 } },
      { 0x001BF, { 0x00070 } },
      { 0x001C0, { 0x0006C } },
      { 0x001C1, { 0x0006C, 0x0006C } },
      { 0x001C3, { 0x00021 } },
      { 0x001C4, { 0x00044, 0x0017D } },
      { 0x001C5, { 0x00044, 0x0017E } },
      { 0x001C6, { 0x00064, 0x0017E } },
      { 0x001C7, { 0x0004C, 0x0004A } },
      { 0x001C8, { 0x0004C, 0x0006A } },
      { 0x001C9, { 0x0006C, 0x0006A } },
      { 0x001CA, { 0x0004E, 0x0004A } },
      { 0x001CB, { 0x0004E, 0x0006A } },
      { 0x001CC, { 0x0006E, 0x0006A } },
      { 0x001CD, { 0x00102 } },
      { 0x001CE, { 0x00103 } },
      { 0x001CF, { 0x0012C } },
      { 0x001D0, { 0x0012D } },
      { 0x001D1, { 0x0014E } },
      { 0x001D2, { 0x0014F } },
      { 0x001D3, { 0x0016C } },
      { 0x001D4, { 0x0016D } },
      { 0x001E4, { 0x00047, 0x00335 } },
      { 0x001E5, { 0x00067, 0x00335 } },
      { 0x001E6, { 0x0011E } },
      { 0x001E7, { 0x0011F } },
      { 0x001F1, { 0x00044, 0x0005A } },
      { 0x001F2, { 0x00044, 0x0007A } },
      { 0x001F3, { 0x00064, 0x0007A } },
      { 0x001F5, { 0x00123 } },
      { 0x001FE, { 0x0004F, 0x00338, 0x00301 } },
      { 0x0021A, { 0x00162 } },
      { 0x0021B, { 0x001AB } },
      { 0x0021C, { 0x00033 } },
      { 0x00222, { 0x00038 } },
      { 0x00223, { 0x00038 } },
      { 0x00224, { 0x0005A, 0x00326 } },
      { 0x00225, { 0x0007A, 0x00326 } },
      { 0x00226, { 0x000C5 } },
      { 0x00227, { 0x000E5 } },
      { 0x0023C, { 0x00063, 0x00338 } },
      { 0x0023E, { 0x00054, 0x00338 } },
      { 0x00241, { 0x0003F } },
      { 0x00244, { 0x00055, 0x00335 } },
      { 0x00246, { 0x00045, 0x00338 } },
      { 0x00247, { 0x00065, 0x00338 } },
      { 0x00248, { 0x0004A, 0x00335 } },
      { 0x00249, { 0x0006A, 0x00335 } },
      { 0x0024D, { 0x00072, 0x00335 } },
      { 0x0024E, { 0x00059, 0x00335 } },
      { 0x0024F, { 0x00079, 0x00335 } },
      { 0x00251, { 0x00061 } },
      { 0x00253, { 0x00062, 0x00314 } },
      { 0x00256, { 0x00064, 0x00328 } },
      { 0x00257, { 0x00064, 0x00314 } },
      { 0x00259, { 0x001DD } },
      { 0x0025A, { 0x001DD, 0x002DE } },
      { 0x0025B, { 0x0A793 } },
      { 0x00260, { 0x00067, 0x00314 } },
      { 0x00261, { 0x00067 } },
      { 0x00263, { 0x00079 } },
      { 0x00266, { 0x00068, 0x00314 } },
      { 0x00268, { 0x00069, 0x00335 } },
      { 0x00269, { 0x00069 } },
      { 0x0026A, { 0x00069 } },
      { 0x0026B, { 0x0006C, 0x00334 } },
      { 0x0026D, { 0x0006C, 0x00328 } },
      { 0x0026E, { 0x0006C, 0x0021D } },
      { 0x0026F, { 0x00077 } },
      { 0x00271, { 0x00072, 0x0006E, 0x00326 } },
      { 0x00273, { 0x0006E, 0x00328 } },
      { 0x00275, { 0x0006F, 0x00335 } },
      { 0x00276, { 0x0006F, 0x01D07 } },
      { 0x0027C, { 0x00072, 0x00329 } },
      { 0x0027D, { 0x00072, 0x00328 } },
      { 0x00282, { 0x00073, 0x00328 } },
      { 0x0028B, { 0x00075 } },
      { 0x0028F, { 0x00079 } },
      { 0x00290, { 0x0007A, 0x00328 } },
      { 0x00292, { 0x0021D } },
      { 0x00294, { 0x0003F } },
      { 0x00295, { 0x0A7CE } },
      { 0x002A0, { 0x00071, 0x00314 } },
      { 0x002A3, { 0x00064, 0x0007A } },
      { 0x002A4, { 0x00064, 0x0021D } },
      { 0x002A5, { 0x00064, 0x00291 } },
      { 0x002A6, { 0x00074, 0x00073 } },
      { 0x002A7, { 0x00074, 0x00283 } },
      { 0x002A8, { 0x00074, 0x00255 } },
      { 0x002A9, { 0x00066, 0x0006E, 0x00329 } },
      { 0x002AA, { 0x0006C, 0x00073 } },
      { 0x002AB, { 0x0006C, 0x0007A } },
      { 0x002B3, { 0x018F4 } },
      { 0x002B9, { 0x00027 } },
      { 0x002BA, { 0x00027, 0x00027 } },
      { 0x002BB, { 0x00027 } },
      { 0x002BC, { 0x00027 } },
      { 0x002BD, { 0x00027 } },
      { 0x002BE, { 0x00027 } },
      { 0x002BF, { 0x00559 } },
      { 0x002C2, { 0x0003C } },
      { 0x002C3, { 0x0003E } },
      { 0x002C4, { 0x0005E } },
      { 0x002C6, { 0x0005E } },
      { 0x002C8, { 0x00027 } },
      { 0x002CA, { 0x00027 } },
      { 0x002CB, { 0x00027 } },
      { 0x002D0, { 0x0003A } },
      { 0x002D3, { 0x00559 } },
      { 0x002D7, { 0x0002D } },
      { 0x002D8, { 0x002C7 } },
      { 0x002D9, { 0x00971 } },
      { 0x002DA, { 0x000B0 } },
      { 0x002DB, { 0x00069 } },
      { 0x002DC, { 0x0007E } },
      { 0x002DD, { 0x00027, 0x00027 } },
      { 0x002E1, { 0x018F3 } },
      { 0x002E2, { 0x018F5 } },
      { 0x002E4, { 0x002C1 } },
      { 0x002EE, { 0x00027, 0x00027 } },
      { 0x002F4, { 0x00027 } },
      { 0x002F6, { 0x00027, 0x00027 } },
      { 0x002F8, { 0x0003A } },
      { 0x002FB, { 0x002EA } },
      { 0x00305, { 0x00304 } },
      { 0x0030C, { 0x00306 } },
      { 0x0030D, { 0x00670 } },
      { 0x00310, { 0x00306, 0x00307 } },
      { 0x00311, { 0x00302 } },
      { 0x00315, { 0x00313 } },
      { 0x00317, { 0x00650 } },
      { 0x0031A, { 0x01AE9 } },
      { 0x00320, { 0x00331 } },
      { 0x00321, { 0x00326 } },
      { 0x00322, { 0x00328 } },
      { 0x00327, { 0x00326 } },
      { 0x00336, { 0x00335 } },
      { 0x00337, { 0x00338 } },
      { 0x00339, { 0x00326 } },
      { 0x00340, { 0x00300 } },
      { 0x00341, { 0x00301 } },
      { 0x00342, { 0x00303 } },
      { 0x00343, { 0x00313 } },
      { 0x00345, { 0x00328 } },
      { 0x00347, { 0x00333 } },
      { 0x00348, { 0x10EFA } },
      { 0x00357, { 0x00350 } },
      { 0x00358, { 0x00307 } },
      { 0x00366, { 0x0030A } },
      { 0x0036E, { 0x00306 } },
      { 0x00370, { 0x02C75 } },
      { 0x00374, { 0x00027 } },
      { 0x00375, { 0x002CF } },
      { 0x00376, { 0x00418 } },
      { 0x00377, { 0x01D0E } },
      { 0x0037A, { 0x00069 } },
      { 0x0037B, { 0x00254 } },
      { 0x0037D, { 0x0A73F } },
      { 0x0037E, { 0x0003B } },
      { 0x0037F, { 0x0004A } },
      { 0x00384, { 0x00027 } },
      { 0x00387, { 0x000B7 } },
      { 0x00391, { 0x00041 } },
      { 0x00392, { 0x00042 } },
      { 0x00395, { 0x00045 } },
      { 0x00396, { 0x0005A } },
      { 0x00397, { 0x00048 } },
      { 0x00398, { 0x0004F, 0x00335 } },
      { 0x00399, { 0x0006C } },
      { 0x0039A, { 0x0004B } },
      { 0x0039B, { 0x00245 } },
      { 0x0039C, { 0x0004D } },
      { 0x0039D, { 0x0004E } },
      { 0x0039F, { 0x0004F } },
      { 0x003A1, { 0x00050 } },
      { 0x003A3, { 0x001A9 } },
      { 0x003A4, { 0x00054 } },
      { 0x003A5, { 0x00059 } },
      { 0x003A7, { 0x00058 } },
      { 0x003B1, { 0x00061 } },
      { 0x003B2, { 0x000DF } },
      { 0x003B3, { 0x00079 } },
      { 0x003B4, { 0x01E9F } },
      { 0x003B5, { 0x0A793 } },
      { 0x003B7, { 0x0006E, 0x00329 } },
      { 0x003B8, { 0x0004F, 0x00335 } },
      { 0x003B9, { 0x00069 } },
      { 0x003BA, { 0x00138 } },
      { 0x003BD, { 0x00076 } },
      { 0x003BF, { 0x0006F } },
      { 0x003C1, { 0x00070 } },
      { 0x003C3, { 0x0006F } },
      { 0x003C4, { 0x01D1B } },
      { 0x003C5, { 0x00075 } },
      { 0x003C6, { 0x00278 } },
      { 0x003D0, { 0x000DF } },
      { 0x003D1, { 0x0004F, 0x00335 } },
      { 0x003D2, { 0x00059 } },
      { 0x003D5, { 0x00278 } },
      { 0x003D6, { 0x003C0 } },
      { 0x003DB, { 0x003C2 } },
      { 0x003DC, { 0x00046 } },
      { 0x003E4, { 0x00427 } },
      { 0x003E5, { 0x00447 } },
      { 0x003E8, { 0x00032 } },
      { 0x003E9, { 0x001A8 } },
      { 0x003EC, { 0x00036 } },
      { 0x003ED, { 0x0006F } },
      { 0x003F0, { 0x00138 } },
      { 0x003F1, { 0x00070 } },
      { 0x003F2, { 0x00063 } },
      { 0x003F3, { 0x0006A } },
      { 0x003F4, { 0x0004F, 0x00335 } },
      { 0x003F5, { 0x0A793 } },
      { 0x003F7, { 0x000DE } },
      { 0x003F8, { 0x00070 } },
      { 0x003F9, { 0x00043 } },
      { 0x003FA, { 0x0004D } },
      { 0x003FD, { 0x00186 } },
      { 0x003FF, { 0x0A73E } },
      { 0x00404, { 0x0A792 } },
      { 0x00405, { 0x00053 } },
      { 0x00406, { 0x0006C } },
      { 0x00408, { 0x0004A } },
      { 0x00410, { 0x00041 } },
      { 0x00411, { 0x00062, 0x00304 } },
      { 0x00412, { 0x00042 } },
      { 0x00413, { 0x00393 } },
      { 0x00415, { 0x00045 } },
      { 0x00417, { 0x00033 } },
      { 0x00419, { 0x0040D } },
      { 0x0041A, { 0x0004B } },
      { 0x0041B, { 0x00245 } },
      { 0x0041C, { 0x0004D } },
      { 0x0041D, { 0x00048 } },
      { 0x0041E, { 0x0004F } },
      { 0x0041F, { 0x003A0 } },
      { 0x00420, { 0x00050 } },
      { 0x00421, { 0x00043 } },
      { 0x00422, { 0x00054 } },
      { 0x00423, { 0x00059 } },
      { 0x00424, { 0x003A6 } },
      { 0x00425, { 0x00058 } },
      { 0x0042B, { 0x00062, 0x0006C } },
      { 0x0042C, { 0x00062 } },
      { 0x0042E, { 0x0006C, 0x0004F } },
      { 0x00430, { 0x00061 } },
      { 0x00431, { 0x00036 } },
      { 0x00432, { 0x00299 } },
      { 0x00433, { 0x00072 } },
      { 0x00435, { 0x00065 } },
      { 0x00437, { 0x0025C } },
      { 0x00438, { 0x01D0E } },
      { 0x0043A, { 0x00138 } },
      { 0x0043C, { 0x0028D } },
      { 0x0043D, { 0x0029C } },
      { 0x0043E, { 0x0006F } },
      { 0x0043F, { 0x003C0 } },
      { 0x00440, { 0x00070 } },
      { 0x00441, { 0x00063 } },
      { 0x00442, { 0x01D1B } },
      { 0x00443, { 0x00079 } },
      { 0x00444, { 0x00278 } },
      { 0x00445, { 0x00078 } },
      { 0x00448, { 0x00077 } },
      { 0x0044A, { 0x002C9, 0x00062 } },
      { 0x0044B, { 0x00185, 0x00069 } },
      { 0x0044C, { 0x00185 } },
      { 0x0044F, { 0x01D19 } },
      { 0x00454, { 0x0A793 } },
      { 0x00455, { 0x00073 } },
      { 0x00456, { 0x00069 } },
      { 0x00458, { 0x0006A } },
      { 0x0045B, { 0x00068, 0x00335 } },
      { 0x0045D, { 0x00439 } },
      { 0x0045F, { 0x00075, 0x00329 } },
      { 0x00461, { 0x00077 } },
      { 0x00462, { 0x00062, 0x00335 } },
      { 0x00463, { 0x00062, 0x00335 } },
      { 0x00470, { 0x003A8 } },
      { 0x00471, { 0x003C8 } },
      { 0x00472, { 0x0004F, 0x00335 } },
      { 0x00473, { 0x0006F, 0x00335 } },
      { 0x00474, { 0x00056 } },
      { 0x00475, { 0x00076 } },
      { 0x0047C, { 0x00460, 0x00486, 0x00487 } },
      { 0x0047D, { 0x00077, 0x00486, 0x00487 } },
      { 0x0048A, { 0x0040D, 0x00326 } },
      { 0x0048B, { 0x00439, 0x00326 } },
      { 0x0048C, { 0x00062, 0x00335 } },
      { 0x0048D, { 0x00062, 0x00335 } },
      { 0x00490, { 0x00393, 0x00027 } },
      { 0x00491, { 0x00072, 0x00027 } },
      { 0x00492, { 0x00393, 0x00335 } },
      { 0x00493, { 0x00072, 0x00335 } },
      { 0x00496, { 0x00416, 0x00329 } },
      { 0x00497, { 0x00436, 0x00329 } },
      { 0x00498, { 0x00033, 0x00326 } },
      { 0x00499, { 0x0025C, 0x00326 } },
      { 0x0049A, { 0x0004B, 0x00329 } },
      { 0x0049B, { 0x00138, 0x00329 } },
      { 0x0049E, { 0x0004B, 0x00335 } },
      { 0x0049F, { 0x00138, 0x00335 } },
      { 0x004A2, { 0x00048, 0x00329 } },
      { 0x004A3, { 0x0029C, 0x00329 } },
      { 0x004AA, { 0x00043, 0x00326 } },
      { 0x004AB, { 0x00063, 0x00326 } },
      { 0x004AC, { 0x00054, 0x00329 } },
      { 0x004AD, { 0x01D1B, 0x00329 } },
      { 0x004AE, { 0x00059 } },
      { 0x004AF, { 0x00079 } },
      { 0x004B0, { 0x00059, 0x00335 } },
      { 0x004B1, { 0x00079, 0x00335 } },
      { 0x004B2, { 0x00058, 0x00329 } },
      { 0x004BB, { 0x00068 } },
      { 0x004BD, { 0x00065 } },
      { 0x004BE, { 0x004BC, 0x00328 } },
      { 0x004BF, { 0x00065, 0x00328 } },
      { 0x004C0, { 0x0006C } },
      { 0x004C5, { 0x00245, 0x00326 } },
      { 0x004C6, { 0x0043B, 0x00326 } },
      { 0x004C7, { 0x00048, 0x00326 } },
      { 0x004C8, { 0x0029C, 0x00326 } },
      { 0x004C9, { 0x00048, 0x00326 } },
      { 0x004CA, { 0x0029C, 0x00326 } },
      { 0x004CB, { 0x004B6 } },
      { 0x004CC, { 0x004B7 } },
      { 0x004CD, { 0x0004D, 0x00326 } },
      { 0x004CE, { 0x0028D, 0x00326 } },
      { 0x004CF, { 0x0006C } },
      { 0x004D4, { 0x00041, 0x00045 } },
      { 0x004D5, { 0x00061, 0x00065 } },
      { 0x004D8, { 0x0018F } },
      { 0x004D9, { 0x001DD } },
      { 0x004E0, { 0x00033 } },
      { 0x004E1, { 0x0021D } },
      { 0x004E8, { 0x0004F, 0x00335 } },
      { 0x004E9, { 0x0006F, 0x00335 } },
      { 0x00501, { 0x00064 } },
      { 0x0050A, { 0x001F6 } },
      { 0x0050C, { 0x00047 } },
      { 0x0050D, { 0x00262 } },
      { 0x00510, { 0x00190 } },
      { 0x00511, { 0x0A793 } },
      { 0x0051B, { 0x00071 } },
      { 0x0051C, { 0x00057 } },
      { 0x0051D, { 0x00077 } },
      { 0x0053B, { 0x012AE } },
      { 0x00544, { 0x01206 } },
      { 0x0054A, { 0x01323 } },
      { 0x0054C, { 0x01261 } },
      { 0x0054D, { 0x00055 } },
      { 0x0054F, { 0x00053 } },
      { 0x00553, { 0x003A6 } },
      { 0x00555, { 0x0004F } },
      { 0x0055A, { 0x00027 } },
      { 0x0055D, { 0x00027 } },
      { 0x00561, { 0x00077 } },
      { 0x00563, { 0x00071 } },
      { 0x00566, { 0x00071 } },
      { 0x0056E, { 0x01E9F } },
      { 0x00570, { 0x00068 } },
      { 0x00572, { 0x0006E, 0x00329 } },
      { 0x00575, { 0x00237 } },
      { 0x00578, { 0x0006E } },
      { 0x0057A, { 0x00270 } },
      { 0x0057C, { 0x0006E } },
      { 0x0057D, { 0x00075 } },
      { 0x00581, { 0x00067 } },
      { 0x00582, { 0x00069 } },
      { 0x00584, { 0x00066 } },
      { 0x00585, { 0x0006F } },
      { 0x00587, { 0x00565, 0x00069 } },
      { 0x00589, { 0x0003A } },
      { 0x0059C, { 0x00301 } },
      { 0x0059D, { 0x00301 } },
      { 0x005A4, { 0x0059A } },
      { 0x005A8, { 0x00599 } },
      { 0x005AD, { 0x00596 } },
      { 0x005AE, { 0x00598 } },
      { 0x005AF, { 0x0030A } },
      { 0x005B4, { 0x00323 } },
      { 0x005B9, { 0x00307 } },
      { 0x005BA, { 0x00307 } },
      { 0x005C0, { 0x0006C } },
      { 0x005C1, { 0x00307 } },
      { 0x005C2, { 0x00307 } },
      { 0x005C3, { 0x0003A } },
      { 0x005C4, { 0x00307 } },
      { 0x005C5, { 0x00323 } },
      { 0x005D5, { 0x0006C } },
      { 0x005D8, { 0x00076 } },
      { 0x005D9, { 0x00027 } },
      { 0x005DF, { 0x0006C } },
      { 0x005E1, { 0x0006F } },
      { 0x005F0, { 0x0006C, 0x0006C } },
      { 0x005F1, { 0x0006C, 0x00027 } },
      { 0x005F2, { 0x00027, 0x00027 } },
      { 0x005F3, { 0x00027 } },
      { 0x005F4, { 0x00027, 0x00027 } },
      { 0x00609, { 0x000BA, 0x0002F, 0x02080, 0x02080 } },
      { 0x0060A, { 0x000BA, 0x0002F, 0x02080, 0x02080, 0x02080 } },
      { 0x0060D, { 0x0002C } },
      { 0x0060F, { 0x00639 } },
      { 0x00618, { 0x00301 } },
      { 0x00619, { 0x00313 } },
      { 0x0061A, { 0x00650 } },
      { 0x00623, { 0x0006C, 0x00674 } },
      { 0x00624, { 0x00648, 0x00674 } },
      { 0x00625, { 0x0006C, 0x00655 } },
      { 0x00626, { 0x00649, 0x00674 } },
      { 0x00627, { 0x0006C } },
      { 0x0062B, { 0x00649, 0x006DB } },
      { 0x00634, { 0x00633, 0x006DB } },
      { 0x0063D, { 0x00649, 0x00302 } },
      { 0x0063F, { 0x00649, 0x006DB } },
      { 0x00647, { 0x0006F } },
      { 0x0064A, { 0x00649 } },
      { 0x0064B, { 0x0030B } },
      { 0x0064E, { 0x00301 } },
      { 0x0064F, { 0x00313 } },
      { 0x00652, { 0x0030A } },
      { 0x00653, { 0x00303 } },
      { 0x00656, { 0x00329 } },
      { 0x00657, { 0x00312 } },
      { 0x00658, { 0x00306 } },
      { 0x00659, { 0x00304 } },
      { 0x0065A, { 0x00306 } },
      { 0x0065B, { 0x00302 } },
      { 0x0065C, { 0x00323 } },
      { 0x0065D, { 0x00314 } },
      { 0x0065F, { 0x00655 } },
      { 0x00660, { 0x0002E } },
      { 0x00661, { 0x0006C } },
      { 0x00665, { 0x0006F } },
      { 0x00667, { 0x00056 } },
      { 0x00668, { 0x00245 } },
      { 0x0066A, { 0x000BA, 0x0002F, 0x02080 } },
      { 0x0066B, { 0x0002C } },
      { 0x0066C, { 0x0060C } },
      { 0x0066D, { 0x0002A } },
      { 0x0066E, { 0x00649 } },
      { 0x0066F, { 0x006A1 } },
      { 0x00672, { 0x0006C, 0x00674 } },
      { 0x00673, { 0x0006C, 0x00655 } },
      { 0x00675, { 0x0006C, 0x00674 } },
      { 0x00676, { 0x00648, 0x00674 } },
      { 0x00677, { 0x00648, 0x00313, 0x00674 } },
      { 0x00678, { 0x00649, 0x00674 } },
      { 0x00679, { 0x00649, 0x00615 } },
      { 0x0067A, { 0x0062A } },
      { 0x0067E, { 0x00649, 0x006DB } },
      { 0x00681, { 0x0062D, 0x00654 } },
      { 0x00685, { 0x0062D, 0x006DB } },
      { 0x00688, { 0x0062F, 0x00615 } },
      { 0x0068B, { 0x0068A, 0x00615 } },
      { 0x0068E, { 0x0062F, 0x006DB } },
      { 0x0068F, { 0x0062F, 0x006DB } },
      { 0x00691, { 0x00631, 0x00615 } },
      { 0x00692, { 0x00631, 0x00306 } },
      { 0x00698, { 0x00631, 0x006DB } },
      { 0x0069E, { 0x00635, 0x006DB } },
      { 0x0069F, { 0x00637, 0x006DB } },
      { 0x006A4, { 0x006A1, 0x006DB } },
      { 0x006A7, { 0x00641 } },
      { 0x006A8, { 0x006A1, 0x006DB } },
      { 0x006A9, { 0x00643 } },
      { 0x006AA, { 0x00643 } },
      { 0x006AD, { 0x00643, 0x006DB } },
      { 0x006B4, { 0x006AF, 0x006DB } },
      { 0x006B5, { 0x00644, 0x00306 } },
      { 0x006B7, { 0x00644, 0x006DB } },
      { 0x006BA, { 0x00649 } },
      { 0x006BB, { 0x00649, 0x00615 } },
      { 0x006BD, { 0x00649, 0x006DB } },
      { 0x006BE, { 0x0006F } },
      { 0x006C1, { 0x0006F } },
      { 0x006C2, { 0x006C0 } },
      { 0x006C3, { 0x00629 } },
      { 0x006C6, { 0x00648, 0x00306 } },
      { 0x006C7, { 0x00648, 0x00313 } },
      { 0x006C8, { 0x00648, 0x00670 } },
      { 0x006C9, { 0x00648, 0x00302 } },
      { 0x006CB, { 0x00648, 0x006DB } },
      { 0x006CC, { 0x00649 } },
      { 0x006CE, { 0x00649, 0x00306 } },
      { 0x006D0, { 0x0067B } },
      { 0x006D1, { 0x00649, 0x006DB } },
      { 0x006D2, { 0x00649 } },
      { 0x006D4, { 0x0002D } },
      { 0x006D5, { 0x0006F } },
      { 0x006DF, { 0x0030A } },
      { 0x006E8, { 0x00306, 0x00307 } },
      { 0x006EC, { 0x00307 } },
      { 0x006EE, { 0x0062F, 0x00302 } },
      { 0x006EF, { 0x00631, 0x00302 } },
      { 0x006F0, { 0x0002E } },
      { 0x006F1, { 0x0006C } },
      { 0x006F2, { 0x00662 } },
      { 0x006F3, { 0x00663 } },
      { 0x006F4, { 0x00664 } },
      { 0x006F5, { 0x0006F } },
      { 0x006F6, { 0x00666 } },
      { 0x006F7, { 0x00056 } },
      { 0x006F8, { 0x00245 } },
      { 0x006F9, { 0x00669 } },
      { 0x006FD, { 0x00621, 0x10EFA } },
      { 0x006FE, { 0x00645, 0x10EFA } },
      { 0x006FF, { 0x0006F, 0x00302 } },
      { 0x00701, { 0x0002E } },
      { 0x00702, { 0x0002E } },
      { 0x00703, { 0x0003A } },
      { 0x00704, { 0x0003A } },
      { 0x00740, { 0x00307 } },
      { 0x00741, { 0x00307 } },
      { 0x00742, { 0x0073C } },
      { 0x00747, { 0x00301 } },
      { 0x00751, { 0x00628, 0x006DB } },
      { 0x00752, { 0x00649, 0x006DB } },
      { 0x00756, { 0x00649, 0x00306 } },
      { 0x00762, { 0x006AC } },
      { 0x00763, { 0x00643, 0x006DB } },
      { 0x00767, { 0x00754 } },
      { 0x00768, { 0x00646, 0x00615 } },
      { 0x00769, { 0x00646, 0x00306 } },
      { 0x0076C, { 0x00631, 0x00654 } },
      { 0x00771, { 0x00697, 0x00615 } },
      { 0x00772, { 0x0062D, 0x00654 } },
      { 0x0077E, { 0x00633, 0x00302 } },
      { 0x007C0, { 0x0004F } },
      { 0x007CA, { 0x0006C } },
      { 0x007EB, { 0x00304 } },
      { 0x007ED, { 0x00307 } },
      { 0x007EE, { 0x00302 } },
      { 0x007F3, { 0x00308 } },
      { 0x007F4, { 0x00027 } },
      { 0x007F5, { 0x00027 } },
      { 0x007FA, { 0x0005F } },
      { 0x0088F, { 0x00649, 0x0030A } },
      { 0x008A1, { 0x00628, 0x00654 } },
      { 0x008A4, { 0x006A2, 0x006DB } },
      { 0x008A7, { 0x00645, 0x006DB } },
      { 0x008A8, { 0x00649, 0x00654 } },
      { 0x008A9, { 0x00754 } },
      { 0x008AE, { 0x0062F, 0x00324, 0x00323 } },
      { 0x008AF, { 0x00635, 0x00324, 0x00323 } },
      { 0x008B0, { 0x006AF } },
      { 0x008B1, { 0x00648 } },
      { 0x008B2, { 0x00632, 0x00302 } },
      { 0x008B6, { 0x00628, 0x006E2 } },
      { 0x008B7, { 0x00649, 0x006DB, 0x006E2 } },
      { 0x008B9, { 0x00631, 0x00306, 0x00307 } },
      { 0x008BA, { 0x00649, 0x00306, 0x00307 } },
      { 0x008BB, { 0x006A1 } },
      { 0x008BC, { 0x006A1 } },
      { 0x008BD, { 0x00649 } },
      { 0x008BE, { 0x00649, 0x006DB, 0x00306 } },
      { 0x008BF, { 0x0062A, 0x00306 } },
      { 0x008C0, { 0x00649, 0x00615, 0x00306 } },
      { 0x008C1, { 0x00686, 0x00306 } },
      { 0x008C2, { 0x00643, 0x00306 } },
      { 0x008E5, { 0x0064C } },
      { 0x008E8, { 0x0064C } },
      { 0x008EA, { 0x00307 } },
      { 0x008EB, { 0x00308 } },
      { 0x008ED, { 0x00323 } },
      { 0x008EE, { 0x00324 } },
      { 0x008F0, { 0x0030B } },
      { 0x008F1, { 0x0064C } },
      { 0x008F2, { 0x0064D } },
      { 0x008F3, { 0x00313 } },
      { 0x008F8, { 0x00350 } },
      { 0x008F9, { 0x00354 } },
      { 0x008FA, { 0x00355 } },
      { 0x008FF, { 0x00350 } },
      { 0x00900, { 0x00352 } },
      { 0x00901, { 0x00306, 0x00307 } },
      { 0x00902, { 0x00307 } },
      { 0x00903, { 0x0003A } },
      { 0x00904, { 0x00905, 0x00946 } },
      { 0x00906, { 0x00905, 0x0093E } },
      { 0x00908, { 0x00930, 0x0094D, 0x00907 } },
      { 0x0090D, { 0x0090F, 0x00306 } },
      { 0x0090E, { 0x0090F, 0x00946 } },
      { 0x00910, { 0x0090F, 0x11B64 } },
      { 0x00911, { 0x00905, 0x0093E, 0x00306 } },
      { 0x00912, { 0x00905, 0x0093E, 0x00946 } },
      { 0x00913, { 0x00905, 0x0093E, 0x11B64 } },
      { 0x00914, { 0x00905, 0x0093E, 0x00948 } },
      { 0x0093B, { 0x0093E, 0x0093A } },
      { 0x0093C, { 0x00323 } },
      { 0x0093F, { 0x009BF } },
      { 0x00945, { 0x00306 } },
      { 0x00947, { 0x11B64 } },
      { 0x00949, { 0x0093E, 0x00306 } },
      { 0x00952, { 0x00331 } },
      { 0x00953, { 0x00300 } },
      { 0x00954, { 0x00301 } },
      { 0x00956, { 0x11B62 } },
      { 0x00957, { 0x11B63 } },
      { 0x00965, { 0x00964, 0x00964 } },
      { 0x00966, { 0x0006F } },
      { 0x00967, { 0x00669 } },
      { 0x00969, { 0x00033 } },
      { 0x00972, { 0x00905, 0x00306 } },
      { 0x00973, { 0x00905, 0x0093A } },
      { 0x00974, { 0x00905, 0x0093E, 0x0093A } },
      { 0x00975, { 0x00905, 0x0094F } },
      { 0x0097D, { 0x0003F } },
      { 0x00981, { 0x00306, 0x00307 } },
      { 0x00986, { 0x00985, 0x009BE } },
      { 0x009BC, { 0x00323 } },
      { 0x009E0, { 0x0098B, 0x009C3 } },
      { 0x009E1, { 0x0098B, 0x009C3 } },
      { 0x009E6, { 0x0006F } },
      { 0x009EA, { 0x00038 } },
      { 0x009ED, { 0x00039 } },
      { 0x009F0, { 0x009B0 } },
      { 0x00A02, { 0x00307 } },
      { 0x00A03, { 0x00983 } },
      { 0x00A06, { 0x00A05, 0x00A3E } },
      { 0x00A07, { 0x0092A, 0x0094D, 0x0091F, 0x009BF } },
      { 0x00A08, { 0x0092A, 0x0094D, 0x0091F, 0x00A40 } },
      { 0x00A09, { 0x00A73, 0x11B62 } },
      { 0x00A0A, { 0x00A73, 0x11B63 } },
      { 0x00A0F, { 0x0092A, 0x0094D, 0x0091F, 0x11B64 } },
      { 0x00A10, { 0x00A05, 0x00948 } },
      { 0x00A14, { 0x00A05, 0x00A4C } },
      { 0x00A15, { 0x00935 } },
      { 0x00A1C, { 0x00924, 0x0094D, 0x00924 } },
      { 0x00A1F, { 0x0091F } },
      { 0x00A20, { 0x00920 } },
      { 0x00A24, { 0x00909 } },
      { 0x00A27, { 0x0092A } },
      { 0x00A2B, { 0x00922 } },
      { 0x00A2E, { 0x0092D } },
      { 0x00A35, { 0x00939 } },
      { 0x00A38, { 0x0092E } },
      { 0x00A3C, { 0x00323 } },
      { 0x00A3F, { 0x009BF } },
      { 0x00A41, { 0x11B62 } },
      { 0x00A42, { 0x11B63 } },
      { 0x00A47, { 0x11B64 } },
      { 0x00A48, { 0x00948 } },
      { 0x00A4B, { 0x00946 } },
      { 0x00A4D, { 0x0094D } },
      { 0x00A66, { 0x0006F } },
      { 0x00A67, { 0x00039 } },
      { 0x00A6A, { 0x00038 } },
      { 0x00A72, { 0x0092A, 0x0094D, 0x0091F } },
      { 0x00A81, { 0x00306, 0x00307 } },
      { 0x00A82, { 0x00307 } },
      { 0x00A83, { 0x0003A } },
      { 0x00A86, { 0x00A85, 0x00ABE } },
      { 0x00A8D, { 0x00A85, 0x00AC5 } },
      { 0x00A8F, { 0x00A85, 0x00AC7 } },
      { 0x00A90, { 0x00A85, 0x00AC8 } },
      { 0x00A91, { 0x00A85, 0x00ABE, 0x00AC5 } },
      { 0x00A93, { 0x00A85, 0x00ABE, 0x00AC7 } },
      { 0x00A94, { 0x00A85, 0x00ABE, 0x00AC8 } },
      { 0x00AAA, { 0x00447 } },
      { 0x00AB0, { 0x00968 } },
      { 0x00ABC, { 0x00323 } },
      { 0x00ABD, { 0x0093D } },
      { 0x00AC1, { 0x00941 } },
      { 0x00AC2, { 0x00942 } },
      { 0x00ACD, { 0x0094D } },
      { 0x00AE6, { 0x0006F } },
      { 0x00AE8, { 0x00968 } },
      { 0x00AE9, { 0x00033 } },
      { 0x00AEA, { 0x0096A } },
      { 0x00AEB, { 0x00447 } },
      { 0x00AEE, { 0x0096E } },
      { 0x00AF0, { 0x00970 } },
      { 0x00B01, { 0x00306, 0x00307 } },
      { 0x00B03, { 0x00038 } },
      { 0x00B06, { 0x00B05, 0x00B3E } },
      { 0x00B20, { 0x0004F } },
      { 0x00B3C, { 0x00323 } },
      { 0x00B66, { 0x0006F } },
      { 0x00B68, { 0x00039 } },
      { 0x00B82, { 0x0030A } },
      { 0x00B8A, { 0x00B89, 0x00BB3 } },
      { 0x00B94, { 0x00B92, 0x00BB3 } },
      { 0x00B9C, { 0x00B90 } },
      { 0x00BB0, { 0x00B88 } },
      { 0x00BB8, { 0x00BB6 } },
      { 0x00BBE, { 0x00B88 } },
      { 0x00BC8, { 0x00BA9 } },
      { 0x00BCA, { 0x00BC6, 0x00B88 } },
      { 0x00BCB, { 0x00BC7, 0x00B88 } },
      { 0x00BCC, { 0x00BC6, 0x00BB3 } },
      { 0x00BCD, { 0x00307 } },
      { 0x00BD7, { 0x00BB3 } },
      { 0x00BE6, { 0x0006F } },
      { 0x00BE7, { 0x00B95 } },
      { 0x00BE8, { 0x00B89 } },
      { 0x00BEA, { 0x00B9A } },
      { 0x00BEB, { 0x00B88, 0x00BC1 } },
      { 0x00BEC, { 0x00B9A, 0x00BC1 } },
      { 0x00BED, { 0x00B8E } },
      { 0x00BEE, { 0x00B85 } },
      { 0x00BF0, { 0x00BAF } },
      { 0x00BF2, { 0x00B9A, 0x00BC2 } },
      { 0x00BF4, { 0x00BAE, 0x00BC0 } },
      { 0x00BF5, { 0x00BF3 } },
      { 0x00BF7, { 0x00B8E, 0x00BB5 } },
      { 0x00BF8, { 0x00BB7 } },
      { 0x00BFA, { 0x00BA8, 0x00BC0 } },
      { 0x00C00, { 0x00306, 0x00307 } },
      { 0x00C02, { 0x0006F } },
      { 0x00C03, { 0x00983 } },
      { 0x00C13, { 0x00C12, 0x00C55 } },
      { 0x00C14, { 0x00C12, 0x00C4C } },
      { 0x00C16, { 0x00C96, 0x00323 } },
      { 0x00C20, { 0x00C30, 0x005BC } },
      { 0x00C22, { 0x00C21, 0x00323 } },
      { 0x00C25, { 0x00C27, 0x005BC } },
      { 0x00C2D, { 0x00C2C, 0x00323 } },
      { 0x00C2E, { 0x00C35, 0x00C41 } },
      { 0x00C37, { 0x00C35, 0x00323 } },
      { 0x00C39, { 0x00C35, 0x00C3E } },
      { 0x00C42, { 0x00C41, 0x00C3E } },
      { 0x00C44, { 0x00C43, 0x00C3E } },
      { 0x00C60, { 0x00C0B, 0x00C3E } },
      { 0x00C61, { 0x00C0C, 0x00C3E } },
      { 0x00C66, { 0x0006F } },
      { 0x00C81, { 0x00306, 0x00307 } },
      { 0x00C82, { 0x0006F } },
      { 0x00C83, { 0x00983 } },
      { 0x00C85, { 0x00C05 } },
      { 0x00C86, { 0x00C06 } },
      { 0x00C87, { 0x00C07 } },
      { 0x00C90, { 0x00C10 } },
      { 0x00C92, { 0x00C12 } },
      { 0x00C93, { 0x00C12, 0x00C55 } },
      { 0x00C94, { 0x00C12, 0x00C4C } },
      { 0x00C97, { 0x00C17 } },
      { 0x00C9C, { 0x00C1C } },
      { 0x00C9D, { 0x00C1D } },
      { 0x00C9E, { 0x00C1E } },
      { 0x00C9F, { 0x00C1F } },
      { 0x00CA3, { 0x00C23 } },
      { 0x00CA6, { 0x00C26 } },
      { 0x00CA8, { 0x00C28 } },
      { 0x00CAF, { 0x00C2F } },
      { 0x00CB0, { 0x00C30 } },
      { 0x00CB1, { 0x00C31 } },
      { 0x00CB2, { 0x00C32 } },
      { 0x00CB3, { 0x00C33 } },
      { 0x00CBF, { 0x00C3F } },
      { 0x00CC1, { 0x00C41 } },
      { 0x00CC3, { 0x00C43 } },
      { 0x00CDC, { 0x00C5C } },
      { 0x00CE1, { 0x00C8C, 0x00CBE } },
      { 0x00CE6, { 0x0004F } },
      { 0x00CE7, { 0x00C67 } },
      { 0x00CE8, { 0x00C68 } },
      { 0x00CEF, { 0x00C6F } },
      { 0x00D01, { 0x00306, 0x00307 } },
      { 0x00D02, { 0x0006F } },
      { 0x00D03, { 0x00983 } },
      { 0x00D08, { 0x00D07, 0x00D57 } },
      { 0x00D09, { 0x00B89 } },
      { 0x00D0A, { 0x00B89, 0x00D57 } },
      { 0x00D0C, { 0x00D28, 0x00D41 } },
      { 0x00D10, { 0x00BC6, 0x00D0E } },
      { 0x00D13, { 0x00D12, 0x00D3E } },
      { 0x00D14, { 0x00D12, 0x00D57 } },
      { 0x00D16, { 0x00BB5 } },
      { 0x00D19, { 0x00D28, 0x00D41 } },
      { 0x00D1C, { 0x00B90 } },
      { 0x00D1F, { 0x00073 } },
      { 0x00D20, { 0x0006F } },
      { 0x00D23, { 0x00BA3 } },
      { 0x00D25, { 0x00BAE } },
      { 0x00D31, { 0x00D30 } },
      { 0x00D34, { 0x00BB4 } },
      { 0x00D36, { 0x00BB6 } },
      { 0x00D3A, { 0x00B9F, 0x00BBF } },
      { 0x00D3F, { 0x00BBF } },
      { 0x00D40, { 0x00BBF } },
      { 0x00D42, { 0x00D41 } },
      { 0x00D43, { 0x00D41 } },
      { 0x00D46, { 0x00BC6 } },
      { 0x00D47, { 0x00BC7 } },
      { 0x00D48, { 0x00BC6, 0x00BC6 } },
      { 0x00D4E, { 0x00971 } },
      { 0x00D5A, { 0x00D28, 0x00D4D, 0x00D2E } },
      { 0x00D5F, { 0x0006F, 0x00D30, 0x0006F } },
      { 0x00D61, { 0x00D1E } },
      { 0x00D66, { 0x0006F } },
      { 0x00D6A, { 0x00D30, 0x00D4D } },
      { 0x00D6B, { 0x00D26, 0x00D4D, 0x00D30 } },
      { 0x00D6C, { 0x00D28, 0x00D4D, 0x00D28 } },
      { 0x00D6D, { 0x00039 } },
      { 0x00D6E, { 0x00D35, 0x00D4D, 0x00D30 } },
      { 0x00D6F, { 0x00D28, 0x00D4D } },
      { 0x00D76, { 0x00D39, 0x00D4D, 0x00D2E } },
      { 0x00D79, { 0x00D28, 0x00D41 } },
      { 0x00D7A, { 0x00BA3, 0x00D4D } },
      { 0x00D7B, { 0x00D28, 0x00D4D } },
      { 0x00D7C, { 0x00D30, 0x00D4D } },
      { 0x00D7D, { 0x00D32, 0x00D4D } },
      { 0x00D7E, { 0x00D33, 0x00D4D } },
      { 0x00D82, { 0x0006F } },
      { 0x00D83, { 0x00983 } },
      { 0x00D8D, { 0x00DC3, 0x00DD8 } },
      { 0x00D92, { 0x00D91, 0x00DCA } },
      { 0x00D93, { 0x00D91, 0x00DD9 } },
      { 0x00DB5, { 0x00D91 } },
      { 0x00DB6, { 0x00D9B } },
      { 0x00DB9, { 0x00D94 } },
      { 0x00DC0, { 0x00DA0 } },
      { 0x00DC4, { 0x00DB7 } },
      { 0x00DE9, { 0x00DE8, 0x00DCF } },
      { 0x00DEA, { 0x00DA2 } },
      { 0x00DEB, { 0x00DAF } },
      { 0x00DEF, { 0x00DE8, 0x00DD3 } },
      { 0x00E03, { 0x00E02 } },
      { 0x00E0B, { 0x00E0A } },
      { 0x00E0F, { 0x00E0E } },
      { 0x00E14, { 0x00E04 } },
      { 0x00E15, { 0x00E04 } },
      { 0x00E17, { 0x00E11 } },
      { 0x00E21, { 0x00E06 } },
      { 0x00E26, { 0x00E20 } },
      { 0x00E33, { 0x0030A, 0x00E32 } },
      { 0x00E41, { 0x00E40, 0x00E40 } },
      { 0x00E45, { 0x00E32 } },
      { 0x00E4D, { 0x0030A } },
      { 0x00E50, { 0x0006F } },
      { 0x00E88, { 0x00E08 } },
      { 0x00E8D, { 0x00E22 } },
      { 0x00E9A, { 0x00E1A } },
      { 0x00E9B, { 0x00E1B } },
      { 0x00E9D, { 0x00E1D } },
      { 0x00E9E, { 0x00E1E } },
      { 0x00E9F, { 0x00E1F } },
      { 0x00EB3, { 0x0030A, 0x00EB2 } },
      { 0x00EB8, { 0x00E38 } },
      { 0x00EB9, { 0x00E39 } },
      { 0x00EC8, { 0x00E48 } },
      { 0x00EC9, { 0x00E49 } },
      { 0x00ECA, { 0x00E4A } },
      { 0x00ECB, { 0x00E4B } },
      { 0x00ECD, { 0x0030A } },
      { 0x00ED0, { 0x0006F } },
      { 0x00EDC, { 0x00EAB, 0x00E99 } },
      { 0x00EDD, { 0x00EAB, 0x00EA1 } },
      { 0x00F00, { 0x00F68, 0x00F7C, 0x00F7E } },
      { 0x00F02, { 0x00F60, 0x00F74, 0x00F82, 0x00F7F } },
      { 0x00F03, { 0x00F60, 0x00F74, 0x00F82, 0x00F14 } },
      { 0x00F0C, { 0x00F0B } },
      { 0x00F0E, { 0x00F0D, 0x00F0D } },
      { 0x00F1B, { 0x00F1A, 0x00F1A } },
      { 0x00F1E, { 0x00F1D, 0x00F1D } },
      { 0x00F1F, { 0x00F1A, 0x00F1D } },
      { 0x00F37, { 0x00325 } },
      { 0x00F6A, { 0x00F62 } },
      { 0x00F77, { 0x00FB2, 0x00F71, 0x00F80 } },
      { 0x00F79, { 0x00FB3, 0x00F71, 0x00F80 } },
      { 0x00F7B, { 0x00F7A, 0x00F7A } },
      { 0x00F7D, { 0x00F7C, 0x00F7C } },
      { 0x00FCE, { 0x00F1D, 0x00F1A } },
      { 0x00FD5, { 0x05350 } },
      { 0x00FD6, { 0x0534D } },
      { 0x01000, { 0x00D30, 0x0102C } },
      { 0x01002, { 0x00D30 } },
      { 0x01004, { 0x00063 } },
      { 0x01010, { 0x0006F, 0x0102C } },
      { 0x0101D, { 0x0006F } },
      { 0x0101F, { 0x01015, 0x0102C } },
      { 0x01023, { 0x00D30, 0x0102C, 0x01039, 0x00D30, 0x0102C } },
      { 0x01029, { 0x0101E, 0x0103C } },
      { 0x0102A, { 0x0101E, 0x0103C, 0x00B47, 0x0102C, 0x0103A } },
      { 0x01031, { 0x00B47 } },
      { 0x01036, { 0x0030A } },
      { 0x01038, { 0x00983 } },
      { 0x01040, { 0x0006F } },
      { 0x0104B, { 0x0104A, 0x0104A } },
      { 0x0105A, { 0x00063 } },
      { 0x01061, { 0x0101B, 0x0103E } },
      { 0x01065, { 0x01041 } },
      { 0x01066, { 0x01015, 0x0103E } },
      { 0x0106F, { 0x01015, 0x0102C, 0x0103E } },
      { 0x01070, { 0x01003, 0x0103E } },
      { 0x0107E, { 0x0107D, 0x0103E } },
      { 0x01081, { 0x00D30, 0x0103E } },
      { 0x0109E, { 0x01083, 0x0030A } },
      { 0x010A0, { 0x0A786 } },
      { 0x010D7, { 0x0006F, 0x0102C } },
      { 0x010D8, { 0x00D30 } },
      { 0x010E7, { 0x00079 } },
      { 0x010F3, { 0x0021D } },
      { 0x010FF, { 0x0006F } },
      { 0x01101, { 0x01100, 0x01100 } },
      { 0x01104, { 0x01103, 0x01103 } },
      { 0x01108, { 0x01107, 0x01107 } },
      { 0x0110A, { 0x01109, 0x01109 } },
      { 0x0110D, { 0x0110C, 0x0110C } },
      { 0x01113, { 0x01102, 0x01100 } },
      { 0x01114, { 0x01102, 0x01102 } },
      { 0x01115, { 0x01102, 0x01103 } },
      { 0x01116, { 0x01102, 0x01107 } },
      { 0x01117, { 0x01103, 0x01100 } },
      { 0x01118, { 0x01105, 0x01102 } },
      { 0x01119, { 0x01105, 0x01105 } },
      { 0x0111A, { 0x01105, 0x01112 } },
      { 0x0111B, { 0x01105, 0x0110B } },
      { 0x0111C, { 0x01106, 0x01107 } },
      { 0x0111D, { 0x01106, 0x0110B } },
      { 0x0111E, { 0x01107, 0x01100 } },
      { 0x0111F, { 0x01107, 0x01102 } },
      { 0x01120, { 0x01107, 0x01103 } },
      { 0x01121, { 0x01107, 0x01109 } },
      { 0x01122, { 0x01107, 0x01109, 0x01100 } },
      { 0x01123, { 0x01107, 0x01109, 0x01103 } },
      { 0x01124, { 0x01107, 0x01109, 0x01107 } },
      { 0x01125, { 0x01107, 0x01109, 0x01109 } },
      { 0x01126, { 0x01107, 0x01109, 0x0110C } },
      { 0x01127, { 0x01107, 0x0110C } },
      { 0x01128, { 0x01107, 0x0110E } },
      { 0x01129, { 0x01107, 0x01110 } },
      { 0x0112A, { 0x01107, 0x01111 } },
      { 0x0112B, { 0x01107, 0x0110B } },
      { 0x0112C, { 0x01107, 0x01107, 0x0110B } },
      { 0x0112D, { 0x01109, 0x01100 } },
      { 0x0112E, { 0x01109, 0x01102 } },
      { 0x0112F, { 0x01109, 0x01103 } },
      { 0x01130, { 0x01109, 0x01105 } },
      { 0x01131, { 0x01109, 0x01106 } },
      { 0x01132, { 0x01109, 0x01107 } },
      { 0x01133, { 0x01109, 0x01107, 0x01100 } },
      { 0x01134, { 0x01109, 0x01109, 0x01109 } },
      { 0x01135, { 0x01109, 0x0110B } },
      { 0x01136, { 0x01109, 0x0110C } },
      { 0x01137, { 0x01109, 0x0110E } },
      { 0x01138, { 0x01109, 0x0110F } },
      { 0x01139, { 0x01109, 0x01110 } },
      { 0x0113A, { 0x01109, 0x01111 } },
      { 0x0113B, { 0x01105, 0x01112 } },
      { 0x0113D, { 0x0113C, 0x0113C } },
      { 0x0113F, { 0x0113E, 0x0113E } },
      { 0x01141, { 0x0110B, 0x01100 } },
      { 0x01142, { 0x0110B, 0x01103 } },
      { 0x01143, { 0x0110B, 0x01106 } },
      { 0x01144, { 0x0110B, 0x01107 } },
      { 0x01145, { 0x0110B, 0x01109 } },
      { 0x01146, { 0x0110B, 0x01140 } },
      { 0x01147, { 0x0110B, 0x0110B } },
      { 0x01148, { 0x0110B, 0x0110C } },
      { 0x01149, { 0x0110B, 0x0110E } },
      { 0x0114A, { 0x0110B, 0x01110 } },
      { 0x0114B, { 0x0110B, 0x01111 } },
      { 0x0114D, { 0x0110C, 0x0110B } },
      { 0x0114F, { 0x0114E, 0x0114E } },
      { 0x01151, { 0x01150, 0x01150 } },
      { 0x01152, { 0x0110E, 0x0110F } },
      { 0x01153, { 0x0110E, 0x01112 } },
      { 0x01156, { 0x01111, 0x01107 } },
      { 0x01157, { 0x01111, 0x0110B } },
      { 0x01158, { 0x01112, 0x01112 } },
      { 0x0115A, { 0x01100, 0x01103 } },
      { 0x0115B, { 0x01102, 0x01109 } },
      { 0x0115C, { 0x01102, 0x0110C } },
      { 0x0115D, { 0x01102, 0x01112 } },
      { 0x0115E, { 0x01103, 0x01105 } },
      { 0x01162, { 0x01161, 0x04E28 } },
      { 0x01164, { 0x01163, 0x04E28 } },
      { 0x01166, { 0x01165, 0x04E28 } },
      { 0x01168, { 0x01167, 0x04E28 } },
      { 0x0116A, { 0x01169, 0x01161 } },
      { 0x0116B, { 0x01169, 0x01161, 0x04E28 } },
      { 0x0116C, { 0x01169, 0x04E28 } },
      { 0x0116F, { 0x0116E, 0x01165 } },
      { 0x01170, { 0x0116E, 0x01165, 0x04E28 } },
      { 0x01171, { 0x0116E, 0x04E28 } },
      { 0x01173, { 0x030FC } },
      { 0x01174, { 0x030FC, 0x04E28 } },
      { 0x01175, { 0x04E28 } },
      { 0x01176, { 0x01161, 0x01169 } },
      { 0x01177, { 0x01161, 0x0116E } },
      { 0x01178, { 0x01163, 0x01169 } },
      { 0x01179, { 0x01163, 0x0116D } },
      { 0x0117A, { 0x01165, 0x01169 } },
      { 0x0117B, { 0x01165, 0x0116E } },
      { 0x0117C, { 0x01165, 0x030FC } },
      { 0x0117D, { 0x01167, 0x01169 } },
      { 0x0117E, { 0x01167, 0x0116E } },
      { 0x0117F, { 0x01169, 0x01165 } },
      { 0x01180, { 0x01169, 0x01165, 0x04E28 } },
      { 0x01181, { 0x01169, 0x01167, 0x04E28 } },
      { 0x01182, { 0x01169, 0x01169 } },
      { 0x01183, { 0x01169, 0x0116E } },
      { 0x01184, { 0x0116D, 0x01163 } },
      { 0x01185, { 0x0116D, 0x01163, 0x04E28 } },
      { 0x01186, { 0x0116D, 0x01163 } },
      { 0x01187, { 0x0116D, 0x01169 } },
      { 0x01188, { 0x0116D, 0x04E28 } },
      { 0x01189, { 0x0116E, 0x01161 } },
      { 0x0118A, { 0x0116E, 0x01161, 0x04E28 } },
      { 0x0118B, { 0x0116E, 0x01165, 0x030FC } },
      { 0x0118C, { 0x0116E, 0x01167, 0x04E28 } },
      { 0x0118D, { 0x0116E, 0x0116E } },
      { 0x0118E, { 0x01172, 0x01161 } },
      { 0x0118F, { 0x01172, 0x01165 } },
      { 0x01190, { 0x01172, 0x01165, 0x04E28 } },
      { 0x01191, { 0x01172, 0x01167 } },
      { 0x01192, { 0x01172, 0x01167, 0x04E28 } },
      { 0x01193, { 0x01172, 0x0116E } },
      { 0x01194, { 0x01172, 0x04E28 } },
      { 0x01195, { 0x030FC, 0x0116E } },
      { 0x01196, { 0x030FC, 0x030FC } },
      { 0x01197, { 0x030FC, 0x04E28, 0x0116E } },
      { 0x01198, { 0x04E28, 0x01161 } },
      { 0x01199, { 0x04E28, 0x01163 } },
      { 0x0119A, { 0x04E28, 0x01169 } },
      { 0x0119B, { 0x04E28, 0x0116E } },
      { 0x0119C, { 0x04E28, 0x030FC } },
      { 0x0119D, { 0x04E28, 0x0119E } },
      { 0x0119F, { 0x0119E, 0x01165 } },
      { 0x011A0, { 0x0119E, 0x0116E } },
      { 0x011A1, { 0x0119E, 0x04E28 } },
      { 0x011A2, { 0x0119E, 0x0119E } },
      { 0x011A3, { 0x01161, 0x030FC } },
      { 0x011A4, { 0x01163, 0x0116E } },
      { 0x011A5, { 0x01167, 0x01163 } },
      { 0x011A6, { 0x01169, 0x01163 } },
      { 0x011A7, { 0x01169, 0x01163, 0x04E28 } },
      { 0x011A8, { 0x01100 } },
      { 0x011A9, { 0x01100, 0x01100 } },
      { 0x011AA, { 0x01100, 0x01109 } },
      { 0x011AB, { 0x01102 } },
      { 0x011AC, { 0x01102, 0x0110C } },
      { 0x011AD, { 0x01102, 0x01112 } },
      { 0x011AE, { 0x01103 } },
      { 0x011AF, { 0x01105 } },
      { 0x011B0, { 0x01105, 0x01100 } },
      { 0x011B1, { 0x01105, 0x01106 } },
      { 0x011B2, { 0x01105, 0x01107 } },
      { 0x011B3, { 0x01105, 0x01109 } },
      { 0x011B4, { 0x01105, 0x01110 } },
      { 0x011B5, { 0x01105, 0x01111 } },
      { 0x011B6, { 0x01105, 0x01112 } },
      { 0x011B7, { 0x01106 } },
      { 0x011B8, { 0x01107 } },
      { 0x011B9, { 0x01107, 0x01109 } },
      { 0x011BA, { 0x01109 } },
      { 0x011BB, { 0x01109, 0x01109 } },
      { 0x011BC, { 0x0110B } },
      { 0x011BD, { 0x0110C } },
      { 0x011BE, { 0x0110E } },
      { 0x011BF, { 0x0110F } },
      { 0x011C0, { 0x01110 } },
      { 0x011C1, { 0x01111 } },
      { 0x011C2, { 0x01112 } },
      { 0x011C3, { 0x01100, 0x01105 } },
      { 0x011C4, { 0x01100, 0x01109, 0x01100 } },
      { 0x011C5, { 0x01102, 0x01100 } },
      { 0x011C6, { 0x01102, 0x01103 } },
      { 0x011C7, { 0x01102, 0x01109 } },
      { 0x011C8, { 0x01102, 0x01140 } },
      { 0x011C9, { 0x01102, 0x01110 } },
      { 0x011CA, { 0x01103, 0x01100 } },
      { 0x011CB, { 0x01103, 0x01105 } },
      { 0x011CC, { 0x01105, 0x01100, 0x01109 } },
      { 0x011CD, { 0x01105, 0x01102 } },
      { 0x011CE, { 0x01105, 0x01103 } },
      { 0x011CF, { 0x01105, 0x01103, 0x01112 } },
      { 0x011D0, { 0x01105, 0x01105 } },
      { 0x011D1, { 0x01105, 0x01106, 0x01100 } },
      { 0x011D2, { 0x01105, 0x01106, 0x01109 } },
      { 0x011D3, { 0x01105, 0x01107, 0x01109 } },
      { 0x011D4, { 0x01105, 0x01107, 0x01112 } },
      { 0x011D5, { 0x01105, 0x01107, 0x0110B } },
      { 0x011D6, { 0x01105, 0x01109, 0x01109 } },
      { 0x011D7, { 0x01105, 0x01140 } },
      { 0x011D8, { 0x01105, 0x0110F } },
      { 0x011D9, { 0x01105, 0x01159 } },
      { 0x011DA, { 0x01106, 0x01100 } },
      { 0x011DB, { 0x01106, 0x01105 } },
      { 0x011DC, { 0x01106, 0x01107 } },
      { 0x011DD, { 0x01106, 0x01109 } },
      { 0x011DE, { 0x01106, 0x01109, 0x01109 } },
      { 0x011DF, { 0x01106, 0x01140 } },
      { 0x011E0, { 0x01106, 0x0110E } },
      { 0x011E1, { 0x01106, 0x01112 } },
      { 0x011E2, { 0x01106, 0x0110B } },
      { 0x011E3, { 0x01107, 0x01105 } },
      { 0x011E4, { 0x01107, 0x01111 } },
      { 0x011E5, { 0x01107, 0x01112 } },
      { 0x011E6, { 0x01107, 0x0110B } },
      { 0x011E7, { 0x01109, 0x01100 } },
      { 0x011E8, { 0x01109, 0x01103 } },
      { 0x011E9, { 0x01109, 0x01105 } },
      { 0x011EA, { 0x01109, 0x01107 } },
      { 0x011EB, { 0x01140 } },
      { 0x011EC, { 0x0110B, 0x01100 } },
      { 0x011ED, { 0x0110B, 0x01100, 0x01100 } },
      { 0x011EE, { 0x0110B, 0x0110B } },
      { 0x011EF, { 0x0110B, 0x0110F } },
      { 0x011F0, { 0x0114C } },
      { 0x011F1, { 0x0110B, 0x01109 } },
      { 0x011F2, { 0x0110B, 0x01140 } },
      { 0x011F3, { 0x01111, 0x01107 } },
      { 0x011F4, { 0x01111, 0x0110B } },
      { 0x011F5, { 0x01112, 0x01102 } },
      { 0x011F6, { 0x01112, 0x01105 } },
      { 0x011F7, { 0x01112, 0x01106 } },
      { 0x011F8, { 0x01112, 0x01107 } },
      { 0x011F9, { 0x01159 } },
      { 0x011FA, { 0x01100, 0x01102 } },
      { 0x011FB, { 0x01100, 0x01107 } },
      { 0x011FC, { 0x01100, 0x0110E } },
      { 0x011FD, { 0x01100, 0x0110F } },
      { 0x011FE, { 0x01100, 0x01112 } },
      { 0x011FF, { 0x01102, 0x01102 } },
      { 0x01200, { 0x00055 } },
      { 0x01223, { 0x00270 } },
      { 0x01240, { 0x003A6 } },
      { 0x01260, { 0x00548 } },
      { 0x01294, { 0x00571 } },
      { 0x012D0, { 0x0004F } },
      { 0x013A0, { 0x00044 } },
      { 0x013A1, { 0x00052 } },
      { 0x013A2, { 0x00054 } },
      { 0x013A4, { 0x0004F, 0x00027 } },
      { 0x013A5, { 0x00069 } },
      { 0x013A8, { 0x02C75 } },
      { 0x013A9, { 0x00059 } },
      { 0x013AA, { 0x00041 } },
      { 0x013AB, { 0x0004A } },
      { 0x013AC, { 0x00045 } },
      { 0x013AE, { 0x0003F } },
      { 0x013B0, { 0x02C75 } },
      { 0x013B1, { 0x00393 } },
      { 0x013B3, { 0x00057 } },
      { 0x013B7, { 0x0004D } },
      { 0x013BB, { 0x00048 } },
      { 0x013BD, { 0x00059 } },
      { 0x013BE, { 0x0004F, 0x00335 } },
      { 0x013BF, { 0x001AB } },
      { 0x013C0, { 0x00047 } },
      { 0x013C2, { 0x00068 } },
      { 0x013C3, { 0x0005A } },
      { 0x013C7, { 0x00460 } },
      { 0x013CB, { 0x00190 } },
      { 0x013CC, { 0x00055, 0x00335 } },
      { 0x013CE, { 0x00034 } },
      { 0x013CF, { 0x00062 } },
      { 0x013D2, { 0x00052 } },
      { 0x013D4, { 0x00057 } },
      { 0x013D5, { 0x00053 } },
      { 0x013D9, { 0x00056 } },
      { 0x013DA, { 0x00053 } },
      { 0x013DE, { 0x0004C } },
      { 0x013DF, { 0x00043 } },
      { 0x013E2, { 0x00050 } },
      { 0x013E6, { 0x0004B } },
      { 0x013E7, { 0x00064 } },
      { 0x013EB, { 0x0004F, 0x00335 } },
      { 0x013EE, { 0x00036 } },
      { 0x013F0, { 0x000DF } },
      { 0x013F2, { 0x00068, 0x00314 } },
      { 0x013F3, { 0x00047 } },
      { 0x013F4, { 0x00042 } },
      { 0x013FB, { 0x00262 } },
      { 0x013FC, { 0x00299 } },
      { 0x01400, { 0x0003D } },
      { 0x01403, { 0x00394 } },
      { 0x0140C, { 0x000B7, 0x01401 } },
      { 0x0140D, { 0x01401, 0x000B7 } },
      { 0x0140E, { 0x000B7, 0x00394 } },
      { 0x0140F, { 0x00394, 0x000B7 } },
      { 0x01410, { 0x000B7, 0x01404 } },
      { 0x01411, { 0x01404, 0x000B7 } },
      { 0x01412, { 0x000B7, 0x01405 } },
      { 0x01413, { 0x01405, 0x000B7 } },
      { 0x01414, { 0x000B7, 0x01406 } },
      { 0x01415, { 0x01406, 0x000B7 } },
      { 0x01417, { 0x000B7, 0x0140A } },
      { 0x01418, { 0x0140A, 0x000B7 } },
      { 0x01419, { 0x000B7, 0x0140B } },
      { 0x0141A, { 0x0140B, 0x000B7 } },
      { 0x01427, { 0x000B7 } },
      { 0x0142B, { 0x01401, 0x01420 } },
      { 0x0142C, { 0x00394, 0x01420 } },
      { 0x0142D, { 0x01405, 0x01420 } },
      { 0x0142E, { 0x0140A, 0x01420 } },
      { 0x0142F, { 0x00056 } },
      { 0x01431, { 0x00245 } },
      { 0x01433, { 0x0003E } },
      { 0x01437, { 0x000B7, 0x0003E } },
      { 0x01438, { 0x0003C } },
      { 0x0143A, { 0x000B7, 0x00056 } },
      { 0x0143B, { 0x00056, 0x000B7 } },
      { 0x0143C, { 0x000B7, 0x00245 } },
      { 0x0143D, { 0x00245, 0x000B7 } },
      { 0x0143E, { 0x000B7, 0x01432 } },
      { 0x0143F, { 0x01432, 0x000B7 } },
      { 0x01440, { 0x000B7, 0x0003E } },
      { 0x01441, { 0x0003E, 0x000B7 } },
      { 0x01442, { 0x000B7, 0x01434 } },
      { 0x01443, { 0x01434, 0x000B7 } },
      { 0x01444, { 0x000B7, 0x0003C } },
      { 0x01445, { 0x0003C, 0x000B7 } },
      { 0x01446, { 0x000B7, 0x01439 } },
      { 0x01447, { 0x01439, 0x000B7 } },
      { 0x0144A, { 0x00027 } },
      { 0x0144C, { 0x00055 } },
      { 0x0144E, { 0x00548 } },
      { 0x01454, { 0x000B7, 0x01450 } },
      { 0x01457, { 0x000B7, 0x00055 } },
      { 0x01458, { 0x00055, 0x000B7 } },
      { 0x01459, { 0x000B7, 0x00548 } },
      { 0x0145A, { 0x00548, 0x000B7 } },
      { 0x0145B, { 0x000B7, 0x0144F } },
      { 0x0145C, { 0x0144F, 0x000B7 } },
      { 0x0145D, { 0x000B7, 0x01450 } },
      { 0x0145E, { 0x01450, 0x000B7 } },
      { 0x0145F, { 0x000B7, 0x01451 } },
      { 0x01460, { 0x01451, 0x000B7 } },
      { 0x01461, { 0x000B7, 0x01455 } },
      { 0x01462, { 0x01455, 0x000B7 } },
      { 0x01463, { 0x000B7, 0x01456 } },
      { 0x01464, { 0x01456, 0x000B7 } },
      { 0x01467, { 0x00055, 0x00027 } },
      { 0x01468, { 0x00548, 0x00027 } },
      { 0x01469, { 0x01450, 0x00027 } },
      { 0x0146A, { 0x01455, 0x00027 } },
      { 0x0146D, { 0x00050 } },
      { 0x0146F, { 0x00064 } },
      { 0x01472, { 0x00062 } },
      { 0x01473, { 0x00062, 0x00307 } },
      { 0x01474, { 0x000B7, 0x0146B } },
      { 0x01475, { 0x0146B, 0x000B7 } },
      { 0x01476, { 0x000B7, 0x00050 } },
      { 0x01477, { 0x00070, 0x000B7 } },
      { 0x01478, { 0x000B7, 0x0146E } },
      { 0x01479, { 0x0146E, 0x000B7 } },
      { 0x0147A, { 0x000B7, 0x00064 } },
      { 0x0147B, { 0x00064, 0x000B7 } },
      { 0x0147C, { 0x000B7, 0x01470 } },
      { 0x0147D, { 0x01470, 0x000B7 } },
      { 0x0147E, { 0x000B7, 0x00062 } },
      { 0x0147F, { 0x00062, 0x000B7 } },
      { 0x01480, { 0x000B7, 0x00062, 0x00307 } },
      { 0x01481, { 0x00062, 0x00307, 0x000B7 } },
      { 0x01485, { 0x0146B, 0x00027 } },
      { 0x01486, { 0x00050, 0x00027 } },
      { 0x01487, { 0x00064, 0x00027 } },
      { 0x01488, { 0x00062, 0x00027 } },
      { 0x0148D, { 0x0004A } },
      { 0x01492, { 0x000B7, 0x01489 } },
      { 0x01493, { 0x01489, 0x000B7 } },
      { 0x01494, { 0x000B7, 0x0148B } },
      { 0x01495, { 0x0148B, 0x000B7 } },
      { 0x01496, { 0x000B7, 0x0148C } },
      { 0x01497, { 0x0148C, 0x000B7 } },
      { 0x01498, { 0x000B7, 0x0004A } },
      { 0x01499, { 0x0004A, 0x000B7 } },
      { 0x0149A, { 0x000B7, 0x0148E } },
      { 0x0149B, { 0x0148E, 0x000B7 } },
      { 0x0149C, { 0x000B7, 0x01490 } },
      { 0x0149D, { 0x01490, 0x000B7 } },
      { 0x0149E, { 0x000B7, 0x01491 } },
      { 0x0149F, { 0x01491, 0x000B7 } },
      { 0x014A5, { 0x00393 } },
      { 0x014AA, { 0x0004C } },
      { 0x014AC, { 0x000B7, 0x014A3 } },
      { 0x014AD, { 0x014A3, 0x000B7 } },
      { 0x014AE, { 0x000B7, 0x00393 } },
      { 0x014AF, { 0x00393, 0x000B7 } },
      { 0x014B0, { 0x000B7, 0x014A6 } },
      { 0x014B1, { 0x014A6, 0x000B7 } },
      { 0x014B2, { 0x000B7, 0x014A7 } },
      { 0x014B3, { 0x014A7, 0x000B7 } },
      { 0x014B4, { 0x000B7, 0x014A8 } },
      { 0x014B5, { 0x014A8, 0x000B7 } },
      { 0x014B6, { 0x000B7, 0x0004C } },
      { 0x014B7, { 0x0006C, 0x000B7 } },
      { 0x014B8, { 0x000B7, 0x014AB } },
      { 0x014B9, { 0x014AB, 0x000B7 } },
      { 0x014BF, { 0x00032 } },
      { 0x014C9, { 0x000B7, 0x014C0 } },
      { 0x014CA, { 0x014C0, 0x000B7 } },
      { 0x014CB, { 0x000B7, 0x014C7 } },
      { 0x014CC, { 0x014C7, 0x000B7 } },
      { 0x014CD, { 0x000B7, 0x014C8 } },
      { 0x014CE, { 0x014C8, 0x000B7 } },
      { 0x014D1, { 0x01421 } },
      { 0x014DC, { 0x000B7, 0x014D3 } },
      { 0x014DD, { 0x014D3, 0x000B7 } },
      { 0x014DE, { 0x000B7, 0x014D5 } },
      { 0x014DF, { 0x014D5, 0x000B7 } },
      { 0x014E0, { 0x000B7, 0x014D6 } },
      { 0x014E1, { 0x014D6, 0x000B7 } },
      { 0x014E2, { 0x000B7, 0x014D7 } },
      { 0x014E3, { 0x014D7, 0x000B7 } },
      { 0x014E4, { 0x000B7, 0x014D8 } },
      { 0x014E5, { 0x014D8, 0x000B7 } },
      { 0x014E6, { 0x000B7, 0x014DA } },
      { 0x014E7, { 0x014DA, 0x000B7 } },
      { 0x014E8, { 0x000B7, 0x014DB } },
      { 0x014E9, { 0x014DB, 0x000B7 } },
      { 0x014F6, { 0x000B7, 0x014ED } },
      { 0x014F7, { 0x014ED, 0x000B7 } },
      { 0x014F8, { 0x000B7, 0x014EF } },
      { 0x014F9, { 0x014EF, 0x000B7 } },
      { 0x014FA, { 0x000B7, 0x014F0 } },
      { 0x014FB, { 0x014F0, 0x000B7 } },
      { 0x014FC, { 0x000B7, 0x014F1 } },
      { 0x014FD, { 0x014F1, 0x000B7 } },
      { 0x014FE, { 0x000B7, 0x014F2 } },
      { 0x014FF, { 0x014F2, 0x000B7 } },
      { 0x01500, { 0x000B7, 0x014F4 } },
      { 0x01501, { 0x014F4, 0x000B7 } },
      { 0x01502, { 0x000B7, 0x014F5 } },
      { 0x01503, { 0x014F5, 0x000B7 } },
      { 0x0150C, { 0x0150B, 0x0003C } },
      { 0x0150D, { 0x0150B, 0x01455 } },
      { 0x0150E, { 0x0150B, 0x00062 } },
      { 0x0150F, { 0x0150B, 0x01490 } },
      { 0x01517, { 0x000B7, 0x01510 } },
      { 0x01518, { 0x01510, 0x000B7 } },
      { 0x01519, { 0x000B7, 0x01511 } },
      { 0x0151A, { 0x01511, 0x000B7 } },
      { 0x0151B, { 0x000B7, 0x01512 } },
      { 0x0151C, { 0x01512, 0x000B7 } },
      { 0x0151D, { 0x000B7, 0x01513 } },
      { 0x0151E, { 0x01513, 0x000B7 } },
      { 0x0151F, { 0x000B7, 0x01514 } },
      { 0x01520, { 0x01514, 0x000B7 } },
      { 0x01521, { 0x000B7, 0x01515 } },
      { 0x01522, { 0x01515, 0x000B7 } },
      { 0x01523, { 0x000B7, 0x01516 } },
      { 0x01524, { 0x01516, 0x000B7 } },
      { 0x0152F, { 0x000B7, 0x00034 } },
      { 0x01530, { 0x00034, 0x000B7 } },
      { 0x01531, { 0x000B7, 0x01528 } },
      { 0x01532, { 0x01528, 0x000B7 } },
      { 0x01533, { 0x000B7, 0x01529 } },
      { 0x01534, { 0x01529, 0x000B7 } },
      { 0x01535, { 0x000B7, 0x0152A } },
      { 0x01536, { 0x0152A, 0x000B7 } },
      { 0x01537, { 0x000B7, 0x0152B } },
      { 0x01538, { 0x0152B, 0x000B7 } },
      { 0x01539, { 0x000B7, 0x0152D } },
      { 0x0153A, { 0x0152D, 0x000B7 } },
      { 0x0153B, { 0x000B7, 0x0152E } },
      { 0x0153C, { 0x0152E, 0x000B7 } },
      { 0x01540, { 0x01429 } },
      { 0x01541, { 0x00078 } },
      { 0x0154E, { 0x000B7, 0x0154C } },
      { 0x0154F, { 0x0154C, 0x000B7 } },
      { 0x0155B, { 0x000B7, 0x0155A } },
      { 0x0155C, { 0x0155A, 0x000B7 } },
      { 0x01568, { 0x000B7, 0x01567 } },
      { 0x01569, { 0x01567, 0x000B7 } },
      { 0x01577, { 0x01E9F } },
      { 0x0157C, { 0x00048 } },
      { 0x0157D, { 0x00078 } },
      { 0x0157E, { 0x01550, 0x0146C } },
      { 0x0157F, { 0x01550, 0x00050 } },
      { 0x01580, { 0x01550, 0x0146E } },
      { 0x01581, { 0x01550, 0x00064 } },
      { 0x01582, { 0x01550, 0x01470 } },
      { 0x01583, { 0x01550, 0x00062 } },
      { 0x01584, { 0x01550, 0x00062, 0x00307 } },
      { 0x01585, { 0x01550, 0x01483 } },
      { 0x01587, { 0x00052 } },
      { 0x0158E, { 0x01595, 0x0148A } },
      { 0x0158F, { 0x01595, 0x0148B } },
      { 0x01590, { 0x01595, 0x0148C } },
      { 0x01591, { 0x01595, 0x0004A } },
      { 0x01592, { 0x01595, 0x0148E } },
      { 0x01593, { 0x01595, 0x01490 } },
      { 0x01594, { 0x01595, 0x01491 } },
      { 0x015AF, { 0x00062 } },
      { 0x015B4, { 0x00046 } },
      { 0x015B5, { 0x02132 } },
      { 0x015B7, { 0x0A7FB } },
      { 0x015C4, { 0x02C6F } },
      { 0x015C5, { 0x00041 } },
      { 0x015DE, { 0x00044 } },
      { 0x015EA, { 0x00044 } },
      { 0x015EF, { 0x00460 } },
      { 0x015F0, { 0x0004D } },
      { 0x015F7, { 0x00042 } },
      { 0x01602, { 0x01490 } },
      { 0x01603, { 0x01489 } },
      { 0x01604, { 0x014D3 } },
      { 0x01607, { 0x014DA } },
      { 0x01622, { 0x01543 } },
      { 0x01623, { 0x01546 } },
      { 0x01624, { 0x0154A } },
      { 0x0162E, { 0x001B1 } },
      { 0x0162F, { 0x003A9 } },
      { 0x01634, { 0x001B1 } },
      { 0x01635, { 0x003A9 } },
      { 0x0166D, { 0x00058 } },
      { 0x0166E, { 0x00078 } },
      { 0x0166F, { 0x01550, 0x0146B } },
      { 0x01670, { 0x01595, 0x01489 } },
      { 0x01671, { 0x01596, 0x0148B } },
      { 0x01672, { 0x01596, 0x0148C } },
      { 0x01673, { 0x01596, 0x0004A } },
      { 0x01674, { 0x01596, 0x0148E } },
      { 0x01675, { 0x01596, 0x01490 } },
      { 0x01676, { 0x01596, 0x01491 } },
      { 0x01677, { 0x015A7, 0x000B7 } },
      { 0x01678, { 0x015A8, 0x000B7 } },
      { 0x01679, { 0x015A9, 0x000B7 } },
      { 0x0167A, { 0x015AA, 0x000B7 } },
      { 0x0167B, { 0x015AB, 0x000B7 } },
      { 0x0167C, { 0x015AC, 0x000B7 } },
      { 0x0167D, { 0x015AD, 0x000B7 } },
      { 0x01680, { 0x00020 } },
      { 0x016B2, { 0x0003C } },
      { 0x016B7, { 0x00058 } },
      { 0x016C1, { 0x0006C } },
      { 0x016C2, { 0x016BD } },
      { 0x016CC, { 0x00027 } },
      { 0x016D5, { 0x0004B } },
      { 0x016D6, { 0x0004D } },
      { 0x016D8, { 0x003A8 } },
      { 0x016E1, { 0x016BC } },
      { 0x016EB, { 0x000B7 } },
      { 0x016EC, { 0x0003A } },
      { 0x016ED, { 0x0002B } },
      { 0x016F0, { 0x003A6 } },
      { 0x01734, { 0x01715 } },
      { 0x01735, { 0x0002F } },
      { 0x0178F, { 0x0178A } },
      { 0x017A3, { 0x017A2 } },
      { 0x017B7, { 0x00E34 } },
      { 0x017B8, { 0x00E35 } },
      { 0x017B9, { 0x00E36 } },
      { 0x017BA, { 0x00E37 } },
      { 0x017C6, { 0x0030A } },
      { 0x017CB, { 0x00E48 } },
      { 0x017D3, { 0x0030A } },
      { 0x017D4, { 0x00E2F } },
      { 0x017D5, { 0x00E5A } },
      { 0x017D9, { 0x00E4F } },
      { 0x017DA, { 0x00E5B } },
      { 0x017E0, { 0x0006F } },
      { 0x01803, { 0x0003A } },
      { 0x01809, { 0x0003A } },
      { 0x01855, { 0x01835 } },
      { 0x01896, { 0x0185C } },
      { 0x018B3, { 0x000B7, 0x018B1 } },
      { 0x018B6, { 0x000B7, 0x018B4 } },
      { 0x018B9, { 0x000B7, 0x018B8 } },
      { 0x018C2, { 0x000B7, 0x018C0 } },
      { 0x018C6, { 0x000B7, 0x014C2 } },
      { 0x018C7, { 0x014C2, 0x000B7 } },
      { 0x018C8, { 0x000B7, 0x014C3 } },
      { 0x018C9, { 0x014C3, 0x000B7 } },
      { 0x018CA, { 0x000B7, 0x014C4 } },
      { 0x018CB, { 0x014C4, 0x000B7 } },
      { 0x018CC, { 0x000B7, 0x014C5 } },
      { 0x018CD, { 0x014C5, 0x000B7 } },
      { 0x018CE, { 0x000B7, 0x01543 } },
      { 0x018CF, { 0x000B7, 0x01546 } },
      { 0x018D0, { 0x000B7, 0x01547 } },
      { 0x018D1, { 0x000B7, 0x01548 } },
      { 0x018D2, { 0x000B7, 0x01549 } },
      { 0x018D3, { 0x000B7, 0x0154B } },
      { 0x018DB, { 0x018F5 } },
      { 0x018DC, { 0x018DF, 0x0141E } },
      { 0x018DD, { 0x0141E, 0x018DF } },
      { 0x018E0, { 0x01543, 0x000B7 } },
      { 0x018E3, { 0x0155E, 0x000B7 } },
      { 0x018E4, { 0x01566, 0x000B7 } },
      { 0x018E5, { 0x0156B, 0x000B7 } },
      { 0x018E8, { 0x01586, 0x000B7 } },
      { 0x018EA, { 0x01597, 0x000B7 } },
      { 0x018ED, { 0x00460, 0x000B7 } },
      { 0x018F0, { 0x015F4, 0x000B7 } },
      { 0x018F2, { 0x0161B, 0x000B7 } },
      { 0x019D0, { 0x0199E } },
      { 0x019D1, { 0x019B1 } },
      { 0x01A80, { 0x01A45 } },
      { 0x01A90, { 0x01A45 } },
      { 0x01AA9, { 0x01AA8, 0x01AA8 } },
      { 0x01AAB, { 0x01AAA, 0x01AA8 } },
      { 0x01AB4, { 0x006DB } },
      { 0x01AB7, { 0x00328 } },
      { 0x01AD9, { 0x01AC6 } },
      { 0x01AE2, { 0x00304 } },
      { 0x01AE7, { 0x01AE5 } },
      { 0x01AE8, { 0x00304, 0x00304 } },
      { 0x01B52, { 0x01B0D } },
      { 0x01B53, { 0x01B11 } },
      { 0x01B58, { 0x01B28 } },
      { 0x01B5C, { 0x01B50 } },
      { 0x01B5F, { 0x01B5E, 0x01B5E } },
      { 0x01C3C, { 0x01C3B, 0x01C3B } },
      { 0x01C7F, { 0x01C7E, 0x01C7E } },
      { 0x01CD0, { 0x00302 } },
      { 0x01CD2, { 0x00304 } },
      { 0x01CD3, { 0x00027, 0x00027 } },
      { 0x01CD5, { 0x0032B } },
      { 0x01CD8, { 0x0032E } },
      { 0x01CD9, { 0x0032D } },
      { 0x01CDA, { 0x0030E } },
      { 0x01CDC, { 0x00329 } },
      { 0x01CDD, { 0x00323 } },
      { 0x01CDE, { 0x00324 } },
      { 0x01CED, { 0x00316 } },
      { 0x01D04, { 0x00063 } },
      { 0x01D08, { 0x0025C } },
      { 0x01D0B, { 0x00138 } },
      { 0x01D0D, { 0x0028D } },
      { 0x01D0F, { 0x0006F } },
      { 0x01D10, { 0x00254 } },
      { 0x01D11, { 0x0006F } },
      { 0x01D14, { 0x001DD, 0x0006F } },
      { 0x01D1C, { 0x00075 } },
      { 0x01D20, { 0x00076 } },
      { 0x01D21, { 0x00077 } },
      { 0x01D22, { 0x0007A } },
      { 0x01D24, { 0x001A8 } },
      { 0x01D26, { 0x00072 } },
      { 0x01D27, { 0x0028C } },
      { 0x01D28, { 0x003C0 } },
      { 0x01D29, { 0x01D18 } },
      { 0x01D2B, { 0x0043B } },
      { 0x01D3E, { 0x018D6 } },
      { 0x01D52, { 0x000BA } },
      { 0x01D6B, { 0x00075, 0x00065 } },
      { 0x01D6E, { 0x00066, 0x00334 } },
      { 0x01D6F, { 0x00072, 0x0006E, 0x00334 } },
      { 0x01D70, { 0x0006E, 0x00334 } },
      { 0x01D72, { 0x00072, 0x00334 } },
      { 0x01D73, { 0x0027E, 0x00334 } },
      { 0x01D74, { 0x00073, 0x00334 } },
      { 0x01D75, { 0x00074, 0x00334 } },
      { 0x01D76, { 0x0007A, 0x00334 } },
      { 0x01D78, { 0x01D34 } },
      { 0x01D7B, { 0x00069, 0x00335 } },
      { 0x01D7C, { 0x00069, 0x00335 } },
      { 0x01D7D, { 0x00070, 0x00335 } },
      { 0x01D7E, { 0x00075, 0x00335 } },
      { 0x01D7F, { 0x0028A, 0x00335 } },
      { 0x01D83, { 0x00067 } },
      { 0x01D8C, { 0x00079 } },
      { 0x01D90, { 0x0024B } },
      { 0x01D9F, { 0x01D4B } },
      { 0x01DA2, { 0x01D4D } },
      { 0x01DBA, { 0x018D4 } },
      { 0x01DBB, { 0x01646 } },
      { 0x01DE8, { 0x01ADA } },
      { 0x01DEE, { 0x02DEC } },
      { 0x01E43, { 0x0AB51 } },
      { 0x01E9A, { 0x01EA3 } },
      { 0x01E9D, { 0x00066 } },
      { 0x01E9E, { 0x000DF } },
      { 0x01EFF, { 0x00079 } },
      { 0x01F7D, { 0x01FF4 } },
      { 0x01FBD, { 0x00027 } },
      { 0x01FBE, { 0x00069 } },
      { 0x01FBF, { 0x00027 } },
      { 0x01FC0, { 0x0007E } },
      { 0x01FEF, { 0x00027 } },
      { 0x01FF6, { 0x013EF } },
      { 0x01FFD, { 0x00027 } },
      { 0x01FFE, { 0x00027 } },
      { 0x02000, { 0x00020 } },
      { 0x02001, { 0x00020 } },
      { 0x02002, { 0x00020 } },
      { 0x02003, { 0x00020 } },
      { 0x02004, { 0x00020 } },
      { 0x02005, { 0x00020 } },
      { 0x02006, { 0x00020 } },
      { 0x02007, { 0x00020 } },
      { 0x02008, { 0x00020 } },
      { 0x02009, { 0x00020 } },
      { 0x0200A, { 0x00020 } },
      { 0x02010, { 0x0002D } },
      { 0x02011, { 0x0002D } },
      { 0x02012, { 0x0002D } },
      { 0x02013, { 0x0002D } },
      { 0x02014, { 0x030FC } },
      { 0x02015, { 0x030FC } },
      { 0x02016, { 0x0006C, 0x0006C } },
      { 0x02018, { 0x00027 } },
      { 0x02019, { 0x00027 } },
      { 0x0201A, { 0x0002C } },
      { 0x0201B, { 0x00027 } },
      { 0x0201C, { 0x00027, 0x00027 } },
      { 0x0201D, { 0x00027, 0x00027 } },
      { 0x0201F, { 0x00027, 0x00027 } },
      { 0x02022, { 0x000B7 } },
      { 0x02024, { 0x0002E } },
      { 0x02025, { 0x0002E, 0x0002E } },
      { 0x02026, { 0x0002E, 0x0002E, 0x0002E } },
      { 0x02027, { 0x000B7 } },
      { 0x02028, { 0x00020 } },
      { 0x02029, { 0x00020 } },
      { 0x0202F, { 0x00020 } },
      { 0x02030, { 0x000BA, 0x0002F, 0x02080, 0x02080 } },
      { 0x02031, { 0x000BA, 0x0002F, 0x02080, 0x02080, 0x02080 } },
      { 0x02032, { 0x00027 } },
      { 0x02033, { 0x00027, 0x00027 } },
      { 0x02034, { 0x00027, 0x00027, 0x00027 } },
      { 0x02035, { 0x00027 } },
      { 0x02036, { 0x00027, 0x00027 } },
      { 0x02037, { 0x00027, 0x00027, 0x00027 } },
      { 0x02039, { 0x0003C } },
      { 0x0203A, { 0x0003E } },
      { 0x0203C, { 0x00021, 0x00021 } },
      { 0x0203E, { 0x002C9 } },
      { 0x02041, { 0x0002F } },
      { 0x02043, { 0x0002D } },
      { 0x02044, { 0x0002F } },
      { 0x02047, { 0x0003F, 0x0003F } },
      { 0x02048, { 0x0003F, 0x00021 } },
      { 0x02049, { 0x00021, 0x0003F } },
      { 0x0204E, { 0x0002A } },
      { 0x02052, { 0x000BA, 0x0002F, 0x02080 } },
      { 0x02053, { 0x0007E } },
      { 0x02057, { 0x00027, 0x00027, 0x00027, 0x00027 } },
      { 0x0205A, { 0x0003A } },
      { 0x0205D, { 0x02D57 } },
      { 0x0205E, { 0x02D42 } },
      { 0x0205F, { 0x00020 } },
      { 0x02070, { 0x000BA } },
      { 0x02079, { 0x0A770 } },
      { 0x020A1, { 0x00043, 0x020EB } },
      { 0x020A4, { 0x000A3 } },
      { 0x020A5, { 0x00072, 0x0006E, 0x00338 } },
      { 0x020A8, { 0x00052, 0x00073 } },
      { 0x020A9, { 0x00057, 0x00335 } },
      { 0x020AB, { 0x00064, 0x00335, 0x00331 } },
      { 0x020AC, { 0x0A792 } },
      { 0x020AD, { 0x0004B, 0x00335 } },
      { 0x020AE, { 0x00054, 0x020EB } },
      { 0x020B6, { 0x0006C, 0x00074 } },
      { 0x020BD, { 0x00554 } },
      { 0x020C1, { 0x00631, 0x00649, 0x0006C, 0x00644 } },
      { 0x020DB, { 0x006DB } },
      { 0x02100, { 0x00061, 0x0002F, 0x00063 } },
      { 0x02101, { 0x00061, 0x0002F, 0x00073 } },
      { 0x02102, { 0x00043 } },
      { 0x02103, { 0x000B0, 0x00043 } },
      { 0x02105, { 0x00063, 0x0002F, 0x0006F } },
      { 0x02106, { 0x00063, 0x0002F, 0x00075 } },
      { 0x02107, { 0x00190 } },
      { 0x02108, { 0x0042D } },
      { 0x02109, { 0x000B0, 0x00046 } },
      { 0x0210A, { 0x00067 } },
      { 0x0210B, { 0x00048 } },
      { 0x0210C, { 0x00048 } },
      { 0x0210D, { 0x00048 } },
      { 0x0210E, { 0x00068 } },
      { 0x0210F, { 0x00068, 0x00335 } },
      { 0x02110, { 0x0006C } },
      { 0x02111, { 0x0006C } },
      { 0x02112, { 0x0004C } },
      { 0x02113, { 0x0006C } },
      { 0x02115, { 0x0004E } },
      { 0x02116, { 0x0004E, 0x0006F } },
      { 0x02119, { 0x00050 } },
      { 0x0211A, { 0x00051 } },
      { 0x0211B, { 0x00052 } },
      { 0x0211C, { 0x00052 } },
      { 0x0211D, { 0x00052 } },
      { 0x02121, { 0x00054, 0x00045, 0x0004C } },
      { 0x02124, { 0x0005A } },
      { 0x02126, { 0x003A9 } },
      { 0x02127, { 0x001B1 } },
      { 0x02128, { 0x0005A } },
      { 0x02129, { 0x0027F } },
      { 0x0212A, { 0x0004B } },
      { 0x0212C, { 0x00042 } },
      { 0x0212D, { 0x00043 } },
      { 0x0212E, { 0x00065 } },
      { 0x0212F, { 0x00065 } },
      { 0x02130, { 0x00045 } },
      { 0x02131, { 0x00046 } },
      { 0x02133, { 0x0004D } },
      { 0x02134, { 0x0006F } },
      { 0x02135, { 0x005D0 } },
      { 0x02136, { 0x005D1 } },
      { 0x02137, { 0x005D2 } },
      { 0x02138, { 0x005D3 } },
      { 0x02139, { 0x00069 } },
      { 0x0213B, { 0x00046, 0x00041, 0x00058 } },
      { 0x0213C, { 0x003C0 } },
      { 0x0213D, { 0x00079 } },
      { 0x0213E, { 0x00393 } },
      { 0x0213F, { 0x003A0 } },
      { 0x02140, { 0x001A9 } },
      { 0x02141, { 0x0A4E8 } },
      { 0x02142, { 0x0A4F6 } },
      { 0x02143, { 0x16F00 } },
      { 0x02145, { 0x00044 } },
      { 0x02146, { 0x00064 } },
      { 0x02147, { 0x00065 } },
      { 0x02148, { 0x00069 } },
      { 0x02149, { 0x0006A } },
      { 0x02160, { 0x0006C } },
      { 0x02161, { 0x0006C, 0x0006C } },
      { 0x02162, { 0x0006C, 0x0006C, 0x0006C } },
      { 0x02163, { 0x0006C, 0x00056 } },
      { 0x02164, { 0x00056 } },
      { 0x02165, { 0x00056, 0x0006C } },
      { 0x02166, { 0x00056, 0x0006C, 0x0006C } },
      { 0x02167, { 0x00056, 0x0006C, 0x0006C, 0x0006C } },
      { 0x02168, { 0x0006C, 0x00058 } },
      { 0x02169, { 0x00058 } },
      { 0x0216A, { 0x00058, 0x0006C } },
      { 0x0216B, { 0x00058, 0x0006C, 0x0006C } },
      { 0x0216C, { 0x0004C } },
      { 0x0216D, { 0x00043 } },
      { 0x0216E, { 0x00044 } },
      { 0x0216F, { 0x0004D } },
      { 0x02170, { 0x00069 } },
      { 0x02171, { 0x00069, 0x00069 } },
      { 0x02172, { 0x00069, 0x00069, 0x00069 } },
      { 0x02173, { 0x00069, 0x00076 } },
      { 0x02174, { 0x00076 } },
      { 0x02175, { 0x00076, 0x00069 } },
      { 0x02176, { 0x00076, 0x00069, 0x00069 } },
      { 0x02177, { 0x00076, 0x00069, 0x00069, 0x00069 } },
      { 0x02178, { 0x00069, 0x00078 } },
      { 0x02179, { 0x00078 } },
      { 0x0217A, { 0x00078, 0x00069 } },
      { 0x0217B, { 0x00078, 0x00069, 0x00069 } },
      { 0x0217C, { 0x0006C } },
      { 0x0217D, { 0x00063 } },
      { 0x0217E, { 0x00064 } },
      { 0x0217F, { 0x00072, 0x0006E } },
      { 0x02183, { 0x00186 } },
      { 0x02184, { 0x00254 } },
      { 0x02191, { 0x016CF } },
      { 0x02195, { 0x016E8 } },
      { 0x021B5, { 0x021B2 } },
      { 0x021BA, { 0x1F10E } },
      { 0x021BE, { 0x016DA } },
      { 0x021BF, { 0x016D0 } },
      { 0x021C4, { 0x1F8D0 } },
      { 0x021CC, { 0x1F8D1 } },
      { 0x02200, { 0x02C6F } },
      { 0x02203, { 0x0018E } },
      { 0x02206, { 0x00394 } },
      { 0x0220F, { 0x003A0 } },
      { 0x02211, { 0x001A9 } },
      { 0x02212, { 0x0002D } },
      { 0x02214, { 0x0002B, 0x00307 } },
      { 0x02215, { 0x0002F } },
      { 0x02216, { 0x0005C } },
      { 0x02217, { 0x0002A } },
      { 0x02218, { 0x000B0 } },
      { 0x02219, { 0x000B7 } },
      { 0x0221E, { 0x0006F, 0x0006F } },
      { 0x02223, { 0x0006C } },
      { 0x02225, { 0x0006C, 0x0006C } },
      { 0x02228, { 0x00076 } },
      { 0x02229, { 0x00548 } },
      { 0x0222A, { 0x00055 } },
      { 0x0222B, { 0x00283 } },
      { 0x0222C, { 0x00283, 0x00283 } },
      { 0x0222D, { 0x00283, 0x00283, 0x00283 } },
      { 0x0222F, { 0x0222E, 0x0222E } },
      { 0x02230, { 0x0222E, 0x0222E, 0x0222E } },
      { 0x02236, { 0x0003A } },
      { 0x02238, { 0x0002D, 0x00307 } },
      { 0x0223C, { 0x0007E } },
      { 0x02250, { 0x0003D, 0x00307 } },
      { 0x02251, { 0x0003D, 0x00307, 0x00323 } },
      { 0x02257, { 0x0003D, 0x0030A } },
      { 0x02259, { 0x0003D, 0x00302 } },
      { 0x0225A, { 0x0003D, 0x00306 } },
      { 0x0225E, { 0x0003D, 0x0036B } },
      { 0x02263, { 0x02261 } },
      { 0x0226A, { 0x0003C, 0x0003C } },
      { 0x0226B, { 0x0003E, 0x0003E } },
      { 0x02282, { 0x01455 } },
      { 0x02283, { 0x01450 } },
      { 0x02295, { 0x102A8 } },
      { 0x02296, { 0x0004F, 0x00335 } },
      { 0x02299, { 0x00298 } },
      { 0x0229D, { 0x0004F, 0x00335 } },
      { 0x022A4, { 0x00054 } },
      { 0x022A5, { 0x0A4D5 } },
      { 0x022C0, { 0x02227 } },
      { 0x022C1, { 0x00076 } },
      { 0x022C2, { 0x00548 } },
      { 0x022C3, { 0x00055 } },
      { 0x022C4, { 0x016DC } },
      { 0x022C5, { 0x000B7 } },
      { 0x022C8, { 0x016DE } },
      { 0x022D6, { 0x0003C, 0x000B7 } },
      { 0x022D7, { 0x000B7, 0x0003E } },
      { 0x022D8, { 0x0003C, 0x0003C, 0x0003C } },
      { 0x022D9, { 0x0003E, 0x0003E, 0x0003E } },
      { 0x022EE, { 0x02D57 } },
      { 0x022EF, { 0x000B7, 0x000B7, 0x000B7 } },
      { 0x022F4, { 0x0A793 } },
      { 0x022FF, { 0x00045 } },
      { 0x02300, { 0x02205 } },
      { 0x02325, { 0x02324 } },
      { 0x02329, { 0x0276C } },
      { 0x0232A, { 0x0276D } },
      { 0x02341, { 0x0303C } },
      { 0x02359, { 0x00394, 0x00332 } },
      { 0x0235A, { 0x016DC, 0x00332 } },
      { 0x0235C, { 0x000B0, 0x00332 } },
      { 0x0235F, { 0x0229B } },
      { 0x02361, { 0x00054, 0x00308 } },
      { 0x02362, { 0x02207, 0x00308 } },
      { 0x02363, { 0x022C6, 0x00308 } },
      { 0x02364, { 0x000B0, 0x00308 } },
      { 0x02365, { 0x00629 } },
      { 0x02368, { 0x0007E, 0x00308 } },
      { 0x02369, { 0x01435 } },
      { 0x0236B, { 0x02207, 0x00334 } },
      { 0x0236C, { 0x0004F, 0x00335 } },
      { 0x02373, { 0x00069 } },
      { 0x02374, { 0x00070 } },
      { 0x02375, { 0x003C9 } },
      { 0x02376, { 0x00061, 0x00332 } },
      { 0x02377, { 0x0A793, 0x00332 } },
      { 0x02378, { 0x00069, 0x00332 } },
      { 0x02379, { 0x003C9, 0x00332 } },
      { 0x0237A, { 0x00061 } },
      { 0x0237F, { 0x016BD } },
      { 0x0239C, { 0x04E28 } },
      { 0x0239F, { 0x04E28 } },
      { 0x023A2, { 0x04E28 } },
      { 0x023A5, { 0x04E28 } },
      { 0x023AA, { 0x04E28 } },
      { 0x023AE, { 0x04E28 } },
      { 0x023C1, { 0x02355 } },
      { 0x023C2, { 0x0234E } },
      { 0x023C3, { 0x0234B } },
      { 0x023C6, { 0x0236D } },
      { 0x023E8, { 0x02081, 0x02080 } },
      { 0x023FC, { 0x023FB } },
      { 0x023FD, { 0x0006C } },
      { 0x023FE, { 0x0263E } },
      { 0x0244A, { 0x0005C, 0x0005C } },
      { 0x02460, { 0x02780 } },
      { 0x02461, { 0x02781 } },
      { 0x02462, { 0x02782 } },
      { 0x02463, { 0x02783 } },
      { 0x02464, { 0x02784 } },
      { 0x02465, { 0x02785 } },
      { 0x02466, { 0x02786 } },
      { 0x02467, { 0x02787 } },
      { 0x02468, { 0x02788 } },
      { 0x02469, { 0x02789 } },
      { 0x02474, { 0x00028, 0x0006C, 0x00029 } },
      { 0x02475, { 0x00028, 0x00032, 0x00029 } },
      { 0x02476, { 0x00028, 0x00033, 0x00029 } },
      { 0x02477, { 0x00028, 0x00034, 0x00029 } },
      { 0x02478, { 0x00028, 0x00035, 0x00029 } },
      { 0x02479, { 0x00028, 0x00036, 0x00029 } },
      { 0x0247A, { 0x00028, 0x00037, 0x00029 } },
      { 0x0247B, { 0x00028, 0x00038, 0x00029 } },
      { 0x0247C, { 0x00028, 0x00039, 0x00029 } },
      { 0x0247D, { 0x00028, 0x0006C, 0x0004F, 0x00029 } },
      { 0x0247E, { 0x00028, 0x0006C, 0x0006C, 0x00029 } },
      { 0x0247F, { 0x00028, 0x0006C, 0x00032, 0x00029 } },
      { 0x02480, { 0x00028, 0x0006C, 0x00033, 0x00029 } },
      { 0x02481, { 0x00028, 0x0006C, 0x00034, 0x00029 } },
      { 0x02482, { 0x00028, 0x0006C, 0x00035, 0x00029 } },
      { 0x02483, { 0x00028, 0x0006C, 0x00036, 0x00029 } },
      { 0x02484, { 0x00028, 0x0006C, 0x00037, 0x00029 } },
      { 0x02485, { 0x00028, 0x0006C, 0x00038, 0x00029 } },
      { 0x02486, { 0x00028, 0x0006C, 0x00039, 0x00029 } },
      { 0x02487, { 0x00028, 0x00032, 0x0004F, 0x00029 } },
      { 0x02488, { 0x0006C, 0x0002E } },
      { 0x02489, { 0x00032, 0x0002E } },
      { 0x0248A, { 0x00033, 0x0002E } },
      { 0x0248B, { 0x00034, 0x0002E } },
      { 0x0248C, { 0x00035, 0x0002E } },
      { 0x0248D, { 0x00036, 0x0002E } },
      { 0x0248E, { 0x00037, 0x0002E } },
      { 0x0248F, { 0x00038, 0x0002E } },
      { 0x02490, { 0x00039, 0x0002E } },
      { 0x02491, { 0x0006C, 0x0004F, 0x0002E } },
      { 0x02492, { 0x0006C, 0x0006C, 0x0002E } },
      { 0x02493, { 0x0006C, 0x00032, 0x0002E } },
      { 0x02494, { 0x0006C, 0x00033, 0x0002E } },
      { 0x02495, { 0x0006C, 0x00034, 0x0002E } },
      { 0x02496, { 0x0006C, 0x00035, 0x0002E } },
      { 0x02497, { 0x0006C, 0x00036, 0x0002E } },
      { 0x02498, { 0x0006C, 0x00037, 0x0002E } },
      { 0x02499, { 0x0006C, 0x00038, 0x0002E } },
      { 0x0249A, { 0x0006C, 0x00039, 0x0002E } },
      { 0x0249B, { 0x00032, 0x0004F, 0x0002E } },
      { 0x0249C, { 0x00028, 0x00061, 0x00029 } },
      { 0x0249D, { 0x00028, 0x00062, 0x00029 } },
      { 0x0249E, { 0x00028, 0x00063, 0x00029 } },
      { 0x0249F, { 0x00028, 0x00064, 0x00029 } },
      { 0x024A0, { 0x00028, 0x00065, 0x00029 } },
      { 0x024A1, { 0x00028, 0x00066, 0x00029 } },
      { 0x024A2, { 0x00028, 0x00067, 0x00029 } },
      { 0x024A3, { 0x00028, 0x00068, 0x00029 } },
      { 0x024A4, { 0x00028, 0x00069, 0x00029 } },
      { 0x024A5, { 0x00028, 0x0006A, 0x00029 } },
      { 0x024A6, { 0x00028, 0x0006B, 0x00029 } },
      { 0x024A7, { 0x00028, 0x0006C, 0x00029 } },
      { 0x024A8, { 0x00028, 0x00072, 0x0006E, 0x00029 } },
      { 0x024A9, { 0x00028, 0x0006E, 0x00029 } },
      { 0x024AA, { 0x00028, 0x0006F, 0x00029 } },
      { 0x024AB, { 0x00028, 0x00070, 0x00029 } },
      { 0x024AC, { 0x00028, 0x00071, 0x00029 } },
      { 0x024AD, { 0x00028, 0x00072, 0x00029 } },
      { 0x024AE, { 0x00028, 0x00073, 0x00029 } },
      { 0x024AF, { 0x00028, 0x00074, 0x00029 } },
      { 0x024B0, { 0x00028, 0x00075, 0x00029 } },
      { 0x024B1, { 0x00028, 0x00076, 0x00029 } },
      { 0x024B2, { 0x00028, 0x00077, 0x00029 } },
      { 0x024B3, { 0x00028, 0x00078, 0x00029 } },
      { 0x024B4, { 0x00028, 0x00079, 0x00029 } },
      { 0x024B5, { 0x00028, 0x0007A, 0x00029 } },
      { 0x024B8, { 0x000A9 } },
      { 0x024C5, { 0x02117 } },
      { 0x024C7, { 0x000AE } },
      { 0x024DB, { 0x024BE } },
      { 0x024EA, { 0x1F10D } },
      { 0x02500, { 0x030FC } },
      { 0x02501, { 0x030FC } },
      { 0x02503, { 0x02502 } },
      { 0x0250F, { 0x0250C } },
      { 0x02523, { 0x0251C } },
      { 0x0256A, { 0x001C2 } },
      { 0x02571, { 0x0002F } },
      { 0x02573, { 0x00058 } },
      { 0x02588, { 0x0220E } },
      { 0x02590, { 0x0258C } },
      { 0x02594, { 0x002C9 } },
      { 0x02597, { 0x02596 } },
      { 0x0259D, { 0x02598 } },
      { 0x025A0, { 0x0220E } },
      { 0x025B1, { 0x023E5 } },
      { 0x025B3, { 0x00394 } },
      { 0x025B7, { 0x022B3 } },
      { 0x025B8, { 0x025B6 } },
      { 0x025BA, { 0x025B6 } },
      { 0x025BD, { 0x102BC } },
      { 0x025C1, { 0x022B2 } },
      { 0x025C7, { 0x016DC } },
      { 0x025CA, { 0x016DC } },
      { 0x025CB, { 0x000B0 } },
      { 0x025CE, { 0x0233E } },
      { 0x025E0, { 0x02312 } },
      { 0x025E6, { 0x000B0 } },
      { 0x02609, { 0x00298 } },
      { 0x02610, { 0x025A1 } },
      { 0x02625, { 0x1099E } },
      { 0x02630, { 0x0039E } },
      { 0x02638, { 0x02388 } },
      { 0x0264E, { 0x0224F } },
      { 0x02657, { 0x1FA55 } },
      { 0x0265D, { 0x1FA57 } },
      { 0x02662, { 0x016DC } },
      { 0x02669, { 0x1D158, 0x1D165 } },
      { 0x0266A, { 0x1D158, 0x1D165, 0x1D16E } },
      { 0x026AC, { 0x00970 } },
      { 0x02768, { 0x00028 } },
      { 0x02769, { 0x00029 } },
      { 0x0276E, { 0x0003C } },
      { 0x0276F, { 0x0003E } },
      { 0x02772, { 0x00028 } },
      { 0x02773, { 0x00029 } },
      { 0x02774, { 0x0007B } },
      { 0x02775, { 0x0007D } },
      { 0x02795, { 0x0002B } },
      { 0x02796, { 0x0002D } },
      { 0x02797, { 0x000F7 } },
      { 0x027C2, { 0x0A4D5 } },
      { 0x027C8, { 0x0005C, 0x01455 } },
      { 0x027C9, { 0x01450, 0x0002F } },
      { 0x027CB, { 0x0002F } },
      { 0x027CD, { 0x0005C } },
      { 0x027D9, { 0x00054 } },
      { 0x027E8, { 0x0276C } },
      { 0x027E9, { 0x0276D } },
      { 0x028FF, { 0x1CEE0 } },
      { 0x0292B, { 0x00078 } },
      { 0x0292C, { 0x00078 } },
      { 0x02963, { 0x016D0, 0x016DA } },
      { 0x02965, { 0x021C3, 0x021C2 } },
      { 0x0296E, { 0x016D0, 0x021C2 } },
      { 0x0296F, { 0x021C3, 0x016DA } },
      { 0x02999, { 0x02D42 } },
      { 0x029B0, { 0x02349 } },
      { 0x029B5, { 0x1CEF0 } },
      { 0x029BE, { 0x0233E } },
      { 0x029C4, { 0x0303C } },
      { 0x029C5, { 0x02342 } },
      { 0x029C7, { 0x0233B } },
      { 0x029D6, { 0x102C0 } },
      { 0x029D9, { 0x0299A } },
      { 0x029F4, { 0x0003A, 0x02192 } },
      { 0x029F5, { 0x0005C } },
      { 0x029F6, { 0x0002F, 0x00304 } },
      { 0x029F8, { 0x0002F } },
      { 0x029F9, { 0x0005C } },
      { 0x02A00, { 0x00298 } },
      { 0x02A01, { 0x102A8 } },
      { 0x02A02, { 0x02297 } },
      { 0x02A03, { 0x0228D } },
      { 0x02A04, { 0x0228E } },
      { 0x02A05, { 0x02293 } },
      { 0x02A06, { 0x02294 } },
      { 0x02A0C, { 0x00283, 0x00283, 0x00283, 0x00283 } },
      { 0x02A1D, { 0x016DE } },
      { 0x02A20, { 0x0003E, 0x0003E } },
      { 0x02A21, { 0x016DA } },
      { 0x02A22, { 0x0002B, 0x0030A } },
      { 0x02A23, { 0x0002B, 0x00302 } },
      { 0x02A24, { 0x0002B, 0x00303 } },
      { 0x02A25, { 0x0002B, 0x00323 } },
      { 0x02A26, { 0x0002B, 0x00330 } },
      { 0x02A27, { 0x0002B, 0x02082 } },
      { 0x02A29, { 0x0002D, 0x00313 } },
      { 0x02A2A, { 0x0002D, 0x00323 } },
      { 0x02A2F, { 0x00078 } },
      { 0x02A30, { 0x00078, 0x00307 } },
      { 0x02A3D, { 0x02319 } },
      { 0x02A3E, { 0x02A1F } },
      { 0x02A3F, { 0x02210 } },
      { 0x02A6A, { 0x0007E, 0x00307 } },
      { 0x02A6E, { 0x0003D, 0x020F0 } },
      { 0x02A74, { 0x0003A, 0x0003A, 0x0003D } },
      { 0x02A75, { 0x0003D, 0x0003D } },
      { 0x02A76, { 0x0003D, 0x0003D, 0x0003D } },
      { 0x02AA5, { 0x0003E, 0x0003C } },
      { 0x02AAA, { 0x015D5 } },
      { 0x02AAB, { 0x015D2 } },
      { 0x02AD7, { 0x01450, 0x01455 } },
      { 0x02AFB, { 0x0002F, 0x0002F, 0x0002F } },
      { 0x02AFD, { 0x0002F, 0x0002F } },
      { 0x02B96, { 0x0003D, 0x01AB2 } },
      { 0x02BEC, { 0x0219E } },
      { 0x02BED, { 0x0219F } },
      { 0x02BEE, { 0x021A0 } },
      { 0x02BEF, { 0x021A1 } },
      { 0x02C67, { 0x00048, 0x00329 } },
      { 0x02C69, { 0x0004B, 0x00329 } },
      { 0x02C82, { 0x00042 } },
      { 0x02C83, { 0x00299 } },
      { 0x02C84, { 0x00393 } },
      { 0x02C85, { 0x00072 } },
      { 0x02C86, { 0x00394 } },
      { 0x02C88, { 0x0A792 } },
      { 0x02C89, { 0x0A793 } },
      { 0x02C8B, { 0x003C2 } },
      { 0x02C8C, { 0x02C6B } },
      { 0x02C8D, { 0x02C6C } },
      { 0x02C8E, { 0x00048 } },
      { 0x02C8F, { 0x0029C } },
      { 0x02C90, { 0x0004F, 0x00335 } },
      { 0x02C91, { 0x0006F, 0x00335 } },
      { 0x02C92, { 0x0006C } },
      { 0x02C93, { 0x00069 } },
      { 0x02C94, { 0x0004B } },
      { 0x02C95, { 0x00138 } },
      { 0x02C96, { 0x003BB } },
      { 0x02C97, { 0x0028C } },
      { 0x02C98, { 0x0004D } },
      { 0x02C99, { 0x0028D } },
      { 0x02C9A, { 0x0004E } },
      { 0x02C9B, { 0x00274 } },
      { 0x02C9C, { 0x00033 } },
      { 0x02C9D, { 0x00293 } },
      { 0x02C9E, { 0x0004F } },
      { 0x02C9F, { 0x0006F } },
      { 0x02CA0, { 0x003A0 } },
      { 0x02CA1, { 0x003C0 } },
      { 0x02CA2, { 0x00050 } },
      { 0x02CA3, { 0x00070 } },
      { 0x02CA4, { 0x00043 } },
      { 0x02CA5, { 0x00063 } },
      { 0x02CA6, { 0x00054 } },
      { 0x02CA7, { 0x01D1B } },
      { 0x02CA8, { 0x00059 } },
      { 0x02CA9, { 0x00079 } },
      { 0x02CAA, { 0x003A6 } },
      { 0x02CAB, { 0x00278 } },
      { 0x02CAC, { 0x00058 } },
      { 0x02CAD, { 0x003C7 } },
      { 0x02CAE, { 0x003A8 } },
      { 0x02CAF, { 0x003C8 } },
      { 0x02CB0, { 0x0A64C } },
      { 0x02CB1, { 0x003C9 } },
      { 0x02CB2, { 0x0002D, 0x00307 } },
      { 0x02CB3, { 0x0002D, 0x00307 } },
      { 0x02CB4, { 0x0003C, 0x000B7 } },
      { 0x02CB5, { 0x0003C, 0x000B7 } },
      { 0x02CB6, { 0x0039E } },
      { 0x02CB7, { 0x02261 } },
      { 0x02CBA, { 0x0002D } },
      { 0x02CBB, { 0x0002D } },
      { 0x02CBC, { 0x00428 } },
      { 0x02CBD, { 0x00077 } },
      { 0x02CC0, { 0x00554 } },
      { 0x02CC1, { 0x003FC } },
      { 0x02CC4, { 0x00033 } },
      { 0x02CC5, { 0x0021D } },
      { 0x02CC6, { 0x0002F } },
      { 0x02CC7, { 0x0002F } },
      { 0x02CCA, { 0x00039 } },
      { 0x02CCB, { 0x00039 } },
      { 0x02CCC, { 0x00033 } },
      { 0x02CCD, { 0x0021D } },
      { 0x02CCE, { 0x00050 } },
      { 0x02CCF, { 0x00070 } },
      { 0x02CD0, { 0x0004C } },
      { 0x02CD1, { 0x0029F } },
      { 0x02CD2, { 0x00036 } },
      { 0x02CD3, { 0x00036 } },
      { 0x02CDC, { 0x00036 } },
      { 0x02CDD, { 0x01E9F } },
      { 0x02CE0, { 0x00278 } },
      { 0x02CE1, { 0x00278 } },
      { 0x02CE4, { 0x003D7 } },
      { 0x02CE8, { 0x00554 } },
      { 0x02CE9, { 0x02627 } },
      { 0x02CF9, { 0x0005C, 0x0005C } },
      { 0x02D31, { 0x0004F, 0x00335 } },
      { 0x02D37, { 0x00245 } },
      { 0x02D38, { 0x00056 } },
      { 0x02D39, { 0x00045 } },
      { 0x02D3A, { 0x0018E } },
      { 0x02D41, { 0x0004F, 0x00338 } },
      { 0x02D48, { 0x000B7, 0x000B7, 0x000B7 } },
      { 0x02D49, { 0x001A9 } },
      { 0x02D4F, { 0x0006C } },
      { 0x02D51, { 0x00021 } },
      { 0x02D54, { 0x0004F } },
      { 0x02D55, { 0x00051 } },
      { 0x02D59, { 0x00298 } },
      { 0x02D5D, { 0x00058 } },
      { 0x02D60, { 0x00394 } },
      { 0x02D63, { 0x016EF } },
      { 0x02DE8, { 0x01DDF } },
      { 0x02DEA, { 0x0030A } },
      { 0x02DED, { 0x00368 } },
      { 0x02DEE, { 0x01ADB } },
      { 0x02DEF, { 0x0036F } },
      { 0x02DF6, { 0x00363 } },
      { 0x02DF7, { 0x00364 } },
      { 0x02E1A, { 0x0002D, 0x00308 } },
      { 0x02E1E, { 0x0007E, 0x00307 } },
      { 0x02E1F, { 0x0007E, 0x00323 } },
      { 0x02E26, { 0x01455 } },
      { 0x02E27, { 0x01450 } },
      { 0x02E28, { 0x00028, 0x00028 } },
      { 0x02E29, { 0x00029, 0x00029 } },
      { 0x02E2A, { 0x02235 } },
      { 0x02E2B, { 0x02234 } },
      { 0x02E2C, { 0x02237 } },
      { 0x02E2E, { 0x0061F } },
      { 0x02E30, { 0x000B0 } },
      { 0x02E31, { 0x000B7 } },
      { 0x02E32, { 0x0060C } },
      { 0x02E35, { 0x0061B } },
      { 0x02E39, { 0x01E9F } },
      { 0x02E3D, { 0x02D42 } },
      { 0x02E3F, { 0x000B6 } },
      { 0x02E40, { 0x0003D } },
      { 0x02E82, { 0x04E5B } },
      { 0x02E83, { 0x04E5A } },
      { 0x02E85, { 0x04EBB } },
      { 0x02E89, { 0x05202 } },
      { 0x02E8B, { 0x0353E } },
      { 0x02E8E, { 0x05140 } },
      { 0x02E8F, { 0x05C23 } },
      { 0x02E90, { 0x05C22 } },
      { 0x02E92, { 0x05DF3 } },
      { 0x02E93, { 0x05E7A } },
      { 0x02E94, { 0x05F51 } },
      { 0x02E96, { 0x05FC4 } },
      { 0x02E97, { 0x038FA } },
      { 0x02E98, { 0x0624C } },
      { 0x02E99, { 0x06535 } },
      { 0x02E9B, { 0x065E1 } },
      { 0x02E9E, { 0x06B7A } },
      { 0x02E9F, { 0x06BCD } },
      { 0x02EA0, { 0x06C11 } },
      { 0x02EA1, { 0x06C35 } },
      { 0x02EA2, { 0x06C3A } },
      { 0x02EA3, { 0x0706C } },
      { 0x02EA4, { 0x0722B } },
      { 0x02EA6, { 0x04E2C } },
      { 0x02EA8, { 0x072AD } },
      { 0x02EAB, { 0x07F52 } },
      { 0x02EAD, { 0x0793B } },
      { 0x02EAF, { 0x07CF9 } },
      { 0x02EB1, { 0x07F53 } },
      { 0x02EB2, { 0x07F52 } },
      { 0x02EB9, { 0x08002 } },
      { 0x02EBA, { 0x08080 } },
      { 0x02EBE, { 0x08279 } },
      { 0x02EBF, { 0x08279 } },
      { 0x02EC0, { 0x08279 } },
      { 0x02EC1, { 0x0864E } },
      { 0x02EC2, { 0x08864 } },
      { 0x02EC3, { 0x08980 } },
      { 0x02EC4, { 0x0897F } },
      { 0x02EC5, { 0x089C1 } },
      { 0x02EC8, { 0x08BA0 } },
      { 0x02EC9, { 0x08D1D } },
      { 0x02ECB, { 0x08F66 } },
      { 0x02ECC, { 0x08FB6 } },
      { 0x02ECD, { 0x08FB6 } },
      { 0x02ECF, { 0x0961D } },
      { 0x02ED0, { 0x09485 } },
      { 0x02ED1, { 0x01110, 0x030FC, 0x01102, 0x0110C } },
      { 0x02ED2, { 0x09578 } },
      { 0x02ED3, { 0x0957F } },
      { 0x02ED4, { 0x095E8 } },
      { 0x02ED6, { 0x0961D } },
      { 0x02ED8, { 0x09752 } },
      { 0x02ED9, { 0x097E6 } },
      { 0x02EDA, { 0x09875 } },
      { 0x02EDB, { 0x098CE } },
      { 0x02EDC, { 0x098DE } },
      { 0x02EDD, { 0x098DF } },
      { 0x02EDF, { 0x098E0 } },
      { 0x02EE0, { 0x09963 } },
      { 0x02EE2, { 0x09A6C } },
      { 0x02EE4, { 0x09B3C } },
      { 0x02EE5, { 0x09C7C } },
      { 0x02EE8, { 0x09EA6 } },
      { 0x02EE9, { 0x09EC4 } },
      { 0x02EEB, { 0x06589 } },
      { 0x02EEC, { 0x09F50 } },
      { 0x02EED, { 0x06B6F } },
      { 0x02EEE, { 0x09F7F } },
      { 0x02EEF, { 0x07ADC } },
      { 0x02EF0, { 0x09F99 } },
      { 0x02EF2, { 0x04E80 } },
      { 0x02EF3, { 0x09F9F } },
      { 0x02F00, { 0x030FC } },
      { 0x02F01, { 0x04E28 } },
      { 0x02F02, { 0x0005C } },
      { 0x02F03, { 0x0002F } },
      { 0x02F04, { 0x04E59 } },
      { 0x02F05, { 0x04E85 } },
      { 0x02F06, { 0x04E8C } },
      { 0x02F07, { 0x04EA0 } },
      { 0x02F08, { 0x04EBA } },
      { 0x02F09, { 0x0513F } },
      { 0x02F0A, { 0x05165 } },
      { 0x02F0B, { 0x0516B } },
      { 0x02F0C, { 0x05182 } },
      { 0x02F0D, { 0x05196 } },
      { 0x02F0E, { 0x051AB } },
      { 0x02F0F, { 0x051E0 } },
      { 0x02F10, { 0x051F5 } },
      { 0x02F11, { 0x05200 } },
      { 0x02F12, { 0x0529B } },
      { 0x02F13, { 0x052F9 } },
      { 0x02F14, { 0x05315 } },
      { 0x02F15, { 0x0531A } },
      { 0x02F16, { 0x05338 } },
      { 0x02F17, { 0x05341 } },
      { 0x02F18, { 0x0535C } },
      { 0x02F19, { 0x05369 } },
      { 0x02F1A, { 0x05382 } },
      { 0x02F1B, { 0x053B6 } },
      { 0x02F1C, { 0x053C8 } },
      { 0x02F1D, { 0x053E3 } },
      { 0x02F1E, { 0x053E3 } },
      { 0x02F1F, { 0x0571F } },
      { 0x02F20, { 0x0571F } },
      { 0x02F21, { 0x05902 } },
      { 0x02F22, { 0x0590A } },
      { 0x02F23, { 0x05915 } },
      { 0x02F24, { 0x05927 } },
      { 0x02F25, { 0x05973 } },
      { 0x02F26, { 0x05B50 } },
      { 0x02F27, { 0x05B80 } },
      { 0x02F28, { 0x05BF8 } },
      { 0x02F29, { 0x05C0F } },
      { 0x02F2A, { 0x05C22 } },
      { 0x02F2B, { 0x05C38 } },
      { 0x02F2C, { 0x05C6E } },
      { 0x02F2D, { 0x05C71 } },
      { 0x02F2E, { 0x05DDB } },
      { 0x02F2F, { 0x05DE5 } },
      { 0x02F30, { 0x05DF1 } },
      { 0x02F31, { 0x05DFE } },
      { 0x02F32, { 0x05E72 } },
      { 0x02F33, { 0x05E7A } },
      { 0x02F34, { 0x05E7F } },
      { 0x02F35, { 0x05EF4 } },
      { 0x02F36, { 0x05EFE } },
      { 0x02F37, { 0x05F0B } },
      { 0x02F38, { 0x05F13 } },
      { 0x02F39, { 0x05F50 } },
      { 0x02F3A, { 0x05F61 } },
      { 0x02F3B, { 0x05F73 } },
      { 0x02F3C, { 0x05FC3 } },
      { 0x02F3D, { 0x06208 } },
      { 0x02F3E, { 0x06236 } },
      { 0x02F3F, { 0x0624B } },
      { 0x02F40, { 0x0652F } },
      { 0x02F41, { 0x06534 } },
      { 0x02F42, { 0x06587 } },
      { 0x02F43, { 0x06597 } },
      { 0x02F44, { 0x065A4 } },
      { 0x02F45, { 0x065B9 } },
      { 0x02F46, { 0x065E0 } },
      { 0x02F47, { 0x065E5 } },
      { 0x02F48, { 0x066F0 } },
      { 0x02F49, { 0x06708 } },
      { 0x02F4A, { 0x06728 } },
      { 0x02F4B, { 0x06B20 } },
      { 0x02F4C, { 0x06B62 } },
      { 0x02F4D, { 0x06B79 } },
      { 0x02F4E, { 0x06BB3 } },
      { 0x02F4F, { 0x06BCB } },
      { 0x02F50, { 0x06BD4 } },
      { 0x02F51, { 0x06BDB } },
      { 0x02F52, { 0x06C0F } },
      { 0x02F53, { 0x06C14 } },
      { 0x02F54, { 0x06C34 } },
      { 0x02F55, { 0x0706B } },
      { 0x02F56, { 0x0722A } },
      { 0x02F57, { 0x07236 } },
      { 0x02F58, { 0x0723B } },
      { 0x02F59, { 0x01102, 0x0116E, 0x04E28 } },
      { 0x02F5A, { 0x07247 } },
      { 0x02F5B, { 0x07259 } },
      { 0x02F5C, { 0x0725B } },
      { 0x02F5D, { 0x072AC } },
      { 0x02F5E, { 0x07384 } },
      { 0x02F5F, { 0x07389 } },
      { 0x02F60, { 0x074DC } },
      { 0x02F61, { 0x074E6 } },
      { 0x02F62, { 0x07518 } },
      { 0x02F63, { 0x0751F } },
      { 0x02F64, { 0x07528 } },
      { 0x02F65, { 0x07530 } },
      { 0x02F66, { 0x0758B } },
      { 0x02F67, { 0x07592 } },
      { 0x02F68, { 0x07676 } },
      { 0x02F69, { 0x0767D } },
      { 0x02F6A, { 0x076AE } },
      { 0x02F6B, { 0x076BF } },
      { 0x02F6C, { 0x076EE } },
      { 0x02F6D, { 0x077DB } },
      { 0x02F6E, { 0x077E2 } },
      { 0x02F6F, { 0x077F3 } },
      { 0x02F70, { 0x0793A } },
      { 0x02F71, { 0x079B8 } },
      { 0x02F72, { 0x079BE } },
      { 0x02F73, { 0x07A74 } },
      { 0x02F74, { 0x07ACB } },
      { 0x02F75, { 0x07AF9 } },
      { 0x02F76, { 0x07C73 } },
      { 0x02F77, { 0x07CF8 } },
      { 0x02F78, { 0x07F36 } },
      { 0x02F79, { 0x07F51 } },
      { 0x02F7A, { 0x07F8A } },
      { 0x02F7B, { 0x07FBD } },
      { 0x02F7C, { 0x08001 } },
      { 0x02F7D, { 0x0800C } },
      { 0x02F7E, { 0x08012 } },
      { 0x02F7F, { 0x08033 } },
      { 0x02F80, { 0x0807F } },
      { 0x02F81, { 0x08089 } },
      { 0x02F82, { 0x081E3 } },
      { 0x02F83, { 0x081EA } },
      { 0x02F84, { 0x081F3 } },
      { 0x02F85, { 0x081FC } },
      { 0x02F86, { 0x0820C } },
      { 0x02F87, { 0x0821B } },
      { 0x02F88, { 0x0821F } },
      { 0x02F89, { 0x0826E } },
      { 0x02F8A, { 0x08272 } },
      { 0x02F8B, { 0x08278 } },
      { 0x02F8C, { 0x0864D } },
      { 0x02F8D, { 0x0866B } },
      { 0x02F8E, { 0x08840 } },
      { 0x02F8F, { 0x0884C } },
      { 0x02F90, { 0x08863 } },
      { 0x02F91, { 0x0897E } },
      { 0x02F92, { 0x0898B } },
      { 0x02F93, { 0x089D2 } },
      { 0x02F94, { 0x08A00 } },
      { 0x02F95, { 0x08C37 } },
      { 0x02F96, { 0x08C46 } },
      { 0x02F97, { 0x08C55 } },
      { 0x02F98, { 0x08C78 } },
      { 0x02F99, { 0x08C9D } },
      { 0x02F9A, { 0x08D64 } },
      { 0x02F9B, { 0x08D70 } },
      { 0x02F9C, { 0x08DB3 } },
      { 0x02F9D, { 0x08EAB } },
      { 0x02F9E, { 0x08ECA } },
      { 0x02F9F, { 0x08F9B } },
      { 0x02FA0, { 0x08FB0 } },
      { 0x02FA1, { 0x08FB5 } },
      { 0x02FA2, { 0x09091 } },
      { 0x02FA3, { 0x09149 } },
      { 0x02FA4, { 0x091C6 } },
      { 0x02FA5, { 0x091CC } },
      { 0x02FA6, { 0x091D1 } },
      { 0x02FA7, { 0x01110, 0x030FC, 0x01102, 0x0110C } },
      { 0x02FA8, { 0x09580 } },
      { 0x02FA9, { 0x0961C } },
      { 0x02FAA, { 0x096B6 } },
      { 0x02FAB, { 0x096B9 } },
      { 0x02FAC, { 0x096E8 } },
      { 0x02FAD, { 0x09751 } },
      { 0x02FAE, { 0x0975E } },
      { 0x02FAF, { 0x09762 } },
      { 0x02FB0, { 0x09769 } },
      { 0x02FB1, { 0x097CB } },
      { 0x02FB2, { 0x097ED } },
      { 0x02FB3, { 0x097F3 } },
      { 0x02FB4, { 0x09801 } },
      { 0x02FB5, { 0x098A8 } },
      { 0x02FB6, { 0x098DB } },
      { 0x02FB7, { 0x098DF } },
      { 0x02FB8, { 0x09996 } },
      { 0x02FB9, { 0x09999 } },
      { 0x02FBA, { 0x099AC } },
      { 0x02FBB, { 0x09AA8 } },
      { 0x02FBC, { 0x09AD8 } },
      { 0x02FBD, { 0x09ADF } },
      { 0x02FBE, { 0x09B25 } },
      { 0x02FBF, { 0x09B2F } },
      { 0x02FC0, { 0x09B32 } },
      { 0x02FC1, { 0x09B3C } },
      { 0x02FC2, { 0x09B5A } },
      { 0x02FC3, { 0x09CE5 } },
      { 0x02FC4, { 0x09E75 } },
      { 0x02FC5, { 0x09E7F } },
      { 0x02FC6, { 0x09EA5 } },
      { 0x02FC7, { 0x09EBB } },
      { 0x02FC8, { 0x09EC3 } },
      { 0x02FC9, { 0x09ECD } },
      { 0x02FCA, { 0x09ED1 } },
      { 0x02FCB, { 0x09EF9 } },
      { 0x02FCC, { 0x09EFD } },
      { 0x02FCD, { 0x09F0E } },
      { 0x02FCE, { 0x09F13 } },
      { 0x02FCF, { 0x09F20 } },
      { 0x02FD0, { 0x09F3B } },
      { 0x02FD1, { 0x09F4A } },
      { 0x02FD2, { 0x09F52 } },
      { 0x02FD3, { 0x09F8D } },
      { 0x02FD4, { 0x09F9C } },
      { 0x02FD5, { 0x09FA0 } },
      { 0x03002, { 0x002F3 } },
      { 0x03003, { 0x00027, 0x00027 } },
      { 0x03007, { 0x0004F } },
      { 0x03008, { 0x0276C } },
      { 0x03009, { 0x0276D } },
      { 0x03012, { 0x020B8 } },
      { 0x03014, { 0x00028 } },
      { 0x03015, { 0x00029 } },
      { 0x0301A, { 0x027E6 } },
      { 0x0301B, { 0x027E7 } },
      { 0x0302C, { 0x00309 } },
      { 0x0302D, { 0x00325 } },
      { 0x03033, { 0x0002F } },
      { 0x03036, { 0x020B8 } },
      { 0x03038, { 0x05341 } },
      { 0x03039, { 0x05344 } },
      { 0x0303A, { 0x05345 } },
      { 0x0304F, { 0x0276C } },
      { 0x0309A, { 0x0030A } },
      { 0x0309B, { 0x0FF9E } },
      { 0x0309C, { 0x0FF9F } },
      { 0x030A0, { 0x0003D } },
      { 0x030A4, { 0x04EBB } },
      { 0x030A8, { 0x05DE5 } },
      { 0x030AB, { 0x0529B } },
      { 0x030BF, { 0x05915 } },
      { 0x030C8, { 0x0535C } },
      { 0x030CB, { 0x04E8C } },
      { 0x030CE, { 0x0002F } },
      { 0x030CF, { 0x0516B } },
      { 0x030D8, { 0x03078 } },
      { 0x030ED, { 0x053E3 } },
      { 0x030FB, { 0x000B7 } },
      { 0x03126, { 0x0513F } },
      { 0x03131, { 0x01100 } },
      { 0x03132, { 0x01100, 0x01100 } },
      { 0x03133, { 0x01100, 0x01109 } },
      { 0x03134, { 0x01102 } },
      { 0x03135, { 0x01102, 0x0110C } },
      { 0x03136, { 0x01102, 0x01112 } },
      { 0x03137, { 0x01103 } },
      { 0x03138, { 0x01103, 0x01103 } },
      { 0x03139, { 0x01105 } },
      { 0x0313A, { 0x01105, 0x01100 } },
      { 0x0313B, { 0x01105, 0x01106 } },
      { 0x0313C, { 0x01105, 0x01107 } },
      { 0x0313D, { 0x01105, 0x01109 } },
      { 0x0313E, { 0x01105, 0x01110 } },
      { 0x0313F, { 0x01105, 0x01111 } },
      { 0x03140, { 0x01105, 0x01112 } },
      { 0x03141, { 0x01106 } },
      { 0x03142, { 0x01107 } },
      { 0x03143, { 0x01107, 0x01107 } },
      { 0x03144, { 0x01107, 0x01109 } },
      { 0x03145, { 0x01109 } },
      { 0x03146, { 0x01109, 0x01109 } },
      { 0x03147, { 0x0110B } },
      { 0x03148, { 0x0110C } },
      { 0x03149, { 0x0110C, 0x0110C } },
      { 0x0314A, { 0x0110E } },
      { 0x0314B, { 0x0110F } },
      { 0x0314C, { 0x01110 } },
      { 0x0314D, { 0x01111 } },
      { 0x0314E, { 0x01112 } },
      { 0x0314F, { 0x01161 } },
      { 0x03150, { 0x01161, 0x04E28 } },
      { 0x03151, { 0x01163 } },
      { 0x03152, { 0x01163, 0x04E28 } },
      { 0x03153, { 0x01165 } },
      { 0x03154, { 0x01165, 0x04E28 } },
      { 0x03155, { 0x01167 } },
      { 0x03156, { 0x01167, 0x04E28 } },
      { 0x03157, { 0x01169 } },
      { 0x03158, { 0x01169, 0x01161 } },
      { 0x03159, { 0x01169, 0x01161, 0x04E28 } },
      { 0x0315A, { 0x01169, 0x04E28 } },
      { 0x0315B, { 0x0116D } },
      { 0x0315C, { 0x0116E } },
      { 0x0315D, { 0x0116E, 0x01165 } },
      { 0x0315E, { 0x0116E, 0x01165, 0x04E28 } },
      { 0x0315F, { 0x0116E, 0x04E28 } },
      { 0x03160, { 0x01172 } },
      { 0x03161, { 0x030FC } },
      { 0x03162, { 0x030FC, 0x04E28 } },
      { 0x03163, { 0x04E28 } },
      { 0x03164, { 0x01160 } },
      { 0x03165, { 0x01102, 0x01102 } },
      { 0x03166, { 0x01102, 0x01103 } },
      { 0x03167, { 0x01102, 0x01109 } },
      { 0x03168, { 0x01102, 0x01140 } },
      { 0x03169, { 0x01105, 0x01100, 0x01109 } },
      { 0x0316A, { 0x01105, 0x01103 } },
      { 0x0316B, { 0x01105, 0x01107, 0x01109 } },
      { 0x0316C, { 0x01105, 0x01140 } },
      { 0x0316D, { 0x01105, 0x01159 } },
      { 0x0316E, { 0x01106, 0x01107 } },
      { 0x0316F, { 0x01106, 0x01109 } },
      { 0x03170, { 0x01106, 0x01140 } },
      { 0x03171, { 0x01106, 0x0110B } },
      { 0x03172, { 0x01107, 0x01100 } },
      { 0x03173, { 0x01107, 0x01103 } },
      { 0x03174, { 0x01107, 0x01109, 0x01100 } },
      { 0x03175, { 0x01107, 0x01109, 0x01103 } },
      { 0x03176, { 0x01107, 0x0110C } },
      { 0x03177, { 0x01107, 0x01110 } },
      { 0x03178, { 0x01107, 0x0110B } },
      { 0x03179, { 0x01107, 0x01107, 0x0110B } },
      { 0x0317A, { 0x01109, 0x01100 } },
      { 0x0317B, { 0x01109, 0x01102 } },
      { 0x0317C, { 0x01109, 0x01103 } },
      { 0x0317D, { 0x01109, 0x01107 } },
      { 0x0317E, { 0x01109, 0x0110C } },
      { 0x0317F, { 0x01140 } },
      { 0x03180, { 0x0110B, 0x0110B } },
      { 0x03181, { 0x0114C } },
      { 0x03182, { 0x0110B, 0x01109 } },
      { 0x03183, { 0x0110B, 0x01140 } },
      { 0x03184, { 0x01111, 0x0110B } },
      { 0x03185, { 0x01112, 0x01112 } },
      { 0x03186, { 0x01159 } },
      { 0x03187, { 0x0116D, 0x01163 } },
      { 0x03188, { 0x0116D, 0x01163, 0x04E28 } },
      { 0x03189, { 0x0116D, 0x04E28 } },
      { 0x0318A, { 0x01172, 0x01167 } },
      { 0x0318B, { 0x01172, 0x01167, 0x04E28 } },
      { 0x0318C, { 0x01172, 0x04E28 } },
      { 0x0318D, { 0x0119E } },
      { 0x0318E, { 0x0119E, 0x04E28 } },
      { 0x031D0, { 0x030FC } },
      { 0x031D1, { 0x04E28 } },
      { 0x031D3, { 0x0002F } },
      { 0x031D4, { 0x0005C } },
      { 0x031D6, { 0x04E5B } },
      { 0x031DA, { 0x04E85 } },
      { 0x031DB, { 0x0276C } },
      { 0x031DF, { 0x04E5A } },
      { 0x031E0, { 0x04E59 } },
      { 0x03200, { 0x00028, 0x01100, 0x00029 } },
      { 0x03201, { 0x00028, 0x01102, 0x00029 } },
      { 0x03202, { 0x00028, 0x01103, 0x00029 } },
      { 0x03203, { 0x00028, 0x01105, 0x00029 } },
      { 0x03204, { 0x00028, 0x01106, 0x00029 } },
      { 0x03205, { 0x00028, 0x01107, 0x00029 } },
      { 0x03206, { 0x00028, 0x01109, 0x00029 } },
      { 0x03207, { 0x00028, 0x0110B, 0x00029 } },
      { 0x03208, { 0x00028, 0x0110C, 0x00029 } },
      { 0x03209, { 0x00028, 0x0110E, 0x00029 } },
      { 0x0320A, { 0x00028, 0x0110F, 0x00029 } },
      { 0x0320B, { 0x00028, 0x01110, 0x00029 } },
      { 0x0320C, { 0x00028, 0x01111, 0x00029 } },
      { 0x0320D, { 0x00028, 0x01112, 0x00029 } },
      { 0x0320E, { 0x00028, 0x0AC00, 0x00029 } },
      { 0x0320F, { 0x00028, 0x0B098, 0x00029 } },
      { 0x03210, { 0x00028, 0x0B2E4, 0x00029 } },
      { 0x03211, { 0x00028, 0x0B77C, 0x00029 } },
      { 0x03212, { 0x00028, 0x0B9C8, 0x00029 } },
      { 0x03213, { 0x00028, 0x0BC14, 0x00029 } },
      { 0x03214, { 0x00028, 0x0C0AC, 0x00029 } },
      { 0x03215, { 0x00028, 0x0C544, 0x00029 } },
      { 0x03216, { 0x00028, 0x0C790, 0x00029 } },
      { 0x03217, { 0x00028, 0x0CC28, 0x00029 } },
      { 0x03218, { 0x00028, 0x0CE74, 0x00029 } },
      { 0x03219, { 0x00028, 0x0D0C0, 0x00029 } },
      { 0x0321A, { 0x00028, 0x0D30C, 0x00029 } },
      { 0x0321B, { 0x00028, 0x0D558, 0x00029 } },
      { 0x0321C, { 0x00028, 0x0C8FC, 0x00029 } },
      { 0x0321D, { 0x00028, 0x0C624, 0x0C804, 0x00029 } },
      { 0x0321E, { 0x00028, 0x0C624, 0x0D6C4, 0x00029 } },
      { 0x03220, { 0x00028, 0x030FC, 0x00029 } },
      { 0x03221, { 0x00028, 0x04E8C, 0x00029 } },
      { 0x03222, { 0x00028, 0x04E09, 0x00029 } },
      { 0x03223, { 0x00028, 0x056DB, 0x00029 } },
      { 0x03224, { 0x00028, 0x04E94, 0x00029 } },
      { 0x03225, { 0x00028, 0x0516D, 0x00029 } },
      { 0x03226, { 0x00028, 0x04E03, 0x00029 } },
      { 0x03227, { 0x00028, 0x0516B, 0x00029 } },
      { 0x03228, { 0x00028, 0x04E5D, 0x00029 } },
      { 0x03229, { 0x00028, 0x05341, 0x00029 } },
      { 0x0322A, { 0x00028, 0x06708, 0x00029 } },
      { 0x0322B, { 0x00028, 0x0706B, 0x00029 } },
      { 0x0322C, { 0x00028, 0x06C34, 0x00029 } },
      { 0x0322D, { 0x00028, 0x06728, 0x00029 } },
      { 0x0322E, { 0x00028, 0x091D1, 0x00029 } },
      { 0x0322F, { 0x00028, 0x0571F, 0x00029 } },
      { 0x03230, { 0x00028, 0x065E5, 0x00029 } },
      { 0x03231, { 0x00028, 0x0682A, 0x00029 } },
      { 0x03232, { 0x00028, 0x06709, 0x00029 } },
      { 0x03233, { 0x00028, 0x0793E, 0x00029 } },
      { 0x03234, { 0x00028, 0x0540D, 0x00029 } },
      { 0x03235, { 0x00028, 0x07279, 0x00029 } },
      { 0x03236, { 0x00028, 0x08CA1, 0x00029 } },
      { 0x03237, { 0x00028, 0x0795D, 0x00029 } },
      { 0x03238, { 0x00028, 0x052B4, 0x00029 } },
      { 0x03239, { 0x00028, 0x04EE3, 0x00029 } },
      { 0x0323A, { 0x00028, 0x0547C, 0x00029 } },
      { 0x0323B, { 0x00028, 0x05B66, 0x00029 } },
      { 0x0323C, { 0x00028, 0x076E3, 0x00029 } },
      { 0x0323D, { 0x00028, 0x04F01, 0x00029 } },
      { 0x0323E, { 0x00028, 0x08CC7, 0x00029 } },
      { 0x0323F, { 0x00028, 0x05354, 0x00029 } },
      { 0x03240, { 0x00028, 0x0796D, 0x00029 } },
      { 0x03241, { 0x00028, 0x04F11, 0x00029 } },
      { 0x03242, { 0x00028, 0x081EA, 0x00029 } },
      { 0x03243, { 0x00028, 0x081F3, 0x00029 } },
      { 0x032C0, { 0x0006C, 0x06708 } },
      { 0x032C1, { 0x00032, 0x06708 } },
      { 0x032C2, { 0x00033, 0x06708 } },
      { 0x032C3, { 0x00034, 0x06708 } },
      { 0x032C4, { 0x00035, 0x06708 } },
      { 0x032C5, { 0x00036, 0x06708 } },
      { 0x032C6, { 0x00037, 0x06708 } },
      { 0x032C7, { 0x00038, 0x06708 } },
      { 0x032C8, { 0x00039, 0x06708 } },
      { 0x032C9, { 0x0006C, 0x0004F, 0x06708 } },
      { 0x032CA, { 0x0006C, 0x0006C, 0x06708 } },
      { 0x032CB, { 0x0006C, 0x00032, 0x06708 } },
      { 0x03358, { 0x0004F, 0x070B9 } },
      { 0x03359, { 0x0006C, 0x070B9 } },
      { 0x0335A, { 0x00032, 0x070B9 } },
      { 0x0335B, { 0x00033, 0x070B9 } },
      { 0x0335C, { 0x00034, 0x070B9 } },
      { 0x0335D, { 0x00035, 0x070B9 } },
      { 0x0335E, { 0x00036, 0x070B9 } },
      { 0x0335F, { 0x00037, 0x070B9 } },
      { 0x03360, { 0x00038, 0x070B9 } },
      { 0x03361, { 0x00039, 0x070B9 } },
      { 0x03362, { 0x0006C, 0x0004F, 0x070B9 } },
      { 0x03363, { 0x0006C, 0x0006C, 0x070B9 } },
      { 0x03364, { 0x0006C, 0x00032, 0x070B9 } },
      { 0x03365, { 0x0006C, 0x00033, 0x070B9 } },
      { 0x03366, { 0x0006C, 0x00034, 0x070B9 } },
      { 0x03367, { 0x0006C, 0x00035, 0x070B9 } },
      { 0x03368, { 0x0006C, 0x00036, 0x070B9 } },
      { 0x03369, { 0x0006C, 0x00037, 0x070B9 } },
      { 0x0336A, { 0x0006C, 0x00038, 0x070B9 } },
      { 0x0336B, { 0x0006C, 0x00039, 0x070B9 } },
      { 0x0336C, { 0x00032, 0x0004F, 0x070B9 } },
      { 0x0336D, { 0x00032, 0x0006C, 0x070B9 } },
      { 0x0336E, { 0x00032, 0x00032, 0x070B9 } },
      { 0x0336F, { 0x00032, 0x00033, 0x070B9 } },
      { 0x03370, { 0x00032, 0x00034, 0x070B9 } },
      { 0x033E0, { 0x0006C, 0x065E5 } },
      { 0x033E1, { 0x00032, 0x065E5 } },
      { 0x033E2, { 0x00033, 0x065E5 } },
      { 0x033E3, { 0x00034, 0x065E5 } },
      { 0x033E4, { 0x00035, 0x065E5 } },
      { 0x033E5, { 0x00036, 0x065E5 } },
      { 0x033E6, { 0x00037, 0x065E5 } },
      { 0x033E7, { 0x00038, 0x065E5 } },
      { 0x033E8, { 0x00039, 0x065E5 } },
      { 0x033E9, { 0x0006C, 0x0004F, 0x065E5 } },
      { 0x033EA, { 0x0006C, 0x0006C, 0x065E5 } },
      { 0x033EB, { 0x0006C, 0x00032, 0x065E5 } },
      { 0x033EC, { 0x0006C, 0x00033, 0x065E5 } },
      { 0x033ED, { 0x0006C, 0x00034, 0x065E5 } },
      { 0x033EE, { 0x0006C, 0x00035, 0x065E5 } },
      { 0x033EF, { 0x0006C, 0x00036, 0x065E5 } },
      { 0x033F0, { 0x0006C, 0x00037, 0x065E5 } },
      { 0x033F1, { 0x0006C, 0x00038, 0x065E5 } },
      { 0x033F2, { 0x0006C, 0x00039, 0x065E5 } },
      { 0x033F3, { 0x00032, 0x0004F, 0x065E5 } },
      { 0x033F4, { 0x00032, 0x0006C, 0x065E5 } },
      { 0x033F5, { 0x00032, 0x00032, 0x065E5 } },
      { 0x033F6, { 0x00032, 0x00033, 0x065E5 } },
      { 0x033F7, { 0x00032, 0x00034, 0x065E5 } },
      { 0x033F8, { 0x00032, 0x00035, 0x065E5 } },
      { 0x033F9, { 0x00032, 0x00036, 0x065E5 } },
      { 0x033FA, { 0x00032, 0x00037, 0x065E5 } },
      { 0x033FB, { 0x00032, 0x00038, 0x065E5 } },
      { 0x033FC, { 0x00032, 0x00039, 0x065E5 } },
      { 0x033FD, { 0x00033, 0x0004F, 0x065E5 } },
      { 0x033FE, { 0x00033, 0x0006C, 0x065E5 } },
      { 0x039B3, { 0x0363D } },
      { 0x0439B, { 0x03588 } },
      { 0x04420, { 0x03B3B } },
      { 0x04695, { 0x278AE } },
      { 0x04E00, { 0x030FC } },
      { 0x04E15, { 0x0110C, 0x01169 } },
      { 0x04E1B, { 0x01109, 0x01109, 0x030FC } },
      { 0x04E36, { 0x0005C } },
      { 0x04E3F, { 0x0002F } },
      { 0x04E8E, { 0x1B122 } },
      { 0x04ECA, { 0x01109, 0x030FC, 0x01100 } },
      { 0x05002, { 0x04F75 } },
      { 0x0503C, { 0x05024 } },
      { 0x05152, { 0x16FF3 } },
      { 0x0535F, { 0x01106, 0x01161 } },
      { 0x05408, { 0x01109, 0x030FC, 0x01106 } },
      { 0x0555F, { 0x05553 } },
      { 0x056D7, { 0x053E3 } },
      { 0x0586B, { 0x05861 } },
      { 0x058EB, { 0x0571F } },
      { 0x058FF, { 0x058AB } },
      { 0x05B00, { 0x05AAF } },
      { 0x05E32, { 0x05E21 } },
      { 0x05E50, { 0x03B3A } },
      { 0x06138, { 0x2B73F } },
      { 0x06238, { 0x06236 } },
      { 0x06409, { 0x03A41 } },
      { 0x06663, { 0x0403F } },
      { 0x06669, { 0x0665A } },
      { 0x066F6, { 0x03ADA } },
      { 0x06726, { 0x04443 } },
      { 0x067FF, { 0x0676E } },
      { 0x069E9, { 0x03BA3 } },
      { 0x06A27, { 0x0699D } },
      { 0x06F59, { 0x06E88 } },
      { 0x0723F, { 0x01102, 0x0116E, 0x04E28 } },
      { 0x0784F, { 0x07814 } },
      { 0x07D76, { 0x07D55 } },
      { 0x080A6, { 0x0670C } },
      { 0x080CA, { 0x06710 } },
      { 0x080D0, { 0x0670F } },
      { 0x080F6, { 0x03B35 } },
      { 0x08101, { 0x06713 } },
      { 0x08127, { 0x06718 } },
      { 0x08141, { 0x080FC } },
      { 0x081A7, { 0x06723 } },
      { 0x0853F, { 0x0848D } },
      { 0x08641, { 0x08637 } },
      { 0x08A1E, { 0x046B6 } },
      { 0x08A7D, { 0x08A2E } },
      { 0x08B8F, { 0x08B86 } },
      { 0x08C63, { 0x08C5C } },
      { 0x08D86, { 0x08D7F } },
      { 0x08DFA, { 0x08DE5 } },
      { 0x08E9B, { 0x08E97 } },
      { 0x08F27, { 0x08EFF } },
      { 0x090DE, { 0x090CE } },
      { 0x093AE, { 0x093AD } },
      { 0x09577, { 0x01110, 0x030FC, 0x01102, 0x0110C } },
      { 0x096B8, { 0x096B7 } },
      { 0x09E43, { 0x09E42 } },
      { 0x09ED2, { 0x09ED1 } },
      { 0x09FC3, { 0x04039 } },
      { 0x0A494, { 0x0A2CD } },
      { 0x0A49C, { 0x0A0C0 } },
      { 0x0A49E, { 0x0A04A } },
      { 0x0A4A7, { 0x0A458 } },
      { 0x0A4A8, { 0x0A132 } },
      { 0x0A4AC, { 0x0A050 } },
      { 0x0A4B0, { 0x0A3C2 } },
      { 0x0A4BA, { 0x0A3BF } },
      { 0x0A4BE, { 0x0A2B1 } },
      { 0x0A4BF, { 0x0A259 } },
      { 0x0A4C0, { 0x0A3AB } },
      { 0x0A4C2, { 0x0A3B5 } },
      { 0x0A4D0, { 0x00042 } },
      { 0x0A4D1, { 0x00050 } },
      { 0x0A4D2, { 0x00064 } },
      { 0x0A4D3, { 0x00044 } },
      { 0x0A4D4, { 0x00054 } },
      { 0x0A4D6, { 0x00047 } },
      { 0x0A4D7, { 0x0004B } },
      { 0x0A4D9, { 0x0004A } },
      { 0x0A4DA, { 0x00043 } },
      { 0x0A4DB, { 0x00186 } },
      { 0x0A4DC, { 0x0005A } },
      { 0x0A4DD, { 0x00046 } },
      { 0x0A4DE, { 0x02132 } },
      { 0x0A4DF, { 0x0004D } },
      { 0x0A4E0, { 0x0004E } },
      { 0x0A4E1, { 0x0004C } },
      { 0x0A4E2, { 0x00053 } },
      { 0x0A4E3, { 0x00052 } },
      { 0x0A4E5, { 0x00245 } },
      { 0x0A4E6, { 0x00056 } },
      { 0x0A4E7, { 0x00048 } },
      { 0x0A4EA, { 0x00057 } },
      { 0x0A4EB, { 0x00058 } },
      { 0x0A4EC, { 0x00059 } },
      { 0x0A4ED, { 0x01660 } },
      { 0x0A4EE, { 0x00041 } },
      { 0x0A4EF, { 0x02C6F } },
      { 0x0A4F0, { 0x00045 } },
      { 0x0A4F1, { 0x0018E } },
      { 0x0A4F2, { 0x0006C } },
      { 0x0A4F3, { 0x0004F } },
      { 0x0A4F4, { 0x00055 } },
      { 0x0A4F5, { 0x00548 } },
      { 0x0A4F7, { 0x015E1 } },
      { 0x0A4F8, { 0x0002E } },
      { 0x0A4F9, { 0x0002C } },
      { 0x0A4FA, { 0x0002E, 0x0002E } },
      { 0x0A4FB, { 0x0002E, 0x0002C } },
      { 0x0A4FD, { 0x0003A } },
      { 0x0A4FE, { 0x0002D, 0x0002E } },
      { 0x0A4FF, { 0x0003D } },
      { 0x0A60E, { 0x0002E } },
      { 0x0A644, { 0x00032 } },
      { 0x0A645, { 0x001A8 } },
      { 0x0A647, { 0x00069 } },
      { 0x0A64D, { 0x003C9 } },
      { 0x0A650, { 0x0042A, 0x0006C } },
      { 0x0A651, { 0x002C9, 0x00062, 0x00069 } },
      { 0x0A668, { 0x00298 } },
      { 0x0A66F, { 0x020E9 } },
      { 0x0A67C, { 0x00306 } },
      { 0x0A67E, { 0x002C7 } },
      { 0x0A695, { 0x00068, 0x00314 } },
      { 0x0A698, { 0x0004F, 0x0004F } },
      { 0x0A699, { 0x0006F, 0x0006F } },
      { 0x0A69A, { 0x102A8 } },
      { 0x0A6A1, { 0x00418 } },
      { 0x0A6B0, { 0x016B9 } },
      { 0x0A6B1, { 0x02C75 } },
      { 0x0A6CD, { 0x002A1 } },
      { 0x0A6CE, { 0x00245 } },
      { 0x0A6DB, { 0x003A0 } },
      { 0x0A6DF, { 0x00056 } },
      { 0x0A6EB, { 0x0003F } },
      { 0x0A6EF, { 0x00032 } },
      { 0x0A6F0, { 0x00302 } },
      { 0x0A6F1, { 0x00304 } },
      { 0x0A6F4, { 0x0A6F3, 0x0A6F3 } },
      { 0x0A714, { 0x002EB } },
      { 0x0A716, { 0x002EA } },
      { 0x0A728, { 0x00054, 0x00033 } },
      { 0x0A729, { 0x00074, 0x0021D } },
      { 0x0A731, { 0x00073 } },
      { 0x0A732, { 0x00041, 0x00041 } },
      { 0x0A733, { 0x00061, 0x00061 } },
      { 0x0A734, { 0x00041, 0x0004F } },
      { 0x0A735, { 0x00061, 0x0006F } },
      { 0x0A736, { 0x00041, 0x00055 } },
      { 0x0A737, { 0x00061, 0x00075 } },
      { 0x0A738, { 0x00041, 0x00056 } },
      { 0x0A739, { 0x00061, 0x00076 } },
      { 0x0A73A, { 0x00041, 0x00056 } },
      { 0x0A73B, { 0x00061, 0x00076 } },
      { 0x0A73C, { 0x00041, 0x00059 } },
      { 0x0A73D, { 0x00061, 0x00079 } },
      { 0x0A740, { 0x0004B, 0x00335 } },
      { 0x0A74A, { 0x0004F, 0x00335 } },
      { 0x0A74B, { 0x0006F, 0x00335 } },
      { 0x0A74E, { 0x0004F, 0x0004F } },
      { 0x0A74F, { 0x0006F, 0x0006F } },
      { 0x0A75A, { 0x00032 } },
      { 0x0A761, { 0x00077, 0x00326 } },
      { 0x0A76A, { 0x00033 } },
      { 0x0A76B, { 0x0021D } },
      { 0x0A76E, { 0x00039 } },
      { 0x0A777, { 0x00074, 0x00066 } },
      { 0x0A778, { 0x00026 } },
      { 0x0A77A, { 0x0A779 } },
      { 0x0A789, { 0x0003A } },
      { 0x0A78C, { 0x00027 } },
      { 0x0A78F, { 0x000B7 } },
      { 0x0A795, { 0x0A727 } },
      { 0x0A798, { 0x00046 } },
      { 0x0A799, { 0x00066 } },
      { 0x0A79A, { 0x10412 } },
      { 0x0A79B, { 0x1043A } },
      { 0x0A79D, { 0x0029A } },
      { 0x0A79E, { 0x0A4E4 } },
      { 0x0A79F, { 0x00075 } },
      { 0x0A7AB, { 0x00033 } },
      { 0x0A7B1, { 0x0A4D5 } },
      { 0x0A7B2, { 0x0004A } },
      { 0x0A7B3, { 0x00058 } },
      { 0x0A7B4, { 0x00042 } },
      { 0x0A7B5, { 0x000DF } },
      { 0x0A7B6, { 0x0A64C } },
      { 0x0A7B7, { 0x003C9 } },
      { 0x0A7CF, { 0x0A7CE } },
      { 0x0A7D2, { 0x0A7D3 } },
      { 0x0A7D4, { 0x0A7D5 } },
      { 0x0A7D6, { 0x000DF } },
      { 0x0A7DA, { 0x00245 } },
      { 0x0A7DB, { 0x003BB } },
      { 0x0A7DC, { 0x00245, 0x00338 } },
      { 0x0A7F1, { 0x018F5 } },
      { 0x0A7F7, { 0x030FC } },
      { 0x0A830, { 0x00964 } },
      { 0x0A960, { 0x01103, 0x01106 } },
      { 0x0A961, { 0x01103, 0x01107 } },
      { 0x0A962, { 0x01103, 0x01109 } },
      { 0x0A963, { 0x01103, 0x0110C } },
      { 0x0A964, { 0x01105, 0x01100 } },
      { 0x0A965, { 0x01105, 0x01100, 0x01100 } },
      { 0x0A966, { 0x01105, 0x01103 } },
      { 0x0A967, { 0x01105, 0x01103, 0x01103 } },
      { 0x0A968, { 0x01105, 0x01106 } },
      { 0x0A969, { 0x01105, 0x01107 } },
      { 0x0A96A, { 0x01105, 0x01107, 0x01107 } },
      { 0x0A96B, { 0x01105, 0x01107, 0x0110B } },
      { 0x0A96C, { 0x01105, 0x01109 } },
      { 0x0A96D, { 0x01105, 0x0110C } },
      { 0x0A96E, { 0x01105, 0x0110F } },
      { 0x0A96F, { 0x01106, 0x01100 } },
      { 0x0A970, { 0x01106, 0x01103 } },
      { 0x0A971, { 0x01106, 0x01109 } },
      { 0x0A972, { 0x01107, 0x01109, 0x01110 } },
      { 0x0A973, { 0x01107, 0x0110F } },
      { 0x0A974, { 0x01107, 0x01112 } },
      { 0x0A975, { 0x01109, 0x01109, 0x01107 } },
      { 0x0A976, { 0x0110B, 0x01105 } },
      { 0x0A977, { 0x0110B, 0x01112 } },
      { 0x0A978, { 0x0110C, 0x0110C, 0x01112 } },
      { 0x0A979, { 0x01110, 0x01110 } },
      { 0x0A97A, { 0x01111, 0x01112 } },
      { 0x0A97B, { 0x01112, 0x01109 } },
      { 0x0A97C, { 0x01159, 0x01159 } },
      { 0x0A992, { 0x02C3F } },
      { 0x0A9A3, { 0x0A99D } },
      { 0x0A9C6, { 0x0A9D0 } },
      { 0x0A9CF, { 0x00662 } },
      { 0x0AA53, { 0x0AA01 } },
      { 0x0AA56, { 0x0AA23 } },
      { 0x0AB32, { 0x00065 } },
      { 0x0AB35, { 0x00066 } },
      { 0x0AB3D, { 0x0006F } },
      { 0x0AB3E, { 0x0006F, 0x00338 } },
      { 0x0AB3F, { 0x00254, 0x00338 } },
      { 0x0AB41, { 0x001DD, 0x0006F, 0x00338 } },
      { 0x0AB42, { 0x001DD, 0x0006F, 0x00335 } },
      { 0x0AB47, { 0x00072 } },
      { 0x0AB48, { 0x00072 } },
      { 0x0AB4D, { 0x00283 } },
      { 0x0AB4E, { 0x00075 } },
      { 0x0AB52, { 0x00075 } },
      { 0x0AB53, { 0x003C7 } },
      { 0x0AB55, { 0x003C7 } },
      { 0x0AB5A, { 0x00079 } },
      { 0x0AB60, { 0x00459 } },
      { 0x0AB62, { 0x00254, 0x00065 } },
      { 0x0AB63, { 0x00075, 0x0006F } },
      { 0x0AB70, { 0x01D05 } },
      { 0x0AB71, { 0x00280 } },
      { 0x0AB72, { 0x01D1B } },
      { 0x0AB74, { 0x0006F, 0x0031B } },
      { 0x0AB75, { 0x00069 } },
      { 0x0AB7A, { 0x01D00 } },
      { 0x0AB7B, { 0x01D0A } },
      { 0x0AB7C, { 0x01D07 } },
      { 0x0AB7E, { 0x00242 } },
      { 0x0AB80, { 0x02C76 } },
      { 0x0AB81, { 0x00072 } },
      { 0x0AB83, { 0x00077 } },
      { 0x0AB87, { 0x0028D } },
      { 0x0AB8B, { 0x0029C } },
      { 0x0AB8E, { 0x0006F, 0x00335 } },
      { 0x0AB90, { 0x00262 } },
      { 0x0AB93, { 0x0007A } },
      { 0x0AB9B, { 0x0A793 } },
      { 0x0AB9C, { 0x00075, 0x00335 } },
      { 0x0AB9F, { 0x00185 } },
      { 0x0ABA2, { 0x00280 } },
      { 0x0ABA9, { 0x00076 } },
      { 0x0ABAA, { 0x00073 } },
      { 0x0ABAE, { 0x0029F } },
      { 0x0ABAF, { 0x00063 } },
      { 0x0ABB2, { 0x01D18 } },
      { 0x0ABB6, { 0x00138 } },
      { 0x0ABBB, { 0x0006F, 0x00335 } },
      { 0x0D7B0, { 0x01169, 0x01167 } },
      { 0x0D7B1, { 0x01169, 0x01169, 0x04E28 } },
      { 0x0D7B2, { 0x0116D, 0x01161 } },
      { 0x0D7B3, { 0x0116D, 0x01161, 0x04E28 } },
      { 0x0D7B4, { 0x0116D, 0x01165 } },
      { 0x0D7B5, { 0x0116E, 0x01167 } },
      { 0x0D7B6, { 0x0116E, 0x04E28, 0x04E28 } },
      { 0x0D7B7, { 0x01172, 0x01161, 0x04E28 } },
      { 0x0D7B8, { 0x01172, 0x01169 } },
      { 0x0D7B9, { 0x030FC, 0x01161 } },
      { 0x0D7BA, { 0x030FC, 0x01165 } },
      { 0x0D7BB, { 0x030FC, 0x01165, 0x04E28 } },
      { 0x0D7BC, { 0x030FC, 0x01169 } },
      { 0x0D7BD, { 0x04E28, 0x01163, 0x01169 } },
      { 0x0D7BE, { 0x04E28, 0x01163, 0x04E28 } },
      { 0x0D7BF, { 0x04E28, 0x01167 } },
      { 0x0D7C0, { 0x04E28, 0x01167, 0x04E28 } },
      { 0x0D7C1, { 0x04E28, 0x01169, 0x04E28 } },
      { 0x0D7C2, { 0x04E28, 0x0116D } },
      { 0x0D7C3, { 0x04E28, 0x01172 } },
      { 0x0D7C4, { 0x04E28, 0x04E28 } },
      { 0x0D7C5, { 0x0119E, 0x01161 } },
      { 0x0D7C6, { 0x0119E, 0x01165, 0x04E28 } },
      { 0x0D7CB, { 0x01102, 0x01105 } },
      { 0x0D7CC, { 0x01102, 0x0110E } },
      { 0x0D7CD, { 0x01103, 0x01103 } },
      { 0x0D7CE, { 0x01103, 0x01103, 0x01107 } },
      { 0x0D7CF, { 0x01103, 0x01107 } },
      { 0x0D7D0, { 0x01103, 0x01109 } },
      { 0x0D7D1, { 0x01103, 0x01109, 0x01100 } },
      { 0x0D7D2, { 0x01103, 0x0110C } },
      { 0x0D7D3, { 0x01103, 0x0110E } },
      { 0x0D7D4, { 0x01103, 0x01110 } },
      { 0x0D7D5, { 0x01105, 0x01100, 0x01100 } },
      { 0x0D7D6, { 0x01105, 0x01100, 0x01112 } },
      { 0x0D7D7, { 0x01105, 0x01105, 0x0110F } },
      { 0x0D7D8, { 0x01105, 0x01106, 0x01112 } },
      { 0x0D7D9, { 0x01105, 0x01107, 0x01103 } },
      { 0x0D7DA, { 0x01105, 0x01107, 0x01111 } },
      { 0x0D7DB, { 0x01105, 0x0114C } },
      { 0x0D7DC, { 0x01105, 0x01159, 0x01112 } },
      { 0x0D7DD, { 0x01105, 0x0110B } },
      { 0x0D7DE, { 0x01106, 0x01102 } },
      { 0x0D7DF, { 0x01106, 0x01102, 0x01102 } },
      { 0x0D7E0, { 0x01106, 0x01106 } },
      { 0x0D7E1, { 0x01106, 0x01107, 0x01109 } },
      { 0x0D7E2, { 0x01106, 0x0110C } },
      { 0x0D7E3, { 0x01107, 0x01103 } },
      { 0x0D7E4, { 0x01107, 0x01105, 0x01111 } },
      { 0x0D7E5, { 0x01107, 0x01106 } },
      { 0x0D7E6, { 0x01107, 0x01107 } },
      { 0x0D7E7, { 0x01107, 0x01109, 0x01103 } },
      { 0x0D7E8, { 0x01107, 0x0110C } },
      { 0x0D7E9, { 0x01107, 0x0110E } },
      { 0x0D7EA, { 0x01109, 0x01106 } },
      { 0x0D7EB, { 0x01109, 0x01107, 0x0110B } },
      { 0x0D7EC, { 0x01109, 0x01109, 0x01100 } },
      { 0x0D7ED, { 0x01109, 0x01109, 0x01103 } },
      { 0x0D7EE, { 0x01109, 0x01140 } },
      { 0x0D7EF, { 0x01109, 0x0110C } },
      { 0x0D7F0, { 0x01109, 0x0110E } },
      { 0x0D7F1, { 0x01109, 0x01110 } },
      { 0x0D7F2, { 0x01105, 0x01112 } },
      { 0x0D7F3, { 0x01140, 0x01107 } },
      { 0x0D7F4, { 0x01140, 0x01107, 0x0110B } },
      { 0x0D7F5, { 0x0114C, 0x01106 } },
      { 0x0D7F6, { 0x0114C, 0x01112 } },
      { 0x0D7F7, { 0x0110C, 0x01107 } },
      { 0x0D7F8, { 0x0110C, 0x01107, 0x01107 } },
      { 0x0D7F9, { 0x0110C, 0x0110C } },
      { 0x0D7FA, { 0x01111, 0x01109 } },
      { 0x0D7FB, { 0x01111, 0x01110 } },
      { 0x0F900, { 0x08C48 } },
      { 0x0F901, { 0x066F4 } },
      { 0x0F902, { 0x08ECA } },
      { 0x0F903, { 0x08CC8 } },
      { 0x0F904, { 0x06ED1 } },
      { 0x0F905, { 0x04E32 } },
      { 0x0F906, { 0x053E5 } },
      { 0x0F907, { 0x09F9C } },
      { 0x0F908, { 0x09F9C } },
      { 0x0F909, { 0x05951 } },
      { 0x0F90A, { 0x091D1 } },
      { 0x0F90B, { 0x05587 } },
      { 0x0F90C, { 0x05948 } },
      { 0x0F90D, { 0x061F6 } },
      { 0x0F90E, { 0x07669 } },
      { 0x0F90F, { 0x07F85 } },
      { 0x0F910, { 0x0863F } },
      { 0x0F911, { 0x087BA } },
      { 0x0F912, { 0x088F8 } },
      { 0x0F913, { 0x0908F } },
      { 0x0F914, { 0x06A02 } },
      { 0x0F915, { 0x06D1B } },
      { 0x0F916, { 0x070D9 } },
      { 0x0F917, { 0x073DE } },
      { 0x0F918, { 0x0843D } },
      { 0x0F919, { 0x0916A } },
      { 0x0F91A, { 0x099F1 } },
      { 0x0F91B, { 0x04E82 } },
      { 0x0F91C, { 0x05375 } },
      { 0x0F91D, { 0x06B04 } },
      { 0x0F91E, { 0x0721B } },
      { 0x0F91F, { 0x0862D } },
      { 0x0F920, { 0x09E1E } },
      { 0x0F921, { 0x05D50 } },
      { 0x0F922, { 0x06FEB } },
      { 0x0F923, { 0x085CD } },
      { 0x0F924, { 0x08964 } },
      { 0x0F925, { 0x062C9 } },
      { 0x0F926, { 0x081D8 } },
      { 0x0F927, { 0x0881F } },
      { 0x0F928, { 0x05ECA } },
      { 0x0F929, { 0x06717 } },
      { 0x0F92A, { 0x06D6A } },
      { 0x0F92B, { 0x072FC } },
      { 0x0F92C, { 0x090CE } },
      { 0x0F92D, { 0x04F86 } },
      { 0x0F92E, { 0x051B7 } },
      { 0x0F92F, { 0x052DE } },
      { 0x0F930, { 0x064C4 } },
      { 0x0F931, { 0x06AD3 } },
      { 0x0F932, { 0x07210 } },
      { 0x0F933, { 0x076E7 } },
      { 0x0F934, { 0x08001 } },
      { 0x0F935, { 0x08606 } },
      { 0x0F936, { 0x0865C } },
      { 0x0F937, { 0x08DEF } },
      { 0x0F938, { 0x09732 } },
      { 0x0F939, { 0x09B6F } },
      { 0x0F93A, { 0x09DFA } },
      { 0x0F93B, { 0x0788C } },
      { 0x0F93C, { 0x0797F } },
      { 0x0F93D, { 0x07DA0 } },
      { 0x0F93E, { 0x083C9 } },
      { 0x0F93F, { 0x09304 } },
      { 0x0F940, { 0x09E7F } },
      { 0x0F941, { 0x08AD6 } },
      { 0x0F942, { 0x058DF } },
      { 0x0F943, { 0x05F04 } },
      { 0x0F944, { 0x07C60 } },
      { 0x0F945, { 0x0807E } },
      { 0x0F946, { 0x07262 } },
      { 0x0F947, { 0x078CA } },
      { 0x0F948, { 0x08CC2 } },
      { 0x0F949, { 0x096F7 } },
      { 0x0F94A, { 0x058D8 } },
      { 0x0F94B, { 0x05C62 } },
      { 0x0F94C, { 0x06A13 } },
      { 0x0F94D, { 0x06DDA } },
      { 0x0F94E, { 0x06F0F } },
      { 0x0F94F, { 0x07D2F } },
      { 0x0F950, { 0x07E37 } },
      { 0x0F951, { 0x0964B } },
      { 0x0F952, { 0x052D2 } },
      { 0x0F953, { 0x0808B } },
      { 0x0F954, { 0x051DC } },
      { 0x0F955, { 0x051CC } },
      { 0x0F956, { 0x07A1C } },
      { 0x0F957, { 0x07DBE } },
      { 0x0F958, { 0x083F1 } },
      { 0x0F959, { 0x09675 } },
      { 0x0F95A, { 0x08B80 } },
      { 0x0F95B, { 0x062CF } },
      { 0x0F95C, { 0x06A02 } },
      { 0x0F95D, { 0x08AFE } },
      { 0x0F95E, { 0x04E39 } },
      { 0x0F95F, { 0x05BE7 } },
      { 0x0F960, { 0x06012 } },
      { 0x0F961, { 0x07387 } },
      { 0x0F962, { 0x07570 } },
      { 0x0F963, { 0x05317 } },
      { 0x0F964, { 0x078FB } },
      { 0x0F965, { 0x04FBF } },
      { 0x0F966, { 0x05FA9 } },
      { 0x0F967, { 0x04E0D } },
      { 0x0F968, { 0x06CCC } },
      { 0x0F969, { 0x06578 } },
      { 0x0F96A, { 0x07D22 } },
      { 0x0F96B, { 0x053C3 } },
      { 0x0F96C, { 0x0585E } },
      { 0x0F96D, { 0x07701 } },
      { 0x0F96E, { 0x08449 } },
      { 0x0F96F, { 0x08AAA } },
      { 0x0F970, { 0x06BBA } },
      { 0x0F971, { 0x08FB0 } },
      { 0x0F972, { 0x06C88 } },
      { 0x0F973, { 0x062FE } },
      { 0x0F974, { 0x082E5 } },
      { 0x0F975, { 0x063A0 } },
      { 0x0F976, { 0x07565 } },
      { 0x0F977, { 0x04EAE } },
      { 0x0F978, { 0x05169 } },
      { 0x0F979, { 0x051C9 } },
      { 0x0F97A, { 0x06881 } },
      { 0x0F97B, { 0x07CE7 } },
      { 0x0F97C, { 0x0826F } },
      { 0x0F97D, { 0x08AD2 } },
      { 0x0F97E, { 0x091CF } },
      { 0x0F97F, { 0x052F5 } },
      { 0x0F980, { 0x05442 } },
      { 0x0F981, { 0x05973 } },
      { 0x0F982, { 0x05EEC } },
      { 0x0F983, { 0x065C5 } },
      { 0x0F984, { 0x06FFE } },
      { 0x0F985, { 0x0792A } },
      { 0x0F986, { 0x095AD } },
      { 0x0F987, { 0x09A6A } },
      { 0x0F988, { 0x09E97 } },
      { 0x0F989, { 0x09ECE } },
      { 0x0F98A, { 0x0529B } },
      { 0x0F98B, { 0x066C6 } },
      { 0x0F98C, { 0x06B77 } },
      { 0x0F98D, { 0x08F62 } },
      { 0x0F98E, { 0x05E74 } },
      { 0x0F98F, { 0x06190 } },
      { 0x0F990, { 0x06200 } },
      { 0x0F991, { 0x0649A } },
      { 0x0F992, { 0x06F23 } },
      { 0x0F993, { 0x07149 } },
      { 0x0F994, { 0x07489 } },
      { 0x0F995, { 0x079CA } },
      { 0x0F996, { 0x07DF4 } },
      { 0x0F997, { 0x0806F } },
      { 0x0F998, { 0x08F26 } },
      { 0x0F999, { 0x084EE } },
      { 0x0F99A, { 0x09023 } },
      { 0x0F99B, { 0x0934A } },
      { 0x0F99C, { 0x05217 } },
      { 0x0F99D, { 0x052A3 } },
      { 0x0F99E, { 0x054BD } },
      { 0x0F99F, { 0x070C8 } },
      { 0x0F9A0, { 0x088C2 } },
      { 0x0F9A1, { 0x08AAA } },
      { 0x0F9A2, { 0x05EC9 } },
      { 0x0F9A3, { 0x05FF5 } },
      { 0x0F9A4, { 0x0637B } },
      { 0x0F9A5, { 0x06BAE } },
      { 0x0F9A6, { 0x07C3E } },
      { 0x0F9A7, { 0x07375 } },
      { 0x0F9A8, { 0x04EE4 } },
      { 0x0F9A9, { 0x056F9 } },
      { 0x0F9AA, { 0x05BE7 } },
      { 0x0F9AB, { 0x05DBA } },
      { 0x0F9AC, { 0x0601C } },
      { 0x0F9AD, { 0x073B2 } },
      { 0x0F9AE, { 0x07469 } },
      { 0x0F9AF, { 0x07F9A } },
      { 0x0F9B0, { 0x08046 } },
      { 0x0F9B1, { 0x09234 } },
      { 0x0F9B2, { 0x096F6 } },
      { 0x0F9B3, { 0x09748 } },
      { 0x0F9B4, { 0x09818 } },
      { 0x0F9B5, { 0x04F8B } },
      { 0x0F9B6, { 0x079AE } },
      { 0x0F9B7, { 0x091B4 } },
      { 0x0F9B8, { 0x096B7 } },
      { 0x0F9B9, { 0x060E1 } },
      { 0x0F9BA, { 0x04E86 } },
      { 0x0F9BB, { 0x050DA } },
      { 0x0F9BC, { 0x05BEE } },
      { 0x0F9BD, { 0x05C3F } },
      { 0x0F9BE, { 0x06599 } },
      { 0x0F9BF, { 0x06A02 } },
      { 0x0F9C0, { 0x071CE } },
      { 0x0F9C1, { 0x07642 } },
      { 0x0F9C2, { 0x084FC } },
      { 0x0F9C3, { 0x0907C } },
      { 0x0F9C4, { 0x09F8D } },
      { 0x0F9C5, { 0x06688 } },
      { 0x0F9C6, { 0x0962E } },
      { 0x0F9C7, { 0x05289 } },
      { 0x0F9C8, { 0x0677B } },
      { 0x0F9C9, { 0x067F3 } },
      { 0x0F9CA, { 0x06D41 } },
      { 0x0F9CB, { 0x06E9C } },
      { 0x0F9CC, { 0x07409 } },
      { 0x0F9CD, { 0x07559 } },
      { 0x0F9CE, { 0x0786B } },
      { 0x0F9CF, { 0x07D10 } },
      { 0x0F9D0, { 0x0985E } },
      { 0x0F9D1, { 0x0516D } },
      { 0x0F9D2, { 0x0622E } },
      { 0x0F9D3, { 0x09678 } },
      { 0x0F9D4, { 0x0502B } },
      { 0x0F9D5, { 0x05D19 } },
      { 0x0F9D6, { 0x06DEA } },
      { 0x0F9D7, { 0x08F2A } },
      { 0x0F9D8, { 0x05F8B } },
      { 0x0F9D9, { 0x06144 } },
      { 0x0F9DA, { 0x06817 } },
      { 0x0F9DB, { 0x07387 } },
      { 0x0F9DC, { 0x09686 } },
      { 0x0F9DD, { 0x05229 } },
      { 0x0F9DE, { 0x0540F } },
      { 0x0F9DF, { 0x05C65 } },
      { 0x0F9E0, { 0x06613 } },
      { 0x0F9E1, { 0x0674E } },
      { 0x0F9E2, { 0x068A8 } },
      { 0x0F9E3, { 0x06CE5 } },
      { 0x0F9E4, { 0x07406 } },
      { 0x0F9E5, { 0x075E2 } },
      { 0x0F9E6, { 0x07F79 } },
      { 0x0F9E7, { 0x088CF } },
      { 0x0F9E8, { 0x088E1 } },
      { 0x0F9E9, { 0x091CC } },
      { 0x0F9EA, { 0x096E2 } },
      { 0x0F9EB, { 0x0533F } },
      { 0x0F9EC, { 0x06EBA } },
      { 0x0F9ED, { 0x0541D } },
      { 0x0F9EE, { 0x071D0 } },
      { 0x0F9EF, { 0x07498 } },
      { 0x0F9F0, { 0x085FA } },
      { 0x0F9F1, { 0x096A3 } },
      { 0x0F9F2, { 0x09C57 } },
      { 0x0F9F3, { 0x09E9F } },
      { 0x0F9F4, { 0x06797 } },
      { 0x0F9F5, { 0x06DCB } },
      { 0x0F9F6, { 0x081E8 } },
      { 0x0F9F7, { 0x07ACB } },
      { 0x0F9F8, { 0x07B20 } },
      { 0x0F9F9, { 0x07C92 } },
      { 0x0F9FA, { 0x072C0 } },
      { 0x0F9FB, { 0x07099 } },
      { 0x0F9FC, { 0x08B58 } },
      { 0x0F9FD, { 0x04EC0 } },
      { 0x0F9FE, { 0x08336 } },
      { 0x0F9FF, { 0x0523A } },
      { 0x0FA00, { 0x05207 } },
      { 0x0FA01, { 0x05EA6 } },
      { 0x0FA02, { 0x062D3 } },
      { 0x0FA03, { 0x07CD6 } },
      { 0x0FA04, { 0x05B85 } },
      { 0x0FA05, { 0x06D1E } },
      { 0x0FA06, { 0x066B4 } },
      { 0x0FA07, { 0x08F3B } },
      { 0x0FA08, { 0x0884C } },
      { 0x0FA09, { 0x0964D } },
      { 0x0FA0A, { 0x0898B } },
      { 0x0FA0B, { 0x05ED3 } },
      { 0x0FA0C, { 0x05140 } },
      { 0x0FA0D, { 0x055C0 } },
      { 0x0FA10, { 0x0585A } },
      { 0x0FA12, { 0x06674 } },
      { 0x0FA15, { 0x051DE } },
      { 0x0FA16, { 0x0732A } },
      { 0x0FA17, { 0x076CA } },
      { 0x0FA18, { 0x0793C } },
      { 0x0FA19, { 0x0795E } },
      { 0x0FA1A, { 0x07965 } },
      { 0x0FA1B, { 0x0798F } },
      { 0x0FA1C, { 0x09756 } },
      { 0x0FA1D, { 0x07CBE } },
      { 0x0FA1E, { 0x07FBD } },
      { 0x0FA20, { 0x08612 } },
      { 0x0FA22, { 0x08AF8 } },
      { 0x0FA25, { 0x09038 } },
      { 0x0FA26, { 0x090FD } },
      { 0x0FA2A, { 0x098EF } },
      { 0x0FA2B, { 0x098FC } },
      { 0x0FA2C, { 0x09928 } },
      { 0x0FA2D, { 0x09DB4 } },
      { 0x0FA2E, { 0x090CE } },
      { 0x0FA2F, { 0x096B7 } },
      { 0x0FA30, { 0x04FAE } },
      { 0x0FA31, { 0x050E7 } },
      { 0x0FA32, { 0x0514D } },
      { 0x0FA33, { 0x052C9 } },
      { 0x0FA34, { 0x052E4 } },
      { 0x0FA35, { 0x05351 } },
      { 0x0FA36, { 0x0559D } },
      { 0x0FA37, { 0x05606 } },
      { 0x0FA38, { 0x05668 } },
      { 0x0FA39, { 0x05840 } },
      { 0x0FA3A, { 0x058A8 } },
      { 0x0FA3B, { 0x05C64 } },
      { 0x0FA3C, { 0x05C6E } },
      { 0x0FA3D, { 0x06094 } },
      { 0x0FA3E, { 0x06168 } },
      { 0x0FA3F, { 0x0618E } },
      { 0x0FA40, { 0x061F2 } },
      { 0x0FA41, { 0x0654F } },
      { 0x0FA42, { 0x065E2 } },
      { 0x0FA43, { 0x06691 } },
      { 0x0FA44, { 0x06885 } },
      { 0x0FA45, { 0x06D77 } },
      { 0x0FA46, { 0x06E1A } },
      { 0x0FA47, { 0x06F22 } },
      { 0x0FA48, { 0x0716E } },
      { 0x0FA49, { 0x0722B } },
      { 0x0FA4A, { 0x07422 } },
      { 0x0FA4B, { 0x07891 } },
      { 0x0FA4C, { 0x0793E } },
      { 0x0FA4D, { 0x07949 } },
      { 0x0FA4E, { 0x07948 } },
      { 0x0FA4F, { 0x07950 } },
      { 0x0FA50, { 0x07956 } },
      { 0x0FA51, { 0x0795D } },
      { 0x0FA52, { 0x0798D } },
      { 0x0FA53, { 0x0798E } },
      { 0x0FA54, { 0x07A40 } },
      { 0x0FA55, { 0x07A81 } },
      { 0x0FA56, { 0x07BC0 } },
      { 0x0FA57, { 0x07DF4 } },
      { 0x0FA58, { 0x07E09 } },
      { 0x0FA59, { 0x07E41 } },
      { 0x0FA5A, { 0x07F72 } },
      { 0x0FA5B, { 0x08005 } },
      { 0x0FA5C, { 0x081ED } },
      { 0x0FA5D, { 0x08279 } },
      { 0x0FA5E, { 0x08279 } },
      { 0x0FA5F, { 0x08457 } },
      { 0x0FA60, { 0x08910 } },
      { 0x0FA61, { 0x08996 } },
      { 0x0FA62, { 0x08B01 } },
      { 0x0FA63, { 0x08B39 } },
      { 0x0FA64, { 0x08CD3 } },
      { 0x0FA65, { 0x08D08 } },
      { 0x0FA66, { 0x08FB6 } },
      { 0x0FA67, { 0x09038 } },
      { 0x0FA68, { 0x096E3 } },
      { 0x0FA69, { 0x097FF } },
      { 0x0FA6A, { 0x0983B } },
      { 0x0FA6B, { 0x06075 } },
      { 0x0FA6C, { 0x242EE } },
      { 0x0FA6D, { 0x08218 } },
      { 0x0FA70, { 0x04E26 } },
      { 0x0FA71, { 0x051B5 } },
      { 0x0FA72, { 0x05168 } },
      { 0x0FA73, { 0x04F80 } },
      { 0x0FA74, { 0x05145 } },
      { 0x0FA75, { 0x05180 } },
      { 0x0FA76, { 0x052C7 } },
      { 0x0FA77, { 0x052FA } },
      { 0x0FA78, { 0x0559D } },
      { 0x0FA79, { 0x05555 } },
      { 0x0FA7A, { 0x05599 } },
      { 0x0FA7B, { 0x055E2 } },
      { 0x0FA7C, { 0x0585A } },
      { 0x0FA7D, { 0x058B3 } },
      { 0x0FA7E, { 0x05944 } },
      { 0x0FA7F, { 0x05954 } },
      { 0x0FA80, { 0x05A62 } },
      { 0x0FA81, { 0x05B28 } },
      { 0x0FA82, { 0x05ED2 } },
      { 0x0FA83, { 0x05ED9 } },
      { 0x0FA84, { 0x05F69 } },
      { 0x0FA85, { 0x05FAD } },
      { 0x0FA86, { 0x060D8 } },
      { 0x0FA87, { 0x0614E } },
      { 0x0FA88, { 0x06108 } },
      { 0x0FA89, { 0x0618E } },
      { 0x0FA8A, { 0x06160 } },
      { 0x0FA8B, { 0x061F2 } },
      { 0x0FA8C, { 0x06234 } },
      { 0x0FA8D, { 0x063C4 } },
      { 0x0FA8E, { 0x0641C } },
      { 0x0FA8F, { 0x06452 } },
      { 0x0FA90, { 0x06556 } },
      { 0x0FA91, { 0x06674 } },
      { 0x0FA92, { 0x06717 } },
      { 0x0FA93, { 0x0671B } },
      { 0x0FA94, { 0x06756 } },
      { 0x0FA95, { 0x06B79 } },
      { 0x0FA96, { 0x06BBA } },
      { 0x0FA97, { 0x06D41 } },
      { 0x0FA98, { 0x06EDB } },
      { 0x0FA99, { 0x06ECB } },
      { 0x0FA9A, { 0x06F22 } },
      { 0x0FA9B, { 0x0701E } },
      { 0x0FA9C, { 0x0716E } },
      { 0x0FA9D, { 0x077A7 } },
      { 0x0FA9E, { 0x07235 } },
      { 0x0FA9F, { 0x072AF } },
      { 0x0FAA0, { 0x0732A } },
      { 0x0FAA1, { 0x07471 } },
      { 0x0FAA2, { 0x07506 } },
      { 0x0FAA3, { 0x0753B } },
      { 0x0FAA4, { 0x0761D } },
      { 0x0FAA5, { 0x0761F } },
      { 0x0FAA6, { 0x076CA } },
      { 0x0FAA7, { 0x076DB } },
      { 0x0FAA8, { 0x076F4 } },
      { 0x0FAA9, { 0x0774A } },
      { 0x0FAAA, { 0x07740 } },
      { 0x0FAAB, { 0x078CC } },
      { 0x0FAAC, { 0x07AB1 } },
      { 0x0FAAD, { 0x07BC0 } },
      { 0x0FAAE, { 0x07C7B } },
      { 0x0FAAF, { 0x07D5B } },
      { 0x0FAB0, { 0x07DF4 } },
      { 0x0FAB1, { 0x07F3E } },
      { 0x0FAB2, { 0x08005 } },
      { 0x0FAB3, { 0x08352 } },
      { 0x0FAB4, { 0x083EF } },
      { 0x0FAB5, { 0x08779 } },
      { 0x0FAB6, { 0x08941 } },
      { 0x0FAB7, { 0x08986 } },
      { 0x0FAB8, { 0x08996 } },
      { 0x0FAB9, { 0x08ABF } },
      { 0x0FABA, { 0x08AF8 } },
      { 0x0FABB, { 0x08ACB } },
      { 0x0FABC, { 0x08B01 } },
      { 0x0FABD, { 0x08AFE } },
      { 0x0FABE, { 0x08AED } },
      { 0x0FABF, { 0x08B39 } },
      { 0x0FAC0, { 0x08B8A } },
      { 0x0FAC1, { 0x08D08 } },
      { 0x0FAC2, { 0x08F38 } },
      { 0x0FAC3, { 0x09072 } },
      { 0x0FAC4, { 0x09199 } },
      { 0x0FAC5, { 0x09276 } },
      { 0x0FAC6, { 0x0967C } },
      { 0x0FAC7, { 0x096E3 } },
      { 0x0FAC8, { 0x09756 } },
      { 0x0FAC9, { 0x097DB } },
      { 0x0FACA, { 0x097FF } },
      { 0x0FACB, { 0x0980B } },
      { 0x0FACC, { 0x0983B } },
      { 0x0FACD, { 0x09B12 } },
      { 0x0FACE, { 0x09F9C } },
      { 0x0FACF, { 0x2284A } },
      { 0x0FAD0, { 0x22844 } },
      { 0x0FAD1, { 0x233D5 } },
      { 0x0FAD2, { 0x03B9D } },
      { 0x0FAD3, { 0x04018 } },
      { 0x0FAD4, { 0x04039 } },
      { 0x0FAD5, { 0x25249 } },
      { 0x0FAD6, { 0x25CD0 } },
      { 0x0FAD7, { 0x27ED3 } },
      { 0x0FAD8, { 0x09F43 } },
      { 0x0FAD9, { 0x09F8E } },
      { 0x0FB00, { 0x00066, 0x00066 } },
      { 0x0FB01, { 0x00066, 0x00069 } },
      { 0x0FB02, { 0x00066, 0x0006C } },
      { 0x0FB03, { 0x00066, 0x00066, 0x00069 } },
      { 0x0FB04, { 0x00066, 0x00066, 0x0006C } },
      { 0x0FB06, { 0x00073, 0x00074 } },
      { 0x0FB13, { 0x00574, 0x00576 } },
      { 0x0FB14, { 0x00574, 0x00565 } },
      { 0x0FB15, { 0x00574, 0x0056B } },
      { 0x0FB16, { 0x0057E, 0x00576 } },
      { 0x0FB17, { 0x00574, 0x0056D } },
      { 0x0FB20, { 0x005E2 } },
      { 0x0FB21, { 0x005D0 } },
      { 0x0FB22, { 0x005D3 } },
      { 0x0FB23, { 0x005D4 } },
      { 0x0FB24, { 0x005DB } },
      { 0x0FB25, { 0x005DC } },
      { 0x0FB26, { 0x005DD } },
      { 0x0FB27, { 0x005E8 } },
      { 0x0FB28, { 0x005EA } },
      { 0x0FB29, { 0x0002D, 0x00307 } },
      { 0x0FB2B, { 0x0FB2A } },
      { 0x0FB2D, { 0x0FB2C } },
      { 0x0FB2F, { 0x0FB2E } },
      { 0x0FB30, { 0x0FB2E } },
      { 0x0FB39, { 0x0FB1D } },
      { 0x0FB49, { 0x0FB2A } },
      { 0x0FB4F, { 0x005D0, 0x005DC } },
      { 0x0FB50, { 0x00671 } },
      { 0x0FB51, { 0x00671 } },
      { 0x0FB52, { 0x0067B } },
      { 0x0FB53, { 0x0067B } },
      { 0x0FB54, { 0x0067B } },
      { 0x0FB55, { 0x0067B } },
      { 0x0FB56, { 0x00649, 0x006DB } },
      { 0x0FB57, { 0x00649, 0x006DB } },
      { 0x0FB58, { 0x00649, 0x006DB } },
      { 0x0FB59, { 0x00649, 0x006DB } },
      { 0x0FB5A, { 0x00680 } },
      { 0x0FB5B, { 0x00680 } },
      { 0x0FB5C, { 0x00680 } },
      { 0x0FB5D, { 0x00680 } },
      { 0x0FB5E, { 0x0062A } },
      { 0x0FB5F, { 0x0062A } },
      { 0x0FB60, { 0x0062A } },
      { 0x0FB61, { 0x0062A } },
      { 0x0FB62, { 0x0067F } },
      { 0x0FB63, { 0x0067F } },
      { 0x0FB64, { 0x0067F } },
      { 0x0FB65, { 0x0067F } },
      { 0x0FB66, { 0x00649, 0x00615 } },
      { 0x0FB67, { 0x00649, 0x00615 } },
      { 0x0FB68, { 0x00649, 0x00615 } },
      { 0x0FB69, { 0x00649, 0x00615 } },
      { 0x0FB6A, { 0x006A1, 0x006DB } },
      { 0x0FB6B, { 0x006A1, 0x006DB } },
      { 0x0FB6C, { 0x006A1, 0x006DB } },
      { 0x0FB6D, { 0x006A1, 0x006DB } },
      { 0x0FB6E, { 0x006A6 } },
      { 0x0FB6F, { 0x006A6 } },
      { 0x0FB70, { 0x006A6 } },
      { 0x0FB71, { 0x006A6 } },
      { 0x0FB72, { 0x00684 } },
      { 0x0FB73, { 0x00684 } },
      { 0x0FB74, { 0x00684 } },
      { 0x0FB75, { 0x00684 } },
      { 0x0FB76, { 0x00683 } },
      { 0x0FB77, { 0x00683 } },
      { 0x0FB78, { 0x00683 } },
      { 0x0FB79, { 0x00683 } },
      { 0x0FB7A, { 0x00686 } },
      { 0x0FB7B, { 0x00686 } },
      { 0x0FB7C, { 0x00686 } },
      { 0x0FB7D, { 0x00686 } },
      { 0x0FB7E, { 0x00687 } },
      { 0x0FB7F, { 0x00687 } },
      { 0x0FB80, { 0x00687 } },
      { 0x0FB81, { 0x00687 } },
      { 0x0FB82, { 0x0068D } },
      { 0x0FB83, { 0x0068D } },
      { 0x0FB84, { 0x0068C } },
      { 0x0FB85, { 0x0068C } },
      { 0x0FB86, { 0x0062F, 0x006DB } },
      { 0x0FB87, { 0x0062F, 0x006DB } },
      { 0x0FB88, { 0x0062F, 0x00615 } },
      { 0x0FB89, { 0x0062F, 0x00615 } },
      { 0x0FB8A, { 0x00631, 0x006DB } },
      { 0x0FB8B, { 0x00631, 0x006DB } },
      { 0x0FB8C, { 0x00631, 0x00615 } },
      { 0x0FB8D, { 0x00631, 0x00615 } },
      { 0x0FB8E, { 0x00643 } },
      { 0x0FB8F, { 0x00643 } },
      { 0x0FB90, { 0x00643 } },
      { 0x0FB91, { 0x00643 } },
      { 0x0FB92, { 0x006AF } },
      { 0x0FB93, { 0x006AF } },
      { 0x0FB94, { 0x006AF } },
      { 0x0FB95, { 0x006AF } },
      { 0x0FB96, { 0x006B3 } },
      { 0x0FB97, { 0x006B3 } },
      { 0x0FB98, { 0x006B3 } },
      { 0x0FB99, { 0x006B3 } },
      { 0x0FB9A, { 0x006B1 } },
      { 0x0FB9B, { 0x006B1 } },
      { 0x0FB9C, { 0x006B1 } },
      { 0x0FB9D, { 0x006B1 } },
      { 0x0FB9E, { 0x00649 } },
      { 0x0FB9F, { 0x00649 } },
      { 0x0FBA0, { 0x00649, 0x00615 } },
      { 0x0FBA1, { 0x00649, 0x00615 } },
      { 0x0FBA2, { 0x00649, 0x00615 } },
      { 0x0FBA3, { 0x00649, 0x00615 } },
      { 0x0FBA4, { 0x006C0 } },
      { 0x0FBA5, { 0x006C0 } },
      { 0x0FBA6, { 0x0006F } },
      { 0x0FBA7, { 0x0006F } },
      { 0x0FBA8, { 0x0006F } },
      { 0x0FBA9, { 0x0006F } },
      { 0x0FBAA, { 0x0006F } },
      { 0x0FBAB, { 0x0006F } },
      { 0x0FBAC, { 0x0006F } },
      { 0x0FBAD, { 0x0006F } },
      { 0x0FBAE, { 0x00649 } },
      { 0x0FBAF, { 0x00649 } },
      { 0x0FBB0, { 0x006D3 } },
      { 0x0FBB1, { 0x006D3 } },
      { 0x0FBD3, { 0x00643, 0x006DB } },
      { 0x0FBD4, { 0x00643, 0x006DB } },
      { 0x0FBD5, { 0x00643, 0x006DB } },
      { 0x0FBD6, { 0x00643, 0x006DB } },
      { 0x0FBD7, { 0x00648, 0x00313 } },
      { 0x0FBD8, { 0x00648, 0x00313 } },
      { 0x0FBD9, { 0x00648, 0x00306 } },
      { 0x0FBDA, { 0x00648, 0x00306 } },
      { 0x0FBDB, { 0x00648, 0x00670 } },
      { 0x0FBDC, { 0x00648, 0x00670 } },
      { 0x0FBDD, { 0x00648, 0x00313, 0x00674 } },
      { 0x0FBDE, { 0x00648, 0x006DB } },
      { 0x0FBDF, { 0x00648, 0x006DB } },
      { 0x0FBE0, { 0x006C5 } },
      { 0x0FBE1, { 0x006C5 } },
      { 0x0FBE2, { 0x00648, 0x00302 } },
      { 0x0FBE3, { 0x00648, 0x00302 } },
      { 0x0FBE4, { 0x0067B } },
      { 0x0FBE5, { 0x0067B } },
      { 0x0FBE6, { 0x0067B } },
      { 0x0FBE7, { 0x0067B } },
      { 0x0FBE8, { 0x00649 } },
      { 0x0FBE9, { 0x00649 } },
      { 0x0FBEA, { 0x00649, 0x00674, 0x0006C } },
      { 0x0FBEB, { 0x00649, 0x00674, 0x0006C } },
      { 0x0FBEC, { 0x00649, 0x00674, 0x0006F } },
      { 0x0FBED, { 0x00649, 0x00674, 0x0006F } },
      { 0x0FBEE, { 0x00649, 0x00674, 0x00648 } },
      { 0x0FBEF, { 0x00649, 0x00674, 0x00648 } },
      { 0x0FBF0, { 0x00649, 0x00674, 0x00648, 0x00313 } },
      { 0x0FBF1, { 0x00649, 0x00674, 0x00648, 0x00313 } },
      { 0x0FBF2, { 0x00649, 0x00674, 0x00648, 0x00306 } },
      { 0x0FBF3, { 0x00649, 0x00674, 0x00648, 0x00306 } },
      { 0x0FBF4, { 0x00649, 0x00674, 0x00648, 0x00670 } },
      { 0x0FBF5, { 0x00649, 0x00674, 0x00648, 0x00670 } },
      { 0x0FBF6, { 0x00649, 0x00674, 0x0067B } },
      { 0x0FBF7, { 0x00649, 0x00674, 0x0067B } },
      { 0x0FBF8, { 0x00649, 0x00674, 0x0067B } },
      { 0x0FBF9, { 0x00649, 0x00674, 0x00649 } },
      { 0x0FBFA, { 0x00649, 0x00674, 0x00649 } },
      { 0x0FBFB, { 0x00649, 0x00674, 0x00649 } },
      { 0x0FBFC, { 0x00649 } },
      { 0x0FBFD, { 0x00649 } },
      { 0x0FBFE, { 0x00649 } },
      { 0x0FBFF, { 0x00649 } },
      { 0x0FC00, { 0x00649, 0x00674, 0x0062C } },
      { 0x0FC01, { 0x00649, 0x00674, 0x0062D } },
      { 0x0FC02, { 0x00649, 0x00674, 0x00645 } },
      { 0x0FC03, { 0x00649, 0x00674, 0x00649 } },
      { 0x0FC04, { 0x00649, 0x00674, 0x00649 } },
      { 0x0FC05, { 0x00628, 0x0062C } },
      { 0x0FC06, { 0x00628, 0x0062D } },
      { 0x0FC07, { 0x00628, 0x0062E } },
      { 0x0FC08, { 0x00628, 0x00645 } },
      { 0x0FC09, { 0x00628, 0x00649 } },
      { 0x0FC0A, { 0x00628, 0x00649 } },
      { 0x0FC0B, { 0x0062A, 0x0062C } },
      { 0x0FC0C, { 0x0062A, 0x0062D } },
      { 0x0FC0D, { 0x0062A, 0x0062E } },
      { 0x0FC0E, { 0x0062A, 0x00645 } },
      { 0x0FC0F, { 0x0062A, 0x00649 } },
      { 0x0FC10, { 0x0062A, 0x00649 } },
      { 0x0FC11, { 0x00649, 0x006DB, 0x0062C } },
      { 0x0FC12, { 0x00649, 0x006DB, 0x00645 } },
      { 0x0FC13, { 0x00649, 0x006DB, 0x00649 } },
      { 0x0FC14, { 0x00649, 0x006DB, 0x00649 } },
      { 0x0FC15, { 0x0062C, 0x0062D } },
      { 0x0FC16, { 0x0062C, 0x00645 } },
      { 0x0FC17, { 0x0062D, 0x0062C } },
      { 0x0FC18, { 0x0062D, 0x00645 } },
      { 0x0FC19, { 0x0062E, 0x0062C } },
      { 0x0FC1A, { 0x0062E, 0x0062D } },
      { 0x0FC1B, { 0x0062E, 0x00645 } },
      { 0x0FC1C, { 0x00633, 0x0062C } },
      { 0x0FC1D, { 0x00633, 0x0062D } },
      { 0x0FC1E, { 0x00633, 0x0062E } },
      { 0x0FC1F, { 0x00633, 0x00645 } },
      { 0x0FC20, { 0x00635, 0x0062D } },
      { 0x0FC21, { 0x00635, 0x00645 } },
      { 0x0FC22, { 0x00636, 0x0062C } },
      { 0x0FC23, { 0x00636, 0x0062D } },
      { 0x0FC24, { 0x00636, 0x0062E } },
      { 0x0FC25, { 0x00636, 0x00645 } },
      { 0x0FC26, { 0x00637, 0x0062D } },
      { 0x0FC27, { 0x00637, 0x00645 } },
      { 0x0FC28, { 0x00638, 0x00645 } },
      { 0x0FC29, { 0x00639, 0x0062C } },
      { 0x0FC2A, { 0x00639, 0x00645 } },
      { 0x0FC2B, { 0x0063A, 0x0062C } },
      { 0x0FC2C, { 0x0063A, 0x00645 } },
      { 0x0FC2D, { 0x00641, 0x0062C } },
      { 0x0FC2E, { 0x00641, 0x0062D } },
      { 0x0FC2F, { 0x00641, 0x0062E } },
      { 0x0FC30, { 0x00641, 0x00645 } },
      { 0x0FC31, { 0x00641, 0x00649 } },
      { 0x0FC32, { 0x00641, 0x00649 } },
      { 0x0FC33, { 0x00642, 0x0062D } },
      { 0x0FC34, { 0x00642, 0x00645 } },
      { 0x0FC35, { 0x00642, 0x00649 } },
      { 0x0FC36, { 0x00642, 0x00649 } },
      { 0x0FC37, { 0x00643, 0x0006C } },
      { 0x0FC38, { 0x00643, 0x0062C } },
      { 0x0FC39, { 0x00643, 0x0062D } },
      { 0x0FC3A, { 0x00643, 0x0062E } },
      { 0x0FC3B, { 0x00643, 0x00644 } },
      { 0x0FC3C, { 0x00643, 0x00645 } },
      { 0x0FC3D, { 0x00643, 0x00649 } },
      { 0x0FC3E, { 0x00643, 0x00649 } },
      { 0x0FC3F, { 0x00644, 0x0062C } },
      { 0x0FC40, { 0x00644, 0x0062D } },
      { 0x0FC41, { 0x00644, 0x0062E } },
      { 0x0FC42, { 0x00644, 0x00645 } },
      { 0x0FC43, { 0x00644, 0x00649 } },
      { 0x0FC44, { 0x00644, 0x00649 } },
      { 0x0FC45, { 0x00645, 0x0062C } },
      { 0x0FC46, { 0x00645, 0x0062D } },
      { 0x0FC47, { 0x00645, 0x0062E } },
      { 0x0FC48, { 0x00645, 0x00645 } },
      { 0x0FC49, { 0x00645, 0x00649 } },
      { 0x0FC4A, { 0x00645, 0x00649 } },
      { 0x0FC4B, { 0x00628, 0x0062E } },
      { 0x0FC4C, { 0x00646, 0x0062D } },
      { 0x0FC4D, { 0x00646, 0x0062E } },
      { 0x0FC4E, { 0x00646, 0x00645 } },
      { 0x0FC4F, { 0x00646, 0x00649 } },
      { 0x0FC50, { 0x00646, 0x00649 } },
      { 0x0FC51, { 0x0006F, 0x0062C } },
      { 0x0FC52, { 0x0006F, 0x00645 } },
      { 0x0FC53, { 0x0006F, 0x00649 } },
      { 0x0FC54, { 0x0006F, 0x00649 } },
      { 0x0FC55, { 0x00649, 0x0062C } },
      { 0x0FC56, { 0x00649, 0x0062D } },
      { 0x0FC57, { 0x00649, 0x0062E } },
      { 0x0FC58, { 0x00649, 0x00645 } },
      { 0x0FC59, { 0x00649, 0x00649 } },
      { 0x0FC5A, { 0x00649, 0x00649 } },
      { 0x0FC5B, { 0x00630, 0x00670 } },
      { 0x0FC5C, { 0x00631, 0x00670 } },
      { 0x0FC5D, { 0x00649, 0x00670 } },
      { 0x0FC5E, { 0x0FE72, 0x00651 } },
      { 0x0FC5F, { 0x0FE74, 0x00651 } },
      { 0x0FC60, { 0x0FE76, 0x00651 } },
      { 0x0FC61, { 0x0FE78, 0x00651 } },
      { 0x0FC62, { 0x0FE7A, 0x00651 } },
      { 0x0FC63, { 0x0FE7C, 0x00670 } },
      { 0x0FC64, { 0x00649, 0x00674, 0x00631 } },
      { 0x0FC65, { 0x00649, 0x00674, 0x00632 } },
      { 0x0FC66, { 0x00649, 0x00674, 0x00645 } },
      { 0x0FC67, { 0x00649, 0x00674, 0x00646 } },
      { 0x0FC68, { 0x00649, 0x00674, 0x00649 } },
      { 0x0FC69, { 0x00649, 0x00674, 0x00649 } },
      { 0x0FC6A, { 0x00628, 0x00631 } },
      { 0x0FC6B, { 0x00628, 0x00632 } },
      { 0x0FC6C, { 0x00628, 0x00645 } },
      { 0x0FC6D, { 0x00628, 0x00646 } },
      { 0x0FC6E, { 0x00628, 0x00649 } },
      { 0x0FC6F, { 0x00628, 0x00649 } },
      { 0x0FC70, { 0x0062A, 0x00631 } },
      { 0x0FC71, { 0x0062A, 0x00632 } },
      { 0x0FC72, { 0x0062A, 0x00645 } },
      { 0x0FC73, { 0x0062A, 0x00646 } },
      { 0x0FC74, { 0x0062A, 0x00649 } },
      { 0x0FC75, { 0x0062A, 0x00649 } },
      { 0x0FC76, { 0x00649, 0x006DB, 0x00631 } },
      { 0x0FC77, { 0x00649, 0x006DB, 0x00632 } },
      { 0x0FC78, { 0x00649, 0x006DB, 0x00645 } },
      { 0x0FC79, { 0x00649, 0x006DB, 0x00646 } },
      { 0x0FC7A, { 0x00649, 0x006DB, 0x00649 } },
      { 0x0FC7B, { 0x00649, 0x006DB, 0x00649 } },
      { 0x0FC7C, { 0x00641, 0x00649 } },
      { 0x0FC7D, { 0x00641, 0x00649 } },
      { 0x0FC7E, { 0x00642, 0x00649 } },
      { 0x0FC7F, { 0x00642, 0x00649 } },
      { 0x0FC80, { 0x00643, 0x0006C } },
      { 0x0FC81, { 0x00643, 0x00644 } },
      { 0x0FC82, { 0x00643, 0x00645 } },
      { 0x0FC83, { 0x00643, 0x00649 } },
      { 0x0FC84, { 0x00643, 0x00649 } },
      { 0x0FC85, { 0x00644, 0x00645 } },
      { 0x0FC86, { 0x00644, 0x00649 } },
      { 0x0FC87, { 0x00644, 0x00649 } },
      { 0x0FC88, { 0x00645, 0x0006C } },
      { 0x0FC89, { 0x00645, 0x00645 } },
      { 0x0FC8A, { 0x00646, 0x00631 } },
      { 0x0FC8B, { 0x00646, 0x00632 } },
      { 0x0FC8C, { 0x00646, 0x00645 } },
      { 0x0FC8D, { 0x00646, 0x00646 } },
      { 0x0FC8E, { 0x00646, 0x00649 } },
      { 0x0FC8F, { 0x00646, 0x00649 } },
      { 0x0FC90, { 0x00649, 0x00670 } },
      { 0x0FC91, { 0x00649, 0x00631 } },
      { 0x0FC92, { 0x00649, 0x00632 } },
      { 0x0FC93, { 0x00649, 0x00645 } },
      { 0x0FC94, { 0x00649, 0x00646 } },
      { 0x0FC95, { 0x00649, 0x00649 } },
      { 0x0FC96, { 0x00649, 0x00649 } },
      { 0x0FC97, { 0x00649, 0x00674, 0x0062C } },
      { 0x0FC98, { 0x00649, 0x00674, 0x0062D } },
      { 0x0FC99, { 0x00649, 0x00674, 0x0062E } },
      { 0x0FC9A, { 0x00649, 0x00674, 0x00645 } },
      { 0x0FC9B, { 0x00649, 0x00674, 0x0006F } },
      { 0x0FC9C, { 0x00628, 0x0062C } },
      { 0x0FC9D, { 0x00628, 0x0062D } },
      { 0x0FC9E, { 0x00628, 0x0062E } },
      { 0x0FC9F, { 0x00628, 0x00645 } },
      { 0x0FCA0, { 0x00628, 0x0006F } },
      { 0x0FCA1, { 0x0062A, 0x0062C } },
      { 0x0FCA2, { 0x0062A, 0x0062D } },
      { 0x0FCA3, { 0x0062A, 0x0062E } },
      { 0x0FCA4, { 0x0062A, 0x00645 } },
      { 0x0FCA5, { 0x0062A, 0x0006F } },
      { 0x0FCA6, { 0x00649, 0x006DB, 0x00645 } },
      { 0x0FCA7, { 0x0062C, 0x0062D } },
      { 0x0FCA8, { 0x0062C, 0x00645 } },
      { 0x0FCA9, { 0x0062D, 0x0062C } },
      { 0x0FCAA, { 0x0062D, 0x00645 } },
      { 0x0FCAB, { 0x0062E, 0x0062C } },
      { 0x0FCAC, { 0x0062E, 0x00645 } },
      { 0x0FCAD, { 0x00633, 0x0062C } },
      { 0x0FCAE, { 0x00633, 0x0062D } },
      { 0x0FCAF, { 0x00633, 0x0062E } },
      { 0x0FCB0, { 0x00633, 0x00645 } },
      { 0x0FCB1, { 0x00635, 0x0062D } },
      { 0x0FCB2, { 0x00635, 0x0062E } },
      { 0x0FCB3, { 0x00635, 0x00645 } },
      { 0x0FCB4, { 0x00636, 0x0062C } },
      { 0x0FCB5, { 0x00636, 0x0062D } },
      { 0x0FCB6, { 0x00636, 0x0062E } },
      { 0x0FCB7, { 0x00636, 0x00645 } },
      { 0x0FCB8, { 0x00637, 0x0062D } },
      { 0x0FCB9, { 0x00638, 0x00645 } },
      { 0x0FCBA, { 0x00639, 0x0062C } },
      { 0x0FCBB, { 0x00639, 0x00645 } },
      { 0x0FCBC, { 0x0063A, 0x0062C } },
      { 0x0FCBD, { 0x0063A, 0x00645 } },
      { 0x0FCBE, { 0x00641, 0x0062C } },
      { 0x0FCBF, { 0x00641, 0x0062D } },
      { 0x0FCC0, { 0x00641, 0x0062E } },
      { 0x0FCC1, { 0x00641, 0x00645 } },
      { 0x0FCC2, { 0x00642, 0x0062D } },
      { 0x0FCC3, { 0x00642, 0x00645 } },
      { 0x0FCC4, { 0x00643, 0x0062C } },
      { 0x0FCC5, { 0x00643, 0x0062D } },
      { 0x0FCC6, { 0x00643, 0x0062E } },
      { 0x0FCC7, { 0x00643, 0x00644 } },
      { 0x0FCC8, { 0x00643, 0x00645 } },
      { 0x0FCC9, { 0x00644, 0x0062C } },
      { 0x0FCCA, { 0x00644, 0x0062D } },
      { 0x0FCCB, { 0x00644, 0x0062E } },
      { 0x0FCCC, { 0x00644, 0x00645 } },
      { 0x0FCCD, { 0x00644, 0x0006F } },
      { 0x0FCCE, { 0x00645, 0x0062C } },
      { 0x0FCCF, { 0x00645, 0x0062D } },
      { 0x0FCD0, { 0x00645, 0x0062E } },
      { 0x0FCD1, { 0x00645, 0x00645 } },
      { 0x0FCD2, { 0x00628, 0x0062E } },
      { 0x0FCD3, { 0x00646, 0x0062D } },
      { 0x0FCD4, { 0x00646, 0x0062E } },
      { 0x0FCD5, { 0x00646, 0x00645 } },
      { 0x0FCD6, { 0x00646, 0x0006F } },
      { 0x0FCD7, { 0x0006F, 0x0062C } },
      { 0x0FCD8, { 0x0006F, 0x00645 } },
      { 0x0FCD9, { 0x0006F, 0x00670 } },
      { 0x0FCDA, { 0x00649, 0x0062C } },
      { 0x0FCDB, { 0x00649, 0x0062D } },
      { 0x0FCDC, { 0x00649, 0x0062E } },
      { 0x0FCDD, { 0x00649, 0x00645 } },
      { 0x0FCDE, { 0x00649, 0x0006F } },
      { 0x0FCDF, { 0x00649, 0x00674, 0x00645 } },
      { 0x0FCE0, { 0x00649, 0x00674, 0x0006F } },
      { 0x0FCE1, { 0x00628, 0x00645 } },
      { 0x0FCE2, { 0x00628, 0x0006F } },
      { 0x0FCE3, { 0x0062A, 0x00645 } },
      { 0x0FCE4, { 0x0062A, 0x0006F } },
      { 0x0FCE5, { 0x00649, 0x006DB, 0x00645 } },
      { 0x0FCE6, { 0x00649, 0x006DB, 0x0006F } },
      { 0x0FCE7, { 0x00633, 0x00645 } },
      { 0x0FCE8, { 0x00633, 0x0006F } },
      { 0x0FCE9, { 0x00633, 0x006DB, 0x00645 } },
      { 0x0FCEA, { 0x00633, 0x006DB, 0x0006F } },
      { 0x0FCEB, { 0x00643, 0x00644 } },
      { 0x0FCEC, { 0x00643, 0x00645 } },
      { 0x0FCED, { 0x00644, 0x00645 } },
      { 0x0FCEE, { 0x00646, 0x00645 } },
      { 0x0FCEF, { 0x00646, 0x0006F } },
      { 0x0FCF0, { 0x00649, 0x00645 } },
      { 0x0FCF1, { 0x00649, 0x0006F } },
      { 0x0FCF2, { 0x0FE77, 0x00651 } },
      { 0x0FCF3, { 0x0FE79, 0x00651 } },
      { 0x0FCF4, { 0x0FE7B, 0x00651 } },
      { 0x0FCF5, { 0x00637, 0x00649 } },
      { 0x0FCF6, { 0x00637, 0x00649 } },
      { 0x0FCF7, { 0x00639, 0x00649 } },
      { 0x0FCF8, { 0x00639, 0x00649 } },
      { 0x0FCF9, { 0x0063A, 0x00649 } },
      { 0x0FCFA, { 0x0063A, 0x00649 } },
      { 0x0FCFB, { 0x00633, 0x00649 } },
      { 0x0FCFC, { 0x00633, 0x00649 } },
      { 0x0FCFD, { 0x00633, 0x006DB, 0x00649 } },
      { 0x0FCFE, { 0x00633, 0x006DB, 0x00649 } },
      { 0x0FCFF, { 0x0062D, 0x00649 } },
      { 0x0FD00, { 0x0062D, 0x00649 } },
      { 0x0FD01, { 0x0062C, 0x00649 } },
      { 0x0FD02, { 0x0062C, 0x00649 } },
      { 0x0FD03, { 0x0062E, 0x00649 } },
      { 0x0FD04, { 0x0062E, 0x00649 } },
      { 0x0FD05, { 0x00635, 0x00649 } },
      { 0x0FD06, { 0x00635, 0x00649 } },
      { 0x0FD07, { 0x00636, 0x00649 } },
      { 0x0FD08, { 0x00636, 0x00649 } },
      { 0x0FD09, { 0x00633, 0x006DB, 0x0062C } },
      { 0x0FD0A, { 0x00633, 0x006DB, 0x0062D } },
      { 0x0FD0B, { 0x00633, 0x006DB, 0x0062E } },
      { 0x0FD0C, { 0x00633, 0x006DB, 0x00645 } },
      { 0x0FD0D, { 0x00633, 0x006DB, 0x00631 } },
      { 0x0FD0E, { 0x00633, 0x00631 } },
      { 0x0FD0F, { 0x00635, 0x00631 } },
      { 0x0FD10, { 0x00636, 0x00631 } },
      { 0x0FD11, { 0x00637, 0x00649 } },
      { 0x0FD12, { 0x00637, 0x00649 } },
      { 0x0FD13, { 0x00639, 0x00649 } },
      { 0x0FD14, { 0x00639, 0x00649 } },
      { 0x0FD15, { 0x0063A, 0x00649 } },
      { 0x0FD16, { 0x0063A, 0x00649 } },
      { 0x0FD17, { 0x00633, 0x00649 } },
      { 0x0FD18, { 0x00633, 0x00649 } },
      { 0x0FD19, { 0x00633, 0x006DB, 0x00649 } },
      { 0x0FD1A, { 0x00633, 0x006DB, 0x00649 } },
      { 0x0FD1B, { 0x0062D, 0x00649 } },
      { 0x0FD1C, { 0x0062D, 0x00649 } },
      { 0x0FD1D, { 0x0062C, 0x00649 } },
      { 0x0FD1E, { 0x0062C, 0x00649 } },
      { 0x0FD1F, { 0x0062E, 0x00649 } },
      { 0x0FD20, { 0x0062E, 0x00649 } },
      { 0x0FD21, { 0x00635, 0x00649 } },
      { 0x0FD22, { 0x00635, 0x00649 } },
      { 0x0FD23, { 0x00636, 0x00649 } },
      { 0x0FD24, { 0x00636, 0x00649 } },
      { 0x0FD25, { 0x00633, 0x006DB, 0x0062C } },
      { 0x0FD26, { 0x00633, 0x006DB, 0x0062D } },
      { 0x0FD27, { 0x00633, 0x006DB, 0x0062E } },
      { 0x0FD28, { 0x00633, 0x006DB, 0x00645 } },
      { 0x0FD29, { 0x00633, 0x006DB, 0x00631 } },
      { 0x0FD2A, { 0x00633, 0x00631 } },
      { 0x0FD2B, { 0x00635, 0x00631 } },
      { 0x0FD2C, { 0x00636, 0x00631 } },
      { 0x0FD2D, { 0x00633, 0x006DB, 0x0062C } },
      { 0x0FD2E, { 0x00633, 0x006DB, 0x0062D } },
      { 0x0FD2F, { 0x00633, 0x006DB, 0x0062E } },
      { 0x0FD30, { 0x00633, 0x006DB, 0x00645 } },
      { 0x0FD31, { 0x00633, 0x0006F } },
      { 0x0FD32, { 0x00633, 0x006DB, 0x0006F } },
      { 0x0FD33, { 0x00637, 0x00645 } },
      { 0x0FD34, { 0x00633, 0x0062C } },
      { 0x0FD35, { 0x00633, 0x0062D } },
      { 0x0FD36, { 0x00633, 0x0062E } },
      { 0x0FD37, { 0x00633, 0x006DB, 0x0062C } },
      { 0x0FD38, { 0x00633, 0x006DB, 0x0062D } },
      { 0x0FD39, { 0x00633, 0x006DB, 0x0062E } },
      { 0x0FD3A, { 0x00637, 0x00645 } },
      { 0x0FD3B, { 0x00638, 0x00645 } },
      { 0x0FD3C, { 0x0006C, 0x0030B } },
      { 0x0FD3D, { 0x0006C, 0x0030B } },
      { 0x0FD3E, { 0x00028 } },
      { 0x0FD3F, { 0x00029 } },
      { 0x0FD50, { 0x0062A, 0x0062C, 0x00645 } },
      { 0x0FD51, { 0x0062A, 0x0062D, 0x0062C } },
      { 0x0FD52, { 0x0062A, 0x0062D, 0x0062C } },
      { 0x0FD53, { 0x0062A, 0x0062D, 0x00645 } },
      { 0x0FD54, { 0x0062A, 0x0062E, 0x00645 } },
      { 0x0FD55, { 0x0062A, 0x00645, 0x0062C } },
      { 0x0FD56, { 0x0062A, 0x00645, 0x0062D } },
      { 0x0FD57, { 0x0062A, 0x00645, 0x0062E } },
      { 0x0FD58, { 0x0062C, 0x00645, 0x0062D } },
      { 0x0FD59, { 0x0062C, 0x00645, 0x0062D } },
      { 0x0FD5A, { 0x0062D, 0x00645, 0x00649 } },
      { 0x0FD5B, { 0x0062D, 0x00645, 0x00649 } },
      { 0x0FD5C, { 0x00633, 0x0062D, 0x0062C } },
      { 0x0FD5D, { 0x00633, 0x0062C, 0x0062D } },
      { 0x0FD5E, { 0x00633, 0x0062C, 0x00649 } },
      { 0x0FD5F, { 0x00633, 0x00645, 0x0062D } },
      { 0x0FD60, { 0x00633, 0x00645, 0x0062D } },
      { 0x0FD61, { 0x00633, 0x00645, 0x0062C } },
      { 0x0FD62, { 0x00633, 0x00645, 0x00645 } },
      { 0x0FD63, { 0x00633, 0x00645, 0x00645 } },
      { 0x0FD64, { 0x00635, 0x0062D, 0x0062D } },
      { 0x0FD65, { 0x00635, 0x0062D, 0x0062D } },
      { 0x0FD66, { 0x00635, 0x00645, 0x00645 } },
      { 0x0FD67, { 0x00633, 0x006DB, 0x0062D, 0x00645 } },
      { 0x0FD68, { 0x00633, 0x006DB, 0x0062D, 0x00645 } },
      { 0x0FD69, { 0x00633, 0x006DB, 0x0062C, 0x00649 } },
      { 0x0FD6A, { 0x00633, 0x006DB, 0x00645, 0x0062E } },
      { 0x0FD6B, { 0x00633, 0x006DB, 0x00645, 0x0062E } },
      { 0x0FD6C, { 0x00633, 0x006DB, 0x00645, 0x00645 } },
      { 0x0FD6D, { 0x00633, 0x006DB, 0x00645, 0x00645 } },
      { 0x0FD6E, { 0x00636, 0x0062D, 0x00649 } },
      { 0x0FD6F, { 0x00636, 0x0062E, 0x00645 } },
      { 0x0FD70, { 0x00636, 0x0062E, 0x00645 } },
      { 0x0FD71, { 0x00637, 0x00645, 0x0062D } },
      { 0x0FD72, { 0x00637, 0x00645, 0x0062D } },
      { 0x0FD73, { 0x00637, 0x00645, 0x00645 } },
      { 0x0FD74, { 0x00637, 0x00645, 0x00649 } },
      { 0x0FD75, { 0x00639, 0x0062C, 0x00645 } },
      { 0x0FD76, { 0x00639, 0x00645, 0x00645 } },
      { 0x0FD77, { 0x00639, 0x00645, 0x00645 } },
      { 0x0FD78, { 0x00639, 0x00645, 0x00649 } },
      { 0x0FD79, { 0x0063A, 0x00645, 0x00645 } },
      { 0x0FD7A, { 0x0063A, 0x00645, 0x00649 } },
      { 0x0FD7B, { 0x0063A, 0x00645, 0x00649 } },
      { 0x0FD7C, { 0x00641, 0x0062E, 0x00645 } },
      { 0x0FD7D, { 0x00641, 0x0062E, 0x00645 } },
      { 0x0FD7E, { 0x00642, 0x00645, 0x0062D } },
      { 0x0FD7F, { 0x00642, 0x00645, 0x00645 } },
      { 0x0FD80, { 0x00644, 0x0062D, 0x00645 } },
      { 0x0FD81, { 0x00644, 0x0062D, 0x00649 } },
      { 0x0FD82, { 0x00644, 0x0062D, 0x00649 } },
      { 0x0FD83, { 0x00644, 0x0062C, 0x0062C } },
      { 0x0FD84, { 0x00644, 0x0062C, 0x0062C } },
      { 0x0FD85, { 0x00644, 0x0062E, 0x00645 } },
      { 0x0FD86, { 0x00644, 0x0062E, 0x00645 } },
      { 0x0FD87, { 0x00644, 0x00645, 0x0062D } },
      { 0x0FD88, { 0x00644, 0x00645, 0x0062D } },
      { 0x0FD89, { 0x00645, 0x0062D, 0x0062C } },
      { 0x0FD8A, { 0x00645, 0x0062D, 0x00645 } },
      { 0x0FD8B, { 0x00645, 0x0062D, 0x00649 } },
      { 0x0FD8C, { 0x00645, 0x0062C, 0x0062D } },
      { 0x0FD8D, { 0x00645, 0x0062C, 0x00645 } },
      { 0x0FD8E, { 0x00645, 0x0062E, 0x0062C } },
      { 0x0FD8F, { 0x00645, 0x0062E, 0x00645 } },
      { 0x0FD92, { 0x00645, 0x0062C, 0x0062E } },
      { 0x0FD93, { 0x0006F, 0x00645, 0x0062C } },
      { 0x0FD94, { 0x0006F, 0x00645, 0x00645 } },
      { 0x0FD95, { 0x00646, 0x0062D, 0x00645 } },
      { 0x0FD96, { 0x00646, 0x0062D, 0x00649 } },
      { 0x0FD97, { 0x00646, 0x0062C, 0x00645 } },
      { 0x0FD98, { 0x00646, 0x0062C, 0x00645 } },
      { 0x0FD99, { 0x00646, 0x0062C, 0x00649 } },
      { 0x0FD9A, { 0x00646, 0x00645, 0x00649 } },
      { 0x0FD9B, { 0x00646, 0x00645, 0x00649 } },
      { 0x0FD9C, { 0x00649, 0x00645, 0x00645 } },
      { 0x0FD9D, { 0x00649, 0x00645, 0x00645 } },
      { 0x0FD9E, { 0x00628, 0x0062E, 0x00649 } },
      { 0x0FD9F, { 0x0062A, 0x0062C, 0x00649 } },
      { 0x0FDA0, { 0x0062A, 0x0062C, 0x00649 } },
      { 0x0FDA1, { 0x0062A, 0x0062E, 0x00649 } },
      { 0x0FDA2, { 0x0062A, 0x0062E, 0x00649 } },
      { 0x0FDA3, { 0x0062A, 0x00645, 0x00649 } },
      { 0x0FDA4, { 0x0062A, 0x00645, 0x00649 } },
      { 0x0FDA5, { 0x0062C, 0x00645, 0x00649 } },
      { 0x0FDA6, { 0x0062C, 0x0062D, 0x00649 } },
      { 0x0FDA7, { 0x0062C, 0x00645, 0x00649 } },
      { 0x0FDA8, { 0x00633, 0x0062E, 0x00649 } },
      { 0x0FDA9, { 0x00635, 0x0062D, 0x00649 } },
      { 0x0FDAA, { 0x00633, 0x006DB, 0x0062D, 0x00649 } },
      { 0x0FDAB, { 0x00636, 0x0062D, 0x00649 } },
      { 0x0FDAC, { 0x00644, 0x0062C, 0x00649 } },
      { 0x0FDAD, { 0x00644, 0x00645, 0x00649 } },
      { 0x0FDAE, { 0x00649, 0x0062D, 0x00649 } },
      { 0x0FDAF, { 0x00649, 0x0062C, 0x00649 } },
      { 0x0FDB0, { 0x00649, 0x00645, 0x00649 } },
      { 0x0FDB1, { 0x00645, 0x00645, 0x00649 } },
      { 0x0FDB2, { 0x00642, 0x00645, 0x00649 } },
      { 0x0FDB3, { 0x00646, 0x0062D, 0x00649 } },
      { 0x0FDB4, { 0x00642, 0x00645, 0x0062D } },
      { 0x0FDB5, { 0x00644, 0x0062D, 0x00645 } },
      { 0x0FDB6, { 0x00639, 0x00645, 0x00649 } },
      { 0x0FDB7, { 0x00643, 0x00645, 0x00649 } },
      { 0x0FDB8, { 0x00646, 0x0062C, 0x0062D } },
      { 0x0FDB9, { 0x00645, 0x0062E, 0x00649 } },
      { 0x0FDBA, { 0x00644, 0x0062C, 0x00645 } },
      { 0x0FDBB, { 0x00643, 0x00645, 0x00645 } },
      { 0x0FDBC, { 0x00644, 0x0062C, 0x00645 } },
      { 0x0FDBD, { 0x00646, 0x0062C, 0x0062D } },
      { 0x0FDBE, { 0x0062C, 0x0062D, 0x00649 } },
      { 0x0FDBF, { 0x0062D, 0x0062C, 0x00649 } },
      { 0x0FDC0, { 0x00645, 0x0062C, 0x00649 } },
      { 0x0FDC1, { 0x00641, 0x00645, 0x00649 } },
      { 0x0FDC2, { 0x00628, 0x0062D, 0x00649 } },
      { 0x0FDC3, { 0x00643, 0x00645, 0x00645 } },
      { 0x0FDC4, { 0x00639, 0x0062C, 0x00645 } },
      { 0x0FDC5, { 0x00635, 0x00645, 0x00645 } },
      { 0x0FDC6, { 0x00633, 0x0062E, 0x00649 } },
      { 0x0FDC7, { 0x00646, 0x0062C, 0x00649 } },
      { 0x0FDF0, { 0x00635, 0x00644, 0x00649 } },
      { 0x0FDF1, { 0x00642, 0x00644, 0x00649 } },
      { 0x0FDF2, { 0x0006C, 0x00644, 0x00644, 0x00651, 0x00670,
                   0x0006F } },
      { 0x0FDF3, { 0x0006C, 0x00643, 0x00628, 0x00631 } },
      { 0x0FDF4, { 0x00645, 0x0062D, 0x00645, 0x0062F } },
      { 0x0FDF5, { 0x00635, 0x00644, 0x00639, 0x00645 } },
      { 0x0FDF6, { 0x00631, 0x00633, 0x00648, 0x00644 } },
      { 0x0FDF7, { 0x00639, 0x00644, 0x00649, 0x0006F } },
      { 0x0FDF8, { 0x00648, 0x00633, 0x00644, 0x00645 } },
      { 0x0FDF9, { 0x00635, 0x00644, 0x00649 } },
      { 0x0FDFA, { 0x00635, 0x00644, 0x00649, 0x00020, 0x0006C,
                   0x00644, 0x00644, 0x0006F, 0x00020, 0x00639,
                   0x00644, 0x00649, 0x0006F, 0x00020, 0x00648,
                   0x00633, 0x00644, 0x00645 } },
      { 0x0FDFB, { 0x0062C, 0x00644, 0x00020, 0x0062C, 0x00644,
                   0x0006C, 0x00644, 0x0006F } },
      { 0x0FDFC, { 0x00631, 0x00649, 0x0006C, 0x00644 } },
      { 0x0FE19, { 0x02D57 } },
      { 0x0FE30, { 0x0003A } },
      { 0x0FE31, { 0x02502 } },
      { 0x0FE34, { 0x02307 } },
      { 0x0FE35, { 0x023DC } },
      { 0x0FE36, { 0x023DD } },
      { 0x0FE37, { 0x023DE } },
      { 0x0FE38, { 0x023DF } },
      { 0x0FE39, { 0x023E0 } },
      { 0x0FE3A, { 0x023E1 } },
      { 0x0FE49, { 0x002C9 } },
      { 0x0FE4A, { 0x002C9 } },
      { 0x0FE4B, { 0x002C9 } },
      { 0x0FE4C, { 0x002C9 } },
      { 0x0FE4D, { 0x0005F } },
      { 0x0FE4E, { 0x0005F } },
      { 0x0FE4F, { 0x0005F } },
      { 0x0FE58, { 0x0002D } },
      { 0x0FE68, { 0x0005C } },
      { 0x0FE80, { 0x00621 } },
      { 0x0FE81, { 0x00622 } },
      { 0x0FE82, { 0x00622 } },
      { 0x0FE83, { 0x0006C, 0x00674 } },
      { 0x0FE84, { 0x0006C, 0x00674 } },
      { 0x0FE85, { 0x00648, 0x00674 } },
      { 0x0FE86, { 0x00648, 0x00674 } },
      { 0x0FE87, { 0x0006C, 0x00655 } },
      { 0x0FE88, { 0x0006C, 0x00655 } },
      { 0x0FE89, { 0x00649, 0x00674 } },
      { 0x0FE8A, { 0x00649, 0x00674 } },
      { 0x0FE8B, { 0x00649, 0x00674 } },
      { 0x0FE8C, { 0x00649, 0x00674 } },
      { 0x0FE8D, { 0x0006C } },
      { 0x0FE8E, { 0x0006C } },
      { 0x0FE8F, { 0x00628 } },
      { 0x0FE90, { 0x00628 } },
      { 0x0FE91, { 0x00628 } },
      { 0x0FE92, { 0x00628 } },
      { 0x0FE93, { 0x00629 } },
      { 0x0FE94, { 0x00629 } },
      { 0x0FE95, { 0x0062A } },
      { 0x0FE96, { 0x0062A } },
      { 0x0FE97, { 0x0062A } },
      { 0x0FE98, { 0x0062A } },
      { 0x0FE99, { 0x00649, 0x006DB } },
      { 0x0FE9A, { 0x00649, 0x006DB } },
      { 0x0FE9B, { 0x00649, 0x006DB } },
      { 0x0FE9C, { 0x00649, 0x006DB } },
      { 0x0FE9D, { 0x0062C } },
      { 0x0FE9E, { 0x0062C } },
      { 0x0FE9F, { 0x0062C } },
      { 0x0FEA0, { 0x0062C } },
      { 0x0FEA1, { 0x0062D } },
      { 0x0FEA2, { 0x0062D } },
      { 0x0FEA3, { 0x0062D } },
      { 0x0FEA4, { 0x0062D } },
      { 0x0FEA5, { 0x0062E } },
      { 0x0FEA6, { 0x0062E } },
      { 0x0FEA7, { 0x0062E } },
      { 0x0FEA8, { 0x0062E } },
      { 0x0FEA9, { 0x0062F } },
      { 0x0FEAA, { 0x0062F } },
      { 0x0FEAB, { 0x00630 } },
      { 0x0FEAC, { 0x00630 } },
      { 0x0FEAD, { 0x00631 } },
      { 0x0FEAE, { 0x00631 } },
      { 0x0FEAF, { 0x00632 } },
      { 0x0FEB0, { 0x00632 } },
      { 0x0FEB1, { 0x00633 } },
      { 0x0FEB2, { 0x00633 } },
      { 0x0FEB3, { 0x00633 } },
      { 0x0FEB4, { 0x00633 } },
      { 0x0FEB5, { 0x00633, 0x006DB } },
      { 0x0FEB6, { 0x00633, 0x006DB } },
      { 0x0FEB7, { 0x00633, 0x006DB } },
      { 0x0FEB8, { 0x00633, 0x006DB } },
      { 0x0FEB9, { 0x00635 } },
      { 0x0FEBA, { 0x00635 } },
      { 0x0FEBB, { 0x00635 } },
      { 0x0FEBC, { 0x00635 } },
      { 0x0FEBD, { 0x00636 } },
      { 0x0FEBE, { 0x00636 } },
      { 0x0FEBF, { 0x00636 } },
      { 0x0FEC0, { 0x00636 } },
      { 0x0FEC1, { 0x00637 } },
      { 0x0FEC2, { 0x00637 } },
      { 0x0FEC3, { 0x00637 } },
      { 0x0FEC4, { 0x00637 } },
      { 0x0FEC5, { 0x00638 } },
      { 0x0FEC6, { 0x00638 } },
      { 0x0FEC7, { 0x00638 } },
      { 0x0FEC8, { 0x00638 } },
      { 0x0FEC9, { 0x00639 } },
      { 0x0FECA, { 0x00639 } },
      { 0x0FECB, { 0x00639 } },
      { 0x0FECC, { 0x00639 } },
      { 0x0FECD, { 0x0063A } },
      { 0x0FECE, { 0x0063A } },
      { 0x0FECF, { 0x0063A } },
      { 0x0FED0, { 0x0063A } },
      { 0x0FED1, { 0x00641 } },
      { 0x0FED2, { 0x00641 } },
      { 0x0FED3, { 0x00641 } },
      { 0x0FED4, { 0x00641 } },
      { 0x0FED5, { 0x00642 } },
      { 0x0FED6, { 0x00642 } },
      { 0x0FED7, { 0x00642 } },
      { 0x0FED8, { 0x00642 } },
      { 0x0FED9, { 0x00643 } },
      { 0x0FEDA, { 0x00643 } },
      { 0x0FEDB, { 0x00643 } },
      { 0x0FEDC, { 0x00643 } },
      { 0x0FEDD, { 0x00644 } },
      { 0x0FEDE, { 0x00644 } },
      { 0x0FEDF, { 0x00644 } },
      { 0x0FEE0, { 0x00644 } },
      { 0x0FEE1, { 0x00645 } },
      { 0x0FEE2, { 0x00645 } },
      { 0x0FEE3, { 0x00645 } },
      { 0x0FEE4, { 0x00645 } },
      { 0x0FEE5, { 0x00646 } },
      { 0x0FEE6, { 0x00646 } },
      { 0x0FEE7, { 0x00646 } },
      { 0x0FEE8, { 0x00646 } },
      { 0x0FEE9, { 0x0006F } },
      { 0x0FEEA, { 0x0006F } },
      { 0x0FEEB, { 0x0006F } },
      { 0x0FEEC, { 0x0006F } },
      { 0x0FEED, { 0x00648 } },
      { 0x0FEEE, { 0x00648 } },
      { 0x0FEEF, { 0x00649 } },
      { 0x0FEF0, { 0x00649 } },
      { 0x0FEF1, { 0x00649 } },
      { 0x0FEF2, { 0x00649 } },
      { 0x0FEF3, { 0x00649 } },
      { 0x0FEF4, { 0x00649 } },
      { 0x0FEF5, { 0x00644, 0x00622 } },
      { 0x0FEF6, { 0x00644, 0x00622 } },
      { 0x0FEF7, { 0x00644, 0x0006C, 0x00674 } },
      { 0x0FEF8, { 0x00644, 0x0006C, 0x00674 } },
      { 0x0FEF9, { 0x00644, 0x0006C, 0x00655 } },
      { 0x0FEFA, { 0x00644, 0x0006C, 0x00655 } },
      { 0x0FEFB, { 0x00644, 0x0006C } },
      { 0x0FEFC, { 0x00644, 0x0006C } },
      { 0x0FF01, { 0x00021 } },
      { 0x0FF02, { 0x00027, 0x00027 } },
      { 0x0FF07, { 0x00027 } },
      { 0x0FF0D, { 0x030FC } },
      { 0x0FF1A, { 0x0003A } },
      { 0x0FF21, { 0x00041 } },
      { 0x0FF22, { 0x00042 } },
      { 0x0FF23, { 0x00043 } },
      { 0x0FF25, { 0x00045 } },
      { 0x0FF28, { 0x00048 } },
      { 0x0FF29, { 0x0006C } },
      { 0x0FF2A, { 0x0004A } },
      { 0x0FF2B, { 0x0004B } },
      { 0x0FF2D, { 0x0004D } },
      { 0x0FF2E, { 0x0004E } },
      { 0x0FF2F, { 0x0004F } },
      { 0x0FF30, { 0x00050 } },
      { 0x0FF33, { 0x00053 } },
      { 0x0FF34, { 0x00054 } },
      { 0x0FF38, { 0x00058 } },
      { 0x0FF39, { 0x00059 } },
      { 0x0FF3A, { 0x0005A } },
      { 0x0FF3B, { 0x00028 } },
      { 0x0FF3C, { 0x0005C } },
      { 0x0FF3D, { 0x00029 } },
      { 0x0FF3E, { 0x0FE3F } },
      { 0x0FF40, { 0x00027 } },
      { 0x0FF41, { 0x00061 } },
      { 0x0FF43, { 0x00063 } },
      { 0x0FF45, { 0x00065 } },
      { 0x0FF47, { 0x00067 } },
      { 0x0FF48, { 0x00068 } },
      { 0x0FF49, { 0x00069 } },
      { 0x0FF4A, { 0x0006A } },
      { 0x0FF4C, { 0x0006C } },
      { 0x0FF4F, { 0x0006F } },
      { 0x0FF50, { 0x00070 } },
      { 0x0FF53, { 0x00073 } },
      { 0x0FF56, { 0x00076 } },
      { 0x0FF58, { 0x00078 } },
      { 0x0FF59, { 0x00079 } },
      { 0x0FF5C, { 0x02502 } },
      { 0x0FF5E, { 0x0301C } },
      { 0x0FF65, { 0x000B7 } },
      { 0x0FFE3, { 0x002C9 } },
      { 0x0FFE8, { 0x0006C } },
      { 0x0FFED, { 0x025AA } },
      { 0x10101, { 0x000B7 } },
      { 0x1018E, { 0x0004E, 0x0030A } },
      { 0x10196, { 0x00058, 0x00335 } },
      { 0x10197, { 0x00056, 0x00335 } },
      { 0x10198, { 0x0006C, 0x00335, 0x0006C, 0x00335, 0x00053,
                   0x00335 } },
      { 0x10199, { 0x0006C, 0x00335, 0x0006C, 0x00335 } },
      { 0x101A0, { 0x00554 } },
      { 0x10282, { 0x00042 } },
      { 0x10285, { 0x00394 } },
      { 0x10286, { 0x00045 } },
      { 0x10287, { 0x00046 } },
      { 0x1028A, { 0x0006C } },
      { 0x1028D, { 0x00245 } },
      { 0x10290, { 0x00058 } },
      { 0x10292, { 0x0004F } },
      { 0x10294, { 0x016DC } },
      { 0x10295, { 0x00050 } },
      { 0x10296, { 0x00053 } },
      { 0x10297, { 0x00054 } },
      { 0x1029B, { 0x0002B } },
      { 0x102A0, { 0x00041 } },
      { 0x102A1, { 0x00042 } },
      { 0x102A2, { 0x00043 } },
      { 0x102A3, { 0x00394 } },
      { 0x102A5, { 0x00046 } },
      { 0x102AB, { 0x0004F } },
      { 0x102AD, { 0x003D8 } },
      { 0x102B0, { 0x0004D } },
      { 0x102B1, { 0x00054 } },
      { 0x102B2, { 0x00059 } },
      { 0x102B3, { 0x003A6 } },
      { 0x102B4, { 0x00058 } },
      { 0x102B5, { 0x003A8 } },
      { 0x102B6, { 0x003A9 } },
      { 0x102B8, { 0x02D40 } },
      { 0x102CF, { 0x00048 } },
      { 0x102E1, { 0x0062F } },
      { 0x102E4, { 0x00648 } },
      { 0x102E8, { 0x00637 } },
      { 0x102F2, { 0x00635 } },
      { 0x102F5, { 0x0005A } },
      { 0x10301, { 0x00042 } },
      { 0x10302, { 0x00043 } },
      { 0x10309, { 0x0006C } },
      { 0x10311, { 0x0004D } },
      { 0x10312, { 0x003D8 } },
      { 0x10315, { 0x00054 } },
      { 0x10317, { 0x00058 } },
      { 0x1031A, { 0x00038 } },
      { 0x1031F, { 0x0002A } },
      { 0x10320, { 0x0006C } },
      { 0x10322, { 0x00058 } },
      { 0x103D1, { 0x10382 } },
      { 0x103D3, { 0x10393 } },
      { 0x10401, { 0x00190 } },
      { 0x10404, { 0x0004F } },
      { 0x10411, { 0x0A4F6 } },
      { 0x10415, { 0x00043 } },
      { 0x1041B, { 0x0004C } },
      { 0x1041F, { 0x02C70 } },
      { 0x10420, { 0x00053 } },
      { 0x10423, { 0x00186 } },
      { 0x10425, { 0x00418 } },
      { 0x10429, { 0x0A793 } },
      { 0x1042A, { 0x0029A } },
      { 0x1042C, { 0x0006F } },
      { 0x1043D, { 0x00063 } },
      { 0x1043F, { 0x00277 } },
      { 0x10442, { 0x0025E } },
      { 0x10443, { 0x0029F } },
      { 0x10448, { 0x00073 } },
      { 0x1044B, { 0x00254 } },
      { 0x1044D, { 0x01D0E } },
      { 0x104A0, { 0x10486 } },
      { 0x104B0, { 0x00245 } },
      { 0x104B4, { 0x00052 } },
      { 0x104BC, { 0x004C3 } },
      { 0x104C2, { 0x0004F } },
      { 0x104C3, { 0x00298 } },
      { 0x104C4, { 0x000DE } },
      { 0x104CD, { 0x0040B } },
      { 0x104CE, { 0x00055 } },
      { 0x104D0, { 0x016E6 } },
      { 0x104D1, { 0x003A8 } },
      { 0x104D2, { 0x00037 } },
      { 0x104D8, { 0x0028C } },
      { 0x104DB, { 0x003BB } },
      { 0x104EA, { 0x0006F } },
      { 0x104EB, { 0x0A669 } },
      { 0x104F6, { 0x00075 } },
      { 0x104F9, { 0x003C8 } },
      { 0x10513, { 0x0004E } },
      { 0x10516, { 0x0004F } },
      { 0x10518, { 0x0004B } },
      { 0x1051C, { 0x00043 } },
      { 0x1051D, { 0x00056 } },
      { 0x10525, { 0x00046 } },
      { 0x10526, { 0x0004C } },
      { 0x10527, { 0x00058 } },
      { 0x10A3A, { 0x00323 } },
      { 0x10A50, { 0x0002E } },
      { 0x10A57, { 0x10A56, 0x10A56 } },
      { 0x10CFA, { 0x10CA5 } },
      { 0x10CFC, { 0x10C82 } },
      { 0x10EC6, { 0x00646 } },
      { 0x10EC7, { 0x00680 } },
      { 0x10ED0, { 0x000B0, 0x00332 } },
      { 0x110BB, { 0x00970 } },
      { 0x111C7, { 0x00970 } },
      { 0x111CA, { 0x00323 } },
      { 0x111CB, { 0x0093A } },
      { 0x111DB, { 0x0A8FC } },
      { 0x111DC, { 0x0A8FB } },
      { 0x111DE, { 0x02248 } },
      { 0x11300, { 0x0030A } },
      { 0x11413, { 0x11434, 0x11442, 0x11412 } },
      { 0x11419, { 0x11434, 0x11442, 0x11418 } },
      { 0x11424, { 0x11434, 0x11442, 0x11423 } },
      { 0x1142A, { 0x11434, 0x11442, 0x11429 } },
      { 0x1142D, { 0x11434, 0x11442, 0x1142C } },
      { 0x1142F, { 0x11434, 0x11442, 0x1142E } },
      { 0x1144C, { 0x1144B, 0x1144B } },
      { 0x11492, { 0x00998 } },
      { 0x11494, { 0x0099A } },
      { 0x11496, { 0x0099C } },
      { 0x11498, { 0x0099E } },
      { 0x11499, { 0x0099F } },
      { 0x1149B, { 0x009A1 } },
      { 0x1149D, { 0x009B2 } },
      { 0x1149E, { 0x009A4 } },
      { 0x1149F, { 0x009A5 } },
      { 0x114A0, { 0x009A6 } },
      { 0x114A1, { 0x009A7 } },
      { 0x114A2, { 0x009A8 } },
      { 0x114A3, { 0x009AA } },
      { 0x114A7, { 0x009AE } },
      { 0x114A8, { 0x009AF } },
      { 0x114A9, { 0x009AC } },
      { 0x114AA, { 0x009A3 } },
      { 0x114AB, { 0x009B0 } },
      { 0x114AD, { 0x009B7 } },
      { 0x114AE, { 0x009B8 } },
      { 0x114B0, { 0x009BE } },
      { 0x114B1, { 0x009BF } },
      { 0x114B9, { 0x009C7 } },
      { 0x114BC, { 0x009CB } },
      { 0x114BD, { 0x009D7 } },
      { 0x114BE, { 0x009CC } },
      { 0x114BF, { 0x00306, 0x00307 } },
      { 0x114C1, { 0x00983 } },
      { 0x114C2, { 0x009CD } },
      { 0x114C3, { 0x00323 } },
      { 0x114C4, { 0x009BD } },
      { 0x114C5, { 0x00077, 0x00307 } },
      { 0x114D0, { 0x0006F } },
      { 0x114D1, { 0x009E7 } },
      { 0x114D2, { 0x009E8 } },
      { 0x114D6, { 0x009EC } },
      { 0x115D8, { 0x11582 } },
      { 0x115D9, { 0x11582 } },
      { 0x115DA, { 0x11583 } },
      { 0x115DB, { 0x11584 } },
      { 0x115DC, { 0x115B2 } },
      { 0x115DD, { 0x115B3 } },
      { 0x11642, { 0x11641, 0x11641 } },
      { 0x11700, { 0x00072, 0x0006E } },
      { 0x11706, { 0x00076 } },
      { 0x1170A, { 0x00077 } },
      { 0x1170E, { 0x00077 } },
      { 0x1170F, { 0x00077 } },
      { 0x118A0, { 0x00056 } },
      { 0x118A2, { 0x00046 } },
      { 0x118A3, { 0x0004C } },
      { 0x118A4, { 0x00059 } },
      { 0x118A6, { 0x00045 } },
      { 0x118A8, { 0x02207 } },
      { 0x118A9, { 0x0005A } },
      { 0x118AC, { 0x00039 } },
      { 0x118AE, { 0x00045 } },
      { 0x118AF, { 0x00034 } },
      { 0x118B2, { 0x0004C } },
      { 0x118B5, { 0x0004F } },
      { 0x118B7, { 0x016DC } },
      { 0x118B8, { 0x00055 } },
      { 0x118BB, { 0x00035 } },
      { 0x118BC, { 0x00054 } },
      { 0x118C0, { 0x00076 } },
      { 0x118C1, { 0x00073 } },
      { 0x118C2, { 0x00046 } },
      { 0x118C3, { 0x00069 } },
      { 0x118C4, { 0x0007A } },
      { 0x118C6, { 0x00037 } },
      { 0x118C8, { 0x0006F } },
      { 0x118CA, { 0x00033 } },
      { 0x118CC, { 0x00039 } },
      { 0x118CE, { 0x0A793 } },
      { 0x118D5, { 0x00036 } },
      { 0x118D6, { 0x00039 } },
      { 0x118D7, { 0x0006F } },
      { 0x118D8, { 0x00075 } },
      { 0x118DC, { 0x00079 } },
      { 0x118E0, { 0x0004F } },
      { 0x118E3, { 0x00072, 0x0006E } },
      { 0x118E4, { 0x00669 } },
      { 0x118E5, { 0x0005A } },
      { 0x118E6, { 0x00057 } },
      { 0x118E9, { 0x00043 } },
      { 0x118EC, { 0x00058 } },
      { 0x118EF, { 0x00057 } },
      { 0x118F2, { 0x00043 } },
      { 0x11AE6, { 0x11AE5, 0x11AEF } },
      { 0x11AE7, { 0x11AE5, 0x11AF0 } },
      { 0x11AE8, { 0x11AE5, 0x11AE5 } },
      { 0x11AE9, { 0x11AE5, 0x11AE5, 0x11AEF } },
      { 0x11AEA, { 0x11AE5, 0x11AE5, 0x11AF0 } },
      { 0x11AEC, { 0x11AEB, 0x11AEF } },
      { 0x11AED, { 0x11AEB, 0x11AEB } },
      { 0x11AEE, { 0x11AEB, 0x11AEB, 0x11AEF } },
      { 0x11AF4, { 0x11AF3, 0x11AEF } },
      { 0x11AF5, { 0x11AF3, 0x11AF0 } },
      { 0x11AF6, { 0x11AF3, 0x11AF3 } },
      { 0x11AF7, { 0x11AF3, 0x11AF3, 0x11AEF } },
      { 0x11AF8, { 0x11AF3, 0x11AF3, 0x11AF0 } },
      { 0x11B60, { 0x0093A } },
      { 0x11B66, { 0x00306 } },
      { 0x11C42, { 0x11C41, 0x11C41 } },
      { 0x11CB2, { 0x11CAA } },
      { 0x11DD9, { 0x0003A } },
      { 0x11DDA, { 0x0006C } },
      { 0x11DE0, { 0x0004F } },
      { 0x11DE1, { 0x0006C } },
      { 0x12038, { 0x1039A } },
      { 0x132F9, { 0x1099E } },
      { 0x16EA6, { 0x003A0 } },
      { 0x16EAA, { 0x0006C } },
      { 0x16EB6, { 0x00062 } },
      { 0x16EC1, { 0x003C0 } },
      { 0x16ED1, { 0x00185 } },
      { 0x16F07, { 0x00393 } },
      { 0x16F08, { 0x00056 } },
      { 0x16F0A, { 0x00054 } },
      { 0x16F16, { 0x0004C } },
      { 0x16F1A, { 0x00394 } },
      { 0x16F1C, { 0x0A658 } },
      { 0x16F26, { 0x0A4F6 } },
      { 0x16F28, { 0x0006C } },
      { 0x16F2D, { 0x00190 } },
      { 0x16F35, { 0x00052 } },
      { 0x16F3A, { 0x00053 } },
      { 0x16F3B, { 0x00033 } },
      { 0x16F3D, { 0x00245 } },
      { 0x16F3F, { 0x0003E } },
      { 0x16F40, { 0x00041 } },
      { 0x16F42, { 0x00055 } },
      { 0x16F43, { 0x00059 } },
      { 0x16F51, { 0x00027 } },
      { 0x16F52, { 0x00027 } },
      { 0x16FF2, { 0x0513F } },
      { 0x1CCD6, { 0x00041 } },
      { 0x1CCD7, { 0x00042 } },
      { 0x1CCD8, { 0x00043 } },
      { 0x1CCD9, { 0x00044 } },
      { 0x1CCDA, { 0x00045 } },
      { 0x1CCDB, { 0x00046 } },
      { 0x1CCDC, { 0x00047 } },
      { 0x1CCDD, { 0x00048 } },
      { 0x1CCDE, { 0x0006C } },
      { 0x1CCDF, { 0x0004A } },
      { 0x1CCE0, { 0x0004B } },
      { 0x1CCE1, { 0x0004C } },
      { 0x1CCE2, { 0x0004D } },
      { 0x1CCE3, { 0x0004E } },
      { 0x1CCE4, { 0x0004F } },
      { 0x1CCE5, { 0x00050 } },
      { 0x1CCE6, { 0x00051 } },
      { 0x1CCE7, { 0x00052 } },
      { 0x1CCE8, { 0x00053 } },
      { 0x1CCE9, { 0x00054 } },
      { 0x1CCEA, { 0x00055 } },
      { 0x1CCEB, { 0x00056 } },
      { 0x1CCEC, { 0x00057 } },
      { 0x1CCED, { 0x00058 } },
      { 0x1CCEE, { 0x00059 } },
      { 0x1CCEF, { 0x0005A } },
      { 0x1CCF0, { 0x0004F } },
      { 0x1CCF1, { 0x0006C } },
      { 0x1CCF2, { 0x00032 } },
      { 0x1CCF3, { 0x00033 } },
      { 0x1CCF4, { 0x00034 } },
      { 0x1CCF5, { 0x00035 } },
      { 0x1CCF6, { 0x00036 } },
      { 0x1CCF7, { 0x00037 } },
      { 0x1CCF8, { 0x00038 } },
      { 0x1CCF9, { 0x00039 } },
      { 0x1CCFB, { 0x1F6F8 } },
      { 0x1CEEF, { 0x02D42 } },
      { 0x1D114, { 0x0007B } },
      { 0x1D16D, { 0x0002E } },
      { 0x1D202, { 0x004FE } },
      { 0x1D206, { 0x00033 } },
      { 0x1D20B, { 0x00418 } },
      { 0x1D20D, { 0x00056 } },
      { 0x1D20F, { 0x0005C } },
      { 0x1D212, { 0x00037 } },
      { 0x1D213, { 0x00046 } },
      { 0x1D214, { 0x102BC } },
      { 0x1D215, { 0x0A4F6 } },
      { 0x1D216, { 0x00052 } },
      { 0x1D217, { 0x02C6F } },
      { 0x1D21A, { 0x0004F, 0x00335 } },
      { 0x1D21B, { 0x02144 } },
      { 0x1D21C, { 0x0A4D5 } },
      { 0x1D221, { 0x00190 } },
      { 0x1D222, { 0x00460 } },
      { 0x1D22A, { 0x0004C } },
      { 0x1D22B, { 0x0A4F6 } },
      { 0x1D230, { 0x0A7FB } },
      { 0x1D236, { 0x0003C } },
      { 0x1D237, { 0x0003E } },
      { 0x1D238, { 0x0228F } },
      { 0x1D239, { 0x02290 } },
      { 0x1D23A, { 0x0002F } },
      { 0x1D23B, { 0x0005C } },
      { 0x1D23F, { 0x016CB } },
      { 0x1D245, { 0x00548 } },
      { 0x1D400, { 0x00041 } },
      { 0x1D401, { 0x00042 } },
      { 0x1D402, { 0x00043 } },
      { 0x1D403, { 0x00044 } },
      { 0x1D404, { 0x00045 } },
      { 0x1D405, { 0x00046 } },
      { 0x1D406, { 0x00047 } },
      { 0x1D407, { 0x00048 } },
      { 0x1D408, { 0x0006C } },
      { 0x1D409, { 0x0004A } },
      { 0x1D40A, { 0x0004B } },
      { 0x1D40B, { 0x0004C } },
      { 0x1D40C, { 0x0004D } },
      { 0x1D40D, { 0x0004E } },
      { 0x1D40E, { 0x0004F } },
      { 0x1D40F, { 0x00050 } },
      { 0x1D410, { 0x00051 } },
      { 0x1D411, { 0x00052 } },
      { 0x1D412, { 0x00053 } },
      { 0x1D413, { 0x00054 } },
      { 0x1D414, { 0x00055 } },
      { 0x1D415, { 0x00056 } },
      { 0x1D416, { 0x00057 } },
      { 0x1D417, { 0x00058 } },
      { 0x1D418, { 0x00059 } },
      { 0x1D419, { 0x0005A } },
      { 0x1D41A, { 0x00061 } },
      { 0x1D41B, { 0x00062 } },
      { 0x1D41C, { 0x00063 } },
      { 0x1D41D, { 0x00064 } },
      { 0x1D41E, { 0x00065 } },
      { 0x1D41F, { 0x00066 } },
      { 0x1D420, { 0x00067 } },
      { 0x1D421, { 0x00068 } },
      { 0x1D422, { 0x00069 } },
      { 0x1D423, { 0x0006A } },
      { 0x1D424, { 0x0006B } },
      { 0x1D425, { 0x0006C } },
      { 0x1D426, { 0x00072, 0x0006E } },
      { 0x1D427, { 0x0006E } },
      { 0x1D428, { 0x0006F } },
      { 0x1D429, { 0x00070 } },
      { 0x1D42A, { 0x00071 } },
      { 0x1D42B, { 0x00072 } },
      { 0x1D42C, { 0x00073 } },
      { 0x1D42D, { 0x00074 } },
      { 0x1D42E, { 0x00075 } },
      { 0x1D42F, { 0x00076 } },
      { 0x1D430, { 0x00077 } },
      { 0x1D431, { 0x00078 } },
      { 0x1D432, { 0x00079 } },
      { 0x1D433, { 0x0007A } },
      { 0x1D434, { 0x00041 } },
      { 0x1D435, { 0x00042 } },
      { 0x1D436, { 0x00043 } },
      { 0x1D437, { 0x00044 } },
      { 0x1D438, { 0x00045 } },
      { 0x1D439, { 0x00046 } },
      { 0x1D43A, { 0x00047 } },
      { 0x1D43B, { 0x00048 } },
      { 0x1D43C, { 0x0006C } },
      { 0x1D43D, { 0x0004A } },
      { 0x1D43E, { 0x0004B } },
      { 0x1D43F, { 0x0004C } },
      { 0x1D440, { 0x0004D } },
      { 0x1D441, { 0x0004E } },
      { 0x1D442, { 0x0004F } },
      { 0x1D443, { 0x00050 } },
      { 0x1D444, { 0x00051 } },
      { 0x1D445, { 0x00052 } },
      { 0x1D446, { 0x00053 } },
      { 0x1D447, { 0x00054 } },
      { 0x1D448, { 0x00055 } },
      { 0x1D449, { 0x00056 } },
      { 0x1D44A, { 0x00057 } },
      { 0x1D44B, { 0x00058 } },
      { 0x1D44C, { 0x00059 } },
      { 0x1D44D, { 0x0005A } },
      { 0x1D44E, { 0x00061 } },
      { 0x1D44F, { 0x00062 } },
      { 0x1D450, { 0x00063 } },
      { 0x1D451, { 0x00064 } },
      { 0x1D452, { 0x00065 } },
      { 0x1D453, { 0x00066 } },
      { 0x1D454, { 0x00067 } },
      { 0x1D456, { 0x00069 } },
      { 0x1D457, { 0x0006A } },
      { 0x1D458, { 0x0006B } },
      { 0x1D459, { 0x0006C } },
      { 0x1D45A, { 0x00072, 0x0006E } },
      { 0x1D45B, { 0x0006E } },
      { 0x1D45C, { 0x0006F } },
      { 0x1D45D, { 0x00070 } },
      { 0x1D45E, { 0x00071 } },
      { 0x1D45F, { 0x00072 } },
      { 0x1D460, { 0x00073 } },
      { 0x1D461, { 0x00074 } },
      { 0x1D462, { 0x00075 } },
      { 0x1D463, { 0x00076 } },
      { 0x1D464, { 0x00077 } },
      { 0x1D465, { 0x00078 } },
      { 0x1D466, { 0x00079 } },
      { 0x1D467, { 0x0007A } },
      { 0x1D468, { 0x00041 } },
      { 0x1D469, { 0x00042 } },
      { 0x1D46A, { 0x00043 } },
      { 0x1D46B, { 0x00044 } },
      { 0x1D46C, { 0x00045 } },
      { 0x1D46D, { 0x00046 } },
      { 0x1D46E, { 0x00047 } },
      { 0x1D46F, { 0x00048 } },
      { 0x1D470, { 0x0006C } },
      { 0x1D471, { 0x0004A } },
      { 0x1D472, { 0x0004B } },
      { 0x1D473, { 0x0004C } },
      { 0x1D474, { 0x0004D } },
      { 0x1D475, { 0x0004E } },
      { 0x1D476, { 0x0004F } },
      { 0x1D477, { 0x00050 } },
      { 0x1D478, { 0x00051 } },
      { 0x1D479, { 0x00052 } },
      { 0x1D47A, { 0x00053 } },
      { 0x1D47B, { 0x00054 } },
      { 0x1D47C, { 0x00055 } },
      { 0x1D47D, { 0x00056 } },
      { 0x1D47E, { 0x00057 } },
      { 0x1D47F, { 0x00058 } },
      { 0x1D480, { 0x00059 } },
      { 0x1D481, { 0x0005A } },
      { 0x1D482, { 0x00061 } },
      { 0x1D483, { 0x00062 } },
      { 0x1D484, { 0x00063 } },
      { 0x1D485, { 0x00064 } },
      { 0x1D486, { 0x00065 } },
      { 0x1D487, { 0x00066 } },
      { 0x1D488, { 0x00067 } },
      { 0x1D489, { 0x00068 } },
      { 0x1D48A, { 0x00069 } },
      { 0x1D48B, { 0x0006A } },
      { 0x1D48C, { 0x0006B } },
      { 0x1D48D, { 0x0006C } },
      { 0x1D48E, { 0x00072, 0x0006E } },
      { 0x1D48F, { 0x0006E } },
      { 0x1D490, { 0x0006F } },
      { 0x1D491, { 0x00070 } },
      { 0x1D492, { 0x00071 } },
      { 0x1D493, { 0x00072 } },
      { 0x1D494, { 0x00073 } },
      { 0x1D495, { 0x00074 } },
      { 0x1D496, { 0x00075 } },
      { 0x1D497, { 0x00076 } },
      { 0x1D498, { 0x00077 } },
      { 0x1D499, { 0x00078 } },
      { 0x1D49A, { 0x00079 } },
      { 0x1D49B, { 0x0007A } },
      { 0x1D49C, { 0x00041 } },
      { 0x1D49E, { 0x00043 } },
      { 0x1D49F, { 0x00044 } },
      { 0x1D4A2, { 0x00047 } },
      { 0x1D4A5, { 0x0004A } },
      { 0x1D4A6, { 0x0004B } },
      { 0x1D4A9, { 0x0004E } },
      { 0x1D4AA, { 0x0004F } },
      { 0x1D4AB, { 0x00050 } },
      { 0x1D4AC, { 0x00051 } },
      { 0x1D4AE, { 0x00053 } },
      { 0x1D4AF, { 0x00054 } },
      { 0x1D4B0, { 0x00055 } },
      { 0x1D4B1, { 0x00056 } },
      { 0x1D4B2, { 0x00057 } },
      { 0x1D4B3, { 0x00058 } },
      { 0x1D4B4, { 0x00059 } },
      { 0x1D4B5, { 0x0005A } },
      { 0x1D4B6, { 0x00061 } },
      { 0x1D4B7, { 0x00062 } },
      { 0x1D4B8, { 0x00063 } },
      { 0x1D4B9, { 0x00064 } },
      { 0x1D4BB, { 0x00066 } },
      { 0x1D4BD, { 0x00068 } },
      { 0x1D4BE, { 0x00069 } },
      { 0x1D4BF, { 0x0006A } },
      { 0x1D4C0, { 0x0006B } },
      { 0x1D4C1, { 0x0006C } },
      { 0x1D4C2, { 0x00072, 0x0006E } },
      { 0x1D4C3, { 0x0006E } },
      { 0x1D4C5, { 0x00070 } },
      { 0x1D4C6, { 0x00071 } },
      { 0x1D4C7, { 0x00072 } },
      { 0x1D4C8, { 0x00073 } },
      { 0x1D4C9, { 0x00074 } },
      { 0x1D4CA, { 0x00075 } },
      { 0x1D4CB, { 0x00076 } },
      { 0x1D4CC, { 0x00077 } },
      { 0x1D4CD, { 0x00078 } },
      { 0x1D4CE, { 0x00079 } },
      { 0x1D4CF, { 0x0007A } },
      { 0x1D4D0, { 0x00041 } },
      { 0x1D4D1, { 0x00042 } },
      { 0x1D4D2, { 0x00043 } },
      { 0x1D4D3, { 0x00044 } },
      { 0x1D4D4, { 0x00045 } },
      { 0x1D4D5, { 0x00046 } },
      { 0x1D4D6, { 0x00047 } },
      { 0x1D4D7, { 0x00048 } },
      { 0x1D4D8, { 0x0006C } },
      { 0x1D4D9, { 0x0004A } },
      { 0x1D4DA, { 0x0004B } },
      { 0x1D4DB, { 0x0004C } },
      { 0x1D4DC, { 0x0004D } },
      { 0x1D4DD, { 0x0004E } },
      { 0x1D4DE, { 0x0004F } },
      { 0x1D4DF, { 0x00050 } },
      { 0x1D4E0, { 0x00051 } },
      { 0x1D4E1, { 0x00052 } },
      { 0x1D4E2, { 0x00053 } },
      { 0x1D4E3, { 0x00054 } },
      { 0x1D4E4, { 0x00055 } },
      { 0x1D4E5, { 0x00056 } },
      { 0x1D4E6, { 0x00057 } },
      { 0x1D4E7, { 0x00058 } },
      { 0x1D4E8, { 0x00059 } },
      { 0x1D4E9, { 0x0005A } },
      { 0x1D4EA, { 0x00061 } },
      { 0x1D4EB, { 0x00062 } },
      { 0x1D4EC, { 0x00063 } },
      { 0x1D4ED, { 0x00064 } },
      { 0x1D4EE, { 0x00065 } },
      { 0x1D4EF, { 0x00066 } },
      { 0x1D4F0, { 0x00067 } },
      { 0x1D4F1, { 0x00068 } },
      { 0x1D4F2, { 0x00069 } },
      { 0x1D4F3, { 0x0006A } },
      { 0x1D4F4, { 0x0006B } },
      { 0x1D4F5, { 0x0006C } },
      { 0x1D4F6, { 0x00072, 0x0006E } },
      { 0x1D4F7, { 0x0006E } },
      { 0x1D4F8, { 0x0006F } },
      { 0x1D4F9, { 0x00070 } },
      { 0x1D4FA, { 0x00071 } },
      { 0x1D4FB, { 0x00072 } },
      { 0x1D4FC, { 0x00073 } },
      { 0x1D4FD, { 0x00074 } },
      { 0x1D4FE, { 0x00075 } },
      { 0x1D4FF, { 0x00076 } },
      { 0x1D500, { 0x00077 } },
      { 0x1D501, { 0x00078 } },
      { 0x1D502, { 0x00079 } },
      { 0x1D503, { 0x0007A } },
      { 0x1D504, { 0x00041 } },
      { 0x1D505, { 0x00042 } },
      { 0x1D507, { 0x00044 } },
      { 0x1D508, { 0x00045 } },
      { 0x1D509, { 0x00046 } },
      { 0x1D50A, { 0x00047 } },
      { 0x1D50D, { 0x0004A } },
      { 0x1D50E, { 0x0004B } },
      { 0x1D50F, { 0x0004C } },
      { 0x1D510, { 0x0004D } },
      { 0x1D511, { 0x0004E } },
      { 0x1D512, { 0x0004F } },
      { 0x1D513, { 0x00050 } },
      { 0x1D514, { 0x00051 } },
      { 0x1D516, { 0x00053 } },
      { 0x1D517, { 0x00054 } },
      { 0x1D518, { 0x00055 } },
      { 0x1D519, { 0x00056 } },
      { 0x1D51A, { 0x00057 } },
      { 0x1D51B, { 0x00058 } },
      { 0x1D51C, { 0x00059 } },
      { 0x1D51E, { 0x00061 } },
      { 0x1D51F, { 0x00062 } },
      { 0x1D520, { 0x00063 } },
      { 0x1D521, { 0x00064 } },
      { 0x1D522, { 0x00065 } },
      { 0x1D523, { 0x00066 } },
      { 0x1D524, { 0x00067 } },
      { 0x1D525, { 0x00068 } },
      { 0x1D526, { 0x00069 } },
      { 0x1D527, { 0x0006A } },
      { 0x1D528, { 0x0006B } },
      { 0x1D529, { 0x0006C } },
      { 0x1D52A, { 0x00072, 0x0006E } },
      { 0x1D52B, { 0x0006E } },
      { 0x1D52C, { 0x0006F } },
      { 0x1D52D, { 0x00070 } },
      { 0x1D52E, { 0x00071 } },
      { 0x1D52F, { 0x00072 } },
      { 0x1D530, { 0x00073 } },
      { 0x1D531, { 0x00074 } },
      { 0x1D532, { 0x00075 } },
      { 0x1D533, { 0x00076 } },
      { 0x1D534, { 0x00077 } },
      { 0x1D535, { 0x00078 } },
      { 0x1D536, { 0x00079 } },
      { 0x1D537, { 0x0007A } },
      { 0x1D538, { 0x00041 } },
      { 0x1D539, { 0x00042 } },
      { 0x1D53B, { 0x00044 } },
      { 0x1D53C, { 0x00045 } },
      { 0x1D53D, { 0x00046 } },
      { 0x1D53E, { 0x00047 } },
      { 0x1D540, { 0x0006C } },
      { 0x1D541, { 0x0004A } },
      { 0x1D542, { 0x0004B } },
      { 0x1D543, { 0x0004C } },
      { 0x1D544, { 0x0004D } },
      { 0x1D546, { 0x0004F } },
      { 0x1D54A, { 0x00053 } },
      { 0x1D54B, { 0x00054 } },
      { 0x1D54C, { 0x00055 } },
      { 0x1D54D, { 0x00056 } },
      { 0x1D54E, { 0x00057 } },
      { 0x1D54F, { 0x00058 } },
      { 0x1D550, { 0x00059 } },
      { 0x1D552, { 0x00061 } },
      { 0x1D553, { 0x00062 } },
      { 0x1D554, { 0x00063 } },
      { 0x1D555, { 0x00064 } },
      { 0x1D556, { 0x00065 } },
      { 0x1D557, { 0x00066 } },
      { 0x1D558, { 0x00067 } },
      { 0x1D559, { 0x00068 } },
      { 0x1D55A, { 0x00069 } },
      { 0x1D55B, { 0x0006A } },
      { 0x1D55C, { 0x0006B } },
      { 0x1D55D, { 0x0006C } },
      { 0x1D55E, { 0x00072, 0x0006E } },
      { 0x1D55F, { 0x0006E } },
      { 0x1D560, { 0x0006F } },
      { 0x1D561, { 0x00070 } },
      { 0x1D562, { 0x00071 } },
      { 0x1D563, { 0x00072 } },
      { 0x1D564, { 0x00073 } },
      { 0x1D565, { 0x00074 } },
      { 0x1D566, { 0x00075 } },
      { 0x1D567, { 0x00076 } },
      { 0x1D568, { 0x00077 } },
      { 0x1D569, { 0x00078 } },
      { 0x1D56A, { 0x00079 } },
      { 0x1D56B, { 0x0007A } },
      { 0x1D56C, { 0x00041 } },
      { 0x1D56D, { 0x00042 } },
      { 0x1D56E, { 0x00043 } },
      { 0x1D56F, { 0x00044 } },
      { 0x1D570, { 0x00045 } },
      { 0x1D571, { 0x00046 } },
      { 0x1D572, { 0x00047 } },
      { 0x1D573, { 0x00048 } },
      { 0x1D574, { 0x0006C } },
      { 0x1D575, { 0x0004A } },
      { 0x1D576, { 0x0004B } },
      { 0x1D577, { 0x0004C } },
      { 0x1D578, { 0x0004D } },
      { 0x1D579, { 0x0004E } },
      { 0x1D57A, { 0x0004F } },
      { 0x1D57B, { 0x00050 } },
      { 0x1D57C, { 0x00051 } },
      { 0x1D57D, { 0x00052 } },
      { 0x1D57E, { 0x00053 } },
      { 0x1D57F, { 0x00054 } },
      { 0x1D580, { 0x00055 } },
      { 0x1D581, { 0x00056 } },
      { 0x1D582, { 0x00057 } },
      { 0x1D583, { 0x00058 } },
      { 0x1D584, { 0x00059 } },
      { 0x1D585, { 0x0005A } },
      { 0x1D586, { 0x00061 } },
      { 0x1D587, { 0x00062 } },
      { 0x1D588, { 0x00063 } },
      { 0x1D589, { 0x00064 } },
      { 0x1D58A, { 0x00065 } },
      { 0x1D58B, { 0x00066 } },
      { 0x1D58C, { 0x00067 } },
      { 0x1D58D, { 0x00068 } },
      { 0x1D58E, { 0x00069 } },
      { 0x1D58F, { 0x0006A } },
      { 0x1D590, { 0x0006B } },
      { 0x1D591, { 0x0006C } },
      { 0x1D592, { 0x00072, 0x0006E } },
      { 0x1D593, { 0x0006E } },
      { 0x1D594, { 0x0006F } },
      { 0x1D595, { 0x00070 } },
      { 0x1D596, { 0x00071 } },
      { 0x1D597, { 0x00072 } },
      { 0x1D598, { 0x00073 } },
      { 0x1D599, { 0x00074 } },
      { 0x1D59A, { 0x00075 } },
      { 0x1D59B, { 0x00076 } },
      { 0x1D59C, { 0x00077 } },
      { 0x1D59D, { 0x00078 } },
      { 0x1D59E, { 0x00079 } },
      { 0x1D59F, { 0x0007A } },
      { 0x1D5A0, { 0x00041 } },
      { 0x1D5A1, { 0x00042 } },
      { 0x1D5A2, { 0x00043 } },
      { 0x1D5A3, { 0x00044 } },
      { 0x1D5A4, { 0x00045 } },
      { 0x1D5A5, { 0x00046 } },
      { 0x1D5A6, { 0x00047 } },
      { 0x1D5A7, { 0x00048 } },
      { 0x1D5A8, { 0x0006C } },
      { 0x1D5A9, { 0x0004A } },
      { 0x1D5AA, { 0x0004B } },
      { 0x1D5AB, { 0x0004C } },
      { 0x1D5AC, { 0x0004D } },
      { 0x1D5AD, { 0x0004E } },
      { 0x1D5AE, { 0x0004F } },
      { 0x1D5AF, { 0x00050 } },
      { 0x1D5B0, { 0x00051 } },
      { 0x1D5B1, { 0x00052 } },
      { 0x1D5B2, { 0x00053 } },
      { 0x1D5B3, { 0x00054 } },
      { 0x1D5B4, { 0x00055 } },
      { 0x1D5B5, { 0x00056 } },
      { 0x1D5B6, { 0x00057 } },
      { 0x1D5B7, { 0x00058 } },
      { 0x1D5B8, { 0x00059 } },
      { 0x1D5B9, { 0x0005A } },
      { 0x1D5BA, { 0x00061 } },
      { 0x1D5BB, { 0x00062 } },
      { 0x1D5BC, { 0x00063 } },
      { 0x1D5BD, { 0x00064 } },
      { 0x1D5BE, { 0x00065 } },
      { 0x1D5BF, { 0x00066 } },
      { 0x1D5C0, { 0x00067 } },
      { 0x1D5C1, { 0x00068 } },
      { 0x1D5C2, { 0x00069 } },
      { 0x1D5C3, { 0x0006A } },
      { 0x1D5C4, { 0x0006B } },
      { 0x1D5C5, { 0x0006C } },
      { 0x1D5C6, { 0x00072, 0x0006E } },
      { 0x1D5C7, { 0x0006E } },
      { 0x1D5C8, { 0x0006F } },
      { 0x1D5C9, { 0x00070 } },
      { 0x1D5CA, { 0x00071 } },
      { 0x1D5CB, { 0x00072 } },
      { 0x1D5CC, { 0x00073 } },
      { 0x1D5CD, { 0x00074 } },
      { 0x1D5CE, { 0x00075 } },
      { 0x1D5CF, { 0x00076 } },
      { 0x1D5D0, { 0x00077 } },
      { 0x1D5D1, { 0x00078 } },
      { 0x1D5D2, { 0x00079 } },
      { 0x1D5D3, { 0x0007A } },
      { 0x1D5D4, { 0x00041 } },
      { 0x1D5D5, { 0x00042 } },
      { 0x1D5D6, { 0x00043 } },
      { 0x1D5D7, { 0x00044 } },
      { 0x1D5D8, { 0x00045 } },
      { 0x1D5D9, { 0x00046 } },
      { 0x1D5DA, { 0x00047 } },
      { 0x1D5DB, { 0x00048 } },
      { 0x1D5DC, { 0x0006C } },
      { 0x1D5DD, { 0x0004A } },
      { 0x1D5DE, { 0x0004B } },
      { 0x1D5DF, { 0x0004C } },
      { 0x1D5E0, { 0x0004D } },
      { 0x1D5E1, { 0x0004E } },
      { 0x1D5E2, { 0x0004F } },
      { 0x1D5E3, { 0x00050 } },
      { 0x1D5E4, { 0x00051 } },
      { 0x1D5E5, { 0x00052 } },
      { 0x1D5E6, { 0x00053 } },
      { 0x1D5E7, { 0x00054 } },
      { 0x1D5E8, { 0x00055 } },
      { 0x1D5E9, { 0x00056 } },
      { 0x1D5EA, { 0x00057 } },
      { 0x1D5EB, { 0x00058 } },
      { 0x1D5EC, { 0x00059 } },
      { 0x1D5ED, { 0x0005A } },
      { 0x1D5EE, { 0x00061 } },
      { 0x1D5EF, { 0x00062 } },
      { 0x1D5F0, { 0x00063 } },
      { 0x1D5F1, { 0x00064 } },
      { 0x1D5F2, { 0x00065 } },
      { 0x1D5F3, { 0x00066 } },
      { 0x1D5F4, { 0x00067 } },
      { 0x1D5F5, { 0x00068 } },
      { 0x1D5F6, { 0x00069 } },
      { 0x1D5F7, { 0x0006A } },
      { 0x1D5F8, { 0x0006B } },
      { 0x1D5F9, { 0x0006C } },
      { 0x1D5FA, { 0x00072, 0x0006E } },
      { 0x1D5FB, { 0x0006E } },
      { 0x1D5FC, { 0x0006F } },
      { 0x1D5FD, { 0x00070 } },
      { 0x1D5FE, { 0x00071 } },
      { 0x1D5FF, { 0x00072 } },
      { 0x1D600, { 0x00073 } },
      { 0x1D601, { 0x00074 } },
      { 0x1D602, { 0x00075 } },
      { 0x1D603, { 0x00076 } },
      { 0x1D604, { 0x00077 } },
      { 0x1D605, { 0x00078 } },
      { 0x1D606, { 0x00079 } },
      { 0x1D607, { 0x0007A } },
      { 0x1D608, { 0x00041 } },
      { 0x1D609, { 0x00042 } },
      { 0x1D60A, { 0x00043 } },
      { 0x1D60B, { 0x00044 } },
      { 0x1D60C, { 0x00045 } },
      { 0x1D60D, { 0x00046 } },
      { 0x1D60E, { 0x00047 } },
      { 0x1D60F, { 0x00048 } },
      { 0x1D610, { 0x0006C } },
      { 0x1D611, { 0x0004A } },
      { 0x1D612, { 0x0004B } },
      { 0x1D613, { 0x0004C } },
      { 0x1D614, { 0x0004D } },
      { 0x1D615, { 0x0004E } },
      { 0x1D616, { 0x0004F } },
      { 0x1D617, { 0x00050 } },
      { 0x1D618, { 0x00051 } },
      { 0x1D619, { 0x00052 } },
      { 0x1D61A, { 0x00053 } },
      { 0x1D61B, { 0x00054 } },
      { 0x1D61C, { 0x00055 } },
      { 0x1D61D, { 0x00056 } },
      { 0x1D61E, { 0x00057 } },
      { 0x1D61F, { 0x00058 } },
      { 0x1D620, { 0x00059 } },
      { 0x1D621, { 0x0005A } },
      { 0x1D622, { 0x00061 } },
      { 0x1D623, { 0x00062 } },
      { 0x1D624, { 0x00063 } },
      { 0x1D625, { 0x00064 } },
      { 0x1D626, { 0x00065 } },
      { 0x1D627, { 0x00066 } },
      { 0x1D628, { 0x00067 } },
      { 0x1D629, { 0x00068 } },
      { 0x1D62A, { 0x00069 } },
      { 0x1D62B, { 0x0006A } },
      { 0x1D62C, { 0x0006B } },
      { 0x1D62D, { 0x0006C } },
      { 0x1D62E, { 0x00072, 0x0006E } },
      { 0x1D62F, { 0x0006E } },
      { 0x1D630, { 0x0006F } },
      { 0x1D631, { 0x00070 } },
      { 0x1D632, { 0x00071 } },
      { 0x1D633, { 0x00072 } },
      { 0x1D634, { 0x00073 } },
      { 0x1D635, { 0x00074 } },
      { 0x1D636, { 0x00075 } },
      { 0x1D637, { 0x00076 } },
      { 0x1D638, { 0x00077 } },
      { 0x1D639, { 0x00078 } },
      { 0x1D63A, { 0x00079 } },
      { 0x1D63B, { 0x0007A } },
      { 0x1D63C, { 0x00041 } },
      { 0x1D63D, { 0x00042 } },
      { 0x1D63E, { 0x00043 } },
      { 0x1D63F, { 0x00044 } },
      { 0x1D640, { 0x00045 } },
      { 0x1D641, { 0x00046 } },
      { 0x1D642, { 0x00047 } },
      { 0x1D643, { 0x00048 } },
      { 0x1D644, { 0x0006C } },
      { 0x1D645, { 0x0004A } },
      { 0x1D646, { 0x0004B } },
      { 0x1D647, { 0x0004C } },
      { 0x1D648, { 0x0004D } },
      { 0x1D649, { 0x0004E } },
      { 0x1D64A, { 0x0004F } },
      { 0x1D64B, { 0x00050 } },
      { 0x1D64C, { 0x00051 } },
      { 0x1D64D, { 0x00052 } },
      { 0x1D64E, { 0x00053 } },
      { 0x1D64F, { 0x00054 } },
      { 0x1D650, { 0x00055 } },
      { 0x1D651, { 0x00056 } },
      { 0x1D652, { 0x00057 } },
      { 0x1D653, { 0x00058 } },
      { 0x1D654, { 0x00059 } },
      { 0x1D655, { 0x0005A } },
      { 0x1D656, { 0x00061 } },
      { 0x1D657, { 0x00062 } },
      { 0x1D658, { 0x00063 } },
      { 0x1D659, { 0x00064 } },
      { 0x1D65A, { 0x00065 } },
      { 0x1D65B, { 0x00066 } },
      { 0x1D65C, { 0x00067 } },
      { 0x1D65D, { 0x00068 } },
      { 0x1D65E, { 0x00069 } },
      { 0x1D65F, { 0x0006A } },
      { 0x1D660, { 0x0006B } },
      { 0x1D661, { 0x0006C } },
      { 0x1D662, { 0x00072, 0x0006E } },
      { 0x1D663, { 0x0006E } },
      { 0x1D664, { 0x0006F } },
      { 0x1D665, { 0x00070 } },
      { 0x1D666, { 0x00071 } },
      { 0x1D667, { 0x00072 } },
      { 0x1D668, { 0x00073 } },
      { 0x1D669, { 0x00074 } },
      { 0x1D66A, { 0x00075 } },
      { 0x1D66B, { 0x00076 } },
      { 0x1D66C, { 0x00077 } },
      { 0x1D66D, { 0x00078 } },
      { 0x1D66E, { 0x00079 } },
      { 0x1D66F, { 0x0007A } },
      { 0x1D670, { 0x00041 } },
      { 0x1D671, { 0x00042 } },
      { 0x1D672, { 0x00043 } },
      { 0x1D673, { 0x00044 } },
      { 0x1D674, { 0x00045 } },
      { 0x1D675, { 0x00046 } },
      { 0x1D676, { 0x00047 } },
      { 0x1D677, { 0x00048 } },
      { 0x1D678, { 0x0006C } },
      { 0x1D679, { 0x0004A } },
      { 0x1D67A, { 0x0004B } },
      { 0x1D67B, { 0x0004C } },
      { 0x1D67C, { 0x0004D } },
      { 0x1D67D, { 0x0004E } },
      { 0x1D67E, { 0x0004F } },
      { 0x1D67F, { 0x00050 } },
      { 0x1D680, { 0x00051 } },
      { 0x1D681, { 0x00052 } },
      { 0x1D682, { 0x00053 } },
      { 0x1D683, { 0x00054 } },
      { 0x1D684, { 0x00055 } },
      { 0x1D685, { 0x00056 } },
      { 0x1D686, { 0x00057 } },
      { 0x1D687, { 0x00058 } },
      { 0x1D688, { 0x00059 } },
      { 0x1D689, { 0x0005A } },
      { 0x1D68A, { 0x00061 } },
      { 0x1D68B, { 0x00062 } },
      { 0x1D68C, { 0x00063 } },
      { 0x1D68D, { 0x00064 } },
      { 0x1D68E, { 0x00065 } },
      { 0x1D68F, { 0x00066 } },
      { 0x1D690, { 0x00067 } },
      { 0x1D691, { 0x00068 } },
      { 0x1D692, { 0x00069 } },
      { 0x1D693, { 0x0006A } },
      { 0x1D694, { 0x0006B } },
      { 0x1D695, { 0x0006C } },
      { 0x1D696, { 0x00072, 0x0006E } },
      { 0x1D697, { 0x0006E } },
      { 0x1D698, { 0x0006F } },
      { 0x1D699, { 0x00070 } },
      { 0x1D69A, { 0x00071 } },
      { 0x1D69B, { 0x00072 } },
      { 0x1D69C, { 0x00073 } },
      { 0x1D69D, { 0x00074 } },
      { 0x1D69E, { 0x00075 } },
      { 0x1D69F, { 0x00076 } },
      { 0x1D6A0, { 0x00077 } },
      { 0x1D6A1, { 0x00078 } },
      { 0x1D6A2, { 0x00079 } },
      { 0x1D6A3, { 0x0007A } },
      { 0x1D6A4, { 0x00069 } },
      { 0x1D6A5, { 0x00237 } },
      { 0x1D6A8, { 0x00041 } },
      { 0x1D6A9, { 0x00042 } },
      { 0x1D6AA, { 0x00393 } },
      { 0x1D6AB, { 0x00394 } },
      { 0x1D6AC, { 0x00045 } },
      { 0x1D6AD, { 0x0005A } },
      { 0x1D6AE, { 0x00048 } },
      { 0x1D6AF, { 0x0004F, 0x00335 } },
      { 0x1D6B0, { 0x0006C } },
      { 0x1D6B1, { 0x0004B } },
      { 0x1D6B2, { 0x00245 } },
      { 0x1D6B3, { 0x0004D } },
      { 0x1D6B4, { 0x0004E } },
      { 0x1D6B5, { 0x0039E } },
      { 0x1D6B6, { 0x0004F } },
      { 0x1D6B7, { 0x003A0 } },
      { 0x1D6B8, { 0x00050 } },
      { 0x1D6B9, { 0x0004F, 0x00335 } },
      { 0x1D6BA, { 0x001A9 } },
      { 0x1D6BB, { 0x00054 } },
      { 0x1D6BC, { 0x00059 } },
      { 0x1D6BD, { 0x003A6 } },
      { 0x1D6BE, { 0x00058 } },
      { 0x1D6BF, { 0x003A8 } },
      { 0x1D6C0, { 0x003A9 } },
      { 0x1D6C1, { 0x02207 } },
      { 0x1D6C2, { 0x00061 } },
      { 0x1D6C3, { 0x000DF } },
      { 0x1D6C4, { 0x00079 } },
      { 0x1D6C5, { 0x01E9F } },
      { 0x1D6C6, { 0x0A793 } },
      { 0x1D6C7, { 0x003B6 } },
      { 0x1D6C8, { 0x0006E, 0x00329 } },
      { 0x1D6C9, { 0x0004F, 0x00335 } },
      { 0x1D6CA, { 0x00069 } },
      { 0x1D6CB, { 0x00138 } },
      { 0x1D6CC, { 0x003BB } },
      { 0x1D6CD, { 0x003BC } },
      { 0x1D6CE, { 0x00076 } },
      { 0x1D6CF, { 0x003BE } },
      { 0x1D6D0, { 0x0006F } },
      { 0x1D6D1, { 0x003C0 } },
      { 0x1D6D2, { 0x00070 } },
      { 0x1D6D3, { 0x003C2 } },
      { 0x1D6D4, { 0x0006F } },
      { 0x1D6D5, { 0x01D1B } },
      { 0x1D6D6, { 0x00075 } },
      { 0x1D6D7, { 0x00278 } },
      { 0x1D6D8, { 0x003C7 } },
      { 0x1D6D9, { 0x003C8 } },
      { 0x1D6DA, { 0x003C9 } },
      { 0x1D6DB, { 0x02202 } },
      { 0x1D6DC, { 0x0A793 } },
      { 0x1D6DD, { 0x0004F, 0x00335 } },
      { 0x1D6DE, { 0x00138 } },
      { 0x1D6DF, { 0x00278 } },
      { 0x1D6E0, { 0x00070 } },
      { 0x1D6E1, { 0x003C0 } },
      { 0x1D6E2, { 0x00041 } },
      { 0x1D6E3, { 0x00042 } },
      { 0x1D6E4, { 0x00393 } },
      { 0x1D6E5, { 0x00394 } },
      { 0x1D6E6, { 0x00045 } },
      { 0x1D6E7, { 0x0005A } },
      { 0x1D6E8, { 0x00048 } },
      { 0x1D6E9, { 0x0004F, 0x00335 } },
      { 0x1D6EA, { 0x0006C } },
      { 0x1D6EB, { 0x0004B } },
      { 0x1D6EC, { 0x00245 } },
      { 0x1D6ED, { 0x0004D } },
      { 0x1D6EE, { 0x0004E } },
      { 0x1D6EF, { 0x0039E } },
      { 0x1D6F0, { 0x0004F } },
      { 0x1D6F1, { 0x003A0 } },
      { 0x1D6F2, { 0x00050 } },
      { 0x1D6F3, { 0x0004F, 0x00335 } },
      { 0x1D6F4, { 0x001A9 } },
      { 0x1D6F5, { 0x00054 } },
      { 0x1D6F6, { 0x00059 } },
      { 0x1D6F7, { 0x003A6 } },
      { 0x1D6F8, { 0x00058 } },
      { 0x1D6F9, { 0x003A8 } },
      { 0x1D6FA, { 0x003A9 } },
      { 0x1D6FB, { 0x02207 } },
      { 0x1D6FC, { 0x00061 } },
      { 0x1D6FD, { 0x000DF } },
      { 0x1D6FE, { 0x00079 } },
      { 0x1D6FF, { 0x01E9F } },
      { 0x1D700, { 0x0A793 } },
      { 0x1D701, { 0x003B6 } },
      { 0x1D702, { 0x0006E, 0x00329 } },
      { 0x1D703, { 0x0004F, 0x00335 } },
      { 0x1D704, { 0x00069 } },
      { 0x1D705, { 0x00138 } },
      { 0x1D706, { 0x003BB } },
      { 0x1D707, { 0x003BC } },
      { 0x1D708, { 0x00076 } },
      { 0x1D709, { 0x003BE } },
      { 0x1D70A, { 0x0006F } },
      { 0x1D70B, { 0x003C0 } },
      { 0x1D70C, { 0x00070 } },
      { 0x1D70D, { 0x003C2 } },
      { 0x1D70E, { 0x0006F } },
      { 0x1D70F, { 0x01D1B } },
      { 0x1D710, { 0x00075 } },
      { 0x1D711, { 0x00278 } },
      { 0x1D712, { 0x003C7 } },
      { 0x1D713, { 0x003C8 } },
      { 0x1D714, { 0x003C9 } },
      { 0x1D715, { 0x02202 } },
      { 0x1D716, { 0x0A793 } },
      { 0x1D717, { 0x0004F, 0x00335 } },
      { 0x1D718, { 0x00138 } },
      { 0x1D719, { 0x00278 } },
      { 0x1D71A, { 0x00070 } },
      { 0x1D71B, { 0x003C0 } },
      { 0x1D71C, { 0x00041 } },
      { 0x1D71D, { 0x00042 } },
      { 0x1D71E, { 0x00393 } },
      { 0x1D71F, { 0x00394 } },
      { 0x1D720, { 0x00045 } },
      { 0x1D721, { 0x0005A } },
      { 0x1D722, { 0x00048 } },
      { 0x1D723, { 0x0004F, 0x00335 } },
      { 0x1D724, { 0x0006C } },
      { 0x1D725, { 0x0004B } },
      { 0x1D726, { 0x00245 } },
      { 0x1D727, { 0x0004D } },
      { 0x1D728, { 0x0004E } },
      { 0x1D729, { 0x0039E } },
      { 0x1D72A, { 0x0004F } },
      { 0x1D72B, { 0x003A0 } },
      { 0x1D72C, { 0x00050 } },
      { 0x1D72D, { 0x0004F, 0x00335 } },
      { 0x1D72E, { 0x001A9 } },
      { 0x1D72F, { 0x00054 } },
      { 0x1D730, { 0x00059 } },
      { 0x1D731, { 0x003A6 } },
      { 0x1D732, { 0x00058 } },
      { 0x1D733, { 0x003A8 } },
      { 0x1D734, { 0x003A9 } },
      { 0x1D735, { 0x02207 } },
      { 0x1D736, { 0x00061 } },
      { 0x1D737, { 0x000DF } },
      { 0x1D738, { 0x00079 } },
      { 0x1D739, { 0x01E9F } },
      { 0x1D73A, { 0x0A793 } },
      { 0x1D73B, { 0x003B6 } },
      { 0x1D73C, { 0x0006E, 0x00329 } },
      { 0x1D73D, { 0x0004F, 0x00335 } },
      { 0x1D73E, { 0x00069 } },
      { 0x1D73F, { 0x00138 } },
      { 0x1D740, { 0x003BB } },
      { 0x1D741, { 0x003BC } },
      { 0x1D742, { 0x00076 } },
      { 0x1D743, { 0x003BE } },
      { 0x1D744, { 0x0006F } },
      { 0x1D745, { 0x003C0 } },
      { 0x1D746, { 0x00070 } },
      { 0x1D747, { 0x003C2 } },
      { 0x1D748, { 0x0006F } },
      { 0x1D749, { 0x01D1B } },
      { 0x1D74A, { 0x00075 } },
      { 0x1D74B, { 0x00278 } },
      { 0x1D74C, { 0x003C7 } },
      { 0x1D74D, { 0x003C8 } },
      { 0x1D74E, { 0x003C9 } },
      { 0x1D74F, { 0x02202 } },
      { 0x1D750, { 0x0A793 } },
      { 0x1D751, { 0x0004F, 0x00335 } },
      { 0x1D752, { 0x00138 } },
      { 0x1D753, { 0x00278 } },
      { 0x1D754, { 0x00070 } },
      { 0x1D755, { 0x003C0 } },
      { 0x1D756, { 0x00041 } },
      { 0x1D757, { 0x00042 } },
      { 0x1D758, { 0x00393 } },
      { 0x1D759, { 0x00394 } },
      { 0x1D75A, { 0x00045 } },
      { 0x1D75B, { 0x0005A } },
      { 0x1D75C, { 0x00048 } },
      { 0x1D75D, { 0x0004F, 0x00335 } },
      { 0x1D75E, { 0x0006C } },
      { 0x1D75F, { 0x0004B } },
      { 0x1D760, { 0x00245 } },
      { 0x1D761, { 0x0004D } },
      { 0x1D762, { 0x0004E } },
      { 0x1D763, { 0x0039E } },
      { 0x1D764, { 0x0004F } },
      { 0x1D765, { 0x003A0 } },
      { 0x1D766, { 0x00050 } },
      { 0x1D767, { 0x0004F, 0x00335 } },
      { 0x1D768, { 0x001A9 } },
      { 0x1D769, { 0x00054 } },
      { 0x1D76A, { 0x00059 } },
      { 0x1D76B, { 0x003A6 } },
      { 0x1D76C, { 0x00058 } },
      { 0x1D76D, { 0x003A8 } },
      { 0x1D76E, { 0x003A9 } },
      { 0x1D76F, { 0x02207 } },
      { 0x1D770, { 0x00061 } },
      { 0x1D771, { 0x000DF } },
      { 0x1D772, { 0x00079 } },
      { 0x1D773, { 0x01E9F } },
      { 0x1D774, { 0x0A793 } },
      { 0x1D775, { 0x003B6 } },
      { 0x1D776, { 0x0006E, 0x00329 } },
      { 0x1D777, { 0x0004F, 0x00335 } },
      { 0x1D778, { 0x00069 } },
      { 0x1D779, { 0x00138 } },
      { 0x1D77A, { 0x003BB } },
      { 0x1D77B, { 0x003BC } },
      { 0x1D77C, { 0x00076 } },
      { 0x1D77D, { 0x003BE } },
      { 0x1D77E, { 0x0006F } },
      { 0x1D77F, { 0x003C0 } },
      { 0x1D780, { 0x00070 } },
      { 0x1D781, { 0x003C2 } },
      { 0x1D782, { 0x0006F } },
      { 0x1D783, { 0x01D1B } },
      { 0x1D784, { 0x00075 } },
      { 0x1D785, { 0x00278 } },
      { 0x1D786, { 0x003C7 } },
      { 0x1D787, { 0x003C8 } },
      { 0x1D788, { 0x003C9 } },
      { 0x1D789, { 0x02202 } },
      { 0x1D78A, { 0x0A793 } },
      { 0x1D78B, { 0x0004F, 0x00335 } },
      { 0x1D78C, { 0x00138 } },
      { 0x1D78D, { 0x00278 } },
      { 0x1D78E, { 0x00070 } },
      { 0x1D78F, { 0x003C0 } },
      { 0x1D790, { 0x00041 } },
      { 0x1D791, { 0x00042 } },
      { 0x1D792, { 0x00393 } },
      { 0x1D793, { 0x00394 } },
      { 0x1D794, { 0x00045 } },
      { 0x1D795, { 0x0005A } },
      { 0x1D796, { 0x00048 } },
      { 0x1D797, { 0x0004F, 0x00335 } },
      { 0x1D798, { 0x0006C } },
      { 0x1D799, { 0x0004B } },
      { 0x1D79A, { 0x00245 } },
      { 0x1D79B, { 0x0004D } },
      { 0x1D79C, { 0x0004E } },
      { 0x1D79D, { 0x0039E } },
      { 0x1D79E, { 0x0004F } },
      { 0x1D79F, { 0x003A0 } },
      { 0x1D7A0, { 0x00050 } },
      { 0x1D7A1, { 0x0004F, 0x00335 } },
      { 0x1D7A2, { 0x001A9 } },
      { 0x1D7A3, { 0x00054 } },
      { 0x1D7A4, { 0x00059 } },
      { 0x1D7A5, { 0x003A6 } },
      { 0x1D7A6, { 0x00058 } },
      { 0x1D7A7, { 0x003A8 } },
      { 0x1D7A8, { 0x003A9 } },
      { 0x1D7A9, { 0x02207 } },
      { 0x1D7AA, { 0x00061 } },
      { 0x1D7AB, { 0x000DF } },
      { 0x1D7AC, { 0x00079 } },
      { 0x1D7AD, { 0x01E9F } },
      { 0x1D7AE, { 0x0A793 } },
      { 0x1D7AF, { 0x003B6 } },
      { 0x1D7B0, { 0x0006E, 0x00329 } },
      { 0x1D7B1, { 0x0004F, 0x00335 } },
      { 0x1D7B2, { 0x00069 } },
      { 0x1D7B3, { 0x00138 } },
      { 0x1D7B4, { 0x003BB } },
      { 0x1D7B5, { 0x003BC } },
      { 0x1D7B6, { 0x00076 } },
      { 0x1D7B7, { 0x003BE } },
      { 0x1D7B8, { 0x0006F } },
      { 0x1D7B9, { 0x003C0 } },
      { 0x1D7BA, { 0x00070 } },
      { 0x1D7BB, { 0x003C2 } },
      { 0x1D7BC, { 0x0006F } },
      { 0x1D7BD, { 0x01D1B } },
      { 0x1D7BE, { 0x00075 } },
      { 0x1D7BF, { 0x00278 } },
      { 0x1D7C0, { 0x003C7 } },
      { 0x1D7C1, { 0x003C8 } },
      { 0x1D7C2, { 0x003C9 } },
      { 0x1D7C3, { 0x02202 } },
      { 0x1D7C4, { 0x0A793 } },
      { 0x1D7C5, { 0x0004F, 0x00335 } },
      { 0x1D7C6, { 0x00138 } },
      { 0x1D7C7, { 0x00278 } },
      { 0x1D7C8, { 0x00070 } },
      { 0x1D7C9, { 0x003C0 } },
      { 0x1D7CA, { 0x00046 } },
      { 0x1D7CB, { 0x003DD } },
      { 0x1D7CE, { 0x0004F } },
      { 0x1D7CF, { 0x0006C } },
      { 0x1D7D0, { 0x00032 } },
      { 0x1D7D1, { 0x00033 } },
      { 0x1D7D2, { 0x00034 } },
      { 0x1D7D3, { 0x00035 } },
      { 0x1D7D4, { 0x00036 } },
      { 0x1D7D5, { 0x00037 } },
      { 0x1D7D6, { 0x00038 } },
      { 0x1D7D7, { 0x00039 } },
      { 0x1D7D8, { 0x0004F } },
      { 0x1D7D9, { 0x0006C } },
      { 0x1D7DA, { 0x00032 } },
      { 0x1D7DB, { 0x00033 } },
      { 0x1D7DC, { 0x00034 } },
      { 0x1D7DD, { 0x00035 } },
      { 0x1D7DE, { 0x00036 } },
      { 0x1D7DF, { 0x00037 } },
      { 0x1D7E0, { 0x00038 } },
      { 0x1D7E1, { 0x00039 } },
      { 0x1D7E2, { 0x0004F } },
      { 0x1D7E3, { 0x0006C } },
      { 0x1D7E4, { 0x00032 } },
      { 0x1D7E5, { 0x00033 } },
      { 0x1D7E6, { 0x00034 } },
      { 0x1D7E7, { 0x00035 } },
      { 0x1D7E8, { 0x00036 } },
      { 0x1D7E9, { 0x00037 } },
      { 0x1D7EA, { 0x00038 } },
      { 0x1D7EB, { 0x00039 } },
      { 0x1D7EC, { 0x0004F } },
      { 0x1D7ED, { 0x0006C } },
      { 0x1D7EE, { 0x00032 } },
      { 0x1D7EF, { 0x00033 } },
      { 0x1D7F0, { 0x00034 } },
      { 0x1D7F1, { 0x00035 } },
      { 0x1D7F2, { 0x00036 } },
      { 0x1D7F3, { 0x00037 } },
      { 0x1D7F4, { 0x00038 } },
      { 0x1D7F5, { 0x00039 } },
      { 0x1D7F6, { 0x0004F } },
      { 0x1D7F7, { 0x0006C } },
      { 0x1D7F8, { 0x00032 } },
      { 0x1D7F9, { 0x00033 } },
      { 0x1D7FA, { 0x00034 } },
      { 0x1D7FB, { 0x00035 } },
      { 0x1D7FC, { 0x00036 } },
      { 0x1D7FD, { 0x00037 } },
      { 0x1D7FE, { 0x00038 } },
      { 0x1D7FF, { 0x00039 } },
      { 0x1E6E9, { 0x0002B } },
      { 0x1E6EE, { 0x01AC8 } },
      { 0x1E8C7, { 0x0006C } },
      { 0x1E8C8, { 0x02220 } },
      { 0x1E8C9, { 0x00663 } },
      { 0x1E8CB, { 0x00038 } },
      { 0x1E8CC, { 0x02202 } },
      { 0x1E8CD, { 0x02202, 0x00335 } },
      { 0x1EE00, { 0x0006C } },
      { 0x1EE01, { 0x00628 } },
      { 0x1EE02, { 0x0062C } },
      { 0x1EE03, { 0x0062F } },
      { 0x1EE05, { 0x00648 } },
      { 0x1EE06, { 0x00632 } },
      { 0x1EE07, { 0x0062D } },
      { 0x1EE08, { 0x00637 } },
      { 0x1EE09, { 0x00649 } },
      { 0x1EE0A, { 0x00643 } },
      { 0x1EE0B, { 0x00644 } },
      { 0x1EE0C, { 0x00645 } },
      { 0x1EE0D, { 0x00646 } },
      { 0x1EE0E, { 0x00633 } },
      { 0x1EE0F, { 0x00639 } },
      { 0x1EE10, { 0x00641 } },
      { 0x1EE11, { 0x00635 } },
      { 0x1EE12, { 0x00642 } },
      { 0x1EE13, { 0x00631 } },
      { 0x1EE14, { 0x00633, 0x006DB } },
      { 0x1EE15, { 0x0062A } },
      { 0x1EE16, { 0x00649, 0x006DB } },
      { 0x1EE17, { 0x0062E } },
      { 0x1EE18, { 0x00630 } },
      { 0x1EE19, { 0x00636 } },
      { 0x1EE1A, { 0x00638 } },
      { 0x1EE1B, { 0x0063A } },
      { 0x1EE1C, { 0x00649 } },
      { 0x1EE1D, { 0x00649 } },
      { 0x1EE1E, { 0x006A1 } },
      { 0x1EE1F, { 0x006A1 } },
      { 0x1EE21, { 0x00628 } },
      { 0x1EE22, { 0x0062C } },
      { 0x1EE24, { 0x0006F } },
      { 0x1EE27, { 0x0062D } },
      { 0x1EE29, { 0x00649 } },
      { 0x1EE2A, { 0x00643 } },
      { 0x1EE2B, { 0x00644 } },
      { 0x1EE2C, { 0x00645 } },
      { 0x1EE2D, { 0x00646 } },
      { 0x1EE2E, { 0x00633 } },
      { 0x1EE2F, { 0x00639 } },
      { 0x1EE30, { 0x00641 } },
      { 0x1EE31, { 0x00635 } },
      { 0x1EE32, { 0x00642 } },
      { 0x1EE34, { 0x00633, 0x006DB } },
      { 0x1EE35, { 0x0062A } },
      { 0x1EE36, { 0x00649, 0x006DB } },
      { 0x1EE37, { 0x0062E } },
      { 0x1EE39, { 0x00636 } },
      { 0x1EE3B, { 0x0063A } },
      { 0x1EE42, { 0x0062C } },
      { 0x1EE47, { 0x0062D } },
      { 0x1EE49, { 0x00649 } },
      { 0x1EE4B, { 0x00644 } },
      { 0x1EE4D, { 0x00646 } },
      { 0x1EE4E, { 0x00633 } },
      { 0x1EE4F, { 0x00639 } },
      { 0x1EE51, { 0x00635 } },
      { 0x1EE52, { 0x00642 } },
      { 0x1EE54, { 0x00633, 0x006DB } },
      { 0x1EE57, { 0x0062E } },
      { 0x1EE59, { 0x00636 } },
      { 0x1EE5B, { 0x0063A } },
      { 0x1EE5D, { 0x00649 } },
      { 0x1EE5F, { 0x006A1 } },
      { 0x1EE61, { 0x00628 } },
      { 0x1EE62, { 0x0062C } },
      { 0x1EE64, { 0x0006F } },
      { 0x1EE67, { 0x0062D } },
      { 0x1EE68, { 0x00637 } },
      { 0x1EE69, { 0x00649 } },
      { 0x1EE6A, { 0x00643 } },
      { 0x1EE6C, { 0x00645 } },
      { 0x1EE6D, { 0x00646 } },
      { 0x1EE6E, { 0x00633 } },
      { 0x1EE6F, { 0x00639 } },
      { 0x1EE70, { 0x00641 } },
      { 0x1EE71, { 0x00635 } },
      { 0x1EE72, { 0x00642 } },
      { 0x1EE74, { 0x00633, 0x006DB } },
      { 0x1EE75, { 0x0062A } },
      { 0x1EE76, { 0x00649, 0x006DB } },
      { 0x1EE77, { 0x0062E } },
      { 0x1EE79, { 0x00636 } },
      { 0x1EE7A, { 0x00638 } },
      { 0x1EE7B, { 0x0063A } },
      { 0x1EE7C, { 0x00649 } },
      { 0x1EE7E, { 0x006A1 } },
      { 0x1EE80, { 0x0006C } },
      { 0x1EE81, { 0x00628 } },
      { 0x1EE82, { 0x0062C } },
      { 0x1EE83, { 0x0062F } },
      { 0x1EE84, { 0x0006F } },
      { 0x1EE85, { 0x00648 } },
      { 0x1EE86, { 0x00632 } },
      { 0x1EE87, { 0x0062D } },
      { 0x1EE88, { 0x00637 } },
      { 0x1EE89, { 0x00649 } },
      { 0x1EE8B, { 0x00644 } },
      { 0x1EE8C, { 0x00645 } },
      { 0x1EE8D, { 0x00646 } },
      { 0x1EE8E, { 0x00633 } },
      { 0x1EE8F, { 0x00639 } },
      { 0x1EE90, { 0x00641 } },
      { 0x1EE91, { 0x00635 } },
      { 0x1EE92, { 0x00642 } },
      { 0x1EE93, { 0x00631 } },
      { 0x1EE94, { 0x00633, 0x006DB } },
      { 0x1EE95, { 0x0062A } },
      { 0x1EE96, { 0x00649, 0x006DB } },
      { 0x1EE97, { 0x0062E } },
      { 0x1EE98, { 0x00630 } },
      { 0x1EE99, { 0x00636 } },
      { 0x1EE9A, { 0x00638 } },
      { 0x1EE9B, { 0x0063A } },
      { 0x1EEA1, { 0x00628 } },
      { 0x1EEA2, { 0x0062C } },
      { 0x1EEA3, { 0x0062F } },
      { 0x1EEA5, { 0x00648 } },
      { 0x1EEA6, { 0x00632 } },
      { 0x1EEA7, { 0x0062D } },
      { 0x1EEA8, { 0x00637 } },
      { 0x1EEA9, { 0x00649 } },
      { 0x1EEAB, { 0x00644 } },
      { 0x1EEAC, { 0x00645 } },
      { 0x1EEAD, { 0x00646 } },
      { 0x1EEAE, { 0x00633 } },
      { 0x1EEAF, { 0x00639 } },
      { 0x1EEB0, { 0x00641 } },
      { 0x1EEB1, { 0x00635 } },
      { 0x1EEB2, { 0x00642 } },
      { 0x1EEB3, { 0x00631 } },
      { 0x1EEB4, { 0x00633, 0x006DB } },
      { 0x1EEB5, { 0x0062A } },
      { 0x1EEB6, { 0x00649, 0x006DB } },
      { 0x1EEB7, { 0x0062E } },
      { 0x1EEB8, { 0x00630 } },
      { 0x1EEB9, { 0x00636 } },
      { 0x1EEBA, { 0x00638 } },
      { 0x1EEBB, { 0x0063A } },
      { 0x1F100, { 0x0004F, 0x0002E } },
      { 0x1F101, { 0x0004F, 0x0002C } },
      { 0x1F102, { 0x0006C, 0x0002C } },
      { 0x1F103, { 0x00032, 0x0002C } },
      { 0x1F104, { 0x00033, 0x0002C } },
      { 0x1F105, { 0x00034, 0x0002C } },
      { 0x1F106, { 0x00035, 0x0002C } },
      { 0x1F107, { 0x00036, 0x0002C } },
      { 0x1F108, { 0x00037, 0x0002C } },
      { 0x1F109, { 0x00038, 0x0002C } },
      { 0x1F10A, { 0x00039, 0x0002C } },
      { 0x1F10F, { 0x00024, 0x020E0 } },
      { 0x1F110, { 0x00028, 0x00041, 0x00029 } },
      { 0x1F111, { 0x00028, 0x00042, 0x00029 } },
      { 0x1F112, { 0x00028, 0x00043, 0x00029 } },
      { 0x1F113, { 0x00028, 0x00044, 0x00029 } },
      { 0x1F114, { 0x00028, 0x00045, 0x00029 } },
      { 0x1F115, { 0x00028, 0x00046, 0x00029 } },
      { 0x1F116, { 0x00028, 0x00047, 0x00029 } },
      { 0x1F117, { 0x00028, 0x00048, 0x00029 } },
      { 0x1F118, { 0x00028, 0x0006C, 0x00029 } },
      { 0x1F119, { 0x00028, 0x0004A, 0x00029 } },
      { 0x1F11A, { 0x00028, 0x0004B, 0x00029 } },
      { 0x1F11B, { 0x00028, 0x0004C, 0x00029 } },
      { 0x1F11C, { 0x00028, 0x0004D, 0x00029 } },
      { 0x1F11D, { 0x00028, 0x0004E, 0x00029 } },
      { 0x1F11E, { 0x00028, 0x0004F, 0x00029 } },
      { 0x1F11F, { 0x00028, 0x00050, 0x00029 } },
      { 0x1F120, { 0x00028, 0x00051, 0x00029 } },
      { 0x1F121, { 0x00028, 0x00052, 0x00029 } },
      { 0x1F122, { 0x00028, 0x00053, 0x00029 } },
      { 0x1F123, { 0x00028, 0x00054, 0x00029 } },
      { 0x1F124, { 0x00028, 0x00055, 0x00029 } },
      { 0x1F125, { 0x00028, 0x00056, 0x00029 } },
      { 0x1F126, { 0x00028, 0x00057, 0x00029 } },
      { 0x1F127, { 0x00028, 0x00058, 0x00029 } },
      { 0x1F128, { 0x00028, 0x00059, 0x00029 } },
      { 0x1F129, { 0x00028, 0x0005A, 0x00029 } },
      { 0x1F12A, { 0x00028, 0x00053, 0x00029 } },
      { 0x1F16D, { 0x033C4, 0x00009, 0x020DD } },
      { 0x1F16E, { 0x00043, 0x020E0 } },
      { 0x1F240, { 0x00028, 0x0672C, 0x00029 } },
      { 0x1F241, { 0x00028, 0x04E09, 0x00029 } },
      { 0x1F242, { 0x00028, 0x04E8C, 0x00029 } },
      { 0x1F243, { 0x00028, 0x05B89, 0x00029 } },
      { 0x1F244, { 0x00028, 0x070B9, 0x00029 } },
      { 0x1F245, { 0x00028, 0x06253, 0x00029 } },
      { 0x1F246, { 0x00028, 0x076D7, 0x00029 } },
      { 0x1F247, { 0x00028, 0x052DD, 0x00029 } },
      { 0x1F248, { 0x00028, 0x06557, 0x00029 } },
      { 0x1F312, { 0x0263D } },
      { 0x1F318, { 0x0263E } },
      { 0x1F319, { 0x0263D } },
      { 0x1F333, { 0x1CEBC } },
      { 0x1F34E, { 0x1CEBD } },
      { 0x1F34F, { 0x1CEBD } },
      { 0x1F352, { 0x1CEBE } },
      { 0x1F353, { 0x1CEBF } },
      { 0x1F377, { 0x1CEBA } },
      { 0x1F3E2, { 0x1CEBB } },
      { 0x1F40D, { 0x1CCFA } },
      { 0x1F443, { 0x1CCFC } },
      { 0x1F514, { 0x1FBFA } },
      { 0x1F700, { 0x00051, 0x00045 } },
      { 0x1F701, { 0x0A658 } },
      { 0x1F702, { 0x00394 } },
      { 0x1F704, { 0x102BC } },
      { 0x1F707, { 0x00041, 0x00052 } },
      { 0x1F708, { 0x00056, 0x01DE4 } },
      { 0x1F70A, { 0x02629 } },
      { 0x1F714, { 0x0004F, 0x00335 } },
      { 0x1F728, { 0x102A8 } },
      { 0x1F73A, { 0x029DF } },
      { 0x1F74C, { 0x00043 } },
      { 0x1F754, { 0x016DC } },
      { 0x1F755, { 0x022A1 } },
      { 0x1F75C, { 0x00073, 0x00073, 0x00073 } },
      { 0x1F75E, { 0x0224F } },
      { 0x1F768, { 0x00054 } },
      { 0x1F76B, { 0x0004D, 0x00042 } },
      { 0x1F76C, { 0x00056, 0x00042 } },
      { 0x1F771, { 0x022A0 } },
      { 0x1FBF0, { 0x0004F } },
      { 0x1FBF1, { 0x0006C } },
      { 0x1FBF2, { 0x00032 } },
      { 0x1FBF3, { 0x00033 } },
      { 0x1FBF4, { 0x00034 } },
      { 0x1FBF5, { 0x00035 } },
      { 0x1FBF6, { 0x00036 } },
      { 0x1FBF7, { 0x00037 } },
      { 0x1FBF8, { 0x00038 } },
      { 0x1FBF9, { 0x00039 } },
      { 0x20674, { 0x051F5 } },
      { 0x21533, { 0x058F7 } },
      { 0x21587, { 0x0591A } },
      { 0x216A7, { 0x216A8 } },
      { 0x21FE8, { 0x0276C } },
      { 0x22505, { 0x05F9A } },
      { 0x23D40, { 0x06D85 } },
      { 0x248FD, { 0x073A5 } },
      { 0x2511A, { 0x25119 } },
      { 0x25AD4, { 0x08D1B } },
      { 0x2659D, { 0x265A8 } },
      { 0x26D06, { 0x26C36 } },
      { 0x2AEC5, { 0x24814 } },
      { 0x2B73A, { 0x05CC0 } },
      { 0x2B73E, { 0x2335F } },
      { 0x2D161, { 0x05351 } },
      { 0x2F800, { 0x04E3D } },
      { 0x2F801, { 0x04E38 } },
      { 0x2F802, { 0x04E41 } },
      { 0x2F803, { 0x20122 } },
      { 0x2F804, { 0x04F60 } },
      { 0x2F805, { 0x04FAE } },
      { 0x2F806, { 0x04FBB } },
      { 0x2F807, { 0x04F75 } },
      { 0x2F808, { 0x0507A } },
      { 0x2F809, { 0x05099 } },
      { 0x2F80A, { 0x050E7 } },
      { 0x2F80B, { 0x050CF } },
      { 0x2F80C, { 0x0349E } },
      { 0x2F80D, { 0x2063A } },
      { 0x2F80E, { 0x0514D } },
      { 0x2F80F, { 0x05154 } },
      { 0x2F810, { 0x05164 } },
      { 0x2F811, { 0x05177 } },
      { 0x2F812, { 0x2051C } },
      { 0x2F813, { 0x034B9 } },
      { 0x2F814, { 0x05167 } },
      { 0x2F815, { 0x0518D } },
      { 0x2F816, { 0x2054B } },
      { 0x2F817, { 0x05197 } },
      { 0x2F818, { 0x051A4 } },
      { 0x2F819, { 0x04ECC } },
      { 0x2F81A, { 0x051AC } },
      { 0x2F81B, { 0x051B5 } },
      { 0x2F81C, { 0x291DF } },
      { 0x2F81D, { 0x051F5 } },
      { 0x2F81E, { 0x05203 } },
      { 0x2F81F, { 0x034DF } },
      { 0x2F820, { 0x0523B } },
      { 0x2F821, { 0x05246 } },
      { 0x2F822, { 0x05272 } },
      { 0x2F823, { 0x05277 } },
      { 0x2F824, { 0x03515 } },
      { 0x2F825, { 0x052C7 } },
      { 0x2F826, { 0x052C9 } },
      { 0x2F827, { 0x052E4 } },
      { 0x2F828, { 0x052FA } },
      { 0x2F829, { 0x05305 } },
      { 0x2F82A, { 0x05306 } },
      { 0x2F82B, { 0x05317 } },
      { 0x2F82C, { 0x05349 } },
      { 0x2F82D, { 0x05351 } },
      { 0x2F82E, { 0x0535A } },
      { 0x2F82F, { 0x05373 } },
      { 0x2F830, { 0x0537D } },
      { 0x2F831, { 0x0537F } },
      { 0x2F832, { 0x0537F } },
      { 0x2F833, { 0x0537F } },
      { 0x2F834, { 0x20A2C } },
      { 0x2F835, { 0x07070 } },
      { 0x2F836, { 0x053CA } },
      { 0x2F837, { 0x053DF } },
      { 0x2F838, { 0x20B63 } },
      { 0x2F839, { 0x053EB } },
      { 0x2F83A, { 0x053F1 } },
      { 0x2F83B, { 0x05406 } },
      { 0x2F83C, { 0x0549E } },
      { 0x2F83D, { 0x05438 } },
      { 0x2F83E, { 0x05448 } },
      { 0x2F83F, { 0x05468 } },
      { 0x2F840, { 0x054A2 } },
      { 0x2F841, { 0x054F6 } },
      { 0x2F842, { 0x05510 } },
      { 0x2F843, { 0x05553 } },
      { 0x2F844, { 0x05563 } },
      { 0x2F845, { 0x05584 } },
      { 0x2F846, { 0x05584 } },
      { 0x2F847, { 0x05599 } },
      { 0x2F848, { 0x055AB } },
      { 0x2F849, { 0x055B3 } },
      { 0x2F84A, { 0x055C2 } },
      { 0x2F84B, { 0x05716 } },
      { 0x2F84C, { 0x05606 } },
      { 0x2F84D, { 0x05717 } },
      { 0x2F84E, { 0x05651 } },
      { 0x2F84F, { 0x05674 } },
      { 0x2F850, { 0x05207 } },
      { 0x2F851, { 0x058EE } },
      { 0x2F852, { 0x057CE } },
      { 0x2F853, { 0x057F4 } },
      { 0x2F854, { 0x0580D } },
      { 0x2F855, { 0x0578B } },
      { 0x2F856, { 0x05832 } },
      { 0x2F857, { 0x05831 } },
      { 0x2F858, { 0x058AC } },
      { 0x2F859, { 0x214E4 } },
      { 0x2F85A, { 0x058F2 } },
      { 0x2F85B, { 0x058F7 } },
      { 0x2F85C, { 0x05906 } },
      { 0x2F85D, { 0x0591A } },
      { 0x2F85E, { 0x05922 } },
      { 0x2F85F, { 0x05962 } },
      { 0x2F860, { 0x216A8 } },
      { 0x2F861, { 0x216EA } },
      { 0x2F862, { 0x059EC } },
      { 0x2F863, { 0x05A1B } },
      { 0x2F864, { 0x05A27 } },
      { 0x2F865, { 0x059D8 } },
      { 0x2F866, { 0x05A66 } },
      { 0x2F867, { 0x036EE } },
      { 0x2F868, { 0x036FC } },
      { 0x2F869, { 0x05B08 } },
      { 0x2F86A, { 0x05B3E } },
      { 0x2F86B, { 0x05B3E } },
      { 0x2F86C, { 0x219C8 } },
      { 0x2F86D, { 0x05BC3 } },
      { 0x2F86E, { 0x05BD8 } },
      { 0x2F86F, { 0x05BE7 } },
      { 0x2F870, { 0x05BF3 } },
      { 0x2F871, { 0x21B18 } },
      { 0x2F872, { 0x05BFF } },
      { 0x2F873, { 0x05C06 } },
      { 0x2F874, { 0x05F53 } },
      { 0x2F875, { 0x05C22 } },
      { 0x2F876, { 0x03781 } },
      { 0x2F877, { 0x05C60 } },
      { 0x2F878, { 0x05C6E } },
      { 0x2F879, { 0x05CC0 } },
      { 0x2F87A, { 0x05C8D } },
      { 0x2F87B, { 0x21DE4 } },
      { 0x2F87C, { 0x05D43 } },
      { 0x2F87D, { 0x21DE6 } },
      { 0x2F87E, { 0x05D6E } },
      { 0x2F87F, { 0x05D6B } },
      { 0x2F880, { 0x05D7C } },
      { 0x2F881, { 0x05DE1 } },
      { 0x2F882, { 0x05DE2 } },
      { 0x2F883, { 0x0382F } },
      { 0x2F884, { 0x05DFD } },
      { 0x2F885, { 0x05E28 } },
      { 0x2F886, { 0x05E3D } },
      { 0x2F887, { 0x05E69 } },
      { 0x2F888, { 0x03862 } },
      { 0x2F889, { 0x22183 } },
      { 0x2F88A, { 0x0387C } },
      { 0x2F88B, { 0x05EB0 } },
      { 0x2F88C, { 0x05EB3 } },
      { 0x2F88D, { 0x05EB6 } },
      { 0x2F88E, { 0x05ECA } },
      { 0x2F88F, { 0x2A392 } },
      { 0x2F890, { 0x05EFE } },
      { 0x2F891, { 0x22331 } },
      { 0x2F892, { 0x22331 } },
      { 0x2F893, { 0x08201 } },
      { 0x2F894, { 0x05F22 } },
      { 0x2F895, { 0x05F22 } },
      { 0x2F896, { 0x038C7 } },
      { 0x2F897, { 0x232B8 } },
      { 0x2F898, { 0x261DA } },
      { 0x2F899, { 0x05F62 } },
      { 0x2F89A, { 0x05F6B } },
      { 0x2F89B, { 0x038E3 } },
      { 0x2F89C, { 0x05F9A } },
      { 0x2F89D, { 0x05FCD } },
      { 0x2F89E, { 0x05FD7 } },
      { 0x2F89F, { 0x05FF9 } },
      { 0x2F8A0, { 0x06081 } },
      { 0x2F8A1, { 0x0393A } },
      { 0x2F8A2, { 0x0391C } },
      { 0x2F8A3, { 0x06094 } },
      { 0x2F8A4, { 0x226D4 } },
      { 0x2F8A5, { 0x060C7 } },
      { 0x2F8A6, { 0x06148 } },
      { 0x2F8A7, { 0x0614C } },
      { 0x2F8A8, { 0x0614E } },
      { 0x2F8A9, { 0x0614C } },
      { 0x2F8AA, { 0x0617A } },
      { 0x2F8AB, { 0x0618E } },
      { 0x2F8AC, { 0x061B2 } },
      { 0x2F8AD, { 0x061A4 } },
      { 0x2F8AE, { 0x061AF } },
      { 0x2F8AF, { 0x061DE } },
      { 0x2F8B0, { 0x061F2 } },
      { 0x2F8B1, { 0x061F6 } },
      { 0x2F8B2, { 0x06210 } },
      { 0x2F8B3, { 0x0621B } },
      { 0x2F8B4, { 0x0625D } },
      { 0x2F8B5, { 0x062B1 } },
      { 0x2F8B6, { 0x062D4 } },
      { 0x2F8B7, { 0x06350 } },
      { 0x2F8B8, { 0x22B0C } },
      { 0x2F8B9, { 0x0633D } },
      { 0x2F8BA, { 0x062FC } },
      { 0x2F8BB, { 0x06368 } },
      { 0x2F8BC, { 0x06383 } },
      { 0x2F8BD, { 0x063E4 } },
      { 0x2F8BE, { 0x22BF1 } },
      { 0x2F8BF, { 0x06422 } },
      { 0x2F8C0, { 0x063C5 } },
      { 0x2F8C1, { 0x063A9 } },
      { 0x2F8C2, { 0x03A2E } },
      { 0x2F8C3, { 0x06469 } },
      { 0x2F8C4, { 0x0647E } },
      { 0x2F8C5, { 0x0649D } },
      { 0x2F8C6, { 0x06477 } },
      { 0x2F8C7, { 0x03A6C } },
      { 0x2F8C8, { 0x0654F } },
      { 0x2F8C9, { 0x0656C } },
      { 0x2F8CA, { 0x2300A } },
      { 0x2F8CB, { 0x065E3 } },
      { 0x2F8CC, { 0x066F8 } },
      { 0x2F8CD, { 0x06649 } },
      { 0x2F8CE, { 0x03B19 } },
      { 0x2F8CF, { 0x06691 } },
      { 0x2F8D0, { 0x03B08 } },
      { 0x2F8D1, { 0x03AE4 } },
      { 0x2F8D2, { 0x05192 } },
      { 0x2F8D3, { 0x05195 } },
      { 0x2F8D4, { 0x06700 } },
      { 0x2F8D5, { 0x0669C } },
      { 0x2F8D6, { 0x080AD } },
      { 0x2F8D7, { 0x043D9 } },
      { 0x2F8D8, { 0x06717 } },
      { 0x2F8D9, { 0x0671B } },
      { 0x2F8DA, { 0x06721 } },
      { 0x2F8DB, { 0x0675E } },
      { 0x2F8DC, { 0x06753 } },
      { 0x2F8DD, { 0x233C3 } },
      { 0x2F8DE, { 0x03B49 } },
      { 0x2F8DF, { 0x067FA } },
      { 0x2F8E0, { 0x06785 } },
      { 0x2F8E1, { 0x06852 } },
      { 0x2F8E2, { 0x06885 } },
      { 0x2F8E3, { 0x2346D } },
      { 0x2F8E4, { 0x0688E } },
      { 0x2F8E5, { 0x0681F } },
      { 0x2F8E6, { 0x06914 } },
      { 0x2F8E7, { 0x03B9D } },
      { 0x2F8E8, { 0x06942 } },
      { 0x2F8E9, { 0x069A3 } },
      { 0x2F8EA, { 0x069EA } },
      { 0x2F8EB, { 0x06AA8 } },
      { 0x2F8EC, { 0x236A3 } },
      { 0x2F8ED, { 0x06ADB } },
      { 0x2F8EE, { 0x03C18 } },
      { 0x2F8EF, { 0x06B21 } },
      { 0x2F8F0, { 0x238A7 } },
      { 0x2F8F1, { 0x06B54 } },
      { 0x2F8F2, { 0x03C4E } },
      { 0x2F8F3, { 0x06B72 } },
      { 0x2F8F4, { 0x06B9F } },
      { 0x2F8F5, { 0x06BBA } },
      { 0x2F8F6, { 0x06BBB } },
      { 0x2F8F7, { 0x23A8D } },
      { 0x2F8F8, { 0x21D0B } },
      { 0x2F8F9, { 0x23AFA } },
      { 0x2F8FA, { 0x06C4E } },
      { 0x2F8FB, { 0x23CBC } },
      { 0x2F8FC, { 0x06CBF } },
      { 0x2F8FD, { 0x06CCD } },
      { 0x2F8FE, { 0x06C67 } },
      { 0x2F8FF, { 0x06D16 } },
      { 0x2F900, { 0x06D3E } },
      { 0x2F901, { 0x06D77 } },
      { 0x2F902, { 0x06D41 } },
      { 0x2F903, { 0x06D69 } },
      { 0x2F904, { 0x06D78 } },
      { 0x2F905, { 0x06D85 } },
      { 0x2F906, { 0x23D1E } },
      { 0x2F907, { 0x06D34 } },
      { 0x2F908, { 0x06E2F } },
      { 0x2F909, { 0x06E6E } },
      { 0x2F90A, { 0x03D33 } },
      { 0x2F90B, { 0x06ECB } },
      { 0x2F90C, { 0x06EC7 } },
      { 0x2F90D, { 0x23ED1 } },
      { 0x2F90E, { 0x06DF9 } },
      { 0x2F90F, { 0x06F6E } },
      { 0x2F910, { 0x23F5E } },
      { 0x2F911, { 0x23F8E } },
      { 0x2F912, { 0x06FC6 } },
      { 0x2F913, { 0x07039 } },
      { 0x2F914, { 0x0701E } },
      { 0x2F915, { 0x0701B } },
      { 0x2F916, { 0x03D96 } },
      { 0x2F917, { 0x0704A } },
      { 0x2F918, { 0x0707D } },
      { 0x2F919, { 0x07077 } },
      { 0x2F91A, { 0x070AD } },
      { 0x2F91B, { 0x20525 } },
      { 0x2F91C, { 0x07145 } },
      { 0x2F91D, { 0x24263 } },
      { 0x2F91E, { 0x0719C } },
      { 0x2F91F, { 0x243AB } },
      { 0x2F920, { 0x07228 } },
      { 0x2F921, { 0x07235 } },
      { 0x2F922, { 0x07250 } },
      { 0x2F923, { 0x24608 } },
      { 0x2F924, { 0x07280 } },
      { 0x2F925, { 0x07295 } },
      { 0x2F926, { 0x24735 } },
      { 0x2F927, { 0x24814 } },
      { 0x2F928, { 0x0737A } },
      { 0x2F929, { 0x0738B } },
      { 0x2F92A, { 0x03EAC } },
      { 0x2F92B, { 0x073A5 } },
      { 0x2F92C, { 0x03EB8 } },
      { 0x2F92D, { 0x03EB8 } },
      { 0x2F92E, { 0x07447 } },
      { 0x2F92F, { 0x0745C } },
      { 0x2F930, { 0x07471 } },
      { 0x2F931, { 0x07485 } },
      { 0x2F932, { 0x074CA } },
      { 0x2F933, { 0x03F1B } },
      { 0x2F934, { 0x07524 } },
      { 0x2F935, { 0x24C36 } },
      { 0x2F936, { 0x0753E } },
      { 0x2F937, { 0x24C92 } },
      { 0x2F938, { 0x07570 } },
      { 0x2F939, { 0x2219F } },
      { 0x2F93A, { 0x07610 } },
      { 0x2F93B, { 0x24FA1 } },
      { 0x2F93C, { 0x24FB8 } },
      { 0x2F93D, { 0x25044 } },
      { 0x2F93E, { 0x03FFC } },
      { 0x2F93F, { 0x04008 } },
      { 0x2F940, { 0x076F4 } },
      { 0x2F941, { 0x250F3 } },
      { 0x2F942, { 0x250F2 } },
      { 0x2F943, { 0x25119 } },
      { 0x2F944, { 0x25133 } },
      { 0x2F945, { 0x0771E } },
      { 0x2F946, { 0x0771F } },
      { 0x2F947, { 0x0771F } },
      { 0x2F948, { 0x0774A } },
      { 0x2F949, { 0x04039 } },
      { 0x2F94A, { 0x0778B } },
      { 0x2F94B, { 0x04046 } },
      { 0x2F94C, { 0x04096 } },
      { 0x2F94D, { 0x2541D } },
      { 0x2F94E, { 0x0784E } },
      { 0x2F94F, { 0x0788C } },
      { 0x2F950, { 0x078CC } },
      { 0x2F951, { 0x040E3 } },
      { 0x2F952, { 0x25626 } },
      { 0x2F953, { 0x07956 } },
      { 0x2F954, { 0x2569A } },
      { 0x2F955, { 0x256C5 } },
      { 0x2F956, { 0x0798F } },
      { 0x2F957, { 0x079EB } },
      { 0x2F958, { 0x0412F } },
      { 0x2F959, { 0x07A40 } },
      { 0x2F95A, { 0x07A4A } },
      { 0x2F95B, { 0x07A4F } },
      { 0x2F95C, { 0x2597C } },
      { 0x2F95D, { 0x25AA7 } },
      { 0x2F95E, { 0x25AA7 } },
      { 0x2F95F, { 0x07AEE } },
      { 0x2F960, { 0x04202 } },
      { 0x2F961, { 0x25BAB } },
      { 0x2F962, { 0x07BC6 } },
      { 0x2F963, { 0x07BC9 } },
      { 0x2F964, { 0x04227 } },
      { 0x2F965, { 0x25C80 } },
      { 0x2F966, { 0x07CD2 } },
      { 0x2F967, { 0x042A0 } },
      { 0x2F968, { 0x07CE8 } },
      { 0x2F969, { 0x07CE3 } },
      { 0x2F96A, { 0x07D00 } },
      { 0x2F96B, { 0x25F86 } },
      { 0x2F96C, { 0x07D63 } },
      { 0x2F96D, { 0x04301 } },
      { 0x2F96E, { 0x07DC7 } },
      { 0x2F96F, { 0x07E02 } },
      { 0x2F970, { 0x07E45 } },
      { 0x2F971, { 0x04334 } },
      { 0x2F972, { 0x26228 } },
      { 0x2F973, { 0x26247 } },
      { 0x2F974, { 0x04359 } },
      { 0x2F975, { 0x262D9 } },
      { 0x2F976, { 0x07F7A } },
      { 0x2F977, { 0x2633E } },
      { 0x2F978, { 0x07F95 } },
      { 0x2F979, { 0x07FFA } },
      { 0x2F97A, { 0x08005 } },
      { 0x2F97B, { 0x264DA } },
      { 0x2F97C, { 0x26523 } },
      { 0x2F97D, { 0x08060 } },
      { 0x2F97E, { 0x265A8 } },
      { 0x2F97F, { 0x08070 } },
      { 0x2F980, { 0x2335F } },
      { 0x2F981, { 0x043D5 } },
      { 0x2F982, { 0x080B2 } },
      { 0x2F983, { 0x08103 } },
      { 0x2F984, { 0x0440B } },
      { 0x2F985, { 0x0813E } },
      { 0x2F986, { 0x05AB5 } },
      { 0x2F987, { 0x267A7 } },
      { 0x2F988, { 0x267B5 } },
      { 0x2F989, { 0x23393 } },
      { 0x2F98A, { 0x2339C } },
      { 0x2F98B, { 0x08201 } },
      { 0x2F98C, { 0x08204 } },
      { 0x2F98D, { 0x08F9E } },
      { 0x2F98E, { 0x0446B } },
      { 0x2F98F, { 0x08291 } },
      { 0x2F990, { 0x0828B } },
      { 0x2F991, { 0x0829D } },
      { 0x2F992, { 0x052B3 } },
      { 0x2F993, { 0x082B1 } },
      { 0x2F994, { 0x082B3 } },
      { 0x2F995, { 0x082BD } },
      { 0x2F996, { 0x082E6 } },
      { 0x2F997, { 0x26B3C } },
      { 0x2F998, { 0x082E5 } },
      { 0x2F999, { 0x0831D } },
      { 0x2F99A, { 0x08363 } },
      { 0x2F99B, { 0x083AD } },
      { 0x2F99C, { 0x08323 } },
      { 0x2F99D, { 0x083BD } },
      { 0x2F99E, { 0x083E7 } },
      { 0x2F99F, { 0x08457 } },
      { 0x2F9A0, { 0x08353 } },
      { 0x2F9A1, { 0x083CA } },
      { 0x2F9A2, { 0x083CC } },
      { 0x2F9A3, { 0x083DC } },
      { 0x2F9A4, { 0x26C36 } },
      { 0x2F9A5, { 0x26D6B } },
      { 0x2F9A6, { 0x26CD5 } },
      { 0x2F9A7, { 0x0452B } },
      { 0x2F9A8, { 0x084F1 } },
      { 0x2F9A9, { 0x084F3 } },
      { 0x2F9AA, { 0x08516 } },
      { 0x2F9AB, { 0x273CA } },
      { 0x2F9AC, { 0x08564 } },
      { 0x2F9AD, { 0x26F2C } },
      { 0x2F9AE, { 0x0455D } },
      { 0x2F9AF, { 0x04561 } },
      { 0x2F9B0, { 0x26FB1 } },
      { 0x2F9B1, { 0x270D2 } },
      { 0x2F9B2, { 0x0456B } },
      { 0x2F9B3, { 0x08650 } },
      { 0x2F9B4, { 0x0865C } },
      { 0x2F9B5, { 0x08667 } },
      { 0x2F9B6, { 0x08669 } },
      { 0x2F9B7, { 0x086A9 } },
      { 0x2F9B8, { 0x08688 } },
      { 0x2F9B9, { 0x0870E } },
      { 0x2F9BA, { 0x086E2 } },
      { 0x2F9BB, { 0x08779 } },
      { 0x2F9BC, { 0x08728 } },
      { 0x2F9BD, { 0x0876B } },
      { 0x2F9BE, { 0x08786 } },
      { 0x2F9BF, { 0x045D7 } },
      { 0x2F9C0, { 0x087E1 } },
      { 0x2F9C1, { 0x08801 } },
      { 0x2F9C2, { 0x045F9 } },
      { 0x2F9C3, { 0x08860 } },
      { 0x2F9C4, { 0x08863 } },
      { 0x2F9C5, { 0x27667 } },
      { 0x2F9C6, { 0x088D7 } },
      { 0x2F9C7, { 0x088DE } },
      { 0x2F9C8, { 0x04635 } },
      { 0x2F9C9, { 0x088FA } },
      { 0x2F9CA, { 0x034BB } },
      { 0x2F9CB, { 0x278AE } },
      { 0x2F9CC, { 0x27966 } },
      { 0x2F9CD, { 0x046BE } },
      { 0x2F9CE, { 0x046C7 } },
      { 0x2F9CF, { 0x08AA0 } },
      { 0x2F9D0, { 0x08AED } },
      { 0x2F9D1, { 0x08B8A } },
      { 0x2F9D2, { 0x08C55 } },
      { 0x2F9D3, { 0x27CA8 } },
      { 0x2F9D4, { 0x08CAB } },
      { 0x2F9D5, { 0x08CC1 } },
      { 0x2F9D6, { 0x08D1B } },
      { 0x2F9D7, { 0x08D77 } },
      { 0x2F9D8, { 0x27F2F } },
      { 0x2F9D9, { 0x20804 } },
      { 0x2F9DA, { 0x08DCB } },
      { 0x2F9DB, { 0x08DBC } },
      { 0x2F9DC, { 0x08DF0 } },
      { 0x2F9DD, { 0x208DE } },
      { 0x2F9DE, { 0x08ED4 } },
      { 0x2F9DF, { 0x08F38 } },
      { 0x2F9E0, { 0x285D2 } },
      { 0x2F9E1, { 0x285ED } },
      { 0x2F9E2, { 0x09094 } },
      { 0x2F9E3, { 0x090F1 } },
      { 0x2F9E4, { 0x09111 } },
      { 0x2F9E5, { 0x2872E } },
      { 0x2F9E6, { 0x0911B } },
      { 0x2F9E7, { 0x09238 } },
      { 0x2F9E8, { 0x092D7 } },
      { 0x2F9E9, { 0x092D8 } },
      { 0x2F9EA, { 0x0927C } },
      { 0x2F9EB, { 0x093F9 } },
      { 0x2F9EC, { 0x09415 } },
      { 0x2F9ED, { 0x28BFA } },
      { 0x2F9EE, { 0x0958B } },
      { 0x2F9EF, { 0x04995 } },
      { 0x2F9F0, { 0x095B7 } },
      { 0x2F9F1, { 0x28D77 } },
      { 0x2F9F2, { 0x049E6 } },
      { 0x2F9F3, { 0x096C3 } },
      { 0x2F9F4, { 0x05DB2 } },
      { 0x2F9F5, { 0x09723 } },
      { 0x2F9F6, { 0x29145 } },
      { 0x2F9F7, { 0x2921A } },
      { 0x2F9F8, { 0x04A6E } },
      { 0x2F9F9, { 0x04A76 } },
      { 0x2F9FA, { 0x097E0 } },
      { 0x2F9FB, { 0x2940A } },
      { 0x2F9FC, { 0x04AB2 } },
      { 0x2F9FD, { 0x29496 } },
      { 0x2F9FE, { 0x0980B } },
      { 0x2F9FF, { 0x0980B } },
      { 0x2FA00, { 0x09829 } },
      { 0x2FA01, { 0x295B6 } },
      { 0x2FA02, { 0x098E2 } },
      { 0x2FA03, { 0x04B33 } },
      { 0x2FA04, { 0x09929 } },
      { 0x2FA05, { 0x099A7 } },
      { 0x2FA06, { 0x099C2 } },
      { 0x2FA07, { 0x099FE } },
      { 0x2FA08, { 0x04BCE } },
      { 0x2FA09, { 0x29B30 } },
      { 0x2FA0A, { 0x09B12 } },
      { 0x2FA0B, { 0x09C40 } },
      { 0x2FA0C, { 0x09CFD } },
      { 0x2FA0D, { 0x04CCE } },
      { 0x2FA0E, { 0x04CED } },
      { 0x2FA0F, { 0x09D67 } },
      { 0x2FA10, { 0x2A0CE } },
      { 0x2FA11, { 0x04CF8 } },
      { 0x2FA12, { 0x2A105 } },
      { 0x2FA13, { 0x2A20E } },
      { 0x2FA14, { 0x2A291 } },
      { 0x2FA15, { 0x09EBB } },
      { 0x2FA16, { 0x04D56 } },
      { 0x2FA17, { 0x09EF9 } },
      { 0x2FA18, { 0x09EFE } },
      { 0x2FA19, { 0x09F05 } },
      { 0x2FA1A, { 0x09F0F } },
      { 0x2FA1B, { 0x09F16 } },
      { 0x2FA1C, { 0x09F3B } },
      { 0x2FA1D, { 0x2A600 } },
  }
#endif /* VAR_INITIALIZERS */
;  /* confusable_map */

extern const sizeof_t num_confusable_characters
#if VAR_INITIALIZERS
  = sizeof(confusable_map) / sizeof(a_confusable_map_elem)
#endif /* VAR_INITIALIZERS */
;

EXTERN a_hash_value hash_id_representation(a_void_ptr key);

EXTERN a_boolean id_representations_match(a_void_ptr entry_ptr,
                                          a_void_ptr key_ptr);

#endif /* UNICODE_VULNERABILITY_DETECTION_SUPPORTED */

inline a_boolean is_type_returning_type_trait(a_token_kind token)
/*
Return TRUE if the specified token is a "type-returning type trait", e.g.,
tok_underlying_type.
*/
{
  a_boolean result;

  switch (token) {
    case tok_underlying_type:
    case tok_add_lvalue_reference:
    case tok_add_pointer:
    case tok_add_rvalue_reference:
    case tok_decay:
    case tok_make_signed:
    case tok_make_unsigned:
    case tok_remove_all_extents:
    case tok_remove_const:
    case tok_remove_cv:
    case tok_remove_cvref:
    case tok_remove_extent:
    case tok_remove_pointer:
    case tok_remove_reference:
    case tok_remove_reference_t:
    case tok_remove_restrict:
    case tok_remove_volatile:
      result = TRUE;
      break;
    default:
      result = FALSE;
      break;
  }  /* switch */
  return result;
}  /* is_type_returning_type_trait */


/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef LEXICAL_H */

