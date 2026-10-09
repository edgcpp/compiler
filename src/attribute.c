/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

attribute.c -- Processing of attributes.


How to Add a New Attribute
==========================
The attribute framework is designed to make it relatively easy to add a new
attribute.  Typically, adding an attribute involves just a few steps:

  (1) Add a new ak_... attribute kind constant in il_def.h.
      For example: ak_section.

  (2) Add a new row to the table known_attr_table[] below.  For example:
        { "section", "(sn)", "gx", ak_section },
      Such a row describes the source form of the attribute ("section"),
      what kind of arguments (if any) the attribute takes ("(sn)" means that
      a normal string literal is expected), and which mode the attribute
      should be accepted in ("gx" means that all GNU C and C++ modes should
      accept the attribute).
      See the definition of struct an_attr_descr for details.

  (3) Add a new row to the table known_attr_appl_table[] below.  For example:
        { ak_section, "r|v:-a!", apply_section_attr },
      Such a row describes simple constraints for the entity to which the
      attribute is applied ("r|v:-a!" means that the entity must be a routine
      or a non-automatic variable), and which function to call to "apply" the
      attribute (apply_section_attr in this case) which may check additional
      constraints and/or update the IL entry for the entity.  (If no function
      should be called, specify NO_APPL_FN instead.)
      See the definition of struct an_attr_appl_descr for details.

  (4) Write the application function specified in step 3 (if any).

  (5) If needed, update the table attr_corresp_table[] below to customize
      the rules for handling corresponding attributes across translation
      units (by default corresponding attributes must match exactly; see
      the definition of an_attr_corresp_descr for details).

  (6) Update the switch statement in disp_attribute (in il_display.c).

Even when none of these steps is taken, an unrecognized attribute will still
automatically be recorded in the IL when record_unrecognized_attributes is
TRUE (when FALSE, a warning is issued for unrecognized attributes). 

For attributes that are only of interest to a back end, it is conceivable that
no application function is required or that the application function does not
update the target IL entry because the an_attribute entry that is automatically
recorded is sufficient for the back end (ak_carries_dependency is currently an
example of the latter).

It is recommended that attributes not produce an entirely new IL entry, but
instead just modify the entry they apply to "in place".  Some GNU attributes
(mode and vector_size) do however produce a new a_type entry when applied to
another a_type entry.  For that reason, the application function type is:
    typedef char* an_attr_application_fn(an_attribute_ptr  ap,
                                         char              *entity,
                                         an_il_entry_kind  entity_kind);
The returned value is the entity resulting from applying the given attribute
to the given entity (in most cases the returned entity is the given entity,
since attributes usually do not create new entries).
*/

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Header files used by files involved in declaration processing. */
#include "decl_hdrs.h"

/* Other required header files. */
#include "disambig.h"
#include "expr.h"
#include "folding.h"
#include "layout.h"
#include "statements.h"

#if GNU_EXTENSIONS_ALLOWED
#include "il_walk.h"
#include "layout.h"
#include "sys_predef.h"
#endif /* GNU_EXTENSIONS_ALLOWED */

#include "templates.h"

#if MICROSOFT_EXTENSIONS_ALLOWED
#define assert_not_handle_or_tracking_reference(tp)                         \
  check_assertion(!cli_or_cx_enabled ||                                     \
                  (!is_handle_type(tp) && !is_tracking_reference_type(tp)))
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define assert_not_handle_or_tracking_reference(tp)  /* Nothing */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#define MAX_ATTRIBUTE_NAME_LENGTH 100

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

typedef struct an_attr_descr const *an_attr_descr_ptr;
typedef struct an_attr_descr {
  /* Data structure describing the name and kind of an attribute, the form
     of its arguments if any (i.e., its "signature"), and the modes in which
     that attribute should be recognized.  This defines the structure of the
     table known_attr_table (below) of recognizable attributes. */
  a_const_char	*name;
			/* The name of the attribute.  If an attribute name
			   can include optional leading/trailing underscores,
			   those underscores are not included here.  The name
			   may not be longer than MAX_ATTRIBUTE_NAME_LENGTH. */
  a_const_char	*sig;
			/* A compact encoding of the "signature" of this
			   attribute.  If sig is "", no attribute arguments
			   are permitted.  If sig starts with "?", arguments
			   are optional.  The arguments are described by a
			   parenthesized comma-separated list of codes (no
			   spaces are permitted):
			     "t": a type-id is expected
			     "ci": an integer constant is expected
			     "ct": an integer constant or a type is expected
			           (similar to a "sizeof(...)" argument)
			     "n": an identifier is expected
			     "sn": a narrow string literal is expected
			     "sx": a string literal is expected (wide/narrow)
			     "X": an unevaluated expression
			     "Xc": an unevaluated conditional-expression
			     "Xe": an evaluated expression
			     "Xce": an evaluated conditional-expression
			     "*": an arbitrary set of tokens is expected
			          (this can only be for the last argument)
			   An expression code can be followed by a "b" to
			   indicate that the expression is a predicate and is
			   therefore contextually converted to bool (e.g.,
			   "Xceb" for the operand of "[[assume(...)]]").
			   A code can be followed by a "+" to indicate that
			   one or more arguments of that kind are expected.
			   A "?" indicates that the argument list may
                           terminate at that point.
			   Examples:
			     "(ci)": one integer constant required
			     "(?sn+)": one or more string literals are
			         optional, but the enclosing parentheses are
			         required (i.e., attr() or attr("str", "ing")
			         are okay, but just attr is not).
			     "(n?,t,ci)": an identifier is required; it can
			         optionally be followed by a type and an
			         integer constant (both or neither).
			     "?(n?,t?,ci)": either no argument list appears at
			         all, or an identifier appears, optionally
			         followed by a type, itself optionally followed
			         by an integer constant.
			*/
  a_const_char	*cond;
			/* A compact encoding of the condition in which this
			   attribute is accepted.  The encoding consists of an
			   optional prefix (see below) followed by a condition
			   string cstr.  cstr[0] indicates the attribute
			   family: 'c' for [[...]] (standard C++11 or C23),
			   'g' for __attribute((...)) in GNU modes, 's' for
			   __attribute((...)) in Sun mode, 'l' for
			   __attribute((...)) in Clang mode, and 'm' for
			   __declspec(...) in Microsoft mode.  cstr[1] is
			   '+' if the attribute only applies in C++ modes,
			   'c' if it only applies in C mode, and 'x' if it
			   applies in both C and C++ modes (some combinations
			   are impossible; e.g. "sc" is meaningless since there
			   is no "Sun C" mode).  The first two characters can
			   be followed by a namespace name enclosed by "[]"
			   (meaning the namespace name is required) or "{}"
			   (the namespace name is optional).  E.g., if name is
			   "test" and cstr is "c+[xyz]", then this is a
			   description entry for [[xyz::test ... ]].  If
			   cstr[0] is 'g', 'l' or 'm', the first two characters
			   can be followed by a parenthesized range of
			   applicable versions.  E.g., "gx(30100-39999)" means
			   the attribute is valid in GNU C/C++ modes with
			   gnu_version >= 30100 and gnu_version < 40000.
			   Either end of the range can be dropped; e.g.,
			   "mc(1400-)" means the attribute is valid in
			   Microsoft C mode with microsoft_version >= 1400.
			   For standard-notation attributes, a range of values
			   for std_version can also be provided (it should
			   appear after the bracketed namespace name, if any).

			   Auxiliary version specification(s) can also be
			   specified by appending "|", one of "C", "G", "M", or
			   "S" (for clang_version, gnu_version,
			   microsoft_version, and std_version respectively) and
			   an applicable version range.  The auxiliary version
			   specifier doesn't affect the attribute family.  For
			   example, "c+(201703-|M(1910-))" specifies a
			   standard attribute that is available when
			   std_version >= 201703 or when microsoft_version >=
			   1910.

			   If the auxiliary versions should be used in place of
			   the default range of applicable versions, the
			   applicable versions can be omitted.  For example,
			   "c[msvc](|M(1910-))" specifies a standard-notation
			   attribute in the "msvc" namespace that's only
			   available when microsoft_version >= 1910.

			   A prefix "1" means the attribute can appear at most
			   once per attribute group.  E.g., "1c+" indicates a
			   standard C++ attribute that can appear at most once
			   in a attribute group (and with no namespace).  [This
			   is still enabled even though C++23 and C23 now allow
			   duplicate attributes so it's not currently used.] */
  an_attribute_kind
		attr_kind;
			/* The attribute kind to record in the corresponding
			   attribute entry. */
} an_attr_descr;


/*
Table of recognizable attributes.  See the description of an_attr_descr (above)
for the meaning and format of each field.  Every attribute form recognized by
the front end should have a distinct entry in this table (e.g., the standard
attribute [[noreturn]] and the GNU attribute __attribute((noreturn)) have
separate entries).
See also the complementary table known_attr_appl_table below.
*/
static constexpr an_attr_descr known_attr_table[] = {
  /* C++ standard attributes (C++11 and later).  Note the use of "c+" to
     indicate these are valid in C++ modes only. */
  { "align", "(ct)", "c+", ak_align },
  { "assume", "(Xceb)", "c+(202302-|G(130000-)|C(190000-))", ak_assume },
  { "base_check", "", "c+", ak_base_check },
  { "carries_dependency", "", "c+", ak_carries_dependency },
  { "deprecated", "?(sx)", "c+(201402-|M(1910-))", ak_deprecated },
  { "final", "", "c+", ak_final },
  { "hiding", "", "c+", ak_hiding },
  { "known_semantics", "", "c+[msvc](|M(1927-))", ak_known_semantics },
  { "noreturn", "", "c+", ak_noreturn },
  { "override", "", "c+", ak_override },
  { "nodiscard", "?(sx)", "c+(201703-|M(1910-))", ak_nodiscard },
  { "noop_dtor", "", "c+[msvc](|M(1928-))", ak_noop_dtor },
  { "maybe_unused", "", "c+(201703-|M(1910-)|G(70100-)|C(30900-))",
    ak_maybe_unused },
  { "fallthrough", "", "c+(201703-|M(1910-))", ak_fallthrough },
  { "likely", "", "c+(202002-|G(80300-))", ak_likely },
  { "unlikely", "", "c+(202002-|G(80300-))", ak_unlikely },
  { "no_unique_address", "", "c+(202002-|G(80300-)|C(90000-))",
    ak_no_unique_address },
  { "indeterminate", "", "c+(202603-)", ak_indeterminate },

  /* C standard attributes (C23 and later).  Also accepted by default when
     gnu_version >= 100000, microsoft_version >= 1934, or clang_version >=
     170000 (see the setting of std_attributes_enabled). */
  { "deprecated", "?(sx)", "c", ak_deprecated },
  { "fallthrough", "", "c", ak_fallthrough },
  { "nodiscard", "?(sx)", "c", ak_nodiscard },
  { "noreturn", "", "c", ak_noreturn },
  { "_Noreturn", "", "c", ak_noreturn },
  { "maybe_unused", "", "c", ak_maybe_unused },

  /* Nonstandard attributes. */
  { "enable_if", "(X,sn)", "lx(30500-)", ak_enable_if },
  { "overloadable", "", "lx", ak_overloadable },
  { "pass_object_size", "(ci)", "lx", ak_pass_object_size },
  { "diagnose_if", "(X,sn,sn?,sn)", "lx", ak_diagnose_if },

#if GNU_EXTENSIONS_ALLOWED
  /* GNU Attributes. */
  { "alias", "(sn)", "gx", ak_alias },
  { "assume", "(Xcb)", "gx(130000-)", ak_assume },
  { "aligned", "?(ci)", "gx", ak_align },
  { "alloc_size", "(ci?,ci)", "gx(40200-)", ak_alloc_size },
  { "always_inline", "", "gx", ak_always_inline },
  { "artificial", "", "gx(40000-)", ak_artificial },
#if GNU_X86_ATTRIBUTES_ALLOWED
  { "cdecl", "", "gx", ak_cdecl },
#endif /* GNU_X86_ATTRIBUTES_ALLOWED */
  { "cleanup", "(n)", "gx", ak_cleanup },
  { "cold", "", "gx(40300-)", ak_cold },
  { "common", "", "gx", ak_common },
  { "const", "", "gx", ak_const },
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
  { "constructor", "?(ci)", "gx", ak_constructor },
#else /* !GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
  { "constructor", "", "gx", ak_constructor },
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
  { "deprecated", "?(sx)", "gx(30100-)", ak_deprecated },
  { "deprecated", "?(sx)", "lx[gnu](60000-)", ak_deprecated },
  { "deprecated", "?(sx+)", "lx", ak_deprecated },
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
  { "destructor", "?(ci)", "gx", ak_destructor },
#else /* !GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
  { "destructor", "", "gx", ak_destructor },
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
  { "error", "(sn)", "gx(40000-)", ak_error },
#if GNU_VECTOR_TYPES_ALLOWED
  { "ext_vector_type", "(ci)", "lx", ak_ext_vector_type },
#endif /* GNU_VECTOR_TYPES_ALLOWED */
  { "externally_visible", "", "gx(40000-)", ak_externally_visible },
  { "fallthrough", "", "gx(70000-)", ak_fallthrough },
  { "fallthrough", "", "lx{clang}(30900-)", ak_fallthrough },
  { "fallthrough", "", "lx{gnu}(30900-)", ak_fallthrough },
#if GNU_X86_ATTRIBUTES_ALLOWED
  { "fastcall", "", "gx(30400-)", ak_fastcall },
#endif /* GNU_X86_ATTRIBUTES_ALLOWED */
  { "flatten", "", "gx(40000-)", ak_flatten },
  { "format", "(n,ci,ci)", "gx", ak_format },
  { "format_arg", "(ci)", "gx", ak_format_arg },
  { "gnu_inline", "", "gx", ak_gnu_inline },
  { "hot", "", "gx(40300-)", ak_hot },
  { "ifunc", "(sn)", "gx", ak_ifunc },
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
  { "init_priority", "(ci)", "g+", ak_init_priority },
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
  { "internal_linkage", "", "lx{clang}(40000-)", ak_internal_linkage },
  { "malloc", "?(X?,ci)", "gx", ak_malloc },
  { "may_alias", "", "gx(30300-)", ak_may_alias },
  { "mode", "(n)", "gx", ak_mode },
  { "no_instrument_function", "", "gx", ak_no_instrument_function },
  { "no_check_memory_usage", "", "gx", ak_no_check_memory_usage },
#if GNU_NAKED_ATTRIBUTE_ALLOWED
  { "naked", "", "gx", ak_naked },
#endif /* GNU_NAKED_ATTRIBUTE_ALLOWED */
  { "nocommon", "", "gx", ak_nocommon },
  { "noinline", "", "gx", ak_noinline },
  { "nonnull", "?(?ci+)", "gx", ak_nonnull },
  { "noplt", "", "gx", ak_noplt },
  { "noreturn", "", "gx", ak_noreturn },
  { "noreturn", "", "lx{gnu}", ak_noreturn },
  { "nothrow", "", "gx", ak_nothrow },
  { "packed", "", "gx", ak_packed },
  { "pure", "", "gx", ak_pure },
  { "section", "(sn)", "gx", ak_section },
  { "sentinel", "?(ci)", "gx", ak_sentinel },
#if GNU_X86_ATTRIBUTES_ALLOWED
  { "stdcall", "", "gx", ak_stdcall },
#endif /* GNU_X86_ATTRIBUTES_ALLOWED */
  { "strong", "", "gx", ak_strong },
  { "target", "(*)", "gx(40400-)", ak_target },
#if THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
  { "tls_model", "(sn)", "gx(30300-)", ak_tls_model },
#endif /* THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED */
  { "transparent_union", "", "gc", ak_transparent_union },
  { "unavailable", "?(sn)", "gx(120000-)", ak_unavailable },
  { "unused", "", "gx", ak_unused },
  { "used", "", "gx", ak_used },
#if GNU_VECTOR_TYPES_ALLOWED
  { "vector_size", "(ci)", "gx", ak_vector_size },
  { "neon_vector_type", "(ci)", "lx", ak_neon_vector_type },
  { "neon_polyvector_type", "(ci)", "lx", ak_neon_polyvector_type },
#endif /* GNU_VECTOR_TYPES_ALLOWED */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  { "visibility", "(sn)", "gx", ak_visibility },
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
  { "volatile", "", "gx", ak_noreturn },
  { "warn_unused_result", "", "gx", ak_warn_unused_result },
  { "warning", "(sn)", "gx(40000-)", ak_warning },
  { "weak", "", "gx", ak_weak },
  { "weakref", "?(sn)", "gx(40100-)", ak_weakref },
  { "abi_tag", "?(sn+)", "gx(40800-)", ak_abi_tag },
  { "no_specializations", "?(sn)", "l+[clang](200000-)",ak_no_specializations},
#endif /* GNU_EXTENSIONS_ALLOWED */

#if MICROSOFT_EXTENSIONS_ALLOWED
  /* Microsoft __declspec attributes. */
  { "align", "(ci)", "mx", ak_align },
  { "allocate", "(sn)", "mx", ak_section },
  { "appdomain", "", "m+", ak_appdomain },
  { "assembly_info", "(ci,ci)", "mx", ak_assembly_info },
  { "deprecated", "?(sx)", "mx", ak_deprecated },
  { "dllexport", "", "mx", ak_dllexport },
  { "dllimport", "", "mx", ak_dllimport },
  { "__edg_interior_ptr_alias", "", "m+", ak_edg_interior_ptr_alias },
  { "__edg_pin_ptr_alias", "", "m+", ak_edg_pin_ptr_alias },
  { "empty_bases", "", "m+", ak_empty_bases },
  { "guard", "(n)", "m+", ak_guard },
  { "hybrid_patchable", "", "mx", ak_hybrid_patchable },
  { "implementation_key", "(ci)", "mx", ak_implementation_key },
  { "intrin_type", "", "mx", ak_intrin_type },
  { "jitintrinsic", "", "m+", ak_jitintrinsic },
  { "naked", "", "mx", ak_naked },
  { "no_init_all", "", "mx", ak_no_init_all },
  { "noalias", "", "mx(1400-)", ak_noalias },
  { "noinline", "", "mx", ak_noinline },
  { "non_user_code", "", "m+", ak_non_user_code },
  { "noreturn", "", "mx", ak_noreturn },
  { "nothrow", "", "m+", ak_nothrow },
  { "novtable", "", "m+", ak_novtable },
  { "process", "", "m+", ak_process },
  { "property", "(*)", "m+", ak_property },
  { "pure", "", "mx", ak_pure },
  { "restrict", "", "mx(1400-)", ak_restrict },
  { "safebuffers", "", "mx", ak_safebuffers },
  { "selectany", "", "mx", ak_selectany },
  { "spectre", "(*)", "mx", ak_spectre },
#if THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
  { "thread", "", "mx", ak_thread },
#endif /* THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED */
  { "uuid", "(sn)", "m+", ak_uuid },
  { "layout_as_external", "", "m+", ak_layout_as_external },
  { "no_empty_identity_interface", "", "m+", ak_no_empty_identity_interface },
  { "no_ftm", "", "m+", ak_no_ftm },
  { "no_refcount", "", "m+", ak_no_refcount },
  { "no_release_return", "", "m+", ak_no_release_return },
  { "no_weakreferencesource", "", "m+", ak_no_weakreferencesource },
  { "one_phase_constructed", "", "m+", ak_one_phase_constructed },
  { "allocator", "", "mx(1900-)", ak_allocator },
  { "no_sanitize_address", "", "mx(1928-)", ak_no_sanitize_address },
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if SUN_EXTENSIONS_ALLOWED && GNU_EXTENSIONS_ALLOWED
  /* Sun-mode GNU-style attributes. */
  { "aligned", "?(ci)", "s+", ak_align },
  { "constructor", "", "s+", ak_constructor },
  { "destructor", "", "s+", ak_destructor },
  { "packed", "", "s+", ak_packed },
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  { "visibility", "(sn)", "s+", ak_visibility },
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
  { "weak", "", "s+", ak_weak },
#endif /* SUN_EXTENSIONS_ALLOWED && GNU_EXTENSIONS_ALLOWED */

#if INCLUDE_EDG_TEST_ATTRIBUTES
  /* Attributes used for testing by EDG. */
  { "testattr1", "?(*)", "c+", ak_unrecognized },
  { "t1", "(*)", "c+[edg]", ak_unrecognized },
  { "t2", "(*)", "c+[edg]", ak_unrecognized },
  { "t3", "(*)", "c+[edg]", ak_unrecognized },
  { "t4", "(ct)", "c+[edg]", ak_unrecognized },
  { "t5", "(sn?,n)", "c+[edg]", ak_unrecognized },
  { "t6", "(?ci+)", "c+[edg]", ak_unrecognized },
  { "e1", "(*)", "c+[edg]", ak_edg_e1 },
  { "n1", "(*)", "c+[edg]", ak_edg_n1 },
#endif /* INCLUDE_EDG_TEST_ATTRIBUTES */

  /* Clang-specific attributes. */
  { "assume", "(Xcb)", "lx(190000-)", ak_assume },
  { "availability", "(*)", "lx{clang}", ak_availability },
  { "unavailable", "?(sn)", "lx(30500-)", ak_unavailable },
  { "using_if_exists", "", "l+{clang}", ak_using_if_exists },
  { "exclude_from_explicit_instantiation", "", "lx{clang}(80000-)",
    ak_exclude_from_explicit_instantiation },

  { NULL, NULL, NULL, ak_last }
};

#define KNOWN_ATTR_TABLE_LENGTH \
  ((sizeof_t)(sizeof(known_attr_table)/sizeof(known_attr_table[0])-1))


typedef char* an_attr_application_fn(an_attribute_ptr  ap,
                                     char              *entity,
                                     an_il_entry_kind  entity_kind);

typedef struct an_attr_appl_descr {
  /* Data structure describing how an attribute can be applied to an IL
     entity. */
  an_attribute_kind
		kind;
			/* The kind of attribute this description is applied
			   to.  (This is only useful for internal consistency
			   checking.) */
  a_const_char	*target_constraints;
			/* A compact description of the kind of target entity
			   that this attribute can be applied to.  If this is
			   the empty string, the attributes apply to any entity
			   a priori (though appl_fn can impose limitations of
			   its own).  Otherwise, the string consists of one or
			   more entity kind descriptions separated by "|".
			   Each entity kind description is a single character
			   describing the broad kind of entity (e.g., "r" for
			   "routine"), optionally prefixed by "W" or followed
			   by ":" and one or more "property switches".  The
			   "W" prefix indicates that the entity kind is not a
			   match, but that only a warning should be issued if
			   an attribute is applied to such an entity. The
			   property switches are of the form "+x" or "-x"
			   where "x" is a character representing a property.
			   The "+x" form indicates that the property is
			   required whereas the "-x" for indicates that it is
			   prohibited.  For example, "r" means the attribute
			   is allowed on any routine, "r:+v" means it applies
			   only on virtual routines, and "r:-v" means it
			   applies only on nonvirtual routines.  Here is the
			   set of entity codes and property codes currently
			   recognized:
			     "T"  : type-transforming (must be first)
			     "t"  : types
			       "f"  : function types
			     "c"  : class type (after "class", "struct", or
			              "union" keyword)
			       "d"  : class definition
			     "e"  : enum type (after "enum" or "enum class")
			       "d"  : enum definition
			     "E"  : enumerator constant
			       (no property switches)
			     "r"  : routines
			       "m"  : class member
			       "i"  : inline
			       "p"  : pure virtual
			       "v"  : virtual
			       "x"  : external linkage
			     "v"  : variables
			       "a"  : automatic variables
			       "h"  : exception handler parameter variable
			       "l"  : local variables (automatic/static)
			       "r"  : register variables
			       "x"  : external linkage
			     "p"  : parameters
			       (no property switches)
			     "d"  : fields
			       "b"  : bit field
			     "u"  : using-declarations/using-directives
			       (no property switches)
			     "n"  : namespaces
			       (no property switches)
			     "s"  : statements
			       (no property switches)
			     "l"  : labels
			       (no property switches)
			     "a"  : asm declarations/statements
			       (no property switches)
			     "C"  : concepts
			       (no property switches)
			     "0"  : stand-alone attribute (no target entity)
			       (no property switches)
			   A switch is optionally followed by a "!" to indicate
			   that a failure to meet the requirement should be
			   diagnosed as a hard error (otherwise, it elicits a
			   warning).  For example "v:-r|d:-b!" means that the
			   attribute applies to non-register variables and to
			   fields that aren't bit fields: When applied to a
			   register variable, a warning is issued, but when
			   applied to a bit field an error is issued.  Note
			   the "T" code which is special: If it is present, it
			   must be first and it indicates that it applies to a
			   type by producing a new type entry (instead of
			   modifying an existing type entry "in place").
			   If an attribute fails to meet the constraints
			   encoded here, that attribute is reclassified as
			   ak_unrecognized.  This is true even if the failure 
			   only triggers a warning (because of a 'W' prefix on
			   the entity code, or the lack of a '!' suffix on a
			   property switch). */
  an_attr_application_fn
		*appl_fn;
			/* NULL or a pointer to the function to call to apply
			   the attribute to the entity it appertains to.  (Such
			   a function could enforce constraints and/or reflect
			   the attribute in some aspects of the IL.) */
} an_attr_appl_descr;

#define NO_APPL_FN ((an_attr_application_fn*)NULL)

/* Forward declarations for attribute application functions. */

/* Application functions for standard attributes. */
static an_attr_application_fn apply_align_attr;
static an_attr_application_fn apply_assume_attr;
static an_attr_application_fn apply_base_check_attr;
static an_attr_application_fn apply_carries_dependency_attr;
static an_attr_application_fn apply_deprecated_or_unavailable_attr;
static an_attr_application_fn apply_final_attr;
static an_attr_application_fn apply_hiding_attr;
static an_attr_application_fn apply_noreturn_attr;
static an_attr_application_fn apply_override_attr;
static an_attr_application_fn apply_nodiscard_attr;
static an_attr_application_fn apply_maybe_unused_attr;
static an_attr_application_fn apply_fallthrough_attr;
static an_attr_application_fn apply_likely_attr;
static an_attr_application_fn apply_no_unique_address_attr;
static an_attr_application_fn apply_indeterminate_attr;

/* Internal attributes. */
static an_attr_application_fn apply_conditional_explicit;

/* Other attributes. */
static an_attr_application_fn apply_enable_if_attr;
static an_attr_application_fn apply_pass_object_size_attr;

#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
/* Application functions for nonstandard attributes available in both GNU and
   Microsoft configurations. */
#if GNU_NAKED_ATTRIBUTE_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
static an_attr_application_fn apply_naked_attr;
#endif /* GNU_NAKED_ATTRIBUTE_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
static an_attr_application_fn apply_noinline_attr;
static an_attr_application_fn apply_nothrow_attr;
static an_attr_application_fn apply_pure_attr;
static an_attr_application_fn apply_section_attr;
#endif /* GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */

#if GNU_EXTENSIONS_ALLOWED
/* Application functions for GNU-only attributes. */
static an_attr_application_fn apply_alias_attr;
static an_attr_application_fn apply_alloc_size_attr;
static an_attr_application_fn apply_always_inline_attr;
#if GNU_X86_ATTRIBUTES_ALLOWED
static an_attr_application_fn apply_cdecl_attr;
#endif /* GNU_X86_ATTRIBUTES_ALLOWED */
static an_attr_application_fn apply_cleanup_attr;
static an_attr_application_fn apply_common_attr;
static an_attr_application_fn apply_const_attr;
static an_attr_application_fn apply_constructor_attr;
static an_attr_application_fn apply_destructor_attr;
#if GNU_X86_ATTRIBUTES_ALLOWED
static an_attr_application_fn apply_fastcall_attr;
#endif /* GNU_X86_ATTRIBUTES_ALLOWED */
static an_attr_application_fn apply_format_attr;
static an_attr_application_fn apply_format_arg_attr;
static an_attr_application_fn apply_gnu_inline_attr;
static an_attr_application_fn apply_ifunc_attr;
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
static an_attr_application_fn apply_init_priority_attr;
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
static an_attr_application_fn apply_internal_linkage_attr;
static an_attr_application_fn apply_malloc_attr;
static an_attr_application_fn apply_may_alias_attr;
static an_attr_application_fn apply_mode_attr;
static an_attr_application_fn apply_no_instrument_function_attr;
static an_attr_application_fn apply_no_check_memory_usage_attr;
static an_attr_application_fn apply_nocommon_attr;
static an_attr_application_fn apply_nonnull_attr;
static an_attr_application_fn apply_packed_attr;
static an_attr_application_fn apply_sentinel_attr;
#if GNU_X86_ATTRIBUTES_ALLOWED
static an_attr_application_fn apply_stdcall_attr;
#endif /* GNU_X86_ATTRIBUTES_ALLOWED */
static an_attr_application_fn apply_strong_attr;
static an_attr_application_fn apply_target_attr;
#if THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
static an_attr_application_fn apply_tls_model_attr;
#endif /* THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED */
static an_attr_application_fn apply_transparent_union_attr;
static an_attr_application_fn apply_unused_attr;
static an_attr_application_fn apply_used_attr;
#if GNU_VECTOR_TYPES_ALLOWED
static an_attr_application_fn apply_vector_size_attr;
static an_attr_application_fn apply_neon_vector_type_attr;
static an_attr_application_fn apply_ext_vector_type_attr;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
static an_attr_application_fn apply_visibility_attr;
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
static an_attr_application_fn apply_warn_unused_result_attr;
static an_attr_application_fn apply_weak_attr;
static an_attr_application_fn apply_weakref_attr;
static an_attr_application_fn apply_abi_tag_attr;
static an_attr_application_fn apply_no_specializations;
#endif /* GNU_EXTENSIONS_ALLOWED */

#if MICROSOFT_EXTENSIONS_ALLOWED
/* Application functions for Microsoft-__declspec-only attributes. */
static an_attr_application_fn apply_appdomain_attr;
static an_attr_application_fn apply_assembly_info_attr;
static an_attr_application_fn apply_dllimport_dllexport_attr;
static an_attr_application_fn apply_edg_interior_ptr_alias_attr;
static an_attr_application_fn apply_edg_pin_ptr_alias_attr;
static an_attr_application_fn apply_implementation_key_attr;
static an_attr_application_fn apply_intrin_type_attr;
static an_attr_application_fn apply_jitintrinsic_attr;
static an_attr_application_fn apply_noalias_attr;
static an_attr_application_fn apply_non_user_code_attr;
static an_attr_application_fn apply_novtable_attr;
static an_attr_application_fn apply_process_attr;
static an_attr_application_fn apply_property_attr;
static an_attr_application_fn apply_restrict_attr;
static an_attr_application_fn apply_safebuffers_attr;
static an_attr_application_fn apply_selectany_attr;
#if THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
static an_attr_application_fn apply_thread_attr;
#endif /* THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED */
static an_attr_application_fn apply_uuid_attr;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if INCLUDE_EDG_TEST_ATTRIBUTES
/* Application functions used for testing by EDG. */
static an_attr_application_fn apply_edg_e1_attr;
static an_attr_application_fn apply_edg_n1_attr;
#endif /* INCLUDE_EDG_TEST_ATTRIBUTES */

static an_attr_application_fn apply_availability_attr;
static an_attr_application_fn apply_using_if_exists_attr;
static an_attr_application_fn apply_exclude_from_explicit_instantiation_attr;

/*
Table of entries describing how to apply a specific attribute kind to an IL
entity.  See the description of an_attr_appl_descr for the meaning and form
of each field in this table.  Distinct forms of the same attribute share the
same table entry (and the table must match the enumeration order of
an_attribute_kind).
See also the complementary table known_attr_table above.
*/
STATIC_THREAD an_attr_appl_descr known_attr_appl_table[(int)ak_last+1] = {
  { ak_unrecognized, "", NO_APPL_FN },
  { ak_empty_attr, "", NO_APPL_FN },
  { ak_attr_using_prefix, "", NO_APPL_FN },
  /* Standard attributes. */
  { ak_align, "", apply_align_attr },
  { ak_assume, "s", apply_assume_attr },
  { ak_base_check, "c:+d", apply_base_check_attr },
  { ak_carries_dependency, "r|p", apply_carries_dependency_attr },
  { ak_deprecated, "t|p|c|e|r|v|d|n|E|C",
    apply_deprecated_or_unavailable_attr },
  { ak_final, "r:+v!|c:+d!", apply_final_attr },
  { ak_hiding, "t|c|e|r:+m!|v|d", apply_hiding_attr },
  { ak_known_semantics, "", NO_APPL_FN },
  { ak_noreturn, "t|p|r|v|d", apply_noreturn_attr },
  { ak_override, "r:+v!", apply_override_attr },
  { ak_nodiscard, "r|c|e", apply_nodiscard_attr },
  { ak_noop_dtor, "", NO_APPL_FN },
  { ak_maybe_unused, "c|t|v|p|d|r|e|E|l", apply_maybe_unused_attr },
  { ak_fallthrough, "s", apply_fallthrough_attr },
  { ak_likely, "l|s", apply_likely_attr },
  { ak_unlikely, "l|s", apply_likely_attr },
  { ak_no_unique_address, "d:-b!", apply_no_unique_address_attr },
  { ak_indeterminate, "p|v:+a!", apply_indeterminate_attr },
  /* Nonstandard attributes. */
  { ak_enable_if, "t", apply_enable_if_attr },
  { ak_overloadable, "r", NO_APPL_FN },
  { ak_pass_object_size, "p", apply_pass_object_size_attr },
  { ak_diagnose_if, "r", NO_APPL_FN },
  { ak_unavailable, "t|p|c|e|r|v|d|n|E", apply_deprecated_or_unavailable_attr},
  /* Nonstandard attributes available in both GNU and Microsoft
     configurations. */
#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
#if GNU_NAKED_ATTRIBUTE_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
  { ak_naked, "r", apply_naked_attr },
#endif /* GNU_NAKED_ATTRIBUTE_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
  { ak_noinline, "t|p|r|v|d", apply_noinline_attr },
  { ak_nothrow, "t|r|v|d", apply_nothrow_attr },
  { ak_pure, "r|Wv", apply_pure_attr },
  { ak_section, "r|v:-a!", apply_section_attr },
#endif /* GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  /* GNU-only attributes. */
  { ak_alias, "r|v:-l!", apply_alias_attr },
  { ak_alloc_size, "", apply_alloc_size_attr },
  { ak_always_inline, "r|Wt|Wp|Wv|Wd", apply_always_inline_attr },
  { ak_artificial, "r:+i", NO_APPL_FN },
#if GNU_X86_ATTRIBUTES_ALLOWED
  { ak_cdecl, "t|r|v|d|p", apply_cdecl_attr },
#endif /* GNU_X86_ATTRIBUTES_ALLOWED */
  { ak_cleanup, "v|Wp", apply_cleanup_attr },
  { ak_cold, "r|l", NO_APPL_FN },
  { ak_common, "v:-a|Wr", apply_common_attr },
  { ak_const, "t|r|v|d|p", apply_const_attr },
  { ak_constructor, "r", apply_constructor_attr },
  { ak_destructor, "r", apply_destructor_attr },
  { ak_error, "r", NO_APPL_FN },
#if GNU_VECTOR_TYPES_ALLOWED
  { ak_ext_vector_type, "T", apply_ext_vector_type_attr },
#endif /* GNU_VECTOR_TYPES_ALLOWED */
  { ak_externally_visible, "r:+x|v:+x", NO_APPL_FN },
#if GNU_X86_ATTRIBUTES_ALLOWED
  { ak_fastcall, "t|r|v|d|p", apply_fastcall_attr },
#endif /* GNU_X86_ATTRIBUTES_ALLOWED */
  { ak_flatten, "r", NO_APPL_FN },
  { ak_format, "t|r|v|d|p", apply_format_attr },
  { ak_format_arg, "r", apply_format_arg_attr },
  { ak_gnu_inline, "r", apply_gnu_inline_attr },
  { ak_hot, "r|l", NO_APPL_FN },
  { ak_ifunc, "r", apply_ifunc_attr },
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
  { ak_init_priority, "v:-l", apply_init_priority_attr },
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
  { ak_internal_linkage, "v|c|r", apply_internal_linkage_attr },
  { ak_malloc, "r", apply_malloc_attr },
  { ak_may_alias, "T|c|e", apply_may_alias_attr },
  { ak_mode, "T|e", apply_mode_attr },
  { ak_no_instrument_function, "r", apply_no_instrument_function_attr },
  { ak_no_check_memory_usage, "r", apply_no_check_memory_usage_attr },
  { ak_nocommon, "v:-a|Wr", apply_nocommon_attr },
  { ak_nonnull, "t|r|v|d|p", apply_nonnull_attr },
  { ak_noplt, "r", NO_APPL_FN },
  { ak_packed, "c|e|d|Wv|Wp|Wr|Wt", apply_packed_attr },
  { ak_sentinel, "t|r|v|d", apply_sentinel_attr },
#if GNU_X86_ATTRIBUTES_ALLOWED
  { ak_stdcall, "t|r|v|d|p", apply_stdcall_attr },
#endif /* GNU_X86_ATTRIBUTES_ALLOWED */
  { ak_strong, "u", apply_strong_attr },
  { ak_target, "r", apply_target_attr },
#if THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
  { ak_tls_model, "v|Wr|Wd|Wp", apply_tls_model_attr },
#endif /* THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED */
  { ak_transparent_union, "c|p|t", apply_transparent_union_attr },
  { ak_unused, "c|e|t|r|v|p|l|Wd|Wn|Wu", apply_unused_attr },
  { ak_used, "r|v:-a|Wc|We|Wt|Wp|Wd|Wl|Wn", apply_used_attr },
#if GNU_VECTOR_TYPES_ALLOWED
  { ak_vector_size, "T", apply_vector_size_attr },
  { ak_neon_vector_type, "T", apply_neon_vector_type_attr },
  { ak_neon_polyvector_type, "T", apply_neon_vector_type_attr },
#endif /* GNU_VECTOR_TYPES_ALLOWED */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  { ak_visibility, "r:+x|v:+x|c|n|e", apply_visibility_attr },
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
  { ak_warn_unused_result, "t|r|v|d", apply_warn_unused_result_attr },
  { ak_warning, "r", NO_APPL_FN },
  { ak_weak, "r:+x!|v:+x!", apply_weak_attr },
  { ak_weakref, "r|v", apply_weakref_attr },
  { ak_abi_tag, "r|c|n|v|e", apply_abi_tag_attr },
  { ak_no_specializations, "v|c|r", apply_no_specializations },
#endif /* GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  /* Microsoft-only attributes. */
  { ak_appdomain, "v|Wt|Wp", apply_appdomain_attr },
  { ak_assembly_info, "c|e", apply_assembly_info_attr },
  { ak_dllexport, "c|e|r|v:-a!|Wt|Wp", apply_dllimport_dllexport_attr },
  { ak_dllimport, "c|e|r|v:-a!|Wt|Wp", apply_dllimport_dllexport_attr },
  { ak_edg_interior_ptr_alias, "t", apply_edg_interior_ptr_alias_attr },
  { ak_edg_pin_ptr_alias, "t", apply_edg_pin_ptr_alias_attr },
  { ak_empty_bases, "c", NO_APPL_FN },
  { ak_guard, "p|r|v", NO_APPL_FN },
  { ak_hybrid_patchable, "", NO_APPL_FN },
  { ak_implementation_key, "", apply_implementation_key_attr },
  { ak_intrin_type, "c|Wp", apply_intrin_type_attr },
  { ak_jitintrinsic, "r", apply_jitintrinsic_attr },
  { ak_no_init_all, "", NO_APPL_FN },
  { ak_noalias, "r|Wp", apply_noalias_attr },
  { ak_non_user_code, "t|p|r|v|d", apply_non_user_code_attr },
  { ak_novtable, "c|Wp", apply_novtable_attr },
  { ak_process, "v|Wt|Wp", apply_process_attr },
  { ak_property, "d|Wt|Wp", apply_property_attr },
  { ak_restrict, "r|Wp", apply_restrict_attr },
  { ak_safebuffers, "r", apply_safebuffers_attr },
  { ak_selectany, "v:+x!|Wr|Wt|Wp|Wd", apply_selectany_attr },
  { ak_spectre, "", NO_APPL_FN },
#if THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
  { ak_thread, "v|Wt|Wp", apply_thread_attr },
#endif /* THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED */
  { ak_uuid, "c|e|Wr|Wv|Wt|Wp|Wd", apply_uuid_attr },
  { ak_layout_as_external, "t|p|r|v|d", NO_APPL_FN },
  { ak_no_empty_identity_interface, "t|p|r|v|d", NO_APPL_FN },
  { ak_no_ftm, "t|p|r|v|d", NO_APPL_FN },
  { ak_no_refcount, "t|p|r|v|d", NO_APPL_FN },
  { ak_no_release_return, "t|p|r|v|d", NO_APPL_FN },
  { ak_no_weakreferencesource, "t|p|r|v|d", NO_APPL_FN },
  { ak_one_phase_constructed, "t|p|r|v|d", NO_APPL_FN },
  { ak_allocator, "r", NO_APPL_FN },
  { ak_no_sanitize_address, "r|v", NO_APPL_FN },
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if INCLUDE_EDG_TEST_ATTRIBUTES
  { ak_edg_e1, "", apply_edg_e1_attr },
  { ak_edg_n1, "", apply_edg_n1_attr },
#endif /* INCLUDE_EDG_TEST_ATTRIBUTES */

  /* Clang-specific attributes. */
  { ak_availability, "", apply_availability_attr },
  { ak_using_if_exists, "u|t|r|v|n", apply_using_if_exists_attr },
  { ak_exclude_from_explicit_instantiation, "r:+m|v|c",
    apply_exclude_from_explicit_instantiation_attr },

  /* Internal attributes. */
  { ak_annotation, "", NO_APPL_FN },
  { ak_conditional_explicit, "", apply_conditional_explicit },
  { ak_pragma_pack_state, "", NO_APPL_FN },

  { ak_last, "!!ERROR", NO_APPL_FN }
};


typedef struct an_attr_corresp_descr *an_attr_corresp_descr_ptr;
typedef struct an_attr_corresp_descr {
  /* Data structure describing special rules when checking attributes on
     corresponding entities across translation units.  (If an attribute
     kind/family/target-entity triple does not appear in this table, it is
     assumed that that kind of attribute must match exactly across
     translation units.) */
  an_attribute_kind
		kind;	/* The kind of attribute this entry describes. */
  an_attribute_family
		family;	/* The attribute family to which this entry applies,
			   or af_last if it applies to all families. */
  an_il_entry_kind
		target_kind;
			/* The kind of target entity for which this entry
			   is meant (e.g., an attribute X may apply to both
			   variables and types, but the handling of cross-
			   translation-unit correspondences may be different
			   for variables and types).  A value of iek_last
			   applies to all entities. */
  an_attr_corresp_flag_set
		corresp_flags;
			/* Flags describing how correspondence checking
			   should be handled.  See the ACF_... macros in
			   attribute.h for details. */
  an_attr_corresp_checking_fn
		*checking_fn;
			/* If (corresp_flags & ACF_MATCH_MASK) is
			   ACF_CUSTOM_MATCH, this is a pointer to a function
			   to check the correspondence between two
			   attributes.  Otherwise, NULL (the most common
			   case). */
} an_attr_corresp_descr;


STATIC_THREAD an_attr_corresp_descr attr_corresp_table[] = {
  { ak_align, af_last, iek_last, ACF_MATCH_OPTIONAL | ACF_ALWAYS_TRANS_COPY,
    NO_CHECKING_FN },
  { ak_noreturn, af_std, iek_last, ACF_STRICT_MATCH, NO_CHECKING_FN },
  { ak_noreturn, af_gnu, iek_last, ACF_MATCH_OPTIONAL, NO_CHECKING_FN },
  { ak_noreturn, af_ms_declspec, iek_last, ACF_MATCH_OPTIONAL,
            NO_CHECKING_FN },
  { ak_deprecated, af_last, iek_last, ACF_MATCH_OPTIONAL, NO_CHECKING_FN },
  { ak_maybe_unused, af_last, iek_last, ACF_MATCH_OPTIONAL, NO_CHECKING_FN },
#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
  { ak_noinline, af_last, iek_last, ACF_MATCH_OPTIONAL, NO_CHECKING_FN },
  { ak_nothrow, af_last, iek_last, ACF_MATCH_OPTIONAL, NO_CHECKING_FN },
#endif /* GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  { ak_constructor, af_gnu, iek_last, ACF_STRICT_MATCH_OR_VOID,
            NO_CHECKING_FN },
  { ak_destructor, af_gnu, iek_last, ACF_STRICT_MATCH_OR_VOID,
            NO_CHECKING_FN },
  { ak_flatten, af_gnu, iek_last, ACF_MATCH_OPTIONAL, NO_CHECKING_FN },
  { ak_format_arg, af_gnu, iek_last, ACF_MATCH_OPTIONAL, NO_CHECKING_FN },
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
  { ak_init_priority, af_gnu, iek_last, ACF_STRICT_MATCH_OR_VOID,
            NO_CHECKING_FN },
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
  { ak_malloc, af_gnu, iek_last, ACF_MATCH_OPTIONAL | ACF_ALWAYS_TRANS_COPY,
    NO_CHECKING_FN },
  { ak_mode, af_gnu, iek_last, ACF_MATCH_OPTIONAL, NO_CHECKING_FN },
  { ak_nocommon, af_gnu, iek_last, ACF_MATCH_OPTIONAL, NO_CHECKING_FN },
  { ak_nonnull, af_gnu, iek_last, ACF_MATCH_OPTIONAL, NO_CHECKING_FN },
  { ak_unused, af_last, iek_last, ACF_MATCH_OPTIONAL, NO_CHECKING_FN },
  { ak_used, af_gnu, iek_last, ACF_MATCH_OPTIONAL, NO_CHECKING_FN },
#if GNU_VECTOR_TYPES_ALLOWED
  { ak_vector_size, af_gnu, iek_last, ACF_MATCH_OPTIONAL, NO_CHECKING_FN },
  { ak_neon_vector_type, af_gnu, iek_last, ACF_MATCH_OPTIONAL,
            NO_CHECKING_FN },
  { ak_neon_polyvector_type, af_gnu, iek_last, ACF_MATCH_OPTIONAL,
            NO_CHECKING_FN },
#endif /* GNU_VECTOR_TYPES_ALLOWED */
  { ak_weak, af_gnu, iek_last, ACF_MATCH_OPTIONAL, NO_CHECKING_FN },
  { ak_weakref, af_gnu, iek_last, ACF_MATCH_OPTIONAL, NO_CHECKING_FN },
#endif /* GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  { ak_dllexport, af_ms_declspec, iek_last, ACF_MATCH_OPTIONAL,
            NO_CHECKING_FN },
  { ak_dllimport, af_ms_declspec, iek_last, ACF_MATCH_OPTIONAL,
            NO_CHECKING_FN },
  { ak_implementation_key, af_ms_declspec, iek_last, ACF_STRICT_MATCH_OR_VOID,
            NO_CHECKING_FN },
  { ak_naked, af_ms_declspec, iek_last, ACF_MATCH_OPTIONAL, NO_CHECKING_FN },
  { ak_selectany, af_ms_declspec, iek_last, ACF_MATCH_OPTIONAL,
            NO_CHECKING_FN },
  { ak_uuid, af_ms_declspec, iek_last, ACF_STRICT_MATCH_OR_VOID,
            NO_CHECKING_FN },
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  { ak_diagnose_if, af_last, iek_last, ACF_MATCH_OPTIONAL, NO_CHECKING_FN },
  { ak_last, af_last, iek_last, ACF_NO_FLAGS, NO_CHECKING_FN }
};

#define ATTR_CORRESP_TABLE_LENGTH \
  ((sizeof_t)(sizeof(attr_corresp_table)/sizeof(attr_corresp_table[0])-1))

/*
A list of attribute namespaces that are known to the front end.
*/
static constexpr a_const_char *valid_attribute_namespaces[] = {
  "clang",
  "gnu",
  "__gnu__",
  "msvc",
  "_Clang",
#if INCLUDE_EDG_TEST_ATTRIBUTES
  "edg",
#endif /* INCLUDE_EDG_TEST_ATTRIBUTES */
  NULL  /* must be last */
};

/*
Pointer to a hash table indexing attr_corresp_table by attribute kind.
*/
STATIC_THREAD a_hash_table_ptr
		attr_corresp_checking_map;

/*
Bucket type for attr_corresp_checking_map.
*/
typedef struct an_attr_corresp_checking_map_entry
	*an_attr_corresp_checking_map_entry_ptr;
typedef struct an_attr_corresp_checking_map_entry {
  an_attr_corresp_checking_map_entry_ptr
		next;
			/* The next map entry for an attribute of the same
			   kind as this entry. */
  an_attr_corresp_descr_ptr
		descr;
			/* The attribute description entry for this map
			   entry. */
} an_attr_corresp_checking_map_entry;


STATIC_THREAD an_attr_corresp_checking_map_entry
		corresp_checking_map_entries[ATTR_CORRESP_TABLE_LENGTH];
			/* Since the number of buckets for
			   attr_corresp_checking_map is fixed, we can store
			   the buckets in a fixed array. */


a_boolean compare_for_attr_corresp_checking_map(a_void_ptr  entry,
                                                a_void_ptr  key)
/*
Compare the attribute kind associated with entry (entry is a pointer to an
object of type an_attr_corresp_checking_map_entry) to the attribute kind
pointed to by key.  Return TRUE if they are equal.
*/
{
  an_attribute_kind  entry_kind =
                 ((an_attr_corresp_checking_map_entry_ptr)entry)->descr->kind;

  return entry_kind == *(an_attribute_kind*)key;
}  /* compare_for_attr_corresp_checking_map */


a_hash_value hash_attribute_kind(a_void_ptr  key)
/*
key points to an attribute kind.  Return that value (which is trivially
suitable as a hash).
*/
{
  return (a_hash_value)*(an_attribute_kind*)key;
}  /* hash_attribute_kind */


static void init_attr_corresp_checking_map(void)
/*
Initialize the attribute correspondence checking map.
*/
{
  unsigned int  k;

  attr_corresp_checking_map = alloc_hash_table(NO_MEMORY_REGION_NUMBER,
                       (a_hash_table_size)ATTR_CORRESP_TABLE_LENGTH,
                       fn_for_function(hash_attribute_kind),
                       fn_for_function(compare_for_attr_corresp_checking_map));
  for (k = 0; k<ATTR_CORRESP_TABLE_LENGTH; ++k) {
    an_attr_corresp_checking_map_entry_ptr  *p_ep;
    an_attribute_kind                       kind = attr_corresp_table[k].kind;
    p_ep = (an_attr_corresp_checking_map_entry_ptr*)
                 hash_find(attr_corresp_checking_map, &kind, /*create=*/TRUE);
    corresp_checking_map_entries[k].next = *p_ep;
    corresp_checking_map_entries[k].descr = &attr_corresp_table[k];
    *p_ep = &corresp_checking_map_entries[k];
  }  /* for */
}  /* init_attr_corresp_checking_map */


static a_const_char *attribute_display_name(an_attribute_ptr ap)
/*
Returns the name of an attribute, suitable for display in diagnostic messages.
If necessary, memory is allocated to contain the result (so it can be used
in diagnostic messages that may be delayed).
*/
{
  a_const_char *result = ap->name;

  if (ap->namespace_name != NULL) {
    Small_string<MAX_ATTRIBUTE_NAME_LENGTH * 2 + 3>
                buffer(ap->namespace_name, "::", ap->name);

    result = buffer.to_allocated_storage(FE_allocator<char>());
  }  /* if */
  if (result == NULL) {
    /* The attribute has no name.  This routine is often used to display
       an error string, so use a dummy string to keep those routines happy. */
    result = "";
  }  /* if */
  return result;
}  /* attribute_display_name */


static a_boolean attribute_namespace_is_recognized(a_const_char *name,
                                                   size_t       length)
/*
Returns TRUE if the specified name is known to the front end as an attribute
namespace.  If non-zero, length represents the number of characters in name
(used when name is not NULL-terminated).
*/
{
  a_boolean            result = FALSE;
  a_const_char * const *np;

  if (length == 0) {
    length = strlen(name);
  }  /* if */
  for (np = valid_attribute_namespaces; *np != NULL; np++) {
    if (strncmp(*np, name, length) == 0) {
      result = TRUE;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* attribute_namespace_is_recognized */


static void check_for_unrecognized_attribute_namespace(an_attribute_ptr ap)
/*
Issues a diagnostic if ap->name is not an attribute namespace that is
known to the front end.  Also sets ap->is_invalid_namespace in that case to
avoid issuing "unknown attribute" warnings when using this namespace.
*/
{
  a_const_char *name = ap->name;

  if (!attribute_namespace_is_recognized(name, (size_t)0)) {
    ap->is_invalid_namespace = TRUE;
    pos_st_warning(ec_attribute_namespace_unrecognized, &ap->position, name);
  }  /* if */
}  /* check_for_unrecognized_attribute_namespace */


void get_attr_corresp_checking_info(an_attribute_ptr             ap,
                                    an_il_entry_kind             target_kind,
                                    an_attr_corresp_flag_set     *p_flags,
                                    an_attr_corresp_checking_fn  **p_fn)
/*
Given an attribute (ap) and the kind of entity it applies to (target_kind)
return a set of flags (*p_flags) describing how checking and copying the
attribute across translation units should be handled, and return through
*p_fn a function that should be called for correspondence checking (or NULL
if no function should be called).
*/
{
  an_attr_corresp_checking_map_entry_ptr  *p_ep, ep = NULL;
  an_attribute_kind                       key = (an_attribute_kind)ap->kind;

  if (attr_corresp_checking_map == NULL) init_attr_corresp_checking_map();
  p_ep = (an_attr_corresp_checking_map_entry_ptr*)
            hash_find(attr_corresp_checking_map, &key, /*create=*/FALSE);
  if (p_ep != NULL) {
    /* There are entries associated with ap->kind.  See if they also match
       family and target entity kind. */
    check_assertion(*p_ep != NULL);
    for (ep = *p_ep; ep != NULL; ep = ep->next) {
      if (ep->descr->family != af_last &&
          ep->descr->family != (an_attribute_family)ap->family) {
        continue;
      } else if (ep->descr->target_kind != iek_last &&
                 ep->descr->target_kind != target_kind) {
        continue;
      } else {
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  if (ep == NULL) {
    /* By default, require strict matching. */
    *p_flags = ACF_STRICT_MATCH;
    *p_fn = NO_CHECKING_FN;
  } else {
    *p_flags = ep->descr->corresp_flags;
    *p_fn = ep->descr->checking_fn;
  }  /* if */
}  /* get_attr_corresp_checking_info */

#if CHECKING

NORETURN static void abort_for_misconfigured_attribute(
                                                an_attribute_ptr  ap,
                                                a_const_char      *filename,
                                                int               line_number,
                                                a_const_char      *function,
                                                a_const_char      *msg)
/*
Abort with a message indicating the given file name, line number, function,
and message.  Also indicate the name of the affected attribute.  This function
is called through the macro check_attr_config.
*/
{
  /* Create a parenthesized note, mentioning the attribute name, to be
     appended to the message. */
  Small_string<MAX_ATTRIBUTE_NAME_LENGTH + 20>
                attr_name("(for attribute ", attribute_display_name(ap), ")");

  assertion_failed(filename, line_number, function, msg,
                   attr_name.as_temp_characters());
}  /* abort_for_misconfigured_attribute */


/* Macro to test an assertion regarding the attribute configuration tables. */
/*lint -emacro(506,check_attr_config)*/
#define check_attr_config(test, ap, msg)                                     \
  ((/*lint --e(774)*/(test)) ? (void)0 :                                     \
    abort_for_misconfigured_attribute((ap), __FILE__, __LINE__, __EDG_func__,\
                                      (char*)msg))

#else /* !CHECKING */

#define check_attr_config(test, ap, msg)  /* Nothing */

#endif /*CHECKING */

/*
Pointer to a hash table indexing known_attr_table by attribute name.
*/
STATIC_THREAD a_hash_table_ptr
		attr_name_map;

/*
Bucket type for attr_name_map.
*/
typedef struct an_attr_name_map_entry {
  an_attr_name_map_entry_ptr
		next;
			/* The next map entry for an attribute of the same
			   name as this entry. */
  an_attr_descr_ptr
		descr;
			/* The attribute description entry for this map
			   entry. */
} an_attr_name_map_entry;


STATIC_THREAD an_attr_name_map_entry
		attr_name_map_entries[KNOWN_ATTR_TABLE_LENGTH];
			/* Since the number of buckets for attr_name_map is
			   fixed, we can store the buckets in a fixed array. */


a_boolean compare_for_attr_name_map(a_void_ptr  entry,
                                    a_void_ptr  key)
/*
Compare the attribute name associated with entry (entry is a pointer to an
entry of type an_attr_name_map_entry) to the given key (key is a pointer to a
character string).  Return TRUE if they are equal.
*/
{
  a_const_char *name = ((an_attr_name_map_entry_ptr)entry)->descr->name;

  return strcmp(name, (char*)key) == 0;
}  /* compare_for_attr_name_map */


static void init_attr_name_map(void)
/*
Initialize the attribute name map.
*/
{
  unsigned int  k;

  attr_name_map = alloc_hash_table(NO_MEMORY_REGION_NUMBER,
                                   (a_hash_table_size)KNOWN_ATTR_TABLE_LENGTH,
                                   fn_for_function(hash_source_string),
                                   fn_for_function(compare_for_attr_name_map));
  for (k = 0; k<KNOWN_ATTR_TABLE_LENGTH; ++k) {
    an_attr_name_map_entry_ptr  *ep;
    a_const_char                *name = known_attr_table[k].name;
    check_assertion(strlen(name) <= MAX_ATTRIBUTE_NAME_LENGTH);
    ep = (an_attr_name_map_entry_ptr*)hash_find(attr_name_map,
                                                (a_void_ptr)name,
                                                /*create=*/TRUE);
    attr_name_map_entries[k].next = *ep;
    attr_name_map_entries[k].descr = &known_attr_table[k];
    *ep = &attr_name_map_entries[k];
#if EXPENSIVE_CHECKING
    {  /* Verify that any namespace referred to by a "cond" string is also
          in the valid_attribute_namespaces array. */
      a_const_char *p1, *p2;
      char         closing = ']';
      p1 = strchr(known_attr_table[k].cond, '[');
      if (p1 == NULL) {
        p1 = strchr(known_attr_table[k].cond, '{');
        closing = '}';
      }  /* if */
      if (p1 != NULL) {
        p1++;
        p2 = strchr(p1, closing);
        check_assertion(p2 != NULL);
        if (!attribute_namespace_is_recognized(p1, size_t_arg(p2 - p1))) {
          unexpected_condition_str("attribute namespace is missing");
        }  /* if */
      }  /* if */
    }
#endif /* EXPENSIVE_CHECKING */
  }  /* for */
}  /* init_attr_name_map */


static a_boolean in_attr_cond_range(unsigned long               version,
                                    a_const_char                **cond_range,
                                    ARG_UNUSED an_attribute_ptr ap)
/*
*cond_range is a "version range" portion of the cond string in an attribute
description entry (an_attr_descr) for the given attribute.  Return TRUE if
version lies in the indicated range.  *cond_range is set to point to the
character after the range (or to the start of an auxiliary range if one
exists).
*/
{
  unsigned long  min_version = 0, max_version = (unsigned long)-1;
  a_const_char   *str = *cond_range;

  check_attr_config(str[0] == '(', ap, "invalid version range configuration");
  str += 1;
  if (str[0] != '-') {
    check_attr_config(str[0] >= '0' && str[0] <= '9', ap,
                      "invalid version range configuration");
    min_version = strtoul(str, (char **)&str, 10);
  }  /* if */
  if (str[0] == '-') {
    str += 1;
    if (str[0] >= '0' && str[0] <= '9') {
      max_version = strtoul(str, (char **)&str, 10);
    }  /* if */
  } else {
    /* Not a range, but a single version number. */
    max_version = min_version;
  }  /* if */
  check_attr_config(str[0] == ')' || str[0] == '|', ap,
                    "invalid version range configuration");
  if (str[0] == ')') str += 1;
  *cond_range = str;
  return version >= min_version && version <= max_version;
}  /* in_attr_cond_range */


static a_boolean attribute_condition_satisfied(
                                           unsigned long               version,
                                           a_const_char                *str,
                                           ARG_UNUSED an_attribute_ptr ap)
/*
str is the beginning of the primary version range in the condition string in
the description entry (an_attr_descr) for the given attribute.  version is
the value of the global version variable (i.e., std_version, microsoft_version,
gnu_version) associated with the family type of the attribute.
Return TRUE if the condition is satisfied (either by the primary version
comparison or any auxiliary comparisons).
*/
{
  a_boolean      result = FALSE, mode = FALSE;

  if (str[0] == '(' && str[1] == '|') {
    /* If the condition starts with "(|", there is no primary condition, and
       only auxiliary conditions are considered. */
    str += 1;
  } else {
    /* See if the primary range comparison succeeds. */
    result = in_attr_cond_range(version, &str, ap);
  }
  /* Process any auxiliary version ranges if necessary. */
  while (!result && str[0] == '|') {
    str += 1;
    switch (str[0]) {
      case 'M':
        version = microsoft_version;
        mode = microsoft_mode;
        break;
      case 'G':
        version = gnu_version;
        mode = gnu_mode;
        break;
      case 'C':
        version = clang_version;
        mode = clang_mode;
        break;
      case 'S':
        version = std_version;
        mode = TRUE;
        break;
      default:
        check_attr_config(FALSE, ap, "invalid auxiliary range specifier");
        break;
    }  /* switch */
    str += 1;
    /* Compute the result of this operation.  Note that we check the condition
       before checking the mode to ensure full expression parsing. */
    result = in_attr_cond_range(version, &str, ap) && mode;
  }  /* while */
  return result;
}  /* attribute_condition_satisfied */


static a_boolean is_attr_in_gnu_namespace(an_attribute_ptr  ap)
/*
Check whether the given attribute is in the gnu attribute namespace.
*/
{
  return strcmp(ap->namespace_name, "gnu") == 0 ||
         strcmp(ap->namespace_name, "__gnu__") == 0;
}  /* is_attr_in_gnu_namespace */


static a_boolean is_attr_in_clang_namespace(an_attribute_ptr  ap)
/*
Check whether the given attribute is in the clang attribute namespace.
*/
{
  return strcmp(ap->namespace_name, "clang") == 0 ||
         strcmp(ap->namespace_name, "_Clang") == 0;
}  /* is_attr_in_clang_namespace */


static a_boolean attribute_namespace_satisfied(a_const_char      **cond,
                                               an_attribute_ptr  ap)
/*
**cond points to the location in the attribute condition string where the
attribute namespace (if any) is specified.  This routine returns TRUE if there
is a match vis-a-vis the specified attribute namespace and the condition
string.  When TRUE is returned, *cond is updated to point to the position in
the attribute string past the closing "]" or "}".
*/
{
  a_const_char  *ptr = *cond;
  a_boolean     match = FALSE;
  a_boolean     required = (*ptr == '[');
  a_boolean     optional = (*ptr == '{');

  if (ap->namespace_name == NULL && !required) {
    /* No attribute namespace specified and none required. */
    match = TRUE;
    /* *cond doesn't need to be updated. */
  } else if (ap->namespace_name != NULL) {
    if (required || optional) {
      /* A namespace has been specified and the condition string indicates that
         a namespace is either required or optional.  See if it is a match. */
      sizeof_t  len = strlen(ap->namespace_name);
      ptr++;
      a_boolean name_match = FALSE;
      if (strncmp(ap->namespace_name, ptr, len) == 0) {
        name_match = TRUE;
      } else if (strncmp(ap->namespace_name, "_Clang", 7) == 0 &&
                 strncmp(ptr, "clang", 5) == 0) {
        /* As a special case, map "_Clang" to "clang". */
        name_match = TRUE;
        len = 5;
      }  /* if */
      if (name_match && ptr[len] == (required ? ']' : '}')) {
        match = TRUE;
        *cond = ptr+len+1;
      }  /* if */
    } else {
      /* A namespace has been specified but none is required by the condition
         string.  GNU appears to accept, e.g., [[gnu::packed]], so if the
         attribute namespace matches the emulation mode, allow it.  Clang
         seems inconsistent here, accepting many but not all instances of
         "gnu" namespace attributes when they appear in attributes, but
         sometimes not when querying for them with __has_cpp_attribute.
         Err on the side of accepting them. */
      if (!(clang_mode && ap->family == af_has_attribute) && gnu_mode &&
          is_attr_in_gnu_namespace(ap)) {
        match = TRUE;
      } else if (clang_version_is(>=210000) && ap->family == af_std &&
                 is_attr_in_clang_namespace(ap)) {
        /* It appears that clang accepts GNU attributes with standard attribute
           syntax when using the clang namespace starting with version 21. */
        match = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return match;
}  /* attribute_namespace_satisfied */


static a_boolean cond_matches_std_attr_mode(a_const_char      *cond,
                                            an_attribute_ptr  ap)
/*
cond is the "cond" field of an attribute description entry for the given
standard-syntax attribute.  Return TRUE if the current mode and the attribute's
namespace (if any) matches the modes and namespace encoded in that string.
*/
{
  a_boolean  match = FALSE;

  if (*cond == 'c') {
    /* A 'c' by itself implies C mode only.  A 'c+' implies C++ mode only. */
    cond++;
    if (*cond == '+') {
      match = !C_mode();
      cond++;
    } else {
      match = C_mode();
    }  /* if */
    if (match) {
      /* See if the attribute namespace is a match. */
      match = attribute_namespace_satisfied(&cond, ap);
      /* Next check for a version constraint, if any. */
      if (match && *cond == '(') {
        match = attribute_condition_satisfied(std_version, cond, ap);
      }  /* if */
    }  /* if */
  }  /* if */
  return match;
}  /* cond_matches_std_attr_mode */


static a_boolean cond_matches_gnu_attr_mode(a_const_char      *cond,
                                            an_attribute_ptr  ap)
/*
cond is the "cond" field of an attribute description entry for the given GNU
attribute.  Return TRUE if the current mode matches the modes encoded in that
string.
*/
{
  a_boolean  match = FALSE;

  if (cond[0] == 's') {
    match = sun_mode;
    /* The full string should just be "s+": There is no Sun C mode and no
       sun_version. */
    check_attr_config(cond[1] == '+' && cond[2] == '\0', ap,
                      "invalid Sun mode attribute configuration");
  } else if (cond[0] == 'g') {
    match = (cond[1] == 'x' && gnu_mode) ||
            (cond[1] == 'c' && gcc_mode) ||
            (cond[1] == '+' && gpp_mode);
    cond += 2;
    if (match) {
      /* See if the attribute namespace is a match. */
      match = attribute_namespace_satisfied(&cond, ap);
    }  /* if */
    if (match && *cond == '(') {
      /* A range specification follows. */
      match = attribute_condition_satisfied(gnu_version, cond, ap);
    }  /* if */
  } else if (cond[0] == 'l' && clang_mode) {
    match = (cond[1] == 'x') ||
            (cond[1] == 'c' && C_mode()) ||
            (cond[1] == '+' && !C_mode());
    cond += 2;
    if (match) {
      /* See if the attribute namespace is a match. */
      match = attribute_namespace_satisfied(&cond, ap);
    }  /* if */
    if (match && *cond == '(') {
      /* A range specification follows. */
      match = attribute_condition_satisfied(clang_version, cond, ap);
    }  /* if */
  }  /* if */
  return match;
}  /* cond_matches_gnu_attr_mode */


static a_boolean cond_matches_ms_declspec_mode(a_const_char      *cond,
                                               an_attribute_ptr  ap)
/*
cond is the "cond" field of an attribute description entry for the given
Microsoft __declspec attribute.  Return TRUE if the current mode matches the
modes encoded in that string.
*/
{
  a_boolean  match = FALSE;

  if (cond[0] == 'm' && ms_extensions) {
    match = cond[1] == 'x' ||
            (cond[1] == 'c' && C_mode()) ||
            (cond[1] == '+' && !C_mode());
    if (match && cond[2] == '(') {
      /* A range specification follows. */
      match = attribute_condition_satisfied(microsoft_version, cond+2, ap);
    }  /* if */
  }  /* if */
  return match;
}  /* cond_matches_ms_declspec_mode */


STATIC_THREAD int
                attr_family_seen[(int)ak_last];
			/* An array used to efficiently detect duplicated
			   attributes. */


void reset_attr_family_seen(an_attribute_ptr ap)
/*
Reset the appropriate flag in attr_family_seen for the specified attribute.
*/
{
  attr_family_seen[ap->kind] = 0;
}  /* reset_attr_family_seen */


static an_attr_name_map_entry_ptr *lookup_attribute_name(
                                                    a_const_char        *name,
                                                    an_attribute_family family)
/*
Look up name in the attr_name_map hash table (which is initialized if this
is its first use).  In C23 an attribute can be specified with two leading and
trailing underscores (i.e., __attr__ is equivalent to attr).  GNU mode
attributes can also have leading and trailing underscores.  Return the result
of the lookup.
*/
{
  char buf[MAX_ATTRIBUTE_NAME_LENGTH + 1];

  if (name[0] == '_' && name[1] == '_' &&
      (c23_mode ||
       (family == af_gnu ||
        family == af_has_attribute ||
        (family == af_std && gnu_mode)))) {
    /* Strip leading and trailing "__" if the result would be neither empty
       nor too long. */
    sizeof_t len = strlen(name);
    if (len > 4 && name[len - 1] == '_' && name[len - 2] == '_') {
      len -= 4;
      if (len <= MAX_ATTRIBUTE_NAME_LENGTH) {
        (void)strncpy(buf, name + 2, size_t_arg(len));
        buf[len] = '\0';
        name = buf;
      }  /* if */
    }  /* if */
  }  /* if */
  return (an_attr_name_map_entry_ptr *)hash_find(attr_name_map,
                                                 (a_void_ptr)name,
                                                 /*create=*/FALSE);
}  /* lookup_attribute_name */


static an_attr_descr_ptr get_attr_descr_for_attribute(an_attribute_ptr  ap)
/*
The given attribute has a determined family, name (and namespace name, if
applicable).  Find and return the associated attribute description record if
there is an applicable one; otherwise, return NULL.
*/
{
  an_attr_descr_ptr           result = NULL;
  an_attr_name_map_entry_ptr  *p_ep, ep = NULL;
  a_const_char                *name = ap->name;
  an_attribute_family         family = ap->family;

  if (gnu_mode && gnu_version >= 40800 &&
      family == af_std &&
      ap->namespace_name != NULL &&
      !ms_extensions &&
      is_attr_in_gnu_namespace(ap)) {
    /* Starting with version 4.8, GCC maps standard attributes of the form
       [[ gnu::xyz(...) ]] to __attribute((xyz(...))).  This includes
       attribute names with added underscores (see below). */
    family = af_gnu;
    ap->is_std_gcc_attribute = TRUE;
  } else if (clang_mode &&
             family == af_std &&
             ap->namespace_name != NULL &&
             !ms_extensions &&
             is_attr_in_clang_namespace(ap)) {
    /* Clang also maps [[ clang::xyz(...) ]] to __attribute((xyz(...))). */
    family = af_gnu;
    ap->is_std_gcc_attribute = TRUE;
  }  /* if */
  p_ep = lookup_attribute_name(name, (an_attribute_family)family);
  if (p_ep != NULL) {
    check_assertion(*p_ep != NULL);
    for (ep = *p_ep; ep != NULL; ep = ep->next) {
      a_const_char *cond = ep->descr->cond;
      /* Skip a leading "1" (which indicates that the attribute should appear
         at most once in a group). */
      if (cond[0] == '1') ++cond;
      switch (family) {
        case af_std:
          if (cond_matches_std_attr_mode(cond, ap)) goto search_done;
          break;
        case af_gnu:
          if (cond_matches_gnu_attr_mode(cond, ap)) goto search_done;
          break;
        case af_ms_declspec:
          if (cond_matches_ms_declspec_mode(cond, ap)) goto search_done;
          break;
        default:
          unexpected_condition();
      }  /* switch */
    }  /* for */
search_done:
    if (ep != NULL) {
      /* An attribute description matching the given attribute and current
         mode was found.  Update the "kind" and "transforms_type_specifier"
         fields in the attribute accordingly. */
      an_attr_appl_descr  *aadp;
      result = ep->descr;
      ap->kind = result->attr_kind;
      aadp = &known_attr_appl_table[(int)ap->kind];
      ap->transforms_type_specifier = (aadp->target_constraints[0] == 'T');
      /* Check that the attribute application table contains the correct
         entry at the expected index. */
      check_attr_config(aadp->kind == ap->kind, ap,
                        "known_attr_appl_table misconfigured");
    }  /* if */
  }  /* if */
  return result;
}  /* get_attr_descr_for_attribute */


static void record_empty_attribute_argument(an_attribute_ptr   ap,
                                            a_const_char       *sig,
                                            a_source_position  *lparen_pos)
/*
An argument list of the form "()" has been encountered (the current token is
the right parenthesis, except in some error cases) for the given attribute.
sig points to the character after the "(" in the attribute's signature and
*lparen_pos is the position of the left parenthesis.  If the signature does
not allow for an empty list, issue a diagnostic (and set ap->kind to
ak_unrecognized).  Either way, return an aak_empty attribute argument.
*/
{
  an_attribute_arg_ptr  aap = alloc_attribute_arg();

  aap->kind = (an_attribute_arg_kind)aak_empty;
  aap->position = *lparen_pos;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  aap->end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  if (*sig != '*' && *sig != '?' && *sig != ')' && curr_token == tok_rparen &&
      !is_unrecognized_attr(ap)) {
    pos_st_error(ec_invalid_empty_attribute_arg_list, lparen_pos,
                 attribute_display_name(ap));
    make_attr_unrecognized(ap);
  }  /* if */
  ap->arguments = aap;
}  /* record_empty_attribute_argument */


static an_attribute_arg_ptr scan_attr_type_arg(an_attribute_ptr  ap)
/*
Scan a (possibly dependent) type argument for the given attribute.  If an
error occurs, set ap->kind to ak_unrecognized and return NULL.  Otherwise,
return a pointer to the argument's representation.
*/
{
  an_attribute_arg_ptr  aap = NULL;
  a_type_ptr            type;
  a_source_position     arg_pos;

  arg_pos = pos_curr_token;
  type_name(&type);
  if (!is_error_type(type)) {
    aap = alloc_attribute_arg();
    aap->kind = (an_attribute_arg_kind)aak_type;
    aap->position = arg_pos;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    aap->end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    aap->variant.type = type;
  } else {
    make_attr_unrecognized(ap);
  }  /* if */
  return aap;
}  /* scan_attr_type_arg */


static an_attribute_arg_ptr scan_attr_expr_arg(
                                          an_attribute_ptr  ap,
                                          int               precedence,
                                          a_boolean         evaluated,
                                          a_boolean         convert_to_bool)
/*
Scan an expression argument for the given attribute with the given initial
precedence.  If an error occurs, set ap->kind to ak_unrecognized and return
NULL.  Otherwise, return a pointer to the argument's representation.
evaluated is TRUE if the expression is potentially-evaluated; if FALSE, the
expression is unevaluated (like a sizeof operand).  convert_to_bool is TRUE if
the expression is a predicate that must be contextually converted to bool.
*/
{
  an_attribute_arg_ptr  aap = NULL;
  a_source_position     arg_pos = pos_curr_token;
  an_expr_node_ptr      expr;

  expr = scan_expr_for_attribute(precedence, evaluated, convert_to_bool);
  if (!is_error_node(expr)) {
    aap = alloc_attribute_arg();
    aap->kind = aak_expression;
    aap->position = arg_pos;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    aap->end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    aap->variant.expr = expr;
  } else {
    make_attr_unrecognized(ap);
  }  /* if */
  return aap;
}  /* scan_attr_expr_arg */


static an_attribute_arg_ptr scan_attr_integer_constant_arg(
                                                         an_attribute_ptr  ap)
/*
Scan a (possibly dependent) integer constant argument for the given attribute.
If an error occurs, set ap->kind to ak_unrecognized and return NULL.
Otherwise, return a pointer to the argument's representation.
*/
{
  an_attribute_arg_ptr  aap = NULL;
  a_constant_ptr        constant = local_constant();
  a_source_position     arg_pos;
  a_boolean             err = FALSE;

  arg_pos = pos_curr_token;
  scan_integral_constant_expression(constant);
  if (is_error_constant(constant)) {
    err = TRUE;
  } else if (constant->kind != (a_constant_repr_kind)ck_integer &&
             constant->kind != (a_constant_repr_kind)ck_template_param) {
    /* Most likely a multiple of the UPC THREADS constant. */
    pos_error(ec_exp_int_constant, &arg_pos);
    err = TRUE;
  } else {
    an_expr_node_ptr        expr = constant->expr;
    a_memory_region_number  region_to_switch_back_to;
    aap = alloc_attribute_arg();
    aap->kind = (an_attribute_arg_kind)aak_constant;
    aap->position = arg_pos;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    aap->end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    /* Make sure any constants are allocated in the file scope memory region,
       because they will be pointed to by *aap, which is in the file scope
       memory region. */
    switch_to_file_scope_region(&region_to_switch_back_to);
    aap->variant.constant = alloc_shareable_constant(constant);
    if (expr != NULL && aap->variant.constant->expr == NULL &&
        !aap->variant.constant->local_expr_ref &&
        innermost_function_scope != NULL) {
      /* The backing expression got dropped because of a memory region issue.
         Refer to it indirectly instead.  (If local_expr_ref is already set,
         fix_memory_region_problems_in_copied_constant has already set up
         the indirect reference.) */
      make_local_expr_node_ref(
                       expr, (a_local_expr_node_ref_kind)lerk_constant_expr,
                       (char*)aap->variant.constant, innermost_function_scope);
    }  /* if */
    switch_back_to_original_region(region_to_switch_back_to);
  }  /* if */
  if (err) {
    make_attr_unrecognized(ap);
  }  /* if */
  release_local_constant(&constant);
  return aap;
}  /* scan_attr_integer_constant_arg */


static an_attribute_arg_ptr scan_attr_string_arg(an_attribute_ptr  ap,
                                                 a_boolean         narrow_only)
/*
A string literal is expected next as an attribute argument (if narrow_only is
TRUE it must be a narrow string).  If that's the case, return an aak_constant
entry; otherwise, issue an error, set ap->kind to ak_unrecognized, and return
NULL.
*/
{
  an_attribute_arg_ptr  aap = NULL;
  size_t                require_matching = 0;

  if (ap->family == af_gnu) {
    while (curr_token == tok_lparen) {
      /* Ignore any extraneous parentheses on GNU-style attribute strings. */
      (void)get_token();
      require_matching++;
    }  /* while */
  }  /* if */
  if (curr_token == tok_string_literal) {
    if (const_for_curr_token.kind == (a_constant_repr_kind)ck_error) {
      /* A malformed string literal: An error has already been issued. */
      expect_error();
    } else if (!narrow_only ||
               is_ordinary_string_constant(&const_for_curr_token)) {
      a_memory_region_number  region_to_switch_back_to;
      aap = alloc_attribute_arg();
      aap->kind = (an_attribute_arg_kind)aak_constant;
      aap->position = pos_curr_token;
#if EXTRA_SOURCE_POSITIONS_IN_IL
      aap->end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      switch_to_file_scope_region(&region_to_switch_back_to);
      aap->variant.constant = alloc_shareable_constant(&const_for_curr_token);
      switch_back_to_original_region(region_to_switch_back_to);
    } else {
      /* A wide string literal where only a narrow one is expected:
         Issue an error. */
      pos_error(ec_wide_string_not_allowed, &pos_curr_token);
    }  /* if */
    (void)get_token();
    while (require_matching--) {
      (void)required_token(tok_rparen, ec_exp_rparen);
    }  /* while */
  } else {
    syntax_error(ec_exp_string_literal);
  }  /* if */
  if (aap == NULL) {
    make_attr_unrecognized(ap);
  }  /* if */
  return aap;
}  /* scan_attr_string_arg */


static an_attribute_arg_ptr scan_attr_identifier_arg(an_attribute_ptr  ap)
/*
An identifier is expected next as an attribute argument.  If that's the case,
return an aak_token entry; otherwise, issue an error, set ap->kind to
ak_unrecognized, and return NULL.
*/
{
  an_attribute_arg_ptr  aap = NULL;

  if (curr_token == tok_identifier || is_keyword_token(curr_token)) {
    aap = alloc_attribute_arg();
    aap->kind = (an_attribute_arg_kind)aak_token;
    aap->position = pos_curr_token;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    aap->end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    aap->token_kind = curr_token;
    aap->variant.token = il_string_for_curr_token();
    (void)get_token();
  } else {
    syntax_error(ec_exp_identifier);
    make_attr_unrecognized(ap);
  }  /* if */
  return aap;
}  /* scan_attr_identifier_arg */


static an_attribute_arg_ptr get_raw_token(void)
/*
Create and return an aak_raw_token entry for the current token.  The current
token is consumed by a call to get_token.
*/
{
  an_attribute_arg_ptr  aap = alloc_attribute_arg();

  check_assertion(curr_token != tok_newline);
  aap->kind = (an_attribute_arg_kind)aak_raw_token;
  aap->position = pos_curr_token;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  aap->end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  aap->token_kind = curr_token;
  aap->variant.token = il_string_for_curr_token();
  (void)get_token();
  return aap;
}  /* get_raw_token */


static an_attribute_arg_ptr get_balanced_token(
                                         an_attribute_arg_ptr  *unmatched_aap)
/*
The standard defines the non-terminal "balanced-token" as follows:
  balanced-token:
         ( balanced-token-seq )
         [ balanced-token-seq ]
         { balanced-token-seq }
         any token other than a parenthesis, a bracket, or a brace
Scan such a construct and return a list of aak_raw_token attribute argument
entries for the corresponding tokens.  If the balanced token is a "(", "[", or
"{" that has no matching closing delimiter, set *unmatched_aap to point to the
entry for the opening delimiter, unless *unmatched_aap already points to an
entry.  When reflection is enabled, the splice delimiters "[:" and ":]" are
also considered.
*/
{
  an_attribute_arg_ptr  aap = NULL;
  a_token_kind          closing_token;

  switch (curr_token) {
    case tok_end_of_source:
      expect_error();
      goto done;
    case tok_lparen:
      closing_token = tok_rparen;
      break;
    case tok_lbracket:
      closing_token = tok_rbracket;
      break;
    case tok_lbrace:
      closing_token = tok_rbrace;
      break;
    case tok_lsplice:
      closing_token = tok_rsplice;
      break;
    case tok_rparen:
    case tok_rbracket:
    case tok_rbrace:
    case tok_rsplice:
      goto done;
    case tok_identifier:
      if (locator_for_curr_id.symbol_header != NULL) {
        a_symbol  *sym = locator_for_curr_id.symbol_header->symbol;
        if (sym != NULL) {
          record_potential_pack_reference(sym, &pos_curr_token);
        }  /* if */
      }  /* if */
      FALLTHROUGH
    default:
      closing_token = tok_last;
      break;
  }  /* switch */
  aap = get_raw_token();
  if (closing_token != tok_last) {
    /* The balanced-token started with a left delimiter: Scan and record
       tokens until the matching closing delimiter (or tok_end_of_source). */
    an_attribute_arg_ptr  *p_aap = &aap->next;
    for (;;) {
      *p_aap = get_balanced_token(unmatched_aap);
      if (*p_aap == NULL) break;
      /* Keep p_aap pointing to the last "next" pointer. */
      do { p_aap = &(*p_aap)->next; } while (*p_aap != NULL);
    }  /* for */
    if (curr_token == closing_token) {
      *p_aap = get_raw_token();
    } else if (*unmatched_aap == NULL) {
      *unmatched_aap = aap;
    }  /* if */
  }  /* if */
done:
  return aap;
}  /* get_balanced_token */


static an_attribute_arg_ptr scan_attr_remaining_arg_tokens(an_attribute_ptr ap)
/*
Scan tokens until (but not including) a right parenthesis matching the left
parenthesis opening the argument list of the given attribute.  Return these
tokens as a list of aak_raw_token attribute argument entries.  (This is called
for attributes whose "signature string" ends in "*)".  That includes
unrecognized attributes.)  The sequence of raw token entries is terminated by
an aak_empty argument that holds the position of the subsequent token
(normally, a right parenthesis).  Scanning also stops at an unmatched right
parenthesis, bracket, or brace, or if tok_end_of_source is encountered: In
such cases an error is issued and ap->kind is set to ak_unrecognized.
*/
{
  an_attribute_arg_ptr  aap = NULL, unmatched_aap = NULL, *p_aap = &aap;

  /* Record tokens while keeping track of the number of parentheses, brackets,
     and braces.  Stop when encountering a right parenthesis, bracket, or
     brace not matching a recorded token. */
  for (;;) {
    *p_aap = get_balanced_token(&unmatched_aap);
    if (*p_aap == NULL) break;
    /* Keep p_aap pointing to the last "next" pointer. */
    do { p_aap = &(*p_aap)->next; } while (*p_aap != NULL);
  }  /* for */
  if (curr_token != tok_rparen && unmatched_aap == NULL) {
    unmatched_aap = aap;
  }  /* if */
  if (unmatched_aap != NULL) {
    pos_error(ec_unbalanced_attribute_argument, &unmatched_aap->position);
    make_attr_unrecognized(ap);
  }  /* if */
  /* Append an aak_empty argument to the sequence of raw tokens.  This is
     primarily useful for diagnostic purposes, by providing a record of the
     position of the subsequent token. */
  *p_aap = alloc_attribute_arg();
  (*p_aap)->kind = (an_attribute_arg_kind)aak_empty;
  (*p_aap)->position = pos_curr_token;
  /* Check that the token sequence was "balanced". */
  return aap;
}  /* scan_attr_remaining_arg_tokens */


static void scan_attr_arg_list(an_attribute_ptr  ap,
                               a_const_char      *sig)
/*
A non-empty attribute argument list for the given attribute is next.  The
current token is the first token after the left parenthesis, and sig points
to the first character after "(" in the given attribute's signature.  Scan
and record the argument list.  If there is an error, set ap->kind to
ak_unrecognized.
*/
{
  an_attribute_arg_ptr  *p_aap = &ap->arguments;
  a_const_char          *saved_sig;
  a_boolean             may_terminate, requirements_met = FALSE;

  /* Loop for each argument in the argument list. */
  do {
    a_pack_expansion_stack_entry_ptr  pesep;
    a_boolean                         any_more;
    /* Skip a "?" indicating that the argument list may terminate at this
       point. */
    if (*sig == '?') {
      may_terminate = TRUE;
      ++sig;
      if (*sig == ',') ++sig;
      if (curr_token == tok_rparen) break;
    } else {
      may_terminate = requirements_met;
    }  /* if */
    requirements_met = FALSE;
    saved_sig = sig;
    any_more = begin_potential_pack_expansion_context(&pesep);
    while (any_more) {
      switch (*sig++) {
        case 'c':
          /* Scan a constant argument that is not a string literal.  Currently
             only integral constants are supported (or needed). */
          if (*sig == 't' && is_decl_not_expr(DFS_ABSTRACT_DECLARATOR_ALLOWED |
                                              DFS_IS_SIZEOF |
                                              DFS_SINGLE_TYPE_REQUIRED)) {
            /* "ct" and what looks like a type-id follows. */
            *p_aap = scan_attr_type_arg(ap);
            ++sig;
          } else if (*sig == 't' || *sig == 'i') {
            /* "ct" on what appears to be an expression, or "ci". */ 
            *p_aap = scan_attr_integer_constant_arg(ap);
            ++sig;
          } else {
            check_attr_config(FALSE, ap,
                              "invalid attribute signature configuration");
          }  /* if */
          break;
        case 'n':
          /* Scan an identifier token and record it as an attribute
             argument */
          *p_aap = scan_attr_identifier_arg(ap);
          break;
        case 's':
          /* Scan a string literal as an attribute argument. */
          if (*sig == 'n') {
            /* Scan a narrow string literal. */
            *p_aap = scan_attr_string_arg(ap, /*narrow_only=*/TRUE);
            ++sig;
          } else if (*sig == 'x') {
            /* Scan any string literal. */
            *p_aap = scan_attr_string_arg(ap, /*narrow_only=*/FALSE);
            ++sig;
          } else {
            check_attr_config(FALSE, ap,
                              "invalid attribute signature configuration");
          }  /* if */
          break;
        case 't':
          /* Scan a type-id and record it as an attribute argument. */
          *p_aap = scan_attr_type_arg(ap);
          break;
        case 'X':
          /* Scan an expression. */
          { int        precedence = PREC_LOWEST;
            a_boolean  evaluated = FALSE;
            a_boolean  convert_to_bool = FALSE;
            if (*sig == 'c') {
              precedence = PREC_QUEST_MARK;
              ++sig;
            }  /* if */
            if (*sig == 'e') {
              evaluated = TRUE;
              ++sig;
            }  /* if */
            if (*sig == 'b') {
              convert_to_bool = TRUE;
              ++sig;
            }  /* if */
            *p_aap = scan_attr_expr_arg(ap, precedence, evaluated,
                                        convert_to_bool);
          }  /* if */
          break;
        case '*':
          /* Scan the remaining tokens (including commas) up until an unmatched
             parenthesis, bracket, or brace, and record each one as an
             attribute argument. */
          *p_aap = scan_attr_remaining_arg_tokens(ap);
          break;
        default:
          check_attr_config(FALSE, ap,
                            "invalid attribute signature configuration");
      }  /* switch */
      { a_pack_expansion_descr_ptr pedep;
        pedep = end_potential_pack_expansion_context(pesep,
                                                     /*is_declarator=*/FALSE);
        if (pedep != NULL && *p_aap != NULL) {
          (*p_aap)->pack_expansion_descr = pedep;
          (*p_aap)->is_pack_expansion = TRUE;
        }  /* if */
      }
      while (*p_aap != NULL) p_aap = &(*p_aap)->next;
      any_more = advance_to_next_pack_element(pesep);
      if (*sig == '+') {
        /* A repeated signature. */
        if (curr_token == tok_rparen) {
          /* If a right parenthesis is next, break out of this (otherwise
             infinite) loop. */
          ++sig;
        } else {
          /* Look for another argument with the same signature, but remember
             that at least one argument with the proper signature has been
             seen (in case there are no more, e.g., if a NULL parameter pack
             follows). */
          sig = saved_sig;
          requirements_met = TRUE;
        }  /* if */
      }  /* if */
      if (*sig == '?') {
        /* Skip a "?" at the end of an argument (indicating that the argument
           list may terminate at this point). */
        ++sig;
        may_terminate = TRUE;
      }  /* if */
      /* Go to next argument. */
      if (*sig == ',') ++sig;
      if (*sig == ')') {
        if (curr_token == tok_comma) {
          /* Something like attribute(0,).  Let the caller issue the error. */
          goto done;
        } else if (any_more) {
          abandon_potential_pack_expansion_context(pesep);
          str_error(ec_too_many_arguments_provided_for_attribute,
                    attribute_display_name(ap));
          goto done;
        }  /* if */
      }  /* if */
    }  /* while */
  } while (loop_token(tok_comma));
  if (*sig == ')') {
    /* Signature expects a right parenthesis at this location. */
    may_terminate = TRUE;
  }  /* if */
  if (!may_terminate) {
    /* More arguments were expected. */
    pos_st_error(ec_missing_attribute_arguments, &pos_curr_token,
                 attribute_display_name(ap));
    make_attr_unrecognized(ap);
  }  /* if */
done:;
}  /* scan_attr_arg_list */


static void scan_attribute_args(an_attribute_ptr  ap,
                                a_const_char      *sig)
/*
Scan a parenthesized list of attribute arguments (if one is present) for the
given attribute.  sig is a string describing the structure of the expected
arguments (if any).  Diagnostics will be emitted if the actual arguments do
not match the pattern indicated by sig; in that case, ap->kind is set to
ak_unrecognized.
*/
{
  a_source_position  lparen_pos;

  add_stop_token(tok_rparen);
  if (curr_token == tok_lparen) {
    lparen_pos = pos_curr_token;
    /* An argument list appears to follow.  Parse it and check it against
       sig. */
    if (*sig == '\0') {
      /* No arguments are allowed on this attribute.  Scan the unexpected list
         as if the signature were "(*)". */
      str_error(ec_arguments_provided_for_attribute,
                attribute_display_name(ap));
      make_attr_unrecognized(ap);
      sig = "(*)";
    }  /* if */
    /* Skip a leading '?' indicating that the argument list was optional. */
    if (*sig == '?') ++sig;
    check_attr_config(*sig == '(', ap,
                      "invalid attribute signature configuration");
    ++sig;
    /* Skip over the left parenthesis. */
    (void)get_token();
    /* Scan the non-empty argument list. */
    scan_attr_arg_list(ap, sig);
    if (ap->arguments == NULL) {
      /* An empty attribute argument "()". */
      record_empty_attribute_argument(ap, sig, &lparen_pos);
    }  /* if */
    (void)required_token(tok_rparen, ec_exp_rparen);
  } else if (*sig == '(') {
    /* No arguments are present, but sig indicates that arguments are not
       optional.  Issue a syntax error. */
    syntax_error(ec_exp_lparen);
    make_attr_unrecognized(ap);
  } else {
    check_attr_config(*sig == '\0' || *sig == '?', ap,
                      "invalid attribute signature configuration");
    if (curr_token == tok_ellipsis) {
      pos_error(ec_expansion_contains_no_packs, &pos_curr_token);
    }  /* if */
  }  /* if */
  remove_stop_token(tok_rparen);
}  /* scan_attribute_args */


a_boolean equivalent_attributes(an_attribute_ptr  ap1,
                                an_attribute_ptr  ap2,
                                a_boolean         ignore_family)
/*
Return TRUE if the two given attributes are equivalent.  Two attributes are
equivalent if they have the same kind, equal arguments (if any), and if they
are of the same family (af_std, af_gnu, ...).  If ignore_family is TRUE, the
families need not be equal.
*/
{
  a_boolean  result = FALSE;

  if (ap1->kind == ap2->kind &&
      (ignore_family || ap1->family == ap2->family)) {
    /* If the arguments to the attributes are equal, the attributes are
       equivalent. */
    an_attribute_arg_ptr  aap1 = ap1->arguments, aap2 = ap2->arguments;
    result = TRUE;
    while (aap1 != NULL && aap2 != NULL && result) {
      if (aap1->kind != aap2->kind) {
        result = FALSE;
        break;
      } else {
        switch (aap1->kind) {
          case aak_empty:
          case aak_last:
            break;
          case aak_raw_token:
          case aak_token:
            result = (strcmp(aap1->variant.token, aap2->variant.token) == 0);
            break;
          case aak_constant:
            result = eq_constants(aap1->variant.constant,
                                  aap2->variant.constant);
            break;
          case aak_type:
            result = identical_types(aap1->variant.type, aap2->variant.type);
            break;
          case aak_expression:
            result = compare_expressions(expr_node_from_attribute_arg(aap1),
                                         expr_node_from_attribute_arg(aap2),
                                         CC_NO_OPTIONS);
            break;
          default_is_unexpected();
        }  /* switch */
      }  /* if */
      aap1 = aap1->next;
      aap2 = aap2->next;
    }  /* while */
    /* If all arguments up to now are equal, but one list has additional
       arguments, the attributes are not equivalent. */
    if (result && (aap1 != NULL || aap2 != NULL)) result = FALSE;
  }  /* if */
  return result;
}  /* equivalent_attributes */


an_attribute_ptr *f_last_attribute_link(an_attribute_ptr  *attributes)
/*
Return the address of the last "next" pointer in the list given by *attributes.
(If *attributes is NULL, return attributes.)  If attributes itself is NULL,
then return NULL.
*/
{
  if (attributes != NULL) {
    attributes = get_last_simple_list_link(attributes);
  }  /* if */
  return attributes;
}  /* f_last_attribute_link */


static an_attribute_ptr make_attribute(an_attribute_family  family)
/*
Allocate and return an attribute of the given family.  Set its position to
that of the current token.
*/
{
  an_attribute_ptr  ap = alloc_attribute();

  ap->family = family;
  ap->position = pos_curr_token;
  return ap;
}  /* make_attribute */


a_boolean is_valid_attribute_identifier(a_token_kind  tok)
/*
Return TRUE if the given token can be used as a standard attribute name or
attribute namespace.
*/
{
  return tok == tok_identifier || is_keyword_token(tok);
}  /* is_valid_attribute_identifier */


static void record_attribute_name(an_attribute_ptr  ap)
/*
The current token is an attribute name (or perhaps an attribute-namespace
name).  Record the source form of the token as a null-terminated character
string in ap->name (if needed, it will be moved to ap->namespace_name by the
caller), making sure that if a name appears multiple times in the translation
unit, the same string is reused in all cases.  Also update the end position
of the attribute to be that of the current token (in configurations that
track end positions).
*/
{
  check_assertion(is_valid_attribute_identifier(curr_token) ||
                  (curr_token == tok_assign && ap->kind == ak_annotation));
  if (curr_token == tok_restrict) {
    /* The general mechanism for turning a tok_restrict into a string
       won't work in cases where SUPPRESS_RESTRICT_IN_GENERATED_CODE is TRUE,
       so handle that as a special case here. */
    ap->name = copy_string_to_region(file_scope_region_number, "restrict");
  } else if (ap->family == af_alignas && C_mode()) {
    /* In C mode, the "alignas" attribute is spelled "_Alignas". */
    ap->name = copy_string_to_region(file_scope_region_number, "_Alignas");
  } else {
    /* Record the attribute name as an IL string. */
    ap->name = il_string_for_curr_token();
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  ap->end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* record_attribute_name */


an_attribute_ptr scan_attribute(an_attribute_family  af,
                                an_attribute_ptr     using_ns_ap)
/*
Scan a standard attribute of one of the following forms
    <identifier>
    <identifier> ( <arg-list> )
and return a pointer to its representation (or NULL in severe error cases).
If af is af_std, the following qualified forms are also accepted
    <identifier> :: <identifier>
    <identifier> :: <identifier> ( <arg-list> )

<identifier> in this context includes keywords.  af indicates which syntax
the attribute is declared with.  If using_ns_ap is non-NULL, it indicates the
attribute namespace name specified by a preceding "using" prefix that is to
be used as the implicit attribute namespace name for this attribute.
*/
{
  an_attribute_ptr   ap = NULL;
  a_boolean          err = FALSE;

  if (!is_valid_attribute_identifier(curr_token)) {
    syntax_error(ec_exp_identifier);
  } else {
    an_attr_descr_ptr  adp;
    a_const_char       *sig;
    ap = make_attribute(af);
    record_attribute_name(ap);
    (void)get_token();
    /* Look for a "::".  In most cases that will be a single tok_colon_colon
       token, but in pre-C23 modes where GNU and Clang accept standard
       attribute syntax, that would appear as two tok_colon tokens. */
    if (af == af_std && curr_token == tok_colon && C_mode() && !c23_mode) {
      add_colon_to_tok_colon_if_present();
    }  /* if */
    if (af == af_std && curr_token == tok_colon_colon) {
      /* The previous name was the attribute namespace name.  The attribute
         name proper should follow the "::". */
      (void)get_token();
      if (!is_valid_attribute_identifier(curr_token)) {
        syntax_error(ec_exp_identifier);
      } else {
        if (using_ns_ap != NULL) {
          /* A "using" prefix has been specified, so namespaces may not appear
             on any attribute on this list. */
          pos_error(ec_namespace_not_allowed, &ap->position);
          ap = NULL;
          err = TRUE;
        } else {
          /* Re-purpose the "namespace" attribute that had been scanned as
             the attribute (copying the name to namespace_name). */
          check_for_unrecognized_attribute_namespace(ap);
          ap->namespace_name = ap->name;
          ap->name = NULL;
          record_attribute_name(ap);
        }  /* if */
        (void)get_token();
      }  /* if */
    } else if (using_ns_ap != NULL) {
      /* A "using" prefix was specified; apply that namespace name to this
         attribute. */
      ap->namespace_name = using_ns_ap->name;
      ap->namespace_from_using = TRUE;
      ap->is_invalid_namespace = using_ns_ap->is_invalid_namespace;
    }  /* if */
    if (!err) {
      adp = get_attr_descr_for_attribute(ap);
      if (adp == NULL) {
        /* An unrecognized attribute.  Use "?(*)" as its signature, indicating
           that an argument list is optional, and if it is present, it will
           just be recorded as a sequence of tokens. */
        sig = "?(*)";
      } else {
        /* The attribute was recognized: Retrieve its signature from its
           description entry. */
        sig = adp->sig;
      }  /* if */
      scan_attribute_args(ap, sig);
      if (adp != NULL) {
        /* Check for duplicate attributes. */
        if ((attr_family_seen[ap->kind] & (1 << ap->family)) == 0) {
          attr_family_seen[ap->kind] |= 1 << ap->family;
        } else {
          /* A duplicate attribute kind.  Look at the cond adp->string to see
             if that is disallowed. */
          if (adp->cond[0] == '1') {
            err = TRUE;
            make_attr_unrecognized(ap);
          }  /* if */
          pos_diagnostic(err ? es_error : es_remark, ec_attr_twice_in_group,
                         &ap->position);
        }  /* if */
      } else if (!record_unrecognized_attributes ||
                 ap->family == af_ms_declspec) {
        /* If we are not recording unrecognized attributes, drop unrecognized
           attributes with a warning.  Always issue a discretionary error for
           unrecognized Microsoft __declspec attributes. */
        an_error_severity  sev = es_warning;
        if (ap->family == af_ms_declspec) {
          sev = es_discretionary_error;
        }  /* if */
        if (ap->is_invalid_namespace) {
          /* If we've already diagnosed the namespace as being unrecognized,
             it's probably not helpful to give a second warning, so downgrade
             this to a remark. */
          sev = es_remark;
        }  /* if */
        pos_st_diagnostic(sev, ec_unrecognized_attribute, &ap->position,
                          attribute_display_name(ap));
        if (!record_unrecognized_attributes) ap = NULL;
      }  /* if */
    }  /* if */
  }  /* if */
  return ap;
}  /* scan_attribute */


an_attribute_ptr make_module_attribute(a_const_char        *name,
                                       an_attribute_family family,
                                       an_attribute_ptr    next)
/*
Utility to create an attribute with the specified name and family.  Used to
create an_attribute entries for entities defined in modules.  The newly-
created attribute is returned (and its next pointer is set to the value of
the "next" argument).  Note that this routine assumes there is no source
location or arguments for the attribute.
*/
{
  an_attribute_ptr   ap;

  ap = make_attribute((an_attribute_family)family);
  ap->position = null_source_position;
  ap->next = next;
  ap->name = copy_string_to_region(file_scope_region_number, name);
  check_assertion(get_attr_descr_for_attribute(ap) != NULL);
  return ap;
}  /* make_module_attribute */


static an_attribute_ptr scan_annotation(void)
/*
Scan an annotation of the form

	= constant-expression

and return an ak_annotation attribute with the result constant as an
argument.
*/
{
  an_attribute_ptr   ap = make_attribute(af_std);

  ap->kind = ak_annotation;
  record_attribute_name(ap);
  (void)get_token();
  ap->arguments = alloc_attribute_arg();
  scan_annotation_value(ap->arguments);
  return ap;
}  /* scan_annotation */


static an_attribute_ptr scan_attributes_list(an_attribute_location loc,
                                             an_attribute_family   af,
                                             a_token_kind          end_token,
                                             an_attribute_ptr      using_ns_ap)
/*
Scan a comma-separated list of attributes of the given family.  end_token is
the token kind that terminates the list (that final token, which should be in
the stop tokens set, is not considered part of the list and is therefore not
consumed).  loc describes the syntactic location in which the attributes
appear.  If using_ns_ap is non-NULL, it specifies the attribute namespace
that appeared in a previous "using" prefix.  Can return NULL on error.
*/
{
  an_attribute_ptr   attributes = NULL, *p_attribute = &attributes, ap;

  for (;;) {
    a_pack_expansion_stack_entry_ptr pesep;
    a_boolean                        any_more;
    any_more = begin_potential_pack_expansion_context(&pesep);
    /* Loop for a variadic template pack expansion. */
    while (any_more) {
      if (curr_token == end_token || curr_token == tok_comma) {
        /* An empty attribute: Create a placeholder attribute entry for it. */
        *p_attribute = make_attribute(af);
        (*p_attribute)->kind = ak_empty_attr;
#if EXTRA_SOURCE_POSITIONS_IN_IL
        (*p_attribute)->end_position = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      } else if (curr_token == tok_string_literal &&
                 microsoft_mode && microsoft_version >= 1700 &&
                 af == (an_attribute_family)af_ms_declspec) {
        /* Microsoft permits attributes like:
	     void g(__declspec("SAL_pre SAL_valid") char *);
           The normal Microsoft compiler ignores these, but certain tools
           recognize them.  We just record them as "unrecognized"
           attributes. */
        *p_attribute = make_attribute(af);
        (*p_attribute)->kind = ak_unrecognized;
        if (const_for_curr_token.kind == (a_constant_repr_kind)ck_error) {
          /* A malformed string literal: An error has already been issued. */
          expect_error();
        } else if (!is_ordinary_string_constant(&const_for_curr_token)) {
           pos_error(ec_wide_string_not_allowed, &pos_curr_token);
        } else {
          a_string_view const_str(
                   const_for_curr_token.variant.string.value,
                   size_t_arg(const_for_curr_token.variant.string.length - 1));
          Small_string<MAX_ATTRIBUTE_NAME_LENGTH + 2>
                        name("\"", const_str, "\"");

          /* Create a name that includes the quotation characters. */
          (*p_attribute)->name =
                               name.to_allocated_storage(IL_allocator<char>());
        }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
        (*p_attribute)->end_position = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        /* Skip the string literal. */
        (void)get_token();
      } else if (reflection_enabled && curr_token == tok_assign) {
        if (using_ns_ap != NULL) {
          pos_error(ec_annotation_after_using, &pos_curr_token);
        }  /* if */
        *p_attribute = scan_annotation();
      } else {
        *p_attribute = scan_attribute(af, using_ns_ap);
      }  /* if */
      { a_pack_expansion_descr_ptr pedep;
        if (pesep != NULL &&
            (*p_attribute == NULL || is_unrecognized_attr(*p_attribute))) {
          /* For an unknown attribute, we do not know if packs encountered
             within the attribute arguments were expanded within the pack or
             should be expanded with the attribute as a whole.  By recording
             the presence of an ellipsis, we avoid spurious diagnostics. */
          record_pack_expansion_ellipsis_position(&pos_curr_token);
          pesep->expansion_with_no_packs_diagnostic_issued = TRUE;
        }  /* if */
        pedep = end_potential_pack_expansion_context(pesep,
                                                     /*is_declarator=*/FALSE);
        if (pedep != NULL) {
          if (*p_attribute == NULL) {
            /* In error cases, scan_attribute may not have produced an
               attribute entry.  Alternatively, we may have run into an
               unrecognized attribute. */
            check_assertion_or_expect_error(!record_unrecognized_attributes);
          } else {
            (*p_attribute)->is_pack_expansion = TRUE;
            (*p_attribute)->pack_expansion_descr = pedep;
          }  /* if */
        }  /* if */
      }
      any_more = advance_to_next_pack_element(pesep);
      p_attribute = last_attribute_link(p_attribute);
    }  /* while */
    if (curr_token == end_token) {
      break;
    } else if ((curr_token == tok_identifier ||
                curr_token == tok_string_literal) &&
               af == af_ms_declspec) {
      /* A comma is optional when separating __declspec attributes.  I.e.,
         __declspec(naked noalias) and __declspec(naked, noalias) are
         equivalent. */
    } else {
      if (curr_token != tok_comma) {
        /* Skip tokens until we reach the start of the next attribute, or the
           end of the attribute list. */
        add_stop_token(tok_comma);
        syntax_error(ec_exp_comma);
        remove_stop_token(tok_comma);
      }  /* if */
      if (curr_token == tok_comma) {
        (void)get_token();
      } else {
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  for (ap = attributes; ap != NULL; ap = ap->next) {
    ap->syntactic_location = loc;
    /* Clear the attr_family_seen array. */
    reset_attr_family_seen(ap);
  }  /* for */
  return attributes;
}  /* scan_attributes_list */


static void make_attribute_group(an_attribute_ptr   ap,
                                 a_source_position  *group_pos)
/*
Create an attribute group for the given list of attributes and record the
given position for that group.  The current token is the last token of the
attribute group construct.  The list can be NULL, in which case this routine
performs no action.
*/
{
  if (ap != NULL) {
    an_attribute_group_ptr  group = alloc_attribute_group();
    group->position = *group_pos;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    group->end_position = end_pos_curr_token;
    curr_construct_end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    end_position_of_attributes = pos_curr_token;
    last_token_number_of_attributes = curr_token_sequence_number;
    for (; ap != NULL; ap = ap->next) {
      ap->group = group;
    }  /* for */
  }  /* if */
}  /* make_attribute_group */


STATIC_THREAD a_boolean
		already_diagnosed_using;
			/* Flag indicating whether a non-standard
			   attribute-using-prefix has already been
			   diagnosed. */


static an_attribute_ptr scan_std_attribute_group(an_attribute_location  loc)
/*
Scan a standard attribute group of the form
    [ [  <attribute-list>  ] ]
      or
    [ [  using attribute-namespace : <attribute-list>  ] ]
<attribute-list> is a possibly empty list of attributes.  The attribute list
can also contain "empty attributes" (e.g., [[,,,]] ).  loc is the syntactic
location in which the group appears.
*/
{
  an_attribute_ptr   attributes = NULL, using_ns_ap = NULL;
  a_source_position  group_pos;

  group_pos = pos_curr_token;
  report_gnu_cpp11_extension_if_needed(&group_pos, ec_std_attributes_is_cpp11);
  check_assertion(curr_token == tok_lbracket);
  (void)get_token();
  check_assertion(curr_token == tok_lbracket);
  (void)get_token();
  add_stop_token(tok_rbracket);
  if (curr_token == tok_using &&
      using_attribute_namespaces_enabled &&
      is_valid_attribute_identifier(next_token())) {
    /* A "using" prefix; scan the attribute namespace name and use it as
       the namespace for all attributes in this group.  If the "using" token
       appears without a following identifier, assume "using" is an
       implementation-defined attribute in such cases. */
    (void)get_token();
    if (!is_valid_attribute_identifier(curr_token)) {
      syntax_error(ec_exp_identifier);
    } else {
      /* Use a separate attribute to indicate the presence of a "using"
         prefix. */
      if (!cpp17_mode && gpp_mode) {
        if (!already_diagnosed_using && !in_system_header()) {
          pos_warning(ec_using_attribute_nonstandard, &pos_curr_token);
          already_diagnosed_using = TRUE;
        }  /* if */
      }  /* if */
      using_ns_ap = make_attribute(af_std);
      using_ns_ap->kind = ak_attr_using_prefix;
      using_ns_ap->syntactic_location = loc;
      record_attribute_name(using_ns_ap);
      check_for_unrecognized_attribute_namespace(using_ns_ap);
      (void)get_token();
      if (curr_token == tok_rsplice) {
        /* Something like [ [using edg:] ].  Consume the ":]" token and skip
           to the final bracket. */
        (void)get_token();
        goto final_closing_bracket;
      } else {
        (void)required_token(tok_colon, ec_exp_colon);
      }  /* if */
    }  /* if */
  }  /* if */
  attributes = scan_attributes_list(loc, af_std, tok_rbracket, using_ns_ap);
  (void)required_token(tok_rbracket, ec_exp_rbracket);
final_closing_bracket:
  (void)required_token(tok_rbracket, ec_exp_rbracket);
  remove_stop_token(tok_rbracket);
  if (using_ns_ap != NULL) {
    /* Add the "using" prefix attribute to the beginning of the list. */
    using_ns_ap->next = attributes;
    attributes = using_ns_ap;
  }  /* if */
  make_attribute_group(attributes, &group_pos);
  return attributes;
}  /* scan_std_attribute_group */


static an_attribute_ptr scan_alignas_construct(an_attribute_location  loc)
/*
Scan either
  alignas ( type-id ...opt )
or
  alignas ( constant-expr ...opt )
and return an attribute description that is equivalent to that of the
corresponding [[ align(...) ]] construct, except that the attribute family is
af_alignas and the attribute name is "alignas".
*/
{
  an_attribute_ptr   ap = make_attribute((an_attribute_family)af_alignas);
  a_source_position  group_pos;

  check_assertion(curr_token == tok_alignas);
  check_for_c23_deprecation("_Alignas", ec_c23_alignas_deprecated);
  ap->kind = ak_align;
  record_attribute_name(ap);
  ap->syntactic_location = loc;
  group_pos = pos_curr_token;
  /* Skip over "alignas". */
  (void)get_token();
  /* Use the general attribute argument scanning framework to scan a type or
     constant. */
  scan_attribute_args(ap, "(?ct+)");
  make_attribute_group(ap, &group_pos);
  attr_family_seen[ap->kind] |= 1 << ap->family;
  return ap;
}  /* scan_alignas_construct */


static an_attribute_ptr scan_gnu_attribute_group(an_attribute_location  loc)
/*
Scan a GNU attribute group of the form
    __attribute ( (  <attribute-list>  ) )
<attribute-list> is a possibly empty list of attributes.  The attribute list
can also contain "empty attributes" (e.g., __attribute((,,,)) ).  loc is the
syntactic location in which the group appears.
*/
{
  an_attribute_ptr   attributes = NULL;
  a_source_position  group_pos;

  check_assertion(curr_token == tok_attribute);
  group_pos = pos_curr_token;
  report_gnu_extension_if_needed(&pos_curr_token,
                                 ec_attribute_is_gnu_extension);
  /* Skip over "__attribute". */
  (void)get_token();
  /* There should now be two left parens. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_stop_token(tok_rparen);
  attributes = scan_attributes_list(loc, af_gnu, tok_rparen,
                                    (an_attribute_ptr)NULL);
  (void)required_token(tok_rparen, ec_exp_rparen);
  make_attribute_group(attributes, &group_pos);
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_stop_token(tok_rparen);
  return attributes;
}  /* scan_gnu_attribute_group */


static an_attribute_ptr scan_ms_declspec_group(an_attribute_location  loc)
/*
Scan a Microsoft attribute group of the form
    __declspec (  <attribute-list>  )
<attribute-list> is a possibly empty list of attributes.  Unlike standard and
GNU attributes, a comma separating attributes is optional.  The attribute list
can also contain "empty attributes" (e.g., __declspec((,,,)) ).  loc is the
syntactic location in which the group appears.
*/
{
  an_attribute_ptr   attributes = NULL;
  a_source_position  group_pos;

  check_assertion(curr_token == tok_declspec);
  group_pos = pos_curr_token;
  /* Skip over "__declspec". */
  (void)get_token();
  /* There should now be a left parenthesis. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_stop_token(tok_rparen);
  attributes = scan_attributes_list(loc, af_ms_declspec, tok_rparen,
                                    (an_attribute_ptr)NULL);
  make_attribute_group(attributes, &group_pos);
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_stop_token(tok_rparen);
  return attributes;
}  /* scan_ms_declspec_group */


an_attribute_ptr scan_attributes(an_attribute_location  loc)
/*
Scan any attributes that are next (usually, this means "next in the token
stream", but if there are "unscanned" attributes, return those instead; see
unscan_attributes).  loc indicates the syntactic context in which the call
is made.
*/
{
  an_attribute_ptr  attributes = NULL, *p_attributes = &attributes;

  if (unscanned_attributes != NULL) {
    /* Return previously scanned attributes. */
    an_attribute_ptr  ap;
    attributes = unscanned_attributes;
    /* Update the syntactic location to the given one. */
    for (ap = attributes; ap != NULL; ap = ap->next) {
      ap->syntactic_location = loc;
    }  /* for */
    unscanned_attributes = NULL;
  } else {
    a_boolean  new_attr_seen;
    do {
      new_attr_seen = FALSE;
      if (std_attribute_tokens_next()) {
        /* Two brackets are next: Those must be introducing a standard
           attribute construct. */
        *p_attributes = scan_std_attribute_group((an_attribute_location)loc);
        new_attr_seen = TRUE;
      } else if (curr_token == tok_alignas) {
        *p_attributes = scan_alignas_construct((an_attribute_location)loc);
        new_attr_seen = TRUE;
      } else if (curr_token == tok_attribute && gnu_attributes_enabled) {
        *p_attributes = scan_gnu_attribute_group((an_attribute_location)loc);
        new_attr_seen = TRUE;
      } else if (curr_token == tok_declspec &&
                 ms_declspec_attributes_enabled) {
        /* Microsoft __declspec attributes are allowed only in a few syntactic
           contexts. */
        if (loc == al_prefix || loc == al_specifier || loc == al_tag_name ||
            (loc == al_post_func &&
             scope_stack_top().decl_parse_state != NULL &&
             scope_stack_top().decl_parse_state->is_lambda)) {
          *p_attributes = scan_ms_declspec_group((an_attribute_location)loc);
          new_attr_seen = TRUE;
        }  /* if */
      }  /* if */
      p_attributes = last_attribute_link(p_attributes);
    } while (new_attr_seen);
  }  /* if */
  return attributes;
}  /* scan_attributes */


void scan_and_discard_attributes(an_error_severity sev,
                                 an_error_code     err_code)
/*
If any attributes exist at the current position, scan and discard them
reporting the specified error with the specified severity.  A single message
is given though multiple attributes may be dropped.
*/
{
  an_attribute_ptr ap = scan_attributes(al_implicit);
  if (ap != NULL) {
    pos_diagnostic(sev, err_code, &ap->position);
  }  /* if */
}  /* scan_and_discard_attributes */


an_attribute_ptr scan_gnu_attribute_groups(an_attribute_location  loc)
/*
This routine is similar to scan_attributes, except that only GNU attribute
groups are scanned and it may not be called when there are "unscanned"
attributes.
*/
{
  an_attribute_ptr  attributes = NULL, *p_attributes = &attributes;

  check_assertion(unscanned_attributes == NULL);
  if (gnu_attributes_enabled) {
    while (curr_token == tok_attribute) {
      p_attributes = last_attribute_link(p_attributes);
      *p_attributes = scan_gnu_attribute_group((an_attribute_location)loc);
    }  /* while */
  }  /* if */
  return attributes;
}  /* scan_gnu_attribute_groups */

#if CHECKING

a_boolean unscanned_attributes_pending(void)
/*
Return TRUE if there are unscanned attributes pending.
*/
{
  return unscanned_attributes != NULL;
}  /* unscanned_attributes_pending */

#endif /* CHECKING */

void unscan_attributes(an_attribute_ptr  attributes)
/*
The given list of attributes was returned by a call to scan_attributes, but
it now appears it doesn't apply to the current context.  E.g., we may be
parsing a statement starting with a set of attributes, but if the statement
is a declaration, the attributes should really be scanned by declaration
processing.  Record the given pointer to be returned by the next call to
scan_attributes.
*/
{
  check_assertion(unscanned_attributes == NULL);
  unscanned_attributes = attributes;
}  /* unscan_attributes */


void skip_over_attributes(void)
/*
If attributes are ahead in the token stream, skip over them.  This is used
in contexts where attributes have no effect (the caller will issue a warning)
or during disambiguation lookahead (e.g., while determining the nature of a
template).
*/
{
  for (;;) {
    if (std_attribute_tokens_next()) {
      /* Skip over standard attributes. */
      flush_until_matching_token_full(/*limit_flush=*/FALSE);
      if (curr_token == tok_rbracket) (void)get_token();
    } else if (curr_token == tok_attribute && gnu_attributes_enabled) {
      /* Skip over GNU attributes. */
      (void)get_token();
      if (curr_token == tok_lparen) {
        flush_until_matching_token_full(/*limit_flush=*/FALSE);
        if (curr_token == tok_rparen) (void)get_token();
      }  /* if */
    } else if (curr_token == tok_declspec && ms_declspec_attributes_enabled) {
      /* Skip over Microsoft __declspec attributes. */
      (void)get_token();
      if (curr_token == tok_lparen) {
        flush_until_matching_token_full(/*limit_flush=*/FALSE);
        if (curr_token == tok_rparen) (void)get_token();
      }  /* if */
    } else if (curr_token == tok_alignas) {
      /* Skip over alignas attributes. */
      (void)get_token();
      if (curr_token == tok_lparen) {
        flush_until_matching_token_full(/*limit_flush=*/FALSE);
        if (curr_token == tok_rparen) (void)get_token();
      }  /* if */
    } else {
      break;
    }  /* if */
  }  /* for */
}  /* skip_over_attributes */


void report_bad_attribute_target(an_error_severity  sev,
                                 an_attribute_ptr   ap)
/*
Issue a diagnostic with the given severity indicating that the given attribute
doesn't apply to the entity on which it is specified.  The given attribute is
turned into an ak_unrecognized attribute.
*/
{
  if (ap->family == af_alignas) {
    pos_diagnostic(sev, ec_wrong_entity_for_alignas, &ap->position);
  } else {
    pos_st_diagnostic(sev, ec_wrong_entity_for_attribute, &ap->position,
                      attribute_display_name(ap));
  }  /* if */
  make_attr_unrecognized(ap);
}  /* report_bad_attribute_target */


static void report_bad_attribute_arg(an_attribute_arg_ptr  aap,
                                     an_attribute_ptr      ap)
/*
The given argument of the given attribute is invalid.  Issue an error and turn
the given attribute into an ak_unrecognized attribute.
*/
{
  pos_st_error(ec_invalid_argument_to_attribute, &aap->position,
               attribute_display_name(ap));
  make_attr_unrecognized(ap);
}  /* report_bad_attribute_arg */


static void check_simple_type_constraints(a_const_char      *constr,
                                          an_attribute_ptr  ap,
                                          a_type_ptr        type)
/*
constr encodes a simple target constraint for a type.  Check that the
attribute ap applied to the given type matches those constraints.
*/
{
  check_attr_config(constr[0] == 'T' || constr[0] == 't' || constr[0] == 'c' ||
                    constr[0] == 'e',
                    ap, "invalid attribute constraint configuration");
  if (constr[1] == ':') {
    /* Type property switches follow.  E.g., "t:-f" indicates the type cannot
       be a function type. */
    an_error_code  err = ec_no_error;
    constr += 2;
    for(;;) {
      if (*constr == '\0' || *constr == '|') break;
      check_attr_config(constr[0] == '-' || constr[0] == '+',
                        ap, "invalid attribute constraint configuration");
      if (constr[1] == 'f') {
        /* Check for function types. */
        if (is_function_type(type)) {
          if (constr[0] == '-') {
            err = ec_attr_disallows_function_type;
          }  /* if */
        } else {
          if (constr[0] == '+') {
            err = ec_attr_requires_function_type;
          }  /* if */
        }  /* if */
        constr += 2;
      } else if (is_tag_attribute(ap) && constr[1] == 'd') {
        /* A tag definition is required or disallowed. */
        if (ap->on_primary_declaration) {
          if (constr[0] == '-') {
            err = ec_attr_disallows_definition;
          }  /* if */
        } else {
          if (constr[0] == '+') {
            err = ec_attr_requires_definition;
          }  /* if */
        }  /* if */
        constr += 2;
      } else {
        unexpected_condition_str2(
           "invalid property code for constraint configuration of attribute",
           attribute_display_name(ap));
      }  /* if */
      if (err != ec_no_error) break;
      if (*constr == '!') {
        ++constr;
      }  /* if */
    }  /* for */
    if (err != ec_no_error) {
      /* Issue the diagnostic. */
      an_error_severity  sev = *constr == '!' ? es_error : es_warning;
      pos_st_diagnostic(sev, err, &ap->position, attribute_display_name(ap));
      /* Treat the attribute as unrecognized for error recovery purposes. */
      make_attr_unrecognized(ap);
    }  /* if */
  }  /* if */
}  /* check_simple_type_constraints */


static void check_simple_field_constraints(a_const_char      *constr,
                                           an_attribute_ptr  ap,
                                           a_field_ptr       field)
/*
constr encodes a simple target constraint for a field.  Check that the
attribute ap applied to the given field matches those constraints.
*/
{
  check_assertion(constr[0] == 'd');
  if (constr[1] == ':') {
    /* Field property switches follow.  E.g., "d:-b" indicates the field cannot
       be a bit field. */
    an_error_code  err = ec_no_error;
    constr += 2;
    for (;;) {
      if (*constr == '\0' || *constr == '|') break;
      check_attr_config(constr[0] == '-' || constr[0] == '+',
                        ap, "invalid attribute constraint configuration");
      if (constr[1] == 'b') {
        /* Check for bit-fields. */
        if (field->is_bit_field) {
          if (constr[0] == '-') {
            err = ec_attr_disallows_bit_field;
          }  /* if */
        } else {
          if (constr[0] == '+') {
            err = ec_attr_requires_bit_field;
          }  /* if */
        }  /* if */
        constr += 2;
      } else {
        unexpected_condition_str2(
           "invalid property code for constraint configuration of attribute",
           attribute_display_name(ap));
      }  /* if */
      if (err != ec_no_error) break;
      if (*constr == '!') {
        ++constr;
      }  /* if */
    }  /* for */
    if (err) {
      /* Treat the attribute as unrecognized for error recovery purposes. */
      make_attr_unrecognized(ap);
    }  /* if */
    if (err != ec_no_error) {
      /* Issue the diagnostic. */
      an_error_severity  sev = *constr == '!' ? es_error : es_warning;
      pos_st_diagnostic(sev, err, &ap->position, attribute_display_name(ap));
      /* Treat the attribute as unrecognized for error recovery purposes. */
      make_attr_unrecognized(ap);
    }  /* if */
  }  /* if */
}  /* check_simple_field_constraints */


static void check_simple_routine_constraints(a_const_char      *constr,
                                             an_attribute_ptr  ap,
                                             a_routine_ptr     routine)
/*
constr encodes a simple target constraint for a routine.  Check that the
attribute ap applied to the given routine matches those constraints.
*/
{
  check_assertion(constr[0] == 'r');
  if (constr[1] == ':') {
    /* Routine property switches follow.  E.g., "r:-m" indicates the routine
       cannot be a class member. */
    an_error_code  err = ec_no_error;
    constr += 2;
    for (;;) {
      if (*constr == '\0' || *constr == '|') break;
      check_attr_config(constr[0] == '-' || constr[0] == '+',
                        ap, "invalid attribute constraint configuration");
      if (constr[1] == 'm') {
        /* Check for class member functions */
        if (routine->source_corresp.is_class_member) {
          if (constr[0] == '-') {
            err = ec_attr_disallows_member_function;
          }  /* if */
        } else {
          if (constr[0] == '+') {
            err = ec_attr_requires_member_function;
          }  /* if */
        }  /* if */
        constr += 2;
      } else if (constr[1] == 'v') {
        /* Check for virtual functions */
        if (routine->is_prototype_instantiation &&
            routine->source_corresp.is_class_member &&
            routine->template_arg_list == NULL &&
            base_classes_of(parent_class_of(routine)) != NULL) {
          /* A member of a class template prototype instantiation that has
             base classes: We cannot tell if it is virtual or not. */
        } else if (routine->is_virtual) {
          if (constr[0] == '-') {
            err = ec_attr_disallows_virtual_function;
          }  /* if */
        } else {
          if (constr[0] == '+') {
            err = ec_attr_requires_virtual_function;
          }  /* if */
        }  /* if */
        constr += 2;
      } else if (constr[1] == 'p') {
        /* Check for pure virtual functions */
        if (routine->pure_virtual) {
          if (constr[0] == '-') {
            err = ec_attr_disallows_pure_virtual_function;
          }  /* if */
        } else {
          if (constr[0] == '+') {
            err = ec_attr_requires_pure_virtual_function;
          }  /* if */
        }  /* if */
        constr += 2;
      } else if (constr[1] == 'x') {
        /* Check for external linkage. */
        if (routine->storage_class == (a_storage_class)sc_extern ||
            routine->storage_class == (a_storage_class)sc_unspecified) {
          if (constr[0] == '-') {
            err = ec_attr_disallows_external_linkage;
          }  /* if */
        } else {
          if (constr[0] == '+') {
            err = ec_attribute_requires_external_linkage;
          }  /* if */
        }  /* if */
        constr += 2;
      } else if (constr[1] == 'i') {
        /* Check for inline. */
        if (routine->is_inline) {
          if (constr[0] == '-') {
            err = ec_attr_disallows_inline;
          }  /* if */
        } else {
          if (constr[0] == '+') {
            err = ec_attr_requires_inline;
          }  /* if */
        }  /* if */
        constr += 2;
      } else {
        unexpected_condition_str2(
           "invalid property code for constraint configuration of attribute",
           attribute_display_name(ap));
      }  /* if */
      if (err != ec_no_error) break;
      if (*constr == '!') {
        ++constr;
      }  /* if */
    }  /* for */
    if (err != ec_no_error) {
      /* Issue the diagnostic. */
      an_error_severity  sev = *constr == '!' ? es_error : es_warning;
      pos_st_diagnostic(sev, err, &ap->position, attribute_display_name(ap));
      /* Treat the attribute as unrecognized for error recovery purposes. */
      make_attr_unrecognized(ap);
    }  /* if */
  }  /* if */
}  /* check_simple_routine_constraints */


static void check_simple_statement_constraints(
                                         a_const_char                *constr,
                                         ARG_UNUSED an_attribute_ptr ap,
                                         ARG_UNUSED a_statement_ptr  statement)
/*
constr encodes a simple target constraint for a statement.  Check that the
attribute ap applied to the given statement matches those constraints.
*/
{
  check_assertion(constr[0] == 's');
}  /* check_simple_statement_constraints */


static void check_simple_variable_constraints(a_const_char      *constr,
                                              an_attribute_ptr  ap,
                                              a_variable_ptr    variable)
/*
constr encodes a simple target constraint for a variable.  Check that the
attribute ap applied to the given variable matches those constraints.
*/
{
  check_assertion(constr[0] == 'v');
  if (constr[1] == ':') {
    /* Variable property switches follow.  E.g., "v:-a" indicates the variable
       cannot be automatic. */
    an_error_code  err = ec_no_error;
    constr += 2;
    for (;;) {
      if (*constr == '\0' || *constr == '|') break;
      check_attr_config(constr[0] == '-' || constr[0] == '+',
                        ap, "invalid attribute constraint configuration");
      if (constr[1] == 'a') {
        /* Check for automatic storage duration. */
        if (!var_has_static_or_thread_storage_duration(variable)) {
          if (constr[0] == '-') {
            err = ec_attr_disallows_automatic_storage;
          }  /* if */
        } else {
          if (constr[0] == '+') {
            err = ec_attr_requires_automatic_storage;
          }  /* if */
        }  /* if */
        constr += 2;
      } else if (constr[1] == 'h') {
        /* Check for exception handler parameter variables. */
        if (variable->is_handler_param) {
          if (constr[0] == '-') {
            err = ec_attr_disallows_handler_param;
          }  /* if */
        } else {
          if (constr[0] == '+') {
            err = ec_attr_requires_handler_param;
          }  /* if */
        }  /* if */
        constr += 2;
      } else if (constr[1] == 'l') {
        /* Check for local variables. */
        if (variable->source_corresp.is_local_to_function &&
            /* Don't consider block extern declarations local. */
            variable->storage_class != (a_storage_class)sc_extern) {
          if (constr[0] == '-') {
            err = ec_attribute_does_not_apply_to_local_variable;
          }  /* if */
        } else {
          if (constr[0] == '+') {
            err = ec_attr_requires_local_variable;
          }  /* if */
        }  /* if */
        constr += 2;
      } else if (constr[1] == 'r') {
        /* Check for register variables. */
        if (variable->storage_class == (a_storage_class)sc_register) {
          if (constr[0] == '-') {
            err = ec_attr_disallows_register_storage;
          }  /* if */
        } else {
          if (constr[0] == '+') {
            err = ec_attr_requires_register_storage;
          }  /* if */
        }  /* if */
        constr += 2;
      } else if (constr[1] == 'x') {
        /* Check for external linkage. */
        if (variable->storage_class == (a_storage_class)sc_extern ||
            variable->storage_class == (a_storage_class)sc_unspecified) {
          if (constr[0] == '-') {
            err = ec_attr_disallows_external_linkage;
          }  /* if */
        } else {
          if (constr[0] == '+') {
            err = ec_attribute_requires_external_linkage;
          }  /* if */
        }  /* if */
        constr += 2;
      } else {
        unexpected_condition_str2(
           "invalid property code for constraint configuration of attribute",
           attribute_display_name(ap));
      }  /* if */
      if (err != ec_no_error) break;
      if (*constr == '!') {
        ++constr;
      }  /* if */
    }  /* for */
    if (err != ec_no_error) {
      /* Issue the diagnostic. */
      an_error_severity  sev = *constr == '!' ? es_error : es_warning;
      pos_st_diagnostic(sev, err, &ap->position, attribute_display_name(ap));
      /* Treat the attribute as unrecognized for error recovery purposes. */
      make_attr_unrecognized(ap);
    }  /* if */
  }  /* if */
}  /* check_simple_variable_constraints */


static void check_simple_parameter_constraints(
                                           a_const_char                *constr,
                                           ARG_UNUSED an_attribute_ptr ap,
                                           ARG_UNUSED a_param_type_ptr ptp)
/*
constr encodes a simple target constraint for a parameter.  Check that the
attribute ap applied to the parameter represented by ptp matches those
constraints.
*/
{
  check_assertion(constr[0] == 'p');
}  /* check_simple_parameter_constraints */


static void check_simple_label_constraints(a_const_char                *constr,
                                           ARG_UNUSED an_attribute_ptr ap,
                                           ARG_UNUSED a_label_ptr      label)
/*
constr encodes a simple target constraint for a label.  Check that the
attribute ap applied to the given label matches those constraints.
*/
{
  check_assertion(constr[0] == 'l');
}  /* check_simple_label_constraints */


static void check_simple_asm_constraints(a_const_char                *constr,
                                         ARG_UNUSED an_attribute_ptr ap,
                                         ARG_UNUSED an_asm_entry_ptr label)
/*
constr encodes a simple target constraint for an asm declaration.  Check that
the attribute ap applied to the given entry matches those constraints.
*/
{
  check_assertion(constr[0] == 'a');
}  /* check_simple_asm_constraints */


static void check_simple_namespace_constraints(
                                           a_const_char                *constr,
                                           ARG_UNUSED an_attribute_ptr ap,
                                           ARG_UNUSED a_namespace_ptr  nsp)
/*
constr encodes a simple target constraint for a namespace.  Check that the
attribute ap applied to the given namespace matches those constraints.
*/
{
  check_assertion(constr[0] == 'n');
}  /* check_simple_namespace_constraints */


static void check_simple_using_decl_constraints(
                                           a_const_char                *constr,
                                           ARG_UNUSED an_attribute_ptr ap,
                                           ARG_UNUSED a_using_decl_ptr udp)
/*
constr encodes a simple target constraint for an entry of type a_using_decl.
Check that the attribute ap applied to the entry pointed to by udp matches
those constraints.
*/
{
  check_assertion(constr[0] == 'u');
}  /* check_simple_using_decl_constraints */


static void check_simple_constant_constraints(
                                           a_const_char                *constr,
                                           ARG_UNUSED an_attribute_ptr ap,
                                           ARG_UNUSED a_constant_ptr   cp)
/*
constr encodes a simple target constraint for a constant.  Check that the
attribute ap applied to the given constant matches those constraints.
Only enumerator constants currently have attributes attached (that check
is not enforced here because the enumeration type is not fully defined here,
so is_enum_constant fails).
*/
{
  check_assertion(constr[0] == 'E');
}  /* check_simple_constant_constraints */


static void check_simple_concept_constraints(
                                           a_const_char                *constr,
                                           ARG_UNUSED an_attribute_ptr ap,
                                           ARG_UNUSED a_template_ptr   tmpl)
/*
constr encodes a simple target constraint for a concept.  Check that the
attribute ap applied to the given template entry matches those constraints.
*/
{
  check_assertion(constr[0] == 'C');
}  /* check_simple_concept_constraints */


static a_boolean check_target_entity_match(a_const_char      *constr,
                                           an_attribute_ptr  ap,
                                           a_const_char      *entity,
                                           an_il_entry_kind  entity_kind)
/*
constr is a target constraint string as described in the definition of
an_attr_appl_descr.  ap is an attribute that is subject to that constraint.
Return FALSE and issue a diagnostic if entity/entity_kind represents an entity
not covered by constr.  Otherwise, return TRUE and check that additional
properties required by constr are met (if not, issue a diagnostic as
appropriate and set ap->kind to ak_unrecognized).
*/
{
  a_boolean  match_found = FALSE, weak_mismatch;

  if (constr[0] == '\0') {
    /* No (simple) target entity constraint. */
    match_found = TRUE;
    goto done;
  }  /* if */
  for (;;) {
    if (constr[0] == 'W') {
      /* An entity kind description prefixed by a W means that the indicated
         entity kind is not a match, but that a warning (rather than an error)
         should be issued if the attribute is specified on the indicated
         entity kind. */
      weak_mismatch = TRUE;
      ++constr;
      check_attr_config(constr[1] != ':',
                        ap, "invalid attribute constraint configuration");
    } else {
      weak_mismatch = FALSE;
    }  /* if */
    switch (constr[0]) {
      case '0':
        if (entity_kind == iek_none) {
          match_found = TRUE;
        }  /* if */
        break;
      case 'T':
      case 't':
      case 'c':
      case 'e':
        if (entity_kind == iek_type) {
          a_type_ptr  tp = (a_type_ptr)entity;
          if (constr[0] == 'c' || constr[0] == 'e') {
            /* 'c' and 'e' match only for attributes specified on the tag
               name of, respectively, a class or enum type. */
            if (!is_tag_attribute(ap) ||
                (constr[0] == 'c' && !is_immediate_class_type(tp)) ||
                (constr[0] == 'e' && !is_immediate_enum_type(tp))) {
              break;
            }  /* if */
          } else if (is_tag_attribute(ap)) {
            /* 't' and 'T' do not cover the case of attributes specified on
               tag names. */
            break;
          }  /* if */
          if (!weak_mismatch) check_simple_type_constraints(constr, ap, tp);
          match_found = TRUE;
        }  /* if */
        break;
      case 'a':
        if (entity_kind == iek_asm_entry) {
          if (!weak_mismatch) {
            check_simple_asm_constraints(constr, ap, (an_asm_entry_ptr)entity);
          }  /* if */
          match_found = TRUE;
        }  /* if */
        break;
      case 'C':
        if (entity_kind == iek_template) {
          a_template  *tmpl = (a_template*)entity;
          if (tmpl->kind == templk_concept) {
            if (!weak_mismatch) {
              check_simple_concept_constraints(constr, ap, tmpl);
            }  /* if */
            match_found = TRUE;
          }  /* if */
        }  /* if */
        break;
      case 'd':
        if (entity_kind == iek_field) {
          if (!weak_mismatch) {
            check_simple_field_constraints(constr, ap, (a_field_ptr)entity);
          }  /* if */
          match_found = TRUE;
        }  /* if */
        break;
      case 'l':
        if (entity_kind == iek_label) {
          if (!weak_mismatch) {
            check_simple_label_constraints(constr, ap, (a_label_ptr)entity);
          }  /* if */
          match_found = TRUE;
        }  /* if */
        break;
      case 'n':
        if (entity_kind == iek_namespace) {
          if (!weak_mismatch) {
            check_simple_namespace_constraints(constr, ap,
                                               (a_namespace_ptr)entity);
          }  /* if */
          match_found = TRUE;
        }  /* if */
        break;
      case 'p':
        if (entity_kind == iek_param_type) {
          if (!weak_mismatch) {
            check_simple_parameter_constraints(constr, ap,
                                               (a_param_type_ptr)entity);
          }  /* if */
          match_found = TRUE;
        }  /* if */
        break;
      case 'r':
        if (entity_kind == iek_routine) {
          if (!weak_mismatch) {
            check_simple_routine_constraints(constr, ap,
                                             (a_routine_ptr)entity);
          }  /* if */
          match_found = TRUE;
        }  /* if */
        break;
      case 's':
        if (entity_kind == iek_statement) {
          if (!weak_mismatch) {
            check_simple_statement_constraints(constr, ap,
                                               (a_statement_ptr)entity);
          }  /* if */
          match_found = TRUE;
        }  /* if */
        break;
      case 'u':
        if (entity_kind == iek_using_decl) {
          if (!weak_mismatch) {
            check_simple_using_decl_constraints(constr, ap,
                                                (a_using_decl_ptr)entity);
          }  /* if */
          match_found = TRUE;
        }  /* if */
        break;
      case 'v':
        if (entity_kind == iek_variable) {
          if (!weak_mismatch) {
            check_simple_variable_constraints(constr, ap,
                                              (a_variable_ptr)entity);
          }  /* if */
          match_found = TRUE;
        }  /* if */
        break;
      case 'E':
        if (entity_kind == iek_constant) {
          if (!weak_mismatch) {
            check_simple_constant_constraints(constr, ap,
                                              (a_constant_ptr)entity);
          }  /* if */
          match_found = TRUE;
        }  /* if */
        break;
      default:
        unexpected_condition_str2(
           "invalid entity code for constraint configuration of attribute",
           attribute_display_name(ap));
    }  /* switch */
    if (match_found) break;
    /* Skip to the next constraint (if any). */
    while (*constr != '\0' && *constr != '|') ++constr;
    if (*constr == '\0') break;
    /* Pass over the "|". */
    ++constr;
  }  /* for */
  if (!match_found || weak_mismatch) {
    /* Issue a diagnostic if no match was found. */
    an_error_severity  sev = es_error;
    if (match_found && weak_mismatch) {
      sev = es_warning;
    } else if (ap->family == af_ms_declspec &&
               ap->syntactic_location == al_tag_name) {
      /* Microsoft compilers ignore recognized attributes on tag names.
         We issue a warning. */
      sev = es_warning;
    } else if (is_gcc_attribute(ap) &&
               entity_kind == iek_type &&
               !is_type_transforming_attribute(ap)) {
      /* GCC only issues a warning on recognized attributes incorrectly
         applied to types.  (However, a hard error is still issued on a type-
         transforming attribute applied to a class or enumeration type.) */
      sev = es_warning;
    } else if (ap->family != af_alignas &&
               entity != NULL &&
               (clang_mode || gnu_version_is(>=40300))) {
      /* Clang and recent versions of GNU are more forgiving about most
         attributes not applying to the proper entity. */
      sev = es_warning;
    }  /* if */
    report_bad_attribute_target(sev, ap);
  }  /* if */
done:
  if (match_found && is_std_attribute(ap)) {
    a_decl_parse_state    *dps = (a_decl_parse_state*)ap->assoc_info;
    if (dps != NULL && (dps->dso_flags & DSO_FRIEND) != 0 &&
        !dps->is_definition &&
        !(gpp_mode && !clang_mode)) {
      /* Something like "friend class alignas(int) A;" is invalid unless
         the declaration is a definition.  GCC (but not clang) allows this. */
      report_bad_attribute_target(es_error, ap);
      match_found = FALSE;
    }  /* if */
  }  /* if */
  return match_found;
}  /* check_target_entity_match */


an_attribute_ptr* get_attribute_link(char              *entity,
                                     an_il_entry_kind  entity_kind)
/*
Return a pointer to the field of the given entity that points to the attributes
list recorded for that entity.  (For entities with a source correspondence scp,
this is &scp.attributes.)
*/
{
  an_attribute_ptr  *p_attributes = NULL;

  switch (entity_kind) {
    case iek_field:
    case iek_type:
    case iek_routine:
    case iek_variable:
    case iek_label:
    case iek_namespace:
    case iek_constant:
    case iek_asm_entry:
    case iek_template:
      p_attributes = &((a_source_correspondence*)entity)->attributes;
      break;
    case iek_param_type:
      p_attributes = &((a_param_type*)entity)->attributes;
      break;
    case iek_using_decl:
      p_attributes = &((a_using_decl*)entity)->attributes;
      break;
    case iek_statement:
      p_attributes = &((a_statement*)entity)->attributes;
      break;
    case iek_base_class:
      p_attributes = &((a_base_class*)entity)->attributes;
      break;
    case iek_module_import_decl:
      p_attributes = &((a_module_import_decl*)entity)->attributes;
      break;
    default:
      unexpected_condition();
  }  /* switch */
  return p_attributes;
}  /* get_attribute_link */

#if DEBUG

void db_attribute(an_attribute_ptr  ap)
/*
Output the given attribute to f_debug.
*/
{
  a_const_char *str = NULL;

  if (ap == NULL) {
    (void)fprintf(f_debug, "null attribute pointer\n");
    goto done;
  }  /* if */
  switch (ap->family) {
    case af_std:
      str = "[[";
      break;
    case af_gnu:
      str = "__attribute((";
      break;
    case af_ms_declspec:
      str = "__declspec(";
      break;
    case af_alignas:
      str = "";
      break;
    default:
      unexpected_condition();
  }  /* switch */
  (void)fprintf(f_debug, "%s", str);
  if (ap->kind == ak_attr_using_prefix) {
    (void)fprintf(f_debug, "\"using\" ");
  }  /* if */
  if (ap->namespace_name != NULL) {
    (void)fprintf(f_debug, "%s::", ap->namespace_name);
  }  /* if */
  if (ap->name != NULL) {
    (void)fprintf(f_debug, "%s", ap->name);
  }  /* if */
  if (ap->arguments != NULL) {
    an_attribute_arg_ptr  aap = ap->arguments;
    (void)fprintf(f_debug, "(");
    for (; aap != NULL; aap = aap->next) {
      switch (aap->kind) {
        case aak_empty:
        case aak_last:
          break;
        case aak_raw_token:
        case aak_token:
          (void)fprintf(f_debug, "%s", aap->variant.token);
          break;
        case aak_constant:
          db_constant(aap->variant.constant);
          break;
        case aak_type:
          db_abbreviated_type(aap->variant.type);
          break;
        case aak_expression:
          db_expression(expr_node_from_attribute_arg(aap));
          break;
        default_is_unexpected();
      }  /* switch */
      if (aap->next != NULL) {
        /* Another argument follows.  Separate raw tokens by whitespace, and
           other arguments by commas. */
        (void)fprintf(f_debug, "%s",
                      aap->kind == (an_attribute_arg_kind)aak_raw_token ?
                                                                   "" : ", ");
      }  /* if */
    }  /* for */
    (void)fprintf(f_debug, ")");
  }  /* if */
  switch (ap->family) {
    case af_std:
      str = "]]";
      break;
    case af_gnu:
      str = "))";
      break;
    case af_ms_declspec:
      str = ")";
      break;
    case af_alignas:
      str = "";
      break;
    default:
      unexpected_condition();
  }  /* switch */
  (void)fprintf(f_debug, "%s", str);
  (void)fprintf(f_debug, " at ");
  db_source_position(&ap->position);
done:;
}  /* db_attribute */


void db_attribute_list(an_attribute_ptr  ap)
/*
Output the given list of attributes to f_debug.
*/
{
  for (; ap != NULL; ap = ap->next) {
    db_attribute(ap);
    (void)fprintf(f_debug, "\n");
  }  /* for */
}  /* db_attribute_list */


static void db_log_attribute_action(a_const_char       *descr,
                                    an_attribute_ptr   ap,
                                    char               *entity,
                                    an_il_entry_kind   entity_kind)
/*
If the debug flag "trace_attributes" is set, produce some output on f_debug
including (a) the string descr, (b) a rendering of the given attribute, and
(c) a rendering of the given entity.
*/
{
  if (db_flag_is_set("trace_attributes")) {
    (void)fprintf(f_debug, "ATTR %s ", descr);
    db_attribute(ap);
    if (entity != NULL) {
      (void)fprintf(f_debug, "\nfor %s ",
                    il_entry_kind_names[(int)entity_kind]);
      if (entity_kind == iek_type) {
        db_abbreviated_type((a_type_ptr)entity);
      } else if (source_corresp_for_il_entry(entity, entity_kind) != NULL) {
        a_source_correspondence  *scp = (a_source_correspondence*)entity;
        (void)fprintf(f_debug, "%s", db_name_str(scp, entity_kind));
      } else {
        switch (entity_kind) {
          case iek_param_type:
            { a_const_char *name = ((a_param_type*)entity)->name;
              (void)fprintf(f_debug, "%s", name == NULL ? "(unnamed)" : name);
            }
            break;
          case iek_using_decl:
            (void)fprintf(f_debug, "at ");
            db_source_position(&((a_using_decl*)entity)->position);
            break;
          case iek_statement:
            (void)fprintf(f_debug, "at ");
            db_source_position(&((a_statement*)entity)->position);
            break;
          default:
            (void)fprintf(f_debug, "(no extra info)");
        }  /* if */
      }  /* if */
      (void)fprintf(f_debug, ".\n");
    } else {
      (void)fprintf(f_debug, "\nis stand-alone.\n");
    }  /* if */
    (void)fprintf(f_debug, "ATTR END\n");
  }  /* if */
}  /* db_log_attribute_action */

#else /* !DEBUG */

#define db_log_attribute_action(descr, ap, entity, entity_kind)  /*Nothing*/

#endif /* DEBUG */

static void make_local_expr_node_refs_for_args(an_attribute_ptr ap)
/*
All attributes are allocated in the file scope memory region, but an argument
expression can reside in a function scope memory region: That is the case for
the operand of "[[assume(x > 0)]];" appearing in the body of a function with a
parameter x, for example.  Use a local expr node reference to "point" to each
such argument expression of the given attribute, so that the expression can
still be retrieved once the function scope memory region is discarded.
*/
{
  an_attribute_arg_ptr aap;

  for (aap = ap->arguments; aap != NULL; aap = aap->next) {
    if (aap->kind == aak_expression && !aap->local_expr_ref &&
        !in_file_scope(aap->variant.expr)) {
      /* The mechanism for local-expr-nodes assumes that all "referrers" are
         IL entities whose first field is of type a_source_correspondence.
         Allocate a "dummy" a_scoped_expression IL entry that has such a field.
         No values are actually used in this IL entry (except that
         source_corresp.enclosing_routine is set and later used to find the
         original expression).  The function scope is not necessarily the one
         indicated by innermost_function_scope: For a lambda declarator, as in
         "auto a = [] [[assume(true)]] () {};", the scope of the enclosing
         function must be located in the scope stack. */
      a_scope_ptr          function_scope = get_innermost_function_scope();
      a_scoped_expression  *sexpr;

      check_assertion(function_scope != NULL);
      sexpr = alloc_scoped_expression();
      make_local_expr_node_ref(aap->variant.expr, lerk_scoped_expr,
                               (char*)sexpr, function_scope);
      aap->local_expr_ref = TRUE;
      aap->variant.sexpr = sexpr;
    }  /* if */
  }  /* for */
}  /* make_local_expr_node_refs_for_args */


static char* apply_one_attribute(an_attribute_ptr   ap,
                                 char               *entity,
                                 an_il_entry_kind   entity_kind)
/*
Attempt to apply the given attribute to the given entity.  The attribute is
not applied if it is unrecognized or if it doesn't meet its target constraints
as encoded in known_attr_appl_table.  Return the resulting entity (which is
often the same as the given entity, but not always since attributes can
potentially produce a new entity of the same kind as the original entity).
The application of the attribute may result in diagnostics and may cause
the attribute to get marked as unrecognized.  Whether the attribute is applied
or not, its argument expressions are made retrievable from the file scope
memory region in which the attribute itself resides.
*/
{
  a_const_char *constr = known_attr_appl_table[ap->kind].target_constraints;
  an_attr_application_fn
               *appl_fn = known_attr_appl_table[(int)ap->kind].appl_fn;

  if (check_target_entity_match(constr, ap, entity, entity_kind) &&
      !is_unrecognized_attr(ap)) {
    if (appl_fn != NULL) {
      entity = appl_fn(ap, entity, entity_kind);
      db_log_attribute_action("apply", ap, entity, entity_kind);
    }  /* if */
  }  /* if */
  /* An attribute that was not applied -- e.g., because it doesn't appertain to
     this kind of entity -- remains recorded in the IL, so its arguments have
     to be made retrievable in that case, too. */
  make_local_expr_node_refs_for_args(ap);
  return entity;
}  /* apply_one_attribute */


static bool is_non_tag_type_transforming_attribute(an_attribute_ptr  ap)
/*
This function determines if the given attribute both is a type-transforming
attribute and does not apply to tags.  This is used to determine which
attributes are applied by attach_attributes and which attributes are extracted
via extract_type_transforming_attributes (these are then generally applied with
attach_type_attributes).
*/
{
  return is_type_transforming_attribute(ap) && !is_tag_attribute(ap);
}  /* is_non_tag_type_transforming_attribute */


void attach_attributes(an_attribute_ptr  attributes,
                       char              *entity,
                       an_il_entry_kind  entity_kind)
/*
Attach and apply the given list of attributes to the given IL entry, except
that type-transforming attributes applied in a location other than al_tag_name
are not applied (but still attached).  Perform any required checking, and
update the IL entry's fields if applicable.  (To apply type-transforming
attributes, call attach_type_attributes.  If not already separated,
extract_type_transforming_attributes should be used to form a separate list of
attributes for attach_type_attributes.)
*/
{
  char              *new_entity = entity;
  an_attribute_ptr  ap, next_ap;

  if (entity != NULL) {
    an_attribute_ptr  *p_list = get_attribute_link(entity, entity_kind);
    *last_attribute_link(p_list) = attributes;
  }  /* if */
  for (ap = attributes; ap != NULL; ap = next_ap) {
    db_log_attribute_action("attach", ap, entity, entity_kind);
    /* Save the "next" pointer because attribute application might move the
       attribute. */
    next_ap = ap->next;
    if (ap->applied_to_declared_type) {
      /* The attribute was already applied to the type declared by the
         declaration in which it appeared (see
         apply_calling_convention_attributes).  Applying it a second time
         would repeat any diagnostics it produces. */
      ap->applied_to_declared_type = FALSE;
    } else if (!is_non_tag_type_transforming_attribute(ap)) {
      new_entity = apply_one_attribute(ap, new_entity, entity_kind);
    }  /* if */
  }  /* for */
  check_assertion(new_entity == entity);
}  /* attach_attributes */


void transform_type_with_gnu_attributes(a_type_ptr        *p_type,
                                        an_attribute_ptr  attributes,
                                        void              *assoc_info)
/*
Apply any type-transforming GNU attributes in the given list of attributes to
*p_type and set *p_type to the resulting type.  The attribute list is not
attached to any IL entry.  assoc_info is the value that should be recorded in
the assoc_info field of the attribute while it is applied to the type:
Normally, it is a pointer to the a_decl_parse_state associated with the
construct produces *p_type.
(This routine is used to handle GNU type-transforming attributes on typedefs.)
*/
{
  an_attribute_ptr  ap, next_ap;

  for (ap = attributes; ap != NULL; ap = next_ap) {
    /* Save the "next" pointer because attribute application might move the
       attribute. */
    next_ap = ap->next;
    if (is_gcc_attribute(ap) &&
        is_type_transforming_attribute(ap)) {
      ap->assoc_info = assoc_info;
      *p_type = (a_type_ptr)apply_one_attribute(ap, (char*)*p_type, iek_type);
      ap->assoc_info = NULL;
    }  /* if */
  }  /* for */
}  /* transform_type_with_gnu_attributes */


#if GNU_X86_ATTRIBUTES_ALLOWED

static a_boolean is_calling_convention_attribute(an_attribute_ptr  ap)
/*
Return TRUE if the given attribute is one of the GNU calling convention
attributes ("cdecl", "fastcall", and "stdcall").
*/
{
  return is_gcc_attribute(ap) &&
         (ap->kind == ak_cdecl || ap->kind == ak_fastcall ||
          ap->kind == ak_stdcall);
}  /* is_calling_convention_attribute */


void apply_calling_convention_attributes(a_type_ptr        *p_type,
                                         an_attribute_ptr  attributes,
                                         void              *assoc_info)
/*
Apply any GNU calling convention attributes in the given list of attributes to
the function type underlying *p_type and set *p_type to the resulting type.
Each attribute that is applied is marked as having been applied to the
declared type, so that attach_attributes does not apply it a second time (which
would repeat any diagnostics it produces) when the list is later attached to
the entity being declared.  The attribute list is not attached to any IL entry.
assoc_info is the value that should be recorded in the assoc_info field of an
attribute while it is applied to the type: Normally, it is a pointer to the
a_decl_parse_state associated with the construct that produces *p_type.
(This routine is used to handle GNU calling convention attributes on typedefs,
whose type must record its calling convention before it can be compared with
the type of an earlier declaration of the same name.)
*/
{
  an_attribute_ptr  ap, next_ap;

  for (ap = attributes; ap != NULL; ap = next_ap) {
    /* Save the "next" pointer because attribute application might move the
       attribute. */
    next_ap = ap->next;
    if (is_calling_convention_attribute(ap)) {
      ap->assoc_info = assoc_info;
      *p_type = (a_type_ptr)apply_one_attribute(ap, (char*)*p_type, iek_type);
      ap->assoc_info = NULL;
      ap->applied_to_declared_type = TRUE;
    }  /* if */
  }  /* for */
}  /* apply_calling_convention_attributes */

#endif /* GNU_X86_ATTRIBUTES_ALLOWED */


a_type_ptr make_typeref_with_attributes(a_type_ptr        tp,
                                        an_attribute_ptr  attributes)
/*
Return a new tk_typeref entry with the given attributes and whose underlying
type is tp, except if tp is already such a typeref (in that case, just append
the given attributes to it).
*/
{
  a_type_ptr  result;

  if (tp->kind == (a_type_kind)tk_typeref &&
      is_typeref_kind(tp, trk_for_type_attributes)) {
    /* Reuse the existing "for attributes" typeref entry. */
    result = tp;
    *last_attribute_link(&tp->source_corresp.attributes) = attributes;
  } else {
    /* Create a new "for attributes" typeref entry. */
    result = alloc_type((a_type_kind)tk_typeref);
    result->variant.typeref.type = tp;
    result->variant.typeref.kind = trk_for_type_attributes;
    result->source_corresp.attributes = attributes;
  }  /* if */
  return result;
}  /* make_typeref_with_attributes */


void attach_type_attributes(a_type_ptr        *p_type,
                            an_attribute_ptr  attributes,
                            void              *assoc_info)
/*
Apply the given attributes to *p_type, which results in a type T.  Attach the
attributes to the type entry for T directly if T is a routine type, and via a
typeref pointing to the attributes on top of T otherwise.  Return the type
entry to which the attributes are attached through *p_type.  If attributes is
NULL, do nothing.  assoc_info is the value that should be recorded in the
assoc_info field of the attribute while it is applied to the type: Normally,
it is a pointer to the a_decl_parse_state associated with the construct that
produces *p_type.
*/
{
  if (attributes != NULL) {
    an_attribute_ptr  ap, next_ap;
    a_type_ptr        new_type = *p_type;
    for (ap = attributes; ap != NULL; ap = next_ap) {
      /* Save the "next" pointer because attribute application might move the
         attribute. */
      next_ap = ap->next;
      ap->assoc_info = assoc_info;
      new_type = (a_type_ptr)
                           apply_one_attribute(ap, (char*)new_type, iek_type);
      ap->assoc_info = NULL;
    }  /* for */
    if (new_type->kind != (a_type_kind)tk_routine &&
        !(new_type->kind == (a_type_kind)tk_typeref &&
          is_typeref_kind(new_type, trk_for_type_attributes))) {
      /* Attributes should not be recorded directly in type entries that might
         be shared.  Use a typeref to carry the attributes instead. */
      *p_type =  make_typeref_with_attributes(new_type, attributes);
    } else {
      /* A function type not under a typedef will not be reused without its
         attributes applied.  Attach the attributes directly (besides saving
         a tk_typeref entry, it also avoid surprises with existing code that
         has been assuming that tk_typerefs on top of routine types must be
         typedef/decltype/typeof entries).  Similarly, if new_type is a
         tk_typeref entry with kind trk_for_type_attributes, attributes can be
         attached to it directly. */
      *last_attribute_link(&new_type->source_corresp.attributes) = attributes;
      *p_type = new_type;
    }  /* if */
#if DEBUG
    if (db_flag_is_set("trace_attributes")) {
      for (ap = attributes; ap != NULL; ap = ap->next) {
        db_log_attribute_action("attach", ap, (char*)new_type, iek_type);
      }  /* for */
    }  /* if */
#endif /* DEBUG */
  }  /* if */
}  /* attach_type_attributes */


void extract_type_transforming_attributes(an_attribute_ptr *p_attributes,
                                          an_attribute_ptr *p_extracted)
/*
Update the list of attributes pointed to by p_attributes removing attributes
that are both type-transforming and do not apply to tags.  The removed
attributes are placed into a new attribute list pointed to by p_extracted.
This is generally used to extract attributes so that p_attributes can be passed
to attach_attributes, while p_extracted can be passed to
attach_type_attributes.
*/
{
  /* Create a copy of the pointer to the currently-examined attribute. */
  an_attribute_ptr  curr_ap = *p_attributes;

  /* Process the attributes from p_attributes, assigning them to either an
     updated p_attributes or p_extracted. */
  while (curr_ap != NULL) {
    if (is_non_tag_type_transforming_attribute(curr_ap)) {
      /* The attribute is both type-transforming and does not apply to tags, it
         belongs in the extracted list.  Update the extracted list head to
         point to this attribute, then update the head position. */
      *p_extracted = curr_ap;
      p_extracted = &curr_ap->next;
    } else {
      /* The attribute was not type-transforming or applies to tags, it belongs
         in the attributes list.  Update the attributes list head to point to
         this attribute, then update the head position. */
      *p_attributes = curr_ap;
      p_attributes = &curr_ap->next;
    }  /* if */
    {
      /* Update the curr_ap to point to the next attribute to examine. */
      an_attribute_ptr  tmp_ap = curr_ap;
      curr_ap = tmp_ap->next;
      /* Break the chain on the attribute we just finished working with.  The
         head of this chain will be set by the next loop or otherwise will
         remain null designating the end of the list. */
      tmp_ap->next = NULL;
    }
  }  /* while */
}  /* extract_type_transforming_attributes */


an_attribute_ptr copy_of_attributes_list(an_attribute_ptr  attributes)
/*
Return a copy of the given list of attributes (which may be NULL).  Any
attribute arguments will be shared between the original attribute and the
copy that is returned.
*/
{
  an_attribute_ptr  result = NULL, *p_attr = &result, ap;

  for (ap = attributes; ap != NULL; ap = ap->next) {
    copy_attribute(ap, *p_attr);
    p_attr = &(*p_attr)->next;
  }  /* for */
  return result;
}  /* copy_of_attributes_list */


static void substitute_attribute_arg_constant(
                                           an_attribute_arg_ptr  aap,
                                           a_template_param_ptr  t_params,
                                           a_template_arg_ptr    t_args,
                                           a_type_ptr            parent_class,
                                           a_ctws_state          *ctws_state,
                                           a_boolean             *p_error)
/*
aap is an attribute argument encapsulating a ck_template_param constant that
will be applied to a nondependent entity E.  Perform template argument
substitutions on this constant to obtain an attribute argument that can be
applied to E.  t_args is NULL if E is a non-template member of a class template
instance; otherwise, it corresponds to the template arguments for E, and
t_params are the corresponding template parameters.  If E is a class member,
parent_class points to the entry for its parent class (which may imply
additional substitutions); otherwise, parent_class is NULL.  ctws_state points
to state information for the substitution (primarily used for the case when the
type argument is a parameter pack).  *p_error is set to TRUE if a substitution
error occurs.
*/
{
  check_assertion(constant_is(aap->variant.constant, ck_template_param));
  substitute_constant(&aap->variant.constant, parent_class, t_params, t_args,
		      CTWS_NO_OPTIONS, ctws_state, &aap->position, p_error);
}  /* substitute_attribute_arg_constant */


static void substitute_attribute_arg_type(an_attribute_arg_ptr  aap,
                                          a_template_param_ptr  t_params,
                                          a_template_arg_ptr    t_args,
                                          a_type_ptr            parent_class,
                                          a_ctws_state          *ctws_state,
                                          a_boolean             *p_error)
/*
aap is an attribute type argument that will be applied to a nondependent
entity E, but whose type entry still may depend on template parameters.
Perform template argument substitutions on this type to obtain an attribute
type argument that can be applied to E.  t_args is NULL if E is a non-template
member of a class template instance; otherwise, it corresponds to the template
arguments for E, and t_params are the corresponding template parameters.  If E
is a class member, parent_class points to the entry for its parent class
(which may imply additional substitutions); otherwise, parent_class is NULL.
ctws_state points to state information for the substitution (primarily used for
the case when the type argument is a parameter pack).  *p_error is set to TRUE
if a substitution error occurs.
*/
{
  if (parent_class != NULL &&
      parent_class->variant.class_struct_union.is_template_class &&
      !parent_class->variant.class_struct_union.is_specialized) {
    /* If the parent class is itself a template instance (but not an explicit
       specialization), first recursively substitute any parameters that it is
       associated with. */
    a_template_arg_ptr    parent_t_args;
    a_template_param_ptr  parent_t_params;
    get_substitution_pairs_for_template_class(parent_class, &parent_t_params,
                                              &parent_t_args);
    substitute_attribute_arg_type(aap, parent_t_params, parent_t_args,
                                  parent_class_or_null(parent_class),
                                  ctws_state, p_error);
  }  /* if */
  if (!*p_error && t_args != NULL) {
    aap->variant.type = copy_type_with_substitution(
                                    aap->variant.type, t_args, t_params,
                                    &aap->position, CTWS_NO_OPTIONS, p_error,
                                    ctws_state);
  }  /* if */
}  /* substitute_attribute_arg_type */


static a_boolean attribute_applies_to_partial_instantiation(
                                             ARG_UNUSED an_attribute_kind kind)
/*
Returns TRUE if the specified attribute kind applies to a partial
instantiation.
*/
{
  a_boolean result = FALSE;

#if GNU_EXTENSIONS_ALLOWED
  if (kind == (an_attribute_kind)ak_abi_tag) {
    result = gnu_version >= 40900;
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  return result;
}  /* attribute_applies_to_partial_instantiation */


static an_expr_node_ptr substitute_attribute_expr(
                                              an_expr_node_ptr     expr,
                                              a_template_arg_ptr   t_args,
                                              a_template_param_ptr t_params,
                                              a_source_position    *position,
                                              a_boolean            *p_error,
                                              a_ctws_state         *ctws_state)
/*
Do template argument substitution for the given expression (which appears as
an argument in an attribute) and return the substituted expression.  t_args
represents the template arguments for that specialization and t_params the
associated template parameters.  *position is the position of the expression.
If p_error is non-NULL, *p_error is set to TRUE if the substitution results in
an invalid entity.  If p_error is NULL, a substitution error is diagnosed as
an error.  ctws_state is the state information for the substitution.
*/
{
  a_constant_ptr constant = local_constant();
  a_constant_ptr alloc_con;

  expr = copy_template_param_expr(expr, t_args, t_params, (a_type_ptr)NULL,
                                  position, CTWS_NON_CONSTANT_EXPR, p_error,
                                  ctws_state, constant, &alloc_con);
  if (expr == NULL) {
    if (alloc_con != NULL) {
      expr = alloc_node_for_allocated_constant(alloc_con);
    } else {
      expr = alloc_node_for_constant(constant);
    }  /* if */
  }  /* if */
  release_local_constant(&constant);
  return expr;
}  /* substitute_attribute_expr */


an_attribute_ptr copy_of_attributes_with_substitution(
                                an_attribute_ptr      attributes,
                                a_boolean             primary_only,
                                a_symbol_ptr          template_sym,
                                a_template_param_ptr  t_params,
                                a_template_arg_ptr    t_args,
                                a_type_ptr            parent_class,
                                a_boolean             is_partial_instantiation,
                                a_boolean             *p_error)
/*
Return a copy of the given list of attributes (which may be NULL) after
substituting template parameters (if any).  If primary_only is TRUE, only the
attributes whose on_primary_declaration flag is set are copied.  If the entity
to which the attributes are to be applied is a template specialization,
template_sym is the template associated with the specialization, t_args
represents the template arguments for that specialization and t_params the
associated template parameters; otherwise, template_sym, t_args and t_params
are NULL.  If the entity to which the attributes are to be applied is a class
member, parent_class is the enclosing class: That parent class may itself be a
specialization, and its parameter substitutions are also applied to the
attributes.  E.g.:
  template<typename T> struct S { struct N; };
  template<typename T> struct [[align(T)]] S<T>::N {};
  S<double>::N sn;
The [[align(T)]] attribute is instantiated for S<double>::N with t_params and
t_args both set to NULL (since S<T>::N is not itself a template), and
parent_class pointing to the entry for S<double> (which implies the T->double
substitution).  When is_partial_instantiation is TRUE, only attributes that
apply to a partial instantiation are copied; when is_partial_instantiation
is FALSE, only attributes that apply to a full instantiation are copied
(does not apply to function templates).
If p_error is non-NULL, *p_error is set to TRUE if the substitution results in
an invalid entity.  If p_error is NULL, a substitution error is diagnosed as
an error.
*/
{
  an_attribute_ptr  result = NULL, *p_attr = &result, ap;
  a_boolean         err = FALSE, substitution_error_reported = FALSE;
  a_boolean         rescan_pushed = FALSE;

  /* There are two doubly-nested loops here (i.e., a total of four nested loop
     constructs).  The outer loop copies each attribute with an associated
     immediately-nested loop to handle pack-expansion of attributes.  For
     example:
       template<int ... Ns> [[gnu::aligned(Ns)...]] int f();
       int r = f<8, 16, 32>();
     The innermost loops deal with copying the arguments for the outer loops
     (one loop over the arguments of the original attributes, and another
     immediately-nested one for any pack expansion within those). */
  for (ap = attributes; ap != NULL; ap = ap->next) {
    a_ctws_state                      actws_state;
    a_pack_expansion_descr_ptr        apedep;
    a_pack_expansion_stack_entry_ptr  apesep;
    a_boolean                         more_attributes;
    if (primary_only && !ap->on_primary_declaration) continue;
    if (symbol_is(template_sym, sk_class_template) &&
        is_partial_instantiation !=
                       attribute_applies_to_partial_instantiation(ap->kind)) {
      /* For instantiations of class templates, apply the attribute at either
         partial instantiation time or full instantiation time (but not both),
         as determined by the attribute kind. */
      continue;
    }  /* if */
    if (!rescan_pushed) {
      /* If we will be substituting template arguments below, push a rescan
         context if one has not already been pushed. */
      push_instantiation_scope_for_rescan(template_sym);
      rescan_pushed = TRUE;
    }  /* if */
    init_ctws_state(&actws_state);
    apedep = ap->pack_expansion_descr;
    more_attributes = begin_rescan_pack_expansion_context(
                                                     apedep, t_params, t_args,
                                                     &apesep, CTWS_NO_OPTIONS,
                                                     &actws_state, &err);
    while (more_attributes) {
      copy_attribute(ap, *p_attr);
      /* If the attribute was a parameter pack expansion, it's being expanded
         now so reset the flag. */
      (*p_attr)->is_pack_expansion = FALSE;
      if ((*p_attr)->arguments != NULL) {
        a_ctws_state		       ctws_state;
        a_pack_expansion_descr_ptr       pedep;
        a_pack_expansion_stack_entry_ptr pesep;
        a_boolean                        any_more;
        an_attribute_arg_ptr             *p_aap = &(*p_attr)->arguments;
        an_attribute_arg_ptr             aap = *p_aap;
        init_ctws_state(&ctws_state);
        do {
          *p_aap = alloc_attribute_arg();
          **p_aap = *aap;
          /* Boilerplate code for handling a pack expansion in an attribute
             argument (alignas is currently the only case). */
          pedep = aap->pack_expansion_descr;
          any_more = begin_rescan_pack_expansion_context(
                                          pedep, t_params, t_args, &pesep,
                                          CTWS_NO_OPTIONS, &ctws_state, &err);
          if (!any_more && aap->is_pack_expansion) {
            /* Indicate an empty pack expansion. */
            (*p_aap)->kind = (an_attribute_arg_kind)aak_empty;
          }  /* if */
          while (any_more) {
            /* Substitute template parameters in the attribute arguments. */
            switch (aap->kind) {
              case aak_empty:
              case aak_raw_token:
              case aak_token:
              case aak_last:
                /* Nothing to do. */
                break;
              case aak_constant:
                if (constant_is(aap->variant.constant, ck_template_param)) {
                  (*p_aap)->variant.constant = aap->variant.constant;
                  substitute_attribute_arg_constant(*p_aap, t_params, t_args,
                                                    parent_class, &ctws_state,
                                                    &err);

                } else {
                  an_expr_node_ptr  saved_expr = aap->variant.constant->expr;
                  a_memory_region_number
                                    region_to_switch_back_to;
                  /* Do not copy the backing expression since it may have a
                     dependent component (which we cannot easily
                     substitute). */
                  aap->variant.constant->expr = NULL;
                  switch_to_file_scope_region(&region_to_switch_back_to);
                  (*p_aap)->variant.constant =
                               alloc_unshared_constant(aap->variant.constant);
                  switch_back_to_original_region(region_to_switch_back_to);
                  if (saved_expr != NULL && in_file_scope(saved_expr)) {
                    aap->variant.constant->expr = saved_expr;
                  }  /* if */
                }  /* if */
                break;
              case aak_type:
                (*p_aap)->variant.type = aap->variant.type;
                substitute_attribute_arg_type(*p_aap, t_params, t_args,
                                              parent_class, &ctws_state, &err);
                break;
              case aak_expression:
                { an_expr_node_ptr  expr = expr_node_from_attribute_arg(aap);
                  /* The copy stores the substituted expression directly and
                     therefore does not reach it through the local expr node
                     reference that the original argument may have. */
                  (*p_aap)->local_expr_ref = FALSE;
                  (*p_aap)->variant.expr = substitute_attribute_expr(
                                            expr, t_args, t_params,
                                            &((*p_aap)->position),
                                            &err, &ctws_state);
                }
                break;
              default_is_unexpected();
            }  /* switch */
            if (err) {
              /* A substitution error.  Issue a diagnostic for the first error
                 if p_error is NULL (i.e., the caller cannot be notified
                 directly of the error). */
              if (p_error == NULL && !substitution_error_reported) {
                pos_error(ec_bad_attribute_template_substitution,
                          &aap->position);
                substitution_error_reported = TRUE;
              }  /* if */
              make_attr_unrecognized(*p_attr);
            }  /* if */
            /* If the argument was a parameter pack expansion, it's been
               expanded now so reset the flag. */
            (*p_aap)->is_pack_expansion = FALSE;
            (void)end_potential_pack_expansion_context(
                                              pesep, /*is_declarator=*/FALSE);
            any_more = advance_to_next_pack_element(pesep);
            if (any_more) {
              /* Allocate another attribute argument for the next element of
                 the pack expansion. */
              p_aap = &(*p_aap)->next;
              *p_aap = alloc_attribute_arg();
              **p_aap = *aap;
            }  /* if */
          }  /* while */
          p_aap = &(*p_aap)->next;
          aap = aap->next;
        } while (aap != NULL);
      }  /* if */
      p_attr = &(*p_attr)->next;
      (void)end_potential_pack_expansion_context(apesep,
                                                 /*is_declarator=*/FALSE);
      more_attributes = advance_to_next_pack_element(apesep);
    }  /* while */
  }  /* for */
  if (rescan_pushed) {
    /* If a rescan context was pushed above, pop it now. */
    pop_instantiation_scope_for_rescan();
  }  /* if */
  if (err && p_error != NULL) *p_error = TRUE;
  return result;
}  /* copy_of_attributes_with_substitution */


void mark_primary_decl_attributes(an_attribute_ptr  attributes)
/*
Set the on_primary_declaration flag to TRUE in each of the attribute entries
in the given list.
*/
{
  an_attribute_ptr  ap;
  for (ap = attributes; ap != NULL; ap = ap->next) {
    ap->on_primary_declaration = TRUE;
  }  /* for */
}  /* mark_primary_decl_attributes */


an_attribute_ptr composite_attributes(an_attribute_ptr  ap1,
                                      an_attribute_ptr  ap2)
/*
Return the "composite attributes list" of the two given list of attributes.
Currently, this is just the concatenation of copies of those lists.
*/
{
  an_attribute_ptr  result = NULL;

  if (ap1 == NULL) {
    result = copy_of_attributes_list(ap2);
  } else {
    result = copy_of_attributes_list(ap1);
    if (ap2 != NULL) {
      *f_last_attribute_link(&result) = copy_of_attributes_list(ap2);
    }  /* if */
  }  /* if */
  return result;
}  /* composite_attributes */


an_attribute_ptr get_param_variable_attr_copies(a_param_type_ptr  ptp)
/*
Return a list containing copies of the attributes from ptp->attributes that
really apply to the associated parameter variable.  The copied attributes have
their syntactic location recorded as al_implicit.
*/
{
  an_attribute_ptr  result = NULL, *p_attr = &result, ap;

  for (ap = ptp->attributes; ap != NULL; ap = ap->next) {
    a_boolean  do_copy;
    switch (ap->kind) {
#if GNU_EXTENSIONS_ALLOWED
      case ak_unused:
        do_copy = TRUE;
        break;
#endif /* GNU_EXTENSIONS_ALLOWED */
      case ak_deprecated:
      case ak_unavailable:
      case ak_maybe_unused:
        do_copy = TRUE;
        break;
      default:
        do_copy = FALSE;
    }  /* switch */ /*lint !e764*/  /* Lint complains about there not being
                                       an actual switch case. */
    if (do_copy) {
      copy_attribute(ap, *p_attr);
      (*p_attr)->next = NULL;
      (*p_attr)->assoc_info = NULL;
      (*p_attr)->syntactic_location = al_implicit;
      p_attr = &(*p_attr)->next;
    }  /* if */
  }  /* for */
  return result;
}  /* get_param_variable_attr_copies */


static a_boolean get_attr_arg_integer(an_attribute_arg_ptr  aap,
                                      an_attribute_ptr      ap,
                                      a_host_large_integer  min_val,
                                      a_host_large_integer  max_val,
                                      a_host_large_integer  *val)
/*
aap is an aak_constant argument for the given attribute.  Return TRUE if the
associated constant is a ck_integer whose value is in the range determined
by min_val and max_val (inclusive).  If the constant is a ck_integer, set *val
to the associated value (as produced by value_of_integer_constant).  Issue an
error on out-of-range cases.  (The attribute argument can also be a ck_error
or ck_template_param constant.  In such cases, this function has no effect
other than returning FALSE.)
*/
{
  a_boolean       known_good_value = FALSE;
  a_constant_ptr  con = aap->variant.constant;

  if (con->kind != (a_constant_repr_kind)ck_template_param &&
      con->kind != (a_constant_repr_kind)ck_error) {
    a_boolean  ovflo = FALSE;
    if (con->kind == (a_constant_repr_kind)ck_integer &&
        is_integral_or_enum_type(con->type)) {
      *val = value_of_integer_constant(con, &ovflo);
      if (ovflo || *val < min_val || *val > max_val) {
        report_bad_attribute_arg(aap, ap);
      } else {
        known_good_value = TRUE;
      }  /* if */
    } else {
      pos_error(ec_exp_int_constant, &aap->position);
      make_attr_unrecognized(ap);
    }  /* if */
  }  /* if */
  return known_good_value;
}  /* get_attr_arg_integer */


static a_type_ptr get_func_type_for_attr(an_attribute_ptr  ap,
                                         char              **entity,
                                         an_il_entry_kind  entity_kind)
/*
The given attribute is being applied to *entity (of the given kind).
Several GNU attributes (like "const" or "format") apply to a function type, a
pointer-to-function type or to a routine, variable or field with such a type.
(However, GCC does not currently accept such attributes for pointer-to-member
types.)  In all such cases, the underlying routine type must be modified.  If
applicable, return that routine type for *entity after ensuring that it is
modifiable (which in the case of a type may mean that *entity is modified).
Otherwise, return NULL and issue a diagnostic if appropriate.
*/
{
  a_type_ptr  *p_type = NULL, type, func_type;

  switch (entity_kind) {
    case iek_routine:      p_type = &((a_routine*)*entity)->type;    break;
    case iek_variable:     p_type = &((a_variable*)*entity)->type;   break;
    case iek_field:        p_type = &((a_field*)*entity)->type;      break;
    case iek_param_type:   p_type = &((a_param_type*)*entity)->type; break;
    case iek_type:
      if (!is_type_transforming_attribute(ap) &&
          type_is_typedef(*(a_type_ptr*)entity)) {
        /* All function-type attributes applied to typedef declarations apply
           to the underlying type.  Make p_type point to the pointer to that
           underlying type since ensure_underlying_function_type_is_modifiable
           will otherwise drop the typedef type. */
        p_type = &(*(a_type_ptr*)entity)->variant.typeref.type;
      } else {
        p_type = (a_type_ptr*)entity;
      }  /* if */
      break;
    default:
      unexpected_condition();
  }  /* switch */
  type = *p_type;
  /* There are currently no attributes that apply to handles or tracked
     references to functions. */
  assert_not_handle_or_tracking_reference(type);
  if (is_function_type(type) || is_pointer_to_function_type(type)) {
    /* The normal case. */
    ensure_underlying_function_type_is_modifiable(p_type, &func_type);
  } else {
    /* Either an invalid type, or a template case. */
    if (!is_template_dependent_type(type)) {
      if (type_specifier_of_type(type) == NULL) {
        /* In some cases, the type may still be under construction: It should
           therefore not be used in the diagnostic. */
        pos_st_warning(ec_attr_not_applied_to_function_type, &ap->position,
                       attribute_display_name(ap));
      } else {
        pos_stty_warning(ec_attr_requires_func_type, &ap->position,
                         attribute_display_name(ap),
                         type);
      }  /* if */
      make_attr_unrecognized(ap);
    }  /* if */
    func_type = NULL;
  }  /* if */
  return func_type;
}  /* get_func_type_for_attr */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void exclude_prior_attribute_kind(an_attribute_kind  kind,
                                         an_attribute_ptr   new_attr,
                                         char               *entity,
                                         an_il_entry_kind   entity_kind)
/*
If the list of attributes associated with the given entity contains an
attribute of the given kind preceding the attribute new_attr, issue an error
and make new_attr unrecognized.
*/
{
  an_attribute_ptr  ap = *get_attribute_link(entity, entity_kind);

  for (; ap != NULL && ap != new_attr; ap = ap->next) {
    if (ap->kind == kind) {
      pos_st2_error(ec_attribute_conflict, &new_attr->position,
                    attribute_display_name(ap),
                    new_attr->name);
      make_attr_unrecognized(new_attr);
      break;
    }  /* if */
  }  /* for */
}  /* exclude_prior_attribute_kind */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static char* apply_align_attr(an_attribute_ptr  ap,
                              char              *entity,
                              an_il_entry_kind  entity_kind)
/*
Apply the given "align" (or "aligned") attribute to the given entity and
return that entity.  This is also the function called for the C++11 "alignas"
and C11 _Alignas specifiers.
*/
{
  a_const_char *constr = NULL;
  a_boolean    std_specifier = is_std_attribute(ap) &&
                               !ap->is_std_gcc_attribute;
  /* g++ appears to use the last attribute in the declaration in earlier
     versions, and even in later versions for types. */
  a_boolean    use_last_attribute = !C_mode() && gnu_mode && !clang_mode &&
                                    (gnu_version < 40800 ||
                                     entity_kind == iek_type);
  a_boolean    attribute_discarded = FALSE;
  a_decl_parse_state
               *dps = (a_decl_parse_state*)ap->assoc_info;

  if (is_gcc_attribute(ap)) {
    /* GCC allows types, parameters, and bit fields to have a user-specified
       alignment. */
    if (gnu_version >= 40300) {
      /* Newer versions of GCC also allow the alignment of functions. */
      constr = "c|e|t|v:-r!|d|p|r";
    } else {
      constr = "c|e|t|v:-r!|d|p";
    }  /* if */
  } else if (std_specifier) {
    if (gpp_mode && !clang_mode) {
      /* g++ allows alignas on a typedef. */
      constr = "c|e|t|v:-r!-h!|d:-b!";
    } else if (dps != NULL && dps->deferred_alignas_attributes) {
      /* DR444 says that an _Alignas attribute is allowed in a compound
         literal, e.g., "(_Alignas(32) int){1}".  In such cases, there is no IL
         entity to attach the attribute to, so this is treated as a
         type-transforming attribute (which will have a typeref with kind
         trk_for_type_attributes). */
      if (c11_mode) {
        constr = "T";
      } else {
        /* This attribute should not appear here. */
        pos_error(ec_attribute_not_allowed, &ap->position);
        make_attr_unrecognized(ap);
      }  /* if */
    } else {
      constr = "c|e|v:-r!-h!|d:-b!";
    }  /* if */
    if (c11_mode && ap->family == af_alignas) {
      /* C11 allows _Alignas in syntactic locations different from C++11's
         alignas. */
      if (ap->syntactic_location != al_prefix &&
          ap->syntactic_location != al_specifier) {
        pos_diagnostic(es_discretionary_error, ec_attribute_not_allowed,
                       &ap->position);
      }  /* if */
    }  /* if */
  } else {
    check_assertion(ap->family == af_ms_declspec);
    constr = "c|e|t|v|d|r";
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cli_or_cx_enabled) {
    /* The alignment attribute cannot be combined with certain other
       attributes. */
    exclude_prior_attribute_kind(ak_appdomain, ap, entity, entity_kind);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (constr != NULL &&
      check_target_entity_match(constr, ap, entity, entity_kind) &&
      !is_unrecognized_attr(ap)) {
    an_attribute_arg_ptr  aap = ap->arguments;
    do {
      a_targ_alignment  alignment = 0;
      a_boolean         apply_value = TRUE;
      if (ap->arguments == NULL) {
        /* If there is no argument to the GNU "aligned" attribute, then the
           maximum alignment useful on the target is implied. */
        check_assertion(is_gcc_attribute(ap));
        alignment = targ_maximum_intrinsic_alignment;
      } else if (aap->kind == (an_attribute_arg_kind)aak_empty) {
        /* alignas accepts pack expansions.  We may get here with an empty
           expansions: The attribute has no effect in that case. */
        check_assertion(ap->family == af_alignas);
        break;
      } else if (aap->kind == (an_attribute_arg_kind)aak_type) {
        a_type_ptr  tp = aap->variant.type;
        check_assertion(std_specifier);
        /* For references and/or arrays, use the underlying type. */
        if (is_any_reference_type(tp)) tp = type_pointed_to(tp);
        if (is_array_type(tp)) tp = underlying_array_element_type(tp);
        if (is_function_type(tp)) {
          pos_error(ec_function_type_not_allowed, &aap->position);
          apply_value = FALSE;
          make_attr_unrecognized(ap);
        } else if (is_template_dependent_type(tp)) {
          /* Something like "[[align(T)]]" with T a template parameter: The
             alignment value isn't generally known and should therefore not be
             recorded. */
          apply_value = FALSE;
        } else {
          complete_type_is_needed(tp);
          if (is_incomplete_type(tp)) {
            issue_incomplete_type_diag(&aap->position, tp);
            apply_value = FALSE;
            make_attr_unrecognized(ap);
          } else if (is_sizeless_type(tp)) {
            pos_ty_error(ec_sizeless_type_not_allowed, &aap->position, tp);
            apply_value = FALSE;
            make_attr_unrecognized(ap);
          } else {
            alignment = alignment_of_type(aap->variant.type);
          }  /* if */
        }  /* if */
      } else if (aap->kind == (an_attribute_arg_kind)aak_constant) {
        a_host_large_integer  value = 0;
        if (get_attr_arg_integer(aap, ap, (a_host_large_integer)0,
                                 MAX_HOST_LARGE_INTEGER, &value)) {
          if (value == 0) {
            /* The standard attribute [[align(0)]] is simply ignored. */
            apply_value = FALSE;
          } else if (!check_pack_alignment_value(value, &alignment)) {
            pos_error(ec_bad_attribute_alignment, &aap->position);
            apply_value = FALSE;
            make_attr_unrecognized(ap);
          }  /* if */
        } else {
          apply_value = FALSE;
        }  /* if */
      }  /* if */
      if (gpp_mode && !clang_mode && std_specifier &&
          entity_kind == iek_type && type_is_typedef((a_type_ptr)entity)) {
        /* An alignas on a typedef; treat it is though it were a GNU
           attribute (it's non-standard). */
        pos_warning(ec_wrong_entity_for_alignas, &ap->position);
        std_specifier = FALSE;
      }  /* if */
      if (!apply_value) {
        /* Nothing more to do. */
      } else if (gnu_version_is(<60000) &&
                 entity_kind == iek_type &&
                 is_immediate_enum_type((a_type_ptr)entity)) {
        /* Early versions of GCC appear to have ignored alignment attributes
           on enumeration types. */
        pos_warning(ec_attributes_ignored, &ap->position);
        make_attr_unrecognized(ap);
        attribute_discarded = TRUE;
      } else if (std_specifier ||
                 (gnu_mode && gnu_version >= 40800 &&
                  (entity_kind == iek_variable ||
                   entity_kind == iek_field))) {
        /* For standard alignment specifiers (i.e., alignas, _Alignas, and
           the early draft [[align()]]), don't apply the attribute immediately
           (i.e., here) because only the attribute with the strongest alignment
           is the effective alignment.  Record the attribute that has the
           strongest alignment here, then use that when
           record_strongest_alignment_attr is later called after the
           declaration/definition of the entity to make it effective (and issue
           appropriate errors).  Note that GCC version 4.8.0 and later use
           the "strongest" attribute rather than the "last" attribute (and
           are handled here), except when the attributes are applied to
           types (in which case the "last" attribute is the effective one). */
        check_assertion(dps != NULL);
        if (alignment > dps->alignment || use_last_attribute) {
          dps->alignment = alignment;
          dps->strongest_alignment = ap;
        }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (entity_kind == iek_field) {
          a_type_ptr  class_type = parent_class_of((a_field*)entity);
          class_type_supp(class_type)->has_explicitly_aligned_subobject = TRUE;
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      } else if (entity_kind == iek_field) {
        /* Apply the specified alignment.  This may be an increase or a
           decrease compared to the natural alignment of the type, but a
           lower #pragma pack setting will take precedence in non-Microsoft
           modes. */
        a_field_ptr       fp = (a_field_ptr)entity;
        a_targ_alignment  eff_alignment = alignment;
#if MICROSOFT_EXTENSIONS_ALLOWED
        a_type_ptr        class_type = parent_class_of(fp);
        class_type_supp(class_type)->has_explicitly_aligned_subobject = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        if (!microsoft_mode && current_pack_pragma_value() != 0 &&
            alignment > current_pack_pragma_value()) {
          eff_alignment = current_pack_pragma_value();
        }  /* if */
        fp->alignment = eff_alignment;
      } else if (entity_kind == iek_variable) {
        a_variable_ptr  vp = (a_variable_ptr)entity;
        if (use_last_attribute || is_gcc_attribute(ap)) {
          /* GCC retains the "last" applied alignment.  Declarator attributes
             are applied before prefix attributes. */
          vp->alignment = alignment;
        } else if (alignment > vp->alignment) {
          vp->alignment = alignment;
        }  /* if */
      } else if (entity_kind == iek_type) {
        a_type_ptr  tp = (a_type_ptr)entity;
        /* Set the alignment here.  When the actual class layout, or choice of
           integral type, is performed the value indicated here will be
           honored.  Note that this attribute applies to a typedef itself, not
           to its underlying type. */
        if (ap->family == af_ms_declspec) {
          if (type_is_typedef(tp) &&
              alignment < alignment_of_type(tp->variant.typeref.type)) {
            pos_warning(ec_declspec_align_reduction_ignored, &ap->position);
            make_attr_unrecognized(ap);
          } else if (is_immediate_enum_type(tp)) {
            /* Microsoft compilers ignore the attribute in
                 enum __declspec(align(16)) E {};
            */
            pos_warning(ec_extended_modifier_ignored_on_enum, &ap->position);
            make_attr_unrecognized(ap);
          } else {
            set_declspec_align(tp, alignment, &ap->position);
          }  /* if */
        } else {
          /* Apply the given alignment, but in the case of the standard
             "alignas" specifier (in non-GCC mode), ensure only the strictest
             alignment is recorded. */
          if (!tp->alignment_set_explicitly ||
              alignment > tp->alignment || use_last_attribute) {
            /* Set the alignment along with an indication that the alignment
               has been explicitly set.  Note that for enum types the
               underlying type may not yet be known (and the type is
               incomplete at this point), so the final alignment check will
               be done once the type becomes complete. */
            tp->alignment = alignment;
            tp->alignment_set_explicitly = TRUE;
          }  /* if */
        }  /* if */
      } else if (entity_kind == iek_routine) {
        a_type_ptr  func_type = get_func_type_for_attr(ap, &entity,
                                                       entity_kind);
        if (func_type != NULL) {
          func_type->alignment = alignment;
          func_type->alignment_set_explicitly = TRUE;
        }  /* if */
      } else if (entity_kind == iek_param_type) {
        if (clang_mode) {
          /* Clang accepts alignment attributes on parameter types but it's
             not clear what the effect is.  Record the attribute without any
             other change to the IL. */
        } else {
          /* GCC does not allow alignment attributes on most parameter types
             but does accept them on pointer types.  In both cases it's not
             clear what the effect is. */
          a_param_type_ptr ptp = (a_param_type_ptr)entity;
          a_type_ptr       tp = skip_typerefs(ptp->type);
          check_assertion(gnu_mode);
          if (!is_pointer_type(tp) && !is_reference_type(tp)) {
            report_bad_attribute_target(es_error, ap);
            make_attr_unrecognized(ap);
          }  /* if */
        }  /* if */
      } else {
        unexpected_condition();
      }  /* if */
      if (aap != NULL) aap = aap->next;
    } while (aap != NULL);
    if (is_unrecognized_attr(ap) && std_specifier && dps != NULL &&
        !attribute_discarded) {
      /* With the standard alignment specifier ("alignas") we may have seen
         some valid and some invalid arguments.  If we made the attribute
         unrecognized as a whole, discard any pending alignment updates. */
      expect_error();
      dps->alignment = 0;
    }  /* if */
  }  /* if */
  return entity;
}  /* apply_align_attr */


static char* apply_assume_attr(an_attribute_ptr             ap,
                               char                         *entity,
                               ARG_UNUSED an_il_entry_kind  entity_kind)
/*
Apply the given "assume" attribute to the null statement (pointed to by
entity).  The predicate argument was contextually converted to bool when it
was scanned, so all that remains to be checked here is that the statement the
attribute appertains to is indeed a null statement.
*/
{
#if CHECKING
  an_attribute_arg_ptr  aap = ap->arguments;
#endif /* CHECKING */
  a_statement_ptr       stmt;

  check_assertion(entity_kind == iek_statement &&
                  aap != NULL &&
                  aap->kind == aak_expression &&
                  aap->next == NULL);
  check_assertion(!is_error_node(expr_node_from_attribute_arg(aap)));
  stmt = (a_statement_ptr)entity;
  if (stmt->kind != stmk_empty) {
    /* The attribute should only apply to null statements. */
    an_error_severity  sev = (gpp_mode && !clang_mode) ? es_warning : es_error;
    pos_diagnostic(sev, ec_assume_statement_applies_to_null_statements,
                   &stmt->position);
    if (sev == es_error) {
      make_attr_unrecognized(ap);
    }  /* if */
  }  /* if */
  return entity;
}  /* apply_assume_attr */


static void issue_warning_for_removed_attribute(an_attribute_ptr  ap)
/*
The specified C++ attribute had appeared in a working version of the
standard (and had been implemented in the front end), but is not part
of the ratified C++11 standard.  A warning is issued in strict mode.
*/
{
  if (strict_ansi_mode) {
    check_assertion(ap->family == af_std && cpp11_mode);
    pos_diagnostic(es_warning, ec_attribute_is_nonstandard,
                   &ap->position);
  }  /* if */
}  /* issue_warning_for_removed_attribute */


static char* apply_base_check_attr(ARG_UNUSED an_attribute_ptr ap,
                                   char                        *entity,
                                   an_il_entry_kind            entity_kind)
/*
The given entity must be a class type.  Apply the "base_check" attribute to it
and return entity.  (The "checking" implied by "base_check" is delayed until
the class' definition has been completed.)  Note that this C++ attribute
appeared in working drafts of the C++11 standard, but is not part of the C++11
standard.
*/
{
  a_type_ptr  tp = (a_type_ptr)entity;

  issue_warning_for_removed_attribute(ap);
  check_assertion(entity_kind == iek_type);
  symbol_supplement_for_class(tp)->base_check = TRUE;
  return entity;
}  /* apply_base_check_attr */


static void check_carries_dependency_for_params(a_decl_parse_state_ptr  dps)
/*
Check constraints on the carries_dependency attribute specified on the
parameters in the given declaration.  (This function is set up as an
end-of-declaration callback when applying a carries_dependency attribute to
a parameter.  So we know that the declaration involved a function declarator.)
*/
{
  if (is_at_least_one_error() && is_or_contains_error_type(dps->type)) {
    /* Nothing to check. */
  } else if (dps->declared_type->kind != (a_type_kind)tk_routine ||
             dps->storage_class == (a_storage_class)sc_typedef) {
    /* Presumably the attribute was specified on a parameter that is not for
       a function declaration: An error. */
    a_type_ptr        f_type = dps->declared_type;
    a_param_type_ptr  ptp;

    if (dps->is_deduction_guide){
      /* For a deduction guide, the function type is stored in dps->type. */
      f_type = dps->type;
    }
    /* Look for the function type that has the parameter with the attribute. */
    while (f_type->kind != (a_type_kind)tk_routine) {
      f_type = underlying_type_of_derived_type(f_type);
      check_assertion(f_type != NULL);
    }  /* while */
    /* In each parameter, check for the erroneous presence of the
       [[carries_dependency]] attribute. */
    for (ptp = function_type_params(f_type); ptp != NULL; ptp = ptp->next) {
      if (ptp->attributes != NULL) {
        an_attribute_ptr  ap = find_attribute(ak_carries_dependency,
                                              ptp->attributes);
        if (ap != NULL) {
          report_bad_attribute_target(es_error, ap);
        }  /* if */
      }  /* if */
    }  /* for */
  } else if (dps->first_decl) {
    /* Nothing to check. */
  } else if (dps->prev_type == NULL || is_error_type(dps->prev_type)) {
    /* In some unusual error cases, first_decl may be FALSE, but the type of
       the preceding declaration is not available. */
    expect_error();
  } else {
    a_type_ptr        orig_type = skip_typerefs(dps->prev_type);
    a_param_type_ptr  ptp, orig_ptp;
    ptp = function_type_params(dps->declared_type);
    orig_ptp = function_type_params(orig_type);
    for (; ptp != NULL; ptp = ptp->next, orig_ptp = orig_ptp->next) {
      check_assertion(orig_ptp != NULL);
      if (ptp->attributes != NULL) {
        an_attribute_ptr  ap = find_attribute(ak_carries_dependency,
                                              ptp->attributes);
        if (ap != NULL &&
            (orig_ptp->attributes == NULL ||
             find_attribute(ak_carries_dependency,
                            orig_ptp->attributes) == NULL)) {
          pos_sy_error(ec_carries_dependency_not_on_first_decl,
                       &ap->position, dps->sym);
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
}  /* check_carries_dependency_for_params */


static char* apply_carries_dependency_attr(an_attribute_ptr  ap,
                                           char              *entity,
                                           an_il_entry_kind  entity_kind)
/*
The given entity must be a parameter or a routine.  Apply the 
"carries_dependency" attribute to it and return the entity.
*/
{
  a_decl_parse_state  *dps = (a_decl_parse_state*)ap->assoc_info;

  if (gpp_mode && !clang_mode) {
    /* GCC doesn't yet recognize the carries_dependency attribute; give a
       warning. */
    pos_warning(ec_carries_dependency_ignored, &ap->position);
    make_attr_unrecognized(ap);
  } else if (entity_kind == iek_param_type) {
    /* The constraints cannot be checked until the declarator containing these
       parameters is fully processed. */
    dps = dps->assoc_func_decl_state;
    check_assertion(dps != NULL);
    add_end_of_parse_action(check_carries_dependency_for_params, dps,
                            /*secondary_decls=*/FALSE);
  } else if (entity_kind == iek_routine) {
    if (dps != NULL && !dps->first_decl) {
      /* A redeclaration: Check that the attribute was present on the first
         declaration. */
      a_routine_ptr     rp = (a_routine_ptr)entity;
      an_attribute_ptr  prev;
      prev = find_attribute(ak_carries_dependency,
                            rp->source_corresp.attributes);
      check_assertion(prev != NULL);
      if (prev == ap) {
        /* The current attribute is the first "carries_dependency" attribute
           for this routine: Issue an error. */
        pos_sy_error(ec_carries_dependency_not_on_first_decl, &ap->position,
                     symbol_for(rp));
        make_attr_unrecognized(ap);
      }  /* if */
    }  /* if */
  } else {
    unexpected_condition();
  }  /* if */
  return entity;
}  /* apply_carries_dependency_attr */


static char* apply_conditional_explicit(an_attribute_ptr  ap,
                                        char              *entity,
                                        an_il_entry_kind  entity_kind)
/*
The given ak_conditional_explicit attribute (family: af_internal) represents a
C++20 "explicit(<bool-expression>)" construct.  It should only appear on
functions and function templates: If so, record it (otherwise, an error should
be issued by check_explicit_specifier).
*/
{
  if (entity_kind == iek_routine) {
    a_routine_ptr  rp = (a_routine_ptr)entity;
    a_type_ptr     func_type;
    ensure_underlying_function_type_is_modifiable(&rp->type, &func_type);
    if (func_type->kind == (a_type_kind)tk_routine) {
      an_attribute_arg_ptr  aap = ap->arguments;
      a_constant_ptr        arg_con;
      func_type->variant.routine.extra_info->is_conditionally_explicit = TRUE;
      check_assertion(aap != NULL && aap->next == NULL &&
                      aap->kind == (an_attribute_arg_kind)aak_constant);
      arg_con = aap->variant.constant;
      if (!constant_is(arg_con, ck_template_param) &&
          !is_false_constant(arg_con)) {
        if (special_kind_is(rp, sfk_constructor)) {
          rp->is_explicit_constructor = TRUE;
        } else if (special_kind_is(rp, sfk_conversion)) {
          rp->is_explicit_conversion_function = TRUE;
        } else {
          expect_error();
        }  /* if */
      }  /* if */
      { /* Move the attribute from the routine to the type. */
        an_attribute_ptr  *p_list = get_attribute_link(entity, entity_kind);
        while (*p_list != ap) {
          p_list = &(*p_list)->next;
        }  /* while */
        *p_list = ap->next;
        ap->next = func_type->source_corresp.attributes;
        func_type->source_corresp.attributes = ap;
      }
    }  /* if */
  } else {
    expect_error();
  }  /* if */
  return entity;
}  /* apply_conditional_explicit */


static void check_for_previous_string_literal(
                                        an_attribute_kind           kind,
                                        a_source_correspondence_ptr scp,
                                        a_constant_ptr              cp,
                                        a_source_position           *error_pos)
/*
This routine is used when scanning a string literal argument for the IL
entity denoted by scp.  The string literal being scanned is in cp and the
attribute kind is specified by "kind".  If the IL entity had a previous
string literal that was different, issue a remark (at error_pos).  Also, if
a wide string literal was specified, issue a remark (since it can't be used
in diagnostics).
*/
{
  an_attribute_ptr  prev_ap = attribute_string_literal_arg(kind, scp);

  if (!is_ordinary_string_constant(cp)) {
    pos_remark(ec_wide_deprecation_string, error_pos);
  }  /* if */
  if (prev_ap != NULL) {
    if (!eq_constants(prev_ap->arguments->variant.constant, cp)) {
      /* Note that if multiple attributes were recorded,
         attribute_string_literal_arg will return the first. */
      pos_remark(ec_decl_modifiers_incompatible_with_previous_decl, error_pos);
    }  /* if */
  }  /* if */
}  /* check_for_previous_string_literal */


static char* apply_deprecated_or_unavailable_attr(
                                                 an_attribute_ptr  ap,
                                                 char              *entity,
                                                 an_il_entry_kind  entity_kind)
/*
This routine applies the "deprecated" or "unavailable" (GNU and Clang modes
only) attribute to the given entity and returns it.  The processing for these
attributes is identical (except "deprecated" results in a warning and
"unavailable" in an error if the entity is later referenced).  Note that both
attributes can appertain to the same entity (though if the entity is referenced
the "unavailable" error will trump the "deprecated" warning).
*/
{
  check_assertion(entity_kind == iek_routine || entity_kind == iek_variable ||
                  entity_kind == iek_field || entity_kind == iek_type ||
                  entity_kind == iek_param_type ||
                  entity_kind == iek_namespace ||
                  entity_kind == iek_constant ||
                  entity_kind == iek_template);
  if (entity_kind == iek_type) {
    /* Only user-defined types can be marked as deprecated or unavailable. */
    a_type_ptr  tp = (a_type_ptr)entity;
    if (is_immediate_class_type(tp)) {
      /* Reset cached information. */
      class_type_supp(tp)->does_not_contain_deprecated_or_unavailable_type
                                                                       = FALSE;
    }  /* if */
    if (!(is_tag_type(tp) || type_is_typedef(tp))) {
      report_bad_attribute_target(es_warning, ap);
    } else if (ap->family == af_ms_declspec &&
               ap->syntactic_location == al_tag_name) {
      /* Microsoft compilers ignore the attribute on enum types and on
         unnamed classes. */
      if (is_immediate_enum_type(tp)) {
        pos_warning(ec_extended_modifier_ignored_on_enum, &ap->position);
        make_attr_unrecognized(ap);
      } else if (tp->variant.class_struct_union.originally_unnamed) {
        pos_st_warning(ec_attribute_ignored_on_unnamed_type,
                       &ap->position, attribute_display_name(ap));
        make_attr_unrecognized(ap);
      }  /* if */
    }  /* if */
  } else if (entity_kind == iek_param_type) {
    /* Note that when the entity is a parameter type, the attribute is later
       transferred to the corresponding parameter variable. */
    if (ap->family == af_ms_declspec) {
      /* Microsoft appears to accept and then discard the attribute. */
      make_attr_unrecognized(ap);
    }  /* if */
  }  /* if */
  if (!is_unrecognized_attr(ap)) {
    a_source_correspondence *scp = source_corresp_for_il_entry(entity,
                                                               entity_kind);
    if (ap->arguments != NULL) {
      an_attribute_arg_ptr  aap = ap->arguments;
      a_constant_ptr        cp;
      check_assertion(aap->kind == aak_constant);
      cp = aap->variant.constant;
      check_assertion(cp->kind == ck_string &&
                      cp->variant.string.value[cp->variant.string.length-1] ==
                                                                         '\0');
      if ((ap->family == af_ms_declspec &&
           microsoft_mode && microsoft_version < 1400) ||
          (ap->family == af_gnu &&
           gnu_mode && gnu_version < 40500)) {
        /* Only Microsoft and GNU compilers of recent vintage allow an
           optional string argument. */
        report_bad_attribute_arg(aap, ap);
      } else if (scp != NULL) {
        /* Issue any diagnostics associated with the string literal, if
           needed. */
        check_for_previous_string_literal(ap->kind, scp, cp, &aap->position);
      }  /* if */
      if (aap->next != NULL) {
        /* Clang allows for two strings in their deprecated attribute (a
           "message" and a "replacement").  A redeclaration with different
           strings isn't flagged by Clang (and is not checked here). */
        aap = aap->next;
        check_assertion(aap->kind == aak_constant);
        cp = aap->variant.constant;
        check_assertion(cp->kind == ck_string &&
                        cp->variant.string.value[cp->variant.string.length-1]
                                                                      == '\0');
      }  /* if */
    }  /* if */
    if (scp != NULL && ap->kind != ak_unrecognized) {
      scp->is_deprecated_or_unavailable = TRUE;
    }  /* if */
  }  /* if */
  return entity;
}  /* apply_deprecated_or_unavailable_attr */
 

an_attribute_ptr attribute_string_literal_arg(an_attribute_kind           kind,
                                              a_source_correspondence_ptr scp)
/*
If the specified IL entity (scp) has an attribute of "kind" and that attribute
has a string literal associated with it, return that attribute.  Note that the
string literal may or may not be a narrow string literal.  Note also that
only the first such matching attribute is returned.
*/
{
  an_attribute_ptr  result = NULL;
  an_attribute_ptr  ap = scp->attributes;

  for (; ap != NULL; ap = ap->next) {
    if (ap->kind == kind &&
        ap->arguments != NULL &&
        ap->arguments->next == NULL &&
        ap->arguments->kind == (an_attribute_arg_kind)aak_constant &&
        ap->arguments->variant.constant->kind ==
                                           (a_constant_repr_kind)ck_string) {
      result = ap;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* attribute_string_literal_arg */


a_const_char *attribute_string_for_kind(an_attribute_kind           kind,
                                        a_source_correspondence_ptr scp)
/*
Return the value of the narrow string literal recorded for the particular
attribute kind (if any) applied to the entity associated with scp.
*/
{
  a_const_char      *result = NULL;

  an_attribute_ptr  ap = attribute_string_literal_arg(kind, scp);
  if (ap != NULL &&
      is_ordinary_string_constant(ap->arguments->variant.constant)) {
    result = ap->arguments->variant.constant->variant.string.value;
  }  /* if */
  return result;
}  /* attribute_string_for_kind */


static char* apply_final_attr(an_attribute_ptr  ap,
                              char              *entity,
                              an_il_entry_kind  entity_kind)
/*
The given entity must be a member function or a class.  Apply the "final"
attribute to it and return the entity.  Note that this C++ attribute
appeared in working drafts of the C++11 standard, but is not part of the
C++11 standard.
*/
{
  issue_warning_for_removed_attribute(ap);
  if (entity_kind == iek_routine) {
    a_routine_ptr  rp = (a_routine_ptr)entity;
    a_type_ptr     parent_class = parent_class_of(rp);
    if (!is_incomplete_type(parent_class)) {
      /* Since the class is complete, the attribute is being applied to an
         out-of-class member definition, which is invalid. */
      pos_st_error(ec_attr_must_appear_in_class_definition,
                   &ap->position, attribute_display_name(ap));
      make_attr_unrecognized(ap);
    } else {
      rp->final = TRUE;
    }  /* if */
  } else if (entity_kind == iek_type) {
    a_type_ptr  tp = (a_type_ptr)entity;
    check_assertion(is_immediate_class_type(tp));
    tp->variant.class_struct_union.final = TRUE;
  } else {
    unexpected_condition();
  }  /* if */
  return entity;
}  /* apply_final_attr */


static char* apply_hiding_attr(an_attribute_ptr            ap,
                               char                        *entity,
                               ARG_UNUSED an_il_entry_kind entity_kind)
/*
Check that the "hiding" attribute appears in a class definition.  Additional
checking is delayed until the definition is complete (because later member
declarations can affect the validity of the attribute), but record the use
of this attribute in the class to avoid unnecessary work for class definitions
that do not involve the "hiding" attribute.  Return the given entity.

Note that this C++ attribute appeared in working drafts of the C++11 standard,
but is not part of the C++11 standard.
*/
{
  a_decl_parse_state  *dps = (a_decl_parse_state*)ap->assoc_info;

  issue_warning_for_removed_attribute(ap);
  if (scope_stack_top().kind != (a_scope_kind)sck_class_struct_union) {
    pos_st_error(ec_attr_must_appear_in_class_definition,
                 &ap->position, attribute_display_name(ap));
    make_attr_unrecognized(ap);
  } else if (dps != NULL && (dps->dso_flags & DSO_FRIEND) != 0) {
    /* Something like "friend class[[hiding]] X;" is invalid. */
    report_bad_attribute_target(es_error, ap);
  } else {
    symbol_supplement_for_class(scope_stack_top().assoc_type)
                                                   ->check_hiding_attr = TRUE;
  }  /* if */
  return entity;
}  /* apply_hiding_attr */


static char* apply_noreturn_attr(an_attribute_ptr  ap,
                                 char              *entity,
                                 an_il_entry_kind  entity_kind)
/*
Apply the given "noreturn" attribute to the given entity and return that
entity.
*/
{
  if (entity_kind != iek_routine && !is_gcc_attribute(ap)) {
    /* The standard attribute form and the Microsoft __declspec form apply only
       to routines.  (Early Microsoft compilers simply ignore the attribute
       when it is applied to a non-routine.) */
    an_error_severity  sev;
    sev = (microsoft_mode && microsoft_version < 1400) ? es_warning : es_error;
    report_bad_attribute_target(sev, ap);
  } else if (ap->family == af_std) {
    /* The standard attribute has more constraints than the corresponding GNU
       attribute: It can appear on a routine only, and it must appear on the
       first declaration of that routine. */
    a_routine_ptr       rp = (a_routine_ptr)entity;
    a_decl_parse_state  *dps = (a_decl_parse_state*)ap->assoc_info;
    if (ap->syntactic_location != al_lambda_expression &&
        dps != NULL && !dps->first_decl) {
      /* A redeclaration: The attribute should have appeared on the first
         declaration.  (It is tempting to use rp->type to test whether the
         [[noreturn]] attribute appeared previously.  However, that doesn't
         work with block-extern declarations in some cases.  For example:
           void g1() {  [[noreturn]] void f(); }
           void g2() {
             [[noreturn]] void f();  // [[noreturn]] from g1() is not
           }                         // indicated in rp->type.
         Instead we may have to use the underlying sk_extern_routine symbol.
      */
      a_type_ptr  prev_type;
      if ((rp->storage_class != (a_storage_class)sc_extern &&
           rp->storage_class != (a_storage_class)sc_unspecified) ||
          rp->source_corresp.is_class_member ||
          rp->is_template_function ||
          rp->type->variant.routine.extra_info->does_not_return) {
        /* Cases that may not have an associated sk_extern_routine symbol.
           Fortunately, these don't run into problems with block-extern
           declarations either. */
        prev_type = rp->type;
      } else if (scope_stack[depth_scope_stack].in_prototype_instantiation) {
        /* A block-extern declaration in a prototype instantiation.  In this
           case no sk_extern_routine symbol is available either, but it is
           unclear whether this constitutes an actual declaration while no
           real instantiation has been done.  Nonetheless, we diagnose cases
           like:
             template<class T> void g() {
               int f();
               [[noreturn]] int f();
             }
           if prototype instantiations of function templates are done. */
        prev_type = dps->prev_type;
      } else {
        a_symbol_locator  loc, eloc;
        a_symbol_ptr      esym;
        make_locator_for_symbol(symbol_for(rp), &loc);
        esym = find_external_symbol(&loc, enum_cast<a_name_linkage_kind>(
                                              rp->source_corresp.name_linkage),
                                    rp->type, rp->trailing_requires_clause,
                                    &eloc);
        check_assertion(esym != NULL &&
                        esym->kind == (a_symbol_kind)sk_extern_routine);
        prev_type = esym->variant.extern_symbol_descr->type;
      }  /* if */
      if (prev_type != NULL &&
          !prev_type->variant.routine.extra_info->does_not_return) {
        pos_st_error(ec_attr_must_also_appear_in_first_declaration,
                     &ap->position, attribute_display_name(ap));
        make_attr_unrecognized(ap);
      }  /* if */
    }  /* if */
  }  /* if */
  /* If applicable, update the routine type entry. */
  if (!is_unrecognized_attr(ap)) {
    a_type_ptr  func_type = get_func_type_for_attr(ap, &entity, entity_kind);
    if (func_type != NULL) {
      func_type->variant.routine.extra_info->does_not_return = TRUE;
      if (ap->family == af_std &&
          C_mode() && !is_void_type(func_type->variant.routine.return_type)) {
        /* C23 suggests a diagnostic on noreturn routines with non-void
           return types. */
        pos_warning(ec_noreturn_with_return_type, &ap->position);
      }  /* if */
    }  /* if */
  }  /* if */
  return entity;
}  /* apply_noreturn_attr */


static char* apply_override_attr(an_attribute_ptr  ap,
                                 char              *entity,
                                 an_il_entry_kind  entity_kind)
/*
Apply the given "override" attribute to the given entity and return that
entity.  Note that this C++ attribute appeared in working drafts of the C++11
standard, but is not part of the C++11 standard.
*/
{
  issue_warning_for_removed_attribute(ap);
  check_assertion(entity_kind == iek_routine);
  if (scope_stack_top().kind != (a_scope_kind)sck_class_struct_union) {
    /* The attribute is presumably being applied to an out-of-class member
       definition, which is invalid. */
    pos_st_error(ec_attr_must_appear_in_class_definition,
                 &ap->position, attribute_display_name(ap));
    make_attr_unrecognized(ap);
  } else {
    a_decl_parse_state  *dps = (a_decl_parse_state*)ap->assoc_info;
    if (!dps->override_okay) {
      pos_error(ec_override_member_does_not_override, &dps->declarator_pos);
    }  /* if */
  }  /* if */
  return entity;
}  /* apply_override_attr */


static char* apply_nodiscard_attr(an_attribute_ptr  ap,
                                  char              *entity,
                                  an_il_entry_kind  entity_kind)
/*
Apply the given "nodiscard" attribute to the given entity (which must be a
routine, class, or enum) and return that entity.  See also the GCC
"warn_unused_result" attribute which is similar (but applies to the routine's
type rather than to the routine itself).
*/
{
  check_assertion(nodiscard_attribute_enabled);
  if (entity_kind == iek_routine) {
    a_routine_ptr rp = (a_routine_ptr)entity;
    if (rp->special_kind == (a_special_function_kind)sfk_destructor ||
        (rp->type->variant.routine.return_type != NULL &&
         rp->special_kind != (a_special_function_kind)sfk_constructor &&
         is_void_type(rp->type->variant.routine.return_type) &&
        (!rp->is_template_function || rp->is_prototype_instantiation) &&
        !curr_scope_is_class_instantiation())) {
      /* It doesn't make sense to apply this attribute to a routine with a
         void return type, or to destructors -- issue a warning
         (but not for real instantiations).  The attribute is allowed on
         constructors (as of P1771R1). */
      pos_warning(ec_nodiscard_doesnt_apply, &ap->position);
      make_attr_unrecognized(ap);
    } else {
      rp->has_nodiscard_attribute = TRUE;
    }  /* if */
  } else if (entity_kind == iek_type) {
    a_type_ptr  type = (a_type_ptr)entity;
    type = skip_typerefs(type);
    if (is_immediate_class_type(type)) {
      class_type_supp(type)->has_nodiscard_attribute = TRUE;
    } else if (is_immediate_enum_type(type)) {
      integer_type_supp(type)->has_nodiscard_attribute = TRUE;
    } else {
      unexpected_condition();
    }  /* if */
  } else {
    unexpected_condition();
  }  /* if */
  if (!is_unrecognized_attr(ap) && ap->arguments != NULL) {
    /* Check for an optional string literal argument (as of P1301R4). */
    a_source_correspondence *scp = source_corresp_for_il_entry(entity,
                                                               entity_kind);
    an_attribute_arg_ptr  aap = ap->arguments;
    a_constant_ptr        cp;
    check_assertion(aap->next == NULL &&
                    aap->kind == (an_attribute_arg_kind)aak_constant);
    cp = aap->variant.constant;
    check_assertion(cp->kind == (a_constant_repr_kind)ck_string &&
                    cp->variant.string.value[cp->variant.string.length-1] ==
                                                                         '\0');
    /* Issue any diagnostics associated with the string literal, if needed. */
    check_for_previous_string_literal(ak_nodiscard, scp, cp, &aap->position);
  }  /* if */
  return entity;
}  /* apply_nodiscard_attr */


static char* apply_maybe_unused_attr(an_attribute_ptr  ap,
                                     char              *entity,
                                     an_il_entry_kind  entity_kind)
/*
Apply the given "maybe_unused" attribute to the given entity and return that
entity.  Note that the front end doesn't necessarily detect all cases of
unused entities even without this attribute.  See also the GCC "unused"
attribute, which is similar.
*/
{
  if (entity_kind == iek_type) {
    a_type_ptr type = (a_type_ptr)entity;
    if (is_immediate_class_type(type) ||
        is_immediate_enum_type(type) ||
        type_is_typedef(type)) {
      /* The attribute applies to class, enumeration, and typedef types. */
    } else {
      /* The attribute does not apply to this type. */
      report_bad_attribute_target(es_error, ap);
      make_attr_unrecognized(ap);
    }  /* if */
  }  /* if */
  if (!is_unrecognized_attr(ap)) {
    if (entity_kind == iek_param_type) {
      /* Attributes applied to parameter types are attached to the
         associated variable at a later time (see
         attach_param_variable_attributes). */
    } else if (entity_kind == iek_label &&
               !C_mode() && strict_ansi_mode) {
      /* Starting with C23, maybe_unused is allowed on a label.  Strictly
         speaking C++ does not currently allow it (though most implementations
         seem to). */
      report_bad_attribute_target(strict_ansi_discretionary_severity, ap);
      if (strict_ansi_discretionary_severity <= es_warning) {
        ((a_source_correspondence_ptr)entity)->maybe_unused = TRUE;
      } else {
        make_attr_unrecognized(ap);
      }  /* if */
    } else {
      ((a_source_correspondence_ptr)entity)->maybe_unused = TRUE;
    }  /* if */
  }  /* if */
  return entity;
}  /* apply_maybe_unused_attr */


static char* apply_fallthrough_attr(an_attribute_ptr  ap,
                                    char              *entity,
                                    an_il_entry_kind  entity_kind)
/*
Apply the given "fallthrough" attribute to the specified statement entity and
return that entity.  Note that the syntax is validated, but the attribute
currently has no affect in the front end (i.e., the front end does not
currently diagnose falling from one switch case to another so there is no
diagnostic to suppress).
*/
{
  a_statement_ptr sp = (a_statement_ptr)entity;

  check_assertion(entity_kind == iek_statement);
  if (sp->kind != (a_statement_kind)stmk_empty) {
    /* The attribute must be applied to a null statement. */
    pos_diagnostic(clang_mode ? es_error :
                                strict_ansi_discretionary_severity,
                   ec_fallthrough_applies_to_null_statement, &ap->position);
    make_attr_unrecognized(ap);
  } else if (!in_switch_statement()) {
    /* The attribute must appear within an enclosing switch statement. */
    pos_error(ec_fallthrough_not_in_switch, &ap->position);
    make_attr_unrecognized(ap);
  } else {
    /* Apply the attribute to the statement.  Note that the check to verify
       that the statement is followed by a case label or default label is
       performed during statement processing (and an appropriate diagnostic
       issued at that time). */
    sp->is_fallthrough_statement = TRUE;
  }  /* if */
  return entity;
}  /* apply_fallthrough_attr */


static char* apply_likely_attr(an_attribute_ptr  ap,
                               char              *entity,
                               an_il_entry_kind  entity_kind)
/*
Apply the "likely" or "unlikely" attribute to the specified label or statement
entity and return that entity.  Note that the syntax is validated, but the
attribute currently has no effect in the front end.
*/
{
  a_boolean err = FALSE;

  if (entity_kind == iek_statement) {
    a_statement_ptr sp = (a_statement_ptr)entity;
    if ((sp->is_likely && ap->kind == ak_unlikely) ||
        (sp->is_unlikely && ap->kind == ak_likely)) {
      /* A conflict with a previous attribute. */
      err = TRUE;
    } else if (ap->kind == ak_likely) {
      sp->is_likely = TRUE;
    } else {
      sp->is_unlikely = TRUE;
    }  /* if */
  } else if (entity_kind == iek_label) {
    a_label_ptr lp = (a_label_ptr)entity;
    if ((lp->is_likely && ap->kind == ak_unlikely) ||
        (lp->is_unlikely && ap->kind == ak_likely)) {
      /* A conflict with a previous attribute. */
      err = TRUE;
    } else if (ap->kind == ak_likely) {
      lp->is_likely = TRUE;
    } else {
      lp->is_unlikely = TRUE;
    }  /* if */
  } else {
    unexpected_condition();
  }  /* if */
  if (err) {
    /* A previous likely/unlikely attribute has already been specified. */
    pos_error(ec_likely_unlikely_conflict, &ap->position);
  }  /* if */
  return entity;
}  /* apply_likely_attr */


static char* apply_no_unique_address_attr(
                                       ARG_UNUSED an_attribute_ptr ap,
                                       char                        *entity,
                                       an_il_entry_kind            entity_kind)
/*
Apply the "no_unique_address" attribute to the field and return that entity.
*/
{
  a_field_ptr fp = (a_field_ptr)entity;

  check_assertion(entity_kind == iek_field);
  fp->has_no_unique_address_attribute = TRUE;
#if !IA64_ABI
  no_unique_address_attribute_seen = TRUE;
#endif /* !IA64_ABI */
  return entity;
}  /* apply_no_unique_address_attr */


static void check_indeterminate_for_params(a_decl_parse_state_ptr  dps)
/*
Check constraints on the "indeterminate" attribute specified on the parameters
in the given declaration.  (This function is set up as an end-of-declaration
callback when applying an "indeterminate" attribute to a parameter.  So we know
that the declaration involved a function declarator.)
*/
{
  a_type_ptr  f_type = dps->declared_type;

  if (is_at_least_one_error() && is_or_contains_error_type(dps->type)) {
    /* Nothing to check. */
  } else if (!type_is(f_type, tk_routine) ||
             dps->storage_class == sc_typedef) {
    /* Presumably the attribute was specified on a parameter that is not for
       a function declaration: An error. */
    a_param_type_ptr  ptp;
    if (dps->is_deduction_guide){
      /* For a deduction guide, the function type is stored in dps->type. */
      f_type = dps->type;
    }
    /* Look for the function type that has the parameter with the attribute. */
    while (!type_is(f_type, tk_routine)) {
      f_type = underlying_type_of_derived_type(f_type);
      check_assertion(f_type != NULL);
    }  /* while */
    /* In each parameter, check for the erroneous presence of the
       [[indeterminate]] attribute. */
    for (ptp = function_type_params(f_type); ptp != NULL; ptp = ptp->next) {
      if (ptp->attributes != NULL) {
        an_attribute_ptr  ap = find_attribute(ak_indeterminate,
                                              ptp->attributes);
        if (ap != NULL) {
          report_bad_attribute_target(es_error, ap);
        }  /* if */
      }  /* if */
    }  /* for */
  } else if (dps->first_decl) {
    /* Nothing to check. */
  } else if (dps->prev_type == NULL || is_error_type(dps->prev_type)) {
    /* In some unusual error cases, first_decl may be FALSE, but the type of
       the preceding declaration is not available. */
    expect_error();
  } else {
    a_type_ptr        orig_type = skip_typerefs(dps->prev_type);
    a_param_type_ptr  ptp, orig_ptp;
    ptp = function_type_params(dps->declared_type);
    orig_ptp = function_type_params(orig_type);
    for (; ptp != NULL; ptp = ptp->next, orig_ptp = orig_ptp->next) {
      check_assertion(orig_ptp != NULL);
      if (ptp->attributes != NULL) {
        an_attribute_ptr  ap = find_attribute(ak_indeterminate,
                                              ptp->attributes);
        if (ap != NULL &&
            (orig_ptp->attributes == NULL ||
             !has_attr(ak_indeterminate, orig_ptp->attributes))) {
          pos_sy_error(ec_indeterminate_not_on_first_decl, &ap->position,
                       dps->sym);
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
}  /* check_indeterminate_for_params */


static char* apply_indeterminate_attr(ARG_UNUSED an_attribute_ptr ap,
                                      char                        *entity,
                                      an_il_entry_kind            entity_kind)
/*
Apply the "indeterminate" attribute to a variable or parameter and return that
entity.
*/
{
  a_decl_parse_state  *dps = (a_decl_parse_state*)ap->assoc_info;

  if (entity_kind == iek_param_type) {
    /* The constraints cannot be checked until the declarator containing this
       parameter is fully processed. */
    dps = dps->assoc_func_decl_state;
    check_assertion(dps != NULL);
    add_end_of_parse_action(check_indeterminate_for_params, dps,
                            /*secondary_decls=*/FALSE);
  } else if (entity_kind == iek_variable) {
    /* Okay. */
  } else {
    unexpected_condition();
  }  /* if */
  return entity;
}  /* apply_indeterminate_attr */


static void deferred_check_enable_if_attr(a_decl_parse_state_ptr  dps)
/*
A check for the "enable_if" attribute has been deferred and can now be
completed.
*/
{
  if (dps->sym == NULL || !is_function_or_template_symbol(dps->sym)) {
    pos_st_warning(ec_wrong_entity_for_attribute, &dps->start_pos,
                   "enable_if");
  }  /* if */
}  /* deferred_check_enable_if_attr */


static char* apply_enable_if_attr(an_attribute_ptr  ap,
                                  char              *entity,
                                  an_il_entry_kind  entity_kind)
/*
The given entity must be a routine type.  Apply the Clang "enable_if" attribute
to it and return the entity.
*/
{
  an_attribute_arg_ptr  aap = ap->arguments;
  an_expr_node_ptr      expr;

  check_assertion(entity_kind == iek_type && aap != NULL &&
                  aap->kind == aak_expression && aap->next != NULL);
  expr = expr_node_from_attribute_arg(aap);
  check_assertion(!is_error_node(expr));
  expr = process_boolean_attribute_expression(expr);
  aap->variant.expr = expr;
  if (is_error_node(expr)) {
    /* The expression must be convertible to bool. */
    make_attr_unrecognized(ap);
  } else {
    a_type_ptr          rtp = (a_type_ptr)entity;
    a_decl_parse_state  *dps = (a_decl_parse_state*)ap->assoc_info;
    check_assertion(dps != NULL);
    if (rtp->kind != (a_type_kind)tk_routine || dps->in_nested_declarator) {
      pos_st_warning(ec_wrong_entity_for_attribute, &ap->position,
                     attribute_display_name(ap));
      make_attr_unrecognized(ap);
    } else {
      /* Set a flag on the routine's type to indicate that an enable_if
         attribute is present. */
      rtp->variant.routine.extra_info->has_enable_if_attribute = TRUE;
    }  /* if */
    if (ap->kind == ak_enable_if) {
      add_end_of_parse_action(deferred_check_enable_if_attr, dps,
                              /*secondary_decls=*/TRUE);
    }  /* if */
  }  /* if */
  return entity;
}  /* apply_enable_if_attr */


static void check_pass_object_size_attr(a_decl_parse_state_ptr  dps)
/*
dps should represent a function or function declaration.  If it doesn't, issue
an error; otherwise, record the presence of the Clang "pass_object_size"
attribute on at least one of the parameters.
*/
{
  if (dps->sym == NULL || !is_simple_function_symbol(dps->sym)) {
    pos_error(ec_pass_object_size_not_in_function_decl, &dps->declarator_pos);
  } else {
    func_sym_routine(dps->sym)->has_pass_object_size_attr = TRUE;
  }  /* if */
}  /* check_pass_object_size_attr */


static char* apply_pass_object_size_attr(an_attribute_ptr  ap,
                                         char              *entity,
                                         an_il_entry_kind  entity_kind)
/*
The given entity must be a parameter.  Apply the Clang "pass_object_size"
attribute to it and return the entity.
*/
{
  an_attribute_arg_ptr  aap = ap->arguments;
  a_constant_ptr        arg;
  a_host_large_integer  flags = 0;
  a_decl_parse_state    *dps = (a_decl_parse_state*)ap->assoc_info;
  a_boolean             ovflo;

  check_assertion(entity_kind == iek_param_type &&
                  aap != NULL && aap->kind == aak_constant &&
                  aap->next == NULL);
  arg = aap->variant.constant;
  check_assertion(constant_is(arg, ck_integer));
  flags = value_of_integer_constant(arg, &ovflo);
  if (ovflo || flags < 0 || flags > 3) {
    pos_st_num2_diagnostic(es_error, ec_attr_arg_out_of_small_integer_range,
                           &ap->position, "pass_object_size", (int32_t)flags,
                           3);
    make_attr_unrecognized(ap);
  } else {
    add_end_of_parse_action(check_pass_object_size_attr,
                            dps->assoc_func_decl_state,
                            /*secondary_decls=*/TRUE);
  }  /* if */
  return entity;
}  /* apply_pass_object_size_attr */

#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
#if GNU_NAKED_ATTRIBUTE_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED

static char* apply_naked_attr(ARG_UNUSED an_attribute_ptr ap,
                              char                        *entity,
                              an_il_entry_kind            entity_kind)
/*
Apply the GNU or Microsoft "naked" attribute to the given entity and return
that entity.
*/
{
  check_assertion(entity_kind == iek_routine);
  ((a_routine*)entity)->is_naked = TRUE;
  return entity;
}  /* apply_naked_attr */

#endif /* GNU_NAKED_ATTRIBUTE_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */

static a_boolean gnu_accepts_noinline_on_inline_routine(an_attribute_ptr  ap)
/*
ap is a GNU "noinline" attribute being applied to an inline routine.  Return
TRUE if GCC accepts that combination silently in the context of ap. For
example:

  inline __attribute__((noinline)) int f() { return 0; }  // No warning.

This idiom is used to ensure a noinline function in a header file does not
cause linker errors. GCC in C mode will warn on this while it does not in C++
*/
{
  a_boolean           result = FALSE;
  a_decl_parse_state  *dps = (a_decl_parse_state*)ap->assoc_info;

  if (!C_mode()) {
    if (dps == NULL) {
      /* The attribute is not being applied as part of a declaration, which
         means it is being copied to a template instance. */
      result = TRUE;
    } else {
      result = ap->on_primary_declaration &&
               (dps->dso_flags &
                     (DSO_INLINE | DSO_CONSTEXPR | DSO_CONSTEVAL)) != 0 &&
               !dps->inline_on_earlier_decl;
    }  /* if */
  }  /* if */
  return result;
}  /* gnu_accepts_noinline_on_inline_routine */


static char* apply_noinline_attr(an_attribute_ptr  ap,
                                 char              *entity,
                                 an_il_entry_kind  entity_kind)
/*
The given entity must be a routine.  Apply the GNU "noinline" attribute to it
and return the entity.
*/
{
  if (entity_kind == iek_routine) {
    a_routine_ptr  rp = (a_routine_ptr)entity;
    a_boolean      has_always_inline = FALSE;
#if GNU_EXTENSIONS_ALLOWED
    has_always_inline = find_attribute(ak_always_inline,
                                       rp->source_corresp.attributes) != NULL;
#endif /* GNU_EXTENSIONS_ALLOWED */
    rp->never_inline = TRUE;
    if (rp->is_inline && is_gcc_attribute(ap) &&
        (!rp->source_corresp.is_class_member ||
         rp->defined_outside_of_parent || has_always_inline) &&
        (has_always_inline || !gnu_accepts_noinline_on_inline_routine(ap))) {
      pos_warning(ec_inline_gnu_noinline_conflict, &ap->position);
    }  /* if */
  } else {
    an_error_severity  sev;
    if (gnu_mode || (microsoft_mode && microsoft_version < 1400)) {
      sev = es_warning;
    } else {
      sev = es_error;
    }  /* if */
    report_bad_attribute_target(sev, ap);
  }  /* if */
  return entity;
}  /* apply_noinline_attr */


static char* apply_nothrow_attr(an_attribute_ptr  ap,
                                char              *entity,
                                an_il_entry_kind  entity_kind)
/*
Apply the given "nothrow" attribute to the given entity and return that
entity.
*/
{
  if (entity_kind != iek_routine) {
    /* The nothrow attribute applies only to routines, but GCC only issues a
       warning in some other contexts. */
    an_error_severity  sev = is_gcc_attribute(ap) ? es_warning : es_error;
    report_bad_attribute_target(sev, ap);
    goto done;
  } else {
    a_routine_ptr  rp = (a_routine_ptr)entity;
    if (!is_unrecognized_attr(ap)) {
      rp->never_throws = TRUE;
      if (ap->family == af_ms_declspec && !C_mode()) {
        /* The Microsoft __declspec(nothrow) attribute implies "noexcept" if
           no specification was previously specified. */
        a_type_ptr  func_tp;
        ensure_underlying_function_type_is_modifiable(&rp->type, &func_tp);
        if (rout_type_supp(func_tp)->exception_specification == NULL) {
          an_exception_specification_ptr
                      esp = alloc_exception_specification();
          esp->is_noexcept = TRUE;
          esp->compiler_generated = TRUE;
          esp->from_attribute = TRUE;
          rout_type_supp(func_tp)->exception_specification = esp;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
done:
  return entity;
}  /* apply_nothrow_attr */


static char* apply_pure_attr(ARG_UNUSED an_attribute_ptr ap,
                             char                        *entity,
                             an_il_entry_kind            entity_kind)
/*
Apply the "pure" attribute to the specified entity (which must be a routine).
*/
{
  check_assertion(entity_kind == iek_routine);
  ((a_routine_ptr)entity)->is_pure = TRUE;
  return entity;
}  /* apply_pure_attr */


static char* apply_section_attr(an_attribute_ptr  ap,
                                char              *entity,
                                an_il_entry_kind  entity_kind)
/*
The given entity must be a function or variable.  Apply the GNU "section"
or Microsoft "allocate" attribute to it and return the entity.
*/
{
  an_attribute_arg_ptr  aap = ap->arguments;
  a_constant_ptr        arg;
  a_const_char          *str;

  check_assertion(entity_kind == iek_routine || entity_kind == iek_variable);
  check_assertion(aap != NULL && aap->next == NULL &&
                  aap->kind == (an_attribute_arg_kind)aak_constant);
  arg = aap->variant.constant;
  check_assertion(arg->kind == (a_constant_repr_kind)ck_string);
  str = arg->variant.string.value;
  if (ap->family == af_ms_declspec) {
    /* Microsoft compilers disallow different section names on different
       declarations.  (GNU compilers retain the last section name.)  Also,
       unlike GNU compilers, Microsoft only allows a section name to appear
       on a variable declaration; not a routine declaration. */
    if (entity_kind == iek_routine) {
      report_bad_attribute_target(es_error, ap);
    } else {
      a_const_char *prev_str = ((a_variable_ptr)entity)->section;
      if (prev_str != NULL && strcmp(prev_str, str) != 0) {
        pos_diagnostic(es_discretionary_error,
                       ec_decl_modifiers_incompatible_with_previous_decl,
                       &ap->position);
        make_attr_unrecognized(ap);
      }  /* if */
    }  /* if */
  }  /* if */
  if (!is_unrecognized_attr(ap)) {
    /* Make a separate copy of the string value because it will be traversed as
       iek_other_text whereas the original pointed to by the ck_string will be
       traversed as iek_string_text. */
    str = copy_string_to_region(file_scope_region_number, str);
    if (entity_kind == iek_variable) {
      ((a_variable_ptr)entity)->section = str;
#if GNU_EXTENSIONS_ALLOWED
    } else {
      ensure_gnu_routine_supp(((a_routine_ptr)entity))->section = str;
#endif /* GNU_EXTENSIONS_ALLOWED */
    }  /* if */
  }  /* if */
  return entity;
}  /* apply_section_attr */

#endif /* GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED

static void add_alias_fixup(a_symbol_ptr        alias,
                            a_const_char        *alias_name,
                            a_const_char        *aliased_name,
                            a_source_position   *alias_position);

static char* apply_alias_attr(an_attribute_ptr  ap,
                              char              *entity,
                              an_il_entry_kind  entity_kind)
/*
Apply the given GNU "alias" attribute to the given routine or variable, and
return the routine or variable.  This function may also be called for the
"weakref" attribute (via apply_weakref_attr).
*/
{
  a_constant_ptr  arg;

  check_assertion(ap->arguments != NULL && ap->arguments->next == NULL &&
                  ap->arguments->kind == (an_attribute_arg_kind)aak_constant);
  arg = ap->arguments->variant.constant;
  check_assertion(arg->kind == (a_constant_repr_kind)ck_string);
  if (entity_kind == iek_routine) {
    if (gnu_version >= 40000 && innermost_function_scope != NULL) {
      /* Recent versions of GCC ignore attributes on block-extern function
         declarations. */
      pos_st_warning(ec_local_function_attribute_ignored, &ap->position,
                     attribute_display_name(ap));
      make_attr_unrecognized(ap);
    } else {
      a_routine_ptr  rp = (a_routine_ptr)entity;
      if (rp->is_ifunc) {
        /* Can't be both an alias and an ifunc. */
        pos_error(ec_ifunc_cant_be_alias, &ap->position);
      } else {
        rp->implicit_alias = FALSE;
        if (ap->kind == ak_alias) {
          rp->is_gnu_alias = TRUE;
        }  /* if */
        add_alias_fixup(symbol_for(rp), (char*)NULL, arg->variant.string.value,
                        &ap->position);
      }  /* if */
    }  /* if */
  } else if (entity_kind == iek_variable) {
    a_variable_ptr  vp = (a_variable_ptr)entity;
    if (gnu_version < 40200 && 
        vp->storage_class != (a_storage_class)sc_extern &&
        vp->storage_class != (a_storage_class)sc_unspecified) {
      pos_st_error(ec_attribute_requires_external_linkage, &ap->position,
                   attribute_display_name(ap));
      make_attr_unrecognized(ap);
    } else {
      if (ap->kind == ak_alias) {
        vp->is_gnu_alias = TRUE;
      }  /* if */
      add_alias_fixup(symbol_for(vp), (char*)NULL, arg->variant.string.value,
                      &ap->position);
    }  /* if */
  } else {
    unexpected_condition();
  }  /* if */
  return entity;
}  /* apply_alias_attr */


static char* apply_alloc_size_attr(an_attribute_ptr  ap,
                                   char              *entity,
                                   an_il_entry_kind  entity_kind)
/*
Apply the GNU "alloc_size" attribute to the given entity if applicable, and
return that entity.  
The alloc_size attribute applies to functions that return a pointer to
allocated memory.  It takes one or two integer arguments that specify
parameters that determine the size of the returned memory.  Back ends
can use this information to optimize the behavior of __builtin_object_size.
For example:
  void* myalloc(unsigned n_items, unsigned item_size)
                                              __attribute((alloc_size(1, 2)));
*/
{
  a_type_ptr  func_type;

  check_assertion(ap->arguments != NULL);
  switch (entity_kind) {
    case iek_routine:
    case iek_variable:
    case iek_field:
    case iek_param_type:
    case iek_type:
      func_type = get_func_type_for_attr(ap, &entity, entity_kind);
      break;
    default:
      func_type = NULL;
  }  /* switch */
  if (func_type == NULL) {
    report_bad_attribute_target(es_warning, ap);
  } else {
    /* Check that the attribute parameters denote a valid parameter index. */
    a_host_large_integer  p1, p2 = 0, n_params = 0;
    a_param_type_ptr      ptp = func_type->variant.routine.extra_info
                                         ->param_type_list;
    if (func_type->variant.routine.extra_info->this_class != NULL) {
      /* For nonstatic member function, the implicit "*this" parameter is
         number one, and the first declared parameter is numbered two. */
      ++n_params;
    }  /* if */
    for (; ptp != NULL; ptp = ptp->next) ++n_params;
    if (get_attr_arg_integer(ap->arguments, ap,
                             (a_host_large_integer)1, n_params, &p1) &&
        (ap->arguments->next == NULL ||
         get_attr_arg_integer(ap->arguments->next, ap,
                              (a_host_large_integer)1, n_params, &p2))) {
      /* The attribute arguments are valid. */
    } else {
      make_attr_unrecognized(ap);
    }  /* if */
  }  /* if */
  return entity;
}  /* apply_alloc_size_attr */


static char* apply_always_inline_attr(ARG_UNUSED an_attribute_ptr ap,
                                      char                        *entity,
                                      an_il_entry_kind            entity_kind)
/*
The given entity must be a routine.  Apply the GNU "always_inline" attribute
to it and return the entity.
*/
{
  a_routine_ptr  rp = (a_routine_ptr)entity;

  check_assertion(entity_kind == iek_routine && gnu_mode);
  if (!rp->is_inline && gnu_version >= 40700) {
    /* Later GNU versions give a warning and don't mark the routine as
       "inline". */
    if (ap->on_primary_declaration) {
      /* Only give a warning on the primary declaration. */
      pos_warning(ec_always_inline_requires_inline, &ap->position);
    }  /* if */
    make_attr_unrecognized(ap);
  } else {
    /* Early versions of GNU simply implied "inline" in cases where it
       wasn't already specified. */
    set_inline_flag(rp, TRUE);
    rp->always_inline = TRUE;
  }  /* if */
  return entity;
}  /* apply_always_inline_attr */

#if GNU_X86_ATTRIBUTES_ALLOWED

static char* apply_cdecl_attr(an_attribute_ptr  ap,
                              char              *entity,
                              an_il_entry_kind  entity_kind)
/*
Apply the GNU "cdecl" attribute to the given entity and return that entity.
*/
{
  a_type_ptr  func_type = get_func_type_for_attr(ap, &entity, entity_kind);

  if (!target_is_32_bit_x86_based()) {
    /* This attribute is supported only in 32-bit x86 configurations. */
    pos_warning(ec_attribute_not_supported_in_x86_64, &ap->position);
    make_attr_unrecognized(ap);
  } else if (func_type != NULL) {
    a_routine_type_supplement_ptr  rtsp =
                                        func_type->variant.routine.extra_info;
    if (rtsp->calling_convention != (a_calling_convention)cc_default &&
        rtsp->calling_convention != (a_calling_convention)cc_cdecl) {
      /* gcc issues an error on incompatible calling convention attributes,
         but g++ silently keeps the non-cdecl convention. */
      an_error_severity  sev = gpp_mode ? es_warning : es_error;
      pos_diagnostic(sev, ec_conflicting_calling_conventions, &ap->position);
    } else {
      rtsp->calling_convention = (a_calling_convention)cc_cdecl;
      rtsp->explicit_calling_convention = TRUE;
    }  /* if */
  }  /* if */
  return entity;
}  /* apply_cdecl_attr */

#endif /* GNU_X86_ATTRIBUTES_ALLOWED */

static char* apply_cleanup_attr(ARG_UNUSED an_attribute_ptr ap,
                                char                        *entity,
                                ARG_UNUSED an_il_entry_kind entity_kind)
/*
The given entity must be a variable or parameter.  Apply the GNU "cleanup"
attribute to it and return the entity.
*/
{
  a_variable_ptr        vp = (a_variable_ptr)entity;
  an_attribute_arg_ptr  aap = ap->arguments;
  a_symbol_ptr          sym;
  a_symbol_locator      loc;
  /* The table-based configuration ensures that we can make a number of
     assumptions here. */
  check_assertion(aap != NULL && aap->next == NULL &&
                  aap->kind == (an_attribute_arg_kind)aak_token);
  /* First look up and validate the cleanup routine. */
  clear_locator(&loc, &aap->position);
  (void)find_symbol(aap->variant.token, (sizeof_t)strlen(aap->variant.token),
                    &loc);
  sym = normal_id_lookup(&loc, IDL_NO_OPTIONS);
  if (sym == NULL || sym->kind != (a_symbol_kind)sk_routine) {
    pos_warning(ec_invalid_cleanup_routine, &aap->position);
    make_attr_unrecognized(ap);
  } else {
    a_type_ptr  rtp = skip_typerefs(sym->variant.routine.ptr->type);
    a_routine_type_supplement_ptr
                rtsp = rtp->variant.routine.extra_info;
    if (!rtsp->prototyped) {
      /* No check possible: Assume the function is acceptable. */
    } else if (rtsp->param_type_list == NULL ||
               rtsp->param_type_list->next != NULL) {
      pos_error(ec_bad_type_for_cleanup_routine, &aap->position);
      make_attr_unrecognized(ap);
    } else {
      /* Check that the cleanup routine can be called with an argument that
         is the address of the given variable. */
      a_std_conv_descr  std_conv;
      clear_std_conv_descr(&std_conv);
      if (impl_conversion_possible(make_pointer_type(vp->type),
                                   /*source_is_constant=*/FALSE,
                                   /*source_is_string_literal=*/FALSE,
                                   /*source_is_function=*/FALSE,
                                   /*is_copy_initialization=*/FALSE,
                                   (a_constant*)NULL,
                                   rtsp->param_type_list->type,
                                   /*singleton_braced_init=*/FALSE,
                                   /*allow_qualifier_or_eh_mismatch=*/FALSE,
                                   /*suppress_extensions=*/TRUE,
                                   ec_nonstandard_conversion_for_cleanup,
                                   &std_conv)) {
        if (std_conv.warning_suggested != ec_no_error) {
          pos_warning(std_conv.warning_suggested, &ap->position);
        }  /* if */
      } else {
        pos_error(ec_bad_type_for_cleanup_routine, &ap->position);
        make_attr_unrecognized(ap);
      }  /* if */
    }  /* if */
  }  /* if */
  /* Next validate the variable. */
  if (vp->storage_class != sc_auto) {
    pos_warning(ec_attribute_cleanup_requires_automatic_storage,
                &ap->position);
    make_attr_unrecognized(ap);
  } else if (vp->is_parameter) {
    pos_warning(ec_attribute_cleanup_for_parameter, &ap->position);
    make_attr_unrecognized(ap);
  } else if (vp->source_corresp.enclosing_routine != NULL &&
             vp->source_corresp.enclosing_routine->is_constexpr) {
    /* Give a discretionary error on use within a constexpr routine as this
       is not currently supported. */
    pos_diagnostic(es_discretionary_error,
                   ec_cleanup_attribute_in_constexpr_function, &ap->position);
    if (is_effective_error(ec_cleanup_attribute_in_constexpr_function,
                           es_discretionary_error, &ap->position)) {
      make_attr_unrecognized(ap);
    }  /* if */
  }  /* if */
  /* If all went well, record the cleanup routine. */
  if (!is_unrecognized_attr(ap)) {
    mark_referenced(sym, &ap->position);
    vp->cleanup_routine = sym->variant.routine.ptr;
    mark_routine_referenced(vp->cleanup_routine);
    vp->cleanup_routine->called = TRUE;
    vp->used = TRUE;
    symbol_for(vp)->referenced = TRUE;
  }  /* if */
  return entity;
}  /* apply_cleanup_attr */


static char* apply_common_attr(ARG_UNUSED an_attribute_ptr ap,
                               char                        *entity,
                               an_il_entry_kind            entity_kind)
/*
Apply the given "common" attribute to the given entity (which must be a
variable) and return the entity.
*/
{
  a_variable_ptr vp = (a_variable_ptr)entity;

  check_assertion(entity_kind == iek_variable);
  if (vp->has_internal_linkage_attribute) {
    /* Cannot have common and internal_linkage attributes. */
    pos_error(ec_cannot_be_common_internal_linkage, &ap->position);
  } else {
    vp->is_common = TRUE;
  }  /* if */
  return entity;
}  /* apply_common_attr */


static char* apply_const_attr(an_attribute_ptr  ap,
                              char              *entity,
                              an_il_entry_kind  entity_kind)
/*
Apply the GNU "const" attribute to the given entity and return that entity.
*/
{
  a_type_ptr  func_type = get_func_type_for_attr(ap, &entity, entity_kind);

  if (func_type != NULL) {
    if (entity_kind == iek_type) {
      report_bad_attribute_target(es_warning, ap);
    } else {
      func_type->variant.routine.extra_info->is_const = TRUE;
    }  /* if */
  }  /* if */
  return entity;
}  /* apply_const_attr */

#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED

static a_boolean get_priority(an_attribute_ptr    ap,
                              a_gnu_init_priority min_priority,
                              a_gnu_init_priority *priority)
/*
If the given attribute has arguments, sets *priority to the specified priority
and returns TRUE.  The minimum valid priority value for the attribute is
specified by min_priority.  If the argument is template-dependent or in error
cases return zero (in error cases, also set ap->kind to ak_unrecognized).
*/
{
  an_attribute_arg_ptr  aap = ap->arguments;
  a_boolean             result = FALSE;
  a_host_large_integer  attr_priority;

  if (aap != NULL &&
      get_attr_arg_integer(aap, ap, (a_host_large_integer)min_priority,
                           (a_host_large_integer)65535, &attr_priority)) {
    if (attr_priority < 101) {
      /* Priorities less than 101 are reserved for internal use. */
      pos_warning(ap->kind == ak_init_priority ?
                        ec_init_priority_reserved :
                        ec_ctor_dtor_priority_reserved,
                  &ap->position);
    }  /* if */
    *priority = (a_gnu_init_priority)attr_priority;
    result = TRUE;
  }  /* if */
  return result;
}  /* get_priority */

#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */

static char* apply_constructor_attr(an_attribute_ptr  ap,
                                    char              *entity,
                                    an_il_entry_kind  entity_kind)
/*
The given entity must be a routine.  Apply the GNU "constructor" attribute to
it and return the entity.
*/
{
  a_routine_ptr         rp = (a_routine_ptr)entity;

  check_assertion(entity_kind == iek_routine &&
                  (ap->arguments == NULL || ap->arguments->next == NULL));
  if (!is_error_type(rp->type) &&
      routine_type_is_nonstatic_member_function(rp->type)) {
    pos_st_warning(ec_attribute_ignored_on_nonstatic_member_function,
                   &ap->position, attribute_display_name(ap));
  } else {
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
    /* Retrieve the priority value.  In error cases, ap->kind will be set to
       ak_unrecognized. */
    if (get_priority(ap, 0, &ensure_gnu_routine_supp(rp)->ctor_priority)) {
      rp->has_ctor_priority = TRUE;
    }  /* if */
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
    if (!is_unrecognized_attr(ap)) {
      rp->is_initialization_routine = TRUE;
    }  /* if */
  }  /* if */
  mark_referenced(symbol_for(rp), &ap->position);
  return entity;
}  /* apply_constructor_attr */


static char* apply_destructor_attr(an_attribute_ptr  ap,
                                   char              *entity,
                                   an_il_entry_kind  entity_kind)
/*
The given entity must be a routine.  Apply the GNU "destructor" attribute to
it and return the entity.
*/
{
  a_routine_ptr         rp = (a_routine_ptr)entity;

  check_assertion(entity_kind == iek_routine &&
                  (ap->arguments == NULL || ap->arguments->next == NULL));
  if (!is_error_type(rp->type) &&
      routine_type_is_nonstatic_member_function(rp->type)) {
    pos_st_warning(ec_attribute_ignored_on_nonstatic_member_function,
                   &ap->position, attribute_display_name(ap));
  } else {
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
    /* Retrieve the priority value.  In error cases, ap->kind will be set to
       ak_unrecognized. */
    if (get_priority(ap, 0, &ensure_gnu_routine_supp(rp)->dtor_priority)) {
      rp->has_dtor_priority = TRUE;
    }  /* if */
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
    if (!is_unrecognized_attr(ap)) {
      rp->is_finalization_routine = TRUE;
    }  /* if */
  }  /* if */
  mark_referenced(symbol_for(rp), &ap->position);
  return entity;
}  /* apply_destructor_attr */

#if GNU_X86_ATTRIBUTES_ALLOWED

static char* apply_fastcall_attr(an_attribute_ptr  ap,
                                 char              *entity,
                                 an_il_entry_kind  entity_kind)
/*
Apply the GNU "fastcall" attribute to the given entity and return that entity.
*/
{
  a_type_ptr  func_type = get_func_type_for_attr(ap, &entity, entity_kind);

  if (!target_is_32_bit_x86_based()) {
    /* This attribute is supported only in 32-bit x86 configurations. */
    pos_warning(ec_attribute_not_supported_in_x86_64, &ap->position);
    make_attr_unrecognized(ap);
  } else if (func_type != NULL) {
    a_routine_type_supplement_ptr  rtsp =
                                        func_type->variant.routine.extra_info;
    if (rtsp->calling_convention != (a_calling_convention)cc_default &&
        rtsp->calling_convention != (a_calling_convention)cc_fastcall) {
      /* gcc issues an error, but g++ accepts the conflict. If the previous
         convention was cdecl, fastcall is recorded.  If the previous
         convention was stdcall, g++ appears to record a calling convention
         distinct from any other; we don't emulate that behavior and instead
         just record the latest attribute. */
      an_error_severity  sev = gpp_mode ? es_warning : es_error;
      pos_diagnostic(sev, ec_conflicting_calling_conventions, &ap->position);
    }  /* if */
    rtsp->calling_convention = (a_calling_convention)cc_fastcall;
    rtsp->explicit_calling_convention = TRUE;
  }  /* if */
  return entity;
}  /* apply_fastcall_attr */

#endif /* GNU_X86_ATTRIBUTES_ALLOWED */

static constexpr struct {
  /* Data structure for table entries mapping the argument to a GNU "format"
     attribute to the equivalent EDG pragma.  This is used by the function
     apply_format_attr below. */
  a_const_char	*name;
			/* Format name. */
  a_pragma_kind
		arg_pragma;
			/* Matching EDG pragma. */
} format_name_map[] = {
  { "printf", (a_pragma_kind)pk_printf_args },
  { "scanf", (a_pragma_kind)pk_scanf_args },
  { "strftime", (a_pragma_kind)pk_none },
	/* The EDG front end does not support strftime format checking, so
	   this form of the attribute is silently ignored. */
};

#define FORMAT_NAME_MAP_LENGTH                                             \
     ((int)(sizeof(format_name_map)/sizeof(format_name_map[0])))

static char* apply_format_attr(an_attribute_ptr  ap,
                               char              *entity,
                               an_il_entry_kind  entity_kind)
/*
Apply the GNU "format" attribute to the given entity and return that entity.
This attribute specifies that a function operates with a format string similar
to that of the standard functions printf, scanf, or strftime.  An example of
the "format" attribute is
	int myprintf(void*, char const*, ...)
                                         __attribute((format(printf, 2, 3)));
The attribute argument "2" indicates which parameter is the format string, and
the attribute argument "3" indicates which function argument is the first one
described by the format string.
*/
{
  a_type_ptr     func_type = get_func_type_for_attr(ap, &entity, entity_kind);
  an_attribute_arg_ptr
                 aap = ap->arguments;
  a_boolean      known_values = TRUE;
  a_const_char   *format_name;
  int            k, val[2];
#define FMT_ARG 0
#define FIRST_SUBST_ARG 1
  a_pragma_kind  arg_pragma = pk_none;
  
  check_assertion(aap->kind == aak_token);
  format_name = aap->variant.token;
  for (k = 0; k < FORMAT_NAME_MAP_LENGTH; ++k) {
    if (same_string_ignoring_underscores(format_name_map[k].name,
                                         format_name)) {
      arg_pragma = format_name_map[k].arg_pragma;
      break;
    }  /* if */
  }  /* for */
  if (k == FORMAT_NAME_MAP_LENGTH) {
    /* An unrecognized format function type is not a fatal error. */
    pos_st_warning(ec_unrecognized_format_function_type, &aap->position,
                   format_name);
    make_attr_unrecognized(ap);
    known_values = FALSE;
  }  /* if */
  /* Convert the next two attribute arguments to integer values and check some
     basic range constraints.  GNU allows template parameters here but Clang
     does not (we allow them in both modes). */
  for (k = 0; k<2; ++k) {
    a_host_large_integer  v;
    aap = aap->next;
    if (get_attr_arg_integer(aap, ap, (a_host_large_integer)0,
                             (a_host_large_integer)INT_MAX, &v)) {
      val[k] = (int)v;
    } else {
      known_values = FALSE;
    }  /* if */
  }  /* for */
  if (func_type != NULL && known_values) {
    a_routine_type_supplement_ptr
                      rtsp = func_type->variant.routine.extra_info;
    a_param_type_ptr  ptp;
    if (!rtsp->prototyped) {
      /* For an unprototyped function, no checks are required.  However, we
         currently do not record the substitution argument for later checking.
         So we silently ignore the attribute in that case (which is achieved
         by setting the substituted argument field to zero). */
      val[FIRST_SUBST_ARG] = 0;
    } else {
      int       count = 0;
      a_boolean has_pack_expansion = FALSE;
      /* Check to see that the format argument has string type and that the
         substitution argument is the first variable argument. */
      if (rtsp->this_class != NULL) {
        /* For nonstatic member function, the implicit "*this" parameter is
           number one, and the first declared parameter is numbered two. */
        ++count;
      }  /* if */
      for (ptp = rtsp->param_type_list; ptp != NULL; ptp = ptp->next) {
        ++count;
        if (count == val[FMT_ARG]) {
          assert_not_handle_or_tracking_reference(ptp->type);
          if (!(is_pointer_type(ptp->type) &&
                is_character_type(type_pointed_to(ptp->type)))) {
            pos_error(ec_fmt_arg_is_not_string,
                      &ap->arguments->next->position);
            make_attr_unrecognized(ap);
          }  /* if */
        }  /* if */
        if (count == val[FIRST_SUBST_ARG]) {
          if (ptp->pack_expansion_descr != NULL || ptp->is_pack_element) {
            /* A pack expansion or expanded pack is accepted by Clang.
               We allow it in GNU mode as well. */
            has_pack_expansion = TRUE;
          }  /* if */
        }  /* if */
      }  /* for */
      if (!has_pack_expansion && !rtsp->has_ellipsis &&
          val[FIRST_SUBST_ARG] == count + 1 &&
          entity_kind == iek_routine) {
        /* No ellipsis or pack expansion found so far, but look for one
           additional case. */
        a_routine_ptr rp = (a_routine_ptr)entity;
        if (rp->is_template_function && !rp->is_prototype_instantiation) {
          /* It's possible that this template instantiation has a pack
             expansion that expanded to zero arguments.  Look through the
             parameters of the prototype to see if there was originally
             a pack expansion. */
          check_assertion(rp->assoc_template != NULL);
          a_type_ptr proto_type =
                     rp->assoc_template->prototype_instantiation.routine->type;
          check_assertion(proto_type->kind == tk_routine);
          a_routine_type_supplement_ptr  proto_rtsp =
                                        proto_type->variant.routine.extra_info;
          for (ptp = proto_rtsp->param_type_list; ptp!=NULL; ptp = ptp->next) {
            if (ptp->pack_expansion_descr != NULL) {
              has_pack_expansion = TRUE;
              break;
            }  /* if */
          }  /* for */
        }  /* if */
      }  /* if */
      /* If the format argument index is out of range, issue an error. */
      if (count < val[FMT_ARG]) {
        pos_error(ec_fmt_arg_does_not_exist, &ap->arguments->next->position);
        make_attr_unrecognized(ap);
      }  /* if */
      if (val[FIRST_SUBST_ARG] != 0 &&
          !(rtsp->has_ellipsis || has_pack_expansion)) {
        /* A function type without an ellipsis or a parameter pack cannot have
           the "format" attribute (unless the substitution argument was
           specified as zero).  Clang gives a warning. */
        an_error_severity  sev = clang_mode ? es_warning : es_error;
        pos_diagnostic(sev, ec_format_rout_not_varargs, &ap->position);
        if (sev == es_error) {
          make_attr_unrecognized(ap);
        }  /* if */
      }  /* if */
      if (val[FIRST_SUBST_ARG] > 0 &&
          !(has_pack_expansion || val[FIRST_SUBST_ARG] == count + 1)) {
        if (clang_mode && !(val[FIRST_SUBST_ARG] <= val[FMT_ARG])) {
          /* Clang is silent in most cases, but does complain if the second
             argument is less than or equal to the first. */
        } else {
          pos_error(ec_subst_arg_is_not_variable,
                    &ap->arguments->next->next->position);
          make_attr_unrecognized(ap);
        }  /* if */
      }  /* if */
    }  /* if */
    /* If the "first argument to check" is specified as zero, GNU only checks
       the format string for consistency without matching it up to argument
       types.  Since the EDG front end is not set up for just checking format
       string consistency, we silently ignore the attribute in that case. */
    if (val[FIRST_SUBST_ARG] > 0 && !is_unrecognized_attr(ap)) {
      rtsp->arg_pragma = arg_pragma;
      rtsp->fmt_arg = val[FMT_ARG];
      rtsp->format_first_subst_arg = val[FIRST_SUBST_ARG];
    }  /* if */
  }  /* if */
#undef FMT_ARG
#undef FIRST_SUBST_ARG
  return entity;
}  /* apply_format_attr */


static char* apply_format_arg_attr(an_attribute_ptr  ap,
                                   char              *entity,
                                   an_il_entry_kind  entity_kind)
/*
The given entity must be a function.  Apply the GNU "format_arg" attribute to
it and return the entity.
*/
{
  an_attribute_arg_ptr
                 aap = ap->arguments;

  check_assertion(entity_kind == iek_routine);
  if (aap->variant.constant->kind != (a_constant_repr_kind)ck_template_param) {
    a_boolean             ovflo = FALSE;
    a_host_large_integer  arg_num =
                     value_of_integer_constant(aap->variant.constant, &ovflo);
    if (ovflo || arg_num < 0 || arg_num > INT_MAX) { /*lint !e685*/
      report_bad_attribute_arg(aap, ap);
    } else {
      a_type_ptr  func_type = get_func_type_for_attr(ap, &entity, entity_kind);
      check_assertion(func_type != NULL);
      a_routine_type_supplement_ptr
                  rtsp = func_type->variant.routine.extra_info;
      if (!rtsp->prototyped) {
        /* For an unprototyped function, no checks are required. */
      } else {
        /* Check to see that the format argument has string type and that the
           substitution argument is variable. */
        int               count = 0;
        a_param_type_ptr  ptp;
        if (rtsp->this_class != NULL) {
          /* For nonstatic member function, the implicit "*this" parameter is
             number one, and the first declared parameter is numbered two. */
          ++count;
        }  /* if */
        for (ptp = rtsp->param_type_list; ptp != NULL; ptp = ptp->next) {
          ++count;
          assert_not_handle_or_tracking_reference(ptp->type);
          if (count == arg_num &&
              !(is_pointer_type(ptp->type) &&
                is_character_type(type_pointed_to(ptp->type)))) {
            pos_error(ec_fmt_arg_is_not_string, &ap->position);
            make_attr_unrecognized(ap);
          }  /* if */
        }  /* for */
        /* If the format argument index is out of range, issue an error
           message. */
        if (count < arg_num) {
          pos_error(ec_fmt_arg_does_not_exist, &ap->position);
          make_attr_unrecognized(ap);
        }  /* if */
      }  /* if */
      if (!is_unrecognized_attr(ap)) {
        rtsp->fmt_arg = (int)arg_num;
      }  /* if */
    }  /* if */
  }  /* if */
  return entity;
}  /* apply_format_arg_attr */


static char* apply_gnu_inline_attr(an_attribute_ptr  ap,
                                   char              *entity,
                                   an_il_entry_kind  entity_kind)
/*
The given entity must be a function.  Apply the GNU "gnu_inline" attribute to
it and return the entity.
*/
{
  a_routine_ptr  rp = (a_routine_ptr)entity;

  check_assertion(entity_kind == iek_routine);
  if (!rp->is_inline) {
    pos_warning(ec_gnu_inline_requires_inline, &ap->position);
    make_attr_unrecognized(ap);
  } else {
    rp->gnu_c89_inline = TRUE;
    if (gpp_mode) {
      /* In GNU C++ mode, this attribute also indicates that an inline function
         definition should not be emitted as standalone code. */
      rp->definition_for_inlining_only = TRUE;
      rp->suppress_inline_body = TRUE;
    }  /* if */
  }  /* if */
  return entity;
}  /* apply_gnu_inline_attr */


static char* apply_ifunc_attr(an_attribute_ptr  ap,
                              char              *entity,
                              an_il_entry_kind  entity_kind)
/*
The "ifunc" attribute is being applied to a routine of some kind.  Apply the
attribute to it and return the entity.
*/
{
  a_routine_ptr   rp = (a_routine_ptr)entity;
  a_constant_ptr  arg;

  check_assertion(entity_kind == iek_routine &&
                  ap->arguments != NULL && ap->arguments->next == NULL &&
                  ap->arguments->kind == (an_attribute_arg_kind)aak_constant);
  arg = ap->arguments->variant.constant;
  check_assertion(arg->kind == (a_constant_repr_kind)ck_string);
  if (rp->is_gnu_alias) {
    /* Can't be both an alias and an ifunc. */
    pos_error(ec_ifunc_cant_be_alias, &ap->position);
  } else if (rp->is_weak) {
    /* ifunc can't be weak. */
    pos_error(ec_ifunc_cant_be_weak, &ap->position);
  } else {
    rp->is_ifunc = TRUE;
    add_alias_fixup(symbol_for(rp), (char*)NULL, arg->variant.string.value,
                    &ap->position);
  }  /* if */
  return entity;
}  /* apply_ifunc_attr */

#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED

static char* apply_init_priority_attr(an_attribute_ptr  ap,
                                      char              *entity,
                                      an_il_entry_kind  entity_kind)
/*
Apply the GNU "init_priority" attribute to the given entity (which must be a
variable) and return entity.
*/
{
  a_decl_parse_state    *dps = (a_decl_parse_state*)ap->assoc_info;
  a_variable_ptr        vp = (a_variable_ptr)entity;
  a_type_ptr            tp;

  check_assertion(entity_kind == iek_variable &&
                  (ap->arguments == NULL || ap->arguments->next == NULL));
  tp = skip_typerefs(vp->type);
  /* Only accept the init_priority attributes on class type variables and on
     arrays of class type objects, and only on entities that are initialized
     at program start-up time. */
  if (is_array_type(tp)) tp = underlying_array_element_type(tp);
  if (is_class_struct_union_type(tp) && dps->is_definition &&
      (is_file_or_namespace_scope(&scope_stack_top()) ||
       vp->source_corresp.is_class_member)) {
    (void)get_priority(ap, 1, &vp->init_priority);
  } else {
    pos_error(ec_bad_variable_for_init_priority, &ap->position);
  }  /* if */
  return entity;
}  /* apply_init_priority_attr */

#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */

static char* apply_internal_linkage_attr(an_attribute_ptr  ap,
                                         char              *entity,
                                         an_il_entry_kind  entity_kind)
/*
Apply the clang "internal_linkage" attribute to the given entity (which must be
a class type, routine, or variable) and return entity.
*/
{
  a_decl_parse_state    *dps = (a_decl_parse_state*)ap->assoc_info;
  a_type_ptr            tp;

  check_assertion((ap->arguments == NULL || ap->arguments->next == NULL));
  if (entity_kind == iek_type) {
    tp = (a_type_ptr)entity;
    check_assertion(is_class_struct_union_type(tp));
    tp->variant.class_struct_union.has_internal_linkage_attribute = TRUE;
  } else if (entity_kind == iek_routine) {
    a_routine_ptr rp = (a_routine_ptr)entity;
    if (dps != NULL && !dps->first_decl &&
        (dps->dso_flags & DSO_FRIEND) == 0 &&
        !rp->has_internal_linkage_attribute) {
      /* The attribute must have appeared on the initial declaration. */
      pos_error(ec_internal_linkage_not_on_prior_declaration, &ap->position);
    } else {
      rp->has_internal_linkage_attribute = TRUE;
      rp->storage_class = (a_storage_class)sc_static;
      rp->source_corresp.name_linkage = (a_name_linkage_kind)nlk_internal;
    }  /* if */
  } else if (entity_kind == iek_variable) {
    a_variable_ptr vp = (a_variable_ptr)entity;
    if (vp->is_common) {
      /* Cannot have common and internal_linkage attributes. */
      pos_error(ec_cannot_be_common_internal_linkage, &ap->position);
    } else if (dps != NULL && !dps->first_decl &&
               !vp->has_internal_linkage_attribute) {
      /* The attribute must have appeared on the initial declaration. */
      pos_error(ec_internal_linkage_not_on_prior_declaration, &ap->position);
    } else {
      vp->has_internal_linkage_attribute = TRUE;
      vp->storage_class = (a_storage_class)sc_static;
      vp->source_corresp.name_linkage = (a_name_linkage_kind)nlk_internal;
    }  /* if */
  } else {
    unexpected_condition();
  }  /* if */
  return entity;
}  /* apply_internal_linkage_attr */


static char* apply_malloc_attr(ARG_UNUSED an_attribute_ptr ap,
                               char                        *entity,
                               an_il_entry_kind            entity_kind)
/*
The given entity must be a function.  Apply the GNU "malloc" attribute to it
and return the entity.
*/
{
  check_assertion(entity_kind == iek_routine);
  ((a_routine_ptr)entity)->allocates_memory = TRUE;
  an_attribute_arg_ptr  aap = ap->arguments;
  if (aap != NULL) {
    /* If one (or more) argument(s) are specified, make sure the first one
       is a routine (or a cast of a routine). */
    check_assertion(aap->kind == aak_expression);
    an_expr_node_ptr expr = expr_node_from_attribute_arg(aap);
    if (!is_routine_node(expr) &&
        !(is_operation_node(expr) &&
          node_operator_is(expr, eok_cast) &&
          is_routine_node(expr->variant.operation.operands))) {
      pos_error(ec_bad_malloc_attribute, &aap->position);
    }  /* if */
  }  /* if */
  return entity;
}  /* apply_malloc_attr */


static char* apply_may_alias_attr(an_attribute_ptr  ap,
                                  char              *entity,
                                  an_il_entry_kind  entity_kind)
/*
The given entity must be a type.  Return the type produced by applying the
given "may_alias" attribute to it.
*/
{
  a_type_ptr  type = (a_type*)entity;

  check_assertion(entity_kind == iek_type);
  if (is_tag_attribute(ap)) {
    /* The attribute appears in a class or enum definition.  E.g.:
         struct __attribute((may_alias)) X { ... };
       Set the may_alias flag directly. */
    type->may_alias = TRUE;
  } else if (!C_mode() &&
             (is_class_struct_union_type(type) || is_enum_type(type))) {
    /* GNU C++ compilers ignore the attribute applied on a use (as opposed to a
       declaration) of a class/enum type with a warning.  However, since this
       this makes a subtle difference with potential grave consequences, we
       issue an error. */
    report_bad_attribute_target(es_error, ap);
  } else if (type->kind == (a_type_kind)tk_routine) {
    /* A routine type (not under a typedef) is never shared and can therefore
       be modified directly.  (Furthermore, some code assumes that tk_typerefs
       on top of routine types must be typedef/decltype/typeof entries; so,
       no other typeref entry should be placed on top of the type.) */
    type->may_alias = TRUE;
  } else {
    /* Create a typeref entry to apply the may_alias flag to.  The
       attribute entries themselves will be attached elsewhere. */
    type = make_typeref_with_attributes(type, (an_attribute_ptr)NULL);
    type->may_alias = TRUE;
  }  /* if */
  return (char*)type;
}  /* apply_may_alias_attr */


a_type_ptr get_type_with_mode(a_type_ptr        type,
                              a_type_mode_kind  mode,
                              a_source_position *pos)
/*
Return a type, similar to the type provided, but with the indicated machine
mode.  The source position at which any errors should be emitted is given by
pos; if pos is NULL, no error is emitted.
*/
{
  an_integer_kind  ikind;
  a_float_kind     fkind = (a_float_kind)fk_float;
  a_type_kind      type_kind = (a_type_kind)tk_unknown;
  a_targ_size_t    size = 0;

  switch (mode) {
    case tmk_QI:
      type_kind = (a_type_kind)tk_integer;
      size = 1;
      break;
    case tmk_HI:
      type_kind = (a_type_kind)tk_integer;
      size = 2;
      break;
    case tmk_SI:
      type_kind = (a_type_kind)tk_integer;
      size = 4;
      break;
    case tmk_DI:
      type_kind = (a_type_kind)tk_integer;
      size = 8;
      break;
    case tmk_TI:
      type_kind = (a_type_kind)tk_integer;
      size = 16;
      break;
    case tmk_SF:
      type_kind = (a_type_kind)tk_float;
      size = 4;
      break;
    case tmk_DF:
      type_kind = (a_type_kind)tk_float;
      size = 8;
      break;
    case tmk_XF:
      type_kind = (a_type_kind)tk_float;
      /* Force to __float80 type. */
      fkind = float_kind_for_float80;
      break;
    case tmk_TF:
      type_kind = (a_type_kind)tk_float;
      /* Force to __float128 type. */
      fkind = float_kind_for_float128;
      break;
#if C99_IL_EXTENSIONS_SUPPORTED
    case tmk_SC:
      type_kind = (a_type_kind)tk_complex;
      size = 4;
      break;
    case tmk_DC:
      type_kind = (a_type_kind)tk_complex;
      size = 8;
      break;
    case tmk_XC:
      type_kind = (a_type_kind)tk_complex;
      /* Force to __float80 base type. */
      fkind = float_kind_for_float80;
      break;
    case tmk_TC:
      type_kind = (a_type_kind)tk_complex;
      /* Force to __float128 base type. */
      fkind = float_kind_for_float128;
      break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case tmk_error:
      type_kind = (a_type_kind)tk_error;
      break;
    default:
      unexpected_condition();
  }  /* switch */
  /* If the mode was erroneous, we can just return the original type.
     Otherwise, find a type with the appropriate mode. */
  if (type_kind != (a_type_kind)tk_error) {
    a_type_qualifier_set qualifiers;
    /* Remember the type qualifiers so that we can create an identically
       qualified copy. */
    qualifiers = get_type_qualifiers(type);
    /* Check to see that the type implied by the mode matches the type
       of the variable. */
    type = skip_typerefs(type);
    if (type->kind != type_kind) {
      if (pos != NULL) {
        pos_ty_error(ec_mode_incompatible_with_type, pos, type);
      }  /* if */
      type = error_type();
    } else if (type->kind == (a_type_kind)tk_integer) {
      ikind = int_kind_for_bit_size((unsigned int)(size * targ_char_bit),
                                    is_signed_integral_type(type));
      if (ikind == (an_integer_kind)ik_none) {
        if (pos != NULL) {
          pos_error(ec_no_type_of_specified_width, pos);
        }  /* if */
        type = error_type();
      } else {
        type = integer_type(ikind);
      }  /* if */
    } else {
      if (fkind == (a_float_kind)fk_float) {
        /* Some base floating point types are already specified above; if not,
           look for the smallest floating-point type that matches the size. */
        for (fkind = (a_float_kind)0;
             fkind < (a_float_kind)fk_first_extended_type;
             fkind = (a_float_kind)((int)fkind + 1)) {
          if (fkind == fk_float32x || fkind == fk_float64x) {
            /* Ignore _FloatNx types. */
          } else if (float_type(fkind)->size == size) {
            break; 
          }  /* if */
        }  /* for */
        if (fkind == (a_float_kind)fk_first_extended_type) {
          if (pos != NULL) {
            pos_error(ec_no_type_of_specified_width, pos);
          }  /* if */
          type = error_type();
        }  /* if */
#if !FLOAT80_ENABLING_POSSIBLE
      } else if (fkind == float_kind_for_float80
#if FLOAT128_ENABLING_POSSIBLE
                 && fkind != float_kind_for_float128
#endif /* FLOAT128_ENABLING_POSSIBLE */
                                                    ) {
        if (pos != NULL) {
          pos_error(ec_no_float80, pos);
        }  /* if */
        type = error_type();
#endif /* !FLOAT80_ENABLING_POSSIBLE */
#if !FLOAT128_ENABLING_POSSIBLE
      } else if (fkind == float_kind_for_float128) {
        if (pos != NULL) {
          pos_error(ec_no_float128, pos);
        }  /* if */
        type = error_type();
#endif /* !FLOAT128_ENABLING_POSSIBLE */
      }  /* if */
      if (!is_error_type(type)) {
        check_assertion(fkind < (a_float_kind)fk_last);
        if (type->kind == (a_type_kind)tk_float) {
          type = float_type(fkind);
#if C99_IL_EXTENSIONS_SUPPORTED
        } else if (type->kind == (a_type_kind)tk_complex) {
          type = complex_type(fkind);
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
        } else {
          unexpected_condition();
        }  /* if */
      }  /* if */
    }  /* if */
    /* The new type should have the same qualifiers as the original. */
    type = make_qualified_type(type, qualifiers);
  }  /* if */
  return type;
}  /* get_type_with_mode */


static char* apply_mode_attr(an_attribute_ptr  ap,
                             char              *entity,
                             an_il_entry_kind  entity_kind)
/*
The given entity must be a type (entity_kind is iek_type).  Apply the GNU
"mode" attribute to it and return the resulting type.  If the attribute
doesn't apply to the given type, issue an error and return an error type.
*/
{
  a_type_ptr            type = (a_type_ptr)entity;
  an_attribute_arg_ptr  aap = ap->arguments;
  a_const_char          *name, *ename;
  int                   i;
#if GNU_VECTOR_TYPES_ALLOWED
  unsigned              vector_length = 0;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
  sizeof_t              name_len, ename_len;

  /* Simple table-based constraint checking ensures that we can make a number
     of assumptions here. */
  check_assertion(entity_kind == iek_type &&
                  aap != NULL && aap->next == NULL &&
                  aap->kind == (an_attribute_arg_kind)aak_token);
  /* First decode the mode argument. */
  /* Get the name of the mode. */
  name = aap->variant.token;
  name_len = strlen(name);
  /* Strip off double underscores if applicable.  The underscores must be
     present both before and after the mode name. */
  if (name_len > 4 && name[0] == '_' && name[1] == '_' &&
      name[name_len-1] == '_' && name[name_len-2] == '_') {
    name += 2;
    name_len -= 4;
  }  /* if */
  ename = name;
  ename_len = name_len;
#if GNU_VECTOR_TYPES_ALLOWED
  if (name[0] == 'V') {
    /* The mode attribute allows the following vector prefixes: V1, V2, V4,
       V8, and V16. */
    if (name[1] == '1' && name[2] == '6') {
      vector_length = 16;
      ename += 3;
      ename_len -= 3;
    } else if (name[1] == '1' || name[1] == '2' || name[1] == '4' ||
               name[1] == '8') {
      vector_length = (unsigned)name[1] - '0';
      ename += 2;
      ename_len -= 2;
    }  /* if */
  }  /* if */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
  /* Look up the mode name. */
  for (i = (int)tmk_first; i < (int)tmk_last; ++i) {
    if (strncmp(type_mode_kind_names[i], ename, size_t_arg(ename_len)) == 0 &&
        (sizeof_t)strlen(type_mode_kind_names[i]) == ename_len) {
      break;
    }  /* if */
  }  /* for */
  /* If it wasn't in the table, it might be one of the special
     "byte", "word", or "pointer" values.  (These cannot have a 'V<length>'
     prefix.) */
  if (i == (int)tmk_last) {
    if (strncmp("byte", name, 4) == 0 && name_len == 4) {
      i = (int)tmk_QI;
    } else if (strncmp("word", name, 4) == 0 && name_len == 4) {
      i = (int)targ_word_mode;
    } else if (strncmp("unwind_word", name, 11) == 0 && name_len == 11) {
      i = (int)targ_unwind_word_mode;
    } else if (strncmp("libgcc_cmp_return", name, 17) == 0 && name_len == 17) {
      i = (int)targ_libgcc_cmp_return_mode;
    } else if (strncmp("libgcc_shift_count", name, 18) == 0 &&
               name_len == 18) {
      i = (int)targ_libgcc_shift_count_mode;
    } else if (strncmp("pointer", name, 7) == 0 && name_len == 7 &&
               targ_all_pointers_same_size) {
      i = (int)targ_pointer_mode;
    }  /* if */
  }  /* if */
  if (i == (int)tmk_last) {
    /* If the mode was not valid, issue an error message and return an error
       type if the attribute is not a tag attribute (if it is a tag attribute,
       the caller does not expect the type to be changed). */
    report_bad_attribute_arg(aap, ap);
    if (!is_tag_attribute(ap)) type = error_type();
  } else if (is_template_param_type(type)) {
    /* Leave template parameter types unchanged. */
  } else {
    a_type_ptr  mode_type =
                 get_type_with_mode(type, (a_type_mode_kind)i, &ap->position);
    if (is_tag_attribute(ap)) {
      /* Something like "enum E { x } __attribute((mode(byte)))": Just change
         the underlying type of the enum to correspond to the given mode. */
      if (is_immediate_enum_type(type) && !is_error_type(mode_type)) {
        type->variant.integer.int_kind = mode_type->variant.integer.int_kind;
      }  /* if */
    } else {
#if GENERATE_SOURCE_SEQUENCE_LISTS
      a_decl_parse_state  *dps = (a_decl_parse_state*)ap->assoc_info;
      if (is_enum_type(type) &&
          (dps->dso_flags & DSO_DEFINES_SOMETHING) != 0) {
        /* Consider something like:
             typedef enum { e } E __attribute((mode(word)));
             E x = e;
           At this point, the entry for the enum type is not "autonomous"
           because it's part of the declaration of something else (i.e., the
           typedef E).  However, we're about to replace the enum type by a
           plain integer type and the C++-generating back end will thus never
           render the non-autonomous declaration because it would only do so
           while traversing E.  That would lead to an error since the constant
           e would never be declared.  So if the enumeration is defined, we now
           mark it as autonomous, which will change the C++-generating back end
           rendering to something like:
             enum { e };
             typedef long E __attribute((mode(word)));
             E x = (e);
           and that is equivalent to the input. */
        a_type_ptr  etp = skip_typerefs(type);
        if (!etp->incomplete) {
          etp->autonomous_primary_tag_decl = TRUE;
        }  /* if */
      }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      type = mode_type;
    }  /* if */
#if GNU_VECTOR_TYPES_ALLOWED
    if (vector_length != 0 && !is_error_type(mode_type)) {
      a_type_ptr            unqual_type = skip_typerefs(type);
      a_type_qualifier_set  qualifiers = get_type_qualifiers(type);
      if (unqual_type->kind != (a_type_kind)tk_integer &&
          unqual_type->kind != (a_type_kind)tk_float) {
        /* get_type_with_mode will have issued an error if type->kind wasn't
           tk_integer or tk_float. */
        make_attr_unrecognized(ap);
        expect_error();
      } else if (is_tag_attribute(ap)) {
        pos_diagnostic(gnu_version >= 40000 ? es_error : es_warning,
                       ec_vector_size_attribute_on_enum_type, &ap->position);
        make_attr_unrecognized(ap);
      } else {
        a_type_ptr  vtype = make_vector_type(unqual_type, vector_length);
        vtype->source_corresp.decl_position = ap->position;
        type = make_qualified_type(vtype, qualifiers);
      }  /* if */
    }  /* if */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
  }  /* if */
  return (char*)type;
}  /* apply_mode_attr */


static char* apply_no_instrument_function_attr(
                                       ARG_UNUSED an_attribute_ptr ap,
                                       char                        *entity,
                                       ARG_UNUSED an_il_entry_kind entity_kind)
/*
The given entity must be a function.  Apply the GNU "no_instrument_function"
attribute to it and return the entity.
*/
{
  check_assertion(entity_kind == iek_routine);
  ((a_routine_ptr)entity)->no_instrument_function = TRUE;
  return entity;
}  /* apply_no_instrument_function_attr */


static char* apply_no_check_memory_usage_attr(
                                       ARG_UNUSED an_attribute_ptr ap,
                                       char                        *entity,
                                       ARG_UNUSED an_il_entry_kind entity_kind)
/*
The given entity must be a function.  Apply the GNU "no_check_memory_usage"
attribute to it and return the entity.
*/
{
  check_assertion(entity_kind == iek_routine);
  ((a_routine_ptr)entity)->no_check_memory_usage = TRUE;
  return entity;
}  /* apply_no_check_memory_usage_attr */


static a_boolean is_template_instance(char             *entity,
                                      an_il_entry_kind entity_kind)
/*
Returns TRUE if the entity (a routine, class, or variable) is an instance of
a template.
*/
{
  a_boolean result = FALSE;

  switch (entity_kind) {
    case iek_routine:
      result = ((a_routine*)entity)->is_template_function;
      break;
    case iek_variable:
      result = ((a_variable*)entity)->is_template_variable;
      break;
    case iek_type:
      { a_type_ptr tp = ((a_type*)entity);
        if (is_immediate_class_type(tp)) {
          result = tp->variant.class_struct_union.is_template_class;
        }  /* if */
      }
      break;
    default:
      unexpected_condition();
  }  /* switch */
  return result;
}  /* is_template_instance */


static char* apply_no_specializations(an_attribute_ptr ap,
                                      char             *entity,
                                      an_il_entry_kind entity_kind)
/*
The given entity must be an instance of a class template, variable template or
a function template.  Issue an error if the entity is not applicable for the
clang::no_specializations attribute.
*/
{
  if (!is_template_instance(entity, entity_kind)) {
    pos_st_diagnostic(es_warning, ec_wrong_entity_for_attribute,
                      &ap->position, attribute_display_name(ap));
    make_attr_unrecognized(ap);
  }  /* if */
  return entity;
}  /* apply_no_specializations */


static char* apply_nocommon_attr(ARG_UNUSED an_attribute_ptr ap,
                                 char                        *entity,
                                 ARG_UNUSED an_il_entry_kind entity_kind)
/*
Apply the given "nocommon" attribute to the given entity (which must be a
variable) and return the entity.
*/
{
  check_assertion(entity_kind == iek_variable);
  ((a_variable_ptr)entity)->is_not_common = TRUE;
  return entity;
}  /* apply_nocommon_attr */


static void record_nonnull_attr(a_type_ptr         rtp,
                                int                param_num,
                                a_source_position  *diag_pos)
/*
Mark the param_num-th parameter of routine type rtp as requiring a nonnull
argument.  If param_num is zero, mark all the pointer parameters of *rtp this
way.  Issue any diagnostics at the given position (e.g., when the indicated
parameter has a nonpointer type).
*/
{
  a_routine_type_supplement_ptr  rtsp = rtp->variant.routine.extra_info;
  a_param_type_ptr               ptp;
  int                            p = 1;
  a_boolean                      no_effect = TRUE;

  if (rtsp->this_class != NULL) {
    /* For member functions, the count of explicit parameters starts at 2.
       If "parameter 1" is indicated by the attribute, we just ignore the
       attribute for checking purposes. */
    if (param_num == 1) {
      goto done;
    } else {
      ++p;
    }  /* if */
  }  /* if */
  for (ptp = rtsp->param_type_list; ptp != NULL; ptp = ptp->next, ++p) {
    a_boolean  is_ptr = is_pointer_type(ptp->type);
    assert_not_handle_or_tracking_reference(ptp->type);
    if (p == param_num || (param_num == 0 && is_ptr)) {
      /* We have found the specific indicated parameter, or this is a parameter
         of pointer type and all such parameters should be marked as
         "non-NULL". */
      if (!is_ptr) {
        pos_error(ec_nonnull_on_nonpointer, diag_pos);
      } else {
        ptp->nonnull = TRUE;
      }  /* if */
      no_effect = FALSE;
      if (param_num != 0) break;
    }  /* if */
  }  /* for */
  if (no_effect) {
    if (param_num != 0) {
      /* A specific parameter position was given, but no corresponding
         parameter exists. */
      pos_error(ec_nonnull_parameter_number_too_large, diag_pos);
    } else {
      /* All pointer parameters should be marked as non-NULL, but there were
         no such parameters. */
      pos_warning(ec_no_pointer_parameters, diag_pos);
    }  /* if */
  }  /* if */
done:;
}  /* record_nonnull_attr */


static char* apply_nonnull_attr(an_attribute_ptr  ap,
                                char              *entity,
                                an_il_entry_kind  entity_kind)
/*
Apply the given GNU "nonnull" attribute to the given entity and return that
entity.
*/
{
  a_type_ptr     func_type = get_func_type_for_attr(ap, &entity, entity_kind);
  an_attribute_arg_ptr
                 aap = ap->arguments;

  if (func_type != NULL) {
    if (aap == NULL) {
      /* "__attribute((nonnull))" indicates that all pointer arguments can be
         assumed to be nonnull. */
      record_nonnull_attr(func_type, 0, &ap->position);
    } else if (aap->kind != (an_attribute_arg_kind)aak_empty) {
      for (; aap != NULL; aap = aap->next) {
        a_host_large_integer  pnum;
        if (get_attr_arg_integer(aap, ap, (a_host_large_integer)1,
                                 (a_host_large_integer)(INT_MAX-1), &pnum)) {
          record_nonnull_attr(func_type, (int)pnum, &ap->position);
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  return entity;
}  /* apply_nonnull_attr */


static char* apply_packed_attr(ARG_UNUSED an_attribute_ptr ap,
                               char                        *entity,
                               an_il_entry_kind            entity_kind)
/*
Apply the given GNU "packed" attribute to the given entity and return that
entity.
*/
{
  check_assertion(gnu_mode || sun_mode);
  if (entity_kind == iek_field) {
    ((a_field_ptr)entity)->is_packed = TRUE;
  } else if (entity_kind == iek_type) {
    a_type_ptr  tp = (a_type_ptr)entity;
    if (is_immediate_enum_type(tp)) {
      /* A packed enumerated type can be smaller than an "int". */
      tp->variant.integer.packed = TRUE;
    } else if (is_immediate_class_type(tp)) {
      /* A packed class is one where all of the members are aligned on a
         1-byte boundary.  In addition, bit fields may straddle container
         boundaries. */
      tp->variant.class_struct_union.is_packed = TRUE;
      tp->variant.class_struct_union.max_member_alignment = 1;
    } else {
      unexpected_condition();
    }  /* if */
  } else {
    unexpected_condition();
  }  /* if */
  return entity;
}  /* apply_packed_attr */


static char* apply_sentinel_attr(an_attribute_ptr  ap,
                                 char              *entity,
                                 an_il_entry_kind  entity_kind)
/*
Apply the GNU "sentinel" attribute to the given entity and return that entity.
*/
{
  a_type_ptr     func_type = get_func_type_for_attr(ap, &entity, entity_kind);

  if (func_type != NULL) {
    a_routine_type_supplement_ptr
                          rtsp = func_type->variant.routine.extra_info;
    an_attribute_arg_ptr  aap = ap->arguments;
    if (!rtsp->has_ellipsis) {
      pos_error(ec_gnu_sentinel_attribute_requires_ellipsis, &ap->position);
    } else if (aap == NULL) {
      /* __attribute((sentinel)) is equivalent to
         __attribute((sentinel(0))).  (The IL representation is "one off"
         because 0 represents the "no sentinel" case.) */
      rtsp->sentinel_pos = 1;
    } else {
      a_host_large_integer  pnum;
      check_assertion(aap->kind == (an_attribute_arg_kind)aak_constant);
      for (; aap != NULL; aap = aap->next) {
        if (get_attr_arg_integer(aap, ap, (a_host_large_integer)0,
                                 (a_host_large_integer)(INT_MAX-1), &pnum)) {
          rtsp->sentinel_pos = (int)(pnum+1);
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  return entity;
}  /* apply_sentinel_attr */

#if GNU_X86_ATTRIBUTES_ALLOWED

static char* apply_stdcall_attr(an_attribute_ptr  ap,
                                char              *entity,
                                an_il_entry_kind  entity_kind)
/*
Apply the GNU "stdcall" attribute to the given entity and return that entity.
*/
{
  a_type_ptr  func_type = get_func_type_for_attr(ap, &entity, entity_kind);

  if (!target_is_32_bit_x86_based()) {
    /* This attribute is supported only in 32-bit x86 configurations. */
    pos_warning(ec_attribute_not_supported_in_x86_64, &ap->position);
    make_attr_unrecognized(ap);
  } else if (func_type != NULL) {
    a_routine_type_supplement_ptr  rtsp =
                                        func_type->variant.routine.extra_info;
    if (rtsp->calling_convention != (a_calling_convention)cc_default &&
        rtsp->calling_convention != (a_calling_convention)cc_stdcall) {
      /* gcc issues an error, but g++ accepts the conflict. If the previous
         convention was cdecl, stdcall is recorded.  If the previous convention
         was fastcall, g++ appears to record a calling convention distinct from
         any other; we don't emulate that behavior and instead just record the
         latest attribute. */
      an_error_severity  sev = gpp_mode ? es_warning : es_error;
      pos_diagnostic(sev, ec_conflicting_calling_conventions, &ap->position);
    }  /* if */
    rtsp->calling_convention = (a_calling_convention)cc_stdcall;
    rtsp->explicit_calling_convention = TRUE;
  }  /* if */
  return entity;
}  /* apply_stdcall_attr */

#endif /* GNU_X86_ATTRIBUTES_ALLOWED */

static char* apply_strong_attr(an_attribute_ptr  ap,
                               char              *entity,
                               an_il_entry_kind  entity_kind)
/*
The given entity must have kind iek_using_decl.  Apply the GNU "strong"
attribute to it and return the entity.
*/
{
  a_using_decl_ptr  udp = (a_using_decl_ptr)entity;

  check_assertion(entity_kind == iek_using_decl);
  if (udp->is_using_directive) {
    a_scope_stack_entry_ptr  ssep = &scope_stack_top();
    /* Because the strong using-directive makes use of the namespace scope in
       which it appears, it is only valid in a namespace scope (including the
       file scope). */
    if (is_file_or_namespace_scope(ssep)) {
      /* Add the current namespace to the inline namespace list.  Strong
         using-directives make use of a variant of the inline namespace
         mechanism. */
      a_namespace_ptr	udp_nsp =  (a_namespace_ptr)udp->entity.ptr;
      udp->inline_namespace = TRUE;
      udp->strong = TRUE;
      udp_nsp->named_in_strong_using = TRUE;
      add_to_inline_namespace_list(ssep, udp);
    } else {
      /* The strong using appeared in an invalid scope. */
      pos_error(ec_bad_strong_using_scope, &ap->position);
      make_attr_unrecognized(ap);
    }  /* if */
  } else {
    /* The attribute cannot be specified on a using-declaration. */
    report_bad_attribute_target(es_error, ap);
  }  /* if */
  return entity;
}  /* apply_strong_attr */


static void next_target_argument(a_const_char         **target_arg,
                                 a_const_char         *str_end,
                                 an_attribute_arg_ptr aap,
                                 a_routine_ptr        routine,
                                 a_boolean            *error_issued)
/*
This function is used to iterate through a GNU "target" attribute string.
On input, *target_arg points to the string to parse and *target_arg is
updated to point to the next argument (if one exists).  str_end denotes
the final valid character in the string (which may be a comma or quote
character).  aap specifies the attribute argument pointer and routine
specifies the routine to which this argument is being applied.  If an
error is issued, *error_issued is set to TRUE.
*/
{
  size_t       str_len = 0;
  a_const_char *ptr = *target_arg;

  /* Search for comma delimiter or end-of-string. */
  while (ptr < str_end && *ptr != ',') {
    check_assertion(*ptr != '\0');
    ++ptr; ++str_len;
  }  /* while */
  if (str_len > 0) {
    if (clang_mode) {
      /* Clang ignores spaces. */
      while (**target_arg == ' ') {
        (*target_arg)++;
        str_len--;
      }  /* while */
    }  /* if */
    validate_target_argument(*target_arg, str_len, aap, routine, error_issued);
    if (*ptr == ',') {
      *target_arg = ++ptr;
    } else {
      *target_arg = ptr;
    }  /* if */
  }  /* if */
}  /* next_target_argument */


static void validate_target_argument_string(
                                           an_attribute_arg_ptr  aap,
                                           a_routine_ptr         routine,
                                           a_boolean             *error_issued)
/*
Validate the entire argument string (in aap) given to the "target" attribute.
The attribute is being applied to "routine".  If an error is issued,
*error_issued is set to TRUE.
*/
{
  a_const_char          *target_name;
  size_t                length;

  for (; aap != NULL &&
         aap->kind == (an_attribute_arg_kind)aak_raw_token &&
         !*error_issued;
       aap = aap->next) {
    target_name = aap->variant.token;
    length = strlen(target_name);
    if (*target_name == ',' && length == 1) {
      /* It's a comma list, keep going. */
    } else if (*target_name == '"' && length >= 2) {
      /* Validate the argument string. */
      a_const_char *target_str = &target_name[1];
      while (target_str < &target_name[length-1]) {
        /* Loop through each component of the string. */
        next_target_argument(&target_str, &target_name[length-1],
                             aap, routine, error_issued);
      }  /* while */
      if (*error_issued) {
        break;
      }  /* if */
    } else {
      pos_error(ec_exp_string_literal, &aap->position);
      *error_issued = TRUE;
      break;
    }  /* if */
  }  /* for */
}  /* validate_target_argument_string */

#if GNU_FUNCTION_MULTIVERSIONING

#if USE_X86_FUNCTION_MULTIVERSIONING && DO_IL_LOWERING
STATIC_THREAD a_boolean
                mv_builtins_loaded;
                        /* Flag that is TRUE if the GCC builtins required
                           during the lowering of multiversioning routines
                           have been loaded. */
#endif /* USE_X86_FUNCTION_MULTIVERSIONING && DO_IL_LOWERING */

a_boolean process_multiversion_function(an_attribute_ptr ap,
                                        a_scope_depth    scope_depth,
                                        a_routine_ptr    representative,
                                        a_routine_ptr    *target,
                                        a_boolean        *found_existing)
/*
Check that the GNU "target" attribute(s) specified by ap are okay (returns TRUE
if no errors are reported).  Also do the processing associated with the target
attribute, i.e., creating a new routine and a new symbol when appropriate.
This function is called directly from decl_routine and decl_member_function and
not via the normal attribute processing mechanism.

scope_depth indicates the scope in which a new routine (if needed) should be
created.  representative is the "representative" routine (or NULL if none has
been identified yet).  On input, *target, if non-NULL represents the routine
that is being defined/declared, and on exit is the target-specific routine that
should be used henceforth (and may differ from its original value).

The first time that a routine with a target attribute is encountered two
routines are created -- a routine is created that will be used as the
"representative" routine, and has is_representative set to TRUE.  The other
routine will have is_target_specific_version set to TRUE, and its
mv_target_bitset will reflect the specific CPU and/or ISA architecture(s) as
specified by the attribute.  Only the is_representative routine is available
from the symbol table; the target-specific routines are pointed to from the
representative routine.  Symbols for target-specific versioned routines do not
appear in the symbol table.  Sets *found_existing to TRUE if an existing
target-specific routine was found (and to FALSE otherwise).

Only invoked in C++ mode (normal attribute processing takes care of "target"
attributes in C mode).
*/
{
  an_attribute_arg_ptr  aap = ap->arguments;
  a_boolean             err = FALSE;
  a_routine_ptr         target_routine = NULL;
  a_routine_ptr         existing;
  a_symbol_locator      loc;
  a_symbol_ptr          new_sym, sym;

  /* GNU version 4.4.0 and later recognize the "target" attribute, but it
     only has its multiversion meaning in 4.8.0 and later. */
  check_assertion(gpp_mode && gnu_version >= 40800 &&
                  aap->kind == (an_attribute_arg_kind)aak_raw_token &&
                  aap->variant.token[0] == '"');
  *found_existing = FALSE;
  if (scope_stack_top().default_name_linkage ==
                                          (a_name_linkage_kind)nlk_external) {
    /* For function multiversioning purposes, a "target" attribute in an
       extern "C" block is ignored (it'll be recorded later).  No error is
       reported, but the routine returns FALSE (to prevent multiversioning
       code from being executed). */
    err = TRUE;
    goto done;
  }  /* if */
  if (representative == NULL) {
    /* This is the first instance of a "target" version; this routine will
       be the representative function and a new routine will be allocated for
       the target-specific version. */
    check_assertion(*target != NULL);
    representative = *target;
    ensure_gnu_routine_supp(representative)->is_representative = TRUE;
    /* Use the "ifunc" mechanism for the representative function. */
    representative->is_ifunc = TRUE;
    sym = symbol_for(representative);
  } else {
    /* The representative routine has already been created. */
    check_assertion(gnu_routine_supp(representative)->is_representative);
    sym = symbol_for(representative);
    if (sym->kind == (a_symbol_kind)sk_member_function) {
      /* In the member function case, the target-specific routine has already
         been created (no need to create a new one). */
      target_routine = *target;
    }  /* if */
  }  /* if */
  /* Allocate (if necessary) the target-specific version.  Use the storage
     class and linkage from the representative routine. */
  if (target_routine == NULL) {
    target_routine = make_routine(representative->type,
                                  representative->storage_class,
                                  scope_depth);
    target_routine->source_corresp.name_linkage =
                                   representative->source_corresp.name_linkage;
  }  /* if */
  /* Create a new symbol for this routine.  This symbol won't be entered
     in the symbol table. */
  make_locator_for_symbol(sym, &loc);
  new_sym = make_symbol(sym->kind, &loc);
  /* Copy the original symbol, then reset any pointers. */
  *new_sym = *sym;
  new_sym->next = NULL;
  new_sym->next_in_scope = NULL;
  new_sym->prev_in_scope = NULL;
  new_sym->next_in_lookup_table = NULL;
  /* New symbol points to the new routine and vice versa. */
  new_sym->variant.routine.ptr = target_routine;
  set_source_corresp(&target_routine->source_corresp, new_sym);
  /* Copy storage class and linkage. */
  target_routine->storage_class = representative->storage_class;
  target_routine->source_corresp.name_linkage =
                                   representative->source_corresp.name_linkage;
  set_routine_special_kind(target_routine, representative->special_kind);
  if (special_kind_is(target_routine, sfk_operator)) {
    target_routine->variant.opname_kind = representative->variant.opname_kind;
  }  /* if */
  /* Set parent pointer appropriately. */
  if (representative->source_corresp.is_class_member) {
    set_class_membership(new_sym, &target_routine->source_corresp,
                         scp_parent_class(&representative->source_corresp));
  } else if (scp_is_namespace_member(&representative->source_corresp)) {
    set_namespace_membership(new_sym, &target_routine->source_corresp,
                        scp_parent_namespace(&representative->source_corresp));
  }  /* if */
  /* Fill in information about the target-specific version routine. */
  ensure_gnu_routine_supp(target_routine)->is_target_specific_version = TRUE;
  if (representative->is_inline) {
    /* Transfer the setting of "is_inline". */
    set_inline_flag(target_routine, TRUE);
    if (instantiate_extern_inline && !representative->is_consteval &&
        !representative->on_inline_function_list) {
      /* When inline functions are instantiated like templates, add the
         function to the list of inline functions if it is inline (the
         target-specific versions will be on the list, but not the
         representative function -- which, in some configurations becomes a
         lowered ifunc and this prevents it from being multiply-defined). */
      add_to_inline_function_list(representative);
    }  /* if */
  }  /* if */
  /* Validate the argument string. */
  validate_target_argument_string(aap, target_routine, &err);
  /* Check to see if the new routine is compatible with those already
     declared (if any). */
  existing = find_existing_mv_routine(representative, target_routine, aap);
  if (existing != NULL) {
    /* A routine has been previously declared (or defined) with the same
       set of target attributes; give an error if there are two definitions. */
    if (existing->defined) {
      a_symbol *existing_sym = symbol_for(existing);

      issue_redef_diag(&error_position, existing_sym);
      err = TRUE;
    } else {
      /* Use the previously declared routine. */
      *found_existing = TRUE;
      ensure_gnu_routine_supp(target_routine)->is_target_specific_version =
                                                                         FALSE;
      check_assertion(gnu_routine_supp(target_routine)->
                              mv_info.targeted_version.representative == NULL);
      target_routine = existing;
    }  /* if */
  } else {
    /* Queue the new target-specific routine on the list. */
    add_to_specific_version_list(representative, target_routine);
  }  /* if */
#if USE_X86_FUNCTION_MULTIVERSIONING && DO_IL_LOWERING
  if (!mv_builtins_loaded && target_is_x86_based()) {
    /* During the lowering process, these builtins will be needed, so load
       them now.  This is done here rather than during lowering because
       lexing/parsing may be finished by the time lowering actually needs
       these builtins.  Load these into the primary translation unit (if
       there are multiple translation units). */
    a_gcc_pragma_options_entry_ptr save_gcc_pragma_options_stack;
    /* If any "GCC pragma" options are in effect, temporarily disable them. */
    save_gcc_pragma_options_stack = gcc_pragma_options_stack;
    gcc_pragma_options_stack = NULL;
    a_translation_unit_ptr saved_tup = curr_translation_unit;
    switch_translation_unit(translation_units);
    load_matching_builtin_function_by_name("__builtin_cpu_init");
    load_matching_builtin_function_by_name("__builtin_cpu_is");
    load_matching_builtin_function_by_name("__builtin_cpu_supports");
    switch_translation_unit(saved_tup);
    mv_builtins_loaded = TRUE;
    gcc_pragma_options_stack = save_gcc_pragma_options_stack;
  }  /* if */
#endif /* USE_X86_FUNCTION_MULTIVERSIONING && DO_IL_LOWERING */
  *target = target_routine;
done:
  return !err;
#undef MAX_TARGET_PAIR_LEN
}  /* process_multiversion_function */

#endif /* GNU_FUNCTION_MULTIVERSIONING */

static char *apply_target_attr(an_attribute_ptr  ap,
                               char              *entity,
                               an_il_entry_kind  entity_kind)
/*
In C++ mode, most of the work for applying the "target" attributes has already
been done in process_multiversion_function (because "target" is also used for
function multiversioning).  This routine handles the cases where
multiversioning isn't applicable.
*/
{
  an_attribute_arg_ptr  aap = ap->arguments;
  char                  *result = entity;

  /* First token must be a string literal. */
  if (aap->kind == (an_attribute_arg_kind)aak_raw_token &&
      aap->variant.token[0] != '"') {
    pos_error(ec_exp_string_literal, &aap->position);
    make_attr_unrecognized(ap);
  } else {
    check_assertion(entity_kind == iek_routine);
    if (C_mode()
#if GNU_FUNCTION_MULTIVERSIONING
        || scope_stack_top().default_name_linkage ==
                                              (a_name_linkage_kind)nlk_external
#endif /* GNU_FUNCTION_MULTIVERSIONING */
                                                                            ) {
      /* Validate the "target" argument string in C mode and in cases where
         function multiversioning doesn't check (i.e., extern "C" blocks). */
      a_boolean err = FALSE;
      validate_target_argument_string(aap, (a_routine_ptr)entity, &err);
      if (err) {
        make_attr_unrecognized(ap);
      }  /* if */
    }  /* if */
  }  /* if */
#if DEBUG
  if (db_flag_is_set("trace_attributes")) {
    (void)fprintf(f_debug, "apply_target_attr: target=%s\n",
                  aap->variant.token);
  }  /* if */
#endif /* DEBUG */
  return result;
}  /* apply_target_attr */

#if THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED

static char* apply_tls_model_attr(an_attribute_ptr  ap,
                                  char              *entity,
                                  an_il_entry_kind  entity_kind)
/*
Check the validity of the GNU "tls_model" attribute for the given entity and
return that entity.
*/
{
  a_variable_ptr      vp = (a_variable_ptr)entity;
  a_decl_parse_state  *dps = (a_decl_parse_state*)ap->assoc_info;
  a_const_char        *valid_model_names[] =
                                       { "global-dynamic", "local-dynamic",
                                         "initial-exec", "local-exec", NULL };

  check_assertion(entity_kind == iek_variable);
  check_assertion(ap->arguments != NULL && ap->arguments->next == NULL &&
                  ap->arguments->kind == (an_attribute_arg_kind)aak_constant);
  /* Check that the variable has thread-local storage. */
  if ((vp->decl_modifiers & DM_THREAD) == 0 &&
      !(dps != NULL && (dps->decl_modifiers.flags & DM_THREAD) != 0)) {
    report_bad_attribute_target(es_warning, ap);
  } else {
    /* Check that the model name (the attribute argument) is valid. */
    a_constant_ptr    arg = ap->arguments->variant.constant;
    a_const_char      **pvmn = valid_model_names;
    check_assertion(arg->kind == (a_constant_repr_kind)ck_string);
    for (; *pvmn != NULL; ++pvmn) {
      if (strcmp(arg->variant.string.value, *pvmn) == 0) break;
    }  /* for */
    if (*pvmn == NULL) {
      pos_error(ec_bad_tls_model_attr_arg, &ap->position);
      make_attr_unrecognized(ap);
    } else if (dps != NULL && !dps->first_decl) {
      an_attribute_ptr  prev_ap;
      a_constant_ptr    prev_arg;
      prev_ap = find_attribute(ak_tls_model, vp->source_corresp.attributes);
      if (prev_ap != NULL) {
        check_assertion(prev_ap->arguments != NULL &&
                        prev_ap->arguments->kind ==
                                       (an_attribute_arg_kind)aak_constant);
        prev_arg = prev_ap->arguments->variant.constant;
        check_assertion(prev_arg->kind == (a_constant_repr_kind)ck_string);
        if (strcmp(arg->variant.string.value,
                   prev_arg->variant.string.value)) {
          pos2_diagnostic(es_error, ec_inconsistent_tls_model_attr_arg,
                          &ap->arguments->position, &prev_ap->position);
          make_attr_unrecognized(ap);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return entity;
}  /* apply_tls_model_attr */

#endif /* THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED */

a_boolean check_transparent_union(a_type_ptr        tp,
                                  a_source_position *pos)
/*
"tp" is known to be an (immediate) union type.  Verify that it can be
transparent.  If not, issue a diagnostic and return FALSE.
*/
{
  a_field_ptr  first_field, f = NULL;

  check_assertion(tp->kind == (a_type_kind)tk_union);
  first_field = tp->variant.class_struct_union.field_list;
  if (first_field != NULL) {
    /* Check to see that all members of the union have the same size as the
       first field of the union.  If the first field is an integer field,
       subsequent integer fields may be smaller than the first field.
       Otherwise, GCC does not permit the union to be transparent.  It seems
       that GCC looks at the type of the field, not the actual size -- for
       example, the size of bit fields is ignored (but GCC 4.x and later
       don't allow a bit field as the first field).   The first field cannot
       have a (real or complex) floating-point type. */
    if (is_floating_type(first_field->type)) {
      /* The first field cannot have a floating-point type. */
      f = first_field;
      pos_ty_warning(ec_transparent_union_cannot_have_floating_first_field,
                     pos, tp);
    } else if (gnu_version >= 40000 && first_field->is_bit_field) {
      /* The first field cannot be a bit field. */
      f = first_field;
      pos_ty_warning(ec_transparent_union_cannot_have_bit_field_first, pos,
                     tp);
    } else {
      for (f = first_field->next; f != NULL; f = f->next) {
        a_type_ptr  ft1 = skip_typerefs(first_field->type),
                    ft2 = skip_typerefs(f->type);
        if (ft1->size == ft2->size ||
            (ft1->size > ft2->size &&
             ft1->kind == ft2->kind && ft1->kind == (a_type_kind)tk_integer)) {
          /* Acceptable field type. */
        } else {
          /* Unacceptable field type: Issue a warning. */
          a_symbol_ptr sym = symbol_for(f);
          if (sym != NULL && has_name(f)) {
            pos_syty_warning(ec_union_cannot_be_transparent_sym, pos, sym, tp);
          } else {
            pos_ty2_warning(ec_union_cannot_be_transparent, pos, tp, f->type);
          }  /* if */
          break;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  /* If an error occurred, f will point to the first field that did not meet
     the requirements. */
  return f == NULL;
}  /* check_transparent_union */


static char* apply_transparent_union_attr(an_attribute_ptr  ap,
                                          char              *entity,
                                          an_il_entry_kind  entity_kind)
/*
The given entity must a type or parameter.  Apply the GNU "transparent_union"
attribute to it and return the entity.
*/
{
  if (entity_kind == iek_param_type) {
    a_param_type_ptr  ptp = (a_param_type_ptr)entity;
    if (!is_union_type(ptp->type)) {
      pos_ty_error(ec_transparent_type_is_not_union, &ap->position, ptp->type);
      make_attr_unrecognized(ap);
    } else if (is_incomplete_type(ptp->type) ||
               !check_transparent_union(skip_typerefs(ptp->type),
                                        &ap->position)) {
      /* Parameters must already have complete types, and there is no point in
         complaining twice.  Similarly, if check_transparent_union returns
         FALSE, a diagnostic has been issued already. */
    } else {
      /* Record the attribute. */
      ptp->is_transparent = TRUE;
    }  /* if */
  } else if (entity_kind == iek_type) {
    a_type_ptr  type = (a_type_ptr)entity, tp = skip_typerefs(type);
    /* If type is a typedef, the transparent_union attribute applies to the
       underlying type. */
    if (tp->kind != (a_type_kind)tk_union) {
      pos_ty_error(ec_transparent_type_is_not_union, &ap->position, type);
      make_attr_unrecognized(ap);
    } else if (is_tag_attribute(ap)) {
      /* We cannot do any checking in the non-typedef case because the type
         has not yet been laid out.  When do_class_layout processes the type,
         it will call check_transparent_union to make sure that the attribute
         is legal. */
      tp->variant.class_struct_union.is_transparent = TRUE;
    } else if (ap->syntactic_location != al_prefix &&
               (ap->syntactic_location != al_declarator_id ||
                !type_is_typedef(type) ||
                is_incomplete_type(tp))) {
      pos_warning(ec_transparent_attribute_ignored, &ap->position);
      make_attr_unrecognized(ap);
    } else {
      /* In the typedef case, the type has already been laid out and we can do
         the check now. */
      check_assertion(type_is_typedef(type));
      if (check_transparent_union(tp, &ap->position)) {
        tp->variant.class_struct_union.is_transparent = TRUE;
      } else {
        make_attr_unrecognized(ap);
      }  /* if */
    }  /* if */
  } else {
    unexpected_condition();
  }  /* if */
  return entity;
}  /* apply_transparent_union_attr */


static char* apply_unused_attr(ARG_UNUSED an_attribute_ptr ap,
                               char                        *entity,
                               an_il_entry_kind            entity_kind)
/*
Apply the given "unused" attribute to the given entity (and return that
entity).
*/
{
  switch (entity_kind) {
    case iek_type:
      ((a_type*)entity)->variables_are_implicitly_referenced = TRUE;
      break;
    case iek_routine:
    case iek_variable:
    case iek_label:
      ((a_source_correspondence*)entity)->maybe_unused = TRUE;
      break;
    case iek_param_type:
      /* Nothing to do here.  If this is a definition, the attribute applies
         to the associated variable (see attach_param_variable_attributes).
         Otherwise, the attribute has no effect. */
      break;
    default:
      unexpected_condition();
  }  /* switch */
  return entity;
}  /* apply_unused_attr */


static char* apply_used_attr(ARG_UNUSED an_attribute_ptr ap,
                             char                        *entity,
                             an_il_entry_kind            entity_kind)
/*
Apply the given "used" attribute to the given entity (and return that entity).
*/
{
  if (entity_kind == iek_routine) {
    ((a_routine*)entity)->has_gnu_used_attribute = TRUE;
  } else if (entity_kind == iek_variable) {
    ((a_variable*)entity)->has_gnu_used_attribute = TRUE;
  } else {
    unexpected_condition();
  }  /* if */
#if MAINTAIN_NEEDED_FLAGS
  mark_as_needed(entity, entity_kind);
#endif /* MAINTAIN_NEEDED_FLAGS */
  return entity;
}  /* apply_used_attr */

#if GNU_VECTOR_TYPES_ALLOWED

static char* apply_vector_size_attr(an_attribute_ptr  ap,
                                    char              *entity,
                                    an_il_entry_kind  entity_kind)
/*
The given entity must be a type (entity_kind is iek_type).  Apply the GNU
"vector_size" attribute to it and return the resulting vector type.  If the
attribute doesn't apply to the given type, issue an error and return an
error type.
*/
{
  a_type_ptr            elem_type = (a_type_ptr)entity, vector_type, result;
  an_attribute_arg_ptr  aap = ap->arguments;
  a_constant_ptr        size_con;
  a_boolean             ovflo = FALSE, err = FALSE;
  a_host_large_integer  size = 0;
  a_decl_parse_state    *dps = NULL;

  /* Simple table-based constraint checking ensures that we can make a number
     of assumptions here. */
  check_assertion(entity_kind == iek_type &&
                  aap != NULL && aap->next == NULL &&
                  aap->kind == (an_attribute_arg_kind)aak_constant);
  if (gnu_mode && gnu_version >= 40000 &&
      (elem_type->kind == (a_type_kind)tk_routine ||
       elem_type->kind == (a_type_kind)tk_array)) {
    /* If the attribute was applied to a function or array declarator, it
       applies to the return type or element type respectively. */
    dps = (a_decl_parse_state*)ap->assoc_info;
    if (!dps->in_nested_declarator) {
      elem_type = dps->declared_type;
    } else {
      dps = NULL;
    }  /* if */
  }  /* if */
  /* Validate the element type. */
  if (is_error_type(elem_type)) {
    err = TRUE;
#if C99_IL_EXTENSIONS_SUPPORTED
  } else if (is_nonreal_floating_type(elem_type)) {
    pos_error(ec_vector_size_attribute_on_complex_type, &ap->position);
    err = TRUE;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  } else if (!is_integral_or_enum_type(elem_type) &&
             !is_floating_type(elem_type) &&
             (clang_mode || !is_pointer_type(elem_type)) &&
             !is_or_contains_template_param(elem_type)) {
    /* GNU allows a pointer type as an argument to the vector_size attribute
       (clang does not). */
    pos_error(ec_vector_size_attribute_requires_integral_floating_or_enum_type,
              &ap->position);
    err = TRUE;
  } else {
    check_assertion(!is_incomplete_type(elem_type));
  }  /* if */
  /* Validate the vector size. */
  size_con = aap->variant.constant;
  if (size_con->kind == (a_constant_repr_kind)ck_template_param) {
    if (gnu_version < 40400) {
      /* Early GCC versions ignore dependent vector sizes with a warning, but
         that seems overly surprising.  So we issue an error on such cases. */
      pos_error(ec_dependent_vector_size, &ap->position);
      err = TRUE;
    } else {
      /* Record a dummy (nonzero) size. */
      size = 1;
    }  /* if */
  } else {
    a_host_large_unsigned  elem_size = skip_typerefs(elem_type)->size;
    check_assertion(size_con->kind == (a_constant_repr_kind)ck_integer);
    size = value_of_integer_constant(size_con, &ovflo);
    if (ovflo ||
        size > (a_host_large_integer)targ_maximum_pack_alignment) {
      /* More recent versions of g++ appear to accept almost arbitrary vector
         sizes, and set the alignment to match.  However, we do not want to
         exceed the maximum representable alignment. */
      pos_error(ec_vector_size_too_large, &ap->position);
      err = TRUE;
    } else if (size <= 0) {
      pos_error(ec_vector_size_must_be_power_of_two, &ap->position);
      err = TRUE;
    } else if (elem_size == 0) {
      expect_error();
    } else if (!err && ((a_host_large_unsigned)size % elem_size) != 0) {
      pos_error(ec_vector_size_must_be_multiple_of_element_size,
                &ap->position);
      err = TRUE;
    } else {
      a_host_large_unsigned  n_elems = (a_host_large_unsigned)size / elem_size;
      if ((n_elems & (n_elems-1)) != 0) {
        pos_error(ec_vector_size_must_be_power_of_two, &ap->position);
        err = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (err) {
    /* Make sure the attribute is marked as "unrecognized" so further stages
       of processing don't treat it as a valid vector_size attribute. */
    make_attr_unrecognized(ap);
    result = error_type();
  } else {
    vector_type = alloc_type((a_type_kind)tk_vector);
    vector_type->source_corresp.decl_position = ap->position;
    vector_type->size = (a_targ_size_t)size;
    vector_type->alignment = (a_targ_alignment)size;
    vector_type->variant.vector.kind = vk_gnu;
    vector_type->variant.vector.element_type = elem_type;
    vector_type->variant.vector.size_constant = size_con;
    if (dps != NULL) {
      /* The attribute appeared on a function or array declarator.  Don't
         modify the function or array type directly, but the return type or
         element type.  At this point in the processing
         (add_to_derived_types_list has not yet been called) this is achieved
         by updating dps->declared_type. */
      dps->declared_type = vector_type;
      result = (a_type_ptr)entity;
    } else {
      result = vector_type;
    }  /* if */
  }  /* if */
  return (char*)result;
}  /* apply_vector_size_attr */


static char* apply_neon_vector_type_attr(an_attribute_ptr  ap,
                                         char              *entity,
                                         an_il_entry_kind  entity_kind)
/*
The given entity must be a type (entity_kind is iek_type).  Apply the Clang
"neon_vector_type" or "neon_polyvector_type" attribute to it and return the
resulting vector type.  If the attribute doesn't apply to the given type, issue
an error and return an error type.
*/
{
  a_type_ptr             elem_type = (a_type_ptr)entity,
                         utp = skip_typerefs(elem_type),
                         result;
  an_attribute_arg_ptr   aap = ap->arguments;
  a_constant_ptr         elems_con;
  a_boolean              ovflo = FALSE, err = FALSE;
  a_host_large_unsigned  elem_size, elems = 0;

  /* Simple table-based constraint checking ensures that we can make a number
     of assumptions here. */
  check_assertion(entity_kind == iek_type &&
                  aap != NULL && aap->next == NULL &&
                  aap->kind == aak_constant);
  /* Validate the element type. */
  if (is_error_type(elem_type)) {
    err = TRUE;
  } else if (ap->kind == ak_neon_vector_type) {
    err = !is_valid_neon_vector_element_type(utp);
    if (err) {
      pos_error(ec_invalid_neon_vector_element_type, &ap->position, elem_type);
    }  /* if */
  } else if (ap->kind == ak_neon_polyvector_type) {
    err = !is_valid_neon_polyvector_element_type(utp);
    if (err) {
      pos_error(ec_invalid_neon_polyvector_element_type, &ap->position,
                elem_type);
    }  /* if */
  } else {
    check_assertion(!is_incomplete_type(utp));
  }  /* if */
  /* Validate the vector size. */
  elems_con = aap->variant.constant;
  elem_size = utp->size;
  if (elems_con->kind == ck_template_param) {
    /* Issue an error for a dependent vector size. */
    pos_error(ec_dependent_vector_size, &ap->position);
    err = TRUE;
  } else {
    check_assertion(elems_con->kind == ck_integer);
    elems = unsigned_value_of_integer_constant(elems_con, &ovflo);
    if (ovflo || (elems * elem_size != 8 && elems * elem_size != 16)) {
      pos_error(ec_invalid_neon_vector_size, &ap->position);
      err = TRUE;
    }  /* if */
  }  /* if */
  if (err) {
    /* Make sure the attribute is marked as "unrecognized" so further stages
       of processing don't treat it as a valid vector_size attribute. */
    make_attr_unrecognized(ap);
    result = error_type();
  } else {
    result = alloc_type(tk_vector);
    result->source_corresp.decl_position = ap->position;
    result->size = elems * elem_size;
    result->alignment = (a_targ_alignment)(elems * elem_size);
    result->variant.vector.element_type = elem_type;
    result->variant.vector.size_constant = elems_con;
    result->variant.vector.kind = ap->kind == ak_neon_polyvector_type ?
                                                        vk_neon_poly : vk_neon;
  }  /* if */
  return (char*)result;
}  /* apply_neon_vector_type_attr */


a_host_large_integer validate_ext_vector_size(a_constant         *size_con,
                                              a_type_ptr         elem_type,
                                              a_source_position  *diag_pos,
                                              a_boolean          *p_err)
/*
Check that size_con is a valid operand for the ext_vector_size attribute, and
return the resulting vector size (in bytes).  elem_type is the vector element
type.  If diag_pos is non-NULL, issue any errors at the given position.  If
size_con is not valid, set *p_err to TRUE.
*/
{
  a_host_large_integer  size = 0;

  if (constant_is(size_con, ck_template_param)) {
    /* Record a dummy (nonzero) size. */
    size = 1;
  } else if (!constant_is(size_con, ck_integer)) {
    check_assertion_or_expect_error(diag_pos == NULL);
    *p_err = TRUE;
  } else {
    a_host_large_unsigned  elem_size;
    a_boolean              ovflo = FALSE;
    elem_type = skip_typerefs(elem_type);
    elem_size = elem_type->size;
    size = value_of_integer_constant(size_con, &ovflo);
    if (ovflo || size <= 0 || size >= 2048) {
      if (diag_pos != NULL) {
        pos_error(ec_ext_vector_type_invalid_size, diag_pos);
      }  /* if */
      *p_err = TRUE;
    } else if (elem_size == 0) {
      check_assertion_or_expect_error(diag_pos == NULL);
      *p_err = TRUE;
    }  /* if */
  }  /* if */
  if (!*p_err) {
    /* The attribute argument gives the number of elements in the vector;
       convert that to the overall size of the vector type.  An exception is
       made for vectors of bool type; in that case each element takes a single
       bit (and rounded up to a size that is a power of two). */
    if (is_bool_type(elem_type)) {
      if (size <= (a_host_large_integer)targ_char_bit) {
        size = 1;
      } else {
        check_assertion(targ_char_bit != 0);
        size = (size + targ_char_bit - 1) / targ_char_bit;
        size = (a_host_large_integer)next_pow2((uint64_t)size);
      }  /* if */
    } else {
      size = (a_host_large_integer)elem_type->size * size;
    }  /* if */
  }  /*if */
  return size;
}  /* validate_ext_vector_size */

                                              
static char* apply_ext_vector_type_attr(an_attribute_ptr  ap,
                                        char              *entity,
                                        an_il_entry_kind  entity_kind)
/*
The given entity must be a type (entity_kind is iek_type).  Apply the Clang
"ext_vector_type" attribute to it and return the resulting vector type.  If the
attribute doesn't apply to the given type, issue an error and return an
error type.
*/
{
  a_type_ptr            elem_type = (a_type_ptr)entity, result;
  an_attribute_arg_ptr  aap = ap->arguments;
  a_boolean             err = FALSE;
  a_boolean             bool_type_allowed = clang_version_is(>=150000);
  a_host_large_integer  size = 0;
  a_decl_parse_state    *dps = (a_decl_parse_state*)ap->assoc_info;

  /* Simple table-based constraint checking ensures that we can make a number
     of assumptions here. */
  check_assertion(entity_kind == iek_type &&
                  aap != NULL && aap->next == NULL &&
                  aap->kind == (an_attribute_arg_kind)aak_constant);
  /* Validate the element type. */
  if (is_error_type(elem_type)) {
    err = TRUE;
#if C99_IL_EXTENSIONS_SUPPORTED
  } else if (is_nonreal_floating_type(elem_type)) {
    pos_error(bool_type_allowed ?
                      ec_ext_vector_type_requires_bool_integral_floating_type :
                      ec_ext_vector_type_requires_integral_floating_type,
              &ap->position);
    err = TRUE;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  } else if (!((is_integral_type(elem_type) &&
                (bool_type_allowed || !is_bool_type(elem_type))) ||
               is_floating_type(elem_type) ||
               is_template_param_type(elem_type))) {
    pos_error(bool_type_allowed ?
                      ec_ext_vector_type_requires_bool_integral_floating_type :
                      ec_ext_vector_type_requires_integral_floating_type,
              &ap->position);
    err = TRUE;
  } else {
    check_assertion(!is_incomplete_type(elem_type));
  }  /* if */
  if (!(dps->declared_storage_class == sc_typedef || dps->is_alias)) {
    /* The ext_vector_type attribute must appear in a typedef or an alias
       declaration. */
    pos_error(ec_ext_vector_type_not_in_typedef, &ap->position);
    err = TRUE;
  }  /* if */
  /* Validate the argument. */
  size = validate_ext_vector_size(aap->variant.constant, elem_type,
                                  &ap->position, &err);
  if (err) {
    /* Make sure the attribute is marked as "unrecognized". */
    make_attr_unrecognized(ap);
    result = error_type();
  } else {
    /* Allocate the vector type. */
    a_type_ptr  vector_type = alloc_type(tk_vector);
    vector_type->source_corresp.decl_position = ap->position;
    vector_type->size = (a_targ_size_t)size;
    vector_type->alignment = (a_targ_alignment)size;
    vector_type->variant.vector.element_type = elem_type;
    vector_type->variant.vector.size_constant = aap->variant.constant;
    vector_type->variant.vector.kind = vk_ext;
    result = vector_type;
  }  /* if */
  return (char*)result;
}  /* apply_ext_vector_type_attr */

#endif /* GNU_VECTOR_TYPES_ALLOWED */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED

/*
The ELF visibility stack is represented by a list of entries of type
an_ELF_visibility_stack_entry.  Such entries may be pushed on the stack in
two ways: (1) via the "#pragma GCC visibility push" construct, or (2) via
the visibility attribute on a namespace definition.
*/
typedef struct an_ELF_visibility_stack_entry
                                           *an_ELF_visibility_stack_entry_ptr;
typedef struct an_ELF_visibility_stack_entry {
  an_ELF_visibility_stack_entry_ptr
		next;	/* Pointer to the next entry on the stack (or NULL if
			   no entry is currently on the stack). */
  an_ELF_visibility_kind
		visibility;
			/* The ELF visibility that was pushed. */
  a_bit_field	namespace_attribute:1;
			/* TRUE if this entry was pushed as the result of a
			   namespace attribute (instead of a pragma). */
} an_ELF_visibility_stack_entry;


STATIC_THREAD an_ELF_visibility_stack_entry_ptr
		ELF_visibility_stack;
			/* Pointer to the topmost entry of the ELF visibility
			   stack (or NULL if the stack is empty). */

STATIC_THREAD an_ELF_visibility_stack_entry_ptr
		avail_ELF_visibility_stack_entries;
			/* List of entries popped from the ELF visibility
			   stack, and available for reuse. */

#if DEBUG
STATIC_THREAD unsigned long
		num_ELF_visibility_stack_entries_allocated;
			/* Number of ELF visibility stack entries allocated,
			   to track total use of memory. */
#endif /* DEBUG */

void push_ELF_visibility(an_ELF_visibility_kind  evk,
                         a_boolean               namespace_attribute)
/*
Push the given ELF visibility on the ELF visibility stack.  namespace_attribute
is TRUE if this "push" operation is for a namespace attribute.
*/
{
  an_ELF_visibility_stack_entry_ptr  entry;

  if (avail_ELF_visibility_stack_entries != NULL) {
    entry = avail_ELF_visibility_stack_entries;
    avail_ELF_visibility_stack_entries =
                                     avail_ELF_visibility_stack_entries->next;
  } else {
    entry = alloc_fe_of_type(an_ELF_visibility_stack_entry);
#if DEBUG
    ++num_ELF_visibility_stack_entries_allocated;
#endif /* DEBUG */
  }  /* if */
  entry->next = ELF_visibility_stack;
  entry->visibility = evk;
  entry->namespace_attribute = namespace_attribute;
  ELF_visibility_stack = entry;
}  /* push_ELF_visibility */


void pop_ELF_visibility(a_boolean  namespace_attribute)
/*
Pop the topmost entry from the ELF visibility stack.  namespace_attribute is
TRUE if this "pop" operation is for a namespace attribute.  A warning is
issued if namespace attribute is TRUE, and the entry popped was not created
for a namespace attribute.  A warning is also issued if the ELF visibility
stack is empty.
*/
{
  if (ELF_visibility_stack != NULL) {
    an_ELF_visibility_stack_entry_ptr  entry = ELF_visibility_stack;
    if (namespace_attribute && !entry->namespace_attribute) {
      pos_warning(ec_ELF_visibility_pop_mismatch, &pos_curr_token);
    }  /* if */
    ELF_visibility_stack = entry->next;
    entry->next = avail_ELF_visibility_stack_entries;
    avail_ELF_visibility_stack_entries = entry;
  } else {
    pos_warning(ec_ELF_visibility_stack_empty, &pos_curr_token);
  }  /* if */
}  /* pop_ELF_visibility */


an_ELF_visibility_kind ELF_visibility_from_string(a_const_char *visibility_str)
/*
Return the ELF visibility kind corresponding to the given string, or
evk_unspecified if the string is not recognized.
*/
{
  an_ELF_visibility_kind  result = (an_ELF_visibility_kind)evk_unspecified;

  if (strcmp(visibility_str, "hidden") == 0) {
    result = (an_ELF_visibility_kind)evk_hidden;
  } else if (strcmp(visibility_str, "protected") == 0) {
    result = (an_ELF_visibility_kind)evk_protected;
  } else if (strcmp(visibility_str, "internal") == 0) {
    result = (an_ELF_visibility_kind)evk_internal;
  } else if (strcmp(visibility_str, "default") == 0) {
    result = (an_ELF_visibility_kind)evk_default;
  }  /* if */
  return result;
}  /* ELF_visibility_from_string */


void update_for_default_ELF_visibility(an_ELF_visibility_kind  *visibility,
                                       a_boolean               is_class_member)
/*
If the given ELF visibility is evk_unspecified, replace it by the default
visibility implied by the ELF visibility stack or the enclosing class scope.
is_class_member is TRUE if the visibility is that of a class member.
*/
{
  if (*visibility == (an_ELF_visibility_kind)evk_unspecified) {
    if (scope_is(&scope_stack_top(), sck_class_struct_union) &&
        is_class_member) {
      *visibility = scope_stack_top().ELF_visibility;
    } else if (depth_innermost_namespace_scope != NO_SCOPE_DEPTH &&
               depth_innermost_function_scope == NO_SCOPE_DEPTH) {
      /* Local declarations are not affected by the default ELF visibility
         of the surrounding namespace scope. */
      if (ELF_visibility_stack != NULL) {
        *visibility = ELF_visibility_stack->visibility;
      }  /* if */
    }  /* if */
  }  /* if */
}  /* update_for_default_ELF_visibility */


static void apply_ELF_visibility_to_current_namespace(
                                           an_ELF_visibility_kind  visibility)
/*
Record the given visibility for the current namespace or namespace-extension
definition.
*/
{
  a_scope_stack_entry_ptr  sp = &scope_stack_top();

  sp->ELF_visibility = visibility;
  push_ELF_visibility(visibility, /*namespace_attribute=*/TRUE);
}  /* apply_ELF_visibility_to_current_namespace */


static char* apply_visibility_attr(an_attribute_ptr  ap,
                                   char              *entity,
                                   an_il_entry_kind  entity_kind)
/*
Apply the GNU "visibility" attribute to the given entity and return that
entity.
*/
{
  a_constant_ptr          arg;
  an_ELF_visibility_kind  evk;

  check_assertion(ap->arguments != NULL && ap->arguments->next == NULL &&
                  ap->arguments->kind == (an_attribute_arg_kind)aak_constant);
  arg = ap->arguments->variant.constant;
  check_assertion(arg->kind == (a_constant_repr_kind)ck_string);
  evk = ELF_visibility_from_string(arg->variant.string.value);
  if (!gnu_visibility_attribute_enabled) {
    pos_st_warning(ec_unrecognized_attribute, &ap->position,
                   attribute_display_name(ap));
    make_attr_unrecognized(ap);
  } else {
    a_routine_ptr       rp;
    a_variable_ptr      vp;
    a_type_ptr          tp;
    a_decl_parse_state  *dps = (a_decl_parse_state*)ap->assoc_info;
    switch (entity_kind) {
      case iek_routine:
        rp = (a_routine_ptr)entity;
        if (rp->ELF_visibility != (an_ELF_visibility_kind)evk_unspecified &&
            rp->ELF_visibility != evk &&
            !dps->is_explicit_instantiation) {
          pos_warning(ec_gnu_visibility_conflict, &ap->position);
          make_attr_unrecognized(ap);
        } else {
          rp->ELF_visibility = evk;
        }  /* if */
        break;
      case iek_variable:
        vp = (a_variable_ptr)entity;
        if (vp->ELF_visibility != (an_ELF_visibility_kind)evk_unspecified &&
            vp->ELF_visibility != evk &&
            !dps->is_explicit_instantiation) {
          pos_warning(ec_gnu_visibility_conflict, &ap->position);
          make_attr_unrecognized(ap);
        } else {
          vp->ELF_visibility = evk;
        }  /* if */
        break;
      case iek_type:
        tp = (a_type_ptr)entity;
        if (!C_mode() && gnu_version >= 40000 &&
            is_immediate_class_type(tp) && !class_type_has_body(tp)) {
          class_type_supp(tp)->ELF_visibility = evk;
        } else if ((clang_mode || gnu_version_is(>=60000)) &&
                   is_immediate_enum_type(tp)) {
          tp->variant.integer.ELF_visibility = evk;
        } else {
          pos_ty_warning(ec_attribute_does_not_apply_to_type, &ap->position,
                         tp);
        }  /* if */
        break;
      case iek_namespace:
        check_assertion(scope_stack_top().assoc_namespace ==
                                                     (a_namespace_ptr)entity);
        apply_ELF_visibility_to_current_namespace(evk);
        break;
      default:
        unexpected_condition();
    }  /* switch */
    if (evk == (an_ELF_visibility_kind)evk_unspecified) {
      /* An invalid visibility kind was specified. */
      pos_error(ec_unrecognized_visibility, &ap->arguments->position);
    }  /* if */
  }  /* if */
  return entity;
}  /* apply_visibility_attr */

#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */

static void check_unused_result_attr(an_attribute_ptr  ap,
                                     a_type_ptr        func_type)
/*
Apply the GNU "warn_unused_result" attribute specified by ap on the
specified function type.
*/
{
  check_assertion(ap != NULL && func_type != NULL &&
                  func_type->variant.routine.return_type != NULL);
  if (is_void_type(func_type->variant.routine.return_type)) {
    pos_warning(ec_warn_unused_result_with_void_return, &ap->position);
    make_attr_unrecognized(ap);
  } else {
    func_type->variant.routine.extra_info->result_should_be_used = TRUE;
  }  /* if */
}  /* check_unused_result_attr */


static void deferred_check_unused_result_attr(a_decl_parse_state_ptr  dps)
/*
A check for the "warn_unused_result" attribute has been deferred and can
now be completed.
*/
{
  an_attribute_ptr  ap;
  a_type_ptr        func_type;

  check_assertion(dps->sym != NULL &&
                  (is_function_or_template_symbol(dps->sym) ||
                   (dps->sym->kind == sk_type &&
                    (is_function_type(dps->sym->variant.type.ptr) ||
                     is_pointer_to_function_type(
                                               dps->sym->variant.type.ptr)))));
  func_type = underlying_function_type(dps->sym);
  ap = find_attribute(ak_warn_unused_result,
                      func_type->source_corresp.attributes);
  check_unused_result_attr(ap, func_type);
}  /* deferred_check_unused_result_attr */


static char* apply_warn_unused_result_attr(an_attribute_ptr  ap,
                                           char              *entity,
                                           an_il_entry_kind  entity_kind)
/*
Apply the GNU "warn_unused_result" attribute to the given entity and return
that entity.  See also the "nodiscard" standard attribute, which is similar.
*/
{
  a_type_ptr  func_type = get_func_type_for_attr(ap, &entity, entity_kind);

  if (func_type != NULL) {
    if (func_type->variant.routine.return_type == NULL) {
      /* If the attribute is in a parenthesized declarator, the return type of
         the function hasn't been parsed yet; record an end-of-parse action to
         apply this attribute. */
      a_decl_parse_state  *dps = (a_decl_parse_state*)ap->assoc_info;
      check_assertion(dps != NULL);
      add_end_of_parse_action(deferred_check_unused_result_attr, dps,
                              /*secondary_decls=*/FALSE);
    } else {
      /* The attribute can be handled now. */
      check_unused_result_attr(ap, func_type);
    }  /* if */
  }  /* if */
  return entity;
}  /* apply_warn_unused_result_attr */


static char* apply_weak_attr(ARG_UNUSED an_attribute_ptr ap,
                             char                        *entity,
                             an_il_entry_kind            entity_kind)
/*
Apply the GNU "weak" attribute to the given entity and return that entity.
*/
{
  if (entity_kind == iek_variable) {
    ((a_variable*)entity)->is_weak = TRUE;
  } else if (entity_kind == iek_routine) {
    if (((a_routine*)entity)->is_ifunc) {
      /* ifunc can't be weak. */
      pos_error(ec_ifunc_cant_be_weak, &ap->position);
    } else {
      ((a_routine*)entity)->is_weak = TRUE;
    }  /* if */
  } else {
    unexpected_condition();
  }  /* if */
  return entity;
}  /* apply_weak_attr */


static char* apply_weakref_attr(an_attribute_ptr  ap,
                                char              *entity,
                                an_il_entry_kind  entity_kind)
/*
Apply the GNU "weakref" attribute to the given entity and return that entity.
*/
{
  /* We already checked the target constraint "r|v".  Now check whether the
     entity is external (but apply the attribute even if that constraint
     fails since it results in better error recovery). */
  (void)check_target_entity_match(gnu_version < 40200 ? (char*)"r:+x!|v:+x!"
                                                      : (char*)"r:-x!|v:-x!",
                                  ap, entity, entity_kind);
  if (entity_kind == iek_routine) {
    ((a_routine_ptr)entity)->is_weak = TRUE;
    ((a_routine_ptr)entity)->is_weakref = TRUE;
  } else if (entity_kind == iek_variable) {
    ((a_variable_ptr)entity)->is_weak = TRUE;
    ((a_variable_ptr)entity)->is_weakref = TRUE;
  } else {
    unexpected_condition();
  }  /* if */
  if (ap->arguments != NULL) {
    entity = apply_alias_attr(ap, entity, entity_kind);
  }  /* if */
  return entity;
}  /* apply_weakref_attr */


static a_boolean abi_tag_list_is_subset_of(an_attribute_ptr superset_ap,
                                           an_attribute_ptr subset_ap)
/*
Returns TRUE if the list of narrow string literal abi_tag attribute arguments
pointed to by subset_ap is a subset of those specified by superset_ap.  When
FALSE is returned, an error is emitted.  Note that these lists are unordered,
requiring an exhaustive search.
*/
{
  an_attribute_ptr     super_ap, sub_ap;
  an_attribute_arg_ptr sub_aap, super_aap;
  a_boolean            result = TRUE;

  super_ap = find_attribute((an_attribute_kind)ak_abi_tag, superset_ap);
  sub_ap = find_attribute((an_attribute_kind)ak_abi_tag, subset_ap);
  check_assertion(super_ap != NULL && sub_ap != NULL);
  for (sub_aap = sub_ap->arguments; sub_aap != NULL; sub_aap = sub_aap->next) {
#if EXPENSIVE_CHECKING
    check_assertion(sub_aap->kind == (an_attribute_arg_kind)aak_constant &&
                    sub_aap->variant.constant->kind ==
                                              (a_constant_repr_kind)ck_string);
#endif /* EXPENSIVE_CHECKING */
    for (super_aap = super_ap->arguments;
         super_aap != NULL;
         super_aap = super_aap->next) {
#if EXPENSIVE_CHECKING
      check_assertion(super_aap->kind == (an_attribute_arg_kind)aak_constant &&
                      super_aap->variant.constant->kind ==
                                              (a_constant_repr_kind)ck_string);
#endif /* EXPENSIVE_CHECKING */
      if (string_constants_are_the_same(sub_aap->variant.constant,
                                        super_aap->variant.constant)) {
        /* Found this item on both lists; continue to the next item. */
        break;
      }  /* if */
    }  /* for */
    if (super_aap == NULL) {
      /* Didn't find this string on the superset list. */
      result = FALSE;
      break;
    }  /* if */
  }  /* for */
  if (!result) {
    a_diagnostic_ptr dp;
    check_assertion(sub_aap != NULL && super_ap != NULL);
    dp = pos_st_start_error(ec_abi_tag_redefinition, &sub_aap->position,
                            sub_aap->variant.constant->variant.string.value);
    add_diag_info_with_pos_insert(dp, ec_abi_tag_prev_declaration,
                                  &super_ap->position);
    end_diagnostic(dp);
  }  /* if */
  return result;
}  /* abi_tag_list_is_subset_of */


static char* apply_abi_tag_attr(an_attribute_ptr  ap,
                                char              *entity,
                                an_il_entry_kind  entity_kind)
/*
Apply the GNU "abi_tag" attribute to the given entity and return that entity.
Note that in cases where multiple abi_tag attributes are specified on the
same declaration, only the last abi_tag attribute is maintained (this seems
to match GNU's behavior).
*/
{
  if (C_mode()) {
    /* gcc allows the attribute with a warning that it is ignored (because
       it affects only mangling). */
    pos_warning(ec_abi_tag_ignored_in_C_mode, &ap->position);
    make_attr_unrecognized(ap);
  } else if (ap->arguments == NULL && entity_kind != iek_namespace) {
    /* abi_tag attributes can have no arguments for inline namespaces, but
       not for routines or types. */
    pos_st_error(ec_invalid_empty_attribute_arg_list, &ap->position,
                 attribute_display_name(ap));
    make_attr_unrecognized(ap);
  } else {
    /* Do processing for abi_tag attributes. */
    a_source_correspondence_ptr scp = (a_source_correspondence*)entity;
    a_decl_parse_state  *dps = (a_decl_parse_state*)ap->assoc_info;
    a_routine_ptr       rp = NULL;
    a_namespace_ptr     nsp = NULL;
    a_type_ptr          tp = NULL;
    a_variable_ptr      vp = NULL;
    an_attribute_ptr    prev;
    an_attribute_arg_ptr aap;
#if CHECKING
    /* Older versions of GNU accept more than just narrow string literals, but
       that seems to be a bug, so limit the arguments to narrow string
       literals. */
    for (aap = ap->arguments; aap != NULL; aap = aap->next) {
      check_assertion(aap->kind == (an_attribute_arg_kind)aak_constant &&
                      aap->variant.constant->kind ==
                                              (a_constant_repr_kind)ck_string);
    }  /* for */
#endif /* CHECKING */
    if (entity_kind == iek_routine) {
      rp = (a_routine_ptr)entity;
    } else if (entity_kind == iek_variable) {
      vp = (a_variable_ptr)entity;
    } else if (entity_kind == iek_namespace) {
      /* The abi_tag is being applied to a (presumably inline) namespace. */
      nsp = (a_namespace_ptr)entity;
      if (gnu_version < 50000) {
        /* Support for abi_tag attributes on inline namespaces was not present
           in earlier versions. */
        pos_warning(ec_attributes_ignored, &ap->position);
        make_attr_unrecognized(ap);
      } else if (!nsp->is_inline) {
        /* Ignore abi_tag attributes on non-inline namespaces. */
        pos_warning(ec_ignoring_attribute_on_non_inline_namespace,
                    &ap->position);
        make_attr_unrecognized(ap);
      } else if (nsp->source_corresp.name == NULL) {
        /* Ignore abi_tag attributes on anonymous namespaces. */
        pos_warning(ec_ignoring_attribute_on_anonymous_namespace,
                    &ap->position);
        make_attr_unrecognized(ap);
      } else if (ap->arguments == NULL) {
        /* If no arguments are specified, e.g., __attribute__((abi_tag)), GCC
           uses the namespace name as the abi_tag name.  Create a string
           constant and point a new attribute argument to it. */
        a_memory_region_number  region_to_switch_back_to;
        a_constant_ptr          constant = local_constant();
        a_targ_size_t           name_length =
                            strlen(nsp->source_corresp.name) + 1;/*lint !e776*/
        char *name = alloc_text_of_string_literal((sizeof_t)name_length);
        (void)strcpy(name, nsp->source_corresp.name);
        clear_constant(constant, (a_constant_repr_kind)ck_string);
        constant->type = string_type(name_length);
        constant->variant.string.length = name_length;
        constant->variant.string.value  = name;
        aap = alloc_attribute_arg();
        aap->kind = (an_attribute_arg_kind)aak_constant;
        /* No source position is recorded for this case. */
        switch_to_file_scope_region(&region_to_switch_back_to);
        aap->variant.constant = alloc_shareable_constant(constant);
        switch_back_to_original_region(region_to_switch_back_to);
        ap->arguments = aap;
        release_local_constant(&constant);
      }  /* if */
    } else {
      check_assertion(entity_kind == iek_type);
      tp = (a_type_ptr)entity;
      check_assertion(is_immediate_class_type(tp) ||
                      is_immediate_enum_type(tp));
    }  /* if */
    /* There appears to have been some major tweaking of the way the abi_tag
       was handled between the 4.8.0 and 4.9.0 releases of g++; the code
       below attempts to emulate both behaviors. */
    if (gnu_version < 40900) {
      if (entity_kind == iek_routine && rp->is_template_function) {
        /* It appears that abi_tag attributes are silently ignored on
           function templates before version 4.9.0. */
        make_attr_unrecognized(ap);
      } else if (entity_kind == iek_type &&
                 is_immediate_class_type(tp) &&
                 !ap->on_primary_declaration &&
                 tp->variant.class_struct_union.is_template_class) {
        /* In GCC 4.8.x the attribute is ignored on class declarations that
           result from template instantiations or specializations, unless
           the instantiation/specialization provides a definition.  For
           class templates this is handled by not instantiating the
           attribute at all.  But for members of class templates, we just
           discard the attribute here.  E.g., the attribute has no effect
           in the following:
             template<class T> struct S {
               struct __attribute((abi_tag("XYZ"))) N;
             };
             template<> struct S<int>::N {};
             void f(S<int>::N *p) {}
             void f(S<double>::N *p) {}
           */
        make_attr_unrecognized(ap);
      }  /* if */
    } else {
      if (dps != NULL &&
          entity_kind == iek_type &&
          is_immediate_class_type(tp) &&
          (dps->is_explicit_instantiation ||
           tp->variant.class_struct_union.is_specialized)) {
        /* Ignore attributes (with a warning) on explicit specializations
           (they had been accepted prior to 4.9.0). */
        pos_warning(ec_abi_tag_ignored_on_specialization, &ap->position);
        make_attr_unrecognized(ap);
      } else if (dps != NULL &&
                 entity_kind == iek_routine &&
                 rp->is_template_function &&
                 !rp->is_prototype_instantiation) {
        /* Attributes specified on specializations are also ignored. */
        pos_warning(ec_abi_tag_ignored_on_specialization, &ap->position);
        make_attr_unrecognized(ap);
      }  /* if */
    }  /* if */
    if (ap->kind == ak_abi_tag) {
      /* See if there are any previous abi_tag attributes on this entity (there
         should at least be the current abi_tag attribute). */
      prev = find_attribute(ak_abi_tag, scp->attributes);
      check_assertion(prev != NULL);
      if (entity_kind == iek_namespace && prev != ap) {
        /* Inline namespaces seem to "collect" attribute names.  We could
           check for and remove duplicates here, but that might cause problems
           in C++-generating configurations, so leave any duplicates at this
           point (any duplicates are removed during the mangling process). */
      } else {
        a_boolean redeclaration = FALSE;
        if (dps != NULL &&
            ((entity_kind == iek_routine &&
              !dps->first_decl &&
              !rp->defined_in_friend_decl) ||
             (entity_kind == iek_type &&
              dps->redeclares_tag &&
              dps->tag_def_or_forward_decl))) {
          /* This is a redeclaration of a function or class. */
          redeclaration = TRUE;
        }  /* if */
        if (!redeclaration) {
          if (prev == ap) {
            /* Usual case: not a redeclaration and a single abi_tag
               attribute. */
          } else {
            /* There are at least two abi_tag attributes on a single
               declaration; ignore the first (with a warning), then "remove"
               it. */
            pos_warning(ec_abi_tag_ignored, &prev->position);
            make_attr_unrecognized(prev);
          }  /* if */
        } else {
          /* A redeclaration. */
          if (prev == ap) {
            /* Previous declaration didn't have an abi_tag attribute, but this
               one does; report the mismatch. */
            pos_sy_error(ec_no_abi_tag_on_declaration, &ap->position,
                         (a_symbol_ptr)scp->assoc_info);
            make_attr_unrecognized(ap);
          } else {
            /* Make sure that every string in the new abi_tag list is also
               on the previous attribute list. */
            if (!abi_tag_list_is_subset_of(prev, ap)) {
              make_attr_unrecognized(ap);
            }  /* if */
            /* Get rid of the previous abi_tag attribute in any case (g++ only
               acts on the last one). */
            make_attr_unrecognized(prev);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    if (ap->kind == ak_abi_tag) {
      /* If the attribute hasn't been marked as unrecognized, set the
         corresponding flag in the entity. */
      gnu_abi_tag_attribute_seen = TRUE;
      if (entity_kind == iek_routine) {
        rp->has_gnu_abi_tag_attribute = TRUE;
      } else if (entity_kind == iek_variable) {
        vp->has_gnu_abi_tag_attribute = TRUE;
      } else if (entity_kind == iek_namespace) {
        /* Mark the namespace as having abi_tag attributes. */
        check_assertion(nsp != NULL);
        nsp->has_gnu_abi_tag_attribute = TRUE;
        /* Also indicate that entities defined in this namespace scope are
           subject to the attributes defined by this namespace. */
        check_assertion(scope_stack_top().assoc_namespace == nsp);
        scope_stack_top().in_gnu_abi_tag_namespace = TRUE;
      } else {
        tp->has_gnu_abi_tag_attribute = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return entity;
}  /* apply_abi_tag_attr */


void add_implicit_abi_tag_attribute(a_source_correspondence *scp,
                                    an_il_entry_kind        entity_kind,
                                    a_constant_ptr          con)
/*
Add an implicit abi_tag attribute to the entity described by scp and
entity_kind.  con is a string constant to be used for the attribute.  Note that
all implicit abi_tag attributes are collected in a single attribute (at the
head of the attribute list for the entity).
*/
{
  an_attribute_ptr      ap, implicit_ap;
  an_attribute_arg_ptr  aap;

  check_assertion(gnu_abi_tag_attribute_seen &&
                  con->kind == (a_constant_repr_kind)ck_string);
  for (ap = scp->attributes; ap != NULL; ap = ap->next) {
    if (ap->kind == ak_abi_tag) {
      for (aap = ap->arguments; aap != NULL; aap = aap->next) {
        check_assertion(aap->kind == (an_attribute_arg_kind)aak_constant &&
                        aap->variant.constant->kind ==
                                              (a_constant_repr_kind)ck_string);
        if (string_constants_are_the_same(con, aap->variant.constant)) {
          /* Don't add a duplicate abi_tag. */
          goto done;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* for */
  /* Maintain all implicit abi_tag attributes in a single attribute entry at
     the head of the attribute list for this entity (with potentially multiple
     attribute arguments).  There is no source position information (since
     these are implicit). */
  if (scp->attributes == NULL ||
      !scp->attributes->is_implicit_abi_tag_attribute) {
    implicit_ap = make_attribute((an_attribute_family)af_gnu);
    implicit_ap->kind = ak_abi_tag;
    implicit_ap->name = copy_string_to_region(file_scope_region_number,
                                              "abi_tag");
    implicit_ap->is_implicit_abi_tag_attribute = TRUE;
    implicit_ap->next = scp->attributes;
    scp->attributes = implicit_ap;
  } else {
    implicit_ap = scp->attributes;
  }  /* if */
  aap = alloc_attribute_arg();
  aap->kind = (an_attribute_arg_kind)aak_constant;
  aap->variant.constant = con;
  aap->next = implicit_ap->arguments;
  implicit_ap->arguments = aap;
  if (entity_kind == iek_routine) {
    a_routine_ptr rp = (a_routine_ptr)scp;
    rp->has_gnu_abi_tag_attribute = TRUE;
  } else {
    a_variable_ptr vp = (a_variable_ptr)scp;
    check_assertion(entity_kind == iek_variable);
    vp->has_gnu_abi_tag_attribute = TRUE;
  }  /* if */
done:;
}  /* add_implicit_abi_tag_attribute */

#endif /* GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED

static char* apply_appdomain_attr(an_attribute_ptr            ap,
                                  char                        *entity,
                                  ARG_UNUSED an_il_entry_kind entity_kind)
/*
Apply the Microsoft __declspec(appdomain) attribute to the given entity
(and return that entity).
*/
{
  if (!cli_or_cx_enabled) {
    pos_st_error(ec_cppcli_attribute_only, &ap->position,
                 attribute_display_name(ap));
    make_attr_unrecognized(ap);
  } else {
    /* "appdomain" cannot be combined with "align". */
    exclude_prior_attribute_kind(ak_align, ap, entity, entity_kind);
  }  /* if */
  return entity;
}  /* apply_appdomain_attr */


static void get_assembly_info_from_attribute(
                            an_attribute_ptr         ap,
                            an_assembly_scope_index  *assembly_scope_index,
                            a_cpp_cli_token          *metadata_type_def_token)
/*
Extract from the given assembly_info attribute the value of the assembly/scope
index and the metadata type-def token that it refers to.
*/
{
  a_constant_ptr  arg, arg2;
  a_boolean       ovflo;

  check_assertion(ap->kind == ak_assembly_info);
  check_assertion(ap->arguments != NULL && 
                  ap->arguments->kind == (an_attribute_arg_kind)aak_constant &&
                  ap->arguments->next != NULL &&
                  ap->arguments->next->kind == 
                                          (an_attribute_arg_kind)aak_constant);
  arg = ap->arguments->variant.constant;
  check_assertion(arg->kind == (a_constant_repr_kind)ck_integer);
  arg2 = ap->arguments->next->variant.constant;
  check_assertion(arg2->kind == (a_constant_repr_kind)ck_integer);
  *assembly_scope_index = 
      (an_assembly_scope_index)unsigned_value_of_integer_constant(arg, &ovflo);
  check_assertion(!ovflo);
  *metadata_type_def_token = 
             (a_cpp_cli_token)unsigned_value_of_integer_constant(arg2, &ovflo);
  check_assertion(!ovflo);
}  /* get_assembly_info_from_attribute */


static char *apply_assembly_info_attr(an_attribute_ptr  ap,
                                      char              *entity,
                                      an_il_entry_kind  entity_kind)
/*
Apply the Microsoft  __declspec(assembly_info(<index>, <def-token>)) attribute
to the given entity (and return that entity).  <index> is the assembly index
of the assembly in which type is defined.  <def-token> is the def-token of
the type.
*/
{
  a_type_ptr               tp = (a_type_ptr)entity;
  an_assembly_scope_index  assembly_scope_index;
  an_assembly_index        assembly_index;
  a_cpp_cli_token          metadata_type_def_token;
  a_cli_metadata_file_ptr  cmfp;

  /* Get the assembly index and the typedef token. */
  check_assertion(entity_kind == iek_type);
  get_assembly_info_from_attribute(ap, &assembly_scope_index,
                                   &metadata_type_def_token);
  assembly_index = assembly_index_from_assembly_scope_index(
                                                         assembly_scope_index);
  /* Get the assembly position. */
  cmfp = map_assembly_index_to_cmfp(assembly_index);
  if (cmfp == NULL) {
    catastrophe(ec_bad_assembly_index);
  }  /* if */
  /* Apply the values to the various il entries, but check that another
     assembly_info attribute has not already been applied to this type.  The
     first application should prevail so that the first definition encountered
     (from source or metadata) is used. */
  if (is_class_or_struct(tp)) {
    a_class_type_supplement_ptr  ctsp = class_type_supp(tp);
    if (compare_source_positions(&cmfp->inserted_position,
                                 &tp->source_corresp.decl_position) == 0 &&
        ctsp->assembly_scope_index == 0) {
      check_assertion(ctsp->assembly_scope_index == 0 &&
                      ctsp->metadata_type_def_token == 0);
      ctsp->assembly_scope_index = assembly_scope_index;
      ctsp->metadata_type_def_token = metadata_type_def_token;
    }  /* if */
  } else if (is_immediate_enum_type(tp)) {
    an_integer_type_supplement_ptr itsp = integer_type_supp(tp);
    if (compare_source_positions(&cmfp->inserted_position,
                                 &tp->source_corresp.decl_position) == 0 &&
        itsp->assembly_scope_index == 0) {
      check_assertion(itsp->assembly_scope_index == 0 &&
                      itsp->metadata_type_def_token == 0);
      itsp->assembly_scope_index = assembly_scope_index;
      itsp->metadata_type_def_token = metadata_type_def_token;
    }  /* if */
  } else {
    pos_error(ec_bad_assembly_info_attribute, &ap->position);
  }  /* if */
#if BACK_END_IS_CP_GEN_BE
  tp->has_been_declared = TRUE;
#endif /* BACK_END_IS_CP_GEN_BE */
  return entity;
}  /* apply_assembly_info_attr */


static char* apply_dllimport_dllexport_attr(an_attribute_ptr  ap,
                                            char              *entity,
                                            an_il_entry_kind  entity_kind)
/*
The dllimport and dllexport attributes are not really "applied" here, because
in some template cases some local information is not conveniently accessible
at this point.  However, some simple constraints can most easily be checked
and diagnosed here.
The given entity is returned.
*/
{
  if (entity_kind == iek_type) {
    check_assertion(ap->syntactic_location == al_tag_name);
    /* The warnings generated here appear to be generated only by earlier
       versions of Microsoft Visual Studio. */
    if (microsoft_version < 1200) {
      /* When applied to tag types, dllimport/dllexport is accepted on C++
         class types only. */
      if (C_mode() && is_immediate_class_type((a_type_ptr)entity)) {
        pos_st_warning(ec_struct_declspec_ignored_in_C_mode,
                       &ap->position, attribute_display_name(ap));
        make_attr_unrecognized(ap);
      } else if (is_immediate_enum_type((a_type_ptr)entity)) {
        pos_warning(ec_extended_modifier_ignored_on_enum, &ap->position);
        make_attr_unrecognized(ap);
      }  /* if */
    }  /* if */
  }  /* if */
  return entity;
}  /* apply_dllimport_dllexport_attr */


static char* apply_edg_interior_ptr_alias_attr(an_attribute_ptr  ap,
                                               char              *entity,
                                               an_il_entry_kind  entity_kind)
/*
Apply the __declspec(__edg_interior_ptr_alias) attribute.  entity must
represent an alias (the instantiation of an alias template) to a type T; the
attribute turns it into an interior pointer to T.
*/
{
  if (!cppcli_enabled) {
    pos_st_error(ec_cppcli_attribute_only, &ap->position,
                 attribute_display_name(ap));
    make_attr_unrecognized(ap);
  } else {
    a_type_ptr  tp;
    check_assertion(entity_kind == iek_type);
    tp = (a_type_ptr)entity;
    if (!type_is_typedef(tp) || !typeref_is_using_decl_alias(tp)) {
      report_bad_attribute_target(es_error, ap);
    } else {
      tp->variant.typeref.type =
                             make_interior_ptr_type(tp->variant.typeref.type);
    }  /* if */
  }  /* if */
  return entity;
}  /* apply_edg_interior_ptr_alias_attr */


static char* apply_edg_pin_ptr_alias_attr(an_attribute_ptr  ap,
                                          char              *entity,
                                          an_il_entry_kind  entity_kind)
/*
Apply the __declspec(__edg_pin_ptr_alias) attribute.  entity must represent an
alias (the instantiation of an alias template) to a type T; the attribute
turns it into an pin pointer to T.
*/
{
  if (!cppcli_enabled) {
    pos_st_error(ec_cppcli_attribute_only, &ap->position,
                 attribute_display_name(ap));
    make_attr_unrecognized(ap);
  } else {
    a_type_ptr  tp;
    check_assertion(entity_kind == iek_type);
    tp = (a_type_ptr)entity;
    if (!type_is_typedef(tp) || !typeref_is_using_decl_alias(tp)) {
      report_bad_attribute_target(es_error, ap);
    } else {
      tp->variant.typeref.type = make_pin_ptr_type(tp->variant.typeref.type);
    }  /* if */
  }  /* if */
  return entity;
}  /* apply_edg_pin_ptr_alias_attr */


static char* apply_implementation_key_attr(
                                       an_attribute_ptr            ap,
                                       char                        *entity,
                                       ARG_UNUSED an_il_entry_kind entity_kind)
/*
Check the Microsoft __declspec(implementation_key) attribute (it isn't really
"applied" to the given entity).  Return the given entity.
*/
{
  if (!in_microsoft_implementation_key_mapping_region) {
    pos_error(ec_implementation_key_outside_mapping_region, &ap->position);
    make_attr_unrecognized(ap);
  }  /* if */
  return entity;
}  /* apply_implementation_key_attr */


static char* apply_intrin_type_attr(ARG_UNUSED an_attribute_ptr ap,
                                    char                        *entity,
                                    ARG_UNUSED an_il_entry_kind entity_kind)
/*
Apply the Microsoft __declspec(intrin_type) attribute to the given entity (and
return that entity).
*/
{
  check_assertion(entity_kind == iek_type);
  ((a_type_ptr)entity)->is_microsoft_intrinsic = TRUE;
  return entity;
}  /* apply_intrin_type_attr */


static char* apply_jitintrinsic_attr(an_attribute_ptr            ap,
                                     char                        *entity,
                                     ARG_UNUSED an_il_entry_kind entity_kind)
/*
Apply the Microsoft __declspec(jitintrinsic) attribute to the given entity
(and return that entity).
*/
{
  if (!cli_or_cx_enabled) {
    pos_st_error(ec_cppcli_attribute_only, &ap->position,
                 attribute_display_name(ap));
    make_attr_unrecognized(ap);
  }  /* if */
  return entity;
}  /* apply_jitintrinsic_attr */


static char* apply_noalias_attr(ARG_UNUSED an_attribute_ptr ap,
                                char                        *entity,
                                ARG_UNUSED an_il_entry_kind entity_kind)
/*
Apply the Microsoft __declspec(noalias) attribute to the given entity (and
return that entity).
*/
{
  check_assertion(entity_kind == iek_routine);
  ((a_routine*)entity)->decl_modifiers |= DM_NOALIAS;
  return entity;
}  /* apply_noalias_attr */


static char* apply_non_user_code_attr(ARG_UNUSED an_attribute_ptr ap,
                                      char                        *entity,
                                      ARG_UNUSED an_il_entry_kind entity_kind)
/*
Apply the Microsoft __declspec(non_user_code) attribute to the given entity
(and return that entity).
*/
{
  return entity;
}  /* apply_non_user_code_attr */


static char* apply_novtable_attr(ARG_UNUSED an_attribute_ptr ap,
                                 char                        *entity,
                                 ARG_UNUSED an_il_entry_kind entity_kind)
/*
Apply the Microsoft __declspec(novtable) attribute to the given entity (and
return that entity).
*/
{
  a_type_ptr  tp = (a_type_ptr)entity;

  check_assertion(entity_kind == iek_type && is_immediate_class_type(tp));
  class_type_supp(tp)->decl_modifiers |= DM_NOVTABLE;
  return entity;
}  /* apply_novtable_attr */


static char* apply_process_attr(an_attribute_ptr            ap,
                                char                        *entity,
                                ARG_UNUSED an_il_entry_kind entity_kind)
/*
Apply the Microsoft __declspec(process) attribute to the given entity
(and return that entity).
*/
{
  if (!cli_or_cx_enabled) {
    pos_st_error(ec_cppcli_attribute_only, &ap->position,
                 attribute_display_name(ap));
    make_attr_unrecognized(ap);
  }  /* if */
  return entity;
}  /* apply_process_attr */


static char* apply_property_attr(an_attribute_ptr  ap,
                                 char              *entity,
                                 an_il_entry_kind  entity_kind)
/*
Apply the Microsoft __declspec(property) attribute to the given entity (and
return that entity).  Since the attribute arguments were recorded as a
sequence of "raw tokens", this involves some simple parsing of that token
stream.
*/
{
  if (entity_kind != iek_field) {
    report_bad_attribute_target(es_warning, ap);
  } else if (cli_or_cx_enabled &&
             is_managed_class_type(parent_class_of((a_field*)entity))) {
    /* __declspec(property(...)) is not allowed on members of managed class
       types. */
    pos_error(ec_property_attribute_in_managed_class, &ap->position);
    make_attr_unrecognized(ap);
  } else {
    an_attribute_arg_ptr  aap = ap->arguments;
    a_field_ptr           fp = (a_field*)entity;
    an_error_code         errcode = ec_bad_declspec_property;
    /* The raw token sequence should correspond to one of the following forms:
       "get = <id>", "put = <id>", "get = <id> , put = <id>", or
       "put = <id> , get = <id>". */
    check_assertion(!field_is_property_or_event(fp) ||
                    property_or_event_kind_is(fp, pek_declspec_property));
    for (;;) {
      a_boolean  is_get = FALSE, is_put = FALSE;
      /* Check for "get" or "put". */
      check_assertion(aap != NULL);
      if (aap->kind != (an_attribute_arg_kind)aak_raw_token) {
        break;
      } else if (strcmp(aap->variant.token, "get") == 0) {
        if (field_is_property_or_event(fp) &&
            fp->property_or_event_descr->get_routine.name != NULL) {
          errcode = ec_dupl_get_or_put;
          break;
        }  /* if */
        is_get = TRUE;
      } else if (strcmp(aap->variant.token, "put") == 0) {
        if (field_is_property_or_event(fp) &&
            fp->property_or_event_descr->set_routine.name != NULL) {
          errcode = ec_dupl_get_or_put;
          break;
        }  /* if */
        is_put = TRUE;
      } else {
        break;
      }  /* if */
      aap = aap->next;
      /* Skip the "=" that should follow. */
      check_assertion(aap != NULL);
      if (aap->kind != (an_attribute_arg_kind)aak_raw_token ||
          aap->token_kind != tok_assign) {
        errcode = ec_exp_assign;
        break;
      }  /* if */
      aap = aap->next;
      /* An identifier should be next: Record it in the field entry. */
      check_assertion(aap != NULL);
      if (aap->token_kind != tok_identifier) {
        errcode = ec_exp_identifier;
        break;
      } else if (!is_get && !is_put) {
        unexpected_condition();
      } else {
        if (fp->property_or_event_descr == NULL) {
          fp->property_or_event_descr = alloc_property_or_event_descr(
                             (a_property_or_event_kind)pek_declspec_property);
          fp->property_or_event_descr->variant.field = fp;
        }  /* if */
        if (is_get) {
          fp->property_or_event_descr->get_routine.name = aap->variant.token;
        } else {
          fp->property_or_event_descr->set_routine.name = aap->variant.token;
        }  /* if */
      }  /* if */
      aap = aap->next;
      /* Check for a comma. */
      check_assertion(aap != NULL);
      if (aap->kind == (an_attribute_arg_kind)aak_empty) {
        /* We found the end of the construct without error. */
        aap = aap->next;
        check_assertion(aap == NULL);
        break;
      } else {
        check_assertion(aap->kind == (an_attribute_arg_kind)aak_raw_token);
        if (aap->token_kind == tok_comma) {
          /* A comma: Continue the loop. */
          aap = aap->next;
        } else {
          errcode = ec_exp_rparen;
          break;
        }  /* if */
      }  /* if */
    }  /* for */
    if (aap != NULL) {
      pos_error(errcode, &aap->position);
      if (!field_is_property_or_event(fp)) {
        a_decl_parse_state  *dps = (a_decl_parse_state*)ap->assoc_info;
        /* The field isn't a property field after all, but the field type was
           checked assuming this would be a property field.  Avoid error
           recovery issues by proceeding with an error type. */
        dps->type = fp->type = error_type();
        dps->is_property_or_event_field = FALSE;
        dps->is_declspec_property_field = FALSE;
        make_attr_unrecognized(ap);
      }  /* if */
    } else if (fp->is_bit_field) {
      pos_diagnostic(es_discretionary_error, ec_declspec_property_not_allowed,
                     &ap->position);
    }  /* if */
  }  /* if */
  return entity;
}  /* apply_property_attr */


static char* apply_restrict_attr(an_attribute_ptr            ap,
                                 char                        *entity,
                                 ARG_UNUSED an_il_entry_kind entity_kind)
/*
Apply the Microsoft __declspec(restrict) attribute to the given entity (and
return that entity).
*/
{
  a_routine_ptr  rp = (a_routine*)entity;

  check_assertion(entity_kind == iek_routine);
  if (is_pointer_or_handle_type(return_type_of(rp->type))) {
    rp->decl_modifiers |= DM_RESTRICT;
  } else {
    pos_error(ec_bad_declspec_restrict_return, &ap->position);
  }  /* if */
  return entity;
}  /* apply_restrict_attr */


static char* apply_safebuffers_attr(ARG_UNUSED an_attribute_ptr ap,
                                    char                        *entity,
                                    ARG_UNUSED an_il_entry_kind entity_kind)
/*
Apply the Microsoft __declspec(safebuffers) attribute to the given entity (and
return that entity).
*/
{
  a_routine_ptr  rp = (a_routine*)entity;

  check_assertion(entity_kind == iek_routine);
  rp->decl_modifiers |= DM_SAFEBUFFERS;
  return entity;
}  /* apply_safebuffers_attr */


static char* apply_selectany_attr(an_attribute_ptr            ap,
                                  char                        *entity,
                                  ARG_UNUSED an_il_entry_kind entity_kind)
/*
Apply the Microsoft __declspec(selectany) attribute to the given entity (and
return that entity).
*/
{
  check_assertion(entity_kind == iek_variable);
  if (scope_stack[decl_scope_level].kind ==
                                       (a_scope_kind)sck_class_struct_union) {
    /* The declaration of a static data member.  The selectany specifier can
       appear on an out-of-class static data member definition, but not on an
       in-class declaration (even if the in-class declaration has an
       initializer). */
    pos_st_diagnostic(es_discretionary_error,
                      ec_decl_modifiers_invalid_for_this_decl, &ap->position,
                      attribute_display_name(ap));
  } else {
    ((a_variable*)entity)->decl_modifiers |= DM_SELECTANY;
  }  /* if */
  return entity;
}  /* apply_selectany_attr */

#if THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED

static char* apply_thread_attr(an_attribute_ptr            ap,
                               char                        *entity,
                               ARG_UNUSED an_il_entry_kind entity_kind)
/*
Apply the Microsoft __declspec(thread) attribute to the given entity (and
return that entity).
*/
{
  a_variable_ptr      vp = (a_variable*)entity;
  a_decl_parse_state  *dps = (a_decl_parse_state*)ap->assoc_info;

  check_assertion(entity_kind == iek_variable && dps != NULL);
  /* The "thread" specifier can only be applied to variables with a static
     lifetime. */
  if (!var_has_static_storage_duration(vp)) {
    pos_error(ec_cannot_use_thread_local_storage, &ap->position);
  } else if (!dps->first_decl && !(vp->decl_modifiers & DM_THREAD)) {
    /* This variable was previously declared with no thread locality. */
    pos_sy_error(ec_incompatible_thread_locality, &ap->position, dps->sym);
  } else {
    vp->decl_modifiers |= DM_THREAD;
  }  /* if */
  return entity;
}  /* apply_thread_attr */

#endif /* THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED */

static char* apply_uuid_attr(an_attribute_ptr  ap,
                             char              *entity,
                             an_il_entry_kind  entity_kind)
/*
Apply the Microsoft __declspec(uuid(...)) attribute to the given entity (and
return that entity).
*/
{
  a_constant_ptr  arg;
  char            *str;

  check_assertion(entity_kind == iek_type);
  check_assertion(ap->arguments != NULL && ap->arguments->next == NULL &&
                  ap->arguments->kind == (an_attribute_arg_kind)aak_constant);
  arg = ap->arguments->variant.constant;
  check_assertion(arg->kind == (a_constant_repr_kind)ck_string);
  if (!convert_GUID_string_literal(arg, &str)) {
    pos_error(ec_bad_uuid_string, &ap->arguments->position);
  } else {
    a_type_ptr    tp = (a_type_ptr)entity;
    a_const_char  *prev_str = uuid_string_of_type(tp);
    if (prev_str != NULL && strcmp(prev_str, str) != 0) {
      pos_diagnostic(es_discretionary_error,
                     ec_decl_modifiers_incompatible_with_previous_decl,
                     &ap->position);
    } else if (is_immediate_class_type(tp)) {
      /* If this is a class template instance (but not an explicit
         specialization), the uuid is ignored. */
      if (!tp->variant.class_struct_union.is_template_class ||
          tp->variant.class_struct_union.is_specialized) {
        class_type_supp(tp)->uuid_string = str;
      }  /* if */
    } else if (is_immediate_enum_type(tp)) {
      if (C_mode()) {
        report_bad_attribute_target(es_discretionary_error, ap);
      } else {
        integer_type_supp(tp)->uuid_string = str;
      }  /* if */
    } else {
      unexpected_condition();
    }  /* if */
  }  /* if */
  return entity;
}  /* apply_uuid_attr */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if INCLUDE_EDG_TEST_ATTRIBUTES

static char* apply_edg_e1_attr(an_attribute_ptr            ap,
                               char                        *entity,
                               ARG_UNUSED an_il_entry_kind entity_kind)
/*
A test attribute that triggers an error in all contexts (it has no other
effect).
*/
{
  pos_error(ec_unattached_attribute, &ap->position);
  return entity;
}  /* apply_edg_e1_attr */


static char* apply_edg_n1_attr(an_attribute_ptr            ap,
                               char                        *entity,
                               ARG_UNUSED an_il_entry_kind entity_kind)
/*
A test attribute that triggers an error if it appears outside of namespace
scope (it has no other effect).
*/
{
  if (!is_file_or_namespace_scope(&scope_stack_top())) {
    pos_error(ec_unattached_attribute, &ap->position);
  }  /* if */
  return entity;
}  /* apply_edg_n1_attr */

#endif /* INCLUDE_EDG_TEST_ATTRIBUTES */

static a_boolean is_class_template_member_entity(char             *entity,
                                                 an_il_entry_kind entity_kind)
/*
Return TRUE if entity is a member of a class template (including prototype
instantiations).
*/
{
  a_boolean    result = FALSE;
  a_type_ptr   parent_class = NULL;
  a_symbol_ptr sym = NULL;

  if (entity_kind == iek_routine) {
    sym = symbol_for((a_routine_ptr)entity);
  } else if (entity_kind == iek_variable) {
    sym = symbol_for((a_variable_ptr)entity);
  } else if (entity_kind == iek_type) {
    sym = symbol_for((a_type_ptr)entity);
  }  /* if */
  if (sym != NULL && sym->is_class_member) {
    parent_class = sym_parent_class(sym);
  }  /* if */
  if (parent_class != NULL) {
    parent_class = skip_typerefs(parent_class);
    result = parent_class->variant.class_struct_union.is_template_class;
  }  /* if */
  return result;
}  /* is_class_template_member_entity */


static char* apply_exclude_from_explicit_instantiation_attr(
                                             an_attribute_ptr  ap,
                                             char              *entity,
                                             an_il_entry_kind  entity_kind)
/*
The given entity must be a member of a class template.  Issue a warning if
the entity is not applicable for the exclude_from_explicit_instantiation
attribute.
*/
{
  if (!is_class_template_member_entity(entity, entity_kind)) {
    pos_st_diagnostic(es_warning, ec_wrong_entity_for_attribute,
                      &ap->position, attribute_display_name(ap));
    make_attr_unrecognized(ap);
  }  /* if */
  return entity;
}  /* apply_exclude_from_explicit_instantiation_attr */


static char* apply_availability_attr(an_attribute_ptr            ap,
                                     ARG_UNUSED char             *entity,
                                     ARG_UNUSED an_il_entry_kind entity_kind)
/*
Apply the "availability" attribute to a declared entity and return that entity.
Full support of this attribute requires knowledge about the target operating
system which the front end does not possess.  Invoke the
check_availability_attr routine (intended to be replaced by customers) to
determine whether the corresponding declaration should be made invisible.
See https://clang.llvm.org/docs/AttributeReference.html#availability for the
entire attribute syntax (not parsed here).
*/
{
  if (!check_availability_attr(ap)) {
    /* The current mode deactivates the annotated declaration: Mark the symbol
       as invisible. */
    a_decl_parse_state  *dps = (a_decl_parse_state*)ap->assoc_info;
    check_assertion(dps != NULL);
    if (dps->sym != NULL) {
      dps->sym->is_invisible = TRUE;
    }  /* if */
  }  /* if */
  return entity;
}  /* apply_availability_attr */


static char* apply_using_if_exists_attr(an_attribute_ptr ap,
                                        char             *entity,
                                        an_il_entry_kind entity_kind)
/*
Called to apply the "using_if_exists" attribute to a declared entity and return
that entity.  The "using_if_exists" attribute appears to be supported by clang
in two locations, e.g.:

  [[clang::using_if_exists]] using NS::not_there, NS::not_there2;
  using NS::not_there3, NS::not_there4 [[clang::using_if_exists]];

In the first case, this routine is called with entity being the
using-declaration, in the second case, the attribute is applied to the
"NS::not_there4" IL entity (if one exists).

The actual semantics are implemented by "looking ahead" among the attributes,
which happens before this function is invoked.
*/
{
  if (ap->syntactic_location == al_post_using_declarator ||
      (ap->syntactic_location == al_prefix &&
       entity_kind == iek_using_decl)) {
    /* Looks okay. */
  } else {
    /* This attribute doesn't apply here. */
    pos_st_diagnostic(es_discretionary_error, ec_wrong_entity_for_attribute,
                      &ap->position, attribute_display_name(ap));
  }  /* if */
  return entity;
}  /* apply_using_if_exists_attr */

#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
/*
The "alias" and "weakref" attributes can refer to entities that are declared
later in a translation unit.  Therefore, we record such attributes in a fixup
list and process the list at the end of the translation unit.  This is also
used to implement the "redefine_extname" pragma used by the Sun Solaris
operating system.
*/

typedef struct an_alias_fixup *an_alias_fixup_ptr;
typedef struct an_alias_fixup {
  an_alias_fixup_ptr
		next;	/* Pointer to the next fixup to process. */
  a_symbol_ptr	alias;
			/* The symbol that is an alias for another entity.
			   NULL if this is an entry created by a
			   redefine_extname pragma directive. */
  a_const_char	*alias_name;
			/* If this is an entry created by a redefine_extname
			   pragma directive, the name to substitute by the
			   name indicated by aliased_name.  NULL otherwise. */
  a_const_char	*aliased_name;
			/* The name of the entity being aliased. */
  a_source_position
		alias_position;
			/* The position of the alias attribute (used for error
			   reporting purposes). */
} an_alias_fixup;

/* Pointer to the head of the list of alias fixups. */
STATIC_THREAD an_alias_fixup_ptr
		alias_fixup_list;

/* Pointer to the last element on the list of alias fixups. */
STATIC_THREAD an_alias_fixup_ptr
		last_alias_fixup;

/* Pointer to a list of available (freed) alias fixups. */
STATIC_THREAD an_alias_fixup_ptr
		avail_alias_fixups;

#if DEBUG
STATIC_THREAD unsigned long
		num_alias_fixups_allocated;
#endif /* DEBUG */


static void add_alias_fixup(a_symbol_ptr        alias,
                            a_const_char        *alias_name,
                            a_const_char        *aliased_name,
                            a_source_position   *alias_position)
/*
Allocate a fixup entry for a new alias described by the given parameters.
Note that the caller must allocate alias_name in the IL (when non-NULL) as
it will be pointed to by the aliased IL entry.
*/
{
  an_alias_fixup_ptr  entry;

  if (avail_alias_fixups != NULL) {
    entry = avail_alias_fixups;
    avail_alias_fixups = avail_alias_fixups->next;
  } else {
    entry = (an_alias_fixup_ptr)alloc_fe(sizeof(an_alias_fixup));
#if DEBUG
    ++num_alias_fixups_allocated;
#endif /* DEBUG */
  }  /* if */
  /* Append the entry at the of the fixup list. */
  entry->next = NULL;
  if (alias_fixup_list == NULL) {
    alias_fixup_list = entry;
  } else {
    last_alias_fixup->next = entry;
  }  /* if */
  last_alias_fixup = entry;
  /* Fill in the entry's fields. */
  entry->alias = alias;
  entry->alias_name = alias_name;
  entry->aliased_name = aliased_name;
  entry->alias_position = *alias_position;
#if GNU_EXTENSIONS_ALLOWED
  if (alias != NULL) {
    /* This fixup represents an entity declared with the "alias" or "weakref"
       attributes.  This is recorded early so that we can tell whether an
       entity is an alias before the alias is resolved. */
    alias->is_alias = TRUE;
    if (is_simple_function_symbol(alias)) {
      /* When the fixup entry is resolved, the GNU routine supplement entry
         will have to be updated.  Ordinarily, the supplement is allocated
         lazily, but fixup resolution is a very late process that possibly
         occurs after cross-translation-unit correspondence processing.  We
         therefore ensure its allocation now. */
      a_routine_ptr  rp = alias->variant.routine.ptr;
      if (!has_gnu_routine_supp(rp)) {
        (void)alloc_gnu_supplement_for_routine(alias->variant.routine.ptr);
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
}  /* add_alias_fixup */


static void free_alias_fixup(an_alias_fixup_ptr  entry)
/*
Return the given entry to the list of available entries.
*/
{
  entry->next = avail_alias_fixups;
  avail_alias_fixups = entry;
}  /* free_alias_fixup */

#if GNU_EXTENSIONS_ALLOWED

static void report_any_alias_loop(an_alias_fixup_ptr  alias_fixup)
/*
Issue an error if the given alias fixup describes an alias that completes a
cycle of aliased entities.  Break the cycle if that is the case.
*/
{
  a_boolean     alias_loop = FALSE;
  a_symbol_ptr  sym = alias_fixup->alias;

  switch (sym->kind) {
    case sk_routine:
      { a_routine_ptr  orig_rp = sym->variant.routine.ptr, rp;
        rp = gnu_routine_supp(orig_rp)->aliased_routine;
        for (; rp != NULL && has_gnu_routine_supp(rp);
               rp = gnu_routine_supp(rp)->aliased_routine) {
          if (same_entities(rp, orig_rp)) {
            alias_loop = TRUE;
            ensure_gnu_routine_supp(orig_rp)->aliased_routine = NULL;
            orig_rp->implicit_alias = FALSE;
            break;
          }  /* if */
        }  /* for */
      }
      break;
    case sk_variable:
      { a_variable_ptr  orig_vp = sym->variant.variable.ptr,
                        vp = orig_vp->aliased_variable;
        for (; vp != NULL; vp = vp->aliased_variable) {
          if (same_entities(vp, orig_vp)) {
            alias_loop = TRUE;
            orig_vp->aliased_variable = NULL;
            break;
          }  /* if */
        }  /* for */
      }
      break;
    default:
      unexpected_condition();
  }  /* switch */
  if (alias_loop) {
    pos_error(ec_alias_loop, &alias_fixup->alias_position);
  }  /* if */
}  /* report_any_alias_loop */


static a_boolean is_aliasable(a_symbol_ptr  aliased_sym,
                              a_symbol_ptr  alias_sym)
/*
alias_sym represents an alias declared with the GNU "alias" or "weakref"
attribute, and aliased_sym (which may be NULL) is what was found when looking
up the name indicated by the attribute.  Return TRUE if alias_sym can indeed
be recorded as an alias for aliased_sym.
*/
{
  a_boolean  result;

  if (aliased_sym == NULL) {
    /* The alias is to a name not at all declared in the current translation
       unit. */
    result = FALSE;
  } else if (aliased_sym->kind != alias_sym->kind) {
    /* The alias refers to an entity of a kind different from that implied
       by the alias declaration (e.g., a variable alias referring to a
       function declaration).  This is treated as "aliasable" here: An error
       is issued elsewhere. */
    result = TRUE;
  } else if (aliased_sym->defined || aliased_sym->is_alias) {
    /* These cases are always valid. */
    result = TRUE;
  } else {
    /* If the alias is established with the "alias" attribute, aliased_sym
       must be defined or it must itself be an alias.  However, if it is
       defined with the "weakref" attribute, that is not required. */ 
    a_boolean  is_weakref = FALSE;
    switch (alias_sym->kind) {
      case sk_routine:
        is_weakref = alias_sym->variant.routine.ptr->is_weakref;
        break;
      case sk_variable:
        is_weakref = alias_sym->variant.variable.ptr->is_weakref;
        break;
      default:
        unexpected_condition();
    }  /* if */
    result = is_weakref;
  }  /* if */
  return result;
}  /* is_aliasable */


/* Pointer to a hash table mapping explicit asm names (a GNU extension) to
   symbols corresponding to the entities declared with the explicit asm
   names.  This is used when looking up alias names (which should find asm
   names). */
STATIC_THREAD a_hash_table_ptr
		asm_name_map;


a_boolean compare_for_asm_name_map(a_void_ptr  entry,
                                   a_void_ptr  key)
/*
Compare the asm name associated with entry (entry is a symbol pointer) to the
given key (key is a pointer to a character string).  Return TRUE if they are
equal.
*/
{
  a_symbol_ptr  sym = (a_symbol_ptr)entry;
  a_const_char  *str = NULL;

  switch (sym->kind) {
    case sk_variable:
      check_assertion(sym->variant.variable.ptr->asm_name_is_valid);
      str = sym->variant.variable.ptr->asm_name_or_reg.name;
      break;
    case sk_routine:
      str = gnu_routine_supp(sym->variant.routine.ptr)->asm_name;
      break;
    default:
      unexpected_condition();
  }  /* switch */
  return strcmp(str, (char*)key) == 0;
}  /* compare_for_asm_name_map */


void record_asm_name_for_lookup(a_symbol_ptr  sym)
/*
The given symbol represents a variable or a routine.  If it has a GNU asm name,
record the name-symbol pair for easy lookup later on (in case a GNU alias
attribute refers to that name).
*/
{
  a_symbol_ptr  *p_sym;
  a_const_char  *str = NULL;

  switch (sym->kind) {
    case sk_variable:
      if (sym->variant.variable.ptr->asm_name_is_valid) {
        str = sym->variant.variable.ptr->asm_name_or_reg.name;
      }  /* if */
      break;
    case sk_routine:
      str = gnu_routine_supp(sym->variant.routine.ptr)->asm_name;
      break;
    default:
      unexpected_condition();
  }  /* switch */
  if (str != NULL) {
    p_sym = (a_symbol_ptr*)hash_find(asm_name_map, (a_void_ptr)str,
                                     /*create=*/TRUE);
    /* If multiple entities are declared with the same asm name, retain the
       first one if it is "defined" (aliases are considered "defined" in this
       context).  Otherwise, record the new one instead. */
    if (*p_sym == NULL || (!(*p_sym)->defined && !(*p_sym)->is_alias)) {
      *p_sym = sym;
    }  /* if */
  }  /* if */
}  /* record_asm_name_for_lookup */

#endif /* GNU_EXTENSIONS_ALLOWED */

void process_alias_fixup_list(a_boolean  early_attr_resolution)
/*
Traverse the list of alias fixups and set the alias fields as needed.
Also used for the GNU ifunc attribute.  If early_attr_resolution is TRUE, 
this is an early pass to tentatively resolve some attributes.  Another pass
will be made later on.
*/
{
  an_alias_fixup_ptr  entries = alias_fixup_list, entry;
  a_symbol_ptr        aliased_sym;
  a_symbol_locator    locator;
  a_source_position   *pos = NULL;

  alias_fixup_list = last_alias_fixup = NULL;
  while (entries != NULL) {
    a_boolean  keep_entry = early_attr_resolution;
    aliased_sym = NULL;
    entry = entries;
    entries = entries->next;
    if (entry->alias == NULL) {
      /* This entry is the result of a redefine_extname pragma directive. */
      if (early_attr_resolution) goto next_entry;
      pos = &entry->alias_position;
#if GNU_EXTENSIONS_ALLOWED
    } else {
      pos = &entry->alias->decl_position;
      if (entry->alias->defined &&
          !(entry->alias->kind == (a_symbol_kind)sk_variable &&
            entry->alias->variant.variable.ptr->storage_class ==
                                                 (a_storage_class)sc_static &&
            entry->alias->variant.variable.ptr->init_kind ==
                                                  (an_init_kind)initk_none)) {
        /* An entity cannot have a definition and simultaneously be an alias
           for another entity.  An exception is made for weakref attributes on
           variables: They are defined, but they cannot have an initializer.
           GNU also allows ifunc attributes to have a definition (but only
           in C mode); it causes problems later on, so we give an error. */
        if (early_attr_resolution) goto next_entry;
        pos_error(ec_alias_cannot_have_definition, pos);
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    }  /* if */
    clear_locator(&locator, pos);
    (void)find_symbol(entry->aliased_name,
                      (sizeof_t)strlen(entry->aliased_name), &locator);
    aliased_sym = normal_id_lookup(&locator, IDL_LINKAGE_LOOKUP);
#if GNU_EXTENSIONS_ALLOWED
    if (entry->alias != NULL && !is_aliasable(aliased_sym, entry->alias)) {
      /* If ordinary lookup of the alias attribute didn't yield an aliasable
         entity, we attempt to find the alias among the GNU asm names we
         previously recorded. */
      a_symbol_ptr  *p_sym;
      p_sym = (a_symbol_ptr*)hash_find(asm_name_map,
                                       (a_void_ptr)entry->aliased_name,
                                       /*create=*/FALSE);
      if (p_sym != NULL && *p_sym != NULL) aliased_sym = *p_sym;
    }  /* if */
    if (early_attr_resolution && aliased_sym == NULL) goto next_entry;
#endif /* GNU_EXTENSIONS_ALLOWED */
    if (entry->alias == NULL) {
#if REDEFINE_EXTNAME_PRAGMA_ENABLED
      /* This entry corresponds to a redefine_extname pragma directive. */
      check_assertion(entry->alias_name != NULL);
      if (aliased_sym == NULL) {
        /* Normal symbol lookup failed to find a suitable symbol; look
           on the other_symbols list for routines/variables that may have been
           implicitly declared in scopes that have since been popped, e.g.,
              int main() {
                old();
              }
           where "old" has no previous declaration. */
        a_symbol_ptr ext_sym;
        for (ext_sym = locator.symbol_header->other_symbols;
             ext_sym != NULL;
             ext_sym = ext_sym->next) {
          if (ext_sym->kind == (a_symbol_kind)sk_extern_routine) {
            a_routine_ptr rp =
                     ext_sym->variant.extern_symbol_descr->variant.routine.ptr;
            ensure_gnu_routine_supp(rp)->asm_name = entry->alias_name;
            break;
          } else if (ext_sym->kind == (a_symbol_kind)sk_extern_variable) {
            ext_sym->variant.extern_symbol_descr->variant.variable->
                                      asm_name_or_reg.name = entry->alias_name;
            break;
          }  /* if */
        }  /* for */
        /* If ext_sym == NULL, there is no declaration on which the pragma has
           an effect (but no diagnostic is issued -- which matches GNU's
           and Sun's behavior). */
      } else {
        a_source_correspondence_ptr  scp = NULL;
        switch (aliased_sym->kind) {
          case sk_routine:
            ensure_gnu_routine_supp(aliased_sym->variant.routine.ptr)->
                                                  asm_name = entry->alias_name;
            scp = &aliased_sym->variant.routine.ptr->source_corresp;
            break;
          case sk_variable:
            aliased_sym->variant.variable.ptr->asm_name_or_reg.name =
                                                          entry->alias_name;
            scp = &aliased_sym->variant.variable.ptr->source_corresp;
            break;
          case sk_overloaded_function:
            pos_sy_error(ec_bad_linkage_for_redefine_extname, pos,
                         aliased_sym);
            break;
          default:
            break;
        }  /* switch */
        if (scp != NULL &&
            scp->name_linkage != (a_name_linkage_kind)nlk_external &&
            !(aliased_sym->kind == (a_symbol_kind)sk_variable &&
              !scp_is_namespace_member(scp))) {
          /* #pragma redefine_extname applies to mangled names, but currently
             we can only look up the declared name.  We therefore only allow
             the pragma when both names are identical; i.e., for extern "C"
             entities, and for variables in global scope. */
          pos_sy_error(ec_bad_linkage_for_redefine_extname, pos, aliased_sym);
        }  /* if */
      }  /* if */
#else /* REDEFINE_EXTNAME_PRAGMA_ENABLED */
      unexpected_condition();
#endif /* REDEFINE_EXTNAME_PRAGMA_ENABLED */
#if GNU_EXTENSIONS_ALLOWED
    } else if (!is_aliasable(aliased_sym, entry->alias)) {
      /* The aliased entity was not declared in this translation unit (i.e.,
         aliased_sym is NULL), or no direct alias can be recorded for some
         other reason (e.g., the "alias" attribute requires that aliased_sym
         have a definition or be an alias itself).  GCC versions prior to 4.0
         (on Intel platforms) treat this as an alternative way to specify the
         asm name of the alias.  Newer GCC versions treat it as an error (as
         do earlier versions on some non-Intel platforms).  We issue an error
         when emulating newer GCC versions, and a warning otherwise (a back
         end can still obtain the name of the alias from the attribute entry).
         No diagnostic is issued if the alias is for a "weakref" attribute. */
      a_boolean  is_weakref = FALSE;
      if (early_attr_resolution) goto next_entry;
      switch (entry->alias->kind) {
        case sk_routine:
          is_weakref = entry->alias->variant.routine.ptr->is_weakref;
          break;
        case sk_variable:
          is_weakref = entry->alias->variant.variable.ptr->is_weakref;
          break;
        default:
          /* Can happen for member functions with ifunc attributes when
             the resolver isn't found (which is likely if a nested mangled
             name is specified). */
          check_assertion(entry->alias->variant.routine.ptr->is_ifunc &&
                          is_function_or_template_symbol(entry->alias));
      }  /* switch */
      if (!is_weakref) {
        pos_st_diagnostic(gnu_version < 40000 ? es_warning
                                              : es_discretionary_error,
                          ec_aliased_name_undeclared,
                          &entry->alias_position, entry->aliased_name);
      }  /* if */
    } else if (aliased_sym->kind != entry->alias->kind) {
      if (early_attr_resolution) goto next_entry;
      pos_sy_error(ec_aliased_name_bad_kind,
                   &entry->alias->decl_position, aliased_sym);
    } else if ((entry->alias->kind == (a_symbol_kind)sk_routine &&
                in_secondary_trans_unit(entry->alias->variant.routine.ptr) !=
                in_secondary_trans_unit(aliased_sym->variant.routine.ptr)) ||
               (entry->alias->kind == (a_symbol_kind)sk_variable &&
                in_secondary_trans_unit(entry->alias->variant.variable.ptr) !=
                in_secondary_trans_unit(aliased_sym->variant.variable.ptr))) {
      /* We've found a match, but it's in another translation unit.  Put this
         entry back on the list to revisit later. */
      keep_entry = TRUE;
    } else {
      /* Usual case: An entity declared in this translation unit is aliased
         using the GNU "alias" (or "weakref" or "ifunc") attribute. */
      switch (entry->alias->kind) {
        case sk_routine:
          if (entry->alias->variant.routine.ptr->is_ifunc) {
            /* Give a warning if the resolver routine doesn't have the
               correct type; it should take no arguments and return
               a function pointer. */
            a_type_ptr routine_type = aliased_sym->variant.routine.ptr->type;
            a_type_ptr return_type = routine_type->variant.routine.return_type;
            a_routine_type_supplement_ptr
                       rtsp = routine_type->variant.routine.extra_info;
            if (!is_pointer_to_function_type(return_type) ||
                rtsp->param_type_list != NULL) {
              if (early_attr_resolution) goto next_entry;
              pos_syty_warning(ec_incompatible_ifunc_resolver_type,
                               &entry->alias_position, aliased_sym,
                               routine_type);
            }  /* if */
          }  /* if */
          ensure_gnu_routine_supp(entry->alias->variant.routine.ptr)->
                            aliased_routine = aliased_sym->variant.routine.ptr;
          report_any_alias_loop(entry);
          break;
        case sk_variable:
          entry->alias->variant.variable.ptr->aliased_variable =
                                             aliased_sym->variant.variable.ptr;
          report_any_alias_loop(entry);
          break;
        default:
          unexpected_condition();
      }  /* switch */
      /* Applying an alias attribute may make the alias the preferred symbol
         associated with its asm name (if any): Call record_asm_name_for_lookup
         to update asm_name_map if needed. */
      record_asm_name_for_lookup(entry->alias);
      /* The aliased entity is referenced ("used") in the alias specification;
         only now, however, do we know the symbol to mark it as used. */
      record_symbol_reference(SRK_USE | SRK_REFERENCE, aliased_sym,
                              &entry->alias->decl_position,
                              /*update_il_entry=*/TRUE);
      keep_entry = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
    }  /* if */
next_entry:
    if (keep_entry) {
      if (alias_fixup_list == NULL) {
        alias_fixup_list = entry;
      } else {
        last_alias_fixup->next = entry;
      }  /* if */
      last_alias_fixup = entry;
      entry->next = NULL;
    } else {
      free_alias_fixup(entry);
    }  /* if */
  }  /* while */
}  /* process_alias_fixup_list */

#if REDEFINE_EXTNAME_PRAGMA_ENABLED

#if DEBUG
STATIC_THREAD unsigned long
		pragma_extname_string_space;
#endif /* DEBUG */

void redefine_extname_pragma(a_pending_pragma_ptr  ppp)
/*
Process the GNU/Solaris redefine_extname pragma by recording an appropriate
alias fixup entry.  Such fixup entries are applied at a later time by
process_alias_fixup_list.
*/
{
  a_const_char *src_name = NULL, *asm_name = NULL;
  sizeof_t     src_name_len = 0, asm_name_len = 0;
  a_boolean    err = FALSE;

  begin_rescan_of_pragma_tokens(ppp);
  if (curr_token == tok_identifier) {
    src_name = locator_for_curr_id.symbol_header->identifier;
    src_name_len = locator_for_curr_id.symbol_header->identifier_length;
    (void)get_token();
    if (curr_token == tok_identifier) {
      asm_name = locator_for_curr_id.symbol_header->identifier;
      asm_name_len = locator_for_curr_id.symbol_header->identifier_length;
      (void)get_token();
    } else {
      err = TRUE;
      pos_error(ec_exp_identifier, &error_position);
    }  /* if */
  } else {
    err = TRUE;
    pos_error(ec_exp_identifier, &error_position);
  }  /* if */
  wrapup_rescan_of_pragma_tokens(err);
  if (!err) {
    sizeof_t  prefix_len = sizeof("redefine_extname ")-1;
    sizeof_t  pragma_len = prefix_len+src_name_len+1+asm_name_len+1;
    check_assertion(asm_name != NULL);
    add_alias_fixup((a_symbol_ptr)NULL,
                    copy_string_to_region(file_scope_region_number, asm_name),
                    src_name,
                    &ppp->pragma_position);
    /* Recreate the pragma string: "redefine_extname <src-name> <asm-name>". */
    ppp->pragma_text  = (char *)alloc_primary_file_scope_il(pragma_len);
#if DEBUG
    pragma_extname_string_space += pragma_len;
#endif /* DEBUG */
    /*lint -e(668)*/(void)memcpy(ppp->pragma_text, "redefine_extname ",
                                 size_t_arg(prefix_len));
    /*lint -e(668)*/(void)memcpy(ppp->pragma_text+prefix_len, src_name,
                                 size_t_arg(src_name_len));
    ppp->pragma_text[prefix_len+src_name_len] = ' ';
    /*lint -e(668)*/(void)memcpy(ppp->pragma_text+prefix_len+src_name_len+1,
                                 asm_name, size_t_arg(asm_name_len+1));
    /* Record the pragma in the IL. */
    create_il_entry_for_pragma(ppp, (a_symbol_ptr)NULL, (a_statement_ptr)NULL);
  }  /* if */
}  /* redefine_extname_pragma */

#endif /* REDEFINE_EXTNAME_PRAGMA_ENABLED */

#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */
#if GNU_EXTENSIONS_ALLOWED

a_type_ptr copy_gnu_type_properties(a_type_ptr  dst,
                                    a_type_ptr  src)
/*
Copy any GNU type properties (set by attributes) in type dst to type src.
*/
{
  a_type_ptr        result = dst;
  a_boolean         src_may_alias = FALSE, dst_may_alias = FALSE;
  an_attribute_ptr  may_alias_ap = NULL, ap;

  /* Skip any typerefs, but record any properties they embed. */
  while (src->kind == (a_type_kind)tk_typeref) {
    if (src->may_alias && !src_may_alias) {
      src_may_alias = TRUE;
      may_alias_ap = find_attribute(ak_may_alias,
                                    src->source_corresp.attributes);
      check_assertion(may_alias_ap != NULL);
    }  /* if */
    src = src->variant.typeref.type;
  }  /* while */
  while (dst->kind == (a_type_kind)tk_typeref) {
    if (dst->may_alias) dst_may_alias = TRUE;
    dst = dst->variant.typeref.type;
  }  /* while */
  if (dst == src) {
    /* Nothing to be done. */
  } else {
    check_assertion(dst->kind == src->kind);
    switch (src->kind) {
      case tk_routine:
        { a_routine_type_supplement_ptr src_rtsp, dst_rtsp;
          src_rtsp = src->variant.routine.extra_info;
          dst_rtsp = dst->variant.routine.extra_info;
          if (src->alignment_set_explicitly &&
              src->alignment > dst->alignment) {
            dst->alignment = src->alignment;
            dst->alignment_set_explicitly = TRUE;
          }  /* if */
#if GNU_X86_ATTRIBUTES_ALLOWED
          if (src_rtsp->calling_convention !=
                                           (a_calling_convention)cc_default &&
              dst_rtsp->calling_convention !=
                                           (a_calling_convention)cc_stdcall) {
            dst_rtsp->calling_convention = src_rtsp->calling_convention;
            dst_rtsp->explicit_calling_convention =
                                        src_rtsp->explicit_calling_convention;
          }  /* if */
#endif /* GNU_X86_ATTRIBUTES_ALLOWED */
          if (src_rtsp->is_const) {
            dst_rtsp->is_const = TRUE;
          }  /* if */
          if (src_rtsp->result_should_be_used) {
            dst_rtsp->result_should_be_used = TRUE;
          }  /* if */
          if (src_rtsp->arg_pragma != (a_pragma_kind)pk_none) {
            dst_rtsp->arg_pragma = src_rtsp->arg_pragma;
            dst_rtsp->fmt_arg = src_rtsp->fmt_arg;
            dst_rtsp->format_first_subst_arg= src_rtsp->format_first_subst_arg;
          }  /* if */
          if (src_rtsp->prototyped && dst_rtsp->prototyped) {
            /* Copy any "nonnull" attributes. */
            a_param_type_ptr  src_ptp = src_rtsp->param_type_list;
            a_param_type_ptr  dst_ptp = dst_rtsp->param_type_list;
            while (src_ptp != NULL) {
              check_assertion(dst_ptp != NULL);
              if (src_ptp->nonnull) dst_ptp->nonnull = TRUE;
              src_ptp = src_ptp->next;
              dst_ptp = dst_ptp->next;
            }  /* while */
          }  /* if */
          if (src->may_alias) dst->may_alias = TRUE;
          /* Update the result since a skip_typerefs was applied to dst. */
          result = dst;
        }
        break;
      default:
        /* No properties to copy. */
        break;
    }  /* switch */
  }  /* if */
  if (src_may_alias && !dst_may_alias && !result->may_alias) {
    /* Copy over the "may_alias" attribute. */
    ap =  alloc_attribute();
    *ap = *may_alias_ap;
    ap->next = NULL;
    attach_type_attributes(&result, ap, NULL);
  }  /* if */
  return result;
}  /* copy_gnu_type_properties */

#endif /* GNU_EXTENSIONS_ALLOWED */

/*
A dummy attribute used solely for the processing of attribute_is_supported.
*/
STATIC_THREAD an_attribute_ptr
                dummy_attr;

a_boolean attribute_is_supported(a_const_char        *name,
                                 a_const_char        *namespace_name,
                                 an_attribute_family family)
/*
Return TRUE if name (and namespace_name, if non-NULL) designate an
attribute of the specified family that is enabled in the current execution
of the front end, FALSE otherwise.  Passing af_internal or af_has_attribute
as the value of family indicates that an attribute in any family is permitted,
with standard attributes given preference.
*/
{
  an_attr_name_map_entry_ptr ep;
  an_attr_name_map_entry_ptr *p_ep;
  a_boolean                  supported = FALSE;

  p_ep = lookup_attribute_name(name, family);
  if (p_ep != NULL) {
    check_assertion(*p_ep != NULL);
    if (dummy_attr == NULL) {
      /* Allocate a dummy attribute for matching. */
      dummy_attr = alloc_attribute();
    }  /* if */
    dummy_attr->family = family;
    dummy_attr->name = name;
    dummy_attr->namespace_name = namespace_name;
    /* Scan through the attributes with this name to see if one meets the
       criteria of the current emulation mode and version. */
    for (ep = *p_ep; !supported && ep != NULL; ep = ep->next) {
      /* Get the condition string for the current attribute. */
      a_const_char *cond = ep->descr->cond;
      if (*cond == '1') {
        /* Skip a leading "1" (which indicates that the attribute should
           appear at most once in a group). */
        ++cond;
      }  /* if */
      switch (family) {
        case af_internal:
        case af_has_attribute:
          supported = (cond_matches_std_attr_mode(cond, dummy_attr) ||
                       cond_matches_gnu_attr_mode(cond, dummy_attr) ||
                       cond_matches_ms_declspec_mode(cond, dummy_attr));
          break;
        case af_gnu:
          supported = cond_matches_gnu_attr_mode(cond, dummy_attr);
          break;
        case af_std:
          supported = cond_matches_std_attr_mode(cond, dummy_attr);
          break;
        case af_ms_declspec:
          supported = cond_matches_ms_declspec_mode(cond, dummy_attr);
          break;
        default:
          unexpected_condition();
      }  /* if */
    }  /* for */
  }  /* if */
  return supported;
}  /* attribute_is_supported */


a_boolean attribute_is_template_dependent(an_attribute_ptr ap)
/*
Returns TRUE if any of the arguments to the attribute are template-dependent.
*/
{
  an_attribute_arg_ptr  aap;
  a_boolean             result = FALSE;

  for (aap = ap->arguments; aap != NULL; aap = aap->next) {
    if ((aap->kind == aak_constant &&
         aap->variant.constant->kind == ck_template_param) ||
        (aap->kind == aak_type &&
         is_template_dependent_type(aap->variant.type)) ||
        (aap->kind == aak_expression &&
         is_template_dependent_type(expr_node_from_attribute_arg(aap)->type))){
      result = TRUE;
      break;
    }  /* if */
  }  /* if */
  return result;
}  /* attribute_is_template_dependent */


static a_host_large_integer get_strictest_alignment_value(
                                                         an_attribute_ptr *oap)
/*
Return the value of the strictest alignment attribute in the list beginning
with *oap.  Alignment values can be constants or types (in which case the
alignment of the type is used).  For template-dependent alignment arguments,
return MAX_HOST_LARGE_INTEGER.  *oap is set to the alignment attribute with the
strictest alignment.
*/
{
  a_host_large_integer  alignment, max = 0;
  an_attribute_arg_ptr  aap;
  an_attribute_ptr      ap;

  for (ap = *oap; ap != NULL; ap = ap->next) {
    if (ap->kind == ak_align) {
      if (attribute_is_template_dependent(ap)) {
        /* A dependent alignment can have any value; assume it is the
           strictest. */
        alignment = MAX_HOST_LARGE_INTEGER;
      } else {
        aap = ap->arguments;
        alignment = 0;
        switch (aap->kind) {
          case aak_constant:
            (void)get_attr_arg_integer(aap, ap, (a_host_large_integer)0,
                                       MAX_HOST_LARGE_INTEGER, &alignment);
            break;
          case aak_type:
            alignment = aap->variant.type->alignment;
            break;
          default:
            unexpected_condition();
        }  /* switch */
      }  /* if */
      if (alignment > max) {
        max = alignment;
        *oap = ap;
      }  /* if */
    }  /* if */
  }  /* for */
  return max;
}  /* get_strictest_alignment_value */


static void equivalent_align_attribute(an_attribute_ptr  old_ap,
                                       an_attribute_ptr  new_ap,
                                       a_source_position *def_pos,
                                       a_boolean         is_definition)

/*
Issue a diagnostic if alignment attributes from two declarations are not
equivalent.  The position for the diagnostic, if any, is given by *def_pos.
is_definition is TRUE if the new attribute (new_ap) appears on a definition.
*/
{
  a_diag_list       diag_list;
  a_diagnostic_ptr  dp;

  if (is_definition) {
    if (new_ap == NULL) {
      /* If a previous declaration specified an alignment but the
         definition does not, give an error. */
      clear_diag_list(&diag_list);
      dp = pos_start_error(ec_no_alignment_on_definition, def_pos);
      more_info_diagnostic(ec_attribute_declared_here, &old_ap->position,
                           &diag_list);
      add_more_info_list(dp, &diag_list);
      end_diagnostic(dp);
    } else {
      /* There may be multiple alignment attributes on both the prior
         declaration as well as the current one.  Look for the strictest
         alignment on both lists and compare those. */
      a_host_large_integer old_align = get_strictest_alignment_value(&old_ap);
      a_host_large_integer new_align = get_strictest_alignment_value(&new_ap);
      /* If a previous declaration specified an alignment, it must
         be effectively the same as the alignment specified on the
         definition.  That can be hard to diagnose on a prototype
         instantiation so just flag the obvious cases here. */
      if (equivalent_attributes(old_ap, new_ap,
                                       /*ignore_family=*/FALSE)) {
        /* Same arguments.  Leave the lists as they are. */
      } else {
        a_boolean err = FALSE;
        if (attribute_is_template_dependent(old_ap) ||
            attribute_is_template_dependent(new_ap)) {
          /* Assume a template-dependent (or value-dependent) type could
             match. */
        } else {
          /* Alignment arguments are not the same (but they may still have
             equivalent values). */
          if (old_align != new_align) {
            clear_diag_list(&diag_list);
            dp = pos_start_error(ec_align_not_equivalent, &new_ap->position);
            more_info_diagnostic(ec_attribute_declared_here, &old_ap->position,
                                 &diag_list);
            add_more_info_list(dp, &diag_list);
            end_diagnostic(dp);
            err = TRUE;
          }  /* if */
        }  /* if */
        if (!err) {
          /* The two lists are equivalent (or, more likely, template-
             dependent), but not identical.  In this case, replace the
             alignment attributes in the declaration with those in the
             definition (leaving other attributes as they were).  This is
             accomplished by marking them unrecognized (the new attributes will
             be added as part of the normal processing). */
          do {
            if (old_ap->kind == ak_align) {
              make_attr_unrecognized(old_ap);
            }  /* if */
            old_ap = old_ap->next;
          } while (old_ap != NULL);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* equivalent_align_attribute */


void apply_attributes_to_prototype_instantiation(
                                an_attribute_ptr                 new_list,
                                a_template_symbol_supplement_ptr tssp,
                                a_source_position                *def_pos,
                                a_boolean                        is_definition)
/*
This routine applies the set of attributes in new_list to the class template
specified by tssp.  The source position (for diagnostics) of the template
declaration is given by *def_pos.  is_definition is TRUE if the template being
declared is also a definition.

For non-template class types, attributes are applied immediately, and
re-checked at each redeclaration, but for the template case, attributes
applied to class templates are basically cached (in tssp->attributes) until an
instantiation occurs, at which time they are applied to the resulting class
type.  This routine examines attributes (if any) on an existing declaration
and reconciles them with the set of attributes specified on the current
declaration.  Diagnostics are issued where applicable, and any applicable
attributes from new_list are applied to tssp->attributes.
*/
{
  an_attribute_ptr  new_ap, old_ap, *app, next_ap;
  an_attribute_ptr  added_tail = NULL, added_head = NULL;
  a_boolean         align_processed = FALSE;

  /* First, look at all attributes that are already attached to the prototype
     instantiation and see if there is an attribute of the same kind in
     the new list. */
  for (app = &tssp->attributes; *app != NULL; app = &(*app)->next) {
    old_ap = *app;
    if (is_std_attribute(old_ap) || old_ap->kind == ak_align) {
      switch (old_ap->kind) {
        case ak_align:
          /* Issue an error if the alignment attribute(s) are not equivalent
             to a prior declaration. */
          if (!align_processed) {
            new_ap = find_attribute(ak_align, new_list);
            equivalent_align_attribute(old_ap, new_ap, def_pos, is_definition);
            /* The call to equivalent_align_attribute above has processed all
               applicable ak_align attributes, so there's no need to do the
               check for further ak_align attributes (but don't exit the loop
               in case there are other attribute kinds). */
             align_processed = TRUE;
           }  /* if */
          break;
        case ak_deprecated:
        case ak_unavailable:
        case ak_maybe_unused:
          /* Declarations and definitions do not need to match, but if the
             attribute appears anywhere, it applies to the entity.  In this
             case the attribute is already attached to the prototype
             instantiation so no action is needed. */
          break;
        case ak_nodiscard:
          /* The standard doesn't specify what to do here. */
          break;
        case ak_unrecognized:
          /* Probably an error from earlier; ignore it here. */
          break;
        case ak_carries_dependency:
        case ak_noreturn:
        case ak_fallthrough:
        case ak_likely:
        case ak_unlikely:
        case ak_no_unique_address:
        default:
          /* These do not appertain to class types (and an error will be
             emitted later).  Ignore them here. */
          break;
      }  /* switch */
    }  /* if */
  }  /* for */
  /* app now points to the last entry in the old attribute list (and any
     new attributes will be appended). */
  /* Now make a pass through the "new" attribute list to see if requirements
     have been met. */
  for (new_ap = new_list; new_ap != NULL; new_ap = next_ap) {
    a_boolean add = FALSE;
    next_ap = new_ap->next;
    new_ap->next = NULL;
    old_ap = find_attribute(new_ap->kind, tssp->attributes);
    switch (new_ap->kind) {
      case ak_align:
        if (old_ap == NULL) {
          /* No alignment on a previous declaration is okay. */
          add = TRUE;
        } else {
          /* If a previous declaration had an alignment, the standard
             requires that the two be equivalent.  Note that equivalency was
             checked in the first loop, so no need to do it again here.
             The original attribute is kept (and the new one discarded). */
        }  /* if */
        break;
      case ak_deprecated:
      case ak_unavailable:
      case ak_maybe_unused:
        /* Declarations and definitions do not need to match, but if the
           attribute appears anywhere, it applies to the entity.  Add it
           if it's not already there. */
        if (old_ap == NULL) {
          add = TRUE;
        }  /* if */
        break;
      case ak_nodiscard:
      default:
        /* Unspecified; add it if it's not already there. */
        if (old_ap == NULL) {
          add = TRUE;
        }  /* if */
        break;
      case ak_unrecognized:
        /* Probably an error from earlier; ignore it here. */
        break;
      case ak_carries_dependency:
      case ak_noreturn:
      case ak_fallthrough:
      case ak_likely:
      case ak_unlikely:
      case ak_no_unique_address:
        /* These do not appertain to class types (and an error will be
           emitted later).  Add them (so an error will be issued). */
        add = TRUE;
        break;
    }  /* switch */
    if (add) {
      /* Add the new attribute to the list of attributes for this prototype
         instantiation (keep the list separate until the end of this
         function). */
      if (added_head == NULL) {
        added_head = new_ap;
      } else {
        check_assertion(added_tail != NULL);
        added_tail->next = new_ap;
      }  /* if */
      added_tail = new_ap;
    }  /* if */
  }  /* for */
  if (added_head != NULL) {
    /* Some attributes need to be added to the original list. */
    if (*app == NULL) {
      *app = added_head;
    } else {
      (*app)->next = added_head;
    }  /* if */
  }  /* if */
}  /* apply_attributes_to_prototype_instantiation */


void attribute_one_time_init(void)
/*
Do one-time initialization of variables related to the processing of
attributes.
*/
{
  init_attr_name_map();
  /* Save variables from attribute.h and attribute.c that are needed for
     precompiled headers */
  if (precompiled_header_processing_required) {
    STATIC_THREAD a_pch_saved_variable saved_vars[] = {
#if GNU_EXTENSIONS_ALLOWED
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
      pch_saved_var_array_elem(ELF_visibility_stack),
      pch_saved_var_array_elem(avail_ELF_visibility_stack_entries),
#if DEBUG
      pch_saved_var_array_elem(num_ELF_visibility_stack_entries_allocated),
#endif /* DEBUG */
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
      pch_saved_var_array_elem(asm_name_map),
      pch_saved_var_array_elem(gnu_abi_tag_attribute_seen),
#endif /* GNU_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
      pch_saved_var_array_elem(alias_fixup_list),
      pch_saved_var_array_elem(last_alias_fixup),
      pch_saved_var_array_elem(avail_alias_fixups),
#if DEBUG
      pch_saved_var_array_elem(num_alias_fixups_allocated),
#if REDEFINE_EXTNAME_PRAGMA_ENABLED
      pch_saved_var_array_elem(pragma_extname_string_space),
#endif /* REDEFINE_EXTNAME_PRAGMA_ENABLED */
#endif /* DEBUG */
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */
#if USE_X86_FUNCTION_MULTIVERSIONING && DO_IL_LOWERING
      pch_saved_var_array_elem(mv_builtins_loaded),
#endif /* USE_X86_FUNCTION_MULTIVERSIONING && DO_IL_LOWERING */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  register_trans_unit_variable(asm_name_map);
#endif /* GNU_EXTENSIONS_ALLOWED */
  register_trans_unit_variable(unscanned_attributes);
  register_trans_unit_variable(already_diagnosed_using);
}  /* attribute_one_time_init */


void attribute_trans_unit_init(void)
/*
Initialize variables related to GNU attributes that are specific to a given
translation unit.
*/
{
#if GNU_EXTENSIONS_ALLOWED
  asm_name_map = alloc_hash_table(FRONT_END_REGION_NUMBER,
                                  (a_hash_table_size)1000,
                                  fn_for_function(hash_source_string),
                                  fn_for_function(compare_for_asm_name_map));
#endif /* GNU_EXTENSIONS_ALLOWED */
  unscanned_attributes = NULL;
#if USE_X86_FUNCTION_MULTIVERSIONING && DO_IL_LOWERING
  mv_builtins_loaded = FALSE;
#endif /* USE_X86_FUNCTION_MULTIVERSIONING && DO_IL_LOWERING */
  already_diagnosed_using = FALSE;
}  /* attribute_trans_unit_init */


void attribute_init(void)
/*
Initialize static variables related to attribute processing that must
be initialized for each compilation.
*/
{
#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
#if GNU_EXTENSIONS_ALLOWED
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  ELF_visibility_stack = NULL;
  avail_ELF_visibility_stack_entries = NULL;
#if DEBUG
  num_ELF_visibility_stack_entries_allocated = 0;
#endif /* DEBUG */
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
  gnu_abi_tag_attribute_seen = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
  avail_alias_fixups = NULL;
  alias_fixup_list = NULL;
  last_alias_fixup = NULL;
#if DEBUG
  num_alias_fixups_allocated = 0;
#if REDEFINE_EXTNAME_PRAGMA_ENABLED
  pragma_extname_string_space = 0;
#endif /* REDEFINE_EXTNAME_PRAGMA_ENABLED */
#endif /* DEBUG */
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */
  memzero((char*)attr_family_seen, sizeof(attr_family_seen));
  attr_corresp_checking_map = NULL;
  dummy_attr = NULL;
}  /* attribute_init */

#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
#if DEBUG

unsigned long show_attribute_space_used(void)
/*
Display and return the amount of space used for various GNU attribute-related
entities.
*/
{
  unsigned long grand_total = 0;
  unsigned long num, size, total;

  db_space_used_header("GNU attributes use:");
#if GNU_EXTENSIONS_ALLOWED
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  db_space_used_lost("GNU visibility stack",
                     avail_ELF_visibility_stack_entries,
                     num_ELF_visibility_stack_entries_allocated,
                     an_ELF_visibility_stack_entry);
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
#endif /* GNU_EXTENSIONS_ALLOWED */
  db_space_used("alias fixups", num_alias_fixups_allocated, an_alias_fixup);
#if REDEFINE_EXTNAME_PRAGMA_ENABLED
  db_space_used("pragma extname strings", pragma_extname_string_space, char);
#endif /* REDEFINE_EXTNAME_PRAGMA_ENABLED */
  return grand_total;
}  /* show_attribute_space_used */

#endif /* DEBUG */

#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

