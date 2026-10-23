/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

ifc_modules_internal.h -- Declarations and forward declarations exposed only to
                          the IFC module implementation files.  For IFC-related
                          declarations used by the broader front end see
                          ifc_modules.h.

*/

/* Avoid including these declarations more than once: */
#ifndef IFC_MODULES_INTERNAL_H
#define IFC_MODULES_INTERNAL_H 1

#if !STANDALONE_UTILITY_PROGRAM

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

struct a_module;
struct an_ifc_input_state;

#if !ASSUME_LITTLE_ENDIAN_IFC_MODULES

enum an_ifc_module_primary_endianness {
  ifc_mpe_little,       /* The primary endianness of the module is
                           little-endian. */
  ifc_mpe_big,          /* The primary endianness of the module is
                           big-endian. */
  ifc_mpe_unknown       /* The primary endianness of the module is
                           unknown. */
};

#endif /* !ASSUME_LITTLE_ENDIAN_IFC_MODULES */

/*
The minimum major and minor supported version combination.
*/
#define IFC_MIN_VER_MAJOR 0
#define IFC_MIN_VER_MINOR 33

/*
State for an_ifc_module_file when the object represents an IFC file read.
*/
struct an_ifc_module_file_read_state {
  an_ifc_input_state
                *input_state = NULL;
                        /* The IFC module input state associated with this file
                           (if any). */
  size_t        f_size = 0;
                        /* The size of the module file. */
#if USE_MMAP_FOR_MEMORY_REGIONS
  void          *mmap_addr = NULL;
                        /* A pointer to the memory-mapped beginning of the
                           module file. */
  size_t        mmap_size = 0;
                        /* The size of the memory-mapped partition. */
  a_mapped_input_file
                mapped_file = {};
                        /* Handles for the memory-mapped input file. */
  unsigned char
                *byte_buffer = NULL;
                        /* Pointer to the current position in the buffer
                           used by get_byte, etc. */
  unsigned char
                *buffer_end = NULL;
                        /* Pointer to the last byte of the buffer used by
                           get_byte, etc. */
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
};  /* an_ifc_module_file_read_state */

/*
State for an_ifc_module_file when the object represents an IFC file write.
*/
struct an_ifc_module_file_write_state {
  a_module      *source_module;
                        /* The IL module being written. */
  a_const_char  *source_file_name;
                        /* The source file name that was used to build this
                           module file. */
};  /* an_ifc_module_file_write_state */

/* Forward declaration of a_module_file_kind from il_def.h. */
enum a_module_file_kind : a_byte;

/* Forward declaration of a_token_kind from il_def.h. */
enum a_token_kind : unsigned short;

/*
A structure abstracting the representation of the IFC module file and the
associated memory managed.
*/
struct an_ifc_module_file {
  an_ifc_module_file(a_module_file_kind mk, a_boolean for_read_val = TRUE);
  an_ifc_module_file(an_ifc_module_file &&old);
  ~an_ifc_module_file();

  an_ifc_module_file &operator=(an_ifc_module_file &&old);

  void close();

  a_boolean is_for_read() const
    { return this->for_read; }

  an_ifc_module_file_read_state& get_read_state()
    { check_assertion(this->for_read); return this->read_state; }
  an_ifc_module_file_read_state const& get_read_state() const
    { check_assertion(this->for_read); return this->read_state; }

  an_ifc_module_file_write_state& get_write_state()
    { check_assertion(!this->for_read); return this->write_state; }
  an_ifc_module_file_write_state const& get_write_state() const
    { check_assertion(!this->for_read); return this->write_state; }

  a_module_file_kind
                module_kind;
                        /* The module file kind. */
  an_ifc_version_storage
                version_major = IFC_MIN_VER_MAJOR;
                        /* The module file's major version.  Defaulted to the
                           lowest supported version until initialization is
                           complete. */
  an_ifc_version_storage
                version_minor = IFC_MIN_VER_MINOR;
                        /* The module file's minor version.  Defaulted to the
                           lowest supported version until initialization is
                           complete. */
#if !ASSUME_LITTLE_ENDIAN_IFC_MODULES
  an_ifc_module_primary_endianness
                endianness = ifc_mpe_unknown;
                        /* The primary endianness of the module file. */
#endif /* !ASSUME_LITTLE_ENDIAN_IFC_MODULES */
  FILE          *f_module = NULL;
                        /* The file descriptor for the file. */
private:
  a_boolean     for_read;
                        /* TRUE if this represents an IFC module file that's
                           being read from.  FALSE if this represents an IFC
                           module file that's being written to. */
#ifdef UNION_AS_STRUCT
/* FIXME: Workaround for union-as-struct build issue. */
#undef union
#endif /* ifdef UNION_AS_STRUCT */
  union {
    /* When for_read is TRUE: */
    an_ifc_module_file_read_state
                read_state;
                        /* The state associated with reading an IFC file. */
    /* When for_read is FALSE: */
    an_ifc_module_file_write_state
                write_state;
                        /* The state associated with writing an IFC file. */
  };
#ifdef UNION_AS_STRUCT
#define union struct
#endif /* ifdef UNION_AS_STRUCT */
};  /* an_ifc_module_file */

extern a_boolean is_for_read(an_ifc_module_file *file);


inline an_ifc_input_state *input_state_for(const an_ifc_module_entry &entry)
/*
Given an IFC module entry, return the corresponding an_ifc_input_state
instance.
*/
{
  an_ifc_input_state *input_state = entry.file->get_read_state().input_state;

  /* If this assertion fails, the caller is using an IFC file instance that
     does not have a corresponding input state set on it.  This can happen when
     using IFC processing logic with an IFC module file lacking an associated
     IFC module interface. */
  check_assertion_str(input_state != NULL, "module requested but not bound");
  return input_state;
}  /* input_state_for */


extern a_boolean is_at_least(an_ifc_module_file     *file,
                             an_ifc_version_storage minimum_version_major,
                             an_ifc_version_storage minimum_version_minor);

extern a_const_char* ifc_token_name_of(a_token_kind token_kind);

extern a_string_view get_partition_name_from_kind(
                                              an_ifc_partition_kind part_kind);

extern void get_bytes(an_ifc_module_file *file,
                      void               *entity,
                      size_t             length,
                      a_boolean          header_bytes);

extern void init_byte_buffer(an_ifc_module_file *file,
                             size_t             offset,
                             ARG_UNUSED size_t  length);

#if USE_MMAP_FOR_MEMORY_REGIONS

inline unsigned char* get_byte_buffer(an_ifc_module_file *file)
/*
Return the byte buffer position that should be read from for the given
read-only memory mapped IFC module file.
*/
{
  /* The byte buffer can only be retrieved in read contexts. */
  return file->get_read_state().byte_buffer;
}  /* get_byte_buffer */

#endif /* USE_MMAP_FOR_MEMORY_REGIONS */


#if ASSUME_LITTLE_ENDIAN_IFC_MODULES

constexpr inline a_boolean has_matching_endianness(
                                           ARG_UNUSED an_ifc_module_file *file)
/*
In modes where little endian modules are assumed, simply return TRUE
unconditionally.
*/
{
  return TRUE;
}  /* has_matching_endianness */

#else /* !ASSUME_LITTLE_ENDIAN_IFC_MODULES */

extern a_boolean has_matching_endianness(an_ifc_module_file *file);

#endif /* ASSUME_LITTLE_ENDIAN_IFC_MODULES */

extern a_boolean check_module(const an_ifc_module_reference &ref);

extern an_ifc_module_file* get_module(const an_ifc_module_reference &ref);

/*
An index type representing an index to a partition element for an associated
module and partition kind.
*/
struct an_ifc_partition_kind_index : public an_ifc_module_entry {
  an_ifc_partition_kind_index()
    : an_ifc_module_entry(NULL), partition_kind(ifc_pk_none),
      value(0)
    {}
  an_ifc_partition_kind_index(an_ifc_module_file    *file_val,
                              an_ifc_partition_kind partition_kind_val,
                              an_ifc_index_type     value_val)
    : an_ifc_module_entry(file_val), partition_kind(partition_kind_val),
      value(value_val)
    {}

  inline a_boolean operator==(const an_ifc_partition_kind_index &other) const;
  a_boolean operator!=(const an_ifc_partition_kind_index &other) const
    { return !(*this == other); }

  an_ifc_partition_kind
                partition_kind;
                        /* The associated partition kind value for this
                           index. */
  an_ifc_index_type
                value;  /* The index value into the associated partition of
                           "sort" for this index.  Represented as the largest
                           common underlying type for all partition kinds. */
};  /* an_ifc_partition_kind_index */


a_boolean an_ifc_partition_kind_index::operator==(
                                      const an_ifc_partition_kind_index &other)
                                                                          const
/*
Return TRUE if two an_ifc_partition_kind_index structures refer to the same
partition element in the same file; otherwise, return FALSE.
*/
{
  a_boolean result = TRUE;

  if (this->file != other.file) {
    result = FALSE;
  } else if (this->partition_kind != other.partition_kind) {
    result = FALSE;
  } else if (this->value != other.value) {
    result = FALSE;
  }  /* if */
  return result;
}  /* an_ifc_partition_kind_index::operator== */


inline a_boolean is_null_index(an_ifc_partition_kind_index idx)
/*
Given an IFC partition kind index, return TRUE if the given index is considered
a null index; otherwise, return FALSE.
*/
{
  return idx.partition_kind == ifc_pk_none;
}  /* is_null_index */


template<typename an_ifc_Index_type>
extern an_ifc_partition_kind get_partition_kind(an_ifc_Index_type idx);

template<typename an_ifc_Index_type>
extern an_ifc_index_type get_partition_index(an_ifc_Index_type idx);

template<typename an_ifc_Index_type>
extern Opt<size_t> get_partition_offset(an_ifc_Index_type idx);

/*
An internal representation of an IFC partition metadata.
*/
struct an_ifc_partition_metadata {
  a_const_char  *name;  /* The name of the partition in the IFC file. */
  size_t        offset; /* An offset from the beginning of the file to the
                           start of the partition. */
  uint32_t      size;   /* The number of bytes in the partition. */
  uint32_t      entry_size;
                        /* The size of an entry in the partition. */
  uint32_t      *format_validated;
                        /* An array of bits for checking IFC format validation.

                           The lower 16 bits of each "block" (uint32_t) are
                           used to represented whether validation was performed
                           (1 is TRUE, 0 is FALSE).  The higher 16 bits are
                           used to represent whether the validated element was
                           invalid (1 is TRUE, 0 is FALSE).

                           This approach is taken to optimize both for space
                           (as only two bits are used for each element) and
                           memory locality (as the validated and invalid flags
                           are always contained within the same 32-bit
                           integer).  Invalid is represented as the TRUE state
                           to reduce the number of writes in the "happy
                           path." */
};  /* an_ifc_partition_metadata */

extern an_ifc_partition_metadata* get_partition_metadata(
                                            an_ifc_input_state    *input_state,
                                            an_ifc_partition_kind part_kind);

template<typename an_ifc_Index_type>
extern an_ifc_partition_metadata* get_partition_metadata(
                                                        an_ifc_Index_type idx);

/*
An enum representing the kind of validation trace.
*/
enum an_ifc_validation_trace_kind {
  ifc_vtk_field,
  ifc_vtk_partition
};

/*
A structure used to track the "path" taken during validation of an IFC node.
*/
struct an_ifc_validation_trace {
  an_ifc_validation_trace(a_const_char                  *field_name_val,
                          size_t                        offset_val,
                          const an_ifc_validation_trace *parent_val)
    : trace_kind(ifc_vtk_field), parent(parent_val),
      field_info{field_name_val, offset_val}
    {}

  an_ifc_validation_trace(an_ifc_module_file            *file_val,
                          an_ifc_partition_kind         partition_kind_val,
                          an_ifc_index_type             partition_idx,
                          const an_ifc_validation_trace *parent_val)
    : trace_kind(ifc_vtk_partition), parent(parent_val),
      partition_info{file_val, partition_kind_val, partition_idx}
    {}

  an_ifc_validation_trace_kind
                trace_kind;
                        /* The variant describing this trace. */
  const an_ifc_validation_trace
                *parent;
                        /* The parent of this trace or NULL if none. */

  /*
  The data structure used to represent tracing information when trace_kind ==
  ifc_vtk_field.
  */
  struct field_info_trace_data {
    a_const_char
                *name;  /* The field name associated with this trace. */
    size_t      offset; /* The relative offset of the field from the node
                           start associated with this trace. */
  };  /* field_info_trace_data */

#ifdef UNION_AS_STRUCT
/* FIXME: Workaround for union-as-struct build issue. */
#undef union
#endif /* ifdef UNION_AS_STRUCT */
  union {
    /* When trace_kind == ifc_vtk_field. */
    field_info_trace_data
                field_info;
                        /* The associated information about the traced
                           field. */
    /* When trace_kind == ifc_vtk_partition. */
    an_ifc_partition_kind_index
                partition_info;
                        /* The associated information about the traced
                           partition element. */
#ifdef UNION_AS_STRUCT
#define union struct
#endif /* ifdef UNION_AS_STRUCT */
  };
};  /* an_ifc_validation_trace */

extern void unknown_partition_conversion(
                                      an_ifc_module_file            *file,
                                      const char                    *sort_name,
                                      an_ifc_index_type             index,
                                      const an_ifc_validation_trace *trace);

extern void invalid_sort(an_ifc_module_file            *file,
                         const an_ifc_validation_trace *trace);

extern void invalid_partition(an_ifc_module_file            *file,
                              const an_ifc_validation_trace *trace);

extern a_boolean validate_element_exists(
                                  an_ifc_module_file            *file,
                                  an_ifc_partition_kind         partition_kind,
                                  an_ifc_index_type             index,
                                  const an_ifc_validation_trace *trace);

template<typename an_ifc_Node_type>
extern an_ifc_Node_type construct_node_from_module(an_ifc_module_file *file);

template<typename an_ifc_Node_type, typename an_ifc_Index_type>
extern void construct_node(Opt<an_ifc_Node_type> *result,
                           an_ifc_Index_type     idx);

template<typename an_ifc_Node_type, typename an_ifc_Index_type>
extern void construct_node_prechecked(an_ifc_Node_type  *result,
                                      an_ifc_Index_type idx);

template<typename an_ifc_Node_type, typename an_ifc_Index_type>
extern void construct_node_unchecked(an_ifc_Node_type  *result,
                                     an_ifc_Index_type idx);

#if DEBUG
/* GCOVR_EXCL_START */

extern void db_decl_cache(an_ifc_decl_index decl_idx);
extern void db_print_indent(unsigned amount);

/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

extern void ifc_modules_read_one_time_init();

extern void ifc_modules_read_trans_unit_delayed_init();

extern void ifc_modules_read_trans_unit_init();

extern void ifc_modules_read_trans_unit_wrapup();

extern void ifc_modules_write_one_time_init();

#if MAKE_FRONT_END_CALLABLE

extern void ifc_modules_read_cleanup();

extern void ifc_modules_write_cleanup();

#endif /* MAKE_FRONT_END_CALLABLE */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* !STANDALONE_UTILITY_PROGRAM */

#endif /* ifndef IFC_MODULES_INTERNAL_H */

