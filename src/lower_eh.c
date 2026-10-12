/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

lower_eh.c -- IL lowering for exception handling constructs.

Note that the processing here comes in three levels:

- With DO_FULL_PORTABLE_EH_LOWERING set, all EH constructs are lowered to
  C.  That means code is generated to maintain an EH stack, try/catch are
  fully lowered (using setjmp), throw is fully lowered, cleanup tables
  and typeinfo entries are generated, and __eh_curr_region is maintained.
  This mode is compatible with the C-generating back end and the minimal
  runtime supplied.  Setting DO_FULL_PORTABLE_EH_LOWERING forces the
  setting of GENERATE_EH_TABLES.
- With GENERATE_EH_TABLES set (and DO_FULL_PORTABLE_EH_LOWERING not set),
  the cleanup tables and typeinfo entries are generated, but throw/try/catch
  are retained.  Other low-level things are represented as
  enk_lowered_eh_construct expression nodes.  Also, the object address
  table is not maintained, and the handle numbers for entities in the
  cleanup tables use stack offsets represented as ck_stack_offset
  constants.  If you use this mode, you'll have to make modifications
  to the EDG-supplied runtime.
- With neither flag set, no tables are generated (but the object lifetime
  information is preserved), throw/try/catch are retained, and all other
  executable constructs are represented as enk_lowered_eh_construct nodes.
  If you use this mode, you'll have to write your own runtime routines.

The statements and expressions attached under exception handling
constructs (e.g., try/catch, throw) are lowered in all modes (as long as
IL lowering itself is done).
*/

#include "basic_hdrs.h"
#if DO_IL_LOWERING
/* Header files common to all files. */
#include "fe_common.h"
/* Header files used by files involved in IL lowering. */
#include "lower_hdrs.h"
#endif /* DO_IL_LOWERING */

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Only include this code if it is needed: */
#if DO_IL_LOWERING

#if MICROSOFT_EXTENSIONS_ALLOWED
#include "decls.h"
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if MAINTAIN_NEEDED_FLAGS
#include "il_walk.h"
#endif /* MAINTAIN_NEEDED_FLAGS */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
This switch controls whether a destructor pointer is passed to the runtime
on a throw setup call.  If the switch is FALSE, no destructor pointer
is passed, and the typeinfo structure contains a pointer to the
destructor for the type, which the runtime uses to determine whether
a destructor should be called.  The TRUE setting selects the newer
approach.
*/
#if ABI_CHANGES_FOR_RTTI && ABI_COMPATIBILITY_VERSION >= 238
#define PASS_DTOR_POINTER_TO_THROW TRUE
#else /* !(ABI_CHANGES_FOR_RTTI && ABI_COMPATIBILITY_VERSION >= 238) */
#define PASS_DTOR_POINTER_TO_THROW FALSE
#endif /* ABI_CHANGES_FOR_RTTI && ABI_COMPATIBILITY_VERSION >= 238 */

/*
Flag that is TRUE during generate_typeinfo_vars.
*/
STATIC_THREAD a_boolean
		in_typeinfo_var_generation_phase;


/* Forward declaration needed because of mutual recursion: */
static a_type_ptr make_base_class_spec_type(void);

static a_type_ptr array_of(a_type_ptr elem_type)
/*
Make an array type whose elements have type elem_type, and return a pointer
to it.  The array is given size [0]; it's assumed that will be changed.
It's also assumed that set_type_size will be called later (e.g., from
finish_array_var).
*/
{
  a_type_ptr array_type = alloc_type((a_type_kind)tk_array);

  array_type->variant.array.variant.number_of_elements = 0; /* Initially. */
  array_type->variant.array.element_type = elem_type;
  /* set_type_size is not called yet. */
  return array_type;
}  /* array_of */

#if GENERATE_EH_TABLES

static a_variable_ptr make_unnamed_local_static_array_var(
                                                  a_type_ptr elem_type,
                                                  a_boolean  in_function_scope)
/*
Create an unnamed local static variable whose type is an array of elem_type,
and return a pointer to the variable.  The array size is begun as [0] and
will be adjusted as elements are added.  finish_array_var must be called
sometime later to set the size on the type.  If in_function_scope is TRUE,
put the variable in the function scope instead of the current scope
(which might be a block scope).
*/
{
  a_variable_ptr var;
  a_type_ptr     array_type = array_of(elem_type);

  /* Make the variable.  It is unnamed and static. */
  var = make_unnamed_local_static_variable(array_type, in_function_scope);
  return var;
}  /* make_unnamed_local_static_array_var */


static a_variable_ptr make_init_unnamed_local_static_array_var(
                                              a_type_ptr     elem_type,
                                              a_boolean      in_function_scope,
                                              a_constant_ptr *aggr_con)
/*
Create an unnamed local static variable whose type is an array of elem_type,
and return a pointer to the variable.  The array size is begun as [0] and
will be adjusted as elements are added.  finish_array_var must be called
sometime later to set the size on the type.  The variable will be initialized;
to start the process, an aggregate constant is attached to the variable.
A pointer to this aggregate constant is returned in *aggr_con.
Initial values must be added under the aggregate.  If in_function_scope
is TRUE, put the variable in the function scope instead of the current
scope (which might be a block scope).
*/
{
  a_variable_ptr var;

  /* Make the variable with an array type. */
  var = make_unnamed_local_static_array_var(elem_type, in_function_scope);
  /* The initial value is an aggregate constant pointing to a list of
     aggregate constants. */
  *aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
  /* Attach the aggregate constant as the initial value of the variable. */
  /* Use a local-static-variable-init entry to indicate the initialization. */
  (void)make_local_static_variable_init(var, 
                                        in_function_scope ?
                                              innermost_function_scope :
                                              curr_context->scope,
                                        (an_init_kind)initk_static,
                                        *aggr_con, (a_dynamic_init_ptr)NULL);
  return var;
}  /* make_init_unnamed_local_static_array_var */

#endif /* GENERATE_EH_TABLES */
#if GENERATE_EH_TABLES || !IA64_ABI

static a_targ_size_t incr_nelems_of_array_var(a_variable_ptr var)
/*
Increment the number of elements of the array variable pointed to by var.
Return the pre-incremented value (which is right as a subscript).
*/
{
  /* Add one to the array size.  set_type_size is called later. */
  return var->type->variant.array.variant.number_of_elements++;
}  /* incr_nelems_of_array_var */


static a_targ_size_t add_elem_to_array_var(a_constant_ptr con,
                                           a_variable_ptr var,
                                           a_constant_ptr aggr_con)
/*
Add the indicated constant as an initializer of an element of the array
variable pointed to by var.  aggr_con is the top-level aggregate constant
that is the initial value of the variable.  Increment the number of
elements of the array.  Return the pre-incremented size (which is right
as a subscript).
*/
{
  /* Add the constant to the aggregate initializer list. */
  if (aggr_con->variant.aggregate.first_constant == NULL) {
    aggr_con->variant.aggregate.first_constant = con;
  } else {
    aggr_con->variant.aggregate.last_constant->next = con;
  }  /* if */
  aggr_con->variant.aggregate.last_constant = con;
  /* Increment the number of elements in the array. */
  return incr_nelems_of_array_var(var);
}  /* add_elem_to_array_var */


static void finish_array_var(a_variable_ptr var,
                             a_constant_ptr aggr_con)
/*
var is an array variable (for example, one created by
make_init_unnamed_local_static_array_var).  The building of the variable
is now completed, so finish it off.  In particular, the array size is now
known, so call set_type_size on the type.  aggr_con is the ck_aggregate
constant that is the initial value of the variable, or NULL if the variable
is uninitialized.
*/
{
  /* Finish off the array type by setting its size. */
  set_type_size(var->type);
  /* Set the aggregate type the same as the variable type. */
  if (aggr_con != NULL) aggr_con->type = var->type;
}  /* finish_array_var */

#endif /* GENERATE_EH_TABLES || !IA64_ABI */

/*
Following is code needed to define typeinfo implementation variables.
These are needed for RTTI as well as for exception handling.
*/

STATIC_THREAD a_type_ptr
		typeinfo_types[(int)tik_last+1];
			/* The various typeinfo types, as generated by IL
			   lowering.  (The types declared in the runtime
			   library headers are in types_of_type_info.) */

#if IA64_ABI

STATIC_THREAD a_class_list_entry_ptr
		vmi_class_types;
			/* Various vmi_class_type_info types (derived classes
			   of the typeinfo type for multiple-inheritance
			   cases).  The variant with one base class is
			   stored in typeinfo_types; all the rest are stored
			   in this list. */

#else /* !IA64_ABI */

#if ABI_CHANGES_FOR_RTTI
STATIC_THREAD a_field_ptr
		typeinfo_tinfo_field;
			/* tinfo field within typeinfo_type. */
#endif /* ABI_CHANGES_FOR_RTTI */

#endif /* !IA64_ABI */

#if ABI_CHANGES_FOR_RTTI

a_type_info_kind is_type_info_type(a_type_ptr type)
/*
Returns the kind of type_info type that type is -- or tik_last if it is
not a user type_info type.
*/
{
  int i;

  /* Iterate through all the typeinfo types. */
  for (i = 0; i != (int)tik_last; ++i) {
    /* If we found the type, stop. */
    if (types_of_type_info[i] == type) break;
  }  /* for */
  return (a_type_info_kind)i;
}  /* is_type_info_type */


a_type_info_kind is_type_info_vtbl(a_variable_ptr vtbl)
/*
Return the kind of type_info type whose vtable is vtbl -- or tik_last if
this vtable does not belong to any type_info type.
*/
{
  int i;

  for (i = 0; i != (int)tik_last; ++i) {
    /* If we found the vtable, stop. */
    if (vtbls_for_type_info[i] == vtbl) break;
  }  /* for */
  return (a_type_info_kind)i;
}  /* is_type_info_vtbl */


static void set_name_for_typeinfo_type(a_type_ptr   type,
                                       a_const_char *name)
/*
Set the name for type (a typeinfo type) as indicated.  The type must
be named and is given external linkage so we can generate a virtual
function table for it.
*/
{
  type->source_corresp.name = 
                           alloc_lowered_name_string((sizeof_t)strlen(name)+1);
  (void)strcpy((char *)type->source_corresp.name, name);
  type->source_corresp.name_linkage =
                                   (a_name_linkage_kind)nlk_cplusplus_external;
}  /* set_name_for_typeinfo_type */


static a_type_ptr make_user_typeinfo_type(void)
/*
Make a type with the same structure as the std::type_info type from the
<typeinfo> header, and return it.  

Its definition is

  struct type_info {
    __vtbl_entry *_vptr;  // Virtual function table pointer
    // IA64_ABI only:
    const char   *__name; // The mangled name of the type.
  };

This must match the header file and the runtime definition.  Well, "match"
is too strong a word: the virtual function table pointer must be at the
proper offset.  The rest of the fields, and the overall size, need
not match up.

Note that even if we have the type_info type from the <typeinfo> header
(pointed to by type_of_type_info), we do not use it, because we don't
know that it has the structure that we require, and we don't know
whether it is complete at this point (it might be completed later in
the compilation).  We build a version of that type for IL lowering.

Note a notational convention (possibly overly subtle): a "type_info"
refers to a type declared in the header, and a "typeinfo" (no underscore)
refers to the corresponding type built by IL lowering.  The type
built here is considered the tik_user typeinfo type, and a call to
this routine essentially implements make_typeinfo_type for tik_user.
*/
{
  a_field_ptr last_field;
  int         i;

  /* If the implementation typeinfo types have not been made already,
     make them now so that they will be in the right place on the type
     list relative to this type.  */
  for (i = (int)tik_user + 1; i != (int)tik_last; ++i) {
    /* The call here will result in a recursive call to the present routine,
       but that one will find that the type is not NULL and will not get into
       a recursion loop. */
    if (typeinfo_types[i] == NULL) {
      (void)make_typeinfo_type((a_type_info_kind)i, (a_type_ptr)NULL);
    }  /* if */
  }  /* for */
  /* If the type has been made already, we're done. */
  if (typeinfo_types[(int)tik_user] == NULL) {
    /* Make the struct type. */
    typeinfo_types[(int)tik_user] =
                              make_lowered_class_type((a_type_kind)tk_struct);
    add_to_front_of_file_scope_types_list(typeinfo_types[(int)tik_user]);
    /* The name cannot be "type_info" because that's the name of the
       real user-visible type, so we call this one (arbitrarily)
       "__EDG_type_info". */
    set_name_for_typeinfo_type(typeinfo_types[(int)tik_user],
                               "__EDG_type_info");
    last_field = NULL;
    /* Make a field for the virtual function table pointer. */
    make_lowered_field("__vptr", pointer_to_vtbl_type(),
                       typeinfo_types[(int)tik_user], &last_field);
#if IA64_ABI
    /* Make a field for the mangled name. */
    make_lowered_field("__name", 
                       make_pointer_type(
                            make_qualified_type(
                                        integer_type((an_integer_kind)ik_char),
                                        TQ_CONST)),
                       typeinfo_types[(int)tik_user], &last_field);
#endif /* !IA64_ABI */
    finish_class_type(typeinfo_types[(int)tik_user]);
  }  /* if */
  return typeinfo_types[(int)tik_user];
}  /* make_user_typeinfo_type */

#if CHECKING

a_boolean is_generated_typeinfo_type(a_type_ptr  type)
/*
Return TRUE if the given type is a typeinfo type created by IL lowering.
*/
{
  a_boolean  result = FALSE;
  int        i;

  for (i = 0; i != (int)tik_last; ++i) {
    if (typeinfo_types[i] == type) {
      result = TRUE;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* is_generated_typeinfo_type */

#endif /* CHECKING */
#endif /* ABI_CHANGES_FOR_RTTI */

a_type_ptr make_runtime_typeinfo_type(void)
/*
The type returned is suitable to use as the type of a typeinfo pointer
parameter used by the runtime library.  In the IA-64 ABI, the type of the
structure varies, so "const void *" is used.  make_typeinfo_type should be
used for cases where a typeinfo variable is being created.
*/
{
  a_type_ptr result;

#if IA64_ABI
  result = void_type();
#else /* !IA64_ABI */
  result = make_typeinfo_type(tik_implementation, NULL);
#endif /* IA64_ABI */
  result = make_pointer_type(make_qualified_type(result, TQ_CONST));
  return result;
}  /* make_runtime_typeinfo_type */


a_type_ptr make_typeinfo_type(a_type_info_kind      kind, 
                              ARG_UNUSED a_type_ptr type)
/*
Make the typeinfo struct type of the kind indicated if it is not made
already, and return a pointer to it.  If type is non-NULL, it
indicates the type for which the typeinfo type is being made.
*/
#if IA64_ABI
/*
The definition varies depending on type.  For simple types, it is the same 
as user_type_info.  
*/
#else /* !IA64_ABI */
/*
Its definition is

  struct typeinfo {
    type_info  base;  // User type_info
    const char *name; // Name
    char       *id;   // Id object pointer
    base_class_spec
               **bc;  // Pointer to base class array
  };

Note that this is the typeinfo implementation type, and it contains the
type_info that the user sees returned from typeid.

The first two fields listed are present only if ABI_CHANGES_FOR_RTTI is TRUE.
If PASS_DTOR_POINTER_TO_THROW is FALSE, there is also, following the id field:

    __vptp    dtor;  // Destructor

Before 2.41, the name field was not const (it was made const when const
string literals were implemented).
*/
#endif /* !IA64_ABI */
{
  a_field_ptr   last_field;
#if ABI_CHANGES_FOR_RTTI
  a_type_ptr    base_class;
#endif /* ABI_CHANGES_FOR_RTTI */
  a_type_ptr    *type_ptr;
#if IA64_ABI
  a_targ_size_t num_bases = 0, array_num_bases = 0;
#endif /* IA64_ABI */
          
#if !IA64_ABI
  /* There is only one typeinfo implementation type. */
  check_assertion(kind == tik_implementation);
#else /* IA64_ABI */
  /* The class being created must appear on the file scope types list
     before the classes that inherit from it.  By creating the derived
     classes now, we make sure that happens.  (The classes are added
     to the front of the types list, so the last type added -- the
     base class -- ends up being first on the list.) */
  switch (kind) {
    case tik_fundamental:
    case tik_enum:
    case tik_array:
    case tik_function:
    case tik_pointer:
    case tik_ptr_to_member:
    case tik_si_class:
    case tik_vmi_class:
      /* No derived classes. */
      break;
    case tik_pbase:
      if (typeinfo_types[(int)tik_pointer] == NULL) {
        (void)make_typeinfo_type(tik_pointer, (a_type_ptr)NULL);
      }  /* if */
      if (typeinfo_types[(int)tik_ptr_to_member] == NULL) {
        (void)make_typeinfo_type(tik_ptr_to_member, (a_type_ptr)NULL);
      }  /* if */
      break;
    case tik_class:
      if (typeinfo_types[(int)tik_si_class] == NULL) {
        (void)make_typeinfo_type(tik_si_class, (a_type_ptr)NULL);
      }  /* if */
      if (typeinfo_types[(int)tik_vmi_class] == NULL) {
        (void)make_typeinfo_type(tik_vmi_class, (a_type_ptr)NULL);
      }  /* if */
      break;
    case tik_user:
      /* This is a special case.  There are lots of derived classes, 
         but they are all set up from make_user_typeinfo_type. */
      (void)make_user_typeinfo_type();
      break;
    default:
      unexpected_condition();
  }  /* switch */
  /* Find the location where the typeinfo type will be stored.  The
     tik_vmi_class case is complicated by the fact that we need to know how
     many base classes there are. */
  if (kind == tik_vmi_class) {
    a_base_class_ptr base;
    if (type == NULL) {
      /* Assume one base. */
      num_bases = 1;
    } else {
      for (base = base_classes_of(type); base != NULL; 
           base = base->next) {
        if (base->direct) ++num_bases;
      }  /* for */
    }  /* if */
    array_num_bases = num_bases;
    if (emulate_gnu_abi_bugs) {
      /* g++ 3.2 always puts one extra entry at the end of the structure. */
      array_num_bases++;
    }  /* if */
    if (num_bases == 1) {
      /* Use the entry in the typeinfo_types array. */
      type_ptr = &typeinfo_types[(int)tik_vmi_class];
    } else {
      a_class_list_entry_ptr clep;
      a_field_ptr            base_field;
      /* Search the vmi_class_types list. */
      for (clep = vmi_class_types; clep != NULL; clep = clep->next) {
        /* Look at this class type. */
        type_ptr = &clep->class_type;
        /* See how many bases there are.  The base class array is the
           fourth field, preceded by the base class, the flags, and the
           base count.  */
        base_field = (*type_ptr)->variant.class_struct_union.field_list->
                                                              next->next->next;
        check_assertion(is_array_type(base_field->type));
        if (num_array_elements(base_field->type) == array_num_bases) break;
      }  /* for */
      /* If we did not find the type, create a new entry on the list. */
      if (clep == NULL) {
        clep = alloc_list_entry_for_class();
        clep->next = vmi_class_types;
        vmi_class_types = clep;
        type_ptr = &clep->class_type;
      }  /* if */
    }  /* if */
  } else
#endif /* IA64_ABI */
  /* Do not add code here. */
  {
    /* Normal case -- not tik_vmi_class, in the IA-64 ABI case. */
    type_ptr = &typeinfo_types[(int)kind];
  }  /* if */
  if (*type_ptr == NULL) {
    /* Make the struct type for the typeinfo type we are creating. */
    *type_ptr = make_lowered_class_type((a_type_kind)tk_struct);
    last_field = NULL;
#if IA64_ABI
    /* Give the type a name.  It's OK to use the same one the runtime will use
       because the type we are creating will not be placed in the abi
       namespace.  We only need to create a vtable for the vmi_class_type with
       one base, and we can't have two types with the same name, so we don't
       set the name for other variants. */
    if (kind != tik_vmi_class || num_bases == 1) {
      set_name_for_typeinfo_type(*type_ptr, type_info_names[(int)kind]);
    }  /* if */
    /* The tik_vmi_class type case must be handled specially.  There are an
       unbounded number of possible tik_vmi_class types so we cannot create
       them all up front.  For any variant other than the case with exactly one
       base, the new class is directly added after the case with one base. */
    if (kind == tik_vmi_class && num_bases != 1) {
      /* Make sure the variant with one base exists. */
      (void)make_typeinfo_type(tik_vmi_class, (a_type_ptr)NULL);
      (*type_ptr)->next = typeinfo_types[(int)tik_vmi_class]->next;
      typeinfo_types[(int)tik_vmi_class]->next = *type_ptr;
      if ((*type_ptr)->next == NULL) {
        curr_translation_unit->file_scope_pointers_block.last_type = *type_ptr;
      }  /* if */
    } else
#endif /* IA64_ABI */
    /* Do not add code here. */
    {
      add_to_front_of_file_scope_types_list(*type_ptr);
    }  /* if */
#if ABI_CHANGES_FOR_RTTI
#if !IA64_ABI
    base_class = make_user_typeinfo_type();
#else /* IA64_ABI */
    /* Develop the base class type. */
    switch (kind) {
      case tik_fundamental:
      case tik_enum:
      case tik_array:
      case tik_function:
      case tik_class:
      case tik_pbase:
        base_class = make_user_typeinfo_type();
        break;
      case tik_pointer:
      case tik_ptr_to_member:
        base_class = make_typeinfo_type(tik_pbase, (a_type_ptr)NULL);
        break;
      case tik_si_class:
      case tik_vmi_class:
        base_class = make_typeinfo_type(tik_class, (a_type_ptr)NULL);
        break;
      default:
        unexpected_condition();
    }  /* if */
#endif /* IA64_ABI */
    /* field: base_class "base" */
    make_lowered_field("base", base_class, *type_ptr, &last_field);
#if !IA64_ABI
    if (kind == tik_implementation) {
      typeinfo_tinfo_field = last_field;
    }  /* if */
    /* field: const char *name */
    make_lowered_field("name",
                       make_pointer_type(
                            make_qualified_type(
                                        integer_type((an_integer_kind)ik_char),
                                        TQ_CONST)),
                       *type_ptr, &last_field);
#endif /* !IA64_ABI */
#endif /* ABI_CHANGES_FOR_RTTI */
#if !IA64_ABI
    /* field: char *id */
    make_lowered_field("id",
                     make_pointer_type(integer_type((an_integer_kind)ik_char)),
                       *type_ptr, &last_field);
#if !PASS_DTOR_POINTER_TO_THROW
    /* field: __vptp dtor */
    make_lowered_field("dtor", make_vptp_type(),
                       *type_ptr, &last_field);
#endif /* !PASS_DTOR_POINTER_TO_THROW */
    /* field: base_class_spec *bc */
    make_lowered_field("bc",
                       make_pointer_type(make_base_class_spec_type()),
                       *type_ptr, &last_field);
#else /* IA64_ABI */
    switch (kind) {
      case tik_fundamental:
      case tik_enum:
      case tik_array:
      case tik_function:
      case tik_class:
      case tik_pointer:
        /* There are no additional fields. */
        break;
      case tik_pbase:
        /* field: unsigned int flags */
        make_lowered_field("flags",
                           integer_type((an_integer_kind)ik_unsigned_int),
                           *type_ptr, &last_field);
        /* field: const type_info* pointee */
        make_lowered_field("pointee",
                           make_pointer_type(
                                  make_qualified_type(
                                           make_user_typeinfo_type(),
                                           TQ_CONST)),
                           *type_ptr, &last_field);
        break;
      case tik_ptr_to_member:
        /* field: const __class_type_info* context */
        make_lowered_field("context",
                           make_pointer_type(
                                  make_qualified_type(
                                        make_typeinfo_type(tik_class,
                                                           (a_type_ptr)NULL),
                                        TQ_CONST)),
                           *type_ptr, &last_field);
        break;
      case tik_si_class:
        /* field: const __class_type_info* base_type */
        make_lowered_field("base_type",
                           make_pointer_type(
                                  make_qualified_type(
                                        make_typeinfo_type(tik_class,
                                                           (a_type_ptr)NULL),
                                        TQ_CONST)),
                           *type_ptr, &last_field);
        break;
      case tik_vmi_class:
        { a_type_ptr       array_type;
          /* field: unsigned int flags */
          make_lowered_field("flags",
                             integer_type((an_integer_kind)ik_unsigned_int),
                             *type_ptr, &last_field);
          /* field: unsigned int vmi_base_count */
          make_lowered_field("base_count",
                             integer_type((an_integer_kind)ik_unsigned_int),
                             *type_ptr, &last_field);
          /* field: base_info[num_bases] */
          array_type = array_of(make_base_class_spec_type());
          array_type->variant.array.variant.number_of_elements=array_num_bases;
          set_type_size(array_type);
          make_lowered_field("base_info", array_type, *type_ptr, &last_field);
        }
        break;
      default:
        unexpected_condition();
    }  /* switch */
#endif /* IA64_ABI */
    finish_class_type(*type_ptr);
  }  /* if */
  return *type_ptr;
}  /* make_typeinfo_type */


static a_type_info_kind get_typeinfo_kind(ARG_UNUSED a_type_ptr type)
/*
Return kind of typeinfo entry that should be used to represent "type".
For example, if "type" is a pointer type, the kind is tik_pointer.
*/
{
  a_type_info_kind  tinfo_kind;

#if IA64_ABI
  if (is_enum_type(type)) {
    tinfo_kind = tik_enum;
  } else if (is_void_type(type) ||
#if GNU_VECTOR_TYPES_ALLOWED && IA64_ABI
             is_vector_type(type) || 
#endif /* GNU_VECTOR_TYPES_ALLOWED && IA64_ABI */
             is_integral_type(type) || 
             is_floating_type(type) ||
             is_or_was_nullptr_type(type)) {
    tinfo_kind = tik_fundamental;
  } else if (is_array_type(type)) {
    tinfo_kind = tik_array;
  } else if (is_function_type(type)) {
    tinfo_kind = tik_function;
  } else if (is_pointer_type(type)) {
    tinfo_kind = tik_pointer;
  } else if (is_ptr_to_member_type(type)) {
    tinfo_kind = tik_ptr_to_member;
  } else if (is_class_struct_union_type(type)) {
    /* The exact type to use depends on the complexity of the inheritance
       used in type. */
    a_base_class_ptr bases = base_classes_of(type);
    if (bases == NULL) {
      /* If there are no bases, then we can simply use class_type_info.  */
      tinfo_kind = tik_class;
    } else {
      unsigned int num_direct_bases = 0;
      /* If there is only one public, non-virtual, direct base at offset zero,
         then we can use si_class_type_info. */
      tinfo_kind = tik_si_class;
      while (bases != NULL) {
        /* Only direct base classes matter. */
        if (bases->direct) {
          if (++num_direct_bases > 1) {
            /* If there is more than one direct base, we must use
               vmi_class_type_info. */
            tinfo_kind = tik_vmi_class;
            break;
          } else if (bases->is_virtual || bases->offset != 0 || 
                     (bases->derivation->access != 
                      (an_access_specifier)as_public)) {
            /* If this base is virtual, or is not at offset zero, then we
               must use vmi_class_type_info even if this is the only base. */
            tinfo_kind = tik_vmi_class;
            break;
          }  /* if */
        }  /* if */
        /* Continue to the next base class. */
        bases = bases->next;
      }  /* while */
    }  /* if */
  } else {
    unexpected_condition_str("get_typeinfo_kind: bad type");
  }  /* if */
#else /* !IA64_ABI */
  /* Use the basic implementation type. */
  tinfo_kind = tik_implementation;
#endif /* !IA64_ABI */

  return tinfo_kind;
}  /* get_typeinfo_kind */


static a_type_ptr make_appropriate_typeinfo_type(a_type_ptr type)
/*
Return the typeinfo implementation type corresponding to "type", i.e.
the typeinfo type that can represent "type".
*/
{
  return make_typeinfo_type(get_typeinfo_kind(type), type);
}  /* make_appropriate_typeinfo_type */

#if IA64_ABI

static a_symbol_ptr get_namespace_sym_for_type_info(a_type_info_kind kind)
/*
Return the symbol for the namespace containing the type_info type with the
indicated kind, or NULL if the type_info is not in a namespace.
*/
{
  a_symbol_ptr sym = NULL;

  switch (kind) {
    case tik_user:
      if (type_info_in_namespace_std) {
        sym = symbol_for_namespace_std;
        check_assertion(sym != NULL);
      }  /* if */
      break;
    case tik_fundamental:
    case tik_enum:
    case tik_array:
    case tik_function:
    case tik_class:
    case tik_si_class:
    case tik_vmi_class:
    case tik_pbase:
    case tik_pointer:
    case tik_ptr_to_member:
      sym = symbol_for_namespace_abi;
      check_assertion(sym != NULL);
      break;
    default:
      unexpected_condition();
  }  /* switch */
  return sym;
}  /* get_namespace_sym_for_type_info */

#endif /* IA64_ABI */

static a_variable_ptr find_existing_variable_named(char *name)
/*
If there is an existing file-scope variable with the given name, return a
pointer to it.  Otherwise, return NULL.  Used to find id object variables
and typeinfo variables with a given name.
*/
{
  a_variable_ptr var;

  for (var = il_header.primary_scope->variables;
       var != NULL;
       var = var->next) {
    if (var->source_corresp.name != NULL &&
        var->source_corresp.name[0] == name[0] && /* For speed. */
        strcmp(var->source_corresp.name, name) == 0) {
      /* Found an existing variable. */
      break;
    }  /* if */
  }  /* for */
  return var;
}  /* find_existing_variable_named */


static void set_optional_vtable_flag_to_match_type(a_variable_ptr var,
                                                   a_type_ptr     type)
/*
If type is a class type with a virtual function table that is
marked as "optional", set the is_optional_vtable flag in the variable
var (a typeinfo variable or something similar that is put out when the
vtable is put out).
*/
{
  if (is_immediate_class_type(type)) {
    a_variable_ptr vtbl_var = primary_vtbl_var_for_class_if_any(type);
    if (vtbl_var != NULL && vtbl_var->is_optional_vtable) {
      var->is_optional_vtable = TRUE;
    }  /* if */
  }  /* if */
}  /* set_optional_vtable_flag_to_match_type */

#if !IA64_ABI

static a_variable_ptr make_id_object_var(a_type_ptr type)
/*
Make an id object variable for the given type, and return a pointer to it.
The id object variable is pointed to from the definition of the typeinfo for
the type.  When static typeinfo objects are put out in multiple files,
they will point to the same (external) id object, so one can tell that they
all denote the same type.
*/
{
  a_variable_ptr  id_object_var;
  char            *temp_name;

  /* Develop the mangled name. */
  temp_name = mangled_id_object_name(type);
  /* Find and reuse an existing id object for (a different copy of) the
     same type if there is one. */
  id_object_var = find_existing_variable_named(temp_name);
  if (id_object_var == NULL) {
    /* Make the variable. */
    id_object_var = make_lowered_variable(temp_name,
                                          /*already_il_name=*/FALSE,
                                        integer_type((an_integer_kind)ik_char),
                                          (a_storage_class)sc_unspecified);
    id_object_var->source_corresp.name_has_been_mangled = TRUE;
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
    id_object_var->ELF_visibility = ELF_visibility_of_type(type);
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
    /* The id object variable is optional if the vtable is optional. */
    set_optional_vtable_flag_to_match_type(id_object_var, type);
  }  /* if */
  return id_object_var;
}  /* make_id_object_var */

#endif /* !IA64_ABI */

/*
Pointer to the base_class_spec struct type (used to represent a base class
for exception throw and catch specifications).  NULL until created.
*/
STATIC_THREAD a_type_ptr
		base_class_spec_type;

/*
Bit set values for the flags byte of base_class_spec.  These must
match the runtime's definition.
*/
typedef a_host_large_unsigned a_base_class_flags_set;
#define BCS_VIRTUAL		0x01
			/* TRUE if the offset gives the position of a
			   pointer to the (virtual) base class rather than
			   the offset to the base class itself. */
#if !IA64_ABI
#define BCS_LAST		0x02
			/* TRUE if this is the last base class specification
			   in the array. */
#endif /* !IA64_ABI */
#if ABI_CHANGES_FOR_RTTI
#if !IA64_ABI
#define BCS_PUBLIC 		0x04
#else /* IA64_ABI */
#define BCS_PUBLIC 		0x02
#endif /* !IA64_ABI */
			/* TRUE if the base class is public.  For non-direct
			   base classes, TRUE if the cumulative access across
			   the all derivation steps gives public access. */
#if !IA64_ABI
#define BCS_AMBIGUOUS		0x08
			/* TRUE if the base class is ambiguous. */
#define BCS_DIRECT		0x10
			/* TRUE if the base class is a direct base class. */
#endif /* !IA64_ABI */
#endif /* ABI_CHANGES_FOR_RTTI */

STATIC_THREAD a_type_ptr
                ptr_to_const_typeinfo_type;
                        /* A "cached" version of a pointer to a const-qualified
                           (tik_implementation) typeinfo type. */

#if DO_FULL_PORTABLE_EH_LOWERING || !IA64_ABI

static a_type_ptr make_ptr_to_const_typeinfo_type(void)
/*
Return a pointer-to-const typeinfo (tik_implementation) type.
*/
{
  if (ptr_to_const_typeinfo_type == NULL) {
    ptr_to_const_typeinfo_type = make_pointer_type(
                                   make_qualified_type(
                                     make_typeinfo_type(tik_implementation,
                                                        (a_type_ptr)NULL),
                                     TQ_CONST));
  }  /* if */
  return ptr_to_const_typeinfo_type;
}  /* make_ptr_to_const_typeinfo_type */

#endif /* DO_FULL_PORTABLE_EH_LOWERING || !IA64_ABI */

static a_type_ptr make_base_class_spec_type(void)
/*
Make the base_class_spec struct type (used to represent a base class
for exception throw and catch specifications) if it is not made already,
and return a pointer to it.  Its definition is

  struct base_class_spec {
#if IA64_ABI 
    const class_type_info 
                  *tinfo;  // typeinfo for base class
    long          offset_flags; 
                           // Combined offset and flags.
#else // !IA64_ABI
    const typeinfo
                  *tinfo;  // typeinfo for base class
    short         offset;  // Offset of base class in derived class
    unsigned char flags;   // Flags
#endif // !IA64_ABI
  };

*/
{
  a_field_ptr   last_field;

  if (base_class_spec_type == NULL) {
    /* Make the struct type. */
    base_class_spec_type = make_lowered_class_type((a_type_kind)tk_struct);
    add_to_front_of_file_scope_types_list(base_class_spec_type);
    last_field = NULL;
    /* field: typeinfo *tinfo (Cfront-like ABI). */
#if !IA64_ABI
    make_lowered_field("tinfo", 
                       make_ptr_to_const_typeinfo_type(),
                       base_class_spec_type, &last_field);
    /* field: short offset */
    make_lowered_field("offset", 
                       integer_type(targ_delta_int_kind),
                       base_class_spec_type, &last_field);
    /* field: unsigned char flags */
    make_lowered_field("flags",
                       integer_type((an_integer_kind)ik_unsigned_char),
                       base_class_spec_type, &last_field);
#else /* IA64_ABI */
    /* IA-64 ABI. */
    /* field: const class_type_info *tinfo. */
    make_lowered_field("tinfo", 
                       make_pointer_type(
                                make_qualified_type(
                                         make_typeinfo_type(tik_class,
                                                            (a_type_ptr)NULL),
                                         TQ_CONST)),
                       base_class_spec_type, &last_field);
    /* field: long offset_flags */
    make_lowered_field("offset_flags", 
                       integer_type((an_integer_kind)ik_long),
                       base_class_spec_type, &last_field);
#endif /* IA64_ABI */
    finish_class_type(base_class_spec_type);
  }  /* if */
  return base_class_spec_type;
}  /* make_base_class_spec_type */

#if !IA64_ABI

static a_variable_ptr make_base_class_array_var(a_type_ptr type)
/*
type is a class type that has base classes.  Make a variable initialized
with an array of base_class_spec entries for the base classes of the type.
This is used as part of the typeinfo information.  The variable is always
allocated in the file scope memory region.
*/
{
  a_type_ptr       array_type;
  a_base_class_ptr bcp;
  a_constant_ptr   aggr_con, sub_aggr_con;
  a_variable_ptr   typeinfo_var, bc_var;
  a_boolean        ovflo;
  a_constant_ptr   typeinfo_con, offset_con, flags_con = NULL;
  a_base_class_flags_set
                   flags_value;
  a_targ_size_t    offset;

  /* The current region is already the file scope memory region when
     this routine is called. */
  /* Make an initialized static variable that is an array of base_class_spec
     structures. */
  /* Make the array type. */
  array_type = array_of(make_qualified_type(make_base_class_spec_type(),
                                            TQ_CONST));
  /* Make the variable.  It is unnamed and static and in the file scope. */
  /* make_init_unnamed_local_static_array_var cannot be used because we
     want the variable always to be in the file scope. */
  bc_var = make_file_scope_temporary(array_type);
  /* The initial value is an aggregate constant pointing to a list of
     aggregate constants. */
  aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
  /* Attach the aggregate constant as the initial value of the variable. */
  bc_var->init_kind = (an_init_kind)initk_static;
  bc_var->initializer.constant = aggr_con;
#if CHECKING
  flags_con = NULL;
#endif /* CHECKING */
  /* The initial value is an aggregate constant pointing to a list of
     aggregate constants for base_class_spec structures. */
  for (bcp = type->variant.class_struct_union.extra_info->base_classes;
       bcp != NULL;
       bcp = bcp->next) {
    /* Include information only on direct, virtual, and ambiguous base
       classes. */
    if (bcp->direct || bcp->is_virtual
#if ABI_CHANGES_FOR_RTTI
                                      || bcp->ambiguous
#endif /* ABI_CHANGES_FOR_RTTI */
                                                        ) {
      /* The base class specification consists of three fields:
           1)  A pointer to the typeinfo variable for the base class.
           2)  The offset of the base class in the derived class.
           3)  A flags byte.
      */
      flags_value = 0;
      /* Make an address constant for a pointer to the base class typeinfo
         variable. */
      typeinfo_con = alloc_constant((a_constant_repr_kind)ck_address);
      typeinfo_var = bcp->type->typeinfo_var;
      check_assertion_str(typeinfo_var != NULL,
                          "make_base_class_array_var: NULL typeinfo var");
      set_variable_address_constant(typeinfo_var, typeinfo_con,
                                    /*set_address_taken_flag=*/TRUE);
      /* Make the offset constant. */
      if (bcp->is_virtual) {
        /* Virtual base class.  The offset is to the pointer, and a flag in the
           flags byte indicates indirection. */
        offset = bcp->pointer_offset;
        flags_value |= BCS_VIRTUAL;
      } else {
        /* Non-virtual base class.  The offset is to the data. */
        offset = bcp->offset;
      }  /* if */
#if ABI_CHANGES_FOR_RTTI
      if (bcp->direct) {
        flags_value |= BCS_DIRECT;
      }  /* if */
      if (bcp->ambiguous) {
        flags_value |= BCS_AMBIGUOUS;
      }  /* if */
      { a_base_class_derivation_ptr preferred_derivation =
                                                  preferred_derivation_of(bcp);
        if (access_to_end_of_path((an_access_specifier)as_public,
                                  preferred_derivation->path,
                                  preferred_derivation) ==
                                             (an_access_specifier)as_public) {
          /* BCS_PUBLIC flag is TRUE if there is public access to the base
             class.  For non-direct base classes, the access indicated is the
             best available access across the derivation steps. */
          flags_value |= BCS_PUBLIC;
        }  /* if */
      }
#endif /* ABI_CHANGES_FOR_RTTI */
      offset_con = alloc_constant((a_constant_repr_kind)ck_integer);
      set_integer_constant_with_overflow_check(offset_con,
                                               (a_host_large_integer)offset,
                                               targ_delta_int_kind,
                                               bcp->type,
                                               /*preserve_needed_flag=*/FALSE);
      /* Make the flags constant. */
      flags_con = alloc_constant((a_constant_repr_kind)ck_integer);
      set_unsigned_integer_constant(flags_con,
                                    (a_host_large_unsigned)flags_value,
                                    (an_integer_kind)ik_unsigned_char);
      /* Link the constants together and make an aggregate constant. */
      typeinfo_con->next = offset_con;
      offset_con->next = flags_con;
      sub_aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
      sub_aggr_con->type = base_class_spec_type;
      sub_aggr_con->variant.aggregate.first_constant = typeinfo_con;
      sub_aggr_con->variant.aggregate.last_constant = flags_con;
      /* Add the constant to the aggregate initializer list. */
      (void)add_elem_to_array_var(sub_aggr_con, bc_var, aggr_con);
    }  /* if */
  }  /* for */
  /* Put the BCS_LAST bit on in the last entry. */
  check_assertion_str(flags_con != NULL,
                      "make_base_class_array_var: no base classes");
  flags_value = unsigned_value_of_integer_constant(flags_con, &ovflo);
  flags_value |= BCS_LAST;
  set_unsigned_integer_value(&flags_con->variant.integer_value, flags_value);
  /* Finish off the variable. */
  finish_array_var(bc_var, aggr_con);
  return bc_var;
}  /* make_base_class_array_var */

#endif /* !IA64_ABI */
#if ABI_CHANGES_FOR_RTTI

static char *make_typeinfo_name(a_type_ptr type)
/*
Make a null-terminated string for the name of the indicated type (in the
file scope IL memory region), and return a pointer to it.
*/
{
  char                              *name;
#if !IA64_ABI
  an_il_to_str_output_control_block octl;
#else /* IA64_ABI */
  char                              *mangled_name;
#endif /* !IA64_ABI */

#if !IA64_ABI
  /* Set up for use of form_type. */
  clear_il_to_str_output_control_block(&octl);
  octl.output_str = put_str_to_temp_text_buffer_octl;
  octl.suppress_typedefs = TRUE;
  pos_in_temp_text_buffer = 0;
  /* Generate the string for the type in temp_text_buffer. */
  form_type(type, &octl);
  /* Add 1 to pos_in_temp_text_buffer for the final null.  The null has already
     been stored. */
  name = alloc_text_of_string_literal((sizeof_t)(pos_in_temp_text_buffer + 1));
  (void)strcpy(name, temp_text_buffer);
#else /* IA64_ABI */
  /* The IA-64 ABI uses the mangled name of the type. */
  mangled_name = mangled_typeinfo_string(type);
  name = alloc_text_of_string_literal((sizeof_t)(strlen(mangled_name) + 1));
  (void)strcpy(name, mangled_name);
#endif /* IA64_ABI */
  return name;
}  /* make_typeinfo_name */

#if IA64_ABI

static a_boolean type_is_externally_visible(a_type_ptr type)
/*
Return TRUE if the indicated type is visible across more than one
translation unit.  This controls whether the typeinfo for the type
is placed in a COMDAT.
*/
{
  a_boolean is_extern = FALSE;

  /* Types that refer to local types need not be shared, unless they
     are from a routine that may be instantiated more than once. */
  if (!is_or_contains_local_type(type)) {
    is_extern = TRUE;
  } else {
    if (type->source_corresp.is_local_to_function) {
      a_routine_ptr enclosing_routine = enclosing_routine_for_local_type(type);
      if (routine_might_exist_in_multiple_copies(enclosing_routine)) {
        is_extern = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return is_extern;
}  /* type_is_externally_visible */

#endif /* IA64_ABI */

static a_constant_ptr make_typeinfo_name_constant(a_type_ptr type)
/*
Make a constant that is the address of a string for the name of the given
type, for use in typeinfo implementation constants.  For the IA-64 ABI,
a variable is initialized to the string, and the address of the
variable is returned.
*/
{
  char           *name = make_typeinfo_name(type);
  sizeof_t       name_length = strlen(name) + 1;
  a_constant_ptr constant = local_constant();
  a_constant_ptr string_con, addr_con;
#if IA64_ABI
  char           *var_name;
  a_variable_ptr typeinfo_name_var;
  a_type_ptr     var_type;
  a_boolean      use_comdat;
#endif /* IA64_ABI */

  /* Generate a string constant. */
  clear_constant(constant, (a_constant_repr_kind)ck_string);
  constant->type = string_type((a_targ_size_t)name_length);
  constant->variant.string.length = (a_targ_size_t)name_length;
  constant->variant.string.value  = name;
#if IA64_ABI
  /* An initial value string cannot be shared. */
  string_con = alloc_unshared_constant(constant);
  /* Make the variable const even if strings are not const. */
  var_type = make_qualified_type(string_con->type, TQ_CONST);
  /* Store the constant in a variable; the IA64 ABI requires that the name
     be stored in a variable with a prescribed name. */
  /* The variable is always a COMDAT even if the associated typeinfo
     is static, unless it contains a reference to a local type. */
  use_comdat = type_is_externally_visible(type);
  var_name = mangled_typeinfo_string_name(type);
  typeinfo_name_var = make_lowered_variable(var_name, 
                                            /*already_il_name=*/FALSE,
                                            var_type,
                                            use_comdat ?
                                              (a_storage_class)sc_unspecified :
                                              (a_storage_class)sc_static);
  typeinfo_name_var->source_corresp.name_has_been_mangled = TRUE;
  set_variable_address_constant(typeinfo_name_var, constant, 
                                /*set_address_taken_flag=*/TRUE);
  /* Initialize the variable. */
  typeinfo_name_var->init_kind = (an_init_kind)initk_static;
  typeinfo_name_var->initializer.constant = string_con;
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  typeinfo_name_var->ELF_visibility = ELF_visibility_of_type(type);
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
  if (use_comdat) {
    put_variable_into_comdat_group(typeinfo_name_var);
    /* The typeinfo string variable is optional if the vtable is optional. */
    set_optional_vtable_flag_to_match_type(typeinfo_name_var, type);
  }  /* if */
#else /* !IA64_ABI */
  /* Generate a constant for the address of the string. */
  string_con = alloc_shareable_constant(constant);
  set_constant_address_constant(string_con, constant);
#endif /* !IA64_ABI */
  /* Do the array->pointer decay.  Note that the field in the typeinfo
     structure is const, and we also add const here if string literals
     are not const. */
  implicit_cast(constant,
                make_pointer_type(make_qualified_type(
                                        integer_type((an_integer_kind)ik_char),
                                        TQ_CONST)));
  addr_con = move_local_constant_to_il(&constant);
  return addr_con;
}  /* make_typeinfo_name_constant */

#endif /* ABI_CHANGES_FOR_RTTI */

#if IA64_ABI

/*
Bit set values for the flags field in __pbase_type_info.  These must
match the runtime's definition.
*/
typedef a_host_large_unsigned a_pbase_flags_set;
#define PFS_CONST		0x01
			/* TRUE if the type pointed to is "const". */
#define PFS_VOLATILE		0x02
			/* TRUE if the type pointed to is "volatile".  */
#define PFS_RESTRICT		0x04
			/* TRUE if the type pointed to is qualified with
			   "restrict". */
#define PFS_INCOMPLETE		0x08
			/* TRUE if the type pointed to is incomplete. */
#define PFS_INCOMPLETE_CLASS	0x10
			/* TRUE (in a pointer-to-member type) if the class
			   containing the member is incomplete. */
#define PFS_TRANSACTION_SAFE	0x20
			/* Currently unused. */
#define PFS_NOEXCEPT		0x40
			/* TRUE (in a function-type) if the function-type is
			   a noexcept function. */

/*
Bit set values for the flags word in a __vmi_class_type_info
structure.  These must match the runtime's definition.
*/
typedef a_host_large_unsigned
		a_vmi_class_flags_set;

#define VMI_NON_DIAMOND_REPEAT	0x01
			/* TRUE if any non-virtual base class appears more
			   than once in the hierarchy. */

#define VMI_DIAMOND		0x02
			/* TRUE if the class is "diamond shaped."  In other
			   words, TRUE if any virtual base class can be
	`		   reached by more than one path through the
			   hierarchy. */


static a_boolean is_incomplete_type_for_purposes_of_rtti(a_type_ptr type)
/*
Return TRUE if the type is incomplete from the point of view of the RTTI
implementation, i.e., if the PFS_INCOMPLETE/PFS_INCOMPLETE_CLASS flags should
be set in the typeinfo object and the typeinfo object should be emitted with
internal linkage.
*/
{
  a_boolean is_incomplete = FALSE;

  while (is_pointer_type(type)) {
    type = type_pointed_to(type);
  }  /* while */
  if (is_ptr_to_member_type(type)) {
    type = pm_class_type(type);
  }  /* if */
  if (is_aggregate_or_union_type(type) && 
      is_incomplete_type(type)) {
    is_incomplete = TRUE;
  }  /* if */
  return is_incomplete;
}  /* is_incomplete_type_for_purposes_of_rtti */


static a_vmi_class_flags_set vmi_flags_for_type(a_type_ptr	type)
/*
Compute the flags that describe the inheritance hierarchy of "type".  The
flag set value is returned.
*/
{
  a_vmi_class_flags_set	flags = 0;
  a_base_class_ptr	bcp;

  /* Go through the base class list.  Stop if both of the flags have already
     been set. */
  for (bcp = type->variant.class_struct_union.extra_info->base_classes;
       bcp != NULL && flags != (VMI_DIAMOND | VMI_NON_DIAMOND_REPEAT);
       bcp = bcp->next) {
    if (bcp->is_virtual) {
      /* The VMI_DIAMOND flag should be set if any virtual base class has
         more than one derivation path. */
      check_assertion(bcp->derivation != NULL);
      if (bcp->derivation->next != NULL) flags |= VMI_DIAMOND;
    } else {
      /* The VMI_NON_DIAMOND_REPEAT flag should be set if any non-virtual
         base class appears more than once in the hierarchy. */
      if (bcp->ambiguous) flags |= VMI_NON_DIAMOND_REPEAT;
    }  /* if */
  }  /* for */
#if DEBUG
  if (db_flag_is_set("vmi_flags")) {
    db_type_name(type);
    if (flags & VMI_DIAMOND) fprintf(f_debug, " vmi_diamond");
    if (flags & VMI_NON_DIAMOND_REPEAT) {
      fprintf(f_debug, " vmi_non_diamond_repeat");
    }  /* if */
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  return flags;
}  /* vmi_flags_for_type */

#endif /* IA64_ABI */

static void define_typeinfo_var(a_type_ptr            type,
                                a_boolean             force_static,
                                ARG_UNUSED a_boolean  use_comdat)
/*
Generate a definition for the typeinfo variable (used to provide runtime
type information) associated with type "type".  If force_static is TRUE,
change the typeinfo variable to static.  If use_comdat is TRUE, emit the
typeinfo variable in a COMDAT group.
*/
{
  a_boolean      is_class_type = is_immediate_class_type(type);
  a_type_info_kind
                 typeinfo_kind = get_typeinfo_kind(type);
  a_type_ptr     tinfo_type = make_typeinfo_type(typeinfo_kind, type);
  a_variable_ptr typeinfo_var = type->typeinfo_var;
  a_variable_ptr vtbl_var;
  a_constant_ptr aggr_con;
#if !IA64_ABI
  a_constant_ptr id_con, bc_con;
  a_type_ptr     curr_field_type;
#endif /* !IA64_ABI */
  a_field_ptr    curr_field;
  a_source_position
                 saved_error_position;
#if ABI_CHANGES_FOR_RTTI
  a_constant_ptr type_info_con, vptr_con, name_con;
  a_type_info_kind
                 typeinfo_kind_for_vtbl;
#endif /* ABI_CHANGES_FOR_RTTI */
#if !PASS_DTOR_POINTER_TO_THROW
  a_constant_ptr dtor_con;
  a_routine_ptr  dtor_routine;
#endif /* !PASS_DTOR_POINTER_TO_THROW */

  /* typedefs and cv-qualified types are not allowed at this level. */
  check_assertion(type->kind != (a_type_kind)tk_typeref);
  saved_error_position = error_position;
  error_position = type->source_corresp.decl_position;
#if IA64_ABI
  if (is_class_type) {
    /* Update the type in the typeinfo variable.  It might be different
       if type is a class and was incomplete when make_typeinfo_var was
       called.  Note that the address of the typeinfo variable is always
       cast to some base class when stored somewhere, so changing its
       type now is not going to cause problems. */
    if (f_skip_typerefs(typeinfo_var->type) != tinfo_type) {
      typeinfo_var->type = make_qualified_type(tinfo_type, TQ_CONST);
    }  /* if */
  }  /* if */
#endif /* IA64_ABI */
  /* Set the linkage on the typeinfo variable. */
  if (force_static) {
    /* When forced to by the flag force_static, change the storage class to
       static and the linkage to internal. */
    typeinfo_var->storage_class = (a_storage_class)sc_static;
    if (has_name(typeinfo_var)) {
      typeinfo_var->source_corresp.name_linkage =
                                             (a_name_linkage_kind)nlk_internal;
    }  /* if */
    /* The typeinfo variable is optional if the vtable is optional. */
    set_optional_vtable_flag_to_match_type(typeinfo_var, type);
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* If the typeinfo object has internal linkage, we cannot make it dllimport
       or dllexport. */
    typeinfo_var->decl_modifiers &= ~DM_DLLFLAGS;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
    typeinfo_var->ELF_visibility = (an_ELF_visibility_kind)evk_unspecified;
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
#if SUN_EXTENSIONS_ALLOWED
    /* If the typeinfo object has  internal linkage, we cannot give it a Sun
       link scope. */
    typeinfo_var->decl_modifiers &= ~DM_ANY_SUN_LINK_SCOPE;
#endif /* SUN_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if ((typeinfo_var->decl_modifiers & DM_DLLIMPORT) != 0) {
    /* A typeinfo object for a dllimport-ed class should not be defined. */
    goto done;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else if (!is_class_type) {
    /* typeinfo variables for nonclass cases keep whatever storage class
       they already have. */
#if IA64_ABI
    if (use_comdat) {
      put_variable_into_comdat_group(typeinfo_var);
    }  /* if */
#endif /* IA64_ABI */
  } else {
    /* For an externally-linked class, change the variable to an external
       definition. */
    typeinfo_var->storage_class = (a_storage_class)sc_unspecified;
    /* The variable can be referenced from another compilation unit.
       This is probably already set. */
    typeinfo_var->source_corresp.referenced = TRUE;
    /* The typeinfo variable is optional if the vtable is optional. */
    set_optional_vtable_flag_to_match_type(typeinfo_var, type);
#if ONE_INSTANTIATION_PER_OBJECT
    if (one_instantiation_per_object) {
      /* Put the typeinfo variable into the same slice as the virtual
         function table, if there is one. */
      vtbl_var = type->variant.class_struct_union.extra_info->
                                                    virtual_function_table_var;
      if (vtbl_var != NULL) {
        typeinfo_var->instantiation_needed_bit_number =
                                     vtbl_var->instantiation_needed_bit_number;
      }  /* if */
    }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if IA64_ABI
    if (use_comdat) {
      put_variable_into_comdat_group(typeinfo_var);
    }  /* if */
#endif /* IA64_ABI */
  }  /* if */
#if ONE_INSTANTIATION_PER_OBJECT && \
    DUPLICATE_SPECIAL_STATICS_IN_INSTANTIATION_SLICES
  if (one_instantiation_per_object &&
      typeinfo_var->storage_class == (a_storage_class)sc_static) {
    /* type_info objects with internal linkage should not be externalized for
       the sake of having only one copy over all the object files.  Keep them
       with internal linkage instead. */
    typeinfo_var->source_corresp.duplicate_static_in_instantiation_slices =
                                                                         TRUE;
  }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT && DUPLICATE_SPECIAL_STATICS_IN_... */

  { a_memory_region_number region_to_switch_back_to;
#if !IA64_ABI
    /* Cfront-like ABI: */
    /* The initial value of the typeinfo variable is an aggregate containing
       values as follows:
         1)  type_info -- define virtual function table pointer (only if
             ABI_CHANGES_FOR_RTTI is TRUE).
         2)  Pointer to name string (only if ABI_CHANGES_FOR_RTTI is TRUE).
         3)  Id object: pointer to id object variable, or NULL if an internally
             linked class.
         4)  Destructor: pointer to destructor, or NULL.
         5)  Base class array pointer: pointer to array containing pointers
             to typeinfo structures for base classes, or NULL if there are
             no base classes.
    */
#endif /* !IA64_ABI */
    /* Switch to the file scope memory region so that initial values will
       be allocated there. */
    switch_to_file_scope_region(&region_to_switch_back_to);
    curr_field = tinfo_type->variant.class_struct_union.field_list;
#if ABI_CHANGES_FOR_RTTI
    /* Make the virtual function table pointer.  This is just the address of
       an extern variable.  The runtime provides the definition. */
    typeinfo_kind_for_vtbl = typeinfo_kind;
#if !IA64_ABI
    curr_field_type = curr_field->type;
    /* The implementation type does not have a corresponding user type.
       Use tik_user. */
    if (typeinfo_kind == tik_implementation) typeinfo_kind_for_vtbl = tik_user;
#endif /* !IA64_ABI */
    vtbl_var = vtbls_for_type_info[(int)typeinfo_kind_for_vtbl];
    if (vtbl_var == NULL) {
      /* We didn't generate a virtual function table for the corresponding
         type_info type, probably because we don't have that type.
         Generate a virtual function table for the typeinfo type. */
      a_const_char  *saved_name;
      a_symbol_ptr  ns_sym = NULL;
      a_type_ptr    tinfo_type_for_vtbl =
                                   typeinfo_types[(int)typeinfo_kind_for_vtbl];
      a_scope_ptr   saved_parent;  
      check_assertion(tinfo_type_for_vtbl != NULL);
      /* Give the type the name of the corresponding type_info briefly so
         the name can be used in generating the virtual function table name. */
      saved_name = tinfo_type_for_vtbl->source_corresp.name;
      tinfo_type_for_vtbl->source_corresp.name =
                                  type_info_names[(int)typeinfo_kind_for_vtbl];
      /* Add a parent pointer for the namespace temporarily to get the
         mangled name right. */
      saved_parent = parent_scope_of(tinfo_type_for_vtbl);
#if IA64_ABI
      ns_sym = get_namespace_sym_for_type_info(typeinfo_kind_for_vtbl);
#else /* !IA64_ABI */
      if (type_info_in_namespace_std) {
        check_assertion(symbol_for_namespace_std != NULL);
        ns_sym = symbol_for_namespace_std;
      }  /* if */
#endif /* !IA64_ABI */
      if (ns_sym != NULL) {
        tinfo_type_for_vtbl->source_corresp.parent_scope =
                      ns_sym->variant.namespace_info.ptr->variant.assoc_scope;
      }  /* if */
      vtbl_var = make_var_for_virtual_function_table(tinfo_type_for_vtbl,
                                                     (a_base_class_ptr)NULL,
                                                     (a_base_class_ptr)NULL);
      /* Usually define_one_virtual_function_table sets the name, but we force
         setting of the mangled name now because of this trick with altering
         the containing namespace. */
      set_virtual_function_table_name(vtbl_var,
                                      tinfo_type_for_vtbl,
                                      (a_base_class_ptr)NULL,
                                      (a_base_class_ptr)NULL);
      /* Save the virtual function table so it won't be generated again.
         (The subroutine will not have recognized this as a type_info
         type, because it's not -- it's the runtime typeinfo version.) */
      vtbls_for_type_info[(int)typeinfo_kind_for_vtbl] = vtbl_var;
      /* Restore the former name and parent information. */
      tinfo_type_for_vtbl->source_corresp.name = saved_name;
      if (ns_sym != NULL) {
        tinfo_type_for_vtbl->source_corresp.parent_scope = saved_parent;
      }  /* if */
    }  /* if */
    vptr_con = alloc_constant((a_constant_repr_kind)ck_address);
    set_variable_address_constant(vtbl_var, vptr_con,
                                  /*set_address_taken_flag=*/TRUE);
    vtbl_var->source_corresp.referenced = TRUE;
#if IA64_ABI
    /* The vptr is supposed to point at the first virtual function entry, so
       skip the offset and typeinfo pointer.  (We know that there are no
       virtual bases in the typeinfo class hierarchy, so there are no vbase or
       vcall offsets.) */
    vptr_con->variant.address.offset =
                                     (a_targ_ptrdiff_t)(2 * vtbl_entry_size());
#endif /* !IA64_ABI */
    /* Do the array --> pointer decay. */
    implicit_cast(vptr_con, pointer_to_vtbl_type());
    /* Make the constant for the type_info.  For cases involving a
       derived class, this ends up being the initializer for the
       base class part of the object. */
    type_info_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
    type_info_con->type = typeinfo_types[(int)tik_user]; /* sic */
    type_info_con->variant.aggregate.first_constant = vptr_con;
#if !IA64_ABI
    type_info_con->variant.aggregate.last_constant = vptr_con;
    /* Make the name constant. */
    curr_field = curr_field->next;
    curr_field_type = curr_field->type;
#endif /* !IA64_ABI */
    name_con = make_typeinfo_name_constant(type);
#if IA64_ABI
    vptr_con->next = name_con;
    type_info_con->variant.aggregate.last_constant = name_con;
#endif /* IA64_ABI */
    typeinfo_var->source_corresp.assoc_info = NULL;  /* Be neat. */
    curr_field = curr_field->next;
#endif /* ABI_CHANGES_FOR_RTTI */
#if !IA64_ABI
    /* Id object pointer. */
    curr_field_type = curr_field->type;
    id_con = alloc_constant((a_constant_repr_kind)ck_address);
    if (is_class_type &&
        type->source_corresp.name_linkage !=
                                           (a_name_linkage_kind)nlk_external &&
        type->source_corresp.name_linkage !=
                                 (a_name_linkage_kind)nlk_cplusplus_external) {
      /* Internally linked class; id object pointer is NULL. */
      make_zero_of_proper_type(curr_field_type, id_con);
    } else {
      /* Externally linked class, or nonclass; make id object variable. */
      set_variable_address_constant(make_id_object_var(type), id_con,
                                    /*set_address_taken_flag=*/TRUE);
    }  /* if */
#if !PASS_DTOR_POINTER_TO_THROW
    /* In versions preceding 2.38, there is a destructor pointer in the
       typeinfo entry.  It was used by the runtime to find the destructor
       to delete a thrown object.  That had some disadvantages, e.g.,
       the destructor sometimes had to be generated so its pointer could
       be put into the typeinfo variable, even though no object of that
       type was ever thrown.  The newer approach is to pass the destructor
       pointer on the throw setup call. */
    /* Destructor pointer. */
    curr_field = curr_field->next;
    curr_field_type = curr_field->type;
    dtor_con = alloc_constant((a_constant_repr_kind)ck_address);
    /* See if the class has a destructor. */
    dtor_routine = NULL;
    if (is_class_type) {
      a_symbol_ptr dtor_sym;
      check_assertion(type->source_corresp.assoc_info != NULL);
      dtor_sym = symbol_supplement_for_class(type)->destructor;
      if (dtor_sym != NULL) {
        dtor_routine = dtor_sym->variant.routine.ptr;
        if (dtor_routine->function_def_number == NULL_function_def_number &&
            (dtor_routine->storage_class == (a_storage_class)sc_static ||
             dtor_routine->compiler_generated ||
             dtor_routine->pure_virtual ||
             (dtor_routine->is_template_function && force_static))) {
          /* The destructor is static or compiler-generated and declared
             but not defined.  Use a null pointer.  Do that also for
             a destructor that's a template function, if the typeinfo
             is static; if the destructor has not been fully instantiated,
             that means there is no actual use of it (e.g., a throw) in
             this compilation unit. */
          dtor_routine = NULL;
        }  /* if */
      }  /* if */
    }  /* if */
    if (dtor_routine == NULL) {
      /* The class has no destructor, or the type is not a class; use a
         NULL pointer. */
      make_zero_of_proper_type(curr_field_type, dtor_con);
    } else {
      /* The class has a destructor.  Make a pointer to the routine. */
      dtor_routine->source_corresp.referenced = TRUE;
      set_routine_address_constant(dtor_routine, dtor_con,
                                   /*set_address_taken_flag=*/TRUE);
      implicit_cast(dtor_con, curr_field_type);
    }  /* if */
#endif /* !PASS_DTOR_POINTER_TO_THROW */
    /* Base class array pointer. */
    curr_field = curr_field->next;
    curr_field_type = curr_field->type;
    bc_con = alloc_constant((a_constant_repr_kind)ck_address);
    if (!is_class_type ||
        type->variant.class_struct_union.extra_info->base_classes == NULL) {
      /* The class has no base classes, or the type is not a class; use a
         NULL pointer. */
      make_zero_of_proper_type(curr_field_type, bc_con);
    } else {
      /* The class has base classes; make a variable whose initial value is
         an array of pointers to the typeinfo information for the base classes,
         and use its address here. */
      a_variable_ptr bc_var = make_base_class_array_var(type);
      set_variable_address_constant(bc_var, bc_con,
                                    /*set_address_taken_flag=*/TRUE);
      /* Make the type pointer-to-element instead of pointer-to-array. */
      implicit_cast(bc_con, curr_field_type);
    }  /* if */
#endif /* !IA64_ABI */
    /* Make the aggregate constant and attach it to the variable as its initial
       value. */
    aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
    aggr_con->type = typeinfo_var->type;
#if !IA64_ABI
#if ABI_CHANGES_FOR_RTTI
    aggr_con->variant.aggregate.first_constant = type_info_con;
    type_info_con->next = name_con;
    name_con->next = id_con;
#else /* !ABI_CHANGES_FOR_RTTI */
    aggr_con->variant.aggregate.first_constant = id_con;
#endif /* ABI_CHANGES_FOR_RTTI */
#if !PASS_DTOR_POINTER_TO_THROW
    id_con->next = dtor_con;
    dtor_con->next = bc_con;
#else /* PASS_DTOR_POINTER_TO_THROW */
    id_con->next = bc_con;
#endif /* !PASS_DTOR_POINTER_TO_THROW */
    aggr_con->variant.aggregate.last_constant = bc_con;
#else /* IA64_ABI */
    /* Add additional fields for derived type_info classes, if any. */
    switch (typeinfo_kind) {
      case tik_fundamental:
      case tik_enum:
      case tik_array:
      case tik_function:
      case tik_class:
        /* There are no additional fields. */
        aggr_con->variant.aggregate.first_constant = type_info_con;
        aggr_con->variant.aggregate.last_constant = type_info_con;
        break;
      case tik_pbase:
        /* This case should never happen; no typeinfo variables are actually
           of type __pbase_type_info; they should be either
           __pointer_type_info or __pointer_to_member_type_info. */
        unexpected_condition();
        break;
      case tik_pointer:
      case tik_ptr_to_member:
        /* Pointers and pointers to members. */
        /* Add the "flags" and "pointee" fields. */
        { a_constant_ptr    flags_con, pointee_con, pbase_con, context_con;
          a_pbase_flags_set flags_value;
          a_type_ptr        pointed_to_type;
          a_variable_ptr    context_typeinfo_var;
          /* Flags field. */
          flags_value = 0;
          if (type->kind == (a_type_kind)tk_pointer) {
            pointed_to_type = type_pointed_to(type);
          } else {
            pointed_to_type = pm_member_type(type);
          }  /* if */
          if (emulate_gnu_abi_bugs && gnu_abi_version < 30300 &&
              pointed_to_type->kind == (a_type_kind)tk_array) {
            /* g++ 3.2 fails to put the cv-qualifier bits on an array type. */
          } else {
            if (is_const_qualified_type(pointed_to_type)) {
              flags_value |= PFS_CONST;
            }  /* if */
            if (is_volatile_qualified_type(pointed_to_type)) {
              flags_value |= PFS_VOLATILE;
            }  /* if */
          }  /* if */
          if ((get_type_qualifiers(pointed_to_type) & TQ_RESTRICT) != 0) {
            flags_value |= PFS_RESTRICT;
          }  /* if */
          if (is_pointer_type(type) && 
              is_incomplete_type_for_purposes_of_rtti(type)) {
            flags_value |= PFS_INCOMPLETE;
          }  else if (is_ptr_to_member_type(type) &&
                      is_incomplete_type_for_purposes_of_rtti(type)) {
            flags_value |= PFS_INCOMPLETE_CLASS;
          }  /* if */
          if (is_function_type(pointed_to_type) &&
              is_nothrow_type(skip_typerefs(pointed_to_type))) {
            /* Exception specifications are part of the type system and the
               pointed-to function has a noexcept exception specification.
               The IA-64 ABI represents this in the pointer typeinfo (not
               the function typeinfo), so add a flag here and make sure the
               pointed-to function type does not include the exception
               specification. */
            pointed_to_type = function_type_without_noexcept_exception_spec(
                                               skip_typerefs(pointed_to_type));
            flags_value |= PFS_NOEXCEPT;
          }  /* if */
          flags_con = alloc_constant((a_constant_repr_kind)ck_integer);
          set_unsigned_integer_constant(flags_con,
                                        flags_value,
                                        (an_integer_kind)ik_unsigned_int);
          /* Pointee field. */
          pointee_con = alloc_constant((a_constant_repr_kind)ck_address);
          pointed_to_type = f_skip_typerefs(make_unqualified_type
                                                           (pointed_to_type));
          set_variable_address_constant(make_typeinfo_var(pointed_to_type),
                                        pointee_con,
                                        /*set_address_taken_flag=*/TRUE);
          implicit_cast(pointee_con, 
                        make_pointer_type(
                              make_qualified_type(make_user_typeinfo_type(),
                                                  TQ_CONST)));
          type_info_con->next = flags_con;
          flags_con->next = pointee_con;
          /* Make the pbase_type_info initializer. */
          pbase_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
          pbase_con->type = make_typeinfo_type(tik_pbase, (a_type_ptr)NULL);
          pbase_con->variant.aggregate.first_constant = type_info_con;
          pbase_con->variant.aggregate.last_constant = pointee_con;
          /* Make the aggregate constant. */
          aggr_con->variant.aggregate.first_constant = pbase_con;
          if (is_pointer_type(type)) {
            aggr_con->variant.aggregate.last_constant = pbase_con;
          } else if (type->kind == (a_type_kind)tk_ptr_to_member) {
            /* Add the context field for the pointer-to-member case. */
            context_con = alloc_constant((a_constant_repr_kind)ck_address);
            context_typeinfo_var = make_typeinfo_var(pm_class_type(type));
            set_variable_address_constant(context_typeinfo_var,
                                          context_con,
                                          /*set_address_taken_flag=*/TRUE);
            implicit_cast(context_con,
                          make_pointer_type(
                                 make_qualified_type(
                                          make_typeinfo_type(tik_class,
                                                             (a_type_ptr)NULL),
                                          TQ_CONST)));
            pbase_con->next = context_con;
            aggr_con->variant.aggregate.last_constant = context_con;
          }  /* if */
        }
        break;
      case tik_si_class:
      case tik_vmi_class:
        /* Single- and multiple-inheritance classes. */
        { a_constant_ptr         class_con, base_con, flags_con;
          a_constant_ptr         offset_con, count_con, base_info_con;
          a_constant_ptr         base_array_con;
          a_base_class_ptr       base;
          a_vmi_class_flags_set  vmi_flags_value;
          a_base_class_flags_set base_flags_value;
          a_host_large_unsigned  base_count;
          a_host_large_integer   offset;
          /* Make the class_type_info initializer. */
          class_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
          class_con->type = make_typeinfo_type(tik_class, (a_type_ptr)NULL);
          class_con->variant.aggregate.first_constant = type_info_con;
          class_con->variant.aggregate.last_constant = type_info_con;
          aggr_con->variant.aggregate.first_constant = class_con;
          if (typeinfo_kind == tik_si_class) {
            /* Find the single direct base type. */
            for (base = base_classes_of(type); base; base = base->next) {
              if (base->direct) break;
            }  /* for */
            /* There should be a direct base; otherwise we would not be using
               si_class_type_info. */
            check_assertion(base != NULL);
            /* Make the base_type initializer. */
            base_con = alloc_constant((a_constant_repr_kind)ck_address);
            set_variable_address_constant(make_typeinfo_var(base->type),
                                          base_con,
                                          /*set_address_taken_flag=*/TRUE);
            implicit_cast(base_con,
                          make_pointer_type(
                                  make_qualified_type(
                                          make_typeinfo_type(tik_class,
                                                             (a_type_ptr)NULL),
                                          TQ_CONST)));
            class_con->next = base_con;
            aggr_con->variant.aggregate.last_constant = base_con;
          } else {
            a_base_class_derivation_ptr  derivation;
            /* Compute the flags that describe the inheritance hierarchy.
               These flags are used by the runtime to optimize the search
               for a base class. */
            vmi_flags_value = vmi_flags_for_type(type);
            /* Build up the base class information array. */
            base_count = 0;
            base_array_con = alloc_constant(
                                         (a_constant_repr_kind)ck_aggregate);
            for (base = direct_base_classes_of(type);
                 base != NULL;
                 base = base->next_direct) {
              /* Keep track of how many entries are in the array. */
              ++base_count;
              /* Base field. */
              base_con = alloc_constant((a_constant_repr_kind)ck_address);
              set_variable_address_constant(make_typeinfo_var(base->type),
                                            base_con,
                                            /*set_address_taken_flag=*/TRUE);
              implicit_cast(base_con,
                            make_pointer_type(
                                    make_qualified_type(
                                          make_typeinfo_type(tik_class,
                                                             (a_type_ptr)NULL),
                                          TQ_CONST)));
              /* offset_flags field.  Value is offset<<8 plus flags. */
	      offset_con = alloc_constant((a_constant_repr_kind)ck_integer);
              if (base->is_virtual) {
                offset = (a_host_large_integer)(base->vbase_offset_index *
                                     (a_virtual_table_index)vtbl_entry_size());
              } else {
                offset = (a_host_large_integer)base->offset;
              }  /* if */
              /* Flags field. */
              base_flags_value = 0;
              if (base->is_virtual) {
                base_flags_value |= BCS_VIRTUAL;
              }  /* if */
              derivation = base->derivation;
              while (!derivation->direct) derivation = derivation->next;
              if (derivation->access == (an_access_specifier)as_public) { 
                base_flags_value |= BCS_PUBLIC;
              }  /* if */
              set_integer_constant(offset_con,
                                   ((offset << 8) |
                                    (a_host_large_integer)base_flags_value),
                                   ik_long);
              /* Initialize the array entry. */
              base_con->next = offset_con;
              base_info_con = alloc_constant(
                                         (a_constant_repr_kind)ck_aggregate);
              base_info_con->type = make_base_class_spec_type();
              base_info_con->variant.aggregate.first_constant = base_con;
              base_info_con->variant.aggregate.last_constant = offset_con;
              /* Add it to the array. */
              if (base_array_con->variant.aggregate.first_constant == NULL) {
                base_array_con->variant.aggregate.first_constant = 
                                                                base_info_con;
                base_array_con->variant.aggregate.last_constant = 
                                                                base_info_con;
              } else {
                base_array_con->variant.aggregate.last_constant->next = 
                                                              base_info_con;
                base_array_con->variant.aggregate.last_constant = 
                                                                base_info_con;
              }  /* if */
            }  /* for */
            /* Build the flags initializer. */
            flags_con = alloc_constant((a_constant_repr_kind)ck_integer);
            set_integer_constant(flags_con,
                                 (a_host_large_integer)vmi_flags_value,
                                 ik_unsigned_int);
            /* Build the count initializer. */
            count_con = alloc_constant((a_constant_repr_kind)ck_integer);
            set_integer_constant(count_con, (a_host_large_integer)base_count,
                                 ik_unsigned_int);
            /* Set the type of the array. */
            base_array_con->type = array_of(make_base_class_spec_type());
            base_array_con->type->variant.array.variant.number_of_elements =
                                                                    base_count;
            set_type_size(base_array_con->type);
            class_con->next = flags_con;
            flags_con->next = count_con;
            count_con->next = base_array_con;
            aggr_con->variant.aggregate.last_constant = base_array_con;
          }  /* if */
        }
        break;
      default:
        unexpected_condition();
    }  /* switch */
#endif /* IA64_ABI */
    typeinfo_var->init_kind = (an_init_kind)initk_static;
    typeinfo_var->initializer.constant = aggr_con;
    /* Return to the memory region that was current when this routine was
       entered. */
    switch_back_to_original_region(region_to_switch_back_to);
  }
#if MICROSOFT_EXTENSIONS_ALLOWED
done:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  error_position = saved_error_position;
}  /* define_typeinfo_var */


static void generate_class_typeinfo_var_definition(a_type_ptr type)
/*
Generate the definition of the typeinfo variable for the indicated class
type, if necessary.  A definition of the typeinfo variable is needed
somewhere in the program, but not necessarily in the current compilation
unit.
*/
{
  a_variable_ptr typeinfo_var = type->typeinfo_var;

  check_assertion(typeinfo_var != NULL);
  /* If the variable has been defined already, no further processing
     is necessary. */
  if (typeinfo_var->storage_class == (a_storage_class)sc_extern) {
    /* Determine whether the typeinfo variable should be defined in the
       current compilation unit. */
    a_boolean      definition_needed = FALSE, force_static = FALSE;
    a_boolean      use_comdat = FALSE;
    a_variable_ptr vtbl_var = type->variant.class_struct_union.extra_info->
                                                    virtual_function_table_var;
    if (vtbl_var != NULL) {
      /* Polymorphic class. */
      /* The typeinfo variable is usually defined if and only if the virtual
         function table is defined, and it is static if and only if the
         virtual function table is static.  However, when the vtable is
         optional, the typeinfo goes out if referenced, and the fact that we
         got here means we need the typeinfo in that case even if the vtable
         is not being defined. */
      force_static = (vtbl_var->storage_class == (a_storage_class)sc_static);
      definition_needed = (vtbl_var->init_kind != (an_init_kind)initk_none ||
                           vtbl_var->is_optional_vtable);
#if GNU_EXTENSIONS_ALLOWED && IA64_ABI
      if (gpp_mode && !definition_needed && !force_static &&
          type->variant.class_struct_union.is_template_class) {
        /* g++ seems to put out typeinfo variables for template classes
           even when the vtable isn't being put out and it's not optional.
           The typeinfo is a COMDAT, so an extra definition doesn't hurt. */
        definition_needed = TRUE;
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED && IA64_ABI */
      /* If the typeinfo is static, it has to be defined; no one else
         is going to do it.  Virtual function tables are sometimes not
         defined if they're not used, but typeinfo variables are created
         only if they're needed, so if one exists, it must be defined,
         even if the virtual function table is not defined. */
      if (force_static) definition_needed = TRUE;
#if IA64_ABI
      use_comdat = !force_static;
      if (!generate_rtti_typeinfo) {
        /* Not generating RTTI information (e.g., --no_rtti has been
           specified).  g++ with -fno-rtti decouples the generation of
           definitions for typeinfo variables from the generation of the
           definitions of the associated virtual function tables.  The
           typeinfo is put out as a COMDAT instead. */
        if (use_comdat) definition_needed = TRUE;
      }  /* if */
#endif /* IA64_ABI */
    } else {
      /* The class has no virtual function table (i.e., it's not
         polymorphic), so its typeinfo variable must be defined and must
         be static. */
      definition_needed = TRUE;
#if !IA64_ABI
      force_static = TRUE;
#else /* IA64_ABI */
      /* Use static linkage only for classes with internal or no linkage,
         or that are incomplete. */
      force_static = ((type->source_corresp.name_linkage ==
                       (a_name_linkage_kind)nlk_internal) ||
                      (type->source_corresp.name_linkage ==
                       (a_name_linkage_kind)nlk_none) ||
                      is_incomplete_type(type));
      use_comdat = !force_static;
#endif /* IA64_ABI */
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (!force_static && (typeinfo_var->decl_modifiers & DM_DLLIMPORT) != 0) {
      /* The typeinfo object for dllimport-ed classes should not be defined.
         However, if the object has internal linkage, we cannot make it
         dllimport or dllexport and hence we should proceed as if it were
         neither. */
      definition_needed = FALSE;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (definition_needed) {
      /* The definition is needed, so force definitions on the typeinfo
         variables for the base classes of this class, because they
         will be referenced from the definition. */
      a_base_class_ptr bcp;
      for (bcp = type->variant.class_struct_union.extra_info->base_classes;
           bcp != NULL;
           bcp = bcp->next) {
        (void)make_typeinfo_var(bcp->type);
        generate_class_typeinfo_var_definition(bcp->type);
      }  /* for */
      define_typeinfo_var(type, force_static, use_comdat);
    }  /* if */
  }  /* if */
}  /* generate_class_typeinfo_var_definition */


static char *alloc_mangled_typeinfo_name(a_type_ptr type)
/*
Allocate a string for the mangled name of the typeinfo variable for the
indicated type, and return a pointer to the string.  The string is
allocated in the file-scope IL memory region.
*/
{
  char     *temp_name, *mangled_name;
  sizeof_t alloc_length;

  /* Develop the mangled name. */
  temp_name = mangled_typeinfo_name(type);
  /* Allocate space for the mangled name, including the final null. */
  alloc_length = strlen(temp_name) + 1;
  mangled_name = alloc_lowered_name_string(alloc_length);
  /* Copy the name. */
  (void)strcpy(mangled_name, temp_name);
  return mangled_name;
}  /* alloc_mangled_typeinfo_name */

#if IA64_ABI

static a_boolean typeinfo_is_defined_in_runtime(a_type_ptr type) 
/*
Return TRUE if type's typeinfo is always defined in the runtime library.
*/
{
  a_boolean result = FALSE;

  if (is_void_type(type) ||
      is_integral_type(type) ||
#if C99_IL_EXTENSIONS_SUPPORTED
      /* Complex types are not defined in the runtime library. */
      is_real_floating_type(type) ||
#else /* !C99_IL_EXTENSIONS_SUPPORTED */
      is_floating_type(type) ||
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      is_or_was_nullptr_type(type)) {
    result = TRUE;
  } else if (is_pointer_type(type)) {
    a_type_qualifier_set quals;
    type = type_pointed_to(type);
    quals = get_type_qualifiers (type);
    if ((quals == TQ_NONE || quals == TQ_CONST) &&
        (is_void_type(type) ||
#if C99_IL_EXTENSIONS_SUPPORTED
         is_real_floating_type(type) ||
#else /* !C99_IL_EXTENSIONS_SUPPORTED */
         is_floating_type(type) ||
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
         is_integral_type(type))) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* typeinfo_is_defined_in_runtime */

#endif /* IA64_ABI */

a_variable_ptr make_typeinfo_var(a_type_ptr type)
/*
Make a typeinfo variable for the indicated type (if it does not exist
already) and return a pointer to it.  The variable points to runtime
type information.  It is always allocated in the file scope memory region.
This is not the type_info structure that is visible to the programmer
via the typeid operator (but it contains it).  Note that this routine
must be called early in the lowering of the file scope, because
it needs to generate a mangled name and a type-name string, and those 
cannot be generated after certain lowering has been done (e.g., of
pointers-to-members).
*/
{
  a_variable_ptr  typeinfo_var;
  a_type_ptr      typeinfo_var_type;
  char            *mangled_name;
  a_storage_class storage_class;
  a_boolean       define_now;
  a_boolean       force_static = FALSE;
  a_boolean       use_comdat = FALSE;

  /* typedefs and cv-qualified types are not allowed at this level. */
  check_assertion(type->kind != (a_type_kind)tk_typeref);
  /* No need to create the variable if it exists already. */
  typeinfo_var = type->typeinfo_var;
  if (typeinfo_var == NULL) {
    typeinfo_var_type = make_qualified_type(
                                          make_appropriate_typeinfo_type(type),
                                          TQ_CONST);
    /* The variable must be created. */
    if (is_immediate_class_type(type)) {
      /* Class type.  We delay the process of defining the typeinfo
	 variable until later.  For polymorphic classes, that's because
	 the typeinfo is defined if and only if the virtual function
	 table is defined, and we can't know that yet.  For
	 non-polymorphic classes, the type of definition is dependent
	 on the linkage of the class, which is not known until the end
	 of the compilation.  Also, the class might be incomplete now,
         if incomplete types are allowed in exception specifications
         as an extension, and therefore we can't even know if the class
         is polymorphic. */
      /* Assume an extern typeinfo variable, adjust later if necessary. */
      storage_class = (a_storage_class)sc_extern;
      define_now = FALSE;
      /* Determine the name for the typeinfo variable.  A name is required for
         typeinfo variables that end up being externally linked.  A name is
         not required for internally linked variables, but it turns out to be
         helpful for the C-generating back end (the C-generating back end
         cannot handle out-of-order initialization of unnamed static variables,
         and we cannot be sure that the typeinfo variables for base classes
         will be put out before those for derived classes). */
      mangled_name = alloc_mangled_typeinfo_name(type);
    } else {
      /* Non-class type. */
#if ABI_CHANGES_FOR_RTTI && !IA64_ABI
      /* typeinfo variables for non-classes are always static. */
      storage_class = (a_storage_class)sc_static;
      force_static = TRUE;
      define_now = TRUE;
      /* Static variables need not have names, and in fact we would have
         problems with duplicate names if typeinfo variables are generated
         for two copies of the same type within one compilation (e.g., two
         pointer-to-member-function types). */
      mangled_name = NULL;
#else /* !ABI_CHANGES_FOR_RTTI || IA64_ABI */
#if !IA64_ABI
      /* Old Cfront-like ABI implementation. */
      /* typeinfo variables for non-classes are always external tentative
         definitions (initialized to NULL/zero by default). */
      storage_class = (a_storage_class)sc_unspecified;
      define_now = FALSE;
#else /* IA64_ABI */
      /* IA-64 implementation. */
      if (!building_runtime && typeinfo_is_defined_in_runtime(type)) {
        /* These types are always defined in the runtime library, so they
           are not generated here. */
        define_now = FALSE;
        storage_class = (a_storage_class)sc_extern;
      } else {
        define_now = TRUE;
        storage_class = (a_storage_class)sc_unspecified;
        if (is_incomplete_type_for_purposes_of_rtti(type)) {
          /* typeinfo trees that refer ultimately to incomplete types are
             put out as static (we don't have complete type information here;
             someone else has to supply the real definition). */
          storage_class = (a_storage_class)sc_static;
          force_static = TRUE;
        } else {
          /* See whether the typeinfo should be in a COMDAT. */
          use_comdat = type_is_externally_visible(type);
        }  /* if */
      }  /* if */
#endif /* IA64_ABI */
      /* Determine the name for the typeinfo variable. */
      mangled_name = alloc_mangled_typeinfo_name(type);
      /* Look for an existing typeinfo variable with this name.  This can
         happen if typeinfo variables are generated for two copies of the
         same type within one compilation (e.g., two pointer-to-member
         function types), and it's important to generate only one variable
         in those cases. */
      typeinfo_var = find_existing_variable_named(mangled_name);
      if (typeinfo_var != NULL &&
          identical_types(typeinfo_var->type, typeinfo_var_type)) {
        /* Remember the variable in the type.  Note that it's okay to
           have two types pointing to a single typeinfo variable in this
           case, because the variable is never defined. */
        type->typeinfo_var = typeinfo_var;
        goto end_of_routine;
      }  /* if */
#endif /* ABI_CHANGES_FOR_RTTI && !IA64_ABI */
    }  /* if */
    typeinfo_var = make_lowered_variable(mangled_name,
                                         /*already_il_name=*/TRUE,
                                         typeinfo_var_type,
                                         storage_class);
    typeinfo_var->source_corresp.name_has_been_mangled = TRUE;
    /* Remember the variable in the type. */
    type->typeinfo_var = typeinfo_var;
#if MAINTAIN_NEEDED_FLAGS
    /* If the type is already marked to be kept in the IL, make sure
       the typeinfo variable is too. */
    if (il_entry_prefix_of(type).keep_in_il) {
      mark_to_keep_in_il((char *)typeinfo_var, iek_variable);
    }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
    if (define_now) {
      /* The typeinfo variable is supposed to be defined right now (for
         non-class cases). */
      define_typeinfo_var(type, force_static, use_comdat);
    } else if (is_immediate_class_type(type)) {
#if MICROSOFT_EXTENSIONS_ALLOWED || SUN_EXTENSIONS_ALLOWED
      a_class_type_supplement_ptr  ctsp = class_type_supp(type);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || SUN_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (microsoft_mode &&
          typeinfo_var->storage_class != (a_storage_class)sc_static) {
        /* Set any required dllimport/dllexport attributes.  If the storage
           class of the virtual table changes (to sc_static), we will need to
           update this again. */
        typeinfo_var->decl_modifiers |= (ctsp->decl_modifiers & DM_DLLFLAGS);
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
      if (typeinfo_var->storage_class != (a_storage_class)sc_static) {
        /* Adjust the ELF visibility of the externally visible typeinfo
           variable.  (This may be updated if the variable becomes static.) */
        typeinfo_var->ELF_visibility = ELF_visibility_of_type(type);
      }  /* if */
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
#if SUN_EXTENSIONS_ALLOWED
      if ((ctsp->decl_modifiers & DM_ANY_SUN_LINK_SCOPE) != 0 &&
          typeinfo_var->storage_class != (a_storage_class)sc_static) {
        /* Set any required __global/__symbolic/__hidden attributes.  If the
           storage class of the virtual table changes, we will need to update
           this again. */
        typeinfo_var->decl_modifiers |=
                               (ctsp->decl_modifiers & DM_ANY_SUN_LINK_SCOPE);
      }  /* if */
#endif /* SUN_EXTENSIONS_ALLOWED */
      if (in_typeinfo_var_generation_phase) {
        /* If we're already in the definition generation phase, generate the
           definition for a class typeinfo now. */
        generate_class_typeinfo_var_definition(type);
      }  /* if */
    }  /* if */
  }  /* if */
#if !ABI_CHANGES_FOR_RTTI || IA64_ABI
end_of_routine:
#endif /* !ABI_CHANGES_FOR_RTTI || IA64_ABI */
  return typeinfo_var;
}  /* make_typeinfo_var */


#if ABI_CHANGES_FOR_RTTI

#if IA64_ABI

/*
Pointer to the routine entry for the runtime routine __cxa_bad_typeid.
NULL until created.
*/
STATIC_THREAD a_routine_ptr
		bad_typeid_routine;

#else /* !IA64_ABI */

/*
Pointer to the routine entry for the runtime routine __get_typeid.
NULL until created.
*/
STATIC_THREAD a_routine_ptr
		get_typeid_routine;

#endif /* !IA64_ABI */

void lower_typeid(an_expr_node_ptr expr)
/*
Do lowering of an enk_typeid expression node, i.e., a C++ typeid operation.
The expression itself may be an rvalue or an lvalue, while the argument
expression is always an lvalue.  enk_typeid expressions always leave the
front end as lvalues, but may have undergone an lvalue-to-rvalue
conversion in cases where their value is not used.
*/
{
  a_type_ptr       typeid_type;
  an_expr_node_ptr typeid_expr, opnds;
  an_expr_node_ptr new_expr, null_constant_node, test_node;
  an_expr_node_ptr vptr_expr;
  a_variable_ptr   typeinfo_var;
  a_constant_ptr   null_constant = local_constant();
#if IA64_ABI
  an_expr_node_ptr minus_one_expr, bad_typeid_expr;
  a_boolean        non_null, save_assume_references_cannot_be_null = FALSE;
#else /* !IA64_ABI */
  an_expr_node_ptr question_node;
#endif /* !IA64_ABI */

  check_assertion(node_is(expr, enk_typeid));
  opnds = expr->variant.typeid_info.type_with_opt_expr;
  check_assertion(node_is(opnds, enk_type_operand));
  typeid_type = opnds->variant.type_operand.type;
  typeid_expr = opnds->next;
  if (!expr->variant.typeid_info.is_dynamic) {
    /* The operand expression is not needed; the type is known statically. */
    /* Make the runtime typeinfo variable. */
#if IA64_ABI
    a_type_ptr typeinfo_type;
#endif /* !IA64_ABI */
    typeinfo_var = get_typeinfo_var(typeid_type);
    /* Make an expression that refers to the user type_info member within
       the implementation typeinfo variable. */
    new_expr = var_lvalue_expr(typeinfo_var);
#if !IA64_ABI
    new_expr = field_lvalue_selection_expr(new_expr, typeinfo_tinfo_field);
#else /* !IA64_ABI */
    /* Move down through the base classes until we find the user type_info
       member. */
    for (typeinfo_type = f_skip_typerefs(new_expr->type);
         !identical_types(typeinfo_type, typeinfo_types[(int)tik_user]);
         typeinfo_type = f_skip_typerefs(new_expr->type)) {
      a_field_ptr field;
      check_assertion(is_immediate_class_type(typeinfo_type));
      field = typeinfo_type->variant.class_struct_union.field_list;
      new_expr = field_lvalue_selection_expr(new_expr, field);
    }  /* for */
#endif /* !IA64_ABI */
  } else {
    /* Polymorphic class case with expression. */
    check_assertion(is_glvalue_node(typeid_expr));
    check_assertion(is_immediate_class_type(typeid_type) &&
                    is_polymorphic_class_type(typeid_type));
#if IA64_ABI
    /* Before lowering the expression and turning it into an rvalue pointer,
       see if we can tell that the address of the lvalue cannot be NULL
       (in which case we don't need to generate code to handle that case).
       To match GNU's behavior in this case, make sure
       assume_references_cannot_be_null is set to TRUE. */
    if (gnu_mode) {
      save_assume_references_cannot_be_null = assume_references_cannot_be_null;
      assume_references_cannot_be_null = TRUE;
    }  /* if */
    non_null = cannot_be_null(typeid_expr);
    if (gnu_mode) {
      assume_references_cannot_be_null = save_assume_references_cannot_be_null;
    }  /* if */
#endif /* IA64_ABI */
    lower_expr(typeid_expr);
    /* The expression is an lvalue.  The standard specifically notes that
       if the expression is obtained by applying the unary * operator to a
       pointer, and the pointer is a null pointer value, a std::bad_typeid
       exception is thrown.  Taking the address of the lvalue will strip an
       eok_indirect operator if one exists and yield an rvalue pointer
       which can be tested against NULL. */
    /* As an extension to the *p rule above, discard any subscript if the
       top level operation is an eok_subscript (i.e., p[x] -- really *(p + x)
       -- becomes simply p). */
    if (is_operation_node(typeid_expr) &&
        node_operator_is(typeid_expr, eok_subscript)) {
      typeid_expr = subscript_or_padd_pointer_operand(typeid_expr);
      typeid_expr->next = NULL;
    } else {
      typeid_expr = add_address_of_to_node(typeid_expr);
    }  /* if */
    check_assertion(!typeid_expr->is_lvalue);
    /* Build the runtime call __get_typeid(typeid_expr ? vptr : NULL)
       where vptr is the virtual function table pointer value from the
       class object.  If NULL is passed to the runtime routine, it throws
       bad_typeid. */
    /* For the IA-64 ABI, generate
         typeid_expr ? (typeinfo*)(vptr[-1]) :
                       (__cxa_bad_typeid(), (typeinfo*)0)
       but only in cases where typeid_expr can have a NULL value.
    */
    /* Make code to get the virtual function table pointer. */
#if IA64_ABI
    if (non_null) {
      /* No need for a reusable copy in this case. */
      vptr_expr = typeid_expr;
    } else
#endif /* IA64_ABI */
    /* Do not insert code here. */
    {
      vptr_expr = make_reusable_copy(typeid_expr, /*vars_can_change=*/FALSE);
    }  /* if */
    vptr_expr = make_any_vptr_rvalue(vptr_expr, (an_expr_node_ptr *)NULL);
#if IA64_ABI
    /* Make (std::type_info*)vptr[-1]. */
    minus_one_expr = node_for_integer_constant(-1L, (an_integer_kind)ik_int);
    vptr_expr->next = minus_one_expr;
    vptr_expr = make_operator_node((an_expr_operator_kind)eok_subscript,
                                   make_vtbl_entry_type(),
                                   vptr_expr);
    vptr_expr = add_cast_if_necessary(vptr_expr, 
                               make_pointer_type(make_user_typeinfo_type()));
    if (non_null) {
      /* No need to check for a NULL value in this case. */
      new_expr = vptr_expr;
    } else {
      /* Make "__cxa_bad_typeid(), (std::typeinfo*)0". */
      (void)make_prototyped_runtime_routine("__cxa_bad_typeid",
                                            &bad_typeid_routine,
                                            void_type(),
                                            (a_type_ptr)NULL,
                                            (a_type_ptr)NULL,
                                            (a_type_ptr)NULL,
                                            (a_type_ptr)NULL,
                                            (a_type_ptr)NULL,
                                            (a_type_ptr)NULL,
                                            (a_type_ptr)NULL);
      bad_typeid_expr = make_call_node(bad_typeid_routine,
                                       (an_expr_node_ptr)NULL);
      make_zero_of_proper_type(make_pointer_type(make_user_typeinfo_type()),
                               null_constant);
      null_constant_node = alloc_node_for_constant(null_constant);
      bad_typeid_expr = make_comma_node(bad_typeid_expr, null_constant_node);
      /* Assemble the "?" operation. */
      test_node = boolean_controlling_expr(typeid_expr);
      test_node->next = vptr_expr;
      vptr_expr->next = bad_typeid_expr;
      new_expr = make_operator_node((an_expr_operator_kind)eok_question,
                                    vptr_expr->type, test_node);
    }  /* if */
#else /* !IA64_ABI */
    /* Make a NULL pointer constant of the vptr type. */
    make_zero_of_proper_type(vptr_expr->type, null_constant);
    null_constant_node = alloc_node_for_constant(null_constant);
    /* Assemble the "?" operation. */
    test_node = boolean_controlling_expr(typeid_expr);
    test_node->next = vptr_expr;
    vptr_expr->next = null_constant_node;
    question_node = make_operator_node((an_expr_operator_kind)eok_question,
                                       vptr_expr->type,
                                       test_node);
    /* Make the __get_typeid call. */
    new_expr = make_prototyped_runtime_call("__get_typeid",
                                 &get_typeid_routine,
                                 make_pointer_type(make_user_typeinfo_type()),
                                 pointer_to_vtbl_type(), NULL, question_node);
#endif /* !IA64_ABI */
    new_expr = add_indirection_to_node(new_expr);
  }  /* if */
  /* Cast to maintain original type. */
  new_expr = add_cast_to_glvalue_if_necessary(new_expr, expr->type);
  if (!expr->is_lvalue) {
    /* new_expr is an lvalue, convert it to an rvalue to match the
       lvalueness of the original expression. */
    new_expr = rvalue_expr_for_lvalue(new_expr);
  }  /* if */
  /* Overwrite the enk_typeid node. */
  overwrite_node(expr, new_expr);
  release_local_constant(&null_constant);
}  /* lower_typeid */

#endif /* ABI_CHANGES_FOR_RTTI */

/*
Following is code related to typeinfo entries that is needed only for
exception handling.
*/
/*
Bit set values for the flags byte of exception_type_spec.  These must
match the runtime's definition.
*/
#if GENERATE_EH_TABLES
#define ETS_IS_POINTER		0x01
			/* A pointer to an object of the type specified
			   by typeinfo. */
#define ETS_CONST		0x02
#define ETS_VOLATILE		0x04
			/* Indication of the type qualifiers on the type
			   pointed to, in the pointer case. */
#define ETS_IS_REFERENCE	0x08
			/* A reference to an object of the type specified
			   by typeinfo. */
/*
Note that ETS_IS_POINTER_TO_NOEXCEPT_FUNCTION and ETS_IS_ELLIPSIS are
"overloaded" (i.e., they use the same bit).  That's because the library
currently uses a_byte to store these flags and there are no unused bits.
If either ETS_IS_POINTER or ETS_IS_POINTER_TO_MEMBER_FUNCTION is set,
the bit is treated as ETS_IS_POINTER_TO_NOEXCEPT_FUNCTION, otherwise it is
treated as ETS_IS_ELLIPSIS.
*/
#define ETS_IS_POINTER_TO_NOEXCEPT_FUNCTION 0x10
			/* When (ETS_IS_POINTER_TO_NOEXCEPT_FUNCTION |
			   ETS_IS_POINTER) is TRUE, a pointer to a function
			   or member function type with a "noexcept" exception
			   specification (in configurations where exception
			   specifications are considered part of the function
			   type). */
#if DO_FULL_PORTABLE_EH_LOWERING
#define ETS_IS_ELLIPSIS		0x10
			/* When (ETS_IS_POINTER_TO_NOEXCEPT_FUNCTION |
			   ETS_IS_POINTER) is FALSE, the catch clause
			   contains an ellipsis. */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
#define ETS_LAST		0x20
			/* TRUE if this is the last type specification in
			   the array. */
#define ETS_IS_POINTER_TO_DATA_MEMBER 0x40
			/* A pointer to data member type is specified by
			   typeinfo.  Note that this differs from the pointer
			   case: the typeinfo refers to the pointer to data
			   member type. */
#define ETS_IS_POINTER_TO_MEMBER_FUNCTION 0x80
			/* A pointer to member function type is specified by
			   typeinfo.  Note that this differs from the pointer
			   case: the typeinfo refers to the pointer to
			   member function type. */
#endif /* GENERATE_EH_TABLES */
/*
Type used for internally storing the ETS_* flags (but use
targ_ets_flag_type_int_kind when passing them to the run time library).
*/
typedef unsigned long an_eh_type_flags_set;

#if GENERATE_EH_TABLES
#if ABI_COMPATIBILITY_VERSION >= 241

static a_variable_ptr ptr_flags_var_for_type(a_type_ptr *type)
/*
Build an initialized array whose values represent the cv-qualifiers of
the multi-level pointer type type, for use in qualifying a typeinfo.
Return a pointer to the variable and set *type to the underlying type.
*/
{
  a_constant_ptr	aggr_con, flag_con;
  a_variable_ptr	var;
  a_boolean		done;
  a_type_qualifier_set	qualifiers;
  a_host_large_unsigned	flags_value;

  /* Create the array variable. */
  var = make_init_unnamed_local_static_array_var(
                                     integer_type(targ_ets_flag_type_int_kind),
                                     /*in_function_scope=*/FALSE,
                                     &aggr_con);
  check_assertion(is_pointer_type(*type));
  do {
    *type = type_pointed_to(*type);
    done = !is_pointer_type(*type);
    qualifiers = get_type_qualifiers(*type);
    /* Make a flags set that describes the cv-qualifiers at this level. */
    flags_value = 0;
    if (qualifiers & TQ_CONST) {
      flags_value |= ETS_CONST;
    }  /* if */
    if (qualifiers & TQ_VOLATILE) {
      flags_value |= ETS_VOLATILE;
    }  /* if */
    /* See if the underlying type is a pointer to member type. */
    if (is_or_was_ptr_to_data_member_type(*type)) {
      flags_value |= ETS_IS_POINTER_TO_DATA_MEMBER;
    } else if (is_or_was_ptr_to_member_function_type(*type)) {
      flags_value |= ETS_IS_POINTER_TO_MEMBER_FUNCTION;
      if (is_ptr_to_member_type(*type) &&
          is_nothrow_type(skip_typerefs(*type))) {
        /* A noexcept exception specification is part of the pointer-to-member-
           function's type. */
        *type = function_type_without_noexcept_exception_spec(
                                                         skip_typerefs(*type));
        flags_value |= ETS_IS_POINTER_TO_NOEXCEPT_FUNCTION;
      }  /* if */
    } else if (is_function_type(*type) &&
               is_nothrow_type(skip_typerefs(*type))) {
      /* A noexcept exception specification is part of the function's
         type. */
      *type = function_type_without_noexcept_exception_spec(
                                                         skip_typerefs(*type));
      flags_value |= ETS_IS_POINTER_TO_NOEXCEPT_FUNCTION | ETS_IS_POINTER;
    }  /* if */
    if (done) {
      flags_value |= ETS_LAST;
    }  /* if */
    /* Make a constant for the value of the flags set. */
    flag_con = alloc_constant((a_constant_repr_kind)ck_integer);
    set_unsigned_integer_constant(flag_con, flags_value,
                                  targ_ets_flag_type_int_kind);
    /* Add the constant to the aggregate initializer list. */
    (void)add_elem_to_array_var(flag_con, var, aggr_con);
  } while (!done);
  /* Finish off the variable. */
  finish_array_var(var, aggr_con);
  return var;
}  /* ptr_flags_var_for_type */

#endif /* ABI_COMPATIBILITY_VERSION >= 241 */
#endif /* GENERATE_EH_TABLES */

static a_type_ptr eff_type_for_typeinfo(a_type_ptr           type,
                                        an_eh_type_flags_set *flags_value,
                                        a_variable_ptr       *ptr_flags_var)
/*
Type information is required for the indicated type, because it is
used in an exception handling or RTTI construct.  Determine the underlying
effective type for the typeinfo, and return it.  The relationship of the
original type to the effective typeinfo type is indicated in the returned
value of *flags_value and *ptr_flags_var.  For example, for a pointer to
a class, the typeinfo for the underlying class is returned, and
*flags_value is set to indicate a pointer.  *ptr_flags_var is used for
multi-level pointers.  It is returned pointing to an array of qualifier
flag sets that describes the cv-qualifiers at each level of the
multi-level pointer.  For cases other than multi-level pointers, it
is returned NULL.  If ptr_flags_var is NULL, the array is not built
because the caller does not need it.

In modes with GENERATE_EH_TABLES set to FALSE, this routine just strips
the cv-qualifiers and passes the type through.
*/
{
  a_type_ptr            eff_type;

  eff_type = type;
  *flags_value = 0;
  if (ptr_flags_var != NULL) *ptr_flags_var = NULL;
#if GENERATE_EH_TABLES
  /* For a pointer or reference to a type, use the typeinfo for the
     underlying type and a flag to indicate the reference or pointer.
     Both flags are on for a reference to a pointer. */
  if (is_reference_type(eff_type)) {
    eff_type = type_pointed_to(eff_type);
    *flags_value |= ETS_IS_REFERENCE;
  }  /* if */
  /* Note a top-level pointer to member type. */
  if (is_or_was_ptr_to_data_member_type(eff_type)) {
    *flags_value |= ETS_IS_POINTER_TO_DATA_MEMBER;
  } else if (is_or_was_ptr_to_member_function_type(eff_type)) {
    *flags_value |= ETS_IS_POINTER_TO_MEMBER_FUNCTION;
    if (is_ptr_to_member_type(eff_type) &&
        is_nothrow_type(skip_typerefs(eff_type))) {
      /* A noexcept exception specification is part of the pointer-to-member-
         function type.  Include a flag to that effect, then strip the
         exception specification from the effective type (as if the exception
         specification was a type qualifier). */
      eff_type =
        function_type_without_noexcept_exception_spec(skip_typerefs(eff_type));
      *flags_value |= ETS_IS_POINTER_TO_NOEXCEPT_FUNCTION;
    }  /* if */
  }  /* if */
  if (is_pointer_type(eff_type) && !is_or_was_nullptr_type(eff_type)) {
    a_type_ptr under_ptr = type_pointed_to(eff_type);
#if ABI_COMPATIBILITY_VERSION >= 241
    if (is_pointer_type(under_ptr) && !is_or_was_nullptr_type(under_ptr)) {
      /* Multi-level pointer.  Build the flags array. */
      /* Don't build the array if we don't need it. */
      if (ptr_flags_var != NULL) {
        *ptr_flags_var = ptr_flags_var_for_type(&eff_type);
      } else {
        /* Find the underlying type. */
        eff_type = under_ptr;
        while (is_pointer_type(eff_type) && !is_or_was_nullptr_type(eff_type)){
          eff_type = type_pointed_to(eff_type);
        }  /* while */
      }  /* if */
    } else
#endif /* ABI_COMPATIBILITY_VERSION >= 241 */
    /* Do not insert code here. */
    {
      a_type_qualifier_set qualifiers;
      eff_type = under_ptr;
      *flags_value |= ETS_IS_POINTER;
      /* Remember the type qualifiers on the type pointed to. */
      qualifiers = get_type_qualifiers(eff_type);
      if (qualifiers & TQ_CONST) {
        *flags_value |= ETS_CONST;
      }  /* if */
      if (qualifiers & TQ_VOLATILE) {
        *flags_value |= ETS_VOLATILE;
      }  /* if */
      if (is_or_was_ptr_to_data_member_type(eff_type)) {
        *flags_value |= ETS_IS_POINTER_TO_DATA_MEMBER;
      } else if (is_or_was_ptr_to_member_function_type(eff_type)) {
        *flags_value |= ETS_IS_POINTER_TO_MEMBER_FUNCTION;
      }  /* if */
      if (is_function_type(eff_type) &&
          is_nothrow_type(skip_typerefs(eff_type))) {
        /* A noexcept exception specification is part of the function or
           pointer-to-member-function type.  Include a flag to that effect,
           then strip the exception specification from the effective type (as
           if the exception specification was a type qualifier). */
        eff_type = function_type_without_noexcept_exception_spec(eff_type);
        *flags_value |= ETS_IS_POINTER_TO_NOEXCEPT_FUNCTION;
      }  /* if */
    }  /* if */
  }  /* if */
#else /* !GENERATE_EH_TABLES */
#if IA64_ABI
  /* The IA-64 ABI has no typeinfo encoding for a reference type, so
     don't try to make one.  Mark Mitchell reports that this is probably a
     bug in g++, but until the ABI spec is changed to provide a
     representation for a reference avoid aborts by skipping down to
     the underlying type.  Customers who roll their own IA-64 ABI
     exception handling may or may not want this code. */
  if (is_reference_type(eff_type)) {
    eff_type = type_pointed_to(eff_type);
  }  /* if */
#endif /* IA64_ABI */
#endif /* GENERATE_EH_TABLES */
  /* Strip typerefs but watch out for rewritten pointers-to-members or
     nullptrs. */
  eff_type = get_underlying_type(eff_type);
  return eff_type;
}  /* eff_type_for_typeinfo */


a_variable_ptr get_typeinfo_var(a_type_ptr type)
/*
Return a pointer to the previously-created typeinfo variable for the
indicated type.  When lowering a function scope, generate the variable
if necessary.
*/
{
  a_variable_ptr typeinfo_var = type->typeinfo_var;

  /* typedefs and cv-qualified types are not allowed at this level. */
  check_assertion(type->kind != (a_type_kind)tk_typeref ||
                  is_or_was_nullptr_type(type) ||
                  is_or_was_ptr_to_member_function_type(type) ||
                  is_or_was_ptr_to_data_member_type(type));
  if (typeinfo_var == NULL) {
    /* typeinfo variables for types should have been generated already,
       except for local types and for types like pointer types which
       have a different underlying effective type. */
#if CHECKING
    an_eh_type_flags_set flags_value;
    check_assertion_str(!lowering_file_scope ||
                        eff_type_for_typeinfo(type, &flags_value,
                                              (a_variable_ptr *)NULL)->
                                                          typeinfo_var != NULL,
                        "missing typeinfo variable");
#endif /* CHECKING */
    typeinfo_var = make_typeinfo_var(type);
  }  /* if */
  return typeinfo_var;
}  /* get_typeinfo_var */


static a_variable_ptr typeinfo_var_for_type(
                                           a_type_ptr           type,
                                           an_eh_type_flags_set *flags_value,
                                           a_variable_ptr       *ptr_flags_var)
/*
Create the typeinfo variable for the indicated type, and return a pointer
to it.  This is used to create the representation for a type used in
exception handling: the typeinfo returned is the "interesting" part
of the type, and the relationship of the original type to the typeinfo
type is indicated in the returned value of *flags_value and
*ptr_flags_var.  See eff_type_for_typeinfo for details.
*/
{
  a_variable_ptr typeinfo_var;
  a_type_ptr     eff_type;

  eff_type = eff_type_for_typeinfo(type, flags_value, ptr_flags_var);
  /* Get the typeinfo variable. */
  typeinfo_var = get_typeinfo_var(eff_type);
  return typeinfo_var;
}  /* typeinfo_var_for_type */


static void generate_type_typeinfo_var_if_needed(a_type_ptr type)
/*
Generate a typeinfo variable for the indicated type, if necessary.
Also define it if necessary.
*/
{
  if (type->used_in_exception_or_rtti) {
    /* This type was used in an exception handling or RTTI construct,
       so a typeinfo variable is needed. */
    /* Switch to the effective underlying type for typeinfo purposes. */
    an_eh_type_flags_set flags_value;
    type = eff_type_for_typeinfo(type, &flags_value, (a_variable_ptr *)NULL);
    (void)make_typeinfo_var(type);
  }  /* if */
  /* A typeinfo variable might also have been previously generated for
     some reason (e.g., it's pointed to from a virtual function table). */
  if (type->typeinfo_var != NULL) {
    /* For non-class types, the typeinfo variable definition was
       put out immediately.  For class types, we have to decide whether
       to put out the definition now. */
    if (is_immediate_class_type(type)) {
      generate_class_typeinfo_var_definition(type);
    }  /* if */
#if MAINTAIN_NEEDED_FLAGS && !GENERATE_EH_TABLES
    if (is_pointer_type(type) &&
        is_class_struct_union_type(type_pointed_to(type))) {
      /* Save a pointer to the class typeinfo variable so we can determine if
         this typeinfo variable is indeed "needed" during the "needed walk". */
      a_type_ptr class_type = skip_typerefs(type_pointed_to(type));
      check_assertion(class_type->typeinfo_var != NULL);
      type->typeinfo_var->eff_class_typeinfo_var = class_type->typeinfo_var;
    }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS && !GENERATE_EH_TABLES */
  }  /* if */
}  /* generate_type_typeinfo_var_if_needed */


/* Forward declaration needed because of mutual recursion: */
static void generate_scope_typeinfo_vars(a_scope_ptr scope);


static void generate_type_list_typeinfo_vars(a_type_ptr type_list)
/*
Visit all the types on the indicated list and its subtree and
generate/define typeinfo variables for any types that need them.
*/
{
  a_type_ptr type;

  for (type = type_list; type != NULL; type = type->next) {
    if (ignore_type_in_back_end(type)) continue;
    generate_type_typeinfo_var_if_needed(type);
    if (is_immediate_class_type(type)) {
      a_class_type_supplement_ptr ctsp =
                                   type->variant.class_struct_union.extra_info;
      if (!scope_is_null_or_placeholder(ctsp->assoc_scope)) {
        generate_scope_typeinfo_vars(ctsp->assoc_scope);
      }  /* if */
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
      generate_type_list_typeinfo_vars(ctsp->promoted_local_types);
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
    }  /* if */
  }  /* for */
}  /* generate_type_list_typeinfo_vars */


static void generate_scope_typeinfo_vars(a_scope_ptr scope)
/*
Visit all the types of the indicated scope and its subtree and generate/
define typeinfo variables for any types that need them.
*/
{
  a_scope_ptr     block_scope;
  a_namespace_ptr nsp;

  /* Visit all types on the types list. */
  generate_type_list_typeinfo_vars(scope->types);
  /* Visit all namespace scopes. */
  for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
    if (!nsp->is_namespace_alias) {
      generate_scope_typeinfo_vars(nsp->variant.assoc_scope);
    }  /* if */
  }  /* for */
  /* Visit all block scopes. */
  for (block_scope = scope->scopes;
       block_scope != NULL;
       block_scope = block_scope->next) {
    generate_scope_typeinfo_vars(block_scope);
  }  /* for */
}  /* generate_scope_typeinfo_vars */


void generate_typeinfo_vars(void)
/*
Generate declarations and/or definitions for all typeinfo variables needed.
This is called near the beginning of lowering of the file scope, but
after generation of definitions for virtual function tables (because
some typeinfo entries are defined if and only if the associated virtual
function table is defined).  Declarations (but not definitions) will
have already been generated for some typeinfo variables, e.g., those
pointed to from virtual function tables.
*/
{
  a_scope_orphaned_list_header_ptr solhp;

  in_typeinfo_var_generation_phase = TRUE;
  generate_scope_typeinfo_vars(il_header.primary_scope);
  /* Process function-local types by visiting the types on orphan lists. */
  for (solhp = il_header.scope_orphaned_list_headers;
       solhp != NULL;
       solhp = solhp->next) {
    generate_type_list_typeinfo_vars(solhp->orphaned_types);
  }  /* for */
  /* Also visit types on the list of nontag exception handling/RTTI types. */
  generate_type_list_typeinfo_vars(
                             il_header.nontag_types_used_in_exception_or_rtti);
  in_typeinfo_var_generation_phase = FALSE;
}  /* generate_typeinfo_vars */

#if GENERATE_EH_TABLES
#if DO_FULL_PORTABLE_EH_LOWERING

static a_variable_ptr make_unnamed_local_array_var(a_type_ptr elem_type)
/*
Create an unnamed local (auto) variable whose type is an array of elem_type,
and return a pointer to the variable.  The array size is begun as [0] and
will be adjusted as elements are added.  finish_array_var must be called
sometime later to set the size on the type.  The variable is put in the
current function scope even if the current context is a block inside that.
*/
{
  return make_temporary_in_scope(array_of(elem_type), innermost_function_scope,
                                 /*force_static=*/FALSE,
                                 /*promote_if_necessary=*/FALSE);
}  /* make_unnamed_local_array_var */


/*
Pointer to the variable entry for the object address array table of
a function.  NULL until allocated.
*/
STATIC_THREAD a_variable_ptr
		object_addr_table_var;


a_handle_number object_addr_table_index(void)
/*
Add an entry to the object address table array (creating the table and its
associated variable if necessary) and return its index number.  The entry
is not filled in (that's done by init_object_addr_table_entry); this routine
just increases the size of the array by one.
*/
{
  a_handle_number entry_number;

  /* Make the variable if it has not yet been made. */
  if (object_addr_table_var == NULL) {
    /* The variable is an array whose elements have type "void *". */
    a_type_ptr elem_type = void_star_type();
    object_addr_table_var = make_unnamed_local_array_var(elem_type);
  }  /* if */
  /* Add an element to the object address table array. */
  entry_number = (a_handle_number)
                               incr_nelems_of_array_var(object_addr_table_var);
  return entry_number;
}  /* object_addr_table_index */


void init_object_addr_table_entry(an_init_pos_descr_ptr ipdp,
                                  a_handle_number       entry_number,
                                  an_insert_location    *insert_location)
/*
Initialize entry entry_number of the object address table so that it
points to the entity identified by ipdp.  Any code required is inserted
at *insert_location and *insert_location is updated.
*/
{
  an_expr_node_ptr object_addr_table_node, subsc_node, object_addr_node;

  /* Insert code to initialize the element of the table to the address of the
     object, i.e.,
       object_addr_table[n] = (void *)ipdp-address;
  */
  object_addr_table_node =
                          array_first_element_addr_expr(object_addr_table_var);
  object_addr_table_node->next = node_for_integer_constant((long)entry_number,
                                                         targ_size_t_int_kind);
  subsc_node = make_lvalue_operator_node((an_expr_operator_kind)eok_subscript,
                                         type_pointed_to(
                                                 object_addr_table_node->type),
                                         object_addr_table_node);
  object_addr_node = add_cast_if_necessary(make_address_of_init_entity_node(
                                                      ipdp,
                                                      /*using_as_dest=*/FALSE),
                                           void_star_type());
  (void)insert_assignment_statement(subsc_node,
                                    (an_expr_operator_kind)eok_assign,
                                    object_addr_node, insert_location);
}  /* init_object_addr_table_entry */

#endif /* DO_FULL_PORTABLE_EH_LOWERING */
#if !DO_FULL_PORTABLE_EH_LOWERING

static a_targ_size_t offset_for_init_modifiers(
                                            an_init_pos_modifier_ptr modifiers)
/*
Return the byte offset that results from the indicated list of modifiers
of an init position description.
*/
{
  a_targ_size_t offset = 0;

  /* If there are no modifiers, return 0. */
  if (modifiers != NULL) {
    /* Process the modifiers preceding the final modifier, then add the final
       qualifier (recall that the modifiers are in order from the innermost
       to the outermost). */
    offset = offset_for_init_modifiers(modifiers->next);
    /* Add the final modifier. */
    if (modifiers->curr_field != NULL) {
      /* Add a field selection. */
      a_field_ptr field = modifiers->curr_field;
      offset += field->offset;
    } else if (modifiers->curr_base != NULL) {
      /* Add a base class selection.  Note that we assume a complete object
         here, which is okay for the intended use of this routine. */
      offset += modifiers->curr_base->offset;
    } else {
      /* Add an array element selection. */
      a_type_ptr elem_type = f_skip_typerefs(modifiers->type);
      offset += elem_type->size * modifiers->curr_elem;
    }  /* if */
  }  /* if */
  return offset;
}  /* offset_for_init_modifiers */

#endif /* !DO_FULL_PORTABLE_EH_LOWERING */

/*
Bit flags for the flags field of a region description.  These must match
the runtime's definition.
*/
typedef unsigned long a_region_descr_flags_set;
#define RDF_NONE		0
#define RDF_INDIRECT		0x01
			/* TRUE if the address provided by the handle field
			   is a pointer to the object.  Not used in the
			   fully-lowered portable form. */
#define RDF_CONDITIONAL_FLAG	0x02
			/* TRUE if the object has an associated flag that
			   indicates whether the construction has occurred.
			   The region entry following this one gives the
			   location of the flag. */
#define RDF_NEW_ALLOCATION	0x04
			/* TRUE if the object was allocated by new and
			   is to be freed in the event of a throw.  Also
			   used to deallocate VLAs. */
#define RDF_ARRAY		0x08
			/* TRUE if the object is an array (or requires
			   information normally provided only for arrays). */
#define RDF_THIS_PARAM_OFFSET	0x10
			/* TRUE if the object is at an address relative to
			   the "this" parameter of the current routine, i.e.,
			   it's a base class or member being handled in
			   a constructor or destructor.  Not used in the
			   portable scheme. */
#if !DO_FULL_PORTABLE_EH_LOWERING && USING_KAI_INLINER
/*
This flag is used by the Kuck & Associates optimizer/inliner when doing
partial EH lowering.  The flag is not generated by IL lowering or used
by the EDG-supplied runtime.  
*/
#define RDF_LET_THIS            0x20
                        /* TRUE if address computed should be used as a 
                           "formal this" parameter.  The region entry
                           following this one describes what to do
                           with the "formal this" pointer.  The
                           following entry will have the
                           RDF_THIS_PARAM_OFFSET flag set, and
                           possibly the RDF_LET_THIS flag set.
                           Chained RDF_LET_THIS entries allow object
                           addresses to be specified when multiple
                           (indirection+offset) operations are
                           necessary to reach the object from a
                           stack-local variable or the "actual this".
                           More detailed information on RDF_LET_THIS
                           is available from KAI. */
			/* Note that this uses the same bit as
			   RDF_SUBOBJECT_VTABLE, and is valid only when
			   RDF_BASE_CLASS_SUBOBJECT is FALSE and RDF_ARRAY
			   is FALSE. */
#endif /* !DO_FULL_PORTABLE_EH_LOWERING && USING_KAI_INLINER */
#define RDF_SUBOBJECT_VTABLE		0x20
			/* When RDF_BASE_CLASS_SUBOBJECT is TRUE and
			   RDF_ARRAY is FALSE, this flag indicates that
			   a region entry following this one gives the
			   address of the subobject destruction vtable
			   table to be used when calling the destructor.
			   This argument is also used to pass a pointer
			   to a "delegation" destructor.
			   If there is also an extra entry for a conditional
			   flag, the subobject vtable entry follows the flag
			   entry.  Note that this uses the same bit as
			   RDF_LET_THIS. */
#define RDF_BASE_CLASS_SUBOBJECT	0x40
			/* TRUE if the object is a base class of some other
			   object and therefore is not a complete object.
			   Note that this is meaningful only when RDF_ARRAY
			   is FALSE, because it uses the same bit as
			   RDF_VLA. */
#define RDF_VLA				0x40
			/* TRUE if the object is a variable-length array
			   (VLA) and a region entry following this one gives
			   the address of a variable containing the array
			   element count.  If there is also an extra entry
			   for a conditional flag, the element count entry
			   follows the flag entry.  Note that this is
			   meaningful only when RDF_ARRAY is TRUE, because
			   it uses the same bit as RDF_BASE_CLASS_SUBOBJECT. */
#define RDF_GUARD_VAR_FOR_LOCAL_STATIC	0x80
			/* TRUE if the object is the guard variable associated
			   with the initialization of a local static variable.
			   The cleanup action is to set the variable back
			   to zero. */


/* Type used to carry information about the location of an entity in the
   form used in the region table. */
#if DO_FULL_PORTABLE_EH_LOWERING
/* In the portable scheme, all that's needed is an index into the object
   address table. */
typedef a_handle_number a_handle;
#define clear_handle(handle) (*(handle) = 0)
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
typedef struct a_handle {
  /* Representation for the non-portable scheme, which can result in a
     ck_stack_offset constant. */
  a_variable_ptr
		variable;
			/* A variable whose offset in the stack is the base
			   for the handle value, or NULL if there is no such
			   variable.  If this is non-NULL, a ck_stack_offset
			   constant will be required. */
  a_targ_size_t	offset;	/* Offset relative to the variable if there is one, or
			   constant value if there is no variable. */
  a_region_descr_flags_set
		flags;	/* Extra flag bits needed, e.g., RDF_INDIRECT. */
} a_handle;

static void clear_handle(a_handle *handle)
/*
Clear the fields of a handle to default values.  A handle is used to
represent the address of an entity in the region table.
*/
{
  handle->variable = NULL;
  handle->offset = 0;
  handle->flags = 0;
}  /* clear_handle */

#endif /* DO_FULL_PORTABLE_EH_LOWERING */


static void make_handle_for_entity(
                                an_init_pos_descr_ptr         ipdp,
                                a_handle                      *handle,
                                ARG_UNUSED an_insert_location *insert_location)
/*
Return the handle (identifier to be used in the region table) for the
entity indicated by ipdp.  The handle is placed in *handle.  If any code
needs to be generated to establish that handle, insert the code at
*insert_location.
*/
{
#if DO_FULL_PORTABLE_EH_LOWERING
  /* Portable scheme: allocate the proper entry in the object address table. */
  a_handle_number handle_number = object_addr_table_index();
  *handle = handle_number;
  /* Put the entity address in the object address table. */
  init_object_addr_table_entry(ipdp, handle_number, insert_location);
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
  /* Non-portable scheme.  Use a stack offset, with variations. */
  clear_handle(handle);
  if (ipdp->indirect_through_variable) {
    if (ipdp->variable->is_this_parameter) {
      /* This entity is relative to the "this" parameter, e.g., it's a
         member or base class being initialized in a constructor. */
      handle->flags |= RDF_THIS_PARAM_OFFSET;
      handle->offset = offset_for_init_modifiers(ipdp->modifiers);
      /* variable stays NULL. */
    } else {
      /* Indirection through a variable (with or without an offset). */
      handle->flags |= RDF_INDIRECT;
      handle->variable = ipdp->variable;
      if (ipdp->modifiers != NULL) {
        handle->offset = offset_for_init_modifiers(ipdp->modifiers);
      }  /* if */
    }  /* if */
  } else {
    /* Not indirect. */
    handle->variable = ipdp->variable;
    handle->offset = offset_for_init_modifiers(ipdp->modifiers);
  }  /* if */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
}  /* make_handle_for_entity */


static a_constant_ptr make_handle_constant(a_handle *handle)
/*
Make a constant for the indicated handle (description of the location of
a variable) and return a pointer to the constant.
*/
{
  a_constant_ptr handle_con;

#if DO_FULL_PORTABLE_EH_LOWERING
  /* Portable scheme: the number is the index in the object address table
     or the array table. */
  handle_con = alloc_constant((a_constant_repr_kind)ck_integer);
  set_unsigned_integer_constant_with_overflow_check(
                                    handle_con, (a_host_large_unsigned)*handle,
                                    targ_var_handle_int_kind,
                                    (a_type_ptr)NULL,
                                    /*preserve_needed_flag=*/FALSE);
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
  /* Non-portable scheme -- can use a ck_stack_offset for the offset of
     a variable. */
  if (handle->variable != NULL) {
    a_variable_ptr var = handle->variable;
    a_type_ptr     var_handle_type = integer_type(targ_var_handle_int_kind);
    if (var_has_static_or_thread_storage_duration(var)) {
      /* A guard variable for the initialization of a static variable,
         which is itself static.  Put the address of the variable in the
         constant, cast to the appropriate integral type. */
      handle_con = alloc_constant((a_constant_repr_kind)ck_address);
      set_variable_address_constant(var, handle_con,
                                    /*set_address_taken_flag=*/TRUE);
      handle_con->variant.address.offset = handle->offset;
      implicit_cast(handle_con, var_handle_type);
    } else {
      /* Normal case (auto variable). */
      handle_con = alloc_constant((a_constant_repr_kind)ck_stack_offset);
      handle_con->type = var_handle_type;
      handle_con->variant.stack_offset.variable = var;
      handle_con->variant.stack_offset.offset = handle->offset;
      set_variable_address_taken(var);
    }  /* if */
  } else {
    /* No variable, so this is a simple constant (e.g., an index into the
       array table). */
    handle_con = alloc_constant((a_constant_repr_kind)ck_integer);
    set_unsigned_integer_constant_with_overflow_check(handle_con,
                                         (a_host_large_unsigned)handle->offset,
                                         targ_var_handle_int_kind,
                                         (a_type_ptr)NULL,
                                         /*preserve_needed_flag=*/FALSE);
  }  /* if */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
  return handle_con;
}  /* make_handle_constant */


/*
Pointer to the array_descr struct type (used as a supplement to
the region description entry to represent an array object for exception
processing).  NULL until created.
*/
STATIC_THREAD a_type_ptr
		array_descr_type;


static a_type_ptr make_array_descr_type(void)
/*
Make the array_descr struct type (used as a supplement to the region
description entry to represent an array object for exception processing)
if it is not made already, and return a pointer to it.  Its definition is

  struct array_descr {
    unsigned short handle;     // Index of object in object address table
                               // (or stack offset in non-portable scheme)
    size_t         elem_size;  // Array element size
    long           elem_count; // Element count
  };

*/
{
  a_field_ptr   last_field;

  if (array_descr_type == NULL) {
    /* Make the struct type. */
    array_descr_type = make_lowered_class_type((a_type_kind)tk_struct);
    add_to_front_of_file_scope_types_list(array_descr_type);
    last_field = NULL;
    /* field: unsigned short (or whatever) handle */
    make_lowered_field("handle",
                       integer_type(targ_var_handle_int_kind),
                       array_descr_type, &last_field);
    /* field: size_t elem_size */
    make_lowered_field("elem_size",
                       integer_type(targ_size_t_int_kind),
                       array_descr_type, &last_field);
    /* field: long elem_count */
    make_lowered_field("elem_count",
                       integer_type((an_integer_kind)ik_long),
                       array_descr_type, &last_field);
    finish_class_type(array_descr_type);
  }  /* if */
  return array_descr_type;
}  /* make_array_descr_type */


/*
Pointer to the variable entry for the array table of a function, and to
the top-level aggregate constant that is its initial value.  NULL
until allocated.
*/
STATIC_THREAD a_variable_ptr
		array_table_var;
STATIC_THREAD a_constant_ptr
		array_table_aggr_con;


static void make_array_table_entry(an_init_pos_descr_ptr ipdp,
                                   a_handle              *handle,
                                   a_boolean             is_vla)
/*
Add an entry to the array table (creating the table and its associated
variable if necessary) for the object whose position is given by ipdp.
The handle (identifying information for the region table) for
the object is provided in *handle; it is modified appropriately
on return.  is_vla is TRUE if the object is a variable-length
array (VLA).  This routine can also be called for non-arrays in cases
where an array table entry is needed to provide information not
included in the region description entry (for example, for a
new-allocation record in a case where the delete routine requires a
second parameter giving the size; the size is not available in the
region description entry).
*/
{
  a_handle_number  entry_number;
  a_targ_ptrdiff_t elem_count;
  a_constant_ptr   handle_con, elem_size_con, size_con, aggr_con;
  a_type_ptr       elem_type;
  a_type_ptr       entity_type = type_from_init_pos_descr(ipdp);

  /* Make the variable for the array table if it has not yet been made. */
  if (array_table_var == NULL) {
    /* The variable is an array whose elements have type array_descr. */
    array_table_var =
          make_init_unnamed_local_static_array_var(make_array_descr_type(),
                                                   /*in_function_scope=*/TRUE,
                                                   &array_table_aggr_con);
  }  /* if */
  /* Make the aggregate constant for the entry in the array table.  It consists
     of the handle offset, the size of each element, and the number of
     elements. */
  /* Make the handle constant. */
  handle_con = make_handle_constant(handle);
  if (is_vla) {
    /* The object is a variable-length array.  The caller will pass RDF_VLA
       and the location of a variable that gives the number of elements.
       The value put in the table is arbitrary, but use -1. */
    elem_count = -1;
    elem_type = entity_type;
    if (!ipdp->array_element_sequence) {
      elem_type = underlying_array_element_type(entity_type);
    }  /* if */
  } else if (ipdp->array_element_sequence) {
    /* The entity is a sequence of array elements.  Get the element
       count.  -1 indicates that the runtime should look up the number of
       elements in the array.  Note that the position given is the position
       of the first element, not of the whole array. */
    if (is_array_type(entity_type)) {
      /* For a multi-dimensional array, make sure we get the size of
         the underlying element type. */
      elem_type = underlying_array_element_type(entity_type);
    } else {
      elem_type = entity_type;
    }  /* if */
    elem_count = ipdp->array_element_count;
  } else if (is_array_type(entity_type)) {
    /* The entity is a whole array. */
    elem_type = underlying_array_element_type(entity_type);
    elem_count = (a_targ_ptrdiff_t)num_array_elements(entity_type);
  } else {
    /* Not an array (see header comment above).  Use an element count of 0. */
    elem_type = entity_type;
    elem_count = 0;
  }  /* if */
  elem_type = skip_typerefs(elem_type);
  elem_size_con = alloc_constant((a_constant_repr_kind)ck_integer);
  set_unsigned_integer_constant(elem_size_con,
                                (a_host_large_unsigned)elem_type->size,
                                targ_size_t_int_kind);
  size_con = alloc_constant((a_constant_repr_kind)ck_integer);
  set_integer_constant(size_con, (a_host_large_integer)elem_count,
                       (an_integer_kind)ik_long);
  /* Link the constants together and make an aggregate constant. */
  handle_con->next = elem_size_con;
  elem_size_con->next = size_con;
  aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
  aggr_con->type = array_descr_type;
  aggr_con->variant.aggregate.first_constant = handle_con;
  aggr_con->variant.aggregate.last_constant = size_con;
  /* Add the aggregate as an element of the object address table array. */
  entry_number = (a_handle_number)add_elem_to_array_var(aggr_con,
                                                        array_table_var,
                                                        array_table_aggr_con);
  /* Adjust the handle to refer to the index into the array table in place
     of the original object. */
#if DO_FULL_PORTABLE_EH_LOWERING
  *handle = entry_number;
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
  handle->variable = NULL;
  handle->offset = entry_number;
  /* Set the flag that indicates this object is an array. */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
}  /* make_array_table_entry */


/*
Pointer to the region_descr struct type (used to represent a cleanup
region for exception processing).  NULL until created.
*/
STATIC_THREAD a_type_ptr
		region_descr_type;


static a_type_ptr make_region_descr_type(void)
/*
Make the region_descr struct type (used to represent the cleanup required
in a particular region for exception processing) if it is not made already,
and return a pointer to it.  Its definition is

  struct region_descr {
    __vptp         dtor;    // Destructor or delete routine pointer
    unsigned short handle;  // Index of object in object address table
                            // (or stack offset in non-portable scheme)
    unsigned short next;    // Next cleanup region
    unsigned char  flags;   // Bit flags
  };

*/
{
  a_field_ptr   last_field;

  if (region_descr_type == NULL) {
    /* Make the struct type. */
    region_descr_type = make_lowered_class_type((a_type_kind)tk_struct);
    add_to_front_of_file_scope_types_list(region_descr_type);
    last_field = NULL;
    /* field: __vptp dtor */
    make_lowered_field("dtor", make_vptp_type(),
                       region_descr_type, &last_field);
    /* field: unsigned short (or whatever) handle */
    make_lowered_field("handle",
                       integer_type(targ_var_handle_int_kind),
                       region_descr_type, &last_field);
    /* field: unsigned short next */
    make_lowered_field("next",
                       integer_type(targ_region_number_int_kind),
                       region_descr_type, &last_field);
    /* field: unsigned char flags */
    make_lowered_field("flags",
                       integer_type((an_integer_kind)ik_unsigned_char),
                       region_descr_type, &last_field);
    finish_class_type(region_descr_type);
  }  /* if */
  return region_descr_type;
}  /* make_region_descr_type */


a_cleanup_region_number cleanup_region_number(a_dynamic_init_ptr dip)
/*
Return the cleanup region number for the indicated initialization.
If dip is NULL, return null_eh_region_number.
*/
{
  a_cleanup_region_number region_number;

  /* Get the region number. */
  if (dip != NULL) {
    a_destructible_entity_descr_ptr dedp = dip->destructible_entity_descr;
    check_assertion(dedp != NULL);
    region_number = dedp->region_number;
  } else {
    /* There is no region.  Use a code that indicates that. */
    region_number = null_eh_region_number;
  }  /* if */
  return region_number;
}  /* cleanup_region_number */


static a_constant_ptr next_region_number_constant(a_dynamic_init_ptr dip)
/*
Return a pointer to the next-region-number constant of the region table
entry for the indicated destruction.
*/
{
  a_destructible_entity_descr_ptr
                 dedp = dip->destructible_entity_descr;
  a_constant_ptr aggr_con = dedp->region_table_entry;
  /* The constant for the next region number is the third one on the
     list of constants in the region table entry.
     See add_region_table_entry. */
  a_constant_ptr con = aggr_con->variant.aggregate.first_constant->next->next;
  return con;
}  /* next_region_number_constant */


static void set_next_region_number(a_dynamic_init_ptr      dip,
                                   a_cleanup_region_number next_region_number)
/*
Set the next region number of the indicated destruction to next_region_number.
This routine is used to relink entries after they've been created.
*/
{
  a_constant_ptr con = next_region_number_constant(dip);
  a_constant_ptr con_next = con->next;

  set_unsigned_integer_constant(con, (a_host_large_unsigned)next_region_number,
                                targ_region_number_int_kind);
  con->next = con_next;
}  /* set_next_region_number */

#if DO_UNORDERED_EH_PROCESSING

static a_cleanup_region_number get_next_region_number(a_dynamic_init_ptr dip)
/*
Fetch the next region number of the indicated destruction.
*/
{
  a_constant_ptr          con = next_region_number_constant(dip);
  a_boolean               ovflo;
  a_cleanup_region_number next_region_number =
      (a_cleanup_region_number)unsigned_value_of_integer_constant(con, &ovflo);
  check_assertion(!ovflo);
  return next_region_number;
}  /* get_next_region_number */

#endif /* DO_UNORDERED_EH_PROCESSING */

STATIC_THREAD a_cleanup_region_number
		next_avail_region_number;
			/* Next available destructible object region number
			   within the current function. */

/*
Pointer to the variable entry for the region table of a function (which
contains information about destructible objects), and to the top-level
aggregate constant that is its initial value.  NULL until allocated.
*/
STATIC_THREAD a_variable_ptr
		region_table_var;
STATIC_THREAD a_constant_ptr
		region_table_aggr_con;


static a_constant_ptr add_raw_region_table_entry(a_constant_ptr con_list,
                                                 a_constant_ptr end_con_list)
/*
Create a region table entry from the constants on the list delimited by
con_list and end_con_list and add it to the region table array.  Return
a pointer to the aggregate constant created.
*/
{
  a_constant_ptr aggr_con;

  aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
  aggr_con->type = region_descr_type;
  aggr_con->variant.aggregate.first_constant = con_list;
  aggr_con->variant.aggregate.last_constant = end_con_list;
  /* Add the aggregate as an element of the region table array. */
  (void)add_elem_to_array_var(aggr_con, region_table_var,
                              region_table_aggr_con);
  /* Increment the count of entries in the array. */
  next_avail_region_number++;
  if (next_avail_region_number >= null_eh_region_number) {
    /* Too many regions. */
    catastrophe(ec_program_too_large);
  }  /* if */
  return aggr_con;
}  /* add_raw_region_table_entry */


static a_constant_ptr add_region_table_entry(
                                         a_routine_ptr            dtor_routine,
                                         a_handle                 *handle,
                                         a_cleanup_region_number  next_region,
                                         a_region_descr_flags_set flags_value)
/*
Create an entry in the exception cleanup region table.  dtor_routine,
handle, next_region, and flags_value give the values for the various
fields.   Create an aggregate constant for the entry and add
it to the initial value of region_table_var.  Return the address of
the aggregate constant.
*/
{
  a_constant_ptr dtor_con, handle_con, next_con, flags_con, aggr_con;
  a_type_ptr     ptr_func_type;

#if !DO_FULL_PORTABLE_EH_LOWERING
  /* Move any addressing flags (e.g., RDF_INDIRECT) from the handle to
     the flags value. */
  flags_value |= handle->flags;
#endif /* !DO_FULL_PORTABLE_EH_LOWERING */
  /* Make the aggregate constant for the entry in the region description
     table.  It has a structure as follows:
       struct region_descr {
         __vptp         dtor;    // Destructor or delete routine pointer
         unsigned short handle;  // Index of object in object address table
                                 // (or stack offset in non-portable scheme)
         unsigned short next;    // Next cleanup region
         unsigned char  flags;   // Bit flags
       };
  */
  /* Make the destructor pointer. */
  dtor_con = alloc_constant((a_constant_repr_kind)ck_address);
  /* Create the generic function pointer type if it does not exist already. */
  ptr_func_type = make_vptp_type();
  if (dtor_routine == NULL) {
    /* The object has no destructor; use a NULL pointer. */
    make_zero_of_proper_type(ptr_func_type, dtor_con);
  } else {
    /* The class has a destructor.  Make a pointer to the routine. */
    dtor_routine->source_corresp.referenced = TRUE;
    set_routine_address_constant(dtor_routine, dtor_con,
                                 /*set_address_taken_flag=*/TRUE);
    implicit_cast(dtor_con, ptr_func_type);
  }  /* if */
  /* Make the handle constant. */
  handle_con = make_handle_constant(handle);
  /* Make the next region index number.  NOTE that next_region_number_constant
     expects the constant to be the third one on the list. */
  next_con = alloc_constant((a_constant_repr_kind)ck_integer);
  set_unsigned_integer_constant(next_con, (a_host_large_unsigned)next_region,
                                targ_region_number_int_kind);
  /* Make the flags constant. */
  flags_con = alloc_constant((a_constant_repr_kind)ck_integer);
  set_unsigned_integer_constant(flags_con, (a_host_large_unsigned)flags_value,
                                (an_integer_kind)ik_unsigned_char);
  /* Link the constants together to make an aggregate constant. */
  dtor_con->next = handle_con;
  handle_con->next = next_con;
  next_con->next = flags_con;
  /* Make the aggregate and add it as an element of the region table array. */
  aggr_con = add_raw_region_table_entry(dtor_con, flags_con);
  return aggr_con;
}  /* add_region_table_entry */


/*
Pointer to the runtime routine __vla_dealloc_eh.  NULL until allocated.
*/
STATIC_THREAD a_routine_ptr
                vla_dealloc_eh_routine;

/*
Pointer to the runtime routine __destroy_exception_object.  NULL until
allocated.
*/
STATIC_THREAD a_routine_ptr
                destroy_exception_object_routine;


static a_routine_ptr make_destroy_exception_object_routine(void)
/*
Make (if necessary) the __destroy_exception_object runtime routine, and
return a pointer to it.
*/
{
  a_routine_ptr routine =
             make_prototyped_runtime_routine("__destroy_exception_object",
                                             &destroy_exception_object_routine,
                                             void_type(),
                                             (a_type_ptr)NULL,
                                             (a_type_ptr)NULL,
                                             (a_type_ptr)NULL,
                                             (a_type_ptr)NULL,
                                             (a_type_ptr)NULL,
                                             (a_type_ptr)NULL,
                                             (a_type_ptr)NULL);
  return routine;
}  /* make_destroy_exception_object_routine */


static a_constant_ptr make_region_table_entry(
                        an_init_pos_descr_ptr   ipdp,
                        a_routine_ptr           routine,
                        a_boolean               is_delete,
                        a_boolean               is_freeing_of_exception_object,
                        a_boolean               is_local_static_guard_var,
                        a_boolean               is_vla_deallocation,
                        a_boolean               has_conditional_flag,
                        a_handle                *conditional_flag_handle,
                        a_boolean               has_subobject_vtable,
                        a_handle                *subobject_vtable_handle,
                        a_boolean               is_vla,
                        a_handle                *vla_elem_count_handle,
                        a_boolean               use_delegation_dtor,
                        a_cleanup_region_number next_region_number,
                        a_cleanup_region_number *region_number,
                        an_insert_location      *insert_location)
/*
Add an entry to the region table (which describes destructible
objects) related to the object whose position is given by ipdp.
routine is a destructor (is_delete == FALSE) or a delete routine
(is_delete == TRUE) to be called to do cleanup on the object.
is_freeing_of_exception_object is TRUE if the entry should indicate
the freeing of the runtime exception object on termination of a catch
handler (the object and routine are ignored in that case).
is_local_static_guard_var is TRUE if the object is the guard variable
for a local static variable initialization, and the region entry
should indicate that the guard variable is to be reset to zero (that's
the "destruction" associated with the guard variable).
is_vla_deallocation is TRUE if the object is a variable-length array
(VLA) and the region table entry should indicate that the array must
be deallocated.  has_conditional_flag is TRUE if there is a
conditional flag variable that is non-zero to indicate that the
destruction or deletion should be done.  In that case,
conditional_flag_handle gives the handle for the address for the
conditional flag.  has_subobject_vtable is TRUE if a subobject
construction vtable needs to be passed to the destructor.  In that
case, subobject_vtable_handle gives the handle for the address for the
vtable.  use_delegation_dtor is TRUE if the "delegation" destructor
should be used; in that case has_subobject_vtable must be TRUE and
subobject_vtable_handle gives the handle for the second argument to the
destructor.  is_vla is TRUE if the object is a variable-length array.  In
that case, vla_elem_count_handle gives the handle for the address of a
variable that contains the number of elements in the array.
next_region_number is used as the next-region-table-entry number for
the new entry.  The region table entry number for the new entry is
returned in *region_number.  Any initialization code required will be
inserted at *insert_location.  The region table variable is created if
necessary.  Return a pointer to the aggregate constant for the region
table entry.
*/
{
  a_handle        handle;
  a_region_descr_flags_set
                  flags_value = 0;
  a_constant_ptr  region_table_entry;
  a_boolean       need_array_info = FALSE, need_elem_count = FALSE;

  if (!is_freeing_of_exception_object) {
    /* Make the handle for the entity. */
    make_handle_for_entity(ipdp, &handle, insert_location);
  } else {
    /* No object handle needed. */
    clear_handle(&handle);
  }  /* if */
  /* See if we need array information on the entity. */
#ifndef RUNTIME_VLA_DEALLOCATION_NEEDS_ELEMENT_COUNT
  /* The EDG-supplied runtime VLA deallocation routine doesn't need the
     array element count.  If a different runtime routine is used, this
     macro can be changed accordingly. */
#define RUNTIME_VLA_DEALLOCATION_NEEDS_ELEMENT_COUNT FALSE
#endif /* ifndef RUNTIME_VLA_DEALLOCATION_NEEDS_ELEMENT_COUNT */
#if !RUNTIME_VLA_DEALLOCATION_NEEDS_ELEMENT_COUNT
  if (is_vla_deallocation) {
    /* The runtime doesn't need the element count or array information. */
    /* need_array_info = FALSE;  -- already set. */
  } else
#endif /* !RUNTIME_VLA_DEALLOCATION_NEEDS_ELEMENT_COUNT */
  /* Do not insert code here. */
  if (is_freeing_of_exception_object) {
    /* No array information needed (no object). */
    /* need_array_info = FALSE;  -- already set. */
  } else if (ipdp->array_element_sequence ||
             is_array_type(type_from_init_pos_descr(ipdp))) {
    need_array_info = TRUE;
  } else if (is_delete) {
    /* Check for the 2-argument version of delete; we need array information
       for that because we need the size of the entity. */
    a_param_type_ptr param1 = unlowered_param_type_list_for_routine(routine);
    check_assertion(param1 != NULL);
    if (param1->next != NULL) {
      /* Two-argument form.  Need array information. */
      need_array_info = TRUE;
    }  /* if */
  }  /* if */
  if (need_array_info) {
    /* We need an entry in the array table. */
    make_array_table_entry(ipdp, &handle, is_vla);
    /* Set the flag that indicates this object is an array. */
    flags_value |= RDF_ARRAY;
    if (is_vla) {
      flags_value |= RDF_VLA;
      need_elem_count = TRUE;
    }  /* if */
  }  /* if */
  if (is_local_static_guard_var) {
    flags_value |= RDF_GUARD_VAR_FOR_LOCAL_STATIC;
  }  /* if */
  if (has_conditional_flag) {
    /* This entry needs a conditional flag.  More on this below. */
    /* The object address table entry must be initialized when the conditional
       flag is initialized; here is too late because the runtime needs to
       be able to test the conditional flag even if the associated entity
       was not constructed.  See init_conditional_flag_var. */
    flags_value |= RDF_CONDITIONAL_FLAG;
  }  /* if */
  if (is_delete || is_vla_deallocation) {
    /* Indicate the delete case. */
    flags_value |= RDF_NEW_ALLOCATION;
    if (is_vla_deallocation) {
      /* VLA deallocation uses an RDF_NEW_ALLOCATION and passes the
         runtime routine __vla_dealloc_eh as the "delete" routine. */
      routine = make_prototyped_runtime_routine("__vla_dealloc_eh",
                                                &vla_dealloc_eh_routine,
                                                void_type(),
                                                void_star_type(),
                                                (a_type_ptr)NULL,
                                                (a_type_ptr)NULL,
                                                (a_type_ptr)NULL,
                                                (a_type_ptr)NULL,
                                                (a_type_ptr)NULL,
                                                (a_type_ptr)NULL);
    }  /* if */
  } else if (is_freeing_of_exception_object) {
    /* Freeing of the exception object passes the runtime routine
       __destroy_exception_object as the "destructor". */
    routine = make_destroy_exception_object_routine();
  }  /* if */
  if (use_delegation_dtor) {
    /* Use a "delegation" destructor (since we don't know whether the
       complete or subobject destructor should be invoked).  The
       argument (specified by the subobject_vtable_handle) will be passed
       to the "delegation" destructor and will invoke the complete
       destructor if the parameter is NULL otherwise it will invoke the
       subobject destructor. */
    check_assertion(has_subobject_vtable);
    flags_value |= RDF_BASE_CLASS_SUBOBJECT | RDF_SUBOBJECT_VTABLE;
#if IA64_ABI
    routine = alternate_entry_point(routine,
                                    (a_ctor_or_dtor_kind)cdk_delegation,
                                    /*define_now=*/FALSE);
    check_assertion(dtor_needs_vtt_argument(routine));
#endif /* IA64_ABI */
  } else if (ipdp != NULL && ipdp->base_class_subobject) {
    /* The entity is a base class subobject (in a constructor or
       destructor). */
    flags_value |= RDF_BASE_CLASS_SUBOBJECT;
    if (has_subobject_vtable) {
      /* This entry has a subobject vtable.  More on this below. */
      flags_value |= RDF_SUBOBJECT_VTABLE;
    }  /* if */
#if IA64_ABI
    routine = alternate_entry_point(routine,
                                    (a_ctor_or_dtor_kind)cdk_subobject,
                                    /*define_now=*/FALSE);
    check_assertion(dtor_needs_vtt_argument(routine) == has_subobject_vtable);
  } else if (routine != NULL &&
             (routine->special_kind == 
                                  (a_special_function_kind)sfk_destructor)) {
    routine = alternate_entry_point(routine,
                                    (a_ctor_or_dtor_kind)cdk_complete,
                                    /*define_now=*/FALSE);
#endif /* IA64_ABI */
  }  /* if */
  /* Make the variable for the region table if it has not yet been made. */
  if (region_table_var == NULL) {
    /* The variable is an array whose elements have type array_descr. */
    region_table_var =
          make_init_unnamed_local_static_array_var(make_region_descr_type(),
                                                   /*in_function_scope=*/TRUE,
                                                   &region_table_aggr_con);
  }  /* if */
  /* Assign a region number to this entry. */
  *region_number = next_avail_region_number;
  /* Make the region table entry. */
  region_table_entry = add_region_table_entry(routine,
                                              &handle,
                                              next_region_number,
                                              flags_value);
  if (has_conditional_flag) {
    /* Make a second region table entry for the conditional flag. */
    (void)add_region_table_entry((a_routine_ptr)NULL,
                                 conditional_flag_handle,
                                 null_eh_region_number,
                                 (a_region_descr_flags_set)RDF_NONE);
  }  /* if */
  if (has_subobject_vtable) {
    /* Make an additional region table entry for the subobject vtable
       address. */
    (void)add_region_table_entry((a_routine_ptr)NULL,
                                 subobject_vtable_handle,
                                 null_eh_region_number,
                                 (a_region_descr_flags_set)RDF_NONE);
  } else if (need_elem_count) {
    /* Make an additional region table entry for the VLA element count
       variable address. */
    (void)add_region_table_entry((a_routine_ptr)NULL,
                                 vla_elem_count_handle,
                                 null_eh_region_number,
                                 (a_region_descr_flags_set)RDF_NONE);
  }  /* if */
  return region_table_entry;
}  /* make_region_table_entry */

#if DO_UNORDERED_EH_PROCESSING

static void maintain_unordered_destructions_set(a_dynamic_init_ptr dip)
/*
dip points to a destruction with the "unordered" flag TRUE, for which
the region table entry was just created.  Do some extra processing
required for unordered destructions.
*/
{
  a_destructible_entity_descr_ptr dedp = dip->destructible_entity_descr;
  a_dynamic_init_ptr              next_dip = dedp->next_in_region_table;

  check_assertion(dip->unordered);
  /* If this entry is part of an unordered set, all the region table entries
     for the entities in the unordered set are treated as a block.  That is,
     the entire block goes into the region table cleanup chain as soon as
     any member of the set is initialized, and the entire block stays in
     the region table cleanup chain until all of the entities have been
     destroyed.  Each entity is given a conditional flag so that we can
     tell at runtime which entities have been constructed and not yet
     destroyed.  Since we do not want to go back and fix code, the
     first unordered entry encountered establishes the beginning region
     number for the block.  Subsequent contiguous unordered entries
     are linked into the region table on a list following the region
     table for the initial entry.  The final entry (so far) points to
     the first region number past the ordered set, i.e., the original
     next region number from the initial entry.  The region_number
     and cleanup_state_to_set_when_starting_destruction fields of the
     entries after the first are set to the region number of the first
     entry so that the entire block will be put into the cleanup chain
     and kept there until the destruction for the initial entry is
     generated (it gets generated after the destruction for the others). */
  if (next_dip != NULL && next_dip->unordered) {
    a_destructible_entity_descr_ptr
                              next_dedp = next_dip->destructible_entity_descr;
    /* Relink the previous last entry to this new entry, and this new
       entry to point to the first region number beyond the ordered set. */
    a_cleanup_region_number next_region_number_past_ordered_set =
                                              get_next_region_number(next_dip);
    set_next_region_number(next_dip, dedp->region_number);
    set_next_region_number(dip, next_region_number_past_ordered_set);
    dedp->region_number = next_dedp->region_number;
    dedp->cleanup_state_to_set_when_starting_destruction = next_dip;
  }  /* if */
}  /* maintain_unordered_destructions_set */

#endif /* DO_UNORDERED_EH_PROCESSING */

void make_dyn_init_region_table_entry(a_dynamic_init_ptr dip,
                                      a_dynamic_init_ptr next_dip,
                                      an_insert_location *insert_location)
/*
Add an entry to the region table (which describes destructible objects)
for the initialization described by dip.  The entry will point to
next_dip as its next region.  The initialization must have an attached
destructible entity description, and the conditional_flag_var field of
that entry must be filled in if appropriate (if a conditional flag
variable is indicated, a region table entry will be created for it
as well); cleanup_state_to_set_when_starting_destruction should
also be set (it differs from next_dip in that the latter does not
cross over lifetime boundaries).  Also insert (at *insert_location)
initialization code for the proper entry in the object address table.
The region table variable is created if necessary.
*/
{
  a_destructible_entity_descr_ptr dedp = dip->destructible_entity_descr;
  a_handle                        conditional_flag_handle;
  a_handle                        subobject_vtable_handle;
  a_handle                        vla_elem_count_handle;
  a_cleanup_region_number         next_region_number;
  an_init_pos_descr               ipd;
  a_boolean                       has_subobject_vtable = FALSE;
  a_boolean                       is_vla = FALSE;
  a_boolean                       use_delegation_dtor = FALSE;

  check_assertion(dedp != NULL);
  /* Make a handle that describes the address of the conditional flag if
     any. */
  if (dedp->conditional_flag_var != NULL) {
#if DO_FULL_PORTABLE_EH_LOWERING
    conditional_flag_handle = dedp->conditional_flag_handle;
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
    set_var_init_pos_descr(dedp->conditional_flag_var, &ipd);
    make_handle_for_entity(&ipd, &conditional_flag_handle, insert_location);
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
  }  /* if */
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
  /* Make a handle that describes the address of the subobject vtable
     if any. */
  if (dedp->needs_subobject_construction_vtbl) {
    an_expr_node_ptr vtt_addr_node;
    a_variable_ptr   temp_var;
    has_subobject_vtable = TRUE;
    /* Make an expression that computes the address of the VTT to use. */
#if IA64_ABI
    build_construction_vtbls_pointer(dedp,
                                     (an_init_pos_descr *)NULL,
                                     (an_insert_location_ptr)NULL,
                                     &vtt_addr_node);
#else /* !IA64_ABI */
    { a_construction_vtbl_array_index idx =
                        (dedp->construction_vtbls_var_is_array ?
                          (a_construction_vtbl_array_index)1 :
                          dedp->subobject_construction_base_class->
                               base_subarray_index_in_construction_vtbl_array);
      vtt_addr_node = vtbl_addr_from_construction_vtbls_array(
                             dedp->construction_vtbls_var,
                             (a_boolean)dedp->construction_vtbls_var_is_array,
                             idx);
    }
#endif /* IA64_ABI */
    /* Assign the VTT pointer to a temporary. */
    temp_var = make_lowered_temporary(make_virtual_table_table_pointer_type());
    (void)insert_var_assignment_statement(temp_var, vtt_addr_node,
                                          insert_location);
    set_var_indirect_init_pos_descr(temp_var, &ipd);
    make_handle_for_entity(&ipd, &subobject_vtable_handle, insert_location);
#if !IA64_ABI
    /* Make a routine that sets the transfer pointer and calls the
       destructor, and record that as the "destructor" to be called. */
    dip->destructor = make_subobject_destruction_routine(dip);
#endif /* !IA64_ABI */
  } else if (dedp->use_delegation_dtor) {
    /* It isn't known at compilation time which destructor (i.e., complete
       or subobject) should be used to do the destruction, so invoke a
       "delegation" destructor and let it decide at run-time which to call. */
    use_delegation_dtor = TRUE;
    has_subobject_vtable = TRUE;
    set_var_indirect_init_pos_descr(dedp->delegation_dtor_arg, &ipd);
    make_handle_for_entity(&ipd, &subobject_vtable_handle, insert_location);
#if !IA64_ABI
    /* Create the "delegation" destructor. */
    dip->destructor = make_delegation_destruction_routine(dip);
#endif /* !IA64_ABI */
  }  /* if */
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
#if ABI_COMPATIBILITY_VERSION < 306
  /* For ABI versions before 3.06, VLA exception handling can't be supported.
     VLAs aren't supposed to be allowed to be enabled. */
  check_assertion_str(!vla_enabled,
                      "Exception handling for VLAs not supported");
#else /* ABI_COMPATIBILITY_VERSION >= 306 */
  if (is_dynamic_init_for_vla(dip)) {
    /* Extra information is needed for variable-length arrays. */
#if !RUNTIME_VLA_DEALLOCATION_NEEDS_ELEMENT_COUNT
    if (dip->is_vla_deallocation) {
      /* The runtime doesn't need the array element count in a VLA
         deallocation. */
    } else
#endif /* !RUNTIME_VLA_DEALLOCATION_NEEDS_ELEMENT_COUNT */
    /* Do not insert code here. */
    {
      is_vla = TRUE;
      set_var_init_pos_descr(dip->variable->vla_element_count_variable, &ipd);
      make_handle_for_entity(&ipd, &vla_elem_count_handle, insert_location);
    }  /* if */
  }  /* if */    
#endif /* ABI_COMPATIBILITY_VERSION < 306 */
  dedp->next_in_region_table = next_dip;
  next_region_number = cleanup_region_number(
                         dedp->cleanup_state_to_set_when_starting_destruction);
  dedp->region_table_entry =
             make_region_table_entry(&dedp->init_pos_descr,
                                     dip->destructor,
                                     (a_boolean)dip->
                                            is_freeing_of_storage_on_exception,
                                     (a_boolean)dip->
                                                is_freeing_of_exception_object,
                                     (a_boolean)dip->
                                        is_guard_var_for_local_static_var_init,
                                     (a_boolean)dip->is_vla_deallocation,
                                     (dedp->conditional_flag_var != NULL),
                                     &conditional_flag_handle,
                                     has_subobject_vtable,
                                     &subobject_vtable_handle,
                                     is_vla,
                                     &vla_elem_count_handle,
                                     use_delegation_dtor,
                                     next_region_number,
                                     &dedp->region_number,
                                     insert_location);
#if DO_UNORDERED_EH_PROCESSING
  if (dip->unordered) {
    /* The destruction is unordered with respect to some surrounding
       destructions, so do some extra processing. */
    maintain_unordered_destructions_set(dip);
  }  /* if */
#endif /* DO_UNORDERED_EH_PROCESSING */
}  /* make_dyn_init_region_table_entry */


static a_constant_ptr clone_raw_region_table_entry(
                                        a_constant_ptr          aggr_con,
                                        a_cleanup_region_number *region_number)
/*
Clone the region table entry defined by the aggregate constant.  Return
a pointer to the clone, and set *region_number to the region number for
the clone.
*/
{
  a_constant_ptr con_list, end_con_list, source_con, copy_con, clone_aggr_con;
  
  /* Copy the list of constants. */
  con_list = end_con_list = NULL;
  for (source_con = aggr_con->variant.aggregate.first_constant;
       source_con != NULL;
       source_con = source_con->next) {
    /* Make a copy of the constant. */
    copy_con = alloc_unshared_constant_full(source_con, /*source_in_il=*/TRUE,
                                            /*suppress_copy=*/FALSE);
    /* Add the constant to the list of the aggregate copy. */
    if (con_list == NULL) {
      con_list = copy_con;
    } else {
      check_assertion(end_con_list != NULL);
      end_con_list->next = copy_con;
    }  /* if */
    end_con_list = copy_con;
  }  /* for */
  /* Add the cloned region table entry. */
  *region_number = next_avail_region_number;
  clone_aggr_con = add_raw_region_table_entry(con_list, end_con_list);
  return clone_aggr_con;
}  /* clone_raw_region_table_entry */


void clone_region_table_entry_list(a_dynamic_init_ptr dip,
                                   a_dynamic_init_ptr stop_before)
/*
Clone the region table entry associated with the initialization pointed to
by dip, and all preceding initializations (following the
next_in_region_table pointer), stopping before the entry stop_before.
stop_before == NULL means stop at the beginning of the current lifetime.
*/
{
  a_destructible_entity_descr_ptr dedp = dip->destructible_entity_descr;
  a_cleanup_region_number         next_region_number, region_number;
  a_constant_ptr                  orig_region_table_entry, next_entry;
  a_dynamic_init_ptr              next_dip = dedp->next_in_region_table;

  if (next_dip != stop_before) {
    /* This is not the last entry on the list, so do a recursive call to
       clone the rest of the list. */
    clone_region_table_entry_list(next_dip, stop_before);
  }  /* if */
  /* Clone the entry and update the information in dedp (that is, the clone
     becomes the official entry from now on). */
  orig_region_table_entry = dedp->region_table_entry;
  dedp->region_table_entry = clone_raw_region_table_entry(
                                                       orig_region_table_entry,
                                                       &dedp->region_number);
  next_entry = orig_region_table_entry->next;
  if (dedp->conditional_flag_var != NULL) {
    /* The entry has a conditional flag, so clone the region table entry for
       the conditional flag too. */
    (void)clone_raw_region_table_entry(next_entry, &region_number);
    next_entry = next_entry->next;
  }  /* if */
#if !RUNTIME_VLA_DEALLOCATION_NEEDS_ELEMENT_COUNT
  if (dip->is_vla_deallocation) {
    /* The runtime doesn't need the array element count in a VLA
        deallocation. */
  } else
#endif /* !RUNTIME_VLA_DEALLOCATION_NEEDS_ELEMENT_COUNT */
  /* Do not insert code here. */
  if (is_dynamic_init_for_vla(dip)) {
    /* The entry has a VLA element count entry, so clone that too. */
    (void)clone_raw_region_table_entry(next_entry, &region_number);
  }  /* if */
  /* Link the clone to the proper next entry. */
  next_dip = normalize_cleanup_state_for_outer_lifetimes(next_dip);
  next_region_number = cleanup_region_number(next_dip);
  set_next_region_number(dip, next_region_number);
  dedp->cleanup_state_to_set_when_starting_destruction = next_dip;
#if DO_UNORDERED_EH_PROCESSING
  if (dip->unordered) {
    /* The destruction is unordered with respect to some surrounding
       destructions, so do some extra processing. */
    maintain_unordered_destructions_set(dip);
  }  /* if */
#endif /* DO_UNORDERED_EH_PROCESSING */
}  /* clone_region_table_entry_list */

#endif /* GENERATE_EH_TABLES */
#if DO_FULL_PORTABLE_EH_LOWERING

/*
Variable entries for the global variables used for exception processing.
NULL until created.
*/
STATIC_THREAD a_variable_ptr
		eh_curr_region_var,
		curr_eh_stack_entry_var,
		catch_clause_number_var,
		caught_object_address_var;


static a_variable_ptr make_eh_curr_region_var(void)
/*
Make __eh_curr_region, a global variable used for exception processing,
if it has not already been made.  Return a pointer to it.
*/
{
  if (eh_curr_region_var == NULL) {
    eh_curr_region_var =
               make_lowered_variable("__eh_curr_region",
                                     /*already_il_name=*/FALSE,
                                     integer_type(targ_region_number_int_kind),
                                     (a_storage_class)sc_extern);
  }  /* if */
  return eh_curr_region_var;
}  /* make_eh_curr_region_var */


static void assign_to_eh_curr_region(an_expr_node_ptr   node,
                                     an_insert_location *insert_location)
/*
Insert an assignment to set __eh_curr_region to the given expression node.
The assignment is inserted at *insert_location and *insert_location is
updated.
*/
{
  (void)insert_var_assignment_statement(make_eh_curr_region_var(), node,
                                        insert_location);
}  /* assign_to_eh_curr_region */


static a_variable_ptr make_catch_clause_number_var(void)
/*
Make __catch_clause_number, a global variable used for exception processing,
if it has not already been made.  Return a pointer to it.
*/
{
  if (catch_clause_number_var == NULL) {
    catch_clause_number_var =
               make_lowered_variable("__catch_clause_number",
                                     /*already_il_name=*/FALSE,
                                     integer_type((an_integer_kind)ik_int),
                                     (a_storage_class)sc_extern);
  }  /* if */
  return catch_clause_number_var;
}  /* make_catch_clause_number_var */


static a_variable_ptr make_caught_object_address_var(void)
/*
Make __caught_object_address, a global variable used for exception processing,
if it has not already been made.  Return a pointer to it.
*/
{
  if (caught_object_address_var == NULL) {
    caught_object_address_var =
                             make_lowered_variable("__caught_object_address",
                                                   /*already_il_name=*/FALSE,
                                                   void_star_type(),
                                                   (a_storage_class)sc_extern);
  }  /* if */
  return caught_object_address_var;
}  /* make_caught_object_address_var */


/*
Pointer to the exception_type_spec struct type (used to represent a type
for exception throw and catch specifications).  NULL until created.
*/
STATIC_THREAD a_type_ptr
		exception_type_spec_type;


static a_type_ptr make_exception_type_spec_type(void)
/*
Make the exception_type_spec struct type (used to represent a type
for exception throw and catch specifications) if it is not made already,
and return a pointer to it.  Its definition is

  struct exception_type_spec {
    const typeinfo *tinfo;
    targ_ets_flag_type_int_kind   // Originally "unsigned char", now "int"
                   flags;
    targ_ets_flag_type_int_kind   // Originally "unsigned char", now "int"
                   *ptr_flags;
  };

The ptr_flags field is not present for ABI levels less than 2.41.
*/
{
  a_field_ptr   last_field;

  if (exception_type_spec_type == NULL) {
    /* Make the struct type. */
    exception_type_spec_type = make_lowered_class_type((a_type_kind)tk_struct);
    add_to_front_of_file_scope_types_list(exception_type_spec_type);
    last_field = NULL;
    /* field: typeinfo *tinfo */
    make_lowered_field("tinfo", 
                       make_ptr_to_const_typeinfo_type(),
                       exception_type_spec_type, &last_field);
    /* field: targ_ets_flag_type_int_kind flags */
    make_lowered_field("flags",
                       integer_type(targ_ets_flag_type_int_kind),
                       exception_type_spec_type, &last_field);
#if ABI_COMPATIBILITY_VERSION >= 241
    /* field: targ_ets_flag_type_int_kind *ptr_flags */
    make_lowered_field("ptr_flags",
                       make_pointer_type(
                                    integer_type(targ_ets_flag_type_int_kind)),
                       exception_type_spec_type, &last_field);
#endif /* ABI_COMPATIBILITY_VERSION >= 241 */
    finish_class_type(exception_type_spec_type);
  }  /* if */
  return exception_type_spec_type;
}  /* make_exception_type_spec_type */


static a_variable_ptr make_exception_type_spec_array_var(
                                                      a_constant_ptr first_con,
                                                      a_constant_ptr last_con,
                                                      unsigned long  num_elems)
/*
Create a variable whose initial value is an array of exception type
specification entries, and return a pointer to the variable.  first_con
and last_con point to the beginning and end of the list of initializing
constants for the array, and num_elems is the count of elements on that list.
*/
{
  a_variable_ptr var;
  a_constant_ptr aggr_con;

  /* Make a variable that is an array of exception type specification
     entries. */
  var = make_init_unnamed_local_static_array_var(
                                              make_exception_type_spec_type(),
                                              /*in_function_scope=*/FALSE,
                                              &aggr_con);
  aggr_con->variant.aggregate.first_constant = first_con;
  aggr_con->variant.aggregate.last_constant  = last_con;
  /* Set the array size. */
  var->type->variant.array.variant.number_of_elements = num_elems;
  /* Finish off the variable. */
  finish_array_var(var, aggr_con);
  return var;
}  /* make_exception_type_spec_array_var */


static void add_exception_type_spec_array_entry(a_type_ptr     type,
                                                a_constant_ptr *first_con,
                                                a_constant_ptr *last_con,
                                                unsigned long  *num_elems,
                                                a_boolean      last_entry)
/*
Add an entry that describes the type "type" to the array of exception type
specifications being built up as the initializer for a variable.  If type
is NULL, add an ellipsis entry.  *first_con and *last_con point to the
beginning and end of the list of constants for the array.  Increment
*num_elems.  last_entry is TRUE for the last entry in the array.
*/
{
  a_variable_ptr typeinfo_var, ptr_flags_var = NULL;
  an_eh_type_flags_set
                 flags_value;
  a_constant_ptr typeinfo_con, flags_con, sub_aggr_con;

  /* Each element of the array is an exception_type_spec struct
     containing a pointer to the typeinfo information, a flags byte,
     and possibly a pointer to an array of flags bytes.
     The flags byte indicates the cases where the type indicated is a
     reference or pointer to the typeinfo type. */
  /* Create an aggregate constant with three constants (a pointer to the
     typeinfo variable, the flags value, and the pointer to the
     flags array) under it. */
  typeinfo_con = alloc_constant((a_constant_repr_kind)ck_address);
  if (type == NULL) {
    /* This entry is for an ellipsis, so the typeinfo pointer is NULL. */
    make_zero_of_proper_type(make_ptr_to_const_typeinfo_type(),
                             typeinfo_con);
    flags_value = ETS_IS_ELLIPSIS;
  } else {
    /* Normal case. */
    typeinfo_var = typeinfo_var_for_type(type, &flags_value, &ptr_flags_var);
    set_variable_address_constant(typeinfo_var, typeinfo_con,
                                  /*set_address_taken_flag=*/TRUE);
#if IA64_ABI
    /* The typeinfo variable has a type derived from the
       implementation typeinfo type.  Cast the address constant to the
       appropriate type. */
    implicit_cast(typeinfo_con,
                  make_ptr_to_const_typeinfo_type());
#endif /* IA64_ABI */
  }  /* if */
  if (last_entry) flags_value |= ETS_LAST;
  flags_con = alloc_constant((a_constant_repr_kind)ck_integer);
  set_unsigned_integer_constant(flags_con, (a_host_large_unsigned)flags_value,
                                targ_ets_flag_type_int_kind);
  sub_aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
  sub_aggr_con->type = exception_type_spec_type;
  sub_aggr_con->variant.aggregate.first_constant = typeinfo_con;
  typeinfo_con->next = flags_con;
#if ABI_COMPATIBILITY_VERSION >= 241
  /* Add the address of the array giving cv-qualifiers for a multi-level
     pointer, if needed. */
  {
    a_constant_ptr ptr_flags_con =
                              alloc_constant((a_constant_repr_kind)ck_address);
    a_type_ptr     ptr_flags_type = make_pointer_type(
                                    integer_type(targ_ets_flag_type_int_kind));
    if (ptr_flags_var == NULL) {
      make_zero_of_proper_type(ptr_flags_type, ptr_flags_con);
    } else {
      set_variable_address_constant(ptr_flags_var, ptr_flags_con,
                                    /*set_address_taken_flag=*/TRUE);
      /* Do the array-to-pointer decay. */
      implicit_cast(ptr_flags_con, ptr_flags_type);
    }  /* if */
    flags_con->next = ptr_flags_con;
    sub_aggr_con->variant.aggregate.last_constant = ptr_flags_con;
  }
#else /* ABI_COMPATIBILITY_VERSION < 241 */
  /* Old version: no ptr_flags field. */
  sub_aggr_con->variant.aggregate.last_constant = flags_con;
#endif /* ABI_COMPATIBILITY_VERSION >= 241 */
  /* Add this aggregate constant to the list of constants under the aggregate
     constant for the array. */
  if (*first_con == NULL) {
    *first_con = sub_aggr_con;
  } else {
    check_assertion(last_con != NULL && *last_con != NULL);
    (*last_con)->next = sub_aggr_con;
  }  /* if */
  *last_con = sub_aggr_con;
  (*num_elems)++;
}  /* add_exception_type_spec_array_entry */


static a_variable_ptr exception_type_spec_array_from_throw_spec(
                                     an_exception_specification_ptr throw_spec)
/*
Make an array that describes the throw specification indicated by throw_spec,
and return a pointer to the variable for the array.  Return NULL if the
throw specification indicates that no types may be thrown.
*/
{
  a_variable_ptr                       var;
  an_exception_specification_type_ptr  espt;
  a_constant_ptr                       first_con, last_con;
  unsigned long                        num_elems = 0;

  check_assertion(!throw_spec->is_noexcept);
  espt = throw_spec->variant.exception_specification_type_list;
  /* If the routine can throw nothing, return NULL. */
  if (espt == NULL) {
    var = NULL;
  } else {
    /* There are some types on the throw list, so an array of those will
       have to be built. */
    first_con = last_con = NULL;
    /* Fill the array with entries for the types that can be thrown. */
    for (;
         espt != NULL;
         espt = espt->next) {
      add_exception_type_spec_array_entry(espt->type, &first_con, &last_con,
                                          &num_elems, (espt->next == NULL));
    }  /* for */
    /* Make the variable.  This is done late so that it appears on the
       variable list after any variables needed in the initializer (e.g.,
       ptr_flags arrays). */
    var = make_exception_type_spec_array_var(first_con, last_con, num_elems);
  }  /* if */
  return var;
}  /* exception_type_spec_array_from_throw_spec */


static a_variable_ptr make_catch_array_var(a_handler_ptr handlers)
/*
Generate an exception type specification array to describe the types of the
catch clauses on the indicated list.  Return a pointer to the variable.
*/
{
  a_variable_ptr var;
  a_handler_ptr  handler;
  a_constant_ptr first_con, last_con;
  unsigned long  num_elems = 0;

  first_con = last_con = NULL;
  /* Fill the array with entries for the catch clause types. */
  for (handler = handlers;
       handler != NULL;
       handler = handler->next) {
    a_type_ptr handler_type;
    if (handler->parameter == NULL) {
      /* NULL means an ellipsis catch, one that catches any type. */
      handler_type = NULL;
    } else {
      handler_type = handler->parameter->type;
    }  /* if */
    add_exception_type_spec_array_entry(handler_type, &first_con, &last_con,
                                        &num_elems, (handler->next == NULL));
  }  /* for */
  /* Make the variable.  This is done late so that it appears on the
     variable list after any variables needed in the initializer (e.g.,
     ptr_flags arrays). */
  var = make_exception_type_spec_array_var(first_con, last_con, num_elems);
  return var;
}  /* make_catch_array_var */


/*
Pointer to the jmp_buf type (used for setjmp/longjmp as part of exception
try/throw).  NULL until created.
*/
STATIC_THREAD a_type_ptr
		jmp_buf_type;


static a_type_ptr make_jmp_buf_type(void)
/*
Make the jmp_buf type (used for setjmp/longjmp as part of exception
try/throw) if it is not made already, and return a pointer to it.
*/
{
  if (jmp_buf_type == NULL) {
    /* jmp_buf is an array; that's part of the standard.  We assume it's
       an array of elements of some integral or floating type; that's not
       standard, but it's common.  For other cases, one can pick an
       integral kind and number of elements to give the right size and
       alignment. */
    jmp_buf_type = alloc_type((a_type_kind)tk_array);
    jmp_buf_type->variant.array.element_type =
                             targ_jmp_buf_elements_are_float ?
                                  float_type(targ_jmp_buf_element_float_kind) :
                                  integer_type(targ_jmp_buf_element_int_kind);
    jmp_buf_type->variant.array.variant.number_of_elements =
                                                     targ_jmp_buf_num_elements;
    set_type_size(jmp_buf_type);
  }  /* if */
  return jmp_buf_type;
}  /* make_jmp_buf_type */


/*
Pointer to the eh_stack_entry struct type (used to represent a stack frame
for exception processing).  NULL until created.  Also fields within that
type.
*/
STATIC_THREAD a_type_ptr
		eh_stack_entry_type;
STATIC_THREAD a_field_ptr
		ehse_next_field,
		ehse_kind_field,
		ehse_variant_field,
		ehse_try_field,
		ehse_try_setjmp_buffer_field,
		ehse_try_catch_entries_field,
		ehse_try_rtinfo_field,
		ehse_try_region_number_field,
		ehse_function_field,
		ehse_function_regions_field,
		ehse_function_obj_table_field,
		ehse_function_array_table_field,
		ehse_function_saved_region_number_field,
		ehse_throw_spec_field;

/*
Values for the "kind" field of eh_stack_entry.  This must match the runtime's
definition of these values.
*/
enum an_eh_stack_entry_kind {
  ehsek_old_try_block,	/* Used for a try block up to version 3.10. */
  ehsek_function,
  ehsek_throw_spec,
  /*lint -esym(749,*ehsek_throw_processing_marker)*/
  ehsek_throw_processing_marker,	/* Used by runtime. */
  /*lint -esym(749,*ehsek_vec_new_or_delete)*/
  ehsek_vec_new_or_delete,		/* Used by runtime. */
#if ABI_COMPATIBILITY_VERSION <= 310
  ehsek_try_block = ehsek_old_try_block,
#else /* ABI_COMPATIBILITY_VERSION > 310 */
  ehsek_try_block,
  /*lint -esym(749,*ehsek_old_try_block)*/
#endif /* ABI_COMPATIBILITY_VERSION <= 310 */
  ehsek_noexcept
};


static a_type_ptr make_eh_stack_entry_type(void)
/*
Make the eh_stack_entry struct type (used to represent a stack frame for
exception processing) if it is not made already, and return a pointer to it.
Its definition is

  struct eh_stack_entry {
    eh_stack_entry *next;  // Next stack frame
    unsigned char  kind    // try, function, or throw
    union {
      struct {
        jmp_buf        setjmp_buffer;       // Buffer for setjmp
        exception_type_spec *catch_entries; // Catch list
        void           *rtinfo;             // Runtime info
        unsigned short region_number;       // Region number at entry
      } try_block;
      struct {
        region_descr   *regions;            // Cleanup regions
        void           **obj_table;         // Object address table
        array_descr    *array_table;        // Array table
        unsigned short saved_region_number; // Saved __eh_curr_region
      } function;
      exception_type_spec *throw_spec;      // Throw spec list
    } variant;
  };

*/
{
  a_field_ptr   last_field;
  a_type_ptr    try_block_struct_type, function_struct_type;
  a_type_ptr    variant_union_type, ptr_exception_type_spec;

  if (eh_stack_entry_type == NULL) {
    /* Make the class types (without defining them) to get them on the
       types list in the right order (they get added to the front so they
       end up in reverse order of insertion). */
    /* Make the eh_stack_entry struct type. */
    eh_stack_entry_type = make_lowered_class_type((a_type_kind)tk_struct);
    add_to_front_of_file_scope_types_list(eh_stack_entry_type);
    /* Make the variant union type. */
    variant_union_type = make_lowered_class_type((a_type_kind)tk_union);
    add_to_front_of_file_scope_types_list(variant_union_type);
    /* Make the try_block variant struct. */
    try_block_struct_type = make_lowered_class_type((a_type_kind)tk_struct);
    add_to_front_of_file_scope_types_list(try_block_struct_type);
    last_field = NULL;
    /* field: jmp_buf setjmp_buffer */
    make_lowered_field("setjmp_buffer", make_jmp_buf_type(),
                       try_block_struct_type, &last_field);
    ehse_try_setjmp_buffer_field = last_field;
    /* field: exception_type_spec *catch_entries */
    ptr_exception_type_spec =
                            make_pointer_type(make_exception_type_spec_type());
    make_lowered_field("catch_entries", ptr_exception_type_spec,
                       try_block_struct_type, &last_field);
    ehse_try_catch_entries_field = last_field;
    /* field: void *rtinfo */
    make_lowered_field("rtinfo", void_star_type(),
                       try_block_struct_type, &last_field);
    ehse_try_rtinfo_field = last_field;
    /* field: unsigned short region_number */
    make_lowered_field("region_number",
                       integer_type(targ_region_number_int_kind),
                       try_block_struct_type, &last_field);
    ehse_try_region_number_field = last_field;
    finish_class_type(try_block_struct_type);
    /* Make the function variant struct. */
    function_struct_type = make_lowered_class_type((a_type_kind)tk_struct);
    add_to_front_of_file_scope_types_list(function_struct_type);
    last_field = NULL;
    /* field: region_descr *regions */
    make_lowered_field("regions",
                       make_pointer_type(make_region_descr_type()),
                       function_struct_type, &last_field);
    ehse_function_regions_field = last_field;
    /* field: void **obj_table */
    make_lowered_field("obj_table",
                       make_pointer_type(void_star_type()),
                       function_struct_type, &last_field);
    ehse_function_obj_table_field = last_field;
    /* field: array_descr *array_table */
    make_lowered_field("array_table",
                       make_pointer_type(make_array_descr_type()),
                       function_struct_type, &last_field);
    ehse_function_array_table_field = last_field;
    /* field: unsigned short saved_region_number */
    make_lowered_field("saved_region_number",
                       integer_type(targ_region_number_int_kind),
                       function_struct_type, &last_field);
    ehse_function_saved_region_number_field = last_field;
    finish_class_type(function_struct_type);
    /* Define the variant union type. */
    last_field = NULL;
    /* field: struct {...} try_block */
    make_lowered_field("try_block", try_block_struct_type,
                       variant_union_type, &last_field);
    ehse_try_field = last_field;
    /* field: struct {...} function */
    make_lowered_field("function", function_struct_type,
                       variant_union_type, &last_field);
    ehse_function_field = last_field;
    /* field: exception_type_spec *throw_spec */
    make_lowered_field("throw_spec", ptr_exception_type_spec,
                       variant_union_type, &last_field);
    ehse_throw_spec_field = last_field;
    finish_class_type(variant_union_type);
    /* Define the eh_stack_entry struct type. */
    last_field = NULL;
    /* field: eh_stack_entry *next */
    make_lowered_field("next", make_pointer_type(eh_stack_entry_type),
                       eh_stack_entry_type, &last_field);
    ehse_next_field = last_field;
    /* field: unsigned char kind */
    make_lowered_field("kind",
                       integer_type((an_integer_kind)ik_unsigned_char),
                       eh_stack_entry_type, &last_field);
    ehse_kind_field = last_field;
    /* field: union {...} variant */
    make_lowered_field("variant", variant_union_type,
                       eh_stack_entry_type, &last_field);
    ehse_variant_field = last_field;
    finish_class_type(eh_stack_entry_type);
  }  /* if */
  return eh_stack_entry_type;
}  /* make_eh_stack_entry_type */


static a_variable_ptr make_curr_eh_stack_entry_var(void)
/*
Make __curr_eh_stack_entry, a global variable used for exception processing,
if it has not already been made.  Return a pointer to it.
*/
{
  if (curr_eh_stack_entry_var == NULL) {
    curr_eh_stack_entry_var =
           make_lowered_variable("__curr_eh_stack_entry",
                                 /*already_il_name=*/FALSE,
                                 make_pointer_type(make_eh_stack_entry_type()),
                                 (a_storage_class)sc_extern);
  }  /* if */
  return curr_eh_stack_entry_var;
}  /* make_curr_eh_stack_entry_var */


static void push_eh_stack_frame(an_eh_stack_entry_kind kind,
                                a_variable_ptr         *stack_frame_var,
                                an_insert_location     *insert_location)
/*
Generate code to push an exception handling stack frame on the stack.
A local temporary variable is created to hold the stack frame; a pointer to
that variable is returned in *stack_frame_var.  The invariant fields of
the stack frame are set, and the kind is set to "kind".  The code is
inserted at *insert_location and *insert_location is updated to allow
the caller to do insertion after the code inserted.
*/
{
  a_variable_ptr   local_frame;
  an_expr_node_ptr local_frame_next, local_frame_kind;

  /* Create the local variable for the stack frame. */
  *stack_frame_var = local_frame =
                            make_lowered_temporary(make_eh_stack_entry_type());
  /* Add code as follows:
       local_frame.next = __curr_eh_stack_entry;
       __curr_eh_stack_entry = &local_frame;
       local_frame.kind = kind;
  */
  local_frame_next = field_lvalue_selection_expr(var_lvalue_expr(local_frame),
                                                 ehse_next_field);
  (void)insert_assignment_statement(
                              local_frame_next,
                              (an_expr_operator_kind)eok_assign,
                              var_rvalue_expr(make_curr_eh_stack_entry_var()),
                              insert_location);
  (void)insert_var_assignment_statement(curr_eh_stack_entry_var,
                                        var_addr_expr(local_frame),
                                        insert_location);
  local_frame_kind = field_lvalue_selection_expr(var_lvalue_expr(local_frame),
                                                 ehse_kind_field);
  (void)insert_assignment_statement(
                              local_frame_kind,
                              (an_expr_operator_kind)eok_assign,
                              node_for_integer_constant((long)kind,
                                            (an_integer_kind)ik_unsigned_char),
                              insert_location);
}  /* push_eh_stack_frame */


static void pop_eh_stack_frame(an_eh_stack_entry_kind kind,
                               a_variable_ptr         stack_frame,
                               an_insert_location     *insert_location)
/*
Generate code to pop an exception handling stack frame off the stack.
kind indicates the kind of stack frame.  *stack_frame points to a
local temporary variable that holds the stack frame.  The code is
inserted at *insert_location.
*/
{
  an_expr_node_ptr local_frame_next, stack_frame_function_saved_region_number;

  /* If this is a function stack frame, restore __eh_curr_region from
     the stack frame. */
  if (kind == ehsek_function) {
    stack_frame_function_saved_region_number = 
                  field_rvalue_selection_expr(
                    field_lvalue_selection_expr(
                      field_lvalue_selection_expr(var_lvalue_expr(stack_frame),
                                                  ehse_variant_field),
                      ehse_function_field),
                    ehse_function_saved_region_number_field);
    /* Copy stack_frame.variant.function.saved_region_number into
       the global variable __eh_curr_region. */
    assign_to_eh_curr_region(stack_frame_function_saved_region_number,
                             insert_location);
  }  /* if */
  /* Add __curr_eh_stack_entry = local_frame.next; */
  local_frame_next = field_rvalue_selection_expr(var_lvalue_expr(stack_frame),
                                                 ehse_next_field);
  (void)insert_var_assignment_statement(curr_eh_stack_entry_var,
                                        local_frame_next,
                                        insert_location);
}  /* pop_eh_stack_frame */

#endif /* DO_FULL_PORTABLE_EH_LOWERING */

a_boolean has_destructions(an_object_lifetime_ptr lifetime)
/*
Returns TRUE if the specified lifetime (which can be NULL) or any of its
sub-lifetimes contain at least one destruction.  Object lifetimes without
destructions should only occur when
LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS is TRUE.
*/
{
  an_object_lifetime_ptr  olp;
  a_boolean               result = FALSE;

  if (lifetime != NULL) {
    if (lifetime->destructions != NULL) {
      result = TRUE;
    } else {
      for (olp = lifetime->child_lifetime; olp != NULL; olp = olp->next) {
        if (has_destructions(olp)) {
          result = TRUE;
          break;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  return result;
}  /* has_destructions */


void add_eh_function_prologue(a_scope_ptr scope)
/*
Add any prologue needed for exception handling to the function whose scope
is given by "scope".  Called only if exceptions are enabled.  This is
done late so that it gets inserted before any code inserted at the
beginning of the function.  Epilogue code is also inserted at each return
statement if necessary.
*/
{
  a_routine_ptr             routine;
  an_insert_location        insert_location;
  a_boolean                 need_function_epilogue = FALSE;
  a_return_memo_ptr         rmp;
  a_source_position         saved_error_position, saved_code_pos;
#if DO_FULL_PORTABLE_EH_LOWERING
  a_type_ptr                routine_type, spec_array_ptr;
  an_exception_specification_ptr
                            tsp;
  a_variable_ptr            throw_frame = NULL, func_frame = NULL,
                            spec_array_var;
  an_expr_node_ptr          spec_array_node, throw_frame_throw_spec;
  an_expr_node_ptr          func_frame_function_regions;
  an_expr_node_ptr          func_frame_function_obj_table;
  an_expr_node_ptr          func_frame_function_array_table;
  an_expr_node_ptr          func_frame_function_saved_region_number;
  a_boolean                 need_throw_epilogue = FALSE;
  an_eh_stack_entry_kind    eh_stack_kind = ehsek_old_try_block;
#endif /* DO_FULL_PORTABLE_EH_LOWERING */

  saved_code_pos = code_pos_for_lowering;
  saved_error_position = error_position;
  /* Set the current position to the opening brace of the function. */
  code_pos_for_lowering = scope->assoc_block->position;
  error_position = code_pos_for_lowering;
  /* The insert location for the statements is the start of the top block of
     the routine. */
  set_block_start_insert_location(scope->assoc_block, &insert_location);
  routine = scope->variant.routine.ptr;
#if DO_FULL_PORTABLE_EH_LOWERING
  routine_type = routine->type;
  routine_type = skip_typerefs(routine_type);
  /* See if the routine has a throw specification. */
  tsp = routine_type->variant.routine.extra_info->exception_specification;
  check_assertion(tsp == NULL || !tsp->indeterminate);
  if (building_runtime) {
    /* In the case where we're building the runtime library, don't take
       action on the exception specification (if any).  That's to prevent
       problems when the runtime library explicitly calls std::terminate(). */
  } else if (tsp != NULL && !tsp->throw_any) {
    /* The routine has an exception specification that the runtime library
       needs to know about.  (A null pointer means the function can throw
       anything.)  The noexcept(false) case is treated similarly (as is
       the Microsoft-specific throw(...)). */
#if ABI_COMPATIBILITY_VERSION >= 405
    if (tsp->is_noexcept) {
      /* The exception specification specifies either "noexcept" or
         "noexcept(expr)" where expr evaluates to true.  Push an entry
         on the EH stack (there is no data to pass, just the presence of
         the stack frame is enough). */
      eh_stack_kind = ehsek_noexcept;
      push_eh_stack_frame(eh_stack_kind, &throw_frame, &insert_location);
    } else
#endif /* ABI_COMPATIBILITY_VERSION >= 405 */
    /* Do not insert code here. */
    {
      /* Generate code to push an entry on the EH stack. */
      eh_stack_kind = ehsek_throw_spec;
      push_eh_stack_frame(eh_stack_kind, &throw_frame, &insert_location);
#if ABI_COMPATIBILITY_VERSION < 405
      if (tsp->is_noexcept) {
        /* For older runtime libraries that don't recognize the ehsek_noexcept
           EH stack entry, approximate it by using a ehsek_throw_spec
           EH stack entry with no types (i.e., like a throw()). */
        spec_array_var = NULL;
      } else
#endif /* ABI_COMPATIBILITY_VERSION < 405 */
      /* Do not insert code here. */
      {
        /* Build an array of the throw types. */
        spec_array_var = exception_type_spec_array_from_throw_spec(tsp);
      }  /* if */
      /* Generate code to set the throw_spec field of the stack entry to
         point to the array (or NULL if no types can be thrown). */
      spec_array_ptr = make_pointer_type(make_exception_type_spec_type());
      if (spec_array_var == NULL) {
        /* No types can be thrown, so use a NULL pointer. */
        a_constant_ptr null_constant = local_constant();
        make_zero_of_proper_type(spec_array_ptr, null_constant);
        spec_array_node = alloc_node_for_constant(null_constant);
        release_local_constant(&null_constant);
      } else {
        /* Use the address of the first element of the array. */
        spec_array_node = array_first_element_addr_expr(spec_array_var);
      }  /* if */
      /* Make an expression for throw_frame.variant.throw_spec */
      throw_frame_throw_spec = 
                    field_lvalue_selection_expr(
                      field_lvalue_selection_expr(var_lvalue_expr(throw_frame),
                                                  ehse_variant_field),
                      ehse_throw_spec_field);
      /* Assign the array address to throw_frame.variant.throw_spec */
      (void)insert_assignment_statement(throw_frame_throw_spec,
                                        (an_expr_operator_kind)eok_assign,
                                        spec_array_node, &insert_location);
    }  /* if */
    need_throw_epilogue = TRUE;
  }  /* if */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
  if (has_destructions(scope->lifetime) || routine->contains_try_block) {
    /* The function contains destructible objects, or it contains try
       blocks, so it needs a prologue and epilogue. */
    need_function_epilogue = TRUE;
#if GENERATE_EH_TABLES
  } else if (region_table_var != NULL ||
             array_table_var != NULL
#if DO_FULL_PORTABLE_EH_LOWERING
             || object_addr_table_var != NULL
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
                                             ) {
    /* Some other routines generated by lowering also need the prologue
       and epilogue. */
    need_function_epilogue = TRUE;
#endif /* GENERATE_EH_TABLES */
  }  /* if */
  if (need_function_epilogue) {
    /* We need the prologue and epilogue. */
#if GENERATE_EH_TABLES
    /* Finish off the various arrays. */
    if (region_table_var != NULL) {
      finish_array_var(region_table_var, region_table_aggr_con);
    }  /* if */
    if (array_table_var != NULL) {
      finish_array_var(array_table_var, array_table_aggr_con);
    }  /* if */
#endif /* GENERATE_EH_TABLES */
#if DO_FULL_PORTABLE_EH_LOWERING
    /* Generate code to push an entry on the EH stack. */
    push_eh_stack_frame(ehsek_function, &func_frame, &insert_location);
    /* Put pointers to the various arrays into the stack. */
    if (region_table_var != NULL) {
      /* Make an expression for throw_frame.variant.function.regions */
      func_frame_function_regions = 
                  field_lvalue_selection_expr(
                    field_lvalue_selection_expr(
                      field_lvalue_selection_expr(var_lvalue_expr(func_frame),
                                                  ehse_variant_field),
                      ehse_function_field),
                    ehse_function_regions_field);
      /* Assign the region table address to
         func_frame.variant.function.regions */
      (void)insert_assignment_statement(func_frame_function_regions,
                                        (an_expr_operator_kind)eok_assign,
                                        array_first_element_addr_expr(
                                                             region_table_var),
                                        &insert_location);
    }  /* if */
    if (object_addr_table_var != NULL) {
      finish_array_var(object_addr_table_var, (a_constant_ptr)NULL);
      /* Make an expression for throw_frame.variant.function.obj_table */
      func_frame_function_obj_table = 
                  field_lvalue_selection_expr(
                    field_lvalue_selection_expr(
                      field_lvalue_selection_expr(var_lvalue_expr(func_frame),
                                                  ehse_variant_field),
                      ehse_function_field),
                    ehse_function_obj_table_field);
      /* Assign the object address table address to
         func_frame.variant.function.obj_table */
      (void)insert_assignment_statement(func_frame_function_obj_table,
                                        (an_expr_operator_kind)eok_assign,
                                        array_first_element_addr_expr(
                                                        object_addr_table_var),
                                        &insert_location);
    }  /* if */
    if (array_table_var != NULL) {
      /* Make an expression for throw_frame.variant.function.array_table */
      func_frame_function_array_table = 
                  field_lvalue_selection_expr(
                    field_lvalue_selection_expr(
                      field_lvalue_selection_expr(var_lvalue_expr(func_frame),
                                                  ehse_variant_field),
                      ehse_function_field),
                    ehse_function_array_table_field);
      /* Assign the object address table address to
         func_frame.variant.function.array_table */
      (void)insert_assignment_statement(func_frame_function_array_table,
                                        (an_expr_operator_kind)eok_assign,
                                        array_first_element_addr_expr(
                                                              array_table_var),
                                        &insert_location);
    }  /* if */
    /* Generate an assignment to save __eh_curr_region in the stack. */
    /* Make an expression for
       throw_frame.variant.function.saved_region_number */
    func_frame_function_saved_region_number = 
                  field_lvalue_selection_expr(
                    field_lvalue_selection_expr(
                      field_lvalue_selection_expr(var_lvalue_expr(func_frame),
                                                  ehse_variant_field),
                      ehse_function_field),
                    ehse_function_saved_region_number_field);
    /* Copy the global variable __eh_curr_region into
       func_frame.variant.function.saved_region_number */
    (void)insert_assignment_statement(func_frame_function_saved_region_number,
                                      (an_expr_operator_kind)eok_assign,
                                      var_rvalue_expr(
                                                    make_eh_curr_region_var()),
                                      &insert_location);
    /* Reset __eh_curr_region to null_eh_region_number. */
    (void)insert_var_assignment_statement(eh_curr_region_var,
                                          node_for_integer_constant(
                                                 (long)null_eh_region_number,
                                                 targ_region_number_int_kind),
                                          &insert_location);
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
    /* Non-portable schemes: generate an enk_lowered_eh_construct/
       leck_function_prologue expression node. */
    { an_expr_node_ptr node = alloc_lowered_eh_construct_node(
                          (a_lowered_eh_construct_kind)leck_function_prologue);
      an_eh_prologue_supplement_ptr psp =
                                node->variant.lowered_eh.variant.prologue_info;
      psp->routine = routine;
#if GENERATE_EH_TABLES
      psp->region_table = region_table_var;
      psp->array_table = array_table_var;
#endif /* GENERATE_EH_TABLES */
      (void)insert_expr_statement(node, &insert_location);
    }
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
  }  /* if */
  if (need_function_epilogue
#if DO_FULL_PORTABLE_EH_LOWERING
      || need_throw_epilogue
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
                            ) {
    /* Need to add epilogue code at each return in the routine. */
    for (rmp = return_memo_list; rmp != NULL; rmp = rmp->next) {
      /* Turn the return into a block. */
      turn_branch_into_block(rmp->stmt, &insert_location, &rmp->stmt);
      if (need_function_epilogue) {
#if DO_FULL_PORTABLE_EH_LOWERING
        /* Insert code to pop the prologue pushed for the function. */
        pop_eh_stack_frame(ehsek_function, func_frame, &insert_location);
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
        /* Insert an enk_lowered_eh_construct node to represent the
           function epilogue. */
        an_expr_node_ptr node = alloc_lowered_eh_construct_node(
                          (a_lowered_eh_construct_kind)leck_function_epilogue);
        node->variant.lowered_eh.variant.epilogue_routine = routine;
        (void)insert_expr_statement(node, &insert_location);
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
      }  /* if */
#if DO_FULL_PORTABLE_EH_LOWERING
      if (need_throw_epilogue) {
        /* Insert code to pop the prologue pushed for the throw or noexcept
           specification. */
        pop_eh_stack_frame(eh_stack_kind, throw_frame, &insert_location);
      }  /* if */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
    }  /* for */
  }  /* if */
  error_position = saved_error_position;
  code_pos_for_lowering = saved_code_pos;
}  /* add_eh_function_prologue */

#if !DO_FULL_PORTABLE_EH_LOWERING

static a_handler_ptr current_catch_handler(void)
/*
Find and return the address of the handler for the current catch clause.
*/
{
  a_context_ptr context;
  a_handler_ptr handler = NULL;

  for (context = curr_context; context != NULL; context = context->parent) {
    a_scope_ptr scope = context->scope;
    if (scope->kind == (a_scope_kind)sck_block) {
      handler = scope->variant.assoc_handler;
      if (handler != NULL) break;
    }  /* if */
  }  /* for */
  check_assertion(handler != NULL);
  return handler;
}  /* current_catch_handler */

#endif /* !DO_FULL_PORTABLE_EH_LOWERING */

an_expr_node_ptr make_caught_object_address_node(void)
/*
Make an rvalue expression node for the address of the object caught at the
currently active catch clause, and return a pointer to the node.
*/
{
  an_expr_node_ptr source_node;
#if DO_FULL_PORTABLE_EH_LOWERING
  /* In the portable scheme, use the global variable
     __caught_object_address. */
  a_variable_ptr   caught_object_addr = make_caught_object_address_var();

  source_node = var_rvalue_expr(caught_object_addr);
#else /* DO_FULL_PORTABLE_EH_LOWERING */
  /* In other schemes, use an enk_lowered_eh_construct/
     leck_caught_object_address expression node. */
  source_node = alloc_lowered_eh_construct_node(
                      (a_lowered_eh_construct_kind)leck_caught_object_address);
  source_node->type = void_star_type();
  source_node->variant.lowered_eh.variant.caught_object_handler =
                                                       current_catch_handler();
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
  return source_node;
}  /* make_caught_object_address_node */


#if DO_FULL_PORTABLE_EH_LOWERING
#if ABI_COMPATIBILITY_VERSION >= 233
/*
Pointer to the routine entry for the runtime routine __exception_caught.
NULL until created.
*/
STATIC_THREAD a_routine_ptr
		exception_caught_routine;
#endif /* ABI_COMPATIBILITY_VERSION >= 233 */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */


void begin_catch_clause(a_handler_ptr handler)
/*
Generate code for the start of a catch clause.  The current context is
for the scope of the handler.
*/
{
  an_init_pos_descr       ipd;
  an_insert_location      insert_location;
  an_implied_copy_source  source_desc;

  set_block_start_insert_location(handler->statement, &insert_location);
#if GENERATE_EH_TABLES
#if ABI_COMPATIBILITY_VERSION > 310
  /* Add a cleanup entry for the exception object allocated by the runtime.
     This is used to ensure that the exception object is deleted at the right
     point (after the catch clause parameter) when an exception is thrown
     (not rethrown) from within the catch clause. */
  add_runtime_exception_object_cleanup(&insert_location);
#endif /* ABI_COMPATIBILITY_VERSION > 310 */
#endif /* GENERATE_EH_TABLES */
  if (handler->parameter != NULL) {
    /* Insert code to initialize the catch clause parameter from the
       runtime copy of the thrown object. */
    set_var_init_pos_descr(handler->parameter, &ipd);
    /* Indicate that the source is a copy of the object thrown by an exception
       handling "throw". */
    clear_implied_copy_source(&source_desc);
    source_desc.runtime_throw = TRUE;
    lower_dynamic_init(handler->dynamic_init, &ipd,
                       &source_desc,
                       (a_variable_ptr)NULL,
                       LDIO_FULL_EXPR,
                       /*others_follow_in_aggr=*/FALSE,
                       &insert_location, (a_boolean *)NULL,
                       (a_constant **)NULL);
    /* Mark the parameter as referenced. */
    handler->parameter->source_corresp.referenced = TRUE;
  }  /* if */
#if ABI_COMPATIBILITY_VERSION >= 233
  /* Mark the point where the catch entry processing is finished, and the
     exception can be considered caught. */
#if DO_FULL_PORTABLE_EH_LOWERING
  /* Portable scheme: */
  /* Make a call of the runtime routine __exception_caught. */
  make_call_statement(make_prototyped_runtime_routine("__exception_caught",
                                                     &exception_caught_routine,
                                                     void_type(), NULL,
                                                     NULL, NULL, NULL, NULL,
                                                     NULL, NULL),
                      (an_expr_node_ptr)NULL,
                      (an_expr_node_ptr)NULL,
                      &insert_location);
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
  /* In other schemes, insert an enk_lowered_eh_construct/
     leck_exception_caught expression node. */
  { an_expr_node_ptr node = alloc_lowered_eh_construct_node(
                           (a_lowered_eh_construct_kind)leck_exception_caught);
    (void)insert_expr_statement(node, &insert_location);
  }
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
#endif /* ABI_COMPATIBILITY_VERSION >= 233 */
}  /* begin_catch_clause */


void cleanup_on_exit_from_try_block(
                              ARG_UNUSED a_context_ptr        context_ptr,
                              ARG_UNUSED a_try_supplement_ptr try_block,
                              an_insert_location              *insert_location)
/*
Generate any cleanup required on exit from a try block.  context_ptr points to
the context for the try block.  Any code generated is inserted at
*insert_location.
*/
{
#if DO_FULL_PORTABLE_EH_LOWERING
  /* In the portable scheme, pop the stack frame for the try block. */
  pop_eh_stack_frame(ehsek_try_block, context_ptr->try_frame, insert_location);
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
  /* In other schemes, insert an enk_lowered_eh_construct/leck_try_epilogue
     expression node. */
  an_expr_node_ptr node = alloc_lowered_eh_construct_node(
                               (a_lowered_eh_construct_kind)leck_try_epilogue);
  node->variant.lowered_eh.variant.epilogue_try_block = try_block;
  (void)insert_expr_statement(node, insert_location);
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
}  /* cleanup_on_exit_from_try_block */


#if DO_FULL_PORTABLE_EH_LOWERING
/*
Pointer to the routine entry for the runtime routine __free_thrown_object.
NULL until created.
*/
STATIC_THREAD a_routine_ptr
		free_thrown_object_routine;
#endif /* DO_FULL_PORTABLE_EH_LOWERING */


void cleanup_on_exit_from_catch(ARG_UNUSED a_handler_ptr handler,
                                an_insert_location       *insert_location)
/*
Generate any cleanup required on exit from a catch clause.  Any code
generated is inserted at insert_location.
*/
{
#if DO_FULL_PORTABLE_EH_LOWERING
  /* Portable scheme: */
  /* Make a call of the runtime that tells the runtime it can now
     destroy the caught object and free the space for it. */
#if ABI_COMPATIBILITY_VERSION > 310
  make_call_statement(make_destroy_exception_object_routine(),
                      (an_expr_node_ptr)NULL,
                      (an_expr_node_ptr)NULL,
                      insert_location);
#else /* ABI_COMPATIBILITY_VERSION <= 310 */
  make_call_statement(make_prototyped_runtime_routine("__free_thrown_object",
                                                   &free_thrown_object_routine,
                                                   void_type(), NULL,
                                                   NULL, NULL, NULL, NULL,
                                                   NULL, NULL),
                      (an_expr_node_ptr)NULL,
                      (an_expr_node_ptr)NULL,
                      insert_location);
#endif /* ABI_COMPATIBILITY_VERSION > 310 */
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
  /* In other schemes, insert an enk_lowered_eh_construct/leck_catch_epilogue
     expression node. */
  an_expr_node_ptr node = alloc_lowered_eh_construct_node(
                             (a_lowered_eh_construct_kind)leck_catch_epilogue);
  node->variant.lowered_eh.variant.epilogue_handler = handler;
  (void)insert_expr_statement(node, insert_location);
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
}  /* cleanup_on_exit_from_catch */

#if DO_FULL_PORTABLE_EH_LOWERING
#if FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS

static void add_var_addr_to_list_if_modified_in_try_block(
                                                    a_variable_ptr   var,
                                                    an_expr_node_ptr *arg_list)
/*
If the indicated variable is modified in a try block, add an expression node
for the address of the variable at the front of the indicated expression list.
This is used in a call that convinces optimizers that these variables
must be stored out when modified.
*/
{
  an_expr_node_ptr arg;

  if (var->modified_within_try_block) {
    check_assertion_str(var->source_corresp.referenced,
    "add_var_addr_to_list_if_modified_in_try_block: referenced flag is FALSE");
    if (var->init_kind == (an_init_kind)initk_binding) {
      /* Structured binding "variables" have an expression that represents the
         variable; copy and lower the expression. */
      arg = copy_expr_tree(var->initializer.bound_expr, CE_NO_OPTIONS);
      lower_expr(arg);
      arg = add_address_of_to_node(arg);
    } else {
      arg = var_addr_expr(var);
    }  /* if */
    arg->next = *arg_list;
    *arg_list = arg;
  }  /* if */
}  /* add_var_addr_to_list_if_modified_in_try_block */

#endif /* FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */

#if DO_FULL_PORTABLE_EH_LOWERING
/*
Pointers to the routine entries for the runtime routines setjmp and 
__suppress_optim_on_vars_in_try.  NULL until created.
*/
STATIC_THREAD a_routine_ptr
		setjmp_routine,
		suppress_optim_on_vars_in_try_routine;


a_boolean is_eh_setjmp_routine(a_routine_ptr routine)
/*
Return TRUE if routine is the runtime routine setjmp called by the lowered
code for try blocks.
*/
{
  return routine != NULL && routine == setjmp_routine;
}  /* is_eh_setjmp_routine */

#endif /* DO_FULL_PORTABLE_EH_LOWERING */


#if DO_FULL_PORTABLE_EH_LOWERING

static void initialize_eh_stack_entry_for_try(
                                       a_variable_ptr     try_frame,
                                       a_variable_ptr     catch_array_var,
                                       an_insert_location *insert_location,
                                       an_expr_node_ptr   *setjmp_compare_node)
/*
Generate code to initialize the fields specific to a try block in the
EH stack entry identified by the variable try_frame.  catch_array_var is
the variable containing the catch information, or NULL for an internal
try block.  Code is inserted at *insert_location, and *insert_location
is updated accordingly.  An expression that does a setjmp call compared
with zero is built, and a pointer to it is returned in *setjmp_compare_node.
*/
{
  an_expr_node_ptr   setjmp_call;
  an_expr_node_ptr   try_frame_catch_entries, try_frame_setjmp_buffer;
  an_expr_node_ptr   try_frame_rtinfo, try_frame_region_number;
  a_constant_ptr     null_constant = local_constant();

  /* Put the address of the catch types description array into the stack
     frame. */
  try_frame_catch_entries = 
                  field_lvalue_selection_expr(
                    field_lvalue_selection_expr(
                      field_lvalue_selection_expr(var_lvalue_expr(try_frame),
                                                  ehse_variant_field),
                      ehse_try_field),
                    ehse_try_catch_entries_field);
  if (catch_array_var != NULL) {
    (void)insert_assignment_statement(try_frame_catch_entries,
                                      (an_expr_operator_kind)eok_assign,
                                      array_first_element_addr_expr(
                                                              catch_array_var),
                                      insert_location);
  } else {
    /* Internal try -- no catch_array_var. */
    make_zero_of_proper_type(ehse_try_catch_entries_field->type,
                             null_constant);
    (void)insert_assignment_statement(try_frame_catch_entries,
                                      (an_expr_operator_kind)eok_assign,
                                      alloc_node_for_constant(null_constant),
                                      insert_location);
  }  /* if */
  /* Set the rtinfo field (which points to runtime information) to NULL. */
  try_frame_rtinfo = 
                  field_lvalue_selection_expr(
                    field_lvalue_selection_expr(
                      field_lvalue_selection_expr(var_lvalue_expr(try_frame),
                                                  ehse_variant_field),
                      ehse_try_field),
                    ehse_try_rtinfo_field);
  make_zero_of_proper_type(ehse_try_rtinfo_field->type, null_constant);
  (void)insert_assignment_statement(try_frame_rtinfo,
                                    (an_expr_operator_kind)eok_assign,
                                    alloc_node_for_constant(null_constant),
                                    insert_location);
  /* Set the region_number field to the region number at entry to the try
     block.  This tells the runtime where to stop the cleanup process to
     end the "try" but not things in the surrounding function. */
  try_frame_region_number = 
                  field_lvalue_selection_expr(
                    field_lvalue_selection_expr(
                      field_lvalue_selection_expr(var_lvalue_expr(try_frame),
                                                  ehse_variant_field),
                      ehse_try_field),
                    ehse_try_region_number_field);
  (void)insert_assignment_statement(try_frame_region_number,
                                    (an_expr_operator_kind)eok_assign,
                                    var_rvalue_expr(make_eh_curr_region_var()),
                                    insert_location);
  /* Change the original stmk_try_block statement into an if statement
     that looks like
       if (setjmp(try_frame.variant.try_block.setjmp_buffer) == 0) ...
  */
  /* Make try_frame.variant.try_block.setjmp_buffer. */
  try_frame_setjmp_buffer = 
                    field_lvalue_selection_expr(
                      field_lvalue_selection_expr(
                        field_lvalue_selection_expr(var_lvalue_expr(try_frame),
                                                    ehse_variant_field),
                        ehse_try_field),
                      ehse_try_setjmp_buffer_field);
  /* Perform an array decay on the argument. */
  try_frame_setjmp_buffer = make_array_to_pointer_node(
                                                      try_frame_setjmp_buffer);
  /* Make the setjmp call. */
  setjmp_call = make_prototyped_runtime_call(targ_setjmp_func, &setjmp_routine,
                                       integer_type((an_integer_kind)ik_int),
                                       make_jmp_buf_type(), NULL,
                                       try_frame_setjmp_buffer);
  /* Generate the comparison against zero. */
  setjmp_call->next = node_for_integer_constant(0L, (an_integer_kind)ik_int);
  *setjmp_compare_node = make_operator_node((an_expr_operator_kind)eok_eq,
                                            setjmp_call->type, setjmp_call);
  release_local_constant(&null_constant);
#if GNU_EXTENSIONS_ALLOWED && BACK_END_IS_C_GEN_BE && \
    GCC_IS_GENERATED_CODE_TARGET
  check_assertion(innermost_function_scope != NULL);
  if (innermost_function_scope->variant.routine.ptr->always_inline) {
    /* This function has the GCC always_inline attribute and we've just added
       a call to setjmp in the lowered code.  A back end GCC will fail to
       compile this code as the presence of setjmp prohibits inlining.
       Remove the always_inline attribute (and issue a remark). */
    an_attribute_ptr ap, prev = NULL;
    a_routine_ptr    rp = innermost_function_scope->variant.routine.ptr;
    for (ap = rp->source_corresp.attributes;
         ap != NULL;
         ap = ap->next) {
      if (ap->kind == ak_always_inline) {
        rp->always_inline = FALSE;
        pos_remark(ec_always_inline_suppressed, &ap->position);
        if (prev == NULL) {
          rp->source_corresp.attributes = ap->next;
        } else {
          prev->next = ap->next;
        }  /* if */
      }  /* if */
      prev = ap;
    }  /* for */
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED && BACK_END_IS_C_GEN_BE && GCC_IS_... */
}  /* initialize_eh_stack_entry_for_try */

#endif /* DO_FULL_PORTABLE_EH_LOWERING */

void lower_try_block(a_statement_ptr                 statement,
                     a_boolean                       is_function_try_block,
                     a_destructor_wrapper_info_block *dtor_info)
/*
Do IL lowering for an stmk_try_block statement.  If is_function_try_block
is TRUE, this is a function-try-block of a constructor or destructor,
and wrapper code to construct or destroy members and bases will be inserted
in the dependent statement of the "try".  For the destructor case,
dtor_info points to a block that provides additional information to
be passed down.
*/
{
  a_try_supplement_ptr
                     tsp = statement->variant.try_block;
  a_statement_ptr    dependent_stmt = tsp->statement, last_statement;
  a_statement_ptr    orig_stmt;
  a_handler_ptr      handlers = tsp->handlers, handler;
  an_insert_location insert_location;
  a_context          try_context;
  an_object_lifetime_ptr
                     lifetime;
#if DO_FULL_PORTABLE_EH_LOWERING
  a_variable_ptr     try_frame, catch_array_var;
  an_expr_node_ptr   compare_node, catch_clause_number_node;
  a_statement_ptr    prev_if_stmt, if_stmt;
  long               catch_clause_number;
#if FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS
  a_label_ptr        label;
  an_expr_node_ptr   modified_var_arg_list = NULL;
  a_context_ptr      context;
  a_variable_ptr     var;
#endif /* FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */

#if DO_FULL_PORTABLE_EH_LOWERING
  /* Change the stmk_try_block statement into a block, and prepare to insert
     code at the start of the block. */
  turn_statement_into_block(statement, &insert_location, &orig_stmt);
  /* Generate code to push a stack frame. */
  push_eh_stack_frame(ehsek_try_block, &try_frame, &insert_location);
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
  /* In the non-portable schemes, we keep the original "try" statement. */
  orig_stmt = statement;
  turn_statement_into_block(dependent_stmt, &insert_location, &dependent_stmt);
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
  /* Push a context around the try and catch.  This is needed to ensure that
     the "try" stack frame is popped on a goto out of the try or catch. */
  lifetime = tsp->lifetime;
  push_context(&try_context, (a_scope_ptr)NULL, lifetime);
  try_context.is_function_try_block = is_function_try_block;
#if DO_FULL_PORTABLE_EH_LOWERING
  curr_context->try_frame = try_frame;
  if (keep_object_lifetime_info_in_lowered_il) {
    /* To keep the object lifetime when the try block is eliminated,
       attach the object lifetime to the block generated above. */
    unbind_object_lifetime(lifetime);
    bind_object_lifetime(lifetime, iek_block,
                         (char *)statement->variant.block.extra_info);
  }  /* if */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
  begin_object_lifetime(lifetime, &insert_location);
#if DO_FULL_PORTABLE_EH_LOWERING
  /* Generate a description of the catch clause types. */
  catch_array_var = make_catch_array_var(handlers);
  /* Initialize the fields specific to a try block in the EH stack entry. */
  initialize_eh_stack_entry_for_try(try_frame, catch_array_var,
                                    &insert_location, &compare_node);
  /* Rewrite the stmk_try_block as an "if". */
  set_statement_kind(orig_stmt, (a_statement_kind)stmk_if);
  orig_stmt->expr = compare_node;
  /* The dependent statement is the statement under the "try". */
  orig_stmt->variant.if_stmt.then_statement = dependent_stmt;
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
  lower_block_statement(dependent_stmt,
                        is_function_try_block,
                        /*is_block_of_stmt_expr=*/FALSE,
                        dtor_info,
                        &last_statement);
  if (is_function_try_block && dtor_info != NULL) {
    /* Function try block in a destructor.  Insert destructor wrapper code
       at the end of the dependent statement. */
    a_statement_ptr    insert_stmt, return_stmt;
    an_insert_location epilogue_insert_location;
    check_assertion(last_statement != NULL);
    set_insert_location(last_statement, &epilogue_insert_location);
    insert_dtor_member_and_base_destructions(&epilogue_insert_location,
                                             dependent_stmt,
                                             dtor_info);
    /* Effectively eliminate the return at the end of the try block and fall
       through to beyond the try block. */
    insert_stmt = epilogue_insert_location.variant.statement.stmt;
    if (epilogue_insert_location.kind == ilk_after_statement) {
      return_stmt = insert_stmt->next;
    } else {
      check_assertion(epilogue_insert_location.kind == ilk_block_start);
      return_stmt = insert_stmt->variant.block.statements;
    }  /* if */
    check_assertion(return_stmt != NULL &&
                    return_stmt->kind == (a_statement_kind)stmk_return);
    /* Take the return off the return memo list. */
    check_assertion(return_memo_list != NULL &&
                    return_stmt == return_memo_list->stmt);
    { a_return_memo_ptr rmp = return_memo_list;
      return_memo_list = rmp->next;
      rmp->next = NULL;
      free_return_memo_list(rmp);
    }
    /* Turn the return statement into a no-op rather than eliminate it (in case
       there are any pragmas that refer to it). */
    turn_statement_into_noop(return_stmt);
  }  /* if */
#if DO_FULL_PORTABLE_EH_LOWERING
#if FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS
  /* Add a label inside the dependent statement to defeat optimization.
     See comment below. */
  { a_statement_ptr label_stmt = alloc_statement(stmk_label,
                                                 /*compiler_generated=*/TRUE);
    label = alloc_label();
    add_to_labels_list(label);
    label_stmt->variant.label.ptr = label;
    label->exec_stmt = label_stmt;
    /* Add the label statement at the start of the try compound statement. */
    check_assertion(dependent_stmt->kind == (a_statement_kind)stmk_block);
    label_stmt->next = dependent_stmt->variant.block.statements;
    label_stmt->parent = dependent_stmt;
    dependent_stmt->variant.block.statements = label_stmt;
    mark_stmk_inits_as_following_exec_statement(label_stmt->next);
  }
#endif /* FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS */
  catch_clause_number = 0;
  prev_if_stmt = orig_stmt;
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
  /* Walk through the catch clauses. */
  for (handler = handlers;
       handler != NULL;
       handler = handler->next) {
    a_statement_ptr dep_statement = handler->statement;
#if DO_FULL_PORTABLE_EH_LOWERING
    catch_clause_number++;
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
    /* Lower the dependent statement of the catch clause. */
    /* Note that the code to initialize the parameter (if there is one)
       is generated during the lowering of the dependent statement. */
    lower_statement(dep_statement);
#if DO_FULL_PORTABLE_EH_LOWERING
#if FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS
    /* Work up through the context stack from the current location (the
       try block) out to the function scope.  At each scope, look for local
       variables that are modified within the try and add them to a list
       of modified variables (that will be used as an argument list for a
       call to a dummy routine). */
    context = curr_context;
    do {
      context = context->parent;
      for (var = context->scope->variables;
           var != NULL;
           var = var->next) {
        add_var_addr_to_list_if_modified_in_try_block(var,
                                                      &modified_var_arg_list);
      }  /* for */
      for (var = context->scope->nonstatic_variables;
           var != NULL;
           var = var->next) {
        add_var_addr_to_list_if_modified_in_try_block(var,
                                                      &modified_var_arg_list);
      }  /* for */
    } while (context->scope != innermost_function_scope);
#endif /* FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS */
    /* The special processing for the ellipsis entry is not done if we want
       to add an "else" at the end to suppress optimization. */
    if (handler->parameter == NULL
#if FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS
        && modified_var_arg_list == NULL
#endif /* FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS */
                                        ) {
      /* This is an ellipsis entry.  No "if" is required, since it accepts
         any type.  Previous error checks have ensured that this is the
         last clause. */
      check_assertion_str(handler->next == NULL,
                          "lower_try_block: ellipsis clause not last");
      prev_if_stmt->variant.if_stmt.else_statement = dep_statement;
    } else {
      /* Test the catch clause number returned by the runtime in an "if"
         statement:
           if (__catch_clause_number == n) ...
      */
      catch_clause_number_node =
                               var_rvalue_expr(make_catch_clause_number_var());
      catch_clause_number_node->next = 
                            node_for_integer_constant(catch_clause_number,
                                                      (an_integer_kind)ik_int);
      compare_node = make_operator_node((an_expr_operator_kind)eok_eq,
                                        catch_clause_number_node->type,
                                        catch_clause_number_node);
      if_stmt = alloc_statement(stmk_if, /*compiler_generated=*/TRUE);
      if_stmt->position = handler->catch_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
      if_stmt->end_position = dep_statement->end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      if_stmt->expr = compare_node;
      if_stmt->variant.if_stmt.then_statement = dep_statement;
      prev_if_stmt->variant.if_stmt.else_statement = if_stmt;
      prev_if_stmt = if_stmt;
    }  /* if */
    /* Clear the assoc_handler pointer in the handler scope because it's not
       a C field. */
    handler->statement->variant.block.extra_info->assoc_scope->
                                                  variant.assoc_handler = NULL;
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
    if (handler->parameter != NULL) {
      /* Presumably the catch handler for this clause will need access to the
         typeinfo for the parameter, so create a variable for it (thereby
         forcing the typeinfo to be emitted). */
      a_variable_ptr       ptr_flags_var = NULL;
      an_eh_type_flags_set flags_value;
      check_assertion(handler->parameter->type->used_in_exception_or_rtti);
      handler->typeinfo_var = typeinfo_var_for_type(handler->parameter->type,
                                                    &flags_value,
                                                    &ptr_flags_var);
    }  /* if */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
  }  /* for */
#if DO_FULL_PORTABLE_EH_LOWERING
#if FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS
  /* Add an unreachable "else" at the end of the "if".  It contains a call
     followed by a goto that look like
       suppress_optim_on_vars_modified_in_try(&x, &y, &z);
       goto start_of_try;
     This convinces C compilers that the indicated variables must be stored
     out when modified in the try block. */
  /* The extra code is needed only if there are such modified variables. */
  if (modified_var_arg_list != NULL) {
    a_statement_ptr    block_stmt, call_stmt, goto_stmt;
    an_expr_node_ptr   call_node;
    an_insert_location block_insert_location;
    a_type_ptr         first_arg_type;
    check_assertion(is_pointer_type(modified_var_arg_list->type));
    first_arg_type = type_pointed_to(modified_var_arg_list->type);
    if (is_qualified_type(first_arg_type)) {
      /* __suppress_optim_on_vars_in_try is declared as (void *,...), but if
         the first argument is a pointer to a qualified type, a back end
         might reject the argument.  Since the arguments don't really matter,
         add a NULL argument to the beginning of the list. */
      an_expr_node_ptr null_node;
      a_constant_ptr   null_con = local_constant();
      make_zero_of_proper_type(void_star_type(), null_con);
      null_node = alloc_node_for_constant(null_con);
      release_local_constant(&null_con);
      null_node->next = modified_var_arg_list;
      modified_var_arg_list = null_node;
    }  /* if */
    call_node = make_prototyped_runtime_call("__suppress_optim_on_vars_in_try",
                                       &suppress_optim_on_vars_in_try_routine,
                                       void_type(),
                                       void_star_type(), NULL,
                                       modified_var_arg_list);
    /* This routine takes a variable number of arguments. */
    suppress_optim_on_vars_in_try_routine->
                         type->variant.routine.extra_info->has_ellipsis = TRUE;
    call_stmt = alloc_expr_statement(call_node);
    /* Add a block statement as the "else" of the last "if" for a catch
       handler. */
    block_stmt = alloc_statement(stmk_block, /*compiler_generated=*/TRUE);
    prev_if_stmt->variant.if_stmt.else_statement = block_stmt;
    /* Put the call into the block. */
    set_block_start_insert_location(block_stmt, &block_insert_location);
    insert_statement(call_stmt, &block_insert_location);
    /* Put the goto following the call. */
    goto_stmt = alloc_statement(stmk_goto, /*compiler_generated=*/TRUE);
    goto_stmt->variant.label.ptr = label;
    insert_statement(goto_stmt, &block_insert_location);
  }  /* if */
#endif /* FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
  /* Generate code to pop the "try" frame off the stack after the rewritten
     "if" statement.  This isn't needed for constructor function try blocks,
      because the end is not reachable. */
  if (!is_function_try_block || dtor_info != NULL) {
    set_insert_location(orig_stmt, &insert_location);
    cleanup_on_exit_from_try_block(&try_context, tsp, &insert_location);
  }  /* if */
  /* Pop the context pushed around the try block. */
  pop_context();
}  /* lower_try_block */


#if DO_FULL_PORTABLE_EH_LOWERING
/*
Pointers to routine entries for the runtime routines __throw_setup,
__throw_setup_dtor, __throw, and __rethrow, used in throwing exceptions.
NULL until allocated.
*/
STATIC_THREAD a_routine_ptr
		throw_setup_routine,
		throw_setup_dtor_routine,
		throw_setup_ptr_routine,
		throw_routine,
		rethrow_routine,
		internal_rethrow_routine;


static an_expr_node_ptr make_rethrow_call(void)
/*
Make an expression that does a rethrow, and return a pointer to it.
*/
{
  an_expr_node_ptr rethrow_node =
                 make_prototyped_runtime_call("__rethrow", &rethrow_routine,
                                        void_type(),
                                        NULL, NULL,
                                        (an_expr_node_ptr)NULL);

  rethrow_routine->type->variant.routine.extra_info->does_not_return = TRUE;
  return rethrow_node;
}  /* make_rethrow_call */

#if ABI_COMPATIBILITY_VERSION >= 235

static an_expr_node_ptr make_internal_rethrow_call(void)
/*
Make an expression that calls the internal-rethrow routine, and return a
pointer to it.
*/
{
  an_expr_node_ptr rethrow_node =
                 make_prototyped_runtime_call("__internal_rethrow",
                                        &internal_rethrow_routine,
                                        void_type(),
                                        NULL, NULL,
                                        (an_expr_node_ptr)NULL);

  internal_rethrow_routine->type
                          ->variant.routine.extra_info->does_not_return = TRUE;
  return rethrow_node;
}  /* make_internal_rethrow_call */

#endif /* ABI_COMPATIBILITY_VERSION >= 235 */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */

an_expr_node_ptr make_internal_try_expr(an_expr_node_ptr try_expr,
                                        an_expr_node_ptr catch_expr)
/*
Make an expression for an internal "try" block that attempts to execute
try_expr and, if an exception is thrown, executes catch_expr and then
does a rethrow.  Return a pointer to the expression created.  The
type of the overall expression will be void (i.e., the value of try_expr
is not passed through).
*/
{
  an_expr_node_ptr   internal_try_node;
#if DO_FULL_PORTABLE_EH_LOWERING
  /* Fully-lowered scheme: */
  a_variable_ptr     try_frame;
  an_expr_node_ptr   compare_node, rethrow_node;
  an_expr_node_ptr   catch_plus_rethrow, zero_node, question_node;
  an_insert_location insert_location;

  set_expr_creation_insert_location(&insert_location);
  /* Push a stack frame for the internal try. */
  push_eh_stack_frame(ehsek_try_block, &try_frame, &insert_location);
  /* Initialize the fields specific to a try block in the EH stack entry.
     The NULL array of catch entries indicates an internal try block.
     This also sets compare_node to point to an expression that
     does a setjmp call and compares it to zero. */
  initialize_eh_stack_entry_for_try(try_frame, (a_variable_ptr)NULL,
                                    &insert_location, &compare_node);
#if ABI_COMPATIBILITY_VERSION >= 235
  /* Make an internal rethrow. */
  rethrow_node = make_internal_rethrow_call();
#else /* ABI_COMPATIBILITY_VERSION < 235 */
  /* Make a rethrow. */
  rethrow_node = make_rethrow_call();
#endif /* ABI_COMPATIBILITY_VERSION >= 235 */
  /* Make (catch-expr, rethrow). */
  catch_plus_rethrow = make_comma_node(catch_expr, rethrow_node);
  /* Add a zero constant cast to void after the rethrow to give the
     expression void type. */
  zero_node = zero_cast_to_void();
  catch_plus_rethrow = make_comma_node(catch_plus_rethrow, zero_node);
  /* Make (setjmp(...)==0) ? (void)try_expr : (catch_expr, rethrow, (void)0) */
  /* Cast try_expr to void since its value is discarded. */
  try_expr = add_cast_if_necessary(try_expr, void_type());
  compare_node->next = try_expr;
  try_expr->next = catch_plus_rethrow;
  question_node = make_operator_node((an_expr_operator_kind)eok_question,
                                     try_expr->type, compare_node);
  insert_expr(question_node, &insert_location);
  /* Pop the stack frame pushed earlier. */
  pop_eh_stack_frame(ehsek_try_block, try_frame, &insert_location);
  internal_try_node = insert_location.variant.expr;
  /* Cast the overall try block expression to void.  Without this, the
     expression would have as its value/type something quasi-random coming
     from the end of the code sequence for the stack pop. */
  internal_try_node = add_cast_if_necessary(internal_try_node, void_type());
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
  /* In other schemes, insert an enk_lowered_eh_construct/leck_internal_try
     expression node. */
  internal_try_node = alloc_lowered_eh_construct_node(
                               (a_lowered_eh_construct_kind)leck_internal_try);
  internal_try_node->variant.lowered_eh.variant.try_and_catch_expr = try_expr;
  try_expr->next = catch_expr;
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
  return internal_try_node;
}  /* make_internal_try_expr */


#if DO_FULL_PORTABLE_EH_LOWERING
#if !ABI_CHANGES_FOR_RTTI

/* Routines to build an access string for a throw, using the temp_text
   buffer. */

STATIC_THREAD sizeof_t
		curr_size_access_string_buffer;
			/* Number of characters currently used in
			   temp_text_buffer. */


static void add_char_to_access_string_buffer(char ch)
/*
Add the indicated character to the temp_text_buffer.
*/
{
  ensure_temp_text_buffer_space(curr_size_access_string_buffer+1);
  temp_text_buffer[curr_size_access_string_buffer++] = ch;
}  /* add_char_to_access_string_buffer */


static void add_to_throw_access_string(a_type_ptr                   type,
                                       an_accessible_base_class_ptr abcp,
                                       a_base_class_ptr             parent_bcp)
/*
Generate part of the access control string for a throw.  type is a
class type to be processed.  abcp is the list of accessible base classes
from the throw expression.  parent_bcp is the base class pointer for the
immediate parent of this base class as we descend recursively, or NULL
if there is no parent.
*/
{
  a_base_class_ptr bcp, throw_bcp;
  a_type_ptr       throw_class = abcp->base_class->derived_class;
  char             ch;
  an_accessible_base_class_ptr
                   temp_abcp;

  /* Go through the direct and virtual base classes (the same ones listed
     in the typeinfo base class list).  Do the top level first before
     looking at base classes of base classes because we want to find
     virtual base classes at the top. */
  for (bcp = type->variant.class_struct_union.extra_info->base_classes;
       bcp != NULL;
       bcp = bcp->next) {
   if (bcp->direct || bcp->is_virtual) {
      /* Add "Y" if this base class is on the list of accessible base classes,
         "N" otherwise.  Ambiguous classes are considered inaccessible. */
      ch = 'N';
      if (!bcp->ambiguous) {
        /* Find the base class entry on the throw type that corresponds to the
           base class we are examining here. */
        throw_bcp = corresponding_base_class(bcp, throw_class, parent_bcp);
        /* See if the base class is on the list of accessible base classes. */
        for (temp_abcp = abcp;
             temp_abcp != NULL;
             temp_abcp = temp_abcp->next) {
          if (temp_abcp->base_class == throw_bcp) {
            /* Yes, the base class is accessible. */
            ch = 'Y';
            break;
          }  /* if */
        }  /* if */
      }  /* if */
      add_char_to_access_string_buffer(ch);
    }  /* if */
  }  /* for */
  /* Go through the base classes of the direct and virtual base classes. */
  for (bcp = type->variant.class_struct_union.extra_info->base_classes;
       bcp != NULL;
       bcp = bcp->next) {
    if (bcp->direct || bcp->is_virtual) {
      /* Find the base class entry on the throw type that corresponds to the
         base class we are examining here. */
      throw_bcp = corresponding_base_class(bcp, throw_class, parent_bcp);
      add_to_throw_access_string(bcp->type, abcp, throw_bcp);
    }  /* if */
  }  /* for */
}  /* add_to_throw_access_string */


static a_constant_ptr make_throw_access_string(an_expr_node_ptr expr)
/*
expr is a throw expression node.  If necessary, generate a string
constant to describe the accessible base classes of the thrown type and
return a pointer to it.  The constant is shareable.  If no constant
is needed, return NULL.

This mechanism was made obsolete by changes in the definition of
access checking for throw.  Now, typeinfo information can be used
instead.
*/
{
  a_type_ptr                   type;
  an_accessible_base_class_ptr abcp;
  a_constant_ptr               string_con = NULL;
  a_constant_ptr               constant = local_constant();
  char                         *pstr;

  /* No constant is needed if there are no accessible base classes
     (that includes the case where the type involved is not a class type). */
  abcp = expr->variant.throw_info->accessible_base_classes;
  if (abcp != NULL) {
    /* A string is needed.  Get the class involved in the throw. */
    type = abcp->base_class->derived_class;
    /* Start with an empty string. */
    curr_size_access_string_buffer = 0;
    /* Build the string. */
    add_to_throw_access_string(type, abcp, (a_base_class_ptr)NULL);
    /* Put a terminating null on the string. */
    add_char_to_access_string_buffer('\0');
    /* Build the string constant. */
    pstr = alloc_text_of_string_literal(curr_size_access_string_buffer);
    (void)strcpy(pstr, temp_text_buffer);
    clear_constant(constant, (a_constant_repr_kind)ck_string);
    constant->type =
                    string_type((a_targ_size_t)curr_size_access_string_buffer);
    constant->variant.string.length = curr_size_access_string_buffer;
    constant->variant.string.value  = pstr;
    string_con = alloc_shareable_constant(constant);
  }  /* if */
  release_local_constant(&constant);
  return string_con;
}  /* make_throw_access_string */

#endif /* !ABI_CHANGES_FOR_RTTI */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */

#if !DO_FULL_PORTABLE_EH_LOWERING

an_expr_node_ptr make_thrown_object_address_node(void)
/*
Make an expression node that represents the address in the runtime to which
a thrown object should be copied, and return a pointer to the node.
*/
{
  an_expr_node_ptr node = alloc_lowered_eh_construct_node(
                      (a_lowered_eh_construct_kind)leck_thrown_object_address);
  node->type = void_star_type();
  return node;
}  /* make_thrown_object_address_node */

#endif /* !DO_FULL_PORTABLE_EH_LOWERING */

void lower_throw(an_expr_node_ptr expr)
/*
Lower an enk_throw expression node.
*/
{
  a_type_ptr         throw_type;
  a_dynamic_init_ptr dip;
  an_init_pos_descr  ipd;
  an_insert_location insert_location;
  a_throw_supplement_ptr
                     tsp = expr->variant.throw_info;
#if DO_FULL_PORTABLE_EH_LOWERING
  a_type_ptr         ptr_throw_type;
  a_variable_ptr     temp_var, typeinfo_var, ptr_flags_var;
  an_expr_node_ptr   call_node, typeinfo_node, size_node, flags_node = NULL;
  an_expr_node_ptr   assign_node;
  an_eh_type_flags_set
                     flags_value;
#endif /* DO_FULL_PORTABLE_EH_LOWERING */

  /* Check for a throw with no operand, i.e., a rethrow. */
  if (tsp == NULL) {
#if DO_FULL_PORTABLE_EH_LOWERING
    /* This is a rethrow.  Replace the enk_throw node with a call of
       __rethrow. */
    call_node = make_rethrow_call();
    overwrite_node(expr, call_node);
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
    /* No lowering required in the non-portable schemes. */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
  } else {
    /* Throw of an object. */
#if DO_FULL_PORTABLE_EH_LOWERING
    a_type_ptr       size_t_type = integer_type(targ_size_t_int_kind);
    /* Make the typeinfo variable for the underlying throw type (before the
       type is lowered). */
    typeinfo_var = typeinfo_var_for_type(get_underlying_type(tsp->type),
                                         &flags_value,
                                         &ptr_flags_var);
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
    throw_type = tsp->type;
    lower_os_type(throw_type);
    throw_type = f_skip_typerefs(throw_type);
    dip = tsp->dynamic_init;
    /* There should be no destructor indicated, because the runtime handles
       the destruction. */
    check_assertion(dip->destructor == NULL);
#if DO_FULL_PORTABLE_EH_LOWERING
    /* Make the assignment
         temp = __throw_setup(&typeinfo, size, flags)
       This allocates the space into which the thrown object is copied and
       sets temp to point to that space.  typeinfo is the typeinfo variable
       for the base type of the type thrown; size is the size in bytes of
       the type thrown; and flags has the ETS_IS_POINTER bit set to indicate
       that a pointer to the typeinfo type is being thrown.  If the
       thrown type has a destructor, the call is instead
         temp = __throw_setup_dtor(&typeinfo, size, flags, dtor)
       (This latter form was added in version 2.38.)
       If the thrown type is a multi-level pointer, the call is instead
         temp = __throw_setup_ptr(&typeinfo, size, ptr_flags);
       (This latter form was added in version 2.41.)
    */
#if !ABI_CHANGES_FOR_RTTI
    /* The old form is
         temp = __throw_alloc(&typeinfo, size, flags, access)
       which has access non-NULL when throwing a class -- it is a character
       string indicating which of the base classes are accessible. */
#endif /* !ABI_CHANGES_FOR_RTTI */
    ptr_throw_type = make_pointer_type(throw_type);
    temp_var = make_local_temporary(ptr_throw_type);
    /* Make the arguments for the __throw_setup call. */
    typeinfo_node = var_addr_expr(typeinfo_var);
    typeinfo_node = add_cast_if_necessary(typeinfo_node,
                                          make_runtime_typeinfo_type());
    size_node = node_for_host_large_integer(
                 (a_host_large_integer)throw_type->size, targ_size_t_int_kind);
    typeinfo_node->next = size_node;
    if (ptr_flags_var != NULL) {
      /* Build the parameter list for __throw_setup_ptr. */
      size_node->next = array_first_element_addr_expr(ptr_flags_var);
    } else {
      flags_node = node_for_integer_constant((long)flags_value,
                                             targ_ets_flag_type_int_kind);
      size_node->next = flags_node;
    }  /* if */
#if !ABI_CHANGES_FOR_RTTI
    /* Make a string to describe the accessible base classes, and pass
       its address to the runtime routine. */
    { an_expr_node_ptr access_node;
      a_constant_ptr   access_con = local_constant();
      a_constant_ptr   string_con;
      string_con = make_throw_access_string(expr);
      if (string_con == NULL) {
        /* No access string.  Use a NULL pointer. */
        make_zero_of_proper_type(char_star_type(), access_con);
      } else {
        set_constant_address_constant(string_con, access_con);
        implicit_cast(access_con, char_star_type());
      }  /* if */
      access_node = alloc_node_for_constant(access_con);
      check_assertion(flags_node != NULL);
      flags_node->next = access_node;
      release_local_constant(&access_con);
    }
#endif /* !ABI_CHANGES_FOR_RTTI */
    /* Make the __throw_setup call. */
#if !ABI_CHANGES_FOR_RTTI
    /* Old interface */
    call_node = make_prototyped_runtime_call_full("__throw_alloc",
                                     &throw_setup_routine, void_star_type(),
                                     make_runtime_typeinfo_type(),
                                     size_t_type,
                                     integer_type(targ_ets_flag_type_int_kind),
                                     char_star_type(), NULL, NULL, NULL,
                                     typeinfo_node);
#else /* ABI_CHANGES_FOR_RTTI */
#if PASS_DTOR_POINTER_TO_THROW
    if (tsp->destructor != NULL) {
      /* A destructor must be called by the runtime to destroy the object,
         so call __throw_setup_dtor and pass the destructor pointer. */
      a_routine_ptr destructor = tsp->destructor;
#if IA64_ABI
      destructor = alternate_entry_point(destructor,
                                         (a_ctor_or_dtor_kind)cdk_complete,
                                         /*define_now=*/FALSE);
#endif /* IA64_ABI */
      check_assertion(flags_node != NULL);
      flags_node->next = add_cast(function_addr_expr(destructor),
                                  make_dtor_type());
      call_node = make_prototyped_runtime_call_full("__throw_setup_dtor",
                                     &throw_setup_dtor_routine,
                                     void_star_type(),
                                     make_runtime_typeinfo_type(),
                                     size_t_type,
                                     integer_type(targ_ets_flag_type_int_kind),
                                     make_dtor_type(), NULL, NULL, NULL,
                                     typeinfo_node);
    } else
#endif /* PASS_DTOR_POINTER_TO_THROW */
    /* Do not insert code here; this is the "else" of an "if". */
    {
      if (ptr_flags_var != NULL) {
        /* A multi-level pointer.  Use __throw_setup_ptr. */
        call_node = make_prototyped_runtime_call_full("__throw_setup_ptr",
            &throw_setup_ptr_routine, void_star_type(),
            make_runtime_typeinfo_type(), size_t_type,
            make_pointer_type(integer_type(targ_ets_flag_type_int_kind)),
            NULL, NULL, NULL, NULL, typeinfo_node);
      } else {
        /* Not a multi_level pointer. */
        call_node = make_prototyped_runtime_call_full("__throw_setup",
                                     &throw_setup_routine, void_star_type(),
                                     make_runtime_typeinfo_type(),
                                     size_t_type,
                                     integer_type(targ_ets_flag_type_int_kind),
                                     NULL, NULL, NULL, NULL, typeinfo_node);
      }  /* if */
    }  /* if */
#endif /* !ABI_CHANGES_FOR_RTTI */
    /* Cast the pointer to the right type. */
    call_node = add_cast_if_necessary(call_node, ptr_throw_type);
    /* Make the node to assign the pointer to the temporary. */
    assign_node = make_var_assignment_expr(temp_var, call_node);
    /* Make the call to the __throw routine, which actually does the
       throw.  It has no arguments. */
    call_node = make_prototyped_runtime_call("__throw", &throw_routine,
                                       void_type(),
                                       NULL, NULL,
                                       (an_expr_node_ptr)NULL);
    throw_routine->type->variant.routine.extra_info->does_not_return = TRUE;
    /* Overwrite the original node with a comma expression joining the
       __throw_setup and __throw expressions:
         ((temp = __throw_setup(...)), __throw())
    */
    assign_node->next = call_node;
    set_expr_node_kind(expr, (an_expr_node_kind)enk_operation);
    set_node_operator(expr, (an_expr_operator_kind)eok_comma,
                      call_node->type, /*is_lvalue=*/FALSE, assign_node);
    /* Now generate the initialization code for the dynamic initialization
       and insert it preceding the call of __throw.  That gets it between
       the allocation and the throw:
         ((temp = __throw_setup(...)), (initialization, __throw()))
       The address to be initialized is pointed to by the temporary. */
    set_var_indirect_init_pos_descr(temp_var, &ipd);
    set_expr_insert_location(call_node, &insert_location);
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
    /* For the non-portable schemes, we want to lower the dynamic
       initialization, with any references to the destination address
       represented by an enk_lowered_eh_construct/leck_thrown_object_address
       expression node. */
    set_thrown_object_init_pos_descr(throw_type, &ipd);
    set_expr_creation_insert_location(&insert_location);
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
    /* Generate code to copy the thrown expression to the runtime. */
    lower_dynamic_init(dip, &ipd,
                       (an_implied_copy_source *)NULL,
                       (a_variable_ptr)NULL,
                       LDIO_THROW,
                       /*others_follow_in_aggr=*/FALSE,
                       &insert_location, (a_boolean *)NULL,
                       (a_constant **)NULL);
#if !DO_FULL_PORTABLE_EH_LOWERING
    /* Put the lowered node pointer into the throw supplement. */
    tsp->expr = insert_location.variant.expr;
    tsp->dynamic_init = NULL;
#endif /* !DO_FULL_PORTABLE_EH_LOWERING */
  }  /* if */
}  /* lower_throw */


#if ABI_COMPATIBILITY_VERSION >= 233

#if DO_FULL_PORTABLE_EH_LOWERING
/*
Pointer to the routine entry for the runtime routine __exception_started.
NULL until created.
*/
STATIC_THREAD a_routine_ptr
		exception_started_routine;
#endif /* DO_FULL_PORTABLE_EH_LOWERING */


void record_exception_started(an_insert_location *insert_location)
/*
Insert code at the indicated insert point, as part of the code for
a throw, to indicate the point at which the exception is considered
started.  This is used when throwing a class that has a copy constructor.
In some cases, the final copy constructor call is considered "inside"
the throw, whereas the rest of the throw expression evaluation is
"outside" the throw.
*/
{
#if DO_FULL_PORTABLE_EH_LOWERING
  /* Portable scheme: */
  /* Make a call of the runtime routine __exception_started. */
  make_call_statement(make_prototyped_runtime_routine("__exception_started",
                                                    &exception_started_routine,
                                                    void_type(), NULL,
                                                    NULL, NULL, NULL, NULL,
                                                    NULL, NULL),
                      (an_expr_node_ptr)NULL,
                      (an_expr_node_ptr)NULL,
                      insert_location);
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
  /* In other schemes, insert an enk_lowered_eh_construct/
     leck_exception_started expression node. */
  { an_expr_node_ptr node = alloc_lowered_eh_construct_node(
                          (a_lowered_eh_construct_kind)leck_exception_started);
    (void)insert_expr_statement(node, insert_location);
  }
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
}  /* record_exception_started */

#endif /* ABI_COMPATIBILITY_VERSION >= 233 */

void insert_code_to_indicate_cleanup_state(
                                         a_dynamic_init_ptr   cleanup_state,
                                         an_insert_location   *insert_location,
                                         ARG_UNUSED a_boolean unreachable)
/*
Insert code to indicate the indicated cleanup state.  The code is inserted
at *insert_location, and *insert_location is updated.  This routine is called
only when exceptions are enabled.  unreachable is TRUE if the inserted
code will be unreachable (presumably, it's being added to provide information
for back ends that use the cleanup state information statically to build
tables).
*/
{
  an_expr_node_ptr node;

  check_assertion_str(exceptions_enabled,
     "insert_code_to_indicate_cleanup_state: called with exceptions disabled");
#if !INDICATE_CLEANUP_STATE_IN_UNREACHABLE_CODE
  check_assertion(long_lifetime_temps ||
                  (innermost_function_scope != NULL &&
                   (has_destructions(innermost_function_scope->lifetime) ||
                    innermost_function_scope->variant.routine.ptr->
                                                        compiler_generated)));
#endif /* !INDICATE_CLEANUP_STATE_IN_UNREACHABLE_CODE */
#if DO_FULL_PORTABLE_EH_LOWERING
  /* In the portable scheme, assign the region number to __eh_curr_region. */
  node = node_for_integer_constant((long)cleanup_region_number(cleanup_state),
                                   targ_region_number_int_kind);
  assign_to_eh_curr_region(node, insert_location);
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
  /* In the other schemes, generate an enk_lower_eh_construct/
     leck_[unreachable_]cleanup_state expression node. */
  node = alloc_lowered_eh_construct_node(
             unreachable ?
                  (a_lowered_eh_construct_kind)leck_unreachable_cleanup_state :
                  (a_lowered_eh_construct_kind)leck_cleanup_state);
#if GENERATE_EH_TABLES
  /* With partial lowering, the region number is put into the node. */
  node->variant.lowered_eh.variant.cleanup_region_number =
                                          cleanup_region_number(cleanup_state);
#else /* GENERATE_EH_TABLES */
  /* With no lowering, a pointer to the dynamic initialization entry is put
     into the node. */
  node->variant.lowered_eh.variant.cleanup_ptr = cleanup_state;
#endif /* GENERATE_EH_TABLES */
  (void)insert_expr_statement(node, insert_location);
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
}  /* insert_code_to_indicate_cleanup_state */


void save_eh_lowering_context(ARG_UNUSED an_eh_lowering_context *ehcontext)
/*
Save the current state of exception handling lowering, as reflected in
global variables, in *ehcontext, for later restoration by
restore_eh_lowering_context.
*/
{
  if (exceptions_enabled) {
#if DO_FULL_PORTABLE_EH_LOWERING
    ehcontext->object_addr_table_var = object_addr_table_var;
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
#if GENERATE_EH_TABLES
    ehcontext->array_table_var = array_table_var;
    ehcontext->array_table_aggr_con = array_table_aggr_con;
    ehcontext->region_table_var = region_table_var;
    ehcontext->region_table_aggr_con = region_table_aggr_con;
    ehcontext->next_avail_region_number = next_avail_region_number;
#endif /* GENERATE_EH_TABLES */
  }  /* if */
}  /* save_eh_lowering_context */


void restore_eh_lowering_context(ARG_UNUSED an_eh_lowering_context *ehcontext)
/*
Restore the current state of exception handling lowering, as reflected in
global variables, from *ehcontext.
*/
{
  if (exceptions_enabled) {
#if DO_FULL_PORTABLE_EH_LOWERING
    object_addr_table_var = ehcontext->object_addr_table_var;
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
#if GENERATE_EH_TABLES
    array_table_var = ehcontext->array_table_var;
    array_table_aggr_con = ehcontext->array_table_aggr_con;
    region_table_var = ehcontext->region_table_var;
    region_table_aggr_con = ehcontext->region_table_aggr_con;
    next_avail_region_number = ehcontext->next_avail_region_number;
#endif /* GENERATE_EH_TABLES */
  }  /* if */
}  /* restore_eh_lowering_context */


void eh_function_lower_init(void)
/*
Initialize static variables needed on a per-function basis for
IL lowering for exceptions.
*/
{
  if (exceptions_enabled) {
#if DO_FULL_PORTABLE_EH_LOWERING
    object_addr_table_var = NULL;
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
#if GENERATE_EH_TABLES
    array_table_var = NULL;
    array_table_aggr_con = NULL;
    region_table_var = NULL;
    region_table_aggr_con = NULL;
    next_avail_region_number = 0;
#endif /* GENERATE_EH_TABLES */
  }  /* if */
}  /* eh_function_lower_init */


void eh_lower_one_time_init(void)
/*
Do one-time initialization of variables related to lowering of structures
involved in exception handling.
*/
{
  /* Save variables from lower_eh.h and lower_eh.c that are needed for
     precompiled headers */
  if (precompiled_header_processing_required) {
    STATIC_THREAD a_pch_saved_variable saved_vars[] = {
      pch_array_saved_var_array_elem(typeinfo_types),
      pch_saved_var_array_elem(base_class_spec_type),
#if ABI_CHANGES_FOR_RTTI
#if IA64_ABI
      pch_saved_var_array_elem(vmi_class_types),
      pch_saved_var_array_elem(bad_typeid_routine),
#else /* !IA64_ABI */
      pch_saved_var_array_elem(typeinfo_tinfo_field),
      pch_saved_var_array_elem(get_typeid_routine),
#endif /* !IA64_ABI */
      pch_array_saved_var_array_elem(vtbls_for_type_info),
#endif /* ABI_CHANGES_FOR_RTTI */
#if GENERATE_EH_TABLES
      pch_saved_var_array_elem(array_descr_type),
      pch_saved_var_array_elem(region_descr_type),
      pch_saved_var_array_elem(vla_dealloc_eh_routine),
      pch_saved_var_array_elem(destroy_exception_object_routine),
#endif /* GENERATE_EH_TABLES */
#if DO_FULL_PORTABLE_EH_LOWERING
      pch_saved_var_array_elem(eh_curr_region_var),
      pch_saved_var_array_elem(curr_eh_stack_entry_var),
      pch_saved_var_array_elem(catch_clause_number_var),
      pch_saved_var_array_elem(caught_object_address_var),
      pch_saved_var_array_elem(exception_type_spec_type),
      pch_saved_var_array_elem(jmp_buf_type),
      pch_saved_var_array_elem(eh_stack_entry_type),
      pch_saved_var_array_elem(ehse_next_field),
      pch_saved_var_array_elem(ehse_kind_field),
      pch_saved_var_array_elem(ehse_variant_field),
      pch_saved_var_array_elem(ehse_try_field),
      pch_saved_var_array_elem(ehse_try_setjmp_buffer_field),
      pch_saved_var_array_elem(ehse_try_catch_entries_field),
      pch_saved_var_array_elem(ehse_try_rtinfo_field),
      pch_saved_var_array_elem(ehse_try_region_number_field),
      pch_saved_var_array_elem(ehse_function_field),
      pch_saved_var_array_elem(ehse_function_regions_field),
      pch_saved_var_array_elem(ehse_function_obj_table_field),
      pch_saved_var_array_elem(ehse_function_array_table_field),
      pch_saved_var_array_elem(ehse_function_saved_region_number_field),
      pch_saved_var_array_elem(ehse_throw_spec_field),
      pch_saved_var_array_elem(free_thrown_object_routine),
      pch_saved_var_array_elem(setjmp_routine),
      pch_saved_var_array_elem(suppress_optim_on_vars_in_try_routine),
      pch_saved_var_array_elem(throw_setup_routine),
      pch_saved_var_array_elem(throw_setup_dtor_routine),
      pch_saved_var_array_elem(throw_setup_ptr_routine),
      pch_saved_var_array_elem(throw_routine),
      pch_saved_var_array_elem(rethrow_routine),
      pch_saved_var_array_elem(internal_rethrow_routine),
#if ABI_COMPATIBILITY_VERSION >= 233
      pch_saved_var_array_elem(exception_caught_routine),
      pch_saved_var_array_elem(exception_started_routine),
#endif /* ABI_COMPATIBILITY_VERSION >= 233 */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  /* Register variables that must be saved and restored when switching
     between translation units. */
  register_trans_unit_array(typeinfo_types);
  register_trans_unit_variable(base_class_spec_type);
#if ABI_CHANGES_FOR_RTTI
#if IA64_ABI
  register_trans_unit_variable(vmi_class_types);
  register_trans_unit_variable(bad_typeid_routine);
#else /* !IA64_ABI */
  register_trans_unit_variable(typeinfo_tinfo_field);
  register_trans_unit_variable(get_typeid_routine);
#endif /* !IA64_ABI */
  register_trans_unit_array(vtbls_for_type_info);
#endif /* ABI_CHANGES_FOR_RTTI */
#if GENERATE_EH_TABLES
  register_trans_unit_variable(array_descr_type);
  register_trans_unit_variable(region_descr_type);
#endif /* GENERATE_EH_TABLES */
#if DO_FULL_PORTABLE_EH_LOWERING
  register_trans_unit_variable(eh_curr_region_var);
  register_trans_unit_variable(curr_eh_stack_entry_var);
  register_trans_unit_variable(catch_clause_number_var);
  register_trans_unit_variable(caught_object_address_var);
  register_trans_unit_variable(exception_type_spec_type);
  register_trans_unit_variable(jmp_buf_type);
  register_trans_unit_variable(eh_stack_entry_type);
  register_trans_unit_variable(ehse_next_field);
  register_trans_unit_variable(ehse_kind_field);
  register_trans_unit_variable(ehse_variant_field);
  register_trans_unit_variable(ehse_try_field);
  register_trans_unit_variable(ehse_try_setjmp_buffer_field);
  register_trans_unit_variable(ehse_try_catch_entries_field);
  register_trans_unit_variable(ehse_try_rtinfo_field);
  register_trans_unit_variable(ehse_try_region_number_field);
  register_trans_unit_variable(ehse_function_field);
  register_trans_unit_variable(ehse_function_regions_field);
  register_trans_unit_variable(ehse_function_obj_table_field);
  register_trans_unit_variable(ehse_function_array_table_field);
  register_trans_unit_variable(ehse_function_saved_region_number_field);
  register_trans_unit_variable(ehse_throw_spec_field);
  register_trans_unit_variable(free_thrown_object_routine);
  register_trans_unit_variable(setjmp_routine);
  register_trans_unit_variable(suppress_optim_on_vars_in_try_routine);
  register_trans_unit_variable(throw_setup_routine);
  register_trans_unit_variable(throw_setup_dtor_routine);
  register_trans_unit_variable(throw_setup_ptr_routine);
  register_trans_unit_variable(throw_routine);
  register_trans_unit_variable(rethrow_routine);
  register_trans_unit_variable(internal_rethrow_routine);
#if ABI_COMPATIBILITY_VERSION >= 233
  register_trans_unit_variable(exception_caught_routine);
  register_trans_unit_variable(exception_started_routine);
#endif /* ABI_COMPATIBILITY_VERSION >= 233 */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
}  /* eh_lower_one_time_init */


void eh_lower_trans_unit_init(void)
/*
Initialize static variables related to IL lowering of exceptions that
must be initialized for each translation unit.
*/
{
  base_class_spec_type = NULL;
#if ABI_CHANGES_FOR_RTTI
#if IA64_ABI
  vmi_class_types = NULL;
  bad_typeid_routine = NULL;
#else /* !IA64_ABI */
  typeinfo_tinfo_field = NULL;
  get_typeid_routine = NULL;
#endif /* !IA64_ABI */
  { int i;
    for (i = 0; i != (int)tik_last; ++i) {
      typeinfo_types[i] = NULL;
      vtbls_for_type_info[i] = NULL;
    }  /* for */
  }
#endif /* ABI_CHANGES_FOR_RTTI */
#if GENERATE_EH_TABLES
  array_descr_type = NULL;
  region_descr_type = NULL;
  vla_dealloc_eh_routine = NULL;
  destroy_exception_object_routine = NULL;
#endif /* GENERATE_EH_TABLES */
#if DO_FULL_PORTABLE_EH_LOWERING
  eh_curr_region_var = NULL;
  curr_eh_stack_entry_var = NULL;
  catch_clause_number_var = NULL;
  caught_object_address_var = NULL;
  exception_type_spec_type = NULL;
  jmp_buf_type = NULL;
  eh_stack_entry_type = NULL;
  ehse_next_field = NULL;
  ehse_kind_field = NULL;
  ehse_variant_field = NULL;
  ehse_try_field = NULL;
  ehse_try_setjmp_buffer_field = NULL;
  ehse_try_catch_entries_field = NULL;
  ehse_try_rtinfo_field = NULL;
  ehse_try_region_number_field = NULL;
  ehse_function_field = NULL;
  ehse_function_regions_field = NULL;
  ehse_function_obj_table_field = NULL;
  ehse_function_array_table_field = NULL;
  ehse_function_saved_region_number_field = NULL;
  ehse_throw_spec_field = NULL;
  free_thrown_object_routine = NULL;
  setjmp_routine = NULL;
  suppress_optim_on_vars_in_try_routine = NULL;
  throw_setup_routine = NULL;
  throw_setup_dtor_routine = NULL;
  throw_setup_ptr_routine = NULL;
  throw_routine = NULL;
  rethrow_routine = NULL;
  internal_rethrow_routine = NULL;
#if ABI_COMPATIBILITY_VERSION >= 233
  exception_caught_routine = NULL;
  exception_started_routine = NULL;
#endif /* ABI_COMPATIBILITY_VERSION >= 233 */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
  in_typeinfo_var_generation_phase = FALSE;
}  /* eh_lower_trans_unit_init */


void eh_lower_init(void)
/*
Initialize static variables related to IL lowering of exceptions that
must be initialized for each compilation.
*/
{
#if GENERATE_EH_TABLES
  /* Make a constant for the maximum region number, also used for the
     null region number.  */
  { a_targ_size_t    size;
    a_targ_alignment align;
    /* Find out how big a field is used for region numbers. */
    get_integer_size_and_alignment(targ_region_number_int_kind, &size, &align);
    size = size * targ_char_bit;
    /* Make a bit mask "size" bits long. */
    if (size >= sizeof(unsigned long)*CHAR_BIT) {
      null_eh_region_number = ~(unsigned long)0;
    } else {
      null_eh_region_number = ((unsigned long)1 << (int)size) - 1;
    }  /* if */
  }
  array_table_var = NULL;
  array_table_aggr_con = NULL;
  region_table_var = NULL;
  region_table_aggr_con = NULL;
  next_avail_region_number = 0;
#endif /* GENERATE_EH_TABLES */
#if DO_FULL_PORTABLE_EH_LOWERING
  object_addr_table_var = NULL;
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
  /* eh_lower_trans_unit_init is called from il_lower_trans_unit_init. */
  ptr_to_const_typeinfo_type = NULL;
}  /* eh_lower_init */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* DO_IL_LOWERING */

