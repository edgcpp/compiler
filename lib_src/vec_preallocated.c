/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

#include "basics.h"
#include "runtime.h"
#include "eh.h"
#pragma hdrstop
#include "vec_newdel.h"
#include "memzero.h"

#if EXCEPTION_HANDLING
static void add_preallocated_eh_entry(an_eh_stack_entry_ptr    ehsep,
                                      an_array_alloc_eh_info_ptr info,
                                      void                     *array_ptr,
                                      size_t                   count,
                                      size_t                   element_size,
                                      a_destructor_ptr         dtor,
                                      a_boolean                is_vec_new)
/*
Link ehsep to the exception-handling stack and initialize info for the
preallocated array at array_ptr, which has count elements of element_size.
dtor is the element destructor, and is_vec_new indicates construction rather
than destruction.
*/
{
  ehsep->next = __curr_eh_stack_entry;
  __curr_eh_stack_entry = ehsep;
  ehsep->kind = ehsek_vec_new_or_delete;
  ehsep->variant.array_alloc_eh_info = info;
  info->array_ptr = array_ptr;
  info->number_of_elements = count;
  info->element_size = element_size;
  info->prefix_size = 0;
  info->elements_processed = 0;
  info->is_vec_new = is_vec_new;
  info->free_memory_on_cleanup = FALSE;
  info->destructor = dtor;
  info->delete_routine = NULL;
  info->is_two_arg = FALSE;
  info->free_array = NULL;
#ifdef __EDG_IA64_ABI
  info->terminate_immediately = FALSE;
#endif /* defined(__EDG_IA64_ABI) */
}  /* add_preallocated_eh_entry */


EXTERN_C void __cleanup_vec_new_or_delete(an_eh_stack_entry_ptr ehsep)
/*
Use ehsep to clean up an interrupted array construction or destruction.
Allocated arrays carry their deallocator in the stack entry; preallocated
arrays do not.
*/
{
  an_array_alloc_eh_info_ptr info = ehsep->variant.array_alloc_eh_info;
  size_t count;
  size_t i;

#ifdef __EDG_IA64_ABI
  if (info->terminate_immediately) {
    __call_terminate();
  }  /* if */
#endif /* defined(__EDG_IA64_ABI) */
  if (info->is_vec_new) {
    count = info->elements_processed;
  } else {
    count = info->number_of_elements - info->elements_processed;
  }  /* if */
  if (info->destructor != NULL) {
    for (i = count; i != 0; --i) {
      void *element = (char *)info->array_ptr + (i - 1) * info->element_size;
#ifndef __EDG_IA64_ABI
      (*info->destructor)(element, 0x2);
#else /* defined(__EDG_IA64_ABI) */
      (*info->destructor)(element);
#endif /* defined(__EDG_IA64_ABI) */
    }  /* for */
  }  /* if */
  if (info->free_memory_on_cleanup) {
    check_assertion(info->free_array != NULL);
    (*info->free_array)(info->array_ptr,
                        info->element_size * info->number_of_elements,
                        info->prefix_size, info->delete_routine,
                        info->is_two_arg);
  }  /* if */
}  /* __cleanup_vec_new_or_delete */
#endif /* EXCEPTION_HANDLING */


EXTERN_C void *__edg_vec_ctor(void              *array_ptr,
                              size_t            number_of_elements,
                              size_t            element_size,
                              a_constructor_ptr ctor,
                              a_destructor_ptr  dtor,
                              int               zero_init)
/*
Construct number_of_elements objects of element_size in preallocated storage
at array_ptr using ctor, zeroing the storage first if zero_init is TRUE.  Use
dtor to clean up constructed elements if a constructor throws.  Return
array_ptr.
*/
{
  size_t i;
#if EXCEPTION_HANDLING
  an_eh_stack_entry ehse;
  an_array_alloc_eh_info info;
#endif /* EXCEPTION_HANDLING */

  if (zero_init) __memzero(array_ptr, number_of_elements * element_size);
#if EXCEPTION_HANDLING
  if (dtor != NULL) {
    add_preallocated_eh_entry(&ehse, &info, array_ptr, number_of_elements,
                              element_size, dtor, /*is_vec_new=*/TRUE);
  }  /* if */
#endif /* EXCEPTION_HANDLING */
  if (ctor != NULL) {
    for (i = 0; i < number_of_elements; ++i) {
      void *element = (char *)array_ptr + i * element_size;
#if CFRONT_COMPATIBILITY_MODE
      a_cfront_constructor_ptr cfront_ctor = (a_cfront_constructor_ptr)ctor;
      (*cfront_ctor)(element, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
#else /* CFRONT_COMPATIBILITY_MODE */
      (*ctor)(element);
#endif /* CFRONT_COMPATIBILITY_MODE */
#if EXCEPTION_HANDLING
      if (dtor != NULL) ++info.elements_processed;
#endif /* EXCEPTION_HANDLING */
    }  /* for */
  }  /* if */
#if EXCEPTION_HANDLING
  if (dtor != NULL) __curr_eh_stack_entry = ehse.next;
#endif /* EXCEPTION_HANDLING */
  return array_ptr;
}  /* __edg_vec_ctor */


EXTERN_C void __edg_vec_dtor(void             *array_ptr,
                             size_t           number_of_elements,
                             size_t           element_size,
                             a_destructor_ptr dtor)
/*
Use dtor to destroy number_of_elements objects of element_size in preallocated
storage at array_ptr, in reverse order.
*/
{
  size_t i;
#if EXCEPTION_HANDLING
  an_eh_stack_entry ehse;
  an_array_alloc_eh_info info;
#endif /* EXCEPTION_HANDLING */

  if (array_ptr == NULL) return;
#if EXCEPTION_HANDLING
  add_preallocated_eh_entry(&ehse, &info, array_ptr, number_of_elements,
                            element_size, dtor, /*is_vec_new=*/FALSE);
#endif /* EXCEPTION_HANDLING */
  if (dtor != NULL) {
    for (i = number_of_elements; i != 0; --i) {
      void *element = (char *)array_ptr + (i - 1) * element_size;
#if EXCEPTION_HANDLING
      ++info.elements_processed;
#endif /* EXCEPTION_HANDLING */
#ifndef __EDG_IA64_ABI
      (*dtor)(element, 0x2);
#else /* defined(__EDG_IA64_ABI) */
      (*dtor)(element);
#endif /* defined(__EDG_IA64_ABI) */
    }  /* for */
  }  /* if */
#if EXCEPTION_HANDLING
  __curr_eh_stack_entry = ehse.next;
#endif /* EXCEPTION_HANDLING */
}  /* __edg_vec_dtor */


static void copy_preallocated_array(void                   *array_ptr,
                                    void                   *src_array_ptr,
                                    size_t                 number_of_elements,
                                    size_t                 element_size,
                                    a_copy_constructor_ptr ctor,
                                    a_destructor_ptr       dtor)
/*
Copy number_of_elements objects of element_size from src_array_ptr to
preallocated storage at array_ptr using ctor.  Use dtor to clean up
constructed elements if a copy constructor throws.
*/
{
  size_t i;
#if EXCEPTION_HANDLING
  an_eh_stack_entry ehse;
  an_array_alloc_eh_info info;

  if (dtor != NULL) {
    add_preallocated_eh_entry(&ehse, &info, array_ptr, number_of_elements,
                              element_size, dtor, /*is_vec_new=*/TRUE);
  }  /* if */
#endif /* EXCEPTION_HANDLING */
  if (ctor != NULL) {
    for (i = 0; i < number_of_elements; ++i) {
      void *dest = (char *)array_ptr + i * element_size;
      void *src = (char *)src_array_ptr + i * element_size;
      (*ctor)(dest, src);
#if EXCEPTION_HANDLING
      if (dtor != NULL) ++info.elements_processed;
#endif /* EXCEPTION_HANDLING */
    }  /* for */
  }  /* if */
#if EXCEPTION_HANDLING
  if (dtor != NULL) __curr_eh_stack_entry = ehse.next;
#endif /* EXCEPTION_HANDLING */
}  /* copy_preallocated_array */


#ifndef __EDG_IA64_ABI
EXTERN_C void __edg_vec_cctor(void                   *array_ptr,
                              size_t                 number_of_elements,
                              size_t                 element_size,
                              a_copy_constructor_ptr ctor,
                              void                   *src_array_ptr,
                              a_destructor_ptr       dtor)
/*
Copy the array at src_array_ptr into preallocated storage at array_ptr.
The count, element size, copy constructor, and destructor are supplied by
the caller.
*/
{
  copy_preallocated_array(array_ptr, src_array_ptr, number_of_elements,
                          element_size, ctor, dtor);
}  /* __edg_vec_cctor */
#else /* defined(__EDG_IA64_ABI) */
EXTERN_C void ABI_NAMESPACE::__cxa_vec_cctor(
                                    void                   *array_ptr,
                                    void                   *src_array_ptr,
                                    size_t                 number_of_elements,
                                    size_t                 element_size,
                                    a_copy_constructor_ptr ctor,
                                    a_destructor_ptr       dtor)
/*
Copy the array at src_array_ptr into preallocated storage at array_ptr.
The count, element size, copy constructor, and destructor are supplied by
the caller.
*/
{
  copy_preallocated_array(array_ptr, src_array_ptr, number_of_elements,
                          element_size, ctor, dtor);
}  /* __cxa_vec_cctor */
#endif /* defined(__EDG_IA64_ABI) */


#ifdef __EDG_IA64_ABI
EXTERN_C void ABI_NAMESPACE::__cxa_vec_ctor(
                                          void              *array_ptr,
                                          size_t            number_of_elements,
                                          size_t            element_size,
                                          a_constructor_ptr ctor,
                                          a_destructor_ptr  dtor)
/*
Construct number_of_elements objects of element_size in preallocated storage
at array_ptr using ctor.  Use dtor to clean up if a constructor throws.
*/
{
  (void)__edg_vec_ctor(array_ptr, number_of_elements, element_size, ctor, dtor,
                       /*zero_init=*/FALSE);
}  /* __cxa_vec_ctor */


EXTERN_C void ABI_NAMESPACE::__cxa_vec_dtor(
                                           void             *array_ptr,
                                           size_t           number_of_elements,
                                           size_t           element_size,
                                           a_destructor_ptr dtor)
/*
Use dtor to destroy number_of_elements objects of element_size at array_ptr.
*/
{
  __edg_vec_dtor(array_ptr, number_of_elements, element_size, dtor);
}  /* __cxa_vec_dtor */
#endif /* defined(__EDG_IA64_ABI) */
