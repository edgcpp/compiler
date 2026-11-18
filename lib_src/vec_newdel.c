/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

C++ runtime routines to provide vector new() and delete() functionality.

*/

#include "basics.h"
#include "runtime.h"
#include "eh.h"
#pragma hdrstop
#include "vec_newdel.h"
#include "main.h"
#if ABI_COMPATIBILITY_VERSION >= 300
#include "memzero.h"
#endif /* ABI_COMPATIBILITY_VERSION >= 300 */

/*
For arrays, _vec_new() and _vec_delete() will maintain a linked list of 
"hidden" information on each array allocated and subsequently deleted.
This information will be used by _vec_ctor() and _vec_dtor() to determine
the size of undimensioned arrays.
*/

/*
Hidden structure of information for each array "allocated" by new().
*/
typedef struct vec_info *a_vec_info_ptr;
				/* Pointer to a vector information struct. */
typedef struct vec_info {
  a_vec_info_ptr
		next;		/* Pointer to the next structure in a linked
				   list. */
  void 	        *array_ptr;	/* Pointer to array. */
  size_t        array_size;	/* Size of memory in the array. */
} vec_info;


#if !USE_PREFIX_FOR_ARRAY_ALLOC_INFO
static a_vec_info_ptr _head_vec_info = NULL;
				/* Pointer to the beginning of the linked list
				   of array information. */

static a_vec_info_ptr _free_vec_info = NULL;
				/* Pointer to a list of free array information
				   structures. */
#endif /* !USE_PREFIX_FOR_ARRAY_ALLOC_INFO */

#ifndef __EDG_IA64_ABI
EXTERN_C void _array_pointer_not_from_vec_new();
                               /* Function called when an invalid pointer that
                                  was not allocated by vec_new is passed
                                  to one of the vector handling routines. */
#endif /* ifndef __EDG_IA64_ABI */

/*
Increment a void* pointer by a given value.
*/
#define increment_ptr(ptr, incr) (ptr = ((void*)((char*)ptr + incr)))

#if EXCEPTION_HANDLING
static void add_vec_new_or_delete_eh_stack_entry
				(an_eh_stack_entry_ptr	    ehsep,
                                 an_array_alloc_eh_info_ptr aaehip,
			         a_boolean		    is_vec_new)
/*
Link an entry onto the EH stack that describes the vec_new or vec_delete
operation that is in process.
*/
{
  ehsep->next = __curr_eh_stack_entry;
  __curr_eh_stack_entry = ehsep;
  ehsep->kind = ehsek_vec_new_or_delete;
  ehsep->variant.array_alloc_eh_info = aaehip;
  aaehip->array_ptr                 = NULL;
  aaehip->number_of_elements        = 0;
  aaehip->element_size              = 0;
  aaehip->prefix_size               = 0;
  aaehip->elements_processed        = 0;
  aaehip->is_vec_new                = is_vec_new;
  aaehip->free_memory_on_cleanup    = FALSE;
  aaehip->destructor		    = NULL;
  aaehip->delete_routine	    = NULL;
  aaehip->is_two_arg	 	    = FALSE;
  aaehip->free_array                = NULL;
}  /* add_vec_new_or_delete_eh_stack_entry */
#endif /* EXCEPTION_HANDLING */


#if USE_PREFIX_FOR_ARRAY_ALLOC_INFO
/*
Structure used to record the size of an array that has been allocated.
Space for this structure is reserved at the beginning of the block of
memory allocated to hold the array.
*/
#ifndef __EDG_IA64_ABI
typedef struct an_alloc_prefix *an_alloc_prefix_ptr;
struct an_alloc_prefix {
  size_t	size;
			/* The size of the array, in bytes. */
  size_t	encoded_number_of_elements;
			/* The number of elements in the array.  This value
                           is encoded so that a zero value is not represented
			   as a zero.  This is done to help detect situations
			   in which the prefix has been overwritten. */
};

/*
Compute the size in bytes of the prefix to be allocated.  This must
be at least as large as an_alloc_prefix, but must also be a multiple
of the most strict alignment. */
size_t	__array_new_prefix_size =
		  ((sizeof(an_alloc_prefix) + MOST_STRICT_ALIGNMENT - 1) /
                               MOST_STRICT_ALIGNMENT) * MOST_STRICT_ALIGNMENT;
#else /* defined(__EDG_IA64_ABI) */
#if __EDG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES
/*
Define the type used for the array cookie.  The ARM EABI uses a variant
version of the mechanism in the IA-64 ABI.
*/
typedef struct an_alloc_prefix {
  size_t	element_size;
			/* The size of an element. */
  size_t	element_count;
			/* The number of elements in the array. */
} an_alloc_prefix;
#else /* !__EDG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES */
typedef size_t an_alloc_prefix;
#endif /* __EDG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES */
typedef an_alloc_prefix *an_alloc_prefix_ptr;
#endif /* defined(__EDG_IA64_ABI) */
#else /* !USE_PREFIX_FOR_ARRAY_ALLOC_INFO */
/*
No prefix is used in the alternate mode.
*/
size_t	__array_new_prefix_size = 0;
#endif /* USE_PREFIX_FOR_ARRAY_ALLOC_INFO */


static inline void* alloc_array(size_t		size,
                                size_t          prefix_size,
			        a_new_ptr	new_routine)
/*
Call the allocation routine to allocate the memory for the array.  Adjust
the size as needed to provide storage for the prefix information used to
save the array size.  The size of the prefix, if any, is given by
prefix_size.
*/
{
  void		*array_ptr;
  /* Increment the size to provide space for the prefix block. */
  size += prefix_size;
  /* Allocate the memory using the appropriate new routine.  If a pointer
     was provided by the caller, use that one.  Otherwise, use the
     one specified by the ABI being used. */
  if (new_routine == NULL) {
#if ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE
    array_ptr = operator new[](size);
#else /* !ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE */
    array_ptr = operator new(size);
#endif /* ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE */
  } else {
    array_ptr = (*new_routine)(size);
  }  /* if */
  if (array_ptr != NULL) {
    /* Return a pointer to the part of the array after the prefix. */
    array_ptr = (void*)(((char *)array_ptr) + prefix_size);
  }  /* if */
  return array_ptr;
}  /* alloc_array */


static void free_array(void*		array_ptr,
		       size_t		size,
                       size_t           prefix_size,
		       a_delete_ptr	delete_routine,
		       int		is_two_arg)
/*
Call the deallocation routine to free the memory for the array.  Adjust
the size as needed to provide storage for the prefix information used to
save the array size.  The size of the prefix (if any) is given by
prefix_size.
*/
{
  /* Increment the size to provide space for the prefix block. */
  size += prefix_size;
  /* Adjust the pointer to point to the start of the prefix. */
  array_ptr = (void*)(((char *)array_ptr) - prefix_size);
  /* Free the memory using the appropriate new routine.  If a pointer
     was provided by the caller, use that one.  Otherwise, use the
     one specified by the ABI being used. */
  if (delete_routine == NULL) {
#if ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE
    operator delete[](array_ptr);
#else /* !ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE */
    operator delete(array_ptr);
#endif /* ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE */
  } else {
    if (is_two_arg) {
      a_two_operand_delete_ptr two_op_delete_routine;
      two_op_delete_routine = (a_two_operand_delete_ptr)delete_routine;
      (*two_op_delete_routine)(array_ptr, size);
    } else {
      (*delete_routine)(array_ptr);
    }  /* if */
  }  /* if */
}  /* free_array */


#if !USE_PREFIX_FOR_ARRAY_ALLOC_INFO
/*ARGSUSED*/ /* <-- "number_of_elements" is only used when
                    USE_PREFIX_FOR_ARRAY_ALLOC_INFO is TRUE.
		    "element_size" is not used in that case. */
#else /* USE_PREFIX_FOR_ARRAY_ALLOC_INFO */
#ifdef __EDG_IA64_ABI
/*ARGSUSED*/ /* <-- "size" is not used in that case. */
#if !__EDG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES
/*ARGSUSED*/ /* <-- "element_size" is not used in that case. */
#endif /* !__EDG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES */
#else /* ifndef __EDG_IA64_ABI */
/*ARGSUSED*/ /* <-- "element_size" is not used in that case. */
#endif /* ifdef __EDG_IA64_ABI */
#endif /* USE_PREFIX_FOR_ARRAY_ALLOC_INFO */

static inline a_boolean record_array_alloc_info(void*	  array_ptr,
	   				        size_t	  size,
					        ptrdiff_t number_of_elements,
						size_t	  element_size)
/*
Record the size of the array so that it can be retrieved later using
the array pointer.  Returns TRUE if an error occurred and the size
could not be recorded.
*/
{
#if USE_PREFIX_FOR_ARRAY_ALLOC_INFO
  an_alloc_prefix_ptr	app;
#ifndef __EDG_IA64_ABI
  /* Get a pointer to the prefix information and fill in the fields.
     The number of elements is saved as the complement of the actual
     value.  This is done so that zeroing out the prefix (as might happen
     if the memory were overwritten) would not result in valid values. */
  app = (an_alloc_prefix_ptr)(((char *)array_ptr) - __array_new_prefix_size);
  app->size = size;
  app->encoded_number_of_elements = ~number_of_elements;
#else /* defined(__EDG_IA64_ABI) */
  app = ((an_alloc_prefix_ptr)array_ptr) - 1;
#if !__EDG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES
  *app = (an_alloc_prefix)number_of_elements;
#else /* __EDG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES */
  app->element_size = element_size;
  app->element_count = number_of_elements;
#endif /* !__EDG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES */
#endif /* defined(__EDG_IA64_ABI) */
  return FALSE;
#else /* !USE_PREFIX_FOR_ARRAY_ALLOC_INFO */
  a_vec_info_ptr	info_ptr;
  /* Allocate the needed memory and construct the "hidden" array
     information. */
  if (_free_vec_info != NULL) {
    /* Reuse a previously allocated structure. */
    info_ptr = _free_vec_info;
    _free_vec_info = info_ptr->next;
  } else {
    /* Allocate an array information structure from free memory. */
    info_ptr = (a_vec_info_ptr)malloc(sizeof(vec_info));
    if (info_ptr == NULL) {
      array_ptr = NULL;
    }  /* if */
  }  /* if */
  if (info_ptr != NULL) {
    info_ptr->next       = _head_vec_info;
    info_ptr->array_ptr  = array_ptr;
    info_ptr->array_size = size;
    _head_vec_info  = info_ptr;
  }  /* if */
  return array_ptr == NULL;
#endif /* USE_PREFIX_FOR_ARRAY_ALLOC_INFO */
}  /* record_array_alloc_info */


#if !USE_PREFIX_FOR_ARRAY_ALLOC_INFO
/*ARGSUSED*/ /* <-- "element_size" is only used when
                    USE_PREFIX_FOR_ARRAY_ALLOC_INFO is TRUE. */
#endif /* !USE_PREFIX_FOR_ARRAY_ALLOC_INFO */
static inline size_t get_array_size(void*	array_ptr,
			 	    size_t	element_size,
				    size_t	*number_of_elements)
/*
Return the array size saved when the array was allocated.  The size is
stored either in a prefix allocated immediately before the array or in
a separate data structure.  array_ptr points to the start of the array
whose size is to be determined.  element_size is the size of each element.
The number of elements in the array is returned in *number_of_elements.
*/
{
#if USE_PREFIX_FOR_ARRAY_ALLOC_INFO
  an_alloc_prefix_ptr	app;
  size_t		size;
#ifndef __EDG_IA64_ABI
  size_t		size_to_check;
  /* Get the size from the prefix.  Compute the size from the
     element count saved in the prefix and the element size passed
     by the caller.  Note that the number of elements is saved as the
     complement of the actual value.  If the two sizes do not agree,
     either the memory was not allocated by array_new, or the prefix was
     corrupted. */
  app = (an_alloc_prefix_ptr)(((char *)array_ptr) - __array_new_prefix_size);
  size = app->size;
  *number_of_elements = ~(app->encoded_number_of_elements);
  size_to_check = element_size * *number_of_elements;
  /* The array size is adjusted to always allocate at least a byte of
     storage for the array. */
  if (size_to_check == 0) size_to_check = 1;
  if (size != size_to_check) _array_pointer_not_from_vec_new();
#else /* defined(__EDG_IA64_ABI) */
  app = ((an_alloc_prefix_ptr)array_ptr) - 1;
#if __EDG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES
  *number_of_elements = app->element_count;
#else /* !__EDG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES */
  *number_of_elements = *app;
#endif /* __EDG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES */
  size = *number_of_elements * element_size;
#endif /* defined(__EDG_IA64_ABI) */
  return size;
#else /* !USE_PREFIX_FOR_ARRAY_ALLOC_INFO */
  a_vec_info_ptr        prev_ptr;
  a_vec_info_ptr 	info_ptr = NULL;
  size_t		size;
  /* Find the "hidden" information  for this array. */
  for (prev_ptr = NULL, info_ptr = _head_vec_info;
       (info_ptr != NULL) && (info_ptr->array_ptr != array_ptr);
       info_ptr = info_ptr->next) {
    prev_ptr = info_ptr;
  }  /* for */
  if (info_ptr == NULL) {
    /* This array was not allocated by vec_new, so we do not know the
       size.  Call a function that will abort.  The name should
       be sufficient to identify the nature of the problem to the user. */
     _array_pointer_not_from_vec_new();
  }  /* if */
  size = info_ptr->array_size;
  /* Unhook this array information from the linked list and add to the
     front of the free list. */
  if (prev_ptr == NULL) {
    /* This structure is on the beginning of the linked list. */
    _head_vec_info = info_ptr->next;
  } else {
    prev_ptr->next = info_ptr->next;
  }  /* if */
  info_ptr->next = _free_vec_info;
   _free_vec_info = info_ptr;
  return size;
#endif /* USE_PREFIX_FOR_ARRAY_ALLOC_INFO */
}  /* get_array_size */


/*ARGSUSED*/ /* <-- "dtor" is only used when EXCEPTION_HANDLING is TRUE. 
                    "prefix_size" is only used when
                    USE_PREFIX_FOR_ARRAY_ALLOC_INFO is true. */
static void *array_new_general(void                  *array_ptr,
                               ptrdiff_t	     number_of_elements,
                               size_t                element_size,
                               size_t                prefix_size,
                               void                  *src_array_ptr,
                               a_constructor_ptr     ctor,
                               a_destructor_ptr	     dtor,
		               a_new_ptr	     new_routine,
                               a_delete_ptr          delete_routine,
			       int		     is_two_arg,
                               a_boolean             zero_init)
/*
Allocate storage for an array, then call a constructor for each
element of the array.  If array_ptr is NULL, allocate the space for an
array of class objects (with number_of_elements elements each of size
element_size).  Also remember the size of the array in a
behind-the-scenes data structure so that it can be recalled at the
time of the corresponding vec_delete call.  If array_ptr is non-NULL,
it points to an already-allocated array.  If ctor is non-NULL, it
points to a constructor function to be called for each element of the
array (whether the array is allocated here or pre-allocated).  Return
the address of the array.

src_array_ptr points to the array to be copied when ctor points to a
copy constructor.

dtor is a pointer to the destructor for objects of the element type.
This is used by the exception handling mechanism for object cleanup
if an exception is thrown while the array is being constructed.
If there is no destructor then dtor is NULL and no cleanup is done.

delete_routine is a pointer to the delete routine to be used to deallocate the
space in the event that an exception is thrown during construction.
is_two_arg is TRUE if delete_routine refers to a two argument version of the
delete operator.  zero_init is TRUE if the memory should be cleared before
invoking constructors.

This routine needs to record the size of the array that was allocated so
that the size is known when the array is deallocated.  One of two means
of recording this information is used depending on the setting of the
configuration flag USE_PREFIX_FOR_ARRAY_ALLOC_INFO.  If this flag is TRUE,
a prefix structure is allocated as part of the array allocation, and this
prefix is used to store the size.  The pointer returned to the caller points
to the memory after the prefix block.  The prefix_size parameter specifies
the size of the prefix to be allocated.

When USE_PREFIX_FOR_ARRAY_ALLOC_INFO is FALSE, a linked list is used
to record the number of elements in the array.  Consequently, the
performance degrades if a large number of arrays are allocated.  For
this reason, the alternate mechanism is recommended for production
use.
*/
{
  size_t    array_size;
  ptrdiff_t i;
  void      *arr_ptr;

#if EXCEPTION_HANDLING
  an_eh_stack_entry		ehse;
  an_array_alloc_eh_info	aaehi;
  a_boolean			create_eh_stack_entry;
  a_boolean			free_memory_on_cleanup = array_ptr == NULL;

  /* An entry is created on the EH stack if the class for which the
     array is being created has a destructor, or if the object is
     dynamically allocated. */
  create_eh_stack_entry = dtor != NULL || array_ptr == NULL;
#endif /* EXCEPTION_HANDLING */
  if (array_ptr == NULL || prefix_size != 0) {
    a_boolean	err;
#if EXCEPTION_HANDLING && ABI_COMPATIBILITY_VERSION >= 406
    /* A run-time check is inserted during lowering to ensure that the
       multiplication below doesn't overflow.  If the number of elements
       is too small or too large std::bad_array_new_length is thrown. */
#ifdef __EDG_IA64_ABI
    /* The IA-64 ABI standard calls for a check for overflow here
       (though the front end has already performed this check). */
    if (element_size != 0 &&
        number_of_elements > ((size_t)(-1) - prefix_size)/element_size) {
      ABI_NAMESPACE::__cxa_throw_bad_array_new_length();
    }  /* if */
#endif /* defined(__EDG_IA64_ABI) */
#endif /* EXCEPTION_HANDLING && ABI_COMPATIBILITY_VERSION >= 406 */
    array_size = number_of_elements * element_size;
    /* Always allocate at least a byte of storage for the array to guarantee
       that the pointer returned to the caller (which points after the prefix)
       cannot be the same as a subsequently allocated object. */
    if (array_size == 0) array_size = 1;
    if (array_ptr == NULL) {
      /* Allocate the array if a pointer has not been supplied by the
         caller. */
      array_ptr = alloc_array(array_size, prefix_size, new_routine);
      if (array_ptr == NULL) {
        goto error_exit;
      }  /* if */
    }  /* if */
    /* Record the array size information so that the array can be properly
       freed later. */
    if (prefix_size != 0) {
      err = record_array_alloc_info(array_ptr, array_size, number_of_elements,
                                    element_size);
      if (err) goto error_exit;
    }  /* if */
#if ABI_COMPATIBILITY_VERSION >= 300
  } else if (zero_init) {
    array_size = number_of_elements * element_size;
#endif /* ABI_COMPATIBILITY_VERSION >= 300 */
  }  /* if */
#if ABI_COMPATIBILITY_VERSION >= 300
  if (zero_init) {
    __memzero(array_ptr, array_size);
  }  /* if */
#endif /* ABI_COMPATIBILITY_VERSION >= 300 */
#if EXCEPTION_HANDLING
  if (create_eh_stack_entry) {
    add_vec_new_or_delete_eh_stack_entry(&ehse, &aaehi, /*is_vec_new=*/TRUE);
    aaehi.free_memory_on_cleanup = free_memory_on_cleanup;
    aaehi.free_array             = free_array;
    aaehi.number_of_elements     = number_of_elements;
    aaehi.element_size           = element_size;
    aaehi.prefix_size            = prefix_size;
    aaehi.destructor		 = dtor;
    aaehi.delete_routine	 = delete_routine;
    aaehi.is_two_arg		 = is_two_arg;
    aaehi.array_ptr              = array_ptr;
#ifdef __EDG_IA64_ABI
    aaehi.terminate_immediately  = FALSE;
#endif /* defined(__EDG_IA64_ABI) */
  }  /* if */
#endif /* EXCEPTION_HANDLING */
  /* Call the constructor, if any, for each member of the array.  Note that
     there may be zero elements.  Cfront tacks on what appears to be eight
     additional NULL values to be used as the addresses of the first
     eight virtual base classes.  The EDG compiler generates a special wrapper
     for use by vec_new and doesn't need the additional arguments.  The
     additional arguments here allow cfront-generated vec_new calls to
     be used with this vec_new. */
  if (ctor != NULL) {
    for (i = 0, arr_ptr = array_ptr;
         i < number_of_elements;
         i++, increment_ptr(arr_ptr, element_size)) {
     if (src_array_ptr == NULL) {
       /* Call the default constructor. */
#if CFRONT_COMPATIBILITY_MODE
        a_cfront_constructor_ptr	cfront_ctor;
        cfront_ctor = (a_cfront_constructor_ptr)ctor;
        (*cfront_ctor)(arr_ptr, (void *)0, (void *)0, (void *)0, (void *)0,
                       (void *)0, (void *)0, (void *)0, (void *)0);
#else /* CFRONT_COMPATIBILITY_MODE */
        (*ctor)(arr_ptr); 
#endif /* CFRONT_COMPATIBILITY_MODE */
      } else {
        /* Call the copy constructor. */
        a_copy_constructor_ptr	cctor;
        cctor = (a_copy_constructor_ptr)ctor;
        (*cctor)(arr_ptr, src_array_ptr);
      }  /* if */
#if EXCEPTION_HANDLING
      if (dtor != NULL) {
        /* Update the counter of the number of elements processed in the
           EH stack entry. */
        aaehi.elements_processed++;
      }  /* if */
#endif /* EXCEPTION_HANDLING */
      /* Go to the next element in the source array when the constructor
         being called is a copy constructor. */
      if (src_array_ptr != NULL) increment_ptr(src_array_ptr, element_size);
    }  /* for */
  }  /* if */
#if EXCEPTION_HANDLING
  if (create_eh_stack_entry) {
    /* Unlink the vec_new EH stack entry. */
    __curr_eh_stack_entry = __curr_eh_stack_entry->next;
  }  /* if */
#endif /* EXCEPTION_HANDLING */
error_exit:
  /* Return the pointer to the array. */
  return array_ptr;
}  /* array_new_general */

#ifndef __EDG_IA64_ABI
#if ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE
EXTERN_C void *__array_new(an_element_count_param       number_of_elements,
                           size_t                       element_size,
                           a_constructor_ptr	 	ctor,
                           a_destructor_ptr	        dtor,
			   a_new_ptr		 	new_routine,
                           a_delete_ptr          	delete_routine,
			   int			 	is_two_arg)
/*
This entry point is used for operations that use class specific array
new and delete operators.  The new and delete routines are pointed to
by new_routine and delete_routine.  is_two_arg is TRUE if the delete
routine is one that requires two arguments.
*/
{
  return (array_new_general((void*)NULL, number_of_elements, element_size,
                            __array_new_prefix_size, (void*)NULL, ctor, dtor,
                            new_routine, delete_routine, is_two_arg,
                            /*zero_init=*/FALSE));
}  /* __array_new */
#endif /* ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE */


#if ABI_COMPATIBILITY_VERSION >= 234
EXTERN_C void *__placement_array_new(
			   void				*array_ptr,
			   an_element_count_param       number_of_elements,
                           size_t                       element_size,
                           a_constructor_ptr	 	ctor,
                           a_destructor_ptr	        dtor)
/*
This entry point is used for placement array new operations.  The actual
memory is allocated by a call to the appropriate new routine before
this routine is called.  This routine is used to record the array size
information and to call the constructor for each array element.
*/
{
  return (array_new_general(array_ptr, number_of_elements, element_size,
                            __array_new_prefix_size, (void*)NULL, ctor, dtor,
                            (a_new_ptr)NULL, (a_delete_ptr)NULL,
                            /*is_two_arg=*/FALSE, /*zero_init=*/FALSE));
}  /* __placement_array_new */
#endif /* ABI_COMPATIBILITY_VERSION >= 234 */



EXTERN_C void *__vec_new_eh(void                         *array_ptr,
                            an_element_count_param       number_of_elements,
                            size_t                       element_size,
                            a_constructor_ptr	 	 ctor,
                            a_destructor_ptr	         dtor)
/*
This entry point is used by code that uses exception handling for
new operations that do not involve the use of a class specific
operator new.
*/
{
  return (array_new_general(array_ptr, number_of_elements, element_size,
                            (array_ptr == NULL) ? __array_new_prefix_size : 0,
                            (void*)NULL, ctor, dtor,
                            (a_new_ptr)NULL, (a_delete_ptr)NULL,
                            /*is_two_arg=*/FALSE, /*zero_init=*/FALSE));
}  /* __vec_new_eh */


EXTERN_C void *__vec_new(void                         *array_ptr,
                         an_element_count_param       number_of_elements,
                         size_t                       element_size,
                         a_constructor_ptr            ctor)
/*
This is an entry point used for compatibility with code generated
before EH was supported.  This is similar to vec_new_eh, except that
no destructor pointer is provided.
*/
{
  return (array_new_general(array_ptr, number_of_elements, element_size,
                            (array_ptr == NULL) ? __array_new_prefix_size : 0,
                            (void*)NULL, ctor, (a_destructor_ptr)NULL, 
                            (a_new_ptr)NULL, (a_delete_ptr)NULL,
                            /*is_two_arg=*/FALSE, /*zero_init=*/FALSE));
}  /* __vec_new */


EXTERN_C void __vec_cctor_eh(void                       *array_ptr,
                             size_t                     number_of_elements,
                             size_t                     element_size,
                             a_copy_constructor_ptr	ctor,
                	     void                       *src_array_ptr,
                             a_destructor_ptr		dtor)
/*
This is an entry point to array_new_general used to call the copy
constructor for each element of an array.  The corresponding element of
the array pointed to by src_array_ptr is the source operand for the
copy constructor.  Because this runtime routine will only be called for
constructor initialization of member arrays, the number_of_elements
can never be zero.
*/
{
  (void)array_new_general(array_ptr, number_of_elements, element_size,
                          (array_ptr == NULL) ? __array_new_prefix_size : 0,
                          src_array_ptr, (a_constructor_ptr)ctor, dtor, 
                          (a_new_ptr)NULL, (a_delete_ptr)NULL,
                          /*is_two_arg=*/FALSE, /*zero_init=*/FALSE);
}  /* __vec_cctor_eh */


#if ABI_COMPATIBILITY_VERSION >= 300
EXTERN_C void *__array_new_zero(an_element_count_param number_of_elements,
                                size_t                 element_size,
                                a_constructor_ptr      ctor,
                                a_destructor_ptr       dtor,
                                a_new_ptr              new_routine,
                                a_delete_ptr           delete_routine,
                                int                    is_two_arg)
/*
This entry point is used for operations requiring value-initialization.
In such cases, memory is zeroed before calling a (default) constructor
on it.  See array_new_general for the meaning of the parameters.
*/
{
  return (array_new_general((void*)NULL, number_of_elements, element_size,
                            __array_new_prefix_size, (void*)NULL, ctor, dtor,
                            new_routine, delete_routine, is_two_arg,
                            /*zero_init=*/TRUE));
}  /* __array_new_zero */


EXTERN_C void *__placement_array_new_zero(
                                     void                   *array_ptr,
                                     an_element_count_param number_of_elements,
                                     size_t                 element_size,
                                     a_constructor_ptr      ctor,
                                     a_destructor_ptr       dtor)
/*
This entry point is used for placement array new operations requiring value-
initialization.  The actual memory is allocated by a call to the appropriate
new routine before this routine is called.  This routine is used to record
the array size information and to call the constructor for each array element.
*/
{
  return (array_new_general(array_ptr, number_of_elements, element_size,
                            __array_new_prefix_size, (void*)NULL, ctor, dtor,
                            (a_new_ptr)NULL, (a_delete_ptr)NULL,
                            /*is_two_arg=*/FALSE, /*zero_init=*/TRUE));
}  /* __placement_array_new_zero */


EXTERN_C void *__vec_new_eh_zero(void                   *array_ptr,
                                 an_element_count_param number_of_elements,
                                 size_t                 element_size,
                                 a_constructor_ptr      ctor,
                                 a_destructor_ptr       dtor)
/*
This entry point is used by code that uses exception handling for
new operations that do not involve the use of a class specific
operator new but that require memory to be zeroed before the default
constructor is called.
*/
{
  return (array_new_general(array_ptr, number_of_elements, element_size,
                            (array_ptr == NULL) ? __array_new_prefix_size : 0,
                            (void*)NULL, ctor, dtor, (a_new_ptr)NULL, 
                            (a_delete_ptr)NULL, /*is_two_arg=*/FALSE, 
                            /*zero_init=*/TRUE));
}  /* __vec_new_eh_zero */
#endif /* ABI_COMPATIBILITY_VERSION >= 300 */
#else /* defined(__EDG_IA64_ABI) */
EXTERN_C void *ABI_NAMESPACE::__cxa_vec_new(
                                         size_t            number_of_elements,
                                         size_t            element_size,
                                         size_t            prefix_size,
                                         a_constructor_ptr ctor,
                                         a_destructor_ptr  dtor)
/*
The entry point used for ordinary array new.
*/
{
  return (array_new_general((void *)NULL, (ptrdiff_t)number_of_elements,
                            element_size, prefix_size, (void *)NULL,
                            ctor, dtor, (a_new_ptr)NULL, (a_delete_ptr)NULL,
                            /*is_two_arg=*/FALSE, /*zero_init=*/FALSE));
}  /* __cxa_vec_new */


EXTERN_C void *ABI_NAMESPACE::__cxa_vec_new2(
                                         size_t            number_of_elements,
                                         size_t            element_size,
                                         size_t            prefix_size,
                                         a_constructor_ptr ctor,
                                         a_destructor_ptr  dtor,
                                         a_new_ptr         new_routine,
                                         a_delete_ptr      delete_routine)
/*
The entry point used for array new with class-specific new and delete
operators.
*/
{
  return (array_new_general((void *)NULL, (ptrdiff_t)number_of_elements,
                            element_size, prefix_size, (void *)NULL,
                            ctor, dtor, new_routine, delete_routine,
                            /*is_two_arg=*/FALSE, /*zero_init=*/FALSE));
}  /* __cxa_vec_new2 */


EXTERN_C void *ABI_NAMESPACE::__cxa_vec_new3(
                                  size_t                   number_of_elements,
                                  size_t                   element_size,
                                  size_t                   prefix_size,
                                  a_constructor_ptr        ctor,
                                  a_destructor_ptr         dtor,
                                  a_new_ptr                new_routine,
                                  a_two_operand_delete_ptr delete_routine)
/*
The entry point used for array new with class-specific new and delete
operators where the delete operator, if any, takes two arguments.
*/
{
  return (array_new_general((void *)NULL, (ptrdiff_t)number_of_elements,
                            element_size, prefix_size, (void *)NULL,
                            ctor, dtor, new_routine, 
                            (a_delete_ptr)delete_routine,
                            /*is_two_arg=*/TRUE, /*zero_init=*/FALSE));
}  /* __cxa_vec_new3 */


#endif /* defined(__EDG_IA64_ABI) */

#ifndef __EDG_IA64_ABI
/*ARGSUSED*/ /* terminate_immediately is used only in the IA-64 ABI. */
#endif /* ifndef __EDG_IA64_ABI */
static void array_delete_general(void                *array_ptr,
                                 ptrdiff_t           number_of_elements_param,
                                 size_t              element_size,
                                 size_t              prefix_size,
                                 a_destructor_ptr    dtor,
				 int		     delete_flag,
                                 a_delete_ptr	     delete_routine,
				 int		     is_two_arg,
                                 int                 terminate_immediately)
                                     
/*
Call a destructor for each element of an array, then delete the storage
for the array.  array_ptr points to the array, which has number_of_elements
elements each of size element_size.  If number_of_elements is -1, use the
size stored by vec_new at the time of allocation of this array.  In that case,
the size of the prefix is given by prefix_size.  If array_ptr is NULL, this
routine does nothing and returns.  If dtor is non-NULL, it points to a
destructor function to be called for each element of the array.  If
delete_flag is TRUE, the storage for the array is deallocated after the
destruction; number_of_elements must be -1 for that case.  If
terminate_immediately is TRUE, std::terminate will be called if any of the
destructor elements throws an exception; otherwise, the remainder of the
elements will be destroyed and the exception will be rethrown.
*/
{
  size_t                i;
  void                  *arr_ptr;
  size_t		array_size = 0;
  size_t		number_of_elements = number_of_elements_param;

  /* If the address of the array is NULL, do nothing. */
  if (array_ptr != NULL ) {
#if EXCEPTION_HANDLING
    an_eh_stack_entry		ehse;
    an_array_alloc_eh_info	aaehi;
    add_vec_new_or_delete_eh_stack_entry(&ehse, &aaehi, 
                                         /*is_vec_new=*/FALSE);
    aaehi.free_memory_on_cleanup = delete_flag;
    aaehi.free_array             = free_array;
    aaehi.array_ptr              = array_ptr;
    aaehi.number_of_elements     = number_of_elements;
    aaehi.element_size           = element_size;
    aaehi.prefix_size            = prefix_size;
    aaehi.destructor		 = dtor;
    aaehi.delete_routine	 = delete_routine;
    aaehi.is_two_arg		 = is_two_arg;
#ifdef __EDG_IA64_ABI
    aaehi.terminate_immediately  = terminate_immediately;
#endif /* defined(__EDG_IA64_ABI) */
#endif /* EXCEPTION_HANDLING */
    /* Determine the number of elements in the array, if unknown.  Note that
       number_of_elements_param is used because that value is signed. */
    if (number_of_elements_param == -1 && prefix_size != 0) {
      /* Determine the number of elements from the memory allocation size. */
      array_size = get_array_size(array_ptr, element_size,
                                  &number_of_elements);
    }  /* if */
#if EXCEPTION_HANDLING
    aaehi.number_of_elements     = number_of_elements;
#endif /* EXCEPTION_HANDLING */

    /* Call the destructor, if specified, on each element in the array, in
       reverse order. */
    if (dtor != NULL) {
      for (i = 0,
           arr_ptr = (void*)((char*)array_ptr +
                             (number_of_elements - 1) * element_size);
           i < number_of_elements;
           i++, increment_ptr(arr_ptr, -(int)(element_size))) {
#if EXCEPTION_HANDLING
        /* Update the counter of the number of elements processed in the
           EH stack entry.  This is incremented before the destructor is
           called so that, should an exception occur, we won't try
           destroying this element again. */
        aaehi.elements_processed++;
#endif /* EXCEPTION_HANDLING */
#ifndef __EDG_IA64_ABI
        /* Call the destructor with 0x2 - whole object = TRUE
                                    0x1 - delete object = FALSE. */
        (*dtor)(arr_ptr, 0x2 /*whole object = TRUE, delete = FALSE*/);
#else /* defined(__EDG_IA64_ABI) */
        (*dtor)(arr_ptr);
#endif /* defined(__EDG_IA64_ABI) */
      }  /* for */
    }  /* if */
#if EXCEPTION_HANDLING
    /* Unlink the vec_new EH stack entry.  This is unlinked before the memory
       for the array is freed.  If an exception occurs during the free
       it should just be handled by the normal mechanism. */
    __curr_eh_stack_entry = __curr_eh_stack_entry->next;
#endif /* EXCEPTION_HANDLING */
    /* Delete the array, if requested. */
    if (delete_flag) {
      free_array(array_ptr, array_size, prefix_size, delete_routine,
                 is_two_arg);
    }  /* if */
  }  /* if */
}  /* array_delete_general */

#ifndef __EDG_IA64_ABI
/*ARGSUSED*/ /* <-- "unused" is unused. */
EXTERN_C void __vec_delete(void                   *array_ptr,
                           an_element_count_param number_of_elements,
                           size_t                 element_size,
                           a_destructor_ptr       dtor,
                           int                    delete_flag,
                           int                    unused)
/*
Entry point used for the normal vector delete operation.  The unused
parameter is there for cfront compatibility.
*/
{
  array_delete_general(array_ptr, number_of_elements, element_size, 
                       (number_of_elements == -1) ? 
                                                 __array_new_prefix_size : 0,
                       dtor, delete_flag, (a_delete_ptr)NULL,
                       /*is_two_arg=*/FALSE, /*terminate_immediately=*/FALSE);
}  /* __vec_delete */


#if ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE
EXTERN_C void __array_delete(void                   *array_ptr,
                             an_element_count_param number_of_elements,
                             size_t                 element_size,
                             a_destructor_ptr       dtor,
                             a_delete_ptr	    delete_routine,
			     int		    is_two_arg)
/*
This entry point is used for operations that use class specific array
new and delete operators.  The delete routine is pointed to
by delete_routine.  is_two_arg is TRUE if the delete routine is one that
requires two arguments.
*/
{
  array_delete_general(array_ptr, number_of_elements, element_size, 
                       (number_of_elements == -1) ? 
                                                __array_new_prefix_size : 0,
                       dtor, /*delete_flag=*/TRUE, delete_routine, 
                       is_two_arg, /*terminate_immediately=*/FALSE);
}  /* __array_delete */
#endif /* ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE */
#else /* defined(__EDG_IA64_ABI) */
EXTERN_C void ABI_NAMESPACE::__cxa_vec_delete(void             *array_ptr,
                                              size_t           element_size,
                                              size_t           prefix_size,
                                              a_destructor_ptr dtor)
/*
The entry point for ordinary array delete.
*/
{
  array_delete_general(array_ptr, /*number_of_elements=*/-1, element_size,
                       prefix_size, dtor, /*delete_flag=*/TRUE,
                       (a_delete_ptr)NULL, /*is_two_arg=*/FALSE,
                       /*terminate_immediately=*/FALSE);
}  /* __cxa_vec_delete */


EXTERN_C void ABI_NAMESPACE::__cxa_vec_delete2(void             *array_ptr,
                                               size_t           element_size,
                                               size_t           prefix_size,
                                               a_destructor_ptr dtor,
                                               a_delete_ptr     delete_routine)
/*
The entry point for array delete with a class-specific operator delete.
*/
{
  array_delete_general(array_ptr, /*number_of_elements=*/-1, element_size,
                       prefix_size, dtor, /*delete_flag=*/TRUE,
                       delete_routine, /*is_two_arg=*/FALSE,
                       /*terminate_immediately=*/FALSE);
}  /* __cxa_vec_delete2 */


EXTERN_C void ABI_NAMESPACE::__cxa_vec_delete3(
                                     void                     *array_ptr,
                                     size_t                   element_size,
                                     size_t                   prefix_size,
                                     a_destructor_ptr         dtor,
                                     a_two_operand_delete_ptr delete_routine)
/*
The entry point for array delete with a two-argument class-specific operator
delete.
*/
{
  array_delete_general(array_ptr, /*number_of_elements=*/-1, element_size,
                       prefix_size, dtor, /*delete_flag=*/TRUE,
                       (a_delete_ptr)delete_routine, /*is_two_arg=*/TRUE,
                       /*terminate_immediately=*/FALSE);
}  /* __cxa_vec_delete3 */


EXTERN_C void ABI_NAMESPACE::__cxa_vec_cleanup(
					void             *array_ptr,
					size_t           number_of_elements,
					size_t           element_size,
					a_destructor_ptr dtor)
/*
The entry point used to invoke the destructor on an array of objects,
and to invoke terminate if a destructor exits with a throw.
*/
{
  if (dtor != NULL) {
    array_delete_general(array_ptr, number_of_elements, element_size,
                         /*prefix_size=*/0, dtor, /*delete_flag=*/FALSE,
                         (a_delete_ptr)NULL, /*is_two_arg=*/FALSE,
                         /*terminate_immediately=*/TRUE);
  }  /* if */
}  /* __cxa_vec_cleanup */

#if ABI_COMPATIBILITY_VERSION >= 406

EXTERN_C void ABI_NAMESPACE::__cxa_throw_bad_array_new_length(void)
/*
This routine is called by lowered code or the run-time library if it has been
determined that the overall size of an array new operation is invalid for
some reason (i.e., overflow, less than zero, too small for initializer list).
*/
{
#if EXCEPTION_HANDLING
  __throw_bad_array_new_length();
#endif /* EXCEPTION_HANDLING */
}  /* __cxa_throw_bad_array_new_length */

#endif /* ABI_COMPATIBILITY_VERSION >= 406 */

#endif /* defined(__EDG_IA64_ABI) */

#ifndef __EDG_IA64_ABI                                     
EXTERN_C void _array_pointer_not_from_vec_new()
/*
This routine is used when a pointer that was not created by vec_new is
passed to one other vector handling routines that needs to get the size
from the information created by vec_new.  This routine simply aborts.
The name is intended to describe the nature of the problem to the user
*/
{
  __abort_execution(ec_array_not_from_vec_new);
}
#endif /* ifndef __EDG_IA64_ABI */
