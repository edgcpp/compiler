/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*
It must be possible to include this file more than once, so it intentionally
does not have an include guard.
*/

/*

walk_entry.h -- Routines used by il_walk.c to walk IL entries.

When #include'd, this file produces definitions for one or two functions.
The first (non-optional) function will be defined with a declarator-id that is
the expansion of the macro WALK_ENTRY_ROUTINE_NAME.  The second is always a
"static" function and declared with a declarator-id that is the expansion of
the macro WALK_ORPHANED_ENTRY_ROUTINE_NAME (but if that macro is not defined,
no such function is produced).  Note that if the macros specifying the names
are qualified names (e.g., "edg::some_name"), a declaration must have been
provided earlier in the appropriate scope.

Various macros control how the routines traverse a given IL entry:

1)  With DO_SUBTREE_WALK TRUE, the routines walk not only the entry itself
    but also its subtree.

2)  With DO_SUBTREE_WALK and NEEDED_FLAG_WALK TRUE, the routines walk
    the subtree and do so in the special way required for setting the
    needed flag.  Entries related to the following are not walked
    (because they do not affect whether the entities are needed to
    create an executable program):
      -- Access control (including friendship)
      -- "using" declarations and directives
      -- The hidden name table
      -- Source sequence entries
      -- The based types list
    Also, some back-pointers are not walked if they introduce cycles
    in the data structure where no entry in the cycle has a "needed"
    flag, since such cycles would cause recursion loops in this walk.

3)  With DO_SUBTREE_WALK and KEEP_IN_IL_WALK TRUE, the routines walk
    the subtree and do so in the special way required for setting the
    keep_in_il flag.

4)  With DO_SUBTREE_WALK FALSE, the routines walk just the entry itself.
    This is used for remapping of pointers.
  
*/

#if !STANDALONE_UTILITY_PROGRAM && DO_IL_LOWERING
/* We need pm_class_type_possibly_lowered. */
#include "lower_il.h"
#else /* !(!STANDALONE_UTILITY_PROGRAM && DO_IL_LOWERING) */
/* No lowering, so use pm_class_type for pm_class_type_possibly_lowered. */
#undef pm_class_type_possibly_lowered
#define pm_class_type_possibly_lowered(x) pm_class_type(x)
#endif /* !STANDALONE_UTILITY_PROGRAM && DO_IL_LOWERING */

/*
Macro to remap a pointer from an "old" value to a "new" value.  ptr is
the pointer, ptr_type the type of ptr, and entry_kind is the kind of entry
pointed to.  For the NEEDED_FLAG_WALK and KEEP_IN_IL_WALK cases, just
walks the pointer.

This macro is used for secondary references to IL entities, e.g., a
reference from executable code to a declarative entry, where it is known
that the declarative entry will be reached from a primary pointer while
walking the declarative structure.  When in doubt, a walk_ptr is the safer
choice; it may be unnecessarily slow, but it will work correctly even if
the entry is not reached from elsewhere.
*/
#undef remap_ptr
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
/* When doing the IL walk to set the "needed" or "keep_in_il" flags, all
   references are significant and must be followed. */
#define remap_ptr(ptr, ptr_type, entry_kind) \
  walk_ptr(ptr, ptr_type, entry_kind)
#else /* !(NEEDED_FLAG_WALK || KEEP_IN_IL_WALK) */
#define remap_ptr(ptr, ptr_type, entry_kind) \
{ if (walk_remap_func != NULL) { \
    (ptr) = (ptr_type)walk_remap_func((char *)(ptr), (entry_kind)); \
  }  /* if */ \
}  /* remap_ptr */
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */

/*
Like remap_ptr, but used for pointers in lists, i.e., "next" pointers
and start-of-list pointers.
*/
#undef remap_list_ptr
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
/* When doing the IL walk to set the "needed" or "keep_in_il" flags, all
   references are significant and must be followed. */
#define remap_list_ptr(ptr, ptr_type, entry_kind) \
  walk_list_ptr(ptr, ptr_type, entry_kind)
#else /* !(NEEDED_FLAG_WALK && ...) */
#define remap_list_ptr(ptr, ptr_type, entry_kind) \
{ if (walk_list_remap_func != NULL) { \
    (ptr) = (ptr_type)walk_list_remap_func((char *)(ptr), (entry_kind)); \
  }  /* if */ \
}  /* remap_list_ptr */
#endif /* NEEDED_FLAG_WALK && ... */

/*
Like remap_ptr, but used for "next" pointers in entries.  These are
remapped only if not processing subtrees (if subtrees are being processed,
walk_list handles the remapping when doing the parent of this entry).
*/
#undef remap_next_ptr
#if DO_SUBTREE_WALK
#define remap_next_ptr(ptr, ptr_type, entry_kind) /* Nothing */
#else /* !DO_SUBTREE_WALK */
#define remap_next_ptr(ptr, ptr_type, entry_kind) \
  remap_list_ptr((ptr), ptr_type, (entry_kind))
#endif /* DO_SUBTREE_WALK */

/*
Similar to remap_ptr, but expands to nothing in the NEEDED_FLAG_WALK mode.
*/
#undef remap_ptr_not_needed
#if NEEDED_FLAG_WALK
#define remap_ptr_not_needed(ptr, ptr_type, entry_kind) /* Nothing */
#else /* !NEEDED_FLAG_WALK */
#define remap_ptr_not_needed(ptr, ptr_type, entry_kind) \
  remap_ptr(ptr, ptr_type, entry_kind)
#endif /* NEEDED_FLAG_WALK */

/*
Macro to remap a pointer to its new value, walk the subtree of the pointer
(if appropriate), and process the entry pointed to.  ptr is the pointer,
ptr_type is the type of ptr, and entry_kind is the kind of entry pointed to.
*/
#undef walk_ptr
#if DO_SUBTREE_WALK
#if NEEDED_FLAG_WALK
/* Walking to set the "needed" flag. */
#define walk_ptr(ptr, ptr_type, entry_kind) \
{ if ((ptr) != NULL) walk_tree_and_set_needed((char *)(ptr), (entry_kind)); }
#else /* !NEEDED_FLAG_WALK */
#if KEEP_IN_IL_WALK
/* Walking to set the "keep_in_il" flag. */
#define walk_ptr(ptr, ptr_type, entry_kind) \
{ if ((ptr) != NULL) { \
    walk_tree_and_set_keep_in_il((char *)(ptr), (entry_kind)); \
  }  /* if */ \
}
#else /* !KEEP_IN_IL_WALK */
#define walk_ptr(ptr, ptr_type, entry_kind) \
{ remap_ptr((ptr), ptr_type, (entry_kind)); \
  if ((ptr) != NULL) walk_entry_and_subtree((char *)(ptr), (entry_kind)); \
}  /* walk_ptr */
#endif /* KEEP_IN_IL_WALK */
#endif /* NEEDED_FLAG_WALK */
#else /* !DO_SUBTREE_WALK */
#define walk_ptr(ptr, ptr_type, entry_kind) \
  remap_ptr((ptr), ptr_type, (entry_kind))
#endif /* DO_SUBTREE_WALK */

/*
Like walk_ptr, but used for "next" pointers in entries where a full subtree
walk needs to be performed.  It can only be used on a single pointer per entry
with the same entry_kind.  Note that the entire purpose of this routine is
to use iteration rather than recursion to perform a walk of "next" pointers
(to use less stack space).
*/
#undef walk_next_ptr
#if DO_SUBTREE_WALK
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
#define walk_next_ptr(ptr, ptr_type, entry_kind) \
{ next_entry_ptr = (char *)ptr; }
#else /* !(NEEDED_FLAG_WALK || KEEP_IN_IL_WALK) */
#define walk_next_ptr(ptr, ptr_type, entry_kind) \
{ remap_ptr((ptr), ptr_type, (entry_kind)); \
  next_entry_ptr = (char *)ptr; \
}  /* walk_next_ptr */
#endif /* !(NEEDED_FLAG_WALK || KEEP_IN_IL_WALK) */
#else /* !DO_SUBTREE_WALK */
#define walk_next_ptr(ptr, ptr_type, entry_kind) \
{ remap_ptr((ptr), ptr_type, (entry_kind)); }
#endif /* DO_SUBTREE_WALK */

/*
Like walk_ptr, but used for pointers in lists, i.e., "next" pointers
and start-of-list pointers.
*/
#undef walk_list_ptr
#if DO_SUBTREE_WALK
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
/* Walking to set the "needed" or "keep_in_il" flag.  Same as walk_ptr. */
#define walk_list_ptr(ptr, ptr_type, entry_kind) \
  walk_ptr((ptr), ptr_type, (entry_kind))
#else /* !(NEEDED_FLAG_WALK || KEEP_IN_IL_WALK) */
#define walk_list_ptr(ptr, ptr_type, entry_kind) \
{ remap_list_ptr((ptr), ptr_type, (entry_kind)); \
  if ((ptr) != NULL) walk_entry_and_subtree((char *)(ptr), (entry_kind)); \
}  /* walk_list_ptr */
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
#else /* !DO_SUBTREE_WALK */
#define walk_list_ptr(ptr, ptr_type, entry_kind) \
  remap_list_ptr((ptr), ptr_type, (entry_kind))
#endif /* DO_SUBTREE_WALK */

/*
Similar to walk_ptr, but expands to nothing in the NEEDED_FLAG_WALK mode.
*/
#undef walk_ptr_not_needed
#if NEEDED_FLAG_WALK
#define walk_ptr_not_needed(ptr, ptr_type, entry_kind) /* Nothing */
#else /* !NEEDED_FLAG_WALK */
#define walk_ptr_not_needed(ptr, ptr_type, entry_kind) \
  walk_ptr(ptr, ptr_type, entry_kind)
#endif /* NEEDED_FLAG_WALK */

/*
Macro similar to walk_ptr, but used for string entries.  ptr is the pointer
to the entry, entry_kind is the kind of entry pointed to, and entry_length is
the string length for iek_string_text entries, unused otherwise.
*/
#undef walk_string_ptr
#if DO_SUBTREE_WALK
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
#define walk_string_ptr(ptr, entry_kind, entry_length) /* Nothing. */
#else /* !(NEEDED_FLAG_WALK && ...) */
#define walk_string_ptr(ptr, entry_kind, entry_length) \
{ remap_ptr((ptr), a_char_ptr, (entry_kind)); \
  walk_string_entry((char *)(ptr), (entry_kind), (sizeof_t)(entry_length)); \
}  /* walk_string_ptr */
#endif /* NEEDED_FLAG_WALK && ... */
#else /* !DO_SUBTREE_WALK */
#define walk_string_ptr(ptr, entry_kind, entry_length) \
  remap_ptr((ptr), a_char_ptr, (entry_kind))
#endif /* DO_SUBTREE_WALK */

/*
Process a list, each entry linked to the next by the "next" field.
ptr is the pointer to the list, ptr_type is the type of ptr, and entry_kind
is the kind of entries on the list.  If walking subtrees, each entry is
processed; if not, ptr is remapped but the list is not traversed.
walk_list_on_link_field can be used when the link field is called something
other than "next".
*/
#undef walk_list_on_link_field
#if DO_SUBTREE_WALK
#define walk_list_on_link_field(ptr, ptr_type, entry_kind, link_field) \
  [] (ptr_type *ptr_ptr) {                                             \
    for (; *ptr_ptr != NULL; ptr_ptr = &(*ptr_ptr)->link_field) {      \
      walk_list_ptr(*ptr_ptr, ptr_type, (entry_kind));                 \
    }  /* for */                                                       \
  } (&(ptr))  /* walk_list_on_link_field */
#else /* !DO_SUBTREE_WALK */
#define walk_list_on_link_field(ptr, ptr_type, entry_kind, link_field) \
  remap_list_ptr((ptr), ptr_type, (entry_kind))
#endif /* DO_SUBTREE_WALK */
#undef walk_list
#define walk_list(ptr, ptr_type, entry_kind) \
  walk_list_on_link_field(ptr, ptr_type, entry_kind, next)

/*
Similar to walk_list, but expands to nothing in the NEEDED_FLAG_WALK mode.
*/
#undef walk_list_not_needed
#if NEEDED_FLAG_WALK
#define walk_list_not_needed(ptr, ptr_type, entry_kind) /* Nothing */
#else /* !NEEDED_FLAG_WALK */
#define walk_list_not_needed(ptr, ptr_type, entry_kind) \
  walk_list(ptr, ptr_type, entry_kind)
#endif /* NEEDED_FLAG_WALK */

/*
Macro to test that a class scope member should be kept even if unneeded
during the walk_needed_on_list traversal when KEEP_IN_IL_WALK is TRUE.
Specifically, if lowering is done, unneeded class scope entities in the
primary translation unit should not be kept, because lowering will
promote them to namespace scope (only called for static data members,
member functions, and nested types).  Prototype instantiations don't
get lowered, so the promotion doesn't happen for them.
*/
#undef class_scope_member_should_be_kept
#if KEEP_IN_IL_WALK
#if DO_IL_LOWERING
#define class_scope_member_should_be_kept(entry_ptr) \
  (suppress_il_lowering || in_secondary_trans_unit(entry_ptr) ||	\
   parent_class_of(entry_ptr)->variant.class_struct_union.is_nonreal_class)
#else /* !DO_IL_LOWERING */
#define class_scope_member_should_be_kept(entry_ptr) TRUE
#endif /* DO_IL_LOWERING */
#endif /* KEEP_IN_IL_WALK */

/*
Similar to walk_list, but used to walk lists attached to a scope.
scope_kind indicates the scope kind.  When KEEP_IN_IL_WALK is TRUE,
expands to code that acts differently for certain kinds of scopes:
  -- For the file scope and namespace scopes, it walks the list but
     calls walk_ptr only on those entries with the "needed" flag TRUE
     (and on those, it clears the keep_in_il flag before doing the walk,
     to deal with entities that can be redeclared and whose subtrees
     can therefore change).
  -- For class scopes, every entry gets keep_in_il set (because classes
     are kept or removed in their entirety).  keep_in_il is cleared
     and set again, as above, because of changing subtrees.  This does
     not apply if the class scope is going to be lowered.
  -- For other scopes, it does a normal walk_list.
In NEEDED_FLAG_WALK mode, expands to nothing.  In other modes, expands to a
simple walk_list.
*/
#undef walk_needed_on_list
#if NEEDED_FLAG_WALK
#define walk_needed_on_list(ptr, ptr_type, entry_kind, scope_kind)/* Nothing */
#else /* !NEEDED_FLAG_WALK */
#if KEEP_IN_IL_WALK
/*lint -emacro(506,walk_needed_on_list)*/
#define walk_needed_on_list(ptr, ptr_type, entry_kind, scope_kind) \
{ if ((scope_kind) == (a_scope_kind)sck_file || \
      (scope_kind) == (a_scope_kind)sck_namespace || \
      (scope_kind) == (a_scope_kind)sck_class_struct_union) { \
    ptr_type   local_ptr = (ptr); \
    a_boolean  class_member_to_be_kept = \
                 local_ptr != NULL && \
                 ((scope_kind) == (a_scope_kind)sck_class_struct_union && \
                  class_scope_member_should_be_kept(local_ptr)); \
    for (; local_ptr != NULL; local_ptr = local_ptr->next) { \
      if (class_member_to_be_kept || \
          needed_flag_is_set(&local_ptr->source_corresp) || \
          il_entry_prefix_of(local_ptr).keep_in_il) { \
        clear_keep_in_il_to_allow_subtree_walk((char *)local_ptr, entry_kind);\
        walk_list_ptr(local_ptr, ptr_type, (entry_kind)); \
      }  /* if */ \
    }  /* for */ \
  } else { \
    walk_list(ptr, ptr_type, entry_kind); \
  }  /* if */ \
}  /* walk_needed_on_list */
#else /* !KEEP_IN_IL_WALK */
#define walk_needed_on_list(ptr, ptr_type, entry_kind, scope_kind) \
  walk_list(ptr, ptr_type, entry_kind)
#endif /* KEEP_IN_IL_WALK */
#endif /* NEEDED_FLAG_WALK */

/*
Similar to walk_list, but when KEEP_IN_IL_WALK is TRUE, does a
special walk that clears the keep_in_il flag and resets it, to
ensure that subtrees are walked.  In NEEDED_FLAG_WALK mode, expands
to nothing.  In other modes, expands to a simple walk_list.
*/
#undef walk_list_with_keep_in_il_reset
#if NEEDED_FLAG_WALK
#define walk_list_with_keep_in_il_reset(ptr, ptr_type, entry_kind)/* Nothing */
#else /* !NEEDED_FLAG_WALK */
#if KEEP_IN_IL_WALK
#define walk_list_with_keep_in_il_reset(ptr, ptr_type, entry_kind) \
[] (ptr_type local_ptr) { \
  for (; local_ptr != NULL; local_ptr = local_ptr->next) { \
    clear_keep_in_il_to_allow_subtree_walk((char *)local_ptr, entry_kind); \
    walk_list_ptr(local_ptr, ptr_type, (entry_kind)); \
  }  /* for */ \
} (ptr)  /* walk_list_with_keep_in_il_reset */
#else /* !KEEP_IN_IL_WALK */
#define walk_list_with_keep_in_il_reset(ptr, ptr_type, entry_kind) \
  walk_list(ptr, ptr_type, entry_kind)
#endif /* KEEP_IN_IL_WALK */
#endif /* NEEDED_FLAG_WALK */


#undef simple_walk_param_list_default_arg_exprs
#undef walk_param_list_default_arg_exprs
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
/*
Walk any default argument expressions associated with the given
parameter type list.
*/
#define simple_walk_param_list_default_arg_exprs(param_list) \
[] (a_param_type_ptr ptp) { \
  for (; ptp != NULL; ptp = ptp->next) { \
    walk_ptr(ptp->default_arg_expr, an_expr_node_ptr, iek_expr_node); \
  }  /* for */ \
} (param_list)  /* simple_walk_param_list_default_arg_exprs */

/*
For the needed-flag or keep-in-il walk, walk any default argument
expressions associated with the given parameter type list, but if
we're doing the needed-flag traversal in a mode where lowering is
expected to be done, do nothing.  In a simple world, this special
traversal would not be needed: lowering clears default argument
expression pointers to NULL, and the needed and keep-in-il walks are
done after lowering, so those walks should see only NULL pointers and
never mark the expressions as needed.  However, when a (lowered) call
in a function scope memory region is walked, the function type in the
file scope is still unlowered and therefore still has attached default
argument expressions.  We don't want to mark something referenced in
such an expression as needed merely because it appears in a default
argument expression, because the default argument expression may not
be used if all the calls provide a full set of arguments.  That
suggests that we should just pretend that the default argument
pointers are NULL already, because eventually they will be, but that
doesn't work either, because there are some function types that do not
get lowered, e.g., in prototype instantiations and in casts in backing
expressions, and those default argument pointers will never be cleared
(and might have associated object lifetimes pointing to them).  Also,
it turns out to be impractical to know in general whether a given
function type will eventually be lowered.  So the pragmatic solution
is to mark the default argument expression as keep-in-il but not as
needed, which produces correct IL that possibly includes some unneeded
entities.  To improve the odds on the normal cases, default arguments
associated with function definitions are handled when the function
type is walked, which guarantees that any lowering that will occur has
already happened, and therefore that we never in that case mark
something as keep-in-il when it's not actually required.
*/
#if NEEDED_FLAG_WALK && !STANDALONE_UTILITY_PROGRAM && DO_IL_LOWERING
#define walk_param_list_default_arg_exprs(param_list) \
{ if (!il_lowering_needed() || walking_secondary_trans_unit) { \
    simple_walk_param_list_default_arg_exprs(param_list); \
  }  /* if */ \
}  /* walk_param_list_default_arg_exprs */
#else /* !(NEEDED_FLAG_WALK && !STANDALONE_UTILITY_PROGRAM && DO_IL_LOWERING)*/
#define walk_param_list_default_arg_exprs(param_list) \
{ simple_walk_param_list_default_arg_exprs(param_list); }
#endif /* NEEDED_FLAG_WALK && !STANDALONE_UTILITY_PROGRAM && DO_IL_LOWERING */
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */


/*
Set the definition_needed or keep_definition_in_il flag in a type if the
type is a class type.  Used to indicate cases that require the full type
of a class rather than just a declaration.
set_proper_definition_needed_flag sets either definition_needed or
keep_definition_in_il, or does nothing, depending on the configuration.
If prototype instantiations are recorded in the IL, they are marked along with
the real instantiations that the template generated.
*/
#undef definition_needed_if_class
#undef set_proper_definition_needed_flag
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
#if NEEDED_FLAG_WALK
#define set_proper_definition_needed_flag(ptr)                               \
  set_class_definition_needed(ptr)
#else /* !NEEDED_FLAG_WALK (i.e., KEEP_IN_IL_WALK) */
#define set_proper_definition_needed_flag(ptr)                               \
  set_class_keep_definition_in_il(ptr)
#endif /* NEEDED_FLAG_WALK */
#define definition_needed_if_class(ptr) \
{ a_type_ptr local_ptr = skip_typerefs(ptr); \
  if (local_ptr->kind == (a_type_kind)tk_array) { \
    local_ptr = underlying_array_element_type(local_ptr); \
    local_ptr = skip_typerefs(local_ptr); \
  }  /* if */ \
  if (is_immediate_class_type(local_ptr) && !is_incomplete_type(local_ptr)) { \
    set_proper_definition_needed_flag(local_ptr); \
  }  /* if */ \
}  /* definition_needed_if_class */
#else /* !(NEEDED_FLAG_WALK || KEEP_IN_IL_WALK) */
#define definition_needed_if_class(ptr) /* Nothing */
#define set_proper_definition_needed_flag(ptr) /* Nothing */
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */

/*
Set the definition_needed or keep_definition_in_il flag in a routine.
*/
#undef set_proper_routine_definition_needed_flag
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
#if NEEDED_FLAG_WALK
#define set_proper_routine_definition_needed_flag(ptr)   \
  set_routine_definition_needed(ptr)
#else /* !NEEDED_FLAG_WALK (i.e., KEEP_IN_IL_WALK) */
#define set_proper_routine_definition_needed_flag(ptr)       \
  set_routine_keep_definition_in_il(ptr)
#endif /* NEEDED_FLAG_WALK */
#else /* !(NEEDED_FLAG_WALK || KEEP_IN_IL_WALK) */
#define set_proper_routine_definition_needed_flag(ptr) /* Nothing */
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */

/*
Macro to remap parent scope only if it exists.  When doing the needed
or keep-in-IL walk, visit the associated namespace or class (through which
the scope will also be visited).  When remapping, just ensure that the
parent scope pointer is remapped. */
#undef walk_or_remap_parent
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
#define walk_or_remap_parent(ptr, walk) \
{ if ((ptr).parent_scope != NULL) { \
    if ((ptr).parent_scope->kind == (a_scope_kind)sck_namespace) { \
      walk_ptr((ptr).parent_scope->variant.assoc_namespace, a_namespace_ptr, \
               iek_namespace); \
    } else if ((ptr).is_class_member) {  \
      walk_ptr((ptr).parent_scope->variant.assoc_type, a_type_ptr, iek_type); \
      set_proper_definition_needed_flag(scp_parent_class(&ptr)); \
    }  /* if */  \
  }  /* if */  \
}  /* walk_or_remap_parent */
#else /* !(NEEDED_FLAG_WALK || KEEP_IN_IL_WALK) */
/*lint -emacro(506,walk_or_remap_parent)*/
#define walk_or_remap_parent(ptr, walk) \
{ if ((walk)) { \
    walk_ptr((ptr).parent_scope, a_scope_ptr, iek_scope); \
  } else { \
    remap_ptr((ptr).parent_scope, a_scope_ptr, iek_scope); \
  }  /* if */ \
}  /* walk_or_remap_parent */
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */

/*
Clear a front end pointer (to avoid passing it to the next phase) if
necessary.
*/
#undef conditionally_clear_fe_pointer
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
#define conditionally_clear_fe_pointer(ptr) /* Nothing */
#else /* !(NEEDED_FLAG_WALK || KEEP_IN_IL_WALK) */
#define conditionally_clear_fe_pointer(ptr) \
{ if (clear_fe_pointers_during_walk) (ptr) = NULL; }
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */

/*
Either clear the name qualifier pointer or walk it.  The argument to this
macro is the pointer to the name reference containing the name qualifier
pointer.  The name qualifier is either ignored or cleared if the name
reference came from a prototype instantiation and we are not recording
prototype instantiations in the IL.
*/
#undef clear_or_walk_name_reference_field
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
#define clear_or_walk_name_reference_field(name_ref_ptr, field, \
                                           ptr_type, entry_kind) \
{ if (prototype_instantiations_in_il || \
      !(name_ref_ptr)->from_prototype_instantiation) { \
    walk_ptr((field), ptr_type, (entry_kind)); \
  }  /* if */ \
}
#else /* !(NEEDED_FLAG_WALK || KEEP_IN_IL_WALK) */
#define clear_or_walk_name_reference_field(name_ref_ptr, field, \
                                           ptr_type, entry_kind) \
{ if (prototype_instantiations_in_il || \
      !(name_ref_ptr)->from_prototype_instantiation) { \
    walk_ptr((field), ptr_type, (entry_kind)); \
  } else { \
    if (clear_fe_pointers_during_walk) { \
      (field) = NULL; \
    }  /* if */ \
  }  /* if */ \
}
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */

#undef remap_source_sequence_entry
#if GENERATE_SOURCE_SEQUENCE_LISTS && !NEEDED_FLAG_WALK && !KEEP_IN_IL_WALK
#define remap_source_sequence_entry(ptr) \
  remap_ptr((ptr).source_sequence_entry, a_source_sequence_entry_ptr, \
            iek_source_sequence_entry)
#else /* !(GENERATE_SOURCE_SEQUENCE_LISTS && ...) */
#define remap_source_sequence_entry(ptr) /* Nothing */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS & ... */

/*
Process the source correspondence field pointed to by ptr.
*/
#undef walk_source_corresp
#undef walk_source_corresp_full
#if NEEDED_FLAG_WALK
/* Note intentional use of walk_ptr not walk_list for name references, because
  of issues with export creating lists that run between translation units. */
#define walk_source_corresp_full(ptr, walk) \
{ \
  walk_or_remap_parent((ptr), (walk)); \
  walk_ptr((ptr).name_references, a_name_reference_ptr, iek_name_reference) \
}  /* walk_source_corresp_full */
#else /* !NEEDED_FLAG_WALK */
#undef walk_unmangled_name
#if NEED_NAME_MANGLING
#define walk_unmangled_name(ptr) \
  walk_string_ptr((ptr).unmangled_name_or_mangled_encoding, iek_id_name, 0)
#else /* !NEED_NAME_MANGLING */
#define walk_unmangled_name(ptr) /* Nothing */
#endif /* NEED_NAME_MANGLING */
#undef walk_per_instantiation_needed_flags
#if ONE_INSTANTIATION_PER_OBJECT
#define walk_per_instantiation_needed_flags(ptr) \
  walk_list((ptr).per_instantiation_needed_flags, \
            a_per_instantiation_needed_flags_entry_ptr, \
            iek_per_instantiation_needed_flags_entry)
#else /* !ONE_INSTANTIATION_PER_OBJECT */
#define walk_per_instantiation_needed_flags(ptr) /* Nothing */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#undef walk_decl_position_supplement
#if EXTRA_SOURCE_POSITIONS_IN_IL
#define walk_decl_position_supplement(ptr) \
  walk_ptr((ptr).decl_pos_info, a_decl_position_supplement_ptr, \
           iek_decl_position_supplement)
#else /* !EXTRA_SOURCE_POSITIONS_IN_IL */
#define walk_decl_position_supplement(ptr) /* Nothing */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

#define walk_source_corresp_full(ptr, walk) \
{ walk_string_ptr((ptr).name, iek_id_name, 0); \
  walk_unmangled_name(ptr); \
  conditionally_clear_fe_pointer((ptr).trans_unit_corresp); \
  walk_or_remap_parent((ptr), (walk)); \
  remap_ptr((ptr).enclosing_routine, a_routine_ptr, iek_routine); \
  remap_source_sequence_entry(ptr); \
  conditionally_clear_fe_pointer((ptr).assoc_info); \
  walk_per_instantiation_needed_flags(ptr); \
  walk_decl_position_supplement(ptr); \
  /* See note above. */ \
  walk_ptr((ptr).name_references, a_name_reference_ptr, iek_name_reference) \
  walk_list((ptr).attributes, an_attribute_ptr, iek_attribute); \
}  /* walk_source_corresp_full */
#endif /* NEEDED_FLAG_WALK */
/* The default source correspondence walk just remaps its parent pointer. */
#define walk_source_corresp(ptr) walk_source_corresp_full((ptr), FALSE)

/*
If defined, nonstatic_variable_always_needed returns TRUE if the specified
variable must be flagged as needed even if it is unreferenced and FALSE
otherwise.  In configurations in which the macro would always return FALSE,
it should be left undefined as an indication that there is no need to loop
over the list of nonstatic variables in the scope.
*/
#undef nonstatic_variable_always_needed
#if GNU_EXTENSIONS_ALLOWED
/* A cleanup attribute on a variable implies that the variable is needed,
   even if unreferenced, to ensure that the associated cleanup routine is
   called when the variable goes out of scope. */
#define nonstatic_variable_always_needed(var) \
  ((var)->cleanup_routine != NULL)
#endif /* GNU_EXTENSIONS_ALLOWED */

#undef report_bad_init_kind
#if CHECKING
#define report_bad_init_kind()                                        \
  internal_error("walk_entry_and_subtree: bad init kind")
#else /* !CHECKING */
#define report_bad_init_kind()  /* Nothing */
#endif /* CHECKING */

#undef walk_initializer
/*lint -emacro(506,walk_initializer)*/
#define walk_initializer(init_kind, initializer, is_member_constant)  \
{ switch (init_kind) {                                                \
    case initk_zero:                                                  \
    case initk_function_local:                                        \
      /* No pointers. */                                              \
      break;                                                          \
    case initk_none:                                                  \
      if (is_member_constant) {                                       \
        /* Un-lowered constant is not used, but kept in the IL. */    \
        walk_ptr_not_needed((initializer).constant, a_constant_ptr,   \
                            iek_constant);                            \
      }  /* if */                                                     \
      break;                                                          \
    case initk_static:                                                \
      walk_ptr((initializer).constant, a_constant_ptr, iek_constant); \
      break;                                                          \
    case initk_dynamic:                                               \
      walk_ptr((initializer).dynamic, a_dynamic_init_ptr,             \
               iek_dynamic_init);                                     \
      break;                                                          \
    case initk_binding:                                               \
      walk_ptr((initializer).bound_expr, an_expr_node_ptr,            \
               iek_expr_node);                                        \
      break;                                                          \
    default:                                                          \
      report_bad_init_kind();                                         \
  }  /* switch */                                                     \
}  /* walk_initializer */


/* The name is provided by a macro so it can be different things, e.g.,
    walk_entry_and_subtree and remap_pointers_in_entry. */
WALK_ENTRY_ROUTINE_STATIC /* "static" if routine should be static. */
void WALK_ENTRY_ROUTINE_NAME(char             *entry_ptr,
                             an_il_entry_kind entry_kind)
/*
Process the entry at entry_ptr (which is of kind indicated by entry_kind)
by remapping its pointers, and, if DO_SUBTREE_WALK is TRUE, walking its
subtree and calling the entry_process_func.  When DO_SUBTREE_WALK is
TRUE, if the entry has already been seen, or if the pointer crosses into
the file scope, do not process it (but record an orphan in the latter case).
Note that entry_ptr is frequently cast to the kind of the IL entry through
the use of the "eptr" macro, rather than through the use of block-scope
variables.  That's to decrease the amount of stack needed by this routine
(since it may be deeply recursive) and some compilers (particularly in
debug builds) don't recognize that these variables are mutually-exclusive.
*/
{
#if DO_SUBTREE_WALK
  char  *next_entry_ptr = NULL;
handle_next_entry:
  /* Do a termination test (to prune the walk) if walking subtrees. */
  /* See if there is a termination-test function provided by the caller.
     If so, call it to see if we just return on encountering this entry. */
  if (walk_termination_test_func != NULL) {
    if (walk_termination_test_func(entry_ptr, entry_kind)) {
      goto end_of_routine;
    }  /* if */
  } else {
    /* The default termination test (a) stops on crossing from a function
       scope memory region into the file scope memory region (recording
       an orphan on the entry in the file scope memory region), and
       (b) sets the il_walk_flag in the entry prefix to mark the entries
       that have been seen already, stopping on encountering one already
       marked. */
    /* This default termination test cannot be used when walking
       secondary translation units, because the IL for those is intermixed.
       If we were to record an orphan, we might do so in the wrong translation
       unit. */
    if ([] (char *local_entry_ptr, an_il_entry_kind local_entry_kind) {
        an_il_entry_prefix_ptr epp = &il_entry_prefix_of(local_entry_ptr);
        a_boolean              result;
        /* If we are walking through a function scope, and the entry here is
           in the file scope, terminate the walk. */
        if (!walking_file_scope && epp->file_scope) {
          /* Add non-string file scope IL entries referenced from a
             function scope to the orphaned IL entries lists. */
          possibly_add_orphaned_file_scope_il_entry(local_entry_ptr,
                                                    local_entry_kind);
          result = TRUE;
        } else {
          /* See if this entry has been reached already, and if so, don't
             process it or its subtree.  This is indicated by the il_walk_flag
             field of the entry prefix. */
          if (epp->il_walk_flag == flag_value_meaning_visited) {
            /* Entry has already been visited. */
            result = TRUE;
          } else {
            /* Set the flag to indicate that this entry has been visited. */
            epp->il_walk_flag = flag_value_meaning_visited;
            result = FALSE;
          }  /* if */
        }  /* if */
        return result;
      } (entry_ptr, entry_kind)) {
      goto end_of_routine;
    }
  }  /* if */
#endif /* DO_SUBTREE_WALK */
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 5) {
    fprintf(f_debug, "Walking IL tree, entry kind = %s\n",
                     il_entry_kind_names[(int)entry_kind]);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  /* For each pointer in the entry, remap it and walk the subtree.
     In general, linked lists are traversed while processing the entry
     that contains the head-of-list pointer.  This is done to avoid
     using recursion to process very long lists (the stack space 
     requirements could be ridiculous).  It also means the "next"
     pointers should not be processed or remapped during the processing
     of the entries that contain them, because they've already been
     handled.  Of course if the subtrees are not being walked
     the "next" fields must be processed as they are encountered. */
  switch (entry_kind) {
    case iek_source_file:
      {
#define eptr ((a_source_file_ptr)entry_ptr)
        walk_string_ptr(eptr->file_name, iek_other_text, 0);
        walk_string_ptr(eptr->full_name, iek_other_text, 0);
        walk_string_ptr(eptr->name_as_written, iek_other_text, 0);
        walk_list(eptr->first_child_file, a_source_file_ptr, iek_source_file);
        remap_ptr(eptr->last_child_file, a_source_file_ptr, iek_source_file);
        remap_next_ptr(eptr->next, a_source_file_ptr, iek_source_file);
        walk_ptr(eptr->assoc_module, a_module_ptr, iek_module);
#undef eptr
      }
      break;
    case iek_seq_number_lookup_entry:
      {
#define eptr ((a_seq_number_lookup_entry_ptr)entry_ptr)
        remap_ptr(eptr->source_file, a_source_file_ptr, iek_source_file);
        remap_next_ptr(eptr->next, a_seq_number_lookup_entry_ptr,
                       iek_seq_number_lookup_entry);
#undef eptr
      }
      break;
    case iek_constant:
      {
#define eptr ((a_constant_ptr)entry_ptr)
        remap_next_ptr(eptr->next, a_constant_ptr, iek_constant);
        walk_ptr(eptr->type, a_type_ptr, iek_type);
        walk_ptr(eptr->orig_type, a_type_ptr, iek_type);
        /* If backing expressions are kept, they are only keep-in-IL, not
           needed. */
        walk_ptr_not_needed(eptr->expr, an_expr_node_ptr, iek_expr_node);
        conditionally_clear_fe_pointer(eptr->rescan_info);
#if DO_IL_LOWERING
        conditionally_clear_fe_pointer(eptr->assoc_var);
#endif /* DO_IL_LOWERING */
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
        if (eptr->type != NULL) {
          definition_needed_if_class(eptr->type);
        }  /* if */
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
        switch (eptr->kind) {
          case ck_error:
          case ck_integer:
#if FIXED_POINT_ALLOWED
          case ck_fixed_point:
#endif /* FIXED_POINT_ALLOWED */
#if UPC_EXTENSIONS_ALLOWED
          case ck_upc_threads:
          case ck_upc_mythread:
#endif /* UPC_EXTENSIONS_ALLOWED */
          case ck_float:
#if C99_IL_EXTENSIONS_SUPPORTED
          case ck_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
          case ck_void:
            /* No pointers. */
            break;
          case ck_string:
            walk_string_ptr(eptr->variant.string.value, iek_string_text,
                            eptr->variant.string.length);
#if PRESERVE_EMBED_DIRECTIVE_WHEN_OPTIMIZED
            if (eptr->variant.string.embed_directive != NULL) {
              walk_string_ptr(eptr->variant.string.embed_directive,
                              iek_other_text, 0);
            }  /* if */
#endif /* PRESERVE_EMBED_DIRECTIVE_WHEN_OPTIMIZED */
            break;
#if C99_IL_EXTENSIONS_SUPPORTED
          case ck_complex:
            walk_ptr(eptr->variant.complex_value,
                     an_internal_complex_value_ptr,
                     iek_internal_complex_value);
            break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
          case ck_address:
            switch (eptr->variant.address.kind) {
              case abk_routine:
                /* Routines will be visited from the scope. */
                remap_ptr(eptr->variant.address.variant.routine, a_routine_ptr,
                          iek_routine);
                set_proper_routine_definition_needed_flag(
                                        eptr->variant.address.variant.routine);
                break;
              case abk_variable:
                /* Variables will be visited from the scope. */
                remap_ptr(eptr->variant.address.variant.variable,
                          a_variable_ptr, iek_variable);
                break;
              case abk_constant:
              case abk_temporary:
                /* Constants might not be on the scope constant list, so visit
                   their subtrees. */
                walk_ptr(eptr->variant.address.variant.constant,
                         a_constant_ptr, iek_constant);
                break;
              case abk_uuidof:
                /* Class types will be visited from the scope. */
                remap_ptr(eptr->variant.address.variant.type, a_type_ptr,
                          iek_type);
                break;
              case abk_typeid:
#if MICROSOFT_EXTENSIONS_ALLOWED
              case abk_cli_typeid:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                /* The recorded type is not limited to the kind of types that
                   are visited from the scope.  So walk the subtree in any
                   case. */
                walk_ptr(eptr->variant.address.variant.type, a_type_ptr,
                         iek_type);
                break;
#if MICROSOFT_EXTENSIONS_ALLOWED
              case abk_cli_array:
                /* No variant fields. */
                break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
              case abk_label:
                /* Labels will be visited from the scope. */
                remap_ptr(eptr->variant.address.variant.label, a_label_ptr,
                          iek_label);
                break;
              default:
                unexpected_condition_str(
                             "walk_entry_and_subtree: bad address const kind");
            }  /* switch */
            walk_list(eptr->variant.address.subobject_path,
                      a_subobject_path_ptr, iek_subobject_path);
            break;
          case ck_ptr_to_member:
            remap_ptr(eptr->variant.ptr_to_member.casting_base_class,
                      a_base_class_ptr, iek_base_class);
            walk_ptr(eptr->variant.ptr_to_member.name_reference,
                     a_name_reference_ptr, iek_name_reference);
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
            /* coverity[var_deref_model] */
            set_proper_definition_needed_flag(
                                   pm_class_type_possibly_lowered(eptr->type));
            if (eptr->variant.ptr_to_member.cast_to_base) {
              /* On a cast from a derived to a base class, mark the derived
                 class's definition as needed. */
              set_proper_definition_needed_flag(
                eptr->variant.ptr_to_member.casting_base_class->derived_class);
            }  /* if */
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
            if (eptr->variant.ptr_to_member.is_function_ptr) {
              remap_ptr(eptr->variant.ptr_to_member.variant.routine,
                        a_routine_ptr, iek_routine);
              if (eptr->variant.ptr_to_member.variant.routine != NULL) {
                set_proper_routine_definition_needed_flag(
                                  eptr->variant.ptr_to_member.variant.routine);
              }  /* if */
            } else {
              remap_ptr(eptr->variant.ptr_to_member.variant.field, a_field_ptr,
                        iek_field);
            }  /* if */
            break;
#if GNU_EXTENSIONS_ALLOWED
          case ck_label_difference:
            walk_ptr(eptr->variant.label_difference.from_address,
                     a_constant_ptr, iek_constant);
            walk_ptr(eptr->variant.label_difference.to_address,
                     a_constant_ptr, iek_constant);
            break;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING && GENERATE_EH_TABLES && !DO_FULL_PORTABLE_EH_LOWERING
          case ck_stack_offset:
            remap_ptr(eptr->variant.stack_offset.variable, a_variable_ptr,
                      iek_variable);
            break;
#endif /* DO_IL_LOWERING && ... */
          case ck_dynamic_init:
            walk_ptr(eptr->variant.dynamic_init.ptr, a_dynamic_init_ptr,
                     iek_dynamic_init);
#if DO_IL_LOWERING
            if (eptr->constant_for_base_class) {
              remap_ptr(eptr->variant.dynamic_init.field_or_base.base,
                        a_base_class_ptr, iek_base_class);
            } else {
              remap_ptr(eptr->variant.dynamic_init.field_or_base.field,
                        a_field_ptr, iek_field);
            }  /* if */
#endif /* DO_IL_LOWERING */
            break;
          case ck_aggregate:
            walk_list(eptr->variant.aggregate.first_constant, a_constant_ptr,
                      iek_constant);
            remap_ptr(eptr->variant.aggregate.last_constant, a_constant_ptr,
                      iek_constant);
#if DO_IL_LOWERING
            if (eptr->constant_for_base_class) {
              remap_ptr(eptr->variant.aggregate.field_or_base.base,
                        a_base_class_ptr, iek_base_class);
            } else {
              remap_ptr(eptr->variant.aggregate.field_or_base.field,
                        a_field_ptr, iek_field);
            }  /* if */
#endif /* DO_IL_LOWERING */
            break;
          case ck_init_repeat:
            walk_ptr(eptr->variant.init_repeat.constant, a_constant_ptr,
                     iek_constant);
            break;
          case ck_designator:
            if (eptr->variant.designator.is_field_designator) {
              if (eptr->variant.designator.is_generic) {
                walk_string_ptr(eptr->variant.designator.variant.field_name,
                                iek_id_name, 0);
              } else {
                remap_ptr(eptr->variant.designator.variant.field, a_field_ptr,
                          iek_field);
              }  /* if */
            } else {
              if (eptr->variant.designator.is_generic) {
                walk_list(eptr->variant.designator.variant.subscript,
                          a_constant_ptr, iek_constant);
              }  /* if */
            }  /* if */
            break;
          case ck_template_param:
            switch (eptr->variant.template_param.kind) {
              case tpck_param:
              case tpck_member:
                /* No action required. */
                break;
              case tpck_expression:
                walk_ptr(eptr->variant.template_param.variant.expr,
                         an_expr_node_ptr, iek_expr_node);
                break;
              case tpck_unknown_function:
                walk_ptr(eptr->variant.template_param.variant.unknown_function.
                                                               conversion_type,
                         a_type_ptr, iek_type);
#if MICROSOFT_EXTENSIONS_ALLOWED
                walk_ptr(eptr->variant.template_param.variant.unknown_function
                                                      .property_or_event_descr,
                         a_property_or_event_descr_ptr,
                         iek_property_or_event_descr);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                conditionally_clear_fe_pointer(eptr->variant.template_param.
                                              variant.unknown_function.symbol);
                break;
              case tpck_address:
              case tpck_dependent_constant:
                walk_ptr(eptr->variant.template_param.variant.constant,
                         a_constant_ptr, iek_constant);
                break;
              case tpck_concat_string_literals:
                walk_list(eptr->variant.template_param
                               .variant.string_literal_list,
                          a_constant_ptr, iek_constant);
                break;
              case tpck_sizeof:
              case tpck_datasizeof:
              case tpck_alignof:
              case tpck_uuidof:
              case tpck_typeid:
              case tpck_noexcept:
                walk_ptr(
                        eptr->variant.template_param.variant.templ_sizeof.type,
                         a_type_ptr, iek_type);
                walk_ptr(
                        eptr->variant.template_param.variant.templ_sizeof.expr,
                         an_expr_node_ptr, iek_expr_node);
                break;
              case tpck_template_ref:
                walk_ptr(eptr->variant.template_param.variant.template_ref.con,
                         a_constant_ptr, iek_constant);
                walk_list(eptr->variant.template_param.variant.
                                                         template_ref.arg_list,
                          a_template_arg_ptr, iek_template_arg);
                break;
              case tpck_integer_pack:
                walk_ptr(eptr->variant.template_param.variant.bound,
                         a_constant_ptr, iek_constant);
                break;
              case tpck_destructor:
                walk_ptr(eptr->variant.template_param.variant.destructor.type,
                         a_type_ptr, iek_type);
                break;
              default:
                unexpected_condition_str(
                   "walk_entry_and_subtree: bad template param constant kind");
            }  /* switch */
            break;
          case ck_reflection:
            { an_il_entry_kind  kind = eptr->variant.reflection.entity.kind;
              /* Unlike a reflected routine, scope, or namespace (each of which
                 is written from its own memory region and so is merely
                 remapped here), a reflected type must be written wherever it
                 is reached.  A type reflected inside an uninstantiated
                 template can be reachable only through the reflection that
                 designates it (e.g., a dependent type used as a name
                 qualifier), so it is walked, like the other entities below,
                 rather than just remapped.  A data member specification is
                 likewise reachable only through the reflection that describes
                 it. */
              if (kind == iek_expr_node || kind == iek_constant ||
                  kind == iek_token_sequence ||
                  kind == iek_data_member_spec ||
                  kind == iek_type) {
                walk_ptr(eptr->variant.reflection.entity.ptr, a_char_ptr,
                         kind);
              } else {
                remap_ptr(eptr->variant.reflection.entity.ptr, a_char_ptr,
                          kind);
              }  /* if */
            }
            break;
          default:
            unexpected_condition_str(
                                  "walk_entry_and_subtree: bad constant kind");
        }  /* switch */
        walk_source_corresp_full(eptr->source_corresp,
                                 (constant_is(eptr, ck_template_param) ||
                                  eptr->is_named_constant_definition));
#undef eptr
      }
      break;
    case iek_param_type:
      {
#define eptr ((a_param_type_ptr)entry_ptr)
        remap_next_ptr(eptr->next, a_param_type_ptr, iek_param_type);
        walk_ptr(eptr->type, a_type_ptr, iek_type);
        walk_ptr(eptr->declared_type, a_type_ptr, iek_type);
        /* The C language doesn't actually require that parameter types
           be complete if the function is not called.  However, some
           C compilers (e.g., gcc) warn on an incomplete parameter
           type, so keep the class definition to avoid such warnings. */
        definition_needed_if_class(eptr->type);
        walk_string_ptr(eptr->name, iek_id_name, 0);
        /* The default_arg_expr field is not walked at this level in the
           needed and keep-in-il walks, because we don't know enough about
           the context.  See the uses of walk_param_list_default_arg_exprs. */
#if !NEEDED_FLAG_WALK && !KEEP_IN_IL_WALK
        walk_ptr(eptr->default_arg_expr, an_expr_node_ptr, iek_expr_node);
#endif /* !NEEDED_FLAG_WALK && !KEEP_IN_IL_WALK */
        conditionally_clear_fe_pointer(
                       eptr->orig_param_type_for_unevaluated_default_arg_expr);
        walk_list(eptr->entities_defined_in_default_arg,
                  an_il_entity_list_entry_ptr, iek_il_entity_list_entry);
#if EXTRA_SOURCE_POSITIONS_IN_IL
        walk_ptr(eptr->decl_pos_info, a_decl_position_supplement_ptr,
                 iek_decl_position_supplement);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        walk_list(eptr->attributes, an_attribute_ptr, iek_attribute);
#if MICROSOFT_EXTENSIONS_ALLOWED
        walk_list(eptr->ms_attributes, an_ms_attribute_ptr, iek_ms_attribute);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        conditionally_clear_fe_pointer(eptr->pack_expansion_descr);
#undef eptr
      }
      break;
    case iek_routine_type_supplement:
      {
#define eptr ((a_routine_type_supplement_ptr)entry_ptr)
        walk_list(eptr->param_type_list, a_param_type_ptr, iek_param_type);
        remap_ptr(eptr->this_class, a_type_ptr, iek_type);
        walk_ptr(eptr->prototype_scope, a_scope_ptr, iek_scope);
        walk_ptr(eptr->exception_specification, an_exception_specification_ptr,
                 iek_exception_specification);
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
        /* Default argument expressions are not walked at the iek_param_type
           level for the needed and keep-in-il walks.  Do them now, except
           if we know we can do them later as part of processing a routine.
           See the comment on walk_param_list_default_arg_exprs. */
        if (!C_mode() && eptr->assoc_routine == NULL) {
          walk_param_list_default_arg_exprs(eptr->param_type_list);
        }  /* if */
        /* Do not walk the assoc_routine pointer for the needed or keep-in-il
           traversal.  We don't want this to force keeping of the routine
           definition if it's not otherwise needed. */
#else /* !(NEEDED_FLAG_WALK || KEEP_IN_IL_WALK) */
        remap_ptr(eptr->assoc_routine, a_routine_ptr, iek_routine);
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
#undef eptr
      }
      break;
#if !NEEDED_FLAG_WALK
    case iek_based_type_list_member:
      {
#define eptr ((a_based_type_list_member_ptr)entry_ptr)
        remap_next_ptr(eptr->next, a_based_type_list_member_ptr,
                       iek_based_type_list_member);
        /* Do walk_ptr instead of remap_ptr because the reference might
           be to an entity not otherwise in the IL tree, e.g., a front-end-only
           type. */
        walk_ptr(eptr->based_type, a_type_ptr, iek_type);
#undef eptr
      }
      break;
#endif /* !NEEDED_FLAG_WALK */
    case iek_type:
      {
#define eptr ((a_type_ptr)entry_ptr)
        /* Template parameter scopes are not linked into the IL, so for
           those we walk instead of remap the parent scope pointer. */
        walk_source_corresp_full(eptr->source_corresp,
                                (eptr->kind ==
                                              (a_type_kind)tk_template_param));
        remap_next_ptr(eptr->next, a_type_ptr, iek_type);
#if NEEDED_FLAG_WALK
        /* When walking to set "needed" flags, the based types list in
           general is not walked, but if there is a based type entry for the
           unqualified version of an array type, walk it. */
        [] (a_type_ptr tp) {
          a_type_ptr unqual_array_type;
          if (tp->kind == tk_array &&
              is_qualified_version_of_array_typedef(tp, &unqual_array_type)) {
            walk_ptr(unqual_array_type, a_type_ptr, iek_type);
          }  /* if */
        } (eptr);
#else /* !NEEDED_FLAG_WALK */
        walk_list(eptr->based_types, a_based_type_list_member_ptr,
                  iek_based_type_list_member);
#endif /* NEEDED_FLAG_WALK */
#if DO_IL_LOWERING
        remap_ptr_not_needed(eptr->typeinfo_var, a_variable_ptr, iek_variable);
#endif /* DO_IL_LOWERING */
        switch (eptr->kind) {
          case tk_unknown:
            expect_error_str("unknown type in IL walk");
            break;
          case tk_error:
          case tk_void:
#if FIXED_POINT_ALLOWED
          case tk_fixed_point:
#endif /* FIXED_POINT_ALLOWED */
          case tk_float:
#if C99_IL_EXTENSIONS_SUPPORTED
          case tk_complex:
          case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
#if GNU_EXTENSIONS_ALLOWED && GNU_VECTOR_TYPES_ALLOWED
          case tk_scalable_vector_count:
          case tk_mfp8:
          case tk_float8e4m3:
          case tk_float8e5m2:
#endif /* GNU_EXTENSIONS_ALLOWED && GNU_VECTOR_TYPES_ALLOWED */
          case tk_nullptr:
          case tk_reflection:
            /* No pointers. */
            break;
          case tk_integer:
            if (eptr->variant.integer.enum_type) {
              if (integer_type_is_scoped_enum(eptr)) {
                walk_ptr(eptr->variant.integer.enum_info.assoc_scope,
                         a_scope_ptr, iek_scope);
              } else {
                walk_list(eptr->variant.integer.enum_info.constant_list,
                          a_constant_ptr, iek_constant);
              }  /* if */
            } else {
              walk_ptr(eptr->variant.integer.enum_info.affiliated_type,
                       a_type_ptr, iek_type);
            }  /* if */
            walk_ptr(eptr->variant.integer.extra_info,
                     an_integer_type_supplement_ptr,
                     iek_integer_type_supplement);
            break;
          case tk_pointer:
            walk_ptr(eptr->variant.pointer.type, a_type_ptr, iek_type);
#if MICROSOFT_EXTENSIONS_ALLOWED
            remap_ptr(eptr->variant.pointer.base_variable, a_variable_ptr,
                      iek_variable);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
            break;
          case tk_array:
            if (eptr->variant.array.is_variable_size_array &&
                !eptr->variant.array.is_vla) {
              walk_ptr(eptr->variant.array.variant.element_count_expr,
                       an_expr_node_ptr, iek_expr_node);
            } else if (eptr->variant.array.is_template_dependent_size_array) {
              walk_ptr(eptr->variant.array.variant.element_count_constant,
                       a_constant_ptr, iek_constant);
            }  /* if */
            walk_ptr(eptr->variant.array.bound_constant,
                     a_constant_ptr, iek_constant);
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
            /* In some error cases, some array types on the vla_dimensions
               list are incomplete during the needed flag and keep-in-IL
               walks. */
            if (eptr->variant.array.element_type == NULL) break;
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
            walk_ptr(eptr->variant.array.element_type, a_type_ptr, iek_type);
            definition_needed_if_class(eptr->variant.array.element_type);
            break;
          case tk_class:
          case tk_struct:
          case tk_union:
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
            /* The field list is part of the definition and is walked only if
               the definition should be walked. */
            if (
#if NEEDED_FLAG_WALK
                class_definition_needed_flag_is_set(eptr)
#else /* !NEEDED_FLAG_WALK (i.e., KEEP_IN_IL_WALK) */
                eptr->variant.class_struct_union.keep_definition_in_il
#endif /* NEEDED_FLAG_WALK */
                                                                     )
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
            /* Do not insert code here. */
            {
                walk_list(eptr->variant.class_struct_union.field_list,
                          a_field_ptr, iek_field);
            }  /* if */
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
            /* Handle the class type supplement inline, because we need
               to have a pointer to the class to decide whether or not to
               process definition-related fields. */
            if (eptr->variant.class_struct_union.extra_info != NULL) {
              goto handle_class_type_supplement_for_class;
            }  /* if */
#else /* !(NEEDED_FLAG_WALK || KEEP_IN_IL_WALK) */
            walk_ptr(eptr->variant.class_struct_union.extra_info,
                     a_class_type_supplement_ptr, iek_class_type_supplement);
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
            break;
          case tk_typeref:
            walk_ptr(eptr->variant.typeref.type, a_type_ptr, iek_type);
#if NEEDED_FLAG_WALK
            /* A decltype operand should not be marked as needed. */
            if (eptr->variant.typeref.kind != trk_is_decltype) {
#endif /* NEEDED_FLAG_WALK */
              walk_ptr(eptr->variant.typeref.extra_info,
                       a_typeref_type_supplement_ptr,
                       iek_typeref_type_supplement);
#if NEEDED_FLAG_WALK
            }  /* if */
#endif /* NEEDED_FLAG_WALK */
#if DO_IL_LOWERING
#if KEEP_IN_IL_WALK
            walk_ptr(eptr->variant.typeref.orig_type, a_type_ptr, iek_type);
#else /* !KEEP_IN_IL_WALK */
            conditionally_clear_fe_pointer(eptr->variant.typeref.orig_type);
#endif /* KEEP_IN_IL_WALK */
#endif /* DO_IL_LOWERING */
            break;
          case tk_ptr_to_member:
            remap_ptr(eptr->variant.ptr_to_member.class_of_which_a_member,
                      a_type_ptr, iek_type);
            walk_ptr(eptr->variant.ptr_to_member.orig_class_of_which_a_member,
                     a_type_ptr, iek_type);
            walk_ptr(eptr->variant.ptr_to_member.type, a_type_ptr, iek_type);
            break;
          case tk_routine:
            walk_ptr(eptr->variant.routine.return_type, a_type_ptr, iek_type);
            walk_ptr(eptr->variant.routine.extra_info,
                     a_routine_type_supplement_ptr,
                     iek_routine_type_supplement);
#if DO_IL_LOWERING
            /* eptr->variant.routine.unlowered_type is not walked (as this
               is not part of the IL per se). */
#endif /* DO_IL_LOWERING */
            break;
          case tk_template_param:
            walk_ptr(eptr->variant.template_param.extra_info,
                     a_template_param_type_supplement_ptr,
                     iek_template_param_type_supplement);
            break;
#if GNU_EXTENSIONS_ALLOWED && GNU_VECTOR_TYPES_ALLOWED
          case tk_vector:
            walk_ptr(eptr->variant.vector.element_type, a_type_ptr, iek_type);
            walk_ptr(eptr->variant.vector.size_constant, a_constant_ptr,
                     iek_constant);
            break;
          case tk_scalable_vector:
            walk_ptr(eptr->variant.scalable_vector.element_type, a_type_ptr,
                     iek_type);
            break;
          case tk_riscv_vector:
            walk_ptr(eptr->variant.riscv_vector.element_type, a_type_ptr,
                     iek_type);
            break;
#endif /* GNU_EXTENSIONS_ALLOWED && GNU_VECTOR_TYPES_ALLOWED */
          default:
            unexpected_condition_str("walk_entry_and_subtree: bad type kind");
        }  /* switch */
#undef eptr
      }
      break;
    case iek_variable:
      {
#define eptr ((a_variable_ptr)entry_ptr)
        walk_source_corresp(eptr->source_corresp);
        remap_next_ptr(eptr->next, a_variable_ptr, iek_variable);
        walk_ptr(eptr->type, a_type_ptr, iek_type);
        definition_needed_if_class(eptr->type);
        if (eptr->is_struct_binding) {
          remap_ptr(eptr->variant.container, a_variable_ptr, iek_variable);
        } else if (eptr->is_struct_binding_container) {
          walk_list(eptr->variant.bindings, an_il_entity_list_entry_ptr,
                    iek_il_entity_list_entry);
        } else {
          remap_ptr_not_needed(eptr->variant.assoc_param_type,
                               a_param_type_ptr, iek_param_type);
        }  /* if */
        walk_initializer(eptr->init_kind, eptr->initializer,
                         eptr->is_member_constant);
        walk_list(eptr->entities_defined_in_initializer,
                  an_il_entity_list_entry_ptr, iek_il_entity_list_entry);
#if MICROSOFT_EXTENSIONS_ALLOWED
        walk_ptr(eptr->property_or_event_descr, a_property_or_event_descr_ptr,
                 iek_property_or_event_descr);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
        walk_string_ptr(eptr->section, iek_other_text, 0);
#endif /* GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
        walk_ptr(eptr->template_info, a_variable_template_info_ptr,
                 iek_variable_template_info);
#if GNU_EXTENSIONS_ALLOWED
        if (eptr->cleanup_routine != NULL) {
          remap_ptr(eptr->cleanup_routine, a_routine_ptr, iek_routine);
          set_proper_routine_definition_needed_flag(eptr->cleanup_routine);
        }  /* if */
        if (eptr->asm_name_is_valid) {
          walk_string_ptr(eptr->asm_name_or_reg.name, iek_other_text, 0);
        }  /* if */
        walk_ptr(eptr->aliased_variable, a_variable_ptr, iek_variable);
#endif /* GNU_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING
        /* This has to be iek_id_name because the name is copied from
           a variable name originally. */
        walk_string_ptr(eptr->comdat_group, iek_id_name, 0);
        remap_ptr(eptr->vla_element_count_variable, a_variable_ptr,
                  iek_variable);
#endif /* DO_IL_LOWERING */
#if GENERATE_SOURCE_SEQUENCE_LISTS
        walk_ptr(eptr->declared_type, a_type_ptr, iek_type);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if MINIMAL_INLINING
        conditionally_clear_fe_pointer(eptr->remapping_for_inlining);
#endif /* MINIMAL_INLINING */
#if SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
        if (!eptr->is_thread_local) {
          remap_ptr(eptr->init_routine.dynamic_init_routine,
                    a_routine_ptr, iek_routine);
        }  /* if */
#endif /* SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */
#if USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES
        if (eptr->is_thread_local) {
          remap_ptr(eptr->init_routine.thread.init_routine,
                    a_routine_ptr, iek_routine);
          remap_ptr(eptr->init_routine.thread.wrapper,
                    a_routine_ptr, iek_routine);
        }  /* if */
#endif /* USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES */
#undef eptr
      }
      break;
    case iek_variable_template_info:
      {
#define eptr ((a_variable_template_info_ptr)entry_ptr)
        walk_list(eptr->template_arg_list, a_template_arg_ptr,
                  iek_template_arg);
        walk_list(eptr->partial_spec_template_arg_list, a_template_arg_ptr,
                  iek_template_arg);
        remap_ptr(eptr->assoc_template, a_template_ptr, iek_template);
#undef eptr
      }
      break;
    case iek_field:
      {
#define eptr ((a_field_ptr)entry_ptr)
        walk_source_corresp(eptr->source_corresp);
        remap_next_ptr(eptr->next, a_field_ptr, iek_field);
        walk_ptr(eptr->type, a_type_ptr, iek_type);
        definition_needed_if_class(eptr->type);
        walk_ptr(eptr->initializer, a_dynamic_init_ptr, iek_dynamic_init);
        walk_list(eptr->entities_defined_in_initializer,
                  an_il_entity_list_entry_ptr, iek_il_entity_list_entry);
        walk_ptr(eptr->bit_size_constant, a_constant_ptr, iek_constant);
#if MICROSOFT_EXTENSIONS_ALLOWED
        walk_ptr(eptr->property_or_event_descr, a_property_or_event_descr_ptr,
                 iek_property_or_event_descr);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if BACK_END_IS_C_GEN_BE
        walk_ptr(eptr->bit_field_alignment_type, a_type_ptr, iek_type);
#endif /* BACK_END_IS_C_GEN_BE */
#undef eptr
      }
      break;
    case iek_exception_specification:
      {
#define eptr ((an_exception_specification_ptr)entry_ptr)
        if (eptr->arg_cached) {
          /* The exception specification was never required to be
             evaluated.  The token cache pointer is for front end use
             only. */
          conditionally_clear_fe_pointer(eptr->variant.token_cache);
        } else if (eptr->copy_from_prototype) {
          conditionally_clear_fe_pointer(eptr->variant.routine);
        } else if (eptr->is_noexcept) {
          walk_ptr(eptr->variant.noexcept_arg, a_constant_ptr, iek_constant);
        } else {
          walk_list(eptr->variant.exception_specification_type_list,
                    an_exception_specification_type_ptr,
                    iek_exception_specification_type);
        }  /* if */
#undef eptr
      }
      break;
    case iek_exception_specification_type:
      {
#define eptr ((an_exception_specification_type_ptr)entry_ptr)
        remap_next_ptr(eptr->next, an_exception_specification_type_ptr,
                       iek_exception_specification_type);
        walk_ptr(eptr->type, a_type_ptr, iek_type);
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
        /* If the exception specification is a class or a pointer to class,
           the class must be complete. */
        [] (a_type_ptr temp_type) {
          if (temp_type != NULL) {
            temp_type = skip_typerefs(temp_type);
            if (temp_type->kind == (a_type_kind)tk_pointer) {
              temp_type = temp_type->variant.pointer.type;
            }  /* if */
            definition_needed_if_class(temp_type);
          }  /* if */
        } (eptr->type);
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
#undef eptr
      }
      break;
    case iek_routine:
      {
#define eptr ((a_routine_ptr)entry_ptr)
        walk_source_corresp(eptr->source_corresp);
        remap_next_ptr(eptr->next, a_routine_ptr, iek_routine);
        walk_ptr(eptr->type, a_type_ptr, iek_type);
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
        /* If the routine is a virtual function with a covariant return
           type, the class type in the return type must be complete. */
        if (eptr->covariant_return_virtual_override) {
          [] (a_type_ptr temp_type) {
            temp_type = skip_typerefs(temp_type);
            check_assertion_str2(temp_type->kind == tk_routine,
                                 "walk_entry_and_subtree:",
                                 "type of virtual function is not tk_routine");
            temp_type = temp_type->variant.routine.return_type;
            temp_type = skip_typerefs(temp_type);
            check_assertion_str2(
                            temp_type->kind == tk_pointer,
                            "walk_entry_and_subtree:",
                            "return type of covariant virtual is not pointer");
            temp_type = temp_type->variant.pointer.type;
            definition_needed_if_class(temp_type);
          } (eptr->type);
        }
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
        /* assoc_scope points to a different memory region and is not
           walked automatically.  The entry_process_func can arrange
           to call walk_routine_scope_il if it wants to. */
        walk_list(eptr->template_arg_list, a_template_arg_ptr,
                  iek_template_arg);
        remap_ptr(eptr->assoc_template, a_template_ptr, iek_template);
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
        /* Note that we do not test "defined" here because defined gets cleared
           before some calls to walk the IL. */
        if (eptr->function_def_number != NULL_function_def_number) {
          /* This is a defined routine, so its return type must be complete. */
          definition_needed_if_class(
                       skip_typerefs(eptr->type)->variant.routine.return_type);
        }  /* if */
#if BACK_END_IS_CP_GEN_BE
        if (microsoft_mode && !C_mode() &&
            eptr->special_kind == (a_special_function_kind)sfk_conversion) {
          /* In Microsoft mode, a conversion function returning a
             pointer to a class requires that the class be complete so
             it can be compared against the return type of other conversion
             functions in the same class.  This is an issue in Microsoft mode
             because of an odd way of handling ambiguities (by throwing them
             away, which means that conversion functions that don't get called
             can affect the outcome of overload resolution; see x5949.C).
             The problem is only relevant in source-to-source applications,
             where overload resolution gets done again on the output of
             the front end. */
          [] (a_type_ptr return_type) {
            if (is_pointer_or_handle_type(return_type)) {
              return_type = type_pointed_to(return_type);
              if (is_class_struct_union_type(return_type)) {
                return_type = skip_typerefs(return_type);
                set_proper_definition_needed_flag(return_type);
              }  /* if */
            }  /* if */
          } (skip_typerefs(eptr->type)->variant.routine.return_type);
        }  /* if */
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
#if IA64_ABI && DO_IL_LOWERING
        if (eptr->special_kind == (a_special_function_kind)sfk_constructor ||
            eptr->special_kind == (a_special_function_kind)sfk_destructor) {
          conditionally_clear_fe_pointer(
                               eptr->variant.ctor_dtor.alternate_entry_points);
        }  /* if */
        remap_ptr(eptr->primary_ctor_or_dtor, a_routine_ptr, iek_routine);
#endif /* IA64_ABI && DO_IL_LOWERING */
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (rout_is_cli_accessor(eptr)) {
          remap_ptr(eptr->variant.property_or_event_descr,
                    a_property_or_event_descr_ptr,
                    iek_property_or_event_descr);
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        if (special_kind_is(eptr, sfk_lambda_entry_point)) {
          remap_ptr(eptr->variant.lambda_call_operator, a_routine_ptr,
                    iek_routine);
        }  /* if */
        walk_ptr(eptr->trailing_requires_clause, a_requires_clause_ptr,
                 iek_requires_clause);
#if MICROSOFT_EXTENSIONS_ALLOWED
        walk_list(eptr->overridden_functions, an_il_entity_list_entry_ptr,
                  iek_il_entity_list_entry);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* No processing of befriending_classes for the "needed" sweep. */
#if !NEEDED_FLAG_WALK
#if KEEP_IN_IL_WALK
        /* Visit befriending classes for the "keep_in_il" sweep. */
        set_keep_in_il_on_befriending_classes(rout_befriending_classes(eptr));
#else /* !KEEP_IN_IL_WALK */
        /* All cases except NEEDED_FLAG_WALK and KEEP_IN_IL_WALK. */
        if (!eptr->is_inheriting_ctor) {
          walk_list(eptr->friends_or_originator.befriending_classes,
                    a_class_list_entry_ptr, iek_class_list_entry);
        }  /* if */
#endif /* KEEP_IN_IL_WALK */
#endif /* !NEEDED_FLAG_WALK */
        if (eptr->is_inheriting_ctor) {
          walk_ptr_not_needed(eptr->friends_or_originator.inherited_routine,
                              a_routine_ptr, iek_routine);
        }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
        walk_ptr(eptr->declared_type, a_type_ptr, iek_type);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
        /* Default argument expressions are not walked at the iek_param_type
           level for the needed and keep-in-il walks.  Do them now.
           See the comment on walk_param_list_default_arg_exprs. */
        if (!C_mode()) {
          walk_param_list_default_arg_exprs(
                          skip_typerefs(eptr->type)->variant.routine.extra_info
                                                   ->param_type_list);
        }  /* if */
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
        /* The routines pointed to are not marked definition_needed at
           this point.  The overridden function definition is not
           needed at all.  The overriding function definition is needed
           only if the thunk definition is needed, and that's handled in
           set_routine_definition_needed. */
        remap_ptr(eptr->overriding_function_for_wrapper,
                  a_routine_ptr, iek_routine);
        remap_ptr_not_needed(
                  eptr->overridden_function_for_wrapper,
                  a_routine_ptr, iek_routine);
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
        walk_ptr(eptr->gnu_extra_info, a_gnu_routine_supplement_ptr,
                 iek_gnu_routine_supplement);
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */
#if !NEEDED_FLAG_WALK
        walk_ptr(eptr->generating_using_decl, a_using_decl_ptr,
                 iek_using_decl);
#endif /* !NEEDED_FLAG_WALK */
#undef eptr
      }
      break;
#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
    case iek_gnu_routine_supplement:
      {
#define eptr ((a_gnu_routine_supplement_ptr)entry_ptr)
        walk_string_ptr(eptr->section, iek_other_text, 0);
        walk_ptr(eptr->aliased_routine, a_routine_ptr, iek_routine);
        if (eptr->aliased_routine != NULL) {
          set_proper_routine_definition_needed_flag(eptr->aliased_routine);
        }  /* if */
#if LOWER_IFUNC
        remap_ptr_not_needed(eptr->resolver_var, a_variable_ptr, iek_variable);
#endif /* LOWER_IFUNC */
        walk_ptr(eptr->inline_partner, a_routine_ptr, iek_routine);
        if (eptr->inline_partner != NULL) {
          set_proper_routine_definition_needed_flag(eptr->inline_partner);
        }  /* if */
        walk_string_ptr(eptr->asm_name, iek_other_text, 0);
#if GNU_FUNCTION_MULTIVERSIONING
        if (eptr->is_representative) {
          walk_list_not_needed(eptr->mv_info.representative.targeted_versions,
                               a_routine_list_entry_ptr,
                               iek_routine_list_entry);
        } else if (eptr->is_target_specific_version) {
          walk_ptr(eptr->mv_info.targeted_version.representative,
                   a_routine_ptr, iek_routine);
        }  /* if */
#endif /* GNU_FUNCTION_MULTIVERSIONING */
#undef eptr
      }
      break;
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */
    case iek_label:
      {
#define eptr ((a_label_ptr)entry_ptr)
        walk_source_corresp(eptr->source_corresp);
        remap_next_ptr(eptr->next, a_label_ptr, iek_label);
        remap_ptr_not_needed(eptr->exec_stmt, a_statement_ptr, iek_statement);
#undef eptr
      }
      break;
    case iek_expr_node:
      {
#define eptr ((an_expr_node_ptr)entry_ptr)
        walk_ptr(eptr->type, a_type_ptr, iek_type);
        walk_ptr(eptr->orig_lvalue_type, a_type_ptr, iek_type);
        if (!is_glvalue_node(eptr)) {
          definition_needed_if_class(eptr->type);
        }  /* if */
        remap_next_ptr(eptr->next, an_expr_node_ptr, iek_expr_node);
        conditionally_clear_fe_pointer(eptr->extra.rescan_info);
        switch (eptr->kind) {
          case enk_error:
            /* No pointers. */
            break;
          case enk_operation:
            walk_list(eptr->variant.operation.operands, an_expr_node_ptr,
                      iek_expr_node);
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
            /* Certain operators on pointers require that the type pointed
               to be complete. */
            [] (an_expr_node_ptr expr) {
              a_type_ptr optype;
              a_type_ptr op1_type = expr->variant.operation.operands->type;

              switch (expr->variant.operation.kind) {
                case eok_psubtract:
                case eok_pdiff:
                case eok_post_incr:
                case eok_post_decr:
                case eok_pre_incr:
                case eok_pre_decr:
                case eok_padd_assign:
                case eok_psubtract_assign:
                  /* First operand may be a pointer. */
                  if (!is_pointer_type(op1_type)) break;
                  optype = type_pointed_to(op1_type);
                  goto do_definition_needed_if_class;
                case eok_points_to_vacuous_destructor_call:
                case eok_cli_subscript:
                  /* First operand may be a pointer or handle. */
                  if (!is_pointer_or_handle_type(op1_type)) break;
                  optype = type_pointed_to(op1_type);
                  goto do_definition_needed_if_class;
                case eok_subscript:
                case eok_padd:
                  optype = expr->variant.operation.pointer_operand_is_second ?
                             expr->variant.operation.operands->next->type :
                             op1_type;
                  if (!is_pointer_or_handle_type(optype)) break;
                  optype = type_pointed_to(optype);
do_definition_needed_if_class:
                  definition_needed_if_class(optype);
                  break;
                case eok_dynamic_cast:
                  /* Destination class (pointed to by result type) must be
                     complete.  Watch out for the case where the result type
                     is "void *", and watch out for prototype instantiation
                     cases. */
                  if (is_any_ptr_or_ref_type(expr->type)) {
                    optype = type_pointed_to(expr->type);
                    definition_needed_if_class(optype);
                  }  /* if */
                  /* Source type must also be complete, but watch out for
                     prototype instantiation cases where the first operand
                     isn't a pointer to class. */
                  if (!is_pointer_or_handle_type(op1_type) ||
                      !is_class_struct_union_type(type_pointed_to(op1_type))) {
                    break;
                  }  /* if */
                  optype = f_skip_typerefs(type_pointed_to(op1_type));
                  goto do_set_proper_definition_needed_flag;
                case eok_ref_dynamic_cast:
                  /* Destination class type must be complete. */
                  definition_needed_if_class(expr->type);
                  /* Source type must also be complete, but watch out for
                     prototype instantiation cases where the first operand
                     isn't a class. */
                  if (!is_class_struct_union_type(op1_type)) {
                    break;
                  }  /* if */
                  optype = f_skip_typerefs(op1_type);
                  goto do_set_proper_definition_needed_flag;
                case eok_base_class_cast:
                  /* First operand must be a complete class (lvalue, rvalue,
                     or pointer to class). */
                  optype = op1_type;
                  goto do_related_class_cast_set_definition_needed;
                case eok_derived_class_cast:
                  /* Destination type must be a complete class (or pointer
                     to complete class). */
                  optype = expr->type;
do_related_class_cast_set_definition_needed:
                  if (is_pointer_or_handle_type(optype)) {
                    optype = type_pointed_to(optype);
                  }  /* if */
                  optype = skip_typerefs(optype);
                  goto do_set_proper_definition_needed_flag;
                case eok_pm_base_class_cast:
                  /* First operand is a pointer to member. */
                  optype = pm_class_type_possibly_lowered(op1_type);
                  goto do_set_proper_definition_needed_flag;
                case eok_pm_derived_class_cast:
                  /* Destination class (pointed to by result type) must be
                     complete. */
                  optype = pm_class_type_possibly_lowered(expr->type);
do_set_proper_definition_needed_flag:
                  set_proper_definition_needed_flag(optype);
                  break;
                default:
                  break;
              }  /* switch */
            } (eptr);
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
            break;
          case enk_constant:
            walk_ptr(eptr->variant.constant.ptr, a_constant_ptr, iek_constant);
            walk_ptr(eptr->variant.constant.name_reference,
                     a_name_reference_ptr, iek_name_reference);
            break;
          case enk_variable:
            /* Variables are handled from the scope that contains them.  Do
               not visit them here. */
            remap_ptr(eptr->variant.variable.ptr, a_variable_ptr,
                      iek_variable);
            walk_ptr(eptr->variant.variable.name_reference,
                     a_name_reference_ptr, iek_name_reference);
            break;
          case enk_routine:
            /* Functions are handled from the scope that contains them.  Do
               not visit them here. */
            if (node_routine(eptr) != NULL) {
              remap_ptr(node_routine(eptr), a_routine_ptr, iek_routine);
              set_proper_routine_definition_needed_flag(node_routine(eptr));
            }  /* if */
            walk_ptr(eptr->variant.routine.name_reference,
                     a_name_reference_ptr, iek_name_reference);
            break;
          case enk_field:
            /* Fields are handled in processing the tag that contains
               them. */
            remap_ptr(eptr->variant.field.ptr, a_field_ptr, iek_field);
            walk_ptr(eptr->variant.field.name_reference,
                     a_name_reference_ptr, iek_name_reference);
            break;
          case enk_temp_init:
            walk_ptr(eptr->variant.init.dynamic_init,
                     a_dynamic_init_ptr, iek_dynamic_init);
            walk_ptr(eptr->variant.init.source.type, a_type_ptr, iek_type);
            /* The type of the temporary requires a definition. */
            definition_needed_if_class(eptr->type);
            break;
          case enk_lambda:
            walk_ptr(eptr->variant.init.source.lambda, a_lambda_ptr,
                     iek_lambda);
            walk_ptr(eptr->variant.init.dynamic_init,
                     a_dynamic_init_ptr, iek_dynamic_init);
            break;
          case enk_new_delete:
            walk_ptr(eptr->variant.new_delete, a_new_delete_supplement_ptr,
                     iek_new_delete_supplement);
            break;
#if MICROSOFT_EXTENSIONS_ALLOWED
          case enk_gcnew:
            walk_ptr(eptr->variant.gcnew_info, a_gcnew_supplement_ptr,
                     iek_gcnew_supplement);
            break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          case enk_throw:
            walk_ptr(eptr->variant.throw_info, a_throw_supplement_ptr,
                     iek_throw_supplement);
            break;
          case enk_condition:
            walk_ptr(eptr->variant.condition, a_condition_supplement_ptr,
                     iek_condition_supplement);
            break;
          case enk_object_lifetime:
            walk_ptr(eptr->variant.object_lifetime.expr, an_expr_node_ptr,
                     iek_expr_node);
            remap_ptr_not_needed(eptr->variant.object_lifetime.ptr,
                                 an_object_lifetime_ptr, iek_object_lifetime);
            break;
          case enk_typeid:
            walk_list(eptr->variant.typeid_info.type_with_opt_expr,
                      an_expr_node_ptr, iek_expr_node);
#if MICROSOFT_EXTENSIONS_ALLOWED
            if (!eptr->is_cli_typeid)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
            /* Do not insert code here. */
            {
              /* Make sure the definition of type_info is retained, even though
                 the node only uses a pointer to it.  This is necessary with
                 cp_gen_be output. */
              set_proper_definition_needed_flag(
                                  f_skip_typerefs(eptr->type)); /*lint !e2666*/
            }  /* if */
            break;
          case enk_sizeof:
          case enk_datasizeof:
          case enk_alignof:
            if (eptr->variant.sizeof_info.is_type) {
              walk_ptr(eptr->variant.sizeof_info.variant.type, a_type_ptr,
                       iek_type);
              definition_needed_if_class(
                                       eptr->variant.sizeof_info.variant.type);
            } else {
              walk_ptr(eptr->variant.sizeof_info.variant.expr,
                       an_expr_node_ptr, iek_expr_node);
              /* If the expression is an lvalue, make sure its type's
                 definition is kept. */
              definition_needed_if_class(eptr->
                                       variant.sizeof_info.variant.expr->type);
            }  /* if */
            break;
          case enk_sizeof_pack:
            if (eptr->variant.sizeof_pack.is_template_template) {
              walk_ptr(eptr->variant.sizeof_pack.variant.templ, a_template_ptr,
                       iek_template);
            } else if (eptr->variant.sizeof_pack.is_type) {
              walk_ptr(eptr->variant.sizeof_pack.variant.type, a_type_ptr,
                       iek_type);
            } else {
              walk_ptr(eptr->variant.sizeof_pack.variant.expr,
                       an_expr_node_ptr, iek_expr_node);
            }  /* if */
            break;
          case enk_address_of_ellipsis:
            /* No pointers. */
            break;
          case enk_statement:
            walk_ptr(eptr->variant.statement, a_statement_ptr, iek_statement);
            break;
          case enk_reuse_value:
            remap_ptr_not_needed(eptr->variant.reused_value_init,
                                 a_dynamic_init_ptr,
                                 iek_dynamic_init);
            break;
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
          /* Nodes generated by IL lowering for partial lowering of exception
             handling features. */
          case enk_lowered_eh_construct:
            switch (eptr->variant.lowered_eh.kind) {
              case leck_caught_object_address:
                remap_ptr_not_needed(eptr->variant.lowered_eh.variant.
                                                         caught_object_handler,
                                     a_handler_ptr, iek_handler);
                break;
              case leck_thrown_object_address:
                /* No pointers. */
                break;
              case leck_cleanup_state:
              case leck_unreachable_cleanup_state:
#if !GENERATE_EH_TABLES
                remap_ptr_not_needed(eptr->variant.lowered_eh.variant.
                                                                   cleanup_ptr,
                                     a_dynamic_init_ptr, iek_dynamic_init);
#endif /* !GENERATE_EH_TABLES */
                break;
              case leck_function_prologue:
                walk_ptr(eptr->variant.lowered_eh.variant.prologue_info,
                         an_eh_prologue_supplement_ptr,
                         iek_eh_prologue_supplement);
                break;
              case leck_function_epilogue:
                remap_ptr_not_needed(
                          eptr->variant.lowered_eh.variant.epilogue_routine,
                          a_routine_ptr, iek_routine);
                break;
              case leck_catch_epilogue:
                remap_ptr_not_needed(
                          eptr->variant.lowered_eh.variant.epilogue_handler,
                          a_handler_ptr, iek_handler);
                break;
              case leck_try_epilogue:
                remap_ptr_not_needed(
                          eptr->variant.lowered_eh.variant.epilogue_try_block,
                          a_try_supplement_ptr, iek_try_supplement);
                break;
              case leck_exception_caught:
              case leck_exception_started:
                /* No pointers. */
                break;
#if !GENERATE_EH_TABLES
              case leck_initialization_completed:
                remap_ptr_not_needed(eptr->variant.lowered_eh.variant.
                                                                  dynamic_init,
                                     a_dynamic_init_ptr, iek_dynamic_init);
                break;
#endif /* !GENERATE_EH_TABLES */
              case leck_internal_try:
                walk_list(eptr->variant.lowered_eh.variant.try_and_catch_expr,
                           an_expr_node_ptr, iek_expr_node);
                break;
              default:
                unexpected_condition_str(
                      "walk_entry_and_subtree: bad lowered eh construct kind");
            }  /* switch */
            break;
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
          case enk_result_of_overriding_function:
            /* Node generated as part of the body of an entry function used
               as a wrapper for a call of an overriding virtual function
               with a covariant return type. */
            break;
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
#if VLA_DEALLOCATIONS_IN_IL
          case enk_vla_dealloc:
            remap_ptr(eptr->variant.vla_variable, a_variable_ptr,
                      iek_variable);
            break;
#endif /* VLA_DEALLOCATIONS_IN_IL */
          case enk_type_operand:
            walk_ptr(eptr->variant.type_operand.type, a_type_ptr, iek_type);
            walk_ptr(eptr->variant.type_operand.name_reference,
                     a_name_reference_ptr, iek_name_reference);
            if (eptr->type_definition_needed) {
              definition_needed_if_class(eptr->variant.type_operand.type);
            }  /* if */
            break;
          case enk_builtin_operation:
            walk_list(eptr->variant.builtin_operation.operands,
                      an_expr_node_ptr, iek_expr_node);
            break;
          case enk_param_ref:
            /* No variant-specific fields to traverse. */
            break;
          case enk_braced_init_list:
            walk_list(eptr->variant.braced_init_list,
                      an_expr_node_ptr, iek_expr_node);
            break;
          case enk_c11_generic:
            walk_list(eptr->variant.c11_generic.operands,
                      an_expr_node_ptr, iek_expr_node);
            remap_ptr(eptr->variant.c11_generic.result,
                      an_expr_node_ptr, iek_expr_node);
            break;
#if BUILTIN_FUNCTIONS_ENABLED
          case enk_builtin_choose_expr:
            walk_list(eptr->variant.builtin_choose_expr.operands,
                      an_expr_node_ptr, iek_expr_node);
            break;
#endif /* BUILTIN_FUNCTIONS_ENABLED */
          case enk_await:
          case enk_yield:
            walk_ptr(eptr->variant.await_info.operand, an_expr_node_ptr,
                     iek_expr_node);
            walk_list(eptr->variant.await_info.ready_resume_suspend,
                      an_expr_node_ptr, iek_expr_node);
            break;
          case enk_fold:
            walk_list(eptr->variant.fold.operands, an_expr_node_ptr,
                      iek_expr_node);
            break;
          case enk_initializer:
            walk_ptr(eptr->variant.initializer.dyn_init,
                     a_dynamic_init_ptr, iek_dynamic_init);
            break;
          case enk_concept_id:
            walk_ptr(eptr->variant.concept_id.concept_template, a_template_ptr,
                     iek_template);
            walk_list(eptr->variant.concept_id.args, a_template_arg_ptr,
                      iek_template_arg);
            break;
          case enk_requires:
            walk_list(eptr->variant.requires_expr.requirements,
                      an_expr_node_ptr, iek_expr_node);
            walk_list(eptr->variant.requires_expr.parameters, a_param_type_ptr,
                      iek_param_type);
            break;
          case enk_compound_req:
            walk_list(eptr->variant.compound_req.expr_and_constraint,
                      an_expr_node_ptr, iek_expr_node);
            break;
          case enk_nested_req:
            walk_ptr(eptr->variant.nested_req.constraint, an_expr_node_ptr,
                     iek_expr_node);
            break;
          case enk_const_eval_deferred:
            walk_ptr(eptr->variant.const_eval_deferred.wrapped,
                     an_expr_node_ptr, iek_expr_node);
            break;
          case enk_template_name:
            walk_ptr(eptr->variant.template_name, a_template_ptr,
                     iek_template);
            break;
          case enk_token_sequence:
            walk_list(eptr->variant.token_sequence.interpolations,
                      an_expr_node_ptr, iek_expr_node);
            walk_ptr(eptr->variant.token_sequence.tokens,
                     a_token_sequence_ptr, iek_token_sequence);
            break;
          case enk_pack_index:
            walk_ptr(eptr->variant.pack_index.expr, an_expr_node_ptr,
                     iek_expr_node);
            walk_ptr(eptr->variant.pack_index.index_expr, an_expr_node_ptr,
                     iek_expr_node);
            break;
          default:
            unexpected_condition_str(
                                 "walk_entry_and_subtree: bad expr node kind");
        }  /* switch */
#undef eptr
      }
      break;
    case iek_for_loop:
      {
#define eptr ((a_for_loop_ptr)entry_ptr)
        walk_ptr(eptr->initialization, a_statement_ptr, iek_statement);
        walk_ptr(eptr->increment, an_expr_node_ptr, iek_expr_node);
        walk_ptr(eptr->for_init_scope, a_scope_ptr, iek_scope);
#if UPC_EXTENSIONS_ALLOWED
        walk_ptr(eptr->affinity, an_expr_node_ptr, iek_expr_node);
#endif /* UPC_EXTENSIONS_ALLOWED */
#undef eptr
      }
      break;
    case iek_range_based_for_loop:
      {
#define eptr ((a_range_based_for_loop_ptr)entry_ptr)
        walk_ptr(eptr->initialization, a_statement_ptr, iek_statement);
        remap_ptr(eptr->iterator, a_variable_ptr, iek_variable);
        remap_ptr(eptr->range, a_variable_ptr, iek_variable);
        walk_ptr(eptr->range_based_for_scope, a_scope_ptr, iek_scope);
        walk_ptr(eptr->iterator_scope, a_scope_ptr, iek_scope);
        remap_ptr(eptr->begin, a_variable_ptr, iek_variable);
        remap_ptr(eptr->end, a_variable_ptr, iek_variable);
        walk_ptr(eptr->ne_call_expr, an_expr_node_ptr, iek_expr_node);
        walk_ptr(eptr->incr_call_expr, an_expr_node_ptr, iek_expr_node);
#undef eptr
      }
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case iek_for_each_loop:
      {
#define eptr ((a_for_each_loop_ptr)entry_ptr)
        if (!eptr->uses_prev_decl_iterator) {
          remap_ptr(eptr->iterator.variable, a_variable_ptr, iek_variable);
        } else {
          remap_ptr(eptr->iterator.prev_decl.variable, a_variable_ptr,
                    iek_variable);
          remap_ptr(eptr->iterator.prev_decl.field, a_field_ptr,
                    iek_field);
          walk_ptr(eptr->iterator.prev_decl.assign_expr,
                   an_expr_node_ptr, iek_expr_node);
        }  /* if */
        remap_ptr(eptr->collection_expr_ref, a_variable_ptr, iek_variable);
        walk_ptr(eptr->for_each_scope, a_scope_ptr, iek_scope);
        walk_ptr(eptr->iterator_scope, a_scope_ptr, iek_scope);
        remap_ptr(eptr->temporary_variable, a_variable_ptr,
                  iek_variable);
        switch (eptr->kind) {
          case sfepk_none:
            break;
          case sfepk_array_pattern:
          case sfepk_stl_pattern:
            remap_ptr(eptr->variant.stl_array_pattern.end_variable,
                      a_variable_ptr, iek_variable);
            walk_ptr(eptr->variant.stl_array_pattern.ne_call_expr,
                     an_expr_node_ptr, iek_expr_node);
            walk_ptr(eptr->variant.stl_array_pattern.incr_call_expr,
                     an_expr_node_ptr, iek_expr_node);
            break;
          case sfepk_cli_pattern:
            walk_ptr(eptr->variant.cli_pattern.movenext_call_expression,
                     an_expr_node_ptr, iek_expr_node);
            break;
          case sfepk_cli_array_pattern:
            remap_ptr(eptr->variant.cli_array_pattern.upper_bound_vars,
                      a_variable_ptr, iek_variable);
            remap_ptr(eptr->variant.cli_array_pattern.loop_vars,
                      a_variable_ptr, iek_variable);
            break;
          default:
            unexpected_condition_str(
                                 "walk_entry_and_subtree: bad for each kind");
        }  /* switch */
#undef eptr
      }
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case iek_switch_case_entry:
      {
#define eptr ((a_switch_case_entry_ptr)entry_ptr)
#if !NEEDED_FLAG_WALK && !KEEP_IN_IL_WALK
        remap_ptr(eptr->stmt, a_statement_ptr, iek_statement);
#endif /* !NEEDED_FLAG_WALK && !KEEP_IN_IL_WALK */
        walk_ptr(eptr->case_value, a_constant_ptr, iek_constant);
#if GNU_EXTENSIONS_ALLOWED
        walk_ptr(eptr->range_end, a_constant_ptr, iek_constant);
#endif /* GNU_EXTENSIONS_ALLOWED */
        remap_next_ptr(eptr->next, a_switch_case_entry_ptr,
                       iek_switch_case_entry);
#if !NEEDED_FLAG_WALK && !KEEP_IN_IL_WALK
        remap_ptr(eptr->next_on_sorted_list, a_switch_case_entry_ptr,
                  iek_switch_case_entry);
#endif /* !NEEDED_FLAG_WALK && !KEEP_IN_IL_WALK */
#undef eptr
      }
      break;
    case iek_switch_stmt_descr:
      {
#define eptr ((a_switch_stmt_descr_ptr)entry_ptr)
        walk_list(eptr->cases, a_switch_case_entry_ptr, iek_switch_case_entry);
        remap_ptr(eptr->default_case, a_switch_case_entry_ptr,
                 iek_switch_case_entry);
        remap_list_ptr(eptr->sorted_cases, a_switch_case_entry_ptr,
                       iek_switch_case_entry);
#undef eptr
      }
      break;
    case iek_handler:
      {
#define eptr ((a_handler_ptr)entry_ptr)
        remap_next_ptr(eptr->next, a_handler_ptr, iek_handler);
#if NEEDED_FLAG_WALK
        walk_ptr(eptr->parameter, a_variable_ptr, iek_variable);
#else /* !NEEDED_FLAG_WALK */
        /* The associated parameter, if any, will appear on the variables
           list of the current scope.  Therefore, here we just remap the
           pointer but do not walk the subtree. */
        remap_ptr(eptr->parameter, a_variable_ptr, iek_variable);
#endif /* NEEDED_FLAG_WALK */
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
        [] (a_variable_ptr parameter) {
          if (parameter != NULL) {
            a_type_ptr param_type = parameter->type;
            if (is_any_ptr_or_ref_type(param_type)) {
              param_type = type_pointed_to(param_type);
              definition_needed_if_class(param_type);
            }  /* if */
          }  /* if */
        } (eptr->parameter);
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
        walk_ptr(eptr->statement, a_statement_ptr, iek_statement);
        walk_ptr(eptr->dynamic_init, a_dynamic_init_ptr, iek_dynamic_init);
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
        walk_ptr(eptr->typeinfo_var, a_variable_ptr, iek_variable);
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#undef eptr
      }
      break;
    case iek_try_supplement:
      {
#define eptr ((a_try_supplement_ptr)entry_ptr)
        walk_ptr(eptr->statement, a_statement_ptr, iek_statement);
        walk_list(eptr->handlers, a_handler_ptr, iek_handler);
#if MICROSOFT_EXTENSIONS_ALLOWED
        walk_ptr(eptr->finally_statement, a_statement_ptr, iek_statement);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        remap_ptr_not_needed(eptr->lifetime, an_object_lifetime_ptr,
                             iek_object_lifetime);
#undef eptr
      }
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case iek_microsoft_try_supplement:
      {
#define eptr ((a_microsoft_try_supplement_ptr)entry_ptr)
        walk_ptr(eptr->guarded_statement, a_statement_ptr, iek_statement);
        walk_ptr(eptr->except_expr, an_expr_node_ptr, iek_expr_node);
        walk_ptr(eptr->cleanup_statement, a_statement_ptr, iek_statement);
#undef eptr
      }
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case iek_block:
      {
#define eptr ((a_block_ptr)entry_ptr)
#if !NEEDED_FLAG_WALK
        if (eptr->is_statement_expression) {
          /* The block scope for a statement expression that appears in a
             nonlocal scope (in an unevaluated context) doesn't appear on any
             scopes list.  So it must be walked here. */
          walk_ptr(eptr->assoc_scope, a_scope_ptr, iek_scope);
          walk_ptr(eptr->lifetime, an_object_lifetime_ptr,
                   iek_object_lifetime);
        } else {
          /* The associated scope, if any, will appear on the list of local
             scopes for the current scope.  Therefore, here we just remap
             the pointer but do not walk the subtree. */
          remap_ptr_not_needed(eptr->assoc_scope, a_scope_ptr, iek_scope);
          remap_ptr_not_needed(eptr->lifetime, an_object_lifetime_ptr,
                               iek_object_lifetime);
        }  /* if */
#endif /* !NEEDED_FLAG_WALK */
#undef eptr
      }
      break;
    case iek_statement:
      /* Statements account for more than 10% of the IL nodes (they're the
         second most common, after expr nodes), so do not call a subroutine
         for them. */
      {
#define eptr ((a_statement_ptr)entry_ptr)
        remap_next_ptr(eptr->next, a_statement_ptr, iek_statement);
        remap_ptr_not_needed(eptr->parent, a_statement_ptr, iek_statement);
        walk_list(eptr->attributes, an_attribute_ptr, iek_attribute);
#if GENERATE_SOURCE_SEQUENCE_LISTS && !NEEDED_FLAG_WALK && !KEEP_IN_IL_WALK
        remap_ptr(eptr->source_sequence_entry,
                  a_source_sequence_entry_ptr,
                  iek_source_sequence_entry);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS && ... */
        walk_ptr(eptr->expr, an_expr_node_ptr, iek_expr_node);
        switch (eptr->kind) {
          case stmk_empty:
          case stmk_expr:
#if GNU_EXTENSIONS_ALLOWED
          case stmk_assigned_goto:
#endif /* GNU_EXTENSIONS_ALLOWED */
#if UPC_EXTENSIONS_ALLOWED
          case stmk_upc_notify:
          case stmk_upc_wait:
          case stmk_upc_barrier:
          case stmk_upc_fence:
#endif /* UPC_EXTENSIONS_ALLOWED */
          case stmk_coroutine_return:
            /* No additional pointers. */
            break;
          case stmk_if:
          case stmk_if_consteval:
          case stmk_if_not_consteval:
            walk_ptr(eptr->variant.if_stmt.then_statement, a_statement_ptr,
                     iek_statement);
            walk_ptr(eptr->variant.if_stmt.else_statement, a_statement_ptr,
                     iek_statement);
            break;
          case stmk_constexpr_if:
            walk_ptr(eptr->variant.constexpr_if, a_constexpr_if_ptr,
                     iek_constexpr_if);
            break;
          case stmk_while:
          case stmk_end_test_while:
            walk_ptr(eptr->variant.loop_statement, a_statement_ptr,
                     iek_statement);
            break;
          case stmk_goto:
            remap_ptr(eptr->variant.label.ptr, a_label_ptr, iek_label);
            remap_ptr_not_needed(eptr->variant.label.lifetime,
                                 an_object_lifetime_ptr, iek_object_lifetime);
            break;
          case stmk_label:
            remap_ptr_not_needed(eptr->variant.label.ptr, a_label_ptr,
                                 iek_label);
            remap_ptr_not_needed(eptr->variant.label.lifetime,
                                 an_object_lifetime_ptr, iek_object_lifetime);
            break;
          case stmk_return:
            walk_ptr(eptr->variant.return_dynamic_init, a_dynamic_init_ptr,
                     iek_dynamic_init);
            break;
          case stmk_coroutine:
            walk_ptr(eptr->variant.coroutine.descr, a_coroutine_descr_ptr,
                     iek_coroutine_descr);
            break;
          case stmk_block:
            /* Do extra_info before statements to get declarations out
               before the statements that use them. */
            walk_ptr(eptr->variant.block.extra_info, a_block_ptr, iek_block);
            walk_list(eptr->variant.block.statements, a_statement_ptr,
                      iek_statement);
            break;
#if UPC_EXTENSIONS_ALLOWED
          /* The upc_forall statement is handled like a for statement. */
          case stmk_upc_forall:
#endif /* UPC_EXTENSIONS_ALLOWED */
          case stmk_for:
            walk_ptr(eptr->variant.for_loop.extra_info, a_for_loop_ptr,
                     iek_for_loop);
            walk_ptr(eptr->variant.for_loop.statement, a_statement_ptr,
                     iek_statement);
            break;
          case stmk_range_based_for:
            walk_ptr(eptr->variant.range_based_for_loop.extra_info,
                     a_range_based_for_loop_ptr, iek_range_based_for_loop);
            walk_ptr(eptr->variant.range_based_for_loop.statement,
                     a_statement_ptr, iek_statement);
            break;
#if MICROSOFT_EXTENSIONS_ALLOWED
          case stmk_for_each:
            walk_ptr(eptr->variant.for_each_loop.extra_info,
                     a_for_each_loop_ptr, iek_for_each_loop);
            walk_ptr(eptr->variant.for_each_loop.statement,
                     a_statement_ptr, iek_statement);
            break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          case stmk_switch_case:
            remap_ptr_not_needed(eptr->variant.switch_case.switch_statement,
                                 a_statement_ptr, iek_statement);
            walk_ptr(eptr->variant.switch_case.extra_info,
                     a_switch_case_entry_ptr, iek_switch_case_entry);
            break;
          case stmk_switch:
            walk_ptr(eptr->variant.switch_stmt.body_statement, a_statement_ptr,
                     iek_statement);
            walk_ptr(eptr->variant.switch_stmt.extra_info,
                     a_switch_stmt_descr_ptr, iek_switch_stmt_descr);
            break;
          case stmk_init:
            remap_ptr(eptr->variant.dynamic_init, a_dynamic_init_ptr,
                      iek_dynamic_init);
            break;
          case stmk_asm:
            walk_ptr(eptr->variant.asm_entry, an_asm_entry_ptr,
                     iek_asm_entry);
            break;
#if ASM_FUNCTION_ALLOWED
          case stmk_asm_func_body:
            walk_string_ptr(eptr->variant.asm_func_body, iek_other_text, 0);
            break;
#endif /* ASM_FUNCTION_ALLOWED */
          case stmk_try_block:
            walk_ptr(eptr->variant.try_block, a_try_supplement_ptr,
                     iek_try_supplement);
            break;
#if MICROSOFT_EXTENSIONS_ALLOWED
          case stmk_microsoft_try:
            walk_ptr(eptr->variant.microsoft_try,
                     a_microsoft_try_supplement_ptr,
                     iek_microsoft_try_supplement);
            break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          case stmk_decl:
            walk_list(eptr->variant.decl.entities, an_il_entity_list_entry_ptr,
                      iek_il_entity_list_entry);
            break;
          case stmk_set_vla_size:
            remap_ptr(eptr->variant.vla_dimension, a_vla_dimension_ptr,
                      iek_vla_dimension);
            break;
          case stmk_vla_decl:
            if (eptr->variant.vla.is_typedef_decl) {
              remap_ptr(eptr->variant.vla.variant.typedef_type, a_type_ptr,
                        iek_type);
            } else {
              remap_ptr(eptr->variant.vla.variant.variable, a_variable_ptr,
                        iek_variable);
            }  /* if */
            break;
          case stmk_stmt_expr_result:
            walk_ptr(eptr->variant.stmt_expr_result.dynamic_init,
                     a_dynamic_init_ptr, iek_dynamic_init);
            break;
          default:
            unexpected_condition_str(
                                 "walk_entry_and_subtree: bad statement kind");
        }  /* switch */
#undef eptr
      }
      break;
    case iek_pragma:
      {
#define eptr ((a_pragma_ptr)entry_ptr)
        remap_next_ptr(eptr->next, a_pragma_ptr, iek_pragma);
        remap_ptr_not_needed(eptr->entity.ptr, a_char_ptr,
                             (an_il_entry_kind)eptr->entity.kind);
#if GENERATE_SOURCE_SEQUENCE_LISTS && !NEEDED_FLAG_WALK && !KEEP_IN_IL_WALK
        remap_ptr(eptr->source_sequence_entry, a_source_sequence_entry_ptr,
                  iek_source_sequence_entry);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS && ... */
        walk_string_ptr(eptr->pragma_text, iek_other_text, 0);
#if IDENT_DIRECTIVE_AND_PRAGMA
        if (eptr->kind == (a_pragma_kind)pk_ident_directive) {
          walk_ptr(eptr->variant.ident_string, a_constant_ptr, iek_constant);
        }  /* if */
#endif /* IDENT_DIRECTIVE_AND_PRAGMA */
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (eptr->kind == (a_pragma_kind)pk_comment) {
          walk_ptr(eptr->variant.comment.str, a_constant_ptr, iek_constant);
        } else if (eptr->kind == (a_pragma_kind)pk_conform) {
          walk_string_ptr(eptr->variant.conform.identifier, iek_other_text, 0);
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#undef eptr
      }
      break;
#if RECORD_HIDDEN_NAMES_IN_IL
#if !NEEDED_FLAG_WALK
    case iek_hidden_name:
      {
#define eptr ((a_hidden_name_ptr)entry_ptr)
        remap_next_ptr(eptr->next, a_hidden_name_ptr, iek_hidden_name);
        walk_ptr(eptr->entity.ptr, a_char_ptr,
                 (an_il_entry_kind)eptr->entity.kind);
#undef eptr
      }
      break;
#endif /* !NEEDED_FLAG_WALK */
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
    case iek_template_parameter:
      {
#define eptr ((a_template_parameter_ptr)entry_ptr)
        walk_source_corresp(eptr->source_corresp);
        remap_next_ptr(eptr->next, a_template_parameter_ptr,
                       iek_template_parameter);
        switch (eptr->kind) {
          case tpk_error:
            break;
          case tpk_type:
            walk_ptr(eptr->variant.type.ptr, a_type_ptr, iek_type);
            walk_ptr(eptr->variant.type.default_arg_type, a_type_ptr,
                     iek_type);
            break;
          case tpk_nontype:
            walk_ptr(eptr->variant.nontype.constant, a_constant_ptr,
                     iek_constant);
            walk_ptr(eptr->variant.nontype.default_arg_constant,
                     a_constant_ptr, iek_constant);
            break;
          case tpk_template:
            walk_ptr(eptr->variant.templ.class_template, a_template_ptr,
                     iek_template);
            walk_ptr(eptr->variant.templ.default_arg_template, a_template_ptr,
                     iek_template);
            break;
          default:
            unexpected_condition_str("unexpected template parameter kind");
        }  /* switch */
#undef eptr
      }
      break;
    case iek_requires_clause:
#define eptr ((a_requires_clause_ptr)entry_ptr)
      walk_ptr(eptr->constraint, an_expr_node_ptr, iek_expr_node);
#undef eptr
      break;
    case iek_template_decl:
      {
#define eptr ((a_template_decl_ptr)entry_ptr)
        walk_ptr(eptr->parent, a_template_decl_ptr, iek_template_decl);
        walk_list(eptr->param_list, a_template_parameter_ptr,
                  iek_template_parameter);
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (eptr->is_generic) {
          walk_list(eptr->constraint.where_clauses,
                    a_generic_constraint_clause_ptr,
                    iek_generic_constraint_clause);
        } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Do not insert code here. */
        {
          walk_ptr(eptr->constraint.requires_clause, a_requires_clause_ptr,
                   iek_requires_clause);
        }  /* if */
        walk_ptr(eptr->scope, a_scope_ptr, iek_scope);
#undef eptr
      }
      break;
    case iek_template:
      {
#define eptr ((a_template_ptr)entry_ptr)
        /* Template template parameters have a parent scope that is in the
           template declaration scope, so they require a walk_ptr for the
           parent. */
        walk_source_corresp_full(eptr->source_corresp,
              (eptr->kind == (a_template_kind)templk_template_template_param));
        remap_next_ptr(eptr->next, a_template_ptr, iek_template);
#if RECORD_TEMPLATE_STRINGS
        walk_string_ptr(eptr->text, iek_other_text, 0);
#endif /* RECORD_TEMPLATE_STRINGS */
        walk_ptr(eptr->template_decl, a_template_decl_ptr, iek_template_decl);
        switch (eptr->kind) {
          case templk_none:
            /* This is an error case; presumably diagnosed in the front end. */
            break;
          case templk_function:
          case templk_member_function:
            remap_ptr(eptr->prototype_instantiation.routine, a_routine_ptr,
                      iek_routine);
            break;
          case templk_class:
          case templk_member_class:
          case templk_member_enum:
            remap_ptr(eptr->prototype_instantiation.type, a_type_ptr,
                      iek_type);
            break;
          case templk_static_data_member:
          case templk_variable:
            remap_ptr(eptr->prototype_instantiation.variable, a_variable_ptr,
                      iek_variable);
            break;
          case templk_template_template_param:
            /* No active variant field. */
            break;
          case templk_concept:
            walk_ptr(eptr->prototype_instantiation.constraint,
                     an_expr_node_ptr, iek_expr_node);
            break;
          default:
            unexpected_condition_str(
                               "walk_entry_and_subtree: bad template kind");
            break;
        }  /* switch */
        remap_ptr(eptr->canonical_template, a_template_ptr, iek_template);
        remap_ptr(eptr->definition_template, a_template_ptr, iek_template);
        remap_ptr(eptr->prototype_template, a_template_ptr, iek_template);
        /* The template_info pointer should be NULL for any entry actually
           written and read. */
        conditionally_clear_fe_pointer(eptr->template_info);
#undef eptr
      }
      break;
#if RECORD_MACROS_IN_IL
    case iek_macro:
      {
#define eptr ((a_macro_ptr)entry_ptr)
        walk_source_corresp(eptr->source_corresp);
        remap_next_ptr(eptr->next, a_macro_ptr, iek_macro);
        walk_string_ptr(eptr->text, iek_other_text, 0);
#undef eptr
      }
      break;
#endif /* RECORD_MACROS_IN_IL */
#if MACRO_INVOCATION_TREE_IN_IL
    case iek_macro_invocation_record_block:
      {
#define eptr ((a_macro_invocation_record_block_ptr)entry_ptr)
        walk_ptr(eptr->left_subtree, a_macro_invocation_record_block_ptr,
                 iek_macro_invocation_record_block);
        [] (a_macro_invocation_record_block_ptr mirbp) {
          for (int i = 0; i < MACRO_INVOCATION_RECORDS_PER_BLOCK; ++i) {
            remap_ptr(mirbp->records[i].assoc_macro, a_macro_ptr, iek_macro);
#if RECORD_MACRO_ARGS
            walk_string_ptr(mirbp->records[i].arguments, iek_other_text, 0);
#endif /* RECORD_MACRO_ARGS*/
          }  /* for */
        } (eptr);
        walk_ptr(eptr->right_subtree, a_macro_invocation_record_block_ptr,
                 iek_macro_invocation_record_block);
#if !NEEDED_FLAG_WALK && !KEEP_IN_IL_WALK
        /* All the macro invocation record blocks are visited by the
           tree traversal, so just map the pointers for the doubly-linked
           list. */
        remap_ptr(eptr->next, a_macro_invocation_record_block_ptr,
                  iek_macro_invocation_record_block);
        remap_ptr(eptr->prev, a_macro_invocation_record_block_ptr,
                  iek_macro_invocation_record_block);
#endif /* !NEEDED_FLAG_WALK && !KEEP_IN_IL_WALK */
#undef eptr
      }
      break;
#endif /* MACRO_INVOCATION_TREE_IN_IL */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    case iek_element_position:
      {
#if !DO_SUBTREE_WALK
#define eptr ((an_element_position_ptr)entry_ptr)
        remap_next_ptr(eptr->next, an_element_position_ptr,
                       iek_element_position);
#undef eptr
#endif /* !DO_SUBTREE_WALK */
      }
      break;
    case iek_decl_position_supplement:
      {
#define eptr ((a_decl_position_supplement_ptr)entry_ptr)
        walk_list(eptr->extra_positions, an_element_position_ptr,
                  iek_element_position);
#undef eptr
      }
      break;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    case iek_name_qualifier:
      {
#define eptr ((a_name_qualifier_ptr)entry_ptr)
        /* The "next" pointer is for front end use only. */
        conditionally_clear_fe_pointer(eptr->next);
        if (eptr->is_class) {
          walk_ptr(eptr->qualifier.class_type, a_type_ptr, iek_type);
          /* "if_class" test is needed because the qualifier can be an
             enum in Microsoft mode. */
          definition_needed_if_class(eptr->qualifier.class_type);
        } else {
          walk_ptr(eptr->qualifier.namespace_ptr, a_namespace_ptr,
                   iek_namespace);
        }  /* if */
        if (eptr->name != NULL) {
          walk_string_ptr(eptr->name, iek_other_text, 0);
        }  /* if */
        walk_ptr(eptr->previous_qualifier, a_name_qualifier_ptr,
                 iek_name_qualifier);
#undef eptr
      }
      break;
    case iek_name_reference:
      {
#define eptr ((a_name_reference_ptr)entry_ptr)
        /* Note intentional use of walk_next_ptr instead of remap_next_ptr,
           because of issues with export creating lists that run between
           translation units. */
        walk_next_ptr(eptr->next, a_name_reference_ptr, iek_name_reference);
        walk_list(eptr->orig_template_arg_list, a_template_arg_ptr,
                  iek_template_arg);
        if (eptr->qualifier != NULL) {
          clear_or_walk_name_reference_field(eptr, eptr->qualifier,
                                             a_name_qualifier_ptr,
                                             iek_name_qualifier);
        }  /* if */
        if (eptr->special_kind == (a_special_function_kind)sfk_none) {
          if (eptr->variant.destructor_type != NULL) {
            clear_or_walk_name_reference_field(eptr,
                                               eptr->variant.destructor_type,
                                               a_type_ptr, iek_type);
          }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED && !DO_IL_LOWERING
        } else {
          clear_or_walk_name_reference_field(
                    eptr, eptr->variant.property_or_event_descr,
                    a_property_or_event_descr_ptr,
                    iek_property_or_event_descr);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED && !DO_IL_LOWERING */
        }  /* if */
#undef eptr
      }
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case iek_ms_attribute:
      {
#define eptr ((an_ms_attribute_ptr)entry_ptr)
        remap_next_ptr(eptr->next, an_ms_attribute_ptr, iek_ms_attribute);
        remap_ptr(eptr->next_in_block, an_ms_attribute_ptr, iek_ms_attribute);
#if NEEDED_FLAG_WALK
        /* a_param_type entries and a_base_class entries have no needed flags
           but point back to their associated Microsoft attributes.  Don't
           process those entries since it would cause a recursive loop. */
        if (eptr->entity.kind != iek_param_type &&
            eptr->entity.kind != iek_base_class)
#endif /* NEEDED_FLAG_WALK */
        /* Do not insert code here. */
        {
          remap_ptr(eptr->entity.ptr, a_char_ptr,
                    (an_il_entry_kind)eptr->entity.kind);
        }
        if (eptr->kind == (an_ms_attribute_kind)msak_custom) {
          remap_ptr(eptr->variant.custom_info.type, a_type_ptr, iek_type);
          remap_ptr(eptr->variant.custom_info.constructor, a_routine_ptr,
                    iek_routine);
          walk_list(eptr->variant.custom_info.args, an_expr_node_ptr,
                    iek_expr_node);
          walk_list(eptr->variant.custom_info.named_args,
                    a_custom_ms_attribute_arg_ptr,
                    iek_custom_ms_attribute_arg);
        } else {
          conditionally_clear_fe_pointer(eptr->variant.info.kind_descr);
          walk_string_ptr(eptr->variant.info.name, iek_other_text, 0);
          walk_string_ptr(eptr->variant.info.string, iek_other_text, 0);
          walk_list(eptr->variant.info.arg_list, an_ms_attribute_arg_ptr,
                    iek_ms_attribute_arg);
        }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS && !NEEDED_FLAG_WALK && !KEEP_IN_IL_WALK
        remap_ptr(eptr->source_sequence_entry, a_source_sequence_entry_ptr,
                  iek_source_sequence_entry);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS && ... */
#undef eptr
      }
      break;
    case  iek_ms_attribute_arg:
      {
#define eptr ((an_ms_attribute_arg_ptr)entry_ptr)
        remap_next_ptr(eptr->next, an_ms_attribute_arg_ptr,
                      iek_ms_attribute_arg);
        walk_string_ptr(eptr->param_name, iek_other_text, 0);
        switch (eptr->kind) {
          case msaak_string:
            walk_ptr(eptr->variant.string_constant, a_constant_ptr,
                     iek_constant);
            break;
          case msaak_other:
            walk_string_ptr(eptr->variant.other_string, iek_other_text, 0);
            break;
          case msaak_uuid:
            walk_string_ptr(eptr->variant.uuid_string, iek_other_text, 0);
            break;
          default:
            break;
        }  /* switch */
#undef eptr
      }
      break;
    case  iek_custom_ms_attribute_arg:
#define eptr ((a_custom_ms_attribute_arg_ptr)entry_ptr)
      {
        remap_next_ptr(eptr->next, a_custom_ms_attribute_arg_ptr,
                      iek_custom_ms_attribute_arg);
        remap_ptr(eptr->field, a_field_ptr, iek_field);
        walk_ptr(eptr->expression, an_expr_node_ptr, iek_expr_node);
#undef eptr
      }
      break;
    case iek_property_index_type:
      {
#define eptr ((a_property_index_type_ptr)entry_ptr)
        remap_next_ptr(eptr->next, a_property_index_type_ptr,
                       iek_property_index_type);
        walk_ptr(eptr->type, a_type_ptr, iek_type);
#undef eptr
      }
      break;
    case iek_property_or_event_descr:
      {
#define eptr ((a_property_or_event_descr_ptr)entry_ptr)
        if (eptr->is_static) {
          remap_ptr(eptr->variant.variable, a_variable_ptr, iek_variable);
        } else {
          remap_ptr(eptr->variant.field, a_field_ptr, iek_field);
        }  /* if */
        switch (eptr->kind) {
          case pek_declspec_property:
            walk_string_ptr(eptr->get_routine.name, iek_other_text, 0);
            walk_string_ptr(eptr->set_routine.name, iek_other_text, 0);
            break;
          case pek_cli_property:
            walk_list(eptr->indices, a_property_index_type_ptr,
                      iek_property_index_type);
            remap_ptr(eptr->get_routine.ptr, a_routine_ptr, iek_routine);
            remap_ptr(eptr->set_routine.ptr, a_routine_ptr, iek_routine);
            break;
          case pek_cli_event:
            remap_ptr(eptr->add_routine, a_routine_ptr, iek_routine);
            remap_ptr(eptr->remove_routine, a_routine_ptr, iek_routine);
            remap_ptr(eptr->raise_routine, a_routine_ptr, iek_routine);
#if KEEP_IN_IL_WALK
            keep_event_delegate_definition_in_il(eptr);
#endif /* KEEP_IN_IL_WALK */
            break;
          default:
            unexpected_condition();
        }  /* switch */
#undef eptr
      }
      break;
      case iek_generic_constraint:
        {
#define eptr ((a_generic_constraint_ptr)entry_ptr)
          /* Walk the next pointer because the constraint clause is not in
             the IL if all_template_info_in_il is not set. */
          walk_next_ptr(eptr->next, a_generic_constraint_ptr,
                        iek_generic_constraint);
          remap_ptr(eptr->type, a_type_ptr, iek_type);
#undef eptr
        }
        break;
      case iek_generic_constraint_clause:
        {
#define eptr ((a_generic_constraint_clause_ptr)entry_ptr)
          /* The next pointer is handled by the walk_list for the
             a_template_decl entry. */
          remap_ptr(eptr->type, a_type_ptr, iek_type);
          walk_ptr(eptr->constraints, a_generic_constraint_ptr,
                   iek_generic_constraint);
#undef eptr
        }
        break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
    case iek_ms_if_exists:
      {
#define eptr ((an_ms_if_exists_ptr)entry_ptr)
        remap_next_ptr(eptr->next, an_ms_if_exists_ptr, iek_ms_if_exists);
        walk_ptr(eptr->entity.ptr, a_char_ptr,
                 (an_il_entry_kind)eptr->entity.kind);
        walk_ptr(eptr->name_reference, a_name_reference_ptr,
                 iek_name_reference);
#undef eptr
      }
      break;
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
    case iek_object_lifetime:
      {
#define eptr ((an_object_lifetime_ptr)entry_ptr)
        remap_ptr_not_needed(eptr->entity.ptr, a_char_ptr,
                             (an_il_entry_kind)eptr->entity.kind);
        /* The destructors list is linked on the field
           "next_in_destruction_list" because the usual "next" is used for
           a different list. */
        walk_list_on_link_field(eptr->destructions, a_dynamic_init_ptr,
                                iek_dynamic_init, next_in_destruction_list);
#if !NEEDED_FLAG_WALK && !KEEP_IN_IL_WALK
        /* Don't walk the parent pointer for keep_in_il processing because
           that can cause visits to siblings that aren't going to stay in the
           tree. */
        remap_ptr(eptr->parent_lifetime, an_object_lifetime_ptr,
                  iek_object_lifetime);
#endif /* !NEEDED_FLAG_WALK && !KEEP_IN_IL_WALK */
        remap_ptr_not_needed(eptr->parent_destruction_sublist,
                             a_dynamic_init_ptr, iek_dynamic_init);
        walk_list(eptr->child_lifetime, an_object_lifetime_ptr,
                  iek_object_lifetime);
        remap_next_ptr(eptr->next, an_object_lifetime_ptr,
                       iek_object_lifetime);
#undef eptr
      }
      break;
    case iek_scope:
      {
#define eptr ((a_scope_ptr)entry_ptr)
        a_scope_kind kind = eptr->kind;
#if !NEEDED_FLAG_WALK && !KEEP_IN_IL_WALK
        remap_next_ptr(eptr->next, a_scope_ptr, iek_scope);
        remap_ptr(eptr->prev, a_scope_ptr, iek_scope);
        remap_ptr(eptr->parent, a_scope_ptr, iek_scope);
#else /* !(!NEEDED_FLAG_WALK && !KEEP_IN_IL_WALK) */
        if (!scope_is(eptr, sck_function)) {
          /* For needed and keep-in-il walks, don't walk the previous and
             next pointers of function scopes. */
          remap_next_ptr(eptr->next, a_scope_ptr, iek_scope);
          remap_ptr(eptr->prev, a_scope_ptr, iek_scope);
        }  /* if */
#endif /* !NEEDED_FLAG_WALK && !KEEP_IN_IL_WALK */
        switch (kind) {
          case sck_file:
          case sck_template_declaration:
            /* No pointers */
            break;
          case sck_block:
            /* Call remap_ptr on the handler entry since it is also on a list
               pointed to from the try-block statement. */
            remap_ptr_not_needed(eptr->variant.assoc_handler, a_handler_ptr,
                                 iek_handler);
            /* Also see assoc_block below. */
            break;
          case sck_func_prototype:
            /* Walk the associated function type.  In C++, prototype scopes
               exist mainly to carry hidden name lists and can otherwise
               leave that type unprocessed.  In C, the associated type may
               be a declared-type copy that is not on the types list and is
               not the type of a routine. */
            walk_ptr(eptr->variant.assoc_type, a_type_ptr, iek_type);
            break;
          case sck_class_struct_union:
          case sck_enum:
            remap_ptr_not_needed(eptr->variant.assoc_type, a_type_ptr,
                                 iek_type);
            break;
          case sck_condition:
            remap_ptr_not_needed(eptr->variant.assoc_statement,
                                 a_statement_ptr, iek_statement);
            break;
          case sck_namespace:
            remap_ptr_not_needed(eptr->variant.assoc_namespace,
                                 a_namespace_ptr, iek_namespace);
            break;
          case sck_function:
            /* "ptr", which points to the routine associated with this scope,
               is done after the declarations. */
            walk_list(eptr->variant.routine.parameters, a_variable_ptr,
                      iek_variable);
            walk_list(eptr->variant.routine.constructor_inits,
                      a_constructor_init_ptr, iek_constructor_init);
            walk_ptr(eptr->variant.routine.lifetime_of_local_static_vars,
                     an_object_lifetime_ptr, iek_object_lifetime);
            walk_ptr(eptr->variant.routine.this_param_variable, a_variable_ptr,
                     iek_variable);
            remap_ptr_not_needed(eptr->variant.routine.return_value_variable,
                                 a_variable_ptr, iek_variable);
            break;
          case sck_template_instantiation:
            /* Front end only. */
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
            break;
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
          default:
            unexpected_condition_str("walk_entry_and_subtree: bad scope kind");
        }  /* switch */
        /* "assoc_block" is done after the declarations. */
        /* The lifetime pointer needs to be walked and not remapped in
           the file scope and function scopes. */
        walk_ptr(eptr->lifetime, an_object_lifetime_ptr, iek_object_lifetime);
        walk_list(eptr->constants, a_constant_ptr, iek_constant);
#if DO_SUBTREE_WALK
#if NEEDED_FLAG_WALK
        /* Do not walk the types and variables lists to set the "needed"
           flag; a variable or type is not needed simply because it's
           declared. */
        /* On the "needed" flag walk for a class, mark all the virtual
           functions as needed.  Note that if IL lowering is done, there
           will be no functions attached to the class anymore. */
        if (kind == (a_scope_kind)sck_class_struct_union) {
          [] (a_routine_ptr rout) {
            for (; rout != NULL; rout = rout->next) {
              if (rout->is_virtual) {
                walk_ptr(rout, a_routine_ptr, iek_routine);
              }  /* if */
            }  /* for */
          } (eptr->routines);
        }  /* if */
#else /* !NEEDED_FLAG_WALK */
#if KEEP_IN_IL_WALK
        if (kind == (a_scope_kind)sck_function ||
            kind == (a_scope_kind)sck_block ||
            (kind == (a_scope_kind)sck_class_struct_union &&
             eptr->variant.assoc_type->source_corresp.is_local_to_function)) {
          /* For lists within a function, mark everything to be kept, because
             we don't remove individual entities within function bodies. */
          walk_list(eptr->types, a_type_ptr, iek_type);
          walk_list(eptr->variables, a_variable_ptr, iek_variable);
          walk_list(eptr->routines, a_routine_ptr, iek_routine);
        } else {
          /* For lists not within a function, mark only the needed entities
             to be kept. */
          walk_needed_on_list(eptr->types, a_type_ptr, iek_type, kind);
          walk_needed_on_list(eptr->variables, a_variable_ptr, iek_variable,
                              kind);
          walk_needed_on_list(eptr->routines, a_routine_ptr, iek_routine,
                              kind);
        }  /* if */
#else /* !KEEP_IN_IL_WALK */
        /* Not needed flag walk or keep_in_il walk. */
        if (eptr->scope_orphaned_list_header_generated) {
          /* The local types and static variables at function scope or
             block scope within a function are in the file scope memory region.
             They will be processed during the file scope memory region
             walk because a_scope_orphaned_list_header entry for these lists
             would have been created. */
          remap_list_ptr(eptr->types, a_type_ptr, iek_type);
          remap_list_ptr(eptr->variables, a_variable_ptr, iek_variable);
        } else {
          /* Not a function or block scope, or one for which the orphan
             lists have not been generated yet. */
          walk_list(eptr->types, a_type_ptr, iek_type);
          walk_list(eptr->variables, a_variable_ptr, iek_variable);
        }  /* if */
        walk_list(eptr->routines, a_routine_ptr, iek_routine);
#endif /* KEEP_IN_IL_WALK */
#endif /* NEEDED_FLAG_WALK */
#else /* !DO_SUBTREE_WALK */
        /* Not walking subtrees.  Just remap the pointers. */
        remap_list_ptr(eptr->types, a_type_ptr, iek_type);
        remap_list_ptr(eptr->variables, a_variable_ptr, iek_variable);
        remap_list_ptr(eptr->routines, a_routine_ptr, iek_routine);
#endif /* DO_SUBTREE_WALK */
#if NEEDED_FLAG_WALK && defined(nonstatic_variable_always_needed)
        if (kind == (a_scope_kind)sck_function ||
            kind == (a_scope_kind)sck_block) { 
          /* Some variables may be needed because of attributes or the
             like.  The nonstatic_variable_always_needed macro provides the
             logic to determine if a given variable satisfies the relevant
             criteria. */
          [] (a_variable_ptr var) {
            for (; var != NULL; var = var->next) {
              if (nonstatic_variable_always_needed(var)) {
                walk_ptr(var, a_variable_ptr, iek_variable);
              }  /* if */
            }  /* for */
          } (eptr->nonstatic_variables);
        }  /* if */
#else /* !(NEEDED_FLAG_WALK && defined(nonstatic_variable_always_needed)) */
        walk_list_not_needed(eptr->nonstatic_variables, a_variable_ptr,
                             iek_variable);
#endif /* NEEDED_FLAG_WALK && defined(nonstatic_variable_always_needed) */
        walk_list_not_needed(eptr->labels, a_label_ptr, iek_label);
        walk_list(eptr->scopes, a_scope_ptr, iek_scope);
        walk_list_with_keep_in_il_reset(eptr->namespaces, a_namespace_ptr,
                                        iek_namespace);
        walk_list_not_needed(eptr->using_declarations, a_using_decl_ptr,
                             iek_using_decl);
        walk_list_not_needed(eptr->using_directives, a_using_decl_ptr,
                             iek_using_decl);
        walk_list(eptr->asm_entries, an_asm_entry_ptr, iek_asm_entry);
        walk_list(eptr->dynamic_inits, a_dynamic_init_ptr, iek_dynamic_init);
        walk_list(eptr->local_static_variable_inits,
                  a_local_static_variable_init_ptr,
                  iek_local_static_variable_init);
        walk_list(eptr->vla_dimensions, a_vla_dimension_ptr,
                  iek_vla_dimension);
#if !NEEDED_FLAG_WALK
        walk_list(eptr->expr_node_refs, a_local_expr_node_ref_ptr,
                  iek_local_expr_node_ref);
#endif /* NEEDED_FLAG_WALK */
        walk_list(eptr->scope_refs, a_local_scope_ref_ptr,
                  iek_local_scope_ref);
        walk_list(eptr->pragmas, a_pragma_ptr, iek_pragma);
        walk_list(eptr->templates, a_template_ptr, iek_template);
#if MICROSOFT_EXTENSIONS_ALLOWED
        walk_list(eptr->ms_attributes, an_ms_attribute_ptr, iek_ms_attribute);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
        walk_list(eptr->ms_if_exists, an_ms_if_exists_ptr, iek_ms_if_exists);
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
        if (kind == sck_function || eptr->is_stmt_expr_block) {
          if (kind == sck_function) {
            remap_ptr(eptr->variant.routine.ptr, a_routine_ptr, iek_routine);
          }  /* if */
          walk_ptr(eptr->assoc_block, a_statement_ptr, iek_statement);
        } else {
          remap_ptr(eptr->assoc_block, a_statement_ptr, iek_statement);
        }  /* if */
#if RECORD_HIDDEN_NAMES_IN_IL
        walk_list_not_needed(eptr->hidden_names, a_hidden_name_ptr,
                             iek_hidden_name);
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if KEEP_IN_IL_WALK
        /* When setting the keep_in_il flag, source sequence entries are
           kept if and only if the associated IL entry is kept.  (In
           function scopes, all entries are kept.)  Note that this is
           done last so all the keep_in_il flags are set already.
           The file scope is not processed here, but rather in
           mark_to_keep_in_il, because it must be done after orphan
           processing. */
        if (kind == (a_scope_kind)sck_function) {
          set_keep_in_il_on_source_sequence_entries(eptr);
        }  /* if */
#else /* !KEEP_IN_IL_WALK */
        walk_list_not_needed(eptr->source_sequence_list,
                             a_source_sequence_entry_ptr,
                             iek_source_sequence_entry);
        /* The src_seq_sublist_list, which appears only on function scopes,
           is not walked at this time: it is handled during orphan list
           processing. */
#if !NEEDED_FLAG_WALK
        remap_list_ptr(eptr->src_seq_sublist_list, a_src_seq_sublist_ptr,
                       iek_src_seq_sublist);
#endif /* !NEEDED_FLAG_WALK */
#endif /* KEEP_IN_IL_WALK */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#undef eptr
      }
      break;
#if C99_IL_EXTENSIONS_SUPPORTED
    case iek_internal_complex_value:
      /* No pointers. */
      break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case iek_namespace:
      {
#define eptr ((a_namespace_ptr)entry_ptr)
        walk_source_corresp(eptr->source_corresp);
        remap_next_ptr(eptr->next, a_namespace_ptr, iek_namespace);
#if MICROSOFT_EXTENSIONS_ALLOWED
        walk_ptr(eptr->proxy_class, a_type_ptr, iek_type);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        if (eptr->is_namespace_alias) {
          remap_ptr(eptr->variant.assoc_namespace, a_namespace_ptr,
                    iek_namespace);
        } else {
          /* When doing the "needed" flag walk, the members of a namespace are
             not considered needed merely because the namespace itself is
             needed. */
          walk_ptr_not_needed(eptr->variant.assoc_scope, a_scope_ptr,
                              iek_scope);
        }  /* if */
#undef eptr
      }
      break;
#if !NEEDED_FLAG_WALK
    case iek_using_decl:
      {
#define eptr ((a_using_decl_ptr)entry_ptr)
        remap_next_ptr(eptr->next, a_using_decl_ptr, iek_using_decl);
        /* walk_ptr used instead of remap_ptr because the entity referenced
           can be a constant, e.g., in a prototype instantiation. */
        walk_ptr(eptr->entity.ptr, a_char_ptr,
                 (an_il_entry_kind)eptr->entity.kind);
        walk_list(eptr->attributes, an_attribute_ptr, iek_attribute);
        if (eptr->is_class_member) {
          walk_ptr(eptr->qualifier.class_type, a_type_ptr, iek_type);
        } else {
          remap_ptr(eptr->qualifier.namespace_ptr, a_namespace_ptr,
                    iek_namespace);
        }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS && !KEEP_IN_IL_WALK
        remap_ptr(eptr->source_sequence_entry, a_source_sequence_entry_ptr,
                  iek_source_sequence_entry);
        remap_ptr(eptr->next_in_set, a_using_decl_ptr,
                  iek_using_decl);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS && ... */
#undef eptr
      }
      break;
#endif /* !NEEDED_FLAG_WALK */
    case iek_dynamic_init:
      {
#define eptr ((a_dynamic_init_ptr)entry_ptr)
        remap_next_ptr(eptr->next, a_dynamic_init_ptr, iek_dynamic_init);
        remap_ptr(eptr->variable, a_variable_ptr, iek_variable);
        remap_ptr(eptr->destructor, a_routine_ptr, iek_routine);
        if (eptr->destructor != NULL) {
          set_proper_routine_definition_needed_flag(eptr->destructor);
        }  /* if */
        remap_ptr_not_needed(eptr->lifetime, an_object_lifetime_ptr,
                             iek_object_lifetime);
#if !NEEDED_FLAG_WALK
        remap_next_ptr(eptr->next_in_destruction_list, a_dynamic_init_ptr,
                       iek_dynamic_init);
#endif /* !NEEDED_FLAG_WALK */
        remap_ptr_not_needed(eptr->init_expr_lifetime, an_object_lifetime_ptr,
                             iek_object_lifetime);
        switch (eptr->kind) {
          case dik_none:
          case dik_zero:
            /* No pointers. */
            break;
          case dik_constant:
          case dik_nonconstant_aggregate:
            walk_ptr(eptr->variant.constant.ptr, a_constant_ptr, iek_constant);
            break;
          case dik_lambda:
            walk_ptr(eptr->variant.constant.ptr, a_constant_ptr, iek_constant);
            walk_ptr(eptr->variant.constant.lambda, a_lambda_ptr, iek_lambda);
            break;
          case dik_expression:
          case dik_class_result_via_ctor:
            walk_ptr(eptr->variant.expression, an_expr_node_ptr,
                     iek_expr_node);
            break;
          case dik_constructor:
            if (eptr->variant.constructor.ptr != NULL) {
              remap_ptr(eptr->variant.constructor.ptr, a_routine_ptr,
                        iek_routine);
              set_proper_routine_definition_needed_flag(
                                                eptr->variant.constructor.ptr);
            }  /* if */
            walk_list(eptr->variant.constructor.args, an_expr_node_ptr,
                      iek_expr_node);
            break;
          case dik_bitwise_copy:
            walk_ptr(eptr->variant.bitwise_copy.source, an_expr_node_ptr,
                     iek_expr_node);
            break;
          default:
            unexpected_condition_str(
                              "walk_entry_and_subtree: bad dynamic init kind");
        }  /* switch */
#if DO_IL_LOWERING
        conditionally_clear_fe_pointer(eptr->destructible_entity_descr);
        conditionally_clear_fe_pointer(eptr->init_destination);
        conditionally_clear_fe_pointer(eptr->assoc_new);
#endif /* DO_IL_LOWERING */
        remap_ptr(eptr->lifetime_of_overlapping_temps, an_object_lifetime_ptr,
                  iek_object_lifetime);
        /* "_not_needed" here to avoid loops in walk. */
        remap_ptr_not_needed(eptr->master_entry, a_dynamic_init_ptr,
                             iek_dynamic_init);
        conditionally_clear_fe_pointer(eptr->rescan_info);
#undef eptr
      }
      break;
    case iek_local_static_variable_init:
      {
#define eptr ((a_local_static_variable_init_ptr)entry_ptr)
        remap_next_ptr(eptr->next, a_local_static_variable_init_ptr,
                       iek_local_static_variable_init);
        remap_ptr_not_needed(eptr->variable, a_variable_ptr, iek_variable);
        walk_initializer(eptr->init_kind, eptr->initializer,
                         /*is_member_constant=*/FALSE);
        remap_ptr_not_needed(eptr->lifetime, an_object_lifetime_ptr,
                             iek_object_lifetime);
#undef eptr
      }
      break;
    case iek_vla_dimension:
      {
#define eptr ((a_vla_dimension_ptr)entry_ptr)
        remap_next_ptr(eptr->next, a_vla_dimension_ptr, iek_vla_dimension);
        walk_ptr(eptr->type, a_type_ptr, iek_type);
        if (eptr->dimension_expr != NULL) {
          walk_ptr(eptr->dimension_expr, an_expr_node_ptr, iek_expr_node);
        } else {
          remap_ptr(eptr->original_dimension, a_vla_dimension_ptr,
                    iek_vla_dimension);
        }  /* if */
#if DO_IL_LOWERING
#if LOWER_VARIABLE_LENGTH_ARRAYS
        remap_ptr(eptr->total_number_of_elements, a_variable_ptr,
                  iek_variable);
#else /* !LOWER_VARIABLE_LENGTH_ARRAYS */
        remap_ptr(eptr->dimension_variable, a_variable_ptr, iek_variable);
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
#endif /* DO_IL_LOWERING */
#undef eptr
      }
      break;
#if !NEEDED_FLAG_WALK
#if DO_IL_LOWERING && IA64_ABI
    case iek_vcall_offset_entry:
      {
#define eptr ((a_vcall_offset_entry_ptr)entry_ptr)
        remap_next_ptr(eptr->next, a_vcall_offset_entry_ptr, 
                       iek_vcall_offset_entry);
        remap_ptr(eptr->routine, a_routine_ptr, iek_routine);
        remap_ptr(eptr->base_class, a_base_class_ptr, iek_base_class);
#undef eptr
      }
      break;
#endif /* DO_IL_LOWERING && IA64_ABI */
    case iek_overriding_virtual_function:
      {
#define eptr ((an_overriding_virtual_function_ptr)entry_ptr)
        remap_next_ptr(eptr->next, an_overriding_virtual_function_ptr,
                       iek_overriding_virtual_function);
        remap_ptr(eptr->overriding_function, a_routine_ptr, iek_routine);
        remap_ptr(eptr->primary_function, a_routine_ptr, iek_routine);
        remap_ptr(eptr->base_class, a_base_class_ptr, iek_base_class);
        remap_ptr(eptr->return_adjustment_base_class, a_base_class_ptr,
                  iek_base_class);
#undef eptr
      }
      break;
    case iek_derivation_step:
      {
#define eptr ((a_derivation_step_ptr)entry_ptr)
        /* Walk the prev pointer.  This ensures (together with not walking the
           whole list in iek_base_class_derivation) that the traversal stops
           once we reach an entry on a shared path that has already been
           walked. */
        walk_next_ptr(eptr->prev, a_derivation_step_ptr,
                      iek_derivation_step);
        remap_ptr_not_needed(eptr->next, a_derivation_step_ptr,
                             iek_derivation_step);
        remap_ptr(eptr->base_class, a_base_class_ptr, iek_base_class);
#undef eptr
      }
      break;
    case iek_base_class_derivation:
      {
#define eptr ((a_base_class_derivation_ptr)entry_ptr)
        remap_next_ptr(eptr->next, a_base_class_derivation_ptr,
                       iek_base_class_derivation);
        /* Walk the path, starting from path_tail, to ensure we only walk the
           list between path and path_tail.  We intentionally don't use
           walk_list_on_link_field as we only want to walk the part of the path
           that hasn't been walked before. */
        walk_ptr(eptr->path_tail, a_derivation_step_ptr,
                 iek_derivation_step);
        remap_ptr_not_needed(eptr->path, a_derivation_step_ptr,
                             iek_derivation_step);
#undef eptr
      }
      break;
#endif /* !NEEDED_FLAG_WALK */
    case iek_base_class:
      {
#define eptr ((a_base_class_ptr)entry_ptr)
        remap_next_ptr(eptr->next, a_base_class_ptr, iek_base_class);
        remap_ptr_not_needed(eptr->next_direct, a_base_class_ptr,
                             iek_base_class);
#if IA64_ABI
        remap_ptr_not_needed(eptr->next_preorder, a_base_class_ptr, 
                             iek_base_class);
        remap_ptr_not_needed(eptr->primary_base_class, a_base_class_ptr, 
                             iek_base_class);
#endif /* IA64_ABI */
        walk_list(eptr->attributes, an_attribute_ptr, iek_attribute);
#if MICROSOFT_EXTENSIONS_ALLOWED
        walk_list(eptr->ms_attributes, an_ms_attribute_ptr, iek_ms_attribute);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        remap_ptr(eptr->type, a_type_ptr, iek_type);
        /* Use an unconditional walk for the orig_type since it can be
           a decltype which won't appear on the types list. */
        walk_ptr(eptr->orig_type, a_type_ptr, iek_type);
        set_proper_definition_needed_flag(eptr->type);
        remap_ptr(eptr->derived_class, a_type_ptr, iek_type);
        if (eptr->derived_class != NULL) {
          set_proper_definition_needed_flag(eptr->derived_class);
        }  /* if */
        conditionally_clear_fe_pointer(eptr->trans_unit_corresp);
#if CFRONT_OBJECT_CODE_COMPATIBILITY
        remap_ptr_not_needed(eptr->data_section_base_class, a_base_class_ptr,
                             iek_base_class);
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
#if !IA64_ABI
        remap_ptr_not_needed(eptr->pointer_base_class, a_base_class_ptr,
                             iek_base_class);
#endif /* !IA64_ABI */
        walk_list_not_needed(eptr->derivation, a_base_class_derivation_ptr,
                             iek_base_class_derivation);
        if (eptr->is_pack_expansion) {
          conditionally_clear_fe_pointer(eptr->variant.pack_expansion_descr);
        } else {
          walk_list_not_needed(eptr->variant.overriding_virtual_functions,
                               an_overriding_virtual_function_ptr,
                               iek_overriding_virtual_function);
        }  /* if */
#if DO_IL_LOWERING
#if !IA64_ABI
        conditionally_clear_fe_pointer(eptr->virtual_function_table_var);
#endif /* !IA64_ABI */
#endif /* DO_IL_LOWERING */
#undef eptr
      }
      break;
    case iek_class_list_entry:
      {
#define eptr ((a_class_list_entry_ptr)entry_ptr)
        remap_next_ptr(eptr->next, a_class_list_entry_ptr,
                       iek_class_list_entry);
        remap_ptr(eptr->class_type, a_type_ptr, iek_type);
#undef eptr
      }
      break;
    case iek_routine_list_entry:
      {
#define eptr ((a_routine_list_entry_ptr)entry_ptr)
        remap_next_ptr(eptr->next, a_routine_list_entry_ptr,
                       iek_routine_list_entry);
        remap_ptr(eptr->routine, a_routine_ptr, iek_routine);
#undef eptr
      }
      break;
    case iek_class_type_supplement:
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
handle_class_type_supplement_for_class:
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
      [] (a_class_type_supplement_ptr ctsp
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
          , a_type_ptr type
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
      ) {
        /* Fields to be processed even if the definition of the class is
           not to be processed: */
        /* Nonreal classes based on template template parameters can have
           an associated template that is in a template declaration scope
           and therefore doesn't show up elsewhere on the walk.  Thus
           the walk_ptr. */
        walk_ptr(ctsp->assoc_template, a_template_ptr, iek_template);
        walk_list(ctsp->template_arg_list, a_template_arg_ptr,
                  iek_template_arg);
        walk_list(ctsp->partial_spec_template_arg_list, a_template_arg_ptr,
                  iek_template_arg);
#if MICROSOFT_EXTENSIONS_ALLOWED
        walk_string_ptr(ctsp->uuid_string, iek_other_text, 0);
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
        /* Recall that type points to a class pointer in this case. */
        if (is_cli_array_type(type) &&
            ctsp->template_arg_list != NULL &&
            is_type_templ_arg(ctsp->template_arg_list) &&
            ctsp->template_arg_list->variant.type != NULL &&
            is_handle_type(ctsp->template_arg_list->variant.type)) {
          a_type_ptr underlying_type = f_skip_typerefs(
                       type_pointed_to(ctsp->template_arg_list->variant.type));
          if (is_immediate_class_type(underlying_type)) {
          /* Keep the underlying class type of a handle of a C++/CLI array
             type in the IL. */
            set_proper_definition_needed_flag(underlying_type);
          }  /* if */
        }  /* if */
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if !NEEDED_FLAG_WALK
#if KEEP_IN_IL_WALK
        /* Visit befriending classes for the "keep_in_il" sweep. */
        set_keep_in_il_on_befriending_classes(ctsp->befriending_classes);
#else /* !KEEP_IN_IL_WALK */
        /* All cases except NEEDED_FLAG_WALK and KEEP_IN_IL_WALK. */
        walk_list(ctsp->befriending_classes, a_class_list_entry_ptr,
                  iek_class_list_entry);
#endif /* KEEP_IN_IL_WALK */
#endif /* !NEEDED_FLAG_WALK */
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
        /* During these walks, visit the definition only if necessary. */
        if (
#if NEEDED_FLAG_WALK
            class_definition_needed_flag_is_set(type)
#else /* !NEEDED_FLAG_WALK (i.e., KEEP_IN_IL_WALK) */
            type->variant.class_struct_union.keep_definition_in_il
#endif /* NEEDED_FLAG_WALK */
                                                                        )
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
        /* Do not insert code here. */
        {
          /* Fields to be processed only if the definition of the class
             is to be processed: */
          walk_list(ctsp->base_classes, a_base_class_ptr, iek_base_class);
          remap_ptr_not_needed(ctsp->direct_base_classes, a_base_class_ptr,
                               iek_base_class);
#if IA64_ABI
          remap_ptr_not_needed(ctsp->preorder_base_classes, a_base_class_ptr,
                               iek_base_class);
          remap_ptr_not_needed(ctsp->primary_base_class, a_base_class_ptr, 
                               iek_base_class);
#endif /* IA64_ABI */
          remap_ptr(ctsp->anonymous_union_field, a_field_ptr, iek_field);
          walk_ptr(ctsp->assoc_scope, a_scope_ptr, iek_scope);
          remap_ptr_not_needed(ctsp->virtual_function_info_base_class,
                               a_base_class_ptr, iek_base_class);
#if DO_IL_LOWERING && IA64_ABI
          walk_list_not_needed(ctsp->vcall_offsets, a_vcall_offset_entry_ptr, 
                               iek_vcall_offset_entry);
#endif /* DO_IL_LOWERING && IA64_ABI */
          walk_list_not_needed(ctsp->friends, an_il_entity_list_entry_ptr,
                               iek_il_entity_list_entry);
#if MAINTAIN_CLASS_MEMBER_LIST
          walk_list_not_needed(ctsp->member_declarations,
                               an_il_entity_list_entry_ptr,
                               iek_il_entity_list_entry);
#endif /* MAINTAIN_CLASS_MEMBER_LIST */
#if MICROSOFT_EXTENSIONS_ALLOWED
          conditionally_clear_fe_pointer(ctsp->partial_class_bodies);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NEW_CAN_BE_FOLDED_INTO_CTOR
          remap_ptr(ctsp->assoc_operator_new_routine, a_routine_ptr,
                    iek_routine);
#if !DO_IL_LOWERING
          if (ctsp->assoc_operator_new_routine != NULL) {
            set_proper_routine_definition_needed_flag(
                                             ctsp->assoc_operator_new_routine);
          }  /* if */
#endif /* !DO_IL_LOWERING */
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
#if DELETE_CAN_BE_FOLDED_INTO_DTOR
          remap_ptr(ctsp->assoc_operator_delete_routine, a_routine_ptr,
                    iek_routine);
#if !DO_IL_LOWERING
          if (ctsp->assoc_operator_delete_routine != NULL) {
            set_proper_routine_definition_needed_flag(
                                          ctsp->assoc_operator_delete_routine);
          }  /* if */
#endif /* !DO_IL_LOWERING */
#endif /* DELETE_CAN_BE_FOLDED_INTO_DTOR */
#if DO_IL_LOWERING
          conditionally_clear_fe_pointer(ctsp->virtual_function_table_var);
#if IA64_ABI
          conditionally_clear_fe_pointer(ctsp->virtual_table_table_var);
#endif /* IA64_ABI */
          remap_ptr_not_needed(ctsp->subobject_partner, a_type_ptr, iek_type);
#if MICROSOFT_EXTENSIONS_ALLOWED
          conditionally_clear_fe_pointer(ctsp->uuid_variable);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
          conditionally_clear_fe_pointer(ctsp->promoted_local_types);
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
#else /* !DO_IL_LOWERING */
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
          if (ctsp->anonymous_union_kind ==
                                       (an_anonymous_union_kind)auk_variable) {
            /* Deal with cases like
                 static union { typedef int T; };
                 T x;
               (Defining types in an anonymous union is no longer allowed,
               but we accept it for backwards compatibility.)
               When the type is marked as needed, the variable for the
               anonymous union must also be marked.  This is a problem only
               for types in anonymous unions; references to data members will
               include a reference to the variable. */
            if (ctsp->assoc_scope->types != NULL) {
              a_variable_ptr anon_union_var;
              /* Recall that type points to a class pointer. */
              anon_union_var = find_parent_var_of_anon_union_type(type);
              walk_ptr(anon_union_var, a_variable_ptr, iek_variable);
            }  /* if */
          }  /* if */
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
#endif /* DO_IL_LOWERING */
        }  /* if */
#if !NEEDED_FLAG_WALK
        if (ctsp->defined_in_variable_initializer) {
          remap_ptr(ctsp->lambda_parent.variable, a_variable_ptr,
                    iek_variable);
        } else if (ctsp->defined_in_field_initializer) {
          remap_ptr(ctsp->lambda_parent.field, a_field_ptr, iek_field);
        } else {
          remap_ptr(ctsp->lambda_parent.routine, a_routine_ptr, iek_routine);
        }  /* if */
#endif /* !NEEDED_FLAG_WALK */
#if MICROSOFT_EXTENSIONS_ALLOWED
        walk_ptr_not_needed(ctsp->corresponding_basic_type, a_type_ptr,
                            iek_type);
        walk_ptr_not_needed(ctsp->base_dispose_bool_routine, a_routine_ptr,
                            iek_routine);
        walk_ptr_not_needed(ctsp->base_idisposable_dispose_routine,
                            a_routine_ptr, iek_routine);
        walk_ptr_not_needed(ctsp->base_object_finalize_routine, a_routine_ptr,
                            iek_routine);
        walk_ptr(ctsp->invocation_type, a_type_ptr, iek_type);
        walk_ptr(ctsp->event_interfaces, an_event_interface_ptr,
                 iek_event_interface);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        walk_ptr(ctsp->proxy_of_type, a_type_ptr, iek_type);
      } (
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
         entry_kind == iek_type ?
               /* Processing comes here from the class type. */
               ((a_type_ptr)entry_ptr)->variant.class_struct_union.extra_info :
               (a_class_type_supplement_ptr)entry_ptr,
         /* For the "needed" and "keep_in_il" walk we have to be able to know
            where the class type is. */
         entry_kind == iek_type ? (a_type_ptr)entry_ptr : NULL
#else /* !(NEEDED_FLAG_WALK || KEEP_IN_IL_WALK) */
         (a_class_type_supplement_ptr)entry_ptr
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
        );
      break;
    case iek_template_param_type_supplement:
      {
#define eptr ((a_template_param_type_supplement_ptr)entry_ptr)
        /* Use walk_ptr instead of remap_ptr because proxy classes are
           not linked into the IL. */
        walk_ptr(eptr->class_type, a_type_ptr, iek_type);
        remap_ptr(eptr->orig_nested_type, a_type_ptr, iek_type);
        conditionally_clear_fe_pointer(eptr->template_symbol);
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (all_template_info_in_il) {
          remap_ptr(eptr->generic_constraints, a_generic_constraint_ptr,
                    iek_generic_constraint);
        } else {
          walk_ptr(eptr->generic_constraints, a_generic_constraint_ptr,
                   iek_generic_constraint);
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        if (eptr->coordinates.depth == BIT_PRECISE_INT_NESTING_DEPTH) {
          walk_ptr(eptr->constraint.bit_width_constant, a_constant_ptr,
                   iek_constant);
        } else if (eptr->coordinates.depth ==
                                   CLASS_TEMPLATE_PLACEHOLDER_NESTING_DEPTH) {
          conditionally_clear_fe_pointer(
                                       eptr->constraint.class_template_symbol);
        } else {
          walk_ptr(eptr->constraint.type_constraint, an_expr_node_ptr,
                   iek_expr_node);
        }  /* if */
#undef eptr
      }
      break;
    case iek_typeref_type_supplement:
      {
#define eptr ((a_typeref_type_supplement_ptr)entry_ptr)
        walk_list(eptr->template_arg_list, a_template_arg_ptr,
                  iek_template_arg);
        if (!prototype_instantiations_in_il) {
          conditionally_clear_fe_pointer(eptr->orig_template_arg_list);
        } else {
          walk_list(eptr->orig_template_arg_list, a_template_arg_ptr,
                    iek_template_arg);
        }  /* if */
#if DEFAULT_RECORD_FORM_OF_NAME_REFERENCE
        walk_list(eptr->name_qualifier, a_name_qualifier_ptr,
                  iek_name_qualifier);
#if !STANDALONE_UTILITY_PROGRAM
        if (eptr->name_qualifier != NULL) {
          /* These should not be created if not recording name references.
             However, the record_form_of_name_reference flag is not
             reliable in standalone utility programs. */
          check_assertion(record_form_of_name_reference);
        }  /* if */
#endif /* !STANDALONE_UTILITY_PROGRAM */
#endif /* DEFAULT_RECORD_FORM_OF_NAME_REFERENCE */
        remap_ptr(eptr->assoc_template, a_template_ptr, iek_template);
        walk_ptr(eptr->expr, an_expr_node_ptr, iek_expr_node);
        walk_ptr(eptr->proxy_class, a_type_ptr, iek_type);
        walk_ptr(eptr->operator_type_arg, a_type_ptr, iek_type);
#undef eptr
      }
      break;
    case iek_constructor_init:
      {
#define eptr ((a_constructor_init_ptr)entry_ptr)
        remap_next_ptr(eptr->next, a_constructor_init_ptr,
                       iek_constructor_init);
        switch (eptr->kind) {
          case cik_virtual_base_class:
          case cik_direct_base_class:
            /* With prototype instantiations, there can be generated base
               class entries. */
            walk_ptr(eptr->variant.base_class, a_base_class_ptr,
                     iek_base_class);
            break;
          case cik_field:
            remap_ptr(eptr->variant.field, a_field_ptr, iek_field);
            break;
          case cik_delegation:
            /* No variant field. */
            break;
          default:
            unexpected_condition_str(
                          "walk_entry_and_subtree: bad constructor init kind");
        }  /* switch */
        walk_ptr(eptr->initializer, a_dynamic_init_ptr, iek_dynamic_init);
        walk_ptr(eptr->source_expr, an_expr_node_ptr, iek_expr_node);
        walk_ptr(eptr->orig_type, a_type_ptr, iek_type);
#undef eptr
      }
      break;
    case iek_asm_entry:
      {
#define eptr ((an_asm_entry_ptr)entry_ptr)
        walk_source_corresp(eptr->source_corresp);
        remap_next_ptr(eptr->next, an_asm_entry_ptr, iek_asm_entry);
        walk_ptr(eptr->asm_string, a_constant_ptr, iek_constant);
#if GNU_EXTENSIONS_ALLOWED
        walk_list(eptr->operands, an_asm_operand_ptr, iek_asm_operand);
        walk_list(eptr->clobbers, a_named_register_list_ptr, 
                  iek_named_register_list);
        walk_list(eptr->labels, a_label_list_ptr, iek_label_list);
#endif /* GNU_EXTENSIONS_ALLOWED */
#undef eptr
      }
      break;
#if GNU_EXTENSIONS_ALLOWED
    case iek_asm_operand:
      {
#define eptr ((an_asm_operand_ptr)entry_ptr)
        remap_next_ptr(eptr->next, an_asm_operand_ptr, iek_asm_operand);
        walk_string_ptr(eptr->name, iek_other_text, 0);
#if RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
        walk_string_ptr(eptr->constraints_string, iek_other_text, 0);
#else /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
        walk_list(eptr->constraints,
                  an_asm_operand_constraint_ptr, iek_asm_operand_constraint);
#endif /* RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
        walk_ptr(eptr->expression, an_expr_node_ptr, iek_expr_node);
#undef eptr
      }
      break;
#if !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
    case iek_asm_operand_constraint:
      {
#define eptr ((an_asm_operand_constraint_ptr)entry_ptr)
#if !DO_SUBTREE_WALK
        remap_next_ptr(eptr->next, an_asm_operand_constraint_ptr,
                       iek_asm_operand_constraint);
#endif /* !DO_SUBTREE_WALK */
#if GNU_X86_ASM_EXTENSIONS_ALLOWED
        walk_string_ptr(eptr->cond_code, iek_other_text, 0);
#endif /* GNU_X86_ASM_EXTENSIONS_ALLOWED */
#undef eptr
      }
      break;
#endif /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
    case iek_named_register_list:
#if !DO_SUBTREE_WALK
      {
#define eptr ((a_named_register_list_ptr)entry_ptr)
        remap_next_ptr(eptr->next, a_named_register_list_ptr,
                       iek_named_register_list);
#undef eptr
      }
#endif /* !DO_SUBTREE_WALK */
      break;
    case iek_label_list:
      {
#define eptr ((a_label_list_ptr)entry_ptr)
        remap_next_ptr(eptr->next, a_label_list_ptr, iek_label_list);
        remap_ptr(eptr->label, a_label_ptr, iek_label);
#undef eptr
      }
      break;
#endif /* GNU_EXTENSIONS_ALLOWED */
    case iek_template_arg:
      {
#define eptr ((a_template_arg_ptr)entry_ptr)
        remap_next_ptr(eptr->next, a_template_arg_ptr, iek_template_arg);
        if (is_type_templ_arg(eptr)) {
          walk_ptr(eptr->variant.type, a_type_ptr, iek_type);
        } else if (is_nontype_templ_arg(eptr)) {
          if (!eptr->is_array_bound_of_unknown_type) {
            walk_ptr(eptr->variant.constant, a_constant_ptr, iek_constant);
          }  /* if */
        } else if (is_template_templ_arg(eptr)) {
          /* A template template argument. */
          walk_ptr(eptr->variant.templ.ptr, a_template_ptr, iek_template);
        } else if (is_start_of_pack_expansion_templ_arg(eptr)) {
          /* A start of pack expansion argument. */
        } else {
          unexpected_condition();
        }  /* if */
        conditionally_clear_fe_pointer(eptr->arg_operand);
        conditionally_clear_fe_pointer(eptr->pack_expansion_descr);
#undef eptr
      }
      break;
    case iek_new_delete_supplement:
      {
#define eptr ((a_new_delete_supplement_ptr)entry_ptr)
        walk_ptr(eptr->type, a_type_ptr, iek_type);
        definition_needed_if_class(eptr->type);
        walk_ptr(eptr->routine, a_routine_ptr, iek_routine);
        if (eptr->routine != NULL) {
          set_proper_routine_definition_needed_flag(eptr->routine);
        }  /* if */
        walk_list(eptr->arg, an_expr_node_ptr, iek_expr_node);
        walk_ptr(eptr->dynamic_init, a_dynamic_init_ptr, iek_dynamic_init);
        walk_ptr(eptr->freeing_of_storage_on_exception, a_dynamic_init_ptr,
                 iek_dynamic_init);
        walk_ptr(eptr->number_of_elements, an_expr_node_ptr, iek_expr_node);
#undef eptr
      }
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case iek_gcnew_supplement:
      {
#define eptr ((a_gcnew_supplement_ptr)entry_ptr)
        walk_ptr(eptr->type, a_type_ptr, iek_type);
        definition_needed_if_class(eptr->type);
        walk_list(eptr->cli_array_dimension_lengths, an_expr_node_ptr,
                  iek_expr_node);
        walk_ptr(eptr->dynamic_init, a_dynamic_init_ptr, iek_dynamic_init);
#undef eptr
      }
    break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case iek_throw_supplement:
      {
#define eptr ((a_throw_supplement_ptr)entry_ptr)
        walk_ptr(eptr->type, a_type_ptr, iek_type);
        definition_needed_if_class(eptr->type);
        walk_ptr(eptr->dynamic_init, a_dynamic_init_ptr, iek_dynamic_init);
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
        walk_ptr(eptr->expr, an_expr_node_ptr, iek_expr_node);
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#if !ABI_CHANGES_FOR_RTTI
        walk_list_not_needed(eptr->accessible_base_classes,
                             an_accessible_base_class_ptr,
                             iek_accessible_base_class);
#endif /* !ABI_CHANGES_FOR_RTTI */
        remap_ptr(eptr->destructor, a_routine_ptr, iek_routine);
        if (eptr->destructor != NULL) {
          set_proper_routine_definition_needed_flag(eptr->destructor);
        }  /* if */
#undef eptr
      }
      break;
    case iek_condition_supplement:
      {
#define eptr ((a_condition_supplement_ptr)entry_ptr)

        walk_ptr_not_needed(eptr->scope, a_scope_ptr, iek_scope);
        walk_ptr(eptr->dynamic_init, a_dynamic_init_ptr, iek_dynamic_init);
        walk_ptr(eptr->expr, an_expr_node_ptr, iek_expr_node);
        walk_ptr(eptr->initialization, a_statement_ptr, iek_statement);
#undef eptr
      }
      break;
#if !ABI_CHANGES_FOR_RTTI
#if !NEEDED_FLAG_WALK
    case iek_accessible_base_class:
      {
#define eptr ((an_accessible_base_class_ptr)entry_ptr)
        remap_next_ptr(eptr->next, an_accessible_base_class_ptr,
                       iek_accessible_base_class);
        remap_ptr(eptr->base_class, a_base_class_ptr, iek_base_class);
#undef eptr
      }
      break;
#endif /* !NEEDED_FLAG_WALK */
#endif /* !ABI_CHANGES_FOR_RTTI */
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
    case iek_eh_prologue_supplement:
      {
#define eptr ((an_eh_prologue_supplement_ptr)entry_ptr)
        remap_ptr(eptr->routine, a_routine_ptr, iek_routine);
#if GENERATE_EH_TABLES
        remap_ptr(eptr->region_table, a_variable_ptr, iek_variable);
        remap_ptr(eptr->array_table, a_variable_ptr, iek_variable);
#endif /* GENERATE_EH_TABLES */
#undef eptr
      }
      break;
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#if GENERATE_SOURCE_SEQUENCE_LISTS && !NEEDED_FLAG_WALK
    case iek_source_sequence_entry:
      [] (a_source_sequence_entry_ptr eptr) {
        an_il_entry_kind            kind = (an_il_entry_kind)eptr->entity.kind;

#if !KEEP_IN_IL_WALK
        remap_next_ptr(eptr->next, a_source_sequence_entry_ptr,
                       iek_source_sequence_entry);
        remap_ptr(eptr->prev, a_source_sequence_entry_ptr,
                  iek_source_sequence_entry);
#endif /* !KEEP_IN_IL_WALK */
#if CHECKING
        /* Check for empty source sequence entries that remain in the IL. */
        if (kind == (an_il_entry_kind)iek_none) {
          unexpected_condition_str(
                        "walk_entry_and_subtree: empty source sequence entry");
        }  /* if */
#endif /* CHECKING */
        /* Types get walked instead of remapped because some types defined
           in prototype scopes in C (e.g., in a cast) get eliminated from the
           IL.  Secondary declarations, end of construct entries, instantiation
           directives, module import declarations, static assertions, and
           linkage specification blocks get walked because they are in effect
           supplements to the source sequence entry rather than freestanding IL
           entries; they aren't pointed to from elsewhere in the IL tree. */
        if (kind == iek_type ||
            kind == iek_src_seq_secondary_decl ||
            kind == iek_src_seq_end_of_construct ||
            kind == iek_instantiation_directive ||
            kind == iek_module_import_decl ||
#if GENERATE_LINKAGE_SPEC_BLOCKS
            kind == iek_linkage_spec_block ||
#endif /* GENERATE_LINKAGE_SPEC_BLOCKS */
            kind == iek_static_assertion ||
            kind == iek_statement) {
          walk_ptr(eptr->entity.ptr, a_char_ptr, kind);
        } else {
          remap_ptr(eptr->entity.ptr, a_char_ptr, kind);
        }  /* if */
      } ((a_source_sequence_entry_ptr)entry_ptr);
      break;
    case iek_src_seq_secondary_decl:
      [] (a_src_seq_secondary_decl_ptr eptr) {
        an_il_entry_kind kind = (an_il_entry_kind)eptr->entity.kind;
        /* Types get walked instead of remapped because some types defined
           in prototype scopes in C (e.g., in a cast) get eliminated from the
           IL.  Similarly, friend function declarations may refer to routines
           that do not appear on any list (e.g., dependent class members) and
           must be walked here. */
        if (kind == iek_type || eptr->friend_decl) {
          walk_ptr(eptr->entity.ptr, a_char_ptr, kind);
        } else {
          remap_ptr(eptr->entity.ptr, a_char_ptr, kind);
        }  /* if */
        walk_ptr(eptr->declared_type, a_type_ptr, iek_type);
        walk_ptr(eptr->name_reference, a_name_reference_ptr,
                 iek_name_reference);
        walk_list(eptr->attributes, an_attribute_ptr, iek_attribute);
#if EXTRA_SOURCE_POSITIONS_IN_IL
        walk_ptr(eptr->decl_pos_info, a_decl_position_supplement_ptr,
                 iek_decl_position_supplement);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      } ((a_src_seq_secondary_decl_ptr)entry_ptr);
      break;
    case iek_src_seq_end_of_construct:
      [] (a_src_seq_end_of_construct_ptr eptr) {
        an_il_entry_kind kind = (an_il_entry_kind)eptr->entity.kind;
        /* Types get walked instead of remapped because some types defined
           in prototype scopes in C (e.g., in a cast) get eliminated from the
           IL. */
        if (kind == iek_type) {
          walk_ptr(eptr->entity.ptr, a_char_ptr, kind);
        } else {
          remap_ptr(eptr->entity.ptr, a_char_ptr, kind);
        }  /* if */
      } ((a_src_seq_end_of_construct_ptr)entry_ptr);
      break;
#if !KEEP_IN_IL_WALK
    case iek_src_seq_sublist:
      {
#define eptr ((a_src_seq_sublist_ptr)entry_ptr)
        remap_next_ptr(eptr->next, a_src_seq_sublist_ptr, iek_src_seq_sublist);
        walk_list(eptr->source_sequence_list, a_source_sequence_entry_ptr,
                  iek_source_sequence_entry);
        remap_ptr(eptr->last_source_sequence_entry,
                  a_source_sequence_entry_ptr, iek_source_sequence_entry);
#undef eptr
      }
      break;
#endif /* !KEEP_IN_IL_WALK */
    case iek_instantiation_directive:
      {
#define eptr ((an_instantiation_directive_ptr)entry_ptr)
        remap_ptr(eptr->entity.ptr, a_char_ptr,
                  (an_il_entry_kind)eptr->entity.kind);
        walk_list(eptr->attributes, an_attribute_ptr, iek_attribute);
#if GENERATE_SOURCE_SEQUENCE_LISTS
        walk_ptr(eptr->declared_type, a_type_ptr, iek_type);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if EXTRA_SOURCE_POSITIONS_IN_IL
        walk_ptr(eptr->decl_pos_info, a_decl_position_supplement_ptr,
                 iek_decl_position_supplement);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#undef eptr
      }
      break;
#if GENERATE_LINKAGE_SPEC_BLOCKS
    case iek_linkage_spec_block:
      {
#define eptr ((a_linkage_spec_block_ptr)entry_ptr)
        walk_ptr(eptr->name_string, a_constant_ptr, iek_constant);
#undef eptr
      }
      break;
#endif /* GENERATE_LINKAGE_SPEC_BLOCKS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS && ... */
    case iek_static_assertion:
      {
#define eptr ((a_static_assertion_ptr)entry_ptr)
        walk_ptr(eptr->condition, a_constant_ptr, iek_constant);
        walk_ptr(eptr->string_literal, a_constant_ptr, iek_constant);
#undef eptr
      }
      break;
    case iek_scope_orphaned_list_header:
      {
#define eptr ((a_scope_orphaned_list_header_ptr)entry_ptr)
        remap_next_ptr(eptr->next, a_scope_orphaned_list_header_ptr,
                       iek_scope_orphaned_list_header);
        remap_ptr(eptr->assoc_routine, a_routine_ptr, iek_routine);
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
        /* Don't walk these lists.  They will have been walked from the
           function scope if necessary. */
        remap_list_ptr(eptr->orphaned_types, a_type_ptr, iek_type);
        remap_list_ptr(eptr->orphaned_variables, a_variable_ptr,
                       iek_variable);
        remap_list_ptr(eptr->orphaned_namespaces, a_namespace_ptr,
                       iek_namespace);
#else /* !(NEEDED_FLAG_WALK || KEEP_IN_IL_WALK) */
        walk_list(eptr->orphaned_types, a_type_ptr, iek_type);
        walk_list(eptr->orphaned_variables, a_variable_ptr, iek_variable);
        walk_list(eptr->orphaned_namespaces, a_namespace_ptr, iek_namespace);
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
#if GENERATE_SOURCE_SEQUENCE_LISTS && !NEEDED_FLAG_WALK && !KEEP_IN_IL_WALK
        walk_list(eptr->orphaned_src_seq_sublists,
                  a_src_seq_sublist_ptr, iek_src_seq_sublist);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS && */
#undef eptr
      }
      break;
#if ONE_INSTANTIATION_PER_OBJECT
    case iek_per_instantiation_needed_flags_entry:
      {
#if !DO_SUBTREE_WALK
#define eptr ((a_per_instantiation_needed_flags_entry_ptr)entry_ptr)
        remap_next_ptr(eptr->next, a_per_instantiation_needed_flags_entry_ptr,
                       iek_per_instantiation_needed_flags_entry);
#undef eptr
#endif /* !DO_SUBTREE_WALK */
      }
      break;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
    case iek_local_expr_node_ref:
      {
#define eptr ((a_local_expr_node_ref_ptr)entry_ptr)
        remap_next_ptr(eptr->next, a_local_expr_node_ref_ptr,
                       iek_local_expr_node_ref);
        walk_ptr(eptr->expr, an_expr_node_ptr, iek_expr_node);
        walk_ptr(eptr->referrer.ptr, a_char_ptr,
                 (an_il_entry_kind)eptr->referrer.kind);
#undef eptr
      }
      break;
    case iek_local_scope_ref:
      {
#define eptr ((a_local_scope_ref_ptr)entry_ptr)
        remap_next_ptr(eptr->next, a_local_scope_ref_ptr, iek_local_scope_ref);
        remap_ptr_not_needed(eptr->scope, a_scope_ptr, iek_scope);
        remap_ptr_not_needed(eptr->referrer.ptr, a_char_ptr,
                             (an_il_entry_kind)eptr->referrer.kind);
#undef eptr
      }
      break;
    case iek_il_entity_list_entry:
      {
#define eptr ((an_il_entity_list_entry_ptr)entry_ptr)
        remap_next_ptr(eptr->next, an_il_entity_list_entry_ptr,
                       iek_il_entity_list_entry);
        walk_ptr_not_needed(eptr->entity.ptr, a_char_ptr,
                            (an_il_entry_kind)eptr->entity.kind);
#undef eptr
      }
      break;
    case iek_lambda:
      {
#define eptr ((a_lambda_ptr)entry_ptr)
        walk_list(eptr->capture_list, a_lambda_capture_ptr,
                  iek_lambda_capture);
        walk_ptr(eptr->closure_class, a_type_ptr, iek_type);
        set_proper_definition_needed_flag(eptr->closure_class);
        if (eptr->is_generic && !prototype_instantiations_in_il) {
          conditionally_clear_fe_pointer(eptr->lambda_routine);
        } else {
          walk_ptr(eptr->lambda_routine, a_routine_ptr, iek_routine);
        }  /* if */
        set_proper_routine_definition_needed_flag(eptr->lambda_routine);
#undef eptr
      }
      break;
    case iek_lambda_capture:
      {
#define eptr ((a_lambda_capture_ptr)entry_ptr)
        remap_next_ptr(eptr->next, a_lambda_capture_ptr, iek_lambda_capture);
        if (eptr->is_init_capture) {
          walk_ptr(eptr->captured.initializer, a_dynamic_init_ptr,
                   iek_dynamic_init);
          conditionally_clear_fe_pointer(eptr->capture_info.init_capture_dps);
        } else {
          if (eptr->is_indirect_init_capture) {
            remap_ptr(eptr->captured.init_capture_field, a_field_ptr,
                      iek_field);
          } else {
            remap_ptr(eptr->captured.variable, a_variable_ptr, iek_variable);
          }  /* if */
          remap_ptr(eptr->capture_info.source_closure_field, a_field_ptr,
                    iek_field);
        }  /* if */
        remap_ptr(eptr->closure_field, a_field_ptr, iek_field);
#undef eptr
      }
      break;
    case iek_attribute:
      {
#define eptr ((an_attribute_ptr)entry_ptr)
        remap_next_ptr(eptr->next, an_attribute_ptr, iek_attribute);
        walk_string_ptr(eptr->name, iek_id_name, 0);
        walk_string_ptr(eptr->namespace_name, iek_id_name, 0);
        /* Argument entries may be shared between copies of an attribute (e.g.,
           the attribute entries associated with a_routine and their copies
           recorded in a secondary source sequence entry for that routine).
           As a consequence, walk_list cannot be used here. */
        walk_ptr(eptr->arguments, an_attribute_arg_ptr, iek_attribute_arg);
        walk_ptr(eptr->group, an_attribute_group_ptr, iek_attribute_group);
        conditionally_clear_fe_pointer(eptr->assoc_info);
        conditionally_clear_fe_pointer(eptr->pack_expansion_descr);
#undef eptr
      }
      break;
    case iek_attribute_arg:
      {
#define eptr ((an_attribute_arg_ptr)entry_ptr)
        walk_next_ptr(eptr->next, an_attribute_arg_ptr, iek_attribute_arg);
        conditionally_clear_fe_pointer(eptr->pack_expansion_descr);
        switch (eptr->kind) {
          case aak_empty:
          case aak_last:
            /* Nothing to do. */
            break;
          case aak_token:
          case aak_raw_token:
            walk_string_ptr(eptr->variant.token, iek_id_name, 0);
            break;
          case aak_constant:
            walk_ptr(eptr->variant.constant, a_constant_ptr, iek_constant);
            break;
          case aak_type:
            walk_ptr(eptr->variant.type, a_type_ptr, iek_type);
            break;
          case aak_expression:
            if (eptr->local_expr_ref) {
              walk_ptr(eptr->variant.sexpr, a_scoped_expression_ptr,
                       iek_scoped_expression);
            } else {
              walk_ptr(eptr->variant.expr, an_expr_node_ptr, iek_expr_node);
            }  /* if */
            break;
          default_is_unexpected();
        }  /* switch */
#undef eptr
      }
      break;
    case iek_attribute_group:
      /* No pointer members. */
      break;
    case iek_integer_type_supplement:
      {
#define eptr ((an_integer_type_supplement_ptr)entry_ptr)
#if MICROSOFT_EXTENSIONS_ALLOWED
        walk_string_ptr(eptr->uuid_string, iek_other_text, 0);
        remap_ptr(eptr->boxed_type, a_type_ptr, iek_type);
#if DO_IL_LOWERING
        conditionally_clear_fe_pointer(eptr->uuid_variable);
#endif /* DO_IL_LOWERING */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        walk_ptr(eptr->base_type, a_type_ptr, iek_type);
        remap_ptr(eptr->assoc_template, a_template_ptr, iek_template);
#undef eptr
      }
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case iek_cli_metadata_file:
      {
#define eptr ((a_cli_metadata_file_ptr)entry_ptr)
        remap_next_ptr(eptr->next, a_cli_metadata_file_ptr,
                       iek_cli_metadata_file);
        walk_string_ptr(eptr->name_as_written, iek_other_text, 0);
        walk_string_ptr(eptr->full_name, iek_other_text, 0);
#undef eptr
      }
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case iek_coroutine_descr:
      {
#define eptr ((a_coroutine_descr_ptr)entry_ptr)
        remap_ptr(eptr->traits, a_type_ptr, iek_type);
        walk_ptr(eptr->handle, a_variable_ptr, iek_variable);
        walk_ptr(eptr->promise, a_variable_ptr, iek_variable);
        walk_ptr(eptr->init_await_resume, a_variable_ptr, iek_variable);
        walk_ptr(eptr->this_param_copy, a_variable_ptr, iek_variable);
        /* scope->nonstatic_variables points to the same list, so the list
           has already been walked. */
        remap_list_ptr(eptr->parameter_copies, a_variable_ptr, iek_variable);
        remap_ptr(eptr->final_suspend_label, a_label_ptr, iek_label);
        walk_ptr(eptr->initial_suspend_call, an_expr_node_ptr, iek_expr_node);
        walk_ptr(eptr->final_suspend_call, an_expr_node_ptr, iek_expr_node);
        walk_ptr(eptr->unhandled_exception_call, an_expr_node_ptr,
                 iek_expr_node);
        walk_ptr(eptr->get_return_object_call, an_expr_node_ptr,
                 iek_expr_node);
        walk_ptr(eptr->alloc_failure_gro_call, an_expr_node_ptr,
                 iek_expr_node);
        walk_ptr(eptr->new_routine, a_routine_ptr, iek_routine);
        walk_ptr(eptr->delete_routine, a_routine_ptr, iek_routine);
#undef eptr
      }
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case iek_event_interface:
      {
#define eptr ((an_event_interface_ptr)entry_ptr)
        walk_next_ptr(eptr->next, an_event_interface_ptr,
                      iek_event_interface);
        remap_ptr(eptr->interface_type, a_type_ptr, iek_type);
#undef eptr
      }
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case iek_subobject_path:
      {
#define eptr ((a_subobject_path_ptr)entry_ptr)
        remap_next_ptr(eptr->next, a_subobject_path_ptr, iek_subobject_path);
        if (eptr->is_base_class) {
          remap_ptr(eptr->variant.base_class, a_base_class_ptr,
                    iek_base_class);
        } else if (!eptr->is_offset) {
          remap_ptr(eptr->variant.field, a_field_ptr, iek_field);
        }  /* if */
#undef eptr
      }
      break;
    case iek_constexpr_if:
      {
#define eptr ((a_constexpr_if_ptr)entry_ptr)
        walk_ptr(eptr->then_statement, a_statement_ptr, iek_statement);
        walk_ptr(eptr->else_statement, a_statement_ptr, iek_statement);
#undef eptr
      }
      break;
    case iek_module:
      {
#define eptr ((a_module_ptr)entry_ptr)
        /* FIXME: make sure all fields are walked appropriately. */
        walk_string_ptr(eptr->resolved_file, iek_other_text, 0);
        switch (eptr->kind) {
          case mk_none:
            break;
          case mk_header_unit:
            walk_string_ptr(eptr->variant.header_unit.name,
                            iek_other_text, 0);
            walk_string_ptr(eptr->variant.header_unit.resolved_header,
                            iek_other_text, 0);
            break;
          case mk_unit:
            walk_string_ptr(eptr->variant.unit.name, iek_id_name, 0);
            break;
          case mk_unit_partition:
            walk_string_ptr(eptr->variant.unit_partition.name, iek_id_name,
                            0);
            walk_ptr(eptr->variant.unit_partition.unit, a_module_ptr,
                     iek_module);
            break;
          default_is_unexpected();
        }  /* switch */
#undef eptr
      }
      break;
    case iek_module_import_decl:
      {
#define eptr ((a_module_import_decl_ptr)entry_ptr)
        /* FIXME: make sure all fields are walked appropriately. */
        remap_next_ptr(eptr->next, a_module_import_decl_ptr,
                       iek_module_import_decl);
        walk_list(eptr->attributes, an_attribute_ptr, iek_attribute);
        walk_ptr(eptr->module_info, a_module_ptr, iek_module);
#undef eptr
      }
      break;
    case iek_token_sequence:
#define eptr ((a_token_sequence*)entry_ptr)
      walk_list(eptr->tokens, a_token_sequence_entry_ptr,
                iek_token_sequence_entry);
      conditionally_clear_fe_pointer(eptr->token_cache);
#undef eptr
      break;
    case iek_token_sequence_entry:
#define eptr ((a_token_sequence_entry*)entry_ptr)
      remap_next_ptr(eptr->next, a_token_sequence_entry_ptr,
                     iek_token_sequence_entry);
      walk_string_ptr(eptr->spelling, iek_other_text, 0);
#undef eptr
      break;
    case iek_scoped_expression:
#define eptr ((a_scoped_expression*)entry_ptr)
      walk_source_corresp(eptr->source_corresp);
      walk_ptr(eptr->expr, an_expr_node_ptr, iek_expr_node);
#undef eptr
      break;
    case iek_data_member_spec:
#define eptr ((a_data_member_spec*)entry_ptr)
      walk_ptr(eptr->type, a_type_ptr, iek_type);
      walk_string_ptr(eptr->name, iek_id_name, 0);
      walk_list(eptr->annotations, an_attribute_ptr, iek_attribute);
#undef eptr
      break;
    case iek_id_name:
    case iek_string_text:
    case iek_other_text:
      /* String entries should go to walk_string_entry */
    case iek_none:
    case iek_last:
    default:
      unexpected_condition_str("walk_entry_and_subtree: bad entry kind");
  }  /* switch */
#if DO_SUBTREE_WALK
  /* Call the routine to process the entry if there is such a routine. */
  if (entry_process_func != NULL) entry_process_func(entry_ptr, entry_kind);
  if (next_entry_ptr != NULL) {
    /* A "next" pointer has been set; continue the walk with that entry.
       This uses iteration rather than recursion (to minimize stack usage). */
    entry_ptr = next_entry_ptr;
    next_entry_ptr = NULL;
    goto handle_next_entry;
  }  /* if */
end_of_routine:;
#endif /* DO_SUBTREE_WALK */
}  /* walk_entry_and_subtree */

#undef walk_or_remap_parent
#undef walk_source_corresp
#undef walk_source_corresp_full

#ifdef WALK_ORPHANED_ENTRY_ROUTINE_NAME

/*
Process a list of identical type orphaned file scope IL entries linked
together by the orphaned pointer preceding the IL entry structure.  Each
IL entry will be processed as an individual IL entry.  walk_ptr will be used
to process each entry in the list.  ptr is the pointer to the first IL entry,
ptr_type is the type of the pointer and entry_kind is the kind of entries.
*/
#undef walk_orphan_entry_list
#define walk_orphan_entry_list(ptr, ptr_type, entry_kind) \
{ ptr_type *orph_ptr = (ptr_type *)&(ptr); \
  for (; *orph_ptr != NULL; \
       orph_ptr = (ptr_type *)&fs_orphan_pointer_of(*orph_ptr)) { \
    walk_ptr(*orph_ptr, ptr_type, entry_kind) \
  }  /* for */ \
}  /* walk_orphan_entry_list */

/*
Local macro to ease stepping through the orphaned_file_scopes_il_entries
array and process the lists of IL entries of each type pointed to by the
"first_entry" of each array element.
*/
#undef walk_orphan_entry_list_for_entry_kind
#define walk_orphan_entry_list_for_entry_kind(ptr_type, entry_kind) \
  walk_orphan_entry_list( \
               orphaned_file_scope_il_entries[(int)entry_kind].first_entry, \
               ptr_type, entry_kind)


/* The routine name is a macro so it can be expanded different ways, e.g.,
   as walk_orphaned_file_scope_il_entries. */
static void WALK_ORPHANED_ENTRY_ROUTINE_NAME(void)
/*
For each IL entry kind, process any orphaned file scope IL entries chained
to the orphaned_file_scope_il_entries table.  As function scopes were walked,
any file scope IL entries referenced were added onto the list of orphaned
IL entries.  These IL entries may not be and probably are not referenced
from the file scope IL tree.  Walk through the lists of orphaned IL entries
of each kind.  
*/
{

  /* Process the list of individual IL entries of each IL type. */
  walk_orphan_entry_list_for_entry_kind(a_source_file_ptr, iek_source_file);
  walk_orphan_entry_list_for_entry_kind(a_constant_ptr, iek_constant);
  walk_orphan_entry_list_for_entry_kind(a_param_type_ptr, iek_param_type);
  walk_orphan_entry_list_for_entry_kind(a_routine_type_supplement_ptr,
                                        iek_routine_type_supplement);
  walk_orphan_entry_list_for_entry_kind(a_based_type_list_member_ptr,
                                        iek_based_type_list_member);
  walk_orphan_entry_list_for_entry_kind(a_type_ptr, iek_type);
  walk_orphan_entry_list_for_entry_kind(a_variable_ptr, iek_variable);
  walk_orphan_entry_list_for_entry_kind(a_field_ptr, iek_field);
  walk_orphan_entry_list_for_entry_kind(an_exception_specification_ptr,
                                        iek_exception_specification);
  walk_orphan_entry_list_for_entry_kind(an_exception_specification_type_ptr,
                                        iek_exception_specification_type);
  walk_orphan_entry_list_for_entry_kind(a_routine_ptr, iek_routine);
  walk_orphan_entry_list_for_entry_kind(a_label_ptr, iek_label);
  walk_orphan_entry_list_for_entry_kind(an_expr_node_ptr, iek_expr_node);
  walk_orphan_entry_list_for_entry_kind(a_for_loop_ptr, iek_for_loop);
  walk_orphan_entry_list_for_entry_kind(a_range_based_for_loop_ptr,
                                        iek_range_based_for_loop);
#if MICROSOFT_EXTENSIONS_ALLOWED
  walk_orphan_entry_list_for_entry_kind(a_for_each_loop_ptr,
                                        iek_for_each_loop);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  walk_orphan_entry_list_for_entry_kind(a_switch_case_entry_ptr,
                                        iek_switch_case_entry);
  walk_orphan_entry_list_for_entry_kind(a_switch_stmt_descr_ptr,
                                        iek_switch_stmt_descr);
  walk_orphan_entry_list_for_entry_kind(a_handler_ptr, iek_handler);
  walk_orphan_entry_list_for_entry_kind(a_try_supplement_ptr,
                                        iek_try_supplement);
#if MICROSOFT_EXTENSIONS_ALLOWED
  walk_orphan_entry_list_for_entry_kind(a_microsoft_try_supplement_ptr,
                                        iek_microsoft_try_supplement);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  walk_orphan_entry_list_for_entry_kind(a_block_ptr, iek_block);
  walk_orphan_entry_list_for_entry_kind(a_statement_ptr, iek_statement);
  walk_orphan_entry_list_for_entry_kind(an_object_lifetime_ptr,
                                        iek_object_lifetime);
  walk_orphan_entry_list_for_entry_kind(a_scope_ptr, iek_scope);
  /* The string types iek_id_name, iek_string_text, and iek_other_text
     are not maintained on an orphan list.  String types at the file
     scope that are referenced from a function scope are written in that
     function scope region. */
#if C99_IL_EXTENSIONS_SUPPORTED
  walk_orphan_entry_list_for_entry_kind(an_internal_complex_value_ptr,
                                        iek_internal_complex_value);
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  walk_orphan_entry_list_for_entry_kind(a_namespace_ptr, iek_namespace);
  walk_orphan_entry_list_for_entry_kind(a_using_decl_ptr, iek_using_decl);
  walk_orphan_entry_list_for_entry_kind(a_dynamic_init_ptr, iek_dynamic_init);
  walk_orphan_entry_list_for_entry_kind(an_overriding_virtual_function_ptr,
                                        iek_overriding_virtual_function);
  walk_orphan_entry_list_for_entry_kind(a_derivation_step_ptr,
                                        iek_derivation_step);
  walk_orphan_entry_list_for_entry_kind(a_base_class_derivation_ptr,
                                        iek_base_class_derivation);
  walk_orphan_entry_list_for_entry_kind(a_base_class_ptr, iek_base_class);
  walk_orphan_entry_list_for_entry_kind(a_class_list_entry_ptr,
                                        iek_class_list_entry);
  walk_orphan_entry_list_for_entry_kind(a_routine_list_entry_ptr,
                                        iek_routine_list_entry);
  walk_orphan_entry_list_for_entry_kind(a_class_type_supplement_ptr,
                                        iek_class_type_supplement);
  walk_orphan_entry_list_for_entry_kind(a_template_param_type_supplement_ptr,
                                        iek_template_param_type_supplement);
  walk_orphan_entry_list_for_entry_kind(a_constructor_init_ptr,
                                        iek_constructor_init);
  walk_orphan_entry_list_for_entry_kind(an_asm_entry_ptr, iek_asm_entry);
  walk_orphan_entry_list_for_entry_kind(a_template_arg_ptr, iek_template_arg);
  walk_orphan_entry_list_for_entry_kind(a_new_delete_supplement_ptr,
                                        iek_new_delete_supplement);
#if MICROSOFT_EXTENSIONS_ALLOWED
  walk_orphan_entry_list_for_entry_kind(a_gcnew_supplement_ptr,
                                        iek_gcnew_supplement);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  walk_orphan_entry_list_for_entry_kind(a_throw_supplement_ptr,
                                        iek_throw_supplement);
#if !ABI_CHANGES_FOR_RTTI
  walk_orphan_entry_list_for_entry_kind(an_accessible_base_class_ptr,
                                        iek_accessible_base_class);
#endif /* !ABI_CHANGES_FOR_RTTI */
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
  walk_orphan_entry_list_for_entry_kind(an_eh_prologue_supplement_ptr,
                                        iek_eh_prologue_supplement);
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
  walk_orphan_entry_list_for_entry_kind(a_template_parameter_ptr,
                                        iek_template_parameter);
  walk_orphan_entry_list_for_entry_kind(a_template_decl_ptr,
                                        iek_template_decl);
  walk_orphan_entry_list_for_entry_kind(a_name_reference_ptr,
                                        iek_name_reference);
  walk_orphan_entry_list_for_entry_kind(a_name_qualifier_ptr,
                                        iek_name_qualifier);
  walk_orphan_entry_list_for_entry_kind(an_attribute_ptr, iek_attribute);
  walk_orphan_entry_list_for_entry_kind(a_token_sequence_ptr,
                                        iek_token_sequence);
  walk_orphan_entry_list_for_entry_kind(a_data_member_spec_ptr,
                                        iek_data_member_spec);
  /* Note that no orphan list walking is needed for iek_source_sequence_entry
     nor for its subordinate entries like iek_src_seq_secondary_decl
     and iek_src_seq_end_of_construct, since such entries will
     never appear on an orphan list.  Ditto for iek_src_seq_sublist. */
  /* Likewise iek_per_instantiation_needed_flags_entry. */
  /* Also iek_module_import_decl. */
}  /* walk_orphaned_file_scope_il_entries */

#endif /* ifdef WALK_ORPHANED_ENTRY_ROUTINE_NAME */

#ifdef UNDEF_WALK_ENTRY_MACROS_AT_END
/*
Get rid of the macros defined in this file so they aren't used accidentally.
*/
#undef remap_ptr
#undef remap_list_ptr
#undef remap_next_ptr
#undef remap_ptr_not_needed
#undef walk_ptr
#undef walk_list_ptr
#undef walk_ptr_not_needed
#undef walk_string_ptr
#undef walk_list_on_link_field
#undef walk_list
#undef walk_list_not_needed
#undef walk_needed_on_list
#undef walk_list_with_keep_in_il_reset
#undef simple_walk_param_list_default_arg_exprs
#undef walk_param_list_default_arg_exprs
#undef definition_needed_if_class
#undef set_proper_definition_needed_flag
#undef set_proper_routine_definition_needed_flag
#undef walk_or_remap_parent
#undef remap_source_sequence_entry
#undef walk_source_corresp
#undef walk_source_corresp_full
#undef walk_unmangled_name
#undef conditionally_clear_fe_pointer
#undef pm_class_type_possibly_lowered
#undef report_bad_init_kind
#undef nonstatic_variable_always_needed
#undef walk_initializer
#undef walk_orphan_entry_list
#undef walk_orphan_entry_list_for_entry_kind
#undef clear_or_walk_name_reference_field
#endif /* ifdef UNDEF_WALK_ENTRY_MACROS_AT_END */

