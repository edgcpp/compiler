/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

mem_manage.c -- Memory management routines.

*/

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

#if IL_SHOULD_BE_WRITTEN_TO_FILE
#include "il_file.h"
#if !STANDALONE_UTILITY_PROGRAM
#include "il_write.h"
#endif /* !STANDALONE_UTILITY_PROGRAM */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

#if !STANDALONE_UTILITY_PROGRAM
#include "pch.h"
#endif /* !STANDALONE_UTILITY_PROGRAM */

#include "direct_allocator.h"

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
Define a macro that is TRUE in configurations that need the
memory_allocation_map to be tracked (to ensure all general allocation is freed
or for checking purposes); otherwise, this macro is FALSE and any non-reclaimed
general allocation is reclaimed when the process exits.
*/
#if MAKE_FRONT_END_CALLABLE || EXPENSIVE_CHECKING
#define TRACK_GENERAL_ALLOCATION TRUE
#else  /* !(MAKE_FRONT_END_CALLABLE || EXPENSIVE_CHECKING) */
#define TRACK_GENERAL_ALLOCATION FALSE
#endif /* MAKE_FRONT_END_CALLABLE || EXPENSIVE_CHECKING */

#if TRACK_GENERAL_ALLOCATION

using a_memory_allocation_map = Ptr_map<void*, sizeof_t, Direct_allocator>;
                        /* The type for a memory allocation map used for
                           internal tracking of allocated memory. */

STATIC_THREAD a_memory_allocation_map
		*memory_allocation_map;
			/* A list of memory blocks allocated in general memory.
			   This is used to free the blocks at the end of
			   compilation. */

#endif /* TRACK_GENERAL_ALLOCATION */
#if EXPENSIVE_CHECKING

using a_memory_allocation_set = Ptr_set<void*, Direct_allocator>;
                        /* The type for a memory allocation set used for
                           internal tracking of allocated memory. */

STATIC_THREAD a_memory_allocation_set
		*resizable_memory_allocations;
			/* A set of pointers in the memory allocation map that
			   are allowed to be resized.  This is used to make
			   sure only blocks that were marked resizable can be
			   resized. */

#endif /* EXPENSIVE_CHECKING */

#ifdef USING_PURIFY
#include "purify.h"

STATIC_THREAD a_boolean
		purify_is_active;
			/* TRUE when a purify'd version of the executable
			   is being used.  This causes the memory allocation
			   routines to allocate the memory in a way that
			   can be tracked by Purify. */
#endif /* USING_PURIFY */


STATIC_THREAD a_mem_block_header_ptr
		reusable_blocks_list;
			/* List of memory blocks freed and available for
			   reuse.  These are only partial blocks; full blocks
			   are actually freed with free. */

STATIC_THREAD a_boolean
		okay_to_free_mem_blocks;
			/* TRUE if it is okay to free (using free())
			   memory region blocks that are no longer needed.
			   This is set FALSE if other memory (for example,
			   mmap memory) is being used for memory region
			   blocks. */

/*
Size of a_mem_block_header after adjustment so that the storage following
it will be properly aligned.  This is a constant.
*/
static sizeof_t adjusted_header_size = (sizeof_t)(sizeof(a_mem_block_header) +
  /*lint --e(835,506)*/
  ((sizeof(a_mem_block_header) % HOST_ALIGNMENT_REQUIRED) == 0 ?
     0 : (HOST_ALIGNMENT_REQUIRED -
          (sizeof(a_mem_block_header) % HOST_ALIGNMENT_REQUIRED))));


#if DEBUG
/* GCOVR_EXCL_START */
/*
Record of memory allocated, for space tracking purposes.
*/
STATIC_THREAD unsigned long
		max_mem_allocated;
			/* The high-water mark, the largest amount of memory
			   allocated by malloc but not freed during
			   execution. */
STATIC_THREAD unsigned long
		mem_in_use_after_one_time_init;
			/* This is a snapshot of (num_bytes_allocated -
			   num_bytes_freed) when one-time initialization has
			   completed. */
STATIC_THREAD unsigned long
		max_mem_in_use_after_tu_init;
			/* This is a snapshot of the largest value
			   (num_bytes_allocated - num_bytes_freed) when TU
			   initialization has completed. */
STATIC_THREAD unsigned long
		num_reclaim_attempts;
			/* Total number of times the reusable_blocks_list was
			   consulted while non-NULL as part of a
			   alloc_new_mem_block call. */
STATIC_THREAD unsigned long
		num_reclaim_traversals;
			/* Total number of link entries consulted when
			   attempting to reuse a block on the
			   reusable_blocks_list. */
STATIC_THREAD unsigned long
		num_reclaim_successes;
			/* Total number of times a reclaim attempt was made
			   from the reusable_blocks_list that succeeded. */
STATIC_THREAD unsigned long
		num_malloc_calls;
			/* Total number of calls to malloc. */
STATIC_THREAD unsigned long
		num_bytes_allocated;
			/* Total number of bytes allocated by malloc. */
STATIC_THREAD unsigned long
		num_free_calls;
			/* Total number of calls to free. */
STATIC_THREAD unsigned long
		num_bytes_freed;
			/* Total number of bytes freed by free. */
STATIC_THREAD unsigned long
		total_general_mem_allocated;
			/* The amount of memory that was allocated in (but not
			   freed from) general storage, i.e., by
			   alloc_general and realloc_general. */
STATIC_THREAD unsigned long
		total_mem_used;
			/* Total memory allocated by alloc_in_region.
			   Reset for each input file. */
STATIC_THREAD unsigned long
		num_alignment_bytes_allocated;
			/* Number of bytes wasted in alignment cracks.
			   Reset for each input file. */

#if USE_MMAP_FOR_MEMORY_REGIONS
STATIC_THREAD unsigned long
		num_mapped_bytes_allocated;
			/* Number of bytes of memory that are mapped to
			   files. */

STATIC_THREAD unsigned long
		num_mapped_bytes_from_pch;
			/* Number of bytes of memory that have been mapped
			   from a precompiled header file. */
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */

STATIC_THREAD unsigned long
		num_text_buffers_allocated;
			/* Number of text buffers that have been allocated. */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

#if DEBUG
/* GCOVR_EXCL_START */
static void adjust_record_of_total_allocation(long amount)
/*
Record that an additional "amount" bytes of memory have been allocated.
"amount" is negative to indicate space being freed.  "Allocated"
and "freed" here mean via malloc/free, not by some mechanism on top of that.
*/
{
  /* Do "increment" carefully, since one variable is unsigned and the
     other is not. */
  if (amount >= 0) {
    num_bytes_allocated += (unsigned long)amount;
    ++num_malloc_calls;
  } else {
    num_bytes_freed += (unsigned long)-amount;
    ++num_free_calls;
  }  /* if */
  /* Keep track of the high-water mark. */
  if ((num_bytes_allocated - num_bytes_freed) > max_mem_allocated) {
    max_mem_allocated = (num_bytes_allocated - num_bytes_freed);
  }  /* if */
}  /* adjust_record_of_total_allocation */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

static char *malloc_with_check(sizeof_t size)
/*
Interface to malloc that allocates "size" bytes.  Checks for failure of 
allocation and generates a catastrophic error.
*/
{
  char *ptr;

  if (size > PTRDIFF_MAX) {
    /* Newer versions of GCC detect under -Wall when malloc is called with a
       size exceeding the maximum object size.  This warning is triggered for
       some cases of (highly unlikely) user input driven allocations.

       To prevent this warning from being issued (and protect against these
       unlikely cases) the allocation size is checked explicitly before calling
       malloc.  */
    insufficient_address_space();
  }  /* if */
  if ((ptr = (char *)malloc((true_size_t)size_t_arg(size))) == NULL) {
    catastrophe(ec_out_of_memory);
  }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
  /* Track total allocation. */
  /* Can't do this conditionally on db_active since db_active is not yet
     set when command line processing is done. */
  adjust_record_of_total_allocation((long)size);
  if (db_flag_is_set("malloc") || debug_level >= 5) {
    fprintf(f_debug, "malloc_with_check: allocating %lu at %p, total = %lu\n",
                     (unsigned long)size, (a_void_ptr)ptr,
                     (unsigned long)(num_bytes_allocated - num_bytes_freed));
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  return (ptr);
}  /* malloc_with_check */


void *malloc_for_interpreter(sizeof_t size)
/*
Interface to malloc for the interpreter.  A failure to allocate doesn't trigger
a catastrophe.
*/
{
  void *result;

  result = (void*)malloc((true_size_t)size_t_arg(size));
#if DEBUG
/* GCOVR_EXCL_START */
  /* Track total allocation. */
  if (result != NULL) {
    /* Can't do this conditionally on db_active since db_active is not yet
       set when command line processing is done. */
    adjust_record_of_total_allocation((long)size);
    if (db_flag_is_set("malloc") || debug_level >= 5) {
      fprintf(f_debug,
              "malloc_for_interpreter: allocating %lu at %p, total = %lu\n",
              (unsigned long)size, result,
              (unsigned long)(num_bytes_allocated - num_bytes_freed));
    }  /* if */
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  return result;
}  /* malloc_for_interpreter */


void free_for_interpreter(void     *block,
                          sizeof_t size)
/*
Free a block of the given size allocated by a call to malloc_for_interpreter.
*/
{
#if DEBUG
/* GCOVR_EXCL_START */
  /* Can't do this conditionally on db_active since db_active is not yet
     set when command line processing is done. */
  adjust_record_of_total_allocation(-(long)size);
  if (debug_level >= 5) {
    fprintf(f_debug, "free_for_interpreter: freeing block of size %lu\n",
                     (unsigned long)size);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  free((char*)block);
}  /* free_for_interpreter */


static char *realloc_with_check(char                *old_ptr,
                                ARG_UNUSED sizeof_t old_size,
                                sizeof_t            new_size)
/*
Interface to realloc: reallocate the block pointed to by "old_ptr" to give
it the new size "new_size".  If "old_ptr" is NULL, works like 
malloc_with_check.  "old_size" is present to help with tracking of space used.
*/
{
  char *ptr;

  /* Don't count on realloc allowing a first parameter of NULL to imply
     malloc-like behavior.  The SVID doesn't define realloc that way. */
  if (old_ptr == NULL) {
    ptr = malloc_with_check(new_size);
  } else {
    if ((ptr = (char *)realloc(old_ptr,
                               (true_size_t)size_t_arg(new_size))) == NULL) {
      catastrophe(ec_out_of_memory);
    }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
    /* Track total allocation. */
    /* Can't do this conditionally on db_active since db_active is not yet
       set when command line processing is done. */
    adjust_record_of_total_allocation((long)(new_size - old_size));
    if (debug_level >= 5) {
      fprintf(f_debug,
         "realloc_with_check: new size = %lu, old size = %lu, total = %lu\n",
                       (unsigned long)new_size,
                       (unsigned long)old_size,
                       (unsigned long)(num_bytes_allocated - num_bytes_freed));
    }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  }  /* if */
  return (ptr);
}  /* realloc_with_check */


#if !STANDALONE_UTILITY_PROGRAM

#define MEM_ALLOC_HISTORY_INCREMENTAL_ALLOCATION 500
			/* Initial and incremental allocation sizes for
			   mem_alloc_history.  */


static void add_mem_alloc_history_entry(a_void_ptr	addr,
		                        sizeof_t	size)
/*
Add an entry to the memory allocation history array.
*/
{
  a_mem_alloc_history_ptr	mahp;
  db_enter(5, "add_mem_alloc_history_entry");
#if USE_MMAP_FOR_MEMORY_REGIONS
  if (num_of_mem_alloc_history_entries == size_of_mem_alloc_history) {
    /* There is no more space in the array, allocate a larger array. */
    a_mem_alloc_history_number	old_size;
    a_mem_alloc_history_number	new_size;
    old_size = size_of_mem_alloc_history;
    new_size = old_size + MEM_ALLOC_HISTORY_INCREMENTAL_ALLOCATION;
    size_of_mem_alloc_history = new_size;
    mem_alloc_history = (a_mem_alloc_history_ptr)realloc_buffer
                          ((char *)mem_alloc_history,
	                   (sizeof_t)old_size * sizeof(a_mem_alloc_history),
		           (sizeof_t)new_size * sizeof(a_mem_alloc_history));
  }  /* if */
#else /* !USE_MMAP_FOR_MEMORY_REGIONS */
  if (num_of_mem_alloc_history_entries == SIZE_OF_MEM_ALLOC_HISTORY) {
    /* This condition should be handled by the caller. */
    unexpected_condition_str2("add_mem_alloc_history_entry:",
			      "too many memory history entries");
  }  /* if */
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
  mahp = &mem_alloc_history[num_of_mem_alloc_history_entries++];
  mahp->addr = addr;
  mahp->size = size;
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 5) {
    fprintf(f_debug, "Added mem_alloc_history, addr: %p, size: %lu\n",
            addr, (unsigned long)size);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  db_exit();
}  /* add_mem_alloc_history_entry */


#if !USE_MMAP_FOR_MEMORY_REGIONS
void preallocate_pch_memory(void)
/*
Preallocate the memory to be used for PCH memory region storage when
memory mapping is not available.  The command line processing routine
is responsible for making sure that pch_mem_size is not large enough
to cause the memory allocation history table to overflow.
*/
{
  sizeof_t			size_allocated = 0;

  /* If no value was provided on the command line, use the default value. */
  if (pch_mem_size == 0) pch_mem_size = DEFAULT_PREALLOCATED_PCH_MEM_SIZE;
  while (size_allocated < pch_mem_size) {
    a_void_ptr	ptr;
    ptr = (a_void_ptr)malloc(HOST_ALLOCATION_INCREMENT);
    if (ptr == NULL) {
      catastrophe(ec_out_of_memory_during_pch_allocation);
    }  /* if */
    size_allocated += HOST_ALLOCATION_INCREMENT;
    add_mem_alloc_history_entry(ptr, HOST_ALLOCATION_INCREMENT);
#if DEBUG
/* GCOVR_EXCL_START */
    /* Track total allocation. */
    /* Can't do this conditionally on db_active since db_active is not yet
       set when command line processing is done. */
    adjust_record_of_total_allocation((long)HOST_ALLOCATION_INCREMENT);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  }  /* while */
}  /* preallocate_pch_memory */


void free_unused_pch_memory(void)
/*
We have completed any PCH processing that is required.  Release any
preallocated memory that has not been used.
*/
{
  a_mem_alloc_history_number	n;

  /* Free any unused preallocated blocks. */
  for (n = mem_alloc_history_entries_used;
       n < num_of_mem_alloc_history_entries; ++n) {
    (void)free(mem_alloc_history[n].addr);
#if DEBUG
/* GCOVR_EXCL_START */
    adjust_record_of_total_allocation(-(long)(mem_alloc_history[n].size));
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  }  /* for */
  /* Set the number of entries in existence to the number used so far.
     This will cause any new memory blocks to malloc new memory instead
     of trying to use the preallocated memory. */
  num_of_mem_alloc_history_entries = mem_alloc_history_entries_used;
}  /* free_unused_pch_memory */
#endif /* !USE_MMAP_FOR_MEMORY_REGIONS */


#if USE_MMAP_FOR_MEMORY_REGIONS
STATIC_THREAD a_boolean
		mmap_initialized;
			/* TRUE when the file used for mmap has been
			   opened. */

STATIC_THREAD sizeof_t
		mmap_size_allocated;
			/* The number of bytes of mapped memory that have
			   been allocated.  This is usually the same as
			   mmap_file_offset, except when a precompiled
			   header file is in use.  The memory mapped from
			   the PCH file is included in mmap_size_allocated,
			   but not in mmap_file_offset. */

STATIC_THREAD sizeof_t
		mmap_file_offset;
			/* The offset into the mmap file of the next block
			   to be allocated. */

STATIC_THREAD an_mmap_handle
		memory_region_file;
			/* The temporary file used to back the memory mapped
			   region blocks. */

void record_mapped_mem_block(a_void_ptr	addr,
			     sizeof_t	size)
/*
Create a memory allocation history entry for a block of memory
mapped to a file.  This is used when regions from a PCH input
file are mapped into the address space of subsequent compilation.
*/
{
  /* Record this allocation in the memory allocation history array. */
  add_mem_alloc_history_entry(addr, size);
  /* The number of entries actually used is always the same as the
     number of entries that exist in mmap mode. */
  mem_alloc_history_entries_used = num_of_mem_alloc_history_entries;
  mmap_size_allocated += size;
#if DEBUG
/* GCOVR_EXCL_START */
  /* Record the total amount of allocated memory that was allocated via
     memory mapped files. */
  num_mapped_bytes_allocated += (unsigned long)size;
  num_mapped_bytes_from_pch += (unsigned long)size;
  adjust_record_of_total_allocation((long)size);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
}  /* record_mapped_mem_block */


void free_mapped_mem_blocks(void)
/*
Unmap the memory blocks that have been mapped.
*/
{
  a_mem_alloc_history_number	n;

  for (n = 0; n < num_of_mem_alloc_history_entries; ++n) {
    sizeof_t	size = mem_alloc_history[n].size;
    unmap_memory(mem_alloc_history[n].addr, size);
#if DEBUG
/* GCOVR_EXCL_START */
    /* Record the total amount of allocated memory that was allocated via
       memory mapped files. */
    num_mapped_bytes_allocated -= (unsigned long)size;
    adjust_record_of_total_allocation(-(long)size);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  }  /* for */
  num_of_mem_alloc_history_entries = 0;
  mem_alloc_history_entries_used = 0;
  /* Reset the number of bytes allocated in the memory mapped file so that
     any new allocations will start over from the beginning of the file. */
  mmap_size_allocated = 0;
  mmap_file_offset = 0;
}  /* free_mapped_mem_blocks */


a_void_ptr alloc_new_mem_block(sizeof_t size)
/*
Allocate a block of memory to be used for memory region storage.  This
version uses a memory mapped file to obtain the storage.  This is done
so that the memory region storage may be obtained in a separate
range of addresses.  This, in turn, simplifies the processing needed
to ensure that the precompiled header processing routines can read
the memory regions into the same addresses that were used when the
PCH was created.
*/
{
  a_void_ptr	addr = NULL;

  if (!mmap_initialized) {
    /* On the first call, open the file that will be mapped. */
    memory_region_file = open_memory_region_tmp_file();
    mmap_size_allocated = 0;
    mmap_initialized = TRUE;
    mmap_file_offset = 0;
#if USE_FIXED_ADDRESS_FOR_MMAP
    /* Attempt using the fixed address; if the mapping fails, clear
       fixed_address_for_mmap so that all subsequent allocations use a
       system-assigned address instead. */
    addr = map_memory_region_file(memory_region_file,
                                  (void*)fixed_address_for_mmap,
                                  mmap_size_allocated, size, mmap_file_offset);
    if (addr == NULL) {
      fixed_address_for_mmap = NULL;
    }  /* if */
#endif /* USE_FIXED_ADDRESS_FOR_MMAP */
  }  /* if */
  if (addr == NULL) {
    addr = map_memory_region_file(memory_region_file,
#if USE_FIXED_ADDRESS_FOR_MMAP
                                  /*base_addr=*/(void*)fixed_address_for_mmap,
#else /* !USE_FIXED_ADDRESS_FOR_MMAP */
                                  /*base_addr=*/NULL,
#endif /* USE_FIXED_ADDRESS_FOR_MMAP */
                                  mmap_size_allocated, size, mmap_file_offset);
  }  /* if */
  if (addr == NULL) {
    catastrophe(ec_unable_to_get_mapped_memory);
  }  /* if */
  mmap_size_allocated += size;
  mmap_file_offset += size;
  /* Record this allocation in the memory allocation history array. */
  add_mem_alloc_history_entry(addr, size);
  /* The number of entries actually used is always the same as the
     number of entries that exist in mmap mode. */
  mem_alloc_history_entries_used = num_of_mem_alloc_history_entries;
#if DEBUG
/* GCOVR_EXCL_START */
  /* Record the total amount of allocated memory that was allocated via
     memory mapped files. */
  num_mapped_bytes_allocated += (unsigned long)size;
  adjust_record_of_total_allocation((long)size);
  if (debug_level >= 5) {
    fprintf(f_debug, "Allocated %lu bytes of mapped memory at %p\n",
            (unsigned long)size, addr);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  return addr;
}  /* alloc_new_mem_block */

#else /* !USE_MMAP_FOR_MEMORY_REGIONS */

STATIC_THREAD a_boolean
		additional_allocation_needed;
			/* TRUE when an allocation has been done that
			   cannot be satisfied by the pre-allocated memory. */


a_void_ptr alloc_new_mem_block(sizeof_t size)
/*
Allocate a block of memory to be used for memory region storage.  This
version uses a fixed block of memory allocated at the beginning of
the compilation.  This is done to maximize the probability that
the memory addresses used in one compilation will match the addresses
used in a subsequent compilation, which is necessary when using
precompiled headers.  When the preallocated memory is exhausted,
additional memory is obtained using malloc and any use of
precompiled headers is suppressed.
*/
{
  a_void_ptr		addr;

  if (!additional_allocation_needed) {
    if (mem_alloc_history_entries_used == num_of_mem_alloc_history_entries) {
      /* All of the preallocated memory has been used. */
      exhausted_preallocated_memory = TRUE;
      additional_allocation_needed = TRUE;
    } else if (size != HOST_ALLOCATION_INCREMENT) {
      /* A memory block is required that is larger than any of the ones that
         have been preallocated. */
      large_mem_block_needed = TRUE;
      large_mem_block_error_pos = error_position;
      additional_allocation_needed = TRUE;
    }  /* if */
    if (additional_allocation_needed) {
      /* A condition occurred which makes it impossible to create a
         precompiled header file. */
      suppress_creation_of_pch();
      /* Free any unused preallocated blocks. */
      free_unused_pch_memory();
    }  /* if */
  }  /* if */
  if (additional_allocation_needed) {
    /* On this call, or a previous call, we needed to use memory other
       than that was preallocated.  All subsequent allocations should
       just be done by malloc. */
    addr = (a_void_ptr)malloc_with_check(size);
  } else {
    /* Get the next entry from the preallocated list. */
    addr = mem_alloc_history[mem_alloc_history_entries_used++].addr;
  }  /* if */
  total_mem_blocks_allocated++;
  return addr;
}  /* alloc_new_mem_block */
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
#endif /* !STANDALONE_UTILITY_PROGRAM */
 

a_mem_block_header_ptr alloc_mem_block(a_memory_region_number region_number,
                                       sizeof_t               min_size,
                                       char                   *desired_addr,
                                       a_boolean              small_extension)
/*
Add a new memory block to the existing blocks for the indicated region.
The memory block must have at least "min_size" bytes available in it.
If desired_addr is not NULL, look for a memory block for which the
actual start_of_block is at the specified address.  If small_extension
is TRUE, this allocation is a small extension on a memory region; allocate
a smaller-sized block.  Return a pointer to the block header.
*/
{
  a_mem_block_header_ptr hdr, prev_hdr;
  sizeof_t               alloc_size, needed_size, default_size;
  a_void_ptr             alloc_addr;
  a_mem_block_header_ptr hdr_found = NULL;
  a_mem_block_header_ptr prev_hdr_found = NULL;

  db_enter(5, "alloc_mem_block");
  /* Determine the desirable default allocation size. */
  if (small_extension) {
    default_size = (sizeof_t)2048;
  } else {
    default_size = HOST_ALLOCATION_INCREMENT;
  }  /* if */
  /* Reuse a previously-allocated piece if possible.  Such a piece was
     the wasted space on the end of a previous block. */
  if (reusable_blocks_list != NULL) {
#if DEBUG
/* GCOVR_EXCL_START */
    ++num_reclaim_attempts;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
    needed_size = min_size + adjusted_header_size;
    for (prev_hdr = NULL, hdr = reusable_blocks_list;
         hdr != NULL;
         prev_hdr = hdr, hdr = hdr->next) {
      /* See if the area is big enough (it almost always will be). */
      /* Suppress the CodeCenter warning caused because after_end_of_block
         may be pointing to memory that is not allocated, or is part of a
         different allocation. */
      /*SUPPRESS 22*/
      check_assertion(hdr->after_end_of_block >= hdr->start_of_block);
#if DEBUG
/* GCOVR_EXCL_START */
      ++num_reclaim_traversals;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
      alloc_size = (sizeof_t)(hdr->after_end_of_block - hdr->start_of_block) +
                   adjusted_header_size;
      if (alloc_size >= needed_size) {
        if (hdr->start_of_block == desired_addr || hdr_found == NULL) {
           /* We've found a candidate, or if this is the desired address,
              we've found a definite match.  Save a pointer to this block */
           hdr_found = hdr;
           prev_hdr_found = prev_hdr;
           if (desired_addr == NULL ||
               hdr->start_of_block == desired_addr) break;
        }  /* if */
      }  /* if */
    }  /* for */
    if (hdr_found != NULL) {
      /* We've found an acceptable piece.  Take it out of the list
         and use it. */
      if (prev_hdr_found == NULL) {
        reusable_blocks_list = hdr_found->next;
      } else {
        prev_hdr_found->next = hdr_found->next;
      }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
      if (debug_level >= 5) {
        fprintf(f_debug, "alloc_mem_block: reusing block, size = %lu\n",
                         (unsigned long)alloc_size);
      }  /* if */
      ++num_reclaim_successes;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
      hdr = hdr_found;
      goto have_hdr;
    }  /* if */
  }  /* if */
  /* No piece available for reuse, so allocate a new one. */
  alloc_size = min_size + adjusted_header_size;
#ifdef USING_PURIFY
  if (!purify_is_active) {
#endif /* USING_PURIFY */
    /* Use the default allocation size unless the minimum required
       size is bigger than that (that's possible for incredibly large
       string literals formed by token concatenation). */
    if (alloc_size < default_size) alloc_size = default_size;
#ifdef USING_PURIFY
  } else {
    /* Don't use the default allocation size when using Purify.  Just
       allocate a block of the proper size. */
    /* If the minimum size is zero, allocate HOST_ALIGNMENT_REQUIRED bytes
       of storage beyond what is used by the header, which is the
       minimum possible allocation. */
    if (min_size == 0) alloc_size += HOST_ALIGNMENT_REQUIRED;
  }  /* if */
#endif /* USING_PURIFY */
  /* Make sure the block size preserves alignment of the end (this is
     just so that we're not allocating space at the end that can hardly 
     ever be used). */
  do_host_alignment(&alloc_size);
#if STANDALONE_UTILITY_PROGRAM
  alloc_addr = malloc_with_check(alloc_size);
#else /* !STANDALONE_UTILITY_PROGRAM */
  if (precompiled_header_processing_required) {
#if USE_MMAP_FOR_MEMORY_REGIONS
    /* When using mmap, make sure the size is a multiple of the host
       page size. */
    alloc_size = do_page_alignment(alloc_size);
#else /* !USE_MMAP_FOR_MEMORY_REGIONS */
    /* When not using mmap, all allocations must match the
       HOST_ALLOCATION_INCREMENT; otherwise, the memory allocation history will
       not be processed properly. */
    if (alloc_size < HOST_ALLOCATION_INCREMENT) {
      alloc_size = HOST_ALLOCATION_INCREMENT;
    }  /* if */
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
    alloc_addr = alloc_new_mem_block(alloc_size);
  } else {
    alloc_addr = malloc_with_check(alloc_size);
  }  /* if */
#endif /* STANDALONE_UTILITY_PROGRAM */
  /* Fill in the block header. */
  hdr = (a_mem_block_header_ptr)alloc_addr;
  /* malloc_size non-zero indicates that this block came directly from
     malloc. */
  hdr->malloc_size = alloc_size;
  hdr->start_of_block = (char *)alloc_addr + adjusted_header_size;
  hdr->after_end_of_block = (char *)alloc_addr + alloc_size;
have_hdr:
  /* Everything in the block is available. */
  hdr->next_avail_in_block = hdr->start_of_block;
  hdr->trimmed = FALSE;
#ifdef USING_PURIFY
  /* When using Purify, reserve the smallest possible piece of memory
     at the beginning of the block.  This is done to prevent what
     looks like an unused memory block from being created. */
  if (min_size == 0 && purify_is_active) {
    hdr->next_avail_in_block += HOST_ALIGNMENT_REQUIRED;
  }  /* if */
#endif /* USING_PURIFY */
  /* Link the block into the region. */
  hdr->next = mem_region_table[region_number];
  mem_region_table[region_number] = hdr;
  db_exit();
  return (hdr);
}  /* alloc_mem_block */


static void free_complete_block(a_mem_block_header_ptr hdr)
/*
Free a block that was allocated by malloc.
*/
{
#if DEBUG
/* GCOVR_EXCL_START */
  /* Can't do this conditionally on db_active since db_active is not yet
     set when command line processing is done. */
  adjust_record_of_total_allocation(-((long)hdr->malloc_size));
  if (debug_level >= 5) {
    fprintf(f_debug, "free_complete_block: freeing block of size %lu\n",
                     (unsigned long)hdr->malloc_size);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  free((char *)hdr);
}  /* free_complete_block */


static void free_mem_block(a_mem_block_header_ptr hdr)
/*
Free the storage associated with the indicated memory block.
*/
{
  db_enter(5, "free_mem_block");
#if OVERWRITE_FREED_MEM_BLOCKS
  /* Overwrite the memory being freed so that any reference to the freed
     memory is more likely to be detected. */
  memset(hdr->start_of_block, 0xdb,
         size_t_arg(hdr->after_end_of_block - hdr->start_of_block));
#endif /* OVERWRITE_FREED_MEM_BLOCKS */
  if (!okay_to_free_mem_blocks) {
    /* If memory blocks cannot be freed, don't attempt to merge the blocks.
       This is an optimization because the list of blocks can get quite large
       when they are not being freed. */
    hdr->next = reusable_blocks_list;
    reusable_blocks_list = hdr;
  } else if (hdr->malloc_size > 0 &&
             hdr->malloc_size ==
                           (sizeof_t)(hdr->after_end_of_block - (char *)hdr)) {
    /* Blocks that are complete blocks as originally allocated by malloc
       can be freed by calling free. */
    free_complete_block(hdr);
  } else {
    /* Add the resulting block to the list. */
    hdr->next = reusable_blocks_list;
    reusable_blocks_list = hdr;
  }  /* if */
  db_exit();
}  /* free_mem_block */


void trim_mem_block(a_mem_block_header_ptr hdr)
/*
Free any unallocated space remaining in the indicated memory block.
*/
{
  sizeof_t               space_remaining_in_block;
  a_mem_block_header_ptr new_hdr;
  char                   *alloc_addr;

  db_enter(5, "trim_mem_block");
  /* Save the remaining space only if it's big enough. */
  check_assertion(hdr->after_end_of_block >= hdr->next_avail_in_block);
  space_remaining_in_block = (sizeof_t)(hdr->after_end_of_block -
                                        hdr->next_avail_in_block);
  /* The criterion for "big enough" is really not for very much space.
     Even very small blocks can be reused, at a minor cost in
     execution time if the file scope region gets too fragmented. */
  if (space_remaining_in_block >= HUGE_FE_MEM_THRESHOLD) {
    /* Remaining space is "big enough" that it's worth saving.  We know
       next_avail_in_block is properly aligned because of the way that
       alloc_in_region works.  Fabricate a header for the space, then 
       free it. */
    alloc_addr = hdr->next_avail_in_block;
    new_hdr = (a_mem_block_header_ptr)alloc_addr;
    /* Indicate that the area didn't come directly from malloc. */
    new_hdr->malloc_size = 0;
    new_hdr->next_avail_in_block = new_hdr->start_of_block =
                                  alloc_addr + adjusted_header_size;
    new_hdr->after_end_of_block = alloc_addr + space_remaining_in_block;
    new_hdr->trimmed = FALSE;
    /* Recycle the block. */
    new_hdr->next = reusable_blocks_list;
    reusable_blocks_list = new_hdr;
    /* Trim the original block so it does not include the freed space. */
    hdr->after_end_of_block = alloc_addr;
  }  /* if */
  hdr->trimmed = TRUE;
  db_exit();
}  /* trim_mem_block */

#if DEBUG
/* GCOVR_EXCL_START */

void track_allocation(a_memory_region_number region_number,
                      sizeof_t               size,
                      sizeof_t               orig_size)
/*
Add the given number of allocated bytes to the total allocation for the given
region number.  If the original size differs from the allocated size, the extra
bytes will be added to the total number of bytes allocated for alignment
purposes.
*/
{
  total_mem_used += (unsigned long)size;
  num_alignment_bytes_allocated += (unsigned long)(size - orig_size);
  /* Can't do this conditionally on db_active since db_active is not yet
     set when command line processing is done. */
  allocated_in_region[region_number] += (unsigned long)size;
}  /* track_allocation */

/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

void ensure_function_def_table_space(a_function_def_number function_def_number)
/*
Make sure that the function definition table is large enough to hold
the number of entries indicated by function_def_number.
*/
{
  a_function_def_number old_size;

  if (function_def_number >= size_of_function_def_table) {
    /* function_def_table must be created or enlarged. */
    /* Add enough entries to cover a pretty large compilation (each function
       compiled uses one entry). */
    old_size = size_of_function_def_table;
    size_of_function_def_table = function_def_number + 2048;
    il_header.function_def_table = (a_function_def_descr*)realloc_buffer(
                          (char *)il_header.function_def_table,
                          (sizeof_t)old_size * sizeof(a_function_def_descr),
                          ((sizeof_t)size_of_function_def_table *
                           sizeof(a_function_def_descr)));
    /* Depending on NULL represented as zero bits here. */
    memzero((char *)&il_header.function_def_table[old_size],
            ((size_t)(size_of_function_def_table - old_size) *
             sizeof(a_function_def_descr)));
  }  /* if */
}  /* ensure_function_def_table_space */

#if !STANDALONE_UTILITY_PROGRAM

a_function_def_number new_function_def_number(void)
/*
Assign a function definition number and make sure that the function definition
table is large enough to hold the new entry.  Return the function definition
number.  Each function definition has a definition number assigned to it.
The number is used to access the function definition entry, which is needed
to get to the scope of the function, and also to get to the memory region
if you only have a function definition number and not a routine pointer.
*/
{
  a_function_def_number	result;

  if (highest_used_function_def_number == MAX_FUNCTION_DEF_NUMBER) {
    /* Too many function definitions. */
    catastrophe(ec_program_too_large);
  }  /* if */
  result = ++highest_used_function_def_number;
  ensure_function_def_table_space(result);
  return result;
}  /* new_function_def_number */

#endif /* !STANDALONE_UTILITY_PROGRAM */

void ensure_mem_region_table_space(a_memory_region_number region_number)
/*
Make sure that the memory region tables are large enough to hold
the number of entries indicated by region_number.
*/
{
  a_memory_region_number old_size;

  if (region_number >= size_of_mem_region_table) {
    /* mem_region_table must be created or enlarged. */
    /* Add enough entries to cover a pretty large compilation (each top-level
       function compiled uses one entry). */
    old_size = size_of_mem_region_table;
    size_of_mem_region_table = region_number + 2048;
    mem_region_table = (a_mem_block_header_ptr *)realloc_buffer(
                          (char *)mem_region_table,
                          (sizeof_t)old_size * sizeof(a_mem_block_header_ptr),
                          (sizeof_t)size_of_mem_region_table *
                                               sizeof(a_mem_block_header_ptr));
    /* Depending on NULL represented as zero bits here. */
    memzero((char *)&mem_region_table[old_size],
            ((size_t)(size_of_mem_region_table - old_size) *
             sizeof(a_mem_block_header_ptr)));
    /* region_scope_entry is a parallel array to mem_region_table, and must
       be similarly expanded. */
    il_header.region_scope_entry = (a_scope_ptr *)realloc_buffer(
                          (char *)il_header.region_scope_entry,
                          (sizeof_t)old_size * sizeof(a_scope_ptr),
                          (sizeof_t)size_of_mem_region_table *
                                                          sizeof(a_scope_ptr));
    /* Depending on NULL represented as zero bits here. */
    memzero((char *)&il_header.region_scope_entry[old_size],
            ((size_t)(size_of_mem_region_table - old_size) *
             sizeof(a_scope_ptr)));
#if IL_SHOULD_BE_WRITTEN_TO_FILE
    /* ... and also index_for_il_file. */
    index_for_il_file = (a_file_position *)realloc_buffer(
                          (char *)index_for_il_file,
                          (sizeof_t)old_size * sizeof(a_file_position),
                          (sizeof_t)size_of_mem_region_table *
                                                      sizeof(a_file_position));
    memzero((char *)&index_for_il_file[old_size],
            ((size_t)(size_of_mem_region_table - old_size) *
             sizeof(a_file_position)));
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
  }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
  /* ... and also allocated_in_region.  Note that this allocation might
         be out of sync with the others. */
  /* Can't do this conditionally on db_active since db_active is not yet
     set when command line processing is done. */
  if (size_of_allocated_in_region < size_of_mem_region_table) {
    allocated_in_region = (unsigned long *)realloc_buffer(
                          (char *)allocated_in_region,
                          (sizeof_t)size_of_allocated_in_region *
                                                        sizeof(unsigned long),
                          (sizeof_t)size_of_mem_region_table *
                                                        sizeof(unsigned long));
    /* Zero the new entries. */
    memzero((char *)&allocated_in_region[size_of_allocated_in_region],
            ((size_t)(size_of_mem_region_table - size_of_allocated_in_region) *
             sizeof(unsigned long)));
    size_of_allocated_in_region = size_of_mem_region_table;
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
}  /* ensure_mem_region_table_space */


void init_memory_region_without_initial_allocation
                        (a_memory_region_number region_number)
/*
Initialize the indicated region number.
*/
{
  ensure_mem_region_table_space(region_number);
  mem_region_table[region_number] = NULL;
  /* Keep track of the highest memory region number used. */
  if (region_number > highest_used_region_number) {
    highest_used_region_number = region_number;
  }  /* if */
}  /* init_memory_region_without_initial_allocation */


void init_memory_region(a_memory_region_number region_number,
                        sizeof_t               min_size)
/*
Initialize the indicated region number.  Allocate at least min_size bytes
as the initial allocation for the region.  In general, new_memory_region
should be called instead.  init_memory_region is called directly for the
special "front end" memory region.
*/
{
  init_memory_region_without_initial_allocation(region_number);
  /* Allocate the initial memory block. */
  (void)alloc_mem_block(region_number, min_size, (char *)NULL,
                        /*small_extension=*/FALSE);
}  /* init_memory_region */


a_memory_region_number new_memory_region(void)
/*
Create a new memory region and return its memory region number.
A new region is used for each function's executable code and data.
*/
{
  a_memory_region_number region_number;

  db_enter(5, "new_memory_region");
  if (highest_used_region_number == MAX_MEMORY_REGION_NUMBER) {
    /* Too many regions (extremely unlikely). */
    catastrophe(ec_program_too_large);
  }  /* if */
  region_number = ++highest_used_region_number;
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 2) {
    fprintf(f_debug, "New memory region, number %ld.\n", (long)region_number);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

  init_memory_region(region_number, (sizeof_t)0);

  db_exit();
  return (region_number);
}  /* new_memory_region */


char *alloc_general(sizeof_t size)
/*
Allocate and return "size" bytes of general storage.  This differs from
alloc_fe in that the storage will last through execution of the back end if the
back end is executed in the same program.  Any memory that is not freed via
free_general will be reclaimed upon mem_manage_wrapup.
*/
{
  char *ptr = malloc_with_check(size);

#if TRACK_GENERAL_ALLOCATION
  memory_allocation_map->map(ptr, size);
#endif /* TRACK_GENERAL_ALLOCATION */
#if DEBUG
/* GCOVR_EXCL_START */
  total_general_mem_allocated += (unsigned long)size;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  return ptr;
}  /* alloc_general */


void free_general(a_void_ptr          ptr,
                  ARG_UNUSED sizeof_t size)
/*
Free a tracked block of memory to general storage.
*/
{
  if (ptr == NULL) {
    /* If this assertion fails a null block of memory was given with a non-zero
       size.  Thus, either the caller got the pointer to the block of memory or
       the size wrong. */
    check_assertion(size == 0);
  } else {
#if TRACK_GENERAL_ALLOCATION
    /* Check to ensure the specified amount to free matches the allocated
       amount. */
    check_assertion(memory_allocation_map->get(ptr) == size);
#endif /* TRACK_GENERAL_ALLOCATION */
    /* Update internal memory tracking. */
#if DEBUG
/* GCOVR_EXCL_START */
    total_general_mem_allocated -= (unsigned long)size;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
#if TRACK_GENERAL_ALLOCATION
    memory_allocation_map->unmap(ptr);
#endif /* TRACK_GENERAL_ALLOCATION */
#if EXPENSIVE_CHECKING
    if (resizable_memory_allocations->contains(ptr)) {
      resizable_memory_allocations->remove(ptr);
    }  /* if */
#endif /* EXPENSIVE_CHECKING */
    /* Release the memory. */
    free(ptr);
  }  /* if */
}  /* free_general */


char *alloc_resizable_buffer(sizeof_t size)
/*
Allocate "size" bytes of storage that can be resized later using
realloc_general.  Because these buffers can be resized, they can't be
allocated in a memory region.  A list of these allocations is maintained so
that the memory can be freed when the front end is reset.
*/
{
  char *ptr = alloc_general(size);

#if EXPENSIVE_CHECKING
  resizable_memory_allocations->add(ptr);
#endif /* EXPENSIVE_CHECKING */
  return ptr;
}  /* alloc_resizable_buffer */


char *realloc_buffer(char     *old_ptr,
                     sizeof_t old_size,
                     sizeof_t new_size)
/*
Reallocate the area pointed to by old_ptr, which currently has size old_size,
so that it will have size new_size.  Return a pointer to the new area.
The old space must have been allocated in general storage by
alloc_resizable_buffer or realloc_buffer.  If old_ptr == NULL, this routine
acts like alloc_resizable_buffer.
*/
{
  char *ptr;

  if (old_ptr == NULL) {
    ptr = alloc_resizable_buffer(new_size);
  } else {
#if EXPENSIVE_CHECKING
    /* If this assertion fails, the given pointer was not allocated as a
       resizable buffer. */
    check_assertion(resizable_memory_allocations->contains(old_ptr));
#endif /* EXPENSIVE_CHECKING */
    ptr = realloc_with_check(old_ptr, old_size, new_size);
#if TRACK_GENERAL_ALLOCATION
    /* Update the internal bookkeeping. */
    memory_allocation_map->unmap(old_ptr);
    memory_allocation_map->map(ptr, new_size);
#endif /* TRACK_GENERAL_ALLOCATION */
#if EXPENSIVE_CHECKING
    resizable_memory_allocations->remove(old_ptr);
    resizable_memory_allocations->add(ptr);
#endif /* EXPENSIVE_CHECKING */
  }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
  total_general_mem_allocated -= (unsigned long)old_size;
  total_general_mem_allocated += (unsigned long)new_size;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  return ptr;
}  /* realloc_buffer */



void free_memory_region(a_memory_region_number region_number)
/*
Free the space for the entire memory region indicated by region_number.
This is presumably being done because the associated information is no longer
needed (e.g., it has been written out to the IL file).
*/
{
  a_mem_block_header_ptr hdr, next_hdr;

  db_enter(5, "free_memory_region");
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 5) {
    fprintf(f_debug, "free_memory_region: region %lu, size = %lu\n",
                     (unsigned long)(unsigned)region_number,
                     (unsigned long)allocated_in_region[region_number]);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
#if !STANDALONE_UTILITY_PROGRAM && EXPENSIVE_CHECKING
  /* Let the symbol table know the given memory region is being freed. */
  symbol_table_memory_region_wrap_up(region_number);
#endif /* !STANDALONE_UTILITY_PROGRAM && EXPENSIVE_CHECKING */
  /* Traverse the list of blocks and free each one. */
  for (hdr = mem_region_table[region_number]; hdr != NULL;) {
    next_hdr = hdr->next;
    free_mem_block(hdr);
    hdr = next_hdr;
  }  /* for */
  mem_region_table[region_number] = NULL;
  il_header.region_scope_entry[region_number] = NULL;
#if DEBUG
/* GCOVR_EXCL_START */
  allocated_in_region[region_number] = 0;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  if (region_number == file_scope_region_number) {
    /* The storage for local constants has been freed. */
    available_local_constants = NULL;
  }  /* if */
  db_exit();
}  /* free_memory_region */


static void unlink_non_malloc_blocks(a_mem_block_header_ptr	*block_list)
/*
Go through the list of memory blocks in block_list and remove any entries
that do not represent malloc allocations.
*/
{
  a_mem_block_header_ptr hdr;
  a_mem_block_header_ptr next_hdr;
  a_mem_block_header_ptr prev_hdr;

  for (prev_hdr = NULL, hdr = *block_list; hdr != NULL; hdr = next_hdr) {
    next_hdr = hdr->next;
    if (hdr->malloc_size == 0) {
      if (prev_hdr == NULL) {
        *block_list = next_hdr;
      } else {
        prev_hdr->next = next_hdr;
      }  /* if */
    } else {
      prev_hdr = hdr;
    }  /* if */
  }  /* for */
}  /* unlink_non_malloc_blocks */


static void free_mem_blocks(a_mem_block_header_ptr	*block_list)
/*
Go through the list of memory blocks in block_list and free the entire
block of any entries that represent actual malloc allocations.  Clear
the block_list when done.
*/
{
  a_mem_block_header_ptr hdr;
  a_mem_block_header_ptr next_hdr;

  for (hdr = *block_list; hdr != NULL; hdr = next_hdr) {
    next_hdr = hdr->next;
    /* There should only be malloc entries on the list when this routine
       is called.  Note that in other places in this file that call
       free_complete_block the caller also checks that the malloc_size is the
       same as the space used in the block.  That test is not needed here
       because that test is used to prevent the freeing of split blocks.  But
       here we know that the other parts of any split blocks have already
       been removed from any lists so that the complete blocks can be freed. */
    check_assertion(hdr->malloc_size > 0);
    free_complete_block(hdr);
  }  /* for */
  *block_list = NULL;
}  /* free_mem_blocks */


static void free_mem_blocks_for_region(a_memory_region_number	region_number)
/*
Go through the list of memory blocks in region_number and free the entire
block of any entries that represent actual malloc allocations.  This is
called by free_all_memory_regions to quickly free the all memory.
*/
{
  free_mem_blocks(&mem_region_table[region_number]);
  il_header.region_scope_entry[region_number] = NULL;
}  /* free_mem_blocks_for_region */


void free_all_memory_regions(void)
/*
Free all of the memory regions that have been used, including the front
end memory region.
*/
{
  a_memory_region_number region_number;

  if (okay_to_free_mem_blocks) {
    /* Because we know that we are freeing all of the memory, this routine
       simply frees the blocks that represent actual malloc allocations.
       First we unlink blocks that don't represent malloc allocations because
       they might be part of a block for which the malloc was done in a
       different memory region. */
    for (region_number = highest_used_region_number;
         region_number != NULL_region_number;
         region_number--) {
      unlink_non_malloc_blocks(&mem_region_table[region_number]);
    }  /* for */
    unlink_non_malloc_blocks(&mem_region_table[NULL_region_number]);
    unlink_non_malloc_blocks(&reusable_blocks_list);
    /* The only thing left on the block lists at this point will be actual
       allocations. */
    for (region_number = highest_used_region_number;
         region_number != NULL_region_number;
         region_number--) {
      free_mem_blocks_for_region(region_number);
    }  /* for */
    /* Free the front end memory region. */
    free_mem_blocks_for_region(NULL_region_number);
    /* There could be malloc blocks on the reusable blocks list, so free any
       such blocks. */
    free_mem_blocks(&reusable_blocks_list);
  } else {
    /* The memory can't be freed -- just return it to the reusable list
       via the normal free_memory_region mechanism. */
    for (region_number = highest_used_region_number;
         region_number != NULL_region_number;
         region_number--) {
      free_memory_region(region_number);
    }  /* for */
    /* Free the front end memory region. */
    free_memory_region(NULL_region_number);
#if !STANDALONE_UTILITY_PROGRAM
#if USE_MMAP_FOR_MEMORY_REGIONS
    free_mapped_mem_blocks();
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
#endif /* !STANDALONE_UTILITY_PROGRAM */
    reusable_blocks_list = NULL;
  }  /* if */
}  /* free_all_memory_regions */


void trim_memory_region(a_memory_region_number region_number)
/*
Trim the current (last) block of the indicated memory region to free
any unused space.
*/
{
  trim_mem_block(mem_region_table[region_number]);
}  /* trim_memory_region */

#if !STANDALONE_UTILITY_PROGRAM
#if IL_SHOULD_BE_WRITTEN_TO_FILE

static a_boolean memory_region_should_be_kept_for_routine(a_routine_ptr	rout,
							  a_scope_ptr	scope)
/*
Return TRUE if the memory region containing rout should be kept in
memory because of the properties of rout.  scope is the scope of rout
(not the scope of the primary routine of the memory region).
*/
{
  a_boolean	keep_memory = FALSE;
#if DO_IL_LOWERING
  a_type_ptr	closure_class;
#endif /* DO_IL_LOWERING */

  check_assertion(rout != NULL);
  if (keep_function_body_for_possible_inlining(rout)) {
    /* Keep the region for an inline function so it can be used to
       do inlining. */
    keep_memory = TRUE;
  } else if (rout->contains_local_class_type) {
    /* Local class types may have generated special members that need to be
       defined because of later instantiations.  Those definitions may need
       access to the enclosing function scope. */
    keep_memory = TRUE;
#if DO_IL_LOWERING
  } else if (parent_is_lambda_closure(rout, &closure_class) &&
             class_type_supp(closure_class)->has_lambda_conversion_function) {
    /* The definition of the lambda entry point is done when the file scope
       is lowered.  Keep the memory so that the region can be used for
       that definition. */
    keep_memory = TRUE;
#endif /* DO_IL_LOWERING */
  } else if (!scope->function_body_processing_finished) {
    /* If we haven't finished processing the function body, don't write
       it out.  In particular, if IL lowering has not been done yet,
       do not write out the body. */
    keep_memory = TRUE;
#if MAINTAIN_NEEDED_FLAGS
  } else if (!rout->keep_definition_in_il || !rout->definition_needed) {
    /* This memory region so far looks as if it's unneeded.  Hold on
       to it for now.  If we make it to the end of the compilation with
       the memory region still unneeded, we will have the option of
       freeing it at that point. */
    keep_memory = TRUE;
#endif /* MAINTAIN_NEEDED_FLAGS */
  }  /* if */
  return keep_memory;
}  /* memory_region_should_be_kept_for_routine */

#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
#endif /* !STANDALONE_UTILITY_PROGRAM */

void check_for_done_with_memory_region(a_memory_region_number region_number)
/*
We're done creating the indicated memory region in the front end.  Determine
whether the front end has any further use for it and/or whether it has to be
kept around in order possibly to be written to a PCH file.  If either is
TRUE, the memory region can be trimmed (since it in any case is not going to
grow any larger), but it must be kept around.  If not, it may be possible to
dispose of it, depending on whether the IL is passed to the back end in
memory or with an IL file.
*/
{
  a_boolean      keep_memory;
#if !STANDALONE_UTILITY_PROGRAM
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  a_scope_ptr    scope;
  a_routine_ptr  rout;
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
#endif /* !STANDALONE_UTILITY_PROGRAM */

  db_enter(5, "check_for_done_with_memory_region");
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 1) {
    fprintf(f_debug,
            "check_for_done_with_memory_region: region %lu, size = %lu\n",
            (unsigned long)(unsigned)region_number,
            (unsigned long)allocated_in_region[region_number]);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
#if STANDALONE_UTILITY_PROGRAM
  /* In a standalone program the memory is always freed. */
  keep_memory = FALSE;
#else /* !STANDALONE_UTILITY_PROGRAM */
#if !IL_SHOULD_BE_WRITTEN_TO_FILE
  /* The IL is passed to the back end in memory, so it is always kept. */
  keep_memory = TRUE;
#else /* IL_SHOULD_BE_WRITTEN_TO_FILE */
  /* Communication with the back end is via a file.  The memory
     is freed after it's been written to the IL file. */
  scope = il_header.region_scope_entry[region_number];
  check_assertion(scope != NULL);
  rout = (scope->kind == (a_scope_kind)sck_function) ?
                                      scope->variant.routine.ptr : NULL;
  keep_memory = FALSE;
#if !FREE_MEMORY_REGIONS_EARLY
  if (region_number != file_scope_region_number) {
    /* Function scope memory regions are not freed early: Keep the memory
       until all memory regions are freed. */
    keep_memory = TRUE;
  } else
#endif /* !FREE_MEMORY_REGIONS_EARLY */
  /* Do not insert code here. */
  if (may_be_building_new_pch()) {
    /* We are still considering whether to build a PCH file, so keep this
       region around so we can use it in generating the PCH file.
       check_for_done_with_memory_region will be called again once we've
       written the PCH or decided not to write one.  We can still trim the
       unused portion of the memory block at this time, though. */
    keep_memory = TRUE;
  } else if (in_secondary_trans_unit(scope)) {
    /* In a secondary translation unit, hold on to all memory regions
       for further processing.  Note that functions that are supposed
       to be discarded (such as non-templates in a translation unit
       compiled only for its exported templates) will have been thrown
       away before this routine is called. */
    keep_memory = TRUE;
  } else if (scope->do_not_free_memory_region) {
    /* The memory region might be needed later (e.g., for a generic
       lambda instantiation).  Do not free it. */
    keep_memory = TRUE;
  } else if (skip_il_read) {
    /* For debugging purposes, keep the memory region around. */
    keep_memory = TRUE;
  }  /* if */
  if (!keep_memory && rout != NULL) {
    /* So far we don't need to keep the memory.  Go though the list of
       function scopes to see if the memory region should be kept for
       any of them. */
    a_scope_ptr	sp;
    for (sp = scope; sp != NULL; sp = sp->next) {
      a_routine_ptr	rp = sp->variant.routine.ptr;
      check_assertion(rp != NULL);
      if (memory_region_should_be_kept_for_routine(rp, sp)) {
        keep_memory = TRUE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
  if (rout != NULL && debug_level >= 2) {
    fprintf(f_debug, "check_for_done_with_memory_region: ");
    fprintf(f_debug, "%s memory region for ",
            keep_memory ? "keeping" : "writing/freeing");
    db_name(&rout->source_corresp);
    fprintf(f_debug, "\n");
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  if (!keep_memory) {
    /* Write the region to the file and free it. */
    check_assertion(!in_secondary_trans_unit(scope));
    write_memory_region(region_number);
  }  /* if */
#endif /* !IL_SHOULD_BE_WRITTEN_TO_FILE */
#endif /* !STANDALONE_UTILITY_PROGRAM */
  if (keep_memory) { /*lint !e774*/
    /* Keep the memory for the region.  Trim the region to reclaim unused
       storage at the end of the last block.  Unused storage at the ends
       of blocks other than the last was previously reclaimed. */
    trim_memory_region(region_number);
  } else {
    /* Free the memory for the region. */
    /* coverity[dead_error_line] */
    free_memory_region(region_number);
  }  /* if */
  db_exit();
}  /* check_for_done_with_memory_region */

#if !STANDALONE_UTILITY_PROGRAM

void check_for_done_with_all_function_memory_regions(void)
/*
This routine is called at the end of the compilation to write out
function memory regions that were not previously written out.
Such routines remain in the IL tree (a) if unneeded entities are not
being eliminated, (b) if they are marked with keep_definition_in_il but
not definition_needed, (c) if they are inline, or (d) if they are
part of secondary translation units (perhaps).
*/
{
  db_enter(5, "check_for_done_with_all_function_memory_regions");
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  {
  /* Loop through the memory regions.  Skip the front end and file scope
     memory regions. */
  a_memory_region_number  n = FILE_SCOPE_REGION_NUMBER + 1;
  for (; n <= highest_used_region_number; ++n) {
    if (mem_region_table[n] == NULL) {
      /* This memory has already been freed. */
    } else {
      a_scope_ptr sp = il_header.region_scope_entry[n];
      a_boolean   from_secondary_trans_unit = in_secondary_trans_unit(sp);
      /* Skip the file-scope memory regions of secondary translation units. */
      if (!from_secondary_trans_unit || sp->kind != (a_scope_kind)sck_file) {
        check_assertion(sp->kind == (a_scope_kind)sck_function);

#if CHECKING || DEBUG
        a_routine_ptr rout = sp->variant.routine.ptr;
#endif /* CHECKING || DEBUG */
        check_assertion_str2(!rout->is_trivial_default_constructor ||
                             rout->is_defaulted,
                           "check_for_done_with_all_function_memory_regions:",
                           "trivial default constructor");
#if DEBUG
/* GCOVR_EXCL_START */
        if (debug_level >= 2) {
          fprintf(f_debug,
                  "check_for_done_with_all_function_memory_regions: ");
          fprintf(f_debug, "%s memory region for ",
                           !from_secondary_trans_unit ?
                                                "writing/freeing" : "freeing");
          db_name(&rout->source_corresp);
          fprintf(f_debug, "\n");
        }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
        if (!from_secondary_trans_unit) write_memory_region(n);
        if (!skip_il_read) {
          free_memory_region(n);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  }
#endif /* !IL_SHOULD_BE_WRITTEN_TO_FILE */
  db_exit();
}  /* check_for_done_with_all_function_memory_regions */

#endif /* !STANDALONE_UTILITY_PROGRAM */

#if DEBUG
/* GCOVR_EXCL_START */
#if !STANDALONE_UTILITY_PROGRAM
void show_mem_manage_space_used(unsigned long total_accounted_for)
/*
Display the total amounts of memory used, for debug purposes.
total_accounted_for is the amount of space that is accounted for by
usage counts in other files.
*/
{
  unsigned long          num, size, total, grand_total = 0;
  a_memory_region_number region_number;
  a_mem_block_header_ptr hdr;
  unsigned long          total_used, total_unallocated = 0;
  unsigned long		 total_in_freed_blocks = 0;

  db_space_used_header("Memory management table use:");
  db_space_used("text buffers", num_text_buffers_allocated, a_text_buffer);
  db_space_used_total();
  fprintf(f_debug, "\nAllocated space in all categories:\n");
  fprintf(f_debug, "%25s %8s %8s %8lu\n", "Total of above", "", "",
          total_accounted_for);
  fprintf(f_debug, "%25s %8s %8s %8lu\n", "Skipped for alignment", "", "",
          num_alignment_bytes_allocated);
#if USE_MMAP_FOR_MEMORY_REGIONS
  fprintf(f_debug, "%25s %8s %8s %8lu\n", "File mapped memory", "", "",
          num_mapped_bytes_allocated);
  fprintf(f_debug, "%25s %8s %8s %8lu (included in previous line)\n",
          "Mapped from PCH", "", "", num_mapped_bytes_from_pch);
  fprintf(f_debug, "%25s %8s %8s %8lu\n",
          "Mapped IL file size", "", "", (unsigned long)mmap_file_offset);
#else /* !USE_MMAP_FOR_MEMORY_REGIONS */
  if (precompiled_header_processing_required) {
    fprintf(f_debug, "%25s %8s %8s %8lu\n",
            "Preallocated PCH memory", "", "", (unsigned long)pch_mem_size);
  }  /* if */
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
  total_accounted_for += num_alignment_bytes_allocated;
  /* total_mem_used only counts space allocated in memory regions, so
     it does not include what's in total_general_mem_allocated. */
  total_used = total_mem_used + total_general_mem_allocated;
  fprintf(f_debug, "%25s %8s %8s %8lu\n", "Not listed", "", "",
                   total_used - total_accounted_for);
  fprintf(f_debug, "%25s %8s %8s %8lu\n", "Total used", "", "",
                   total_used);
  /* Size the unallocated parts of memory blocks. */
  for (region_number = NULL_region_number;
       region_number <= highest_used_region_number;
       region_number++) {
    for (hdr = mem_region_table[region_number]; hdr != NULL; hdr = hdr->next) {
      total_unallocated += (unsigned long)(hdr->after_end_of_block -
                                           hdr->next_avail_in_block);
    }  /* for */
  }  /* for */
  fprintf(f_debug, "%25s %8s %8s %8lu\n", "Avail in used mem blocks", "", "",
                   total_unallocated);

  /* Size the memory blocks on the available list. */
  unsigned long num_reusable_blocks = 0;
  unsigned long num_reusable_block_bytes = 0;
  for (hdr = reusable_blocks_list; hdr != NULL; hdr = hdr->next) {
    num_reusable_block_bytes += (unsigned long)(hdr->after_end_of_block -
                                                hdr->start_of_block);
    ++num_reusable_blocks;
  }  /* for */
  total_in_freed_blocks += num_reusable_block_bytes;
  fprintf(f_debug, "%25s %26lu\n", "Avail in freed mem blocks",
          total_in_freed_blocks);
  /* Print useful metrics about memory management. */
  fprintf(f_debug, "\nMemory management metrics:\n");
  fprintf(f_debug, "%25s %26lu\n", "Calls to malloc", num_malloc_calls);
  fprintf(f_debug, "%25s %26lu\n", "Bytes allocated", num_bytes_allocated);
  fprintf(f_debug, "%25s %26lu\n", "Calls to free", num_free_calls);
  fprintf(f_debug, "%25s %26lu\n", "Bytes freed", num_bytes_freed);
  fprintf(f_debug, "%25s %26lu\n", "Memory reuse attempts",
          num_reclaim_attempts);
  fprintf(f_debug, "%25s %26lu\n", "Memory reuse successes",
          num_reclaim_successes);
  fprintf(f_debug, "%25s %26.2f\n", "Avg reuse traversals",
          (double)num_reclaim_traversals / (double)num_reclaim_attempts);
  fprintf(f_debug, "%25s %26lu\n", "Num reusable blocks", num_reusable_blocks);
  fprintf(f_debug, "%25s %26lu\n", "Avg reusable block size",
          num_reusable_block_bytes / max_val(num_reusable_blocks, 1lu));
  fprintf(f_debug, "%25s %26lu\n", "Post one-time init size",
          mem_in_use_after_one_time_init);
  fprintf(f_debug, "%25s %26lu\n", "Max post trans init alloc",
          max_mem_in_use_after_tu_init);
  fprintf(f_debug, "%25s %26lu\n", "Max mem alloc", max_mem_allocated);
}  /* show_mem_manage_space_used */
#endif /* !STANDALONE_UTILITY_PROGRAM */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */


a_text_buffer_ptr alloc_text_buffer(sizeof_t	allocation_increment)
/*
Allocate and initialize a text buffer.  allocation_increment is the
size of the initial memory allocation and any additional allocations.
Note that text buffers are allocated in general memory, because the buffers
they point to are allocated there.
*/
{
  a_text_buffer_ptr tbp = alloc_general_of_type(a_text_buffer);

  tbp->allocated_size = allocation_increment;
  tbp->allocation_increment = allocation_increment;
  tbp->size = 0;
  tbp->buffer = alloc_resizable_buffer(allocation_increment);
#if DEBUG
/* GCOVR_EXCL_START */
  num_text_buffers_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  return tbp;
}  /* alloc_text_buffer */


void reset_text_buffer(a_text_buffer_ptr	buffer)
/*
Reset the specified buffer to indicate that it is empty.
*/
{
  buffer->size = 0;
}  /* reset_text_buffer */


void expand_text_buffer(a_text_buffer_ptr	buffer,
			sizeof_t		length)
/*
Expand the specified text buffer so that it is large enough to hold
"length" characters.
*/
{
  if (length > buffer->allocated_size) {
    /* There is not enough room for the new characters.  Reallocate the
       buffer. */
    sizeof_t	new_size;
    /* Compute a new size that is a multiple of the allocation increment. */
    new_size = ((length + buffer->allocation_increment - 1) /
                buffer->allocation_increment) * buffer->allocation_increment;
    buffer->buffer = (char *)realloc_buffer(buffer->buffer,
                                            buffer->allocated_size,
                                            new_size);
    /* Each time the buffer is reallocated, double the allocation increment. */
    buffer->allocation_increment *= 2;
    buffer->allocated_size = new_size;
  }  /* if */
}  /* expand_text_buffer */


void set_buffer_position(a_text_buffer_ptr	buffer,
			 char			*pos)
/*
Update buffer so that any additional characters that are appended
will be placed starting at the location specified by pos.
*/
{
  /* Make sure pos is a valid location in the buffer. */
  check_assertion(buffer->buffer <= pos &&
                  &buffer->buffer[buffer->allocated_size - 1] >= pos);
  buffer->size = (sizeof_t)(pos - buffer->buffer);
}  /* set_buffer_position */


void add_to_text_buffer(a_text_buffer_ptr	buffer,
			a_const_char		*string,
			sizeof_t		length)
/*
Add "length" characters of "string" to the text buffer pointed to "buffer".
*/
{
  sizeof_t	new_size;

  new_size = buffer->size + length;
  ensure_text_buffer_space(buffer, new_size);
  /* Copy the characters into the buffer. */
  memcpy(&buffer->buffer[buffer->size], string, size_t_arg(length));
  buffer->size = new_size;
}  /* add_to_text_buffer */


void f_add_string_to_text_buffer(a_text_buffer_ptr	buffer,
			         a_const_char		*string)
/*
Add the specified string to a text buffer.  This version can be called
(instead of the macro) when the string argument has side-effects.
*/
{
  sizeof_t	length;

  length = strlen(string);
  add_to_text_buffer(buffer, string, length);
}  /* f_add_string_to_text_buffer */


void truncate_text_buffer_to(a_text_buffer_ptr buffer,
                             sizeof_t          length)
/*
Truncate the given text buffer to the given length.
*/
{
  check_assertion(buffer->size >= length);
  buffer->size = length;
}  /* truncate_text_buffer_to */


void remove_null_terminator_from_text_buffer(a_text_buffer_ptr	buffer)
/*
If the last character of the text buffer is a null terminator, update
the buffer size to remove the terminator from the string (so that other
text can be added).
*/
{
  if (buffer->size > 0 && buffer->buffer[buffer->size - 1] == '\0') {
    buffer->size--;
  }  /* if */
}  /* remove_null_terminator_from_text_buffer */

#if EXPENSIVE_CHECKING

static a_boolean is_in_memory_region(char                   *ptr,
                                     sizeof_t               size,
                                     a_memory_region_number region_number)
/*
Return TRUE if the given pointer for an object of the given size is in one of
the memory region blocks; otherwise, return FALSE.
*/
{
  a_boolean result = FALSE;

  for (a_mem_block_header_ptr hdr = mem_region_table[region_number];
       hdr != NULL; hdr = hdr->next) {
    if (hdr->start_of_block <= ptr &&
        (ptr + size) <= hdr->next_avail_in_block) {
      result = TRUE;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* is_in_memory_region */

#endif /* EXPENSIVE_CHECKING */

namespace detail {

void free_fe_huge(a_void_ptr ptr,
                  sizeof_t   size)
/*
Free a large (i.e., with a size equal to or greater than HUGE_FE_MEM_THRESHOLD)
front end allocation (i.e., allocated by alloc_fe) starting at the given
address of the given size.

The underlying memory block is returned to the reusable block lists for future
allocations in any memory region.
*/
{
  check_assertion(size >= HUGE_FE_MEM_THRESHOLD);
  a_mem_block_header *hdr = mem_region_table[FRONT_END_REGION_NUMBER];
  a_mem_block_header **prev_next =
                             &mem_region_table[FRONT_END_REGION_NUMBER];
#if CHECKING
  a_boolean          block_found = FALSE;
#endif /* CHECKING */

  /* Traverse the list of blocks and free each one. */
  for (; hdr != NULL; hdr = hdr->next) {
    if (hdr->start_of_block == ptr) {
      a_mem_block_header_ptr next_hdr = hdr->next;

      /* Free the block. */
      free_mem_block(hdr);
#if CHECKING
      block_found = TRUE;
#endif /* CHECKING */
      /* Unlink this memory block. */
      *prev_next = next_hdr;
      break;
    }  /* if */
    prev_next = &hdr->next;
  }  /* for */
  check_assertion(block_found);
}  /* free_fe_huge */


void free_fe_normal(a_void_ptr ptr,
                    sizeof_t   size)
/*
Free the block of memory (allocated by alloc_fe) pointed to by ptr of the
specified size.  The block is recorded for possible reuse later by alloc_fe.
*/
{
  /* All allocations from alloc_fe are at least 1 byte. */
  size = max_val(size, (sizeof_t)1);
#if EXPENSIVE_CHECKING
  /* Check to ensure that the given pointer with the given size could
     conceivably fit within the memory region. */
  check_assertion(is_in_memory_region((char*)ptr, size,
                                      FRONT_END_REGION_NUMBER));
#endif /* EXPENSIVE_CHECKING */
  auto freed_blocks = detail::freed_fe_map->get_or_alloc(size);
  freed_blocks->push_back(ptr);
}  /* free_fe_normal */

}  /* namespace detail */
#if DEBUG
/* GCOVR_EXCL_START */

void db_text_buffer(a_const_char	*prefix,
		    a_text_buffer_ptr	buf)
/*
Display the contents of a text buffer, for debugging purposes.  "prefix"
is a string used to label the output, and may be NULL.  "buf" is the buffer
to be displayed.
*/
{
  unsigned int i;
  if (prefix != NULL) fprintf(f_debug, "%s: ", prefix);
  for (i = 0; i < buf->size; ++i) {
   char	c = buf->buffer[i];
   if (c == 0) {
     fprintf(f_debug, "\\0");
   } else {
     fprintf(f_debug, "%c", c);
   }  /* if */
  }  /* for */
  fprintf(f_debug, "\n");
}  /* db_text_buffer */


void db_prefix(void  *entry_ptr)
/*
Display the IL entry prefix of the given IL entry.
*/
{
  char  *entry = (char*)entry_ptr;

  if (entry == NULL) {
    fprintf(f_debug, "NULL pointer\n");
  } else {
    an_il_entry_prefix_ptr prefix =
                  (an_il_entry_prefix_ptr)(entry - SPACE_FOR_IL_ENTRY_PREFIX);

    if (prefix->file_scope) {
      fprintf(f_debug, "file_scope ");
    }  /* if */
    if (prefix->secondary_trans_unit) {
      fprintf(f_debug, "secondary_trans_unit ");
    }  /* if */
    if (prefix->il_walk_flag) {
      fprintf(f_debug, "il_walk_flag ");
    }  /* if */
    if (prefix->il_lowering_flag) {
      fprintf(f_debug, "lowering_flag ");
    }  /* if */
#if MAINTAIN_NEEDED_FLAGS
    if (prefix->keep_in_il) {
      fprintf(f_debug, "keep_in_il ");
    }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
#if IL_SHOULD_BE_WRITTEN_TO_FILE && ALTERNATE_IL_FILE_FORMAT
    fprintf(f_debug, "(entry_number = %lu) ",
            (unsigned long)prefix->entry_number);
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE && ALTERNATE_IL_FILE_FORMAT */
#if MAINTAIN_ALLOCATION_SEQUENCE_NUMBER
    fprintf(f_debug, "(alloc_seq_number = %lu) ",
            (unsigned long)prefix->alloc_seq_number);
#endif /* MAINTAIN_ALLOCATION_SEQUENCE_NUMBER */
#if EXPENSIVE_CHECKING
    fprintf(f_debug,
            "\nmemory regions: translation unit = %lu, owned by = %lu ",
            (unsigned long)prefix->file_scope_region_number,
            (unsigned long)prefix->region_number);
#endif /* EXPENSIVE_CHECKING */
#if ORPHAN_PROCESSING_NEEDED
    if (prefix->file_scope) {
      fprintf(f_debug, "\norphan ptr = %p ",
              (a_void_ptr)fs_orphan_pointer_of(entry));
    }  /* if */
#endif /* ORPHAN_PROCESSING_NEEDED */
    fputs("\n", f_debug);
  }  /* if */
}  /* db_prefix */


an_il_entry_prefix_ptr db_prefix_ptr(char  *entry)
/*
Return a pointer to the prefix of the given entry.  (It is sometimes useful to
call this from a symbolic debugger; e.g., when setting a watchpoint for a
prefix field.)
*/
{
  return &il_entry_prefix_of(entry);
}  /* db_prefix_ptr */

/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

#if EXPENSIVE_CHECKING

extern an_il_entry_prefix_ptr expensive_il_entry_prefix_of(char *ptr)
/*
Return a pointer to the prefix of the given IL entry, but check to make sure
the IL entry actually has a prefix (i.e., that it's not mistakenly allocated
on the stack).
*/
{
  an_il_entry_prefix_ptr result =
                     (an_il_entry_prefix_ptr)(ptr - SPACE_FOR_IL_ENTRY_PREFIX);
  check_assertion(result->magic_number == IL_ENTRY_MAGIC_NUMBER);
  return result;
}  /* expensive_il_entry_prefix_of */

#endif /* EXPENSIVE_CHECKING */

void mem_manage_one_time_init(void)
/*
Do one-time initialization of variables related to the mem_manage routines.
*/
{
  okay_to_free_mem_blocks = TRUE;
#if !STANDALONE_UTILITY_PROGRAM
  /* Save variables from mem_manage.h and mem_manage.c that are needed for
     precompiled headers */
  if (precompiled_header_processing_required) {
    STATIC_THREAD a_pch_saved_variable saved_vars[] = {
    /* highest_used_region_number is saved directly in the PCH file, and
       therefore doesn't need to be saved here. */
      pch_saved_var_array_elem(reusable_blocks_list),
#if DEBUG
/* GCOVR_EXCL_START */
      pch_saved_var_array_elem(total_mem_used),
      pch_saved_var_array_elem(num_alignment_bytes_allocated),
      pch_saved_var_array_elem(num_text_buffers_allocated),
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
#if USE_MMAP_FOR_MEMORY_REGIONS
  /* When doing precompiled header processing, we allocate memory blocks
     in mapped memory, which cannot be freed. */
  okay_to_free_mem_blocks = !precompiled_header_processing_required;
  mem_alloc_history = NULL;
  mmap_initialized = FALSE;
  mmap_size_allocated = 0;
  mmap_file_offset = 0;
#else /* !USE_MMAP_FOR_MEMORY_REGIONS */
  exhausted_preallocated_memory = FALSE;
  large_mem_block_needed = FALSE;
  total_mem_blocks_allocated = 0;
  additional_allocation_needed = FALSE;
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
  /* Register variables that must be saved and restored when switching
     between translation units. */
  register_trans_unit_variable(file_scope_region_number);
#endif /* !STANDALONE_UTILITY_PROGRAM */
#if DEBUG
/* GCOVR_EXCL_START */
  allocated_in_region = NULL;
  size_of_allocated_in_region = 0;
  num_text_buffers_allocated = 0;
#if USE_MMAP_FOR_MEMORY_REGIONS
  num_mapped_bytes_allocated = 0;
  num_mapped_bytes_from_pch = 0;
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
#ifdef USING_PURIFY
  /* Call the Purify runtime routine to determine whether this executable
     has been processed using Purify. */
  purify_is_active = purify_is_running();
#endif /* USING_PURIFY */
  reusable_blocks_list = NULL;
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  index_for_il_file = NULL;
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
  il_header.region_scope_entry = NULL;
  il_header.function_def_table = NULL;
}  /* mem_manage_one_time_init */

#if DEBUG
/* GCOVR_EXCL_START */

void mem_manage_one_time_init_done()
/*
This function is called after one-time initialization is complete to record the
amount of memory that was in use after the initialization phase.
*/
{
  mem_in_use_after_one_time_init = (num_bytes_allocated - num_bytes_freed);
}  /* mem_manage_one_time_init_done */

/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

void mem_manage_trans_unit_init(void)
/*
Initialize static variables related to the mem_manage routines that
must be initialized for each translation unit.
*/
{
  if (!is_primary_translation_unit) {
    /* For secondary translation units, continue numbering memory regions
       from where we left off.  The file scope memory region is the next
       available region. */
    file_scope_region_number = highest_used_region_number+1;
    init_memory_region(file_scope_region_number, (sizeof_t)0);
  }  /* if */
  /* Update the translation unit's file scope region number to make sure the
     correct translation unit region number is immediately visible on the
     translation unit. */
  curr_translation_unit->file_scope_region_number = file_scope_region_number;
}  /* mem_manage_trans_unit_init */

#if DEBUG
/* GCOVR_EXCL_START */

void mem_manage_trans_unit_init_done()
/*
This function is called after translation unit initialization is complete to
record the amount of memory that was in use after the initialization phase.

If multiple translation units are compiled, this is the largest value of
any translation unit.
*/
{
  size_t this_tu_init = (num_bytes_allocated - num_bytes_freed);

  if (this_tu_init > max_mem_in_use_after_tu_init) {
    max_mem_in_use_after_tu_init =
                        (decltype(max_mem_in_use_after_tu_init))(this_tu_init);
  }  /* if */
}  /* mem_manage_trans_unit_init_done */

/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

void mem_manage_early_init(void)
/*
One time initialization that must take place early on in the front end.
This is done before command line processing.
*/
{
#if !STANDALONE_UTILITY_PROGRAM
  num_of_mem_alloc_history_entries = 0;
  size_of_mem_alloc_history = 0;
  mem_alloc_history_entries_used = 0;
#endif /* !STANDALONE_UTILITY_PROGRAM */
  /* Initialize the general allocator. */
#if TRACK_GENERAL_ALLOCATION
  memory_allocation_map = new_direct<a_memory_allocation_map>(
                                                           /*mask_width=*/10u);
#endif /* TRACK_GENERAL_ALLOCATION */
#if EXPENSIVE_CHECKING
  resizable_memory_allocations = new_direct<a_memory_allocation_set>(
                                                           /*mask_width=*/10u);
#endif /* EXPENSIVE_CHECKING */
  mem_region_table = NULL;
  size_of_mem_region_table = 0;
  size_of_function_def_table = 0;
#if DEBUG
/* GCOVR_EXCL_START */
  max_mem_allocated = 0;
  mem_in_use_after_one_time_init = 0;
  max_mem_in_use_after_tu_init = 0;
  num_reclaim_attempts = 0;
  num_reclaim_traversals = 0;
  num_reclaim_successes = 0;
  num_malloc_calls = 0;
  num_bytes_allocated = 0;
  num_free_calls = 0;
  num_bytes_freed = 0;
  total_general_mem_allocated = 0;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
}  /* mem_manage_early_init */


void mem_manage_reset(void)
/*
Called when a PCH file has just been read to reset the memory management state.
*/
{
  /* Reset free_fe_map and reusable_fe_list.  This prevents issues where the
     front end memory region has been replaced resulting in alloc_fe handing
     out memory addresses already in use. */
  detail::freed_fe_map->clear();
  detail::reusable_fe_list->clear();
}  /* mem_manage_reset */


void mem_manage_init(void)
/*
Initialize static variables related to the mem_manage routines that
must be initialized for each compilation.
*/
{
  highest_used_region_number = NULL_region_number;
  file_scope_region_number = FILE_SCOPE_REGION_NUMBER;
  highest_used_function_def_number = NULL_function_def_number;
#if DEBUG
/* GCOVR_EXCL_START */
  total_mem_used = 0;
  num_alignment_bytes_allocated = 0;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
#if MAINTAIN_ALLOCATION_SEQUENCE_NUMBER
  allocation_sequence_number_seed = 0;
#endif /* MAINTAIN_ALLOCATION_SEQUENCE_NUMBER */
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  if (index_for_il_file !=  NULL) {
    /* This pointer will be non-NULL on all but the first compilation when
       multiple compilations are being processed.  Clear the array of
       values left over from a previous compilation. */
    memzero((char *)index_for_il_file,
            (size_t)size_of_mem_region_table * sizeof(a_file_position));
  }  /* if */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
  /* Initialize the memory region for general front end storage. */
  init_memory_region(NULL_region_number, (sizeof_t)0);
  /* Initialize the memory region for file scope IL information. */
  init_memory_region(FILE_SCOPE_REGION_NUMBER, (sizeof_t)0);
  /* Initialize the front end memory reuse data structures. */
  detail::freed_fe_map = new_general<detail::a_size_to_ptr_map>(1u);
  detail::reusable_fe_list =
                          new_general<detail::a_reusable_allocation_list>(10u);
}  /* mem_manage_init */

#if MAKE_FRONT_END_CALLABLE

static void free_general_memory(a_memory_allocation_map **map)
/*
Free the general memory specified by *map.
*/
{
  /* Free the allocated memory. */
  for (const a_memory_allocation_map::an_entry &entry : **map) {
    if (!entry.has_value()) {
      continue;
    }  /* if */
    free((void*)entry.key());
  }  /* for */
  /* Free the map itself and remove the reference to it. */
  delete_direct(map);
}  /* free_general_memory */


void mem_manage_wrapup(void)
/*
Free all memory used by the compilation.  This must be called at the
very end of processing.
*/
{
  /* Don't keep checking the stop token stack in db_enter/db_exit because
     the storage goes away when the front end memory region is freed. */
  curr_stop_token_stack_entry = NULL;
  if (mem_region_table != NULL) free_all_memory_regions();
#if !STANDALONE_UTILITY_PROGRAM
#if USE_MMAP_FOR_MEMORY_REGIONS
  if (mmap_initialized) {
    /* If a memory mapped temporary file was created, close it now. */
    close_memory_region_tmp_file(memory_region_file);
    mmap_initialized = FALSE;
  }  /* if */
  free_mapped_mem_blocks();
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
#endif /* !STANDALONE_UTILITY_PROGRAM */
#if EXPENSIVE_CHECKING
  delete_direct(&resizable_memory_allocations);
#endif /* EXPENSIVE_CHECKING */
  free_general_memory(&memory_allocation_map);
}  /* mem_manage_wrapup */

#endif /* MAKE_FRONT_END_CALLABLE */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

