/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

Throw processing for exception handling.

*/

#include "basics.h"
#include "runtime.h"
#include "eh.h"
#pragma hdrstop
#include "vec_newdel.h"

#if EXCEPTION_HANDLING

/*
A class used to declare a pointer-to-member object in a_throw_stack_entry.
*/
struct a_dummy_class {};

/* Structure used to maintain a stack of throws that are currently
   being processed. */
typedef struct a_throw_stack_entry *a_throw_stack_entry_ptr;
typedef struct a_throw_stack_entry {
  a_throw_stack_entry_ptr
		next;
			/* The next stack entry. */
  a_type_info_impl_ptr
		type_info;
			/* Type_Info of the object thrown. */
  a_destructor_ptr
		destructor;
			/* Destructor for the thrown object. */
  an_ETS_flag_set
		flags;
			/* A collection of bits that specify how the
			   additional information about the thrown object. */
  an_ETS_flag_set
		*ptr_flags;
			/* Pointer to an array of ETS flags for multi-level
			   pointers.  NULL for single level pointers, and for
			   ABI versions earlier that 2.41.  Used to implement
			   qualification conversions on multi-level
			   pointers. */
  an_access_flag_string
		access_flags;
			/* A null terminated character string that specifies
			   the accessibility of the base classes.  "y" means
			   that the base class is accessible, "m" means that
			   it is not. */
  void*		object_address;
			/* Pointer to the memory allocated to store
			   the copy of the object. */
  void*		pointer_buffer;
			/* A piece of memory large enough to store a pointer.
			   When a pointer is thrown the pointer may undergo
			   one of several possible conversions including a
			   conversion from a pointer to derived to a pointer
			   to base.  This buffer is used to store the modified
		  	   pointer.  The original pointer must be preserved for
			   use by a rethrow.  This is also used when a nullptr
			   is thrown.  It can be caught as a pointer value.
			   The pointer value is constructed here. */
  void* a_dummy_class::*
		ptr_to_data_member_buffer;
			/* A piece of memory large enough to store a
			   pointer-to-member that points to a nonstatic data
			   member.  When a nullptr is thrown it can be caught
			   as a pointer-to-member.  The pointer-to-member
			   value is constructed here. */
  void* (a_dummy_class::*
		ptr_to_member_function_buffer)();
			/* A piece of memory large enough to store a
			   pointer-to-member that points to a member function.
			   When a nullptr is thrown it can be caught
			   as a pointer-to-member.  The pointer-to-member
			   value is constructed here. */
  an_eh_stack_entry_ptr
		nearest_enclosing_try_block;
			/* Pointer to the nearest enclosing try block
			   (that is not currently in a handler) at
			   the point at which the throw was started.
			   This is used to detect abandoned throws. */
  a_throw_stack_entry_ptr
		primary_entry;
			/* If this is a rethrow, points to the throw stack
			   entry of the original throw. */
  unsigned long	use_count;
			/* Present only in primary entries (not rethrows).
			   Represents the number of throw stack entries
			   that are still active that refer to the object. */
  a_byte_boolean
		is_rethrow;
			/* TRUE if this entry represents a rethrow.
		  	   The object_address entry for a rethrow points
			   to an object allocated by a previous throw. */
  a_byte_boolean
		is_internal;
			/* TRUE if this entry represents an internal
			   rethrow. */
  a_byte_boolean
		discard_entry;
			/* This field is used during the processing of
			   nested throws.  This flag is set when a stack
			   entry is no longer needed but cannot be freed
			   because an entry higher on the stack has not
			   yet been freed. */
  a_byte_boolean
		dtor_called;
			/* TRUE if the destructor for the object has already
			   been called.  This is only used for entries created
			   by throws that are not rethrows. */
  a_byte_boolean
		in_handler;
			/* TRUE when the object has been passed to a
			   handler.  It is at this point that the
			   object can be rethrown. */
  a_byte_boolean
		object_evaluation_complete;
			/* TRUE when the evaluation of the thrown object has
			   been completed, but before the object has been
			   copied to the EH temporary.  If the copy to the EH
			   temporary is elided, then this flag is set when
			   the execution of __throw begins. */
  a_byte_boolean
		object_copy_complete;
			/* Set to FALSE when __throw_alloc is called and
			   set to TRUE when __throw is called.  This
			   flag indicates that the thrown object has been
			   copied and must be destroyed at some point. */
  a_byte_boolean
		use_access_flags;
			/* TRUE if the access_flags string should be used to
			   determine the accessibility of base classes.
			   The access flag string was originally used for
			   this purpose but was later replaced by static
			   information in the base class specification
			   information.  This flag indicates which access
			   checking method should be used for a given throw. */
  an_eh_stack_entry
		throw_marker;
			/* An EH stack entry for the throw marker to be
			   linked into the EH stack while a given throw
			   is active. */
} a_throw_stack_entry;


/* Structure used to record information about blocks of memory handled
   by the EH memory management routines. */
typedef struct a_mem_block_descr *a_mem_block_descr_ptr;
typedef struct a_mem_block_descr {
  a_mem_block_descr_ptr
		next;
			/* The next stack entry. */
  void*		addr;
			/* Address of the block of memory. */
  a_sizeof_t	size;
			/* Size in bytes of the block of memory. */
  a_sizeof_t	used;
			/* Number of bytes used in the block. */
  a_byte_boolean
		dynamically_allocated;
			/* TRUE if the block of memory was dynamically
			   allocated.  The initial memory block is
			   statically allocated. */
} a_mem_block_descr;


/* Describes a single piece of memory allocated by the EH runtime. */
typedef struct a_mem_allocation *a_mem_allocation_ptr;
typedef struct a_mem_allocation {
  a_mem_allocation_ptr
		next;
			/* The next allocation entry. */
  a_sizeof_t	alloc_size;
			/* Size of the piece of memory.  This is the
			   allocated size including any space needed
			   for alignment not just the requested size. */
  void*		addr;
			/* Address of the memory allocated. */
  a_byte_boolean
		is_mem_block_descr_allocation;
			/* TRUE if this is a memory allocation done
			   to keep track of a memory block description
		           record. */
} a_mem_allocation;

extern "C" {
  typedef void (*a_destroy_exception_object_ptr)(void);
			/* Type of the routine called to destroy the
			   exception object. */
}

/* Forward declaration. */
EXTERN_C void __destroy_exception_object(void);

#if ABI_CHANGES_FOR_RTTI
char		MANGLED_NAME_OF_UNIQUE_ID_OF_VOID;
			/* This is used to get the address of the
			   unique ID for the void type for pointer to
			   void* conversions. */
#ifdef __EDG_CPP11_IL_EXTENSIONS_SUPPORTED
char		MANGLED_NAME_OF_UNIQUE_ID_OF_NULLPTR;
			/* This is used to get the address of the
			   unique ID for the std::nullptr_t type for 
			   std::nullptr_t to pointer conversions. */
#endif /* ifdef __EDG_CPP11_IL_EXTENSIONS_SUPPORTED */
#else /* !ABI_CHANGES_FOR_RTTI */
a_type_info_impl
                MANGLED_NAME_OF_VOID;
			/* This is used to get the address of the
			   type_info for the void type for pointer to
			   void* conversions. */
#ifdef __EDG_CPP11_IL_EXTENSIONS_SUPPORTED
a_type_info_impl
                MANGLED_NAME_OF_NULLPTR;
			/* This is used to get the address of the
			   type_info for the std::nullptr_t type for
			   std::nullptr_t to pointer conversions. */
#endif /* ifdef __EDG_CPP11_IL_EXTENSIONS_SUPPORTED */
#endif /* !ABI_CHANGES_FOR_RTTI */


static a_throw_stack_entry_ptr
		curr_throw_stack_entry = NULL;
			/* The pointer to the top of the stack of throw
			   entries. */

static a_mem_block_descr_ptr
		curr_mem_block_descr = NULL;
			/* Pointer to the top of a stack of memory
			   blocks managed by the EH runtime. */

static a_mem_allocation_ptr
		mem_allocation_stack = NULL;
			/* Pointer to the top of a stack of memory
			   allocation entries. */

static a_mem_block_descr
		initial_mem_block_descr;
			/* Initial entry pointed to by the memory block
			   stack. */


static union {
  char		memory[EH_MEMORY_ALLOCATION_INCREMENT];
			/* The initial block of memory to be used.  This
			   avoids the need for the EH runtime to do
			   any dynamic memory allocation in most cases. */
  TYPE_WITH_MOST_STRICT_ALIGNMENT
		dummy;
			/* Used to ensure that the memory block is
			   guaranteed to be aligned on an appropriate
			   boundary. */
} initial_mem_block;

/* Round a given size up to a multiple of MOST_STRICT_ALIGNMENT. */
#define round_size_to_alignment(size)					\
  (((size + MOST_STRICT_ALIGNMENT - 1) / MOST_STRICT_ALIGNMENT) *	\
                                                   MOST_STRICT_ALIGNMENT)

/* The number of bytes needed for a memory block description and any
   required alignment. */
#define NEEDED_FOR_MEM_BLOCK_DESCR \
  round_size_to_alignment(sizeof(a_mem_block_descr))


/* The number of bytes needed for a memory allocation structure and any
   required alignment. */
#define NEEDED_FOR_MEM_ALLOCATION_INFO \
  round_size_to_alignment(sizeof(a_mem_allocation))

/* The number of bytes needed at the end of a memory block to record the
   information needed to allocate a new memory block. */
#define RESERVED_FOR_END_OF_MEM_BLOCK \
  (NEEDED_FOR_MEM_BLOCK_DESCR + NEEDED_FOR_MEM_ALLOCATION_INFO)


/* The number of bytes that must be added to the_ptr to obtain a value
   with suitable alignment.  the_ptr is actually an integer value that
   represents an offset from the base of a block of memory that is known
   to be appropriately aligned. */
#define increment_needed_for_alignment(the_ptr)				\
  (((the_ptr % MOST_STRICT_ALIGNMENT) == 0) ?	\
         0 :								\
         (MOST_STRICT_ALIGNMENT - (the_ptr % MOST_STRICT_ALIGNMENT)))


static void* eh_get_memory(a_sizeof_t	size)
/*
This is a low level routine that just gets a piece of dynamically
allocated memory from the system.   This must get the memory in
a means that will not result in an exception being thrown.
*/
{
  void*		mem_block;

  mem_block = malloc(size);
  /* If we can't get the memory we need, call the terminate routine. */
  if (mem_block == NULL) {
    __call_terminate();
  }  /* if */
  return mem_block;
}  /* eh_get_memory */


static void eh_free_memory(void* ptr)
/*
This is a low level routine that simply frees a piece of dynamically allocated
memory to the system.  This must free the memory in a means that will not
result in an exception being thrown.
*/
{
  free(ptr);
}  /* eh_free_memory */


static void mem_block_descr_init(a_mem_block_descr_ptr mbdp)
/*
Initialize the fields of a memory block description record.
*/
{
  mbdp->next = NULL;
  mbdp->addr = NULL;
  mbdp->size = 0;
  mbdp->used = 0;
  mbdp->dynamically_allocated = FALSE;
}  /* mem_block_descr_init */


static void init_eh_memory_management(void)
/*
Initialize the variables that keep track of memory used by the EH runtime.
*/
{
  /* Initialize the initial memory block description record. */
  mem_block_descr_init(&initial_mem_block_descr);
  initial_mem_block_descr.addr = initial_mem_block.memory;
  initial_mem_block_descr.size = EH_MEMORY_ALLOCATION_INCREMENT;
  initial_mem_block_descr.used = 0;
  initial_mem_block_descr.dynamically_allocated = FALSE;
  curr_mem_block_descr = &initial_mem_block_descr;
}  /* init_eh_memory_management */


/*
Return the address of the specified character position within the
current memory block.
*/
#define addr_in_mem_block(pos)						\
  (void *)(((char *)curr_mem_block_descr->addr) + pos)


static void* alloc_in_mem_block(a_sizeof_t	      size,
			        a_mem_allocation_ptr* map)
/*
Allocate a memory allocation record and the requested amount of space
in the current memory block.  There must be enough space for the allocation
to succeed and size must be a multiple of MOST_STRICT_ALIGNMENT.
*/
{
  void*			ptr;
  int			used;

  /* Get space from the memory block to store a new memory block description
     an a memory allocation record to describe it. */
  used = curr_mem_block_descr->used;
  *map = (a_mem_allocation_ptr)addr_in_mem_block(used);
  used += NEEDED_FOR_MEM_ALLOCATION_INFO;
  ptr = (void*)addr_in_mem_block(used);
  used += size;
  curr_mem_block_descr->used = used;
  /* Add this memory allocation record to the top of the stack. */
  (*map)->next = mem_allocation_stack;
  (*map)->addr = ptr;
  mem_allocation_stack = *map;
  /* Initialize the other fields of the memory allocation record. */
  (*map)->alloc_size = size;
  (*map)->is_mem_block_descr_allocation = FALSE;
  check_assertion(curr_mem_block_descr->used <= curr_mem_block_descr->size);
  check_assertion(size % MOST_STRICT_ALIGNMENT == 0);
  return ptr;
}  /* alloc_in_mem_block */


static void alloc_new_mem_block(a_sizeof_t	size)
/*
Allocate a new memory block of at least "size" bytes.  Actually,
it must also have enough space for an additional mem_block_descr entry
too.  We actually allocate "size + EH_MEM_ALLOCATION_INCREMENT" bytes since
we know that "size" bytes will immediately be consumed.
*/
{
  void*			mem_block;
  a_mem_allocation_ptr	map;
  a_mem_block_descr_ptr	mpdp;
  a_sizeof_t		new_size;

  /* Adjust the requested size.  The adjusted size is a multiple of the
     memory allocation increment.  If (adjusted_size - size) >
     (memory_allocation_increment * .5) then we allocate an extra
     memory_allocation_increment bytes. */
  new_size = ((size / EH_MEMORY_ALLOCATION_INCREMENT) + 1) *
                                              EH_MEMORY_ALLOCATION_INCREMENT;
  if ((new_size - size) < (EH_MEMORY_ALLOCATION_INCREMENT >>1)) {
    new_size += EH_MEMORY_ALLOCATION_INCREMENT;
  }  /* if */
  size = new_size;
  /* Get space from the memory block to store a new memory block description
     an a memory allocation record to describe it. */
  mpdp = (a_mem_block_descr_ptr)alloc_in_mem_block(NEEDED_FOR_MEM_BLOCK_DESCR,
						   &map);
  map->is_mem_block_descr_allocation = TRUE;
  mem_block = eh_get_memory(size);
  /* Add the new memory block description to the top of the stack. */
  mpdp->next = curr_mem_block_descr;
  curr_mem_block_descr = mpdp;
  /* Initialize the fields of the memory block descriptor. */
  mpdp->addr = mem_block;
  mpdp->size = size;
  mpdp->used = 0;
  mpdp->dynamically_allocated = TRUE;
}  /* alloc_new_mem_block */


static void* eh_alloc_on_stack(a_sizeof_t	size)
/*
Allocate a block of memory on the EH memory stack.
*/
{
  a_mem_allocation_ptr	map;
  int			needed_for_alignment;
  void*			ptr;
  a_sizeof_t		alloc_size;

  /* The memory management system is initialized the first time that
     this routine is called. */
  if (curr_mem_block_descr == NULL) {
    init_eh_memory_management();
  }  /* if */
  /* Determine the number of bytes that must be added to size to ensure
     that the resulting "used" value will be appropriately aligned. */
  needed_for_alignment = increment_needed_for_alignment(size);
  /* Make sure that the current memory block would have enough space
     leftover to allocate the requested space, plus the space needed for
     the memory allocation information plus a new memory block descriptor.
     If not, start the new memory block now. */
  alloc_size = size + needed_for_alignment;
  if ((alloc_size + NEEDED_FOR_MEM_ALLOCATION_INFO +
       curr_mem_block_descr->used +
       RESERVED_FOR_END_OF_MEM_BLOCK) > curr_mem_block_descr->size) {
    alloc_new_mem_block(alloc_size);
  }  /* if */
  ptr = alloc_in_mem_block(alloc_size, &map);
#if DEBUG
  if (__debug_level >= 5) {
    fprintf(__f_debug, "Allocated %lu bytes starting at %p, ending at %p\n",
            (unsigned long)size, (void*)ptr, (void*)(((char *)ptr)+size-1));
  }  /* if */
#endif /* DEBUG */
  return ptr;
}  /* eh_alloc_on_stack */


static void free_in_mem_block(void*	ptr)
/*
Free a block of memory allocated in a memory block.
*/
{
  a_mem_allocation_ptr	map;
  int			used;

  map = mem_allocation_stack;
  mem_allocation_stack = map->next;
  check_assertion(map->addr == ptr);
  used = curr_mem_block_descr->used;
  used -= map->alloc_size;
  used -= NEEDED_FOR_MEM_ALLOCATION_INFO;
  curr_mem_block_descr->used = used;
}  /* free_in_mem_block */


static void eh_free_on_stack(void*	ptr)
/*
Free a piece of memory on the memory stack.  If a memory block becomes
empty then remove it from the stack.
*/
{
  /* Free the memory passed by the caller. */
  free_in_mem_block(ptr);
  /* Is the memory block now empty? */
  if (curr_mem_block_descr->used == 0) {
    if (curr_mem_block_descr->next != NULL) {
      /* Don't free the initial memory block. */
      a_mem_block_descr_ptr	mpdp_to_free;
      mpdp_to_free = curr_mem_block_descr;
      curr_mem_block_descr = mpdp_to_free->next;
      /* Free the memory block.  This is freed to the system -- not just to
         the memory stack like other kinds of memory. */
      if (mpdp_to_free->dynamically_allocated) {
        /* Only free dynamically allocated blocks. */
        eh_free_memory(mpdp_to_free->addr);
      }  /* if */
      /* Free the memory block description entry. */
      free_in_mem_block(mpdp_to_free);
    }  /* if */
  }  /* if */
}  /* eh_free_on_stack */


/* Determine whether the type qualifiers are acceptable.  The thrown type
   may not have more qualifiers than the caught type.  The qualifiers are
   only checked if the thrown type is a pointer. */
#define qualifiers_acceptable(caught_flags, thrown_flags)		\
  ((!is_single_level_pointer(thrown_flags)) || 				\
   (caught_flags & thrown_flags & ETS_QUALIFIERS) ==			\
                                            (thrown_flags & ETS_QUALIFIERS))

#if DEBUG
static void db_eh_region_descr(an_eh_region_descr_ptr  ehrdp)
/*
Print the contents of a region description entry.
*/
{
  if (ehrdp == NULL) {
    fprintf(__f_debug, "  <NULL pointer>\n");
  } else {
    if (ehrdp->flags) {
      fprintf(__f_debug, "  flags: ");
      if (ehrdp->flags & RDF_INDIRECT) fprintf(__f_debug, " indirect");
      if (ehrdp->flags & RDF_ARRAY) fprintf(__f_debug, " array");
      if (ehrdp->flags & RDF_NEW_ALLOCATION) fprintf(__f_debug, " new");
      if (is_base_class_subobject(ehrdp->flags)) {
        fprintf(__f_debug, " subobject");
        /* The SUBOBJECT_VTABLE and LET_THIS flags share the same bit.
           The meaning depends on the setting of the BASE_CLASS_SUBOBJECT
           flag.  In addition, the BASE_CLASS_SUBOBJECT and VLA flags share
           the same bit.  The meaning depends on the setting of the RDF_ARRAY
           flag. */
        if (ehrdp->flags & RDF_SUBOBJECT_VTABLE) {
          fprintf(__f_debug, " subobject vtable");
        }  /* if */
      } else if (ehrdp->flags & RDF_ARRAY) {
        if (ehrdp->flags & RDF_VLA) {
          fprintf(__f_debug, " VLA");
        }  /* if */
      }  /* if */
      if (ehrdp->flags & RDF_GUARD_VAR_FOR_LOCAL_STATIC) {
        fprintf(__f_debug, " local static guard");
      }  /* if */
    }  /* if */
    fprintf(__f_debug, "  destr/delete=%p\n",
            (void*)ehrdp->destructor_or_delete_routine);
#if 0
    if (ehrdp->array_size != 0) {
      fprintf(__f_debug, "  array_size=%ld\n", ehrdp->array_size);
    }  /* if */
#endif /* 0 */
    fprintf(__f_debug, "  handle=%d\n", ehrdp->handle);
    fprintf(__f_debug, "  next region=%d\n", ehrdp->index_of_next_region);
  }  /* if */
}  /* db_eh_region_descr */


static const char *eh_stack_entry_kind_name(an_eh_stack_entry_kind kind)
/*
Return the name of the specified EH stack entry kind value.
*/
{
  const char	*name;

  switch (kind) {
    case ehsek_old_try_block:           name = "old try block";     break;
    case ehsek_function:                name = "function";          break;
    case ehsek_throw_spec:              name = "throw spec";        break;
    case ehsek_throw_processing_marker: name = "throw marker";      break;
    case ehsek_vec_new_or_delete:       name = "vec new or delete"; break;
    case ehsek_try_block:               name = "try block";         break;
    case ehsek_noexcept:                name = "noexcept";          break;
    default:                            name = "<BAD KIND>";        break;
  }  /* switch */
  return name;
}  /* eh_stack_entry_kind_name */


static void db_eh_stack_entry(an_eh_stack_entry_ptr	ehsep)
/*
Display an EH stack entry, for debugging purposes.
*/
{
  fprintf(__f_debug, "  EH stack entry at %p, kind=%s", ehsep,
          eh_stack_entry_kind_name(ehsep->kind));
  if (ehsep->kind == (an_eh_stack_entry_kind)ehsek_function) {
    fprintf(__f_debug, ", obj addr table=%p",
            ehsep->variant.function.object_address_table);
  }  /* if */
  fprintf(__f_debug, "\n");
}  /* db_eh_stack_entry */


EXTERN_C void db_eh_stack(void)
/*
Display the EH stack, for debugging purposes.
*/
{
  an_eh_stack_entry_ptr	ehsep;

  for (ehsep = __curr_eh_stack_entry; ehsep != NULL; ehsep = ehsep->next) {
    db_eh_stack_entry(ehsep);
  }  /* for */
}  /* db_eh_stack */


static void db_throw_stack_entry(a_throw_stack_entry_ptr tsep)
{
  fprintf(__f_debug, "typinfo=%p ", (void*)tsep->type_info);
  fprintf(__f_debug, "flags=%0x ", tsep->flags);
  fprintf(__f_debug, "object_address=%p ", (void*)tsep->object_address);
  if (tsep->is_rethrow) {
    fprintf(__f_debug, "is_rethrow=%0d ", tsep->is_rethrow);
  }  /* if */
  if (tsep->is_internal) {
    fprintf(__f_debug, "is_internal=%0d ", tsep->is_internal);
  }  /* if */
  if (tsep->discard_entry) {
    fprintf(__f_debug, "discard_entry=%0d ", tsep->discard_entry);
  }  /* if */
  if (tsep->dtor_called) {
    fprintf(__f_debug, "dtor_called=%0d ", tsep->dtor_called);
  }  /* if */
  if (tsep->in_handler) {
    fprintf(__f_debug, "in_handler=%0d ", tsep->in_handler);
  }  /* if */
  if (tsep->use_count != 0) {
    fprintf(__f_debug, "use_count=%0lu ", tsep->use_count);
  }  /* if */
  if (tsep->primary_entry != NULL) {
    fprintf(__f_debug, "primary_entry=%p ", tsep->primary_entry);
  }  /* if */
}  /* db_throw_stack_entry */


static void db_throw_stack(const char* str)
{
  a_throw_stack_entry_ptr	tsep = curr_throw_stack_entry;
  int				count = 0;

  fprintf(__f_debug, "Throw stack %s:\n", str);
  while (tsep != NULL) {
    fprintf(__f_debug, "  Entry %0d at %p: ", count++, tsep);
    db_throw_stack_entry(tsep);
    fprintf(__f_debug, "\n");
    tsep = tsep->next;
  }  /* while */
}  /* db_throw_stack */
#endif /* DEBUG */


static void cleanup(an_eh_stack_entry_ptr ehsep,
                    a_region_number       region,
		    a_region_number	  stop_at_region)
/*
Do the cleanup operations required in the function described by ehsep.
The current region number within ehsep is designated by region.  Cleanup
processing stops when we reach the region designated by stop_at_region.
Normally this is NULL_REGION_NUMBER but may have another value when
doing a partial cleanup as is done when an object in a try block
requires cleanup.
*/
{
  an_object_ptr	                *obj_addr_array;
  an_eh_region_descr_ptr	ehrdp;
#if DEBUG
  if (__debug_level >= 2) {
    fprintf(__f_debug, "In cleanup(), cleaning up from %lu to %lu\n",
            (unsigned long)region, (unsigned long)stop_at_region);

  }  /* if */
#endif /* DEBUG */
  obj_addr_array = ehsep->variant.function.object_address_table;
  for (; region != stop_at_region; region = ehrdp->index_of_next_region) {
    an_object_ptr	        obj_addr = NULL;
    a_conditional_flag*	        flag_addr = NULL;
    char			*temp_addr;
    a_region_descr_flag_set     flags;
    an_eh_array_supplement_ptr	ehasp = NULL;
    void			*vtbl_ptr;
    a_boolean			has_vtbl_ptr = FALSE;
    a_destroy_exception_object_ptr
				potential_destroy_exception_object_ptr;

    ehrdp = &ehsep->variant.function.regions[region];
#if DEBUG
    if (__debug_level >= 2) {
      fprintf(__f_debug, "Region: %d, descr address=%p\n", region,
              (void*)ehrdp);
      db_eh_region_descr(ehrdp);
    }  /* if */
#endif /* DEBUG */
    /* Check whether this is a special region entry for the destruction
       of the exception object.  To do this we compare the routine
       address with that of __destroy_exception_object.  This is done
       so that routine can be called correctly (with no arguments). */
    potential_destroy_exception_object_ptr =
           (a_destroy_exception_object_ptr)ehrdp->destructor_or_delete_routine;
    if (potential_destroy_exception_object_ptr ==
                                                  __destroy_exception_object) {
      __destroy_exception_object();
      continue;
    }  /* if */
    flags = ehrdp->flags;
    if (flags & RDF_CONDITIONAL_FLAG) {
      /* This cleanup action is conditional.  The next region entry
         contains a handle that points to the flag.  Check the flag and
         only process this entry if it is TRUE. */
#if 0
      /* The following line needs to be modified when stack offsets are
	 being used instead of an object address array. */
#endif /* 0 */
      /* The object information is pointed to directly by the region entry. */
      flag_addr = (a_conditional_flag*)*(obj_addr_array + (ehrdp + 1)->handle);
#if DEBUG
      if (__debug_level >= 2) {
        fprintf(__f_debug, "  Conditional flag=%0d\n", *flag_addr);
      }  /* if */
#endif /* DEBUG */
      /* Skip processing of this entry if the flag is not set. */
      if (!*flag_addr) continue;
    }  /* if */
    if ((flags & RDF_SUBOBJECT_VTABLE) != 0 &&
        is_base_class_subobject(flags)) {
      /* This is a subobject destruction that has a special vtable pointer
         that is to be used.  The next region table entry contains a handle
         that points to the vtable address to be used.  If there is a
         conditional flag, the handle is in the region table entry after
         the conditional flag.  It's possible that the vtbl_ptr is NULL
         (in cases where a delegation destructor is being used). */
      has_vtbl_ptr = TRUE;
      an_eh_region_descr_ptr	vtbl_ehrdp;
      vtbl_ehrdp = ehrdp + 1;
      if (flag_addr != NULL) vtbl_ehrdp++;
#if 0
      /* The following line needs to be modified when stack offsets are
	 being used instead of an object address array. */
#endif /* 0 */
      vtbl_ptr = *(void**)(obj_addr_array + vtbl_ehrdp->handle);
      if (vtbl_ehrdp->flags & RDF_INDIRECT) {
        /* If the indirect flag is set on the vtable region entry, get the
           actual vtable pointer from the address referred to by the region
           table entry.  This is not used by the fully portable mechanism. */
        temp_addr = (char *)*(void**)vtbl_ptr;
        vtbl_ptr = (void *)temp_addr;
      }  /* if */
#if DEBUG
      if (__debug_level >= 2) {
        fprintf(__f_debug, "  Vtable pointer=%p\n", vtbl_ptr);
      }  /* if */
#endif /* DEBUG */
    }  /* if */
#if 0
    /* In an implementation that uses stack offsets instead of an object
       address table, the handle field in the region description entry 
       will be a stack offset to be added to either the stack base or the
       current "this" parameter (if RDF_THIS_PARAM_OFFSET is set). */
#endif /* 0 */
    check_assertion(obj_addr_array != NULL);
    if (flags & RDF_ARRAY) {
      /* The object information is contained in the array supplement. */
      ehasp = &ehsep->variant.function.array_table[ehrdp->handle];
      obj_addr = *(obj_addr_array + ehasp->handle);
    } else {
      /* The object information is pointed to directly by the region entry. */
      obj_addr = *(obj_addr_array + ehrdp->handle);
    }  /* if */
    if (flags & RDF_INDIRECT) {
      /* If the indirect flag is set, get the actual object address from
         the address referred to by the region table entry.  This is not
         used by the fully portable mechanism. */
      temp_addr = (char *)*(void**)obj_addr;
      obj_addr = (void *)temp_addr;
    }  /* if */
#if DEBUG
    if (__debug_level >= 2) {
      fprintf(__f_debug, "  object address=%p\n", (void*)obj_addr);
    }  /* if */
#endif /* DEBUG */
    /* Do the actual cleanup of the object. */
    if ((flags & RDF_GUARD_VAR_FOR_LOCAL_STATIC) != 0) {
      /* The cleanup object is the variable that is set when a local static
         variable is initialized.  When such an entry is on the cleanup list
         it means that the exception was thrown while the local static was
         being initialized.  The cleanup action is to reset the guard
         variable so that it will be initialized again the next time the
         declaration of the local static is reached.  The test of
         RDF_BASE_CLASS_SUBOBJECT is needed because the GUARD_VAR bit
         is shared with the SUBOBJECT_VTABLE bit. */
      flag_addr = (a_conditional_flag*)obj_addr;
#ifdef __EDG_IA64_ABI
      /* In the IA-64 ABI, the guard is not actually set until the object
         initialization is complete, so the guard variable does not really
         need to be cleared (although the runtime routine does do so).  The
         call of __cxa_guard_abort is primarily done so that a lock can be
         released, if the implementation uses such locks. */
      ABI_NAMESPACE::__cxa_guard_abort((an_ia64_guard_ptr)flag_addr);
#else /* ifndef __EDG_IA64_ABI */
      *flag_addr = 0;
#endif /* ifdef __EDG_IA64_ABI */
    } else if (!(flags & RDF_NEW_ALLOCATION)) {
      /* A normal (not a new allocation) region.  Call the destructor for
         the object. */
      a_destructor_ptr	dtor_ptr;
      dtor_ptr = (a_destructor_ptr)ehrdp->destructor_or_delete_routine;
      if (flags & RDF_ARRAY) {
        /* The destructor pointer can be NULL if the array is a VLA.
           In such cases, the region table entry is present so that the
           VLA can be deallocated (although as provided this routine can
           only perform the deallocation when VLA operations are lowered). */
        a_boolean		is_vla = (flags & RDF_VLA) != 0;
        if (dtor_ptr != NULL) {
          an_element_count	elements = ehasp->array_size;
          if (is_vla) {
            /* The array size for a VLA is accessed using the handle in the
               region table entry that follows the entry for the VLA. */
            a_sizeof_t		*element_addr;
            element_addr = (a_sizeof_t*)(obj_addr_array[(ehrdp + 1)->handle]);
            elements = (an_element_count)*element_addr;
         }  /* if */
#ifndef __EDG_IA64_ABI
          __edg_vec_dtor(obj_addr, (size_t)elements,
                         ehasp->element_size, dtor_ptr);
#else /* ifdef __EDG_IA64_ABI */
          ABI_NAMESPACE::__cxa_vec_dtor(obj_addr, elements, 
                                        ehasp->element_size, dtor_ptr);
#endif /* ifdef __EDG_IA64_ABI */
        }  /* if */
      } else if (has_vtbl_ptr) {
        /* A non-array object for which a special destructor must be called
           in order to supply information about the construction vtable to
           be used.  Note that this is also used to invoke a "delegation"
           destructor which inspects the value of the pointer (a VTT value
           in the IA-64 ABI, and a pointer to a base class in the Cfront ABI)
           then invokes either the complete or subobject destructor (in the
           IA-64 ABI) or invokes the destructor with the appropriate second
           argument (in the Cfront ABI). */
        a_destructor_with_vtable_param_ptr	dtor_with_vtable;
        dtor_with_vtable = (a_destructor_with_vtable_param_ptr)dtor_ptr;
        dtor_with_vtable(obj_addr, vtbl_ptr);
      } else {
#ifndef __EDG_IA64_ABI
        /* Not an array and not an object that requires special construction
           vtable information.  Just destroy the object.  If the object is a
           complete object, pass in the value "2" (which corresponds to
           DT_COMPLETE in the front end) to indicate that the
           object and any subobjects should be destroyed.  If the object
           is itself a base class subobject, pass in the value "0"
           indicating that only the object (and not any subobjects)
           should be destroyed. */
        (dtor_ptr)(obj_addr, is_base_class_subobject(flags) ? 0 : 2);
#else /* ifdef __EDG_IA64_ABI */
        (dtor_ptr)(obj_addr);
#endif /* ifdef __EDG_IA64_ABI */
      }  /* if */
    } else {
      /* A new allocation region.  Call the delete operator to free the
         space. */
      if (obj_addr != NULL) {
        if (flags & RDF_ARRAY) {
          /* The array flag indicates that this is the two operand form of
             the delete operation. */
          a_two_operand_delete_ptr	delete_ptr;
          delete_ptr =
                (a_two_operand_delete_ptr)ehrdp->destructor_or_delete_routine;
          (delete_ptr)(obj_addr, ehasp->element_size);
        } else {
          a_delete_ptr	delete_ptr;
          delete_ptr = (a_delete_ptr)ehrdp->destructor_or_delete_routine;
          (delete_ptr)(obj_addr);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
}  /* cleanup */


/*
Return TRUE if tp1_qualifiers does not have some type qualifier that
tp2_qualifiers has.
*/
#define any_qualifier_in_set_missing(tp1_qualifiers, tp2_qualifiers)  \
  ((~(tp1_qualifiers) & (tp2_qualifiers)) != 0)

#if ABI_COMPATIBILITY_VERSION >= 241

static a_boolean check_pointer_levels_and_qualifiers(
			an_exception_type_specification_ptr	etsp,
			an_ETS_flag_set				*ptr_flags)
/*
Compare the type specified by "ptr_flags" with the one specified by
"etsp".  If the pointers have the same number of levels, and if the
qualifiers are compatible, return TRUE; otherwise return FALSE.  The
caller has already verified that both types are multi-level pointers.
The caller is also responsible for ensuring that the types pointed to
are the same.  For the qualifiers to be compatible, a qualification
conversion, as described in 4.4 [conv.qual] of the standard must be
permitted.
*/
{
  a_boolean		okay;
  a_boolean		previous_qualifiers_include_const = TRUE;
  an_ETS_flag_set	*source_ptr_flags;
  an_ETS_flag_set	*dest_ptr_flags;

  dest_ptr_flags = etsp->ptr_flags;
  source_ptr_flags = ptr_flags;
  for (okay = TRUE; okay == TRUE;) {
    an_ETS_flag_set	dest_qualifiers;
    an_ETS_flag_set	source_qualifiers;
    /* Get the qualifiers for the current level. */
    dest_qualifiers = get_qualifiers(*dest_ptr_flags);
    source_qualifiers = get_qualifiers(*source_ptr_flags);
    if (is_last(*source_ptr_flags) != is_last(*dest_ptr_flags)) {
      /* The number of levels of pointers do not match. */
      okay = FALSE;
    } else if (any_qualifier_in_set_missing(dest_qualifiers,
                                            source_qualifiers)) {
      /* Some qualifier is missing. */
      okay = FALSE;
    } else {
      /* If the destination has additional qualifiers not found in the
         source, any previous qualifiers must have included const. */
      if (any_qualifier_in_set_missing(source_qualifiers,
				       dest_qualifiers)) {
	okay = previous_qualifiers_include_const;
	if (!okay) break;
      }  /* if */
      /* See if this qualifier includes const. */
      if (!is_const(dest_qualifiers)) {
	previous_qualifiers_include_const = FALSE;
      }  /* if */
    }  /* if */
    /* Terminate the loop if this is the last qualifier. */
    if (is_last(*source_ptr_flags)) break;
    dest_ptr_flags++;
    source_ptr_flags++;
  }  /* for */
  return okay;
}  /* check_pointer_levels_and_qualifiers */

#endif /* ABI_COMPATIBILITY_VERSION >= 241 */

static int check_exception_type_specifications(
		an_exception_type_specification_ptr	etsp,
		a_type_info_impl_ptr			type_info,
		an_ETS_flag_set				flags,
		an_ETS_flag_set				*ptr_flags,
		an_access_flag_string			access_flags,
		a_boolean				use_access_flags,
		void					**object_ptr,
		an_exception_type_specification_ptr	*etsp_found,
		a_boolean				*nullptr_conv_needed)
/*
Examine the exception type information associated with a given try block or
throw specification and determine whether any of the entries match the
object being thrown.  Returns 0 if no matching catch was found.  If a match
is found the position in the catch array is returned (actually, the array
index plus 1).  A pointer to the exception type specification of the matching
entry is returned in etsp_found.  A flag indicating whether or not the match
involved a conversion from nullptr to a pointer or pointer to member type
is returned via nullptr_conv_needed (if it is not NULL).
*/
{
  int		        result = 0;
  int		        index = 0;
  a_boolean	        done = FALSE;
  a_boolean		is_ptr;

  if (nullptr_conv_needed != NULL) *nullptr_conv_needed = FALSE;
  *etsp_found = NULL;
  is_ptr = is_pointer(flags, ptr_flags);
  do {
    a_boolean	        match = FALSE;
    void*               new_ptr;
    a_boolean		ets_is_ptr;
    a_boolean		is_single_ptr;
    a_boolean		ets_is_single_ptr;
    an_access_flag_string local_access_flags = access_flags;
#if DEBUG
    void* orig_ptr = object_ptr != NULL ? *object_ptr : NULL;
#endif /* DEBUG */
    ets_is_ptr = is_pointer(etsp->flags, etsp->ptr_flags);
    ets_is_single_ptr = is_single_level_pointer(etsp->flags);
    is_single_ptr = is_single_level_pointer(flags);
    index++;
    if (is_ellipsis(etsp->flags)) {
      match = TRUE;
    } else if (ets_is_ptr != is_ptr) {
      /* One is a pointer and the other is not.  This can't be a match. */
    } else if (matching_type_info(etsp->type_info, type_info)) {
      /* The underlying types match.  Determine whether the any pointer levels
         above that type are acceptable. */
      if (!is_ptr) {
        /* Both are not pointers -- a potential match. */
        if (is_pointer_to_member_function(etsp->flags) &&
            is_pointer_to_member_function(flags) &&
            is_noexcept(etsp->flags) &&
            !is_noexcept(flags)) {
          /* Two pointer-to-member-functions, but the exception specifications
             are not a match (i.e., a potentially-throwing function type cannot
             be caught by a non-throwing function type). */
        } else {
          match = TRUE;
        }  /* if */
      } else if (is_single_ptr != ets_is_single_ptr) {
        /* One pointer is single level, the other is multi-level.  No match. */
      } else if (is_single_ptr) {
        /* Both are single level pointers.  Make sure that any qualifiers
           present on the source type are there on the destination. */
        an_ETS_flag_set	source_qualifiers = get_qualifiers(flags);
        an_ETS_flag_set	dest_qualifiers = get_qualifiers(etsp->flags);
        if (!any_qualifier_in_set_missing(dest_qualifiers,
                                          source_qualifiers)) {
          /* The cv-qualifiers are acceptable. */
          if (!is_noexcept(etsp->flags) || is_noexcept(flags)) {
            /* The destination has no noexcept exception specification (in
               which case the source exception specification is irrelevant), or
               both the source and destination have noexcept exception
               specifications (which is also a match). */
            match = TRUE;
          }  /* if */
        }  /* if */
#if ABI_COMPATIBILITY_VERSION >= 241
      } else {
        /* Both are multi-level pointers.  Make sure the source can be
           converted to the destination by a valid qualification conversion. */
        if (check_pointer_levels_and_qualifiers(etsp, ptr_flags)) {
          match = TRUE;
        }  /* if */
#endif /* ABI_COMPATIBILITY_VERSION >= 241 */
      }  /* if */
    }  /* if */
    if (match) {
      /* We already found a match -- doesn't check further. */
#ifdef __EDG_CPP11_IL_EXTENSIONS_SUPPORTED
    } else if ((ets_is_ptr ||
                is_single_level_pointer_to_member(etsp->flags)) &&
#ifndef __EDG_IA64_ABI
#if ABI_CHANGES_FOR_RTTI
               type_info->unique_id != NULL &&
               type_info->unique_id == &MANGLED_NAME_OF_UNIQUE_ID_OF_NULLPTR
#else /* !ABI_CHANGES_FOR_RTTI */
               type_info == &MANGLED_NAME_OF_NULLPTR
#endif /* !ABI_CHANGES_FOR_RTTI */
#else /* ifdef __EDG_IA64_ABI */
               *type_info == typeid(decltype(nullptr))
#endif /* ifdef __EDG_IA64_ABI */
                                                                     ) {
      /* A thrown std::nullptr_t matches a pointer or pointer to member
         type. */
      match = TRUE;
      if (nullptr_conv_needed != NULL) *nullptr_conv_needed = TRUE;
#endif /* ifdef __EDG_CPP11_IL_EXTENSIONS_SUPPORTED */
    } else if (ets_is_ptr != is_ptr) {
      /* One is a pointer and the other is not.  This can't be a match. */
    } else if (!qualifiers_acceptable(etsp->flags, flags)) {
      /* A pointer is being thrown to a catch without appropriate qualifiers.
         This is not a match.  This check only tests the lowest bottom level
         of qualifiers and used for the match of a conversion to void* and
         derived to base conversions below. */
#if ABI_CHANGES_FOR_RTTI
    } else if (
#ifndef __EDG_IA64_ABI
               etsp->type_info->unique_id != NULL &&
               etsp->type_info->unique_id ==
                                         &MANGLED_NAME_OF_UNIQUE_ID_OF_VOID &&
#else /* ifdef __EDG_IA64_ABI */
               matching_type_info(etsp->type_info, &typeid(void)) &&
#endif /* ifdef __EDG_IA64_ABI */
               (ets_is_ptr == is_ptr) && ets_is_single_ptr) {
      /* The exception type specification is a void * and the object
         being thrown is some kind of pointer.  This is a match. */
      match = TRUE;
#else /* !ABI_CHANGES_FOR_RTTI */
    } else if (etsp->type_info == &MANGLED_NAME_OF_VOID &&
               (ets_is_ptr == is_ptr) && ets_is_single_ptr) {
      /* The exception type specification is a void * and the object
         being thrown is some kind of pointer.  This is a match. */
      match = TRUE;
#endif /* !ABI_CHANGES_FOR_RTTI */
    } else if ((!is_ptr ||
               (is_single_ptr && ets_is_single_ptr)) &&
#ifndef __EDG_IA64_ABI
	       type_info->base_class_entries != NULL &&
#else /* ifdef __EDG_IA64_ABI */
               (typeid(*type_info) == typeid(abi::__si_class_type_info) ||
                typeid(*type_info) == typeid(abi::__vmi_class_type_info)) &&
#endif /* ifdef __EDG_IA64_ABI */
	       __derived_to_base_conversion(object_ptr, &new_ptr, type_info,
					    etsp->type_info,
					    &local_access_flags,
                                            use_access_flags)) {
      /* A base class of the class that was thrown.  If the base class
	 is ambiguous or inaccessible then the base class flag will not
         be set.  The pointer is converted from a pointer to the derived 
	 class to a pointer to the base class.  Object_ptr will be NULL
	 when this routine is call to check throw specifications and no
	 object is involved, in which case the pointer conversion will not be
	 done, but derived_to_base_conversion will return TRUE to indicate
	 that such a conversion is possible. */
      match = TRUE;
      /* If a derived to base conversion was done, update the object pointer
	 to point to the base class. */
      if (object_ptr != NULL) *object_ptr = new_ptr;
#if DEBUG
      if (object_ptr != NULL && *object_ptr != NULL) {
        if (__debug_level >= 3) {
          if (orig_ptr != *object_ptr) {
            fprintf(__f_debug, "Orig ptr=%p, new ptr=%p\n", orig_ptr,
                    *object_ptr);
          }  /* if */
        }  /* if */
      }  /* if */
#endif /* DEBUG */
    }  /* if */
    if (match) {
      result = index;
     *etsp_found = etsp;
      break;
    }  /* if */
    done = etsp->flags & ETS_LAST;
    etsp++;
  } while (!done);
  return result;
}  /* check_exception_type_specifications */


static void destroy_thrown_object(a_throw_stack_entry_ptr	tsep)
/*
Call the destructor for the copy of the object created by the runtime and
indicate that the throw stack entry may be discarded when it reaches the
top of the throw stack.
*/
{
  void*				object_address;
  a_throw_stack_entry_ptr	primary_tsep;

  /* If this is a rethrow, get a pointer to the throw stack entry associated
     with the original throw. */
  primary_tsep = tsep->is_rethrow ? tsep->primary_entry : tsep;
  if (!tsep->discard_entry) {
    /* If this is the first time the routine has been called for this entry,
       set the discard flag and decrement the use count. */
    tsep->discard_entry = TRUE;
    /* __exception_caught is not called for internal rethrows, so this
       routine gets called for the eventual catch and needs to process the
       throw stack entry for both the rethrow and the original throw. */
    if (tsep->is_internal) primary_tsep->discard_entry = TRUE;
    check_assertion(primary_tsep->use_count > 0);
    primary_tsep->use_count--;
  }  /* if */
#if DEBUG
  if (__debug_level >= 6) {
    db_throw_stack("at start of destroy_thrown_object");
    fprintf(__f_debug, "Possibly destroying object associated with tsep %p\n",
            tsep);
    fprintf(__f_debug, "  primary_tsep->use_count=%d\n",
            (int)primary_tsep->use_count);
  }  /* if */
#endif /* DEBUG */
  /* If the entry can be destroyed, and the destructor has not already been
     called, then call it now. */
  if (primary_tsep->use_count == 0 && !primary_tsep->dtor_called) {
    /* Call the destructor for the object if needed. */
    primary_tsep->dtor_called = TRUE;
    object_address = primary_tsep->object_address;
    if (primary_tsep->object_copy_complete &&
        !is_pointer(primary_tsep->flags, primary_tsep->ptr_flags)) {
#if DEBUG
      if (__debug_level >= 4) {
        fprintf(__f_debug, "Destroying object at %p\n", object_address);
      }  /* if */
#endif /* DEBUG */
      a_destructor_ptr	dtor_ptr;
      dtor_ptr = (a_destructor_ptr)primary_tsep->destructor;
      if (dtor_ptr != NULL) {
#ifndef __EDG_IA64_ABI
        (dtor_ptr)(object_address, 2);
#else /* ifdef __EDG_IA64_ABI */
        (dtor_ptr)(object_address);
#endif /* ifdef __EDG_IA64_ABI */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* destroy_thrown_object */


EXTERN_C void __exception_started()
/*
Marks the point at which an exception that is thrown is considered
"uncaught".  This is the point after the evaluation of the thrown
object, but before the object is copied to the EH temporary.  If the
copy to the temporary is elided, this point is after both the evaluation
and the copy that is integrated into the evaluation.
*/
{
  a_throw_stack_entry_ptr	tsep = curr_throw_stack_entry;

  /* Link the throw processing marker onto the EH stack. */
  tsep->throw_marker.next = __curr_eh_stack_entry;
  __curr_eh_stack_entry = &tsep->throw_marker;
  tsep->object_evaluation_complete = TRUE;
}  /* __exception_started */


EXTERN_C void __exception_caught(void)
/*
Unlink the throw marker entry from the EH stack.  This is called after
the catch parameter has been copied.
*/
{
  check_assertion(__curr_eh_stack_entry->kind ==
                  ehsek_throw_processing_marker);
#if DEBUG
  if (__debug_level >= 6) {
    db_throw_stack("at start of __exception_caught");
  }  /* if */
#endif /* DEBUG */
  __curr_eh_stack_entry = __curr_eh_stack_entry->next;
}  /* __exception_caught */


EXTERN_C void __throw(void)
/*
Process a throw.  This routine looks through the stack entries for
a try block with a catch that matches the type of the object thrown.
*/
{
  an_eh_stack_entry_ptr		ehsep;
  an_eh_stack_entry_ptr		destination_ehsep = NULL;
#if !UNWIND_STACK_BEFORE_CALLING_TERMINATE
  an_eh_stack_entry_ptr		non_internal_destination_ehsep = NULL;
#endif /* !UNWIND_STACK_BEFORE_CALLING_TERMINATE */
  int				destination_catch_value;
  void*				object_ptr;
  void*				object_buffer_ptr;
  a_type_info_impl_ptr		thrown_type_info;
  an_ETS_flag_set		throw_flags;
  an_ETS_flag_set		*throw_ptr_flags;
  an_exception_type_specification_ptr
				etsp_found = NULL;
  a_boolean			nullptr_conv_needed = FALSE;
  an_access_flag_string         access_flags;
  a_boolean			use_access_flags;

  if (!curr_throw_stack_entry->object_evaluation_complete) {
    /* If the __exception_started routine was not explicitly called by the
       code generated at the throw site, call it now. */
    __exception_started();
  }  /* if */
  /* When __throw is called we know that the object has been copied and
     must be destroyed when the throw stack entry is popped. */
  curr_throw_stack_entry->object_copy_complete = TRUE;
  /* Get the information about the current thrown object from the
     throw stack. */
  thrown_type_info = curr_throw_stack_entry->type_info;
  throw_flags = curr_throw_stack_entry->flags;
  throw_ptr_flags = curr_throw_stack_entry->ptr_flags;
  access_flags = curr_throw_stack_entry->access_flags;
  use_access_flags = curr_throw_stack_entry->use_access_flags;
  /* If the throw object is a pointer we copy the pointer into a separate
     buffer whose address is passed to the catch.  This is done because
     the pointer may undergo a conversion (such as derived to base) and we
     need to preserve the original pointer in case it is needed by a
     rethrow. */
  if (is_pointer(throw_flags, throw_ptr_flags)) {
    /* It is a pointer.  object_buffer_ptr points to the special pointer
       buffer in the throw stack.  object_ptr contains the value of the
       pointer. */
    object_buffer_ptr = curr_throw_stack_entry->object_address;
    object_ptr = *(void**)object_buffer_ptr;
    object_buffer_ptr = (void*)&curr_throw_stack_entry->pointer_buffer;
  } else {
    /* It is not a pointer.  object_buffer_ptr points to the original copy
       of the object.  object_ptr points to the object buffer. */
    object_buffer_ptr = curr_throw_stack_entry->object_address;
    object_ptr = object_buffer_ptr;
  }  /* if */
#if DEBUG
  if (__debug_level >= 1) {
    fprintf(__f_debug, "__throw called\n");
  }  /* if */
#endif /* DEBUG */
  /* Get the address of the thrown object. */
  /* Find the try block that can catch the object being thrown. */
  ehsep = __curr_eh_stack_entry;
  check_assertion(ehsep == &curr_throw_stack_entry->throw_marker);
  /* Skip past the throw marker entry. */
  ehsep = ehsep->next;
  while (ehsep != NULL) {
    an_eh_stack_entry_kind	kind = ehsep->kind;
#if DEBUG
     if (__debug_level >= 2) {
       fprintf(__f_debug, "Pass 1 processing EH stack entry at %p, kind=%d\n",
               (void *)ehsep, kind);
       if (__debug_level >= 5) {
         db_eh_stack_entry(ehsep);
       }  /* if */
     }  /* if */
#endif /* DEBUG */
    if (kind == (an_eh_stack_entry_kind)ehsek_function) {
      /* Do nothing with function blocks at this time. */
    } else if (kind == (an_eh_stack_entry_kind)ehsek_vec_new_or_delete) {
      /* Do nothing with vec_new and vec_delete entries at this time. */
    } else if (kind == (an_eh_stack_entry_kind)ehsek_old_try_block ||
               kind == (an_eh_stack_entry_kind)ehsek_try_block) {
      if (ehsep->variant.try_block.catch_info == NULL) {
        /* Skip over try blocks for which a catch is active. */
        int result;
        if (ehsep->variant.try_block.catch_entries != NULL) {
          /* A normal (i.e., non-internal) try block.  See if any of the
            catch handlers match the object thrown. */
          result = check_exception_type_specifications
				(ehsep->variant.try_block.catch_entries,
				 thrown_type_info, throw_flags,
                                 throw_ptr_flags, access_flags,
				 use_access_flags, &object_ptr, &etsp_found,
                                 &nullptr_conv_needed);
        } else {
          /* An internal try block, which has no catch entries.  An internal
             try block is equivalent to a "catch (...)".  Set result to 1 to
             indicate that this is a valid handler for the throw.  The value
             of result will not be used in the handler code. */
          result = 1;
        }  /* if */
        if (result != 0) {
          /* A matching try block was found.  This could be an regular try
             block or an "internal" try block that is generated as part of
             the EH cleanup code for things like placement new operations
             (with matching placement delete functions).  Internal try
             blocks are treated like normal try blocks in all respects
             except that they are not considered a matching handler for
             purposes of determining whether or not terminate() should be
             called. */
          if (destination_ehsep == NULL) {
            destination_ehsep = ehsep;
            destination_catch_value = result;
          }  /* if */
          if (ehsep->variant.try_block.catch_entries != NULL) {
            /* This is a normal (i.e., not an internal) try block. */
#if !UNWIND_STACK_BEFORE_CALLING_TERMINATE
            non_internal_destination_ehsep = ehsep;
#endif /* !UNWIND_STACK_BEFORE_CALLING_TERMINATE */
            break;
          }  /* if */
        }  /* if */
      }  /* if */
    } else if (destination_ehsep != NULL) {
      /* Once a matching try block has been found, disregard any subsequent
         throw specification entries or throw processing markers that might
         be found.   The only other entries that are considered are
         non-internal try blocks to see if a matching handler can be found. */
      ehsep = ehsep->next;
      continue;
    } else if (kind == (an_eh_stack_entry_kind)ehsek_throw_spec) {
      /* Check for violations of throw specifications.  If a throw
         specification is violated we clean up until we reach the
         violated throw specification and then call unexpected.
         If result is zero, no match was found.  If there is an empty
         specification then, by definition, no match is found. */
      int	result = 0;
      if (ehsep->variant.throw_specification != NULL) {
        an_exception_type_specification_ptr	dummy_etsp;
        result = check_exception_type_specifications
				  (ehsep->variant.throw_specification,
				   thrown_type_info, throw_flags,
                                   throw_ptr_flags, access_flags,
				   use_access_flags, (void**)NULL,
                                   &dummy_etsp, (a_boolean*)NULL);
      }  /* if */
      if (result == 0) {
        destination_ehsep = ehsep;
        break;
      }  /* if */
    } else if (kind == (an_eh_stack_entry_kind)ehsek_noexcept) {
      /* Check for violations of noexcept specifications.  If a noexcept
         specification is violated we clean up until we reach the violated
         noexcept specification and then call terminate.  The mere presence
         of a noexcept stack entry indicates that the function does not
         allow exceptions to be thrown. */
      destination_ehsep = ehsep;
      break;
    } else if (kind == (an_eh_stack_entry_kind)ehsek_throw_processing_marker) {
      /* This entry is put on the stack before object cleanup begins.  If
         we find this marker it means that a destructor threw an
         exception that was not handled within the destructor.  The EH
         stack entry should point to the throw processing marker when
         exception_caught is called. */
      __curr_eh_stack_entry = ehsep;
      /* Indicate that the current thrown object is now in a handler.  This
         makes the object eligible for a rethrow. */
      curr_throw_stack_entry->in_handler = TRUE;
      __exception_caught();
      __call_terminate();
    } else {
      unexpected_condition();
    }  /* if */
    ehsep = ehsep->next;
  }  /* while */
#if !UNWIND_STACK_BEFORE_CALLING_TERMINATE
  /* If no matching (non-internal) handler was found, call terminate. */
  if (non_internal_destination_ehsep == NULL) {
      /* Indicate that the current thrown object is now in a handler.  This
         makes the object eligible for a rethrow. */
      curr_throw_stack_entry->in_handler = TRUE;
    __exception_caught();
    __call_terminate();
  }  /* if */
#endif /* !UNWIND_STACK_BEFORE_CALLING_TERMINATE */
  /* Go through the EH stack again and do any necessary cleanup. */
  ehsep = __curr_eh_stack_entry;
  /* Skip past the throw marker entry. */
  ehsep = ehsep->next;
  while (ehsep != destination_ehsep) {
    an_eh_stack_entry_kind	kind = ehsep->kind;
#if DEBUG
     if (__debug_level >= 2) {
       fprintf(__f_debug, "Pass 2 processing EH stack entry at %p, kind=%d\n",
               (void *)ehsep, kind);
       if (__debug_level >= 5) {
         db_eh_stack_entry(ehsep);
       }  /* if */
     }  /* if */
#endif /* DEBUG */
    if (kind == (an_eh_stack_entry_kind)ehsek_function) {
      cleanup(ehsep, __eh_curr_region, NULL_REGION_NUMBER);
      __eh_curr_region = ehsep->variant.function.saved_region_number;
    } else if (kind == (an_eh_stack_entry_kind)ehsek_vec_new_or_delete) {
      /* A vec_new or vec_delete operation that was in process when the
         exception occurred.  Call the routine to cleanup the partially
         constructed or destructed array. */
      __cleanup_vec_new_or_delete(ehsep);
    } else if (kind == (an_eh_stack_entry_kind)ehsek_old_try_block) {
      /* A try block that is being skipped. */
      if (ehsep->variant.try_block.catch_info != NULL) {
        /* A catch clause associated with this try block is currently
	   being processed.  Because this try block is being bypassed the
           throw entry is no longer needed.  It cannot be discarded yet
           because the thrown objects are allocated using a stack.  Call
           the destructor for the object and set a flag that this entry
           should be discarded when it reaches the top of the stack.  The
           "old try block" entry is generated for ABI versions up to
           3.10.  In newer ABI versions a cleanup entry is generated that
           causes __destroy_exception_object to be called at the appropriate
           time. */
        a_throw_stack_entry_ptr	tsep;
        tsep = (a_throw_stack_entry_ptr)ehsep->variant.try_block.catch_info;
        destroy_thrown_object(tsep);
      }  /* if */
    } else if (kind == (an_eh_stack_entry_kind)ehsek_try_block) {
      /* Make sure that nearest_enclosing_try_block is reset (it likely points
         to automatic storage that will become invalid after the longjmp). */
      a_throw_stack_entry_ptr	tsep;
      for (tsep = curr_throw_stack_entry; tsep != NULL; tsep = tsep->next) {
        if (tsep->nearest_enclosing_try_block == ehsep) {
          tsep->nearest_enclosing_try_block = NULL;
        }  /* if */
      }  /* for */
    } else if (kind == (an_eh_stack_entry_kind)ehsek_throw_spec) {
      /* Do nothing. */
    } else if (kind == (an_eh_stack_entry_kind)ehsek_throw_processing_marker) {
      /* Do nothing. */
    } else if (kind == (an_eh_stack_entry_kind)ehsek_noexcept) {
      /* This should not occur (an ehsek_noexcept stack frame can only
         be the final destination EH stack frame). */
      unexpected_condition();
    } else {
      unexpected_condition();
    }  /* if */
    ehsep = ehsep->next;
  }  /* while */
#if UNWIND_STACK_BEFORE_CALLING_TERMINATE
  /* If no handler was found call the terminate function.  Note that this
     tests "destination_ehsep" instead of "non_internal_destination_ehsep".
     When an internal try block is the matching handler, it should be used
     to do the necessary cleanup even when no "real" matching handler is
     found.  When the internal try block does its rethrow, the rethrow will
     result in a call to terminate() when no matching handler is found. */
  if (destination_ehsep == NULL) {
    /* Indicate that the current thrown object is now in a handler.  This
       makes the object eligible for a rethrow. */
    curr_throw_stack_entry->in_handler = TRUE;
    __exception_caught();
    __call_terminate();
  }  /* if */
#endif /* UNWIND_STACK_BEFORE_CALLING_TERMINATE */
  if (destination_ehsep->kind == (an_eh_stack_entry_kind)ehsek_old_try_block ||
      destination_ehsep->kind == (an_eh_stack_entry_kind)ehsek_try_block) {
    /* A try block may have objects that must be cleaned up before
       transferring control to one of the catch clauses.  This is determined
       by comparing the current region number with the region number in
       the try block.  If they are different then some objects must be
       cleaned up.  Call the cleanup routine to cleanup objects until we
       reach the region number indicated by the value in the try block. */
    if (destination_ehsep->variant.try_block.region_number !=
							 __eh_curr_region) {
      /* Find the function entry that contains the cleanup information. */
      an_eh_stack_entry_ptr	function_ehsep = destination_ehsep->next;
      while (function_ehsep->kind != (an_eh_stack_entry_kind)ehsek_function) {
        function_ehsep = function_ehsep->next;
      }  /* while */
      cleanup(function_ehsep, __eh_curr_region,
              destination_ehsep->variant.try_block.region_number);
      /* Restore the region number to the appropriate value for entry to the
         catch clause. */
      __eh_curr_region = destination_ehsep->variant.try_block.region_number;
    }  /* if */
  }  /* if */

  /* Update the throw processing marker so that its "next" entry points
     to the appropriate location after all cleanup actions have taken
     place. */
  check_assertion(__curr_eh_stack_entry ==
                  &curr_throw_stack_entry->throw_marker);
  __curr_eh_stack_entry->next = destination_ehsep;
  /* Indicate that the current thrown object is now in a handler.  This makes
     the object eligible for a rethrow. */
  curr_throw_stack_entry->in_handler = TRUE;
  if (destination_ehsep->kind == (an_eh_stack_entry_kind)ehsek_old_try_block ||
      destination_ehsep->kind == (an_eh_stack_entry_kind)ehsek_try_block) {
    a_boolean	exception_caught = FALSE;
    __catch_clause_number = destination_catch_value;
    if (is_pointer(throw_flags, throw_ptr_flags)) {
      /* The throw object is a pointer that may have underdone some
         kind of conversion such as a derived to base conversion.  Save
         the updated pointer.  Note that object_buffer_ptr has already
         been modified to point to a separate buffer so that the original
         pointer is preserved in case it is needed by a rethrow. */
      *(void**)object_buffer_ptr = object_ptr;
      __caught_object_address = object_buffer_ptr;
    } else if (nullptr_conv_needed) {
      /* The thrown object is nullptr and the catch is of a pointer or
         pointer-to-member.  Create a null pointer of the appropriate
         kind and pass the address of that object. */
      if (is_pointer(etsp_found->flags, etsp_found->ptr_flags)) {
        /* The nullptr is being caught by a normal pointer. */
        curr_throw_stack_entry->pointer_buffer = NULL;
        __caught_object_address = &curr_throw_stack_entry->pointer_buffer;
      } else if ((etsp_found->flags & ETS_IS_POINTER_TO_DATA_MEMBER) != 0) {
        /* The nullptr is being caught by a pointer-to-data-member. */
        curr_throw_stack_entry->ptr_to_data_member_buffer = NULL;
        __caught_object_address =
                     (void*)&curr_throw_stack_entry->ptr_to_data_member_buffer;
      } else {
        /* The nullptr is being caught by a pointer-to-member-function. */
        curr_throw_stack_entry->ptr_to_member_function_buffer = NULL;
        __caught_object_address =
            (void*)&curr_throw_stack_entry->ptr_to_member_function_buffer;
      }  /* if */
    } else {
      /* The thrown object is not a pointer.  object_ptr starts out with the
         same value as object_buffer_ptr but may be modified my a
         derived to base conversion.  It still points somewhere within
         the object buffer, however. */
      __caught_object_address = object_ptr;
    }  /* if */
    /* Update the pointer in the try block to point to the throw stack entry
       for the thrown object. */
    destination_ehsep->variant.try_block.catch_info =
                                               (void*)curr_throw_stack_entry;
#if ABI_COMPATIBILITY_VERSION < 235
    /* Starting with ABI version 2.35, __exception_caught is called by
       __internal_rethrow at the conclusion of the catch clause associated
       with an internal try block. */
    if (destination_ehsep->variant.try_block.catch_entries == NULL) {
      /* For an internal try block an exception is considered caught as soon
         as the handler is started (because there is no copy constructor to
         be called to initialize the catch parameter).  Mark the exception
         as caught now. */
      exception_caught = TRUE;
    }  /* if */
#endif /* ABI_COMPATIBILITY_VERSION < 235 */
#if ABI_COMPATIBILITY_VERSION < 233
    /* ABI versions earlier than 2.33 don't include calls to the
       __exception_caught routine.  Call it explicitly here.  This
      is equivalent to the old behavior. */
    exception_caught = TRUE;
#endif /* ABI_COMPATIBILITY_VERSION < 233 */
    if (exception_caught) {
      /* Mark the exception as caught now, if appropriate. */
      __exception_caught();
    }  /* if */
    longjmp(destination_ehsep->variant.try_block.setjmp_buffer, 1);
  } else if (destination_ehsep->kind ==
                                (an_eh_stack_entry_kind)ehsek_throw_spec) {
    /* A destination stack entry indicates that a throw specification was
       violated.  Call unexpected.  The EH stack should point to the
       entry for the exception specification that was violated.  Remove
       the throw processing marker from the stack. */
    __curr_eh_stack_entry = __curr_eh_stack_entry->next;
#if !ABI_CHANGES_FOR_RTTI
   /* When RTTI is not used, the old semantics of unexpected() are used in
      which the type of an object thrown by unexpected is not checked against
      the violated exception specification.  Remove the EH stack entry
      for the violated throw specification. */
   __curr_eh_stack_entry = __curr_eh_stack_entry->next;
#endif /* ABI_CHANGES_FOR_RTTI */
    __call_unexpected();
  } else if (destination_ehsep->kind ==
                                      (an_eh_stack_entry_kind)ehsek_noexcept) {
    /* A destination stack entry indicates that a noexcept specification was
       violated.  Call terminate.  The EH stack should point to the
       entry for the exception specification that was violated.  Remove
       the throw processing marker from the stack. */
    __curr_eh_stack_entry = __curr_eh_stack_entry->next;
    __call_terminate();
  }  /* if */
}  /* __throw */


static void push_throw_stack(a_type_info_impl_ptr    type_info,
			     a_destructor_ptr	     destructor,
			     an_ETS_flag_set	     flags,
			     an_ETS_flag_set	     *ptr_flags,
                             an_access_flag_string   access_flags,
                             a_boolean               use_access_flags,
			     void*		     object_address,
			     a_boolean		     is_rethrow,
			     a_boolean		     is_internal,
                             a_throw_stack_entry_ptr primary_entry)
/*
Push an entry onto the throw stack and initialize its fields.
*/
{
  a_throw_stack_entry_ptr	tsep;
  an_eh_stack_entry_ptr		ehsep;

  tsep =
      (a_throw_stack_entry_ptr)eh_alloc_on_stack(sizeof(a_throw_stack_entry));
  /* Record a pointer to the nearest enclosing try block in the throw
     stack entry.  If this throw has the same nearest enclosing try block
     as the previous throw then the previous throw should be discarded.
     This can occur if a throw is done from a copy constructor called
     after __throw_alloc but before __throw. */
  ehsep = __curr_eh_stack_entry;
  while (ehsep != NULL) {
    /* Try blocks that are currently inside a handler are not considered. */
    if ((ehsep->kind == (an_eh_stack_entry_kind)ehsek_old_try_block ||
         ehsep->kind == (an_eh_stack_entry_kind)ehsek_try_block) &&
        ehsep->variant.try_block.catch_info == NULL) break;
    ehsep = ehsep->next;
  }  /* while */
  tsep->nearest_enclosing_try_block = ehsep;
  if (curr_throw_stack_entry != NULL) {
    if (curr_throw_stack_entry->nearest_enclosing_try_block == ehsep) {
      /* There is a previous throw and it does point to the same nearest
         enclosing try block. */
      destroy_thrown_object(curr_throw_stack_entry);
    }  /* if */
  }  /* if */
  tsep->next = curr_throw_stack_entry;
  curr_throw_stack_entry = tsep;
  tsep->type_info = type_info;
  tsep->destructor = destructor;
  tsep->flags = flags;
  tsep->ptr_flags = ptr_flags;
  tsep->access_flags = access_flags;
  tsep->use_access_flags = use_access_flags;
  tsep->object_address = object_address;
  tsep->pointer_buffer = NULL;
  tsep->primary_entry = primary_entry;
  tsep->use_count = 0;
  /* If this is a rethrow, increment the use count of the primary entry.
     Otherwise, increment the use count of this entry. */
  if (is_internal) {
    /* An internal try block.  Don't increment the use count. */
  } else if (is_rethrow) {
    primary_entry->use_count++;
  } else {
    tsep->use_count++;
  }  /* if */
  tsep->is_rethrow = is_rethrow;
  tsep->is_internal = is_internal;
  tsep->dtor_called = FALSE;
  tsep->discard_entry = FALSE;
  tsep->in_handler = FALSE;
  tsep->object_copy_complete = FALSE;
  tsep->object_evaluation_complete = FALSE;
  tsep->throw_marker.next = NULL;
  tsep->throw_marker.kind = ehsek_throw_processing_marker;
}  /* push_throw_stack */


static void rethrow_full(a_boolean	is_internal)
/*
Rethrow the current thrown object.  is_internal is TRUE for an internal
rethrow.
*/
{
  a_throw_stack_entry_ptr	tsep = curr_throw_stack_entry;

  /* Find the throw stack entry for the throw currently being handled. */
  for (; tsep != NULL; tsep = tsep->next) {
    if (tsep->in_handler && !tsep->is_rethrow) break;
  }  /* for */
  if (tsep == NULL) {
    /* No handler is currently active. */
    __call_terminate();
  }  /* if */
  push_throw_stack(tsep->type_info,
                   tsep->destructor,
		   tsep->flags,
                   tsep->ptr_flags,
		   tsep->access_flags,
		   tsep->use_access_flags,
		   tsep->object_address,
		   /*is_rethrow=*/TRUE,
                   is_internal,
                   tsep);
  __throw();
}  /* rethrow_full */


EXTERN_C void __rethrow(void)
/*
Rethrow the current thrown object.
*/
{
  rethrow_full(/*is_internal=*/FALSE);
}  /* __rethrow */


#if ABI_COMPATIBILITY_VERSION >= 235
EXTERN_C void __internal_rethrow(void)
/*
Entry point to rethrow used by internal try blocks.  This routine simply
calls __exception_caught to mark the throw as complete, then does a normal
rethrow.
*/
{
  __exception_caught();
  rethrow_full(/*is_internal=*/TRUE);
}  /* __internal_rethrow */
#endif /* ABI_COMPATIBILITY_VERSION >= 235 */


/*
__throw_alloc is called for ABI versions that do not include RTTI.
When RTTI is supported, __throw_setup is called for ABI versions up to
and including 2.37, and in later ABI versions for types that have no
destructor.  The 2.38 ABI passes the destructor pointer to
__throw_setup_dtor, and removes it from the type_info_impl structure
to fix some corner cases in which a destructor is required solely for
the purpose of creating a type_info_impl object.  The 2.41 ABI uses
__throw_setup_ptr when a multi-level pointer is passed.  This is used
to supply additional information used for qualification conversions.
*/

#if ABI_COMPATIBILITY_VERSION >= 241

EXTERN_C void* __throw_setup_ptr(a_type_info_impl_ptr  type_info,
  			          a_sizeof_t	        size,
			          an_ETS_flag_set	*ptr_flags)
/*
Allocate space for the object to be thrown and save information about
the type being thrown.  This is like __throw_setup, except that the
a pointer to an array of ETS flags for multi-level pointers is passed.
*/
{
  void*				object_address;

  object_address = (void *)eh_alloc_on_stack(size);
  push_throw_stack(type_info, (a_destructor_ptr)NULL, ETS_NO_FLAGS,
                   ptr_flags, (an_access_flag_string)NULL,
	           /*use_access_flags=*/FALSE, object_address,
		   /*is_rethrow=*/FALSE,
                   /*is_internal=*/FALSE,
                   (a_throw_stack_entry_ptr)NULL);
  return object_address;
}  /* __throw_setup_ptr */

#endif /* ABI_COMPATIBILITY_VERSION >= 241 */

#if ABI_CHANGES_FOR_RTTI
EXTERN_C void* __throw_setup(a_type_info_impl_ptr  type_info,
  			     a_sizeof_t	           size,
			     an_ETS_flag_set	   ets_flags)
/*
Allocate space for the object to be thrown and save information about
the type being thrown.  This is like __throw_alloc, except no access_flags
are provided.
*/
{
  void*				object_address;
  a_destructor_ptr		destructor;

  /* For ABI versions up to and including 2.37, the destructor pointer,
     if any, is stored in the type_info_impl object.  After 2.37, this
     routine is only used for objects without destructors. */
#if ABI_COMPATIBILITY_VERSION <= 237
  destructor = type_info->destructor;
#else /* !ABI_COMPATIBILITY_VERSION <= 237 */
  destructor = (a_destructor_ptr)NULL;
#endif /* ABI_COMPATIBILITY_VERSION <= 237 */
  object_address = (void *)eh_alloc_on_stack(size);
  push_throw_stack(type_info, destructor, ets_flags, (an_ETS_flag_set*)NULL,
                   (an_access_flag_string)NULL,
	           /*use_access_flags=*/FALSE, object_address,
		   /*is_rethrow=*/FALSE,
                   /*is_internal=*/FALSE,
                   (a_throw_stack_entry_ptr)NULL);
  return object_address;
}  /* __throw_setup */

#if ABI_COMPATIBILITY_VERSION >= 238

EXTERN_C void* __throw_setup_dtor(a_type_info_impl_ptr  type_info,
  			          a_sizeof_t	        size,
			          int			ets_flags,
				  a_destructor_ptr	destructor)
/*
Allocate space for the object to be thrown and save information about
the type being thrown.  This is like __throw_setup, except that the
destructor pointer is passed as a parameter instead of being fetched
from the type_info_impl structure.  ets_flags is passed as an int (and
not an_ETS_flag_set) because that is how it is passed by the code
generated by the front end.
*/
{
  void*				object_address;

  object_address = (void *)eh_alloc_on_stack(size);
  push_throw_stack(type_info, destructor, ets_flags, (an_ETS_flag_set*)NULL,
                   (an_access_flag_string)NULL,
	           /*use_access_flags=*/FALSE, object_address,
		   /*is_rethrow=*/FALSE,
                   /*is_internal=*/FALSE,
                   (a_throw_stack_entry_ptr)NULL);
  return object_address;
}  /* __throw_setup_dtor */

#endif /* ABI_COMPATIBILITY_VERSION >= 238 */

#else /* !ABI_CHANGES_FOR_RTTI */

EXTERN_C void* __throw_alloc(a_type_info_impl_ptr  type_info,
			     a_sizeof_t		   size,
			     int		   ets_flags,
			     an_access_flag_string access_flags)
/*
Allocate space for the object to be thrown and save information about
the type being thrown.  ets_flags is passed as an int (and not an_ETS_flag_set)
because that is how it is passed by the code generated by the front end.
*/
{
  void*				object_address;

  object_address = (void *)eh_alloc_on_stack(size);
  push_throw_stack(type_info, type_info->destructor, ets_flags,
                   (an_ETS_flag_set*)NULL, access_flags,
                   /*use_access_flags=*/TRUE,
		   object_address, /*is_rethrow=*/FALSE,
                   /*is_internal=*/FALSE,
                   (a_throw_stack_entry_ptr)NULL);
  return object_address;
}  /* __throw_alloc */
#endif /* ABI_CHANGES_FOR_RTTI */


EXTERN_C void __free_thrown_object(void)
/*
Free the space used to make the copy of the thrown object.  Called at
the completion of a catch clause.  This routine is called by the code
generated for catch clauses for ABI versions though version 3.10.  Newer
versions of the ABI call __destroy_exception_object.
*/
{
#if DEBUG
  if (__debug_level >= 6) {
    db_throw_stack("at start of free_thrown_object");
  }  /* if */
#endif /* DEBUG */
  check_assertion(curr_throw_stack_entry != NULL);
  destroy_thrown_object(curr_throw_stack_entry);
#if DEBUG
  if (__debug_level >= 6) {
    db_throw_stack("in free_thrown_object after destroy_thrown_object");
  }  /* if */
#endif /* DEBUG */
  /* Free any entries with the discard_entry flag set.  This always frees
     the top entry but may also free additional entries associated with
     pending catches that were later skipped over by a throw. */
  while (curr_throw_stack_entry != NULL &&
         curr_throw_stack_entry->discard_entry) {
    a_throw_stack_entry_ptr	tsep = curr_throw_stack_entry;
    a_boolean			is_rethrow = tsep->is_rethrow;
    void*			object_address = tsep->object_address;
    /* If this is not a rethrow, the destructor should have already been
       called. */
    check_assertion(is_rethrow || tsep->dtor_called);
    /* Unlink this entry from the throw stack. */
    curr_throw_stack_entry = tsep->next;
    /* Free the space used for the throw stack entry.  Note that the stack
       entry and the object must be freed in the reverse of the order
       in which they were allocated since this is a stack. */
    eh_free_on_stack(tsep);
    if (!is_rethrow) {
      /* Free the space used for the copy of the object. */
      eh_free_on_stack(object_address);
    }  /* if */
  }  /* while */
#if DEBUG
  if (__debug_level >= 3) {
    db_throw_stack("at end of free_thrown_object");
  }  /* if */
#endif /* DEBUG */
}  /* __free_thrown_object */


EXTERN_C void __destroy_exception_object(void)
/*
This routine is called by the cleanup mechanism when the exception object
for a given throw is to be destroyed if it is no longer in use.  This routine
is also called for code generated for catch clauses for ABI versions after
3.10.  This routine just calls __free_thrown_object in most cases except when
there are entries on the throw stack that need to be bypassed to find the
appropriate object to be destroyed.
*/
{
  a_throw_stack_entry_ptr	tsep;

  /* Find the top entry that is in a handler and for which the destructor
     has not yet been called and for which the discard entry flag is
     not yet set.  The discard entry test is used when this routine is called
     by the cleanup mechanism to skip over entries that have already been
     cleaned up. */
  for (tsep = curr_throw_stack_entry; tsep != NULL; tsep = tsep->next) {
    if (tsep->in_handler && !tsep->dtor_called && !tsep->discard_entry) break;
  }  /* for */
  check_assertion(tsep != NULL);
  /* If we are destroying the top entry on the stack, call
      __free_thrown_object, which will also free the memory for the entry.
      Otherwise, just destroy the object. */
  if (tsep == curr_throw_stack_entry) {
    __free_thrown_object();
  } else {
    destroy_thrown_object(tsep);
  }  /* if */
}  /* __destroy_exception_object */


EXTERN_C void __eh_exit_processing(void)
/*
Exit has been called.  Do any processing required to ensure that an
exception thrown by a static destructor or routine registered with
at_exit cannot throw beyond the exit call.
*/
{
  /* Clear the EH stack entry.  This will prevent a throw from finding
     a try block that was entered before exit was called. */
  __curr_eh_stack_entry = NULL;
}  /* __eh_exit_processing */


EXTERN_C void __suppress_optim_on_vars_in_try(void)
/*
Calls of this routine are generated when the C generating back end is
used.  It is used to make optimizers think that the addresses have been
taken of any local variables used inside a try block, thus ensuring that
their values will be saved when calling a routine inside the try block
that may throw an exception.  The routine is not supposed to actually
get called.
*/
{
  unexpected_condition();
}  /* __suppress_optim_on_vars_in_try */

EXTERN_C an_eh_stack_entry_ptr __get_curr_eh_stack_entry(void)
/*
Return a pointer to __get_curr_eh_stack_entry.
*/
{
  return __curr_eh_stack_entry;
}  /* __get_curr_eh_stack_entry */


EXTERN_C void __type_of_thrown_object(a_type_info_impl_ptr	*type,
				      an_ETS_flag_set		*flags,
				      an_ETS_flag_set		**ptr_flags)
/*
Return a pointer to the typeinfo entry for the type of the object that
was thrown and the flags associated with the thrown object.
*/
{
  check_assertion(curr_throw_stack_entry != NULL);
  *type = curr_throw_stack_entry->type_info;
  *flags = curr_throw_stack_entry->flags;
  *ptr_flags = curr_throw_stack_entry->ptr_flags;
}  /* __type_of_thrown_object */


EXTERN_C a_boolean __can_throw_type(a_type_info_impl_ptr	type,
				    an_ETS_flag_set		flags,
				    an_ETS_flag_set		*ptr_flags)
/*
This routine is called by the code that checks whether an exception thrown
by unexpected() violates the current exception specification.  Find the
innermost exception specification and check whether the specified type
and flag combination is allowed.
*/
{
  a_boolean		result = FALSE;
  an_eh_stack_entry_ptr	ehsep;

  ehsep = __curr_eh_stack_entry;
  for (ehsep = __curr_eh_stack_entry; ehsep != NULL; ehsep = ehsep->next) {
    if (ehsep->kind == (an_eh_stack_entry_kind)ehsek_throw_spec) break;
  }  /* for */
  check_assertion(ehsep != NULL);
  if (ehsep->variant.throw_specification != NULL) {
    an_exception_type_specification_ptr	dummy_etsp;
    int					catch_pos;
    catch_pos = check_exception_type_specifications
				  (ehsep->variant.throw_specification,
				   type, flags, ptr_flags,
                                   (an_access_flag_string)NULL,
				   /*use_access_flags=*/FALSE, (void**)NULL,
                                   &dummy_etsp, (a_boolean*)NULL);
    if (catch_pos != 0) result = TRUE;
  }  /* if */
  return result;
}  /* __can_throw_type */

#else /* !EXCEPTION_HANDLING */

EXTERN_C void __eh_exit_processing(void)
/*
A stub version of __eh_exit_processing that is used when the runtime is
built without exception handling support.  This version does nothing.
*/
{
}  /* __eh_exit_processing */

#endif /* EXCEPTION_HANDLING */
