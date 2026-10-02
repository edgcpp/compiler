/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

util.h -- General utility components (mostly templates).

*/

#ifndef EDG_UTIL_H
#define EDG_UTIL_H 1

#include <new>

/* Predeclare some functions normally declared by mem_manage.h so that util.h
   can be used in utility programs. */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

inline char *alloc_fe(sizeof_t size);

inline char *alloc_fe_var_size(sizeof_t size,
                               sizeof_t *actual_size);

inline void free_fe(a_void_ptr   ptr,
                    sizeof_t     size);

inline void free_fe_var_size(a_void_ptr ptr,
                             sizeof_t   size);

extern char *alloc_general(sizeof_t size);

extern void free_general(a_void_ptr ptr,
                         sizeof_t   size);

/* Predeclare FE_allocator. */
template<typename an_Elem>
struct FE_allocator;

NORETURN extern void insufficient_address_space();

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#ifndef EDG_HEADER_UTIL_H
#include "header_util.h"
#endif /* ifndef EDG_HEADER_UTIL_H */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

typedef decltype(nullptr) a_nullptr;


/*
A template alias used to form array types of pointer to a_const_char.
*/
template<size_t size>
using a_const_char_ptr_array = a_const_char *[size];


/*
Enable_if<cond, T> is invalid (causing deduction failure) if cond is FALSE.
Otherwise, it produces T.
*/
template<a_boolean cond, typename a_Thing>
struct Enable_if_helper;

/*lint -esym(758,Enable_if_helper)*/
template<typename a_Thing>
struct Enable_if_helper<true, a_Thing> {
  typedef a_Thing a_thing;
};  /* Enable_if_helper<true, a_Thing> */

template<a_boolean cond, typename a_Thing>
using Enable_if = typename Enable_if_helper<cond, a_Thing>::a_thing;


/*
Is_same<A, B, C> is invalid (causing deduction failure) if A is not the same
type as B.  Otherwise, it produces C.

By default, C is the same type as A.
*/
template<typename a_Type_A, typename a_Type_B, typename a_Ret_type>
struct Is_same_helper;

template<typename a_Type_A, typename a_Ret_type>
struct Is_same_helper<a_Type_A, a_Type_A, a_Ret_type> {
  typedef a_Ret_type ret_ty;
};  /* Is_same_helper<a_type_A, a_type_A> */

template<typename a_Type_A, typename a_Type_B, typename a_Ret_type = a_Type_A>
using Is_same = typename Is_same_helper<a_Type_A, a_Type_B,a_Ret_type>::ret_ty;


/*
A helper class used to determine if an integer type is signed.  If the type is
signed Is_signed_helper::value will be TRUE; otherwise, it will be FALSE.
*/
template<typename an_Integral_type>
struct Is_signed_helper {
  static constexpr a_boolean
                value = (an_Integral_type)-1 < (an_Integral_type)0;
};  /* Is_signed_helper */


/*
This type is commonly used as a base class for exposing C++ type_traits like
template details.
*/
template<typename a_Type, a_Type a_Value>
struct Integral_constant {
  static constexpr a_Type value = a_Value;
};  /* Integral_constant */

namespace detail {

#if ((defined(__clang__)) || \
     (defined(__GNUC__) && __GNUC__ > 5)) && !defined(__EDG__)

/*
An implementation of Is_trivially_copyable that relies on the host compiler
having the builtin __is_trivially_copyable.

Note: bool is used in place of a_boolean to match the standard library
implementations that use this builtin.  Thus, this provides maximum
compatibility with the __is_trivially_copyable builtin (i.e., to avoid possible
warnings about a conversion from bool).
*/
template<typename a_Type>
struct Is_trivially_copyable_builtin_impl :
              public Integral_constant<bool, __is_trivially_copyable(a_Type)> {
};  /* Is_trivially_copyable_builtin_impl */

#endif /* (defined(__clang__) || (defined(__GNUC__) && __GNUC__ > 5) && ... */

/*
An implementation of Is_trivially_copyable that relies on specializations to
remain independent of any underlying compiler or standard library
implementation.

Note: bool, true, and false are used in place of a_boolean, TRUE, and FALSE for
maximum compatibility when comparing with the __is_trivially_copyable builtin
implementation (i.e., Is_trivially_copyable_builtin_impl).
*/
template<typename a_Type>
struct Is_trivially_copyable_edg_impl : Integral_constant<bool, false> {
};  /* Is_trivially_copyable_edg_impl */

template<typename a_Type>
struct Is_trivially_copyable_edg_impl<a_Type*> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_copyable_edg_impl */

#define MARK_TRIVIALLY_COPYABLE(type)                                         \
  template<>                                                                  \
  struct Is_trivially_copyable_edg_impl<type> :                               \
                                              Integral_constant<bool, true> { \
  };  /* Is_trivially_copyable_edg_impl */

MARK_TRIVIALLY_COPYABLE(bool)
#if !defined(_MSC_VER) || defined(_NATIVE_WCHAR_T_DEFINED)
/* MSVC has a mode, enabled via the /Zc:wchar_t- command-line option, in
   which wchar_t is not a distinct type but is treated as a typedef for
   unsigned short.  An explicit specialization for wchar_t will conflict
   with the specialization for unsigned short in that mode. */
MARK_TRIVIALLY_COPYABLE(wchar_t)
#endif /* !defined(_MSC_VER) || defined(_NATIVE_WCHAR_T_DEFINED) */
MARK_TRIVIALLY_COPYABLE(char16_t)
MARK_TRIVIALLY_COPYABLE(char32_t)
MARK_TRIVIALLY_COPYABLE(char)
MARK_TRIVIALLY_COPYABLE(signed char)
MARK_TRIVIALLY_COPYABLE(unsigned char)
MARK_TRIVIALLY_COPYABLE(short)
MARK_TRIVIALLY_COPYABLE(unsigned short)
MARK_TRIVIALLY_COPYABLE(int)
MARK_TRIVIALLY_COPYABLE(unsigned int)
MARK_TRIVIALLY_COPYABLE(long)
MARK_TRIVIALLY_COPYABLE(unsigned long)
MARK_TRIVIALLY_COPYABLE(long long)
MARK_TRIVIALLY_COPYABLE(unsigned long long)

#if HOST_HAS_INT128_EXTENSIONS
MARK_TRIVIALLY_COPYABLE(__int128_t)
MARK_TRIVIALLY_COPYABLE(__uint128_t)
#endif /* HOST_HAS_INT128_EXTENSIONS */

#undef MARK_TRIVIALLY_COPYABLE

/*
This is an implementation of Is_trivially_copyable that uses the EDG
specialization-based trivial copyable implementation.  However, it additionally
checks against the host compiler's builtin implementation as a safety check on
the EDG based specializations.

This compromise allows the front end to work on any platform while maintaining
maximum performance and safety.

Note this implementation uses bool to match the builtin semantics regardless of
the definition of a_boolean in the front end configuration.
*/
template<typename a_Type>
struct Is_trivially_copyable_helper {
  static constexpr a_boolean value =
                      (a_boolean)Is_trivially_copyable_edg_impl<a_Type>::value;
#if ((defined(__clang__)) || \
     (defined(__GNUC__) && __GNUC__ > 5)) && !defined(__EDG__)
  /* This check ensures that for GCC and Clang implementations (which provide
     the __is_trivially_copyable builtin), the front end's specializations
     match the compiler's expectations for Is_trivially_copyable. */
  static_assert(Is_trivially_copyable_edg_impl<a_Type>::value ==
                Is_trivially_copyable_builtin_impl<a_Type>::value,
                "the EDG and builtin Is_trivially_copyable implementations "
                "have diverged");
#endif /* (defined(__clang__) || (defined(__GNUC__) && __GNUC__ > 5) && ... */
};  /* Is_trivially_copyable_helper */

#if (defined(__clang__) && __clang_major__ >= 5) && !defined(__EDG__)

/*
An implementation of Is_trivially_destructible that relies on the host compiler
having the builtin __is_trivially_destructible.

Note: bool is used in place of a_boolean to match the standard library
implementations that use this builtin.  Thus, this provides maximum
compatibility with the __is_trivially_destructible builtin (i.e., to avoid
possible warnings about a conversion from bool).
*/
template<typename a_Type>
struct Is_trivially_destructible_builtin_impl :
          public Integral_constant<bool, __is_trivially_destructible(a_Type)> {
};  /* Is_trivially_destructible_builtin_impl */

#endif /* (defined(__clang__) && __clang_major__ >= 5) && !defined(__EDG__) */

/*
An implementation of Is_trivially_destructible that relies on specializations
to remain independent of any underlying compiler or standard library
implementation.

Note: bool, true, and false are used in place of a_boolean, TRUE, and FALSE for
maximum compatibility when comparing with the __is_trivially_destructible
builtin implementation (i.e., Is_trivially_destructible_builtin_impl).
*/
template<typename a_Type>
struct Is_trivially_destructible_edg_impl : Integral_constant<bool, false> {
};  /* Is_trivially_destructible_edg_impl */

template<typename a_Type>
struct Is_trivially_destructible_edg_impl<a_Type*> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_destructible_edg_impl */

#define MARK_TRIVIALLY_DESTRUCTIBLE(type)                                     \
  template<>                                                                  \
  struct Is_trivially_destructible_edg_impl<type> :                           \
                                              Integral_constant<bool, true> { \
  };  /* Is_trivially_destructible_edg_impl */

MARK_TRIVIALLY_DESTRUCTIBLE(bool)
#if !defined(_MSC_VER) || defined(_NATIVE_WCHAR_T_DEFINED)
/* MSVC has a mode, enabled via the /Zc:wchar_t- command-line option, in
   which wchar_t is not a distinct type but is treated as a typedef for
   unsigned short.  An explicit specialization for wchar_t will conflict
   with the specialization for unsigned short in that mode. */
MARK_TRIVIALLY_DESTRUCTIBLE(wchar_t)
#endif /* !defined(_MSC_VER) || defined(_NATIVE_WCHAR_T_DEFINED) */
MARK_TRIVIALLY_DESTRUCTIBLE(char16_t)
MARK_TRIVIALLY_DESTRUCTIBLE(char32_t)
MARK_TRIVIALLY_DESTRUCTIBLE(char)
MARK_TRIVIALLY_DESTRUCTIBLE(signed char)
MARK_TRIVIALLY_DESTRUCTIBLE(unsigned char)
MARK_TRIVIALLY_DESTRUCTIBLE(short)
MARK_TRIVIALLY_DESTRUCTIBLE(unsigned short)
MARK_TRIVIALLY_DESTRUCTIBLE(int)
MARK_TRIVIALLY_DESTRUCTIBLE(unsigned int)
MARK_TRIVIALLY_DESTRUCTIBLE(long)
MARK_TRIVIALLY_DESTRUCTIBLE(unsigned long)
MARK_TRIVIALLY_DESTRUCTIBLE(long long)
MARK_TRIVIALLY_DESTRUCTIBLE(unsigned long long)

#if HOST_HAS_INT128_EXTENSIONS
MARK_TRIVIALLY_DESTRUCTIBLE(__int128_t)
MARK_TRIVIALLY_DESTRUCTIBLE(__uint128_t)
#endif /* HOST_HAS_INT128_EXTENSIONS */

#undef MARK_TRIVIALLY_DESTRUCTIBLE

/*
This is an implementation of Is_trivially_destructible that uses the EDG
specialization-based trivial destruction implementation.  However, it
additionally checks against the host compiler's builtin implementation as a
safety check on the EDG based specializations.

This compromise allows the front end to work on any platform while maintaining
maximum performance and safety.

Note this implementation uses bool to match the builtin semantics regardless of
the definition of a_boolean in the front end configuration.
*/
template<typename a_Type>
struct Is_trivially_destructible_helper {
  static constexpr a_boolean value =
                  (a_boolean)Is_trivially_destructible_edg_impl<a_Type>::value;
#if (defined(__clang__) && __clang_major__ >= 5) && !defined(__EDG__)
  /* This check ensures that for GCC and Clang implementations (which provide
     the __is_trivially_copyable builtin), the front end's specializations
     match the compiler's expectations for Is_trivially_destructible. */
  static_assert(Is_trivially_destructible_edg_impl<a_Type>::value ==
                Is_trivially_destructible_builtin_impl<a_Type>::value,
                "the EDG and builtin Is_trivially_destructible "
                "implementations have diverged");
#endif /* (defined(__clang__) && __clang_major__ >= 5) && !defined(__EDG__) */
};  /* Is_trivially_destructible_helper */

}  /* namespace detail */

/*
Is_trivially_copyable<a_Type>::value is TRUE when the given type is trivially
copyable; otherwise, it's FALSE.
*/
template<typename a_Type>
using Is_trivially_copyable = detail::Is_trivially_copyable_helper<a_Type>;

/*
Is_trivially_destructible<a_Type>::value is TRUE when the given type is
trivially destructible; otherwise, it's FALSE.
*/
template<typename a_Type>
using Is_trivially_destructible =
                              detail::Is_trivially_destructible_helper<a_Type>;

/*
Overload_priority is a helper type for controlling overload priority by using
nested parent types to provide a gradient of conversions (priorities).  This
results in Overload_priority<N> having a higher priority than
Overload_priority<N - 1>.

Callers should use the highest Overload_priority<N> object to allow resolution
of all represented priorities.

Overload_priority can be used to guide overload resolution when multiple
candidates have otherwise equivalent overload resolution priority.  This is
useful particularly when SFINAE is being used to enable or disable one or more
candidates during overload resolution.

As an example, consider one or more function templates that have conditionally
conflicting overloading resolutions:

  // I want this to be called as fallback logic.
  // All things equal, this is the least important candidate.
  template<typename a_Type>
  auto foo(a_Type value) -> Foo;

  // I want this to be called when value has a member "m1" of type Bar.
  // All things equal, this is a more important candidate.
  template<typename a_Type>
  auto foo(a_Type value) -> Is_same<value.m1, Bar>;

  // I want this to be called when value has a member "m2" of type Bar.
  // All things equal, this is the most important candidate.
  template<typename a_Type>
  auto foo(a_Type value) -> Is_same<value.m2, Bar>;

These functions as written have an undecidable overload resolution when
instantiated with "a_Type" that has a member "m1" and/or "m2" of type "Bar".
Overload_priority can be used to resolve this and apply the intended candidate
resolution.

An example rewrite of the above functions using Overload_priority:

  // I want this to be called as fallback logic.
  // All things equal, this is the least important candidate.
  template<typename a_Type>
  auto foo(a_Type value, Overload_priority<0>) -> Foo;

  // I want this to be called when value has a member "m1" of type Bar.
  // All things equal, this is a more important candidate.
  template<typename a_Type>
  auto foo(a_Type value, Overload_priority<1>) -> Is_same<value.m1, Bar>;

  // I want this to be called when value has a member "m2" of type Bar.
  // All things equal, this is the most important candidate.
  template<typename a_Type>
  auto foo(a_Type value, Overload_priority<2>) -> Is_same<value.m2, Bar>;

An API that relies on Overload_priority will typically be wrapped behind an
interface that makes that aspect invisible. For example:

  template<typename a_Type>
  auto foo(a_Type value) -> auto
    { return foo(value, Overload_priority<2>())); }

Thus, creating the following API:

   struct Baz1 {
   };
   foo(Baz1()); // Calls the first function template.

   struct Baz2 {
     Bar m1;
   };
   foo(Baz2(...)); // Calls the second function template.

   struct Baz3 {
     Bar m2;
   };
   foo(Baz3(...)); // Calls the third function template.

   struct Baz4 {
     Bar m1;
     Bar m2;
   };
   foo(Baz4(...)); // Calls the third function template.

*/
template<int a_Depth>
struct Overload_priority : Overload_priority<a_Depth - 1> {
};  /* Overload_priority */

template<>
struct Overload_priority<0> {
};  /* Overload_priority<0> */


/*
Remove_ref<T> produces T if T is not a reference type, or the type underlying
the reference type otherwise.
*/
template<typename an_Object>
struct Remove_ref_helper {
  typedef an_Object an_object;
};  /* Remove_ref_helper */

/*lint -esym(758,Remove_ref_helper)*/
template<typename an_Object>
struct Remove_ref_helper<an_Object&> {
  typedef an_Object an_object;
};  /* Remove_ref_helper<an_Object&> */

/*lint -esym(758,Remove_ref_helper)*/
template<typename an_Object>
struct Remove_ref_helper<an_Object&&> {
  typedef an_Object an_object;
};  /* Remove_ref_helper<an_Object&&> */

template<typename an_Object>
using Remove_ref = typename Remove_ref_helper<an_Object>::an_object;


/*
dummy_val<T> is not meant to be evaluated.  It is a convenience function to
produce a value of type T in unevaluated operands, or, if T is a reference
type, a glvalue of the type underlying the reference.
*/
template<typename an_Object>
an_Object dummy_val();


/*
Value_for_ptr<T> for a pointer-like type T produces the type pointed to.
*/
template<typename a_Ptr>
using Value_for_ptr = Remove_ref<decltype(*dummy_val<a_Ptr>())>;

#ifdef __EDG__
/* Don't warn on noexcept if exceptions are disabled. */
#pragma diag_suppress 540
#endif /* ifdef __EDG__ */
template<typename an_Object>
INLINE an_Object&& fwd(Remove_ref<an_Object>&  arg) noexcept
/*
This function should only be applied to "forwarding references".  It is used
to forward parameters.  For example:

    template<a_Thing> void f(a_Thing &&p) {
      g(fwd<a_Thing>(p));
    }

If f is called with an rvalue, a_Thing will be deduced to a non-reference type
and fwd<a_Thing>(p) will produce an xvalue.  If f is called with an lvalue,
a_Thing will be deduced to an lvalue reference type, and fwd<a_Thing>(p) will
pass through the lvalue.
*/
{
  return (an_Object&&)arg;
}  /* fwd */


template<typename a_Ptr>
INLINE Value_for_ptr<a_Ptr>&& move_from(a_Ptr  p_object)
/*
Return *p_object as an xvalue, so that it can be moved from.
*/
{
  return (Value_for_ptr<a_Ptr>&&)*p_object;
}  /* move_from */


template<typename a_Ptr, typename ...an_Arg_pack>
INLINE void construct(a_Ptr          p_object,
                      an_Arg_pack&&  ...args)
/*
Construct *p_object with the given arguments.
*/
{
  typedef Value_for_ptr<a_Ptr> an_object;
  /*lint -e1556*/
  ::new((void*)p_object) an_object(fwd<an_Arg_pack>(args)...);
}  /* construct */


template<typename a_Ptr, typename a_Functor>
void functional_init(a_Ptr        p_object,
                     a_Functor&&  fn)
/*
Initialize *p_object with the return value of fn().  Usually fn() should return
a prvalue and the initialization can occur without copy/move (through copy
elision).
*/
{
  typedef Value_for_ptr<a_Ptr> an_object;
  ::new((void*)p_object) an_object(fn());
}  /* functional_init */


template<typename a_Ptr>
INLINE void destroy(a_Ptr  p_object)
/*
Destroy the given object.
*/
{
  typedef Value_for_ptr<a_Ptr> an_object;
  p_object->~an_object();
}  /* destroy */


template<typename a_Ptr>
void swap_at(a_Ptr  p1,
             a_Ptr  p2)
/*
Swap the values pointed to by p1 and p2.
*/
{
BEGIN_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED
  Value_for_ptr<a_Ptr>  tmp = move_from(p1);
  *p1 = move_from(p2);
  *p2 = move_from(&tmp);
END_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED
}  /* swap_at */


/*
This type can be used to reverse a random access iterator.
*/
template<typename a_Base_iter>
struct Reverse_iter {
  using an_iterator = Reverse_iter<a_Base_iter>;

  Reverse_iter(a_Base_iter it)
    : base_it(it)
    {}

  auto operator*() -> decltype(*a_Base_iter())
    { return *(this->base_it); }

  an_iterator& operator++()
    { --this->base_it; return *this; }
  an_iterator& operator--()
    { ++this->base_it; return *this; }

  an_iterator operator+(int adjustment)
    { an_iterator tmp = *this; tmp.base_it - adjustment; return tmp; }
  an_iterator operator-(int adjustment)
    { an_iterator tmp = *this; tmp.base_it + adjustment; return tmp; }

  a_boolean operator==(an_iterator other)
    { return this->base_it == other.base_it; }
  a_boolean operator!=(an_iterator other)
    { return !(*this == other); }

  a_Base_iter base() const
    { return this->base_it; }
private:
  a_Base_iter   base_it;
                        /* The underlying iterator that's being reversed. */
};  /* Reverse_iter */


template<typename an_Object>
void reverse_array(an_Object  *arr,
                   size_t     length)
/*
Reverse the length elements in the given array (or sub-array).
*/
{
  if (length > 1) {
    an_Object  *left = arr, *right = arr+(length-1);
    for (; left < right; ++left, --right) {
      swap_at(left, right);
    }  /* for */
  }  /* if */
}  /* reverse_array */


template<typename a_List_elem>
a_List_elem* reverse_simple_list(a_List_elem  *list)
/*
list points to a singly-linked list of elements connected through an accessible
"next" pointer field.  Reverse the list and return a pointer to the new start
of the list.  list can be NULL.
*/
{
  a_List_elem  *new_list = NULL, *next;

  while (list) {
    next = list->next;
    list->next = new_list;
    new_list = list;
    list = next;
  }
  return new_list;
}  /* reverse_simple_list */


template<typename a_List_elem>
a_List_elem** get_last_simple_list_link(a_List_elem  **p_list)
/*
p_list is non-NULL and *p_list points to a possibly empty singly-linked list,
whose elements are connected through accessible "next" pointer fields.  Return
a pointer to the last such "next" field (or p_list itself if there are none).
*/
{
  while (*p_list != NULL) {
    p_list = &(*p_list)->next;
  }  /* while */
  return p_list;
}  /* get_last_simple_list_link */


template<typename a_List_elem>
a_boolean simple_list_has_cycle(a_List_elem  *list)
/*
Return TRUE if the given linked list (whose elements are connected through
accessible "next" pointer fields) contains a cycle.
*/
{
  a_boolean  result = FALSE;

  if (list != NULL && list->next != NULL) {
    /* Iterate a "fast" pointer twice as often as a "slow" pointer.  If the
       list contains a cycle, the former will catch up with the latter.
       Otherwise, the "fast" pointer will reach NULL first. */
    a_List_elem  *slow = list, *fast = list->next;
    for (;;) {
      fast = fast->next;
      if (fast == NULL) break;
      if (fast == slow) {
        result = TRUE;
        break;
      }  /* if */
      fast = fast->next;
      if (fast == NULL) break;
      if (fast == slow) {
        result = TRUE;
        break;
      }  /* if */
      slow = slow->next;
    }  /* for */
  }  /* if */
  return result;
}  /* simple_list_has_cycle */


template<typename a_List_elem, typename a_Predicate,
         template<typename> class Deleter = FE_allocator>
INLINE void delete_from_simple_list_if(a_List_elem          **head,
                                       a_List_elem          **tail,
                                       a_Predicate          predicate_fn,
                                       Deleter<a_List_elem> deleter = {})
/*
Given a predicate function that accepts a value of a_List_elem* type and
returns a boolean, apply the predicate function to all elements (*head through
*tail) and delete any elements where the function returns TRUE.
*/
{
  a_List_elem *cursor = *head;

  /* If this assertion fails, the given tail was not the real tail of the
     list. */
  check_assertion(*head == NULL || (*head)->prev == NULL);
  *head = *tail = NULL;
  while (cursor != NULL) {
    if (predicate_fn(cursor)) {
      /* Drop and delete the element from the rewritten list. */
      a_List_elem *next_cursor = cursor->next;

      deleter.delete_object(&cursor);
      cursor = next_cursor;
    } else {
      /* Include the element in the rewritten list. */
      if (*head == NULL) {
        *head = cursor;
        *tail = cursor;
      } else {
        (*tail)->next = cursor;
        *tail = cursor;
      }  /* if */
      cursor = cursor->next;
    }  /* if */
  }  /* while */
  if (*tail != NULL) {
    (*tail)->next = NULL;
  }  /* if */
}  /* delete_from_simple_list_if */


template<typename a_List_elem, typename a_Predicate,
         template<typename> class Deleter = FE_allocator>
INLINE void delete_from_double_list_if(a_List_elem          **head,
                                       a_List_elem          **tail,
                                       a_Predicate          predicate_fn,
                                       Deleter<a_List_elem> deleter = {})
/*
Given a predicate function that accepts a value of a_List_elem* type and
returns a boolean, apply the predicate function to all elements (*head through
*tail) and delete any elements where the function returns TRUE.
*/
{
  a_List_elem *cursor = *head;

  /* If this assertion fails, the given head was not the real head or the given
     tail was not the real tail of the list. */
  check_assertion((*head == NULL || (*head)->prev == NULL) &&
                  (*tail == NULL || (*tail)->next == NULL));
  *head = *tail = NULL;
  while (cursor != NULL) {
    if (predicate_fn(cursor)) {
      /* Drop and delete the element from the rewritten list. */
      a_List_elem *next_cursor = cursor->next;

      deleter.delete_object(&cursor);
      cursor = next_cursor;
    } else {
      /* Include the element in the rewritten list. */
      if (*head == NULL) {
        *head = cursor;
        *tail = cursor;
        (*head)->prev = NULL;
      } else {
        (*tail)->next = cursor;
        cursor->prev = *tail;
        *tail = cursor;
      }  /* if */
      cursor = cursor->next;
    }  /* if */
  }  /* while */
  if (*tail != NULL) {
    (*tail)->next = NULL;
  }  /* if */
}  /* delete_from_double_list_if */


template<typename an_Object_type, typename an_Array>
INLINE void copy_construct_element(an_Array             &dest_array,
                                   const an_Object_type &elem,
                                   size_t               num_copies)
/*
Create the given number of copies of the element at the destination.

Note: an_Array must represent all elements to be created as a contiguous memory
block to use this interface.
*/
{
  for (size_t i = 0; i < num_copies; ++i) {
    new (dest_array + i) an_Object_type(elem);
  }  /* for */
}  /* copy_construct_element */


template<typename, typename an_Array>
INLINE void copy_construct_element(an_Array &dest_array,
                                   char     elem,
                                   size_t   num_copies)
/*
Create the given number of copies of the given character at the destination.

Note: an_Array must represent all elements to be created as a contiguous memory
block to use this interface.
*/
{
  memset(&(dest_array[0]), elem, num_copies);
}  /* copy_construct_element */


template<typename an_Object_type, typename an_Array_A, typename an_Array_B>
INLINE Enable_if<!Is_trivially_copyable<an_Object_type>::value, void>
copy_construct_elements(an_Array_A       &dest_array,
                        const an_Array_B &src_array,
                        size_t           num_to_copy)
/*
Copy the given number of non-trivially copyable elements from the source
array-like type to the destination array-like type.

Note: both an_Array_A and an_Array_B must represent all elements to be copied
as contiguous memory blocks to use this interface.
*/
{
  for (size_t i = 0; i < num_to_copy; ++i) {
    new (dest_array + i) an_Object_type(src_array[i]);
  }  /* for */
}  /* copy_construct_elements */


template<typename an_Object_type, typename an_Array_A, typename an_Array_B>
INLINE Enable_if<Is_trivially_copyable<an_Object_type>::value, void>
copy_construct_elements(an_Array_A       &dest_array,
                        const an_Array_B &src_array,
                        size_t           num_to_copy)
/*
Copy the given number of trivially copyable elements from the source array-like
type to the destination array-like type.

Note: both an_Array_A and an_Array_B must represent all elements to be copied
as contiguous memory blocks to use this interface.
*/
{
  size_t num_bytes = num_to_copy * sizeof(an_Object_type);

  if (num_bytes > PTRDIFF_MAX) {
     /* Newer versions of GCC detect under -Wall when memcpy is called with a
        size exceeding the maximum object size.  This warning is triggered for
        some cases of (highly unlikely) user input driven allocations.

        To prevent this warning from being issued (and protect against these
        unlikely cases) the byte count is checked explicitly before calling
        memcpy. */
    insufficient_address_space();
  }  /* if */
  (void)memcpy(&(dest_array[0]), &(src_array[0]), num_bytes);
}  /* copy_construct_elements */


template<typename an_Object_type, typename an_Array,
         template<typename> class Allocator>
INLINE an_Object_type* new_copy_of_elements(
                                         const an_Array            &src_array,
                                         size_t                    num_to_copy,
                                         Allocator<an_Object_type> a)
/*
Copy the given number of elements from the source array-like type to a new
dynamically-allocated array allocated via the given allocator.  Return the
newly-allocated array.

Note: the source array must represent all elements to be copied as contiguous
memory blocks to use this interface.
*/
{
  an_Object_type *dest_array = a.alloc(num_to_copy).start;

  copy_construct_elements<an_Object_type>(dest_array, src_array, num_to_copy);
  return dest_array;
}  /* new_copy_of_elements */


template<template<typename> class Allocator>
INLINE char* new_copy_of_string(a_const_char    *src_str,
                                Allocator<char> a)
/*
Copy the given null-terminated string to a new dynamically-allocated array
allocated via the given allocator.  Return the newly-allocated string.
*/
{
  size_t str_len = strlen(src_str);

  return new_copy_of_elements(src_str, str_len + 1, a);
}  /* new_copy_of_string */


template<typename an_Object_type, typename an_Array>
INLINE Enable_if<!Is_trivially_destructible<an_Object_type>::value, void>
destroy_elements(an_Array &array,
                 size_t   num_to_destroy)
/*
Destroy the given number of non-trivially destructible elements at the given
array starting position.

Note: an_Array must represent all elements to be destroyed as a contiguous
memory block to use this interface.
*/
{
  for (size_t i = 0; i < num_to_destroy; ++i) {
    destroy(&array[i]);
  }  /* for */
}  /* destroy_elements */


template<typename an_Object_type, typename an_Array>
INLINE Enable_if<Is_trivially_destructible<an_Object_type>::value, void>
destroy_elements(ARG_UNUSED an_Array &array,
                 ARG_UNUSED size_t   num_to_destroy)
/*
Destroy the given number of trivially destructible elements at the given array
starting position.

Note: an_Array must represent all elements to be destroyed as a contiguous
memory block to use this interface.
*/
{
  /* No op */
}  /* destroy_elements */


template<typename an_Object_type, typename an_Array_A, typename an_Array_B>
INLINE Enable_if<!Is_trivially_copyable<an_Object_type>::value, void>
move_elements(an_Array_A &dest_array,
              an_Array_B &src_array,
              size_t     num_to_move)
/*
Move the given number of non-trivially copyable elements from the source
array-like type to the destination array-like type.

Note: both an_Array_A and an_Array_B must represent all elements to be copied
as contiguous memory blocks to use this interface.
*/
{
  a_boolean move_left = (&dest_array[0]) < (&src_array[0]);

  if (move_left) {
    for (size_t i = 0; i < num_to_move; ++i) {
      construct(&dest_array[i], move_from(&src_array[i]));
      destroy(&src_array[i]);
    }  /* for */
  } else {
    for (size_t i = num_to_move; i > 0; --i) {
      construct(&dest_array[i - 1], move_from(&src_array[i - 1]));
      destroy(&src_array[i - 1]);
    }  /* for */
  }  /* if */
}  /* move_elements */


template<typename an_Object_type, typename an_Array_A, typename an_Array_B>
INLINE Enable_if<Is_trivially_copyable<an_Object_type>::value, void>
move_elements(an_Array_A &dest_array,
              an_Array_B &src_array,
              size_t     num_to_move)
/*
Move the given number of trivially copyable elements from the source array-like
type to the destination array-like type.

Note: both an_Array_A and an_Array_B must represent all elements to be copied
as contiguous memory blocks to use this interface.
*/
{
  size_t num_bytes = num_to_move * sizeof(an_Object_type);

  if (num_bytes > PTRDIFF_MAX) {
     /* Newer versions of GCC detect under -Wall when memmove is called with a
        size exceeding the maximum object size.  This warning is triggered for
        some cases of (highly unlikely) user input driven allocations.

        To prevent this warning from being issued (and protect against these
        unlikely cases) the byte count is checked explicitly before calling
        memmove. */
    insufficient_address_space();
  }  /* if */
  (void)memmove(&(dest_array[0]), &(src_array[0]), num_bytes);
}  /* move_elements */


/*lint -e{1537}*/
template<typename a_Ptr>
struct Ptr_with_flag {
  /* An iterator and boolean value combined.  This is meant to be returnable
     through registers and therefore does not include constructors or
     destructors.  Instead, it's just a pair of data members.  The function
     ptr_with_flag should be used to construct an object of this type (with
     type deduction as a bonus).  The case where a_Ptr is a native pointer
     could potentially be optimized by encoding the flag in the pointer
     value. */
  typedef a_Ptr a_ptr;
  typedef Value_for_ptr<a_ptr> a_value;
  auto operator->() const -> a_ptr
    { return this->ptr_value; }
  auto  operator*() const -> a_value&
    { return *this->ptr_value; }
  auto ptr() const -> a_ptr
    { return this->ptr_value; }
  auto flagged() const -> a_boolean
    { return this->flag_value; }

  a_ptr		ptr_value;
			/* Embedded pointer. */
  a_boolean	flag_value;
			/* Embedded flag. */
};  /* Ptr_with_flag */

namespace detail {

template<typename a_Ptr>
struct Is_trivially_copyable_edg_impl<Ptr_with_flag<a_Ptr>> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_copyable_edg_impl */

template<typename a_Ptr>
struct Is_trivially_destructible_edg_impl<Ptr_with_flag<a_Ptr>> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_destructible_edg_impl */

}  /* namespace detail */

template<typename a_Ptr>
INLINE Ptr_with_flag<a_Ptr> ptr_with_flag(a_Ptr      ptr,
                                          a_boolean  flag)
/*
Return a Ptr_with_flag initialized with the given values.
*/
{
  return Ptr_with_flag<a_Ptr>{ ptr, flag };
}  /* ptr_with_flag */


template<typename an_Elem>
struct Allocation {
  /* A representation for the result of an allocation.  This is meant to be
     returnable through registers and therefore does not include constructors
     or destructors.  Instead, it's just a pair of immutable data members. */
  typedef an_Elem an_elem;
  an_elem* const
		start;
			/* Pointer to the first allocated element.  If NULL,
			   n_allocated must be zero. */
  const size_t
		n_bytes_allocated;
			/* Number of allocated bytes. */
};  /* Allocation */

namespace detail {

template<typename a_Type>
struct Is_trivially_copyable_edg_impl<Allocation<a_Type>> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_copyable_edg_impl */

}  /* namespace detail */

/* Forward declaration of delete_fe. */
template<typename an_Object>
INLINE void delete_fe(an_Object **p);

/*
A general allocator for front end memory and deleter for front end allocated
objects.
*/
template<typename an_Elem>
struct FE_allocator {
  typedef an_Elem an_elem;
  typedef Allocation<an_elem> an_allocation;
  typedef FE_allocator<an_elem> an_allocator;
  typedef FE_allocator<an_elem> a_deallocator;
  /* Allocator concept. */
  INLINE static auto alloc(size_t n) -> an_allocation;
  INLINE static auto replace_alloc(an_allocation  a,
                                   size_t         new_capacity,
                                   size_t         n_to_move)
                     -> an_allocation;
  INLINE static auto move_alloc(ARG_UNUSED an_allocator  &src,
                                an_allocation            src_alloc,
                                ARG_UNUSED size_t        n_to_move)
                     -> an_allocation
    { return src_alloc; }
  INLINE static void dealloc(an_allocation allocation);
  /* Deleter concept. */
  INLINE static void delete_object(an_Elem **elem)
    { delete_fe(elem); }
};  /* FE_allocator */


template<typename an_Elem>
auto FE_allocator<an_Elem>::alloc(size_t n) -> an_allocation
/*
Allocate at least n elements of type an_Elem and return the resulting
allocation (which reflects the actual number of allocated elements).
*/
{
  sizeof_t n_bytes_allocated;
  char     *addr;

  if (n <= 1) {
    n_bytes_allocated = sizeof(an_elem);
    addr = alloc_fe(sizeof(an_elem));
  } else {
    addr = alloc_fe_var_size(n * sizeof(an_elem), &n_bytes_allocated);
  }  /* if */
  return an_allocation{(an_elem*)addr, n_bytes_allocated};
}  /* FE_allocator::alloc */


template<typename an_Elem>
auto FE_allocator<an_Elem>::replace_alloc(an_allocation a,
                                          size_t        new_capacity,
                                          size_t        n_to_move)
            -> an_allocation
/*
Replace the given allocation -- which was allocated by the same allocator -- by
a new one with at least new_capacity elements.  The first n_to_move elements in
the original allocation must be initialized and are moved to the start of the
new allocation.
*/
{
  an_allocation result = FE_allocator<an_Elem>::alloc(new_capacity);
  an_elem       *old_start = a.start,
                *new_start = result.start;

  move_elements<an_Elem>(new_start, old_start, n_to_move);
  FE_allocator<an_Elem>::dealloc(a);
  return result;
}  /* FE_allocator::replace_alloc */


template<typename an_Elem>
void FE_allocator<an_Elem>::dealloc(an_allocation a)
/*
Release the given allocation -- which was allocated by the same allocator.
The caller is responsible for ensuring the allocation contains no live
objects.
*/
{
  /* Note that free_fe will correctly handle a null allocation. */
  if (a.start == NULL || a.n_bytes_allocated == sizeof(an_Elem)) {
    free_fe((void*)a.start, a.n_bytes_allocated);
  } else {
    free_fe_var_size(a.start, a.n_bytes_allocated);
  }  /* if */
}  /* FE_allocator::dealloc */


template<typename an_Object, typename ...an_Arg_pack>
INLINE an_Object *new_fe(an_Arg_pack&& ...args)
/*
Allocate in front-end memory and construct an object of type an_Object with
the constructor arguments specified by args.  Return a pointer to the object.
*/
{
  an_Object  *p = (an_Object*)alloc_fe(sizeof(an_Object));

  construct(p, fwd<an_Arg_pack>(args)...);
  return p;
}  /* new_fe */


template<typename an_Object>
INLINE void delete_fe(an_Object **p)
/*
Destroy and delete an object of type an_Object that was allocated in front end
memory.  The value of *p will be set to NULL.
*/
{
  if (*p != NULL) {
    destroy(*p);
    free_fe(*p, sizeof(an_Object));
    *p = NULL;
  }  /* if */
}  /* delete_fe */


template<typename an_Elem>
struct General_allocator {
  /* A general allocator for general memory (i.e., as allocated by
     alloc_general in mem_manage.c). */
  typedef an_Elem an_elem;
  typedef Allocation<an_elem> an_allocation;
  typedef General_allocator<an_elem> an_allocator;
  typedef General_allocator<an_elem> a_deallocator;
  INLINE static auto alloc(size_t n) -> an_allocation;
  INLINE static auto replace_alloc(an_allocation  a,
                                   size_t         new_capacity,
                                   size_t         n_to_move)
                     -> an_allocation;
  INLINE static auto move_alloc(ARG_UNUSED an_allocator  &src,
                                an_allocation            src_alloc,
                                ARG_UNUSED size_t        n_to_move)
                     -> an_allocation
    { return src_alloc; }
  INLINE static void dealloc(an_allocation allocation);
};  /* General_allocator */


template<typename an_Elem>
auto General_allocator<an_Elem>::alloc(size_t n) -> an_allocation
/*
Allocate at least n elements of type an_Elem and return the resulting
allocation (which reflects the actual number of allocated elements).
*/
{
  sizeof_t n_bytes = n * sizeof(an_elem);

  return an_allocation{(an_elem*)alloc_general(n_bytes), n_bytes};
}  /* General_allocator::alloc */


template<typename an_Elem>
auto General_allocator<an_Elem>::replace_alloc(an_allocation a,
                                               size_t        new_capacity,
                                               size_t        n_to_move)
            -> an_allocation
/*
Replace the given allocation -- which was allocated by the same allocator -- by
a new one with at least new_capacity elements.  The first n_to_move elements in
the original allocation must be initialized and are moved to the start of the
new allocation.
*/
{
  sizeof_t n_bytes = new_capacity * sizeof(an_elem);
  an_elem  *old_start = a.start,
           *new_start = (an_elem*)alloc_general(n_bytes);

  move_elements<an_Elem>(new_start, old_start, n_to_move);
  free_general(old_start, a.n_bytes_allocated);
  return an_allocation{new_start, n_bytes};
}  /* General_allocator::replace_alloc */


template<typename an_Elem>
INLINE void General_allocator<an_Elem>::dealloc(an_allocation a)
/*
Release the given allocation -- which was allocated by the same allocator.
The caller is responsible for ensuring the allocation contains no live
objects.
*/
{
  /* Note that free_fe will correctly handle a null allocation. */
  free_general(a.start, a.n_bytes_allocated);
}  /* General_allocator::dealloc */


template<typename an_Object, typename ...an_Arg_pack>
INLINE an_Object *new_general(an_Arg_pack&& ...args)
/*
Allocate in general memory and construct an object of type an_Object with
the constructor arguments specified by args.  Return a pointer to the object.
*/
{
  an_Object  *p = General_allocator<an_Object>::alloc(1).start;
  construct(p, fwd<an_Arg_pack>(args)...);
  return p;
}  /* new_general */


template<typename an_Object>
INLINE void delete_general(an_Object **p)
/*
Destroy and delete an object of type an_Object that was allocated in
general memory.  The value of *p will be set to NULL.
*/
{
  if (*p != NULL) {
    destroy(*p);

    Allocation<an_Object> alloc = {*p, sizeof(an_Object)};
    General_allocator<an_Object>::dealloc(alloc);
    *p = NULL;
  }  /* if */
}  /* delete_general */


template<typename an_Elem>
struct IL_allocator {
  /* An allocator for IL memory (i.e., as allocated by alloc_il in
     il_alloc.c). */
  typedef an_Elem an_elem;
  typedef Allocation<an_elem> an_allocation;
  typedef IL_allocator<an_elem> an_allocator;
  INLINE static auto alloc(size_t n) -> an_allocation;
};  /* IL_allocator */


/* Forward declare alloc_il. */
extern char *alloc_il(sizeof_t size);

template<typename an_Elem>
auto IL_allocator<an_Elem>::alloc(size_t n) -> an_allocation
/*
Allocate at least n elements of type an_Elem and return the resulting
allocation (which reflects the actual number of allocated elements).
*/
{
  sizeof_t n_bytes = n * sizeof(an_elem);

  return an_allocation{(an_elem*)alloc_il(n_bytes), n_bytes};
}  /* IL_allocator::alloc */


template<typename an_Object, typename ...an_Arg_pack>
INLINE an_Object *new_il(an_Arg_pack&& ...args)
/*
Allocate in IL memory and construct an object of type an_Object with the
constructor arguments specified by args.  Return a pointer to the object.
*/
{
  an_Object  *p = IL_allocator<an_Object>::alloc(1).start;
  construct(p, fwd<an_Arg_pack>(args)...);
  return p;
}  /* new_il */


/*
A buffered allocator uses a local buffer of memory to avoid system calls for
dynamically allocated memory unless the local buffer overflows, in which case a
fallback allocator is used to provide the requested allocation.

This allocator is designed to be extremely efficient for structures like
Dyn_array that need one contigous block of memory that's regularly reallocated.
It does not provide any kind of "memory pool", so structures that make use of
many different allocations are unlikely to see much of a benefit.

Note that when a Buffered_allocator is copy or move constructed the fallback
allocator is copied, and the local buffer is left uninitialized.  To transfer
ownership of allocated objects from the previous allocator to the new
allocator, move_alloc should be called on any allocations that are to persist.
This is required as the Buffered_allocator doesn't know what objects in its
buffer are constructed, so it cannot perform the ownership transfer itself (put
another way, the Buffered_allocator owns a "dumb" block of memory, and that
memory can't itself be meaningfully "moved").
*/
template<unsigned a_Capacity,
         template<typename> class a_Fallback_allocator,
         typename an_Elem>
struct Buffered_allocator {
  typedef an_Elem an_elem;
  typedef Allocation<an_elem> an_allocation;
  typedef a_Fallback_allocator<an_Elem> a_fallback_allocator;
  typedef Buffered_allocator<a_Capacity, a_Fallback_allocator, an_Elem>
                an_allocator;
  typedef Buffered_allocator<a_Capacity, a_Fallback_allocator, an_Elem>
                a_deallocator;

  Buffered_allocator(const a_fallback_allocator &a = a_fallback_allocator())
    : fallback_allocator(a), local_used(FALSE)
    {}
  Buffered_allocator(const Buffered_allocator &a)
    : Buffered_allocator(a.fallback_allocator)
    {}
  Buffered_allocator(Buffered_allocator &&a)
    : Buffered_allocator(a.fallback_allocator)
    {}
  ~Buffered_allocator()
    {}

  INLINE auto alloc(size_t n) -> an_allocation;
  INLINE auto replace_alloc(an_allocation  a,
                            size_t         new_capacity,
                            size_t         n_to_move) -> an_allocation;
  INLINE auto move_alloc(an_allocator  &src,
                         an_allocation src_alloc,
                         size_t        n_to_move) -> an_allocation;
  INLINE void dealloc(an_allocation allocation);
private:
  INLINE auto local_alloc(size_t n) -> an_allocation;
  INLINE auto fallback_alloc(size_t n) -> an_allocation;
  a_fallback_allocator
                fallback_allocator;
                        /* The fallback allocator used when the local buffer is
                           already in use. */
  a_boolean     local_used;
                        /* TRUE if the local buffer is being used for an
                           allocation, FALSE otherwise. */
#ifdef UNION_AS_STRUCT
/* Workaround for union-as-struct build issue. */
#undef union
#endif /* ifdef UNION_AS_STRUCT */
  union {
#ifdef UNION_AS_STRUCT
#define union struct
#endif /* ifdef UNION_AS_STRUCT */
    an_Elem     local[a_Capacity];
                        /* The local buffer.  Represented as a union so the
                           value can be uninitialized, and construction and
                           destruction are manually managed. */
  };
};  /* Buffered_allocator */


template<unsigned a_Capacity,
         template<typename> class a_Fallback_allocator,
         typename an_Elem>
auto
Buffered_allocator<a_Capacity, a_Fallback_allocator, an_Elem>::local_alloc(
                                                                   size_t n) ->
                                                                  an_allocation
/*
Allocate at least n elements of type an_Elem using space reserved in this
allocator object and return the resulting allocation (which reflects the actual
number of allocated elements).
*/
{
  check_assertion(!this->local_used && n <= a_Capacity);
  an_elem *start = this->local;
  size_t  num_allocated = a_Capacity;

  this->local_used = TRUE;
  return an_allocation{start, num_allocated * sizeof(an_Elem)};
}  /* Buffered_allocator::local_alloc */


template<unsigned a_Capacity,
         template<typename> class a_Fallback_allocator,
         typename an_Elem>
auto
Buffered_allocator<a_Capacity, a_Fallback_allocator, an_Elem>::fallback_alloc(
                                                                   size_t n) ->
                                                                  an_allocation
/*
Allocate at least n elements of type an_Elem using the fallback allocator and
return the resulting allocation (which reflects the actual number of allocated
elements).
*/
{

  an_allocation alloced = this->fallback_allocator.alloc(n);
  an_elem       *start = alloced.start;
  sizeof_t      num_allocated = alloced.n_bytes_allocated;

  return an_allocation{start, num_allocated};
}  /* Buffered_allocator::fallback_alloc */


template<unsigned a_Capacity,
         template<typename> class a_Fallback_allocator,
         typename an_Elem>
auto
Buffered_allocator<a_Capacity, a_Fallback_allocator, an_Elem>::alloc(
                                                                   size_t n) ->
                                                                  an_allocation
/*
Allocate at least n elements of type an_Elem and return the resulting
allocation (which reflects the actual number of allocated elements).
*/
{
  if (!this->local_used && n <= a_Capacity) {
    return this->local_alloc(n);
  } else {
    return this->fallback_alloc(n);
  }  /* if */
}  /* Buffered_allocator::alloc */


template<unsigned a_Capacity,
         template<typename> class a_Fallback_allocator,
         typename an_Elem>
auto
Buffered_allocator<a_Capacity, a_Fallback_allocator, an_Elem>::replace_alloc(
                                                   an_allocation  a,
                                                   size_t         new_capacity,
                                                   size_t         n_to_move) ->
                                                                  an_allocation
/*
Replace the given allocation -- which was allocated by the same allocator -- by
a new one with at least new_capacity elements.  The first n_to_move elements in
the original allocation must be initialized and are moved to the start of the
new allocation.
*/
{
  an_elem *new_start;
  size_t  new_num_allocated;
  an_elem *old_start = a.start;

  if ((!this->local_used || old_start == this->local) &&
      new_capacity <= a_Capacity) {
    /* Either the local buffer was not previously used, or the previous
       allocation was already using the local buffer. */
    this->local_used = TRUE;
    new_start = this->local;
    new_num_allocated = new_capacity * sizeof(an_Elem);
  } else {
    an_allocation alloced = this->fallback_allocator.alloc(new_capacity);

    new_start = alloced.start;
    new_num_allocated = alloced.n_bytes_allocated;
  }  /* if */
  /* If we're still within the local capacity old_start will equal new_start,
     and nothing more needs to happen. */
  if (old_start != new_start) {
    move_elements<an_Elem>(new_start, old_start, n_to_move);
    this->dealloc(a);
  }  /* if */
  return an_allocation{new_start, new_num_allocated};
}  /* Buffered_allocator::replace_alloc */


template<unsigned a_Capacity,
         template<typename> class a_Fallback_allocator,
         typename an_Elem>
auto
Buffered_allocator<a_Capacity, a_Fallback_allocator, an_Elem>::move_alloc(
                                                    an_allocator  &src,
                                                    an_allocation src_alloc,
                                                    size_t        n_to_move) ->
                                                                  an_allocation
/*
Move the ownership of the given source allocation from the source allocator to
this allocator.  The first n_to_move elements in the original allocation are
initialized and should therefore be moved to the new allocator.
*/
{
  an_elem *new_start;
  size_t  new_num_allocated;
  an_elem *old_start = src_alloc.start;

  if (old_start == src.local) {
    /* This allocation is owned by the buffer of src.  Steal the allocation
       from src's buffer and move its contents into this allocator's buffer. */
    an_allocation alloced = this->alloc(n_to_move);

    new_start = alloced.start;
    new_num_allocated = alloced.n_bytes_allocated;
    move_elements<an_elem>(new_start, old_start, n_to_move);
  } else {
    /* This allocation was created by the fallback allocator of src.  Steal the
       allocation from src's fallback allocator to this allocator's fallback
       allocator. */
    an_allocation alloced = this->fallback_allocator.move_alloc(
                                                        src.fallback_allocator,
                                                        src_alloc,
                                                        n_to_move);

    new_start = alloced.start;
    new_num_allocated = alloced.n_bytes_allocated;
  }  /* if */
  return an_allocation{new_start, new_num_allocated};
}  /* Buffered_allocator::move_alloc */


template<unsigned a_Capacity,
         template<typename> class a_Fallback_allocator,
         typename an_Elem>
void
Buffered_allocator<a_Capacity, a_Fallback_allocator, an_Elem>::dealloc(
                                                              an_allocation  a)
/*
Release the given allocation -- which was allocated by the same allocator.
The caller is responsible for ensuring the allocation contains no live
objects.
*/
{
  if (a.start == this->local) {
    /* The local buffer was previously used, mark it as available. */
    this->local_used = FALSE;
  } else {
    /* An allocated block of memory was previously used, deallocate it. */
    this->fallback_allocator.dealloc(a);
  }  /* if */
}  /* Buffered_allocator::dealloc */


/*
A small "meta" class used to create derived Buffered_allocators that can used
to create easy to use "Small" versions of existing allocator-compatible
templates.  See Small_dyn_array for an example.
*/
template<unsigned a_Capacity,
         template<typename> class a_Fallback_allocator>
struct Delegate_buffered_allocator {
  template<typename an_Elem>
  struct Meta : public Buffered_allocator<a_Capacity, a_Fallback_allocator,
                                          an_Elem> {
  };  /* Meta */
};  /* Delegate_buffered_allocator */

/*
This type is used to provide a special "pointer" to a Dyn_array that is still
usable even if the underlying storage is reallocated.

Note this type diverges from the typical EDG style in placing the private data
members first, preceding the public interface.  This is done so that the
decltype operators can find the data members for type deduction.
*/
template<typename an_Array_type>
class Array_ptr {
  an_Array_type *array; /* The array being referenced. */
  size_t        offset; /* The offset into the array. */
public:
  Array_ptr()
     : array(NULL), offset(0)
     {}
  Array_ptr(an_Array_type *array_val,
            size_t        offset_val)
     : array(array_val), offset(offset_val)
     {}

  a_boolean has_value() const
    { return this->array != NULL; }
  auto operator*() const -> decltype((*this->array)[this->offset])
    { return (*this->array)[offset]; }
  auto operator->() const -> decltype(&((*this->array)[this->offset]))
    { return &(*this->array)[offset]; }
};  /* Array_ptr */

/*
The Dyn_array template
======================
The Dyn_array<E, A> template defined below implements a dynamic array construct
not unlike std::vector<E, A>.  E is the element type and A is the allocator
type (which defaults to the front end memory allocator).

The most common std::vector operators are also applicable to Dyn_array.  Things
like operator[], push_back, begin(), end(), etc., work as expected (which,
e.g., means that the C++11 range-based for-statement works for Dyn_array also).

Because every element access goes through the subscript operators, they check
the index against the length only when EXPENSIVE_CHECKING is enabled.  The
operations that shift elements around check their index unconditionally: There
the check costs little compared to the work the operation itself does.
*/

/*lint -esym(1510,*Dyn_array)*/
template<typename an_Elem, template<typename> class Allocator = FE_allocator>
struct Dyn_array: private Allocator<an_Elem> {
  /* A dynamically growable array-like class type. */
  typedef an_Elem an_elem;
  typedef Allocator<an_Elem> an_allocator;
  INLINE Dyn_array(size_t             cap = 0,
                   const an_allocator &a = an_allocator());
  INLINE Dyn_array(size_t             cap,
                   const an_elem      &v,
                   const an_allocator &a = an_allocator());
  INLINE Dyn_array(const Dyn_array&);
  INLINE Dyn_array(Dyn_array&&);
  INLINE ~Dyn_array();
  INLINE auto operator=(const Dyn_array&) -> Dyn_array&;
  INLINE auto operator=(Dyn_array&&) -> Dyn_array&;
  INLINE auto operator[](size_t i) -> an_elem&;
  INLINE auto operator[](size_t i) const -> const an_elem&;
  INLINE auto is_empty() const -> a_boolean
    { return this->n_elems == 0; }
  INLINE auto length() const -> size_t
    { return this->n_elems; }
  INLINE auto capacity() const -> size_t
    { return this->n_allocated; }
  INLINE auto front_elem() -> an_elem&
    { return (*this)[0]; }
  INLINE auto front_elem() const -> const an_elem&
    { return (*this)[0]; }
  INLINE auto back_elem() -> an_elem&
    { return (*this)[this->n_elems-1]; }
  INLINE auto back_elem() const -> const an_elem&
    { return (*this)[this->n_elems-1]; }
  template<template<typename> class Elem_allocator>
  inline an_elem*
  to_allocated_storage(Elem_allocator<an_elem>  allocator) const;
  INLINE void push_back(const an_elem  &value);
  INLINE void push_back(an_elem  &&value);
  template<typename ...an_Arg_pack>
  INLINE void emplace_back(an_Arg_pack&& ...args);
  INLINE void pop_back()
    { destroy(&((*this)[this->n_elems-1])); --this->n_elems; }
  INLINE void insert(size_t i, const an_elem  &value);
  INLINE void insert(size_t i, an_elem  &&value);
  template<typename an_Input_iterator>
  inline void insert(size_t            i,
                     an_Input_iterator begin,
                     size_t            len);
  inline void insert_many(size_t i, size_t num_copies, const an_elem &value);
  INLINE void remove(size_t i);
  INLINE void remove_many(size_t i, size_t num_elements);
  template<typename a_Predicate>
  INLINE void remove_if(size_t i, a_Predicate predicate_fn);
  template<typename a_Predicate>
  INLINE void remove_if(a_Predicate predicate_fn)
    { this->remove_if(0, predicate_fn); }
  INLINE void clear();
  INLINE void resize(size_t new_n, const an_elem  &value);
  INLINE void reserve(size_t);
  /* Interfaces to allow range-based for loop. */
  /*lint -e{1535}*/
  INLINE auto begin() -> an_elem*
    { return this->elems; }
  INLINE auto begin() const -> const an_elem*
    { return this->elems; }
  INLINE auto end() -> an_elem*
    { return this->elems+this->n_elems; }
  INLINE auto end() const -> const an_elem*
    { return this->elems+this->n_elems; }
private:
  typedef typename an_allocator::an_allocation an_allocation;
  an_elem	*elems;
			/* Pointer to the allocated elements. */
  size_t	n_allocated;
			/* Number of elements allocated.  This is also known
			   as the "capacity". */
  size_t	n_elems;
			/* Number of initialized elements.  This is also known
			   as the "length". */
  void grow();
};  /* Dyn_array */

/*
A template alias used to form "small" Dyn_arrays that have an initial
pre-allocated storage capacity before then falling back to a secondary
allocator (typically for dynamically allocated storage).
*/
template<typename an_Elem, unsigned a_Capacity,
         template<typename> class a_Fallback_allocator = FE_allocator>
using Small_dyn_array = Dyn_array<an_Elem, Delegate_buffered_allocator<
                                         a_Capacity,
                                         a_Fallback_allocator>::template Meta>;


template<typename an_Elem, template<typename> class Allocator>
Dyn_array<an_Elem, Allocator>::Dyn_array(size_t             cap,
                                         const an_allocator &a)
/*
Initialize a Dyn_array with a minimum of cap elements whose allocation is
managed by the given allocator.
*/
  : an_allocator(a)
  , elems()
  , n_allocated()
  , n_elems(0)
{
  an_allocation  allocation = this->alloc(cap);
  this->elems = allocation.start;
  this->n_allocated = allocation.n_bytes_allocated / sizeof(an_Elem);
}  /* Dyn_array::Dyn_array */


template<typename an_Elem, template<typename> class Allocator>
Dyn_array<an_Elem, Allocator>::Dyn_array(size_t             cap,
                                         const an_elem      &v,
                                         const an_allocator &a)
/*
Initialize a Dyn_array with a minimum of cap elements whose allocation is
managed by the given allocator.  Initialize the first cap elements to v.
*/
  : an_allocator(a)
  , elems()
  , n_allocated()
  , n_elems(cap)
{
  an_allocation  allocation = this->alloc(cap);
  this->elems = allocation.start;
  this->n_allocated = allocation.n_bytes_allocated / sizeof(an_Elem);

  /* Copy-construct the element into newly-allocated storage. */
  an_elem *dst_elems = this->elems;
  copy_construct_element<an_elem>(dst_elems, v, cap);
}  /* Dyn_array::Dyn_array */


template<typename an_Elem, template<typename> class Allocator>
Dyn_array<an_Elem, Allocator>::Dyn_array(const Dyn_array &src)
/*
Copy constructor.
*/
  : an_allocator(src)
  , elems()
  , n_allocated()
  , n_elems(src.n_elems)
{
  /* Allocate new storage. */
  an_allocation  allocation = this->alloc(src.n_elems);
  this->elems = allocation.start;
  this->n_allocated = allocation.n_bytes_allocated / sizeof(an_Elem);

  /* Copy-construct the elements from the source into the newly-allocated
     storage. */
  an_elem *dst_elems = this->elems;
  an_elem *src_elems = src.elems;
  size_t  new_n = this->n_elems;
  copy_construct_elements<an_elem>(dst_elems, src_elems, new_n);
}  /* Dyn_array::Dyn_array */


template<typename an_Elem, template<typename> class Allocator>
Dyn_array<an_Elem, Allocator>::Dyn_array(Dyn_array &&src)
/*
Move constructor.
*/
  : an_allocator(move_from(&src))
  , elems()
  , n_allocated()
  , n_elems(src.n_elems)
{
  an_allocation src_alloc = an_allocation{src.elems,
                                          src.n_allocated * sizeof(an_Elem)};
  an_allocation new_alloc = this->move_alloc(src, src_alloc, src.n_elems);

  this->elems = new_alloc.start;
  this->n_allocated = new_alloc.n_bytes_allocated / sizeof(an_Elem);
  src.elems = NULL;
  src.n_allocated = 0;
  src.n_elems = 0;
}  /* Dyn_array::Dyn_array */


template<typename an_Elem, template<typename> class Allocator>
Dyn_array<an_Elem, Allocator>::~Dyn_array()
/*
Destructor.
*/
{
  destroy_elements<an_elem>(this->elems, this->n_elems);
  this->dealloc(an_allocation{ this->elems,
                               this->n_allocated * sizeof(an_Elem)});
  this->elems = NULL;
}  /* Dyn_array::~Dyn_array */


/*lint -e{1529}*/
template<typename an_Elem, template<typename> class Allocator>
auto Dyn_array<an_Elem, Allocator>::operator=(const Dyn_array &b)
            -> Dyn_array&
/*
Copy assignment operator.
*/
{
  size_t n = this->n_elems;
  size_t new_n = b.n_elems;

  if (new_n == n) {
    /* Straightforward element-to-element assignment.  Note that this covers
       self-assignment. */
    an_elem *dst_elems = this->elems;
    an_elem *src_elems = b.elems;

    copy_construct_elements<an_elem>(dst_elems, src_elems, new_n);
  } else {
    /* A change in size (and possibly capacity) is needed.  Destroy the
       original elements, and then construct the new ones. */
    this->clear();
    if (this->n_allocated < new_n) {
      this->reserve(new_n);
    }  /* if */

    an_elem *dst_elems = this->elems;
    an_elem *src_elems = b.elems;
    copy_construct_elements<an_elem>(dst_elems, src_elems, new_n);
    this->n_elems = new_n;
  }  /* if */
  return *this;
}  /* Dyn_array::operator= */


template<typename an_Elem, template<typename> class Allocator>
auto Dyn_array<an_Elem, Allocator>::operator=(Dyn_array &&b)
            -> Dyn_array&
/*
Move assignment operator.
*/
{
  if (this != &b) {
    destroy(this);
    construct(this, move_from(&b));
  }  /* if */
  return *this;
}  /* Dyn_array::operator= */


template<typename an_Elem, template<typename> class Allocator>
auto Dyn_array<an_Elem, Allocator>::operator[](size_t i) -> an_elem&
/*
Subscript operator to access the element at the given index.
*/
{
BEGIN_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED
#if EXPENSIVE_CHECKING
  check_assertion(i < this->n_elems);
#endif /* EXPENSIVE_CHECKING */
  return this->elems[i];
END_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED
}  /* Dyn_array::operator[] */


template<typename an_Elem, template<typename> class Allocator>
auto Dyn_array<an_Elem, Allocator>::operator[](size_t i) const ->
                                                                 const an_elem&
/*
Subscript operator to access a const version of the element at the given index.
*/
{
BEGIN_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED
#if EXPENSIVE_CHECKING
  check_assertion(i < this->n_elems);
#endif /* EXPENSIVE_CHECKING */
  return this->elems[i];
END_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED
}  /* Dyn_array::operator[] */


template<typename an_Elem, template<typename> class Allocator>
template<template<typename> class Elem_allocator>
an_Elem*
Dyn_array<an_Elem, Allocator>::to_allocated_storage(
                                            Elem_allocator<an_Elem>  allocator)
                                                                          const
/*
Given an allocator, allocate and return a new array of the contained elements.
*/
{
  return new_copy_of_elements(*this, this->length(), allocator);
}  /* Dyn_array::to_allocated_storage */


template<typename an_Elem, template<typename> class Allocator>
void Dyn_array<an_Elem, Allocator>::push_back(const an_elem &value)
/*
Copy the given value into the position after the currently-last element.
Allocate new storage if needed.
*/
{
  size_t  n = this->n_elems;

  if (n == this->n_allocated) {
    this->grow();
  }  /* if */
  construct(this->elems+n, value);
  this->n_elems = n+1;
}  /* Dyn_array::push_back */


template<typename an_Elem, template<typename> class Allocator>
void Dyn_array<an_Elem, Allocator>::push_back(an_elem &&value)
/*
Move the given value into the position after the currently-last element.
Allocate new storage if needed.
*/
{
  size_t  n = this->n_elems;

  if (n == this->n_allocated) {
    this->grow();
  }  /* if */
  construct(this->elems+n, move_from(&value));
  this->n_elems = n+1;
}  /* Dyn_array::push_back */


template<typename an_Elem, template<typename> class Allocator>
template<typename ...an_Arg_pack>
void Dyn_array<an_Elem, Allocator>::emplace_back(an_Arg_pack&& ...args)
/*
Construct in-place a new value after the currently-last element.  The given
arguments will be forwarded to the constructor.
*/
{
  size_t  n = this->n_elems;

  if (n == this->n_allocated) {
    this->grow();
  }  /* if */
  construct(this->elems+n, fwd<an_Arg_pack>(args)...);
  this->n_elems = n+1;
}  /* Dyn_array::emplace_back */


template<typename an_Elem, template<typename> class Allocator>
void Dyn_array<an_Elem, Allocator>::insert(size_t        i,
                                           const an_elem &value)
/*
Copy-insert the given value at the given index.  All subsequent values (if any)
are first moved one position up.
*/
{
  check_assertion(i <= this->n_elems);
  size_t orig_count = this->n_elems;

  if (orig_count == this->n_allocated) {
    this->grow();
  }  /* if */

  /* Move the existing elements past the insertion point. */
  an_elem  *arr_elems = this->elems;
  an_elem  *move_src = arr_elems + i;
  an_elem  *move_dest = move_src + 1;
  size_t   num_to_move = orig_count - i;
  move_elements<an_elem>(move_dest, move_src, num_to_move);
  /* Insert the new element. */
  construct(arr_elems+i, value);
  ++this->n_elems;
}  /* Dyn_array::insert */


template<typename an_Elem, template<typename> class Allocator>
void Dyn_array<an_Elem, Allocator>::insert(size_t  i,
                                           an_elem &&value)
/*
Move-insert the given value at the given index.  All subsequent values (if any)
are first moved one position up.
*/
{
  check_assertion(i <= this->n_elems);
  size_t orig_count = this->n_elems;

  if (orig_count == this->n_allocated) {
    this->grow();
  }  /* if */

  /* Move the existing elements past the insertion point. */
  an_elem  *arr_elems = this->elems;
  an_elem  *move_src = arr_elems + i;
  an_elem  *move_dest = move_src + 1;
  size_t   num_to_move = orig_count - i;
  move_elements<an_elem>(move_dest, move_src, num_to_move);
  /* Insert the new element. */
  construct(arr_elems+i, move_from(&value));
  ++this->n_elems;
}  /* Dyn_array::insert */


template<typename an_Elem, template<typename> class Allocator>
template<typename an_Input_iterator>
void Dyn_array<an_Elem, Allocator>::insert(size_t            i,
                                           an_Input_iterator start,
                                           size_t            len)
/*
Copy-insert len number of values copying sequentially from the given iterator
into the array beginning at the given index i.  All existing values from index
i through the end of the array are first moved len positions back.
*/
{
  check_assertion(i <= this->n_elems);
  size_t orig_count = this->n_elems;

  /* Ensure adequate capacity for the bulk insert operation. */
  this->reserve(orig_count + len);

  an_elem *arr_elems = this->elems;
  /* Move the existing elements past the inserted sequence. */
  size_t  num_to_move = orig_count - i;
  if (num_to_move != 0) {
    an_elem *move_src = arr_elems + i;
    an_elem *move_dest = move_src + len;

    move_elements<an_elem>(move_dest, move_src, num_to_move);
  }  /* if */

  /* Insert the sequence of elements. */
  an_elem *insert_dest = arr_elems + i;
  copy_construct_elements<an_elem>(insert_dest, start, len);
  this->n_elems += len;
}  /* Dyn_array::insert */


template<typename an_Elem, template<typename> class Allocator>
void Dyn_array<an_Elem, Allocator>::insert_many(size_t        i,
                                                size_t        num_copies,
                                                const an_elem &value)
/*
Copy-insert the given number of copies of the value starting at the given
index.  All subsequent values (if any) are first moved by the number of copies
back.
*/
{
  check_assertion(i <= this->n_elems);
  size_t  orig_count = this->n_elems;

  /* Ensure adequate capacity for the bulk insert operation. */
  this->reserve(orig_count + num_copies);

  an_elem  *arr_elems = this->elems;
  /* Move the existing elements past the inserted copies. */
  an_elem  *move_src = arr_elems + i;
  an_elem  *move_dest = move_src + num_copies;
  size_t   num_to_move = orig_count - i;
  move_elements<an_elem>(move_dest, move_src, num_to_move);

  /* Construct the new elements. */
  an_elem *insert_start = arr_elems + i;
  copy_construct_element<an_elem>(insert_start, value, num_copies);
  this->n_elems += num_copies;
}  /* Dyn_array::insert_many */


template<typename an_Elem, template<typename> class Allocator>
void Dyn_array<an_Elem, Allocator>::remove(size_t  i)
/*
Destroy the entry at the given index.  All subsequent values (if any) are moved
one position down.
*/
{
  check_assertion(i < this->n_elems);
  size_t   orig_count = this->n_elems;
  an_elem  *arr_elems = this->elems;

  destroy(arr_elems + i);

  an_elem *move_dest = arr_elems + i;
  an_elem *move_src = move_dest + 1;
  size_t   num_to_move = orig_count - (i + 1);
  move_elements<an_elem>(move_dest, move_src, num_to_move);
  --this->n_elems;
}  /* Dyn_array::remove */


template<typename an_Elem, template<typename> class Allocator>
void Dyn_array<an_Elem, Allocator>::remove_many(size_t i,
                                                size_t num_elements)
/*
Remove the given number of elements starting at the given index.  All
subsequent values (if any) are first moved by the number of copies back.
*/
{
  check_assertion(i + num_elements <= this->n_elems);
  if (num_elements > 0) {
    size_t  orig_count = this->n_elems;
    an_elem *arr_elems = this->elems;
    an_elem *removal_start = this->elems + i;

    /* Destroy the elements. */
    destroy_elements<an_elem>(removal_start, num_elements);

    /* Move any elements past the point of removal back. */
    an_elem *move_dest = arr_elems + i;
    an_elem *move_src = move_dest + num_elements;
    size_t  num_to_move = orig_count - (i + num_elements);
    move_elements<an_elem>(move_dest, move_src, num_to_move);
    this->n_elems -= num_elements;
  }  /* if */
}  /* Dyn_array::remove_many */


template<typename an_Elem, template<typename> class Allocator>
template<typename a_Predicate>
void Dyn_array<an_Elem, Allocator>::remove_if(size_t      i,
                                              a_Predicate predicate_fn)
/*
Given a predicate function that accepts a value of an_Elem type and returns a
boolean, apply the predicate function to all elements (starting from the given
index) and remove any elements where the function returns TRUE.
*/
{
  an_elem *arr_elems = this->elems;
  size_t  num_removed = 0;

  for (; i < this->n_elems; ++i) {
    if (predicate_fn(arr_elems[i])) {
      ++num_removed;
      destroy(&arr_elems[i]);
    } else if (num_removed > 0) {
      construct(&arr_elems[i - num_removed], move_from(&arr_elems[i]));
      destroy(&arr_elems[i]);
    }  /* if */
  }  /* for */
  this->n_elems -= num_removed;
}  /* Dyn_array::remove_if */


template<typename an_Elem, template<typename> class Allocator>
void Dyn_array<an_Elem, Allocator>::clear()
/*
Remove all the elements in the array.
*/
{
  /* Due to inlining depth restrictions, checking the size before calling
     destroy_elements can save time. */
  if (this->n_elems != 0) {
    destroy_elements<an_elem>(this->elems, this->n_elems);
    this->n_elems = 0;
  }  /* if */
}  /* Dyn_array::clear */


template<typename an_Elem, template<typename> class Allocator>
void Dyn_array<an_Elem, Allocator>::resize(size_t        new_n,
                                           const an_elem &value)
/*
Resize the array to the given length.  Any new elements are copy-inserted from
the given value.
*/
{
  size_t  old_n = this->n_elems;

  if (new_n > old_n) {
    this->reserve(new_n);

    size_t  num_copies = new_n - old_n;
    an_elem *insert_start = this->elems + old_n;
    copy_construct_element<an_elem>(insert_start, value, num_copies);
    this->n_elems += num_copies;
  } else if (new_n < old_n) {
    size_t  num_to_destroy = old_n - new_n;
    an_elem *removal_start = this->elems + old_n - num_to_destroy;

    destroy_elements<an_elem>(removal_start, num_to_destroy);
    this->n_elems -= num_to_destroy;
  }  /* if */
}  /* Dyn_array::resize */


template<typename an_Elem, template<typename> class Allocator>
void Dyn_array<an_Elem, Allocator>::reserve(size_t  new_cap)
/*
Increase the capacity to the given value if that given value is larger than
the current capacity.
*/
{
  size_t  old_cap = this->n_allocated;

  if (new_cap > old_cap) {
    an_allocation  a = this->replace_alloc(
                                      an_allocation{this->elems,
                                                    old_cap * sizeof(an_Elem)},
                                      new_cap, this->n_elems);
    this->elems = a.start;
    this->n_allocated = a.n_bytes_allocated / sizeof(an_Elem);
  }  /* if */
}  /* Dyn_array::reserve */


template<typename an_Elem, template<typename> class Allocator>
void Dyn_array<an_Elem, Allocator>::grow()
/*
Grow the capacity of the array by about half, unless the capacity is less than
2, in which case the capacity is set to 2.
*/
{
  size_t         old_cap = this->n_allocated,
                 new_cap = old_cap < 2 ? 2 : old_cap + old_cap/2 + 1;
  an_allocation  a = this->replace_alloc(
                                     an_allocation{this->elems,
                                                   old_cap * sizeof(an_Elem)},
                                     new_cap, this->n_elems);
  this->elems = a.start;
  this->n_allocated = a.n_bytes_allocated / sizeof(an_Elem);
}  /* Dyn_array::grow */


template<typename an_Object,
         template<typename> class Deleter = FE_allocator>
struct Owning_ptr: private Deleter<an_Object> {
  /* A smart pointer managing an object it owns. */
  typedef an_Object an_object;
  typedef Deleter<an_Object> a_deleter;
  INLINE Owning_ptr()
    : a_deleter(), ptr(NULL) {}
  INLINE Owning_ptr(an_object *p, const a_deleter &d = a_deleter())
    : a_deleter(d), ptr(p) {}
  INLINE Owning_ptr(a_nullptr, const a_deleter &d = a_deleter())
    : a_deleter(d), ptr(NULL) {}
  INLINE Owning_ptr(const Owning_ptr&) = delete;
  INLINE Owning_ptr(Owning_ptr&& src)
    : a_deleter(move_from(&src)), ptr(src.ptr) { src.ptr = NULL; }
  INLINE ~Owning_ptr();
  INLINE auto operator=(const Owning_ptr&) -> Owning_ptr& = delete;
  INLINE auto operator=(Owning_ptr&& src) -> Owning_ptr&;
  INLINE auto operator=(a_nullptr) -> Owning_ptr&;
  INLINE auto operator->() const -> an_object*
    { return this->ptr; }
  INLINE auto operator*() const -> an_object&
    { return *this->ptr; }
  INLINE auto raw() -> an_object*
    { return this->ptr; }
  INLINE auto raw() const -> an_object*
    { return this->ptr; }
  INLINE auto release() -> an_object*;
private:
  an_object	*ptr;	/* Pointer to the owned object. */
};  /* Owning_ptr */


template<typename an_Object, template<typename> class Deleter>
Owning_ptr<an_Object, Deleter>::~Owning_ptr()
/*
Destroy and deallocate the pointed-to object, if any.
*/
{
  this->delete_object(&this->ptr);
}  /* Owning_ptr::~Owning_ptr */


template<typename an_Object, template<typename> class Deleter>
auto Owning_ptr<an_Object, Deleter>::operator=(Owning_ptr &&src)
                                                -> Owning_ptr&
/*
Move src to *this, then return *this.
*/
{
  if (this != &src) {
    this->ptr = src.ptr;
    src.ptr = NULL;
  }  /* if */
  return *this;
}  /* Owning_ptr::operator= */


template<typename an_Object, template<typename> class Deleter>
auto Owning_ptr<an_Object, Deleter>::operator=(a_nullptr)
                                                -> Owning_ptr&
/*
Destroy and deallocate the pointed-to object, if any.  Then, set the owning
pointer to a null value.  Return *this.
*/
{
  this->delete_object(&this->ptr);
  return *this;
}  /* Owning_ptr::operator= */


template<typename an_Object, template<typename> class Deleter>
auto Owning_ptr<an_Object, Deleter>::release() -> an_object*
/*
Release the owned pointer from management and return it.  The caller takes
responsibility for memory management of the returned pointer.
*/
{
  an_Object  *p = this->ptr;

  this->ptr = NULL;
  return p;
}  /* Owning_ptr::release */


template<typename an_Object,
         template<typename> class Deleter_A,
         template<typename> class Deleter_B>
INLINE a_boolean operator==(const Owning_ptr<an_Object, Deleter_A> &ptr_a,
                            const Owning_ptr<an_Object, Deleter_B> &ptr_b)
/*
Return TRUE if the given pointer values are equal; otherwise, return FALSE.
*/
{
  return ptr_a.raw() == ptr_b.raw();
}  /* operator== */


template<typename an_Object,
         template<typename> class Deleter_A,
         template<typename> class Deleter_B>
INLINE a_boolean operator!=(const Owning_ptr<an_Object, Deleter_A> &ptr_a,
                            const Owning_ptr<an_Object, Deleter_B> &ptr_b)
/*
Return TRUE if the given pointer values are not equal; otherwise, return FALSE.
*/
{
  return !(ptr_a == ptr_b);
}  /* operator!= */


template<typename an_Object, template<typename> class Deleter>
INLINE a_boolean operator==(a_nullptr                            ptr_a,
                            const Owning_ptr<an_Object, Deleter> &ptr_b)
/*
Return TRUE if ptr_b is a null pointer; otherwise, return FALSE.
*/
{
  return ptr_a == ptr_b.raw();
}  /* operator== */


template<typename an_Object, template<typename> class Deleter>
INLINE a_boolean operator!=(a_nullptr                            ptr_a,
                            const Owning_ptr<an_Object, Deleter> &ptr_b)
/*
Return TRUE if ptr_b is not a null pointer; otherwise, return FALSE.
*/
{
  return !(ptr_a == ptr_b);
}  /* operator!= */


template<typename an_Object, template<typename> class Deleter>
INLINE a_boolean operator==(const Owning_ptr<an_Object, Deleter> &ptr_a,
                            a_nullptr                            ptr_b)
/*
Return TRUE if ptr_a is a null pointer; otherwise, return FALSE.
*/
{
  return ptr_a.raw() == ptr_b;
}  /* operator== */


template<typename an_Object, template<typename> class Deleter>
INLINE a_boolean operator!=(const Owning_ptr<an_Object, Deleter> &ptr_a,
                            a_nullptr                            ptr_b)
/*
Return TRUE if ptr_a is not a null pointer; otherwise, return FALSE.
*/
{
  return !(ptr_a == ptr_b);
}  /* operator!= */


template<typename an_Object, typename ...an_Arg_pack>
INLINE Owning_ptr<an_Object> owning_ptr(an_Arg_pack&& ...args)
/*
Convenience function to create an owning pointer to an object allocated in
front-end memory.
*/
{
  an_Object *p = new_fe<an_Object>(fwd<an_Arg_pack>(args)...);

  return Owning_ptr<an_Object>(p);
}  /* owning_ptr */


/*
Create and return a bitmask of the given width.

For example: bitwidth=0 is equivalent to 0b0, bitwidth=1 is equivalent to 0b1,
bitwidth=2 is equivalent to 0b11, etc.
*/
#define bitmask_of_width(bitwidth) ((1ull << bitwidth) - 1)


/*
Copy the given source value to the given destination bitfield (of the given
bitwidth).

This macro is used to avoid compiler warnings when copying into a bitfield.
*/
#define copy_to_bitfield(src_val, dest, bitwidth)                             \
    (dest) = ((src_val) & bitmask_of_width(bitwidth))

namespace detail {

/*
The type used for lifetime management of a shared object.
*/
template<typename an_Object>
struct Shared_obj_control_block {
  INLINE ~Shared_obj_control_block() = default;
  an_Object     object;
                        /* The shared object owned by the shared pointer. */
  unsigned      ref_counter;
                        /* The number of references to this control block. */
};  /* Shared_obj_control_block */

}  /* namespace detail */

/*
This type is used to represent a reference counted object.

It is conceptually very similar to a std::shared_ptr.  However, it reduces
indirection by storing the shared object with the shared reference counter.
Additionally, this type differs in that it cannot take ownership of an existing
pointer; this avoids a class of bugs where object lifetime is incorrectly
managed.
*/
template<typename an_Object,
         template<typename> class Allocator = FE_allocator>
struct Shared_obj :
               private Allocator<detail::Shared_obj_control_block<an_Object>> {
  using an_object = an_Object;
  using a_shared_object = Shared_obj<an_Object, Allocator>;
  using a_control_block = detail::Shared_obj_control_block<an_object>;
  using an_allocator = Allocator<a_control_block>;
  using an_allocation = typename an_allocator::an_allocation;

  INLINE Shared_obj()
    : an_allocator(an_allocator()), ctrl_block(NULL) {}
  INLINE explicit Shared_obj(const an_Object    &o,
                             const an_allocator &a = an_allocator());
  INLINE explicit Shared_obj(an_Object          &&o,
                             const an_allocator &a = an_allocator());
  INLINE Shared_obj(const a_shared_object &other)
    : an_allocator(other), ctrl_block(other.ctrl_block)
    { if (this->ctrl_block != NULL) { ++(this->ctrl_block->ref_counter); } }
BEGIN_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED
  INLINE Shared_obj(a_shared_object &&other)
    : an_allocator(other), ctrl_block(other.ctrl_block)
    { other.ctrl_block = NULL; }
END_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED
  INLINE ~Shared_obj();
  INLINE auto operator=(const an_Object &other) -> a_shared_object&;
  INLINE auto operator=(an_Object &&other) -> a_shared_object&;
  INLINE auto operator=(const a_shared_object &other) -> a_shared_object&;
  INLINE auto operator=(a_shared_object &&other) -> a_shared_object&;
  INLINE auto operator->() const -> an_object*
    { return &this->get_ctrl_block().object; }
  INLINE auto operator*() const -> an_object&
    { return this->get_ctrl_block().object; }
  INLINE auto ptr() const -> an_object*;
private:
  template<typename ...an_Arg_pack>
  INLINE Shared_obj(const an_allocator &a,
                    an_Arg_pack&&      ...args);
  INLINE a_control_block& get_ctrl_block() const
    { check_assertion(this->ctrl_block != NULL); return *this->ctrl_block; }
  INLINE void increment_reference() const;
  INLINE void decrement_reference();
  a_control_block
                *ctrl_block;
                        /* Pointer to the control block. */
  template<typename an_Object_ty, typename ...an_Arg_pack>
  friend INLINE Shared_obj<an_Object_ty> shared_obj(an_Arg_pack&& ...args);
};  /* Shared_obj */


template<typename an_Object, template<typename> class Allocator>
Shared_obj<an_Object, Allocator>::Shared_obj(const an_Object    &o,
                           /* Defaulted: */  const an_allocator &a)
/*
Create a new shared copy of the given object using the given allocator.
*/
  : an_allocator(a)
{
  an_allocation allocation = this->alloc(1);

  this->ctrl_block = (a_control_block*)allocation.start;
  new (this->ctrl_block) a_control_block{o, /*ref_counter=*/1};
}  /* Shared_obj::Shared_obj */


template<typename an_Object, template<typename> class Allocator>
Shared_obj<an_Object, Allocator>::Shared_obj(an_Object          &&o,
                           /* Defaulted: */  const an_allocator &a)
/*
Create a new shared copy of the given object using the given allocator.
*/
  : an_allocator(a)
{
  an_allocation allocation = this->alloc(1);

  this->ctrl_block = (a_control_block*)allocation.start;
  new (this->ctrl_block) a_control_block{move_from(&o), /*ref_counter=*/1};
}  /* Shared_obj::Shared_obj */


template<typename an_Object, template<typename> class Allocator>
template<typename ...an_Arg_pack>
Shared_obj<an_Object, Allocator>::Shared_obj(const an_allocator &a,
                                             an_Arg_pack&&      ...args)
/*
In-place construct a new shared object from the given objects using the given
allocator.  This constructor should be used via the shared_obj factory
function.
*/
  : an_allocator(a)
{
  an_allocation allocation = this->alloc(1);

  this->ctrl_block = (a_control_block*)allocation.start;
  new (this->ctrl_block) a_control_block{{fwd<an_Arg_pack>(args)...},
                                         /*ref_counter=*/1};
}  /* Shared_obj::Shared_obj */


template<typename an_Object, template<typename> class Allocator>
Shared_obj<an_Object, Allocator>::~Shared_obj()
/*
Destroy and deallocate the pointed-to object, if any.
*/
{
  this->decrement_reference();
}  /* Shared_obj::~Shared_obj */


template<typename an_Object, template<typename> class Allocator>
auto Shared_obj<an_Object, Allocator>::operator=(const an_Object &other)
                                                            -> a_shared_object&
/*
Copy other to *this, then return *this.
*/
{
  /* Decrement the reference count of the control block currently owned. */
  this->decrement_reference();

  /* Create a copy of the object. */
  an_allocation allocation = this->alloc(1);
  this->ctrl_block = (a_control_block*)allocation.start;
  new (this->ctrl_block) a_control_block{other, /*ref_counter=*/1};
  return *this;
}  /* Shared_obj::operator= */


template<typename an_Object, template<typename> class Allocator>
auto Shared_obj<an_Object, Allocator>::operator=(an_Object &&other)
                                                            -> a_shared_object&
/*
Move other to *this, then return *this.
*/
{
  /* Decrement the reference count of the control block currently owned. */
  this->decrement_reference();

  /* Move-construct the object. */
  an_allocation allocation = this->alloc(1);
  this->ctrl_block = (a_control_block*)allocation.start;
  new (this->ctrl_block) a_control_block{move_from(&other), /*ref_counter=*/1};
  return *this;
}  /* Shared_obj::operator= */


template<typename an_Object, template<typename> class Allocator>
auto Shared_obj<an_Object, Allocator>::operator=(const a_shared_object &other)
                                                            -> a_shared_object&
/*
Copy other to *this, then return *this.
*/
{
  if (this != &other) {
    /* Increase the reference count of the control block being copied. */
    other.increment_reference();
    /* Decrement the reference count of the control block currently owned. */
    this->decrement_reference();
    /* Update the control block. */
    this->ctrl_block = other.ctrl_block;
  }  /* if */
  return *this;
}  /* Shared_obj::operator= */


template<typename an_Object, template<typename> class Allocator>
auto Shared_obj<an_Object, Allocator>::operator=(a_shared_object &&other)
                                                            -> a_shared_object&
/*
Move other to *this, then return *this.
*/
{
  /* An unconditional swap is faster than managing reference counts here. */
  swap_at(&this->ctrl_block, &other.ctrl_block);
  return *this;
}  /* Shared_obj::operator= */


template<typename an_Object, template<typename> class Allocator>
auto Shared_obj<an_Object, Allocator>::ptr() const -> an_object*
/*
Return a pointer to the shared object or NULL if this is the default (empty)
Shared_obj state.
*/
{
  an_object *result = NULL;

  if (this->ctrl_block != NULL) {
    return &this->ctrl_block->object;
  }  /* if */
  return result;
}  /* Shared_obj::ptr */


template<typename an_Object, template<typename> class Allocator>
void Shared_obj<an_Object, Allocator>::increment_reference() const
/*
If a reference to the shared object is currently present, increment it.

Note this is used to increment a control block owned by the "other" object
during copy assignment.  Thus, while it modifies the object in some sense,
it's allowed on const objects.
*/
{
  if (this->ctrl_block != NULL) {
    ++(this->ctrl_block->ref_counter);
  }  /* if */
}  /* Shared_obj::increment_reference */


template<typename an_Object, template<typename> class Allocator>
void Shared_obj<an_Object, Allocator>::decrement_reference()
/*
If a reference to the shared object is currently present, decrement its
counter.  If the counter hits 0, the object will be deallocated.
*/
{
  /* Disable the GCC "maybe uninitialized" warning, which may falsely flag
     the control block as being uninitialized in some contexts. */
BEGIN_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED
  if (this->ctrl_block != NULL) {
    if ((--(this->ctrl_block->ref_counter)) == 0) {
      an_allocation allocation{this->ctrl_block, sizeof(a_control_block)};

      destroy(this->ctrl_block);
      this->dealloc(allocation);
      this->ctrl_block = NULL;
    }  /* if */
  }  /* if */
END_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED
}  /* Shared_obj::decrement_reference */


template<typename an_Object,
         template<typename> class Allocator_A,
         template<typename> class Allocator_B>
INLINE a_boolean operator==(const Shared_obj<an_Object, Allocator_A> &obj_a,
                            const Shared_obj<an_Object, Allocator_B> &obj_b)
/*
Return TRUE if the given shared objects refer to the same underlying shared
object; otherwise, return FALSE.
*/
{
  return obj_a.ptr() == obj_b.ptr();
}  /* operator== */


template<typename an_Object,
         template<typename> class Allocator_A,
         template<typename> class Allocator_B>
INLINE a_boolean operator!=(const Shared_obj<an_Object, Allocator_A> &obj_a,
                            const Shared_obj<an_Object, Allocator_B> &obj_b)
/*
Return TRUE if the given shared objects refer to different underlying
shared objects; otherwise, return FALSE.
*/
{
  return !(obj_a == obj_b);
}  /* operator!= */


template<typename an_Object, typename ...an_Arg_pack>
INLINE Shared_obj<an_Object> shared_obj(an_Arg_pack&& ...args)
/*
Convenience function to create a shared pointer to an object allocated in
front-end memory.
*/
{
  return Shared_obj<an_Object>(typename Shared_obj<an_Object>::an_allocator(),
                               fwd<an_Arg_pack>(args)...);
}  /* shared_obj */


template<typename an_Object>
INLINE an_Object min_val(const an_Object &x,
                         const an_Object &y)
/*
Return the smaller of the given values.  If the values are equal, return the
first one.
*/
{
  return y < x ? y : x;
}  /* min_val */


template<typename an_Object>
INLINE an_Object max_val(const an_Object &x,
                         const an_Object &y)
/*
Return the larger of the given values.  If the values are equal, return the
first one.
*/
{
  return x < y ? y : x;
}  /* max_val */


template<typename an_Object>
INLINE an_Object& min_ref(an_Object &x,
                          an_Object &y)
/*
Return a reference to the smaller of the given referenced values.  If the
values are equal, return the first one.
*/
{
  return y < x ? y : x;
}  /* min_ref */


template<typename an_Object>
INLINE an_Object& max_ref(an_Object &x,
                          an_Object &y)
/*
Return a reference to the larger of the given referenced values.  If the
values are equal, return the first one.
*/
{
  return y < x ? y : x;
}  /* max_ref */


template<typename an_Integer>
INLINE int floor_log2(an_Integer n)
/*
Return floor(log2(n)), assuming n > 0.
*/
{
  int result = 0;

  while (n >>= 1) ++result;
  return result;
}  /* floor_log2 */


INLINE unsigned next_pow2(uint64_t n)
/*
Return the next power of two that is greater than or equal to n.
*/
{
  n--;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
  n |= n >> 32;
  return (unsigned)(n+1);
}  /* next_pow2 */


template<typename an_Unsigned_integer>
size_t count_ones(an_Unsigned_integer  n)
/*
Return the number of trailing "ones" in the binary representation of n.
Note: Recent versions of Clang and GCC optimize this function to just a popcnt
instruction when targeting x86-64 with SSE4 extensions (option -msse4).
*/
{
  size_t r = 0;

  while (n != 0) {
    ++r;
    /* If the bit representation of n is ...10...0 (all trailing zeroes), then
       n-1 is ...01...1 (all trailing ones), and the line below has the net
       effect of clearing the least significant "1". */
    n &= n-1;
  }  /* while */
  return r;
}  /* count_ones */


template<typename a_Ptr, typename a_Comparison>
INLINE void sort_args(a_Comparison cmp,
                      a_Ptr        p_a,
                      a_Ptr        p_b)
/*
Sort the sequence [*p_a, *p_b] according to the given comparison function.
*/
{
  if (cmp(*p_b, *p_a)) {
    swap_at(p_a, p_b);
  }  /* if */
}  /* sort_args */


template<typename a_Ptr, typename a_Comparison>
inline void sort_args(a_Comparison cmp,
                      a_Ptr        p_a,
                      a_Ptr        p_b,
                      a_Ptr        p_c)
/*
Sort the sequence [*p_a, *p_b, *p_c] according to the given comparison
function.  This implementation uses a decision tree to achieve an optimal
number of comparisons (2 or 3) and moves (0, 3, or 4).
*/
{
  typedef Value_for_ptr<a_Ptr> a_value;

  if (cmp(*p_a, *p_b)) {
    if (cmp(*p_c, *p_b)) {
      a_boolean  a_lt_c = cmp(*p_a, *p_c);
      a_value    tmp = move_from(a_lt_c ? p_c : p_a);
      if (a_lt_c) {
        /* a < c < b: */
        /* Finish swap_at(c, b) below. */
      } else {
        /* c <= a < b: */
        *p_a = move_from(p_c);
      }  /* if */
      *p_c = move_from(p_b);
      *p_b = move_from(&tmp);
    } else {
      /* a < b <= c: Nothing to do. */
    }  /* if */
  } else {
    a_value  tmp = move_from(p_a);
    if (cmp(*p_b, *p_c)) {
      *p_a = move_from(p_b);
      /* tmp holds the original *p_a. */
      if (cmp(tmp, *p_c)) {
        /* b <= a < c: */
        *p_b = move_from(&tmp);
      } else {
        /* b < c <= a: */
        *p_b = move_from(p_c);
        *p_c = move_from(&tmp);
      }  /* if */
    } else {
      *p_a = move_from(p_c);
      *p_c = move_from(&tmp);
    }  /* if */
  }  /* if */
}  /* sort_args */


template<typename a_Ptr, typename a_Comparison>
inline void sort_args(a_Comparison cmp,
                      a_Ptr        p_a,
                      a_Ptr        p_b,
                      a_Ptr        p_c,
                      a_Ptr        p_d)
/*
Sort the sequence [*p_a, *p_b, *p_c, *p_d] according to the given comparison
function.  This implementation uses a decision tree to achieve an optimal
number of comparisons (4 or 5) and moves (at most 6).
*/
{
  typedef Value_for_ptr<a_Ptr> a_value;

  if (cmp(*p_a, *p_c)) {
    if (cmp(*p_b, *p_d)) {
      if (cmp(*p_a, *p_b)) {
        if (cmp(*p_c, *p_d)) {
          if (cmp(*p_b, *p_c)) {
            /* a < b < c < d: Nothing to do. */
          } else {
            /* a < c <= b < d: */
            swap_at(p_c, p_b);
          }  /* if */
        } else {
          /* a < b < d <= c: */
          swap_at(p_c, p_d);
        }  /* if */
      } else {
        a_value tmp = move_from(p_a);
        *p_a = move_from(p_b);
        if (cmp(*p_c, *p_d)) {
          /* b <= a < c < d: */
          *p_b = move_from(&tmp);
        } else {
          /* tmp holds the original *p_a. */
          if (cmp(tmp, *p_d)) {
            /* b <= a < d <= c: */
            *p_b = move_from(&tmp);
            tmp = move_from(p_d);
          } else {
            /* b < d <= a < c: */
            *p_b = move_from(p_d);
          }  /* if */
          *p_d = move_from(p_c);
          *p_c = move_from(&tmp);
        }  /* if */
      }  /* if */
    } else {
      if (cmp(*p_a, *p_d)) {
        a_value tmp = move_from(p_b);
        if (cmp(*p_c, *p_b)) {
          if (cmp(*p_d, *p_c)) {
            /* a < d < c < b: */
            *p_b = move_from(p_d);
          } else {
            /* a < c <= d <= b: */
            *p_b = move_from(p_c);
            *p_c = move_from(p_d);
          }  /* if */
          *p_d = move_from(&tmp);
        } else {
          /* a < d <= b <= c: */
          *p_b = move_from(p_d);
          *p_d = move_from(p_c);
          *p_c = move_from(&tmp);
        }  /* if */
      } else {
        a_value  tmp = move_from(p_a);
        *p_a = move_from(p_d);
        if (cmp(*p_c, *p_b)) {
          /* d <= a < c < b: */
          *p_d = move_from(p_b);
          *p_b = move_from(&tmp);
        } else {
          *p_d = move_from(p_c);
          /* tmp holds the original *p_a. */
          if (cmp(tmp, *p_b)) {
            /* d <= a < b <= c: */
            *p_c = move_from(p_b);
            *p_b = move_from(&tmp);
          } else {
            /* d <= b <= a < c: */
            *p_c = move_from(&tmp);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  } else {
    if (cmp(*p_b, *p_d)) {
      if (cmp(*p_c, *p_b)) {
        a_value  tmp = move_from(p_a);
        *p_a = move_from(p_c);
        /* tmp holds the original *p_a. */
        if (cmp(tmp, *p_d)) {
          if (cmp(*p_b, tmp)) {
            /* c < b < a < d: */
            *p_c = move_from(&tmp);
          } else {
            /* c <= a <= b < d: */
            *p_c = move_from(p_b);
            *p_b = move_from(&tmp);
          }  /* if */
        } else {
          /* c <= a, c < b < d <= a: */
          *p_c = move_from(p_d);
          *p_d = move_from(&tmp);
        }  /* if */
      } else {
        a_value  tmp = move_from(p_a);
        *p_a = move_from(p_b);
        /* tmp holds the original *p_a. */
        if (cmp(tmp, *p_d)) {
          /* b <= c <= a < d: */
          *p_b = move_from(p_c);
          *p_c = move_from(&tmp);
        } else {
          if (cmp(*p_c, *p_d)) {
            /* b <= c < d <= a: */
            *p_b = move_from(p_c);
            *p_c = move_from(p_d);
          } else {
            /* b < d <= c <= a: */
            *p_b = move_from(p_d);
          }  /* if */
          *p_d = move_from(&tmp);
        }  /* if */
      }  /* if */
    } else {
      if (cmp(*p_c, *p_d)) {
        a_value  tmp = move_from(p_a);
        *p_a = move_from(p_c);
        /* tmp holds the original *p_a. */
        if (cmp(tmp, *p_b)) {
          if (cmp(*p_d, tmp)) {
            /* c < d < a < b: */
  	    *p_c = move_from(&tmp);
            tmp = move_from(p_d);
          } else {
            /* c <= a <= d <= b: */
            *p_c = move_from(p_d);
          }  /* if */
          *p_d = move_from(p_b);
          *p_b = move_from(&tmp);
        } else {
          /* c < d <= b <= a: */
          *p_c = move_from(p_b);
          *p_b = move_from(p_d);
  	  *p_d = move_from(&tmp);
        }  /* if */
      } else {
        a_value  tmp = move_from(p_a);
        *p_a = move_from(p_d);
        /* tmp holds the original *p_a. */
        if (cmp(tmp, *p_b)) {
          /* d <= c <= a < b: */
          *p_d = move_from(p_b);
          *p_b = move_from(p_c);
  	  *p_c = move_from(&tmp);
        } else {
  	  *p_d = move_from(&tmp);
          if (cmp(*p_c, *p_b)) {
            /* d <= c < b <= a: */
            swap_at(p_c, p_b);
          } else {
            /* d <= b <= c <= a: */
            /* swap_at(a, d) already done by the moves above. */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* sort_args */


template<a_boolean unguarded = FALSE, int moves_limit = 0,
         typename a_Ptr, typename a_Comparison>
inline a_boolean insertion_sort(a_Ptr        first,
                                a_Ptr        last,
                                a_Comparison cmp)
/*
Sort the given sequence using insertion sort.  If unguarded is TRUE, assume
*(first-1) is valid and already in a valid position for the sorted version of
[first-1, last).  If moves_limit is nonzero and more than moves_limit moves
are performed, the sort may be abandoned and FALSE returned.  Otherwise, TRUE
is returned.
*/
{
  typedef Remove_ref<decltype(*first)> a_value;

  a_boolean result = TRUE;
  if (last-first > 1) {
    int moves = 0;
    
    for (a_Ptr it = first;;) {
      a_Ptr sift_1 = it;
      a_Ptr sift = ++it;
      if (it == last) break;

      if (cmp(*sift, *sift_1)) {
        /* Use a move-chain to move *sift to the correct position in the
           sequence sorted thus far. */
        a_value tmp = move_from(sift);
        do {
          *sift-- = move_from(sift_1);
          if (moves_limit != 0) ++moves;
        } while ((unguarded || sift != first) && cmp(tmp, *--sift_1));
        *sift = move_from(&tmp);
        if (moves_limit != 0 && moves > moves_limit) {
          result = FALSE;
          break;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  return result;
}  /* insertion_sort */


template<typename a_Ptr, typename a_Comparison>
inline void heapify(a_Ptr        ptr,
                    size_t       len,
                    size_t       i,
                    a_Comparison cmp)
/*
This is an adaptation of the "heapify" procedure as described in "Introduction
To Algorithms" by Cormen, Leiserson, and Rivest (CLR, first edition).  Some
changes were made:
    (1) the comparison was changed from "x > y" to "cmp(y, x)", and the name
        "largest" was replaced by "extreme".
    (2) the indexing of the array is changed to "base 0" (from "base 1")
    (3) the logic was changed from "recursive" to "iterative"
    (4) the resulting loop was unrolled once so that a chain of "exchange"
        operations (aka. swap_at) could be replaced by a chain of "move"
        operations.

ptr (called "A" in CLR) is a random access iterator ("pointer") to the start of
a sequence of length len, treated as a binary tree (the children of an element
at index k are at indices 2*k+1 and 2*k+2).  The tree rooted at index i
satisfies the heap condition, except perhaps for the root element itself.  This
function fixes the root element my moving it down the tree if needed.
*/
{
  typedef Remove_ref<decltype(*ptr)> a_value;

  size_t  l = 2*i+1, r = l+1, extreme;

  if (l<len && cmp(*(ptr+i), *(ptr+l))) {
    extreme = l;
  } else {
    extreme = i;
  }  /* if */
  if (r<len && cmp(*(ptr+extreme), *(ptr+r))) {
    extreme = r;
  }  /* if */

  if (extreme != i) {
    /* Move the i element to a temporary, until we know where
       it fits. */
    a_value  tmp = move_from(ptr+i);
    *(ptr+i) = move_from(ptr+extreme);
    i = extreme;
    for (;;) {
      a_Ptr  p_extreme;
      l = 2*i+1, r = l+1;
      if (l<len && cmp(tmp, *(ptr+l))) {
        extreme = l;
        p_extreme = ptr+l;
      } else {
        extreme = i;
        p_extreme = &tmp;
      }  /* if */
      if (r<len && cmp(*p_extreme, *(ptr+r))) {
        extreme = r;
        p_extreme = ptr+r;
      }  /* if */
      if (extreme != i) {
        *(ptr+i) = move_from(p_extreme);
        i = extreme;
      } else {
        break;
      }  /* if */
    }  /* for */
    /* i is the index of the last "extreme" that was moved up.  Move the top
       (misplaced) element to this position. */
    *(ptr+i) = move_from(&tmp);
  }  /* if */
}  /* heapify */


template<typename a_Ptr, typename a_comparison>
void build_heap(a_Ptr        first,
                size_t       len,
                a_comparison cmp)
/*
This is an adaptation of the "Build-Heap" procedure as described in
"Introduction To Algorithms" by Cormen, Leiserson, and Rivest (CLR, first
edition).
*/
{
  for (size_t i = len/2; i > 0;) {
    --i;
    heapify(first, len, i, cmp);
  }  /* for */
}  /* build_heap */


template<typename a_Ptr, typename a_comparison>
void heap_sort(a_Ptr        first,
               a_Ptr        last,
               a_comparison cmp)
/*
This is an adaptation of the "Heapsort" procedure as described in "Introduction
To Algorithms" by Cormen, Leiserson, and Rivest (CLR, first edition).
*/
{
  ptrdiff_t len = last - first;

  if (len > 1) {
    build_heap(first, (size_t)len, cmp);
    for (ptrdiff_t i = len; i > 1;) {
      --i;
      swap_at(first, first+i);
      --len;
      heapify(first, (size_t)len, 0, cmp);
    }  /* for */
  }  /* if */
}  /* heap_sort */


namespace pdqsort_impl {
/* An adaptation of Orson Peters' "pattern-defeating quicksort" (PDQSort).
   This is not the original code, but an adaptation of it.  The original
   copyright notice read as follows:

    pdqsort.h - Pattern-defeating quicksort.

    Copyright (c) 2015 Orson Peters

    This software is provided 'as-is', without any express or implied warranty.
    In no event will the authors be held liable for any damages arising from
    the use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
       claim that you wrote the original software. If you use this software in
       a product, an acknowledgment in the product documentation would be
       appreciated but is not required.

    2. Altered source versions must be plainly marked as such, and must not be
       misrepresented as being the original software.

    3. This notice may not be removed or altered from any source distribution.
*/

/*
The maximum length of a sequence for which insertion sort is used.
*/
#define MAX_INSERTION_SORT_LENGTH 15

/*
The minimum length of a sequence to cause the pivot to be selected using
Tukey's ninther.
*/
#define MIN_NINTHER_PIVOT_LENGTH 129

/*
Length in bytes of a cache line (must be a power of 2).
*/
#define CACHE_LINE_SIZE 64

/*
The input is split up in blocks of the following length.  This follows the
approach of "BlockQuicksort: How Branch Mispredictions don't affect Quicksort"
by Stefan Edelkamp and Armin Weiss.  The block size should be a multiple of 8,
and no more than 248 (it must fit in a byte).
*/
#define BLOCK_SIZE ((a_byte)64)


template<typename an_Object>
INLINE an_Object* align_to_cache_line(an_Object *p)
/*
Return p minimally advanced to be aligned to a cache line.
*/
{
  return (an_Object*)
             (((uintptr_t)p+CACHE_LINE_SIZE-1) & (uintptr_t)-CACHE_LINE_SIZE);
}  /* align_to_cache_line */


template<typename a_Ptr>
inline void move_using_offsets(a_Ptr     first,
                               a_Ptr     last,
                               a_byte*   offsets_l,
                               a_byte*   offsets_r,
                               int       num,
                               a_boolean use_swaps)
/*
Given a sequence [first, last], offset_l[0..num-1] are forward offsets wrt.
first and offset_r[0..num-1] are backward offsets wrt. last.  Let l0, l1, ...
be the elements designated by offset_l and r0, r1, ... be the elements
designated by offset_r.  If use_swaps is TRUE, swap (l0, r0), then (l1, r1),
then (l2, r2), etc.  Otherwise, perform a circular move chain:
    tmp <- l0 <- r0 <- l1 <- r1 <- l2 ... < r[num-1] <- tmp
Either way, the ln values will be swapped with the rn values.
*/
{
  typedef Value_for_ptr<a_Ptr> a_value;
  if (num != 0) {
    if (use_swaps) {
      /* This case is needed for the descending distribution, where we need
         to have proper swapping for pdqsort to remain O(n). */
      for (int i = 0; i < num; ++i) {
        swap_at(first+offsets_l[i], last-offsets_r[i]);
      }  /* for */
    } else {
      a_Ptr   l = first + offsets_l[0],
              r = last - offsets_r[0];
      a_value tmp = move_from(l);
      *l = move_from(r);
      for (int i = 1; i < num; ++i) {
        l = first+offsets_l[i];
        *r = move_from(l);
        r = last-offsets_r[i];
        *l = move_from(r);
      }  /* for */
      *r = move_from(&tmp);
    }  /* if */
  }  /* if */
}  /* move_using_offsets */


template<typename a_Ptr, typename a_Comparison>
inline Ptr_with_flag<a_Ptr> partition_right_branchless(a_Ptr        begin,
                                                       a_Ptr        end,
                                                       a_Comparison cmp)
/*
Partition the sequence [begin, end) around pivot *begin using comparison
function cmp.  Elements equal to the pivot are put in the right-hand partition.
Return as a "pointer with flag" (a) the position of the pivot after
partitioning and (b) whether the passed sequence already was correctly
partitioned.  This function assumes the pivot is a median of at least 3
elements and that [begin, end) is at least MAX_INSERTION_SORT_LENGTH+1 long.
Uses branchless partitioning.
*/
{
  typedef Value_for_ptr<a_Ptr> a_value;

  /* Cache the pivot in a local variable. */
  a_value pivot = move_from(begin);
  a_Ptr first = begin, last = end;

  /* Find the first element greater than or equal to the pivot (the median of
     3 guarantees this exists). */
  while (cmp(*++first, pivot)) {}
  /* Find the first element strictly smaller than the pivot. We have to guard
     this search if there was no element before *first. */
  if (first-1 == begin) {
    while (first < last && !cmp(*--last, pivot)) {}
  } else {
    while (!cmp(*--last, pivot)) {}
  }  /* if */

  /* If the first pair of elements that should be swapped to partition are
     the same element, the passed in sequence already was correctly
     partitioned. */
  a_boolean already_partitioned = first >= last;
  if (!already_partitioned) {
    swap_at(first, last);
    ++first;
  }  /* if */

  /* The following branchless partitioning is derived from "BlockQuicksort:
     How Branch Mispredictions don't affect Quicksort" by Stefan Edelkamp
     and Armin Weiss. */
  a_byte  offsets_l_storage[BLOCK_SIZE+CACHE_LINE_SIZE],
          offsets_r_storage[BLOCK_SIZE+CACHE_LINE_SIZE];
  a_byte* offsets_l = align_to_cache_line(offsets_l_storage);
  a_byte* offsets_r = align_to_cache_line(offsets_r_storage);
  int     num_l = 0, num_r = 0, start_l = 0, start_r = 0;
  while (last-first > 2*BLOCK_SIZE) {
    /* Fill up offset blocks with elements that are on the wrong side. */
    if (num_l == 0) {
      start_l = 0;
      a_Ptr it = first;
      for (a_byte i = 0; i < BLOCK_SIZE;) {
        offsets_l[num_l] = i++; num_l += !cmp(*it, pivot); ++it;
        offsets_l[num_l] = i++; num_l += !cmp(*it, pivot); ++it;
        offsets_l[num_l] = i++; num_l += !cmp(*it, pivot); ++it;
        offsets_l[num_l] = i++; num_l += !cmp(*it, pivot); ++it;
        offsets_l[num_l] = i++; num_l += !cmp(*it, pivot); ++it;
        offsets_l[num_l] = i++; num_l += !cmp(*it, pivot); ++it;
        offsets_l[num_l] = i++; num_l += !cmp(*it, pivot); ++it;
        offsets_l[num_l] = i++; num_l += !cmp(*it, pivot); ++it;
      }  /* for */
    }  /* if */
    if (num_r == 0) {
      start_r = 0;
      a_Ptr it = last;
      for (a_byte i = 0; i < BLOCK_SIZE;) {
        offsets_r[num_r] = ++i; num_r += cmp(*--it, pivot);
        offsets_r[num_r] = ++i; num_r += cmp(*--it, pivot);
        offsets_r[num_r] = ++i; num_r += cmp(*--it, pivot);
        offsets_r[num_r] = ++i; num_r += cmp(*--it, pivot);
        offsets_r[num_r] = ++i; num_r += cmp(*--it, pivot);
        offsets_r[num_r] = ++i; num_r += cmp(*--it, pivot);
        offsets_r[num_r] = ++i; num_r += cmp(*--it, pivot);
        offsets_r[num_r] = ++i; num_r += cmp(*--it, pivot);
      }  /* for */
    }  /* if */

    /* Move the misplaced elements to the other side and update block sizes
       and first/last boundaries. */
    int num = min_val(num_l, num_r);
    move_using_offsets(first, last, offsets_l+start_l, offsets_r+start_r,
                       num, num_l == num_r);
    num_l -= num; num_r -= num;
    start_l += num; start_r += num;
    if (num_l == 0) first += BLOCK_SIZE;
    if (num_r == 0) last -= BLOCK_SIZE;
  }  /* while */

  int l_size = 0, r_size = 0,
      unknown_left = (int)(last-first) - ((num_r || num_l) ? BLOCK_SIZE : 0);
  /* Handle a leftover block by assigning the unknown elements to the other
     block. */
  if (num_r) {
    l_size = unknown_left;
    r_size = BLOCK_SIZE;
  } else if (num_l) {
    l_size = BLOCK_SIZE;
    r_size = unknown_left;
  } else {
    /* No leftover block: Split the unknown elements in two blocks. */
    l_size = unknown_left/2;
    r_size = unknown_left-l_size;
  }  /* if */

  /* Fill offset buffers if needed. */
  if (unknown_left && !num_l) {
    start_l = 0;
    a_Ptr it = first;
    for (unsigned char i = 0; i < l_size;) {
      offsets_l[num_l] = i++; num_l += !cmp(*it, pivot); ++it;
    }  /* for */
  }  /* if */
  if (unknown_left && !num_r) {
    start_r = 0;
    a_Ptr it = last;
    for (unsigned char i = 0; i < r_size;) {
      offsets_r[num_r] = ++i; num_r += cmp(*--it, pivot);
    }  /* for */
  }  /* if */

  int num = min_val(num_l, num_r);
  move_using_offsets(first, last, offsets_l+start_l, offsets_r+start_r,
                     num, num_l == num_r);
  num_l -= num; num_r -= num;
  start_l += num; start_r += num;
  if (num_l == 0) first += l_size;
  if (num_r == 0) last -= r_size;
      
  /* We have now fully identified [first, last)'s proper position: Swap the
     last elements. */
  if (num_l) {
    offsets_l += start_l;
    while (num_l--) {
      swap_at(first+offsets_l[num_l], --last);
    }  /* while */
    first = last;
  }  /* if */
  if (num_r) {
    offsets_r += start_r;
    while (num_r--) {
      swap_at(last-offsets_r[num_r], first);
      ++first;
    }  /* while */
    last = first;
  }  /* if */

  /* Put the pivot in the right place. */
  a_Ptr pivot_pos = first-1;
  *begin = move_from(pivot_pos);
  *pivot_pos = move_from(&pivot);
  return ptr_with_flag(pivot_pos, already_partitioned);
}  /* partition_right_branchless */


template<typename a_Ptr, typename a_Comparison>
inline Ptr_with_flag<a_Ptr> partition_right(a_Ptr        begin,
                                            a_Ptr        end,
                                            a_Comparison cmp)
/*
Partition [begin, end) around pivot *begin using comparison function cmp.
Elements equal to the pivot are put in the right-hand partition.  Return the
position of the pivot after partitioning and whether the passed sequence
already was correctly partitioned.  This function assumes the pivot is a
median of at least 3 elements and that [begin, end) is at least
MAX_INSERTION_SORT_LENGTH+1 long.
*/
{
  typedef Value_for_ptr<a_Ptr> a_value;

  /* Cache the pivot in a local variable. */
  a_value pivot = move_from(begin);
  a_Ptr first = begin, last = end;

  /* Find the first element greater than or equal to the pivot (the median of
     3 guarantees this exists). */
  while (cmp(*++first, pivot)) {}
  /* Find the first element strictly smaller than the pivot.  We have to guard
     this search if there was no element before *first. */
  if (first-1 == begin) {
    while (first < last && !cmp(*--last, pivot)) {}
  } else {
    while (!cmp(*--last, pivot)) {}
  }  /* if */

  /* If the first pair of elements that should be swapped to partition are
     the same element, the passed-in sequence already was correctly
     partitioned. */
  a_boolean already_partitioned = first >= last;
    
  /* Keep swapping pairs of elements that are on the wrong side of the pivot.
     Previously swapped pairs guard the searches, which is why the first
     iteration is special-cased above. */
  while (first < last) {
    swap_at(first, last);
    while (cmp(*++first, pivot)) {}
    while (!cmp(*--last, pivot)) {}
  }  /* while */

  /* Put the pivot in the right place. */
  a_Ptr pivot_pos = first-1;
  *begin = move_from(pivot_pos);
  *pivot_pos = move_from(&pivot);

  return ptr_with_flag(pivot_pos, already_partitioned);
}  /* partition_right */


template<typename a_Ptr, typename a_Comparison>
inline a_Ptr partition_left(a_Ptr        begin,
                            a_Ptr        end,
                            a_Comparison cmp)
/*
Same as partition_right, except elements equal to the pivot are put to the
left of the pivot and only the pivot position is returned.  Since this is only
used for the "many equal" case -- which is somewhat rare -- and in that case
pdqsort already has O(n) performance, no block quicksort is applied here for
simplicity.
*/
{
  typedef Value_for_ptr<a_Ptr> a_value;

  /* Cache the pivot in a local variable. */
  a_value pivot = move_from(begin);
  a_Ptr first = begin, last = end;

  while (cmp(pivot, *--last)) {}

  if (last+1 == end) {
    while (first < last && !cmp(pivot, *++first)) {}
  } else {
    while (!cmp(pivot, *++first)) {}
  }  /* if */
  while (first < last) {
    swap_at(first, last);
    while (cmp(pivot, *--last)) {}
    while (!cmp(pivot, *++first)) {}
  }  /* while */

  a_Ptr pivot_pos = last;
  *begin = move_from(pivot_pos);
  *pivot_pos = move_from(&pivot);

  return pivot_pos;
}  /* partition_left */


template<a_boolean branchless, typename a_Ptr, typename a_Comparison>
void pdqsort_loop(a_Ptr        begin,
                  a_Ptr        end,
                  a_Comparison cmp,
                  int          bad_allowed,
                  a_boolean    leftmost)
/*
Sort the elements in [begin, end) according to the comparison object cmp.  The
primary algorithm is Quicksort.  If branchless is TRUE, a variation of
Quicksort is used to reduce the number of mispredicted branches.  If too many
partition pairs are highly unbalanced, the algorithm switches to heapsort to
sort the leaf partitions.  "Too many" in that context is determined by
bad_allowed.  This function is called recursively on left partitions.  For
recursive calls that do not include the original left-most element, leftmost
is FALSE.
*/
{
  /* Use a loop to eliminate tail-recursion on the right partition. */
  for (;;) {
    a_ptrdiff length = end-begin;

    /* Use insertion_sort for small arrays. */
    if (length < MAX_INSERTION_SORT_LENGTH+1) {
      if (leftmost) {
        insertion_sort(begin, end, cmp);
      } else {
        insertion_sort</*unguarded=*/TRUE>(begin, end, cmp);
      }  /* if */
      break;
    }  /* if */

    /* Choose a pivot as the median of 3 or the Tukey ninther (pseudo-median
       of 9). */
    a_ptrdiff half_length = length/2;
    a_Ptr     mid = begin+half_length;
    if (length >= MIN_NINTHER_PIVOT_LENGTH) {
      sort_args(cmp, begin, mid, end-1);
      sort_args(cmp, begin+1, mid-1, end-2);
      sort_args(cmp, begin+2, mid+1, end-3);
      sort_args(cmp, mid-1, mid, mid+1);
      swap_at(begin, mid);
    } else {
      sort_args(cmp, mid, begin, end-1);
    }  /* if */

    /* If *(begin-1) is the end of the right partition of a previous partition
       operation there is no element in [begin, end) that is smaller than
       *(begin-1).  Then if our pivot compares equal to *(begin-1) we change
       strategy, putting equal elements in the left partition and greater
       elements in the right partition.  In that case, we do not have to
       process the left partition any further because all its values are equal
       and therefore already sorted. */
    if (!leftmost && !cmp(*(begin-1), *begin)) {
      begin = partition_left(begin, end, cmp)+1;
      continue;
    }  /* if */

    /* Partition [begin, end) around the pivot *begin. */
    Ptr_with_flag<a_Ptr> part_result =
                    branchless ? partition_right_branchless(begin, end, cmp)
                               : partition_right(begin, end, cmp);
    a_Ptr      pivot_pos = part_result.ptr();
    a_boolean  already_partitioned = part_result.flagged();

    /* If the partitions are highly unbalanced, shuffle some elements to break
       many patterns. */
    a_ptrdiff  l_size = pivot_pos-begin;
    a_ptrdiff  r_size = end-(pivot_pos+1);
    a_boolean  highly_unbalanced = (l_size < length/8) || (r_size < length/8);
    if (highly_unbalanced) {
      /* If we have seen too many highly-unbalanced partitions, switch to
         heapsort to guarantee O(n log n). */
      if (--bad_allowed == 0) {
          heap_sort(begin, end, cmp);
          break;
      }  /* if */
      if (l_size >= MAX_INSERTION_SORT_LENGTH+1) {
        /* Shuffle some elements around, unless we will use insertion sort on
           the next level. */
        swap_at(begin, begin+l_size/4);
        swap_at(pivot_pos-1, pivot_pos-l_size/4);

        if (l_size > MIN_NINTHER_PIVOT_LENGTH-1) {
          swap_at(begin+1, begin+(l_size/4+1));
          swap_at(begin+2, begin+(l_size/4+2));
          swap_at(pivot_pos-2, pivot_pos-(l_size/4+1));
          swap_at(pivot_pos-3, pivot_pos-(l_size/4+2));
        }  /* if */
      }  /* if */
      if (r_size >= MAX_INSERTION_SORT_LENGTH+1) {
        /* Shuffle some elements around, unless we will use insertion sort on
           the next level. */
        swap_at(pivot_pos+1, pivot_pos+(1+r_size/4));
        swap_at(end-1, end-r_size/4);
            
        if (r_size > MIN_NINTHER_PIVOT_LENGTH-1) {
          swap_at(pivot_pos+2, pivot_pos+(2+r_size/4));
          swap_at(pivot_pos+3, pivot_pos+(3+r_size/4));
          swap_at(end-2, end-(1+r_size/4));
          swap_at(end-3, end-(2+r_size/4));
        }  /* if */
      }  /* if */
    } else {
      /* If the partitions were somewhat balanced and we tried to sort an
         already-partitioned sequence try to use insertion sort, but give up
         after more than about 8 moves. */
      if (already_partitioned &&
          insertion_sort</*unguarded=*/FALSE, /*moves_limit=*/8>(
                                                     begin, pivot_pos, cmp) &&
          insertion_sort</*unguarded=*/FALSE, /*moves_limit=*/8>(
                                                   pivot_pos + 1, end, cmp)) {
           break;
      }  /* if */
    }  /* if */
        
    /* Recurse to sort the left partition.  The right partition is handled
       by the loop (i.e., we eliminated tail recursion). */
    pdqsort_loop<branchless>(begin, pivot_pos, cmp, bad_allowed, leftmost);
    begin = pivot_pos+1;
    leftmost = FALSE;
  }  /* for */
}  /* pdqsort_loop */

#undef BLOCK_SIZE
#undef CACHE_LINE_SIZE
#undef MIN_NINTHER_PIVOT_LENGTH
#undef MAX_INSERTION_SORT_LENGTH

}  /* namespace pdqsort_impl */

template<typename a_Ptr, typename a_Comparison>
INLINE void sort(a_Ptr        begin,
                 a_Ptr        end,
                 a_Comparison cmp)
/*
Sort [begin, end) with Orson Peters' "pattern-defeating quicksort" (PDQSort).
a_Ptr must be a random-access iterator.  cmp must induce a weak ordering on
the elements of the sequence.
*/
{
  if (begin != end) {
    pdqsort_impl::pdqsort_loop</*branchless=*/TRUE>(
                  begin, end, cmp, floor_log2(end-begin), /*left_most=*/TRUE);
  }  /* if */
}  /* sort */


template<typename a_Sequence, typename a_Comparison>
INLINE void sort(a_Sequence    *p_seq,
                 a_Comparison  cmp)
/*
Sort the elements of the given sequence.
*/
{
  sort(p_seq->begin(), p_seq->end(), cmp);
}  /* sort */


template<typename a_Forward_iterator>
a_Forward_iterator rotate(a_Forward_iterator first,
                          a_Forward_iterator middle,
                          a_Forward_iterator last)
/*
Swap the elements in the range [first, last) in such a way that the elements in
[first, middle) are placed after the elements in [middle, last) while the
orders of the elements in both ranges are preserved.

If first == middle return the iterator last.  If middle == last return the
iterator first.  Otherwise, return the iterator first + (last - middle).
*/
{
  if (first == middle) {
    return last;
  }  /* if */
  if (middle == last) {
    return first;
  }  /* if */

  a_Forward_iterator write = first;
  a_Forward_iterator next_read = first;
  for (a_Forward_iterator read = middle; read != last; ++write, ++read) {
    if (write == next_read) {
      next_read = read;
    }  /* if */
    swap_at(write, read);
  }  /* for */
  (void)rotate(write, next_read, last);
  return write;
}  /* rotate */

#if DEBUG

template<typename T>
void db_f_print_t(FILE *stream, ARG_UNUSED const T &value)
/*
Provide a generic printing interface for generic function diagnostics.
*/
{
  fprintf(stream, "unspecified");
}  /* db_f_print_t */

#endif /* DEBUG */
#if EXPENSIVE_CHECKING && !STANDALONE_UTILITY_PROGRAM

template<typename a_Value_Fn>
inline void validate_elements_in_order(size_t     num_elements,
                                       a_Value_Fn value_fn)
/*
Validate that the input container of num_elements elements is in order for a
binary search.  This is implemented in terms of operator< and operator== to
minimize the number of operators that need to be implemented.
*/
{
  a_boolean any_out_of_order = FALSE;
  auto &&last_value = value_fn(0);

  for (size_t i = 1; i < num_elements; ++i) {
    auto &&curr_value = value_fn(i);
    if (!(last_value < curr_value || last_value == curr_value)) {
      fprintf(stderr, "Binary search element %td (", i);
#if DEBUG
      db_f_print_t(stderr, curr_value);
#else /* !DEBUG */
      fprintf(stderr, "unspecified");
#endif /* DEBUG */
      fprintf(stderr, ") is out of order\n");
      any_out_of_order = TRUE;
    }  /* if */
    last_value = curr_value;
  }  /* for */
  /* Fail loudly if anything is not ordered correctly, delayed so multiple
     order errors can be reported first. */
  check_assertion(!any_out_of_order);
}  /* validate_elements_in_order */


/* Forward declare no_very_expensive_checking. */
extern EDG_THREAD a_boolean
                no_very_expensive_checking;

#endif /* EXPENSIVE_CHECKING && !STANDALONE_UTILITY_PROGRAM */

template<typename T, typename a_Value_Fn>
inline ptrdiff_t low_bound(size_t     num_elements,
                           const T    &value,
                           a_Value_Fn value_fn)
/*
Search for the first element in a container of num_elements elements that is
not less than (i.e., greater or equal to) value using value_fn to retrieve
values at a given index.  value_fn should take a ptrdiff_t argument
representing a given index into the container, and return the value of type T
at that index.  Return the index of said element or -1 if no such element is
found.
*/
{
  ptrdiff_t result;
  size_t    begin_idx = 0;
  size_t    curr_size = num_elements;

#if EXPENSIVE_CHECKING && !STANDALONE_UTILITY_PROGRAM
  if (!no_very_expensive_checking && num_elements >= 2) {
    validate_elements_in_order(num_elements, value_fn);
  }  /* if */
#endif /* EXPENSIVE_CHECKING && !STANDALONE_UTILITY_PROGRAM */
  while (curr_size > 0) {
    /* Calculate the current index.  First compute an index relative to the
       amount of data we have.  Then add the relative index to our starting
       index to get the true index into the unknown container. */
    size_t midpoint_idx = curr_size / 2;
    size_t curr_idx = begin_idx + midpoint_idx;
    /* Retrieve the value at our current index from the unknown container.
       Extracted to a variable to ease debugging. */
    auto &&curr_value = value_fn(curr_idx);
    if (curr_value < value) {
      /* The value was smaller than the value we're searching for (we want the
         first value greater than or equal to our target value) so consider the
         value to our immediate right the new best candidate.

         The best candidate and starting search positions are algorithmically
         equivalent. begin_idx is the largest value we know of that was
         preceded by a value smaller than our target value.  Thus, it's both
         our best current answer to the lower bound, and the earliest point we
         will need to check going forward.  Therefore, update begin_idx with
         this new best candidate index.

         Since this algorithm works with a rolling count of elements rather
         than indexes, we need to subtract the number of elements we just
         removed (i.e., our midpoint index + 1) to make the count inclusive.
         Note that since we may have an even or odd count, we can't do a blind
         assignment of curr_size / 2 (inflates the count when even) or
         curr_size / 2 - 1 (deflates the count when odd) without adding an
         additional branch.

         Additionally, note that if the index we've moved onto is past the end
         of the array that's okay as we will end up with num_elements (which
         then gets transformed to -1 at the end of the function) as intended,
         and we will not reenter as curr_size will be 0. */
      begin_idx = curr_idx + 1;
      curr_size -= midpoint_idx + 1;
    } else {
      /* The value was greater than or equal to the value we're searching for,
         so we need to consider earlier elements.

         Since this algorithm works with a rolling count of elements rather
         than indexes, we need to subtract the number of elements we just
         removed.  Since our midpoint already in effect represents the number
         of elements in the left half that remain to be examined, simply use
         its value. */
      curr_size = midpoint_idx;
    }  /* if */
  }  /* while */
  /* We ran out of elements and ran past the end of the container, detect this
     and return -1 (to better to conform to developer expectations of "no
     result"). */
  if (begin_idx == num_elements) {
    result = -1;
  } else {
    result = (ptrdiff_t)begin_idx;
  }  /* if */
  return result;
}  /* low_bound */


template<typename T, typename a_Value_Fn>
INLINE ptrdiff_t bin_search(size_t     num_elements,
                            const T    &value,
                            a_Value_Fn value_fn)
/*
Search for the first element in a container of num_elements elements that is
equal to value using value_fn to retrieve values at a given index.  value_fn
should take a ptrdiff_t argument representing a given index into the container,
and return the value of type T at that index.  Return the index of said element
or -1 if no such element is found.
*/
{
  ptrdiff_t result_idx = low_bound(num_elements, value, value_fn);

  /* If we received a valid result index into our container, check to see if
     the value at the result index matches the value we were searching for.

     Use !(x == y) rather than x != y to simplify implementing wrapper types we
     may want to feed to bin_search. */
  if (result_idx != -1 && !(value_fn((size_t)result_idx) == value)) {
    /* The value didn't match so invalidate the result index. */
    result_idx = -1;
  }  /* if */
  return result_idx;
}  /* bin_search */


template<typename T>
INLINE ptrdiff_t array_bin_search(T       *t_start,
                                  size_t  num_elements,
                                  const T &value)
/*
Search for the first element in an array of num_elements elements beginning at
t_start, that is equal to value.  Return the index of said element or -1 if no
such element is found.
*/
{
  auto read_array_element_at = [t_start](ptrdiff_t idx) {
    return *(t_start + idx);
  };
  return bin_search(num_elements, value, read_array_element_at);
}  /* array_bin_search */


template<template<typename> class Allocator>
struct Allocated_string;

namespace detail {

/*
A struct used for formatting the given value in a hexadecimal representation.
This type should not be named outside of util.h.
*/
template<typename a_Type>
struct Hex_view {
  a_Type        value;  /* The value to be formatted in hex. */
  Hex_view(a_Type value_to_format)
    : value(value_to_format)
    {}
};  /* Hex_view */


/*
A struct used for formatting a given value as an octal value.
This type should not be named outside of util.h.
*/
struct an_octal_view {
  unsigned long long
                value;  /* The value to be formatted in octal. */
  an_octal_view(unsigned long long value_to_format)
    : value(value_to_format)
    {}
};  /* an_octal_view */


/*
Used to represent textual alignment for a padded string.
This type should not be named outside of util.h.
*/
enum a_text_alignment {
  ta_left,  /* The text shall be on the left. */
  ta_right  /* The text shall be on the right. */
};


/*
A struct used for aligning a value (converted to a string via
String_formatter<a_Value_type>) within a larger string of width characters.

As an example, a value_to_format of the integer value 10, with right alignment,
a padding character of X, and a width of 5, would result in the string "XXX10".

This type should not be named outside of util.h.
*/
template<typename a_Value_type>
struct Padded_string {
  a_text_alignment
                alignment;
                        /* The alignment of the given string. */
  size_t        width;  /* The desired width of the string. */
  char          padding_char;
                        /* The character used for padding. */
  a_Value_type  value;  /* The value to convert to text and then align. */
  Padded_string(a_text_alignment   text_alignment,
                size_t             text_width,
                char               text_padding_char,
                const a_Value_type &value_to_format)
    : alignment(text_alignment), width(text_width),
      padding_char(text_padding_char), value(value_to_format)
    {}
};  /* Padded_string */

}  /* namespace detail */

template<typename a_Type>
INLINE detail::Hex_view<a_Type> hex_view_of(a_Type value)
/*
*/
{
  return detail::Hex_view<a_Type>(value);
}  /* hex_view_of */


INLINE detail::an_octal_view octal_view_of(unsigned long long value)
/*
*/
{
  return detail::an_octal_view(value);
}  /* octal_view_of */


template<typename a_Value_type>
INLINE detail::Padded_string<a_Value_type>
left_pad(size_t             width,
         char               padding_char,
         const a_Value_type &value)
/*
Return a Padded_string configured to align the given string text to be the
given width.  If the given string text is not at least width characters long
padding of the given character will be added on the left side until it is the
appropriate size.
*/
{
  return detail::Padded_string<a_Value_type>(detail::ta_right, width,
                                             padding_char, value);
}  /* left_pad */


template<typename a_Value_type>
INLINE detail::Padded_string<a_Value_type>
right_pad(size_t             width,
          char               padding_char,
          const a_Value_type &value)
/*
Return a Padded_string configured to align the given string text to be the
given width.  If the given string text is not at least width characters long
padding of the given character will be added on the right side until it is the
appropriate size.
*/
{
  return detail::Padded_string<a_Value_type>(detail::ta_left, width,
                                             padding_char, value);
}  /* right_pad */


template<typename an_Integral_type>
INLINE constexpr
Enable_if<sizeof(an_Integral_type) == sizeof(uint32_t) &&
          Is_signed_helper<an_Integral_type>::value, int32_t>
max_integral_value()
/*
Return the maximum value of a 32-bit signed integer.
*/
{
  return INT32_MAX;
}  /* max_integral_value */


template<typename an_Integral_type>
INLINE constexpr
Enable_if<sizeof(an_Integral_type) == sizeof(uint64_t) &&
          Is_signed_helper<an_Integral_type>::value, int64_t>
max_integral_value()
/*
Return the maximum value of a 64-bit signed integer.
*/
{
  return INT64_MAX;
}  /* max_integral_value */

#if HOST_HAS_INT128_EXTENSIONS

template<typename an_Integral_type>
INLINE constexpr
Enable_if<sizeof(an_Integral_type) == sizeof(__int128_t) &&
          Is_signed_helper<an_Integral_type>::value, __int128_t>
max_integral_value()
/*
Return the maximum value of a 128-bit signed integer.
*/
{
  return ~(__int128_t(1) << 127);
}  /* max_integral_value */

#endif /* HOST_HAS_INT128_EXTENSIONS */

template<typename an_Integral_type>
INLINE constexpr
Enable_if<sizeof(an_Integral_type) == sizeof(uint32_t) &&
          !Is_signed_helper<an_Integral_type>::value, uint32_t>
max_integral_value()
/*
Return the maximum value of a 32-bit unsigned integer.
*/
{
  static_assert(!(an_Integral_type(-1) < an_Integral_type(0)),
                "integer type must be unsigned");
  return UINT32_MAX;
}  /* max_integral_value */


template<typename an_Integral_type>
INLINE constexpr
Enable_if<sizeof(an_Integral_type) == sizeof(uint64_t) &&
          !Is_signed_helper<an_Integral_type>::value, uint64_t>
max_integral_value()
/*
Return the maximum value of a 64-bit unsigned integer.
*/
{
  return UINT64_MAX;
}  /* max_integral_value */

#if HOST_HAS_INT128_EXTENSIONS

template<typename an_Integral_type>
INLINE constexpr
Enable_if<sizeof(an_Integral_type) == sizeof(__uint128_t) &&
          !Is_signed_helper<an_Integral_type>::value, __uint128_t>
max_integral_value()
/*
Return the maximum value of a 128-bit unsigned integer.
*/
{
  return ~(__uint128_t(0));
}  /* max_integral_value */

#endif /* HOST_HAS_INT128_EXTENSIONS */

template<typename an_Integral_type>
inline constexpr
Enable_if<Is_signed_helper<an_Integral_type>::value, size_t>
integral_digits(an_Integral_type value,
                int              base)
/*
Given a value, return the number of digits required to represent it for a
signed numbering system with the given base.  Note this function does not count
the negative sign as a digit (i.e., 3 and -3 are both considered 1 digit).
*/
{
  size_t digits = 1;

  for (; value != 0 && (value / base) != 0; ++digits) {
    value = static_cast<an_Integral_type>(value / base);
  }  /* for */
  return digits;
}  /* integral_digits */


template<typename an_Integral_type>
inline constexpr
Enable_if<!Is_signed_helper<an_Integral_type>::value, size_t>
integral_digits(an_Integral_type value,
                unsigned int     base)
/*
Given a value, return the number of digits required to represent it for a
unsigned numbering system with the given base.
*/
{
  size_t digits = 1;

  for (; value != 0 && (value / base) != 0; ++digits) {
    value = static_cast<an_Integral_type>(value / base);
  }  /* for */
  return digits;
}  /* integral_digits */


template<typename an_Integral_type>
INLINE size_t max_integral_digits()
/*
Compute the maximum number of digits a given unsigned integral type can hold.
Note this function does not count the negative sign as a digit (i.e., 3 and -3
are both considered 1 digit).
*/
{
  constexpr size_t result = integral_digits(
                                        max_integral_value<an_Integral_type>(),
                                        10);

  return result;
}  /* max_integral_digits */

namespace detail {

template<typename ...a_Format_arg>
INLINE int snprintf_impl(char         *dest_buff,
                         size_t       dest_buff_size,
                         a_const_char *format_str,
                         a_Format_arg ...args)
/*
Given the buffer to write to, the size of the buffer, the formatting string,
and the formatting string arguments, write the desired formatted string to
dest_buff with a terminating null character.  The number of characters written
(not including the terminating null character) is returned upon success.  If an
error occurs or the buffer was not sufficiently large an unspecified negative
value will be returned.
*/
{
  check_assertion(dest_buff != NULL && dest_buff_size > 0);
#if EDG_WIN32
  return sprintf_s(dest_buff, dest_buff_size, format_str, args...);
#else /* !EDG_WIN32 */
  int result = snprintf(dest_buff, dest_buff_size, format_str, args...);

  if (result >= 0 && size_t_arg(result) >= dest_buff_size) {
    /* If this occurs, the number of characters required exceeds the size of
       the buffer.  To match the Windows sprintf_s behavior, return -1 in place
       of the number of characters that would have been written.  */
    result = -1;
  }  /* if */
  return result;
#endif /* EDG_WIN32 */
}  /* snprintf_impl */


/*
A forward declaration of a type specialized to handle converting different
values to strings.

Each specialization should implement two functions:

  static inline size_t size_hint_of(a_Type value);

  template<typename a_Dyn_array>
  static inline void append_into(a_Dyn_array &underlying_array,
                                 a_Type      value,
                                 size_t      size_hint;
*/
template<typename a_Type>
struct String_formatter;


template<typename a_Dyn_array, typename ...a_Format_arg>
INLINE void append_using_c_formatting(a_const_char *formatting_str,
                                      a_Dyn_array  &underlying_array,
                                      size_t       size_hint,
                                      a_Format_arg ...args)
/*
Append the characters of the formatted string produced by the given
formatting string and associated arguments into the underlying array.
size_hint is an overestimate (i.e., maximum) number of characters this
value might use (modulo a temporary null character -- for use by
snprintf_impl).
*/
{
  size_t orig_size = underlying_array.length();
  size_t extra_space = size_hint + 1;

  /* Create space in the underlying array to write the arguments. */
  underlying_array.resize(orig_size + extra_space, '\0');

  /* Write the formatted string. */
  auto buff_ptr = &underlying_array[orig_size];
  int  chars_written = snprintf_impl(buff_ptr, extra_space, formatting_str,
                                     args...);
  /* If this assertion fails, there was an error writing the string. */
  check_assertion(chars_written > 0);
  /* Remove any extra characters (including the terminating null character
     added by snprintf_impl). */
  underlying_array.resize(orig_size + (size_t)chars_written, '\0');
}  /* append_using_c_formatting */


template<typename a_Dyn_array, size_t a_Size>
INLINE void append_string_literal(a_Dyn_array  &underlying_array,
                                  a_const_char (&text)[a_Size])
/*
Append the given string literal text into the underlying array.  The given text
argument should be a string literal (i.e., an array type where
strlen(text) + 1 == sizeof(text)).
*/
{
  /* If this assertion fails, presumably a partially filled buffer was supplied
     rather than a string literal. */
  check_assertion(strlen(text) == a_Size - 1);
  underlying_array.insert(underlying_array.length(), &(text[0]), a_Size - 1);
}  /* append_string_literal */


/*
A string formatter for char* (C-string) values.
*/
template<>
struct String_formatter<char*> {
  static INLINE size_t size_hint_of(char *value)
    { return strlen(value); }
  template<typename a_Dyn_array>
  static INLINE void append_into(a_Dyn_array &underlying_array,
                                 char        *chars,
                                 size_t      size_hint);
};  /* String_formatter */


template<typename a_Dyn_array>
void
String_formatter<char*>::append_into(a_Dyn_array &underlying_array,
                                     char        *chars,
                                     size_t      size_hint)
/*
Append the given characters into the underlying array.  size_hint is the number
of characters to append.
*/
{
  underlying_array.insert(underlying_array.length(), chars, size_hint);
}  /* String_formatter::append_into */


/*
A string formatter for a_const_char* (C-string) values.
*/
template<>
struct String_formatter<a_const_char*> {
  static INLINE size_t size_hint_of(a_const_char *value)
    { return strlen(value); }
  template<typename a_Dyn_array>
  static INLINE void append_into(a_Dyn_array  &underlying_array,
                                 a_const_char *chars,
                                 size_t       size_hint);
};  /* String_formatter */


template<typename a_Dyn_array>
void
String_formatter<a_const_char*>::append_into(a_Dyn_array  &underlying_array,
                                             a_const_char *chars,
                                             size_t       size_hint)
/*
Append the given characters into the underlying array.  size_hint is the number
of characters to append.
*/
{
  underlying_array.insert(underlying_array.length(), chars, size_hint);
}  /* String_formatter::append_into */


/*
A string formatter for Allocated_string values.
*/
template<template<typename> class Allocator>
struct String_formatter<Allocated_string<Allocator>> {
  static INLINE size_t size_hint_of(const Allocated_string<Allocator> &str)
    { return str.length(); }

  template<typename a_Dyn_array>
  static INLINE void
  append_into(a_Dyn_array                       &underlying_array,
              const Allocated_string<Allocator> &str,
              size_t                            size_hint);
};  /* String_formatter */


template<template<typename> class Allocator>
template<typename a_Dyn_array>
void String_formatter<Allocated_string<Allocator>>::append_into(
                          a_Dyn_array                       &underlying_array,
                          const Allocated_string<Allocator> &str,
                          ARG_UNUSED size_t                 size_hint)
/*
Convert the given Allocated_string value into its character representation, and
append the characters into the underlying array.  size_hint is unused.
*/
{
  underlying_array.insert(underlying_array.length(), str.as_temp_characters(),
                          str.length());
}  /* append_into */


/*
A string formatter for a_string_view values.
*/
template<>
struct String_formatter<a_string_view> {
  static INLINE size_t size_hint_of(a_string_view value)
    { return value.length(); }
  template<typename a_Dyn_array>
  static INLINE void append_into(a_Dyn_array   &underlying_array,
                                 a_string_view value,
                                 size_t        size_hint);
};  /* String_formatter */


template<typename a_Dyn_array>
void String_formatter<a_string_view>::append_into(
                                          a_Dyn_array        &underlying_array,
                                          a_string_view      value,
                                          ARG_UNUSED size_t  size_hint)
/*
Append the characters in the given string view value into the underlying array.
size_hint is unused.
*/
{
  underlying_array.insert(underlying_array.length(), value.start(),
                          value.length());
}  /* append_into */


/*
A string formatter for Padded_string values.
*/
template<typename a_Value_type>
struct String_formatter<Padded_string<a_Value_type>> {
  static INLINE size_t size_hint_of(const Padded_string<a_Value_type> &value)
    { return value.width; }
  template<typename a_Dyn_array>
  static INLINE void append_into(
                           a_Dyn_array                       &underlying_array,
                           const Padded_string<a_Value_type> &value,
                           size_t                            size_hint);
};  /* String_formatter */


template<typename a_Value_type>
template<typename a_Dyn_array>
void String_formatter<Padded_string<a_Value_type>>::append_into(
                           a_Dyn_array                       &underlying_array,
                           const Padded_string<a_Value_type> &value,
                           ARG_UNUSED size_t                 size_hint)
/*
Append the characters for the given padded string value into the underlying
array (see Padded_string for more information).  size_hint is unused.
*/
{
  size_t original_size = underlying_array.length();
  size_t delegate_estimate =
                     String_formatter<a_Value_type>::size_hint_of(value.value);

  String_formatter<a_Value_type>::append_into(underlying_array, value.value,
                                              delegate_estimate);

  size_t num_chars_added = underlying_array.length() - original_size;
  size_t num_chars = 0;
  /* If this assertion fails, the delegate value is larger than the width
     of the padded string.  The width should be increased to accommodate
     the larger size or the logic bug resulting in the overflow should be
     corrected. */
  check_assertion(num_chars_added <= value.width);
  if (value.width > num_chars_added) {
    num_chars = value.width - num_chars_added;
  }  /* if */
  if (value.alignment == ta_right) {
    /* The text needs to be on the right, so add characters before adding the
       text. */
    underlying_array.insert_many(original_size, num_chars, value.padding_char);
  } else if (value.alignment == ta_left) {
    /* The text needs to be on the left, so add characters after adding the
       text. */
    underlying_array.insert_many(underlying_array.length(), num_chars,
                                 value.padding_char);
  }  /* if */
}  /* append_into */


/*
A string formatter for Hex_view<double> values.
*/
template<>
struct String_formatter<Hex_view<double>> {
  static INLINE size_t size_hint_of(ARG_UNUSED Hex_view<double> value)
    { return 49; }
  template<typename a_Dyn_array>
  static INLINE void append_into(a_Dyn_array      &underlying_array,
                                 Hex_view<double> value,
                                 size_t           size_hint);
};  /* String_formatter */


template<typename a_Dyn_array>
void String_formatter<Hex_view<double>>::append_into(
                                            a_Dyn_array      &underlying_array,
                                            Hex_view<double> value,
                                            size_t           size_hint)
/*
Append the characters representing the given double value into the underlying
array in exponential hex format.  size_hint is an overestimate (i.e., maximum)
number of characters this value might use.
*/
{
  append_using_c_formatting("%a", underlying_array, size_hint, value.value);
}  /* append_into */


/*
A string formatter for Hex_view<long double> values.
*/
template<>
struct String_formatter<Hex_view<long double>> {
  static INLINE size_t size_hint_of(ARG_UNUSED Hex_view<long double> value)
    { return 49; }
  template<typename a_Dyn_array>
  static INLINE void append_into(a_Dyn_array           &underlying_array,
                                 Hex_view<long double> value,
                                 size_t                size_hint);
};  /* String_formatter */


template<typename a_Dyn_array>
void String_formatter<Hex_view<long double>>::append_into(
                                      a_Dyn_array            &underlying_array,
                                      Hex_view<long double>  value,
                                      size_t                 size_hint)
/*
Append the characters representing the given long double value into the
underlying array in exponential hex format.  size_hint is an overestimate
(i.e., maximum) number of characters this value might use.
*/
{
  append_using_c_formatting("%aL", underlying_array, size_hint, value.value);
}  /* append_into */


/*
A string formatter for an_octal_view values.
*/
template<>
struct String_formatter<an_octal_view> {
  static INLINE size_t size_hint_of(an_octal_view value)
    { return integral_digits(value.value, 8); }
  template<typename a_Dyn_array>
  static INLINE void append_into(a_Dyn_array   &underlying_array,
                                 an_octal_view value,
                                 size_t        size_hint);
};  /* String_formatter */


template<typename a_Dyn_array>
void String_formatter<an_octal_view>::append_into(
                                               a_Dyn_array   &underlying_array,
                                               an_octal_view value,
                                               size_t        size_hint)
/*
Append the characters representing in the given integral value into the
underlying array in octal format.  size_hint is an overestimate (i.e., maximum)
number of characters this value might use.
*/
{
  append_using_c_formatting("%llo", underlying_array, size_hint, value.value);
}  /* append_into */


/*
A string formatter for void* values.
*/
template<>
struct String_formatter<void*> {
  static INLINE size_t size_hint_of(ARG_UNUSED void *value)
    { return 30; }
  template<typename a_Dyn_array>
  static INLINE void append_into(a_Dyn_array &underlying_array,
                                 void        *value,
                                 size_t      size_hint);
};  /* String_formatter */


template<typename a_Dyn_array>
void String_formatter<void*>::append_into(a_Dyn_array       &underlying_array,
                                          void              *value,
                                          ARG_UNUSED size_t size_hint)
/*
Append the characters representing the given void* value into the underlying
array.  size_hint is unused.
*/
{
  append_using_c_formatting("%p", underlying_array, size_hint, value);
}  /* append_into */


/*
A string formatter (and associated delegates) for unsigned integer values to be
formatted with the given base.
*/
template<typename an_Integral_type, int a_Base>
struct Unsigned_int_formatter {
  static INLINE size_t size_hint_of(an_Integral_type value)
    { return integral_digits(value, a_Base); }
  template<typename a_Dyn_array>
  static INLINE void append_into(a_Dyn_array      &underlying_array,
                                 an_Integral_type value,
                                 size_t           size_hint);
};  /* Unsigned_int_formatter */


template<typename an_Integral_type, int a_Base>
template<typename a_Dyn_array>
void Unsigned_int_formatter<an_Integral_type, a_Base>::append_into(
                                            a_Dyn_array      &underlying_array,
                                            an_Integral_type value,
                                            size_t           size_hint)

/*
Convert the given unsigned integer value into its character representation, and
append the characters representing the value into the underlying array.
size_hint is the number of digits to represent the given value as a string.
*/
{
  size_t orig_size = underlying_array.length();

  /* Create space in the underlying array to write the arguments. */
  underlying_array.resize(orig_size + size_hint, '\0');
  for (size_t i = size_hint; i != 0; --i) {
    static_assert(a_Base <= 16, "the base must not exceed 16");
    char c = "0123456789abcdef"[value % a_Base];

    underlying_array[orig_size + (i - 1)] = c;
    value /= a_Base;
  }  /* for */
}  /* append_into */


/*
A string formatter (and associated delegates) for unsigned integer values to be
formatted with the given base.
*/
template<typename an_Integral_type, int a_Base>
struct Signed_int_formatter {
  static INLINE size_t size_hint_of(an_Integral_type value);
  template<typename a_Dyn_array>
  static INLINE void append_into(a_Dyn_array      &underlying_array,
                                 an_Integral_type value,
                                 size_t           size_hint);
};  /* Signed_int_formatter */


template<typename an_Integral_type, int a_Base>
size_t Signed_int_formatter<an_Integral_type, a_Base>::size_hint_of(
                                                        an_Integral_type value)
/*
Return the number of digits to represent the given value as a string (plus one
if the value is negative).
*/
{
  size_t result = integral_digits(value, a_Base);

  /* Add one for a negative sign. */
  if (value < 0) {
    ++result;
  }  /* if */
  return result;
}  /* size_hint_of */


template<typename an_Integral_type, int a_Base>
template<typename a_Dyn_array>
void Signed_int_formatter<an_Integral_type, a_Base>::append_into(
                                            a_Dyn_array      &underlying_array,
                                            an_Integral_type value,
                                            size_t           size_hint)
/*
Convert the given signed integer value into its character representation, and
append the characters representing the value into the underlying array.
size_hint is the number of digits to represent the given value as a string
(plus one if the value is negative).
*/
{
  int sign_negation = 1;

  if (value < 0) {
    sign_negation = -1;
    underlying_array.push_back('-');
    --size_hint;
  }  /* if */

  /* Create space in the underlying array to write the integer. */
  size_t orig_size = underlying_array.length();
  underlying_array.resize(orig_size + size_hint, '\0');
  for (size_t i = size_hint; i != 0; --i) {
    static_assert(a_Base <= 16, "the base must not exceed 16");
    char c = "0123456789abcdef"[(value % a_Base) * sign_negation];

    underlying_array[orig_size + (i - 1)] = c;
    value /= a_Base;
  }  /* for */
}  /* append_into */


/*
A string formatter for double values.
*/
template<>
struct String_formatter<double> {
  static INLINE size_t size_hint_of(double value);
  template<typename a_Dyn_array>
  static INLINE void append_into(a_Dyn_array &underlying_array,
                                 double      value,
                                 size_t      size_hint);
};  /* String_formatter */


size_t String_formatter<double>::size_hint_of(double value)
/*
Return a (possibly overestimated) number of characters to represent the given
value for the decimal and its precision.
*/
{
  /* The default precision of %f is 6, so start with 8 characters to handle a
     case like: x.xxxxxx. */
  size_t chars = 8;

  /* If the floating point value is negative, invert it and add an additional
     character to the count (to account for the '-' character). */
  if (value < 1) {
    ++chars;
    value = -value;
  }  /* if */
  /* Since the first digit left of the decimal place is already accounted for,
     check for additional powers of 10.  This considers anything greater than
     9.999 an additional power of 10 to account for discrepancies in equality
     when doing floating point arithmetic.  */
  while (value > 9.999) {
    value = value / 10;
    ++chars;
  }  /* while */
  return chars;
}  /* size_hint_of */


template<typename a_Dyn_array>
void String_formatter<double>::append_into(a_Dyn_array &underlying_array,
                                           double      value,
                                           size_t      size_hint)
/*
Append the characters representing the given double value into the underlying
array.  size_hint is unused.
*/
{
  append_using_c_formatting("%f", underlying_array, size_hint, value);
}  /* append_into */


/*
A string formatter (and associated delegates) for unsigned integer values to be
formatted as base-16/hex values.
*/
template<typename an_Integral_type>
struct Hex_unsigned_int_formatter {
  using Base_ty = Unsigned_int_formatter<an_Integral_type, 16>;

  static INLINE size_t size_hint_of(Hex_view<an_Integral_type> value)
    { return Base_ty::size_hint_of(value.value); }
  template<typename a_Dyn_array>
  static INLINE void append_into(a_Dyn_array                &underlying_array,
                                 Hex_view<an_Integral_type> value,
                                 size_t                     size_hint)
    { Base_ty::append_into(underlying_array, value.value, size_hint); }
};  /* Hex_unsigned_int_formatter */


/*
Delegate the implementation of the string formatter that formats the given type
in decimal to formatter_type.  This macro is undefined at the end of the
namespace.
*/
#define DELEGATE_DEC_FORMATTER(type, formatter_type) \
  template<> \
  struct String_formatter<type> : formatter_type<type, 10>  { \
  };  /* String_formatter */


/*
Delegate the implementation of the string formatter that formats the given type
in hexadecimal to formatter_type.  This macro is undefined at the end of the
namespace.
*/
#define DELEGATE_HEX_FORMATTER(type) \
  template<> \
  struct String_formatter<Hex_view<type>> : Hex_unsigned_int_formatter<type> {\
  };  /* String_formatter */


DELEGATE_DEC_FORMATTER(unsigned long long, Unsigned_int_formatter)
DELEGATE_DEC_FORMATTER(unsigned long,      Unsigned_int_formatter)
DELEGATE_DEC_FORMATTER(unsigned,           Unsigned_int_formatter)
DELEGATE_DEC_FORMATTER(unsigned short,     Unsigned_int_formatter)

DELEGATE_DEC_FORMATTER(long long, Signed_int_formatter)
DELEGATE_DEC_FORMATTER(long,      Signed_int_formatter)
DELEGATE_DEC_FORMATTER(int,       Signed_int_formatter)
DELEGATE_DEC_FORMATTER(short,     Signed_int_formatter)

DELEGATE_HEX_FORMATTER(unsigned long long)
DELEGATE_HEX_FORMATTER(unsigned long)
DELEGATE_HEX_FORMATTER(unsigned)
DELEGATE_HEX_FORMATTER(unsigned short)

#if HOST_HAS_INT128_EXTENSIONS
DELEGATE_DEC_FORMATTER(__uint128_t, Unsigned_int_formatter)
DELEGATE_DEC_FORMATTER(__int128_t,  Signed_int_formatter)
DELEGATE_HEX_FORMATTER(__uint128_t)
#endif /* HOST_HAS_INT128_EXTENSIONS */

template<typename... a_Text_convertible_type>
size_t estimate_byte_count_for_init(a_Text_convertible_type... args)
/*
Use a detail::String_formatter<a_Text_convertible_type> on each argument
to return an estimated number of bytes needed for an Allocated_string of the
given arguments.
*/
{
  /* Gather size estimates of each pack element. */
  size_t element_sizes [] = {
    detail::String_formatter<a_Text_convertible_type>::size_hint_of(args)...,
    /* An additional 0u value is appended to gracefully handle empty packs. */
    size_t_arg(0)
  };
  /* Start at an initial size of 1 to account for the null terminator. */
  size_t total_size = 1;

  /* Calculate the total size estimate off of the element sizes, and get a
     backing Dyn_array instance (with the appropriate space reserved based on
     the estimate). */
  /* coverity[dead_error_condition] */
  for (size_t i = 0; i != sizeof...(args); ++i) {
    /* coverity[dead_error_line] */
    total_size += element_sizes[i];
  }  /* for */
  return total_size;
}  /* estimate_byte_count_for_init */


template<typename a_Reserve_fn, typename... a_Text_convertible_type>
INLINE void append_with_custom_reserve(a_Reserve_fn               reserve_func,
                                       a_Text_convertible_type... args)
/*
The "reserve_func" should be a function that takes a character count estimate
and returns a pointer to a Dyn_array.  Said Dyn_array should be returned with
an appropriate capacity allocated for the given estimate.  The arguments (i.e.,
"args") provided are mapped to a
detail::String_formatter<a_Text_convertible_type> (abbreviated "formatter").
The arguments compose the character count estimate via the sum of the
respective formatter::size_hint_of functions.  Once the character count
estimate is computed, reserve_func is called with the given estimate, and then
each argument is sequentially appended using the respective
formatter::append_into functions.
*/
{
  /* Gather size estimates of each pack element. */
  size_t element_sizes [] = {
    detail::String_formatter<a_Text_convertible_type>::size_hint_of(args)...,
    /* An additional 0u value is appended to gracefully handle empty packs. */
    size_t_arg(0)
  };
  /* Start at an initial size of 1 to account for the null terminator. */
  size_t total_size = 1;

  /* Calculate the total size estimate off of the element sizes, and get a
     backing Dyn_array instance (with the appropriate space reserved based on
     the estimate). */
  /* coverity[dead_error_condition] */
  for (size_t i = 0; i != sizeof...(args); ++i) {
    /* coverity[dead_error_line] */
    total_size += element_sizes[i];
  }  /* for */

  auto                *backing_array = reserve_func(total_size);
  LOCAL_UNUSED size_t counter = 0;
  /* The following expression is expanded to effectively evaluate as:

       detail::String_formatter<type_1>::append_into(*backing_array,
                                                     arg_1,
                                                     element_sizes[counter++]);
       detail::String_formatter<type_2>::append_into(*backing_array,
                                                     arg_2,
                                                     element_sizes[counter++]);
       ...
       detail::String_formatter<type_n>::append_into(*backing_array,
                                                     arg_n,
                                                     element_sizes[counter++]);
   */
  PACK_EXPAND_VOID_EXPR(
                detail::String_formatter<a_Text_convertible_type>::append_into(
                                                     *backing_array,
                                                     args,
                                                     element_sizes[counter++]))
  /* Ensure the underlying array is always null-terminated. */
  backing_array->push_back('\0');
}  /* append_with_custom_reserve */

#undef DELEGATE_HEX_FORMATTER
#undef DELEGATE_DEC_FORMATTER

}  /* namespace detail */

/*
The fundamental string type, which can be instantiated with different
allocators as necessary.

Note: this type does not currently support (and thus should not be used with)
multi-byte character strings.
*/
template<template<typename> class Allocator>
struct Allocated_string {
  typedef Allocator<char> an_allocator;

  template<typename... a_Text_convertible_type>
  INLINE Allocated_string(const an_allocator         &a,
                          a_Text_convertible_type... args);
  template<typename... a_Text_convertible_type>
  INLINE Allocated_string(a_Text_convertible_type... args)
    : Allocated_string(an_allocator{}, args...)
    { }

  INLINE a_const_char *as_temp_characters() const
    { return this->backing_array.begin(); }
  template<template<typename> class Char_allocator>
  inline char* to_allocated_storage(Char_allocator<char>  allocator) const;

  INLINE void write_to_buffer(char *buffer, size_t buffer_len) const;

  INLINE auto operator[](size_t i) -> char&
    { return this->backing_array[i]; }
  INLINE auto operator[](size_t i) const -> const char&
    { return this->backing_array[i]; }

  INLINE size_t length() const
    { return this->backing_array.length() - 1; }
  INLINE a_boolean is_empty() const
    { return this->length() == 0; }

  INLINE void truncate_to(size_t new_length);

  template<typename... a_Text_convertible_type>
  INLINE void insert(size_t position, a_Text_convertible_type... args);
  template<typename... a_Text_convertible_type>
  INLINE void append(a_Text_convertible_type... args);
  template<typename... a_Text_convertible_type>
  INLINE void reset_to(a_Text_convertible_type... args);
private:
  Dyn_array<char, Allocator>
                backing_array;
                        /* The character array which is responsible for holding
                           the actual characters composing the string. */
};  /* Allocated_string */


template<template<typename> class Allocator>
template<typename... a_Text_convertible_type>
Allocated_string<Allocator>::Allocated_string(const an_allocator         &a,
                                              a_Text_convertible_type... args)
/*
Construct a new string using the given allocator.  The passed arguments are
appended in the fashion described in detail::append_with_custom_reserve.
*/
  : backing_array(detail::estimate_byte_count_for_init(args...), a)
{
  /* The backing array should have been given an appropriate estimate above;
     verify that. */
  auto reserve_func = [this](size_t total_size) {
    check_assertion(this->backing_array.capacity() >= total_size);
    return &this->backing_array;
  };
  detail::append_with_custom_reserve(reserve_func, args...);
}  /* Allocated_string */


template<template<typename> class Allocator>
template<template<typename> class Char_allocator>
char*
Allocated_string<Allocator>::to_allocated_storage(
                                               Char_allocator<char>  allocator)
                                                                          const
/*
Given an allocator, allocate and return a new character string.
*/
{
  check_assertion(this->backing_array.back_elem() == '\0');
  return this->backing_array.to_allocated_storage(allocator);
}  /* Allocated_string::to_allocated_storage */


template<template<typename> class Allocator>
void Allocated_string<Allocator>::write_to_buffer(char              *buffer,
                                                  ARG_UNUSED size_t buffer_len)
                                                                          const
/*
Write this string to the given buffer of the given length.
*/
{
  check_assertion(this->backing_array.back_elem() == '\0');
  check_assertion(size_t_arg(this->backing_array.length()) <= buffer_len);
  /* Copy the contents of the string to the given buffer. */
  memcpy(buffer, this->backing_array.begin(), this->backing_array.length());
}  /* Allocated_string::write_to_buffer */


template<template<typename> class Allocator>
void Allocated_string<Allocator>::truncate_to(size_t new_length)
/*
Truncate the string to be at most the given number of characters.
*/
{
  /* Remove the null terminator. */
  check_assertion(this->backing_array.back_elem() == '\0');
  this->backing_array.pop_back();
  /* Remove any extra characters. */
  while (size_t_arg(this->backing_array.length()) > new_length) {
    this->backing_array.pop_back();
  }  /* while */
  /* Ensure the underlying array is always null-terminated. */
  this->backing_array.push_back('\0');
}  /* Allocated_string::truncate_to */


template<template<typename> class Allocator>
template<typename... a_Text_convertible_type>
void Allocated_string<Allocator>::insert(size_t                     position,
                                         a_Text_convertible_type... args)
/*
The passed arguments are inserted at the given position.

This function works by appending the characters in the fashion described in
detail::append_with_custom_reserve.  It then uses rotate to move these
characters to the insertion point and the characters previously between
[position, end) to the end of the underlying array.
*/
{
  /* Remove the null terminator. */
  check_assertion(this->backing_array.back_elem() == '\0');
  this->backing_array.pop_back();
  /* Append the new characters. */
  auto     reserve_func = [this](size_t total_size) {
    auto min_new_size = this->backing_array.length() + total_size;

    if (min_new_size > this->backing_array.capacity()) {
      auto capacity = this->backing_array.capacity();
      auto reserve_amount = max_val(capacity * 2, min_new_size);

      this->backing_array.reserve(reserve_amount);
    }  /* if */
    return &this->backing_array;
  };
  size_t original_len = this->backing_array.length();
  detail::append_with_custom_reserve(reserve_func, args...);
  /* Rotate the characters moving the appended elements into the correct
     insertion position. */
  (void)rotate(this->backing_array.begin() + position,
               this->backing_array.begin() + original_len,
               this->backing_array.end() - 1);
}  /* Allocated_string::insert */


template<template<typename> class Allocator>
template<typename... a_Text_convertible_type>
void Allocated_string<Allocator>::append(a_Text_convertible_type... args)
/*
The passed arguments are appended in the fashion described in
detail::append_with_custom_reserve.
*/
{
  /* Remove the null terminator. */
  check_assertion(this->backing_array.back_elem() == '\0');
  this->backing_array.pop_back();
  /* Append the new characters. */
  auto reserve_func = [this](size_t total_size) {
    auto min_new_size = this->backing_array.length() + total_size;

    if (min_new_size > this->backing_array.capacity()) {
      auto capacity = this->backing_array.capacity();
      auto reserve_amount = max_val(capacity * 2, min_new_size);

      this->backing_array.reserve(reserve_amount);
    }  /* if */
    return &this->backing_array;
  };
  detail::append_with_custom_reserve(reserve_func, args...);
}  /* Allocated_string::append */


template<template<typename> class Allocator>
template<typename... a_Text_convertible_type>
void Allocated_string<Allocator>::reset_to(a_Text_convertible_type... args)
/*
The string is reset to an empty state and the passed arguments are appended in
the fashion described in detail::append_with_custom_reserve.
*/
{
  Dyn_array<char, Allocator>  *backing = &this->backing_array;

  /* Remove the null terminator. */
  check_assertion(backing->back_elem() == '\0');
  backing->clear();
  /* Append the new characters. */
  auto reserve_func = [backing](size_t total_size) {
    backing->reserve(total_size);
    return backing;
  };
  detail::append_with_custom_reserve(reserve_func, args...);
}  /* Allocated_string::reset_to */


template<template<typename> class Allocator_a,
         template<typename> class Allocator_b>
INLINE a_boolean operator==(const Allocated_string<Allocator_a> &str1,
                            const Allocated_string<Allocator_b> &str2)
/*
Return TRUE if the string represented by str1 is the same as the string
represented by str2.  If both strings are empty, the strings are considered the
same.
*/
{
  a_boolean result = TRUE;;

  if (str1.length() != str2.length()) {
    result = FALSE;
  } else {
    a_const_char *tmp1 = str1.as_temp_characters();
    a_const_char *tmp2 = str2.as_temp_characters();

    result = strncmp(tmp1, tmp2, str1.length()) == 0;
  }  /* if */
  return result;
}  /* operator== */


template<template<typename> class Allocator_a,
         template<typename> class Allocator_b>
INLINE a_boolean operator!=(const Allocated_string<Allocator_a> &str1,
                            const Allocated_string<Allocator_b> &str2)
/*
Return TRUE if the string represented by str1 is not the same as the string
represented by str2.  If both strings are empty, the strings are considered the
same.
*/
{
  return !(str1 == str2);
}  /* operator!= */


template<template<typename> class Allocator>
INLINE a_boolean operator==(const Allocated_string<Allocator> &str1,
                            a_const_char                      *str2)
/*
Return TRUE if the string represented by str1 is the same as the string
represented by str2.  If both strings are empty, the strings are considered the
same.
*/
{
  a_boolean result = TRUE;
  size_t    str2_len = strlen(str2);

  if (str1.length() != str2_len) {
    result = FALSE;
  } else {
    a_const_char *tmp1 = str1.as_temp_characters();

    result = strncmp(tmp1, str2, str1.length()) == 0;
  }  /* if */
  return result;
}  /* operator== */


template<template<typename> class Allocator>
INLINE a_boolean operator!=(const Allocated_string<Allocator> &str1,
                            a_const_char                      *str2)
/*
Return TRUE if the string represented by str1 is not the same as the string
represented by str2.  If both strings are empty, the strings are considered the
same.
*/
{
  return !(str1 == str2);
}  /* operator!= */


template<template<typename> class Allocator>
INLINE a_boolean operator==(a_const_char                      *str1,
                            const Allocated_string<Allocator> &str2)
/*
Return TRUE if the string represented by str1 is the same as the string
represented by str2.  If both strings are empty, the strings are considered the
same.
*/
{
  return (str2 == str1);
}  /* operator== */


template<template<typename> class Allocator>
INLINE a_boolean operator!=(a_const_char                      *str1,
                            const Allocated_string<Allocator> &str2)
/*
Return TRUE if the string represented by str1 is not the same as the string
represented by str2.  If both strings are empty, the "strings" are considered
the same.
*/
{
  return !(str2 == str1);
}  /* operator!= */


/*
An alias for the "normal" usage of Allocated_string (i.e., with a dynamically
allocating fe_alloc-backed allocator).
*/
typedef Allocated_string<General_allocator> a_string;

/*
A template alias used to form "small" Allocated_strings that have an initial
pre-allocated storage capacity before then falling back to a secondary
allocator (typically for dynamically allocated storage).
*/
template<unsigned a_Capacity,
         template<typename> class a_Fallback_allocator = General_allocator>
using Small_string = Allocated_string<Delegate_buffered_allocator<
                                         a_Capacity,
                                         a_Fallback_allocator>::template Meta>;

/*
A typedef used for converting various types of numbers to strings on the stack.
*/
typedef Small_string<50> a_number_buffer;


template<template<typename> class Allocator>
void print(const Allocated_string<Allocator> &string,
           FILE                              *stream,
           a_const_char                      *end = "\n")
/*
Print the given allocated string into the given stream.  The given end
character sequence terminates the printed string.
*/
{
  fputs(string.as_temp_characters(), stream);
  fputs(end, stream);
}  /* print */


/*
The Ptr_map template
====================
The Ptr_map<K, V, A> template defined below is a flat hash-based map of keys of
type K to values of type V, using A as an allocator.  It is called Ptr_map
because it works well to map non-null pointers, but the only notable key-type
requirement is that K{} (i.e., the default-constructed value of K) not be used
as a key value.  So mapping nonzero integers works very well, also, as do other
types for which the default-constructed value is never a valid key (the
default-constructed value is used to denote "empty" slots in the table).

Ptr_map uses unqualified calls to "hash_ptr" to compute hash values.  For keys
that aren't native pointers or integers, add an overloaded function that covers
that key type.  The function should return type uintptr_t.

New (key, value) pairs can be added with the map(...) member and a value
associated with a given key can be retrieved with get(k).  If the hash of a
key is already known, variants map_with_hash(...) and get_with_hash(...) are
available.  Removing a key is achieved by calling the unmap(...) member.

This is not a multi-map: Client code has to ensure that specific keys are not
matched twice.  An existing key can have its associated value replaced by
invoking the members replace(...) or replace_with_hash(...).  If it is not
known whether a key is present in the map, the members map_or_replace(...) and
map_or_replace_with_hash(...) will efficiently map the key if it is not yet
present or replace the associated value if it is present.

This implementation limits the load factor to 0.5.  That makes for efficient
lookups in most cases, but can be wasteful of storage.  It is therefore best
to keep the (key, value) size small.  In some cases, it may therefore be
useful to have the key and/or value be a handle to the associated data instead
of the data itself.

Ptr_map does not currently provide an interface to traverse all the elements
in the map.
*/

INLINE uintptr_t hash_ptr(void  *ptr)
/*
Return a hash value for the given pointer value.
*/
{
#if HOST_ALIGNMENT_REQUIRED == 1
#define HASH_PTR_SHIFT 0
#else /* HOST_ALIGNMENT_REQUIRED > 1 */
#if HOST_ALIGNMENT_REQUIRED == 2
#define HASH_PTR_SHIFT 1
#else /* HOST_ALIGNMENT_REQUIRED > 2 */
#if HOST_ALIGNMENT_REQUIRED == 4
#define HASH_PTR_SHIFT 2
#else /* HOST_ALIGNMENT_REQUIRED > 4 */
#if HOST_ALIGNMENT_REQUIRED == 8
#define HASH_PTR_SHIFT 3
#else /* HOST_ALIGNMENT_REQUIRED > 8 */
#if HOST_ALIGNMENT_REQUIRED == 16
#define HASH_PTR_SHIFT 4
#else /* HOST_ALIGNMENT_REQUIRED > 16 */
#if HOST_ALIGNMENT_REQUIRED == 32
#define HASH_PTR_SHIFT 5
#else /* HOST_ALIGNMENT_REQUIRED > 32 */
#define HASH_PTR_SHIFT 6
#endif /* == 32 */
#endif /* == 16 */
#endif /* == 8 */
#endif /* == 4 */
#endif /* == 2 */
#endif /* == 1 */
  return (uintptr_t)ptr >> HASH_PTR_SHIFT;
#undef HASH_PTR_SHIFT
}  /* hash_ptr */


template<typename an_Integral_type>
INLINE Enable_if<(uintptr_t)((an_Integral_type)1), uintptr_t>
hash_ptr(an_Integral_type i)
/*
Generic version of hash_ptr for integers.
*/
{
  return (uintptr_t)i;
}  /* hash_ptr */


template<typename T> uintptr_t hash_ptr(T *p)
/*
Version of hash_ptr for native pointers.  This assumes IL-aligned pointers.
*/
{
  return hash_ptr((void*)p);
}  /* hash_ptr */


/*lint -esym(758,Ptr_map_entry<*, *>::(anonymous))*/
template<typename a_Ptr_key, typename a_Value>
struct Ptr_map_entry {
  typedef a_Ptr_key a_key;
  typedef a_Value a_value;
  typedef Ptr_map_entry<a_Ptr_key, a_Value> an_entry;
  INLINE Ptr_map_entry()
    : stored_key()
    {}
  INLINE Ptr_map_entry(const a_key &init_key, const a_value &init_value)
    : stored_key(init_key), stored_value(init_value)
    {}
  INLINE Ptr_map_entry(a_key &&init_key, const a_value &init_value)
    : stored_key(move_from(&init_key)), stored_value(init_value)
    {}
  INLINE Ptr_map_entry(const an_entry &other) = delete;
  INLINE Ptr_map_entry(an_entry &&other);
  INLINE ~Ptr_map_entry();

  INLINE a_boolean has_value() const
    { return this->stored_key != a_key(); }

  INLINE a_key &key()
    { return this->stored_key; }
  INLINE const a_key &key() const
    { return this->stored_key; }
  INLINE a_value &value()
    { check_assertion(this->has_value()); return this->stored_value; }
  INLINE const a_value &value() const
    { check_assertion(this->has_value()); return this->stored_value; }

  auto operator=(const an_entry &other) -> an_entry& = delete;
  INLINE auto operator=(an_entry &&other) -> an_entry&;
private:
  a_key         stored_key;
                        /* The pointer value mapped by this entry.  (A "key" in
                           the hash table.) */
#ifdef UNION_AS_STRUCT
/* Workaround for union-as-struct build issue. */
#undef union
#endif /* ifdef UNION_AS_STRUCT */
  union {
    a_Value     stored_value;
                        /* A value associated with ptr. Represented as a union
                           so the value can be uninitialized, and construction
                           and destruction are manually managed. */
#ifdef UNION_AS_STRUCT
#define union struct
#endif /* ifdef UNION_AS_STRUCT */
  };
};  /* Ptr_map_entry */


template<typename a_Ptr_key, typename a_Value>
Ptr_map_entry<a_Ptr_key, a_Value>::Ptr_map_entry(an_entry &&other)
/*
Move-construct a Ptr_map_entry from another Ptr_map_entry.
*/
  : stored_key(move_from(&other.stored_key))
{
  if (this->has_value()) {
    construct(&this->stored_value, move_from(&other.stored_value));
  }  /* if */
}  /* Ptr_map_entry::Ptr_map_entry */


template<typename a_Ptr_key, typename a_Value>
Ptr_map_entry<a_Ptr_key, a_Value>::~Ptr_map_entry()
/*
Destruct a Ptr_map_entry invoking the destructor for the stored value if there
is one.
*/
{
  if (this->has_value()) {
    destroy(&this->stored_value);
  }  /* if */
}  /* Ptr_map_entry::~Ptr_map_entry */


template<typename a_Ptr_key, typename a_Value>
auto Ptr_map_entry<a_Ptr_key, a_Value>::operator=(an_entry &&other) ->
                                                                      an_entry&
/*
Move-assign into this Ptr_map_entry from another Ptr_map_entry.
*/
{
  destroy(this);
  construct(this, move_from(&other));
  return *this;
}  /* Ptr_map_entry::operator= */


/*lint -esym(1510,*Ptr_map)*/
template<typename a_Ptr_key, typename a_Value,
         template<typename> class Allocator = FE_allocator>
struct Ptr_map: private Allocator<Ptr_map_entry<a_Ptr_key, a_Value>> {
  /* A flat hash table whose keys are non-null scalar values (usually native
     pointers, but integers can be used too).  This implementation is optimized
     for lookups that generally succeed and small associated values (i.e., the
     key and value are kept together).  The allocator must allocate exactly the
     number of requested elements. */
  typedef a_Ptr_key a_key;
  typedef a_Value a_value;
  typedef Allocator<Ptr_map_entry<a_Ptr_key, a_Value>> an_allocator;
  typedef Ptr_map_entry<a_key, a_value> an_entry;
  typedef an_entry const* an_iterator;
  INLINE Ptr_map(unsigned int       mask_width,
                 const an_allocator &a = an_allocator());
  INLINE ~Ptr_map();
  INLINE auto get_with_hash(const a_key &key,
                            uintptr_t   hash) const -> a_value;
  INLINE auto get(const a_key &key) const -> a_value
    { return this->get_with_hash(key, hash_ptr(key)); }
  INLINE void map_with_hash(const a_key   &key,
                            const a_value &value,
                            uintptr_t     hash);
  INLINE void map_with_hash(a_key         &&key,
                            const a_value &value,
                            uintptr_t     hash);
  INLINE void map(const a_key &key, const a_value &value)
    { this->map_with_hash(key, value, hash_ptr(key)); }
  INLINE void map(a_key &&key, const a_value &value)
    { this->map_with_hash(key, value, hash_ptr(key)); }
  INLINE void replace_with_hash(const a_key    &key,
                                const a_value  &value,
                                uintptr_t      hash);
  INLINE void replace(const a_key &key, const a_value  &value)
    { this->replace_with_hash(key, value, hash_ptr(key)); }
  INLINE auto map_or_replace_with_hash(const a_key    &key,
                                       const a_value  &value,
                                       uintptr_t      hash)
              -> a_value;
  INLINE auto map_or_replace(const a_key   &key,
                             const a_value &value) -> a_value
    { return this->map_or_replace_with_hash(key, value, hash_ptr(key)); }
  inline void unmap(const a_key &key);
  inline void clear();
  INLINE auto number_of_elements() const -> size_t
    { return this->n_elements; }
#if DEBUG
  void db_ptrs() const;
#endif /* DEBUG */
  INLINE an_iterator begin() const
    /*lint -e{1535}*/
    { return table; }
  INLINE an_iterator end() const
    /*lint -e{1535}*/
    { return &table[hash_mask+1]; }
private:
  typedef typename an_allocator::an_allocation an_allocation;
  an_entry	*table;
			/* Pointer to the hash table. */
  size_t	hash_mask;
			/* The mask to apply to the hash value before indexing
			   in the table.  This mask is increased as the table
			   grows. */
  size_t	n_elements;
			/* The number of elements stored in the table. */
  INLINE a_boolean has_value_at(size_t idx) const
    { return this->table[idx].has_value(); }
  INLINE void make_space_for_colliding_key(size_t      idx,
                                           const a_key &new_key);
  INLINE void construct_entry_at(size_t        idx,
                                 const a_key   &new_key,
                                 const a_value &new_value);
  INLINE void construct_entry_at(size_t        idx,
                                 a_key         &&new_key,
                                 const a_value &new_value);
  INLINE void create_table(size_t n_slots);
  INLINE void expand_table();
  INLINE void check_deleted_slot(size_t  idx0);
};  /* Ptr_map */


template<typename a_Ptr_key, typename a_Value,
         template<typename> class Allocator>
Ptr_map<a_Ptr_key, a_Value, Allocator>::Ptr_map(unsigned int       mask_width,
                                                const an_allocator &a)
/*
Initialize the given pointer map with a capacity for 1<<mask_width slots.
*/
  : an_allocator(a)
{
  unsigned n_slots = (1<<mask_width);

  create_table(n_slots);
  this->n_elements = 0;
}  /* Ptr_map::Ptr_map */


template<typename a_Ptr_key, typename a_Value,
         template<typename> class Allocator>
Ptr_map<a_Ptr_key, a_Value, Allocator>::~Ptr_map()
/*
Release the storage for the map.
*/
{
  size_t  mask = this->hash_mask;
  size_t  n_slots = mask+1;

  for (size_t k = 0; k<n_slots; ++k) {
    destroy(&table[k]);
  }  /* for */
  this->dealloc(an_allocation{this->table, n_slots * sizeof(an_entry)});
  this->table = NULL;
}  /* Ptr_map::~Ptr_map */


template<typename a_Ptr_key, typename a_Value,
         template<typename> class Allocator>
auto Ptr_map<a_Ptr_key, a_Value, Allocator>::get_with_hash(
                                                        const a_key &key,
                                                        uintptr_t   hash) const
                                                    -> a_value
/*
Look up key in the map and return the associated value if found, or a_value()
if not found.  hash is the precomputed hash value for the key.
*/
{
  size_t   mask = this->hash_mask;
  size_t   idx = hash & mask;
  an_entry *tbl = this->table;
  a_value  result = a_value();

  /* If this assertion fails no value could possibly be found as the key is
     indistinguishable from an unused entry in the table. */
  check_assertion(key != a_key());
  for (;;) {
    an_entry &entry = tbl[idx];

    if (entry.key() == key) {
      result = tbl[idx].value();
      break;
    } else if (!entry.has_value()) {
      break;
    }  /* if */
    idx = (idx+1) & mask;
  }  /* for */
  return result;
}  /* Ptr_map::get_with_hash */


#ifdef TRACE_PTR_MAP
static void	*traced_key_ptr = NULL;
			/* Pointer that is checked for mapping activity.
			   Intended to be set from within a debugger and
			   watched by setting a breakpoint on function
			   ptr_map_intercept. */

inline void ptr_map_intercept(a_const_char  *msg)
/*
Function called when a map key equal to traced_key_ptr is entered into or
removed from a Ptr_map instance.
*/
{
  fprintf(f_debug, "\nMap activity for %p: %s\n", traced_key_ptr, msg);
}  /* ptr_map_intercept */

#define check_traced_key_ptr(ptr, msg)                                        \
  if ((a_byte*)(ptr) == traced_key_ptr) ptr_map_intercept(msg);

#else /* !defined(TRACE_PTR_MAP) */
#define check_traced_key_ptr(ptr, msg) /* Nothing */
#endif /* ifdef TRACE_PTR_MAP */

template<typename a_Ptr_key, typename a_Value,
         template<typename> class Allocator>
void Ptr_map<a_Ptr_key, a_Value, Allocator>::map_with_hash(
                                                        const a_key    &key,
                                                        const a_value  &value,
                                                        uintptr_t      hash)
/*
Associate a copy of value with the given key.  hash is the precomputed hash
value of that key.
*/
{
  size_t mask = this->hash_mask;
  size_t idx = hash & mask;

  /* If this assertion fails the mapped value will be lost as the key is
     indistinguishable from an unused entry in the table. */
  check_assertion(key != a_key());
  check_traced_key_ptr(key, "mapped");
  if (this->has_value_at(idx)) {
    this->make_space_for_colliding_key(idx, key);
  }  /* if */
  /* Record the new mapping. */
  this->construct_entry_at(idx, key, value);
}  /* Ptr_map::map_with_hash */


template<typename a_Ptr_key, typename a_Value,
         template<typename> class Allocator>
void Ptr_map<a_Ptr_key, a_Value, Allocator>::map_with_hash(
                                                        a_key          &&key,
                                                        const a_value  &value,
                                                        uintptr_t      hash)
/*
Associate a copy of value with the given key.  hash is the precomputed hash
value of that key.
*/
{
  size_t mask = this->hash_mask;
  size_t idx = hash & mask;

  /* If this assertion fails the mapped value will be lost as the key is
     indistinguishable from an unused entry in the table. */
  check_assertion(key != a_key());
  check_traced_key_ptr(key, "mapped");
  if (this->has_value_at(idx)) {
    this->make_space_for_colliding_key(idx, key);
  }  /* if */
  /* Record the new mapping. */
  this->construct_entry_at(idx, move_from(&key), value);
}  /* Ptr_map::map_with_hash */


template<typename a_Ptr_key, typename a_Value,
         template<typename> class Allocator>
void Ptr_map<a_Ptr_key, a_Value, Allocator>::replace_with_hash(
                                                        const a_key    &key,
                                                        const a_value  &value,
                                                        uintptr_t      hash)
/*
Replace the value associated with the given key by the given value.  hash is
the precomputed hash of that key.
*/
{
  size_t   mask = this->hash_mask;
  size_t   idx = hash & mask;
  an_entry *tbl = this->table;

  /* If this assertion fails the mapped value will be lost as the key is
     indistinguishable from an unused entry in the table. */
  check_assertion(key != a_key());
  check_traced_key_ptr(key, "replaced");
  for (;;) {
    an_entry &entry = tbl[idx];

    if (entry.key() == key) {
      entry.value() = value;
      break;
    } else {
      idx = (idx+1) & mask;
    }  /* if */
  }  /* for */
}  /* Ptr_map::replace_with_hash */


template<typename a_Ptr_key, typename a_Value,
         template<typename> class Allocator>
auto Ptr_map<a_Ptr_key, a_Value, Allocator>::map_or_replace_with_hash(
                                                        const a_key    &key,
                                                        const a_value  &value,
                                                        uintptr_t      hash)
                                                    -> a_value
/*
If the given key is already mapped, replace its associated value by the given
value and return the previously associated value.  Otherwise, record a new key,
associate it with the given value, and return a_value().  hash is the
precomputed hash of that key.
*/
{
  size_t   mask = this->hash_mask;
  size_t   idx = hash & mask;
  an_entry *tbl = this->table;
  a_value  old_value = a_value();

  /* If this assertion fails the mapped value will be lost as the key is
     indistinguishable from an unused entry in the table. */
  check_assertion(key != a_key());
  check_traced_key_ptr(key, "mapped or replaced");
  if (!this->has_value_at(idx)) {
    this->construct_entry_at(idx, key, value);
  } else {
    for (;;) {
      an_entry &entry = tbl[idx];

      if (entry.key() == key) {
        old_value = move_from(&entry.value());
        entry.value() = value;
        break;
      } else {
        idx = (idx+1) & mask;
        if (!this->has_value_at(idx)) {
          this->construct_entry_at(idx, key, value);
          break;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  return old_value;
}  /* Ptr_map::map_or_replace_with_hash */


template<typename a_Ptr_key, typename a_Value,
         template<typename> class Allocator>
void Ptr_map<a_Ptr_key, a_Value, Allocator>::unmap(const a_key &key)
/*
Remove the given key from the table (it must exist).
*/
{
  uintptr_t hash = hash_ptr(key);
  size_t    mask = this->hash_mask;
  size_t    idx = hash & mask;
  an_entry  *tbl = this->table;

  check_traced_key_ptr(key, "UNmapped");
  /* Find the item to delete (we're assuming it exists). */
  while (tbl[idx].key() != key) {
    idx = (idx+1) & mask;
  }  /* while */
  /* Delete the entry. */
  destroy(&tbl[idx]);
  construct(&tbl[idx]);
  /* If the next slot is empty, we're done.  Otherwise, we may have to move
     another element into the emptied slot. */
  if (tbl[(idx+1) & mask].key() != a_key()) {
    this->check_deleted_slot(idx);
  }  /* if */
  this->n_elements -= 1;
}  /* Ptr_map::unmap */


template<typename a_Ptr_key, typename a_Value,
         template<typename> class Allocator>
void Ptr_map<a_Ptr_key, a_Value, Allocator>::clear()
/*
Remove all entries in the Ptr_map.
*/
{
  size_t  mask = this->hash_mask;
  size_t  n_slots = mask+1;

  for (size_t k = 0; k<n_slots; ++k) {
    if (this->has_value_at(k)) {
      /* If the entry was used, replace it. */
      destroy(&table[k]);
      construct(&table[k]);
    }  /* if */
  }  /* for */
}  /* Ptr_map::clear */


template<typename a_Ptr_key, typename a_Value,
         template<typename> class Allocator>
void Ptr_map<a_Ptr_key, a_Value, Allocator>::make_space_for_colliding_key(
                                                     size_t         idx,
                                          ARG_UNUSED const a_key    &new_key)
/*
The given key has a hash value that collides with an existing mapping.
Rearrange the current elements so that the given index can be used for a new
value.
*/
{
#if EXPENSIVE_CHECKING
  { a_value  old_val = this->get(new_key);
    if (old_val != a_value()) {
      unexpected_condition_str("duplicate map key in Ptr_map");
    }  /* if */
  }
#endif /* EXPENSIVE_CHECKING */

  /* Move the existing mapping to the next available spot. */
  size_t initial_hit = idx;
  for (;;) {
    idx = (idx+1) & this->hash_mask;
    if (!this->has_value_at(idx)) {
      swap_at(&this->table[idx], &this->table[initial_hit]);
      break;
    }  /* if */
  }  /* for */
}  /* Ptr_map::make_space_for_colliding_key */


template<typename a_Ptr_key, typename a_Value,
         template<typename> class Allocator>
void Ptr_map<a_Ptr_key, a_Value, Allocator>::construct_entry_at(
                                                      size_t        idx,
                                                      const a_key   &new_key,
                                                      const a_value &new_value)
/*
Construct a new entry with the given key and value at the given index.
*/
{
  an_entry new_entry(new_key, new_value);

  this->table[idx] = move_from(&new_entry);
  this->n_elements += 1;
  if (this->n_elements * 2 > this->hash_mask) {
    this->expand_table();
  }  /* if */
}  /* Ptr_map::construct_entry_at */


template<typename a_Ptr_key, typename a_Value,
         template<typename> class Allocator>
void Ptr_map<a_Ptr_key, a_Value, Allocator>::construct_entry_at(
                                                      size_t        idx,
                                                      a_key         &&new_key,
                                                      const a_value &new_value)
/*
Construct a new entry with the given key and value at the given index.
*/
{
  an_entry new_entry(move_from(&new_key), new_value);

  this->table[idx] = move_from(&new_entry);
  this->n_elements += 1;
  if (this->n_elements * 2 > this->hash_mask) {
    this->expand_table();
  }  /* if */
}  /* Ptr_map::construct_entry_at */


template<typename a_Ptr_key, typename a_Value,
         template<typename> class Allocator>
void Ptr_map<a_Ptr_key, a_Value, Allocator>::create_table(size_t n_slots)
/*
Create a new table with at least the given number of slots (if the underlying
allocator hands more memory, the slot count will be updated appropriately).
This function replaces the state of table and hash_mask.  The caller is
responsible for deallocating the previous table.
*/
{
  an_allocation  allocation = this->alloc(n_slots);

  this->table = allocation.start;
  this->hash_mask = n_slots - 1;
  for (size_t i = 0; i < n_slots; ++i) {
    construct(&this->table[i]);
  }  /* for */
}  /* Ptr_map::create_table */


template<typename a_Ptr_key, typename a_Value,
         template<typename> class Allocator>
void Ptr_map<a_Ptr_key, a_Value, Allocator>::expand_table()
/*
Double the size of the hash table (and rehash entries as needed).
*/
{
  size_t   old_n_slots = this->hash_mask + 1;
  an_entry *old_table = this->table;

  this->create_table(2 * (this->hash_mask + 1));

  size_t new_mask = this->hash_mask;
  an_entry *new_table = this->table;
  for (size_t k = 0; k < old_n_slots; ++k) {
    an_entry &entry = old_table[k];

    if (entry.has_value()) {
      size_t idx = hash_ptr(entry.key()) & new_mask;

      while (new_table[idx].has_value()) {
        idx = (idx+1) & new_mask;
      }  /* while */
      new_table[idx] = move_from(&entry);
    }  /* if */
  }  /* for */
  this->table = new_table;
  this->hash_mask = new_mask;
  this->dealloc(an_allocation{old_table, old_n_slots * sizeof(an_entry)});
}  /* Ptr_map::expand_table */


template<typename a_Ptr_key, typename a_Value,
         template<typename> class Allocator>
void Ptr_map<a_Ptr_key, a_Value, Allocator>::check_deleted_slot(size_t  idx0)
/*
Slot idx0 has been cleared (i.e., this->table[idx0].ptr has been set to null).
The next slot is not empty.  There may therefore exist entries that are
associated with that slot (i.e., have the same hash index).  This function
makes sure that such entries can be found, by moving up entries as needed.

This corresponds to Algorithm R in section 6.4 of volume 3 of Donald E. Knuth's
"The Art of Computer Programming" (Sorting and Searching -- Second Edition),
with the assumption that step R1 has already been performed (idx0 is "j") and
we know that the subsequent slot is not empty.
*/
{
  an_entry *tbl = this->table;
  size_t   mask = this->hash_mask;
  size_t   idx = (idx0+1) & mask;
  size_t   ridx;

  for (;;) {
    for (;;) {
      ridx = hash_ptr(tbl[idx].key()) & mask;
      /* See if we can move the entry at idx to idx0.  ridx is its "ideal"
         slot: the place from where probing will start.  So we cannot move it
         ahead of there.  I.e., if idx0 lies outside [ridx, idx-1] (considering
         "wrap-around"), do not move the entry and try the next entry
         instead. */
      if ((ridx <= idx0 && idx0 < idx) ||
          (idx0 >= ridx && idx < ridx) ||
          (idx0 < idx && idx < ridx)) {
        /* idx0 is in [ridx, idx-1]: Move the entry. */
        break;
      } else {
        idx = (idx+1) & mask;
        if (!tbl[idx].has_value()) goto done;
      }  /* if */
    }  /* for */
    swap_at(&tbl[idx0], &tbl[idx]);
    idx0 = idx;
    idx = (idx0+1) & mask;
    if (!tbl[idx].has_value()) goto done;
  }  /* for */
done:;
}  /* Ptr_map::check_deleted_slot */

#if DEBUG

template<typename a_Ptr_key, typename a_Value,
         template<typename> class Allocator>
void Ptr_map<a_Ptr_key, a_Value, Allocator>::db_ptrs() const
/*
Output some information about the map's key contents to f_debug.
*/
{
  an_entry *tbl = this->table;
  size_t   mask = this->hash_mask;
  size_t   n_slots = mask+1;

  for (size_t k = 0; k<n_slots; ++k) {
    fprintf(f_debug, "[%2zu] ", k);
    if (!this->has_value_at(k)) {
      fprintf(f_debug, "(empty)\n");
    } else {
      a_key  ptr = tbl[k].key();

      fprintf(f_debug, "h = %2u  %p\n",
              (unsigned)(hash_ptr(ptr) & mask), (void*)ptr);
    }  /* if */
  }  /* for */
}  /* Ptr_map::db_ptrs */

#endif /* DEBUG */

/*
A template type used to map a given Ptr_map-compatible key type to an array of
associated value types (each with the given default initial capacity).
*/
template<typename a_Ptr_key, typename a_Value, unsigned a_Capacity,
         template<typename> class Allocator = FE_allocator>
struct Ptr_multi_map {
  typedef a_Ptr_key a_key;
  typedef a_Value a_value;
  typedef Small_dyn_array<a_Value, a_Capacity> a_multi_value;
  typedef Allocator<a_multi_value> a_value_allocator;
  typedef Allocator<Ptr_map_entry<a_Ptr_key, a_multi_value*>> a_map_allocator;
  typedef typename Ptr_map<a_Ptr_key, a_multi_value*, Allocator>::an_iterator
                                                                   an_iterator;

  INLINE Ptr_multi_map(unsigned int            mask_width,
                       const a_map_allocator   &ma = a_map_allocator(),
                       const a_value_allocator &va = a_value_allocator());
  INLINE ~Ptr_multi_map();

  INLINE auto get(a_Ptr_key key) -> a_multi_value*;
  INLINE auto get_or_alloc(a_Ptr_key key) -> a_multi_value*;
  INLINE auto take(a_Ptr_key key) -> a_multi_value;
  INLINE void remove(a_Ptr_key key);
  INLINE void clear();

  INLINE auto number_of_elements() -> size_t
    { return this->backing_map.number_of_elements(); }

  INLINE an_iterator begin() const
    { return this->backing_map.begin(); }
  INLINE an_iterator end() const
    { return this->backing_map.end(); }
private:
  INLINE void dealloc(a_multi_value *values);
  Ptr_map<a_Ptr_key, a_multi_value*, Allocator>
                backing_map;
                        /* A table that maps pointer keys to multi-value
                           containers. */
  a_value_allocator
                multi_value_allocator;
                        /* The allocator used to allocate multi-value
                           containers. */
};  /* Ptr_multi_map */


template<typename a_Ptr_key, typename a_Value, unsigned a_Capacity,
         template<typename> class Allocator>
Ptr_multi_map<a_Ptr_key, a_Value, a_Capacity, Allocator>::Ptr_multi_map(
                                            unsigned int            mask_width,
                                            const a_map_allocator   &ma,
                                            const a_value_allocator &va)
/*
Construct a new Ptr_multi_map using the given mask_width, map allocator (to
allocate the underlying Ptr_map) and value allocator (to allocate the
associated multi-value containers).
*/
  : backing_map(mask_width, ma), multi_value_allocator(va)
{
}  /* Ptr_multi_map::Ptr_multi_map */


template<typename a_Ptr_key, typename a_Value, unsigned a_Capacity,
         template<typename> class Allocator>
Ptr_multi_map<a_Ptr_key, a_Value, a_Capacity, Allocator>::~Ptr_multi_map()
/*
Destruct the Ptr_multi_map tearing down any allocated multi-values.
*/
{
  this->clear();
}  /* Ptr_multi_map::~Ptr_multi_map */


template<typename a_Ptr_key, typename a_Value, unsigned a_Capacity,
         template<typename> class Allocator>
auto
Ptr_multi_map<a_Ptr_key, a_Value, a_Capacity, Allocator>::get(a_Ptr_key key)
                                                          -> a_multi_value*
/*
Return a dynamic array of values if it exists for the given key; otherwise,
return NULL.
*/
{
  return this->backing_map.get(key);
}  /* Ptr_multi_map::get */


template<typename a_Ptr_key, typename a_Value, unsigned a_Capacity,
         template<typename> class Allocator>
auto
Ptr_multi_map<a_Ptr_key, a_Value, a_Capacity, Allocator>::get_or_alloc(
                                                                 a_Ptr_key key)
                                                          -> a_multi_value*
/*
Return a dynamic array of values if it exists for the given key (creating the
dynamic array if it does not already exist).
*/
{
  a_multi_value *values = this->backing_map.get(key);

  if (values == NULL) {
    typename a_value_allocator::an_allocation
                    values_alloc = multi_value_allocator.alloc(1);

    values = values_alloc.start;
    construct(values);
    this->backing_map.map(key, values);
  }  /* if */
  return values;
}  /* Ptr_multi_map::get_or_alloc */


template<typename a_Ptr_key, typename a_Value, unsigned a_Capacity,
         template<typename> class Allocator>
auto
Ptr_multi_map<a_Ptr_key, a_Value, a_Capacity, Allocator>::take(a_Ptr_key key)
                                                          -> a_multi_value
/*
Return a copy of the dynamic array of values if it exists for the given key.
The value must exist in the map and will be unregistered after being copied.
*/
{
  check_assertion(this->get(key) != NULL);
  a_multi_value result = *this->backing_map.get(key);

  this->remove(key);
  return result;
}  /* Ptr_multi_map::take */


template<typename a_Ptr_key, typename a_Value, unsigned a_Capacity,
         template<typename> class Allocator>
void Ptr_multi_map<a_Ptr_key, a_Value, a_Capacity, Allocator>::remove(
                                                                 a_Ptr_key key)
/*
Given the key, remove the associated list from the map, and deconstruct and
deallocate the associated multi-value (if any).
*/
{
  a_multi_value *values = this->backing_map.get(key);

  if (values != NULL) {
    this->dealloc(values);
    this->backing_map.unmap(key);
  }  /* if */
}  /* Ptr_multi_map::remove */


template<typename a_Ptr_key, typename a_Value, unsigned a_Capacity,
         template<typename> class Allocator>
void Ptr_multi_map<a_Ptr_key, a_Value, a_Capacity, Allocator>::clear()
/*
Clear the map destroying all mapped entries.
*/
{
  using an_entry = Ptr_map_entry<a_Ptr_key, a_multi_value*>;
  /* Deallocate the underlying allocated multi-value objects. */
  for (const an_entry &entry : this->backing_map) {
    if (entry.has_value()) {
      this->dealloc(entry.value());
    }  /* if */
  }  /* for */
  this->backing_map.clear();
}  /* Ptr_multi_map::clear */


template<typename a_Ptr_key, typename a_Value, unsigned a_Capacity,
         template<typename> class Allocator>
void Ptr_multi_map<a_Ptr_key, a_Value, a_Capacity, Allocator>::dealloc(
                                                         a_multi_value *values)
/*
Given the multi-value, deconstruct and deallocate the multi-value list.
*/
{
  destroy(values);

  typename a_value_allocator::an_allocation
                values_alloc{values, sizeof(a_multi_value)};
  this->multi_value_allocator.dealloc(values_alloc);
}  /* Ptr_multi_map::dealloc */

#if !STANDALONE_UTILITY_PROGRAM

/*
A structure that wraps a path stored in a C-string to enable operations such
as equality and hashing to operate with path semantics as opposed to raw
pointer (or string) semantics.
*/
struct a_path_handle {
  a_const_char
		*ptr = NULL;
			/* Pointer to the path. */
  a_path_handle() = default;
  a_path_handle(a_const_char *path) : ptr(path) {}
};


INLINE a_boolean operator==(const a_path_handle path1,
                            const a_path_handle path2)
/*
Return TRUE if the path contained by path1 is the same as the path contained by
path2.  If both handles contain NULL pointers then the "paths" are considered
the same.
*/
{
  a_boolean result;
  if (path1.ptr == NULL || path2.ptr == NULL) {
    result = (path1.ptr == path2.ptr);
  } else {
    result = (compare_file_names_general(path1.ptr, path2.ptr) == 0);
  }  /* if */
  return result;
}  /* operator== */


INLINE a_boolean operator!=(const a_path_handle path1,
                            const a_path_handle path2)
/*
Return TRUE if the path contained by path1 is not the same as the path
contained by path2.  If both handles contain NULL pointers then the "paths" are
considered the same.
*/
{
  return !(path1 == path2);
}  /* operator!= */

#endif /* !STANDALONE_UTILITY_PROGRAM */

INLINE uintptr_t hash_ptr(a_string_view str)
/*
Compute a hash for the given string view.  The hash must be appropriate for
Ptr_map.
*/
{
  a_hash_value  value = 0;

  for (size_t i = 0; i < str.length(); ++i) {
    value = (value << 5) + value + (a_hash_value)(str.start()[i]);
  }  /* for */
  return value;
}  /* hash_ptr */


template<template<typename> class Allocator>
INLINE uintptr_t hash_ptr(const Allocated_string<Allocator> &str)
/*
Compute a hash for the given string.  The hash must be appropriate for Ptr_map.
*/
{
  a_string_view str_view(str.as_temp_characters(), str.length());

  return hash_ptr(str_view);
}  /* hash_ptr */

#if !STANDALONE_UTILITY_PROGRAM

INLINE uintptr_t hash_ptr(const a_path_handle path)
/*
Compute a hash for the given path.  The hash must be appropriate for Ptr_map.
*/
{
  a_const_char  *norm_path = normalize_file_name(path.ptr);
  a_string_view str_view(norm_path);

  return hash_ptr(str_view);
}  /* hash_ptr */

#endif /* !STANDALONE_UTILITY_PROGRAM */

/*
Ptr_set is a simplified wrapper around Ptr_map for representing the specific
case of a "set" (i.e., only a present or absent state, there's no true "mapped"
value).
*/
template<typename a_Ptr, template<typename> class Allocator = FE_allocator>
struct Ptr_set {
  typedef Ptr_map<a_Ptr, a_boolean, Allocator> a_map_type;
  typedef typename a_map_type::an_allocator an_allocator;
  INLINE Ptr_set(unsigned int       mask_width,
                 const an_allocator &a = an_allocator())
    : underlying_map(mask_width, a)
    {}
  INLINE a_boolean contains(a_Ptr key) const
    { return this->underlying_map.get(key); }
  INLINE void add(a_Ptr key)
    { this->underlying_map.map(key, TRUE); }
  INLINE void remove(a_Ptr key)
    { this->underlying_map.unmap(key); }
private:
  a_map_type    underlying_map;
                        /* The Ptr_map backing the set.  Elements considered
                           in the set are stored in the underlying map
                           with a paired value of TRUE. */
};  /* Ptr_set */

namespace detail {

/*
This is an implementation type used to facilitate easier implementation of
partial specializations for Seq_comparator.  See Seq_comparator and its
specializations for more information.
*/
template<typename an_Elem_a, typename an_Elem_b, typename an_Eq_fn,
         template<typename> class Allocator>
struct Seq_comparator_impl: private Allocator<uint32_t> {
  using an_allocator = Allocator<uint32_t>;
  using an_allocation = Allocation<uint32_t>;
  using an_equality_fn = an_Eq_fn;

  inline Seq_comparator_impl(an_Elem_a          *array_a_val,
                             size_t             array_a_len_val,
                             an_Elem_b          *array_b_val,
                             size_t             array_b_len_val,
                             an_equality_fn     eq_fn_val,
                             const an_allocator &allocator = an_allocator());
  inline ~Seq_comparator_impl();

  template<typename a_Consumer_fn>
  void diff(a_Consumer_fn fn);
private:
  an_Elem_a& input_a(size_t idx)
    { return this->array_a[this->array_a_len - (idx + 1)]; }
  an_Elem_b& input_b(size_t idx)
    { return this->array_b[this->array_b_len - (idx + 1)]; }
  unsigned& output(size_t a, size_t b)
    { return this->lcs_table[(a * this->array_b_len) + b]; }
  an_Elem_a     *array_a;
                        /* The given "top row" sequence being compared. */
  size_t        array_a_len;
                        /* The length of the given "top row" sequence being
                           compared. */
  an_Elem_b     *array_b;
                        /* The given "left column" sequence being compared. */
  size_t        array_b_len;
                        /* The length of the given "left column" sequence being
                           compared. */
  an_equality_fn
                eq_fn;  /* The function used to check for equality. */
  uint32_t      *lcs_table;
                        /* A pointer to the allocated alignment table. */
};  /* Seq_comparator_impl */


template<typename an_Elem_a, typename an_Elem_b, typename an_Eq_fn,
         template<typename> class Allocator>
Seq_comparator_impl<an_Elem_a, an_Elem_b, an_Eq_fn, Allocator>::
                                                           Seq_comparator_impl(
                                            an_Elem_a          *array_a_val,
                                            size_t             array_a_len_val,
                                            an_Elem_b          *array_b_val,
                                            size_t             array_b_len_val,
                                            an_equality_fn     eq_fn_val,
                                            const an_allocator &allocator)
/*
Given two arrays and their sizes, allocate a new alignment table with the given
allocator.  The allocated alignment table will then be populated using the
given equality function to check equality of array elements.
*/
  : an_allocator(allocator),
    array_a(array_a_val), array_a_len(array_a_len_val),
    array_b(array_b_val), array_b_len(array_b_len_val), eq_fn(eq_fn_val),
    lcs_table(this->alloc(array_a_len_val * array_b_len_val).start)
{
  /* Zero the "top" and "left" edge of the comparison. */
  for (size_t a = 0; a < this->array_a_len; ++a) {
    this->output(a, 0) = 0;
  }  /* for */
  for (size_t b = 0; b < this->array_b_len; ++b) {
    this->output(0, b) = 0;
  }  /* for */
  /* Compute the alignment table. */
  for (size_t a = 1; a < this->array_a_len; ++a) {
    for (size_t b = 1; b < this->array_b_len; ++b) {
      if ((*this->eq_fn)(this->input_a(a), this->input_b(b))) {
        /* The current values match, take the score to the top and left that
           lead to this point and increase it with another match. */
        unsigned prev_diag_val = this->output(a - 1, b - 1);

        this->output(a, b) = prev_diag_val + 1;
      } else {
        /* The current values do not match, take either the score from the top
           or the left (which ever scored better). */
        unsigned prev_b_val = this->output(a, b - 1);
        unsigned prev_a_val = this->output(a - 1, b);

        this->output(a, b) = max_val(prev_b_val, prev_a_val);
      }  /* if */
    }  /* for */
  }  /* for */
}  /* Seq_comparator_impl::Seq_comparator_impl */


template<typename an_Elem_a, typename an_Elem_b, typename an_Eq_fn,
         template<typename> class Allocator>
Seq_comparator_impl<an_Elem_a, an_Elem_b, an_Eq_fn, Allocator>::
                                                         ~Seq_comparator_impl()
/*
Destroy the Seq_comparator instance and its allocated resources.
*/
{
  size_t num_elements = this->array_a_len * this->array_b_len;

  this->dealloc(an_allocation{this->lcs_table,
                              num_elements * sizeof(uint32_t)});
}  /* Seq_comparator_impl::~Seq_comparator_impl */


template<typename an_Elem_a, typename an_Elem_b, typename an_Eq_fn,
         template<typename> class Allocator>
template<typename a_Consumer_fn>
void Seq_comparator_impl<an_Elem_a, an_Elem_b, an_Eq_fn, Allocator>::diff(
                                                              a_Consumer_fn fn)
/*
Given a function that accepts two arguments (the first an_Elem_a* type and the
second an_Elem_b* type), walk back through the computed comparison.

The provided function will be called as follows:
 - If the values match, both arguments will be supplied.
 - If there was a deletion, only the left argument will be supplied (the other
   shall be NULL).
 - If there was an insertion, only the right argument will be supplied (the
   other shall be NULL).
*/
{
  size_t a = this->array_a_len;
  size_t b = this->array_b_len;

  while (TRUE) {
    if (a >= 1 && b >= 1 &&
        (*this->eq_fn)(this->input_a(a - 1), this->input_b(b - 1))) {
      /* The values at a and b match: reverse the scoring and walk back up and
         to the left to see what comparison led here. */
      fn(&this->input_a(--a), &this->input_b(--b));
    } else if (b > 1 && (a == 1 ||
                         (this->output(a - 1, b - 2) >=
                          this->output(a - 2, b - 1)))) {
      /* The left value is scoring better: this is an insertion. */
      fn(NULL, &this->input_b(--b));
    } else if (a > 1 && (b == 1 ||
                         (this->output(a - 1, b - 2) <
                          this->output(a - 2, b - 1)))) {
      /* The right value is scoring better: this is a deletion. */
      fn(&this->input_a(--a), NULL);
    } else {
      /* The root of the comparison has been reached: stop. */
      break;
    }  /* if */
  }  /* while */
}  /* Seq_comparator_impl::diff */

}  /* namespace detail */

/*
This class is used to compare two sequences using a dynamic programming
approach to the longest common subsequence problem.  The computed alignment
table can then be examined to yield the diff via traceback.

The following equality function signatures are supported by this class and its
specializations:

  a_boolean (*)(const an_Elem_a&, const an_Elem_b&);
  a_boolean (*)(const an_Elem_a*, const an_Elem_b*);

A limited set of function type patterns specified via partial specialization
(in place of a fully generic equality function argument) is a trade off made to
reduce burden on code using this type.  Fully generalizing the equality
function would require either a significantly more complicated type declaration
on the usage side in the anticipated most common cases.

Notably, this class reverses its view of the given inputs so the diff can be
returned in order without requiring further allocations or reversing the input
data beforehand.

Additionally, note the implementation is aimed at debugging operations.  As
such it's not fully optimized and does not currently do any pruning of the
input data; this may be added in the future.
*/
template<typename an_Elem_a, typename an_Elem_b = an_Elem_a,
         template<typename> class Allocator = FE_allocator>
struct Seq_comparator: public detail::Seq_comparator_impl<
                              an_Elem_a, an_Elem_b,
                              a_boolean (*)(const an_Elem_a&,const an_Elem_b&),
                              Allocator> {
  using a_base_type = detail::Seq_comparator_impl<
                              an_Elem_a, an_Elem_b,
                              a_boolean (*)(const an_Elem_a&,const an_Elem_b&),
                              Allocator>;
  using a_base_type::a_base_type;
};  /* Seq_comparator */


/*
This is a partial specialization of Seq_comparator that tweaks the signature of
the comparison function's function pointer to be more friendly to comparing
arrays of pointers.  As the front end makes heavy usage of linked list
structures with individual elements that have large footprints (instead of
arrays) this specialization makes the Seq_comparator much more friendly at the
call site.
*/
template<typename an_Elem_a, typename an_Elem_b,
         template<typename> class Allocator>
struct Seq_comparator<an_Elem_a*, an_Elem_b*, Allocator>:
                                            public detail::Seq_comparator_impl<
                              an_Elem_a*, an_Elem_b*,
                              a_boolean (*)(const an_Elem_a*,const an_Elem_b*),
                              Allocator> {
  using a_base_type = detail::Seq_comparator_impl<
                              an_Elem_a*, an_Elem_b*,
                              a_boolean (*)(const an_Elem_a*,const an_Elem_b*),
                              Allocator>;
  using a_base_type::a_base_type;
};  /* Seq_comparator */


template<typename a_Forward_iterator>
INLINE sizeof_t distance(a_Forward_iterator begin,
                         a_Forward_iterator end)
/*
Return the number of elements in the range [begin, end).
*/
 {
  sizeof_t result = 0;

  for (; begin != end; ++begin) {
    ++result;
  }  /* for */
  return result;
}  /* distance */


template<typename a_Linked_list_type, typename a_Predicate>
INLINE unsigned count_list_elements(a_Linked_list_type list_head,
                                    a_Predicate        predicate)
/*
Given the head of a linked list and a predicate, traverse the list and return
the number of elements in the list where the predicate returns TRUE.
*/
{
  unsigned count = 0;

  for (a_Linked_list_type el = list_head; el != NULL; el = el->next) {
    if (predicate(el)) {
      ++count;
    }  /* if */
  }  /* if */
  return count;
}  /* count_list_elements */


template<typename a_Linked_list_type>
INLINE unsigned count_list_elements(a_Linked_list_type list_head)
/*
Given the head of a linked list, traverse the list and return the number of
elements in the list.
*/
{
  auto always_true = [](ARG_UNUSED const a_Linked_list_type &el) -> a_boolean {
    return TRUE;
  };

  return count_list_elements(list_head, always_true);
}  /* count_list_elements */


template<typename an_Elem_type, typename a_Dest_type = an_Elem_type,
         template<typename> class Allocator = FE_allocator>
inline Dyn_array<a_Dest_type*, Allocator>
linked_list_to_ptr_array(an_Elem_type *list_head,
                         size_t       initial_capacity = 0)
/*
Given the head of a linked list, traverse the list and construct an equivalent
dynamic array of pointers with the given initial capacity.

This function should be preferred when a_Dest_type is relatively expensive to
copy, otherwise, use linked_list_to_array.
*/
{
  Dyn_array<a_Dest_type*, Allocator> result(initial_capacity);

  for (; list_head != NULL; list_head = list_head->next) {
    result.push_back(list_head);
  }  /* for */
  return result;
}  /* linked_list_to_ptr_array */


template<typename an_Elem_type, typename a_Dest_type = an_Elem_type,
         template<typename> class Allocator = FE_allocator>
inline Dyn_array<a_Dest_type, Allocator>
linked_list_to_array(an_Elem_type *list_head,
                     size_t       initial_capacity = 0)
/*
Given the head of a linked list, traverse the list and construct an equivalent
dynamic array with the given initial capacity.

This function should be preferred when a_Dest_type is relatively cheap to copy,
otherwise, use linked_list_to_ptr_array.
*/
{
  Dyn_array<a_Dest_type, Allocator> result(initial_capacity);

  for (; list_head != NULL; list_head = list_head->next) {
    result.push_back(*list_head);
  }  /* for */
  return result;
}  /* linked_list_to_array */


template<typename an_Integral_type>
INLINE a_boolean checked_addition(an_Integral_type *output,
                                  uint64_t         a,
                                  uint64_t         b)
/*
Perform a checked addition, *output = a + b.  Return TRUE if *output has
been set and overflow did not occur; otherwise, return FALSE.
*/
{
  static_assert(!(an_Integral_type(-1) < an_Integral_type(0)),
                "integer type must be unsigned");
  a_boolean      result;
  constexpr auto max_value = max_integral_value<an_Integral_type>();
  uint64_t       diff = max_value - a;

  /* This exploits that "a + b > c" if and only if "b > c - a".  Thus, by using
     c = UINTX_MAX, overflow of a + b can be detected. */
  if (b > diff) {
    result = FALSE;
  } else {
    result = TRUE;
    *output = (an_Integral_type)(a + b);
  }  /* if */
  return result;
}  /* checked_addition */


template<typename an_Integral_type>
INLINE a_boolean checked_multiplication(an_Integral_type *output,
                                        uint64_t         a,
                                        uint64_t         b)
/*
Perform a checked multiplication, *output = a * b.  Return TRUE if *output
has been set and overflow did not occur; otherwise, return FALSE.
*/
{
  static_assert(!(an_Integral_type(-1) < an_Integral_type(0)),
                "integer type must be unsigned");
  a_boolean      result;
  constexpr auto max_value = max_integral_value<an_Integral_type>();

  /* This exploits that "a * b > c" if and only if "a > c / b".  Thus, by using
     c = UINTX_MAX, overflow of a * b can be detected.  b != 0 is additionally
     checked to ensure division by zero does not occur. */
  if (b != 0 && a > max_value / b) {
    result = FALSE;
  } else {
    result = TRUE;
    *output = (an_Integral_type)(a * b);
  }  /* if */
  return result;
}  /* checked_multiplication */


template<typename an_Integral_type>
inline void append_as_big_endian(a_byte           *dest_array,
                                 an_Integral_type value)
/*
Append the given value to the array position represented by dest_array using
a big endian byte order.
*/
{
  constexpr size_t num_bytes = sizeof(an_Integral_type);

  if (host_little_endian) {
    /* Swap the byte order into big endian. */
    for (size_t i = 0; i < num_bytes; ++i) {
      dest_array[i] = (value >> (((num_bytes - 1) - i) * 8)) & 0xff;
    }  /* for */
  } else {
    /* Copy the bytes. */
    memcpy(dest_array, &value, num_bytes);
  }  /* if */
}  /* append_as_big_endian */


template<typename an_Integral_type>
INLINE an_Integral_type bit_rotate_left(an_Integral_type value,
                                        unsigned         num_bits)
/*
Perform the given number of left bit rotations on the given value returning the
result.
*/
{
  constexpr unsigned num_bits_total = sizeof(an_Integral_type) * 8;

  return (value << num_bits) | (value >> (num_bits_total - num_bits));
}  /* bit_rotate_left */


template<typename an_Integral_type>
INLINE an_Integral_type bit_rotate_right(an_Integral_type value,
                                         unsigned         num_bits)
/*
Perform the given number of right bit rotations on the given value returning
the result.
*/
{
  constexpr unsigned num_bits_total = sizeof(an_Integral_type) * 8;

  return (value >> num_bits) | (value << (num_bits_total - num_bits));
}  /* bit_rotate_right */


namespace detail {
namespace sha256 {

constexpr size_t
                num_words_in_digest = 8;
                        /* The number of 32-bit words in a SHA-2 256 bit
                           digest. */
constexpr size_t
                num_bytes_in_digest = num_words_in_digest * 4;
                        /* The number of 8-bit bytes in a SHA-2 256 bit
                           digest. */
constexpr size_t
                chunk_size = 64;
                        /* The number of bytes used by the SHA-2 256 bit
                           algorithm to store a pending chunk (composing 512
                           bits total). */
constexpr uint32_t
                constants[64] = {
                              0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
                              0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
                              0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
                              0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
                              0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
                              0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
                              0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
                              0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
                              0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
                              0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
                              0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
                              0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
                              0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
                              0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
                              0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
                              0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };
                        /* The round constants used by the SHA-2 algorithm's
                           compression loop. */

}  /* namespace sha256 */
}  /* namespace detail */

/*
This structure is used to represent a SHA-2 256 bit digest.
*/
struct a_sha256_digest {
  a_sha256_digest(a_byte *init_bytes)
    { memcpy(this->bytes, init_bytes, detail::sha256::num_bytes_in_digest); }

  a_byte operator[](size_t idx) const
    { return this->bytes[idx]; }

  operator a_byte const*() const
    { return this->bytes; }
private:
  a_byte       bytes[detail::sha256::num_bytes_in_digest];
                        /* The bytes composing the digest. */
};  /* a_sha256_digest */

/*
This structure is used to compute a SHA-2 256 bit digest for the given bytes.

This implementation makes no guarantees about cryptographic security and has
not been formally validated by the Cryptographic Module Validation Program.
*/
struct a_sha256_hash {
  a_sha256_hash() = default;
  inline void update(a_byte const *bytes, size_t num_bytes);
  inline a_sha256_digest compute_digest();
private:
  inline void transform_chunk();
  a_byte        chunk_bytes[detail::sha256::chunk_size] = {};
                        /* The currently pending chunk bytes.  These bytes
                           will be fed through SHA-2's compression loop in
                           a_sha256_hash::transform_chunk. */
  uint32_t      chunk_len = 0;
                        /* The number of bytes in chunk_bytes currently. */
  uint64_t      num_bits_of_data = 0;
                        /* The total number of bits in the source data (i.e.,
                           the total number of bits that have been given to
                           a_sha256_hash::update).  This implementation does
                           not currently allow sub-byte bit lengths to be added
                           to the hash. */
  uint32_t      state[detail::sha256::num_words_in_digest] =
                            { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                              0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
                        /* The current hash state.  This is initialized to the
                           SHA-2 starting state and then updated by the
                           compression loop in a_sha256_hash::transform_chunk.
                           The state is then finalized by
                           a_sha256_hash::compute_digest.  At that point, the
                           values stored here are equivalent to the current
                           SHA-2 256 bit digest value on big endian systems (on
                           little endian systems the byte order of the "words",
                           i.e., 32-bit integers, must first be swapped). */
};  /* a_sha256_hash */


void a_sha256_hash::update(a_byte const *bytes, size_t num_bytes)
/*
Append the given bytes (counted by num_bytes) to the hash state.
*/
{
  for (size_t i = 0; i < num_bytes; ++i) {
    this->chunk_bytes[this->chunk_len++] = bytes[i];
    if (this->chunk_len == detail::sha256::chunk_size) {
      this->transform_chunk();
    }  /* if */
  }  /* for */
  this->num_bits_of_data += num_bytes * 8;
}  /* a_sha256_hash::update */


void a_sha256_hash::transform_chunk()
/*
Process the current complete chunk.
*/
{
  /* The chunk should be complete before any call to transform_chunk. */
  check_assertion(this->chunk_len == detail::sha256::chunk_size);
  /* Initialize the message schedule with the chunk values. */
  uint32_t msg_sch[detail::sha256::chunk_size] = {};

  /* Change the chunk size into the number of 32-bit words that need to be
     copied. */
  for (size_t i = 0; i < detail::sha256::chunk_size / 4; ++i) {
    /* Convert every 4 bytes into a 32-bit word value in the message
       schedule.  For the first byte, copy it directly.  For subsequent bytes,
       move the existing bits over and then add the next byte. */
    msg_sch[i] = this->chunk_bytes[i * 4] & 0xff;
    for (size_t k = 1; k < 4; ++k) {
      msg_sch[i] = (msg_sch[i] << 8) | (this->chunk_bytes[(i * 4) + k] & 0xff);
    }  /* for */
  }  /* for */
  /* Extend the first 16 words (those that were just written) in the message
     schedule into the remaining 48 words. */
  for (size_t i = 16; i < 64; ++i) {
    uint32_t &src_byte_a = msg_sch[i - 15];
    uint32_t sig_bits_a = bit_rotate_right(src_byte_a, 7) ^
                          bit_rotate_right(src_byte_a, 18) ^
                          (src_byte_a >> 3);
    uint32_t &src_byte_b = msg_sch[i - 2];
    uint32_t sig_bits_b = bit_rotate_right(src_byte_b, 17) ^
                          bit_rotate_right(src_byte_b, 19) ^
                          (src_byte_b >> 10);

    msg_sch[i] = msg_sch[i - 16] + sig_bits_a + msg_sch[i - 7] + sig_bits_b;
  }  /* for */

  uint32_t tmp_state[detail::sha256::num_words_in_digest];
  /* Initialize the local hash state. */
  for (size_t i = 0; i < detail::sha256::num_words_in_digest; ++i) {
    tmp_state[i] = this->state[i];
  }  /* for */
  /* Perform the compression loop. */
  for (size_t i = 0; i < 64; ++i) {
    uint32_t s1 = bit_rotate_right(tmp_state[4], 6) ^
                  bit_rotate_right(tmp_state[4], 11) ^
                  bit_rotate_right(tmp_state[4], 25);
    uint32_t ch = (tmp_state[4] & tmp_state[5]) ^
                  (~tmp_state[4] & tmp_state[6]);
    uint32_t tmp_1 = tmp_state[7] + s1 + ch + detail::sha256::constants[i] +
                     msg_sch[i];
    uint32_t s0 = bit_rotate_right(tmp_state[0], 2) ^
                  bit_rotate_right(tmp_state[0], 13) ^
                  bit_rotate_right(tmp_state[0], 22);
    uint32_t maj = (tmp_state[0] & tmp_state[1]) ^
                   (tmp_state[0] & tmp_state[2]) ^
                   (tmp_state[1] & tmp_state[2]);
    uint32_t tmp_2 = s0 + maj;

    tmp_state[7] = tmp_state[6];
    tmp_state[6] = tmp_state[5];
    tmp_state[5] = tmp_state[4];
    tmp_state[4] = tmp_state[3] + tmp_1;
    tmp_state[3] = tmp_state[2];
    tmp_state[2] = tmp_state[1];
    tmp_state[1] = tmp_state[0];
    tmp_state[0] = tmp_1 + tmp_2;
  }  /* for */
  /* Merge the local hash state with the overall hash state. */
  for (size_t i = 0; i < detail::sha256::num_words_in_digest; ++i) {
    this->state[i] += tmp_state[i];
  }  /* for */
  /* Reset the chunk length. */
  this->chunk_len = 0;
}  /* a_sha256_hash::transform_chunk */


a_sha256_digest a_sha256_hash::compute_digest()
/*
Finalize the hash state and return the SHA-2 256 bit digest value.
*/
{
  /* The length of the original message will be stored in a 64-bit integer. */
  constexpr unsigned num_bytes_for_len = sizeof(uint64_t);

  /* Append a single "1" bit by adding a byte with the binary representation
     "10000000" (note the chunk is guaranteed to have remaining bytes at this
     point, as otherwise the a_sha256_hash::update function would've triggered
     a chunk transform). */
  this->chunk_bytes[this->chunk_len++] = 0x80;
  /* If there are not enough bytes left to store the length in the chunk, fill
     the chunk and transform. */
  if (detail::sha256::chunk_size - this->chunk_len < num_bytes_for_len) {
    while (this->chunk_len < detail::sha256::chunk_size) {
      this->chunk_bytes[this->chunk_len++] = 0x0;
    }  /* while */
    this->transform_chunk();
  }  /* if */
  /* Fill the chunk with padding zeros while leaving space for the 64-bit
     integer value representing the data length. */
  while (this->chunk_len < detail::sha256::chunk_size - num_bytes_for_len) {
    this->chunk_bytes[this->chunk_len++] = 0x0;
  }  /* while */
  /* Copy the length. */
  append_as_big_endian(this->chunk_bytes + this->chunk_len,
                       this->num_bits_of_data);
  this->chunk_len += num_bytes_for_len;
  /* Perform the final message transform. */
  this->transform_chunk();

  /* Form the digest (which is always big endian). */
  a_byte digest[detail::sha256::num_bytes_in_digest];
  for (size_t i = 0; i < detail::sha256::num_words_in_digest; ++i) {
    append_as_big_endian(digest + (i * 4), this->state[i]);
  }  /* for */
  return a_sha256_digest(digest);
}  /* a_sha256_hash::compute_digest */


/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef EDG_UTIL_H */

