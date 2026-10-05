/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

mem_manage.h -- Declarations relating to mem_manage.c (having to do with
                memory management).

*/

/* Avoid including these declarations more than once. */
#ifndef MEM_MANAGE_H
#define MEM_MANAGE_H 1

#ifndef HOST_ENVIR_H
#include "host_envir.h"
#endif /* ifndef HOST_ENVIR_H */

/* Basic memory management data structures are defined in mem_tables.h. */
#ifndef MEM_TABLES_H
#include "mem_tables.h"
#endif /* ifndef MEM_TABLES_H */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
Determine (if possible) the number of low-order zero bits required in
aligned addresses on the host.  This allows use of masking instead of
a remainder operation.
*/
#if HOST_ALIGNMENT_REQUIRED == 1
#define ALIGNMENT_BITS 0
#else /* HOST_ALIGNMENT_REQUIRED != 1 */
#if HOST_ALIGNMENT_REQUIRED == 2
#define ALIGNMENT_BITS 0x1
#else /* HOST_ALIGNMENT_REQUIRED != 2 */
#if HOST_ALIGNMENT_REQUIRED == 4
#define ALIGNMENT_BITS 0x3
#else /* HOST_ALIGNMENT_REQUIRED != 4 */
#if HOST_ALIGNMENT_REQUIRED == 8
#define ALIGNMENT_BITS 0x7
#else /* HOST_ALIGNMENT_REQUIRED != 8 */
#if HOST_ALIGNMENT_REQUIRED == 16
#define ALIGNMENT_BITS 0xf
#else /* HOST_ALIGNMENT_REQUIRED != 16 */
#if HOST_ALIGNMENT_REQUIRED == 32
#define ALIGNMENT_BITS 0x1f
#else /* HOST_ALIGNMENT_REQUIRED != 32 */
/* Alignment will have to be established with "%". */
#define ALIGNMENT_BITS (-1)
#endif /* == 32 */
#endif /* == 16 */
#endif /* == 8 */
#endif /* == 4 */
#endif /* == 2 */
#endif /* == 1 */

#if ALIGNMENT_BITS == 0
/* No alignment required. */
#define align_expr(value) 0
#else /* ALIGNMENT_BITS != 0 */
#if ALIGNMENT_BITS == -1
/* Alignment requirement is not a recognized small power of two.  Use "%". */
#define align_expr(value) ((value) % HOST_ALIGNMENT_REQUIRED)
#else /* ALIGNMENT_BITS >= 0 */
/* Alignment requirement is a recognized small power of two.  Use masking. */
#define align_expr(value) ((value) & ALIGNMENT_BITS)
#endif /* ALIGNMENT_BITS == -1 */
#endif /* ALIGNMENT_BITS == 0 */


template<typename a_Size_type>
inline void do_host_alignment(ARG_UNUSED a_Size_type *size)
/*
Round up the given size -- if required -- to make it evenly divisible by
HOST_ALIGNMENT_REQUIRED.
*/
{
#if ALIGNMENT_BITS != 0
  a_Size_type excess_bytes = align_expr(*size);

  if (excess_bytes != 0) {
    *size += HOST_ALIGNMENT_REQUIRED - excess_bytes;
  }  /* if */
#endif /* ALIGNMENT_BITS != 0 */
}  /* do_host_alignment */


/* Allocate space in "general" storage. */
extern char *alloc_general(sizeof_t size);
/* Free space in "general" storage. */
extern void free_general(a_void_ptr ptr,
                         sizeof_t   size);
/* Allocate memory that can be resized later. */
extern char *alloc_resizable_buffer(sizeof_t size);
/* Resize allocated space in "general" storage. */
extern char *realloc_buffer(char     *old_ptr,
                            sizeof_t old_size,
                            sizeof_t new_size);

constexpr sizeof_t
		HUGE_FE_MEM_THRESHOLD = (sizeof(a_mem_block_header) + 2048);
			/* The number of bytes before a front end allocation is
			   considered a huge allocation. */

extern a_mem_block_header_ptr alloc_mem_block(
                                       a_memory_region_number region_number,
                                       sizeof_t               min_size,
                                       char                   *desired_addr,
                                       a_boolean              small_extension);

extern void trim_mem_block(a_mem_block_header_ptr hdr);

#if DEBUG
extern void track_allocation(a_memory_region_number region_number,
                             sizeof_t               size,
                             sizeof_t               orig_size);
#endif /* DEBUG */

#if !STANDALONE_UTILITY_PROGRAM
#ifdef TRACE_ALLOC
extern void trace_alloc_check(void *ptr);
#endif /* TRACE_ALLOC */
#endif /* !STANDALONE_UTILITY_PROGRAM */


INLINE char *alloc_in_region(a_memory_region_number region_number,
                             sizeof_t               size)
/*
Allocate "size" bytes in memory region "region_number", and return a
pointer to them.  Generate a catastrophic error and do not return if
the storage cannot be allocated.  Memory region 0 (NULL_region_number)
is used for allocation of general front end memory (i.e., not IL).
*/
{
  char                   *temp_ptr;
  a_mem_block_header_ptr hdr;

  /* Ensure at least one byte is allocated to ensure that zero-sized objects
     have distinct memory addresses. */
  size = max_val(size, (sizeof_t)1);

#if DEBUG
  sizeof_t orig_size = size;
#endif /* DEBUG */
  /* Round up the size if necessary to preserve alignment.  Note that
     aside from keeping the data correctly aligned, this also keeps the
     next available address properly aligned, which is important in
     trim_mem_block. */
  do_host_alignment(&size);

  /* See if enough space remains in the current block.  If not, get
     a new block.  Note that we add the required host alignment to the
     requested allocation size.  This is done to ensure that no piece
     of memory ends precisely at the end of low-level allocation.  On
     some systems this can cause memory faults by system routines that
     seem to make the assumption that this won't occur. */
  hdr = mem_region_table[region_number];

  sizeof_t  true_size = size + HOST_ALIGNMENT_REQUIRED;
  a_boolean use_dedicated_mem_block = true_size >= HUGE_FE_MEM_THRESHOLD;
  a_boolean small_extension = FALSE;
  a_boolean region_trimmed = hdr->trimmed;
  if (use_dedicated_mem_block) {
    /* If above the HUGE_FE_MEM_THRESHOLD, a dedicated memory region header is
       created.  This uses the small extension logic so that the minimum memory
       region size (if a new allocation is required) is not
       HOST_ALLOCATION_INCREMENT. */
    trim_mem_block(hdr);
    small_extension = TRUE;
    hdr = alloc_mem_block(region_number, true_size, (char*)NULL,
                          /*small_extension=*/TRUE);
  } else {
    /* Suppress the CodeCenter warning caused because after_end_of_block
       may be pointing to memory that is not allocated, or is part of a
       different allocation. */
    /*SUPPRESS 22*/
    sizeof_t remaining_space = (sizeof_t)(hdr->after_end_of_block -
                                          hdr->next_avail_in_block);
    if (true_size > remaining_space) {
      /* Not enough space remaining in current block.  Free any unused
         space at the end of the current last block, and start a new block.
         If the memory region has already been trimmed, allocate only a
         small extension.  This comes up when per-instantiation needed flag
         entries are added to a function after it has been trimmed. */
      small_extension = region_trimmed;
      if (!hdr->trimmed) {
        trim_mem_block(hdr);
      }  /* if */
      hdr = alloc_mem_block(region_number, true_size,
                            (char *)NULL, small_extension);
    }  /* if */
  }  /* if */
  /* Take the required space out of the current block. */
  temp_ptr = hdr->next_avail_in_block;
  hdr->next_avail_in_block += size;
  if (small_extension) {
    sizeof_t remaining_space = (sizeof_t)(hdr->after_end_of_block -
                                          hdr->next_avail_in_block);

    if (remaining_space >= HUGE_FE_MEM_THRESHOLD) {
      /* This case should show up rarely outside of configurations where
         USE_MMAP_FOR_MEMORY_REGIONS is FALSE.  However, in configurations
         where USE_MMAP_FOR_MEMORY_REGIONS is TRUE, this pruning is essential
         so as to not waste large amounts of memory (because any allocation
         performed while precompiled_header_processing is TRUE has a minimum
         size of HOST_ALLOCATION_INCREMENT to prevent spuriously terminating
         PCH processing). */
      trim_mem_block(hdr);
      /* Future blocks in this memory region will be allocated as small
         extensions if this header is marked as trimmed.  If the original
         header was not trimmed, reset this header's trimmed state so memory
         region allocation doesn't allocate small blocks going forward. */
      hdr->trimmed = (a_byte_boolean)region_trimmed;
    }  /* if */
  }  /* if */
  if (use_dedicated_mem_block) {
    /* If forming a dedicated memory region header, consume the rest of the
       block now that the memory has been taken from it.  If this is not done
       (despite the trim above) some bytes may remain "available" in the memory
       region header (which would then be used during the next call to this
       function for the current memory region number).

       Preventing these unexpected secondary uses of the memory region header's
       associated memory block allows the code in free_fe_huge to work as
       intended (reclaiming the block in its entirety). */
    hdr->next_avail_in_block = hdr->after_end_of_block;
  }  /* if */
#if DEBUG
  track_allocation(region_number, size, orig_size);
#endif /* DEBUG */
#if !STANDALONE_UTILITY_PROGRAM
#ifdef TRACE_ALLOC
  trace_alloc_check(temp_ptr);
#endif /* TRACE_ALLOC */
#endif /* !STANDALONE_UTILITY_PROGRAM */
  return temp_ptr;
}  /* alloc_in_region */


INLINE a_void_ptr alloc_general_or_in_region(a_memory_region_number region,
                                             sizeof_t               size)
/*
Allocate either general memory or memory from a memory region.  If
"region" is NO_MEMORY_REGION_NUMBER, general memory is used.  Otherwise,
memory is allocated in the memory region specified by "region".
*/
{
  a_void_ptr ptr;

  if (region == NO_MEMORY_REGION_NUMBER)  {
    ptr = alloc_general(size);
  } else {
    ptr = alloc_in_region(region, size);
  }  /* if */
  return ptr;
}  /* alloc_general_or_in_region */


extern void *malloc_for_interpreter(sizeof_t size);

extern void free_for_interpreter(void     *block,
                                 sizeof_t size);

/* Make sure that mem_region_table is large enough. */
extern
void ensure_mem_region_table_space(a_memory_region_number region_number);

extern void ensure_function_def_table_space(
                                    a_function_def_number function_def_number);

namespace detail {

typedef Ptr_multi_map<sizeof_t, a_void_ptr, 50, General_allocator>
		a_size_to_ptr_map;
			/* A map to an array of pointers to blocks of memory
			   of a given size. */

EXTERN_THREAD a_size_to_ptr_map
		*freed_fe_map;
			/* Pointer to a map to freed front end memory entries
			   of a given size. */

/*
The type used for tracking a pointer's size for reuse during allocation.
*/
struct a_reusable_allocation {
  a_reusable_allocation(a_void_ptr ptr_val, sizeof_t size_val)
    : ptr(ptr_val), size(size_val)
    {}
  a_void_ptr    ptr;    /* The address of the allocation. */
  sizeof_t      size;   /* The number of bytes in the allocation. */
};  /* a_reusable_allocation */

template<>
struct Is_trivially_copyable_edg_impl<a_reusable_allocation> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_copyable_edg_impl */

template<>
struct Is_trivially_destructible_edg_impl<a_reusable_allocation> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_destructible_edg_impl */

typedef Dyn_array<a_reusable_allocation, General_allocator>
		a_reusable_allocation_list;
			/* A list of allocations that can be reused. */

EXTERN_THREAD a_reusable_allocation_list
		*reusable_fe_list;
			/* Pointer to a map to freed front end memory entries
			   of a given size. */

extern void free_fe_huge(a_void_ptr ptr,
                         sizeof_t   size);

extern void free_fe_normal(a_void_ptr ptr,
                           sizeof_t   size);

}  /* namespace detail */


inline char *alloc_fe(sizeof_t     size)
/*
Allocate a block of front end memory of the specified size and return
a pointer.  Look for a previously freed block.  If none is found allocate
a new block.

If the allocated memory is freed prior to the termination of the front end, the
free_fe function should be used to free it.
*/
{
  check_assertion_str(in_front_end,
                      "memory region allocation must not occur after front "
                      "end processing has ended");
  void *ptr = NULL;

  /* Ensure at least one byte is allocated to ensure that zero-sized objects
     have distinct memory addresses. */
  size = max_val(size, (sizeof_t)1);
  if (size < HUGE_FE_MEM_THRESHOLD && detail::freed_fe_map != NULL) {
    /* Look for a previously freed block. */
    auto freed_blocks = detail::freed_fe_map->get(size);
    if (freed_blocks != NULL && freed_blocks->length() > 0) {
      /* Return the entry at the end of the array and remove it. */
      ptr = freed_blocks->back_elem();
      freed_blocks->pop_back();
    }  /* if */
  }  /* if */
  if (ptr == NULL) {
    /* Allocate a new block. */
    ptr = alloc_in_region(FRONT_END_REGION_NUMBER, size);
  }   /* if */
  return (char*)ptr;
}  /* alloc_fe */


inline char *alloc_fe_var_size(sizeof_t size,
                               sizeof_t *actual_size)
/*
Allocate a block of front end memory of the specified size or larger and return
a pointer.  *actual_size is set to the true size of the allocated memory so the
extra memory can be used if useful.  Look for a previously freed block.  If
none is found allocate a new block.

If the allocated memory is freed prior to the termination of the front end, the
free_fe_var_size function should be used to free it.
*/
{
  check_assertion_str(in_front_end,
                      "memory region allocation must not occur after front "
                      "end processing has ended");
  void *ptr = NULL;

  /* Ensure at least one byte is allocated to ensure that zero-sized objects
     have distinct memory addresses. */
  size = max_val(size, (sizeof_t)1);
  if (size < HUGE_FE_MEM_THRESHOLD && detail::reusable_fe_list != NULL) {
    /* Look for a previously freed block. */
    for (size_t i = detail::reusable_fe_list->length(); i != 0; --i) {
      auto &alloc = (*detail::reusable_fe_list)[i - 1];
      if (alloc.size >= size) {
        ptr = alloc.ptr;
        *actual_size = alloc.size;
        detail::reusable_fe_list->remove(i - 1);
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  if (ptr == NULL) {
    /* Allocate a new block. */
    ptr = alloc_in_region(FRONT_END_REGION_NUMBER, size);
    *actual_size = size;
  }   /* if */
  return (char*)ptr;
}  /* alloc_fe_var_size */


template<typename T>
INLINE T* alloc_fe(void)
/*
Function to allocate front end memory for an entry of the given type.  This is
similar to the older macro "alloc_fe_of_type" (see below), but it handles type
names containing commas (e.g., "alloc_fe<Ptr_map<int, bool>>()").
*/
{
  return (T*)alloc_fe(sizeof(T));
}  /* alloc_fe */


INLINE void free_fe(a_void_ptr   ptr,
                    sizeof_t     size)
/*
Free the block of memory (allocated by alloc_fe) pointed to by ptr of the
specified size.  The block is recorded for possible reuse later.
*/
{
  if (ptr == NULL) {
    /* If this assertion fails a null block of memory was given with a non-zero
       size.  Thus, either the caller got the pointer to the block of memory or
       the size wrong. */
    check_assertion(size == 0);
  } else if (size >= HUGE_FE_MEM_THRESHOLD) {
    detail::free_fe_huge(ptr, size);
  } else {
    detail::free_fe_normal(ptr, size);
  }  /* if */
}  /* free_fe */


INLINE void free_fe_var_size(a_void_ptr   ptr,
                             sizeof_t     size)
/*
Free the block of memory (allocated by alloc_fe_var_size) pointed to by ptr of
the specified size.  The block is recorded for possible reuse later.
*/
{
  if (ptr == NULL) {
    /* If this assertion fails a null block of memory was given with a non-zero
       size.  Thus, either the caller got the pointer to the block of memory or
       the size wrong. */
    check_assertion(size == 0);
  } else if (size >= HUGE_FE_MEM_THRESHOLD) {
    detail::free_fe_huge(ptr, size);
  } else {
    detail::reusable_fe_list->emplace_back(ptr, size);
  }  /* if */
}  /* free_fe_var_size */


template<typename a_Type>
INLINE void free_fe(a_Type *ptr)
/*
Free an entry in front end storage of type a_Type.
*/
{
  free_fe((void*)ptr, sizeof(a_Type));
}  /* free_fe */

/*
Macro that allocates an entry for the specified type in front end memory.
*/
/*lint -e665*/
#define alloc_fe_of_type(type) (type*)alloc_fe(sizeof(type))

/*
Macro that allocates an entry for the specified type in general memory.
*/
#define alloc_general_of_type(type) (type*)alloc_general(sizeof(type))

/*
Macro that allocates an entry for the specified type in general memory or
in a memory region.
*/
#define alloc_general_or_in_region_of_type(region, type)		\
  (type*)alloc_general_or_in_region(region, sizeof(type))

/* Allocate a block of memory to be used for memory region storage. */
extern a_void_ptr alloc_new_mem_block(sizeof_t size);
/* Create a new memory region. */
extern a_memory_region_number new_memory_region(void);
/* Initialize a memory region without allocating the initial block. */
extern void init_memory_region_without_initial_allocation
                                      (a_memory_region_number region_number);
/* Initialize a memory region. */
extern void init_memory_region(a_memory_region_number region_number,
                               sizeof_t               min_size);
/* Check whether a memory region is still needed in the front end. */
extern void check_for_done_with_memory_region(
                                       a_memory_region_number region_number);
#if !STANDALONE_UTILITY_PROGRAM
extern void check_for_done_with_all_function_memory_regions(void);
#endif /* !STANDALONE_UTILITY_PROGRAM */
/* Free the space in a memory region. */
extern void free_memory_region(a_memory_region_number region_number);
/* Free all of the memory regions. */
extern void free_all_memory_regions(void);
/* Free the unused space in the final block of a memory region. */
extern void trim_memory_region(a_memory_region_number region_number);
#if DEBUG
/* Display the amount of memory used, for debug purposes. */
extern void show_mem_manage_space_used(unsigned long total_accounted_for);
#endif /* DEBUG */
/* Early initialization of memory management routines. */
extern void mem_manage_early_init(void);
/* One-time initialization of memory management routines. */
extern void mem_manage_one_time_init(void);
/* Initialize memory management. */
extern void mem_manage_trans_unit_init(void);
extern void mem_manage_reset(void);
extern void mem_manage_init(void);
#if DEBUG
/* Functions for tracking important high water marks. */
extern void mem_manage_one_time_init_done();
extern void mem_manage_trans_unit_init_done();
#endif /* DEBUG */

#if MAKE_FRONT_END_CALLABLE
/* Free memory used by the compilation. */
extern void mem_manage_wrapup(void);
#endif /* MAKE_FRONT_END_CALLABLE */

#if !STANDALONE_UTILITY_PROGRAM

a_function_def_number new_function_def_number(void);

extern void record_mapped_mem_block(a_void_ptr	addr,
				    sizeof_t	size);

#if !USE_MMAP_FOR_MEMORY_REGIONS
extern void preallocate_pch_memory(void);
extern void free_unused_pch_memory(void);
#else /* USE_MMAP_FOR_MEMORY_REGIONS */
#define free_unused_pch_memory() /* Nothing */
extern void free_mapped_mem_blocks(void);
#endif /* !USE_MMAP_FOR_MEMORY_REGIONS */

/*
Structure used to record the memory allocations that have been done.
This is used in PCH processing so that the process that reads in
a PCH file can duplicate the sequence of memory allocations done by
the creator of the PCH.
*/
typedef struct a_mem_alloc_history *a_mem_alloc_history_ptr;
typedef struct a_mem_alloc_history {
  a_void_ptr	addr;
			/* Address at which the memory was allocated. */
  sizeof_t	size;
			/* Number of bytes allocated. */
} a_mem_alloc_history;

typedef long	a_mem_alloc_history_number;
			/* Type of an index into the
			    mem_alloc_history array. */

EXTERN_THREAD a_mem_alloc_history_number
		num_of_mem_alloc_history_entries;
			/* Number of elements used in the memory allocation
			   history array. */

EXTERN_THREAD a_mem_alloc_history_number
		size_of_mem_alloc_history;
			/* Number of array elements in the memory allocation
			   history array. */

EXTERN_THREAD a_mem_alloc_history_number
		mem_alloc_history_entries_used;
			/* The number of entries in the mem_alloc_history
			   array for which the associated memory is
			   actually in use by the compilation. */

#if USE_MMAP_FOR_MEMORY_REGIONS
EXTERN_THREAD a_mem_alloc_history_ptr
		mem_alloc_history;
			/* Pointer to an array of memory allocation history
			   entries. */

#else /* !USE_MMAP_FOR_MEMORY_REGIONS */
#define SIZE_OF_MEM_ALLOC_HISTORY 500
			/* Number of entries in the fixed size memory
			   allocation history array. */

EXTERN_THREAD a_mem_alloc_history
		mem_alloc_history[SIZE_OF_MEM_ALLOC_HISTORY];
			/* Array of memory allocation history entries used
			   to store the preallocated memory blocks used
			   for PCH processing. */

EXTERN_THREAD a_boolean
		exhausted_preallocated_memory;
			/* TRUE if all of the preallocated PCH memory has
			   been used, making creation of a PCH impossible. */

EXTERN_THREAD a_boolean
		large_mem_block_needed;
			/* TRUE if a PCH file cannot be created because
			   a memory block that is larger than those
			   preallocated is needed. */

EXTERN_THREAD a_source_position
		large_mem_block_error_pos;
			/* Error position when a large entity was
			   allocated that prevented generation of a
			   precompiled header file. */

EXTERN_THREAD a_mem_alloc_history_number
		total_mem_blocks_allocated;
			/* Total number of memory blocks allocated.  This
			   may be larger than the number of memory history
			   entries when the preallocated memory has been
			   exhausted. */
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */

/*
TRUE if a new PCH may be created containing the information currently
being constructed by the compilation.  This has an effect on how
memory management is done.  The memory for regions that may need to be
written out as part of the PCH cannot be freed until after the PCH is
written.
*/
#define may_be_building_new_pch() (header_stop_position_pending)

/*
Macro that is TRUE if two memory allocation history entries are equivalent.
*/
#define equivalent_mem_alloc_history(m1, m2)				\
  ((m1).addr == (m2).addr && (m1).size == (m2).size)

#endif /* !STANDALONE_UTILITY_PROGRAM */

#if DEBUG
EXTERN_THREAD unsigned long
		*allocated_in_region;
			/* Parallel array to mem_region_table.  Keeps track
			   of the allocation in each region. */
EXTERN_THREAD a_memory_region_number
		size_of_allocated_in_region;
			/* Size of allocated_in_region (in entries, not 
			   bytes). */
#endif /* DEBUG */

/*
A general purpose text buffer that is automatically resized as characters
are added.  The string may or may not be null-terminated, but if it is
null-terminated, the null-terminator will be included in "size".
*/
typedef struct a_text_buffer {
  sizeof_t	allocated_size;
			/* The size in bytes of the memory allocated for the
			   buffer. */
  sizeof_t	size;
			/* The number of characters currently in the buffer. */
  sizeof_t	allocation_increment;
			/* Initially, this is the size of the initial memory
			   allocation for the buffer.  Each time the buffer
			   is reallocated, this size is doubled. */
  char		*buffer;
			/* Pointer to the buffer containing the characters. */
} a_text_buffer;

extern a_text_buffer_ptr alloc_text_buffer(sizeof_t	allocation_increment);

extern void reset_text_buffer(a_text_buffer_ptr	buffer);

extern void expand_text_buffer(a_text_buffer_ptr	buffer,
			       sizeof_t			length);

extern void add_to_text_buffer(a_text_buffer_ptr	buffer,
			       a_const_char		*string,
			       sizeof_t			length);

extern void f_add_string_to_text_buffer(a_text_buffer_ptr	buffer,
				        a_const_char		*string);

extern void truncate_text_buffer_to(a_text_buffer_ptr buffer,
                                    sizeof_t          length);

extern
void remove_null_terminator_from_text_buffer(a_text_buffer_ptr	buffer);

/*
Add the specified string to a text buffer.
*/
#define add_string_to_text_buffer(buffer, string)			\
  (add_to_text_buffer(buffer, string, (sizeof_t)(strlen(string))))
#define add_string_with_length_to_text_buffer(buffer, string, len)	\
  (add_to_text_buffer(buffer, string, (sizeof_t)(len)))

/*
This structure is used to represent a portion of a text buffer (it's
effectively a string_view for a_text_buffer).  Thus while the structure does
not itself contain text, the text in the buffer at the stored position
information is compared when determining equality.

Unlike a normal string_view, this type is safe if the buffer underlying
a_text_buffer is reallocated.
*/
struct a_text_buffer_view {
  inline a_boolean operator==(const a_text_buffer_view &other) const;
  inline a_boolean operator!=(const a_text_buffer_view &other) const
    { return !(*this == other); }

  a_text_buffer *buffer;
                        /* The underlying text buffer. */
  sizeof_t      start;  /* The start of this substring. */
  size_t        length; /* The length of this substring. */
};  /* a_text_buffer_view */


a_boolean a_text_buffer_view::operator==(const a_text_buffer_view &other) const
/*
Return TRUE if this text buffer view is equal to the given text buffer view;
otherwise, return FALSE.
*/
{
  a_boolean result = TRUE;

  /* If one of these assertions fail, the respective buffer has either shrunk
     since construction or the view itself was not properly constructed. */
  check_assertion(this->start + this->length <= this->buffer->size);
  check_assertion(other.start + other.length <= other.buffer->size);
  if (this->length != other.length) {
    result = FALSE;
  } else if (strncmp(this->buffer->buffer + this->start,
                     other.buffer->buffer + other.start,
                     this->length) != 0) {
    result = FALSE;
  }  /* if */
  return result;
}  /* a_text_buffer_view::operator== */


template<typename a_Function>
inline a_text_buffer_view capture_buffer_append(a_text_buffer *buffer,
                                                a_Function    func)
/*
Given a text buffer and a function to execute that will append 0 or more
characters to the buffer, run the function and return a text buffer view
representing the content added to the text buffer.
*/
{
  sizeof_t start = buffer->size;

  func();

  sizeof_t end = buffer->size;
  /* If this assertion fails the buffer was shrunk during the call to func
     instead of expanded via an append. */
  check_assertion(end >= start);
  return a_text_buffer_view{buffer, start, end - start};
}  /* capture_buffer_append */


/*
Make sure that the specified buffer has at least "length" total bytes in it.
If not, expand the buffer by reallocating it.
*/
#define ensure_text_buffer_space(buf, length)			\
{ if (((sizeof_t)(length)) > (buf)->allocated_size) {			\
    expand_text_buffer(buf, (sizeof_t)(length));			\
  }  /* if */							\
}  /* ensure_text_buffer_space */

extern void set_buffer_position(a_text_buffer_ptr	buffer,
				char			*pos);

/*
Add the specified character to the text buffer specifier by "buf".
*/
#define add_char_to_text_buffer(buf, ch)				\
{ ensure_text_buffer_space(buf, (buf)->size+1);			\
  (buf)->buffer[(buf)->size] = (ch);					\
  (buf)->size++;							\
}  /* add_char_to_text_buffer */

#if DEBUG
extern void db_text_buffer(a_const_char      *prefix,
			   a_text_buffer_ptr buf);

extern void db_prefix(void  *entry_ptr);

extern an_il_entry_prefix_ptr db_prefix_ptr(char  *entry);
#endif /* DEBUG */

/*
Data structures which can be saved in a pre-compiled header file cannot
contain pointers to functions (as Address Space Layout Randomization
will ensure that addresses vary from invocation to invocation).
Define an enumeration of the functions which are pointed to by such
data structures and use the enumerators as array indices (into the
function_pointers array) so that PCH files can be used on systems that
implement ASLR.

Currently, these function pointer indices are used in the a_hash_table and
a_pragma_kind_description data structures.

When adding an entry in this enumeration, also make a similar change
in function_pointers (in fe_init.c).
*/

enum a_function_number : a_byte {
  fn_null,                             /* Indicates a NULL function pointer. */
  fn_hash_attribute_kind,
  fn_compare_for_attr_corresp_checking_map,
  fn_hash_source_string,
  fn_compare_for_attr_name_map,
#if GNU_EXTENSIONS_ALLOWED
  fn_compare_for_asm_name_map,
#endif /* GNU_EXTENSIONS_ALLOWED */
  fn_hash_include_search_result,
  fn_compare_include_search_result,
  fn_hash_include_file_history,
  fn_compare_include_file_history,
#if UNIQUE_FILE_IDENTIFIER_AVAILABLE
  fn_hash_unique_file_id_for_table,
  fn_compare_unique_file_id,
#endif /* UNIQUE_FILE_IDENTIFIER_AVAILABLE */
#if MICROSOFT_EXTENSIONS_ALLOWED
  fn_hash_include_alias,
  fn_compare_include_alias,
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  fn_hash_instantiation,
  fn_compare_instantiation,
#if MICROSOFT_EXTENSIONS_ALLOWED
  fn_hash_prop_or_event_accessor_header_lookup,
  fn_compare_prop_or_event_accessor_header_lookup,
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  fn_hash_symbol_header_lookup_entry,
  fn_compare_symbol_header_lookup_entry,
  fn_record_arg_pragma,
  fn_instantiation_pragma,
  fn_pack_pragma,
#if IDENT_DIRECTIVE_AND_PRAGMA
  fn_ident_pragma,
  fn_ident_directive,
#endif /* IDENT_DIRECTIVE_AND_PRAGMA */
#if PRAGMA_WEAK_ALLOWED
  fn_weak_pragma,
#endif /* PRAGMA_WEAK_ALLOWED */
  fn_once_pragma,
  fn_hdrstop_or_no_pch_pragma,
  fn_define_type_info_pragma,
  fn_stdc_pragma,
#if UPC_EXTENSIONS_ALLOWED
  fn_upc_pragma,
#endif /* UPC_EXTENSIONS_ALLOWED */
#if REDEFINE_EXTNAME_PRAGMA_ENABLED
  fn_redefine_extname_pragma,
#endif /* REDEFINE_EXTNAME_PRAGMA_ENABLED */
#if SUN_EXTENSIONS_ALLOWED
  fn_ldscope_pragma,
#endif /* SUN_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  fn_gcc_pragma,
#if GNU_VECTOR_TYPES_ALLOWED && BUILTIN_FUNCTIONS_ENABLED
  fn_gnu_riscv_pragma,
  fn_clang_riscv_pragma,
#endif /* GNU_VECTOR_TYPES_ALLOWED && BUILTIN_FUNCTIONS_ENABLED */
#endif /* GNU_EXTENSIONS_ALLOWED */
  fn_diag_pragma,
  fn_diagnostic_pragma,
#if INCLUDE_EDG_TEST_PRAGMAS
  fn_test_immediate_pragma,
  fn_test_next_construct_pragma,
#endif /* INCLUDE_EDG_TEST_PRAGMAS */
#if DEBUG
  fn_db_opt_pragma,
  fn_db_name_pragma,
#endif /* DEBUG */
#if NEED_IL_DISPLAY
  fn_pragma_il_display,
#endif /* NEED_IL_DISPLAY */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
  fn_if_exists_pragma,
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
  fn_push_macro_pragma,
  fn_pop_macro_pragma,
#if MICROSOFT_EXTENSIONS_ALLOWED
  fn_microsoft_start_map_region_pragma,
  fn_microsoft_stop_map_region_pragma,
#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
  fn_setlocale_pragma,
#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
  fn_microsoft_comment_pragma,
  fn_microsoft_conform_pragma,
  fn_microsoft_include_alias_pragma,
  fn_hash_unresolved_type_map_key,
  fn_compare_for_unresolved_type_map,
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  fn_hash_void_pointer,
  fn_compare_for_pointer_pair_map,
  fn_compare_substituted_type_list_entry,
  fn_hash_token_sequence_xref,
  fn_compare_token_sequence_xref,
#if UNICODE_VULNERABILITY_DETECTION_SUPPORTED
  fn_hash_id_representation,
  fn_id_representations_match,
#endif /* UNICODE_VULNERABILITY_DETECTION_SUPPORTED */
  fn_hash_name_reference,
  fn_compare_name_reference,
#if CREATE_LEXICAL_TYPEREFS
  fn_hash_type_and_name_qualifier,
  fn_compare_type_and_name_qualifier,
  fn_hash_type_and_template_arg_list,
  fn_compare_type_and_template_arg_list,
#endif /* CREATE_LEXICAL_TYPEREFS */
  fn_last
};


/* Generic function pointer type. */
typedef void (*a_function_pointer)();

/* Declare the array of function pointers that map to the enumeration above. */
EXTERN_CONSTINIT_ARRAY_FORWARD_DECL(a_function_pointer, function_pointers,
                                    fn_last + 1)

/*
This token pasting macro converts a function name into an enumerator
in a_function_number_tag (by prepending "fn_" to the function name).
Callers must ensure that the appropriate enumerator has been added to the
a_function_number_tag enumeration (as well as the definition of
function_pointers in fe_init.c).
*/
#define fn_for_function(name) ((a_function_number)(EDG_CONCAT(fn_,name)))

/*
A macro to convert a function pointer enumerator (a_function_number_tag)
into a function pointer.  The caller must cast the result to a function
pointer of the appropriate type.
*/
#if CHECKING
#define index_to_function_pointer(index) \
  (check_assertion((unsigned int)(index) < (unsigned int)fn_last), \
   function_pointers[(unsigned int)index])
#else /* !CHECKING */
#define index_to_function_pointer(index) \
  function_pointers[(unsigned int)index]
#endif /* CHECKING */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef MEM_MANAGE_H */

