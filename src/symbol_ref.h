/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

symbol_ref.h - Declarations related to symbol reference processing.

*/

/* Avoid including these declarations more than once. */
#ifndef SYMBOL_REF_H
#define SYMBOL_REF_H 1

#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */
#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */
#ifndef TEMPLATES_H
#include "templates.h"
#endif /* ifndef TEMPLATES_H */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
A symbol-reference-set is a bit vector designed to describe the declarations
and uses of symbols.  The bit positions are specified by the SRK_ values
defined below.  The bit vector is used in generating cross-reference
information and in tracking use-def status of variables.  Note that "use" and
"modification" apply only to objects -- i.e., to variables and static and
nonstatic data members -- and that "address taken" applies only to objects
and functions.  A given reference may be described by the union of several
bits.  For example, every declaration has the SRK_DECLARATION bit set; those
which are also definitions have the SRK_DEFINITION bit set, too.  Similarly,
every reference has SRK_REFERENCE set, but most references will have one or
more additional bits set as well; for example, an increment expression like
"++x" would have SRK_USE and SRK_MODIFICATION set (as well as SRK_REFERENCE).

It is intended that implementations that need to track reference information
in more detail would be able to define (and maintain) additional bits in the
bit vector.  For example, bits could be defined to describe the specific ways
in which an address can be taken (e.g., to discriminate between taking the
address of a const and taking the address of a nonconst object).
*/
#define SRK_NONE ((a_symbol_reference_kind)0x0)
			/*lint -esym(755,SRK_NONE)*/
#define SRK_DECLARATION ((a_symbol_reference_kind)0x1)
			/* Any declaration. */
#define SRK_DEFINITION ((a_symbol_reference_kind)0x2)
			/* A declaration that is also definition. */
#define SRK_REFERENCE ((a_symbol_reference_kind)0x4)
			/* Any kind of reference.  Most commonly a reference
			   will be a use, a modification, an "address-taken",
			   or a reference in an error context (see following
			   bit positions).  If it is none of those, the
			   reference bit may still be set -- e.g., for a
			   reference to a class or typedef name in a
			   declaration, to a label in a goto statement, to a
			   routine name in a call, to a variable in a sizeof
			   operation, etc.). */
#define SRK_USE ((a_symbol_reference_kind)0x8)
			/* A use of the value of an object.  Both the use and
			   modification bits may be set for a given reference
			   (e.g., an increment). */
#define SRK_MODIFICATION ((a_symbol_reference_kind)0x10)
			/* A reference that changes the value of an object.
			   Both the use and modification bits may be set for a
			   given reference (e.g., an increment). */
#define SRK_ADDRESS_TAKEN ((a_symbol_reference_kind)0x20)
			/* A reference in which the address of an object or
			   function is taken. */
#define SRK_ERROR ((a_symbol_reference_kind)0x40)
			/* A reference of some sort, but because of an error
			   in the source the kind of reference is uncertain;
			   such a reference is treated both as a use and as a
			   modification, in order to suppress use/def
			   diagnostics. */
#define SRK_IMPLICIT ((a_symbol_reference_kind)0x80)
			/* A reference or declaration is implicit. */
#define SRK_FRIEND ((a_symbol_reference_kind)0x100)
			/* Or'ed with SRK_DECLARATION, a friend declaration. */
#define SRK_TENTATIVE_DEF ((a_symbol_reference_kind)0x200)
			/* Or'ed with SRK_DEFINITION, a variable declaration
			   is a tentative definition (C only). */
#define SRK_IMPLICIT_TEMPLATE_ARG ((a_symbol_reference_kind)0x400)
			/* Within a template instantiation, an implicit
			   reference to a name involved in a template argument
			   by means of an explicit reference to a template
			   parameter. */
#define SRK_INITIALIZATION ((a_symbol_reference_kind)0x800)
			/* Or'ed with SRK_DEFINITION to indicate an explicit
			   or implicit variable initialization.  In addition,
			   may be or'ed with SRK_REFERENCE to indicate an
			   explicit reference in a mem-initializer list. */
#define SRK_CONST_ADDRESS_TAKEN ((a_symbol_reference_kind)0x1000)
			/* Or'ed with SRK_ADDRESS_TAKEN to indicate an
			   address taken in a way that can't modify the object
			   without casting away constness. */
#define SRK_PROTO_INST_REF ((a_symbol_reference_kind)0x2000)
			/* A reference in a prototype instantiation, in
			   a context where we can't tell what kind of use
			   was made. */
#define SRK_DEFAULT_ARG_EXPR ((a_symbol_reference_kind)0x4000)
			/* A reference in a default argument expression. */
#define SRK_TEMPLATE_INSTANTIATION ((a_symbol_reference_kind)0x8000)
			/* A (full or partial) template instantiation. */
#define SRK_CONST_VALUE_USE ((a_symbol_reference_kind)0x10000)
			/* Or'd with SRK_USE, indicates a case where the
			   value of a const-valued variable is used, but the
			   variable itself is not "used" according to the
			   C++ standard definition, because the value is
			   never fetched from the variable in memory. */
#define SRK_ALL_REFERENCES \
  (SRK_USE | SRK_MODIFICATION | SRK_ADDRESS_TAKEN | SRK_ERROR | \
   SRK_CONST_ADDRESS_TAKEN | SRK_PROTO_INST_REF | SRK_CONST_VALUE_USE)
			/* All types of references.  Used to mask off those
			   bits. */
#define SRK_ALL_VARIABLE_USES \
  (SRK_USE | SRK_ADDRESS_TAKEN | SRK_PROTO_INST_REF | SRK_CONST_VALUE_USE)
			/* All reference kinds that constitute "use" of a
			   variable's value in one way or another. */
#define SRK_ALL_VARIABLE_MODIFICATIONS \
  (SRK_MODIFICATION | SRK_ADDRESS_TAKEN | SRK_PROTO_INST_REF)
			/* All reference kinds that constitute "modification"
			   of a variable's value in one way or another. */

extern void db_symbol_ref_kind(a_symbol_reference_kind  kind);

/* Record use information (for cross-reference, etc.). */
extern void record_symbol_declaration(
                            a_symbol_reference_kind      srk_flags,
                            a_symbol_ptr                 sym_ptr,
                            a_source_position            *source_position,
                            a_source_sequence_entry_ptr  ssep);

extern a_boolean check_use_of_deleted_function(a_symbol_ptr      rout_sym,
                                               a_boolean         elided_ref,
                                               a_source_position *pos);

extern
void record_symbol_reference_full(a_symbol_reference_kind kind,
                                  a_symbol_ptr            sym_ptr,
                                  a_source_position       *source_position,
                                  a_boolean               update_il_entry,
                                  a_source_correspondence *specific_il_entry);

extern void record_symbol_reference(a_symbol_reference_kind  kind,
                                    a_symbol_ptr             sym_ptr,
                                    a_source_position        *source_position,
                                    a_boolean                update_il_entry);

#define mark_defined(sym, pos)                                          \
  record_symbol_declaration(SRK_DECLARATION | SRK_DEFINITION, (sym),    \
                            (pos), (a_source_sequence_entry_ptr)NULL)
#define mark_declared(sym, pos)                                         \
  record_symbol_declaration(SRK_DECLARATION, (sym), (pos),              \
                            (a_source_sequence_entry_ptr)NULL)

#define mark_referenced(sym, err_pos)                                   \
  record_symbol_reference(SRK_REFERENCE, (sym), (err_pos),              \
                          /*update_il_entry=*/TRUE)

extern void reference_to_invalid_name(a_symbol_locator *locator);

extern void record_param_id_list_declarations(a_func_info_block_ptr func_info);

extern void record_using_decl(a_symbol_ptr       sym,
                              a_source_position  *pos,
                              a_using_decl_ptr   udp,
                              a_using_decl_ptr   prev_udp);

extern void mark_variable_value_set(a_symbol_ptr  sym);

extern void mark_expansion_variable_used(a_variable_ptr  variable);

extern void reference_to_implicitly_invoked_function
                                    (a_symbol_ptr       sym,
                                     a_source_position  *pos,
                                     a_type_ptr         class_of_object,
                                     a_boolean          honor_virtual,
                                     a_boolean          evaluated,
                                     a_boolean          instantiate,
                                     a_boolean          check_access,
                                     a_boolean          elided_reference,
                                     a_boolean          *error_detected);

extern a_boolean reference_to_trivial_default_constructor(
                                           a_type_ptr         class_type,
                                           a_type_ptr         access_class,
                                           a_source_position  *pos,
                                           a_boolean          check_access,
                                           a_boolean          *error_detected);

extern
void reference_to_trivial_copy_constructor(a_type_ptr        class_type,
                                           a_type_ptr        access_class,
                                           a_source_position *pos,
                                           a_boolean         check_access,
                                           a_boolean         elided_reference,
                                           a_boolean         *error_detected);

void check_use_of_deprecated_or_unavailable_entity(
                                            a_source_correspondence_ptr  scp,
                                            a_source_position            *pos);

#if RECORD_HIDDEN_NAMES_IN_IL
extern void check_name_hiding_for_scope(a_scope_ptr  sp);
extern void check_name_hiding_by_parameter(a_symbol_locator *param_locator);
#endif /* RECORD_HIDDEN_NAMES_IN_IL */

extern void symbol_ref_one_time_init(void);

/* This macro is just a stub.  It can be replaced in implementations that
   need to track uses that require a complete class type.  (Note: the type
   pointer tp that is passed in need not be a class type.) */
#define record_complete_class_type_needed(tp)  /* Nothing */

inline void complete_class_type_is_needed(a_type_ptr  tp,
                                          a_boolean   *p_subst_error = NULL)
/*
tp is a class type.  This is a context in which a type is required to be
complete, so if tp is incomplete see if it is a template class that can be
instantiated.  Issuing a diagnostic on an incomplete type is done separately.
Also (if appropriate for the implementation) record that the class was
required to be complete in the current context.  If p_subst_error is non-NULL
and an error occurs during partial specialization selection, *p_subst_error is
set to TRUE.
*/
{
  if (!C_mode()) {
    instantiate_template_class(tp, p_subst_error);
  }  /* if */
  record_complete_class_type_needed(tp);
}  /* complete_class_type_is_needed */

inline void complete_type_is_needed(a_type_ptr  tp)
/*
This is a context in which a type is required to be complete, so if tp
is incomplete see if it is a template class that can be instantiated (or
array thereof).  Issuing a diagnostic on an incomplete type is done
separately.  Also (if appropriate for the implementation) record that the
class was required to be complete in the current context.
*/
{
  if (C_dialect == C_dialect_cplusplus && is_incomplete_type(tp)) {
    check_for_uninstantiated_template_class(tp);
  }  /* if */
  record_complete_class_type_needed(tp);
}  /* complete_type_is_needed */

/*
If tp is a real instance of a class template, make sure it is fully
instantiated.
*/
#define complete_template_instance_is_needed(tp)			\
{									\
  a_type_ptr	cti_type;						\
  cti_type = skip_typerefs(tp);						\
  if (is_unspecialized_template_class(cti_type) &&			\
      !cti_type->variant.class_struct_union.is_nonreal_class &&		\
      !is_cli_generic_instance_type(cti_type)) {			\
    complete_class_type_is_needed(cti_type);				\
  }  /* if */								\
}  /* complete_template_instance_is_needed */

/* If the variable is a variable template instance or template static
   data member with an incomplete array size, attempt an instantiation
   of the variable so that the size of the variable can be known. */
#define complete_variable_type_is_needed(vp)				\
{									\
  if (vp->is_template_variable &&					\
      is_incomplete_array_type(vp->type)) {				\
    complete_template_variable_type_is_needed(vp);		\
  }  /* if */								\
}  /* complete_variable_type_is_needed */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef SYMBOL_REF_H */

