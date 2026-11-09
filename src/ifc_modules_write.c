/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

ifc_modules_write.c -- IFC writing code.

*/

/* Header files common to all files. */
#include "fe_common.h"

#include <errno.h>

/* Additional header files. */
#include "ifc_modules.h"
#include "ifc_map_functions.h"
#include "ifc_modules_internal.h"
#include "il_def.h"
#include "il_walk.h"

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE


NORETURN static void ifc_write_catastrophe(
                               an_error_code reason = ec_unsupported_il_to_ifc)
/*
Issue a catastrophic diagnostic that an IFC file could not be created for the
current translation unit for the given reason and terminate the compilation.
This routine does not return.
*/
{
  a_diagnostic_ptr diag = start_catastrophe(ec_ifc_creation_failure);

  add_diag_info(diag, reason);
  end_diagnostic(diag);
  /* Avoid spurious warning.  The function above does not return. */
  exit_compilation(es_internal_error);
}  /* ifc_write_catastrophe */


using an_ifc_output_buffer = Dyn_array<a_byte, General_allocator>;
                        /* The type used to hold the in-memory bytes. */

namespace {

/*
This structure represents an IFC partition via a resizable in memory buffer.
*/
struct an_ifc_output_partition {
  an_ifc_output_partition(size_t part_name_offset_val,
                          size_t element_size_val)
    : part_name_offset(part_name_offset_val), element_size(element_size_val)
    {}

  inline size_t new_element(a_byte ***start, size_t *byte_offset);
  inline size_t new_elements(size_t num_nodes);
  inline void fetch_element(a_byte ***start,
                            size_t *byte_offset,
                            size_t index);

  a_byte* get_bytes()
    { return this->contents.begin(); }
  a_byte const * get_bytes() const
    { return this->contents.begin(); }
  size_t get_num_bytes() const
    { return this->contents.length(); }

  size_t get_num_elements() const
    { return this->get_num_bytes() / this->element_size; }

  const size_t  part_name_offset;
                        /* The offset into the string table where this
                           partition's name can be retrieved. */
  const size_t  element_size;
                        /* The number of bytes for an element. */
private:
  an_ifc_output_buffer
                contents = {};
                        /* The bytes held by the partition. */
  a_byte        *content_start = NULL;
                        /* A pointer to the start of the partition bytes. */
};  /* an_ifc_output_partition */


size_t an_ifc_output_partition::new_element(a_byte ***start,
                                            size_t *byte_offset)
/*
Construct a new IFC output node in the current partition.  *start is updated to
be a pointer to the pointer representing the start of the partition byte
buffer.  *byte_offset is updated to be the offset into the partition byte
buffer where the node starts.  Return the index into the partition.
*/
{
  size_t orig_size = this->contents.length();
  size_t capacity = this->contents.capacity();

  /* Check to see if this will push the array over capacity; if so, increase
     the capacity. */
  if (orig_size + this->element_size > capacity) {
    this->contents.reserve(capacity * 2);
  }  /* if */
  /* Insert the space for this new element. */
  this->contents.resize(orig_size + this->element_size, '\0');
  /* Update the content start pointer in case the buffer was reallocated. */
  this->content_start = this->contents.begin();
  *start = &this->content_start;
  *byte_offset = orig_size;
  return orig_size / this->element_size;
}  /* an_ifc_output_partition::new_element */


size_t an_ifc_output_partition::new_elements(size_t num_nodes)
/*
Construct the given number of new IFC output nodes in the current partition.
Return the index into the partition.
*/
{
  size_t orig_size = this->contents.length();
  size_t capacity = this->contents.capacity();
  size_t new_elements_size = num_nodes * this->element_size;


  /* Check to see if this will push the array over capacity; if so, increase
     the capacity. */
  if (orig_size + new_elements_size > capacity) {
    size_t reserve_amount = max_val(capacity * 2, new_elements_size);

    this->contents.reserve(reserve_amount);
  }  /* if */
  /* Insert the space for this new element. */
  this->contents.resize(orig_size + (this->element_size * num_nodes), '\0');
  /* Update the content start pointer in case the buffer was reallocated. */
  this->content_start = this->contents.begin();
  return orig_size / this->element_size;
}  /* an_ifc_output_partition::new_elements */


inline void an_ifc_output_partition::fetch_element(a_byte ***start,
                                                   size_t *byte_offset,
                                                   size_t index)
/*
Fetch the IFC output node in the current partition at the given index.  *start
is updated to be a pointer to the pointer representing the start of the
partition byte buffer.  *byte_offset is updated to be the offset into the
partition byte buffer where the node starts.
*/
{
  *start = &this->content_start;
  *byte_offset = index * this->element_size;
}  /* an_ifc_output_partition::fetch_element */


struct an_ifc_output_token_cache;


/*
This structure represents the IFC file we're building up in memory.
*/
struct an_ifc_output_state {
  an_ifc_output_state(an_ifc_module_file *output_file_val);
  ~an_ifc_output_state();

  inline an_ifc_module_file *get_file() const
    { return this->output_file; }

  inline size_t add_to_string_table(a_const_char *bytes, size_t num_bytes);
  inline size_t add_to_string_table(a_const_char *string_text);
  inline size_t add_to_string_table(a_string_view str);

  template<typename an_ifc_Node_type>
  inline size_t alloc_node(an_ifc_Node_type *result);
  template<typename an_ifc_Node_type>
  inline size_t alloc_node_block(size_t num_nodes);

  template<typename an_ifc_Node_type>
  inline an_ifc_chart_index alloc_chart(an_ifc_Node_type *result);
  template<typename an_ifc_Node_type>
  inline an_ifc_edg_constant_index alloc_constant(an_ifc_Node_type *result);
  template<typename an_ifc_Node_type>
  inline an_ifc_decl_index alloc_decl(an_ifc_Node_type *result);
  template<typename an_ifc_Node_type>
  inline void alloc_decl_trait(an_ifc_decl_index decl_idx,
                               an_ifc_Node_type  *result);
  template<typename an_ifc_Node_type>
  inline an_ifc_dir_index alloc_dir(an_ifc_Node_type *result);
  template<typename an_ifc_Node_type>
  inline an_ifc_type_index alloc_type(an_ifc_Node_type *result);
  template<typename an_ifc_Node_type>
  inline an_ifc_name_index alloc_name(an_ifc_Node_type *result);

  template<typename an_ifc_Node_type>
  inline an_ifc_edg_complex_token_index alloc_complex_token(
                                                     an_ifc_Node_type *result);
  inline an_ifc_edg_token_cache_offset alloc_token_cache(
                                const an_ifc_output_token_cache &output_cache);
  inline an_ifc_expr_index alloc_token_cache_expr(
                                const an_ifc_output_token_cache &output_cache);


  template<typename an_ifc_Node_type>
  inline void fetch_node(an_ifc_Node_type *result, size_t index);
  template<typename an_ifc_Node_type>
  inline void fetch_decl(an_ifc_Node_type *result, an_ifc_decl_index decl_idx);

  void set_global_scope(an_ifc_scope_offset scope_offset)
    { this->global_scope = scope_offset; }

  void sort_traits();

  void write();
private:
  template<typename an_ifc_Node_type>
  an_ifc_output_partition *get_or_init_partition();
  an_ifc_output_partition *get_partition(an_ifc_partition_kind kind)
    { return this->partitions[kind - 1]; }
  void set_partition(an_ifc_partition_kind   kind,
                     an_ifc_output_partition *value)
    { this->partitions[kind - 1] = value; }
  template<typename an_ifc_Node_type>
  void sort_trait_partition();
  an_ifc_module_file
                *output_file;
                        /* The target IFC module file. */
  an_ifc_scope_offset
                global_scope = {};
                        /* The global scope. */
  an_ifc_output_buffer
                string_table = {};
                        /* The string table's bytes. */
  an_ifc_unit_sort
                unit_sort = ifc_us_header;
                        /* The module unit kind. */
  a_boolean     internal_partition = FALSE;
                        /* TRUE if the module unit is mk_unit_partition and the
                           partition is an internal partition; otherwise,
                           FALSE.  */
  size_t        module_name_offset = 0;
                        /* The offset into the string table representing
                           the module name. */
  size_t        source_file_name_offset = 0;
                        /* The offset into the string table representing
                           the source file name. */
  an_ifc_output_partition
                *partitions[IFC_PARTITION_COUNT] = {};
                        /* An array of IFC output partition pointers
                           that are initialized as needed. */
};  /* an_ifc_output_state */


an_ifc_output_state::an_ifc_output_state(an_ifc_module_file *output_file_val)
/*
Construct a new IFC output state writing to the given IFC module file.
*/
  : output_file(output_file_val)
{
  /* Add a null character to the start of the string table so that a 0 value
     TextOffset results in a null string. */
  (void)this->add_to_string_table("", /*num_bytes=*/1);

  /* Add the source file name to the string table. */
  an_ifc_module_file_write_state &write_state =
                                          this->output_file->get_write_state();
  if (write_state.source_module != NULL) {
    a_module *src_mod = write_state.source_module;

    if (src_mod->kind == mk_unit) {
      this->unit_sort = ifc_us_primary;
    } else if (src_mod->kind == mk_unit_partition) {
      this->unit_sort = ifc_us_partition;
    }  /* if */
    if (src_mod->kind == mk_unit || src_mod->kind == mk_unit_partition) {
      a_string_view mod_name = module_name_of(src_mod);

      this->module_name_offset = this->add_to_string_table(mod_name);
    }  /* if */
    if (src_mod->variant.unit_partition.is_internal) {
      /* If this IL module represents an internal module partition, mark the
         output state as that of an internal module partition. */
      this->internal_partition = TRUE;
    }  /* if */
  }  /* if */
  if (write_state.source_file_name != NULL) {
    this->source_file_name_offset = this->add_to_string_table(
                                                 write_state.source_file_name);
  }  /* if */
}  /* an_ifc_output_state::an_ifc_output_state */


an_ifc_output_state::~an_ifc_output_state()
/*
Clean up the IFC output state.
*/
{
  for (size_t i = 0; i < IFC_PARTITION_COUNT; ++i) {
    delete_general(&(partitions[i]));
  }  /* for */
}  /* an_ifc_output_state::~an_ifc_output_state */


size_t an_ifc_output_state::add_to_string_table(a_const_char *bytes,
                                                size_t       num_bytes)
/*
Insert the given number of bytes into the string table.  Return the byte offset
into the string table where the string starts.
*/
{
  size_t start = this->string_table.length();

  this->string_table.insert(start, (a_byte*)bytes, num_bytes);
  return start;
}  /* an_ifc_output_state::add_to_string_table */


size_t an_ifc_output_state::add_to_string_table(a_const_char *string_text)
/*
Insert the given null terminated string into the string table.  Return the byte
offset into the string table where the string starts.
*/
{
  size_t start = this->string_table.length();
  size_t num_bytes = strlen(string_text) + 1;

  this->string_table.insert(start, (a_byte*)string_text, num_bytes);
  return start;
}  /* an_ifc_output_state::add_to_string_table */


size_t an_ifc_output_state::add_to_string_table(a_string_view str)
/*
Insert the given string into the string table.  Return the byte offset into the
string table where the string starts.

Note: A null terminator is always added ensuring at least one null terminator
terminates the string table.
*/
{
  size_t start = this->string_table.length();

  this->string_table.insert(start, (a_byte*)str.start(), str.length());
  this->string_table.push_back('\0');
  return start;
}  /* an_ifc_output_state::add_to_string_table */


template<typename an_ifc_Node_type>
size_t an_ifc_output_state::alloc_node(an_ifc_Node_type *result)
/*
Allocate a node in its corresponding output partition.  Set *result to the
allocated node.  Return the node's index into the partition.
*/
{
  an_ifc_output_partition
                *output_part = this->get_or_init_partition<an_ifc_Node_type>();
  a_byte        **start;
  size_t        byte_offset;
  size_t        index = output_part->new_element(&start, &byte_offset);
  an_ifc_Node_type
                constructed_value(this->output_file, start, byte_offset);
  *result = constructed_value;
  return index;
}  /* an_ifc_output_state::alloc_node */


template<typename an_ifc_Node_type>
size_t an_ifc_output_state::alloc_node_block(size_t num_nodes)
/*
Allocate the given number of nodes in their corresponding output partition.
Return the first node in the block's index into the partition.
*/
{
  size_t result = 0;

  if (num_nodes > 0) {
    an_ifc_output_partition
                *output_part = this->get_or_init_partition<an_ifc_Node_type>();

    result = output_part->new_elements(num_nodes);
  }  /* if */
  return result;
}  /* an_ifc_output_state::alloc_node_block */


template<typename an_ifc_Node_type>
an_ifc_chart_index an_ifc_output_state::alloc_chart(an_ifc_Node_type *result)
/*
Allocate a chart node in its corresponding output partition.  Set *result to
the allocated node.  Return the node's chart index.
*/
{
  an_ifc_partition_kind
                part_kind = get_ifc_partition_kind<an_ifc_Node_type>();
  size_t        part_offset = this->alloc_node(result);
  an_ifc_chart_sort
                chart_sort = to_chart_sort(part_kind);

  return an_ifc_chart_index(result->get_file(), chart_sort, part_offset);
}  /* an_ifc_output_state::alloc_chart */


template<typename an_ifc_Node_type>
an_ifc_edg_constant_index an_ifc_output_state::alloc_constant(
                                                      an_ifc_Node_type *result)
/*
Allocate a constant node in its corresponding output partition.  Set *result to
the allocated node.  Return the node's constant index.
*/
{
  an_ifc_partition_kind
                part_kind = get_ifc_partition_kind<an_ifc_Node_type>();
  size_t        part_offset = this->alloc_node(result);
  an_ifc_edg_constant_sort
                constant_sort = to_edg_constant_sort(part_kind);

  return an_ifc_edg_constant_index(result->get_file(), constant_sort,
                                   part_offset);
}  /* an_ifc_output_state::alloc_constant */


template<typename an_ifc_Node_type>
an_ifc_decl_index an_ifc_output_state::alloc_decl(an_ifc_Node_type *result)
/*
Allocate a declaration node in its corresponding output partition.  Set *result
to the allocated node.  Return the node's declaration index.
*/
{
  an_ifc_partition_kind
                part_kind = get_ifc_partition_kind<an_ifc_Node_type>();
  size_t        part_offset = this->alloc_node(result);
  an_ifc_decl_sort
                decl_sort = to_decl_sort(part_kind);

  return an_ifc_decl_index(result->get_file(), decl_sort, part_offset);
}  /* an_ifc_output_state::alloc_decl */


template<typename an_ifc_Node_type>
an_ifc_dir_index an_ifc_output_state::alloc_dir(an_ifc_Node_type *result)
/*
Allocate a directive node in its corresponding output partition.  Set *result
to the allocated node.  Return the node's directive index.
*/
{
  an_ifc_partition_kind
                part_kind = get_ifc_partition_kind<an_ifc_Node_type>();
  size_t        part_offset = this->alloc_node(result);
  an_ifc_dir_sort
                dir_sort = to_dir_sort(part_kind);

  return an_ifc_dir_index(result->get_file(), dir_sort, part_offset);
}  /* an_ifc_output_state::alloc_dir */


template<typename an_ifc_Node_type>
void an_ifc_output_state::alloc_decl_trait(an_ifc_decl_index decl_idx,
                                           an_ifc_Node_type  *result)
/*
Allocate a trait node for the given declaration in the corresponding trait
output partition.  Set *result to the allocated node.
*/
{
  (void)this->alloc_node(result);
  set_ifc_decl(result, decl_idx);
}  /* an_ifc_output_state::alloc_decl_trait */


template<typename an_ifc_Node_type>
an_ifc_type_index an_ifc_output_state::alloc_type(an_ifc_Node_type *result)
/*
Allocate a type node in its corresponding output partition.  Set *result to the
allocated node.  Return the node's type index.
*/
{
  an_ifc_type_index
                result_idx;
  an_ifc_partition_kind
                part_kind = get_ifc_partition_kind<an_ifc_Node_type>();
  size_t        part_offset = this->alloc_node(result);

  if (is_edg_type_sort(part_kind)) {
    an_ifc_edg_extension_type
                type_extension;
    size_t      extension_offset = this->alloc_node(&type_extension);
    an_ifc_edg_type_sort
                type_sort = to_edg_type_sort(part_kind);
    an_ifc_edg_type_index
                edg_type_index(result->get_file(), type_sort, part_offset);

    set_ifc_value(&type_extension, edg_type_index);
    result_idx = an_ifc_type_index(result->get_file(),
                                   ifc_ts_type_vendor_extension,
                                   extension_offset + 1);
  } else {
    an_ifc_type_sort type_sort = to_type_sort(part_kind);

    result_idx = an_ifc_type_index(result->get_file(), type_sort, part_offset);
  }  /* if */
  return result_idx;
}  /* an_ifc_output_state::alloc_type */


template<typename an_ifc_Node_type>
an_ifc_name_index an_ifc_output_state::alloc_name(an_ifc_Node_type *result)
/*
Allocate a name node in its corresponding output partition.  Set *result to the
allocated node.  Return the node's name index.
*/
{
  an_ifc_partition_kind
                part_kind = get_ifc_partition_kind<an_ifc_Node_type>();
  size_t        part_offset = this->alloc_node(result);
  an_ifc_name_sort
                name_sort = to_name_sort(part_kind);

  return an_ifc_name_index(result->get_file(), name_sort, part_offset);
}  /* an_ifc_output_state::alloc_name */


template<typename an_ifc_Node_type>
an_ifc_edg_complex_token_index an_ifc_output_state::alloc_complex_token(
                                                      an_ifc_Node_type *result)
/*
Allocate a complex token node in its corresponding output partition.  Set
*result to the allocated node.  Return the node's complex token index.
*/
{
  an_ifc_partition_kind
                part_kind = get_ifc_partition_kind<an_ifc_Node_type>();
  size_t        part_offset = this->alloc_node(result);
  an_ifc_edg_complex_token_sort
                token_sort = to_edg_complex_token_sort(part_kind);

  return an_ifc_edg_complex_token_index(result->get_file(), token_sort,
                                        part_offset);
}  /* an_ifc_output_state::alloc_complex_token */


template<typename an_ifc_Node_type>
void an_ifc_output_state::fetch_node(an_ifc_Node_type *result,
                                     size_t           index)
/*
Fetch a node from its corresponding output partition at the given index.  Set
*result to the fetched node.
*/
{
  an_ifc_partition_kind
                part_kind = get_ifc_partition_kind<an_ifc_Node_type>();
  an_ifc_output_partition
                *output_part = this->get_partition(part_kind);
  a_byte        **start;
  size_t        byte_offset;

  output_part->fetch_element(&start, &byte_offset, index);

  an_ifc_Node_type constructed_value(this->output_file, start, byte_offset);
  *result = constructed_value;
}  /* an_ifc_output_state::fetch_node */


template<typename an_ifc_Node_type>
void an_ifc_output_state::fetch_decl(an_ifc_Node_type  *result,
                                     an_ifc_decl_index index)
/*
Fetch a node from its corresponding output partition at the given index.  Set
*result to the fetched node.
*/
{
  check_assertion(to_partition_kind(index.sort) ==
                  get_ifc_partition_kind<an_ifc_Node_type>());
  this->fetch_node(result, index.value);
}  /* an_ifc_output_state::fetch_decl */



void an_ifc_output_state::sort_traits()
/*
Sort any trait partitions so that the reader can perform binary search.

Note that no Byte_buffer_entity objects representing a trait (with non-local
storage owned by this output state) should exist when this function is called.
Should such any object exist, use of any said object after this call may result
in undefined behavior.
*/
{
  this->sort_trait_partition<an_ifc_edg_trait_function_definition>();
}  /* an_ifc_output_state::sort_traits */


/*
This structure encapsulates various pieces of meta information that are
required for the IFC file format for an individual partition.
*/
struct an_ifc_output_partition_metadata {
  an_ifc_partition_kind
                kind;   /* The partition kind of this partition (this can be
                           used to retrieve the output partition from the array
                           of partitions -- i.e.,
                           an_ifc_output_state::partitions). */
  size_t        relative_offset;
                        /* The byte offset from the start of partition writing.
                           This is different from the byte offset from the
                           start of the file as it does not factor in the size
                           of preceding non-partition data (e.g., the table of
                           contents). */
};  /* an_ifc_output_partition_metadata */

}  /* namespace */
namespace detail {

template<>
struct Is_trivially_copyable_edg_impl<an_ifc_output_partition_metadata> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_copyable_edg_impl */

template<>
struct Is_trivially_destructible_edg_impl<an_ifc_output_partition_metadata> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_destructible_edg_impl */

}  /* namespace detail */

NORETURN static void ifc_write_error()
/*
Issue a catastrophic diagnostic that the IFC file could not be created and
terminate the compilation.  This routine does not return.
*/
{
  error_position = null_source_position;
  file_write_error(ec_ifc, errno);
}  /* ifc_write_error */

namespace {

/*
This structure encapsulates various pieces of meta information that are
required for the IFC file format.
*/
struct an_ifc_output_metadata {
  Dyn_array<an_ifc_output_partition_metadata, General_allocator>
                used_partitions = {};
                        /* The metadata for the partitions that will be written
                           to the IFC file. */
  size_t        num_partition_bytes = 0;
                        /* The total number of bytes used for the partitions
                           that will be written to the IFC file. */
  size_t        num_string_table_bytes = 0;
                        /* The number of bytes used for the string table that
                           will be written to the IFC file. */
};  /* an_ifc_output_metadata */

/*
This structure encapsulates the write operations into a particular IFC file
along with the hashing of the contents as they're written out.
*/
struct an_ifc_output_stream {
  an_ifc_output_stream(an_ifc_module_file *output_file_val)
    : output_file(output_file_val)
    {}
  an_ifc_module_file *get_file() const
    { return this->output_file; }
  a_module_file_kind get_module_kind() const
    { return this->output_file->module_kind; }
  void write_bytes_at(size_t       file_pos,
                      a_byte const *bytes,
                      size_t       num_bytes);
  void write_bytes(a_byte const *bytes,
                   size_t       num_bytes,
                   a_boolean    add_to_checksum = TRUE);
  a_sha256_digest finish_checksum();
private:
  an_ifc_module_file
                *output_file;
                        /* The IFC module file being written to. */
  a_sha256_hash hash_state = {};
                        /* The hash state of all bytes written that should
                           be considered part of the checksum. */
#if CHECKING
  a_boolean     checksum_computed = FALSE;
                        /* TRUE if the checksum has been computed via a call to
                           an_ifc_output_stream::finish_checksum. */
#endif /* CHECKING */
};  /* an_ifc_output_stream */


void an_ifc_output_stream::write_bytes_at(size_t       file_pos,
                                          a_byte const *bytes,
                                          size_t       num_bytes)
/*
Write the given bytes (counted by num_bytes) at the given file position.  The
file position must already exist and the file must already have at least
num_bytes bytes following the specified file position.
*/
{
  long orig_file_pos = ftell(this->output_file->f_module);

  if (orig_file_pos < 0) {
    ifc_write_error();
  }  /* if */
  /* If this assertion fails, this write operation would increase the size of
     the file.  write_bytes should be used for append operations. */
  check_assertion(file_pos + num_bytes < size_t_arg(orig_file_pos));
  { /* Move the cursor to the desired write position. */
    int seek_result = fseek(this->output_file->f_module,
                            (long)file_pos, SEEK_SET);

    if (seek_result != 0) {
      ifc_write_error();
    }  /* if */
  }
  this->write_bytes(bytes, num_bytes, /*add_to_checksum=*/FALSE);
  { /* Restore the cursor to its original position. */
    int seek_result = fseek(this->output_file->f_module, orig_file_pos,
                            SEEK_SET);
    if (seek_result != 0) {
      ifc_write_error();
    }  /* if */
  }
}  /* an_ifc_output_stream::write_bytes_at */


void an_ifc_output_stream::write_bytes(a_byte const *bytes,
                                       size_t       num_bytes,
                     /* Defaulted: */  a_boolean    add_to_checksum)
/*
Append the given bytes (counted by num_bytes) to the underlying file.  If
add_to_checksum is TRUE, the bytes will be factored into the SHA-2 256 checksum
associated with the file.
*/
{
  /* The write must be either performed without modifying the checksum
     or the checksum must not yet have been calculated. */
  check_assertion(!add_to_checksum || !this->checksum_computed);
  size_t num_written = fwrite(bytes, num_bytes, /*num_to_write=*/1,
                              this->output_file->f_module);

  if (add_to_checksum) {
    this->hash_state.update(bytes, num_bytes);
  }  /* if */
  if (num_written != 1) {
    ifc_write_error();
  }  /* if */
}  /* an_ifc_output_stream::write_bytes */


a_sha256_digest an_ifc_output_stream::finish_checksum()
/*
Compute and return the checksum of all bytes written to the file that
should be considered part of the checksum.
*/
{
#if CHECKING
  /* The output stream can only be "finished" once. */
  check_assertion(!this->checksum_computed);
  this->checksum_computed = TRUE;
#endif /* CHECKING */
  return this->hash_state.compute_digest();
}  /* an_ifc_output_stream::finish_checksum */

}  /* namespace */

static void write_ifc_magic_bytes(an_ifc_output_stream *output_stream)
/*
Write the series of magic bytes identifying the IFC file.
*/
{
  /* If this assertion fails the front end is presumably trying to write a
     Microsoft compatible IFC file and that's not currently possible. */
  check_assertion(output_stream->get_module_kind() == mfk_edg_ifc);
  /* FIXME: If the file header ever changes size, we'll need to change this. */
  output_stream->write_bytes(edg_ifc_magic_numbers,
                             sizeof(edg_ifc_magic_numbers),
                             /*add_to_checksum=*/FALSE);
}  /* write_ifc_magic_bytes */


static an_ifc_architecture_sort get_target_ifc_architecture()
/*
Return the IFC architecture sort value corresponding to the current target.
*/
{
  an_ifc_architecture_sort result;

  if (target_is_x86_based()) {
    if (target_is_64_bits()) {
      result = ifc_as_x64;
    } else {
      result = ifc_as_x86;
    }  /* if */
  } else if (target_is_arm_based()) {
    if (target_is_x86_compatible()) {
      if (target_is_64_bits()) {
        result = ifc_as_hybrid_x86_arm64;
      } else {
        ifc_write_catastrophe();
      }  /* if */
    } else if (target_is_64_bits()) {
      result = ifc_as_arm64;
    } else {
      result = ifc_as_arm32;
    }  /* if */
  } else {
    ifc_write_catastrophe();
  }  /* if */
  return result;
}  /* get_target_ifc_architecture */


static size_t get_partitions_start(an_ifc_module_file           *file,
                                   const an_ifc_output_metadata &metadata)
/*
Return the byte offset for the start of the partitions.
*/
{
  size_t result = 0;

  /* Add the bytes for the magic numbers. */
  result += sizeof(edg_ifc_magic_numbers);
  /* Add the bytes for the file header. */
  result += get_ifc_buffer_size<an_ifc_file_header_storage>(file);
  return result;
}  /* get_partitions_start */


static size_t get_string_table_start(an_ifc_module_file           *file,
                                     const an_ifc_output_metadata &metadata)
/*
Return the byte offset for the start of the string table given the associated
module file and output metadata.
*/
{
  size_t result = 0;

  /* Add the bytes for the start of the partitions. */
  result += get_partitions_start(file, metadata);
  /* Add the bytes for the partitions themselves. */
  result += metadata.num_partition_bytes;
  return result;
}  /* get_string_table_start */


static size_t get_toc_start(an_ifc_module_file           *file,
                            const an_ifc_output_metadata &metadata)
/*
Return the byte offset for the start of the table of contents given the
associated module file and output metadata.
*/
{
  size_t result = 0;

  /* Add the bytes for the start of the string table. */
  result += get_string_table_start(file, metadata);
  /* Add the bytes for the string table. */
  result += metadata.num_string_table_bytes;
  return result;
}  /* get_toc_start */


template<typename an_ifc_Node_type>
static a_byte const *node_as_bytes(const an_ifc_Node_type &node)
/*
Return the given node's storage as a const byte pointer.
*/
{
  return (a_byte const *)*(node.get_storage());
}  /* node_as_bytes */


static void write_ifc_header(an_ifc_output_stream         *output_stream,
                             an_ifc_unit_sort             unit_sort,
                             a_boolean                    is_internal,
                             size_t                       module_name,
                             size_t                       source_file_name,
                             an_ifc_scope_offset          global_scope,
                             const an_ifc_output_metadata &metadata)
/*
Use the associated offset of the source file name into the string table, global
scope offset, and metadata to form the IFC file header.  Write the created file
header into the given output stream.
*/
{
  /* Create an instance of an_ifc_file_header_storage on the stack and
     use it to set up, then write, the file header. */
  an_ifc_module_file *file = output_stream->get_file();
  an_ifc_file_header file_header(file);
  an_ifc_version     ifc_major_version(file, file->version_major);
  an_ifc_version     ifc_minor_version(file, file->version_minor);

  /* Set the IFC version information. */
  set_ifc_major_version(&file_header, ifc_major_version);
  set_ifc_minor_version(&file_header, ifc_minor_version);
  /* Set the ABI version. */
  set_ifc_arch(&file_header, get_target_ifc_architecture());

  /* Set the string table information. */
  an_ifc_byte_offset ifc_string_table_start(file,
                                            get_string_table_start(file,
                                                                   metadata));
  an_ifc_cardinality ifc_string_table_length(file,
                                             metadata.num_string_table_bytes);
  set_ifc_string_table_bytes(&file_header, ifc_string_table_start);
  set_ifc_string_table_size(&file_header, ifc_string_table_length);

  /* Set the unit information. */
  an_ifc_unit_index ifc_unit_idx(file, unit_sort, module_name);
  set_ifc_unit(&file_header, ifc_unit_idx);

  /* Set the source path information. */
  an_ifc_text_offset ifc_source_path(file, source_file_name);
  set_ifc_src_path(&file_header, ifc_source_path);

  /* Set the primary scope information. */
  set_ifc_global_scope(&file_header, global_scope);

  /* Set the table of contents offset. */
  an_ifc_byte_offset ifc_toc_start(file, get_toc_start(file, metadata));
  set_ifc_toc(&file_header, ifc_toc_start);

  /* Set the number of partitions. */
  an_ifc_cardinality ifc_partition_count(
               file,
               (an_ifc_cardinality_storage)metadata.used_partitions.length());
  set_ifc_partition_count(&file_header, ifc_partition_count);

  /* Set whether or not this is an internal partition. */
  an_ifc_bool ifc_is_internal(file, is_internal);
  set_ifc_internal(&file_header, ifc_is_internal);
  /* Do the append. */
  using a_sha256_storage_type = typename an_ifc_sha256::storage_type;
  using a_header_storage_type = typename an_ifc_file_header::storage_type;

  /* Write the file header's SHA-2 256 bit checksum as a placeholder. */
  size_t sha256_len = get_ifc_buffer_size<a_sha256_storage_type>(file);
  output_stream->write_bytes(node_as_bytes(file_header), sha256_len,
                             /*add_to_checksum=*/FALSE);

  /* Write the remaining file header contents (included in the checksum). */
  size_t header_len = get_ifc_buffer_size<a_header_storage_type>(file);
  output_stream->write_bytes(node_as_bytes(file_header) + sha256_len,
                             header_len - sha256_len);
}  /* write_ifc_header */


static void write_ifc_partition(an_ifc_output_stream          *output_stream,
                                const an_ifc_output_partition &partition)
/*
Append the contents of the given partition to the given module output stream.
*/
{
  output_stream->write_bytes(partition.get_bytes(), partition.get_num_bytes());
}  /* write_ifc_partition */


static void write_string_table(an_ifc_output_stream       *output_stream,
                               const an_ifc_output_buffer &string_table_bytes)
/*
Append the contents of the given string table to the given module output
stream.
*/
{
  output_stream->write_bytes(string_table_bytes.begin(),
                             string_table_bytes.length());
}  /* write_string_table */


static void write_table_of_contents_entry(
                   an_ifc_output_stream                   *output_stream,
                   size_t                                 start_of_partitions,
                   const an_ifc_output_partition          &partition,
                   const an_ifc_output_partition_metadata &partition_metadata)
/*
Write the table of contents entry for the given partition into the given module
output stream.

The start of partitions value should be the byte offset from the start of the
file to where the partition bytes have been written.  Partition metadata is the
associated partition metadata for the partition.
*/
{
  an_ifc_module_file *file = output_stream->get_file();
  an_ifc_partition   toc_entry(file);
  an_ifc_text_offset ifc_name_offset(file, partition.part_name_offset);

  /* Set the name of the partition. */
  set_ifc_name(&toc_entry, ifc_name_offset);

  /* Set the offset where this partition starts in the file. */
  an_ifc_byte_offset ifc_part_offset(file,
                                     (start_of_partitions +
                                      partition_metadata.relative_offset));
  set_ifc_offset(&toc_entry, ifc_part_offset);

  /* Set the number of elements in this partition. */
  size_t             num_elements = (partition.get_num_bytes() /
                                     partition.element_size);
  an_ifc_cardinality ifc_num_elements(file, num_elements);
  set_ifc_cardinality(&toc_entry, ifc_num_elements);

  /* Set the entry size. */
  size_t             entity_size = partition.element_size;
  an_ifc_entity_size ifc_entity_size(file, entity_size);
  set_ifc_entry_size(&toc_entry, ifc_entity_size);

  using a_storage_type = typename an_ifc_partition::storage_type;
  size_t storage_len = get_ifc_buffer_size<a_storage_type>(file);
  output_stream->write_bytes(node_as_bytes(toc_entry), storage_len);
}  /* write_table_of_contents_entry */


static void write_ifc_checksum(an_ifc_output_stream *output_stream)
/*
Create and write out the IFC file header for the given IFC module file using
the associated offset of the source file name into the string table, global
scope offset, and metadata.
*/
{
  an_ifc_module_file *file = output_stream->get_file();
  a_sha256_digest    digest = output_stream->finish_checksum();

  using a_sha256_type = typename an_ifc_sha256::storage_type;
  size_t sha256_size = get_ifc_buffer_size<a_sha256_type>(file);
  check_assertion(sha256_size == 32);
  output_stream->write_bytes_at(/*file_pos=*/4, digest, sha256_size);
}  /* write_ifc_checksum */


static an_ifc_output_metadata make_output_metadata(
                                     an_ifc_output_partition    **partitions,
                                     const an_ifc_output_buffer &string_table)
/*
Given the array of partitions and string table for an IFC output state,
return the computed IFC output metadata.
*/
{
  an_ifc_output_metadata result;
  size_t                 relative_start = 0;

  for (size_t i = 0; i < IFC_PARTITION_COUNT; ++i) {
    const an_ifc_output_partition *partition = partitions[i];

    if (partition == NULL) {
      continue;
    }  /* if */

    an_ifc_output_partition_metadata part_metadata;
    /* Add the partition metadata for this partition. */
    part_metadata.kind = (an_ifc_partition_kind)(i + 1);
    part_metadata.relative_offset = relative_start;
    result.used_partitions.push_back(part_metadata);

    /* Update the relative_start for the next partition. */
    size_t bytes_in_partition = partition->get_num_bytes();
    relative_start += bytes_in_partition;
    /* Update the total number of bytes used by all partitions. */
    result.num_partition_bytes += bytes_in_partition;
  }  /* for */
  result.num_string_table_bytes =
                            (an_ifc_cardinality_storage)string_table.length();
  return result;
}  /* make_output_metadata */

namespace {

void an_ifc_output_state::write()
/*
Write the IFC output state to the file associated with the output state's
output file (i.e., an_ifc_output_state::output_file).
*/
{
  an_ifc_output_stream
                output_stream(this->output_file);
  an_ifc_output_metadata
                metadata = make_output_metadata(this->partitions,
                                                this->string_table);

  write_ifc_magic_bytes(&output_stream);
  write_ifc_header(&output_stream, this->unit_sort, this->internal_partition,
                   this->module_name_offset,
                   this->source_file_name_offset,
                   this->global_scope,
                   metadata);
  for (const an_ifc_output_partition_metadata &part_metadata :
                                                   metadata.used_partitions) {
    an_ifc_output_partition
                *partition = this->get_partition(part_metadata.kind);

    write_ifc_partition(&output_stream, *partition);
  }  /* for */
  write_string_table(&output_stream, this->string_table);

  size_t start_of_partitions = get_partitions_start(this->output_file,
                                                    metadata);
  for (const an_ifc_output_partition_metadata &part_metadata :
                                                   metadata.used_partitions) {
    an_ifc_output_partition
                *partition = this->get_partition(part_metadata.kind);

    write_table_of_contents_entry(&output_stream, start_of_partitions,
                                  *partition, part_metadata);
  }  /* for */
  write_ifc_checksum(&output_stream);
}  /* an_ifc_output_state::write */


template<typename an_ifc_Node_type>
an_ifc_output_partition *an_ifc_output_state::get_or_init_partition()
/*
Return the corresponding output partition for the given node type.  If the
output partition is not already initialized, initialize it now.
*/
{
  an_ifc_partition_kind
                part_kind = get_ifc_partition_kind<an_ifc_Node_type>();
  an_ifc_output_partition
                *output_part = this->get_partition(part_kind);

  if (output_part == NULL) {
    /* The output partition has not yet been created; create it now. */
    size_t       part_size = get_ifc_partition_element_size(this->output_file,
                                                            part_kind);
    /* The partition will need a name for the table of contents write out.
       To simplify things later, create the name now. */
    a_string_view part_name = get_partition_name_from_kind(part_kind);
    size_t        part_name_offset = this->add_to_string_table(part_name);

    output_part = new_general<an_ifc_output_partition>(part_name_offset,
                                                       part_size);
    this->set_partition(part_kind, output_part);
  }  /* if */
  return output_part;
}  /* an_ifc_output_state::get_or_init_partition */


template<typename an_ifc_Node_type>
void an_ifc_output_state::sort_trait_partition()
/*
The IFC requires that traits be ordered so that binary search can be performed.
Perform a sort on the trait contents now to correctly arrange the contents.
*/
{
  an_ifc_partition_kind
                part_kind = get_ifc_partition_kind<an_ifc_Node_type>();
  an_ifc_output_partition
                *output_partition = this->get_partition(part_kind);

  if (output_partition != NULL) {
    /* Create an array representing the element positions.  Then sort the array
       of element positions without actually moving any partition elements in
       the byte buffer.  Finally, create a new output partition and copy the
       partition bytes from the original output partition into the new
       output partition at the correct position. */
    size_t      num_traits = output_partition->get_num_elements();
    Dyn_array<size_t, General_allocator>
                idx_array(/*capacity=*/num_traits);

    for (size_t i = 0; i < num_traits; ++i) {
      idx_array.push_back(i);
    }  /* for */

    auto        compare_function = [this](size_t idx_a, size_t idx_b) {
      an_ifc_Node_type node_a;
      an_ifc_Node_type node_b;

      this->fetch_node(&node_a, idx_a);
      this->fetch_node(&node_b, idx_b);
      return get_ifc_encoded_decl(node_a) < get_ifc_encoded_decl(node_b);
    };
    sort(&idx_array, compare_function);

    /* Create a replacement partition. */
    an_ifc_output_partition
                *new_partition = new_general<an_ifc_output_partition>(
                                            output_partition->part_name_offset,
                                            output_partition->element_size);
    this->set_partition(part_kind, new_partition);
    /* Copy the elements from the original partition into the new partition
       in the correct position. */
    new_partition->new_elements(num_traits);
    for (size_t i = 0; i < num_traits; ++i) {
      size_t input_idx = idx_array[i];
      size_t dst_offset = (i * output_partition->element_size);
      size_t src_offset = (input_idx * output_partition->element_size);

      memcpy(new_partition->get_bytes() + dst_offset,
             output_partition->get_bytes() + src_offset,
             output_partition->element_size);
    }  /* for */
    /* Free the replaced partition. */
    delete_general(&output_partition);
  }  /* if */
}  /* an_ifc_output_state::sort_trait_partition */


using a_seq_number_key = uint32_t;
                        /* This type is used to key entries of a_seq_number in
                           the an_ifc_il_map::seq_number_map. */

using a_scope_member_array = Dyn_array<an_ifc_decl_index, General_allocator>;
                        /* This type is used to represent an array of scope
                           members for an_ifc_il_map::scope_members. */

/*
A structure used to represent a token cache to be added to the IFC.  This
structure does not itself create any IFC nodes; instead it represents the token
cache as it is constructed.  To form the token cache represented by its current
state it should be passed to an_ifc_output_state::alloc_token_cache.
*/
struct an_ifc_output_token_cache {
  void add_basic(an_ifc_edg_basic_token_sort basic_token);
  void add_complex(an_ifc_edg_complex_token_index complex_token);

  size_t get_num_basic_tokens() const
    { return this->basic_tokens.length(); }
  size_t get_num_complex_tokens() const
    { return this->complex_tokens.length(); }

  an_ifc_edg_basic_token_sort get_basic_token(size_t i) const
    { return this->basic_tokens[i]; }
  an_ifc_edg_complex_token_index get_complex_token(size_t i) const
    { return this->complex_tokens[i]; }
private:
  Dyn_array<an_ifc_edg_basic_token_sort, General_allocator>
                basic_tokens = {};
                        /* This is the array of EDG IFC basic tokens that will
                           be converted to a block of EDG IFC TokenBasic
                           nodes. */
  Dyn_array<an_ifc_edg_complex_token_index, General_allocator>
                complex_tokens = {};
                        /* This is the array of EDG IFC complex token indexes
                           that will be converted to a block of EDG IFC
                           TokenComplex nodes. */
};  /* an_ifc_output_token_cache */


an_ifc_edg_token_cache_offset an_ifc_output_state::alloc_token_cache(
                                 const an_ifc_output_token_cache &output_cache)
/*
Allocate and populate the token cache with the information tracked by the given
IFC output token cache.  Return the IFC token cache offset of the created token
cache.
*/
{
  /* Allocate the token cache. */
  an_ifc_edg_token_cache
                token_cache;
  size_t        token_cache_offset = this->alloc_node(&token_cache);
  an_ifc_edg_token_cache_offset
                result(token_cache.get_file(), token_cache_offset + 1);
  /* Allocate the basic and complex token blocks. */
  size_t        num_basic_tokens = output_cache.get_num_basic_tokens();
  size_t        basic_tokens_start = this->
                                      alloc_node_block<an_ifc_edg_token_basic>(
                                                             num_basic_tokens);
  size_t        num_complex_tokens = output_cache.get_num_complex_tokens();
  size_t        complex_tokens_start = this->
                               alloc_node_block<an_ifc_edg_heap_complex_token>(
                                                           num_complex_tokens);
  /* Associate the basic tokens with the token cache. */
  an_ifc_edg_token_basic_offset
                ifc_basic_tokens_start(token_cache.get_file(),
                                       basic_tokens_start);
  an_ifc_cardinality
                ifc_num_basic_tokens(token_cache.get_file(), num_basic_tokens);
  set_ifc_tokens(&token_cache, ifc_basic_tokens_start);
  set_ifc_num_tokens(&token_cache, ifc_num_basic_tokens);

  /* Associate the complex tokens with the token cache. */
  an_ifc_edg_heap_complex_token_offset
                ifc_complex_tokens_start(token_cache.get_file(),
                                         complex_tokens_start);
  an_ifc_cardinality
                ifc_num_complex_tokens(token_cache.get_file(),
                                       num_complex_tokens);
  set_ifc_complex_tokens(&token_cache, ifc_complex_tokens_start);
  set_ifc_num_complex_tokens(&token_cache, ifc_num_complex_tokens);

  /* Complete the basic tokens. */
  for (size_t i = 0; i < num_basic_tokens; ++i) {
    an_ifc_edg_token_basic basic_token;

    this->fetch_node(&basic_token, basic_tokens_start + i);
    set_ifc_kind(&basic_token, output_cache.get_basic_token(i));
  }  /* for */
  /* Complete the complex tokens. */
  for (size_t i = 0; i < num_complex_tokens; ++i) {
    an_ifc_edg_heap_complex_token complex_token;

    this->fetch_node(&complex_token, complex_tokens_start + i);
    set_ifc_index(&complex_token, output_cache.get_complex_token(i));
  }  /* for */
  return result;
}  /* an_ifc_output_state::alloc_token_cache */


an_ifc_expr_index an_ifc_output_state::alloc_token_cache_expr(
                                 const an_ifc_output_token_cache &output_cache)
/*
Allocate and populate the token cache with the information tracked by the given
IFC output token cache.  Return the IFC expression index of the created token
cache.
*/
{
  an_ifc_edg_token_cache_offset
                token_cache_offset = this->alloc_token_cache(output_cache);

  return an_ifc_expr_index(token_cache_offset.get_file(),
                           ifc_es_expr_vendor_extension,
                           token_cache_offset.value);
}  /* an_ifc_output_state::alloc_token_cache_expr */


void an_ifc_output_token_cache::add_basic(
                                       an_ifc_edg_basic_token_sort basic_token)
/*
Add the given basic token to the end of the token cache.
*/
{
  /* Complex tokens should be added via add_complex not add_basic to ensure
     the complex token and basic token arrays are properly managed. */
  check_assertion(basic_token != ifc_ebts_complex);
  this->basic_tokens.push_back(basic_token);
}  /* an_ifc_output_token_cache::add_basic */


void an_ifc_output_token_cache::add_complex(
                                      an_ifc_edg_complex_token_index token_idx)
/*
Add the given complex token to the end of the token cache.
*/
{
  this->basic_tokens.push_back(ifc_ebts_complex);
  this->complex_tokens.push_back(token_idx);
}  /* an_ifc_output_token_cache::add_complex */


/*
This structure maps IL entries to their IFC entries.
*/
struct an_ifc_il_map {
  an_ifc_il_map(an_ifc_output_state *output_state_val)
    : output_state(output_state_val)
    {}

  /* General functions for entering entities into the IFC. */
  an_ifc_type_index find_or_enter_type(a_type_ptr type);
  an_ifc_type_index enter_type(a_type_ptr type);
  an_ifc_scope_offset find_or_enter_scope(a_scope_ptr scope);
  an_ifc_scope_offset enter_scope(a_scope_ptr scope);
  an_ifc_decl_index find_or_enter_class_struct_union(a_type_ptr type);
  an_ifc_decl_index enter_class_struct_union(a_type_ptr type);
  an_ifc_decl_index find_or_enter_enum(a_type_ptr type);
  an_ifc_decl_index enter_enum(a_type_ptr type);
  an_ifc_decl_index find_or_enter_namespace(a_namespace_ptr nsp);
  an_ifc_decl_index enter_namespace(a_namespace_ptr nsp);
  an_ifc_decl_index find_or_enter_routine(a_routine_ptr rp);
  an_ifc_decl_index enter_routine(a_routine_ptr rp);
  an_ifc_decl_index find_or_enter_template(a_template_ptr templ);
  an_ifc_decl_index enter_template(a_template_ptr templ);
  an_ifc_decl_index find_or_enter_using_directive(a_using_decl_ptr udp,
                                                  a_scope_ptr      scope);
  an_ifc_decl_index enter_using_directive(a_using_decl_ptr udp,
                                          a_scope_ptr      scope);

  /* Functions for retrieving scope member state. */
  size_t get_number_of_scopes() const
    { return this->scope_members.length(); }
  const a_scope_member_array& get_scope_members(size_t scope_idx) const
    { return this->scope_members[scope_idx]; }
private:
  an_ifc_module_file* get_default_file() const
    { return this->output_state->get_file(); }

  /* Functions for adding/referencing names and text. */
  an_ifc_text_offset string_as_text_offset(a_const_char *str,
                                           size_t       str_len);
  an_ifc_text_offset string_as_text_offset(a_const_char *str);
  an_ifc_name_index string_as_name_index(a_const_char *str);
  template<typename a_Type>
  an_ifc_text_offset entity_name_as_text_offset(a_Type *il_entity);
  template<typename a_Type>
  an_ifc_name_index entity_name_as_name_index(a_Type *il_entity);

  /* Functions for entering source position information. */
  an_ifc_name_index find_or_enter_src_file(a_source_file_ptr file);
  an_ifc_name_index enter_src_file(a_source_file_ptr file);
  an_ifc_source_location find_or_enter_null_pos();
  an_ifc_source_location find_or_enter_pos(const a_source_position &pos);
  an_ifc_source_location enter_pos(const a_source_position &pos);
  template<typename a_Type>
  an_ifc_source_location find_or_enter_entity_pos(a_Type *il_entity);

  /* Functions for entering new nodes in the output state with a memoized
     association and an expected returned index type. */
  template<typename an_ifc_Node_type>
  inline an_ifc_type_index map_new_type(a_type_ptr       type,
                                        an_ifc_Node_type *node);
  template<typename a_Type, typename an_ifc_Node_type>
  inline an_ifc_decl_index map_new_decl(a_Type           *il_entity,
                                        an_ifc_Node_type *node);

  /* Functions for entering specific type kinds. */
  an_ifc_type_index enter_class_struct_union_type(a_type_ptr type);
  an_ifc_type_index enter_constructor_type(a_type_ptr type);
  an_ifc_type_index enter_enum_type(a_type_ptr type);
  an_ifc_type_index enter_float_type(a_type_ptr type);
  an_ifc_type_index enter_free_function_type(a_type_ptr type);
  an_ifc_type_index enter_integer_type(a_type_ptr type);
  an_ifc_type_index enter_member_function_type(a_type_ptr type);
  an_ifc_type_index enter_nullptr_type(a_type_ptr type);
  an_ifc_type_index enter_pointer_type(a_type_ptr type);
  an_ifc_type_index enter_routine_params_type(a_type_ptr type);
  an_ifc_type_index enter_routine_type(a_type_ptr type);
  an_ifc_type_index enter_template_type_param_type(
                                             a_type_ptr        type,
                                             an_ifc_decl_index param_decl_idx);
  an_ifc_type_index enter_typedef_type(a_type_ptr type);
  an_ifc_type_index enter_void_type(a_type_ptr type);

  /* Functions for entering portions of other larger entities. */
  an_ifc_sequence enter_enumerators(a_type_ptr type);
  an_ifc_chart_index enter_routine_params(a_routine_ptr rp);
  an_ifc_edg_template_argument_index enter_template_argument(
                                                 a_template_arg_ptr templ_arg);
  an_ifc_chart_index enter_template_params(a_template_ptr templ);
  an_ifc_type_index enter_template_template_param_type(a_template_ptr templ);
  void set_template_param_coordinates(
                               an_ifc_decl_parameter             *param_decl,
                               const a_template_param_coordinate &coordinates);
  void set_up_template_non_type_param(an_ifc_decl_parameter    *param_decl,
                                      a_template_parameter_ptr templ_param);
  void set_up_template_template_param(an_ifc_decl_parameter    *param_decl,
                                      an_ifc_decl_index        curr_param_idx,
                                      a_template_parameter_ptr templ_param);
  void set_up_template_type_param(an_ifc_decl_parameter    *param_decl,
                                  an_ifc_decl_index        curr_param_idx,
                                  a_template_parameter_ptr templ_param);

  /* Functions for entering constants. */
  an_ifc_edg_constant_index enter_constant(const a_constant *cp);

  /* Functions for adding tokens and manipulating token caches. */
  an_ifc_output_token_cache copy_template_body_to_cache(a_template_ptr templ);
  an_ifc_edg_complex_token_index find_or_enter_textual_token(
                                                    const a_shared_token &tok);
  void enter_basic_token_to_cache(an_ifc_output_token_cache *ifc_cache,
                                  const a_shared_token      &tok);
  an_ifc_edg_complex_token_index enter_textual_token(
                                                    const a_shared_token &tok);
  void enter_textual_token_to_cache(an_ifc_output_token_cache *ifc_cache,
                                    const a_shared_token      &tok);
  an_ifc_edg_complex_token_index enter_constant_token(
                                  an_ifc_edg_constant_token_sort token_kind,
                                  an_ifc_edg_constant_index      constant_idx);
  void enter_constant_token_to_cache(an_ifc_output_token_cache *ifc_cache,
                                     const a_shared_token      &tok);
  void enter_extracted_body_to_cache(an_ifc_output_token_cache *ifc_cache,
                                     const a_shared_token      &tok);
  void enter_identifier_token_to_cache(an_ifc_output_token_cache *ifc_cache,
                                       const a_shared_token      &tok);
  void enter_token_cache(an_ifc_output_token_cache *ifc_cache,
                         a_token_cache             *fe_cache);

  /* Functions for entering special IFC fundamental types. */
  an_ifc_type_index find_or_enter_class_scope_type();
  an_ifc_type_index find_or_enter_namespace_scope_type();
  an_ifc_type_index find_or_enter_alias_typedef_type();
  an_ifc_type_index find_or_enter_scoped_enum_type();
  an_ifc_type_index find_or_enter_struct_scope_type();
  an_ifc_type_index find_or_enter_union_scope_type();
  an_ifc_type_index find_or_enter_unscoped_enum_type();

  /* Functions for entering declarations that are invoked by proxy (e.g.,
     enter_typedef is called when needed by enter_type). */
  an_ifc_decl_index enter_alias_template(a_template_ptr templ);
  an_ifc_decl_index enter_class_template(a_template_ptr templ);
  an_ifc_decl_index enter_constructor(a_routine_ptr rp);
  an_ifc_decl_index enter_destructor(a_routine_ptr rp);
  an_ifc_decl_index enter_field(a_field_ptr field);
  an_ifc_decl_index enter_free_function(a_routine_ptr rp);
  an_ifc_decl_index enter_function_template(a_template_ptr templ);
  an_ifc_decl_index enter_member_function(a_routine_ptr rp);
  an_ifc_decl_index enter_namespace(a_scope_ptr scope);
  an_ifc_decl_index enter_typedef(a_type_ptr type);

  /* Functions associating entities with their corresponding scopes. */
  an_ifc_decl_index find_or_enter_home_scope(a_scope_ptr scope);
  an_ifc_decl_index enter_home_scope(a_scope_ptr scope);
  void map_scope_member(a_scope_ptr scope, an_ifc_decl_index decl);
  template<typename a_Type>
  an_ifc_decl_index associate_entity_home_scope(a_Type *il_entity);
  template<typename a_Type>
  an_ifc_decl_index associate_entity_home_scope(a_Type      *il_entity,
                                                a_scope_ptr scope);

  an_ifc_output_state
                *output_state;
                        /* The associated IFC output state. */
  Ptr_map<a_string_view, size_t, General_allocator>
                string_table_map = {/*mask_width=*/10};
                        /* A map of string values to their given offsets
                           in the string table.  This is used to deduplicate
                           strings as they're added to the IFC reducing the
                           overall file size. */
  Ptr_map<a_token_kind, size_t, General_allocator>
                textual_token_map = {/*mask_width=*/10};
                        /* A map of token kinds to their offset in the textual
                           token partition (plus one to differentiate from the
                           null case). */
  Ptr_map<a_source_file_ptr, an_ifc_name_index, General_allocator>
                src_file_map = {/*mask_width=*/10};
                        /* A map of IL source files to IFC source file
                           names. */
  Ptr_map<a_seq_number_key, an_ifc_line_offset, General_allocator>
                seq_number_map = {/*mask_width=*/10};
                        /* A map of IL sequence numbers to IFC source line
                           offsets. */
  Ptr_map<a_type_ptr, an_ifc_type_index, General_allocator>
                type_map = {/*mask_width=*/10};
                        /* A map of IL types to IFC type indexes. */
  Ptr_map<a_scope_ptr, an_ifc_scope_offset, General_allocator>
                scope_map = {/*mask_width=*/10};
                        /* A map of IL scopes to their corresponding IFC scope
                           offsets. */
  an_ifc_type_index
                fund_class_type;
                        /* The fundamental type used to represent a class
                           DeclSort::Scope. */
  an_ifc_type_index
                fund_namespace_type;
                        /* The fundamental type used to represent a namespace
                           DeclSort::Scope. */
  an_ifc_type_index
                fund_alias_typedef_type;
                        /* The fundamental type used to represent a typedef
                           DeclSort::Alias. */
  an_ifc_type_index
                fund_scoped_enum_type;
                        /* The fundamental type used to represent a scoped
                           DeclSort::Enumeration. */
  an_ifc_type_index
                fund_struct_type;
                        /* The fundamental type used to represent a struct
                           DeclSort::Scope. */
  an_ifc_type_index
                fund_union_type;
                        /* The fundamental type used to represent a union
                           DeclSort::Scope. */
  an_ifc_type_index
                fund_unscoped_enum_type;
                        /* The fundamental type used to represent an unscoped
                           DeclSort::Enumeration. */
  Dyn_array<a_scope_member_array, General_allocator>
                scope_members;
                        /* Each scope offset maps to an array in scope members
                           containing the members of that scope.  This data is
                           then used to generate the scope membership IFC
                           information after declarations are mapped. */
  Ptr_map<a_tagged_pointer, an_ifc_decl_index, General_allocator>
                il_entry_to_decl = {/*mask_width=*/10};
                        /* A map of IL entries to their associated IFC decl
                           indexes. */
};  /* an_ifc_il_map */

}  /* namespace */

static void dump_scope_recursively(an_ifc_il_map *il_map,
                                   a_scope_ptr   scope);

namespace {

an_ifc_type_index an_ifc_il_map::find_or_enter_type(a_type_ptr type)
/*
For the given type find or enter the type into the IFC output state.  Return
the type index for the type file.
*/
{
  an_ifc_type_index result = this->type_map.get(type);

  if (is_null_index(result)) {
    result = this->enter_type(type);
  }  /* if */
  return result;
}  /* an_ifc_il_map::find_or_enter_type */


static constexpr a_type_qualifier_set
                ifc_common_qualifiers = TQ_CONST | TQ_VOLATILE | TQ_RESTRICT;
                        /* These are the qualifiers natively representable by
                           the IFC TypeSort::Qualified type. */


static inline a_boolean typeref_has_any_common_qualifiers(a_type_ptr type)
/*
If the type has any of the common type qualifiers supported natively by the IFC
format (const, volatile, or __restrict) return TRUE; otherwise, return FALSE.
*/
{
  check_assertion(type->kind == tk_typeref);

  return (type->variant.typeref.qualifiers & ifc_common_qualifiers) != 0;
}  /* typeref_has_any_common_qualifiers */


static inline a_boolean typeref_has_any_extended_qualifiers(a_type_ptr type)
/*
If the type has any of type qualifiers supported by EDG but not natively by the
IFC format return TRUE; otherwise, return FALSE.
*/
{
  return (type->variant.typeref.qualifiers & ~ifc_common_qualifiers) != 0;
}  /* typeref_has_any_extended_qualifiers */


an_ifc_type_index an_ifc_il_map::enter_type(a_type_ptr type)
/*
For the given type enter the type into the IFC output state.  Return the type
index for the type file.
*/
{
  check_assertion(is_null_index(this->type_map.get(type)));
  an_ifc_type_index result;

  switch (type->kind) {
    case tk_class:
    case tk_struct:
    case tk_union:
      result = this->enter_class_struct_union_type(type);
      break;
    case tk_float:
      result = this->enter_float_type(type);
      break;
    case tk_integer:
      if (is_enum_type(type)) {
        result = this->enter_enum_type(type);
      } else {
        result = this->enter_integer_type(type);
      }  /* if */
      break;
    case tk_nullptr:
      result = this->enter_nullptr_type(type);
      break;
    case tk_pointer:
      result = this->enter_pointer_type(type);
      break;
    case tk_routine:
      result = this->enter_routine_type(type);
      break;
    case tk_template_param:
      /* This type should be entered when the template parameter chart
         is set up in enter_template_params. */
      unexpected_condition();
      break;
    case tk_typeref:
      if (typeref_is_typedef(type)) {
        /* If a typedef has its own qualifiers the IL type will be mapped to
           two distinct IFC types.  Thus, if this case is encountered either
           the typedef shouldn't have been directly given qualifiers or this
           code needs to be updated. */
        check_assertion(type->variant.typeref.qualifiers == TQ_NONE);
        result = this->enter_typedef_type(type);
      } else if (is_typeref_kind(type, trk_is_decltype)) {
        /* FIXME: This is almost definitely incomplete (e.g., dependent cases).
           The IFC has a decltype type node for representing decltype
           expressions (presumably we only need this in dependent cases?). */
        a_type_ptr underlying_type = type->variant.typeref.type;

        result = this->find_or_enter_type(underlying_type);
      } else if (is_typeref_kind(type, trk_none)) {
        a_type_ptr underlying_type = type->variant.typeref.type;

        result = this->find_or_enter_type(underlying_type);
      } else {
        /* FIXME: Handle other kinds of typerefs. */
        ifc_write_catastrophe();
      }  /* if */
      /* If the typeref adds qualifiers, add a qualifying type. */
      if (typeref_has_any_extended_qualifiers(type)) {
        /* FIXME: Handle the extended qualifiers. */
        ifc_write_catastrophe();
      } else if (typeref_has_any_common_qualifiers(type)) {
        an_ifc_type_qualified
                qualified_type;
        an_ifc_type_index
                unqualified_type = result;

        /* Update the result and type mapping to include the qualifiers. */
        result = this->map_new_type(type, &qualified_type);
        set_ifc_unqualified(&qualified_type, unqualified_type);

        /* Apply the appropriate qualifiers. */
        an_ifc_qualifier_bitfield_query
                qualifers = (an_ifc_qualifier_bitfield_query)0;
        if (type->variant.typeref.qualifiers & TQ_CONST) {
          qualifers = qualifers | ifc_qb_const;
        }  /* if */
        if (type->variant.typeref.qualifiers & TQ_VOLATILE) {
          qualifers = qualifers | ifc_qb_volatile;
        }  /* if */
        if (type->variant.typeref.qualifiers & TQ_RESTRICT) {
          qualifers = qualifers | ifc_qb_restrict;
        }  /* if */

        an_ifc_qualifier_bitfield_storage
                ifc_raw_qualifiers = to_bitmask(qualified_type.get_file(),
                                                qualifers);
        an_ifc_qualifier_bitfield
                ifc_qualifiers(qualified_type.get_file(), ifc_raw_qualifiers);
        set_ifc_qualifiers(&qualified_type, ifc_qualifiers);
      }  /* if */
      break;
    case tk_void:
      result = this->enter_void_type(type);
      break;
    case tk_error:
#if FIXED_POINT_ALLOWED
    case tk_fixed_point:
#endif /* FIXED_POINT_ALLOWED */
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_imaginary:
    case tk_complex:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case tk_array:
    case tk_ptr_to_member:
#if GNU_VECTOR_TYPES_ALLOWED
    case tk_vector:
    case tk_scalable_vector:
    case tk_scalable_vector_count:
    case tk_riscv_vector:
    case tk_mfp8:
    case tk_float8e4m3:
    case tk_float8e5m2:
#endif /* GNU_VECTOR_TYPES_ALLOWED */
    case tk_reflection:
    case tk_unknown:
      break;
    default_is_unexpected();
  }  /* switch */
  return result;
}  /* an_ifc_il_map::enter_type */


an_ifc_scope_offset an_ifc_il_map::find_or_enter_scope(a_scope_ptr scope)
/*
For the given scope enter the scope into the IFC output state.  Return the
scope offset for the type file.
*/
{
  an_ifc_scope_offset result = this->scope_map.get(scope);

  if (is_null_index(result)) {
    result = this->enter_scope(scope);
  }  /* if */
  return result;
}  /* an_ifc_il_map::find_or_enter_scope */


an_ifc_scope_offset an_ifc_il_map::enter_scope(a_scope_ptr scope)
/*
For the given scope enter the scope into the IFC output state.  Return the
scope offset for the type file.
*/
{
  check_assertion(is_null_index(this->scope_map.get(scope)));
  an_ifc_scope_descriptor
                descriptor;
  size_t        part_offset = this->output_state->alloc_node(&descriptor);
  an_ifc_scope_offset
                result(descriptor.get_file(), part_offset + 1);

  this->scope_map.map(scope, result);
  this->scope_members.push_back(a_scope_member_array());
  /* If this assertion fails a scope member array is missing for one or more
     IFC scope descriptors. */
  check_assertion((size_t)this->scope_members.length() == (part_offset + 1));
  return result;
}  /* an_ifc_il_map::enter_scope */

}  /* namespace */

static inline an_ifc_access_sort convert_access_specifier(
                                                    an_access_specifier access)
/*
Given a front end access specifier return the corresponding IFC access sort.
*/
{
  an_ifc_access_sort result = ifc_as_none;

  switch (access) {
    case as_public:
      result = ifc_as_public;
      break;
    case as_protected:
      result = ifc_as_protected;
      break;
    case as_private:
      result = ifc_as_private;
      break;
    case as_inaccessible:
      /* The inaccessible access level should not make it to IFC writing.  If
         an inaccessible access level needs written to the IFC this would
         require a vendor extension (most likely in the form of some sort of
         optional IFC trait) to be implemented for both the writer and
         reader. */
      unexpected_condition();
    default_is_unexpected();
  }  /* switch */
  return result;
}  /* convert_access_specifier */


static inline an_ifc_access_sort access_specifier_of(
                                                  a_source_correspondence *scp)
/*
Given a front end source correspondence return the corresponding IFC access
sort.
*/
{
  return convert_access_specifier((an_access_specifier)scp->access);
}  /* access_specifier_of */


template<typename a_Type>
static inline an_ifc_access_sort access_specifier_of(a_Type *il_entity)
/*
Return the IFC access sort representing the access of the given IL entity.
*/
{
  return access_specifier_of(&il_entity->source_corresp);
}  /* access_specifier_of */

namespace {

an_ifc_decl_index an_ifc_il_map::find_or_enter_class_struct_union(
                                                               a_type_ptr type)
/*
For the given class, struct, or union type find or enter the respective
declaration into the IFC output state.  Return the declaration index for the
entered declaration.
*/
{
  an_ifc_decl_index result = this->il_entry_to_decl.get(make_tagged_ptr(type));

  if (is_null_index(result)) {
    result = this->enter_class_struct_union(type);
  }  /* if */
  return result;
}  /* an_ifc_il_map::find_or_enter_class_struct_union */


an_ifc_decl_index an_ifc_il_map::enter_class_struct_union(a_type_ptr type)
/*
Enter the given class, struct, or union type into the IFC output state.  Return
the declaration index for the entered declaration.
*/
{
  check_assertion(is_class_struct_union_type(type));
  an_ifc_decl_scope
                scope_decl;
  an_ifc_decl_index
                result = this->map_new_decl(type, &scope_decl);

  /* Set the name information. */
  an_ifc_name_index
                ifc_name_index = this->entity_name_as_name_index(type);
  set_ifc_name(&scope_decl, ifc_name_index);

  /* Set the source location information. */
  an_ifc_source_location
                ifc_src_pos = this->find_or_enter_entity_pos(type);
  set_ifc_locus(&scope_decl, ifc_src_pos);

  /* Set the IFC fundamental type to indicate this is a class scope. */
  an_ifc_type_index
                ifc_scope_type;
  switch (type->kind) {
    case tk_class:
      ifc_scope_type = this->find_or_enter_class_scope_type();
      break;
    case tk_struct:
      ifc_scope_type = this->find_or_enter_struct_scope_type();
      break;
    case tk_union:
      ifc_scope_type = this->find_or_enter_union_scope_type();
      break;
    default:
      ifc_write_catastrophe();
      break;
  }  /* switch */
  set_ifc_type(&scope_decl, ifc_scope_type);

  /* FIXME: Set base. */
  /* Associate the class scope decl with its contents. */
  a_scope_ptr   class_scope = class_type_supp(type)->assoc_scope;
  a_template    *assoc_templ = class_type_supp(type)->assoc_template;
  if (class_scope != NULL && assoc_templ == NULL) {
    /* This traversal is not performed for class templates.  The class template
       definition will be associated below via a trait. */
    an_ifc_scope_offset
                initializer = this->find_or_enter_scope(class_scope);

    set_ifc_initializer(&scope_decl, initializer);
    for (a_field_ptr fp = fields_of(type); fp != NULL; fp = fp->next) {
      (void)this->enter_field(fp);
    }  /* for */
    /* Traverse the class scope's contents. */
    dump_scope_recursively(this, class_scope);
  }  /* if */

  /* Set the scope information. */
  an_ifc_decl_index
                scope_decl_idx = this->associate_entity_home_scope(type);
  set_ifc_home_scope(&scope_decl, scope_decl_idx);
  /* FIXME: Set alignment. */
  /* FIXME: Set pack size. */
  /* FIXME: Set specifiers. */
  /* FIXME: Set traits. */

  /* Set the access specifier. */
  an_ifc_access_sort ifc_access = access_specifier_of(type);
  set_ifc_access(&scope_decl, ifc_access);

  /* Set the properties. */
  an_ifc_reachable_properties_bitfield_query
                properties = (an_ifc_reachable_properties_bitfield_query)0;
  /* Flag that an initializer is present if relevant. */
  if (assoc_templ != NULL) {
    an_ifc_edg_trait_class_template_definition
                def_trait;
    this->output_state->alloc_decl_trait(result, &def_trait);

    /* Create a token cache representation of the initializing constant. */
    an_ifc_output_token_cache
                init_token_cache = this->copy_template_body_to_cache(
                                                                  assoc_templ);
    an_ifc_edg_token_cache_offset
                token_cache_offset = this->output_state->alloc_token_cache(
                                                             init_token_cache);
    set_ifc_initializer(&def_trait, token_cache_offset);
    /* Apply the initializer flag to indicate the presence of this
       definition. */
    properties = properties | ifc_rpb_initializer;
  } else if (!type->incomplete) {
    properties = properties | ifc_rpb_initializer;
  }  /* if */

  an_ifc_reachable_properties_bitfield_storage
                ifc_raw_properties = to_bitmask(scope_decl.get_file(),
                                                properties);
  an_ifc_reachable_properties_bitfield
                ifc_properties(scope_decl.get_file(), ifc_raw_properties);
  set_ifc_properties(&scope_decl, ifc_properties);
  return result;
}  /* an_ifc_il_map::enter_class_struct_union */


an_ifc_decl_index an_ifc_il_map::find_or_enter_enum(a_type_ptr type)
/*
For the given enumeration type find or enter the enumeration into the IFC
output state.  Return the declaration index for the enumeration.
*/
{
  an_ifc_decl_index result = this->il_entry_to_decl.get(make_tagged_ptr(type));

  if (is_null_index(result)) {
    result = this->enter_enum(type);
  }  /* if */
  return result;
}  /* an_ifc_il_map::find_or_enter_enum */


an_ifc_decl_index an_ifc_il_map::enter_enum(a_type_ptr type)
/*
For the given enumeration type enter the enumeration into the IFC output state.
Return the declaration index for the enumeration.
*/
{
  check_assertion(type->kind == tk_enum && type->variant.integer.enum_type);
  an_ifc_decl_enumeration
                enum_decl;
  an_ifc_decl_index
                result = this->map_new_decl(type, &enum_decl);
  /* Set the name information. */
  an_ifc_text_offset
                ifc_name_offset = this->entity_name_as_text_offset(type);

  set_ifc_name(&enum_decl, ifc_name_offset);

  /* Set the source location information. */
  an_ifc_source_location
                ifc_src_pos = this->find_or_enter_entity_pos(type);
  set_ifc_locus(&enum_decl, ifc_src_pos);

  /* Set the type information. */
  an_ifc_type_index
                type_idx;
  if (type->variant.integer.is_scoped_enum) {
    type_idx = this->find_or_enter_scoped_enum_type();
  } else {
    type_idx = this->find_or_enter_unscoped_enum_type();
  }  /* if */
  set_ifc_type(&enum_decl, type_idx);

  /* Set the base type. */
  an_integer_kind
                base_int_kind = type->variant.integer.int_kind;
  a_type_ptr    base_type = integer_type(base_int_kind);
  an_ifc_type_index
                base_type_idx = this->find_or_enter_type(base_type);
  set_ifc_base(&enum_decl, base_type_idx);

  /* Construct and set the initializer to provide the enumerators. */
  an_ifc_sequence seq = this->enter_enumerators(type);
  set_ifc_initializer(&enum_decl, seq);

  /* Set the scope information. */
  an_ifc_decl_index
                scope_decl_idx = this->associate_entity_home_scope(type);
  set_ifc_home_scope(&enum_decl, scope_decl_idx);
  /* FIXME: Set alignment. */
  /* FIXME: Set specifiers. */

  /* Set the access specifier. */
  an_ifc_access_sort ifc_access = access_specifier_of(type);
  set_ifc_access(&enum_decl, ifc_access);
  /* FIXME: Set properties. */
  return result;
}  /* an_ifc_il_map::enter_enum */


an_ifc_decl_index an_ifc_il_map::find_or_enter_namespace(a_namespace_ptr nsp)
/*
For the given namespace find or enter the respective declaration into the IFC
output state.  Return the declaration index for the entered declaration.
*/
{
  an_ifc_decl_index result = this->il_entry_to_decl.get(make_tagged_ptr(nsp));

  if (is_null_index(result)) {
    result = this->enter_namespace(nsp);
  }  /* if */
  return result;
}  /* an_ifc_il_map::find_or_enter_namespace */


an_ifc_decl_index an_ifc_il_map::enter_namespace(a_namespace_ptr nsp)
/*
For the given namespace enter the respective declaration into the IFC output
state.  Return the declaration index for the entered declaration.
*/
{
  an_ifc_decl_scope
                scope_decl;
  an_ifc_decl_index
                result = this->map_new_decl(nsp, &scope_decl);

  /* Set the name information. */
  an_ifc_name_index
                ifc_name_index = this->entity_name_as_name_index(nsp);
  set_ifc_name(&scope_decl, ifc_name_index);

  /* Set the source location information. */
  an_ifc_source_location
                ifc_src_pos = this->find_or_enter_entity_pos(nsp);
  set_ifc_locus(&scope_decl, ifc_src_pos);

  /* Set the IFC fundamental type to indicate this is a namespace scope. */
  an_ifc_type_index
                ifc_scope_type = this->find_or_enter_namespace_scope_type();
  set_ifc_type(&scope_decl, ifc_scope_type);

  /* Namespaces do not have a base type, so use a null type. */
  an_ifc_type_index
                base_type;
  set_ifc_base(&scope_decl, base_type);

  /* Set the scope information. */
  an_ifc_decl_index
                scope_decl_idx = this->associate_entity_home_scope(nsp);
  set_ifc_home_scope(&scope_decl, scope_decl_idx);
  /* FIXME: Set alignment. */
  /* FIXME: Set pack_size. */
  /* FIXME: Set specifiers. */
  /* FIXME: Set traits. */

  /* Set the access specifier. */
  set_ifc_access(&scope_decl, ifc_as_none);
  /* FIXME: Set properties. */
  return result;
}  /* an_ifc_il_map::enter_namespace */


an_ifc_decl_index an_ifc_il_map::find_or_enter_routine(a_routine_ptr rp)
/*
For the given routine find or enter the function declaration into the IFC
output state.  Return the declaration index for the routine.
*/
{
  an_ifc_decl_index result = this->il_entry_to_decl.get(make_tagged_ptr(rp));

  if (is_null_index(result)) {
    result = this->enter_routine(rp);
  }  /* if */
  return result;
}  /* an_ifc_il_map::find_or_enter_routine */


an_ifc_decl_index an_ifc_il_map::enter_routine(a_routine_ptr rp)
/*
For the given routine enter the function declaration into the IFC output state.
Return the declaration index for the routine.
*/
{
  an_ifc_decl_index result;

  switch (rp->special_kind) {
    case sfk_none:
      if (routine_type_is_nonstatic_member_function(rp->type)) {
        result = this->enter_member_function(rp);
      } else {
        result = this->enter_free_function(rp);
      }  /* if */
      break;
    case sfk_constructor:
      result = this->enter_constructor(rp);
      break;
    case sfk_conversion:
      /* FIXME: Implement these. */
      break;
    case sfk_destructor:
      result = this->enter_destructor(rp);
      break;
    case sfk_operator:
      /* FIXME: Implement these. */
      break;
#if BUILTIN_FUNCTIONS_ENABLED
    case sfk_builtin_operator_delete:
    case sfk_builtin_operator_new:
#endif /* BUILTIN_FUNCTIONS_ENABLED */
    case sfk_deduction_guide:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case sfk_dispose_bool:
    case sfk_event_add:
    case sfk_event_raise:
    case sfk_event_remove:
    case sfk_finalizer:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if BUILTIN_FUNCTIONS_ENABLED
    case sfk_gnu_atomic_generic_function:
    case sfk_gnu_atomic_nongeneric_function:
    case sfk_gnu_sync_concrete_function:
#endif /* BUILTIN_FUNCTIONS_ENABLED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case sfk_idisposable_dispose:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case sfk_lambda_entry_point:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case sfk_object_finalize:
    case sfk_property_get:
    case sfk_property_set:
    case sfk_static_constructor:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case sfk_udl_operator:
      ifc_write_catastrophe();
      break;
    case sfk_last:
      /* sfk_last should not appear in the IL. */
      unexpected_condition();
    default_is_unexpected();
  }  /* switch */
  return result;
}  /* an_ifc_il_map::enter_routine */


an_ifc_decl_index an_ifc_il_map::find_or_enter_template(a_template_ptr templ)
/*
For the given template find or enter the template declaration into the IFC
output state.  Return the declaration index for the template.
*/
{
  a_tagged_pointer  tagged_templ = make_tagged_ptr(templ);
  an_ifc_decl_index result = this->il_entry_to_decl.get(tagged_templ);

  if (is_null_index(result)) {
    result = this->enter_template(templ);
  }  /* if */
  return result;
}  /* an_ifc_il_map::find_or_enter_template */


static inline a_boolean is_alias_template(a_template_ptr templ)
/*
Return TRUE if the given template is an alias template; otherwise, return
FALSE.
*/
{
  a_symbol_ptr          template_sym = symbol_for(templ);
  a_template_symbol_supplement_ptr
                        tssp = template_sym->variant.template_info;

  return tssp->variant.class_template.is_alias_template;
}  /* is_alias_template */


an_ifc_decl_index an_ifc_il_map::enter_template(a_template_ptr templ)
/*
For the given template enter the template declaration into the IFC output
state.  Return the declaration index for the template.
*/
{
  an_ifc_decl_index result;

  switch (templ->kind) {
    case templk_class:
      if (is_alias_template(templ)) {
        result = this->enter_alias_template(templ);
      } else {
        result = this->enter_class_template(templ);
      }  /* if */
      break;
    case templk_concept:
      ifc_write_catastrophe();
      break;
    case templk_function:
      result = this->enter_function_template(templ);
      break;
    case templk_member_class:
    case templk_member_enum:
    case templk_member_function:
    case templk_none:
    case templk_static_data_member:
    case templk_template_template_param:
    case templk_variable:
      ifc_write_catastrophe();
      break;
    default_is_unexpected();
  }  /* switch */
  return result;
}  /* an_ifc_il_map::enter_template */


an_ifc_decl_index an_ifc_il_map::find_or_enter_using_directive(
                                                        a_using_decl_ptr udp,
                                                        a_scope_ptr      scope)
/*
For the given using directive find or enter the using directive in the given
scope into the IFC output state.  Return the declaration index for the using
directive.

Note this function explicitly requires a scope as using directives do not have
a source correspondence and thus must explicitly be provided a scope.
*/
{
  a_tagged_pointer  tagged_udp = make_tagged_ptr(udp);
  an_ifc_decl_index result = this->il_entry_to_decl.get(tagged_udp);

  if (is_null_index(result)) {
    result = this->enter_using_directive(udp, scope);
  }  /* if */
  return result;
}  /* an_ifc_il_map::find_or_enter_using_directive */


an_ifc_decl_index an_ifc_il_map::enter_using_directive(a_using_decl_ptr udp,
                                                       a_scope_ptr      scope)
/*
For the given using directive enter the using directive in the given scope into
the IFC output state.  Return the declaration index for the using directive.

Note this function explicitly requires a scope as using directives do not have
a source correspondence and thus must explicitly be provided a scope.
*/
{
  an_ifc_decl_barren
                barren_decl;
  an_ifc_decl_index
                result = this->map_new_decl(udp, &barren_decl);
  /* Set up the directive. */
  an_ifc_dir_using
                using_dir;
  an_ifc_dir_index
                using_dir_idx = this->output_state->alloc_dir(&using_dir);
  /* Set the directive location. */
  an_ifc_source_location
                ifc_src_pos = this->find_or_enter_pos(udp->position);

  set_ifc_locus(&using_dir, ifc_src_pos);
  /* FIXME: Implement the "nominated" (as spelled in source form). */

  /* Set the resolved scope. */
  /* FIXME: Check the entity type. */
  an_ifc_decl_index
                namespace_idx = this->find_or_enter_namespace(
                                             (a_namespace_ptr)udp->entity.ptr);
  /* It is expected that the namespace will have already been entered. */
  check_assertion(!is_null_index(namespace_idx));
  set_ifc_resolution(&using_dir, namespace_idx);
  /* Set the directive on the barren declaration. */
  set_ifc_directive(&barren_decl, using_dir_idx);
  /* FIXME: Set specifiers. */

  /* Set the access specifier. */
  an_ifc_access_sort
                access_sort = convert_access_specifier(udp->access);
  set_ifc_access(&barren_decl, access_sort);
  /* Associate the using-directive with its scope. */
  this->associate_entity_home_scope(udp, scope);
  return result;
}  /* an_ifc_il_map::enter_using_directive */


an_ifc_text_offset an_ifc_il_map::string_as_text_offset(a_const_char *str,
                                                        size_t       str_len)
/*
Return the text offset representing the given string of the given length.  The
lifetime of the given string must be at least as long as the lifetime of the
IFC IL map.
*/
{
  size_t name_offset = 0;

  if (str != NULL && str[0] != '\0') {
    a_string_view str_handle(str, str_len);
    uintptr_t     hash = hash_ptr(str_handle);

    name_offset = this->string_table_map.get_with_hash(str_handle, hash);
    if (name_offset == 0) {
      /* The name does not yet exist in the string table.  Add it now. */
      name_offset = this->output_state->add_to_string_table(str);
      this->string_table_map.map_with_hash(str_handle, name_offset, hash);
    }  /* if */
  }  /* if */
  return an_ifc_text_offset(this->get_default_file(), name_offset);
}  /* an_ifc_il_map::string_as_text_offset */


an_ifc_text_offset an_ifc_il_map::string_as_text_offset(a_const_char *str)
/*
Return the text offset representing the given string.  The lifetime of the
given string must be at least as long as the lifetime of the IFC IL map.
*/
{
  an_ifc_text_offset result;

  if (str != NULL && str[0] != '\0') {
    result = this->string_as_text_offset(str, strlen(str));
  }  /* if */
  return result;
}  /* an_ifc_il_map::string_as_text_offset */


an_ifc_name_index an_ifc_il_map::string_as_name_index(a_const_char *str)
/*
Return the name index representing the given string.  The lifetime of the given
string must be at least as long as the lifetime of the IFC IL map.
*/
{
  an_ifc_text_offset text_offset = this->string_as_text_offset(str);

  return an_ifc_name_index(this->get_default_file(), ifc_ns_text_offset,
                           (an_ifc_text_offset_storage)text_offset);
}  /* an_ifc_il_map::string_as_name_index */


template<typename a_Type>
an_ifc_text_offset an_ifc_il_map::entity_name_as_text_offset(a_Type *il_entity)
/*
Return the text offset representing the name of the given entity.
*/
{
  return this->string_as_text_offset(il_entity->source_corresp.name);
}  /* an_ifc_il_map::entity_name_as_text_offset */


template<typename a_Type>
an_ifc_name_index an_ifc_il_map::entity_name_as_name_index(a_Type *il_entity)
/*
Return the name index representing the name of the given entity.
*/
{
  return this->string_as_name_index(il_entity->source_corresp.name);
}  /* an_ifc_il_map::entity_name_as_name_index */


an_ifc_name_index an_ifc_il_map::find_or_enter_src_file(
                                                       a_source_file_ptr file)
/*
For the given source file find or enter the file into the IFC output state.
Return the name index for the source file.
*/
{
  an_ifc_name_index result = this->src_file_map.get(file);

  if (is_null_index(result)) {
    result = this->enter_src_file(file);
  }  /* if */
  return result;
}  /* an_ifc_il_map::find_or_enter_src_file */


an_ifc_name_index an_ifc_il_map::enter_src_file(a_source_file_ptr file)
/*
For the given source file enter the file into the IFC output state.  Return the
name index for the source file.
*/
{
  an_ifc_name_source_file
                src_file;
  an_ifc_name_index
                result = this->output_state->alloc_name(&src_file);
  size_t        path_offset = this->output_state->add_to_string_table(
                                                             file->file_name);
  an_ifc_text_offset
                ifc_path_offset(src_file.get_file(), path_offset);

  set_ifc_path(&src_file, ifc_path_offset);
  this->src_file_map.map(file, result);
  return result;
}  /* an_ifc_il_map::enter_src_file */

}  /* namespace */

static inline a_seq_number_key
make_seq_num_map_key(const a_source_position &pos)
/*
Given a source position, return the corresponding hash map key.
*/
{
  return a_seq_number_key(pos.seq + 1);
}  /* make_seq_num_map_key */

namespace {

an_ifc_source_location an_ifc_il_map::find_or_enter_null_pos()
/*
Find or enter the NULL source position into the IFC output state.  Return the
corresponding IFC source location.
*/
{
  return this->find_or_enter_pos(null_source_position);
}  /* an_ifc_il_map::find_or_enter_null_pos */


an_ifc_source_location an_ifc_il_map::find_or_enter_pos(
                                                 const a_source_position &pos)
/*
For the given source position find or enter the source position into the IFC
output state.  Return the corresponding IFC source location.
*/
{
  an_ifc_source_location result;
  an_ifc_line_offset     ifc_line_offset = this->seq_number_map.get(
                                                   make_seq_num_map_key(pos));

  if (is_null_index(ifc_line_offset)) {
    result = this->enter_pos(pos);
  } else {
    an_ifc_column ifc_column(ifc_line_offset.get_file(), pos.column);

    result = {ifc_line_offset.get_file()};
    set_ifc_line(&result, ifc_line_offset);
    set_ifc_column(&result, ifc_column);
  }  /* if */
  return result;
}  /* an_ifc_il_map::find_or_enter_pos */


an_ifc_source_location an_ifc_il_map::enter_pos(const a_source_position &pos)
/*
For the given source position enter the source position into the IFC output
state.  Return the corresponding IFC source location.
*/
{
  a_seq_number_key   seq_key = make_seq_num_map_key(pos);

  check_assertion(is_null_index(this->seq_number_map.get(seq_key)));

  an_ifc_line_offset ifc_line_offset;
  if (pos.seq == 0) {
    /* A unknown source line is being referenced. */
    /* Allocate a source line node. */
    an_ifc_source_line
                line;
    size_t      line_offset = this->output_state->alloc_node(&line);
    /* Allocate a source file for the empty source file. */
    an_ifc_name_source_file
                src_file;
    an_ifc_name_index
                src_file_idx = this->output_state->alloc_name(&src_file);

    /* Associate the source file. */
    set_ifc_file(&line, src_file_idx);

    /* Associate an absent line number. */
    an_ifc_line_number
                ifc_line_number(this->get_default_file(), 0);
    set_ifc_line(&line, ifc_line_number);
    /* Use the created line offset. */
    ifc_line_offset = an_ifc_line_offset(line.get_file(), line_offset + 1);
  } else {
    /* A known source line is required. */
    an_ifc_source_line
                line;
    size_t      line_offset = this->output_state->alloc_node(&line);
      /* Gather the information for the source sequence. */
    a_line_number
                line_number;
    a_boolean   at_end_of_source;
    /* physical_line == FALSE means consider information from #line directives
       as well as true file information. */
    a_source_file_ptr
                file = source_file_for_seq(pos.seq, &line_number,
                                           &at_end_of_source,
                                           /*physical_line=*/FALSE);
    an_ifc_name_index
                file_name_idx = this->find_or_enter_src_file(file);
    an_ifc_line_number
                ifc_line_number(line.get_file(), line_number);

    set_ifc_file(&line, file_name_idx);
    set_ifc_line(&line, ifc_line_number);
    /* Use the created line offset. */
    ifc_line_offset = an_ifc_line_offset(line.get_file(), line_offset + 1);
  }  /* if */
  this->seq_number_map.map(seq_key, ifc_line_offset);

  /* Form a result IFC SourceLocation. */
  an_ifc_source_location result(ifc_line_offset.get_file());
  an_ifc_column          ifc_column(result.get_file(), pos.column);
  set_ifc_line(&result, ifc_line_offset);
  set_ifc_column(&result, ifc_column);
  return result;
}  /* an_ifc_il_map::enter_pos */


template<typename a_Type>
an_ifc_source_location an_ifc_il_map::find_or_enter_entity_pos(
                                                             a_Type *il_entity)
/*
For the given entity find or enter the declaration source position into the IFC
output state.  Return the corresponding IFC source location.
*/
{
  a_source_position src_pos = il_entity->source_corresp.decl_position;

  return this->find_or_enter_pos(src_pos);
}  /* an_ifc_il_map::find_or_enter_entity_pos */


template<typename an_ifc_Node_type>
an_ifc_type_index an_ifc_il_map::map_new_type(a_type_ptr       type,
                                              an_ifc_Node_type *node)
/*
For the given IL type, construct a corresponding IFC type node in the IFC
output state.  Return the type index for the constructed type node.
*/
{
  an_ifc_type_index result = this->output_state->alloc_type(node);

  this->type_map.map(type, result);
  return result;
}  /* an_ifc_il_map::map_new_type */


template<typename a_Type, typename an_ifc_Node_type>
an_ifc_decl_index an_ifc_il_map::map_new_decl(a_Type           *il_entity,
                                              an_ifc_Node_type *node)
/*
For the given IL entity, construct a corresponding IFC declaration node in the
IFC output state.  Return the declaration index for the constructed declaration
node.
*/
{
  an_ifc_decl_index result = this->output_state->alloc_decl(node);

  this->il_entry_to_decl.map(make_tagged_ptr(il_entity), result);
  return result;
}  /* an_ifc_il_map::map_new_decl */


an_ifc_type_index an_ifc_il_map::enter_class_struct_union_type(a_type_ptr type)
/*
Enter the given class, struct, or union type into the IFC output state.  Return
the type index for the class type.
*/
{
  check_assertion(is_class_or_struct(type));
  an_ifc_type_index
                result;
  a_template_arg_ptr
                arg_list = class_type_supp(type)->template_arg_list;
  unsigned      arg_count = count_list_elements(arg_list);

  if (arg_count > 0) {
    /* This is a substituted type, enter it as such. */
    an_ifc_edg_type_substituted
                substituted_type;

    result = this->map_new_type(type, &substituted_type);

    a_template_ptr
                templ = class_type_supp(type)->assoc_template;
    an_ifc_decl_index
                ifc_templ_idx = this->find_or_enter_template(templ);
    set_ifc_subject(&substituted_type, ifc_templ_idx);

    /* Preallocate all the arguments to ensure we get one contiguous block. */
    size_t      start = this->output_state->
                alloc_node_block<an_ifc_edg_heap_template_argument>(arg_count);
    /* Associate the preallocated parameter nodes with the type. */
    an_ifc_module_file
                *file = this->get_default_file();
    an_ifc_edg_heap_template_argument_offset
                ifc_arg_start(file, start);
    an_ifc_cardinality
                ifc_arg_count(file, arg_count);
    set_ifc_arguments(&substituted_type, ifc_arg_start);
    set_ifc_num_arguments(&substituted_type, ifc_arg_count);

    /* Complete the arguments. */
    a_template_arg_ptr curr_templ_arg = arg_list;
    for (size_t i = 0; i < arg_count; ++i) {
      size_t                            curr_arg_offset = start + i;
      an_ifc_edg_heap_template_argument curr_arg;

      this->output_state->fetch_node(&curr_arg, curr_arg_offset);

      an_ifc_edg_template_argument_index
                arg_index = this->enter_template_argument(curr_templ_arg);
      set_ifc_index(&curr_arg, arg_index);
      /* Advance to the next argument. */
      curr_templ_arg = curr_templ_arg->next;
    }  /* for */
  } else {
    an_ifc_type_designated
                designated_type;

    result = this->map_new_type(type, &designated_type);

    an_ifc_decl_index
                decl_idx = this->find_or_enter_class_struct_union(type);
    set_ifc_decl(&designated_type, decl_idx);
  }  /* if */
  return result;
}  /* an_ifc_il_map::enter_class_struct_union_type */


an_ifc_type_index an_ifc_il_map::enter_constructor_type(a_type_ptr type)
/*
Enter the type for the given constructor type into the IFC output state.
Return the type index for the constructor type.
*/
{
  an_ifc_type_tor
                tor_type;
  an_ifc_type_index
                result = this->output_state->alloc_type(&tor_type);

  an_ifc_type_index
                ifc_param_types = this->enter_routine_params_type(type);
  set_ifc_source(&tor_type, ifc_param_types);

  /* FIXME: Set eh_spec. */
  /* FIXME: Set convention. */
  return result;
}  /* an_ifc_il_map::enter_constructor_type */


an_ifc_type_index an_ifc_il_map::enter_enum_type(a_type_ptr type)
/*
Enter the given enum type into the IFC output state.  Return the type index for
the enum type.
*/
{
  check_assertion(type->kind == tk_integer);
  an_ifc_type_designated
                designated_type;
  an_ifc_type_index
                result = this->map_new_type(type, &designated_type);
  an_ifc_decl_index
                decl_idx = this->find_or_enter_enum(type);

  set_ifc_decl(&designated_type, decl_idx);
  return result;
}  /* an_ifc_il_map::enter_enum_type */


an_ifc_type_index an_ifc_il_map::enter_float_type(a_type_ptr type)
/*
Enter the given float type into the IFC output state.  Return the type index
for the float type.
*/
{
  check_assertion(type->kind == tk_float);
  an_ifc_type_fundamental
                fund_type;
  an_ifc_type_index
                result = this->map_new_type(type, &fund_type);

  switch (type->variant.float_kind) {
    case fk_float:
      set_ifc_basis(&fund_type, ifc_tbs_float);
      set_ifc_precision(&fund_type, ifc_tps_default);
      break;
    case fk_double:
      set_ifc_basis(&fund_type, ifc_tbs_double);
      set_ifc_precision(&fund_type, ifc_tps_default);
      break;
    case fk_long_double:
      set_ifc_basis(&fund_type, ifc_tbs_double);
      set_ifc_precision(&fund_type, ifc_tps_long);
      break;
    case fk_std_float128:
      set_ifc_basis(&fund_type, ifc_tbs_float);
      set_ifc_precision(&fund_type, ifc_tps_bit128);
      break;
    case fk_std_float16:
      set_ifc_basis(&fund_type, ifc_tbs_float);
      set_ifc_precision(&fund_type, ifc_tps_bit16);
      break;
    case fk_std_float32:
      set_ifc_basis(&fund_type, ifc_tbs_float);
      set_ifc_precision(&fund_type, ifc_tps_bit32);
      break;
    case fk_std_float64:
      set_ifc_basis(&fund_type, ifc_tbs_float);
      set_ifc_precision(&fund_type, ifc_tps_bit64);
      break;
    case fk_float16:
    case fk_float128:
    case fk_fp16:
    case fk_float32x:
    case fk_float64x:
    case fk_float80:
    case fk_std_bfloat16:
    case fk_last:
      /* These extended floating-point kinds have no IFC fundamental-type
         encoding that distinguishes them from other floating-point types
         of the same size ("__fp16" and "__float128" in particular have
         different manglings than "_Float16" and "_Float128"); they will
         be covered by an EDG extension type. */
      ifc_write_catastrophe();
      break;
    default_is_unexpected();
  }  /* switch */
  set_ifc_sign(&fund_type, ifc_tss_signed);
  return result;
}  /* an_ifc_il_map::enter_float_type */


an_ifc_type_index an_ifc_il_map::enter_free_function_type(a_type_ptr type)
/*
Enter the given free function (routine) type into the IFC output state.  Return
the type index for the function type.
*/
{
  check_assertion(type->kind == tk_routine);
  an_ifc_type_function
                func_type;
  an_ifc_type_index
                result = this->map_new_type(type, &func_type);
  a_type_ptr    return_type = type->variant.routine.return_type;
  an_ifc_type_index
                ifc_return_type = this->find_or_enter_type(return_type);

  set_ifc_target(&func_type, ifc_return_type);

  an_ifc_type_index
                ifc_param_types = this->enter_routine_params_type(type);
  set_ifc_source(&func_type, ifc_param_types);
  /* FIXME: Set eh_spec. */
  /* FIXME: Set convention. */
  /* FIXME: Set traits. */
  return result;
}  /* an_ifc_il_map::enter_free_function_type */

}  /* namespace */

static an_ifc_type_precision_sort type_size_to_precision(a_type_ptr type)
/*
Given a type, return the corresponding precision.
*/
{
  an_ifc_type_precision_sort result;

  switch (size_of_type(type)) {
    case 1:
      result = ifc_tps_bit8;
      break;
    case 2:
      result = ifc_tps_bit16;
      break;
    case 4:
      result = ifc_tps_bit32;
      break;
    case 8:
      result = ifc_tps_bit64;
      break;
    case 16:
      result = ifc_tps_bit128;
      break;
    default:
      /* The conversion from type size to type precision is unknown. */
      unexpected_condition();
  }  /* switch */
  return result;
}  /* type_size_to_precision */

namespace {

an_ifc_type_index an_ifc_il_map::enter_integer_type(a_type_ptr type)
/*
Enter the given integer type into the IFC output state.  Return the type index
for the integer type.
*/
{
  check_assertion(type->kind == tk_integer);
  an_ifc_type_fundamental
                fund_type;
  an_ifc_type_index
                result = this->map_new_type(type, &fund_type);

  /* Set the basis, precision, and sign. */
  if (type->variant.integer.bool_type) {
    /* Handle the bool type. */
    set_ifc_basis(&fund_type, ifc_tbs_bool);
    set_ifc_precision(&fund_type, ifc_tps_default);
    set_ifc_sign(&fund_type, ifc_tss_plain);
  } else if (type->variant.integer.wchar_t_type) {
    /* Handle the wchar_t type. */
    set_ifc_basis(&fund_type, ifc_tbs_wchar_t);
    set_ifc_precision(&fund_type, ifc_tps_default);
    set_ifc_sign(&fund_type, ifc_tss_plain);
  } else if (type->variant.integer.char8_t_type) {
    /* Handle the char8_t type. */
    set_ifc_basis(&fund_type, ifc_tbs_char);
    set_ifc_precision(&fund_type, ifc_tps_bit8);
    set_ifc_sign(&fund_type, ifc_tss_plain);
  } else if (type->variant.integer.char16_t_type) {
    /* Handle the char16_t type. */
    set_ifc_basis(&fund_type, ifc_tbs_char);
    set_ifc_precision(&fund_type, ifc_tps_bit16);
    set_ifc_sign(&fund_type, ifc_tss_plain);
  } else if (type->variant.integer.char32_t_type) {
    /* Handle the char32_t type. */
    set_ifc_basis(&fund_type, ifc_tbs_char);
    set_ifc_precision(&fund_type, ifc_tps_bit32);
    set_ifc_sign(&fund_type, ifc_tss_plain);
  } else {
    switch (type->variant.integer.int_kind) {
      case ik_char:
        /* Handle the char type. */
        set_ifc_basis(&fund_type, ifc_tbs_char);
        set_ifc_precision(&fund_type, ifc_tps_default);
        set_ifc_sign(&fund_type, ifc_tss_plain);
        break;
      case ik_signed_char:
        /* Handle the signed char type. */
        set_ifc_basis(&fund_type, ifc_tbs_char);
        set_ifc_precision(&fund_type, ifc_tps_default);
        set_ifc_sign(&fund_type, ifc_tss_signed);
        break;
      case ik_unsigned_char:
        /* Handle the unsigned char type. */
        set_ifc_basis(&fund_type, ifc_tbs_char);
        set_ifc_precision(&fund_type, ifc_tps_default);
        set_ifc_sign(&fund_type, ifc_tss_unsigned);
        break;
      case ik_short:
        /* Handle the (signed) short type. */
        set_ifc_basis(&fund_type, ifc_tbs_int);
        set_ifc_precision(&fund_type, ifc_tps_short);
        set_ifc_sign(&fund_type, ifc_tss_signed);
        break;
      case ik_unsigned_short:
        /* Handle the unsigned short type. */
        set_ifc_basis(&fund_type, ifc_tbs_int);
        set_ifc_precision(&fund_type, ifc_tps_short);
        set_ifc_sign(&fund_type, ifc_tss_unsigned);
        break;
      case ik_int:
        /* Handle the (signed) int type. */
        set_ifc_basis(&fund_type, ifc_tbs_int);
        set_ifc_precision(&fund_type, ifc_tps_default);
        set_ifc_sign(&fund_type, ifc_tss_signed);
        break;
      case ik_unsigned_int:
        /* Handle the unsigned int type. */
        set_ifc_basis(&fund_type, ifc_tbs_int);
        set_ifc_precision(&fund_type, ifc_tps_default);
        set_ifc_sign(&fund_type, ifc_tss_unsigned);
        break;
      case ik_long:
        /* Handle the (signed) long type. */
        set_ifc_basis(&fund_type, ifc_tbs_int);
        set_ifc_precision(&fund_type, ifc_tps_long);
        set_ifc_sign(&fund_type, ifc_tss_signed);
        break;
      case ik_unsigned_long:
        /* Handle the unsigned long type. */
        set_ifc_basis(&fund_type, ifc_tbs_int);
        set_ifc_precision(&fund_type, ifc_tps_long);
        set_ifc_sign(&fund_type, ifc_tss_unsigned);
        break;
      default:
        /* Handle miscellaneous fixed size integer types (e.g., uint32_t,
           int32_t, etc). */
        { an_ifc_type_precision_sort
                  precision_sort = type_size_to_precision(type);
          an_ifc_type_sign_sort
                  sign_sort = is_signed_integral_type(type) ? ifc_tss_signed
                                                            : ifc_tss_unsigned;
          set_ifc_basis(&fund_type, ifc_tbs_int);
          set_ifc_precision(&fund_type, precision_sort);
          set_ifc_sign(&fund_type, sign_sort);
        }
        break;
    }  /* switch */
  }  /* if */
  return result;
}  /* an_ifc_il_map::enter_integer_type */


an_ifc_type_index an_ifc_il_map::enter_member_function_type(a_type_ptr type)
/*
Enter the given member function (routine) type into the IFC output state.
Return the type index for the function type.
*/
{
  check_assertion(type->kind == tk_routine);
  an_ifc_type_method
                func_type;
  an_ifc_type_index
                result = this->map_new_type(type, &func_type);
  a_type_ptr    return_type = type->variant.routine.return_type;
  an_ifc_type_index
                ifc_return_type = this->find_or_enter_type(return_type);

  set_ifc_target(&func_type, ifc_return_type);

  an_ifc_type_index
                ifc_param_types = this->enter_routine_params_type(type);
  set_ifc_source(&func_type, ifc_param_types);

  a_routine_type_supplement_ptr
                rtsp = type->variant.routine.extra_info;
  a_type_ptr    class_type = rtsp->this_class;
  an_ifc_type_index
                ifc_class_type  = this->find_or_enter_type(class_type);
  set_ifc_scope(&func_type, ifc_class_type);
  /* FIXME: Set eh_spec. */
  /* FIXME: Set convention. */
  /* FIXME: Set traits. */
  return result;
}  /* an_ifc_il_map::enter_member_function_type */


an_ifc_type_index an_ifc_il_map::enter_nullptr_type(a_type_ptr type)
/*
Enter the given nullptr type into the IFC output state.  Return the type index
for the nullptr type.
*/
{
  check_assertion(type->kind == tk_nullptr);
  an_ifc_type_fundamental
                fund_type;
  an_ifc_type_index
                result = this->map_new_type(type, &fund_type);

  set_ifc_basis(&fund_type, ifc_tbs_nullptr);
  set_ifc_precision(&fund_type, ifc_tps_default);
  set_ifc_sign(&fund_type, ifc_tss_plain);
  return result;
}  /* an_ifc_il_map::enter_nullptr_type */


an_ifc_type_index an_ifc_il_map::enter_pointer_type(a_type_ptr type)
/*
Enter the given pointer type into the IFC output state.  Return the type index
for the pointer type.
*/
{
  check_assertion(type->kind == tk_pointer);
  an_ifc_type_index result;
  a_type_ptr        pointee_type = type->variant.pointer.type;
  an_ifc_type_index ifc_pointee_type = this->find_or_enter_type(pointee_type);

  if (type->variant.pointer.is_reference) {
    if (type->variant.pointer.is_rvalue_reference) {
      an_ifc_type_lvalue_reference ref_type;

      result = this->map_new_type(type, &ref_type);
      set_ifc_referee(&ref_type, ifc_pointee_type);
    } else {
      an_ifc_type_rvalue_reference ref_type;

      result = this->map_new_type(type, &ref_type);
      set_ifc_referee(&ref_type, ifc_pointee_type);
    }  /* if */
  } else {
    an_ifc_type_pointer ptr_type;

    result = this->map_new_type(type, &ptr_type);
    set_ifc_pointee(&ptr_type, ifc_pointee_type);
  }  /* if */
  return result;
}  /* an_ifc_il_map::enter_pointer_type */


an_ifc_type_index an_ifc_il_map::enter_routine_params_type(a_type_ptr type)
/*
Given a routine type, find or enter the types for the routine's parameters.
Return the IFC type index of the parameter types.
*/
{
  check_assertion(type->kind == tk_routine);
  an_ifc_type_index
                result;
  Small_dyn_array<an_ifc_type_index, 10, General_allocator>
                param_types;
  a_param_type_ptr
                curr_param_type = function_type_params(type);

  /* Convert each parameter type and collect the converted types. */
  while (curr_param_type != NULL) {
    an_ifc_type_index ifc_param_type =
                              this->find_or_enter_type(curr_param_type->type);

    param_types.push_back(ifc_param_type);
    curr_param_type = curr_param_type->next;
  }  /* while */

  if (param_types.length() == 0) {
    /* Do nothing, the result type is a null index as there are no
       parameters. */
  } else if (param_types.length() == 1) {
    /* There is only one parameter type, use it directly. */
    result = param_types[0];
  } else {
    /* Add the parameter types to the type heap and track the type heap offset
       of the first type added to the heap. */
    a_boolean first = TRUE;
    size_t    heap_start_offset = 0;

    for (const an_ifc_type_index &param_type : param_types) {
      an_ifc_heap_type
                heap_type;
      size_t    heap_part_offset = this->output_state->alloc_node(&heap_type);

      if (first) {
        /* Update the heap start offset. */
        first = FALSE;
        heap_start_offset = heap_part_offset;
      }  /* if */
      set_ifc_value(&heap_type, param_type);
    }  /* for */

    /* Create and return a type tuple type pointing to all the parameters for
       this type. */
    an_ifc_type_tuple
                param_tuple_type;
    result = this->output_state->alloc_type(&param_tuple_type);

    an_ifc_index
                ifc_type_heap_start(param_tuple_type.get_file(),
                                    heap_start_offset);
    set_ifc_start(&param_tuple_type, ifc_type_heap_start);

    an_ifc_cardinality ifc_param_type_count(
                            param_tuple_type.get_file(),
                            (an_ifc_cardinality_storage)param_types.length());
    set_ifc_cardinality(&param_tuple_type, ifc_param_type_count);
  }  /* if */
  return result;
}  /* an_ifc_il_map::enter_routine_params_type */


an_ifc_type_index an_ifc_il_map::enter_routine_type(a_type_ptr type)
/*
Enter the given routine type into the IFC output state.  Return the type index
for the function type.
*/
{
  check_assertion(type->kind == tk_routine);
  an_ifc_type_index
                result;
  a_routine_type_supplement_ptr
                rtsp = type->variant.routine.extra_info;
  a_routine_ptr rp = rtsp->assoc_routine;

  if (rp == NULL) {
    /* Sometimes there's no associated routine (e.g., this a type corresponding
       to a function pointer). */
    if (routine_type_is_nonstatic_member_function(type)) {
      result = this->enter_member_function_type(type);
    } else {
      result = this->enter_free_function_type(type);
    }  /* if */
  } else {
    switch (rp->special_kind) {
      case sfk_none:
        if (routine_type_is_nonstatic_member_function(type)) {
          result = this->enter_member_function_type(type);
        } else {
          result = this->enter_free_function_type(type);
        }  /* if */
        break;
      case sfk_constructor:
        result = this->enter_constructor_type(type);
        break;
      case sfk_conversion:
        /* FIXME: Implement these. */
        break;
      case sfk_destructor:
        /* Destructors do not have types in the IFC and should not appear
           here. */
        unexpected_condition();
      case sfk_operator:
        /* FIXME: Implement these. */
        break;
  #if BUILTIN_FUNCTIONS_ENABLED
      case sfk_builtin_operator_delete:
      case sfk_builtin_operator_new:
  #endif /* BUILTIN_FUNCTIONS_ENABLED */
      case sfk_deduction_guide:
  #if MICROSOFT_EXTENSIONS_ALLOWED
      case sfk_dispose_bool:
      case sfk_event_add:
      case sfk_event_raise:
      case sfk_event_remove:
      case sfk_finalizer:
  #endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  #if BUILTIN_FUNCTIONS_ENABLED
      case sfk_gnu_atomic_generic_function:
      case sfk_gnu_atomic_nongeneric_function:
      case sfk_gnu_sync_concrete_function:
  #endif /* BUILTIN_FUNCTIONS_ENABLED */
  #if MICROSOFT_EXTENSIONS_ALLOWED
      case sfk_idisposable_dispose:
  #endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      case sfk_lambda_entry_point:
  #if MICROSOFT_EXTENSIONS_ALLOWED
      case sfk_object_finalize:
      case sfk_property_get:
      case sfk_property_set:
      case sfk_static_constructor:
  #endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      case sfk_udl_operator:
        ifc_write_catastrophe();
        break;
      case sfk_last:
        /* sfk_last should not appear in the IL. */
        unexpected_condition();
      default_is_unexpected();
    }  /* switch */
  }  /* if */
  return result;
}  /* an_ifc_il_map::enter_routine_type */


an_ifc_type_index an_ifc_il_map::enter_template_type_param_type(
                                              a_type_ptr        type,
                                              an_ifc_decl_index param_decl_idx)
/*
Enter the IFC template type parameter corresponding to the given type
represented by the given IFC declaration index.  Return the type index for
the template type parameter.
*/
{
  an_ifc_type_designated
                designated_type;
  an_ifc_type_index
                result = this->map_new_type(type, &designated_type);

  set_ifc_decl(&designated_type, param_decl_idx);
  return result;
}  /* an_ifc_il_map::enter_template_type_param_type */


an_ifc_type_index an_ifc_il_map::enter_typedef_type(a_type_ptr type)
/*
Enter the given typedef type into the IFC output state.  Return the type index
for the typedef type.
*/
{
  check_assertion(type->kind == tk_typeref && typeref_is_typedef(type));
  an_ifc_type_designated
                designated_type;
  an_ifc_type_index
                result = this->map_new_type(type, &designated_type);
  an_ifc_decl_index
                typedef_decl = this->enter_typedef(type);

  set_ifc_decl(&designated_type, typedef_decl);
  return result;
}  /* an_ifc_il_map::enter_typedef_type */


an_ifc_type_index an_ifc_il_map::enter_void_type(a_type_ptr type)
/*
Enter the given void type into the IFC output state.  Return the type index
for the void type.
*/
{
  an_ifc_type_fundamental
                fund_type;
  an_ifc_type_index
                result = this->map_new_type(type, &fund_type);

  set_ifc_basis(&fund_type, ifc_tbs_void);
  return result;
}  /* an_ifc_il_map::enter_void_type */


an_ifc_output_token_cache an_ifc_il_map::copy_template_body_to_cache(
                                                          a_template_ptr templ)
/*
This is a utility function for creating an IFC output token cache representing
the body of the given template.  The created IFC output token cache is
returned.
*/
{
  an_ifc_output_token_cache
                result;
  a_symbol_ptr  templ_sym = symbol_for(templ);
  a_template_symbol_supplement_ptr
                tssp = templ_sym->variant.template_info;

  if (!tssp->cache->tokens.is_empty()) {
    this->enter_token_cache(&result, tssp->cache->tokens.ptr());
  }  /* if */
  return result;
}  /* an_ifc_il_map::copy_template_body_to_cache */


static a_constant_ptr enumerator_constants_for_type(a_type_ptr type)
/*
Return the list of constants representing the enumerators of the given
enumeration type.
*/
{
  check_assertion(type->kind == tk_enum && type->variant.integer.enum_type);
  a_constant_ptr result = NULL;

  if (type->variant.integer.is_scoped_enum) {
    /* Scoped enumerators use a scope with a constant list rather than
       storing the constants directly on the integer type.  Check for
       said scope and then read the constants from it if it exists. */
    a_scope_ptr scope = type->variant.integer.enum_info.assoc_scope;

    if (scope != NULL) {
      result = scope->constants;
    }  /* if */
  } else {
    result = type->variant.integer.enum_info.constant_list;
  }  /* if */
  return result;
}  /* enumerator_constants_for_type */


an_ifc_sequence an_ifc_il_map::enter_enumerators(a_type_ptr type)
/*
Given a enumeration type, enter the type's enumerators.  Return the IFC
sequence representing the IFC DeclSort::Enumerators.
*/
{
  check_assertion(type->kind == tk_enum && type->variant.integer.enum_type);
  an_ifc_sequence result(this->get_default_file());
  a_constant_ptr  constants = enumerator_constants_for_type(type);
  size_t          num_constants = count_list_elements(constants);

  if (num_constants > 0) {
    /* Preallocate all the enumerators to ensure we get one contiguous
       block. */
    size_t      start = this->output_state->
                       alloc_node_block<an_ifc_decl_enumerator>(num_constants);
    /* Associate the preallocated enumerator nodes with the sequence. */
    an_ifc_index
                ifc_start(this->get_default_file(), start);
    an_ifc_cardinality
                ifc_cardinality(this->get_default_file(), num_constants);

    set_ifc_start(&result, ifc_start);
    set_ifc_cardinality(&result, ifc_cardinality);

    /* Find or enter the enumeration type. */
    an_ifc_type_index
                enumeration_type = this->find_or_enter_type(type);
    /* Complete the enumerator declarations. */
    a_constant_ptr curr_constant = constants;
    for (size_t i = 0; i < num_constants; ++i) {
      size_t                 curr_enumerator_offset = start + i;
      an_ifc_decl_enumerator curr_enumerator;

      this->output_state->fetch_node(&curr_enumerator,
                                     curr_enumerator_offset);

      /* Set the enumerator name. */
      an_ifc_text_offset
                ifc_name_offset =
                               this->entity_name_as_text_offset(curr_constant);
      set_ifc_name(&curr_enumerator, ifc_name_offset);

      /* Set the source location information. */
      an_ifc_source_location
                ifc_src_pos = this->find_or_enter_entity_pos(curr_constant);
      set_ifc_locus(&curr_enumerator, ifc_src_pos);
      /* Associate the enumeration type with the enumerator. */
      set_ifc_type(&curr_enumerator, enumeration_type);

      /* Create a token cache representation of the initializing constant. */
      an_ifc_output_token_cache
                init_token_cache;
      /* FIXME: We probably eventually want to have a find_or_enter_constant
         to hash and deduplicate constants. */
      an_ifc_edg_constant_index
                ifc_constant = this->enter_constant(curr_constant);
      an_ifc_edg_complex_token_index
                ifc_constant_token = this->enter_constant_token(
                                                         ifc_ects_int_constant,
                                                         ifc_constant);
      init_token_cache.add_complex(ifc_constant_token);

      /* Set the initializing expression. */
      an_ifc_expr_index init_idx = this->output_state->alloc_token_cache_expr(
                                                             init_token_cache);
      set_ifc_initializer(&curr_enumerator, init_idx);
      /* FIXME: Set specifier. */

      /* Set the access specifier. */
      an_ifc_access_sort ifc_access = access_specifier_of(curr_constant);
      set_ifc_access(&curr_enumerator, ifc_access);

      /* Advance to the next parameter. */
      curr_constant = curr_constant->next;
    }  /* for */
  }  /* if */
  return result;
}  /* an_ifc_il_map::enter_enumerators */


an_ifc_chart_index an_ifc_il_map::enter_routine_params(a_routine_ptr rp)
/*
Given a routine, enter the routine's parameters.  Return the IFC chart index of
the parameters.
*/
{
  an_ifc_chart_index
                result;
  a_type_ptr    type = rp->type;
  a_param_type_ptr
                param_type_list = function_type_params(type);
  unsigned      param_count = count_list_elements(param_type_list);

  if (param_count > 0) {
    an_ifc_chart_unilevel
                param_chart;

    result = this->output_state->alloc_chart(&param_chart);

    /* Preallocate all the parameters to ensure we get one contiguous block. */
    size_t      start = this->output_state->
                          alloc_node_block<an_ifc_decl_parameter>(param_count);
    /* Associate the preallocated parameter nodes with the parameter chart. */
    an_ifc_module_file
                *file = this->get_default_file();
    an_ifc_index
                ifc_param_start(file, start);
    an_ifc_cardinality
                ifc_param_count(file, param_count);
    set_ifc_start(&param_chart, ifc_param_start);
    set_ifc_cardinality(&param_chart, ifc_param_count);

    /* Complete the parameter declarations. */
    a_param_type_ptr curr_param_type = param_type_list;
    for (size_t i = 0; i < param_count; ++i) {
      size_t                curr_param_offset = start + i;
      an_ifc_decl_parameter curr_param;

      this->output_state->fetch_node(&curr_param, curr_param_offset);

      /* Set the parameter name. */
      a_const_char
                *name = curr_param_type->name;
      if (name != NULL) {
        size_t  name_offset = this->output_state->add_to_string_table(name);
        an_ifc_text_offset
                ifc_name_offset(curr_param.get_file(), name_offset);

        set_ifc_name(&curr_param, ifc_name_offset);
      } else {
        an_ifc_text_offset
                ifc_name_offset;

        set_ifc_name(&curr_param, ifc_name_offset);
      }  /* if */

      /* Set the source location information. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
      a_decl_position_supplement_ptr
                decl_pos_sup = curr_param_type->decl_pos_info;
      if (decl_pos_sup != NULL) {
        a_source_position
                src_pos = decl_pos_sup->identifier_range.start;
        an_ifc_source_location
                ifc_src_pos = this->find_or_enter_pos(src_pos);

        set_ifc_locus(&curr_param, ifc_src_pos);
      } else
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      {
        an_ifc_source_location
                ifc_src_pos = this->find_or_enter_null_pos();

        set_ifc_locus(&curr_param, ifc_src_pos);
      }  /* if */

      /* Set the type information. */
      an_ifc_type_index
                ifc_type_idx = this->find_or_enter_type(curr_param_type->type);
      set_ifc_type(&curr_param, ifc_type_idx);
      /* FIXME: Set the constraint. */
      /* FIXME: Set the initializer. */
      /* FIXME: Set the level. */
      /* FIXME: Set the position. */
      set_ifc_sort(&curr_param, ifc_ps_object);
      /* FIXME: Set the reachable properties. */
      /* Advance to the next parameter. */
      curr_param_type = curr_param_type->next;
    }  /* for */
  }  /* if */
  return result;
}  /* an_ifc_il_map::enter_routine_params */


an_ifc_edg_template_argument_index an_ifc_il_map::enter_template_argument(
                                                  a_template_arg_ptr templ_arg)
/*
For the given template argument enter the template argument into the IFC output
state.  Return the EDG IFC template argument index for the template argument.
*/
{
  an_ifc_edg_template_argument_index
                result;
  a_templ_arg_kind
                arg_kind = templ_arg->kind;

  switch (arg_kind) {
    case tak_nontype:
      { an_ifc_edg_template_argument_non_type
                non_type_templ_arg;
        size_t  arg_offset =
                           this->output_state->alloc_node(&non_type_templ_arg);
        an_ifc_output_token_cache
                init_token_cache;

        if (templ_arg->is_array_bound_of_unknown_type) {
          ifc_write_catastrophe();
        } else {
          an_ifc_edg_constant_index
                ifc_constant =
                             this->enter_constant(templ_arg->variant.constant);
          an_ifc_edg_complex_token_index
                ifc_constant_token = this->enter_constant_token(
                                                         ifc_ects_int_constant,
                                                         ifc_constant);

          init_token_cache.add_complex(ifc_constant_token);
        }  /* if */

        /* Set the value for the argument. */
        an_ifc_expr_index
                init_idx = this->output_state->alloc_token_cache_expr(
                                                             init_token_cache);
        set_ifc_value(&non_type_templ_arg, init_idx);
        result = an_ifc_edg_template_argument_index(
                                       non_type_templ_arg.get_file(),
                                       ifc_etas_edg_template_argument_non_type,
                                       arg_offset);
      }
      break;
    case tak_template:
      { an_ifc_edg_template_argument_template
                template_templ_arg;
        size_t  arg_offset =
                           this->output_state->alloc_node(&template_templ_arg);

        if (templ_arg->variant.templ.substituted_param_template != NULL) {
          /* FIXME: What if anything needs done here? */
          ifc_write_catastrophe();
        }  /* if */

        an_ifc_decl_index
                decl_idx =
                    this->find_or_enter_template(templ_arg->variant.templ.ptr);
        set_ifc_value(&template_templ_arg, decl_idx);
        result = an_ifc_edg_template_argument_index(
                                       template_templ_arg.get_file(),
                                       ifc_etas_edg_template_argument_template,
                                       arg_offset);
      }
      break;
    case tak_type:
      { an_ifc_edg_template_argument_type
                template_templ_arg;
        size_t  arg_offset =
                           this->output_state->alloc_node(&template_templ_arg);
        an_ifc_type_index
                type_idx = this->find_or_enter_type(templ_arg->variant.type);

        set_ifc_value(&template_templ_arg, type_idx);
        result = an_ifc_edg_template_argument_index(
                                           template_templ_arg.get_file(),
                                           ifc_etas_edg_template_argument_type,
                                           arg_offset);
      }
      break;
    case tak_start_of_pack_expansion:
      ifc_write_catastrophe();
      break;
    default_is_unexpected();
  }  /* switch */
  return result;
}  /* an_ifc_il_map::enter_template_argument */


an_ifc_chart_index an_ifc_il_map::enter_template_params(a_template_ptr templ)
/*
Enter the chart representing the given template's template parameters.  Return
the index of the template parameter chart.
*/
{
  an_ifc_chart_index
                result;
  a_template_decl_ptr
                templ_decl = templ->template_decl;
  a_template_parameter_ptr
                param_list = templ_decl->param_list;
  unsigned      param_count = count_list_elements(param_list);

  if (param_count > 0) {
    an_ifc_chart_unilevel
                param_chart;

    result = this->output_state->alloc_chart(&param_chart);

    /* Preallocate all the parameters to ensure we get one contiguous block. */
    size_t      start = this->output_state->
                          alloc_node_block<an_ifc_decl_parameter>(param_count);
    /* Associate the preallocated parameter nodes with the parameter chart. */
    an_ifc_module_file
                *file = this->get_default_file();
    an_ifc_index
                ifc_param_start(file, start);
    an_ifc_cardinality
                ifc_param_count(file, param_count);
    set_ifc_start(&param_chart, ifc_param_start);
    set_ifc_cardinality(&param_chart, ifc_param_count);

    /* Complete the parameter declarations. */
    a_template_parameter_ptr curr_templ_param = param_list;
    for (size_t i = 0; i < param_count; ++i) {
      size_t                curr_param_offset = start + i;
      an_ifc_decl_parameter curr_param;

      this->output_state->fetch_node(&curr_param, curr_param_offset);

      an_ifc_decl_index
                curr_param_idx(curr_param.get_file(), ifc_ds_decl_parameter,
                               curr_param_offset);
      a_template_parameter_kind
                param_kind = curr_templ_param->kind;
      switch (param_kind) {
        case tpk_nontype:
          this->set_up_template_non_type_param(&curr_param, curr_templ_param);
          break;
        case tpk_template:
          this->set_up_template_template_param(&curr_param, curr_param_idx,
                                               curr_templ_param);
          break;
        case tpk_type:
          this->set_up_template_type_param(&curr_param, curr_param_idx,
                                           curr_templ_param);
          break;
        case tpk_error:
          /* An error template parameter kind should not make it to IFC
             writing. */
          unexpected_condition();
        default_is_unexpected();
      }  /* switch */
      /* Advance to the next parameter. */
      curr_templ_param = curr_templ_param->next;
    }  /* for */
  }  /* if */
  return result;
}  /* an_ifc_il_map::enter_template_params */


an_ifc_type_index an_ifc_il_map::enter_template_template_param_type(
                                                          a_template_ptr templ)
/*
Enter an IFC forall type representing the given template (representing a
template parameter).
*/
{
  an_ifc_type_forall
                forall_type;
  an_ifc_type_index
                result = this->output_state->alloc_type(&forall_type);
  an_ifc_chart_index
                param_chart = this->enter_template_params(templ);

  set_ifc_chart(&forall_type, param_chart);
  set_ifc_subject(&forall_type, this->find_or_enter_class_scope_type());
  return result;
}  /* an_ifc_il_map::enter_template_template_param_type */


void an_ifc_il_map::set_template_param_coordinates(
                                an_ifc_decl_parameter             *param_decl,
                                const a_template_param_coordinate &coordinates)
/*
Set the level (i.e., depth) and position information for the given IFC
parameter declaration based on the given front end template parameter
coordinate information.
*/
{
  /* Set the level information (i.e., depth). */
  an_ifc_parameter_level
                ifc_param_depth(this->get_default_file(), coordinates.depth);
  set_ifc_level(param_decl, ifc_param_depth);

  /* Set the position information. */
  an_ifc_parameter_position
                ifc_param_position(this->get_default_file(),
                                   coordinates.position);
  set_ifc_position(param_decl, ifc_param_position);
}  /* an_ifc_il_map::set_template_param_coordinates */


void an_ifc_il_map::set_up_template_non_type_param(
                                       an_ifc_decl_parameter    *param_decl,
                                       a_template_parameter_ptr templ_param)
/*
The given IFC parameter declaration node is freshly allocated.  Set its fields
to represent the given front end non-type template parameter.
*/
{
  check_assertion(templ_param->kind == tpk_nontype);
  /* Set the parameter name. */
  an_ifc_text_offset
                ifc_name_offset =
                            this->entity_name_as_text_offset(templ_param);
  set_ifc_name(param_decl, ifc_name_offset);

  /* Set the source location information. */
  an_ifc_source_location
                ifc_src_pos = this->find_or_enter_entity_pos(templ_param);
  set_ifc_locus(param_decl, ifc_src_pos);

  /* Set the type information. */
  a_constant_ptr
                param_constant = templ_param->variant.nontype.constant;
  /* Check some assumptions about the constant representation.  If these
     assertions are violated either the IFC writer has been fed bad data or
     the IFC writer needs to be updated to handle the new case. */
  check_assertion(param_constant->kind == ck_template_param);
  check_assertion(param_constant->variant.template_param.kind == tpck_param);
  a_type_ptr    param_type = param_constant->type;
  an_ifc_type_index
                ifc_param_type = this->find_or_enter_type(param_type);
  set_ifc_type(param_decl, ifc_param_type);

  /* Set the level (i.e., depth) and position information. */
  a_template_param_coordinate
                coordinates =
                    param_constant->variant.template_param.variant.coordinates;
  this->set_template_param_coordinates(param_decl, coordinates);
  /* FIXME: Set the constraint. */
  /* FIXME: Set the initializer. */
  /* Set the parameter sort. */
  set_ifc_sort(param_decl, ifc_ps_non_type);
  /* FIXME: Set the reachable properties. */
}  /* an_ifc_il_map::set_up_template_non_type_param */


void an_ifc_il_map::set_up_template_template_param(
                                       an_ifc_decl_parameter    *param_decl,
                                       an_ifc_decl_index        param_decl_idx,
                                       a_template_parameter_ptr templ_param)
/*
The given IFC parameter declaration node (at the given declaration index) is
freshly allocated.  Set its fields to represent the given front end template
template parameter.
*/
{
  check_assertion(templ_param->kind == tpk_template);
  /* Link the template representing this parameter to the IFC
     DeclSort::Parameter now.  Following the mapping, this ensures that any
     references to the template properly resolve to the correct IFC
     parameter declaration. */
  a_template_ptr   param_template = templ_param->variant.templ.class_template;
  a_tagged_pointer tagged_templ = make_tagged_ptr(param_template);

  this->il_entry_to_decl.map(tagged_templ, param_decl_idx);

  /* Set the parameter name. */
  an_ifc_text_offset
                ifc_name_offset =
                            this->entity_name_as_text_offset(templ_param);
  set_ifc_name(param_decl, ifc_name_offset);

  /* Set the source location information. */
  an_ifc_source_location
                ifc_src_pos = this->find_or_enter_entity_pos(templ_param);
  set_ifc_locus(param_decl, ifc_src_pos);

  /* Set the type information. */
  an_ifc_type_index
                type_idx =
                      this->enter_template_template_param_type(param_template);
  set_ifc_type(param_decl, type_idx);

  /* Set the level (i.e., depth) and position information. */
  this->set_template_param_coordinates(param_decl,
                                       param_template->coordinates);
  /* FIXME: Set the constraint. */
  /* FIXME: Set the initializer. */
  /* Set the parameter sort. */
  set_ifc_sort(param_decl, ifc_ps_template);
  /* FIXME: Set the reachable properties. */
}  /* an_ifc_il_map::set_up_template_template_param */


void an_ifc_il_map::set_up_template_type_param(
                                       an_ifc_decl_parameter    *param_decl,
                                       an_ifc_decl_index        param_decl_idx,
                                       a_template_parameter_ptr templ_param)
/*
The given IFC parameter declaration node (at the given declaration index) is
freshly allocated.  Set its fields to represent the given front end type
template parameter.
*/
{
  check_assertion(templ_param->kind == tpk_type);
  /* Link the type representing this parameter to the IFC DeclSort::Parameter
     now.  Following the mapping, this ensures that any references to the
     type properly resolve to the correct IFC parameter declaration. */
  a_type_ptr    param_type = templ_param->variant.type.ptr;
  (void)this->enter_template_type_param_type(param_type, param_decl_idx);

  /* Set the parameter name. */
  an_ifc_text_offset
                ifc_name_offset =
                            this->entity_name_as_text_offset(templ_param);
  set_ifc_name(param_decl, ifc_name_offset);

  /* Set the source location information. */
  an_ifc_source_location
                ifc_src_pos = this->find_or_enter_entity_pos(templ_param);
  set_ifc_locus(param_decl, ifc_src_pos);

  /* Set the type information. */
  /* FIXME: What does Microsoft put here for a type template parameter? */
  set_ifc_type(param_decl, an_ifc_type_index());

  /* Set the level (i.e., depth) and position information. */
  a_template_param_type_supplement_ptr
                tpts = param_type->variant.template_param.extra_info;
  this->set_template_param_coordinates(param_decl, tpts->coordinates);
  /* FIXME: Set the constraint. */
  /* FIXME: Set the initializer. */
  /* Set the parameter sort. */
  set_ifc_sort(param_decl, ifc_ps_type);
  /* FIXME: Set the reachable properties. */
}  /* an_ifc_il_map::set_up_template_type_param */


an_ifc_edg_constant_index an_ifc_il_map::enter_constant(const a_constant *cp)
/*
For the given constant enter the constant into the IFC output state.  Return
the expr index for the constant.
*/
{
  an_ifc_edg_constant_index
                result;

  switch (cp->kind) {
    case ck_error:
    case ck_last:
      ifc_write_catastrophe();
      break;
    case ck_integer:
      { const an_integer_value
                &int_val = cp->variant.integer_value;
        /* Convert the front end integer value into the necessary number of EDG
           IFC integer constant words.

           an_integer_value must be a multiple of 4 bytes; otherwise, the
           "constant words" will not be completely filled.  As an example:

             0x00 0x00 0x0f 0x00
             ^^^^ ^^^^ ^^^^

           may be written instead of:

             0x00 0x00 0x00 0x0f
             ^^^^ ^^^^ ^^^^ ^^^^
         */
        static_assert(sizeof(an_integer_value) % sizeof(uint32_t) == 0,
                      "an_integer_value must be a multiple of 4 bytes");
        constexpr unsigned
                num_parts_expected =
                                 (sizeof(an_integer_value) / sizeof(uint32_t));
        auto    output_int =
                      integer_value_as_translator<uint32_t,
                                                  num_parts_expected>(int_val);
        /* Allocate the integer constant word block and populate it with
           the computed values. */
        check_assertion(output_int.length() == num_parts_expected);
        size_t  start = this->output_state->
                            alloc_node_block<an_ifc_edg_constant_integer_word>(
                                                           num_parts_expected);
        for (size_t i = 0; i < num_parts_expected; ++i) {
          an_ifc_edg_constant_integer_word int_word;

          this->output_state->fetch_node(&int_word, start + i);

          an_ifc_edg_constant_word_storage word_bytes;
          memcpy(word_bytes, (void*)(&output_int[i]), /*num_bytes=*/4);

          an_ifc_edg_constant_word word_contents(int_word.get_file(),
                                                 word_bytes);
          set_ifc_bytes(&int_word, word_contents);
        }  /* for */

        /* Create the integer constant itself. */
        an_ifc_edg_constant_integer
                int_constant;
        result = this->output_state->alloc_constant(&int_constant);

        /* Associate the constant's type. */
        a_type_ptr
                constant_type = cp->type;
        if (is_enum_type(constant_type)) {
          /* The IFC reading code expects an integer constant with a
             fundamental integral type.  The constant's enumeration type will
             be restored automatically as part of the reconstruction
             process. */
          an_integer_kind
                underlying_int_kind = constant_type->variant.integer.int_kind;

          constant_type = integer_type(underlying_int_kind);
        }  /* if */

        an_ifc_type_index
                ifc_constant_type = this->find_or_enter_type(constant_type);
        set_ifc_type(&int_constant, ifc_constant_type);

        /* Reference the created words. */
        an_ifc_edg_constant_integer_word_offset
                ifc_start(int_constant.get_file(), start);
        an_ifc_cardinality
                ifc_cardinality(int_constant.get_file(), num_parts_expected);
        set_ifc_start(&int_constant, ifc_start);
        set_ifc_cardinality(&int_constant, ifc_cardinality);
      }
      break;
#if FIXED_POINT_ALLOWED
    case ck_fixed_point:
#endif /* FIXED_POINT_ALLOWED */
#if DO_IL_LOWERING && GENERATE_EH_TABLES && !DO_FULL_PORTABLE_EH_LOWERING
    case ck_stack_offset:
#endif /* DO_IL_LOWERING && ... */
    case ck_string:
    case ck_float:
#if C99_IL_EXTENSIONS_SUPPORTED
    case ck_complex:
    case ck_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case ck_address:
    case ck_ptr_to_member:
#if GNU_EXTENSIONS_ALLOWED
    case ck_label_difference:
#endif /* GNU_EXTENSIONS_ALLOWED */
    case ck_dynamic_init:
    case ck_aggregate:
    case ck_init_repeat:
    case ck_template_param:
    case ck_designator:
#if UPC_EXTENSIONS_ALLOWED
    case ck_upc_threads:
    case ck_upc_mythread:
#endif /* UPC_EXTENSIONS_ALLOWED */
    case ck_void:
    case ck_reflection:
      ifc_write_catastrophe();
      break;
    default_is_unexpected();
  } /* switch */
  return result;
}  /* an_ifc_il_map::enter_constant */

}  /* namespace */

static inline an_ifc_edg_basic_token_sort token_to_basic_token_kind(
                                                            a_token_kind token)
/*
Given a front end token kind (for a token where extra_info_kind == teik_none)
return the corresponding EDG IFC BasicToken sort value.  For tokens that should
be handled textually, add a case to ifc_ebts_complex.  See the documentation of
a_token_kind for more information about IFC token serialization.
*/
{
  an_ifc_edg_basic_token_sort result = ifc_ebts_semicolon;

  switch (token) {
    case tok_alignas:
      result = ifc_ebts_alignas;
      break;
    case tok_alignof:
      result = ifc_ebts_alignof;
      break;
    case tok_ampersand:
      result = ifc_ebts_ampersand;
      break;
    case tok_and_and:
      result = ifc_ebts_and_and;
      break;
    case tok_and_assign:
      result = ifc_ebts_and_assign;
      break;
    case tok_arrow:
      result = ifc_ebts_arrow;
      break;
    case tok_arrow_star:
      result = ifc_ebts_arrow_star;
      break;
    case tok_asm:
      result = ifc_ebts_asm;
      break;
    case tok_assign:
      result = ifc_ebts_assign;
      break;
    case tok_auto:
      result = ifc_ebts_auto;
      break;
    case tok_bool:
      result = ifc_ebts_bool;
      break;
    case tok_break:
      result = ifc_ebts_break;
      break;
    case tok_c11_atomic:
      result = ifc_ebts_c11_atomic;
      break;
    case tok_c11_generic:
      result = ifc_ebts_c11_generic;
      break;
    case tok_c11_thread_local:
      result = ifc_ebts_c11_thread_local;
      break;
    case tok_c99_bool:
      result = ifc_ebts_c99_bool;
      break;
    case tok_c99_complex:
      result = ifc_ebts_c99_complex;
      break;
    case tok_c99_generic:
      result = ifc_ebts_c99_generic;
      break;
    case tok_c99_genericfx:
      result = ifc_ebts_c99_genericfx;
      break;
    case tok_c99_imaginary:
      result = ifc_ebts_c99_imaginary;
      break;
    case tok_case:
      result = ifc_ebts_case;
      break;
    case tok_catch:
      result = ifc_ebts_catch;
      break;
    case tok_char:
      result = ifc_ebts_char;
      break;
    case tok_char16_t:
      result = ifc_ebts_char16_t;
      break;
    case tok_char32_t:
      result = ifc_ebts_char32_t;
      break;
    case tok_char8_t:
      result = ifc_ebts_char8_t;
      break;
    case tok_class:
      result = ifc_ebts_class;
      break;
    case tok_colon:
      result = ifc_ebts_colon;
      break;
    case tok_colon_colon:
      result = ifc_ebts_colon_colon;
      break;
    case tok_comma:
      result = ifc_ebts_comma;
      break;
    case tok_compl:
      result = ifc_ebts_compl;
      break;
    case tok_concept:
      result = ifc_ebts_concept;
      break;
    case tok_const:
      result = ifc_ebts_const;
      break;
    case tok_const_cast:
      result = ifc_ebts_const_cast;
      break;
    case tok_consteval:
      result = ifc_ebts_consteval;
      break;
    case tok_constexpr:
      result = ifc_ebts_constexpr;
      break;
    case tok_constinit:
      result = ifc_ebts_constinit;
      break;
    case tok_continue:
      result = ifc_ebts_continue;
      break;
    case tok_coroutine_await:
      result = ifc_ebts_coroutine_await;
      break;
    case tok_coroutine_return:
      result = ifc_ebts_coroutine_return;
      break;
    case tok_coroutine_yield:
      result = ifc_ebts_coroutine_yield;
      break;
    case tok_cpp98_export:
      result = ifc_ebts_cpp98_export;
      break;
    case tok_decltype:
      result = ifc_ebts_decltype;
      break;
    case tok_default:
      result = ifc_ebts_default;
      break;
    case tok_delete:
      result = ifc_ebts_delete;
      break;
    case tok_divide:
      result = ifc_ebts_divide;
      break;
    case tok_divide_assign:
      result = ifc_ebts_divide_assign;
      break;
    case tok_do:
      result = ifc_ebts_do;
      break;
    case tok_double:
      result = ifc_ebts_double;
      break;
    case tok_dynamic_cast:
      result = ifc_ebts_dynamic_cast;
      break;
    case tok_ellipsis:
      result = ifc_ebts_ellipsis;
      break;
    case tok_else:
      result = ifc_ebts_else;
      break;
    case tok_enum:
      result = ifc_ebts_enum;
      break;
    case tok_eq:
      result = ifc_ebts_eq;
      break;
    case tok_excl_or:
      result = ifc_ebts_excl_or;
      break;
    case tok_excl_or_assign:
      result = ifc_ebts_excl_or_assign;
      break;
    case tok_explicit:
      result = ifc_ebts_explicit;
      break;
    case tok_export:
      result = ifc_ebts_export;
      break;
    case tok_export_keyword:
      result = ifc_ebts_export_keyword;
      break;
    case tok_extern:
      result = ifc_ebts_extern;
      break;
    case tok_false:
      result = ifc_ebts_false;
      break;
    case tok_final:
      result = ifc_ebts_final;
      break;
    case tok_float:
      result = ifc_ebts_float;
      break;
    case tok_float128:
      result = ifc_ebts_float128;
      break;
    case tok_float32:
      result = ifc_ebts_float32;
      break;
    case tok_float32x:
      result = ifc_ebts_float32x;
      break;
    case tok_float64:
      result = ifc_ebts_float64;
      break;
    case tok_float64x:
      result = ifc_ebts_float64x;
      break;
    case tok_for:
      result = ifc_ebts_for;
      break;
    case tok_friend:
      result = ifc_ebts_friend;
      break;
    case tok_ge:
      result = ifc_ebts_ge;
      break;
    case tok_goto:
      result = ifc_ebts_goto;
      break;
    case tok_gt:
      result = ifc_ebts_gt;
      break;
    case tok_if:
      result = ifc_ebts_if;
      break;
    case tok_import:
      result = ifc_ebts_import;
      break;
    case tok_infinity:
      result = ifc_ebts_infinity;
      break;
    case tok_inline:
      result = ifc_ebts_inline;
      break;
    case tok_int:
      result = ifc_ebts_int;
      break;
    case tok_lbrace:
      result = ifc_ebts_lbrace;
      break;
    case tok_lbracket:
      result = ifc_ebts_lbracket;
      break;
    case tok_le:
      result = ifc_ebts_le;
      break;
    case tok_long:
      result = ifc_ebts_long;
      break;
    case tok_lparen:
      result = ifc_ebts_lparen;
      break;
    case tok_lsplice:
      result = ifc_ebts_lsplice;
      break;
    case tok_lt:
      result = ifc_ebts_lt;
      break;
    case tok_minus:
      result = ifc_ebts_minus;
      break;
    case tok_minus_assign:
      result = ifc_ebts_minus_assign;
      break;
    case tok_minus_minus:
      result = ifc_ebts_minus_minus;
      break;
    case tok_module:
      result = ifc_ebts_module;
      break;
    case tok_mutable:
      result = ifc_ebts_mutable;
      break;
    case tok_namespace:
      result = ifc_ebts_namespace;
      break;
    case tok_nan:
      result = ifc_ebts_nan;
      break;
    case tok_ne:
      result = ifc_ebts_ne;
      break;
    case tok_new:
      result = ifc_ebts_new;
      break;
    case tok_noexcept:
      result = ifc_ebts_noexcept;
      break;
    case tok_noreturn:
      result = ifc_ebts_noreturn;
      break;
    case tok_not:
      result = ifc_ebts_not;
      break;
    case tok_nullptr:
      result = ifc_ebts_nullptr;
      break;
    case tok_operator:
      result = ifc_ebts_operator;
      break;
    case tok_or:
      result = ifc_ebts_or;
      break;
    case tok_or_assign:
      result = ifc_ebts_or_assign;
      break;
    case tok_or_or:
      result = ifc_ebts_or_or;
      break;
    case tok_overload:
      result = ifc_ebts_overload;
      break;
    case tok_override:
      result = ifc_ebts_override;
      break;
    case tok_paste:
      result = ifc_ebts_paste;
      break;
    case tok_period:
      result = ifc_ebts_period;
      break;
    case tok_period_star:
      result = ifc_ebts_period_star;
      break;
    case tok_plus:
      result = ifc_ebts_plus;
      break;
    case tok_plus_assign:
      result = ifc_ebts_plus_assign;
      break;
    case tok_plus_plus:
      result = ifc_ebts_plus_plus;
      break;
    case tok_private:
      result = ifc_ebts_private;
      break;
    case tok_protected:
      result = ifc_ebts_protected;
      break;
    case tok_public:
      result = ifc_ebts_public;
      break;
    case tok_quest_mark:
      result = ifc_ebts_quest_mark;
      break;
    case tok_rbrace:
      result = ifc_ebts_rbrace;
      break;
    case tok_rbracket:
      result = ifc_ebts_rbracket;
      break;
    case tok_register:
      result = ifc_ebts_register;
      break;
    case tok_reinterpret_cast:
      result = ifc_ebts_reinterpret_cast;
      break;
    case tok_remainder:
      result = ifc_ebts_remainder;
      break;
    case tok_remainder_assign:
      result = ifc_ebts_remainder_assign;
      break;
    case tok_remove_all_extents:
      result = ifc_ebts_remove_all_extents;
      break;
    case tok_requires:
      result = ifc_ebts_requires;
      break;
    case tok_return:
      result = ifc_ebts_return;
      break;
    case tok_rparen:
      result = ifc_ebts_rparen;
      break;
    case tok_rsplice:
      result = ifc_ebts_rsplice;
      break;
    case tok_semicolon:
      result = ifc_ebts_semicolon;
      break;
    case tok_sharp:
      result = ifc_ebts_sharp;
      break;
    case tok_shift_left:
      result = ifc_ebts_shift_left;
      break;
    case tok_shift_left_assign:
      result = ifc_ebts_shift_left_assign;
      break;
    case tok_shift_right:
      result = ifc_ebts_shift_right;
      break;
    case tok_shift_right_assign:
      result = ifc_ebts_shift_right_assign;
      break;
    case tok_short:
      result = ifc_ebts_short;
      break;
    case tok_signed:
      result = ifc_ebts_signed;
      break;
    case tok_sizeof:
      result = ifc_ebts_sizeof;
      break;
    case tok_spaceship:
      result = ifc_ebts_spaceship;
      break;
    case tok_star:
      result = ifc_ebts_star;
      break;
    case tok_static:
      result = ifc_ebts_static;
      break;
    case tok_static_assert:
      result = ifc_ebts_static_assert;
      break;
    case tok_static_cast:
      result = ifc_ebts_static_cast;
      break;
    case tok_struct:
      result = ifc_ebts_struct;
      break;
    case tok_switch:
      result = ifc_ebts_switch;
      break;
    case tok_template:
      result = ifc_ebts_template;
      break;
    case tok_this:
      result = ifc_ebts_this;
      break;
    case tok_thread_local:
      result = ifc_ebts_thread_local;
      break;
    case tok_throw:
      result = ifc_ebts_throw;
      break;
    case tok_times_assign:
      result = ifc_ebts_times_assign;
      break;
    case tok_true:
      result = ifc_ebts_true;
      break;
    case tok_try:
      result = ifc_ebts_try;
      break;
    case tok_typedef:
      result = ifc_ebts_typedef;
      break;
    case tok_typeid:
      result = ifc_ebts_typeid;
      break;
    case tok_typename:
      result = ifc_ebts_typename;
      break;
    case tok_union:
      result = ifc_ebts_union;
      break;
    case tok_unsigned:
      result = ifc_ebts_unsigned;
      break;
    case tok_using:
      result = ifc_ebts_using;
      break;
    case tok_virtual:
      result = ifc_ebts_virtual;
      break;
    case tok_void:
      result = ifc_ebts_void;
      break;
    case tok_volatile:
      result = ifc_ebts_volatile;
      break;
    case tok_wchar_t:
      result = ifc_ebts_wchar_t;
      break;
    case tok_while:
      result = ifc_ebts_while;
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_abstract:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_accum:
    case tok_add_lvalue_reference:
    case tok_add_pointer:
    case tok_add_rvalue_reference:
    case tok_array_extent:
    case tok_array_rank:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_assume:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_attribute:
    case tok_auto_type:
    case tok_backslash:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_based:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
    case tok_bases:
#endif /* GNU_EXTENSIONS_ALLOWED */
    case tok_builtin_addressof:
    case tok_builtin_bit_cast:
    case tok_builtin_complex:
#if GNU_VECTOR_TYPES_ALLOWED
    case tok_builtin_convertvector:
#endif /* GNU_VECTOR_TYPES_ALLOWED */
    case tok_builtin_has_attribute:
    case tok_builtin_is_corresponding_member:
    case tok_builtin_is_pointer_interconvertible_with_class:
    case tok_builtin_is_virtual_base_of:
    case tok_builtin_is_implicit_lifetime:
    case tok_builtin_lt_synthesizes_from_spaceship:
    case tok_builtin_gt_synthesizes_from_spaceship:
    case tok_builtin_le_synthesizes_from_spaceship:
    case tok_builtin_ge_synthesizes_from_spaceship:
    case tok_builtin_is_structural:
    case tok_builtin_offsetof:
#if GNU_VECTOR_TYPES_ALLOWED
    case tok_builtin_shuffle:
    case tok_builtin_shufflevector:
#endif /* GNU_VECTOR_TYPES_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
    case tok_builtin_types_compatible:
#endif /* GNU_EXTENSIONS_ALLOWED */
    case tok_caret_caret:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_cdecl:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_charize:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_clang_version:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_clrcall:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_decay:
    case tok_decorated_function_name:
#if GNU_EXTENSIONS_ALLOWED
    case tok_direct_bases:
#endif /* GNU_EXTENSIONS_ALLOWED */
    case tok_edg_bool_type:
    case tok_edg_internal_opnd:
    case tok_edg_internal_type:
    case tok_edg_is_deducible:
    case tok_edg_ptrdiff_type:
    case tok_edg_size_type:
    case tok_edg_throw:
    case tok_edg_vector_type:
    case tok_edg_wchar_type:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_end_of_if_exists:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_enum_class:
    case tok_enum_struct:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_event:
    case tok_except:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_ext_alignof:
    case tok_extension:
#if NEAR_AND_FAR_ALLOWED
    case tok_far:
#endif /* NEAR_AND_FAR_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_fastcall:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_finally:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_for_each:
    case tok_forceinline:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_fract:
    case tok_func_name:
    case tok_function_name:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_gcnew:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if SUN_EXTENSIONS_ALLOWED
    case tok_global_link_scope:
#endif /* SUN_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
    case tok_gnu_imag:
#endif /* GNU_EXTENSIONS_ALLOWED */
    case tok_gnu_max:
    case tok_gnu_min:
#if GNU_EXTENSIONS_ALLOWED
    case tok_gnu_real:
#endif /* GNU_EXTENSIONS_ALLOWED */
    case tok_gnu_restrict:
    case tok_has_assign:
    case tok_has_copy:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_has_finalizer:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_has_nothrow_assign:
    case tok_has_nothrow_constructor:
    case tok_has_nothrow_copy:
    case tok_has_nothrow_move_assign:
    case tok_has_trivial_assign:
    case tok_has_trivial_constructor:
    case tok_has_trivial_copy:
    case tok_has_trivial_destructor:
    case tok_has_trivial_move_assign:
    case tok_has_trivial_move_constructor:
    case tok_has_unique_object_representations:
    case tok_has_user_destructor:
    case tok_has_virtual_destructor:
#if SUN_EXTENSIONS_ALLOWED
    case tok_hidden_link_scope:
#endif /* SUN_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_if_exists:
    case tok_if_not_exists:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_imaginary_unit:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_implements:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_in:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if INT128_EXTENSIONS_ALLOWED
    case tok_int128:
#endif /* INT128_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_int16:
    case tok_int32:
    case tok_int64:
    case tok_int8:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_intaddr:
    case tok_integer_pack:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_interface:
    case tok_interface_class:
    case tok_interface_struct:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_internal_alias_decl:
    case tok_is_abstract:
    case tok_is_aggregate:
    case tok_is_arithmetic:
    case tok_is_array:
    case tok_is_assignable:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_is_assignable_no_precondition_check:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_is_base_of:
    case tok_is_bounded_array:
    case tok_is_class:
    case tok_is_complete_type:
    case tok_is_compound:
    case tok_is_const:
    case tok_is_constructible:
    case tok_is_convertible:
    case tok_is_convertible_to:
    case tok_is_corresponding_member:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_is_delegate:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_is_destructible:
    case tok_is_empty:
    case tok_is_enum:
    case tok_is_final:
    case tok_is_floating_point:
    case tok_is_function:
    case tok_is_fundamental:
    case tok_is_invocable:
    case tok_is_integral:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_is_interface_class:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_is_layout_compatible:
    case tok_is_literal_type:
    case tok_is_lvalue_reference:
    case tok_is_member_function_pointer:
    case tok_is_member_object_pointer:
    case tok_is_member_pointer:
    case tok_is_nothrow_assignable:
    case tok_is_nothrow_constructible:
    case tok_is_nothrow_convertible:
    case tok_is_nothrow_destructible:
    case tok_is_nothrow_invocable:
    case tok_is_object:
    case tok_is_pod:
    case tok_is_pointer:
    case tok_is_pointer_interconvertible_base_of:
    case tok_is_pointer_interconvertible_with_class:
    case tok_is_polymorphic:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_is_ref_array:
    case tok_is_ref_class:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_is_reference:
    case tok_is_referenceable:
    case tok_is_rvalue_reference:
    case tok_is_same:
    case tok_is_same_as:
    case tok_is_scalar:
    case tok_is_scoped_enum:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_is_sealed:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_is_signed:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_is_simple_value_class:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_is_standard_layout:
    case tok_is_trivial:
    case tok_is_trivially_assignable:
    case tok_is_trivially_constructible:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_is_trivially_copy_assignable:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_is_trivially_copyable:
    case tok_is_trivially_destructible:
    case tok_is_trivially_equality_comparable:
    case tok_is_trivially_relocatable:
    case tok_is_bitwise_cloneable:
    case tok_is_unbounded_array:
    case tok_is_union:
    case tok_is_unsigned:
    case tok_is_valid_winrt_type:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_is_value_class:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_is_void:
    case tok_is_volatile:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_is_win_class:
    case tok_is_win_interface:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_leave:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_make_signed:
    case tok_make_unsigned:
    case tok_microsoft_asm:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_microsoft_identifier:
    case tok_microsoft_inline:
    case tok_microsoft_lprefix:
    case tok_microsoft_ptr32:
    case tok_microsoft_ptr64:
    case tok_microsoft_sptr:
    case tok_microsoft_try:
    case tok_microsoft_uprefix:
    case tok_microsoft_uptr:
    case tok_microsoft_w64:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_native_nullptr:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NEAR_AND_FAR_ALLOWED
    case tok_near:
#endif /* NEAR_AND_FAR_ALLOWED */
    case tok_nonnull:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_noop:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_null:
    case tok_null_unspecified:
    case tok_nullable:
    case tok_nullptr_t:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_partial_ref_class:
    case tok_partial_ref_struct:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_prefix_enum:
    case tok_prefix_for:
    case tok_prefix_interface:
    case tok_prefix_partial:
    case tok_prefix_ref:
    case tok_prefix_value:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_pretty_function_name:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_ref_class:
    case tok_ref_new:
    case tok_ref_struct:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_reference_binds_to_temporary:
    case tok_reference_constructs_from_temporary:
    case tok_reference_converts_from_temporary:
    case tok_remove_const:
    case tok_remove_cv:
    case tok_remove_cvref:
    case tok_remove_extent:
    case tok_remove_pointer:
    case tok_remove_reference:
    case tok_remove_reference_t:
    case tok_remove_restrict:
    case tok_remove_volatile:
    case tok_restrict:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_safe_cast:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_sat:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_sealed:
    case tok_stdcall:
    case tok_super:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if SUN_EXTENSIONS_ALLOWED
    case tok_symbolic_link_scope:
#endif /* SUN_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_thiscall:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_thread:
    case tok_typeof:
    case tok_typeof_unqual:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_unaligned:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_underlying_type:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_unresolved_type:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if UPC_EXTENSIONS_ALLOWED
    case tok_upc_barrier:
    case tok_upc_blocksizeof:
    case tok_upc_elemsizeof:
    case tok_upc_fence:
    case tok_upc_forall:
    case tok_upc_localsizeof:
    case tok_upc_mythread:
    case tok_upc_notify:
    case tok_upc_relaxed:
    case tok_upc_shared:
    case tok_upc_strict:
    case tok_upc_threads:
    case tok_upc_wait:
#endif /* UPC_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_uuid:
    case tok_uuidof:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_va_copy:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_value_class:
    case tok_value_struct:
    case tok_vectorcall:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_bit_precise_int:
      result = ifc_ebts_complex;
      break;
    case tok_gen_constant:
    case tok_char_constant:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_cli_typeid:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_cpp_quote:
    case tok_datasizeof:
    case tok_declspec:
    case tok_decltype_construct:
    case tok_digit_sequence:
    case tok_edg_neon_polyvector_type:
    case tok_edg_neon_vector_type:
    case tok_edg_scalable_vector_type:
    case tok_end_of_source:
    case tok_error:
    case tok_fixed_point_constant:
    case tok_float_constant:
    case tok_header_name:
    case tok_identifier:
    case tok_ifc_decl:
    case tok_ifc_decl_ref:
    case tok_ifc_entity_ref:
    case tok_ifc_param_ref:
    case tok_ifc_type_ref:
    case tok_int_constant:
    case tok_last:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_microsoft_Lprefix:
    case tok_microsoft_Uprefix:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_newline:
    case tok_pending_ifc_expr:
    case tok_pp_number:
    case tok_ptr_to_member:
    case tok_removed_expr:
    case tok_removed_template_body:
    case tok_string_literal:
    case tok_ud_literal:
    case tok_unimplemented:
    case tok_unresolved_ud_literal:
    case tok_va_arg:
    case tok_va_end:
    case tok_va_start:
      /* An attempt was made to convert an unsupported or complex token to an
         EDG IFC basic token kind.  Either the token needs a case added above
         or the wrong conversion function has been called.  See the
         documentation of a_token_kind for more information. */
      unexpected_condition();
      break;
    default_is_unexpected();
  }  /* switch */
  return result;
}  /* token_to_basic_token_kind */


static inline an_ifc_edg_constant_token_sort token_to_constant_token_kind(
                                                            a_token_kind token)
/*
Given a token kind that has an associated constant, return the corresponding
EDG IFC constant token kind.
*/
{
  an_ifc_edg_constant_token_sort result;

  switch (token) {
    case tok_char_constant:
      result = ifc_ects_char_constant;
      break;
    case tok_float_constant:
      result = ifc_ects_float_constant;
      break;
    case tok_gen_constant:
      result = ifc_ects_gen_constant;
      break;
    case tok_int_constant:
      result = ifc_ects_int_constant;
      break;
    case tok_string_literal:
      result = ifc_ects_string_literal;
      break;
    default:
      /* An unsupported token was cached as a basic token.  Either it needs to
         be added above as a basic token, the token needs its own complex token
         add method, or the caller called the wrong add function (i.e., there's
         a corresponding complex token add function that should have instead
         been called). */
      ifc_write_catastrophe();
  }  /* switch */
  return result;
}  /* token_to_constant_token_kind */

namespace {

an_ifc_edg_complex_token_index an_ifc_il_map::find_or_enter_textual_token(
                                                     const a_shared_token &tok)
/*
For the given cached token (representing a token with a singular textual
identity) find or enter the textual token representation into the IFC output
state.  Return the corresponding IFC EDG complex token index.
*/
{
  an_ifc_edg_complex_token_index
                result;
  size_t        textual_token_offset =
                                  this->textual_token_map.get(tok->get_kind());

  if (textual_token_offset == 0) {
    result = this->enter_textual_token(tok);
  } else {
    result = an_ifc_edg_complex_token_index(this->get_default_file(),
                                            ifc_ects_edg_token_textual,
                                            textual_token_offset - 1);
  }  /* if */
  return result;
}  /* an_ifc_il_map::find_or_enter_textual_token */


void an_ifc_il_map::enter_basic_token_to_cache(
                                         an_ifc_output_token_cache *ifc_cache,
                                         const a_shared_token      &tok)
/*
Add the given cached token (representing a token representable as either a
basic token or a textual token) to the given token cache.
*/
{
  an_ifc_edg_basic_token_sort
                ifc_token = token_to_basic_token_kind(tok->get_kind());

  if (ifc_token == ifc_ebts_complex) {
    this->enter_textual_token_to_cache(ifc_cache, tok);
  } else {
    ifc_cache->add_basic(ifc_token);
  }  /* if */
}  /* an_ifc_il_map::enter_basic_token_to_cache */


an_ifc_edg_complex_token_index an_ifc_il_map::enter_textual_token(
                                                     const a_shared_token &tok)
/*
For the given cached token (representing a token with a singular textual
identity) enter the textual token representation into the IFC output state.
Return the corresponding IFC EDG complex token index.
*/
{
  /* If this assertion fails, this token should not be represented as a textual
     token because it's directly representable as a basic token. */
  check_assertion(token_to_basic_token_kind(tok->get_kind()) ==
                  ifc_ebts_complex);
  /* If this assertion fails, this token should not be represented as a textual
     token because it has additional state that needs to be serialized. */
  check_assertion(tok->is_basic());
  /* If this assertion fails, this textual token has already been entered. */
  check_assertion(this->textual_token_map.get(tok->get_kind()) == 0);
  an_ifc_edg_token_textual
                textual_token;
  an_ifc_edg_complex_token_index
                result = this->output_state->alloc_complex_token(
                                                               &textual_token);

  this->textual_token_map.map(tok->get_kind(), result.value + 1);

  size_t        token_name_offset = this->output_state->add_to_string_table(
                                           ifc_token_name_of(tok->get_kind()));
  an_ifc_text_offset
                ifc_token_name_offset(this->get_default_file(),
                                      token_name_offset);
  set_ifc_characters(&textual_token, ifc_token_name_offset);
  return result;
}  /* an_ifc_il_map::enter_textual_token */


void an_ifc_il_map::enter_textual_token_to_cache(
                                          an_ifc_output_token_cache *ifc_cache,
                                          const a_shared_token      &tok)
/*
Add the given cached token (representing a token with a singular textual
identity) to the given token cache.
*/
{
  check_assertion(tok->is_basic());
  an_ifc_edg_complex_token_index
                complex_token_idx = this->find_or_enter_textual_token(tok);

  ifc_cache->add_complex(complex_token_idx);
}  /* an_ifc_il_map::enter_textual_token_to_cache */


an_ifc_edg_complex_token_index an_ifc_il_map::enter_constant_token(
                                   an_ifc_edg_constant_token_sort token_kind,
                                   an_ifc_edg_constant_index      constant_idx)

/*
For the given constant and constant token kind, enter a complex constant token
into the IFC output state.  Return the index of the token.
*/
{
  an_ifc_edg_token_constant
                constant_token;
  an_ifc_edg_complex_token_index
                result = this->output_state->alloc_complex_token(
                                                              &constant_token);

  set_ifc_kind(&constant_token, token_kind);
  set_ifc_constant(&constant_token, constant_idx);
  return result;
}  /* an_ifc_il_map::enter_constant_token */


void an_ifc_il_map::enter_constant_token_to_cache(
                                          an_ifc_output_token_cache *ifc_cache,
                                          const a_shared_token      &tok)
/*
Add the given cached token (representing an EDG IFC constant token) to the
given token cache.
*/
{
  const a_constant
                *constant = tok->get_constant();
  an_ifc_edg_constant_index
                ifc_constant = this->enter_constant(constant);
  an_ifc_edg_constant_token_sort
                ifc_token_kind = token_to_constant_token_kind(tok->get_kind());
  an_ifc_edg_complex_token_index
                complex_token_idx = this->enter_constant_token(ifc_token_kind,
                                                               ifc_constant);

  ifc_cache->add_complex(complex_token_idx);
}  /* an_ifc_il_map::enter_constant_token_to_cache */


void an_ifc_il_map::enter_extracted_body_to_cache(
                                          an_ifc_output_token_cache *ifc_cache,
                                          const a_shared_token      &tok)
/*
Enter the token cache associated with the given extracted template token into
the given token cache.
*/
{
  const an_extracted_template_descr
                *extracted_template = tok->get_extracted_template_descr();
  a_symbol_ptr  template_sym = extracted_template->symbol;

  switch (template_sym->kind) {
    case sk_member_function:
      { a_template_instance_ptr
                instance_ptr = template_sym->variant.routine.instance_ptr;
        a_template_symbol_supplement_ptr
                tssp = instance_ptr->template_info;

        this->enter_token_cache(ifc_cache, tssp->cache->tokens.ptr());
      }
      break;
    default:
      ifc_write_catastrophe();
      break;
  }  /* switch */
}  /* an_ifc_il_map::enter_extracted_body_to_cache */


void an_ifc_il_map::enter_identifier_token_to_cache(
                                          an_ifc_output_token_cache *ifc_cache,
                                          const a_shared_token      &tok)
/*
Add the given cached token (representing an EDG IFC identifier token) to the
given token cache.
*/
{
  an_ifc_edg_token_identifier
                identifier_token;
  an_ifc_edg_complex_token_index
                result = this->output_state->alloc_complex_token(
                                                            &identifier_token);
  a_symbol_header_ptr
                sym_header = tok->get_locator().symbol_header;
  an_ifc_text_offset
                ifc_identifier = this->string_as_text_offset(
                                                sym_header->identifier,
                                                sym_header->identifier_length);

  set_ifc_text(&identifier_token, ifc_identifier);
  ifc_cache->add_complex(result);
}  /* an_ifc_il_map::enter_identifier_token_to_cache */


static a_boolean should_token_be_simplified(const a_shared_token &tok)
/*
Return TRUE if the given token should be simplified to just its corresponding
basic token kind.  This occurs for some token kinds (e.g., tok_true) where the
front end has an associated constant value but it should not be used.
Otherwise, return FALSE.
*/
{
  a_boolean result = FALSE;

  switch (tok->get_kind()) {
    case tok_true:
    case tok_false:
      result = TRUE;
      break;
    default:
      break;
  }  /* switch */
  /* Textual tokens cannot currently be simplified tokens. */
  check_assertion(!result ||
               token_to_basic_token_kind(tok->get_kind()) != ifc_ebts_complex);
  return result;
}  /* should_token_be_simplified */


void an_ifc_il_map::enter_token_cache(an_ifc_output_token_cache *ifc_cache,
                                      a_token_cache             *fe_cache)
/*
Add the tokens in the given front end token cache to the given EDG IFC token
cache.
*/
{
  a_token_cache_iterator it = fe_cache->begin();
  a_token_cache_iterator it_end = fe_cache->end();

  for (; it != it_end; ++it) {
    const a_shared_token &tok = *it;

    if (should_token_be_simplified(tok)) {
      this->enter_basic_token_to_cache(ifc_cache, tok);
      continue;
    }  /* if */

    switch (tok->get_extra_kind()) {
      case teik_constant:
        this->enter_constant_token_to_cache(ifc_cache, tok);
        break;
      case teik_extracted_body:
        this->enter_extracted_body_to_cache(ifc_cache, tok);
        break;
      case teik_identifier:
        this->enter_identifier_token_to_cache(ifc_cache, tok);
        break;
      case teik_none:
        { if (tok->is(tok_end_of_source)) {
            /* An end of source token should never be followed by another
               token.  If this happens, something is wrong with the token cache
               and the authoring code should be corrected. */
            check_assertion((it + 1) == it_end);
            break;
          }  /* if */
          this->enter_basic_token_to_cache(ifc_cache, tok);
        }
        break;
      case teik_asm_string:
      case teik_ifc_index:
      case teik_insert_string:
      case teik_pp_token:
      case teik_pragma:
      case teik_removed_expr:
      case teik_ud_lit:
        /* FIXME: Implement these. */
        ifc_write_catastrophe();
        break;
      case teik_unresolved_ud_lit:
        /* This token state should never appear in a token cache that is to be
           written to an IFC file; see the description of
           tok_unresolved_ud_literal. */
        unexpected_condition();
      default_is_unexpected();
    }  /* switch */
  }  /* for */
}  /* an_ifc_il_map::enter_token_cache */


an_ifc_type_index an_ifc_il_map::find_or_enter_class_scope_type()
/*
Find or enter the fundamental type used by the IFC to indicate that a given IFC
DeclSort::Scope is a class.  Return the index for the fundamental type.
*/
{
  if (is_null_index(this->fund_class_type)) {
    an_ifc_type_fundamental fund_type;

    this->fund_class_type = this->output_state->alloc_type(&fund_type);
    set_ifc_basis(&fund_type, ifc_tbs_class);
    set_ifc_precision(&fund_type, ifc_tps_default);
    set_ifc_sign(&fund_type, ifc_tss_plain);
  }  /* if */
  return this->fund_class_type;
}  /* an_ifc_il_map::find_or_enter_class_scope_type */


an_ifc_type_index an_ifc_il_map::find_or_enter_namespace_scope_type()
/*
Find or enter the fundamental type used by the IFC to indicate that a given IFC
DeclSort::Scope is a namespace.  Return the index for the fundamental type.
*/
{
  if (is_null_index(this->fund_namespace_type)) {
    an_ifc_type_fundamental fund_type;

    this->fund_namespace_type = this->output_state->alloc_type(&fund_type);
    set_ifc_basis(&fund_type, ifc_tbs_namespace);
    set_ifc_precision(&fund_type, ifc_tps_default);
    set_ifc_sign(&fund_type, ifc_tss_plain);
  }  /* if */
  return this->fund_namespace_type;
}  /* an_ifc_il_map::find_or_enter_namespace_scope_type */


an_ifc_type_index an_ifc_il_map::find_or_enter_alias_typedef_type()
/*
Find or enter the fundamental type used by the IFC to indicate that a given IFC
DeclSort::Alias is a typedef.  Return the index for the fundamental type.
*/
{
  if (is_null_index(this->fund_alias_typedef_type)) {
    an_ifc_type_fundamental fund_type;

    this->fund_alias_typedef_type = this->output_state->alloc_type(&fund_type);
    set_ifc_basis(&fund_type, ifc_tbs_typename);
    set_ifc_precision(&fund_type, ifc_tps_default);
    set_ifc_sign(&fund_type, ifc_tss_plain);
  }  /* if */
  return this->fund_alias_typedef_type;
}  /* an_ifc_il_map::find_or_enter_alias_typedef_type */


an_ifc_type_index an_ifc_il_map::find_or_enter_scoped_enum_type()
/*
Find or enter the fundamental type used by the IFC to indicate that a given IFC
DeclSort::Enumeration is a scoped enum type.  Return the index for the
fundamental type.
*/
{
  if (is_null_index(this->fund_scoped_enum_type)) {
    an_ifc_type_fundamental fund_type;

    this->fund_scoped_enum_type = this->output_state->alloc_type(&fund_type);
    set_ifc_basis(&fund_type, ifc_tbs_class);
    set_ifc_precision(&fund_type, ifc_tps_default);
    set_ifc_sign(&fund_type, ifc_tss_plain);
  }  /* if */
  return this->fund_scoped_enum_type;
}  /* an_ifc_il_map::find_or_enter_scoped_enum_type */


an_ifc_type_index an_ifc_il_map::find_or_enter_struct_scope_type()
/*
Find or enter the fundamental type used by the IFC to indicate that a given IFC
DeclSort::Scope is a struct.  Return the index for the fundamental type.
*/
{
  if (is_null_index(this->fund_struct_type)) {
    an_ifc_type_fundamental fund_type;

    this->fund_struct_type = this->output_state->alloc_type(&fund_type);
    set_ifc_basis(&fund_type, ifc_tbs_struct);
    set_ifc_precision(&fund_type, ifc_tps_default);
    set_ifc_sign(&fund_type, ifc_tss_plain);
  }  /* if */
  return this->fund_struct_type;
}  /* an_ifc_il_map::find_or_enter_struct_scope_type */


an_ifc_type_index an_ifc_il_map::find_or_enter_union_scope_type()
/*
Find or enter the fundamental type used by the IFC to indicate that a given IFC
DeclSort::Scope is a union.  Return the index for the fundamental type.
*/
{
  if (is_null_index(this->fund_union_type)) {
    an_ifc_type_fundamental fund_type;

    this->fund_union_type = this->output_state->alloc_type(&fund_type);
    set_ifc_basis(&fund_type, ifc_tbs_union);
    set_ifc_precision(&fund_type, ifc_tps_default);
    set_ifc_sign(&fund_type, ifc_tss_plain);
  }  /* if */
  return this->fund_union_type;
}  /* an_ifc_il_map::find_or_enter_union_scope_type */


an_ifc_type_index an_ifc_il_map::find_or_enter_unscoped_enum_type()
/*
Find or enter the fundamental type used by the IFC to indicate that a given IFC
DeclSort::Enumeration is an unscoped enum type.  Return the index for the
fundamental type.
*/
{
  if (is_null_index(this->fund_unscoped_enum_type)) {
    an_ifc_type_fundamental fund_type;

    this->fund_unscoped_enum_type = this->output_state->alloc_type(&fund_type);
    set_ifc_basis(&fund_type, ifc_tbs_enum);
    set_ifc_precision(&fund_type, ifc_tps_default);
    set_ifc_sign(&fund_type, ifc_tss_plain);
  }  /* if */
  return this->fund_unscoped_enum_type;
}  /* an_ifc_il_map::find_or_enter_unscoped_enum_type */


an_ifc_decl_index an_ifc_il_map::enter_alias_template(a_template_ptr templ)
/*
Enter the given alias template (templ) into the IFC output state.  Return the
declaration index of the alias template declaration.
*/
{
  an_ifc_decl_alias
                alias_templ;
  an_ifc_decl_index
                result = this->map_new_decl(templ, &alias_templ);

  /* Set the name information. */
  an_ifc_text_offset
                ifc_name_offset = this->entity_name_as_text_offset(templ);
  set_ifc_name(&alias_templ, ifc_name_offset);

  /* Set the source location information. */
  an_ifc_source_location
                ifc_src_pos = this->find_or_enter_entity_pos(templ);
  set_ifc_locus(&alias_templ, ifc_src_pos);

  /* Set the for-all type describing the parameters. */
  an_ifc_type_forall
                forall_type;
  an_ifc_type_index
                type_idx = this->output_state->alloc_type(&forall_type);
  an_ifc_chart_index
                param_chart = this->enter_template_params(templ);
  set_ifc_chart(&forall_type, param_chart);
  set_ifc_subject(&forall_type, this->find_or_enter_alias_typedef_type());
  set_ifc_type(&alias_templ, type_idx);

  /* Set the scope information. */
  an_ifc_decl_index
                scope_decl_idx = this->associate_entity_home_scope(templ);
  set_ifc_home_scope(&alias_templ, scope_decl_idx);

  /* Set the aliasee information. */
  an_ifc_type_syntactic
                syntactic_type;
  an_ifc_type_index
                subject_type_idx = this->output_state->alloc_type(
                                                              &syntactic_type);
  /* Create a token cache representation of the initializing constant. */
  an_ifc_output_token_cache
                init_token_cache = this->copy_template_body_to_cache(templ);
  an_ifc_expr_index
                aliasee_expr_idx = this->output_state->alloc_token_cache_expr(
                                                             init_token_cache);
  an_ifc_type_forall
                aliasee_type;
  an_ifc_type_index
                aliasee_type_idx = this->output_state->alloc_type(
                                                                &aliasee_type);
  set_ifc_expr(&syntactic_type, aliasee_expr_idx);
  set_ifc_chart(&aliasee_type, param_chart);
  set_ifc_subject(&aliasee_type, subject_type_idx);
  set_ifc_aliasee(&alias_templ, aliasee_type_idx);

  /* FIXME: Set specifiers. */

  /* Set the access specifier. */
  an_ifc_access_sort
                ifc_access = access_specifier_of(templ);
  set_ifc_access(&alias_templ, ifc_access);
  /* FIXME: Set properties. */
  return result;
}  /* an_ifc_il_map::enter_alias_template */


an_ifc_decl_index an_ifc_il_map::enter_class_template(a_template_ptr templ)
/*
Enter the given class template (templ) into the IFC output state.  Return the
declaration index of the class template declaration.
*/
{
  an_ifc_decl_template
                class_templ;
  an_ifc_decl_index
                result = this->map_new_decl(templ, &class_templ);

  /* Set the name information. */
  an_ifc_name_index
                ifc_name_index = this->entity_name_as_name_index(templ);
  set_ifc_name(&class_templ, ifc_name_index);

  /* Set the source location information. */
  an_ifc_source_location
                ifc_src_pos = this->find_or_enter_entity_pos(templ);
  set_ifc_locus(&class_templ, ifc_src_pos);

  /* Set the scope information. */
  an_ifc_decl_index
                scope_decl_idx = this->associate_entity_home_scope(templ);
  set_ifc_home_scope(&class_templ, scope_decl_idx);

  /* Set the template parameter chart. */
  an_ifc_chart_index
                param_chart = this->enter_template_params(templ);
  set_ifc_chart(&class_templ, param_chart);

  /* Set the parameterized entity information. */
  an_ifc_parameterized_entity
                ifc_entity(this->get_default_file());
  a_type_ptr    prototype_decl = templ->prototype_instantiation.type;
  an_ifc_decl_index
                prototype_decl_idx =
                                this->enter_class_struct_union(prototype_decl);
  set_ifc_decl(&ifc_entity, prototype_decl_idx);
  set_ifc_entity(&class_templ, ifc_entity);

  /* FIXME: Set type. */
  /* FIXME: Set specifiers. */

  /* Set the access specifier. */
  an_ifc_access_sort
                ifc_access = access_specifier_of(templ);
  set_ifc_access(&class_templ, ifc_access);
  /* FIXME: Set properties. */
  return result;
}  /* an_ifc_il_map::enter_class_template */


static INLINE an_ifc_function_traits_bitfield build_function_traits(
                                                      an_ifc_module_file *file,
                                                      a_routine_ptr      rp)
/*
Construct and return a new IFC function traits bitfield value for the given
routine and module file.
*/
{
  an_ifc_function_traits_bitfield_query
          traits = (an_ifc_function_traits_bitfield_query)0;

  if (rp->is_inline) {
    traits = traits | ifc_ftb_inline;
  }  /* if */
  if (rp->is_constexpr) {
    traits = traits | ifc_ftb_constexpr;
  }  /* if */
  if (rp->is_consteval) {
    traits = traits | ifc_ftb_immediate;
  }  /* if */
  if (rp->is_explicit_constructor || rp->is_explicit_conversion_function) {
    traits = traits | ifc_ftb_explicit;
  }  /* if */
  if (rp->is_virtual) {
    traits = traits | ifc_ftb_virtual;
  }  /* if */
  if (rp->pure_virtual) {
    traits = traits | ifc_ftb_pure_virtual;
  }  /* if */
  if (rp->is_defaulted || rp->compiler_generated) {
    traits = traits | ifc_ftb_defaulted;
  }  /* if */
  if (rp->is_deleted) {
    traits = traits | ifc_ftb_deleted;
  }  /* if */
  /* FIXME: Handle: NoReturn, HiddenFriend, and Constrained? */

  an_ifc_function_traits_bitfield_storage
          ifc_raw_traits = to_bitmask(file, traits);
  return an_ifc_function_traits_bitfield(file, ifc_raw_traits);
}  /* build_function_traits */


an_ifc_decl_index an_ifc_il_map::enter_constructor(a_routine_ptr rp)
/*
Enter the given constructor (routine) into the IFC output state.  Return the
declaration index of the constructor declaration.
*/
{
  an_ifc_decl_constructor
                func_decl;
  an_ifc_decl_index
                result = this->map_new_decl(rp, &func_decl);

  /* Set the name information. */
  an_ifc_text_offset
                ifc_name_offset = this->entity_name_as_text_offset(rp);
  set_ifc_name(&func_decl, ifc_name_offset);

  /* Set the source location information. */
  an_ifc_source_location
                ifc_src_pos = this->find_or_enter_entity_pos(rp);
  set_ifc_locus(&func_decl, ifc_src_pos);

  /* Set the type information. */
  a_type_ptr    type = rp->type;
  an_ifc_type_index
                type_idx = this->find_or_enter_type(type);
  set_ifc_type(&func_decl, type_idx);

  /* Set the scope information. */
  an_ifc_decl_index
                scope_decl_idx = this->associate_entity_home_scope(rp);
  set_ifc_home_scope(&func_decl, scope_decl_idx);

  /* Set the parameter information. */
  an_ifc_chart_index
                param_chart_idx = this->enter_routine_params(rp);
  set_ifc_chart(&func_decl, param_chart_idx);

  /* Set the function traits. */
  an_ifc_function_traits_bitfield
                func_traits = build_function_traits(func_decl.get_file(),
                                                    rp);
  set_ifc_traits(&func_decl, func_traits);
  /* FIXME: Set specifiers. */

  /* Set the access specifier. */
  an_ifc_access_sort
                ifc_access = access_specifier_of(rp);
  set_ifc_access(&func_decl, ifc_access);
  /* FIXME: Set properties. */
  return result;
}  /* an_ifc_il_map::enter_constructor */


an_ifc_decl_index an_ifc_il_map::enter_destructor(a_routine_ptr rp)
/*
Enter the given destructor (routine) into the IFC output state.  Return the
declaration index of the destructor declaration.
*/
{
  an_ifc_decl_destructor
                func_decl;
  an_ifc_decl_index
                result = this->map_new_decl(rp, &func_decl);

  /* Set the name information. */
  an_ifc_text_offset
                ifc_name_offset = this->entity_name_as_text_offset(rp);
  set_ifc_name(&func_decl, ifc_name_offset);

  /* Set the source location information. */
  an_ifc_source_location
                ifc_src_pos = this->find_or_enter_entity_pos(rp);
  set_ifc_locus(&func_decl, ifc_src_pos);

  /* Set the scope information. */
  an_ifc_decl_index
                scope_decl_idx = this->associate_entity_home_scope(rp);
  set_ifc_home_scope(&func_decl, scope_decl_idx);
  /* FIXME: Set eh_spec. */

  /* Set the function traits. */
  an_ifc_function_traits_bitfield
                func_traits = build_function_traits(func_decl.get_file(),
                                                    rp);
  set_ifc_traits(&func_decl, func_traits);
  /* FIXME: Set specifiers. */

  /* Set the access specifier. */
  an_ifc_access_sort
                ifc_access = access_specifier_of(rp);
  set_ifc_access(&func_decl, ifc_access);
  /* FIXME: Set convention. */
  /* FIXME: Set properties. */
  return result;
}  /* an_ifc_il_map::enter_destructor */


an_ifc_decl_index an_ifc_il_map::enter_field(a_field_ptr field)
/*
Enter the given field into the IFC output state.  Return the declaration index
of the field declaration.
*/
{
  an_ifc_decl_index result;

  if (field->bit_size == 0) {
    an_ifc_decl_field
                field_decl;

    result = this->map_new_decl(field, &field_decl);

    /* Set the name information. */
    an_ifc_text_offset
                ifc_name_offset = this->entity_name_as_text_offset(field);
    set_ifc_name(&field_decl, ifc_name_offset);

    /* Set the source location information. */
    an_ifc_source_location
                ifc_src_pos = this->find_or_enter_entity_pos(field);
    set_ifc_locus(&field_decl, ifc_src_pos);

    /* Set the type information. */
    an_ifc_type_index
                type_idx = this->find_or_enter_type(field->type);
    set_ifc_type(&field_decl, type_idx);

    /* Set the scope information. */
    an_ifc_decl_index
                scope_decl_idx = this->associate_entity_home_scope(field);
    set_ifc_home_scope(&field_decl, scope_decl_idx);

    /* Set the initializer. */
    if (field->has_initializer) {
      a_shared_token_cache
                init_cache = get_field_initializer_for_module_write(field);
      an_ifc_output_token_cache
                init_token_cache;
      this->enter_token_cache(&init_token_cache, init_cache.ptr());

      an_ifc_expr_index
                ifc_init_expr = this->output_state->alloc_token_cache_expr(
                                                             init_token_cache);
      set_ifc_initializer(&field_decl, ifc_init_expr);
    }  /* if */
    /* FIXME: Set alignment. */
    /* FIXME: Set traits. */
    /* FIXME: Set specifier. */

    /* Set the access specifier. */
    an_ifc_access_sort ifc_access = access_specifier_of(field);
    set_ifc_access(&field_decl, ifc_access);
    /* FIXME: Set properties. */
  } else {
    /* FIXME: Implement bitfields. */
    ifc_write_catastrophe();
  }  /* if */
  return result;
}  /* an_ifc_il_map::enter_field */


an_ifc_decl_index an_ifc_il_map::enter_free_function(a_routine_ptr rp)
/*
Enter the given free function (routine) into the IFC output state.  Return the
declaration index of the free function declaration.
*/
{
  an_ifc_decl_function
                func_decl;
  an_ifc_decl_index
                result = this->map_new_decl(rp, &func_decl);

  /* Set the name information. */
  an_ifc_name_index
                ifc_name_index = this->entity_name_as_name_index(rp);
  set_ifc_name(&func_decl, ifc_name_index);

  /* Set the source location information. */
  an_ifc_source_location
                ifc_src_pos = this->find_or_enter_entity_pos(rp);
  set_ifc_locus(&func_decl, ifc_src_pos);

  /* Set the type information. */
  a_type_ptr    type = rp->type;
  an_ifc_type_index
                type_idx = this->find_or_enter_type(type);
  set_ifc_type(&func_decl, type_idx);

  /* Set the scope information. */
  an_ifc_decl_index
                scope_decl_idx = this->associate_entity_home_scope(rp);
  set_ifc_home_scope(&func_decl, scope_decl_idx);

  /* Set the parameter information. */
  an_ifc_chart_index
                param_chart_idx = this->enter_routine_params(rp);
  set_ifc_chart(&func_decl, param_chart_idx);

  /* Set the function traits. */
  an_ifc_function_traits_bitfield
                func_traits = build_function_traits(func_decl.get_file(),
                                                    rp);
  set_ifc_traits(&func_decl, func_traits);
  /* FIXME: Set specifiers. */

  /* Set the access specifier. */
  an_ifc_access_sort
                ifc_access = access_specifier_of(type);
  set_ifc_access(&func_decl, ifc_access);

  /* Apply the appropriate reachable property flags. */
  an_ifc_reachable_properties_bitfield_query
                properties = (an_ifc_reachable_properties_bitfield_query)0;
  /* Flag that an initializer is present if relevant. */
  if (rp->assoc_template != NULL) {
    an_ifc_edg_trait_function_definition
                def_trait;

    this->output_state->alloc_decl_trait(result, &def_trait);

    /* Create an IFC token cache corresponding to the front end template token
       cache. */
    a_template_ptr
                templ = rp->assoc_template;
    an_ifc_output_token_cache
                init_token_cache = this->copy_template_body_to_cache(templ);
    an_ifc_edg_token_cache_offset
                token_cache_offset = this->output_state->alloc_token_cache(
                                                             init_token_cache);
    set_ifc_initializer(&def_trait, token_cache_offset);
    /* Apply the initializer flag to indicate the presence of this
       definition. */
    properties = properties | ifc_rpb_initializer;
  } else if (is_routine_definition_exported_inline(rp)) {
    an_ifc_edg_trait_function_definition
                def_trait;

    this->output_state->alloc_decl_trait(result, &def_trait);

    /* Create an IFC token cache corresponding to the saved front end
       definition token cache. */
    a_shared_token_cache
                definition_cache =
                                  get_function_definition_for_module_write(rp);
    an_ifc_output_token_cache
                init_token_cache;
    this->enter_token_cache(&init_token_cache, definition_cache.ptr());

    an_ifc_edg_token_cache_offset
                token_cache_offset = this->output_state->alloc_token_cache(
                                                             init_token_cache);
    set_ifc_initializer(&def_trait, token_cache_offset);
    /* Apply the initializer flag to indicate the presence of this
       definition. */
    properties = properties | ifc_rpb_initializer;
  }  /* if */

  an_ifc_reachable_properties_bitfield_storage
                ifc_raw_properties = to_bitmask(func_decl.get_file(),
                                                properties);
  an_ifc_reachable_properties_bitfield
                ifc_properties(func_decl.get_file(), ifc_raw_properties);
  set_ifc_properties(&func_decl, ifc_properties);
  return result;
}  /* an_ifc_il_map::enter_free_function */


an_ifc_decl_index an_ifc_il_map::enter_function_template(a_template_ptr templ)
/*
Enter the given function template declaration into the IFC output state.
Return the declaration index of the function template declaration.
*/
{
  an_ifc_decl_template
                func_templ;
  an_ifc_decl_index
                result = this->map_new_decl(templ, &func_templ);

  /* Set the name information. */
  an_ifc_name_index
                ifc_name_index = this->entity_name_as_name_index(templ);
  set_ifc_name(&func_templ, ifc_name_index);

  /* Set the source location information. */
  an_ifc_source_location
                ifc_src_pos = this->find_or_enter_entity_pos(templ);
  set_ifc_locus(&func_templ, ifc_src_pos);

  /* Set the scope information. */
  an_ifc_decl_index
                scope_decl_idx = this->associate_entity_home_scope(templ);
  set_ifc_home_scope(&func_templ, scope_decl_idx);

  /* Set the template parameter chart. */
  an_ifc_chart_index
                param_chart = this->enter_template_params(templ);
  set_ifc_chart(&func_templ, param_chart);

  /* Set the parameterized entity information. */
  an_ifc_parameterized_entity
                ifc_entity(this->get_default_file());
  a_routine_ptr prototype_decl = templ->prototype_instantiation.routine;
  an_ifc_decl_index
                prototype_decl_idx = this->enter_routine(prototype_decl);
  set_ifc_decl(&ifc_entity, prototype_decl_idx);
  set_ifc_entity(&func_templ, ifc_entity);

  /* FIXME: Set type. */
  /* FIXME: Set specifiers. */

  /* Set the access specifier. */
  an_ifc_access_sort
                ifc_access = access_specifier_of(templ);
  set_ifc_access(&func_templ, ifc_access);
  /* FIXME: Set properties. */
  return result;
}  /* an_ifc_il_map::enter_function_template */


an_ifc_decl_index an_ifc_il_map::enter_member_function(a_routine_ptr rp)
/*
Enter the given member function (routine) into the IFC output state.  Return
the declaration index of the member function declaration.
*/
{
  an_ifc_decl_method
                func_decl;
  an_ifc_decl_index
                result = this->map_new_decl(rp, &func_decl);

  /* Set the name information. */
  an_ifc_name_index
                ifc_name_index = this->entity_name_as_name_index(rp);
  set_ifc_name(&func_decl, ifc_name_index);

  /* Set the source location information. */
  an_ifc_source_location
                ifc_src_pos = this->find_or_enter_entity_pos(rp);
  set_ifc_locus(&func_decl, ifc_src_pos);

  /* Set the type information. */
  a_type_ptr    type = rp->type;
  an_ifc_type_index
                type_idx = this->find_or_enter_type(type);
  set_ifc_type(&func_decl, type_idx);

  /* Set the scope information. */
  an_ifc_decl_index
                scope_decl_idx = this->associate_entity_home_scope(rp);
  set_ifc_home_scope(&func_decl, scope_decl_idx);

  /* Set the parameter information. */
  an_ifc_chart_index
                param_chart_idx = this->enter_routine_params(rp);
  set_ifc_chart(&func_decl, param_chart_idx);

  /* Set the function traits. */
  an_ifc_function_traits_bitfield
                func_traits = build_function_traits(func_decl.get_file(),
                                                    rp);
  set_ifc_traits(&func_decl, func_traits);
  /* FIXME: Set specifiers. */

  /* Set the access specifier. */
  an_ifc_access_sort
                ifc_access = access_specifier_of(type);
  set_ifc_access(&func_decl, ifc_access);

  /* Apply the appropriate reachable property flags. */
  an_ifc_reachable_properties_bitfield_query
                properties = (an_ifc_reachable_properties_bitfield_query)0;
  /* Flag that an initializer is present if relevant. */
  if (is_routine_definition_exported_inline(rp)) {
    an_ifc_edg_trait_function_definition
                def_trait;

    this->output_state->alloc_decl_trait(result, &def_trait);

    /* Create an IFC token cache corresponding to the saved front end
       definition token cache. */
    a_shared_token_cache
                definition_cache =
                                  get_function_definition_for_module_write(rp);
    an_ifc_output_token_cache
                init_token_cache;
    this->enter_token_cache(&init_token_cache, definition_cache.ptr());

    an_ifc_edg_token_cache_offset
                token_cache_offset = this->output_state->alloc_token_cache(
                                                             init_token_cache);
    set_ifc_initializer(&def_trait, token_cache_offset);
    /* Apply the initializer flag to indicate the presence of this
       definition. */
    properties = properties | ifc_rpb_initializer;
  }  /* if */
  an_ifc_reachable_properties_bitfield_storage
                ifc_raw_properties = to_bitmask(func_decl.get_file(),
                                                properties);
  an_ifc_reachable_properties_bitfield
                ifc_properties(func_decl.get_file(), ifc_raw_properties);
  set_ifc_properties(&func_decl, ifc_properties);
  return result;
}  /* an_ifc_il_map::enter_member_function */


an_ifc_decl_index an_ifc_il_map::enter_namespace(a_scope_ptr scope)
/*
Enter the namespace corresponding to the given scope into the IFC output state.
Return the declaration index of the scope declaration.
*/
{
  check_assertion(scope->kind == sck_namespace ||
                  scope->kind == sck_namespace_extension ||
                  scope->kind == sck_namespace_reactivation);
  a_namespace_ptr
                nsp = scope->variant.assoc_namespace;
  an_ifc_decl_index
                result = this->find_or_enter_namespace(nsp);

  /* Resolve the scope to this DeclIndex to resolve the scope for find_
     functions that utilize the scope. */
  this->il_entry_to_decl.map(make_tagged_ptr(scope), result);

  /* Fetch the scope declaration representing the namespace; there is now at
     least one element that needs to be associated with it. */
  an_ifc_decl_scope
		scope_decl;
  this->output_state->fetch_decl(&scope_decl, result);

  /* Associate the namespace scope decl with its contents. */
  an_ifc_scope_offset
                initializer = this->find_or_enter_scope(scope);
  set_ifc_initializer(&scope_decl, initializer);
  return result;
}  /* an_ifc_il_map::enter_namespace */


an_ifc_decl_index an_ifc_il_map::enter_typedef(a_type_ptr type)
/*
Enter the typedef corresponding to the given type into the IFC output state.
Return the declaration index of the IFC DeclSort::Alias (i.e., the IFC
representation of the typedef).
*/
{
  check_assertion(type->kind == tk_typeref && typeref_is_typedef(type));
  an_ifc_decl_alias
                alias_decl;
  an_ifc_decl_index
                result = this->map_new_decl(type, &alias_decl);
  /* Set the name information. */
  an_ifc_text_offset
                ifc_name_offset = this->entity_name_as_text_offset(type);
  set_ifc_name(&alias_decl, ifc_name_offset);

  /* Set the source location information. */
  an_ifc_source_location
                ifc_src_pos = this->find_or_enter_entity_pos(type);
  set_ifc_locus(&alias_decl, ifc_src_pos);

  /* Set the IFC fundamental type to indicate this is a typedef
     DeclSort::Alias. */
  an_ifc_type_index
                type_kind_type = this->find_or_enter_alias_typedef_type();
  set_ifc_type(&alias_decl, type_kind_type);

  /* Set the scope information. */
  an_ifc_decl_index
                scope_decl_idx = this->associate_entity_home_scope(type);
  set_ifc_home_scope(&alias_decl, scope_decl_idx);

  /* Set the aliasee type. */
  an_ifc_type_index
                aliasee = this->find_or_enter_type(type->variant.typeref.type);
  set_ifc_aliasee(&alias_decl, aliasee);
  /* FIXME: Set specifiers. */

  /* Set the access specifier. */
  an_ifc_access_sort ifc_access = access_specifier_of(type);
  set_ifc_access(&alias_decl, ifc_access);
  return result;
}  /* an_ifc_il_map::enter_typedef */


an_ifc_decl_index an_ifc_il_map::find_or_enter_home_scope(a_scope_ptr scope)
/*
For the given scope find or enter the associated scope information into the IFC
output state.  Return the declaration index for the scope.

Note that this function is typically not what should be used to set an entity's
home scope as it does not register the entity with the scope.  Instead, prefer
using associate_entity_home_scope.
*/
{
  a_tagged_pointer
                tagged_scope = make_tagged_ptr(scope);
  an_ifc_decl_index
                result = this->il_entry_to_decl.get(tagged_scope);
  if (is_null_index(result)) {
    result = this->enter_home_scope(scope);
  }  /* if */
  return result;
}  /* an_ifc_il_map::find_or_enter_home_scope */


an_ifc_decl_index an_ifc_il_map::enter_home_scope(a_scope_ptr scope)
/*
For the given scope enter the associated scope information into the IFC output
state.  Return the declaration index for the scope.
*/
{
  check_assertion(is_null_index(
                          this->il_entry_to_decl.get(make_tagged_ptr(scope))));
  an_ifc_decl_index result;

  switch (scope->kind) {
    case sck_class_struct_union:
      result =
             this->find_or_enter_class_struct_union(scope->variant.assoc_type);
      break;
    case sck_file:
      /* Use a null index to indicate the primary scope. */
      /* Additionally ensure this scope is entered in the scope table. */
      (void)this->find_or_enter_scope(scope);
      break;
    case sck_namespace:
    case sck_namespace_extension:
    case sck_namespace_reactivation:
      result = this->enter_namespace(scope);
      break;
    case sck_func_prototype:
    case sck_block:
    case sck_class_reactivation:
    case sck_template_declaration:
    case sck_template_instantiation:
    case sck_instantiation_context:
    case sck_module_decl_import:
    case sck_module_isolated:
    case sck_pragma:
    case sck_function_access:
    case sck_condition:
    case sck_enum:
    case sck_function:
    case sck_none:
      break;
    default_is_unexpected();
  }  /* switch */
  return result;
}  /* an_ifc_il_map::enter_home_scope */


void an_ifc_il_map::map_scope_member(a_scope_ptr       scope,
                                     an_ifc_decl_index decl)
/*
Map the given declaration index to the given scope's associated array of scope
members.
*/
{
  an_ifc_scope_offset offset = this->find_or_enter_scope(scope);

  /* A null index should never be mapped as a member of a scope. */
  check_assertion(!is_null_index(decl));
  this->scope_members[offset.value - 1].push_back(decl);
}  /* an_ifc_il_map::map_scope_member */


template<typename a_Type>
an_ifc_decl_index an_ifc_il_map::associate_entity_home_scope(a_Type *il_entity)
/*
For the given IL entity associate the parent scope and the corresponding IFC
declaration in the IFC output state.  Return the declaration index for the
entity's scope.
*/
{
  a_scope_ptr scope = il_entity->source_corresp.parent_scope;

  return this->associate_entity_home_scope(il_entity, scope);
}  /* an_ifc_il_map::associate_entity_home_scope */


template<typename a_Type>
an_ifc_decl_index an_ifc_il_map::associate_entity_home_scope(
                                                        a_Type      *il_entity,
                                                        a_scope_ptr scope)
/*
For the given IL entity associate the parent scope and the corresponding IFC
declaration in the IFC output state.  Return the declaration index for the
entity's scope.
*/
{
  /* First fetch the entity's IFC DeclIndex.  Then associate the scope and
     DeclIndex. */
  a_tagged_pointer
                tagged_entity = make_tagged_ptr(il_entity);

  /* Only include the entity in the scope member list if it's "real." */
  if (!entity_is_nonreal(tagged_entity)) {
    an_ifc_decl_index
                entity_idx = this->il_entry_to_decl.get(tagged_entity);

    this->map_scope_member(scope, entity_idx);
  }  /* if */
  /* Finally, enter the home scope declaration itself if not already
     entered. */
  return this->find_or_enter_home_scope(scope);
}  /* an_ifc_il_map::associate_entity_home_scope */

}  /* namespace */

static void dump_scope_types(an_ifc_il_map *il_map,
                             a_scope_ptr   scope)
/*
Add all the types in the given scope to the given IL -> IFC mapping.
*/
{
  for (a_type_ptr type = scope->types; type != NULL; type = type->next) {
    if (is_template_class_type(type) || is_template_alias_type(type)) {
      /* Skip template types (these will be handled by
         dump_scope_templates). */
      continue;
    }  /* if */
    if (type->kind == tk_template_param && tptk_is(type, tptk_member)) {
      /* Skip member template parameters, since these are used for nonreal
         nested types (and nonreal nested types are not currently relevant to
         modules).  See create_nonreal_version_of_nested_type for more
         information. */
      continue;
    }  /* if */
    if (type->kind == tk_typeref && type->variant.typeref.predeclared) {
      /* Skip predeclared typedefs (e.g., "_Float16", which is entered in gnu
         and clang emulation modes):  they are re-created by the predefined
         type entry processing of an importing translation unit, and the
         types of some of them cannot currently be written to an IFC file. */
      continue;
    }  /* if */
    (void)il_map->find_or_enter_type(type);
  }  /* for */
}  /* dump_scope_types */


static void dump_scope_templates(an_ifc_il_map *il_map,
                                 a_scope_ptr   scope)
/*
Add all the templates in the given scope to the given IL -> IFC mapping.
*/
{
  for (a_template_ptr tp = scope->templates; tp != NULL; tp = tp->next) {
    (void)il_map->find_or_enter_template(tp);
  }  /* for */
}  /* dump_scope_templates */


static void dump_scope_routines(an_ifc_il_map *il_map,
                                a_scope_ptr   scope)
/*
Add all the routines in the given scope to the given IL -> IFC mapping.
*/
{
  for (a_routine_ptr rp = scope->routines; rp != NULL; rp = rp->next) {
    if (rp->is_template_function && !rp->is_specialized) {
      /* This is a function template which will be handled when
         dump_scope_templates is called. */
      continue;
    }  /* if */
    (void)il_map->find_or_enter_routine(rp);
  }  /* for */
}  /* dump_scope_routines */


static void dump_scope_namespaces(an_ifc_il_map *il_map,
                                  a_scope_ptr   scope)
/*
Add all the namespaces in the given scope to the given IL -> IFC mapping.
*/
{
  for (a_namespace_ptr np = scope->namespaces; np != NULL; np = np->next) {
    if (np->is_namespace_alias) {
      ifc_write_catastrophe();
    } else {
      a_scope_ptr assoc_scope = np->variant.assoc_scope;

      dump_scope_recursively(il_map, assoc_scope);
    }  /* if */
  }  /* for */
}  /* dump_scope_namespaces */


static void dump_scope_using_directives(an_ifc_il_map *il_map,
                                        a_scope_ptr   scope)
/*
Add all the using-directives in the given scope to the given IL -> IFC mapping.
*/
{
  for (a_using_decl_ptr udp = scope->using_directives; udp != NULL;
       udp = udp->next) {
    (void)il_map->find_or_enter_using_directive(udp, scope);
  }  /* for */
}  /* dump_scope_using_directives */


static void dump_scope_recursively(an_ifc_il_map *il_map,
                                   a_scope_ptr   scope)
/*
Add the contents of the given scope and all its child scopes to the given IL ->
IFC mapping.
*/
{
  dump_scope_types(il_map, scope);
  dump_scope_templates(il_map, scope);
  dump_scope_routines(il_map, scope);
  dump_scope_namespaces(il_map, scope);
  dump_scope_using_directives(il_map, scope);
}  /* dump_scope_recursively */


static void complete_scope_info(an_ifc_output_state *output_state,
                                an_ifc_il_map       *il_map)
/*
The IFC scope membership information is built up during the IL -> IFC mapping
process.  This function is called once the IL has been completely mapped to the
output state to create the scope membership information as a sort of
"post-processing" step.

This is done as IFC scopes are required to be contiguous blocks of declaration
indexes; thus, by collecting the scope members and deferring construction no
special traversal logic is required to account for all scope members.
*/
{
  size_t num_of_scopes = il_map->get_number_of_scopes();
  size_t scope_start = 0;

  for (size_t i = 0; i < num_of_scopes; ++i) {
    an_ifc_scope_descriptor scope_descr;

    output_state->fetch_node(&scope_descr, i);

    const a_scope_member_array &members = il_map->get_scope_members(i);
    for (an_ifc_decl_index decl_idx : members) {
      an_ifc_scope_member scope_mem;

      (void)output_state->alloc_node(&scope_mem);
      set_ifc_index(&scope_mem, decl_idx);
    }  /* for */

    an_ifc_module_file *file = scope_descr.get_file();
    an_ifc_index       ifc_scope_start(file, scope_start);
    an_ifc_cardinality ifc_scope_count(
                                file,
                                (an_ifc_cardinality_storage)members.length());
    set_ifc_start(&scope_descr, ifc_scope_start);
    set_ifc_cardinality(&scope_descr, ifc_scope_count);
    scope_start += members.length();
  }  /* for */
}  /* complete_scope_info */


static an_error_code module_to_error_code(a_module_ptr mod)
/*
Return the error code corresponding to the current output module kind.
*/
{
  an_error_code result;

  if (mod == NULL) {
    result = ec_edg_ifc_header_unit;
  } else if (mod->kind == mk_unit) {
    result = ec_edg_ifc_interface_unit;
  } else if (mod->kind == mk_unit_partition) {
    result = ec_edg_ifc_partition_unit;
  } else {
    /* This should not be possible as the module writing code should not have
       been called. */
    unexpected_condition();
  }  /* switch */
  return result;
}  /* module_to_error_code */


static Opt<an_ifc_module_file> create_output_file(a_const_char *file_path,
                                                  a_module_ptr source_module)
/*
Create and return an instance of an IFC module file (with an open file handle
in binary write mode) intended for writing with the given file path and file
kind.

If creation of the file fails for any reason an empty optional is instead
returned.
*/
{
  FILE *f_handle = open_output_file_with_error_handling(
                                          file_path,
                                          /*binary_file=*/TRUE,
                                          /*update_mode=*/FALSE,
                                          /*open_flags=*/OFF_NO_OPTIONS,
                                          module_to_error_code(source_module));

  if (f_handle == NULL) {
    return {};
  }  /* if */

  an_ifc_module_file result(mfk_edg_ifc, /*for_read=*/FALSE);
  result.version_major = 0;
  result.version_minor = 44;
  result.f_module = f_handle;

  an_ifc_module_file_write_state &write_state = result.get_write_state();
  write_state.source_module = source_module;
  write_state.source_file_name = primary_source_file_name;
  return {move_from(&result)};
}  /* create_output_file */


STATIC_THREAD an_ifc_module_file
                *output_module_file;
                        /* The IFC module file being written to.  This is
                           exposed as a global variable so that it can be
                           properly destroyed in the event a catastrophic error
                           occurs (resulting in a longjmp back to EDG_MAIN in
                           MAKE_FRONT_END_CALLABLE configurations) while
                           writing the file.  This is important as otherwise
                           the associated file descriptor is leaked. */


void ifc_modules_write_one_time_init()
/*
Do one-time initialization of static variables defined in this file.
*/
{
  output_module_file = NULL;
}  /* ifc_modules_write_one_time_init */


void ifc_modules_write_out()
/*
Write out the module files for the current translation unit in the EDG flavor
of the IFC format.
*/
{
  a_const_char  *output_file_name = module_unit_output_file_name;
  Opt<an_ifc_module_file>
                opt_module_file = create_output_file(output_file_name,
                                                     trans_unit_module);

  if (opt_module_file.has_value()) {
    /* Verify that there's not already an IFC module file in the process of
       being written.  If multiple IFC modules are intentionally being written
       at the same time, output_module_file should be updated to be a list of
       modules being written out. */
    check_assertion(output_module_file == NULL);
    /* Move the output module file to output_module_file (see
       output_module_file for more information). */
    output_module_file = new_general<an_ifc_module_file>(
                                               move_from(&(*opt_module_file)));

    an_ifc_output_state output_state(output_module_file);
    an_ifc_il_map       il_map(&output_state);
    a_scope_ptr         scope = il_header.primary_scope;
    an_ifc_scope_offset ifc_global_scope = il_map.enter_scope(scope);
    output_state.set_global_scope(ifc_global_scope);
    dump_scope_recursively(&il_map, scope);
    complete_scope_info(&output_state, &il_map);
    output_state.sort_traits();
    output_state.write();
    /* Free the output module file. */
    delete_general(&output_module_file);
  }  /* if */
}  /* ifc_modules_write_out */

#if MAKE_FRONT_END_CALLABLE

void ifc_modules_write_cleanup()
/*
This routine is called at the end of compilation, or if compilation is
terminated prematurely for some reason.  See ifc_modules_cleanup for more
information.
*/
{
  /* Clean up the IFC output module file if it exists (this can happen when a
     catastrophic error occurs and we longjmp back to EDG_MAIN). */
  delete_general(&output_module_file);
}  /* ifc_modules_write_cleanup */

#endif /* MAKE_FRONT_END_CALLABLE */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

