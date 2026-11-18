/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

Declarations for runtime routines for vector new() and delete() functionality.

*/

#ifndef VEC_NEWDEL_H
#define VEC_NEWDEL_H

#include "runtime.h"

#ifndef __EDG_IA64_ABI

EXTERN_C void *__vec_new_eh(void                         *array_ptr,
                            an_element_count_param       number_of_elements,
                            size_t                       element_size,
                            a_constructor_ptr	 	 ctor,
                            a_destructor_ptr	         dtor);

EXTERN_C void *__vec_new(void                         *array_ptr,
                         an_element_count_param       number_of_elements,
                         size_t                       element_size,
                         a_constructor_ptr            ctor);

EXTERN_C void __vec_delete(void                   *array_ptr,
                           an_element_count_param number_of_elements,
                           size_t                 element_size,
                           a_destructor_ptr       dtor,
                           int                    delete_flag,
                           int                    /*unused_arg*/);

EXTERN_C void _array_pointer_not_from_vec_new();

#endif /* ifdef __EDG_IA64_ABI */

/*
Type name used by the exception handling mechanism to point to the
structure used to maintain information about array new and delete
operations that are in process.
*/
typedef struct an_array_alloc_eh_info *an_array_alloc_eh_info_ptr;

struct an_array_alloc_eh_info {
  void* array_ptr;      /* Pointer to the memory allocated for the array. */
  ptrdiff_t number_of_elements;
                        /* Total number of elements in the array. */
  size_t element_size;  /* Size of each element. */
  size_t prefix_size;   /* Size of the array prefix. */
  size_t elements_processed;
                        /* Number of elements constructed or destructed. */
  a_boolean is_vec_new; /* TRUE for construction, FALSE for destruction. */
  a_boolean free_memory_on_cleanup;
                        /* TRUE if cleanup must free the array storage. */
  a_destructor_ptr destructor;
                        /* Destructor for each element of the array. */
  a_delete_ptr delete_routine;
                        /* Deallocation routine, or NULL for the default. */
  a_boolean is_two_arg; /* TRUE for a two-argument deallocation routine. */
#ifdef __EDG_IA64_ABI
  a_boolean terminate_immediately;
                        /* TRUE if a destructor throw requires termination. */
#endif /* defined(__EDG_IA64_ABI) */
};

EXTERN_C void __edg_vec_dtor(void              *array_ptr,
                             size_t            number_of_elements,
                             size_t            element_size,
                             a_destructor_ptr  dtor);

EXTERN_C void *__edg_vec_ctor(void              *array_ptr,
                              size_t            number_of_elements,
                              size_t            element_size,
                              a_constructor_ptr ctor,
                              a_destructor_ptr  dtor,
                              int               zero_init);

#ifndef __EDG_IA64_ABI
EXTERN_C void __edg_vec_cctor(void                   *array_ptr,
                              size_t                 number_of_elements,
                              size_t                 element_size,
                              a_copy_constructor_ptr ctor,
                              void                   *src_array_ptr,
                              a_destructor_ptr       dtor);
#endif /* ifndef __EDG_IA64_ABI */

#endif /* ifndef VEC_NEWDEL_H */
