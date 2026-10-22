/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

ms_attrib.c -- Microsoft attribute processing.

*/

/* Header files common to all files. */
#include "fe_common.h"
/* Header files used by files involved in declaration processing. */
#include "decl_hdrs.h"
#include "expr.h"


#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

#if MICROSOFT_EXTENSIONS_ALLOWED

#include "ms_attrib.h"

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE


#if DEBUG
/* GCOVR_EXCL_START */
/*
Counts of tables allocated, to track total use of memory.
*/
STATIC_THREAD unsigned long
		num_ms_attribute_kind_descrs_allocated,
		num_ms_attribute_params_allocated;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

STATIC_THREAD a_text_buffer_ptr
		ms_attr_buffer;
			/* A text buffer used to create temporary copies of
			   attribute argument values. */

STATIC_THREAD a_boolean
		scan_misc_attributes_as_unrecognized;
			/* TRUE if most recognized attributes should be scanned
			   as "unrecognized" attributes.  This means that the
			   only a string representation is recorded.  The
			   individual arguments are not scanned or checked.
			   No checking is done when the attributes are
			   applied.  This affects msak_misc attributes (so
			   some attributes, such as uuid, will still be
			   processed normally). */

STATIC_THREAD an_ms_attribute_kind_descr_ptr
		unrecognized_attribute;
			/* A special attribute kind entry that represents
			   an unrecognized attribute.  This is used even when
			   unrecognized attributes are not accepted, for
			   error recovery purposes. */

STATIC_THREAD an_ms_attribute_kind_descr_ptr
		custom_attribute;
			/* A special attribute kind entry that represents
			   a custom attribute. */

STATIC_THREAD an_ms_attribute_kind_descr_ptr
		curr_attribute_descr;
			/* Pointer to an attribute description that is in the
			   process of being defined.  This is set by
			   make_attribute_description and is used by
			   add_attribute_parameter. */

STATIC_THREAD a_token_cache_ptr
		attribute_cache;
			/* Token cache containing the tokens of the current
			   attribute block. */

#define ATTRIBUTE_LOOKUP_TABLE_SIZE 61
			/* The number of buckets in the template lookup table.
			   This number should be prime. */

STATIC_THREAD an_ms_attribute_kind_descr_ptr
		attribute_lookup_table[ATTRIBUTE_LOOKUP_TABLE_SIZE];
			/* Table used to determine the attribute kind for a
			   given attribute name.  Each element of the
			   array points to a list of entries for attributes
			   that hash to a given group. */

#define HASH_FACTOR ((unsigned int)73)
			/* The multiplier used in the hash algorithm that
			   generates an index in the hash table from a name
                           string.
			   Do not change without investigating the
			   hash table performance that results.  Prime
			   values are likely to work better than
			   non-prime values. */

static unsigned long hash_attribute_name(a_const_char	*name,
					 sizeof_t	length)
/*
Generate a hash value for the specified name.
*/
{
  unsigned long	hash_value = 0;
  unsigned char	*ptr;

  /* Hash the name.  This involves taking the name's first 3, last 3, and
     middle 3 characters.  Of course, if the identifier has 9 or fewer
     characters, take the entire identifier. */
  ptr = (unsigned char *)name;
  if (length > 9) {
    hash_value = *ptr++;
    hash_value = (hash_value * HASH_FACTOR) + *ptr++;
    hash_value = (hash_value * HASH_FACTOR) + *ptr;
    ptr = (unsigned char *)name + (length >> 1) - 1;
    hash_value = (hash_value * HASH_FACTOR) + *ptr++;
    hash_value = (hash_value * HASH_FACTOR) + *ptr++;
    hash_value = (hash_value * HASH_FACTOR) + *ptr;
    ptr = (unsigned char *)name + length - 3;
    hash_value = (hash_value * HASH_FACTOR) + *ptr++;
    hash_value = (hash_value * HASH_FACTOR) + *ptr++;
    hash_value = (hash_value * HASH_FACTOR) + *ptr;
  } else {
    unsigned int i;
    for (i = 0; i < length; i++) {
      hash_value = (hash_value * HASH_FACTOR) + *ptr++;
    }  /* for */
  }  /* if */
  return hash_value;
}  /* hash_attribute_name */


static void add_attribute_lookup_table_entry(
					an_ms_attribute_kind_descr_ptr	msakdp,
					a_const_char			*name)
/*
Add "msakdp" to the attribute lookup table.  "name" is the name of the
attribute.
*/
{
  size_t   bucket;
  sizeof_t length = strlen(name);

  bucket = hash_attribute_name(name, length) % ATTRIBUTE_LOOKUP_TABLE_SIZE;
  msakdp->next = attribute_lookup_table[bucket];
  msakdp->name = copy_string_to_region(file_scope_region_number, name);
  msakdp->name_length = length;
  attribute_lookup_table[bucket] = msakdp;
}  /* add_attribute_lookup_table_entry  */


static an_ms_attribute_kind_descr_ptr find_attribute_kind(a_const_char	*name,
							  sizeof_t	length)
/*
Look up an attribute named "name" (with a length of "length") in the attribute
lookup table.  If found, return a pointer to the kind description entry,
otherwise return NULL.
*/
{
  size_t                         bucket;
  an_ms_attribute_kind_descr_ptr msakdp;

  bucket = hash_attribute_name(name, length) % ATTRIBUTE_LOOKUP_TABLE_SIZE;
  for (msakdp = attribute_lookup_table[bucket];
       msakdp != NULL; msakdp = msakdp->next) {
    if (msakdp->name_length == length &&
        strncmp(msakdp->name, name, size_t_arg(length)) == 0) {
      break;
    }  /* if */
  }  /* for */
  return msakdp;
}  /* find_attribute_kind */


static an_ms_attribute_kind_descr_ptr alloc_ms_attribute_kind_descr(void)
/*
Allocate an attribute kind description entry, initialize its fields, and
return a pointer to the entry.
*/
{
  an_ms_attribute_kind_descr_ptr msakdp;

  msakdp = alloc_fe_of_type(an_ms_attribute_kind_descr);
#if DEBUG
/* GCOVR_EXCL_START */
  num_ms_attribute_kind_descrs_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  msakdp->kind = (an_ms_attribute_kind)msak_none;
  msakdp->target = msat_invalid;
  msakdp->name = NULL;
  msakdp->name_length = 0;
  msakdp->initialization_style_arg_allowed = FALSE;
  msakdp->next = NULL;
  msakdp->num_params = 0;
  msakdp->parameters = NULL;
  msakdp->parameters_tail = NULL;
  return msakdp;
}  /* alloc_ms_attribute_kind_descr */


static void make_attribute_description(an_ms_attribute_kind	kind,
				       a_const_char		*name,
				       an_ms_attribute_target	target)
/*
Create an attribute kind description entry for the specified attribute
kind, with a name of "name".  If the attribute has a name, enter it into
the attribute lookup table.  This routine sets a static variable to the
entry most recently added.  This is used by add_attribute_parameter, etc.
when specifying the parameters associated with an attribute.
*/
{
  an_ms_attribute_kind_descr_ptr	msakdp;

  msakdp = alloc_ms_attribute_kind_descr();
  if (scan_misc_attributes_as_unrecognized &&
      kind == (an_ms_attribute_kind)msak_misc) {
    /* When scanning miscellaneous attributes as unrecognized, override
       the specified kind and use the unrecognized kind instead. */
    msakdp->kind = (an_ms_attribute_kind)msak_unrecognized;
    msakdp->target = msat_any;
  } else {
    msakdp->kind = kind;
    msakdp->target = target;
  }  /* if */
  if (name != NULL) {
    /* If this is not an unnamed attribute, create a lookup table entry. */
    add_attribute_lookup_table_entry(msakdp, name);
  }  /* if */
  curr_attribute_descr = msakdp;
}  /* make_attribute_description */

#if RECOGNIZE_MICROSOFT_ATTRIBUTES || INCLUDE_EDG_TEST_ATTRIBUTES

static an_ms_attribute_param_ptr alloc_ms_attribute_param(void)
/*
Allocate an attribute parameter entry, initialize its fields, and
return a pointer to the entry.
*/
{
  an_ms_attribute_param_ptr msapp;

  msapp = alloc_fe_of_type(an_ms_attribute_param);
#if DEBUG
/* GCOVR_EXCL_START */
  num_ms_attribute_params_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  msapp->next = NULL;
  msapp->name = NULL;
  msapp->values = NULL;
  msapp->kind = msaak_none;
  msapp->is_unnamed = FALSE;
  return msapp;
}  /* alloc_ms_attribute_param */


static void add_attribute_parameter(an_ms_attribute_arg_kind	kind,
				    a_const_char		*name,
				    a_boolean			is_unnamed,
				    a_const_char		*values)
/*
Add the specified parameter to the list of parameters accepted by the
attribute most recently added by make_attribute_description.

"kind" indicates the type of parameter (string, integer, etc.), "name"
is the parameter name.  A name is specified even if "is_unnamed" is TRUE
so that a name is available, if needed, for diagnostic purposes.  For
msaak_enumeration parameters, "values" is a comma-separated list of the
acceptable values.  The list must not contain any null names.  The value
names must be in lower case, as the arguments will be converted to lower
case.
*/
{
  an_ms_attribute_param_ptr	msapp;

  msapp = alloc_ms_attribute_param();
  msapp->kind = kind;
  msapp->name = copy_string_to_region(file_scope_region_number, name);
  msapp->is_unnamed = is_unnamed;
  /* Link this parameter entry into the list of parameters. */
  if (curr_attribute_descr->parameters == NULL) {
    curr_attribute_descr->parameters = msapp;
  } else {
    curr_attribute_descr->parameters_tail->next = msapp;
  }  /* if */
  curr_attribute_descr->parameters_tail = msapp;
  if (kind == msaak_enumeration && values != NULL) {
    /* Convert the specified list of values from a comma-separated list into
       an array of acceptable values. */
    size_t       num_elements = 1;
    size_t       element;
    a_const_char *ptr;
    char         **list;
    /* Find the number of elements. */
    for (ptr = values; *ptr != '\0'; ptr++) {
      if (*ptr == ',') num_elements++;
    }  /* for */
    /* Allocate an array for the list.  An extra element is allocated for a
       NULL terminating element. */
    list = (char**)alloc_fe((sizeof_t)(sizeof(char*) * (num_elements + 1)));
    /* Set the terminating element to NULL. */
    list[num_elements] = NULL;
    for (element = 0, ptr = values; element < num_elements; ++element) {
      /* Find the end of this element. */
      a_const_char *end = strchr(ptr, ',');
      sizeof_t     length;
      char         *entry;
      /* If this is the last element, find the end of the string. */
      if (end == NULL) end = &ptr[strlen(ptr)];
      length = (sizeof_t)(end - ptr);
      check_assertion_str2(length > 0, "add_attribute_parameter:",
                           "empty enumeration names not allowed");
      entry = copy_string_of_length_to_region(FRONT_END_REGION_NUMBER,
                                              ptr, length);
      list[element] = entry;
      /* Advance to the next entry. */
      ptr = end + 1;
    }  /* for */
    msapp->values = list;
  }  /* if */
}  /* add_attribute_parameter */

#endif /* RECOGNIZE_MICROSOFT_ATTRIBUTES || INCLUDE_EDG_TEST_ATTRIBUTES */
#if RECOGNIZE_MICROSOFT_ATTRIBUTES

static void set_initialization_style_arg_allowed(void)
/*
Set the initialization-style argument allowed flag for the current attribute.
*/
{
  curr_attribute_descr->initialization_style_arg_allowed = TRUE;
}  /* set_initialization_style_arg_allowed */

#endif /* RECOGNIZE_MICROSOFT_ATTRIBUTES */

static void init_attribute_kinds(void)
/*
Create the data structures used to describe the kinds of attributes that
are accepted.
*/
{
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "<unrecognized>", msat_any);
  /* Save a pointer to the special "unrecognized" attribute kind. */
  unrecognized_attribute = curr_attribute_descr;
#if RECOGNIZE_MICROSOFT_ATTRIBUTES
  make_attribute_description((an_ms_attribute_kind)msak_custom,
                             "<custom>", msat_invalid);
  /* Save a pointer to the special "custom" attribute kind. */
  custom_attribute = curr_attribute_descr;
  /* [aggregatable] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "aggregatable", (msat_class | msat_struct));
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_enumeration,
                          "value", /*is_unnamed=*/FALSE,
                          "never,allowed,always");
  /* [aggregates]
     The Microsoft documentation describes a second parameter named
     "variable_name", but this does not actually seem to be accepted
     by the Microsoft compiler (7.1). */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "aggregates", (msat_class | msat_struct));
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_uuid,
                          "clsid", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [appobject] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "appobject", (msat_class | msat_struct));
  /* [as_expression] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "as_expression", msat_any);
  /* [as_string] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "as_string", msat_any);
  /* [async_uuid] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "async_uuid", msat_interface);
  set_initialization_style_arg_allowed();
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_uuid,
                          "uuid", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [attribute] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "attribute", msat_any);
  /* [bindable] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "bindable", msat_method);
  /* [call_as] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "call_as", (msat_method | msat_routine));
  set_initialization_style_arg_allowed();
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "function", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [case] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "case", msat_field);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "value", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [coclass] */
  make_attribute_description((an_ms_attribute_kind)msak_coclass,
                             "coclass", (msat_class | msat_struct));
  /* [com_interface_entry] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "com_interface_entry", 
                             (msat_class | msat_struct));
  set_initialization_style_arg_allowed();
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "entry",
                          /*is_unnamed=*/TRUE,
                          (char*)NULL);
  /* [control] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "control", (msat_class | msat_struct));
  /* [cpp_quote] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "cpp_quote", msat_any);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "statement", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [custom] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "custom", msat_any);
  /* [db_accessor] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "db_accessor", msat_any);
  /* [db_column] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "db_column", msat_any);
  /* [db_command] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "db_command", msat_any);
  /* [db_param] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "db_param", msat_any);
  /* [db_source] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "db_source", msat_any);
  /* [db_table] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "db_table", msat_any);
  /* [default] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "default", 
                             (msat_class | msat_struct | msat_field));
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "interface1", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "interface2", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [defaultbind] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "defaultbind", msat_method);
  /* [defaultcollelem] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "defaultcollelem", msat_method);
  /* [default_value] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "default_value", msat_any);
  /* [defaultvalue] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "defaultvalue", msat_parameter);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "value", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [defaultvtable] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "defaultvtable", msat_any);
  /* [dispinterface] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "dispinterface", msat_interface);
  /* [displaybind] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "displaybind", msat_method);
  /* [dual] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "dual", msat_interface);
  /* [emitidl] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "emitidl", msat_none);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_enumeration,
                          "mode",
                          /*is_unnamed=*/TRUE,
                          "false,true,restricted,forced");
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                          "defaultimports",
                          /*is_unnamed=*/FALSE,
                          (char*)NULL);
  /* [entry] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "entry", msat_any);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "id", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [event_receiver] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "event_receiver", (msat_class | msat_struct));
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_enumeration,
                          "type", /*is_unnamed=*/TRUE,
                          "native,com,managed");
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                          "layout_dependent", /*is_unnamed=*/FALSE,
                          (char*)NULL);
  /* [event_source] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "event_source", (msat_class | msat_struct));
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_enumeration,
                          "type", /*is_unnamed=*/TRUE,
                          "native,com,managed");
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_enumeration,
                          "optimize", /*is_unnamed=*/FALSE,
                          "speed,size");
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                          "decorate", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [export] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "export", 
                             (msat_interface | msat_struct | msat_union |
                              msat_enum | msat_typedef));
  /* [first_is] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "first_is", 
                             (msat_field | msat_method | msat_parameter));
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "value", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [helpcontext] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "helpcontext", 
                             (msat_interface | msat_class | msat_typedef |
                              msat_method));
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "id", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [helpfile] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "helpfile", 
                             (msat_interface | msat_class | msat_typedef |
                              msat_method));
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "filename", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [help_string] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "help_string", msat_any);
  /* [helpstring] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "helpstring", 
                             (msat_interface | msat_class | msat_typedef |
                              msat_method));
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "string", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [helpstringcontext] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "helpstringcontext", 
                             (msat_interface | msat_class | msat_typedef |
                              msat_method));
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "contextID", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [helpstringdll] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "helpstringdll", 
                             (msat_interface | msat_class | msat_typedef |
                              msat_method));
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "string", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [hidden] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "hidden", 
                             (msat_method | msat_interface | msat_class |
                              msat_struct));
  /* [hook] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "hook", msat_any);
  /* [id] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "id", msat_method);
  set_initialization_style_arg_allowed();
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "id", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [idl_module] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "idl_module", msat_any);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "name", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "dllname", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_uuid,
                          "uuid", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "helpstring", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "helpstringcontext", /*is_unnamed=*/FALSE,
                          (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "helpcontext", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "helpfile", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                         "hidden", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                         "restricted", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [idl_quote] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "idl_quote", msat_any);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "text", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [iid_is] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "iid_is", 
                             (msat_field | msat_method | msat_parameter));
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "value", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [immediatebind] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "immediatebind", msat_method);
  /* [implements] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "implements", (msat_class | msat_struct));
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "interfaces", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "dispinterfaces", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [implements_category] */
  /* This is documented as taking a UUID, but this does not seem to be
     the case. */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "implements_category", 
                             (msat_class | msat_struct));
  set_initialization_style_arg_allowed();
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "implements_category", /*is_unnamed=*/FALSE,
                          (char*)NULL);
  /* [import] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "import", msat_any);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "idl_file", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [importidl] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "importidl", msat_any);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "idl_file", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [importlib] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "importlib", msat_any);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "tlb_file", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [in] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "in", msat_parameter);
  /* [include] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "include", msat_any);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "header_file", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [includelib] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "includelib", msat_any);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "name.idl", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [last_is] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "last_is", 
                             (msat_field | msat_method | msat_parameter));
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "value", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [lcid] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "lcid", msat_parameter);
  /* [length_is] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "length_is", 
                             (msat_field | msat_method | msat_parameter));
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "value", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [library_block] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "library_block", msat_any);
  /* [licensed] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "licensed", (msat_class | msat_struct));
  /* [local] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "local", (msat_method | msat_interface));
  /* [max_is] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "max_is", 
                             (msat_field | msat_method | msat_parameter));
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "value", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [module] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "module", (msat_none | msat_class));
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_enumeration,
                          "type", /*is_unnamed=*/FALSE, "dll,exe,service");
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "name", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_uuid,
                          "uuid", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "version", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "lcid", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                          "control", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "helpstring", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "helpstringdll", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "helpfile", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "helpcontext", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "helpstringcontext", /*is_unnamed=*/FALSE,
                          (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                         "hidden", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                         "restricted", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                         "custom", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                         "resource_name", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [ms_union] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "ms_union", 
                             (msat_class | msat_struct | msat_interface));
  /* [multi_value] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "multi_value", msat_any);
  /* [no_injected_text] */
  make_attribute_description((an_ms_attribute_kind)msak_no_injected_text,
                             "no_injected_text", msat_any);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                          "value", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [nonbrowsable] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "nonbrowsable", msat_method);
  /* [noncreatable] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "noncreatable", (msat_class | msat_struct));
  /* [nonextensible] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "nonextensible", msat_interface);
  /* [notify_atlprov] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "notify_atlprov", msat_any);
  /* [odl] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "odl", msat_interface);
  /* [object] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "object", msat_interface);
  /* [oleautomation] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "oleautomation", msat_interface);
  /* [optional] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "optional", msat_any);
  /* [out] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "out", msat_parameter);
  /* [perfmon] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "perfmon", (msat_class | msat_struct));
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "name", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                          "register", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [perf_counter] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "perf_counter", msat_field);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "namestring", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "helpstring", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "name_res", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "help_res", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "countertype_string", /*is_unnamed=*/FALSE,
                          (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "defscale", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                          "default_counter", /*is_unnamed=*/FALSE,
                          (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "detail", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [perf_object] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "perf_object", (msat_class | msat_struct));
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "name_res", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "help_res", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "namestring", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "helpstring", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "detail", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                          "no_instances", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "class", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "maxinstnamelen", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [pointer_default] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "pointer_default", msat_interface);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "value", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [pragma] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "pragma", msat_any);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "pragma_statement", /*is_unnamed=*/TRUE,
                          (char*)NULL);
  /* [process_early] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "process_early", msat_any);
  /* [progid] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "progid", (msat_class | msat_struct));
  set_initialization_style_arg_allowed();
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "name", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [ptr] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "ptr", 
                             (msat_parameter | msat_method | msat_routine |
                              msat_typedef));
  /* [propget] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "propget", (msat_method | msat_routine));
  /* [propput] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "propput", (msat_method | msat_routine));
  /* [propputref] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "propputref", (msat_method | msat_routine));
  /* [provider] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "provider", msat_none);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "name", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_uuid,
                          "uuid", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [public] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "public", msat_typedef);
  /* [range] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "range", (msat_method | msat_parameter));
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "low", /*is_unnamed=*/TRUE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "high", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [rdx] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "rdx", msat_field);
  /* [readonly] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "readonly", msat_method);
  /* [ref] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "ref", 
                             (msat_parameter | msat_method | msat_routine |
                              msat_typedef));
  /* [registration_script] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "registration_script", 
                             (msat_class | msat_struct));
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "script", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [repeatable] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "repeatable", msat_any);
  /* [requestedit] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "requestedit", msat_method);
  /* [request_handler] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "request_handler", (msat_class | msat_struct));
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "name", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "sdl", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [requires_category] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "requires_category", (msat_class | msat_struct));
  set_initialization_style_arg_allowed();
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "requires_category", /*is_unnamed=*/FALSE,
                          (char*)NULL);
  /* [requires_value] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "requires_value", msat_any);
  /* [restricted] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "restricted", 
                             (msat_method | msat_interface | msat_class |
                              msat_struct));
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "interfaces", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [retval] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "retval", msat_parameter);
  /* [satype] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "satype", msat_parameter);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "data_type", /*is_unnamed=*/FALSE, (char*)NULL);
  /* Source Annotation attributes. */
  /* [FormatString] */
  if (!C_mode()) {
    make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                               "FormatString", msat_any);
  }  /* if */
  /* [SA_FormatString] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "SA_FormatString", msat_any);
  /* [InvalidCheck] */
  if (!C_mode()) {
    make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                               "InvalidCheck", msat_any);
  }  /* if */
  /* [SA_InvalidCheck] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "SA_InvalidCheck", msat_any);
  /* [Post] */
  if (!C_mode()) {
    make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                               "Post", msat_any);
  }  /* if */
  /* [SA_Post] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "SA_Post", msat_any);
  /* [Pre] */
  if (!C_mode()) {
    make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                               "Pre", msat_any);
  }  /* if */
  /* [SA_Pre] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "SA_Pre", msat_any);
  /* [PostBound] */
  if (!C_mode()) {
    make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                               "PostBound", msat_any);
  }  /* if */
  /* [SA_PostBound] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "SA_PostBound", msat_any);
  /* [PostRange] */
  if (!C_mode()) {
    make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                               "PostRange", msat_any);
  }  /* if */
  /* [SA_PostRange] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "SA_PostRange", msat_any);
  /* [PreBound] */
  if (!C_mode()) {
    make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                               "PreBound", msat_any);
  }  /* if */
  /* [SA_PreBound] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "SA_PreBound", msat_any);
  /* [SA_PreRange] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "SA_PreRange", msat_any);
  /* [PreRange] */
  if (!C_mode()) {
    make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                               "PreRange", msat_any);
  }  /* if */
  /* [Success] */
  if (!C_mode()) {
    make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                               "Success", msat_any);
  }  /* if */
  /* [SA_Success] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "SA_Success", msat_any);
  /* [source_annotation_attribute] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "source_annotation_attribute", msat_any);
  /* [size_is] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "size_is", 
                             (msat_field | msat_method | msat_parameter));
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "value", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [soap_handler] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "soap_handler", (msat_class | msat_struct));
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "name", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "namespace", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "protocol", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "style", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "use", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [soap_header] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "soap_header", msat_method);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "value", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                          "required", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                          "in", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                          "out", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [soap_method] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "soap_method",
                             msat_method);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "name", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [soap_namespace] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "soap_namespace",
                             msat_any);
  /* [source] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "source", 
                             (msat_class | msat_struct | msat_interface));
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "interfaces", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [string] */
  /* The Microsoft documentation does not have the correct target. */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "string",
                             msat_any);
  /* [support_error_info] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "support_error_info",
                             msat_class);
  /* The documentation describes this parameter as a UUID, but it appears
     to actually be a string. */
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "error_interface", /*is_unnamed=*/FALSE,
                          (char*)NULL);
  /* [switch_type] */
  /* The Microsoft documentation does not describe the field argument and
     does not have the correct target. */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "switch_type",
                             msat_union);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "field", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [switch_is] */
  /* The Microsoft documentation does not describe the type argument and
     does not have the correct target. */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "switch_is",
                             msat_union);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "type", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [synchronize] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "synchronize", 
                             (msat_method | msat_routine));
  /* [tag_name] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "tag_name", msat_method);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "name", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "parse_func", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [threading] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "threading", (msat_class | msat_struct));
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "model", /*is_unnamed=*/FALSE,
                          "apartment,neutral,single,free,both");
  /* [transmit_as] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "transmit_as", msat_typedef);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "type", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [uidefault] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "uidefault", msat_method);
  /* [unhook] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "unhook", msat_any);
  /* [unique] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "unique", 
                             (msat_parameter | msat_method | msat_routine |
                              msat_typedef));
  /* [usage] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "usage", msat_any);
  /* [usesgetlasterror] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "usesgetlasterror", msat_any);
  /* [uuid] */
  if (!C_mode()) {
    make_attribute_description((an_ms_attribute_kind)msak_uuid,
                               "uuid", 
                               (msat_class | msat_struct | msat_interface));
    set_initialization_style_arg_allowed();
    add_attribute_parameter((an_ms_attribute_arg_kind)msaak_uuid,
                            "uuid", /*is_unnamed=*/FALSE, (char*)NULL);
  }  /* if */
  /* [v1_alttype] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "v1_alttype", msat_any);
  /* [v1_early] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "v1_early", msat_any);
  /* [v1_enum] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "v1_enum", msat_enum);
  /* [v1_name] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "v1_name", msat_any);
  /* [vararg] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "vararg", msat_method);
  /* [version] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "version", (msat_class | msat_struct));
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "version", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [vi_progid] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "vi_progid", (msat_class | msat_struct));
  set_initialization_style_arg_allowed();
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "name", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [wire_marshal] */
  /* The Microsoft documentation does not describe the type argument. */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
                             "wire_marshal", msat_typedef);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "type", /*is_unnamed=*/FALSE, (char*)NULL);
#endif /* RECOGNIZE_MICROSOFT_ATTRIBUTES */
#if INCLUDE_EDG_TEST_ATTRIBUTES
  /* These are special attributes included for testing purposes. */
  /* [edg_test_1] */
  make_attribute_description((an_ms_attribute_kind)msak_edg_test,
                             "edg_test_1", msat_none);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "arg1", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                          "arg2", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "arg3", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_uuid,
                          "arg4", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_enumeration,
                          "arg5", /*is_unnamed=*/FALSE, "a,b,c,aa,bb,cc");
  /* [edg_test_2] */
  make_attribute_description((an_ms_attribute_kind)msak_edg_test,
                             "edg_test_2", msat_none);
  /* [edg_test_3] */
  make_attribute_description((an_ms_attribute_kind)msak_edg_test,
                             "edg_test_3", msat_any);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "arg1", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                          "arg2", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "arg3", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_uuid,
                          "arg4", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_enumeration,
                          "arg5", /*is_unnamed=*/FALSE, "a,b,c,aa,bb,cc");
#endif /* INCLUDE_EDG_TEST_ATTRIBUTES */
}  /* init_attribute_kinds */


static void cache_attribute_block(void)
/*
We are at the opening "[" of a Microsoft attribute block.  The block contains
one or more attributes and is terminated by a closing "]".  Create a token
cache containing all of the tokens of the attribute block.
*/
{
  a_token_set_array	stop_tokens;
  a_boolean		save_expand_macros;
  a_boolean		save_do_string_literal_concatenation;
  a_boolean		save_fetch_pp_tokens;
  a_boolean		save_in_microsoft_attribute;
  a_boolean		save_suppress_keyword_recognition;

  /* Save the current value of the lexical scanning mode flags. */
  save_expand_macros = expand_macros;
  save_do_string_literal_concatenation = do_string_literal_concatenation;
  save_fetch_pp_tokens = fetch_pp_tokens;
  save_suppress_keyword_recognition = suppress_keyword_recognition;
  save_in_microsoft_attribute = in_microsoft_attribute;
  /* Set the values required for attribute scanning. */
  expand_macros = TRUE;
  do_string_literal_concatenation = TRUE;
  fetch_pp_tokens = FALSE;
  in_microsoft_attribute = TRUE;
  attribute_cache->clear();
  /* Cache the current token and advance past it. */
  cache_curr_token(attribute_cache);
  (void)get_token();
  /* Initialize a local stop token set. */
  clear_token_set_array(stop_tokens);
  /* Cache all of the tokens up to the closing "]".  Also stop on a right
     brace for error recovery purposes. */
  incr_token_set_array_element(stop_tokens, tok_rbracket);
  incr_token_set_array_element(stop_tokens, tok_rbrace);
  cache_token_stream(attribute_cache, stop_tokens);
  /* Add an end-of-source token to the end of the token cache to
     assure that we don't scan past the end of the cache in the actual
     scan. */
  terminate_token_cache(attribute_cache);
  rescan_copy_of_cache(attribute_cache);
  /* Restore the previous values. */
  expand_macros = save_expand_macros;
  do_string_literal_concatenation = save_do_string_literal_concatenation;
  fetch_pp_tokens = save_fetch_pp_tokens;
  suppress_keyword_recognition = save_suppress_keyword_recognition;
  in_microsoft_attribute = save_in_microsoft_attribute;
}  /* cache_attribute_block */


static a_type_ptr ms_attribute_type_from_symbol(a_symbol_ptr sym)
/*
If sym is a type symbol that meets the requirements of a C++/CLI custom
attribute type, returns the type; otherwise returns NULL.
*/
{
  a_type_ptr  type = NULL;

  if (sym != NULL && is_type_symbol(sym)) {
    type = type_symbol_type(sym);
    if (!is_template_param_type_symbol(sym) && !is_cli_attribute_type(type)) {
      type = NULL;
    }  /* if */
  }  /* if */
  return type;
}  /* ms_attribute_type_from_symbol */


static an_ms_attribute_kind_descr_ptr look_up_attribute(
                                           a_type_ptr *custom_attribute_type,
                                           a_boolean  *is_attribute_attribute)
/*
Look up the identifier that names the attribute to be processed.  If this is a
custom attribute, return in *custom_attribute_type the type describing this
attribute.  If this is the "attribute(...)" attribute, treat it like the
AttributeUsage custom attribute and set *is_attribute_attribute to TRUE (the
caller needs to know because that also implicitly causes derivation from
System::Attribute, unlike direct use of the AttributeUsage attribute).
*/
{
  an_ms_attribute_kind_descr_ptr  attr_descr = NULL;
  a_boolean                       orig_is_attribute_attribute = FALSE;

  check_assertion(custom_attribute_type != NULL);
  *custom_attribute_type = NULL;
  if (!is_generalized_identifier_start(GID_NO_OPTIONS)) {
    /* An identifier that names the attribute was expected. */
    pos_error(ec_exp_attribute_name, &pos_curr_token);
  } else {
    a_symbol_ptr  sym;
    a_type_ptr    type = NULL, alternate_type = NULL;
    a_boolean     err = FALSE;
    if (cppcli_enabled) {
      /* Check for custom attributes. */
      /* C++/CX custom attributes are not yet supported.  The primary
         blocking issue is that attribute processing needs to be deferred until
         the class definition is complete in order for name lookup to function
         correctly. */
      /* In C++/CLI mode the non-custom "attribute" attribute has two potential
         effects.  First, it causes the class to which it is attached to
         derive from System::Attribute if that is not already specified as a
         base class.  Second, if it is followed by arguments, the effect is as
         if those arguments were passed to the custom attribute
         System::AttributeUsage.  In that case, we simply treat the "attribute"
         attribute as if it were the custom attribute System::AttributeUsage
         (but we also set a flag so we can implicitly inherit from
         System::Attribute if needed; that is handled when processing the class
         definition). */
      if (curr_token_is_identifier_string("attribute")) {
        orig_is_attribute_attribute = TRUE;
      }  /* if */
      if (orig_is_attribute_attribute && next_token() == tok_lparen) {
        sym = cli_symbols[(int)csk_system_attribute_usage_attribute];
        make_locator_for_symbol(sym, &locator_for_curr_id);
      } else {
        sym = coalesce_and_lookup_generalized_identifier(
                                    GID_NO_OPTIONS, ilm_tentative_type, &err);
      }  /* if */
      type = ms_attribute_type_from_symbol(sym);
      if (!err && !locator_for_curr_id.is_template_id &&
          (sym == NULL || !is_template_param_type_symbol(sym))) {
        a_symbol_locator  orig_locator;
        a_symbol_ptr      alternate_sym = NULL;
        a_boolean         alt_name_err = FALSE;
        orig_locator = locator_for_curr_id;
        change_ms_attr_locator_into_alt_name_locator(&locator_for_curr_id);
        alternate_sym = coalesce_and_lookup_generalized_identifier(
                           GID_NO_OPTIONS, ilm_tentative_type, &alt_name_err);
        locator_for_curr_id = orig_locator;
        alternate_type = ms_attribute_type_from_symbol(alternate_sym);
      }  /* if */
    }  /* if */
    if (type == NULL && alternate_type == NULL) {
      if (!locator_for_curr_id.is_qualified_name) {
        a_symbol_header_ptr  sym_hdr = locator_for_curr_id.symbol_header;
        attr_descr = find_attribute_kind(
                                       sym_hdr->identifier,
                                       (sizeof_t)strlen(sym_hdr->identifier));
      }  /* if */
      if (attr_descr == NULL) {
        attr_descr = unrecognized_attribute;
        /* An unknown attribute -- issue a diagnostic. */
        pos_warning(ec_unrecognized_ms_attr, &pos_curr_token);
      }  /* if */
    } else if (type != NULL && alternate_type != NULL) {
      /* Ambiguous attribute -- issue a diagnostic. */
      pos_ty2_error(ec_ambiguous_ms_attribute, &pos_curr_token,
                    type, alternate_type);
    } else {
      attr_descr = custom_attribute;
      *custom_attribute_type = (type != NULL) ? type : alternate_type;
    }  /* if */
    if (attr_descr != NULL) {
      /* Bypass the identifier. */
      (void)get_token();
    }  /* if */
  }  /* if */
  if (attr_descr == NULL) {
    /* An error was encountered; discard the rest of this attribute. */
    flush_tokens();
  }  /* if */
  *is_attribute_attribute = orig_is_attribute_attribute;
  return attr_descr;
}  /* look_up_attribute */


/*
Values for the enumerators in the C++/CLI System::AttributeTarget enum.
*/
#define cliat_assembly         0x0001
			/* Applies to an assembly as a whole. */
#define cliat_module           0x0002
			/* Applies to a module as a whole. */
#define cliat_class            0x0004
			/* Applies to a C++/CLI ref class. */
#define cliat_struct           0x0008
			/* Applies to a C++/CLI value type that is not an
			   enum. */
#define cliat_enum             0x0010
			/* Applies to a C++/CLI enum. */
#define cliat_constructor      0x0020
			/* Applies to a constructor. */
#define cliat_method           0x0040
			/* Applies to a function. */
#define cliat_property         0x0080
			/* Applies to a C++/CLI property. */
#define cliat_field            0x0100
			/* Applies to a data member. */
#define cliat_event            0x0200
			/* Applies to an event. */
#define cliat_interface        0x0400
			/* Applies to a C++/CLI interface class. */
#define cliat_parameter        0x0800
			/* Applies to a parameter. */
#define cliat_delegate         0x1000
			/* Applies to a delegate. */
#define cliat_returnvalue      0x2000
			/* Applies to a method's return value. */
#define cliat_genericparameter 0x4000
			/* Applies to a generic parameter. */
#define cliat_all              0x7FFF

typedef unsigned int a_cli_attribute_target;


static an_ms_attribute_target msat_from_cliat(a_cli_attribute_target cliat)
/*
Translate a C++/CLI attribute target bit set to a general Microsoft attribute
target bit set.
*/
{
  unsigned               i;
  an_ms_attribute_target msat = msat_invalid;
  constexpr struct an_enumerator_map {
    a_cli_attribute_target cliat;
    an_ms_attribute_target msat;
  } enumerator_map[] = {
    { cliat_all,               msat_assembly | msat_module |
                               msat_class | msat_struct | msat_enum |
                               msat_constructor | msat_method |
                               msat_property | msat_field |
                               msat_event | msat_interface |
                               msat_parameter | msat_delegate |
                               msat_returnvalue },
    { cliat_assembly,          msat_assembly },
    { cliat_module,            msat_module },
    { cliat_class,             msat_class },
    { cliat_struct,            msat_struct },
    { cliat_enum,              msat_enum },
    { cliat_constructor,       msat_constructor },
    { cliat_method,            msat_method },
    { cliat_property,          msat_property },
    { cliat_field,             msat_field },
    { cliat_event,             msat_event },
    { cliat_interface,         msat_interface },
    { cliat_parameter,         msat_parameter },
    { cliat_delegate,          msat_delegate },
    { cliat_returnvalue,       msat_returnvalue },
    { cliat_genericparameter,  msat_genericparameter }
  };

  for (i = 0;
       cliat != 0 && i < sizeof(enumerator_map)/sizeof(enumerator_map[0]);
       ++i) {
    if ((cliat & enumerator_map[i].cliat) == enumerator_map[i].cliat) {
      cliat &= ~enumerator_map[i].cliat;
      msat |= enumerator_map[i].msat;
    }  /* if */
  }  /* for */
  check_assertion(msat != msat_invalid);
  return msat;
}  /* msat_from_cliat */


/*
Values for the enumerators in the C++/CX
Windows::Foundation::Metadata::AttributeTargets enum.
*/
#define cppcxat_delegate       0x00000001
			/* Applies to a delegate. */
#define cppcxat_enum           0x00000002
			/* Applies to a C++/CX enum. */
#define cppcxat_event          0x00000004
			/* Applies to an event. */
#define cppcxat_field          0x00000008
			/* Applies to a field. */
#define cppcxat_interface      0x00000010
			/* Applies to a C++/CX interface. */
#define cppcxat_method         0x00000040
			/* Applies to a function. */
#define cppcxat_parameter      0x00000080
			/* Applies to a parameter. */
#define cppcxat_property       0x00000100
			/* Applies to a property. */
#define cppcxat_runtimeclass   0x00000200
			/* Applies to a C++/CX ref class. */
#define cppcxat_struct         0x00000400
			/* Applies to a C++/CX value type that is not an
			   enum. */
#define cppcxat_interfaceimpl  0x00000800
			/* Applies to an implementation of an interface. */
#define cppcxat_all            0xFFFFFFFF

typedef unsigned int a_cppcx_attribute_target;

static an_ms_attribute_target msat_from_cppcxat(
                                             a_cppcx_attribute_target cppcxat)
/*
Translate a C++/CX attribute target bit set to a general Microsoft attribute
target bit set.
*/
{
  unsigned               i;
  an_ms_attribute_target msat = msat_invalid;
  constexpr struct an_enumerator_map {
    a_cppcx_attribute_target cppcxat;
    an_ms_attribute_target   msat;
  } enumerator_map[] = {
    { cppcxat_all,           msat_delegate | msat_enum | msat_event |
                             msat_field | msat_interface |
                             msat_method | msat_parameter |
                             msat_property | msat_class |
                             msat_struct | msat_interfaceimpl },
    { cppcxat_delegate,      msat_delegate },
    { cppcxat_enum,          msat_enum },
    { cppcxat_event,         msat_event },
    { cppcxat_field,         msat_field },
    { cppcxat_interface,     msat_interface },
    { cppcxat_method,        msat_method },
    { cppcxat_parameter,     msat_parameter },
    { cppcxat_property,      msat_property },
    { cppcxat_runtimeclass,  msat_class },
    { cppcxat_struct,        msat_struct },
    { cppcxat_interfaceimpl, msat_interfaceimpl }
  };

  for (i = 0;
       cppcxat != 0 && i < sizeof(enumerator_map)/sizeof(enumerator_map[0]);
       ++i) {
    if ((cppcxat & enumerator_map[i].cppcxat) == enumerator_map[i].cppcxat) {
      cppcxat &= ~enumerator_map[i].cppcxat;
      msat |= enumerator_map[i].msat;
    }  /* if */
  }  /* for */
  check_assertion(msat != msat_invalid);
  return msat;
}  /* msat_from_cppcxat */


static void set_attribute_usage_from_attribute(
                                    an_ms_attribute_usage_ptr attribute_usage,
                                    an_ms_attribute_ptr       msap)
/*
Record in attribute_usage the usage constraints of a custom attribute as
specified with the custom AttributeUsage attribute described by msap.
*/
{
  a_boolean                     ovflo;
  a_constant_ptr                con;

  check_assertion(msap->kind == (an_ms_attribute_kind)msak_custom);
  check_assertion(is_cli_type_of_kind(msap->variant.custom_info.type,
                                      csk_system_attribute_usage_attribute));
  if (msap->variant.custom_info.args != NULL &&
      msap->variant.custom_info.args->next == NULL &&
      is_constant_node(msap->variant.custom_info.args) &&
      is_cli_type_of_kind(msap->variant.custom_info.args->type,
                          csk_system_attribute_targets)) {
    con = node_constant(msap->variant.custom_info.args);
    if (cppcx_enabled) {
      if (!int_constant_is_signed(con)) {
        a_cppcx_attribute_target cppcxat;
        cppcxat = (a_cppcx_attribute_target)
                              unsigned_value_of_integer_constant(con, &ovflo);
        check_assertion(!ovflo);
        attribute_usage->valid_on = msat_from_cppcxat(cppcxat);
      } else {
        unexpected_condition();
      }  /* if */
    } else
    /* Do not insert code here. */
    if (int_constant_is_signed(con)) {
      a_cli_attribute_target cliat;
      cliat = (a_cli_attribute_target)value_of_integer_constant(con, &ovflo);
      check_assertion(!ovflo);
      attribute_usage->valid_on = msat_from_cliat(cliat);
    } else {
      unexpected_condition();
    }  /* if */
  } else {
    expect_error();
  }  /* if */
  /* The C++/CX AttributeUsageAttribute does not have the "AllowMultiple" or
     "Inherited" properties.  "AllowMultiple" can be specified by applying the
     AllowMultipleAttribute to the attribute type. */
  if (cppcli_enabled) {
    a_custom_ms_attribute_arg_ptr
                             named_arg = msap->variant.custom_info.named_args;
    for (; named_arg != NULL; named_arg = named_arg->next) {
      a_const_char *name =
                         unmangled_name_of(&named_arg->field->source_corresp);
      check_assertion(name != NULL && is_constant_node(named_arg->expression));
      con = node_constant(named_arg->expression);
      if (is_bool_type(con->type) && strcmp(name, "AllowMultiple") == 0) {
        check_assertion(int_constant_is_signed(con));
        attribute_usage->allow_multiple =
                                  value_of_integer_constant(con, &ovflo) != 0;
        check_assertion(!ovflo);
      } else if (is_bool_type(con->type) && strcmp(name, "Inherited") == 0) {
        check_assertion(int_constant_is_signed(con));
        attribute_usage->inherited =
                                  value_of_integer_constant(con, &ovflo) != 0;
        check_assertion(!ovflo);
      } else {
        unexpected_condition();
      }  /* if */
    }  /* for */
  }  /* if */
}  /* set_attribute_usage_from_attribute */


static an_ms_attribute_usage_ptr attribute_usage_for_attribute_type(
                                                              a_type_ptr type)
/*
Return a description of the valid uses of the custom attribute represented by
the given type.
*/
{
  an_ms_attribute_usage_ptr attribute_usage = NULL;

  type = skip_typerefs(type);
  if (is_immediate_class_type(type)) {
    attribute_usage = &class_type_supp(type)->attribute_usage;
    if (attribute_usage->valid_on == msat_invalid) {
      /* Determine the default attribute usage for the attribute type. */
      if (cppcx_enabled) {
        /* C++/CX attribute types must be sealed, so the default attribute
           usage is always the same. */
        attribute_usage->valid_on = msat_from_cppcxat(cppcxat_all);
        attribute_usage->allow_multiple = FALSE;
        attribute_usage->inherited = FALSE;
      } else if (is_cli_type_of_kind(type, csk_system_attribute)) {
        attribute_usage->valid_on = msat_from_cliat(cliat_all);
        attribute_usage->allow_multiple = FALSE;
        attribute_usage->inherited = TRUE;
      } else {
        /* We do not yet defer attribute processing until the class
           definition is complete in order to be able to inherit the
           attribute usage from the base class. */
#if 0
        check_assertion(is_cli_attribute_type(type));
        a_base_class_ptr  bcp = base_classes_of(type);
        for (; bcp != NULL; bcp = bcp->next) {
          if (bcp->direct && is_cli_attribute_type(bcp->type)) {
            /* Inherit the attribute usage from the base class. */
            *attribute_usage = *attribute_usage_for_attribute_type(bcp->type);
            break;
          }  /* if */
        }  /* for */
#else /* !0 */
        attribute_usage->valid_on = msat_from_cliat(cliat_all);
        attribute_usage->allow_multiple = FALSE;
        attribute_usage->inherited = TRUE;
#endif /* 0 */
      }  /* if */
      check_assertion(attribute_usage->valid_on != msat_invalid);
    }  /* if */
  }  /* if */
  return attribute_usage;
}  /* attribute_usage_for_attribute_type */


static char *get_string_value_for_token(a_boolean	*err)
/*
If the current token is an identifier or string literal, this routine
copies the characters of the token static buffer and converts them to lower
case (for a string literal, the quotes are not part of the string).  If the
string contains a null character, any characters after the null are discarded.
A pointer to the static buffer is returned.  If the current token is something
other than an identifier or string literal, a NULL pointer is returned.  If
the token is a string literal, but the constant is an error constant, "err"
is set to TRUE.  Note that "err" is not TRUE for an unexpected token kind.
*/
{
  a_const_char	    *src = NULL;
  a_boolean	    valid_token = TRUE;
  char		    *result = NULL;
  a_character_kind  char_kind = (a_character_kind)chk_char;
  a_targ_size_t	    len = 0, pos, char_size = 1;

  *err = FALSE;
  /* Copy the characters into a buffer, converting any upper case characters
     to lower case.  Allocate a buffer on the first call of this routine. */
  if (ms_attr_buffer == NULL) ms_attr_buffer = alloc_text_buffer(32);
  reset_text_buffer(ms_attr_buffer);
  /* The source of the characters to be copied depends on the kind of token
     provided. */
  if (curr_token == tok_identifier) {
    /* An identifier.  Use the characters of the identifier. */
    src = locator_for_curr_id.symbol_header->identifier;
    len = (a_targ_size_t)strlen(src);
  } else if (curr_token == tok_string_literal) {
    if (is_error_constant(&const_for_curr_token)) {
      /* We encountered a misformed string literal.  An error should
         have been issued already. */
      check_assertion(is_at_least_one_error());
      *err = TRUE;
    } else {
      src = const_for_curr_token.variant.string.value;
      /* Determine if the source constant is a wide string literal. */
      char_kind = enum_cast<a_character_kind>(
                                          const_for_curr_token.character_kind);
      char_size = character_size[char_kind];
      /* Subtract one character to ignore the null terminator. */
      len = const_for_curr_token.variant.string.length - char_size;
    }  /* if */
  } else if (is_keyword_token(curr_token)) {
    /* A token initially cached as a keyword that should be treated as an
       identifier. */
    src = token_names[(int)curr_token];
    len = (a_targ_size_t)strlen(src);
  } else {
    /* Some other token kind */
    valid_token = FALSE;
  }  /* if */
  /* Copy the token to the buffer. */
  if (src != NULL) {
    /* Copy the characters to the text buffer, converting them to lower
       case. */
    for (pos = 0; pos < len; src += char_size, pos += char_size) {
      char	ch;
      if (char_kind == (a_character_kind)chk_char) {
        ch = *src;
      } else {
        /* A wide literal.  Extract the character value.  Values out of
           range are truncated.  As this routine is used for strings with
           expected values, this should result in an error later. */
        ch = (char)extract_character_from_string(src,
                                                 (unsigned int)char_size);
      }  /* if */
      if (is_id_char[ch-CHAR_MIN]) ch = (char)tolower((int)ch);
      add_char_to_text_buffer(ms_attr_buffer, ch);
    }  /* for */
    /* Add a null terminator. */
    add_char_to_text_buffer(ms_attr_buffer, '\0');
    result = ms_attr_buffer->buffer;
  }  /* if */
  /* For a valid token kind, bypass the token. */
  if (valid_token) (void)get_token();
  return result;
}  /* get_string_value_for_token */


static a_constant_ptr get_string_constant_for_token(a_boolean	*err)
/*
If the current token is an identifier or string literal, this routine
returns a constant that represents the string literal or the identifier
name converted to a string literal constant.  A string literal constant
is returned as scanned by the lexical routines, and so may contain
embedded null characters.  If the current token is something other than
an identifier or string literal, a NULL pointer is returned.  If the token
is a string literal, but the constant is an error constant, "err" is set to
TRUE.  Note that "err" is not TRUE for an unexpected token kind.
*/
{
  a_constant_ptr	result = NULL;
  a_boolean		valid_token = TRUE;
  a_constant_ptr	constant = local_constant();

  *err = FALSE;
  if (curr_token == tok_string_literal) {
    if (is_error_constant(&const_for_curr_token)) {
      /* We encountered a misformed string literal.  An error should
         have been issued already. */
      check_assertion(is_at_least_one_error());
      *err = TRUE;
      set_error_constant(constant);
      result = constant;
    } else {
      result = &const_for_curr_token;
    }  /* if */
  } else if (curr_token == tok_identifier || is_keyword_token(curr_token)) {
    /* Convert the identifier into a string constant.  If a token was
       initially scanned as a keyword, treat is as an identifier with the
       name of the keyword. */
    sizeof_t     length;
    a_const_char *str;
    if (curr_token == tok_identifier) {
      str = locator_for_curr_id.symbol_header->identifier;
    } else {
      str = token_names[(int)curr_token];
    }  /* if */
    /* Add space for the null terminator. */
    length = strlen(str) + 1;
    clear_constant(constant, (a_constant_repr_kind)ck_string);
    constant->type = string_type((a_targ_size_t)length);
    constant->variant.string.length = (a_targ_size_t)length;
    constant->variant.string.value =
                          copy_string_to_region(file_scope_region_number, str);
    result = constant;
  } else {
    /* Some other token kind */
    valid_token = FALSE;
  }  /* if */
  /* For a valid token kind, bypass the token. */
  if (valid_token) (void)get_token();
  /* Get a shared version of the result constant. */
  if (result != NULL) result = alloc_shareable_constant(result);
  release_local_constant(&constant);
  return result;
}  /* get_string_constant_for_token */


static long scan_ms_attribute_integer_arg(void)
/*
Scan an integer argument of a Microsoft attribute.  Return the value
scanned.
*/
{
  a_host_large_integer	value = 0;
  a_constant_ptr	constant = local_constant();

  /* The argument can be an expression, but must be constant. */
  scan_integral_constant_expression(constant);
  if (!is_error_constant(constant)) {
    a_boolean	err;
    value = value_of_integer_constant(constant, &err);
    /*lint --e{587,650,685}*/
    /* coverity[result_independent_of_operands] */
    if (err || value > LONG_MAX || value < LONG_MIN) {
      /* Attribute values should be small integers.  Issue an error on an
         attempt to use a very large integer. */
      pos_error(ec_integer_too_large, &error_position);
    }  /* if */
  }  /* if */
  release_local_constant(&constant);
  return (long)value;
}  /* scan_ms_attribute_integer_arg */


static a_constant_ptr scan_ms_attribute_string_arg(void)
/*
Scan a string argument of a Microsoft attribute.  Return the value
scanned.
*/
{
  a_boolean		err;
  a_constant_ptr	constant;

  constant = get_string_constant_for_token(&err);
  if (constant == NULL && !err) {
    /* The current token was not of an expected kind. */
    syntax_error(ec_exp_string_literal);
    constant = alloc_error_constant();
  }  /* if */
  return constant;
}  /* scan_ms_attribute_string_arg */


static char *scan_ms_attribute_other_arg(void)
/*
Scan an "other" argument of a Microsoft attribute.  Return the value
scanned.  The "other" argument kind is used for arguments that can
be arbitrary tokens.  The tokens until the argument terminator are
converted into a string.
*/
{
  char					*value = NULL;
  a_token_sequence_number		first_token;
  a_token_sequence_number		last_token;
  a_source_position			start_position;

  /* Save the token sequence number of the first token of the argument. */
  first_token = curr_token_sequence_number;
  start_position = pos_curr_token;
  flush_tokens_without_warning();
  /* Save the position of the token following the argument. */
  last_token = curr_token_sequence_number;
  /* Create the string version of the argument. */
  /* Don't allow wrapping keyword-like identifiers inside __identifier(...). */
  init_token_string(&start_position, /*keep_spacing=*/FALSE,
                    /*suppress_identifier_wrapping=*/TRUE);
  add_token_cache_segment_to_string(attribute_cache, first_token,
                                    last_token);
  /* Copy the string to IL memory. */
  value = make_copy_of_token_string();
  return value;
}  /* scan_ms_attribute_other_arg */


static a_boolean scan_ms_attribute_boolean_arg(
					an_ms_attribute_param_ptr	param)
/*
Scan a boolean argument of a Microsoft attribute.  Return the value
scanned.
*/
{
  a_boolean		result = FALSE;
  a_source_position	arg_pos;
  a_boolean		err;
  char			*string_for_token;

  arg_pos = pos_curr_token;
  string_for_token = get_string_value_for_token(&err);
  if (string_for_token == NULL && !err) {
    /* The current token was not of an expected kind. */
    str_error(ec_exp_ms_attr_bool_value, param->name);
    flush_tokens();
  }  /* if */
  /* The source pointer will be NULL in error cases. */
  if (string_for_token != NULL) {
    if (strcmp(string_for_token, "true") == 0) {
      result = TRUE;
    } else if (strcmp(string_for_token, "false") == 0) {
      result = FALSE;
    } else {
      /* An invalid value.  Issue a diagnostic. */
      pos_st_error(ec_exp_ms_attr_bool_value, &arg_pos, param->name);
    }  /* if */
  }  /* if */
  return result;
}  /* scan_ms_attribute_boolean_arg */


static int scan_ms_attribute_enum_arg(an_ms_attribute_param_ptr	param)
/*
Scan a enumeration argument of a Microsoft attribute.  Return the value
that identifies the element of the enumeration that was specified.
The first entry on the list is 1.  Zero is returned if no matching
entry was found.

The value specified may or may not be enclosed in quotes.  Case is not
significant.
*/
{
  int			result = 0;
  a_source_position	arg_pos;
  a_boolean		err;
  char			*string_for_token;

  arg_pos = pos_curr_token;
  string_for_token = get_string_value_for_token(&err);
  if (string_for_token == NULL && !err) {
    /* The current token was not of an expected kind. */
    str_error(ec_exp_ms_attr_enum_value, param->name);
    flush_tokens();
  }  /* if */
  /* The source pointer will be NULL in error cases. */
  if (string_for_token != NULL) {
    /* Look for the resulting value in the list of acceptable values. */
    char	**values;
    for (values = param->values; *values != NULL; values++) {
      if (strcmp(*values, string_for_token) == 0) break;
    }  /* for */
    /* If we found a match, compute the element number. */
    if (*values != NULL) {
      result = (int)(values - param->values);
    } else {
      /* An invalid value.  Issue a diagnostic. */
      pos_st_error(ec_invalid_ms_attr_enum_value, &arg_pos, param->name);
    }  /* if */
  }  /* if */
  return result;
}  /* scan_ms_attribute_enum_arg */


static a_const_char *scan_ms_attribute_uuid_arg(
                                               an_ms_attribute_param_ptr param)
/*
Scan an argument of UUID type.  Such arguments are either a UUID string
or a __uuidof operator.  The UUID string is returned.  A NULL pointer is
returned for invalid arguments.
*/
{
  a_const_char		*result = NULL;
  a_source_position	arg_pos;

  arg_pos = pos_curr_token;
  if (curr_token == tok_string_literal || curr_token == tok_uuid) {
    /* A string literal or an unquoted UUID string.  Scan it as a GUID
       string. */
    result = scan_GUID_string();
  } else if (curr_token == tok_uuidof) {
    /* Scan the __uuidof operator.  It is of the form __uuidof(operand). */
    /* Skip over the __uuidof token before calling scan_uuidof_operand. */
    (void)get_token();
    result = scan_uuidof_operand();
  } else {
    /* An invalid value.  Issue a diagnostic. */
    pos_st_error(ec_invalid_ms_attr_uuid_value, &arg_pos, param->name);
    flush_tokens();
  }  /* if */
  return result;
}  /* scan_ms_attribute_uuid_arg */


static an_ms_attribute_arg_ptr scan_ms_attribute_arg(
					an_ms_attribute_param_ptr	param)
/*
Scan an argument to a Microsoft attribute reference.  Create an argument
entry that describes the argument, and return it to the caller.  "param"
is the parameter description for the parameter associated with the argument.
*/
{
  an_ms_attribute_arg_ptr	arg;

  /* Allocate an argument entry of the required kind. */
  arg = alloc_ms_attribute_arg(param->kind);
  arg->param_name = param->name;
  switch (param->kind) {
    case msaak_uuid:
      /* A GUID string. */
      arg->variant.uuid_string = scan_ms_attribute_uuid_arg(param);
      break;
    case msaak_integer:
      /* An integer constant. */
      arg->variant.integer_value = scan_ms_attribute_integer_arg();
      break;
    case msaak_boolean:
      /* A boolean constant. */
      arg->variant.bool_value = scan_ms_attribute_boolean_arg(param);
      break;
    case msaak_enumeration:
      /* A member of a list of specific values. */
      arg->variant.enum_value = scan_ms_attribute_enum_arg(param);
      break;
    case msaak_string:
      /* A string. */
      arg->variant.string_constant = scan_ms_attribute_string_arg();
      break;
    case msaak_other:
      /* An "other" argument.  Convert all the tokens until the terminating
         "," or ")" into a string. */
      arg->variant.other_string = scan_ms_attribute_other_arg();
      break;
    default:
      unexpected_condition_str("scan_ms_attribute_arg: bad argument kind");
  }  /* switch */
  return arg;
}  /* scan_ms_attribute_arg */


static an_ms_attribute_param_ptr get_named_parameter(
				an_ms_attribute_kind_descr_ptr	attr_descr,
				an_ms_attribute_arg_ptr		arg_list)
/*
The current token is expected to be an identifier that gives the name of
the parameter being specified.  Look up the name in the list of parameters
and return the parameter pointer.  If there is no matching parameter, issue
an error and return a NULL pointer.

attr_descr describes the attribute being scanned.  arg_list is the list of
arguments scanned so far, and is used to detect a duplicated argument.
*/
{
  char				*param_name;
  an_ms_attribute_param_ptr	msapp = NULL;
  a_boolean			err;
  a_source_position		name_pos;

  name_pos = pos_curr_token;
  /* Get the lower case string for the parameter name. */
  param_name = get_string_value_for_token(&err);
  if (param_name != NULL) {
    /* Look for the parameter name in the parameter list. */
    for (msapp = attr_descr->parameters; msapp != NULL; msapp = msapp->next) {
      /* See if the names match.  Ignore parameters that are to be considered
         unnamed. */
      if (!msapp->is_unnamed && strcmp(param_name, msapp->name) == 0) {
        break;
      }  /* if */
    }  /* for */
    if (msapp == NULL) {
      /* No match was found. */
      pos_st2_error(ec_invalid_ms_attr_name, &name_pos, attr_descr->name,
                    param_name);
    } else {
      /* Check for a repeated argument. */
      an_ms_attribute_arg_ptr	msaap;
      for (msaap = arg_list; msaap != NULL; msaap = msaap->next) {
        if (strcmp(msaap->param_name, param_name) == 0) {
          /* The argument has already been given a value. */
          pos_st_error(ec_duplicate_ms_attr_arg, &name_pos, param_name);
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  /* The caller should have already verified that the next token is a "=". */
  check_assertion(curr_token == tok_assign);
  /* Bypass the "=". */
  (void)get_token();
  return msapp;
}  /* get_named_parameter */


static an_ms_attribute_arg_ptr scan_ms_attribute_arg_list(
				an_ms_attribute_kind_descr_ptr	attr_descr,
				a_boolean			*err)
/*
Scan the arguments of a Microsoft attribute reference.  The current token is
usually either the "=" that precedes a single argument or the "(" the
precedes an argument list, but this routine is also called for attributes
that expect a parameter list even if such a list is missing (so that a
diagnostic can be issued here).  Return a pointer to the list of arguments,
or NULL if the argument list is invalid.  If the argument list is invalid,
*err is also set to TRUE.
*/
{
  an_ms_attribute_param_ptr	param;
  an_ms_attribute_arg_ptr	arg_list = NULL;
  an_ms_attribute_arg_ptr	arg_tail = NULL;
  a_boolean			any_named_args = FALSE;
  a_boolean			any_errors = FALSE;

  *err = FALSE;
  param = attr_descr->parameters;
  if (curr_token == tok_assign) {
    /* Some attributes accept "attr=x" style references, which has the effect
       of providing a value for the initial argument. */
    if (!attr_descr->initialization_style_arg_allowed) {
      /* This style of argument is not permitted for this attribute. */
      str_error(ec_cannot_assign_to_ms_attr, attr_descr->name);
      flush_tokens();
      any_errors = TRUE;
    } else {
      /* Scan the argument associated with the initial parameter. */
      /* Bypass the "=" */
      (void)get_token();
      arg_list = scan_ms_attribute_arg(param);
      param = param->next;
    }  /* if */
  } else if (curr_token == tok_lparen) {
    /* Bypass the left parenthesis. */
    check_assertion(curr_token == tok_lparen);
    (void)get_token();
    add_stop_token(tok_rparen);
    /* Scan a comma-separated list of arguments. */
    do {
      an_ms_attribute_arg_ptr	arg;
      /* Arguments may be specified either positionally or with names.
         A named argument is of the form "name=value".  Check for a named
         argument. */
      if ((curr_token == tok_identifier || is_keyword_token(curr_token)) &&
          next_token() == tok_assign) {
        param = get_named_parameter(attr_descr, arg_list);
        any_named_args = TRUE;
        if (param == NULL) {
          /* An invalid named parameter.  Flush to the next argument. */
          flush_tokens();
          continue;
        }  /* if */
      } else if (any_named_args) {
        /* A positional argument cannot follow a named one. */
        if (!any_errors) pos_error(ec_positional_after_named, &error_position);
        /* Flush to the next argument. */
        flush_tokens();
        any_errors = TRUE;
        continue;
      } else if (param == NULL) {
        if (!any_errors) pos_error(ec_too_many_ms_attr_args, &error_position);
        flush_tokens();
        any_errors = TRUE;
        continue;
      }  /* if */
      arg = scan_ms_attribute_arg(param);
      if (arg_list == NULL) {
        arg_list = arg;
      } else {
        check_assertion(arg_tail != NULL);
        arg_tail->next = arg;
      }  /* if */
      arg_tail = arg;
      /* Advance to the next parameter in the list. */
      if (!any_named_args) param = param->next;
    } while (loop_token(tok_comma));
    remove_stop_token(tok_rparen);
    /* Look for the closing right parenthesis. */
    (void)required_token(tok_rparen, ec_exp_rparen);
  }  /* if */
  if (arg_list == NULL && !any_errors &&
      attr_descr->kind  > (an_ms_attribute_kind)msak_misc &&
      param != NULL && !param->is_unnamed) {
    /* If there is a parameter list but no arguments were specified, issue
       an error.  We don't currently know which arguments are required,
       so we don't issue an error for too few arguments. */
    if (arg_list == NULL) {
      *err = TRUE;
      str_error(ec_exp_ms_attr_arg_list, attr_descr->name);
    }  /* if */
  }  /* if */
  return arg_list;
}  /* scan_ms_attribute_arg_list */


static void scan_unrecognized_ms_attribute_arg_list(void)
/*
Scan the arguments of an unrecognized Microsoft attribute reference.  The
current token is the "=" that precedes a single argument or the "(" that
precedes an argument list.  Because the attribute is unrecognized, we don't
know the form of the parameter list expected.  This routine just scans tokens
until the end of the attribute is found.
*/
{
  /* The stop tokens should be set appropriately so that this will flush
     to the "," that separates attributes, or to the closing "]" of
     the attribute block. */
  flush_tokens_without_warning();
}  /* scan_unrecognized_ms_attribute_arg_list */


static an_ms_attribute_target scan_ms_attribute_target(void)
/*
Look for an optional attribute target and return a value indicating which
target, if any, was found.
*/
{
  an_ms_attribute_target  target = msat_none;

  if (next_token() == tok_colon) {
    a_const_char *invalid_target_identifier = NULL;
    if (curr_token == tok_identifier) {
      a_symbol_header_ptr  sym_hdr = locator_for_curr_id.symbol_header;
      switch (sym_hdr->identifier[0]) {
      case 'a':
        if (symbol_header_is_for_identifier_string(sym_hdr, "assembly")) {
          target = msat_assembly;
        }  /* if */
        break;
      case 'c':
        if (symbol_header_is_for_identifier_string(sym_hdr, "constructor")) {
          target = msat_constructor;
        }  /* if */
        break;
      case 'd':
        if (symbol_header_is_for_identifier_string(sym_hdr, "delegate")) {
          target = msat_delegate;
        }  /* if */
        break;
      case 'e':
        if (symbol_header_is_for_identifier_string(sym_hdr, "event")) {
          target = msat_event;
        }  /* if */
        break;
      case 'f':
        if (symbol_header_is_for_identifier_string(sym_hdr, "field")) {
          target = msat_field;
        }  /* if */
        break;
      case 'g':
        if (symbol_header_is_for_identifier_string(sym_hdr,
                                                   "genericparameter")) {
          target = msat_genericparameter;
        }  /* if */
        break;
      case 'i':
        if (symbol_header_is_for_identifier_string(sym_hdr, "interface")) {
          target = msat_interface;
        }  /* if */
        break;
      case 'm':
        if (symbol_header_is_for_identifier_string(sym_hdr, "method")) {
          target = msat_method;
        } else if (symbol_header_is_for_identifier_string(sym_hdr,
                                                          "module")) {
          target = msat_module;
        }  /* if */
        break;
      case 'p':
        if (symbol_header_is_for_identifier_string(sym_hdr, "parameter")) {
          target = msat_parameter;
        } else if (symbol_header_is_for_identifier_string(sym_hdr,
                                                          "property")) {
          target = msat_property;
        }  /* if */
        break;
      case 'r':
        if (symbol_header_is_for_identifier_string(sym_hdr, "returnvalue")) {
          target = msat_returnvalue;
        }  /* if */
        break;
      default:
        /* Nothing to do. */
        break;
      }  /* switch */
      if (target == msat_none) {
        /* The identifier is not a valid attribute target. */
        target = msat_invalid;
        invalid_target_identifier = sym_hdr->identifier;
      }  /* if */
    } else if (curr_token == tok_class) {
      target = msat_class;
    } else if (curr_token == tok_struct) {
      target = msat_struct;
    } else if (curr_token == tok_enum) {
      target = msat_enum;
    } else if (curr_token == tok_typedef) {
      target = msat_typedef;
    } else {
      /* Although the next token is a colon, the current token is not a valid
         attribute target.  If the token is a keyword, issue an error
         indicating the invalid target; otherwise, treat this as if no
         target was specified and allow further scanning to issue
         appropriate diagnostics. */
      if (is_keyword_token(curr_token)) {
        target = msat_invalid;
        invalid_target_identifier = token_names[(int)curr_token];
      } else {
        target = msat_none;
      }  /* if */
    }  /* if */
    if (target == msat_invalid) {
      pos_st_error(ec_invalid_ms_attribute_target, &pos_curr_token,
                   invalid_target_identifier);
      /* Discard the rest of this attribute. */
      flush_tokens();
    } else if (target != msat_none) {
      /* Skip past the target and the colon. */
      (void)get_token();
      (void)get_token();
    }  /* if */
  }  /* if */
  return target;
}  /* scan_ms_attribute_target */


static an_ms_attribute_ptr scan_ms_attribute(
                                         ARG_UNUSED a_boolean is_param_or_base)
/*
Scan a single Microsoft attribute of an attribute block that may contain
multiple attributes.  Return a pointer to the attribute entry that represents
the attribute.

is_param_or_base is TRUE if the attributes appear on a function parameter or a
base class declaration (the corresponding IL entries point back to the
attributes).
*/
{
  an_ms_attribute_target         target;
  an_ms_attribute_kind_descr_ptr attr_descr = NULL;
  a_token_sequence_number        first_token;
  a_token_sequence_number        last_token;
  a_source_position              start_position;
  an_ms_attribute_ptr            attr = NULL;
  a_type_ptr                     custom_attribute_type = NULL;
  a_boolean                      is_attribute_attribute = FALSE;

  /* Save the token sequence number of the first token of this attribute. */
  start_position = pos_curr_token;
  /* Look for an optional attribute target before the attribute name. */
  target = scan_ms_attribute_target();
  /* We are going to cache the attribute proper later on: Remember the first
     token's sequence number. */
  first_token = curr_token_sequence_number;
  if (target != msat_invalid) {
    if (is_keyword_token(curr_token)) {
      /* Turn keywords back into identifiers in this context. */
      a_const_char  *keyword_str = token_names[(int)curr_token];
      (void)find_symbol(keyword_str, (sizeof_t)strlen(keyword_str),
                        &locator_for_curr_id);
      replace_curr_token(tok_identifier);
    }  /* if */
    /* Look up the attribute identifier.  If the identifier is unknown,
       the "unrecognized" attribute will be returned.  In error cases, such as
       a missing attribute name, a NULL attribute description is returned. */
    if (attr_descr == NULL) {
      attr_descr = look_up_attribute(&custom_attribute_type,
                                     &is_attribute_attribute);
    }  /* if */
  }  /* if */
  if (attr_descr != NULL) {
    /* Allocate an entry to represent this attribute. */
    attr = alloc_ms_attribute(attr_descr->kind);
    attr->position = start_position;
    attr->target = target;
    attr->is_attribute_attribute = is_attribute_attribute;
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Create a source sequence entry for the attribute.  This is not done
       for parameter attributes as they appear within a declaration. */
    if (!is_param_or_base) {
      attr->source_sequence_entry = add_empty_source_sequence_entry();
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    /* Look for an argument list.  We do this even for attributes without
       parameters, for error recovery purposes. */
    if (attr_descr->kind == (an_ms_attribute_kind)msak_custom) {
      attr->variant.custom_info.type = custom_attribute_type;
      if (!scan_custom_ms_attribute_arg_list(attr)) {
        /* An error occurred while scanning the argument list.  Set attr to
           NULL to discard the attribute. */
        attr = NULL;
      }  /* if */
    } else {
      a_boolean arg_list_present = curr_token == tok_assign ||
                                   curr_token == tok_lparen;
      attr->variant.info.kind_descr = attr_descr;
      attr->variant.info.name = attr_descr->name;
      if (arg_list_present || attr_descr->parameters != NULL) {
        if (attr->kind == (an_ms_attribute_kind)msak_unrecognized) {
          if (arg_list_present) {
            /* We are scanning an unrecognized attribute, so we don't know the
               form of the expected parameters. */
            scan_unrecognized_ms_attribute_arg_list();
          }  /* if */
        } else {
          /* The attribute is of a known kind.  The argument list can be
             scanned with knowledge of the associated parameters. */
          a_boolean	err;
          attr->variant.info.arg_list =
                                  scan_ms_attribute_arg_list(attr_descr, &err);
          if (err) {
            /* A NULL arg_list is returned if an error occurred while scanning
               the argument list.  Set attr to NULL to discard the
               attribute. */
            attr = NULL;
          }  /* if */
        }  /* if */
      } else if (curr_token != tok_comma && curr_token != tok_rbracket) {
        /* The attribute name was not followed by anything that looks like
           an argument, nor was it followed by anything that looks like an
           attribute separator or end of an attribute list. */
        if (attr->kind == (an_ms_attribute_kind)msak_unrecognized) {
          /* There are forms of Microsoft attributes that we don't currently
             support.  Don't attempt to diagnose a missing argument list as
             we probably got lost earlier on. */
          flush_tokens();
        } else {
          /* No parameters were expected.  Complain of a missing "," or
             "]". */
          syntax_error(ec_exp_comma_or_rbracket);
          /* The attribute is ill-formed, so discard it. */
          attr = NULL;
        }  /* if */
      }  /* if */
      /* Save the position of the token following the attribute. */
      last_token = curr_token_sequence_number;
      /* Create the string version of the attribute. */
      /* Don't allow wrapping keyword-like identifiers inside
         __identifier(...). */
      init_token_string(&start_position, /*keep_spacing=*/FALSE,
                        /*suppress_identifier_wrapping=*/TRUE);
      add_token_cache_segment_to_string(attribute_cache, first_token,
                                        last_token);
      /* Copy the string to IL memory. */
      if (attr != NULL) {
        attr->variant.info.string = make_copy_of_token_string();
      }  /* if */
  #if DEBUG
  /* GCOVR_EXCL_START */
      if (db_flag_is_set("msattr")) {
        if (attr != NULL) db_microsoft_attribute(attr);
      }  /* if */
  /* GCOVR_EXCL_STOP */
  #endif /* DEBUG */
    }  /* if */
  }  /* if */
  return attr;
}  /* scan_ms_attribute */


an_ms_attribute_ptr scan_microsoft_attributes(a_boolean	is_param_or_base)
/*
Scan a Microsoft attribute block.  The general syntax is:

  [ attr ]
  [ attr1, attr2 ]
  [ attr1(value, value), attr2 ]
  [ attr1(argname=value, argname=value) ]

Attributes can be standalone, in which case they are followed by a ";", or
can apply to the declaration that follows.

Each attribute block can contain multiple attributes.  A list of the attributes
is returned.  is_param_or_base is TRUE if the attributes appear on a function
parameter or a base class declaration (the corresponding IL entries point back
to the attributes).
*/
{
  an_ms_attribute_ptr	attr_list = NULL;
  an_ms_attribute_ptr	attr_tail = NULL;

  /* Start a new stop token state. */
  push_stop_token_stack();
  add_stop_token(tok_end_of_source);
  add_stop_token(tok_rbracket);
  add_stop_token(tok_comma);
  /* The current token is expected to be the opening "[". */
  check_assertion(microsoft_attribute_tokens_next());
  /* There can be multiple attribute blocks.  Scan all of them. */
  while (microsoft_attribute_tokens_next()) {
    cache_attribute_block();
    /* Bypass the "[". */
    (void)get_token();
    /* Scan the list of attributes. */
    do {
      an_ms_attribute_ptr	attr;
      /* Scan the attribute. */
      attr = scan_ms_attribute(is_param_or_base);
      /* A NULL attribute may be returned in certain error cases. */
      if (attr == NULL) continue;
      /* Add it to the list of attributes for this block. */
      if (attr_list == NULL) {
        attr_list = attr;
      } else {
        /* The list is linked on both the next and next_in_block pointers. */
        check_assertion(attr_tail != NULL);
        attr_tail->next_in_block = attr;
        attr_tail->next = attr;
      }  /* if */
      attr_tail = attr;
    } while (loop_token(tok_comma));
    /* Look for the closing right bracket. */
    (void)required_token(tok_rbracket, ec_exp_rbracket);
  }  /* while */
  /* Restore the stop token set as at entry. */
  remove_stop_token(tok_end_of_source);
  remove_stop_token(tok_rbracket);
  remove_stop_token(tok_comma);
  pop_stop_token_stack();
  return attr_list;
}  /* scan_microsoft_attributes */


void scan_and_append_microsoft_attributes(
                                        an_ms_attribute_ptr  *p_ms_attributes,
                                        a_boolean            is_param_or_base)
/*
The current token is assumed to be a square bracket that introduces Microsoft
attributes.  Scan these attributes and append them to the list pointed to by
*p_ms_attributes (if *p_ms_attributes is NULL, then the value of
*p_ms_attributes will be modified).  is_param_or_base is TRUE if we're
scanning a parameter declaration or a base class specifier.
*/
{
  an_ms_attribute_ptr  *last_ap = p_ms_attributes;
  while (*last_ap != NULL) {
    last_ap = &(*last_ap)->next;
  }  /* while */
  *last_ap = scan_microsoft_attributes(is_param_or_base);
}  /* scan_and_append_microsoft_attributes */


void skip_microsoft_attribute_tokens(void)
/*
A Microsoft attribute is next.  Skip over its tokens.
*/
{
  check_assertion(curr_token == tok_lbracket);
  while (curr_token == tok_lbracket) {
    flush_until_matching_token_full(/*limit_flush=*/FALSE);
    (void)get_token();
  }  /* if */
}  /* skip_microsoft_attribute_tokens */

#if GENERATE_SOURCE_SEQUENCE_LISTS

static void finalize_ms_attribute_source_sequence_entry(
						an_ms_attribute_ptr	msap,
						a_boolean		err)
/*
Complete the processing of the source sequence entry associated with the
Microsoft attribute "msap".  If "err" is TRUE, remove the empty source
sequence entry from the list.  Also remove it in template declaration contexts
when nonclass prototype instantiations are not recorded in the IL, since in
that case the corresponding entries for the template won't be recorded either.
Otherwise, complete the source sequence entry.
*/
{
  a_boolean  remove_sse = err;

  if (!remove_sse &&
      scope_is(&scope_stack_top(), sck_template_declaration) &&
      (!nonclass_prototype_instantiations ||
       !prototype_instantiations_in_il)) {
    remove_sse = TRUE;
  }  /* if */
  if (remove_sse) {
    /* This attribute is not being added to the IL, so we must remove the
       empty source sequence entry created for it. */
    if (msap->source_sequence_entry != NULL) {
      remove_from_src_seq_list(msap->source_sequence_entry);
      msap->source_sequence_entry = NULL;
    }  /* if */
  } else {
    update_source_sequence_list((char *)msap,
                                (an_il_entry_kind)iek_ms_attribute,
                                msap->source_sequence_entry);
  }  /* if */
}  /* finalize_ms_attribute_source_sequence_entry */

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

static a_boolean process_ms_attr_uuid(an_ms_attribute_ptr  msap)
/*
The given Microsoft attribute represents the "uuid" attribute applied to a
class type.  Record the associated uuid string in the class type.
*/
{
  check_assertion(msap->entity.kind == iek_type &&
                  is_immediate_class_type((a_type_ptr)msap->entity.ptr) &&
                  msap->variant.info.arg_list != NULL &&
                  msap->variant.info.arg_list->kind ==
                                        (an_ms_attribute_arg_kind)msaak_uuid);
  return record_uuid_for_class(
                             (a_type_ptr)msap->entity.ptr,
                             msap->variant.info.arg_list->variant.uuid_string,
                             &msap->position);
}  /* process_ms_attr_uuid */


static a_boolean process_ms_attr_custom(an_ms_attribute_ptr  msap,
                                        char                 *entity,
                                        an_il_entry_kind     kind)
/*
Process a custom attribute.  Return TRUE if the processing was successful;
otherwise, issue any appropriate diagnostics and return FALSE.
*/
{
  a_boolean  result = cli_or_cx_enabled;
  a_type_ptr attribute_type = NULL;
  a_boolean  is_attribute_usage_attribute = FALSE;
  a_boolean  is_allow_multiple_attribute = FALSE;

  check_assertion(msap->kind == (an_ms_attribute_kind)msak_custom);
  if (result) {
    attribute_type = msap->variant.custom_info.type;
    if (is_cli_type_of_kind(attribute_type,
                            csk_system_attribute_usage_attribute)) {
      check_assertion(kind == iek_type &&
                      is_immediate_class_type((a_type_ptr)entity));
      is_attribute_usage_attribute = TRUE;
#if 0
      /* We do not yet defer attribute processing until the class
         definition is complete in order to be able to issue this error. */
      if (!is_cli_attribute_type((a_type_ptr)entity)) {
        pos_ty_error(ec_cli_invalid_use_of_attribute_usage_attribute,
                     &msap->position, attribute_type);
        result = FALSE;
      }  /* if */
/* In error_msg.txt:
ec_cli_invalid_use_of_attribute_usage_attribute;;
"%t can only be used on a %[C++/CLI] attribute class"
*/
#endif /* 0 */
    } else if (cppcx_enabled &&
               is_cli_type_of_kind(
                  attribute_type,
                  csk_windows_foundation_metadata_allow_multiple_attribute)) {
      is_allow_multiple_attribute = TRUE;
      check_assertion(kind == iek_type &&
                      is_immediate_class_type((a_type_ptr)entity));
#if 0
      /* We do not yet defer attribute processing until the class
         definition is complete in order to be able to issue this error. */
      if (!is_cli_attribute_type((a_type_ptr)entity)) {
        pos_ty_error(ec_cli_invalid_use_of_attribute_usage_attribute,
                     &msap->position, attribute_type);
        result = FALSE;
      }  /* if */
#endif /* 0 */
    } else if (!cppcx_enabled &&
               is_cli_type_of_kind(attribute_type,
                                   csk_system_param_array_attribute)) {
      pos_ty_warning(ec_cli_param_array_attribute_deprecated,
                     &msap->position, attribute_type);
    }  /* if */
  }  /* if */
  if (result) {
    an_ms_attribute_usage_ptr attribute_usage;
    attribute_usage = attribute_usage_for_attribute_type(attribute_type);
    if (attribute_usage == NULL) {
      /* The attribute usage is unknown because it is a template-dependent
         or error type; assume allow_multiple is TRUE. */
    } else if (!attribute_usage->allow_multiple) {
      /* We do not yet issue an error if an attribute of the same type and
         target was previously applied to this entity.  Definitions must
         contain a superset of the attributes applied to an earlier
         declaration. */
    }  /* if */
  }  /* if */
  if (result) {
    /* The attribute is valid. */
    if (is_attribute_usage_attribute) {
      an_ms_attribute_usage_ptr attribute_usage;
      attribute_usage = attribute_usage_for_attribute_type((a_type_ptr)entity);
      if (attribute_usage != NULL) {
        set_attribute_usage_from_attribute(attribute_usage, msap);
      }  /* if */
    } else if (is_cli_type_of_kind(attribute_type,
                                   csk_system_obsolete_attribute)) {
      /* We should record the use of the System::ObsoleteAttribute in the
         entity and issue a warning if that entity is used. */
    } else if (is_cli_type_of_kind(attribute_type,
                                   csk_system_flags_attribute)) {
      /* We should record the use of the System::FlagsAttribute in the enum
         type. */
    } else if (is_allow_multiple_attribute) {
      an_ms_attribute_usage_ptr attribute_usage;
      attribute_usage = attribute_usage_for_attribute_type((a_type_ptr)entity);
      if (attribute_usage != NULL) {
        attribute_usage->allow_multiple = TRUE;
      }  /* if */
    } else if (cppcx_enabled &&
               is_cli_type_of_kind(
                      attribute_type,
                      csk_windows_foundation_metadata_deprecated_attribute)) {
      /* We should record the use of the DeprecatedAttribute in the entity
         and issue a warning if that entity is used. */
    }  /* if */
  }  /* if */
  return result;
}  /* process_ms_attr_custom */


static a_boolean process_microsoft_attribute(an_ms_attribute_ptr  msap,
                                             char                 *entity,
                                             an_il_entry_kind     kind)
/*
The given attribute requires special processing (e.g., updating the IL entry
it is bound to): Perform that processing.  entity may be NULL if the attribute
is not associated with a specific IL entry.  Return TRUE if the processing was
successful; otherwise, issue any appropriate diagnostics and return FALSE.
*/
{
  a_boolean result = FALSE;

  switch (msap->kind) {
    case msak_uuid:
      result = process_ms_attr_uuid(msap);
      break;
    case msak_custom:
      result = process_ms_attr_custom(msap, entity, kind);
      break;
#if INCLUDE_EDG_TEST_ATTRIBUTES
    case msak_edg_test:
      /* Nothing to be done. */
      result = TRUE;
      break;
#endif /* INCLUDE_EDG_TEST_ATTRIBUTES */
    case msak_coclass:
      /* Set a flag on the class to mark it for subsequent processing. */
      check_assertion(msap->entity.kind == iek_type &&
                      is_immediate_class_type((a_type_ptr)msap->entity.ptr));
      class_type_supp((a_type_ptr)msap->entity.ptr)->
                                                  has_coclass_attribute = TRUE;
      result = TRUE;
      break;
    case msak_no_injected_text:
      /* Flag to enable/disable injecting code. */
      if (msap->variant.info.arg_list == NULL) {
        /* If there is no parameter, the default is TRUE. */
        no_injected_text = TRUE;
      } else {
        check_assertion(msap->variant.info.arg_list->kind ==
                                      (an_ms_attribute_arg_kind)msaak_boolean);
        no_injected_text =
                 (a_boolean)msap->variant.info.arg_list->variant.integer_value;
      }  /* if */
      result = TRUE;
      break;
    default:
      unexpected_condition();
  }  /* switch */
  return result;
}  /* process_microsoft_attribute */


static a_boolean entity_is_prototype_instantiation(char              *entity,
                                                   an_il_entry_kind  kind)
/*
Return TRUE if the given entity is a prototype instantiation or a static
data member of a prototype instantiation.
*/
{
  a_boolean  result = FALSE;

  switch (kind) {
    case iek_type:
      { a_type_ptr  tp = (a_type_ptr)entity;
        if (is_immediate_class_type(tp) &&
            tp->variant.class_struct_union.is_prototype_instantiation) {
          result = TRUE;
        }  /* if */
      }
      break;
    case iek_routine:
      { a_routine_ptr  rp = (a_routine_ptr)entity;
        result = rp->is_prototype_instantiation;
      }
      break;
    case iek_variable:
      { a_variable_ptr  vp = (a_variable_ptr)entity;
        if (vp->source_corresp.is_class_member) {
          a_type_ptr  tp = parent_class_of(vp);
          if (tp->variant.class_struct_union.is_prototype_instantiation) {
            result = TRUE;
          }  /* if */
        }  /* if */
      }
      break;
    default:
      break;
  }  /* switch */
  return result;
}  /* entity_is_prototype_instantiation */


static an_ms_attribute_target standalone_ms_attribute_targets(void)
/*
Return the kinds of targets a standalone attribute (like "[XXX];") can apply
to in the current mode.
*/
{
  an_ms_attribute_target attr_target;

  if (cppcli_enabled) {
    attr_target = (msat_none | msat_assembly | msat_module);
  } else {
    attr_target = msat_none;
  }  /* if */
  return attr_target;
}  /* standalone_ms_attribute_targets */


void apply_microsoft_attributes(an_ms_attribute_ptr	*attributes,
                                char			*entity,
                                an_il_entry_kind	kind,
                                an_ms_attribute_target	target,
                                an_ms_attribute_target	cli_target)
/*
This routine is used to indicate that the list of Microsoft attributes
specified by "attributes" should apply to the entity specified by "entity".
The attributes must be applicable to the entity kind specified by "target"
or to an child entity of that entity that is explicitly targeted by the
attribute.  The appropriate entity is updated to reflect the attributes that
apply, and the attributes are added to the appropriate IL list (either the
scope list or a list in the param_type entry).  If "target" is msat_invalid,
no attribute can be applied to the entity.
*/
{
  an_ms_attribute_ptr		msap;
  an_ms_attribute_ptr		new_list = NULL;
  an_ms_attribute_ptr		new_tail = NULL;
  a_source_correspondence	*scp;
  an_ms_attribute_ptr		next_msap;
  a_boolean                     record_entity_in_attribute = TRUE;

  scp = source_corresp_for_il_entry(entity, kind);
  if (!prototype_instantiations_in_il) {
    /* Since prototype instantiations are not recorded in the IL, we cannot
       point the attribute to a prototype instantiation. */
    record_entity_in_attribute =
                             !entity_is_prototype_instantiation(entity, kind);
  }  /* if */
  /* Check whether the attributes have the appropriate target. */
  for (msap = *attributes; msap != NULL; msap = next_msap) {
    an_ms_attribute_target explicit_target = msap->target;
    an_ms_attribute_target applicable_targets;
    a_boolean              is_error = FALSE;
    next_msap = msap->next;
    if (cli_or_cx_enabled && msap->kind == (an_ms_attribute_kind)msak_custom) {
      applicable_targets = cli_target;
    } else {
      applicable_targets = target;
    }  /* if */
    if ((applicable_targets &
                         (msat_method | msat_routine | msat_delegate)) != 0) {
      applicable_targets |= msat_returnvalue;
    } else if ((applicable_targets &
                      (msat_constructor | msat_property | msat_event)) != 0) {
      applicable_targets |= msat_method;
    }  /* if */
    if (is_template_dependent_context()) {
      /* The Microsoft compiler doesn't check constraints in template
         declaration contexts. */
      applicable_targets = msat_any;
    }  /* if */
    if (cli_or_cx_enabled && explicit_target == msat_field &&
        (applicable_targets & (msat_property | msat_event)) != 0) {
      a_boolean is_trivial = FALSE;
      if (kind == (an_il_entry_kind)iek_field) {
        a_field_ptr field = (a_field_ptr)entity;
        is_trivial = field->property_or_event_descr->is_trivial;
      } else if (kind == (an_il_entry_kind)iek_variable) {
        a_variable_ptr variable = (a_variable_ptr)entity;
        is_trivial = variable->property_or_event_descr->is_trivial;
      } else {
        unexpected_condition();
      }  /* if */
      if (is_trivial) {
        applicable_targets |= msat_field;
      } else {
        is_error = TRUE;
        pos_error(ec_field_target_on_nontrivial_property_or_event,
                  &msap->position);
      }  /* if */
    }  /* if */
    if (!is_error && explicit_target != msat_none &&
        (applicable_targets & explicit_target) == 0) {
      /* The explicitly specified target is not an applicable target. */
      is_error = TRUE;
      pos_error(ec_invalid_attribute_target_for_ms_attr, &msap->position);
    }  /* if */
    if (!is_error) {
      a_type_ptr             attribute_type = NULL;
      an_ms_attribute_target valid_targets;
      if (msap->kind == (an_ms_attribute_kind)msak_custom) {
        an_ms_attribute_usage_ptr attribute_usage;
        attribute_type = msap->variant.custom_info.type;
        attribute_usage = attribute_usage_for_attribute_type(attribute_type);
        if (attribute_usage == NULL) {
          /* The attribute usage is unknown because it is a template-dependent
             or an error type; assume all targets are valid. */
          valid_targets = cppcx_enabled ?
                              msat_from_cppcxat(cppcxat_all) :
                              msat_from_cliat(cliat_all);
        } else {
          valid_targets = attribute_usage->valid_on;
        }  /* if */
      } else {
        valid_targets = msap->variant.info.kind_descr->target;
      }  /* if */
      check_assertion(valid_targets != msat_invalid);
      if ((valid_targets & ~standalone_ms_attribute_targets()) == 0) {
        /* The attribute can only be used as a standalone attribute. */
        is_error = TRUE;
        if (msap->kind == (an_ms_attribute_kind)msak_custom) {
          pos_ty_error(ec_invalid_use_of_standalone_custom_ms_attr,
                       &msap->position, attribute_type);
        } else if (valid_targets == msat_none) {
          pos_st_error(ec_invalid_use_of_standalone_ms_attr, &msap->position,
                       msap->variant.info.name);
        }  /* if */
      } else if ((valid_targets & applicable_targets) == 0) {
         is_error = TRUE;
         if (msap->kind == (an_ms_attribute_kind)msak_custom) {
           pos_ty_error(ec_invalid_use_of_custom_ms_attr, &msap->position,
                        attribute_type);
         } else {
           pos_st_error(ec_invalid_use_of_ms_attr, &msap->position,
                        msap->variant.info.name);
         }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
         if (db_flag_is_set("msattr")) {
           fprintf(f_debug, "Attribute target: valid=%x, applicable=%x\n",
                   (unsigned int)valid_targets,
                   (unsigned int)applicable_targets);
         }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
      }  /* if */
    }  /* if */
    if (!is_error) {
      /* A valid attribute.  Add it to the new list. */
      if (new_list == NULL) {
        new_list = msap;
      } else {
        check_assertion(new_tail != NULL);
        new_tail->next = msap;
      }  /* if */
      new_tail = msap;
      if (record_entity_in_attribute) {
        /* Update the entity pointer in the attribute. */
        msap->entity.kind = kind;
        msap->entity.ptr = entity;
        if (msap->kind > (an_ms_attribute_kind)msak_misc && !is_error) {
          /* An attribute for which special processing is needed. */
          is_error = !process_microsoft_attribute(msap, entity, kind);
        }  /* if */
      }  /* if */
      /* Except for parameter and base class entries, add the attribute to the
         scope list. */
      if (kind != (an_il_entry_kind)iek_param_type &&
          kind != (an_il_entry_kind)iek_base_class) {
        add_to_ms_attributes_list(msap, decl_scope_level);
      }  /* if */
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    if (kind != (an_il_entry_kind)iek_param_type &&
        kind != (an_il_entry_kind)iek_base_class) {
      /* Either complete the source sequence entry or discard it. */
      finalize_ms_attribute_source_sequence_entry(msap, is_error);
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  }  /* for */
  /* All entities except for param_type entries are expected to have
     source correspondences. */
  if (scp != NULL) {
    scp->has_associated_attribute = TRUE;
  } else if (kind == (an_il_entry_kind)iek_param_type) {
    a_param_type_ptr	ptp;
    /* Set the param type entry to point to the valid attributes. */
    ptp = (a_param_type_ptr)entity;
    ptp->ms_attributes = new_list;
  } else if (kind == (an_il_entry_kind)iek_base_class) {
    ((a_base_class_ptr)entity)->ms_attributes = new_list;
  } else {
    unexpected_condition();
  }  /* if */
  /* Clear the attribute list pointer passed by the caller. */
  *attributes = NULL;
}  /* apply_microsoft_attributes */


void apply_microsoft_attributes_to_type(an_ms_attribute_ptr *attributes,
                                        a_type_ptr          type)
/*
A wrapper around apply_microsoft_attributes to be used when applying
attributes to a type.
*/
{
  an_ms_attribute_target attr_target;
  an_ms_attribute_target cli_attr_target;

  attr_target = msat_invalid;
  cli_attr_target = msat_invalid;
  if (type_is_typedef(type)) {
    attr_target = msat_typedef;
  } else if (is_immediate_enum_type(type)) {
    attr_target = msat_enum;
    /* C++/CLI allows attributes on all enum types, but C++/CX only allows
       them on enum types with a declared assembly visibility. */
    if (!cppcx_enabled || is_cli_enum_type(type)) {
      cli_attr_target = msat_enum;
    }  /* if */
  } else if (cli_or_cx_enabled && is_immediate_delegate_type(type)) {
    cli_attr_target = msat_delegate;
  } else if (cppcli_enabled && is_cli_generic_param_type(type)) {
    cli_attr_target = msat_genericparameter;
  } else if (is_immediate_class_type(type)) {
    if (type->variant.class_struct_union.is_interface) {
      attr_target = msat_interface;
    } else if (type->kind == (a_type_kind)tk_class) {
      attr_target = msat_class;
    } else if (type->kind == (a_type_kind)tk_struct) {
      attr_target = msat_struct;
    } else if (type->kind == (a_type_kind)tk_union) {
      attr_target = msat_union;
    }  /* if */
    if (cli_or_cx_enabled) {
      if (is_immediate_cli_interface_type(type)) {
        cli_attr_target = msat_interface;
      } else if (is_immediate_cli_ref_class_type(type)) {
        cli_attr_target = msat_class;
      } else {
        cli_attr_target = msat_struct;
      }  /* if */
    }  /* if */
  }  /* if */
  apply_microsoft_attributes(attributes, (char*)type,
                             (an_il_entry_kind)iek_type,
                             attr_target, cli_attr_target);
}  /* apply_microsoft_attributes_to_type */


void apply_microsoft_attributes_to_field(an_ms_attribute_ptr *attributes,
                                         a_field_ptr         field)
/*
A wrapper around apply_microsoft_attributes to be used when applying
attributes to a field.
*/
{
  an_ms_attribute_target attr_target;
  an_ms_attribute_target cli_attr_target;

  attr_target = msat_field;
  cli_attr_target = msat_invalid;
  if (cli_or_cx_enabled) {
    if (field_is_property_or_event(field) &&
        property_or_event_kind_is(field, pek_cli_property)) {
      cli_attr_target = msat_property;
    } else if (field_is_property_or_event(field) &&
               property_or_event_kind_is(field, pek_cli_event)) {
      cli_attr_target = msat_event;
    } else {
      cli_attr_target = msat_field;
    }  /* if */
  }  /* if */
  apply_microsoft_attributes(attributes, (char*)field,
                             (an_il_entry_kind)iek_field,
                             attr_target, cli_attr_target);
}  /* apply_microsoft_attributes_to_field */


void apply_microsoft_attributes_to_variable(an_ms_attribute_ptr *attributes,
                                            a_variable_ptr      variable)
/*
A wrapper around apply_microsoft_attributes to be used when applying
attributes to a variable.
*/
{
  an_ms_attribute_target attr_target;
  an_ms_attribute_target cli_attr_target;

  cli_attr_target = msat_invalid;
  if (variable->source_corresp.is_class_member) {
    attr_target = msat_field;
    if (cli_or_cx_enabled) {
      if (var_is_property_or_event(variable) &&
          property_or_event_kind_is(variable, pek_cli_property)) {
        cli_attr_target = msat_property;
      } else if (var_is_property_or_event(variable) &&
                 property_or_event_kind_is(variable, pek_cli_event)) {
        cli_attr_target = msat_event;
      } else {
        cli_attr_target = msat_field;
      }  /* if */
    }  /* if */
  } else {
    attr_target = msat_variable;
  }  /* if */
  apply_microsoft_attributes(attributes, (char*)variable,
                             (an_il_entry_kind)iek_variable,
                             attr_target, cli_attr_target);
}  /* apply_microsoft_attributes_to_variable */


void apply_microsoft_attributes_to_routine(an_ms_attribute_ptr *attributes,
                                           a_routine_ptr       routine)
/*
A wrapper around apply_microsoft_attributes to be used when applying
attributes to a routine.
*/
{
  an_ms_attribute_target attr_target;
  an_ms_attribute_target cli_attr_target;

  if (routine->source_corresp.is_class_member) {
    if (routine->special_kind == (a_special_function_kind)sfk_constructor ||
        routine->special_kind ==
        (a_special_function_kind)sfk_static_constructor) {
      attr_target = msat_constructor;
    } else {
      attr_target = msat_method;
    }  /* if */
    cli_attr_target = attr_target;
  } else {
    attr_target = msat_routine;
    cli_attr_target = msat_method;
  }  /* if */
  apply_microsoft_attributes(attributes, (char*)routine,
                             (an_il_entry_kind)iek_routine,
                             attr_target, cli_attr_target);
}  /* apply_microsoft_attributes_to_routine */


void apply_microsoft_attributes_to_base_class(an_ms_attribute_ptr *attributes,
                                              a_base_class_ptr    bcp)
/*
Apply the given list of attributes (which cannot be NULL) to the given base
class entry.  This is currently only permitted in C++/CX mode, and an error is
issued if the base class is not an interface type.
*/
{
  check_assertion(cppcx_enabled && *attributes != NULL);
  if (!is_cli_interface_type(bcp->type)) {
    /* This kind of attribute can only be applied to C++/CX interface types. */
    dispose_of_unapplied_attributes(attributes,
                                    ec_invalid_base_for_ms_attributes);
  } else {
    an_ms_attribute_target attr_target, cli_attr_target;
    attr_target = msat_interfaceimpl;
    cli_attr_target = msat_interfaceimpl;
    apply_microsoft_attributes(attributes, (char*)bcp,
                               (an_il_entry_kind)iek_base_class,
                               attr_target, cli_attr_target);
  }  /* if */
}  /* apply_microsoft_attributes_to_base_class */


void verify_standalone_attributes(an_ms_attribute_ptr	*attributes)
/*
This routine is used to verify that the list of Microsoft attributes
specified by "attributes" contains only standalone attributes.  Valid
attributes are added to the appropriate IL list.
*/
{
  an_ms_attribute_ptr	msap;
  an_ms_attribute_ptr	next_msap;

  for (msap = *attributes; msap != NULL; msap = next_msap) {
    an_ms_attribute_target explicit_target = msap->target;
    an_ms_attribute_target valid_targets;
    a_type_ptr             attribute_type = NULL;
    a_boolean              is_error = FALSE;
    next_msap = msap->next;
    if ((explicit_target & standalone_ms_attribute_targets()) == 0) {
      /* An explicit attribute target is not a standalone attribute
         target. */
      is_error = TRUE;
      pos_error(ec_invalid_attribute_target_for_standalone_ms_attr,
                &msap->position);
    } else if (msap->kind == (an_ms_attribute_kind)msak_custom) {
      an_ms_attribute_usage_ptr attribute_usage;
      attribute_type = msap->variant.custom_info.type;
      attribute_usage = attribute_usage_for_attribute_type(attribute_type);
      if (attribute_usage == NULL) {
        /* The attribute usage is unknown because it is a template-dependent
           or an error type; assume all targets are valid. */
        valid_targets = cppcx_enabled ?
                            msat_from_cppcxat(cppcxat_all) :
                            msat_from_cliat(cliat_all);
      } else {
        valid_targets = attribute_usage->valid_on;
      }  /* if */
    } else {
      valid_targets = msap->variant.info.kind_descr->target;
    }  /* if */
    if (!is_error &&
        (valid_targets & standalone_ms_attribute_targets()) == 0) {
      is_error = TRUE;
      if (msap->kind == (an_ms_attribute_kind)msak_custom) {
        pos_ty_error(ec_invalid_use_of_custom_ms_attr, &msap->position,
                     attribute_type);
      } else {
        pos_st_error(ec_invalid_use_of_ms_attr, &msap->position,
                     msap->variant.info.name);
      }  /* if */
    }  /* if */
    if (!is_error) {
      if (msap->kind > (an_ms_attribute_kind)msak_misc) {
        /* An attribute for which special processing is needed. */
        is_error = !process_microsoft_attribute(msap, NULL, iek_none);
      }  /* if */
      /* Add the attribute to the IL. */
      add_to_ms_attributes_list(msap, decl_scope_level);
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Either complete the source sequence entry or discard it. */
    finalize_ms_attribute_source_sequence_entry(msap, is_error);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  }  /* for */
  /* Clear the attribute list pointer passed by the caller. */
  *attributes = NULL;
}  /* verify_standalone_attributes */


void dispose_of_unapplied_attributes(an_ms_attribute_ptr	*attributes,
				     an_error_code		error_code)
/*
"attributes" is a list of attributes that could not be applied to an entity.
Issue an error and do any cleanup needed to dispose of the attributes.
"error_code" identifies the message to be issued.  The error is suppressed
if it is ec_no_error.
*/
{
  check_assertion(*attributes != NULL);
  if (error_code != ec_no_error) {
    pos_error(error_code, &(*attributes)->position);
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Remove the empty source sequence entries previously created for these
     attributes. */
  { an_ms_attribute_ptr	msap;
    for (msap = *attributes; msap != NULL; msap = msap->next) {
      finalize_ms_attribute_source_sequence_entry(msap, /*is_error=*/TRUE);
    }  /* for */
  }
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  /* Clear the attribute list pointer passed by the caller. */
  *attributes = NULL;
}  /* dispose_of_unapplied_attributes */


static an_ms_attribute_arg_ptr duplicate_ms_attribute_args(
                                                an_ms_attribute_arg_ptr  orig)
/*
Return a duplicate of the given list of attribute arguments.
*/
{
  an_ms_attribute_arg_ptr  result = NULL, *p_msaap = &result;

  while (orig != NULL) {
    *p_msaap = alloc_ms_attribute_arg(orig->kind);
    **p_msaap = *orig;
    orig = orig->next;
    p_msaap = &((*p_msaap)->next);
  }  /* while */
  return result;
}  /* duplicate_ms_attribute_args */


static a_custom_ms_attribute_arg_ptr duplicate_custom_ms_attribute_args(
                                          a_custom_ms_attribute_arg_ptr  orig)
/*
Return a duplicate of the given list of custom attribute arguments.
*/
{
  a_custom_ms_attribute_arg_ptr  result = NULL, *p_msaap = &result;

  while (orig != NULL) {
    *p_msaap = alloc_custom_ms_attribute_arg();
    **p_msaap = *orig;
    orig = orig->next;
    p_msaap = &((*p_msaap)->next);
  }  /* while */
  return result;
}  /* duplicate_custom_ms_attribute_args */


an_ms_attribute_ptr duplicate_ms_attributes(an_ms_attribute_ptr  orig,
                                            char                 *new_entity)
/*
Return a duplicate of the given list of attributes.  The attribute arguments
(if any) are also duplicated.  However, other items these attributes and
arguments point to (like character strings) are shared.  Update the copy
of the attribute to refer to new_entity.
*/
{
  an_ms_attribute_ptr  result = NULL, *p_msap = &result, prev_msap = NULL;

  while (orig != NULL) {
    *p_msap = alloc_ms_attribute(orig->kind);
    **p_msap = *orig;
    (*p_msap)->entity.ptr = new_entity;
    if (prev_msap != NULL) {
      prev_msap->next = *p_msap;
    }  /* if */
    if (orig->kind == (an_ms_attribute_kind)msak_custom) {
      (*p_msap)->variant.custom_info.args =
                   copy_list_of_expr_trees(orig->variant.custom_info.args,
                                           CE_COPIED_CONSTANTS_MAY_BE_SHARED);
      (*p_msap)->variant.custom_info.named_args =
                                      duplicate_custom_ms_attribute_args(
                                        orig->variant.custom_info.named_args);
    } else {
      (*p_msap)->variant.info.arg_list = duplicate_ms_attribute_args(
                                                 orig->variant.info.arg_list);
    }  /* if */
    prev_msap = *p_msap;
    orig = orig->next_in_block;
    p_msap = &((*p_msap)->next_in_block);
  }  /* while */
  return result;
}  /* duplicate_ms_attributes */


an_ms_attribute_ptr find_ms_attribute_for_entity(
                                            an_ms_attribute_ptr          msap,
                                            a_source_correspondence_ptr  scp)
/*
Find an attribute associated with the nonlocal (i.e., not defined inside a
function) entity whose source correspondence is scp.  If msap is NULL, the
search starts at the beginning of the list of attributes recorded for the
parent scope of scp; otherwise, the search starts with msap->next.  This
allows finding all attributes associated with scp using repeated calls to
find_ms_attribute_for_entity.  If no attribute associated with scp is found,
return NULL.
*/
{
  if (msap == NULL) {
    /* Determine the scope whose Microsoft attributes list should be
       searched. */
    if (C_mode()) {
      /* In C mode, there are no class or namespace scopes; so only the file
         scope is searched. */
      msap = il_header.primary_scope->ms_attributes;
    } else if (scp->is_class_member) {
      msap = class_type_supp(scp_parent_class(scp))->assoc_scope
                                                   ->ms_attributes;
    } else if (scp_is_namespace_member(scp)) {
      msap = scp_parent_namespace(scp)->variant.assoc_scope->ms_attributes;
    } else {
      /* File scope in C++ mode. */
      msap = il_header.primary_scope->ms_attributes;
    }  /* if */
  } else {
    /* Start off right after the given attribute (which is presumably an
       attribute found in a previous call). */
    msap = msap->next;
  }  /* if */
  while (msap != NULL) {
    if (msap->entity.ptr == (char*)scp) break;
    msap = msap->next;
  }  /* while */
  return msap;
}  /* find_ms_attribute_for_entity */

#if DEBUG
/* GCOVR_EXCL_START */

void db_microsoft_attribute(an_ms_attribute_ptr	msap)
/*
Display a Microsoft attribute entry, for debugging purposes.
*/
{
  int arg_number = 0;

  fprintf(f_debug, "Microsoft attribute at %p (%lu/%d):\n  target: ",
          (void *)msap, (unsigned long)msap->position.seq,
          msap->position.column);
  switch (msap->target) {
    case msat_none:             fprintf(f_debug, "<none>\n");           break;
    case msat_assembly:         fprintf(f_debug, "assembly\n");         break;
    case msat_module:           fprintf(f_debug, "module\n");           break;
    case msat_class:            fprintf(f_debug, "class\n");            break;
    case msat_struct:           fprintf(f_debug, "struct\n");           break;
    case msat_union:            fprintf(f_debug, "union\n");            break;
    case msat_enum:             fprintf(f_debug, "enum\n");             break;
    case msat_constructor:      fprintf(f_debug, "constructor\n");      break;
    case msat_method:           fprintf(f_debug, "method\n");           break;
    case msat_property:         fprintf(f_debug, "property\n");         break;
    case msat_field:            fprintf(f_debug, "field\n");            break;
    case msat_event:            fprintf(f_debug, "event\n");            break;
    case msat_interface:        fprintf(f_debug, "interface\n");        break;
    case msat_parameter:        fprintf(f_debug, "parameter\n");        break;
    case msat_delegate:         fprintf(f_debug, "delegate\n");         break;
    case msat_returnvalue:      fprintf(f_debug, "returnvalue\n");      break;
    case msat_genericparameter: fprintf(f_debug, "genericparameter\n"); break;
    case msat_typedef:          fprintf(f_debug, "typedef\n");          break;
    case msat_variable:         fprintf(f_debug, "variable\n");         break;
    case msat_routine:          fprintf(f_debug, "routine\n");          break;
    case msat_interfaceimpl:    fprintf(f_debug, "interfaceimpl\n");    break;
    default:                    unexpected_condition();                 break;
  }  /* switch */
  if (msap->kind == (an_ms_attribute_kind)msak_custom) {
    an_expr_node_ptr              arg;
    a_custom_ms_attribute_arg_ptr named_arg;
    fprintf(f_debug, "  attribute constructor: ");
    db_name_full(&msap->variant.custom_info.constructor->source_corresp,
                 iek_routine);
    fputs("\n", f_debug);
    for (arg = msap->variant.custom_info.args; arg != NULL; arg = arg->next) {
      fprintf(f_debug, "  arg %d:\n", arg_number++);
      db_expression(arg);
    }  /* if */
    for (arg_number = 0, named_arg = msap->variant.custom_info.named_args;
         named_arg != NULL; named_arg = named_arg->next) {
      fprintf(f_debug, "  named arg %d:\n", arg_number++);
      db_field(named_arg->field, 4);
      fputs("\n", f_debug);
      db_expression(named_arg->expression);
    }  /* if */
  } else {
    an_ms_attribute_arg_ptr arg;
    fprintf(f_debug, "  name: '%s'\n",
            msap->variant.info.name == NULL ? "NULL":msap->variant.info.name);
    fprintf(f_debug, "  attribute string: %s\n", msap->variant.info.string);
    for (arg = msap->variant.info.arg_list; arg != NULL; arg = arg->next) {
      fprintf(f_debug, "  argument %d (%s): ", arg_number++, arg->param_name);
      switch (arg->kind) {
        case msaak_integer:
          fprintf(f_debug, "%ld", (long)arg->variant.integer_value);
          break;
        case msaak_boolean:
          fprintf(f_debug, "%s",
                  arg->variant.integer_value ? "true" : "false");
          break;
        case msaak_string:
          db_constant(arg->variant.string_constant);
          break;
        case msaak_other:
          fprintf(f_debug, "%s", arg->variant.other_string);
          break;
        case msaak_uuid:
          fprintf(f_debug, "%s", arg->variant.uuid_string);
          break;
        case msaak_enumeration:
          fprintf(f_debug, "%d", arg->variant.enum_value);
          break;
        default:
          unexpected_condition();
          break;
      }  /* switch */
    }  /* if */
  }  /* for */
  fprintf(f_debug, "\n");
}  /* db_microsoft_attribute */


unsigned long db_show_ms_attrib_space_used(unsigned long grand_total)
/*
Show space used by the Microsoft attribute routines.  This is called by
the symbol table space used routine.  The space used by these
routines is reported as part of the symbol table memory used.
*/
{
  unsigned long	num;
  unsigned long	size;
  unsigned long	total;

  db_space_used("ms attribute descrs",
                 num_ms_attribute_kind_descrs_allocated,
                 an_ms_attribute_kind_descr);
  db_space_used("ms attribute parameters",
                 num_ms_attribute_params_allocated,
                 an_ms_attribute_param);
  return grand_total;
}  /* db_show_ms_attrib_space_used */

/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

void ms_attrib_one_time_init(void)
/*
One-time initialization for ms_attrib.c static variables.
*/
{
  ms_attr_buffer = NULL;
  /* Save variables that are needed for precompiled headers */
  if (precompiled_header_processing_required) {
    STATIC_THREAD a_pch_saved_variable saved_vars[] = {
      pch_array_saved_var_array_elem(attribute_lookup_table),
      pch_saved_var_array_elem(unrecognized_attribute),
      pch_saved_var_array_elem(custom_attribute),
#if DEBUG
/* GCOVR_EXCL_START */
      pch_saved_var_array_elem(num_ms_attribute_kind_descrs_allocated),
      pch_saved_var_array_elem(num_ms_attribute_params_allocated),
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  /* Register variables that must be saved and restored when switching
     between translation units. */
}  /* ms_attrib_one_time_init */


void ms_attrib_init(void)
/*
The per-compilation unit initialization routine for variables related to
Microsoft attribute processing.
*/
{
  scan_misc_attributes_as_unrecognized =
                                       SUPPRESS_MICROSOFT_ATTRIBUTE_PROCESSING;
  unrecognized_attribute = NULL;
  custom_attribute = NULL;
#if DEBUG
/* GCOVR_EXCL_START */
  num_ms_attribute_kind_descrs_allocated = 0;
  num_ms_attribute_params_allocated = 0;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  memzero((char *)attribute_lookup_table, sizeof(attribute_lookup_table));
  curr_attribute_descr = NULL;
  attribute_cache = new_fe<a_token_cache>(/*reusable=*/TRUE);
  /* Build the structure used to describe the various attributes. */
  if (ms_extensions) init_attribute_kinds();
  no_injected_text = FALSE;
}  /* ms_attrib_init */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

